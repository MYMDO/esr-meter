// ============================================================
// 18650 ESR Meter - INA226 + ADS1115 Hybrid
// Arduino Nano, OLED 0.96" I2C
//
// DC Pulsed Load Method:
//   ESR = (V_oc - V_load) / I_load
//
// Hardware:
//   INA226 @ 0x40  - Current measurement via 0.1 Ohm shunt
//   ADS1115 @ 0x48 - Battery voltage via 2:1 divider (100k+100k)
//   OLED   @ 0x3C  - 128x64 SSD1306 display
//   D2            - MOSFET gate drive (HYG012N03LR1TA / IRF3205)
//   R_load = 5.1 Ohm 10W ceramic
//   Kelvin 4-wire: separate Force/Sense pairs
//
// Cycle: Measure OC (20ms) -> ON -> Stabilize (100ms) ->
//        Measure Load (40ms) -> OFF -> Calc + Display -> Wait 800ms
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// ---- OLED Display Object ----
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---- State Machine ----
enum State : uint8_t {
  ST_IDLE,
  ST_MEASURE_OC,
  ST_LOAD_ON,
  ST_STABILIZE,
  ST_MEASURE_LOAD,
  ST_CALC_ESR,
  ST_DISPLAY
};

static State state = ST_IDLE;
static uint32_t timer = 0;
static uint16_t sampleCount = 0;

// ---- Measurement Buffers ----
static float v_oc = 0.0F;
static float v_load = 0.0F;
static float i_load_mA = 0.0F;
static float esr = 0.0F;

// ---- Status Strings (PROGMEM) ----
static const char STR_OK[]       PROGMEM = "OK";
static const char STR_GOOD[]     PROGMEM = "GOOD";
static const char STR_DEGRADED[] PROGMEM = "DEGRADED";
static const char STR_REPLACE[]  PROGMEM = "REPLACE!";
static const char STR_MEAS_OC[]  PROGMEM = "Meas V_oc...";
static const char STR_STAB[]     PROGMEM = "Stabilize...";
static const char STR_MEAS_LOAD[] PROGMEM = "Meas Load...";
static const char STR_NO_BAT[]   PROGMEM = "NO BATTERY";

// ============================================================
// I2C Helpers
// ============================================================

static bool i2cWriteReg(uint8_t addr, uint8_t reg, uint16_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write((uint8_t)(val >> 8));
  Wire.write((uint8_t)(val & 0xFF));
  return Wire.endTransmission() == 0;
}

static uint16_t i2cReadReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  if (Wire.requestFrom(addr, (uint8_t)2) >= 2) {
    return ((uint16_t)Wire.read() << 8) | Wire.read();
  }
  return 0;
}

// ============================================================
// INA226 Driver
// ============================================================

static void INA226_init(void) {
  // Config Register (0x00):
  //   AVG=16 (bits 11:9 = 100)
  //   VBUSCT=2.116ms (bits 8:6 = 011)
  //   VSHCT=2.116ms  (bits 5:3 = 011)
  //   MODE=Cont shunt+bus (bits 2:0 = 111)
  //   Value: 0b0000 1000 1101 1111 = 0x08DF
  i2cWriteReg(INA226_ADDR, 0x00, 0x08DF);
  // Calibration Register (0x05): Current_LSB=50uA, R_shunt=0.1Ohm
  //   Cal = 0.00512 / (50e-6 * 0.1) = 1024 = 0x0400
  i2cWriteReg(INA226_ADDR, 0x05, INA226_CAL);
  delay(70);  // Wait for 16 averaged readings
}

// Returns current in mA (signed, LSB = 50uA)
static float INA226_readCurrent(void) {
  int16_t raw = (int16_t)i2cReadReg(INA226_ADDR, 0x04);
  return (float)raw * (CURRENT_LSB * 1000.0F);
}

static int16_t INA226_readShuntRaw(void) {
  return (int16_t)i2cReadReg(INA226_ADDR, 0x01);
}

// ============================================================
// ADS1115 Driver
// ============================================================

static void ADS1115_init(void) {
  // Continuous conversion mode:
  //   MUX=000 (AIN0 vs GND), PGA=001 (+/-4.096V)
  //   MODE=0 (continuous), DR=111 (860 SPS)
  //   COMP_QUE=11 (disabled)
  i2cWriteReg(ADS1115_ADDR, 0x01, 0x02E3);
  delay(2);
}

// Returns battery voltage in Volts, corrected by divider ratio
static float ADS1115_readVoltage(void) {
  int16_t raw = (int16_t)i2cReadReg(ADS1115_ADDR, 0x00);
  float v_adc = (float)raw * 0.000125F;    // LSB = 125uV at +/-4.096V
  return v_adc / K_DIVIDER;                 // Correct for 2:1 divider
}

// ============================================================
// OLED Display
// ============================================================

static bool OLED_init(void) {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    return false;
  }
  display.clearDisplay();
  display.display();
  return true;
}

static void OLED_showSplash(void) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 0);
  display.println(F("18650 ESR Meter"));
  display.setCursor(4, 12);
  display.println(F("INA226 + ADS1115"));
  display.setCursor(4, 24);
  display.println(F("DC Pulsed Load"));
  display.setCursor(4, 36);
  display.println(F("v1.0"));
  display.display();
  delay(1500);
}

static void OLED_update(float esrVal, float v, float i, const char* status) {
  display.clearDisplay();

  // ---- Line 0-1: ESR value (large font) ----
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (esrVal < 1000.0F) {
    display.print(esrVal, 1);
    display.print('m');
    display.print('O');
  } else {
    display.print(esrVal / 1000.0F, 2);
    display.print('O');
  }

  // ---- Line 2: V and I (small font) ----
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print(F("V:"));
  display.print(v, 3);
  display.print(F("V"));

  display.setCursor(68, 20);
  display.print(F("I:"));
  display.print(i, 2);
  display.print(F("A"));

  // ---- Line 3: Delta-V and Status ----
  display.setCursor(0, 30);
  display.print(F("dV:"));
  float dv = (v_oc - v_load) * 1000.0F;
  display.print(dv, 1);
  display.print(F("mV"));

  display.setCursor(80, 30);
  display.println(status);

  // ---- Line 4: Scale bar ----
  uint16_t barW = (uint16_t)(esrVal * 126.0F / 200.0F);
  if (barW > 126) barW = 126;
  display.drawRect(0, 50, 128, 12, SSD1306_WHITE);
  if (barW > 0) {
    display.fillRect(1, 51, barW, 10, SSD1306_WHITE);
  }

  display.display();
}

static void OLED_showNoBattery(void) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println(F("Connect 18650"));
  display.setCursor(10, 32);
  display.println(F("4-wire Kelvin"));
  display.display();
}

// ============================================================
// Serial Output
// ============================================================

static void serial_printMeasurement(void) {
  Serial.print(F("ESR:"));
  Serial.print(esr, 1);
  Serial.print(F(" V_oc:"));
  Serial.print(v_oc, 3);
  Serial.print(F(" V_ld:"));
  Serial.print(v_load, 3);
  Serial.print(F(" I:"));
  Serial.print(i_load_mA, 2);
  Serial.print(F(" dV:"));
  Serial.print((v_oc - v_load) * 1000.0F, 1);
  Serial.println(F("mV"));
}

// ============================================================
// Setup
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial.println(F("--- 18650 ESR Meter v1.0 ---"));

  Wire.begin();
  Wire.setClock(400000L);

  pinMode(GATE_PIN, OUTPUT);
  digitalWrite(GATE_PIN, LOW);

  // Init INA226
  Serial.print(F("INA226 @0x40 ... "));
  INA226_init();
  Serial.println(F("OK"));

  // Init ADS1115
  Serial.print(F("ADS1115 @0x48 ... "));
  ADS1115_init();
  Serial.println(F("OK"));

  // Init OLED
  Serial.print(F("OLED @0x3C ... "));
  if (!OLED_init()) {
    Serial.println(F("FAILED"));
    Serial.println(F("Check I2C wiring!"));
    while (1);  // Halt
  }
  Serial.println(F("OK"));

  OLED_showSplash();
  state = ST_IDLE;

  Serial.println(F("Ready - connect 18650"));
}

// ============================================================
// Main Loop - State Machine
// ============================================================

void loop() {
  switch (state) {

    // ---- IDLE: wait for battery ----
    case ST_IDLE: {
      float v_test = ADS1115_readVoltage();
      if (v_test < BAT_MIN_V) {
        OLED_showNoBattery();
        delay(250);
        break;
      }
      if (v_test > BAT_MAX_V) {
        // Overvoltage - wait and retry
        delay(500);
        break;
      }
      // Battery detected - start measurement
      sampleCount = 0;
      v_oc = 0.0F;
      state = ST_MEASURE_OC;
      break;
    }

    // ---- MEASURE_OC: average open-circuit voltage ----
    case ST_MEASURE_OC: {
      v_oc += ADS1115_readVoltage();
      sampleCount++;
      if (sampleCount >= OC_SAMPLES) {
        v_oc /= (float)OC_SAMPLES;
        state = ST_LOAD_ON;
      }
      break;
    }

    // ---- LOAD_ON: turn on MOSFET, start timer ----
    case ST_LOAD_ON: {
      digitalWrite(GATE_PIN, HIGH);
      timer = millis();
      state = ST_STABILIZE;
      break;
    }

    // ---- STABILIZE: wait for current to settle ----
    case ST_STABILIZE: {
      if (millis() - timer >= STABILIZE_MS) {
        sampleCount = 0;
        v_load = 0.0F;
        i_load_mA = 0.0F;
        state = ST_MEASURE_LOAD;
      }
      break;
    }

    // ---- MEASURE_LOAD: average V and I under load ----
    case ST_MEASURE_LOAD: {
      v_load += ADS1115_readVoltage();
      i_load_mA += INA226_readCurrent();
      sampleCount++;
      if (sampleCount >= LOAD_SAMPLES) {
        v_load /= (float)LOAD_SAMPLES;
        i_load_mA /= (float)LOAD_SAMPLES;
        i_load_mA -= CURRENT_OFFSET;  // Apply offset correction
        digitalWrite(GATE_PIN, LOW);
        state = ST_CALC_ESR;
      }
      break;
    }

    // ---- CALC_ESR: compute ESR from averaged data ----
    case ST_CALC_ESR: {
      float dv = v_oc - v_load;
      float i_A = i_load_mA / 1000.0F;
      if (dv <= 0.0F || i_A <= 0.001F) {
        // Invalid measurement - skip this cycle
        state = ST_IDLE;
        delay(500);
        break;
      }
      float esrRaw = (dv / i_A) * 1000.0F;    // mOhm
      esr = esrRaw * CAL_GAIN + CAL_OFFSET;
      if (esr < 0.0F) esr = 0.0F;
      state = ST_DISPLAY;
      break;
    }

    // ---- DISPLAY: update OLED, serial, wait for next cycle ----
    case ST_DISPLAY: {
      // Status based on ESR
      const char* status = STR_OK;
      if (esr >= ESR_DEGRADED) status = STR_DEGRADED;
      if (esr >= ESR_REPLACE)  status = STR_REPLACE;
      if (esr <= ESR_GOOD)    status = STR_GOOD;

      OLED_update(esr, v_oc, i_load_mA / 1000.0F, status);
      serial_printMeasurement();

      // Wait for end of 1-second cycle
      uint32_t elapsed = millis() - timer;
      if (elapsed < CYCLE_MS) {
        delay(CYCLE_MS - elapsed);
      }

      state = ST_IDLE;
      break;
    }
  }
}

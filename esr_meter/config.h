#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// 18650 ESR Meter - Configuration & Calibration Constants
// Arduino Nano + INA226 + ADS1115 + OLED 0.96"
// DC Pulsed Load Method
// ============================================================

// ---- Pin Definitions ----
#define GATE_PIN        2       // MOSFET gate drive (active HIGH)
#define OLED_RESET      -1      // OLED reset pin (shared with Arduino reset)

// ---- I2C Addresses ----
#define INA226_ADDR     0x40    // INA226 default (A0=GND, A1=GND)
#define ADS1115_ADDR    0x48    // ADS1115 default (ADDR=GND)
#define OLED_ADDR       0x3C    // SSD1306 OLED default

// ---- Display ----
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// ---- Timing (milliseconds) ----
#define STABILIZE_MS    100     // Wait after MOSFET turns on
#define CYCLE_MS        1000    // Full measurement cycle (1/sec)

// ---- ADC Sampling ----
#define OC_SAMPLES      16      // Voltage readings averaged for V_oc
#define LOAD_SAMPLES    16      // V + I reading pairs averaged for V_load/I_load

// ---- Battery Thresholds ----
#define BAT_MIN_V       2.5F    // Minimum battery voltage
#define BAT_MAX_V       4.5F    // Maximum (overvoltage protection)

// ---- INA226 Calibration ----
// Current_LSB = 50 uA/bit gives range up to +/-1.638 A
// Cal = 0.00512 / (Current_LSB * R_shunt)
// Cal = 0.00512 / (0.00005 * 0.1) = 1024
#define INA226_CAL      1024
#define CURRENT_LSB     0.00005F    // 50 uA/bit

// ---- Voltage Divider ----
// R1 = 100k, R2 = 100k   (2:1 divider from battery to ADS1115 A0)
// K_DIVIDER = R2 / (R1 + R2) = 0.5 (ideal)
// MEASURE: Use multimeter to measure actual R1, R2 and update K_DIVIDER
#define K_DIVIDER       0.5F

// ---- Calibration (to be tuned with known resistors) ----
// ESR = ESR_raw * CAL_GAIN + CAL_OFFSET
// Default: no correction (CAL_GAIN=1, CAL_OFFSET=0)
#define CAL_OFFSET      0.0F    // mOhm (subtract for lead+contact resistance)
#define CAL_GAIN        1.0F    // Multiplicative correction

// Current offset (mA) - measured with no load
#define CURRENT_OFFSET  0.0F    // mA

// ---- ESR Thresholds ----
#define ESR_GOOD        50.0F   // mOhm (below this = healthy)
#define ESR_DEGRADED    150.0F  // mOhm (above this = degraded)
#define ESR_REPLACE     250.0F  // mOhm (above this = replace)

#endif

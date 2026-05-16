# ESR Meter — Agent Guide

## Project structure

```
esr_meter/              # Arduino sketch (folder name MUST match .ino)
├── esr_meter.ino       # State machine, INA226/ADS1115/OLED drivers
└── config.h            # Calibration constants (edit for tuning)
```

Sketch folder + config header. No build system beyond `arduino-cli`.

## Build & upload

```bash
# Compile only (from repo root)
arduino-cli compile --fqbn arduino:avr:nano esr_meter

# Upload (adjust port, from repo root)
arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:nano esr_meter
```

`arduino-cli` is installed locally in `bin/` — add to PATH or use full path.

## Key facts

- **MCU:** Arduino Nano (ATmega328P, 5V, 16 MHz)
- **I2C bus:** INA226 @0x40, ADS1115 @0x48, OLED @0x3C. All on same bus, no conflicts.
- **State machine:** `IDLE → MEASURE_OC → LOAD_ON → STABILIZE → MEASURE_LOAD → CALC_ESR → DISPLAY` — non-blocking via `millis()`.
- **Voltage divider:** 2×100kΩ (2:1) on ADS1115 A0. PGA=±4.096V → LSB=125µV.
- **Current sense:** INA226 built-in 0.1Ω shunt, Current_LSB=50µA/bit, max measurable current = 819mA (INA226 shunt ADC range ±81.92mV).
- **Load:** 5.1Ω 10W ceramic on MOSFET (D2, active HIGH).
- **MOSFET:** HYG012N03LR1TA (preferred, TOLL) or IRF3205 (TO-220 fallback).
- **Calibration** lives in `config.h`: `K_DIVIDER`, `CAL_OFFSET`, `CAL_GAIN`, `CURRENT_OFFSET`.
- **Serial:** 115200 baud, CSV-style output: `ESR:xx.x V_oc:x.xxx V_ld:x.xxx I:xxx.xx dV:xx.xmV`

## Dependencies (install once)

```
arduino-cli core install arduino:avr
arduino-cli lib install "Adafruit SSD1306" "Adafruit GFX Library"
```

## What not to do

- Do NOT change the display library — Adafruit SSD1306 + GFX is the only one set up.
- Do NOT connect VBUS pin on INA226 — voltage is read via ADS1115 divider.
- Do NOT remove the 10kΩ gate pulldown — MOSFET stays off during Arduino reset.
- Do NOT connect Force and Sense together at battery terminals — that breaks Kelvin 4-wire measurement.
- Do NOT move `.ino` out of `esr_meter/` folder — Arduino requires folder name to match `.ino` name.

## Planned: R² battery quality indicator (not yet implemented)

A second load (10Ω 5W + MOSFET on D3) is planned to enable 3-point differential ESR measurement with R² quality scoring. Key constraints discovered during research:

- **INA226 shunt must change to 0.05Ω** — current 0.1Ω saturates at >819mA; parallel loads draw ~1.2A
- **ADS1115 needs 0.1μF charge reservoir** on A0 — 50kΩ divider source impedance is 50× above TI's 1kΩ recommendation
- **Kelvin connections to shunt mandatory** — protoboard trace resistance adds error proportional to total current
- **10Ω resistor must be 5W ceramic** — dissipates 1.76W at 4.2V
- **Use HYG012N03LR1TA, not IRF3205** — logic-level MOSFET required for full enhancement at 5V gate drive
- **Differential pairs replace МНК regression** — battery polarization drift between loads corrupts constant-V_oc assumption; use `R = (V₁−V₂)/(I₂−I₁)` instead

See README.md "R² — індикатор якості батареї" section for full details.

## Accuracy budget

±0.35% post-calibration for 50mΩ 18650 cells. INA226 alone (using VBUS) would be ±5.5% — ADS1115 hybrid is the intentional design choice.

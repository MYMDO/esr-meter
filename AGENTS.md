# 18650 ESR Meter — Agent Guide

## Project structure

```
esr_meter/              # Arduino sketch (folder name = .ino name)
├── esr_meter.ino       # State machine, INA226/ADS1115/OLED drivers
├── config.h            # Calibration constants (edit for tuning)
└── README.md           # Full docs: BOM, schematic, assembly, calibration
```

Single-file sketch + config header. No build system beyond `arduino-cli`.

## Build & upload

```bash
# Compile only
arduino-cli compile --fqbn arduino:avr:nano esr_meter

# Upload (adjust port)
arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:nano esr_meter
```

## Key facts

- **MCU:** Arduino Nano (ATmega328P, 5V, 16 MHz)
- **I2C bus:** INA226 @0x40, ADS1115 @0x48, OLED @0x3C. All on same bus, no conflicts.
- **State machine:** `IDLE → MEASURE_OC → LOAD_ON → STABILIZE → MEASURE_LOAD → CALC_ESR → DISPLAY` — non-blocking via `millis()`.
- **Voltage divider:** 2×100kΩ (2:1) on ADS1115 A0. PGA=±4.096V → LSB=125µV.
- **Current sense:** INA226 built-in 0.1Ω shunt, Current_LSB=50µA/bit.
- **Load:** 5.1Ω 10W ceramic on MOSFET (D2, active HIGH).
- **MOSFET:** HYG012N03LR1TA (preferred, TOLL) or IRF3205 (TO-220 fallback).
- **Calibration** lives in `config.h`: `K_DIVIDER`, `CAL_OFFSET`, `CAL_GAIN`.

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

## Accuracy budget (from `README.md`)

±0.35% post-calibration for 50mΩ 18650 cells. INA226 alone (using VBUS) would be ±5.5% — ADS1115 hybrid is the intentional design choice.

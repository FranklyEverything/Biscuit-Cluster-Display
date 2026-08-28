#pragma once

// ---------- Display wiring ----------
// Waveshare 1.69" ST7789V2, 240 x 280 pixels.
constexpr int PIN_LCD_BACKLIGHT = 4;
constexpr int DISPLAY_ROTATION = 1;  // Horizontal: 280 x 240.

// ---------- Battery monitor ----------
// GPIO 5 must NEVER receive more than 3.3 V. Aim for <= 2.8 V at the
// highest possible battery/charging voltage.
constexpr int PIN_BATTERY_ADC = 5;

// Default hardware: 2S pack and a 2:1 divider (R_TOP is twice R_BOTTOM).
// Enter the actual measured resistor values here; only the ratio matters.
constexpr float BATTERY_R_TOP_OHMS = 200000.0f;
constexpr float BATTERY_R_BOTTOM_OHMS = 100000.0f;

// Fine calibration for resistor tolerance and ESP32 ADC variation.
// This unit measured 8.30 V at the pack and 2.70 V at GPIO 5.
constexpr float BATTERY_CALIBRATION = 8.30f / (2.70f * 3.0f);

constexpr float BATTERY_EMPTY_VOLTS = 6.60f;
constexpr float BATTERY_FULL_VOLTS = 8.30f;
constexpr const char* BATTERY_LABEL = "2S PACK";

// Example for a nominal 12 V / 3S lithium pack (12.6 V fully charged):
//   BATTERY_R_TOP_OHMS    = 100000.0f
//   BATTERY_R_BOTTOM_OHMS = 22000.0f
//   BATTERY_CALIBRATION   = 1.0f, then calibrate with a multimeter
//   BATTERY_EMPTY_VOLTS   = 9.90f
//   BATTERY_FULL_VOLTS    = 12.60f
//   BATTERY_LABEL         = "3S PACK"
// A 100k/22k divider produces about 2.27 V at GPIO 5 from a 12.6 V pack
// and remains below 2.60 V at 14.4 V. Add a 100 nF capacitor from GPIO 5
// to GND near the ESP32 for extra ADC noise filtering.


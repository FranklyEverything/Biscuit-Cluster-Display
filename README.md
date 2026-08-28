# Biscuit Cluster Display

An animated status display for an ESP32-S3 cluster centerpiece. It drives a
Waveshare 1.69-inch ST7789V2 LCD in landscape orientation, renders the spinning
Biscuit logo, and shows a filtered battery-voltage and charge estimate.

The included configuration matches the tested build:

- ESP32-S3 N16R8 development board (16 MB flash, 8 MB octal PSRAM)
- Waveshare 1.69-inch ST7789V2, 240 × 280
- 2S lithium pack, approximately 8.3 V when full
- 2:1 battery-sense divider feeding GPIO 5
- PlatformIO with Arduino-ESP32 and TFT_eSPI

![Biscuit logo](assets/biscuit-logo-turntable.gif)

## Important electrical safety

This particular LCD is a **3.3 V device**. Never connect its VCC pin to 5 V.
Never connect a battery pack above 5 V directly to the ESP32-S3, and never
connect a source above 3.3 V to the LCD.

- Feed a higher-voltage battery into a suitable regulated **5 V buck
  converter**.
- Feed the regulated 5 V output only to the ESP32's **5V/VBUS** input.
- Power LCD VCC from the ESP32's regulated **3V3** pin. If using a separate
  3.3 V regulator, connect its ground to the ESP32 ground.
- Join the battery, converter, ESP32, and LCD grounds.
- Battery voltage is measured separately through a resistor divider.
- GPIO 5 must remain below 3.3 V; this project targets 2.8 V or less at the
  maximum possible charging voltage.
- Avoid powering the 5 V rail externally while USB is connected unless your
  board explicitly supports that arrangement without back-feeding USB.

## LCD wiring

Turn power off while wiring.

| LCD pin | ESP32-S3 connection | Notes |
|---|---:|---|
| VCC | 3V3 | This LCD version is 3.3 V only—do not use 5 V |
| GND | GND | Common ground |
| DIN | GPIO 11 | SPI MOSI |
| CLK | GPIO 12 | SPI clock |
| CS | GPIO 10 | Chip select |
| DC | GPIO 9 | Data/command |
| RST | 3V3 | Tested stable configuration; not GPIO-controlled |
| BL | GPIO 4 | Backlight enable |

The firmware deliberately defines `TFT_RST=-1` because LCD RST is tied to
3.3 V. The display receives a software reset during initialization.

## Battery-sense divider

Wire the divider to the **unregulated battery voltage**, before the 5 V buck:

```text
Battery + ---- R_TOP ----+---- GPIO 5
                         |
                      R_BOTTOM
                         |
Battery - / GND ----------+---- ESP32 GND
```

The ADC voltage is:

```text
Vadc = Vbattery × R_BOTTOM / (R_TOP + R_BOTTOM)
```

### Tested 2S configuration

The default configuration models `R_TOP` as twice `R_BOTTOM`; for example,
200 kΩ and 100 kΩ. An assembled-unit calibration corrects the observed
8.30 V battery / 2.70 V ADC readings. Enter your actual resistor measurements
in [`include/user_config.h`](include/user_config.h) for best accuracy.

### Nominal 12 V / 3S configuration

For a 3S lithium pack that reaches 12.6 V, a safer example is:

- `R_TOP = 100 kΩ`
- `R_BOTTOM = 22 kΩ`
- Optional 100 nF capacitor from GPIO 5 to GND, close to the ESP32

This produces about 2.27 V at the ADC from 12.6 V and about 2.60 V from
14.4 V. Change only these settings in `include/user_config.h`:

```cpp
constexpr float BATTERY_R_TOP_OHMS = 100000.0f;
constexpr float BATTERY_R_BOTTOM_OHMS = 22000.0f;
constexpr float BATTERY_CALIBRATION = 1.0f;
constexpr float BATTERY_EMPTY_VOLTS = 9.90f;
constexpr float BATTERY_FULL_VOLTS = 12.60f;
constexpr const char* BATTERY_LABEL = "3S PACK";
```

Measure the completed divider before connecting GPIO 5. If the battery system
can exceed 14.4 V, recalculate the divider for its true maximum voltage.

## Calibration

1. Fully charge the pack.
2. Measure battery voltage with a reliable multimeter.
3. Measure GPIO 5 to GND.
4. Calculate:

```text
measured_ratio = Vbattery / Vgpio5
ideal_ratio = (R_TOP + R_BOTTOM) / R_BOTTOM
BATTERY_CALIBRATION = measured_ratio / ideal_ratio
```

The firmware uses median sampling, exponential smoothing, 0.05 V display
steps, and percentage hysteresis to reduce ADC jitter.

## Build and upload

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the
   [PlatformIO extension](https://platformio.org/install/ide?install=vscode).
2. Clone or download this repository.
3. Open the repository folder in PlatformIO.
4. Review `include/user_config.h` before applying power.
5. Connect the ESP32-S3 by USB.
6. Select **PlatformIO: Upload**.

The included `platformio.ini` targets a generic ESP32-S3 N16R8 and uses the
tested 20 MHz LCD bus. If your board has different flash or PSRAM, update the
PlatformIO board settings before uploading.

## Custom logo

Replace `assets/biscuit-logo-turntable.gif`, then regenerate the embedded
frames:

```text
python tools/generate_logo.py
```

The script requires Pillow (`python -m pip install pillow`). It produces
`src/logo_frames.h`; no SD card or filesystem is required at runtime.

## Project layout

```text
assets/                 Source animation
include/user_config.h   User-editable battery and display settings
src/main.cpp            Display and battery-monitor firmware
src/logo_frames.h       Generated embedded animation frames
tools/generate_logo.py  GIF-to-XBitmap conversion tool
platformio.ini          Reproducible build configuration
```

## License

Software and included project files are released under the [MIT License](LICENSE).

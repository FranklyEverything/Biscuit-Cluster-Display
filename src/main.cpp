#include <Arduino.h>
#include <TFT_eSPI.h>
#include "logo_frames.h"
#include "user_config.h"

namespace {
constexpr uint8_t BACKLIGHT_PIN = PIN_LCD_BACKLIGHT;
constexpr uint8_t LCD_CS_PIN = 10;
constexpr uint8_t LCD_DC_PIN = 9;
constexpr uint8_t LCD_MOSI_PIN = 11;
constexpr uint8_t LCD_SCLK_PIN = 12;
constexpr bool LCD_DIAGNOSTIC = false;
constexpr bool STATIC_STABILITY_TEST = false;

constexpr int8_t BATTERY_ADC_PIN = PIN_BATTERY_ADC;
constexpr float BATTERY_DIVIDER_RATIO =
    ((BATTERY_R_TOP_OHMS + BATTERY_R_BOTTOM_OHMS) /
     BATTERY_R_BOTTOM_OHMS) * BATTERY_CALIBRATION;

constexpr uint16_t COLOR_BG = TFT_BLACK;
constexpr uint16_t COLOR_LOGO = TFT_WHITE;
constexpr uint16_t COLOR_ACCENT = 0xFD20;
constexpr uint16_t COLOR_DIM = 0x7BEF;
constexpr uint16_t COLOR_PANEL = 0x0841;
constexpr uint16_t COLOR_GREEN = 0x07E0;
constexpr uint16_t COLOR_RED = 0xF800;

TFT_eSPI tft;
TFT_eSprite logoSprite(&tft);
uint8_t currentFrame = 0;
uint32_t nextFrameAt = 0;
uint32_t nextPowerRefreshAt = 0;

int batteryPercentFromVoltage(float volts) {
  // Scale a lithium-style discharge curve between the user-configured empty
  // and full pack voltages, so changing pack size requires config edits only.
  static constexpr float normalizedVoltagePoints[] = {
      0.000f, 0.176f, 0.353f, 0.471f, 0.588f,
      0.706f, 0.824f, 0.941f, 1.000f};
  static constexpr uint8_t percentPoints[] = {
      0, 5, 15, 30, 50, 65, 80, 92, 100};
  if (volts <= BATTERY_EMPTY_VOLTS) return 0;
  if (volts >= BATTERY_FULL_VOLTS) return 100;
  const float normalized = (volts - BATTERY_EMPTY_VOLTS) /
                           (BATTERY_FULL_VOLTS - BATTERY_EMPTY_VOLTS);
  for (uint8_t i = 1; i < 9; ++i) {
    if (normalized <= normalizedVoltagePoints[i]) {
      const float position =
          (normalized - normalizedVoltagePoints[i - 1]) /
          (normalizedVoltagePoints[i] - normalizedVoltagePoints[i - 1]);
      return percentPoints[i - 1] +
             static_cast<int>(position * (percentPoints[i] - percentPoints[i - 1]));
    }
  }
  return 100;
}

float readBatteryVoltage() {
  constexpr uint8_t samples = 31;
  uint16_t millivolts[samples];
  for (uint8_t i = 0; i < samples; ++i) {
    millivolts[i] = analogReadMilliVolts(BATTERY_ADC_PIN);
    delay(1);
  }
  for (uint8_t i = 1; i < samples; ++i) {
    const uint16_t value = millivolts[i];
    int8_t j = i - 1;
    while (j >= 0 && millivolts[j] > value) {
      millivolts[j + 1] = millivolts[j];
      --j;
    }
    millivolts[j + 1] = value;
  }
  const float rawVoltage = (millivolts[samples / 2] / 1000.0f) *
                           BATTERY_DIVIDER_RATIO;
  static float filteredVoltage = -1.0f;
  filteredVoltage = filteredVoltage < 0.0f
                        ? rawVoltage
                        : filteredVoltage * 0.85f + rawVoltage * 0.15f;
  return roundf(filteredVoltage * 20.0f) / 20.0f;
}

void drawHeader() {
  tft.fillRect(0, 0, 280, 38, COLOR_BG);
  tft.fillRoundRect(10, 6, 260, 27, 8, COLOR_PANEL);
  tft.drawRoundRect(10, 6, 260, 27, 8, COLOR_ACCENT);
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, COLOR_PANEL);
  tft.drawString("BISCUIT  CLUSTER", 140, 10);
}

void drawPowerStatus() {
  constexpr int16_t x = 174;
  constexpr int16_t y = 42;
  tft.fillRoundRect(x, y, 96, 188, 10, COLOR_PANEL);
  tft.drawRoundRect(x, y, 96, 188, 10, COLOR_DIM);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);

  if constexpr (BATTERY_ADC_PIN >= 0) {
    const float batteryVolts = readBatteryVoltage();
    const int measuredPercent = batteryPercentFromVoltage(batteryVolts);
    static int percent = -1;
    if (percent < 0 || abs(measuredPercent - percent) >= 2) {
      percent = measuredPercent;
    }
    const uint16_t levelColor = percent <= 15 ? COLOR_RED :
                                (percent <= 35 ? COLOR_ACCENT : COLOR_GREEN);

    tft.setTextColor(TFT_WHITE, COLOR_PANEL);
    tft.drawString("BATTERY", x + 48, y + 18);
    tft.setTextColor(COLOR_DIM, COLOR_PANEL);
    tft.drawString(String(batteryVolts, 2) + " V", x + 48, y + 43);

    tft.drawRoundRect(x + 16, y + 65, 62, 28, 5, COLOR_DIM);
    tft.fillRect(x + 78, y + 73, 4, 12, COLOR_DIM);
    const int16_t fillWidth = map(percent, 0, 100, 0, 54);
    if (fillWidth > 0) {
      tft.fillRoundRect(x + 20, y + 69, fillWidth, 20, 3, levelColor);
    }
    tft.setTextColor(TFT_WHITE, COLOR_PANEL);
    tft.setTextFont(4);
    tft.drawString(String(percent) + "%", x + 48, y + 122);
    tft.setTextFont(2);
    tft.setTextColor(COLOR_DIM, COLOR_PANEL);
    tft.drawString(BATTERY_LABEL, x + 48, y + 158);
  } else {
    tft.setTextColor(COLOR_ACCENT, COLOR_PANEL);
    tft.drawString("POWER ON", x + 48, y + 94);
  }
}

void drawAnimationFrame() {
  constexpr int16_t x = 12;
  constexpr int16_t y = 52;
  const uint8_t* frame = reinterpret_cast<const uint8_t*>(
      pgm_read_ptr(&LOGO_FRAMES[currentFrame]));
  logoSprite.fillSprite(COLOR_BG);
  logoSprite.drawXBitmap(0, 0, frame, LOGO_WIDTH, LOGO_HEIGHT,
                         COLOR_LOGO, COLOR_BG);
  logoSprite.pushSprite(x, y);
  currentFrame = (currentFrame + 1) % LOGO_FRAME_COUNT;
}
}  // namespace

void setup() {
  Serial.begin(115200);

  // Keep the panel quiet and dark until its 3.3 V supply and the ESP32-S3 GPIO
  // rails have settled. This module was intermittently missing
  // initialization when both devices started at the same instant.
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, LOW);
  pinMode(LCD_CS_PIN, OUTPUT);
  pinMode(LCD_DC_PIN, OUTPUT);
  pinMode(LCD_MOSI_PIN, OUTPUT);
  pinMode(LCD_SCLK_PIN, OUTPUT);
  digitalWrite(LCD_CS_PIN, HIGH);
  digitalWrite(LCD_DC_PIN, HIGH);
  digitalWrite(LCD_MOSI_PIN, LOW);
  digitalWrite(LCD_SCLK_PIN, LOW);
  delay(1000);

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);

  tft.init();
  tft.setRotation(DISPLAY_ROTATION);
  tft.fillScreen(COLOR_BG);
  tft.setTextWrap(false);

  if constexpr (LCD_DIAGNOSTIC) {
    tft.fillScreen(TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("LCD TEST", 120, 140);
    digitalWrite(BACKLIGHT_PIN, HIGH);
    Serial.println("LCD diagnostic started");
    return;
  }

  logoSprite.setColorDepth(16);
  if (!logoSprite.createSprite(LOGO_WIDTH, LOGO_HEIGHT)) {
    Serial.println("ERROR: logo sprite allocation failed");
  }

  drawHeader();
  drawPowerStatus();
  drawAnimationFrame();
  delay(100);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  Serial.println("Biscuit Cluster Display ready");
  Serial.printf("Logo frames: %u, display: %dx%d\n", LOGO_FRAME_COUNT, tft.width(), tft.height());
  Serial.printf("PSRAM: %u bytes\n", ESP.getPsramSize());
}

void loop() {
  if constexpr (LCD_DIAGNOSTIC) {
    static uint8_t testColor = 0;
    static uint32_t nextColorAt = 0;
    const uint32_t now = millis();
    if (static_cast<int32_t>(now - nextColorAt) >= 0) {
      nextColorAt = now + 2000;
      static constexpr uint16_t colors[] = {
          TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
      static const char* labels[] = {"RED", "GREEN", "BLUE", "WHITE"};
      const uint16_t color = colors[testColor];
      tft.fillScreen(color);
      tft.setTextDatum(MC_DATUM);
      tft.setTextFont(4);
      tft.setTextColor(testColor == 3 ? TFT_BLACK : TFT_WHITE, color);
      tft.drawString(labels[testColor], 120, 140);
      testColor = (testColor + 1) & 3;
    }
    delay(1);
    return;
  }

  if constexpr (STATIC_STABILITY_TEST) {
    digitalWrite(BACKLIGHT_PIN, HIGH);
    delay(10);
    return;
  }

  const uint32_t now = millis();
  if (static_cast<int32_t>(now - nextFrameAt) >= 0) {
    nextFrameAt = now + LOGO_FRAME_MS;
    drawAnimationFrame();
  }
  if (static_cast<int32_t>(now - nextPowerRefreshAt) >= 0) {
    nextPowerRefreshAt = now + 5000;
    drawPowerStatus();
  }
  delay(1);
}

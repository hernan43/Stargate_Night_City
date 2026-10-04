// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <si5351.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include "Control.h"
#include "CpuProfiles.h"
#include "BoardConfig.h"
#include "Lighting.h"
#include "EffectsConfig.h"
#include "StargatePreferenceStore.h"

static RadioIntent sgRadio;
static Press sgSwitch, sgKeys[3];
static Press sgTurboSwitch;
// PCB SW3 shorts ESP GPIO12 to GND when pressed.
static constexpr unsigned SG_SW3_PIN = 12;
static Si5351 sgClock;
static Adafruit_SSD1306 sgOled(128, 32, &Wire, -1, 400000, 400000);
static bool sgClockOK = false, sgOledOK = false;
static constexpr unsigned SG_CPU_PAGE = 6;
static unsigned sgPage = SG_CPU_PAGE, sgAdc = 0;
static uint32_t sgSampleAt = 0, sgDrawAt = 0;
static constexpr uint32_t SG_OLED_IDLE_MS = 30000;
static uint32_t sgOledActivityAt = 0;
static bool sgOledSleeping = false;
static bool sgEditing = false;
static constexpr uint32_t SG_MENU_IDLE_MS = 10000;

void stargateOledTimeout(uint32_t now) {
  if (uint32_t(now - sgOledActivityAt) >= SG_MENU_IDLE_MS &&
      (sgPage != SG_CPU_PAGE || sgEditing)) {
    sgPage = SG_CPU_PAGE;
    sgEditing = false;
    sgDrawAt = now - 250;
  }
  if (!sgOledOK || sgOledSleeping ||
      uint32_t(now - sgOledActivityAt) < SG_OLED_IDLE_MS) return;
  sgOled.ssd1306_command(SSD1306_DISPLAYOFF);
  sgOledSleeping = true;
}
static constexpr unsigned SG_BT_PIN = 16, SG_RGB_PIN = 14, SG_PAGE_COUNT = 11;
// Logical page IDs stay stable; this is the order seen from the startup screen.
static constexpr unsigned SG_PAGE_ORDER[SG_PAGE_COUNT] = {6, 0, 1, 2, 3, 4, 5, 10, 8, 7, 9};
static unsigned sgCpuSpeed = 0, sgCpuPending = 0;
static bool sgCpuQueued = false, sgCpuFailed = false;

void stargateToggleCpu() {
  sgCpuPending = 1 - (sgCpuQueued ? sgCpuPending : sgCpuSpeed);
  sgCpuQueued = true; sgCpuFailed = false;
  sgDrawAt = millis() - 250;
}

// Called only from the top-level loop, outside an active host transaction.
void stargateApplyCpuWhenIdle(bool idle) {
  if (!sgCpuQueued || !idle || Serial.available() || !sgClockOK) return;
  sgCpuQueued = false;
  sgCpuFailed = sgClock.set_freq(uint64_t(SG_CPU_HZ[sgCpuPending]) * 100ULL, SI5351_CLK2) != 0;
  if (!sgCpuFailed) sgCpuSpeed = sgCpuPending;
  sgDrawAt = millis() - 250;
}

unsigned stargateAdjacentPage(unsigned current, bool next) {
  for (unsigned i = 0; i < SG_PAGE_COUNT; ++i) {
    if (SG_PAGE_ORDER[i] != current) continue;
    return SG_PAGE_ORDER[(i + (next ? 1 : SG_PAGE_COUNT - 1)) % SG_PAGE_COUNT];
  }
  return SG_CPU_PAGE;
}
static bool sgBtOn = SG_BT_DEFAULT_ON;
static LightMode sgLightMode = LightMode::Off;
static unsigned sgLightColor = 0, sgBrightness = SG_RGB_DEFAULT_BRIGHTNESS;
static unsigned sgLedCount = SG_RGB_DEFAULT_COUNT;
static Adafruit_NeoPixel sgPixels(SG_RGB_MAX_LEDS, SG_RGB_PIN, NEO_GRB + NEO_KHZ800);
static bool sgPixelsOK = false, sgRgbDirty = true;
static uint32_t sgRgbEpoch = 0, sgRgbAt = 0, sgRgbBusyAt = 0;
static uint32_t sgRgbLastPacked = 0;
static bool sgWifiBootOn = false;
static StargatePreferenceStore sgPreferenceStore;
static StargatePreferences sgPendingPreferences;
enum class SettingsNotice { None, Queued, Saved, Unchanged, Failed };
static SettingsNotice sgSettingsNotice = SettingsNotice::None;

StargatePreferences stargateCurrentPreferences() {
  StargatePreferences p;
  p.bluetooth = sgBtOn; p.wifiAtBoot = sgWifiBootOn; p.mode = sgLightMode;
  p.color = sgLightColor; p.brightness = sgBrightness; p.count = sgLedCount;
  return p;
}

void stargateLoadPreferences(bool filesystemReady) {
  StargatePreferences p; // Fallback defaults if no valid record can be read.
  sgPreferenceStore.begin(filesystemReady, p);
  sgBtOn = p.bluetooth; sgWifiBootOn = p.wifiAtBoot;
  sgLightMode = p.mode; sgLightColor = p.color;
  sgBrightness = p.brightness; sgLedCount = p.count;
  digitalWrite(SG_BT_PIN, sgBtOn ? HIGH : LOW);
  sgRadio.restoreAtBoot(sgWifiBootOn);
  sgRgbEpoch = millis(); sgRgbDirty = true;
  sgSettingsNotice = SettingsNotice::None;
}

void stargateSaveWhenIdle(bool protocolIdle) {
  if (sgSettingsNotice != SettingsNotice::Queued || !protocolIdle || Serial.available()) return;
  const PreferenceSaveResult result = sgPreferenceStore.save(sgPendingPreferences);
  sgSettingsNotice = result == PreferenceSaveResult::Saved ? SettingsNotice::Saved :
                     result == PreferenceSaveResult::Unchanged ? SettingsNotice::Unchanged :
                     SettingsNotice::Failed;
  sgDrawAt = millis() - 250;
}

void stargateEffectsBegin() {
  // GPIO16 drives R41 -> PC817 -> Q3 -> Bluetooth module power: HIGH = on.
  digitalWrite(SG_BT_PIN, sgBtOn ? HIGH : LOW);
  pinMode(SG_BT_PIN, OUTPUT);
  sgPixels.begin();
  sgPixelsOK = sgPixels.numPixels() == SG_RGB_MAX_LEDS;
  if (sgPixelsOK) { sgPixels.clear(); sgPixels.show(); }
  sgRgbEpoch = millis();
}

// Only the top-level loop calls this, never a nested UNAPI wait. WS2812
// transmission briefly masks interrupts, so defer during host activity.
void stargateUpdateRgb(bool protocolIdle) {
  const uint32_t now = millis();
  if (!protocolIdle || Serial.available()) { sgRgbBusyAt = now; return; }
  if (!sgPixelsOK || uint32_t(now - sgRgbBusyAt) < 3 ||
      uint32_t(now - sgRgbAt) < 40 || !sgPixels.canShow()) return;
  sgRgbAt = now;
  const LightColor c = lightingFrame(sgLightMode, sgLightColor, sgBrightness,
                                     uint32_t(now - sgRgbEpoch));
  const uint32_t packed = sgPixels.Color(c.r, c.g, c.b);
  if (!sgRgbDirty && packed == sgRgbLastPacked) return;
  // Always clear the unused tail, including when LED count is reduced.
  for (unsigned i = 0; i < SG_RGB_MAX_LEDS; ++i)
    sgPixels.setPixelColor(i, i < sgLedCount ? packed : 0);
  sgPixels.show();
  sgRgbLastPacked = packed;
  sgRgbDirty = false;
}

void stargateMenuPress(unsigned key, bool actualWifi, uint32_t now) {
  if (key < 1 || key > 3) return;
  sgOledActivityAt = now;
  if (sgOledSleeping) {
    sgPage = SG_CPU_PAGE;
    sgEditing = false;
    if (sgOledOK) sgOled.ssd1306_command(SSD1306_DISPLAYON);
    sgOledSleeping = false;
    sgDrawAt = now - 250; // Refresh at the next opportunity.
    return; // A wake press must not navigate, toggle Bluetooth, or edit RGB.
  }
  if (sgEditing) {
    if (key == 3) { sgEditing = false; return; }
    const int direction = key == 1 ? -1 : 1;
    if (sgPage == 2) sgLightMode = LightMode((int(sgLightMode) + direction + 4) % 4);
    else if (sgPage == 3) sgLightColor = (int(sgLightColor) + direction + 7) % 7;
    else if (sgPage == 4) sgBrightness = ((int(sgBrightness / 10) - 1 + direction + 10) % 10 + 1) * 10;
    else if (sgPage == 5) sgLedCount = (int(sgLedCount) - 1 + direction + int(SG_RGB_MAX_LEDS)) % SG_RGB_MAX_LEDS + 1;
    sgRgbEpoch = now;
    sgRgbDirty = true;
  } else if (key == 1) sgPage = stargateAdjacentPage(sgPage, false);
  else if (key == 2) sgPage = stargateAdjacentPage(sgPage, true);
  else if (sgPage == 0) sgRadio.toggle(actualWifi);
  else if (sgPage == 1) {
    sgBtOn = !sgBtOn;
    digitalWrite(SG_BT_PIN, sgBtOn ? HIGH : LOW);
  } else if (sgPage >= 2 && sgPage <= 5) sgEditing = true;
  else if (sgPage == 8) sgWifiBootOn = !sgWifiBootOn;
  else if (sgPage == 10) {
    stargateToggleCpu();
  }
  else if (sgPage == 9 && sgSettingsNotice != SettingsNotice::Queued) {
    // Snapshot the values now; later edits must not alter an already queued save.
    sgPendingPreferences = stargateCurrentPreferences();
    sgSettingsNotice = SettingsNotice::Queued;
  }
}

// PCB D4 WIFI_LED: GPIO2 -> R37 (220 ohms) -> LED anode; cathode -> GND.
// This is active HIGH, unlike the active-low LED fitted to some ESP modules.
static constexpr unsigned SG_WIFI_LED_PIN = 2;
static bool sgWifiLedBlinking = false, sgWifiLedLevel = false;
static uint32_t sgWifiLedAt = 0;

void stargateUpdateWifiLed(bool actualWifi, uint32_t now) {
  const bool connecting = actualWifi && WiFi.status() != WL_CONNECTED;
  if (!connecting) {
    sgWifiLedBlinking = false;
    sgWifiLedLevel = actualWifi;
  } else if (!sgWifiLedBlinking) {
    sgWifiLedBlinking = true;
    sgWifiLedLevel = true;
    sgWifiLedAt = now;
  } else if (uint32_t(now - sgWifiLedAt) >= 250) {
    sgWifiLedAt = now;
    sgWifiLedLevel = !sgWifiLedLevel;
  }
  digitalWrite(SG_WIFI_LED_PIN, sgWifiLedLevel ? HIGH : LOW);
}

bool stargateManualOff() { return !sgRadio.enabled() || !sgClockOK; }

void stargateHardwareBegin() {
  digitalWrite(SG_WIFI_LED_PIN, LOW);
  pinMode(SG_WIFI_LED_PIN, OUTPUT);
  pinMode(SW4_PIN, INPUT_PULLUP);
  pinMode(SG_SW3_PIN, INPUT_PULLUP);
  sgTurboSwitch.update(digitalRead(SG_SW3_PIN) == LOW, millis());
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  const uint8_t load = STARGATE_SI5351_LOAD_PF == 6 ? SI5351_CRYSTAL_LOAD_6PF :
                       STARGATE_SI5351_LOAD_PF == 8 ? SI5351_CRYSTAL_LOAD_8PF :
                                                   SI5351_CRYSTAL_LOAD_10PF;
  if (sgClock.init(load, STARGATE_SI5351_CRYSTAL_HZ, 0)) {
    for (int i = 0; i < 3; ++i) sgClock.output_enable((si5351_clock)i, 0);
    // Keep CPU tuning separate from SYS/PSG's PLL.
    sgClock.set_ms_source(SI5351_CLK2, SI5351_PLLB);
    bool ok = !sgClock.set_freq(STOCK_CLOCK_CENTIHZ, SI5351_CLK0);
    ok = !sgClock.set_freq(STOCK_CLOCK_CENTIHZ, SI5351_CLK2) && ok;
#if STARGATE_U2_FITTED == 0
    ok = !sgClock.set_freq(STOCK_CLOCK_CENTIHZ / 2, SI5351_CLK1) && ok;
#endif
    // set_freq may enable an output. Explicitly disable CLK1 when U2 drives PSG.
#if STARGATE_U2_FITTED == 1
    sgClock.output_enable(SI5351_CLK1, 0);
#endif
    sgClockOK = ok;
    if (ok) {
      sgClock.output_enable(SI5351_CLK0, 1);
      sgClock.output_enable(SI5351_CLK2, 1);
#if STARGATE_U2_FITTED == 0
      sgClock.output_enable(SI5351_CLK1, 1);
#endif
    } else {
      for (int i = 0; i < 3; ++i) sgClock.output_enable((si5351_clock)i, 0);
    }
  }
  // No Serial debug output: that UART belongs to the MSX protocol.
  sgOledOK = sgOled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, false, false);
  sgOledActivityAt = millis();
  sgOledSleeping = false;
  Wire.setClock(400000);
  stargateEffectsBegin();
}

// This runs inside explicit waits too. Only queue intent here; never close a
// connection while a UNAPI operation is still using its connection object.
void stargatePollButtons(bool actualWifi) {
  const uint32_t now = millis();
  // Serviced from the main loop and existing cooperative connection waits.
  stargateUpdateWifiLed(actualWifi, now);
  // A tiny display-off command can run even when full-frame drawing is deferred.
  // Network traffic, RGB animation and SW4 do not reset the menu idle timer.
  stargateOledTimeout(now);
  if (uint32_t(now - sgSampleAt) < 10) return;
  sgSampleAt = now;
  if (sgSwitch.update(digitalRead(SW4_PIN) == LOW, now)) sgRadio.toggle(actualWifi);
  if (sgTurboSwitch.update(digitalRead(SG_SW3_PIN) == LOW, now)) {
    stargateToggleCpu();
    sgPage = SG_CPU_PAGE;
    sgEditing = false;
    sgOledActivityAt = now;
    if (sgOledSleeping && sgOledOK) sgOled.ssd1306_command(SSD1306_DISPLAYON);
    sgOledSleeping = false;
    sgDrawAt = now - 250;
  }
  sgAdc = analogRead(A0);
  const unsigned key = menuKey(sgAdc);
  for (unsigned i = 0; i < 3; ++i) {
    if (!sgKeys[i].update(key == i + 1, now)) continue;
    stargateMenuPress(i + 1, actualWifi, now);
  }
}

void stargateDrawBadge(int16_t x, const char* label, bool lit) {
  if (lit) sgOled.fillRect(x, 0, 20, 9, SSD1306_WHITE);
  else sgOled.drawRect(x, 0, 20, 9, SSD1306_WHITE);
  sgOled.setTextSize(1);
  sgOled.setTextColor(lit ? SSD1306_BLACK : SSD1306_WHITE);
  sgOled.setCursor(x + 4, 1);
  sgOled.print(label);
  sgOled.setTextColor(SSD1306_WHITE);
}

void stargateDrawCpu(bool actualWifi) {
  // 128x32 terminal-style dashboard, using the existing pixel font.
  sgOled.setCursor(0, 1);
  sgOled.print(sgCpuFailed ? F("CPU//ERROR") : sgCpuQueued ? F("CPU//WAIT") :
               sgCpuSpeed == 0 ? F("CPU//STOCK") : F("CPU//TURBO"));
  sgOled.drawFastHLine(65, 3, 15, SSD1306_WHITE);
  sgOled.drawFastHLine(71, 6, 9, SSD1306_WHITE);
  stargateDrawBadge(86, "WF", actualWifi &&
                    (WiFi.status() == WL_CONNECTED || sgWifiLedLevel));
  stargateDrawBadge(108, "BT", sgBtOn);
  sgOled.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  // Configured clock, not a measured frequency. Keep all six decimal places.
  char speed[16];
  const uint32_t hz = SG_CPU_HZ[sgCpuSpeed];
  if (hz < 10000000UL)
    snprintf(speed, sizeof(speed), "%lu.%06lu", (unsigned long)(hz / 1000000UL),
             (unsigned long)(hz % 1000000UL));
  else snprintf(speed, sizeof(speed), "%lu.%05lu", (unsigned long)(hz / 1000000UL),
                (unsigned long)((hz % 1000000UL) / 10));
  sgOled.setTextSize(2); sgOled.setCursor(2, 14); sgOled.print(speed);
  sgOled.setTextSize(1); sgOled.setCursor(105, 21); sgOled.print(F("MHz"));
  sgOled.drawFastVLine(101, 15, 12, SSD1306_WHITE);
  sgOled.drawFastHLine(0, 31, 128, SSD1306_WHITE);
  sgOled.drawFastVLine(0, 27, 4, SSD1306_WHITE);
  sgOled.drawFastVLine(127, 27, 4, SSD1306_WHITE);
}

// Short titles use 2x2 text. Long titles use 1x2 so their full names fit.
// Both are twice the height of values and hints on this 128x32 display.
void stargatePageTitle(const char* title) {
  sgOled.setTextSize(strlen(title) <= 10 ? 2 : 1, 2);
  sgOled.setCursor(0, 0); sgOled.print(title);
  sgOled.drawFastHLine(0, 15, 128, SSD1306_WHITE);
  sgOled.setTextSize(1); sgOled.setCursor(0, 16);
}

void stargateDraw(bool actualWifi, bool protocolIdle) {
  if (!sgOledOK || sgOledSleeping || !protocolIdle || Serial.available()) return;
  const uint32_t now = millis();
  if (uint32_t(now - sgDrawAt) < 250) return;
  sgDrawAt = now;
  sgOled.clearDisplay(); sgOled.setTextColor(SSD1306_WHITE);
  sgOled.setTextWrap(false);
  sgOled.setTextSize(1); sgOled.setCursor(0, 0);
  if (sgClockOK && sgPage == SG_CPU_PAGE) {
    stargateDrawCpu(actualWifi);
    sgOled.display();
    return;
  }
  if (!sgClockOK) {
    stargatePageTitle("CLOCK ERROR");
    sgOled.println(F("Clock init failed"));
    sgOled.println(F("Do not use MSX yet"));
  } else if (sgPage == 0) {
    stargatePageTitle("WIFI");
    sgOled.setCursor(62, 4);
    sgOled.print(!actualWifi ? F("OFF") : WiFi.status() == WL_CONNECTED ? F("CONNECTED") : F("CONNECTING"));
    sgOled.setCursor(0, 16); sgOled.print(F("IP: "));
    if (actualWifi && WiFi.status() == WL_CONNECTED) sgOled.println(WiFi.localIP());
    else sgOled.println(actualWifi ? F("waiting...") : F("--"));
    sgOled.println(sgRadio.queued() ? F("Change queued...") : F("SW4 / SEL: toggle"));
  } else if (sgPage == 1) {
    stargatePageTitle("BLUETOOTH");
    sgOled.println(sgBtOn ? F("Power: ON") : F("Power: OFF"));
    sgOled.println(F("SEL: toggle"));
  } else if (sgPage >= 2 && sgPage <= 5) {
    if (sgPage == 2) {
      stargatePageTitle("RGB MODE");
      sgOled.println(SG_MODE_NAMES[unsigned(sgLightMode)]);
    } else if (sgPage == 3) {
      stargatePageTitle("RGB COLOR");
      sgOled.print(SG_COLOR_NAMES[sgLightColor]);
      sgOled.println(sgLightMode == LightMode::Cycle ? F(" / auto cycle") : F(""));
    } else if (sgPage == 4) {
      stargatePageTitle("BRIGHTNESS");
      sgOled.print(sgBrightness); sgOled.println(F("%"));
    } else {
      stargatePageTitle("LED COUNT");
      sgOled.print(sgLedCount); sgOled.println(F(" LEDs"));
    }
    if (!sgPixelsOK) sgOled.println(F("RGB buffer failed"));
    else sgOled.println(sgEditing ? F("< > adjust  SEL done") : F("SEL: edit"));
  } else if (sgPage == 7) {
    stargatePageTitle("DIAGNOSTICS");
    sgOled.print(F("Button ADC: ")); sgOled.println(sgAdc);
    sgOled.println(F("Night City v0.5.2"));
  } else if (sgPage == 8) {
    stargatePageTitle("WIFI AT BOOT");
    sgOled.print(sgWifiBootOn ? F("ON") : F("OFF"));
    sgOled.println(F(" / save to keep"));
    sgOled.println(F("SEL: toggle"));
  } else if (sgPage == 10) {
    stargatePageTitle("CPU MODE");
    const unsigned mode = sgCpuQueued ? sgCpuPending : sgCpuSpeed;
    sgOled.println(mode ? F("TURBO 1.5x / 5.369318") : F("STOCK / 3.579545 MHz"));
    if (sgCpuFailed) sgOled.println(F("Apply failed: retry"));
    else if (sgCpuQueued) sgOled.println(F("Waiting for idle..."));
    else sgOled.println(F("SW3 / SEL: toggle"));
  } else if (sgPage == 9) {
    stargatePageTitle("SAVE SETTINGS");
    if (!sgPreferenceStore.ready()) sgOled.println(F("Storage unavailable"));
    else if (sgSettingsNotice == SettingsNotice::Queued) sgOled.println(F("Waiting for idle..."));
    else if (sgSettingsNotice == SettingsNotice::Failed) sgOled.println(F("Save failed: retry"));
    else if (!sgPreferenceStore.matches(stargateCurrentPreferences())) sgOled.println(F("Unsaved changes"));
    else if (sgSettingsNotice == SettingsNotice::Saved) sgOled.println(F("Settings saved"));
    else if (sgSettingsNotice == SettingsNotice::Unchanged) sgOled.println(F("Already saved"));
    else sgOled.println(F("Settings up to date"));
    sgOled.println(F("SEL: save"));
  }
  sgOled.display();
}

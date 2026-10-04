// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "Lighting.h"
#include "EffectsConfig.h"

struct StargatePreferences {
  bool bluetooth = SG_BT_DEFAULT_ON;
  bool wifiAtBoot = false;
  LightMode mode = LightMode::Off;
  uint8_t color = 0;
  uint8_t brightness = SG_RGB_DEFAULT_BRIGHTNESS;
  uint8_t count = SG_RGB_DEFAULT_COUNT;
};

inline bool samePreferences(const StargatePreferences& a, const StargatePreferences& b) {
  return a.bluetooth == b.bluetooth && a.wifiAtBoot == b.wifiAtBoot &&
         a.mode == b.mode && a.color == b.color &&
         a.brightness == b.brightness && a.count == b.count;
}

inline bool validPreferences(const StargatePreferences& p) {
  return unsigned(p.mode) <= unsigned(LightMode::Cycle) && p.color < 7 &&
         p.brightness >= 10 && p.brightness <= 100 && p.brightness % 10 == 0 &&
         p.count >= 1 && p.count <= SG_RGB_MAX_LEDS;
}

static constexpr size_t SG_PREF_BYTES = 20;
inline uint32_t preferenceCrc(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
  }
  return ~crc;
}
inline void preferencePut32(uint8_t* out, uint32_t n) {
  for (unsigned i = 0; i < 4; ++i) out[i] = uint8_t(n >> (8 * i));
}
inline uint32_t preferenceGet32(const uint8_t* in) {
  return uint32_t(in[0]) | (uint32_t(in[1]) << 8) |
         (uint32_t(in[2]) << 16) | (uint32_t(in[3]) << 24);
}
inline void encodePreferences(const StargatePreferences& p, uint32_t generation,
                              uint8_t* out) {
  out[0] = 'S'; out[1] = 'G'; out[2] = 'P'; out[3] = 'F'; out[4] = 4;
  out[5] = p.bluetooth; out[6] = p.wifiAtBoot; out[7] = uint8_t(p.mode);
  out[8] = p.color; out[9] = p.brightness; out[10] = p.count;
  out[11] = 0;
  preferencePut32(out + 12, generation);
  preferencePut32(out + 16, preferenceCrc(out, 16));
}
inline bool decodePreferences(const uint8_t* in, size_t length,
                              StargatePreferences& out, uint32_t& generation) {
  if (length != SG_PREF_BYTES || in[0] != 'S' || in[1] != 'G' ||
      in[2] != 'P' || in[3] != 'F' || (in[4] < 1 || in[4] > 4) || in[5] > 1 ||
      in[6] > 1 || ((in[4] == 1 || in[4] == 4) && in[11] != 0) ||
      preferenceGet32(in + 16) != preferenceCrc(in, 16)) return false;
  StargatePreferences p;
  p.bluetooth = in[5]; p.wifiAtBoot = in[6]; p.mode = LightMode(in[7]);
  p.color = in[8]; p.brightness = in[9]; p.count = in[10];
  // Validate obsolete profile indices, then discard them. Boot remains stock.
  if (in[4] == 2 && ((in[11] & 15) > 8 || (in[11] >> 4) > 8)) return false;
  if (in[4] == 3 && ((in[11] & 15) > 4 || (in[11] >> 4) > 4)) return false;
  if (!validPreferences(p)) return false;
  out = p;
  generation = preferenceGet32(in + 12);
  return true;
}

inline bool preferenceGenerationNewer(uint32_t a, uint32_t b) {
  const uint32_t delta = a - b;
  return delta != 0 && delta < 0x80000000u;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

enum class LightMode : uint8_t { Off, Solid, Breathe, Cycle };
struct LightColor { uint8_t r, g, b; };
static constexpr LightColor SG_RAINBOW[] = {
  {255, 0, 0}, {255, 80, 0}, {255, 255, 0}, {0, 255, 0},
  {0, 0, 255}, {75, 0, 130}, {148, 0, 211}
};
static constexpr const char* SG_COLOR_NAMES[] = {
  "Red", "Orange", "Yellow", "Green", "Blue", "Indigo", "Violet"
};
static constexpr const char* SG_MODE_NAMES[] = {"Off", "Solid", "Breathe", "Color cycle"};

// Pure calculation: no delays, hardware access, or dynamic allocation.
inline LightColor lightingFrame(LightMode mode, unsigned color, unsigned percent,
                               uint32_t elapsed) {
  if (mode == LightMode::Off) return {0, 0, 0};
  if (percent > 100) percent = 100;
  LightColor c = SG_RAINBOW[color % 7];
  uint32_t level = 255;
  if (mode == LightMode::Breathe) {
    const uint32_t phase = elapsed % 4000;
    const uint32_t ramp = phase <= 2000 ? phase : 4000 - phase;
    const uint32_t x = ramp * 255 / 2000;
    // Smoothstep rounds the peaks; square approximates perceptual dimming.
    const uint32_t smooth = x * x * (765 - 2 * x) / 65025;
    level = smooth * smooth / 255;
  } else if (mode == LightMode::Cycle) {
    const uint32_t phase = elapsed % 14000;
    const unsigned i = phase / 2000;
    const uint32_t t = phase % 2000;
    const LightColor a = SG_RAINBOW[i], b = SG_RAINBOW[(i + 1) % 7];
    c = {uint8_t((a.r * (2000 - t) + b.r * t) / 2000),
         uint8_t((a.g * (2000 - t) + b.g * t) / 2000),
         uint8_t((a.b * (2000 - t) + b.b * t) / 2000)};
  }
  return {uint8_t(uint32_t(c.r) * percent * level / 25500),
          uint8_t(uint32_t(c.g) * percent * level / 25500),
          uint8_t(uint32_t(c.b) * percent * level / 25500)};
}

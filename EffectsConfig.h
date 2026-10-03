// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// These are power-on defaults; OLED adjustments last until reset.
static constexpr bool SG_BT_DEFAULT_ON = false;
static constexpr unsigned SG_RGB_DEFAULT_COUNT = 80;
static constexpr unsigned SG_RGB_DEFAULT_BRIGHTNESS = 50; // percent, 10..100

// 32 pixels take about 960 us to transmit on GPIO14. Keep this limit for
// the first networking + lighting test; longer strips need timing review.
static constexpr unsigned SG_RGB_MAX_LEDS = 120;
static_assert(SG_RGB_DEFAULT_COUNT >= 1 && SG_RGB_DEFAULT_COUNT <= SG_RGB_MAX_LEDS,
              "RGB default count must be 1..32");
static_assert(SG_RGB_DEFAULT_BRIGHTNESS >= 10 && SG_RGB_DEFAULT_BRIGHTNESS <= 100 &&
              SG_RGB_DEFAULT_BRIGHTNESS % 10 == 0, "Use 10,20,...,100 percent");

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
// These are selectable targets, not hardware-qualified maximum speeds.
static constexpr uint32_t SG_CPU_HZ[] = {
  3579545UL, 5369318UL
};
// 1.5x is rounded to the nearest whole Hz, matching the menu precision.
static constexpr unsigned SG_CPU_SPEED_COUNT = sizeof(SG_CPU_HZ) / sizeof(SG_CPU_HZ[0]);

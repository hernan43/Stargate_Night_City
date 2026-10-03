// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// These are evidence gates, not optional feature switches. Confirm the physical
// board before changing them. This source package is NOT ready to flash.
#define STARGATE_CLOCK_WIRING_VERIFIED 1
#define STARGATE_U2_FITTED 1
#define STARGATE_SI5351_CRYSTAL_HZ 25000000UL
#define STARGATE_SI5351_LOAD_PF 10

#if !STARGATE_CLOCK_WIRING_VERIFIED
#error "Confirm v2.1.3 clock wiring, JP2/JP3 open, and stock SYS/CPU clocks first."
#endif
#if STARGATE_U2_FITTED != 0 && STARGATE_U2_FITTED != 1
#error "Set U2_FITTED to 1 for a fitted 74HC74 divider, otherwise 0."
#endif
#if STARGATE_SI5351_CRYSTAL_HZ != 25000000UL && STARGATE_SI5351_CRYSTAL_HZ != 27000000UL
#error "Confirm the Si5351 module's 25/27 MHz reference crystal."
#endif
#if STARGATE_SI5351_LOAD_PF != 6 && STARGATE_SI5351_LOAD_PF != 8 && STARGATE_SI5351_LOAD_PF != 10
#error "Confirm reference crystal load capacitance: 6, 8 or 10 pF."
#endif

constexpr unsigned SW4_PIN = 13;
constexpr unsigned SDA_PIN = 4;
constexpr unsigned SCL_PIN = 5;
constexpr unsigned OLED_ADDRESS = 0x3c;
constexpr uint64_t STOCK_CLOCK_CENTIHZ = 357954500ULL;

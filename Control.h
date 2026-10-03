// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

// Hardware-independent debounce. A button held at boot must be released first.
class Press {
  bool initialized = false, candidate = false, stable = false;
  uint32_t since = 0;
public:
  bool update(bool down, uint32_t now) {
    if (!initialized) {
      initialized = true; candidate = stable = down; since = now;
      return false;
    }
    if (down != candidate) { candidate = down; since = now; }
    if (candidate != stable && uint32_t(now - since) >= 40) {
      stable = candidate;
      return stable;
    }
    return false;
  }
};

class RadioIntent {
  bool wanted = false, pending = false;
public:
  bool enabled() const { return wanted; }
  bool queued() const { return pending; }
  void restoreAtBoot(bool enabled) { wanted = enabled; pending = false; }
  void toggle(bool actual) {
    wanted = !(pending ? wanted : actual);
    pending = true;
  }
  void applied() { pending = false; }
};

// Nominal resistor-ladder bands from v2.1.3; verify ADC values on real hardware.
inline unsigned menuKey(unsigned adc) {
  if (adc < 150) return 0;
  if (adc < 500) return 3; // SW8
  if (adc < 850) return 2; // SW7
  return 1;               // SW6
}

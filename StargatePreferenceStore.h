// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <LittleFS.h>
#include "StargatePreferences.h"

enum class PreferenceSaveResult { Saved, Unchanged, Failed };

// Two independent, checksummed slots. Never overwrite the newest valid slot
// until the replacement has been closed, reopened and verified.
class StargatePreferenceStore {
  bool mounted = false;
  int active = -1;
  uint32_t generation = 0;
  StargatePreferences saved;

  const char* path(int slot) const {
    return slot == 0 ? "/stargate0.cfg" : "/stargate1.cfg";
  }
  bool read(int slot, StargatePreferences& p, uint32_t& gen) {
    File file = LittleFS.open(path(slot), "r");
    if (!file) return false;
    uint8_t bytes[SG_PREF_BYTES];
    const bool sized = file.size() == sizeof(bytes);
    const bool complete = sized && file.read(bytes, sizeof(bytes)) == int(sizeof(bytes));
    file.close();
    return complete && decodePreferences(bytes, sizeof(bytes), p, gen);
  }
public:
  bool ready() const { return mounted; }
  bool hasSaved() const { return active >= 0; }
  bool matches(const StargatePreferences& p) const {
    return hasSaved() && samePreferences(p, saved);
  }
  bool begin(bool filesystemReady, StargatePreferences& out) {
    mounted = filesystemReady; active = -1; generation = 0;
    if (!mounted) return false;
    StargatePreferences a, b;
    uint32_t ga = 0, gb = 0;
    const bool va = read(0, a, ga), vb = read(1, b, gb);
    if (!va && !vb) return false;
    active = vb && (!va || preferenceGenerationNewer(gb, ga)) ? 1 : 0;
    saved = active == 0 ? a : b;
    generation = active == 0 ? ga : gb;
    out = saved;
    return true;
  }
  PreferenceSaveResult save(const StargatePreferences& p) {
    if (!mounted || !validPreferences(p)) return PreferenceSaveResult::Failed;
    if (matches(p)) return PreferenceSaveResult::Unchanged;
    const int target = active == 0 ? 1 : 0;
    const uint32_t next = generation + 1;
    uint8_t bytes[SG_PREF_BYTES];
    encodePreferences(p, next, bytes);
    File file = LittleFS.open(path(target), "w");
    if (!file) return PreferenceSaveResult::Failed;
    const bool complete = file.write(bytes, sizeof(bytes)) == sizeof(bytes);
    file.flush();
    file.close();
    StargatePreferences check;
    uint32_t checkGeneration = 0;
    if (!complete || !read(target, check, checkGeneration) ||
        checkGeneration != next || !samePreferences(p, check))
      return PreferenceSaveResult::Failed;
    saved = p; active = target; generation = next;
    return PreferenceSaveResult::Saved;
  }
};

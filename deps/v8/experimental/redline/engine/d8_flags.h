// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_D8_FLAGS_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_D8_FLAGS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "experimental/redline/core/random.h"

namespace redline {

// The flag profiles of RandomD8Flags. Both profiles draw the common tier-up,
// checking and bytecode flags, the flags of the Maglev graph builder, the CPU
// feature flags and the garbage collection stress flags. kMaglev adds Maglev
// features and inlining limits.
// kTurbolev adds the Turbolev, Turboshaft and backend features, the inlining
// limits of Turbolev, the concurrent recompilation flags and the
// --stress-concurrent-inlining flags.
enum class D8FlagProfile { kMaglev, kTurbolev };

// Parses "maglev" or "turbolev". Returns false for another name.
bool ParseD8FlagProfile(const std::string& name, D8FlagProfile* profile);
const char* D8FlagProfileName(D8FlagProfile profile);

// A random set of d8 flags for one target process, like the random part of
// the V8 profile of Fuzzilli. The flags change how V8 compiles and checks
// code: compiler features, inlining limits, assertions, tier-up, feedback
// allocation and garbage collection. Each flag has a probability, which is a
// tunable (flags.*). A flag with a value also has tunables for the range of
// the value. A session with randomize_d8_flags draws a new set for each new
// target process. The flags come before `fixed_args`, so that a flag of the
// configuration wins. `RandomD8Flags` drops flags that clash with `fixed_args`.
std::vector<std::string> RandomD8Flags(
    Random& rng, D8FlagProfile profile = D8FlagProfile::kMaglev,
    const std::vector<std::string>& fixed_args = {});

// Removes duplicate, orphan and clashing flags from `flags`.
void SanitizeD8Flags(std::vector<std::string>* flags,
                     const std::vector<std::string>& fixed_args = {});

// Combines a target-hitting flag set `base` with a fresh random flag draw
// `extra`. Keeps each flag of `base` with probability flags.target_keep_chance,
// appends new flags from `extra`, and removes orphan or clashing flags.
std::vector<std::string> CombineD8Flags(
    const std::vector<std::string>& base, const std::vector<std::string>& extra,
    Random& rng, const std::vector<std::string>& fixed_args = {});

double TargetGuidedChance();
int TargetGuidedWarmup();
// Multiplies `TargetGuidedChance()` by `(1 - hit_rate)` after
// `completed_children` reaches `TargetGuidedWarmup()`.
double EffectiveTargetGuidedChance(uint64_t completed_children,
                                   uint64_t hit_children);
double TargetMutateChance();
int TargetTournament();
int TargetProbeExecs();

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_D8_FLAGS_H_

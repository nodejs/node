// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_D8_FLAGS_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_D8_FLAGS_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/core/random.h"

namespace redline {

// The flag profiles of RandomD8Flags. kMaglev and kTurbolev draw the common
// tier-up, checking and bytecode flags. They also draw the flags of the Maglev
// graph builder, the CPU feature flags and the garbage collection stress
// flags. kMaglev adds Maglev features and inlining limits.
// kTurbolev adds the Turbolev, Turboshaft and backend features, the inlining
// limits of Turbolev, the concurrent recompilation flags, the
// --stress-concurrent-inlining flags and the Turbofan verifiers. kRuntime
// draws only the common flags that do not act on a JIT tier: inline caches,
// lazy parsing, bytecode, shared strings and garbage collection. kFuzzilli
// draws the non-Wasm flags of the V8 profile of Fuzzilli with the same
// probabilities, ranges and conditions. kFuzzilliWasm also draws the Wasm
// flags of that profile.
enum class D8FlagProfile {
  kMaglev,
  kTurbolev,
  kFuzzilli,
  kFuzzilliWasm,
  kRuntime
};

// Parses "maglev", "turbolev", "fuzzilli", "fuzzilli-wasm" or "runtime".
// Returns false for another name.
bool ParseD8FlagProfile(const std::string& name, D8FlagProfile* profile);
const char* D8FlagProfileName(D8FlagProfile profile);

// A random set of d8 flags for one target process, like the random part of
// the V8 profile of Fuzzilli. The flags change how V8 compiles and checks
// code: compiler features, inlining limits, assertions, tier-up, feedback
// allocation and garbage collection. In kMaglev, kTurbolev and kRuntime, each
// flag has a tunable (flags.*) for its probability and tunables for its value
// range. kFuzzilli and kFuzzilliWasm keep the fixed probabilities and value
// ranges of Fuzzilli instead of flags.* tunables. A session with
// randomize_d8_flags draws a new set for each new target process. The flags
// come before `fixed_args`, so that a flag of the configuration wins.
// `RandomD8Flags` drops flags that clash with `fixed_args`.
std::vector<std::string> RandomD8Flags(
    Random& rng, D8FlagProfile profile = D8FlagProfile::kMaglev,
    const std::vector<std::string>& fixed_args = {});

// Removes duplicate, orphan and clashing flags from `flags`. Two flags with
// the same name, or a flag and its `--no-` form, are duplicates: the first one
// stays. For the contradictory pairs of the js_fuzzer of V8, the flag of
// `fixed_args` or the first flag stays.
void SanitizeD8Flags(std::vector<std::string>* flags,
                     const std::vector<std::string>& fixed_args = {});

// Combines a target-hitting flag set `base` with a fresh random flag draw
// `extra`. Keeps each flag of `base` with probability flags.target_keep_chance,
// appends new flags from `extra`, and removes orphan or clashing flags.
std::vector<std::string> CombineD8Flags(
    const std::vector<std::string>& base, const std::vector<std::string>& extra,
    Random& rng, const std::vector<std::string>& fixed_args = {});

// The flag name of `flag`: the text before `=`.
std::string_view D8FlagName(std::string_view flag);

// True if a test can give `flag` to the fuzzer, for example in a `// Flags:`
// line. The denylist comes from tools/clusterfuzz/js_fuzzer/exceptions.js of
// V8. It also rejects the flags that write logs or traces, and the flags that
// change the d8 shell.
bool IsAllowedTestFlag(std::string_view flag);

// Filters `raw` with IsAllowedTestFlag, writes `-` for `_` in the flag
// names, and removes clashing flags with SanitizeD8Flags. Adds the rejected
// flags to `rejected` if it is not nullptr.
std::vector<std::string> SanitizeTestFlags(
    const std::vector<std::string>& raw,
    std::vector<std::string>* rejected = nullptr);

// Removes the flags of `flags` that have the name of a flag of `fixed_args`,
// or its `--no-` form. The flag of `fixed_args` comes later and wins.
void RemoveFixedD8Flags(std::vector<std::string>* flags,
                        const std::vector<std::string>& fixed_args);

// The union of the flags of two parents, without duplicate flag names. Keeps
// the flags of `first` before the flags of `second`, and at most
// `max_flags` flags.
std::vector<std::string> MergeProgramFlags(
    const std::vector<std::string>& first,
    const std::vector<std::string>& second, size_t max_flags = 6);

// True if `a` and `b` have a flag with the same name.
bool SharesD8Flag(const std::vector<std::string>& a,
                  const std::vector<std::string>& b);

// One trial of the `clusterfuzz_trials_config.json` file of V8: ClusterFuzz
// adds `args` to a d8 process with `probability`, unless the process already
// has a flag of `contradicts`.
struct D8Trial {
  std::vector<std::string> args;
  double probability = 0;
  std::vector<std::string> contradicts;
};

// Parses the trials of `json`, in the format of
// tools/clusterfuzz/trials/clusterfuzz_trials_config.json. Keeps the trials
// with `app_name` "d8". Drops a trial if IsAllowedTestFlag rejects one of its
// flags, and counts it in `dropped`. Returns false with `error` if the text
// is not a list of trials.
bool ParseD8Trials(std::string_view json, std::vector<D8Trial>* trials,
                   std::string* error, size_t* dropped = nullptr);

// Draws the trials in a random order and adds their flags to `flags`. Each
// trial has its probability. A trial is skipped if it contradicts a flag of
// `flags`, of `fixed_args` or of a trial drawn before. A trial is also skipped
// if it sets a flag name of `flags` or of `fixed_args`.
void AddD8Trials(Random& rng, const std::vector<D8Trial>& trials,
                 const std::vector<std::string>& fixed_args,
                 std::vector<std::string>* flags);

// A bounded pool of d8 flag sets, each with a score. All methods are
// thread-safe.
class FlagSetPool {
 public:
  explicit FlagSetPool(size_t capacity) : capacity_(capacity) {}
  FlagSetPool(const FlagSetPool&) = delete;
  FlagSetPool& operator=(const FlagSetPool&) = delete;

  // Adds `flags` with `score`. If the pool has the same set, keeps the higher
  // score. A full pool replaces its lowest score if `score` is higher. The
  // pool ignores an empty set and a score of 0 or less.
  void Record(const std::vector<std::string>& flags, double score);
  // Multiplies all scores by `factor`.
  void Decay(double factor);
  // Sets `flags` to the set with the highest score of `tournament` random
  // sets. A tournament of 1 picks with equal probability. Returns false if
  // the pool is empty.
  bool Pick(Random& rng, int tournament, std::vector<std::string>* flags) const;
  bool empty() const { return size() == 0; }
  size_t size() const { return size_.load(std::memory_order_acquire); }
  size_t capacity() const { return capacity_; }

 private:
  struct Entry {
    std::vector<std::string> flags;
    double score = 0;
  };
  const size_t capacity_;
  mutable std::mutex mutex_;
  std::vector<Entry> entries_;
  std::atomic<size_t> size_{0};
};

// The random d8 flags that stop a target process before its handshake. A new
// flag is a flag that never started a process. A failed start with one random
// flag blocks that flag if it is new. After kBlacklistMinStarts started
// processes, a failed start blocks its only new flag. Else it blocks the new
// flag that is in the most failed starts. That flag must be in more failed
// starts than each other new flag, and in kBlacklistRepeatedFailures failed
// starts or more. All methods are thread-safe.
class D8FlagBlacklist {
 public:
  // Records a start of a target process with the random flags `flags`.
  // Returns true if the start blocked a new flag.
  bool RecordSpawn(const std::vector<std::string>& flags, bool started);
  // Removes the blocked flags from `flags`.
  void Filter(std::vector<std::string>* flags) const;
  bool IsBlocked(std::string_view flag) const;
  // The blocked flag names, sorted.
  std::vector<std::string> Blocked() const;
  uint64_t failed_spawns() const {
    return failed_spawns_.load(std::memory_order_relaxed);
  }

 private:
  struct Counts {
    uint32_t started = 0;
    uint32_t failed = 0;
  };
  mutable std::mutex mutex_;
  std::map<std::string, Counts, std::less<>> counts_;
  std::set<std::string, std::less<>> blocked_;
  uint64_t total_started_ = 0;
  std::atomic<bool> any_blocked_{false};
  std::atomic<uint64_t> failed_spawns_{0};
};

// Normal mode with randomize_d8_flags: the probability that a new target
// process starts from the flags of an imported test. The probability that it
// starts from a flag set that found many new edges.
double ImportedFlagsChance();
double FlagAffinityChance();
// The number of seed candidates that a worker examines to find a program
// whose flags share a flag with its target process. 1 turns the bias off.
int SeedAffinityTries();

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

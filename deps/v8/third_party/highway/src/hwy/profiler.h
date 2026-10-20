// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HIGHWAY_HWY_PROFILER_H_
#define HIGHWAY_HWY_PROFILER_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>  // strcmp, strlen

#include <atomic>
#include <functional>

#include "hwy/base.h"
#include "hwy/highway_export.h"

// High precision, low overhead time measurements. Returns exact call counts and
// total elapsed time for user-defined 'zones' (code regions, i.e. C++ scopes).
//
// Uses RAII to capture begin/end timestamps, with user-specified zone names:
// `{ PROFILER_ZONE("name"); /*code*/ }` or the name of the current function:
// `void FuncToMeasure() { PROFILER_FUNC; /*code*/ }`.
//
// You can reduce the overhead by passing `global_idx`, which can be taken from
// the argument to the `ThreadPool::Run` lambda (if the pool was constructed
// with non-default `PoolWorkerMapping`), or from a saved copy of the
// thread-local `Profiler::Thread`: `PROFILER_ZONE2(global_idx, name)`.
//
// The preferred API allows passing flags, such as requesting inclusive time:
// `static const auto zone = profiler.AddZone("name", flags);` and then
// `PROFILER_ZONE3(profiler, global_idx, zone)`.
//
// After all threads exit all zones, call `Profiler::Get().PrintResults()` to
// print call counts and average durations [CPU cycles] to stdout, sorted in
// descending order of total duration.

// If zero, mock `Profiler` and `profiler::Zone` will be defined.
#ifndef PROFILER_ENABLED
#define PROFILER_ENABLED 0
#endif

#if PROFILER_ENABLED
#include <stdio.h>

#include <algorithm>  // std::sort
#include <utility>
#include <vector>

#include "hwy/aligned_allocator.h"
#include "hwy/bit_set.h"
#include "hwy/timer.h"
#endif  // PROFILER_ENABLED

namespace hwy {

// Flags: we want type-safety (enum class) to catch mistakes such as confusing
// zone with flags. Base type (`uint32_t`) ensures it is safe to cast. Defined
// outside the `#if` because callers pass them to `PROFILER_ZONE3`. When adding
// flags, also update `kNumFlags` and `ChildTotalMask`.
enum class ProfilerFlags : uint32_t {
  kDefault = 0,
  // The zone should report cumulative time, including all child zones. If not
  // specified, zones report self-time, excluding child zones.
  kInclusive = 1
};

// Called during `PrintResults` to print results from other modules.
using ProfilerFunc = std::function<void(void)>;

template <size_t kMaxStrings>
class StringTable {
  static constexpr std::memory_order kRelaxed = std::memory_order_relaxed;
  static constexpr std::memory_order kAcq = std::memory_order_acquire;
  static constexpr std::memory_order kRel = std::memory_order_release;

 public:
  // Returns a copy of the `name` passed to `Add` that returned the
  // given `idx`.
  const char* Name(size_t idx) const {
    // `kAcq` so that the string contents are also visible after the pointer is
    // published via `kRelease` store.
    return ptrs_[idx].load(kAcq);
  }

  // Returns `idx < kMaxStrings`. Can be called concurrently. Calls with the
  // same `name` return the same `idx`.
  size_t Add(const char* name) {
    // Linear search if it already exists. `kAcq` ensures we see prior stores.
    const size_t num_strings = next_ptr_.load(kAcq);
    HWY_ASSERT(num_strings < kMaxStrings);
    for (size_t idx = 1; idx < num_strings; ++idx) {
      const char* existing = ptrs_[idx].load(kAcq);
      // `next_ptr_` was published after writing `ptr_`, hence it is non-null.
      HWY_ASSERT(existing != nullptr);
      if (HWY_UNLIKELY(!strcmp(existing, name))) {
        return idx;
      }
    }

    // Copy `name` into `chars_` before publishing the pointer.
    const size_t len = strlen(name) + 1;
    const size_t pos = next_char_.fetch_add(len, kRelaxed);
    HWY_ASSERT(pos + len <= sizeof(chars_));
    strcpy(chars_ + pos, name);  // NOLINT

    for (;;) {
      size_t idx = next_ptr_.load(kRelaxed);
      HWY_ASSERT(idx < kMaxStrings);

      // Attempt to claim the next `idx` via CAS.
      const char* expected = nullptr;
      if (HWY_LIKELY(ptrs_[idx].compare_exchange_weak(expected, chars_ + pos,
                                                      kRel, kRelaxed))) {
        // Publish the new count and make the `ptrs_` write visible.
        next_ptr_.store(idx + 1, kRel);
        HWY_DASSERT(!strcmp(Name(idx), name));
        return idx;
      }

      // We lost the race. `expected` has been updated.
      if (HWY_UNLIKELY(!strcmp(expected, name))) {
        // Done, another thread added the same name. Note that we waste the
        // extra space in `chars_`, which is fine because it is rare.
        HWY_DASSERT(!strcmp(Name(idx), name));
        return idx;
      }

      // Other thread added a different name. Retry with the next slot.
    }
  }

 private:
  std::atomic<const char*> ptrs_[kMaxStrings];
  std::atomic<size_t> next_ptr_{1};  // next idx
  std::atomic<size_t> next_char_{0};
  char chars_[kMaxStrings * 55];
};

#if PROFILER_ENABLED

// Implementation details.
namespace profiler {

HWY_INLINE_VAR constexpr size_t kNumFlags = 1;

// Upper bounds for fixed-size data structures, guarded via HWY_DASSERT:

// Maximum nesting of zones, chosen such that `PerWorker` is 256 bytes.
HWY_INLINE_VAR constexpr size_t kMaxDepth = 13;
// Reports with more than ~50 are anyway difficult to read.
HWY_INLINE_VAR constexpr size_t kMaxZones = 128;
// Upper bound on global worker_idx across all pools. Note that fiber libraries
// can spawn hundreds of threads. Turin has 128-192 cores.
HWY_INLINE_VAR constexpr size_t kMaxWorkers = 256;

// Type-safe wrapper for zone index plus flags, returned by `AddZone`.
class ZoneHandle {
 public:
  ZoneHandle() : bits_(0) {}  // for Accumulator member initialization

  ZoneHandle(size_t zone_idx, ProfilerFlags flags) {
    HWY_DASSERT(0 != zone_idx && zone_idx < kMaxZones);
    const uint32_t flags_u = static_cast<uint32_t>(flags);
    HWY_DASSERT(flags_u < (1u << kNumFlags));
    bits_ = (static_cast<uint32_t>(zone_idx) << kNumFlags) | flags_u;
    HWY_DASSERT(ZoneIdx() == zone_idx);
  }

  ZoneHandle(const ZoneHandle& other) = default;
  ZoneHandle& operator=(const ZoneHandle& other) = default;

  bool operator==(const ZoneHandle other) const { return bits_ == other.bits_; }
  bool operator!=(const ZoneHandle other) const { return bits_ != other.bits_; }

  size_t ZoneIdx() const {
    HWY_DASSERT(bits_ != 0);
    const size_t zone_idx = bits_ >> kNumFlags;
    HWY_DASSERT(0 != zone_idx && zone_idx < kMaxZones);
    return zone_idx;
  }

  bool IsInclusive() const {
    HWY_DASSERT(bits_ != 0);
    return (bits_ & static_cast<uint32_t>(ProfilerFlags::kInclusive)) != 0;
  }

  // Returns a mask to zero/ignore child totals for inclusive zones.
  uint64_t ChildTotalMask() const {
    // With a ternary operator, clang tends to generate a branch.
    // return IsInclusive() ? 0 : ~uint64_t{0};
    const uint32_t bit =
        bits_ & static_cast<uint32_t>(ProfilerFlags::kInclusive);
    return uint64_t{bit} - 1;
  }

 private:
  uint32_t bits_;
};

// Storage for zone names.
class Zones {
 public:
  // Returns a copy of the `name` passed to `AddZone` that returned the
  // given `zone`.
  const char* Name(ZoneHandle zone) const {
    return strings_.Name(zone.ZoneIdx());
  }

  // Can be called concurrently. Calls with the same `name` return the same
  // `ZoneHandle.ZoneIdx()`.
  ZoneHandle AddZone(const char* name, ProfilerFlags flags) {
    return ZoneHandle(strings_.Add(name), flags);
  }

 private:
  StringTable<kMaxZones> strings_;
};

// Allows other classes such as `ThreadPool` to register/unregister a function
// to call during `PrintResults`. This allows us to gather data from the worker
// threads without having to wait until they exit, and decouples the profiler
// from other modules. Thread-safe.
class Funcs {
  static constexpr auto kAcq = std::memory_order_acquire;
  static constexpr auto kRel = std::memory_order_release;

 public:
  // Can be called concurrently with distinct keys.
  void Add(intptr_t key, ProfilerFunc func) {
    HWY_ASSERT(key != 0 && key != kPending);  // reserved values
    HWY_ASSERT(func);                         // not empty

    for (size_t i = 0; i < kMaxFuncs; ++i) {
      intptr_t expected = 0;
      // Lost a race with a concurrent `Add`, try the next slot.
      if (!keys_[i].compare_exchange_strong(expected, kPending, kRel)) {
        continue;
      }
      // We own the slot: move func there.
      funcs_[i] = std::move(func);
      keys_[i].store(key, kRel);  // publishes the `func` write.
      return;
    }

    HWY_ABORT("Funcs::Add: no free slot, increase kMaxFuncs.");
  }

  // Can be called concurrently with distinct keys. It is an error to call this
  // without a prior `Add` of the same key.
  void Remove(intptr_t key) {
    HWY_ASSERT(key != 0 && key != kPending);  // reserved values

    for (size_t i = 0; i < kMaxFuncs; ++i) {
      intptr_t actual = keys_[i].load(kAcq);
      if (actual == key) {
        // In general, concurrent removal is fine, but in this specific context,
        // owners are expected to remove their key exactly once, from the same
        // thread that added it. In that case, CAS should not fail.
        if (!keys_[i].compare_exchange_strong(actual, kPending, kRel)) {
          HWY_WARN("Funcs: CAS failed, why is there a concurrent Remove?");
        }
        funcs_[i] = ProfilerFunc();
        keys_[i].store(0, kRel);  // publishes the `func` write.
        return;
      }
    }
    HWY_ABORT("Funcs::Remove: failed to find key %p.",
              reinterpret_cast<void*>(key));
  }

  void CallAll() const {
    for (size_t i = 0; i < kMaxFuncs; ++i) {
      intptr_t key = keys_[i].load(kAcq);  // ensures `funcs_` is visible.
      // Safely handles concurrent Add/Remove.
      if (key != 0 && key != kPending) {
        funcs_[i]();
      }
    }
  }

 private:
  static constexpr size_t kMaxFuncs = 64;
  static constexpr intptr_t kPending = -1;

  ProfilerFunc funcs_[kMaxFuncs];  // non-atomic
  std::atomic<intptr_t> keys_[kMaxFuncs] = {};
};

// Holds total duration and number of calls. Worker index is implicit in the
// index of this class within the `Accumulators` array.
struct Accumulator {
  void Add(ZoneHandle new_zone, uint64_t self_duration) {
    duration += self_duration;

    // Only called for valid zones.
    HWY_DASSERT(new_zone != ZoneHandle());
    // Our zone might not have been set yet.
    HWY_DASSERT(zone == ZoneHandle() || zone == new_zone);
    zone = new_zone;

    num_calls += 1;
  }

  void Take(Accumulator& other) {
    duration += other.duration;
    other.duration = 0;

    // `ZoneSet` ensures we only call this for non-empty `other`.
    HWY_DASSERT(other.zone != ZoneHandle());
    // Our zone might not have been set yet.
    HWY_DASSERT(zone == ZoneHandle() || zone == other.zone);
    zone = other.zone;

    num_calls += other.num_calls;
    other.num_calls = 0;
  }

  uint64_t duration = 0;
  ZoneHandle zone;  // flags are used by `Results::Print`
  uint32_t num_calls = 0;
};
static_assert(sizeof(Accumulator) == 16, "Wrong Accumulator size");

using ZoneSet = hwy::BitSet<kMaxZones>;
using WorkerSet = hwy::BitSet<kMaxWorkers>;
using AtomicWorkerSet = hwy::AtomicBitSet<kMaxWorkers>;

// Durations are per-CPU, but end to end performance is defined by wall time.
// Assuming fork-join parallelism, zones are entered by multiple threads
// concurrently, which means the total number of unique threads is also the
// degree of concurrency, so we can estimate wall time as CPU time divided by
// the number of unique threads seen. This is facilitated by unique `global_idx`
// passed in by callers, or taken from thread-local `GlobalIdx()`.
//
// We also want to support varying thread counts per call site, because the same
// function/zone may be called from multiple pools. `EndRootRun` calls
// `CountWorkersAndReset` after each top-level `ThreadPool::Run`, which
// generates one data point summarized via descriptive statistics. Here we
// implement a simpler version of `Stats` because we do not require
// geomean/variance/kurtosis/skewness. Because concurrency is a small integer,
// we can simply compute sums rather than online moments. There is also only one
// instance across all threads, hence we do not require a `Take`.
//
// Note that subsequently discovered prior work estimates the number of active
// and idle processors by updating atomic counters whenever they start/finish a
// task: https://homes.cs.washington.edu/~tom/pubs/quartz.pdf and "Effective
// performance measurement and analysis of multithreaded applications". We
// instead accumulate zone durations into per-thread storage.
// `CountWorkersAndReset` then checks how many were nonzero, which avoids
// expensive atomic updates and ensures accurate counts per-zone, rather than
// estimates of current activity at each sample.
// D. Vyukov's https://github.com/dvyukov/perf-load, also integrated into Linux
// perf, also corrects for parallelism without using atomic counters by tracing
// context switches. Note that we often pin threads, which avoids migrations,
// but reduces the number of context switch events to mainly preemptions.
class ConcurrencyStats {
 public:
  ConcurrencyStats() { Reset(); }

  void Notify(const size_t x) {
    sum_ += x;
    ++n_;
    min_ = HWY_MIN(min_, x);
    max_ = HWY_MAX(max_, x);
  }

  size_t Count() const { return n_; }
  size_t Min() const { return min_; }
  size_t Max() const { return max_; }
  double Mean() const {
    return static_cast<double>(sum_) / static_cast<double>(n_);
  }

  void Reset() {
    sum_ = 0;
    n_ = 0;
    min_ = hwy::HighestValue<size_t>();
    max_ = hwy::LowestValue<size_t>();
  }

 private:
  uint64_t sum_;
  size_t n_;
  size_t min_;
  size_t max_;
};
static_assert(sizeof(ConcurrencyStats) == (8 + 3 * sizeof(size_t)), "");

// Holds the final results across all threads, including `ConcurrencyStats`
// and `PoolStats`, updated/printed by the main thread.
class Results {
 public:
  void TakeAccumulator(const size_t global_idx, const size_t zone_idx,
                       Accumulator& other) {
    HWY_DASSERT(global_idx < kMaxWorkers);
    HWY_DASSERT(zone_idx < kMaxZones);
    HWY_DASSERT(other.zone.ZoneIdx() == zone_idx);

    visited_zones_.Set(zone_idx);
    totals_[zone_idx].Take(other);
    workers_[zone_idx].Set(global_idx);
  }

  // Moves the total number of threads seen during the preceding root-level
  // `ThreadPool::Run` into one data point for `ConcurrencyStats`.
  void CountWorkersAndReset(const size_t zone_idx) {
    HWY_DASSERT(zone_idx < kMaxZones);
    const size_t num_workers = workers_[zone_idx].Count();
    // Although workers_[zone_idx] at one point was non-empty, it is reset
    // below, and so can be empty on the second call to this via `PrintResults`,
    // after one from `EndRootRun`. Do not add a data point if empty.
    if (num_workers != 0) {
      concurrency_[zone_idx].Notify(num_workers);
    }
    workers_[zone_idx] = WorkerSet();
  }

  void CountWorkersAndReset() {
    visited_zones_.Foreach(
        [&](size_t zone_idx) { CountWorkersAndReset(zone_idx); });
  }

  void PrintAndReset(const Zones& zones) {
    const double inv_freq = 1.0 / hwy::platform::InvariantTicksPerSecond();

    // Sort by decreasing total (self) cost. `totals_` are sparse, so sort an
    // index vector instead.
    std::vector<uint32_t> indices;
    indices.reserve(visited_zones_.Count());
    visited_zones_.Foreach([&](size_t zone_idx) {
      indices.push_back(static_cast<uint32_t>(zone_idx));
      // In case the zone exited after `EndRootRun` and was not yet added.
      CountWorkersAndReset(zone_idx);
    });
    std::sort(indices.begin(), indices.end(), [&](uint32_t a, uint32_t b) {
      return totals_[a].duration > totals_[b].duration;
    });
    printf("   %-40s: %10s x %15s / %5s (%5s %3s-%3s) = %9s\n", "Zone", "Calls",
           "Cycles/Call", "Avg Count", "Count", "Min", "Max", "Wall Time(s)");

    for (uint32_t zone_idx : indices) {
      Accumulator& total = totals_[zone_idx];  // cleared after printing
      HWY_ASSERT(total.zone.ZoneIdx() == zone_idx);
      HWY_ASSERT(total.num_calls != 0);  // else visited_zones_ is wrong

      ConcurrencyStats& concurrency = concurrency_[zone_idx];
      const double duration = static_cast<double>(total.duration);
      const double per_call =
          static_cast<double>(total.duration) / total.num_calls;
      // See comment on `ConcurrencyStats`.
      const double avg_concurrency = concurrency.Mean();
      // Avoid division by zero.
      const double concurrency_divisor = HWY_MAX(1.0, avg_concurrency);
      printf("%s%-40s: %10.0f x %15.0f / %5.1f (%5zu %3zu-%3zu) = %9.6f\n",
             total.zone.IsInclusive() ? "(I)" : "   ", zones.Name(total.zone),
             static_cast<double>(total.num_calls), per_call, avg_concurrency,
             concurrency.Count(), concurrency.Min(), concurrency.Max(),
             duration * inv_freq / concurrency_divisor);

      total = Accumulator();
      concurrency.Reset();
      // `workers_` was already reset by `CountWorkersAndReset`.
    }
    visited_zones_ = ZoneSet();
  }

 private:
  // Indicates which of the array entries are in use.
  ZoneSet visited_zones_;
  Accumulator totals_[kMaxZones];
  WorkerSet workers_[kMaxZones];
  ConcurrencyStats concurrency_[kMaxZones];
};

// Delay after capturing timestamps before/after the actual zone runs. Even
// with frequency throttling disabled, this has a multimodal distribution,
// including 32, 34, 48, 52, 59, 62.
struct Overheads {
  uint64_t self = 0;
  uint64_t child = 0;
};
static_assert(sizeof(Overheads) == 16, "Wrong Overheads size");

class Accumulators {
  // We generally want to group threads together because they are often
  // accessed together during a zone, but also want to avoid threads sharing a
  // cache line. Hence interleave 8 zones per worker.
  static constexpr size_t kPerLine = HWY_ALIGNMENT / sizeof(Accumulator);

 public:
  Accumulator& Get(const size_t global_idx, const size_t zone_idx) {
    HWY_DASSERT(global_idx < kMaxWorkers);
    HWY_DASSERT(zone_idx < kMaxZones);
    const size_t line = zone_idx / kPerLine;
    const size_t offset = zone_idx % kPerLine;
    return zones_[(line * kMaxWorkers + global_idx) * kPerLine + offset];
  }

 private:
  Accumulator zones_[kMaxZones * kMaxWorkers];
};

// Reacts to zone enter/exit events. Builds a stack of active zones and
// accumulates self/child duration for each.
class PerWorker {
 public:
  template <typename T>
  static T ClampedSubtract(const T minuend, const T subtrahend) {
    static_assert(IsUnsigned<T>(), "");
    const T difference = minuend - subtrahend;
    // Clang output for this is verified to CMOV rather than branch.
    const T no_underflow = (subtrahend > minuend) ? T{0} : ~T{0};
    return difference & no_underflow;
  }

  void SetOverheads(const Overheads& overheads) { overheads_ = overheads; }

  // Entering a zone: push onto stack.
  void Enter(const uint64_t t_enter) {
    const size_t depth = depth_;
    HWY_DASSERT(depth < kMaxDepth);
    t_enter_[depth] = t_enter;
    child_total_[1 + depth] = 0;
    depth_ = 1 + depth;
  }

  // Exiting the most recently entered zone (top of stack).
  void Exit(const uint64_t t_exit, const size_t global_idx,
            const ZoneHandle zone, Accumulators& accumulators) {
    HWY_DASSERT(depth_ > 0);
    const size_t depth = depth_ - 1;
    const size_t zone_idx = zone.ZoneIdx();
    const uint64_t duration = t_exit - t_enter_[depth];
    // Clang output for this is verified not to branch. This is 0 if inclusive,
    // otherwise the child total.
    const uint64_t child_total =
        child_total_[1 + depth] & zone.ChildTotalMask();

    const uint64_t self_duration = ClampedSubtract(
        duration, overheads_.self + overheads_.child + child_total);
    accumulators.Get(global_idx, zone_idx).Add(zone, self_duration);
    // For faster TakeAccumulator() - not all zones are encountered.
    visited_zones_.Set(zone_idx);

    // Adding this nested time to the parent's `child_total` will
    // cause it to be later subtracted from the parent's `self_duration`.
    child_total_[1 + depth - 1] += duration + overheads_.child;

    depth_ = depth;
  }

  // Returns the duration of one enter/exit pair and resets all state. Called
  // via `DetectSelfOverhead`.
  uint64_t GetFirstDurationAndReset(size_t global_idx,
                                    Accumulators& accumulators) {
    HWY_DASSERT(depth_ == 0);

    HWY_DASSERT(visited_zones_.Count() == 1);
    const size_t zone_idx = visited_zones_.First();
    HWY_DASSERT(zone_idx <= 3);
    HWY_DASSERT(visited_zones_.Get(zone_idx));
    visited_zones_.Clear(zone_idx);

    Accumulator& zone = accumulators.Get(global_idx, zone_idx);
    const uint64_t duration = zone.duration;
    zone = Accumulator();
    return duration;
  }

  // Adds all data to `results` and resets it here. Called from the main thread.
  void MoveTo(const size_t global_idx, Accumulators& accumulators,
              Results& results) {
    visited_zones_.Foreach([&](size_t zone_idx) {
      results.TakeAccumulator(global_idx, zone_idx,
                              accumulators.Get(global_idx, zone_idx));
    });
    // OK to reset even if we have active zones, because we set `visited_zones_`
    // when exiting the zone.
    visited_zones_ = ZoneSet();
  }

 private:
  // 40 bytes:
  ZoneSet visited_zones_;  // Which `zones_` have been active on this worker.
  uint64_t depth_ = 0;     // Current nesting level for active zones.
  Overheads overheads_;

  uint64_t t_enter_[kMaxDepth];
  // Used to deduct child duration from parent's self time (unless inclusive).
  // Shifting by one avoids bounds-checks for depth_ = 0 (root zone).
  uint64_t child_total_[1 + kMaxDepth] = {0};
};
// Enables shift rather than multiplication.
static_assert(sizeof(PerWorker) == 256, "Wrong size");

}  // namespace profiler

class Profiler {
 public:
  static HWY_DLLEXPORT Profiler& Get();

  // Returns `global_idx` from thread-local storage (0 for the main thread).
  // Used by `PROFILER_ZONE/PROFILER_FUNC`. It is faster to instead pass the
  // global_idx from `ThreadPool::Run` (if constructed with non-default
  // `PoolWorkerMapping`) to `PROFILER_ZONE2/PROFILER_ZONE3`.
  // DEPRECATED: use `GlobalIdx` instead.
  static size_t Thread() { return s_global_idx; }
  static size_t GlobalIdx() { return s_global_idx; }
  // Must be called from all worker threads, and once also on the main thread,
  // before any use of `PROFILER_ZONE/PROFILER_FUNC`.
  static void SetGlobalIdx(size_t global_idx) { s_global_idx = global_idx; }

  void ReserveWorker(size_t global_idx) {
    HWY_ASSERT(!workers_reserved_.Get(global_idx));
    workers_reserved_.Set(global_idx);
  }

  void FreeWorker(size_t global_idx) {
    HWY_ASSERT(workers_reserved_.Get(global_idx));
    workers_reserved_.Clear(global_idx);
  }

  // Called by `Zone` from any thread.
  void Enter(uint64_t t_enter, size_t global_idx) {
    GetWorker(global_idx).Enter(t_enter);
  }

  // Called by `~Zone` from any thread.
  void Exit(uint64_t t_exit, size_t global_idx, profiler::ZoneHandle zone) {
    GetWorker(global_idx).Exit(t_exit, global_idx, zone, accumulators_);
  }

  uint64_t GetFirstDurationAndReset(size_t global_idx) {
    return GetWorker(global_idx)
        .GetFirstDurationAndReset(global_idx, accumulators_);
  }

  const char* Name(profiler::ZoneHandle zone) const {
    return zones_.Name(zone);
  }

  // Copies `name` into the string table and returns its unique `zone`. Uses
  // linear search, which is fine because this is called during static init.
  // Called via static initializer and the result is passed to the `Zone` ctor.
  profiler::ZoneHandle AddZone(const char* name,
                               ProfilerFlags flags = ProfilerFlags::kDefault) {
    return zones_.AddZone(name, flags);
  }

  void AddFunc(void* owner, ProfilerFunc func) {
    funcs_.Add(reinterpret_cast<intptr_t>(owner), func);
  }
  void RemoveFunc(void* owner) {
    funcs_.Remove(reinterpret_cast<intptr_t>(owner));
  }

  // For reporting average concurrency. Called by `ThreadPool::Run` on the main
  // thread, returns true if this is the first call since the last `EndRootRun`.
  //
  // We want to report the concurrency of each separate 'invocation' of a zone.
  // A unique per-call identifier (could be approximated with the line number
  // and return address) is not sufficient because the caller may in turn be
  // called from differing parallel sections. A per-`ThreadPool::Run` counter
  // also under-reports concurrency because each pool in nested parallelism
  // (over packages and CCXes) would be considered separate invocations.
  //
  // The alternative of detecting overlapping zones via timestamps is not 100%
  // reliable because timers may not be synchronized across sockets or perhaps
  // even cores. "Invariant" x86 TSCs are indeed synchronized across cores, but
  // not across sockets unless the RESET# signal reaches each at the same time.
  // Linux seems to make an effort to correct this, and Arm's "generic timer"
  // broadcasts to "all cores", but there is no universal guarantee.
  //
  // Under the assumption that all concurrency is via our `ThreadPool`, we can
  // record all `global_idx` for each outermost (root) `ThreadPool::Run`. This
  // collapses all nested pools into one 'invocation'. We then compute per-zone
  // concurrency as the number of unique `global_idx` seen per invocation.
  bool IsRootRun() {
    // We are not the root if a Run was already active.
    return !run_active_.test_and_set(std::memory_order_acquire);
  }

  // Must be called if `IsRootRun` returned true. Resets the state so that the
  // next call to `IsRootRun` will again return true. Called from main thread.
  // Note that some zones may still be active. Their concurrency will be updated
  // when `PrintResults` is called.
  void EndRootRun() {
    UpdateResults();
    results_.CountWorkersAndReset();

    run_active_.clear(std::memory_order_release);
  }

  // Prints results. Call from main thread after all threads have exited all
  // zones. Resets all state, can be called again after more zones.
  void PrintResults() {
    UpdateResults();
    // `CountWorkersAndReset` is fused into `Print`, so do not call it here.

    results_.PrintAndReset(zones_);

    funcs_.CallAll();
  }

  // TODO: remove when no longer called.
  void SetMaxThreads(size_t) {}

 private:
  // Sets main thread index, computes self-overhead, and checks timer support.
  Profiler();

  profiler::PerWorker& GetWorker(size_t global_idx) {
    HWY_DASSERT(workers_reserved_.Get(global_idx));
    return workers_[global_idx];
  }

  // Moves accumulators into Results. Called from the main thread.
  void UpdateResults() {
    // Ensure we see all writes from before the workers' release fence.
    std::atomic_thread_fence(std::memory_order_acquire);

    workers_reserved_.Foreach([&](size_t global_idx) {
      workers_[global_idx].MoveTo(global_idx, accumulators_, results_);
    });
  }

  static thread_local size_t s_global_idx;

  // These are atomic because `ThreadFunc` reserves its slot(s) and even
  // `ThreadPool::ThreadPool` may be called concurrently. Both have bit `i` set
  // between calls to `Reserve*(i)` and `Free*(i)`. They are consulted in
  // `UpdateResults` and to validate arguments in debug builds, and only updated
  // in the pool/thread init/shutdown.
  profiler::AtomicWorkerSet workers_reserved_;

  std::atomic_flag run_active_ = ATOMIC_FLAG_INIT;

  profiler::Funcs funcs_;

  // To avoid locking, each worker has its own working set. We could access this
  // through `thread_local` pointers, but that is slow to read on x86. Because
  // our `ThreadPool` anyway passes a `global_idx` argument, we can instead pass
  // that through the `PROFILER_ZONE2/PROFILER_ZONE3` macros.
  profiler::PerWorker workers_[profiler::kMaxWorkers];

  profiler::Accumulators accumulators_;

  profiler::Results results_;

  profiler::Zones zones_;
};

namespace profiler {

// RAII for zone entry/exit.
class Zone {
 public:
  // Thread-compatible; must not call concurrently with the same `global_idx`,
  // which is either:
  // - passed from `ThreadPool::Run` (if it was constructed with non-default
  //  `PoolWorkerMapping`) to `PROFILER_ZONE2/PROFILER_ZONE3`;
  // - obtained from `Profiler::GlobalIdx()`; or
  // - 0 if running on the main thread.
  Zone(Profiler& profiler, size_t global_idx, ZoneHandle zone)
      : profiler_(profiler) {
    HWY_FENCE;
    const uint64_t t_enter = timer::Start();
    HWY_FENCE;
    global_idx_ = static_cast<uint32_t>(global_idx);
    zone_ = zone;
    profiler.Enter(t_enter, global_idx);
    HWY_FENCE;
  }

  ~Zone() {
    HWY_FENCE;
    const uint64_t t_exit = timer::Stop();
    profiler_.Exit(t_exit, static_cast<size_t>(global_idx_), zone_);
    HWY_FENCE;
  }

 private:
  Profiler& profiler_;
  uint32_t global_idx_;
  ZoneHandle zone_;
};

}  // namespace profiler
#else   // profiler disabled: stub implementation

namespace profiler {
struct ZoneHandle {};
}  // namespace profiler

struct Profiler {
  static HWY_DLLEXPORT Profiler& Get();

  // DEPRECATED: use `GlobalIdx` instead.
  static size_t Thread() { return 0; }
  static size_t GlobalIdx() { return 0; }
  static void SetGlobalIdx(size_t) {}
  void ReserveWorker(size_t) {}
  void FreeWorker(size_t) {}
  void Enter(uint64_t, size_t) {}
  void Exit(uint64_t, size_t, profiler::ZoneHandle) {}
  uint64_t GetFirstDurationAndReset(size_t) { return 0; }

  const char* Name(profiler::ZoneHandle) const { return nullptr; }
  profiler::ZoneHandle AddZone(const char*,
                               ProfilerFlags = ProfilerFlags::kDefault) {
    return profiler::ZoneHandle();
  }

  void AddFunc(void*, ProfilerFunc) {}
  void RemoveFunc(void*) {}

  bool IsRootRun() { return false; }
  void EndRootRun() {}
  void PrintResults() {}

  // TODO: remove when no longer called.
  void SetMaxThreads(size_t) {}
};

namespace profiler {
struct Zone {
  Zone(Profiler&, size_t, ZoneHandle) {}
};

}  // namespace profiler
#endif  // PROFILER_ENABLED || HWY_IDE

}  // namespace hwy

// Creates a `Zone` lvalue with a line-dependent name, which records the elapsed
// time from here until the end of the current scope. `p` is from
// `Profiler::Get()` or a cached reference. `global_idx < kMaxWorkers`. `zone`
// is the return value of `AddZone`. Separating its static init from the `Zone`
// may be more efficient than `PROFILER_ZONE2`.
#define PROFILER_ZONE3(p, global_idx, zone)                               \
  HWY_FENCE;                                                              \
  const hwy::profiler::Zone HWY_CONCAT(Z, __LINE__)(p, global_idx, zone); \
  HWY_FENCE

// For compatibility with old callers that do not pass `p` nor `flags`.
// Also calls AddZone. Usage: `PROFILER_ZONE2(global_idx, "MyZone");`
#define PROFILER_ZONE2(global_idx, name)                              \
  static const hwy::profiler::ZoneHandle HWY_CONCAT(zone, __LINE__) = \
      hwy::Profiler::Get().AddZone(name);                             \
  PROFILER_ZONE3(hwy::Profiler::Get(), global_idx, HWY_CONCAT(zone, __LINE__))
#define PROFILER_FUNC2(global_idx) PROFILER_ZONE2(global_idx, __func__)

// OBSOLETE: it is more efficient to pass `global_idx` from `ThreadPool` to
// `PROFILER_ZONE2/PROFILER_ZONE3`. Here we get it from thread_local storage.
#define PROFILER_ZONE(name) PROFILER_ZONE2(hwy::Profiler::GlobalIdx(), name)
#define PROFILER_FUNC PROFILER_FUNC2(hwy::Profiler::GlobalIdx())

// DEPRECATED: Use `hwy::Profiler::Get()` directly instead.
#define PROFILER_ADD_ZONE(name) hwy::Profiler::Get().AddZone(name)
#define PROFILER_IS_ROOT_RUN() hwy::Profiler::Get().IsRootRun()
#define PROFILER_END_ROOT_RUN() hwy::Profiler::Get().EndRootRun()
#define PROFILER_PRINT_RESULTS() hwy::Profiler::Get().PrintResults()

#endif  // HIGHWAY_HWY_PROFILER_H_

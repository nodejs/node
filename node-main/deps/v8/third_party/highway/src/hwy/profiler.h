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

#include "hwy/highway_export.h"

// High precision, low overhead time measurements. Returns exact call counts and
// total elapsed time for user-defined 'zones' (code regions, i.e. C++ scopes).
//
// Uses RAII to capture begin/end timestamps, with user-specified zone names:
//   { PROFILER_ZONE("name"); /*code*/ } or
// the name of the current function:
//   void FuncToMeasure() { PROFILER_FUNC; /*code*/ }.
// You can reduce the overhead by passing a thread ID:
//   `PROFILER_ZONE2(thread, name)`. The new and preferred API also allows
// passing flags, such as requesting inclusive time:
// `static const auto zone = profiler.AddZone("name", flags);` and then
// `PROFILER_ZONE3(profiler, thread, zone)`.
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
#include <string.h>  // strcmp, strlen

#include <algorithm>  // std::sort
#include <atomic>
#include <vector>

#include "hwy/aligned_allocator.h"
#include "hwy/base.h"
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

#if PROFILER_ENABLED

// Implementation details.
namespace profiler {

HWY_INLINE_VAR constexpr size_t kNumFlags = 1;

// Upper bounds for fixed-size data structures, guarded via HWY_DASSERT:

// Maximum nesting of zones, chosen such that PerThread is 256 bytes.
HWY_INLINE_VAR constexpr size_t kMaxDepth = 13;
// Reports with more than ~50 are anyway difficult to read.
HWY_INLINE_VAR constexpr size_t kMaxZones = 128;
// Upper bound on threads that call `InitThread`, and `thread` arguments. Note
// that fiber libraries can spawn hundreds of threads. Enough for Turin cores.
HWY_INLINE_VAR constexpr size_t kMaxThreads = 256;

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
  static constexpr std::memory_order kRel = std::memory_order_relaxed;

 public:
  // Returns a copy of the `name` passed to `AddZone` that returned the
  // given `zone`.
  const char* Name(ZoneHandle zone) const { return ptrs_[zone.ZoneIdx()]; }

  ZoneHandle AddZone(const char* name, ProfilerFlags flags) {
    // Linear search whether it already exists.
    const size_t num_zones = next_ptr_.load(kRel);
    HWY_ASSERT(num_zones < kMaxZones);
    for (size_t zone_idx = 1; zone_idx < num_zones; ++zone_idx) {
      if (!strcmp(ptrs_[zone_idx], name)) {
        return ZoneHandle(zone_idx, flags);
      }
    }

    // Reserve the next `zone_idx` (index in `ptrs_`).
    const size_t zone_idx = next_ptr_.fetch_add(1, kRel);

    // Copy into `name` into `chars_`.
    const size_t len = strlen(name) + 1;
    const size_t pos = next_char_.fetch_add(len, kRel);
    HWY_ASSERT(pos + len <= sizeof(chars_));
    strcpy(chars_ + pos, name);  // NOLINT

    ptrs_[zone_idx] = chars_ + pos;
    const ZoneHandle zone(zone_idx, flags);
    HWY_DASSERT(!strcmp(Name(zone), name));
    return zone;
  }

 private:
  const char* ptrs_[kMaxZones];
  std::atomic<size_t> next_ptr_{1};  // next zone_idx
  char chars_[kMaxZones * 70];
  std::atomic<size_t> next_char_{0};
};

// Holds total duration and number of calls. Thread is implicit in the index of
// this class within the `Accumulators` array.
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

  void Assimilate(Accumulator& other) {
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

// Modified from `hwy::BitSet4096`. Avoids the second-level `BitSet64`, because
// we only need `kMaxZones` = 128.
class ZoneSet {
 public:
  // No harm if `i` is already set.
  void Set(size_t i) {
    HWY_DASSERT(i < kMaxZones);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    bits_[idx].Set(mod);
    HWY_DASSERT(Get(i));
  }

  void Clear(size_t i) {
    HWY_DASSERT(i < kMaxZones);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    bits_[idx].Clear(mod);
    HWY_DASSERT(!Get(i));
  }

  bool Get(size_t i) const {
    HWY_DASSERT(i < kMaxZones);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    return bits_[idx].Get(mod);
  }

  // Returns lowest i such that Get(i). Caller must ensure Any() beforehand!
  size_t First() const {
    HWY_DASSERT(bits_[0].Any() || bits_[1].Any());
    const size_t idx = bits_[0].Any() ? 0 : 1;
    return idx * 64 + bits_[idx].First();
  }

  // Calls `func(i)` for each `i` in the set. It is safe for `func` to modify
  // the set, but the current Foreach call is only affected if changing one of
  // the not yet visited BitSet64 for which Any() is true.
  template <class Func>
  void Foreach(const Func& func) const {
    bits_[0].Foreach([&func](size_t mod) { func(mod); });
    bits_[1].Foreach([&func](size_t mod) { func(64 + mod); });
  }

  size_t Count() const { return bits_[0].Count() + bits_[1].Count(); }

 private:
  static_assert(kMaxZones == 128, "Update ZoneSet");
  BitSet64 bits_[2];
};

// Modified from `ZoneSet`.
class ThreadSet {
 public:
  // No harm if `i` is already set.
  void Set(size_t i) {
    HWY_DASSERT(i < kMaxThreads);
    const size_t idx = i / 64;
    const size_t mod = i % 64;
    bits_[idx].Set(mod);
  }

  size_t Count() const {
    size_t total = 0;
    for (const BitSet64& bits : bits_) {
      total += bits.Count();
    }
    return total;
  }

 private:
  BitSet64 bits_[DivCeil(kMaxThreads, size_t{64})];
};

// Durations are per-CPU, but end to end performance is defined by wall time.
// Assuming fork-join parallelism, zones are entered by multiple threads
// concurrently, which means the total number of unique threads is also the
// degree of concurrency, so we can estimate wall time as CPU time divided by
// the number of unique threads seen. This is facilitated by unique thread IDs
// passed in by callers, or taken from thread-local storage.
//
// We also want to support varying thread counts per call site, because the same
// function/zone may be called from multiple pools. `EndRootRun` calls
// `CountThreadsAndReset` after each top-level `ThreadPool::Run`, which
// generates one data point summarized via descriptive statistics. Here we
// implement a simpler version of `hwy::Stats` because we do not require
// geomean/variance/kurtosis/skewness. Because concurrency is a small integer,
// we can simply compute sums rather than online moments. There is also only one
// instance across all threads, hence we do not require `Assimilate`.
//
// Note that subsequently discovered prior work estimates the number of active
// and idle processors by updating atomic counters whenever they start/finish a
// task: https://homes.cs.washington.edu/~tom/pubs/quartz.pdf and "Effective
// performance measurement and analysis of multithreaded applications". We
// instead accumulate zone durations into per-thread storage.
// `CountThreadsAndReset` then checks how many were nonzero, which avoids
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

// Holds the final results across all threads, including `ConcurrencyStats`.
// There is only one instance because this is updated by the main thread.
class Results {
 public:
  void Assimilate(const size_t thread, const size_t zone_idx,
                  Accumulator& other) {
    HWY_DASSERT(thread < kMaxThreads);
    HWY_DASSERT(zone_idx < kMaxZones);
    HWY_DASSERT(other.zone.ZoneIdx() == zone_idx);

    visited_zones_.Set(zone_idx);
    totals_[zone_idx].Assimilate(other);
    threads_[zone_idx].Set(thread);
  }

  // Moves the total number of threads seen during the preceding root-level
  // `ThreadPool::Run` into one data point for `ConcurrencyStats`.
  void CountThreadsAndReset(const size_t zone_idx) {
    HWY_DASSERT(zone_idx < kMaxZones);
    const size_t num_threads = threads_[zone_idx].Count();
    // Although threads_[zone_idx] at one point was non-empty, it is reset
    // below, and so can be empty on the second call to this via `PrintResults`,
    // after one from `EndRootRun`. Do not add a data point if empty.
    if (num_threads != 0) {
      concurrency_[zone_idx].Notify(num_threads);
    }
    threads_[zone_idx] = ThreadSet();
  }

  void CountThreadsAndReset() {
    visited_zones_.Foreach(
        [&](size_t zone_idx) { CountThreadsAndReset(zone_idx); });
  }

  void Print(const Zones& zones) {
    const double inv_freq = 1.0 / platform::InvariantTicksPerSecond();

    // Sort by decreasing total (self) cost. `totals_` are sparse, so sort an
    // index vector instead.
    std::vector<uint32_t> indices;
    indices.reserve(visited_zones_.Count());
    visited_zones_.Foreach([&](size_t zone_idx) {
      indices.push_back(static_cast<uint32_t>(zone_idx));
      // In case the zone exited after `EndRootRun` and was not yet added.
      CountThreadsAndReset(zone_idx);
    });
    std::sort(indices.begin(), indices.end(), [&](uint32_t a, uint32_t b) {
      return totals_[a].duration > totals_[b].duration;
    });

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
      // `threads_` was already reset by `CountThreadsAndReset`.
    }
    visited_zones_ = ZoneSet();
  }

 private:
  // Indicates which of the array entries are in use.
  ZoneSet visited_zones_;
  Accumulator totals_[kMaxZones];
  ThreadSet threads_[kMaxZones];
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
  // cache line. Hence interleave 8 zones per thread.
  static constexpr size_t kPerLine = HWY_ALIGNMENT / sizeof(Accumulator);

 public:
  Accumulator& Get(const size_t thread, const size_t zone_idx) {
    HWY_DASSERT(thread < kMaxThreads);
    HWY_DASSERT(zone_idx < kMaxZones);
    const size_t line = zone_idx / kPerLine;
    const size_t offset = zone_idx % kPerLine;
    return zones_[(line * kMaxThreads + thread) * kPerLine + offset];
  }

 private:
  Accumulator zones_[kMaxZones * kMaxThreads];
};

// Reacts to zone enter/exit events. Builds a stack of active zones and
// accumulates self/child duration for each.
class PerThread {
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
  void Exit(const uint64_t t_exit, const size_t thread, const ZoneHandle zone,
            Accumulators& accumulators) {
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
    accumulators.Get(thread, zone_idx).Add(zone, self_duration);
    // For faster Assimilate() - not all zones are encountered.
    visited_zones_.Set(zone_idx);

    // Adding this nested time to the parent's `child_total` will
    // cause it to be later subtracted from the parent's `self_duration`.
    child_total_[1 + depth - 1] += duration + overheads_.child;

    depth_ = depth;
  }

  // Returns the duration of one enter/exit pair and resets all state. Called
  // via `DetectSelfOverhead`.
  uint64_t GetFirstDurationAndReset(size_t thread, Accumulators& accumulators) {
    HWY_DASSERT(depth_ == 0);

    HWY_DASSERT(visited_zones_.Count() == 1);
    const size_t zone_idx = visited_zones_.First();
    HWY_DASSERT(zone_idx <= 3);
    HWY_DASSERT(visited_zones_.Get(zone_idx));
    visited_zones_.Clear(zone_idx);

    Accumulator& zone = accumulators.Get(thread, zone_idx);
    const uint64_t duration = zone.duration;
    zone = Accumulator();
    return duration;
  }

  // Adds all data to `results` and resets it here. Called from the main thread.
  void MoveTo(const size_t thread, Accumulators& accumulators,
              Results& results) {
    visited_zones_.Foreach([&](size_t zone_idx) {
      results.Assimilate(thread, zone_idx, accumulators.Get(thread, zone_idx));
    });
    // OK to reset even if we have active zones, because we set `visited_zones_`
    // when exiting the zone.
    visited_zones_ = ZoneSet();
  }

 private:
  // 40 bytes:
  ZoneSet visited_zones_;  // Which `zones_` have been active on this thread.
  uint64_t depth_ = 0;     // Current nesting level for active zones.
  Overheads overheads_;

  uint64_t t_enter_[kMaxDepth];
  // Used to deduct child duration from parent's self time (unless inclusive).
  // Shifting by one avoids bounds-checks for depth_ = 0 (root zone).
  uint64_t child_total_[1 + kMaxDepth] = {0};
};
// Enables shift rather than multiplication.
static_assert(sizeof(PerThread) == 256, "Wrong size");

}  // namespace profiler

class Profiler {
 public:
  static HWY_DLLEXPORT Profiler& Get();

  // Assigns the next counter value to the `thread_local` that `Thread` reads.
  // Must be called exactly once on each thread before any `PROFILER_ZONE`
  // (without a thread argument) are re-entered by multiple threads.
  // `Profiler()` takes care of calling this for the main thread. It is fine not
  // to call it for other threads as long as they only use `PROFILER_ZONE2` or
  // `PROFILER_ZONE3`, which take a thread argument and do not call `Thread`.
  static void InitThread() { s_thread = s_num_threads.fetch_add(1); }

  // Used by `PROFILER_ZONE/PROFILER_FUNC` to read the `thread` argument from
  // thread_local storage. It is faster to instead pass the ThreadPool `thread`
  // argument to `PROFILER_ZONE2/PROFILER_ZONE3`. Note that the main thread
  // calls `InitThread` first, hence its `Thread` returns zero, which matches
  // the main-first worker numbering used by `ThreadPool`.
  static size_t Thread() { return s_thread; }

  // Speeds up `UpdateResults` by providing an upper bound on the number of
  // threads tighter than `profiler::kMaxThreads`. It is not required to be
  // tight, and threads less than this can still be unused.
  void SetMaxThreads(size_t max_threads) {
    HWY_ASSERT(max_threads <= profiler::kMaxThreads);
    max_threads_ = max_threads;
  }
  size_t MaxThreads() const { return max_threads_; }

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
  // record all `thread` for each outermost (root) `ThreadPool::Run`. This
  // collapses all nested pools into one 'invocation'. We then compute per-zone
  // concurrency as the number of unique `thread` seen per invocation.
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
    results_.CountThreadsAndReset();

    run_active_.clear(std::memory_order_release);
  }

  // Prints results. Call from main thread after all threads have exited all
  // zones. Resets all state, can be called again after more zones.
  void PrintResults() {
    UpdateResults();
    // `CountThreadsAndReset` is fused into `Print`, so do not call it here.

    results_.Print(zones_);
  }

  // Only for use by Zone; called from any thread.
  profiler::PerThread& GetThread(size_t thread) {
    HWY_DASSERT(thread < max_threads_);
    return threads_[thread];
  }

  profiler::Accumulators& Accumulators() { return accumulators_; }

 private:
  // Sets main thread index, computes self-overhead, and checks timer support.
  Profiler();

  // Moves accumulators into Results. Called from the main thread.
  void UpdateResults() {
    for (size_t thread = 0; thread < max_threads_; ++thread) {
      threads_[thread].MoveTo(thread, accumulators_, results_);
    }
  }

  static thread_local size_t s_thread;
  static std::atomic<size_t> s_num_threads;
  size_t max_threads_ = profiler::kMaxThreads;

  std::atomic_flag run_active_ = ATOMIC_FLAG_INIT;

  // To avoid locking, each thread has its own working set. We could access this
  // through `thread_local` pointers, but that is slow to read on x86. Because
  // our `ThreadPool` anyway passes a `thread` argument, we can instead pass
  // that through the `PROFILER_ZONE2/PROFILER_ZONE3` macros.
  profiler::PerThread threads_[profiler::kMaxThreads];

  profiler::Accumulators accumulators_;

  // Updated by the main thread after the root `ThreadPool::Run` and during
  // `PrintResults`.
  profiler::ConcurrencyStats concurrency_[profiler::kMaxZones];

  profiler::Results results_;

  profiler::Zones zones_;
};

namespace profiler {

// RAII for zone entry/exit.
class Zone {
 public:
  // Thread-compatible; must not be called concurrently with the same `thread`.
  // `thread` must be < `HWY_MIN(kMaxThreads, max_threads_)`, and is typically:
  // - passed from `ThreadPool` via `PROFILER_ZONE2/PROFILER_ZONE3`. NOTE:
  //   this value must be unique across all pools, which requires an offset to
  //   a nested pool's `thread` argument.
  // - obtained from `Profiler::Thread()`, or
  // - 0 if only a single thread is active.
  Zone(Profiler& profiler, size_t thread, ZoneHandle zone)
      : profiler_(profiler) {
    HWY_FENCE;
    const uint64_t t_enter = timer::Start();
    HWY_FENCE;
    thread_ = static_cast<uint32_t>(thread);
    zone_ = zone;
    profiler.GetThread(thread).Enter(t_enter);
    HWY_FENCE;
  }

  ~Zone() {
    HWY_FENCE;
    const uint64_t t_exit = timer::Stop();
    profiler_.GetThread(thread_).Exit(t_exit, thread_, zone_,
                                      profiler_.Accumulators());
    HWY_FENCE;
  }

 private:
  Profiler& profiler_;
  uint32_t thread_;
  ZoneHandle zone_;
};

}  // namespace profiler
#else   // profiler disabled: stub implementation

namespace profiler {
struct ZoneHandle {};
}  // namespace profiler

struct Profiler {
  static HWY_DLLEXPORT Profiler& Get();

  static void InitThread() {}
  static size_t Thread() { return 0; }
  void SetMaxThreads(size_t) {}

  const char* Name(profiler::ZoneHandle) const { return nullptr; }
  profiler::ZoneHandle AddZone(const char*,
                               ProfilerFlags = ProfilerFlags::kDefault) {
    return profiler::ZoneHandle();
  }

  bool IsRootRun() { return false; }
  void EndRootRun() {}

  void PrintResults() {}
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
// `Profiler::Get()` or a cached reference. `thread` is < `kMaxThreads`. `zone`
// is the return value of `AddZone`. Separating its static init from the `Zone`
// may be more efficient than `PROFILER_ZONE2`.
#define PROFILER_ZONE3(p, thread, zone)                               \
  HWY_FENCE;                                                          \
  const hwy::profiler::Zone HWY_CONCAT(Z, __LINE__)(p, thread, zone); \
  HWY_FENCE

// For compatibility with old callers that do not pass `p` nor `flags`.
// Also calls AddZone. Usage: `PROFILER_ZONE2(thread, "MyZone");`
#define PROFILER_ZONE2(thread, name)                                  \
  static const hwy::profiler::ZoneHandle HWY_CONCAT(zone, __LINE__) = \
      hwy::Profiler::Get().AddZone(name);                             \
  PROFILER_ZONE3(hwy::Profiler::Get(), thread, HWY_CONCAT(zone, __LINE__))
#define PROFILER_FUNC2(thread) PROFILER_ZONE2(thread, __func__)

// OBSOLETE: it is more efficient to pass `thread` from `ThreadPool` to
// `PROFILER_ZONE2/PROFILER_ZONE3`. Here we get it from thread_local storage.
#define PROFILER_ZONE(name) PROFILER_ZONE2(hwy::Profiler::Thread(), name)
#define PROFILER_FUNC PROFILER_FUNC2(hwy::Profiler::Thread())

// DEPRECATED: Use `hwy::Profiler::Get()` directly instead.
#define PROFILER_ADD_ZONE(name) hwy::Profiler::Get().AddZone(name)
#define PROFILER_IS_ROOT_RUN() hwy::Profiler::Get().IsRootRun()
#define PROFILER_END_ROOT_RUN() hwy::Profiler::Get().EndRootRun()
#define PROFILER_PRINT_RESULTS() hwy::Profiler::Get().PrintResults()

#endif  // HIGHWAY_HWY_PROFILER_H_

// Copyright 2024 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HIGHWAY_HWY_PERF_COUNTERS_H_
#define HIGHWAY_HWY_PERF_COUNTERS_H_

// Reads OS/CPU performance counters.

#include <stddef.h>

#include "hwy/base.h"  // HWY_ABORT
#include "hwy/bit_set.h"

namespace hwy {
namespace platform {

// Avoid padding in case callers such as profiler.h store many instances.
#pragma pack(push, 1)
// Provides access to CPU/OS performance counters. Each instance has space for
// multiple counter values; which counters these are may change in future.
// Although counters are per-CPU, Linux accesses them via a syscall, hence we
// use the monostate pattern to avoid callers having to pass around a pointer.
// Note that this is not thread-safe, so the static member functions should only
// be called from the main thread.
class PerfCounters {
 public:
  // Chosen such that this class occupies one or two cache lines.
  static constexpr size_t kCapacity = 14;

  // Bit indices used to identify counters. The ordering is arbitrary. Some of
  // these counters may be 'removed' in the sense of not being visited by
  // `Foreach`, but their enumerators will remain. New counters may be appended.
  enum Counter {
    kRefCycles = 0,
    kInstructions,
    kBranches,
    kBranchMispredicts,
    kBusCycles,
    kCacheRefs,
    kCacheMisses,
    kL3Loads,
    kL3Stores,
    kPageFaults,  // SW
    kMigrations   // SW
  };  // BitSet64 requires these values to be less than 64.

  // Strings for user-facing messages, not used in the implementation.
  static inline const char* Name(Counter c) {
    switch (c) {
      case kRefCycles:
        return "ref_cycles";
      case kInstructions:
        return "instructions";
      case kBranches:
        return "branches";
      case kBranchMispredicts:
        return "branch_mispredicts";
      case kBusCycles:
        return "bus_cycles";
      case kCacheRefs:
        return "cache_refs";
      case kCacheMisses:
        return "cache_misses";
      case kL3Loads:
        return "l3_load";
      case kL3Stores:
        return "l3_store";
      case kPageFaults:
        return "page_fault";
      case kMigrations:
        return "migration";
      default:
        HWY_UNREACHABLE;
    }
  }

  // Returns false if counters are unavailable. Must be called at least once
  // before `StartAll`; it is separate to reduce the overhead of repeatedly
  // stopping/starting counters.
  HWY_DLLEXPORT static bool Init();

  // Returns false if counters are unavailable, otherwise starts them. Note that
  // they default to stopped. Unless this is called, the values read may be 0.
  HWY_DLLEXPORT static bool StartAll();

  // Stops and zeros all counters. This is not necessary if users subtract the
  // previous counter values, but can increase precision because floating-point
  // has more precision near zero.
  HWY_DLLEXPORT static void StopAllAndReset();

  // Reads the current (extrapolated, in case of multiplexing) counter values.
  HWY_DLLEXPORT PerfCounters();

  // Returns whether any counters were successfully read.
  bool AnyValid() const { return valid_.Any(); }

  // Returns whether the given counter was successfully read.
  bool IsValid(Counter c) const {
    const size_t bit_idx = static_cast<size_t>(c);
    return valid_.Get(bit_idx);
  }

  // Returns the maximum extrapolation factor for any counter, which is the
  // total time between `StartAll` and now or the last `StopAllAndReset`,
  // divided by the time that the counter was actually running. This
  // approximates the number of counter groups that the CPU multiplexes onto the
  // actual counter hardware. It is only meaningful if AnyValid().
  double MaxExtrapolate() const { return max_extrapolate_; }

  // Returns the value of the given counter, or zero if it is not valid.
  double Get(Counter c) const {
    return IsValid(c) ? values_[IndexForCounter(c)] : 0.0;
  }

  // For each valid counter in increasing numerical order, calls `visitor` with
  // the value and `Counter`.
  template <class Visitor>
  void Foreach(const Visitor& visitor) {
    valid_.Foreach([&](size_t bit_idx) {
      const Counter c = static_cast<Counter>(bit_idx);
      visitor(values_[IndexForCounter(c)], c);
    });
  }

 private:
  // Index within `values_` for a given counter.
  HWY_DLLEXPORT static size_t IndexForCounter(Counter c);

  BitSet64 valid_;
  double max_extrapolate_;
  // Floating-point because these are extrapolated (multiplexing). It would be
  // nice for this to fit in one cache line to reduce the cost of reading
  // counters in profiler.h, but some of the values are too large for float and
  // we want more than 8 counters. Ensure all values are sums, not ratios, so
  // that profiler.h can add/subtract them. These are contiguous in memory, in
  // the order that counters were initialized.
  double values_[kCapacity];
};
#pragma pack(pop)

}  // namespace platform
}  // namespace hwy

#endif  // HIGHWAY_HWY_PERF_COUNTERS_H_

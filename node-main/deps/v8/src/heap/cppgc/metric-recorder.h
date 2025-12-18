// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_METRIC_RECORDER_H_
#define V8_HEAP_CPPGC_METRIC_RECORDER_H_

#include <cstdint>

namespace cppgc {
namespace internal {

class StatsCollector;

/**
 * Base class used for reporting GC statistics histograms. Embedders interested
 * in collecting histograms should implement the virtual AddMainThreadEvent
 * methods below and pass an instance of the implementation during Heap
 * creation.
 */
class MetricRecorder {
 public:
  struct GCCycle {
    enum class Type { kMinor, kMajor };
    struct IncrementalPhases {
      int64_t mark_duration_us = -1;
      int64_t sweep_duration_us = -1;
    };
    struct Phases : public IncrementalPhases {
      int64_t weak_duration_us = -1;
      int64_t compact_duration_us = -1;
    };
    struct Sizes {
      int64_t before_bytes = -1;
      int64_t after_bytes = -1;
      int64_t freed_bytes = -1;
    };

    Type type = Type::kMajor;
    Phases total;
    Phases main_thread;
    Phases main_thread_atomic;
    IncrementalPhases main_thread_incremental;
    Sizes objects;
    Sizes memory;
    double collection_rate_in_percent;
    double efficiency_in_bytes_per_us;
    double main_thread_efficiency_in_bytes_per_us;
  };

  struct MainThreadIncrementalMark {
    int64_t duration_us = -1;
  };

  struct MainThreadIncrementalSweep {
    int64_t duration_us = -1;
  };

  virtual ~MetricRecorder() = default;

  virtual void AddMainThreadEvent(const GCCycle& event) {}
  virtual void AddMainThreadEvent(const MainThreadIncrementalMark& event) {}
  virtual void AddMainThreadEvent(const MainThreadIncrementalSweep& event) {}
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_METRIC_RECORDER_H_

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_INCREMENTAL_MARKING_SCHEDULE_H_
#define V8_HEAP_CPPGC_INCREMENTAL_MARKING_SCHEDULE_H_

#include <atomic>

#include "src/base/platform/time.h"

namespace cppgc {
namespace internal {

class V8_EXPORT_PRIVATE IncrementalMarkingSchedule {
 public:
  // Estimated duration of GC cycle in milliseconds.
  static const double kEstimatedMarkingTimeMs;

  // Minimum number of bytes that should be marked during an incremental
  // marking step.
  static const size_t kMinimumMarkedBytesPerIncrementalStep;

  void NotifyIncrementalMarkingStart();

  void UpdateIncrementalMarkedBytes(size_t);
  void AddConcurrentlyMarkedBytes(size_t);

  size_t GetOverallMarkedBytes() const;
  size_t GetConcurrentlyMarkedBytes() const;

  size_t GetNextIncrementalStepDuration(size_t);

  void SetElapsedTimeForTesting(double elapsed_time) {
    elapsed_time_for_testing_ = elapsed_time;
  }

  bool ShouldFlushEphemeronPairs();

 private:
  double GetElapsedTimeInMs(v8::base::TimeTicks);

  v8::base::TimeTicks incremental_marking_start_time_;

  size_t incrementally_marked_bytes_ = 0;
  std::atomic_size_t concurrently_marked_bytes_{0};

  // Using -1 as sentinel to denote
  static constexpr double kNoSetElapsedTimeForTesting = -1;
  double elapsed_time_for_testing_ = kNoSetElapsedTimeForTesting;

  static constexpr size_t kInvalidLastEstimatedLiveBytes = -1;
  size_t last_estimated_live_bytes_ = kInvalidLastEstimatedLiveBytes;
  double ephemeron_pairs_flushing_ratio_target = 0.25;
  static constexpr double kEphemeronPairsFlushingRatioIncrements = 0.25;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_INCREMENTAL_MARKING_SCHEDULE_H_

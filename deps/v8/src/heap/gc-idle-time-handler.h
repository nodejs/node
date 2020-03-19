// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_GC_IDLE_TIME_HANDLER_H_
#define V8_HEAP_GC_IDLE_TIME_HANDLER_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

enum class GCIdleTimeAction : uint8_t {
  kDone,
  kIncrementalStep,
  kFullGC,
};

class GCIdleTimeHeapState {
 public:
  void Print();

  int contexts_disposed;
  double contexts_disposal_rate;
  size_t size_of_objects;
  bool incremental_marking_stopped;
};


// The idle time handler makes decisions about which garbage collection
// operations are executing during IdleNotification.
class V8_EXPORT_PRIVATE GCIdleTimeHandler {
 public:
  // If we haven't recorded any incremental marking events yet, we carefully
  // mark with a conservative lower bound for the marking speed.
  static const size_t kInitialConservativeMarkingSpeed = 100 * KB;

  // Maximum marking step size returned by EstimateMarkingStepSize.
  static const size_t kMaximumMarkingStepSize = 700 * MB;

  // We have to make sure that we finish the IdleNotification before
  // idle_time_in_ms. Hence, we conservatively prune our workload estimate.
  static const double kConservativeTimeRatio;

  // This is the maximum scheduled idle time. Note that it can be more than
  // 16.66 ms when there is currently no rendering going on.
  static const size_t kMaxScheduledIdleTime = 50;

  static const size_t kMaxHeapSizeForContextDisposalMarkCompact = 100 * MB;

  // If contexts are disposed at a higher rate a full gc is triggered.
  static const double kHighContextDisposalRate;

  GCIdleTimeHandler() = default;

  GCIdleTimeAction Compute(double idle_time_in_ms,
                           GCIdleTimeHeapState heap_state);

  bool Enabled();

  static size_t EstimateMarkingStepSize(double idle_time_in_ms,
                                        double marking_speed_in_bytes_per_ms);

  static double EstimateFinalIncrementalMarkCompactTime(
      size_t size_of_objects, double mark_compact_speed_in_bytes_per_ms);

  static bool ShouldDoContextDisposalMarkCompact(int context_disposed,
                                                 double contexts_disposal_rate,
                                                 size_t size_of_objects);

 private:
  DISALLOW_COPY_AND_ASSIGN(GCIdleTimeHandler);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_GC_IDLE_TIME_HANDLER_H_

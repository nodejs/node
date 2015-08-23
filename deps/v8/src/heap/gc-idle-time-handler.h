// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_GC_IDLE_TIME_HANDLER_H_
#define V8_HEAP_GC_IDLE_TIME_HANDLER_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

enum GCIdleTimeActionType {
  DONE,
  DO_NOTHING,
  DO_INCREMENTAL_MARKING,
  DO_SCAVENGE,
  DO_FULL_GC,
  DO_FINALIZE_SWEEPING
};


class GCIdleTimeAction {
 public:
  static GCIdleTimeAction Done() {
    GCIdleTimeAction result;
    result.type = DONE;
    result.parameter = 0;
    result.additional_work = false;
    return result;
  }

  static GCIdleTimeAction Nothing() {
    GCIdleTimeAction result;
    result.type = DO_NOTHING;
    result.parameter = 0;
    result.additional_work = false;
    return result;
  }

  static GCIdleTimeAction IncrementalMarking(intptr_t step_size) {
    GCIdleTimeAction result;
    result.type = DO_INCREMENTAL_MARKING;
    result.parameter = step_size;
    result.additional_work = false;
    return result;
  }

  static GCIdleTimeAction Scavenge() {
    GCIdleTimeAction result;
    result.type = DO_SCAVENGE;
    result.parameter = 0;
    result.additional_work = false;
    return result;
  }

  static GCIdleTimeAction FullGC() {
    GCIdleTimeAction result;
    result.type = DO_FULL_GC;
    result.parameter = 0;
    result.additional_work = false;
    return result;
  }

  static GCIdleTimeAction FinalizeSweeping() {
    GCIdleTimeAction result;
    result.type = DO_FINALIZE_SWEEPING;
    result.parameter = 0;
    result.additional_work = false;
    return result;
  }

  void Print();

  GCIdleTimeActionType type;
  intptr_t parameter;
  bool additional_work;
};


class GCTracer;

// The idle time handler makes decisions about which garbage collection
// operations are executing during IdleNotification.
class GCIdleTimeHandler {
 public:
  // If we haven't recorded any incremental marking events yet, we carefully
  // mark with a conservative lower bound for the marking speed.
  static const size_t kInitialConservativeMarkingSpeed = 100 * KB;

  // Maximum marking step size returned by EstimateMarkingStepSize.
  static const size_t kMaximumMarkingStepSize = 700 * MB;

  // We have to make sure that we finish the IdleNotification before
  // idle_time_in_ms. Hence, we conservatively prune our workload estimate.
  static const double kConservativeTimeRatio;

  // If we haven't recorded any mark-compact events yet, we use
  // conservative lower bound for the mark-compact speed.
  static const size_t kInitialConservativeMarkCompactSpeed = 2 * MB;

  // If we haven't recorded any final incremental mark-compact events yet, we
  // use conservative lower bound for the mark-compact speed.
  static const size_t kInitialConservativeFinalIncrementalMarkCompactSpeed =
      2 * MB;

  // Maximum mark-compact time returned by EstimateMarkCompactTime.
  static const size_t kMaxMarkCompactTimeInMs;

  // Maximum final incremental mark-compact time returned by
  // EstimateFinalIncrementalMarkCompactTime.
  static const size_t kMaxFinalIncrementalMarkCompactTimeInMs;

  // This is the maximum scheduled idle time. Note that it can be more than
  // 16.66 ms when there is currently no rendering going on.
  static const size_t kMaxScheduledIdleTime = 50;

  // The maximum idle time when frames are rendered is 16.66ms.
  static const size_t kMaxFrameRenderingIdleTime = 17;

  static const int kMinBackgroundIdleTime = 900;

  // We conservatively assume that in the next kTimeUntilNextIdleEvent ms
  // no idle notification happens.
  static const size_t kTimeUntilNextIdleEvent = 100;

  // If we haven't recorded any scavenger events yet, we use a conservative
  // lower bound for the scavenger speed.
  static const size_t kInitialConservativeScavengeSpeed = 100 * KB;

  // The minimum size of allocated new space objects to trigger a scavenge.
  static const size_t kMinimumNewSpaceSizeToPerformScavenge = MB / 2;

  // If contexts are disposed at a higher rate a full gc is triggered.
  static const double kHighContextDisposalRate;

  // Incremental marking step time.
  static const size_t kIncrementalMarkingStepTimeInMs = 1;

  static const size_t kMinTimeForOverApproximatingWeakClosureInMs;

  // Number of times we will return a Nothing action in the current mode
  // despite having idle time available before we returning a Done action to
  // ensure we don't keep scheduling idle tasks and making no progress.
  static const int kMaxNoProgressIdleTimes = 10;

  class HeapState {
   public:
    void Print();

    int contexts_disposed;
    double contexts_disposal_rate;
    size_t size_of_objects;
    bool incremental_marking_stopped;
    bool sweeping_in_progress;
    bool sweeping_completed;
    bool has_low_allocation_rate;
    size_t mark_compact_speed_in_bytes_per_ms;
    size_t incremental_marking_speed_in_bytes_per_ms;
    size_t final_incremental_mark_compact_speed_in_bytes_per_ms;
    size_t scavenge_speed_in_bytes_per_ms;
    size_t used_new_space_size;
    size_t new_space_capacity;
    size_t new_space_allocation_throughput_in_bytes_per_ms;
  };

  GCIdleTimeHandler() : idle_times_which_made_no_progress_(0) {}

  GCIdleTimeAction Compute(double idle_time_in_ms, HeapState heap_state);

  void ResetNoProgressCounter() { idle_times_which_made_no_progress_ = 0; }

  static size_t EstimateMarkingStepSize(size_t idle_time_in_ms,
                                        size_t marking_speed_in_bytes_per_ms);

  static size_t EstimateMarkCompactTime(
      size_t size_of_objects, size_t mark_compact_speed_in_bytes_per_ms);

  static size_t EstimateFinalIncrementalMarkCompactTime(
      size_t size_of_objects, size_t mark_compact_speed_in_bytes_per_ms);

  static bool ShouldDoMarkCompact(size_t idle_time_in_ms,
                                  size_t size_of_objects,
                                  size_t mark_compact_speed_in_bytes_per_ms);

  static bool ShouldDoContextDisposalMarkCompact(int context_disposed,
                                                 double contexts_disposal_rate);

  static bool ShouldDoFinalIncrementalMarkCompact(
      size_t idle_time_in_ms, size_t size_of_objects,
      size_t final_incremental_mark_compact_speed_in_bytes_per_ms);

  static bool ShouldDoOverApproximateWeakClosure(size_t idle_time_in_ms);

  static bool ShouldDoScavenge(
      size_t idle_time_in_ms, size_t new_space_size, size_t used_new_space_size,
      size_t scavenger_speed_in_bytes_per_ms,
      size_t new_space_allocation_throughput_in_bytes_per_ms);

 private:
  GCIdleTimeAction NothingOrDone(double idle_time_in_ms);

  // Idle notifications with no progress.
  int idle_times_which_made_no_progress_;

  DISALLOW_COPY_AND_ASSIGN(GCIdleTimeHandler);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_GC_IDLE_TIME_HANDLER_H_

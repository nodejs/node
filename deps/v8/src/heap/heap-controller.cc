// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-controller.h"

#include "src/heap/spaces.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {

// Given GC speed in bytes per ms, the allocation throughput in bytes per ms
// (mutator speed), this function returns the heap growing factor that will
// achieve the target_mutator_utilization_ if the GC speed and the mutator speed
// remain the same until the next GC.
//
// For a fixed time-frame T = TM + TG, the mutator utilization is the ratio
// TM / (TM + TG), where TM is the time spent in the mutator and TG is the
// time spent in the garbage collector.
//
// Let MU be target_mutator_utilization_, the desired mutator utilization for
// the time-frame from the end of the current GC to the end of the next GC.
// Based on the MU we can compute the heap growing factor F as
//
// F = R * (1 - MU) / (R * (1 - MU) - MU), where R = gc_speed / mutator_speed.
//
// This formula can be derived as follows.
//
// F = Limit / Live by definition, where the Limit is the allocation limit,
// and the Live is size of live objects.
// Let’s assume that we already know the Limit. Then:
//   TG = Limit / gc_speed
//   TM = (TM + TG) * MU, by definition of MU.
//   TM = TG * MU / (1 - MU)
//   TM = Limit *  MU / (gc_speed * (1 - MU))
// On the other hand, if the allocation throughput remains constant:
//   Limit = Live + TM * allocation_throughput = Live + TM * mutator_speed
// Solving it for TM, we get
//   TM = (Limit - Live) / mutator_speed
// Combining the two equation for TM:
//   (Limit - Live) / mutator_speed = Limit * MU / (gc_speed * (1 - MU))
//   (Limit - Live) = Limit * MU * mutator_speed / (gc_speed * (1 - MU))
// substitute R = gc_speed / mutator_speed
//   (Limit - Live) = Limit * MU  / (R * (1 - MU))
// substitute F = Limit / Live
//   F - 1 = F * MU  / (R * (1 - MU))
//   F - F * MU / (R * (1 - MU)) = 1
//   F * (1 - MU / (R * (1 - MU))) = 1
//   F * (R * (1 - MU) - MU) / (R * (1 - MU)) = 1
//   F = R * (1 - MU) / (R * (1 - MU) - MU)
double MemoryController::GrowingFactor(double gc_speed, double mutator_speed,
                                       double max_factor) {
  DCHECK_LE(min_growing_factor_, max_factor);
  DCHECK_GE(max_growing_factor_, max_factor);
  if (gc_speed == 0 || mutator_speed == 0) return max_factor;

  const double speed_ratio = gc_speed / mutator_speed;

  const double a = speed_ratio * (1 - target_mutator_utilization_);
  const double b = speed_ratio * (1 - target_mutator_utilization_) -
                   target_mutator_utilization_;

  // The factor is a / b, but we need to check for small b first.
  double factor = (a < b * max_factor) ? a / b : max_factor;
  factor = Min(factor, max_factor);
  factor = Max(factor, min_growing_factor_);
  return factor;
}

size_t MemoryController::CalculateAllocationLimit(
    size_t curr_size, size_t max_size, double max_factor, double gc_speed,
    double mutator_speed, size_t new_space_capacity,
    Heap::HeapGrowingMode growing_mode) {
  double factor = GrowingFactor(gc_speed, mutator_speed, max_factor);

  if (FLAG_trace_gc_verbose) {
    Isolate::FromHeap(heap_)->PrintWithTimestamp(
        "%s factor %.1f based on mu=%.3f, speed_ratio=%.f "
        "(gc=%.f, mutator=%.f)\n",
        ControllerName(), factor, target_mutator_utilization_,
        gc_speed / mutator_speed, gc_speed, mutator_speed);
  }

  if (growing_mode == Heap::HeapGrowingMode::kConservative ||
      growing_mode == Heap::HeapGrowingMode::kSlow) {
    factor = Min(factor, conservative_growing_factor_);
  }

  if (growing_mode == Heap::HeapGrowingMode::kMinimal) {
    factor = min_growing_factor_;
  }

  if (FLAG_heap_growing_percent > 0) {
    factor = 1.0 + FLAG_heap_growing_percent / 100.0;
  }

  CHECK_LT(1.0, factor);
  CHECK_LT(0, curr_size);
  uint64_t limit = static_cast<uint64_t>(curr_size * factor);
  limit = Max(limit, static_cast<uint64_t>(curr_size) +
                         MinimumAllocationLimitGrowingStep(growing_mode));
  limit += new_space_capacity;
  uint64_t halfway_to_the_max =
      (static_cast<uint64_t>(curr_size) + max_size) / 2;
  size_t result = static_cast<size_t>(Min(limit, halfway_to_the_max));

  if (FLAG_trace_gc_verbose) {
    Isolate::FromHeap(heap_)->PrintWithTimestamp(
        "%s Limit: old size: %" PRIuS " KB, new limit: %" PRIuS " KB (%.1f)\n",
        ControllerName(), curr_size / KB, result / KB, factor);
  }

  return result;
}

size_t MemoryController::MinimumAllocationLimitGrowingStep(
    Heap::HeapGrowingMode growing_mode) {
  const size_t kRegularAllocationLimitGrowingStep = 8;
  const size_t kLowMemoryAllocationLimitGrowingStep = 2;
  size_t limit = (Page::kPageSize > MB ? Page::kPageSize : MB);
  return limit * (growing_mode == Heap::HeapGrowingMode::kConservative
                      ? kLowMemoryAllocationLimitGrowingStep
                      : kRegularAllocationLimitGrowingStep);
}

double HeapController::MaxGrowingFactor(size_t curr_max_size) {
  const double min_small_factor = 1.3;
  const double max_small_factor = 2.0;
  const double high_factor = 4.0;

  size_t max_size_in_mb = curr_max_size / MB;
  max_size_in_mb = Max(max_size_in_mb, kMinSize);

  // If we are on a device with lots of memory, we allow a high heap
  // growing factor.
  if (max_size_in_mb >= kMaxSize) {
    return high_factor;
  }

  DCHECK_GE(max_size_in_mb, kMinSize);
  DCHECK_LT(max_size_in_mb, kMaxSize);

  // On smaller devices we linearly scale the factor: (X-A)/(B-A)*(D-C)+C
  double factor = (max_size_in_mb - kMinSize) *
                      (max_small_factor - min_small_factor) /
                      (kMaxSize - kMinSize) +
                  min_small_factor;
  return factor;
}

}  // namespace internal
}  // namespace v8

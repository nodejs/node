// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-controller.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {

const double HeapController::kMinHeapGrowingFactor = 1.1;
const double HeapController::kMaxHeapGrowingFactor = 4.0;
const double HeapController::kConservativeHeapGrowingFactor = 1.3;
const double HeapController::kTargetMutatorUtilization = 0.97;

// Given GC speed in bytes per ms, the allocation throughput in bytes per ms
// (mutator speed), this function returns the heap growing factor that will
// achieve the kTargetMutatorUtilisation if the GC speed and the mutator speed
// remain the same until the next GC.
//
// For a fixed time-frame T = TM + TG, the mutator utilization is the ratio
// TM / (TM + TG), where TM is the time spent in the mutator and TG is the
// time spent in the garbage collector.
//
// Let MU be kTargetMutatorUtilisation, the desired mutator utilization for the
// time-frame from the end of the current GC to the end of the next GC. Based
// on the MU we can compute the heap growing factor F as
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
double HeapController::HeapGrowingFactor(double gc_speed, double mutator_speed,
                                         double max_factor) {
  DCHECK_LE(kMinHeapGrowingFactor, max_factor);
  DCHECK_GE(kMaxHeapGrowingFactor, max_factor);
  if (gc_speed == 0 || mutator_speed == 0) return max_factor;

  const double speed_ratio = gc_speed / mutator_speed;
  const double mu = kTargetMutatorUtilization;

  const double a = speed_ratio * (1 - mu);
  const double b = speed_ratio * (1 - mu) - mu;

  // The factor is a / b, but we need to check for small b first.
  double factor = (a < b * max_factor) ? a / b : max_factor;
  factor = Min(factor, max_factor);
  factor = Max(factor, kMinHeapGrowingFactor);
  return factor;
}

double HeapController::MaxHeapGrowingFactor(size_t max_old_generation_size) {
  const double min_small_factor = 1.3;
  const double max_small_factor = 2.0;
  const double high_factor = 4.0;

  size_t max_old_generation_size_in_mb = max_old_generation_size / MB;
  max_old_generation_size_in_mb =
      Max(max_old_generation_size_in_mb,
          static_cast<size_t>(kMinOldGenerationSize));

  // If we are on a device with lots of memory, we allow a high heap
  // growing factor.
  if (max_old_generation_size_in_mb >= kMaxOldGenerationSize) {
    return high_factor;
  }

  DCHECK_GE(max_old_generation_size_in_mb, kMinOldGenerationSize);
  DCHECK_LT(max_old_generation_size_in_mb, kMaxOldGenerationSize);

  // On smaller devices we linearly scale the factor: (X-A)/(B-A)*(D-C)+C
  double factor = (max_old_generation_size_in_mb - kMinOldGenerationSize) *
                      (max_small_factor - min_small_factor) /
                      (kMaxOldGenerationSize - kMinOldGenerationSize) +
                  min_small_factor;
  return factor;
}

size_t HeapController::CalculateOldGenerationAllocationLimit(
    size_t old_gen_size, size_t max_old_generation_size, double gc_speed,
    double mutator_speed, size_t new_space_capacity,
    Heap::HeapGrowingMode growing_mode) {
  double max_factor = MaxHeapGrowingFactor(max_old_generation_size);
  double factor = HeapGrowingFactor(gc_speed, mutator_speed, max_factor);

  if (FLAG_trace_gc_verbose) {
    heap_->isolate()->PrintWithTimestamp(
        "Heap growing factor %.1f based on mu=%.3f, speed_ratio=%.f "
        "(gc=%.f, mutator=%.f)\n",
        factor, kTargetMutatorUtilization, gc_speed / mutator_speed, gc_speed,
        mutator_speed);
  }

  if (growing_mode == Heap::HeapGrowingMode::kConservative ||
      growing_mode == Heap::HeapGrowingMode::kSlow) {
    factor = Min(factor, kConservativeHeapGrowingFactor);
  }

  if (growing_mode == Heap::HeapGrowingMode::kMinimal) {
    factor = kMinHeapGrowingFactor;
  }

  if (FLAG_heap_growing_percent > 0) {
    factor = 1.0 + FLAG_heap_growing_percent / 100.0;
  }

  CHECK_LT(1.0, factor);
  CHECK_LT(0, old_gen_size);
  uint64_t limit = static_cast<uint64_t>(old_gen_size * factor);
  limit = Max(limit, static_cast<uint64_t>(old_gen_size) +
                         MinimumAllocationLimitGrowingStep(growing_mode));
  limit += new_space_capacity;
  uint64_t halfway_to_the_max =
      (static_cast<uint64_t>(old_gen_size) + max_old_generation_size) / 2;
  size_t result = static_cast<size_t>(Min(limit, halfway_to_the_max));

  if (FLAG_trace_gc_verbose) {
    heap_->isolate()->PrintWithTimestamp(
        "Heap Controller Limit: old size: %" PRIuS " KB, new limit: %" PRIuS
        " KB (%.1f)\n",
        old_gen_size / KB, result / KB, factor);
  }

  return result;
}

size_t HeapController::MinimumAllocationLimitGrowingStep(
    Heap::HeapGrowingMode growing_mode) {
  const size_t kRegularAllocationLimitGrowingStep = 8;
  const size_t kLowMemoryAllocationLimitGrowingStep = 2;
  size_t limit = (Page::kPageSize > MB ? Page::kPageSize : MB);
  return limit * (growing_mode == Heap::HeapGrowingMode::kConservative
                      ? kLowMemoryAllocationLimitGrowingStep
                      : kRegularAllocationLimitGrowingStep);
}

}  // namespace internal
}  // namespace v8

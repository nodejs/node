// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-controller.h"

#include "src/execution/isolate-inl.h"
#include "src/heap/spaces.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {
namespace {

size_t MaximumGlobalMemorySizeFromV8Size(size_t v8_limit,
                                         size_t physical_memory) {
  const size_t kGlobalMemoryToV8Ratio = 8;
  return std::min(static_cast<uint64_t>(
                      physical_memory > 0 ? physical_memory
                                          : std::numeric_limits<size_t>::max()),
                  static_cast<uint64_t>(v8_limit) * kGlobalMemoryToV8Ratio);
}

}  // namespace

template <typename Trait>
double MemoryController<Trait>::GrowingFactor(
    Isolate* isolate, uint64_t physical_memory, size_t max_heap_size,
    std::optional<double> gc_speed, double mutator_speed,
    Heap::HeapGrowingMode growing_mode) {
  const double max_factor = MaxGrowingFactor(physical_memory, max_heap_size);
  double factor = DynamicGrowingFactor(gc_speed, mutator_speed, max_factor);
  switch (growing_mode) {
    case Heap::HeapGrowingMode::kConservative:
    case Heap::HeapGrowingMode::kSlow:
      factor = std::min({factor, Trait::kConservativeGrowingFactor});
      break;
    case Heap::HeapGrowingMode::kMinimal:
      factor = Trait::kMinGrowingFactor;
      break;
    case Heap::HeapGrowingMode::kDefault:
      break;
  }
  if (v8_flags.heap_growing_percent > 0) {
    factor = 1.0 + v8_flags.heap_growing_percent / 100.0;
  }
  if (V8_UNLIKELY(v8_flags.trace_gc_verbose)) {
    isolate->PrintWithTimestamp(
        "[%s] factor %.1f based on mu=%.3f, speed_ratio=%.f "
        "(gc=%.f, mutator=%.f)\n",
        Trait::kName, factor, Trait::kTargetMutatorUtilization,
        gc_speed.value_or(0) / mutator_speed, gc_speed.value_or(0),
        mutator_speed);
  }
  return factor;
}

template <typename Trait>
double MemoryController<Trait>::MaxGrowingFactor(uint64_t physical_memory,
                                                 size_t max_heap_size) {
  constexpr double kMinSmallFactor = 1.3;
  constexpr double kMaxSmallFactor = 2.0;
  constexpr double kHighFactor = 4.0;

  // If we are on a device with lots of memory, we allow a high heap
  // growing factor.
  if (max_heap_size >= Heap::DefaultMaxHeapSize(physical_memory)) {
    return kHighFactor;
  }

  size_t max_size =
      std::max({max_heap_size, Heap::DefaultMinHeapSize(physical_memory)});

  DCHECK_GE(max_size, Heap::DefaultMinHeapSize(physical_memory));
  DCHECK_LT(max_size, Heap::DefaultMaxHeapSize(physical_memory));

  // On smaller devices we linearly scale the factor: C+(D-C)*(X-A)/(B-A)
  double factor = kMinSmallFactor +
                  (kMaxSmallFactor - kMinSmallFactor) *
                      (max_size - Heap::DefaultMinHeapSize(physical_memory)) /
                      (Heap::DefaultMaxHeapSize(physical_memory) -
                       Heap::DefaultMinHeapSize(physical_memory));
  return factor;
}

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
template <typename Trait>
double MemoryController<Trait>::DynamicGrowingFactor(
    std::optional<double> gc_speed, double mutator_speed, double max_factor) {
  DCHECK_LE(Trait::kMinGrowingFactor, max_factor);
  DCHECK_GE(Trait::kMaxGrowingFactor, max_factor);
  if (!gc_speed || mutator_speed == 0) return max_factor;

  const double speed_ratio = *gc_speed / mutator_speed;

  const double a = speed_ratio * (1 - Trait::kTargetMutatorUtilization);
  const double b = speed_ratio * (1 - Trait::kTargetMutatorUtilization) -
                   Trait::kTargetMutatorUtilization;

  // The factor is a / b, but we need to check for small b first.
  double factor = (a < b * max_factor) ? a / b : max_factor;
  DCHECK_LE(factor, max_factor);
  factor = std::max({factor, Trait::kMinGrowingFactor});
  return factor;
}

template <typename Trait>
size_t MemoryController<Trait>::MinimumAllocationLimitGrowingStep(
    Heap::HeapGrowingMode growing_mode) {
  const size_t kRegularAllocationLimitGrowingStep = 8;
  const size_t kLowMemoryAllocationLimitGrowingStep = 2;
  size_t limit = (NormalPage::kPageSize > MB ? NormalPage::kPageSize : MB);
  return limit * (growing_mode == Heap::HeapGrowingMode::kConservative
                      ? kLowMemoryAllocationLimitGrowingStep
                      : kRegularAllocationLimitGrowingStep);
}

template <typename Trait>
size_t MemoryController<Trait>::BoundAllocationLimit(
    Isolate* isolate, size_t current_size, uint64_t limit, size_t min_size,
    size_t max_size, size_t new_space_capacity,
    Heap::HeapGrowingMode growing_mode) {
  CHECK_LT(0, current_size);
  limit = std::max(limit, static_cast<uint64_t>(current_size) +
                              MinimumAllocationLimitGrowingStep(growing_mode)) +
          new_space_capacity;
  const uint64_t halfway_to_the_max =
      (static_cast<uint64_t>(current_size) + max_size) / 2;
  const uint64_t limit_or_halfway =
      std::min<uint64_t>(limit, halfway_to_the_max);
  const size_t result =
      static_cast<size_t>(std::max<uint64_t>(limit_or_halfway, min_size));
  if (V8_UNLIKELY(v8_flags.trace_gc_verbose)) {
    isolate->PrintWithTimestamp(
        "[%s] Limit: old size: %zu KB, new limit: %zu KB\n", Trait::kName,
        current_size / KB, result / KB);
  }
  return result;
}

template class V8_EXPORT_PRIVATE MemoryController<V8HeapTrait>;
template class V8_EXPORT_PRIVATE MemoryController<GlobalMemoryTrait>;

size_t HeapLimits::GlobalMemorySizeFromV8Size(size_t v8_size) {
  const size_t kGlobalMemoryToV8Ratio = 2;
  return std::min(static_cast<uint64_t>(std::numeric_limits<size_t>::max()),
                  static_cast<uint64_t>(v8_size) * kGlobalMemoryToV8Ratio);
}

HeapLimitBounds HeapLimits::AtLeastCurrentLimits() const {
  return {.minimum_old_generation_allocation_limit =
              old_generation_allocation_limit(),
          .minimum_global_allocation_limit = global_allocation_limit()};
}

HeapLimitBounds HeapLimits::AtMostCurrentLimits() const {
  return {.maximum_old_generation_allocation_limit =
              old_generation_allocation_limit(),
          .maximum_global_allocation_limit = global_allocation_limit()};
}

HeapLimits::HeapLimits(Heap* heap, const ResourceConstraints& constraints,
                       size_t max_old_generation_size,
                       size_t initial_old_generation_size)
    : heap_(heap), tracing_track_(heap_->tracing_track()) {
  initial_old_generation_size_ = initial_old_generation_size;
  initial_max_old_generation_size_ = max_old_generation_size;
  SetMaximumSizes(max_old_generation_size,
                  constraints.physical_memory_size_in_bytes());
  ResetAllocationLimit();
}

Heap::LimitsComputationResult HeapLimits::UpdateAllocationLimits(
    Heap::HeapGrowingMode mode, const HeapLimitBounds& boundaries,
    const char* caller) {
  DCHECK(!using_initial_limit());
  tracer()->RecordGCSizeCounters();
  const std::optional<double> v8_gc_speed =
      tracer()->OldGenerationSpeedInBytesPerMillisecond();
  const double v8_mutator_speed =
      tracer()->OldGenerationAllocationThroughputInBytesPerMillisecond();
  const double v8_growing_factor = MemoryController<V8HeapTrait>::GrowingFactor(
      isolate(), heap_->physical_memory(), max_old_generation_size(),
      v8_gc_speed, v8_mutator_speed, mode);
  const std::optional<double> embedder_gc_speed =
      tracer()->EmbedderSpeedInBytesPerMillisecond();
  const double embedder_speed =
      tracer()->EmbedderAllocationThroughputInBytesPerMillisecond();
  const double embedder_growing_factor =
      (embedder_gc_speed.has_value() && embedder_speed > 0)
          ? MemoryController<GlobalMemoryTrait>::GrowingFactor(
                isolate(), heap_->physical_memory(), max_global_memory_size(),
                embedder_gc_speed, embedder_speed, mode)
          : BaseControllerTrait::kMinGrowingFactor;

  const size_t new_space_capacity = heap_->NewSpaceTargetCapacity();

  TRACE_COUNTER(
      TRACE_DISABLED_BY_DEFAULT("v8.gc"),
      perfetto::CounterTrack("NewSpaceTargetCapacity", tracing_track_),
      new_space_capacity);
  TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                perfetto::CounterTrack("OldGenerationSpeed", tracing_track_),
                v8_gc_speed.value_or(0.0));
  TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                perfetto::CounterTrack("EmbedderSpeed", tracing_track_),
                embedder_gc_speed.value_or(0.0));

  const size_t old_gen_consumed_bytes_at_last_gc =
      OldGenerationConsumedBytesAtLastGC();
  const size_t computed_old_generation_allocation_limit =
      old_gen_consumed_bytes_at_last_gc * v8_growing_factor;
  const size_t preliminary_old_generation_allocation_limit =
      MemoryController<V8HeapTrait>::BoundAllocationLimit(
          isolate(), old_gen_consumed_bytes_at_last_gc,
          computed_old_generation_allocation_limit, min_old_generation_size_,
          max_old_generation_size(), new_space_capacity, mode);

  const double global_growing_factor =
      std::max(v8_growing_factor, embedder_growing_factor);
  const double external_growing_factor =
      std::min(global_growing_factor,
               v8_flags.external_memory_max_growing_factor.value());
  DCHECK_GT(global_growing_factor, 0);
  DCHECK_GT(external_growing_factor, 0);
  const size_t global_consumed_bytes_at_last_gc = GlobalConsumedBytesAtLastGC();
  const size_t computed_global_allocation_limit =
      (old_gen_consumed_bytes_at_last_gc + embedder_size_at_last_gc_) *
          global_growing_factor +
      (v8_flags.external_memory_accounted_in_global_limit
           ? external_memory_low_since_last_gc() * external_growing_factor
           : 0);
  const size_t preliminary_global_allocation_limit =
      MemoryController<GlobalMemoryTrait>::BoundAllocationLimit(
          isolate(), global_consumed_bytes_at_last_gc,
          computed_global_allocation_limit, min_global_memory_size_,
          max_global_memory_size(), new_space_capacity, mode);

  // Now enforce provided boundaries on computed/preliminary limits.
  const size_t next_old_generation_allocation_limit =
      boundaries.bounded_old_generation_allocation_limit(
          preliminary_old_generation_allocation_limit);
  const size_t next_global_allocation_limit =
      boundaries.bounded_global_allocation_limit(
          preliminary_global_allocation_limit);

  CHECK_GE(next_global_allocation_limit, next_old_generation_allocation_limit);

  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("v8.gc"), "V8.GCUpdateAllocationLimits",
      "value", [&](perfetto::TracedValue ctx) {
        auto dict = std::move(ctx).WriteDictionary();
        dict.Add("caller", caller);
        dict.Add("growing_mode", ToString(mode));
        dict.Add("v8_gc_speed", v8_gc_speed.value_or(0));
        dict.Add("v8_mutator_speed", v8_mutator_speed);
        dict.Add("v8_growing_factor", v8_growing_factor);
        dict.Add("old_gen_allocation_limit", old_generation_allocation_limit());
        dict.Add("next_old_gen_allocation_limit",
                 next_old_generation_allocation_limit);
        dict.Add("computed_old_generation_allocation_limit",
                 computed_old_generation_allocation_limit);
        dict.Add("preliminary_old_gen_allocation_limit",
                 preliminary_old_generation_allocation_limit);
        dict.Add("old_gen_consumed_bytes_at_last_gc",
                 old_gen_consumed_bytes_at_last_gc);
        dict.Add("old_gen_allocation_limit_consumed_bytes",
                 heap_->OldGenerationAllocationLimitConsumedBytes());
        dict.Add("global_gc_speed", embedder_gc_speed.value_or(0));
        dict.Add("global_mutator_speed", embedder_speed);
        dict.Add("global_growing_factor", global_growing_factor);
        dict.Add("global_allocation_limit", global_allocation_limit());
        dict.Add("next_global_allocation_limit", next_global_allocation_limit);
        dict.Add("computed_global_allocation_limit",
                 computed_global_allocation_limit);
        dict.Add("preliminary_global_allocation_limit",
                 preliminary_global_allocation_limit);
        dict.Add("global_consumed_bytes_at_last_gc",
                 global_consumed_bytes_at_last_gc);
        dict.Add("global_consumed_bytes", heap_->GlobalConsumedBytes());
        dict.Add("embedder_size_at_last_gc", embedder_size_at_last_gc_);
        dict.Add("external_growing_factor", external_growing_factor);
        dict.Add("external_memory_low_since_last_gc",
                 external_memory_low_since_last_gc());
        dict.Add(
            "v8_min_allocation_limit_growing_step",
            MemoryController<V8HeapTrait>::MinimumAllocationLimitGrowingStep(
                mode));
        dict.Add(
            "global_min_allocation_limit_growing_step",
            MemoryController<
                GlobalMemoryTrait>::MinimumAllocationLimitGrowingStep(mode));
        dict.Add("max_old_generation_size", max_old_generation_size());
        dict.Add("max_global_memory_size", max_global_memory_size());
        dict.Add("boundary_min_old_gen_allocation_limit",
                 boundaries.minimum_old_generation_allocation_limit);
        dict.Add("boundary_max_old_gen_allocation_limit",
                 boundaries.maximum_old_generation_allocation_limit);
        dict.Add("boundary_min_global_allocation_limit",
                 boundaries.minimum_global_allocation_limit);
        dict.Add("boundary_max_global_allocation_limit",
                 boundaries.maximum_global_allocation_limit);
        dict.Add("new_space_capacity", new_space_capacity);
      });

  SetAllocationLimit(next_old_generation_allocation_limit,
                     next_global_allocation_limit);

  return {next_old_generation_allocation_limit, next_global_allocation_limit};
}

// When old generation allocation limit is not configured (before the first full
// GC), this method shrinks the initial very large old generation size. This
// method can only shrink allocation limits but not increase it again.
void HeapLimits::ShrinkAllocationLimitIfNotConfigured(
    Heap::HeapGrowingMode mode, size_t old_generation_consumed,
    size_t global_consumed) {
  if (!using_initial_limit()) {
    return;
  }
  const size_t minimum_growing_step =
      MemoryController<V8HeapTrait>::MinimumAllocationLimitGrowingStep(mode);
  size_t new_old_generation_allocation_limit =
      std::max(old_generation_consumed + minimum_growing_step,
               static_cast<size_t>(
                   static_cast<double>(old_generation_allocation_limit()) *
                   (tracer()->AverageSurvivalRatio() / 100)));
  new_old_generation_allocation_limit = std::min(
      new_old_generation_allocation_limit, old_generation_allocation_limit());
  size_t new_global_allocation_limit = std::max(
      global_consumed + minimum_growing_step,
      static_cast<size_t>(static_cast<double>(global_allocation_limit()) *
                          (tracer()->AverageSurvivalRatio() / 100)));
  new_global_allocation_limit =
      std::min(new_global_allocation_limit, global_allocation_limit());
  SetAllocationLimit(new_old_generation_allocation_limit,
                     new_global_allocation_limit);
}

void HeapLimits::SetAllocationLimit(size_t new_old_generation_allocation_limit,
                                    size_t new_global_allocation_limit,
                                    const char* reason) {
  CHECK_GE(new_global_allocation_limit, new_old_generation_allocation_limit);
  TRACE_COUNTER(
      "v8.memory",
      perfetto::CounterTrack("OldGenerationAllocationLimit", tracing_track_),
      new_old_generation_allocation_limit);
  TRACE_COUNTER("v8.memory",
                perfetto::CounterTrack("GlobalAllocationLimit", tracing_track_),
                new_global_allocation_limit);

  old_generation_allocation_limit_.store(new_old_generation_allocation_limit,
                                         std::memory_order_relaxed);
  global_allocation_limit_.store(new_global_allocation_limit,
                                 std::memory_order_relaxed);
}

void HeapLimits::ResetAllocationLimit() {
  SetAllocationLimit(initial_old_generation_size_,
                     GlobalMemorySizeFromV8Size(initial_old_generation_size_));
  set_using_initial_limit(true);
}

void HeapLimits::SetMaximumSizes(size_t max_old_generation_size,
                                 size_t physical_memory) {
  max_old_generation_size_.store(max_old_generation_size,
                                 std::memory_order_relaxed);
  max_global_memory_size_.store(
      v8_flags.ineffective_gc_includes_global
          ? MaximumGlobalMemorySizeFromV8Size(max_old_generation_size,
                                              physical_memory)
          : GlobalMemorySizeFromV8Size(max_old_generation_size),
      std::memory_order_relaxed);
}

void HeapLimits::MaybeResetMaximumSizes(size_t physical_memory) {
  if (initial_max_old_generation_size_ < max_old_generation_size()) {
    SetMaximumSizes(initial_max_old_generation_size_, physical_memory);
  }
}

void HeapLimits::SetMinimumSizes(size_t min_old_generation_size,
                                 size_t physical_memory) {
  min_old_generation_size_ = min_old_generation_size;
  min_global_memory_size_ =
      GlobalMemorySizeFromV8Size(min_old_generation_size_);
}

void HeapLimits::UpdateConsumedAfterGC() {
  old_generation_size_at_last_gc_ = heap_->OldGenerationSizeOfObjects();
  old_generation_wasted_at_last_gc_ = heap_->OldGenerationWastedBytes();
  embedder_size_at_last_gc_ = heap_->EmbedderSizeOfObjects();
  // The GC may call `UpdateLowSinceMarkCompact` even when
  // `is_external_memory_limit_updates_suspended_` is true.
  uint64_t external_memory_total = heap_->external_memory();
  UpdateExternalMemoryLowSinceLastGC(external_memory_total);
  // Limits can now be computed based on estimate from MARK_COMPACT.
  set_using_initial_limit(false);
}

void HeapLimits::UpdateExternalMemoryLowSinceLastGC(uint64_t value) {
  external_memory_low_since_last_gc_.store(value, std::memory_order_relaxed);
  UpdateExternalMemoryLimitForInterrupt(value);
}

void HeapLimits::UpdateExternalMemoryLimitForInterrupt(uint64_t amount) {
  external_memory_limit_for_interrupt_.store(
      amount + kExternalAllocationLimitForInterrupt, std::memory_order_relaxed);
}

size_t HeapLimits::OldGenerationConsumedBytesAtLastGC() const {
  return old_generation_size_at_last_gc_ + old_generation_wasted_at_last_gc_;
}

size_t HeapLimits::GlobalConsumedBytesAtLastGC() const {
  return OldGenerationConsumedBytesAtLastGC() + embedder_size_at_last_gc_ +
         (v8_flags.external_memory_accounted_in_global_limit
              ? external_memory_low_since_last_gc()
              : 0);
}

size_t HeapLimits::PromotedSinceLastGC(size_t old_generation_size) const {
  return old_generation_size > old_generation_size_at_last_gc_
             ? old_generation_size - old_generation_size_at_last_gc_
             : 0;
}

Isolate* HeapLimits::isolate() const { return heap_->isolate(); }

}  // namespace internal
}  // namespace v8

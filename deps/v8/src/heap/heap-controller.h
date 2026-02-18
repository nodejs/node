// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_CONTROLLER_H_
#define V8_HEAP_HEAP_CONTROLLER_H_

#include <cstddef>

#include "src/heap/heap.h"
#include "src/utils/allocation.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {
namespace internal {

class Isolate;

struct BaseControllerTrait {
  static constexpr double kMinGrowingFactor = 1.1;
  static constexpr double kMaxGrowingFactor = 4.0;
  static constexpr double kConservativeGrowingFactor = 1.3;
  static constexpr double kTargetMutatorUtilization = 0.97;
};

struct V8HeapTrait : public BaseControllerTrait {
  static constexpr char kName[] = "HeapController";
};

struct GlobalMemoryTrait : public BaseControllerTrait {
  static constexpr char kName[] = "GlobalMemoryController";
};

template <typename Trait>
class V8_EXPORT_PRIVATE MemoryController : public AllStatic {
 public:
  // Computes the growing step when the limit increases.
  static size_t MinimumAllocationLimitGrowingStep(
      Heap::HeapGrowingMode growing_mode);

  static double GrowingFactor(Isolate* isolate, uint64_t physical_memory,
                              size_t max_heap_size,
                              std::optional<double> gc_speed,
                              double mutator_speed,
                              Heap::HeapGrowingMode growing_mode);

  static size_t BoundAllocationLimit(Isolate* isolate, size_t current_size,
                                     uint64_t limit, size_t min_size,
                                     size_t max_size, size_t new_space_capacity,
                                     Heap::HeapGrowingMode growing_mode);

 private:
  static double MaxGrowingFactor(uint64_t physical_memory,
                                 size_t max_heap_size);
  static double DynamicGrowingFactor(std::optional<double> gc_speed,
                                     double mutator_speed, double max_factor);

  FRIEND_TEST(MemoryControllerTest, HeapGrowingFactor);
  FRIEND_TEST(MemoryControllerTest, MaxHeapGrowingFactor);
};

struct HeapLimitBounds {
  size_t minimum_old_generation_allocation_limit = 0;
  size_t maximum_old_generation_allocation_limit = SIZE_MAX;

  size_t minimum_global_allocation_limit = 0;
  size_t maximum_global_allocation_limit = SIZE_MAX;

  constexpr size_t bounded_old_generation_allocation_limit(size_t val) const {
    DCHECK_LE(minimum_old_generation_allocation_limit,
              maximum_old_generation_allocation_limit);
    return std::clamp(val, minimum_old_generation_allocation_limit,
                      maximum_old_generation_allocation_limit);
  }

  constexpr size_t bounded_global_allocation_limit(size_t val) const {
    DCHECK_LE(minimum_global_allocation_limit, maximum_global_allocation_limit);
    return std::clamp(val, minimum_global_allocation_limit,
                      maximum_global_allocation_limit);
  }

  void AtLeast(size_t new_min_old_gen_limit, size_t new_min_global_limit) {
    minimum_old_generation_allocation_limit =
        bounded_old_generation_allocation_limit(new_min_old_gen_limit);
    minimum_global_allocation_limit =
        bounded_global_allocation_limit(new_min_global_limit);
  }

  void AtMost(size_t new_max_old_gen_limit, size_t new_max_global_limit) {
    maximum_old_generation_allocation_limit =
        bounded_old_generation_allocation_limit(new_max_old_gen_limit);
    maximum_global_allocation_limit =
        bounded_global_allocation_limit(new_max_global_limit);
  }
};

class V8_EXPORT_PRIVATE HeapLimits {
 public:
  static size_t GlobalMemorySizeFromV8Size(size_t v8_size);

  HeapLimits(Heap* heap, const ResourceConstraints& constraints,
             size_t max_old_generation_size,
             size_t initial_old_generation_size);

  HeapLimitBounds AtLeastCurrentLimits() const;
  HeapLimitBounds AtMostCurrentLimits() const;

  Heap::LimitsComputationResult UpdateAllocationLimits(
      Heap::HeapGrowingMode mode, const HeapLimitBounds& boundaries,
      const char* caller = __builtin_FUNCTION());

  void ShrinkAllocationLimitIfNotConfigured(Heap::HeapGrowingMode mode,
                                            size_t old_generation_consumed,
                                            size_t global_consumed);

  // Sets allocation limits for both old generation and the global heap.
  void SetAllocationLimit(size_t new_old_generation_allocation_limit,
                          size_t new_global_allocation_limit,
                          const char* reason = __builtin_FUNCTION());
  void ResetAllocationLimit();

  // Sets max/min old generation size and computes the new global heap limit
  // from it.
  void SetMinimumSizes(size_t min_old_generation_size, size_t physical_memory);
  void SetMaximumSizes(size_t max_old_generation_size, size_t physical_memory);
  void MaybeResetMaximumSizes(size_t physical_memory);

  void UpdateConsumedAfterGC();

  void UpdateExternalMemoryLowSinceLastGC(uint64_t value);

  void UpdateExternalMemoryLimitForInterrupt(uint64_t amount);

  // Returns the size of objects in old generation after the last MarkCompact
  // GC.
  size_t OldGenerationConsumedBytesAtLastGC() const;

  // Returns the global amount of bytes after the last MarkCompact GC.
  size_t GlobalConsumedBytesAtLastGC() const;

  size_t PromotedSinceLastGC(size_t old_generation_size) const;

  bool using_initial_limit() const {
    return using_initial_limit_.load(std::memory_order_relaxed);
  }

  size_t old_generation_allocation_limit() const {
    return old_generation_allocation_limit_.load(std::memory_order_relaxed);
  }

  size_t global_allocation_limit() const {
    return global_allocation_limit_.load(std::memory_order_relaxed);
  }

  // This guards against too eager finalization in small heaps.
  // The number is chosen based on v8.browsing_mobile on Nexus 7v2.
  static constexpr size_t kMarginForSmallHeaps = 32u * MB;

  // Overshoot margin is 50% of allocation limit or half-way to the max heap
  // with special handling of small heaps.
  size_t old_generation_overshoot_margin() const {
    return std::min(
        std::max(old_generation_allocation_limit() / 2, kMarginForSmallHeaps),
        (max_old_generation_size() - old_generation_allocation_limit()) / 2);
  }

  size_t global_overshoot_margin() const {
    return std::min(
        std::max(global_allocation_limit() / 2, kMarginForSmallHeaps),
        (max_global_memory_size() - global_allocation_limit()) / 2);
  }

  size_t min_global_memory_size() const { return min_global_memory_size_; }
  size_t max_global_memory_size() const {
    return max_global_memory_size_.load(std::memory_order_relaxed);
  }
  size_t initial_max_old_generation_size() const {
    return initial_max_old_generation_size_;
  }
  size_t initial_old_generation_size() const {
    return initial_old_generation_size_;
  }

  size_t min_old_generation_size() const { return min_old_generation_size_; }
  size_t max_old_generation_size() const {
    return max_old_generation_size_.load(std::memory_order_relaxed);
  }

  uint64_t external_memory_low_since_last_gc() const {
    return external_memory_low_since_last_gc_.load(std::memory_order_relaxed);
  }

  uint64_t external_memory_limit_for_interrupt() const {
    return external_memory_limit_for_interrupt_.load(std::memory_order_relaxed);
  }

  void set_using_initial_limit(bool value) {
    using_initial_limit_.store(value, std::memory_order_relaxed);
  }

 private:
  static constexpr size_t kExternalAllocationLimitForInterrupt = 128 * KB;

  GCTracer* tracer() { return heap_->tracer(); }
  const GCTracer* tracer() const { return heap_->tracer(); }

  Isolate* isolate() const;

  Heap* const heap_;

  perfetto::NamedTrack tracing_track_;

  size_t initial_max_old_generation_size_ = 0;
  size_t initial_old_generation_size_ = 0;

  // Before the first full GC the old generation allocation limit is considered
  // to be *not* configured (unless initial limits were provided by the
  // embedder, see below). In this mode V8 starts with a very large old
  // generation allocation limit initially. Minor GCs may then shrink this
  // initial limit down until the first full GC computes a proper old generation
  // allocation limit in Heap::RecomputeLimits. The old generation allocation
  // limit is then considered to be configured for all subsequent GCs. After the
  // first full GC this field is only ever reset for top context disposals.
  std::atomic<bool> using_initial_limit_ = true;

  // Full garbage collections can be skipped if the old generation size
  // is below this threshold.
  size_t min_old_generation_size_ = 0;
  // If the old generation size exceeds this limit, then V8 will
  // crash with out-of-memory error.
  std::atomic<size_t> max_old_generation_size_{0};
  // TODO(mlippautz): Clarify whether this should take some embedder
  // configurable limit into account.
  size_t min_global_memory_size_ = 0;
  std::atomic<size_t> max_global_memory_size_{0};

  // The size of objects in old generation after the last MarkCompact GC.
  size_t old_generation_size_at_last_gc_{0};

  // The wasted bytes in old generation after the last MarkCompact GC.
  size_t old_generation_wasted_at_last_gc_{0};

  // The size of embedder memory after the last MarkCompact GC.
  size_t embedder_size_at_last_gc_ = 0;

  // Caches the amount of external memory registered at the last MC.
  std::atomic<uint64_t> external_memory_low_since_last_gc_{0};

  // The limit when to trigger memory pressure from the API.
  std::atomic<uint64_t> external_memory_limit_for_interrupt_{
      kExternalAllocationLimitForInterrupt};

  // Limit that triggers a global GC on the next (normally caused) GC.  This
  // is checked when we have already decided to do a GC to help determine
  // which collector to invoke, before expanding a paged space in the old
  // generation and on every allocation in large object space.
  std::atomic<size_t> old_generation_allocation_limit_{0};
  std::atomic<size_t> global_allocation_limit_{0};
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_CONTROLLER_H_

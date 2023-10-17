// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap-growing.h"

#include <cmath>
#include <memory>

#include "include/cppgc/platform.h"
#include "src/base/macros.h"
#include "src/heap/base/incremental-marking-schedule.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/stats-collector.h"
#include "src/heap/cppgc/task-handle.h"

namespace cppgc {
namespace internal {

namespace {
// Minimum ratio between limit for incremental GC and limit for atomic GC
// (to guarantee that limits are not to close to each other).
constexpr double kMaximumLimitRatioForIncrementalGC = 0.9;
// Minimum ratio between limit for incremental GC and limit for atomic GC
// (to guarantee that limit is not too close to current allocated size).
constexpr double kMinimumLimitRatioForIncrementalGC = 0.5;
}  // namespace

class HeapGrowing::HeapGrowingImpl final
    : public StatsCollector::AllocationObserver {
 public:
  HeapGrowingImpl(GarbageCollector*, StatsCollector*,
                  cppgc::Heap::ResourceConstraints, cppgc::Heap::MarkingType,
                  cppgc::Heap::SweepingType);
  ~HeapGrowingImpl();

  HeapGrowingImpl(const HeapGrowingImpl&) = delete;
  HeapGrowingImpl& operator=(const HeapGrowingImpl&) = delete;

  void AllocatedObjectSizeIncreased(size_t) final;
  // Only trigger GC on growing.
  void AllocatedObjectSizeDecreased(size_t) final {}
  void ResetAllocatedObjectSize(size_t) final;

  size_t limit_for_atomic_gc() const { return limit_for_atomic_gc_; }
  size_t limit_for_incremental_gc() const { return limit_for_incremental_gc_; }

  void DisableForTesting();

 private:
  void ConfigureLimit(size_t allocated_object_size);

  GarbageCollector* collector_;
  StatsCollector* stats_collector_;
  // Allow 1 MB heap by default;
  size_t initial_heap_size_ = 1 * kMB;
  size_t limit_for_atomic_gc_ = 0;       // See ConfigureLimit().
  size_t limit_for_incremental_gc_ = 0;  // See ConfigureLimit().

  SingleThreadedHandle gc_task_handle_;

  bool disabled_for_testing_ = false;

  const cppgc::Heap::MarkingType marking_support_;
  const cppgc::Heap::SweepingType sweeping_support_;
};

HeapGrowing::HeapGrowingImpl::HeapGrowingImpl(
    GarbageCollector* collector, StatsCollector* stats_collector,
    cppgc::Heap::ResourceConstraints constraints,
    cppgc::Heap::MarkingType marking_support,
    cppgc::Heap::SweepingType sweeping_support)
    : collector_(collector),
      stats_collector_(stats_collector),
      gc_task_handle_(SingleThreadedHandle::NonEmptyTag{}),
      marking_support_(marking_support),
      sweeping_support_(sweeping_support) {
  if (constraints.initial_heap_size_bytes > 0) {
    initial_heap_size_ = constraints.initial_heap_size_bytes;
  }
  constexpr size_t kNoAllocatedBytes = 0;
  ConfigureLimit(kNoAllocatedBytes);
  stats_collector->RegisterObserver(this);
}

HeapGrowing::HeapGrowingImpl::~HeapGrowingImpl() {
  stats_collector_->UnregisterObserver(this);
}

void HeapGrowing::HeapGrowingImpl::AllocatedObjectSizeIncreased(size_t) {
  if (disabled_for_testing_) return;
  size_t allocated_object_size = stats_collector_->allocated_object_size();
  if (allocated_object_size > limit_for_atomic_gc_) {
    collector_->CollectGarbage(
        {CollectionType::kMajor, StackState::kMayContainHeapPointers,
         GCConfig::MarkingType::kAtomic, sweeping_support_});
  } else if (allocated_object_size > limit_for_incremental_gc_) {
    if (marking_support_ == cppgc::Heap::MarkingType::kAtomic) return;
    collector_->StartIncrementalGarbageCollection(
        {CollectionType::kMajor, StackState::kMayContainHeapPointers,
         marking_support_, sweeping_support_});
  }
}

void HeapGrowing::HeapGrowingImpl::ResetAllocatedObjectSize(
    size_t allocated_object_size) {
  ConfigureLimit(allocated_object_size);
}

void HeapGrowing::HeapGrowingImpl::ConfigureLimit(
    size_t allocated_object_size) {
  const size_t size = std::max(allocated_object_size, initial_heap_size_);
  limit_for_atomic_gc_ = std::max(static_cast<size_t>(size * kGrowingFactor),
                                  size + kMinLimitIncrease);
  // Estimate when to start incremental GC based on current allocation speed.
  // Ideally we start incremental GC such that it is ready to finalize no
  // later than when we reach |limit_for_atomic_gc_|. However, we need to cap
  // |limit_for_incremental_gc_| within a range to prevent:
  // 1) |limit_for_incremental_gc_| being too close to |limit_for_atomic_gc_|
  //    such that incremental gc gets nothing done before reaching
  //    |limit_for_atomic_gc_| (in case where the allocation rate is very low).
  // 2) |limit_for_incremental_gc_| being too close to |size| such that GC is
  //    essentially always running and write barriers are always active (in
  //    case allocation rate is very high).
  size_t estimated_bytes_allocated_during_incremental_gc =
      std::ceil(heap::base::IncrementalMarkingSchedule::kEstimatedMarkingTime
                    .InMillisecondsF() *
                stats_collector_->GetRecentAllocationSpeedInBytesPerMs());
  size_t limit_incremental_gc_based_on_allocation_rate =
      limit_for_atomic_gc_ - estimated_bytes_allocated_during_incremental_gc;
  size_t maximum_limit_incremental_gc =
      size + (limit_for_atomic_gc_ - size) * kMaximumLimitRatioForIncrementalGC;
  size_t minimum_limit_incremental_gc =
      size + (limit_for_atomic_gc_ - size) * kMinimumLimitRatioForIncrementalGC;
  limit_for_incremental_gc_ =
      std::max(minimum_limit_incremental_gc,
               std::min(maximum_limit_incremental_gc,
                        limit_incremental_gc_based_on_allocation_rate));
}

void HeapGrowing::HeapGrowingImpl::DisableForTesting() {
  disabled_for_testing_ = true;
}

HeapGrowing::HeapGrowing(GarbageCollector* collector,
                         StatsCollector* stats_collector,
                         cppgc::Heap::ResourceConstraints constraints,
                         cppgc::Heap::MarkingType marking_support,
                         cppgc::Heap::SweepingType sweeping_support)
    : impl_(std::make_unique<HeapGrowing::HeapGrowingImpl>(
          collector, stats_collector, constraints, marking_support,
          sweeping_support)) {}

HeapGrowing::~HeapGrowing() = default;

size_t HeapGrowing::limit_for_atomic_gc() const {
  return impl_->limit_for_atomic_gc();
}
size_t HeapGrowing::limit_for_incremental_gc() const {
  return impl_->limit_for_incremental_gc();
}

void HeapGrowing::DisableForTesting() { impl_->DisableForTesting(); }

// static
constexpr double HeapGrowing::kGrowingFactor;

}  // namespace internal
}  // namespace cppgc

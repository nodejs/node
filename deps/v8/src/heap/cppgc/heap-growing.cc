// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap-growing.h"

#include <memory>

#include "include/cppgc/platform.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/stats-collector.h"
#include "src/heap/cppgc/task-handle.h"

namespace cppgc {
namespace internal {

class HeapGrowing::HeapGrowingImpl final
    : public StatsCollector::AllocationObserver {
 public:
  HeapGrowingImpl(GarbageCollector*, StatsCollector*,
                  cppgc::Heap::ResourceConstraints);
  ~HeapGrowingImpl();

  HeapGrowingImpl(const HeapGrowingImpl&) = delete;
  HeapGrowingImpl& operator=(const HeapGrowingImpl&) = delete;

  void AllocatedObjectSizeIncreased(size_t) final;
  // Only trigger GC on growing.
  void AllocatedObjectSizeDecreased(size_t) final {}
  void ResetAllocatedObjectSize(size_t) final;

  size_t limit() const { return limit_; }

 private:
  void ConfigureLimit(size_t allocated_object_size);

  GarbageCollector* collector_;
  StatsCollector* stats_collector_;
  // Allow 1 MB heap by default;
  size_t initial_heap_size_ = 1 * kMB;
  size_t limit_ = 0;  // See ConfigureLimit().

  SingleThreadedHandle gc_task_handle_;
};

HeapGrowing::HeapGrowingImpl::HeapGrowingImpl(
    GarbageCollector* collector, StatsCollector* stats_collector,
    cppgc::Heap::ResourceConstraints constraints)
    : collector_(collector),
      stats_collector_(stats_collector),
      gc_task_handle_(SingleThreadedHandle::NonEmptyTag{}) {
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
  if (stats_collector_->allocated_object_size() > limit_) {
    collector_->CollectGarbage(
        GarbageCollector::Config::ConservativeAtomicConfig());
  }
}

void HeapGrowing::HeapGrowingImpl::ResetAllocatedObjectSize(
    size_t allocated_object_size) {
  ConfigureLimit(allocated_object_size);
}

void HeapGrowing::HeapGrowingImpl::ConfigureLimit(
    size_t allocated_object_size) {
  const size_t size = std::max(allocated_object_size, initial_heap_size_);
  limit_ = std::max(static_cast<size_t>(size * kGrowingFactor),
                    size + kMinLimitIncrease);
}

HeapGrowing::HeapGrowing(GarbageCollector* collector,
                         StatsCollector* stats_collector,
                         cppgc::Heap::ResourceConstraints constraints)
    : impl_(std::make_unique<HeapGrowing::HeapGrowingImpl>(
          collector, stats_collector, constraints)) {}

HeapGrowing::~HeapGrowing() = default;

size_t HeapGrowing::limit() const { return impl_->limit(); }

// static
constexpr double HeapGrowing::kGrowingFactor;

}  // namespace internal
}  // namespace cppgc

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap-base.h"

#include "include/cppgc/heap-consistency.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/platform/platform.h"
#include "src/heap/base/stack.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-statistics-collector.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/marker.h"
#include "src/heap/cppgc/marking-verifier.h"
#include "src/heap/cppgc/page-memory.h"
#include "src/heap/cppgc/prefinalizer-handler.h"
#include "src/heap/cppgc/stats-collector.h"

namespace cppgc {
namespace internal {

namespace {

class ObjectSizeCounter : private HeapVisitor<ObjectSizeCounter> {
  friend class HeapVisitor<ObjectSizeCounter>;

 public:
  size_t GetSize(RawHeap* heap) {
    Traverse(heap);
    return accumulated_size_;
  }

 private:
  static size_t ObjectSize(const HeapObjectHeader* header) {
    const size_t size =
        header->IsLargeObject()
            ? static_cast<const LargePage*>(BasePage::FromPayload(header))
                  ->PayloadSize()
            : header->GetSize();
    DCHECK_GE(size, sizeof(HeapObjectHeader));
    return size - sizeof(HeapObjectHeader);
  }

  bool VisitHeapObjectHeader(HeapObjectHeader* header) {
    if (header->IsFree()) return true;
    accumulated_size_ += ObjectSize(header);
    return true;
  }

  size_t accumulated_size_ = 0;
};

}  // namespace

HeapBase::HeapBase(
    std::shared_ptr<cppgc::Platform> platform,
    const std::vector<std::unique_ptr<CustomSpaceBase>>& custom_spaces,
    StackSupport stack_support,
    std::unique_ptr<MetricRecorder> histogram_recorder)
    : raw_heap_(this, custom_spaces),
      platform_(std::move(platform)),
#if defined(CPPGC_CAGED_HEAP)
      caged_heap_(this, platform_->GetPageAllocator()),
      page_backend_(std::make_unique<PageBackend>(&caged_heap_.allocator())),
#else
      page_backend_(
          std::make_unique<PageBackend>(platform_->GetPageAllocator())),
#endif
      stats_collector_(std::make_unique<StatsCollector>(
          std::move(histogram_recorder), platform_.get())),
      stack_(std::make_unique<heap::base::Stack>(
          v8::base::Stack::GetStackStart())),
      prefinalizer_handler_(std::make_unique<PreFinalizerHandler>(*this)),
      compactor_(raw_heap_),
      object_allocator_(&raw_heap_, page_backend_.get(),
                        stats_collector_.get()),
      sweeper_(*this),
      stack_support_(stack_support) {
  stats_collector_->RegisterObserver(
      &allocation_observer_for_PROCESS_HEAP_STATISTICS_);
}

HeapBase::~HeapBase() = default;

size_t HeapBase::ObjectPayloadSize() const {
  return ObjectSizeCounter().GetSize(const_cast<RawHeap*>(&raw_heap()));
}

void HeapBase::AdvanceIncrementalGarbageCollectionOnAllocationIfNeeded() {
  if (marker_) marker_->AdvanceMarkingOnAllocation();
}
void HeapBase::ExecutePreFinalizers() {
  // Pre finalizers are forbidden from allocating objects.
  cppgc::subtle::DisallowGarbageCollectionScope no_gc_scope(*this);
  prefinalizer_handler_->InvokePreFinalizers();
}

void HeapBase::Terminate() {
  DCHECK(!IsMarking());
  CHECK(!in_disallow_gc_scope());

  sweeper().FinishIfRunning();

  constexpr size_t kMaxTerminationGCs = 20;
  size_t gc_count = 0;
  do {
    CHECK_LT(gc_count++, kMaxTerminationGCs);

    // Clear root sets.
    strong_persistent_region_.ClearAllUsedNodes();
    weak_persistent_region_.ClearAllUsedNodes();
    {
      PersistentRegionLock guard;
      strong_cross_thread_persistent_region_.ClearAllUsedNodes();
      weak_cross_thread_persistent_region_.ClearAllUsedNodes();
    }

    stats_collector()->NotifyMarkingStarted(
        GarbageCollector::Config::CollectionType::kMajor,
        GarbageCollector::Config::IsForcedGC::kForced);
    stats_collector()->NotifyMarkingCompleted(0);
    object_allocator().ResetLinearAllocationBuffers();
    ExecutePreFinalizers();
    sweeper().Start(
        {Sweeper::SweepingConfig::SweepingType::kAtomic,
         Sweeper::SweepingConfig::CompactableSpaceHandling::kSweep});
    sweeper().NotifyDoneIfNeeded();
  } while (strong_persistent_region_.NodesInUse() > 0);

  object_allocator().Terminate();
  disallow_gc_scope_++;

  CHECK_EQ(0u, strong_persistent_region_.NodesInUse());
  CHECK_EQ(0u, weak_persistent_region_.NodesInUse());
  CHECK_EQ(0u, strong_cross_thread_persistent_region_.NodesInUse());
  CHECK_EQ(0u, weak_cross_thread_persistent_region_.NodesInUse());
}

HeapStatistics HeapBase::CollectStatistics(
    HeapStatistics::DetailLevel detail_level) {
  if (detail_level == HeapStatistics::DetailLevel::kBrief) {
    return {stats_collector_->allocated_memory_size(),
            stats_collector_->allocated_object_size(),
            HeapStatistics::DetailLevel::kBrief,
            {}};
  }

  sweeper_.FinishIfRunning();
  object_allocator_.ResetLinearAllocationBuffers();
  return HeapStatisticsCollector().CollectStatistics(this);
}

}  // namespace internal
}  // namespace cppgc

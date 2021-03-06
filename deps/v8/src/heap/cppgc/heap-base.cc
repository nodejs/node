// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap-base.h"

#include "src/base/bounded-page-allocator.h"
#include "src/base/platform/platform.h"
#include "src/heap/base/stack.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
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
    StackSupport stack_support)
    : raw_heap_(this, custom_spaces),
      platform_(std::move(platform)),
#if defined(CPPGC_CAGED_HEAP)
      caged_heap_(this, platform_->GetPageAllocator()),
      page_backend_(std::make_unique<PageBackend>(&caged_heap_.allocator())),
#else
      page_backend_(
          std::make_unique<PageBackend>(platform_->GetPageAllocator())),
#endif
      stats_collector_(std::make_unique<StatsCollector>()),
      stack_(std::make_unique<heap::base::Stack>(
          v8::base::Stack::GetStackStart())),
      prefinalizer_handler_(std::make_unique<PreFinalizerHandler>(*this)),
      compactor_(raw_heap_),
      object_allocator_(&raw_heap_, page_backend_.get(),
                        stats_collector_.get()),
      sweeper_(&raw_heap_, platform_.get(), stats_collector_.get()),
      stack_support_(stack_support) {
}

HeapBase::~HeapBase() = default;

size_t HeapBase::ObjectPayloadSize() const {
  return ObjectSizeCounter().GetSize(const_cast<RawHeap*>(&raw_heap()));
}

HeapBase::NoGCScope::NoGCScope(HeapBase& heap) : heap_(heap) {
  heap_.no_gc_scope_++;
}

HeapBase::NoGCScope::~NoGCScope() { heap_.no_gc_scope_--; }

void HeapBase::AdvanceIncrementalGarbageCollectionOnAllocationIfNeeded() {
  if (marker_) marker_->AdvanceMarkingOnAllocation();
}

}  // namespace internal
}  // namespace cppgc

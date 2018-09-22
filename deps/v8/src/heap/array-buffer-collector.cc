// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/array-buffer-collector.h"

#include "src/base/template-utils.h"
#include "src/heap/array-buffer-tracker.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {

namespace {

void FreeAllocationsHelper(
    Heap* heap, const std::vector<JSArrayBuffer::Allocation>& allocations) {
  for (JSArrayBuffer::Allocation alloc : allocations) {
    JSArrayBuffer::FreeBackingStore(heap->isolate(), alloc);
  }
}

}  // namespace

void ArrayBufferCollector::QueueOrFreeGarbageAllocations(
    std::vector<JSArrayBuffer::Allocation> allocations) {
  if (heap_->ShouldReduceMemory()) {
    FreeAllocationsHelper(heap_, allocations);
  } else {
    base::LockGuard<base::Mutex> guard(&allocations_mutex_);
    allocations_.push_back(std::move(allocations));
  }
}

void ArrayBufferCollector::PerformFreeAllocations() {
  base::LockGuard<base::Mutex> guard(&allocations_mutex_);
  for (const std::vector<JSArrayBuffer::Allocation>& allocations :
       allocations_) {
    FreeAllocationsHelper(heap_, allocations);
  }
  allocations_.clear();
}

class ArrayBufferCollector::FreeingTask final : public CancelableTask {
 public:
  explicit FreeingTask(Heap* heap)
      : CancelableTask(heap->isolate()), heap_(heap) {}

  ~FreeingTask() override = default;

 private:
  void RunInternal() final {
    TRACE_BACKGROUND_GC(
        heap_->tracer(),
        GCTracer::BackgroundScope::BACKGROUND_ARRAY_BUFFER_FREE);
    heap_->array_buffer_collector()->PerformFreeAllocations();
  }

  Heap* heap_;
};

void ArrayBufferCollector::FreeAllocations() {
  // TODO(wez): Remove backing-store from external memory accounting.
  heap_->account_external_memory_concurrently_freed();
  if (!heap_->IsTearingDown() && !heap_->ShouldReduceMemory() &&
      FLAG_concurrent_array_buffer_freeing) {
    V8::GetCurrentPlatform()->CallOnWorkerThread(
        base::make_unique<FreeingTask>(heap_));
  } else {
    // Fallback for when concurrency is disabled/restricted. This is e.g. the
    // case when the GC should reduce memory. For such GCs the
    // QueueOrFreeGarbageAllocations() call would immediately free the
    // allocations and this call would free already queued ones.
    PerformFreeAllocations();
  }
}

}  // namespace internal
}  // namespace v8

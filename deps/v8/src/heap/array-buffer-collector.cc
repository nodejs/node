// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/array-buffer-collector.h"

#include "src/base/template-utils.h"
#include "src/heap/array-buffer-tracker.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/tasks/cancelable-task.h"
#include "src/tasks/task-utils.h"

namespace v8 {
namespace internal {

void ArrayBufferCollector::QueueOrFreeGarbageAllocations(
    std::vector<std::shared_ptr<BackingStore>> backing_stores) {
  if (heap_->ShouldReduceMemory()) {
    // Destruct the vector, which destructs the std::shared_ptrs, freeing
    // the backing stores.
    backing_stores.clear();
  } else {
    base::MutexGuard guard(&allocations_mutex_);
    allocations_.push_back(std::move(backing_stores));
  }
}

void ArrayBufferCollector::PerformFreeAllocations() {
  base::MutexGuard guard(&allocations_mutex_);
  // Destruct the vector, which destructs the vecotr of std::shared_ptrs,
  // freeing the backing stores if their refcount drops to zero.
  allocations_.clear();
}

void ArrayBufferCollector::FreeAllocations() {
  // TODO(wez): Remove backing-store from external memory accounting.
  heap_->account_external_memory_concurrently_freed();
  if (!heap_->IsTearingDown() && !heap_->ShouldReduceMemory() &&
      FLAG_concurrent_array_buffer_freeing) {
    V8::GetCurrentPlatform()->CallOnWorkerThread(
        MakeCancelableTask(heap_->isolate(), [this] {
          TRACE_BACKGROUND_GC(
              heap_->tracer(),
              GCTracer::BackgroundScope::BACKGROUND_ARRAY_BUFFER_FREE);
          PerformFreeAllocations();
        }));
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

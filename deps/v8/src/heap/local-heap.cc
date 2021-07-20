// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/local-heap.h"

#include <atomic>
#include <memory>

#include "src/base/logging.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/handles/local-handles.h"
#include "src/heap/collection-barrier.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/marking-barrier.h"
#include "src/heap/parked-scope.h"
#include "src/heap/safepoint.h"

namespace v8 {
namespace internal {

namespace {
thread_local LocalHeap* current_local_heap = nullptr;
}  // namespace

LocalHeap* LocalHeap::Current() { return current_local_heap; }

#ifdef DEBUG
void LocalHeap::VerifyCurrent() {
  LocalHeap* current = LocalHeap::Current();

  if (is_main_thread())
    DCHECK_NULL(current);
  else
    DCHECK_EQ(current, this);
}
#endif

LocalHeap::LocalHeap(Heap* heap, ThreadKind kind,
                     std::unique_ptr<PersistentHandles> persistent_handles)
    : heap_(heap),
      is_main_thread_(kind == ThreadKind::kMain),
      state_(kParked),
      allocation_failed_(false),
      main_thread_parked_(false),
      prev_(nullptr),
      next_(nullptr),
      handles_(new LocalHandles),
      persistent_handles_(std::move(persistent_handles)),
      marking_barrier_(new MarkingBarrier(this)),
      old_space_allocator_(this, heap->old_space()) {
  heap_->safepoint()->AddLocalHeap(this, [this] {
    if (!is_main_thread()) {
      WriteBarrier::SetForThread(marking_barrier_.get());
      if (heap_->incremental_marking()->IsMarking()) {
        marking_barrier_->Activate(
            heap_->incremental_marking()->IsCompacting());
      }
    }
  });

  if (persistent_handles_) {
    persistent_handles_->Attach(this);
  }
  DCHECK_NULL(current_local_heap);
  if (!is_main_thread()) current_local_heap = this;
}

LocalHeap::~LocalHeap() {
  // Park thread since removing the local heap could block.
  EnsureParkedBeforeDestruction();

  heap_->safepoint()->RemoveLocalHeap(this, [this] {
    old_space_allocator_.FreeLinearAllocationArea();

    if (!is_main_thread()) {
      marking_barrier_->Publish();
      WriteBarrier::ClearForThread(marking_barrier_.get());
    }
  });

  if (!is_main_thread()) {
    DCHECK_EQ(current_local_heap, this);
    current_local_heap = nullptr;
  }

  DCHECK(gc_epilogue_callbacks_.empty());
}

void LocalHeap::EnsurePersistentHandles() {
  if (!persistent_handles_) {
    persistent_handles_.reset(
        heap_->isolate()->NewPersistentHandles().release());
    persistent_handles_->Attach(this);
  }
}

void LocalHeap::AttachPersistentHandles(
    std::unique_ptr<PersistentHandles> persistent_handles) {
  DCHECK_NULL(persistent_handles_);
  persistent_handles_ = std::move(persistent_handles);
  persistent_handles_->Attach(this);
}

std::unique_ptr<PersistentHandles> LocalHeap::DetachPersistentHandles() {
  if (persistent_handles_) persistent_handles_->Detach();
  return std::move(persistent_handles_);
}

#ifdef DEBUG
bool LocalHeap::ContainsPersistentHandle(Address* location) {
  return persistent_handles_ ? persistent_handles_->Contains(location) : false;
}

bool LocalHeap::ContainsLocalHandle(Address* location) {
  return handles_ ? handles_->Contains(location) : false;
}

bool LocalHeap::IsHandleDereferenceAllowed() {
#ifdef DEBUG
  VerifyCurrent();
#endif
  ThreadState state = state_relaxed();
  return state == kRunning || state == kSafepointRequested;
}
#endif

bool LocalHeap::IsParked() {
#ifdef DEBUG
  VerifyCurrent();
#endif
  ThreadState state = state_relaxed();
  return state == kParked || state == kParkedSafepointRequested;
}

void LocalHeap::ParkSlowPath(ThreadState current_state) {
  if (is_main_thread()) {
    while (true) {
      CHECK_EQ(current_state, kSafepointRequested);
      heap_->CollectGarbageForBackground(this);

      current_state = kRunning;
      if (state_.compare_exchange_strong(current_state, kParked)) {
        return;
      }
    }
  } else {
    CHECK_EQ(current_state, kSafepointRequested);
    CHECK(state_.compare_exchange_strong(current_state,
                                         kParkedSafepointRequested));
    heap_->safepoint()->NotifyPark();
  }
}

void LocalHeap::UnparkSlowPath() {
  if (is_main_thread()) {
    ThreadState expected = kParkedSafepointRequested;
    CHECK(state_.compare_exchange_strong(expected, kSafepointRequested));
    heap_->CollectGarbageForBackground(this);
  } else {
    while (true) {
      ThreadState expected = kParked;
      if (!state_.compare_exchange_strong(expected, kRunning)) {
        CHECK_EQ(expected, kParkedSafepointRequested);
        TRACE_GC1(heap_->tracer(), GCTracer::Scope::BACKGROUND_UNPARK,
                  ThreadKind::kBackground);
        heap_->safepoint()->WaitInUnpark();
      } else {
        return;
      }
    }
  }
}

void LocalHeap::EnsureParkedBeforeDestruction() {
  DCHECK_IMPLIES(!is_main_thread(), IsParked());
}

void LocalHeap::SafepointSlowPath() {
  if (is_main_thread()) {
    CHECK_EQ(kSafepointRequested, state_relaxed());
    heap_->CollectGarbageForBackground(this);
  } else {
    TRACE_GC1(heap_->tracer(), GCTracer::Scope::BACKGROUND_SAFEPOINT,
              ThreadKind::kBackground);
    ThreadState expected = kSafepointRequested;
    CHECK(state_.compare_exchange_strong(expected, kSafepoint));
    heap_->safepoint()->WaitInSafepoint();
    // This might be a bit surprising, GlobalSafepoint transitions the state
    // from Safepoint (--> Running) --> Parked when returning from the
    // safepoint.
    Unpark();
  }
}

void LocalHeap::FreeLinearAllocationArea() {
  old_space_allocator_.FreeLinearAllocationArea();
}

void LocalHeap::MakeLinearAllocationAreaIterable() {
  old_space_allocator_.MakeLinearAllocationAreaIterable();
}

void LocalHeap::MarkLinearAllocationAreaBlack() {
  old_space_allocator_.MarkLinearAllocationAreaBlack();
}

void LocalHeap::UnmarkLinearAllocationArea() {
  old_space_allocator_.UnmarkLinearAllocationArea();
}

bool LocalHeap::TryPerformCollection() {
  if (is_main_thread()) {
    heap_->CollectGarbageForBackground(this);
    return true;
  } else {
    heap_->collection_barrier_->RequestGC();

    LocalHeap* main_thread = heap_->main_thread_local_heap();
    ThreadState current = main_thread->state_relaxed();

    while (true) {
      switch (current) {
        case kRunning:
          if (main_thread->state_.compare_exchange_strong(
                  current, kSafepointRequested)) {
            return heap_->collection_barrier_->AwaitCollectionBackground(this);
          }
          break;

        case kSafepointRequested:
          return heap_->collection_barrier_->AwaitCollectionBackground(this);

        case kParked:
          if (main_thread->state_.compare_exchange_strong(
                  current, kParkedSafepointRequested)) {
            return false;
          }
          break;

        case kParkedSafepointRequested:
          return false;

        case kSafepoint:
          UNREACHABLE();
      }
    }
  }
}

Address LocalHeap::PerformCollectionAndAllocateAgain(
    int object_size, AllocationType type, AllocationOrigin origin,
    AllocationAlignment alignment) {
  CHECK(!allocation_failed_);
  CHECK(!main_thread_parked_);
  allocation_failed_ = true;
  static const int kMaxNumberOfRetries = 3;

  for (int i = 0; i < kMaxNumberOfRetries; i++) {
    if (!TryPerformCollection()) {
      main_thread_parked_ = true;
    }

    AllocationResult result = AllocateRaw(object_size, type, origin, alignment);

    if (!result.IsRetry()) {
      allocation_failed_ = false;
      main_thread_parked_ = false;
      return result.ToObjectChecked().address();
    }
  }

  heap_->FatalProcessOutOfMemory("LocalHeap: allocation failed");
}

void LocalHeap::AddGCEpilogueCallback(GCEpilogueCallback* callback,
                                      void* data) {
  DCHECK(!IsParked());
  std::pair<GCEpilogueCallback*, void*> callback_and_data(callback, data);
  DCHECK_EQ(std::find(gc_epilogue_callbacks_.begin(),
                      gc_epilogue_callbacks_.end(), callback_and_data),
            gc_epilogue_callbacks_.end());
  gc_epilogue_callbacks_.push_back(callback_and_data);
}

void LocalHeap::RemoveGCEpilogueCallback(GCEpilogueCallback* callback,
                                         void* data) {
  DCHECK(!IsParked());
  std::pair<GCEpilogueCallback*, void*> callback_and_data(callback, data);
  auto it = std::find(gc_epilogue_callbacks_.begin(),
                      gc_epilogue_callbacks_.end(), callback_and_data);
  *it = gc_epilogue_callbacks_.back();
  gc_epilogue_callbacks_.pop_back();
}

void LocalHeap::InvokeGCEpilogueCallbacksInSafepoint() {
  for (auto callback_and_data : gc_epilogue_callbacks_) {
    callback_and_data.first(callback_and_data.second);
  }
}

}  // namespace internal
}  // namespace v8

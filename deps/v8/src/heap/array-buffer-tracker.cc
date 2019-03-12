// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/array-buffer-tracker.h"

#include <vector>

#include "src/heap/array-buffer-collector.h"
#include "src/heap/array-buffer-tracker-inl.h"
#include "src/heap/heap.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

LocalArrayBufferTracker::~LocalArrayBufferTracker() {
  CHECK(array_buffers_.empty());
}

template <typename Callback>
void LocalArrayBufferTracker::Process(Callback callback) {
  std::vector<JSArrayBuffer::Allocation> backing_stores_to_free;
  TrackingData kept_array_buffers;

  JSArrayBuffer new_buffer;
  JSArrayBuffer old_buffer;
  size_t freed_memory = 0;
  for (TrackingData::iterator it = array_buffers_.begin();
       it != array_buffers_.end(); ++it) {
    old_buffer = it->first;
    DCHECK_EQ(page_, Page::FromHeapObject(old_buffer));
    const CallbackResult result = callback(old_buffer, &new_buffer);
    if (result == kKeepEntry) {
      kept_array_buffers.insert(*it);
    } else if (result == kUpdateEntry) {
      DCHECK(!new_buffer.is_null());
      Page* target_page = Page::FromHeapObject(new_buffer);
      {
        base::MutexGuard guard(target_page->mutex());
        LocalArrayBufferTracker* tracker = target_page->local_tracker();
        if (tracker == nullptr) {
          target_page->AllocateLocalTracker();
          tracker = target_page->local_tracker();
        }
        DCHECK_NOT_NULL(tracker);
        const size_t length = it->second.length;
        // We should decrement before adding to avoid potential overflows in
        // the external memory counters.
        DCHECK_EQ(it->first->is_wasm_memory(), it->second.is_wasm_memory);
        tracker->AddInternal(new_buffer, length);
        MemoryChunk::MoveExternalBackingStoreBytes(
            ExternalBackingStoreType::kArrayBuffer,
            static_cast<MemoryChunk*>(page_),
            static_cast<MemoryChunk*>(target_page), length);
      }
    } else if (result == kRemoveEntry) {
      freed_memory += it->second.length;
      // We pass backing_store() and stored length to the collector for freeing
      // the backing store. Wasm allocations will go through their own tracker
      // based on the backing store.
      backing_stores_to_free.push_back(it->second);
    } else {
      UNREACHABLE();
    }
  }
  if (freed_memory) {
    page_->DecrementExternalBackingStoreBytes(
        ExternalBackingStoreType::kArrayBuffer, freed_memory);
    // TODO(wez): Remove backing-store from external memory accounting.
    page_->heap()->update_external_memory_concurrently_freed(
        static_cast<intptr_t>(freed_memory));
  }

  array_buffers_.swap(kept_array_buffers);

  // Pass the backing stores that need to be freed to the main thread for
  // potential later distribution.
  page_->heap()->array_buffer_collector()->QueueOrFreeGarbageAllocations(
      std::move(backing_stores_to_free));
}

void ArrayBufferTracker::PrepareToFreeDeadInNewSpace(Heap* heap) {
  DCHECK_EQ(heap->gc_state(), Heap::HeapState::SCAVENGE);
  for (Page* page :
       PageRange(heap->new_space()->from_space().first_page(), nullptr)) {
    bool empty = ProcessBuffers(page, kUpdateForwardedRemoveOthers);
    CHECK(empty);
  }
}

void ArrayBufferTracker::FreeAll(Page* page) {
  LocalArrayBufferTracker* tracker = page->local_tracker();
  if (tracker == nullptr) return;
  tracker->Free([](JSArrayBuffer buffer) { return true; });
  if (tracker->IsEmpty()) {
    page->ReleaseLocalTracker();
  }
}

bool ArrayBufferTracker::ProcessBuffers(Page* page, ProcessingMode mode) {
  LocalArrayBufferTracker* tracker = page->local_tracker();
  if (tracker == nullptr) return true;

  DCHECK(page->SweepingDone());
  tracker->Process([mode](JSArrayBuffer old_buffer, JSArrayBuffer* new_buffer) {
    MapWord map_word = old_buffer->map_word();
    if (map_word.IsForwardingAddress()) {
      *new_buffer = JSArrayBuffer::cast(map_word.ToForwardingAddress());
      return LocalArrayBufferTracker::kUpdateEntry;
    }
    return mode == kUpdateForwardedKeepOthers
               ? LocalArrayBufferTracker::kKeepEntry
               : LocalArrayBufferTracker::kRemoveEntry;
  });
  return tracker->IsEmpty();
}

bool ArrayBufferTracker::IsTracked(JSArrayBuffer buffer) {
  Page* page = Page::FromHeapObject(buffer);
  {
    base::MutexGuard guard(page->mutex());
    LocalArrayBufferTracker* tracker = page->local_tracker();
    if (tracker == nullptr) return false;
    return tracker->IsTracked(buffer);
  }
}

void ArrayBufferTracker::TearDown(Heap* heap) {
  // ArrayBuffers can only be found in NEW_SPACE and OLD_SPACE.
  for (Page* p : *heap->old_space()) {
    FreeAll(p);
  }
  NewSpace* new_space = heap->new_space();
  if (new_space->to_space().is_committed()) {
    for (Page* p : new_space->to_space()) {
      FreeAll(p);
    }
  }
#ifdef DEBUG
  if (new_space->from_space().is_committed()) {
    for (Page* p : new_space->from_space()) {
      DCHECK(!p->contains_array_buffers());
    }
  }
#endif  // DEBUG
}

}  // namespace internal
}  // namespace v8

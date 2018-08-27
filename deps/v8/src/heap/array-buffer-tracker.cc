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
  std::vector<JSArrayBuffer::Allocation>* backing_stores_to_free =
      new std::vector<JSArrayBuffer::Allocation>();

  JSArrayBuffer* new_buffer = nullptr;
  JSArrayBuffer* old_buffer = nullptr;
  size_t freed_memory = 0;
  size_t moved_memory = 0;
  for (TrackingData::iterator it = array_buffers_.begin();
       it != array_buffers_.end();) {
    old_buffer = reinterpret_cast<JSArrayBuffer*>(it->first);
    const CallbackResult result = callback(old_buffer, &new_buffer);
    if (result == kKeepEntry) {
      ++it;
    } else if (result == kUpdateEntry) {
      DCHECK_NOT_NULL(new_buffer);
      Page* target_page = Page::FromAddress(new_buffer->address());
      {
        base::LockGuard<base::Mutex> guard(target_page->mutex());
        LocalArrayBufferTracker* tracker = target_page->local_tracker();
        if (tracker == nullptr) {
          target_page->AllocateLocalTracker();
          tracker = target_page->local_tracker();
        }
        DCHECK_NOT_NULL(tracker);
        const size_t size = NumberToSize(new_buffer->byte_length());
        tracker->Add(new_buffer, size);
      }
      moved_memory += it->second;
      it = array_buffers_.erase(it);
    } else if (result == kRemoveEntry) {
      freed_memory += it->second;
      // We pass backing_store() and stored length to the collector for freeing
      // the backing store. Wasm allocations will go through their own tracker
      // based on the backing store.
      backing_stores_to_free->emplace_back(
          old_buffer->backing_store(), it->second, old_buffer->backing_store(),
          old_buffer->allocation_mode(), old_buffer->is_wasm_memory());
      it = array_buffers_.erase(it);
    } else {
      UNREACHABLE();
    }
  }
  if (moved_memory || freed_memory) {
    // Update the Space with any moved or freed backing-store bytes.
    space_->DecrementExternalBackingStoreBytes(freed_memory + moved_memory);

    // TODO(wez): Remove backing-store from external memory accounting.
    space_->heap()->update_external_memory_concurrently_freed(
        static_cast<intptr_t>(freed_memory));
  }

  // Pass the backing stores that need to be freed to the main thread for later
  // distribution.
  // ArrayBufferCollector takes ownership of this pointer.
  space_->heap()->array_buffer_collector()->AddGarbageAllocations(
      backing_stores_to_free);
}

void ArrayBufferTracker::PrepareToFreeDeadInNewSpace(Heap* heap) {
  DCHECK_EQ(heap->gc_state(), Heap::HeapState::SCAVENGE);
  for (Page* page : PageRange(heap->new_space()->FromSpaceStart(),
                              heap->new_space()->FromSpaceEnd())) {
    bool empty = ProcessBuffers(page, kUpdateForwardedRemoveOthers);
    CHECK(empty);
  }
}

void ArrayBufferTracker::FreeAll(Page* page) {
  LocalArrayBufferTracker* tracker = page->local_tracker();
  if (tracker == nullptr) return;
  tracker->Free([](JSArrayBuffer* buffer) { return true; });
  if (tracker->IsEmpty()) {
    page->ReleaseLocalTracker();
  }
}

bool ArrayBufferTracker::ProcessBuffers(Page* page, ProcessingMode mode) {
  LocalArrayBufferTracker* tracker = page->local_tracker();
  if (tracker == nullptr) return true;

  DCHECK(page->SweepingDone());
  tracker->Process(
      [mode](JSArrayBuffer* old_buffer, JSArrayBuffer** new_buffer) {
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

bool ArrayBufferTracker::IsTracked(JSArrayBuffer* buffer) {
  Page* page = Page::FromAddress(buffer->address());
  {
    base::LockGuard<base::Mutex> guard(page->mutex());
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

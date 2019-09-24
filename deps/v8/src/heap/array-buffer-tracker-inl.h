// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_ARRAY_BUFFER_TRACKER_INL_H_
#define V8_HEAP_ARRAY_BUFFER_TRACKER_INL_H_

#include "src/heap/array-buffer-tracker.h"
#include "src/heap/heap-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

void ArrayBufferTracker::RegisterNew(Heap* heap, JSArrayBuffer buffer) {
  if (buffer.backing_store() == nullptr) return;

  // ArrayBuffer tracking works only for small objects.
  DCHECK(!heap->IsLargeObject(buffer));

  const size_t length = buffer.byte_length();
  Page* page = Page::FromHeapObject(buffer);
  {
    base::MutexGuard guard(page->mutex());
    LocalArrayBufferTracker* tracker = page->local_tracker();
    if (tracker == nullptr) {
      page->AllocateLocalTracker();
      tracker = page->local_tracker();
    }
    DCHECK_NOT_NULL(tracker);
    tracker->Add(buffer, length);
  }

  // TODO(wez): Remove backing-store from external memory accounting.
  // We may go over the limit of externally allocated memory here. We call the
  // api function to trigger a GC in this case.
  reinterpret_cast<v8::Isolate*>(heap->isolate())
      ->AdjustAmountOfExternalAllocatedMemory(length);
}

void ArrayBufferTracker::Unregister(Heap* heap, JSArrayBuffer buffer) {
  if (buffer.backing_store() == nullptr) return;

  Page* page = Page::FromHeapObject(buffer);
  const size_t length = buffer.byte_length();
  {
    base::MutexGuard guard(page->mutex());
    LocalArrayBufferTracker* tracker = page->local_tracker();
    DCHECK_NOT_NULL(tracker);
    tracker->Remove(buffer, length);
  }

  // TODO(wez): Remove backing-store from external memory accounting.
  heap->update_external_memory(-static_cast<intptr_t>(length));
}

template <typename Callback>
void LocalArrayBufferTracker::Free(Callback should_free) {
  size_t freed_memory = 0;
  Isolate* isolate = page_->heap()->isolate();
  for (TrackingData::iterator it = array_buffers_.begin();
       it != array_buffers_.end();) {
    // Unchecked cast because the map might already be dead at this point.
    JSArrayBuffer buffer = JSArrayBuffer::unchecked_cast(it->first);
    const size_t length = it->second.length;

    if (should_free(buffer)) {
      JSArrayBuffer::FreeBackingStore(isolate, it->second);
      it = array_buffers_.erase(it);
      freed_memory += length;
    } else {
      ++it;
    }
  }
  if (freed_memory > 0) {
    page_->DecrementExternalBackingStoreBytes(
        ExternalBackingStoreType::kArrayBuffer, freed_memory);

    // TODO(wez): Remove backing-store from external memory accounting.
    page_->heap()->update_external_memory_concurrently_freed(freed_memory);
  }
}

template <typename MarkingState>
void ArrayBufferTracker::FreeDead(Page* page, MarkingState* marking_state) {
  // Callers need to ensure having the page lock.
  LocalArrayBufferTracker* tracker = page->local_tracker();
  if (tracker == nullptr) return;
  tracker->Free([marking_state](JSArrayBuffer buffer) {
    return marking_state->IsWhite(buffer);
  });
  if (tracker->IsEmpty()) {
    page->ReleaseLocalTracker();
  }
}

void LocalArrayBufferTracker::Add(JSArrayBuffer buffer, size_t length) {
  page_->IncrementExternalBackingStoreBytes(
      ExternalBackingStoreType::kArrayBuffer, length);

  AddInternal(buffer, length);
}

void LocalArrayBufferTracker::AddInternal(JSArrayBuffer buffer, size_t length) {
  auto ret = array_buffers_.insert(
      {buffer,
       {buffer.backing_store(), length, buffer.backing_store(),
        buffer.is_wasm_memory()}});
  USE(ret);
  // Check that we indeed inserted a new value and did not overwrite an existing
  // one (which would be a bug).
  DCHECK(ret.second);
}

void LocalArrayBufferTracker::Remove(JSArrayBuffer buffer, size_t length) {
  page_->DecrementExternalBackingStoreBytes(
      ExternalBackingStoreType::kArrayBuffer, length);

  TrackingData::iterator it = array_buffers_.find(buffer);
  // Check that we indeed find a key to remove.
  DCHECK(it != array_buffers_.end());
  DCHECK_EQ(length, it->second.length);
  array_buffers_.erase(it);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_ARRAY_BUFFER_TRACKER_INL_H_

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/conversions-inl.h"
#include "src/heap/array-buffer-tracker.h"
#include "src/heap/heap.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

void ArrayBufferTracker::RegisterNew(Heap* heap, JSArrayBuffer* buffer) {
  void* data = buffer->backing_store();
  if (!data) return;

  size_t length = NumberToSize(buffer->byte_length());
  Page* page = Page::FromAddress(buffer->address());
  {
    base::LockGuard<base::Mutex> guard(page->mutex());
    LocalArrayBufferTracker* tracker = page->local_tracker();
    if (tracker == nullptr) {
      page->AllocateLocalTracker();
      tracker = page->local_tracker();
    }
    DCHECK_NOT_NULL(tracker);
    tracker->Add(buffer, length);
  }
  // We may go over the limit of externally allocated memory here. We call the
  // api function to trigger a GC in this case.
  reinterpret_cast<v8::Isolate*>(heap->isolate())
      ->AdjustAmountOfExternalAllocatedMemory(length);
}

void ArrayBufferTracker::Unregister(Heap* heap, JSArrayBuffer* buffer) {
  void* data = buffer->backing_store();
  if (!data) return;

  Page* page = Page::FromAddress(buffer->address());
  size_t length = 0;
  {
    base::LockGuard<base::Mutex> guard(page->mutex());
    LocalArrayBufferTracker* tracker = page->local_tracker();
    DCHECK_NOT_NULL(tracker);
    length = tracker->Remove(buffer);
  }
  heap->update_external_memory(-static_cast<intptr_t>(length));
}

void LocalArrayBufferTracker::Add(Key key, const Value& value) {
  auto ret = array_buffers_.insert(std::make_pair(key, value));
  USE(ret);
  // Check that we indeed inserted a new value and did not overwrite an existing
  // one (which would be a bug).
  DCHECK(ret.second);
}

LocalArrayBufferTracker::Value LocalArrayBufferTracker::Remove(Key key) {
  TrackingData::iterator it = array_buffers_.find(key);
  // Check that we indeed find a key to remove.
  DCHECK(it != array_buffers_.end());
  Value value = it->second;
  array_buffers_.erase(it);
  return value;
}

}  // namespace internal
}  // namespace v8

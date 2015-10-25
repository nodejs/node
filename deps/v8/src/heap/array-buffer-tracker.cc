// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/array-buffer-tracker.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/objects-inl.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

ArrayBufferTracker::~ArrayBufferTracker() {
  Isolate* isolate = heap()->isolate();
  size_t freed_memory = 0;
  for (auto& buffer : live_array_buffers_) {
    isolate->array_buffer_allocator()->Free(buffer.first, buffer.second);
    freed_memory += buffer.second;
  }
  for (auto& buffer : live_array_buffers_for_scavenge_) {
    isolate->array_buffer_allocator()->Free(buffer.first, buffer.second);
    freed_memory += buffer.second;
  }
  live_array_buffers_.clear();
  live_array_buffers_for_scavenge_.clear();
  not_yet_discovered_array_buffers_.clear();
  not_yet_discovered_array_buffers_for_scavenge_.clear();

  if (freed_memory > 0) {
    heap()->update_amount_of_external_allocated_memory(
        -static_cast<int64_t>(freed_memory));
  }
}


void ArrayBufferTracker::RegisterNew(JSArrayBuffer* buffer) {
  void* data = buffer->backing_store();
  if (!data) return;

  bool in_new_space = heap()->InNewSpace(buffer);
  size_t length = NumberToSize(heap()->isolate(), buffer->byte_length());
  if (in_new_space) {
    live_array_buffers_for_scavenge_[data] = length;
  } else {
    live_array_buffers_[data] = length;
  }

  // We may go over the limit of externally allocated memory here. We call the
  // api function to trigger a GC in this case.
  reinterpret_cast<v8::Isolate*>(heap()->isolate())
      ->AdjustAmountOfExternalAllocatedMemory(length);
}


void ArrayBufferTracker::Unregister(JSArrayBuffer* buffer) {
  void* data = buffer->backing_store();
  if (!data) return;

  bool in_new_space = heap()->InNewSpace(buffer);
  std::map<void*, size_t>* live_buffers =
      in_new_space ? &live_array_buffers_for_scavenge_ : &live_array_buffers_;
  std::map<void*, size_t>* not_yet_discovered_buffers =
      in_new_space ? &not_yet_discovered_array_buffers_for_scavenge_
                   : &not_yet_discovered_array_buffers_;

  DCHECK(live_buffers->count(data) > 0);

  size_t length = (*live_buffers)[data];
  live_buffers->erase(data);
  not_yet_discovered_buffers->erase(data);

  heap()->update_amount_of_external_allocated_memory(
      -static_cast<int64_t>(length));
}


void ArrayBufferTracker::MarkLive(JSArrayBuffer* buffer) {
  void* data = buffer->backing_store();

  // ArrayBuffer might be in the middle of being constructed.
  if (data == heap()->undefined_value()) return;
  if (heap()->InNewSpace(buffer)) {
    not_yet_discovered_array_buffers_for_scavenge_.erase(data);
  } else {
    not_yet_discovered_array_buffers_.erase(data);
  }
}


void ArrayBufferTracker::FreeDead(bool from_scavenge) {
  size_t freed_memory = 0;
  Isolate* isolate = heap()->isolate();
  for (auto& buffer : not_yet_discovered_array_buffers_for_scavenge_) {
    isolate->array_buffer_allocator()->Free(buffer.first, buffer.second);
    freed_memory += buffer.second;
    live_array_buffers_for_scavenge_.erase(buffer.first);
  }

  if (!from_scavenge) {
    for (auto& buffer : not_yet_discovered_array_buffers_) {
      isolate->array_buffer_allocator()->Free(buffer.first, buffer.second);
      freed_memory += buffer.second;
      live_array_buffers_.erase(buffer.first);
    }
  }

  not_yet_discovered_array_buffers_for_scavenge_ =
      live_array_buffers_for_scavenge_;
  if (!from_scavenge) not_yet_discovered_array_buffers_ = live_array_buffers_;

  // Do not call through the api as this code is triggered while doing a GC.
  heap()->update_amount_of_external_allocated_memory(
      -static_cast<int64_t>(freed_memory));
}


void ArrayBufferTracker::PrepareDiscoveryInNewSpace() {
  not_yet_discovered_array_buffers_for_scavenge_ =
      live_array_buffers_for_scavenge_;
}


void ArrayBufferTracker::Promote(JSArrayBuffer* buffer) {
  if (buffer->is_external()) return;
  void* data = buffer->backing_store();
  if (!data) return;
  // ArrayBuffer might be in the middle of being constructed.
  if (data == heap()->undefined_value()) return;
  DCHECK(live_array_buffers_for_scavenge_.count(data) > 0);
  live_array_buffers_[data] = live_array_buffers_for_scavenge_[data];
  live_array_buffers_for_scavenge_.erase(data);
  not_yet_discovered_array_buffers_for_scavenge_.erase(data);
}

}  // namespace internal
}  // namespace v8

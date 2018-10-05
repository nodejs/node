// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/store-buffer.h"

#include <algorithm>

#include "src/base/macros.h"
#include "src/base/template-utils.h"
#include "src/counters.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/store-buffer-inl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

StoreBuffer::StoreBuffer(Heap* heap)
    : heap_(heap), top_(nullptr), current_(0), mode_(NOT_IN_GC) {
  for (int i = 0; i < kStoreBuffers; i++) {
    start_[i] = nullptr;
    limit_[i] = nullptr;
    lazy_top_[i] = nullptr;
  }
  task_running_ = false;
  insertion_callback = &InsertDuringRuntime;
  deletion_callback = &DeleteDuringRuntime;
}

void StoreBuffer::SetUp() {
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  const size_t requested_size = kStoreBufferSize * kStoreBuffers;
  // Allocate buffer memory aligned at least to kStoreBufferSize. This lets us
  // use a bit test to detect the ends of the buffers.
  const size_t alignment =
      std::max<size_t>(kStoreBufferSize, page_allocator->AllocatePageSize());
  void* hint = AlignedAddress(heap_->GetRandomMmapAddr(), alignment);
  VirtualMemory reservation(page_allocator, requested_size, hint, alignment);
  if (!reservation.IsReserved()) {
    heap_->FatalProcessOutOfMemory("StoreBuffer::SetUp");
  }

  Address start = reservation.address();
  const size_t allocated_size = reservation.size();

  start_[0] = reinterpret_cast<Address*>(start);
  limit_[0] = start_[0] + (kStoreBufferSize / kPointerSize);
  start_[1] = limit_[0];
  limit_[1] = start_[1] + (kStoreBufferSize / kPointerSize);

  // Sanity check the buffers.
  Address* vm_limit = reinterpret_cast<Address*>(start + allocated_size);
  USE(vm_limit);
  for (int i = 0; i < kStoreBuffers; i++) {
    DCHECK(reinterpret_cast<Address>(start_[i]) >= reservation.address());
    DCHECK(reinterpret_cast<Address>(limit_[i]) >= reservation.address());
    DCHECK(start_[i] <= vm_limit);
    DCHECK(limit_[i] <= vm_limit);
    DCHECK_EQ(0, reinterpret_cast<Address>(limit_[i]) & kStoreBufferMask);
  }

  // Set RW permissions only on the pages we use.
  const size_t used_size = RoundUp(requested_size, CommitPageSize());
  if (!reservation.SetPermissions(start, used_size,
                                  PageAllocator::kReadWrite)) {
    heap_->FatalProcessOutOfMemory("StoreBuffer::SetUp");
  }
  current_ = 0;
  top_ = start_[current_];
  virtual_memory_.TakeControl(&reservation);
}

void StoreBuffer::TearDown() {
  if (virtual_memory_.IsReserved()) virtual_memory_.Free();
  top_ = nullptr;
  for (int i = 0; i < kStoreBuffers; i++) {
    start_[i] = nullptr;
    limit_[i] = nullptr;
    lazy_top_[i] = nullptr;
  }
}

void StoreBuffer::DeleteDuringRuntime(StoreBuffer* store_buffer, Address start,
                                      Address end) {
  DCHECK(store_buffer->mode() == StoreBuffer::NOT_IN_GC);
  store_buffer->InsertDeletionIntoStoreBuffer(start, end);
}

void StoreBuffer::InsertDuringRuntime(StoreBuffer* store_buffer, Address slot) {
  DCHECK(store_buffer->mode() == StoreBuffer::NOT_IN_GC);
  store_buffer->InsertIntoStoreBuffer(slot);
}

void StoreBuffer::DeleteDuringGarbageCollection(StoreBuffer* store_buffer,
                                                Address start, Address end) {
  // In GC the store buffer has to be empty at any time.
  DCHECK(store_buffer->Empty());
  DCHECK(store_buffer->mode() != StoreBuffer::NOT_IN_GC);
  Page* page = Page::FromAddress(start);
  if (end) {
    RememberedSet<OLD_TO_NEW>::RemoveRange(page, start, end,
                                           SlotSet::PREFREE_EMPTY_BUCKETS);
  } else {
    RememberedSet<OLD_TO_NEW>::Remove(page, start);
  }
}

void StoreBuffer::InsertDuringGarbageCollection(StoreBuffer* store_buffer,
                                                Address slot) {
  DCHECK(store_buffer->mode() != StoreBuffer::NOT_IN_GC);
  RememberedSet<OLD_TO_NEW>::Insert(Page::FromAddress(slot), slot);
}

void StoreBuffer::SetMode(StoreBufferMode mode) {
  mode_ = mode;
  if (mode == NOT_IN_GC) {
    insertion_callback = &InsertDuringRuntime;
    deletion_callback = &DeleteDuringRuntime;
  } else {
    insertion_callback = &InsertDuringGarbageCollection;
    deletion_callback = &DeleteDuringGarbageCollection;
  }
}

int StoreBuffer::StoreBufferOverflow(Isolate* isolate) {
  isolate->heap()->store_buffer()->FlipStoreBuffers();
  isolate->counters()->store_buffer_overflows()->Increment();
  // Called by RecordWriteCodeStubAssembler, which doesnt accept void type
  return 0;
}

void StoreBuffer::FlipStoreBuffers() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  int other = (current_ + 1) % kStoreBuffers;
  MoveEntriesToRememberedSet(other);
  lazy_top_[current_] = top_;
  current_ = other;
  top_ = start_[current_];

  if (!task_running_ && FLAG_concurrent_store_buffer) {
    task_running_ = true;
    V8::GetCurrentPlatform()->CallOnWorkerThread(
        base::make_unique<Task>(heap_->isolate(), this));
  }
}

void StoreBuffer::MoveEntriesToRememberedSet(int index) {
  if (!lazy_top_[index]) return;
  DCHECK_GE(index, 0);
  DCHECK_LT(index, kStoreBuffers);
  Address last_inserted_addr = kNullAddress;

  // We are taking the chunk map mutex here because the page lookup of addr
  // below may require us to check if addr is part of a large page.
  base::LockGuard<base::Mutex> guard(heap_->lo_space()->chunk_map_mutex());
  for (Address* current = start_[index]; current < lazy_top_[index];
       current++) {
    Address addr = *current;
    MemoryChunk* chunk = MemoryChunk::FromAnyPointerAddress(heap_, addr);
    if (IsDeletionAddress(addr)) {
      last_inserted_addr = kNullAddress;
      current++;
      Address end = *current;
      DCHECK(!IsDeletionAddress(end));
      addr = UnmarkDeletionAddress(addr);
      if (end) {
        RememberedSet<OLD_TO_NEW>::RemoveRange(chunk, addr, end,
                                               SlotSet::PREFREE_EMPTY_BUCKETS);
      } else {
        RememberedSet<OLD_TO_NEW>::Remove(chunk, addr);
      }
    } else {
      DCHECK(!IsDeletionAddress(addr));
      if (addr != last_inserted_addr) {
        RememberedSet<OLD_TO_NEW>::Insert(chunk, addr);
        last_inserted_addr = addr;
      }
    }
  }
  lazy_top_[index] = nullptr;
}

void StoreBuffer::MoveAllEntriesToRememberedSet() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  int other = (current_ + 1) % kStoreBuffers;
  MoveEntriesToRememberedSet(other);
  lazy_top_[current_] = top_;
  MoveEntriesToRememberedSet(current_);
  top_ = start_[current_];
}

void StoreBuffer::ConcurrentlyProcessStoreBuffer() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  int other = (current_ + 1) % kStoreBuffers;
  MoveEntriesToRememberedSet(other);
  task_running_ = false;
}

}  // namespace internal
}  // namespace v8

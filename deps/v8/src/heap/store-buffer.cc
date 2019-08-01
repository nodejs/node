// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/store-buffer.h"

#include <algorithm>

#include "src/base/bits.h"
#include "src/base/macros.h"
#include "src/base/template-utils.h"
#include "src/execution/isolate.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/store-buffer-inl.h"
#include "src/init/v8.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"

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
  // Round up the requested size in order to fulfill the VirtualMemory's
  // requrements on the requested size alignment. This may cause a bit of
  // memory wastage if the actual CommitPageSize() will be bigger than the
  // kMinExpectedOSPageSize value but this is a trade-off for keeping the
  // store buffer overflow check in write barriers cheap.
  const size_t requested_size = RoundUp(kStoreBufferSize * kStoreBuffers,
                                        page_allocator->CommitPageSize());
  // Allocate buffer memory aligned at least to kStoreBufferSize. This lets us
  // use a bit test to detect the ends of the buffers.
  STATIC_ASSERT(base::bits::IsPowerOfTwo(kStoreBufferSize));
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
  limit_[0] = start_[0] + (kStoreBufferSize / kSystemPointerSize);
  start_[1] = limit_[0];
  limit_[1] = start_[1] + (kStoreBufferSize / kSystemPointerSize);

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
  virtual_memory_ = std::move(reservation);
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
  base::MutexGuard guard(&mutex_);
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
  MemoryChunk* chunk = nullptr;

  for (Address* current = start_[index]; current < lazy_top_[index];
       current++) {
    Address addr = *current;
    if (chunk == nullptr ||
        MemoryChunk::BaseAddress(addr) != chunk->address()) {
      chunk = MemoryChunk::FromAnyPointerAddress(addr);
    }
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
  base::MutexGuard guard(&mutex_);
  int other = (current_ + 1) % kStoreBuffers;
  MoveEntriesToRememberedSet(other);
  lazy_top_[current_] = top_;
  MoveEntriesToRememberedSet(current_);
  top_ = start_[current_];
}

void StoreBuffer::ConcurrentlyProcessStoreBuffer() {
  base::MutexGuard guard(&mutex_);
  int other = (current_ + 1) % kStoreBuffers;
  MoveEntriesToRememberedSet(other);
  task_running_ = false;
}

}  // namespace internal
}  // namespace v8

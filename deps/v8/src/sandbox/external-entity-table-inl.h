// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_EXTERNAL_ENTITY_TABLE_INL_H_
#define V8_SANDBOX_EXTERNAL_ENTITY_TABLE_INL_H_

#include "src/base/atomicops.h"
#include "src/common/assert-scope.h"
#include "src/sandbox/external-entity-table.h"
#include "src/utils/allocation.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

template <typename Entry, size_t size>
Entry& ExternalEntityTable<Entry, size>::at(uint32_t index) {
  DCHECK_LT(index, capacity());
  return buffer_[index];
}

template <typename Entry, size_t size>
const Entry& ExternalEntityTable<Entry, size>::at(uint32_t index) const {
  DCHECK_LT(index, capacity());
  return buffer_[index];
}

template <typename Entry, size_t size>
bool ExternalEntityTable<Entry, size>::is_initialized() const {
  return buffer_ != nullptr;
}

template <typename Entry, size_t size>
uint32_t ExternalEntityTable<Entry, size>::capacity() const {
  return capacity_.load(std::memory_order_relaxed);
}

template <typename Entry, size_t size>
uint32_t ExternalEntityTable<Entry, size>::freelist_length() const {
  auto freelist = freelist_head_.load(std::memory_order_relaxed);
  DCHECK_LE(freelist.length(), capacity());
  return freelist.length();
}

template <typename Entry, size_t size>
void ExternalEntityTable<Entry, size>::InitializeTable() {
  DCHECK(!is_initialized());

  VirtualAddressSpace* root_space = GetPlatformVirtualAddressSpace();
  DCHECK(IsAligned(kReservationSize, root_space->allocation_granularity()));

  Address buffer_start = root_space->AllocatePages(
      VirtualAddressSpace::kNoHint, kReservationSize,
      root_space->allocation_granularity(), PagePermissions::kNoAccess);
  if (!buffer_start) {
    V8::FatalProcessOutOfMemory(nullptr,
                                "ExternalEntityTable::InitializeTable");
  }
  buffer_ = reinterpret_cast<Entry*>(buffer_start);

  mutex_ = new base::Mutex;
  if (!mutex_) {
    V8::FatalProcessOutOfMemory(nullptr,
                                "ExternalEntityTable::InitializeTable");
  }

  // Allocate the initial block. Mutex must be held for that.
  base::MutexGuard guard(mutex_);
  Grow();
}

template <typename Entry, size_t size>
void ExternalEntityTable<Entry, size>::TearDownTable() {
  DCHECK(is_initialized());

  Address buffer_start = reinterpret_cast<Address>(buffer_);
  GetPlatformVirtualAddressSpace()->FreePages(buffer_start, kReservationSize);
  delete mutex_;

  buffer_ = nullptr;
  mutex_ = nullptr;
  freelist_head_ = FreelistHead();
  capacity_ = 0;
  extra_ = 0;
}

template <typename Entry, size_t size>
uint32_t ExternalEntityTable<Entry, size>::AllocateEntry() {
  DCHECK(is_initialized());

  // We currently don't want entry allocation to trigger garbage collection as
  // this may cause seemingly harmless pointer field assignments to trigger
  // garbage collection. This is especially true for lazily-initialized
  // external pointer slots which will typically only allocate the external
  // pointer table entry when the pointer is first set to a non-null value.
  DisallowGarbageCollection no_gc;

  FreelistHead freelist;
  bool success = false;
  while (!success) {
    // This is essentially DCLP (see
    // https://preshing.com/20130930/double-checked-locking-is-fixed-in-cpp11/)
    // and so requires an acquire load as well as a release store in Grow() to
    // prevent reordering of memory accesses, which could for example cause one
    // thread to read a freelist entry before it has been properly initialized.
    freelist = freelist_head_.load(std::memory_order_acquire);
    if (freelist.is_empty()) {
      // Freelist is empty. Need to take the lock, then attempt to grow the
      // table if no other thread has done it in the meantime.
      base::MutexGuard guard(mutex_);

      // Reload freelist head in case another thread already grew the table.
      freelist = freelist_head_.load(std::memory_order_relaxed);

      if (freelist.is_empty()) {
        // Freelist is (still) empty so grow the table.
        freelist = Grow();
        // Grow() adds one block to the table and so to the freelist.
        DCHECK_EQ(freelist.length(), kEntriesPerBlock);
      }
    }

    success = TryAllocateEntryFromFreelist(freelist);
  }

  uint32_t allocated_entry = freelist.next();
  DCHECK_NE(allocated_entry, 0);
  DCHECK_LT(allocated_entry, capacity());
  return allocated_entry;
}

template <typename Entry, size_t size>
uint32_t ExternalEntityTable<Entry, size>::AllocateEntryBelow(
    uint32_t threshold_index) {
  DCHECK(is_initialized());
  DCHECK_LE(threshold_index, capacity());

  FreelistHead freelist;
  bool success = false;
  while (!success) {
    freelist = freelist_head_.load(std::memory_order_acquire);
    // Check that the next free entry is below the threshold.
    if (freelist.is_empty() || freelist.next() >= threshold_index) return 0;

    success = TryAllocateEntryFromFreelist(freelist);
  }

  uint32_t allocated_entry = freelist.next();
  DCHECK_NE(allocated_entry, 0);
  DCHECK_LT(allocated_entry, threshold_index);
  return allocated_entry;
}

template <typename Entry, size_t size>
bool ExternalEntityTable<Entry, size>::TryAllocateEntryFromFreelist(
    FreelistHead freelist) {
  DCHECK(!freelist.is_empty());
  DCHECK_LT(freelist.next(), capacity());
  DCHECK_LT(freelist.length(), capacity());

  Entry& freelist_entry = at(freelist.next());
  uint32_t next_freelist_entry = freelist_entry.GetNextFreelistEntryIndex();
  FreelistHead new_freelist(next_freelist_entry, freelist.length() - 1);
  bool success = freelist_head_.compare_exchange_strong(
      freelist, new_freelist, std::memory_order_relaxed);

  // When the CAS succeeded, the entry must've been a freelist entry.
  // Otherwise, this is not guaranteed as another thread may have allocated
  // and overwritten the same entry in the meantime.
  if (success) {
    DCHECK_LT(new_freelist.next(), capacity());
    DCHECK_LT(new_freelist.length(), capacity());
    DCHECK_IMPLIES(freelist.length() > 1, !new_freelist.is_empty());
    DCHECK_IMPLIES(freelist.length() == 1, new_freelist.is_empty());
  }
  return success;
}

template <typename Entry, size_t size>
typename ExternalEntityTable<Entry, size>::FreelistHead
ExternalEntityTable<Entry, size>::Grow() {
  // Freelist should be empty when calling this method.
  DCHECK_EQ(freelist_length(), 0);
  // The caller must lock the mutex before calling Grow().
  mutex_->AssertHeld();

  // Grow the table by one block.
  VirtualAddressSpace* root_space = GetPlatformVirtualAddressSpace();
  DCHECK(IsAligned(kBlockSize, root_space->page_size()));
  uint32_t old_capacity = capacity();
  uint32_t new_capacity = old_capacity + kEntriesPerBlock;
  if (new_capacity > kMaxCapacity) {
    V8::FatalProcessOutOfMemory(nullptr, "ExternalEntityTable::Grow");
  }
  Address buffer_start = reinterpret_cast<Address>(buffer_);
  if (!root_space->SetPagePermissions(buffer_start + old_capacity * kEntrySize,
                                      kBlockSize,
                                      PagePermissions::kReadWrite)) {
    V8::FatalProcessOutOfMemory(nullptr, "ExternalEntityTable::Grow");
  }

  capacity_.store(new_capacity, std::memory_order_relaxed);

  // Build the freelist bottom to top but skip entry zero, which is reserved.
  uint32_t start = std::max<uint32_t>(old_capacity, 1);
  uint32_t last = new_capacity - 1;
  for (uint32_t i = start; i < last; i++) {
    uint32_t next_free_entry = i + 1;
    at(i).MakeFreelistEntry(next_free_entry);
  }
  at(last).MakeFreelistEntry(0);

  // This must be a release store to prevent reordering of the preceeding
  // stores to the freelist from being reordered past this store. See
  // AllocateEntry() for more details.
  FreelistHead new_freelist_head(start, last - start + 1);
  freelist_head_.store(new_freelist_head, std::memory_order_release);

  return new_freelist_head;
}

template <typename Entry, size_t size>
void ExternalEntityTable<Entry, size>::Shrink(uint32_t new_capacity) {
  uint32_t old_capacity = capacity();
  DCHECK(IsAligned(new_capacity, kEntriesPerBlock));
  DCHECK_GT(new_capacity, 0);
  DCHECK_LT(new_capacity, old_capacity);

  capacity_.store(new_capacity, std::memory_order_relaxed);

  Address buffer_start = reinterpret_cast<Address>(buffer_);
  Address new_table_end = buffer_start + new_capacity * kEntrySize;
  uint32_t bytes_to_decommit = (old_capacity - new_capacity) * kEntrySize;
  // The pages may contain stale pointers which could be abused by an
  // attacker if they are still accessible, so use Decommit here which
  // guarantees that the pages become inaccessible and will be zeroed out.
  VirtualAddressSpace* root_space = GetPlatformVirtualAddressSpace();
  CHECK(root_space->DecommitPages(new_table_end, bytes_to_decommit));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_EXTERNAL_ENTITY_TABLE_INL_H_

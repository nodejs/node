// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_EXTERNAL_POINTER_TABLE_INL_H_
#define V8_SANDBOX_EXTERNAL_POINTER_TABLE_INL_H_

#include "src/base/atomicops.h"
#include "src/common/assert-scope.h"
#include "src/sandbox/external-pointer-table.h"
#include "src/sandbox/external-pointer.h"
#include "src/utils/allocation.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

Address ExternalPointerTable::Get(ExternalPointerHandle handle,
                                  ExternalPointerTag tag) const {
  uint32_t index = HandleToIndex(handle);
  Entry entry = RelaxedLoad(index);
  DCHECK(entry.IsRegularEntry());
  return entry.Untag(tag);
}

void ExternalPointerTable::Set(ExternalPointerHandle handle, Address value,
                               ExternalPointerTag tag) {
  DCHECK_NE(kNullExternalPointerHandle, handle);
  DCHECK_EQ(0, value & kExternalPointerTagMask);
  DCHECK(tag & kExternalPointerMarkBit);

  uint32_t index = HandleToIndex(handle);
  Entry entry = Entry::MakeRegularEntry(value, tag);
  RelaxedStore(index, entry);
}

Address ExternalPointerTable::Exchange(ExternalPointerHandle handle,
                                       Address value, ExternalPointerTag tag) {
  DCHECK_NE(kNullExternalPointerHandle, handle);
  DCHECK_EQ(0, value & kExternalPointerTagMask);
  DCHECK(tag & kExternalPointerMarkBit);

  uint32_t index = HandleToIndex(handle);
  Entry new_entry = Entry::MakeRegularEntry(value, tag);
  Entry old_entry = RelaxedExchange(index, new_entry);
  DCHECK(old_entry.IsRegularEntry());
  return old_entry.Untag(tag);
}

bool ExternalPointerTable::TryAllocateEntryFromFreelist(Freelist freelist) {
  DCHECK(!freelist.IsEmpty());
  DCHECK_LT(freelist.Head(), capacity());
  DCHECK_LT(freelist.Size(), capacity());

  Entry entry = RelaxedLoad(freelist.Head());
  uint32_t new_freelist_head = entry.ExtractNextFreelistEntry();

  Freelist new_freelist(new_freelist_head, freelist.Size() - 1);
  bool success = Relaxed_CompareAndSwapFreelist(freelist, new_freelist);

  // When the CAS succeeded, the entry must've been a freelist entry.
  // Otherwise, this is not guaranteed as another thread may have allocated
  // the same entry in the meantime.
  if (success) {
    DCHECK(entry.IsFreelistEntry());
    DCHECK_LT(new_freelist.Head(), capacity());
    DCHECK_LT(new_freelist.Size(), capacity());
    DCHECK_IMPLIES(freelist.Size() > 1, !new_freelist.IsEmpty());
    DCHECK_IMPLIES(freelist.Size() == 1, new_freelist.IsEmpty());
  }
  return success;
}

ExternalPointerHandle ExternalPointerTable::AllocateAndInitializeEntry(
    Isolate* isolate, Address initial_value, ExternalPointerTag tag) {
  DCHECK(is_initialized());

  // We currently don't want entry allocation to trigger garbage collection as
  // this may cause seemingly harmless pointer field assignments to trigger
  // garbage collection. This is especially true for lazily-initialized
  // external pointer slots which will typically only allocate the external
  // pointer table entry when the pointer is first set to a non-null value.
  DisallowGarbageCollection no_gc;

  Freelist freelist;
  bool success = false;
  while (!success) {
    // This is essentially DCLP (see
    // https://preshing.com/20130930/double-checked-locking-is-fixed-in-cpp11/)
    // and so requires an acquire load as well as a release store in Grow() to
    // prevent reordering of memory accesses, which could for example cause one
    // thread to read a freelist entry before it has been properly initialized.
    freelist = Acquire_GetFreelist();
    if (freelist.IsEmpty()) {
      // Freelist is empty. Need to take the lock, then attempt to grow the
      // table if no other thread has done it in the meantime.
      base::MutexGuard guard(mutex_);

      // Reload freelist head in case another thread already grew the table.
      freelist = Relaxed_GetFreelist();

      if (freelist.IsEmpty()) {
        // Freelist is (still) empty so grow the table.
        freelist = Grow(isolate);
        // Grow() adds one block to the table and so to the freelist.
        DCHECK_EQ(freelist.Size(), kEntriesPerBlock);
      }
    }

    success = TryAllocateEntryFromFreelist(freelist);
  }

  DCHECK_NE(freelist.Head(), 0);
  DCHECK_LT(freelist.Head(), capacity());

  uint32_t entry_index = freelist.Head();
  Entry entry = Entry::MakeRegularEntry(initial_value, tag);
  RelaxedStore(entry_index, entry);

  return IndexToHandle(entry_index);
}

ExternalPointerHandle ExternalPointerTable::AllocateEvacuationEntry(
    uint32_t start_of_evacuation_area) {
  DCHECK(is_initialized());
  DCHECK_LT(start_of_evacuation_area, capacity());

  Freelist freelist;
  bool success = false;
  while (!success) {
    freelist = Acquire_GetFreelist();
    // Check that the next free entry is below the start of the evacuation area.
    if (freelist.IsEmpty() || freelist.Head() >= start_of_evacuation_area)
      return kNullExternalPointerHandle;

    success = TryAllocateEntryFromFreelist(freelist);
  }

  DCHECK_NE(freelist.Head(), 0);
  DCHECK_LT(freelist.Head(), start_of_evacuation_area);

  return IndexToHandle(freelist.Head());
}

uint32_t ExternalPointerTable::FreelistSize() {
  Freelist freelist = Relaxed_GetFreelist();
  DCHECK_LE(freelist.Size(), capacity());
  return freelist.Size();
}

void ExternalPointerTable::Mark(ExternalPointerHandle handle,
                                Address handle_location) {
  static_assert(sizeof(base::Atomic64) == sizeof(Address));
  // The handle_location must contain the given handle. The only exception to
  // this is when the handle is zero, which means that it hasn't yet been
  // initialized. In that case, the handle may be initialized between the
  // caller loading it and this DCHECK loading it again, in which case the two
  // values would not be the same. This scenario is unproblematic though as the
  // new entry will already be marked as alive as it has just been allocated.
  DCHECK(handle == kNullExternalPointerHandle ||
         handle ==
             base::AsAtomic32::Acquire_Load(
                 reinterpret_cast<ExternalPointerHandle*>(handle_location)));

  uint32_t index = HandleToIndex(handle);

  // Check if the entry should be evacuated for table compaction.
  // The current value of the start of the evacuation area is cached in a local
  // variable here as it otherwise may be changed by another marking thread
  // while this method runs, causing non-optimal behaviour (for example, the
  // allocation of an evacuation entry _after_ the entry that is evacuated).
  uint32_t current_start_of_evacuation_area = start_of_evacuation_area();
  if (index >= current_start_of_evacuation_area) {
    DCHECK(IsCompacting());
    ExternalPointerHandle new_handle =
        AllocateEvacuationEntry(current_start_of_evacuation_area);
    if (new_handle) {
      DCHECK_LT(HandleToIndex(new_handle), current_start_of_evacuation_area);
      uint32_t index = HandleToIndex(new_handle);
      // Even though the new entry will only be accessed during sweeping, this
      // still needs to be an atomic write as another thread may attempt (and
      // fail) to allocate the same table entry, thereby causing a read from
      // this memory location. Without an atomic store here, TSan would then
      // complain about a data race.
      RelaxedStore(index, Entry::MakeEvacuationEntry(handle_location));
#ifdef DEBUG
      // Mark the handle as visited in debug builds to detect double
      // initialization of external pointer fields.
      auto handle_ptr = reinterpret_cast<base::Atomic32*>(handle_location);
      base::Relaxed_Store(handle_ptr, handle | kVisitedHandleMarker);
#endif  // DEBUG
    } else {
      // In this case, the application has allocated a sufficiently large
      // number of entries from the freelist so that new entries would now be
      // allocated inside the area that is being compacted. While it would be
      // possible to shrink that area and continue compacting, we probably do
      // not want to put more pressure on the freelist and so instead simply
      // abort compaction here. Entries that have already been visited will
      // still be compacted during Sweep, but there is no guarantee that any
      // blocks at the end of the table will now be completely free.
      uint32_t compaction_aborted_marker =
          current_start_of_evacuation_area | kCompactionAbortedMarker;
      set_start_of_evacuation_area(compaction_aborted_marker);
    }
  }
  // Even if the entry is marked for evacuation, it still needs to be marked as
  // alive as it may be visited during sweeping before being evacuation.

  Entry old_entry = RelaxedLoad(index);
  DCHECK(old_entry.IsRegularEntry());

  Entry new_entry = old_entry;
  new_entry.SetMarkBit();

  // We don't need to perform the CAS in a loop: if the new value is not equal
  // to the old value, then the mutator must've just written a new value into
  // the entry. This in turn must've set the marking bit already (see
  // ExternalPointerTable::Set), so we don't need to do it again.
  Entry entry = RelaxedCompareAndSwap(index, old_entry, new_entry);
  DCHECK((entry == old_entry) || entry.IsMarked());
  USE(entry);
}

bool ExternalPointerTable::IsCompacting() {
  return start_of_evacuation_area() != kNotCompactingMarker;
}

bool ExternalPointerTable::CompactingWasAbortedDuringMarking() {
  return (start_of_evacuation_area() & kCompactionAbortedMarker) ==
         kCompactionAbortedMarker;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_EXTERNAL_POINTER_TABLE_INL_H_

// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STORE_BUFFER_H_
#define V8_STORE_BUFFER_H_

#include "allocation.h"
#include "checks.h"
#include "globals.h"
#include "platform.h"
#include "v8globals.h"

namespace v8 {
namespace internal {

class Page;
class PagedSpace;
class StoreBuffer;

typedef void (*ObjectSlotCallback)(HeapObject** from, HeapObject* to);

typedef void (StoreBuffer::*RegionCallback)(Address start,
                                            Address end,
                                            ObjectSlotCallback slot_callback,
                                            bool clear_maps);

// Used to implement the write barrier by collecting addresses of pointers
// between spaces.
class StoreBuffer {
 public:
  explicit StoreBuffer(Heap* heap);

  static void StoreBufferOverflow(Isolate* isolate);

  inline Address TopAddress();

  void SetUp();
  void TearDown();

  // This is used by the mutator to enter addresses into the store buffer.
  inline void Mark(Address addr);

  // This is used by the heap traversal to enter the addresses into the store
  // buffer that should still be in the store buffer after GC.  It enters
  // addresses directly into the old buffer because the GC starts by wiping the
  // old buffer and thereafter only visits each cell once so there is no need
  // to attempt to remove any dupes.  During the first part of a GC we
  // are using the store buffer to access the old spaces and at the same time
  // we are rebuilding the store buffer using this function.  There is, however
  // no issue of overwriting the buffer we are iterating over, because this
  // stage of the scavenge can only reduce the number of addresses in the store
  // buffer (some objects are promoted so pointers to them do not need to be in
  // the store buffer).  The later parts of the GC scan the pages that are
  // exempt from the store buffer and process the promotion queue.  These steps
  // can overflow this buffer.  We check for this and on overflow we call the
  // callback set up with the StoreBufferRebuildScope object.
  inline void EnterDirectlyIntoStoreBuffer(Address addr);

  // Iterates over all pointers that go from old space to new space.  It will
  // delete the store buffer as it starts so the callback should reenter
  // surviving old-to-new pointers into the store buffer to rebuild it.
  void IteratePointersToNewSpace(ObjectSlotCallback callback);

  // Same as IteratePointersToNewSpace but additonally clears maps in objects
  // referenced from the store buffer that do not contain a forwarding pointer.
  void IteratePointersToNewSpaceAndClearMaps(ObjectSlotCallback callback);

  static const int kStoreBufferOverflowBit = 1 << (14 + kPointerSizeLog2);
  static const int kStoreBufferSize = kStoreBufferOverflowBit;
  static const int kStoreBufferLength = kStoreBufferSize / sizeof(Address);
  static const int kOldStoreBufferLength = kStoreBufferLength * 16;
  static const int kHashSetLengthLog2 = 12;
  static const int kHashSetLength = 1 << kHashSetLengthLog2;

  void Compact();

  void GCPrologue();
  void GCEpilogue();

  Object*** Limit() { return reinterpret_cast<Object***>(old_limit_); }
  Object*** Start() { return reinterpret_cast<Object***>(old_start_); }
  Object*** Top() { return reinterpret_cast<Object***>(old_top_); }
  void SetTop(Object*** top) {
    ASSERT(top >= Start());
    ASSERT(top <= Limit());
    old_top_ = reinterpret_cast<Address*>(top);
  }

  bool old_buffer_is_sorted() { return old_buffer_is_sorted_; }
  bool old_buffer_is_filtered() { return old_buffer_is_filtered_; }

  // Goes through the store buffer removing pointers to things that have
  // been promoted.  Rebuilds the store buffer completely if it overflowed.
  void SortUniq();

  void EnsureSpace(intptr_t space_needed);
  void Verify();

  bool PrepareForIteration();

#ifdef DEBUG
  void Clean();
  // Slow, for asserts only.
  bool CellIsInStoreBuffer(Address cell);
#endif

  void Filter(int flag);

 private:
  Heap* heap_;

  // The store buffer is divided up into a new buffer that is constantly being
  // filled by mutator activity and an old buffer that is filled with the data
  // from the new buffer after compression.
  Address* start_;
  Address* limit_;

  Address* old_start_;
  Address* old_limit_;
  Address* old_top_;
  Address* old_reserved_limit_;
  VirtualMemory* old_virtual_memory_;

  bool old_buffer_is_sorted_;
  bool old_buffer_is_filtered_;
  bool during_gc_;
  // The garbage collector iterates over many pointers to new space that are not
  // handled by the store buffer.  This flag indicates whether the pointers
  // found by the callbacks should be added to the store buffer or not.
  bool store_buffer_rebuilding_enabled_;
  StoreBufferCallback callback_;
  bool may_move_store_buffer_entries_;

  VirtualMemory* virtual_memory_;

  // Two hash sets used for filtering.
  // If address is in the hash set then it is guaranteed to be in the
  // old part of the store buffer.
  uintptr_t* hash_set_1_;
  uintptr_t* hash_set_2_;
  bool hash_sets_are_empty_;

  void ClearFilteringHashSets();

  bool SpaceAvailable(intptr_t space_needed);
  void Uniq();
  void ExemptPopularPages(int prime_sample_step, int threshold);

  // Set the map field of the object to NULL if contains a map.
  inline void ClearDeadObject(HeapObject *object);

  void IteratePointersToNewSpace(ObjectSlotCallback callback, bool clear_maps);

  void FindPointersToNewSpaceInRegion(Address start,
                                      Address end,
                                      ObjectSlotCallback slot_callback,
                                      bool clear_maps);

  // For each region of pointers on a page in use from an old space call
  // visit_pointer_region callback.
  // If either visit_pointer_region or callback can cause an allocation
  // in old space and changes in allocation watermark then
  // can_preallocate_during_iteration should be set to true.
  void IteratePointersOnPage(
      PagedSpace* space,
      Page* page,
      RegionCallback region_callback,
      ObjectSlotCallback slot_callback);

  void FindPointersToNewSpaceInMaps(
    Address start,
    Address end,
    ObjectSlotCallback slot_callback,
    bool clear_maps);

  void FindPointersToNewSpaceInMapsRegion(
    Address start,
    Address end,
    ObjectSlotCallback slot_callback,
    bool clear_maps);

  void FindPointersToNewSpaceOnPage(
    PagedSpace* space,
    Page* page,
    RegionCallback region_callback,
    ObjectSlotCallback slot_callback,
    bool clear_maps);

  void IteratePointersInStoreBuffer(ObjectSlotCallback slot_callback,
                                    bool clear_maps);

#ifdef VERIFY_HEAP
  void VerifyPointers(PagedSpace* space, RegionCallback region_callback);
  void VerifyPointers(LargeObjectSpace* space);
#endif

  friend class StoreBufferRebuildScope;
  friend class DontMoveStoreBufferEntriesScope;
};


class StoreBufferRebuildScope {
 public:
  explicit StoreBufferRebuildScope(Heap* heap,
                                   StoreBuffer* store_buffer,
                                   StoreBufferCallback callback)
      : store_buffer_(store_buffer),
        stored_state_(store_buffer->store_buffer_rebuilding_enabled_),
        stored_callback_(store_buffer->callback_) {
    store_buffer_->store_buffer_rebuilding_enabled_ = true;
    store_buffer_->callback_ = callback;
    (*callback)(heap, NULL, kStoreBufferStartScanningPagesEvent);
  }

  ~StoreBufferRebuildScope() {
    store_buffer_->callback_ = stored_callback_;
    store_buffer_->store_buffer_rebuilding_enabled_ = stored_state_;
  }

 private:
  StoreBuffer* store_buffer_;
  bool stored_state_;
  StoreBufferCallback stored_callback_;
};


class DontMoveStoreBufferEntriesScope {
 public:
  explicit DontMoveStoreBufferEntriesScope(StoreBuffer* store_buffer)
      : store_buffer_(store_buffer),
        stored_state_(store_buffer->may_move_store_buffer_entries_) {
    store_buffer_->may_move_store_buffer_entries_ = false;
  }

  ~DontMoveStoreBufferEntriesScope() {
    store_buffer_->may_move_store_buffer_entries_ = stored_state_;
  }

 private:
  StoreBuffer* store_buffer_;
  bool stored_state_;
};

} }  // namespace v8::internal

#endif  // V8_STORE_BUFFER_H_

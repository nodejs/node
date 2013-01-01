// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "store-buffer.h"
#include "store-buffer-inl.h"
#include "v8-counters.h"

namespace v8 {
namespace internal {

StoreBuffer::StoreBuffer(Heap* heap)
    : heap_(heap),
      start_(NULL),
      limit_(NULL),
      old_start_(NULL),
      old_limit_(NULL),
      old_top_(NULL),
      old_reserved_limit_(NULL),
      old_buffer_is_sorted_(false),
      old_buffer_is_filtered_(false),
      during_gc_(false),
      store_buffer_rebuilding_enabled_(false),
      callback_(NULL),
      may_move_store_buffer_entries_(true),
      virtual_memory_(NULL),
      hash_set_1_(NULL),
      hash_set_2_(NULL),
      hash_sets_are_empty_(true) {
}


void StoreBuffer::SetUp() {
  virtual_memory_ = new VirtualMemory(kStoreBufferSize * 3);
  uintptr_t start_as_int =
      reinterpret_cast<uintptr_t>(virtual_memory_->address());
  start_ =
      reinterpret_cast<Address*>(RoundUp(start_as_int, kStoreBufferSize * 2));
  limit_ = start_ + (kStoreBufferSize / kPointerSize);

  old_virtual_memory_ =
      new VirtualMemory(kOldStoreBufferLength * kPointerSize);
  old_top_ = old_start_ =
      reinterpret_cast<Address*>(old_virtual_memory_->address());
  // Don't know the alignment requirements of the OS, but it is certainly not
  // less than 0xfff.
  ASSERT((reinterpret_cast<uintptr_t>(old_start_) & 0xfff) == 0);
  int initial_length = static_cast<int>(OS::CommitPageSize() / kPointerSize);
  ASSERT(initial_length > 0);
  ASSERT(initial_length <= kOldStoreBufferLength);
  old_limit_ = old_start_ + initial_length;
  old_reserved_limit_ = old_start_ + kOldStoreBufferLength;

  CHECK(old_virtual_memory_->Commit(
            reinterpret_cast<void*>(old_start_),
            (old_limit_ - old_start_) * kPointerSize,
            false));

  ASSERT(reinterpret_cast<Address>(start_) >= virtual_memory_->address());
  ASSERT(reinterpret_cast<Address>(limit_) >= virtual_memory_->address());
  Address* vm_limit = reinterpret_cast<Address*>(
      reinterpret_cast<char*>(virtual_memory_->address()) +
          virtual_memory_->size());
  ASSERT(start_ <= vm_limit);
  ASSERT(limit_ <= vm_limit);
  USE(vm_limit);
  ASSERT((reinterpret_cast<uintptr_t>(limit_) & kStoreBufferOverflowBit) != 0);
  ASSERT((reinterpret_cast<uintptr_t>(limit_ - 1) & kStoreBufferOverflowBit) ==
         0);

  CHECK(virtual_memory_->Commit(reinterpret_cast<Address>(start_),
                                kStoreBufferSize,
                                false));  // Not executable.
  heap_->public_set_store_buffer_top(start_);

  hash_set_1_ = new uintptr_t[kHashSetLength];
  hash_set_2_ = new uintptr_t[kHashSetLength];
  hash_sets_are_empty_ = false;

  ClearFilteringHashSets();
}


void StoreBuffer::TearDown() {
  delete virtual_memory_;
  delete old_virtual_memory_;
  delete[] hash_set_1_;
  delete[] hash_set_2_;
  old_start_ = old_top_ = old_limit_ = old_reserved_limit_ = NULL;
  start_ = limit_ = NULL;
  heap_->public_set_store_buffer_top(start_);
}


void StoreBuffer::StoreBufferOverflow(Isolate* isolate) {
  isolate->heap()->store_buffer()->Compact();
}


#if V8_TARGET_ARCH_X64
static int CompareAddresses(const void* void_a, const void* void_b) {
  intptr_t a =
      reinterpret_cast<intptr_t>(*reinterpret_cast<const Address*>(void_a));
  intptr_t b =
      reinterpret_cast<intptr_t>(*reinterpret_cast<const Address*>(void_b));
  // Unfortunately if int is smaller than intptr_t there is no branch-free
  // way to return a number with the same sign as the difference between the
  // pointers.
  if (a == b) return 0;
  if (a < b) return -1;
  ASSERT(a > b);
  return 1;
}
#else
static int CompareAddresses(const void* void_a, const void* void_b) {
  intptr_t a =
      reinterpret_cast<intptr_t>(*reinterpret_cast<const Address*>(void_a));
  intptr_t b =
      reinterpret_cast<intptr_t>(*reinterpret_cast<const Address*>(void_b));
  ASSERT(sizeof(1) == sizeof(a));
  // Shift down to avoid wraparound.
  return (a >> kPointerSizeLog2) - (b >> kPointerSizeLog2);
}
#endif


void StoreBuffer::Uniq() {
  // Remove adjacent duplicates and cells that do not point at new space.
  Address previous = NULL;
  Address* write = old_start_;
  ASSERT(may_move_store_buffer_entries_);
  for (Address* read = old_start_; read < old_top_; read++) {
    Address current = *read;
    if (current != previous) {
      if (heap_->InNewSpace(*reinterpret_cast<Object**>(current))) {
        *write++ = current;
      }
    }
    previous = current;
  }
  old_top_ = write;
}


void StoreBuffer::EnsureSpace(intptr_t space_needed) {
  while (old_limit_ - old_top_ < space_needed &&
         old_limit_ < old_reserved_limit_) {
    size_t grow = old_limit_ - old_start_;  // Double size.
    CHECK(old_virtual_memory_->Commit(reinterpret_cast<void*>(old_limit_),
                                      grow * kPointerSize,
                                      false));
    old_limit_ += grow;
  }

  if (old_limit_ - old_top_ >= space_needed) return;

  if (old_buffer_is_filtered_) return;
  ASSERT(may_move_store_buffer_entries_);
  Compact();

  old_buffer_is_filtered_ = true;
  bool page_has_scan_on_scavenge_flag = false;

  PointerChunkIterator it(heap_);
  MemoryChunk* chunk;
  while ((chunk = it.next()) != NULL) {
    if (chunk->scan_on_scavenge()) page_has_scan_on_scavenge_flag = true;
  }

  if (page_has_scan_on_scavenge_flag) {
    Filter(MemoryChunk::SCAN_ON_SCAVENGE);
  }

  // If filtering out the entries from scan_on_scavenge pages got us down to
  // less than half full, then we are satisfied with that.
  if (old_limit_ - old_top_ > old_top_ - old_start_) return;

  // Sample 1 entry in 97 and filter out the pages where we estimate that more
  // than 1 in 8 pointers are to new space.
  static const int kSampleFinenesses = 5;
  static const struct Samples {
    int prime_sample_step;
    int threshold;
  } samples[kSampleFinenesses] =  {
    { 97, ((Page::kPageSize / kPointerSize) / 97) / 8 },
    { 23, ((Page::kPageSize / kPointerSize) / 23) / 16 },
    { 7, ((Page::kPageSize / kPointerSize) / 7) / 32 },
    { 3, ((Page::kPageSize / kPointerSize) / 3) / 256 },
    { 1, 0}
  };
  for (int i = kSampleFinenesses - 1; i >= 0; i--) {
    ExemptPopularPages(samples[i].prime_sample_step, samples[i].threshold);
    // As a last resort we mark all pages as being exempt from the store buffer.
    ASSERT(i != 0 || old_top_ == old_start_);
    if (old_limit_ - old_top_ > old_top_ - old_start_) return;
  }
  UNREACHABLE();
}


// Sample the store buffer to see if some pages are taking up a lot of space
// in the store buffer.
void StoreBuffer::ExemptPopularPages(int prime_sample_step, int threshold) {
  PointerChunkIterator it(heap_);
  MemoryChunk* chunk;
  while ((chunk = it.next()) != NULL) {
    chunk->set_store_buffer_counter(0);
  }
  bool created_new_scan_on_scavenge_pages = false;
  MemoryChunk* previous_chunk = NULL;
  for (Address* p = old_start_; p < old_top_; p += prime_sample_step) {
    Address addr = *p;
    MemoryChunk* containing_chunk = NULL;
    if (previous_chunk != NULL && previous_chunk->Contains(addr)) {
      containing_chunk = previous_chunk;
    } else {
      containing_chunk = MemoryChunk::FromAnyPointerAddress(addr);
    }
    int old_counter = containing_chunk->store_buffer_counter();
    if (old_counter == threshold) {
      containing_chunk->set_scan_on_scavenge(true);
      created_new_scan_on_scavenge_pages = true;
    }
    containing_chunk->set_store_buffer_counter(old_counter + 1);
    previous_chunk = containing_chunk;
  }
  if (created_new_scan_on_scavenge_pages) {
    Filter(MemoryChunk::SCAN_ON_SCAVENGE);
  }
  old_buffer_is_filtered_ = true;
}


void StoreBuffer::Filter(int flag) {
  Address* new_top = old_start_;
  MemoryChunk* previous_chunk = NULL;
  for (Address* p = old_start_; p < old_top_; p++) {
    Address addr = *p;
    MemoryChunk* containing_chunk = NULL;
    if (previous_chunk != NULL && previous_chunk->Contains(addr)) {
      containing_chunk = previous_chunk;
    } else {
      containing_chunk = MemoryChunk::FromAnyPointerAddress(addr);
      previous_chunk = containing_chunk;
    }
    if (!containing_chunk->IsFlagSet(flag)) {
      *new_top++ = addr;
    }
  }
  old_top_ = new_top;

  // Filtering hash sets are inconsistent with the store buffer after this
  // operation.
  ClearFilteringHashSets();
}


void StoreBuffer::SortUniq() {
  Compact();
  if (old_buffer_is_sorted_) return;
  qsort(reinterpret_cast<void*>(old_start_),
        old_top_ - old_start_,
        sizeof(*old_top_),
        &CompareAddresses);
  Uniq();

  old_buffer_is_sorted_ = true;

  // Filtering hash sets are inconsistent with the store buffer after this
  // operation.
  ClearFilteringHashSets();
}


bool StoreBuffer::PrepareForIteration() {
  Compact();
  PointerChunkIterator it(heap_);
  MemoryChunk* chunk;
  bool page_has_scan_on_scavenge_flag = false;
  while ((chunk = it.next()) != NULL) {
    if (chunk->scan_on_scavenge()) page_has_scan_on_scavenge_flag = true;
  }

  if (page_has_scan_on_scavenge_flag) {
    Filter(MemoryChunk::SCAN_ON_SCAVENGE);
  }

  // Filtering hash sets are inconsistent with the store buffer after
  // iteration.
  ClearFilteringHashSets();

  return page_has_scan_on_scavenge_flag;
}


#ifdef DEBUG
void StoreBuffer::Clean() {
  ClearFilteringHashSets();
  Uniq();  // Also removes things that no longer point to new space.
  CheckForFullBuffer();
}


static Address* in_store_buffer_1_element_cache = NULL;


bool StoreBuffer::CellIsInStoreBuffer(Address cell_address) {
  if (!FLAG_enable_slow_asserts) return true;
  if (in_store_buffer_1_element_cache != NULL &&
      *in_store_buffer_1_element_cache == cell_address) {
    return true;
  }
  Address* top = reinterpret_cast<Address*>(heap_->store_buffer_top());
  for (Address* current = top - 1; current >= start_; current--) {
    if (*current == cell_address) {
      in_store_buffer_1_element_cache = current;
      return true;
    }
  }
  for (Address* current = old_top_ - 1; current >= old_start_; current--) {
    if (*current == cell_address) {
      in_store_buffer_1_element_cache = current;
      return true;
    }
  }
  return false;
}
#endif


void StoreBuffer::ClearFilteringHashSets() {
  if (!hash_sets_are_empty_) {
    memset(reinterpret_cast<void*>(hash_set_1_),
           0,
           sizeof(uintptr_t) * kHashSetLength);
    memset(reinterpret_cast<void*>(hash_set_2_),
           0,
           sizeof(uintptr_t) * kHashSetLength);
    hash_sets_are_empty_ = true;
  }
}


void StoreBuffer::GCPrologue() {
  ClearFilteringHashSets();
  during_gc_ = true;
}


#ifdef VERIFY_HEAP
static void DummyScavengePointer(HeapObject** p, HeapObject* o) {
  // Do nothing.
}


void StoreBuffer::VerifyPointers(PagedSpace* space,
                                 RegionCallback region_callback) {
  PageIterator it(space);

  while (it.has_next()) {
    Page* page = it.next();
    FindPointersToNewSpaceOnPage(
        reinterpret_cast<PagedSpace*>(page->owner()),
        page,
        region_callback,
        &DummyScavengePointer);
  }
}


void StoreBuffer::VerifyPointers(LargeObjectSpace* space) {
  LargeObjectIterator it(space);
  for (HeapObject* object = it.Next(); object != NULL; object = it.Next()) {
    if (object->IsFixedArray()) {
      Address slot_address = object->address();
      Address end = object->address() + object->Size();

      while (slot_address < end) {
        HeapObject** slot = reinterpret_cast<HeapObject**>(slot_address);
        // When we are not in GC the Heap::InNewSpace() predicate
        // checks that pointers which satisfy predicate point into
        // the active semispace.
        heap_->InNewSpace(*slot);
        slot_address += kPointerSize;
      }
    }
  }
}
#endif


void StoreBuffer::Verify() {
#ifdef VERIFY_HEAP
  VerifyPointers(heap_->old_pointer_space(),
                 &StoreBuffer::FindPointersToNewSpaceInRegion);
  VerifyPointers(heap_->map_space(),
                 &StoreBuffer::FindPointersToNewSpaceInMapsRegion);
  VerifyPointers(heap_->lo_space());
#endif
}


void StoreBuffer::GCEpilogue() {
  during_gc_ = false;
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    Verify();
  }
#endif
}


void StoreBuffer::FindPointersToNewSpaceInRegion(
    Address start, Address end, ObjectSlotCallback slot_callback) {
  for (Address slot_address = start;
       slot_address < end;
       slot_address += kPointerSize) {
    Object** slot = reinterpret_cast<Object**>(slot_address);
    if (heap_->InNewSpace(*slot)) {
      HeapObject* object = reinterpret_cast<HeapObject*>(*slot);
      ASSERT(object->IsHeapObject());
      slot_callback(reinterpret_cast<HeapObject**>(slot), object);
      if (heap_->InNewSpace(*slot)) {
        EnterDirectlyIntoStoreBuffer(slot_address);
      }
    }
  }
}


// Compute start address of the first map following given addr.
static inline Address MapStartAlign(Address addr) {
  Address page = Page::FromAddress(addr)->area_start();
  return page + (((addr - page) + (Map::kSize - 1)) / Map::kSize * Map::kSize);
}


// Compute end address of the first map preceding given addr.
static inline Address MapEndAlign(Address addr) {
  Address page = Page::FromAllocationTop(addr)->area_start();
  return page + ((addr - page) / Map::kSize * Map::kSize);
}


void StoreBuffer::FindPointersToNewSpaceInMaps(
    Address start,
    Address end,
    ObjectSlotCallback slot_callback) {
  ASSERT(MapStartAlign(start) == start);
  ASSERT(MapEndAlign(end) == end);

  Address map_address = start;
  while (map_address < end) {
    ASSERT(!heap_->InNewSpace(Memory::Object_at(map_address)));
    ASSERT(Memory::Object_at(map_address)->IsMap());

    Address pointer_fields_start = map_address + Map::kPointerFieldsBeginOffset;
    Address pointer_fields_end = map_address + Map::kPointerFieldsEndOffset;

    FindPointersToNewSpaceInRegion(pointer_fields_start,
                                   pointer_fields_end,
                                   slot_callback);
    map_address += Map::kSize;
  }
}


void StoreBuffer::FindPointersToNewSpaceInMapsRegion(
    Address start,
    Address end,
    ObjectSlotCallback slot_callback) {
  Address map_aligned_start = MapStartAlign(start);
  Address map_aligned_end   = MapEndAlign(end);

  ASSERT(map_aligned_start == start);
  ASSERT(map_aligned_end == end);

  FindPointersToNewSpaceInMaps(map_aligned_start,
                               map_aligned_end,
                               slot_callback);
}


// This function iterates over all the pointers in a paged space in the heap,
// looking for pointers into new space.  Within the pages there may be dead
// objects that have not been overwritten by free spaces or fillers because of
// lazy sweeping.  These dead objects may not contain pointers to new space.
// The garbage areas that have been swept properly (these will normally be the
// large ones) will be marked with free space and filler map words.  In
// addition any area that has never been used at all for object allocation must
// be marked with a free space or filler.  Because the free space and filler
// maps do not move we can always recognize these even after a compaction.
// Normal objects like FixedArrays and JSObjects should not contain references
// to these maps.  The special garbage section (see comment in spaces.h) is
// skipped since it can contain absolutely anything.  Any objects that are
// allocated during iteration may or may not be visited by the iteration, but
// they will not be partially visited.
void StoreBuffer::FindPointersToNewSpaceOnPage(
    PagedSpace* space,
    Page* page,
    RegionCallback region_callback,
    ObjectSlotCallback slot_callback) {
  Address visitable_start = page->area_start();
  Address end_of_page = page->area_end();

  Address visitable_end = visitable_start;

  Object* free_space_map = heap_->free_space_map();
  Object* two_pointer_filler_map = heap_->two_pointer_filler_map();

  while (visitable_end < end_of_page) {
    Object* o = *reinterpret_cast<Object**>(visitable_end);
    // Skip fillers but not things that look like fillers in the special
    // garbage section which can contain anything.
    if (o == free_space_map ||
        o == two_pointer_filler_map ||
        (visitable_end == space->top() && visitable_end != space->limit())) {
      if (visitable_start != visitable_end) {
        // After calling this the special garbage section may have moved.
        (this->*region_callback)(visitable_start,
                                 visitable_end,
                                 slot_callback);
        if (visitable_end >= space->top() && visitable_end < space->limit()) {
          visitable_end = space->limit();
          visitable_start = visitable_end;
          continue;
        }
      }
      if (visitable_end == space->top() && visitable_end != space->limit()) {
        visitable_start = visitable_end = space->limit();
      } else {
        // At this point we are either at the start of a filler or we are at
        // the point where the space->top() used to be before the
        // visit_pointer_region call above.  Either way we can skip the
        // object at the current spot:  We don't promise to visit objects
        // allocated during heap traversal, and if space->top() moved then it
        // must be because an object was allocated at this point.
        visitable_start =
            visitable_end + HeapObject::FromAddress(visitable_end)->Size();
        visitable_end = visitable_start;
      }
    } else {
      ASSERT(o != free_space_map);
      ASSERT(o != two_pointer_filler_map);
      ASSERT(visitable_end < space->top() || visitable_end >= space->limit());
      visitable_end += kPointerSize;
    }
  }
  ASSERT(visitable_end == end_of_page);
  if (visitable_start != visitable_end) {
    (this->*region_callback)(visitable_start,
                             visitable_end,
                             slot_callback);
  }
}


void StoreBuffer::IteratePointersInStoreBuffer(
    ObjectSlotCallback slot_callback) {
  Address* limit = old_top_;
  old_top_ = old_start_;
  {
    DontMoveStoreBufferEntriesScope scope(this);
    for (Address* current = old_start_; current < limit; current++) {
#ifdef DEBUG
      Address* saved_top = old_top_;
#endif
      Object** slot = reinterpret_cast<Object**>(*current);
      Object* object = *slot;
      if (heap_->InFromSpace(object)) {
        HeapObject* heap_object = reinterpret_cast<HeapObject*>(object);
        slot_callback(reinterpret_cast<HeapObject**>(slot), heap_object);
        if (heap_->InNewSpace(*slot)) {
          EnterDirectlyIntoStoreBuffer(reinterpret_cast<Address>(slot));
        }
      }
      ASSERT(old_top_ == saved_top + 1 || old_top_ == saved_top);
    }
  }
}


void StoreBuffer::IteratePointersToNewSpace(ObjectSlotCallback slot_callback) {
  // We do not sort or remove duplicated entries from the store buffer because
  // we expect that callback will rebuild the store buffer thus removing
  // all duplicates and pointers to old space.
  bool some_pages_to_scan = PrepareForIteration();

  // TODO(gc): we want to skip slots on evacuation candidates
  // but we can't simply figure that out from slot address
  // because slot can belong to a large object.
  IteratePointersInStoreBuffer(slot_callback);

  // We are done scanning all the pointers that were in the store buffer, but
  // there may be some pages marked scan_on_scavenge that have pointers to new
  // space that are not in the store buffer.  We must scan them now.  As we
  // scan, the surviving pointers to new space will be added to the store
  // buffer.  If there are still a lot of pointers to new space then we will
  // keep the scan_on_scavenge flag on the page and discard the pointers that
  // were added to the store buffer.  If there are not many pointers to new
  // space left on the page we will keep the pointers in the store buffer and
  // remove the flag from the page.
  if (some_pages_to_scan) {
    if (callback_ != NULL) {
      (*callback_)(heap_, NULL, kStoreBufferStartScanningPagesEvent);
    }
    PointerChunkIterator it(heap_);
    MemoryChunk* chunk;
    while ((chunk = it.next()) != NULL) {
      if (chunk->scan_on_scavenge()) {
        chunk->set_scan_on_scavenge(false);
        if (callback_ != NULL) {
          (*callback_)(heap_, chunk, kStoreBufferScanningPageEvent);
        }
        if (chunk->owner() == heap_->lo_space()) {
          LargePage* large_page = reinterpret_cast<LargePage*>(chunk);
          HeapObject* array = large_page->GetObject();
          ASSERT(array->IsFixedArray());
          Address start = array->address();
          Address end = start + array->Size();
          FindPointersToNewSpaceInRegion(start, end, slot_callback);
        } else {
          Page* page = reinterpret_cast<Page*>(chunk);
          PagedSpace* owner = reinterpret_cast<PagedSpace*>(page->owner());
          FindPointersToNewSpaceOnPage(
              owner,
              page,
              (owner == heap_->map_space() ?
                 &StoreBuffer::FindPointersToNewSpaceInMapsRegion :
                 &StoreBuffer::FindPointersToNewSpaceInRegion),
              slot_callback);
        }
      }
    }
    if (callback_ != NULL) {
      (*callback_)(heap_, NULL, kStoreBufferScanningPageEvent);
    }
  }
}


void StoreBuffer::Compact() {
  Address* top = reinterpret_cast<Address*>(heap_->store_buffer_top());

  if (top == start_) return;

  // There's no check of the limit in the loop below so we check here for
  // the worst case (compaction doesn't eliminate any pointers).
  ASSERT(top <= limit_);
  heap_->public_set_store_buffer_top(start_);
  EnsureSpace(top - start_);
  ASSERT(may_move_store_buffer_entries_);
  // Goes through the addresses in the store buffer attempting to remove
  // duplicates.  In the interest of speed this is a lossy operation.  Some
  // duplicates will remain.  We have two hash sets with different hash
  // functions to reduce the number of unnecessary clashes.
  hash_sets_are_empty_ = false;  // Hash sets are in use.
  for (Address* current = start_; current < top; current++) {
    ASSERT(!heap_->cell_space()->Contains(*current));
    ASSERT(!heap_->code_space()->Contains(*current));
    ASSERT(!heap_->old_data_space()->Contains(*current));
    uintptr_t int_addr = reinterpret_cast<uintptr_t>(*current);
    // Shift out the last bits including any tags.
    int_addr >>= kPointerSizeLog2;
    int hash1 =
        ((int_addr ^ (int_addr >> kHashSetLengthLog2)) & (kHashSetLength - 1));
    if (hash_set_1_[hash1] == int_addr) continue;
    uintptr_t hash2 = (int_addr - (int_addr >> kHashSetLengthLog2));
    hash2 ^= hash2 >> (kHashSetLengthLog2 * 2);
    hash2 &= (kHashSetLength - 1);
    if (hash_set_2_[hash2] == int_addr) continue;
    if (hash_set_1_[hash1] == 0) {
      hash_set_1_[hash1] = int_addr;
    } else if (hash_set_2_[hash2] == 0) {
      hash_set_2_[hash2] = int_addr;
    } else {
      // Rather than slowing down we just throw away some entries.  This will
      // cause some duplicates to remain undetected.
      hash_set_1_[hash1] = int_addr;
      hash_set_2_[hash2] = 0;
    }
    old_buffer_is_sorted_ = false;
    old_buffer_is_filtered_ = false;
    *old_top_++ = reinterpret_cast<Address>(int_addr << kPointerSizeLog2);
    ASSERT(old_top_ <= old_limit_);
  }
  heap_->isolate()->counters()->store_buffer_compactions()->Increment();
  CheckForFullBuffer();
}


void StoreBuffer::CheckForFullBuffer() {
  EnsureSpace(kStoreBufferSize * 2);
}

} }  // namespace v8::internal

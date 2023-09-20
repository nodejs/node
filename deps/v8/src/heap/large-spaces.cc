// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/large-spaces.h"

#include "src/base/platform/mutex.h"
#include "src/base/sanitizer/msan.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap-verifier.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/list.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/marking.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk-inl.h"
#include "src/heap/remembered-set.h"
#include "src/heap/slot-set.h"
#include "src/heap/spaces-inl.h"
#include "src/logging/log.h"
#include "src/objects/objects-inl.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

// This check is here to ensure that the lower 32 bits of any real heap object
// can't overlap with the lower 32 bits of cleared weak reference value and
// therefore it's enough to compare only the lower 32 bits of a MaybeObject in
// order to figure out if it's a cleared weak reference or not.
static_assert(kClearedWeakHeapObjectLower32 < LargePage::kHeaderSize);

LargePage::LargePage(Heap* heap, BaseSpace* space, size_t chunk_size,
                     Address area_start, Address area_end,
                     VirtualMemory reservation, Executability executable)
    : MemoryChunk(heap, space, chunk_size, area_start, area_end,
                  std::move(reservation), executable, PageSize::kLarge) {
  static_assert(LargePage::kMaxCodePageSize <= TypedSlotSet::kMaxOffset);

  if (executable && chunk_size > LargePage::kMaxCodePageSize) {
    FATAL("Code page is too large.");
  }

  SetFlag(MemoryChunk::LARGE_PAGE);
  list_node().Initialize();
}

LargePage* LargePage::Initialize(Heap* heap, MemoryChunk* chunk,
                                 Executability executable) {
  if (executable && chunk->size() > LargePage::kMaxCodePageSize) {
    static_assert(LargePage::kMaxCodePageSize <= TypedSlotSet::kMaxOffset);
    FATAL("Code page is too large.");
  }

  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(chunk->area_start(), chunk->area_size());

  LargePage* page = static_cast<LargePage*>(chunk);
  page->SetFlag(MemoryChunk::LARGE_PAGE);
  page->list_node().Initialize();
  return page;
}

size_t LargeObjectSpace::Available() const {
  // We return zero here since we cannot take advantage of already allocated
  // large object memory.
  return 0;
}

void LargePage::ClearOutOfLiveRangeSlots(Address free_start) {
  DCHECK_NULL(slot_set<OLD_TO_NEW>());
  DCHECK_NULL(typed_slot_set<OLD_TO_NEW>());

  DCHECK_NULL(slot_set<OLD_TO_OLD>());
  DCHECK_NULL(typed_slot_set<OLD_TO_OLD>());

  // area_end() might not be aligned to a full bucket size with large objects.
  // Align it to bucket size such that the following RemoveRange invocation just
  // drops the whole bucket and the bucket is reset to nullptr.
  Address aligned_area_end = address() + SlotSet::OffsetForBucket(buckets());
  DCHECK_LE(area_end(), aligned_area_end);
  RememberedSet<OLD_TO_SHARED>::RemoveRange(this, free_start, aligned_area_end,
                                            SlotSet::FREE_EMPTY_BUCKETS);

  RememberedSet<OLD_TO_SHARED>::RemoveRangeTyped(this, free_start, area_end());
}

// -----------------------------------------------------------------------------
// LargeObjectSpaceObjectIterator

LargeObjectSpaceObjectIterator::LargeObjectSpaceObjectIterator(
    LargeObjectSpace* space) {
  current_ = space->first_page();
}

HeapObject LargeObjectSpaceObjectIterator::Next() {
  if (current_ == nullptr) return HeapObject();

  HeapObject object = current_->GetObject();
  current_ = current_->next_page();
  return object;
}

// -----------------------------------------------------------------------------
// OldLargeObjectSpace

LargeObjectSpace::LargeObjectSpace(Heap* heap, AllocationSpace id)
    : Space(heap, id, new NoFreeList(), allocation_counter_),
      size_(0),
      page_count_(0),
      objects_size_(0),
      pending_object_(0) {}

void LargeObjectSpace::TearDown() {
  while (!memory_chunk_list_.Empty()) {
    LargePage* page = first_page();
    LOG(heap()->isolate(),
        DeleteEvent("LargeObjectChunk",
                    reinterpret_cast<void*>(page->address())));
    memory_chunk_list_.Remove(page);
    heap()->memory_allocator()->Free(MemoryAllocator::FreeMode::kImmediately,
                                     page);
  }
}

void LargeObjectSpace::AdvanceAndInvokeAllocationObservers(Address soon_object,
                                                           size_t object_size) {
  if (!heap()->IsAllocationObserverActive()) return;

  if (object_size >= allocation_counter_.NextBytes()) {
    allocation_counter_.InvokeAllocationObservers(soon_object, object_size,
                                                  object_size);
  }

  // Large objects can be accounted immediately since no LAB is involved.
  allocation_counter_.AdvanceAllocationObservers(object_size);
}

AllocationResult OldLargeObjectSpace::AllocateRaw(int object_size) {
  return AllocateRaw(object_size, NOT_EXECUTABLE);
}

AllocationResult OldLargeObjectSpace::AllocateRaw(int object_size,
                                                  Executability executable) {
  object_size = ALIGN_TO_ALLOCATION_ALIGNMENT(object_size);
  DCHECK(!v8_flags.enable_third_party_heap);
  // Check if we want to force a GC before growing the old space further.
  // If so, fail the allocation.
  if (!heap()->CanExpandOldGeneration(object_size) ||
      !heap()->ShouldExpandOldGenerationOnSlowAllocation(
          heap()->main_thread_local_heap(), AllocationOrigin::kRuntime)) {
    return AllocationResult::Failure();
  }

  LargePage* page = AllocateLargePage(object_size, executable);
  if (page == nullptr) return AllocationResult::Failure();
  page->SetOldGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  HeapObject object = page->GetObject();
  UpdatePendingObject(object);
  heap()->StartIncrementalMarkingIfAllocationLimitIsReached(
      heap()->GCFlagsForIncrementalMarking(),
      kGCCallbackScheduleIdleGarbageCollection);
  if (heap()->incremental_marking()->black_allocation()) {
    heap()->marking_state()->TryMarkAndAccountLiveBytes(object);
  }
  DCHECK_IMPLIES(heap()->incremental_marking()->black_allocation(),
                 heap()->marking_state()->IsMarked(object));
  page->InitializationMemoryFence();
  heap()->NotifyOldGenerationExpansion(identity(), page);
  AdvanceAndInvokeAllocationObservers(object.address(),
                                      static_cast<size_t>(object_size));
  return AllocationResult::FromObject(object);
}

AllocationResult OldLargeObjectSpace::AllocateRawBackground(
    LocalHeap* local_heap, int object_size) {
  return AllocateRawBackground(local_heap, object_size, NOT_EXECUTABLE);
}

AllocationResult OldLargeObjectSpace::AllocateRawBackground(
    LocalHeap* local_heap, int object_size, Executability executable) {
  object_size = ALIGN_TO_ALLOCATION_ALIGNMENT(object_size);
  DCHECK(!v8_flags.enable_third_party_heap);
  // Check if we want to force a GC before growing the old space further.
  // If so, fail the allocation.
  if (!heap()->CanExpandOldGenerationBackground(local_heap, object_size) ||
      !heap()->ShouldExpandOldGenerationOnSlowAllocation(
          local_heap, AllocationOrigin::kRuntime)) {
    return AllocationResult::Failure();
  }

  LargePage* page = AllocateLargePage(object_size, executable);
  if (page == nullptr) return AllocationResult::Failure();
  page->SetOldGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  HeapObject object = page->GetObject();
  heap()->StartIncrementalMarkingIfAllocationLimitIsReachedBackground();
  if (heap()->incremental_marking()->black_allocation()) {
    heap()->marking_state()->TryMarkAndAccountLiveBytes(object);
  }
  DCHECK_IMPLIES(heap()->incremental_marking()->black_allocation(),
                 heap()->marking_state()->IsMarked(object));
  page->InitializationMemoryFence();
  if (identity() == CODE_LO_SPACE) {
    heap()->isolate()->AddCodeMemoryChunk(page);
  }
  return AllocationResult::FromObject(object);
}

LargePage* LargeObjectSpace::AllocateLargePage(int object_size,
                                               Executability executable) {
  LargePage* page = heap()->memory_allocator()->AllocateLargePage(
      this, object_size, executable);
  if (page == nullptr) return nullptr;
  DCHECK_GE(page->area_size(), static_cast<size_t>(object_size));

  {
    base::RecursiveMutexGuard guard(&allocation_mutex_);
    AddPage(page, object_size);
  }

  HeapObject object = page->GetObject();

  heap()->CreateFillerObjectAt(object.address(), object_size);
  return page;
}

size_t LargeObjectSpace::CommittedPhysicalMemory() const {
  // On a platform that provides lazy committing of memory, we over-account
  // the actually committed memory. There is no easy way right now to support
  // precise accounting of committed memory in large object space.
  return CommittedMemory();
}

LargePage* CodeLargeObjectSpace::FindPage(Address a) {
  base::RecursiveMutexGuard guard(&allocation_mutex_);
  const Address key = BasicMemoryChunk::FromAddress(a)->address();
  auto it = chunk_map_.find(key);
  if (it != chunk_map_.end()) {
    LargePage* page = it->second;
    CHECK(page->Contains(a));
    return page;
  }
  return nullptr;
}

void CodeLargeObjectSpace::InsertChunkMapEntries(LargePage* page) {
  for (Address current = reinterpret_cast<Address>(page);
       current < reinterpret_cast<Address>(page) + page->size();
       current += MemoryChunk::kPageSize) {
    chunk_map_[current] = page;
  }
}

void CodeLargeObjectSpace::RemoveChunkMapEntries(LargePage* page) {
  for (Address current = page->address();
       current < reinterpret_cast<Address>(page) + page->size();
       current += MemoryChunk::kPageSize) {
    chunk_map_.erase(current);
  }
}

void OldLargeObjectSpace::PromoteNewLargeObject(LargePage* page) {
  DCHECK_EQ(page->owner_identity(), NEW_LO_SPACE);
  DCHECK(page->IsLargePage());
  DCHECK(page->IsFlagSet(MemoryChunk::FROM_PAGE));
  DCHECK(!page->IsFlagSet(MemoryChunk::TO_PAGE));
  PtrComprCageBase cage_base(heap()->isolate());
  static_cast<LargeObjectSpace*>(page->owner())->RemovePage(page);
  page->ClearFlag(MemoryChunk::FROM_PAGE);
  AddPage(page, static_cast<size_t>(page->GetObject().Size(cage_base)));
}

void LargeObjectSpace::AddPage(LargePage* page, size_t object_size) {
  size_ += static_cast<int>(page->size());
  AccountCommitted(page->size());
  objects_size_ += object_size;
  page_count_++;
  memory_chunk_list_.PushBack(page);
  page->set_owner(this);
  page->SetOldGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    IncrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
}

void LargeObjectSpace::RemovePage(LargePage* page) {
  size_ -= static_cast<int>(page->size());
  AccountUncommitted(page->size());
  page_count_--;
  memory_chunk_list_.Remove(page);
  page->set_owner(nullptr);
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    DecrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
}

void LargeObjectSpace::ShrinkPageToObjectSize(LargePage* page,
                                              HeapObject object,
                                              size_t object_size) {
#ifdef DEBUG
  PtrComprCageBase cage_base(heap()->isolate());
  DCHECK_EQ(object, page->GetObject());
  DCHECK_EQ(object_size, page->GetObject().Size(cage_base));
  DCHECK_EQ(page->executable(), NOT_EXECUTABLE);
#endif  // DEBUG

  const size_t used_committed_size =
      ::RoundUp(object.address() - page->address() + object_size,
                MemoryAllocator::GetCommitPageSize());

  // Object shrunk since last GC.
  if (object_size < page->area_size()) {
    page->ClearOutOfLiveRangeSlots(object.address() + object_size);
    const Address new_area_end = page->area_start() + object_size;

    // Object shrunk enough that we can even free some OS pages.
    if (used_committed_size < page->size()) {
      const size_t bytes_to_free = page->size() - used_committed_size;
      heap()->memory_allocator()->PartialFreeMemory(
          page, page->address() + used_committed_size, bytes_to_free,
          new_area_end);
      size_ -= bytes_to_free;
      AccountUncommitted(bytes_to_free);
    } else {
      // Can't free OS page but keep object area up-to-date.
      page->set_area_end(new_area_end);
    }
  }

  DCHECK_EQ(used_committed_size, page->size());
  DCHECK_EQ(object_size, page->area_size());
}

bool LargeObjectSpace::Contains(HeapObject object) const {
  BasicMemoryChunk* chunk = BasicMemoryChunk::FromHeapObject(object);

  bool owned = (chunk->owner() == this);

  SLOW_DCHECK(!owned || ContainsSlow(object.address()));

  return owned;
}

bool LargeObjectSpace::ContainsSlow(Address addr) const {
  for (const LargePage* page : *this) {
    if (page->Contains(addr)) return true;
  }
  return false;
}

std::unique_ptr<ObjectIterator> LargeObjectSpace::GetObjectIterator(
    Heap* heap) {
  return std::unique_ptr<ObjectIterator>(
      new LargeObjectSpaceObjectIterator(this));
}

#ifdef VERIFY_HEAP
// We do not assume that the large object iterator works, because it depends
// on the invariants we are checking during verification.
void LargeObjectSpace::Verify(Isolate* isolate,
                              SpaceVerificationVisitor* visitor) const {
  size_t external_backing_store_bytes[kNumTypes];

  for (int i = 0; i < kNumTypes; i++) {
    external_backing_store_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  PtrComprCageBase cage_base(isolate);
  for (const LargePage* chunk = first_page(); chunk != nullptr;
       chunk = chunk->next_page()) {
    visitor->VerifyPage(chunk);

    // Each chunk contains an object that starts at the large object page's
    // object area start.
    HeapObject object = chunk->GetObject();
    Page* page = Page::FromHeapObject(object);
    CHECK(object.address() == page->area_start());

    // We have only the following types in the large object space:
    const bool is_valid_lo_space_object =                         //
        object.IsAbstractCode(cage_base) ||                       //
        object.IsBigInt(cage_base) ||                             //
        object.IsByteArray(cage_base) ||                          //
        object.IsContext(cage_base) ||                            //
        object.IsExternalString(cage_base) ||                     //
        object.IsFeedbackMetadata(cage_base) ||                   //
        object.IsFeedbackVector(cage_base) ||                     //
        object.IsFixedArray(cage_base) ||                         //
        object.IsFixedDoubleArray(cage_base) ||                   //
        object.IsFreeSpace(cage_base) ||                          //
        object.IsInstructionStream(cage_base) ||                  //
        object.IsPreparseData(cage_base) ||                       //
        object.IsPropertyArray(cage_base) ||                      //
        object.IsScopeInfo() ||                                   //
        object.IsSeqString(cage_base) ||                          //
        object.IsSloppyArgumentsElements(cage_base) ||            //
        object.IsSwissNameDictionary() ||                         //
        object.IsThinString(cage_base) ||                         //
        object.IsUncompiledDataWithoutPreparseData(cage_base) ||  //
#if V8_ENABLE_WEBASSEMBLY                                         //
        object.IsWasmArray() ||                                   //
#endif                                                            //
        object.IsWeakArrayList(cage_base) ||                      //
        object.IsWeakFixedArray(cage_base);
    if (!is_valid_lo_space_object) {
      object.Print();
      FATAL("Found invalid Object (instance_type=%i) in large object space.",
            object.map(cage_base).instance_type());
    }

    // Invoke visitor on each object.
    visitor->VerifyObject(object);

    for (int i = 0; i < kNumTypes; i++) {
      ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
      external_backing_store_bytes[t] += chunk->ExternalBackingStoreBytes(t);
    }

    visitor->VerifyPageDone(chunk);
  }
  for (int i = 0; i < kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    CHECK_EQ(external_backing_store_bytes[t], ExternalBackingStoreBytes(t));
  }
}
#endif

#ifdef DEBUG
void LargeObjectSpace::Print() {
  StdoutStream os;
  LargeObjectSpaceObjectIterator it(this);
  for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
    obj.Print(os);
  }
}
#endif  // DEBUG

void LargeObjectSpace::UpdatePendingObject(HeapObject object) {
  base::SharedMutexGuard<base::kExclusive> guard(&pending_allocation_mutex_);
  pending_object_.store(object.address(), std::memory_order_release);
}

OldLargeObjectSpace::OldLargeObjectSpace(Heap* heap)
    : LargeObjectSpace(heap, LO_SPACE) {}

OldLargeObjectSpace::OldLargeObjectSpace(Heap* heap, AllocationSpace id)
    : LargeObjectSpace(heap, id) {}

NewLargeObjectSpace::NewLargeObjectSpace(Heap* heap, size_t capacity)
    : LargeObjectSpace(heap, NEW_LO_SPACE), capacity_(capacity) {}

AllocationResult NewLargeObjectSpace::AllocateRaw(int object_size) {
  object_size = ALIGN_TO_ALLOCATION_ALIGNMENT(object_size);
  DCHECK(!v8_flags.enable_third_party_heap);
  // Do not allocate more objects if promoting the existing object would exceed
  // the old generation capacity.
  if (!heap()->CanExpandOldGeneration(SizeOfObjects())) {
    return AllocationResult::Failure();
  }

  // Allocation for the first object must succeed independent from the capacity.
  if (SizeOfObjects() > 0 && static_cast<size_t>(object_size) > Available()) {
    return AllocationResult::Failure();
  }

  LargePage* page = AllocateLargePage(object_size, NOT_EXECUTABLE);
  if (page == nullptr) return AllocationResult::Failure();

  // The size of the first object may exceed the capacity.
  capacity_ = std::max(capacity_, SizeOfObjects());

  HeapObject result = page->GetObject();
  page->SetYoungGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  page->SetFlag(MemoryChunk::TO_PAGE);
  UpdatePendingObject(result);
  if (v8_flags.minor_mc) {
    heap()->non_atomic_marking_state()->ClearLiveness(page);
  }
  page->InitializationMemoryFence();
  DCHECK(page->IsLargePage());
  DCHECK_EQ(page->owner_identity(), NEW_LO_SPACE);
  AdvanceAndInvokeAllocationObservers(result.address(),
                                      static_cast<size_t>(object_size));
  return AllocationResult::FromObject(result);
}

size_t NewLargeObjectSpace::Available() const {
  return capacity_ - SizeOfObjects();
}

void NewLargeObjectSpace::Flip() {
  for (LargePage* chunk = first_page(); chunk != nullptr;
       chunk = chunk->next_page()) {
    chunk->SetFlag(MemoryChunk::FROM_PAGE);
    chunk->ClearFlag(MemoryChunk::TO_PAGE);
  }
}

void NewLargeObjectSpace::FreeDeadObjects(
    const std::function<bool(HeapObject)>& is_dead) {
  bool is_marking = heap()->incremental_marking()->IsMarking();
  size_t surviving_object_size = 0;
  PtrComprCageBase cage_base(heap()->isolate());
  for (auto it = begin(); it != end();) {
    LargePage* page = *it;
    it++;
    HeapObject object = page->GetObject();
    if (is_dead(object)) {
      RemovePage(page);
      heap()->memory_allocator()->Free(MemoryAllocator::FreeMode::kConcurrently,
                                       page);
      if (v8_flags.concurrent_marking && is_marking) {
        heap()->concurrent_marking()->ClearMemoryChunkData(page);
      }
    } else {
      surviving_object_size += static_cast<size_t>(object.Size(cage_base));
    }
  }
  // Right-trimming does not update the objects_size_ counter. We are lazily
  // updating it after every GC.
  objects_size_ = surviving_object_size;
}

void NewLargeObjectSpace::SetCapacity(size_t capacity) {
  capacity_ = std::max(capacity, SizeOfObjects());
}

CodeLargeObjectSpace::CodeLargeObjectSpace(Heap* heap)
    : OldLargeObjectSpace(heap, CODE_LO_SPACE),
      chunk_map_(kInitialChunkMapCapacity) {}

AllocationResult CodeLargeObjectSpace::AllocateRaw(int object_size) {
  DCHECK(!v8_flags.enable_third_party_heap);
  return OldLargeObjectSpace::AllocateRaw(object_size, EXECUTABLE);
}

AllocationResult CodeLargeObjectSpace::AllocateRawBackground(
    LocalHeap* local_heap, int object_size) {
  DCHECK(!v8_flags.enable_third_party_heap);
  return OldLargeObjectSpace::AllocateRawBackground(local_heap, object_size,
                                                    EXECUTABLE);
}

void CodeLargeObjectSpace::AddPage(LargePage* page, size_t object_size) {
  OldLargeObjectSpace::AddPage(page, object_size);
  InsertChunkMapEntries(page);
}

void CodeLargeObjectSpace::RemovePage(LargePage* page) {
  RemoveChunkMapEntries(page);
  heap()->isolate()->RemoveCodeMemoryChunk(page);
  OldLargeObjectSpace::RemovePage(page);
}

SharedLargeObjectSpace::SharedLargeObjectSpace(Heap* heap)
    : OldLargeObjectSpace(heap, SHARED_LO_SPACE) {}

AllocationResult SharedLargeObjectSpace::AllocateRawBackground(
    LocalHeap* local_heap, int object_size) {
  DCHECK(!v8_flags.enable_third_party_heap);
  return OldLargeObjectSpace::AllocateRawBackground(local_heap, object_size,
                                                    NOT_EXECUTABLE);
}

}  // namespace internal
}  // namespace v8

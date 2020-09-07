// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/read-only-spaces.h"

#include "include/v8-internal.h"
#include "src/base/lsan.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap-inl.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/objects-inl.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ReadOnlySpace implementation

ReadOnlySpace::ReadOnlySpace(Heap* heap)
    : BaseSpace(heap, RO_SPACE),
      top_(kNullAddress),
      limit_(kNullAddress),
      is_string_padding_cleared_(heap->isolate()->initialized_from_snapshot()),
      capacity_(0),
      area_size_(MemoryChunkLayout::AllocatableMemoryInMemoryChunk(RO_SPACE)) {}

ReadOnlySpace::~ReadOnlySpace() {
  Unseal();
  for (ReadOnlyPage* chunk : pages_) {
    heap()->memory_allocator()->FreeReadOnlyPage(chunk);
  }
  pages_.resize(0);
  accounting_stats_.Clear();
}

ReadOnlyArtifacts::~ReadOnlyArtifacts() {
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();

  for (ReadOnlyPage* chunk : pages_) {
    void* chunk_address = reinterpret_cast<void*>(chunk->address());
    page_allocator->SetPermissions(chunk_address, chunk->size(),
                                   PageAllocator::kReadWrite);
    size_t size = RoundUp(chunk->size(), page_allocator->AllocatePageSize());
    CHECK(page_allocator->FreePages(chunk_address, size));
  }
}

void ReadOnlyArtifacts::set_read_only_heap(
    std::unique_ptr<ReadOnlyHeap> read_only_heap) {
  read_only_heap_ = std::move(read_only_heap);
}

SharedReadOnlySpace::~SharedReadOnlySpace() {
  // Clear the chunk list before the space is deleted, so that the inherited
  // destructors don't try to destroy the BasicMemoryChunks themselves.
  pages_.resize(0);
}

SharedReadOnlySpace::SharedReadOnlySpace(
    Heap* heap, std::shared_ptr<ReadOnlyArtifacts> artifacts)
    : ReadOnlySpace(heap) {
  pages_ = artifacts->pages();
  is_marked_read_only_ = true;
  accounting_stats_ = artifacts->accounting_stats();
  top_ = kNullAddress;
  limit_ = kNullAddress;
}

void ReadOnlySpace::DetachPagesAndAddToArtifacts(
    std::shared_ptr<ReadOnlyArtifacts> artifacts) {
  Heap* heap = ReadOnlySpace::heap();
  Seal(SealMode::kDetachFromHeapAndForget);
  artifacts->set_accounting_stats(accounting_stats_);
  artifacts->TransferPages(std::move(pages_));
  artifacts->set_shared_read_only_space(
      std::make_unique<SharedReadOnlySpace>(heap, artifacts));
  heap->ReplaceReadOnlySpace(artifacts->shared_read_only_space());
}

void ReadOnlyPage::MakeHeaderRelocatable() {
  // Detached read-only space needs to have a valid marking bitmap. Instruct
  // Lsan to ignore it if required.
  LSAN_IGNORE_OBJECT(marking_bitmap_);
  heap_ = nullptr;
  owner_ = nullptr;
}

void ReadOnlySpace::SetPermissionsForPages(MemoryAllocator* memory_allocator,
                                           PageAllocator::Permission access) {
  for (BasicMemoryChunk* chunk : pages_) {
    // Read only pages don't have valid reservation object so we get proper
    // page allocator manually.
    v8::PageAllocator* page_allocator =
        memory_allocator->page_allocator(NOT_EXECUTABLE);
    CHECK(SetPermissions(page_allocator, chunk->address(), chunk->size(),
                         access));
  }
}

// After we have booted, we have created a map which represents free space
// on the heap.  If there was already a free list then the elements on it
// were created with the wrong FreeSpaceMap (normally nullptr), so we need to
// fix them.
void ReadOnlySpace::RepairFreeSpacesAfterDeserialization() {
  BasicMemoryChunk::UpdateHighWaterMark(top_);
  // Each page may have a small free space that is not tracked by a free list.
  // Those free spaces still contain null as their map pointer.
  // Overwrite them with new fillers.
  for (BasicMemoryChunk* chunk : pages_) {
    Address start = chunk->HighWaterMark();
    Address end = chunk->area_end();
    // Put a filler object in the gap between the end of the allocated objects
    // and the end of the allocatable area.
    if (start < end) {
      heap()->CreateFillerObjectAt(start, static_cast<int>(end - start),
                                   ClearRecordedSlots::kNo);
    }
  }
}

void ReadOnlySpace::ClearStringPaddingIfNeeded() {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    // TODO(ulan): Revisit this once third-party heap supports iteration.
    return;
  }
  if (is_string_padding_cleared_) return;

  ReadOnlyHeapObjectIterator iterator(this);
  for (HeapObject o = iterator.Next(); !o.is_null(); o = iterator.Next()) {
    if (o.IsSeqOneByteString()) {
      SeqOneByteString::cast(o).clear_padding();
    } else if (o.IsSeqTwoByteString()) {
      SeqTwoByteString::cast(o).clear_padding();
    }
  }
  is_string_padding_cleared_ = true;
}

void ReadOnlySpace::Seal(SealMode ro_mode) {
  DCHECK(!is_marked_read_only_);

  FreeLinearAllocationArea();
  is_marked_read_only_ = true;
  auto* memory_allocator = heap()->memory_allocator();

  if (ro_mode == SealMode::kDetachFromHeapAndForget) {
    DetachFromHeap();
    for (BasicMemoryChunk* chunk : pages_) {
      memory_allocator->UnregisterMemory(chunk);
      static_cast<ReadOnlyPage*>(chunk)->MakeHeaderRelocatable();
    }
  }

  SetPermissionsForPages(memory_allocator, PageAllocator::kRead);
}

void ReadOnlySpace::Unseal() {
  DCHECK(is_marked_read_only_);
  if (!pages_.empty()) {
    SetPermissionsForPages(heap()->memory_allocator(),
                           PageAllocator::kReadWrite);
  }
  is_marked_read_only_ = false;
}

bool ReadOnlySpace::ContainsSlow(Address addr) {
  BasicMemoryChunk* c = BasicMemoryChunk::FromAddress(addr);
  for (BasicMemoryChunk* chunk : pages_) {
    if (chunk == c) return true;
  }
  return false;
}

namespace {
// Only iterates over a single chunk as the chunk iteration is done externally.
class ReadOnlySpaceObjectIterator : public ObjectIterator {
 public:
  ReadOnlySpaceObjectIterator(Heap* heap, ReadOnlySpace* space,
                              BasicMemoryChunk* chunk)
      : cur_addr_(kNullAddress), cur_end_(kNullAddress), space_(space) {}

  // Advance to the next object, skipping free spaces and other fillers and
  // skipping the special garbage section of which there is one per space.
  // Returns nullptr when the iteration has ended.
  HeapObject Next() override {
    HeapObject next_obj = FromCurrentPage();
    if (!next_obj.is_null()) return next_obj;
    return HeapObject();
  }

 private:
  HeapObject FromCurrentPage() {
    while (cur_addr_ != cur_end_) {
      if (cur_addr_ == space_->top() && cur_addr_ != space_->limit()) {
        cur_addr_ = space_->limit();
        continue;
      }
      HeapObject obj = HeapObject::FromAddress(cur_addr_);
      const int obj_size = obj.Size();
      cur_addr_ += obj_size;
      DCHECK_LE(cur_addr_, cur_end_);
      if (!obj.IsFreeSpaceOrFiller()) {
        if (obj.IsCode()) {
          DCHECK(Code::cast(obj).is_builtin());
          DCHECK_CODEOBJECT_SIZE(obj_size, space_);
        } else {
          DCHECK_OBJECT_SIZE(obj_size);
        }
        return obj;
      }
    }
    return HeapObject();
  }

  Address cur_addr_;  // Current iteration point.
  Address cur_end_;   // End iteration point.
  ReadOnlySpace* space_;
};
}  // namespace

#ifdef VERIFY_HEAP
namespace {
class VerifyReadOnlyPointersVisitor : public VerifyPointersVisitor {
 public:
  explicit VerifyReadOnlyPointersVisitor(Heap* heap)
      : VerifyPointersVisitor(heap) {}

 protected:
  void VerifyPointers(HeapObject host, MaybeObjectSlot start,
                      MaybeObjectSlot end) override {
    if (!host.is_null()) {
      CHECK(ReadOnlyHeap::Contains(host.map()));
    }
    VerifyPointersVisitor::VerifyPointers(host, start, end);

    for (MaybeObjectSlot current = start; current < end; ++current) {
      HeapObject heap_object;
      if ((*current)->GetHeapObject(&heap_object)) {
        CHECK(ReadOnlyHeap::Contains(heap_object));
      }
    }
  }
};
}  // namespace

void ReadOnlySpace::Verify(Isolate* isolate) {
  bool allocation_pointer_found_in_space = top_ == limit_;
  VerifyReadOnlyPointersVisitor visitor(isolate->heap());

  for (BasicMemoryChunk* page : pages_) {
#ifdef V8_SHARED_RO_HEAP
    CHECK_NULL(page->owner());
#else
    CHECK_EQ(page->owner(), this);
#endif

    if (page == Page::FromAllocationAreaAddress(top_)) {
      allocation_pointer_found_in_space = true;
    }
    ReadOnlySpaceObjectIterator it(isolate->heap(), this, page);
    Address end_of_previous_object = page->area_start();
    Address top = page->area_end();

    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      CHECK(end_of_previous_object <= object.address());

      Map map = object.map();
      CHECK(map.IsMap());

      // The object itself should look OK.
      object.ObjectVerify(isolate);

      // All the interior pointers should be contained in the heap.
      int size = object.Size();
      object.IterateBody(map, size, &visitor);
      CHECK(object.address() + size <= top);
      end_of_previous_object = object.address() + size;

      CHECK(!object.IsExternalString());
      CHECK(!object.IsJSArrayBuffer());
    }
  }
  CHECK(allocation_pointer_found_in_space);

#ifdef DEBUG
  VerifyCounters(isolate->heap());
#endif
}

#ifdef DEBUG
void ReadOnlySpace::VerifyCounters(Heap* heap) {
  size_t total_capacity = 0;
  size_t total_allocated = 0;
  for (BasicMemoryChunk* page : pages_) {
    total_capacity += page->area_size();
    ReadOnlySpaceObjectIterator it(heap, this, page);
    size_t real_allocated = 0;
    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      if (!object.IsFreeSpaceOrFiller()) {
        real_allocated += object.Size();
      }
    }
    total_allocated += page->allocated_bytes();
    // The real size can be smaller than the accounted size if array trimming,
    // object slack tracking happened after sweeping.
    DCHECK_LE(real_allocated, accounting_stats_.AllocatedOnPage(page));
    DCHECK_EQ(page->allocated_bytes(), accounting_stats_.AllocatedOnPage(page));
  }
  DCHECK_EQ(total_capacity, accounting_stats_.Capacity());
  DCHECK_EQ(total_allocated, accounting_stats_.Size());
}
#endif  // DEBUG
#endif  // VERIFY_HEAP

size_t ReadOnlySpace::CommittedPhysicalMemory() {
  if (!base::OS::HasLazyCommits()) return CommittedMemory();
  BasicMemoryChunk::UpdateHighWaterMark(top_);
  size_t size = 0;
  for (auto* chunk : pages_) {
    size += chunk->size();
  }

  return size;
}

void ReadOnlySpace::FreeLinearAllocationArea() {
  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.
  if (top_ == kNullAddress) {
    DCHECK_EQ(kNullAddress, limit_);
    return;
  }

  // Clear the bits in the unused black area.
  ReadOnlyPage* page = pages_.back();
  heap()->incremental_marking()->marking_state()->bitmap(page)->ClearRange(
      page->AddressToMarkbitIndex(top_), page->AddressToMarkbitIndex(limit_));

  heap()->CreateFillerObjectAt(top_, static_cast<int>(limit_ - top_),
                               ClearRecordedSlots::kNo);

  BasicMemoryChunk::UpdateHighWaterMark(top_);

  top_ = kNullAddress;
  limit_ = kNullAddress;
}

void ReadOnlySpace::EnsureSpaceForAllocation(int size_in_bytes) {
  if (top_ + size_in_bytes <= limit_) {
    return;
  }

  DCHECK_GE(size_in_bytes, 0);

  FreeLinearAllocationArea();

  BasicMemoryChunk* chunk =
      heap()->memory_allocator()->AllocateReadOnlyPage(AreaSize(), this);
  capacity_ += AreaSize();

  accounting_stats_.IncreaseCapacity(chunk->area_size());
  AccountCommitted(chunk->size());
  CHECK_NOT_NULL(chunk);
  pages_.push_back(static_cast<ReadOnlyPage*>(chunk));

  heap()->CreateFillerObjectAt(chunk->area_start(),
                               static_cast<int>(chunk->area_size()),
                               ClearRecordedSlots::kNo);

  top_ = chunk->area_start();
  limit_ = chunk->area_end();
  return;
}

HeapObject ReadOnlySpace::TryAllocateLinearlyAligned(
    int size_in_bytes, AllocationAlignment alignment) {
  Address current_top = top_;
  int filler_size = Heap::GetFillToAlign(current_top, alignment);

  Address new_top = current_top + filler_size + size_in_bytes;
  if (new_top > limit_) return HeapObject();

  top_ = new_top;
  if (filler_size > 0) {
    return Heap::PrecedeWithFiller(ReadOnlyRoots(heap()),
                                   HeapObject::FromAddress(current_top),
                                   filler_size);
  }

  // Allocation always occurs in the last chunk for RO_SPACE.
  BasicMemoryChunk* chunk = pages_.back();
  int allocated_size = filler_size + size_in_bytes;
  accounting_stats_.IncreaseAllocatedBytes(allocated_size, chunk);
  chunk->IncreaseAllocatedBytes(allocated_size);

  return HeapObject::FromAddress(current_top);
}

AllocationResult ReadOnlySpace::AllocateRawAligned(
    int size_in_bytes, AllocationAlignment alignment) {
  DCHECK(!IsDetached());
  int allocation_size = size_in_bytes;

  HeapObject object = TryAllocateLinearlyAligned(allocation_size, alignment);
  if (object.is_null()) {
    // We don't know exactly how much filler we need to align until space is
    // allocated, so assume the worst case.
    EnsureSpaceForAllocation(allocation_size +
                             Heap::GetMaximumFillToAlign(alignment));
    allocation_size = size_in_bytes;
    object = TryAllocateLinearlyAligned(size_in_bytes, alignment);
    CHECK(!object.is_null());
  }
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object.address(), size_in_bytes);

  return object;
}

AllocationResult ReadOnlySpace::AllocateRawUnaligned(int size_in_bytes) {
  DCHECK(!IsDetached());
  EnsureSpaceForAllocation(size_in_bytes);
  Address current_top = top_;
  Address new_top = current_top + size_in_bytes;
  DCHECK_LE(new_top, limit_);
  top_ = new_top;
  HeapObject object = HeapObject::FromAddress(current_top);

  DCHECK(!object.is_null());
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object.address(), size_in_bytes);

  // Allocation always occurs in the last chunk for RO_SPACE.
  BasicMemoryChunk* chunk = pages_.back();
  accounting_stats_.IncreaseAllocatedBytes(size_in_bytes, chunk);
  chunk->IncreaseAllocatedBytes(size_in_bytes);

  return object;
}

AllocationResult ReadOnlySpace::AllocateRaw(size_t size_in_bytes,
                                            AllocationAlignment alignment) {
#ifdef V8_HOST_ARCH_32_BIT
  AllocationResult result = alignment != kWordAligned
                                ? AllocateRawAligned(size_in_bytes, alignment)
                                : AllocateRawUnaligned(size_in_bytes);
#else
  AllocationResult result =
      AllocateRawUnaligned(static_cast<int>(size_in_bytes));
#endif
  HeapObject heap_obj;
  if (!result.IsRetry() && result.To(&heap_obj)) {
    DCHECK(heap()->incremental_marking()->marking_state()->IsBlack(heap_obj));
  }
  return result;
}

size_t ReadOnlyPage::ShrinkToHighWaterMark() {
  // Shrink pages to high water mark. The water mark points either to a filler
  // or the area_end.
  HeapObject filler = HeapObject::FromAddress(HighWaterMark());
  if (filler.address() == area_end()) return 0;
  CHECK(filler.IsFreeSpaceOrFiller());
  DCHECK_EQ(filler.address() + filler.Size(), area_end());

  size_t unused = RoundDown(static_cast<size_t>(area_end() - filler.address()),
                            MemoryAllocator::GetCommitPageSize());
  if (unused > 0) {
    DCHECK_EQ(0u, unused % MemoryAllocator::GetCommitPageSize());
    if (FLAG_trace_gc_verbose) {
      PrintIsolate(heap()->isolate(), "Shrinking page %p: end %p -> %p\n",
                   reinterpret_cast<void*>(this),
                   reinterpret_cast<void*>(area_end()),
                   reinterpret_cast<void*>(area_end() - unused));
    }
    heap()->CreateFillerObjectAt(
        filler.address(),
        static_cast<int>(area_end() - filler.address() - unused),
        ClearRecordedSlots::kNo);
    heap()->memory_allocator()->PartialFreeMemory(
        this, address() + size() - unused, unused, area_end() - unused);
    if (filler.address() != area_end()) {
      CHECK(filler.IsFreeSpaceOrFiller());
      CHECK_EQ(filler.address() + filler.Size(), area_end());
    }
  }
  return unused;
}

void ReadOnlySpace::ShrinkPages() {
  BasicMemoryChunk::UpdateHighWaterMark(top_);
  heap()->CreateFillerObjectAt(top_, static_cast<int>(limit_ - top_),
                               ClearRecordedSlots::kNo);

  for (ReadOnlyPage* chunk : pages_) {
    DCHECK(chunk->IsFlagSet(Page::NEVER_EVACUATE));
    size_t unused = chunk->ShrinkToHighWaterMark();
    capacity_ -= unused;
    accounting_stats_.DecreaseCapacity(static_cast<intptr_t>(unused));
    AccountUncommitted(unused);
  }
  limit_ = pages_.back()->area_end();
}

ReadOnlyPage* ReadOnlySpace::InitializePage(BasicMemoryChunk* chunk) {
  ReadOnlyPage* page = reinterpret_cast<ReadOnlyPage*>(chunk);
  page->allocated_bytes_ = 0;
  page->SetFlag(BasicMemoryChunk::Flag::NEVER_EVACUATE);
  heap()
      ->incremental_marking()
      ->non_atomic_marking_state()
      ->bitmap(chunk)
      ->MarkAllBits();
  chunk->SetFlag(BasicMemoryChunk::READ_ONLY_HEAP);

  return page;
}

}  // namespace internal
}  // namespace v8

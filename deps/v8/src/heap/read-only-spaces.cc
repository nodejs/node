// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/read-only-spaces.h"

#include "src/base/lsan.h"
#include "src/execution/isolate.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/objects-inl.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ReadOnlySpace implementation

ReadOnlySpace::ReadOnlySpace(Heap* heap)
    : PagedSpace(heap, RO_SPACE, NOT_EXECUTABLE, FreeList::CreateFreeList()),
      is_string_padding_cleared_(heap->isolate()->initialized_from_snapshot()) {
}

ReadOnlyArtifacts::~ReadOnlyArtifacts() {
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();

  MemoryChunk* next_chunk;
  for (MemoryChunk* chunk = pages_.front(); chunk != nullptr;
       chunk = next_chunk) {
    void* chunk_address = reinterpret_cast<void*>(chunk->address());
    page_allocator->SetPermissions(chunk_address, chunk->size(),
                                   PageAllocator::kReadWrite);
    next_chunk = chunk->list_node().next();
    size_t size = RoundUp(chunk->size(), page_allocator->AllocatePageSize());
    CHECK(page_allocator->FreePages(chunk_address, size));
  }
}

void ReadOnlyArtifacts::set_read_only_heap(
    std::unique_ptr<ReadOnlyHeap> read_only_heap) {
  read_only_heap_ = std::move(read_only_heap);
}

SharedReadOnlySpace::~SharedReadOnlySpace() {
  // Clear the memory chunk list before the space is deleted, so that the
  // inherited destructors don't try to destroy the MemoryChunks themselves.
  memory_chunk_list_ = heap::List<MemoryChunk>();
}

SharedReadOnlySpace::SharedReadOnlySpace(
    Heap* heap, std::shared_ptr<ReadOnlyArtifacts> artifacts)
    : ReadOnlySpace(heap) {
  artifacts->pages().ShallowCopyTo(&memory_chunk_list_);
  is_marked_read_only_ = true;
  accounting_stats_ = artifacts->accounting_stats();
}

void ReadOnlySpace::DetachPagesAndAddToArtifacts(
    std::shared_ptr<ReadOnlyArtifacts> artifacts) {
  Heap* heap = ReadOnlySpace::heap();
  Seal(SealMode::kDetachFromHeapAndForget);
  artifacts->set_accounting_stats(accounting_stats_);
  artifacts->TransferPages(std::move(memory_chunk_list_));
  artifacts->set_shared_read_only_space(
      std::make_unique<SharedReadOnlySpace>(heap, artifacts));
  heap->ReplaceReadOnlySpace(artifacts->shared_read_only_space());
}

void ReadOnlyPage::MakeHeaderRelocatable() {
  ReleaseAllocatedMemoryNeededForWritableChunk();
  // Detached read-only space needs to have a valid marking bitmap. Instruct
  // Lsan to ignore it if required.
  LSAN_IGNORE_OBJECT(marking_bitmap_);
  heap_ = nullptr;
  owner_ = nullptr;
}

void ReadOnlySpace::SetPermissionsForPages(MemoryAllocator* memory_allocator,
                                           PageAllocator::Permission access) {
  for (Page* p : *this) {
    // Read only pages don't have valid reservation object so we get proper
    // page allocator manually.
    v8::PageAllocator* page_allocator =
        memory_allocator->page_allocator(p->executable());
    CHECK(SetPermissions(page_allocator, p->address(), p->size(), access));
  }
}

// After we have booted, we have created a map which represents free space
// on the heap.  If there was already a free list then the elements on it
// were created with the wrong FreeSpaceMap (normally nullptr), so we need to
// fix them.
void ReadOnlySpace::RepairFreeListsAfterDeserialization() {
  free_list_->RepairLists(heap());
  // Each page may have a small free space that is not tracked by a free list.
  // Those free spaces still contain null as their map pointer.
  // Overwrite them with new fillers.
  for (Page* page : *this) {
    int size = static_cast<int>(page->wasted_memory());
    if (size == 0) {
      // If there is no wasted memory then all free space is in the free list.
      continue;
    }
    Address start = page->HighWaterMark();
    Address end = page->area_end();
    if (start < end - size) {
      // A region at the high watermark is already in free list.
      HeapObject filler = HeapObject::FromAddress(start);
      CHECK(filler.IsFreeSpaceOrFiller());
      start += filler.Size();
    }
    CHECK_EQ(size, static_cast<int>(end - start));
    heap()->CreateFillerObjectAt(start, size, ClearRecordedSlots::kNo);
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
    for (Page* p : *this) {
      memory_allocator->UnregisterMemory(p);
      static_cast<ReadOnlyPage*>(p)->MakeHeaderRelocatable();
    }
  } else {
    for (Page* p : *this) {
      p->ReleaseAllocatedMemoryNeededForWritableChunk();
    }
  }

  free_list_.reset();

  SetPermissionsForPages(memory_allocator, PageAllocator::kRead);
}

void ReadOnlySpace::Unseal() {
  DCHECK(is_marked_read_only_);
  if (HasPages()) {
    SetPermissionsForPages(heap()->memory_allocator(),
                           PageAllocator::kReadWrite);
  }
  is_marked_read_only_ = false;
}

}  // namespace internal
}  // namespace v8

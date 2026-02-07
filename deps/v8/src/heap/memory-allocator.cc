// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-allocator.h"

#include <cinttypes>
#include <optional>

#include "src/base/address-region.h"
#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/large-page-metadata.h"
#include "src/heap/memory-chunk-metadata.h"
#include "src/heap/memory-pool.h"
#include "src/heap/mutable-page-metadata.h"
#include "src/heap/read-only-spaces.h"
#include "src/heap/zapping.h"
#include "src/logging/log.h"
#include "src/sandbox/hardware-support.h"
#include "src/utils/allocation.h"

namespace v8::internal {

size_t MemoryAllocator::commit_page_size_ = 0;
size_t MemoryAllocator::commit_page_size_bits_ = 0;

MemoryAllocator::MemoryAllocator(Isolate* isolate,
                                 v8::PageAllocator* code_page_allocator,
                                 v8::PageAllocator* trusted_page_allocator,
                                 MemoryPool* page_pool, size_t capacity)
    : isolate_(isolate),
      data_page_allocator_(isolate->page_allocator()),
      read_only_page_allocator_(
          IsolateGroup::current()->read_only_page_allocator()),
      code_page_allocator_(code_page_allocator),
      trusted_page_allocator_(trusted_page_allocator),
      capacity_(RoundUp(capacity, PageMetadata::kPageSize)),
      pool_(page_pool) {
  DCHECK_NOT_NULL(data_page_allocator_);
  DCHECK_NOT_NULL(read_only_page_allocator_);
  DCHECK_NOT_NULL(code_page_allocator_);
  DCHECK_NOT_NULL(trusted_page_allocator_);
}

void MemoryAllocator::TearDown() {
  if (auto* pool = memory_pool()) {
    pool->ReleaseOnTearDown(isolate_);
    DCHECK_EQ(pool->GetCount(isolate_), 0);
  }

  // Check that spaces were torn down before MemoryAllocator.
  DCHECK_EQ(size_, 0u);
  // TODO(gc) this will be true again when we fix FreeMemory.
  // DCHECK_EQ(0, size_executable_);
  capacity_ = 0;

  if (reserved_chunk_at_virtual_memory_limit_) {
    reserved_chunk_at_virtual_memory_limit_->Free();
  }

  code_page_allocator_ = nullptr;
  data_page_allocator_ = nullptr;
  read_only_page_allocator_ = nullptr;
  trusted_page_allocator_ = nullptr;
}

size_t MemoryAllocator::GetPooledChunksCount() {
  return memory_pool() ? memory_pool()->GetCount(isolate_) : 0;
}

size_t MemoryAllocator::GetSharedPooledChunksCount() {
  return memory_pool() ? memory_pool()->GetSharedCount() : 0;
}

size_t MemoryAllocator::GetTotalPooledChunksCount() {
  return memory_pool() ? memory_pool()->GetTotalCount() : 0;
}

void MemoryAllocator::ReleasePooledChunksImmediately() {
  if (auto* pool = memory_pool()) {
    pool->ReleaseImmediately(isolate_);
  }
}

void MemoryAllocator::FreeMemoryRegion(v8::PageAllocator* page_allocator,
                                       Address base, size_t size) {
  FreePages(page_allocator, reinterpret_cast<void*>(base), size);
}

namespace {

Address HandleAllocationFailure(Heap* heap, Executability executable) {
  if (!heap->deserialization_complete()) {
    heap->FatalProcessOutOfMemory(
        executable == EXECUTABLE
            ? "Executable MemoryChunk allocation failed during deserialization."
            : "MemoryChunk allocation failed during deserialization.");
  }
  return kNullAddress;
}

}  // namespace

Address MemoryAllocator::AllocateAlignedMemory(
    size_t chunk_size, size_t area_size, size_t alignment,
    AllocationSpace space, Executability executable, void* address_hint,
    VirtualMemory* controller, PageSize page_size, AllocationHint hint) {
  DCHECK_EQ(space == CODE_SPACE || space == CODE_LO_SPACE,
            executable == EXECUTABLE);
  v8::PageAllocator* page_allocator = this->page_allocator(space);
  DCHECK_LT(area_size, chunk_size);

  PageAllocator::Permission permissions =
      executable == EXECUTABLE
          ? MutablePageMetadata::GetCodeModificationPermission()
          : PageAllocator::kReadWrite;

  v8::PageAllocator::AllocationHint page_allocation_hint =
      v8::PageAllocator::AllocationHint().WithAddress(address_hint);
  if (hint.MayGrow()) {
    page_allocation_hint = page_allocation_hint.WithMayGrow();
  }
  VirtualMemory reservation(page_allocator, chunk_size, page_allocation_hint,
                            alignment, permissions);
  if (!reservation.IsReserved()) {
    return HandleAllocationFailure(isolate_->heap(), executable);
  }

  // We cannot use the last chunk in the address space because we would
  // overflow when comparing top and limit if this chunk is used for a
  // linear allocation area.
  if ((reservation.address() + static_cast<Address>(chunk_size)) == 0u) {
    CHECK(!reserved_chunk_at_virtual_memory_limit_);
    reserved_chunk_at_virtual_memory_limit_ = std::move(reservation);
    CHECK(reserved_chunk_at_virtual_memory_limit_);

    // Retry reserve virtual memory.
    reservation = VirtualMemory(page_allocator, chunk_size,
                                page_allocation_hint, alignment, permissions);
    if (!reservation.IsReserved()) {
      return HandleAllocationFailure(isolate_->heap(), executable);
    }
  }

  Address base = reservation.address();

  if (executable == EXECUTABLE) {
    ThreadIsolation::RegisterJitPage(base, chunk_size);
  }

  UpdateAllocatedSpaceLimits(base, base + chunk_size, executable);

  *controller = std::move(reservation);
  return base;
}

size_t MemoryAllocator::ComputeChunkSize(size_t area_size,
                                         AllocationSpace space) {
  //
  // +----------------------------+<- base aligned at MemoryChunk::kAlignment
  // |          Header            |
  // +----------------------------+<- area_start_ (base + area_start_)
  // |           Area             |
  // +----------------------------+<- area_end_ (area_start + area_size)
  // |  Committed but not used    |
  // +----------------------------+<- base + chunk_size
  //

  return ::RoundUp(
      MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(space) + area_size,
      GetCommitPageSize());
}

std::optional<MemoryAllocator::MemoryChunkAllocationResult>
MemoryAllocator::AllocateUninitializedChunkAt(BaseSpace* space,
                                              size_t area_size,
                                              Executability executable,
                                              Address hint, PageSize page_size,
                                              AllocationHint allocation_hint) {
#ifndef V8_COMPRESS_POINTERS
  // When pointer compression is enabled, spaces are expected to be at a
  // predictable address (see mkgrokdump) so we don't supply a hint and rely on
  // the deterministic behaviour of the BoundedPageAllocator.
  if (hint == kNullAddress) {
    hint = reinterpret_cast<Address>(
        AlignedAddress(isolate_->heap()->GetRandomMmapAddr(),
                       MemoryChunk::GetAlignmentForAllocation()));
  }
#endif

  VirtualMemory reservation;
  size_t chunk_size = ComputeChunkSize(area_size, space->identity());
  DCHECK_EQ(chunk_size % GetCommitPageSize(), 0);

  Address base = AllocateAlignedMemory(
      chunk_size, area_size, MemoryChunk::GetAlignmentForAllocation(),
      space->identity(), executable, reinterpret_cast<void*>(hint),
      &reservation, page_size, allocation_hint);
  if (base == kNullAddress) return {};

  size_ += reservation.size();

  // Update executable memory size.
  if (executable == EXECUTABLE) {
    size_executable_ += reservation.size();
  }

  if (heap::ShouldZapGarbage()) {
    if (executable == EXECUTABLE) {
      CodePageMemoryModificationScopeForDebugging memory_write_scope(
          isolate_->heap(), &reservation,
          base::AddressRegion(base, chunk_size));
      heap::ZapBlock(base, chunk_size, kZapValue);
    } else {
      DCHECK_EQ(executable, NOT_EXECUTABLE);
      // Zap both page header and object area at once. No guard page in-between.
      heap::ZapBlock(base, chunk_size, kZapValue);
    }
  }

  LOG(isolate_,
      NewEvent("MemoryChunk", reinterpret_cast<void*>(base), chunk_size));

  Address area_start = base + MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(
                                  space->identity());
  Address area_end = area_start + area_size;

  return MemoryChunkAllocationResult{
      reinterpret_cast<void*>(base), nullptr, chunk_size, area_start, area_end,
      std::move(reservation),
  };
}

void MemoryAllocator::PartialFreeMemory(MemoryChunkMetadata* chunk,
                                        Address start_free,
                                        size_t bytes_to_free,
                                        Address new_area_end) {
  VirtualMemory* reservation = chunk->reserved_memory();
  DCHECK(reservation->IsReserved());
  chunk->set_size(chunk->size() - bytes_to_free);
  chunk->set_area_end(new_area_end);
  if (chunk->is_executable()) {
    // Add guard page at the end.
    size_t page_size = GetCommitPageSize();
    DCHECK_EQ(0, chunk->area_end() % static_cast<Address>(page_size));
    DCHECK_EQ(chunk->ChunkAddress() + chunk->size(), chunk->area_end());

    if ((V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT ||
         V8_HEAP_USE_BECORE_JIT_WRITE_PROTECT) &&
        !isolate_->jitless()) {
      DCHECK(isolate_->RequiresCodeRange());
      DiscardSealedMemoryScope discard_scope("Partially free memory.");
      reservation->DiscardSystemPages(chunk->area_end(), page_size);
    } else {
      CHECK(reservation->SetPermissions(chunk->area_end(), page_size,
                                        PageAllocator::kNoAccess));
    }
  }
  // On e.g. Windows, a reservation may be larger than a page and releasing
  // partially starting at |start_free| will also release the potentially
  // unused part behind the current page.
  const size_t released_bytes = reservation->Release(start_free);
  DCHECK_GE(size_, released_bytes);
  size_ -= released_bytes;
}

void MemoryAllocator::UnregisterSharedMemoryChunk(MemoryChunkMetadata* chunk) {
  VirtualMemory* reservation = chunk->reserved_memory();
  const size_t size =
      reservation->IsReserved() ? reservation->size() : chunk->size();
  DCHECK_GE(size_, static_cast<size_t>(size));
  size_ -= size;
}

void MemoryAllocator::UnregisterMemoryChunk(
    MemoryChunkMetadata* chunk_metadata) {
  MemoryChunk* chunk = chunk_metadata->Chunk();
  DCHECK(!chunk_metadata->is_unregistered());
  VirtualMemory* reservation = chunk_metadata->reserved_memory();
  const size_t size =
      reservation->IsReserved() ? reservation->size() : chunk_metadata->size();
  DCHECK_GE(size_, static_cast<size_t>(size));

  size_ -= size;
  if (chunk_metadata->is_executable()) {
    DCHECK_GE(size_executable_, size);
    size_executable_ -= size;
#ifdef DEBUG
    UnregisterExecutableMemoryChunk(
        static_cast<MutablePageMetadata*>(chunk_metadata));
#endif  // DEBUG

    ThreadIsolation::UnregisterJitPage(chunk->address(),
                                       chunk_metadata->size());
  }
  // For non-RO pages we want to set them as UNREGISTERED to allow actually
  // freeing them.
  if (!chunk->InReadOnlySpace()) {
    // Cannot use MutablePageMetadata::cast() because that relies on having an
    // owner() which is unsed at this point.
    reinterpret_cast<MutablePageMetadata*>(chunk_metadata)
        ->set_is_unregistered();
  }
}

void MemoryAllocator::UnregisterMutableMemoryChunk(MutablePageMetadata* page) {
  UnregisterMemoryChunk(page);
}

void MemoryAllocator::UnregisterReadOnlyPage(ReadOnlyPageMetadata* page) {
  DCHECK(!page->is_executable());
  UnregisterMemoryChunk(page);
}

void MemoryAllocator::FreeReadOnlyPage(ReadOnlyPageMetadata* chunk) {
  DCHECK(!chunk->is_pre_freed());
  LOG(isolate_, DeleteEvent("MemoryChunk", chunk));

  UnregisterSharedMemoryChunk(chunk);

  v8::PageAllocator* allocator = page_allocator(RO_SPACE);
  VirtualMemory* reservation = chunk->reserved_memory();
  if (reservation->IsReserved()) {
    reservation->Free();
  } else {
    // Only read-only pages can have a non-initialized reservation object. This
    // happens when the pages are remapped to multiple locations and where the
    // reservation would therefore be invalid.
    FreeMemoryRegion(allocator, chunk->ChunkAddress(),
                     RoundUp(chunk->size(), allocator->AllocatePageSize()));
  }

  delete chunk;
}

void MemoryAllocator::PreFreeMemory(MutablePageMetadata* chunk_metadata) {
  MemoryChunk* chunk = chunk_metadata->Chunk();
  DCHECK(!chunk_metadata->is_pre_freed());
  LOG(isolate_, DeleteEvent("MemoryChunk", chunk_metadata));
  RecordMemoryChunkDestroyed(chunk_metadata);
  UnregisterMutableMemoryChunk(chunk_metadata);
  isolate_->heap()->RememberUnmappedPage(
      reinterpret_cast<Address>(chunk_metadata),
      chunk->IsEvacuationCandidate());
  chunk_metadata->ReleaseAllAllocatedMemory();
  chunk_metadata->set_is_pre_freed();
}

void MemoryAllocator::PerformFreeMemory(MutablePageMetadata* chunk_metadata) {
  DCHECK(chunk_metadata->is_unregistered());
  DCHECK(chunk_metadata->is_pre_freed());
  DCHECK(!chunk_metadata->Chunk()->InReadOnlySpace());

  DeleteMemoryChunk(chunk_metadata);
}

void MemoryAllocator::Free(MemoryAllocator::FreeMode mode,
                           MutablePageMetadata* page_metadata) {
  PreFreeMemory(page_metadata);
  switch (mode) {
    case FreeMode::kImmediately:
      PerformFreeMemory(page_metadata);
      break;
    case FreeMode::kDelayThenPool:
      if (memory_pool()) {
        if (page_metadata->is_large()) {
          delayed_then_pooled_large_pages_.push_back(
              static_cast<LargePageMetadata*>(page_metadata));
        } else {
          delayed_then_pooled_pages_.push_back(
              static_cast<PageMetadata*>(page_metadata));
        }
        break;
      }
      // Otherwise, add the page to delayed_then_released_pages_.
      [[fallthrough]];
    case FreeMode::kDelayThenRelease:
      delayed_then_released_pages_.push_back(page_metadata);
      break;
    case FreeMode::kPool:
#ifdef DEBUG
      // Ensure that we only ever put pages with their markbits cleared into the
      // pool. This is necessary because `PreFreeMemory` doesn't clear the
      // marking bitmap and the marking bitmap is reused when this page is taken
      // out of the pool again.
      DCHECK(page_metadata->IsLivenessClear());
      DCHECK_EQ(page_metadata->size(),
                static_cast<size_t>(MutablePageMetadata::kPageSize));
      DCHECK(!page_metadata->is_executable());
#endif  // DEBUG
      if (auto* pool = memory_pool()) {
        pool->Add(isolate_, page_metadata);
      } else {
        PerformFreeMemory(page_metadata);
      }
      break;
  }
}

void MemoryAllocator::ReleaseDelayedPages() {
  for (auto* delayed_page : delayed_then_released_pages_) {
    PerformFreeMemory(delayed_page);
  }
  delayed_then_released_pages_.clear();
  if (!memory_pool()) {
    DCHECK(delayed_then_pooled_pages_.empty());
    DCHECK(delayed_then_pooled_large_pages_.empty());
    return;
  }
  for (auto* delayed_page : delayed_then_pooled_pages_) {
    memory_pool()->Add(isolate_, delayed_page);
  }
  delayed_then_pooled_pages_.clear();
  memory_pool()->AddLarge(isolate_, delayed_then_pooled_large_pages_);
  // AddLarge() leaves pages that couldn't be pooled in the vector to be
  // released afterwards.
  for (auto* delayed_page : delayed_then_pooled_large_pages_) {
    PerformFreeMemory(delayed_page);
  }
  delayed_then_pooled_large_pages_.clear();
}

PageMetadata* MemoryAllocator::AllocatePage(
    MemoryAllocator::AllocationMode alloc_mode, Space* space,
    Executability executable) {
  const size_t size =
      MemoryChunkLayout::AllocatableMemoryInMemoryChunk(space->identity());
  std::optional<MemoryChunkAllocationResult> chunk_info;
  if (alloc_mode == AllocationMode::kTryDelayedAndPooled) {
    DCHECK_EQ(executable, NOT_EXECUTABLE);
    chunk_info = AllocateUninitializedPageFromDelayedOrPool(space);
  }
  if (!chunk_info) {
    chunk_info = AllocateUninitializedChunk(
        space, size, executable, PageSize::kRegular, AllocationHint());
  }
  if (!chunk_info) {
    return nullptr;
  }

  PageMetadata* metadata;
  MemoryChunk::MainThreadFlags trusted_flags;
  if (!chunk_info->optional_metadata) {
    chunk_info->optional_metadata = malloc(sizeof(PageMetadata));
  }
  metadata = new (chunk_info->optional_metadata) PageMetadata(
      isolate_->heap(), space, chunk_info->size, chunk_info->area_start,
      chunk_info->area_end, std::move(chunk_info->reservation), executable,
      &trusted_flags);
  if (executable) {
    RwxMemoryWriteScope scope("Initialize a new MemoryChunk.");
    new (chunk_info->chunk) MemoryChunk(trusted_flags, metadata);
#ifdef DEBUG
    RegisterExecutableMemoryChunk(metadata);
#endif  // DEBUG
  } else {
    new (chunk_info->chunk) MemoryChunk(trusted_flags, metadata);
  }

  DCHECK(metadata->IsLivenessClear());
  space->InitializePage(metadata);
  RecordMemoryChunkCreated(metadata);
  return metadata;
}

ReadOnlyPageMetadata* MemoryAllocator::AllocateReadOnlyPage(
    ReadOnlySpace* space, Address hint) {
  DCHECK_EQ(space->identity(), RO_SPACE);
  size_t size = MemoryChunkLayout::AllocatableMemoryInMemoryChunk(RO_SPACE);
  std::optional<MemoryChunkAllocationResult> chunk_info =
      AllocateUninitializedChunkAt(space, size, NOT_EXECUTABLE, hint,
                                   PageSize::kRegular, AllocationHint());
  if (!chunk_info) {
    return nullptr;
  }
  CHECK_NULL(chunk_info->optional_metadata);
  ReadOnlyPageMetadata* metadata = new ReadOnlyPageMetadata(
      isolate_->heap(), space, chunk_info->size, chunk_info->area_start,
      chunk_info->area_end, std::move(chunk_info->reservation));

  new (chunk_info->chunk) MemoryChunk(metadata->InitialFlags(), metadata);

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  SandboxHardwareSupport::RegisterReadOnlyMemoryInsideSandbox(
      metadata->ChunkAddress(), metadata->size(), PagePermissions::kReadWrite);
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

  return metadata;
}

std::unique_ptr<::v8::PageAllocator::SharedMemoryMapping>
MemoryAllocator::RemapSharedPage(
    ::v8::PageAllocator::SharedMemory* shared_memory, Address new_address) {
  return shared_memory->RemapTo(reinterpret_cast<void*>(new_address));
}

namespace {
bool IsPagePoolSupportedForLargeSpace(LargeObjectSpace* space) {
  const AllocationSpace identity = space->identity();
  return identity == NEW_LO_SPACE || identity == LO_SPACE;
}
}  // namespace

LargePageMetadata* MemoryAllocator::AllocateLargePage(LargeObjectSpace* space,
                                                      size_t object_size,
                                                      Executability executable,
                                                      AllocationHint hint) {
  std::optional<MemoryChunkAllocationResult> chunk_info;

  if (IsPagePoolSupportedForLargeSpace(space)) {
    chunk_info = TryAllocateUninitializedLargePageFromPool(space, object_size);
  }

  if (!chunk_info) {
    chunk_info = AllocateUninitializedChunk(space, object_size, executable,
                                            PageSize::kLarge, hint);
  }

  if (!chunk_info) {
    return nullptr;
  }

  LargePageMetadata* metadata;
  MemoryChunk::MainThreadFlags trusted_flags;
  if (!chunk_info->optional_metadata) {
    chunk_info->optional_metadata = malloc(sizeof(LargePageMetadata));
  }
  metadata = new (chunk_info->optional_metadata) LargePageMetadata(
      isolate_->heap(), space, chunk_info->size, chunk_info->area_start,
      chunk_info->area_end, std::move(chunk_info->reservation), executable,
      &trusted_flags);
  if (executable) {
    RwxMemoryWriteScope scope("Initialize a new MemoryChunk.");
    new (chunk_info->chunk) MemoryChunk(trusted_flags, metadata);
#ifdef DEBUG
    RegisterExecutableMemoryChunk(metadata);
#endif  // DEBUG
  } else {
    new (chunk_info->chunk) MemoryChunk(trusted_flags, metadata);
  }

  RecordMemoryChunkCreated(metadata);
  return metadata;
}

bool MemoryAllocator::ResizeLargePage(LargePageMetadata* page,
                                      size_t old_object_size,
                                      size_t new_object_size) {
  const size_t old_reservation_size = page->reservation_.size();
  const size_t old_page_end = page->reservation_.end();
  const Address new_area_end = page->area_start() + new_object_size;
  const Address new_page_end = RoundUp(new_area_end, GetCommitPageSize());
  const size_t new_page_size = new_page_end - page->ChunkAddress();

  if (old_page_end == new_page_end) {
    // We were able to grow the object without growing its owning page.
    page->set_area_end(new_area_end);
    return true;
  }

  // Currently we only support growing.
  DCHECK_LT(old_page_end, new_page_end);

  if (!page->reservation_.Resize(page->ChunkAddress(), new_page_size,
                                 PageAllocator::kReadWrite)) {
    return false;
  }

  page->set_area_end(new_area_end);
  page->set_size(new_page_size);

#if DEBUG
  AllocationSpace space = page->owner_identity();
  DCHECK(space == NEW_LO_SPACE || space == LO_SPACE);
#endif  // DEBUG

  UpdateAllocatedSpaceLimits(page->ChunkAddress(), new_page_end,
                             Executability::NOT_EXECUTABLE);

  size_ -= old_reservation_size;
  size_ += page->reservation_.size();

  return true;
}

std::optional<MemoryAllocator::MemoryChunkAllocationResult>
MemoryAllocator::AllocateUninitializedPageFromDelayedOrPool(Space* space) {
  std::optional<PooledPage::Result> maybe_result;
  {
    base::MutexGuard guard(chunks_mutex_);
    if (!delayed_then_pooled_pages_.empty()) {
      DCHECK(memory_pool());
      PageMetadata* metadata = delayed_then_pooled_pages_.back();
      delayed_then_pooled_pages_.pop_back();
      maybe_result = memory_pool()->CreatePooledPage(metadata).ToResult();
    }
  }
  if (!maybe_result.has_value() && memory_pool()) {
    maybe_result = memory_pool()->Remove(isolate_);
  }
  if (!maybe_result.has_value()) {
    return {};
  }
  const int size = MutablePageMetadata::kPageSize;
  const Address start = maybe_result->uninitialized_chunk.begin();
  // Pooled pages are always regular data pages.
  DCHECK_NE(CODE_SPACE, space->identity());
  DCHECK_NE(TRUSTED_SPACE, space->identity());
  VirtualMemory reservation(data_page_allocator(), start, size);
  if (heap::ShouldZapGarbage()) {
    heap::ZapBlock(start, size, kZapValue);
  }
  size_ += size;
  UpdateAllocatedSpaceLimits(start, start + size,
                             Executability::NOT_EXECUTABLE);
  const Address area_start =
      start +
      MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(space->identity());
  const Address area_end = start + size;
  return MemoryChunkAllocationResult{
      reinterpret_cast<void*>(maybe_result->uninitialized_chunk.begin()),
      maybe_result->uninitialized_metadata,
      size,
      area_start,
      area_end,
      std::move(reservation),
  };
}

std::optional<MemoryAllocator::MemoryChunkAllocationResult>
MemoryAllocator::TryAllocateUninitializedLargePageFromPool(Space* space,
                                                           size_t object_size) {
  if (!memory_pool()) {
    return {};
  }
  const size_t object_start_offset =
      MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(space->identity());
  // Select a pooled large page which can store |object_size| bytes. In case the
  // page is larger than necessary, the next full GC will trim down its size.
  std::optional<PooledPage::Result> maybe_result =
      memory_pool()->RemoveLarge(isolate_, object_start_offset + object_size);
  if (!maybe_result.has_value()) {
    return {};
  }
  const Address start = maybe_result->uninitialized_chunk.begin();
  const size_t size = maybe_result->uninitialized_chunk.size();
  const Address area_start = start + object_start_offset;
  const Address area_end = start + size;
  VirtualMemory reservation(data_page_allocator(), start, size);
  if (heap::ShouldZapGarbage()) {
    heap::ZapBlock(start, size, kZapValue);
  }
  size_ += size;
  UpdateAllocatedSpaceLimits(start, start + size,
                             Executability::NOT_EXECUTABLE);
  return MemoryChunkAllocationResult{
      reinterpret_cast<void*>(maybe_result->uninitialized_chunk.begin()),
      maybe_result->uninitialized_metadata,
      size,
      area_start,
      area_end,
      std::move(reservation),
  };
}

void MemoryAllocator::InitializeOncePerProcess() {
  commit_page_size_ = v8_flags.v8_os_page_size > 0
                          ? v8_flags.v8_os_page_size * KB
                          : CommitPageSize();
  CHECK(base::bits::IsPowerOfTwo(commit_page_size_));
  commit_page_size_bits_ = base::bits::WhichPowerOfTwo(commit_page_size_);
}

bool MemoryAllocator::SetPermissionsOnExecutableMemoryChunk(VirtualMemory* vm,
                                                            Address start,
                                                            size_t chunk_size) {
  // All addresses and sizes must be aligned to the commit page size.
  DCHECK(IsAligned(start, GetCommitPageSize()));
  DCHECK_EQ(0, chunk_size % GetCommitPageSize());

  if (isolate_->RequiresCodeRange()) {
    // The pages of the code range are already mapped RWX, we just need to
    // recommit them.
    return vm->RecommitPages(start, chunk_size,
                             PageAllocator::kReadWriteExecute);
  } else {
    return vm->SetPermissions(
        start, chunk_size,
        MutablePageMetadata::GetCodeModificationPermission());
  }
}

const MemoryChunk* MemoryAllocator::LookupChunkContainingAddress(
    Address addr) const {
  base::MutexGuard guard(&chunks_mutex_);
  return LookupChunkContainingAddressInSafepoint(addr);
}

const MemoryChunk* MemoryAllocator::LookupChunkContainingAddressInSafepoint(
    Address addr) const {
  // All threads should be either parked or in a safepoint whenever this method
  // is called, thus pages cannot be allocated or freed at the same time and a
  // mutex is not required here.
  // As the address may not correspond to a valid heap object, the chunk we
  // obtain below is not necessarily a valid chunk.
  MemoryChunk* chunk = MemoryChunk::FromAddress(addr);
  // Check if it corresponds to a known normal or large page.
  if (auto normal_page_it = normal_pages_.find(chunk);
      normal_page_it != normal_pages_.end()) {
    // The chunk is a normal page.
    // auto* normal_page = PageMetadata::cast(chunk);
    DCHECK_LE((*normal_page_it)->address(), addr);
    // This code can run from the shared heap isolate and the slot may point
    // into a client heap isolate, so ignore the isolate check.
    if (chunk->MetadataNoIsolateCheck()->Contains(addr)) return chunk;
  } else if (auto large_page_it = large_pages_.upper_bound(chunk);
             large_page_it != large_pages_.begin()) {
    // The chunk could be inside a large page.
    DCHECK_IMPLIES(large_page_it != large_pages_.end(),
                   addr < (*large_page_it)->address());
    auto* large_page_chunk = *std::next(large_page_it, -1);
    DCHECK_NOT_NULL(large_page_chunk);
    DCHECK_LE(large_page_chunk->address(), addr);
    // This code can run from the shared heap isolate and the slot may point
    // into a client heap isolate, so ignore the isolate check.
    if (large_page_chunk->MetadataNoIsolateCheck()->Contains(addr))
      return large_page_chunk;
  }
  // Not found in any page.
  return nullptr;
}

void MemoryAllocator::RecordMemoryChunkCreated(
    const MemoryChunkMetadata* metadata) {
  base::MutexGuard guard(&chunks_mutex_);
  if (metadata->is_large()) {
    auto result = large_pages_.insert(metadata->Chunk());
    USE(result);
    DCHECK(result.second);
  } else {
    auto result = normal_pages_.insert(metadata->Chunk());
    USE(result);
    DCHECK(result.second);
  }
}

void MemoryAllocator::RecordMemoryChunkDestroyed(
    const MemoryChunkMetadata* metadata) {
  base::MutexGuard guard(&chunks_mutex_);
  if (metadata->is_large()) {
    auto size = large_pages_.erase(metadata->Chunk());
    USE(size);
    DCHECK_EQ(1u, size);
  } else {
    auto size = normal_pages_.erase(metadata->Chunk());
    USE(size);
    DCHECK_EQ(1u, size);
  }
}

// static
void MemoryAllocator::DeleteMemoryChunk(MutablePageMetadata* metadata) {
  DCHECK(metadata->reserved_memory()->IsReserved());
  // The Metadata contains a VirtualMemory reservation and the destructor will
  // release the MemoryChunk.
  {
    DiscardSealedMemoryScope discard_scope("Deleting a memory chunk");
    if (metadata->is_large()) {
      static_cast<LargePageMetadata*>(metadata)->~LargePageMetadata();
    } else {
      static_cast<PageMetadata*>(metadata)->~PageMetadata();
    }
  }
  free(metadata);
}

void MemoryAllocator::UpdateAllocatedSpaceLimits(Address low, Address high,
                                                 Executability executable) {
  // The use of atomic primitives does not guarantee correctness (wrt.
  // desired semantics) by default. The loop here ensures that we update the
  // values only if they did not change in between.
  Address ptr;
  switch (executable) {
    case NOT_EXECUTABLE:
      ptr =
          lowest_not_executable_ever_allocated_.load(std::memory_order_relaxed);
      while ((low < ptr) &&
             !lowest_not_executable_ever_allocated_.compare_exchange_weak(
                 ptr, low, std::memory_order_acq_rel)) {
      }
      ptr = highest_not_executable_ever_allocated_.load(
          std::memory_order_relaxed);
      while ((high > ptr) &&
             !highest_not_executable_ever_allocated_.compare_exchange_weak(
                 ptr, high, std::memory_order_acq_rel)) {
      }
      break;
    case EXECUTABLE:
      ptr = lowest_executable_ever_allocated_.load(std::memory_order_relaxed);
      while ((low < ptr) &&
             !lowest_executable_ever_allocated_.compare_exchange_weak(
                 ptr, low, std::memory_order_acq_rel)) {
      }
      ptr = highest_executable_ever_allocated_.load(std::memory_order_relaxed);
      while ((high > ptr) &&
             !highest_executable_ever_allocated_.compare_exchange_weak(
                 ptr, high, std::memory_order_acq_rel)) {
      }
      break;
  }
}

#ifdef DEBUG
void MemoryAllocator::RegisterExecutableMemoryChunk(
    MutablePageMetadata* chunk) {
  base::MutexGuard guard(&executable_memory_mutex_);
  DCHECK(chunk->is_executable());
  DCHECK_EQ(executable_memory_.find(chunk), executable_memory_.end());
  executable_memory_.insert(chunk);
}

void MemoryAllocator::UnregisterExecutableMemoryChunk(
    MutablePageMetadata* chunk) {
  base::MutexGuard guard(&executable_memory_mutex_);
  DCHECK_NE(executable_memory_.find(chunk), executable_memory_.end());
  executable_memory_.erase(chunk);
}
#endif  // DEBUG

}  // namespace v8::internal

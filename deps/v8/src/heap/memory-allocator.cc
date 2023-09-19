// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-allocator.h"

#include <cinttypes>

#include "src/base/address-region.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-spaces.h"
#include "src/logging/log.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// MemoryAllocator
//

size_t MemoryAllocator::commit_page_size_ = 0;
size_t MemoryAllocator::commit_page_size_bits_ = 0;

MemoryAllocator::MemoryAllocator(Isolate* isolate,
                                 v8::PageAllocator* code_page_allocator,
                                 size_t capacity)
    : isolate_(isolate),
      data_page_allocator_(isolate->page_allocator()),
      code_page_allocator_(code_page_allocator),
      capacity_(RoundUp(capacity, Page::kPageSize)),
      size_(0),
      size_executable_(0),
      lowest_ever_allocated_(static_cast<Address>(-1ll)),
      highest_ever_allocated_(kNullAddress),
      unmapper_(isolate->heap(), this) {
  DCHECK_NOT_NULL(code_page_allocator);
}

void MemoryAllocator::TearDown() {
  unmapper()->TearDown();

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
}

class MemoryAllocator::Unmapper::UnmapFreeMemoryJob : public JobTask {
 public:
  explicit UnmapFreeMemoryJob(Isolate* isolate, Unmapper* unmapper)
      : unmapper_(unmapper), tracer_(isolate->heap()->tracer()) {}

  UnmapFreeMemoryJob(const UnmapFreeMemoryJob&) = delete;
  UnmapFreeMemoryJob& operator=(const UnmapFreeMemoryJob&) = delete;

  void Run(JobDelegate* delegate) override {
    if (delegate->IsJoiningThread()) {
      TRACE_GC(tracer_, GCTracer::Scope::UNMAPPER);
      RunImpl(delegate);

    } else {
      TRACE_GC1(tracer_, GCTracer::Scope::BACKGROUND_UNMAPPER,
                ThreadKind::kBackground);
      RunImpl(delegate);
    }
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    const size_t kTaskPerChunk = 8;
    return std::min<size_t>(
        kMaxUnmapperTasks,
        worker_count +
            (unmapper_->NumberOfCommittedChunks() + kTaskPerChunk - 1) /
                kTaskPerChunk);
  }

 private:
  void RunImpl(JobDelegate* delegate) {
    unmapper_->PerformFreeMemoryOnQueuedChunks(FreeMode::kUncommitPooled,
                                               delegate);
    if (v8_flags.trace_unmapper) {
      PrintIsolate(unmapper_->heap_->isolate(), "UnmapFreeMemoryTask Done\n");
    }
  }
  Unmapper* const unmapper_;
  GCTracer* const tracer_;
};

void MemoryAllocator::Unmapper::FreeQueuedChunks() {
  if (NumberOfChunks() == 0) return;

  if (!heap_->IsTearingDown() && v8_flags.concurrent_sweeping) {
    if (job_handle_ && job_handle_->IsValid()) {
      job_handle_->NotifyConcurrencyIncrease();
    } else {
      job_handle_ = V8::GetCurrentPlatform()->PostJob(
          TaskPriority::kUserVisible,
          std::make_unique<UnmapFreeMemoryJob>(heap_->isolate(), this));
      if (v8_flags.trace_unmapper) {
        PrintIsolate(heap_->isolate(), "Unmapper::FreeQueuedChunks: new Job\n");
      }
    }
  } else {
    PerformFreeMemoryOnQueuedChunks(FreeMode::kUncommitPooled);
  }
}

void MemoryAllocator::Unmapper::CancelAndWaitForPendingTasks() {
  if (job_handle_ && job_handle_->IsValid()) job_handle_->Join();

  if (v8_flags.trace_unmapper) {
    PrintIsolate(
        heap_->isolate(),
        "Unmapper::CancelAndWaitForPendingTasks: no tasks remaining\n");
  }
}

void MemoryAllocator::Unmapper::PrepareForGC() {
  // Free non-regular chunks because they cannot be re-used.
  PerformFreeMemoryOnQueuedNonRegularChunks();
}

void MemoryAllocator::Unmapper::EnsureUnmappingCompleted() {
  CancelAndWaitForPendingTasks();
  PerformFreeMemoryOnQueuedChunks(FreeMode::kFreePooled);
}

void MemoryAllocator::Unmapper::PerformFreeMemoryOnQueuedNonRegularChunks(
    JobDelegate* delegate) {
  MemoryChunk* chunk = nullptr;
  while ((chunk = GetMemoryChunkSafe(ChunkQueueType::kNonRegular)) != nullptr) {
    allocator_->PerformFreeMemory(chunk);
    if (delegate && delegate->ShouldYield()) return;
  }
}

void MemoryAllocator::Unmapper::PerformFreeMemoryOnQueuedChunks(
    MemoryAllocator::Unmapper::FreeMode mode, JobDelegate* delegate) {
  MemoryChunk* chunk = nullptr;
  if (v8_flags.trace_unmapper) {
    PrintIsolate(
        heap_->isolate(),
        "Unmapper::PerformFreeMemoryOnQueuedChunks: %d queued chunks\n",
        NumberOfChunks());
  }
  // Regular chunks.
  while ((chunk = GetMemoryChunkSafe(ChunkQueueType::kRegular)) != nullptr) {
    bool pooled = chunk->IsFlagSet(MemoryChunk::POOLED);
    allocator_->PerformFreeMemory(chunk);
    if (pooled) AddMemoryChunkSafe(ChunkQueueType::kPooled, chunk);
    if (delegate && delegate->ShouldYield()) return;
  }
  if (mode == MemoryAllocator::Unmapper::FreeMode::kFreePooled) {
    // The previous loop uncommitted any pages marked as pooled and added them
    // to the pooled list. In case of kFreePooled we need to free them though as
    // well.
    while ((chunk = GetMemoryChunkSafe(ChunkQueueType::kPooled)) != nullptr) {
      allocator_->FreePooledChunk(chunk);
      if (delegate && delegate->ShouldYield()) return;
    }
  }
  PerformFreeMemoryOnQueuedNonRegularChunks();
}

void MemoryAllocator::Unmapper::TearDown() {
  CHECK(!job_handle_ || !job_handle_->IsValid());
  PerformFreeMemoryOnQueuedChunks(FreeMode::kFreePooled);
  for (int i = 0; i < ChunkQueueType::kNumberOfChunkQueues; i++) {
    DCHECK(chunks_[i].empty());
  }
}

size_t MemoryAllocator::Unmapper::NumberOfCommittedChunks() {
  base::MutexGuard guard(&mutex_);
  return chunks_[ChunkQueueType::kRegular].size() +
         chunks_[ChunkQueueType::kNonRegular].size();
}

int MemoryAllocator::Unmapper::NumberOfChunks() {
  base::MutexGuard guard(&mutex_);
  size_t result = 0;
  for (int i = 0; i < ChunkQueueType::kNumberOfChunkQueues; i++) {
    result += chunks_[i].size();
  }
  return static_cast<int>(result);
}

size_t MemoryAllocator::Unmapper::CommittedBufferedMemory() {
  base::MutexGuard guard(&mutex_);

  size_t sum = 0;
  // kPooled chunks are already uncommited. We only have to account for
  // kRegular and kNonRegular chunks.
  for (auto& chunk : chunks_[ChunkQueueType::kRegular]) {
    sum += chunk->size();
  }
  for (auto& chunk : chunks_[ChunkQueueType::kNonRegular]) {
    sum += chunk->size();
  }
  return sum;
}

bool MemoryAllocator::Unmapper::IsRunning() const {
  return job_handle_ && job_handle_->IsValid();
}

bool MemoryAllocator::CommitMemory(VirtualMemory* reservation) {
  Address base = reservation->address();
  size_t size = reservation->size();
  if (!reservation->SetPermissions(base, size, PageAllocator::kReadWrite)) {
    return false;
  }
  UpdateAllocatedSpaceLimits(base, base + size);
  return true;
}

bool MemoryAllocator::UncommitMemory(VirtualMemory* reservation) {
  size_t size = reservation->size();
  if (!reservation->SetPermissions(reservation->address(), size,
                                   PageAllocator::kNoAccess)) {
    return false;
  }
  return true;
}

void MemoryAllocator::FreeMemoryRegion(v8::PageAllocator* page_allocator,
                                       Address base, size_t size) {
  FreePages(page_allocator, reinterpret_cast<void*>(base), size);
}

Address MemoryAllocator::AllocateAlignedMemory(
    size_t chunk_size, size_t area_size, size_t alignment,
    AllocationSpace space, Executability executable, void* hint,
    VirtualMemory* controller) {
  v8::PageAllocator* page_allocator = this->page_allocator(executable);
  DCHECK_LT(area_size, chunk_size);

  VirtualMemory reservation(page_allocator, chunk_size, hint, alignment);
  if (!reservation.IsReserved()) return HandleAllocationFailure(executable);

  // We cannot use the last chunk in the address space because we would
  // overflow when comparing top and limit if this chunk is used for a
  // linear allocation area.
  if ((reservation.address() + static_cast<Address>(chunk_size)) == 0u) {
    CHECK(!reserved_chunk_at_virtual_memory_limit_);
    reserved_chunk_at_virtual_memory_limit_ = std::move(reservation);
    CHECK(reserved_chunk_at_virtual_memory_limit_);

    // Retry reserve virtual memory.
    reservation = VirtualMemory(page_allocator, chunk_size, hint, alignment);
    if (!reservation.IsReserved()) return HandleAllocationFailure(executable);
  }

  Address base = reservation.address();

  if (executable == EXECUTABLE) {
    if (!SetPermissionsOnExecutableMemoryChunk(&reservation, base, area_size,
                                               chunk_size)) {
      return HandleAllocationFailure(executable);
    }
  } else {
    // No guard page between page header and object area. This allows us to make
    // all OS pages for both regions readable+writable at once.
    const size_t commit_size = ::RoundUp(
        MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(space) + area_size,
        GetCommitPageSize());

    if (reservation.SetPermissions(base, commit_size,
                                   PageAllocator::kReadWrite)) {
      UpdateAllocatedSpaceLimits(base, base + commit_size);
    } else {
      return HandleAllocationFailure(executable);
    }
  }

  *controller = std::move(reservation);
  return base;
}

Address MemoryAllocator::HandleAllocationFailure(Executability executable) {
  Heap* heap = isolate_->heap();
  if (!heap->deserialization_complete()) {
    heap->FatalProcessOutOfMemory(
        executable == EXECUTABLE
            ? "Executable MemoryChunk allocation failed during deserialization."
            : "MemoryChunk allocation failed during deserialization.");
  }
  return kNullAddress;
}

size_t MemoryAllocator::ComputeChunkSize(size_t area_size,
                                         AllocationSpace space,
                                         Executability executable) {
  if (executable == EXECUTABLE) {
    //
    //             Executable
    // +----------------------------+<- base aligned at MemoryChunk::kAlignment
    // |           Header           |
    // +----------------------------+<- base + CodePageGuardStartOffset
    // |           Guard            |
    // +----------------------------+<- area_start_
    // |           Area             |
    // +----------------------------+<- area_end_ (area_start + area_size)
    // |   Committed but not used   |
    // +----------------------------+<- aligned at OS page boundary
    // |           Guard            |
    // +----------------------------+<- base + chunk_size
    //

    return ::RoundUp(MemoryChunkLayout::ObjectStartOffsetInCodePage() +
                         area_size + MemoryChunkLayout::CodePageGuardSize(),
                     GetCommitPageSize());
  }

  //
  //           Non-executable
  // +----------------------------+<- base aligned at MemoryChunk::kAlignment
  // |          Header            |
  // +----------------------------+<- area_start_ (base + area_start_)
  // |           Area             |
  // +----------------------------+<- area_end_ (area_start + area_size)
  // |  Committed but not used    |
  // +----------------------------+<- base + chunk_size
  //
  DCHECK_EQ(executable, NOT_EXECUTABLE);

  return ::RoundUp(
      MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(space) + area_size,
      GetCommitPageSize());
}

base::Optional<MemoryAllocator::MemoryChunkAllocationResult>
MemoryAllocator::AllocateUninitializedChunkAt(BaseSpace* space,
                                              size_t area_size,
                                              Executability executable,
                                              Address hint,
                                              PageSize page_size) {
#ifndef V8_COMPRESS_POINTERS
  // When pointer compression is enabled, spaces are expected to be at a
  // predictable address (see mkgrokdump) so we don't supply a hint and rely on
  // the deterministic behaviour of the BoundedPageAllocator.
  if (hint == kNullAddress) {
    hint = reinterpret_cast<Address>(AlignedAddress(
        isolate_->heap()->GetRandomMmapAddr(), MemoryChunk::kAlignment));
  }
#endif

  VirtualMemory reservation;
  size_t chunk_size =
      ComputeChunkSize(area_size, space->identity(), executable);
  DCHECK_EQ(chunk_size % GetCommitPageSize(), 0);

  Address base = AllocateAlignedMemory(
      chunk_size, area_size, MemoryChunk::kAlignment, space->identity(),
      executable, reinterpret_cast<void*>(hint), &reservation);
  if (base == kNullAddress) return {};

  size_ += reservation.size();

  // Update executable memory size.
  if (executable == EXECUTABLE) {
    size_executable_ += reservation.size();
  }

  if (Heap::ShouldZapGarbage()) {
    if (executable == EXECUTABLE) {
      // Page header and object area is split by guard page. Zap page header
      // first.
      ZapBlock(base, MemoryChunkLayout::CodePageGuardStartOffset(), kZapValue);
      // Now zap object area.
      ZapBlock(base + MemoryChunkLayout::ObjectStartOffsetInCodePage(),
               area_size, kZapValue);
    } else {
      DCHECK_EQ(executable, NOT_EXECUTABLE);
      // Zap both page header and object area at once. No guard page in-between.
      ZapBlock(
          base,
          MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(space->identity()) +
              area_size,
          kZapValue);
    }
  }

  LOG(isolate_,
      NewEvent("MemoryChunk", reinterpret_cast<void*>(base), chunk_size));

  Address area_start = base + MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(
                                  space->identity());
  Address area_end = area_start + area_size;

  return MemoryChunkAllocationResult{
      reinterpret_cast<void*>(base), chunk_size, area_start, area_end,
      std::move(reservation),
  };
}

void MemoryAllocator::PartialFreeMemory(BasicMemoryChunk* chunk,
                                        Address start_free,
                                        size_t bytes_to_free,
                                        Address new_area_end) {
  VirtualMemory* reservation = chunk->reserved_memory();
  DCHECK(reservation->IsReserved());
  chunk->set_size(chunk->size() - bytes_to_free);
  chunk->set_area_end(new_area_end);
  if (chunk->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
    // Add guard page at the end.
    size_t page_size = GetCommitPageSize();
    DCHECK_EQ(0, chunk->area_end() % static_cast<Address>(page_size));
    DCHECK_EQ(chunk->address() + chunk->size(),
              chunk->area_end() + MemoryChunkLayout::CodePageGuardSize());

    if (V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT && !isolate_->jitless()) {
      DCHECK(isolate_->RequiresCodeRange());
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

void MemoryAllocator::UnregisterSharedBasicMemoryChunk(
    BasicMemoryChunk* chunk) {
  VirtualMemory* reservation = chunk->reserved_memory();
  const size_t size =
      reservation->IsReserved() ? reservation->size() : chunk->size();
  DCHECK_GE(size_, static_cast<size_t>(size));
  size_ -= size;
}

void MemoryAllocator::UnregisterBasicMemoryChunk(BasicMemoryChunk* chunk,
                                                 Executability executable) {
  DCHECK(!chunk->IsFlagSet(MemoryChunk::UNREGISTERED));
  VirtualMemory* reservation = chunk->reserved_memory();
  const size_t size =
      reservation->IsReserved() ? reservation->size() : chunk->size();
  DCHECK_GE(size_, static_cast<size_t>(size));

  size_ -= size;
  if (executable == EXECUTABLE) {
    DCHECK_GE(size_executable_, size);
    size_executable_ -= size;
#ifdef DEBUG
    UnregisterExecutableMemoryChunk(static_cast<MemoryChunk*>(chunk));
#endif  // DEBUG
    chunk->heap()->UnregisterUnprotectedMemoryChunk(
        static_cast<MemoryChunk*>(chunk));
  }
  chunk->SetFlag(MemoryChunk::UNREGISTERED);
}

void MemoryAllocator::UnregisterMemoryChunk(MemoryChunk* chunk) {
  UnregisterBasicMemoryChunk(chunk, chunk->executable());
}

void MemoryAllocator::UnregisterReadOnlyPage(ReadOnlyPage* page) {
  DCHECK(!page->executable());
  UnregisterBasicMemoryChunk(page, NOT_EXECUTABLE);
}

void MemoryAllocator::FreeReadOnlyPage(ReadOnlyPage* chunk) {
  DCHECK(!chunk->IsFlagSet(MemoryChunk::PRE_FREED));
  LOG(isolate_, DeleteEvent("MemoryChunk", chunk));

  UnregisterSharedBasicMemoryChunk(chunk);

  v8::PageAllocator* allocator = page_allocator(NOT_EXECUTABLE);
  VirtualMemory* reservation = chunk->reserved_memory();
  if (reservation->IsReserved()) {
    reservation->FreeReadOnly();
  } else {
    // Only read-only pages can have a non-initialized reservation object. This
    // happens when the pages are remapped to multiple locations and where the
    // reservation would therefore be invalid.
    FreeMemoryRegion(allocator, chunk->address(),
                     RoundUp(chunk->size(), allocator->AllocatePageSize()));
  }
}

void MemoryAllocator::PreFreeMemory(MemoryChunk* chunk) {
  DCHECK(!chunk->IsFlagSet(MemoryChunk::PRE_FREED));
  LOG(isolate_, DeleteEvent("MemoryChunk", chunk));
  UnregisterMemoryChunk(chunk);
  isolate_->heap()->RememberUnmappedPage(reinterpret_cast<Address>(chunk),
                                         chunk->IsEvacuationCandidate());
  chunk->SetFlag(MemoryChunk::PRE_FREED);
}

void MemoryAllocator::PerformFreeMemory(MemoryChunk* chunk) {
  base::Optional<CodePageHeaderModificationScope> rwx_write_scope;
  if (chunk->executable() == EXECUTABLE) {
    rwx_write_scope.emplace(
        "We are going to modify the chunk's header, so ensure we have write "
        "access to Code page headers");
  }

  DCHECK(chunk->IsFlagSet(MemoryChunk::UNREGISTERED));
  DCHECK(chunk->IsFlagSet(MemoryChunk::PRE_FREED));
  DCHECK(!chunk->InReadOnlySpace());
  chunk->ReleaseAllAllocatedMemory();

  VirtualMemory* reservation = chunk->reserved_memory();
  if (chunk->IsFlagSet(MemoryChunk::POOLED)) {
    UncommitMemory(reservation);
  } else {
    DCHECK(reservation->IsReserved());
    reservation->Free();
  }
}

void MemoryAllocator::Free(MemoryAllocator::FreeMode mode, MemoryChunk* chunk) {
  if (chunk->IsLargePage())
    RecordLargePageDestroyed(*static_cast<LargePage*>(chunk));
  else
    RecordNormalPageDestroyed(*static_cast<Page*>(chunk));
  switch (mode) {
    case FreeMode::kImmediately:
      PreFreeMemory(chunk);
      PerformFreeMemory(chunk);
      break;
    case FreeMode::kConcurrentlyAndPool:
      DCHECK_EQ(chunk->size(), static_cast<size_t>(MemoryChunk::kPageSize));
      DCHECK_EQ(chunk->executable(), NOT_EXECUTABLE);
      chunk->SetFlag(MemoryChunk::POOLED);
      V8_FALLTHROUGH;
    case FreeMode::kConcurrently:
      PreFreeMemory(chunk);
      // The chunks added to this queue will be freed by a concurrent thread.
      unmapper()->AddMemoryChunkSafe(chunk);
      break;
  }
}

void MemoryAllocator::FreePooledChunk(MemoryChunk* chunk) {
  // Pooled pages cannot be touched anymore as their memory is uncommitted.
  // Pooled pages are not-executable.
  FreeMemoryRegion(data_page_allocator(), chunk->address(),
                   static_cast<size_t>(MemoryChunk::kPageSize));
}

Page* MemoryAllocator::AllocatePage(MemoryAllocator::AllocationMode alloc_mode,
                                    Space* space, Executability executable) {
  size_t size =
      MemoryChunkLayout::AllocatableMemoryInMemoryChunk(space->identity());
  base::Optional<MemoryChunkAllocationResult> chunk_info;
  if (alloc_mode == AllocationMode::kUsePool) {
    DCHECK_EQ(size, static_cast<size_t>(
                        MemoryChunkLayout::AllocatableMemoryInMemoryChunk(
                            space->identity())));
    DCHECK_EQ(executable, NOT_EXECUTABLE);
    chunk_info = AllocateUninitializedPageFromPool(space);
  }

  if (!chunk_info) {
    chunk_info =
        AllocateUninitializedChunk(space, size, executable, PageSize::kRegular);
  }

  if (!chunk_info) return nullptr;

  Page* page = new (chunk_info->start) Page(
      isolate_->heap(), space, chunk_info->size, chunk_info->area_start,
      chunk_info->area_end, std::move(chunk_info->reservation), executable);

#ifdef DEBUG
  if (page->executable()) RegisterExecutableMemoryChunk(page);
#endif  // DEBUG

  space->InitializePage(page);
  RecordNormalPageCreated(*page);
  return page;
}

ReadOnlyPage* MemoryAllocator::AllocateReadOnlyPage(ReadOnlySpace* space,
                                                    Address hint) {
  DCHECK_EQ(space->identity(), RO_SPACE);
  size_t size = MemoryChunkLayout::AllocatableMemoryInMemoryChunk(RO_SPACE);
  base::Optional<MemoryChunkAllocationResult> chunk_info =
      AllocateUninitializedChunkAt(space, size, NOT_EXECUTABLE, hint,
                                   PageSize::kRegular);
  if (!chunk_info) return nullptr;
  return new (chunk_info->start) ReadOnlyPage(
      isolate_->heap(), space, chunk_info->size, chunk_info->area_start,
      chunk_info->area_end, std::move(chunk_info->reservation));
}

std::unique_ptr<::v8::PageAllocator::SharedMemoryMapping>
MemoryAllocator::RemapSharedPage(
    ::v8::PageAllocator::SharedMemory* shared_memory, Address new_address) {
  return shared_memory->RemapTo(reinterpret_cast<void*>(new_address));
}

LargePage* MemoryAllocator::AllocateLargePage(LargeObjectSpace* space,
                                              size_t object_size,
                                              Executability executable) {
  base::Optional<MemoryChunkAllocationResult> chunk_info =
      AllocateUninitializedChunk(space, object_size, executable,
                                 PageSize::kLarge);

  if (!chunk_info) return nullptr;

  LargePage* page = new (chunk_info->start) LargePage(
      isolate_->heap(), space, chunk_info->size, chunk_info->area_start,
      chunk_info->area_end, std::move(chunk_info->reservation), executable);

#ifdef DEBUG
  if (page->executable()) RegisterExecutableMemoryChunk(page);
#endif  // DEBUG

  RecordLargePageCreated(*page);
  return page;
}

base::Optional<MemoryAllocator::MemoryChunkAllocationResult>
MemoryAllocator::AllocateUninitializedPageFromPool(Space* space) {
  void* chunk = unmapper()->TryGetPooledMemoryChunkSafe();
  if (chunk == nullptr) return {};
  const int size = MemoryChunk::kPageSize;
  const Address start = reinterpret_cast<Address>(chunk);
  const Address area_start =
      start +
      MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(space->identity());
  const Address area_end = start + size;
  // Pooled pages are always regular data pages.
  DCHECK_NE(CODE_SPACE, space->identity());
  VirtualMemory reservation(data_page_allocator(), start, size);
  if (!CommitMemory(&reservation)) return {};
  if (Heap::ShouldZapGarbage()) {
    ZapBlock(start, size, kZapValue);
  }

  size_ += size;
  return MemoryChunkAllocationResult{
      chunk, size, area_start, area_end, std::move(reservation),
  };
}

void MemoryAllocator::ZapBlock(Address start, size_t size,
                               uintptr_t zap_value) {
  DCHECK(IsAligned(start, kTaggedSize));
  DCHECK(IsAligned(size, kTaggedSize));
  MemsetTagged(ObjectSlot(start), Object(static_cast<Address>(zap_value)),
               size >> kTaggedSizeLog2);
}

void MemoryAllocator::InitializeOncePerProcess() {
  commit_page_size_ = v8_flags.v8_os_page_size > 0
                          ? v8_flags.v8_os_page_size * KB
                          : CommitPageSize();
  CHECK(base::bits::IsPowerOfTwo(commit_page_size_));
  commit_page_size_bits_ = base::bits::WhichPowerOfTwo(commit_page_size_);
}

base::AddressRegion MemoryAllocator::ComputeDiscardMemoryArea(Address addr,
                                                              size_t size) {
  size_t page_size = GetCommitPageSize();
  if (size < page_size + FreeSpace::kSize) {
    return base::AddressRegion(0, 0);
  }
  Address discardable_start = RoundUp(addr + FreeSpace::kSize, page_size);
  Address discardable_end = RoundDown(addr + size, page_size);
  if (discardable_start >= discardable_end) return base::AddressRegion(0, 0);
  return base::AddressRegion(discardable_start,
                             discardable_end - discardable_start);
}

bool MemoryAllocator::SetPermissionsOnExecutableMemoryChunk(VirtualMemory* vm,
                                                            Address start,
                                                            size_t area_size,
                                                            size_t chunk_size) {
  const size_t page_size = GetCommitPageSize();

  // The code area starts at an offset on the first page. To calculate the page
  // aligned size of the area, we have to add that offset and then round up to
  // commit page size.
  size_t area_offset = MemoryChunkLayout::ObjectStartOffsetInCodePage() -
                       MemoryChunkLayout::ObjectPageOffsetInCodePage();
  size_t aligned_area_size = RoundUp(area_offset + area_size, page_size);

  // All addresses and sizes must be aligned to the commit page size.
  DCHECK(IsAligned(start, page_size));
  DCHECK_EQ(0, chunk_size % page_size);

  const size_t guard_size = MemoryChunkLayout::CodePageGuardSize();
  const size_t pre_guard_offset = MemoryChunkLayout::CodePageGuardStartOffset();
  const size_t code_area_offset =
      MemoryChunkLayout::ObjectPageOffsetInCodePage();

  DCHECK_EQ(pre_guard_offset + guard_size + aligned_area_size + guard_size,
            chunk_size);

  const Address pre_guard_page = start + pre_guard_offset;
  const Address code_area = start + code_area_offset;
  const Address post_guard_page = start + chunk_size - guard_size;

  bool jitless = isolate_->jitless();

  if (V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT && !jitless) {
    DCHECK(isolate_->RequiresCodeRange());
    // Commit the header, from start to pre-code guard page.
    // We have to commit it as executable becase otherwise we'll not be able
    // to change permissions to anything else.
    if (vm->RecommitPages(start, pre_guard_offset,
                          PageAllocator::kReadWriteExecute)) {
      // Create the pre-code guard page, following the header.
      if (vm->DiscardSystemPages(pre_guard_page, page_size)) {
        // Commit the executable code body.
        if (vm->RecommitPages(code_area, aligned_area_size,
                              PageAllocator::kReadWriteExecute)) {
          // Create the post-code guard page.
          if (vm->DiscardSystemPages(post_guard_page, page_size)) {
            UpdateAllocatedSpaceLimits(start, code_area + aligned_area_size);
            return true;
          }

          vm->DiscardSystemPages(code_area, aligned_area_size);
        }
      }
      vm->DiscardSystemPages(start, pre_guard_offset);
    }

  } else {
    // Commit the non-executable header, from start to pre-code guard page.
    if (vm->SetPermissions(start, pre_guard_offset,
                           PageAllocator::kReadWrite)) {
      // Create the pre-code guard page, following the header.
      if (vm->SetPermissions(pre_guard_page, page_size,
                             PageAllocator::kNoAccess)) {
        // Commit the executable code body.
        bool set_permission_successed = false;
#if V8_HEAP_USE_PKU_JIT_WRITE_PROTECT
        if (!jitless && RwxMemoryWriteScope::IsSupported()) {
          base::AddressRegion region(code_area, aligned_area_size);
          set_permission_successed =
              base::MemoryProtectionKey::SetPermissionsAndKey(
                  code_page_allocator_, region,
                  PageAllocator::kReadWriteExecute,
                  RwxMemoryWriteScope::memory_protection_key());
        } else
#endif
        {
          set_permission_successed = vm->SetPermissions(
              code_area, aligned_area_size,
              jitless ? PageAllocator::kReadWrite
                      : MemoryChunk::GetCodeModificationPermission());
        }
        if (set_permission_successed) {
          // Create the post-code guard page.
          if (vm->SetPermissions(post_guard_page, page_size,
                                 PageAllocator::kNoAccess)) {
            UpdateAllocatedSpaceLimits(start, code_area + aligned_area_size);
            return true;
          }

          CHECK(vm->SetPermissions(code_area, aligned_area_size,
                                   PageAllocator::kNoAccess));
        }
      }
      CHECK(vm->SetPermissions(start, pre_guard_offset,
                               PageAllocator::kNoAccess));
    }
  }
  return false;
}

// static
const MemoryChunk* MemoryAllocator::LookupChunkContainingAddress(
    const NormalPagesSet& normal_pages, const LargePagesSet& large_pages,
    Address addr) {
  BasicMemoryChunk* chunk = BasicMemoryChunk::FromAddress(addr);
  if (auto it = normal_pages.find(static_cast<Page*>(chunk));
      it != normal_pages.end()) {
    // The chunk is a normal page.
    DCHECK_LE(chunk->address(), addr);
    if (chunk->Contains(addr)) return *it;
  } else if (auto it = large_pages.upper_bound(static_cast<LargePage*>(chunk));
             it != large_pages.begin()) {
    // The chunk could be inside a large page.
    DCHECK_IMPLIES(it != large_pages.end(), addr < (*it)->address());
    auto* large_page = *std::next(it, -1);
    DCHECK_NOT_NULL(large_page);
    DCHECK_LE(large_page->address(), addr);
    if (large_page->Contains(addr)) return large_page;
  }
  // Not found in any page.
  return nullptr;
}

const MemoryChunk* MemoryAllocator::LookupChunkContainingAddress(
    Address addr) const {
  // All threads should be either parked or in a safepoint whenever this method
  // is called, thus pages cannot be allocated or freed at the same time and a
  // mutex is not required here,
  return LookupChunkContainingAddress(normal_pages_, large_pages_, addr);
}

void MemoryAllocator::RecordNormalPageCreated(const Page& page) {
  base::MutexGuard guard(&pages_mutex_);
  auto result = normal_pages_.insert(&page);
  USE(result);
  DCHECK(result.second);
}

void MemoryAllocator::RecordNormalPageDestroyed(const Page& page) {
  base::MutexGuard guard(&pages_mutex_);
  DCHECK_IMPLIES(v8_flags.minor_mc && isolate_->heap()->sweeping_in_progress(),
                 isolate_->heap()->tracer()->IsInAtomicPause());
  auto size = normal_pages_.erase(&page);
  USE(size);
  DCHECK_EQ(1u, size);
}

void MemoryAllocator::RecordLargePageCreated(const LargePage& page) {
  base::MutexGuard guard(&pages_mutex_);
  auto result = large_pages_.insert(&page);
  USE(result);
  DCHECK(result.second);
}

void MemoryAllocator::RecordLargePageDestroyed(const LargePage& page) {
  base::MutexGuard guard(&pages_mutex_);
  DCHECK_IMPLIES(v8_flags.minor_mc && isolate_->heap()->sweeping_in_progress(),
                 isolate_->heap()->tracer()->IsInAtomicPause());
  auto size = large_pages_.erase(&page);
  USE(size);
  DCHECK_EQ(1u, size);
}

}  // namespace internal
}  // namespace v8

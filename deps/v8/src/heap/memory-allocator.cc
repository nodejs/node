// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-allocator.h"

#include <cinttypes>

#include "src/base/address-region.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-spaces.h"
#include "src/logging/log.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

static base::LazyInstance<CodeRangeAddressHint>::type code_range_address_hint =
    LAZY_INSTANCE_INITIALIZER;

Address CodeRangeAddressHint::GetAddressHint(size_t code_range_size) {
  base::MutexGuard guard(&mutex_);
  auto it = recently_freed_.find(code_range_size);
  if (it == recently_freed_.end() || it->second.empty()) {
    return reinterpret_cast<Address>(GetRandomMmapAddr());
  }
  Address result = it->second.back();
  it->second.pop_back();
  return result;
}

void CodeRangeAddressHint::NotifyFreedCodeRange(Address code_range_start,
                                                size_t code_range_size) {
  base::MutexGuard guard(&mutex_);
  recently_freed_[code_range_size].push_back(code_range_start);
}

// -----------------------------------------------------------------------------
// MemoryAllocator
//

MemoryAllocator::MemoryAllocator(Isolate* isolate, size_t capacity,
                                 size_t code_range_size)
    : isolate_(isolate),
      data_page_allocator_(isolate->page_allocator()),
      code_page_allocator_(nullptr),
      capacity_(RoundUp(capacity, Page::kPageSize)),
      size_(0),
      size_executable_(0),
      lowest_ever_allocated_(static_cast<Address>(-1ll)),
      highest_ever_allocated_(kNullAddress),
      unmapper_(isolate->heap(), this) {
  InitializeCodePageAllocator(data_page_allocator_, code_range_size);
}

void MemoryAllocator::InitializeCodePageAllocator(
    v8::PageAllocator* page_allocator, size_t requested) {
  DCHECK_NULL(code_page_allocator_instance_.get());

  code_page_allocator_ = page_allocator;

  if (requested == 0) {
    if (!isolate_->RequiresCodeRange()) return;
    // When a target requires the code range feature, we put all code objects
    // in a kMaximalCodeRangeSize range of virtual address space, so that
    // they can call each other with near calls.
    requested = kMaximalCodeRangeSize;
  } else if (requested <= kMinimumCodeRangeSize) {
    requested = kMinimumCodeRangeSize;
  }

  const size_t reserved_area =
      kReservedCodeRangePages * MemoryAllocator::GetCommitPageSize();
  if (requested < (kMaximalCodeRangeSize - reserved_area)) {
    requested += RoundUp(reserved_area, MemoryChunk::kPageSize);
    // Fullfilling both reserved pages requirement and huge code area
    // alignments is not supported (requires re-implementation).
    DCHECK_LE(kMinExpectedOSPageSize, page_allocator->AllocatePageSize());
  }
  DCHECK(!isolate_->RequiresCodeRange() || requested <= kMaximalCodeRangeSize);

  Address hint =
      RoundDown(code_range_address_hint.Pointer()->GetAddressHint(requested),
                page_allocator->AllocatePageSize());
  VirtualMemory reservation(
      page_allocator, requested, reinterpret_cast<void*>(hint),
      std::max(kMinExpectedOSPageSize, page_allocator->AllocatePageSize()));
  if (!reservation.IsReserved()) {
    V8::FatalProcessOutOfMemory(isolate_,
                                "CodeRange setup: allocate virtual memory");
  }
  code_range_ = reservation.region();
  isolate_->AddCodeRange(code_range_.begin(), code_range_.size());

  // We are sure that we have mapped a block of requested addresses.
  DCHECK_GE(reservation.size(), requested);
  Address base = reservation.address();

  // On some platforms, specifically Win64, we need to reserve some pages at
  // the beginning of an executable space. See
  //   https://cs.chromium.org/chromium/src/components/crash/content/
  //     app/crashpad_win.cc?rcl=fd680447881449fba2edcf0589320e7253719212&l=204
  // for details.
  if (reserved_area > 0) {
    if (!reservation.SetPermissions(base, reserved_area,
                                    PageAllocator::kReadWrite))
      V8::FatalProcessOutOfMemory(isolate_, "CodeRange setup: set permissions");

    base += reserved_area;
  }
  Address aligned_base = RoundUp(base, MemoryChunk::kAlignment);
  size_t size =
      RoundDown(reservation.size() - (aligned_base - base) - reserved_area,
                MemoryChunk::kPageSize);
  DCHECK(IsAligned(aligned_base, kMinExpectedOSPageSize));

  LOG(isolate_,
      NewEvent("CodeRange", reinterpret_cast<void*>(reservation.address()),
               requested));

  code_reservation_ = std::move(reservation);
  code_page_allocator_instance_ = std::make_unique<base::BoundedPageAllocator>(
      page_allocator, aligned_base, size,
      static_cast<size_t>(MemoryChunk::kAlignment));
  code_page_allocator_ = code_page_allocator_instance_.get();
}

void MemoryAllocator::TearDown() {
  unmapper()->TearDown();

  // Check that spaces were torn down before MemoryAllocator.
  DCHECK_EQ(size_, 0u);
  // TODO(gc) this will be true again when we fix FreeMemory.
  // DCHECK_EQ(0, size_executable_);
  capacity_ = 0;

  if (last_chunk_.IsReserved()) {
    last_chunk_.Free();
  }

  if (code_page_allocator_instance_.get()) {
    DCHECK(!code_range_.is_empty());
    code_range_address_hint.Pointer()->NotifyFreedCodeRange(code_range_.begin(),
                                                            code_range_.size());
    code_range_ = base::AddressRegion();
    code_page_allocator_instance_.reset();
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
    TRACE_GC1(tracer_, GCTracer::Scope::BACKGROUND_UNMAPPER,
              ThreadKind::kBackground);
    unmapper_->PerformFreeMemoryOnQueuedChunks<FreeMode::kUncommitPooled>(
        delegate);
    if (FLAG_trace_unmapper) {
      PrintIsolate(unmapper_->heap_->isolate(), "UnmapFreeMemoryTask Done\n");
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
  Unmapper* const unmapper_;
  GCTracer* const tracer_;
};

void MemoryAllocator::Unmapper::FreeQueuedChunks() {
  if (!heap_->IsTearingDown() && FLAG_concurrent_sweeping) {
    if (job_handle_ && job_handle_->IsValid()) {
      job_handle_->NotifyConcurrencyIncrease();
    } else {
      job_handle_ = V8::GetCurrentPlatform()->PostJob(
          TaskPriority::kUserVisible,
          std::make_unique<UnmapFreeMemoryJob>(heap_->isolate(), this));
      if (FLAG_trace_unmapper) {
        PrintIsolate(heap_->isolate(), "Unmapper::FreeQueuedChunks: new Job\n");
      }
    }
  } else {
    PerformFreeMemoryOnQueuedChunks<FreeMode::kUncommitPooled>();
  }
}

void MemoryAllocator::Unmapper::CancelAndWaitForPendingTasks() {
  if (job_handle_ && job_handle_->IsValid()) job_handle_->Join();

  if (FLAG_trace_unmapper) {
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
  PerformFreeMemoryOnQueuedChunks<FreeMode::kReleasePooled>();
}

void MemoryAllocator::Unmapper::PerformFreeMemoryOnQueuedNonRegularChunks(
    JobDelegate* delegate) {
  MemoryChunk* chunk = nullptr;
  while ((chunk = GetMemoryChunkSafe<kNonRegular>()) != nullptr) {
    allocator_->PerformFreeMemory(chunk);
    if (delegate && delegate->ShouldYield()) return;
  }
}

template <MemoryAllocator::Unmapper::FreeMode mode>
void MemoryAllocator::Unmapper::PerformFreeMemoryOnQueuedChunks(
    JobDelegate* delegate) {
  MemoryChunk* chunk = nullptr;
  if (FLAG_trace_unmapper) {
    PrintIsolate(
        heap_->isolate(),
        "Unmapper::PerformFreeMemoryOnQueuedChunks: %d queued chunks\n",
        NumberOfChunks());
  }
  // Regular chunks.
  while ((chunk = GetMemoryChunkSafe<kRegular>()) != nullptr) {
    bool pooled = chunk->IsFlagSet(MemoryChunk::POOLED);
    allocator_->PerformFreeMemory(chunk);
    if (pooled) AddMemoryChunkSafe<kPooled>(chunk);
    if (delegate && delegate->ShouldYield()) return;
  }
  if (mode == MemoryAllocator::Unmapper::FreeMode::kReleasePooled) {
    // The previous loop uncommitted any pages marked as pooled and added them
    // to the pooled list. In case of kReleasePooled we need to free them
    // though.
    while ((chunk = GetMemoryChunkSafe<kPooled>()) != nullptr) {
      allocator_->Free<MemoryAllocator::kAlreadyPooled>(chunk);
      if (delegate && delegate->ShouldYield()) return;
    }
  }
  PerformFreeMemoryOnQueuedNonRegularChunks();
}

void MemoryAllocator::Unmapper::TearDown() {
  CHECK(!job_handle_ || !job_handle_->IsValid());
  PerformFreeMemoryOnQueuedChunks<FreeMode::kReleasePooled>();
  for (int i = 0; i < kNumberOfChunkQueues; i++) {
    DCHECK(chunks_[i].empty());
  }
}

size_t MemoryAllocator::Unmapper::NumberOfCommittedChunks() {
  base::MutexGuard guard(&mutex_);
  return chunks_[kRegular].size() + chunks_[kNonRegular].size();
}

int MemoryAllocator::Unmapper::NumberOfChunks() {
  base::MutexGuard guard(&mutex_);
  size_t result = 0;
  for (int i = 0; i < kNumberOfChunkQueues; i++) {
    result += chunks_[i].size();
  }
  return static_cast<int>(result);
}

size_t MemoryAllocator::Unmapper::CommittedBufferedMemory() {
  base::MutexGuard guard(&mutex_);

  size_t sum = 0;
  // kPooled chunks are already uncommited. We only have to account for
  // kRegular and kNonRegular chunks.
  for (auto& chunk : chunks_[kRegular]) {
    sum += chunk->size();
  }
  for (auto& chunk : chunks_[kNonRegular]) {
    sum += chunk->size();
  }
  return sum;
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

void MemoryAllocator::FreeMemory(v8::PageAllocator* page_allocator,
                                 Address base, size_t size) {
  CHECK(FreePages(page_allocator, reinterpret_cast<void*>(base), size));
}

Address MemoryAllocator::AllocateAlignedMemory(
    size_t reserve_size, size_t commit_size, size_t alignment,
    Executability executable, void* hint, VirtualMemory* controller) {
  v8::PageAllocator* page_allocator = this->page_allocator(executable);
  DCHECK(commit_size <= reserve_size);
  VirtualMemory reservation(page_allocator, reserve_size, hint, alignment);
  if (!reservation.IsReserved()) return kNullAddress;
  Address base = reservation.address();
  size_ += reservation.size();

  if (executable == EXECUTABLE) {
    if (!CommitExecutableMemory(&reservation, base, commit_size,
                                reserve_size)) {
      base = kNullAddress;
    }
  } else {
    if (reservation.SetPermissions(base, commit_size,
                                   PageAllocator::kReadWrite)) {
      UpdateAllocatedSpaceLimits(base, base + commit_size);
    } else {
      base = kNullAddress;
    }
  }

  if (base == kNullAddress) {
    // Failed to commit the body. Free the mapping and any partially committed
    // regions inside it.
    reservation.Free();
    size_ -= reserve_size;
    return kNullAddress;
  }

  *controller = std::move(reservation);
  return base;
}

V8_EXPORT_PRIVATE BasicMemoryChunk* MemoryAllocator::AllocateBasicChunk(
    size_t reserve_area_size, size_t commit_area_size, Executability executable,
    BaseSpace* owner) {
  DCHECK_LE(commit_area_size, reserve_area_size);

  size_t chunk_size;
  Heap* heap = isolate_->heap();
  Address base = kNullAddress;
  VirtualMemory reservation;
  Address area_start = kNullAddress;
  Address area_end = kNullAddress;
  void* address_hint =
      AlignedAddress(heap->GetRandomMmapAddr(), MemoryChunk::kAlignment);

  //
  // MemoryChunk layout:
  //
  //             Executable
  // +----------------------------+<- base aligned with MemoryChunk::kAlignment
  // |           Header           |
  // +----------------------------+<- base + CodePageGuardStartOffset
  // |           Guard            |
  // +----------------------------+<- area_start_
  // |           Area             |
  // +----------------------------+<- area_end_ (area_start + commit_area_size)
  // |   Committed but not used   |
  // +----------------------------+<- aligned at OS page boundary
  // | Reserved but not committed |
  // +----------------------------+<- aligned at OS page boundary
  // |           Guard            |
  // +----------------------------+<- base + chunk_size
  //
  //           Non-executable
  // +----------------------------+<- base aligned with MemoryChunk::kAlignment
  // |          Header            |
  // +----------------------------+<- area_start_ (base + area_start_)
  // |           Area             |
  // +----------------------------+<- area_end_ (area_start + commit_area_size)
  // |  Committed but not used    |
  // +----------------------------+<- aligned at OS page boundary
  // | Reserved but not committed |
  // +----------------------------+<- base + chunk_size
  //

  if (executable == EXECUTABLE) {
    chunk_size = ::RoundUp(MemoryChunkLayout::ObjectStartOffsetInCodePage() +
                               reserve_area_size +
                               MemoryChunkLayout::CodePageGuardSize(),
                           GetCommitPageSize());

    // Size of header (not executable) plus area (executable).
    size_t commit_size = ::RoundUp(
        MemoryChunkLayout::CodePageGuardStartOffset() + commit_area_size,
        GetCommitPageSize());
    base =
        AllocateAlignedMemory(chunk_size, commit_size, MemoryChunk::kAlignment,
                              executable, address_hint, &reservation);
    if (base == kNullAddress) return nullptr;
    // Update executable memory size.
    size_executable_ += reservation.size();

    if (Heap::ShouldZapGarbage()) {
      ZapBlock(base, MemoryChunkLayout::CodePageGuardStartOffset(), kZapValue);
      ZapBlock(base + MemoryChunkLayout::ObjectStartOffsetInCodePage(),
               commit_area_size, kZapValue);
    }

    area_start = base + MemoryChunkLayout::ObjectStartOffsetInCodePage();
    area_end = area_start + commit_area_size;
  } else {
    chunk_size = ::RoundUp(
        MemoryChunkLayout::ObjectStartOffsetInDataPage() + reserve_area_size,
        GetCommitPageSize());
    size_t commit_size = ::RoundUp(
        MemoryChunkLayout::ObjectStartOffsetInDataPage() + commit_area_size,
        GetCommitPageSize());
    base =
        AllocateAlignedMemory(chunk_size, commit_size, MemoryChunk::kAlignment,
                              executable, address_hint, &reservation);

    if (base == kNullAddress) return nullptr;

    if (Heap::ShouldZapGarbage()) {
      ZapBlock(
          base,
          MemoryChunkLayout::ObjectStartOffsetInDataPage() + commit_area_size,
          kZapValue);
    }

    area_start = base + MemoryChunkLayout::ObjectStartOffsetInDataPage();
    area_end = area_start + commit_area_size;
  }

  // Use chunk_size for statistics because we assume that  treat reserved but
  // not-yet committed memory regions of chunks as allocated.
  LOG(isolate_,
      NewEvent("MemoryChunk", reinterpret_cast<void*>(base), chunk_size));

  // We cannot use the last chunk in the address space because we would
  // overflow when comparing top and limit if this chunk is used for a
  // linear allocation area.
  if ((base + chunk_size) == 0u) {
    CHECK(!last_chunk_.IsReserved());
    last_chunk_ = std::move(reservation);
    UncommitMemory(&last_chunk_);
    size_ -= chunk_size;
    if (executable == EXECUTABLE) {
      size_executable_ -= chunk_size;
    }
    CHECK(last_chunk_.IsReserved());
    return AllocateBasicChunk(reserve_area_size, commit_area_size, executable,
                              owner);
  }

  BasicMemoryChunk* chunk =
      BasicMemoryChunk::Initialize(heap, base, chunk_size, area_start, area_end,
                                   owner, std::move(reservation));

  return chunk;
}

MemoryChunk* MemoryAllocator::AllocateChunk(size_t reserve_area_size,
                                            size_t commit_area_size,
                                            Executability executable,
                                            BaseSpace* owner) {
  BasicMemoryChunk* basic_chunk = AllocateBasicChunk(
      reserve_area_size, commit_area_size, executable, owner);

  if (basic_chunk == nullptr) return nullptr;

  MemoryChunk* chunk =
      MemoryChunk::Initialize(basic_chunk, isolate_->heap(), executable);

  if (chunk->executable()) RegisterExecutableMemoryChunk(chunk);
  return chunk;
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
    reservation->SetPermissions(chunk->area_end(), page_size,
                                PageAllocator::kNoAccess);
  }
  // On e.g. Windows, a reservation may be larger than a page and releasing
  // partially starting at |start_free| will also release the potentially
  // unused part behind the current page.
  const size_t released_bytes = reservation->Release(start_free);
  DCHECK_GE(size_, released_bytes);
  size_ -= released_bytes;
}

void MemoryAllocator::UnregisterSharedMemory(BasicMemoryChunk* chunk) {
  VirtualMemory* reservation = chunk->reserved_memory();
  const size_t size =
      reservation->IsReserved() ? reservation->size() : chunk->size();
  DCHECK_GE(size_, static_cast<size_t>(size));
  size_ -= size;
}

void MemoryAllocator::UnregisterMemory(BasicMemoryChunk* chunk,
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
    UnregisterExecutableMemoryChunk(static_cast<MemoryChunk*>(chunk));
  }
  chunk->SetFlag(MemoryChunk::UNREGISTERED);
}

void MemoryAllocator::UnregisterMemory(MemoryChunk* chunk) {
  UnregisterMemory(chunk, chunk->executable());
}

void MemoryAllocator::FreeReadOnlyPage(ReadOnlyPage* chunk) {
  DCHECK(!chunk->IsFlagSet(MemoryChunk::PRE_FREED));
  LOG(isolate_, DeleteEvent("MemoryChunk", chunk));

  UnregisterSharedMemory(chunk);

  v8::PageAllocator* allocator = page_allocator(NOT_EXECUTABLE);
  VirtualMemory* reservation = chunk->reserved_memory();
  if (reservation->IsReserved()) {
    reservation->FreeReadOnly();
  } else {
    // Only read-only pages can have a non-initialized reservation object. This
    // happens when the pages are remapped to multiple locations and where the
    // reservation would therefore be invalid.
    FreeMemory(allocator, chunk->address(),
               RoundUp(chunk->size(), allocator->AllocatePageSize()));
  }
}

void MemoryAllocator::PreFreeMemory(MemoryChunk* chunk) {
  DCHECK(!chunk->IsFlagSet(MemoryChunk::PRE_FREED));
  LOG(isolate_, DeleteEvent("MemoryChunk", chunk));
  UnregisterMemory(chunk);
  isolate_->heap()->RememberUnmappedPage(reinterpret_cast<Address>(chunk),
                                         chunk->IsEvacuationCandidate());
  chunk->SetFlag(MemoryChunk::PRE_FREED);
}

void MemoryAllocator::PerformFreeMemory(MemoryChunk* chunk) {
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

template <MemoryAllocator::FreeMode mode>
void MemoryAllocator::Free(MemoryChunk* chunk) {
  switch (mode) {
    case kFull:
      PreFreeMemory(chunk);
      PerformFreeMemory(chunk);
      break;
    case kAlreadyPooled:
      // Pooled pages cannot be touched anymore as their memory is uncommitted.
      // Pooled pages are not-executable.
      FreeMemory(data_page_allocator(), chunk->address(),
                 static_cast<size_t>(MemoryChunk::kPageSize));
      break;
    case kPooledAndQueue:
      DCHECK_EQ(chunk->size(), static_cast<size_t>(MemoryChunk::kPageSize));
      DCHECK_EQ(chunk->executable(), NOT_EXECUTABLE);
      chunk->SetFlag(MemoryChunk::POOLED);
      V8_FALLTHROUGH;
    case kPreFreeAndQueue:
      PreFreeMemory(chunk);
      // The chunks added to this queue will be freed by a concurrent thread.
      unmapper()->AddMemoryChunkSafe(chunk);
      break;
  }
}

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void MemoryAllocator::Free<
    MemoryAllocator::kFull>(MemoryChunk* chunk);

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void MemoryAllocator::Free<
    MemoryAllocator::kAlreadyPooled>(MemoryChunk* chunk);

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void MemoryAllocator::Free<
    MemoryAllocator::kPreFreeAndQueue>(MemoryChunk* chunk);

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void MemoryAllocator::Free<
    MemoryAllocator::kPooledAndQueue>(MemoryChunk* chunk);

template <MemoryAllocator::AllocationMode alloc_mode, typename SpaceType>
Page* MemoryAllocator::AllocatePage(size_t size, SpaceType* owner,
                                    Executability executable) {
  MemoryChunk* chunk = nullptr;
  if (alloc_mode == kPooled) {
    DCHECK_EQ(size, static_cast<size_t>(
                        MemoryChunkLayout::AllocatableMemoryInMemoryChunk(
                            owner->identity())));
    DCHECK_EQ(executable, NOT_EXECUTABLE);
    chunk = AllocatePagePooled(owner);
  }
  if (chunk == nullptr) {
    chunk = AllocateChunk(size, size, executable, owner);
  }
  if (chunk == nullptr) return nullptr;
  return owner->InitializePage(chunk);
}

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Page* MemoryAllocator::AllocatePage<MemoryAllocator::kRegular, PagedSpace>(
        size_t size, PagedSpace* owner, Executability executable);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Page* MemoryAllocator::AllocatePage<MemoryAllocator::kRegular, SemiSpace>(
        size_t size, SemiSpace* owner, Executability executable);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Page* MemoryAllocator::AllocatePage<MemoryAllocator::kPooled, SemiSpace>(
        size_t size, SemiSpace* owner, Executability executable);

ReadOnlyPage* MemoryAllocator::AllocateReadOnlyPage(size_t size,
                                                    ReadOnlySpace* owner) {
  BasicMemoryChunk* chunk = nullptr;
  if (chunk == nullptr) {
    chunk = AllocateBasicChunk(size, size, NOT_EXECUTABLE, owner);
  }
  if (chunk == nullptr) return nullptr;
  return owner->InitializePage(chunk);
}

std::unique_ptr<::v8::PageAllocator::SharedMemoryMapping>
MemoryAllocator::RemapSharedPage(
    ::v8::PageAllocator::SharedMemory* shared_memory, Address new_address) {
  return shared_memory->RemapTo(reinterpret_cast<void*>(new_address));
}

LargePage* MemoryAllocator::AllocateLargePage(size_t size,
                                              LargeObjectSpace* owner,
                                              Executability executable) {
  MemoryChunk* chunk = AllocateChunk(size, size, executable, owner);
  if (chunk == nullptr) return nullptr;
  return LargePage::Initialize(isolate_->heap(), chunk, executable);
}

template <typename SpaceType>
MemoryChunk* MemoryAllocator::AllocatePagePooled(SpaceType* owner) {
  MemoryChunk* chunk = unmapper()->TryGetPooledMemoryChunkSafe();
  if (chunk == nullptr) return nullptr;
  const int size = MemoryChunk::kPageSize;
  const Address start = reinterpret_cast<Address>(chunk);
  const Address area_start =
      start +
      MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(owner->identity());
  const Address area_end = start + size;
  // Pooled pages are always regular data pages.
  DCHECK_NE(CODE_SPACE, owner->identity());
  VirtualMemory reservation(data_page_allocator(), start, size);
  if (!CommitMemory(&reservation)) return nullptr;
  if (Heap::ShouldZapGarbage()) {
    ZapBlock(start, size, kZapValue);
  }
  BasicMemoryChunk* basic_chunk =
      BasicMemoryChunk::Initialize(isolate_->heap(), start, size, area_start,
                                   area_end, owner, std::move(reservation));
  MemoryChunk::Initialize(basic_chunk, isolate_->heap(), NOT_EXECUTABLE);
  size_ += size;
  return chunk;
}

void MemoryAllocator::ZapBlock(Address start, size_t size,
                               uintptr_t zap_value) {
  DCHECK(IsAligned(start, kTaggedSize));
  DCHECK(IsAligned(size, kTaggedSize));
  MemsetTagged(ObjectSlot(start), Object(static_cast<Address>(zap_value)),
               size >> kTaggedSizeLog2);
}

intptr_t MemoryAllocator::GetCommitPageSize() {
  if (FLAG_v8_os_page_size != 0) {
    DCHECK(base::bits::IsPowerOfTwo(FLAG_v8_os_page_size));
    return FLAG_v8_os_page_size * KB;
  } else {
    return CommitPageSize();
  }
}

base::AddressRegion MemoryAllocator::ComputeDiscardMemoryArea(Address addr,
                                                              size_t size) {
  size_t page_size = MemoryAllocator::GetCommitPageSize();
  if (size < page_size + FreeSpace::kSize) {
    return base::AddressRegion(0, 0);
  }
  Address discardable_start = RoundUp(addr + FreeSpace::kSize, page_size);
  Address discardable_end = RoundDown(addr + size, page_size);
  if (discardable_start >= discardable_end) return base::AddressRegion(0, 0);
  return base::AddressRegion(discardable_start,
                             discardable_end - discardable_start);
}

bool MemoryAllocator::CommitExecutableMemory(VirtualMemory* vm, Address start,
                                             size_t commit_size,
                                             size_t reserved_size) {
  const size_t page_size = GetCommitPageSize();
  // All addresses and sizes must be aligned to the commit page size.
  DCHECK(IsAligned(start, page_size));
  DCHECK_EQ(0, commit_size % page_size);
  DCHECK_EQ(0, reserved_size % page_size);
  const size_t guard_size = MemoryChunkLayout::CodePageGuardSize();
  const size_t pre_guard_offset = MemoryChunkLayout::CodePageGuardStartOffset();
  const size_t code_area_offset =
      MemoryChunkLayout::ObjectStartOffsetInCodePage();
  // reserved_size includes two guard regions, commit_size does not.
  DCHECK_LE(commit_size, reserved_size - 2 * guard_size);
  const Address pre_guard_page = start + pre_guard_offset;
  const Address code_area = start + code_area_offset;
  const Address post_guard_page = start + reserved_size - guard_size;
  // Commit the non-executable header, from start to pre-code guard page.
  if (vm->SetPermissions(start, pre_guard_offset, PageAllocator::kReadWrite)) {
    // Create the pre-code guard page, following the header.
    if (vm->SetPermissions(pre_guard_page, page_size,
                           PageAllocator::kNoAccess)) {
      // Commit the executable code body.
      if (vm->SetPermissions(code_area, commit_size - pre_guard_offset,
                             PageAllocator::kReadWrite)) {
        // Create the post-code guard page.
        if (vm->SetPermissions(post_guard_page, page_size,
                               PageAllocator::kNoAccess)) {
          UpdateAllocatedSpaceLimits(start, code_area + commit_size);
          return true;
        }
        vm->SetPermissions(code_area, commit_size, PageAllocator::kNoAccess);
      }
    }
    vm->SetPermissions(start, pre_guard_offset, PageAllocator::kNoAccess);
  }
  return false;
}

}  // namespace internal
}  // namespace v8

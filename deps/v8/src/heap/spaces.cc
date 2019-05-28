// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/spaces.h"

#include <utility>

#include "src/base/bits.h"
#include "src/base/macros.h"
#include "src/base/platform/semaphore.h"
#include "src/base/template-utils.h"
#include "src/counters.h"
#include "src/heap/array-buffer-tracker.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-controller.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/remembered-set.h"
#include "src/heap/slot-set.h"
#include "src/heap/sweeper.h"
#include "src/msan.h"
#include "src/objects-inl.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/ostreams.h"
#include "src/snapshot/snapshot.h"
#include "src/v8.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

// These checks are here to ensure that the lower 32 bits of any real heap
// object can't overlap with the lower 32 bits of cleared weak reference value
// and therefore it's enough to compare only the lower 32 bits of a MaybeObject
// in order to figure out if it's a cleared weak reference or not.
STATIC_ASSERT(kClearedWeakHeapObjectLower32 > 0);
STATIC_ASSERT(kClearedWeakHeapObjectLower32 < Page::kHeaderSize);
STATIC_ASSERT(kClearedWeakHeapObjectLower32 < LargePage::kHeaderSize);

// ----------------------------------------------------------------------------
// HeapObjectIterator

HeapObjectIterator::HeapObjectIterator(PagedSpace* space)
    : cur_addr_(kNullAddress),
      cur_end_(kNullAddress),
      space_(space),
      page_range_(space->first_page(), nullptr),
      current_page_(page_range_.begin()) {}

HeapObjectIterator::HeapObjectIterator(Page* page)
    : cur_addr_(kNullAddress),
      cur_end_(kNullAddress),
      space_(reinterpret_cast<PagedSpace*>(page->owner())),
      page_range_(page),
      current_page_(page_range_.begin()) {
#ifdef DEBUG
  Space* owner = page->owner();
  DCHECK(owner == page->heap()->old_space() ||
         owner == page->heap()->map_space() ||
         owner == page->heap()->code_space() ||
         owner == page->heap()->read_only_space());
#endif  // DEBUG
}

// We have hit the end of the page and should advance to the next block of
// objects.  This happens at the end of the page.
bool HeapObjectIterator::AdvanceToNextPage() {
  DCHECK_EQ(cur_addr_, cur_end_);
  if (current_page_ == page_range_.end()) return false;
  Page* cur_page = *(current_page_++);
  Heap* heap = space_->heap();

  heap->mark_compact_collector()->sweeper()->EnsurePageIsIterable(cur_page);
#ifdef ENABLE_MINOR_MC
  if (cur_page->IsFlagSet(Page::SWEEP_TO_ITERATE))
    heap->minor_mark_compact_collector()->MakeIterable(
        cur_page, MarkingTreatmentMode::CLEAR,
        FreeSpaceTreatmentMode::IGNORE_FREE_SPACE);
#else
  DCHECK(!cur_page->IsFlagSet(Page::SWEEP_TO_ITERATE));
#endif  // ENABLE_MINOR_MC
  cur_addr_ = cur_page->area_start();
  cur_end_ = cur_page->area_end();
  DCHECK(cur_page->SweepingDone());
  return true;
}

PauseAllocationObserversScope::PauseAllocationObserversScope(Heap* heap)
    : heap_(heap) {
  DCHECK_EQ(heap->gc_state(), Heap::NOT_IN_GC);

  for (SpaceIterator it(heap_); it.has_next();) {
    it.next()->PauseAllocationObservers();
  }
}

PauseAllocationObserversScope::~PauseAllocationObserversScope() {
  for (SpaceIterator it(heap_); it.has_next();) {
    it.next()->ResumeAllocationObservers();
  }
}

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
    if (!kRequiresCodeRange) return;
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
  DCHECK(!kRequiresCodeRange || requested <= kMaximalCodeRangeSize);

  Address hint =
      RoundDown(code_range_address_hint.Pointer()->GetAddressHint(requested),
                page_allocator->AllocatePageSize());
  VirtualMemory reservation(
      page_allocator, requested, reinterpret_cast<void*>(hint),
      Max(kMinExpectedOSPageSize, page_allocator->AllocatePageSize()));
  if (!reservation.IsReserved()) {
    V8::FatalProcessOutOfMemory(isolate_,
                                "CodeRange setup: allocate virtual memory");
  }
  code_range_ = reservation.region();

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

  heap_reservation_.TakeControl(&reservation);
  code_page_allocator_instance_ = base::make_unique<base::BoundedPageAllocator>(
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

class MemoryAllocator::Unmapper::UnmapFreeMemoryTask : public CancelableTask {
 public:
  explicit UnmapFreeMemoryTask(Isolate* isolate, Unmapper* unmapper)
      : CancelableTask(isolate),
        unmapper_(unmapper),
        tracer_(isolate->heap()->tracer()) {}

 private:
  void RunInternal() override {
    TRACE_BACKGROUND_GC(tracer_,
                        GCTracer::BackgroundScope::BACKGROUND_UNMAPPER);
    unmapper_->PerformFreeMemoryOnQueuedChunks<FreeMode::kUncommitPooled>();
    unmapper_->active_unmapping_tasks_--;
    unmapper_->pending_unmapping_tasks_semaphore_.Signal();
    if (FLAG_trace_unmapper) {
      PrintIsolate(unmapper_->heap_->isolate(),
                   "UnmapFreeMemoryTask Done: id=%" PRIu64 "\n", id());
    }
  }

  Unmapper* const unmapper_;
  GCTracer* const tracer_;
  DISALLOW_COPY_AND_ASSIGN(UnmapFreeMemoryTask);
};

void MemoryAllocator::Unmapper::FreeQueuedChunks() {
  if (!heap_->IsTearingDown() && FLAG_concurrent_sweeping) {
    if (!MakeRoomForNewTasks()) {
      // kMaxUnmapperTasks are already running. Avoid creating any more.
      if (FLAG_trace_unmapper) {
        PrintIsolate(heap_->isolate(),
                     "Unmapper::FreeQueuedChunks: reached task limit (%d)\n",
                     kMaxUnmapperTasks);
      }
      return;
    }
    auto task = base::make_unique<UnmapFreeMemoryTask>(heap_->isolate(), this);
    if (FLAG_trace_unmapper) {
      PrintIsolate(heap_->isolate(),
                   "Unmapper::FreeQueuedChunks: new task id=%" PRIu64 "\n",
                   task->id());
    }
    DCHECK_LT(pending_unmapping_tasks_, kMaxUnmapperTasks);
    DCHECK_LE(active_unmapping_tasks_, pending_unmapping_tasks_);
    DCHECK_GE(active_unmapping_tasks_, 0);
    active_unmapping_tasks_++;
    task_ids_[pending_unmapping_tasks_++] = task->id();
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
  } else {
    PerformFreeMemoryOnQueuedChunks<FreeMode::kUncommitPooled>();
  }
}

void MemoryAllocator::Unmapper::CancelAndWaitForPendingTasks() {
  for (int i = 0; i < pending_unmapping_tasks_; i++) {
    if (heap_->isolate()->cancelable_task_manager()->TryAbort(task_ids_[i]) !=
        TryAbortResult::kTaskAborted) {
      pending_unmapping_tasks_semaphore_.Wait();
    }
  }
  pending_unmapping_tasks_ = 0;
  active_unmapping_tasks_ = 0;

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

bool MemoryAllocator::Unmapper::MakeRoomForNewTasks() {
  DCHECK_LE(pending_unmapping_tasks_, kMaxUnmapperTasks);

  if (active_unmapping_tasks_ == 0 && pending_unmapping_tasks_ > 0) {
    // All previous unmapping tasks have been run to completion.
    // Finalize those tasks to make room for new ones.
    CancelAndWaitForPendingTasks();
  }
  return pending_unmapping_tasks_ != kMaxUnmapperTasks;
}

void MemoryAllocator::Unmapper::PerformFreeMemoryOnQueuedNonRegularChunks() {
  MemoryChunk* chunk = nullptr;
  while ((chunk = GetMemoryChunkSafe<kNonRegular>()) != nullptr) {
    allocator_->PerformFreeMemory(chunk);
  }
}

template <MemoryAllocator::Unmapper::FreeMode mode>
void MemoryAllocator::Unmapper::PerformFreeMemoryOnQueuedChunks() {
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
  }
  if (mode == MemoryAllocator::Unmapper::FreeMode::kReleasePooled) {
    // The previous loop uncommitted any pages marked as pooled and added them
    // to the pooled list. In case of kReleasePooled we need to free them
    // though.
    while ((chunk = GetMemoryChunkSafe<kPooled>()) != nullptr) {
      allocator_->Free<MemoryAllocator::kAlreadyPooled>(chunk);
    }
  }
  PerformFreeMemoryOnQueuedNonRegularChunks();
}

void MemoryAllocator::Unmapper::TearDown() {
  CHECK_EQ(0, pending_unmapping_tasks_);
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
  isolate_->counters()->memory_allocated()->Increment(static_cast<int>(size));
  return true;
}

bool MemoryAllocator::UncommitMemory(VirtualMemory* reservation) {
  size_t size = reservation->size();
  if (!reservation->SetPermissions(reservation->address(), size,
                                   PageAllocator::kNoAccess)) {
    return false;
  }
  isolate_->counters()->memory_allocated()->Decrement(static_cast<int>(size));
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

  controller->TakeControl(&reservation);
  return base;
}

void MemoryChunk::DiscardUnusedMemory(Address addr, size_t size) {
  base::AddressRegion memory_area =
      MemoryAllocator::ComputeDiscardMemoryArea(addr, size);
  if (memory_area.size() != 0) {
    MemoryAllocator* memory_allocator = heap_->memory_allocator();
    v8::PageAllocator* page_allocator =
        memory_allocator->page_allocator(executable());
    CHECK(page_allocator->DiscardSystemPages(
        reinterpret_cast<void*>(memory_area.begin()), memory_area.size()));
  }
}

size_t MemoryChunkLayout::CodePageGuardStartOffset() {
  // We are guarding code pages: the first OS page after the header
  // will be protected as non-writable.
  return ::RoundUp(Page::kHeaderSize, MemoryAllocator::GetCommitPageSize());
}

size_t MemoryChunkLayout::CodePageGuardSize() {
  return MemoryAllocator::GetCommitPageSize();
}

intptr_t MemoryChunkLayout::ObjectStartOffsetInCodePage() {
  // We are guarding code pages: the first OS page after the header
  // will be protected as non-writable.
  return CodePageGuardStartOffset() + CodePageGuardSize();
}

intptr_t MemoryChunkLayout::ObjectEndOffsetInCodePage() {
  // We are guarding code pages: the last OS page will be protected as
  // non-writable.
  return Page::kPageSize -
         static_cast<int>(MemoryAllocator::GetCommitPageSize());
}

size_t MemoryChunkLayout::AllocatableMemoryInCodePage() {
  size_t memory = ObjectEndOffsetInCodePage() - ObjectStartOffsetInCodePage();
  DCHECK_LE(kMaxRegularHeapObjectSize, memory);
  return memory;
}

intptr_t MemoryChunkLayout::ObjectStartOffsetInDataPage() {
  return RoundUp(MemoryChunk::kHeaderSize, kTaggedSize);
}

size_t MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(
    AllocationSpace space) {
  if (space == CODE_SPACE) {
    return ObjectStartOffsetInCodePage();
  }
  return ObjectStartOffsetInDataPage();
}

size_t MemoryChunkLayout::AllocatableMemoryInDataPage() {
  size_t memory = MemoryChunk::kPageSize - ObjectStartOffsetInDataPage();
  DCHECK_LE(kMaxRegularHeapObjectSize, memory);
  return memory;
}

size_t MemoryChunkLayout::AllocatableMemoryInMemoryChunk(
    AllocationSpace space) {
  if (space == CODE_SPACE) {
    return AllocatableMemoryInCodePage();
  }
  return AllocatableMemoryInDataPage();
}

Heap* MemoryChunk::synchronized_heap() {
  return reinterpret_cast<Heap*>(
      base::Acquire_Load(reinterpret_cast<base::AtomicWord*>(&heap_)));
}

void MemoryChunk::InitializationMemoryFence() {
  base::SeqCst_MemoryFence();
#ifdef THREAD_SANITIZER
  // Since TSAN does not process memory fences, we use the following annotation
  // to tell TSAN that there is no data race when emitting a
  // InitializationMemoryFence. Note that the other thread still needs to
  // perform MemoryChunk::synchronized_heap().
  base::Release_Store(reinterpret_cast<base::AtomicWord*>(&heap_),
                      reinterpret_cast<base::AtomicWord>(heap_));
#endif
}

void MemoryChunk::DecrementWriteUnprotectCounterAndMaybeSetPermissions(
    PageAllocator::Permission permission) {
  DCHECK(permission == PageAllocator::kRead ||
         permission == PageAllocator::kReadExecute);
  DCHECK(IsFlagSet(MemoryChunk::IS_EXECUTABLE));
  DCHECK(owner()->identity() == CODE_SPACE ||
         owner()->identity() == CODE_LO_SPACE);
  // Decrementing the write_unprotect_counter_ and changing the page
  // protection mode has to be atomic.
  base::MutexGuard guard(page_protection_change_mutex_);
  if (write_unprotect_counter_ == 0) {
    // This is a corner case that may happen when we have a
    // CodeSpaceMemoryModificationScope open and this page was newly
    // added.
    return;
  }
  write_unprotect_counter_--;
  DCHECK_LT(write_unprotect_counter_, kMaxWriteUnprotectCounter);
  if (write_unprotect_counter_ == 0) {
    Address protect_start =
        address() + MemoryChunkLayout::ObjectStartOffsetInCodePage();
    size_t page_size = MemoryAllocator::GetCommitPageSize();
    DCHECK(IsAligned(protect_start, page_size));
    size_t protect_size = RoundUp(area_size(), page_size);
    CHECK(reservation_.SetPermissions(protect_start, protect_size, permission));
  }
}

void MemoryChunk::SetReadable() {
  DecrementWriteUnprotectCounterAndMaybeSetPermissions(PageAllocator::kRead);
}

void MemoryChunk::SetReadAndExecutable() {
  DCHECK(!FLAG_jitless);
  DecrementWriteUnprotectCounterAndMaybeSetPermissions(
      PageAllocator::kReadExecute);
}

void MemoryChunk::SetReadAndWritable() {
  DCHECK(IsFlagSet(MemoryChunk::IS_EXECUTABLE));
  DCHECK(owner()->identity() == CODE_SPACE ||
         owner()->identity() == CODE_LO_SPACE);
  // Incrementing the write_unprotect_counter_ and changing the page
  // protection mode has to be atomic.
  base::MutexGuard guard(page_protection_change_mutex_);
  write_unprotect_counter_++;
  DCHECK_LE(write_unprotect_counter_, kMaxWriteUnprotectCounter);
  if (write_unprotect_counter_ == 1) {
    Address unprotect_start =
        address() + MemoryChunkLayout::ObjectStartOffsetInCodePage();
    size_t page_size = MemoryAllocator::GetCommitPageSize();
    DCHECK(IsAligned(unprotect_start, page_size));
    size_t unprotect_size = RoundUp(area_size(), page_size);
    CHECK(reservation_.SetPermissions(unprotect_start, unprotect_size,
                                      PageAllocator::kReadWrite));
  }
}

namespace {

PageAllocator::Permission DefaultWritableCodePermissions() {
  return FLAG_jitless ? PageAllocator::kReadWrite
                      : PageAllocator::kReadWriteExecute;
}

}  // namespace

MemoryChunk* MemoryChunk::Initialize(Heap* heap, Address base, size_t size,
                                     Address area_start, Address area_end,
                                     Executability executable, Space* owner,
                                     VirtualMemory reservation) {
  MemoryChunk* chunk = FromAddress(base);

  DCHECK_EQ(base, chunk->address());

  chunk->heap_ = heap;
  chunk->size_ = size;
  chunk->header_sentinel_ = HeapObject::FromAddress(base).ptr();
  DCHECK(HasHeaderSentinel(area_start));
  chunk->area_start_ = area_start;
  chunk->area_end_ = area_end;
  chunk->flags_ = Flags(NO_FLAGS);
  chunk->set_owner(owner);
  chunk->InitializeReservedMemory();
  base::AsAtomicPointer::Release_Store(&chunk->slot_set_[OLD_TO_NEW], nullptr);
  base::AsAtomicPointer::Release_Store(&chunk->slot_set_[OLD_TO_OLD], nullptr);
  base::AsAtomicPointer::Release_Store(&chunk->typed_slot_set_[OLD_TO_NEW],
                                       nullptr);
  base::AsAtomicPointer::Release_Store(&chunk->typed_slot_set_[OLD_TO_OLD],
                                       nullptr);
  chunk->invalidated_slots_ = nullptr;
  chunk->skip_list_ = nullptr;
  chunk->progress_bar_ = 0;
  chunk->high_water_mark_ = static_cast<intptr_t>(area_start - base);
  chunk->set_concurrent_sweeping_state(kSweepingDone);
  chunk->page_protection_change_mutex_ = new base::Mutex();
  chunk->write_unprotect_counter_ = 0;
  chunk->mutex_ = new base::Mutex();
  chunk->allocated_bytes_ = chunk->area_size();
  chunk->wasted_memory_ = 0;
  chunk->young_generation_bitmap_ = nullptr;
  chunk->marking_bitmap_ = nullptr;
  chunk->local_tracker_ = nullptr;

  chunk->external_backing_store_bytes_[ExternalBackingStoreType::kArrayBuffer] =
      0;
  chunk->external_backing_store_bytes_
      [ExternalBackingStoreType::kExternalString] = 0;

  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    chunk->categories_[i] = nullptr;
  }

  chunk->AllocateMarkingBitmap();
  if (owner->identity() == RO_SPACE) {
    heap->incremental_marking()
        ->non_atomic_marking_state()
        ->bitmap(chunk)
        ->MarkAllBits();
  } else {
    heap->incremental_marking()->non_atomic_marking_state()->SetLiveBytes(chunk,
                                                                          0);
  }

  DCHECK_EQ(kFlagsOffset, OFFSET_OF(MemoryChunk, flags_));
  DCHECK_EQ(kHeapOffset, OFFSET_OF(MemoryChunk, heap_));
  DCHECK_EQ(kOwnerOffset, OFFSET_OF(MemoryChunk, owner_));

  if (executable == EXECUTABLE) {
    chunk->SetFlag(IS_EXECUTABLE);
    if (heap->write_protect_code_memory()) {
      chunk->write_unprotect_counter_ =
          heap->code_space_memory_modification_scope_depth();
    } else {
      size_t page_size = MemoryAllocator::GetCommitPageSize();
      DCHECK(IsAligned(area_start, page_size));
      size_t area_size = RoundUp(area_end - area_start, page_size);
      CHECK(reservation.SetPermissions(area_start, area_size,
                                       DefaultWritableCodePermissions()));
    }
  }

  chunk->reservation_ = std::move(reservation);

  return chunk;
}

Page* PagedSpace::InitializePage(MemoryChunk* chunk, Executability executable) {
  Page* page = static_cast<Page*>(chunk);
  DCHECK_EQ(MemoryChunkLayout::AllocatableMemoryInMemoryChunk(
                page->owner()->identity()),
            page->area_size());
  // Make sure that categories are initialized before freeing the area.
  page->ResetAllocatedBytes();
  page->SetOldGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  page->AllocateFreeListCategories();
  page->InitializeFreeListCategories();
  page->list_node().Initialize();
  page->InitializationMemoryFence();
  return page;
}

Page* SemiSpace::InitializePage(MemoryChunk* chunk, Executability executable) {
  DCHECK_EQ(executable, Executability::NOT_EXECUTABLE);
  bool in_to_space = (id() != kFromSpace);
  chunk->SetFlag(in_to_space ? MemoryChunk::TO_PAGE : MemoryChunk::FROM_PAGE);
  Page* page = static_cast<Page*>(chunk);
  page->SetYoungGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  page->AllocateLocalTracker();
  page->list_node().Initialize();
#ifdef ENABLE_MINOR_MC
  if (FLAG_minor_mc) {
    page->AllocateYoungGenerationBitmap();
    heap()
        ->minor_mark_compact_collector()
        ->non_atomic_marking_state()
        ->ClearLiveness(page);
  }
#endif  // ENABLE_MINOR_MC
  page->InitializationMemoryFence();
  return page;
}

LargePage* LargePage::Initialize(Heap* heap, MemoryChunk* chunk,
                                 Executability executable) {
  if (executable && chunk->size() > LargePage::kMaxCodePageSize) {
    STATIC_ASSERT(LargePage::kMaxCodePageSize <= TypedSlotSet::kMaxOffset);
    FATAL("Code page is too large.");
  }

  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(chunk->area_start(), chunk->area_size());

  // Initialize the sentinel value for each page boundary since the mutator
  // may initialize the object starting from its end.
  Address sentinel = chunk->address() + MemoryChunk::kHeaderSentinelOffset +
                     MemoryChunk::kPageSize;
  while (sentinel < chunk->area_end()) {
    *reinterpret_cast<intptr_t*>(sentinel) = kNullAddress;
    sentinel += MemoryChunk::kPageSize;
  }

  LargePage* page = static_cast<LargePage*>(chunk);
  page->SetFlag(MemoryChunk::LARGE_PAGE);
  page->list_node().Initialize();
  return page;
}

void Page::AllocateFreeListCategories() {
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    categories_[i] = new FreeListCategory(
        reinterpret_cast<PagedSpace*>(owner())->free_list(), this);
  }
}

void Page::InitializeFreeListCategories() {
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    categories_[i]->Initialize(static_cast<FreeListCategoryType>(i));
  }
}

void Page::ReleaseFreeListCategories() {
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    if (categories_[i] != nullptr) {
      delete categories_[i];
      categories_[i] = nullptr;
    }
  }
}

Page* Page::ConvertNewToOld(Page* old_page) {
  DCHECK(old_page);
  DCHECK(old_page->InNewSpace());
  OldSpace* old_space = old_page->heap()->old_space();
  old_page->set_owner(old_space);
  old_page->SetFlags(0, static_cast<uintptr_t>(~0));
  Page* new_page = old_space->InitializePage(old_page, NOT_EXECUTABLE);
  old_space->AddPage(new_page);
  return new_page;
}

size_t MemoryChunk::CommittedPhysicalMemory() {
  if (!base::OS::HasLazyCommits() || owner()->identity() == LO_SPACE)
    return size();
  return high_water_mark_;
}

bool MemoryChunk::InOldSpace() const {
  return owner()->identity() == OLD_SPACE;
}

bool MemoryChunk::InLargeObjectSpace() const {
  return owner()->identity() == LO_SPACE;
}

MemoryChunk* MemoryAllocator::AllocateChunk(size_t reserve_area_size,
                                            size_t commit_area_size,
                                            Executability executable,
                                            Space* owner) {
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

  // Use chunk_size for statistics and callbacks because we assume that they
  // treat reserved but not-yet committed memory regions of chunks as allocated.
  isolate_->counters()->memory_allocated()->Increment(
      static_cast<int>(chunk_size));

  LOG(isolate_,
      NewEvent("MemoryChunk", reinterpret_cast<void*>(base), chunk_size));

  // We cannot use the last chunk in the address space because we would
  // overflow when comparing top and limit if this chunk is used for a
  // linear allocation area.
  if ((base + chunk_size) == 0u) {
    CHECK(!last_chunk_.IsReserved());
    last_chunk_.TakeControl(&reservation);
    UncommitMemory(&last_chunk_);
    size_ -= chunk_size;
    if (executable == EXECUTABLE) {
      size_executable_ -= chunk_size;
    }
    CHECK(last_chunk_.IsReserved());
    return AllocateChunk(reserve_area_size, commit_area_size, executable,
                         owner);
  }

  MemoryChunk* chunk =
      MemoryChunk::Initialize(heap, base, chunk_size, area_start, area_end,
                              executable, owner, std::move(reservation));

  if (chunk->executable()) RegisterExecutableMemoryChunk(chunk);
  return chunk;
}

void MemoryChunk::SetOldGenerationPageFlags(bool is_marking) {
  if (is_marking) {
    SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::INCREMENTAL_MARKING);
  } else {
    ClearFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    ClearFlag(MemoryChunk::INCREMENTAL_MARKING);
  }
}

void MemoryChunk::SetYoungGenerationPageFlags(bool is_marking) {
  SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
  if (is_marking) {
    SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::INCREMENTAL_MARKING);
  } else {
    ClearFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    ClearFlag(MemoryChunk::INCREMENTAL_MARKING);
  }
}

void Page::ResetAllocatedBytes() { allocated_bytes_ = area_size(); }

void Page::AllocateLocalTracker() {
  DCHECK_NULL(local_tracker_);
  local_tracker_ = new LocalArrayBufferTracker(this);
}

bool Page::contains_array_buffers() {
  return local_tracker_ != nullptr && !local_tracker_->IsEmpty();
}

void Page::ResetFreeListStatistics() {
  wasted_memory_ = 0;
}

size_t Page::AvailableInFreeList() {
  size_t sum = 0;
  ForAllFreeListCategories([&sum](FreeListCategory* category) {
    sum += category->available();
  });
  return sum;
}

#ifdef DEBUG
namespace {
// Skips filler starting from the given filler until the end address.
// Returns the first address after the skipped fillers.
Address SkipFillers(HeapObject filler, Address end) {
  Address addr = filler->address();
  while (addr < end) {
    filler = HeapObject::FromAddress(addr);
    CHECK(filler->IsFiller());
    addr = filler->address() + filler->Size();
  }
  return addr;
}
}  // anonymous namespace
#endif  // DEBUG

size_t Page::ShrinkToHighWaterMark() {
  // Shrinking only makes sense outside of the CodeRange, where we don't care
  // about address space fragmentation.
  VirtualMemory* reservation = reserved_memory();
  if (!reservation->IsReserved()) return 0;

  // Shrink pages to high water mark. The water mark points either to a filler
  // or the area_end.
  HeapObject filler = HeapObject::FromAddress(HighWaterMark());
  if (filler->address() == area_end()) return 0;
  CHECK(filler->IsFiller());
  // Ensure that no objects were allocated in [filler, area_end) region.
  DCHECK_EQ(area_end(), SkipFillers(filler, area_end()));
  // Ensure that no objects will be allocated on this page.
  DCHECK_EQ(0u, AvailableInFreeList());

  size_t unused = RoundDown(static_cast<size_t>(area_end() - filler->address()),
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
        filler->address(),
        static_cast<int>(area_end() - filler->address() - unused),
        ClearRecordedSlots::kNo);
    heap()->memory_allocator()->PartialFreeMemory(
        this, address() + size() - unused, unused, area_end() - unused);
    if (filler->address() != area_end()) {
      CHECK(filler->IsFiller());
      CHECK_EQ(filler->address() + filler->Size(), area_end());
    }
  }
  return unused;
}

void Page::CreateBlackArea(Address start, Address end) {
  DCHECK(heap()->incremental_marking()->black_allocation());
  DCHECK_EQ(Page::FromAddress(start), this);
  DCHECK_NE(start, end);
  DCHECK_EQ(Page::FromAddress(end - 1), this);
  IncrementalMarking::MarkingState* marking_state =
      heap()->incremental_marking()->marking_state();
  marking_state->bitmap(this)->SetRange(AddressToMarkbitIndex(start),
                                        AddressToMarkbitIndex(end));
  marking_state->IncrementLiveBytes(this, static_cast<intptr_t>(end - start));
}

void Page::DestroyBlackArea(Address start, Address end) {
  DCHECK(heap()->incremental_marking()->black_allocation());
  DCHECK_EQ(Page::FromAddress(start), this);
  DCHECK_NE(start, end);
  DCHECK_EQ(Page::FromAddress(end - 1), this);
  IncrementalMarking::MarkingState* marking_state =
      heap()->incremental_marking()->marking_state();
  marking_state->bitmap(this)->ClearRange(AddressToMarkbitIndex(start),
                                          AddressToMarkbitIndex(end));
  marking_state->IncrementLiveBytes(this, -static_cast<intptr_t>(end - start));
}

void MemoryAllocator::PartialFreeMemory(MemoryChunk* chunk, Address start_free,
                                        size_t bytes_to_free,
                                        Address new_area_end) {
  VirtualMemory* reservation = chunk->reserved_memory();
  DCHECK(reservation->IsReserved());
  chunk->size_ -= bytes_to_free;
  chunk->area_end_ = new_area_end;
  if (chunk->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
    // Add guard page at the end.
    size_t page_size = GetCommitPageSize();
    DCHECK_EQ(0, chunk->area_end_ % static_cast<Address>(page_size));
    DCHECK_EQ(chunk->address() + chunk->size(),
              chunk->area_end() + MemoryChunkLayout::CodePageGuardSize());
    reservation->SetPermissions(chunk->area_end_, page_size,
                                PageAllocator::kNoAccess);
  }
  // On e.g. Windows, a reservation may be larger than a page and releasing
  // partially starting at |start_free| will also release the potentially
  // unused part behind the current page.
  const size_t released_bytes = reservation->Release(start_free);
  DCHECK_GE(size_, released_bytes);
  size_ -= released_bytes;
  isolate_->counters()->memory_allocated()->Decrement(
      static_cast<int>(released_bytes));
}

void MemoryAllocator::PreFreeMemory(MemoryChunk* chunk) {
  DCHECK(!chunk->IsFlagSet(MemoryChunk::PRE_FREED));
  LOG(isolate_, DeleteEvent("MemoryChunk", chunk));

  isolate_->heap()->RememberUnmappedPage(reinterpret_cast<Address>(chunk),
                                         chunk->IsEvacuationCandidate());

  VirtualMemory* reservation = chunk->reserved_memory();
  const size_t size =
      reservation->IsReserved() ? reservation->size() : chunk->size();
  DCHECK_GE(size_, static_cast<size_t>(size));
  size_ -= size;
  isolate_->counters()->memory_allocated()->Decrement(static_cast<int>(size));
  if (chunk->executable() == EXECUTABLE) {
    DCHECK_GE(size_executable_, size);
    size_executable_ -= size;
  }

  chunk->SetFlag(MemoryChunk::PRE_FREED);

  if (chunk->executable()) UnregisterExecutableMemoryChunk(chunk);
}


void MemoryAllocator::PerformFreeMemory(MemoryChunk* chunk) {
  DCHECK(chunk->IsFlagSet(MemoryChunk::PRE_FREED));
  chunk->ReleaseAllocatedMemory();

  VirtualMemory* reservation = chunk->reserved_memory();
  if (chunk->IsFlagSet(MemoryChunk::POOLED)) {
    UncommitMemory(reservation);
  } else {
    if (reservation->IsReserved()) {
      reservation->Free();
    } else {
      // Only read-only pages can have non-initialized reservation object.
      DCHECK_EQ(RO_SPACE, chunk->owner()->identity());
      FreeMemory(page_allocator(chunk->executable()), chunk->address(),
                 chunk->size());
    }
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
  return owner->InitializePage(chunk, executable);
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
  MemoryChunk::Initialize(isolate_->heap(), start, size, area_start, area_end,
                          NOT_EXECUTABLE, owner, std::move(reservation));
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


// -----------------------------------------------------------------------------
// MemoryChunk implementation

void MemoryChunk::ReleaseAllocatedMemory() {
  if (skip_list_ != nullptr) {
    delete skip_list_;
    skip_list_ = nullptr;
  }
  if (mutex_ != nullptr) {
    delete mutex_;
    mutex_ = nullptr;
  }
  if (page_protection_change_mutex_ != nullptr) {
    delete page_protection_change_mutex_;
    page_protection_change_mutex_ = nullptr;
  }
  ReleaseSlotSet<OLD_TO_NEW>();
  ReleaseSlotSet<OLD_TO_OLD>();
  ReleaseTypedSlotSet<OLD_TO_NEW>();
  ReleaseTypedSlotSet<OLD_TO_OLD>();
  ReleaseInvalidatedSlots();
  if (local_tracker_ != nullptr) ReleaseLocalTracker();
  if (young_generation_bitmap_ != nullptr) ReleaseYoungGenerationBitmap();
  if (marking_bitmap_ != nullptr) ReleaseMarkingBitmap();

  if (!IsLargePage()) {
    Page* page = static_cast<Page*>(this);
    page->ReleaseFreeListCategories();
  }
}

static SlotSet* AllocateAndInitializeSlotSet(size_t size, Address page_start) {
  size_t pages = (size + Page::kPageSize - 1) / Page::kPageSize;
  DCHECK_LT(0, pages);
  SlotSet* slot_set = new SlotSet[pages];
  for (size_t i = 0; i < pages; i++) {
    slot_set[i].SetPageStart(page_start + i * Page::kPageSize);
  }
  return slot_set;
}

template V8_EXPORT_PRIVATE SlotSet* MemoryChunk::AllocateSlotSet<OLD_TO_NEW>();
template V8_EXPORT_PRIVATE SlotSet* MemoryChunk::AllocateSlotSet<OLD_TO_OLD>();

template <RememberedSetType type>
SlotSet* MemoryChunk::AllocateSlotSet() {
  SlotSet* slot_set = AllocateAndInitializeSlotSet(size_, address());
  SlotSet* old_slot_set = base::AsAtomicPointer::Release_CompareAndSwap(
      &slot_set_[type], nullptr, slot_set);
  if (old_slot_set != nullptr) {
    delete[] slot_set;
    slot_set = old_slot_set;
  }
  DCHECK(slot_set);
  return slot_set;
}

template void MemoryChunk::ReleaseSlotSet<OLD_TO_NEW>();
template void MemoryChunk::ReleaseSlotSet<OLD_TO_OLD>();

template <RememberedSetType type>
void MemoryChunk::ReleaseSlotSet() {
  SlotSet* slot_set = slot_set_[type];
  if (slot_set) {
    slot_set_[type] = nullptr;
    delete[] slot_set;
  }
}

template TypedSlotSet* MemoryChunk::AllocateTypedSlotSet<OLD_TO_NEW>();
template TypedSlotSet* MemoryChunk::AllocateTypedSlotSet<OLD_TO_OLD>();

template <RememberedSetType type>
TypedSlotSet* MemoryChunk::AllocateTypedSlotSet() {
  TypedSlotSet* typed_slot_set = new TypedSlotSet(address());
  TypedSlotSet* old_value = base::AsAtomicPointer::Release_CompareAndSwap(
      &typed_slot_set_[type], nullptr, typed_slot_set);
  if (old_value != nullptr) {
    delete typed_slot_set;
    typed_slot_set = old_value;
  }
  DCHECK(typed_slot_set);
  return typed_slot_set;
}

template void MemoryChunk::ReleaseTypedSlotSet<OLD_TO_NEW>();
template void MemoryChunk::ReleaseTypedSlotSet<OLD_TO_OLD>();

template <RememberedSetType type>
void MemoryChunk::ReleaseTypedSlotSet() {
  TypedSlotSet* typed_slot_set = typed_slot_set_[type];
  if (typed_slot_set) {
    typed_slot_set_[type] = nullptr;
    delete typed_slot_set;
  }
}

InvalidatedSlots* MemoryChunk::AllocateInvalidatedSlots() {
  DCHECK_NULL(invalidated_slots_);
  invalidated_slots_ = new InvalidatedSlots();
  return invalidated_slots_;
}

void MemoryChunk::ReleaseInvalidatedSlots() {
  if (invalidated_slots_) {
    delete invalidated_slots_;
    invalidated_slots_ = nullptr;
  }
}

void MemoryChunk::RegisterObjectWithInvalidatedSlots(HeapObject object,
                                                     int size) {
  if (!ShouldSkipEvacuationSlotRecording()) {
    if (invalidated_slots() == nullptr) {
      AllocateInvalidatedSlots();
    }
    int old_size = (*invalidated_slots())[object];
    (*invalidated_slots())[object] = std::max(old_size, size);
  }
}

bool MemoryChunk::RegisteredObjectWithInvalidatedSlots(HeapObject object) {
  if (ShouldSkipEvacuationSlotRecording()) {
    // Invalidated slots do not matter if we are not recording slots.
    return true;
  }
  if (invalidated_slots() == nullptr) {
    return false;
  }
  return invalidated_slots()->find(object) != invalidated_slots()->end();
}

void MemoryChunk::MoveObjectWithInvalidatedSlots(HeapObject old_start,
                                                 HeapObject new_start) {
  DCHECK_LT(old_start, new_start);
  DCHECK_EQ(MemoryChunk::FromHeapObject(old_start),
            MemoryChunk::FromHeapObject(new_start));
  if (!ShouldSkipEvacuationSlotRecording() && invalidated_slots()) {
    auto it = invalidated_slots()->find(old_start);
    if (it != invalidated_slots()->end()) {
      int old_size = it->second;
      int delta = static_cast<int>(new_start->address() - old_start->address());
      invalidated_slots()->erase(it);
      (*invalidated_slots())[new_start] = old_size - delta;
    }
  }
}

void MemoryChunk::ReleaseLocalTracker() {
  DCHECK_NOT_NULL(local_tracker_);
  delete local_tracker_;
  local_tracker_ = nullptr;
}

void MemoryChunk::AllocateYoungGenerationBitmap() {
  DCHECK_NULL(young_generation_bitmap_);
  young_generation_bitmap_ = static_cast<Bitmap*>(calloc(1, Bitmap::kSize));
}

void MemoryChunk::ReleaseYoungGenerationBitmap() {
  DCHECK_NOT_NULL(young_generation_bitmap_);
  free(young_generation_bitmap_);
  young_generation_bitmap_ = nullptr;
}

void MemoryChunk::AllocateMarkingBitmap() {
  DCHECK_NULL(marking_bitmap_);
  marking_bitmap_ = static_cast<Bitmap*>(calloc(1, Bitmap::kSize));
}

void MemoryChunk::ReleaseMarkingBitmap() {
  DCHECK_NOT_NULL(marking_bitmap_);
  free(marking_bitmap_);
  marking_bitmap_ = nullptr;
}

// -----------------------------------------------------------------------------
// PagedSpace implementation

void Space::CheckOffsetsAreConsistent() const {
  static_assert(Space::kIdOffset == heap_internals::Space::kIdOffset,
                "ID offset inconsistent");
  DCHECK_EQ(Space::kIdOffset, OFFSET_OF(Space, id_));
}

void Space::AddAllocationObserver(AllocationObserver* observer) {
  allocation_observers_.push_back(observer);
  StartNextInlineAllocationStep();
}

void Space::RemoveAllocationObserver(AllocationObserver* observer) {
  auto it = std::find(allocation_observers_.begin(),
                      allocation_observers_.end(), observer);
  DCHECK(allocation_observers_.end() != it);
  allocation_observers_.erase(it);
  StartNextInlineAllocationStep();
}

void Space::PauseAllocationObservers() { allocation_observers_paused_ = true; }

void Space::ResumeAllocationObservers() {
  allocation_observers_paused_ = false;
}

void Space::AllocationStep(int bytes_since_last, Address soon_object,
                           int size) {
  if (!AllocationObserversActive()) {
    return;
  }

  DCHECK(!heap()->allocation_step_in_progress());
  heap()->set_allocation_step_in_progress(true);
  heap()->CreateFillerObjectAt(soon_object, size, ClearRecordedSlots::kNo);
  for (AllocationObserver* observer : allocation_observers_) {
    observer->AllocationStep(bytes_since_last, soon_object, size);
  }
  heap()->set_allocation_step_in_progress(false);
}

intptr_t Space::GetNextInlineAllocationStepSize() {
  intptr_t next_step = 0;
  for (AllocationObserver* observer : allocation_observers_) {
    next_step = next_step ? Min(next_step, observer->bytes_to_next_step())
                          : observer->bytes_to_next_step();
  }
  DCHECK(allocation_observers_.size() == 0 || next_step > 0);
  return next_step;
}

PagedSpace::PagedSpace(Heap* heap, AllocationSpace space,
                       Executability executable)
    : SpaceWithLinearArea(heap, space), executable_(executable) {
  area_size_ = MemoryChunkLayout::AllocatableMemoryInMemoryChunk(space);
  accounting_stats_.Clear();
}

void PagedSpace::TearDown() {
  while (!memory_chunk_list_.Empty()) {
    MemoryChunk* chunk = memory_chunk_list_.front();
    memory_chunk_list_.Remove(chunk);
    heap()->memory_allocator()->Free<MemoryAllocator::kFull>(chunk);
  }
  accounting_stats_.Clear();
}

void PagedSpace::RefillFreeList() {
  // Any PagedSpace might invoke RefillFreeList. We filter all but our old
  // generation spaces out.
  if (identity() != OLD_SPACE && identity() != CODE_SPACE &&
      identity() != MAP_SPACE && identity() != RO_SPACE) {
    return;
  }
  MarkCompactCollector* collector = heap()->mark_compact_collector();
  size_t added = 0;
  {
    Page* p = nullptr;
    while ((p = collector->sweeper()->GetSweptPageSafe(this)) != nullptr) {
      // We regularly sweep NEVER_ALLOCATE_ON_PAGE pages. We drop the freelist
      // entries here to make them unavailable for allocations.
      if (p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
        p->ForAllFreeListCategories(
            [](FreeListCategory* category) { category->Reset(); });
      }
      // Only during compaction pages can actually change ownership. This is
      // safe because there exists no other competing action on the page links
      // during compaction.
      if (is_local()) {
        DCHECK_NE(this, p->owner());
        PagedSpace* owner = reinterpret_cast<PagedSpace*>(p->owner());
        base::MutexGuard guard(owner->mutex());
        owner->RefineAllocatedBytesAfterSweeping(p);
        owner->RemovePage(p);
        added += AddPage(p);
      } else {
        base::MutexGuard guard(mutex());
        DCHECK_EQ(this, p->owner());
        RefineAllocatedBytesAfterSweeping(p);
        added += RelinkFreeListCategories(p);
      }
      added += p->wasted_memory();
      if (is_local() && (added > kCompactionMemoryWanted)) break;
    }
  }
}

void PagedSpace::MergeCompactionSpace(CompactionSpace* other) {
  base::MutexGuard guard(mutex());

  DCHECK(identity() == other->identity());
  // Unmerged fields:
  //   area_size_
  other->FreeLinearAllocationArea();

  // The linear allocation area of {other} should be destroyed now.
  DCHECK_EQ(kNullAddress, other->top());
  DCHECK_EQ(kNullAddress, other->limit());

  // Move over pages.
  for (auto it = other->begin(); it != other->end();) {
    Page* p = *(it++);
    // Relinking requires the category to be unlinked.
    other->RemovePage(p);
    AddPage(p);
    DCHECK_IMPLIES(
        !p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE),
        p->AvailableInFreeList() == p->AvailableInFreeListFromAllocatedBytes());
  }
  DCHECK_EQ(0u, other->Size());
  DCHECK_EQ(0u, other->Capacity());
}


size_t PagedSpace::CommittedPhysicalMemory() {
  if (!base::OS::HasLazyCommits()) return CommittedMemory();
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  size_t size = 0;
  for (Page* page : *this) {
    size += page->CommittedPhysicalMemory();
  }
  return size;
}

bool PagedSpace::ContainsSlow(Address addr) {
  Page* p = Page::FromAddress(addr);
  for (Page* page : *this) {
    if (page == p) return true;
  }
  return false;
}

void PagedSpace::RefineAllocatedBytesAfterSweeping(Page* page) {
  CHECK(page->SweepingDone());
  auto marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
  // The live_byte on the page was accounted in the space allocated
  // bytes counter. After sweeping allocated_bytes() contains the
  // accurate live byte count on the page.
  size_t old_counter = marking_state->live_bytes(page);
  size_t new_counter = page->allocated_bytes();
  DCHECK_GE(old_counter, new_counter);
  if (old_counter > new_counter) {
    DecreaseAllocatedBytes(old_counter - new_counter, page);
    // Give the heap a chance to adjust counters in response to the
    // more precise and smaller old generation size.
    heap()->NotifyRefinedOldGenerationSize(old_counter - new_counter);
  }
  marking_state->SetLiveBytes(page, 0);
}

Page* PagedSpace::RemovePageSafe(int size_in_bytes) {
  base::MutexGuard guard(mutex());
  // Check for pages that still contain free list entries. Bail out for smaller
  // categories.
  const int minimum_category =
      static_cast<int>(FreeList::SelectFreeListCategoryType(size_in_bytes));
  Page* page = free_list()->GetPageForCategoryType(kHuge);
  if (!page && static_cast<int>(kLarge) >= minimum_category)
    page = free_list()->GetPageForCategoryType(kLarge);
  if (!page && static_cast<int>(kMedium) >= minimum_category)
    page = free_list()->GetPageForCategoryType(kMedium);
  if (!page && static_cast<int>(kSmall) >= minimum_category)
    page = free_list()->GetPageForCategoryType(kSmall);
  if (!page && static_cast<int>(kTiny) >= minimum_category)
    page = free_list()->GetPageForCategoryType(kTiny);
  if (!page && static_cast<int>(kTiniest) >= minimum_category)
    page = free_list()->GetPageForCategoryType(kTiniest);
  if (!page) return nullptr;
  RemovePage(page);
  return page;
}

size_t PagedSpace::AddPage(Page* page) {
  CHECK(page->SweepingDone());
  page->set_owner(this);
  memory_chunk_list_.PushBack(page);
  AccountCommitted(page->size());
  IncreaseCapacity(page->area_size());
  IncreaseAllocatedBytes(page->allocated_bytes(), page);
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    IncrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
  return RelinkFreeListCategories(page);
}

void PagedSpace::RemovePage(Page* page) {
  CHECK(page->SweepingDone());
  memory_chunk_list_.Remove(page);
  UnlinkFreeListCategories(page);
  DecreaseAllocatedBytes(page->allocated_bytes(), page);
  DecreaseCapacity(page->area_size());
  AccountUncommitted(page->size());
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    DecrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
}

size_t PagedSpace::ShrinkPageToHighWaterMark(Page* page) {
  size_t unused = page->ShrinkToHighWaterMark();
  accounting_stats_.DecreaseCapacity(static_cast<intptr_t>(unused));
  AccountUncommitted(unused);
  return unused;
}

void PagedSpace::ResetFreeList() {
  for (Page* page : *this) {
    free_list_.EvictFreeListItems(page);
  }
  DCHECK(free_list_.IsEmpty());
}

void PagedSpace::ShrinkImmortalImmovablePages() {
  DCHECK(!heap()->deserialization_complete());
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  FreeLinearAllocationArea();
  ResetFreeList();
  for (Page* page : *this) {
    DCHECK(page->IsFlagSet(Page::NEVER_EVACUATE));
    ShrinkPageToHighWaterMark(page);
  }
}

bool PagedSpace::Expand() {
  // Always lock against the main space as we can only adjust capacity and
  // pages concurrently for the main paged space.
  base::MutexGuard guard(heap()->paged_space(identity())->mutex());

  const int size = AreaSize();

  if (!heap()->CanExpandOldGeneration(size)) return false;

  Page* page =
      heap()->memory_allocator()->AllocatePage(size, this, executable());
  if (page == nullptr) return false;
  // Pages created during bootstrapping may contain immortal immovable objects.
  if (!heap()->deserialization_complete()) page->MarkNeverEvacuate();
  AddPage(page);
  Free(page->area_start(), page->area_size(),
       SpaceAccountingMode::kSpaceAccounted);
  heap()->NotifyOldGenerationExpansion();
  return true;
}


int PagedSpace::CountTotalPages() {
  int count = 0;
  for (Page* page : *this) {
    count++;
    USE(page);
  }
  return count;
}


void PagedSpace::ResetFreeListStatistics() {
  for (Page* page : *this) {
    page->ResetFreeListStatistics();
  }
}

void PagedSpace::SetLinearAllocationArea(Address top, Address limit) {
  SetTopAndLimit(top, limit);
  if (top != kNullAddress && top != limit &&
      heap()->incremental_marking()->black_allocation()) {
    Page::FromAllocationAreaAddress(top)->CreateBlackArea(top, limit);
  }
}

void PagedSpace::DecreaseLimit(Address new_limit) {
  Address old_limit = limit();
  DCHECK_LE(top(), new_limit);
  DCHECK_GE(old_limit, new_limit);
  if (new_limit != old_limit) {
    SetTopAndLimit(top(), new_limit);
    Free(new_limit, old_limit - new_limit,
         SpaceAccountingMode::kSpaceAccounted);
    if (heap()->incremental_marking()->black_allocation()) {
      Page::FromAllocationAreaAddress(new_limit)->DestroyBlackArea(new_limit,
                                                                   old_limit);
    }
  }
}

Address SpaceWithLinearArea::ComputeLimit(Address start, Address end,
                                          size_t min_size) {
  DCHECK_GE(end - start, min_size);

  if (heap()->inline_allocation_disabled()) {
    // Fit the requested area exactly.
    return start + min_size;
  } else if (SupportsInlineAllocation() && AllocationObserversActive()) {
    // Generated code may allocate inline from the linear allocation area for.
    // To make sure we can observe these allocations, we use a lower limit.
    size_t step = GetNextInlineAllocationStepSize();

    // TODO(ofrobots): there is subtle difference between old space and new
    // space here. Any way to avoid it? `step - 1` makes more sense as we would
    // like to sample the object that straddles the `start + step` boundary.
    // Rounding down further would introduce a small statistical error in
    // sampling. However, presently PagedSpace requires limit to be aligned.
    size_t rounded_step;
    if (identity() == NEW_SPACE) {
      DCHECK_GE(step, 1);
      rounded_step = step - 1;
    } else {
      rounded_step = RoundSizeDownToObjectAlignment(static_cast<int>(step));
    }
    return Min(static_cast<Address>(start + min_size + rounded_step), end);
  } else {
    // The entire node can be used as the linear allocation area.
    return end;
  }
}

void PagedSpace::MarkLinearAllocationAreaBlack() {
  DCHECK(heap()->incremental_marking()->black_allocation());
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->CreateBlackArea(current_top, current_limit);
  }
}

void PagedSpace::UnmarkLinearAllocationArea() {
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->DestroyBlackArea(current_top, current_limit);
  }
}

void PagedSpace::FreeLinearAllocationArea() {
  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.
  Address current_top = top();
  Address current_limit = limit();
  if (current_top == kNullAddress) {
    DCHECK_EQ(kNullAddress, current_limit);
    return;
  }

  if (heap()->incremental_marking()->black_allocation()) {
    Page* page = Page::FromAllocationAreaAddress(current_top);

    // Clear the bits in the unused black area.
    if (current_top != current_limit) {
      IncrementalMarking::MarkingState* marking_state =
          heap()->incremental_marking()->marking_state();
      marking_state->bitmap(page)->ClearRange(
          page->AddressToMarkbitIndex(current_top),
          page->AddressToMarkbitIndex(current_limit));
      marking_state->IncrementLiveBytes(
          page, -static_cast<int>(current_limit - current_top));
    }
  }

  InlineAllocationStep(current_top, kNullAddress, kNullAddress, 0);
  SetTopAndLimit(kNullAddress, kNullAddress);
  DCHECK_GE(current_limit, current_top);

  // The code page of the linear allocation area needs to be unprotected
  // because we are going to write a filler into that memory area below.
  if (identity() == CODE_SPACE) {
    heap()->UnprotectAndRegisterMemoryChunk(
        MemoryChunk::FromAddress(current_top));
  }
  Free(current_top, current_limit - current_top,
       SpaceAccountingMode::kSpaceAccounted);
}

void PagedSpace::ReleasePage(Page* page) {
  DCHECK_EQ(
      0, heap()->incremental_marking()->non_atomic_marking_state()->live_bytes(
             page));
  DCHECK_EQ(page->owner(), this);

  free_list_.EvictFreeListItems(page);
  DCHECK(!free_list_.ContainsPageFreeListItems(page));

  if (Page::FromAllocationAreaAddress(allocation_info_.top()) == page) {
    DCHECK(!top_on_previous_step_);
    allocation_info_.Reset(kNullAddress, kNullAddress);
  }

  AccountUncommitted(page->size());
  accounting_stats_.DecreaseCapacity(page->area_size());
  heap()->memory_allocator()->Free<MemoryAllocator::kPreFreeAndQueue>(page);
}

void PagedSpace::SetReadable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    CHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadable();
  }
}

void PagedSpace::SetReadAndExecutable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    CHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadAndExecutable();
  }
}

void PagedSpace::SetReadAndWritable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    CHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadAndWritable();
  }
}

std::unique_ptr<ObjectIterator> PagedSpace::GetObjectIterator() {
  return std::unique_ptr<ObjectIterator>(new HeapObjectIterator(this));
}

bool PagedSpace::RefillLinearAllocationAreaFromFreeList(size_t size_in_bytes) {
  DCHECK(IsAligned(size_in_bytes, kTaggedSize));
  DCHECK_LE(top(), limit());
#ifdef DEBUG
  if (top() != limit()) {
    DCHECK_EQ(Page::FromAddress(top()), Page::FromAddress(limit() - 1));
  }
#endif
  // Don't free list allocate if there is linear space available.
  DCHECK_LT(static_cast<size_t>(limit() - top()), size_in_bytes);

  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.  This also puts it back in the free list
  // if it is big enough.
  FreeLinearAllocationArea();

  if (!is_local()) {
    heap()->StartIncrementalMarkingIfAllocationLimitIsReached(
        heap()->GCFlagsForIncrementalMarking(),
        kGCCallbackScheduleIdleGarbageCollection);
  }

  size_t new_node_size = 0;
  FreeSpace new_node = free_list_.Allocate(size_in_bytes, &new_node_size);
  if (new_node.is_null()) return false;

  DCHECK_GE(new_node_size, size_in_bytes);

  // The old-space-step might have finished sweeping and restarted marking.
  // Verify that it did not turn the page of the new node into an evacuation
  // candidate.
  DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(new_node));

  // Memory in the linear allocation area is counted as allocated.  We may free
  // a little of this again immediately - see below.
  Page* page = Page::FromHeapObject(new_node);
  IncreaseAllocatedBytes(new_node_size, page);

  Address start = new_node->address();
  Address end = new_node->address() + new_node_size;
  Address limit = ComputeLimit(start, end, size_in_bytes);
  DCHECK_LE(limit, end);
  DCHECK_LE(size_in_bytes, limit - start);
  if (limit != end) {
    if (identity() == CODE_SPACE) {
      heap()->UnprotectAndRegisterMemoryChunk(page);
    }
    Free(limit, end - limit, SpaceAccountingMode::kSpaceAccounted);
  }
  SetLinearAllocationArea(start, limit);

  return true;
}

#ifdef DEBUG
void PagedSpace::Print() {}
#endif

#ifdef VERIFY_HEAP
void PagedSpace::Verify(Isolate* isolate, ObjectVisitor* visitor) {
  bool allocation_pointer_found_in_space =
      (allocation_info_.top() == allocation_info_.limit());
  size_t external_space_bytes[kNumTypes];
  size_t external_page_bytes[kNumTypes];

  for (int i = 0; i < kNumTypes; i++) {
    external_space_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  for (Page* page : *this) {
    CHECK(page->owner() == this);

    for (int i = 0; i < kNumTypes; i++) {
      external_page_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
    }

    if (page == Page::FromAllocationAreaAddress(allocation_info_.top())) {
      allocation_pointer_found_in_space = true;
    }
    CHECK(page->SweepingDone());
    HeapObjectIterator it(page);
    Address end_of_previous_object = page->area_start();
    Address top = page->area_end();

    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      CHECK(end_of_previous_object <= object->address());

      // The first word should be a map, and we expect all map pointers to
      // be in map space.
      Map map = object->map();
      CHECK(map->IsMap());
      CHECK(heap()->map_space()->Contains(map) ||
            heap()->read_only_space()->Contains(map));

      // Perform space-specific object verification.
      VerifyObject(object);

      // The object itself should look OK.
      object->ObjectVerify(isolate);

      if (!FLAG_verify_heap_skip_remembered_set) {
        heap()->VerifyRememberedSetFor(object);
      }

      // All the interior pointers should be contained in the heap.
      int size = object->Size();
      object->IterateBody(map, size, visitor);
      CHECK(object->address() + size <= top);
      end_of_previous_object = object->address() + size;

      if (object->IsExternalString()) {
        ExternalString external_string = ExternalString::cast(object);
        size_t size = external_string->ExternalPayloadSize();
        external_page_bytes[ExternalBackingStoreType::kExternalString] += size;
      } else if (object->IsJSArrayBuffer()) {
        JSArrayBuffer array_buffer = JSArrayBuffer::cast(object);
        if (ArrayBufferTracker::IsTracked(array_buffer)) {
          size_t size = array_buffer->byte_length();
          external_page_bytes[ExternalBackingStoreType::kArrayBuffer] += size;
        }
      }
    }
    for (int i = 0; i < kNumTypes; i++) {
      ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
      CHECK_EQ(external_page_bytes[t], page->ExternalBackingStoreBytes(t));
      external_space_bytes[t] += external_page_bytes[t];
    }
  }
  for (int i = 0; i < kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    CHECK_EQ(external_space_bytes[t], ExternalBackingStoreBytes(t));
  }
  CHECK(allocation_pointer_found_in_space);
#ifdef DEBUG
  VerifyCountersAfterSweeping();
#endif
}

void PagedSpace::VerifyLiveBytes() {
  IncrementalMarking::MarkingState* marking_state =
      heap()->incremental_marking()->marking_state();
  for (Page* page : *this) {
    CHECK(page->SweepingDone());
    HeapObjectIterator it(page);
    int black_size = 0;
    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      // All the interior pointers should be contained in the heap.
      if (marking_state->IsBlack(object)) {
        black_size += object->Size();
      }
    }
    CHECK_LE(black_size, marking_state->live_bytes(page));
  }
}
#endif  // VERIFY_HEAP

#ifdef DEBUG
void PagedSpace::VerifyCountersAfterSweeping() {
  size_t total_capacity = 0;
  size_t total_allocated = 0;
  for (Page* page : *this) {
    DCHECK(page->SweepingDone());
    total_capacity += page->area_size();
    HeapObjectIterator it(page);
    size_t real_allocated = 0;
    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      if (!object->IsFiller()) {
        real_allocated += object->Size();
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

void PagedSpace::VerifyCountersBeforeConcurrentSweeping() {
  // We need to refine the counters on pages that are already swept and have
  // not been moved over to the actual space. Otherwise, the AccountingStats
  // are just an over approximation.
  RefillFreeList();

  size_t total_capacity = 0;
  size_t total_allocated = 0;
  auto marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
  for (Page* page : *this) {
    size_t page_allocated =
        page->SweepingDone()
            ? page->allocated_bytes()
            : static_cast<size_t>(marking_state->live_bytes(page));
    total_capacity += page->area_size();
    total_allocated += page_allocated;
    DCHECK_EQ(page_allocated, accounting_stats_.AllocatedOnPage(page));
  }
  DCHECK_EQ(total_capacity, accounting_stats_.Capacity());
  DCHECK_EQ(total_allocated, accounting_stats_.Size());
}
#endif

// -----------------------------------------------------------------------------
// NewSpace implementation

NewSpace::NewSpace(Heap* heap, v8::PageAllocator* page_allocator,
                   size_t initial_semispace_capacity,
                   size_t max_semispace_capacity)
    : SpaceWithLinearArea(heap, NEW_SPACE),
      to_space_(heap, kToSpace),
      from_space_(heap, kFromSpace) {
  DCHECK(initial_semispace_capacity <= max_semispace_capacity);
  DCHECK(
      base::bits::IsPowerOfTwo(static_cast<uint32_t>(max_semispace_capacity)));

  to_space_.SetUp(initial_semispace_capacity, max_semispace_capacity);
  from_space_.SetUp(initial_semispace_capacity, max_semispace_capacity);
  if (!to_space_.Commit()) {
    V8::FatalProcessOutOfMemory(heap->isolate(), "New space setup");
  }
  DCHECK(!from_space_.is_committed());  // No need to use memory yet.
  ResetLinearAllocationArea();
}

void NewSpace::TearDown() {
  allocation_info_.Reset(kNullAddress, kNullAddress);

  to_space_.TearDown();
  from_space_.TearDown();
}

void NewSpace::Flip() { SemiSpace::Swap(&from_space_, &to_space_); }


void NewSpace::Grow() {
  // Double the semispace size but only up to maximum capacity.
  DCHECK(TotalCapacity() < MaximumCapacity());
  size_t new_capacity =
      Min(MaximumCapacity(),
          static_cast<size_t>(FLAG_semi_space_growth_factor) * TotalCapacity());
  if (to_space_.GrowTo(new_capacity)) {
    // Only grow from space if we managed to grow to-space.
    if (!from_space_.GrowTo(new_capacity)) {
      // If we managed to grow to-space but couldn't grow from-space,
      // attempt to shrink to-space.
      if (!to_space_.ShrinkTo(from_space_.current_capacity())) {
        // We are in an inconsistent state because we could not
        // commit/uncommit memory from new space.
        FATAL("inconsistent state");
      }
    }
  }
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


void NewSpace::Shrink() {
  size_t new_capacity = Max(InitialTotalCapacity(), 2 * Size());
  size_t rounded_new_capacity = ::RoundUp(new_capacity, Page::kPageSize);
  if (rounded_new_capacity < TotalCapacity() &&
      to_space_.ShrinkTo(rounded_new_capacity)) {
    // Only shrink from-space if we managed to shrink to-space.
    from_space_.Reset();
    if (!from_space_.ShrinkTo(rounded_new_capacity)) {
      // If we managed to shrink to-space but couldn't shrink from
      // space, attempt to grow to-space again.
      if (!to_space_.GrowTo(from_space_.current_capacity())) {
        // We are in an inconsistent state because we could not
        // commit/uncommit memory from new space.
        FATAL("inconsistent state");
      }
    }
  }
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}

bool NewSpace::Rebalance() {
  // Order here is important to make use of the page pool.
  return to_space_.EnsureCurrentCapacity() &&
         from_space_.EnsureCurrentCapacity();
}

bool SemiSpace::EnsureCurrentCapacity() {
  if (is_committed()) {
    const int expected_pages =
        static_cast<int>(current_capacity_ / Page::kPageSize);
    MemoryChunk* current_page = first_page();
    int actual_pages = 0;

    // First iterate through the pages list until expected pages if so many
    // pages exist.
    while (current_page != nullptr && actual_pages < expected_pages) {
      actual_pages++;
      current_page = current_page->list_node().next();
    }

    // Free all overallocated pages which are behind current_page.
    while (current_page) {
      MemoryChunk* next_current = current_page->list_node().next();
      memory_chunk_list_.Remove(current_page);
      // Clear new space flags to avoid this page being treated as a new
      // space page that is potentially being swept.
      current_page->SetFlags(0, Page::kIsInYoungGenerationMask);
      heap()->memory_allocator()->Free<MemoryAllocator::kPooledAndQueue>(
          current_page);
      current_page = next_current;
    }

    // Add more pages if we have less than expected_pages.
    IncrementalMarking::NonAtomicMarkingState* marking_state =
        heap()->incremental_marking()->non_atomic_marking_state();
    while (actual_pages < expected_pages) {
      actual_pages++;
      current_page =
          heap()->memory_allocator()->AllocatePage<MemoryAllocator::kPooled>(
              MemoryChunkLayout::AllocatableMemoryInDataPage(), this,
              NOT_EXECUTABLE);
      if (current_page == nullptr) return false;
      DCHECK_NOT_NULL(current_page);
      memory_chunk_list_.PushBack(current_page);
      marking_state->ClearLiveness(current_page);
      current_page->SetFlags(first_page()->GetFlags(),
                             static_cast<uintptr_t>(Page::kCopyAllFlags));
      heap()->CreateFillerObjectAt(current_page->area_start(),
                                   static_cast<int>(current_page->area_size()),
                                   ClearRecordedSlots::kNo);
    }
  }
  return true;
}

LinearAllocationArea LocalAllocationBuffer::Close() {
  if (IsValid()) {
    heap_->CreateFillerObjectAt(
        allocation_info_.top(),
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()),
        ClearRecordedSlots::kNo);
    const LinearAllocationArea old_info = allocation_info_;
    allocation_info_ = LinearAllocationArea(kNullAddress, kNullAddress);
    return old_info;
  }
  return LinearAllocationArea(kNullAddress, kNullAddress);
}

LocalAllocationBuffer::LocalAllocationBuffer(
    Heap* heap, LinearAllocationArea allocation_info) V8_NOEXCEPT
    : heap_(heap),
      allocation_info_(allocation_info) {
  if (IsValid()) {
    heap_->CreateFillerObjectAt(
        allocation_info_.top(),
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()),
        ClearRecordedSlots::kNo);
  }
}

LocalAllocationBuffer::LocalAllocationBuffer(const LocalAllocationBuffer& other)
    V8_NOEXCEPT {
  *this = other;
}

LocalAllocationBuffer& LocalAllocationBuffer::operator=(
    const LocalAllocationBuffer& other) V8_NOEXCEPT {
  Close();
  heap_ = other.heap_;
  allocation_info_ = other.allocation_info_;

  // This is needed since we (a) cannot yet use move-semantics, and (b) want
  // to make the use of the class easy by it as value and (c) implicitly call
  // {Close} upon copy.
  const_cast<LocalAllocationBuffer&>(other).allocation_info_.Reset(
      kNullAddress, kNullAddress);
  return *this;
}

void NewSpace::UpdateLinearAllocationArea() {
  // Make sure there is no unaccounted allocations.
  DCHECK(!AllocationObserversActive() || top_on_previous_step_ == top());

  Address new_top = to_space_.page_low();
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  allocation_info_.Reset(new_top, to_space_.page_high());
  // The order of the following two stores is important.
  // See the corresponding loads in ConcurrentMarking::Run.
  original_limit_.store(limit(), std::memory_order_relaxed);
  original_top_.store(top(), std::memory_order_release);
  StartNextInlineAllocationStep();
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}

void NewSpace::ResetLinearAllocationArea() {
  // Do a step to account for memory allocated so far before resetting.
  InlineAllocationStep(top(), top(), kNullAddress, 0);
  to_space_.Reset();
  UpdateLinearAllocationArea();
  // Clear all mark-bits in the to-space.
  IncrementalMarking::NonAtomicMarkingState* marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
  for (Page* p : to_space_) {
    marking_state->ClearLiveness(p);
    // Concurrent marking may have local live bytes for this page.
    heap()->concurrent_marking()->ClearMemoryChunkData(p);
  }
}

void NewSpace::UpdateInlineAllocationLimit(size_t min_size) {
  Address new_limit = ComputeLimit(top(), to_space_.page_high(), min_size);
  allocation_info_.set_limit(new_limit);
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}

void PagedSpace::UpdateInlineAllocationLimit(size_t min_size) {
  Address new_limit = ComputeLimit(top(), limit(), min_size);
  DCHECK_LE(new_limit, limit());
  DecreaseLimit(new_limit);
}

bool NewSpace::AddFreshPage() {
  Address top = allocation_info_.top();
  DCHECK(!OldSpace::IsAtPageStart(top));

  // Do a step to account for memory allocated on previous page.
  InlineAllocationStep(top, top, kNullAddress, 0);

  if (!to_space_.AdvancePage()) {
    // No more pages left to advance.
    return false;
  }

  // Clear remainder of current page.
  Address limit = Page::FromAllocationAreaAddress(top)->area_end();
  int remaining_in_page = static_cast<int>(limit - top);
  heap()->CreateFillerObjectAt(top, remaining_in_page, ClearRecordedSlots::kNo);
  UpdateLinearAllocationArea();

  return true;
}


bool NewSpace::AddFreshPageSynchronized() {
  base::MutexGuard guard(&mutex_);
  return AddFreshPage();
}


bool NewSpace::EnsureAllocation(int size_in_bytes,
                                AllocationAlignment alignment) {
  Address old_top = allocation_info_.top();
  Address high = to_space_.page_high();
  int filler_size = Heap::GetFillToAlign(old_top, alignment);
  int aligned_size_in_bytes = size_in_bytes + filler_size;

  if (old_top + aligned_size_in_bytes > high) {
    // Not enough room in the page, try to allocate a new one.
    if (!AddFreshPage()) {
      return false;
    }

    old_top = allocation_info_.top();
    high = to_space_.page_high();
    filler_size = Heap::GetFillToAlign(old_top, alignment);
  }

  DCHECK(old_top + aligned_size_in_bytes <= high);

  if (allocation_info_.limit() < high) {
    // Either the limit has been lowered because linear allocation was disabled
    // or because incremental marking wants to get a chance to do a step,
    // or because idle scavenge job wants to get a chance to post a task.
    // Set the new limit accordingly.
    Address new_top = old_top + aligned_size_in_bytes;
    Address soon_object = old_top + filler_size;
    InlineAllocationStep(new_top, new_top, soon_object, size_in_bytes);
    UpdateInlineAllocationLimit(aligned_size_in_bytes);
  }
  return true;
}

size_t LargeObjectSpace::Available() {
  // We return zero here since we cannot take advantage of already allocated
  // large object memory.
  return 0;
}

void SpaceWithLinearArea::StartNextInlineAllocationStep() {
  if (heap()->allocation_step_in_progress()) {
    // If we are mid-way through an existing step, don't start a new one.
    return;
  }

  if (AllocationObserversActive()) {
    top_on_previous_step_ = top();
    UpdateInlineAllocationLimit(0);
  } else {
    DCHECK_EQ(kNullAddress, top_on_previous_step_);
  }
}

void SpaceWithLinearArea::AddAllocationObserver(AllocationObserver* observer) {
  InlineAllocationStep(top(), top(), kNullAddress, 0);
  Space::AddAllocationObserver(observer);
  DCHECK_IMPLIES(top_on_previous_step_, AllocationObserversActive());
}

void SpaceWithLinearArea::RemoveAllocationObserver(
    AllocationObserver* observer) {
  Address top_for_next_step =
      allocation_observers_.size() == 1 ? kNullAddress : top();
  InlineAllocationStep(top(), top_for_next_step, kNullAddress, 0);
  Space::RemoveAllocationObserver(observer);
  DCHECK_IMPLIES(top_on_previous_step_, AllocationObserversActive());
}

void SpaceWithLinearArea::PauseAllocationObservers() {
  // Do a step to account for memory allocated so far.
  InlineAllocationStep(top(), kNullAddress, kNullAddress, 0);
  Space::PauseAllocationObservers();
  DCHECK_EQ(kNullAddress, top_on_previous_step_);
  UpdateInlineAllocationLimit(0);
}

void SpaceWithLinearArea::ResumeAllocationObservers() {
  DCHECK_EQ(kNullAddress, top_on_previous_step_);
  Space::ResumeAllocationObservers();
  StartNextInlineAllocationStep();
}

void SpaceWithLinearArea::InlineAllocationStep(Address top,
                                               Address top_for_next_step,
                                               Address soon_object,
                                               size_t size) {
  if (heap()->allocation_step_in_progress()) {
    // Avoid starting a new step if we are mid-way through an existing one.
    return;
  }

  if (top_on_previous_step_) {
    if (top < top_on_previous_step_) {
      // Generated code decreased the top pointer to do folded allocations.
      DCHECK_NE(top, kNullAddress);
      DCHECK_EQ(Page::FromAllocationAreaAddress(top),
                Page::FromAllocationAreaAddress(top_on_previous_step_));
      top_on_previous_step_ = top;
    }
    int bytes_allocated = static_cast<int>(top - top_on_previous_step_);
    AllocationStep(bytes_allocated, soon_object, static_cast<int>(size));
    top_on_previous_step_ = top_for_next_step;
  }
}

std::unique_ptr<ObjectIterator> NewSpace::GetObjectIterator() {
  return std::unique_ptr<ObjectIterator>(new SemiSpaceIterator(this));
}

#ifdef VERIFY_HEAP
// We do not use the SemiSpaceIterator because verification doesn't assume
// that it works (it depends on the invariants we are checking).
void NewSpace::Verify(Isolate* isolate) {
  // The allocation pointer should be in the space or at the very end.
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  // There should be objects packed in from the low address up to the
  // allocation pointer.
  Address current = to_space_.first_page()->area_start();
  CHECK_EQ(current, to_space_.space_start());

  size_t external_space_bytes[kNumTypes];
  for (int i = 0; i < kNumTypes; i++) {
    external_space_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  while (current != top()) {
    if (!Page::IsAlignedToPageSize(current)) {
      // The allocation pointer should not be in the middle of an object.
      CHECK(!Page::FromAllocationAreaAddress(current)->ContainsLimit(top()) ||
            current < top());

      HeapObject object = HeapObject::FromAddress(current);

      // The first word should be a map, and we expect all map pointers to
      // be in map space or read-only space.
      Map map = object->map();
      CHECK(map->IsMap());
      CHECK(heap()->map_space()->Contains(map) ||
            heap()->read_only_space()->Contains(map));

      // The object should not be code or a map.
      CHECK(!object->IsMap());
      CHECK(!object->IsAbstractCode());

      // The object itself should look OK.
      object->ObjectVerify(isolate);

      // All the interior pointers should be contained in the heap.
      VerifyPointersVisitor visitor(heap());
      int size = object->Size();
      object->IterateBody(map, size, &visitor);

      if (object->IsExternalString()) {
        ExternalString external_string = ExternalString::cast(object);
        size_t size = external_string->ExternalPayloadSize();
        external_space_bytes[ExternalBackingStoreType::kExternalString] += size;
      } else if (object->IsJSArrayBuffer()) {
        JSArrayBuffer array_buffer = JSArrayBuffer::cast(object);
        if (ArrayBufferTracker::IsTracked(array_buffer)) {
          size_t size = array_buffer->byte_length();
          external_space_bytes[ExternalBackingStoreType::kArrayBuffer] += size;
        }
      }

      current += size;
    } else {
      // At end of page, switch to next page.
      Page* page = Page::FromAllocationAreaAddress(current)->next_page();
      current = page->area_start();
    }
  }

  for (int i = 0; i < kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    CHECK_EQ(external_space_bytes[t], ExternalBackingStoreBytes(t));
  }

  // Check semi-spaces.
  CHECK_EQ(from_space_.id(), kFromSpace);
  CHECK_EQ(to_space_.id(), kToSpace);
  from_space_.Verify();
  to_space_.Verify();
}
#endif

// -----------------------------------------------------------------------------
// SemiSpace implementation

void SemiSpace::SetUp(size_t initial_capacity, size_t maximum_capacity) {
  DCHECK_GE(maximum_capacity, static_cast<size_t>(Page::kPageSize));
  minimum_capacity_ = RoundDown(initial_capacity, Page::kPageSize);
  current_capacity_ = minimum_capacity_;
  maximum_capacity_ = RoundDown(maximum_capacity, Page::kPageSize);
  committed_ = false;
}


void SemiSpace::TearDown() {
  // Properly uncommit memory to keep the allocator counters in sync.
  if (is_committed()) {
    Uncommit();
  }
  current_capacity_ = maximum_capacity_ = 0;
}


bool SemiSpace::Commit() {
  DCHECK(!is_committed());
  const int num_pages = static_cast<int>(current_capacity_ / Page::kPageSize);
  for (int pages_added = 0; pages_added < num_pages; pages_added++) {
    Page* new_page =
        heap()->memory_allocator()->AllocatePage<MemoryAllocator::kPooled>(
            MemoryChunkLayout::AllocatableMemoryInDataPage(), this,
            NOT_EXECUTABLE);
    if (new_page == nullptr) {
      if (pages_added) RewindPages(pages_added);
      return false;
    }
    memory_chunk_list_.PushBack(new_page);
  }
  Reset();
  AccountCommitted(current_capacity_);
  if (age_mark_ == kNullAddress) {
    age_mark_ = first_page()->area_start();
  }
  committed_ = true;
  return true;
}


bool SemiSpace::Uncommit() {
  DCHECK(is_committed());
  while (!memory_chunk_list_.Empty()) {
    MemoryChunk* chunk = memory_chunk_list_.front();
    memory_chunk_list_.Remove(chunk);
    heap()->memory_allocator()->Free<MemoryAllocator::kPooledAndQueue>(chunk);
  }
  current_page_ = nullptr;
  AccountUncommitted(current_capacity_);
  committed_ = false;
  heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
  return true;
}


size_t SemiSpace::CommittedPhysicalMemory() {
  if (!is_committed()) return 0;
  size_t size = 0;
  for (Page* p : *this) {
    size += p->CommittedPhysicalMemory();
  }
  return size;
}

bool SemiSpace::GrowTo(size_t new_capacity) {
  if (!is_committed()) {
    if (!Commit()) return false;
  }
  DCHECK_EQ(new_capacity & kPageAlignmentMask, 0u);
  DCHECK_LE(new_capacity, maximum_capacity_);
  DCHECK_GT(new_capacity, current_capacity_);
  const size_t delta = new_capacity - current_capacity_;
  DCHECK(IsAligned(delta, AllocatePageSize()));
  const int delta_pages = static_cast<int>(delta / Page::kPageSize);
  DCHECK(last_page());
  IncrementalMarking::NonAtomicMarkingState* marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
  for (int pages_added = 0; pages_added < delta_pages; pages_added++) {
    Page* new_page =
        heap()->memory_allocator()->AllocatePage<MemoryAllocator::kPooled>(
            MemoryChunkLayout::AllocatableMemoryInDataPage(), this,
            NOT_EXECUTABLE);
    if (new_page == nullptr) {
      if (pages_added) RewindPages(pages_added);
      return false;
    }
    memory_chunk_list_.PushBack(new_page);
    marking_state->ClearLiveness(new_page);
    // Duplicate the flags that was set on the old page.
    new_page->SetFlags(last_page()->GetFlags(), Page::kCopyOnFlipFlagsMask);
  }
  AccountCommitted(delta);
  current_capacity_ = new_capacity;
  return true;
}

void SemiSpace::RewindPages(int num_pages) {
  DCHECK_GT(num_pages, 0);
  DCHECK(last_page());
  while (num_pages > 0) {
    MemoryChunk* last = last_page();
    memory_chunk_list_.Remove(last);
    heap()->memory_allocator()->Free<MemoryAllocator::kPooledAndQueue>(last);
    num_pages--;
  }
}

bool SemiSpace::ShrinkTo(size_t new_capacity) {
  DCHECK_EQ(new_capacity & kPageAlignmentMask, 0u);
  DCHECK_GE(new_capacity, minimum_capacity_);
  DCHECK_LT(new_capacity, current_capacity_);
  if (is_committed()) {
    const size_t delta = current_capacity_ - new_capacity;
    DCHECK(IsAligned(delta, Page::kPageSize));
    int delta_pages = static_cast<int>(delta / Page::kPageSize);
    RewindPages(delta_pages);
    AccountUncommitted(delta);
    heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
  }
  current_capacity_ = new_capacity;
  return true;
}

void SemiSpace::FixPagesFlags(intptr_t flags, intptr_t mask) {
  for (Page* page : *this) {
    page->set_owner(this);
    page->SetFlags(flags, mask);
    if (id_ == kToSpace) {
      page->ClearFlag(MemoryChunk::FROM_PAGE);
      page->SetFlag(MemoryChunk::TO_PAGE);
      page->ClearFlag(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK);
      heap()->incremental_marking()->non_atomic_marking_state()->SetLiveBytes(
          page, 0);
    } else {
      page->SetFlag(MemoryChunk::FROM_PAGE);
      page->ClearFlag(MemoryChunk::TO_PAGE);
    }
    DCHECK(page->InYoungGeneration());
  }
}


void SemiSpace::Reset() {
  DCHECK(first_page());
  DCHECK(last_page());
  current_page_ = first_page();
  pages_used_ = 0;
}

void SemiSpace::RemovePage(Page* page) {
  if (current_page_ == page) {
    if (page->prev_page()) {
      current_page_ = page->prev_page();
    }
  }
  memory_chunk_list_.Remove(page);
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    DecrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
}

void SemiSpace::PrependPage(Page* page) {
  page->SetFlags(current_page()->GetFlags(),
                 static_cast<uintptr_t>(Page::kCopyAllFlags));
  page->set_owner(this);
  memory_chunk_list_.PushFront(page);
  pages_used_++;
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    IncrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
}

void SemiSpace::Swap(SemiSpace* from, SemiSpace* to) {
  // We won't be swapping semispaces without data in them.
  DCHECK(from->first_page());
  DCHECK(to->first_page());

  intptr_t saved_to_space_flags = to->current_page()->GetFlags();

  // We swap all properties but id_.
  std::swap(from->current_capacity_, to->current_capacity_);
  std::swap(from->maximum_capacity_, to->maximum_capacity_);
  std::swap(from->minimum_capacity_, to->minimum_capacity_);
  std::swap(from->age_mark_, to->age_mark_);
  std::swap(from->committed_, to->committed_);
  std::swap(from->memory_chunk_list_, to->memory_chunk_list_);
  std::swap(from->current_page_, to->current_page_);
  std::swap(from->external_backing_store_bytes_,
            to->external_backing_store_bytes_);

  to->FixPagesFlags(saved_to_space_flags, Page::kCopyOnFlipFlagsMask);
  from->FixPagesFlags(0, 0);
}

void SemiSpace::set_age_mark(Address mark) {
  DCHECK_EQ(Page::FromAllocationAreaAddress(mark)->owner(), this);
  age_mark_ = mark;
  // Mark all pages up to the one containing mark.
  for (Page* p : PageRange(space_start(), mark)) {
    p->SetFlag(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK);
  }
}

std::unique_ptr<ObjectIterator> SemiSpace::GetObjectIterator() {
  // Use the NewSpace::NewObjectIterator to iterate the ToSpace.
  UNREACHABLE();
}

#ifdef DEBUG
void SemiSpace::Print() {}
#endif

#ifdef VERIFY_HEAP
void SemiSpace::Verify() {
  bool is_from_space = (id_ == kFromSpace);
  size_t external_backing_store_bytes[kNumTypes];

  for (int i = 0; i < kNumTypes; i++) {
    external_backing_store_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  for (Page* page : *this) {
    CHECK_EQ(page->owner(), this);
    CHECK(page->InNewSpace());
    CHECK(page->IsFlagSet(is_from_space ? MemoryChunk::FROM_PAGE
                                        : MemoryChunk::TO_PAGE));
    CHECK(!page->IsFlagSet(is_from_space ? MemoryChunk::TO_PAGE
                                         : MemoryChunk::FROM_PAGE));
    CHECK(page->IsFlagSet(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING));
    if (!is_from_space) {
      // The pointers-from-here-are-interesting flag isn't updated dynamically
      // on from-space pages, so it might be out of sync with the marking state.
      if (page->heap()->incremental_marking()->IsMarking()) {
        CHECK(page->IsFlagSet(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING));
      } else {
        CHECK(
            !page->IsFlagSet(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING));
      }
    }
    for (int i = 0; i < kNumTypes; i++) {
      ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
      external_backing_store_bytes[t] += page->ExternalBackingStoreBytes(t);
    }

    CHECK_IMPLIES(page->list_node().prev(),
                  page->list_node().prev()->list_node().next() == page);
  }
  for (int i = 0; i < kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    CHECK_EQ(external_backing_store_bytes[t], ExternalBackingStoreBytes(t));
  }
}
#endif

#ifdef DEBUG
void SemiSpace::AssertValidRange(Address start, Address end) {
  // Addresses belong to same semi-space
  Page* page = Page::FromAllocationAreaAddress(start);
  Page* end_page = Page::FromAllocationAreaAddress(end);
  SemiSpace* space = reinterpret_cast<SemiSpace*>(page->owner());
  DCHECK_EQ(space, end_page->owner());
  // Start address is before end address, either on same page,
  // or end address is on a later page in the linked list of
  // semi-space pages.
  if (page == end_page) {
    DCHECK_LE(start, end);
  } else {
    while (page != end_page) {
      page = page->next_page();
    }
    DCHECK(page);
  }
}
#endif


// -----------------------------------------------------------------------------
// SemiSpaceIterator implementation.

SemiSpaceIterator::SemiSpaceIterator(NewSpace* space) {
  Initialize(space->first_allocatable_address(), space->top());
}


void SemiSpaceIterator::Initialize(Address start, Address end) {
  SemiSpace::AssertValidRange(start, end);
  current_ = start;
  limit_ = end;
}

size_t NewSpace::CommittedPhysicalMemory() {
  if (!base::OS::HasLazyCommits()) return CommittedMemory();
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  size_t size = to_space_.CommittedPhysicalMemory();
  if (from_space_.is_committed()) {
    size += from_space_.CommittedPhysicalMemory();
  }
  return size;
}


// -----------------------------------------------------------------------------
// Free lists for old object spaces implementation


void FreeListCategory::Reset() {
  set_top(FreeSpace());
  set_prev(nullptr);
  set_next(nullptr);
  available_ = 0;
}

FreeSpace FreeListCategory::PickNodeFromList(size_t minimum_size,
                                             size_t* node_size) {
  DCHECK(page()->CanAllocate());
  FreeSpace node = top();
  if (node.is_null() || static_cast<size_t>(node->Size()) < minimum_size) {
    *node_size = 0;
    return FreeSpace();
  }
  set_top(node->next());
  *node_size = node->Size();
  available_ -= *node_size;
  return node;
}

FreeSpace FreeListCategory::SearchForNodeInList(size_t minimum_size,
                                                size_t* node_size) {
  DCHECK(page()->CanAllocate());
  FreeSpace prev_non_evac_node;
  for (FreeSpace cur_node = top(); !cur_node.is_null();
       cur_node = cur_node->next()) {
    size_t size = cur_node->size();
    if (size >= minimum_size) {
      DCHECK_GE(available_, size);
      available_ -= size;
      if (cur_node == top()) {
        set_top(cur_node->next());
      }
      if (!prev_non_evac_node.is_null()) {
        MemoryChunk* chunk = MemoryChunk::FromHeapObject(prev_non_evac_node);
        if (chunk->owner()->identity() == CODE_SPACE) {
          chunk->heap()->UnprotectAndRegisterMemoryChunk(chunk);
        }
        prev_non_evac_node->set_next(cur_node->next());
      }
      *node_size = size;
      return cur_node;
    }

    prev_non_evac_node = cur_node;
  }
  return FreeSpace();
}

void FreeListCategory::Free(Address start, size_t size_in_bytes,
                            FreeMode mode) {
  FreeSpace free_space = FreeSpace::cast(HeapObject::FromAddress(start));
  free_space->set_next(top());
  set_top(free_space);
  available_ += size_in_bytes;
  if ((mode == kLinkCategory) && (prev() == nullptr) && (next() == nullptr)) {
    owner()->AddCategory(this);
  }
}


void FreeListCategory::RepairFreeList(Heap* heap) {
  FreeSpace n = top();
  while (!n.is_null()) {
    MapWordSlot map_location = n.map_slot();
    // We can't use .is_null() here because *map_location returns an
    // Object (for which "is null" is not defined, as it would be
    // indistinguishable from "is Smi(0)"). Only HeapObject has "is_null()".
    if (map_location.contains_value(kNullAddress)) {
      map_location.store(ReadOnlyRoots(heap).free_space_map());
    } else {
      DCHECK(map_location.contains_value(
          ReadOnlyRoots(heap).free_space_map().ptr()));
    }
    n = n->next();
  }
}

void FreeListCategory::Relink() {
  DCHECK(!is_linked());
  owner()->AddCategory(this);
}

FreeList::FreeList() : wasted_bytes_(0) {
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    categories_[i] = nullptr;
  }
  Reset();
}


void FreeList::Reset() {
  ForAllFreeListCategories(
      [](FreeListCategory* category) { category->Reset(); });
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    categories_[i] = nullptr;
  }
  ResetStats();
}

size_t FreeList::Free(Address start, size_t size_in_bytes, FreeMode mode) {
  Page* page = Page::FromAddress(start);
  page->DecreaseAllocatedBytes(size_in_bytes);

  // Blocks have to be a minimum size to hold free list items.
  if (size_in_bytes < kMinBlockSize) {
    page->add_wasted_memory(size_in_bytes);
    wasted_bytes_ += size_in_bytes;
    return size_in_bytes;
  }

  // Insert other blocks at the head of a free list of the appropriate
  // magnitude.
  FreeListCategoryType type = SelectFreeListCategoryType(size_in_bytes);
  page->free_list_category(type)->Free(start, size_in_bytes, mode);
  DCHECK_EQ(page->AvailableInFreeList(),
            page->AvailableInFreeListFromAllocatedBytes());
  return 0;
}

FreeSpace FreeList::FindNodeIn(FreeListCategoryType type, size_t minimum_size,
                               size_t* node_size) {
  FreeListCategoryIterator it(this, type);
  FreeSpace node;
  while (it.HasNext()) {
    FreeListCategory* current = it.Next();
    node = current->PickNodeFromList(minimum_size, node_size);
    if (!node.is_null()) {
      DCHECK(IsVeryLong() || Available() == SumFreeLists());
      return node;
    }
    RemoveCategory(current);
  }
  return node;
}

FreeSpace FreeList::TryFindNodeIn(FreeListCategoryType type,
                                  size_t minimum_size, size_t* node_size) {
  if (categories_[type] == nullptr) return FreeSpace();
  FreeSpace node = categories_[type]->PickNodeFromList(minimum_size, node_size);
  if (!node.is_null()) {
    DCHECK(IsVeryLong() || Available() == SumFreeLists());
  }
  return node;
}

FreeSpace FreeList::SearchForNodeInList(FreeListCategoryType type,
                                        size_t* node_size,
                                        size_t minimum_size) {
  FreeListCategoryIterator it(this, type);
  FreeSpace node;
  while (it.HasNext()) {
    FreeListCategory* current = it.Next();
    node = current->SearchForNodeInList(minimum_size, node_size);
    if (!node.is_null()) {
      DCHECK(IsVeryLong() || Available() == SumFreeLists());
      return node;
    }
    if (current->is_empty()) {
      RemoveCategory(current);
    }
  }
  return node;
}

FreeSpace FreeList::Allocate(size_t size_in_bytes, size_t* node_size) {
  DCHECK_GE(kMaxBlockSize, size_in_bytes);
  FreeSpace node;
  // First try the allocation fast path: try to allocate the minimum element
  // size of a free list category. This operation is constant time.
  FreeListCategoryType type =
      SelectFastAllocationFreeListCategoryType(size_in_bytes);
  for (int i = type; i < kHuge && node.is_null(); i++) {
    node = FindNodeIn(static_cast<FreeListCategoryType>(i), size_in_bytes,
                      node_size);
  }

  if (node.is_null()) {
    // Next search the huge list for free list nodes. This takes linear time in
    // the number of huge elements.
    node = SearchForNodeInList(kHuge, node_size, size_in_bytes);
  }

  if (node.is_null() && type != kHuge) {
    // We didn't find anything in the huge list. Now search the best fitting
    // free list for a node that has at least the requested size.
    type = SelectFreeListCategoryType(size_in_bytes);
    node = TryFindNodeIn(type, size_in_bytes, node_size);
  }

  if (!node.is_null()) {
    Page::FromHeapObject(node)->IncreaseAllocatedBytes(*node_size);
  }

  DCHECK(IsVeryLong() || Available() == SumFreeLists());
  return node;
}

size_t FreeList::EvictFreeListItems(Page* page) {
  size_t sum = 0;
  page->ForAllFreeListCategories([this, &sum](FreeListCategory* category) {
    DCHECK_EQ(this, category->owner());
    sum += category->available();
    RemoveCategory(category);
    category->Reset();
  });
  return sum;
}

bool FreeList::ContainsPageFreeListItems(Page* page) {
  bool contained = false;
  page->ForAllFreeListCategories(
      [this, &contained](FreeListCategory* category) {
        if (category->owner() == this && category->is_linked()) {
          contained = true;
        }
      });
  return contained;
}

void FreeList::RepairLists(Heap* heap) {
  ForAllFreeListCategories(
      [heap](FreeListCategory* category) { category->RepairFreeList(heap); });
}

bool FreeList::AddCategory(FreeListCategory* category) {
  FreeListCategoryType type = category->type_;
  DCHECK_LT(type, kNumberOfCategories);
  FreeListCategory* top = categories_[type];

  if (category->is_empty()) return false;
  if (top == category) return false;

  // Common double-linked list insertion.
  if (top != nullptr) {
    top->set_prev(category);
  }
  category->set_next(top);
  categories_[type] = category;
  return true;
}

void FreeList::RemoveCategory(FreeListCategory* category) {
  FreeListCategoryType type = category->type_;
  DCHECK_LT(type, kNumberOfCategories);
  FreeListCategory* top = categories_[type];

  // Common double-linked list removal.
  if (top == category) {
    categories_[type] = category->next();
  }
  if (category->prev() != nullptr) {
    category->prev()->set_next(category->next());
  }
  if (category->next() != nullptr) {
    category->next()->set_prev(category->prev());
  }
  category->set_next(nullptr);
  category->set_prev(nullptr);
}

void FreeList::PrintCategories(FreeListCategoryType type) {
  FreeListCategoryIterator it(this, type);
  PrintF("FreeList[%p, top=%p, %d] ", static_cast<void*>(this),
         static_cast<void*>(categories_[type]), type);
  while (it.HasNext()) {
    FreeListCategory* current = it.Next();
    PrintF("%p -> ", static_cast<void*>(current));
  }
  PrintF("null\n");
}


#ifdef DEBUG
size_t FreeListCategory::SumFreeList() {
  size_t sum = 0;
  FreeSpace cur = top();
  while (!cur.is_null()) {
    // We can't use "cur->map()" here because both cur's map and the
    // root can be null during bootstrapping.
    DCHECK(cur->map_slot().contains_value(
        page()->heap()->isolate()->root(RootIndex::kFreeSpaceMap).ptr()));
    sum += cur->relaxed_read_size();
    cur = cur->next();
  }
  return sum;
}

int FreeListCategory::FreeListLength() {
  int length = 0;
  FreeSpace cur = top();
  while (!cur.is_null()) {
    length++;
    cur = cur->next();
    if (length == kVeryLongFreeList) return length;
  }
  return length;
}

bool FreeList::IsVeryLong() {
  int len = 0;
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    FreeListCategoryIterator it(this, static_cast<FreeListCategoryType>(i));
    while (it.HasNext()) {
      len += it.Next()->FreeListLength();
      if (len >= FreeListCategory::kVeryLongFreeList) return true;
    }
  }
  return false;
}


// This can take a very long time because it is linear in the number of entries
// on the free list, so it should not be called if FreeListLength returns
// kVeryLongFreeList.
size_t FreeList::SumFreeLists() {
  size_t sum = 0;
  ForAllFreeListCategories(
      [&sum](FreeListCategory* category) { sum += category->SumFreeList(); });
  return sum;
}
#endif


// -----------------------------------------------------------------------------
// OldSpace implementation

void PagedSpace::PrepareForMarkCompact() {
  // We don't have a linear allocation area while sweeping.  It will be restored
  // on the first allocation after the sweep.
  FreeLinearAllocationArea();

  // Clear the free list before a full GC---it will be rebuilt afterward.
  free_list_.Reset();
}

size_t PagedSpace::SizeOfObjects() {
  CHECK_GE(limit(), top());
  DCHECK_GE(Size(), static_cast<size_t>(limit() - top()));
  return Size() - (limit() - top());
}

bool PagedSpace::SweepAndRetryAllocation(int size_in_bytes) {
  MarkCompactCollector* collector = heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    // Wait for the sweeper threads here and complete the sweeping phase.
    collector->EnsureSweepingCompleted();

    // After waiting for the sweeper threads, there may be new free-list
    // entries.
    return RefillLinearAllocationAreaFromFreeList(size_in_bytes);
  }
  return false;
}

bool CompactionSpace::SweepAndRetryAllocation(int size_in_bytes) {
  MarkCompactCollector* collector = heap()->mark_compact_collector();
  if (FLAG_concurrent_sweeping && collector->sweeping_in_progress()) {
    collector->sweeper()->ParallelSweepSpace(identity(), 0);
    RefillFreeList();
    return RefillLinearAllocationAreaFromFreeList(size_in_bytes);
  }
  return false;
}

bool PagedSpace::SlowRefillLinearAllocationArea(int size_in_bytes) {
  VMState<GC> state(heap()->isolate());
  RuntimeCallTimerScope runtime_timer(
      heap()->isolate(), RuntimeCallCounterId::kGC_Custom_SlowAllocateRaw);
  return RawSlowRefillLinearAllocationArea(size_in_bytes);
}

bool CompactionSpace::SlowRefillLinearAllocationArea(int size_in_bytes) {
  return RawSlowRefillLinearAllocationArea(size_in_bytes);
}

bool PagedSpace::RawSlowRefillLinearAllocationArea(int size_in_bytes) {
  // Allocation in this space has failed.
  DCHECK_GE(size_in_bytes, 0);
  const int kMaxPagesToSweep = 1;

  if (RefillLinearAllocationAreaFromFreeList(size_in_bytes)) return true;

  MarkCompactCollector* collector = heap()->mark_compact_collector();
  // Sweeping is still in progress.
  if (collector->sweeping_in_progress()) {
    if (FLAG_concurrent_sweeping && !is_local() &&
        !collector->sweeper()->AreSweeperTasksRunning()) {
      collector->EnsureSweepingCompleted();
    }

    // First try to refill the free-list, concurrent sweeper threads
    // may have freed some objects in the meantime.
    RefillFreeList();

    // Retry the free list allocation.
    if (RefillLinearAllocationAreaFromFreeList(
            static_cast<size_t>(size_in_bytes)))
      return true;

    // If sweeping is still in progress try to sweep pages.
    int max_freed = collector->sweeper()->ParallelSweepSpace(
        identity(), size_in_bytes, kMaxPagesToSweep);
    RefillFreeList();
    if (max_freed >= size_in_bytes) {
      if (RefillLinearAllocationAreaFromFreeList(
              static_cast<size_t>(size_in_bytes)))
        return true;
    }
  }

  if (is_local()) {
    // The main thread may have acquired all swept pages. Try to steal from
    // it. This can only happen during young generation evacuation.
    PagedSpace* main_space = heap()->paged_space(identity());
    Page* page = main_space->RemovePageSafe(size_in_bytes);
    if (page != nullptr) {
      AddPage(page);
      if (RefillLinearAllocationAreaFromFreeList(
              static_cast<size_t>(size_in_bytes)))
        return true;
    }
  }

  if (heap()->ShouldExpandOldGenerationOnSlowAllocation() && Expand()) {
    DCHECK((CountTotalPages() > 1) ||
           (static_cast<size_t>(size_in_bytes) <= free_list_.Available()));
    return RefillLinearAllocationAreaFromFreeList(
        static_cast<size_t>(size_in_bytes));
  }

  // If sweeper threads are active, wait for them at that point and steal
  // elements form their free-lists. Allocation may still fail their which
  // would indicate that there is not enough memory for the given allocation.
  return SweepAndRetryAllocation(size_in_bytes);
}

// -----------------------------------------------------------------------------
// MapSpace implementation

#ifdef VERIFY_HEAP
void MapSpace::VerifyObject(HeapObject object) { CHECK(object->IsMap()); }
#endif

ReadOnlySpace::ReadOnlySpace(Heap* heap)
    : PagedSpace(heap, RO_SPACE, NOT_EXECUTABLE),
      is_string_padding_cleared_(heap->isolate()->initialized_from_snapshot()) {
}

void ReadOnlyPage::MakeHeaderRelocatable() {
  if (mutex_ != nullptr) {
    // TODO(v8:7464): heap_ and owner_ need to be cleared as well.
    delete mutex_;
    mutex_ = nullptr;
    local_tracker_ = nullptr;
    reservation_.Reset();
  }
}

void ReadOnlySpace::Forget() {
  for (Page* p : *this) {
    heap()->memory_allocator()->PreFreeMemory(p);
  }
}

void ReadOnlySpace::SetPermissionsForPages(PageAllocator::Permission access) {
  MemoryAllocator* memory_allocator = heap()->memory_allocator();
  for (Page* p : *this) {
    ReadOnlyPage* page = static_cast<ReadOnlyPage*>(p);
    if (access == PageAllocator::kRead) {
      page->MakeHeaderRelocatable();
    }

    // Read only pages don't have valid reservation object so we get proper
    // page allocator manually.
    v8::PageAllocator* page_allocator =
        memory_allocator->page_allocator(page->executable());
    CHECK(
        SetPermissions(page_allocator, page->address(), page->size(), access));
  }
}

// After we have booted, we have created a map which represents free space
// on the heap.  If there was already a free list then the elements on it
// were created with the wrong FreeSpaceMap (normally nullptr), so we need to
// fix them.
void ReadOnlySpace::RepairFreeListsAfterDeserialization() {
  free_list_.RepairLists(heap());
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
      CHECK(filler->IsFiller());
      start += filler->Size();
    }
    CHECK_EQ(size, static_cast<int>(end - start));
    heap()->CreateFillerObjectAt(start, size, ClearRecordedSlots::kNo);
  }
}

void ReadOnlySpace::ClearStringPaddingIfNeeded() {
  if (is_string_padding_cleared_) return;

  WritableScope writable_scope(this);
  for (Page* page : *this) {
    HeapObjectIterator iterator(page);
    for (HeapObject o = iterator.Next(); !o.is_null(); o = iterator.Next()) {
      if (o->IsSeqOneByteString()) {
        SeqOneByteString::cast(o)->clear_padding();
      } else if (o->IsSeqTwoByteString()) {
        SeqTwoByteString::cast(o)->clear_padding();
      }
    }
  }
  is_string_padding_cleared_ = true;
}

void ReadOnlySpace::MarkAsReadOnly() {
  DCHECK(!is_marked_read_only_);
  FreeLinearAllocationArea();
  is_marked_read_only_ = true;
  SetPermissionsForPages(PageAllocator::kRead);
}

void ReadOnlySpace::MarkAsReadWrite() {
  DCHECK(is_marked_read_only_);
  SetPermissionsForPages(PageAllocator::kReadWrite);
  is_marked_read_only_ = false;
}

Address LargePage::GetAddressToShrink(Address object_address,
                                      size_t object_size) {
  if (executable() == EXECUTABLE) {
    return 0;
  }
  size_t used_size = ::RoundUp((object_address - address()) + object_size,
                               MemoryAllocator::GetCommitPageSize());
  if (used_size < CommittedPhysicalMemory()) {
    return address() + used_size;
  }
  return 0;
}

void LargePage::ClearOutOfLiveRangeSlots(Address free_start) {
  RememberedSet<OLD_TO_NEW>::RemoveRange(this, free_start, area_end(),
                                         SlotSet::FREE_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_OLD>::RemoveRange(this, free_start, area_end(),
                                         SlotSet::FREE_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_NEW>::RemoveRangeTyped(this, free_start, area_end());
  RememberedSet<OLD_TO_OLD>::RemoveRangeTyped(this, free_start, area_end());
}

// -----------------------------------------------------------------------------
// LargeObjectIterator

LargeObjectIterator::LargeObjectIterator(LargeObjectSpace* space) {
  current_ = space->first_page();
}

HeapObject LargeObjectIterator::Next() {
  if (current_ == nullptr) return HeapObject();

  HeapObject object = current_->GetObject();
  current_ = current_->next_page();
  return object;
}

// -----------------------------------------------------------------------------
// LargeObjectSpace

LargeObjectSpace::LargeObjectSpace(Heap* heap)
    : LargeObjectSpace(heap, LO_SPACE) {}

LargeObjectSpace::LargeObjectSpace(Heap* heap, AllocationSpace id)
    : Space(heap, id), size_(0), page_count_(0), objects_size_(0) {}

void LargeObjectSpace::TearDown() {
  while (!memory_chunk_list_.Empty()) {
    LargePage* page = first_page();
    LOG(heap()->isolate(),
        DeleteEvent("LargeObjectChunk",
                    reinterpret_cast<void*>(page->address())));
    memory_chunk_list_.Remove(page);
    heap()->memory_allocator()->Free<MemoryAllocator::kFull>(page);
  }
}

AllocationResult LargeObjectSpace::AllocateRaw(int object_size) {
  return AllocateRaw(object_size, NOT_EXECUTABLE);
}

AllocationResult LargeObjectSpace::AllocateRaw(int object_size,
                                               Executability executable) {
  // Check if we want to force a GC before growing the old space further.
  // If so, fail the allocation.
  if (!heap()->CanExpandOldGeneration(object_size) ||
      !heap()->ShouldExpandOldGenerationOnSlowAllocation()) {
    return AllocationResult::Retry(identity());
  }

  LargePage* page = AllocateLargePage(object_size, executable);
  if (page == nullptr) return AllocationResult::Retry(identity());
  page->SetOldGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  HeapObject object = page->GetObject();
  heap()->StartIncrementalMarkingIfAllocationLimitIsReached(
      heap()->GCFlagsForIncrementalMarking(),
      kGCCallbackScheduleIdleGarbageCollection);
  if (heap()->incremental_marking()->black_allocation()) {
    heap()->incremental_marking()->marking_state()->WhiteToBlack(object);
  }
  DCHECK_IMPLIES(
      heap()->incremental_marking()->black_allocation(),
      heap()->incremental_marking()->marking_state()->IsBlack(object));
  page->InitializationMemoryFence();
  heap()->NotifyOldGenerationExpansion();
  AllocationStep(object_size, object->address(), object_size);
  return object;
}

LargePage* LargeObjectSpace::AllocateLargePage(int object_size,
                                               Executability executable) {
  LargePage* page = heap()->memory_allocator()->AllocateLargePage(
      object_size, this, executable);
  if (page == nullptr) return nullptr;
  DCHECK_GE(page->area_size(), static_cast<size_t>(object_size));

  AddPage(page, object_size);

  HeapObject object = page->GetObject();

  heap()->CreateFillerObjectAt(object->address(), object_size,
                               ClearRecordedSlots::kNo);
  return page;
}


size_t LargeObjectSpace::CommittedPhysicalMemory() {
  // On a platform that provides lazy committing of memory, we over-account
  // the actually committed memory. There is no easy way right now to support
  // precise accounting of committed memory in large object space.
  return CommittedMemory();
}

LargePage* CodeLargeObjectSpace::FindPage(Address a) {
  const Address key = MemoryChunk::FromAddress(a)->address();
  auto it = chunk_map_.find(key);
  if (it != chunk_map_.end()) {
    LargePage* page = it->second;
    CHECK(page->Contains(a));
    return page;
  }
  return nullptr;
}

void LargeObjectSpace::ClearMarkingStateOfLiveObjects() {
  IncrementalMarking::NonAtomicMarkingState* marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
  LargeObjectIterator it(this);
  for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
    if (marking_state->IsBlackOrGrey(obj)) {
      Marking::MarkWhite(marking_state->MarkBitFrom(obj));
      MemoryChunk* chunk = MemoryChunk::FromHeapObject(obj);
      RememberedSet<OLD_TO_NEW>::FreeEmptyBuckets(chunk);
      chunk->ResetProgressBar();
      marking_state->SetLiveBytes(chunk, 0);
    }
    DCHECK(marking_state->IsWhite(obj));
  }
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

void LargeObjectSpace::PromoteNewLargeObject(LargePage* page) {
  DCHECK_EQ(page->owner()->identity(), NEW_LO_SPACE);
  DCHECK(page->IsLargePage());
  DCHECK(page->IsFlagSet(MemoryChunk::FROM_PAGE));
  DCHECK(!page->IsFlagSet(MemoryChunk::TO_PAGE));
  size_t object_size = static_cast<size_t>(page->GetObject()->Size());
  static_cast<LargeObjectSpace*>(page->owner())->RemovePage(page, object_size);
  AddPage(page, object_size);
  page->ClearFlag(MemoryChunk::FROM_PAGE);
  page->SetOldGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  page->set_owner(this);
}

void LargeObjectSpace::AddPage(LargePage* page, size_t object_size) {
  size_ += static_cast<int>(page->size());
  AccountCommitted(page->size());
  objects_size_ += object_size;
  page_count_++;
  memory_chunk_list_.PushBack(page);
}

void LargeObjectSpace::RemovePage(LargePage* page, size_t object_size) {
  size_ -= static_cast<int>(page->size());
  AccountUncommitted(page->size());
  objects_size_ -= object_size;
  page_count_--;
  memory_chunk_list_.Remove(page);
}

void LargeObjectSpace::FreeUnmarkedObjects() {
  LargePage* current = first_page();
  IncrementalMarking::NonAtomicMarkingState* marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
  // Right-trimming does not update the objects_size_ counter. We are lazily
  // updating it after every GC.
  size_t surviving_object_size = 0;
  while (current) {
    LargePage* next_current = current->next_page();
    HeapObject object = current->GetObject();
    DCHECK(!marking_state->IsGrey(object));
    size_t size = static_cast<size_t>(object->Size());
    if (marking_state->IsBlack(object)) {
      Address free_start;
      surviving_object_size += size;
      if ((free_start = current->GetAddressToShrink(object->address(), size)) !=
          0) {
        DCHECK(!current->IsFlagSet(Page::IS_EXECUTABLE));
        current->ClearOutOfLiveRangeSlots(free_start);
        const size_t bytes_to_free =
            current->size() - (free_start - current->address());
        heap()->memory_allocator()->PartialFreeMemory(
            current, free_start, bytes_to_free,
            current->area_start() + object->Size());
        size_ -= bytes_to_free;
        AccountUncommitted(bytes_to_free);
      }
    } else {
      RemovePage(current, size);
      heap()->memory_allocator()->Free<MemoryAllocator::kPreFreeAndQueue>(
          current);
    }
    current = next_current;
  }
  objects_size_ = surviving_object_size;
}

bool LargeObjectSpace::Contains(HeapObject object) {
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);

  bool owned = (chunk->owner() == this);

  SLOW_DCHECK(!owned || ContainsSlow(object->address()));

  return owned;
}

bool LargeObjectSpace::ContainsSlow(Address addr) {
  for (LargePage* page : *this) {
    if (page->Contains(addr)) return true;
  }
  return false;
}

std::unique_ptr<ObjectIterator> LargeObjectSpace::GetObjectIterator() {
  return std::unique_ptr<ObjectIterator>(new LargeObjectIterator(this));
}

#ifdef VERIFY_HEAP
// We do not assume that the large object iterator works, because it depends
// on the invariants we are checking during verification.
void LargeObjectSpace::Verify(Isolate* isolate) {
  size_t external_backing_store_bytes[kNumTypes];

  for (int i = 0; i < kNumTypes; i++) {
    external_backing_store_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  for (LargePage* chunk = first_page(); chunk != nullptr;
       chunk = chunk->next_page()) {
    // Each chunk contains an object that starts at the large object page's
    // object area start.
    HeapObject object = chunk->GetObject();
    Page* page = Page::FromHeapObject(object);
    CHECK(object->address() == page->area_start());

    // The first word should be a map, and we expect all map pointers to be
    // in map space or read-only space.
    Map map = object->map();
    CHECK(map->IsMap());
    CHECK(heap()->map_space()->Contains(map) ||
          heap()->read_only_space()->Contains(map));

    // We have only the following types in the large object space:
    if (!(object->IsAbstractCode() || object->IsSeqString() ||
          object->IsExternalString() || object->IsThinString() ||
          object->IsFixedArray() || object->IsFixedDoubleArray() ||
          object->IsWeakFixedArray() || object->IsWeakArrayList() ||
          object->IsPropertyArray() || object->IsByteArray() ||
          object->IsFeedbackVector() || object->IsBigInt() ||
          object->IsFreeSpace() || object->IsFeedbackMetadata() ||
          object->IsContext() ||
          object->IsUncompiledDataWithoutPreparseData() ||
          object->IsPreparseData()) &&
        !FLAG_young_generation_large_objects) {
      FATAL("Found invalid Object (instance_type=%i) in large object space.",
            object->map()->instance_type());
    }

    // The object itself should look OK.
    object->ObjectVerify(isolate);

    if (!FLAG_verify_heap_skip_remembered_set) {
      heap()->VerifyRememberedSetFor(object);
    }

    // Byte arrays and strings don't have interior pointers.
    if (object->IsAbstractCode()) {
      VerifyPointersVisitor code_visitor(heap());
      object->IterateBody(map, object->Size(), &code_visitor);
    } else if (object->IsFixedArray()) {
      FixedArray array = FixedArray::cast(object);
      for (int j = 0; j < array->length(); j++) {
        Object element = array->get(j);
        if (element->IsHeapObject()) {
          HeapObject element_object = HeapObject::cast(element);
          CHECK(heap()->Contains(element_object));
          CHECK(element_object->map()->IsMap());
        }
      }
    } else if (object->IsPropertyArray()) {
      PropertyArray array = PropertyArray::cast(object);
      for (int j = 0; j < array->length(); j++) {
        Object property = array->get(j);
        if (property->IsHeapObject()) {
          HeapObject property_object = HeapObject::cast(property);
          CHECK(heap()->Contains(property_object));
          CHECK(property_object->map()->IsMap());
        }
      }
    }
    for (int i = 0; i < kNumTypes; i++) {
      ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
      external_backing_store_bytes[t] += chunk->ExternalBackingStoreBytes(t);
    }
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
  LargeObjectIterator it(this);
  for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
    obj->Print(os);
  }
}

void Page::Print() {
  // Make a best-effort to print the objects in the page.
  PrintF("Page@%p in %s\n", reinterpret_cast<void*>(this->address()),
         this->owner()->name());
  printf(" --------------------------------------\n");
  HeapObjectIterator objects(this);
  unsigned mark_size = 0;
  for (HeapObject object = objects.Next(); !object.is_null();
       object = objects.Next()) {
    bool is_marked =
        heap()->incremental_marking()->marking_state()->IsBlackOrGrey(object);
    PrintF(" %c ", (is_marked ? '!' : ' '));  // Indent a little.
    if (is_marked) {
      mark_size += object->Size();
    }
    object->ShortPrint();
    PrintF("\n");
  }
  printf(" --------------------------------------\n");
  printf(" Marked: %x, LiveCount: %" V8PRIdPTR "\n", mark_size,
         heap()->incremental_marking()->marking_state()->live_bytes(this));
}

#endif  // DEBUG

NewLargeObjectSpace::NewLargeObjectSpace(Heap* heap, size_t capacity)
    : LargeObjectSpace(heap, NEW_LO_SPACE),
      pending_object_(0),
      capacity_(capacity) {}

AllocationResult NewLargeObjectSpace::AllocateRaw(int object_size) {
  // Do not allocate more objects if promoting the existing object would exceed
  // the old generation capacity.
  if (!heap()->CanExpandOldGeneration(SizeOfObjects())) {
    return AllocationResult::Retry(identity());
  }

  // Allocation for the first object must succeed independent from the capacity.
  if (SizeOfObjects() > 0 && static_cast<size_t>(object_size) > Available()) {
    return AllocationResult::Retry(identity());
  }

  LargePage* page = AllocateLargePage(object_size, NOT_EXECUTABLE);
  if (page == nullptr) return AllocationResult::Retry(identity());

  // The size of the first object may exceed the capacity.
  capacity_ = Max(capacity_, SizeOfObjects());

  HeapObject result = page->GetObject();
  page->SetYoungGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  page->SetFlag(MemoryChunk::TO_PAGE);
  pending_object_.store(result->address(), std::memory_order_relaxed);
#ifdef ENABLE_MINOR_MC
  if (FLAG_minor_mc) {
    page->AllocateYoungGenerationBitmap();
    heap()
        ->minor_mark_compact_collector()
        ->non_atomic_marking_state()
        ->ClearLiveness(page);
  }
#endif  // ENABLE_MINOR_MC
  page->InitializationMemoryFence();
  DCHECK(page->IsLargePage());
  DCHECK_EQ(page->owner()->identity(), NEW_LO_SPACE);
  AllocationStep(object_size, result->address(), object_size);
  return result;
}

size_t NewLargeObjectSpace::Available() { return capacity_ - SizeOfObjects(); }

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
  bool freed_pages = false;
  for (auto it = begin(); it != end();) {
    LargePage* page = *it;
    it++;
    HeapObject object = page->GetObject();
    size_t size = static_cast<size_t>(object->Size());
    if (is_dead(object)) {
      freed_pages = true;
      RemovePage(page, size);
      heap()->memory_allocator()->Free<MemoryAllocator::kPreFreeAndQueue>(page);
      if (FLAG_concurrent_marking && is_marking) {
        heap()->concurrent_marking()->ClearMemoryChunkData(page);
      }
    } else {
      surviving_object_size += size;
    }
  }
  // Right-trimming does not update the objects_size_ counter. We are lazily
  // updating it after every GC.
  objects_size_ = surviving_object_size;
  if (freed_pages) {
    heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
  }
}

void NewLargeObjectSpace::SetCapacity(size_t capacity) {
  capacity_ = Max(capacity, SizeOfObjects());
}

CodeLargeObjectSpace::CodeLargeObjectSpace(Heap* heap)
    : LargeObjectSpace(heap, CODE_LO_SPACE),
      chunk_map_(kInitialChunkMapCapacity) {}

AllocationResult CodeLargeObjectSpace::AllocateRaw(int object_size) {
  return LargeObjectSpace::AllocateRaw(object_size, EXECUTABLE);
}

void CodeLargeObjectSpace::AddPage(LargePage* page, size_t object_size) {
  LargeObjectSpace::AddPage(page, object_size);
  InsertChunkMapEntries(page);
}

void CodeLargeObjectSpace::RemovePage(LargePage* page, size_t object_size) {
  RemoveChunkMapEntries(page);
  LargeObjectSpace::RemovePage(page, object_size);
}

}  // namespace internal
}  // namespace v8

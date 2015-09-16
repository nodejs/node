// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/base/atomicops.h"
#include "src/base/bits.h"
#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/cpu-profiler.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/gdb-jit.h"
#include "src/global-handles.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/heap-profiler.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {


const char* Marking::kWhiteBitPattern = "00";
const char* Marking::kBlackBitPattern = "10";
const char* Marking::kGreyBitPattern = "11";
const char* Marking::kImpossibleBitPattern = "01";


// -------------------------------------------------------------------------
// MarkCompactCollector

MarkCompactCollector::MarkCompactCollector(Heap* heap)
    :  // NOLINT
#ifdef DEBUG
      state_(IDLE),
#endif
      reduce_memory_footprint_(false),
      abort_incremental_marking_(false),
      finalize_incremental_marking_(false),
      marking_parity_(ODD_MARKING_PARITY),
      compacting_(false),
      was_marked_incrementally_(false),
      sweeping_in_progress_(false),
      pending_sweeper_jobs_semaphore_(0),
      evacuation_(false),
      migration_slots_buffer_(NULL),
      heap_(heap),
      marking_deque_memory_(NULL),
      marking_deque_memory_committed_(0),
      code_flusher_(NULL),
      have_code_to_deoptimize_(false) {
}

#ifdef VERIFY_HEAP
class VerifyMarkingVisitor : public ObjectVisitor {
 public:
  explicit VerifyMarkingVisitor(Heap* heap) : heap_(heap) {}

  void VisitPointers(Object** start, Object** end) {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        CHECK(heap_->mark_compact_collector()->IsMarked(object));
      }
    }
  }

  void VisitEmbeddedPointer(RelocInfo* rinfo) {
    DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
    if (!rinfo->host()->IsWeakObject(rinfo->target_object())) {
      Object* p = rinfo->target_object();
      VisitPointer(&p);
    }
  }

  void VisitCell(RelocInfo* rinfo) {
    Code* code = rinfo->host();
    DCHECK(rinfo->rmode() == RelocInfo::CELL);
    if (!code->IsWeakObject(rinfo->target_cell())) {
      ObjectVisitor::VisitCell(rinfo);
    }
  }

 private:
  Heap* heap_;
};


static void VerifyMarking(Heap* heap, Address bottom, Address top) {
  VerifyMarkingVisitor visitor(heap);
  HeapObject* object;
  Address next_object_must_be_here_or_later = bottom;

  for (Address current = bottom; current < top; current += kPointerSize) {
    object = HeapObject::FromAddress(current);
    if (MarkCompactCollector::IsMarked(object)) {
      CHECK(Marking::IsBlack(Marking::MarkBitFrom(object)));
      CHECK(current >= next_object_must_be_here_or_later);
      object->Iterate(&visitor);
      next_object_must_be_here_or_later = current + object->Size();
    }
  }
}


static void VerifyMarking(NewSpace* space) {
  Address end = space->top();
  NewSpacePageIterator it(space->bottom(), end);
  // The bottom position is at the start of its page. Allows us to use
  // page->area_start() as start of range on all pages.
  CHECK_EQ(space->bottom(),
           NewSpacePage::FromAddress(space->bottom())->area_start());
  while (it.has_next()) {
    NewSpacePage* page = it.next();
    Address limit = it.has_next() ? page->area_end() : end;
    CHECK(limit == end || !page->Contains(end));
    VerifyMarking(space->heap(), page->area_start(), limit);
  }
}


static void VerifyMarking(PagedSpace* space) {
  PageIterator it(space);

  while (it.has_next()) {
    Page* p = it.next();
    VerifyMarking(space->heap(), p->area_start(), p->area_end());
  }
}


static void VerifyMarking(Heap* heap) {
  VerifyMarking(heap->old_space());
  VerifyMarking(heap->code_space());
  VerifyMarking(heap->map_space());
  VerifyMarking(heap->new_space());

  VerifyMarkingVisitor visitor(heap);

  LargeObjectIterator it(heap->lo_space());
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    if (MarkCompactCollector::IsMarked(obj)) {
      obj->Iterate(&visitor);
    }
  }

  heap->IterateStrongRoots(&visitor, VISIT_ONLY_STRONG);
}


class VerifyEvacuationVisitor : public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        CHECK(!MarkCompactCollector::IsOnEvacuationCandidate(object));
      }
    }
  }
};


static void VerifyEvacuation(Page* page) {
  VerifyEvacuationVisitor visitor;
  HeapObjectIterator iterator(page, NULL);
  for (HeapObject* heap_object = iterator.Next(); heap_object != NULL;
       heap_object = iterator.Next()) {
    // We skip free space objects.
    if (!heap_object->IsFiller()) {
      heap_object->Iterate(&visitor);
    }
  }
}


static void VerifyEvacuation(NewSpace* space) {
  NewSpacePageIterator it(space->bottom(), space->top());
  VerifyEvacuationVisitor visitor;

  while (it.has_next()) {
    NewSpacePage* page = it.next();
    Address current = page->area_start();
    Address limit = it.has_next() ? page->area_end() : space->top();
    CHECK(limit == space->top() || !page->Contains(space->top()));
    while (current < limit) {
      HeapObject* object = HeapObject::FromAddress(current);
      object->Iterate(&visitor);
      current += object->Size();
    }
  }
}


static void VerifyEvacuation(Heap* heap, PagedSpace* space) {
  if (FLAG_use_allocation_folding && (space == heap->old_space())) {
    return;
  }
  PageIterator it(space);

  while (it.has_next()) {
    Page* p = it.next();
    if (p->IsEvacuationCandidate()) continue;
    VerifyEvacuation(p);
  }
}


static void VerifyEvacuation(Heap* heap) {
  VerifyEvacuation(heap, heap->old_space());
  VerifyEvacuation(heap, heap->code_space());
  VerifyEvacuation(heap, heap->map_space());
  VerifyEvacuation(heap->new_space());

  VerifyEvacuationVisitor visitor;
  heap->IterateStrongRoots(&visitor, VISIT_ALL);
}
#endif  // VERIFY_HEAP


void MarkCompactCollector::SetUp() {
  free_list_old_space_.Reset(new FreeList(heap_->old_space()));
  EnsureMarkingDequeIsReserved();
  EnsureMarkingDequeIsCommitted(kMinMarkingDequeSize);
}


void MarkCompactCollector::TearDown() {
  AbortCompaction();
  delete marking_deque_memory_;
}


void MarkCompactCollector::AddEvacuationCandidate(Page* p) {
  DCHECK(!p->NeverEvacuate());
  p->MarkEvacuationCandidate();
  evacuation_candidates_.Add(p);
}


static void TraceFragmentation(PagedSpace* space) {
  int number_of_pages = space->CountTotalPages();
  intptr_t reserved = (number_of_pages * space->AreaSize());
  intptr_t free = reserved - space->SizeOfObjects();
  PrintF("[%s]: %d pages, %d (%.1f%%) free\n",
         AllocationSpaceName(space->identity()), number_of_pages,
         static_cast<int>(free), static_cast<double>(free) * 100 / reserved);
}


bool MarkCompactCollector::StartCompaction(CompactionMode mode) {
  if (!compacting_) {
    DCHECK(evacuation_candidates_.length() == 0);

    CollectEvacuationCandidates(heap()->old_space());

    if (FLAG_compact_code_space) {
      CollectEvacuationCandidates(heap()->code_space());
    } else if (FLAG_trace_fragmentation) {
      TraceFragmentation(heap()->code_space());
    }

    if (FLAG_trace_fragmentation) {
      TraceFragmentation(heap()->map_space());
    }

    heap()->old_space()->EvictEvacuationCandidatesFromFreeLists();
    heap()->code_space()->EvictEvacuationCandidatesFromFreeLists();

    compacting_ = evacuation_candidates_.length() > 0;
  }

  return compacting_;
}


void MarkCompactCollector::ClearInvalidSlotsBufferEntries(PagedSpace* space) {
  PageIterator it(space);
  while (it.has_next()) {
    Page* p = it.next();
    SlotsBuffer::RemoveInvalidSlots(heap_, p->slots_buffer());
  }
}


void MarkCompactCollector::ClearInvalidStoreAndSlotsBufferEntries() {
  heap_->store_buffer()->ClearInvalidStoreBufferEntries();

  ClearInvalidSlotsBufferEntries(heap_->old_space());
  ClearInvalidSlotsBufferEntries(heap_->code_space());
  ClearInvalidSlotsBufferEntries(heap_->map_space());

  LargeObjectIterator it(heap_->lo_space());
  for (HeapObject* object = it.Next(); object != NULL; object = it.Next()) {
    MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
    SlotsBuffer::RemoveInvalidSlots(heap_, chunk->slots_buffer());
  }
}


#ifdef VERIFY_HEAP
static void VerifyValidSlotsBufferEntries(Heap* heap, PagedSpace* space) {
  PageIterator it(space);
  while (it.has_next()) {
    Page* p = it.next();
    SlotsBuffer::VerifySlots(heap, p->slots_buffer());
  }
}


static void VerifyValidStoreAndSlotsBufferEntries(Heap* heap) {
  heap->store_buffer()->VerifyValidStoreBufferEntries();

  VerifyValidSlotsBufferEntries(heap, heap->old_space());
  VerifyValidSlotsBufferEntries(heap, heap->code_space());
  VerifyValidSlotsBufferEntries(heap, heap->map_space());

  LargeObjectIterator it(heap->lo_space());
  for (HeapObject* object = it.Next(); object != NULL; object = it.Next()) {
    MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
    SlotsBuffer::VerifySlots(heap, chunk->slots_buffer());
  }
}
#endif


void MarkCompactCollector::CollectGarbage() {
  // Make sure that Prepare() has been called. The individual steps below will
  // update the state as they proceed.
  DCHECK(state_ == PREPARE_GC);

  MarkLiveObjects();

  DCHECK(heap_->incremental_marking()->IsStopped());

  // ClearNonLiveReferences can deoptimize code in dependent code arrays.
  // Process weak cells before so that weak cells in dependent code
  // arrays are cleared or contain only live code objects.
  ProcessAndClearWeakCells();

  ClearNonLiveReferences();

  ClearWeakCollections();

  heap_->set_encountered_weak_cells(Smi::FromInt(0));

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    VerifyMarking(heap_);
  }
#endif

  ClearInvalidStoreAndSlotsBufferEntries();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    VerifyValidStoreAndSlotsBufferEntries(heap_);
  }
#endif

  SweepSpaces();

#ifdef VERIFY_HEAP
  VerifyWeakEmbeddedObjectsInCode();
  if (FLAG_omit_map_checks_for_leaf_maps) {
    VerifyOmittedMapChecks();
  }
#endif

  Finish();

  if (marking_parity_ == EVEN_MARKING_PARITY) {
    marking_parity_ = ODD_MARKING_PARITY;
  } else {
    DCHECK(marking_parity_ == ODD_MARKING_PARITY);
    marking_parity_ = EVEN_MARKING_PARITY;
  }
}


#ifdef VERIFY_HEAP
void MarkCompactCollector::VerifyMarkbitsAreClean(PagedSpace* space) {
  PageIterator it(space);

  while (it.has_next()) {
    Page* p = it.next();
    CHECK(p->markbits()->IsClean());
    CHECK_EQ(0, p->LiveBytes());
  }
}


void MarkCompactCollector::VerifyMarkbitsAreClean(NewSpace* space) {
  NewSpacePageIterator it(space->bottom(), space->top());

  while (it.has_next()) {
    NewSpacePage* p = it.next();
    CHECK(p->markbits()->IsClean());
    CHECK_EQ(0, p->LiveBytes());
  }
}


void MarkCompactCollector::VerifyMarkbitsAreClean() {
  VerifyMarkbitsAreClean(heap_->old_space());
  VerifyMarkbitsAreClean(heap_->code_space());
  VerifyMarkbitsAreClean(heap_->map_space());
  VerifyMarkbitsAreClean(heap_->new_space());

  LargeObjectIterator it(heap_->lo_space());
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    MarkBit mark_bit = Marking::MarkBitFrom(obj);
    CHECK(Marking::IsWhite(mark_bit));
    CHECK_EQ(0, Page::FromAddress(obj->address())->LiveBytes());
  }
}


void MarkCompactCollector::VerifyWeakEmbeddedObjectsInCode() {
  HeapObjectIterator code_iterator(heap()->code_space());
  for (HeapObject* obj = code_iterator.Next(); obj != NULL;
       obj = code_iterator.Next()) {
    Code* code = Code::cast(obj);
    if (!code->is_optimized_code()) continue;
    if (WillBeDeoptimized(code)) continue;
    code->VerifyEmbeddedObjectsDependency();
  }
}


void MarkCompactCollector::VerifyOmittedMapChecks() {
  HeapObjectIterator iterator(heap()->map_space());
  for (HeapObject* obj = iterator.Next(); obj != NULL; obj = iterator.Next()) {
    Map* map = Map::cast(obj);
    map->VerifyOmittedMapChecks();
  }
}
#endif  // VERIFY_HEAP


static void ClearMarkbitsInPagedSpace(PagedSpace* space) {
  PageIterator it(space);

  while (it.has_next()) {
    Bitmap::Clear(it.next());
  }
}


static void ClearMarkbitsInNewSpace(NewSpace* space) {
  NewSpacePageIterator it(space->ToSpaceStart(), space->ToSpaceEnd());

  while (it.has_next()) {
    Bitmap::Clear(it.next());
  }
}


void MarkCompactCollector::ClearMarkbits() {
  ClearMarkbitsInPagedSpace(heap_->code_space());
  ClearMarkbitsInPagedSpace(heap_->map_space());
  ClearMarkbitsInPagedSpace(heap_->old_space());
  ClearMarkbitsInNewSpace(heap_->new_space());

  LargeObjectIterator it(heap_->lo_space());
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    Marking::MarkWhite(Marking::MarkBitFrom(obj));
    Page::FromAddress(obj->address())->ResetProgressBar();
    Page::FromAddress(obj->address())->ResetLiveBytes();
  }
}


class MarkCompactCollector::SweeperTask : public v8::Task {
 public:
  SweeperTask(Heap* heap, PagedSpace* space) : heap_(heap), space_(space) {}

  virtual ~SweeperTask() {}

 private:
  // v8::Task overrides.
  void Run() override {
    heap_->mark_compact_collector()->SweepInParallel(space_, 0);
    heap_->mark_compact_collector()->pending_sweeper_jobs_semaphore_.Signal();
  }

  Heap* heap_;
  PagedSpace* space_;

  DISALLOW_COPY_AND_ASSIGN(SweeperTask);
};


void MarkCompactCollector::StartSweeperThreads() {
  DCHECK(free_list_old_space_.get()->IsEmpty());
  V8::GetCurrentPlatform()->CallOnBackgroundThread(
      new SweeperTask(heap(), heap()->old_space()),
      v8::Platform::kShortRunningTask);
}


void MarkCompactCollector::EnsureSweepingCompleted() {
  DCHECK(sweeping_in_progress_ == true);

  // If sweeping is not completed or not running at all, we try to complete it
  // here.
  if (!heap()->concurrent_sweeping_enabled() || !IsSweepingCompleted()) {
    SweepInParallel(heap()->paged_space(OLD_SPACE), 0);
  }
  // Wait twice for both jobs.
  if (heap()->concurrent_sweeping_enabled()) {
    pending_sweeper_jobs_semaphore_.Wait();
  }
  ParallelSweepSpacesComplete();
  sweeping_in_progress_ = false;
  RefillFreeList(heap()->paged_space(OLD_SPACE));
  heap()->paged_space(OLD_SPACE)->ResetUnsweptFreeBytes();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && !evacuation()) {
    VerifyEvacuation(heap_);
  }
#endif
}


bool MarkCompactCollector::IsSweepingCompleted() {
  if (!pending_sweeper_jobs_semaphore_.WaitFor(
          base::TimeDelta::FromSeconds(0))) {
    return false;
  }
  pending_sweeper_jobs_semaphore_.Signal();
  return true;
}


void MarkCompactCollector::RefillFreeList(PagedSpace* space) {
  FreeList* free_list;

  if (space == heap()->old_space()) {
    free_list = free_list_old_space_.get();
  } else {
    // Any PagedSpace might invoke RefillFreeLists, so we need to make sure
    // to only refill them for the old space.
    return;
  }

  intptr_t freed_bytes = space->free_list()->Concatenate(free_list);
  space->AddToAccountingStats(freed_bytes);
  space->DecrementUnsweptFreeBytes(freed_bytes);
}


void Marking::TransferMark(Address old_start, Address new_start) {
  // This is only used when resizing an object.
  DCHECK(MemoryChunk::FromAddress(old_start) ==
         MemoryChunk::FromAddress(new_start));

  if (!heap_->incremental_marking()->IsMarking()) return;

  // If the mark doesn't move, we don't check the color of the object.
  // It doesn't matter whether the object is black, since it hasn't changed
  // size, so the adjustment to the live data count will be zero anyway.
  if (old_start == new_start) return;

  MarkBit new_mark_bit = MarkBitFrom(new_start);
  MarkBit old_mark_bit = MarkBitFrom(old_start);

#ifdef DEBUG
  ObjectColor old_color = Color(old_mark_bit);
#endif

  if (Marking::IsBlack(old_mark_bit)) {
    Marking::BlackToWhite(old_mark_bit);
    Marking::MarkBlack(new_mark_bit);
    return;
  } else if (Marking::IsGrey(old_mark_bit)) {
    Marking::GreyToWhite(old_mark_bit);
    heap_->incremental_marking()->WhiteToGreyAndPush(
        HeapObject::FromAddress(new_start), new_mark_bit);
    heap_->incremental_marking()->RestartIfNotMarking();
  }

#ifdef DEBUG
  ObjectColor new_color = Color(new_mark_bit);
  DCHECK(new_color == old_color);
#endif
}


const char* AllocationSpaceName(AllocationSpace space) {
  switch (space) {
    case NEW_SPACE:
      return "NEW_SPACE";
    case OLD_SPACE:
      return "OLD_SPACE";
    case CODE_SPACE:
      return "CODE_SPACE";
    case MAP_SPACE:
      return "MAP_SPACE";
    case LO_SPACE:
      return "LO_SPACE";
    default:
      UNREACHABLE();
  }

  return NULL;
}


void MarkCompactCollector::CollectEvacuationCandidates(PagedSpace* space) {
  DCHECK(space->identity() == OLD_SPACE || space->identity() == CODE_SPACE);

  int number_of_pages = space->CountTotalPages();
  int area_size = space->AreaSize();

  // Pairs of (live_bytes_in_page, page).
  std::vector<std::pair<int, Page*> > pages;
  pages.reserve(number_of_pages);

  PageIterator it(space);
  while (it.has_next()) {
    Page* p = it.next();
    if (p->NeverEvacuate()) continue;
    if (p->IsFlagSet(Page::POPULAR_PAGE)) {
      // This page had slots buffer overflow on previous GC, skip it.
      p->ClearFlag(Page::POPULAR_PAGE);
      continue;
    }
    // Invariant: Evacuation candidates are just created when marking is
    // started. At the end of a GC all evacuation candidates are cleared and
    // their slot buffers are released.
    CHECK(!p->IsEvacuationCandidate());
    CHECK(p->slots_buffer() == NULL);
    DCHECK(p->area_size() == area_size);
    int live_bytes =
        p->WasSwept() ? p->LiveBytesFromFreeList() : p->LiveBytes();
    pages.push_back(std::make_pair(live_bytes, p));
  }

  int candidate_count = 0;
  int total_live_bytes = 0;

  bool reduce_memory =
      reduce_memory_footprint_ || heap()->HasLowAllocationRate();
  if (FLAG_manual_evacuation_candidates_selection) {
    for (size_t i = 0; i < pages.size(); i++) {
      Page* p = pages[i].second;
      if (p->IsFlagSet(MemoryChunk::FORCE_EVACUATION_CANDIDATE_FOR_TESTING)) {
        candidate_count++;
        total_live_bytes += pages[i].first;
        p->ClearFlag(MemoryChunk::FORCE_EVACUATION_CANDIDATE_FOR_TESTING);
        AddEvacuationCandidate(p);
      }
    }
  } else if (FLAG_stress_compaction) {
    for (size_t i = 0; i < pages.size(); i++) {
      Page* p = pages[i].second;
      if (i % 2 == 0) {
        candidate_count++;
        total_live_bytes += pages[i].first;
        AddEvacuationCandidate(p);
      }
    }
  } else {
    const int kTargetFragmentationPercent = 50;
    const int kMaxEvacuatedBytes = 4 * Page::kPageSize;

    const int kTargetFragmentationPercentForReduceMemory = 20;
    const int kMaxEvacuatedBytesForReduceMemory = 12 * Page::kPageSize;

    int max_evacuated_bytes;
    int target_fragmentation_percent;

    if (reduce_memory) {
      target_fragmentation_percent = kTargetFragmentationPercentForReduceMemory;
      max_evacuated_bytes = kMaxEvacuatedBytesForReduceMemory;
    } else {
      target_fragmentation_percent = kTargetFragmentationPercent;
      max_evacuated_bytes = kMaxEvacuatedBytes;
    }
    intptr_t free_bytes_threshold =
        target_fragmentation_percent * (area_size / 100);

    // Sort pages from the most free to the least free, then select
    // the first n pages for evacuation such that:
    // - the total size of evacuated objects does not exceed the specified
    // limit.
    // - fragmentation of (n+1)-th page does not exceed the specified limit.
    std::sort(pages.begin(), pages.end());
    for (size_t i = 0; i < pages.size(); i++) {
      int live_bytes = pages[i].first;
      int free_bytes = area_size - live_bytes;
      if (FLAG_always_compact ||
          (free_bytes >= free_bytes_threshold &&
           total_live_bytes + live_bytes <= max_evacuated_bytes)) {
        candidate_count++;
        total_live_bytes += live_bytes;
      }
      if (FLAG_trace_fragmentation_verbose) {
        PrintF(
            "Page in %s: %d KB free [fragmented if this >= %d KB], "
            "sum of live bytes in fragmented pages %d KB [max is %d KB]\n",
            AllocationSpaceName(space->identity()),
            static_cast<int>(free_bytes / KB),
            static_cast<int>(free_bytes_threshold / KB),
            static_cast<int>(total_live_bytes / KB),
            static_cast<int>(max_evacuated_bytes / KB));
      }
    }
    // How many pages we will allocated for the evacuated objects
    // in the worst case: ceil(total_live_bytes / area_size)
    int estimated_new_pages = (total_live_bytes + area_size - 1) / area_size;
    DCHECK_LE(estimated_new_pages, candidate_count);
    int estimated_released_pages = candidate_count - estimated_new_pages;
    // Avoid (compact -> expand) cycles.
    if (estimated_released_pages == 0 && !FLAG_always_compact)
      candidate_count = 0;
    for (int i = 0; i < candidate_count; i++) {
      AddEvacuationCandidate(pages[i].second);
    }
  }

  if (FLAG_trace_fragmentation) {
    PrintF(
        "Collected %d evacuation candidates [%d KB live] for space %s "
        "[mode %s]\n",
        candidate_count, static_cast<int>(total_live_bytes / KB),
        AllocationSpaceName(space->identity()),
        (reduce_memory ? "reduce memory footprint" : "normal"));
  }
}


void MarkCompactCollector::AbortCompaction() {
  if (compacting_) {
    int npages = evacuation_candidates_.length();
    for (int i = 0; i < npages; i++) {
      Page* p = evacuation_candidates_[i];
      slots_buffer_allocator_.DeallocateChain(p->slots_buffer_address());
      p->ClearEvacuationCandidate();
      p->ClearFlag(MemoryChunk::RESCAN_ON_EVACUATION);
    }
    compacting_ = false;
    evacuation_candidates_.Rewind(0);
  }
  DCHECK_EQ(0, evacuation_candidates_.length());
}


void MarkCompactCollector::Prepare() {
  was_marked_incrementally_ = heap()->incremental_marking()->IsMarking();

#ifdef DEBUG
  DCHECK(state_ == IDLE);
  state_ = PREPARE_GC;
#endif

  DCHECK(!FLAG_never_compact || !FLAG_always_compact);

  if (sweeping_in_progress()) {
    // Instead of waiting we could also abort the sweeper threads here.
    EnsureSweepingCompleted();
  }

  // Clear marking bits if incremental marking is aborted.
  if (was_marked_incrementally_ && abort_incremental_marking_) {
    heap()->incremental_marking()->Abort();
    ClearMarkbits();
    AbortWeakCollections();
    AbortWeakCells();
    AbortCompaction();
    was_marked_incrementally_ = false;
  }

  // Don't start compaction if we are in the middle of incremental
  // marking cycle. We did not collect any slots.
  if (!FLAG_never_compact && !was_marked_incrementally_) {
    StartCompaction(NON_INCREMENTAL_COMPACTION);
  }

  PagedSpaces spaces(heap());
  for (PagedSpace* space = spaces.next(); space != NULL;
       space = spaces.next()) {
    space->PrepareForMarkCompact();
  }

#ifdef VERIFY_HEAP
  if (!was_marked_incrementally_ && FLAG_verify_heap) {
    VerifyMarkbitsAreClean();
  }
#endif
}


void MarkCompactCollector::Finish() {
#ifdef DEBUG
  DCHECK(state_ == SWEEP_SPACES || state_ == RELOCATE_OBJECTS);
  state_ = IDLE;
#endif
  // The stub cache is not traversed during GC; clear the cache to
  // force lazy re-initialization of it. This must be done after the
  // GC, because it relies on the new address of certain old space
  // objects (empty string, illegal builtin).
  isolate()->stub_cache()->Clear();

  if (have_code_to_deoptimize_) {
    // Some code objects were marked for deoptimization during the GC.
    Deoptimizer::DeoptimizeMarkedCode(isolate());
    have_code_to_deoptimize_ = false;
  }

  heap_->incremental_marking()->ClearIdleMarkingDelayCounter();
}


// -------------------------------------------------------------------------
// Phase 1: tracing and marking live objects.
//   before: all objects are in normal state.
//   after: a live object's map pointer is marked as '00'.

// Marking all live objects in the heap as part of mark-sweep or mark-compact
// collection.  Before marking, all objects are in their normal state.  After
// marking, live objects' map pointers are marked indicating that the object
// has been found reachable.
//
// The marking algorithm is a (mostly) depth-first (because of possible stack
// overflow) traversal of the graph of objects reachable from the roots.  It
// uses an explicit stack of pointers rather than recursion.  The young
// generation's inactive ('from') space is used as a marking stack.  The
// objects in the marking stack are the ones that have been reached and marked
// but their children have not yet been visited.
//
// The marking stack can overflow during traversal.  In that case, we set an
// overflow flag.  When the overflow flag is set, we continue marking objects
// reachable from the objects on the marking stack, but no longer push them on
// the marking stack.  Instead, we mark them as both marked and overflowed.
// When the stack is in the overflowed state, objects marked as overflowed
// have been reached and marked but their children have not been visited yet.
// After emptying the marking stack, we clear the overflow flag and traverse
// the heap looking for objects marked as overflowed, push them on the stack,
// and continue with marking.  This process repeats until all reachable
// objects have been marked.

void CodeFlusher::ProcessJSFunctionCandidates() {
  Code* lazy_compile = isolate_->builtins()->builtin(Builtins::kCompileLazy);
  Object* undefined = isolate_->heap()->undefined_value();

  JSFunction* candidate = jsfunction_candidates_head_;
  JSFunction* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    ClearNextCandidate(candidate, undefined);

    SharedFunctionInfo* shared = candidate->shared();

    Code* code = shared->code();
    MarkBit code_mark = Marking::MarkBitFrom(code);
    if (Marking::IsWhite(code_mark)) {
      if (FLAG_trace_code_flushing && shared->is_compiled()) {
        PrintF("[code-flushing clears: ");
        shared->ShortPrint();
        PrintF(" - age: %d]\n", code->GetAge());
      }
      // Always flush the optimized code map if there is one.
      if (!shared->optimized_code_map()->IsSmi()) {
        shared->ClearOptimizedCodeMap();
      }
      shared->set_code(lazy_compile);
      candidate->set_code(lazy_compile);
    } else {
      DCHECK(Marking::IsBlack(code_mark));
      candidate->set_code(code);
    }

    // We are in the middle of a GC cycle so the write barrier in the code
    // setter did not record the slot update and we have to do that manually.
    Address slot = candidate->address() + JSFunction::kCodeEntryOffset;
    Code* target = Code::cast(Code::GetObjectFromEntryAddress(slot));
    isolate_->heap()->mark_compact_collector()->RecordCodeEntrySlot(slot,
                                                                    target);

    Object** shared_code_slot =
        HeapObject::RawField(shared, SharedFunctionInfo::kCodeOffset);
    isolate_->heap()->mark_compact_collector()->RecordSlot(
        shared_code_slot, shared_code_slot, *shared_code_slot);

    candidate = next_candidate;
  }

  jsfunction_candidates_head_ = NULL;
}


void CodeFlusher::ProcessSharedFunctionInfoCandidates() {
  Code* lazy_compile = isolate_->builtins()->builtin(Builtins::kCompileLazy);

  SharedFunctionInfo* candidate = shared_function_info_candidates_head_;
  SharedFunctionInfo* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    ClearNextCandidate(candidate);

    Code* code = candidate->code();
    MarkBit code_mark = Marking::MarkBitFrom(code);
    if (Marking::IsWhite(code_mark)) {
      if (FLAG_trace_code_flushing && candidate->is_compiled()) {
        PrintF("[code-flushing clears: ");
        candidate->ShortPrint();
        PrintF(" - age: %d]\n", code->GetAge());
      }
      // Always flush the optimized code map if there is one.
      if (!candidate->optimized_code_map()->IsSmi()) {
        candidate->ClearOptimizedCodeMap();
      }
      candidate->set_code(lazy_compile);
    }

    Object** code_slot =
        HeapObject::RawField(candidate, SharedFunctionInfo::kCodeOffset);
    isolate_->heap()->mark_compact_collector()->RecordSlot(code_slot, code_slot,
                                                           *code_slot);

    candidate = next_candidate;
  }

  shared_function_info_candidates_head_ = NULL;
}


void CodeFlusher::ProcessOptimizedCodeMaps() {
  STATIC_ASSERT(SharedFunctionInfo::kEntryLength == 4);

  SharedFunctionInfo* holder = optimized_code_map_holder_head_;
  SharedFunctionInfo* next_holder;

  while (holder != NULL) {
    next_holder = GetNextCodeMap(holder);
    ClearNextCodeMap(holder);

    // Process context-dependent entries in the optimized code map.
    FixedArray* code_map = FixedArray::cast(holder->optimized_code_map());
    int new_length = SharedFunctionInfo::kEntriesStart;
    int old_length = code_map->length();
    for (int i = SharedFunctionInfo::kEntriesStart; i < old_length;
         i += SharedFunctionInfo::kEntryLength) {
      // Each entry contains [ context, code, literals, ast-id ] as fields.
      STATIC_ASSERT(SharedFunctionInfo::kEntryLength == 4);
      Context* context =
          Context::cast(code_map->get(i + SharedFunctionInfo::kContextOffset));
      Code* code =
          Code::cast(code_map->get(i + SharedFunctionInfo::kCachedCodeOffset));
      FixedArray* literals = FixedArray::cast(
          code_map->get(i + SharedFunctionInfo::kLiteralsOffset));
      Smi* ast_id =
          Smi::cast(code_map->get(i + SharedFunctionInfo::kOsrAstIdOffset));
      if (Marking::IsWhite(Marking::MarkBitFrom(context))) continue;
      DCHECK(Marking::IsBlack(Marking::MarkBitFrom(context)));
      if (Marking::IsWhite(Marking::MarkBitFrom(code))) continue;
      DCHECK(Marking::IsBlack(Marking::MarkBitFrom(code)));
      if (Marking::IsWhite(Marking::MarkBitFrom(literals))) continue;
      DCHECK(Marking::IsBlack(Marking::MarkBitFrom(literals)));
      // Move every slot in the entry and record slots when needed.
      code_map->set(new_length + SharedFunctionInfo::kCachedCodeOffset, code);
      code_map->set(new_length + SharedFunctionInfo::kContextOffset, context);
      code_map->set(new_length + SharedFunctionInfo::kLiteralsOffset, literals);
      code_map->set(new_length + SharedFunctionInfo::kOsrAstIdOffset, ast_id);
      Object** code_slot = code_map->RawFieldOfElementAt(
          new_length + SharedFunctionInfo::kCachedCodeOffset);
      isolate_->heap()->mark_compact_collector()->RecordSlot(
          code_slot, code_slot, *code_slot);
      Object** context_slot = code_map->RawFieldOfElementAt(
          new_length + SharedFunctionInfo::kContextOffset);
      isolate_->heap()->mark_compact_collector()->RecordSlot(
          context_slot, context_slot, *context_slot);
      Object** literals_slot = code_map->RawFieldOfElementAt(
          new_length + SharedFunctionInfo::kLiteralsOffset);
      isolate_->heap()->mark_compact_collector()->RecordSlot(
          literals_slot, literals_slot, *literals_slot);
      new_length += SharedFunctionInfo::kEntryLength;
    }

    // Process context-independent entry in the optimized code map.
    Object* shared_object = code_map->get(SharedFunctionInfo::kSharedCodeIndex);
    if (shared_object->IsCode()) {
      Code* shared_code = Code::cast(shared_object);
      if (Marking::IsWhite(Marking::MarkBitFrom(shared_code))) {
        code_map->set_undefined(SharedFunctionInfo::kSharedCodeIndex);
      } else {
        DCHECK(Marking::IsBlack(Marking::MarkBitFrom(shared_code)));
        Object** slot =
            code_map->RawFieldOfElementAt(SharedFunctionInfo::kSharedCodeIndex);
        isolate_->heap()->mark_compact_collector()->RecordSlot(slot, slot,
                                                               *slot);
      }
    }

    // Trim the optimized code map if entries have been removed.
    if (new_length < old_length) {
      holder->TrimOptimizedCodeMap(old_length - new_length);
    }

    holder = next_holder;
  }

  optimized_code_map_holder_head_ = NULL;
}


void CodeFlusher::EvictCandidate(SharedFunctionInfo* shared_info) {
  // Make sure previous flushing decisions are revisited.
  isolate_->heap()->incremental_marking()->RecordWrites(shared_info);

  if (FLAG_trace_code_flushing) {
    PrintF("[code-flushing abandons function-info: ");
    shared_info->ShortPrint();
    PrintF("]\n");
  }

  SharedFunctionInfo* candidate = shared_function_info_candidates_head_;
  SharedFunctionInfo* next_candidate;
  if (candidate == shared_info) {
    next_candidate = GetNextCandidate(shared_info);
    shared_function_info_candidates_head_ = next_candidate;
    ClearNextCandidate(shared_info);
  } else {
    while (candidate != NULL) {
      next_candidate = GetNextCandidate(candidate);

      if (next_candidate == shared_info) {
        next_candidate = GetNextCandidate(shared_info);
        SetNextCandidate(candidate, next_candidate);
        ClearNextCandidate(shared_info);
        break;
      }

      candidate = next_candidate;
    }
  }
}


void CodeFlusher::EvictCandidate(JSFunction* function) {
  DCHECK(!function->next_function_link()->IsUndefined());
  Object* undefined = isolate_->heap()->undefined_value();

  // Make sure previous flushing decisions are revisited.
  isolate_->heap()->incremental_marking()->RecordWrites(function);
  isolate_->heap()->incremental_marking()->RecordWrites(function->shared());

  if (FLAG_trace_code_flushing) {
    PrintF("[code-flushing abandons closure: ");
    function->shared()->ShortPrint();
    PrintF("]\n");
  }

  JSFunction* candidate = jsfunction_candidates_head_;
  JSFunction* next_candidate;
  if (candidate == function) {
    next_candidate = GetNextCandidate(function);
    jsfunction_candidates_head_ = next_candidate;
    ClearNextCandidate(function, undefined);
  } else {
    while (candidate != NULL) {
      next_candidate = GetNextCandidate(candidate);

      if (next_candidate == function) {
        next_candidate = GetNextCandidate(function);
        SetNextCandidate(candidate, next_candidate);
        ClearNextCandidate(function, undefined);
        break;
      }

      candidate = next_candidate;
    }
  }
}


void CodeFlusher::EvictOptimizedCodeMap(SharedFunctionInfo* code_map_holder) {
  FixedArray* code_map =
      FixedArray::cast(code_map_holder->optimized_code_map());
  DCHECK(!code_map->get(SharedFunctionInfo::kNextMapIndex)->IsUndefined());

  // Make sure previous flushing decisions are revisited.
  isolate_->heap()->incremental_marking()->RecordWrites(code_map);
  isolate_->heap()->incremental_marking()->RecordWrites(code_map_holder);

  if (FLAG_trace_code_flushing) {
    PrintF("[code-flushing abandons code-map: ");
    code_map_holder->ShortPrint();
    PrintF("]\n");
  }

  SharedFunctionInfo* holder = optimized_code_map_holder_head_;
  SharedFunctionInfo* next_holder;
  if (holder == code_map_holder) {
    next_holder = GetNextCodeMap(code_map_holder);
    optimized_code_map_holder_head_ = next_holder;
    ClearNextCodeMap(code_map_holder);
  } else {
    while (holder != NULL) {
      next_holder = GetNextCodeMap(holder);

      if (next_holder == code_map_holder) {
        next_holder = GetNextCodeMap(code_map_holder);
        SetNextCodeMap(holder, next_holder);
        ClearNextCodeMap(code_map_holder);
        break;
      }

      holder = next_holder;
    }
  }
}


void CodeFlusher::EvictJSFunctionCandidates() {
  JSFunction* candidate = jsfunction_candidates_head_;
  JSFunction* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    EvictCandidate(candidate);
    candidate = next_candidate;
  }
  DCHECK(jsfunction_candidates_head_ == NULL);
}


void CodeFlusher::EvictSharedFunctionInfoCandidates() {
  SharedFunctionInfo* candidate = shared_function_info_candidates_head_;
  SharedFunctionInfo* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    EvictCandidate(candidate);
    candidate = next_candidate;
  }
  DCHECK(shared_function_info_candidates_head_ == NULL);
}


void CodeFlusher::EvictOptimizedCodeMaps() {
  SharedFunctionInfo* holder = optimized_code_map_holder_head_;
  SharedFunctionInfo* next_holder;
  while (holder != NULL) {
    next_holder = GetNextCodeMap(holder);
    EvictOptimizedCodeMap(holder);
    holder = next_holder;
  }
  DCHECK(optimized_code_map_holder_head_ == NULL);
}


void CodeFlusher::IteratePointersToFromSpace(ObjectVisitor* v) {
  Heap* heap = isolate_->heap();

  JSFunction** slot = &jsfunction_candidates_head_;
  JSFunction* candidate = jsfunction_candidates_head_;
  while (candidate != NULL) {
    if (heap->InFromSpace(candidate)) {
      v->VisitPointer(reinterpret_cast<Object**>(slot));
    }
    candidate = GetNextCandidate(*slot);
    slot = GetNextCandidateSlot(*slot);
  }
}


MarkCompactCollector::~MarkCompactCollector() {
  if (code_flusher_ != NULL) {
    delete code_flusher_;
    code_flusher_ = NULL;
  }
}


static inline HeapObject* ShortCircuitConsString(Object** p) {
  // Optimization: If the heap object pointed to by p is a non-internalized
  // cons string whose right substring is HEAP->empty_string, update
  // it in place to its left substring.  Return the updated value.
  //
  // Here we assume that if we change *p, we replace it with a heap object
  // (i.e., the left substring of a cons string is always a heap object).
  //
  // The check performed is:
  //   object->IsConsString() && !object->IsInternalizedString() &&
  //   (ConsString::cast(object)->second() == HEAP->empty_string())
  // except the maps for the object and its possible substrings might be
  // marked.
  HeapObject* object = HeapObject::cast(*p);
  Map* map = object->map();
  InstanceType type = map->instance_type();
  if (!IsShortcutCandidate(type)) return object;

  Object* second = reinterpret_cast<ConsString*>(object)->second();
  Heap* heap = map->GetHeap();
  if (second != heap->empty_string()) {
    return object;
  }

  // Since we don't have the object's start, it is impossible to update the
  // page dirty marks. Therefore, we only replace the string with its left
  // substring when page dirty marks do not change.
  Object* first = reinterpret_cast<ConsString*>(object)->first();
  if (!heap->InNewSpace(object) && heap->InNewSpace(first)) return object;

  *p = first;
  return HeapObject::cast(first);
}


class MarkCompactMarkingVisitor
    : public StaticMarkingVisitor<MarkCompactMarkingVisitor> {
 public:
  static void ObjectStatsVisitBase(StaticVisitorBase::VisitorId id, Map* map,
                                   HeapObject* obj);

  static void ObjectStatsCountFixedArray(
      FixedArrayBase* fixed_array, FixedArraySubInstanceType fast_type,
      FixedArraySubInstanceType dictionary_type);

  template <MarkCompactMarkingVisitor::VisitorId id>
  class ObjectStatsTracker {
   public:
    static inline void Visit(Map* map, HeapObject* obj);
  };

  static void Initialize();

  INLINE(static void VisitPointer(Heap* heap, Object** p)) {
    MarkObjectByPointer(heap->mark_compact_collector(), p, p);
  }

  INLINE(static void VisitPointers(Heap* heap, Object** start, Object** end)) {
    // Mark all objects pointed to in [start, end).
    const int kMinRangeForMarkingRecursion = 64;
    if (end - start >= kMinRangeForMarkingRecursion) {
      if (VisitUnmarkedObjects(heap, start, end)) return;
      // We are close to a stack overflow, so just mark the objects.
    }
    MarkCompactCollector* collector = heap->mark_compact_collector();
    for (Object** p = start; p < end; p++) {
      MarkObjectByPointer(collector, start, p);
    }
  }

  // Marks the object black and pushes it on the marking stack.
  INLINE(static void MarkObject(Heap* heap, HeapObject* object)) {
    MarkBit mark = Marking::MarkBitFrom(object);
    heap->mark_compact_collector()->MarkObject(object, mark);
  }

  // Marks the object black without pushing it on the marking stack.
  // Returns true if object needed marking and false otherwise.
  INLINE(static bool MarkObjectWithoutPush(Heap* heap, HeapObject* object)) {
    MarkBit mark_bit = Marking::MarkBitFrom(object);
    if (Marking::IsWhite(mark_bit)) {
      heap->mark_compact_collector()->SetMark(object, mark_bit);
      return true;
    }
    return false;
  }

  // Mark object pointed to by p.
  INLINE(static void MarkObjectByPointer(MarkCompactCollector* collector,
                                         Object** anchor_slot, Object** p)) {
    if (!(*p)->IsHeapObject()) return;
    HeapObject* object = ShortCircuitConsString(p);
    collector->RecordSlot(anchor_slot, p, object);
    MarkBit mark = Marking::MarkBitFrom(object);
    collector->MarkObject(object, mark);
  }


  // Visit an unmarked object.
  INLINE(static void VisitUnmarkedObject(MarkCompactCollector* collector,
                                         HeapObject* obj)) {
#ifdef DEBUG
    DCHECK(collector->heap()->Contains(obj));
    DCHECK(!collector->heap()->mark_compact_collector()->IsMarked(obj));
#endif
    Map* map = obj->map();
    Heap* heap = obj->GetHeap();
    MarkBit mark = Marking::MarkBitFrom(obj);
    heap->mark_compact_collector()->SetMark(obj, mark);
    // Mark the map pointer and the body.
    MarkBit map_mark = Marking::MarkBitFrom(map);
    heap->mark_compact_collector()->MarkObject(map, map_mark);
    IterateBody(map, obj);
  }

  // Visit all unmarked objects pointed to by [start, end).
  // Returns false if the operation fails (lack of stack space).
  INLINE(static bool VisitUnmarkedObjects(Heap* heap, Object** start,
                                          Object** end)) {
    // Return false is we are close to the stack limit.
    StackLimitCheck check(heap->isolate());
    if (check.HasOverflowed()) return false;

    MarkCompactCollector* collector = heap->mark_compact_collector();
    // Visit the unmarked objects.
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (!o->IsHeapObject()) continue;
      collector->RecordSlot(start, p, o);
      HeapObject* obj = HeapObject::cast(o);
      MarkBit mark = Marking::MarkBitFrom(obj);
      if (Marking::IsBlackOrGrey(mark)) continue;
      VisitUnmarkedObject(collector, obj);
    }
    return true;
  }

 private:
  template <int id>
  static inline void TrackObjectStatsAndVisit(Map* map, HeapObject* obj);

  // Code flushing support.

  static const int kRegExpCodeThreshold = 5;

  static void UpdateRegExpCodeAgeAndFlush(Heap* heap, JSRegExp* re,
                                          bool is_one_byte) {
    // Make sure that the fixed array is in fact initialized on the RegExp.
    // We could potentially trigger a GC when initializing the RegExp.
    if (HeapObject::cast(re->data())->map()->instance_type() !=
        FIXED_ARRAY_TYPE)
      return;

    // Make sure this is a RegExp that actually contains code.
    if (re->TypeTag() != JSRegExp::IRREGEXP) return;

    Object* code = re->DataAt(JSRegExp::code_index(is_one_byte));
    if (!code->IsSmi() &&
        HeapObject::cast(code)->map()->instance_type() == CODE_TYPE) {
      // Save a copy that can be reinstated if we need the code again.
      re->SetDataAt(JSRegExp::saved_code_index(is_one_byte), code);

      // Saving a copy might create a pointer into compaction candidate
      // that was not observed by marker.  This might happen if JSRegExp data
      // was marked through the compilation cache before marker reached JSRegExp
      // object.
      FixedArray* data = FixedArray::cast(re->data());
      Object** slot =
          data->data_start() + JSRegExp::saved_code_index(is_one_byte);
      heap->mark_compact_collector()->RecordSlot(slot, slot, code);

      // Set a number in the 0-255 range to guarantee no smi overflow.
      re->SetDataAt(JSRegExp::code_index(is_one_byte),
                    Smi::FromInt(heap->sweep_generation() & 0xff));
    } else if (code->IsSmi()) {
      int value = Smi::cast(code)->value();
      // The regexp has not been compiled yet or there was a compilation error.
      if (value == JSRegExp::kUninitializedValue ||
          value == JSRegExp::kCompilationErrorValue) {
        return;
      }

      // Check if we should flush now.
      if (value == ((heap->sweep_generation() - kRegExpCodeThreshold) & 0xff)) {
        re->SetDataAt(JSRegExp::code_index(is_one_byte),
                      Smi::FromInt(JSRegExp::kUninitializedValue));
        re->SetDataAt(JSRegExp::saved_code_index(is_one_byte),
                      Smi::FromInt(JSRegExp::kUninitializedValue));
      }
    }
  }


  // Works by setting the current sweep_generation (as a smi) in the
  // code object place in the data array of the RegExp and keeps a copy
  // around that can be reinstated if we reuse the RegExp before flushing.
  // If we did not use the code for kRegExpCodeThreshold mark sweep GCs
  // we flush the code.
  static void VisitRegExpAndFlushCode(Map* map, HeapObject* object) {
    Heap* heap = map->GetHeap();
    MarkCompactCollector* collector = heap->mark_compact_collector();
    if (!collector->is_code_flushing_enabled()) {
      VisitJSRegExp(map, object);
      return;
    }
    JSRegExp* re = reinterpret_cast<JSRegExp*>(object);
    // Flush code or set age on both one byte and two byte code.
    UpdateRegExpCodeAgeAndFlush(heap, re, true);
    UpdateRegExpCodeAgeAndFlush(heap, re, false);
    // Visit the fields of the RegExp, including the updated FixedArray.
    VisitJSRegExp(map, object);
  }

  static VisitorDispatchTable<Callback> non_count_table_;
};


void MarkCompactMarkingVisitor::ObjectStatsCountFixedArray(
    FixedArrayBase* fixed_array, FixedArraySubInstanceType fast_type,
    FixedArraySubInstanceType dictionary_type) {
  Heap* heap = fixed_array->map()->GetHeap();
  if (fixed_array->map() != heap->fixed_cow_array_map() &&
      fixed_array->map() != heap->fixed_double_array_map() &&
      fixed_array != heap->empty_fixed_array()) {
    if (fixed_array->IsDictionary()) {
      heap->RecordFixedArraySubTypeStats(dictionary_type, fixed_array->Size());
    } else {
      heap->RecordFixedArraySubTypeStats(fast_type, fixed_array->Size());
    }
  }
}


void MarkCompactMarkingVisitor::ObjectStatsVisitBase(
    MarkCompactMarkingVisitor::VisitorId id, Map* map, HeapObject* obj) {
  Heap* heap = map->GetHeap();
  int object_size = obj->Size();
  heap->RecordObjectStats(map->instance_type(), object_size);
  non_count_table_.GetVisitorById(id)(map, obj);
  if (obj->IsJSObject()) {
    JSObject* object = JSObject::cast(obj);
    ObjectStatsCountFixedArray(object->elements(), DICTIONARY_ELEMENTS_SUB_TYPE,
                               FAST_ELEMENTS_SUB_TYPE);
    ObjectStatsCountFixedArray(object->properties(),
                               DICTIONARY_PROPERTIES_SUB_TYPE,
                               FAST_PROPERTIES_SUB_TYPE);
  }
}


template <MarkCompactMarkingVisitor::VisitorId id>
void MarkCompactMarkingVisitor::ObjectStatsTracker<id>::Visit(Map* map,
                                                              HeapObject* obj) {
  ObjectStatsVisitBase(id, map, obj);
}


template <>
class MarkCompactMarkingVisitor::ObjectStatsTracker<
    MarkCompactMarkingVisitor::kVisitMap> {
 public:
  static inline void Visit(Map* map, HeapObject* obj) {
    Heap* heap = map->GetHeap();
    Map* map_obj = Map::cast(obj);
    DCHECK(map->instance_type() == MAP_TYPE);
    DescriptorArray* array = map_obj->instance_descriptors();
    if (map_obj->owns_descriptors() &&
        array != heap->empty_descriptor_array()) {
      int fixed_array_size = array->Size();
      heap->RecordFixedArraySubTypeStats(DESCRIPTOR_ARRAY_SUB_TYPE,
                                         fixed_array_size);
    }
    if (TransitionArray::IsFullTransitionArray(map_obj->raw_transitions())) {
      int fixed_array_size =
          TransitionArray::cast(map_obj->raw_transitions())->Size();
      heap->RecordFixedArraySubTypeStats(TRANSITION_ARRAY_SUB_TYPE,
                                         fixed_array_size);
    }
    if (map_obj->has_code_cache()) {
      CodeCache* cache = CodeCache::cast(map_obj->code_cache());
      heap->RecordFixedArraySubTypeStats(MAP_CODE_CACHE_SUB_TYPE,
                                         cache->default_cache()->Size());
      if (!cache->normal_type_cache()->IsUndefined()) {
        heap->RecordFixedArraySubTypeStats(
            MAP_CODE_CACHE_SUB_TYPE,
            FixedArray::cast(cache->normal_type_cache())->Size());
      }
    }
    ObjectStatsVisitBase(kVisitMap, map, obj);
  }
};


template <>
class MarkCompactMarkingVisitor::ObjectStatsTracker<
    MarkCompactMarkingVisitor::kVisitCode> {
 public:
  static inline void Visit(Map* map, HeapObject* obj) {
    Heap* heap = map->GetHeap();
    int object_size = obj->Size();
    DCHECK(map->instance_type() == CODE_TYPE);
    Code* code_obj = Code::cast(obj);
    heap->RecordCodeSubTypeStats(code_obj->kind(), code_obj->GetAge(),
                                 object_size);
    ObjectStatsVisitBase(kVisitCode, map, obj);
  }
};


template <>
class MarkCompactMarkingVisitor::ObjectStatsTracker<
    MarkCompactMarkingVisitor::kVisitSharedFunctionInfo> {
 public:
  static inline void Visit(Map* map, HeapObject* obj) {
    Heap* heap = map->GetHeap();
    SharedFunctionInfo* sfi = SharedFunctionInfo::cast(obj);
    if (sfi->scope_info() != heap->empty_fixed_array()) {
      heap->RecordFixedArraySubTypeStats(
          SCOPE_INFO_SUB_TYPE, FixedArray::cast(sfi->scope_info())->Size());
    }
    ObjectStatsVisitBase(kVisitSharedFunctionInfo, map, obj);
  }
};


template <>
class MarkCompactMarkingVisitor::ObjectStatsTracker<
    MarkCompactMarkingVisitor::kVisitFixedArray> {
 public:
  static inline void Visit(Map* map, HeapObject* obj) {
    Heap* heap = map->GetHeap();
    FixedArray* fixed_array = FixedArray::cast(obj);
    if (fixed_array == heap->string_table()) {
      heap->RecordFixedArraySubTypeStats(STRING_TABLE_SUB_TYPE,
                                         fixed_array->Size());
    }
    ObjectStatsVisitBase(kVisitFixedArray, map, obj);
  }
};


void MarkCompactMarkingVisitor::Initialize() {
  StaticMarkingVisitor<MarkCompactMarkingVisitor>::Initialize();

  table_.Register(kVisitJSRegExp, &VisitRegExpAndFlushCode);

  if (FLAG_track_gc_object_stats) {
    // Copy the visitor table to make call-through possible.
    non_count_table_.CopyFrom(&table_);
#define VISITOR_ID_COUNT_FUNCTION(id) \
  table_.Register(kVisit##id, ObjectStatsTracker<kVisit##id>::Visit);
    VISITOR_ID_LIST(VISITOR_ID_COUNT_FUNCTION)
#undef VISITOR_ID_COUNT_FUNCTION
  }
}


VisitorDispatchTable<MarkCompactMarkingVisitor::Callback>
    MarkCompactMarkingVisitor::non_count_table_;


class CodeMarkingVisitor : public ThreadVisitor {
 public:
  explicit CodeMarkingVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    collector_->PrepareThreadForCodeFlushing(isolate, top);
  }

 private:
  MarkCompactCollector* collector_;
};


class SharedFunctionInfoMarkingVisitor : public ObjectVisitor {
 public:
  explicit SharedFunctionInfoMarkingVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) VisitPointer(p);
  }

  void VisitPointer(Object** slot) {
    Object* obj = *slot;
    if (obj->IsSharedFunctionInfo()) {
      SharedFunctionInfo* shared = reinterpret_cast<SharedFunctionInfo*>(obj);
      MarkBit shared_mark = Marking::MarkBitFrom(shared);
      MarkBit code_mark = Marking::MarkBitFrom(shared->code());
      collector_->MarkObject(shared->code(), code_mark);
      collector_->MarkObject(shared, shared_mark);
    }
  }

 private:
  MarkCompactCollector* collector_;
};


void MarkCompactCollector::PrepareThreadForCodeFlushing(Isolate* isolate,
                                                        ThreadLocalTop* top) {
  for (StackFrameIterator it(isolate, top); !it.done(); it.Advance()) {
    // Note: for the frame that has a pending lazy deoptimization
    // StackFrame::unchecked_code will return a non-optimized code object for
    // the outermost function and StackFrame::LookupCode will return
    // actual optimized code object.
    StackFrame* frame = it.frame();
    Code* code = frame->unchecked_code();
    MarkBit code_mark = Marking::MarkBitFrom(code);
    MarkObject(code, code_mark);
    if (frame->is_optimized()) {
      MarkCompactMarkingVisitor::MarkInlinedFunctionsCode(heap(),
                                                          frame->LookupCode());
    }
  }
}


void MarkCompactCollector::PrepareForCodeFlushing() {
  // If code flushing is disabled, there is no need to prepare for it.
  if (!is_code_flushing_enabled()) return;

  // Ensure that empty descriptor array is marked. Method MarkDescriptorArray
  // relies on it being marked before any other descriptor array.
  HeapObject* descriptor_array = heap()->empty_descriptor_array();
  MarkBit descriptor_array_mark = Marking::MarkBitFrom(descriptor_array);
  MarkObject(descriptor_array, descriptor_array_mark);

  // Make sure we are not referencing the code from the stack.
  DCHECK(this == heap()->mark_compact_collector());
  PrepareThreadForCodeFlushing(heap()->isolate(),
                               heap()->isolate()->thread_local_top());

  // Iterate the archived stacks in all threads to check if
  // the code is referenced.
  CodeMarkingVisitor code_marking_visitor(this);
  heap()->isolate()->thread_manager()->IterateArchivedThreads(
      &code_marking_visitor);

  SharedFunctionInfoMarkingVisitor visitor(this);
  heap()->isolate()->compilation_cache()->IterateFunctions(&visitor);
  heap()->isolate()->handle_scope_implementer()->Iterate(&visitor);

  ProcessMarkingDeque();
}


// Visitor class for marking heap roots.
class RootMarkingVisitor : public ObjectVisitor {
 public:
  explicit RootMarkingVisitor(Heap* heap)
      : collector_(heap->mark_compact_collector()) {}

  void VisitPointer(Object** p) { MarkObjectByPointer(p); }

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

  // Skip the weak next code link in a code object, which is visited in
  // ProcessTopOptimizedFrame.
  void VisitNextCodeLink(Object** p) {}

 private:
  void MarkObjectByPointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    // Replace flat cons strings in place.
    HeapObject* object = ShortCircuitConsString(p);
    MarkBit mark_bit = Marking::MarkBitFrom(object);
    if (Marking::IsBlackOrGrey(mark_bit)) return;

    Map* map = object->map();
    // Mark the object.
    collector_->SetMark(object, mark_bit);

    // Mark the map pointer and body, and push them on the marking stack.
    MarkBit map_mark = Marking::MarkBitFrom(map);
    collector_->MarkObject(map, map_mark);
    MarkCompactMarkingVisitor::IterateBody(map, object);

    // Mark all the objects reachable from the map and body.  May leave
    // overflowed objects in the heap.
    collector_->EmptyMarkingDeque();
  }

  MarkCompactCollector* collector_;
};


// Helper class for pruning the string table.
template <bool finalize_external_strings>
class StringTableCleaner : public ObjectVisitor {
 public:
  explicit StringTableCleaner(Heap* heap) : heap_(heap), pointers_removed_(0) {}

  virtual void VisitPointers(Object** start, Object** end) {
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (o->IsHeapObject() &&
          Marking::IsWhite(Marking::MarkBitFrom(HeapObject::cast(o)))) {
        if (finalize_external_strings) {
          DCHECK(o->IsExternalString());
          heap_->FinalizeExternalString(String::cast(*p));
        } else {
          pointers_removed_++;
        }
        // Set the entry to the_hole_value (as deleted).
        *p = heap_->the_hole_value();
      }
    }
  }

  int PointersRemoved() {
    DCHECK(!finalize_external_strings);
    return pointers_removed_;
  }

 private:
  Heap* heap_;
  int pointers_removed_;
};


typedef StringTableCleaner<false> InternalizedStringTableCleaner;
typedef StringTableCleaner<true> ExternalStringTableCleaner;


// Implementation of WeakObjectRetainer for mark compact GCs. All marked objects
// are retained.
class MarkCompactWeakObjectRetainer : public WeakObjectRetainer {
 public:
  virtual Object* RetainAs(Object* object) {
    if (Marking::IsBlackOrGrey(
            Marking::MarkBitFrom(HeapObject::cast(object)))) {
      return object;
    } else if (object->IsAllocationSite() &&
               !(AllocationSite::cast(object)->IsZombie())) {
      // "dead" AllocationSites need to live long enough for a traversal of new
      // space. These sites get a one-time reprieve.
      AllocationSite* site = AllocationSite::cast(object);
      site->MarkZombie();
      site->GetHeap()->mark_compact_collector()->MarkAllocationSite(site);
      return object;
    } else {
      return NULL;
    }
  }
};


// Fill the marking stack with overflowed objects returned by the given
// iterator.  Stop when the marking stack is filled or the end of the space
// is reached, whichever comes first.
template <class T>
static void DiscoverGreyObjectsWithIterator(Heap* heap,
                                            MarkingDeque* marking_deque,
                                            T* it) {
  // The caller should ensure that the marking stack is initially not full,
  // so that we don't waste effort pointlessly scanning for objects.
  DCHECK(!marking_deque->IsFull());

  Map* filler_map = heap->one_pointer_filler_map();
  for (HeapObject* object = it->Next(); object != NULL; object = it->Next()) {
    MarkBit markbit = Marking::MarkBitFrom(object);
    if ((object->map() != filler_map) && Marking::IsGrey(markbit)) {
      Marking::GreyToBlack(markbit);
      MemoryChunk::IncrementLiveBytesFromGC(object->address(), object->Size());
      marking_deque->PushBlack(object);
      if (marking_deque->IsFull()) return;
    }
  }
}


static inline int MarkWordToObjectStarts(uint32_t mark_bits, int* starts);


static void DiscoverGreyObjectsOnPage(MarkingDeque* marking_deque,
                                      MemoryChunk* p) {
  DCHECK(!marking_deque->IsFull());
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  for (MarkBitCellIterator it(p); !it.Done(); it.Advance()) {
    Address cell_base = it.CurrentCellBase();
    MarkBit::CellType* cell = it.CurrentCell();

    const MarkBit::CellType current_cell = *cell;
    if (current_cell == 0) continue;

    MarkBit::CellType grey_objects;
    if (it.HasNext()) {
      const MarkBit::CellType next_cell = *(cell + 1);
      grey_objects = current_cell & ((current_cell >> 1) |
                                     (next_cell << (Bitmap::kBitsPerCell - 1)));
    } else {
      grey_objects = current_cell & (current_cell >> 1);
    }

    int offset = 0;
    while (grey_objects != 0) {
      int trailing_zeros = base::bits::CountTrailingZeros32(grey_objects);
      grey_objects >>= trailing_zeros;
      offset += trailing_zeros;
      MarkBit markbit(cell, 1 << offset);
      DCHECK(Marking::IsGrey(markbit));
      Marking::GreyToBlack(markbit);
      Address addr = cell_base + offset * kPointerSize;
      HeapObject* object = HeapObject::FromAddress(addr);
      MemoryChunk::IncrementLiveBytesFromGC(object->address(), object->Size());
      marking_deque->PushBlack(object);
      if (marking_deque->IsFull()) return;
      offset += 2;
      grey_objects >>= 2;
    }

    grey_objects >>= (Bitmap::kBitsPerCell - 1);
  }
}


int MarkCompactCollector::DiscoverAndEvacuateBlackObjectsOnPage(
    NewSpace* new_space, NewSpacePage* p) {
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  MarkBit::CellType* cells = p->markbits()->cells();
  int survivors_size = 0;

  for (MarkBitCellIterator it(p); !it.Done(); it.Advance()) {
    Address cell_base = it.CurrentCellBase();
    MarkBit::CellType* cell = it.CurrentCell();

    MarkBit::CellType current_cell = *cell;
    if (current_cell == 0) continue;

    int offset = 0;
    while (current_cell != 0) {
      int trailing_zeros = base::bits::CountTrailingZeros32(current_cell);
      current_cell >>= trailing_zeros;
      offset += trailing_zeros;
      Address address = cell_base + offset * kPointerSize;
      HeapObject* object = HeapObject::FromAddress(address);
      DCHECK(Marking::IsBlack(Marking::MarkBitFrom(object)));

      int size = object->Size();
      survivors_size += size;

      Heap::UpdateAllocationSiteFeedback(object, Heap::RECORD_SCRATCHPAD_SLOT);

      offset += 2;
      current_cell >>= 2;

      // TODO(hpayer): Refactor EvacuateObject and call this function instead.
      if (heap()->ShouldBePromoted(object->address(), size) &&
          TryPromoteObject(object, size)) {
        continue;
      }

      AllocationAlignment alignment = object->RequiredAlignment();
      AllocationResult allocation = new_space->AllocateRaw(size, alignment);
      if (allocation.IsRetry()) {
        if (!new_space->AddFreshPage()) {
          // Shouldn't happen. We are sweeping linearly, and to-space
          // has the same number of pages as from-space, so there is
          // always room.
          UNREACHABLE();
        }
        allocation = new_space->AllocateRaw(size, alignment);
        DCHECK(!allocation.IsRetry());
      }
      Object* target = allocation.ToObjectChecked();

      MigrateObject(HeapObject::cast(target), object, size, NEW_SPACE);
      heap()->IncrementSemiSpaceCopiedObjectSize(size);
    }
    *cells = 0;
  }
  return survivors_size;
}


static void DiscoverGreyObjectsInSpace(Heap* heap, MarkingDeque* marking_deque,
                                       PagedSpace* space) {
  PageIterator it(space);
  while (it.has_next()) {
    Page* p = it.next();
    DiscoverGreyObjectsOnPage(marking_deque, p);
    if (marking_deque->IsFull()) return;
  }
}


static void DiscoverGreyObjectsInNewSpace(Heap* heap,
                                          MarkingDeque* marking_deque) {
  NewSpace* space = heap->new_space();
  NewSpacePageIterator it(space->bottom(), space->top());
  while (it.has_next()) {
    NewSpacePage* page = it.next();
    DiscoverGreyObjectsOnPage(marking_deque, page);
    if (marking_deque->IsFull()) return;
  }
}


bool MarkCompactCollector::IsUnmarkedHeapObject(Object** p) {
  Object* o = *p;
  if (!o->IsHeapObject()) return false;
  HeapObject* heap_object = HeapObject::cast(o);
  MarkBit mark = Marking::MarkBitFrom(heap_object);
  return Marking::IsWhite(mark);
}


bool MarkCompactCollector::IsUnmarkedHeapObjectWithHeap(Heap* heap,
                                                        Object** p) {
  Object* o = *p;
  DCHECK(o->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(o);
  MarkBit mark = Marking::MarkBitFrom(heap_object);
  return Marking::IsWhite(mark);
}


void MarkCompactCollector::MarkStringTable(RootMarkingVisitor* visitor) {
  StringTable* string_table = heap()->string_table();
  // Mark the string table itself.
  MarkBit string_table_mark = Marking::MarkBitFrom(string_table);
  if (Marking::IsWhite(string_table_mark)) {
    // String table could have already been marked by visiting the handles list.
    SetMark(string_table, string_table_mark);
  }
  // Explicitly mark the prefix.
  string_table->IteratePrefix(visitor);
  ProcessMarkingDeque();
}


void MarkCompactCollector::MarkAllocationSite(AllocationSite* site) {
  MarkBit mark_bit = Marking::MarkBitFrom(site);
  SetMark(site, mark_bit);
}


void MarkCompactCollector::MarkRoots(RootMarkingVisitor* visitor) {
  // Mark the heap roots including global variables, stack variables,
  // etc., and all objects reachable from them.
  heap()->IterateStrongRoots(visitor, VISIT_ONLY_STRONG);

  // Handle the string table specially.
  MarkStringTable(visitor);

  // There may be overflowed objects in the heap.  Visit them now.
  while (marking_deque_.overflowed()) {
    RefillMarkingDeque();
    EmptyMarkingDeque();
  }
}


void MarkCompactCollector::MarkImplicitRefGroups(
    MarkObjectFunction mark_object) {
  List<ImplicitRefGroup*>* ref_groups =
      isolate()->global_handles()->implicit_ref_groups();

  int last = 0;
  for (int i = 0; i < ref_groups->length(); i++) {
    ImplicitRefGroup* entry = ref_groups->at(i);
    DCHECK(entry != NULL);

    if (!IsMarked(*entry->parent)) {
      (*ref_groups)[last++] = entry;
      continue;
    }

    Object*** children = entry->children;
    // A parent object is marked, so mark all child heap objects.
    for (size_t j = 0; j < entry->length; ++j) {
      if ((*children[j])->IsHeapObject()) {
        mark_object(heap(), HeapObject::cast(*children[j]));
      }
    }

    // Once the entire group has been marked, dispose it because it's
    // not needed anymore.
    delete entry;
  }
  ref_groups->Rewind(last);
}


// Mark all objects reachable from the objects on the marking stack.
// Before: the marking stack contains zero or more heap object pointers.
// After: the marking stack is empty, and all objects reachable from the
// marking stack have been marked, or are overflowed in the heap.
void MarkCompactCollector::EmptyMarkingDeque() {
  Map* filler_map = heap_->one_pointer_filler_map();
  while (!marking_deque_.IsEmpty()) {
    HeapObject* object = marking_deque_.Pop();
    // Explicitly skip one word fillers. Incremental markbit patterns are
    // correct only for objects that occupy at least two words.
    Map* map = object->map();
    if (map == filler_map) continue;

    DCHECK(object->IsHeapObject());
    DCHECK(heap()->Contains(object));
    DCHECK(!Marking::IsWhite(Marking::MarkBitFrom(object)));

    MarkBit map_mark = Marking::MarkBitFrom(map);
    MarkObject(map, map_mark);

    MarkCompactMarkingVisitor::IterateBody(map, object);
  }
}


// Sweep the heap for overflowed objects, clear their overflow bits, and
// push them on the marking stack.  Stop early if the marking stack fills
// before sweeping completes.  If sweeping completes, there are no remaining
// overflowed objects in the heap so the overflow flag on the markings stack
// is cleared.
void MarkCompactCollector::RefillMarkingDeque() {
  isolate()->CountUsage(v8::Isolate::UseCounterFeature::kMarkDequeOverflow);
  DCHECK(marking_deque_.overflowed());

  DiscoverGreyObjectsInNewSpace(heap(), &marking_deque_);
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap(), &marking_deque_, heap()->old_space());
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap(), &marking_deque_, heap()->code_space());
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap(), &marking_deque_, heap()->map_space());
  if (marking_deque_.IsFull()) return;

  LargeObjectIterator lo_it(heap()->lo_space());
  DiscoverGreyObjectsWithIterator(heap(), &marking_deque_, &lo_it);
  if (marking_deque_.IsFull()) return;

  marking_deque_.ClearOverflowed();
}


// Mark all objects reachable (transitively) from objects on the marking
// stack.  Before: the marking stack contains zero or more heap object
// pointers.  After: the marking stack is empty and there are no overflowed
// objects in the heap.
void MarkCompactCollector::ProcessMarkingDeque() {
  EmptyMarkingDeque();
  while (marking_deque_.overflowed()) {
    RefillMarkingDeque();
    EmptyMarkingDeque();
  }
}


// Mark all objects reachable (transitively) from objects on the marking
// stack including references only considered in the atomic marking pause.
void MarkCompactCollector::ProcessEphemeralMarking(
    ObjectVisitor* visitor, bool only_process_harmony_weak_collections) {
  bool work_to_do = true;
  DCHECK(marking_deque_.IsEmpty() && !marking_deque_.overflowed());
  while (work_to_do) {
    if (!only_process_harmony_weak_collections) {
      isolate()->global_handles()->IterateObjectGroups(
          visitor, &IsUnmarkedHeapObjectWithHeap);
      MarkImplicitRefGroups(&MarkCompactMarkingVisitor::MarkObject);
    }
    ProcessWeakCollections();
    work_to_do = !marking_deque_.IsEmpty();
    ProcessMarkingDeque();
  }
}


void MarkCompactCollector::ProcessTopOptimizedFrame(ObjectVisitor* visitor) {
  for (StackFrameIterator it(isolate(), isolate()->thread_local_top());
       !it.done(); it.Advance()) {
    if (it.frame()->type() == StackFrame::JAVA_SCRIPT) {
      return;
    }
    if (it.frame()->type() == StackFrame::OPTIMIZED) {
      Code* code = it.frame()->LookupCode();
      if (!code->CanDeoptAt(it.frame()->pc())) {
        code->CodeIterateBody(visitor);
      }
      ProcessMarkingDeque();
      return;
    }
  }
}


void MarkCompactCollector::RetainMaps() {
  if (reduce_memory_footprint_ || abort_incremental_marking_ ||
      FLAG_retain_maps_for_n_gc == 0) {
    // Do not retain dead maps if flag disables it or there is
    // - memory pressure (reduce_memory_footprint_),
    // - GC is requested by tests or dev-tools (abort_incremental_marking_).
    return;
  }

  ArrayList* retained_maps = heap()->retained_maps();
  int length = retained_maps->Length();
  int new_length = 0;
  for (int i = 0; i < length; i += 2) {
    DCHECK(retained_maps->Get(i)->IsWeakCell());
    WeakCell* cell = WeakCell::cast(retained_maps->Get(i));
    if (cell->cleared()) continue;
    int age = Smi::cast(retained_maps->Get(i + 1))->value();
    int new_age;
    Map* map = Map::cast(cell->value());
    MarkBit map_mark = Marking::MarkBitFrom(map);
    if (Marking::IsWhite(map_mark)) {
      if (age == 0) {
        // The map has aged. Do not retain this map.
        continue;
      }
      Object* constructor = map->GetConstructor();
      if (!constructor->IsHeapObject() || Marking::IsWhite(Marking::MarkBitFrom(
                                              HeapObject::cast(constructor)))) {
        // The constructor is dead, no new objects with this map can
        // be created. Do not retain this map.
        continue;
      }
      Object* prototype = map->prototype();
      if (prototype->IsHeapObject() &&
          Marking::IsWhite(Marking::MarkBitFrom(HeapObject::cast(prototype)))) {
        // The prototype is not marked, age the map.
        new_age = age - 1;
      } else {
        // The prototype and the constructor are marked, this map keeps only
        // transition tree alive, not JSObjects. Do not age the map.
        new_age = age;
      }
      MarkObject(map, map_mark);
    } else {
      new_age = FLAG_retain_maps_for_n_gc;
    }
    if (i != new_length) {
      retained_maps->Set(new_length, cell);
      Object** slot = retained_maps->Slot(new_length);
      RecordSlot(slot, slot, cell);
      retained_maps->Set(new_length + 1, Smi::FromInt(new_age));
    } else if (new_age != age) {
      retained_maps->Set(new_length + 1, Smi::FromInt(new_age));
    }
    new_length += 2;
  }
  Object* undefined = heap()->undefined_value();
  for (int i = new_length; i < length; i++) {
    retained_maps->Clear(i, undefined);
  }
  if (new_length != length) retained_maps->SetLength(new_length);
  ProcessMarkingDeque();
}


void MarkCompactCollector::EnsureMarkingDequeIsReserved() {
  DCHECK(!marking_deque_.in_use());
  if (marking_deque_memory_ == NULL) {
    marking_deque_memory_ = new base::VirtualMemory(kMaxMarkingDequeSize);
    marking_deque_memory_committed_ = 0;
  }
  if (marking_deque_memory_ == NULL) {
    V8::FatalProcessOutOfMemory("EnsureMarkingDequeIsReserved");
  }
}


void MarkCompactCollector::EnsureMarkingDequeIsCommitted(size_t max_size) {
  // If the marking deque is too small, we try to allocate a bigger one.
  // If that fails, make do with a smaller one.
  CHECK(!marking_deque_.in_use());
  for (size_t size = max_size; size >= kMinMarkingDequeSize; size >>= 1) {
    base::VirtualMemory* memory = marking_deque_memory_;
    size_t currently_committed = marking_deque_memory_committed_;

    if (currently_committed == size) return;

    if (currently_committed > size) {
      bool success = marking_deque_memory_->Uncommit(
          reinterpret_cast<Address>(marking_deque_memory_->address()) + size,
          currently_committed - size);
      if (success) {
        marking_deque_memory_committed_ = size;
        return;
      }
      UNREACHABLE();
    }

    bool success = memory->Commit(
        reinterpret_cast<Address>(memory->address()) + currently_committed,
        size - currently_committed,
        false);  // Not executable.
    if (success) {
      marking_deque_memory_committed_ = size;
      return;
    }
  }
  V8::FatalProcessOutOfMemory("EnsureMarkingDequeIsCommitted");
}


void MarkCompactCollector::InitializeMarkingDeque() {
  DCHECK(!marking_deque_.in_use());
  DCHECK(marking_deque_memory_committed_ > 0);
  Address addr = static_cast<Address>(marking_deque_memory_->address());
  size_t size = marking_deque_memory_committed_;
  if (FLAG_force_marking_deque_overflows) size = 64 * kPointerSize;
  marking_deque_.Initialize(addr, addr + size);
}


void MarkingDeque::Initialize(Address low, Address high) {
  DCHECK(!in_use_);
  HeapObject** obj_low = reinterpret_cast<HeapObject**>(low);
  HeapObject** obj_high = reinterpret_cast<HeapObject**>(high);
  array_ = obj_low;
  mask_ = base::bits::RoundDownToPowerOfTwo32(
              static_cast<uint32_t>(obj_high - obj_low)) -
          1;
  top_ = bottom_ = 0;
  overflowed_ = false;
  in_use_ = true;
}


void MarkingDeque::Uninitialize(bool aborting) {
  if (!aborting) {
    DCHECK(IsEmpty());
    DCHECK(!overflowed_);
  }
  DCHECK(in_use_);
  top_ = bottom_ = 0xdecbad;
  in_use_ = false;
}


void MarkCompactCollector::MarkLiveObjects() {
  GCTracer::Scope gc_scope(heap()->tracer(), GCTracer::Scope::MC_MARK);
  double start_time = 0.0;
  if (FLAG_print_cumulative_gc_stat) {
    start_time = base::OS::TimeCurrentMillis();
  }
  // The recursive GC marker detects when it is nearing stack overflow,
  // and switches to a different marking system.  JS interrupts interfere
  // with the C stack limit check.
  PostponeInterruptsScope postpone(isolate());

  IncrementalMarking* incremental_marking = heap_->incremental_marking();
  if (was_marked_incrementally_) {
    incremental_marking->Finalize();
  } else {
    // Abort any pending incremental activities e.g. incremental sweeping.
    incremental_marking->Abort();
    if (marking_deque_.in_use()) {
      marking_deque_.Uninitialize(true);
    }
  }

#ifdef DEBUG
  DCHECK(state_ == PREPARE_GC);
  state_ = MARK_LIVE_OBJECTS;
#endif

  EnsureMarkingDequeIsCommittedAndInitialize(
      MarkCompactCollector::kMaxMarkingDequeSize);

  PrepareForCodeFlushing();

  RootMarkingVisitor root_visitor(heap());
  MarkRoots(&root_visitor);

  ProcessTopOptimizedFrame(&root_visitor);

  // Retaining dying maps should happen before or during ephemeral marking
  // because a map could keep the key of an ephemeron alive. Note that map
  // aging is imprecise: maps that are kept alive only by ephemerons will age.
  RetainMaps();

  {
    GCTracer::Scope gc_scope(heap()->tracer(), GCTracer::Scope::MC_WEAKCLOSURE);

    // The objects reachable from the roots are marked, yet unreachable
    // objects are unmarked.  Mark objects reachable due to host
    // application specific logic or through Harmony weak maps.
    ProcessEphemeralMarking(&root_visitor, false);

    // The objects reachable from the roots, weak maps or object groups
    // are marked. Objects pointed to only by weak global handles cannot be
    // immediately reclaimed. Instead, we have to mark them as pending and mark
    // objects reachable from them.
    //
    // First we identify nonlive weak handles and mark them as pending
    // destruction.
    heap()->isolate()->global_handles()->IdentifyWeakHandles(
        &IsUnmarkedHeapObject);
    // Then we mark the objects.
    heap()->isolate()->global_handles()->IterateWeakRoots(&root_visitor);
    ProcessMarkingDeque();

    // Repeat Harmony weak maps marking to mark unmarked objects reachable from
    // the weak roots we just marked as pending destruction.
    //
    // We only process harmony collections, as all object groups have been fully
    // processed and no weakly reachable node can discover new objects groups.
    ProcessEphemeralMarking(&root_visitor, true);
  }

  AfterMarking();

  if (FLAG_print_cumulative_gc_stat) {
    heap_->tracer()->AddMarkingTime(base::OS::TimeCurrentMillis() - start_time);
  }
}


void MarkCompactCollector::AfterMarking() {
  // Prune the string table removing all strings only pointed to by the
  // string table.  Cannot use string_table() here because the string
  // table is marked.
  StringTable* string_table = heap()->string_table();
  InternalizedStringTableCleaner internalized_visitor(heap());
  string_table->IterateElements(&internalized_visitor);
  string_table->ElementsRemoved(internalized_visitor.PointersRemoved());

  ExternalStringTableCleaner external_visitor(heap());
  heap()->external_string_table_.Iterate(&external_visitor);
  heap()->external_string_table_.CleanUp();

  // Process the weak references.
  MarkCompactWeakObjectRetainer mark_compact_object_retainer;
  heap()->ProcessAllWeakReferences(&mark_compact_object_retainer);

  // Remove object groups after marking phase.
  heap()->isolate()->global_handles()->RemoveObjectGroups();
  heap()->isolate()->global_handles()->RemoveImplicitRefGroups();

  // Flush code from collected candidates.
  if (is_code_flushing_enabled()) {
    code_flusher_->ProcessCandidates();
  }

  if (FLAG_track_gc_object_stats) {
    if (FLAG_trace_gc_object_stats) {
      heap()->TraceObjectStats();
    }
    heap()->CheckpointObjectStats();
  }
}


void MarkCompactCollector::ClearNonLiveReferences() {
  // Iterate over the map space, setting map transitions that go from
  // a marked map to an unmarked map to null transitions.  This action
  // is carried out only on maps of JSObjects and related subtypes.
  HeapObjectIterator map_iterator(heap()->map_space());
  for (HeapObject* obj = map_iterator.Next(); obj != NULL;
       obj = map_iterator.Next()) {
    Map* map = Map::cast(obj);

    if (!map->CanTransition()) continue;

    MarkBit map_mark = Marking::MarkBitFrom(map);
    ClearNonLivePrototypeTransitions(map);
    ClearNonLiveMapTransitions(map, map_mark);

    if (Marking::IsWhite(map_mark)) {
      have_code_to_deoptimize_ |=
          map->dependent_code()->MarkCodeForDeoptimization(
              isolate(), DependentCode::kWeakCodeGroup);
      map->set_dependent_code(DependentCode::cast(heap()->empty_fixed_array()));
    }
  }

  WeakHashTable* table = heap_->weak_object_to_code_table();
  uint32_t capacity = table->Capacity();
  for (uint32_t i = 0; i < capacity; i++) {
    uint32_t key_index = table->EntryToIndex(i);
    Object* key = table->get(key_index);
    if (!table->IsKey(key)) continue;
    uint32_t value_index = table->EntryToValueIndex(i);
    Object* value = table->get(value_index);
    DCHECK(key->IsWeakCell());
    if (WeakCell::cast(key)->cleared()) {
      have_code_to_deoptimize_ |=
          DependentCode::cast(value)->MarkCodeForDeoptimization(
              isolate(), DependentCode::kWeakCodeGroup);
      table->set(key_index, heap_->the_hole_value());
      table->set(value_index, heap_->the_hole_value());
      table->ElementRemoved();
    }
  }
}


void MarkCompactCollector::ClearNonLivePrototypeTransitions(Map* map) {
  FixedArray* prototype_transitions =
      TransitionArray::GetPrototypeTransitions(map);
  int number_of_transitions =
      TransitionArray::NumberOfPrototypeTransitions(prototype_transitions);

  const int header = TransitionArray::kProtoTransitionHeaderSize;
  int new_number_of_transitions = 0;
  for (int i = 0; i < number_of_transitions; i++) {
    Object* cell = prototype_transitions->get(header + i);
    if (!WeakCell::cast(cell)->cleared()) {
      if (new_number_of_transitions != i) {
        prototype_transitions->set(header + new_number_of_transitions, cell);
        Object** slot = prototype_transitions->RawFieldOfElementAt(
            header + new_number_of_transitions);
        RecordSlot(slot, slot, cell);
      }
      new_number_of_transitions++;
    }
  }

  if (new_number_of_transitions != number_of_transitions) {
    TransitionArray::SetNumberOfPrototypeTransitions(prototype_transitions,
                                                     new_number_of_transitions);
  }

  // Fill slots that became free with undefined value.
  for (int i = new_number_of_transitions; i < number_of_transitions; i++) {
    prototype_transitions->set_undefined(header + i);
  }
}


void MarkCompactCollector::ClearNonLiveMapTransitions(Map* map,
                                                      MarkBit map_mark) {
  Object* potential_parent = map->GetBackPointer();
  if (!potential_parent->IsMap()) return;
  Map* parent = Map::cast(potential_parent);

  // Follow back pointer, check whether we are dealing with a map transition
  // from a live map to a dead path and in case clear transitions of parent.
  bool current_is_alive = Marking::IsBlackOrGrey(map_mark);
  bool parent_is_alive = Marking::IsBlackOrGrey(Marking::MarkBitFrom(parent));
  if (!current_is_alive && parent_is_alive) {
    ClearMapTransitions(parent, map);
  }
}


// Clear a possible back pointer in case the transition leads to a dead map.
// Return true in case a back pointer has been cleared and false otherwise.
bool MarkCompactCollector::ClearMapBackPointer(Map* target) {
  if (Marking::IsBlackOrGrey(Marking::MarkBitFrom(target))) return false;
  target->SetBackPointer(heap_->undefined_value(), SKIP_WRITE_BARRIER);
  return true;
}


void MarkCompactCollector::ClearMapTransitions(Map* map, Map* dead_transition) {
  Object* transitions = map->raw_transitions();
  int num_transitions = TransitionArray::NumberOfTransitions(transitions);

  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  DescriptorArray* descriptors = map->instance_descriptors();

  // A previously existing simple transition (stored in a WeakCell) may have
  // been cleared. Clear the useless cell pointer, and take ownership
  // of the descriptor array.
  if (transitions->IsWeakCell() && WeakCell::cast(transitions)->cleared()) {
    map->set_raw_transitions(Smi::FromInt(0));
  }
  if (num_transitions == 0 &&
      descriptors == dead_transition->instance_descriptors() &&
      number_of_own_descriptors > 0) {
    TrimDescriptorArray(map, descriptors, number_of_own_descriptors);
    DCHECK(descriptors->number_of_descriptors() == number_of_own_descriptors);
    map->set_owns_descriptors(true);
    return;
  }

  int transition_index = 0;

  bool descriptors_owner_died = false;

  // Compact all live descriptors to the left.
  for (int i = 0; i < num_transitions; ++i) {
    Map* target = TransitionArray::GetTarget(transitions, i);
    if (ClearMapBackPointer(target)) {
      if (target->instance_descriptors() == descriptors) {
        descriptors_owner_died = true;
      }
    } else {
      if (i != transition_index) {
        DCHECK(TransitionArray::IsFullTransitionArray(transitions));
        TransitionArray* t = TransitionArray::cast(transitions);
        Name* key = t->GetKey(i);
        t->SetKey(transition_index, key);
        Object** key_slot = t->GetKeySlot(transition_index);
        RecordSlot(key_slot, key_slot, key);
        // Target slots do not need to be recorded since maps are not compacted.
        t->SetTarget(transition_index, t->GetTarget(i));
      }
      transition_index++;
    }
  }

  // If there are no transitions to be cleared, return.
  // TODO(verwaest) Should be an assert, otherwise back pointers are not
  // properly cleared.
  if (transition_index == num_transitions) return;

  if (descriptors_owner_died) {
    if (number_of_own_descriptors > 0) {
      TrimDescriptorArray(map, descriptors, number_of_own_descriptors);
      DCHECK(descriptors->number_of_descriptors() == number_of_own_descriptors);
      map->set_owns_descriptors(true);
    } else {
      DCHECK(descriptors == heap_->empty_descriptor_array());
    }
  }

  // Note that we never eliminate a transition array, though we might right-trim
  // such that number_of_transitions() == 0. If this assumption changes,
  // TransitionArray::Insert() will need to deal with the case that a transition
  // array disappeared during GC.
  int trim = TransitionArray::Capacity(transitions) - transition_index;
  if (trim > 0) {
    // Non-full-TransitionArray cases can never reach this point.
    DCHECK(TransitionArray::IsFullTransitionArray(transitions));
    TransitionArray* t = TransitionArray::cast(transitions);
    heap_->RightTrimFixedArray<Heap::SEQUENTIAL_TO_SWEEPER>(
        t, trim * TransitionArray::kTransitionSize);
    t->SetNumberOfTransitions(transition_index);
    // The map still has a full transition array.
    DCHECK(TransitionArray::IsFullTransitionArray(map->raw_transitions()));
  }
}


void MarkCompactCollector::TrimDescriptorArray(Map* map,
                                               DescriptorArray* descriptors,
                                               int number_of_own_descriptors) {
  int number_of_descriptors = descriptors->number_of_descriptors_storage();
  int to_trim = number_of_descriptors - number_of_own_descriptors;
  if (to_trim == 0) return;

  heap_->RightTrimFixedArray<Heap::SEQUENTIAL_TO_SWEEPER>(
      descriptors, to_trim * DescriptorArray::kDescriptorSize);
  descriptors->SetNumberOfDescriptors(number_of_own_descriptors);

  if (descriptors->HasEnumCache()) TrimEnumCache(map, descriptors);
  descriptors->Sort();

  if (FLAG_unbox_double_fields) {
    LayoutDescriptor* layout_descriptor = map->layout_descriptor();
    layout_descriptor = layout_descriptor->Trim(heap_, map, descriptors,
                                                number_of_own_descriptors);
    SLOW_DCHECK(layout_descriptor->IsConsistentWithMap(map, true));
  }
}


void MarkCompactCollector::TrimEnumCache(Map* map,
                                         DescriptorArray* descriptors) {
  int live_enum = map->EnumLength();
  if (live_enum == kInvalidEnumCacheSentinel) {
    live_enum = map->NumberOfDescribedProperties(OWN_DESCRIPTORS, DONT_ENUM);
  }
  if (live_enum == 0) return descriptors->ClearEnumCache();

  FixedArray* enum_cache = descriptors->GetEnumCache();

  int to_trim = enum_cache->length() - live_enum;
  if (to_trim <= 0) return;
  heap_->RightTrimFixedArray<Heap::SEQUENTIAL_TO_SWEEPER>(
      descriptors->GetEnumCache(), to_trim);

  if (!descriptors->HasEnumIndicesCache()) return;
  FixedArray* enum_indices_cache = descriptors->GetEnumIndicesCache();
  heap_->RightTrimFixedArray<Heap::SEQUENTIAL_TO_SWEEPER>(enum_indices_cache,
                                                          to_trim);
}


void MarkCompactCollector::ProcessWeakCollections() {
  GCTracer::Scope gc_scope(heap()->tracer(),
                           GCTracer::Scope::MC_WEAKCOLLECTION_PROCESS);
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::FromInt(0)) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    DCHECK(MarkCompactCollector::IsMarked(weak_collection));
    if (weak_collection->table()->IsHashTable()) {
      ObjectHashTable* table = ObjectHashTable::cast(weak_collection->table());
      Object** anchor = reinterpret_cast<Object**>(table->address());
      for (int i = 0; i < table->Capacity(); i++) {
        if (MarkCompactCollector::IsMarked(HeapObject::cast(table->KeyAt(i)))) {
          Object** key_slot =
              table->RawFieldOfElementAt(ObjectHashTable::EntryToIndex(i));
          RecordSlot(anchor, key_slot, *key_slot);
          Object** value_slot =
              table->RawFieldOfElementAt(ObjectHashTable::EntryToValueIndex(i));
          MarkCompactMarkingVisitor::MarkObjectByPointer(this, anchor,
                                                         value_slot);
        }
      }
    }
    weak_collection_obj = weak_collection->next();
  }
}


void MarkCompactCollector::ClearWeakCollections() {
  GCTracer::Scope gc_scope(heap()->tracer(),
                           GCTracer::Scope::MC_WEAKCOLLECTION_CLEAR);
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::FromInt(0)) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    DCHECK(MarkCompactCollector::IsMarked(weak_collection));
    if (weak_collection->table()->IsHashTable()) {
      ObjectHashTable* table = ObjectHashTable::cast(weak_collection->table());
      for (int i = 0; i < table->Capacity(); i++) {
        HeapObject* key = HeapObject::cast(table->KeyAt(i));
        if (!MarkCompactCollector::IsMarked(key)) {
          table->RemoveEntry(i);
        }
      }
    }
    weak_collection_obj = weak_collection->next();
    weak_collection->set_next(heap()->undefined_value());
  }
  heap()->set_encountered_weak_collections(Smi::FromInt(0));
}


void MarkCompactCollector::AbortWeakCollections() {
  GCTracer::Scope gc_scope(heap()->tracer(),
                           GCTracer::Scope::MC_WEAKCOLLECTION_ABORT);
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::FromInt(0)) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    weak_collection_obj = weak_collection->next();
    weak_collection->set_next(heap()->undefined_value());
  }
  heap()->set_encountered_weak_collections(Smi::FromInt(0));
}


void MarkCompactCollector::ProcessAndClearWeakCells() {
  Object* weak_cell_obj = heap()->encountered_weak_cells();
  while (weak_cell_obj != Smi::FromInt(0)) {
    WeakCell* weak_cell = reinterpret_cast<WeakCell*>(weak_cell_obj);
    // We do not insert cleared weak cells into the list, so the value
    // cannot be a Smi here.
    HeapObject* value = HeapObject::cast(weak_cell->value());
    if (!MarkCompactCollector::IsMarked(value)) {
      // Cells for new-space objects embedded in optimized code are wrapped in
      // WeakCell and put into Heap::weak_object_to_code_table.
      // Such cells do not have any strong references but we want to keep them
      // alive as long as the cell value is alive.
      // TODO(ulan): remove this once we remove Heap::weak_object_to_code_table.
      if (value->IsCell()) {
        Object* cell_value = Cell::cast(value)->value();
        if (cell_value->IsHeapObject() &&
            MarkCompactCollector::IsMarked(HeapObject::cast(cell_value))) {
          // Resurrect the cell.
          MarkBit mark = Marking::MarkBitFrom(value);
          SetMark(value, mark);
          Object** slot = HeapObject::RawField(value, Cell::kValueOffset);
          RecordSlot(slot, slot, *slot);
          slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
          RecordSlot(slot, slot, *slot);
        } else {
          weak_cell->clear();
        }
      } else {
        weak_cell->clear();
      }
    } else {
      Object** slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
      RecordSlot(slot, slot, *slot);
    }
    weak_cell_obj = weak_cell->next();
    weak_cell->clear_next(heap());
  }
  heap()->set_encountered_weak_cells(Smi::FromInt(0));
}


void MarkCompactCollector::AbortWeakCells() {
  Object* weak_cell_obj = heap()->encountered_weak_cells();
  while (weak_cell_obj != Smi::FromInt(0)) {
    WeakCell* weak_cell = reinterpret_cast<WeakCell*>(weak_cell_obj);
    weak_cell_obj = weak_cell->next();
    weak_cell->clear_next(heap());
  }
  heap()->set_encountered_weak_cells(Smi::FromInt(0));
}


void MarkCompactCollector::RecordMigratedSlot(Object* value, Address slot) {
  if (heap_->InNewSpace(value)) {
    heap_->store_buffer()->Mark(slot);
  } else if (value->IsHeapObject() && IsOnEvacuationCandidate(value)) {
    SlotsBuffer::AddTo(&slots_buffer_allocator_, &migration_slots_buffer_,
                       reinterpret_cast<Object**>(slot),
                       SlotsBuffer::IGNORE_OVERFLOW);
  }
}


// We scavenge new space simultaneously with sweeping. This is done in two
// passes.
//
// The first pass migrates all alive objects from one semispace to another or
// promotes them to old space.  Forwarding address is written directly into
// first word of object without any encoding.  If object is dead we write
// NULL as a forwarding address.
//
// The second pass updates pointers to new space in all spaces.  It is possible
// to encounter pointers to dead new space objects during traversal of pointers
// to new space.  We should clear them to avoid encountering them during next
// pointer iteration.  This is an issue if the store buffer overflows and we
// have to scan the entire old space, including dead objects, looking for
// pointers to new space.
void MarkCompactCollector::MigrateObject(HeapObject* dst, HeapObject* src,
                                         int size, AllocationSpace dest) {
  Address dst_addr = dst->address();
  Address src_addr = src->address();
  DCHECK(heap()->AllowedToBeMigrated(src, dest));
  DCHECK(dest != LO_SPACE && size <= Page::kMaxRegularHeapObjectSize);
  if (dest == OLD_SPACE) {
    DCHECK(IsAligned(size, kPointerSize));
    switch (src->ContentType()) {
      case HeapObjectContents::kTaggedValues:
        MigrateObjectTagged(dst, src, size);
        break;

      case HeapObjectContents::kMixedValues:
        MigrateObjectMixed(dst, src, size);
        break;

      case HeapObjectContents::kRawValues:
        MigrateObjectRaw(dst, src, size);
        break;
    }

    if (compacting_ && dst->IsJSFunction()) {
      Address code_entry_slot = dst->address() + JSFunction::kCodeEntryOffset;
      Address code_entry = Memory::Address_at(code_entry_slot);

      if (Page::FromAddress(code_entry)->IsEvacuationCandidate()) {
        SlotsBuffer::AddTo(&slots_buffer_allocator_, &migration_slots_buffer_,
                           SlotsBuffer::CODE_ENTRY_SLOT, code_entry_slot,
                           SlotsBuffer::IGNORE_OVERFLOW);
      }
    }
  } else if (dest == CODE_SPACE) {
    PROFILE(isolate(), CodeMoveEvent(src_addr, dst_addr));
    heap()->MoveBlock(dst_addr, src_addr, size);
    SlotsBuffer::AddTo(&slots_buffer_allocator_, &migration_slots_buffer_,
                       SlotsBuffer::RELOCATED_CODE_OBJECT, dst_addr,
                       SlotsBuffer::IGNORE_OVERFLOW);
    Code::cast(dst)->Relocate(dst_addr - src_addr);
  } else {
    DCHECK(dest == NEW_SPACE);
    heap()->MoveBlock(dst_addr, src_addr, size);
  }
  heap()->OnMoveEvent(dst, src, size);
  Memory::Address_at(src_addr) = dst_addr;
}


void MarkCompactCollector::MigrateObjectTagged(HeapObject* dst, HeapObject* src,
                                               int size) {
  Address src_slot = src->address();
  Address dst_slot = dst->address();
  for (int remaining = size / kPointerSize; remaining > 0; remaining--) {
    Object* value = Memory::Object_at(src_slot);
    Memory::Object_at(dst_slot) = value;
    RecordMigratedSlot(value, dst_slot);
    src_slot += kPointerSize;
    dst_slot += kPointerSize;
  }
}


void MarkCompactCollector::MigrateObjectMixed(HeapObject* dst, HeapObject* src,
                                              int size) {
  if (src->IsFixedTypedArrayBase()) {
    heap()->MoveBlock(dst->address(), src->address(), size);
    Address base_pointer_slot =
        dst->address() + FixedTypedArrayBase::kBasePointerOffset;
    RecordMigratedSlot(Memory::Object_at(base_pointer_slot), base_pointer_slot);
  } else if (src->IsJSArrayBuffer()) {
    heap()->MoveBlock(dst->address(), src->address(), size);

    // Visit inherited JSObject properties and byte length of ArrayBuffer
    Address regular_slot =
        dst->address() + JSArrayBuffer::BodyDescriptor::kStartOffset;
    Address regular_slots_end =
        dst->address() + JSArrayBuffer::kByteLengthOffset + kPointerSize;
    while (regular_slot < regular_slots_end) {
      RecordMigratedSlot(Memory::Object_at(regular_slot), regular_slot);
      regular_slot += kPointerSize;
    }

    // Skip backing store and visit just internal fields
    Address internal_field_slot = dst->address() + JSArrayBuffer::kSize;
    Address internal_fields_end =
        dst->address() + JSArrayBuffer::kSizeWithInternalFields;
    while (internal_field_slot < internal_fields_end) {
      RecordMigratedSlot(Memory::Object_at(internal_field_slot),
                         internal_field_slot);
      internal_field_slot += kPointerSize;
    }
  } else if (FLAG_unbox_double_fields) {
    Address dst_addr = dst->address();
    Address src_addr = src->address();
    Address src_slot = src_addr;
    Address dst_slot = dst_addr;

    LayoutDescriptorHelper helper(src->map());
    DCHECK(!helper.all_fields_tagged());
    for (int remaining = size / kPointerSize; remaining > 0; remaining--) {
      Object* value = Memory::Object_at(src_slot);

      Memory::Object_at(dst_slot) = value;

      if (helper.IsTagged(static_cast<int>(src_slot - src_addr))) {
        RecordMigratedSlot(value, dst_slot);
      }

      src_slot += kPointerSize;
      dst_slot += kPointerSize;
    }
  } else {
    UNREACHABLE();
  }
}


void MarkCompactCollector::MigrateObjectRaw(HeapObject* dst, HeapObject* src,
                                            int size) {
  heap()->MoveBlock(dst->address(), src->address(), size);
}


// Visitor for updating pointers from live objects in old spaces to new space.
// It does not expect to encounter pointers to dead objects.
class PointersUpdatingVisitor : public ObjectVisitor {
 public:
  explicit PointersUpdatingVisitor(Heap* heap) : heap_(heap) {}

  void VisitPointer(Object** p) { UpdatePointer(p); }

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) UpdatePointer(p);
  }

  void VisitCell(RelocInfo* rinfo) {
    DCHECK(rinfo->rmode() == RelocInfo::CELL);
    Object* cell = rinfo->target_cell();
    Object* old_cell = cell;
    VisitPointer(&cell);
    if (cell != old_cell) {
      rinfo->set_target_cell(reinterpret_cast<Cell*>(cell));
    }
  }

  void VisitEmbeddedPointer(RelocInfo* rinfo) {
    DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
    Object* target = rinfo->target_object();
    Object* old_target = target;
    VisitPointer(&target);
    // Avoid unnecessary changes that might unnecessary flush the instruction
    // cache.
    if (target != old_target) {
      rinfo->set_target_object(target);
    }
  }

  void VisitCodeTarget(RelocInfo* rinfo) {
    DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Object* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    Object* old_target = target;
    VisitPointer(&target);
    if (target != old_target) {
      rinfo->set_target_address(Code::cast(target)->instruction_start());
    }
  }

  void VisitCodeAgeSequence(RelocInfo* rinfo) {
    DCHECK(RelocInfo::IsCodeAgeSequence(rinfo->rmode()));
    Object* stub = rinfo->code_age_stub();
    DCHECK(stub != NULL);
    VisitPointer(&stub);
    if (stub != rinfo->code_age_stub()) {
      rinfo->set_code_age_stub(Code::cast(stub));
    }
  }

  void VisitDebugTarget(RelocInfo* rinfo) {
    DCHECK((RelocInfo::IsJSReturn(rinfo->rmode()) &&
            rinfo->IsPatchedReturnSequence()) ||
           (RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
            rinfo->IsPatchedDebugBreakSlotSequence()));
    Object* target = Code::GetCodeFromTargetAddress(rinfo->call_address());
    VisitPointer(&target);
    rinfo->set_call_address(Code::cast(target)->instruction_start());
  }

  static inline void UpdateSlot(Heap* heap, Object** slot) {
    Object* obj = reinterpret_cast<Object*>(
        base::NoBarrier_Load(reinterpret_cast<base::AtomicWord*>(slot)));

    if (!obj->IsHeapObject()) return;

    HeapObject* heap_obj = HeapObject::cast(obj);

// TODO(ishell): remove, once crbug/454297 is caught.
#if V8_TARGET_ARCH_64_BIT
#ifndef V8_OS_AIX  // no point checking on AIX as full 64 range is supported
    const uintptr_t kBoundary = V8_UINT64_C(1) << 48;
    STATIC_ASSERT(kBoundary > 0);
    if (reinterpret_cast<uintptr_t>(heap_obj->address()) >= kBoundary) {
      CheckLayoutDescriptorAndDie(heap, slot);
    }
#endif
#endif
    MapWord map_word = heap_obj->map_word();
    if (map_word.IsForwardingAddress()) {
      DCHECK(heap->InFromSpace(heap_obj) ||
             MarkCompactCollector::IsOnEvacuationCandidate(heap_obj));
      HeapObject* target = map_word.ToForwardingAddress();
      base::NoBarrier_CompareAndSwap(
          reinterpret_cast<base::AtomicWord*>(slot),
          reinterpret_cast<base::AtomicWord>(obj),
          reinterpret_cast<base::AtomicWord>(target));
      DCHECK(!heap->InFromSpace(target) &&
             !MarkCompactCollector::IsOnEvacuationCandidate(target));
    }
  }

 private:
  inline void UpdatePointer(Object** p) { UpdateSlot(heap_, p); }

  static void CheckLayoutDescriptorAndDie(Heap* heap, Object** slot);

  Heap* heap_;
};


#if V8_TARGET_ARCH_64_BIT
// TODO(ishell): remove, once crbug/454297 is caught.
void PointersUpdatingVisitor::CheckLayoutDescriptorAndDie(Heap* heap,
                                                          Object** slot) {
  const int kDataBufferSize = 128;
  uintptr_t data[kDataBufferSize] = {0};
  int index = 0;
  data[index++] = 0x10aaaaaaaaUL;  // begin marker

  data[index++] = reinterpret_cast<uintptr_t>(slot);
  data[index++] = 0x15aaaaaaaaUL;

  Address slot_address = reinterpret_cast<Address>(slot);

  uintptr_t space_owner_id = 0xb001;
  if (heap->new_space()->ToSpaceContains(slot_address)) {
    space_owner_id = 1;
  } else if (heap->new_space()->FromSpaceContains(slot_address)) {
    space_owner_id = 2;
  } else if (heap->old_space()->ContainsSafe(slot_address)) {
    space_owner_id = 3;
  } else if (heap->code_space()->ContainsSafe(slot_address)) {
    space_owner_id = 4;
  } else if (heap->map_space()->ContainsSafe(slot_address)) {
    space_owner_id = 5;
  } else {
    // Lo space or other.
    space_owner_id = 6;
  }
  data[index++] = space_owner_id;
  data[index++] = 0x20aaaaaaaaUL;

  // Find map word lying near before the slot address (usually the map word is
  // at -3 words from the slot but just in case we look up further.
  Object** map_slot = slot;
  bool found = false;
  const int kMaxDistanceToMap = 64;
  for (int i = 0; i < kMaxDistanceToMap; i++, map_slot--) {
    Address map_address = reinterpret_cast<Address>(*map_slot);
    if (heap->map_space()->ContainsSafe(map_address)) {
      found = true;
      break;
    }
  }
  data[index++] = found;
  data[index++] = 0x30aaaaaaaaUL;
  data[index++] = reinterpret_cast<uintptr_t>(map_slot);
  data[index++] = 0x35aaaaaaaaUL;

  if (found) {
    Address obj_address = reinterpret_cast<Address>(map_slot);
    Address end_of_page =
        reinterpret_cast<Address>(Page::FromAddress(obj_address)) +
        Page::kPageSize;
    Address end_address =
        Min(obj_address + kPointerSize * kMaxDistanceToMap, end_of_page);
    int size = static_cast<int>(end_address - obj_address);
    data[index++] = size / kPointerSize;
    data[index++] = 0x40aaaaaaaaUL;
    memcpy(&data[index], reinterpret_cast<void*>(map_slot), size);
    index += size / kPointerSize;
    data[index++] = 0x50aaaaaaaaUL;

    HeapObject* object = HeapObject::FromAddress(obj_address);
    data[index++] = reinterpret_cast<uintptr_t>(object);
    data[index++] = 0x60aaaaaaaaUL;

    Map* map = object->map();
    data[index++] = reinterpret_cast<uintptr_t>(map);
    data[index++] = 0x70aaaaaaaaUL;

    LayoutDescriptor* layout_descriptor = map->layout_descriptor();
    data[index++] = reinterpret_cast<uintptr_t>(layout_descriptor);
    data[index++] = 0x80aaaaaaaaUL;

    memcpy(&data[index], reinterpret_cast<void*>(map->address()), Map::kSize);
    index += Map::kSize / kPointerSize;
    data[index++] = 0x90aaaaaaaaUL;
  }

  data[index++] = 0xeeeeeeeeeeUL;
  DCHECK(index < kDataBufferSize);
  base::OS::PrintError("Data: %p\n", static_cast<void*>(data));
  base::OS::Abort();
}
#endif


static void UpdatePointer(HeapObject** address, HeapObject* object) {
  MapWord map_word = object->map_word();
  // The store buffer can still contain stale pointers in dead large objects.
  // Ignore these pointers here.
  DCHECK(map_word.IsForwardingAddress() ||
         object->GetHeap()->lo_space()->FindPage(
             reinterpret_cast<Address>(address)) != NULL);
  if (map_word.IsForwardingAddress()) {
    // Update the corresponding slot.
    *address = map_word.ToForwardingAddress();
  }
}


static String* UpdateReferenceInExternalStringTableEntry(Heap* heap,
                                                         Object** p) {
  MapWord map_word = HeapObject::cast(*p)->map_word();

  if (map_word.IsForwardingAddress()) {
    return String::cast(map_word.ToForwardingAddress());
  }

  return String::cast(*p);
}


bool MarkCompactCollector::TryPromoteObject(HeapObject* object,
                                            int object_size) {
  DCHECK(object_size <= Page::kMaxRegularHeapObjectSize);

  OldSpace* old_space = heap()->old_space();

  HeapObject* target;
  AllocationAlignment alignment = object->RequiredAlignment();
  AllocationResult allocation = old_space->AllocateRaw(object_size, alignment);
  if (allocation.To(&target)) {
    MigrateObject(target, object, object_size, old_space->identity());
    // If we end up needing more special cases, we should factor this out.
    if (V8_UNLIKELY(target->IsJSArrayBuffer())) {
      heap()->PromoteArrayBuffer(target);
    }
    heap()->IncrementPromotedObjectsSize(object_size);
    return true;
  }

  return false;
}


bool MarkCompactCollector::IsSlotInBlackObject(Page* p, Address slot,
                                               HeapObject** out_object) {
  Space* owner = p->owner();
  if (owner == heap_->lo_space() || owner == NULL) {
    Object* large_object = heap_->lo_space()->FindObject(slot);
    // This object has to exist, otherwise we would not have recorded a slot
    // for it.
    CHECK(large_object->IsHeapObject());
    HeapObject* large_heap_object = HeapObject::cast(large_object);
    if (IsMarked(large_heap_object)) {
      *out_object = large_heap_object;
      return true;
    }
    return false;
  }

  uint32_t mark_bit_index = p->AddressToMarkbitIndex(slot);
  unsigned int start_index = mark_bit_index >> Bitmap::kBitsPerCellLog2;
  MarkBit::CellType index_in_cell = 1U
                                    << (mark_bit_index & Bitmap::kBitIndexMask);
  MarkBit::CellType* cells = p->markbits()->cells();
  Address cell_base = p->area_start();
  unsigned int cell_base_start_index = Bitmap::IndexToCell(
      Bitmap::CellAlignIndex(p->AddressToMarkbitIndex(cell_base)));

  // Check if the slot points to the start of an object. This can happen e.g.
  // when we left trim a fixed array. Such slots are invalid and we can remove
  // them.
  if ((cells[start_index] & index_in_cell) != 0) {
    return false;
  }

  // Check if the object is in the current cell.
  MarkBit::CellType slot_mask;
  if ((cells[start_index] == 0) ||
      (base::bits::CountTrailingZeros32(cells[start_index]) >
       base::bits::CountTrailingZeros32(cells[start_index] | index_in_cell))) {
    // If we are already in the first cell, there is no live object.
    if (start_index == cell_base_start_index) return false;

    // If not, find a cell in a preceding cell slot that has a mark bit set.
    do {
      start_index--;
    } while (start_index > cell_base_start_index && cells[start_index] == 0);

    // The slot must be in a dead object if there are no preceding cells that
    // have mark bits set.
    if (cells[start_index] == 0) {
      return false;
    }

    // The object is in a preceding cell. Set the mask to find any object.
    slot_mask = 0xffffffff;
  } else {
    // The object start is before the the slot index. Hence, in this case the
    // slot index can not be at the beginning of the cell.
    CHECK(index_in_cell > 1);
    // We are interested in object mark bits right before the slot.
    slot_mask = index_in_cell - 1;
  }

  MarkBit::CellType current_cell = cells[start_index];
  CHECK(current_cell != 0);

  // Find the last live object in the cell.
  unsigned int leading_zeros =
      base::bits::CountLeadingZeros32(current_cell & slot_mask);
  CHECK(leading_zeros != 32);
  unsigned int offset = Bitmap::kBitIndexMask - leading_zeros;

  cell_base += (start_index - cell_base_start_index) * 32 * kPointerSize;
  Address address = cell_base + offset * kPointerSize;
  HeapObject* object = HeapObject::FromAddress(address);
  CHECK(Marking::IsBlack(Marking::MarkBitFrom(object)));
  CHECK(object->address() < reinterpret_cast<Address>(slot));
  if (object->address() <= slot &&
      (object->address() + object->Size()) > slot) {
    // If the slot is within the last found object in the cell, the slot is
    // in a live object.
    *out_object = object;
    return true;
  }
  return false;
}


bool MarkCompactCollector::IsSlotInBlackObjectSlow(Page* p, Address slot) {
  // This function does not support large objects right now.
  Space* owner = p->owner();
  if (owner == heap_->lo_space() || owner == NULL) return true;

  for (MarkBitCellIterator it(p); !it.Done(); it.Advance()) {
    Address cell_base = it.CurrentCellBase();
    MarkBit::CellType* cell = it.CurrentCell();

    MarkBit::CellType current_cell = *cell;
    if (current_cell == 0) continue;

    int offset = 0;
    while (current_cell != 0) {
      int trailing_zeros = base::bits::CountTrailingZeros32(current_cell);
      current_cell >>= trailing_zeros;
      offset += trailing_zeros;
      Address address = cell_base + offset * kPointerSize;

      HeapObject* object = HeapObject::FromAddress(address);
      int size = object->Size();

      if (object->address() > slot) return false;
      if (object->address() <= slot && slot < (object->address() + size)) {
        return true;
      }

      offset++;
      current_cell >>= 1;
    }
  }
  return false;
}


bool MarkCompactCollector::IsSlotInLiveObject(Address slot) {
  HeapObject* object = NULL;
  // The target object is black but we don't know if the source slot is black.
  // The source object could have died and the slot could be part of a free
  // space. Find out based on mark bits if the slot is part of a live object.
  if (!IsSlotInBlackObject(Page::FromAddress(slot), slot, &object)) {
    return false;
  }

  DCHECK(object != NULL);

    switch (object->ContentType()) {
      case HeapObjectContents::kTaggedValues:
        return true;

      case HeapObjectContents::kRawValues: {
        InstanceType type = object->map()->instance_type();
        // Slots in maps and code can't be invalid because they are never
        // shrunk.
        if (type == MAP_TYPE || type == CODE_TYPE) return true;

        // Consider slots in objects that contain ONLY raw data as invalid.
        return false;
      }

      case HeapObjectContents::kMixedValues: {
        if (object->IsFixedTypedArrayBase()) {
          return static_cast<int>(slot - object->address()) ==
                 FixedTypedArrayBase::kBasePointerOffset;
        } else if (object->IsJSArrayBuffer()) {
          int off = static_cast<int>(slot - object->address());
          return (off >= JSArrayBuffer::BodyDescriptor::kStartOffset &&
                  off <= JSArrayBuffer::kByteLengthOffset) ||
                 (off >= JSArrayBuffer::kSize &&
                  off < JSArrayBuffer::kSizeWithInternalFields);
        } else if (FLAG_unbox_double_fields) {
          // Filter out slots that happen to point to unboxed double fields.
          LayoutDescriptorHelper helper(object->map());
          DCHECK(!helper.all_fields_tagged());
          return helper.IsTagged(static_cast<int>(slot - object->address()));
        }
        break;
      }
    }
    UNREACHABLE();
    return true;
}


void MarkCompactCollector::VerifyIsSlotInLiveObject(Address slot,
                                                    HeapObject* object) {
  // The target object has to be black.
  CHECK(Marking::IsBlack(Marking::MarkBitFrom(object)));

  // The target object is black but we don't know if the source slot is black.
  // The source object could have died and the slot could be part of a free
  // space. Use the mark bit iterator to find out about liveness of the slot.
  CHECK(IsSlotInBlackObjectSlow(Page::FromAddress(slot), slot));
}


void MarkCompactCollector::RemoveObjectSlots(Address start_slot,
                                             Address end_slot) {
  // Remove entries by replacing them with an old-space slot containing a smi
  // that is located in an unmovable page.
  int npages = evacuation_candidates_.length();
  for (int i = 0; i < npages; i++) {
    Page* p = evacuation_candidates_[i];
    DCHECK(p->IsEvacuationCandidate() ||
           p->IsFlagSet(Page::RESCAN_ON_EVACUATION));
    if (p->IsEvacuationCandidate()) {
      SlotsBuffer::RemoveObjectSlots(heap_, p->slots_buffer(), start_slot,
                                     end_slot);
    }
  }
}


void MarkCompactCollector::EvacuateNewSpace() {
  // There are soft limits in the allocation code, designed trigger a mark
  // sweep collection by failing allocations.  But since we are already in
  // a mark-sweep allocation, there is no sense in trying to trigger one.
  AlwaysAllocateScope scope(isolate());

  NewSpace* new_space = heap()->new_space();

  // Store allocation range before flipping semispaces.
  Address from_bottom = new_space->bottom();
  Address from_top = new_space->top();

  // Flip the semispaces.  After flipping, to space is empty, from space has
  // live objects.
  new_space->Flip();
  new_space->ResetAllocationInfo();

  int survivors_size = 0;

  // First pass: traverse all objects in inactive semispace, remove marks,
  // migrate live objects and write forwarding addresses.  This stage puts
  // new entries in the store buffer and may cause some pages to be marked
  // scan-on-scavenge.
  NewSpacePageIterator it(from_bottom, from_top);
  while (it.has_next()) {
    NewSpacePage* p = it.next();
    survivors_size += DiscoverAndEvacuateBlackObjectsOnPage(new_space, p);
  }

  heap_->IncrementYoungSurvivorsCounter(survivors_size);
  new_space->set_age_mark(new_space->top());
}


void MarkCompactCollector::EvacuateLiveObjectsFromPage(Page* p) {
  AlwaysAllocateScope always_allocate(isolate());
  PagedSpace* space = static_cast<PagedSpace*>(p->owner());
  DCHECK(p->IsEvacuationCandidate() && !p->WasSwept());
  p->SetWasSwept();

  int offsets[16];

  for (MarkBitCellIterator it(p); !it.Done(); it.Advance()) {
    Address cell_base = it.CurrentCellBase();
    MarkBit::CellType* cell = it.CurrentCell();

    if (*cell == 0) continue;

    int live_objects = MarkWordToObjectStarts(*cell, offsets);
    for (int i = 0; i < live_objects; i++) {
      Address object_addr = cell_base + offsets[i] * kPointerSize;
      HeapObject* object = HeapObject::FromAddress(object_addr);
      DCHECK(Marking::IsBlack(Marking::MarkBitFrom(object)));

      int size = object->Size();
      AllocationAlignment alignment = object->RequiredAlignment();
      HeapObject* target_object;
      AllocationResult allocation = space->AllocateRaw(size, alignment);
      if (!allocation.To(&target_object)) {
        // If allocation failed, use emergency memory and re-try allocation.
        CHECK(space->HasEmergencyMemory());
        space->UseEmergencyMemory();
        allocation = space->AllocateRaw(size, alignment);
      }
      if (!allocation.To(&target_object)) {
        // OS refused to give us memory.
        V8::FatalProcessOutOfMemory("Evacuation");
        return;
      }

      MigrateObject(target_object, object, size, space->identity());
      DCHECK(object->map_word().IsForwardingAddress());
    }

    // Clear marking bits for current cell.
    *cell = 0;
  }
  p->ResetLiveBytes();
}


void MarkCompactCollector::EvacuatePages() {
  int npages = evacuation_candidates_.length();
  int abandoned_pages = 0;
  for (int i = 0; i < npages; i++) {
    Page* p = evacuation_candidates_[i];
    DCHECK(p->IsEvacuationCandidate() ||
           p->IsFlagSet(Page::RESCAN_ON_EVACUATION));
    DCHECK(static_cast<int>(p->parallel_sweeping()) ==
           MemoryChunk::SWEEPING_DONE);
    PagedSpace* space = static_cast<PagedSpace*>(p->owner());
    // Allocate emergency memory for the case when compaction fails due to out
    // of memory.
    if (!space->HasEmergencyMemory()) {
      space->CreateEmergencyMemory();  // If the OS lets us.
    }
    if (p->IsEvacuationCandidate()) {
      // During compaction we might have to request a new page in order to free
      // up a page.  Check that we actually got an emergency page above so we
      // can guarantee that this succeeds.
      if (space->HasEmergencyMemory()) {
        EvacuateLiveObjectsFromPage(p);
        // Unlink the page from the list of pages here. We must not iterate
        // over that page later (e.g. when scan on scavenge pages are
        // processed). The page itself will be freed later and is still
        // reachable from the evacuation candidates list.
        p->Unlink();
      } else {
        // Without room for expansion evacuation is not guaranteed to succeed.
        // Pessimistically abandon unevacuated pages.
        for (int j = i; j < npages; j++) {
          Page* page = evacuation_candidates_[j];
          slots_buffer_allocator_.DeallocateChain(page->slots_buffer_address());
          page->ClearEvacuationCandidate();
          page->SetFlag(Page::RESCAN_ON_EVACUATION);
        }
        abandoned_pages = npages - i;
        break;
      }
    }
  }
  if (npages > 0) {
    // Release emergency memory.
    PagedSpaces spaces(heap());
    for (PagedSpace* space = spaces.next(); space != NULL;
         space = spaces.next()) {
      if (space->HasEmergencyMemory()) {
        space->FreeEmergencyMemory();
      }
    }
    if (FLAG_trace_fragmentation) {
      if (abandoned_pages != 0) {
        PrintF(
            "  Abandon %d out of %d page defragmentations due to lack of "
            "memory\n",
            abandoned_pages, npages);
      } else {
        PrintF("  Defragmented %d pages\n", npages);
      }
    }
  }
}


class EvacuationWeakObjectRetainer : public WeakObjectRetainer {
 public:
  virtual Object* RetainAs(Object* object) {
    if (object->IsHeapObject()) {
      HeapObject* heap_object = HeapObject::cast(object);
      MapWord map_word = heap_object->map_word();
      if (map_word.IsForwardingAddress()) {
        return map_word.ToForwardingAddress();
      }
    }
    return object;
  }
};


static inline void UpdateSlot(Isolate* isolate, ObjectVisitor* v,
                              SlotsBuffer::SlotType slot_type, Address addr) {
  switch (slot_type) {
    case SlotsBuffer::CODE_TARGET_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::CODE_TARGET, 0, NULL);
      rinfo.Visit(isolate, v);
      break;
    }
    case SlotsBuffer::CELL_TARGET_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::CELL, 0, NULL);
      rinfo.Visit(isolate, v);
      break;
    }
    case SlotsBuffer::CODE_ENTRY_SLOT: {
      v->VisitCodeEntry(addr);
      break;
    }
    case SlotsBuffer::RELOCATED_CODE_OBJECT: {
      HeapObject* obj = HeapObject::FromAddress(addr);
      Code::cast(obj)->CodeIterateBody(v);
      break;
    }
    case SlotsBuffer::DEBUG_TARGET_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::DEBUG_BREAK_SLOT, 0, NULL);
      if (rinfo.IsPatchedDebugBreakSlotSequence()) rinfo.Visit(isolate, v);
      break;
    }
    case SlotsBuffer::JS_RETURN_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::JS_RETURN, 0, NULL);
      if (rinfo.IsPatchedReturnSequence()) rinfo.Visit(isolate, v);
      break;
    }
    case SlotsBuffer::EMBEDDED_OBJECT_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::EMBEDDED_OBJECT, 0, NULL);
      rinfo.Visit(isolate, v);
      break;
    }
    case SlotsBuffer::OBJECT_SLOT: {
      v->VisitPointer(reinterpret_cast<Object**>(addr));
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
}


enum SweepingMode { SWEEP_ONLY, SWEEP_AND_VISIT_LIVE_OBJECTS };


enum SkipListRebuildingMode { REBUILD_SKIP_LIST, IGNORE_SKIP_LIST };


enum FreeSpaceTreatmentMode { IGNORE_FREE_SPACE, ZAP_FREE_SPACE };


template <MarkCompactCollector::SweepingParallelism mode>
static intptr_t Free(PagedSpace* space, FreeList* free_list, Address start,
                     int size) {
  if (mode == MarkCompactCollector::SWEEP_ON_MAIN_THREAD) {
    DCHECK(free_list == NULL);
    return space->Free(start, size);
  } else {
    // TODO(hpayer): account for wasted bytes in concurrent sweeping too.
    return size - free_list->Free(start, size);
  }
}


// Sweeps a page. After sweeping the page can be iterated.
// Slots in live objects pointing into evacuation candidates are updated
// if requested.
// Returns the size of the biggest continuous freed memory chunk in bytes.
template <SweepingMode sweeping_mode,
          MarkCompactCollector::SweepingParallelism parallelism,
          SkipListRebuildingMode skip_list_mode,
          FreeSpaceTreatmentMode free_space_mode>
static int Sweep(PagedSpace* space, FreeList* free_list, Page* p,
                 ObjectVisitor* v) {
  DCHECK(!p->IsEvacuationCandidate() && !p->WasSwept());
  DCHECK_EQ(skip_list_mode == REBUILD_SKIP_LIST,
            space->identity() == CODE_SPACE);
  DCHECK((p->skip_list() == NULL) || (skip_list_mode == REBUILD_SKIP_LIST));
  DCHECK(parallelism == MarkCompactCollector::SWEEP_ON_MAIN_THREAD ||
         sweeping_mode == SWEEP_ONLY);

  Address free_start = p->area_start();
  DCHECK(reinterpret_cast<intptr_t>(free_start) % (32 * kPointerSize) == 0);
  int offsets[16];

  SkipList* skip_list = p->skip_list();
  int curr_region = -1;
  if ((skip_list_mode == REBUILD_SKIP_LIST) && skip_list) {
    skip_list->Clear();
  }

  intptr_t freed_bytes = 0;
  intptr_t max_freed_bytes = 0;

  for (MarkBitCellIterator it(p); !it.Done(); it.Advance()) {
    Address cell_base = it.CurrentCellBase();
    MarkBit::CellType* cell = it.CurrentCell();
    int live_objects = MarkWordToObjectStarts(*cell, offsets);
    int live_index = 0;
    for (; live_objects != 0; live_objects--) {
      Address free_end = cell_base + offsets[live_index++] * kPointerSize;
      if (free_end != free_start) {
        int size = static_cast<int>(free_end - free_start);
        if (free_space_mode == ZAP_FREE_SPACE) {
          memset(free_start, 0xcc, size);
        }
        freed_bytes = Free<parallelism>(space, free_list, free_start, size);
        max_freed_bytes = Max(freed_bytes, max_freed_bytes);
      }
      HeapObject* live_object = HeapObject::FromAddress(free_end);
      DCHECK(Marking::IsBlack(Marking::MarkBitFrom(live_object)));
      Map* map = live_object->synchronized_map();
      int size = live_object->SizeFromMap(map);
      if (sweeping_mode == SWEEP_AND_VISIT_LIVE_OBJECTS) {
        live_object->IterateBody(map->instance_type(), size, v);
      }
      if ((skip_list_mode == REBUILD_SKIP_LIST) && skip_list != NULL) {
        int new_region_start = SkipList::RegionNumber(free_end);
        int new_region_end =
            SkipList::RegionNumber(free_end + size - kPointerSize);
        if (new_region_start != curr_region || new_region_end != curr_region) {
          skip_list->AddObject(free_end, size);
          curr_region = new_region_end;
        }
      }
      free_start = free_end + size;
    }
    // Clear marking bits for current cell.
    *cell = 0;
  }
  if (free_start != p->area_end()) {
    int size = static_cast<int>(p->area_end() - free_start);
    if (free_space_mode == ZAP_FREE_SPACE) {
      memset(free_start, 0xcc, size);
    }
    freed_bytes = Free<parallelism>(space, free_list, free_start, size);
    max_freed_bytes = Max(freed_bytes, max_freed_bytes);
  }
  p->ResetLiveBytes();

  if (parallelism == MarkCompactCollector::SWEEP_IN_PARALLEL) {
    // When concurrent sweeping is active, the page will be marked after
    // sweeping by the main thread.
    p->set_parallel_sweeping(MemoryChunk::SWEEPING_FINALIZE);
  } else {
    p->SetWasSwept();
  }
  return FreeList::GuaranteedAllocatable(static_cast<int>(max_freed_bytes));
}


// Return true if the given code is deoptimized or will be deoptimized.
bool MarkCompactCollector::WillBeDeoptimized(Code* code) {
  return code->is_optimized_code() && code->marked_for_deoptimization();
}


void MarkCompactCollector::EvacuateNewSpaceAndCandidates() {
  Heap::RelocationLock relocation_lock(heap());

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_SWEEP_NEWSPACE);
    EvacuationScope evacuation_scope(this);
    EvacuateNewSpace();
  }

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_EVACUATE_PAGES);
    EvacuationScope evacuation_scope(this);
    EvacuatePages();
  }

  // Second pass: find pointers to new space and update them.
  PointersUpdatingVisitor updating_visitor(heap());

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_UPDATE_NEW_TO_NEW_POINTERS);
    // Update pointers in to space.
    SemiSpaceIterator to_it(heap()->new_space()->bottom(),
                            heap()->new_space()->top());
    for (HeapObject* object = to_it.Next(); object != NULL;
         object = to_it.Next()) {
      Map* map = object->map();
      object->IterateBody(map->instance_type(), object->SizeFromMap(map),
                          &updating_visitor);
    }
  }

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_UPDATE_ROOT_TO_NEW_POINTERS);
    // Update roots.
    heap_->IterateRoots(&updating_visitor, VISIT_ALL_IN_SWEEP_NEWSPACE);
  }

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_UPDATE_OLD_TO_NEW_POINTERS);
    StoreBufferRebuildScope scope(heap_, heap_->store_buffer(),
                                  &Heap::ScavengeStoreBufferCallback);
    heap_->store_buffer()->IteratePointersToNewSpace(&UpdatePointer);
  }

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_UPDATE_POINTERS_TO_EVACUATED);
    SlotsBuffer::UpdateSlotsRecordedIn(heap_, migration_slots_buffer_);
    if (FLAG_trace_fragmentation_verbose) {
      PrintF("  migration slots buffer: %d\n",
             SlotsBuffer::SizeOfChain(migration_slots_buffer_));
    }

    if (compacting_ && was_marked_incrementally_) {
      // It's difficult to filter out slots recorded for large objects.
      LargeObjectIterator it(heap_->lo_space());
      for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
        // LargeObjectSpace is not swept yet thus we have to skip
        // dead objects explicitly.
        if (!IsMarked(obj)) continue;

        Page* p = Page::FromAddress(obj->address());
        if (p->IsFlagSet(Page::RESCAN_ON_EVACUATION)) {
          obj->Iterate(&updating_visitor);
          p->ClearFlag(Page::RESCAN_ON_EVACUATION);
        }
      }
    }
  }

  int npages = evacuation_candidates_.length();
  {
    GCTracer::Scope gc_scope(
        heap()->tracer(),
        GCTracer::Scope::MC_UPDATE_POINTERS_BETWEEN_EVACUATED);
    for (int i = 0; i < npages; i++) {
      Page* p = evacuation_candidates_[i];
      DCHECK(p->IsEvacuationCandidate() ||
             p->IsFlagSet(Page::RESCAN_ON_EVACUATION));

      if (p->IsEvacuationCandidate()) {
        SlotsBuffer::UpdateSlotsRecordedIn(heap_, p->slots_buffer());
        if (FLAG_trace_fragmentation_verbose) {
          PrintF("  page %p slots buffer: %d\n", reinterpret_cast<void*>(p),
                 SlotsBuffer::SizeOfChain(p->slots_buffer()));
        }

        // Important: skip list should be cleared only after roots were updated
        // because root iteration traverses the stack and might have to find
        // code objects from non-updated pc pointing into evacuation candidate.
        SkipList* list = p->skip_list();
        if (list != NULL) list->Clear();
      } else {
        if (FLAG_gc_verbose) {
          PrintF("Sweeping 0x%" V8PRIxPTR " during evacuation.\n",
                 reinterpret_cast<intptr_t>(p));
        }
        PagedSpace* space = static_cast<PagedSpace*>(p->owner());
        p->ClearFlag(MemoryChunk::RESCAN_ON_EVACUATION);

        switch (space->identity()) {
          case OLD_SPACE:
            Sweep<SWEEP_AND_VISIT_LIVE_OBJECTS, SWEEP_ON_MAIN_THREAD,
                  IGNORE_SKIP_LIST, IGNORE_FREE_SPACE>(space, NULL, p,
                                                       &updating_visitor);
            break;
          case CODE_SPACE:
            if (FLAG_zap_code_space) {
              Sweep<SWEEP_AND_VISIT_LIVE_OBJECTS, SWEEP_ON_MAIN_THREAD,
                    REBUILD_SKIP_LIST, ZAP_FREE_SPACE>(space, NULL, p,
                                                       &updating_visitor);
            } else {
              Sweep<SWEEP_AND_VISIT_LIVE_OBJECTS, SWEEP_ON_MAIN_THREAD,
                    REBUILD_SKIP_LIST, IGNORE_FREE_SPACE>(space, NULL, p,
                                                          &updating_visitor);
            }
            break;
          default:
            UNREACHABLE();
            break;
        }
      }
    }
  }

  GCTracer::Scope gc_scope(heap()->tracer(),
                           GCTracer::Scope::MC_UPDATE_MISC_POINTERS);

  heap_->string_table()->Iterate(&updating_visitor);

  // Update pointers from external string table.
  heap_->UpdateReferencesInExternalStringTable(
      &UpdateReferenceInExternalStringTableEntry);

  EvacuationWeakObjectRetainer evacuation_object_retainer;
  heap()->ProcessAllWeakReferences(&evacuation_object_retainer);

  heap_->isolate()->inner_pointer_to_code_cache()->Flush();

  slots_buffer_allocator_.DeallocateChain(&migration_slots_buffer_);
  DCHECK(migration_slots_buffer_ == NULL);

  // The hashing of weak_object_to_code_table is no longer valid.
  heap()->weak_object_to_code_table()->Rehash(
      heap()->isolate()->factory()->undefined_value());
}


void MarkCompactCollector::MoveEvacuationCandidatesToEndOfPagesList() {
  int npages = evacuation_candidates_.length();
  for (int i = 0; i < npages; i++) {
    Page* p = evacuation_candidates_[i];
    if (!p->IsEvacuationCandidate()) continue;
    p->Unlink();
    PagedSpace* space = static_cast<PagedSpace*>(p->owner());
    p->InsertAfter(space->LastPage());
  }
}


void MarkCompactCollector::ReleaseEvacuationCandidates() {
  int npages = evacuation_candidates_.length();
  for (int i = 0; i < npages; i++) {
    Page* p = evacuation_candidates_[i];
    if (!p->IsEvacuationCandidate()) continue;
    PagedSpace* space = static_cast<PagedSpace*>(p->owner());
    space->Free(p->area_start(), p->area_size());
    p->set_scan_on_scavenge(false);
    slots_buffer_allocator_.DeallocateChain(p->slots_buffer_address());
    p->ResetLiveBytes();
    space->ReleasePage(p);
  }
  evacuation_candidates_.Rewind(0);
  compacting_ = false;
  heap()->FreeQueuedChunks();
}


static const int kStartTableEntriesPerLine = 5;
static const int kStartTableLines = 171;
static const int kStartTableInvalidLine = 127;
static const int kStartTableUnusedEntry = 126;

#define _ kStartTableUnusedEntry
#define X kStartTableInvalidLine
// Mark-bit to object start offset table.
//
// The line is indexed by the mark bits in a byte.  The first number on
// the line describes the number of live object starts for the line and the
// other numbers on the line describe the offsets (in words) of the object
// starts.
//
// Since objects are at least 2 words large we don't have entries for two
// consecutive 1 bits.  All entries after 170 have at least 2 consecutive bits.
char kStartTable[kStartTableLines * kStartTableEntriesPerLine] = {
    0, _, _,
    _, _,  // 0
    1, 0, _,
    _, _,  // 1
    1, 1, _,
    _, _,  // 2
    X, _, _,
    _, _,  // 3
    1, 2, _,
    _, _,  // 4
    2, 0, 2,
    _, _,  // 5
    X, _, _,
    _, _,  // 6
    X, _, _,
    _, _,  // 7
    1, 3, _,
    _, _,  // 8
    2, 0, 3,
    _, _,  // 9
    2, 1, 3,
    _, _,  // 10
    X, _, _,
    _, _,  // 11
    X, _, _,
    _, _,  // 12
    X, _, _,
    _, _,  // 13
    X, _, _,
    _, _,  // 14
    X, _, _,
    _, _,  // 15
    1, 4, _,
    _, _,  // 16
    2, 0, 4,
    _, _,  // 17
    2, 1, 4,
    _, _,  // 18
    X, _, _,
    _, _,  // 19
    2, 2, 4,
    _, _,  // 20
    3, 0, 2,
    4, _,  // 21
    X, _, _,
    _, _,  // 22
    X, _, _,
    _, _,  // 23
    X, _, _,
    _, _,  // 24
    X, _, _,
    _, _,  // 25
    X, _, _,
    _, _,  // 26
    X, _, _,
    _, _,  // 27
    X, _, _,
    _, _,  // 28
    X, _, _,
    _, _,  // 29
    X, _, _,
    _, _,  // 30
    X, _, _,
    _, _,  // 31
    1, 5, _,
    _, _,  // 32
    2, 0, 5,
    _, _,  // 33
    2, 1, 5,
    _, _,  // 34
    X, _, _,
    _, _,  // 35
    2, 2, 5,
    _, _,  // 36
    3, 0, 2,
    5, _,  // 37
    X, _, _,
    _, _,  // 38
    X, _, _,
    _, _,  // 39
    2, 3, 5,
    _, _,  // 40
    3, 0, 3,
    5, _,  // 41
    3, 1, 3,
    5, _,  // 42
    X, _, _,
    _, _,  // 43
    X, _, _,
    _, _,  // 44
    X, _, _,
    _, _,  // 45
    X, _, _,
    _, _,  // 46
    X, _, _,
    _, _,  // 47
    X, _, _,
    _, _,  // 48
    X, _, _,
    _, _,  // 49
    X, _, _,
    _, _,  // 50
    X, _, _,
    _, _,  // 51
    X, _, _,
    _, _,  // 52
    X, _, _,
    _, _,  // 53
    X, _, _,
    _, _,  // 54
    X, _, _,
    _, _,  // 55
    X, _, _,
    _, _,  // 56
    X, _, _,
    _, _,  // 57
    X, _, _,
    _, _,  // 58
    X, _, _,
    _, _,  // 59
    X, _, _,
    _, _,  // 60
    X, _, _,
    _, _,  // 61
    X, _, _,
    _, _,  // 62
    X, _, _,
    _, _,  // 63
    1, 6, _,
    _, _,  // 64
    2, 0, 6,
    _, _,  // 65
    2, 1, 6,
    _, _,  // 66
    X, _, _,
    _, _,  // 67
    2, 2, 6,
    _, _,  // 68
    3, 0, 2,
    6, _,  // 69
    X, _, _,
    _, _,  // 70
    X, _, _,
    _, _,  // 71
    2, 3, 6,
    _, _,  // 72
    3, 0, 3,
    6, _,  // 73
    3, 1, 3,
    6, _,  // 74
    X, _, _,
    _, _,  // 75
    X, _, _,
    _, _,  // 76
    X, _, _,
    _, _,  // 77
    X, _, _,
    _, _,  // 78
    X, _, _,
    _, _,  // 79
    2, 4, 6,
    _, _,  // 80
    3, 0, 4,
    6, _,  // 81
    3, 1, 4,
    6, _,  // 82
    X, _, _,
    _, _,  // 83
    3, 2, 4,
    6, _,  // 84
    4, 0, 2,
    4, 6,  // 85
    X, _, _,
    _, _,  // 86
    X, _, _,
    _, _,  // 87
    X, _, _,
    _, _,  // 88
    X, _, _,
    _, _,  // 89
    X, _, _,
    _, _,  // 90
    X, _, _,
    _, _,  // 91
    X, _, _,
    _, _,  // 92
    X, _, _,
    _, _,  // 93
    X, _, _,
    _, _,  // 94
    X, _, _,
    _, _,  // 95
    X, _, _,
    _, _,  // 96
    X, _, _,
    _, _,  // 97
    X, _, _,
    _, _,  // 98
    X, _, _,
    _, _,  // 99
    X, _, _,
    _, _,  // 100
    X, _, _,
    _, _,  // 101
    X, _, _,
    _, _,  // 102
    X, _, _,
    _, _,  // 103
    X, _, _,
    _, _,  // 104
    X, _, _,
    _, _,  // 105
    X, _, _,
    _, _,  // 106
    X, _, _,
    _, _,  // 107
    X, _, _,
    _, _,  // 108
    X, _, _,
    _, _,  // 109
    X, _, _,
    _, _,  // 110
    X, _, _,
    _, _,  // 111
    X, _, _,
    _, _,  // 112
    X, _, _,
    _, _,  // 113
    X, _, _,
    _, _,  // 114
    X, _, _,
    _, _,  // 115
    X, _, _,
    _, _,  // 116
    X, _, _,
    _, _,  // 117
    X, _, _,
    _, _,  // 118
    X, _, _,
    _, _,  // 119
    X, _, _,
    _, _,  // 120
    X, _, _,
    _, _,  // 121
    X, _, _,
    _, _,  // 122
    X, _, _,
    _, _,  // 123
    X, _, _,
    _, _,  // 124
    X, _, _,
    _, _,  // 125
    X, _, _,
    _, _,  // 126
    X, _, _,
    _, _,  // 127
    1, 7, _,
    _, _,  // 128
    2, 0, 7,
    _, _,  // 129
    2, 1, 7,
    _, _,  // 130
    X, _, _,
    _, _,  // 131
    2, 2, 7,
    _, _,  // 132
    3, 0, 2,
    7, _,  // 133
    X, _, _,
    _, _,  // 134
    X, _, _,
    _, _,  // 135
    2, 3, 7,
    _, _,  // 136
    3, 0, 3,
    7, _,  // 137
    3, 1, 3,
    7, _,  // 138
    X, _, _,
    _, _,  // 139
    X, _, _,
    _, _,  // 140
    X, _, _,
    _, _,  // 141
    X, _, _,
    _, _,  // 142
    X, _, _,
    _, _,  // 143
    2, 4, 7,
    _, _,  // 144
    3, 0, 4,
    7, _,  // 145
    3, 1, 4,
    7, _,  // 146
    X, _, _,
    _, _,  // 147
    3, 2, 4,
    7, _,  // 148
    4, 0, 2,
    4, 7,  // 149
    X, _, _,
    _, _,  // 150
    X, _, _,
    _, _,  // 151
    X, _, _,
    _, _,  // 152
    X, _, _,
    _, _,  // 153
    X, _, _,
    _, _,  // 154
    X, _, _,
    _, _,  // 155
    X, _, _,
    _, _,  // 156
    X, _, _,
    _, _,  // 157
    X, _, _,
    _, _,  // 158
    X, _, _,
    _, _,  // 159
    2, 5, 7,
    _, _,  // 160
    3, 0, 5,
    7, _,  // 161
    3, 1, 5,
    7, _,  // 162
    X, _, _,
    _, _,  // 163
    3, 2, 5,
    7, _,  // 164
    4, 0, 2,
    5, 7,  // 165
    X, _, _,
    _, _,  // 166
    X, _, _,
    _, _,  // 167
    3, 3, 5,
    7, _,  // 168
    4, 0, 3,
    5, 7,  // 169
    4, 1, 3,
    5, 7  // 170
};
#undef _
#undef X


// Takes a word of mark bits.  Returns the number of objects that start in the
// range.  Puts the offsets of the words in the supplied array.
static inline int MarkWordToObjectStarts(uint32_t mark_bits, int* starts) {
  int objects = 0;
  int offset = 0;

  // No consecutive 1 bits.
  DCHECK((mark_bits & 0x180) != 0x180);
  DCHECK((mark_bits & 0x18000) != 0x18000);
  DCHECK((mark_bits & 0x1800000) != 0x1800000);

  while (mark_bits != 0) {
    int byte = (mark_bits & 0xff);
    mark_bits >>= 8;
    if (byte != 0) {
      DCHECK(byte < kStartTableLines);  // No consecutive 1 bits.
      char* table = kStartTable + byte * kStartTableEntriesPerLine;
      int objects_in_these_8_words = table[0];
      DCHECK(objects_in_these_8_words != kStartTableInvalidLine);
      DCHECK(objects_in_these_8_words < kStartTableEntriesPerLine);
      for (int i = 0; i < objects_in_these_8_words; i++) {
        starts[objects++] = offset + table[1 + i];
      }
    }
    offset += 8;
  }
  return objects;
}


int MarkCompactCollector::SweepInParallel(PagedSpace* space,
                                          int required_freed_bytes) {
  int max_freed = 0;
  int max_freed_overall = 0;
  PageIterator it(space);
  while (it.has_next()) {
    Page* p = it.next();
    max_freed = SweepInParallel(p, space);
    DCHECK(max_freed >= 0);
    if (required_freed_bytes > 0 && max_freed >= required_freed_bytes) {
      return max_freed;
    }
    max_freed_overall = Max(max_freed, max_freed_overall);
    if (p == space->end_of_unswept_pages()) break;
  }
  return max_freed_overall;
}


int MarkCompactCollector::SweepInParallel(Page* page, PagedSpace* space) {
  int max_freed = 0;
  if (page->TryParallelSweeping()) {
    FreeList* free_list = free_list_old_space_.get();
    FreeList private_free_list(space);
    max_freed = Sweep<SWEEP_ONLY, SWEEP_IN_PARALLEL, IGNORE_SKIP_LIST,
                      IGNORE_FREE_SPACE>(space, &private_free_list, page, NULL);
    free_list->Concatenate(&private_free_list);
  }
  return max_freed;
}


void MarkCompactCollector::SweepSpace(PagedSpace* space, SweeperType sweeper) {
  space->ClearStats();

  // We defensively initialize end_of_unswept_pages_ here with the first page
  // of the pages list.
  space->set_end_of_unswept_pages(space->FirstPage());

  PageIterator it(space);

  int pages_swept = 0;
  bool unused_page_present = false;
  bool parallel_sweeping_active = false;

  while (it.has_next()) {
    Page* p = it.next();
    DCHECK(p->parallel_sweeping() == MemoryChunk::SWEEPING_DONE);

    // Clear sweeping flags indicating that marking bits are still intact.
    p->ClearWasSwept();

    if (p->IsFlagSet(Page::RESCAN_ON_EVACUATION) ||
        p->IsEvacuationCandidate()) {
      // Will be processed in EvacuateNewSpaceAndCandidates.
      DCHECK(evacuation_candidates_.length() > 0);
      continue;
    }

    // One unused page is kept, all further are released before sweeping them.
    if (p->LiveBytes() == 0) {
      if (unused_page_present) {
        if (FLAG_gc_verbose) {
          PrintF("Sweeping 0x%" V8PRIxPTR " released page.\n",
                 reinterpret_cast<intptr_t>(p));
        }
        // Adjust unswept free bytes because releasing a page expects said
        // counter to be accurate for unswept pages.
        space->IncreaseUnsweptFreeBytes(p);
        space->ReleasePage(p);
        continue;
      }
      unused_page_present = true;
    }

    switch (sweeper) {
      case CONCURRENT_SWEEPING:
        if (!parallel_sweeping_active) {
          if (FLAG_gc_verbose) {
            PrintF("Sweeping 0x%" V8PRIxPTR ".\n",
                   reinterpret_cast<intptr_t>(p));
          }
          Sweep<SWEEP_ONLY, SWEEP_ON_MAIN_THREAD, IGNORE_SKIP_LIST,
                IGNORE_FREE_SPACE>(space, NULL, p, NULL);
          pages_swept++;
          parallel_sweeping_active = true;
        } else {
          if (FLAG_gc_verbose) {
            PrintF("Sweeping 0x%" V8PRIxPTR " in parallel.\n",
                   reinterpret_cast<intptr_t>(p));
          }
          p->set_parallel_sweeping(MemoryChunk::SWEEPING_PENDING);
          space->IncreaseUnsweptFreeBytes(p);
        }
        space->set_end_of_unswept_pages(p);
        break;
      case SEQUENTIAL_SWEEPING: {
        if (FLAG_gc_verbose) {
          PrintF("Sweeping 0x%" V8PRIxPTR ".\n", reinterpret_cast<intptr_t>(p));
        }
        if (space->identity() == CODE_SPACE && FLAG_zap_code_space) {
          Sweep<SWEEP_ONLY, SWEEP_ON_MAIN_THREAD, REBUILD_SKIP_LIST,
                ZAP_FREE_SPACE>(space, NULL, p, NULL);
        } else if (space->identity() == CODE_SPACE) {
          Sweep<SWEEP_ONLY, SWEEP_ON_MAIN_THREAD, REBUILD_SKIP_LIST,
                IGNORE_FREE_SPACE>(space, NULL, p, NULL);
        } else {
          Sweep<SWEEP_ONLY, SWEEP_ON_MAIN_THREAD, IGNORE_SKIP_LIST,
                IGNORE_FREE_SPACE>(space, NULL, p, NULL);
        }
        pages_swept++;
        break;
      }
      default: { UNREACHABLE(); }
    }
  }

  if (FLAG_gc_verbose) {
    PrintF("SweepSpace: %s (%d pages swept)\n",
           AllocationSpaceName(space->identity()), pages_swept);
  }

  // Give pages that are queued to be freed back to the OS.
  heap()->FreeQueuedChunks();
}


void MarkCompactCollector::SweepSpaces() {
  GCTracer::Scope gc_scope(heap()->tracer(), GCTracer::Scope::MC_SWEEP);
  double start_time = 0.0;
  if (FLAG_print_cumulative_gc_stat) {
    start_time = base::OS::TimeCurrentMillis();
  }

#ifdef DEBUG
  state_ = SWEEP_SPACES;
#endif

  MoveEvacuationCandidatesToEndOfPagesList();

  // Noncompacting collections simply sweep the spaces to clear the mark
  // bits and free the nonlive blocks (for old and map spaces).  We sweep
  // the map space last because freeing non-live maps overwrites them and
  // the other spaces rely on possibly non-live maps to get the sizes for
  // non-live objects.
  {
    GCTracer::Scope sweep_scope(heap()->tracer(),
                                GCTracer::Scope::MC_SWEEP_OLDSPACE);
    { SweepSpace(heap()->old_space(), CONCURRENT_SWEEPING); }
    sweeping_in_progress_ = true;
    if (heap()->concurrent_sweeping_enabled()) {
      StartSweeperThreads();
    }
  }
  {
    GCTracer::Scope sweep_scope(heap()->tracer(),
                                GCTracer::Scope::MC_SWEEP_CODE);
    SweepSpace(heap()->code_space(), SEQUENTIAL_SWEEPING);
  }

  EvacuateNewSpaceAndCandidates();

  // NOTE: ArrayBuffers must be evacuated first, before freeing them. Otherwise
  // not yet discovered buffers for scavenge will have all of them, and they
  // will be erroneously freed.
  heap()->FreeDeadArrayBuffers(false);

  // ClearNonLiveReferences depends on precise sweeping of map space to
  // detect whether unmarked map became dead in this collection or in one
  // of the previous ones.
  {
    GCTracer::Scope sweep_scope(heap()->tracer(),
                                GCTracer::Scope::MC_SWEEP_MAP);
    SweepSpace(heap()->map_space(), SEQUENTIAL_SWEEPING);
  }

  // Deallocate unmarked objects and clear marked bits for marked objects.
  heap_->lo_space()->FreeUnmarkedObjects();

  // Deallocate evacuated candidate pages.
  ReleaseEvacuationCandidates();
  CodeRange* code_range = heap()->isolate()->code_range();
  if (code_range != NULL && code_range->valid()) {
    code_range->ReserveEmergencyBlock();
  }

  if (FLAG_print_cumulative_gc_stat) {
    heap_->tracer()->AddSweepingTime(base::OS::TimeCurrentMillis() -
                                     start_time);
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && !sweeping_in_progress_) {
    VerifyEvacuation(heap());
  }
#endif
}


void MarkCompactCollector::ParallelSweepSpaceComplete(PagedSpace* space) {
  PageIterator it(space);
  while (it.has_next()) {
    Page* p = it.next();
    if (p->parallel_sweeping() == MemoryChunk::SWEEPING_FINALIZE) {
      p->set_parallel_sweeping(MemoryChunk::SWEEPING_DONE);
      p->SetWasSwept();
    }
    DCHECK(p->parallel_sweeping() == MemoryChunk::SWEEPING_DONE);
  }
}


void MarkCompactCollector::ParallelSweepSpacesComplete() {
  ParallelSweepSpaceComplete(heap()->old_space());
}


void MarkCompactCollector::EnableCodeFlushing(bool enable) {
  if (isolate()->debug()->is_loaded() ||
      isolate()->debug()->has_break_points()) {
    enable = false;
  }

  if (enable) {
    if (code_flusher_ != NULL) return;
    code_flusher_ = new CodeFlusher(isolate());
  } else {
    if (code_flusher_ == NULL) return;
    code_flusher_->EvictAllCandidates();
    delete code_flusher_;
    code_flusher_ = NULL;
  }

  if (FLAG_trace_code_flushing) {
    PrintF("[code-flushing is now %s]\n", enable ? "on" : "off");
  }
}


// TODO(1466) ReportDeleteIfNeeded is not called currently.
// Our profiling tools do not expect intersections between
// code objects. We should either reenable it or change our tools.
void MarkCompactCollector::ReportDeleteIfNeeded(HeapObject* obj,
                                                Isolate* isolate) {
  if (obj->IsCode()) {
    PROFILE(isolate, CodeDeleteEvent(obj->address()));
  }
}


Isolate* MarkCompactCollector::isolate() const { return heap_->isolate(); }


void MarkCompactCollector::Initialize() {
  MarkCompactMarkingVisitor::Initialize();
  IncrementalMarking::Initialize();
}


bool SlotsBuffer::IsTypedSlot(ObjectSlot slot) {
  return reinterpret_cast<uintptr_t>(slot) < NUMBER_OF_SLOT_TYPES;
}


bool SlotsBuffer::AddTo(SlotsBufferAllocator* allocator,
                        SlotsBuffer** buffer_address, SlotType type,
                        Address addr, AdditionMode mode) {
  SlotsBuffer* buffer = *buffer_address;
  if (buffer == NULL || !buffer->HasSpaceForTypedSlot()) {
    if (mode == FAIL_ON_OVERFLOW && ChainLengthThresholdReached(buffer)) {
      allocator->DeallocateChain(buffer_address);
      return false;
    }
    buffer = allocator->AllocateBuffer(buffer);
    *buffer_address = buffer;
  }
  DCHECK(buffer->HasSpaceForTypedSlot());
  buffer->Add(reinterpret_cast<ObjectSlot>(type));
  buffer->Add(reinterpret_cast<ObjectSlot>(addr));
  return true;
}


void SlotsBuffer::RemoveInvalidSlots(Heap* heap, SlotsBuffer* buffer) {
  // Remove entries by replacing them with an old-space slot containing a smi
  // that is located in an unmovable page.
  const ObjectSlot kRemovedEntry = HeapObject::RawField(
      heap->empty_fixed_array(), FixedArrayBase::kLengthOffset);
  DCHECK(Page::FromAddress(reinterpret_cast<Address>(kRemovedEntry))
             ->NeverEvacuate());

  while (buffer != NULL) {
    SlotsBuffer::ObjectSlot* slots = buffer->slots_;
    intptr_t slots_count = buffer->idx_;

    for (int slot_idx = 0; slot_idx < slots_count; ++slot_idx) {
      ObjectSlot slot = slots[slot_idx];
      if (!IsTypedSlot(slot)) {
        Object* object = *slot;
        if ((object->IsHeapObject() && heap->InNewSpace(object)) ||
            !heap->mark_compact_collector()->IsSlotInLiveObject(
                reinterpret_cast<Address>(slot))) {
          slots[slot_idx] = kRemovedEntry;
        }
      } else {
        ++slot_idx;
        DCHECK(slot_idx < slots_count);
      }
    }
    buffer = buffer->next();
  }
}


void SlotsBuffer::RemoveObjectSlots(Heap* heap, SlotsBuffer* buffer,
                                    Address start_slot, Address end_slot) {
  // Remove entries by replacing them with an old-space slot containing a smi
  // that is located in an unmovable page.
  const ObjectSlot kRemovedEntry = HeapObject::RawField(
      heap->empty_fixed_array(), FixedArrayBase::kLengthOffset);
  DCHECK(Page::FromAddress(reinterpret_cast<Address>(kRemovedEntry))
             ->NeverEvacuate());

  while (buffer != NULL) {
    SlotsBuffer::ObjectSlot* slots = buffer->slots_;
    intptr_t slots_count = buffer->idx_;
    bool is_typed_slot = false;

    for (int slot_idx = 0; slot_idx < slots_count; ++slot_idx) {
      ObjectSlot slot = slots[slot_idx];
      if (!IsTypedSlot(slot)) {
        Address slot_address = reinterpret_cast<Address>(slot);
        if (slot_address >= start_slot && slot_address < end_slot) {
          slots[slot_idx] = kRemovedEntry;
          if (is_typed_slot) {
            slots[slot_idx - 1] = kRemovedEntry;
          }
        }
        is_typed_slot = false;
      } else {
        is_typed_slot = true;
        DCHECK(slot_idx < slots_count);
      }
    }
    buffer = buffer->next();
  }
}


void SlotsBuffer::VerifySlots(Heap* heap, SlotsBuffer* buffer) {
  while (buffer != NULL) {
    SlotsBuffer::ObjectSlot* slots = buffer->slots_;
    intptr_t slots_count = buffer->idx_;

    for (int slot_idx = 0; slot_idx < slots_count; ++slot_idx) {
      ObjectSlot slot = slots[slot_idx];
      if (!IsTypedSlot(slot)) {
        Object* object = *slot;
        if (object->IsHeapObject()) {
          HeapObject* heap_object = HeapObject::cast(object);
          CHECK(!heap->InNewSpace(object));
          heap->mark_compact_collector()->VerifyIsSlotInLiveObject(
              reinterpret_cast<Address>(slot), heap_object);
        }
      } else {
        ++slot_idx;
        DCHECK(slot_idx < slots_count);
      }
    }
    buffer = buffer->next();
  }
}


static inline SlotsBuffer::SlotType SlotTypeForRMode(RelocInfo::Mode rmode) {
  if (RelocInfo::IsCodeTarget(rmode)) {
    return SlotsBuffer::CODE_TARGET_SLOT;
  } else if (RelocInfo::IsCell(rmode)) {
    return SlotsBuffer::CELL_TARGET_SLOT;
  } else if (RelocInfo::IsEmbeddedObject(rmode)) {
    return SlotsBuffer::EMBEDDED_OBJECT_SLOT;
  } else if (RelocInfo::IsDebugBreakSlot(rmode)) {
    return SlotsBuffer::DEBUG_TARGET_SLOT;
  } else if (RelocInfo::IsJSReturn(rmode)) {
    return SlotsBuffer::JS_RETURN_SLOT;
  }
  UNREACHABLE();
  return SlotsBuffer::NUMBER_OF_SLOT_TYPES;
}


void MarkCompactCollector::RecordRelocSlot(RelocInfo* rinfo, Object* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  RelocInfo::Mode rmode = rinfo->rmode();
  if (target_page->IsEvacuationCandidate() &&
      (rinfo->host() == NULL ||
       !ShouldSkipEvacuationSlotRecording(rinfo->host()))) {
    Address addr = rinfo->pc();
    SlotsBuffer::SlotType slot_type = SlotTypeForRMode(rmode);
    if (rinfo->IsInConstantPool()) {
      addr = rinfo->constant_pool_entry_address();
      if (RelocInfo::IsCodeTarget(rmode)) {
        slot_type = SlotsBuffer::CODE_ENTRY_SLOT;
      } else {
        DCHECK(RelocInfo::IsEmbeddedObject(rmode));
        slot_type = SlotsBuffer::OBJECT_SLOT;
      }
    }
    bool success = SlotsBuffer::AddTo(
        &slots_buffer_allocator_, target_page->slots_buffer_address(),
        slot_type, addr, SlotsBuffer::FAIL_ON_OVERFLOW);
    if (!success) {
      EvictPopularEvacuationCandidate(target_page);
    }
  }
}


void MarkCompactCollector::EvictPopularEvacuationCandidate(Page* page) {
  if (FLAG_trace_fragmentation) {
    PrintF("Page %p is too popular. Disabling evacuation.\n",
           reinterpret_cast<void*>(page));
  }

  isolate()->CountUsage(v8::Isolate::UseCounterFeature::kSlotsBufferOverflow);

  // TODO(gc) If all evacuation candidates are too popular we
  // should stop slots recording entirely.
  page->ClearEvacuationCandidate();

  DCHECK(!page->IsFlagSet(Page::POPULAR_PAGE));
  page->SetFlag(Page::POPULAR_PAGE);

  // We were not collecting slots on this page that point
  // to other evacuation candidates thus we have to
  // rescan the page after evacuation to discover and update all
  // pointers to evacuated objects.
  page->SetFlag(Page::RESCAN_ON_EVACUATION);
}


void MarkCompactCollector::RecordCodeEntrySlot(Address slot, Code* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  if (target_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(reinterpret_cast<Object**>(slot))) {
    if (!SlotsBuffer::AddTo(&slots_buffer_allocator_,
                            target_page->slots_buffer_address(),
                            SlotsBuffer::CODE_ENTRY_SLOT, slot,
                            SlotsBuffer::FAIL_ON_OVERFLOW)) {
      EvictPopularEvacuationCandidate(target_page);
    }
  }
}


void MarkCompactCollector::RecordCodeTargetPatch(Address pc, Code* target) {
  DCHECK(heap()->gc_state() == Heap::MARK_COMPACT);
  if (is_compacting()) {
    Code* host =
        isolate()->inner_pointer_to_code_cache()->GcSafeFindCodeForInnerPointer(
            pc);
    MarkBit mark_bit = Marking::MarkBitFrom(host);
    if (Marking::IsBlack(mark_bit)) {
      RelocInfo rinfo(pc, RelocInfo::CODE_TARGET, 0, host);
      RecordRelocSlot(&rinfo, target);
    }
  }
}


static inline SlotsBuffer::SlotType DecodeSlotType(
    SlotsBuffer::ObjectSlot slot) {
  return static_cast<SlotsBuffer::SlotType>(reinterpret_cast<intptr_t>(slot));
}


void SlotsBuffer::UpdateSlots(Heap* heap) {
  PointersUpdatingVisitor v(heap);

  for (int slot_idx = 0; slot_idx < idx_; ++slot_idx) {
    ObjectSlot slot = slots_[slot_idx];
    if (!IsTypedSlot(slot)) {
      PointersUpdatingVisitor::UpdateSlot(heap, slot);
    } else {
      ++slot_idx;
      DCHECK(slot_idx < idx_);
      UpdateSlot(heap->isolate(), &v, DecodeSlotType(slot),
                 reinterpret_cast<Address>(slots_[slot_idx]));
    }
  }
}


SlotsBuffer* SlotsBufferAllocator::AllocateBuffer(SlotsBuffer* next_buffer) {
  return new SlotsBuffer(next_buffer);
}


void SlotsBufferAllocator::DeallocateBuffer(SlotsBuffer* buffer) {
  delete buffer;
}


void SlotsBufferAllocator::DeallocateChain(SlotsBuffer** buffer_address) {
  SlotsBuffer* buffer = *buffer_address;
  while (buffer != NULL) {
    SlotsBuffer* next_buffer = buffer->next();
    DeallocateBuffer(buffer);
    buffer = next_buffer;
  }
  *buffer_address = NULL;
}
}  // namespace internal
}  // namespace v8

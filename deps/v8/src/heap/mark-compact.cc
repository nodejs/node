// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/mark-compact.h"

#include "src/base/atomicops.h"
#include "src/base/bits.h"
#include "src/base/sys-info.h"
#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/frames-inl.h"
#include "src/gdb-jit.h"
#include "src/global-handles.h"
#include "src/heap/array-buffer-tracker.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/slots-buffer.h"
#include "src/heap/spaces-inl.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/profiler/cpu-profiler.h"
#include "src/utils-inl.h"
#include "src/v8.h"

namespace v8 {
namespace internal {


const char* Marking::kWhiteBitPattern = "00";
const char* Marking::kBlackBitPattern = "11";
const char* Marking::kGreyBitPattern = "10";
const char* Marking::kImpossibleBitPattern = "01";


// The following has to hold in order for {Marking::MarkBitFrom} to not produce
// invalid {kImpossibleBitPattern} in the marking bitmap by overlapping.
STATIC_ASSERT(Heap::kMinObjectSizeInWords >= 2);


// -------------------------------------------------------------------------
// MarkCompactCollector

MarkCompactCollector::MarkCompactCollector(Heap* heap)
    :  // NOLINT
#ifdef DEBUG
      state_(IDLE),
#endif
      marking_parity_(ODD_MARKING_PARITY),
      was_marked_incrementally_(false),
      evacuation_(false),
      slots_buffer_allocator_(nullptr),
      migration_slots_buffer_(nullptr),
      heap_(heap),
      marking_deque_memory_(NULL),
      marking_deque_memory_committed_(0),
      code_flusher_(nullptr),
      have_code_to_deoptimize_(false),
      compacting_(false),
      sweeping_in_progress_(false),
      compaction_in_progress_(false),
      pending_sweeper_tasks_semaphore_(0),
      pending_compaction_tasks_semaphore_(0) {
}

#ifdef VERIFY_HEAP
class VerifyMarkingVisitor : public ObjectVisitor {
 public:
  explicit VerifyMarkingVisitor(Heap* heap) : heap_(heap) {}

  void VisitPointers(Object** start, Object** end) override {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        CHECK(heap_->mark_compact_collector()->IsMarked(object));
      }
    }
  }

  void VisitEmbeddedPointer(RelocInfo* rinfo) override {
    DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
    if (!rinfo->host()->IsWeakObject(rinfo->target_object())) {
      Object* p = rinfo->target_object();
      VisitPointer(&p);
    }
  }

  void VisitCell(RelocInfo* rinfo) override {
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
      // The next word for sure belongs to the current object, jump over it.
      current += kPointerSize;
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
  void VisitPointers(Object** start, Object** end) override {
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
  HeapObjectIterator iterator(page);
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
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  free_list_old_space_.Reset(new FreeList(heap_->old_space()));
  free_list_code_space_.Reset(new FreeList(heap_->code_space()));
  free_list_map_space_.Reset(new FreeList(heap_->map_space()));
  EnsureMarkingDequeIsReserved();
  EnsureMarkingDequeIsCommitted(kMinMarkingDequeSize);
  slots_buffer_allocator_ = new SlotsBufferAllocator();

  if (FLAG_flush_code) {
    code_flusher_ = new CodeFlusher(isolate());
    if (FLAG_trace_code_flushing) {
      PrintF("[code-flushing is now on]\n");
    }
  }
}


void MarkCompactCollector::TearDown() {
  AbortCompaction();
  delete marking_deque_memory_;
  delete slots_buffer_allocator_;
  delete code_flusher_;
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

    heap()->old_space()->EvictEvacuationCandidatesFromLinearAllocationArea();
    heap()->code_space()->EvictEvacuationCandidatesFromLinearAllocationArea();

    compacting_ = evacuation_candidates_.length() > 0;
  }

  return compacting_;
}


void MarkCompactCollector::ClearInvalidStoreAndSlotsBufferEntries() {
  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_CLEAR_STORE_BUFFER);
    RememberedSet<OLD_TO_NEW>::ClearInvalidSlots(heap());
  }

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_CLEAR_SLOTS_BUFFER);
    for (Page* p : evacuation_candidates_) {
      SlotsBuffer::RemoveInvalidSlots(heap_, p->slots_buffer());
    }
  }
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    VerifyValidStoreAndSlotsBufferEntries();
  }
#endif
}


#ifdef VERIFY_HEAP
static void VerifyValidSlotsBufferEntries(Heap* heap, PagedSpace* space) {
  PageIterator it(space);
  while (it.has_next()) {
    Page* p = it.next();
    SlotsBuffer::VerifySlots(heap, p->slots_buffer());
  }
}


void MarkCompactCollector::VerifyValidStoreAndSlotsBufferEntries() {
  RememberedSet<OLD_TO_NEW>::VerifyValidSlots(heap());

  VerifyValidSlotsBufferEntries(heap(), heap()->old_space());
  VerifyValidSlotsBufferEntries(heap(), heap()->code_space());
  VerifyValidSlotsBufferEntries(heap(), heap()->map_space());

  LargeObjectIterator it(heap()->lo_space());
  for (HeapObject* object = it.Next(); object != NULL; object = it.Next()) {
    MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
    SlotsBuffer::VerifySlots(heap(), chunk->slots_buffer());
  }
}
#endif


void MarkCompactCollector::CollectGarbage() {
  // Make sure that Prepare() has been called. The individual steps below will
  // update the state as they proceed.
  DCHECK(state_ == PREPARE_GC);

  MarkLiveObjects();

  DCHECK(heap_->incremental_marking()->IsStopped());

  ClearNonLiveReferences();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    VerifyMarking(heap_);
  }
#endif

  SweepSpaces();

  EvacuateNewSpaceAndCandidates();

  Finish();
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
  SweeperTask(Heap* heap, AllocationSpace space_to_start)
      : heap_(heap), space_to_start_(space_to_start) {}

  virtual ~SweeperTask() {}

 private:
  // v8::Task overrides.
  void Run() override {
    DCHECK_GE(space_to_start_, FIRST_PAGED_SPACE);
    DCHECK_LE(space_to_start_, LAST_PAGED_SPACE);
    const int offset = space_to_start_ - FIRST_PAGED_SPACE;
    const int num_spaces = LAST_PAGED_SPACE - FIRST_PAGED_SPACE + 1;
    for (int i = 0; i < num_spaces; i++) {
      const int space_id = FIRST_PAGED_SPACE + ((i + offset) % num_spaces);
      DCHECK_GE(space_id, FIRST_PAGED_SPACE);
      DCHECK_LE(space_id, LAST_PAGED_SPACE);
      heap_->mark_compact_collector()->SweepInParallel(
          heap_->paged_space(space_id), 0);
    }
    heap_->mark_compact_collector()->pending_sweeper_tasks_semaphore_.Signal();
  }

  Heap* heap_;
  AllocationSpace space_to_start_;

  DISALLOW_COPY_AND_ASSIGN(SweeperTask);
};


void MarkCompactCollector::StartSweeperThreads() {
  DCHECK(free_list_old_space_.get()->IsEmpty());
  DCHECK(free_list_code_space_.get()->IsEmpty());
  DCHECK(free_list_map_space_.get()->IsEmpty());
  V8::GetCurrentPlatform()->CallOnBackgroundThread(
      new SweeperTask(heap(), OLD_SPACE), v8::Platform::kShortRunningTask);
  V8::GetCurrentPlatform()->CallOnBackgroundThread(
      new SweeperTask(heap(), CODE_SPACE), v8::Platform::kShortRunningTask);
  V8::GetCurrentPlatform()->CallOnBackgroundThread(
      new SweeperTask(heap(), MAP_SPACE), v8::Platform::kShortRunningTask);
}


void MarkCompactCollector::SweepOrWaitUntilSweepingCompleted(Page* page) {
  PagedSpace* owner = reinterpret_cast<PagedSpace*>(page->owner());
  if (!page->SweepingDone()) {
    SweepInParallel(page, owner);
    if (!page->SweepingDone()) {
      // We were not able to sweep that page, i.e., a concurrent
      // sweeper thread currently owns this page. Wait for the sweeper
      // thread to be done with this page.
      page->WaitUntilSweepingCompleted();
    }
  }
}


void MarkCompactCollector::SweepAndRefill(CompactionSpace* space) {
  if (FLAG_concurrent_sweeping && !IsSweepingCompleted()) {
    SweepInParallel(heap()->paged_space(space->identity()), 0);
    space->RefillFreeList();
  }
}


void MarkCompactCollector::EnsureSweepingCompleted() {
  DCHECK(sweeping_in_progress_ == true);

  // If sweeping is not completed or not running at all, we try to complete it
  // here.
  if (!FLAG_concurrent_sweeping || !IsSweepingCompleted()) {
    SweepInParallel(heap()->paged_space(OLD_SPACE), 0);
    SweepInParallel(heap()->paged_space(CODE_SPACE), 0);
    SweepInParallel(heap()->paged_space(MAP_SPACE), 0);
  }

  if (FLAG_concurrent_sweeping) {
    pending_sweeper_tasks_semaphore_.Wait();
    pending_sweeper_tasks_semaphore_.Wait();
    pending_sweeper_tasks_semaphore_.Wait();
  }

  ParallelSweepSpacesComplete();
  sweeping_in_progress_ = false;
  heap()->old_space()->RefillFreeList();
  heap()->code_space()->RefillFreeList();
  heap()->map_space()->RefillFreeList();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && !evacuation()) {
    VerifyEvacuation(heap_);
  }
#endif
}


bool MarkCompactCollector::IsSweepingCompleted() {
  if (!pending_sweeper_tasks_semaphore_.WaitFor(
          base::TimeDelta::FromSeconds(0))) {
    return false;
  }
  pending_sweeper_tasks_semaphore_.Signal();
  return true;
}


void Marking::TransferMark(Heap* heap, Address old_start, Address new_start) {
  // This is only used when resizing an object.
  DCHECK(MemoryChunk::FromAddress(old_start) ==
         MemoryChunk::FromAddress(new_start));

  if (!heap->incremental_marking()->IsMarking()) return;

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
    heap->incremental_marking()->WhiteToGreyAndPush(
        HeapObject::FromAddress(new_start), new_mark_bit);
    heap->incremental_marking()->RestartIfNotMarking();
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


void MarkCompactCollector::ComputeEvacuationHeuristics(
    int area_size, int* target_fragmentation_percent,
    int* max_evacuated_bytes) {
  // For memory reducing mode we directly define both constants.
  const int kTargetFragmentationPercentForReduceMemory = 20;
  const int kMaxEvacuatedBytesForReduceMemory = 12 * Page::kPageSize;

  // For regular mode (which is latency critical) we define less aggressive
  // defaults to start and switch to a trace-based (using compaction speed)
  // approach as soon as we have enough samples.
  const int kTargetFragmentationPercent = 70;
  const int kMaxEvacuatedBytes = 4 * Page::kPageSize;
  // Time to take for a single area (=payload of page). Used as soon as there
  // exist enough compaction speed samples.
  const int kTargetMsPerArea = 1;

  if (heap()->ShouldReduceMemory()) {
    *target_fragmentation_percent = kTargetFragmentationPercentForReduceMemory;
    *max_evacuated_bytes = kMaxEvacuatedBytesForReduceMemory;
  } else {
    const intptr_t estimated_compaction_speed =
        heap()->tracer()->CompactionSpeedInBytesPerMillisecond();
    if (estimated_compaction_speed != 0) {
      // Estimate the target fragmentation based on traced compaction speed
      // and a goal for a single page.
      const intptr_t estimated_ms_per_area =
          1 + static_cast<intptr_t>(area_size) / estimated_compaction_speed;
      *target_fragmentation_percent =
          100 - 100 * kTargetMsPerArea / estimated_ms_per_area;
      if (*target_fragmentation_percent <
          kTargetFragmentationPercentForReduceMemory) {
        *target_fragmentation_percent =
            kTargetFragmentationPercentForReduceMemory;
      }
    } else {
      *target_fragmentation_percent = kTargetFragmentationPercent;
    }
    *max_evacuated_bytes = kMaxEvacuatedBytes;
  }
}


void MarkCompactCollector::CollectEvacuationCandidates(PagedSpace* space) {
  DCHECK(space->identity() == OLD_SPACE || space->identity() == CODE_SPACE);

  int number_of_pages = space->CountTotalPages();
  int area_size = space->AreaSize();

  // Pairs of (live_bytes_in_page, page).
  typedef std::pair<int, Page*> LiveBytesPagePair;
  std::vector<LiveBytesPagePair> pages;
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
    // started. This means that sweeping has finished. Furthermore, at the end
    // of a GC all evacuation candidates are cleared and their slot buffers are
    // released.
    CHECK(!p->IsEvacuationCandidate());
    CHECK(p->slots_buffer() == nullptr);
    CHECK(p->SweepingDone());
    DCHECK(p->area_size() == area_size);
    pages.push_back(std::make_pair(p->LiveBytesFromFreeList(), p));
  }

  int candidate_count = 0;
  int total_live_bytes = 0;

  const bool reduce_memory = heap()->ShouldReduceMemory();
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
    // The following approach determines the pages that should be evacuated.
    //
    // We use two conditions to decide whether a page qualifies as an evacuation
    // candidate, or not:
    // * Target fragmentation: How fragmented is a page, i.e., how is the ratio
    //   between live bytes and capacity of this page (= area).
    // * Evacuation quota: A global quota determining how much bytes should be
    //   compacted.
    //
    // The algorithm sorts all pages by live bytes and then iterates through
    // them starting with the page with the most free memory, adding them to the
    // set of evacuation candidates as long as both conditions (fragmentation
    // and quota) hold.
    int max_evacuated_bytes;
    int target_fragmentation_percent;
    ComputeEvacuationHeuristics(area_size, &target_fragmentation_percent,
                                &max_evacuated_bytes);

    const intptr_t free_bytes_threshold =
        target_fragmentation_percent * (area_size / 100);

    // Sort pages from the most free to the least free, then select
    // the first n pages for evacuation such that:
    // - the total size of evacuated objects does not exceed the specified
    // limit.
    // - fragmentation of (n+1)-th page does not exceed the specified limit.
    std::sort(pages.begin(), pages.end(),
              [](const LiveBytesPagePair& a, const LiveBytesPagePair& b) {
                return a.first < b.first;
              });
    for (size_t i = 0; i < pages.size(); i++) {
      int live_bytes = pages[i].first;
      int free_bytes = area_size - live_bytes;
      if (FLAG_always_compact ||
          ((free_bytes >= free_bytes_threshold) &&
           ((total_live_bytes + live_bytes) <= max_evacuated_bytes))) {
        candidate_count++;
        total_live_bytes += live_bytes;
      }
      if (FLAG_trace_fragmentation_verbose) {
        PrintIsolate(isolate(),
                     "compaction-selection-page: space=%s free_bytes_page=%d "
                     "fragmentation_limit_kb=%d fragmentation_limit_percent=%d "
                     "sum_compaction_kb=%d "
                     "compaction_limit_kb=%d\n",
                     AllocationSpaceName(space->identity()), free_bytes / KB,
                     free_bytes_threshold / KB, target_fragmentation_percent,
                     total_live_bytes / KB, max_evacuated_bytes / KB);
      }
    }
    // How many pages we will allocated for the evacuated objects
    // in the worst case: ceil(total_live_bytes / area_size)
    int estimated_new_pages = (total_live_bytes + area_size - 1) / area_size;
    DCHECK_LE(estimated_new_pages, candidate_count);
    int estimated_released_pages = candidate_count - estimated_new_pages;
    // Avoid (compact -> expand) cycles.
    if ((estimated_released_pages == 0) && !FLAG_always_compact) {
      candidate_count = 0;
    }
    for (int i = 0; i < candidate_count; i++) {
      AddEvacuationCandidate(pages[i].second);
    }
  }

  if (FLAG_trace_fragmentation) {
    PrintIsolate(isolate(),
                 "compaction-selection: space=%s reduce_memory=%d pages=%d "
                 "total_live_bytes=%d\n",
                 AllocationSpaceName(space->identity()), reduce_memory,
                 candidate_count, total_live_bytes / KB);
  }
}


void MarkCompactCollector::AbortCompaction() {
  if (compacting_) {
    for (Page* p : evacuation_candidates_) {
      slots_buffer_allocator_->DeallocateChain(p->slots_buffer_address());
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

  // If concurrent unmapping tasks are still running, we should wait for
  // them here.
  heap()->WaitUntilUnmappingOfFreeChunksCompleted();

  // Clear marking bits if incremental marking is aborted.
  if (was_marked_incrementally_ && heap_->ShouldAbortIncrementalMarking()) {
    heap()->incremental_marking()->Stop();
    ClearMarkbits();
    AbortWeakCollections();
    AbortWeakCells();
    AbortTransitionArrays();
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
  GCTracer::Scope gc_scope(heap()->tracer(), GCTracer::Scope::MC_FINISH);

  // The hashing of weak_object_to_code_table is no longer valid.
  heap()->weak_object_to_code_table()->Rehash(
      heap()->isolate()->factory()->undefined_value());

  // Clear the marking state of live large objects.
  heap_->lo_space()->ClearMarkingStateOfLiveObjects();

#ifdef DEBUG
  DCHECK(state_ == SWEEP_SPACES || state_ == RELOCATE_OBJECTS);
  state_ = IDLE;
#endif
  heap_->isolate()->inner_pointer_to_code_cache()->Flush();

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

  if (marking_parity_ == EVEN_MARKING_PARITY) {
    marking_parity_ = ODD_MARKING_PARITY;
  } else {
    DCHECK(marking_parity_ == ODD_MARKING_PARITY);
    marking_parity_ = EVEN_MARKING_PARITY;
  }
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
      if (!shared->OptimizedCodeMapIsCleared()) {
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
    isolate_->heap()->mark_compact_collector()->RecordCodeEntrySlot(
        candidate, slot, target);

    Object** shared_code_slot =
        HeapObject::RawField(shared, SharedFunctionInfo::kCodeOffset);
    isolate_->heap()->mark_compact_collector()->RecordSlot(
        shared, shared_code_slot, *shared_code_slot);

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
      if (!candidate->OptimizedCodeMapIsCleared()) {
        candidate->ClearOptimizedCodeMap();
      }
      candidate->set_code(lazy_compile);
    }

    Object** code_slot =
        HeapObject::RawField(candidate, SharedFunctionInfo::kCodeOffset);
    isolate_->heap()->mark_compact_collector()->RecordSlot(candidate, code_slot,
                                                           *code_slot);

    candidate = next_candidate;
  }

  shared_function_info_candidates_head_ = NULL;
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


class MarkCompactMarkingVisitor
    : public StaticMarkingVisitor<MarkCompactMarkingVisitor> {
 public:
  static void Initialize();

  INLINE(static void VisitPointer(Heap* heap, HeapObject* object, Object** p)) {
    MarkObjectByPointer(heap->mark_compact_collector(), object, p);
  }

  INLINE(static void VisitPointers(Heap* heap, HeapObject* object,
                                   Object** start, Object** end)) {
    // Mark all objects pointed to in [start, end).
    const int kMinRangeForMarkingRecursion = 64;
    if (end - start >= kMinRangeForMarkingRecursion) {
      if (VisitUnmarkedObjects(heap, object, start, end)) return;
      // We are close to a stack overflow, so just mark the objects.
    }
    MarkCompactCollector* collector = heap->mark_compact_collector();
    for (Object** p = start; p < end; p++) {
      MarkObjectByPointer(collector, object, p);
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
                                         HeapObject* object, Object** p)) {
    if (!(*p)->IsHeapObject()) return;
    HeapObject* target_object = HeapObject::cast(*p);
    collector->RecordSlot(object, p, target_object);
    MarkBit mark = Marking::MarkBitFrom(target_object);
    collector->MarkObject(target_object, mark);
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
  INLINE(static bool VisitUnmarkedObjects(Heap* heap, HeapObject* object,
                                          Object** start, Object** end)) {
    // Return false is we are close to the stack limit.
    StackLimitCheck check(heap->isolate());
    if (check.HasOverflowed()) return false;

    MarkCompactCollector* collector = heap->mark_compact_collector();
    // Visit the unmarked objects.
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (!o->IsHeapObject()) continue;
      collector->RecordSlot(object, p, o);
      HeapObject* obj = HeapObject::cast(o);
      MarkBit mark = Marking::MarkBitFrom(obj);
      if (Marking::IsBlackOrGrey(mark)) continue;
      VisitUnmarkedObject(collector, obj);
    }
    return true;
  }

 private:
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
      heap->mark_compact_collector()->RecordSlot(data, slot, code);

      // Set a number in the 0-255 range to guarantee no smi overflow.
      re->SetDataAt(JSRegExp::code_index(is_one_byte),
                    Smi::FromInt(heap->ms_count() & 0xff));
    } else if (code->IsSmi()) {
      int value = Smi::cast(code)->value();
      // The regexp has not been compiled yet or there was a compilation error.
      if (value == JSRegExp::kUninitializedValue ||
          value == JSRegExp::kCompilationErrorValue) {
        return;
      }

      // Check if we should flush now.
      if (value == ((heap->ms_count() - kRegExpCodeThreshold) & 0xff)) {
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
};


void MarkCompactMarkingVisitor::Initialize() {
  StaticMarkingVisitor<MarkCompactMarkingVisitor>::Initialize();

  table_.Register(kVisitJSRegExp, &VisitRegExpAndFlushCode);

  if (FLAG_track_gc_object_stats) {
    ObjectStatsVisitor::Initialize(&table_);
  }
}


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

  void VisitPointers(Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) VisitPointer(p);
  }

  void VisitPointer(Object** slot) override {
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
      Code* optimized_code = frame->LookupCode();
      MarkBit optimized_code_mark = Marking::MarkBitFrom(optimized_code);
      MarkObject(optimized_code, optimized_code_mark);
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

  void VisitPointer(Object** p) override { MarkObjectByPointer(p); }

  void VisitPointers(Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

  // Skip the weak next code link in a code object, which is visited in
  // ProcessTopOptimizedFrame.
  void VisitNextCodeLink(Object** p) override {}

 private:
  void MarkObjectByPointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    // Replace flat cons strings in place.
    HeapObject* object = HeapObject::cast(*p);
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

  void VisitPointers(Object** start, Object** end) override {
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
    MarkBit mark_bit = Marking::MarkBitFrom(HeapObject::cast(object));
    DCHECK(!Marking::IsGrey(mark_bit));
    if (Marking::IsBlack(mark_bit)) {
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
void MarkCompactCollector::DiscoverGreyObjectsWithIterator(T* it) {
  // The caller should ensure that the marking stack is initially not full,
  // so that we don't waste effort pointlessly scanning for objects.
  DCHECK(!marking_deque()->IsFull());

  Map* filler_map = heap()->one_pointer_filler_map();
  for (HeapObject* object = it->Next(); object != NULL; object = it->Next()) {
    MarkBit markbit = Marking::MarkBitFrom(object);
    if ((object->map() != filler_map) && Marking::IsGrey(markbit)) {
      Marking::GreyToBlack(markbit);
      PushBlack(object);
      if (marking_deque()->IsFull()) return;
    }
  }
}


void MarkCompactCollector::DiscoverGreyObjectsOnPage(MemoryChunk* p) {
  DCHECK(!marking_deque()->IsFull());
  LiveObjectIterator<kGreyObjects> it(p);
  HeapObject* object = NULL;
  while ((object = it.Next()) != NULL) {
    MarkBit markbit = Marking::MarkBitFrom(object);
    DCHECK(Marking::IsGrey(markbit));
    Marking::GreyToBlack(markbit);
    PushBlack(object);
    if (marking_deque()->IsFull()) return;
  }
}


class MarkCompactCollector::HeapObjectVisitor {
 public:
  virtual ~HeapObjectVisitor() {}
  virtual bool Visit(HeapObject* object) = 0;
};


class MarkCompactCollector::EvacuateVisitorBase
    : public MarkCompactCollector::HeapObjectVisitor {
 public:
  EvacuateVisitorBase(Heap* heap, CompactionSpaceCollection* compaction_spaces,
                      SlotsBuffer** evacuation_slots_buffer,
                      LocalStoreBuffer* local_store_buffer)
      : heap_(heap),
        evacuation_slots_buffer_(evacuation_slots_buffer),
        compaction_spaces_(compaction_spaces),
        local_store_buffer_(local_store_buffer) {}

  bool TryEvacuateObject(PagedSpace* target_space, HeapObject* object,
                         HeapObject** target_object) {
    int size = object->Size();
    AllocationAlignment alignment = object->RequiredAlignment();
    AllocationResult allocation = target_space->AllocateRaw(size, alignment);
    if (allocation.To(target_object)) {
      heap_->mark_compact_collector()->MigrateObject(
          *target_object, object, size, target_space->identity(),
          evacuation_slots_buffer_, local_store_buffer_);
      return true;
    }
    return false;
  }

 protected:
  Heap* heap_;
  SlotsBuffer** evacuation_slots_buffer_;
  CompactionSpaceCollection* compaction_spaces_;
  LocalStoreBuffer* local_store_buffer_;
};


class MarkCompactCollector::EvacuateNewSpaceVisitor final
    : public MarkCompactCollector::EvacuateVisitorBase {
 public:
  static const intptr_t kLabSize = 4 * KB;
  static const intptr_t kMaxLabObjectSize = 256;

  explicit EvacuateNewSpaceVisitor(Heap* heap,
                                   CompactionSpaceCollection* compaction_spaces,
                                   SlotsBuffer** evacuation_slots_buffer,
                                   LocalStoreBuffer* local_store_buffer,
                                   HashMap* local_pretenuring_feedback)
      : EvacuateVisitorBase(heap, compaction_spaces, evacuation_slots_buffer,
                            local_store_buffer),
        buffer_(LocalAllocationBuffer::InvalidBuffer()),
        space_to_allocate_(NEW_SPACE),
        promoted_size_(0),
        semispace_copied_size_(0),
        local_pretenuring_feedback_(local_pretenuring_feedback) {}

  bool Visit(HeapObject* object) override {
    heap_->UpdateAllocationSite<Heap::kCached>(object,
                                               local_pretenuring_feedback_);
    int size = object->Size();
    HeapObject* target_object = nullptr;
    if (heap_->ShouldBePromoted(object->address(), size) &&
        TryEvacuateObject(compaction_spaces_->Get(OLD_SPACE), object,
                          &target_object)) {
      // If we end up needing more special cases, we should factor this out.
      if (V8_UNLIKELY(target_object->IsJSArrayBuffer())) {
        heap_->array_buffer_tracker()->Promote(
            JSArrayBuffer::cast(target_object));
      }
      promoted_size_ += size;
      return true;
    }
    HeapObject* target = nullptr;
    AllocationSpace space = AllocateTargetObject(object, &target);
    heap_->mark_compact_collector()->MigrateObject(
        HeapObject::cast(target), object, size, space,
        (space == NEW_SPACE) ? nullptr : evacuation_slots_buffer_,
        (space == NEW_SPACE) ? nullptr : local_store_buffer_);
    if (V8_UNLIKELY(target->IsJSArrayBuffer())) {
      heap_->array_buffer_tracker()->MarkLive(JSArrayBuffer::cast(target));
    }
    semispace_copied_size_ += size;
    return true;
  }

  intptr_t promoted_size() { return promoted_size_; }
  intptr_t semispace_copied_size() { return semispace_copied_size_; }

 private:
  enum NewSpaceAllocationMode {
    kNonstickyBailoutOldSpace,
    kStickyBailoutOldSpace,
  };

  inline AllocationSpace AllocateTargetObject(HeapObject* old_object,
                                              HeapObject** target_object) {
    const int size = old_object->Size();
    AllocationAlignment alignment = old_object->RequiredAlignment();
    AllocationResult allocation;
    if (space_to_allocate_ == NEW_SPACE) {
      if (size > kMaxLabObjectSize) {
        allocation =
            AllocateInNewSpace(size, alignment, kNonstickyBailoutOldSpace);
      } else {
        allocation = AllocateInLab(size, alignment);
      }
    }
    if (allocation.IsRetry() || (space_to_allocate_ == OLD_SPACE)) {
      allocation = AllocateInOldSpace(size, alignment);
    }
    bool ok = allocation.To(target_object);
    DCHECK(ok);
    USE(ok);
    return space_to_allocate_;
  }

  inline bool NewLocalAllocationBuffer() {
    AllocationResult result =
        AllocateInNewSpace(kLabSize, kWordAligned, kStickyBailoutOldSpace);
    LocalAllocationBuffer saved_old_buffer = buffer_;
    buffer_ = LocalAllocationBuffer::FromResult(heap_, result, kLabSize);
    if (buffer_.IsValid()) {
      buffer_.TryMerge(&saved_old_buffer);
      return true;
    }
    return false;
  }

  inline AllocationResult AllocateInNewSpace(int size_in_bytes,
                                             AllocationAlignment alignment,
                                             NewSpaceAllocationMode mode) {
    AllocationResult allocation =
        heap_->new_space()->AllocateRawSynchronized(size_in_bytes, alignment);
    if (allocation.IsRetry()) {
      if (!heap_->new_space()->AddFreshPageSynchronized()) {
        if (mode == kStickyBailoutOldSpace) space_to_allocate_ = OLD_SPACE;
      } else {
        allocation = heap_->new_space()->AllocateRawSynchronized(size_in_bytes,
                                                                 alignment);
        if (allocation.IsRetry()) {
          if (mode == kStickyBailoutOldSpace) space_to_allocate_ = OLD_SPACE;
        }
      }
    }
    return allocation;
  }

  inline AllocationResult AllocateInOldSpace(int size_in_bytes,
                                             AllocationAlignment alignment) {
    AllocationResult allocation =
        compaction_spaces_->Get(OLD_SPACE)->AllocateRaw(size_in_bytes,
                                                        alignment);
    if (allocation.IsRetry()) {
      v8::internal::Heap::FatalProcessOutOfMemory(
          "MarkCompactCollector: semi-space copy, fallback in old gen", true);
    }
    return allocation;
  }

  inline AllocationResult AllocateInLab(int size_in_bytes,
                                        AllocationAlignment alignment) {
    AllocationResult allocation;
    if (!buffer_.IsValid()) {
      if (!NewLocalAllocationBuffer()) {
        space_to_allocate_ = OLD_SPACE;
        return AllocationResult::Retry(OLD_SPACE);
      }
    }
    allocation = buffer_.AllocateRawAligned(size_in_bytes, alignment);
    if (allocation.IsRetry()) {
      if (!NewLocalAllocationBuffer()) {
        space_to_allocate_ = OLD_SPACE;
        return AllocationResult::Retry(OLD_SPACE);
      } else {
        allocation = buffer_.AllocateRawAligned(size_in_bytes, alignment);
        if (allocation.IsRetry()) {
          space_to_allocate_ = OLD_SPACE;
          return AllocationResult::Retry(OLD_SPACE);
        }
      }
    }
    return allocation;
  }

  LocalAllocationBuffer buffer_;
  AllocationSpace space_to_allocate_;
  intptr_t promoted_size_;
  intptr_t semispace_copied_size_;
  HashMap* local_pretenuring_feedback_;
};


class MarkCompactCollector::EvacuateOldSpaceVisitor final
    : public MarkCompactCollector::EvacuateVisitorBase {
 public:
  EvacuateOldSpaceVisitor(Heap* heap,
                          CompactionSpaceCollection* compaction_spaces,
                          SlotsBuffer** evacuation_slots_buffer,
                          LocalStoreBuffer* local_store_buffer)
      : EvacuateVisitorBase(heap, compaction_spaces, evacuation_slots_buffer,
                            local_store_buffer) {}

  bool Visit(HeapObject* object) override {
    CompactionSpace* target_space = compaction_spaces_->Get(
        Page::FromAddress(object->address())->owner()->identity());
    HeapObject* target_object = nullptr;
    if (TryEvacuateObject(target_space, object, &target_object)) {
      DCHECK(object->map_word().IsForwardingAddress());
      return true;
    }
    return false;
  }
};


void MarkCompactCollector::DiscoverGreyObjectsInSpace(PagedSpace* space) {
  PageIterator it(space);
  while (it.has_next()) {
    Page* p = it.next();
    DiscoverGreyObjectsOnPage(p);
    if (marking_deque()->IsFull()) return;
  }
}


void MarkCompactCollector::DiscoverGreyObjectsInNewSpace() {
  NewSpace* space = heap()->new_space();
  NewSpacePageIterator it(space->bottom(), space->top());
  while (it.has_next()) {
    NewSpacePage* page = it.next();
    DiscoverGreyObjectsOnPage(page);
    if (marking_deque()->IsFull()) return;
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

  DiscoverGreyObjectsInNewSpace();
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap()->old_space());
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap()->code_space());
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap()->map_space());
  if (marking_deque_.IsFull()) return;

  LargeObjectIterator lo_it(heap()->lo_space());
  DiscoverGreyObjectsWithIterator(&lo_it);
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
        Code::BodyDescriptor::IterateBody(code, visitor);
      }
      ProcessMarkingDeque();
      return;
    }
  }
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
    start_time = heap_->MonotonicallyIncreasingTimeInMs();
  }
  // The recursive GC marker detects when it is nearing stack overflow,
  // and switches to a different marking system.  JS interrupts interfere
  // with the C stack limit check.
  PostponeInterruptsScope postpone(isolate());

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_MARK_FINISH_INCREMENTAL);
    IncrementalMarking* incremental_marking = heap_->incremental_marking();
    if (was_marked_incrementally_) {
      incremental_marking->Finalize();
    } else {
      // Abort any pending incremental activities e.g. incremental sweeping.
      incremental_marking->Stop();
      if (marking_deque_.in_use()) {
        marking_deque_.Uninitialize(true);
      }
    }
  }

#ifdef DEBUG
  DCHECK(state_ == PREPARE_GC);
  state_ = MARK_LIVE_OBJECTS;
#endif

  EnsureMarkingDequeIsCommittedAndInitialize(
      MarkCompactCollector::kMaxMarkingDequeSize);

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_MARK_PREPARE_CODE_FLUSH);
    PrepareForCodeFlushing();
  }

  RootMarkingVisitor root_visitor(heap());

  {
    GCTracer::Scope gc_scope(heap()->tracer(), GCTracer::Scope::MC_MARK_ROOTS);
    MarkRoots(&root_visitor);
    ProcessTopOptimizedFrame(&root_visitor);
  }

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_MARK_WEAK_CLOSURE);

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

  if (FLAG_print_cumulative_gc_stat) {
    heap_->tracer()->AddMarkingTime(heap_->MonotonicallyIncreasingTimeInMs() -
                                    start_time);
  }
  if (FLAG_track_gc_object_stats) {
    if (FLAG_trace_gc_object_stats) {
      heap()->object_stats_->TraceObjectStats();
    }
    heap()->object_stats_->CheckpointObjectStats();
  }
}


void MarkCompactCollector::ClearNonLiveReferences() {
  GCTracer::Scope gc_scope(heap()->tracer(), GCTracer::Scope::MC_CLEAR);

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_CLEAR_STRING_TABLE);

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
  }

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_CLEAR_WEAK_LISTS);
    // Process the weak references.
    MarkCompactWeakObjectRetainer mark_compact_object_retainer;
    heap()->ProcessAllWeakReferences(&mark_compact_object_retainer);
  }

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_CLEAR_GLOBAL_HANDLES);

    // Remove object groups after marking phase.
    heap()->isolate()->global_handles()->RemoveObjectGroups();
    heap()->isolate()->global_handles()->RemoveImplicitRefGroups();
  }

  // Flush code from collected candidates.
  if (is_code_flushing_enabled()) {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_CLEAR_CODE_FLUSH);
    code_flusher_->ProcessCandidates();
  }


  DependentCode* dependent_code_list;
  Object* non_live_map_list;
  ClearWeakCells(&non_live_map_list, &dependent_code_list);

  {
    GCTracer::Scope gc_scope(heap()->tracer(), GCTracer::Scope::MC_CLEAR_MAPS);
    ClearSimpleMapTransitions(non_live_map_list);
    ClearFullMapTransitions();
  }

  MarkDependentCodeForDeoptimization(dependent_code_list);

  ClearWeakCollections();

  ClearInvalidStoreAndSlotsBufferEntries();
}


void MarkCompactCollector::MarkDependentCodeForDeoptimization(
    DependentCode* list_head) {
  GCTracer::Scope gc_scope(heap()->tracer(),
                           GCTracer::Scope::MC_CLEAR_DEPENDENT_CODE);
  Isolate* isolate = this->isolate();
  DependentCode* current = list_head;
  while (current->length() > 0) {
    have_code_to_deoptimize_ |= current->MarkCodeForDeoptimization(
        isolate, DependentCode::kWeakCodeGroup);
    current = current->next_link();
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
              isolate, DependentCode::kWeakCodeGroup);
      table->set(key_index, heap_->the_hole_value());
      table->set(value_index, heap_->the_hole_value());
      table->ElementRemoved();
    }
  }
}


void MarkCompactCollector::ClearSimpleMapTransitions(
    Object* non_live_map_list) {
  Object* the_hole_value = heap()->the_hole_value();
  Object* weak_cell_obj = non_live_map_list;
  while (weak_cell_obj != Smi::FromInt(0)) {
    WeakCell* weak_cell = WeakCell::cast(weak_cell_obj);
    Map* map = Map::cast(weak_cell->value());
    DCHECK(Marking::IsWhite(Marking::MarkBitFrom(map)));
    Object* potential_parent = map->constructor_or_backpointer();
    if (potential_parent->IsMap()) {
      Map* parent = Map::cast(potential_parent);
      if (Marking::IsBlackOrGrey(Marking::MarkBitFrom(parent)) &&
          parent->raw_transitions() == weak_cell) {
        ClearSimpleMapTransition(parent, map);
      }
    }
    weak_cell->clear();
    weak_cell_obj = weak_cell->next();
    weak_cell->clear_next(the_hole_value);
  }
}


void MarkCompactCollector::ClearSimpleMapTransition(Map* map,
                                                    Map* dead_transition) {
  // A previously existing simple transition (stored in a WeakCell) is going
  // to be cleared. Clear the useless cell pointer, and take ownership
  // of the descriptor array.
  map->set_raw_transitions(Smi::FromInt(0));
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  DescriptorArray* descriptors = map->instance_descriptors();
  if (descriptors == dead_transition->instance_descriptors() &&
      number_of_own_descriptors > 0) {
    TrimDescriptorArray(map, descriptors);
    DCHECK(descriptors->number_of_descriptors() == number_of_own_descriptors);
    map->set_owns_descriptors(true);
  }
}


void MarkCompactCollector::ClearFullMapTransitions() {
  HeapObject* undefined = heap()->undefined_value();
  Object* obj = heap()->encountered_transition_arrays();
  while (obj != Smi::FromInt(0)) {
    TransitionArray* array = TransitionArray::cast(obj);
    int num_transitions = array->number_of_entries();
    DCHECK_EQ(TransitionArray::NumberOfTransitions(array), num_transitions);
    if (num_transitions > 0) {
      Map* map = array->GetTarget(0);
      Map* parent = Map::cast(map->constructor_or_backpointer());
      bool parent_is_alive =
          Marking::IsBlackOrGrey(Marking::MarkBitFrom(parent));
      DescriptorArray* descriptors =
          parent_is_alive ? parent->instance_descriptors() : nullptr;
      bool descriptors_owner_died =
          CompactTransitionArray(parent, array, descriptors);
      if (descriptors_owner_died) {
        TrimDescriptorArray(parent, descriptors);
      }
    }
    obj = array->next_link();
    array->set_next_link(undefined, SKIP_WRITE_BARRIER);
  }
  heap()->set_encountered_transition_arrays(Smi::FromInt(0));
}


bool MarkCompactCollector::CompactTransitionArray(
    Map* map, TransitionArray* transitions, DescriptorArray* descriptors) {
  int num_transitions = transitions->number_of_entries();
  bool descriptors_owner_died = false;
  int transition_index = 0;
  // Compact all live transitions to the left.
  for (int i = 0; i < num_transitions; ++i) {
    Map* target = transitions->GetTarget(i);
    DCHECK_EQ(target->constructor_or_backpointer(), map);
    if (Marking::IsWhite(Marking::MarkBitFrom(target))) {
      if (descriptors != nullptr &&
          target->instance_descriptors() == descriptors) {
        descriptors_owner_died = true;
      }
    } else {
      if (i != transition_index) {
        Name* key = transitions->GetKey(i);
        transitions->SetKey(transition_index, key);
        Object** key_slot = transitions->GetKeySlot(transition_index);
        RecordSlot(transitions, key_slot, key);
        // Target slots do not need to be recorded since maps are not compacted.
        transitions->SetTarget(transition_index, transitions->GetTarget(i));
      }
      transition_index++;
    }
  }
  // If there are no transitions to be cleared, return.
  if (transition_index == num_transitions) {
    DCHECK(!descriptors_owner_died);
    return false;
  }
  // Note that we never eliminate a transition array, though we might right-trim
  // such that number_of_transitions() == 0. If this assumption changes,
  // TransitionArray::Insert() will need to deal with the case that a transition
  // array disappeared during GC.
  int trim = TransitionArray::Capacity(transitions) - transition_index;
  if (trim > 0) {
    heap_->RightTrimFixedArray<Heap::SEQUENTIAL_TO_SWEEPER>(
        transitions, trim * TransitionArray::kTransitionSize);
    transitions->SetNumberOfTransitions(transition_index);
  }
  return descriptors_owner_died;
}


void MarkCompactCollector::TrimDescriptorArray(Map* map,
                                               DescriptorArray* descriptors) {
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) {
    DCHECK(descriptors == heap_->empty_descriptor_array());
    return;
  }

  int number_of_descriptors = descriptors->number_of_descriptors_storage();
  int to_trim = number_of_descriptors - number_of_own_descriptors;
  if (to_trim > 0) {
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
  DCHECK(descriptors->number_of_descriptors() == number_of_own_descriptors);
  map->set_owns_descriptors(true);
}


void MarkCompactCollector::TrimEnumCache(Map* map,
                                         DescriptorArray* descriptors) {
  int live_enum = map->EnumLength();
  if (live_enum == kInvalidEnumCacheSentinel) {
    live_enum =
        map->NumberOfDescribedProperties(OWN_DESCRIPTORS, ENUMERABLE_STRINGS);
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
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::FromInt(0)) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    DCHECK(MarkCompactCollector::IsMarked(weak_collection));
    if (weak_collection->table()->IsHashTable()) {
      ObjectHashTable* table = ObjectHashTable::cast(weak_collection->table());
      for (int i = 0; i < table->Capacity(); i++) {
        if (MarkCompactCollector::IsMarked(HeapObject::cast(table->KeyAt(i)))) {
          Object** key_slot =
              table->RawFieldOfElementAt(ObjectHashTable::EntryToIndex(i));
          RecordSlot(table, key_slot, *key_slot);
          Object** value_slot =
              table->RawFieldOfElementAt(ObjectHashTable::EntryToValueIndex(i));
          MarkCompactMarkingVisitor::MarkObjectByPointer(this, table,
                                                         value_slot);
        }
      }
    }
    weak_collection_obj = weak_collection->next();
  }
}


void MarkCompactCollector::ClearWeakCollections() {
  GCTracer::Scope gc_scope(heap()->tracer(),
                           GCTracer::Scope::MC_CLEAR_WEAK_COLLECTIONS);
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
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::FromInt(0)) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    weak_collection_obj = weak_collection->next();
    weak_collection->set_next(heap()->undefined_value());
  }
  heap()->set_encountered_weak_collections(Smi::FromInt(0));
}


void MarkCompactCollector::ClearWeakCells(Object** non_live_map_list,
                                          DependentCode** dependent_code_list) {
  Heap* heap = this->heap();
  GCTracer::Scope gc_scope(heap->tracer(),
                           GCTracer::Scope::MC_CLEAR_WEAK_CELLS);
  Object* weak_cell_obj = heap->encountered_weak_cells();
  Object* the_hole_value = heap->the_hole_value();
  DependentCode* dependent_code_head =
      DependentCode::cast(heap->empty_fixed_array());
  Object* non_live_map_head = Smi::FromInt(0);
  while (weak_cell_obj != Smi::FromInt(0)) {
    WeakCell* weak_cell = reinterpret_cast<WeakCell*>(weak_cell_obj);
    Object* next_weak_cell = weak_cell->next();
    bool clear_value = true;
    bool clear_next = true;
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
          RecordSlot(value, slot, *slot);
          slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
          RecordSlot(weak_cell, slot, *slot);
          clear_value = false;
        }
      }
      if (value->IsMap()) {
        // The map is non-live.
        Map* map = Map::cast(value);
        // Add dependent code to the dependent_code_list.
        DependentCode* candidate = map->dependent_code();
        // We rely on the fact that the weak code group comes first.
        STATIC_ASSERT(DependentCode::kWeakCodeGroup == 0);
        if (candidate->length() > 0 &&
            candidate->group() == DependentCode::kWeakCodeGroup) {
          candidate->set_next_link(dependent_code_head);
          dependent_code_head = candidate;
        }
        // Add the weak cell to the non_live_map list.
        weak_cell->set_next(non_live_map_head);
        non_live_map_head = weak_cell;
        clear_value = false;
        clear_next = false;
      }
    } else {
      // The value of the weak cell is alive.
      Object** slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
      RecordSlot(weak_cell, slot, *slot);
      clear_value = false;
    }
    if (clear_value) {
      weak_cell->clear();
    }
    if (clear_next) {
      weak_cell->clear_next(the_hole_value);
    }
    weak_cell_obj = next_weak_cell;
  }
  heap->set_encountered_weak_cells(Smi::FromInt(0));
  *non_live_map_list = non_live_map_head;
  *dependent_code_list = dependent_code_head;
}


void MarkCompactCollector::AbortWeakCells() {
  Object* the_hole_value = heap()->the_hole_value();
  Object* weak_cell_obj = heap()->encountered_weak_cells();
  while (weak_cell_obj != Smi::FromInt(0)) {
    WeakCell* weak_cell = reinterpret_cast<WeakCell*>(weak_cell_obj);
    weak_cell_obj = weak_cell->next();
    weak_cell->clear_next(the_hole_value);
  }
  heap()->set_encountered_weak_cells(Smi::FromInt(0));
}


void MarkCompactCollector::AbortTransitionArrays() {
  HeapObject* undefined = heap()->undefined_value();
  Object* obj = heap()->encountered_transition_arrays();
  while (obj != Smi::FromInt(0)) {
    TransitionArray* array = TransitionArray::cast(obj);
    obj = array->next_link();
    array->set_next_link(undefined, SKIP_WRITE_BARRIER);
  }
  heap()->set_encountered_transition_arrays(Smi::FromInt(0));
}

void MarkCompactCollector::RecordMigratedSlot(
    Object* value, Address slot, SlotsBuffer** evacuation_slots_buffer,
    LocalStoreBuffer* local_store_buffer) {
  // When parallel compaction is in progress, store and slots buffer entries
  // require synchronization.
  if (heap_->InNewSpace(value)) {
    if (compaction_in_progress_) {
      local_store_buffer->Record(slot);
    } else {
      Page* page = Page::FromAddress(slot);
      RememberedSet<OLD_TO_NEW>::Insert(page, slot);
    }
  } else if (value->IsHeapObject() && IsOnEvacuationCandidate(value)) {
    SlotsBuffer::AddTo(slots_buffer_allocator_, evacuation_slots_buffer,
                       reinterpret_cast<Object**>(slot),
                       SlotsBuffer::IGNORE_OVERFLOW);
  }
}


void MarkCompactCollector::RecordMigratedCodeEntrySlot(
    Address code_entry, Address code_entry_slot,
    SlotsBuffer** evacuation_slots_buffer) {
  if (Page::FromAddress(code_entry)->IsEvacuationCandidate()) {
    SlotsBuffer::AddTo(slots_buffer_allocator_, evacuation_slots_buffer,
                       SlotsBuffer::CODE_ENTRY_SLOT, code_entry_slot,
                       SlotsBuffer::IGNORE_OVERFLOW);
  }
}


void MarkCompactCollector::RecordMigratedCodeObjectSlot(
    Address code_object, SlotsBuffer** evacuation_slots_buffer) {
  SlotsBuffer::AddTo(slots_buffer_allocator_, evacuation_slots_buffer,
                     SlotsBuffer::RELOCATED_CODE_OBJECT, code_object,
                     SlotsBuffer::IGNORE_OVERFLOW);
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
  }
  UNREACHABLE();
  return SlotsBuffer::NUMBER_OF_SLOT_TYPES;
}


static inline SlotsBuffer::SlotType DecodeSlotType(
    SlotsBuffer::ObjectSlot slot) {
  return static_cast<SlotsBuffer::SlotType>(reinterpret_cast<intptr_t>(slot));
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
        slots_buffer_allocator_, target_page->slots_buffer_address(), slot_type,
        addr, SlotsBuffer::FAIL_ON_OVERFLOW);
    if (!success) {
      EvictPopularEvacuationCandidate(target_page);
    }
  }
}


class RecordMigratedSlotVisitor final : public ObjectVisitor {
 public:
  RecordMigratedSlotVisitor(MarkCompactCollector* collector,
                            SlotsBuffer** evacuation_slots_buffer,
                            LocalStoreBuffer* local_store_buffer)
      : collector_(collector),
        evacuation_slots_buffer_(evacuation_slots_buffer),
        local_store_buffer_(local_store_buffer) {}

  V8_INLINE void VisitPointer(Object** p) override {
    collector_->RecordMigratedSlot(*p, reinterpret_cast<Address>(p),
                                   evacuation_slots_buffer_,
                                   local_store_buffer_);
  }

  V8_INLINE void VisitPointers(Object** start, Object** end) override {
    while (start < end) {
      collector_->RecordMigratedSlot(*start, reinterpret_cast<Address>(start),
                                     evacuation_slots_buffer_,
                                     local_store_buffer_);
      ++start;
    }
  }

  V8_INLINE void VisitCodeEntry(Address code_entry_slot) override {
    if (collector_->compacting_) {
      Address code_entry = Memory::Address_at(code_entry_slot);
      collector_->RecordMigratedCodeEntrySlot(code_entry, code_entry_slot,
                                              evacuation_slots_buffer_);
    }
  }

 private:
  MarkCompactCollector* collector_;
  SlotsBuffer** evacuation_slots_buffer_;
  LocalStoreBuffer* local_store_buffer_;
};


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
                                         int size, AllocationSpace dest,
                                         SlotsBuffer** evacuation_slots_buffer,
                                         LocalStoreBuffer* local_store_buffer) {
  Address dst_addr = dst->address();
  Address src_addr = src->address();
  DCHECK(heap()->AllowedToBeMigrated(src, dest));
  DCHECK(dest != LO_SPACE);
  if (dest == OLD_SPACE) {
    DCHECK_OBJECT_SIZE(size);
    DCHECK(evacuation_slots_buffer != nullptr);
    DCHECK(IsAligned(size, kPointerSize));

    heap()->MoveBlock(dst->address(), src->address(), size);
    RecordMigratedSlotVisitor visitor(this, evacuation_slots_buffer,
                                      local_store_buffer);
    dst->IterateBody(&visitor);
  } else if (dest == CODE_SPACE) {
    DCHECK_CODEOBJECT_SIZE(size, heap()->code_space());
    DCHECK(evacuation_slots_buffer != nullptr);
    PROFILE(isolate(), CodeMoveEvent(src_addr, dst_addr));
    heap()->MoveBlock(dst_addr, src_addr, size);
    RecordMigratedCodeObjectSlot(dst_addr, evacuation_slots_buffer);
    Code::cast(dst)->Relocate(dst_addr - src_addr);
  } else {
    DCHECK_OBJECT_SIZE(size);
    DCHECK(evacuation_slots_buffer == nullptr);
    DCHECK(dest == NEW_SPACE);
    heap()->MoveBlock(dst_addr, src_addr, size);
  }
  heap()->OnMoveEvent(dst, src, size);
  Memory::Address_at(src_addr) = dst_addr;
}


static inline void UpdateSlot(Isolate* isolate, ObjectVisitor* v,
                              SlotsBuffer::SlotType slot_type, Address addr) {
  switch (slot_type) {
    case SlotsBuffer::CODE_TARGET_SLOT: {
      RelocInfo rinfo(isolate, addr, RelocInfo::CODE_TARGET, 0, NULL);
      rinfo.Visit(isolate, v);
      break;
    }
    case SlotsBuffer::CELL_TARGET_SLOT: {
      RelocInfo rinfo(isolate, addr, RelocInfo::CELL, 0, NULL);
      rinfo.Visit(isolate, v);
      break;
    }
    case SlotsBuffer::CODE_ENTRY_SLOT: {
      v->VisitCodeEntry(addr);
      break;
    }
    case SlotsBuffer::RELOCATED_CODE_OBJECT: {
      HeapObject* obj = HeapObject::FromAddress(addr);
      Code::BodyDescriptor::IterateBody(obj, v);
      break;
    }
    case SlotsBuffer::DEBUG_TARGET_SLOT: {
      RelocInfo rinfo(isolate, addr, RelocInfo::DEBUG_BREAK_SLOT_AT_POSITION, 0,
                      NULL);
      if (rinfo.IsPatchedDebugBreakSlotSequence()) rinfo.Visit(isolate, v);
      break;
    }
    case SlotsBuffer::EMBEDDED_OBJECT_SLOT: {
      RelocInfo rinfo(isolate, addr, RelocInfo::EMBEDDED_OBJECT, 0, NULL);
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


// Visitor for updating pointers from live objects in old spaces to new space.
// It does not expect to encounter pointers to dead objects.
class PointersUpdatingVisitor : public ObjectVisitor {
 public:
  explicit PointersUpdatingVisitor(Heap* heap) : heap_(heap) {}

  void VisitPointer(Object** p) override { UpdatePointer(p); }

  void VisitPointers(Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) UpdatePointer(p);
  }

  void VisitCell(RelocInfo* rinfo) override {
    DCHECK(rinfo->rmode() == RelocInfo::CELL);
    Object* cell = rinfo->target_cell();
    Object* old_cell = cell;
    VisitPointer(&cell);
    if (cell != old_cell) {
      rinfo->set_target_cell(reinterpret_cast<Cell*>(cell));
    }
  }

  void VisitEmbeddedPointer(RelocInfo* rinfo) override {
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

  void VisitCodeTarget(RelocInfo* rinfo) override {
    DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Object* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    Object* old_target = target;
    VisitPointer(&target);
    if (target != old_target) {
      rinfo->set_target_address(Code::cast(target)->instruction_start());
    }
  }

  void VisitCodeAgeSequence(RelocInfo* rinfo) override {
    DCHECK(RelocInfo::IsCodeAgeSequence(rinfo->rmode()));
    Object* stub = rinfo->code_age_stub();
    DCHECK(stub != NULL);
    VisitPointer(&stub);
    if (stub != rinfo->code_age_stub()) {
      rinfo->set_code_age_stub(Code::cast(stub));
    }
  }

  void VisitDebugTarget(RelocInfo* rinfo) override {
    DCHECK(RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
           rinfo->IsPatchedDebugBreakSlotSequence());
    Object* target =
        Code::GetCodeFromTargetAddress(rinfo->debug_call_address());
    VisitPointer(&target);
    rinfo->set_debug_call_address(Code::cast(target)->instruction_start());
  }

  static inline void UpdateSlot(Heap* heap, Object** slot) {
    Object* obj = reinterpret_cast<Object*>(
        base::NoBarrier_Load(reinterpret_cast<base::AtomicWord*>(slot)));

    if (!obj->IsHeapObject()) return;

    HeapObject* heap_obj = HeapObject::cast(obj);

    MapWord map_word = heap_obj->map_word();
    if (map_word.IsForwardingAddress()) {
      DCHECK(heap->InFromSpace(heap_obj) ||
             MarkCompactCollector::IsOnEvacuationCandidate(heap_obj) ||
             Page::FromAddress(heap_obj->address())
                 ->IsFlagSet(Page::COMPACTION_WAS_ABORTED));
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

  Heap* heap_;
};


void MarkCompactCollector::UpdateSlots(SlotsBuffer* buffer) {
  PointersUpdatingVisitor v(heap_);
  size_t buffer_size = buffer->Size();

  for (size_t slot_idx = 0; slot_idx < buffer_size; ++slot_idx) {
    SlotsBuffer::ObjectSlot slot = buffer->Get(slot_idx);
    if (!SlotsBuffer::IsTypedSlot(slot)) {
      PointersUpdatingVisitor::UpdateSlot(heap_, slot);
    } else {
      ++slot_idx;
      DCHECK(slot_idx < buffer_size);
      UpdateSlot(heap_->isolate(), &v, DecodeSlotType(slot),
                 reinterpret_cast<Address>(buffer->Get(slot_idx)));
    }
  }
}


void MarkCompactCollector::UpdateSlotsRecordedIn(SlotsBuffer* buffer) {
  while (buffer != NULL) {
    UpdateSlots(buffer);
    buffer = buffer->next();
  }
}


static void UpdatePointer(HeapObject** address, HeapObject* object) {
  MapWord map_word = object->map_word();
  // Since we only filter invalid slots in old space, the store buffer can
  // still contain stale pointers in large object and in map spaces. Ignore
  // these pointers here.
  DCHECK(map_word.IsForwardingAddress() ||
         !object->GetHeap()->old_space()->Contains(
             reinterpret_cast<Address>(address)));
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
  unsigned int cell_index = mark_bit_index >> Bitmap::kBitsPerCellLog2;
  MarkBit::CellType index_mask = 1u << Bitmap::IndexInCell(mark_bit_index);
  MarkBit::CellType* cells = p->markbits()->cells();
  Address base_address = p->area_start();
  unsigned int base_address_cell_index = Bitmap::IndexToCell(
      Bitmap::CellAlignIndex(p->AddressToMarkbitIndex(base_address)));

  // Check if the slot points to the start of an object. This can happen e.g.
  // when we left trim a fixed array. Such slots are invalid and we can remove
  // them.
  if (index_mask > 1) {
    if ((cells[cell_index] & index_mask) != 0 &&
        (cells[cell_index] & (index_mask >> 1)) == 0) {
      return false;
    }
  } else {
    // Left trimming moves the mark bits so we cannot be in the very first cell.
    DCHECK(cell_index != base_address_cell_index);
    if ((cells[cell_index] & index_mask) != 0 &&
        (cells[cell_index - 1] & (1u << Bitmap::kBitIndexMask)) == 0) {
      return false;
    }
  }

  // Check if the object is in the current cell.
  MarkBit::CellType slot_mask;
  if ((cells[cell_index] == 0) ||
      (base::bits::CountTrailingZeros32(cells[cell_index]) >
       base::bits::CountTrailingZeros32(cells[cell_index] | index_mask))) {
    // If we are already in the first cell, there is no live object.
    if (cell_index == base_address_cell_index) return false;

    // If not, find a cell in a preceding cell slot that has a mark bit set.
    do {
      cell_index--;
    } while (cell_index > base_address_cell_index && cells[cell_index] == 0);

    // The slot must be in a dead object if there are no preceding cells that
    // have mark bits set.
    if (cells[cell_index] == 0) {
      return false;
    }

    // The object is in a preceding cell. Set the mask to find any object.
    slot_mask = ~0u;
  } else {
    // We are interested in object mark bits right before the slot.
    slot_mask = index_mask + (index_mask - 1);
  }

  MarkBit::CellType current_cell = cells[cell_index];
  CHECK(current_cell != 0);

  // Find the last live object in the cell.
  unsigned int leading_zeros =
      base::bits::CountLeadingZeros32(current_cell & slot_mask);
  CHECK(leading_zeros != Bitmap::kBitsPerCell);
  int offset = static_cast<int>(Bitmap::kBitIndexMask - leading_zeros) - 1;

  base_address += (cell_index - base_address_cell_index) *
                  Bitmap::kBitsPerCell * kPointerSize;
  Address address = base_address + offset * kPointerSize;
  HeapObject* object = HeapObject::FromAddress(address);
  CHECK(Marking::IsBlack(Marking::MarkBitFrom(object)));
  CHECK(object->address() < reinterpret_cast<Address>(slot));
  if ((object->address() + kPointerSize) <= slot &&
      (object->address() + object->Size()) > slot) {
    // If the slot is within the last found object in the cell, the slot is
    // in a live object.
    // Slots pointing to the first word of an object are invalid and removed.
    // This can happen when we move the object header while left trimming.
    *out_object = object;
    return true;
  }
  return false;
}


bool MarkCompactCollector::IsSlotInBlackObjectSlow(Page* p, Address slot) {
  // This function does not support large objects right now.
  Space* owner = p->owner();
  if (owner == heap_->lo_space() || owner == NULL) {
    Object* large_object = heap_->lo_space()->FindObject(slot);
    // This object has to exist, otherwise we would not have recorded a slot
    // for it.
    CHECK(large_object->IsHeapObject());
    HeapObject* large_heap_object = HeapObject::cast(large_object);
    if (IsMarked(large_heap_object)) {
      return true;
    }
    return false;
  }

  LiveObjectIterator<kBlackObjects> it(p);
  HeapObject* object = NULL;
  while ((object = it.Next()) != NULL) {
    int size = object->Size();

    if (object->address() > slot) return false;
    if (object->address() <= slot && slot < (object->address() + size)) {
      return true;
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
  int offset = static_cast<int>(slot - object->address());
  return object->IsValidSlot(offset);
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


void MarkCompactCollector::EvacuateNewSpacePrologue() {
  NewSpace* new_space = heap()->new_space();
  NewSpacePageIterator it(new_space->bottom(), new_space->top());
  // Append the list of new space pages to be processed.
  while (it.has_next()) {
    newspace_evacuation_candidates_.Add(it.next());
  }
  new_space->Flip();
  new_space->ResetAllocationInfo();
}

void MarkCompactCollector::EvacuateNewSpaceEpilogue() {
  newspace_evacuation_candidates_.Rewind(0);
}


void MarkCompactCollector::AddEvacuationSlotsBufferSynchronized(
    SlotsBuffer* evacuation_slots_buffer) {
  base::LockGuard<base::Mutex> lock_guard(&evacuation_slots_buffers_mutex_);
  evacuation_slots_buffers_.Add(evacuation_slots_buffer);
}

class MarkCompactCollector::Evacuator : public Malloced {
 public:
  Evacuator(MarkCompactCollector* collector,
            const List<Page*>& evacuation_candidates,
            const List<NewSpacePage*>& newspace_evacuation_candidates)
      : collector_(collector),
        evacuation_candidates_(evacuation_candidates),
        newspace_evacuation_candidates_(newspace_evacuation_candidates),
        compaction_spaces_(collector->heap()),
        local_slots_buffer_(nullptr),
        local_store_buffer_(collector->heap()),
        local_pretenuring_feedback_(HashMap::PointersMatch,
                                    kInitialLocalPretenuringFeedbackCapacity),
        new_space_visitor_(collector->heap(), &compaction_spaces_,
                           &local_slots_buffer_, &local_store_buffer_,
                           &local_pretenuring_feedback_),
        old_space_visitor_(collector->heap(), &compaction_spaces_,
                           &local_slots_buffer_, &local_store_buffer_),
        duration_(0.0),
        bytes_compacted_(0),
        task_id_(0) {}

  // Evacuate the configured set of pages in parallel.
  inline void EvacuatePages();

  // Merge back locally cached info sequentially. Note that this method needs
  // to be called from the main thread.
  inline void Finalize();

  CompactionSpaceCollection* compaction_spaces() { return &compaction_spaces_; }

  uint32_t task_id() { return task_id_; }
  void set_task_id(uint32_t id) { task_id_ = id; }

 private:
  static const int kInitialLocalPretenuringFeedbackCapacity = 256;

  Heap* heap() { return collector_->heap(); }

  void ReportCompactionProgress(double duration, intptr_t bytes_compacted) {
    duration_ += duration;
    bytes_compacted_ += bytes_compacted;
  }

  inline bool EvacuateSinglePage(MemoryChunk* p, HeapObjectVisitor* visitor);

  MarkCompactCollector* collector_;

  // Pages to process.
  const List<Page*>& evacuation_candidates_;
  const List<NewSpacePage*>& newspace_evacuation_candidates_;

  // Locally cached collector data.
  CompactionSpaceCollection compaction_spaces_;
  SlotsBuffer* local_slots_buffer_;
  LocalStoreBuffer local_store_buffer_;
  HashMap local_pretenuring_feedback_;

  // Vistors for the corresponding spaces.
  EvacuateNewSpaceVisitor new_space_visitor_;
  EvacuateOldSpaceVisitor old_space_visitor_;

  // Book keeping info.
  double duration_;
  intptr_t bytes_compacted_;

  // Task id, if this evacuator is executed on a background task instead of
  // the main thread. Can be used to try to abort the task currently scheduled
  // to executed to evacuate pages.
  uint32_t task_id_;
};

bool MarkCompactCollector::Evacuator::EvacuateSinglePage(
    MemoryChunk* p, HeapObjectVisitor* visitor) {
  bool success = true;
  if (p->parallel_compaction_state().TrySetValue(
          MemoryChunk::kCompactingDone, MemoryChunk::kCompactingInProgress)) {
    if (p->IsEvacuationCandidate() || p->InNewSpace()) {
      DCHECK_EQ(p->parallel_compaction_state().Value(),
                MemoryChunk::kCompactingInProgress);
      int saved_live_bytes = p->LiveBytes();
      double evacuation_time;
      {
        AlwaysAllocateScope always_allocate(heap()->isolate());
        TimedScope timed_scope(&evacuation_time);
        success = collector_->VisitLiveObjects(p, visitor, kClearMarkbits);
      }
      if (success) {
        ReportCompactionProgress(evacuation_time, saved_live_bytes);
        p->parallel_compaction_state().SetValue(
            MemoryChunk::kCompactingFinalize);
      } else {
        p->parallel_compaction_state().SetValue(
            MemoryChunk::kCompactingAborted);
      }
    } else {
      // There could be popular pages in the list of evacuation candidates
      // which we do not compact.
      p->parallel_compaction_state().SetValue(MemoryChunk::kCompactingDone);
    }
  }
  return success;
}

void MarkCompactCollector::Evacuator::EvacuatePages() {
  for (NewSpacePage* p : newspace_evacuation_candidates_) {
    DCHECK(p->InNewSpace());
    DCHECK_EQ(p->concurrent_sweeping_state().Value(),
              NewSpacePage::kSweepingDone);
    bool success = EvacuateSinglePage(p, &new_space_visitor_);
    DCHECK(success);
    USE(success);
  }
  for (Page* p : evacuation_candidates_) {
    DCHECK(p->IsEvacuationCandidate() ||
           p->IsFlagSet(MemoryChunk::RESCAN_ON_EVACUATION));
    DCHECK_EQ(p->concurrent_sweeping_state().Value(), Page::kSweepingDone);
    EvacuateSinglePage(p, &old_space_visitor_);
  }
}

void MarkCompactCollector::Evacuator::Finalize() {
  heap()->old_space()->MergeCompactionSpace(compaction_spaces_.Get(OLD_SPACE));
  heap()->code_space()->MergeCompactionSpace(
      compaction_spaces_.Get(CODE_SPACE));
  heap()->tracer()->AddCompactionEvent(duration_, bytes_compacted_);
  heap()->IncrementPromotedObjectsSize(new_space_visitor_.promoted_size());
  heap()->IncrementSemiSpaceCopiedObjectSize(
      new_space_visitor_.semispace_copied_size());
  heap()->IncrementYoungSurvivorsCounter(
      new_space_visitor_.promoted_size() +
      new_space_visitor_.semispace_copied_size());
  heap()->MergeAllocationSitePretenuringFeedback(local_pretenuring_feedback_);
  local_store_buffer_.Process(heap()->store_buffer());
  collector_->AddEvacuationSlotsBufferSynchronized(local_slots_buffer_);
}

class MarkCompactCollector::CompactionTask : public CancelableTask {
 public:
  explicit CompactionTask(Heap* heap, Evacuator* evacuator)
      : CancelableTask(heap->isolate()), heap_(heap), evacuator_(evacuator) {
    evacuator->set_task_id(id());
  }

  virtual ~CompactionTask() {}

 private:
  // v8::internal::CancelableTask overrides.
  void RunInternal() override {
    evacuator_->EvacuatePages();
    heap_->mark_compact_collector()
        ->pending_compaction_tasks_semaphore_.Signal();
  }

  Heap* heap_;
  Evacuator* evacuator_;

  DISALLOW_COPY_AND_ASSIGN(CompactionTask);
};

int MarkCompactCollector::NumberOfParallelCompactionTasks(int pages,
                                                          intptr_t live_bytes) {
  if (!FLAG_parallel_compaction) return 1;
  // Compute the number of needed tasks based on a target compaction time, the
  // profiled compaction speed and marked live memory.
  //
  // The number of parallel compaction tasks is limited by:
  // - #evacuation pages
  // - (#cores - 1)
  const double kTargetCompactionTimeInMs = 1;
  const int kNumSweepingTasks = 3;

  intptr_t compaction_speed =
      heap()->tracer()->CompactionSpeedInBytesPerMillisecond();

  const int available_cores =
      Max(1, base::SysInfo::NumberOfProcessors() - kNumSweepingTasks - 1);
  int tasks;
  if (compaction_speed > 0) {
    tasks = 1 + static_cast<int>(static_cast<double>(live_bytes) /
                                 compaction_speed / kTargetCompactionTimeInMs);
  } else {
    tasks = pages;
  }
  const int tasks_capped_pages = Min(pages, tasks);
  return Min(available_cores, tasks_capped_pages);
}


void MarkCompactCollector::EvacuatePagesInParallel() {
  int num_pages = 0;
  intptr_t live_bytes = 0;
  for (Page* page : evacuation_candidates_) {
    num_pages++;
    live_bytes += page->LiveBytes();
  }
  for (NewSpacePage* page : newspace_evacuation_candidates_) {
    num_pages++;
    live_bytes += page->LiveBytes();
  }
  DCHECK_GE(num_pages, 1);

  // Used for trace summary.
  intptr_t compaction_speed = 0;
  if (FLAG_trace_fragmentation) {
    compaction_speed = heap()->tracer()->CompactionSpeedInBytesPerMillisecond();
  }

  const int num_tasks = NumberOfParallelCompactionTasks(num_pages, live_bytes);

  // Set up compaction spaces.
  Evacuator** evacuators = new Evacuator*[num_tasks];
  for (int i = 0; i < num_tasks; i++) {
    evacuators[i] = new Evacuator(this, evacuation_candidates_,
                                  newspace_evacuation_candidates_);
  }

  // Kick off parallel tasks.
  StartParallelCompaction(evacuators, num_tasks);
  // Wait for unfinished and not-yet-started tasks.
  WaitUntilCompactionCompleted(&evacuators[1], num_tasks - 1);

  // Finalize local evacuators by merging back all locally cached data.
  for (int i = 0; i < num_tasks; i++) {
    evacuators[i]->Finalize();
    delete evacuators[i];
  }
  delete[] evacuators;

  // Finalize pages sequentially.
  for (NewSpacePage* p : newspace_evacuation_candidates_) {
    DCHECK_EQ(p->parallel_compaction_state().Value(),
              MemoryChunk::kCompactingFinalize);
    p->parallel_compaction_state().SetValue(MemoryChunk::kCompactingDone);
  }

  int abandoned_pages = 0;
  for (Page* p : evacuation_candidates_) {
    switch (p->parallel_compaction_state().Value()) {
      case MemoryChunk::ParallelCompactingState::kCompactingAborted:
        // We have partially compacted the page, i.e., some objects may have
        // moved, others are still in place.
        // We need to:
        // - Leave the evacuation candidate flag for later processing of
        //   slots buffer entries.
        // - Leave the slots buffer there for processing of entries added by
        //   the write barrier.
        // - Rescan the page as slot recording in the migration buffer only
        //   happens upon moving (which we potentially didn't do).
        // - Leave the page in the list of pages of a space since we could not
        //   fully evacuate it.
        // - Mark them for rescanning for store buffer entries as we otherwise
        //   might have stale store buffer entries that become "valid" again
        //   after reusing the memory. Note that all existing store buffer
        //   entries of such pages are filtered before rescanning.
        DCHECK(p->IsEvacuationCandidate());
        p->SetFlag(Page::COMPACTION_WAS_ABORTED);
        abandoned_pages++;
        break;
      case MemoryChunk::kCompactingFinalize:
        DCHECK(p->IsEvacuationCandidate());
        DCHECK(p->SweepingDone());
        p->Unlink();
        break;
      case MemoryChunk::kCompactingDone:
        DCHECK(p->IsFlagSet(Page::POPULAR_PAGE));
        DCHECK(p->IsFlagSet(Page::RESCAN_ON_EVACUATION));
        break;
      default:
        // MemoryChunk::kCompactingInProgress.
        UNREACHABLE();
    }
    p->parallel_compaction_state().SetValue(MemoryChunk::kCompactingDone);
  }
  if (FLAG_trace_fragmentation) {
    PrintIsolate(isolate(),
                 "%8.0f ms: compaction: parallel=%d pages=%d aborted=%d "
                 "tasks=%d cores=%d live_bytes=%" V8_PTR_PREFIX
                 "d compaction_speed=%" V8_PTR_PREFIX "d\n",
                 isolate()->time_millis_since_init(), FLAG_parallel_compaction,
                 num_pages, abandoned_pages, num_tasks,
                 base::SysInfo::NumberOfProcessors(), live_bytes,
                 compaction_speed);
  }
}

void MarkCompactCollector::StartParallelCompaction(Evacuator** evacuators,
                                                   int len) {
  compaction_in_progress_ = true;
  for (int i = 1; i < len; i++) {
    CompactionTask* task = new CompactionTask(heap(), evacuators[i]);
    V8::GetCurrentPlatform()->CallOnBackgroundThread(
        task, v8::Platform::kShortRunningTask);
  }

  // Contribute on main thread.
  evacuators[0]->EvacuatePages();
}

void MarkCompactCollector::WaitUntilCompactionCompleted(Evacuator** evacuators,
                                                        int len) {
  // Try to cancel compaction tasks that have not been run (as they might be
  // stuck in a worker queue). Tasks that cannot be canceled, have either
  // already completed or are still running, hence we need to wait for their
  // semaphore signal.
  for (int i = 0; i < len; i++) {
    if (!heap()->isolate()->cancelable_task_manager()->TryAbort(
            evacuators[i]->task_id())) {
      pending_compaction_tasks_semaphore_.Wait();
    }
  }
  compaction_in_progress_ = false;
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
  DCHECK(!p->IsEvacuationCandidate() && !p->SweepingDone());
  DCHECK_EQ(skip_list_mode == REBUILD_SKIP_LIST,
            space->identity() == CODE_SPACE);
  DCHECK((p->skip_list() == NULL) || (skip_list_mode == REBUILD_SKIP_LIST));
  DCHECK(parallelism == MarkCompactCollector::SWEEP_ON_MAIN_THREAD ||
         sweeping_mode == SWEEP_ONLY);

  Address free_start = p->area_start();
  DCHECK(reinterpret_cast<intptr_t>(free_start) % (32 * kPointerSize) == 0);

  // If we use the skip list for code space pages, we have to lock the skip
  // list because it could be accessed concurrently by the runtime or the
  // deoptimizer.
  SkipList* skip_list = p->skip_list();
  if ((skip_list_mode == REBUILD_SKIP_LIST) && skip_list) {
    skip_list->Clear();
  }

  intptr_t freed_bytes = 0;
  intptr_t max_freed_bytes = 0;
  int curr_region = -1;

  LiveObjectIterator<kBlackObjects> it(p);
  HeapObject* object = NULL;
  while ((object = it.Next()) != NULL) {
    DCHECK(Marking::IsBlack(Marking::MarkBitFrom(object)));
    Address free_end = object->address();
    if (free_end != free_start) {
      int size = static_cast<int>(free_end - free_start);
      if (free_space_mode == ZAP_FREE_SPACE) {
        memset(free_start, 0xcc, size);
      }
      freed_bytes = Free<parallelism>(space, free_list, free_start, size);
      max_freed_bytes = Max(freed_bytes, max_freed_bytes);
    }
    Map* map = object->synchronized_map();
    int size = object->SizeFromMap(map);
    if (sweeping_mode == SWEEP_AND_VISIT_LIVE_OBJECTS) {
      object->IterateBody(map->instance_type(), size, v);
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

  // Clear the mark bits of that page and reset live bytes count.
  Bitmap::Clear(p);

  if (free_start != p->area_end()) {
    int size = static_cast<int>(p->area_end() - free_start);
    if (free_space_mode == ZAP_FREE_SPACE) {
      memset(free_start, 0xcc, size);
    }
    freed_bytes = Free<parallelism>(space, free_list, free_start, size);
    max_freed_bytes = Max(freed_bytes, max_freed_bytes);
  }
  p->concurrent_sweeping_state().SetValue(Page::kSweepingDone);
  return FreeList::GuaranteedAllocatable(static_cast<int>(max_freed_bytes));
}


void MarkCompactCollector::InvalidateCode(Code* code) {
  if (heap_->incremental_marking()->IsCompacting() &&
      !ShouldSkipEvacuationSlotRecording(code)) {
    DCHECK(compacting_);

    // If the object is white than no slots were recorded on it yet.
    MarkBit mark_bit = Marking::MarkBitFrom(code);
    if (Marking::IsWhite(mark_bit)) return;

    // Ignore all slots that might have been recorded in the body of the
    // deoptimized code object. Assumption: no slots will be recorded for
    // this object after invalidating it.
    RemoveObjectSlots(code->instruction_start(),
                      code->address() + code->Size());
  }
}


// Return true if the given code is deoptimized or will be deoptimized.
bool MarkCompactCollector::WillBeDeoptimized(Code* code) {
  return code->is_optimized_code() && code->marked_for_deoptimization();
}


void MarkCompactCollector::RemoveObjectSlots(Address start_slot,
                                             Address end_slot) {
  // Remove entries by replacing them with an old-space slot containing a smi
  // that is located in an unmovable page.
  for (Page* p : evacuation_candidates_) {
    DCHECK(p->IsEvacuationCandidate() ||
           p->IsFlagSet(Page::RESCAN_ON_EVACUATION));
    if (p->IsEvacuationCandidate()) {
      SlotsBuffer::RemoveObjectSlots(heap_, p->slots_buffer(), start_slot,
                                     end_slot);
    }
  }
}


#ifdef VERIFY_HEAP
static void VerifyAllBlackObjects(MemoryChunk* page) {
  LiveObjectIterator<kAllLiveObjects> it(page);
  HeapObject* object = NULL;
  while ((object = it.Next()) != NULL) {
    CHECK(Marking::IsBlack(Marking::MarkBitFrom(object)));
  }
}
#endif  // VERIFY_HEAP


bool MarkCompactCollector::VisitLiveObjects(MemoryChunk* page,
                                            HeapObjectVisitor* visitor,
                                            IterationMode mode) {
#ifdef VERIFY_HEAP
  VerifyAllBlackObjects(page);
#endif  // VERIFY_HEAP

  LiveObjectIterator<kBlackObjects> it(page);
  HeapObject* object = nullptr;
  while ((object = it.Next()) != nullptr) {
    DCHECK(Marking::IsBlack(Marking::MarkBitFrom(object)));
    if (!visitor->Visit(object)) {
      if (mode == kClearMarkbits) {
        page->markbits()->ClearRange(
            page->AddressToMarkbitIndex(page->area_start()),
            page->AddressToMarkbitIndex(object->address()));
        if (page->old_to_new_slots() != nullptr) {
          page->old_to_new_slots()->RemoveRange(
              0, static_cast<int>(object->address() - page->address()));
        }
        RecomputeLiveBytes(page);
      }
      return false;
    }
  }
  if (mode == kClearMarkbits) {
    Bitmap::Clear(page);
  }
  return true;
}


void MarkCompactCollector::RecomputeLiveBytes(MemoryChunk* page) {
  LiveObjectIterator<kBlackObjects> it(page);
  int new_live_size = 0;
  HeapObject* object = nullptr;
  while ((object = it.Next()) != nullptr) {
    new_live_size += object->Size();
  }
  page->SetLiveBytes(new_live_size);
}


void MarkCompactCollector::VisitLiveObjectsBody(Page* page,
                                                ObjectVisitor* visitor) {
#ifdef VERIFY_HEAP
  VerifyAllBlackObjects(page);
#endif  // VERIFY_HEAP

  LiveObjectIterator<kBlackObjects> it(page);
  HeapObject* object = NULL;
  while ((object = it.Next()) != NULL) {
    DCHECK(Marking::IsBlack(Marking::MarkBitFrom(object)));
    Map* map = object->synchronized_map();
    int size = object->SizeFromMap(map);
    object->IterateBody(map->instance_type(), size, visitor);
  }
}


void MarkCompactCollector::SweepAbortedPages() {
  // Second pass on aborted pages.
  for (Page* p : evacuation_candidates_) {
    if (p->IsFlagSet(Page::COMPACTION_WAS_ABORTED)) {
      p->ClearFlag(MemoryChunk::COMPACTION_WAS_ABORTED);
      p->concurrent_sweeping_state().SetValue(Page::kSweepingInProgress);
      PagedSpace* space = static_cast<PagedSpace*>(p->owner());
      switch (space->identity()) {
        case OLD_SPACE:
          Sweep<SWEEP_ONLY, SWEEP_ON_MAIN_THREAD, IGNORE_SKIP_LIST,
                IGNORE_FREE_SPACE>(space, nullptr, p, nullptr);
          break;
        case CODE_SPACE:
          if (FLAG_zap_code_space) {
            Sweep<SWEEP_ONLY, SWEEP_ON_MAIN_THREAD, REBUILD_SKIP_LIST,
                  ZAP_FREE_SPACE>(space, NULL, p, nullptr);
          } else {
            Sweep<SWEEP_ONLY, SWEEP_ON_MAIN_THREAD, REBUILD_SKIP_LIST,
                  IGNORE_FREE_SPACE>(space, NULL, p, nullptr);
          }
          break;
        default:
          UNREACHABLE();
          break;
      }
    }
  }
}


void MarkCompactCollector::EvacuateNewSpaceAndCandidates() {
  GCTracer::Scope gc_scope(heap()->tracer(), GCTracer::Scope::MC_EVACUATE);
  Heap::RelocationLock relocation_lock(heap());

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_EVACUATE_NEW_SPACE);
    EvacuationScope evacuation_scope(this);

    EvacuateNewSpacePrologue();
    EvacuatePagesInParallel();
    EvacuateNewSpaceEpilogue();
    heap()->new_space()->set_age_mark(heap()->new_space()->top());
  }

  UpdatePointersAfterEvacuation();

  // Give pages that are queued to be freed back to the OS. Note that filtering
  // slots only handles old space (for unboxed doubles), and thus map space can
  // still contain stale pointers. We only free the chunks after pointer updates
  // to still have access to page headers.
  heap()->FreeQueuedChunks();

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_EVACUATE_CLEAN_UP);
    // After updating all pointers, we can finally sweep the aborted pages,
    // effectively overriding any forward pointers.
    SweepAbortedPages();

    // EvacuateNewSpaceAndCandidates iterates over new space objects and for
    // ArrayBuffers either re-registers them as live or promotes them. This is
    // needed to properly free them.
    heap()->array_buffer_tracker()->FreeDead(false);

    // Deallocate evacuated candidate pages.
    ReleaseEvacuationCandidates();
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && !sweeping_in_progress_) {
    VerifyEvacuation(heap());
  }
#endif
}


void MarkCompactCollector::UpdatePointersAfterEvacuation() {
  GCTracer::Scope gc_scope(heap()->tracer(),
                           GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS);
  {
    GCTracer::Scope gc_scope(
        heap()->tracer(),
        GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_TO_EVACUATED);
    UpdateSlotsRecordedIn(migration_slots_buffer_);
    if (FLAG_trace_fragmentation_verbose) {
      PrintF("  migration slots buffer: %d\n",
             SlotsBuffer::SizeOfChain(migration_slots_buffer_));
    }
    slots_buffer_allocator_->DeallocateChain(&migration_slots_buffer_);
    DCHECK(migration_slots_buffer_ == NULL);

    // TODO(hpayer): Process the slots buffers in parallel. This has to be done
    // after evacuation of all pages finishes.
    int buffers = evacuation_slots_buffers_.length();
    for (int i = 0; i < buffers; i++) {
      SlotsBuffer* buffer = evacuation_slots_buffers_[i];
      UpdateSlotsRecordedIn(buffer);
      slots_buffer_allocator_->DeallocateChain(&buffer);
    }
    evacuation_slots_buffers_.Rewind(0);
  }

  // Second pass: find pointers to new space and update them.
  PointersUpdatingVisitor updating_visitor(heap());

  {
    GCTracer::Scope gc_scope(
        heap()->tracer(), GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_TO_NEW);
    // Update pointers in to space.
    SemiSpaceIterator to_it(heap()->new_space());
    for (HeapObject* object = to_it.Next(); object != NULL;
         object = to_it.Next()) {
      Map* map = object->map();
      object->IterateBody(map->instance_type(), object->SizeFromMap(map),
                          &updating_visitor);
    }
    // Update roots.
    heap_->IterateRoots(&updating_visitor, VISIT_ALL_IN_SWEEP_NEWSPACE);

    RememberedSet<OLD_TO_NEW>::IterateWithWrapper(heap_, UpdatePointer);
  }

  {
    GCTracer::Scope gc_scope(
        heap()->tracer(),
        GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_BETWEEN_EVACUATED);
    for (Page* p : evacuation_candidates_) {
      DCHECK(p->IsEvacuationCandidate() ||
             p->IsFlagSet(Page::RESCAN_ON_EVACUATION));

      if (p->IsEvacuationCandidate()) {
        UpdateSlotsRecordedIn(p->slots_buffer());
        if (FLAG_trace_fragmentation_verbose) {
          PrintF("  page %p slots buffer: %d\n", reinterpret_cast<void*>(p),
                 SlotsBuffer::SizeOfChain(p->slots_buffer()));
        }
        slots_buffer_allocator_->DeallocateChain(p->slots_buffer_address());

        // Important: skip list should be cleared only after roots were updated
        // because root iteration traverses the stack and might have to find
        // code objects from non-updated pc pointing into evacuation candidate.
        SkipList* list = p->skip_list();
        if (list != NULL) list->Clear();

        // First pass on aborted pages, fixing up all live objects.
        if (p->IsFlagSet(Page::COMPACTION_WAS_ABORTED)) {
          p->ClearEvacuationCandidate();
          VisitLiveObjectsBody(p, &updating_visitor);
        }
      }

      if (p->IsFlagSet(Page::RESCAN_ON_EVACUATION)) {
        if (FLAG_gc_verbose) {
          PrintF("Sweeping 0x%" V8PRIxPTR " during evacuation.\n",
                 reinterpret_cast<intptr_t>(p));
        }
        PagedSpace* space = static_cast<PagedSpace*>(p->owner());
        p->ClearFlag(MemoryChunk::RESCAN_ON_EVACUATION);
        p->concurrent_sweeping_state().SetValue(Page::kSweepingInProgress);

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

  {
    GCTracer::Scope gc_scope(heap()->tracer(),
                             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_WEAK);
    heap_->string_table()->Iterate(&updating_visitor);

    // Update pointers from external string table.
    heap_->UpdateReferencesInExternalStringTable(
        &UpdateReferenceInExternalStringTableEntry);

    EvacuationWeakObjectRetainer evacuation_object_retainer;
    heap()->ProcessAllWeakReferences(&evacuation_object_retainer);
  }
}


void MarkCompactCollector::ReleaseEvacuationCandidates() {
  for (Page* p : evacuation_candidates_) {
    if (!p->IsEvacuationCandidate()) continue;
    PagedSpace* space = static_cast<PagedSpace*>(p->owner());
    space->Free(p->area_start(), p->area_size());
    p->ResetLiveBytes();
    CHECK(p->SweepingDone());
    space->ReleasePage(p, true);
  }
  evacuation_candidates_.Rewind(0);
  compacting_ = false;
  heap()->FreeQueuedChunks();
}


int MarkCompactCollector::SweepInParallel(PagedSpace* space,
                                          int required_freed_bytes,
                                          int max_pages) {
  int max_freed = 0;
  int max_freed_overall = 0;
  int page_count = 0;
  for (Page* p : sweeping_list(space)) {
    max_freed = SweepInParallel(p, space);
    DCHECK(max_freed >= 0);
    if (required_freed_bytes > 0 && max_freed >= required_freed_bytes) {
      return max_freed;
    }
    max_freed_overall = Max(max_freed, max_freed_overall);
    page_count++;
    if (max_pages > 0 && page_count >= max_pages) {
      break;
    }
  }
  return max_freed_overall;
}


int MarkCompactCollector::SweepInParallel(Page* page, PagedSpace* space) {
  int max_freed = 0;
  if (page->mutex()->TryLock()) {
    // If this page was already swept in the meantime, we can return here.
    if (page->concurrent_sweeping_state().Value() != Page::kSweepingPending) {
      page->mutex()->Unlock();
      return 0;
    }
    page->concurrent_sweeping_state().SetValue(Page::kSweepingInProgress);
    FreeList* free_list;
    FreeList private_free_list(space);
    if (space->identity() == OLD_SPACE) {
      free_list = free_list_old_space_.get();
      max_freed =
          Sweep<SWEEP_ONLY, SWEEP_IN_PARALLEL, IGNORE_SKIP_LIST,
                IGNORE_FREE_SPACE>(space, &private_free_list, page, NULL);
    } else if (space->identity() == CODE_SPACE) {
      free_list = free_list_code_space_.get();
      max_freed =
          Sweep<SWEEP_ONLY, SWEEP_IN_PARALLEL, REBUILD_SKIP_LIST,
                IGNORE_FREE_SPACE>(space, &private_free_list, page, NULL);
    } else {
      free_list = free_list_map_space_.get();
      max_freed =
          Sweep<SWEEP_ONLY, SWEEP_IN_PARALLEL, IGNORE_SKIP_LIST,
                IGNORE_FREE_SPACE>(space, &private_free_list, page, NULL);
    }
    free_list->Concatenate(&private_free_list);
    page->concurrent_sweeping_state().SetValue(Page::kSweepingDone);
    page->mutex()->Unlock();
  }
  return max_freed;
}


void MarkCompactCollector::StartSweepSpace(PagedSpace* space) {
  space->ClearStats();

  PageIterator it(space);

  int will_be_swept = 0;
  bool unused_page_present = false;

  while (it.has_next()) {
    Page* p = it.next();
    DCHECK(p->SweepingDone());

    if (p->IsFlagSet(Page::RESCAN_ON_EVACUATION) ||
        p->IsEvacuationCandidate()) {
      // Will be processed in EvacuateNewSpaceAndCandidates.
      DCHECK(evacuation_candidates_.length() > 0);
      continue;
    }

    if (p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
      // We need to sweep the page to get it into an iterable state again. Note
      // that this adds unusable memory into the free list that is later on
      // (in the free list) dropped again. Since we only use the flag for
      // testing this is fine.
      p->concurrent_sweeping_state().SetValue(Page::kSweepingInProgress);
      Sweep<SWEEP_ONLY, SWEEP_ON_MAIN_THREAD, IGNORE_SKIP_LIST,
            IGNORE_FREE_SPACE>(space, nullptr, p, nullptr);
      continue;
    }

    // One unused page is kept, all further are released before sweeping them.
    if (p->LiveBytes() == 0) {
      if (unused_page_present) {
        if (FLAG_gc_verbose) {
          PrintIsolate(isolate(), "sweeping: released page: %p", p);
        }
        space->ReleasePage(p, false);
        continue;
      }
      unused_page_present = true;
    }

    p->concurrent_sweeping_state().SetValue(Page::kSweepingPending);
    sweeping_list(space).push_back(p);
    int to_sweep = p->area_size() - p->LiveBytes();
    space->accounting_stats_.ShrinkSpace(to_sweep);
    will_be_swept++;
  }

  if (FLAG_gc_verbose) {
    PrintIsolate(isolate(), "sweeping: space=%s initialized_for_sweeping=%d",
                 AllocationSpaceName(space->identity()), will_be_swept);
  }
  std::sort(sweeping_list(space).begin(), sweeping_list(space).end(),
            [](Page* a, Page* b) { return a->LiveBytes() < b->LiveBytes(); });
}


void MarkCompactCollector::SweepSpaces() {
  GCTracer::Scope gc_scope(heap()->tracer(), GCTracer::Scope::MC_SWEEP);
  double start_time = 0.0;
  if (FLAG_print_cumulative_gc_stat) {
    start_time = heap_->MonotonicallyIncreasingTimeInMs();
  }

#ifdef DEBUG
  state_ = SWEEP_SPACES;
#endif

  {
    sweeping_in_progress_ = true;
    {
      GCTracer::Scope sweep_scope(heap()->tracer(),
                                  GCTracer::Scope::MC_SWEEP_OLD);
      StartSweepSpace(heap()->old_space());
    }
    {
      GCTracer::Scope sweep_scope(heap()->tracer(),
                                  GCTracer::Scope::MC_SWEEP_CODE);
      StartSweepSpace(heap()->code_space());
    }
    {
      GCTracer::Scope sweep_scope(heap()->tracer(),
                                  GCTracer::Scope::MC_SWEEP_MAP);
      StartSweepSpace(heap()->map_space());
    }
    if (FLAG_concurrent_sweeping) {
      StartSweeperThreads();
    }
  }

  // Deallocate unmarked large objects.
  heap_->lo_space()->FreeUnmarkedObjects();

  if (FLAG_print_cumulative_gc_stat) {
    heap_->tracer()->AddSweepingTime(heap_->MonotonicallyIncreasingTimeInMs() -
                                     start_time);
  }
}


void MarkCompactCollector::ParallelSweepSpacesComplete() {
  sweeping_list(heap()->old_space()).clear();
  sweeping_list(heap()->code_space()).clear();
  sweeping_list(heap()->map_space()).clear();
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


void MarkCompactCollector::RecordCodeEntrySlot(HeapObject* object, Address slot,
                                               Code* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  if (target_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(object)) {
    if (!SlotsBuffer::AddTo(slots_buffer_allocator_,
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
      RelocInfo rinfo(isolate(), pc, RelocInfo::CODE_TARGET, 0, host);
      RecordRelocSlot(&rinfo, target);
    }
  }
}

}  // namespace internal
}  // namespace v8

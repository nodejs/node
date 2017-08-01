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
#include "src/heap/concurrent-marking.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/item-parallel-job.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/page-parallel-job.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/workstealing-marking-deque.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/tracing/tracing-category-observer.h"
#include "src/utils-inl.h"
#include "src/v8.h"
#include "src/v8threads.h"

namespace v8 {
namespace internal {


const char* Marking::kWhiteBitPattern = "00";
const char* Marking::kBlackBitPattern = "11";
const char* Marking::kGreyBitPattern = "10";
const char* Marking::kImpossibleBitPattern = "01";

// The following has to hold in order for {ObjectMarking::MarkBitFrom} to not
// produce invalid {kImpossibleBitPattern} in the marking bitmap by overlapping.
STATIC_ASSERT(Heap::kMinObjectSizeInWords >= 2);

// =============================================================================
// Verifiers
// =============================================================================

#ifdef VERIFY_HEAP
namespace {

class MarkingVerifier : public ObjectVisitor, public RootVisitor {
 public:
  virtual void Run() = 0;

 protected:
  explicit MarkingVerifier(Heap* heap) : heap_(heap) {}

  virtual MarkingState marking_state(MemoryChunk* chunk) = 0;

  virtual void VerifyPointers(Object** start, Object** end) = 0;

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    VerifyPointers(start, end);
  }

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    VerifyPointers(start, end);
  }

  void VerifyRoots(VisitMode mode);
  void VerifyMarkingOnPage(const Page& page, const MarkingState& state,
                           Address start, Address end);
  void VerifyMarking(NewSpace* new_space);
  void VerifyMarking(PagedSpace* paged_space);

  Heap* heap_;
};

void MarkingVerifier::VerifyRoots(VisitMode mode) {
  heap_->IterateStrongRoots(this, mode);
}

void MarkingVerifier::VerifyMarkingOnPage(const Page& page,
                                          const MarkingState& state,
                                          Address start, Address end) {
  HeapObject* object;
  Address next_object_must_be_here_or_later = start;
  for (Address current = start; current < end;) {
    object = HeapObject::FromAddress(current);
    // One word fillers at the end of a black area can be grey.
    if (ObjectMarking::IsBlackOrGrey(object, state) &&
        object->map() != heap_->one_pointer_filler_map()) {
      CHECK(ObjectMarking::IsBlack(object, state));
      CHECK(current >= next_object_must_be_here_or_later);
      object->Iterate(this);
      next_object_must_be_here_or_later = current + object->Size();
      // The object is either part of a black area of black allocation or a
      // regular black object
      CHECK(
          state.bitmap()->AllBitsSetInRange(
              page.AddressToMarkbitIndex(current),
              page.AddressToMarkbitIndex(next_object_must_be_here_or_later)) ||
          state.bitmap()->AllBitsClearInRange(
              page.AddressToMarkbitIndex(current + kPointerSize * 2),
              page.AddressToMarkbitIndex(next_object_must_be_here_or_later)));
      current = next_object_must_be_here_or_later;
    } else {
      current += kPointerSize;
    }
  }
}

void MarkingVerifier::VerifyMarking(NewSpace* space) {
  Address end = space->top();
  // The bottom position is at the start of its page. Allows us to use
  // page->area_start() as start of range on all pages.
  CHECK_EQ(space->bottom(), Page::FromAddress(space->bottom())->area_start());

  PageRange range(space->bottom(), end);
  for (auto it = range.begin(); it != range.end();) {
    Page* page = *(it++);
    Address limit = it != range.end() ? page->area_end() : end;
    CHECK(limit == end || !page->Contains(end));
    VerifyMarkingOnPage(*page, marking_state(page), page->area_start(), limit);
  }
}

void MarkingVerifier::VerifyMarking(PagedSpace* space) {
  for (Page* p : *space) {
    VerifyMarkingOnPage(*p, marking_state(p), p->area_start(), p->area_end());
  }
}

class FullMarkingVerifier : public MarkingVerifier {
 public:
  explicit FullMarkingVerifier(Heap* heap) : MarkingVerifier(heap) {}

  void Run() override {
    VerifyRoots(VISIT_ONLY_STRONG);
    VerifyMarking(heap_->new_space());
    VerifyMarking(heap_->old_space());
    VerifyMarking(heap_->code_space());
    VerifyMarking(heap_->map_space());

    LargeObjectIterator it(heap_->lo_space());
    for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
      if (ObjectMarking::IsBlackOrGrey(obj, marking_state(obj))) {
        obj->Iterate(this);
      }
    }
  }

 protected:
  MarkingState marking_state(MemoryChunk* chunk) override {
    return MarkingState::Internal(chunk);
  }

  MarkingState marking_state(HeapObject* object) {
    return MarkingState::Internal(object);
  }

  void VerifyPointers(Object** start, Object** end) override {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        CHECK(ObjectMarking::IsBlackOrGrey(object, marking_state(object)));
      }
    }
  }

  void VisitEmbeddedPointer(Code* host, RelocInfo* rinfo) override {
    DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
    if (!host->IsWeakObject(rinfo->target_object())) {
      Object* p = rinfo->target_object();
      VisitPointer(host, &p);
    }
  }

  void VisitCellPointer(Code* host, RelocInfo* rinfo) override {
    DCHECK(rinfo->rmode() == RelocInfo::CELL);
    if (!host->IsWeakObject(rinfo->target_cell())) {
      ObjectVisitor::VisitCellPointer(host, rinfo);
    }
  }
};

class YoungGenerationMarkingVerifier : public MarkingVerifier {
 public:
  explicit YoungGenerationMarkingVerifier(Heap* heap) : MarkingVerifier(heap) {}

  MarkingState marking_state(MemoryChunk* chunk) override {
    return MarkingState::External(chunk);
  }

  MarkingState marking_state(HeapObject* object) {
    return MarkingState::External(object);
  }

  void Run() override {
    VerifyRoots(VISIT_ALL_IN_SCAVENGE);
    VerifyMarking(heap_->new_space());
  }

  void VerifyPointers(Object** start, Object** end) override {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        if (!heap_->InNewSpace(object)) return;
        CHECK(ObjectMarking::IsBlackOrGrey(object, marking_state(object)));
      }
    }
  }
};

class EvacuationVerifier : public ObjectVisitor, public RootVisitor {
 public:
  virtual void Run() = 0;

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    VerifyPointers(start, end);
  }

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    VerifyPointers(start, end);
  }

 protected:
  explicit EvacuationVerifier(Heap* heap) : heap_(heap) {}

  inline Heap* heap() { return heap_; }

  virtual void VerifyPointers(Object** start, Object** end) = 0;

  void VerifyRoots(VisitMode mode);
  void VerifyEvacuationOnPage(Address start, Address end);
  void VerifyEvacuation(NewSpace* new_space);
  void VerifyEvacuation(PagedSpace* paged_space);

  Heap* heap_;
};

void EvacuationVerifier::VerifyRoots(VisitMode mode) {
  heap_->IterateStrongRoots(this, mode);
}

void EvacuationVerifier::VerifyEvacuationOnPage(Address start, Address end) {
  Address current = start;
  while (current < end) {
    HeapObject* object = HeapObject::FromAddress(current);
    if (!object->IsFiller()) object->Iterate(this);
    current += object->Size();
  }
}

void EvacuationVerifier::VerifyEvacuation(NewSpace* space) {
  PageRange range(space->bottom(), space->top());
  for (auto it = range.begin(); it != range.end();) {
    Page* page = *(it++);
    Address current = page->area_start();
    Address limit = it != range.end() ? page->area_end() : space->top();
    CHECK(limit == space->top() || !page->Contains(space->top()));
    VerifyEvacuationOnPage(current, limit);
  }
}

void EvacuationVerifier::VerifyEvacuation(PagedSpace* space) {
  for (Page* p : *space) {
    if (p->IsEvacuationCandidate()) continue;
    if (p->Contains(space->top()))
      heap_->CreateFillerObjectAt(
          space->top(), static_cast<int>(space->limit() - space->top()),
          ClearRecordedSlots::kNo);

    VerifyEvacuationOnPage(p->area_start(), p->area_end());
  }
}

class FullEvacuationVerifier : public EvacuationVerifier {
 public:
  explicit FullEvacuationVerifier(Heap* heap) : EvacuationVerifier(heap) {}

  void Run() override {
    VerifyRoots(VISIT_ALL);
    VerifyEvacuation(heap_->new_space());
    VerifyEvacuation(heap_->old_space());
    VerifyEvacuation(heap_->code_space());
    VerifyEvacuation(heap_->map_space());
  }

 protected:
  void VerifyPointers(Object** start, Object** end) override {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        if (heap()->InNewSpace(object)) {
          CHECK(heap()->InToSpace(object));
        }
        CHECK(!MarkCompactCollector::IsOnEvacuationCandidate(object));
      }
    }
  }
};

class YoungGenerationEvacuationVerifier : public EvacuationVerifier {
 public:
  explicit YoungGenerationEvacuationVerifier(Heap* heap)
      : EvacuationVerifier(heap) {}

  void Run() override {
    VerifyRoots(VISIT_ALL_IN_SCAVENGE);
    VerifyEvacuation(heap_->new_space());
    VerifyEvacuation(heap_->old_space());
    VerifyEvacuation(heap_->code_space());
    VerifyEvacuation(heap_->map_space());
  }

 protected:
  void VerifyPointers(Object** start, Object** end) override {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        CHECK_IMPLIES(heap()->InNewSpace(object), heap()->InToSpace(object));
      }
    }
  }
};

}  // namespace
#endif  // VERIFY_HEAP

// =============================================================================
// MarkCompactCollectorBase, MinorMarkCompactCollector, MarkCompactCollector
// =============================================================================

static int NumberOfAvailableCores() {
  return Max(
      1, static_cast<int>(
             V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads()));
}

int MarkCompactCollectorBase::NumberOfParallelCompactionTasks(int pages) {
  return FLAG_parallel_compaction ? Min(NumberOfAvailableCores(), pages) : 1;
}

int MarkCompactCollectorBase::NumberOfPointerUpdateTasks(int pages) {
  // Limit the number of update tasks as task creation often dominates the
  // actual work that is being done.
  static const int kMaxPointerUpdateTasks = 8;
  return FLAG_parallel_pointer_update
             ? Min(kMaxPointerUpdateTasks, Min(NumberOfAvailableCores(), pages))
             : 1;
}

int MinorMarkCompactCollector::NumberOfMarkingTasks() {
  return FLAG_minor_mc_parallel_marking
             ? Min(NumberOfAvailableCores(), kNumMarkers)
             : 1;
}

MarkCompactCollector::MarkCompactCollector(Heap* heap)
    : MarkCompactCollectorBase(heap),
      page_parallel_job_semaphore_(0),
#ifdef DEBUG
      state_(IDLE),
#endif
      was_marked_incrementally_(false),
      evacuation_(false),
      compacting_(false),
      black_allocation_(false),
      have_code_to_deoptimize_(false),
      marking_deque_(heap),
      code_flusher_(nullptr),
      sweeper_(heap) {
}

void MarkCompactCollector::SetUp() {
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);
  marking_deque()->SetUp();

  if (FLAG_flush_code) {
    code_flusher_ = new CodeFlusher(isolate());
    if (FLAG_trace_code_flushing) {
      PrintF("[code-flushing is now on]\n");
    }
  }
}

void MinorMarkCompactCollector::SetUp() {}

void MarkCompactCollector::TearDown() {
  AbortCompaction();
  marking_deque()->TearDown();
  delete code_flusher_;
}

void MinorMarkCompactCollector::TearDown() {}

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

bool MarkCompactCollector::StartCompaction() {
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

    compacting_ = evacuation_candidates_.length() > 0;
  }

  return compacting_;
}

void MarkCompactCollector::CollectGarbage() {
  // Make sure that Prepare() has been called. The individual steps below will
  // update the state as they proceed.
  DCHECK(state_ == PREPARE_GC);

  heap()->minor_mark_compact_collector()->CleanupSweepToIteratePages();

  MarkLiveObjects();

  DCHECK(heap_->incremental_marking()->IsStopped());

  ClearNonLiveReferences();

  RecordObjectStats();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    FullMarkingVerifier verifier(heap());
    verifier.Run();
  }
#endif

  StartSweepSpaces();

  Evacuate();

  Finish();
}

#ifdef VERIFY_HEAP
void MarkCompactCollector::VerifyMarkbitsAreClean(PagedSpace* space) {
  for (Page* p : *space) {
    const MarkingState state = MarkingState::Internal(p);
    CHECK(state.bitmap()->IsClean());
    CHECK_EQ(0, state.live_bytes());
  }
}


void MarkCompactCollector::VerifyMarkbitsAreClean(NewSpace* space) {
  for (Page* p : PageRange(space->bottom(), space->top())) {
    const MarkingState state = MarkingState::Internal(p);
    CHECK(state.bitmap()->IsClean());
    CHECK_EQ(0, state.live_bytes());
  }
}


void MarkCompactCollector::VerifyMarkbitsAreClean() {
  VerifyMarkbitsAreClean(heap_->old_space());
  VerifyMarkbitsAreClean(heap_->code_space());
  VerifyMarkbitsAreClean(heap_->map_space());
  VerifyMarkbitsAreClean(heap_->new_space());

  LargeObjectIterator it(heap_->lo_space());
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    CHECK(ObjectMarking::IsWhite(obj, MarkingState::Internal(obj)));
    CHECK_EQ(0, MarkingState::Internal(obj).live_bytes());
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
  for (Page* p : *space) {
    MarkingState::Internal(p).ClearLiveness();
  }
}


static void ClearMarkbitsInNewSpace(NewSpace* space) {
  for (Page* p : *space) {
    MarkingState::Internal(p).ClearLiveness();
  }
}


void MarkCompactCollector::ClearMarkbits() {
  ClearMarkbitsInPagedSpace(heap_->code_space());
  ClearMarkbitsInPagedSpace(heap_->map_space());
  ClearMarkbitsInPagedSpace(heap_->old_space());
  ClearMarkbitsInNewSpace(heap_->new_space());
  heap_->lo_space()->ClearMarkingStateOfLiveObjects();
}

class MarkCompactCollector::Sweeper::SweeperTask : public v8::Task {
 public:
  SweeperTask(Sweeper* sweeper, base::Semaphore* pending_sweeper_tasks,
              base::AtomicNumber<intptr_t>* num_sweeping_tasks,
              AllocationSpace space_to_start)
      : sweeper_(sweeper),
        pending_sweeper_tasks_(pending_sweeper_tasks),
        num_sweeping_tasks_(num_sweeping_tasks),
        space_to_start_(space_to_start) {}

  virtual ~SweeperTask() {}

 private:
  // v8::Task overrides.
  void Run() override {
    DCHECK_GE(space_to_start_, FIRST_SPACE);
    DCHECK_LE(space_to_start_, LAST_PAGED_SPACE);
    const int offset = space_to_start_ - FIRST_SPACE;
    const int num_spaces = LAST_PAGED_SPACE - FIRST_SPACE + 1;
    for (int i = 0; i < num_spaces; i++) {
      const int space_id = FIRST_SPACE + ((i + offset) % num_spaces);
      DCHECK_GE(space_id, FIRST_SPACE);
      DCHECK_LE(space_id, LAST_PAGED_SPACE);
      sweeper_->ParallelSweepSpace(static_cast<AllocationSpace>(space_id), 0);
    }
    num_sweeping_tasks_->Decrement(1);
    pending_sweeper_tasks_->Signal();
  }

  Sweeper* sweeper_;
  base::Semaphore* pending_sweeper_tasks_;
  base::AtomicNumber<intptr_t>* num_sweeping_tasks_;
  AllocationSpace space_to_start_;

  DISALLOW_COPY_AND_ASSIGN(SweeperTask);
};

void MarkCompactCollector::Sweeper::StartSweeping() {
  sweeping_in_progress_ = true;
  ForAllSweepingSpaces([this](AllocationSpace space) {
    std::sort(sweeping_list_[space].begin(), sweeping_list_[space].end(),
              [](Page* a, Page* b) {
                return MarkingState::Internal(a).live_bytes() <
                       MarkingState::Internal(b).live_bytes();
              });
  });
}

void MarkCompactCollector::Sweeper::StartSweeperTasks() {
  if (FLAG_concurrent_sweeping && sweeping_in_progress_) {
    ForAllSweepingSpaces([this](AllocationSpace space) {
      if (space == NEW_SPACE) return;
      num_sweeping_tasks_.Increment(1);
      semaphore_counter_++;
      V8::GetCurrentPlatform()->CallOnBackgroundThread(
          new SweeperTask(this, &pending_sweeper_tasks_semaphore_,
                          &num_sweeping_tasks_, space),
          v8::Platform::kShortRunningTask);
    });
  }
}

void MarkCompactCollector::Sweeper::SweepOrWaitUntilSweepingCompleted(
    Page* page) {
  if (!page->SweepingDone()) {
    ParallelSweepPage(page, page->owner()->identity());
    if (!page->SweepingDone()) {
      // We were not able to sweep that page, i.e., a concurrent
      // sweeper thread currently owns this page. Wait for the sweeper
      // thread to be done with this page.
      page->WaitUntilSweepingCompleted();
    }
  }
}

void MarkCompactCollector::SweepAndRefill(CompactionSpace* space) {
  if (FLAG_concurrent_sweeping && sweeper().sweeping_in_progress()) {
    sweeper().ParallelSweepSpace(space->identity(), 0);
    space->RefillFreeList();
  }
}

Page* MarkCompactCollector::Sweeper::GetSweptPageSafe(PagedSpace* space) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  SweptList& list = swept_list_[space->identity()];
  if (list.length() > 0) {
    return list.RemoveLast();
  }
  return nullptr;
}

void MarkCompactCollector::Sweeper::EnsureCompleted() {
  if (!sweeping_in_progress_) return;

  // If sweeping is not completed or not running at all, we try to complete it
  // here.
  ForAllSweepingSpaces(
      [this](AllocationSpace space) { ParallelSweepSpace(space, 0); });

  if (FLAG_concurrent_sweeping) {
    while (semaphore_counter_ > 0) {
      pending_sweeper_tasks_semaphore_.Wait();
      semaphore_counter_--;
    }
  }

  ForAllSweepingSpaces([this](AllocationSpace space) {
    if (space == NEW_SPACE) {
      swept_list_[NEW_SPACE].Clear();
    }
    DCHECK(sweeping_list_[space].empty());
  });
  sweeping_in_progress_ = false;
}

void MarkCompactCollector::Sweeper::EnsureNewSpaceCompleted() {
  if (!sweeping_in_progress_) return;
  if (!FLAG_concurrent_sweeping || sweeping_in_progress()) {
    for (Page* p : *heap_->new_space()) {
      SweepOrWaitUntilSweepingCompleted(p);
    }
  }
}

void MarkCompactCollector::EnsureSweepingCompleted() {
  if (!sweeper().sweeping_in_progress()) return;

  sweeper().EnsureCompleted();
  heap()->old_space()->RefillFreeList();
  heap()->code_space()->RefillFreeList();
  heap()->map_space()->RefillFreeList();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && !evacuation()) {
    FullEvacuationVerifier verifier(heap_);
    verifier.Run();
  }
#endif

  if (heap()->memory_allocator()->unmapper()->has_delayed_chunks())
    heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
}

bool MarkCompactCollector::Sweeper::AreSweeperTasksRunning() {
  return num_sweeping_tasks_.Value() != 0;
}

void MarkCompactCollector::ComputeEvacuationHeuristics(
    size_t area_size, int* target_fragmentation_percent,
    size_t* max_evacuated_bytes) {
  // For memory reducing and optimize for memory mode we directly define both
  // constants.
  const int kTargetFragmentationPercentForReduceMemory = 20;
  const size_t kMaxEvacuatedBytesForReduceMemory = 12 * MB;
  const int kTargetFragmentationPercentForOptimizeMemory = 20;
  const size_t kMaxEvacuatedBytesForOptimizeMemory = 6 * MB;

  // For regular mode (which is latency critical) we define less aggressive
  // defaults to start and switch to a trace-based (using compaction speed)
  // approach as soon as we have enough samples.
  const int kTargetFragmentationPercent = 70;
  const size_t kMaxEvacuatedBytes = 4 * MB;
  // Time to take for a single area (=payload of page). Used as soon as there
  // exist enough compaction speed samples.
  const float kTargetMsPerArea = .5;

  if (heap()->ShouldReduceMemory()) {
    *target_fragmentation_percent = kTargetFragmentationPercentForReduceMemory;
    *max_evacuated_bytes = kMaxEvacuatedBytesForReduceMemory;
  } else if (heap()->ShouldOptimizeForMemoryUsage()) {
    *target_fragmentation_percent =
        kTargetFragmentationPercentForOptimizeMemory;
    *max_evacuated_bytes = kMaxEvacuatedBytesForOptimizeMemory;
  } else {
    const double estimated_compaction_speed =
        heap()->tracer()->CompactionSpeedInBytesPerMillisecond();
    if (estimated_compaction_speed != 0) {
      // Estimate the target fragmentation based on traced compaction speed
      // and a goal for a single page.
      const double estimated_ms_per_area =
          1 + area_size / estimated_compaction_speed;
      *target_fragmentation_percent = static_cast<int>(
          100 - 100 * kTargetMsPerArea / estimated_ms_per_area);
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
  size_t area_size = space->AreaSize();

  // Pairs of (live_bytes_in_page, page).
  typedef std::pair<size_t, Page*> LiveBytesPagePair;
  std::vector<LiveBytesPagePair> pages;
  pages.reserve(number_of_pages);

  DCHECK(!sweeping_in_progress());
  Page* owner_of_linear_allocation_area =
      space->top() == space->limit()
          ? nullptr
          : Page::FromAllocationAreaAddress(space->top());
  for (Page* p : *space) {
    if (p->NeverEvacuate() || p == owner_of_linear_allocation_area) continue;
    // Invariant: Evacuation candidates are just created when marking is
    // started. This means that sweeping has finished. Furthermore, at the end
    // of a GC all evacuation candidates are cleared and their slot buffers are
    // released.
    CHECK(!p->IsEvacuationCandidate());
    CHECK_NULL(p->slot_set<OLD_TO_OLD>());
    CHECK_NULL(p->typed_slot_set<OLD_TO_OLD>());
    CHECK(p->SweepingDone());
    DCHECK(p->area_size() == area_size);
    pages.push_back(std::make_pair(p->LiveBytesFromFreeList(), p));
  }

  int candidate_count = 0;
  size_t total_live_bytes = 0;

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
    size_t max_evacuated_bytes;
    int target_fragmentation_percent;
    ComputeEvacuationHeuristics(area_size, &target_fragmentation_percent,
                                &max_evacuated_bytes);

    const size_t free_bytes_threshold =
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
      size_t live_bytes = pages[i].first;
      DCHECK_GE(area_size, live_bytes);
      size_t free_bytes = area_size - live_bytes;
      if (FLAG_always_compact ||
          ((free_bytes >= free_bytes_threshold) &&
           ((total_live_bytes + live_bytes) <= max_evacuated_bytes))) {
        candidate_count++;
        total_live_bytes += live_bytes;
      }
      if (FLAG_trace_fragmentation_verbose) {
        PrintIsolate(isolate(),
                     "compaction-selection-page: space=%s free_bytes_page=%zu "
                     "fragmentation_limit_kb=%" PRIuS
                     " fragmentation_limit_percent=%d sum_compaction_kb=%zu "
                     "compaction_limit_kb=%zu\n",
                     AllocationSpaceName(space->identity()), free_bytes / KB,
                     free_bytes_threshold / KB, target_fragmentation_percent,
                     total_live_bytes / KB, max_evacuated_bytes / KB);
      }
    }
    // How many pages we will allocated for the evacuated objects
    // in the worst case: ceil(total_live_bytes / area_size)
    int estimated_new_pages =
        static_cast<int>((total_live_bytes + area_size - 1) / area_size);
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
                 "total_live_bytes=%zu\n",
                 AllocationSpaceName(space->identity()), reduce_memory,
                 candidate_count, total_live_bytes / KB);
  }
}


void MarkCompactCollector::AbortCompaction() {
  if (compacting_) {
    RememberedSet<OLD_TO_OLD>::ClearAll(heap());
    for (Page* p : evacuation_candidates_) {
      p->ClearEvacuationCandidate();
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

  // Instead of waiting we could also abort the sweeper threads here.
  EnsureSweepingCompleted();

  if (heap()->incremental_marking()->IsSweeping()) {
    heap()->incremental_marking()->Stop();
  }

  // If concurrent unmapping tasks are still running, we should wait for
  // them here.
  heap()->memory_allocator()->unmapper()->WaitUntilCompleted();

  heap()->concurrent_marking()->EnsureTaskCompleted();

  // Clear marking bits if incremental marking is aborted.
  if (was_marked_incrementally_ && heap_->ShouldAbortIncrementalMarking()) {
    heap()->incremental_marking()->Stop();
    heap()->incremental_marking()->AbortBlackAllocation();
    ClearMarkbits();
    AbortWeakCollections();
    AbortWeakCells();
    AbortTransitionArrays();
    AbortCompaction();
    heap_->local_embedder_heap_tracer()->AbortTracing();
    marking_deque()->Clear();
    was_marked_incrementally_ = false;
  }

  if (!was_marked_incrementally_) {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WRAPPER_PROLOGUE);
    heap_->local_embedder_heap_tracer()->TracePrologue();
  }

  // Don't start compaction if we are in the middle of incremental
  // marking cycle. We did not collect any slots.
  if (!FLAG_never_compact && !was_marked_incrementally_) {
    StartCompaction();
  }

  PagedSpaces spaces(heap());
  for (PagedSpace* space = spaces.next(); space != NULL;
       space = spaces.next()) {
    space->PrepareForMarkCompact();
  }
  heap()->account_external_memory_concurrently_freed();

#ifdef VERIFY_HEAP
  if (!was_marked_incrementally_ && FLAG_verify_heap) {
    VerifyMarkbitsAreClean();
  }
#endif
}


void MarkCompactCollector::Finish() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_FINISH);

  if (!heap()->delay_sweeper_tasks_for_testing_) {
    sweeper().StartSweeperTasks();
  }

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

  // The stub caches are not traversed during GC; clear them to force
  // their lazy re-initialization. This must be done after the
  // GC, because it relies on the new address of certain old space
  // objects (empty string, illegal builtin).
  isolate()->load_stub_cache()->Clear();
  isolate()->store_stub_cache()->Clear();

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
  Code* interpreter_entry_trampoline =
      isolate_->builtins()->builtin(Builtins::kInterpreterEntryTrampoline);
  Object* undefined = isolate_->heap()->undefined_value();

  JSFunction* candidate = jsfunction_candidates_head_;
  JSFunction* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    ClearNextCandidate(candidate, undefined);

    SharedFunctionInfo* shared = candidate->shared();

    Code* code = shared->code();
    if (ObjectMarking::IsWhite(code, MarkingState::Internal(code))) {
      if (FLAG_trace_code_flushing && shared->is_compiled()) {
        PrintF("[code-flushing clears: ");
        shared->ShortPrint();
        PrintF(" - age: %d]\n", code->GetAge());
      }
      // Always flush the optimized code.
      if (candidate->has_feedback_vector()) {
        candidate->feedback_vector()->ClearOptimizedCode();
      }
      if (shared->HasBytecodeArray()) {
        shared->set_code(interpreter_entry_trampoline);
        candidate->set_code(interpreter_entry_trampoline);
      } else {
        shared->set_code(lazy_compile);
        candidate->set_code(lazy_compile);
      }
    } else {
      DCHECK(ObjectMarking::IsBlack(code, MarkingState::Internal(code)));
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
  Code* interpreter_entry_trampoline =
      isolate_->builtins()->builtin(Builtins::kInterpreterEntryTrampoline);
  SharedFunctionInfo* candidate = shared_function_info_candidates_head_;
  SharedFunctionInfo* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    ClearNextCandidate(candidate);

    Code* code = candidate->code();
    if (ObjectMarking::IsWhite(code, MarkingState::Internal(code))) {
      if (FLAG_trace_code_flushing && candidate->is_compiled()) {
        PrintF("[code-flushing clears: ");
        candidate->ShortPrint();
        PrintF(" - age: %d]\n", code->GetAge());
      }
      if (candidate->HasBytecodeArray()) {
        candidate->set_code(interpreter_entry_trampoline);
      } else {
        candidate->set_code(lazy_compile);
      }
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
  isolate_->heap()->incremental_marking()->IterateBlackObject(shared_info);

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
  DCHECK(!function->next_function_link()->IsUndefined(isolate_));
  Object* undefined = isolate_->heap()->undefined_value();

  // Make sure previous flushing decisions are revisited.
  isolate_->heap()->incremental_marking()->IterateBlackObject(function);
  isolate_->heap()->incremental_marking()->IterateBlackObject(
      function->shared());

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
    heap->mark_compact_collector()->MarkObject(object);
  }

  // Marks the object black without pushing it on the marking stack.
  // Returns true if object needed marking and false otherwise.
  INLINE(static bool MarkObjectWithoutPush(Heap* heap, HeapObject* object)) {
    return ObjectMarking::WhiteToBlack(object, MarkingState::Internal(object));
  }

  // Mark object pointed to by p.
  INLINE(static void MarkObjectByPointer(MarkCompactCollector* collector,
                                         HeapObject* object, Object** p)) {
    if (!(*p)->IsHeapObject()) return;
    HeapObject* target_object = HeapObject::cast(*p);
    collector->RecordSlot(object, p, target_object);
    collector->MarkObject(target_object);
  }


  // Visit an unmarked object.
  INLINE(static void VisitUnmarkedObject(MarkCompactCollector* collector,
                                         HeapObject* obj)) {
#ifdef DEBUG
    DCHECK(collector->heap()->Contains(obj));
#endif
    if (ObjectMarking::WhiteToBlack(obj, MarkingState::Internal(obj))) {
      Map* map = obj->map();
      Heap* heap = obj->GetHeap();
      ObjectMarking::WhiteToBlack(obj, MarkingState::Internal(obj));
      // Mark the map pointer and the body.
      heap->mark_compact_collector()->MarkObject(map);
      IterateBody(map, obj);
    }
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
      if (ObjectMarking::IsBlackOrGrey(data, MarkingState::Internal(data))) {
        Object** slot =
            data->data_start() + JSRegExp::saved_code_index(is_one_byte);
        heap->mark_compact_collector()->RecordSlot(data, slot, code);
      }

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
      JSObjectVisitor::Visit(map, object);
      return;
    }
    JSRegExp* re = reinterpret_cast<JSRegExp*>(object);
    // Flush code or set age on both one byte and two byte code.
    UpdateRegExpCodeAgeAndFlush(heap, re, true);
    UpdateRegExpCodeAgeAndFlush(heap, re, false);
    // Visit the fields of the RegExp, including the updated FixedArray.
    JSObjectVisitor::Visit(map, object);
  }
};


void MarkCompactMarkingVisitor::Initialize() {
  StaticMarkingVisitor<MarkCompactMarkingVisitor>::Initialize();

  table_.Register(kVisitJSRegExp, &VisitRegExpAndFlushCode);
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

class SharedFunctionInfoMarkingVisitor : public ObjectVisitor,
                                         public RootVisitor {
 public:
  explicit SharedFunctionInfoMarkingVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) MarkObject(p);
  }

  void VisitPointer(HeapObject* host, Object** slot) override {
    MarkObject(slot);
  }

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) MarkObject(p);
  }

  void VisitRootPointer(Root root, Object** slot) override { MarkObject(slot); }

 private:
  void MarkObject(Object** slot) {
    Object* obj = *slot;
    if (obj->IsSharedFunctionInfo()) {
      SharedFunctionInfo* shared = reinterpret_cast<SharedFunctionInfo*>(obj);
      collector_->MarkObject(shared->code());
      collector_->MarkObject(shared);
    }
  }
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
    MarkObject(code);
    if (frame->is_optimized()) {
      Code* optimized_code = frame->LookupCode();
      MarkObject(optimized_code);
    }
  }
}


void MarkCompactCollector::PrepareForCodeFlushing() {
  // If code flushing is disabled, there is no need to prepare for it.
  if (!is_code_flushing_enabled()) return;

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

void MinorMarkCompactCollector::CleanupSweepToIteratePages() {
  for (Page* p : sweep_to_iterate_pages_) {
    if (p->IsFlagSet(Page::SWEEP_TO_ITERATE)) {
      p->ClearFlag(Page::SWEEP_TO_ITERATE);
      marking_state(p).ClearLiveness();
    }
  }
  sweep_to_iterate_pages_.clear();
}

// Visitor class for marking heap roots.
// TODO(ulan): Remove ObjectVisitor base class after fixing marking of
// the string table and the top optimized code.
class MarkCompactCollector::RootMarkingVisitor : public ObjectVisitor,
                                                 public RootVisitor {
 public:
  explicit RootMarkingVisitor(Heap* heap)
      : collector_(heap->mark_compact_collector()) {}

  void VisitPointer(HeapObject* host, Object** p) override {
    MarkObjectByPointer(p);
  }

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

  void VisitRootPointer(Root root, Object** p) override {
    MarkObjectByPointer(p);
  }

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

  // Skip the weak next code link in a code object, which is visited in
  // ProcessTopOptimizedFrame.
  void VisitNextCodeLink(Code* host, Object** p) override {}

 private:
  void MarkObjectByPointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    HeapObject* object = HeapObject::cast(*p);

    if (ObjectMarking::WhiteToBlack<MarkBit::NON_ATOMIC>(
            object, MarkingState::Internal(object))) {
      Map* map = object->map();
      // Mark the map pointer and body, and push them on the marking stack.
      collector_->MarkObject(map);
      MarkCompactMarkingVisitor::IterateBody(map, object);
      // Mark all the objects reachable from the map and body.  May leave
      // overflowed objects in the heap.
      collector_->EmptyMarkingDeque();
    }
  }

  MarkCompactCollector* collector_;
};

class InternalizedStringTableCleaner : public ObjectVisitor {
 public:
  InternalizedStringTableCleaner(Heap* heap, HeapObject* table)
      : heap_(heap), pointers_removed_(0), table_(table) {}

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    // Visit all HeapObject pointers in [start, end).
    MarkCompactCollector* collector = heap_->mark_compact_collector();
    Object* the_hole = heap_->the_hole_value();
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (o->IsHeapObject()) {
        HeapObject* heap_object = HeapObject::cast(o);
        if (ObjectMarking::IsWhite(heap_object,
                                   MarkingState::Internal(heap_object))) {
          pointers_removed_++;
          // Set the entry to the_hole_value (as deleted).
          *p = the_hole;
        } else {
          // StringTable contains only old space strings.
          DCHECK(!heap_->InNewSpace(o));
          collector->RecordSlot(table_, p, o);
        }
      }
    }
  }

  int PointersRemoved() {
    return pointers_removed_;
  }

 private:
  Heap* heap_;
  int pointers_removed_;
  HeapObject* table_;
};

class ExternalStringTableCleaner : public RootVisitor {
 public:
  explicit ExternalStringTableCleaner(Heap* heap) : heap_(heap) {}

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    // Visit all HeapObject pointers in [start, end).
    Object* the_hole = heap_->the_hole_value();
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (o->IsHeapObject()) {
        HeapObject* heap_object = HeapObject::cast(o);
        if (ObjectMarking::IsWhite(heap_object,
                                   MarkingState::Internal(heap_object))) {
          if (o->IsExternalString()) {
            heap_->FinalizeExternalString(String::cast(*p));
          } else {
            // The original external string may have been internalized.
            DCHECK(o->IsThinString());
          }
          // Set the entry to the_hole_value (as deleted).
          *p = the_hole;
        }
      }
    }
  }

 private:
  Heap* heap_;
};

// Helper class for pruning the string table.
class YoungGenerationExternalStringTableCleaner : public RootVisitor {
 public:
  YoungGenerationExternalStringTableCleaner(
      const MinorMarkCompactCollector& collector)
      : heap_(collector.heap()), collector_(collector) {}

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    DCHECK_EQ(static_cast<int>(root),
              static_cast<int>(Root::kExternalStringsTable));
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (o->IsHeapObject()) {
        HeapObject* heap_object = HeapObject::cast(o);
        if (ObjectMarking::IsWhite(heap_object,
                                   collector_.marking_state(heap_object))) {
          if (o->IsExternalString()) {
            heap_->FinalizeExternalString(String::cast(*p));
          } else {
            // The original external string may have been internalized.
            DCHECK(o->IsThinString());
          }
          // Set the entry to the_hole_value (as deleted).
          *p = heap_->the_hole_value();
        }
      }
    }
  }

 private:
  Heap* heap_;
  const MinorMarkCompactCollector& collector_;
};

// Marked young generation objects and all old generation objects will be
// retained.
class MinorMarkCompactWeakObjectRetainer : public WeakObjectRetainer {
 public:
  explicit MinorMarkCompactWeakObjectRetainer(
      const MinorMarkCompactCollector& collector)
      : collector_(collector) {}

  virtual Object* RetainAs(Object* object) {
    HeapObject* heap_object = HeapObject::cast(object);
    if (!collector_.heap()->InNewSpace(heap_object)) return object;

    DCHECK(!ObjectMarking::IsGrey(heap_object,
                                  collector_.marking_state(heap_object)));
    if (ObjectMarking::IsBlack(heap_object,
                               collector_.marking_state(heap_object))) {
      return object;
    }
    return nullptr;
  }

 private:
  const MinorMarkCompactCollector& collector_;
};

// Implementation of WeakObjectRetainer for mark compact GCs. All marked objects
// are retained.
class MarkCompactWeakObjectRetainer : public WeakObjectRetainer {
 public:
  virtual Object* RetainAs(Object* object) {
    HeapObject* heap_object = HeapObject::cast(object);
    DCHECK(!ObjectMarking::IsGrey(heap_object,
                                  MarkingState::Internal(heap_object)));
    if (ObjectMarking::IsBlack(heap_object,
                               MarkingState::Internal(heap_object))) {
      return object;
    } else if (object->IsAllocationSite() &&
               !(AllocationSite::cast(object)->IsZombie())) {
      // "dead" AllocationSites need to live long enough for a traversal of new
      // space. These sites get a one-time reprieve.
      AllocationSite* site = AllocationSite::cast(object);
      site->MarkZombie();
      ObjectMarking::WhiteToBlack(site, MarkingState::Internal(site));
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
    if ((object->map() != filler_map) &&
        ObjectMarking::GreyToBlack(object, MarkingState::Internal(object))) {
      PushBlack(object);
      if (marking_deque()->IsFull()) return;
    }
  }
}

void MarkCompactCollector::DiscoverGreyObjectsOnPage(MemoryChunk* p) {
  DCHECK(!marking_deque()->IsFull());
  LiveObjectIterator<kGreyObjects> it(p, MarkingState::Internal(p));
  HeapObject* object = NULL;
  while ((object = it.Next()) != NULL) {
    bool success =
        ObjectMarking::GreyToBlack(object, MarkingState::Internal(object));
    DCHECK(success);
    USE(success);
    PushBlack(object);
    if (marking_deque()->IsFull()) return;
  }
}

class RecordMigratedSlotVisitor : public ObjectVisitor {
 public:
  explicit RecordMigratedSlotVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  inline void VisitPointer(HeapObject* host, Object** p) final {
    RecordMigratedSlot(host, *p, reinterpret_cast<Address>(p));
  }

  inline void VisitPointers(HeapObject* host, Object** start,
                            Object** end) final {
    while (start < end) {
      RecordMigratedSlot(host, *start, reinterpret_cast<Address>(start));
      ++start;
    }
  }

  inline void VisitCodeEntry(JSFunction* host,
                             Address code_entry_slot) override {
    Address code_entry = Memory::Address_at(code_entry_slot);
    if (Page::FromAddress(code_entry)->IsEvacuationCandidate()) {
      RememberedSet<OLD_TO_OLD>::InsertTyped(Page::FromAddress(code_entry_slot),
                                             nullptr, CODE_ENTRY_SLOT,
                                             code_entry_slot);
    }
  }

  inline void VisitCodeTarget(Code* host, RelocInfo* rinfo) override {
    DCHECK_EQ(host, rinfo->host());
    DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    // The target is always in old space, we don't have to record the slot in
    // the old-to-new remembered set.
    DCHECK(!collector_->heap()->InNewSpace(target));
    collector_->RecordRelocSlot(host, rinfo, target);
  }

  inline void VisitDebugTarget(Code* host, RelocInfo* rinfo) override {
    DCHECK_EQ(host, rinfo->host());
    DCHECK(RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
           rinfo->IsPatchedDebugBreakSlotSequence());
    Code* target = Code::GetCodeFromTargetAddress(rinfo->debug_call_address());
    // The target is always in old space, we don't have to record the slot in
    // the old-to-new remembered set.
    DCHECK(!collector_->heap()->InNewSpace(target));
    collector_->RecordRelocSlot(host, rinfo, target);
  }

  inline void VisitEmbeddedPointer(Code* host, RelocInfo* rinfo) override {
    DCHECK_EQ(host, rinfo->host());
    DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
    HeapObject* object = HeapObject::cast(rinfo->target_object());
    collector_->heap()->RecordWriteIntoCode(host, rinfo, object);
    collector_->RecordRelocSlot(host, rinfo, object);
  }

  inline void VisitCellPointer(Code* host, RelocInfo* rinfo) override {
    DCHECK_EQ(host, rinfo->host());
    DCHECK(rinfo->rmode() == RelocInfo::CELL);
    Cell* cell = rinfo->target_cell();
    // The cell is always in old space, we don't have to record the slot in
    // the old-to-new remembered set.
    DCHECK(!collector_->heap()->InNewSpace(cell));
    collector_->RecordRelocSlot(host, rinfo, cell);
  }

  inline void VisitCodeAgeSequence(Code* host, RelocInfo* rinfo) override {
    DCHECK_EQ(host, rinfo->host());
    DCHECK(RelocInfo::IsCodeAgeSequence(rinfo->rmode()));
    Code* stub = rinfo->code_age_stub();
    USE(stub);
    DCHECK(!Page::FromAddress(stub->address())->IsEvacuationCandidate());
  }

  // Entries that are skipped for recording.
  inline void VisitExternalReference(Code* host, RelocInfo* rinfo) final {}
  inline void VisitExternalReference(Foreign* host, Address* p) final {}
  inline void VisitRuntimeEntry(Code* host, RelocInfo* rinfo) final {}
  inline void VisitInternalReference(Code* host, RelocInfo* rinfo) final {}

 protected:
  inline virtual void RecordMigratedSlot(HeapObject* host, Object* value,
                                         Address slot) {
    if (value->IsHeapObject()) {
      Page* p = Page::FromAddress(reinterpret_cast<Address>(value));
      if (p->InNewSpace()) {
        RememberedSet<OLD_TO_NEW>::Insert(Page::FromAddress(slot), slot);
      } else if (p->IsEvacuationCandidate()) {
        RememberedSet<OLD_TO_OLD>::Insert(Page::FromAddress(slot), slot);
      }
    }
  }

  MarkCompactCollector* collector_;
};

class MigrationObserver {
 public:
  explicit MigrationObserver(Heap* heap) : heap_(heap) {}

  virtual ~MigrationObserver() {}
  virtual void Move(AllocationSpace dest, HeapObject* src, HeapObject* dst,
                    int size) = 0;

 protected:
  Heap* heap_;
};

class ProfilingMigrationObserver final : public MigrationObserver {
 public:
  explicit ProfilingMigrationObserver(Heap* heap) : MigrationObserver(heap) {}

  inline void Move(AllocationSpace dest, HeapObject* src, HeapObject* dst,
                   int size) final {
    if (dest == CODE_SPACE || (dest == OLD_SPACE && dst->IsBytecodeArray())) {
      PROFILE(heap_->isolate(),
              CodeMoveEvent(AbstractCode::cast(src), dst->address()));
    }
    heap_->OnMoveEvent(dst, src, size);
  }
};

class YoungGenerationMigrationObserver final : public MigrationObserver {
 public:
  YoungGenerationMigrationObserver(Heap* heap,
                                   MarkCompactCollector* mark_compact_collector)
      : MigrationObserver(heap),
        mark_compact_collector_(mark_compact_collector) {}

  inline void Move(AllocationSpace dest, HeapObject* src, HeapObject* dst,
                   int size) final {
    // Migrate color to old generation marking in case the object survived young
    // generation garbage collection.
    if (heap_->incremental_marking()->IsMarking()) {
      DCHECK(ObjectMarking::IsWhite(
          dst, mark_compact_collector_->marking_state(dst)));
      heap_->incremental_marking()->TransferColor<MarkBit::ATOMIC>(src, dst);
    }
  }

 protected:
  base::Mutex mutex_;
  MarkCompactCollector* mark_compact_collector_;
};

class YoungGenerationRecordMigratedSlotVisitor final
    : public RecordMigratedSlotVisitor {
 public:
  explicit YoungGenerationRecordMigratedSlotVisitor(
      MarkCompactCollector* collector)
      : RecordMigratedSlotVisitor(collector) {}

  inline void VisitCodeEntry(JSFunction* host, Address code_entry_slot) final {
    Address code_entry = Memory::Address_at(code_entry_slot);
    if (Page::FromAddress(code_entry)->IsEvacuationCandidate() &&
        IsLive(host)) {
      RememberedSet<OLD_TO_OLD>::InsertTyped(Page::FromAddress(code_entry_slot),
                                             nullptr, CODE_ENTRY_SLOT,
                                             code_entry_slot);
    }
  }

  void VisitCodeTarget(Code* host, RelocInfo* rinfo) final { UNREACHABLE(); }
  void VisitDebugTarget(Code* host, RelocInfo* rinfo) final { UNREACHABLE(); }
  void VisitEmbeddedPointer(Code* host, RelocInfo* rinfo) final {
    UNREACHABLE();
  }
  void VisitCellPointer(Code* host, RelocInfo* rinfo) final { UNREACHABLE(); }
  void VisitCodeAgeSequence(Code* host, RelocInfo* rinfo) final {
    UNREACHABLE();
  }

 private:
  // Only record slots for host objects that are considered as live by the full
  // collector.
  inline bool IsLive(HeapObject* object) {
    return ObjectMarking::IsBlack(object, collector_->marking_state(object));
  }

  inline void RecordMigratedSlot(HeapObject* host, Object* value,
                                 Address slot) final {
    if (value->IsHeapObject()) {
      Page* p = Page::FromAddress(reinterpret_cast<Address>(value));
      if (p->InNewSpace()) {
        RememberedSet<OLD_TO_NEW>::Insert(Page::FromAddress(slot), slot);
      } else if (p->IsEvacuationCandidate() && IsLive(host)) {
        RememberedSet<OLD_TO_OLD>::Insert(Page::FromAddress(slot), slot);
      }
    }
  }
};

class HeapObjectVisitor {
 public:
  virtual ~HeapObjectVisitor() {}
  virtual bool Visit(HeapObject* object) = 0;
};

class EvacuateVisitorBase : public HeapObjectVisitor {
 public:
  void AddObserver(MigrationObserver* observer) {
    migration_function_ = RawMigrateObject<MigrationMode::kObserved>;
    observers_.push_back(observer);
  }

 protected:
  enum MigrationMode { kFast, kObserved };

  typedef void (*MigrateFunction)(EvacuateVisitorBase* base, HeapObject* dst,
                                  HeapObject* src, int size,
                                  AllocationSpace dest);

  template <MigrationMode mode>
  static void RawMigrateObject(EvacuateVisitorBase* base, HeapObject* dst,
                               HeapObject* src, int size,
                               AllocationSpace dest) {
    Address dst_addr = dst->address();
    Address src_addr = src->address();
    DCHECK(base->heap_->AllowedToBeMigrated(src, dest));
    DCHECK(dest != LO_SPACE);
    if (dest == OLD_SPACE) {
      DCHECK_OBJECT_SIZE(size);
      DCHECK(IsAligned(size, kPointerSize));
      base->heap_->CopyBlock(dst_addr, src_addr, size);
      if (mode != MigrationMode::kFast)
        base->ExecuteMigrationObservers(dest, src, dst, size);
      dst->IterateBodyFast(dst->map()->instance_type(), size,
                           base->record_visitor_);
    } else if (dest == CODE_SPACE) {
      DCHECK_CODEOBJECT_SIZE(size, base->heap_->code_space());
      base->heap_->CopyBlock(dst_addr, src_addr, size);
      Code::cast(dst)->Relocate(dst_addr - src_addr);
      if (mode != MigrationMode::kFast)
        base->ExecuteMigrationObservers(dest, src, dst, size);
      dst->IterateBodyFast(dst->map()->instance_type(), size,
                           base->record_visitor_);
    } else {
      DCHECK_OBJECT_SIZE(size);
      DCHECK(dest == NEW_SPACE);
      base->heap_->CopyBlock(dst_addr, src_addr, size);
      if (mode != MigrationMode::kFast)
        base->ExecuteMigrationObservers(dest, src, dst, size);
    }
    base::NoBarrier_Store(reinterpret_cast<base::AtomicWord*>(src_addr),
                          reinterpret_cast<base::AtomicWord>(dst_addr));
  }

  EvacuateVisitorBase(Heap* heap, CompactionSpaceCollection* compaction_spaces,
                      RecordMigratedSlotVisitor* record_visitor)
      : heap_(heap),
        compaction_spaces_(compaction_spaces),
        record_visitor_(record_visitor) {
    migration_function_ = RawMigrateObject<MigrationMode::kFast>;
  }

  inline bool TryEvacuateObject(PagedSpace* target_space, HeapObject* object,
                                HeapObject** target_object) {
#ifdef VERIFY_HEAP
    if (AbortCompactionForTesting(object)) return false;
#endif  // VERIFY_HEAP
    int size = object->Size();
    AllocationAlignment alignment = object->RequiredAlignment();
    AllocationResult allocation = target_space->AllocateRaw(size, alignment);
    if (allocation.To(target_object)) {
      MigrateObject(*target_object, object, size, target_space->identity());
      return true;
    }
    return false;
  }

  inline void ExecuteMigrationObservers(AllocationSpace dest, HeapObject* src,
                                        HeapObject* dst, int size) {
    for (MigrationObserver* obs : observers_) {
      obs->Move(dest, src, dst, size);
    }
  }

  inline void MigrateObject(HeapObject* dst, HeapObject* src, int size,
                            AllocationSpace dest) {
    migration_function_(this, dst, src, size, dest);
  }

#ifdef VERIFY_HEAP
  bool AbortCompactionForTesting(HeapObject* object) {
    if (FLAG_stress_compaction) {
      const uintptr_t mask = static_cast<uintptr_t>(FLAG_random_seed) &
                             Page::kPageAlignmentMask & ~kPointerAlignmentMask;
      if ((reinterpret_cast<uintptr_t>(object->address()) &
           Page::kPageAlignmentMask) == mask) {
        Page* page = Page::FromAddress(object->address());
        if (page->IsFlagSet(Page::COMPACTION_WAS_ABORTED_FOR_TESTING)) {
          page->ClearFlag(Page::COMPACTION_WAS_ABORTED_FOR_TESTING);
        } else {
          page->SetFlag(Page::COMPACTION_WAS_ABORTED_FOR_TESTING);
          return true;
        }
      }
    }
    return false;
  }
#endif  // VERIFY_HEAP

  Heap* heap_;
  CompactionSpaceCollection* compaction_spaces_;
  RecordMigratedSlotVisitor* record_visitor_;
  std::vector<MigrationObserver*> observers_;
  MigrateFunction migration_function_;
};

class EvacuateNewSpaceVisitor final : public EvacuateVisitorBase {
 public:
  static const intptr_t kLabSize = 4 * KB;
  static const intptr_t kMaxLabObjectSize = 256;

  explicit EvacuateNewSpaceVisitor(Heap* heap,
                                   CompactionSpaceCollection* compaction_spaces,
                                   RecordMigratedSlotVisitor* record_visitor,
                                   base::HashMap* local_pretenuring_feedback)
      : EvacuateVisitorBase(heap, compaction_spaces, record_visitor),
        buffer_(LocalAllocationBuffer::InvalidBuffer()),
        space_to_allocate_(NEW_SPACE),
        promoted_size_(0),
        semispace_copied_size_(0),
        local_pretenuring_feedback_(local_pretenuring_feedback) {}

  inline bool Visit(HeapObject* object) override {
    heap_->UpdateAllocationSite<Heap::kCached>(object,
                                               local_pretenuring_feedback_);
    int size = object->Size();
    HeapObject* target_object = nullptr;
    if (heap_->ShouldBePromoted(object->address(), size) &&
        TryEvacuateObject(compaction_spaces_->Get(OLD_SPACE), object,
                          &target_object)) {
      promoted_size_ += size;
      return true;
    }
    HeapObject* target = nullptr;
    AllocationSpace space = AllocateTargetObject(object, &target);
    MigrateObject(HeapObject::cast(target), object, size, space);
    semispace_copied_size_ += size;
    return true;
  }

  intptr_t promoted_size() { return promoted_size_; }
  intptr_t semispace_copied_size() { return semispace_copied_size_; }
  AllocationInfo CloseLAB() { return buffer_.Close(); }

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
    AllocationSpace space_allocated_in = space_to_allocate_;
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
      space_allocated_in = OLD_SPACE;
    }
    bool ok = allocation.To(target_object);
    DCHECK(ok);
    USE(ok);
    return space_allocated_in;
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
  base::HashMap* local_pretenuring_feedback_;
};

template <PageEvacuationMode mode>
class EvacuateNewSpacePageVisitor final : public HeapObjectVisitor {
 public:
  explicit EvacuateNewSpacePageVisitor(
      Heap* heap, RecordMigratedSlotVisitor* record_visitor,
      base::HashMap* local_pretenuring_feedback)
      : heap_(heap),
        record_visitor_(record_visitor),
        moved_bytes_(0),
        local_pretenuring_feedback_(local_pretenuring_feedback) {}

  static void Move(Page* page) {
    switch (mode) {
      case NEW_TO_NEW:
        page->heap()->new_space()->MovePageFromSpaceToSpace(page);
        page->SetFlag(Page::PAGE_NEW_NEW_PROMOTION);
        break;
      case NEW_TO_OLD: {
        page->Unlink();
        Page* new_page = Page::ConvertNewToOld(page);
        new_page->SetFlag(Page::PAGE_NEW_OLD_PROMOTION);
        break;
      }
    }
  }

  inline bool Visit(HeapObject* object) {
    heap_->UpdateAllocationSite<Heap::kCached>(object,
                                               local_pretenuring_feedback_);
    if (mode == NEW_TO_OLD) {
      object->IterateBodyFast(record_visitor_);
    }
    return true;
  }

  intptr_t moved_bytes() { return moved_bytes_; }
  void account_moved_bytes(intptr_t bytes) { moved_bytes_ += bytes; }

 private:
  Heap* heap_;
  RecordMigratedSlotVisitor* record_visitor_;
  intptr_t moved_bytes_;
  base::HashMap* local_pretenuring_feedback_;
};

class EvacuateOldSpaceVisitor final : public EvacuateVisitorBase {
 public:
  EvacuateOldSpaceVisitor(Heap* heap,
                          CompactionSpaceCollection* compaction_spaces,
                          RecordMigratedSlotVisitor* record_visitor)
      : EvacuateVisitorBase(heap, compaction_spaces, record_visitor) {}

  inline bool Visit(HeapObject* object) override {
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

class EvacuateRecordOnlyVisitor final : public HeapObjectVisitor {
 public:
  explicit EvacuateRecordOnlyVisitor(Heap* heap) : heap_(heap) {}

  inline bool Visit(HeapObject* object) {
    RecordMigratedSlotVisitor visitor(heap_->mark_compact_collector());
    object->IterateBody(&visitor);
    return true;
  }

 private:
  Heap* heap_;
};

void MarkCompactCollector::DiscoverGreyObjectsInSpace(PagedSpace* space) {
  for (Page* p : *space) {
    DiscoverGreyObjectsOnPage(p);
    if (marking_deque()->IsFull()) return;
  }
}


void MarkCompactCollector::DiscoverGreyObjectsInNewSpace() {
  NewSpace* space = heap()->new_space();
  for (Page* page : PageRange(space->bottom(), space->top())) {
    DiscoverGreyObjectsOnPage(page);
    if (marking_deque()->IsFull()) return;
  }
}


bool MarkCompactCollector::IsUnmarkedHeapObject(Object** p) {
  Object* o = *p;
  if (!o->IsHeapObject()) return false;
  return ObjectMarking::IsWhite(HeapObject::cast(o),
                                MarkingState::Internal(HeapObject::cast(o)));
}

void MarkCompactCollector::MarkStringTable(RootMarkingVisitor* visitor) {
  StringTable* string_table = heap()->string_table();
  // Mark the string table itself.
  if (ObjectMarking::WhiteToBlack(string_table,
                                  MarkingState::Internal(string_table))) {
    // Explicitly mark the prefix.
    string_table->IteratePrefix(visitor);
    ProcessMarkingDeque();
  }
}

void MarkCompactCollector::MarkRoots(RootMarkingVisitor* visitor) {
  // Mark the heap roots including global variables, stack variables,
  // etc., and all objects reachable from them.
  heap()->IterateStrongRoots(visitor, VISIT_ONLY_STRONG);

  // Handle the string table specially.
  MarkStringTable(visitor);

  // There may be overflowed objects in the heap.  Visit them now.
  while (marking_deque()->overflowed()) {
    RefillMarkingDeque();
    EmptyMarkingDeque();
  }
}

// Mark all objects reachable from the objects on the marking stack.
// Before: the marking stack contains zero or more heap object pointers.
// After: the marking stack is empty, and all objects reachable from the
// marking stack have been marked, or are overflowed in the heap.
void MarkCompactCollector::EmptyMarkingDeque() {
  while (!marking_deque()->IsEmpty()) {
    HeapObject* object = marking_deque()->Pop();

    DCHECK(!object->IsFiller());
    DCHECK(object->IsHeapObject());
    DCHECK(heap()->Contains(object));
    DCHECK(!(ObjectMarking::IsWhite<MarkBit::NON_ATOMIC>(
        object, MarkingState::Internal(object))));

    Map* map = object->map();
    MarkObject(map);
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
  DCHECK(marking_deque()->overflowed());

  DiscoverGreyObjectsInNewSpace();
  if (marking_deque()->IsFull()) return;

  DiscoverGreyObjectsInSpace(heap()->old_space());
  if (marking_deque()->IsFull()) return;
  DiscoverGreyObjectsInSpace(heap()->code_space());
  if (marking_deque()->IsFull()) return;
  DiscoverGreyObjectsInSpace(heap()->map_space());
  if (marking_deque()->IsFull()) return;
  LargeObjectIterator lo_it(heap()->lo_space());
  DiscoverGreyObjectsWithIterator(&lo_it);
  if (marking_deque()->IsFull()) return;

  marking_deque()->ClearOverflowed();
}

// Mark all objects reachable (transitively) from objects on the marking
// stack.  Before: the marking stack contains zero or more heap object
// pointers.  After: the marking stack is empty and there are no overflowed
// objects in the heap.
void MarkCompactCollector::ProcessMarkingDeque() {
  EmptyMarkingDeque();
  while (marking_deque()->overflowed()) {
    RefillMarkingDeque();
    EmptyMarkingDeque();
  }
  DCHECK(marking_deque()->IsEmpty());
}

// Mark all objects reachable (transitively) from objects on the marking
// stack including references only considered in the atomic marking pause.
void MarkCompactCollector::ProcessEphemeralMarking(
    bool only_process_harmony_weak_collections) {
  DCHECK(marking_deque()->IsEmpty() && !marking_deque()->overflowed());
  bool work_to_do = true;
  while (work_to_do) {
    if (!only_process_harmony_weak_collections) {
      if (heap_->local_embedder_heap_tracer()->InUse()) {
        TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WRAPPER_TRACING);
        heap_->local_embedder_heap_tracer()->RegisterWrappersWithRemoteTracer();
        heap_->local_embedder_heap_tracer()->Trace(
            0,
            EmbedderHeapTracer::AdvanceTracingActions(
                EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
      }
    } else {
      // TODO(mlippautz): We currently do not trace through blink when
      // discovering new objects reachable from weak roots (that have been made
      // strong). This is a limitation of not having a separate handle type
      // that doesn't require zapping before this phase. See crbug.com/668060.
      heap_->local_embedder_heap_tracer()->ClearCachedWrappersToTrace();
    }
    ProcessWeakCollections();
    work_to_do = !marking_deque()->IsEmpty();
    ProcessMarkingDeque();
  }
  CHECK(marking_deque()->IsEmpty());
  CHECK_EQ(0, heap()->local_embedder_heap_tracer()->NumberOfWrappersToTrace());
}

void MarkCompactCollector::ProcessTopOptimizedFrame(
    RootMarkingVisitor* visitor) {
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

class ObjectStatsVisitor : public HeapObjectVisitor {
 public:
  ObjectStatsVisitor(Heap* heap, ObjectStats* live_stats,
                     ObjectStats* dead_stats)
      : live_collector_(heap, live_stats), dead_collector_(heap, dead_stats) {
    DCHECK_NOT_NULL(live_stats);
    DCHECK_NOT_NULL(dead_stats);
    // Global objects are roots and thus recorded as live.
    live_collector_.CollectGlobalStatistics();
  }

  bool Visit(HeapObject* obj) override {
    if (ObjectMarking::IsBlack(obj, MarkingState::Internal(obj))) {
      live_collector_.CollectStatistics(obj);
    } else {
      DCHECK(!ObjectMarking::IsGrey(obj, MarkingState::Internal(obj)));
      dead_collector_.CollectStatistics(obj);
    }
    return true;
  }

 private:
  ObjectStatsCollector live_collector_;
  ObjectStatsCollector dead_collector_;
};

void MarkCompactCollector::VisitAllObjects(HeapObjectVisitor* visitor) {
  SpaceIterator space_it(heap());
  HeapObject* obj = nullptr;
  while (space_it.has_next()) {
    std::unique_ptr<ObjectIterator> it(space_it.next()->GetObjectIterator());
    ObjectIterator* obj_it = it.get();
    while ((obj = obj_it->Next()) != nullptr) {
      visitor->Visit(obj);
    }
  }
}

void MarkCompactCollector::RecordObjectStats() {
  if (V8_UNLIKELY(FLAG_gc_stats)) {
    heap()->CreateObjectStats();
    ObjectStatsVisitor visitor(heap(), heap()->live_object_stats_,
                               heap()->dead_object_stats_);
    VisitAllObjects(&visitor);
    if (V8_UNLIKELY(FLAG_gc_stats &
                    v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING)) {
      std::stringstream live, dead;
      heap()->live_object_stats_->Dump(live);
      heap()->dead_object_stats_->Dump(dead);
      TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("v8.gc_stats"),
                           "V8.GC_Objects_Stats", TRACE_EVENT_SCOPE_THREAD,
                           "live", TRACE_STR_COPY(live.str().c_str()), "dead",
                           TRACE_STR_COPY(dead.str().c_str()));
    }
    if (FLAG_trace_gc_object_stats) {
      heap()->live_object_stats_->PrintJSON("live");
      heap()->dead_object_stats_->PrintJSON("dead");
    }
    heap()->live_object_stats_->CheckpointObjectStats();
    heap()->dead_object_stats_->ClearObjectStats();
  }
}

class YoungGenerationMarkingVisitor final
    : public HeapVisitor<void, YoungGenerationMarkingVisitor> {
 public:
  using BaseClass = HeapVisitor<int, YoungGenerationMarkingVisitor>;

  YoungGenerationMarkingVisitor(Heap* heap,
                                WorkStealingMarkingDeque* global_marking_deque,
                                int task_id)
      : heap_(heap), marking_deque_(global_marking_deque, task_id) {}

  void VisitPointers(HeapObject* host, Object** start, Object** end) final {
    const int kMinRangeForMarkingRecursion = 64;
    if (end - start >= kMinRangeForMarkingRecursion) {
      if (MarkRecursively(host, start, end)) return;
    }
    for (Object** p = start; p < end; p++) {
      VisitPointer(host, p);
    }
  }

  void VisitPointer(HeapObject* host, Object** slot) final {
    Object* target = *slot;
    if (heap_->InNewSpace(target)) {
      HeapObject* target_object = HeapObject::cast(target);
      MarkObjectViaMarkingDeque(target_object);
    }
  }

  // Special cases for young generation. Also see StaticNewSpaceVisitor.

  void VisitJSFunction(Map* map, JSFunction* object) final {
    if (!ShouldVisit(object)) return;
    int size = JSFunction::BodyDescriptorWeakCode::SizeOf(map, object);
    VisitMapPointer(object, object->map_slot());
    JSFunction::BodyDescriptorWeakCode::IterateBody(object, size, this);
    return;
  }

  void VisitNativeContext(Map* map, Context* object) final {
    if (!ShouldVisit(object)) return;
    int size = Context::ScavengeBodyDescriptor::SizeOf(map, object);
    VisitMapPointer(object, object->map_slot());
    Context::ScavengeBodyDescriptor::IterateBody(object, size, this);
    return;
  }

  void VisitJSApiObject(Map* map, JSObject* object) final {
    return VisitJSObject(map, object);
  }

  void VisitBytecodeArray(Map* map, BytecodeArray* object) final {
    UNREACHABLE();
    return;
  }

  void VisitSharedFunctionInfo(Map* map, SharedFunctionInfo* object) final {
    UNREACHABLE();
    return;
  }

 private:
  inline MarkingState marking_state(HeapObject* object) {
    SLOW_DCHECK(
        MarkingState::External(object).bitmap() ==
        heap_->minor_mark_compact_collector()->marking_state(object).bitmap());
    return MarkingState::External(object);
  }

  inline void MarkObjectViaMarkingDeque(HeapObject* object) {
    if (ObjectMarking::WhiteToBlack<MarkBit::ATOMIC>(object,
                                                     marking_state(object))) {
      // Marking deque overflow is unsupported for the young generation.
      CHECK(marking_deque_.Push(object));
    }
  }

  inline bool MarkRecursively(HeapObject* host, Object** start, Object** end) {
    // TODO(mlippautz): Stack check on background tasks. We cannot do a reliable
    // stack check on background tasks yet.
    for (Object** p = start; p < end; p++) {
      Object* target = *p;
      if (heap_->InNewSpace(target)) {
        HeapObject* target_object = HeapObject::cast(target);
        if (ObjectMarking::WhiteToBlack<MarkBit::ATOMIC>(
                target_object, marking_state(target_object))) {
          Visit(target_object);
        }
      }
    }
    return true;
  }

  Heap* heap_;
  LocalWorkStealingMarkingDeque marking_deque_;
};

class MinorMarkCompactCollector::RootMarkingVisitor : public RootVisitor {
 public:
  explicit RootMarkingVisitor(MinorMarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitRootPointer(Root root, Object** p) override {
    MarkObjectByPointer(p);
  }

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

 private:
  inline MarkingState marking_state(HeapObject* object) {
    SLOW_DCHECK(MarkingState::External(object).bitmap() ==
                collector_->marking_state(object).bitmap());
    return MarkingState::External(object);
  }

  void MarkObjectByPointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    HeapObject* object = HeapObject::cast(*p);

    if (!collector_->heap()->InNewSpace(object)) return;

    if (ObjectMarking::WhiteToBlack<MarkBit::NON_ATOMIC>(
            object, marking_state(object))) {
      collector_->marking_visitor(kMainMarker)->Visit(object);
      collector_->EmptyMarkingDeque();
    }
  }

  MinorMarkCompactCollector* collector_;
};

class MarkingItem;
class PageMarkingItem;
class RootMarkingItem;
class YoungGenerationMarkingTask;

class MarkingItem : public ItemParallelJob::Item {
 public:
  virtual ~MarkingItem() {}
  virtual void Process(YoungGenerationMarkingTask* task) = 0;
};

class YoungGenerationMarkingTask : public ItemParallelJob::Task {
 public:
  YoungGenerationMarkingTask(Isolate* isolate,
                             MinorMarkCompactCollector* collector,
                             WorkStealingMarkingDeque* marking_deque,
                             YoungGenerationMarkingVisitor* visitor,
                             int task_id)
      : ItemParallelJob::Task(isolate),
        collector_(collector),
        marking_deque_(marking_deque, task_id),
        visitor_(visitor) {}

  void RunInParallel() override {
    double marking_time = 0.0;
    {
      TimedScope scope(&marking_time);
      MarkingItem* item = nullptr;
      while ((item = GetItem<MarkingItem>()) != nullptr) {
        item->Process(this);
        item->MarkFinished();
        EmptyLocalMarkingDeque();
      }
      EmptyMarkingDeque();
      DCHECK(marking_deque_.IsEmpty());
    }
    if (FLAG_trace_minor_mc_parallel_marking) {
      PrintIsolate(collector_->isolate(), "marking[%p]: time=%f\n",
                   static_cast<void*>(this), marking_time);
    }
  };

  void MarkObject(Object* object) {
    if (!collector_->heap()->InNewSpace(object)) return;
    HeapObject* heap_object = HeapObject::cast(object);
    if (ObjectMarking::WhiteToBlack<MarkBit::ATOMIC>(
            heap_object, collector_->marking_state(heap_object))) {
      visitor_->Visit(heap_object);
    }
  }

 private:
  void EmptyLocalMarkingDeque() {
    HeapObject* object = nullptr;
    while (marking_deque_.Pop(&object)) {
      visitor_->Visit(object);
    }
  }

  void EmptyMarkingDeque() {
    HeapObject* object = nullptr;
    while (marking_deque_.WaitForMoreObjects()) {
      while (marking_deque_.Pop(&object)) {
        visitor_->Visit(object);
      }
    }
  }

  MinorMarkCompactCollector* collector_;
  LocalWorkStealingMarkingDeque marking_deque_;
  YoungGenerationMarkingVisitor* visitor_;
};

class BatchedRootMarkingItem : public MarkingItem {
 public:
  explicit BatchedRootMarkingItem(std::vector<Object*>&& objects)
      : objects_(objects) {}
  virtual ~BatchedRootMarkingItem() {}

  void Process(YoungGenerationMarkingTask* task) override {
    for (Object* object : objects_) {
      task->MarkObject(object);
    }
  }

 private:
  std::vector<Object*> objects_;
};

class PageMarkingItem : public MarkingItem {
 public:
  explicit PageMarkingItem(MemoryChunk* chunk) : chunk_(chunk) {}
  virtual ~PageMarkingItem() {}

  void Process(YoungGenerationMarkingTask* task) override {
    base::LockGuard<base::RecursiveMutex> guard(chunk_->mutex());
    MarkUntypedPointers(task);
    MarkTypedPointers(task);
  }

 private:
  inline Heap* heap() { return chunk_->heap(); }

  void MarkUntypedPointers(YoungGenerationMarkingTask* task) {
    RememberedSet<OLD_TO_NEW>::Iterate(chunk_, [this, task](Address slot) {
      return CheckAndMarkObject(task, slot);
    });
  }

  void MarkTypedPointers(YoungGenerationMarkingTask* task) {
    Isolate* isolate = heap()->isolate();
    RememberedSet<OLD_TO_NEW>::IterateTyped(
        chunk_, [this, isolate, task](SlotType slot_type, Address host_addr,
                                      Address slot) {
          return UpdateTypedSlotHelper::UpdateTypedSlot(
              isolate, slot_type, slot, [this, task](Object** slot) {
                return CheckAndMarkObject(task,
                                          reinterpret_cast<Address>(slot));
              });
        });
  }

  SlotCallbackResult CheckAndMarkObject(YoungGenerationMarkingTask* task,
                                        Address slot_address) {
    Object* object = *reinterpret_cast<Object**>(slot_address);
    if (heap()->InNewSpace(object)) {
      // Marking happens before flipping the young generation, so the object
      // has to be in ToSpace.
      DCHECK(heap()->InToSpace(object));
      HeapObject* heap_object = reinterpret_cast<HeapObject*>(object);
      task->MarkObject(heap_object);
      return KEEP_SLOT;
    }
    return REMOVE_SLOT;
  }

  MemoryChunk* chunk_;
};

// This root visitor walks all roots and creates items bundling objects that
// are then processed later on. Slots have to be dereferenced as they could
// live on the native (C++) stack, which requires filtering out the indirection.
class MinorMarkCompactCollector::RootMarkingVisitorSeedOnly
    : public RootVisitor {
 public:
  explicit RootMarkingVisitorSeedOnly(ItemParallelJob* job) : job_(job) {
    buffered_objects_.reserve(kBufferSize);
  }

  void VisitRootPointer(Root root, Object** p) override {
    if (!(*p)->IsHeapObject()) return;
    AddObject(*p);
  }

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) {
      if (!(*p)->IsHeapObject()) continue;
      AddObject(*p);
    }
  }

  void FlushObjects() {
    job_->AddItem(new BatchedRootMarkingItem(std::move(buffered_objects_)));
    // Moving leaves the container in a valid but unspecified state. Reusing the
    // container requires a call without precondition that resets the state.
    buffered_objects_.clear();
    buffered_objects_.reserve(kBufferSize);
  }

 private:
  // Bundling several objects together in items avoids issues with allocating
  // and deallocating items; both are operations that are performed on the main
  // thread.
  static const int kBufferSize = 32;

  void AddObject(Object* object) {
    buffered_objects_.push_back(object);
    if (buffered_objects_.size() == kBufferSize) FlushObjects();
  }

  ItemParallelJob* job_;
  std::vector<Object*> buffered_objects_;
};

MinorMarkCompactCollector::MinorMarkCompactCollector(Heap* heap)
    : MarkCompactCollectorBase(heap), page_parallel_job_semaphore_(0) {
  marking_deque_ = new WorkStealingMarkingDeque();
  for (int i = 0; i < kNumMarkers; i++) {
    marking_visitor_[i] =
        new YoungGenerationMarkingVisitor(heap, marking_deque_, i);
  }
}

MinorMarkCompactCollector::~MinorMarkCompactCollector() {
  for (int i = 0; i < kNumMarkers; i++) {
    DCHECK_NOT_NULL(marking_visitor_[i]);
    delete marking_visitor_[i];
  }
  delete marking_deque_;
}

SlotCallbackResult MinorMarkCompactCollector::CheckAndMarkObject(
    Heap* heap, Address slot_address) {
  Object* object = *reinterpret_cast<Object**>(slot_address);
  if (heap->InNewSpace(object)) {
    // Marking happens before flipping the young generation, so the object
    // has to be in ToSpace.
    DCHECK(heap->InToSpace(object));
    HeapObject* heap_object = reinterpret_cast<HeapObject*>(object);
    const MarkingState state = MarkingState::External(heap_object);
    if (ObjectMarking::WhiteToBlack<MarkBit::NON_ATOMIC>(heap_object, state)) {
      heap->minor_mark_compact_collector()
          ->marking_visitor(kMainMarker)
          ->Visit(heap_object);
    }
    return KEEP_SLOT;
  }
  return REMOVE_SLOT;
}

static bool IsUnmarkedObjectForYoungGeneration(Heap* heap, Object** p) {
  DCHECK_IMPLIES(heap->InNewSpace(*p), heap->InToSpace(*p));
  return heap->InNewSpace(*p) &&
         !ObjectMarking::IsBlack(HeapObject::cast(*p),
                                 MarkingState::External(HeapObject::cast(*p)));
}

void MinorMarkCompactCollector::MarkRootSetInParallel() {
  // Seed the root set (roots + old->new set).
  ItemParallelJob job(isolate()->cancelable_task_manager(),
                      &page_parallel_job_semaphore_);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_SEED);
    RootMarkingVisitorSeedOnly root_seed_visitor(&job);
    heap()->IterateRoots(&root_seed_visitor, VISIT_ALL_IN_SCAVENGE);
    RememberedSet<OLD_TO_NEW>::IterateMemoryChunks(
        heap(), [&job](MemoryChunk* chunk) {
          job.AddItem(new PageMarkingItem(chunk));
        });
    root_seed_visitor.FlushObjects();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_ROOTS);
    const int num_tasks = NumberOfMarkingTasks();
    for (int i = 0; i < num_tasks; i++) {
      job.AddTask(new YoungGenerationMarkingTask(
          isolate(), this, marking_deque(), marking_visitor(i), i));
    }
    job.Run();
  }
}

void MinorMarkCompactCollector::MarkLiveObjects() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK);

  PostponeInterruptsScope postpone(isolate());

  RootMarkingVisitor root_visitor(this);

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MINOR_MC_MARK_IDENTIFY_GLOBAL_HANDLES);
    isolate()->global_handles()->IdentifyWeakUnmodifiedObjects(
        &Heap::IsUnmodifiedHeapObject);
  }

  MarkRootSetInParallel();

  // Mark rest on the main thread.
  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_WEAK);
    heap()->IterateEncounteredWeakCollections(&root_visitor);
    ProcessMarkingDeque();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_GLOBAL_HANDLES);
    isolate()->global_handles()->MarkNewSpaceWeakUnmodifiedObjectsPending(
        &IsUnmarkedObjectForYoungGeneration);
    isolate()->global_handles()->IterateNewSpaceWeakUnmodifiedRoots(
        &root_visitor);
    ProcessMarkingDeque();
  }
}

void MinorMarkCompactCollector::ProcessMarkingDeque() {
  EmptyMarkingDeque();
}

void MinorMarkCompactCollector::EmptyMarkingDeque() {
  LocalWorkStealingMarkingDeque local_marking_deque(marking_deque(),
                                                    kMainMarker);
  HeapObject* object = nullptr;
  while (local_marking_deque.Pop(&object)) {
    DCHECK(!object->IsFiller());
    DCHECK(object->IsHeapObject());
    DCHECK(heap()->Contains(object));
    DCHECK(!(ObjectMarking::IsWhite<MarkBit::NON_ATOMIC>(
        object, marking_state(object))));
    DCHECK((ObjectMarking::IsBlack<MarkBit::NON_ATOMIC>(
        object, marking_state(object))));
    marking_visitor(kMainMarker)->Visit(object);
  }
  DCHECK(local_marking_deque.IsEmpty());
}

void MinorMarkCompactCollector::CollectGarbage() {
  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_SWEEPING);
    heap()->mark_compact_collector()->sweeper().EnsureNewSpaceCompleted();
    CleanupSweepToIteratePages();
  }

  MarkLiveObjects();
  ClearNonLiveReferences();
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    YoungGenerationMarkingVerifier verifier(heap());
    verifier.Run();
  }
#endif  // VERIFY_HEAP

  Evacuate();
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    YoungGenerationEvacuationVerifier verifier(heap());
    verifier.Run();
  }
#endif  // VERIFY_HEAP

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARKING_DEQUE);
    heap()->incremental_marking()->UpdateMarkingDequeAfterScavenge();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_RESET_LIVENESS);
    for (Page* p : PageRange(heap()->new_space()->FromSpaceStart(),
                             heap()->new_space()->FromSpaceEnd())) {
      DCHECK(!p->IsFlagSet(Page::SWEEP_TO_ITERATE));
      marking_state(p).ClearLiveness();
    }
  }

  heap()->account_external_memory_concurrently_freed();
}

void MinorMarkCompactCollector::MakeIterable(
    Page* p, MarkingTreatmentMode marking_mode,
    FreeSpaceTreatmentMode free_space_mode) {
  // We have to clear the full collectors markbits for the areas that we
  // remove here.
  MarkCompactCollector* full_collector = heap()->mark_compact_collector();
  Address free_start = p->area_start();
  DCHECK(reinterpret_cast<intptr_t>(free_start) % (32 * kPointerSize) == 0);
  LiveObjectIterator<kBlackObjects> it(p, marking_state(p));
  HeapObject* object = nullptr;

  while ((object = it.Next()) != nullptr) {
    DCHECK(ObjectMarking::IsBlack(object, marking_state(object)));
    Address free_end = object->address();
    if (free_end != free_start) {
      CHECK_GT(free_end, free_start);
      size_t size = static_cast<size_t>(free_end - free_start);
      if (free_space_mode == ZAP_FREE_SPACE) {
        memset(free_start, 0xcc, size);
        full_collector->marking_state(p).bitmap()->ClearRange(
            p->AddressToMarkbitIndex(free_start),
            p->AddressToMarkbitIndex(free_end));
      }
      p->heap()->CreateFillerObjectAt(free_start, static_cast<int>(size),
                                      ClearRecordedSlots::kNo);
    }
    Map* map = object->synchronized_map();
    int size = object->SizeFromMap(map);
    free_start = free_end + size;
  }

  if (free_start != p->area_end()) {
    CHECK_GT(p->area_end(), free_start);
    size_t size = static_cast<size_t>(p->area_end() - free_start);
    if (free_space_mode == ZAP_FREE_SPACE) {
      memset(free_start, 0xcc, size);
      full_collector->marking_state(p).bitmap()->ClearRange(
          p->AddressToMarkbitIndex(free_start),
          p->AddressToMarkbitIndex(p->area_end()));
    }
    p->heap()->CreateFillerObjectAt(free_start, static_cast<int>(size),
                                    ClearRecordedSlots::kNo);
  }

  if (marking_mode == MarkingTreatmentMode::CLEAR) {
    marking_state(p).ClearLiveness();
    p->ClearFlag(Page::SWEEP_TO_ITERATE);
  }
}

void MinorMarkCompactCollector::ClearNonLiveReferences() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_CLEAR);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_CLEAR_STRING_TABLE);
    // Internalized strings are always stored in old space, so there is no need
    // to clean them here.
    YoungGenerationExternalStringTableCleaner external_visitor(*this);
    heap()->external_string_table_.IterateNewSpaceStrings(&external_visitor);
    heap()->external_string_table_.CleanUpNewSpaceStrings();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_CLEAR_WEAK_LISTS);
    // Process the weak references.
    MinorMarkCompactWeakObjectRetainer retainer(*this);
    heap()->ProcessYoungWeakReferences(&retainer);
  }
}

void MinorMarkCompactCollector::EvacuatePrologue() {
  NewSpace* new_space = heap()->new_space();
  // Append the list of new space pages to be processed.
  for (Page* p : PageRange(new_space->bottom(), new_space->top())) {
    new_space_evacuation_pages_.Add(p);
  }
  new_space->Flip();
  new_space->ResetAllocationInfo();
}

void MinorMarkCompactCollector::EvacuateEpilogue() {
  heap()->new_space()->set_age_mark(heap()->new_space()->top());
}

void MinorMarkCompactCollector::Evacuate() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_EVACUATE);
  base::LockGuard<base::Mutex> guard(heap()->relocation_mutex());

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_EVACUATE_PROLOGUE);
    EvacuatePrologue();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_EVACUATE_COPY);
    EvacuatePagesInParallel();
  }

  UpdatePointersAfterEvacuation();

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_EVACUATE_REBALANCE);
    if (!heap()->new_space()->Rebalance()) {
      FatalProcessOutOfMemory("NewSpace::Rebalance");
    }
  }

  // Give pages that are queued to be freed back to the OS.
  heap()->memory_allocator()->unmapper()->FreeQueuedChunks();

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_EVACUATE_CLEAN_UP);
    for (Page* p : new_space_evacuation_pages_) {
      if (p->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION) ||
          p->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION)) {
        p->ClearFlag(Page::PAGE_NEW_NEW_PROMOTION);
        p->ClearFlag(Page::PAGE_NEW_OLD_PROMOTION);
        p->SetFlag(Page::SWEEP_TO_ITERATE);
        sweep_to_iterate_pages_.push_back(p);
      }
    }
    new_space_evacuation_pages_.Rewind(0);
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_EVACUATE_EPILOGUE);
    EvacuateEpilogue();
  }
}

void MarkCompactCollector::MarkLiveObjects() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK);
  // The recursive GC marker detects when it is nearing stack overflow,
  // and switches to a different marking system.  JS interrupts interfere
  // with the C stack limit check.
  PostponeInterruptsScope postpone(isolate());

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_FINISH_INCREMENTAL);
    IncrementalMarking* incremental_marking = heap_->incremental_marking();
    if (was_marked_incrementally_) {
      incremental_marking->Finalize();
    } else {
      CHECK(incremental_marking->IsStopped());
    }
  }

#ifdef DEBUG
  DCHECK(state_ == PREPARE_GC);
  state_ = MARK_LIVE_OBJECTS;
#endif

  marking_deque()->StartUsing();

  heap_->local_embedder_heap_tracer()->EnterFinalPause();

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_PREPARE_CODE_FLUSH);
    PrepareForCodeFlushing();
  }

  RootMarkingVisitor root_visitor(heap());

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_ROOTS);
    MarkRoots(&root_visitor);
    ProcessTopOptimizedFrame(&root_visitor);
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WEAK_CLOSURE);

    // The objects reachable from the roots are marked, yet unreachable
    // objects are unmarked.  Mark objects reachable due to host
    // application specific logic or through Harmony weak maps.
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_EPHEMERAL);
      ProcessEphemeralMarking(false);
    }

    // The objects reachable from the roots, weak maps or object groups
    // are marked. Objects pointed to only by weak global handles cannot be
    // immediately reclaimed. Instead, we have to mark them as pending and mark
    // objects reachable from them.
    //
    // First we identify nonlive weak handles and mark them as pending
    // destruction.
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_WEAK_HANDLES);
      heap()->isolate()->global_handles()->IdentifyWeakHandles(
          &IsUnmarkedHeapObject);
      ProcessMarkingDeque();
    }
    // Then we mark the objects.

    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_WEAK_ROOTS);
      heap()->isolate()->global_handles()->IterateWeakRoots(&root_visitor);
      ProcessMarkingDeque();
    }

    // Repeat Harmony weak maps marking to mark unmarked objects reachable from
    // the weak roots we just marked as pending destruction.
    //
    // We only process harmony collections, as all object groups have been fully
    // processed and no weakly reachable node can discover new objects groups.
    {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WEAK_CLOSURE_HARMONY);
      ProcessEphemeralMarking(true);
      {
        TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WRAPPER_EPILOGUE);
        heap()->local_embedder_heap_tracer()->TraceEpilogue();
      }
    }
  }
}


void MarkCompactCollector::ClearNonLiveReferences() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_STRING_TABLE);

    // Prune the string table removing all strings only pointed to by the
    // string table.  Cannot use string_table() here because the string
    // table is marked.
    StringTable* string_table = heap()->string_table();
    InternalizedStringTableCleaner internalized_visitor(heap(), string_table);
    string_table->IterateElements(&internalized_visitor);
    string_table->ElementsRemoved(internalized_visitor.PointersRemoved());

    ExternalStringTableCleaner external_visitor(heap());
    heap()->external_string_table_.IterateAll(&external_visitor);
    heap()->external_string_table_.CleanUpAll();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_LISTS);
    // Process the weak references.
    MarkCompactWeakObjectRetainer mark_compact_object_retainer;
    heap()->ProcessAllWeakReferences(&mark_compact_object_retainer);
  }

  // Flush code from collected candidates.
  if (is_code_flushing_enabled()) {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_CODE_FLUSH);
    code_flusher_->ProcessCandidates();
  }


  DependentCode* dependent_code_list;
  Object* non_live_map_list;
  ClearWeakCells(&non_live_map_list, &dependent_code_list);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_MAPS);
    ClearSimpleMapTransitions(non_live_map_list);
    ClearFullMapTransitions();
  }

  MarkDependentCodeForDeoptimization(dependent_code_list);

  ClearWeakCollections();
}


void MarkCompactCollector::MarkDependentCodeForDeoptimization(
    DependentCode* list_head) {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_DEPENDENT_CODE);
  Isolate* isolate = this->isolate();
  DependentCode* current = list_head;
  while (current->length() > 0) {
    have_code_to_deoptimize_ |= current->MarkCodeForDeoptimization(
        isolate, DependentCode::kWeakCodeGroup);
    current = current->next_link();
  }

  {
    ArrayList* list = heap_->weak_new_space_object_to_code_list();
    int counter = 0;
    for (int i = 0; i < list->Length(); i += 2) {
      WeakCell* obj = WeakCell::cast(list->Get(i));
      WeakCell* dep = WeakCell::cast(list->Get(i + 1));
      if (obj->cleared() || dep->cleared()) {
        if (!dep->cleared()) {
          Code* code = Code::cast(dep->value());
          if (!code->marked_for_deoptimization()) {
            DependentCode::SetMarkedForDeoptimization(
                code, DependentCode::DependencyGroup::kWeakCodeGroup);
            code->InvalidateEmbeddedObjects();
            have_code_to_deoptimize_ = true;
          }
        }
      } else {
        // We record the slot manually because marking is finished at this
        // point and the write barrier would bailout.
        list->Set(counter, obj, SKIP_WRITE_BARRIER);
        RecordSlot(list, list->Slot(counter), obj);
        counter++;
        list->Set(counter, dep, SKIP_WRITE_BARRIER);
        RecordSlot(list, list->Slot(counter), dep);
        counter++;
      }
    }
  }

  WeakHashTable* table = heap_->weak_object_to_code_table();
  uint32_t capacity = table->Capacity();
  for (uint32_t i = 0; i < capacity; i++) {
    uint32_t key_index = table->EntryToIndex(i);
    Object* key = table->get(key_index);
    if (!table->IsKey(isolate, key)) continue;
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
  while (weak_cell_obj != Smi::kZero) {
    WeakCell* weak_cell = WeakCell::cast(weak_cell_obj);
    Map* map = Map::cast(weak_cell->value());
    DCHECK(ObjectMarking::IsWhite(map, MarkingState::Internal(map)));
    Object* potential_parent = map->constructor_or_backpointer();
    if (potential_parent->IsMap()) {
      Map* parent = Map::cast(potential_parent);
      if (ObjectMarking::IsBlackOrGrey(parent,
                                       MarkingState::Internal(parent)) &&
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
  map->set_raw_transitions(Smi::kZero);
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
  while (obj != Smi::kZero) {
    TransitionArray* array = TransitionArray::cast(obj);
    int num_transitions = array->number_of_entries();
    DCHECK_EQ(TransitionArray::NumberOfTransitions(array), num_transitions);
    if (num_transitions > 0) {
      Map* map = array->GetTarget(0);
      Map* parent = Map::cast(map->constructor_or_backpointer());
      bool parent_is_alive =
          ObjectMarking::IsBlackOrGrey(parent, MarkingState::Internal(parent));
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
  heap()->set_encountered_transition_arrays(Smi::kZero);
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
    if (ObjectMarking::IsWhite(target, MarkingState::Internal(target))) {
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
    heap_->RightTrimFixedArray(transitions,
                               trim * TransitionArray::kTransitionSize);
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
    heap_->RightTrimFixedArray(descriptors,
                               to_trim * DescriptorArray::kEntrySize);
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
  heap_->RightTrimFixedArray(descriptors->GetEnumCache(), to_trim);

  if (!descriptors->HasEnumIndicesCache()) return;
  FixedArray* enum_indices_cache = descriptors->GetEnumIndicesCache();
  heap_->RightTrimFixedArray(enum_indices_cache, to_trim);
}


void MarkCompactCollector::ProcessWeakCollections() {
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::kZero) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    DCHECK(ObjectMarking::IsBlackOrGrey(
        weak_collection, MarkingState::Internal(weak_collection)));
    if (weak_collection->table()->IsHashTable()) {
      ObjectHashTable* table = ObjectHashTable::cast(weak_collection->table());
      for (int i = 0; i < table->Capacity(); i++) {
        HeapObject* heap_object = HeapObject::cast(table->KeyAt(i));
        if (ObjectMarking::IsBlackOrGrey(heap_object,
                                         MarkingState::Internal(heap_object))) {
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
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_COLLECTIONS);
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::kZero) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    DCHECK(ObjectMarking::IsBlackOrGrey(
        weak_collection, MarkingState::Internal(weak_collection)));
    if (weak_collection->table()->IsHashTable()) {
      ObjectHashTable* table = ObjectHashTable::cast(weak_collection->table());
      for (int i = 0; i < table->Capacity(); i++) {
        HeapObject* key = HeapObject::cast(table->KeyAt(i));
        if (!ObjectMarking::IsBlackOrGrey(key, MarkingState::Internal(key))) {
          table->RemoveEntry(i);
        }
      }
    }
    weak_collection_obj = weak_collection->next();
    weak_collection->set_next(heap()->undefined_value());
  }
  heap()->set_encountered_weak_collections(Smi::kZero);
}


void MarkCompactCollector::AbortWeakCollections() {
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::kZero) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    weak_collection_obj = weak_collection->next();
    weak_collection->set_next(heap()->undefined_value());
  }
  heap()->set_encountered_weak_collections(Smi::kZero);
}


void MarkCompactCollector::ClearWeakCells(Object** non_live_map_list,
                                          DependentCode** dependent_code_list) {
  Heap* heap = this->heap();
  TRACE_GC(heap->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_CELLS);
  Object* weak_cell_obj = heap->encountered_weak_cells();
  Object* the_hole_value = heap->the_hole_value();
  DependentCode* dependent_code_head =
      DependentCode::cast(heap->empty_fixed_array());
  Object* non_live_map_head = Smi::kZero;
  while (weak_cell_obj != Smi::kZero) {
    WeakCell* weak_cell = reinterpret_cast<WeakCell*>(weak_cell_obj);
    Object* next_weak_cell = weak_cell->next();
    bool clear_value = true;
    bool clear_next = true;
    // We do not insert cleared weak cells into the list, so the value
    // cannot be a Smi here.
    HeapObject* value = HeapObject::cast(weak_cell->value());
    if (!ObjectMarking::IsBlackOrGrey(value, MarkingState::Internal(value))) {
      // Cells for new-space objects embedded in optimized code are wrapped in
      // WeakCell and put into Heap::weak_object_to_code_table.
      // Such cells do not have any strong references but we want to keep them
      // alive as long as the cell value is alive.
      // TODO(ulan): remove this once we remove Heap::weak_object_to_code_table.
      if (value->IsCell()) {
        Object* cell_value = Cell::cast(value)->value();
        if (cell_value->IsHeapObject() &&
            ObjectMarking::IsBlackOrGrey(
                HeapObject::cast(cell_value),
                MarkingState::Internal(HeapObject::cast(cell_value)))) {
          // Resurrect the cell.
          ObjectMarking::WhiteToBlack(value, MarkingState::Internal(value));
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
  heap->set_encountered_weak_cells(Smi::kZero);
  *non_live_map_list = non_live_map_head;
  *dependent_code_list = dependent_code_head;
}


void MarkCompactCollector::AbortWeakCells() {
  Object* the_hole_value = heap()->the_hole_value();
  Object* weak_cell_obj = heap()->encountered_weak_cells();
  while (weak_cell_obj != Smi::kZero) {
    WeakCell* weak_cell = reinterpret_cast<WeakCell*>(weak_cell_obj);
    weak_cell_obj = weak_cell->next();
    weak_cell->clear_next(the_hole_value);
  }
  heap()->set_encountered_weak_cells(Smi::kZero);
}


void MarkCompactCollector::AbortTransitionArrays() {
  HeapObject* undefined = heap()->undefined_value();
  Object* obj = heap()->encountered_transition_arrays();
  while (obj != Smi::kZero) {
    TransitionArray* array = TransitionArray::cast(obj);
    obj = array->next_link();
    array->set_next_link(undefined, SKIP_WRITE_BARRIER);
  }
  heap()->set_encountered_transition_arrays(Smi::kZero);
}

void MarkCompactCollector::RecordRelocSlot(Code* host, RelocInfo* rinfo,
                                           Object* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  Page* source_page = Page::FromAddress(reinterpret_cast<Address>(host));
  if (target_page->IsEvacuationCandidate() &&
      (rinfo->host() == NULL ||
       !ShouldSkipEvacuationSlotRecording(rinfo->host()))) {
    RelocInfo::Mode rmode = rinfo->rmode();
    Address addr = rinfo->pc();
    SlotType slot_type = SlotTypeForRelocInfoMode(rmode);
    if (rinfo->IsInConstantPool()) {
      addr = rinfo->constant_pool_entry_address();
      if (RelocInfo::IsCodeTarget(rmode)) {
        slot_type = CODE_ENTRY_SLOT;
      } else {
        DCHECK(RelocInfo::IsEmbeddedObject(rmode));
        slot_type = OBJECT_SLOT;
      }
    }
    RememberedSet<OLD_TO_OLD>::InsertTyped(
        source_page, reinterpret_cast<Address>(host), slot_type, addr);
  }
}

static inline SlotCallbackResult UpdateSlot(Object** slot) {
  Object* obj = reinterpret_cast<Object*>(
      base::NoBarrier_Load(reinterpret_cast<base::AtomicWord*>(slot)));

  if (obj->IsHeapObject()) {
    HeapObject* heap_obj = HeapObject::cast(obj);
    MapWord map_word = heap_obj->map_word();
    if (map_word.IsForwardingAddress()) {
      DCHECK(heap_obj->GetHeap()->InFromSpace(heap_obj) ||
             MarkCompactCollector::IsOnEvacuationCandidate(heap_obj) ||
             Page::FromAddress(heap_obj->address())
                 ->IsFlagSet(Page::COMPACTION_WAS_ABORTED));
      HeapObject* target = map_word.ToForwardingAddress();
      base::NoBarrier_CompareAndSwap(
          reinterpret_cast<base::AtomicWord*>(slot),
          reinterpret_cast<base::AtomicWord>(obj),
          reinterpret_cast<base::AtomicWord>(target));
      DCHECK(!heap_obj->GetHeap()->InFromSpace(target));
      DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(target));
    }
  }
  return REMOVE_SLOT;
}

// Visitor for updating root pointers and to-space pointers.
// It does not expect to encounter pointers to dead objects.
// TODO(ulan): Remove code object specific functions. This visitor
// nevers visits code objects.
class PointersUpdatingVisitor : public ObjectVisitor, public RootVisitor {
 public:
  void VisitPointer(HeapObject* host, Object** p) override { UpdateSlot(p); }

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) UpdateSlot(p);
  }

  void VisitRootPointer(Root root, Object** p) override { UpdateSlot(p); }

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) UpdateSlot(p);
  }

  void VisitCellPointer(Code* host, RelocInfo* rinfo) override {
    UpdateTypedSlotHelper::UpdateCell(rinfo, UpdateSlot);
  }

  void VisitEmbeddedPointer(Code* host, RelocInfo* rinfo) override {
    UpdateTypedSlotHelper::UpdateEmbeddedPointer(rinfo, UpdateSlot);
  }

  void VisitCodeTarget(Code* host, RelocInfo* rinfo) override {
    UpdateTypedSlotHelper::UpdateCodeTarget(rinfo, UpdateSlot);
  }

  void VisitCodeEntry(JSFunction* host, Address entry_address) override {
    UpdateTypedSlotHelper::UpdateCodeEntry(entry_address, UpdateSlot);
  }

  void VisitDebugTarget(Code* host, RelocInfo* rinfo) override {
    UpdateTypedSlotHelper::UpdateDebugTarget(rinfo, UpdateSlot);
  }
};

static String* UpdateReferenceInExternalStringTableEntry(Heap* heap,
                                                         Object** p) {
  MapWord map_word = HeapObject::cast(*p)->map_word();

  if (map_word.IsForwardingAddress()) {
    return String::cast(map_word.ToForwardingAddress());
  }

  return String::cast(*p);
}

void MarkCompactCollector::EvacuatePrologue() {
  // New space.
  NewSpace* new_space = heap()->new_space();
  // Append the list of new space pages to be processed.
  for (Page* p : PageRange(new_space->bottom(), new_space->top())) {
    new_space_evacuation_pages_.Add(p);
  }
  new_space->Flip();
  new_space->ResetAllocationInfo();

  // Old space.
  DCHECK(old_space_evacuation_pages_.is_empty());
  old_space_evacuation_pages_.Swap(&evacuation_candidates_);
  DCHECK(evacuation_candidates_.is_empty());
}

void MarkCompactCollector::EvacuateEpilogue() {
  // New space.
  heap()->new_space()->set_age_mark(heap()->new_space()->top());
  // Old space. Deallocate evacuated candidate pages.
  ReleaseEvacuationCandidates();
}

class Evacuator : public Malloced {
 public:
  enum EvacuationMode {
    kObjectsNewToOld,
    kPageNewToOld,
    kObjectsOldToOld,
    kPageNewToNew,
  };

  static inline EvacuationMode ComputeEvacuationMode(MemoryChunk* chunk) {
    // Note: The order of checks is important in this function.
    if (chunk->IsFlagSet(MemoryChunk::PAGE_NEW_OLD_PROMOTION))
      return kPageNewToOld;
    if (chunk->IsFlagSet(MemoryChunk::PAGE_NEW_NEW_PROMOTION))
      return kPageNewToNew;
    if (chunk->InNewSpace()) return kObjectsNewToOld;
    DCHECK(chunk->IsEvacuationCandidate());
    return kObjectsOldToOld;
  }

  // NewSpacePages with more live bytes than this threshold qualify for fast
  // evacuation.
  static int PageEvacuationThreshold() {
    if (FLAG_page_promotion)
      return FLAG_page_promotion_threshold * Page::kAllocatableMemory / 100;
    return Page::kAllocatableMemory + kPointerSize;
  }

  Evacuator(Heap* heap, RecordMigratedSlotVisitor* record_visitor)
      : heap_(heap),
        compaction_spaces_(heap_),
        local_pretenuring_feedback_(kInitialLocalPretenuringFeedbackCapacity),
        new_space_visitor_(heap_, &compaction_spaces_, record_visitor,
                           &local_pretenuring_feedback_),
        new_to_new_page_visitor_(heap_, record_visitor,
                                 &local_pretenuring_feedback_),
        new_to_old_page_visitor_(heap_, record_visitor,
                                 &local_pretenuring_feedback_),

        old_space_visitor_(heap_, &compaction_spaces_, record_visitor),
        duration_(0.0),
        bytes_compacted_(0) {}

  virtual ~Evacuator() {}

  bool EvacuatePage(Page* page);

  void AddObserver(MigrationObserver* observer) {
    new_space_visitor_.AddObserver(observer);
    old_space_visitor_.AddObserver(observer);
  }

  // Merge back locally cached info sequentially. Note that this method needs
  // to be called from the main thread.
  inline void Finalize();

  CompactionSpaceCollection* compaction_spaces() { return &compaction_spaces_; }
  AllocationInfo CloseNewSpaceLAB() { return new_space_visitor_.CloseLAB(); }

 protected:
  static const int kInitialLocalPretenuringFeedbackCapacity = 256;

  // |saved_live_bytes| returns the live bytes of the page that was processed.
  virtual bool RawEvacuatePage(Page* page, intptr_t* saved_live_bytes) = 0;

  inline Heap* heap() { return heap_; }

  void ReportCompactionProgress(double duration, intptr_t bytes_compacted) {
    duration_ += duration;
    bytes_compacted_ += bytes_compacted;
  }

  Heap* heap_;

  // Locally cached collector data.
  CompactionSpaceCollection compaction_spaces_;
  base::HashMap local_pretenuring_feedback_;

  // Visitors for the corresponding spaces.
  EvacuateNewSpaceVisitor new_space_visitor_;
  EvacuateNewSpacePageVisitor<PageEvacuationMode::NEW_TO_NEW>
      new_to_new_page_visitor_;
  EvacuateNewSpacePageVisitor<PageEvacuationMode::NEW_TO_OLD>
      new_to_old_page_visitor_;
  EvacuateOldSpaceVisitor old_space_visitor_;

  // Book keeping info.
  double duration_;
  intptr_t bytes_compacted_;
};

bool Evacuator::EvacuatePage(Page* page) {
  bool success = false;
  DCHECK(page->SweepingDone());
  intptr_t saved_live_bytes = 0;
  double evacuation_time = 0.0;
  {
    AlwaysAllocateScope always_allocate(heap()->isolate());
    TimedScope timed_scope(&evacuation_time);
    success = RawEvacuatePage(page, &saved_live_bytes);
  }
  ReportCompactionProgress(evacuation_time, saved_live_bytes);
  if (FLAG_trace_evacuation) {
    PrintIsolate(heap()->isolate(),
                 "evacuation[%p]: page=%p new_space=%d "
                 "page_evacuation=%d executable=%d contains_age_mark=%d "
                 "live_bytes=%" V8PRIdPTR " time=%f success=%d\n",
                 static_cast<void*>(this), static_cast<void*>(page),
                 page->InNewSpace(),
                 page->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION) ||
                     page->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION),
                 page->IsFlagSet(MemoryChunk::IS_EXECUTABLE),
                 page->Contains(heap()->new_space()->age_mark()),
                 saved_live_bytes, evacuation_time, success);
  }
  return success;
}

void Evacuator::Finalize() {
  heap()->old_space()->MergeCompactionSpace(compaction_spaces_.Get(OLD_SPACE));
  heap()->code_space()->MergeCompactionSpace(
      compaction_spaces_.Get(CODE_SPACE));
  heap()->tracer()->AddCompactionEvent(duration_, bytes_compacted_);
  heap()->IncrementPromotedObjectsSize(new_space_visitor_.promoted_size() +
                                       new_to_old_page_visitor_.moved_bytes());
  heap()->IncrementSemiSpaceCopiedObjectSize(
      new_space_visitor_.semispace_copied_size() +
      new_to_new_page_visitor_.moved_bytes());
  heap()->IncrementYoungSurvivorsCounter(
      new_space_visitor_.promoted_size() +
      new_space_visitor_.semispace_copied_size() +
      new_to_old_page_visitor_.moved_bytes() +
      new_to_new_page_visitor_.moved_bytes());
  heap()->MergeAllocationSitePretenuringFeedback(local_pretenuring_feedback_);
}

class FullEvacuator : public Evacuator {
 public:
  FullEvacuator(MarkCompactCollector* collector,
                RecordMigratedSlotVisitor* record_visitor)
      : Evacuator(collector->heap(), record_visitor), collector_(collector) {}

 protected:
  bool RawEvacuatePage(Page* page, intptr_t* live_bytes) override;

  MarkCompactCollector* collector_;
};

bool FullEvacuator::RawEvacuatePage(Page* page, intptr_t* live_bytes) {
  bool success = false;
  LiveObjectVisitor object_visitor;
  const MarkingState state = collector_->marking_state(page);
  *live_bytes = state.live_bytes();
  switch (ComputeEvacuationMode(page)) {
    case kObjectsNewToOld:
      success = object_visitor.VisitBlackObjects(
          page, state, &new_space_visitor_, LiveObjectVisitor::kClearMarkbits);
      DCHECK(success);
      ArrayBufferTracker::ProcessBuffers(
          page, ArrayBufferTracker::kUpdateForwardedRemoveOthers);
      break;
    case kPageNewToOld:
      success = object_visitor.VisitBlackObjects(
          page, state, &new_to_old_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      DCHECK(success);
      new_to_old_page_visitor_.account_moved_bytes(state.live_bytes());
      // ArrayBufferTracker will be updated during sweeping.
      break;
    case kPageNewToNew:
      success = object_visitor.VisitBlackObjects(
          page, state, &new_to_new_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      DCHECK(success);
      new_to_new_page_visitor_.account_moved_bytes(state.live_bytes());
      // ArrayBufferTracker will be updated during sweeping.
      break;
    case kObjectsOldToOld:
      success = object_visitor.VisitBlackObjects(
          page, state, &old_space_visitor_, LiveObjectVisitor::kClearMarkbits);
      if (!success) {
        // Aborted compaction page. We have to record slots here, since we
        // might not have recorded them in first place.
        // Note: We mark the page as aborted here to be able to record slots
        // for code objects in |RecordMigratedSlotVisitor| and to be able
        // to identify the page later on for post processing.
        page->SetFlag(Page::COMPACTION_WAS_ABORTED);
        EvacuateRecordOnlyVisitor record_visitor(heap());
        success = object_visitor.VisitBlackObjects(
            page, state, &record_visitor, LiveObjectVisitor::kKeepMarking);
        ArrayBufferTracker::ProcessBuffers(
            page, ArrayBufferTracker::kUpdateForwardedKeepOthers);
        DCHECK(success);
        success = false;
      } else {
        ArrayBufferTracker::ProcessBuffers(
            page, ArrayBufferTracker::kUpdateForwardedRemoveOthers);
      }
      break;
  }
  return success;
}

class YoungGenerationEvacuator : public Evacuator {
 public:
  YoungGenerationEvacuator(MinorMarkCompactCollector* collector,
                           RecordMigratedSlotVisitor* record_visitor)
      : Evacuator(collector->heap(), record_visitor), collector_(collector) {}

 protected:
  bool RawEvacuatePage(Page* page, intptr_t* live_bytes) override;

  MinorMarkCompactCollector* collector_;
};

bool YoungGenerationEvacuator::RawEvacuatePage(Page* page,
                                               intptr_t* live_bytes) {
  bool success = false;
  LiveObjectVisitor object_visitor;
  const MarkingState state = collector_->marking_state(page);
  *live_bytes = state.live_bytes();
  switch (ComputeEvacuationMode(page)) {
    case kObjectsNewToOld:
      success = object_visitor.VisitBlackObjects(
          page, state, &new_space_visitor_, LiveObjectVisitor::kClearMarkbits);
      DCHECK(success);
      ArrayBufferTracker::ProcessBuffers(
          page, ArrayBufferTracker::kUpdateForwardedRemoveOthers);
      break;
    case kPageNewToOld:
      success = object_visitor.VisitBlackObjects(
          page, state, &new_to_old_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      DCHECK(success);
      new_to_old_page_visitor_.account_moved_bytes(state.live_bytes());
      // TODO(mlippautz): If cleaning array buffers is too slow here we can
      // delay it until the next GC.
      ArrayBufferTracker::FreeDead(page, state);
      if (heap()->ShouldZapGarbage())
        collector_->MakeIterable(page, MarkingTreatmentMode::KEEP,
                                 ZAP_FREE_SPACE);
      break;
    case kPageNewToNew:
      success = object_visitor.VisitBlackObjects(
          page, state, &new_to_new_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      DCHECK(success);
      new_to_new_page_visitor_.account_moved_bytes(state.live_bytes());
      // TODO(mlippautz): If cleaning array buffers is too slow here we can
      // delay it until the next GC.
      ArrayBufferTracker::FreeDead(page, state);
      if (heap()->ShouldZapGarbage())
        collector_->MakeIterable(page, MarkingTreatmentMode::KEEP,
                                 ZAP_FREE_SPACE);
      break;
    case kObjectsOldToOld:
      UNREACHABLE();
      break;
  }
  return success;
}

class EvacuationJobTraits {
 public:
  struct PageData {
    MarkingState marking_state;
  };

  typedef PageData PerPageData;
  typedef Evacuator* PerTaskData;

  static void ProcessPageInParallel(Heap* heap, PerTaskData evacuator,
                                    MemoryChunk* chunk, PerPageData) {
    evacuator->EvacuatePage(reinterpret_cast<Page*>(chunk));
  }
};

template <class Evacuator, class Collector>
void MarkCompactCollectorBase::CreateAndExecuteEvacuationTasks(
    Collector* collector, PageParallelJob<EvacuationJobTraits>* job,
    RecordMigratedSlotVisitor* record_visitor,
    MigrationObserver* migration_observer, const intptr_t live_bytes) {
  // Used for trace summary.
  double compaction_speed = 0;
  if (FLAG_trace_evacuation) {
    compaction_speed = heap()->tracer()->CompactionSpeedInBytesPerMillisecond();
  }

  const bool profiling =
      heap()->isolate()->is_profiling() ||
      heap()->isolate()->logger()->is_logging_code_events() ||
      heap()->isolate()->heap_profiler()->is_tracking_object_moves();
  ProfilingMigrationObserver profiling_observer(heap());

  const int wanted_num_tasks =
      NumberOfParallelCompactionTasks(job->NumberOfPages());
  Evacuator** evacuators = new Evacuator*[wanted_num_tasks];
  for (int i = 0; i < wanted_num_tasks; i++) {
    evacuators[i] = new Evacuator(collector, record_visitor);
    if (profiling) evacuators[i]->AddObserver(&profiling_observer);
    if (migration_observer != nullptr)
      evacuators[i]->AddObserver(migration_observer);
  }
  job->Run(wanted_num_tasks, [evacuators](int i) { return evacuators[i]; });
  const Address top = heap()->new_space()->top();
  for (int i = 0; i < wanted_num_tasks; i++) {
    evacuators[i]->Finalize();
    // Try to find the last LAB that was used for new space allocation in
    // evacuation tasks. If it was adjacent to the current top, move top back.
    const AllocationInfo info = evacuators[i]->CloseNewSpaceLAB();
    if (info.limit() != nullptr && info.limit() == top) {
      DCHECK_NOT_NULL(info.top());
      *heap()->new_space()->allocation_top_address() = info.top();
    }
    delete evacuators[i];
  }
  delete[] evacuators;

  if (FLAG_trace_evacuation) {
    PrintIsolate(isolate(),
                 "%8.0f ms: evacuation-summary: parallel=%s pages=%d "
                 "wanted_tasks=%d tasks=%d cores=%" PRIuS
                 " live_bytes=%" V8PRIdPTR " compaction_speed=%.f\n",
                 isolate()->time_millis_since_init(),
                 FLAG_parallel_compaction ? "yes" : "no", job->NumberOfPages(),
                 wanted_num_tasks, job->NumberOfTasks(),
                 V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads(),
                 live_bytes, compaction_speed);
  }
}

bool MarkCompactCollectorBase::ShouldMovePage(Page* p, intptr_t live_bytes) {
  const bool reduce_memory = heap()->ShouldReduceMemory();
  const Address age_mark = heap()->new_space()->age_mark();
  return !reduce_memory && !p->NeverEvacuate() &&
         (live_bytes > Evacuator::PageEvacuationThreshold()) &&
         !p->Contains(age_mark) && heap()->CanExpandOldGeneration(live_bytes);
}

void MarkCompactCollector::EvacuatePagesInParallel() {
  PageParallelJob<EvacuationJobTraits> job(
      heap_, heap_->isolate()->cancelable_task_manager(),
      &page_parallel_job_semaphore_);
  intptr_t live_bytes = 0;

  for (Page* page : old_space_evacuation_pages_) {
    live_bytes += MarkingState::Internal(page).live_bytes();
    job.AddPage(page, {marking_state(page)});
  }

  for (Page* page : new_space_evacuation_pages_) {
    intptr_t live_bytes_on_page = MarkingState::Internal(page).live_bytes();
    live_bytes += live_bytes_on_page;
    if (ShouldMovePage(page, live_bytes_on_page)) {
      if (page->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK)) {
        EvacuateNewSpacePageVisitor<NEW_TO_OLD>::Move(page);
      } else {
        EvacuateNewSpacePageVisitor<NEW_TO_NEW>::Move(page);
      }
    }
    job.AddPage(page, {marking_state(page)});
  }
  DCHECK_GE(job.NumberOfPages(), 1);

  RecordMigratedSlotVisitor record_visitor(this);
  CreateAndExecuteEvacuationTasks<FullEvacuator>(this, &job, &record_visitor,
                                                 nullptr, live_bytes);
  PostProcessEvacuationCandidates();
}

void MinorMarkCompactCollector::EvacuatePagesInParallel() {
  PageParallelJob<EvacuationJobTraits> job(
      heap_, heap_->isolate()->cancelable_task_manager(),
      &page_parallel_job_semaphore_);
  intptr_t live_bytes = 0;

  for (Page* page : new_space_evacuation_pages_) {
    intptr_t live_bytes_on_page = marking_state(page).live_bytes();
    live_bytes += live_bytes_on_page;
    if (ShouldMovePage(page, live_bytes_on_page)) {
      if (page->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK)) {
        EvacuateNewSpacePageVisitor<NEW_TO_OLD>::Move(page);
      } else {
        EvacuateNewSpacePageVisitor<NEW_TO_NEW>::Move(page);
      }
    }
    job.AddPage(page, {marking_state(page)});
  }
  DCHECK_GE(job.NumberOfPages(), 1);

  YoungGenerationMigrationObserver observer(heap(),
                                            heap()->mark_compact_collector());
  YoungGenerationRecordMigratedSlotVisitor record_visitor(
      heap()->mark_compact_collector());
  CreateAndExecuteEvacuationTasks<YoungGenerationEvacuator>(
      this, &job, &record_visitor, &observer, live_bytes);
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

MarkCompactCollector::Sweeper::ClearOldToNewSlotsMode
MarkCompactCollector::Sweeper::GetClearOldToNewSlotsMode(Page* p) {
  AllocationSpace identity = p->owner()->identity();
  if (p->slot_set<OLD_TO_NEW>() &&
      (identity == OLD_SPACE || identity == MAP_SPACE)) {
    return MarkCompactCollector::Sweeper::CLEAR_REGULAR_SLOTS;
  } else if (p->typed_slot_set<OLD_TO_NEW>() && identity == CODE_SPACE) {
    return MarkCompactCollector::Sweeper::CLEAR_TYPED_SLOTS;
  }
  return MarkCompactCollector::Sweeper::DO_NOT_CLEAR;
}

int MarkCompactCollector::Sweeper::RawSweep(
    Page* p, FreeListRebuildingMode free_list_mode,
    FreeSpaceTreatmentMode free_space_mode) {
  Space* space = p->owner();
  DCHECK_NOT_NULL(space);
  DCHECK(free_list_mode == IGNORE_FREE_LIST || space->identity() == OLD_SPACE ||
         space->identity() == CODE_SPACE || space->identity() == MAP_SPACE);
  DCHECK(!p->IsEvacuationCandidate() && !p->SweepingDone());

  // Sweeper takes the marking state of the full collector.
  const MarkingState state = MarkingState::Internal(p);

  // If there are old-to-new slots in that page, we have to filter out slots
  // that are in dead memory which is freed by the sweeper.
  ClearOldToNewSlotsMode slots_clearing_mode = GetClearOldToNewSlotsMode(p);

  // The free ranges map is used for filtering typed slots.
  std::map<uint32_t, uint32_t> free_ranges;

  // Before we sweep objects on the page, we free dead array buffers which
  // requires valid mark bits.
  ArrayBufferTracker::FreeDead(p, state);

  Address free_start = p->area_start();
  DCHECK(reinterpret_cast<intptr_t>(free_start) % (32 * kPointerSize) == 0);

  // If we use the skip list for code space pages, we have to lock the skip
  // list because it could be accessed concurrently by the runtime or the
  // deoptimizer.
  const bool rebuild_skip_list =
      space->identity() == CODE_SPACE && p->skip_list() != nullptr;
  SkipList* skip_list = p->skip_list();
  if (rebuild_skip_list) {
    skip_list->Clear();
  }

  intptr_t freed_bytes = 0;
  intptr_t max_freed_bytes = 0;
  int curr_region = -1;

  LiveObjectIterator<kBlackObjects> it(p, state);
  HeapObject* object = NULL;

  while ((object = it.Next()) != NULL) {
    DCHECK(ObjectMarking::IsBlack(object, state));
    Address free_end = object->address();
    if (free_end != free_start) {
      CHECK_GT(free_end, free_start);
      size_t size = static_cast<size_t>(free_end - free_start);
      if (free_space_mode == ZAP_FREE_SPACE) {
        memset(free_start, 0xcc, size);
      }
      if (free_list_mode == REBUILD_FREE_LIST) {
        freed_bytes = reinterpret_cast<PagedSpace*>(space)->UnaccountedFree(
            free_start, size);
        max_freed_bytes = Max(freed_bytes, max_freed_bytes);
      } else {
        p->heap()->CreateFillerObjectAt(free_start, static_cast<int>(size),
                                        ClearRecordedSlots::kNo);
      }

      if (slots_clearing_mode == CLEAR_REGULAR_SLOTS) {
        RememberedSet<OLD_TO_NEW>::RemoveRange(p, free_start, free_end,
                                               SlotSet::KEEP_EMPTY_BUCKETS);
      } else if (slots_clearing_mode == CLEAR_TYPED_SLOTS) {
        free_ranges.insert(std::pair<uint32_t, uint32_t>(
            static_cast<uint32_t>(free_start - p->address()),
            static_cast<uint32_t>(free_end - p->address())));
      }
    }
    Map* map = object->synchronized_map();
    int size = object->SizeFromMap(map);
    if (rebuild_skip_list) {
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

  if (free_start != p->area_end()) {
    CHECK_GT(p->area_end(), free_start);
    size_t size = static_cast<size_t>(p->area_end() - free_start);
    if (free_space_mode == ZAP_FREE_SPACE) {
      memset(free_start, 0xcc, size);
    }
    if (free_list_mode == REBUILD_FREE_LIST) {
      freed_bytes = reinterpret_cast<PagedSpace*>(space)->UnaccountedFree(
          free_start, size);
      max_freed_bytes = Max(freed_bytes, max_freed_bytes);
    } else {
      p->heap()->CreateFillerObjectAt(free_start, static_cast<int>(size),
                                      ClearRecordedSlots::kNo);
    }

    if (slots_clearing_mode == CLEAR_REGULAR_SLOTS) {
      RememberedSet<OLD_TO_NEW>::RemoveRange(p, free_start, p->area_end(),
                                             SlotSet::KEEP_EMPTY_BUCKETS);
    } else if (slots_clearing_mode == CLEAR_TYPED_SLOTS) {
      free_ranges.insert(std::pair<uint32_t, uint32_t>(
          static_cast<uint32_t>(free_start - p->address()),
          static_cast<uint32_t>(p->area_end() - p->address())));
    }
  }

  // Clear invalid typed slots after collection all free ranges.
  if (slots_clearing_mode == CLEAR_TYPED_SLOTS) {
    TypedSlotSet* typed_slot_set = p->typed_slot_set<OLD_TO_NEW>();
    if (typed_slot_set != nullptr) {
      typed_slot_set->RemoveInvaldSlots(free_ranges);
    }
  }

  // Clear the mark bits of that page and reset live bytes count.
  state.ClearLiveness();

  p->concurrent_sweeping_state().SetValue(Page::kSweepingDone);
  if (free_list_mode == IGNORE_FREE_LIST) return 0;
  return static_cast<int>(FreeList::GuaranteedAllocatable(max_freed_bytes));
}

void MarkCompactCollector::InvalidateCode(Code* code) {
  Page* page = Page::FromAddress(code->address());
  Address start = code->instruction_start();
  Address end = code->address() + code->Size();

  RememberedSet<OLD_TO_NEW>::RemoveRangeTyped(page, start, end);

  if (heap_->incremental_marking()->IsCompacting() &&
      !ShouldSkipEvacuationSlotRecording(code)) {
    DCHECK(compacting_);

    // If the object is white than no slots were recorded on it yet.
    if (ObjectMarking::IsWhite(code, MarkingState::Internal(code))) return;

    // Ignore all slots that might have been recorded in the body of the
    // deoptimized code object. Assumption: no slots will be recorded for
    // this object after invalidating it.
    RememberedSet<OLD_TO_OLD>::RemoveRangeTyped(page, start, end);
  }
}


// Return true if the given code is deoptimized or will be deoptimized.
bool MarkCompactCollector::WillBeDeoptimized(Code* code) {
  return code->is_optimized_code() && code->marked_for_deoptimization();
}

void MarkCompactCollector::RecordLiveSlotsOnPage(Page* page) {
  EvacuateRecordOnlyVisitor visitor(heap());
  LiveObjectVisitor object_visitor;
  object_visitor.VisitBlackObjects(page, MarkingState::Internal(page), &visitor,
                                   LiveObjectVisitor::kKeepMarking);
}

template <class Visitor>
bool LiveObjectVisitor::VisitBlackObjects(MemoryChunk* chunk,
                                          const MarkingState& state,
                                          Visitor* visitor,
                                          IterationMode iteration_mode) {
  LiveObjectIterator<kBlackObjects> it(chunk, state);
  HeapObject* object = nullptr;
  while ((object = it.Next()) != nullptr) {
    DCHECK(ObjectMarking::IsBlack(object, state));
    if (!visitor->Visit(object)) {
      if (iteration_mode == kClearMarkbits) {
        state.bitmap()->ClearRange(
            chunk->AddressToMarkbitIndex(chunk->area_start()),
            chunk->AddressToMarkbitIndex(object->address()));
        SlotSet* slot_set = chunk->slot_set<OLD_TO_NEW>();
        if (slot_set != nullptr) {
          slot_set->RemoveRange(
              0, static_cast<int>(object->address() - chunk->address()),
              SlotSet::PREFREE_EMPTY_BUCKETS);
        }
        RememberedSet<OLD_TO_NEW>::RemoveRangeTyped(chunk, chunk->address(),
                                                    object->address());
        RecomputeLiveBytes(chunk, state);
      }
      return false;
    }
  }
  if (iteration_mode == kClearMarkbits) {
    state.ClearLiveness();
  }
  return true;
}

void LiveObjectVisitor::RecomputeLiveBytes(MemoryChunk* chunk,
                                           const MarkingState& state) {
  LiveObjectIterator<kBlackObjects> it(chunk, state);
  int new_live_size = 0;
  HeapObject* object = nullptr;
  while ((object = it.Next()) != nullptr) {
    new_live_size += object->Size();
  }
  state.SetLiveBytes(new_live_size);
}

void MarkCompactCollector::Sweeper::AddSweptPageSafe(PagedSpace* space,
                                                     Page* page) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  swept_list_[space->identity()].Add(page);
}

void MarkCompactCollector::Evacuate() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE);
  base::LockGuard<base::Mutex> guard(heap()->relocation_mutex());

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_PROLOGUE);
    EvacuatePrologue();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_COPY);
    EvacuationScope evacuation_scope(this);
    EvacuatePagesInParallel();
  }

  UpdatePointersAfterEvacuation();

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_REBALANCE);
    if (!heap()->new_space()->Rebalance()) {
      FatalProcessOutOfMemory("NewSpace::Rebalance");
    }
  }

  // Give pages that are queued to be freed back to the OS. Note that filtering
  // slots only handles old space (for unboxed doubles), and thus map space can
  // still contain stale pointers. We only free the chunks after pointer updates
  // to still have access to page headers.
  heap()->memory_allocator()->unmapper()->FreeQueuedChunks();

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_CLEAN_UP);

    for (Page* p : new_space_evacuation_pages_) {
      if (p->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION)) {
        p->ClearFlag(Page::PAGE_NEW_NEW_PROMOTION);
        sweeper().AddPage(p->owner()->identity(), p);
      } else if (p->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION)) {
        p->ClearFlag(Page::PAGE_NEW_OLD_PROMOTION);
        p->ForAllFreeListCategories(
            [](FreeListCategory* category) { DCHECK(!category->is_linked()); });
        sweeper().AddPage(p->owner()->identity(), p);
      }
    }
    new_space_evacuation_pages_.Rewind(0);

    for (Page* p : old_space_evacuation_pages_) {
      // Important: skip list should be cleared only after roots were updated
      // because root iteration traverses the stack and might have to find
      // code objects from non-updated pc pointing into evacuation candidate.
      SkipList* list = p->skip_list();
      if (list != NULL) list->Clear();
      if (p->IsFlagSet(Page::COMPACTION_WAS_ABORTED)) {
        sweeper().AddPage(p->owner()->identity(), p);
        p->ClearFlag(Page::COMPACTION_WAS_ABORTED);
      }
    }
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_EPILOGUE);
    EvacuateEpilogue();
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && !sweeper().sweeping_in_progress()) {
    FullEvacuationVerifier verifier(heap());
    verifier.Run();
  }
#endif
}

template <RememberedSetType type>
class PointerUpdateJobTraits {
 public:
  typedef int PerPageData;  // Per page data is not used in this job.
  typedef const MarkCompactCollectorBase* PerTaskData;

  static void ProcessPageInParallel(Heap* heap, PerTaskData task_data,
                                    MemoryChunk* chunk, PerPageData) {
    UpdateUntypedPointers(heap, chunk, task_data);
    UpdateTypedPointers(heap, chunk, task_data);
  }

 private:
  static void UpdateUntypedPointers(Heap* heap, MemoryChunk* chunk,
                                    const MarkCompactCollectorBase* collector) {
    base::LockGuard<base::RecursiveMutex> guard(chunk->mutex());
    if (type == OLD_TO_NEW) {
      RememberedSet<OLD_TO_NEW>::Iterate(
          chunk, [heap, collector](Address slot) {
            return CheckAndUpdateOldToNewSlot(heap, slot, collector);
          });
    } else {
      RememberedSet<OLD_TO_OLD>::Iterate(chunk, [](Address slot) {
        return UpdateSlot(reinterpret_cast<Object**>(slot));
      });
    }
  }

  static void UpdateTypedPointers(Heap* heap, MemoryChunk* chunk,
                                  const MarkCompactCollectorBase* collector) {
    if (type == OLD_TO_OLD) {
      Isolate* isolate = heap->isolate();
      RememberedSet<OLD_TO_OLD>::IterateTyped(
          chunk,
          [isolate](SlotType slot_type, Address host_addr, Address slot) {
            return UpdateTypedSlotHelper::UpdateTypedSlot(isolate, slot_type,
                                                          slot, UpdateSlot);
          });
    } else {
      Isolate* isolate = heap->isolate();
      RememberedSet<OLD_TO_NEW>::IterateTyped(
          chunk, [isolate, heap, collector](SlotType slot_type,
                                            Address host_addr, Address slot) {
            return UpdateTypedSlotHelper::UpdateTypedSlot(
                isolate, slot_type, slot, [heap, collector](Object** slot) {
                  return CheckAndUpdateOldToNewSlot(
                      heap, reinterpret_cast<Address>(slot), collector);
                });
          });
    }
  }

  static SlotCallbackResult CheckAndUpdateOldToNewSlot(
      Heap* heap, Address slot_address,
      const MarkCompactCollectorBase* collector) {
    // There may be concurrent action on slots in dead objects. Concurrent
    // sweeper threads may overwrite the slot content with a free space object.
    // Moreover, the pointed-to object may also get concurrently overwritten
    // with a free space object. The sweeper always gets priority performing
    // these writes.
    base::NoBarrierAtomicValue<Object*>* slot =
        base::NoBarrierAtomicValue<Object*>::FromAddress(slot_address);
    Object* slot_reference = slot->Value();
    if (heap->InFromSpace(slot_reference)) {
      HeapObject* heap_object = reinterpret_cast<HeapObject*>(slot_reference);
      DCHECK(heap_object->IsHeapObject());
      MapWord map_word = heap_object->map_word();
      // There could still be stale pointers in large object space, map space,
      // and old space for pages that have been promoted.
      if (map_word.IsForwardingAddress()) {
        // A sweeper thread may concurrently write a size value which looks like
        // a forwarding pointer. We have to ignore these values.
        if (map_word.ToRawValue() < Page::kPageSize) {
          return REMOVE_SLOT;
        }
        // Update the corresponding slot only if the slot content did not
        // change in the meantime. This may happen when a concurrent sweeper
        // thread stored a free space object at that memory location.
        slot->TrySetValue(slot_reference, map_word.ToForwardingAddress());
      }
      // If the object was in from space before and is after executing the
      // callback in to space, the object is still live.
      // Unfortunately, we do not know about the slot. It could be in a
      // just freed free space object.
      if (heap->InToSpace(slot->Value())) {
        return KEEP_SLOT;
      }
    } else if (heap->InToSpace(slot_reference)) {
      // Slots can point to "to" space if the page has been moved, or if the
      // slot has been recorded multiple times in the remembered set. Since
      // there is no forwarding information present we need to check the
      // markbits to determine liveness.
      HeapObject* heap_object = reinterpret_cast<HeapObject*>(slot_reference);
      if (ObjectMarking::IsBlack(heap_object,
                                 collector->marking_state(heap_object)))
        return KEEP_SLOT;
    } else {
      DCHECK(!heap->InNewSpace(slot_reference));
    }
    return REMOVE_SLOT;
  }
};

template <RememberedSetType type>
void MarkCompactCollectorBase::UpdatePointersInParallel(
    Heap* heap, base::Semaphore* semaphore,
    const MarkCompactCollectorBase* collector) {
  PageParallelJob<PointerUpdateJobTraits<type> > job(
      heap, heap->isolate()->cancelable_task_manager(), semaphore);
  RememberedSet<type>::IterateMemoryChunks(
      heap, [&job](MemoryChunk* chunk) { job.AddPage(chunk, 0); });
  int num_pages = job.NumberOfPages();
  int num_tasks = NumberOfPointerUpdateTasks(num_pages);
  job.Run(num_tasks, [collector](int i) { return collector; });
}

class ToSpacePointerUpdateJobTraits {
 public:
  struct PageData {
    Address start;
    Address end;
    MarkingState marking_state;
  };

  typedef PageData PerPageData;
  typedef PointersUpdatingVisitor* PerTaskData;

  static void ProcessPageInParallel(Heap* heap, PerTaskData visitor,
                                    MemoryChunk* chunk, PerPageData page_data) {
    if (chunk->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION)) {
      // New->new promoted pages contain garbage so they require iteration
      // using markbits.
      ProcessPageInParallelVisitLive(heap, visitor, chunk, page_data);
    } else {
      ProcessPageInParallelVisitAll(heap, visitor, chunk, page_data);
    }
  }

 private:
  static void ProcessPageInParallelVisitAll(Heap* heap, PerTaskData visitor,
                                            MemoryChunk* chunk,
                                            PerPageData page_data) {
    for (Address cur = page_data.start; cur < page_data.end;) {
      HeapObject* object = HeapObject::FromAddress(cur);
      Map* map = object->map();
      int size = object->SizeFromMap(map);
      object->IterateBody(map->instance_type(), size, visitor);
      cur += size;
    }
  }

  static void ProcessPageInParallelVisitLive(Heap* heap, PerTaskData visitor,
                                             MemoryChunk* chunk,
                                             PerPageData page_data) {
    LiveObjectIterator<kBlackObjects> it(chunk, page_data.marking_state);
    HeapObject* object = NULL;
    while ((object = it.Next()) != NULL) {
      Map* map = object->map();
      int size = object->SizeFromMap(map);
      object->IterateBody(map->instance_type(), size, visitor);
    }
  }
};

template <class MarkingStateProvider>
void UpdateToSpacePointersInParallel(
    Heap* heap, base::Semaphore* semaphore,
    const MarkingStateProvider& marking_state_provider) {
  PageParallelJob<ToSpacePointerUpdateJobTraits> job(
      heap, heap->isolate()->cancelable_task_manager(), semaphore);
  Address space_start = heap->new_space()->bottom();
  Address space_end = heap->new_space()->top();
  for (Page* page : PageRange(space_start, space_end)) {
    Address start =
        page->Contains(space_start) ? space_start : page->area_start();
    Address end = page->Contains(space_end) ? space_end : page->area_end();
    job.AddPage(page, {start, end, marking_state_provider.marking_state(page)});
  }
  PointersUpdatingVisitor visitor;
  int num_tasks = FLAG_parallel_pointer_update ? job.NumberOfPages() : 1;
  job.Run(num_tasks, [&visitor](int i) { return &visitor; });
}

void MarkCompactCollector::UpdatePointersAfterEvacuation() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS);


  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_TO_NEW);
    UpdateToSpacePointersInParallel(heap_, &page_parallel_job_semaphore_,
                                    *this);
    // Update roots.
    PointersUpdatingVisitor updating_visitor;
    heap_->IterateRoots(&updating_visitor, VISIT_ALL_IN_SWEEP_NEWSPACE);
    UpdatePointersInParallel<OLD_TO_NEW>(heap_, &page_parallel_job_semaphore_,
                                         this);
  }

  {
    Heap* heap = this->heap();
    TRACE_GC(heap->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_TO_EVACUATED);
    UpdatePointersInParallel<OLD_TO_OLD>(heap_, &page_parallel_job_semaphore_,
                                         this);
  }

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_WEAK);
    // Update pointers from external string table.
    heap_->UpdateReferencesInExternalStringTable(
        &UpdateReferenceInExternalStringTableEntry);

    EvacuationWeakObjectRetainer evacuation_object_retainer;
    heap()->ProcessWeakListRoots(&evacuation_object_retainer);
  }
}

void MinorMarkCompactCollector::UpdatePointersAfterEvacuation() {
  TRACE_GC(heap()->tracer(),
           GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS);

  PointersUpdatingVisitor updating_visitor;

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_TO_NEW);
    {
      TRACE_GC(
          heap()->tracer(),
          GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_TO_NEW_TOSPACE);
      UpdateToSpacePointersInParallel(heap_, &page_parallel_job_semaphore_,
                                      *this);
    }
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_TO_NEW_ROOTS);
      heap_->IterateRoots(&updating_visitor, VISIT_ALL_IN_MINOR_MC_UPDATE);
    }
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_TO_NEW_OLD);
      UpdatePointersInParallel<OLD_TO_NEW>(heap_, &page_parallel_job_semaphore_,
                                           this);
    }
  }

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_WEAK);

    EvacuationWeakObjectRetainer evacuation_object_retainer;
    heap()->ProcessWeakListRoots(&evacuation_object_retainer);

    // Update pointers from external string table.
    heap()->UpdateNewSpaceReferencesInExternalStringTable(
        &UpdateReferenceInExternalStringTableEntry);
    heap()->IterateEncounteredWeakCollections(&updating_visitor);
  }
}

void MarkCompactCollector::PostProcessEvacuationCandidates() {
  int aborted_pages = 0;
  for (Page* p : old_space_evacuation_pages_) {
    if (p->IsFlagSet(Page::COMPACTION_WAS_ABORTED)) {
      p->ClearEvacuationCandidate();
      aborted_pages++;
    } else {
      DCHECK(p->IsEvacuationCandidate());
      DCHECK(p->SweepingDone());
      p->Unlink();
    }
  }
  if (FLAG_trace_evacuation && (aborted_pages > 0)) {
    PrintIsolate(isolate(), "%8.0f ms: evacuation: aborted=%d\n",
                 isolate()->time_millis_since_init(), aborted_pages);
  }
}

void MarkCompactCollector::ReleaseEvacuationCandidates() {
  for (Page* p : old_space_evacuation_pages_) {
    if (!p->IsEvacuationCandidate()) continue;
    PagedSpace* space = static_cast<PagedSpace*>(p->owner());
    MarkingState::Internal(p).SetLiveBytes(0);
    CHECK(p->SweepingDone());
    space->ReleasePage(p);
  }
  old_space_evacuation_pages_.Rewind(0);
  compacting_ = false;
  heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
}

int MarkCompactCollector::Sweeper::ParallelSweepSpace(AllocationSpace identity,
                                                      int required_freed_bytes,
                                                      int max_pages) {
  int max_freed = 0;
  int pages_freed = 0;
  Page* page = nullptr;
  while ((page = GetSweepingPageSafe(identity)) != nullptr) {
    int freed = ParallelSweepPage(page, identity);
    pages_freed += 1;
    DCHECK_GE(freed, 0);
    max_freed = Max(max_freed, freed);
    if ((required_freed_bytes) > 0 && (max_freed >= required_freed_bytes))
      return max_freed;
    if ((max_pages > 0) && (pages_freed >= max_pages)) return max_freed;
  }
  return max_freed;
}

int MarkCompactCollector::Sweeper::ParallelSweepPage(Page* page,
                                                     AllocationSpace identity) {
  int max_freed = 0;
  {
    base::LockGuard<base::RecursiveMutex> guard(page->mutex());
    // If this page was already swept in the meantime, we can return here.
    if (page->SweepingDone()) return 0;
    DCHECK_EQ(Page::kSweepingPending,
              page->concurrent_sweeping_state().Value());
    page->concurrent_sweeping_state().SetValue(Page::kSweepingInProgress);
    const FreeSpaceTreatmentMode free_space_mode =
        Heap::ShouldZapGarbage() ? ZAP_FREE_SPACE : IGNORE_FREE_SPACE;
    if (identity == NEW_SPACE) {
      RawSweep(page, IGNORE_FREE_LIST, free_space_mode);
    } else {
      max_freed = RawSweep(page, REBUILD_FREE_LIST, free_space_mode);
    }
    DCHECK(page->SweepingDone());

    // After finishing sweeping of a page we clean up its remembered set.
    TypedSlotSet* typed_slot_set = page->typed_slot_set<OLD_TO_NEW>();
    if (typed_slot_set) {
      typed_slot_set->FreeToBeFreedChunks();
    }
    SlotSet* slot_set = page->slot_set<OLD_TO_NEW>();
    if (slot_set) {
      slot_set->FreeToBeFreedBuckets();
    }
  }

  {
    base::LockGuard<base::Mutex> guard(&mutex_);
    swept_list_[identity].Add(page);
  }
  return max_freed;
}

void MarkCompactCollector::Sweeper::AddPage(AllocationSpace space, Page* page) {
  DCHECK(!FLAG_concurrent_sweeping || !AreSweeperTasksRunning());
  PrepareToBeSweptPage(space, page);
  sweeping_list_[space].push_back(page);
}

void MarkCompactCollector::Sweeper::PrepareToBeSweptPage(AllocationSpace space,
                                                         Page* page) {
  page->concurrent_sweeping_state().SetValue(Page::kSweepingPending);
  DCHECK_GE(page->area_size(),
            static_cast<size_t>(MarkingState::Internal(page).live_bytes()));
  size_t to_sweep =
      page->area_size() - MarkingState::Internal(page).live_bytes();
  if (space != NEW_SPACE)
    heap_->paged_space(space)->accounting_stats_.ShrinkSpace(to_sweep);
}

Page* MarkCompactCollector::Sweeper::GetSweepingPageSafe(
    AllocationSpace space) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  Page* page = nullptr;
  if (!sweeping_list_[space].empty()) {
    page = sweeping_list_[space].front();
    sweeping_list_[space].pop_front();
  }
  return page;
}

void MarkCompactCollector::Sweeper::AddSweepingPageSafe(AllocationSpace space,
                                                        Page* page) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  sweeping_list_[space].push_back(page);
}

void MarkCompactCollector::StartSweepSpace(PagedSpace* space) {
  space->ClearStats();

  int will_be_swept = 0;
  bool unused_page_present = false;

  // Loop needs to support deletion if live bytes == 0 for a page.
  for (auto it = space->begin(); it != space->end();) {
    Page* p = *(it++);
    DCHECK(p->SweepingDone());

    if (p->IsEvacuationCandidate()) {
      // Will be processed in Evacuate.
      DCHECK(evacuation_candidates_.length() > 0);
      continue;
    }

    if (p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
      // We need to sweep the page to get it into an iterable state again. Note
      // that this adds unusable memory into the free list that is later on
      // (in the free list) dropped again. Since we only use the flag for
      // testing this is fine.
      p->concurrent_sweeping_state().SetValue(Page::kSweepingInProgress);
      Sweeper::RawSweep(p, Sweeper::IGNORE_FREE_LIST,
                        Heap::ShouldZapGarbage()
                            ? FreeSpaceTreatmentMode::ZAP_FREE_SPACE
                            : FreeSpaceTreatmentMode::IGNORE_FREE_SPACE);
      continue;
    }

    // One unused page is kept, all further are released before sweeping them.
    if (MarkingState::Internal(p).live_bytes() == 0) {
      if (unused_page_present) {
        if (FLAG_gc_verbose) {
          PrintIsolate(isolate(), "sweeping: released page: %p",
                       static_cast<void*>(p));
        }
        ArrayBufferTracker::FreeAll(p);
        space->ReleasePage(p);
        continue;
      }
      unused_page_present = true;
    }

    sweeper().AddPage(space->identity(), p);
    will_be_swept++;
  }

  if (FLAG_gc_verbose) {
    PrintIsolate(isolate(), "sweeping: space=%s initialized_for_sweeping=%d",
                 AllocationSpaceName(space->identity()), will_be_swept);
  }
}

void MarkCompactCollector::StartSweepSpaces() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_SWEEP);
#ifdef DEBUG
  state_ = SWEEP_SPACES;
#endif

  {
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
    sweeper().StartSweeping();
  }

  // Deallocate unmarked large objects.
  heap_->lo_space()->FreeUnmarkedObjects();
}

void MarkCompactCollector::Initialize() {
  MarkCompactMarkingVisitor::Initialize();
  IncrementalMarking::Initialize();
}

void MarkCompactCollector::RecordCodeEntrySlot(HeapObject* host, Address slot,
                                               Code* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  Page* source_page = Page::FromAddress(reinterpret_cast<Address>(host));
  if (target_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(host)) {
    // TODO(ulan): remove this check after investigating crbug.com/414964.
    CHECK(target->IsCode());
    RememberedSet<OLD_TO_OLD>::InsertTyped(
        source_page, reinterpret_cast<Address>(host), CODE_ENTRY_SLOT, slot);
  }
}


void MarkCompactCollector::RecordCodeTargetPatch(Address pc, Code* target) {
  DCHECK(heap()->gc_state() == Heap::MARK_COMPACT);
  if (is_compacting()) {
    Code* host =
        isolate()->inner_pointer_to_code_cache()->GcSafeFindCodeForInnerPointer(
            pc);
    if (ObjectMarking::IsBlack(host, MarkingState::Internal(host))) {
      RelocInfo rinfo(pc, RelocInfo::CODE_TARGET, 0, host);
      // The target is always in old space, we don't have to record the slot in
      // the old-to-new remembered set.
      DCHECK(!heap()->InNewSpace(target));
      RecordRelocSlot(host, &rinfo, target);
    }
  }
}

}  // namespace internal
}  // namespace v8

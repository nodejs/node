// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/mark-compact.h"

#include <unordered_map>

#include "src/base/utils/random-number-generator.h"
#include "src/cancelable-task.h"
#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/frames-inl.h"
#include "src/global-handles.h"
#include "src/heap/array-buffer-collector.h"
#include "src/heap/array-buffer-tracker-inl.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/invalidated-slots-inl.h"
#include "src/heap/item-parallel-job.h"
#include "src/heap/local-allocator.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/sweeper.h"
#include "src/heap/worklist.h"
#include "src/ic/stub-cache.h"
#include "src/transitions-inl.h"
#include "src/utils-inl.h"
#include "src/v8.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

const char* Marking::kWhiteBitPattern = "00";
const char* Marking::kBlackBitPattern = "11";
const char* Marking::kGreyBitPattern = "10";
const char* Marking::kImpossibleBitPattern = "01";

// The following has to hold in order for {MarkingState::MarkBitFrom} to not
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

  virtual Bitmap* bitmap(const MemoryChunk* chunk) = 0;

  virtual void VerifyPointers(Object** start, Object** end) = 0;

  virtual bool IsMarked(HeapObject* object) = 0;

  virtual bool IsBlackOrGrey(HeapObject* object) = 0;

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    VerifyPointers(start, end);
  }

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    VerifyPointers(start, end);
  }

  void VerifyRoots(VisitMode mode);
  void VerifyMarkingOnPage(const Page* page, Address start, Address end);
  void VerifyMarking(NewSpace* new_space);
  void VerifyMarking(PagedSpace* paged_space);

  Heap* heap_;
};

void MarkingVerifier::VerifyRoots(VisitMode mode) {
  heap_->IterateStrongRoots(this, mode);
}

void MarkingVerifier::VerifyMarkingOnPage(const Page* page, Address start,
                                          Address end) {
  HeapObject* object;
  Address next_object_must_be_here_or_later = start;
  for (Address current = start; current < end;) {
    object = HeapObject::FromAddress(current);
    // One word fillers at the end of a black area can be grey.
    if (IsBlackOrGrey(object) &&
        object->map() != heap_->one_pointer_filler_map()) {
      CHECK(IsMarked(object));
      CHECK(current >= next_object_must_be_here_or_later);
      object->Iterate(this);
      next_object_must_be_here_or_later = current + object->Size();
      // The object is either part of a black area of black allocation or a
      // regular black object
      CHECK(
          bitmap(page)->AllBitsSetInRange(
              page->AddressToMarkbitIndex(current),
              page->AddressToMarkbitIndex(next_object_must_be_here_or_later)) ||
          bitmap(page)->AllBitsClearInRange(
              page->AddressToMarkbitIndex(current + kPointerSize * 2),
              page->AddressToMarkbitIndex(next_object_must_be_here_or_later)));
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
    VerifyMarkingOnPage(page, page->area_start(), limit);
  }
}

void MarkingVerifier::VerifyMarking(PagedSpace* space) {
  for (Page* p : *space) {
    VerifyMarkingOnPage(p, p->area_start(), p->area_end());
  }
}

class FullMarkingVerifier : public MarkingVerifier {
 public:
  explicit FullMarkingVerifier(Heap* heap)
      : MarkingVerifier(heap),
        marking_state_(
            heap->mark_compact_collector()->non_atomic_marking_state()) {}

  void Run() override {
    VerifyRoots(VISIT_ONLY_STRONG);
    VerifyMarking(heap_->new_space());
    VerifyMarking(heap_->old_space());
    VerifyMarking(heap_->code_space());
    VerifyMarking(heap_->map_space());

    LargeObjectIterator it(heap_->lo_space());
    for (HeapObject* obj = it.Next(); obj != nullptr; obj = it.Next()) {
      if (marking_state_->IsBlackOrGrey(obj)) {
        obj->Iterate(this);
      }
    }
  }

 protected:
  Bitmap* bitmap(const MemoryChunk* chunk) override {
    return marking_state_->bitmap(chunk);
  }

  bool IsMarked(HeapObject* object) override {
    return marking_state_->IsBlack(object);
  }

  bool IsBlackOrGrey(HeapObject* object) override {
    return marking_state_->IsBlackOrGrey(object);
  }

  void VerifyPointers(Object** start, Object** end) override {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        CHECK(marking_state_->IsBlackOrGrey(object));
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

 private:
  MarkCompactCollector::NonAtomicMarkingState* marking_state_;
};

class YoungGenerationMarkingVerifier : public MarkingVerifier {
 public:
  explicit YoungGenerationMarkingVerifier(Heap* heap)
      : MarkingVerifier(heap),
        marking_state_(
            heap->minor_mark_compact_collector()->non_atomic_marking_state()) {}

  Bitmap* bitmap(const MemoryChunk* chunk) override {
    return marking_state_->bitmap(chunk);
  }

  bool IsMarked(HeapObject* object) override {
    return marking_state_->IsGrey(object);
  }

  bool IsBlackOrGrey(HeapObject* object) override {
    return marking_state_->IsBlackOrGrey(object);
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
        CHECK(IsMarked(object));
      }
    }
  }

 private:
  MinorMarkCompactCollector::NonAtomicMarkingState* marking_state_;
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

using MarkCompactMarkingVisitor =
    MarkingVisitor<FixedArrayVisitationMode::kRegular,
                   TraceRetainingPathMode::kEnabled,
                   MarkCompactCollector::MarkingState>;

namespace {

// This root visitor walks all roots and creates items bundling objects that
// are then processed later on. Slots have to be dereferenced as they could
// live on the native (C++) stack, which requires filtering out the indirection.
template <class BatchedItem>
class RootMarkingVisitorSeedOnly : public RootVisitor {
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
    job_->AddItem(new BatchedItem(std::move(buffered_objects_)));
    // Moving leaves the container in a valid but unspecified state. Reusing the
    // container requires a call without precondition that resets the state.
    buffered_objects_.clear();
    buffered_objects_.reserve(kBufferSize);
  }

 private:
  // Bundling several objects together in items avoids issues with allocating
  // and deallocating items; both are operations that are performed on the main
  // thread.
  static const int kBufferSize = 128;

  void AddObject(Object* object) {
    buffered_objects_.push_back(object);
    if (buffered_objects_.size() == kBufferSize) FlushObjects();
  }

  ItemParallelJob* job_;
  std::vector<Object*> buffered_objects_;
};

}  // namespace

static int NumberOfAvailableCores() {
  return Max(
      1, static_cast<int>(
             V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads()));
}

int MarkCompactCollectorBase::NumberOfParallelCompactionTasks(int pages) {
  DCHECK_GT(pages, 0);
  return FLAG_parallel_compaction ? Min(NumberOfAvailableCores(), pages) : 1;
}

int MarkCompactCollectorBase::NumberOfParallelPointerUpdateTasks(int pages,
                                                                 int slots) {
  DCHECK_GT(pages, 0);
  // Limit the number of update tasks as task creation often dominates the
  // actual work that is being done.
  const int kMaxPointerUpdateTasks = 8;
  const int kSlotsPerTask = 600;
  const int wanted_tasks =
      (slots >= 0) ? Max(1, Min(pages, slots / kSlotsPerTask)) : pages;
  return FLAG_parallel_pointer_update
             ? Min(kMaxPointerUpdateTasks,
                   Min(NumberOfAvailableCores(), wanted_tasks))
             : 1;
}

int MarkCompactCollectorBase::NumberOfParallelToSpacePointerUpdateTasks(
    int pages) {
  DCHECK_GT(pages, 0);
  // No cap needed because all pages we need to process are fully filled with
  // interesting objects.
  return FLAG_parallel_pointer_update ? Min(NumberOfAvailableCores(), pages)
                                      : 1;
}

int MinorMarkCompactCollector::NumberOfParallelMarkingTasks(int pages) {
  DCHECK_GT(pages, 0);
  if (!FLAG_minor_mc_parallel_marking) return 1;
  // Pages are not private to markers but we can still use them to estimate the
  // amount of marking that is required.
  const int kPagesPerTask = 2;
  const int wanted_tasks = Max(1, pages / kPagesPerTask);
  return Min(NumberOfAvailableCores(), Min(wanted_tasks, kNumMarkers));
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
      marking_worklist_(heap),
      sweeper_(new Sweeper(heap, non_atomic_marking_state())) {
  old_to_new_slots_ = -1;
}

MarkCompactCollector::~MarkCompactCollector() { delete sweeper_; }

void MarkCompactCollector::SetUp() {
  DCHECK_EQ(0, strcmp(Marking::kWhiteBitPattern, "00"));
  DCHECK_EQ(0, strcmp(Marking::kBlackBitPattern, "11"));
  DCHECK_EQ(0, strcmp(Marking::kGreyBitPattern, "10"));
  DCHECK_EQ(0, strcmp(Marking::kImpossibleBitPattern, "01"));
}

void MinorMarkCompactCollector::SetUp() {}

void MarkCompactCollector::TearDown() {
  AbortCompaction();
  AbortWeakObjects();
  if (heap()->incremental_marking()->IsMarking()) {
    marking_worklist()->Clear();
  }
}

void MinorMarkCompactCollector::TearDown() {}

void MarkCompactCollector::AddEvacuationCandidate(Page* p) {
  DCHECK(!p->NeverEvacuate());
  p->MarkEvacuationCandidate();
  evacuation_candidates_.push_back(p);
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
    DCHECK(evacuation_candidates_.empty());

    CollectEvacuationCandidates(heap()->old_space());

    if (FLAG_compact_code_space) {
      CollectEvacuationCandidates(heap()->code_space());
    } else if (FLAG_trace_fragmentation) {
      TraceFragmentation(heap()->code_space());
    }

    if (FLAG_trace_fragmentation) {
      TraceFragmentation(heap()->map_space());
    }

    compacting_ = !evacuation_candidates_.empty();
  }

  return compacting_;
}

void MarkCompactCollector::CollectGarbage() {
  // Make sure that Prepare() has been called. The individual steps below will
  // update the state as they proceed.
  DCHECK(state_ == PREPARE_GC);

  heap()->minor_mark_compact_collector()->CleanupSweepToIteratePages();

  MarkLiveObjects();
  ClearNonLiveReferences();
  VerifyMarking();

  RecordObjectStats();

  StartSweepSpaces();

  Evacuate();

  Finish();
}

#ifdef VERIFY_HEAP
void MarkCompactCollector::VerifyMarkbitsAreClean(PagedSpace* space) {
  for (Page* p : *space) {
    CHECK(non_atomic_marking_state()->bitmap(p)->IsClean());
    CHECK_EQ(0, non_atomic_marking_state()->live_bytes(p));
  }
}


void MarkCompactCollector::VerifyMarkbitsAreClean(NewSpace* space) {
  for (Page* p : PageRange(space->bottom(), space->top())) {
    CHECK(non_atomic_marking_state()->bitmap(p)->IsClean());
    CHECK_EQ(0, non_atomic_marking_state()->live_bytes(p));
  }
}


void MarkCompactCollector::VerifyMarkbitsAreClean() {
  VerifyMarkbitsAreClean(heap_->old_space());
  VerifyMarkbitsAreClean(heap_->code_space());
  VerifyMarkbitsAreClean(heap_->map_space());
  VerifyMarkbitsAreClean(heap_->new_space());

  LargeObjectIterator it(heap_->lo_space());
  for (HeapObject* obj = it.Next(); obj != nullptr; obj = it.Next()) {
    CHECK(non_atomic_marking_state()->IsWhite(obj));
    CHECK_EQ(0, non_atomic_marking_state()->live_bytes(
                    MemoryChunk::FromAddress(obj->address())));
  }
}

void MarkCompactCollector::VerifyWeakEmbeddedObjectsInCode() {
  HeapObjectIterator code_iterator(heap()->code_space());
  for (HeapObject* obj = code_iterator.Next(); obj != nullptr;
       obj = code_iterator.Next()) {
    Code* code = Code::cast(obj);
    if (!code->is_optimized_code()) continue;
    if (WillBeDeoptimized(code)) continue;
    code->VerifyEmbeddedObjectsDependency();
  }
}

#endif  // VERIFY_HEAP

void MarkCompactCollector::ClearMarkbitsInPagedSpace(PagedSpace* space) {
  for (Page* p : *space) {
    non_atomic_marking_state()->ClearLiveness(p);
  }
}

void MarkCompactCollector::ClearMarkbitsInNewSpace(NewSpace* space) {
  for (Page* p : *space) {
    non_atomic_marking_state()->ClearLiveness(p);
  }
}


void MarkCompactCollector::ClearMarkbits() {
  ClearMarkbitsInPagedSpace(heap_->code_space());
  ClearMarkbitsInPagedSpace(heap_->map_space());
  ClearMarkbitsInPagedSpace(heap_->old_space());
  ClearMarkbitsInNewSpace(heap_->new_space());
  heap_->lo_space()->ClearMarkingStateOfLiveObjects();
}

void MarkCompactCollector::EnsureSweepingCompleted() {
  if (!sweeper()->sweeping_in_progress()) return;

  sweeper()->EnsureCompleted();
  heap()->old_space()->RefillFreeList();
  heap()->code_space()->RefillFreeList();
  heap()->map_space()->RefillFreeList();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && !evacuation()) {
    FullEvacuationVerifier verifier(heap());
    verifier.Run();
  }
#endif

  if (heap()->memory_allocator()->unmapper()->has_delayed_chunks())
    heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
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
    if (p->NeverEvacuate() || (p == owner_of_linear_allocation_area) ||
        !p->CanAllocate())
      continue;
    // Invariant: Evacuation candidates are just created when marking is
    // started. This means that sweeping has finished. Furthermore, at the end
    // of a GC all evacuation candidates are cleared and their slot buffers are
    // released.
    CHECK(!p->IsEvacuationCandidate());
    CHECK_NULL(p->slot_set<OLD_TO_OLD>());
    CHECK_NULL(p->typed_slot_set<OLD_TO_OLD>());
    CHECK(p->SweepingDone());
    DCHECK(p->area_size() == area_size);
    pages.push_back(std::make_pair(p->allocated_bytes(), p));
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
  } else if (FLAG_stress_compaction_random) {
    double fraction = isolate()->fuzzer_rng()->NextDouble();
    size_t pages_to_mark_count =
        static_cast<size_t>(fraction * (pages.size() + 1));
    for (uint64_t i : isolate()->fuzzer_rng()->NextSample(
             pages.size(), pages_to_mark_count)) {
      candidate_count++;
      total_live_bytes += pages[i].first;
      AddEvacuationCandidate(pages[i].second);
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
    evacuation_candidates_.clear();
  }
  DCHECK(evacuation_candidates_.empty());
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

  // Clear marking bits if incremental marking is aborted.
  if (was_marked_incrementally_ && heap_->ShouldAbortIncrementalMarking()) {
    heap()->incremental_marking()->Stop();
    heap()->incremental_marking()->AbortBlackAllocation();
    FinishConcurrentMarking();
    heap()->incremental_marking()->Deactivate();
    ClearMarkbits();
    AbortWeakCollections();
    AbortWeakObjects();
    AbortCompaction();
    heap_->local_embedder_heap_tracer()->AbortTracing();
    marking_worklist()->Clear();
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
  for (PagedSpace* space = spaces.next(); space != nullptr;
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

void MarkCompactCollector::FinishConcurrentMarking() {
  if (FLAG_concurrent_marking) {
    heap()->concurrent_marking()->EnsureCompleted();
    heap()->concurrent_marking()->FlushLiveBytes(non_atomic_marking_state());
  }
}

void MarkCompactCollector::VerifyMarking() {
  CHECK(marking_worklist()->IsEmpty());
  DCHECK(heap_->incremental_marking()->IsStopped());
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    FullMarkingVerifier verifier(heap());
    verifier.Run();
  }
#endif
#ifdef VERIFY_HEAP
  heap()->old_space()->VerifyLiveBytes();
  heap()->map_space()->VerifyLiveBytes();
  heap()->code_space()->VerifyLiveBytes();
#endif
}

void MarkCompactCollector::Finish() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_FINISH);

#ifdef DEBUG
  heap()->VerifyCountersBeforeConcurrentSweeping();
#endif

  sweeper()->StartSweeperTasks();

  // The hashing of weak_object_to_code_table is no longer valid.
  heap()->weak_object_to_code_table()->Rehash();

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
}

void MinorMarkCompactCollector::CleanupSweepToIteratePages() {
  for (Page* p : sweep_to_iterate_pages_) {
    if (p->IsFlagSet(Page::SWEEP_TO_ITERATE)) {
      p->ClearFlag(Page::SWEEP_TO_ITERATE);
      non_atomic_marking_state()->ClearLiveness(p);
    }
  }
  sweep_to_iterate_pages_.clear();
}

class MarkCompactCollector::RootMarkingVisitor final : public RootVisitor {
 public:
  explicit RootMarkingVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitRootPointer(Root root, Object** p) final {
    MarkObjectByPointer(root, p);
  }

  void VisitRootPointers(Root root, Object** start, Object** end) final {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(root, p);
  }

 private:
  V8_INLINE void MarkObjectByPointer(Root root, Object** p) {
    if (!(*p)->IsHeapObject()) return;

    collector_->MarkRootObject(root, HeapObject::cast(*p));
  }

  MarkCompactCollector* const collector_;
};

// This visitor is used to visit the body of special objects held alive by
// other roots.
//
// It is currently used for
// - Code held alive by the top optimized frame. This code cannot be deoptimized
// and thus have to be kept alive in an isolate way, i.e., it should not keep
// alive other code objects reachable through the weak list but they should
// keep alive its embedded pointers (which would otherwise be dropped).
// - Prefix of the string table.
class MarkCompactCollector::CustomRootBodyMarkingVisitor final
    : public ObjectVisitor {
 public:
  explicit CustomRootBodyMarkingVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitPointer(HeapObject* host, Object** p) final {
    MarkObject(host, *p);
  }

  void VisitPointers(HeapObject* host, Object** start, Object** end) final {
    for (Object** p = start; p < end; p++) MarkObject(host, *p);
  }

  // VisitEmbedderPointer is defined by ObjectVisitor to call VisitPointers.

 private:
  void MarkObject(HeapObject* host, Object* object) {
    if (!object->IsHeapObject()) return;
    collector_->MarkObject(host, HeapObject::cast(object));
  }

  MarkCompactCollector* const collector_;
};

class InternalizedStringTableCleaner : public ObjectVisitor {
 public:
  InternalizedStringTableCleaner(Heap* heap, HeapObject* table)
      : heap_(heap), pointers_removed_(0), table_(table) {}

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    // Visit all HeapObject pointers in [start, end).
    Object* the_hole = heap_->the_hole_value();
    MarkCompactCollector::NonAtomicMarkingState* marking_state =
        heap_->mark_compact_collector()->non_atomic_marking_state();
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (o->IsHeapObject()) {
        HeapObject* heap_object = HeapObject::cast(o);
        if (marking_state->IsWhite(heap_object)) {
          pointers_removed_++;
          // Set the entry to the_hole_value (as deleted).
          *p = the_hole;
        } else {
          // StringTable contains only old space strings.
          DCHECK(!heap_->InNewSpace(o));
          MarkCompactCollector::RecordSlot(table_, p, o);
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
    MarkCompactCollector::NonAtomicMarkingState* marking_state =
        heap_->mark_compact_collector()->non_atomic_marking_state();
    Object* the_hole = heap_->the_hole_value();
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (o->IsHeapObject()) {
        HeapObject* heap_object = HeapObject::cast(o);
        if (marking_state->IsWhite(heap_object)) {
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
      MinorMarkCompactCollector* collector)
      : heap_(collector->heap()),
        marking_state_(collector->non_atomic_marking_state()) {}

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    DCHECK_EQ(static_cast<int>(root),
              static_cast<int>(Root::kExternalStringsTable));
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (o->IsHeapObject()) {
        HeapObject* heap_object = HeapObject::cast(o);
        if (marking_state_->IsWhite(heap_object)) {
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
  MinorMarkCompactCollector::NonAtomicMarkingState* marking_state_;
};

// Marked young generation objects and all old generation objects will be
// retained.
class MinorMarkCompactWeakObjectRetainer : public WeakObjectRetainer {
 public:
  explicit MinorMarkCompactWeakObjectRetainer(
      MinorMarkCompactCollector* collector)
      : heap_(collector->heap()),
        marking_state_(collector->non_atomic_marking_state()) {}

  virtual Object* RetainAs(Object* object) {
    HeapObject* heap_object = HeapObject::cast(object);
    if (!heap_->InNewSpace(heap_object)) return object;

    // Young generation marking only marks to grey instead of black.
    DCHECK(!marking_state_->IsBlack(heap_object));
    if (marking_state_->IsGrey(heap_object)) {
      return object;
    }
    return nullptr;
  }

 private:
  Heap* heap_;
  MinorMarkCompactCollector::NonAtomicMarkingState* marking_state_;
};

// Implementation of WeakObjectRetainer for mark compact GCs. All marked objects
// are retained.
class MarkCompactWeakObjectRetainer : public WeakObjectRetainer {
 public:
  explicit MarkCompactWeakObjectRetainer(
      MarkCompactCollector::NonAtomicMarkingState* marking_state)
      : marking_state_(marking_state) {}

  virtual Object* RetainAs(Object* object) {
    HeapObject* heap_object = HeapObject::cast(object);
    DCHECK(!marking_state_->IsGrey(heap_object));
    if (marking_state_->IsBlack(heap_object)) {
      return object;
    } else if (object->IsAllocationSite() &&
               !(AllocationSite::cast(object)->IsZombie())) {
      // "dead" AllocationSites need to live long enough for a traversal of new
      // space. These sites get a one-time reprieve.
      AllocationSite* site = AllocationSite::cast(object);
      site->MarkZombie();
      marking_state_->WhiteToBlack(site);
      return object;
    } else {
      return nullptr;
    }
  }

 private:
  MarkCompactCollector::NonAtomicMarkingState* marking_state_;
};

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

  inline void VisitCodeTarget(Code* host, RelocInfo* rinfo) override {
    DCHECK_EQ(host, rinfo->host());
    DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
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
        DCHECK_IMPLIES(p->InToSpace(),
                       p->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION));
        RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(
            Page::FromAddress(slot), slot);
      } else if (p->IsEvacuationCandidate()) {
        RememberedSet<OLD_TO_OLD>::Insert<AccessMode::NON_ATOMIC>(
            Page::FromAddress(slot), slot);
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
      DCHECK(
          heap_->incremental_marking()->atomic_marking_state()->IsWhite(dst));
      heap_->incremental_marking()->TransferColor(src, dst);
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

  void VisitCodeTarget(Code* host, RelocInfo* rinfo) final { UNREACHABLE(); }
  void VisitEmbeddedPointer(Code* host, RelocInfo* rinfo) final {
    UNREACHABLE();
  }

 private:
  // Only record slots for host objects that are considered as live by the full
  // collector.
  inline bool IsLive(HeapObject* object) {
    return collector_->non_atomic_marking_state()->IsBlack(object);
  }

  inline void RecordMigratedSlot(HeapObject* host, Object* value,
                                 Address slot) final {
    if (value->IsHeapObject()) {
      Page* p = Page::FromAddress(reinterpret_cast<Address>(value));
      if (p->InNewSpace()) {
        DCHECK_IMPLIES(p->InToSpace(),
                       p->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION));
        RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(
            Page::FromAddress(slot), slot);
      } else if (p->IsEvacuationCandidate() && IsLive(host)) {
        RememberedSet<OLD_TO_OLD>::Insert<AccessMode::NON_ATOMIC>(
            Page::FromAddress(slot), slot);
      }
    }
  }
};

class HeapObjectVisitor {
 public:
  virtual ~HeapObjectVisitor() {}
  virtual bool Visit(HeapObject* object, int size) = 0;
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
    base::Relaxed_Store(reinterpret_cast<base::AtomicWord*>(src_addr),
                        reinterpret_cast<base::AtomicWord>(dst_addr));
  }

  EvacuateVisitorBase(Heap* heap, LocalAllocator* local_allocator,
                      RecordMigratedSlotVisitor* record_visitor)
      : heap_(heap),
        local_allocator_(local_allocator),
        record_visitor_(record_visitor) {
    migration_function_ = RawMigrateObject<MigrationMode::kFast>;
  }

  inline bool TryEvacuateObject(AllocationSpace target_space,
                                HeapObject* object, int size,
                                HeapObject** target_object) {
#ifdef VERIFY_HEAP
    if (AbortCompactionForTesting(object)) return false;
#endif  // VERIFY_HEAP
    AllocationAlignment alignment = object->RequiredAlignment();
    AllocationResult allocation =
        local_allocator_->Allocate(target_space, size, alignment);
    if (allocation.To(target_object)) {
      MigrateObject(*target_object, object, size, target_space);
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
  LocalAllocator* local_allocator_;
  RecordMigratedSlotVisitor* record_visitor_;
  std::vector<MigrationObserver*> observers_;
  MigrateFunction migration_function_;
};

class EvacuateNewSpaceVisitor final : public EvacuateVisitorBase {
 public:
  explicit EvacuateNewSpaceVisitor(
      Heap* heap, LocalAllocator* local_allocator,
      RecordMigratedSlotVisitor* record_visitor,
      Heap::PretenuringFeedbackMap* local_pretenuring_feedback)
      : EvacuateVisitorBase(heap, local_allocator, record_visitor),
        buffer_(LocalAllocationBuffer::InvalidBuffer()),
        promoted_size_(0),
        semispace_copied_size_(0),
        local_pretenuring_feedback_(local_pretenuring_feedback) {}

  inline bool Visit(HeapObject* object, int size) override {
    HeapObject* target_object = nullptr;
    if (heap_->ShouldBePromoted(object->address()) &&
        TryEvacuateObject(OLD_SPACE, object, size, &target_object)) {
      promoted_size_ += size;
      return true;
    }
    heap_->UpdateAllocationSite(object->map(), object,
                                local_pretenuring_feedback_);
    HeapObject* target = nullptr;
    AllocationSpace space = AllocateTargetObject(object, size, &target);
    MigrateObject(HeapObject::cast(target), object, size, space);
    semispace_copied_size_ += size;
    return true;
  }

  intptr_t promoted_size() { return promoted_size_; }
  intptr_t semispace_copied_size() { return semispace_copied_size_; }

 private:
  inline AllocationSpace AllocateTargetObject(HeapObject* old_object, int size,
                                              HeapObject** target_object) {
    AllocationAlignment alignment = old_object->RequiredAlignment();
    AllocationSpace space_allocated_in = NEW_SPACE;
    AllocationResult allocation =
        local_allocator_->Allocate(NEW_SPACE, size, alignment);
    if (allocation.IsRetry()) {
      allocation = AllocateInOldSpace(size, alignment);
      space_allocated_in = OLD_SPACE;
    }
    bool ok = allocation.To(target_object);
    DCHECK(ok);
    USE(ok);
    return space_allocated_in;
  }

  inline AllocationResult AllocateInOldSpace(int size_in_bytes,
                                             AllocationAlignment alignment) {
    AllocationResult allocation =
        local_allocator_->Allocate(OLD_SPACE, size_in_bytes, alignment);
    if (allocation.IsRetry()) {
      v8::internal::Heap::FatalProcessOutOfMemory(
          "MarkCompactCollector: semi-space copy, fallback in old gen", true);
    }
    return allocation;
  }

  LocalAllocationBuffer buffer_;
  intptr_t promoted_size_;
  intptr_t semispace_copied_size_;
  Heap::PretenuringFeedbackMap* local_pretenuring_feedback_;
};

template <PageEvacuationMode mode>
class EvacuateNewSpacePageVisitor final : public HeapObjectVisitor {
 public:
  explicit EvacuateNewSpacePageVisitor(
      Heap* heap, RecordMigratedSlotVisitor* record_visitor,
      Heap::PretenuringFeedbackMap* local_pretenuring_feedback)
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
        DCHECK(!new_page->InNewSpace());
        new_page->SetFlag(Page::PAGE_NEW_OLD_PROMOTION);
        break;
      }
    }
  }

  inline bool Visit(HeapObject* object, int size) {
    if (mode == NEW_TO_NEW) {
      heap_->UpdateAllocationSite(object->map(), object,
                                  local_pretenuring_feedback_);
    } else if (mode == NEW_TO_OLD) {
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
  Heap::PretenuringFeedbackMap* local_pretenuring_feedback_;
};

class EvacuateOldSpaceVisitor final : public EvacuateVisitorBase {
 public:
  EvacuateOldSpaceVisitor(Heap* heap, LocalAllocator* local_allocator,
                          RecordMigratedSlotVisitor* record_visitor)
      : EvacuateVisitorBase(heap, local_allocator, record_visitor) {}

  inline bool Visit(HeapObject* object, int size) override {
    HeapObject* target_object = nullptr;
    if (TryEvacuateObject(
            Page::FromAddress(object->address())->owner()->identity(), object,
            size, &target_object)) {
      DCHECK(object->map_word().IsForwardingAddress());
      return true;
    }
    return false;
  }
};

class EvacuateRecordOnlyVisitor final : public HeapObjectVisitor {
 public:
  explicit EvacuateRecordOnlyVisitor(Heap* heap) : heap_(heap) {}

  inline bool Visit(HeapObject* object, int size) {
    RecordMigratedSlotVisitor visitor(heap_->mark_compact_collector());
    object->IterateBody(&visitor);
    return true;
  }

 private:
  Heap* heap_;
};

bool MarkCompactCollector::IsUnmarkedHeapObject(Object** p) {
  Object* o = *p;
  if (!o->IsHeapObject()) return false;
  HeapObject* heap_object = HeapObject::cast(o);
  return heap_object->GetHeap()
      ->mark_compact_collector()
      ->non_atomic_marking_state()
      ->IsWhite(HeapObject::cast(o));
}

void MarkCompactCollector::MarkStringTable(
    ObjectVisitor* custom_root_body_visitor) {
  StringTable* string_table = heap()->string_table();
  // Mark the string table itself.
  if (marking_state()->WhiteToBlack(string_table)) {
    // Explicitly mark the prefix.
    string_table->IteratePrefix(custom_root_body_visitor);
  }
}

void MarkCompactCollector::MarkRoots(RootVisitor* root_visitor,
                                     ObjectVisitor* custom_root_body_visitor) {
  // Mark the heap roots including global variables, stack variables,
  // etc., and all objects reachable from them.
  heap()->IterateStrongRoots(root_visitor, VISIT_ONLY_STRONG);

  // Custom marking for string table and top optimized frame.
  MarkStringTable(custom_root_body_visitor);
  ProcessTopOptimizedFrame(custom_root_body_visitor);
}

void MarkCompactCollector::ProcessMarkingWorklist() {
  HeapObject* object;
  MarkCompactMarkingVisitor visitor(this, marking_state());
  while ((object = marking_worklist()->Pop()) != nullptr) {
    DCHECK(!object->IsFiller());
    DCHECK(object->IsHeapObject());
    DCHECK(heap()->Contains(object));
    DCHECK(!(marking_state()->IsWhite(object)));
    marking_state()->GreyToBlack(object);
    Map* map = object->map();
    MarkObject(object, map);
    visitor.Visit(map, object);
  }
  DCHECK(marking_worklist()->IsBailoutEmpty());
}

// Mark all objects reachable (transitively) from objects on the marking
// stack including references only considered in the atomic marking pause.
void MarkCompactCollector::ProcessEphemeralMarking(
    bool only_process_harmony_weak_collections) {
  DCHECK(marking_worklist()->IsEmpty());
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
    work_to_do = !marking_worklist()->IsEmpty();
    ProcessMarkingWorklist();
  }
  CHECK(marking_worklist()->IsEmpty());
  CHECK_EQ(0, heap()->local_embedder_heap_tracer()->NumberOfWrappersToTrace());
}

void MarkCompactCollector::ProcessTopOptimizedFrame(ObjectVisitor* visitor) {
  for (StackFrameIterator it(isolate(), isolate()->thread_local_top());
       !it.done(); it.Advance()) {
    if (it.frame()->type() == StackFrame::INTERPRETED) {
      return;
    }
    if (it.frame()->type() == StackFrame::OPTIMIZED) {
      Code* code = it.frame()->LookupCode();
      if (!code->CanDeoptAt(it.frame()->pc())) {
        Code::BodyDescriptor::IterateBody(code, visitor);
      }
      return;
    }
  }
}

class ObjectStatsVisitor : public HeapObjectVisitor {
 public:
  ObjectStatsVisitor(Heap* heap, ObjectStats* live_stats,
                     ObjectStats* dead_stats)
      : live_collector_(heap, live_stats),
        dead_collector_(heap, dead_stats),
        marking_state_(
            heap->mark_compact_collector()->non_atomic_marking_state()) {
    DCHECK_NOT_NULL(live_stats);
    DCHECK_NOT_NULL(dead_stats);
    // Global objects are roots and thus recorded as live.
    live_collector_.CollectGlobalStatistics();
  }

  bool Visit(HeapObject* obj, int size) override {
    if (marking_state_->IsBlack(obj)) {
      live_collector_.CollectStatistics(obj);
    } else {
      DCHECK(!marking_state_->IsGrey(obj));
      dead_collector_.CollectStatistics(obj);
    }
    return true;
  }

 private:
  ObjectStatsCollector live_collector_;
  ObjectStatsCollector dead_collector_;
  MarkCompactCollector::NonAtomicMarkingState* marking_state_;
};

void MarkCompactCollector::VisitAllObjects(HeapObjectVisitor* visitor) {
  SpaceIterator space_it(heap());
  HeapObject* obj = nullptr;
  while (space_it.has_next()) {
    std::unique_ptr<ObjectIterator> it(space_it.next()->GetObjectIterator());
    ObjectIterator* obj_it = it.get();
    while ((obj = obj_it->Next()) != nullptr) {
      visitor->Visit(obj, obj->Size());
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
    : public NewSpaceVisitor<YoungGenerationMarkingVisitor> {
 public:
  YoungGenerationMarkingVisitor(
      Heap* heap, MinorMarkCompactCollector::MarkingState* marking_state,
      MinorMarkCompactCollector::MarkingWorklist* global_worklist, int task_id)
      : heap_(heap),
        worklist_(global_worklist, task_id),
        marking_state_(marking_state) {}

  V8_INLINE void VisitPointers(HeapObject* host, Object** start,
                               Object** end) final {
    for (Object** p = start; p < end; p++) {
      VisitPointer(host, p);
    }
  }

  V8_INLINE void VisitPointer(HeapObject* host, Object** slot) final {
    Object* target = *slot;
    if (heap_->InNewSpace(target)) {
      HeapObject* target_object = HeapObject::cast(target);
      MarkObjectViaMarkingWorklist(target_object);
    }
  }

 private:
  inline void MarkObjectViaMarkingWorklist(HeapObject* object) {
    if (marking_state_->WhiteToGrey(object)) {
      // Marking deque overflow is unsupported for the young generation.
      CHECK(worklist_.Push(object));
    }
  }

  Heap* heap_;
  MinorMarkCompactCollector::MarkingWorklist::View worklist_;
  MinorMarkCompactCollector::MarkingState* marking_state_;
};

class MinorMarkCompactCollector::RootMarkingVisitor : public RootVisitor {
 public:
  explicit RootMarkingVisitor(MinorMarkCompactCollector* collector)
      : collector_(collector),
        marking_state_(collector_->non_atomic_marking_state()) {}

  void VisitRootPointer(Root root, Object** p) override {
    MarkObjectByPointer(p);
  }

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

 private:
  void MarkObjectByPointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    HeapObject* object = HeapObject::cast(*p);

    if (!collector_->heap()->InNewSpace(object)) return;

    if (marking_state_->WhiteToGrey(object)) {
      collector_->main_marking_visitor()->Visit(object);
      collector_->ProcessMarkingWorklist();
    }
  }

  MinorMarkCompactCollector* collector_;
  MinorMarkCompactCollector::NonAtomicMarkingState* marking_state_;
};

class MarkingItem;
class GlobalHandlesMarkingItem;
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
  YoungGenerationMarkingTask(
      Isolate* isolate, MinorMarkCompactCollector* collector,
      MinorMarkCompactCollector::MarkingWorklist* global_worklist, int task_id)
      : ItemParallelJob::Task(isolate),
        collector_(collector),
        marking_worklist_(global_worklist, task_id),
        marking_state_(collector->marking_state()),
        visitor_(isolate->heap(), marking_state_, global_worklist, task_id) {
    local_live_bytes_.reserve(isolate->heap()->new_space()->Capacity() /
                              Page::kPageSize);
  }

  void RunInParallel() override {
    double marking_time = 0.0;
    {
      TimedScope scope(&marking_time);
      MarkingItem* item = nullptr;
      while ((item = GetItem<MarkingItem>()) != nullptr) {
        item->Process(this);
        item->MarkFinished();
        EmptyLocalMarkingWorklist();
      }
      EmptyMarkingWorklist();
      DCHECK(marking_worklist_.IsLocalEmpty());
      FlushLiveBytes();
    }
    if (FLAG_trace_minor_mc_parallel_marking) {
      PrintIsolate(collector_->isolate(), "marking[%p]: time=%f\n",
                   static_cast<void*>(this), marking_time);
    }
  };

  void MarkObject(Object* object) {
    if (!collector_->heap()->InNewSpace(object)) return;
    HeapObject* heap_object = HeapObject::cast(object);
    if (marking_state_->WhiteToGrey(heap_object)) {
      const int size = visitor_.Visit(heap_object);
      IncrementLiveBytes(heap_object, size);
    }
  }

 private:
  void EmptyLocalMarkingWorklist() {
    HeapObject* object = nullptr;
    while (marking_worklist_.Pop(&object)) {
      const int size = visitor_.Visit(object);
      IncrementLiveBytes(object, size);
    }
  }

  void EmptyMarkingWorklist() {
    HeapObject* object = nullptr;
    while (marking_worklist_.Pop(&object)) {
      const int size = visitor_.Visit(object);
      IncrementLiveBytes(object, size);
    }
  }

  void IncrementLiveBytes(HeapObject* object, intptr_t bytes) {
    local_live_bytes_[Page::FromAddress(reinterpret_cast<Address>(object))] +=
        bytes;
  }

  void FlushLiveBytes() {
    for (auto pair : local_live_bytes_) {
      marking_state_->IncrementLiveBytes(pair.first, pair.second);
    }
  }

  MinorMarkCompactCollector* collector_;
  MinorMarkCompactCollector::MarkingWorklist::View marking_worklist_;
  MinorMarkCompactCollector::MarkingState* marking_state_;
  YoungGenerationMarkingVisitor visitor_;
  std::unordered_map<Page*, intptr_t, Page::Hasher> local_live_bytes_;
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
  explicit PageMarkingItem(MemoryChunk* chunk,
                           base::AtomicNumber<intptr_t>* global_slots)
      : chunk_(chunk), global_slots_(global_slots), slots_(0) {}
  virtual ~PageMarkingItem() { global_slots_->Increment(slots_); }

  void Process(YoungGenerationMarkingTask* task) override {
    base::LockGuard<base::Mutex> guard(chunk_->mutex());
    MarkUntypedPointers(task);
    MarkTypedPointers(task);
  }

 private:
  inline Heap* heap() { return chunk_->heap(); }

  void MarkUntypedPointers(YoungGenerationMarkingTask* task) {
    RememberedSet<OLD_TO_NEW>::Iterate(
        chunk_,
        [this, task](Address slot) { return CheckAndMarkObject(task, slot); },
        SlotSet::PREFREE_EMPTY_BUCKETS);
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
      slots_++;
      return KEEP_SLOT;
    }
    return REMOVE_SLOT;
  }

  MemoryChunk* chunk_;
  base::AtomicNumber<intptr_t>* global_slots_;
  intptr_t slots_;
};

class GlobalHandlesMarkingItem : public MarkingItem {
 public:
  GlobalHandlesMarkingItem(GlobalHandles* global_handles, size_t start,
                           size_t end)
      : global_handles_(global_handles), start_(start), end_(end) {}
  virtual ~GlobalHandlesMarkingItem() {}

  void Process(YoungGenerationMarkingTask* task) override {
    GlobalHandlesRootMarkingVisitor visitor(task);
    global_handles_
        ->IterateNewSpaceStrongAndDependentRootsAndIdentifyUnmodified(
            &visitor, start_, end_);
  }

 private:
  class GlobalHandlesRootMarkingVisitor : public RootVisitor {
   public:
    explicit GlobalHandlesRootMarkingVisitor(YoungGenerationMarkingTask* task)
        : task_(task) {}

    void VisitRootPointer(Root root, Object** p) override {
      DCHECK_EQ(Root::kGlobalHandles, root);
      task_->MarkObject(*p);
    }

    void VisitRootPointers(Root root, Object** start, Object** end) override {
      DCHECK_EQ(Root::kGlobalHandles, root);
      for (Object** p = start; p < end; p++) {
        task_->MarkObject(*p);
      }
    }

   private:
    YoungGenerationMarkingTask* task_;
  };

  GlobalHandles* global_handles_;
  size_t start_;
  size_t end_;
};

MinorMarkCompactCollector::MinorMarkCompactCollector(Heap* heap)
    : MarkCompactCollectorBase(heap),
      worklist_(new MinorMarkCompactCollector::MarkingWorklist()),
      main_marking_visitor_(new YoungGenerationMarkingVisitor(
          heap, marking_state(), worklist_, kMainMarker)),
      page_parallel_job_semaphore_(0) {
  static_assert(
      kNumMarkers <= MinorMarkCompactCollector::MarkingWorklist::kMaxNumTasks,
      "more marker tasks than marking deque can handle");
}

MinorMarkCompactCollector::~MinorMarkCompactCollector() {
  delete worklist_;
  delete main_marking_visitor_;
}

static bool IsUnmarkedObjectForYoungGeneration(Heap* heap, Object** p) {
  DCHECK_IMPLIES(heap->InNewSpace(*p), heap->InToSpace(*p));
  return heap->InNewSpace(*p) && !heap->minor_mark_compact_collector()
                                      ->non_atomic_marking_state()
                                      ->IsGrey(HeapObject::cast(*p));
}

template <class ParallelItem>
static void SeedGlobalHandles(GlobalHandles* global_handles,
                              ItemParallelJob* job) {
  // Create batches of global handles.
  const size_t kGlobalHandlesBufferSize = 1000;
  const size_t new_space_nodes = global_handles->NumberOfNewSpaceNodes();
  for (size_t start = 0; start < new_space_nodes;
       start += kGlobalHandlesBufferSize) {
    size_t end = start + kGlobalHandlesBufferSize;
    if (end > new_space_nodes) end = new_space_nodes;
    job->AddItem(new ParallelItem(global_handles, start, end));
  }
}

void MinorMarkCompactCollector::MarkRootSetInParallel() {
  base::AtomicNumber<intptr_t> slots;
  {
    ItemParallelJob job(isolate()->cancelable_task_manager(),
                        &page_parallel_job_semaphore_);

    // Seed the root set (roots + old->new set).
    {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_SEED);
      // Create batches of roots.
      RootMarkingVisitorSeedOnly<BatchedRootMarkingItem> root_seed_visitor(
          &job);
      heap()->IterateRoots(&root_seed_visitor, VISIT_ALL_IN_MINOR_MC_MARK);
      // Create batches of global handles.
      SeedGlobalHandles<GlobalHandlesMarkingItem>(isolate()->global_handles(),
                                                  &job);
      // Create items for each page.
      RememberedSet<OLD_TO_NEW>::IterateMemoryChunks(
          heap(), [&job, &slots](MemoryChunk* chunk) {
            job.AddItem(new PageMarkingItem(chunk, &slots));
          });
      // Flush any remaining objects in the seeding visitor.
      root_seed_visitor.FlushObjects();
    }

    // Add tasks and run in parallel.
    {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_ROOTS);
      const int new_space_pages =
          static_cast<int>(heap()->new_space()->Capacity()) / Page::kPageSize;
      const int num_tasks = NumberOfParallelMarkingTasks(new_space_pages);
      for (int i = 0; i < num_tasks; i++) {
        job.AddTask(
            new YoungGenerationMarkingTask(isolate(), this, worklist(), i));
      }
      job.Run();
      DCHECK(worklist()->IsGlobalEmpty());
    }
  }
  old_to_new_slots_ = static_cast<int>(slots.Value());
}

void MinorMarkCompactCollector::MarkLiveObjects() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK);

  PostponeInterruptsScope postpone(isolate());

  RootMarkingVisitor root_visitor(this);

  MarkRootSetInParallel();

  // Mark rest on the main thread.
  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_WEAK);
    heap()->IterateEncounteredWeakCollections(&root_visitor);
    ProcessMarkingWorklist();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_GLOBAL_HANDLES);
    isolate()->global_handles()->MarkNewSpaceWeakUnmodifiedObjectsPending(
        &IsUnmarkedObjectForYoungGeneration);
    isolate()
        ->global_handles()
        ->IterateNewSpaceWeakUnmodifiedRootsForFinalizers(&root_visitor);
    isolate()
        ->global_handles()
        ->IterateNewSpaceWeakUnmodifiedRootsForPhantomHandles(
            &root_visitor, &IsUnmarkedObjectForYoungGeneration);
    ProcessMarkingWorklist();
  }
}

void MinorMarkCompactCollector::ProcessMarkingWorklist() {
  MarkingWorklist::View marking_worklist(worklist(), kMainMarker);
  HeapObject* object = nullptr;
  while (marking_worklist.Pop(&object)) {
    DCHECK(!object->IsFiller());
    DCHECK(object->IsHeapObject());
    DCHECK(heap()->Contains(object));
    DCHECK(non_atomic_marking_state()->IsGrey(object));
    main_marking_visitor()->Visit(object);
  }
  DCHECK(marking_worklist.IsLocalEmpty());
}

void MinorMarkCompactCollector::CollectGarbage() {
  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_SWEEPING);
    heap()->mark_compact_collector()->sweeper()->EnsureNewSpaceCompleted();
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
    heap()->incremental_marking()->UpdateMarkingWorklistAfterScavenge();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_RESET_LIVENESS);
    for (Page* p : PageRange(heap()->new_space()->FromSpaceStart(),
                             heap()->new_space()->FromSpaceEnd())) {
      DCHECK(!p->IsFlagSet(Page::SWEEP_TO_ITERATE));
      non_atomic_marking_state()->ClearLiveness(p);
      if (FLAG_concurrent_marking) {
        // Ensure that concurrent marker does not track pages that are
        // going to be unmapped.
        heap()->concurrent_marking()->ClearLiveness(p);
      }
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
  DCHECK_EQ(0, reinterpret_cast<intptr_t>(free_start) % (32 * kPointerSize));

  for (auto object_and_size :
       LiveObjectRange<kGreyObjects>(p, marking_state()->bitmap(p))) {
    HeapObject* const object = object_and_size.first;
    DCHECK(non_atomic_marking_state()->IsGrey(object));
    Address free_end = object->address();
    if (free_end != free_start) {
      CHECK_GT(free_end, free_start);
      size_t size = static_cast<size_t>(free_end - free_start);
      full_collector->non_atomic_marking_state()->bitmap(p)->ClearRange(
          p->AddressToMarkbitIndex(free_start),
          p->AddressToMarkbitIndex(free_end));
      if (free_space_mode == ZAP_FREE_SPACE) {
        memset(free_start, 0xcc, size);
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
    full_collector->non_atomic_marking_state()->bitmap(p)->ClearRange(
        p->AddressToMarkbitIndex(free_start),
        p->AddressToMarkbitIndex(p->area_end()));
    if (free_space_mode == ZAP_FREE_SPACE) {
      memset(free_start, 0xcc, size);
    }
    p->heap()->CreateFillerObjectAt(free_start, static_cast<int>(size),
                                    ClearRecordedSlots::kNo);
  }

  if (marking_mode == MarkingTreatmentMode::CLEAR) {
    non_atomic_marking_state()->ClearLiveness(p);
    p->ClearFlag(Page::SWEEP_TO_ITERATE);
  }
}

void MinorMarkCompactCollector::ClearNonLiveReferences() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_CLEAR);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_CLEAR_STRING_TABLE);
    // Internalized strings are always stored in old space, so there is no need
    // to clean them here.
    YoungGenerationExternalStringTableCleaner external_visitor(this);
    heap()->external_string_table_.IterateNewSpaceStrings(&external_visitor);
    heap()->external_string_table_.CleanUpNewSpaceStrings();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_CLEAR_WEAK_LISTS);
    // Process the weak references.
    MinorMarkCompactWeakObjectRetainer retainer(this);
    heap()->ProcessYoungWeakReferences(&retainer);
  }
}

void MinorMarkCompactCollector::EvacuatePrologue() {
  NewSpace* new_space = heap()->new_space();
  // Append the list of new space pages to be processed.
  for (Page* p : PageRange(new_space->bottom(), new_space->top())) {
    new_space_evacuation_pages_.push_back(p);
  }
  new_space->Flip();
  new_space->ResetAllocationInfo();
}

void MinorMarkCompactCollector::EvacuateEpilogue() {
  heap()->new_space()->set_age_mark(heap()->new_space()->top());
  // Give pages that are queued to be freed back to the OS.
  heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
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
    new_space_evacuation_pages_.clear();
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

  heap_->local_embedder_heap_tracer()->EnterFinalPause();

  RootMarkingVisitor root_visitor(this);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_ROOTS);
    CustomRootBodyMarkingVisitor custom_root_body_visitor(this);
    MarkRoots(&root_visitor, &custom_root_body_visitor);
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_MAIN);
    if (FLAG_parallel_marking) {
      DCHECK(FLAG_concurrent_marking);
      heap_->concurrent_marking()->RescheduleTasksIfNeeded();
    }
    ProcessMarkingWorklist();

    FinishConcurrentMarking();
    ProcessMarkingWorklist();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WEAK_CLOSURE);

    DCHECK(marking_worklist()->IsEmpty());

    // The objects reachable from the roots are marked, yet unreachable
    // objects are unmarked.  Mark objects reachable due to host
    // application specific logic or through Harmony weak maps.
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_EPHEMERAL);
      ProcessEphemeralMarking(false);
      DCHECK(marking_worklist()->IsEmpty());
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
      ProcessMarkingWorklist();
    }

    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_WEAK_ROOTS);
      heap()->isolate()->global_handles()->IterateWeakRootsForFinalizers(
          &root_visitor);
      ProcessMarkingWorklist();
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
      DCHECK(marking_worklist()->IsEmpty());
    }

    {
      heap()->isolate()->global_handles()->IterateWeakRootsForPhantomHandles(
          &IsUnmarkedHeapObject);
    }
  }

  if (was_marked_incrementally_) {
    heap()->incremental_marking()->Deactivate();
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
    MarkCompactWeakObjectRetainer mark_compact_object_retainer(
        non_atomic_marking_state());
    heap()->ProcessAllWeakReferences(&mark_compact_object_retainer);
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_MAPS);
    // ClearFullMapTransitions must be called before WeakCells are cleared.
    ClearFullMapTransitions();
  }
  DependentCode* dependent_code_list;
  ClearWeakCellsAndSimpleMapTransitions(&dependent_code_list);
  MarkDependentCodeForDeoptimization(dependent_code_list);

  ClearWeakCollections();

  DCHECK(weak_objects_.weak_cells.IsGlobalEmpty());
  DCHECK(weak_objects_.transition_arrays.IsGlobalEmpty());
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

void MarkCompactCollector::ClearSimpleMapTransition(
    WeakCell* potential_transition, Map* dead_target) {
  DCHECK(non_atomic_marking_state()->IsWhite(dead_target));
  Object* potential_parent = dead_target->constructor_or_backpointer();
  if (potential_parent->IsMap()) {
    Map* parent = Map::cast(potential_parent);
    DisallowHeapAllocation no_gc_obviously;
    if (non_atomic_marking_state()->IsBlackOrGrey(parent) &&
        TransitionsAccessor(parent, &no_gc_obviously)
            .HasSimpleTransitionTo(potential_transition)) {
      ClearSimpleMapTransition(parent, dead_target);
    }
  }
}

void MarkCompactCollector::ClearSimpleMapTransition(Map* map,
                                                    Map* dead_target) {
  DCHECK(!map->is_prototype_map());
  DCHECK(!dead_target->is_prototype_map());
  // Clear the useless weak cell pointer, and take ownership of the descriptor
  // array.
  map->set_raw_transitions(Smi::kZero);
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  DescriptorArray* descriptors = map->instance_descriptors();
  if (descriptors == dead_target->instance_descriptors() &&
      number_of_own_descriptors > 0) {
    TrimDescriptorArray(map, descriptors);
    DCHECK(descriptors->number_of_descriptors() == number_of_own_descriptors);
    map->set_owns_descriptors(true);
  }
}

void MarkCompactCollector::ClearFullMapTransitions() {
  TransitionArray* array;
  while (weak_objects_.transition_arrays.Pop(kMainThread, &array)) {
    int num_transitions = array->number_of_entries();
    if (num_transitions > 0) {
      Map* map = array->GetTarget(0);
      DCHECK_NOT_NULL(map);  // WeakCells aren't cleared yet.
      Map* parent = Map::cast(map->constructor_or_backpointer());
      bool parent_is_alive = non_atomic_marking_state()->IsBlackOrGrey(parent);
      DescriptorArray* descriptors =
          parent_is_alive ? parent->instance_descriptors() : nullptr;
      bool descriptors_owner_died =
          CompactTransitionArray(parent, array, descriptors);
      if (descriptors_owner_died) {
        TrimDescriptorArray(parent, descriptors);
      }
    }
  }
}

bool MarkCompactCollector::CompactTransitionArray(
    Map* map, TransitionArray* transitions, DescriptorArray* descriptors) {
  DCHECK(!map->is_prototype_map());
  int num_transitions = transitions->number_of_entries();
  bool descriptors_owner_died = false;
  int transition_index = 0;
  // Compact all live transitions to the left.
  for (int i = 0; i < num_transitions; ++i) {
    Map* target = transitions->GetTarget(i);
    DCHECK_EQ(target->constructor_or_backpointer(), map);
    if (non_atomic_marking_state()->IsWhite(target)) {
      if (descriptors != nullptr &&
          target->instance_descriptors() == descriptors) {
        DCHECK(!target->is_prototype_map());
        descriptors_owner_died = true;
      }
    } else {
      if (i != transition_index) {
        Name* key = transitions->GetKey(i);
        transitions->SetKey(transition_index, key);
        Object** key_slot = transitions->GetKeySlot(transition_index);
        RecordSlot(transitions, key_slot, key);
        Object* raw_target = transitions->GetRawTarget(i);
        transitions->SetTarget(transition_index, raw_target);
        Object** target_slot = transitions->GetTargetSlot(transition_index);
        RecordSlot(transitions, target_slot, raw_target);
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
  int trim = transitions->Capacity() - transition_index;
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

    TrimEnumCache(map, descriptors);
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
    live_enum = map->NumberOfEnumerableProperties();
  }
  if (live_enum == 0) return descriptors->ClearEnumCache();
  EnumCache* enum_cache = descriptors->GetEnumCache();

  FixedArray* keys = enum_cache->keys();
  int to_trim = keys->length() - live_enum;
  if (to_trim <= 0) return;
  heap_->RightTrimFixedArray(keys, to_trim);

  FixedArray* indices = enum_cache->indices();
  to_trim = indices->length() - live_enum;
  if (to_trim <= 0) return;
  heap_->RightTrimFixedArray(indices, to_trim);
}


void MarkCompactCollector::ProcessWeakCollections() {
  MarkCompactMarkingVisitor visitor(this, marking_state());
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::kZero) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    DCHECK(non_atomic_marking_state()->IsBlackOrGrey(weak_collection));
    if (weak_collection->table()->IsHashTable()) {
      ObjectHashTable* table = ObjectHashTable::cast(weak_collection->table());
      for (int i = 0; i < table->Capacity(); i++) {
        HeapObject* heap_object = HeapObject::cast(table->KeyAt(i));
        if (non_atomic_marking_state()->IsBlackOrGrey(heap_object)) {
          Object** key_slot =
              table->RawFieldOfElementAt(ObjectHashTable::EntryToIndex(i));
          RecordSlot(table, key_slot, *key_slot);
          Object** value_slot =
              table->RawFieldOfElementAt(ObjectHashTable::EntryToValueIndex(i));
          if (V8_UNLIKELY(FLAG_track_retaining_path) &&
              (*value_slot)->IsHeapObject()) {
            heap()->AddEphemeralRetainer(heap_object,
                                         HeapObject::cast(*value_slot));
          }
          visitor.VisitPointer(table, value_slot);
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
    DCHECK(non_atomic_marking_state()->IsBlackOrGrey(weak_collection));
    if (weak_collection->table()->IsHashTable()) {
      ObjectHashTable* table = ObjectHashTable::cast(weak_collection->table());
      for (int i = 0; i < table->Capacity(); i++) {
        HeapObject* key = HeapObject::cast(table->KeyAt(i));
        if (!non_atomic_marking_state()->IsBlackOrGrey(key)) {
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

void MarkCompactCollector::ClearWeakCellsAndSimpleMapTransitions(
    DependentCode** dependent_code_list) {
  Heap* heap = this->heap();
  TRACE_GC(heap->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_CELLS);
  DependentCode* dependent_code_head =
      DependentCode::cast(heap->empty_fixed_array());
  WeakCell* weak_cell;
  while (weak_objects_.weak_cells.Pop(kMainThread, &weak_cell)) {
    // We do not insert cleared weak cells into the list, so the value
    // cannot be a Smi here.
    HeapObject* value = HeapObject::cast(weak_cell->value());
    if (!non_atomic_marking_state()->IsBlackOrGrey(value)) {
      // Cells for new-space objects embedded in optimized code are wrapped in
      // WeakCell and put into Heap::weak_object_to_code_table.
      // Such cells do not have any strong references but we want to keep them
      // alive as long as the cell value is alive.
      // TODO(ulan): remove this once we remove Heap::weak_object_to_code_table.
      if (value->IsCell()) {
        Object* cell_value = Cell::cast(value)->value();
        if (cell_value->IsHeapObject() &&
            non_atomic_marking_state()->IsBlackOrGrey(
                HeapObject::cast(cell_value))) {
          // Resurrect the cell.
          non_atomic_marking_state()->WhiteToBlack(value);
          Object** slot = HeapObject::RawField(value, Cell::kValueOffset);
          RecordSlot(value, slot, *slot);
          slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
          RecordSlot(weak_cell, slot, *slot);
        } else {
          weak_cell->clear();
        }
      } else if (value->IsMap()) {
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
        ClearSimpleMapTransition(weak_cell, map);
        weak_cell->clear();
      } else {
        // All other objects.
        weak_cell->clear();
      }
    } else {
      // The value of the weak cell is alive.
      Object** slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
      RecordSlot(weak_cell, slot, *slot);
    }
  }
  *dependent_code_list = dependent_code_head;
}

void MarkCompactCollector::AbortWeakObjects() {
  weak_objects_.weak_cells.Clear();
  weak_objects_.transition_arrays.Clear();
}

void MarkCompactCollector::RecordRelocSlot(Code* host, RelocInfo* rinfo,
                                           Object* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  Page* source_page = Page::FromAddress(reinterpret_cast<Address>(host));
  if (target_page->IsEvacuationCandidate() &&
      (rinfo->host() == nullptr ||
       !source_page->ShouldSkipEvacuationSlotRecording())) {
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

template <AccessMode access_mode>
static inline SlotCallbackResult UpdateSlot(Object** slot) {
  Object* obj = *slot;
  if (obj->IsHeapObject()) {
    HeapObject* heap_obj = HeapObject::cast(obj);
    MapWord map_word = heap_obj->map_word();
    if (map_word.IsForwardingAddress()) {
      DCHECK(heap_obj->GetHeap()->InFromSpace(heap_obj) ||
             MarkCompactCollector::IsOnEvacuationCandidate(heap_obj) ||
             Page::FromAddress(heap_obj->address())
                 ->IsFlagSet(Page::COMPACTION_WAS_ABORTED));
      HeapObject* target = map_word.ToForwardingAddress();
      if (access_mode == AccessMode::NON_ATOMIC) {
        *slot = target;
      } else {
        base::AsAtomicPointer::Release_CompareAndSwap(slot, obj, target);
      }
      DCHECK(!heap_obj->GetHeap()->InFromSpace(target));
      DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(target));
    }
  }
  // OLD_TO_OLD slots are always removed after updating.
  return REMOVE_SLOT;
}

// Visitor for updating root pointers and to-space pointers.
// It does not expect to encounter pointers to dead objects.
// TODO(ulan): Remove code object specific functions. This visitor
// nevers visits code objects.
class PointersUpdatingVisitor : public ObjectVisitor, public RootVisitor {
 public:
  void VisitPointer(HeapObject* host, Object** p) override {
    UpdateSlotInternal(p);
  }

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) UpdateSlotInternal(p);
  }

  void VisitRootPointer(Root root, Object** p) override {
    UpdateSlotInternal(p);
  }

  void VisitRootPointers(Root root, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) UpdateSlotInternal(p);
  }

  void VisitEmbeddedPointer(Code* host, RelocInfo* rinfo) override {
    UpdateTypedSlotHelper::UpdateEmbeddedPointer(rinfo, UpdateSlotInternal);
  }

  void VisitCodeTarget(Code* host, RelocInfo* rinfo) override {
    UpdateTypedSlotHelper::UpdateCodeTarget(rinfo, UpdateSlotInternal);
  }

 private:
  static inline SlotCallbackResult UpdateSlotInternal(Object** slot) {
    return UpdateSlot<AccessMode::NON_ATOMIC>(slot);
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
    new_space_evacuation_pages_.push_back(p);
  }
  new_space->Flip();
  new_space->ResetAllocationInfo();

  // Old space.
  DCHECK(old_space_evacuation_pages_.empty());
  old_space_evacuation_pages_ = std::move(evacuation_candidates_);
  evacuation_candidates_.clear();
  DCHECK(evacuation_candidates_.empty());
}

void MarkCompactCollector::EvacuateEpilogue() {
  aborted_evacuation_candidates_.clear();
  // New space.
  heap()->new_space()->set_age_mark(heap()->new_space()->top());
  // Deallocate unmarked large objects.
  heap()->lo_space()->FreeUnmarkedObjects();
  // Old space. Deallocate evacuated candidate pages.
  ReleaseEvacuationCandidates();
  // Give pages that are queued to be freed back to the OS.
  heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
#ifdef DEBUG
  // Old-to-old slot sets must be empty after evacuation.
  for (Page* p : *heap()->old_space()) {
    DCHECK_NULL((p->slot_set<OLD_TO_OLD, AccessMode::ATOMIC>()));
    DCHECK_NULL((p->typed_slot_set<OLD_TO_OLD, AccessMode::ATOMIC>()));
    DCHECK_NULL(p->invalidated_slots());
  }
#endif
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
        local_allocator_(heap_),
        local_pretenuring_feedback_(kInitialLocalPretenuringFeedbackCapacity),
        new_space_visitor_(heap_, &local_allocator_, record_visitor,
                           &local_pretenuring_feedback_),
        new_to_new_page_visitor_(heap_, record_visitor,
                                 &local_pretenuring_feedback_),
        new_to_old_page_visitor_(heap_, record_visitor,
                                 &local_pretenuring_feedback_),

        old_space_visitor_(heap_, &local_allocator_, record_visitor),
        duration_(0.0),
        bytes_compacted_(0) {}

  virtual ~Evacuator() {}

  void EvacuatePage(Page* page);

  void AddObserver(MigrationObserver* observer) {
    new_space_visitor_.AddObserver(observer);
    old_space_visitor_.AddObserver(observer);
  }

  // Merge back locally cached info sequentially. Note that this method needs
  // to be called from the main thread.
  inline void Finalize();

 protected:
  static const int kInitialLocalPretenuringFeedbackCapacity = 256;

  // |saved_live_bytes| returns the live bytes of the page that was processed.
  virtual void RawEvacuatePage(Page* page, intptr_t* saved_live_bytes) = 0;

  inline Heap* heap() { return heap_; }

  void ReportCompactionProgress(double duration, intptr_t bytes_compacted) {
    duration_ += duration;
    bytes_compacted_ += bytes_compacted;
  }

  Heap* heap_;

  // Locally cached collector data.
  LocalAllocator local_allocator_;
  Heap::PretenuringFeedbackMap local_pretenuring_feedback_;

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

void Evacuator::EvacuatePage(Page* page) {
  DCHECK(page->SweepingDone());
  intptr_t saved_live_bytes = 0;
  double evacuation_time = 0.0;
  {
    AlwaysAllocateScope always_allocate(heap()->isolate());
    TimedScope timed_scope(&evacuation_time);
    RawEvacuatePage(page, &saved_live_bytes);
  }
  ReportCompactionProgress(evacuation_time, saved_live_bytes);
  if (FLAG_trace_evacuation) {
    PrintIsolate(
        heap()->isolate(),
        "evacuation[%p]: page=%p new_space=%d "
        "page_evacuation=%d executable=%d contains_age_mark=%d "
        "live_bytes=%" V8PRIdPTR " time=%f success=%d\n",
        static_cast<void*>(this), static_cast<void*>(page), page->InNewSpace(),
        page->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION) ||
            page->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION),
        page->IsFlagSet(MemoryChunk::IS_EXECUTABLE),
        page->Contains(heap()->new_space()->age_mark()), saved_live_bytes,
        evacuation_time, page->IsFlagSet(Page::COMPACTION_WAS_ABORTED));
  }
}

void Evacuator::Finalize() {
  local_allocator_.Finalize();
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
  void RawEvacuatePage(Page* page, intptr_t* live_bytes) override;

  MarkCompactCollector* collector_;
};

void FullEvacuator::RawEvacuatePage(Page* page, intptr_t* live_bytes) {
  MarkCompactCollector::NonAtomicMarkingState* marking_state =
      collector_->non_atomic_marking_state();
  *live_bytes = marking_state->live_bytes(page);
  HeapObject* failed_object = nullptr;
  switch (ComputeEvacuationMode(page)) {
    case kObjectsNewToOld:
      LiveObjectVisitor::VisitBlackObjectsNoFail(
          page, marking_state, &new_space_visitor_,
          LiveObjectVisitor::kClearMarkbits);
      // ArrayBufferTracker will be updated during pointers updating.
      break;
    case kPageNewToOld:
      LiveObjectVisitor::VisitBlackObjectsNoFail(
          page, marking_state, &new_to_old_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      new_to_old_page_visitor_.account_moved_bytes(
          marking_state->live_bytes(page));
      // ArrayBufferTracker will be updated during sweeping.
      break;
    case kPageNewToNew:
      LiveObjectVisitor::VisitBlackObjectsNoFail(
          page, marking_state, &new_to_new_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      new_to_new_page_visitor_.account_moved_bytes(
          marking_state->live_bytes(page));
      // ArrayBufferTracker will be updated during sweeping.
      break;
    case kObjectsOldToOld: {
      const bool success = LiveObjectVisitor::VisitBlackObjects(
          page, marking_state, &old_space_visitor_,
          LiveObjectVisitor::kClearMarkbits, &failed_object);
      if (!success) {
        // Aborted compaction page. Actual processing happens on the main
        // thread for simplicity reasons.
        collector_->ReportAbortedEvacuationCandidate(failed_object, page);
      } else {
        // ArrayBufferTracker will be updated during pointers updating.
      }
      break;
    }
  }
}

class YoungGenerationEvacuator : public Evacuator {
 public:
  YoungGenerationEvacuator(MinorMarkCompactCollector* collector,
                           RecordMigratedSlotVisitor* record_visitor)
      : Evacuator(collector->heap(), record_visitor), collector_(collector) {}

 protected:
  void RawEvacuatePage(Page* page, intptr_t* live_bytes) override;

  MinorMarkCompactCollector* collector_;
};

void YoungGenerationEvacuator::RawEvacuatePage(Page* page,
                                               intptr_t* live_bytes) {
  MinorMarkCompactCollector::NonAtomicMarkingState* marking_state =
      collector_->non_atomic_marking_state();
  *live_bytes = marking_state->live_bytes(page);
  switch (ComputeEvacuationMode(page)) {
    case kObjectsNewToOld:
      LiveObjectVisitor::VisitGreyObjectsNoFail(
          page, marking_state, &new_space_visitor_,
          LiveObjectVisitor::kClearMarkbits);
      // ArrayBufferTracker will be updated during pointers updating.
      break;
    case kPageNewToOld:
      LiveObjectVisitor::VisitGreyObjectsNoFail(
          page, marking_state, &new_to_old_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      new_to_old_page_visitor_.account_moved_bytes(
          marking_state->live_bytes(page));
      // TODO(mlippautz): If cleaning array buffers is too slow here we can
      // delay it until the next GC.
      ArrayBufferTracker::FreeDead(page, marking_state);
      if (heap()->ShouldZapGarbage()) {
        collector_->MakeIterable(page, MarkingTreatmentMode::KEEP,
                                 ZAP_FREE_SPACE);
      } else if (heap()->incremental_marking()->IsMarking()) {
        // When incremental marking is on, we need to clear the mark bits of
        // the full collector. We cannot yet discard the young generation mark
        // bits as they are still relevant for pointers updating.
        collector_->MakeIterable(page, MarkingTreatmentMode::KEEP,
                                 IGNORE_FREE_SPACE);
      }
      break;
    case kPageNewToNew:
      LiveObjectVisitor::VisitGreyObjectsNoFail(
          page, marking_state, &new_to_new_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      new_to_new_page_visitor_.account_moved_bytes(
          marking_state->live_bytes(page));
      // TODO(mlippautz): If cleaning array buffers is too slow here we can
      // delay it until the next GC.
      ArrayBufferTracker::FreeDead(page, marking_state);
      if (heap()->ShouldZapGarbage()) {
        collector_->MakeIterable(page, MarkingTreatmentMode::KEEP,
                                 ZAP_FREE_SPACE);
      } else if (heap()->incremental_marking()->IsMarking()) {
        // When incremental marking is on, we need to clear the mark bits of
        // the full collector. We cannot yet discard the young generation mark
        // bits as they are still relevant for pointers updating.
        collector_->MakeIterable(page, MarkingTreatmentMode::KEEP,
                                 IGNORE_FREE_SPACE);
      }
      break;
    case kObjectsOldToOld:
      UNREACHABLE();
      break;
  }
}

class PageEvacuationItem : public ItemParallelJob::Item {
 public:
  explicit PageEvacuationItem(Page* page) : page_(page) {}
  virtual ~PageEvacuationItem() {}
  Page* page() const { return page_; }

 private:
  Page* page_;
};

class PageEvacuationTask : public ItemParallelJob::Task {
 public:
  PageEvacuationTask(Isolate* isolate, Evacuator* evacuator)
      : ItemParallelJob::Task(isolate), evacuator_(evacuator) {}

  void RunInParallel() override {
    PageEvacuationItem* item = nullptr;
    while ((item = GetItem<PageEvacuationItem>()) != nullptr) {
      evacuator_->EvacuatePage(item->page());
      item->MarkFinished();
    }
  };

 private:
  Evacuator* evacuator_;
};

template <class Evacuator, class Collector>
void MarkCompactCollectorBase::CreateAndExecuteEvacuationTasks(
    Collector* collector, ItemParallelJob* job,
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
      NumberOfParallelCompactionTasks(job->NumberOfItems());
  Evacuator** evacuators = new Evacuator*[wanted_num_tasks];
  for (int i = 0; i < wanted_num_tasks; i++) {
    evacuators[i] = new Evacuator(collector, record_visitor);
    if (profiling) evacuators[i]->AddObserver(&profiling_observer);
    if (migration_observer != nullptr)
      evacuators[i]->AddObserver(migration_observer);
    job->AddTask(new PageEvacuationTask(heap()->isolate(), evacuators[i]));
  }
  job->Run();
  for (int i = 0; i < wanted_num_tasks; i++) {
    evacuators[i]->Finalize();
    delete evacuators[i];
  }
  delete[] evacuators;

  if (FLAG_trace_evacuation) {
    PrintIsolate(isolate(),
                 "%8.0f ms: evacuation-summary: parallel=%s pages=%d "
                 "wanted_tasks=%d tasks=%d cores=%" PRIuS
                 " live_bytes=%" V8PRIdPTR " compaction_speed=%.f\n",
                 isolate()->time_millis_since_init(),
                 FLAG_parallel_compaction ? "yes" : "no", job->NumberOfItems(),
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
  ItemParallelJob evacuation_job(isolate()->cancelable_task_manager(),
                                 &page_parallel_job_semaphore_);
  intptr_t live_bytes = 0;

  for (Page* page : old_space_evacuation_pages_) {
    live_bytes += non_atomic_marking_state()->live_bytes(page);
    evacuation_job.AddItem(new PageEvacuationItem(page));
  }

  for (Page* page : new_space_evacuation_pages_) {
    intptr_t live_bytes_on_page = non_atomic_marking_state()->live_bytes(page);
    if (live_bytes_on_page == 0 && !page->contains_array_buffers()) continue;
    live_bytes += live_bytes_on_page;
    if (ShouldMovePage(page, live_bytes_on_page)) {
      if (page->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK)) {
        EvacuateNewSpacePageVisitor<NEW_TO_OLD>::Move(page);
        DCHECK_EQ(heap()->old_space(), page->owner());
        // The move added page->allocated_bytes to the old space, but we are
        // going to sweep the page and add page->live_byte_count.
        heap()->old_space()->DecreaseAllocatedBytes(page->allocated_bytes(),
                                                    page);
      } else {
        EvacuateNewSpacePageVisitor<NEW_TO_NEW>::Move(page);
      }
    }
    evacuation_job.AddItem(new PageEvacuationItem(page));
  }
  if (evacuation_job.NumberOfItems() == 0) return;

  RecordMigratedSlotVisitor record_visitor(this);
  CreateAndExecuteEvacuationTasks<FullEvacuator>(
      this, &evacuation_job, &record_visitor, nullptr, live_bytes);
  PostProcessEvacuationCandidates();
}

void MinorMarkCompactCollector::EvacuatePagesInParallel() {
  ItemParallelJob evacuation_job(isolate()->cancelable_task_manager(),
                                 &page_parallel_job_semaphore_);
  intptr_t live_bytes = 0;

  for (Page* page : new_space_evacuation_pages_) {
    intptr_t live_bytes_on_page = non_atomic_marking_state()->live_bytes(page);
    if (live_bytes_on_page == 0 && !page->contains_array_buffers()) continue;
    live_bytes += live_bytes_on_page;
    if (ShouldMovePage(page, live_bytes_on_page)) {
      if (page->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK)) {
        EvacuateNewSpacePageVisitor<NEW_TO_OLD>::Move(page);
      } else {
        EvacuateNewSpacePageVisitor<NEW_TO_NEW>::Move(page);
      }
    }
    evacuation_job.AddItem(new PageEvacuationItem(page));
  }
  if (evacuation_job.NumberOfItems() == 0) return;

  YoungGenerationMigrationObserver observer(heap(),
                                            heap()->mark_compact_collector());
  YoungGenerationRecordMigratedSlotVisitor record_visitor(
      heap()->mark_compact_collector());
  CreateAndExecuteEvacuationTasks<YoungGenerationEvacuator>(
      this, &evacuation_job, &record_visitor, &observer, live_bytes);
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

// Return true if the given code is deoptimized or will be deoptimized.
bool MarkCompactCollector::WillBeDeoptimized(Code* code) {
  return code->is_optimized_code() && code->marked_for_deoptimization();
}

void MarkCompactCollector::RecordLiveSlotsOnPage(Page* page) {
  EvacuateRecordOnlyVisitor visitor(heap());
  LiveObjectVisitor::VisitBlackObjectsNoFail(page, non_atomic_marking_state(),
                                             &visitor,
                                             LiveObjectVisitor::kKeepMarking);
}

template <class Visitor, typename MarkingState>
bool LiveObjectVisitor::VisitBlackObjects(MemoryChunk* chunk,
                                          MarkingState* marking_state,
                                          Visitor* visitor,
                                          IterationMode iteration_mode,
                                          HeapObject** failed_object) {
  for (auto object_and_size :
       LiveObjectRange<kBlackObjects>(chunk, marking_state->bitmap(chunk))) {
    HeapObject* const object = object_and_size.first;
    if (!visitor->Visit(object, object_and_size.second)) {
      if (iteration_mode == kClearMarkbits) {
        marking_state->bitmap(chunk)->ClearRange(
            chunk->AddressToMarkbitIndex(chunk->area_start()),
            chunk->AddressToMarkbitIndex(object->address()));
        *failed_object = object;
      }
      return false;
    }
  }
  if (iteration_mode == kClearMarkbits) {
    marking_state->ClearLiveness(chunk);
  }
  return true;
}

template <class Visitor, typename MarkingState>
void LiveObjectVisitor::VisitBlackObjectsNoFail(MemoryChunk* chunk,
                                                MarkingState* marking_state,
                                                Visitor* visitor,
                                                IterationMode iteration_mode) {
  for (auto object_and_size :
       LiveObjectRange<kBlackObjects>(chunk, marking_state->bitmap(chunk))) {
    HeapObject* const object = object_and_size.first;
    DCHECK(marking_state->IsBlack(object));
    const bool success = visitor->Visit(object, object_and_size.second);
    USE(success);
    DCHECK(success);
  }
  if (iteration_mode == kClearMarkbits) {
    marking_state->ClearLiveness(chunk);
  }
}

template <class Visitor, typename MarkingState>
void LiveObjectVisitor::VisitGreyObjectsNoFail(MemoryChunk* chunk,
                                               MarkingState* marking_state,
                                               Visitor* visitor,
                                               IterationMode iteration_mode) {
  for (auto object_and_size :
       LiveObjectRange<kGreyObjects>(chunk, marking_state->bitmap(chunk))) {
    HeapObject* const object = object_and_size.first;
    DCHECK(marking_state->IsGrey(object));
    const bool success = visitor->Visit(object, object_and_size.second);
    USE(success);
    DCHECK(success);
  }
  if (iteration_mode == kClearMarkbits) {
    marking_state->ClearLiveness(chunk);
  }
}

template <typename MarkingState>
void LiveObjectVisitor::RecomputeLiveBytes(MemoryChunk* chunk,
                                           MarkingState* marking_state) {
  int new_live_size = 0;
  for (auto object_and_size :
       LiveObjectRange<kAllLiveObjects>(chunk, marking_state->bitmap(chunk))) {
    new_live_size += object_and_size.second;
  }
  marking_state->SetLiveBytes(chunk, new_live_size);
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
        sweeper()->AddPage(p->owner()->identity(), p, Sweeper::REGULAR);
      } else if (p->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION)) {
        p->ClearFlag(Page::PAGE_NEW_OLD_PROMOTION);
        p->ForAllFreeListCategories(
            [](FreeListCategory* category) { DCHECK(!category->is_linked()); });
        sweeper()->AddPage(p->owner()->identity(), p, Sweeper::REGULAR);
      }
    }
    new_space_evacuation_pages_.clear();

    for (Page* p : old_space_evacuation_pages_) {
      // Important: skip list should be cleared only after roots were updated
      // because root iteration traverses the stack and might have to find
      // code objects from non-updated pc pointing into evacuation candidate.
      SkipList* list = p->skip_list();
      if (list != nullptr) list->Clear();
      if (p->IsFlagSet(Page::COMPACTION_WAS_ABORTED)) {
        sweeper()->AddPage(p->owner()->identity(), p, Sweeper::REGULAR);
        p->ClearFlag(Page::COMPACTION_WAS_ABORTED);
      }
    }
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_EPILOGUE);
    EvacuateEpilogue();
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && !sweeper()->sweeping_in_progress()) {
    FullEvacuationVerifier verifier(heap());
    verifier.Run();
  }
#endif
}

class UpdatingItem : public ItemParallelJob::Item {
 public:
  virtual ~UpdatingItem() {}
  virtual void Process() = 0;
};

class PointersUpdatingTask : public ItemParallelJob::Task {
 public:
  explicit PointersUpdatingTask(Isolate* isolate)
      : ItemParallelJob::Task(isolate) {}

  void RunInParallel() override {
    UpdatingItem* item = nullptr;
    while ((item = GetItem<UpdatingItem>()) != nullptr) {
      item->Process();
      item->MarkFinished();
    }
  };
};

template <typename MarkingState>
class ToSpaceUpdatingItem : public UpdatingItem {
 public:
  explicit ToSpaceUpdatingItem(MemoryChunk* chunk, Address start, Address end,
                               MarkingState* marking_state)
      : chunk_(chunk),
        start_(start),
        end_(end),
        marking_state_(marking_state) {}
  virtual ~ToSpaceUpdatingItem() {}

  void Process() override {
    if (chunk_->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION)) {
      // New->new promoted pages contain garbage so they require iteration using
      // markbits.
      ProcessVisitLive();
    } else {
      ProcessVisitAll();
    }
  }

 private:
  void ProcessVisitAll() {
    PointersUpdatingVisitor visitor;
    for (Address cur = start_; cur < end_;) {
      HeapObject* object = HeapObject::FromAddress(cur);
      Map* map = object->map();
      int size = object->SizeFromMap(map);
      object->IterateBody(map->instance_type(), size, &visitor);
      cur += size;
    }
  }

  void ProcessVisitLive() {
    // For young generation evacuations we want to visit grey objects, for
    // full MC, we need to visit black objects.
    PointersUpdatingVisitor visitor;
    for (auto object_and_size : LiveObjectRange<kAllLiveObjects>(
             chunk_, marking_state_->bitmap(chunk_))) {
      object_and_size.first->IterateBodyFast(&visitor);
    }
  }

  MemoryChunk* chunk_;
  Address start_;
  Address end_;
  MarkingState* marking_state_;
};

template <typename MarkingState>
class RememberedSetUpdatingItem : public UpdatingItem {
 public:
  explicit RememberedSetUpdatingItem(Heap* heap, MarkingState* marking_state,
                                     MemoryChunk* chunk,
                                     RememberedSetUpdatingMode updating_mode)
      : heap_(heap),
        marking_state_(marking_state),
        chunk_(chunk),
        updating_mode_(updating_mode) {}
  virtual ~RememberedSetUpdatingItem() {}

  void Process() override {
    base::LockGuard<base::Mutex> guard(chunk_->mutex());
    UpdateUntypedPointers();
    UpdateTypedPointers();
  }

 private:
  template <AccessMode access_mode>
  inline SlotCallbackResult CheckAndUpdateOldToNewSlot(Address slot_address) {
    Object** slot = reinterpret_cast<Object**>(slot_address);
    if (heap_->InFromSpace(*slot)) {
      HeapObject* heap_object = reinterpret_cast<HeapObject*>(*slot);
      DCHECK(heap_object->IsHeapObject());
      MapWord map_word = heap_object->map_word();
      if (map_word.IsForwardingAddress()) {
        if (access_mode == AccessMode::ATOMIC) {
          HeapObject** heap_obj_slot = reinterpret_cast<HeapObject**>(slot);
          base::AsAtomicPointer::Relaxed_Store(heap_obj_slot,
                                               map_word.ToForwardingAddress());
        } else {
          *slot = map_word.ToForwardingAddress();
        }
      }
      // If the object was in from space before and is after executing the
      // callback in to space, the object is still live.
      // Unfortunately, we do not know about the slot. It could be in a
      // just freed free space object.
      if (heap_->InToSpace(*slot)) {
        return KEEP_SLOT;
      }
    } else if (heap_->InToSpace(*slot)) {
      // Slots can point to "to" space if the page has been moved, or if the
      // slot has been recorded multiple times in the remembered set, or
      // if the slot was already updated during old->old updating.
      // In case the page has been moved, check markbits to determine liveness
      // of the slot. In the other case, the slot can just be kept.
      HeapObject* heap_object = reinterpret_cast<HeapObject*>(*slot);
      if (Page::FromAddress(heap_object->address())
              ->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION)) {
        // IsBlackOrGrey is required because objects are marked as grey for
        // the young generation collector while they are black for the full
        // MC.);
        if (marking_state_->IsBlackOrGrey(heap_object)) {
          return KEEP_SLOT;
        } else {
          return REMOVE_SLOT;
        }
      }
      return KEEP_SLOT;
    } else {
      DCHECK(!heap_->InNewSpace(*slot));
    }
    return REMOVE_SLOT;
  }

  void UpdateUntypedPointers() {
    if (chunk_->slot_set<OLD_TO_NEW, AccessMode::NON_ATOMIC>() != nullptr) {
        RememberedSet<OLD_TO_NEW>::Iterate(
            chunk_,
            [this](Address slot) {
              return CheckAndUpdateOldToNewSlot<AccessMode::NON_ATOMIC>(slot);
            },
            SlotSet::PREFREE_EMPTY_BUCKETS);
    }
    if ((updating_mode_ == RememberedSetUpdatingMode::ALL) &&
        (chunk_->slot_set<OLD_TO_OLD, AccessMode::NON_ATOMIC>() != nullptr)) {
      InvalidatedSlotsFilter filter(chunk_);
      RememberedSet<OLD_TO_OLD>::Iterate(
          chunk_,
          [&filter](Address slot) {
            if (!filter.IsValid(slot)) return REMOVE_SLOT;
            return UpdateSlot<AccessMode::NON_ATOMIC>(
                reinterpret_cast<Object**>(slot));
          },
          SlotSet::PREFREE_EMPTY_BUCKETS);
    }
    if ((updating_mode_ == RememberedSetUpdatingMode::ALL) &&
        chunk_->invalidated_slots() != nullptr) {
#ifdef DEBUG
      for (auto object_size : *chunk_->invalidated_slots()) {
        HeapObject* object = object_size.first;
        int size = object_size.second;
        DCHECK_LE(object->SizeFromMap(object->map()), size);
      }
#endif
      // The invalidated slots are not needed after old-to-old slots were
      // processsed.
      chunk_->ReleaseInvalidatedSlots();
    }
  }

  void UpdateTypedPointers() {
    Isolate* isolate = heap_->isolate();
    if (chunk_->typed_slot_set<OLD_TO_NEW, AccessMode::NON_ATOMIC>() !=
        nullptr) {
      CHECK_NE(chunk_->owner(), heap_->map_space());
      RememberedSet<OLD_TO_NEW>::IterateTyped(
          chunk_,
          [isolate, this](SlotType slot_type, Address host_addr, Address slot) {
            return UpdateTypedSlotHelper::UpdateTypedSlot(
                isolate, slot_type, slot, [this](Object** slot) {
                  return CheckAndUpdateOldToNewSlot<AccessMode::NON_ATOMIC>(
                      reinterpret_cast<Address>(slot));
                });
          });
    }
    if ((updating_mode_ == RememberedSetUpdatingMode::ALL) &&
        (chunk_->typed_slot_set<OLD_TO_OLD, AccessMode::NON_ATOMIC>() !=
         nullptr)) {
      CHECK_NE(chunk_->owner(), heap_->map_space());
      RememberedSet<OLD_TO_OLD>::IterateTyped(
          chunk_,
          [isolate](SlotType slot_type, Address host_addr, Address slot) {
            return UpdateTypedSlotHelper::UpdateTypedSlot(
                isolate, slot_type, slot, UpdateSlot<AccessMode::NON_ATOMIC>);
          });
    }
  }

  Heap* heap_;
  MarkingState* marking_state_;
  MemoryChunk* chunk_;
  RememberedSetUpdatingMode updating_mode_;
};

UpdatingItem* MinorMarkCompactCollector::CreateToSpaceUpdatingItem(
    MemoryChunk* chunk, Address start, Address end) {
  return new ToSpaceUpdatingItem<NonAtomicMarkingState>(
      chunk, start, end, non_atomic_marking_state());
}

UpdatingItem* MarkCompactCollector::CreateToSpaceUpdatingItem(
    MemoryChunk* chunk, Address start, Address end) {
  return new ToSpaceUpdatingItem<NonAtomicMarkingState>(
      chunk, start, end, non_atomic_marking_state());
}

UpdatingItem* MinorMarkCompactCollector::CreateRememberedSetUpdatingItem(
    MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) {
  return new RememberedSetUpdatingItem<NonAtomicMarkingState>(
      heap(), non_atomic_marking_state(), chunk, updating_mode);
}

UpdatingItem* MarkCompactCollector::CreateRememberedSetUpdatingItem(
    MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) {
  return new RememberedSetUpdatingItem<NonAtomicMarkingState>(
      heap(), non_atomic_marking_state(), chunk, updating_mode);
}

class GlobalHandlesUpdatingItem : public UpdatingItem {
 public:
  GlobalHandlesUpdatingItem(GlobalHandles* global_handles, size_t start,
                            size_t end)
      : global_handles_(global_handles), start_(start), end_(end) {}
  virtual ~GlobalHandlesUpdatingItem() {}

  void Process() override {
    PointersUpdatingVisitor updating_visitor;
    global_handles_->IterateNewSpaceRoots(&updating_visitor, start_, end_);
  }

 private:
  GlobalHandles* global_handles_;
  size_t start_;
  size_t end_;
};

// Update array buffers on a page that has been evacuated by copying objects.
// Target page exclusivity in old space is guaranteed by the fact that
// evacuation tasks either (a) retrieved a fresh page, or (b) retrieved all
// free list items of a given page. For new space the tracker will update
// using a lock.
class ArrayBufferTrackerUpdatingItem : public UpdatingItem {
 public:
  enum EvacuationState { kRegular, kAborted };

  explicit ArrayBufferTrackerUpdatingItem(Page* page, EvacuationState state)
      : page_(page), state_(state) {}
  virtual ~ArrayBufferTrackerUpdatingItem() {}

  void Process() override {
    switch (state_) {
      case EvacuationState::kRegular:
        ArrayBufferTracker::ProcessBuffers(
            page_, ArrayBufferTracker::kUpdateForwardedRemoveOthers);
        break;
      case EvacuationState::kAborted:
        ArrayBufferTracker::ProcessBuffers(
            page_, ArrayBufferTracker::kUpdateForwardedKeepOthers);
        break;
    }
  }

 private:
  Page* const page_;
  const EvacuationState state_;
};

int MarkCompactCollectorBase::CollectToSpaceUpdatingItems(
    ItemParallelJob* job) {
  // Seed to space pages.
  const Address space_start = heap()->new_space()->bottom();
  const Address space_end = heap()->new_space()->top();
  int pages = 0;
  for (Page* page : PageRange(space_start, space_end)) {
    Address start =
        page->Contains(space_start) ? space_start : page->area_start();
    Address end = page->Contains(space_end) ? space_end : page->area_end();
    job->AddItem(CreateToSpaceUpdatingItem(page, start, end));
    pages++;
  }
  if (pages == 0) return 0;
  return NumberOfParallelToSpacePointerUpdateTasks(pages);
}

template <typename IterateableSpace>
int MarkCompactCollectorBase::CollectRememberedSetUpdatingItems(
    ItemParallelJob* job, IterateableSpace* space,
    RememberedSetUpdatingMode mode) {
  int pages = 0;
  for (MemoryChunk* chunk : *space) {
    const bool contains_old_to_old_slots =
        chunk->slot_set<OLD_TO_OLD>() != nullptr ||
        chunk->typed_slot_set<OLD_TO_OLD>() != nullptr;
    const bool contains_old_to_new_slots =
        chunk->slot_set<OLD_TO_NEW>() != nullptr ||
        chunk->typed_slot_set<OLD_TO_NEW>() != nullptr;
    const bool contains_invalidated_slots =
        chunk->invalidated_slots() != nullptr;
    if (!contains_old_to_new_slots && !contains_old_to_old_slots &&
        !contains_invalidated_slots)
      continue;
    if (mode == RememberedSetUpdatingMode::ALL || contains_old_to_new_slots ||
        contains_invalidated_slots) {
      job->AddItem(CreateRememberedSetUpdatingItem(chunk, mode));
      pages++;
    }
  }
  return pages;
}

int MinorMarkCompactCollector::CollectNewSpaceArrayBufferTrackerItems(
    ItemParallelJob* job) {
  int pages = 0;
  for (Page* p : new_space_evacuation_pages_) {
    if (Evacuator::ComputeEvacuationMode(p) == Evacuator::kObjectsNewToOld) {
      if (p->local_tracker() == nullptr) continue;

      pages++;
      job->AddItem(new ArrayBufferTrackerUpdatingItem(
          p, ArrayBufferTrackerUpdatingItem::kRegular));
    }
  }
  return pages;
}

int MarkCompactCollector::CollectNewSpaceArrayBufferTrackerItems(
    ItemParallelJob* job) {
  int pages = 0;
  for (Page* p : new_space_evacuation_pages_) {
    if (Evacuator::ComputeEvacuationMode(p) == Evacuator::kObjectsNewToOld) {
      if (p->local_tracker() == nullptr) continue;

      pages++;
      job->AddItem(new ArrayBufferTrackerUpdatingItem(
          p, ArrayBufferTrackerUpdatingItem::kRegular));
    }
  }
  return pages;
}

int MarkCompactCollector::CollectOldSpaceArrayBufferTrackerItems(
    ItemParallelJob* job) {
  int pages = 0;
  for (Page* p : old_space_evacuation_pages_) {
    if (Evacuator::ComputeEvacuationMode(p) == Evacuator::kObjectsOldToOld &&
        p->IsEvacuationCandidate()) {
      if (p->local_tracker() == nullptr) continue;

      pages++;
      job->AddItem(new ArrayBufferTrackerUpdatingItem(
          p, ArrayBufferTrackerUpdatingItem::kRegular));
    }
  }
  for (auto object_and_page : aborted_evacuation_candidates_) {
    Page* p = object_and_page.second;
    if (p->local_tracker() == nullptr) continue;

    pages++;
    job->AddItem(new ArrayBufferTrackerUpdatingItem(
        p, ArrayBufferTrackerUpdatingItem::kAborted));
  }
  return pages;
}

void MarkCompactCollector::UpdatePointersAfterEvacuation() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS);

  PointersUpdatingVisitor updating_visitor;

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_TO_NEW_ROOTS);
    heap_->IterateRoots(&updating_visitor, VISIT_ALL_IN_SWEEP_NEWSPACE);
  }

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_SLOTS_MAIN);
    ItemParallelJob updating_job(isolate()->cancelable_task_manager(),
                                 &page_parallel_job_semaphore_);

    int remembered_set_pages = 0;
    remembered_set_pages += CollectRememberedSetUpdatingItems(
        &updating_job, heap()->old_space(), RememberedSetUpdatingMode::ALL);
    remembered_set_pages += CollectRememberedSetUpdatingItems(
        &updating_job, heap()->code_space(), RememberedSetUpdatingMode::ALL);
    remembered_set_pages += CollectRememberedSetUpdatingItems(
        &updating_job, heap()->lo_space(), RememberedSetUpdatingMode::ALL);
    const int remembered_set_tasks =
        remembered_set_pages == 0
            ? 0
            : NumberOfParallelPointerUpdateTasks(remembered_set_pages,
                                                 old_to_new_slots_);
    const int to_space_tasks = CollectToSpaceUpdatingItems(&updating_job);
    const int num_tasks = Max(to_space_tasks, remembered_set_tasks);
    for (int i = 0; i < num_tasks; i++) {
      updating_job.AddTask(new PointersUpdatingTask(isolate()));
    }
    updating_job.Run();
  }

  {
    // - Update pointers in map space in a separate phase to avoid data races
    //   with Map->LayoutDescriptor edge.
    // - Update array buffer trackers in the second phase to have access to
    //   byte length which is potentially a HeapNumber.
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_SLOTS_MAP_SPACE);
    ItemParallelJob updating_job(isolate()->cancelable_task_manager(),
                                 &page_parallel_job_semaphore_);

    int array_buffer_pages = 0;
    array_buffer_pages += CollectNewSpaceArrayBufferTrackerItems(&updating_job);
    array_buffer_pages += CollectOldSpaceArrayBufferTrackerItems(&updating_job);

    int remembered_set_pages = 0;
    remembered_set_pages += CollectRememberedSetUpdatingItems(
        &updating_job, heap()->map_space(), RememberedSetUpdatingMode::ALL);
    const int remembered_set_tasks =
        remembered_set_pages == 0
            ? 0
            : NumberOfParallelPointerUpdateTasks(remembered_set_pages,
                                                 old_to_new_slots_);
    const int num_tasks = Max(array_buffer_pages, remembered_set_tasks);
    if (num_tasks > 0) {
      for (int i = 0; i < num_tasks; i++) {
        updating_job.AddTask(new PointersUpdatingTask(isolate()));
      }
      updating_job.Run();
      heap()->array_buffer_collector()->FreeAllocationsOnBackgroundThread();
    }
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
  ItemParallelJob updating_job(isolate()->cancelable_task_manager(),
                               &page_parallel_job_semaphore_);

  CollectNewSpaceArrayBufferTrackerItems(&updating_job);
  // Create batches of global handles.
  SeedGlobalHandles<GlobalHandlesUpdatingItem>(isolate()->global_handles(),
                                               &updating_job);
  const int to_space_tasks = CollectToSpaceUpdatingItems(&updating_job);
  int remembered_set_pages = 0;
  remembered_set_pages += CollectRememberedSetUpdatingItems(
      &updating_job, heap()->old_space(),
      RememberedSetUpdatingMode::OLD_TO_NEW_ONLY);
  remembered_set_pages += CollectRememberedSetUpdatingItems(
      &updating_job, heap()->code_space(),
      RememberedSetUpdatingMode::OLD_TO_NEW_ONLY);
  remembered_set_pages += CollectRememberedSetUpdatingItems(
      &updating_job, heap()->map_space(),
      RememberedSetUpdatingMode::OLD_TO_NEW_ONLY);
  remembered_set_pages += CollectRememberedSetUpdatingItems(
      &updating_job, heap()->lo_space(),
      RememberedSetUpdatingMode::OLD_TO_NEW_ONLY);
  const int remembered_set_tasks = NumberOfParallelPointerUpdateTasks(
      remembered_set_pages, old_to_new_slots_);
  const int num_tasks = Max(to_space_tasks, remembered_set_tasks);
  for (int i = 0; i < num_tasks; i++) {
    updating_job.AddTask(new PointersUpdatingTask(isolate()));
  }

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_TO_NEW_ROOTS);
    heap_->IterateRoots(&updating_visitor, VISIT_ALL_IN_MINOR_MC_UPDATE);
  }
  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_SLOTS);
    updating_job.Run();
    heap()->array_buffer_collector()->FreeAllocationsOnBackgroundThread();
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

void MarkCompactCollector::ReportAbortedEvacuationCandidate(
    HeapObject* failed_object, Page* page) {
  base::LockGuard<base::Mutex> guard(&mutex_);

  aborted_evacuation_candidates_.push_back(std::make_pair(failed_object, page));
}

void MarkCompactCollector::PostProcessEvacuationCandidates() {
  for (auto object_and_page : aborted_evacuation_candidates_) {
    HeapObject* failed_object = object_and_page.first;
    Page* page = object_and_page.second;
    page->SetFlag(Page::COMPACTION_WAS_ABORTED);
    // Aborted compaction page. We have to record slots here, since we
    // might not have recorded them in first place.

    // Remove outdated slots.
    RememberedSet<OLD_TO_NEW>::RemoveRange(page, page->address(),
                                           failed_object->address(),
                                           SlotSet::PREFREE_EMPTY_BUCKETS);
    RememberedSet<OLD_TO_NEW>::RemoveRangeTyped(page, page->address(),
                                                failed_object->address());
    // Recompute live bytes.
    LiveObjectVisitor::RecomputeLiveBytes(page, non_atomic_marking_state());
    // Re-record slots.
    EvacuateRecordOnlyVisitor record_visitor(heap());
    LiveObjectVisitor::VisitBlackObjectsNoFail(page, non_atomic_marking_state(),
                                               &record_visitor,
                                               LiveObjectVisitor::kKeepMarking);
    // Array buffers will be processed during pointer updating.
  }
  const int aborted_pages =
      static_cast<int>(aborted_evacuation_candidates_.size());
  int aborted_pages_verified = 0;
  for (Page* p : old_space_evacuation_pages_) {
    if (p->IsFlagSet(Page::COMPACTION_WAS_ABORTED)) {
      // After clearing the evacuation candidate flag the page is again in a
      // regular state.
      p->ClearEvacuationCandidate();
      aborted_pages_verified++;
    } else {
      DCHECK(p->IsEvacuationCandidate());
      DCHECK(p->SweepingDone());
      p->Unlink();
    }
  }
  DCHECK_EQ(aborted_pages_verified, aborted_pages);
  if (FLAG_trace_evacuation && (aborted_pages > 0)) {
    PrintIsolate(isolate(), "%8.0f ms: evacuation: aborted=%d\n",
                 isolate()->time_millis_since_init(), aborted_pages);
  }
}

void MarkCompactCollector::ReleaseEvacuationCandidates() {
  for (Page* p : old_space_evacuation_pages_) {
    if (!p->IsEvacuationCandidate()) continue;
    PagedSpace* space = static_cast<PagedSpace*>(p->owner());
    non_atomic_marking_state()->SetLiveBytes(p, 0);
    CHECK(p->SweepingDone());
    space->ReleasePage(p);
  }
  old_space_evacuation_pages_.clear();
  compacting_ = false;
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
      DCHECK(!evacuation_candidates_.empty());
      continue;
    }

    if (p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
      // We need to sweep the page to get it into an iterable state again. Note
      // that this adds unusable memory into the free list that is later on
      // (in the free list) dropped again. Since we only use the flag for
      // testing this is fine.
      p->concurrent_sweeping_state().SetValue(Page::kSweepingInProgress);
      sweeper()->RawSweep(p, Sweeper::IGNORE_FREE_LIST,
                          Heap::ShouldZapGarbage()
                              ? FreeSpaceTreatmentMode::ZAP_FREE_SPACE
                              : FreeSpaceTreatmentMode::IGNORE_FREE_SPACE);
      space->IncreaseAllocatedBytes(p->allocated_bytes(), p);
      continue;
    }

    // One unused page is kept, all further are released before sweeping them.
    if (non_atomic_marking_state()->live_bytes(p) == 0) {
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

    sweeper()->AddPage(space->identity(), p, Sweeper::REGULAR);
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
    sweeper()->StartSweeping();
  }
}

}  // namespace internal
}  // namespace v8

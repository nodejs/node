// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/mark-compact.h"

#include <unordered_map>

#include "src/base/utils/random-number-generator.h"
#include "src/codegen/compilation-cache.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/execution.h"
#include "src/execution/frames-inl.h"
#include "src/execution/vm-state-inl.h"
#include "src/handles/global-handles.h"
#include "src/heap/array-buffer-collector.h"
#include "src/heap/array-buffer-tracker-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/invalidated-slots-inl.h"
#include "src/heap/item-parallel-job.h"
#include "src/heap/local-allocator-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/sweeper.h"
#include "src/heap/worklist.h"
#include "src/ic/stub-cache.h"
#include "src/init/v8.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/foreign.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/slots-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/tasks/cancelable-task.h"
#include "src/utils/utils-inl.h"

namespace v8 {
namespace internal {

const char* Marking::kWhiteBitPattern = "00";
const char* Marking::kBlackBitPattern = "11";
const char* Marking::kGreyBitPattern = "10";
const char* Marking::kImpossibleBitPattern = "01";

// The following has to hold in order for {MarkingState::MarkBitFrom} to not
// produce invalid {kImpossibleBitPattern} in the marking bitmap by overlapping.
STATIC_ASSERT(Heap::kMinObjectSizeInTaggedWords >= 2);

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

  virtual ConcurrentBitmap<AccessMode::NON_ATOMIC>* bitmap(
      const MemoryChunk* chunk) = 0;

  virtual void VerifyPointers(ObjectSlot start, ObjectSlot end) = 0;
  virtual void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) = 0;
  virtual void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) = 0;

  virtual bool IsMarked(HeapObject object) = 0;

  virtual bool IsBlackOrGrey(HeapObject object) = 0;

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    VerifyPointers(start, end);
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {
    VerifyPointers(start, end);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    VerifyRootPointers(start, end);
  }

  void VerifyRoots(VisitMode mode);
  void VerifyMarkingOnPage(const Page* page, Address start, Address end);
  void VerifyMarking(NewSpace* new_space);
  void VerifyMarking(PagedSpace* paged_space);
  void VerifyMarking(LargeObjectSpace* lo_space);

  Heap* heap_;
};

void MarkingVerifier::VerifyRoots(VisitMode mode) {
  heap_->IterateStrongRoots(this, mode);
}

void MarkingVerifier::VerifyMarkingOnPage(const Page* page, Address start,
                                          Address end) {
  HeapObject object;
  Address next_object_must_be_here_or_later = start;
  for (Address current = start; current < end;) {
    object = HeapObject::FromAddress(current);
    // One word fillers at the end of a black area can be grey.
    if (IsBlackOrGrey(object) &&
        object.map() != ReadOnlyRoots(heap_).one_pointer_filler_map()) {
      CHECK(IsMarked(object));
      CHECK(current >= next_object_must_be_here_or_later);
      object.Iterate(this);
      next_object_must_be_here_or_later = current + object.Size();
      // The object is either part of a black area of black allocation or a
      // regular black object
      CHECK(
          bitmap(page)->AllBitsSetInRange(
              page->AddressToMarkbitIndex(current),
              page->AddressToMarkbitIndex(next_object_must_be_here_or_later)) ||
          bitmap(page)->AllBitsClearInRange(
              page->AddressToMarkbitIndex(current + kTaggedSize * 2),
              page->AddressToMarkbitIndex(next_object_must_be_here_or_later)));
      current = next_object_must_be_here_or_later;
    } else {
      current += kTaggedSize;
    }
  }
}

void MarkingVerifier::VerifyMarking(NewSpace* space) {
  Address end = space->top();
  // The bottom position is at the start of its page. Allows us to use
  // page->area_start() as start of range on all pages.
  CHECK_EQ(space->first_allocatable_address(),
           space->first_page()->area_start());

  PageRange range(space->first_allocatable_address(), end);
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

void MarkingVerifier::VerifyMarking(LargeObjectSpace* lo_space) {
  LargeObjectIterator it(lo_space);
  for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
    if (IsBlackOrGrey(obj)) {
      obj.Iterate(this);
    }
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
    VerifyMarking(heap_->new_lo_space());
    VerifyMarking(heap_->old_space());
    VerifyMarking(heap_->code_space());
    VerifyMarking(heap_->map_space());
    VerifyMarking(heap_->lo_space());
    VerifyMarking(heap_->code_lo_space());
  }

 protected:
  ConcurrentBitmap<AccessMode::NON_ATOMIC>* bitmap(
      const MemoryChunk* chunk) override {
    return marking_state_->bitmap(chunk);
  }

  bool IsMarked(HeapObject object) override {
    return marking_state_->IsBlack(object);
  }

  bool IsBlackOrGrey(HeapObject object) override {
    return marking_state_->IsBlackOrGrey(object);
  }

  void VerifyPointers(ObjectSlot start, ObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }

  void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }

  void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    VerifyHeapObjectImpl(target);
  }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    DCHECK(RelocInfo::IsEmbeddedObjectMode(rinfo->rmode()));
    if (!host.IsWeakObject(rinfo->target_object())) {
      HeapObject object = rinfo->target_object();
      VerifyHeapObjectImpl(object);
    }
  }

 private:
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object) {
    CHECK(marking_state_->IsBlackOrGrey(heap_object));
  }

  template <typename TSlot>
  V8_INLINE void VerifyPointersImpl(TSlot start, TSlot end) {
    for (TSlot slot = start; slot < end; ++slot) {
      typename TSlot::TObject object = *slot;
      HeapObject heap_object;
      if (object.GetHeapObjectIfStrong(&heap_object)) {
        VerifyHeapObjectImpl(heap_object);
      }
    }
  }

  MarkCompactCollector::NonAtomicMarkingState* marking_state_;
};

class EvacuationVerifier : public ObjectVisitor, public RootVisitor {
 public:
  virtual void Run() = 0;

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    VerifyPointers(start, end);
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {
    VerifyPointers(start, end);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    VerifyRootPointers(start, end);
  }

 protected:
  explicit EvacuationVerifier(Heap* heap) : heap_(heap) {}

  inline Heap* heap() { return heap_; }

  virtual void VerifyPointers(ObjectSlot start, ObjectSlot end) = 0;
  virtual void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) = 0;
  virtual void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) = 0;

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
    HeapObject object = HeapObject::FromAddress(current);
    if (!object.IsFiller()) object.Iterate(this);
    current += object.Size();
  }
}

void EvacuationVerifier::VerifyEvacuation(NewSpace* space) {
  PageRange range(space->first_allocatable_address(), space->top());
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
    if (p->Contains(space->top())) {
      CodePageMemoryModificationScope memory_modification_scope(p);
      heap_->CreateFillerObjectAt(
          space->top(), static_cast<int>(space->limit() - space->top()),
          ClearRecordedSlots::kNo);
    }
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
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object) {
    CHECK_IMPLIES(Heap::InYoungGeneration(heap_object),
                  Heap::InToPage(heap_object));
    CHECK(!MarkCompactCollector::IsOnEvacuationCandidate(heap_object));
  }

  template <typename TSlot>
  void VerifyPointersImpl(TSlot start, TSlot end) {
    for (TSlot current = start; current < end; ++current) {
      typename TSlot::TObject object = *current;
      HeapObject heap_object;
      if (object.GetHeapObjectIfStrong(&heap_object)) {
        VerifyHeapObjectImpl(heap_object);
      }
    }
  }

  void VerifyPointers(ObjectSlot start, ObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }
  void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }
  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    VerifyHeapObjectImpl(target);
  }
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    VerifyHeapObjectImpl(rinfo->target_object());
  }
  void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) override {
    VerifyPointersImpl(start, end);
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

int NumberOfAvailableCores() {
  static int num_cores = V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1;
  // This number of cores should be greater than zero and never change.
  DCHECK_GE(num_cores, 1);
  DCHECK_EQ(num_cores, V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1);
  return num_cores;
}

}  // namespace

int MarkCompactCollectorBase::NumberOfParallelCompactionTasks(int pages) {
  DCHECK_GT(pages, 0);
  int tasks =
      FLAG_parallel_compaction ? Min(NumberOfAvailableCores(), pages) : 1;
  if (!heap_->CanExpandOldGeneration(
          static_cast<size_t>(tasks * Page::kPageSize))) {
    // Optimize for memory usage near the heap limit.
    tasks = 1;
  }
  return tasks;
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

void MarkCompactCollector::TearDown() {
  AbortCompaction();
  AbortWeakObjects();
  if (heap()->incremental_marking()->IsMarking()) {
    marking_worklist()->Clear();
  }
}

void MarkCompactCollector::AddEvacuationCandidate(Page* p) {
  DCHECK(!p->NeverEvacuate());
  p->MarkEvacuationCandidate();
  evacuation_candidates_.push_back(p);
}


static void TraceFragmentation(PagedSpace* space) {
  int number_of_pages = space->CountTotalPages();
  intptr_t reserved = (number_of_pages * space->AreaSize());
  intptr_t free = reserved - space->SizeOfObjects();
  PrintF("[%s]: %d pages, %d (%.1f%%) free\n", space->name(), number_of_pages,
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

#ifdef ENABLE_MINOR_MC
  heap()->minor_mark_compact_collector()->CleanupSweepToIteratePages();
#endif  // ENABLE_MINOR_MC

  MarkLiveObjects();
  ClearNonLiveReferences();
  VerifyMarking();

  RecordObjectStats();

  StartSweepSpaces();
  Evacuate();
  Finish();
}

#ifdef VERIFY_HEAP
void MarkCompactCollector::VerifyMarkbitsAreDirty(ReadOnlySpace* space) {
  ReadOnlyHeapIterator iterator(space);
  for (HeapObject object = iterator.Next(); !object.is_null();
       object = iterator.Next()) {
    CHECK(non_atomic_marking_state()->IsBlack(object));
  }
}

void MarkCompactCollector::VerifyMarkbitsAreClean(PagedSpace* space) {
  for (Page* p : *space) {
    CHECK(non_atomic_marking_state()->bitmap(p)->IsClean());
    CHECK_EQ(0, non_atomic_marking_state()->live_bytes(p));
  }
}


void MarkCompactCollector::VerifyMarkbitsAreClean(NewSpace* space) {
  for (Page* p : PageRange(space->first_allocatable_address(), space->top())) {
    CHECK(non_atomic_marking_state()->bitmap(p)->IsClean());
    CHECK_EQ(0, non_atomic_marking_state()->live_bytes(p));
  }
}

void MarkCompactCollector::VerifyMarkbitsAreClean(LargeObjectSpace* space) {
  LargeObjectIterator it(space);
  for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
    CHECK(non_atomic_marking_state()->IsWhite(obj));
    CHECK_EQ(0, non_atomic_marking_state()->live_bytes(
                    MemoryChunk::FromHeapObject(obj)));
  }
}

void MarkCompactCollector::VerifyMarkbitsAreClean() {
  VerifyMarkbitsAreClean(heap_->old_space());
  VerifyMarkbitsAreClean(heap_->code_space());
  VerifyMarkbitsAreClean(heap_->map_space());
  VerifyMarkbitsAreClean(heap_->new_space());
  // Read-only space should always be black since we never collect any objects
  // in it or linked from it.
  VerifyMarkbitsAreDirty(heap_->read_only_space());
  VerifyMarkbitsAreClean(heap_->lo_space());
  VerifyMarkbitsAreClean(heap_->code_lo_space());
  VerifyMarkbitsAreClean(heap_->new_lo_space());
}

#endif  // VERIFY_HEAP

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
  using LiveBytesPagePair = std::pair<size_t, Page*>;
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
                     "fragmentation_limit_kb=%zu "
                     "fragmentation_limit_percent=%d sum_compaction_kb=%zu "
                     "compaction_limit_kb=%zu\n",
                     space->name(), free_bytes / KB, free_bytes_threshold / KB,
                     target_fragmentation_percent, total_live_bytes / KB,
                     max_evacuated_bytes / KB);
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
                 space->name(), reduce_memory, candidate_count,
                 total_live_bytes / KB);
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

  if (!was_marked_incrementally_) {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_EMBEDDER_PROLOGUE);
    heap_->local_embedder_heap_tracer()->TracePrologue(
        heap_->flags_for_embedder_tracer());
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

void MarkCompactCollector::FinishConcurrentMarking(
    ConcurrentMarking::StopRequest stop_request) {
  // FinishConcurrentMarking is called for both, concurrent and parallel,
  // marking. It is safe to call this function when tasks are already finished.
  if (FLAG_parallel_marking || FLAG_concurrent_marking) {
    heap()->concurrent_marking()->Stop(stop_request);
    heap()->concurrent_marking()->FlushMemoryChunkData(
        non_atomic_marking_state());
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
  if (FLAG_verify_heap) {
    heap()->old_space()->VerifyLiveBytes();
    heap()->map_space()->VerifyLiveBytes();
    heap()->code_space()->VerifyLiveBytes();
  }
#endif
}

void MarkCompactCollector::Finish() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_FINISH);

#ifdef DEBUG
  heap()->VerifyCountersBeforeConcurrentSweeping();
#endif

  CHECK(weak_objects_.current_ephemerons.IsEmpty());
  CHECK(weak_objects_.discovered_ephemerons.IsEmpty());
  weak_objects_.next_ephemerons.Clear();

  sweeper()->StartSweeperTasks();
  sweeper()->StartIterabilityTasks();

  // Clear the marking state of live large objects.
  heap_->lo_space()->ClearMarkingStateOfLiveObjects();
  heap_->code_lo_space()->ClearMarkingStateOfLiveObjects();

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

class MarkCompactCollector::RootMarkingVisitor final : public RootVisitor {
 public:
  explicit RootMarkingVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) final {
    MarkObjectByPointer(root, p);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) final {
    for (FullObjectSlot p = start; p < end; ++p) MarkObjectByPointer(root, p);
  }

 private:
  V8_INLINE void MarkObjectByPointer(Root root, FullObjectSlot p) {
    if (!(*p).IsHeapObject()) return;

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

  void VisitPointer(HeapObject host, ObjectSlot p) final {
    MarkObject(host, *p);
  }

  void VisitPointers(HeapObject host, ObjectSlot start, ObjectSlot end) final {
    for (ObjectSlot p = start; p < end; ++p) {
      DCHECK(!HasWeakHeapObjectTag(*p));
      MarkObject(host, *p);
    }
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final {
    // At the moment, custom roots cannot contain weak pointers.
    UNREACHABLE();
  }

  // VisitEmbedderPointer is defined by ObjectVisitor to call VisitPointers.
  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    MarkObject(host, target);
  }
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    MarkObject(host, rinfo->target_object());
  }

 private:
  V8_INLINE void MarkObject(HeapObject host, Object object) {
    if (!object.IsHeapObject()) return;
    collector_->MarkObject(host, HeapObject::cast(object));
  }

  MarkCompactCollector* const collector_;
};

class InternalizedStringTableCleaner : public ObjectVisitor {
 public:
  InternalizedStringTableCleaner(Heap* heap, HeapObject table)
      : heap_(heap), pointers_removed_(0), table_(table) {}

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    // Visit all HeapObject pointers in [start, end).
    Object the_hole = ReadOnlyRoots(heap_).the_hole_value();
    MarkCompactCollector::NonAtomicMarkingState* marking_state =
        heap_->mark_compact_collector()->non_atomic_marking_state();
    for (ObjectSlot p = start; p < end; ++p) {
      Object o = *p;
      if (o.IsHeapObject()) {
        HeapObject heap_object = HeapObject::cast(o);
        if (marking_state->IsWhite(heap_object)) {
          pointers_removed_++;
          // Set the entry to the_hole_value (as deleted).
          p.store(the_hole);
        } else {
          // StringTable contains only old space strings.
          DCHECK(!Heap::InYoungGeneration(o));
          MarkCompactCollector::RecordSlot(table_, p, heap_object);
        }
      }
    }
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final {
    UNREACHABLE();
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) final { UNREACHABLE(); }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final {
    UNREACHABLE();
  }

  int PointersRemoved() {
    return pointers_removed_;
  }

 private:
  Heap* heap_;
  int pointers_removed_;
  HeapObject table_;
};

class ExternalStringTableCleaner : public RootVisitor {
 public:
  explicit ExternalStringTableCleaner(Heap* heap) : heap_(heap) {}

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    // Visit all HeapObject pointers in [start, end).
    MarkCompactCollector::NonAtomicMarkingState* marking_state =
        heap_->mark_compact_collector()->non_atomic_marking_state();
    Object the_hole = ReadOnlyRoots(heap_).the_hole_value();
    for (FullObjectSlot p = start; p < end; ++p) {
      Object o = *p;
      if (o.IsHeapObject()) {
        HeapObject heap_object = HeapObject::cast(o);
        if (marking_state->IsWhite(heap_object)) {
          if (o.IsExternalString()) {
            heap_->FinalizeExternalString(String::cast(o));
          } else {
            // The original external string may have been internalized.
            DCHECK(o.IsThinString());
          }
          // Set the entry to the_hole_value (as deleted).
          p.store(the_hole);
        }
      }
    }
  }

 private:
  Heap* heap_;
};

// Implementation of WeakObjectRetainer for mark compact GCs. All marked objects
// are retained.
class MarkCompactWeakObjectRetainer : public WeakObjectRetainer {
 public:
  explicit MarkCompactWeakObjectRetainer(
      MarkCompactCollector::NonAtomicMarkingState* marking_state)
      : marking_state_(marking_state) {}

  Object RetainAs(Object object) override {
    HeapObject heap_object = HeapObject::cast(object);
    DCHECK(!marking_state_->IsGrey(heap_object));
    if (marking_state_->IsBlack(heap_object)) {
      return object;
    } else if (object.IsAllocationSite() &&
               !(AllocationSite::cast(object).IsZombie())) {
      // "dead" AllocationSites need to live long enough for a traversal of new
      // space. These sites get a one-time reprieve.

      Object nested = object;
      while (nested.IsAllocationSite()) {
        AllocationSite current_site = AllocationSite::cast(nested);
        // MarkZombie will override the nested_site, read it first before
        // marking
        nested = current_site.nested_site();
        current_site.MarkZombie();
        marking_state_->WhiteToBlack(current_site);
      }

      return object;
    } else {
      return Object();
    }
  }

 private:
  MarkCompactCollector::NonAtomicMarkingState* marking_state_;
};

class RecordMigratedSlotVisitor : public ObjectVisitor {
 public:
  explicit RecordMigratedSlotVisitor(
      MarkCompactCollector* collector,
      EphemeronRememberedSet* ephemeron_remembered_set)
      : collector_(collector),
        ephemeron_remembered_set_(ephemeron_remembered_set) {}

  inline void VisitPointer(HeapObject host, ObjectSlot p) final {
    DCHECK(!HasWeakHeapObjectTag(*p));
    RecordMigratedSlot(host, MaybeObject::FromObject(*p), p.address());
  }

  inline void VisitPointer(HeapObject host, MaybeObjectSlot p) final {
    RecordMigratedSlot(host, *p, p.address());
  }

  inline void VisitPointers(HeapObject host, ObjectSlot start,
                            ObjectSlot end) final {
    while (start < end) {
      VisitPointer(host, start);
      ++start;
    }
  }

  inline void VisitPointers(HeapObject host, MaybeObjectSlot start,
                            MaybeObjectSlot end) final {
    while (start < end) {
      VisitPointer(host, start);
      ++start;
    }
  }

  inline void VisitEphemeron(HeapObject host, int index, ObjectSlot key,
                             ObjectSlot value) override {
    DCHECK(host.IsEphemeronHashTable());
    DCHECK(!Heap::InYoungGeneration(host));

    VisitPointer(host, value);

    if (ephemeron_remembered_set_ && Heap::InYoungGeneration(*key)) {
      auto table = EphemeronHashTable::unchecked_cast(host);
      auto insert_result =
          ephemeron_remembered_set_->insert({table, std::unordered_set<int>()});
      insert_result.first->second.insert(index);
    } else {
      VisitPointer(host, key);
    }
  }

  inline void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    DCHECK_EQ(host, rinfo->host());
    DCHECK(RelocInfo::IsCodeTargetMode(rinfo->rmode()));
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    // The target is always in old space, we don't have to record the slot in
    // the old-to-new remembered set.
    DCHECK(!Heap::InYoungGeneration(target));
    collector_->RecordRelocSlot(host, rinfo, target);
  }

  inline void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    DCHECK_EQ(host, rinfo->host());
    DCHECK(RelocInfo::IsEmbeddedObjectMode(rinfo->rmode()));
    HeapObject object = HeapObject::cast(rinfo->target_object());
    GenerationalBarrierForCode(host, rinfo, object);
    collector_->RecordRelocSlot(host, rinfo, object);
  }

  // Entries that are skipped for recording.
  inline void VisitExternalReference(Code host, RelocInfo* rinfo) final {}
  inline void VisitExternalReference(Foreign host, Address* p) final {}
  inline void VisitRuntimeEntry(Code host, RelocInfo* rinfo) final {}
  inline void VisitInternalReference(Code host, RelocInfo* rinfo) final {}

 protected:
  inline virtual void RecordMigratedSlot(HeapObject host, MaybeObject value,
                                         Address slot) {
    if (value->IsStrongOrWeak()) {
      MemoryChunk* p = MemoryChunk::FromAddress(value.ptr());
      if (p->InYoungGeneration()) {
        DCHECK_IMPLIES(
            p->IsToPage(),
            p->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION) || p->IsLargePage());
        RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(
            MemoryChunk::FromHeapObject(host), slot);
      } else if (p->IsEvacuationCandidate()) {
        RememberedSet<OLD_TO_OLD>::Insert<AccessMode::NON_ATOMIC>(
            MemoryChunk::FromHeapObject(host), slot);
      }
    }
  }

  MarkCompactCollector* collector_;
  EphemeronRememberedSet* ephemeron_remembered_set_;
};

class MigrationObserver {
 public:
  explicit MigrationObserver(Heap* heap) : heap_(heap) {}

  virtual ~MigrationObserver() = default;
  virtual void Move(AllocationSpace dest, HeapObject src, HeapObject dst,
                    int size) = 0;

 protected:
  Heap* heap_;
};

class ProfilingMigrationObserver final : public MigrationObserver {
 public:
  explicit ProfilingMigrationObserver(Heap* heap) : MigrationObserver(heap) {}

  inline void Move(AllocationSpace dest, HeapObject src, HeapObject dst,
                   int size) final {
    if (dest == CODE_SPACE || (dest == OLD_SPACE && dst.IsBytecodeArray())) {
      PROFILE(heap_->isolate(),
              CodeMoveEvent(AbstractCode::cast(src), AbstractCode::cast(dst)));
    }
    heap_->OnMoveEvent(dst, src, size);
  }
};

class HeapObjectVisitor {
 public:
  virtual ~HeapObjectVisitor() = default;
  virtual bool Visit(HeapObject object, int size) = 0;
};

class EvacuateVisitorBase : public HeapObjectVisitor {
 public:
  void AddObserver(MigrationObserver* observer) {
    migration_function_ = RawMigrateObject<MigrationMode::kObserved>;
    observers_.push_back(observer);
  }

 protected:
  enum MigrationMode { kFast, kObserved };

  using MigrateFunction = void (*)(EvacuateVisitorBase* base, HeapObject dst,
                                   HeapObject src, int size,
                                   AllocationSpace dest);

  template <MigrationMode mode>
  static void RawMigrateObject(EvacuateVisitorBase* base, HeapObject dst,
                               HeapObject src, int size, AllocationSpace dest) {
    Address dst_addr = dst.address();
    Address src_addr = src.address();
    DCHECK(base->heap_->AllowedToBeMigrated(src.map(), src, dest));
    DCHECK_NE(dest, LO_SPACE);
    DCHECK_NE(dest, CODE_LO_SPACE);
    if (dest == OLD_SPACE) {
      DCHECK_OBJECT_SIZE(size);
      DCHECK(IsAligned(size, kTaggedSize));
      base->heap_->CopyBlock(dst_addr, src_addr, size);
      if (mode != MigrationMode::kFast)
        base->ExecuteMigrationObservers(dest, src, dst, size);
      dst.IterateBodyFast(dst.map(), size, base->record_visitor_);
    } else if (dest == CODE_SPACE) {
      DCHECK_CODEOBJECT_SIZE(size, base->heap_->code_space());
      base->heap_->CopyBlock(dst_addr, src_addr, size);
      Code::cast(dst).Relocate(dst_addr - src_addr);
      if (mode != MigrationMode::kFast)
        base->ExecuteMigrationObservers(dest, src, dst, size);
      dst.IterateBodyFast(dst.map(), size, base->record_visitor_);
    } else {
      DCHECK_OBJECT_SIZE(size);
      DCHECK(dest == NEW_SPACE);
      base->heap_->CopyBlock(dst_addr, src_addr, size);
      if (mode != MigrationMode::kFast)
        base->ExecuteMigrationObservers(dest, src, dst, size);
    }
    src.set_map_word(MapWord::FromForwardingAddress(dst));
  }

  EvacuateVisitorBase(Heap* heap, LocalAllocator* local_allocator,
                      RecordMigratedSlotVisitor* record_visitor)
      : heap_(heap),
        local_allocator_(local_allocator),
        record_visitor_(record_visitor) {
    migration_function_ = RawMigrateObject<MigrationMode::kFast>;
  }

  inline bool TryEvacuateObject(AllocationSpace target_space, HeapObject object,
                                int size, HeapObject* target_object) {
#ifdef VERIFY_HEAP
    if (AbortCompactionForTesting(object)) return false;
#endif  // VERIFY_HEAP
    AllocationAlignment alignment = HeapObject::RequiredAlignment(object.map());
    AllocationResult allocation =
        local_allocator_->Allocate(target_space, size, alignment);
    if (allocation.To(target_object)) {
      MigrateObject(*target_object, object, size, target_space);
      if (target_space == CODE_SPACE)
        MemoryChunk::FromHeapObject(*target_object)
            ->GetCodeObjectRegistry()
            ->RegisterNewlyAllocatedCodeObject((*target_object).address());
      return true;
    }
    return false;
  }

  inline void ExecuteMigrationObservers(AllocationSpace dest, HeapObject src,
                                        HeapObject dst, int size) {
    for (MigrationObserver* obs : observers_) {
      obs->Move(dest, src, dst, size);
    }
  }

  inline void MigrateObject(HeapObject dst, HeapObject src, int size,
                            AllocationSpace dest) {
    migration_function_(this, dst, src, size, dest);
  }

#ifdef VERIFY_HEAP
  bool AbortCompactionForTesting(HeapObject object) {
    if (FLAG_stress_compaction) {
      const uintptr_t mask = static_cast<uintptr_t>(FLAG_random_seed) &
                             kPageAlignmentMask & ~kObjectAlignmentMask;
      if ((object.ptr() & kPageAlignmentMask) == mask) {
        Page* page = Page::FromHeapObject(object);
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
        local_pretenuring_feedback_(local_pretenuring_feedback),
        is_incremental_marking_(heap->incremental_marking()->IsMarking()) {}

  inline bool Visit(HeapObject object, int size) override {
    if (TryEvacuateWithoutCopy(object)) return true;
    HeapObject target_object;
    if (heap_->ShouldBePromoted(object.address()) &&
        TryEvacuateObject(OLD_SPACE, object, size, &target_object)) {
      promoted_size_ += size;
      return true;
    }
    heap_->UpdateAllocationSite(object.map(), object,
                                local_pretenuring_feedback_);
    HeapObject target;
    AllocationSpace space = AllocateTargetObject(object, size, &target);
    MigrateObject(HeapObject::cast(target), object, size, space);
    semispace_copied_size_ += size;
    return true;
  }

  intptr_t promoted_size() { return promoted_size_; }
  intptr_t semispace_copied_size() { return semispace_copied_size_; }

 private:
  inline bool TryEvacuateWithoutCopy(HeapObject object) {
    if (is_incremental_marking_) return false;

    Map map = object.map();

    // Some objects can be evacuated without creating a copy.
    if (map.visitor_id() == kVisitThinString) {
      HeapObject actual = ThinString::cast(object).unchecked_actual();
      if (MarkCompactCollector::IsOnEvacuationCandidate(actual)) return false;
      object.map_slot().Relaxed_Store(
          MapWord::FromForwardingAddress(actual).ToMap());
      return true;
    }
    // TODO(mlippautz): Handle ConsString.

    return false;
  }

  inline AllocationSpace AllocateTargetObject(HeapObject old_object, int size,
                                              HeapObject* target_object) {
    AllocationAlignment alignment =
        HeapObject::RequiredAlignment(old_object.map());
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
      heap_->FatalProcessOutOfMemory(
          "MarkCompactCollector: semi-space copy, fallback in old gen");
    }
    return allocation;
  }

  LocalAllocationBuffer buffer_;
  intptr_t promoted_size_;
  intptr_t semispace_copied_size_;
  Heap::PretenuringFeedbackMap* local_pretenuring_feedback_;
  bool is_incremental_marking_;
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
        page->heap()->new_space()->from_space().RemovePage(page);
        Page* new_page = Page::ConvertNewToOld(page);
        DCHECK(!new_page->InYoungGeneration());
        new_page->SetFlag(Page::PAGE_NEW_OLD_PROMOTION);
        break;
      }
    }
  }

  inline bool Visit(HeapObject object, int size) override {
    if (mode == NEW_TO_NEW) {
      heap_->UpdateAllocationSite(object.map(), object,
                                  local_pretenuring_feedback_);
    } else if (mode == NEW_TO_OLD) {
      object.IterateBodyFast(record_visitor_);
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

  inline bool Visit(HeapObject object, int size) override {
    HeapObject target_object;
    if (TryEvacuateObject(Page::FromHeapObject(object)->owner()->identity(),
                          object, size, &target_object)) {
      DCHECK(object.map_word().IsForwardingAddress());
      return true;
    }
    return false;
  }
};

class EvacuateRecordOnlyVisitor final : public HeapObjectVisitor {
 public:
  explicit EvacuateRecordOnlyVisitor(Heap* heap) : heap_(heap) {}

  inline bool Visit(HeapObject object, int size) override {
    RecordMigratedSlotVisitor visitor(heap_->mark_compact_collector(),
                                      &heap_->ephemeron_remembered_set_);
    object.IterateBodyFast(&visitor);
    return true;
  }

 private:
  Heap* heap_;
};

bool MarkCompactCollector::IsUnmarkedHeapObject(Heap* heap, FullObjectSlot p) {
  Object o = *p;
  if (!o.IsHeapObject()) return false;
  HeapObject heap_object = HeapObject::cast(o);
  return heap->mark_compact_collector()->non_atomic_marking_state()->IsWhite(
      heap_object);
}

void MarkCompactCollector::MarkStringTable(
    ObjectVisitor* custom_root_body_visitor) {
  StringTable string_table = heap()->string_table();
  // Mark the string table itself.
  if (marking_state()->WhiteToBlack(string_table)) {
    // Explicitly mark the prefix.
    string_table.IteratePrefix(custom_root_body_visitor);
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

void MarkCompactCollector::ProcessEphemeronsUntilFixpoint() {
  bool work_to_do = true;
  int iterations = 0;
  int max_iterations = FLAG_ephemeron_fixpoint_iterations;

  while (work_to_do) {
    PerformWrapperTracing();

    if (iterations >= max_iterations) {
      // Give up fixpoint iteration and switch to linear algorithm.
      ProcessEphemeronsLinear();
      break;
    }

    // Move ephemerons from next_ephemerons into current_ephemerons to
    // drain them in this iteration.
    weak_objects_.current_ephemerons.Swap(weak_objects_.next_ephemerons);
    heap()->concurrent_marking()->set_ephemeron_marked(false);

    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON_MARKING);

      if (FLAG_parallel_marking) {
        heap_->concurrent_marking()->RescheduleTasksIfNeeded();
      }

      work_to_do = ProcessEphemerons();
      FinishConcurrentMarking(
          ConcurrentMarking::StopRequest::COMPLETE_ONGOING_TASKS);
    }

    CHECK(weak_objects_.current_ephemerons.IsEmpty());
    CHECK(weak_objects_.discovered_ephemerons.IsEmpty());

    work_to_do = work_to_do || !marking_worklist()->IsEmpty() ||
                 heap()->concurrent_marking()->ephemeron_marked() ||
                 !marking_worklist()->IsEmbedderEmpty() ||
                 !heap()->local_embedder_heap_tracer()->IsRemoteTracingDone();
    ++iterations;
  }

  CHECK(marking_worklist()->IsEmpty());
  CHECK(weak_objects_.current_ephemerons.IsEmpty());
  CHECK(weak_objects_.discovered_ephemerons.IsEmpty());
}

bool MarkCompactCollector::ProcessEphemerons() {
  Ephemeron ephemeron;
  bool ephemeron_marked = false;

  // Drain current_ephemerons and push ephemerons where key and value are still
  // unreachable into next_ephemerons.
  while (weak_objects_.current_ephemerons.Pop(kMainThread, &ephemeron)) {
    if (ProcessEphemeron(ephemeron.key, ephemeron.value)) {
      ephemeron_marked = true;
    }
  }

  // Drain marking worklist and push discovered ephemerons into
  // discovered_ephemerons.
  ProcessMarkingWorklist();

  // Drain discovered_ephemerons (filled in the drain MarkingWorklist-phase
  // before) and push ephemerons where key and value are still unreachable into
  // next_ephemerons.
  while (weak_objects_.discovered_ephemerons.Pop(kMainThread, &ephemeron)) {
    if (ProcessEphemeron(ephemeron.key, ephemeron.value)) {
      ephemeron_marked = true;
    }
  }

  // Flush local ephemerons for main task to global pool.
  weak_objects_.ephemeron_hash_tables.FlushToGlobal(kMainThread);
  weak_objects_.next_ephemerons.FlushToGlobal(kMainThread);

  return ephemeron_marked;
}

void MarkCompactCollector::ProcessEphemeronsLinear() {
  TRACE_GC(heap()->tracer(),
           GCTracer::Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON_LINEAR);
  CHECK(heap()->concurrent_marking()->IsStopped());
  std::unordered_multimap<HeapObject, HeapObject, Object::Hasher> key_to_values;
  Ephemeron ephemeron;

  DCHECK(weak_objects_.current_ephemerons.IsEmpty());
  weak_objects_.current_ephemerons.Swap(weak_objects_.next_ephemerons);

  while (weak_objects_.current_ephemerons.Pop(kMainThread, &ephemeron)) {
    ProcessEphemeron(ephemeron.key, ephemeron.value);

    if (non_atomic_marking_state()->IsWhite(ephemeron.value)) {
      key_to_values.insert(std::make_pair(ephemeron.key, ephemeron.value));
    }
  }

  ephemeron_marking_.newly_discovered_limit = key_to_values.size();
  bool work_to_do = true;

  while (work_to_do) {
    PerformWrapperTracing();

    ResetNewlyDiscovered();
    ephemeron_marking_.newly_discovered_limit = key_to_values.size();

    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON_MARKING);
      // Drain marking worklist and push all discovered objects into
      // newly_discovered.
      ProcessMarkingWorklistInternal<
          MarkCompactCollector::MarkingWorklistProcessingMode::
              kTrackNewlyDiscoveredObjects>();
    }

    while (weak_objects_.discovered_ephemerons.Pop(kMainThread, &ephemeron)) {
      ProcessEphemeron(ephemeron.key, ephemeron.value);

      if (non_atomic_marking_state()->IsWhite(ephemeron.value)) {
        key_to_values.insert(std::make_pair(ephemeron.key, ephemeron.value));
      }
    }

    if (ephemeron_marking_.newly_discovered_overflowed) {
      // If newly_discovered was overflowed just visit all ephemerons in
      // next_ephemerons.
      weak_objects_.next_ephemerons.Iterate([&](Ephemeron ephemeron) {
        if (non_atomic_marking_state()->IsBlackOrGrey(ephemeron.key) &&
            non_atomic_marking_state()->WhiteToGrey(ephemeron.value)) {
          marking_worklist()->Push(ephemeron.value);
        }
      });

    } else {
      // This is the good case: newly_discovered stores all discovered
      // objects. Now use key_to_values to see if discovered objects keep more
      // objects alive due to ephemeron semantics.
      for (HeapObject object : ephemeron_marking_.newly_discovered) {
        auto range = key_to_values.equal_range(object);
        for (auto it = range.first; it != range.second; ++it) {
          HeapObject value = it->second;
          MarkObject(object, value);
        }
      }
    }

    // Do NOT drain marking worklist here, otherwise the current checks
    // for work_to_do are not sufficient for determining if another iteration
    // is necessary.

    work_to_do = !marking_worklist()->IsEmpty() ||
                 !marking_worklist()->IsEmbedderEmpty() ||
                 !heap()->local_embedder_heap_tracer()->IsRemoteTracingDone();
    CHECK(weak_objects_.discovered_ephemerons.IsEmpty());
  }

  ResetNewlyDiscovered();
  ephemeron_marking_.newly_discovered.shrink_to_fit();

  CHECK(marking_worklist()->IsEmpty());
}

void MarkCompactCollector::PerformWrapperTracing() {
  if (heap_->local_embedder_heap_tracer()->InUse()) {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_EMBEDDER_TRACING);
    {
      LocalEmbedderHeapTracer::ProcessingScope scope(
          heap_->local_embedder_heap_tracer());
      HeapObject object;
      while (marking_worklist()->embedder()->Pop(kMainThread, &object)) {
        scope.TracePossibleWrapper(JSObject::cast(object));
      }
    }
    heap_->local_embedder_heap_tracer()->Trace(
        std::numeric_limits<double>::infinity());
  }
}

void MarkCompactCollector::ProcessMarkingWorklist() {
  ProcessMarkingWorklistInternal<
      MarkCompactCollector::MarkingWorklistProcessingMode::kDefault>();
}

template <MarkCompactCollector::MarkingWorklistProcessingMode mode>
void MarkCompactCollector::ProcessMarkingWorklistInternal() {
  HeapObject object;
  MarkCompactMarkingVisitor visitor(this, marking_state());
  while (!(object = marking_worklist()->Pop()).is_null()) {
    // Left trimming may result in grey or black filler objects on the marking
    // worklist. Ignore these objects.
    if (object.IsFiller()) {
      // Due to copying mark bits and the fact that grey and black have their
      // first bit set, one word fillers are always black.
      DCHECK_IMPLIES(
          object.map() == ReadOnlyRoots(heap()).one_pointer_filler_map(),
          marking_state()->IsBlack(object));
      // Other fillers may be black or grey depending on the color of the object
      // that was trimmed.
      DCHECK_IMPLIES(
          object.map() != ReadOnlyRoots(heap()).one_pointer_filler_map(),
          marking_state()->IsBlackOrGrey(object));
      continue;
    }
    DCHECK(object.IsHeapObject());
    DCHECK(heap()->Contains(object));
    DCHECK(!(marking_state()->IsWhite(object)));
    marking_state()->GreyToBlack(object);
    if (mode == MarkCompactCollector::MarkingWorklistProcessingMode::
                    kTrackNewlyDiscoveredObjects) {
      AddNewlyDiscovered(object);
    }
    Map map = object.map();
    MarkObject(object, map);
    visitor.Visit(map, object);
  }
}

bool MarkCompactCollector::ProcessEphemeron(HeapObject key, HeapObject value) {
  if (marking_state()->IsBlackOrGrey(key)) {
    if (marking_state()->WhiteToGrey(value)) {
      marking_worklist()->Push(value);
      return true;
    }

  } else if (marking_state()->IsWhite(value)) {
    weak_objects_.next_ephemerons.Push(kMainThread, Ephemeron{key, value});
  }

  return false;
}

void MarkCompactCollector::ProcessEphemeronMarking() {
  DCHECK(marking_worklist()->IsEmpty());

  // Incremental marking might leave ephemerons in main task's local
  // buffer, flush it into global pool.
  weak_objects_.next_ephemerons.FlushToGlobal(kMainThread);

  ProcessEphemeronsUntilFixpoint();

  CHECK(marking_worklist()->IsEmpty());
  CHECK(heap()->local_embedder_heap_tracer()->IsRemoteTracingDone());
}

void MarkCompactCollector::ProcessTopOptimizedFrame(ObjectVisitor* visitor) {
  for (StackFrameIterator it(isolate(), isolate()->thread_local_top());
       !it.done(); it.Advance()) {
    if (it.frame()->type() == StackFrame::INTERPRETED) {
      return;
    }
    if (it.frame()->type() == StackFrame::OPTIMIZED) {
      Code code = it.frame()->LookupCode();
      if (!code.CanDeoptAt(it.frame()->pc())) {
        Code::BodyDescriptor::IterateBody(code.map(), code, visitor);
      }
      return;
    }
  }
}

void MarkCompactCollector::RecordObjectStats() {
  if (V8_UNLIKELY(TracingFlags::is_gc_stats_enabled())) {
    heap()->CreateObjectStats();
    ObjectStatsCollector collector(heap(), heap()->live_object_stats_.get(),
                                   heap()->dead_object_stats_.get());
    collector.Collect();
    if (V8_UNLIKELY(TracingFlags::gc_stats.load(std::memory_order_relaxed) &
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
      heap_->concurrent_marking()->RescheduleTasksIfNeeded();
    }
    ProcessMarkingWorklist();

    FinishConcurrentMarking(
        ConcurrentMarking::StopRequest::COMPLETE_ONGOING_TASKS);
    ProcessMarkingWorklist();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WEAK_CLOSURE);

    DCHECK(marking_worklist()->IsEmpty());

    // Mark objects reachable through the embedder heap. This phase is
    // opportunistic as it may not discover graphs that are only reachable
    // through ephemerons.
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_EMBEDDER_TRACING_CLOSURE);
      do {
        // PerformWrapperTracing() also empties the work items collected by
        // concurrent markers. As a result this call needs to happen at least
        // once.
        PerformWrapperTracing();
        ProcessMarkingWorklist();
      } while (!heap_->local_embedder_heap_tracer()->IsRemoteTracingDone() ||
               !marking_worklist()->IsEmbedderEmpty());
      DCHECK(marking_worklist()->IsEmbedderEmpty());
      DCHECK(marking_worklist()->IsEmpty());
    }

    // The objects reachable from the roots are marked, yet unreachable objects
    // are unmarked. Mark objects reachable due to embedder heap tracing or
    // harmony weak maps.
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON);
      ProcessEphemeronMarking();
      DCHECK(marking_worklist()->IsEmpty());
    }

    // The objects reachable from the roots, weak maps, and embedder heap
    // tracing are marked. Objects pointed to only by weak global handles cannot
    // be immediately reclaimed. Instead, we have to mark them as pending and
    // mark objects reachable from them.
    //
    // First we identify nonlive weak handles and mark them as pending
    // destruction.
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_WEAK_HANDLES);
      heap()->isolate()->global_handles()->IterateWeakRootsIdentifyFinalizers(
          &IsUnmarkedHeapObject);
      ProcessMarkingWorklist();
    }

    // Process finalizers, effectively keeping them alive until the next
    // garbage collection.
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_WEAK_ROOTS);
      heap()->isolate()->global_handles()->IterateWeakRootsForFinalizers(
          &root_visitor);
      ProcessMarkingWorklist();
    }

    // Repeat ephemeron processing from the newly marked objects.
    {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WEAK_CLOSURE_HARMONY);
      ProcessEphemeronMarking();
      DCHECK(marking_worklist()->IsEmbedderEmpty());
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

  epoch_++;
}

void MarkCompactCollector::ClearNonLiveReferences() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_STRING_TABLE);

    // Prune the string table removing all strings only pointed to by the
    // string table.  Cannot use string_table() here because the string
    // table is marked.
    StringTable string_table = heap()->string_table();
    InternalizedStringTableCleaner internalized_visitor(heap(), string_table);
    string_table.IterateElements(&internalized_visitor);
    string_table.ElementsRemoved(internalized_visitor.PointersRemoved());

    ExternalStringTableCleaner external_visitor(heap());
    heap()->external_string_table_.IterateAll(&external_visitor);
    heap()->external_string_table_.CleanUpAll();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_FLUSHABLE_BYTECODE);
    ClearOldBytecodeCandidates();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_FLUSHED_JS_FUNCTIONS);
    ClearFlushedJsFunctions();
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
    // ClearFullMapTransitions must be called before weak references are
    // cleared.
    ClearFullMapTransitions();
  }
  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_REFERENCES);
    ClearWeakReferences();
    ClearWeakCollections();
    ClearJSWeakRefs();
  }

  MarkDependentCodeForDeoptimization();

  DCHECK(weak_objects_.transition_arrays.IsEmpty());
  DCHECK(weak_objects_.weak_references.IsEmpty());
  DCHECK(weak_objects_.weak_objects_in_code.IsEmpty());
  DCHECK(weak_objects_.js_weak_refs.IsEmpty());
  DCHECK(weak_objects_.weak_cells.IsEmpty());
  DCHECK(weak_objects_.bytecode_flushing_candidates.IsEmpty());
  DCHECK(weak_objects_.flushed_js_functions.IsEmpty());
}

void MarkCompactCollector::MarkDependentCodeForDeoptimization() {
  std::pair<HeapObject, Code> weak_object_in_code;
  while (weak_objects_.weak_objects_in_code.Pop(kMainThread,
                                                &weak_object_in_code)) {
    HeapObject object = weak_object_in_code.first;
    Code code = weak_object_in_code.second;
    if (!non_atomic_marking_state()->IsBlackOrGrey(object) &&
        !code.embedded_objects_cleared()) {
      if (!code.marked_for_deoptimization()) {
        code.SetMarkedForDeoptimization("weak objects");
        have_code_to_deoptimize_ = true;
      }
      code.ClearEmbeddedObjects(heap_);
      DCHECK(code.embedded_objects_cleared());
    }
  }
}

void MarkCompactCollector::ClearPotentialSimpleMapTransition(Map dead_target) {
  DCHECK(non_atomic_marking_state()->IsWhite(dead_target));
  Object potential_parent = dead_target.constructor_or_backpointer();
  if (potential_parent.IsMap()) {
    Map parent = Map::cast(potential_parent);
    DisallowHeapAllocation no_gc_obviously;
    if (non_atomic_marking_state()->IsBlackOrGrey(parent) &&
        TransitionsAccessor(isolate(), parent, &no_gc_obviously)
            .HasSimpleTransitionTo(dead_target)) {
      ClearPotentialSimpleMapTransition(parent, dead_target);
    }
  }
}

void MarkCompactCollector::ClearPotentialSimpleMapTransition(Map map,
                                                             Map dead_target) {
  DCHECK(!map.is_prototype_map());
  DCHECK(!dead_target.is_prototype_map());
  DCHECK_EQ(map.raw_transitions(), HeapObjectReference::Weak(dead_target));
  // Take ownership of the descriptor array.
  int number_of_own_descriptors = map.NumberOfOwnDescriptors();
  DescriptorArray descriptors = map.instance_descriptors();
  if (descriptors == dead_target.instance_descriptors() &&
      number_of_own_descriptors > 0) {
    TrimDescriptorArray(map, descriptors);
    DCHECK(descriptors.number_of_descriptors() == number_of_own_descriptors);
  }
}

void MarkCompactCollector::FlushBytecodeFromSFI(
    SharedFunctionInfo shared_info) {
  DCHECK(shared_info.HasBytecodeArray());

  // Retain objects required for uncompiled data.
  String inferred_name = shared_info.inferred_name();
  int start_position = shared_info.StartPosition();
  int end_position = shared_info.EndPosition();

  shared_info.DiscardCompiledMetadata(
      isolate(), [](HeapObject object, ObjectSlot slot, HeapObject target) {
        RecordSlot(object, slot, target);
      });

  // The size of the bytecode array should always be larger than an
  // UncompiledData object.
  STATIC_ASSERT(BytecodeArray::SizeFor(0) >=
                UncompiledDataWithoutPreparseData::kSize);

  // Replace bytecode array with an uncompiled data array.
  HeapObject compiled_data = shared_info.GetBytecodeArray();
  Address compiled_data_start = compiled_data.address();
  int compiled_data_size = compiled_data.Size();
  MemoryChunk* chunk = MemoryChunk::FromAddress(compiled_data_start);

  // Clear any recorded slots for the compiled data as being invalid.
  RememberedSet<OLD_TO_NEW>::RemoveRange(
      chunk, compiled_data_start, compiled_data_start + compiled_data_size,
      SlotSet::PREFREE_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_OLD>::RemoveRange(
      chunk, compiled_data_start, compiled_data_start + compiled_data_size,
      SlotSet::PREFREE_EMPTY_BUCKETS);

  // Swap the map, using set_map_after_allocation to avoid verify heap checks
  // which are not necessary since we are doing this during the GC atomic pause.
  compiled_data.set_map_after_allocation(
      ReadOnlyRoots(heap()).uncompiled_data_without_preparse_data_map(),
      SKIP_WRITE_BARRIER);

  // Create a filler object for any left over space in the bytecode array.
  if (!heap()->IsLargeObject(compiled_data)) {
    heap()->CreateFillerObjectAt(
        compiled_data.address() + UncompiledDataWithoutPreparseData::kSize,
        compiled_data_size - UncompiledDataWithoutPreparseData::kSize,
        ClearRecordedSlots::kNo);
  }

  // Initialize the uncompiled data.
  UncompiledData uncompiled_data = UncompiledData::cast(compiled_data);
  UncompiledData::Initialize(
      uncompiled_data, inferred_name, start_position, end_position,
      kFunctionLiteralIdInvalid,
      [](HeapObject object, ObjectSlot slot, HeapObject target) {
        RecordSlot(object, slot, target);
      });

  // Mark the uncompiled data as black, and ensure all fields have already been
  // marked.
  DCHECK(non_atomic_marking_state()->IsBlackOrGrey(inferred_name));
  non_atomic_marking_state()->WhiteToBlack(uncompiled_data);

  // Use the raw function data setter to avoid validity checks, since we're
  // performing the unusual task of decompiling.
  shared_info.set_function_data(uncompiled_data);
  DCHECK(!shared_info.is_compiled());
}

void MarkCompactCollector::ClearOldBytecodeCandidates() {
  DCHECK(FLAG_flush_bytecode ||
         weak_objects_.bytecode_flushing_candidates.IsEmpty());
  SharedFunctionInfo flushing_candidate;
  while (weak_objects_.bytecode_flushing_candidates.Pop(kMainThread,
                                                        &flushing_candidate)) {
    // If the BytecodeArray is dead, flush it, which will replace the field with
    // an uncompiled data object.
    if (!non_atomic_marking_state()->IsBlackOrGrey(
            flushing_candidate.GetBytecodeArray())) {
      FlushBytecodeFromSFI(flushing_candidate);
    }

    // Now record the slot, which has either been updated to an uncompiled data,
    // or is the BytecodeArray which is still alive.
    ObjectSlot slot =
        flushing_candidate.RawField(SharedFunctionInfo::kFunctionDataOffset);
    RecordSlot(flushing_candidate, slot, HeapObject::cast(*slot));
  }
}

void MarkCompactCollector::ClearFlushedJsFunctions() {
  DCHECK(FLAG_flush_bytecode || weak_objects_.flushed_js_functions.IsEmpty());
  JSFunction flushed_js_function;
  while (weak_objects_.flushed_js_functions.Pop(kMainThread,
                                                &flushed_js_function)) {
    flushed_js_function.ResetIfBytecodeFlushed();
  }
}

void MarkCompactCollector::ClearFullMapTransitions() {
  TransitionArray array;
  while (weak_objects_.transition_arrays.Pop(kMainThread, &array)) {
    int num_transitions = array.number_of_entries();
    if (num_transitions > 0) {
      Map map;
      // The array might contain "undefined" elements because it's not yet
      // filled. Allow it.
      if (array.GetTargetIfExists(0, isolate(), &map)) {
        DCHECK(!map.is_null());  // Weak pointers aren't cleared yet.
        Map parent = Map::cast(map.constructor_or_backpointer());
        bool parent_is_alive =
            non_atomic_marking_state()->IsBlackOrGrey(parent);
        DescriptorArray descriptors =
            parent_is_alive ? parent.instance_descriptors() : DescriptorArray();
        bool descriptors_owner_died =
            CompactTransitionArray(parent, array, descriptors);
        if (descriptors_owner_died) {
          TrimDescriptorArray(parent, descriptors);
        }
      }
    }
  }
}

bool MarkCompactCollector::CompactTransitionArray(Map map,
                                                  TransitionArray transitions,
                                                  DescriptorArray descriptors) {
  DCHECK(!map.is_prototype_map());
  int num_transitions = transitions.number_of_entries();
  bool descriptors_owner_died = false;
  int transition_index = 0;
  // Compact all live transitions to the left.
  for (int i = 0; i < num_transitions; ++i) {
    Map target = transitions.GetTarget(i);
    DCHECK_EQ(target.constructor_or_backpointer(), map);
    if (non_atomic_marking_state()->IsWhite(target)) {
      if (!descriptors.is_null() &&
          target.instance_descriptors() == descriptors) {
        DCHECK(!target.is_prototype_map());
        descriptors_owner_died = true;
      }
    } else {
      if (i != transition_index) {
        Name key = transitions.GetKey(i);
        transitions.SetKey(transition_index, key);
        HeapObjectSlot key_slot = transitions.GetKeySlot(transition_index);
        RecordSlot(transitions, key_slot, key);
        MaybeObject raw_target = transitions.GetRawTarget(i);
        transitions.SetRawTarget(transition_index, raw_target);
        HeapObjectSlot target_slot =
            transitions.GetTargetSlot(transition_index);
        RecordSlot(transitions, target_slot, raw_target->GetHeapObject());
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
  int trim = transitions.Capacity() - transition_index;
  if (trim > 0) {
    heap_->RightTrimWeakFixedArray(transitions,
                                   trim * TransitionArray::kEntrySize);
    transitions.SetNumberOfTransitions(transition_index);
  }
  return descriptors_owner_died;
}

void MarkCompactCollector::RightTrimDescriptorArray(DescriptorArray array,
                                                    int descriptors_to_trim) {
  int old_nof_all_descriptors = array.number_of_all_descriptors();
  int new_nof_all_descriptors = old_nof_all_descriptors - descriptors_to_trim;
  DCHECK_LT(0, descriptors_to_trim);
  DCHECK_LE(0, new_nof_all_descriptors);
  Address start = array.GetDescriptorSlot(new_nof_all_descriptors).address();
  Address end = array.GetDescriptorSlot(old_nof_all_descriptors).address();
  RememberedSet<OLD_TO_NEW>::RemoveRange(MemoryChunk::FromHeapObject(array),
                                         start, end,
                                         SlotSet::PREFREE_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_OLD>::RemoveRange(MemoryChunk::FromHeapObject(array),
                                         start, end,
                                         SlotSet::PREFREE_EMPTY_BUCKETS);
  heap()->CreateFillerObjectAt(start, static_cast<int>(end - start),
                               ClearRecordedSlots::kNo);
  array.set_number_of_all_descriptors(new_nof_all_descriptors);
}

void MarkCompactCollector::TrimDescriptorArray(Map map,
                                               DescriptorArray descriptors) {
  int number_of_own_descriptors = map.NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) {
    DCHECK(descriptors == ReadOnlyRoots(heap_).empty_descriptor_array());
    return;
  }
  // TODO(ulan): Trim only if slack is greater than some percentage threshold.
  int to_trim =
      descriptors.number_of_all_descriptors() - number_of_own_descriptors;
  if (to_trim > 0) {
    descriptors.set_number_of_descriptors(number_of_own_descriptors);
    RightTrimDescriptorArray(descriptors, to_trim);

    TrimEnumCache(map, descriptors);
    descriptors.Sort();

    if (FLAG_unbox_double_fields) {
      LayoutDescriptor layout_descriptor = map.layout_descriptor();
      layout_descriptor = layout_descriptor.Trim(heap_, map, descriptors,
                                                 number_of_own_descriptors);
      SLOW_DCHECK(layout_descriptor.IsConsistentWithMap(map, true));
    }
  }
  DCHECK(descriptors.number_of_descriptors() == number_of_own_descriptors);
  map.set_owns_descriptors(true);
}

void MarkCompactCollector::TrimEnumCache(Map map, DescriptorArray descriptors) {
  int live_enum = map.EnumLength();
  if (live_enum == kInvalidEnumCacheSentinel) {
    live_enum = map.NumberOfEnumerableProperties();
  }
  if (live_enum == 0) return descriptors.ClearEnumCache();
  EnumCache enum_cache = descriptors.enum_cache();

  FixedArray keys = enum_cache.keys();
  int to_trim = keys.length() - live_enum;
  if (to_trim <= 0) return;
  heap_->RightTrimFixedArray(keys, to_trim);

  FixedArray indices = enum_cache.indices();
  to_trim = indices.length() - live_enum;
  if (to_trim <= 0) return;
  heap_->RightTrimFixedArray(indices, to_trim);
}

void MarkCompactCollector::ClearWeakCollections() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_COLLECTIONS);
  EphemeronHashTable table;

  while (weak_objects_.ephemeron_hash_tables.Pop(kMainThread, &table)) {
    for (int i = 0; i < table.Capacity(); i++) {
      HeapObject key = HeapObject::cast(table.KeyAt(i));
#ifdef VERIFY_HEAP
      Object value = table.ValueAt(i);

      if (value.IsHeapObject()) {
        CHECK_IMPLIES(
            non_atomic_marking_state()->IsBlackOrGrey(key),
            non_atomic_marking_state()->IsBlackOrGrey(HeapObject::cast(value)));
      }
#endif
      if (!non_atomic_marking_state()->IsBlackOrGrey(key)) {
        table.RemoveEntry(i);
      }
    }
  }
  for (auto it = heap_->ephemeron_remembered_set_.begin();
       it != heap_->ephemeron_remembered_set_.end();) {
    if (!non_atomic_marking_state()->IsBlackOrGrey(it->first)) {
      it = heap_->ephemeron_remembered_set_.erase(it);
    } else {
      ++it;
    }
  }
}

void MarkCompactCollector::ClearWeakReferences() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_REFERENCES);
  std::pair<HeapObject, HeapObjectSlot> slot;
  HeapObjectReference cleared_weak_ref =
      HeapObjectReference::ClearedValue(isolate());
  while (weak_objects_.weak_references.Pop(kMainThread, &slot)) {
    HeapObject value;
    // The slot could have been overwritten, so we have to treat it
    // as MaybeObjectSlot.
    MaybeObjectSlot location(slot.second);
    if ((*location)->GetHeapObjectIfWeak(&value)) {
      DCHECK(!value.IsCell());
      if (non_atomic_marking_state()->IsBlackOrGrey(value)) {
        // The value of the weak reference is alive.
        RecordSlot(slot.first, HeapObjectSlot(location), value);
      } else {
        if (value.IsMap()) {
          // The map is non-live.
          ClearPotentialSimpleMapTransition(Map::cast(value));
        }
        location.store(cleared_weak_ref);
      }
    }
  }
}

void MarkCompactCollector::ClearJSWeakRefs() {
  if (!FLAG_harmony_weak_refs) {
    return;
  }
  JSWeakRef weak_ref;
  while (weak_objects_.js_weak_refs.Pop(kMainThread, &weak_ref)) {
    HeapObject target = HeapObject::cast(weak_ref.target());
    if (!non_atomic_marking_state()->IsBlackOrGrey(target)) {
      weak_ref.set_target(ReadOnlyRoots(isolate()).undefined_value());
    } else {
      // The value of the JSWeakRef is alive.
      ObjectSlot slot = weak_ref.RawField(JSWeakRef::kTargetOffset);
      RecordSlot(weak_ref, slot, target);
    }
  }
  WeakCell weak_cell;
  while (weak_objects_.weak_cells.Pop(kMainThread, &weak_cell)) {
    HeapObject target = HeapObject::cast(weak_cell.target());
    if (!non_atomic_marking_state()->IsBlackOrGrey(target)) {
      DCHECK(!target.IsUndefined());
      // The value of the WeakCell is dead.
      JSFinalizationGroup finalization_group =
          JSFinalizationGroup::cast(weak_cell.finalization_group());
      if (!finalization_group.scheduled_for_cleanup()) {
        heap()->AddDirtyJSFinalizationGroup(
            finalization_group,
            [](HeapObject object, ObjectSlot slot, Object target) {
              if (target.IsHeapObject()) {
                RecordSlot(object, slot, HeapObject::cast(target));
              }
            });
      }
      // We're modifying the pointers in WeakCell and JSFinalizationGroup during
      // GC; thus we need to record the slots it writes. The normal write
      // barrier is not enough, since it's disabled before GC.
      weak_cell.Nullify(isolate(),
                        [](HeapObject object, ObjectSlot slot, Object target) {
                          if (target.IsHeapObject()) {
                            RecordSlot(object, slot, HeapObject::cast(target));
                          }
                        });
      DCHECK(finalization_group.NeedsCleanup());
      DCHECK(finalization_group.scheduled_for_cleanup());
    } else {
      // The value of the WeakCell is alive.
      ObjectSlot slot = weak_cell.RawField(WeakCell::kTargetOffset);
      RecordSlot(weak_cell, slot, HeapObject::cast(*slot));
    }
  }
}

void MarkCompactCollector::AbortWeakObjects() {
  weak_objects_.transition_arrays.Clear();
  weak_objects_.ephemeron_hash_tables.Clear();
  weak_objects_.current_ephemerons.Clear();
  weak_objects_.next_ephemerons.Clear();
  weak_objects_.discovered_ephemerons.Clear();
  weak_objects_.weak_references.Clear();
  weak_objects_.weak_objects_in_code.Clear();
  weak_objects_.js_weak_refs.Clear();
  weak_objects_.weak_cells.Clear();
  weak_objects_.bytecode_flushing_candidates.Clear();
  weak_objects_.flushed_js_functions.Clear();
}

bool MarkCompactCollector::IsOnEvacuationCandidate(MaybeObject obj) {
  return Page::FromAddress(obj.ptr())->IsEvacuationCandidate();
}

MarkCompactCollector::RecordRelocSlotInfo
MarkCompactCollector::PrepareRecordRelocSlot(Code host, RelocInfo* rinfo,
                                             HeapObject target) {
  RecordRelocSlotInfo result;
  result.should_record = false;
  Page* target_page = Page::FromHeapObject(target);
  Page* source_page = Page::FromHeapObject(host);
  if (target_page->IsEvacuationCandidate() &&
      (rinfo->host().is_null() ||
       !source_page->ShouldSkipEvacuationSlotRecording())) {
    RelocInfo::Mode rmode = rinfo->rmode();
    Address addr = rinfo->pc();
    SlotType slot_type = SlotTypeForRelocInfoMode(rmode);
    if (rinfo->IsInConstantPool()) {
      addr = rinfo->constant_pool_entry_address();
      if (RelocInfo::IsCodeTargetMode(rmode)) {
        slot_type = CODE_ENTRY_SLOT;
      } else {
        // Constant pools don't support compressed values at this time
        // (this may change, therefore use a DCHECK).
        DCHECK(RelocInfo::IsFullEmbeddedObject(rmode));
        slot_type = OBJECT_SLOT;
      }
    }
    uintptr_t offset = addr - source_page->address();
    DCHECK_LT(offset, static_cast<uintptr_t>(TypedSlotSet::kMaxOffset));
    result.should_record = true;
    result.memory_chunk = source_page;
    result.slot_type = slot_type;
    result.offset = static_cast<uint32_t>(offset);
  }
  return result;
}

void MarkCompactCollector::RecordRelocSlot(Code host, RelocInfo* rinfo,
                                           HeapObject target) {
  RecordRelocSlotInfo info = PrepareRecordRelocSlot(host, rinfo, target);
  if (info.should_record) {
    RememberedSet<OLD_TO_OLD>::InsertTyped(info.memory_chunk, info.slot_type,
                                           info.offset);
  }
}

namespace {

// Missing specialization MakeSlotValue<FullObjectSlot, WEAK>() will turn
// attempt to store a weak reference to strong-only slot to a compilation error.
template <typename TSlot, HeapObjectReferenceType reference_type>
typename TSlot::TObject MakeSlotValue(HeapObject heap_object);

template <>
Object MakeSlotValue<ObjectSlot, HeapObjectReferenceType::STRONG>(
    HeapObject heap_object) {
  return heap_object;
}

template <>
MaybeObject MakeSlotValue<MaybeObjectSlot, HeapObjectReferenceType::STRONG>(
    HeapObject heap_object) {
  return HeapObjectReference::Strong(heap_object);
}

template <>
MaybeObject MakeSlotValue<MaybeObjectSlot, HeapObjectReferenceType::WEAK>(
    HeapObject heap_object) {
  return HeapObjectReference::Weak(heap_object);
}

#ifdef V8_COMPRESS_POINTERS
template <>
Object MakeSlotValue<FullObjectSlot, HeapObjectReferenceType::STRONG>(
    HeapObject heap_object) {
  return heap_object;
}

template <>
MaybeObject MakeSlotValue<FullMaybeObjectSlot, HeapObjectReferenceType::STRONG>(
    HeapObject heap_object) {
  return HeapObjectReference::Strong(heap_object);
}

// The following specialization
//   MakeSlotValue<FullMaybeObjectSlot, HeapObjectReferenceType::WEAK>()
// is not used.
#endif

template <AccessMode access_mode, HeapObjectReferenceType reference_type,
          typename TSlot>
static inline SlotCallbackResult UpdateSlot(TSlot slot,
                                            typename TSlot::TObject old,
                                            HeapObject heap_obj) {
  static_assert(
      std::is_same<TSlot, FullObjectSlot>::value ||
          std::is_same<TSlot, ObjectSlot>::value ||
          std::is_same<TSlot, FullMaybeObjectSlot>::value ||
          std::is_same<TSlot, MaybeObjectSlot>::value,
      "Only [Full]ObjectSlot and [Full]MaybeObjectSlot are expected here");
  MapWord map_word = heap_obj.map_word();
  if (map_word.IsForwardingAddress()) {
    DCHECK_IMPLIES(!Heap::InFromPage(heap_obj),
                   MarkCompactCollector::IsOnEvacuationCandidate(heap_obj) ||
                       Page::FromHeapObject(heap_obj)->IsFlagSet(
                           Page::COMPACTION_WAS_ABORTED));
    typename TSlot::TObject target =
        MakeSlotValue<TSlot, reference_type>(map_word.ToForwardingAddress());
    if (access_mode == AccessMode::NON_ATOMIC) {
      slot.store(target);
    } else {
      slot.Release_CompareAndSwap(old, target);
    }
    DCHECK(!Heap::InFromPage(target));
    DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(target));
  } else {
    DCHECK(heap_obj.map().IsMap());
  }
  // OLD_TO_OLD slots are always removed after updating.
  return REMOVE_SLOT;
}

template <AccessMode access_mode, typename TSlot>
static inline SlotCallbackResult UpdateSlot(TSlot slot) {
  typename TSlot::TObject obj = slot.Relaxed_Load();
  HeapObject heap_obj;
  if (TSlot::kCanBeWeak && obj->GetHeapObjectIfWeak(&heap_obj)) {
    UpdateSlot<access_mode, HeapObjectReferenceType::WEAK>(slot, obj, heap_obj);
  } else if (obj->GetHeapObjectIfStrong(&heap_obj)) {
    return UpdateSlot<access_mode, HeapObjectReferenceType::STRONG>(slot, obj,
                                                                    heap_obj);
  }
  return REMOVE_SLOT;
}

template <AccessMode access_mode, typename TSlot>
static inline SlotCallbackResult UpdateStrongSlot(TSlot slot) {
  typename TSlot::TObject obj = slot.Relaxed_Load();
  DCHECK(!HAS_WEAK_HEAP_OBJECT_TAG(obj.ptr()));
  HeapObject heap_obj;
  if (obj.GetHeapObject(&heap_obj)) {
    return UpdateSlot<access_mode, HeapObjectReferenceType::STRONG>(slot, obj,
                                                                    heap_obj);
  }
  return REMOVE_SLOT;
}

}  // namespace

// Visitor for updating root pointers and to-space pointers.
// It does not expect to encounter pointers to dead objects.
class PointersUpdatingVisitor : public ObjectVisitor, public RootVisitor {
 public:
  PointersUpdatingVisitor() {}

  void VisitPointer(HeapObject host, ObjectSlot p) override {
    UpdateStrongSlotInternal(p);
  }

  void VisitPointer(HeapObject host, MaybeObjectSlot p) override {
    UpdateSlotInternal(p);
  }

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    for (ObjectSlot p = start; p < end; ++p) {
      UpdateStrongSlotInternal(p);
    }
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final {
    for (MaybeObjectSlot p = start; p < end; ++p) {
      UpdateSlotInternal(p);
    }
  }

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) override {
    UpdateRootSlotInternal(p);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot p = start; p < end; ++p) {
      UpdateRootSlotInternal(p);
    }
  }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    // This visitor nevers visits code objects.
    UNREACHABLE();
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    // This visitor nevers visits code objects.
    UNREACHABLE();
  }

 private:
  static inline SlotCallbackResult UpdateRootSlotInternal(FullObjectSlot slot) {
    return UpdateStrongSlot<AccessMode::NON_ATOMIC>(slot);
  }

  static inline SlotCallbackResult UpdateStrongMaybeObjectSlotInternal(
      MaybeObjectSlot slot) {
    return UpdateStrongSlot<AccessMode::NON_ATOMIC>(slot);
  }

  static inline SlotCallbackResult UpdateStrongSlotInternal(ObjectSlot slot) {
    return UpdateStrongSlot<AccessMode::NON_ATOMIC>(slot);
  }

  static inline SlotCallbackResult UpdateSlotInternal(MaybeObjectSlot slot) {
    return UpdateSlot<AccessMode::NON_ATOMIC>(slot);
  }
};

static String UpdateReferenceInExternalStringTableEntry(Heap* heap,
                                                        FullObjectSlot p) {
  MapWord map_word = HeapObject::cast(*p).map_word();

  if (map_word.IsForwardingAddress()) {
    String new_string = String::cast(map_word.ToForwardingAddress());

    if (new_string.IsExternalString()) {
      MemoryChunk::MoveExternalBackingStoreBytes(
          ExternalBackingStoreType::kExternalString,
          Page::FromAddress((*p).ptr()), Page::FromHeapObject(new_string),
          ExternalString::cast(new_string).ExternalPayloadSize());
    }
    return new_string;
  }

  return String::cast(*p);
}

void MarkCompactCollector::EvacuatePrologue() {
  // New space.
  NewSpace* new_space = heap()->new_space();
  // Append the list of new space pages to be processed.
  for (Page* p :
       PageRange(new_space->first_allocatable_address(), new_space->top())) {
    new_space_evacuation_pages_.push_back(p);
  }
  new_space->Flip();
  new_space->ResetLinearAllocationArea();

  heap()->new_lo_space()->Flip();
  heap()->new_lo_space()->ResetPendingObject();

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
  heap()->code_lo_space()->FreeUnmarkedObjects();
  heap()->new_lo_space()->FreeUnmarkedObjects();
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
    if (chunk->InYoungGeneration()) return kObjectsNewToOld;
    return kObjectsOldToOld;
  }

  // NewSpacePages with more live bytes than this threshold qualify for fast
  // evacuation.
  static intptr_t NewSpacePageEvacuationThreshold() {
    if (FLAG_page_promotion)
      return FLAG_page_promotion_threshold *
             MemoryChunkLayout::AllocatableMemoryInDataPage() / 100;
    return MemoryChunkLayout::AllocatableMemoryInDataPage() + kTaggedSize;
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

  virtual ~Evacuator() = default;

  void EvacuatePage(MemoryChunk* chunk);

  void AddObserver(MigrationObserver* observer) {
    new_space_visitor_.AddObserver(observer);
    old_space_visitor_.AddObserver(observer);
  }

  // Merge back locally cached info sequentially. Note that this method needs
  // to be called from the main thread.
  inline void Finalize();

  virtual GCTracer::BackgroundScope::ScopeId GetBackgroundTracingScope() = 0;

 protected:
  static const int kInitialLocalPretenuringFeedbackCapacity = 256;

  // |saved_live_bytes| returns the live bytes of the page that was processed.
  virtual void RawEvacuatePage(MemoryChunk* chunk,
                               intptr_t* saved_live_bytes) = 0;

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

void Evacuator::EvacuatePage(MemoryChunk* chunk) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"), "Evacuator::EvacuatePage");
  DCHECK(chunk->SweepingDone());
  intptr_t saved_live_bytes = 0;
  double evacuation_time = 0.0;
  {
    AlwaysAllocateScope always_allocate(heap()->isolate());
    TimedScope timed_scope(&evacuation_time);
    RawEvacuatePage(chunk, &saved_live_bytes);
  }
  ReportCompactionProgress(evacuation_time, saved_live_bytes);
  if (FLAG_trace_evacuation) {
    PrintIsolate(heap()->isolate(),
                 "evacuation[%p]: page=%p new_space=%d "
                 "page_evacuation=%d executable=%d contains_age_mark=%d "
                 "live_bytes=%" V8PRIdPTR " time=%f success=%d\n",
                 static_cast<void*>(this), static_cast<void*>(chunk),
                 chunk->InNewSpace(),
                 chunk->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION) ||
                     chunk->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION),
                 chunk->IsFlagSet(MemoryChunk::IS_EXECUTABLE),
                 chunk->Contains(heap()->new_space()->age_mark()),
                 saved_live_bytes, evacuation_time,
                 chunk->IsFlagSet(Page::COMPACTION_WAS_ABORTED));
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
  explicit FullEvacuator(MarkCompactCollector* collector)
      : Evacuator(collector->heap(), &record_visitor_),
        record_visitor_(collector, &ephemeron_remembered_set_),
        collector_(collector) {}

  GCTracer::BackgroundScope::ScopeId GetBackgroundTracingScope() override {
    return GCTracer::BackgroundScope::MC_BACKGROUND_EVACUATE_COPY;
  }

  inline void Finalize() {
    Evacuator::Finalize();

    for (auto it = ephemeron_remembered_set_.begin();
         it != ephemeron_remembered_set_.end(); ++it) {
      auto insert_result =
          heap()->ephemeron_remembered_set_.insert({it->first, it->second});
      if (!insert_result.second) {
        // Insertion didn't happen, there was already an item.
        auto set = insert_result.first->second;
        for (int entry : it->second) {
          set.insert(entry);
        }
      }
    }
  }

 protected:
  void RawEvacuatePage(MemoryChunk* chunk, intptr_t* live_bytes) override;
  EphemeronRememberedSet ephemeron_remembered_set_;
  RecordMigratedSlotVisitor record_visitor_;

  MarkCompactCollector* collector_;
};

void FullEvacuator::RawEvacuatePage(MemoryChunk* chunk, intptr_t* live_bytes) {
  const EvacuationMode evacuation_mode = ComputeEvacuationMode(chunk);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
               "FullEvacuator::RawEvacuatePage", "evacuation_mode",
               evacuation_mode);
  MarkCompactCollector::NonAtomicMarkingState* marking_state =
      collector_->non_atomic_marking_state();
  *live_bytes = marking_state->live_bytes(chunk);
  HeapObject failed_object;
  switch (evacuation_mode) {
    case kObjectsNewToOld:
      LiveObjectVisitor::VisitBlackObjectsNoFail(
          chunk, marking_state, &new_space_visitor_,
          LiveObjectVisitor::kClearMarkbits);
      // ArrayBufferTracker will be updated during pointers updating.
      break;
    case kPageNewToOld:
      LiveObjectVisitor::VisitBlackObjectsNoFail(
          chunk, marking_state, &new_to_old_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      new_to_old_page_visitor_.account_moved_bytes(
          marking_state->live_bytes(chunk));
      // ArrayBufferTracker will be updated during sweeping.
      break;
    case kPageNewToNew:
      LiveObjectVisitor::VisitBlackObjectsNoFail(
          chunk, marking_state, &new_to_new_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      new_to_new_page_visitor_.account_moved_bytes(
          marking_state->live_bytes(chunk));
      // ArrayBufferTracker will be updated during sweeping.
      break;
    case kObjectsOldToOld: {
      const bool success = LiveObjectVisitor::VisitBlackObjects(
          chunk, marking_state, &old_space_visitor_,
          LiveObjectVisitor::kClearMarkbits, &failed_object);
      if (!success) {
        // Aborted compaction page. Actual processing happens on the main
        // thread for simplicity reasons.
        collector_->ReportAbortedEvacuationCandidate(failed_object, chunk);
      } else {
        // ArrayBufferTracker will be updated during pointers updating.
      }
      break;
    }
  }
}

class EvacuationItem : public ItemParallelJob::Item {
 public:
  explicit EvacuationItem(MemoryChunk* chunk) : chunk_(chunk) {}
  ~EvacuationItem() override = default;
  MemoryChunk* chunk() const { return chunk_; }

 private:
  MemoryChunk* chunk_;
};

class PageEvacuationTask : public ItemParallelJob::Task {
 public:
  PageEvacuationTask(Isolate* isolate, Evacuator* evacuator)
      : ItemParallelJob::Task(isolate),
        evacuator_(evacuator),
        tracer_(isolate->heap()->tracer()) {}

  void RunInParallel() override {
    TRACE_BACKGROUND_GC(tracer_, evacuator_->GetBackgroundTracingScope());
    EvacuationItem* item = nullptr;
    while ((item = GetItem<EvacuationItem>()) != nullptr) {
      evacuator_->EvacuatePage(item->chunk());
      item->MarkFinished();
    }
  }

 private:
  Evacuator* evacuator_;
  GCTracer* tracer_;
};

template <class Evacuator, class Collector>
void MarkCompactCollectorBase::CreateAndExecuteEvacuationTasks(
    Collector* collector, ItemParallelJob* job,
    MigrationObserver* migration_observer, const intptr_t live_bytes) {
  // Used for trace summary.
  double compaction_speed = 0;
  if (FLAG_trace_evacuation) {
    compaction_speed = heap()->tracer()->CompactionSpeedInBytesPerMillisecond();
  }

  const bool profiling = isolate()->LogObjectRelocation();
  ProfilingMigrationObserver profiling_observer(heap());

  const int wanted_num_tasks =
      NumberOfParallelCompactionTasks(job->NumberOfItems());
  Evacuator** evacuators = new Evacuator*[wanted_num_tasks];
  for (int i = 0; i < wanted_num_tasks; i++) {
    evacuators[i] = new Evacuator(collector);
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
                 "wanted_tasks=%d tasks=%d cores=%d live_bytes=%" V8PRIdPTR
                 " compaction_speed=%.f\n",
                 isolate()->time_millis_since_init(),
                 FLAG_parallel_compaction ? "yes" : "no", job->NumberOfItems(),
                 wanted_num_tasks, job->NumberOfTasks(),
                 V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1,
                 live_bytes, compaction_speed);
  }
}

bool MarkCompactCollectorBase::ShouldMovePage(Page* p, intptr_t live_bytes) {
  const bool reduce_memory = heap()->ShouldReduceMemory();
  const Address age_mark = heap()->new_space()->age_mark();
  return !reduce_memory && !p->NeverEvacuate() &&
         (live_bytes > Evacuator::NewSpacePageEvacuationThreshold()) &&
         !p->Contains(age_mark) && heap()->CanExpandOldGeneration(live_bytes);
}

void MarkCompactCollector::EvacuatePagesInParallel() {
  ItemParallelJob evacuation_job(isolate()->cancelable_task_manager(),
                                 &page_parallel_job_semaphore_);
  intptr_t live_bytes = 0;

  for (Page* page : old_space_evacuation_pages_) {
    live_bytes += non_atomic_marking_state()->live_bytes(page);
    evacuation_job.AddItem(new EvacuationItem(page));
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
    evacuation_job.AddItem(new EvacuationItem(page));
  }

  // Promote young generation large objects.
  IncrementalMarking::NonAtomicMarkingState* marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();

  for (auto it = heap()->new_lo_space()->begin();
       it != heap()->new_lo_space()->end();) {
    LargePage* current = *it;
    it++;
    HeapObject object = current->GetObject();
    DCHECK(!marking_state->IsGrey(object));
    if (marking_state->IsBlack(object)) {
      heap_->lo_space()->PromoteNewLargeObject(current);
      current->SetFlag(Page::PAGE_NEW_OLD_PROMOTION);
      evacuation_job.AddItem(new EvacuationItem(current));
    }
  }

  if (evacuation_job.NumberOfItems() == 0) return;

  CreateAndExecuteEvacuationTasks<FullEvacuator>(this, &evacuation_job, nullptr,
                                                 live_bytes);
  PostProcessEvacuationCandidates();
}

class EvacuationWeakObjectRetainer : public WeakObjectRetainer {
 public:
  Object RetainAs(Object object) override {
    if (object.IsHeapObject()) {
      HeapObject heap_object = HeapObject::cast(object);
      MapWord map_word = heap_object.map_word();
      if (map_word.IsForwardingAddress()) {
        return map_word.ToForwardingAddress();
      }
    }
    return object;
  }
};

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
                                          HeapObject* failed_object) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
               "LiveObjectVisitor::VisitBlackObjects");
  for (auto object_and_size :
       LiveObjectRange<kBlackObjects>(chunk, marking_state->bitmap(chunk))) {
    HeapObject const object = object_and_size.first;
    if (!visitor->Visit(object, object_and_size.second)) {
      if (iteration_mode == kClearMarkbits) {
        marking_state->bitmap(chunk)->ClearRange(
            chunk->AddressToMarkbitIndex(chunk->area_start()),
            chunk->AddressToMarkbitIndex(object.address()));
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
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
               "LiveObjectVisitor::VisitBlackObjectsNoFail");
  if (chunk->IsLargePage()) {
    HeapObject object = reinterpret_cast<LargePage*>(chunk)->GetObject();
    if (marking_state->IsBlack(object)) {
      const bool success = visitor->Visit(object, object.Size());
      USE(success);
      DCHECK(success);
    }
  } else {
    for (auto object_and_size :
         LiveObjectRange<kBlackObjects>(chunk, marking_state->bitmap(chunk))) {
      HeapObject const object = object_and_size.first;
      DCHECK(marking_state->IsBlack(object));
      const bool success = visitor->Visit(object, object_and_size.second);
      USE(success);
      DCHECK(success);
    }
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
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
               "LiveObjectVisitor::VisitGreyObjectsNoFail");
  if (chunk->IsLargePage()) {
    HeapObject object = reinterpret_cast<LargePage*>(chunk)->GetObject();
    if (marking_state->IsGrey(object)) {
      const bool success = visitor->Visit(object, object.Size());
      USE(success);
      DCHECK(success);
    }
  } else {
    for (auto object_and_size :
         LiveObjectRange<kGreyObjects>(chunk, marking_state->bitmap(chunk))) {
      HeapObject const object = object_and_size.first;
      DCHECK(marking_state->IsGrey(object));
      const bool success = visitor->Visit(object, object_and_size.second);
      USE(success);
      DCHECK(success);
    }
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
  base::MutexGuard guard(heap()->relocation_mutex());

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
      heap()->FatalProcessOutOfMemory("NewSpace::Rebalance");
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
        sweeper()->AddPageForIterability(p);
      } else if (p->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION)) {
        p->ClearFlag(Page::PAGE_NEW_OLD_PROMOTION);
        DCHECK_EQ(OLD_SPACE, p->owner()->identity());
        sweeper()->AddPage(OLD_SPACE, p, Sweeper::REGULAR);
      }
    }
    new_space_evacuation_pages_.clear();

    for (Page* p : old_space_evacuation_pages_) {
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
  ~UpdatingItem() override = default;
  virtual void Process() = 0;
};

class PointersUpdatingTask : public ItemParallelJob::Task {
 public:
  explicit PointersUpdatingTask(Isolate* isolate,
                                GCTracer::BackgroundScope::ScopeId scope)
      : ItemParallelJob::Task(isolate),
        tracer_(isolate->heap()->tracer()),
        scope_(scope) {}

  void RunInParallel() override {
    TRACE_BACKGROUND_GC(tracer_, scope_);
    UpdatingItem* item = nullptr;
    while ((item = GetItem<UpdatingItem>()) != nullptr) {
      item->Process();
      item->MarkFinished();
    }
  }

 private:
  GCTracer* tracer_;
  GCTracer::BackgroundScope::ScopeId scope_;
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
  ~ToSpaceUpdatingItem() override = default;

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
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                 "ToSpaceUpdatingItem::ProcessVisitAll");
    PointersUpdatingVisitor visitor;
    for (Address cur = start_; cur < end_;) {
      HeapObject object = HeapObject::FromAddress(cur);
      Map map = object.map();
      int size = object.SizeFromMap(map);
      object.IterateBodyFast(map, size, &visitor);
      cur += size;
    }
  }

  void ProcessVisitLive() {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                 "ToSpaceUpdatingItem::ProcessVisitLive");
    // For young generation evacuations we want to visit grey objects, for
    // full MC, we need to visit black objects.
    PointersUpdatingVisitor visitor;
    for (auto object_and_size : LiveObjectRange<kAllLiveObjects>(
             chunk_, marking_state_->bitmap(chunk_))) {
      object_and_size.first.IterateBodyFast(&visitor);
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
  ~RememberedSetUpdatingItem() override = default;

  void Process() override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                 "RememberedSetUpdatingItem::Process");
    base::MutexGuard guard(chunk_->mutex());
    CodePageMemoryModificationScope memory_modification_scope(chunk_);
    UpdateUntypedPointers();
    UpdateTypedPointers();
  }

 private:
  template <typename TSlot>
  inline SlotCallbackResult CheckAndUpdateOldToNewSlot(TSlot slot) {
    static_assert(
        std::is_same<TSlot, FullMaybeObjectSlot>::value ||
            std::is_same<TSlot, MaybeObjectSlot>::value,
        "Only FullMaybeObjectSlot and MaybeObjectSlot are expected here");
    using THeapObjectSlot = typename TSlot::THeapObjectSlot;
    HeapObject heap_object;
    if (!(*slot).GetHeapObject(&heap_object)) {
      return REMOVE_SLOT;
    }
    if (Heap::InFromPage(heap_object)) {
      MapWord map_word = heap_object.map_word();
      if (map_word.IsForwardingAddress()) {
        HeapObjectReference::Update(THeapObjectSlot(slot),
                                    map_word.ToForwardingAddress());
      }
      bool success = (*slot).GetHeapObject(&heap_object);
      USE(success);
      DCHECK(success);
      // If the object was in from space before and is after executing the
      // callback in to space, the object is still live.
      // Unfortunately, we do not know about the slot. It could be in a
      // just freed free space object.
      if (Heap::InToPage(heap_object)) {
        return KEEP_SLOT;
      }
    } else if (Heap::InToPage(heap_object)) {
      // Slots can point to "to" space if the page has been moved, or if the
      // slot has been recorded multiple times in the remembered set, or
      // if the slot was already updated during old->old updating.
      // In case the page has been moved, check markbits to determine liveness
      // of the slot. In the other case, the slot can just be kept.
      if (Page::FromHeapObject(heap_object)
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
      DCHECK(!Heap::InYoungGeneration(heap_object));
    }
    return REMOVE_SLOT;
  }

  void UpdateUntypedPointers() {
    if (chunk_->slot_set<OLD_TO_NEW, AccessMode::NON_ATOMIC>() != nullptr) {
      RememberedSet<OLD_TO_NEW>::Iterate(
          chunk_,
          [this](MaybeObjectSlot slot) {
            return CheckAndUpdateOldToNewSlot(slot);
          },
          SlotSet::PREFREE_EMPTY_BUCKETS);
    }
    if ((updating_mode_ == RememberedSetUpdatingMode::ALL) &&
        (chunk_->slot_set<OLD_TO_OLD, AccessMode::NON_ATOMIC>() != nullptr)) {
      InvalidatedSlotsFilter filter(chunk_);
      RememberedSet<OLD_TO_OLD>::Iterate(
          chunk_,
          [&filter](MaybeObjectSlot slot) {
            if (!filter.IsValid(slot.address())) return REMOVE_SLOT;
            return UpdateSlot<AccessMode::NON_ATOMIC>(slot);
          },
          SlotSet::PREFREE_EMPTY_BUCKETS);
    }
    if ((updating_mode_ == RememberedSetUpdatingMode::ALL) &&
        chunk_->invalidated_slots() != nullptr) {
#ifdef DEBUG
      for (auto object_size : *chunk_->invalidated_slots()) {
        HeapObject object = object_size.first;
        int size = object_size.second;
        DCHECK_LE(object.SizeFromMap(object.map()), size);
      }
#endif
      // The invalidated slots are not needed after old-to-old slots were
      // processsed.
      chunk_->ReleaseInvalidatedSlots();
    }
  }

  void UpdateTypedPointers() {
    if (chunk_->typed_slot_set<OLD_TO_NEW, AccessMode::NON_ATOMIC>() !=
        nullptr) {
      CHECK_NE(chunk_->owner(), heap_->map_space());
      const auto check_and_update_old_to_new_slot_fn =
          [this](FullMaybeObjectSlot slot) {
            return CheckAndUpdateOldToNewSlot(slot);
          };
      RememberedSet<OLD_TO_NEW>::IterateTyped(
          chunk_, [=](SlotType slot_type, Address slot) {
            return UpdateTypedSlotHelper::UpdateTypedSlot(
                heap_, slot_type, slot, check_and_update_old_to_new_slot_fn);
          });
    }
    if ((updating_mode_ == RememberedSetUpdatingMode::ALL) &&
        (chunk_->typed_slot_set<OLD_TO_OLD, AccessMode::NON_ATOMIC>() !=
         nullptr)) {
      CHECK_NE(chunk_->owner(), heap_->map_space());
      RememberedSet<OLD_TO_OLD>::IterateTyped(
          chunk_, [=](SlotType slot_type, Address slot) {
            // Using UpdateStrongSlot is OK here, because there are no weak
            // typed slots.
            return UpdateTypedSlotHelper::UpdateTypedSlot(
                heap_, slot_type, slot,
                UpdateStrongSlot<AccessMode::NON_ATOMIC, FullMaybeObjectSlot>);
          });
    }
  }

  Heap* heap_;
  MarkingState* marking_state_;
  MemoryChunk* chunk_;
  RememberedSetUpdatingMode updating_mode_;
};

UpdatingItem* MarkCompactCollector::CreateToSpaceUpdatingItem(
    MemoryChunk* chunk, Address start, Address end) {
  return new ToSpaceUpdatingItem<NonAtomicMarkingState>(
      chunk, start, end, non_atomic_marking_state());
}

UpdatingItem* MarkCompactCollector::CreateRememberedSetUpdatingItem(
    MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) {
  return new RememberedSetUpdatingItem<NonAtomicMarkingState>(
      heap(), non_atomic_marking_state(), chunk, updating_mode);
}

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
  ~ArrayBufferTrackerUpdatingItem() override = default;

  void Process() override {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                 "ArrayBufferTrackerUpdatingItem::Process", "EvacuationState",
                 state_);
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
  const Address space_start = heap()->new_space()->first_allocatable_address();
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

class EphemeronTableUpdatingItem : public UpdatingItem {
 public:
  enum EvacuationState { kRegular, kAborted };

  explicit EphemeronTableUpdatingItem(Heap* heap) : heap_(heap) {}
  ~EphemeronTableUpdatingItem() override = default;

  void Process() override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                 "EphemeronTableUpdatingItem::Process");

    for (auto it = heap_->ephemeron_remembered_set_.begin();
         it != heap_->ephemeron_remembered_set_.end();) {
      EphemeronHashTable table = it->first;
      auto& indices = it->second;
      if (table.map_word().IsForwardingAddress()) {
        // The table has moved, and RecordMigratedSlotVisitor::VisitEphemeron
        // inserts entries for the moved table into ephemeron_remembered_set_.
        it = heap_->ephemeron_remembered_set_.erase(it);
        continue;
      }
      DCHECK(table.map().IsMap());
      DCHECK(table.Object::IsEphemeronHashTable());
      for (auto iti = indices.begin(); iti != indices.end();) {
        // EphemeronHashTable keys must be heap objects.
        HeapObjectSlot key_slot(
            table.RawFieldOfElementAt(EphemeronHashTable::EntryToIndex(*iti)));
        HeapObject key = key_slot.ToHeapObject();
        MapWord map_word = key.map_word();
        if (map_word.IsForwardingAddress()) {
          key = map_word.ToForwardingAddress();
          key_slot.StoreHeapObject(key);
        }
        if (!heap_->InYoungGeneration(key)) {
          iti = indices.erase(iti);
        } else {
          ++iti;
        }
      }
      if (indices.size() == 0) {
        it = heap_->ephemeron_remembered_set_.erase(it);
      } else {
        ++it;
      }
    }
  }

 private:
  Heap* const heap_;
};

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
    remembered_set_pages += CollectRememberedSetUpdatingItems(
        &updating_job, heap()->code_lo_space(), RememberedSetUpdatingMode::ALL);
    const int remembered_set_tasks =
        remembered_set_pages == 0
            ? 0
            : NumberOfParallelPointerUpdateTasks(remembered_set_pages,
                                                 old_to_new_slots_);
    const int to_space_tasks = CollectToSpaceUpdatingItems(&updating_job);
    const int num_ephemeron_table_updating_tasks = 1;
    const int num_tasks =
        Max(to_space_tasks,
            remembered_set_tasks + num_ephemeron_table_updating_tasks);
    for (int i = 0; i < num_tasks; i++) {
      updating_job.AddTask(new PointersUpdatingTask(
          isolate(),
          GCTracer::BackgroundScope::MC_BACKGROUND_EVACUATE_UPDATE_POINTERS));
    }
    updating_job.AddItem(new EphemeronTableUpdatingItem(heap()));
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
        updating_job.AddTask(new PointersUpdatingTask(
            isolate(),
            GCTracer::BackgroundScope::MC_BACKGROUND_EVACUATE_UPDATE_POINTERS));
      }
      updating_job.Run();
      heap()->array_buffer_collector()->FreeAllocations();
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

void MarkCompactCollector::ReportAbortedEvacuationCandidate(
    HeapObject failed_object, MemoryChunk* chunk) {
  base::MutexGuard guard(&mutex_);

  aborted_evacuation_candidates_.push_back(
      std::make_pair(failed_object, static_cast<Page*>(chunk)));
}

void MarkCompactCollector::PostProcessEvacuationCandidates() {
  for (auto object_and_page : aborted_evacuation_candidates_) {
    HeapObject failed_object = object_and_page.first;
    Page* page = object_and_page.second;
    page->SetFlag(Page::COMPACTION_WAS_ABORTED);
    // Aborted compaction page. We have to record slots here, since we
    // might not have recorded them in first place.

    // Remove outdated slots.
    RememberedSet<OLD_TO_NEW>::RemoveRange(page, page->address(),
                                           failed_object.address(),
                                           SlotSet::PREFREE_EMPTY_BUCKETS);
    RememberedSet<OLD_TO_NEW>::RemoveRangeTyped(page, page->address(),
                                                failed_object.address());
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
      p->owner()->memory_chunk_list().Remove(p);
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
  space->ClearAllocatorState();

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

    // One unused page is kept, all further are released before sweeping them.
    if (non_atomic_marking_state()->live_bytes(p) == 0) {
      if (unused_page_present) {
        if (FLAG_gc_verbose) {
          PrintIsolate(isolate(), "sweeping: released page: %p",
                       static_cast<void*>(p));
        }
        ArrayBufferTracker::FreeAll(p);
        space->memory_chunk_list().Remove(p);
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
                 space->name(), will_be_swept);
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

void MarkCompactCollector::MarkingWorklist::PrintWorklist(
    const char* worklist_name, ConcurrentMarkingWorklist* worklist) {
  std::map<InstanceType, int> count;
  int total_count = 0;
  worklist->IterateGlobalPool([&count, &total_count](HeapObject obj) {
    ++total_count;
    count[obj.map().instance_type()]++;
  });
  std::vector<std::pair<int, InstanceType>> rank;
  rank.reserve(count.size());
  for (const auto& i : count) {
    rank.emplace_back(i.second, i.first);
  }
  std::map<InstanceType, std::string> instance_type_name;
#define INSTANCE_TYPE_NAME(name) instance_type_name[name] = #name;
  INSTANCE_TYPE_LIST(INSTANCE_TYPE_NAME)
#undef INSTANCE_TYPE_NAME
  std::sort(rank.begin(), rank.end(),
            std::greater<std::pair<int, InstanceType>>());
  PrintF("Worklist %s: %d\n", worklist_name, total_count);
  for (auto i : rank) {
    PrintF("  [%s]: %d\n", instance_type_name[i.second].c_str(), i.first);
  }
}

#ifdef ENABLE_MINOR_MC

namespace {

#ifdef VERIFY_HEAP

class YoungGenerationMarkingVerifier : public MarkingVerifier {
 public:
  explicit YoungGenerationMarkingVerifier(Heap* heap)
      : MarkingVerifier(heap),
        marking_state_(
            heap->minor_mark_compact_collector()->non_atomic_marking_state()) {}

  ConcurrentBitmap<AccessMode::NON_ATOMIC>* bitmap(
      const MemoryChunk* chunk) override {
    return marking_state_->bitmap(chunk);
  }

  bool IsMarked(HeapObject object) override {
    return marking_state_->IsGrey(object);
  }

  bool IsBlackOrGrey(HeapObject object) override {
    return marking_state_->IsBlackOrGrey(object);
  }

  void Run() override {
    VerifyRoots(VISIT_ALL_IN_SCAVENGE);
    VerifyMarking(heap_->new_space());
  }

 protected:
  void VerifyPointers(ObjectSlot start, ObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }

  void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    VerifyHeapObjectImpl(target);
  }
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    VerifyHeapObjectImpl(rinfo->target_object());
  }
  void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }

 private:
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object) {
    CHECK_IMPLIES(Heap::InYoungGeneration(heap_object), IsMarked(heap_object));
  }

  template <typename TSlot>
  V8_INLINE void VerifyPointersImpl(TSlot start, TSlot end) {
    for (TSlot slot = start; slot < end; ++slot) {
      typename TSlot::TObject object = *slot;
      HeapObject heap_object;
      // Minor MC treats weak references as strong.
      if (object.GetHeapObject(&heap_object)) {
        VerifyHeapObjectImpl(heap_object);
      }
    }
  }

  MinorMarkCompactCollector::NonAtomicMarkingState* marking_state_;
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
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object) {
    CHECK_IMPLIES(Heap::InYoungGeneration(heap_object),
                  Heap::InToPage(heap_object));
  }

  template <typename TSlot>
  void VerifyPointersImpl(TSlot start, TSlot end) {
    for (TSlot current = start; current < end; ++current) {
      typename TSlot::TObject object = *current;
      HeapObject heap_object;
      if (object.GetHeapObject(&heap_object)) {
        VerifyHeapObjectImpl(heap_object);
      }
    }
  }

  void VerifyPointers(ObjectSlot start, ObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }
  void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }
  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    VerifyHeapObjectImpl(target);
  }
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    VerifyHeapObjectImpl(rinfo->target_object());
  }
  void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }
};

#endif  // VERIFY_HEAP

bool IsUnmarkedObjectForYoungGeneration(Heap* heap, FullObjectSlot p) {
  DCHECK_IMPLIES(Heap::InYoungGeneration(*p), Heap::InToPage(*p));
  return Heap::InYoungGeneration(*p) && !heap->minor_mark_compact_collector()
                                             ->non_atomic_marking_state()
                                             ->IsGrey(HeapObject::cast(*p));
}

}  // namespace

class YoungGenerationMarkingVisitor final
    : public NewSpaceVisitor<YoungGenerationMarkingVisitor> {
 public:
  YoungGenerationMarkingVisitor(
      MinorMarkCompactCollector::MarkingState* marking_state,
      MinorMarkCompactCollector::MarkingWorklist* global_worklist, int task_id)
      : worklist_(global_worklist, task_id), marking_state_(marking_state) {}

  V8_INLINE void VisitPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }

  V8_INLINE void VisitPointers(HeapObject host, MaybeObjectSlot start,
                               MaybeObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }

  V8_INLINE void VisitPointer(HeapObject host, ObjectSlot slot) final {
    VisitPointerImpl(host, slot);
  }

  V8_INLINE void VisitPointer(HeapObject host, MaybeObjectSlot slot) final {
    VisitPointerImpl(host, slot);
  }

  V8_INLINE void VisitCodeTarget(Code host, RelocInfo* rinfo) final {
    // Code objects are not expected in new space.
    UNREACHABLE();
  }

  V8_INLINE void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final {
    // Code objects are not expected in new space.
    UNREACHABLE();
  }

 private:
  template <typename TSlot>
  V8_INLINE void VisitPointersImpl(HeapObject host, TSlot start, TSlot end) {
    for (TSlot slot = start; slot < end; ++slot) {
      VisitPointer(host, slot);
    }
  }

  template <typename TSlot>
  V8_INLINE void VisitPointerImpl(HeapObject host, TSlot slot) {
    typename TSlot::TObject target = *slot;
    if (Heap::InYoungGeneration(target)) {
      // Treat weak references as strong.
      // TODO(marja): Proper weakness handling for minor-mcs.
      HeapObject target_object = target.GetHeapObject();
      MarkObjectViaMarkingWorklist(target_object);
    }
  }

  inline void MarkObjectViaMarkingWorklist(HeapObject object) {
    if (marking_state_->WhiteToGrey(object)) {
      // Marking deque overflow is unsupported for the young generation.
      CHECK(worklist_.Push(object));
    }
  }

  MinorMarkCompactCollector::MarkingWorklist::View worklist_;
  MinorMarkCompactCollector::MarkingState* marking_state_;
};

void MinorMarkCompactCollector::SetUp() {}

void MinorMarkCompactCollector::TearDown() {}

MinorMarkCompactCollector::MinorMarkCompactCollector(Heap* heap)
    : MarkCompactCollectorBase(heap),
      worklist_(new MinorMarkCompactCollector::MarkingWorklist()),
      main_marking_visitor_(new YoungGenerationMarkingVisitor(
          marking_state(), worklist_, kMainMarker)),
      page_parallel_job_semaphore_(0) {
  static_assert(
      kNumMarkers <= MinorMarkCompactCollector::MarkingWorklist::kMaxNumTasks,
      "more marker tasks than marking deque can handle");
}

MinorMarkCompactCollector::~MinorMarkCompactCollector() {
  delete worklist_;
  delete main_marking_visitor_;
}

int MinorMarkCompactCollector::NumberOfParallelMarkingTasks(int pages) {
  DCHECK_GT(pages, 0);
  if (!FLAG_minor_mc_parallel_marking) return 1;
  // Pages are not private to markers but we can still use them to estimate the
  // amount of marking that is required.
  const int kPagesPerTask = 2;
  const int wanted_tasks = Max(1, pages / kPagesPerTask);
  return Min(NumberOfAvailableCores(),
             Min(wanted_tasks,
                 MinorMarkCompactCollector::MarkingWorklist::kMaxNumTasks));
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

class YoungGenerationMigrationObserver final : public MigrationObserver {
 public:
  YoungGenerationMigrationObserver(Heap* heap,
                                   MarkCompactCollector* mark_compact_collector)
      : MigrationObserver(heap),
        mark_compact_collector_(mark_compact_collector) {}

  inline void Move(AllocationSpace dest, HeapObject src, HeapObject dst,
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
      : RecordMigratedSlotVisitor(collector, nullptr) {}

  void VisitCodeTarget(Code host, RelocInfo* rinfo) final { UNREACHABLE(); }
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final {
    UNREACHABLE();
  }

 private:
  // Only record slots for host objects that are considered as live by the full
  // collector.
  inline bool IsLive(HeapObject object) {
    return collector_->non_atomic_marking_state()->IsBlack(object);
  }

  inline void RecordMigratedSlot(HeapObject host, MaybeObject value,
                                 Address slot) final {
    if (value->IsStrongOrWeak()) {
      MemoryChunk* p = MemoryChunk::FromAddress(value.ptr());
      if (p->InYoungGeneration()) {
        DCHECK_IMPLIES(
            p->IsToPage(),
            p->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION) || p->IsLargePage());
        RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(
            MemoryChunk::FromHeapObject(host), slot);
      } else if (p->IsEvacuationCandidate() && IsLive(host)) {
        RememberedSet<OLD_TO_OLD>::Insert<AccessMode::NON_ATOMIC>(
            MemoryChunk::FromHeapObject(host), slot);
      }
    }
  }
};

void MinorMarkCompactCollector::UpdatePointersAfterEvacuation() {
  TRACE_GC(heap()->tracer(),
           GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS);

  PointersUpdatingVisitor updating_visitor;
  ItemParallelJob updating_job(isolate()->cancelable_task_manager(),
                               &page_parallel_job_semaphore_);

  CollectNewSpaceArrayBufferTrackerItems(&updating_job);
  // Create batches of global handles.
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
  remembered_set_pages += CollectRememberedSetUpdatingItems(
      &updating_job, heap()->code_lo_space(),
      RememberedSetUpdatingMode::OLD_TO_NEW_ONLY);
  const int remembered_set_tasks =
      remembered_set_pages == 0 ? 0
                                : NumberOfParallelPointerUpdateTasks(
                                      remembered_set_pages, old_to_new_slots_);
  const int num_tasks = Max(to_space_tasks, remembered_set_tasks);
  for (int i = 0; i < num_tasks; i++) {
    updating_job.AddTask(new PointersUpdatingTask(
        isolate(), GCTracer::BackgroundScope::
                       MINOR_MC_BACKGROUND_EVACUATE_UPDATE_POINTERS));
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
    heap()->array_buffer_collector()->FreeAllocations();
  }

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_WEAK);

    EvacuationWeakObjectRetainer evacuation_object_retainer;
    heap()->ProcessWeakListRoots(&evacuation_object_retainer);

    // Update pointers from external string table.
    heap()->UpdateYoungReferencesInExternalStringTable(
        &UpdateReferenceInExternalStringTableEntry);
  }
}

class MinorMarkCompactCollector::RootMarkingVisitor : public RootVisitor {
 public:
  explicit RootMarkingVisitor(MinorMarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) final {
    MarkObjectByPointer(p);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) final {
    for (FullObjectSlot p = start; p < end; ++p) {
      MarkObjectByPointer(p);
    }
  }

 private:
  V8_INLINE void MarkObjectByPointer(FullObjectSlot p) {
    if (!(*p).IsHeapObject()) return;
    collector_->MarkRootObject(HeapObject::cast(*p));
  }
  MinorMarkCompactCollector* const collector_;
};

void MinorMarkCompactCollector::CollectGarbage() {
  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_SWEEPING);
    heap()->mark_compact_collector()->sweeper()->EnsureIterabilityCompleted();
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
    for (Page* p :
         PageRange(heap()->new_space()->from_space().first_page(), nullptr)) {
      DCHECK(!p->IsFlagSet(Page::SWEEP_TO_ITERATE));
      non_atomic_marking_state()->ClearLiveness(p);
      if (FLAG_concurrent_marking) {
        // Ensure that concurrent marker does not track pages that are
        // going to be unmapped.
        heap()->concurrent_marking()->ClearMemoryChunkData(p);
      }
    }
    // Since we promote all surviving large objects immediatelly, all remaining
    // large objects must be dead.
    // TODO(ulan): Don't free all as soon as we have an intermediate generation.
    heap()->new_lo_space()->FreeDeadObjects([](HeapObject) { return true; });
  }

  RememberedSet<OLD_TO_NEW>::IterateMemoryChunks(
      heap(), [](MemoryChunk* chunk) {
        if (chunk->SweepingDone()) {
          RememberedSet<OLD_TO_NEW>::FreeEmptyBuckets(chunk);
        } else {
          RememberedSet<OLD_TO_NEW>::PreFreeEmptyBuckets(chunk);
        }
      });

  heap()->account_external_memory_concurrently_freed();
}

void MinorMarkCompactCollector::MakeIterable(
    Page* p, MarkingTreatmentMode marking_mode,
    FreeSpaceTreatmentMode free_space_mode) {
  CHECK(!p->IsLargePage());
  // We have to clear the full collectors markbits for the areas that we
  // remove here.
  MarkCompactCollector* full_collector = heap()->mark_compact_collector();
  Address free_start = p->area_start();

  for (auto object_and_size :
       LiveObjectRange<kGreyObjects>(p, marking_state()->bitmap(p))) {
    HeapObject const object = object_and_size.first;
    DCHECK(non_atomic_marking_state()->IsGrey(object));
    Address free_end = object.address();
    if (free_end != free_start) {
      CHECK_GT(free_end, free_start);
      size_t size = static_cast<size_t>(free_end - free_start);
      full_collector->non_atomic_marking_state()->bitmap(p)->ClearRange(
          p->AddressToMarkbitIndex(free_start),
          p->AddressToMarkbitIndex(free_end));
      if (free_space_mode == ZAP_FREE_SPACE) {
        ZapCode(free_start, size);
      }
      p->heap()->CreateFillerObjectAt(free_start, static_cast<int>(size),
                                      ClearRecordedSlots::kNo);
    }
    Map map = object.synchronized_map();
    int size = object.SizeFromMap(map);
    free_start = free_end + size;
  }

  if (free_start != p->area_end()) {
    CHECK_GT(p->area_end(), free_start);
    size_t size = static_cast<size_t>(p->area_end() - free_start);
    full_collector->non_atomic_marking_state()->bitmap(p)->ClearRange(
        p->AddressToMarkbitIndex(free_start),
        p->AddressToMarkbitIndex(p->area_end()));
    if (free_space_mode == ZAP_FREE_SPACE) {
      ZapCode(free_start, size);
    }
    p->heap()->CreateFillerObjectAt(free_start, static_cast<int>(size),
                                    ClearRecordedSlots::kNo);
  }

  if (marking_mode == MarkingTreatmentMode::CLEAR) {
    non_atomic_marking_state()->ClearLiveness(p);
    p->ClearFlag(Page::SWEEP_TO_ITERATE);
  }
}

namespace {

// Helper class for pruning the string table.
class YoungGenerationExternalStringTableCleaner : public RootVisitor {
 public:
  YoungGenerationExternalStringTableCleaner(
      MinorMarkCompactCollector* collector)
      : heap_(collector->heap()),
        marking_state_(collector->non_atomic_marking_state()) {}

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    DCHECK_EQ(static_cast<int>(root),
              static_cast<int>(Root::kExternalStringsTable));
    // Visit all HeapObject pointers in [start, end).
    for (FullObjectSlot p = start; p < end; ++p) {
      Object o = *p;
      if (o.IsHeapObject()) {
        HeapObject heap_object = HeapObject::cast(o);
        if (marking_state_->IsWhite(heap_object)) {
          if (o.IsExternalString()) {
            heap_->FinalizeExternalString(String::cast(*p));
          } else {
            // The original external string may have been internalized.
            DCHECK(o.IsThinString());
          }
          // Set the entry to the_hole_value (as deleted).
          p.store(ReadOnlyRoots(heap_).the_hole_value());
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
      : marking_state_(collector->non_atomic_marking_state()) {}

  Object RetainAs(Object object) override {
    HeapObject heap_object = HeapObject::cast(object);
    if (!Heap::InYoungGeneration(heap_object)) return object;

    // Young generation marking only marks to grey instead of black.
    DCHECK(!marking_state_->IsBlack(heap_object));
    if (marking_state_->IsGrey(heap_object)) {
      return object;
    }
    return Object();
  }

 private:
  MinorMarkCompactCollector::NonAtomicMarkingState* marking_state_;
};

}  // namespace

void MinorMarkCompactCollector::ClearNonLiveReferences() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_CLEAR);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_CLEAR_STRING_TABLE);
    // Internalized strings are always stored in old space, so there is no need
    // to clean them here.
    YoungGenerationExternalStringTableCleaner external_visitor(this);
    heap()->external_string_table_.IterateYoung(&external_visitor);
    heap()->external_string_table_.CleanUpYoung();
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
  for (Page* p :
       PageRange(new_space->first_allocatable_address(), new_space->top())) {
    new_space_evacuation_pages_.push_back(p);
  }
  new_space->Flip();
  new_space->ResetLinearAllocationArea();

  heap()->new_lo_space()->Flip();
  heap()->new_lo_space()->ResetPendingObject();
}

void MinorMarkCompactCollector::EvacuateEpilogue() {
  heap()->new_space()->set_age_mark(heap()->new_space()->top());
  // Give pages that are queued to be freed back to the OS.
  heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
}

UpdatingItem* MinorMarkCompactCollector::CreateToSpaceUpdatingItem(
    MemoryChunk* chunk, Address start, Address end) {
  return new ToSpaceUpdatingItem<NonAtomicMarkingState>(
      chunk, start, end, non_atomic_marking_state());
}

UpdatingItem* MinorMarkCompactCollector::CreateRememberedSetUpdatingItem(
    MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) {
  return new RememberedSetUpdatingItem<NonAtomicMarkingState>(
      heap(), non_atomic_marking_state(), chunk, updating_mode);
}

class MarkingItem;
class PageMarkingItem;
class RootMarkingItem;
class YoungGenerationMarkingTask;

class MarkingItem : public ItemParallelJob::Item {
 public:
  ~MarkingItem() override = default;
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
        visitor_(marking_state_, global_worklist, task_id) {
    local_live_bytes_.reserve(isolate->heap()->new_space()->Capacity() /
                              Page::kPageSize);
  }

  void RunInParallel() override {
    TRACE_BACKGROUND_GC(collector_->heap()->tracer(),
                        GCTracer::BackgroundScope::MINOR_MC_BACKGROUND_MARKING);
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
  }

  void MarkObject(Object object) {
    if (!Heap::InYoungGeneration(object)) return;
    HeapObject heap_object = HeapObject::cast(object);
    if (marking_state_->WhiteToGrey(heap_object)) {
      const int size = visitor_.Visit(heap_object);
      IncrementLiveBytes(heap_object, size);
    }
  }

 private:
  void EmptyLocalMarkingWorklist() {
    HeapObject object;
    while (marking_worklist_.Pop(&object)) {
      const int size = visitor_.Visit(object);
      IncrementLiveBytes(object, size);
    }
  }

  void EmptyMarkingWorklist() {
    HeapObject object;
    while (marking_worklist_.Pop(&object)) {
      const int size = visitor_.Visit(object);
      IncrementLiveBytes(object, size);
    }
  }

  void IncrementLiveBytes(HeapObject object, intptr_t bytes) {
    local_live_bytes_[Page::FromHeapObject(object)] += bytes;
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

class PageMarkingItem : public MarkingItem {
 public:
  explicit PageMarkingItem(MemoryChunk* chunk, std::atomic<int>* global_slots)
      : chunk_(chunk), global_slots_(global_slots), slots_(0) {}
  ~PageMarkingItem() override { *global_slots_ = *global_slots_ + slots_; }

  void Process(YoungGenerationMarkingTask* task) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                 "PageMarkingItem::Process");
    base::MutexGuard guard(chunk_->mutex());
    MarkUntypedPointers(task);
    MarkTypedPointers(task);
  }

 private:
  inline Heap* heap() { return chunk_->heap(); }

  void MarkUntypedPointers(YoungGenerationMarkingTask* task) {
    RememberedSet<OLD_TO_NEW>::Iterate(chunk_,
                                       [this, task](MaybeObjectSlot slot) {
                                         return CheckAndMarkObject(task, slot);
                                       },
                                       SlotSet::PREFREE_EMPTY_BUCKETS);
  }

  void MarkTypedPointers(YoungGenerationMarkingTask* task) {
    RememberedSet<OLD_TO_NEW>::IterateTyped(
        chunk_, [=](SlotType slot_type, Address slot) {
          return UpdateTypedSlotHelper::UpdateTypedSlot(
              heap(), slot_type, slot, [this, task](FullMaybeObjectSlot slot) {
                return CheckAndMarkObject(task, slot);
              });
        });
  }

  template <typename TSlot>
  V8_INLINE SlotCallbackResult
  CheckAndMarkObject(YoungGenerationMarkingTask* task, TSlot slot) {
    static_assert(
        std::is_same<TSlot, FullMaybeObjectSlot>::value ||
            std::is_same<TSlot, MaybeObjectSlot>::value,
        "Only FullMaybeObjectSlot and MaybeObjectSlot are expected here");
    MaybeObject object = *slot;
    if (Heap::InYoungGeneration(object)) {
      // Marking happens before flipping the young generation, so the object
      // has to be in a to page.
      DCHECK(Heap::InToPage(object));
      HeapObject heap_object;
      bool success = object.GetHeapObject(&heap_object);
      USE(success);
      DCHECK(success);
      task->MarkObject(heap_object);
      slots_++;
      return KEEP_SLOT;
    }
    return REMOVE_SLOT;
  }

  MemoryChunk* chunk_;
  std::atomic<int>* global_slots_;
  int slots_;
};

void MinorMarkCompactCollector::MarkRootSetInParallel(
    RootMarkingVisitor* root_visitor) {
  std::atomic<int> slots;
  {
    ItemParallelJob job(isolate()->cancelable_task_manager(),
                        &page_parallel_job_semaphore_);

    // Seed the root set (roots + old->new set).
    {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_SEED);
      isolate()->global_handles()->IdentifyWeakUnmodifiedObjects(
          &JSObject::IsUnmodifiedApiObject);
      heap()->IterateRoots(root_visitor, VISIT_ALL_IN_MINOR_MC_MARK);
      // Create items for each page.
      RememberedSet<OLD_TO_NEW>::IterateMemoryChunks(
          heap(), [&job, &slots](MemoryChunk* chunk) {
            job.AddItem(new PageMarkingItem(chunk, &slots));
          });
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
      DCHECK(worklist()->IsEmpty());
    }
  }
  old_to_new_slots_ = slots;
}

void MinorMarkCompactCollector::MarkLiveObjects() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK);

  PostponeInterruptsScope postpone(isolate());

  RootMarkingVisitor root_visitor(this);

  MarkRootSetInParallel(&root_visitor);

  // Mark rest on the main thread.
  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_WEAK);
    ProcessMarkingWorklist();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_GLOBAL_HANDLES);
    isolate()->global_handles()->MarkYoungWeakUnmodifiedObjectsPending(
        &IsUnmarkedObjectForYoungGeneration);
    isolate()->global_handles()->IterateYoungWeakUnmodifiedRootsForFinalizers(
        &root_visitor);
    isolate()
        ->global_handles()
        ->IterateYoungWeakUnmodifiedRootsForPhantomHandles(
            &root_visitor, &IsUnmarkedObjectForYoungGeneration);
    ProcessMarkingWorklist();
  }
}

void MinorMarkCompactCollector::ProcessMarkingWorklist() {
  MarkingWorklist::View marking_worklist(worklist(), kMainMarker);
  HeapObject object;
  while (marking_worklist.Pop(&object)) {
    DCHECK(!object.IsFiller());
    DCHECK(object.IsHeapObject());
    DCHECK(heap()->Contains(object));
    DCHECK(non_atomic_marking_state()->IsGrey(object));
    main_marking_visitor()->Visit(object);
  }
  DCHECK(marking_worklist.IsLocalEmpty());
}

void MinorMarkCompactCollector::Evacuate() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_EVACUATE);
  base::MutexGuard guard(heap()->relocation_mutex());

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
      heap()->FatalProcessOutOfMemory("NewSpace::Rebalance");
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

namespace {

class YoungGenerationEvacuator : public Evacuator {
 public:
  explicit YoungGenerationEvacuator(MinorMarkCompactCollector* collector)
      : Evacuator(collector->heap(), &record_visitor_),
        record_visitor_(collector->heap()->mark_compact_collector()),
        collector_(collector) {}

  GCTracer::BackgroundScope::ScopeId GetBackgroundTracingScope() override {
    return GCTracer::BackgroundScope::MINOR_MC_BACKGROUND_EVACUATE_COPY;
  }

 protected:
  void RawEvacuatePage(MemoryChunk* chunk, intptr_t* live_bytes) override;

  YoungGenerationRecordMigratedSlotVisitor record_visitor_;
  MinorMarkCompactCollector* collector_;
};

void YoungGenerationEvacuator::RawEvacuatePage(MemoryChunk* chunk,
                                               intptr_t* live_bytes) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
               "YoungGenerationEvacuator::RawEvacuatePage");
  MinorMarkCompactCollector::NonAtomicMarkingState* marking_state =
      collector_->non_atomic_marking_state();
  *live_bytes = marking_state->live_bytes(chunk);
  switch (ComputeEvacuationMode(chunk)) {
    case kObjectsNewToOld:
      LiveObjectVisitor::VisitGreyObjectsNoFail(
          chunk, marking_state, &new_space_visitor_,
          LiveObjectVisitor::kClearMarkbits);
      // ArrayBufferTracker will be updated during pointers updating.
      break;
    case kPageNewToOld:
      LiveObjectVisitor::VisitGreyObjectsNoFail(
          chunk, marking_state, &new_to_old_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      new_to_old_page_visitor_.account_moved_bytes(
          marking_state->live_bytes(chunk));
      if (!chunk->IsLargePage()) {
        // TODO(mlippautz): If cleaning array buffers is too slow here we can
        // delay it until the next GC.
        ArrayBufferTracker::FreeDead(static_cast<Page*>(chunk), marking_state);
        if (heap()->ShouldZapGarbage()) {
          collector_->MakeIterable(static_cast<Page*>(chunk),
                                   MarkingTreatmentMode::KEEP, ZAP_FREE_SPACE);
        } else if (heap()->incremental_marking()->IsMarking()) {
          // When incremental marking is on, we need to clear the mark bits of
          // the full collector. We cannot yet discard the young generation mark
          // bits as they are still relevant for pointers updating.
          collector_->MakeIterable(static_cast<Page*>(chunk),
                                   MarkingTreatmentMode::KEEP,
                                   IGNORE_FREE_SPACE);
        }
      }
      break;
    case kPageNewToNew:
      LiveObjectVisitor::VisitGreyObjectsNoFail(
          chunk, marking_state, &new_to_new_page_visitor_,
          LiveObjectVisitor::kKeepMarking);
      new_to_new_page_visitor_.account_moved_bytes(
          marking_state->live_bytes(chunk));
      DCHECK(!chunk->IsLargePage());
      // TODO(mlippautz): If cleaning array buffers is too slow here we can
      // delay it until the next GC.
      ArrayBufferTracker::FreeDead(static_cast<Page*>(chunk), marking_state);
      if (heap()->ShouldZapGarbage()) {
        collector_->MakeIterable(static_cast<Page*>(chunk),
                                 MarkingTreatmentMode::KEEP, ZAP_FREE_SPACE);
      } else if (heap()->incremental_marking()->IsMarking()) {
        // When incremental marking is on, we need to clear the mark bits of
        // the full collector. We cannot yet discard the young generation mark
        // bits as they are still relevant for pointers updating.
        collector_->MakeIterable(static_cast<Page*>(chunk),
                                 MarkingTreatmentMode::KEEP, IGNORE_FREE_SPACE);
      }
      break;
    case kObjectsOldToOld:
      UNREACHABLE();
  }
}

}  // namespace

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
    evacuation_job.AddItem(new EvacuationItem(page));
  }

  // Promote young generation large objects.
  for (auto it = heap()->new_lo_space()->begin();
       it != heap()->new_lo_space()->end();) {
    LargePage* current = *it;
    it++;
    HeapObject object = current->GetObject();
    DCHECK(!non_atomic_marking_state_.IsBlack(object));
    if (non_atomic_marking_state_.IsGrey(object)) {
      heap_->lo_space()->PromoteNewLargeObject(current);
      current->SetFlag(Page::PAGE_NEW_OLD_PROMOTION);
      evacuation_job.AddItem(new EvacuationItem(current));
    }
  }
  if (evacuation_job.NumberOfItems() == 0) return;

  YoungGenerationMigrationObserver observer(heap(),
                                            heap()->mark_compact_collector());
  CreateAndExecuteEvacuationTasks<YoungGenerationEvacuator>(
      this, &evacuation_job, &observer, live_bytes);
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

#endif  // ENABLE_MINOR_MC

}  // namespace internal
}  // namespace v8

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/mark-compact.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/base/utils/random-number-generator.h"
#include "src/codegen/compilation-cache.h"
#include "src/common/globals.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/execution.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-utils-inl.h"
#include "src/execution/isolate-utils.h"
#include "src/execution/vm-state-inl.h"
#include "src/handles/global-handles.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/code-object-registry.h"
#include "src/heap/concurrent-allocator.h"
#include "src/heap/evacuation-allocator-inl.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/global-handle-marking-visitor.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/index-generator.h"
#include "src/heap/invalidated-slots-inl.h"
#include "src/heap/invalidated-slots.h"
#include "src/heap/large-spaces.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/marking-barrier.h"
#include "src/heap/marking-visitor-inl.h"
#include "src/heap/marking-visitor.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/memory-measurement-inl.h"
#include "src/heap/memory-measurement.h"
#include "src/heap/new-spaces.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/parallel-work-item.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/read-only-spaces.h"
#include "src/heap/remembered-set.h"
#include "src/heap/safepoint.h"
#include "src/heap/slot-set.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/sweeper.h"
#include "src/heap/weak-object-worklists.h"
#include "src/init/v8.h"
#include "src/logging/tracing-flags.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/foreign.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects.h"
#include "src/objects/slots-inl.h"
#include "src/objects/smi.h"
#include "src/objects/string-forwarding-table-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/objects/visitors.h"
#include "src/snapshot/shared-heap-serializer.h"
#include "src/tasks/cancelable-task.h"
#include "src/tracing/tracing-category-observer.h"
#include "src/utils/utils-inl.h"

namespace v8 {
namespace internal {

const char* Marking::kWhiteBitPattern = "00";
const char* Marking::kBlackBitPattern = "11";
const char* Marking::kGreyBitPattern = "10";
const char* Marking::kImpossibleBitPattern = "01";

// The following has to hold in order for {MarkingState::MarkBitFrom} to not
// produce invalid {kImpossibleBitPattern} in the marking bitmap by overlapping.
static_assert(Heap::kMinObjectSizeInTaggedWords >= 2);

// =============================================================================
// Verifiers
// =============================================================================

#ifdef VERIFY_HEAP
namespace {

class MarkingVerifier : public ObjectVisitorWithCageBases, public RootVisitor {
 public:
  virtual void Run() = 0;

 protected:
  explicit MarkingVerifier(Heap* heap)
      : ObjectVisitorWithCageBases(heap), heap_(heap) {}

  virtual ConcurrentBitmap<AccessMode::NON_ATOMIC>* bitmap(
      const MemoryChunk* chunk) = 0;

  virtual void VerifyMap(Map map) = 0;
  virtual void VerifyPointers(ObjectSlot start, ObjectSlot end) = 0;
  virtual void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) = 0;
  virtual void VerifyCodePointer(CodeObjectSlot slot) = 0;
  virtual void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) = 0;

  virtual bool IsBlack(HeapObject object) = 0;

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    VerifyPointers(start, end);
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {
    VerifyPointers(start, end);
  }

  void VisitCodePointer(HeapObject host, CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    VerifyCodePointer(slot);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    VerifyRootPointers(start, end);
  }

  void VisitMapPointer(HeapObject object) override {
    VerifyMap(object.map(cage_base()));
  }

  void VerifyRoots();
  void VerifyMarkingOnPage(const Page* page, Address start, Address end);
  void VerifyMarking(NewSpace* new_space);
  void VerifyMarking(PagedSpaceBase* paged_space);
  void VerifyMarking(LargeObjectSpace* lo_space);

  Heap* heap_;
};

void MarkingVerifier::VerifyRoots() {
  heap_->IterateRootsIncludingClients(this,
                                      base::EnumSet<SkipRoot>{SkipRoot::kWeak});
}

void MarkingVerifier::VerifyMarkingOnPage(const Page* page, Address start,
                                          Address end) {
  Address next_object_must_be_here_or_later = start;

  for (auto object_and_size :
       LiveObjectRange<kAllLiveObjects>(page, bitmap(page))) {
    HeapObject object = object_and_size.first;
    size_t size = object_and_size.second;
    Address current = object.address();
    if (current < start) continue;
    if (current >= end) break;
    CHECK(IsBlack(object));
    CHECK(current >= next_object_must_be_here_or_later);
    object.Iterate(cage_base(), this);
    next_object_must_be_here_or_later = current + size;
    // The object is either part of a black area of black allocation or a
    // regular black object
    CHECK(bitmap(page)->AllBitsSetInRange(
              page->AddressToMarkbitIndex(current),
              page->AddressToMarkbitIndex(next_object_must_be_here_or_later)) ||
          bitmap(page)->AllBitsClearInRange(
              page->AddressToMarkbitIndex(current + kTaggedSize * 2),
              page->AddressToMarkbitIndex(next_object_must_be_here_or_later)));
    current = next_object_must_be_here_or_later;
  }
}

void MarkingVerifier::VerifyMarking(NewSpace* space) {
  if (!space) return;
  if (v8_flags.minor_mc) {
    VerifyMarking(PagedNewSpace::From(space)->paged_space());
    return;
  }
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

void MarkingVerifier::VerifyMarking(PagedSpaceBase* space) {
  for (Page* p : *space) {
    VerifyMarkingOnPage(p, p->area_start(), p->area_end());
  }
}

void MarkingVerifier::VerifyMarking(LargeObjectSpace* lo_space) {
  if (!lo_space) return;
  LargeObjectSpaceObjectIterator it(lo_space);
  for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
    if (IsBlack(obj)) {
      obj.Iterate(cage_base(), this);
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
    VerifyRoots();
    VerifyMarking(heap_->new_space());
    VerifyMarking(heap_->new_lo_space());
    VerifyMarking(heap_->old_space());
    VerifyMarking(heap_->code_space());
    if (heap_->map_space()) VerifyMarking(heap_->map_space());
    VerifyMarking(heap_->lo_space());
    VerifyMarking(heap_->code_lo_space());
  }

 protected:
  ConcurrentBitmap<AccessMode::NON_ATOMIC>* bitmap(
      const MemoryChunk* chunk) override {
    return marking_state_->bitmap(chunk);
  }

  bool IsBlack(HeapObject object) override {
    return marking_state_->IsBlack(object);
  }

  void VerifyMap(Map map) override { VerifyHeapObjectImpl(map); }

  void VerifyPointers(ObjectSlot start, ObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }

  void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }

  void VerifyCodePointer(CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    Object maybe_code = slot.load(code_cage_base());
    HeapObject code;
    // The slot might contain smi during CodeDataContainer creation, so skip it.
    if (maybe_code.GetHeapObject(&code)) {
      VerifyHeapObjectImpl(code);
    }
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
    HeapObject target_object = rinfo->target_object(cage_base());
    if (!host.IsWeakObject(target_object)) {
      VerifyHeapObjectImpl(target_object);
    }
  }

 private:
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object) {
    if (heap_->IsShared() !=
        BasicMemoryChunk::FromHeapObject(heap_object)->InSharedHeap())
      return;

    if (heap_->MustBeInSharedOldSpace(heap_object)) {
      CHECK(heap_->SharedHeapContains(heap_object));
    }

    CHECK(marking_state_->IsBlack(heap_object));
  }

  template <typename TSlot>
  V8_INLINE void VerifyPointersImpl(TSlot start, TSlot end) {
    for (TSlot slot = start; slot < end; ++slot) {
      typename TSlot::TObject object = slot.load(cage_base());
      HeapObject heap_object;
      if (object.GetHeapObjectIfStrong(&heap_object)) {
        VerifyHeapObjectImpl(heap_object);
      }
    }
  }

  NonAtomicMarkingState* marking_state_;
};

class EvacuationVerifier : public ObjectVisitorWithCageBases,
                           public RootVisitor {
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

  void VisitCodePointer(HeapObject host, CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    VerifyCodePointer(slot);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    VerifyRootPointers(start, end);
  }

  void VisitMapPointer(HeapObject object) override {
    VerifyMap(object.map(cage_base()));
  }

 protected:
  explicit EvacuationVerifier(Heap* heap)
      : ObjectVisitorWithCageBases(heap), heap_(heap) {}

  inline Heap* heap() { return heap_; }

  virtual void VerifyMap(Map map) = 0;
  virtual void VerifyPointers(ObjectSlot start, ObjectSlot end) = 0;
  virtual void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) = 0;
  virtual void VerifyCodePointer(CodeObjectSlot slot) = 0;
  virtual void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) = 0;

  void VerifyRoots();
  void VerifyEvacuationOnPage(Address start, Address end);
  void VerifyEvacuation(NewSpace* new_space);
  void VerifyEvacuation(PagedSpaceBase* paged_space);

  Heap* heap_;
};

void EvacuationVerifier::VerifyRoots() {
  heap_->IterateRootsIncludingClients(this,
                                      base::EnumSet<SkipRoot>{SkipRoot::kWeak});
}

void EvacuationVerifier::VerifyEvacuationOnPage(Address start, Address end) {
  Address current = start;
  while (current < end) {
    HeapObject object = HeapObject::FromAddress(current);
    if (!object.IsFreeSpaceOrFiller(cage_base())) {
      object.Iterate(cage_base(), this);
    }
    current += object.Size(cage_base());
  }
}

void EvacuationVerifier::VerifyEvacuation(NewSpace* space) {
  if (!space) return;
  if (v8_flags.minor_mc) {
    VerifyEvacuation(PagedNewSpace::From(space)->paged_space());
    return;
  }
  PageRange range(space->first_allocatable_address(), space->top());
  for (auto it = range.begin(); it != range.end();) {
    Page* page = *(it++);
    Address current = page->area_start();
    Address limit = it != range.end() ? page->area_end() : space->top();
    CHECK(limit == space->top() || !page->Contains(space->top()));
    VerifyEvacuationOnPage(current, limit);
  }
}

void EvacuationVerifier::VerifyEvacuation(PagedSpaceBase* space) {
  for (Page* p : *space) {
    if (p->IsEvacuationCandidate()) continue;
    if (p->Contains(space->top())) {
      CodePageMemoryModificationScope memory_modification_scope(p);
      heap_->CreateFillerObjectAt(
          space->top(), static_cast<int>(space->limit() - space->top()));
    }
    VerifyEvacuationOnPage(p->area_start(), p->area_end());
  }
}

class FullEvacuationVerifier : public EvacuationVerifier {
 public:
  explicit FullEvacuationVerifier(Heap* heap) : EvacuationVerifier(heap) {}

  void Run() override {
    DCHECK(!heap_->mark_compact_collector()->sweeping_in_progress());
    VerifyRoots();
    VerifyEvacuation(heap_->new_space());
    VerifyEvacuation(heap_->old_space());
    VerifyEvacuation(heap_->code_space());
    if (heap_->map_space()) VerifyEvacuation(heap_->map_space());
  }

 protected:
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object) {
    if (heap_->IsShared() !=
        BasicMemoryChunk::FromHeapObject(heap_object)->InSharedHeap())
      return;

    CHECK_IMPLIES(Heap::InYoungGeneration(heap_object),
                  Heap::InToPage(heap_object));
    CHECK(!MarkCompactCollector::IsOnEvacuationCandidate(heap_object));
  }

  template <typename TSlot>
  void VerifyPointersImpl(TSlot start, TSlot end) {
    for (TSlot current = start; current < end; ++current) {
      typename TSlot::TObject object = current.load(cage_base());
      HeapObject heap_object;
      if (object.GetHeapObjectIfStrong(&heap_object)) {
        VerifyHeapObjectImpl(heap_object);
      }
    }
  }
  void VerifyMap(Map map) override { VerifyHeapObjectImpl(map); }
  void VerifyPointers(ObjectSlot start, ObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }
  void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }
  void VerifyCodePointer(CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    Object maybe_code = slot.load(code_cage_base());
    HeapObject code;
    // The slot might contain smi during CodeDataContainer creation, so skip it.
    if (maybe_code.GetHeapObject(&code)) {
      VerifyHeapObjectImpl(code);
    }
  }
  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    VerifyHeapObjectImpl(target);
  }
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    VerifyHeapObjectImpl(rinfo->target_object(cage_base()));
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

namespace {

int NumberOfAvailableCores() {
  static int num_cores = V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1;
  // This number of cores should be greater than zero and never change.
  DCHECK_GE(num_cores, 1);
  DCHECK_EQ(num_cores, V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1);
  return num_cores;
}

int NumberOfParallelCompactionTasks(Heap* heap) {
  int tasks = v8_flags.parallel_compaction ? NumberOfAvailableCores() : 1;
  if (!heap->CanPromoteYoungAndExpandOldGeneration(
          static_cast<size_t>(tasks * Page::kPageSize))) {
    // Optimize for memory usage near the heap limit.
    tasks = 1;
  }
  return tasks;
}
}  // namespace

CollectorBase::CollectorBase(Heap* heap, GarbageCollector collector)
    : heap_(heap),
      garbage_collector_(collector),
      marking_state_(heap->isolate()),
      non_atomic_marking_state_(heap->isolate()) {
  DCHECK_NE(GarbageCollector::SCAVENGER, garbage_collector_);
}

bool CollectorBase::IsMajorMC() {
  return !heap_->IsYoungGenerationCollector(garbage_collector_);
}

MarkCompactCollector::MarkCompactCollector(Heap* heap)
    : CollectorBase(heap, GarbageCollector::MARK_COMPACTOR),
#ifdef DEBUG
      state_(IDLE),
#endif
      is_shared_heap_(heap->IsShared()),
      sweeper_(new Sweeper(heap, non_atomic_marking_state())) {
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
  if (heap()->incremental_marking()->IsMajorMarking()) {
    local_marking_worklists()->Publish();
    heap()->main_thread_local_heap()->marking_barrier()->Publish();
    // Marking barriers of LocalHeaps will be published in their destructors.
    marking_worklists()->Clear();
    local_weak_objects()->Publish();
    weak_objects()->Clear();
  }
  sweeper()->TearDown();
}

// static
bool MarkCompactCollector::IsMapOrForwarded(Map map) {
  MapWord map_word = map.map_word(kRelaxedLoad);

  if (map_word.IsForwardingAddress()) {
    // During GC we can't access forwarded maps without synchronization.
    return true;
  } else {
    return map_word.ToMap().IsMap();
  }
}

void MarkCompactCollector::AddEvacuationCandidate(Page* p) {
  DCHECK(!p->NeverEvacuate());

  if (v8_flags.trace_evacuation_candidates) {
    PrintIsolate(
        isolate(),
        "Evacuation candidate: Free bytes: %6zu. Free Lists length: %4d.\n",
        p->area_size() - p->allocated_bytes(), p->FreeListsLength());
  }

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

bool MarkCompactCollector::StartCompaction(StartCompactionMode mode) {
  DCHECK(!compacting_);
  DCHECK(evacuation_candidates_.empty());

  // Bailouts for completely disabled compaction.
  if (!v8_flags.compact ||
      (mode == StartCompactionMode::kAtomic && heap()->IsGCWithStack() &&
       !v8_flags.compact_with_stack) ||
      (v8_flags.gc_experiment_less_compaction &&
       !heap_->ShouldReduceMemory())) {
    return false;
  }

  CollectEvacuationCandidates(heap()->old_space());

  if (heap()->map_space() && v8_flags.compact_maps) {
    CollectEvacuationCandidates(heap()->map_space());
  }

  if (v8_flags.compact_code_space &&
      (!heap()->IsGCWithStack() || v8_flags.compact_code_space_with_stack)) {
    CollectEvacuationCandidates(heap()->code_space());
  } else if (v8_flags.trace_fragmentation) {
    TraceFragmentation(heap()->code_space());
  }

  if (v8_flags.trace_fragmentation && heap()->map_space()) {
    TraceFragmentation(heap()->map_space());
  }

  compacting_ = !evacuation_candidates_.empty();
  return compacting_;
}

void MarkCompactCollector::StartMarking() {
  std::vector<Address> contexts =
      heap()->memory_measurement()->StartProcessing();
  if (v8_flags.stress_per_context_marking_worklist) {
    contexts.clear();
    HandleScope handle_scope(heap()->isolate());
    for (auto context : heap()->FindAllNativeContexts()) {
      contexts.push_back(context->ptr());
    }
  }
  code_flush_mode_ = Heap::GetCodeFlushMode(isolate());
  marking_worklists()->CreateContextWorklists(contexts);
  auto* cpp_heap = CppHeap::From(heap_->cpp_heap());
  local_marking_worklists_ = std::make_unique<MarkingWorklists::Local>(
      marking_worklists(),
      cpp_heap ? cpp_heap->CreateCppMarkingStateForMutatorThread()
               : MarkingWorklists::Local::kNoCppMarkingState);
  local_weak_objects_ = std::make_unique<WeakObjects::Local>(weak_objects());
  marking_visitor_ = std::make_unique<MarkingVisitor>(
      marking_state(), local_marking_worklists(), local_weak_objects_.get(),
      heap_, epoch(), code_flush_mode(),
      heap_->local_embedder_heap_tracer()->InUse(),
      heap_->ShouldCurrentGCKeepAgesUnchanged());
// Marking bits are cleared by the sweeper.
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) {
    VerifyMarkbitsAreClean();
  }
#endif  // VERIFY_HEAP
}

void MarkCompactCollector::CollectGarbage() {
  // Make sure that Prepare() has been called. The individual steps below will
  // update the state as they proceed.
  DCHECK(state_ == PREPARE_GC);

  MarkLiveObjects();
  ClearNonLiveReferences();
  VerifyMarking();
  heap()->memory_measurement()->FinishProcessing(native_context_stats_);
  RecordObjectStats();

  Sweep();
  Evacuate();
  Finish();
}

#ifdef VERIFY_HEAP
void MarkCompactCollector::VerifyMarkbitsAreDirty(ReadOnlySpace* space) {
  ReadOnlyHeapObjectIterator iterator(space);
  for (HeapObject object = iterator.Next(); !object.is_null();
       object = iterator.Next()) {
    CHECK(non_atomic_marking_state()->IsBlack(object));
  }
}

void MarkCompactCollector::VerifyMarkbitsAreClean(PagedSpaceBase* space) {
  for (Page* p : *space) {
    CHECK(non_atomic_marking_state()->bitmap(p)->IsClean());
    CHECK_EQ(0, non_atomic_marking_state()->live_bytes(p));
  }
}

void MarkCompactCollector::VerifyMarkbitsAreClean(NewSpace* space) {
  if (!space) return;
  if (v8_flags.minor_mc) {
    VerifyMarkbitsAreClean(PagedNewSpace::From(space)->paged_space());
    return;
  }
  for (Page* p : PageRange(space->first_allocatable_address(), space->top())) {
    CHECK(non_atomic_marking_state()->bitmap(p)->IsClean());
    CHECK_EQ(0, non_atomic_marking_state()->live_bytes(p));
  }
}

void MarkCompactCollector::VerifyMarkbitsAreClean(LargeObjectSpace* space) {
  if (!space) return;
  LargeObjectSpaceObjectIterator it(space);
  for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
    CHECK(non_atomic_marking_state()->IsWhite(obj));
    CHECK_EQ(0, non_atomic_marking_state()->live_bytes(
                    MemoryChunk::FromHeapObject(obj)));
  }
}

void MarkCompactCollector::VerifyMarkbitsAreClean() {
  VerifyMarkbitsAreClean(heap_->old_space());
  VerifyMarkbitsAreClean(heap_->code_space());
  if (heap_->map_space()) {
    VerifyMarkbitsAreClean(heap_->map_space());
  }
  VerifyMarkbitsAreClean(heap_->new_space());
  // Read-only space should always be black since we never collect any objects
  // in it or linked from it.
  VerifyMarkbitsAreDirty(heap_->read_only_space());
  VerifyMarkbitsAreClean(heap_->lo_space());
  VerifyMarkbitsAreClean(heap_->code_lo_space());
  VerifyMarkbitsAreClean(heap_->new_lo_space());
}

#endif  // VERIFY_HEAP

void MarkCompactCollector::FinishSweepingIfOutOfWork() {
  if (sweeper()->sweeping_in_progress() && v8_flags.concurrent_sweeping &&
      !sweeper()->AreSweeperTasksRunning()) {
    // At this point we know that all concurrent sweeping tasks have run
    // out of work and quit: all pages are swept. The main thread still needs
    // to complete sweeping though.
    EnsureSweepingCompleted(SweepingForcedFinalizationMode::kV8Only);
  }
  if (heap()->cpp_heap()) {
    // Ensure that sweeping is also completed for the C++ managed heap, if one
    // exists and it's out of work.
    CppHeap::From(heap()->cpp_heap())->FinishSweepingIfOutOfWork();
  }
}

void MarkCompactCollector::EnsureSweepingCompleted(
    SweepingForcedFinalizationMode mode) {
  if (sweeper()->sweeping_in_progress()) {
    TRACE_GC_EPOCH(heap()->tracer(), GCTracer::Scope::MC_COMPLETE_SWEEPING,
                   ThreadKind::kMain);

    sweeper()->EnsureCompleted();
    heap()->old_space()->RefillFreeList(sweeper());
    {
      CodePageHeaderModificationScope rwx_write_scope(
          "Updating per-page stats stored in page headers requires write "
          "access to Code page headers");
      heap()->code_space()->RefillFreeList(sweeper());
    }
    if (heap()->map_space()) {
      heap()->map_space()->RefillFreeList(sweeper());
      heap()->map_space()->SortFreeList();
    }

    heap()->tracer()->NotifySweepingCompleted();

#ifdef VERIFY_HEAP
    if (v8_flags.verify_heap && !evacuation()) {
      FullEvacuationVerifier verifier(heap());
      verifier.Run();
    }
#endif
  }

  if (mode == SweepingForcedFinalizationMode::kUnifiedHeap &&
      heap()->cpp_heap()) {
    // Ensure that sweeping is also completed for the C++ managed heap, if one
    // exists.
    CppHeap::From(heap()->cpp_heap())->FinishSweepingIfRunning();
    DCHECK(
        !CppHeap::From(heap()->cpp_heap())->sweeper().IsSweepingInProgress());
  }

  DCHECK_IMPLIES(mode == SweepingForcedFinalizationMode::kUnifiedHeap ||
                     !heap()->cpp_heap(),
                 !heap()->tracer()->IsSweepingInProgress());
}

void MarkCompactCollector::EnsurePageIsSwept(Page* page) {
  sweeper()->EnsurePageIsSwept(page);
}

void MarkCompactCollector::DrainSweepingWorklistForSpace(
    AllocationSpace space) {
  if (!sweeper()->sweeping_in_progress()) return;
  sweeper()->DrainSweepingWorklistForSpace(space);
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
  DCHECK(space->identity() == OLD_SPACE || space->identity() == CODE_SPACE ||
         space->identity() == MAP_SPACE);

  int number_of_pages = space->CountTotalPages();
  size_t area_size = space->AreaSize();

  const bool in_standard_path =
      !(v8_flags.manual_evacuation_candidates_selection ||
        v8_flags.stress_compaction_random || v8_flags.stress_compaction ||
        v8_flags.compact_on_every_full_gc);
  // Those variables will only be initialized if |in_standard_path|, and are not
  // used otherwise.
  size_t max_evacuated_bytes;
  int target_fragmentation_percent;
  size_t free_bytes_threshold;
  if (in_standard_path) {
    // We use two conditions to decide whether a page qualifies as an evacuation
    // candidate, or not:
    // * Target fragmentation: How fragmented is a page, i.e., how is the ratio
    //   between live bytes and capacity of this page (= area).
    // * Evacuation quota: A global quota determining how much bytes should be
    //   compacted.
    ComputeEvacuationHeuristics(area_size, &target_fragmentation_percent,
                                &max_evacuated_bytes);
    free_bytes_threshold = target_fragmentation_percent * (area_size / 100);
  }

  // Pairs of (live_bytes_in_page, page).
  using LiveBytesPagePair = std::pair<size_t, Page*>;
  std::vector<LiveBytesPagePair> pages;
  pages.reserve(number_of_pages);

  CodePageHeaderModificationScope rwx_write_scope(
      "Modification of Code page header flags requires write access");

  DCHECK(!sweeping_in_progress());
  Page* owner_of_linear_allocation_area =
      space->top() == space->limit()
          ? nullptr
          : Page::FromAllocationAreaAddress(space->top());
  for (Page* p : *space) {
    if (p->NeverEvacuate() || (p == owner_of_linear_allocation_area) ||
        !p->CanAllocate())
      continue;

    if (p->IsPinned()) {
      DCHECK(
          !p->IsFlagSet(MemoryChunk::FORCE_EVACUATION_CANDIDATE_FOR_TESTING));
      continue;
    }

    // Invariant: Evacuation candidates are just created when marking is
    // started. This means that sweeping has finished. Furthermore, at the end
    // of a GC all evacuation candidates are cleared and their slot buffers are
    // released.
    CHECK(!p->IsEvacuationCandidate());
    CHECK_NULL(p->slot_set<OLD_TO_OLD>());
    CHECK_NULL(p->typed_slot_set<OLD_TO_OLD>());
    CHECK(p->SweepingDone());
    DCHECK(p->area_size() == area_size);
    if (in_standard_path) {
      // Only the pages with at more than |free_bytes_threshold| free bytes are
      // considered for evacuation.
      if (area_size - p->allocated_bytes() >= free_bytes_threshold) {
        pages.push_back(std::make_pair(p->allocated_bytes(), p));
      }
    } else {
      pages.push_back(std::make_pair(p->allocated_bytes(), p));
    }
  }

  int candidate_count = 0;
  size_t total_live_bytes = 0;

  const bool reduce_memory = heap()->ShouldReduceMemory();
  if (v8_flags.manual_evacuation_candidates_selection) {
    for (size_t i = 0; i < pages.size(); i++) {
      Page* p = pages[i].second;
      if (p->IsFlagSet(MemoryChunk::FORCE_EVACUATION_CANDIDATE_FOR_TESTING)) {
        candidate_count++;
        total_live_bytes += pages[i].first;
        p->ClearFlag(MemoryChunk::FORCE_EVACUATION_CANDIDATE_FOR_TESTING);
        AddEvacuationCandidate(p);
      }
    }
  } else if (v8_flags.stress_compaction_random) {
    double fraction = isolate()->fuzzer_rng()->NextDouble();
    size_t pages_to_mark_count =
        static_cast<size_t>(fraction * (pages.size() + 1));
    for (uint64_t i : isolate()->fuzzer_rng()->NextSample(
             pages.size(), pages_to_mark_count)) {
      candidate_count++;
      total_live_bytes += pages[i].first;
      AddEvacuationCandidate(pages[i].second);
    }
  } else if (v8_flags.stress_compaction) {
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
      if (v8_flags.compact_on_every_full_gc ||
          ((total_live_bytes + live_bytes) <= max_evacuated_bytes)) {
        candidate_count++;
        total_live_bytes += live_bytes;
      }
      if (v8_flags.trace_fragmentation_verbose) {
        PrintIsolate(isolate(),
                     "compaction-selection-page: space=%s free_bytes_page=%zu "
                     "fragmentation_limit_kb=%zu "
                     "fragmentation_limit_percent=%d sum_compaction_kb=%zu "
                     "compaction_limit_kb=%zu\n",
                     space->name(), (area_size - live_bytes) / KB,
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
    if ((estimated_released_pages == 0) && !v8_flags.compact_on_every_full_gc) {
      candidate_count = 0;
    }
    for (int i = 0; i < candidate_count; i++) {
      AddEvacuationCandidate(pages[i].second);
    }
  }

  if (v8_flags.trace_fragmentation) {
    PrintIsolate(isolate(),
                 "compaction-selection: space=%s reduce_memory=%d pages=%d "
                 "total_live_bytes=%zu\n",
                 space->name(), reduce_memory, candidate_count,
                 total_live_bytes / KB);
  }
}

void MarkCompactCollector::AbortCompaction() {
  if (compacting_) {
    CodePageHeaderModificationScope rwx_write_scope(
        "Changing Code page flags and remembered sets require write access "
        "to the page header");
    RememberedSet<OLD_TO_OLD>::ClearAll(heap());
    if (V8_EXTERNAL_CODE_SPACE_BOOL) {
      RememberedSet<OLD_TO_CODE>::ClearAll(heap());
    }
    for (Page* p : evacuation_candidates_) {
      p->ClearEvacuationCandidate();
    }
    compacting_ = false;
    evacuation_candidates_.clear();
  }
  DCHECK(evacuation_candidates_.empty());
}

void MarkCompactCollector::Prepare() {
#ifdef DEBUG
  DCHECK(state_ == IDLE);
  state_ = PREPARE_GC;
#endif

  DCHECK(!sweeping_in_progress());

  // Unmapper tasks needs to be stopped during the GC, otherwise pages queued
  // for freeing might get unmapped during the GC.
  DCHECK(!heap_->memory_allocator()->unmapper()->IsRunning());

  if (!heap()->incremental_marking()->IsMarking()) {
    const auto embedder_flags = heap_->flags_for_embedder_tracer();
    {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_EMBEDDER_PROLOGUE);
      // PrepareForTrace should be called before visitor initialization in
      // StartMarking.
      heap_->local_embedder_heap_tracer()->PrepareForTrace(embedder_flags);
    }
    StartCompaction(StartCompactionMode::kAtomic);
    StartMarking();
    {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_EMBEDDER_PROLOGUE);
      // TracePrologue immediately starts marking which requires V8 worklists to
      // be set up.
      heap_->local_embedder_heap_tracer()->TracePrologue(embedder_flags);
    }
#ifdef V8_COMPRESS_POINTERS
    heap_->isolate()->external_pointer_table().StartCompactingIfNeeded();
#endif  // V8_COMPRESS_POINTERS
  }

  heap_->FreeLinearAllocationAreas();

  PagedSpaceIterator spaces(heap());
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    space->PrepareForMarkCompact();
  }

  // All objects are guaranteed to be initialized in atomic pause
  if (heap()->new_lo_space()) {
    heap()->new_lo_space()->ResetPendingObject();
  }

  NewSpace* new_space = heap()->new_space();
  if (new_space) {
    if (v8_flags.minor_mc) {
      PagedNewSpace::From(new_space)->paged_space()->PrepareForMarkCompact();
    }
    DCHECK_EQ(new_space->top(), new_space->original_top_acquire());
  }
}

void MarkCompactCollector::FinishConcurrentMarking() {
  // FinishConcurrentMarking is called for both, concurrent and parallel,
  // marking. It is safe to call this function when tasks are already finished.
  DCHECK_EQ(heap()->concurrent_marking()->garbage_collector(),
            GarbageCollector::MARK_COMPACTOR);
  if (v8_flags.parallel_marking || v8_flags.concurrent_marking) {
    heap()->concurrent_marking()->Join();
    heap()->concurrent_marking()->FlushMemoryChunkData(
        non_atomic_marking_state());
    heap()->concurrent_marking()->FlushNativeContexts(&native_context_stats_);
  }
  if (auto* cpp_heap = CppHeap::From(heap_->cpp_heap())) {
    cpp_heap->FinishConcurrentMarkingIfNeeded();
  }
}

void MarkCompactCollector::VerifyMarking() {
  CHECK(local_marking_worklists()->IsEmpty());
  DCHECK(heap_->incremental_marking()->IsStopped());
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) {
    FullMarkingVerifier verifier(heap());
    verifier.Run();
  }
#endif
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) {
    heap()->old_space()->VerifyLiveBytes();
    if (heap()->map_space()) heap()->map_space()->VerifyLiveBytes();
    heap()->code_space()->VerifyLiveBytes();
    if (v8_flags.minor_mc && heap()->paged_new_space())
      heap()->paged_new_space()->paged_space()->VerifyLiveBytes();
  }
#endif
}

namespace {

void ShrinkPagesToObjectSizes(Heap* heap, OldLargeObjectSpace* space) {
  size_t surviving_object_size = 0;
  PtrComprCageBase cage_base(heap->isolate());
  for (auto it = space->begin(); it != space->end();) {
    LargePage* current = *(it++);
    HeapObject object = current->GetObject();
    const size_t object_size = static_cast<size_t>(object.Size(cage_base));
    space->ShrinkPageToObjectSize(current, object, object_size);
    surviving_object_size += object_size;
  }
  space->set_objects_size(surviving_object_size);
}

}  // namespace

void MarkCompactCollector::Finish() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_FINISH);

  heap()->isolate()->global_handles()->ClearListOfYoungNodes();

  SweepArrayBufferExtensions();

#ifdef DEBUG
  heap()->VerifyCountersBeforeConcurrentSweeping();
#endif

  marking_visitor_.reset();
  local_marking_worklists_.reset();
  marking_worklists_.ReleaseContextWorklists();
  native_context_stats_.Clear();

  CHECK(weak_objects_.current_ephemerons.IsEmpty());
  CHECK(weak_objects_.discovered_ephemerons.IsEmpty());
  local_weak_objects_->next_ephemerons_local.Publish();
  local_weak_objects_.reset();
  weak_objects_.next_ephemerons.Clear();

  if (heap()->new_lo_space()) {
    GCTracer::Scope sweep_scope(heap()->tracer(),
                                GCTracer::Scope::MC_FINISH_SWEEP_NEW_LO,
                                ThreadKind::kMain);
    SweepLargeSpace(heap()->new_lo_space());
  }

  if (v8_flags.minor_mc && heap()->new_space()) {
    // Keep new space sweeping atomic.
    GCTracer::Scope sweep_scope(heap()->tracer(),
                                GCTracer::Scope::MC_FINISH_SWEEP_NEW,
                                ThreadKind::kMain);
    sweeper()->ParallelSweepSpace(NEW_SPACE,
                                  Sweeper::SweepingMode::kEagerDuringGC, 0);
    heap()->paged_new_space()->paged_space()->RefillFreeList(sweeper());
  }

  sweeper()->StartSweeperTasks();

  // Ensure unmapper tasks are stopped such that queued pages aren't freed
  // before this point. We still need all pages to be accessible for the "update
  // pointers" phase.
  DCHECK(!heap_->memory_allocator()->unmapper()->IsRunning());

  // Shrink pages if possible after processing and filtering slots.
  ShrinkPagesToObjectSizes(heap(), heap()->lo_space());
  ShrinkPagesToObjectSizes(heap(), heap()->code_lo_space());

#ifdef DEBUG
  DCHECK(state_ == SWEEP_SPACES || state_ == RELOCATE_OBJECTS);
  state_ = IDLE;
#endif

  if (have_code_to_deoptimize_) {
    // Some code objects were marked for deoptimization during the GC.
    Deoptimizer::DeoptimizeMarkedCode(isolate());
    have_code_to_deoptimize_ = false;
  }
}

void MarkCompactCollector::SweepArrayBufferExtensions() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_FINISH_SWEEP_ARRAY_BUFFERS);
  heap_->array_buffer_sweeper()->RequestSweep(
      ArrayBufferSweeper::SweepingType::kFull);
}

class MarkCompactCollector::RootMarkingVisitor final : public RootVisitor {
 public:
  explicit RootMarkingVisitor(MarkCompactCollector* collector)
      : collector_(collector), is_shared_heap_(collector->is_shared_heap()) {}

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) final {
    DCHECK(!MapWord::IsPacked(p.Relaxed_Load().ptr()));
    MarkObjectByPointer(root, p);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) final {
    for (FullObjectSlot p = start; p < end; ++p) {
      MarkObjectByPointer(root, p);
    }
  }

  void VisitRunningCode(FullObjectSlot p) final {
    // If Code is currently executing, then we must not remove its
    // deoptimization literals, which it might need in order to successfully
    // deoptimize.
    //
    // Must match behavior in RootsReferencesExtractor::VisitRunningCode, so
    // that heap snapshots accurately describe the roots.
    HeapObject value = HeapObject::cast(*p);
    if (V8_EXTERNAL_CODE_SPACE_BOOL && !IsCodeSpaceObject(value)) {
      // When external code space is enabled, the slot might contain a CodeT
      // object representing an embedded builtin, which doesn't require
      // additional processing.
      DCHECK(CodeT::cast(value).is_off_heap_trampoline());
    } else {
      Code code = Code::cast(value);
      if (code.kind() != CodeKind::BASELINE) {
        DeoptimizationData deopt_data =
            DeoptimizationData::cast(code.deoptimization_data());
        if (deopt_data.length() > 0) {
          DeoptimizationLiteralArray literals = deopt_data.LiteralArray();
          int literals_length = literals.length();
          for (int i = 0; i < literals_length; ++i) {
            MaybeObject maybe_literal = literals.Get(i);
            HeapObject heap_literal;
            if (maybe_literal.GetHeapObject(&heap_literal)) {
              MarkObjectByPointer(Root::kStackRoots,
                                  FullObjectSlot(&heap_literal));
            }
          }
        }
      }
    }
    // And then mark the Code itself.
    VisitRootPointer(Root::kStackRoots, nullptr, p);
  }

 private:
  V8_INLINE void MarkObjectByPointer(Root root, FullObjectSlot p) {
    Object object = *p;
    if (!object.IsHeapObject()) return;
    HeapObject heap_object = HeapObject::cast(object);
    BasicMemoryChunk* target_page =
        BasicMemoryChunk::FromHeapObject(heap_object);
    if (is_shared_heap_ != target_page->InSharedHeap()) return;
    collector_->MarkRootObject(root, heap_object);
  }

  MarkCompactCollector* const collector_;
  const bool is_shared_heap_;
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
// - If V8_ENABLE_SANDBOX, client Isolates' waiter queue node
// ExternalPointer_t in shared Isolates.
class MarkCompactCollector::CustomRootBodyMarkingVisitor final
    : public ObjectVisitorWithCageBases {
 public:
  explicit CustomRootBodyMarkingVisitor(MarkCompactCollector* collector)
      : ObjectVisitorWithCageBases(collector->isolate()),
        collector_(collector) {}

  void VisitPointer(HeapObject host, ObjectSlot p) final {
    MarkObject(host, p.load(cage_base()));
  }

  void VisitMapPointer(HeapObject host) final {
    MarkObject(host, host.map(cage_base()));
  }

  void VisitPointers(HeapObject host, ObjectSlot start, ObjectSlot end) final {
    for (ObjectSlot p = start; p < end; ++p) {
      // The map slot should be handled in VisitMapPointer.
      DCHECK_NE(host.map_slot(), p);
      DCHECK(!HasWeakHeapObjectTag(p.load(cage_base())));
      MarkObject(host, p.load(cage_base()));
    }
  }

  void VisitCodePointer(HeapObject host, CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    MarkObject(host, slot.load(code_cage_base()));
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final {
    // At the moment, custom roots cannot contain weak pointers.
    UNREACHABLE();
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    MarkObject(host, target);
  }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    MarkObject(host, rinfo->target_object(cage_base()));
  }

 private:
  V8_INLINE void MarkObject(HeapObject host, Object object) {
    if (!object.IsHeapObject()) return;
    HeapObject heap_object = HeapObject::cast(object);
    // We use this visitor both in client and shared GCs. The client GC should
    // not mark objects in the shared heap. In shared GCs we are marking each
    // client's top stack frame, so it is actually legal to encounter references
    // into the client heap here in a shared GC. We need to bail out in these
    // cases as well.
    if (collector_->is_shared_heap() != heap_object.InSharedHeap()) return;
    collector_->MarkObject(host, heap_object);
  }

  MarkCompactCollector* const collector_;
};

class MarkCompactCollector::SharedHeapObjectVisitor final
    : public ObjectVisitorWithCageBases {
 public:
  explicit SharedHeapObjectVisitor(MarkCompactCollector* collector)
      : ObjectVisitorWithCageBases(collector->isolate()),
        collector_(collector) {}

  void VisitPointer(HeapObject host, ObjectSlot p) final {
    CheckForSharedObject(host, p, p.load(cage_base()));
  }

  void VisitPointer(HeapObject host, MaybeObjectSlot p) final {
    MaybeObject object = p.load(cage_base());
    HeapObject heap_object;
    if (object.GetHeapObject(&heap_object))
      CheckForSharedObject(host, ObjectSlot(p), heap_object);
  }

  void VisitMapPointer(HeapObject host) final {
    CheckForSharedObject(host, host.map_slot(), host.map(cage_base()));
  }

  void VisitPointers(HeapObject host, ObjectSlot start, ObjectSlot end) final {
    for (ObjectSlot p = start; p < end; ++p) {
      // The map slot should be handled in VisitMapPointer.
      DCHECK_NE(host.map_slot(), p);
      DCHECK(!HasWeakHeapObjectTag(p.load(cage_base())));
      CheckForSharedObject(host, p, p.load(cage_base()));
    }
  }

  void VisitCodePointer(HeapObject host, CodeObjectSlot slot) override {
    UNREACHABLE();
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final {
    for (MaybeObjectSlot p = start; p < end; ++p) {
      // The map slot should be handled in VisitMapPointer.
      DCHECK_NE(host.map_slot(), ObjectSlot(p));
      VisitPointer(host, p);
    }
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override { UNREACHABLE(); }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    UNREACHABLE();
  }

 private:
  V8_INLINE void CheckForSharedObject(HeapObject host, ObjectSlot slot,
                                      Object object) {
    DCHECK(!host.InSharedHeap());
    if (!object.IsHeapObject()) return;
    HeapObject heap_object = HeapObject::cast(object);
    if (!heap_object.InSharedWritableHeap()) return;
    DCHECK(heap_object.InSharedWritableHeap());
    MemoryChunk* host_chunk = MemoryChunk::FromHeapObject(host);
    DCHECK(host_chunk->InYoungGeneration());
    RememberedSet<OLD_TO_SHARED>::Insert<AccessMode::NON_ATOMIC>(
        host_chunk, slot.address());
    collector_->MarkRootObject(Root::kClientHeap, heap_object);
  }

  MarkCompactCollector* const collector_;
};

class InternalizedStringTableCleaner final : public RootVisitor {
 public:
  explicit InternalizedStringTableCleaner(Heap* heap) : heap_(heap) {}

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    UNREACHABLE();
  }

  void VisitRootPointers(Root root, const char* description,
                         OffHeapObjectSlot start,
                         OffHeapObjectSlot end) override {
    DCHECK_EQ(root, Root::kStringTable);
    // Visit all HeapObject pointers in [start, end).
    auto* marking_state = heap_->mark_compact_collector()->marking_state();
    Isolate* isolate = heap_->isolate();
    for (OffHeapObjectSlot p = start; p < end; ++p) {
      Object o = p.load(isolate);
      if (o.IsHeapObject()) {
        HeapObject heap_object = HeapObject::cast(o);
        DCHECK(!Heap::InYoungGeneration(heap_object));
        if (marking_state->IsWhite(heap_object)) {
          pointers_removed_++;
          // Set the entry to the_hole_value (as deleted).
          p.store(StringTable::deleted_element());
        }
      }
    }
  }

  int PointersRemoved() const { return pointers_removed_; }

 private:
  Heap* heap_;
  int pointers_removed_ = 0;
};

class ExternalStringTableCleaner : public RootVisitor {
 public:
  explicit ExternalStringTableCleaner(Heap* heap) : heap_(heap) {}

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    // Visit all HeapObject pointers in [start, end).
    NonAtomicMarkingState* marking_state =
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

#ifdef V8_ENABLE_SANDBOX
class MarkExternalPointerFromExternalStringTable : public RootVisitor {
 public:
  explicit MarkExternalPointerFromExternalStringTable(
      ExternalPointerTable* shared_table)
      : visitor(shared_table) {}

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    // Visit all HeapObject pointers in [start, end).
    for (FullObjectSlot p = start; p < end; ++p) {
      Object o = *p;
      if (o.IsHeapObject()) {
        HeapObject heap_object = HeapObject::cast(o);
        if (heap_object.IsExternalString()) {
          ExternalString string = ExternalString::cast(heap_object);
          string.VisitExternalPointers(&visitor);
        } else {
          // The original external string may have been internalized.
          DCHECK(o.IsThinString());
        }
      }
    }
  }

 private:
  class MarkExternalPointerTableVisitor : public ObjectVisitor {
   public:
    explicit MarkExternalPointerTableVisitor(ExternalPointerTable* table)
        : table_(table) {}
    void VisitExternalPointer(HeapObject host, ExternalPointerSlot slot,
                              ExternalPointerTag tag) override {
      if (!IsSandboxedExternalPointerType(tag)) return;
      DCHECK(IsSharedExternalPointerType(tag));
      ExternalPointerHandle handle = slot.Relaxed_LoadHandle();
      table_->Mark(handle, slot.address());
    }
    void VisitPointers(HeapObject host, ObjectSlot start,
                       ObjectSlot end) override {
      UNREACHABLE();
    }
    void VisitPointers(HeapObject host, MaybeObjectSlot start,
                       MaybeObjectSlot end) override {
      UNREACHABLE();
    }
    void VisitCodePointer(HeapObject host, CodeObjectSlot slot) override {
      UNREACHABLE();
    }
    void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
      UNREACHABLE();
    }
    void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
      UNREACHABLE();
    }

   private:
    ExternalPointerTable* table_;
  };

  MarkExternalPointerTableVisitor visitor;
};
#endif

// Implementation of WeakObjectRetainer for mark compact GCs. All marked objects
// are retained.
class MarkCompactWeakObjectRetainer : public WeakObjectRetainer {
 public:
  explicit MarkCompactWeakObjectRetainer(MarkingState* marking_state)
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
  MarkingState* const marking_state_;
};

class RecordMigratedSlotVisitor : public ObjectVisitorWithCageBases {
 public:
  explicit RecordMigratedSlotVisitor(
      MarkCompactCollector* collector,
      EphemeronRememberedSet* ephemeron_remembered_set)
      : ObjectVisitorWithCageBases(collector->isolate()),
        collector_(collector),
        ephemeron_remembered_set_(ephemeron_remembered_set) {}

  inline void VisitPointer(HeapObject host, ObjectSlot p) final {
    DCHECK(!HasWeakHeapObjectTag(p.load(cage_base())));
    RecordMigratedSlot(host, MaybeObject::FromObject(p.load(cage_base())),
                       p.address());
  }

  inline void VisitMapPointer(HeapObject host) final {
    VisitPointer(host, host.map_slot());
  }

  inline void VisitPointer(HeapObject host, MaybeObjectSlot p) final {
    DCHECK(!MapWord::IsPacked(p.Relaxed_Load(cage_base()).ptr()));
    RecordMigratedSlot(host, p.load(cage_base()), p.address());
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

  inline void VisitCodePointer(HeapObject host, CodeObjectSlot slot) final {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    // This code is similar to the implementation of VisitPointer() modulo
    // new kind of slot.
    DCHECK(!HasWeakHeapObjectTag(slot.load(code_cage_base())));
    Object code = slot.load(code_cage_base());
    RecordMigratedSlot(host, MaybeObject::FromObject(code), slot.address());
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
    DCHECK(!target.InSharedWritableHeap());
    collector_->RecordRelocSlot(host, rinfo, target);
  }

  inline void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    DCHECK_EQ(host, rinfo->host());
    DCHECK(RelocInfo::IsEmbeddedObjectMode(rinfo->rmode()));
    HeapObject object = rinfo->target_object(cage_base());
    GenerationalBarrierForCode(host, rinfo, object);
    WriteBarrier::Shared(host, rinfo, object);
    collector_->RecordRelocSlot(host, rinfo, object);
  }

  // Entries that are skipped for recording.
  inline void VisitExternalReference(Code host, RelocInfo* rinfo) final {}
  inline void VisitRuntimeEntry(Code host, RelocInfo* rinfo) final {}
  inline void VisitInternalReference(Code host, RelocInfo* rinfo) final {}
  inline void VisitExternalPointer(HeapObject host, ExternalPointerSlot slot,
                                   ExternalPointerTag tag) final {}

  virtual void MarkArrayBufferExtensionPromoted(HeapObject object) {}

 protected:
  inline virtual void RecordMigratedSlot(HeapObject host, MaybeObject value,
                                         Address slot) {
    if (value->IsStrongOrWeak()) {
      BasicMemoryChunk* p = BasicMemoryChunk::FromAddress(value.ptr());
      if (p->InYoungGeneration()) {
        DCHECK_IMPLIES(p->IsToPage(),
                       v8_flags.minor_mc ||
                           p->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION) ||
                           p->IsLargePage());

        MemoryChunk* chunk = MemoryChunk::FromHeapObject(host);
        DCHECK(chunk->SweepingDone());
        RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(chunk, slot);
      } else if (p->IsEvacuationCandidate()) {
        if (V8_EXTERNAL_CODE_SPACE_BOOL &&
            p->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
          RememberedSet<OLD_TO_CODE>::Insert<AccessMode::NON_ATOMIC>(
              MemoryChunk::FromHeapObject(host), slot);
        } else {
          RememberedSet<OLD_TO_OLD>::Insert<AccessMode::NON_ATOMIC>(
              MemoryChunk::FromHeapObject(host), slot);
        }
      } else if (p->InSharedHeap() && !collector_->is_shared_heap()) {
        RememberedSet<OLD_TO_SHARED>::Insert<AccessMode::NON_ATOMIC>(
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
    heap_->OnMoveEvent(src, dst, size);
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

#if DEBUG
  void DisableAbortEvacuationAtAddress(MemoryChunk* chunk) {
    abort_evacuation_at_address_ = chunk->area_end();
  }

  void SetUpAbortEvacuationAtAddress(MemoryChunk* chunk) {
    if (v8_flags.stress_compaction || v8_flags.stress_compaction_random) {
      // Stress aborting of evacuation by aborting ~10% of evacuation candidates
      // when stress testing.
      const double kFraction = 0.05;

      if (heap_->isolate()->fuzzer_rng()->NextDouble() < kFraction) {
        const double abort_evacuation_percentage =
            heap_->isolate()->fuzzer_rng()->NextDouble();
        abort_evacuation_at_address_ =
            chunk->area_start() +
            abort_evacuation_percentage * chunk->area_size();
        return;
      }
    }

    abort_evacuation_at_address_ = chunk->area_end();
  }
#endif  // DEBUG

 protected:
  enum MigrationMode { kFast, kObserved };

  PtrComprCageBase cage_base() {
#if V8_COMPRESS_POINTERS
    return PtrComprCageBase{heap_->isolate()};
#else
    return PtrComprCageBase{};
#endif  // V8_COMPRESS_POINTERS
  }

  using MigrateFunction = void (*)(EvacuateVisitorBase* base, HeapObject dst,
                                   HeapObject src, int size,
                                   AllocationSpace dest);

  template <MigrationMode mode>
  static void RawMigrateObject(EvacuateVisitorBase* base, HeapObject dst,
                               HeapObject src, int size, AllocationSpace dest) {
    Address dst_addr = dst.address();
    Address src_addr = src.address();
    PtrComprCageBase cage_base = base->cage_base();
    DCHECK(base->heap_->AllowedToBeMigrated(src.map(cage_base), src, dest));
    DCHECK_NE(dest, LO_SPACE);
    DCHECK_NE(dest, CODE_LO_SPACE);
    if (dest == OLD_SPACE) {
      DCHECK_OBJECT_SIZE(size);
      DCHECK(IsAligned(size, kTaggedSize));
      base->heap_->CopyBlock(dst_addr, src_addr, size);
      if (mode != MigrationMode::kFast)
        base->ExecuteMigrationObservers(dest, src, dst, size);
      // In case the object's map gets relocated during GC we load the old map
      // here. This is fine since they store the same content.
      dst.IterateFast(dst.map(cage_base), size, base->record_visitor_);
      if (V8_UNLIKELY(v8_flags.minor_mc)) {
        base->record_visitor_->MarkArrayBufferExtensionPromoted(dst);
      }
    } else if (dest == MAP_SPACE) {
      DCHECK_OBJECT_SIZE(size);
      DCHECK(IsAligned(size, kTaggedSize));
      base->heap_->CopyBlock(dst_addr, src_addr, size);
      if (mode != MigrationMode::kFast)
        base->ExecuteMigrationObservers(dest, src, dst, size);
      dst.IterateFast(dst.map(cage_base), size, base->record_visitor_);
    } else if (dest == CODE_SPACE) {
      DCHECK_CODEOBJECT_SIZE(size, base->heap_->code_space());
      base->heap_->CopyBlock(dst_addr, src_addr, size);
      Code code = Code::cast(dst);
      code.Relocate(dst_addr - src_addr);
      if (mode != MigrationMode::kFast)
        base->ExecuteMigrationObservers(dest, src, dst, size);
      // In case the object's map gets relocated during GC we load the old map
      // here. This is fine since they store the same content.
      dst.IterateFast(dst.map(cage_base), size, base->record_visitor_);
    } else {
      DCHECK_OBJECT_SIZE(size);
      DCHECK(dest == NEW_SPACE);
      base->heap_->CopyBlock(dst_addr, src_addr, size);
      if (mode != MigrationMode::kFast)
        base->ExecuteMigrationObservers(dest, src, dst, size);
    }
    src.set_map_word(MapWord::FromForwardingAddress(dst), kRelaxedStore);
  }

  EvacuateVisitorBase(Heap* heap, EvacuationAllocator* local_allocator,
                      ConcurrentAllocator* shared_old_allocator,
                      RecordMigratedSlotVisitor* record_visitor)
      : heap_(heap),
        local_allocator_(local_allocator),
        shared_old_allocator_(shared_old_allocator),
        record_visitor_(record_visitor),
        shared_string_table_(shared_old_allocator != nullptr) {
    migration_function_ = RawMigrateObject<MigrationMode::kFast>;
  }

  inline bool TryEvacuateObject(AllocationSpace target_space, HeapObject object,
                                int size, HeapObject* target_object) {
#if DEBUG
    DCHECK_LE(abort_evacuation_at_address_,
              MemoryChunk::FromHeapObject(object)->area_end());
    DCHECK_GE(abort_evacuation_at_address_,
              MemoryChunk::FromHeapObject(object)->area_start());

    if (V8_UNLIKELY(object.address() >= abort_evacuation_at_address_)) {
      return false;
    }
#endif  // DEBUG

    Map map = object.map(cage_base());
    AllocationAlignment alignment = HeapObject::RequiredAlignment(map);
    AllocationResult allocation;
    if (ShouldPromoteIntoSharedHeap(map)) {
      DCHECK_EQ(target_space, OLD_SPACE);
      DCHECK_NOT_NULL(shared_old_allocator_);
      allocation = shared_old_allocator_->AllocateRaw(size, alignment,
                                                      AllocationOrigin::kGC);
    } else {
      allocation = local_allocator_->Allocate(target_space, size,
                                              AllocationOrigin::kGC, alignment);
    }
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

  inline bool ShouldPromoteIntoSharedHeap(Map map) {
    if (shared_string_table_) {
      return String::IsInPlaceInternalizableExcludingExternal(
          map.instance_type());
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

  Heap* heap_;
  EvacuationAllocator* local_allocator_;
  ConcurrentAllocator* shared_old_allocator_;
  RecordMigratedSlotVisitor* record_visitor_;
  std::vector<MigrationObserver*> observers_;
  MigrateFunction migration_function_;
  bool shared_string_table_ = false;
#if DEBUG
  Address abort_evacuation_at_address_{kNullAddress};
#endif  // DEBUG
};

class EvacuateNewSpaceVisitor final : public EvacuateVisitorBase {
 public:
  explicit EvacuateNewSpaceVisitor(
      Heap* heap, EvacuationAllocator* local_allocator,
      ConcurrentAllocator* shared_old_allocator,
      RecordMigratedSlotVisitor* record_visitor,
      Heap::PretenuringFeedbackMap* local_pretenuring_feedback,
      AlwaysPromoteYoung always_promote_young)
      : EvacuateVisitorBase(heap, local_allocator, shared_old_allocator,
                            record_visitor),
        buffer_(LocalAllocationBuffer::InvalidBuffer()),
        promoted_size_(0),
        semispace_copied_size_(0),
        local_pretenuring_feedback_(local_pretenuring_feedback),
        is_incremental_marking_(heap->incremental_marking()->IsMarking()),
        always_promote_young_(always_promote_young) {}

  inline bool Visit(HeapObject object, int size) override {
    if (TryEvacuateWithoutCopy(object)) return true;
    HeapObject target_object;

    if (always_promote_young_ == AlwaysPromoteYoung::kYes) {
      heap_->UpdateAllocationSite(object.map(), object,
                                  local_pretenuring_feedback_);

      if (!TryEvacuateObject(OLD_SPACE, object, size, &target_object)) {
        heap_->FatalProcessOutOfMemory(
            "MarkCompactCollector: young object promotion failed");
      }

      promoted_size_ += size;
      return true;
    }

    if (heap_->new_space()->ShouldBePromoted(object.address()) &&
        TryEvacuateObject(OLD_SPACE, object, size, &target_object)) {
      // Full GCs use AlwaysPromoteYoung::kYes above and MinorMC should never
      // move objects.
      DCHECK(!v8_flags.minor_mc);
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
    DCHECK(!is_incremental_marking_);

    Map map = object.map();

    // Some objects can be evacuated without creating a copy.
    if (map.visitor_id() == kVisitThinString) {
      HeapObject actual = ThinString::cast(object).unchecked_actual();
      if (MarkCompactCollector::IsOnEvacuationCandidate(actual)) return false;
      object.set_map_word(MapWord::FromForwardingAddress(actual),
                          kRelaxedStore);
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
    AllocationResult allocation = local_allocator_->Allocate(
        NEW_SPACE, size, AllocationOrigin::kGC, alignment);
    if (allocation.IsFailure()) {
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
    AllocationResult allocation = local_allocator_->Allocate(
        OLD_SPACE, size_in_bytes, AllocationOrigin::kGC, alignment);
    if (allocation.IsFailure()) {
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
  AlwaysPromoteYoung always_promote_young_;
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
        DCHECK(!v8_flags.minor_mc);
        page->heap()->new_space()->PromotePageInNewSpace(page);
        break;
      case NEW_TO_OLD: {
        page->heap()->new_space()->PromotePageToOldSpace(page);
        break;
      }
    }
  }

  inline bool Visit(HeapObject object, int size) override {
    if (mode == NEW_TO_NEW) {
      DCHECK(!v8_flags.minor_mc);
      heap_->UpdateAllocationSite(object.map(), object,
                                  local_pretenuring_feedback_);
    } else if (mode == NEW_TO_OLD) {
      if (v8_flags.minor_mc) {
        heap_->UpdateAllocationSite(object.map(), object,
                                    local_pretenuring_feedback_);
      }
      DCHECK_IMPLIES(V8_EXTERNAL_CODE_SPACE_BOOL, !IsCodeSpaceObject(object));
      PtrComprCageBase cage_base = GetPtrComprCageBase(object);
      object.IterateFast(cage_base, record_visitor_);
      if (V8_UNLIKELY(v8_flags.minor_mc)) {
        record_visitor_->MarkArrayBufferExtensionPromoted(object);
      }
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
  EvacuateOldSpaceVisitor(Heap* heap, EvacuationAllocator* local_allocator,
                          ConcurrentAllocator* shared_old_allocator,
                          RecordMigratedSlotVisitor* record_visitor)
      : EvacuateVisitorBase(heap, local_allocator, shared_old_allocator,
                            record_visitor) {}

  inline bool Visit(HeapObject object, int size) override {
    HeapObject target_object;
    if (TryEvacuateObject(Page::FromHeapObject(object)->owner_identity(),
                          object, size, &target_object)) {
      DCHECK(object.map_word(heap_->isolate(), kRelaxedLoad)
                 .IsForwardingAddress());
      return true;
    }
    return false;
  }
};

class EvacuateRecordOnlyVisitor final : public HeapObjectVisitor {
 public:
  explicit EvacuateRecordOnlyVisitor(Heap* heap)
      : heap_(heap)
#ifdef V8_COMPRESS_POINTERS
        ,
        cage_base_(heap->isolate())
#endif  // V8_COMPRESS_POINTERS
  {
  }

  // The pointer compression cage base value used for decompression of all
  // tagged values except references to Code objects.
  V8_INLINE PtrComprCageBase cage_base() const {
#ifdef V8_COMPRESS_POINTERS
    return cage_base_;
#else
    return PtrComprCageBase{};
#endif  // V8_COMPRESS_POINTERS
  }

  inline bool Visit(HeapObject object, int size) override {
    RecordMigratedSlotVisitor visitor(heap_->mark_compact_collector(),
                                      &heap_->ephemeron_remembered_set_);
    Map map = object.map(cage_base());
    // Instead of calling object.IterateFast(cage_base(), &visitor) here
    // we can shortcut and use the precomputed size value passed to the visitor.
    DCHECK_EQ(object.SizeFromMap(map), size);
    object.IterateFast(map, size, &visitor);
    return true;
  }

 private:
  Heap* heap_;
#ifdef V8_COMPRESS_POINTERS
  const PtrComprCageBase cage_base_;
#endif  // V8_COMPRESS_POINTERS
};

bool MarkCompactCollector::IsUnmarkedHeapObject(Heap* heap, FullObjectSlot p) {
  Object o = *p;
  if (!o.IsHeapObject()) return false;
  HeapObject heap_object = HeapObject::cast(o);
  return heap->mark_compact_collector()->non_atomic_marking_state()->IsWhite(
      heap_object);
}

void MarkCompactCollector::MarkRoots(RootVisitor* root_visitor,
                                     ObjectVisitor* custom_root_body_visitor) {
  // Mark the heap roots including global variables, stack variables,
  // etc., and all objects reachable from them.
  heap()->IterateRootsIncludingClients(
      root_visitor, base::EnumSet<SkipRoot>{SkipRoot::kWeak, SkipRoot::kStack});

  // Custom marking for top optimized frame.
  ProcessTopOptimizedFrame(custom_root_body_visitor, isolate());

  if (isolate()->is_shared()) {
    isolate()->global_safepoint()->IterateClientIsolates(
        [this, custom_root_body_visitor](Isolate* client) {
          ProcessTopOptimizedFrame(custom_root_body_visitor, client);
        });
  }

  if (!heap_->cpp_heap() && heap_->local_embedder_heap_tracer()->InUse()) {
    // Conservative global handle scanning is necessary for keeping
    // v8::TracedReference alive from the stack. This is only needed when using
    // `EmbedderHeapTracer` and not using `CppHeap`.
    auto& stack = heap()->stack();
    if (stack.stack_start() &&
        heap_->local_embedder_heap_tracer()->embedder_stack_state() ==
            cppgc::EmbedderStackState::kMayContainHeapPointers) {
      GlobalHandleMarkingVisitor global_handles_marker(
          *heap_, marking_state_, *local_marking_worklists_);
      stack.IteratePointers(&global_handles_marker);
    }
  }
}

#ifdef V8_ENABLE_INNER_POINTER_RESOLUTION_MB
namespace {

// This utility function returns the highest address in the page that is lower
// than maybe_inner_ptr, has its markbit set, and whose previous address (if it
// exists) does not have its markbit set. This address is guaranteed to be the
// start of a valid object in the page. In case the markbit corresponding to
// maybe_inner_ptr is set, the function bails out and returns kNullAddress.
Address FindPreviousObjectForConservativeMarking(const Page* page,
                                                 Address maybe_inner_ptr) {
  auto* bitmap = page->marking_bitmap<AccessMode::NON_ATOMIC>();
  const MarkBit::CellType* cells = bitmap->cells();

  // The first actual bit of the bitmap, corresponding to page->area_start(),
  // is at start_index which is somewhere in (not necessarily at the start of)
  // start_cell_index.
  const uint32_t start_index = page->AddressToMarkbitIndex(page->area_start());
  const uint32_t start_cell_index = Bitmap::IndexToCell(start_index);
  // We assume that all markbits before start_index are clear:
  // SLOW_DCHECK(bitmap->AllBitsClearInRange(0, start_index));
  // This has already been checked for the entire bitmap before starting marking
  // by MarkCompactCollector::VerifyMarkbitsAreClean.

  const uint32_t index = page->AddressToMarkbitIndex(maybe_inner_ptr);
  uint32_t cell_index = Bitmap::IndexToCell(index);
  const MarkBit::CellType mask = 1u << Bitmap::IndexInCell(index);
  MarkBit::CellType cell = cells[cell_index];

  // If the markbit is already set, bail out.
  if ((cell & mask) != 0) return kNullAddress;

  // Clear the bits corresponding to higher addresses in the cell.
  cell &= ((~static_cast<MarkBit::CellType>(0)) >>
           (Bitmap::kBitsPerCell - Bitmap::IndexInCell(index) - 1));

  // Traverse the bitmap backwards, until we find a markbit that is set and
  // whose previous markbit (if it exists) is unset.
  // First, iterate backwards to find a cell with any set markbit.
  while (cell == 0 && cell_index > start_cell_index) cell = cells[--cell_index];
  if (cell == 0) {
    DCHECK_EQ(start_cell_index, cell_index);
    // We have reached the start of the page.
    return page->area_start();
  }

  // We have found such a cell.
  const uint32_t leading_zeros = base::bits::CountLeadingZeros(cell);
  const uint32_t leftmost_ones =
      base::bits::CountLeadingZeros(~(cell << leading_zeros));
  const uint32_t index_of_last_leftmost_one =
      Bitmap::kBitsPerCell - leading_zeros - leftmost_ones;

  // If the leftmost sequence of set bits does not reach the start of the cell,
  // we found it.
  if (index_of_last_leftmost_one > 0) {
    return page->MarkbitIndexToAddress(cell_index * Bitmap::kBitsPerCell +
                                       index_of_last_leftmost_one);
  }

  // The leftmost sequence of set bits reaches the start of the cell. We must
  // keep traversing backwards until we find the first unset markbit.
  if (cell_index == start_cell_index) {
    // We have reached the start of the page.
    return page->area_start();
  }

  // Iterate backwards to find a cell with any unset markbit.
  do {
    cell = cells[--cell_index];
  } while (~cell == 0 && cell_index > start_cell_index);
  if (~cell == 0) {
    DCHECK_EQ(start_cell_index, cell_index);
    // We have reached the start of the page.
    return page->area_start();
  }

  // We have found such a cell.
  const uint32_t leading_ones = base::bits::CountLeadingZeros(~cell);
  const uint32_t index_of_last_leading_one =
      Bitmap::kBitsPerCell - leading_ones;
  DCHECK_LT(0, index_of_last_leading_one);
  return page->MarkbitIndexToAddress(cell_index * Bitmap::kBitsPerCell +
                                     index_of_last_leading_one);
}

}  // namespace

Address MarkCompactCollector::FindBasePtrForMarking(Address maybe_inner_ptr) {
  // Check if the pointer is contained by a normal or large page owned by this
  // heap. Bail out if it is not.
  const MemoryChunk* chunk =
      heap()->memory_allocator()->LookupChunkContainingAddress(maybe_inner_ptr);
  if (chunk == nullptr) return kNullAddress;
  DCHECK(chunk->Contains(maybe_inner_ptr));
  // If it is contained in a large page, we want to mark the only object on it.
  if (chunk->IsLargePage()) return chunk->area_start();
  // Otherwise, we have a pointer inside a normal page.
  const Page* page = static_cast<const Page*>(chunk);
  Address base_ptr =
      FindPreviousObjectForConservativeMarking(page, maybe_inner_ptr);
  // If the markbit is set, then we have an object that does not need be marked.
  if (base_ptr == kNullAddress) return kNullAddress;
  // Iterate through the objects in the page forwards, until we find the object
  // containing maybe_inner_ptr.
  DCHECK_LE(base_ptr, maybe_inner_ptr);
  PtrComprCageBase cage_base{page->heap()->isolate()};
  while (true) {
    HeapObject obj(HeapObject::FromAddress(base_ptr));
    const int size = obj.Size(cage_base);
    DCHECK_LT(0, size);
    if (maybe_inner_ptr < base_ptr + size)
      return obj.IsFreeSpaceOrFiller(cage_base) ? kNullAddress : base_ptr;
    base_ptr += size;
    DCHECK_LT(base_ptr, page->area_end());
  }
}
#endif  // V8_ENABLE_INNER_POINTER_RESOLUTION_MB

void MarkCompactCollector::MarkRootsFromStack(RootVisitor* root_visitor) {
  heap()->IterateRootsFromStackIncludingClient(root_visitor);
}

void MarkCompactCollector::MarkObjectsFromClientHeaps() {
  if (!isolate()->is_shared()) return;

  isolate()->global_safepoint()->IterateClientIsolates(
      [collector = this](Isolate* client) {
        collector->MarkObjectsFromClientHeap(client);
      });
}

void MarkCompactCollector::MarkObjectsFromClientHeap(Isolate* client) {
  // There is no OLD_TO_SHARED remembered set for the young generation. We
  // therefore need to iterate each object and check whether it points into the
  // shared heap. As an optimization and to avoid a second heap iteration in the
  // "update pointers" phase, all pointers into the shared heap are recorded in
  // the OLD_TO_SHARED remembered set as well.
  SharedHeapObjectVisitor visitor(this);

  PtrComprCageBase cage_base(client);
  Heap* heap = client->heap();

  if (heap->new_space()) {
    std::unique_ptr<ObjectIterator> iterator =
        heap->new_space()->GetObjectIterator(heap);
    for (HeapObject obj = iterator->Next(); !obj.is_null();
         obj = iterator->Next()) {
      obj.IterateFast(cage_base, &visitor);
    }
  }

  if (heap->new_lo_space()) {
    std::unique_ptr<ObjectIterator> iterator =
        heap->new_lo_space()->GetObjectIterator(heap);
    for (HeapObject obj = iterator->Next(); !obj.is_null();
         obj = iterator->Next()) {
      obj.IterateFast(cage_base, &visitor);
    }
  }

  // In the old generation we can simply use the OLD_TO_SHARED remembered set to
  // find all incoming pointers into the shared heap.
  OldGenerationMemoryChunkIterator chunk_iterator(heap);

  for (MemoryChunk* chunk = chunk_iterator.next(); chunk;
       chunk = chunk_iterator.next()) {
    InvalidatedSlotsFilter filter = InvalidatedSlotsFilter::OldToShared(
        chunk, InvalidatedSlotsFilter::LivenessCheck::kNo);
    RememberedSet<OLD_TO_SHARED>::Iterate(
        chunk,
        [collector = this, cage_base, &filter](MaybeObjectSlot slot) {
          if (!filter.IsValid(slot.address())) return REMOVE_SLOT;
          MaybeObject obj = slot.Relaxed_Load(cage_base);
          HeapObject heap_object;

          if (obj.GetHeapObject(&heap_object) &&
              heap_object.InSharedWritableHeap()) {
            collector->MarkRootObject(Root::kClientHeap, heap_object);
            return KEEP_SLOT;
          } else {
            return REMOVE_SLOT;
          }
        },
        SlotSet::FREE_EMPTY_BUCKETS);
    chunk->ReleaseInvalidatedSlots<OLD_TO_SHARED>();
  }

#ifdef V8_COMPRESS_POINTERS
  DCHECK(IsSandboxedExternalPointerType(kWaiterQueueNodeTag));
  DCHECK(IsSharedExternalPointerType(kWaiterQueueNodeTag));
  // Custom marking for the external pointer table entry used to hold
  // client Isolates' WaiterQueueNode, which is used by JS mutexes and
  // condition variables.
  ExternalPointerHandle* handle_location =
      client->GetWaiterQueueNodeExternalPointerHandleLocation();
  ExternalPointerTable& table = client->shared_external_pointer_table();
  ExternalPointerHandle handle =
      base::AsAtomic32::Relaxed_Load(handle_location);
  if (handle) {
    table.Mark(handle, reinterpret_cast<Address>(handle_location));
  }
#endif  // V8_COMPRESS_POINTERS

#ifdef V8_ENABLE_SANDBOX
  if (IsSandboxedExternalPointerType(kExternalStringResourceTag) ||
      IsSandboxedExternalPointerType(kExternalStringResourceDataTag)) {
    // All ExternalString resources are stored in the shared external pointer
    // table. Mark entries from client heaps.
    ExternalPointerTable& table = client->shared_external_pointer_table();
    MarkExternalPointerFromExternalStringTable external_string_visitor(&table);
    heap->external_string_table_.IterateAll(&external_string_visitor);
  }
#endif  // V8_ENABLE_SANDBOX
}

void MarkCompactCollector::VisitObject(HeapObject obj) {
  marking_visitor_->Visit(obj.map(), obj);
}

bool MarkCompactCollector::MarkTransitiveClosureUntilFixpoint() {
  int iterations = 0;
  int max_iterations = v8_flags.ephemeron_fixpoint_iterations;

  bool another_ephemeron_iteration_main_thread;

  do {
    PerformWrapperTracing();

    if (iterations >= max_iterations) {
      // Give up fixpoint iteration and switch to linear algorithm.
      return false;
    }

    // Move ephemerons from next_ephemerons into current_ephemerons to
    // drain them in this iteration.
    DCHECK(
        local_weak_objects()->current_ephemerons_local.IsLocalAndGlobalEmpty());
    weak_objects_.current_ephemerons.Merge(weak_objects_.next_ephemerons);
    heap()->concurrent_marking()->set_another_ephemeron_iteration(false);

    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON_MARKING);
      another_ephemeron_iteration_main_thread = ProcessEphemerons();
    }

    // Can only check for local emptiness here as parallel marking tasks may
    // still be running. The caller performs the CHECKs for global emptiness.
    CHECK(local_weak_objects()->current_ephemerons_local.IsLocalEmpty());
    CHECK(local_weak_objects()->discovered_ephemerons_local.IsLocalEmpty());

    ++iterations;
  } while (another_ephemeron_iteration_main_thread ||
           heap()->concurrent_marking()->another_ephemeron_iteration() ||
           !local_marking_worklists()->IsEmpty() ||
           !local_marking_worklists()->IsWrapperEmpty() ||
           !heap()->local_embedder_heap_tracer()->IsRemoteTracingDone());

  return true;
}

bool MarkCompactCollector::ProcessEphemerons() {
  Ephemeron ephemeron;
  bool another_ephemeron_iteration = false;

  // Drain current_ephemerons and push ephemerons where key and value are still
  // unreachable into next_ephemerons.
  while (local_weak_objects()->current_ephemerons_local.Pop(&ephemeron)) {
    if (ProcessEphemeron(ephemeron.key, ephemeron.value)) {
      another_ephemeron_iteration = true;
    }
  }

  // Drain marking worklist and push discovered ephemerons into
  // discovered_ephemerons.
  size_t objects_processed;
  std::tie(std::ignore, objects_processed) = ProcessMarkingWorklist(0);

  // As soon as a single object was processed and potentially marked another
  // object we need another iteration. Otherwise we might miss to apply
  // ephemeron semantics on it.
  if (objects_processed > 0) another_ephemeron_iteration = true;

  // Drain discovered_ephemerons (filled in the drain MarkingWorklist-phase
  // before) and push ephemerons where key and value are still unreachable into
  // next_ephemerons.
  while (local_weak_objects()->discovered_ephemerons_local.Pop(&ephemeron)) {
    if (ProcessEphemeron(ephemeron.key, ephemeron.value)) {
      another_ephemeron_iteration = true;
    }
  }

  // Flush local ephemerons for main task to global pool.
  local_weak_objects()->ephemeron_hash_tables_local.Publish();
  local_weak_objects()->next_ephemerons_local.Publish();

  return another_ephemeron_iteration;
}

void MarkCompactCollector::MarkTransitiveClosureLinear() {
  TRACE_GC(heap()->tracer(),
           GCTracer::Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON_LINEAR);
  // This phase doesn't support parallel marking.
  DCHECK(heap()->concurrent_marking()->IsStopped());
  std::unordered_multimap<HeapObject, HeapObject, Object::Hasher> key_to_values;
  Ephemeron ephemeron;

  DCHECK(
      local_weak_objects()->current_ephemerons_local.IsLocalAndGlobalEmpty());
  weak_objects_.current_ephemerons.Merge(weak_objects_.next_ephemerons);
  while (local_weak_objects()->current_ephemerons_local.Pop(&ephemeron)) {
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
      ProcessMarkingWorklist(
          0, MarkingWorklistProcessingMode::kTrackNewlyDiscoveredObjects);
    }

    while (local_weak_objects()->discovered_ephemerons_local.Pop(&ephemeron)) {
      ProcessEphemeron(ephemeron.key, ephemeron.value);

      if (non_atomic_marking_state()->IsWhite(ephemeron.value)) {
        key_to_values.insert(std::make_pair(ephemeron.key, ephemeron.value));
      }
    }

    if (ephemeron_marking_.newly_discovered_overflowed) {
      // If newly_discovered was overflowed just visit all ephemerons in
      // next_ephemerons.
      local_weak_objects()->next_ephemerons_local.Publish();
      weak_objects_.next_ephemerons.Iterate([&](Ephemeron ephemeron) {
        if (non_atomic_marking_state()->IsBlackOrGrey(ephemeron.key) &&
            non_atomic_marking_state()->WhiteToGrey(ephemeron.value)) {
          local_marking_worklists()->Push(ephemeron.value);
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

    work_to_do = !local_marking_worklists()->IsEmpty() ||
                 !local_marking_worklists()->IsWrapperEmpty() ||
                 !heap()->local_embedder_heap_tracer()->IsRemoteTracingDone();
    CHECK(local_weak_objects()
              ->discovered_ephemerons_local.IsLocalAndGlobalEmpty());
  }

  ResetNewlyDiscovered();
  ephemeron_marking_.newly_discovered.shrink_to_fit();

  CHECK(local_marking_worklists()->IsEmpty());

  CHECK(weak_objects_.current_ephemerons.IsEmpty());
  CHECK(weak_objects_.discovered_ephemerons.IsEmpty());

  // Flush local ephemerons for main task to global pool.
  local_weak_objects()->ephemeron_hash_tables_local.Publish();
  local_weak_objects()->next_ephemerons_local.Publish();
}

void MarkCompactCollector::PerformWrapperTracing() {
  if (heap_->local_embedder_heap_tracer()->InUse()) {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_EMBEDDER_TRACING);
    if (local_marking_worklists()->PublishWrapper()) {
      DCHECK(local_marking_worklists()->IsWrapperEmpty());
    } else {
      // Cannot directly publish wrapper objects.
      LocalEmbedderHeapTracer::ProcessingScope scope(
          heap_->local_embedder_heap_tracer());
      HeapObject object;
      while (local_marking_worklists()->PopWrapper(&object)) {
        scope.TracePossibleWrapper(JSObject::cast(object));
      }
    }
    heap_->local_embedder_heap_tracer()->Trace(
        std::numeric_limits<double>::infinity());
  }
}

std::pair<size_t, size_t> MarkCompactCollector::ProcessMarkingWorklist(
    size_t bytes_to_process) {
  return ProcessMarkingWorklist(bytes_to_process,
                                MarkingWorklistProcessingMode::kDefault);
}

std::pair<size_t, size_t> MarkCompactCollector::ProcessMarkingWorklist(
    size_t bytes_to_process, MarkingWorklistProcessingMode mode) {
  HeapObject object;
  size_t bytes_processed = 0;
  size_t objects_processed = 0;
  bool is_per_context_mode = local_marking_worklists()->IsPerContextMode();
  Isolate* isolate = heap()->isolate();
  PtrComprCageBase cage_base(isolate);
  CodePageHeaderModificationScope rwx_write_scope(
      "Marking of Code objects require write access to Code page headers");
  if (parallel_marking_)
    heap_->concurrent_marking()->RescheduleJobIfNeeded(
        GarbageCollector::MARK_COMPACTOR, TaskPriority::kUserBlocking);

  while (local_marking_worklists()->Pop(&object) ||
         local_marking_worklists()->PopOnHold(&object)) {
    // Left trimming may result in grey or black filler objects on the marking
    // worklist. Ignore these objects.
    if (object.IsFreeSpaceOrFiller(cage_base)) {
      // Due to copying mark bits and the fact that grey and black have their
      // first bit set, one word fillers are always black.
      DCHECK_IMPLIES(object.map(cage_base) ==
                         ReadOnlyRoots(isolate).one_pointer_filler_map(),
                     marking_state()->IsBlack(object));
      // Other fillers may be black or grey depending on the color of the object
      // that was trimmed.
      DCHECK_IMPLIES(object.map(cage_base) !=
                         ReadOnlyRoots(isolate).one_pointer_filler_map(),
                     marking_state()->IsBlackOrGrey(object));
      continue;
    }
    DCHECK(object.IsHeapObject());
    DCHECK(heap()->Contains(object));
    DCHECK(!(marking_state()->IsWhite(object)));
    if (mode == MarkCompactCollector::MarkingWorklistProcessingMode::
                    kTrackNewlyDiscoveredObjects) {
      AddNewlyDiscovered(object);
    }
    Map map = object.map(cage_base);
    if (is_per_context_mode) {
      Address context;
      if (native_context_inferrer_.Infer(isolate, map, object, &context)) {
        local_marking_worklists()->SwitchToContext(context);
      }
    }
    size_t visited_size = marking_visitor_->Visit(map, object);
    if (is_per_context_mode) {
      native_context_stats_.IncrementSize(local_marking_worklists()->Context(),
                                          map, object, visited_size);
    }
    bytes_processed += visited_size;
    objects_processed++;
    if (bytes_to_process && bytes_processed >= bytes_to_process) {
      break;
    }
  }
  return std::make_pair(bytes_processed, objects_processed);
}

bool MarkCompactCollector::ProcessEphemeron(HeapObject key, HeapObject value) {
  if (marking_state()->IsBlackOrGrey(key)) {
    if (marking_state()->WhiteToGrey(value)) {
      local_marking_worklists()->Push(value);
      return true;
    }

  } else if (marking_state()->IsWhite(value)) {
    local_weak_objects()->next_ephemerons_local.Push(Ephemeron{key, value});
  }
  return false;
}

void MarkCompactCollector::VerifyEphemeronMarking() {
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) {
    Ephemeron ephemeron;

    DCHECK(
        local_weak_objects()->current_ephemerons_local.IsLocalAndGlobalEmpty());
    weak_objects_.current_ephemerons.Merge(weak_objects_.next_ephemerons);
    while (local_weak_objects()->current_ephemerons_local.Pop(&ephemeron)) {
      CHECK(!ProcessEphemeron(ephemeron.key, ephemeron.value));
    }
  }
#endif  // VERIFY_HEAP
}

void MarkCompactCollector::MarkTransitiveClosure() {
  // Incremental marking might leave ephemerons in main task's local
  // buffer, flush it into global pool.
  local_weak_objects()->next_ephemerons_local.Publish();

  if (!MarkTransitiveClosureUntilFixpoint()) {
    // Fixpoint iteration needed too many iterations and was cancelled. Use the
    // guaranteed linear algorithm. But only in the final single-thread marking
    // phase.
    if (!parallel_marking_) MarkTransitiveClosureLinear();
  }
}

void MarkCompactCollector::ProcessTopOptimizedFrame(ObjectVisitor* visitor,
                                                    Isolate* isolate) {
  for (StackFrameIterator it(isolate, isolate->thread_local_top()); !it.done();
       it.Advance()) {
    if (it.frame()->is_unoptimized()) return;
    if (it.frame()->is_optimized()) {
      CodeLookupResult lookup_result = it.frame()->LookupCodeT();
      // Embedded builtins can't deoptimize.
      if (lookup_result.IsCodeDataContainer()) return;
      Code code = lookup_result.code();
      if (!code.CanDeoptAt(isolate, it.frame()->pc())) {
        PtrComprCageBase cage_base(isolate);
        Code::BodyDescriptor::IterateBody(code.map(cage_base), code, visitor);
      }
      return;
    }
  }
}

void MarkCompactCollector::RecordObjectStats() {
  if (V8_LIKELY(!TracingFlags::is_gc_stats_enabled())) return;
  // Cannot run during bootstrapping due to incomplete objects.
  if (isolate()->bootstrapper()->IsActive()) return;
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
  if (v8_flags.trace_gc_object_stats) {
    heap()->live_object_stats_->PrintJSON("live");
    heap()->dead_object_stats_->PrintJSON("dead");
  }
  heap()->live_object_stats_->CheckpointObjectStats();
  heap()->dead_object_stats_->ClearObjectStats();
}

namespace {

bool ShouldRetainMap(MarkingState* marking_state, Map map, int age) {
  if (age == 0) {
    // The map has aged. Do not retain this map.
    return false;
  }
  Object constructor = map.GetConstructor();
  if (!constructor.IsHeapObject() ||
      marking_state->IsWhite(HeapObject::cast(constructor))) {
    // The constructor is dead, no new objects with this map can
    // be created. Do not retain this map.
    return false;
  }
  return true;
}

}  // namespace

void MarkCompactCollector::RetainMaps() {
  // Retaining maps increases the chances of reusing map transitions at some
  // memory cost, hence disable it when trying to reduce memory footprint more
  // aggressively.
  const bool should_retain_maps =
      !heap()->ShouldReduceMemory() && v8_flags.retain_maps_for_n_gc != 0;

  for (WeakArrayList retained_maps : heap()->FindAllRetainedMaps()) {
    DCHECK_EQ(0, retained_maps.length() % 2);
    for (int i = 0; i < retained_maps.length(); i += 2) {
      MaybeObject value = retained_maps.Get(i);
      HeapObject map_heap_object;
      if (!value->GetHeapObjectIfWeak(&map_heap_object)) {
        continue;
      }
      int age = retained_maps.Get(i + 1).ToSmi().value();
      int new_age;
      Map map = Map::cast(map_heap_object);
      if (should_retain_maps && marking_state()->IsWhite(map)) {
        if (ShouldRetainMap(marking_state(), map, age)) {
          if (marking_state()->WhiteToGrey(map)) {
            local_marking_worklists()->Push(map);
          }
          if (V8_UNLIKELY(v8_flags.track_retaining_path)) {
            heap_->AddRetainingRoot(Root::kRetainMaps, map);
          }
        }
        Object prototype = map.prototype();
        if (age > 0 && prototype.IsHeapObject() &&
            marking_state()->IsWhite(HeapObject::cast(prototype))) {
          // The prototype is not marked, age the map.
          new_age = age - 1;
        } else {
          // The prototype and the constructor are marked, this map keeps only
          // transition tree alive, not JSObjects. Do not age the map.
          new_age = age;
        }
      } else {
        new_age = v8_flags.retain_maps_for_n_gc;
      }
      // Compact the array and update the age.
      if (new_age != age) {
        retained_maps.Set(i + 1, MaybeObject::FromSmi(Smi::FromInt(new_age)));
      }
    }
  }
}

void MarkCompactCollector::MarkLiveObjects() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK);
  // The recursive GC marker detects when it is nearing stack overflow,
  // and switches to a different marking system.  JS interrupts interfere
  // with the C stack limit check.
  PostponeInterruptsScope postpone(isolate());

  bool was_marked_incrementally = false;
  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_FINISH_INCREMENTAL);
    if (heap_->incremental_marking()->Stop()) {
      MarkingBarrier::PublishAll(heap());
      was_marked_incrementally = true;
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
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_CLIENT_HEAPS);
    MarkObjectsFromClientHeaps();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_RETAIN_MAPS);
    RetainMaps();
  }

  if (v8_flags.parallel_marking) {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_FULL_CLOSURE_PARALLEL);
    parallel_marking_ = true;
    heap_->concurrent_marking()->RescheduleJobIfNeeded(
        GarbageCollector::MARK_COMPACTOR, TaskPriority::kUserBlocking);
    MarkTransitiveClosure();
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_FULL_CLOSURE_PARALLEL_JOIN);
      FinishConcurrentMarking();
    }
    parallel_marking_ = false;
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_ROOTS);
    MarkRootsFromStack(&root_visitor);
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_FULL_CLOSURE);
    // Complete the transitive closure single-threaded to avoid races with
    // multiple threads when processing weak maps and embedder heaps.
    CHECK(heap()->concurrent_marking()->IsStopped());
    MarkTransitiveClosure();
    CHECK(local_marking_worklists()->IsEmpty());
    CHECK(
        local_weak_objects()->current_ephemerons_local.IsLocalAndGlobalEmpty());
    CHECK(local_weak_objects()
              ->discovered_ephemerons_local.IsLocalAndGlobalEmpty());
    CHECK(heap()->local_embedder_heap_tracer()->IsRemoteTracingDone());
    VerifyEphemeronMarking();
  }

  if (was_marked_incrementally) {
    // Disable the marking barrier after concurrent/parallel marking has
    // finished as it will reset page flags that share the same bitmap as
    // the evacuation candidate bit.
    MarkingBarrier::DeactivateAll(heap());
    GlobalHandles::DisableMarkingBarrier(heap()->isolate());
  }

  epoch_++;
}

namespace {

class ParallelClearingJob final : public v8::JobTask {
 public:
  class ClearingItem {
   public:
    virtual ~ClearingItem() = default;
    virtual void Run(JobDelegate* delegate) = 0;
  };

  ParallelClearingJob() = default;
  ~ParallelClearingJob() override = default;
  ParallelClearingJob(const ParallelClearingJob&) = delete;
  ParallelClearingJob& operator=(const ParallelClearingJob&) = delete;

  // v8::JobTask overrides.
  void Run(JobDelegate* delegate) override {
    std::unique_ptr<ClearingItem> item;
    {
      base::MutexGuard guard(&items_mutex_);
      item = std::move(items_.back());
      items_.pop_back();
    }
    item->Run(delegate);
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    base::MutexGuard guard(&items_mutex_);
    return items_.size();
  }

  void Add(std::unique_ptr<ClearingItem> item) {
    items_.push_back(std::move(item));
  }

 private:
  mutable base::Mutex items_mutex_;
  std::vector<std::unique_ptr<ClearingItem>> items_;
};

class ClearStringTableJobItem final : public ParallelClearingJob::ClearingItem {
 public:
  explicit ClearStringTableJobItem(Isolate* isolate) : isolate_(isolate) {}

  void Run(JobDelegate* delegate) final {
    if (isolate_->OwnsStringTables()) {
      TRACE_GC1(isolate_->heap()->tracer(),
                GCTracer::Scope::MC_CLEAR_STRING_TABLE,
                delegate->IsJoiningThread() ? ThreadKind::kMain
                                            : ThreadKind::kBackground);
      // Prune the string table removing all strings only pointed to by the
      // string table.  Cannot use string_table() here because the string
      // table is marked.
      StringTable* string_table = isolate_->string_table();
      InternalizedStringTableCleaner internalized_visitor(isolate_->heap());
      string_table->DropOldData();
      string_table->IterateElements(&internalized_visitor);
      string_table->NotifyElementsRemoved(
          internalized_visitor.PointersRemoved());
    }
  }

 private:
  Isolate* const isolate_;
};

class StringForwardingTableCleaner final {
 public:
  explicit StringForwardingTableCleaner(Heap* heap)
      : heap_(heap),
        isolate_(heap_->isolate()),
        marking_state_(
            heap_->mark_compact_collector()->non_atomic_marking_state()) {}
  void Run() {
    StringForwardingTable* forwarding_table =
        isolate_->string_forwarding_table();
    forwarding_table->IterateElements(
        [&](StringForwardingTable::Record* record) {
          TransitionStrings(record);
        });
    forwarding_table->Reset();
  }

 private:
  void TransitionStrings(StringForwardingTable::Record* record) {
    Object original = record->OriginalStringObject(isolate_);
    if (!original.IsHeapObject()) {
      // Only if we always use the forwarding table, the string could be a
      // smi, indicating that the entry died during scavenge.
      DCHECK(v8_flags.always_use_string_forwarding_table);
      DCHECK_EQ(original, StringForwardingTable::deleted_element());
      return;
    }
    if (marking_state_->IsBlack(HeapObject::cast(original))) {
      String original_string = String::cast(original);
      if (original_string.IsThinString()) {
        original_string = ThinString::cast(original_string).actual();
      }
      TryExternalize(original_string, record);
      TryInternalize(original_string, record);
      original_string.set_raw_hash_field(record->raw_hash(isolate_));
    } else {
      record->DisposeExternalResource();
    }
  }

  void TryExternalize(String original_string,
                      StringForwardingTable::Record* record) {
    // If the string is already external, dispose the resource.
    if (original_string.IsExternalString()) {
      record->DisposeUnusedExternalResource(original_string);
      return;
    }

    bool is_one_byte;
    v8::String::ExternalStringResourceBase* external_resource =
        record->external_resource(&is_one_byte);
    if (external_resource == nullptr) return;

    if (is_one_byte) {
      original_string.MakeExternalDuringGC(
          isolate_,
          reinterpret_cast<v8::String::ExternalOneByteStringResource*>(
              external_resource));
    } else {
      original_string.MakeExternalDuringGC(
          isolate_, reinterpret_cast<v8::String::ExternalStringResource*>(
                        external_resource));
    }
  }

  void TryInternalize(String original_string,
                      StringForwardingTable::Record* record) {
    if (original_string.IsInternalizedString()) return;
    Object forward = record->ForwardStringObjectOrHash(isolate_);
    if (!forward.IsHeapObject()) return;
    String forward_string = String::cast(forward);

    // Mark the forwarded string to keep it alive.
    marking_state_->WhiteToBlack(forward_string);
    // Transition the original string to a ThinString and override the
    // forwarding index with the correct hash.
    original_string.MakeThin(isolate_, forward_string);
    // Record the slot in the old-to-old remembered set. This is
    // required as the internalized string could be relocated during
    // compaction.
    ObjectSlot slot =
        ThinString::cast(original_string).RawField(ThinString::kActualOffset);
    MarkCompactCollector::RecordSlot(original_string, slot, forward_string);
  }
  Heap* heap_;
  Isolate* isolate_;
  NonAtomicMarkingState* marking_state_;
};

}  // namespace

void MarkCompactCollector::ClearNonLiveReferences() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR);

  if (isolate()->OwnsStringTables()) {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_CLEAR_STRING_FORWARDING_TABLE);
    // Clear string forwarding table. Live strings are transitioned to
    // ThinStrings/ExternalStrings in the cleanup process.
    // Clearing the string forwarding table must happen before clearing the
    // string table, as entries in the forwarding table can keep internalized
    // strings alive.
    StringForwardingTableCleaner forwarding_table_cleaner(heap());
    forwarding_table_cleaner.Run();
  }

  auto clearing_job = std::make_unique<ParallelClearingJob>();
  clearing_job->Add(std::make_unique<ClearStringTableJobItem>(isolate()));
  auto clearing_job_handle = V8::GetCurrentPlatform()->PostJob(
      TaskPriority::kUserBlocking, std::move(clearing_job));

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_EXTERNAL_STRING_TABLE);
    ExternalStringTableCleaner external_visitor(heap());
    heap()->external_string_table_.IterateAll(&external_visitor);
    heap()->external_string_table_.CleanUpAll();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_GLOBAL_HANDLES);
    // We depend on `IterateWeakRootsForPhantomHandles()` being called before
    // `ProcessOldCodeCandidates()` in order to identify flushed bytecode in the
    // CPU profiler.
    heap()->isolate()->global_handles()->IterateWeakRootsForPhantomHandles(
        &IsUnmarkedHeapObject);
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_FLUSHABLE_BYTECODE);
    // `ProcessFlushedBaselineCandidates()` must be called after
    // `ProcessOldCodeCandidates()` so that we correctly set the code object on
    // the JSFunction after flushing.
    ProcessOldCodeCandidates();
    ProcessFlushedBaselineCandidates();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_FLUSHED_JS_FUNCTIONS);
    ClearFlushedJsFunctions();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_LISTS);
    // Process the weak references.
    MarkCompactWeakObjectRetainer mark_compact_object_retainer(marking_state());
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

  PROFILE(heap()->isolate(), WeakCodeClearEvent());

  MarkDependentCodeForDeoptimization();

#ifdef V8_ENABLE_SANDBOX
  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_SWEEP_EXTERNAL_POINTER_TABLE);
    // External pointer table sweeping needs to happen before evacuating live
    // objects as it may perform table compaction, which requires objects to
    // still be at the same location as during marking.
    isolate()->external_pointer_table().SweepAndCompact(isolate());
    if (isolate()->owns_shareable_data()) {
      isolate()->shared_external_pointer_table().SweepAndCompact(isolate());
    }
  }
#endif  // V8_ENABLE_SANDBOX

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_JOIN_JOB);
    clearing_job_handle->Join();
  }

  DCHECK(weak_objects_.transition_arrays.IsEmpty());
  DCHECK(weak_objects_.weak_references.IsEmpty());
  DCHECK(weak_objects_.weak_objects_in_code.IsEmpty());
  DCHECK(weak_objects_.js_weak_refs.IsEmpty());
  DCHECK(weak_objects_.weak_cells.IsEmpty());
  DCHECK(weak_objects_.code_flushing_candidates.IsEmpty());
  DCHECK(weak_objects_.baseline_flushing_candidates.IsEmpty());
  DCHECK(weak_objects_.flushed_js_functions.IsEmpty());
}

void MarkCompactCollector::MarkDependentCodeForDeoptimization() {
  std::pair<HeapObject, Code> weak_object_in_code;
  while (local_weak_objects()->weak_objects_in_code_local.Pop(
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
  Object potential_parent = dead_target.constructor_or_back_pointer();
  if (potential_parent.IsMap()) {
    Map parent = Map::cast(potential_parent);
    DisallowGarbageCollection no_gc_obviously;
    if (non_atomic_marking_state()->IsBlackOrGrey(parent) &&
        TransitionsAccessor(isolate(), parent)
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
  DescriptorArray descriptors = map.instance_descriptors(isolate());
  if (descriptors == dead_target.instance_descriptors(isolate()) &&
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
  static_assert(BytecodeArray::SizeFor(0) >=
                UncompiledDataWithoutPreparseData::kSize);

  // Replace bytecode array with an uncompiled data array.
  HeapObject compiled_data = shared_info.GetBytecodeArray(isolate());
  Address compiled_data_start = compiled_data.address();
  int compiled_data_size = compiled_data.Size();
  MemoryChunk* chunk = MemoryChunk::FromAddress(compiled_data_start);

  // Clear any recorded slots for the compiled data as being invalid.
  RememberedSet<OLD_TO_NEW>::RemoveRange(
      chunk, compiled_data_start, compiled_data_start + compiled_data_size,
      SlotSet::FREE_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_OLD>::RemoveRange(
      chunk, compiled_data_start, compiled_data_start + compiled_data_size,
      SlotSet::FREE_EMPTY_BUCKETS);

  // Swap the map, using set_map_after_allocation to avoid verify heap checks
  // which are not necessary since we are doing this during the GC atomic pause.
  compiled_data.set_map_after_allocation(
      ReadOnlyRoots(heap()).uncompiled_data_without_preparse_data_map(),
      SKIP_WRITE_BARRIER);

  // Create a filler object for any left over space in the bytecode array.
  if (!heap()->IsLargeObject(compiled_data)) {
    heap()->CreateFillerObjectAt(
        compiled_data.address() + UncompiledDataWithoutPreparseData::kSize,
        compiled_data_size - UncompiledDataWithoutPreparseData::kSize);
  }

  // Initialize the uncompiled data.
  UncompiledData uncompiled_data = UncompiledData::cast(compiled_data);
  uncompiled_data.InitAfterBytecodeFlush(
      inferred_name, start_position, end_position,
      [](HeapObject object, ObjectSlot slot, HeapObject target) {
        RecordSlot(object, slot, target);
      });

  // Mark the uncompiled data as black, and ensure all fields have already been
  // marked.
  DCHECK(marking_state()->IsBlackOrGrey(inferred_name) ||
         (!is_shared_heap() && inferred_name.InSharedWritableHeap()));
  marking_state()->WhiteToBlack(uncompiled_data);

  // Use the raw function data setter to avoid validity checks, since we're
  // performing the unusual task of decompiling.
  shared_info.set_function_data(uncompiled_data, kReleaseStore);
  DCHECK(!shared_info.is_compiled());
}

void MarkCompactCollector::ProcessOldCodeCandidates() {
  DCHECK(v8_flags.flush_bytecode || v8_flags.flush_baseline_code ||
         weak_objects_.code_flushing_candidates.IsEmpty());
  SharedFunctionInfo flushing_candidate;
  while (local_weak_objects()->code_flushing_candidates_local.Pop(
      &flushing_candidate)) {
    bool is_bytecode_live = non_atomic_marking_state()->IsBlackOrGrey(
        flushing_candidate.GetBytecodeArray(isolate()));
    if (v8_flags.flush_baseline_code && flushing_candidate.HasBaselineCode()) {
      CodeT baseline_codet =
          CodeT::cast(flushing_candidate.function_data(kAcquireLoad));
      // Safe to do a relaxed load here since the CodeT was acquire-loaded.
      Code baseline_code = FromCodeT(baseline_codet, kRelaxedLoad);
      if (non_atomic_marking_state()->IsBlackOrGrey(baseline_code)) {
        // Currently baseline code holds bytecode array strongly and it is
        // always ensured that bytecode is live if baseline code is live. Hence
        // baseline code can safely load bytecode array without any additional
        // checks. In future if this changes we need to update these checks to
        // flush code if the bytecode is not live and also update baseline code
        // to bailout if there is no bytecode.
        DCHECK(is_bytecode_live);

        // Regardless of whether the CodeT is a CodeDataContainer or the Code
        // itself, if the Code is live then the CodeT has to be live and will
        // have been marked via the owning JSFunction.
        DCHECK(non_atomic_marking_state()->IsBlackOrGrey(baseline_codet));
      } else if (is_bytecode_live) {
        // If baseline code is flushed but we have a valid bytecode array reset
        // the function_data field to the BytecodeArray/InterpreterData.
        flushing_candidate.set_function_data(
            baseline_code.bytecode_or_interpreter_data(), kReleaseStore);
      }
    }

    if (!is_bytecode_live) {
      // If baseline code flushing is disabled we should only flush bytecode
      // from functions that don't have baseline data.
      DCHECK(v8_flags.flush_baseline_code ||
             !flushing_candidate.HasBaselineCode());

      // If the BytecodeArray is dead, flush it, which will replace the field
      // with an uncompiled data object.
      FlushBytecodeFromSFI(flushing_candidate);
    }

    // Now record the slot, which has either been updated to an uncompiled data,
    // Baseline code or BytecodeArray which is still alive.
    ObjectSlot slot =
        flushing_candidate.RawField(SharedFunctionInfo::kFunctionDataOffset);
    RecordSlot(flushing_candidate, slot, HeapObject::cast(*slot));
  }
}

void MarkCompactCollector::ClearFlushedJsFunctions() {
  DCHECK(v8_flags.flush_bytecode ||
         weak_objects_.flushed_js_functions.IsEmpty());
  JSFunction flushed_js_function;
  while (local_weak_objects()->flushed_js_functions_local.Pop(
      &flushed_js_function)) {
    auto gc_notify_updated_slot = [](HeapObject object, ObjectSlot slot,
                                     Object target) {
      RecordSlot(object, slot, HeapObject::cast(target));
    };
    flushed_js_function.ResetIfCodeFlushed(gc_notify_updated_slot);
  }
}

void MarkCompactCollector::ProcessFlushedBaselineCandidates() {
  DCHECK(v8_flags.flush_baseline_code ||
         weak_objects_.baseline_flushing_candidates.IsEmpty());
  JSFunction flushed_js_function;
  while (local_weak_objects()->baseline_flushing_candidates_local.Pop(
      &flushed_js_function)) {
    auto gc_notify_updated_slot = [](HeapObject object, ObjectSlot slot,
                                     Object target) {
      RecordSlot(object, slot, HeapObject::cast(target));
    };
    flushed_js_function.ResetIfCodeFlushed(gc_notify_updated_slot);

    // Record the code slot that has been updated either to CompileLazy,
    // InterpreterEntryTrampoline or baseline code.
    ObjectSlot slot = flushed_js_function.RawField(JSFunction::kCodeOffset);
    RecordSlot(flushed_js_function, slot, HeapObject::cast(*slot));
  }
}

void MarkCompactCollector::ClearFullMapTransitions() {
  TransitionArray array;
  while (local_weak_objects()->transition_arrays_local.Pop(&array)) {
    int num_transitions = array.number_of_entries();
    if (num_transitions > 0) {
      Map map;
      // The array might contain "undefined" elements because it's not yet
      // filled. Allow it.
      if (array.GetTargetIfExists(0, isolate(), &map)) {
        DCHECK(!map.is_null());  // Weak pointers aren't cleared yet.
        Object constructor_or_back_pointer = map.constructor_or_back_pointer();
        if (constructor_or_back_pointer.IsSmi()) {
          DCHECK(isolate()->has_active_deserializer());
          DCHECK_EQ(constructor_or_back_pointer,
                    Smi::uninitialized_deserialization_value());
          continue;
        }
        Map parent = Map::cast(map.constructor_or_back_pointer());
        bool parent_is_alive =
            non_atomic_marking_state()->IsBlackOrGrey(parent);
        DescriptorArray descriptors =
            parent_is_alive ? parent.instance_descriptors(isolate())
                            : DescriptorArray();
        bool descriptors_owner_died =
            CompactTransitionArray(parent, array, descriptors);
        if (descriptors_owner_died) {
          TrimDescriptorArray(parent, descriptors);
        }
      }
    }
  }
}

// Returns false if no maps have died, or if the transition array is
// still being deserialized.
bool MarkCompactCollector::TransitionArrayNeedsCompaction(
    TransitionArray transitions, int num_transitions) {
  for (int i = 0; i < num_transitions; ++i) {
    MaybeObject raw_target = transitions.GetRawTarget(i);
    if (raw_target.IsSmi()) {
      // This target is still being deserialized,
      DCHECK(isolate()->has_active_deserializer());
      DCHECK_EQ(raw_target.ToSmi(), Smi::uninitialized_deserialization_value());
#ifdef DEBUG
      // Targets can only be dead iff this array is fully deserialized.
      for (int j = 0; j < num_transitions; ++j) {
        DCHECK_IMPLIES(
            !transitions.GetRawTarget(j).IsSmi(),
            !non_atomic_marking_state()->IsWhite(transitions.GetTarget(j)));
      }
#endif
      return false;
    } else if (non_atomic_marking_state()->IsWhite(
                   TransitionsAccessor::GetTargetFromRaw(raw_target))) {
#ifdef DEBUG
      // Targets can only be dead iff this array is fully deserialized.
      for (int j = 0; j < num_transitions; ++j) {
        DCHECK(!transitions.GetRawTarget(j).IsSmi());
      }
#endif
      return true;
    }
  }
  return false;
}

bool MarkCompactCollector::CompactTransitionArray(Map map,
                                                  TransitionArray transitions,
                                                  DescriptorArray descriptors) {
  DCHECK(!map.is_prototype_map());
  int num_transitions = transitions.number_of_entries();
  if (!TransitionArrayNeedsCompaction(transitions, num_transitions)) {
    return false;
  }
  bool descriptors_owner_died = false;
  int transition_index = 0;
  // Compact all live transitions to the left.
  for (int i = 0; i < num_transitions; ++i) {
    Map target = transitions.GetTarget(i);
    DCHECK_EQ(target.constructor_or_back_pointer(), map);
    if (non_atomic_marking_state()->IsWhite(target)) {
      if (!descriptors.is_null() &&
          target.instance_descriptors(isolate()) == descriptors) {
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
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(array);
  RememberedSet<OLD_TO_NEW>::RemoveRange(chunk, start, end,
                                         SlotSet::FREE_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_OLD>::RemoveRange(chunk, start, end,
                                         SlotSet::FREE_EMPTY_BUCKETS);
  heap()->CreateFillerObjectAt(start, static_cast<int>(end - start));
  array.set_number_of_all_descriptors(new_nof_all_descriptors);
}

void MarkCompactCollector::TrimDescriptorArray(Map map,
                                               DescriptorArray descriptors) {
  int number_of_own_descriptors = map.NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) {
    DCHECK(descriptors == ReadOnlyRoots(heap_).empty_descriptor_array());
    return;
  }
  int to_trim =
      descriptors.number_of_all_descriptors() - number_of_own_descriptors;
  if (to_trim > 0) {
    descriptors.set_number_of_descriptors(number_of_own_descriptors);
    RightTrimDescriptorArray(descriptors, to_trim);

    TrimEnumCache(map, descriptors);
    descriptors.Sort();
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
  while (local_weak_objects()->ephemeron_hash_tables_local.Pop(&table)) {
    for (InternalIndex i : table.IterateEntries()) {
      HeapObject key = HeapObject::cast(table.KeyAt(i));
#ifdef VERIFY_HEAP
      if (v8_flags.verify_heap) {
        Object value = table.ValueAt(i);
        if (value.IsHeapObject()) {
          HeapObject heap_object = HeapObject::cast(value);
          CHECK_IMPLIES(
              (!is_shared_heap_ && key.InSharedHeap()) ||
                  non_atomic_marking_state()->IsBlackOrGrey(key),
              (!is_shared_heap_ && heap_object.InSharedHeap()) ||
                  non_atomic_marking_state()->IsBlackOrGrey(heap_object));
        }
      }
#endif
      if (!is_shared_heap_ && key.InSharedHeap()) continue;
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
  while (local_weak_objects()->weak_references_local.Pop(&slot)) {
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
  JSWeakRef weak_ref;
  while (local_weak_objects()->js_weak_refs_local.Pop(&weak_ref)) {
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
  while (local_weak_objects()->weak_cells_local.Pop(&weak_cell)) {
    auto gc_notify_updated_slot = [](HeapObject object, ObjectSlot slot,
                                     Object target) {
      if (target.IsHeapObject()) {
        RecordSlot(object, slot, HeapObject::cast(target));
      }
    };
    HeapObject target = HeapObject::cast(weak_cell.target());
    if (!non_atomic_marking_state()->IsBlackOrGrey(target)) {
      DCHECK(!target.IsUndefined());
      // The value of the WeakCell is dead.
      JSFinalizationRegistry finalization_registry =
          JSFinalizationRegistry::cast(weak_cell.finalization_registry());
      if (!finalization_registry.scheduled_for_cleanup()) {
        heap()->EnqueueDirtyJSFinalizationRegistry(finalization_registry,
                                                   gc_notify_updated_slot);
      }
      // We're modifying the pointers in WeakCell and JSFinalizationRegistry
      // during GC; thus we need to record the slots it writes. The normal write
      // barrier is not enough, since it's disabled before GC.
      weak_cell.Nullify(isolate(), gc_notify_updated_slot);
      DCHECK(finalization_registry.NeedsCleanup());
      DCHECK(finalization_registry.scheduled_for_cleanup());
    } else {
      // The value of the WeakCell is alive.
      ObjectSlot slot = weak_cell.RawField(WeakCell::kTargetOffset);
      RecordSlot(weak_cell, slot, HeapObject::cast(*slot));
    }

    HeapObject unregister_token = weak_cell.unregister_token();
    if (!non_atomic_marking_state()->IsBlackOrGrey(unregister_token)) {
      // The unregister token is dead. Remove any corresponding entries in the
      // key map. Multiple WeakCell with the same token will have all their
      // unregister_token field set to undefined when processing the first
      // WeakCell. Like above, we're modifying pointers during GC, so record the
      // slots.
      JSFinalizationRegistry finalization_registry =
          JSFinalizationRegistry::cast(weak_cell.finalization_registry());
      finalization_registry.RemoveUnregisterToken(
          JSReceiver::cast(unregister_token), isolate(),
          JSFinalizationRegistry::kKeepMatchedCellsInRegistry,
          gc_notify_updated_slot);
    } else {
      // The unregister_token is alive.
      ObjectSlot slot = weak_cell.RawField(WeakCell::kUnregisterTokenOffset);
      RecordSlot(weak_cell, slot, HeapObject::cast(*slot));
    }
  }
  heap()->PostFinalizationRegistryCleanupTaskIfNeeded();
}

bool MarkCompactCollector::IsOnEvacuationCandidate(MaybeObject obj) {
  return Page::FromAddress(obj.ptr())->IsEvacuationCandidate();
}

// static
bool MarkCompactCollector::ShouldRecordRelocSlot(Code host, RelocInfo* rinfo,
                                                 HeapObject target) {
  MemoryChunk* source_chunk = MemoryChunk::FromHeapObject(host);
  BasicMemoryChunk* target_chunk = BasicMemoryChunk::FromHeapObject(target);
  return target_chunk->IsEvacuationCandidate() &&
         !source_chunk->ShouldSkipEvacuationSlotRecording();
}

// static
MarkCompactCollector::RecordRelocSlotInfo
MarkCompactCollector::ProcessRelocInfo(Code host, RelocInfo* rinfo,
                                       HeapObject target) {
  DCHECK_EQ(host, rinfo->host());

  RecordRelocSlotInfo result;
  const RelocInfo::Mode rmode = rinfo->rmode();
  Address addr;
  SlotType slot_type;

  if (rinfo->IsInConstantPool()) {
    addr = rinfo->constant_pool_entry_address();

    if (RelocInfo::IsCodeTargetMode(rmode)) {
      slot_type = SlotType::kConstPoolCodeEntry;
    } else if (RelocInfo::IsCompressedEmbeddedObject(rmode)) {
      slot_type = SlotType::kConstPoolEmbeddedObjectCompressed;
    } else {
      DCHECK(RelocInfo::IsFullEmbeddedObject(rmode));
      slot_type = SlotType::kConstPoolEmbeddedObjectFull;
    }
  } else {
    addr = rinfo->pc();

    if (RelocInfo::IsCodeTargetMode(rmode)) {
      slot_type = SlotType::kCodeEntry;
    } else if (RelocInfo::IsFullEmbeddedObject(rmode)) {
      slot_type = SlotType::kEmbeddedObjectFull;
    } else if (RelocInfo::IsCompressedEmbeddedObject(rmode)) {
      slot_type = SlotType::kEmbeddedObjectCompressed;
    } else {
      DCHECK(RelocInfo::IsDataEmbeddedObject(rmode));
      slot_type = SlotType::kEmbeddedObjectData;
    }
  }

  MemoryChunk* const source_chunk = MemoryChunk::FromHeapObject(host);
  const uintptr_t offset = addr - source_chunk->address();
  DCHECK_LT(offset, static_cast<uintptr_t>(TypedSlotSet::kMaxOffset));
  result.memory_chunk = source_chunk;
  result.slot_type = slot_type;
  result.offset = static_cast<uint32_t>(offset);

  return result;
}

// static
void MarkCompactCollector::RecordRelocSlot(Code host, RelocInfo* rinfo,
                                           HeapObject target) {
  if (!ShouldRecordRelocSlot(host, rinfo, target)) return;
  RecordRelocSlotInfo info = ProcessRelocInfo(host, rinfo, target);

  // Access to TypeSlots need to be protected, since LocalHeaps might
  // publish code in the background thread.
  base::Optional<base::MutexGuard> opt_guard;
  if (v8_flags.concurrent_sparkplug) {
    opt_guard.emplace(info.memory_chunk->mutex());
  }
  RememberedSet<OLD_TO_OLD>::InsertTyped(info.memory_chunk, info.slot_type,
                                         info.offset);
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

template <>
Object MakeSlotValue<OffHeapObjectSlot, HeapObjectReferenceType::STRONG>(
    HeapObject heap_object) {
  return heap_object;
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
static inline void UpdateSlot(PtrComprCageBase cage_base, TSlot slot,
                              typename TSlot::TObject old,
                              HeapObject heap_obj) {
  static_assert(std::is_same<TSlot, FullObjectSlot>::value ||
                    std::is_same<TSlot, ObjectSlot>::value ||
                    std::is_same<TSlot, FullMaybeObjectSlot>::value ||
                    std::is_same<TSlot, MaybeObjectSlot>::value ||
                    std::is_same<TSlot, OffHeapObjectSlot>::value,
                "Only [Full|OffHeap]ObjectSlot and [Full]MaybeObjectSlot are "
                "expected here");
  MapWord map_word = heap_obj.map_word(cage_base, kRelaxedLoad);
  if (map_word.IsForwardingAddress()) {
    DCHECK_IMPLIES((!v8_flags.minor_mc && !Heap::InFromPage(heap_obj)),
                   MarkCompactCollector::IsOnEvacuationCandidate(heap_obj) ||
                       Page::FromHeapObject(heap_obj)->IsFlagSet(
                           Page::COMPACTION_WAS_ABORTED));
    PtrComprCageBase host_cage_base =
        V8_EXTERNAL_CODE_SPACE_BOOL ? GetPtrComprCageBase(heap_obj) : cage_base;
    typename TSlot::TObject target = MakeSlotValue<TSlot, reference_type>(
        map_word.ToForwardingAddress(host_cage_base));
    if (access_mode == AccessMode::NON_ATOMIC) {
      // Needs to be atomic for map space compaction: This slot could be a map
      // word which we update while loading the map word for updating the slot
      // on another page.
      slot.Relaxed_Store(target);
    } else {
      slot.Release_CompareAndSwap(old, target);
    }
    DCHECK(!Heap::InFromPage(target));
    DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(target));
  } else {
    DCHECK(MarkCompactCollector::IsMapOrForwarded(map_word.ToMap()));
  }
}

template <AccessMode access_mode, typename TSlot>
static inline void UpdateSlot(PtrComprCageBase cage_base, TSlot slot) {
  typename TSlot::TObject obj = slot.Relaxed_Load(cage_base);
  HeapObject heap_obj;
  if (TSlot::kCanBeWeak && obj->GetHeapObjectIfWeak(&heap_obj)) {
    UpdateSlot<access_mode, HeapObjectReferenceType::WEAK>(cage_base, slot, obj,
                                                           heap_obj);
  } else if (obj->GetHeapObjectIfStrong(&heap_obj)) {
    UpdateSlot<access_mode, HeapObjectReferenceType::STRONG>(cage_base, slot,
                                                             obj, heap_obj);
  }
}

static inline SlotCallbackResult UpdateOldToSharedSlot(
    PtrComprCageBase cage_base, MaybeObjectSlot slot) {
  MaybeObject obj = slot.Relaxed_Load(cage_base);
  HeapObject heap_obj;

  if (obj.GetHeapObject(&heap_obj)) {
    if (obj.IsWeak()) {
      UpdateSlot<AccessMode::NON_ATOMIC, HeapObjectReferenceType::WEAK>(
          cage_base, slot, obj, heap_obj);
    } else {
      UpdateSlot<AccessMode::NON_ATOMIC, HeapObjectReferenceType::STRONG>(
          cage_base, slot, obj, heap_obj);
    }

    return heap_obj.InSharedWritableHeap() ? KEEP_SLOT : REMOVE_SLOT;
  } else {
    return REMOVE_SLOT;
  }
}

template <AccessMode access_mode, typename TSlot>
static inline void UpdateStrongSlot(PtrComprCageBase cage_base, TSlot slot) {
  typename TSlot::TObject obj = slot.Relaxed_Load(cage_base);
  DCHECK(!HAS_WEAK_HEAP_OBJECT_TAG(obj.ptr()));
  HeapObject heap_obj;
  if (obj.GetHeapObject(&heap_obj)) {
    UpdateSlot<access_mode, HeapObjectReferenceType::STRONG>(cage_base, slot,
                                                             obj, heap_obj);
  }
}

static inline SlotCallbackResult UpdateStrongOldToSharedSlot(
    PtrComprCageBase cage_base, FullMaybeObjectSlot slot) {
  MaybeObject obj = slot.Relaxed_Load(cage_base);
  DCHECK(!HAS_WEAK_HEAP_OBJECT_TAG(obj.ptr()));
  HeapObject heap_obj;
  if (obj.GetHeapObject(&heap_obj)) {
    UpdateSlot<AccessMode::NON_ATOMIC, HeapObjectReferenceType::STRONG>(
        cage_base, slot, obj, heap_obj);
    return heap_obj.InSharedWritableHeap() ? KEEP_SLOT : REMOVE_SLOT;
  }

  return REMOVE_SLOT;
}

template <AccessMode access_mode>
static inline void UpdateStrongCodeSlot(HeapObject host,
                                        PtrComprCageBase cage_base,
                                        PtrComprCageBase code_cage_base,
                                        CodeObjectSlot slot) {
  Object obj = slot.Relaxed_Load(code_cage_base);
  DCHECK(!HAS_WEAK_HEAP_OBJECT_TAG(obj.ptr()));
  HeapObject heap_obj;
  if (obj.GetHeapObject(&heap_obj)) {
    UpdateSlot<access_mode, HeapObjectReferenceType::STRONG>(cage_base, slot,
                                                             obj, heap_obj);

    CodeDataContainer code_data_container =
        CodeDataContainer::cast(HeapObject::FromAddress(
            slot.address() - CodeDataContainer::kCodeOffset));
    Code code = code_data_container.code(code_cage_base);
    Isolate* isolate_for_sandbox = GetIsolateForSandbox(host);
    code_data_container.UpdateCodeEntryPoint(isolate_for_sandbox, code);
  }
}

}  // namespace

// Visitor for updating root pointers and to-space pointers.
// It does not expect to encounter pointers to dead objects.
class PointersUpdatingVisitor final : public ObjectVisitorWithCageBases,
                                      public RootVisitor {
 public:
  explicit PointersUpdatingVisitor(Heap* heap)
      : ObjectVisitorWithCageBases(heap) {}

  void VisitPointer(HeapObject host, ObjectSlot p) override {
    UpdateStrongSlotInternal(cage_base(), p);
  }

  void VisitPointer(HeapObject host, MaybeObjectSlot p) override {
    UpdateSlotInternal(cage_base(), p);
  }

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    for (ObjectSlot p = start; p < end; ++p) {
      UpdateStrongSlotInternal(cage_base(), p);
    }
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final {
    for (MaybeObjectSlot p = start; p < end; ++p) {
      UpdateSlotInternal(cage_base(), p);
    }
  }

  void VisitCodePointer(HeapObject host, CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    UpdateStrongCodeSlot<AccessMode::NON_ATOMIC>(host, cage_base(),
                                                 code_cage_base(), slot);
  }

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) override {
    DCHECK(!MapWord::IsPacked(p.Relaxed_Load().ptr()));
    UpdateRootSlotInternal(cage_base(), p);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot p = start; p < end; ++p) {
      UpdateRootSlotInternal(cage_base(), p);
    }
  }

  void VisitRootPointers(Root root, const char* description,
                         OffHeapObjectSlot start,
                         OffHeapObjectSlot end) override {
    for (OffHeapObjectSlot p = start; p < end; ++p) {
      UpdateRootSlotInternal(cage_base(), p);
    }
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    // This visitor nevers visits code objects.
    UNREACHABLE();
  }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    // This visitor nevers visits code objects.
    UNREACHABLE();
  }

 private:
  static inline void UpdateRootSlotInternal(PtrComprCageBase cage_base,
                                            FullObjectSlot slot) {
    UpdateStrongSlot<AccessMode::NON_ATOMIC>(cage_base, slot);
  }

  static inline void UpdateRootSlotInternal(PtrComprCageBase cage_base,
                                            OffHeapObjectSlot slot) {
    UpdateStrongSlot<AccessMode::NON_ATOMIC>(cage_base, slot);
  }

  static inline void UpdateStrongMaybeObjectSlotInternal(
      PtrComprCageBase cage_base, MaybeObjectSlot slot) {
    UpdateStrongSlot<AccessMode::NON_ATOMIC>(cage_base, slot);
  }

  static inline void UpdateStrongSlotInternal(PtrComprCageBase cage_base,
                                              ObjectSlot slot) {
    UpdateStrongSlot<AccessMode::NON_ATOMIC>(cage_base, slot);
  }

  static inline void UpdateSlotInternal(PtrComprCageBase cage_base,
                                        MaybeObjectSlot slot) {
    UpdateSlot<AccessMode::NON_ATOMIC>(cage_base, slot);
  }
};

#ifdef VERIFY_HEAP
// Visitor for updating root pointers and to-space pointers.
// It does not expect to encounter pointers to dead objects.
class ClientHeapVerifier final : public ObjectVisitorWithCageBases {
 public:
  explicit ClientHeapVerifier(Heap* heap) : ObjectVisitorWithCageBases(heap) {}

  void VisitPointer(HeapObject host, ObjectSlot p) override {
    VerifySlot(cage_base(), p);
  }

  void VisitPointer(HeapObject host, MaybeObjectSlot p) override {
    VerifySlot(cage_base(), p);
  }

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    for (ObjectSlot p = start; p < end; ++p) {
      VerifySlot(cage_base(), p);
    }
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final {
    for (MaybeObjectSlot p = start; p < end; ++p) {
      VerifySlot(cage_base(), p);
    }
  }

  void VisitMapPointer(HeapObject host) override {
    VerifySlot(cage_base(), host.map_slot());
  }

  void VisitCodePointer(HeapObject host, CodeObjectSlot slot) override {
    VerifySlot(code_cage_base(), ObjectSlot(slot.address()));
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {}

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {}

 private:
  void VerifySlot(PtrComprCageBase cage_base, ObjectSlot slot) {
    HeapObject heap_object;
    if (slot.load(cage_base).GetHeapObject(&heap_object)) {
      VerifyHeapObject(heap_object);
    }
  }

  void VerifySlot(PtrComprCageBase cage_base, MaybeObjectSlot slot) {
    HeapObject heap_object;
    if (slot.load(cage_base).GetHeapObject(&heap_object)) {
      VerifyHeapObject(heap_object);
    }
  }

  void VerifyHeapObject(HeapObject heap_object) {
    if (BasicMemoryChunk::FromHeapObject(heap_object)->InReadOnlySpace())
      return;
    if (!heap_object.InSharedHeap()) return;
    CHECK(!heap_object.map_word(kRelaxedLoad).IsForwardingAddress());
  }
};
#endif  // VERIFY_HEAP

static String UpdateReferenceInExternalStringTableEntry(Heap* heap,
                                                        FullObjectSlot p) {
  HeapObject old_string = HeapObject::cast(*p);
  MapWord map_word = old_string.map_word(kRelaxedLoad);

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

  if (new_space) {
    // Append the list of new space pages to be processed.
    for (Page* p : *new_space) {
      if (non_atomic_marking_state()->live_bytes(p) > 0) {
        new_space_evacuation_pages_.push_back(p);
      }
    }
    new_space->EvacuatePrologue();
  }

  if (heap()->new_lo_space()) {
    heap()->new_lo_space()->Flip();
    heap()->new_lo_space()->ResetPendingObject();
  }

  // Old space.
  DCHECK(old_space_evacuation_pages_.empty());
  old_space_evacuation_pages_ = std::move(evacuation_candidates_);
  evacuation_candidates_.clear();
  DCHECK(evacuation_candidates_.empty());
}

void MarkCompactCollector::EvacuateEpilogue() {
  aborted_evacuation_candidates_due_to_oom_.clear();
  aborted_evacuation_candidates_due_to_flags_.clear();

  // New space.
  if (heap()->new_space()) {
    heap()->new_space()->EvacuateEpilogue();
    DCHECK_EQ(0, heap()->new_space()->Size());
  }

  // Old generation. Deallocate evacuated candidate pages.
  ReleaseEvacuationCandidates();

#ifdef DEBUG
  MemoryChunkIterator chunk_iterator(heap());

  while (chunk_iterator.HasNext()) {
    MemoryChunk* chunk = chunk_iterator.Next();

    // Old-to-old slot sets must be empty after evacuation.
    DCHECK_NULL((chunk->slot_set<OLD_TO_OLD, AccessMode::ATOMIC>()));
    DCHECK_NULL((chunk->typed_slot_set<OLD_TO_OLD, AccessMode::ATOMIC>()));

    // Old-to-new slot sets must be empty after evacuation.
    DCHECK_NULL((chunk->slot_set<OLD_TO_NEW, AccessMode::ATOMIC>()));
    DCHECK_NULL((chunk->typed_slot_set<OLD_TO_NEW, AccessMode::ATOMIC>()));

    // GCs need to filter invalidated slots.
    DCHECK_NULL(chunk->invalidated_slots<OLD_TO_OLD>());
    DCHECK_NULL(chunk->invalidated_slots<OLD_TO_NEW>());
    DCHECK_NULL(chunk->invalidated_slots<OLD_TO_SHARED>());
  }
#endif
}

namespace {
ConcurrentAllocator* CreateSharedOldAllocator(Heap* heap) {
  if (v8_flags.shared_string_table && heap->isolate()->shared_isolate()) {
    return new ConcurrentAllocator(nullptr, heap->shared_old_space());
  }

  return nullptr;
}
}  // namespace

class Evacuator : public Malloced {
 public:
  enum EvacuationMode {
    kObjectsNewToOld,
    kPageNewToOld,
    kObjectsOldToOld,
    kPageNewToNew,
  };

  static const char* EvacuationModeName(EvacuationMode mode) {
    switch (mode) {
      case kObjectsNewToOld:
        return "objects-new-to-old";
      case kPageNewToOld:
        return "page-new-to-old";
      case kObjectsOldToOld:
        return "objects-old-to-old";
      case kPageNewToNew:
        return "page-new-to-new";
    }
  }

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
    return v8_flags.page_promotion_threshold *
           MemoryChunkLayout::AllocatableMemoryInDataPage() / 100;
  }

  Evacuator(Heap* heap, RecordMigratedSlotVisitor* record_visitor,
            EvacuationAllocator* local_allocator,
            AlwaysPromoteYoung always_promote_young)
      : heap_(heap),
        local_pretenuring_feedback_(kInitialLocalPretenuringFeedbackCapacity),
        shared_old_allocator_(CreateSharedOldAllocator(heap_)),
        new_space_visitor_(heap_, local_allocator, shared_old_allocator_.get(),
                           record_visitor, &local_pretenuring_feedback_,
                           always_promote_young),
        new_to_new_page_visitor_(heap_, record_visitor,
                                 &local_pretenuring_feedback_),
        new_to_old_page_visitor_(heap_, record_visitor,
                                 &local_pretenuring_feedback_),

        old_space_visitor_(heap_, local_allocator, shared_old_allocator_.get(),
                           record_visitor),
        local_allocator_(local_allocator),
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
  virtual void Finalize();

  virtual GCTracer::Scope::ScopeId GetBackgroundTracingScope() = 0;
  virtual GCTracer::Scope::ScopeId GetTracingScope() = 0;

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

  Heap::PretenuringFeedbackMap local_pretenuring_feedback_;

  // Allocator for the shared heap.
  std::unique_ptr<ConcurrentAllocator> shared_old_allocator_;

  // Visitors for the corresponding spaces.
  EvacuateNewSpaceVisitor new_space_visitor_;
  EvacuateNewSpacePageVisitor<PageEvacuationMode::NEW_TO_NEW>
      new_to_new_page_visitor_;
  EvacuateNewSpacePageVisitor<PageEvacuationMode::NEW_TO_OLD>
      new_to_old_page_visitor_;
  EvacuateOldSpaceVisitor old_space_visitor_;

  // Locally cached collector data.
  EvacuationAllocator* local_allocator_;

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
    AlwaysAllocateScope always_allocate(heap());
    TimedScope timed_scope(&evacuation_time);
    RawEvacuatePage(chunk, &saved_live_bytes);
  }
  ReportCompactionProgress(evacuation_time, saved_live_bytes);
  if (v8_flags.trace_evacuation) {
    PrintIsolate(heap()->isolate(),
                 "evacuation[%p]: page=%p new_space=%d "
                 "page_evacuation=%d executable=%d can_promote=%d "
                 "live_bytes=%" V8PRIdPTR " time=%f success=%d\n",
                 static_cast<void*>(this), static_cast<void*>(chunk),
                 chunk->InNewSpace(),
                 chunk->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION) ||
                     chunk->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION),
                 chunk->IsFlagSet(MemoryChunk::IS_EXECUTABLE),
                 heap()->new_space()->IsPromotionCandidate(chunk),
                 saved_live_bytes, evacuation_time,
                 chunk->IsFlagSet(Page::COMPACTION_WAS_ABORTED));
  }
}

void Evacuator::Finalize() {
  local_allocator_->Finalize();
  if (shared_old_allocator_) shared_old_allocator_->FreeLinearAllocationArea();
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
      : Evacuator(collector->heap(), &record_visitor_, &local_allocator_,
                  AlwaysPromoteYoung::kYes),
        record_visitor_(collector, &ephemeron_remembered_set_),
        local_allocator_(heap_,
                         CompactionSpaceKind::kCompactionSpaceForMarkCompact),
        collector_(collector) {}

  GCTracer::Scope::ScopeId GetBackgroundTracingScope() override {
    return GCTracer::Scope::MC_BACKGROUND_EVACUATE_COPY;
  }

  GCTracer::Scope::ScopeId GetTracingScope() override {
    return GCTracer::Scope::MC_EVACUATE_COPY_PARALLEL;
  }

  void Finalize() override {
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
  EvacuationAllocator local_allocator_;

  MarkCompactCollector* collector_;
};

void FullEvacuator::RawEvacuatePage(MemoryChunk* chunk, intptr_t* live_bytes) {
  const EvacuationMode evacuation_mode = ComputeEvacuationMode(chunk);
  NonAtomicMarkingState* marking_state = collector_->non_atomic_marking_state();
  *live_bytes = marking_state->live_bytes(chunk);
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
               "FullEvacuator::RawEvacuatePage", "evacuation_mode",
               EvacuationModeName(evacuation_mode), "live_bytes", *live_bytes);
  HeapObject failed_object;
  switch (evacuation_mode) {
    case kObjectsNewToOld:
#if DEBUG
      new_space_visitor_.DisableAbortEvacuationAtAddress(chunk);
#endif  // DEBUG
      LiveObjectVisitor::VisitBlackObjectsNoFail(chunk, marking_state,
                                                 &new_space_visitor_);
      marking_state->ClearLiveness(chunk);
      break;
    case kPageNewToOld:
      LiveObjectVisitor::VisitBlackObjectsNoFail(chunk, marking_state,
                                                 &new_to_old_page_visitor_);
      new_to_old_page_visitor_.account_moved_bytes(
          marking_state->live_bytes(chunk));
      break;
    case kPageNewToNew:
      DCHECK(!v8_flags.minor_mc);
      LiveObjectVisitor::VisitBlackObjectsNoFail(chunk, marking_state,
                                                 &new_to_new_page_visitor_);
      new_to_new_page_visitor_.account_moved_bytes(
          marking_state->live_bytes(chunk));
      break;
    case kObjectsOldToOld: {
      RwxMemoryWriteScope rwx_write_scope(
          "Evacuation of objects in Code space requires write access for the "
          "current worker thread.");
#if DEBUG
      old_space_visitor_.SetUpAbortEvacuationAtAddress(chunk);
#endif  // DEBUG
      const bool success = LiveObjectVisitor::VisitBlackObjects(
          chunk, marking_state, &old_space_visitor_, &failed_object);
      if (success) {
        marking_state->ClearLiveness(chunk);
      } else {
        if (v8_flags.crash_on_aborted_evacuation) {
          heap_->FatalProcessOutOfMemory("FullEvacuator::RawEvacuatePage");
        } else {
          // Aborted compaction page. Actual processing happens on the main
          // thread for simplicity reasons.
          collector_->ReportAbortedEvacuationCandidateDueToOOM(
              failed_object.address(), static_cast<Page*>(chunk));
        }
      }
      break;
    }
  }
}

class PageEvacuationJob : public v8::JobTask {
 public:
  PageEvacuationJob(
      Isolate* isolate, std::vector<std::unique_ptr<Evacuator>>* evacuators,
      std::vector<std::pair<ParallelWorkItem, MemoryChunk*>> evacuation_items)
      : evacuators_(evacuators),
        evacuation_items_(std::move(evacuation_items)),
        remaining_evacuation_items_(evacuation_items_.size()),
        generator_(evacuation_items_.size()),
        tracer_(isolate->heap()->tracer()) {}

  void Run(JobDelegate* delegate) override {
    RwxMemoryWriteScope::SetDefaultPermissionsForNewThread();
    Evacuator* evacuator = (*evacuators_)[delegate->GetTaskId()].get();
    if (delegate->IsJoiningThread()) {
      TRACE_GC(tracer_, evacuator->GetTracingScope());
      ProcessItems(delegate, evacuator);
    } else {
      TRACE_GC_EPOCH(tracer_, evacuator->GetBackgroundTracingScope(),
                     ThreadKind::kBackground);
      ProcessItems(delegate, evacuator);
    }
  }

  void ProcessItems(JobDelegate* delegate, Evacuator* evacuator) {
    while (remaining_evacuation_items_.load(std::memory_order_relaxed) > 0) {
      base::Optional<size_t> index = generator_.GetNext();
      if (!index) return;
      for (size_t i = *index; i < evacuation_items_.size(); ++i) {
        auto& work_item = evacuation_items_[i];
        if (!work_item.first.TryAcquire()) break;
        evacuator->EvacuatePage(work_item.second);
        if (remaining_evacuation_items_.fetch_sub(
                1, std::memory_order_relaxed) <= 1) {
          return;
        }
      }
    }
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    const size_t kItemsPerWorker = std::max(1, MB / Page::kPageSize);
    // Ceiling division to ensure enough workers for all
    // |remaining_evacuation_items_|
    const size_t wanted_num_workers =
        (remaining_evacuation_items_.load(std::memory_order_relaxed) +
         kItemsPerWorker - 1) /
        kItemsPerWorker;
    return std::min<size_t>(wanted_num_workers, evacuators_->size());
  }

 private:
  std::vector<std::unique_ptr<Evacuator>>* evacuators_;
  std::vector<std::pair<ParallelWorkItem, MemoryChunk*>> evacuation_items_;
  std::atomic<size_t> remaining_evacuation_items_{0};
  IndexGenerator generator_;

  GCTracer* tracer_;
};

namespace {
template <class Evacuator, class Collector>
size_t CreateAndExecuteEvacuationTasks(
    Collector* collector,
    std::vector<std::pair<ParallelWorkItem, MemoryChunk*>> evacuation_items,
    MigrationObserver* migration_observer) {
  base::Optional<ProfilingMigrationObserver> profiling_observer;
  if (collector->isolate()->log_object_relocation()) {
    profiling_observer.emplace(collector->heap());
  }
  std::vector<std::unique_ptr<v8::internal::Evacuator>> evacuators;
  const int wanted_num_tasks =
      NumberOfParallelCompactionTasks(collector->heap());
  for (int i = 0; i < wanted_num_tasks; i++) {
    auto evacuator = std::make_unique<Evacuator>(collector);
    if (profiling_observer) {
      evacuator->AddObserver(&profiling_observer.value());
    }
    if (migration_observer) {
      evacuator->AddObserver(migration_observer);
    }
    evacuators.push_back(std::move(evacuator));
  }
  V8::GetCurrentPlatform()
      ->CreateJob(
          v8::TaskPriority::kUserBlocking,
          std::make_unique<PageEvacuationJob>(collector->isolate(), &evacuators,
                                              std::move(evacuation_items)))
      ->Join();
  for (auto& evacuator : evacuators) {
    evacuator->Finalize();
  }
  return wanted_num_tasks;
}

bool ShouldMovePage(Page* p, intptr_t live_bytes, intptr_t wasted_bytes,
                    MemoryReductionMode memory_reduction_mode,
                    AlwaysPromoteYoung always_promote_young) {
  Heap* heap = p->heap();
  return v8_flags.page_promotion &&
         (memory_reduction_mode == MemoryReductionMode::kNone) &&
         !p->NeverEvacuate() &&
         (live_bytes + wasted_bytes >
          Evacuator::NewSpacePageEvacuationThreshold()) &&
         (always_promote_young == AlwaysPromoteYoung::kYes ||
          heap->new_space()->IsPromotionCandidate(p)) &&
         heap->CanExpandOldGeneration(live_bytes);
}

void TraceEvacuation(Isolate* isolate, size_t pages_count,
                     size_t wanted_num_tasks, size_t live_bytes,
                     size_t aborted_pages) {
  DCHECK(v8_flags.trace_evacuation);
  PrintIsolate(
      isolate,
      "%8.0f ms: evacuation-summary: parallel=%s pages=%zu "
      "wanted_tasks=%zu cores=%d live_bytes=%" V8PRIdPTR
      " compaction_speed=%.f aborted=%zu\n",
      isolate->time_millis_since_init(),
      v8_flags.parallel_compaction ? "yes" : "no", pages_count,
      wanted_num_tasks, V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1,
      live_bytes,
      isolate->heap()->tracer()->CompactionSpeedInBytesPerMillisecond(),
      aborted_pages);
}

}  // namespace

void MarkCompactCollector::EvacuatePagesInParallel() {
  std::vector<std::pair<ParallelWorkItem, MemoryChunk*>> evacuation_items;
  intptr_t live_bytes = 0;

  // Evacuation of new space pages cannot be aborted, so it needs to run
  // before old space evacuation.
  bool force_page_promotion =
      heap()->IsGCWithStack() && !v8_flags.compact_with_stack;
  for (Page* page : new_space_evacuation_pages_) {
    intptr_t live_bytes_on_page = non_atomic_marking_state()->live_bytes(page);
    DCHECK_LT(0, live_bytes_on_page);
    live_bytes += live_bytes_on_page;
    MemoryReductionMode memory_reduction_mode =
        heap()->ShouldReduceMemory() ? MemoryReductionMode::kShouldReduceMemory
                                     : MemoryReductionMode::kNone;
    if (ShouldMovePage(page, live_bytes_on_page, 0, memory_reduction_mode,
                       AlwaysPromoteYoung::kYes) ||
        force_page_promotion) {
      EvacuateNewSpacePageVisitor<NEW_TO_OLD>::Move(page);
      DCHECK_EQ(heap()->old_space(), page->owner());
      // The move added page->allocated_bytes to the old space, but we are
      // going to sweep the page and add page->live_byte_count.
      heap()->old_space()->DecreaseAllocatedBytes(page->allocated_bytes(),
                                                  page);
    }
    evacuation_items.emplace_back(ParallelWorkItem{}, page);
  }

  if (heap()->IsGCWithStack()) {
    if (!v8_flags.compact_with_stack ||
        !v8_flags.compact_code_space_with_stack) {
      for (Page* page : old_space_evacuation_pages_) {
        if (!v8_flags.compact_with_stack ||
            page->owner_identity() == CODE_SPACE) {
          ReportAbortedEvacuationCandidateDueToFlags(page->area_start(), page);
        }
      }
    }
  }

  if (v8_flags.stress_compaction || v8_flags.stress_compaction_random) {
    // Stress aborting of evacuation by aborting ~10% of evacuation candidates
    // when stress testing.
    const double kFraction = 0.05;

    for (Page* page : old_space_evacuation_pages_) {
      if (page->IsFlagSet(Page::COMPACTION_WAS_ABORTED)) continue;

      if (isolate()->fuzzer_rng()->NextDouble() < kFraction) {
        ReportAbortedEvacuationCandidateDueToFlags(page->area_start(), page);
      }
    }
  }

  for (Page* page : old_space_evacuation_pages_) {
    if (page->IsFlagSet(Page::COMPACTION_WAS_ABORTED)) continue;

    live_bytes += non_atomic_marking_state()->live_bytes(page);
    evacuation_items.emplace_back(ParallelWorkItem{}, page);
  }

  // Promote young generation large objects.
  if (auto* new_lo_space = heap()->new_lo_space()) {
    auto* marking_state =
        heap()->incremental_marking()->non_atomic_marking_state();
    for (auto it = new_lo_space->begin(); it != new_lo_space->end();) {
      LargePage* current = *(it++);
      HeapObject object = current->GetObject();
      DCHECK(!marking_state->IsGrey(object));
      if (marking_state->IsBlack(object)) {
        heap()->lo_space()->PromoteNewLargeObject(current);
        current->SetFlag(Page::PAGE_NEW_OLD_PROMOTION);
        promoted_large_pages_.push_back(current);
        evacuation_items.emplace_back(ParallelWorkItem{}, current);
      }
    }
    new_lo_space->set_objects_size(0);
  }

  const size_t pages_count = evacuation_items.size();
  size_t wanted_num_tasks = 0;
  if (!evacuation_items.empty()) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                 "MarkCompactCollector::EvacuatePagesInParallel", "pages",
                 evacuation_items.size());

    wanted_num_tasks = CreateAndExecuteEvacuationTasks<FullEvacuator>(
        this, std::move(evacuation_items), nullptr);
  }

  const size_t aborted_pages = PostProcessAbortedEvacuationCandidates();

  if (v8_flags.trace_evacuation) {
    TraceEvacuation(isolate(), pages_count, wanted_num_tasks, live_bytes,
                    aborted_pages);
  }
}

class EvacuationWeakObjectRetainer : public WeakObjectRetainer {
 public:
  Object RetainAs(Object object) override {
    if (object.IsHeapObject()) {
      HeapObject heap_object = HeapObject::cast(object);
      MapWord map_word = heap_object.map_word(kRelaxedLoad);
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
                                             &visitor);
}

template <class Visitor, typename MarkingState>
bool LiveObjectVisitor::VisitBlackObjects(MemoryChunk* chunk,
                                          MarkingState* marking_state,
                                          Visitor* visitor,
                                          HeapObject* failed_object) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
               "LiveObjectVisitor::VisitBlackObjects");
  for (auto object_and_size :
       LiveObjectRange<kBlackObjects>(chunk, marking_state->bitmap(chunk))) {
    HeapObject const object = object_and_size.first;
    if (!visitor->Visit(object, object_and_size.second)) {
      *failed_object = object;
      return false;
    }
  }
  return true;
}

template <class Visitor, typename MarkingState>
void LiveObjectVisitor::VisitBlackObjectsNoFail(MemoryChunk* chunk,
                                                MarkingState* marking_state,
                                                Visitor* visitor) {
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

  if (heap()->new_space()) {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_REBALANCE);
    if (!heap()->new_space()->EnsureCurrentCapacity()) {
      heap()->FatalProcessOutOfMemory("NewSpace::Rebalance");
    }
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_CLEAN_UP);

    for (Page* p : new_space_evacuation_pages_) {
      // Full GCs don't promote pages within new space.
      DCHECK(!p->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION));
      if (p->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION)) {
        p->ClearFlag(Page::PAGE_NEW_OLD_PROMOTION);
        DCHECK_EQ(OLD_SPACE, p->owner_identity());
        sweeper()->AddPage(OLD_SPACE, p, Sweeper::REGULAR);
      } else if (v8_flags.minor_mc) {
        // Sweep non-promoted pages to add them back to the free list.
        DCHECK_EQ(NEW_SPACE, p->owner_identity());
        sweeper()->AddPage(NEW_SPACE, p, Sweeper::REGULAR);
      }
    }
    new_space_evacuation_pages_.clear();

    for (LargePage* p : promoted_large_pages_) {
      DCHECK(p->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION));
      p->ClearFlag(Page::PAGE_NEW_OLD_PROMOTION);
      HeapObject object = p->GetObject();
      Marking::MarkWhite(non_atomic_marking_state()->MarkBitFrom(object));
      p->ProgressBar().ResetIfEnabled();
      non_atomic_marking_state()->SetLiveBytes(p, 0);
    }
    promoted_large_pages_.clear();

    for (Page* p : old_space_evacuation_pages_) {
      if (p->IsFlagSet(Page::COMPACTION_WAS_ABORTED)) {
        sweeper()->AddPage(p->owner_identity(), p, Sweeper::REGULAR);
        p->ClearFlag(Page::COMPACTION_WAS_ABORTED);
      }
    }
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_EPILOGUE);
    EvacuateEpilogue();
  }

#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap && !sweeper()->sweeping_in_progress()) {
    FullEvacuationVerifier verifier(heap());
    verifier.Run();
  }
#endif
}

class UpdatingItem : public ParallelWorkItem {
 public:
  virtual ~UpdatingItem() = default;
  virtual void Process() = 0;
};

class PointersUpdatingJob : public v8::JobTask {
 public:
  explicit PointersUpdatingJob(
      Isolate* isolate,
      std::vector<std::unique_ptr<UpdatingItem>> updating_items,
      GCTracer::Scope::ScopeId scope, GCTracer::Scope::ScopeId background_scope)
      : updating_items_(std::move(updating_items)),
        remaining_updating_items_(updating_items_.size()),
        generator_(updating_items_.size()),
        tracer_(isolate->heap()->tracer()),
        scope_(scope),
        background_scope_(background_scope) {}

  void Run(JobDelegate* delegate) override {
    RwxMemoryWriteScope::SetDefaultPermissionsForNewThread();
    if (delegate->IsJoiningThread()) {
      TRACE_GC(tracer_, scope_);
      UpdatePointers(delegate);
    } else {
      TRACE_GC_EPOCH(tracer_, background_scope_, ThreadKind::kBackground);
      UpdatePointers(delegate);
    }
  }

  void UpdatePointers(JobDelegate* delegate) {
    while (remaining_updating_items_.load(std::memory_order_relaxed) > 0) {
      base::Optional<size_t> index = generator_.GetNext();
      if (!index) return;
      for (size_t i = *index; i < updating_items_.size(); ++i) {
        auto& work_item = updating_items_[i];
        if (!work_item->TryAcquire()) break;
        work_item->Process();
        if (remaining_updating_items_.fetch_sub(1, std::memory_order_relaxed) <=
            1) {
          return;
        }
      }
    }
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    size_t items = remaining_updating_items_.load(std::memory_order_relaxed);
    if (!v8_flags.parallel_pointer_update) return items > 0;
    const size_t kMaxPointerUpdateTasks = 8;
    size_t max_concurrency = std::min<size_t>(kMaxPointerUpdateTasks, items);
    DCHECK_IMPLIES(items > 0, max_concurrency > 0);
    return max_concurrency;
  }

 private:
  std::vector<std::unique_ptr<UpdatingItem>> updating_items_;
  std::atomic<size_t> remaining_updating_items_{0};
  IndexGenerator generator_;

  GCTracer* tracer_;
  GCTracer::Scope::ScopeId scope_;
  GCTracer::Scope::ScopeId background_scope_;
};

template <typename MarkingState>
class ToSpaceUpdatingItem : public UpdatingItem {
 public:
  explicit ToSpaceUpdatingItem(Heap* heap, MemoryChunk* chunk, Address start,
                               Address end, MarkingState* marking_state)
      : heap_(heap),
        chunk_(chunk),
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
    PointersUpdatingVisitor visitor(heap_);
    for (Address cur = start_; cur < end_;) {
      HeapObject object = HeapObject::FromAddress(cur);
      Map map = object.map(visitor.cage_base());
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
    PointersUpdatingVisitor visitor(heap_);
    for (auto object_and_size : LiveObjectRange<kAllLiveObjects>(
             chunk_, marking_state_->bitmap(chunk_))) {
      object_and_size.first.IterateBodyFast(visitor.cage_base(), &visitor);
    }
  }

  Heap* heap_;
  MemoryChunk* chunk_;
  Address start_;
  Address end_;
  MarkingState* marking_state_;
};

template <typename MarkingState, GarbageCollector collector>
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
  inline void CheckSlotForOldToSharedUntyped(PtrComprCageBase cage_base,
                                             MemoryChunk* chunk, TSlot slot) {
    HeapObject heap_object;

    if (!slot.load(cage_base).GetHeapObject(&heap_object)) {
      return;
    }

    if (heap_object.InSharedWritableHeap()) {
      RememberedSet<OLD_TO_SHARED>::Insert<AccessMode::NON_ATOMIC>(
          chunk, slot.address());
    }
  }

  inline void CheckSlotForOldToSharedTyped(MemoryChunk* chunk,
                                           SlotType slot_type, Address addr) {
    HeapObject heap_object =
        UpdateTypedSlotHelper::GetTargetObject(chunk->heap(), slot_type, addr);

#if DEBUG
    UpdateTypedSlotHelper::UpdateTypedSlot(
        chunk->heap(), slot_type, addr,
        [heap_object](FullMaybeObjectSlot slot) {
          DCHECK_EQ((*slot).GetHeapObjectAssumeStrong(), heap_object);
          return KEEP_SLOT;
        });
#endif  // DEBUG

    if (heap_object.InSharedWritableHeap()) {
      const uintptr_t offset = addr - chunk->address();
      DCHECK_LT(offset, static_cast<uintptr_t>(TypedSlotSet::kMaxOffset));
      RememberedSet<OLD_TO_SHARED>::InsertTyped(chunk, slot_type,
                                                static_cast<uint32_t>(offset));
    }
  }

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
      DCHECK_IMPLIES(v8_flags.minor_mc,
                     Page::FromHeapObject(heap_object)->IsLargePage());
      MapWord map_word = heap_object.map_word(kRelaxedLoad);
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
      if (v8_flags.minor_mc) {
        MapWord map_word = heap_object.map_word(kRelaxedLoad);
        if (map_word.IsForwardingAddress()) {
          HeapObjectReference::Update(THeapObjectSlot(slot),
                                      map_word.ToForwardingAddress());
          bool success = (*slot).GetHeapObject(&heap_object);
          USE(success);
          DCHECK(success);
        } else if (marking_state_->IsBlack(heap_object)) {
          return KEEP_SLOT;
        }
        return REMOVE_SLOT;
      }
      if (Page::FromHeapObject(heap_object)
              ->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION)) {
        if (marking_state_->IsBlack(heap_object)) {
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
    const bool has_shared_isolate = this->heap_->isolate()->shared_isolate();
    const PtrComprCageBase cage_base = heap_->isolate();
    if (chunk_->slot_set<OLD_TO_NEW, AccessMode::NON_ATOMIC>() != nullptr) {
      // Marking bits are cleared already when the page is already swept. This
      // is fine since in that case the sweeper has already removed dead invalid
      // objects as well.
      InvalidatedSlotsFilter::LivenessCheck liveness_check =
          updating_mode_ == RememberedSetUpdatingMode::ALL &&
                  !chunk_->SweepingDone()
              ? InvalidatedSlotsFilter::LivenessCheck::kYes
              : InvalidatedSlotsFilter::LivenessCheck::kNo;
      InvalidatedSlotsFilter filter =
          InvalidatedSlotsFilter::OldToNew(chunk_, liveness_check);
      int slots = RememberedSet<OLD_TO_NEW>::Iterate(
          chunk_,
          [this, &filter, has_shared_isolate, cage_base](MaybeObjectSlot slot) {
            if (!filter.IsValid(slot.address())) return REMOVE_SLOT;
            SlotCallbackResult result = CheckAndUpdateOldToNewSlot(slot);
            // A new space string might have been promoted into the shared heap
            // during GC.
            if (has_shared_isolate) {
              CheckSlotForOldToSharedUntyped(cage_base, chunk_, slot);
            }
            return result;
          },
          SlotSet::FREE_EMPTY_BUCKETS);

      DCHECK_IMPLIES(collector == GarbageCollector::MARK_COMPACTOR, slots == 0);

      if (slots == 0) {
        chunk_->ReleaseSlotSet<OLD_TO_NEW>();
      }
    }

    if (chunk_->invalidated_slots<OLD_TO_NEW>() != nullptr) {
      // The invalidated slots are not needed after old-to-new slots were
      // processed.
      chunk_->ReleaseInvalidatedSlots<OLD_TO_NEW>();
    }

    if ((updating_mode_ == RememberedSetUpdatingMode::ALL) &&
        (chunk_->slot_set<OLD_TO_OLD, AccessMode::NON_ATOMIC>() != nullptr)) {
      InvalidatedSlotsFilter filter = InvalidatedSlotsFilter::OldToOld(
          chunk_, InvalidatedSlotsFilter::LivenessCheck::kNo);
      RememberedSet<OLD_TO_OLD>::Iterate(
          chunk_,
          [this, has_shared_isolate, &filter, cage_base](MaybeObjectSlot slot) {
            if (filter.IsValid(slot.address())) {
              UpdateSlot<AccessMode::NON_ATOMIC>(cage_base, slot);
              // A string might have been promoted into the shared heap during
              // GC.
              if (has_shared_isolate) {
                CheckSlotForOldToSharedUntyped(cage_base, chunk_, slot);
              }
            }
            // Always keep slot since all slots are dropped at once after
            // iteration.
            return KEEP_SLOT;
          },
          SlotSet::KEEP_EMPTY_BUCKETS);
      chunk_->ReleaseSlotSet<OLD_TO_OLD>();
    }
    if ((updating_mode_ == RememberedSetUpdatingMode::ALL) &&
        chunk_->invalidated_slots<OLD_TO_OLD>() != nullptr) {
      // The invalidated slots are not needed after old-to-old slots were
      // processed.
      chunk_->ReleaseInvalidatedSlots<OLD_TO_OLD>();
    }
    if (V8_EXTERNAL_CODE_SPACE_BOOL) {
      if ((updating_mode_ == RememberedSetUpdatingMode::ALL) &&
          (chunk_->slot_set<OLD_TO_CODE, AccessMode::NON_ATOMIC>() !=
           nullptr)) {
        PtrComprCageBase cage_base = heap_->isolate();
#ifdef V8_EXTERNAL_CODE_SPACE
        PtrComprCageBase code_cage_base(heap_->isolate()->code_cage_base());
#else
        PtrComprCageBase code_cage_base = cage_base;
#endif
        RememberedSet<OLD_TO_CODE>::Iterate(
            chunk_,
            [=](MaybeObjectSlot slot) {
              HeapObject host = HeapObject::FromAddress(
                  slot.address() - CodeDataContainer::kCodeOffset);
              DCHECK(host.IsCodeDataContainer(cage_base));
              UpdateStrongCodeSlot<AccessMode::NON_ATOMIC>(
                  host, cage_base, code_cage_base,
                  CodeObjectSlot(slot.address()));
              // Always keep slot since all slots are dropped at once after
              // iteration.
              return KEEP_SLOT;
            },
            SlotSet::FREE_EMPTY_BUCKETS);
        chunk_->ReleaseSlotSet<OLD_TO_CODE>();
      }
      // The invalidated slots are not needed after old-to-code slots were
      // processed, but since there are no invalidated OLD_TO_CODE slots,
      // there's nothing to clear.
    }
    if (updating_mode_ == RememberedSetUpdatingMode::ALL) {
      if (chunk_->slot_set<OLD_TO_SHARED, AccessMode::NON_ATOMIC>()) {
        // Client GCs need to remove invalidated OLD_TO_SHARED slots.
        DCHECK(!heap_->IsShared());
        InvalidatedSlotsFilter filter = InvalidatedSlotsFilter::OldToShared(
            chunk_, InvalidatedSlotsFilter::LivenessCheck::kNo);
        RememberedSet<OLD_TO_SHARED>::Iterate(
            chunk_,
            [&filter](MaybeObjectSlot slot) {
              return filter.IsValid(slot.address()) ? KEEP_SLOT : REMOVE_SLOT;
            },
            SlotSet::FREE_EMPTY_BUCKETS);
      }
      chunk_->ReleaseInvalidatedSlots<OLD_TO_SHARED>();
    }
  }

  void UpdateTypedPointers() {
    const bool has_shared_isolate = heap_->isolate()->shared_isolate();
    if (chunk_->typed_slot_set<OLD_TO_NEW, AccessMode::NON_ATOMIC>() !=
        nullptr) {
      CHECK_NE(chunk_->owner(), heap_->map_space());
      const auto check_and_update_old_to_new_slot_fn =
          [this](FullMaybeObjectSlot slot) {
            return CheckAndUpdateOldToNewSlot(slot);
          };
      RememberedSet<OLD_TO_NEW>::IterateTyped(
          chunk_,
          [this, has_shared_isolate, &check_and_update_old_to_new_slot_fn](
              SlotType slot_type, Address slot) {
            SlotCallbackResult result = UpdateTypedSlotHelper::UpdateTypedSlot(
                heap_, slot_type, slot, check_and_update_old_to_new_slot_fn);
            // A new space string might have been promoted into the shared heap
            // during GC.
            if (has_shared_isolate) {
              CheckSlotForOldToSharedTyped(chunk_, slot_type, slot);
            }
            return result;
          });
    }
    if ((updating_mode_ == RememberedSetUpdatingMode::ALL) &&
        (chunk_->typed_slot_set<OLD_TO_OLD, AccessMode::NON_ATOMIC>() !=
         nullptr)) {
      CHECK_NE(chunk_->owner(), heap_->map_space());
      RememberedSet<OLD_TO_OLD>::IterateTyped(
          chunk_, [this, has_shared_isolate](SlotType slot_type, Address slot) {
            // Using UpdateStrongSlot is OK here, because there are no weak
            // typed slots.
            PtrComprCageBase cage_base = heap_->isolate();
            SlotCallbackResult result = UpdateTypedSlotHelper::UpdateTypedSlot(
                heap_, slot_type, slot, [cage_base](FullMaybeObjectSlot slot) {
                  UpdateStrongSlot<AccessMode::NON_ATOMIC>(cage_base, slot);
                  // Always keep slot since all slots are dropped at once after
                  // iteration.
                  return KEEP_SLOT;
                });
            // A string might have been promoted into the shared heap during GC.
            if (has_shared_isolate) {
              CheckSlotForOldToSharedTyped(chunk_, slot_type, slot);
            }
            return result;
          });
      chunk_->ReleaseTypedSlotSet<OLD_TO_OLD>();
    }
  }

  Heap* heap_;
  MarkingState* marking_state_;
  MemoryChunk* chunk_;
  RememberedSetUpdatingMode updating_mode_;
};

std::unique_ptr<UpdatingItem>
MarkCompactCollector::CreateRememberedSetUpdatingItem(
    MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) {
  return std::make_unique<RememberedSetUpdatingItem<
      NonAtomicMarkingState, GarbageCollector::MARK_COMPACTOR>>(
      heap(), non_atomic_marking_state(), chunk, updating_mode);
}

namespace {
template <typename IterateableSpace, typename Collector>
int CollectRememberedSetUpdatingItems(
    Collector* collector, std::vector<std::unique_ptr<UpdatingItem>>* items,
    IterateableSpace* space, RememberedSetUpdatingMode mode) {
  int pages = 0;
  for (MemoryChunk* chunk : *space) {
    // No need to update pointers on evacuation candidates. Evacuated pages will
    // be released after this phase.
    if (chunk->IsEvacuationCandidate()) continue;
    const bool contains_old_to_old_slots =
        chunk->slot_set<OLD_TO_OLD>() != nullptr ||
        chunk->typed_slot_set<OLD_TO_OLD>() != nullptr;
    const bool contains_old_to_code_slots =
        V8_EXTERNAL_CODE_SPACE_BOOL &&
        chunk->slot_set<OLD_TO_CODE>() != nullptr;
    const bool contains_old_to_new_slots =
        chunk->slot_set<OLD_TO_NEW>() != nullptr ||
        chunk->typed_slot_set<OLD_TO_NEW>() != nullptr;
    const bool contains_old_to_shared_slots =
        chunk->slot_set<OLD_TO_SHARED>() != nullptr ||
        chunk->typed_slot_set<OLD_TO_SHARED>() != nullptr;
    const bool contains_old_to_old_invalidated_slots =
        chunk->invalidated_slots<OLD_TO_OLD>() != nullptr;
    const bool contains_old_to_new_invalidated_slots =
        chunk->invalidated_slots<OLD_TO_NEW>() != nullptr;
    const bool contains_old_to_shared_invalidated_slots =
        chunk->invalidated_slots<OLD_TO_SHARED>() != nullptr;
    if (!contains_old_to_new_slots && !contains_old_to_old_slots &&
        !contains_old_to_old_invalidated_slots &&
        !contains_old_to_new_invalidated_slots && !contains_old_to_code_slots &&
        !contains_old_to_shared_slots &&
        !contains_old_to_shared_invalidated_slots)
      continue;
    if (mode == RememberedSetUpdatingMode::ALL || contains_old_to_new_slots ||
        contains_old_to_old_invalidated_slots ||
        contains_old_to_new_invalidated_slots) {
      items->emplace_back(
          collector->CreateRememberedSetUpdatingItem(chunk, mode));
      pages++;
    }
  }
  return pages;
}
}  // namespace

class EphemeronTableUpdatingItem : public UpdatingItem {
 public:
  enum EvacuationState { kRegular, kAborted };

  explicit EphemeronTableUpdatingItem(Heap* heap) : heap_(heap) {}
  ~EphemeronTableUpdatingItem() override = default;

  void Process() override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                 "EphemeronTableUpdatingItem::Process");
    PtrComprCageBase cage_base(heap_->isolate());

    for (auto it = heap_->ephemeron_remembered_set_.begin();
         it != heap_->ephemeron_remembered_set_.end();) {
      EphemeronHashTable table = it->first;
      auto& indices = it->second;
      if (table.map_word(cage_base, kRelaxedLoad).IsForwardingAddress()) {
        // The table has moved, and RecordMigratedSlotVisitor::VisitEphemeron
        // inserts entries for the moved table into ephemeron_remembered_set_.
        it = heap_->ephemeron_remembered_set_.erase(it);
        continue;
      }
      DCHECK(table.map(cage_base).IsMap(cage_base));
      DCHECK(table.IsEphemeronHashTable(cage_base));
      for (auto iti = indices.begin(); iti != indices.end();) {
        // EphemeronHashTable keys must be heap objects.
        HeapObjectSlot key_slot(table.RawFieldOfElementAt(
            EphemeronHashTable::EntryToIndex(InternalIndex(*iti))));
        HeapObject key = key_slot.ToHeapObject();
        MapWord map_word = key.map_word(cage_base, kRelaxedLoad);
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

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_TO_NEW_ROOTS);
    // The external string table is updated at the end.
    PointersUpdatingVisitor updating_visitor(heap());
    heap_->IterateRootsIncludingClients(
        &updating_visitor,
        base::EnumSet<SkipRoot>{SkipRoot::kExternalStringTable});
  }

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_CLIENT_HEAPS);
    UpdatePointersInClientHeaps();
  }

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_SLOTS_MAIN);
    std::vector<std::unique_ptr<UpdatingItem>> updating_items;

    CollectRememberedSetUpdatingItems(this, &updating_items,
                                      heap()->old_space(),
                                      RememberedSetUpdatingMode::ALL);
    CollectRememberedSetUpdatingItems(this, &updating_items,
                                      heap()->code_space(),
                                      RememberedSetUpdatingMode::ALL);
    CollectRememberedSetUpdatingItems(this, &updating_items, heap()->lo_space(),
                                      RememberedSetUpdatingMode::ALL);
    CollectRememberedSetUpdatingItems(this, &updating_items,
                                      heap()->code_lo_space(),
                                      RememberedSetUpdatingMode::ALL);
    if (heap()->map_space()) {
      CollectRememberedSetUpdatingItems(this, &updating_items,
                                        heap()->map_space(),
                                        RememberedSetUpdatingMode::ALL);
    }

    // Iterating to space may require a valid body descriptor for e.g.
    // WasmStruct which races with updating a slot in Map. Since to space is
    // empty after a full GC, such races can't happen.
    DCHECK_IMPLIES(heap()->new_space(), heap()->new_space()->Size() == 0);

    updating_items.push_back(
        std::make_unique<EphemeronTableUpdatingItem>(heap()));

    V8::GetCurrentPlatform()
        ->CreateJob(
            v8::TaskPriority::kUserBlocking,
            std::make_unique<PointersUpdatingJob>(
                isolate(), std::move(updating_items),
                GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_PARALLEL,
                GCTracer::Scope::MC_BACKGROUND_EVACUATE_UPDATE_POINTERS))
        ->Join();
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

void MarkCompactCollector::UpdatePointersInClientHeaps() {
  if (!isolate()->is_shared()) return;

  isolate()->global_safepoint()->IterateClientIsolates(
      [this](Isolate* client) { UpdatePointersInClientHeap(client); });
}

void MarkCompactCollector::UpdatePointersInClientHeap(Isolate* client) {
  PtrComprCageBase cage_base(client);
  MemoryChunkIterator chunk_iterator(client->heap());

  while (chunk_iterator.HasNext()) {
    MemoryChunk* chunk = chunk_iterator.Next();
    CodePageMemoryModificationScope unprotect_code_page(chunk);

    DCHECK_NULL(chunk->invalidated_slots<OLD_TO_SHARED>());
    RememberedSet<OLD_TO_SHARED>::Iterate(
        chunk,
        [cage_base](MaybeObjectSlot slot) {
          return UpdateOldToSharedSlot(cage_base, slot);
        },
        SlotSet::FREE_EMPTY_BUCKETS);

    if (chunk->InYoungGeneration()) chunk->ReleaseSlotSet<OLD_TO_SHARED>();

    RememberedSet<OLD_TO_SHARED>::IterateTyped(chunk, [this](SlotType slot_type,
                                                             Address slot) {
      // Using UpdateStrongSlot is OK here, because there are no weak
      // typed slots.
      PtrComprCageBase cage_base = heap_->isolate();
      return UpdateTypedSlotHelper::UpdateTypedSlot(
          heap_, slot_type, slot, [cage_base](FullMaybeObjectSlot slot) {
            return UpdateStrongOldToSharedSlot(cage_base, slot);
          });
    });
    if (chunk->InYoungGeneration()) chunk->ReleaseTypedSlotSet<OLD_TO_SHARED>();
  }

#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) {
    ClientHeapVerifier verifier_visitor(client->heap());

    HeapObjectIterator iterator(client->heap(),
                                HeapObjectIterator::kNoFiltering);
    for (HeapObject obj = iterator.Next(); !obj.is_null();
         obj = iterator.Next()) {
      obj.IterateFast(cage_base, &verifier_visitor);
    }
  }
#endif  // VERIFY_HEAP
}

void MarkCompactCollector::ReportAbortedEvacuationCandidateDueToOOM(
    Address failed_start, Page* page) {
  DCHECK(!page->IsFlagSet(Page::COMPACTION_WAS_ABORTED));
  page->SetFlag(Page::COMPACTION_WAS_ABORTED);
  base::MutexGuard guard(&mutex_);
  aborted_evacuation_candidates_due_to_oom_.push_back(
      std::make_pair(failed_start, page));
}

void MarkCompactCollector::ReportAbortedEvacuationCandidateDueToFlags(
    Address failed_start, Page* page) {
  DCHECK(!page->IsFlagSet(Page::COMPACTION_WAS_ABORTED));
  page->SetFlag(Page::COMPACTION_WAS_ABORTED);
  base::MutexGuard guard(&mutex_);
  aborted_evacuation_candidates_due_to_flags_.push_back(
      std::make_pair(failed_start, page));
}

namespace {

void ReRecordPage(Heap* heap,
                  v8::internal::NonAtomicMarkingState* marking_state,
                  Address failed_start, Page* page) {
  DCHECK(page->IsFlagSet(Page::COMPACTION_WAS_ABORTED));

  // Aborted compaction page. We have to record slots here, since we
  // might not have recorded them in first place.

  // Remove mark bits in evacuated area.
  marking_state->bitmap(page)->ClearRange(
      page->AddressToMarkbitIndex(page->area_start()),
      page->AddressToMarkbitIndex(failed_start));

  // Remove outdated slots.
  RememberedSet<OLD_TO_NEW>::RemoveRange(page, page->address(), failed_start,
                                         SlotSet::FREE_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_NEW>::RemoveRangeTyped(page, page->address(),
                                              failed_start);

  RememberedSet<OLD_TO_SHARED>::RemoveRange(page, page->address(), failed_start,
                                            SlotSet::FREE_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_SHARED>::RemoveRangeTyped(page, page->address(),
                                                 failed_start);

  // Remove invalidated slots.
  if (failed_start > page->area_start()) {
    InvalidatedSlotsCleanup old_to_new_cleanup =
        InvalidatedSlotsCleanup::OldToNew(page);
    old_to_new_cleanup.Free(page->area_start(), failed_start);

    InvalidatedSlotsCleanup old_to_shared_cleanup =
        InvalidatedSlotsCleanup::OldToShared(page);
    old_to_shared_cleanup.Free(page->area_start(), failed_start);
  }

  // Recompute live bytes.
  LiveObjectVisitor::RecomputeLiveBytes(page, marking_state);
  // Re-record slots.
  EvacuateRecordOnlyVisitor record_visitor(heap);
  LiveObjectVisitor::VisitBlackObjectsNoFail(page, marking_state,
                                             &record_visitor);
  // Array buffers will be processed during pointer updating.
}

}  // namespace

size_t MarkCompactCollector::PostProcessAbortedEvacuationCandidates() {
  CHECK_IMPLIES(v8_flags.crash_on_aborted_evacuation,
                aborted_evacuation_candidates_due_to_oom_.empty());
  for (auto start_and_page : aborted_evacuation_candidates_due_to_oom_) {
    ReRecordPage(heap(), non_atomic_marking_state(), start_and_page.first,
                 start_and_page.second);
  }
  for (auto start_and_page : aborted_evacuation_candidates_due_to_flags_) {
    ReRecordPage(heap(), non_atomic_marking_state(), start_and_page.first,
                 start_and_page.second);
  }
  const size_t aborted_pages =
      aborted_evacuation_candidates_due_to_oom_.size() +
      aborted_evacuation_candidates_due_to_flags_.size();
  size_t aborted_pages_verified = 0;
  for (Page* p : old_space_evacuation_pages_) {
    if (p->IsFlagSet(Page::COMPACTION_WAS_ABORTED)) {
      // Only clear EVACUATION_CANDIDATE flag after all slots were re-recorded
      // on all aborted pages. Necessary since repopulating
      // OLD_TO_OLD still requires the EVACUATION_CANDIDATE flag. After clearing
      // the evacuation candidate flag the page is again in a regular state.
      p->ClearEvacuationCandidate();
      aborted_pages_verified++;
    } else {
      DCHECK(p->IsEvacuationCandidate());
      DCHECK(p->SweepingDone());
    }
  }
  DCHECK_EQ(aborted_pages_verified, aborted_pages);
  USE(aborted_pages_verified);
  return aborted_pages;
}

void MarkCompactCollector::ReleaseEvacuationCandidates() {
  for (Page* p : old_space_evacuation_pages_) {
    if (!p->IsEvacuationCandidate()) continue;
    PagedSpace* space = static_cast<PagedSpace*>(p->owner());
    non_atomic_marking_state()->SetLiveBytes(p, 0);
    CHECK(p->SweepingDone());
    space->memory_chunk_list().Remove(p);
    space->ReleasePage(p);
  }
  old_space_evacuation_pages_.clear();
  compacting_ = false;
}

void MarkCompactCollector::SweepLargeSpace(LargeObjectSpace* space) {
  auto* marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
  PtrComprCageBase cage_base(heap()->isolate());
  size_t surviving_object_size = 0;
  for (auto it = space->begin(); it != space->end();) {
    LargePage* current = *(it++);
    HeapObject object = current->GetObject();
    DCHECK(!marking_state->IsGrey(object));
    if (!marking_state->IsBlack(object)) {
      // Object is dead and page can be released.
      space->RemovePage(current);
      heap()->memory_allocator()->Free(MemoryAllocator::FreeMode::kConcurrently,
                                       current);

      continue;
    }
    Marking::MarkWhite(non_atomic_marking_state()->MarkBitFrom(object));
    current->ProgressBar().ResetIfEnabled();
    non_atomic_marking_state()->SetLiveBytes(current, 0);
    surviving_object_size += static_cast<size_t>(object.Size(cage_base));
  }
  space->set_objects_size(surviving_object_size);
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
        if (v8_flags.gc_verbose) {
          PrintIsolate(isolate(), "sweeping: released page: %p",
                       static_cast<void*>(p));
        }
        space->memory_chunk_list().Remove(p);
        space->ReleasePage(p);
        continue;
      }
      unused_page_present = true;
    }

    sweeper()->AddPage(space->identity(), p, Sweeper::REGULAR);
    will_be_swept++;
  }

  if (v8_flags.gc_verbose) {
    PrintIsolate(isolate(), "sweeping: space=%s initialized_for_sweeping=%d",
                 space->name(), will_be_swept);
  }
}

void MarkCompactCollector::StartSweepNewSpace() {
  PagedSpaceBase* paged_space = heap()->paged_new_space()->paged_space();
  paged_space->ClearAllocatorState();

  int will_be_swept = 0;

  for (auto it = paged_space->begin(); it != paged_space->end();) {
    Page* p = *(it++);
    DCHECK(p->SweepingDone());

    if (non_atomic_marking_state()->live_bytes(p) > 0) {
      // Non-empty pages will be evacuated/promoted.
      continue;
    }

    // New space preallocates all its pages. Don't free empty pages since they
    // will just be reallocated.
    DCHECK_EQ(NEW_SPACE, paged_space->identity());
    sweeper_->AddPage(NEW_SPACE, p, Sweeper::REGULAR);
    will_be_swept++;
  }

  if (v8_flags.gc_verbose) {
    PrintIsolate(isolate(), "sweeping: space=%s initialized_for_sweeping=%d",
                 paged_space->name(), will_be_swept);
  }
}

void MarkCompactCollector::Sweep() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_SWEEP);
#ifdef DEBUG
  state_ = SWEEP_SPACES;
#endif

  {
    {
      GCTracer::Scope sweep_scope(
          heap()->tracer(), GCTracer::Scope::MC_SWEEP_LO, ThreadKind::kMain);
      SweepLargeSpace(heap()->lo_space());
    }
    {
      GCTracer::Scope sweep_scope(heap()->tracer(),
                                  GCTracer::Scope::MC_SWEEP_CODE_LO,
                                  ThreadKind::kMain);
      SweepLargeSpace(heap()->code_lo_space());
    }
    {
      GCTracer::Scope sweep_scope(
          heap()->tracer(), GCTracer::Scope::MC_SWEEP_OLD, ThreadKind::kMain);
      StartSweepSpace(heap()->old_space());
    }
    {
      GCTracer::Scope sweep_scope(
          heap()->tracer(), GCTracer::Scope::MC_SWEEP_CODE, ThreadKind::kMain);
      StartSweepSpace(heap()->code_space());
    }
    if (heap()->map_space()) {
      GCTracer::Scope sweep_scope(
          heap()->tracer(), GCTracer::Scope::MC_SWEEP_MAP, ThreadKind::kMain);
      StartSweepSpace(heap()->map_space());
    }
    if (v8_flags.minor_mc && heap()->new_space()) {
      GCTracer::Scope sweep_scope(
          heap()->tracer(), GCTracer::Scope::MC_SWEEP_NEW, ThreadKind::kMain);
      StartSweepNewSpace();
    }
    sweeper()->StartSweeping();
  }
}

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

  bool IsBlack(HeapObject object) override {
    return marking_state_->IsBlack(object);
  }

  void Run() override {
    VerifyRoots();
    VerifyMarking(heap_->new_space());
  }

 protected:
  void VerifyMap(Map map) override { VerifyHeapObjectImpl(map); }

  void VerifyPointers(ObjectSlot start, ObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }

  void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }
  void VerifyCodePointer(CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    // Code slots never appear in new space because CodeDataContainers, the
    // only object that can contain code pointers, are always allocated in
    // the old space.
    UNREACHABLE();
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    VerifyHeapObjectImpl(target);
  }
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    VerifyHeapObjectImpl(rinfo->target_object(cage_base()));
  }
  void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }

 private:
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object) {
    CHECK_IMPLIES(Heap::InYoungGeneration(heap_object), IsBlack(heap_object));
  }

  template <typename TSlot>
  V8_INLINE void VerifyPointersImpl(TSlot start, TSlot end) {
    PtrComprCageBase cage_base =
        GetPtrComprCageBaseFromOnHeapAddress(start.address());
    for (TSlot slot = start; slot < end; ++slot) {
      typename TSlot::TObject object = slot.load(cage_base);
      HeapObject heap_object;
      // Minor MC treats weak references as strong.
      if (object.GetHeapObject(&heap_object)) {
        VerifyHeapObjectImpl(heap_object);
      }
    }
  }

  NonAtomicMarkingState* marking_state_;
};

class YoungGenerationEvacuationVerifier : public EvacuationVerifier {
 public:
  explicit YoungGenerationEvacuationVerifier(Heap* heap)
      : EvacuationVerifier(heap) {}

  void Run() override {
    DCHECK(!heap_->mark_compact_collector()->sweeping_in_progress());
    DCHECK(!heap_->minor_mark_compact_collector()->sweeping_in_progress());
    VerifyRoots();
    VerifyEvacuation(heap_->new_space());
    VerifyEvacuation(heap_->old_space());
    VerifyEvacuation(heap_->code_space());
    if (heap_->map_space()) VerifyEvacuation(heap_->map_space());
  }

 protected:
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object) {
    CHECK_IMPLIES(Heap::InYoungGeneration(heap_object),
                  Heap::InToPage(heap_object));
  }

  template <typename TSlot>
  void VerifyPointersImpl(TSlot start, TSlot end) {
    for (TSlot current = start; current < end; ++current) {
      typename TSlot::TObject object = current.load(cage_base());
      HeapObject heap_object;
      if (object.GetHeapObject(&heap_object)) {
        VerifyHeapObjectImpl(heap_object);
      }
    }
  }
  void VerifyMap(Map map) override { VerifyHeapObjectImpl(map); }
  void VerifyPointers(ObjectSlot start, ObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }
  void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) override {
    VerifyPointersImpl(start, end);
  }
  void VerifyCodePointer(CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    Object maybe_code = slot.load(code_cage_base());
    HeapObject code;
    // The slot might contain smi during CodeDataContainer creation, so skip it.
    if (maybe_code.GetHeapObject(&code)) {
      VerifyHeapObjectImpl(code);
    }
  }
  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    VerifyHeapObjectImpl(target);
  }
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    VerifyHeapObjectImpl(rinfo->target_object(cage_base()));
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
                                             ->IsBlack(HeapObject::cast(*p));
}

}  // namespace

YoungGenerationMainMarkingVisitor::YoungGenerationMainMarkingVisitor(
    Isolate* isolate, MarkingState* marking_state,
    MarkingWorklists::Local* worklists_local)
    : YoungGenerationMarkingVisitorBase<YoungGenerationMainMarkingVisitor,
                                        MarkingState>(isolate, worklists_local),
      marking_state_(marking_state) {}

bool YoungGenerationMainMarkingVisitor::ShouldVisit(HeapObject object) {
  return marking_state_->GreyToBlack(object);
}

MinorMarkCompactCollector::~MinorMarkCompactCollector() = default;

void MinorMarkCompactCollector::SetUp() {}

void MinorMarkCompactCollector::TearDown() {
  if (heap()->incremental_marking()->IsMinorMarking()) {
    local_marking_worklists()->Publish();
    heap()->main_thread_local_heap()->marking_barrier()->Publish();
    // Marking barriers of LocalHeaps will be published in their destructors.
    marking_worklists()->Clear();
  }
}

void MinorMarkCompactCollector::FinishConcurrentMarking() {
  if (v8_flags.concurrent_marking) {
    DCHECK_EQ(heap()->concurrent_marking()->garbage_collector(),
              GarbageCollector::MINOR_MARK_COMPACTOR);
    heap()->concurrent_marking()->Cancel();
    heap()->concurrent_marking()->FlushMemoryChunkData(
        non_atomic_marking_state());
  }
}

// static
constexpr size_t MinorMarkCompactCollector::kMaxParallelTasks;

MinorMarkCompactCollector::MinorMarkCompactCollector(Heap* heap)
    : CollectorBase(heap, GarbageCollector::MINOR_MARK_COMPACTOR),
      page_parallel_job_semaphore_(0),
      sweeper_(std::make_unique<Sweeper>(heap_, non_atomic_marking_state())) {}

std::pair<size_t, size_t> MinorMarkCompactCollector::ProcessMarkingWorklist(
    size_t bytes_to_process) {
  // TODO(v8:13012): Implement this later. It should be similar to
  // MinorMarkCompactCollector::DrainMarkingWorklist.
  UNREACHABLE();
}

void MinorMarkCompactCollector::CleanupPromotedPages() {
  for (Page* p : promoted_pages_) {
    p->ClearFlag(Page::PAGE_NEW_NEW_PROMOTION);
    p->ClearFlag(Page::PAGE_NEW_OLD_PROMOTION);
    non_atomic_marking_state()->ClearLiveness(p);
  }
  promoted_pages_.clear();

  for (LargePage* p : promoted_large_pages_) {
    DCHECK(p->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION));
    p->ClearFlag(Page::PAGE_NEW_OLD_PROMOTION);
    HeapObject object = p->GetObject();
    Marking::MarkWhite(non_atomic_marking_state()->MarkBitFrom(object));
    p->ProgressBar().ResetIfEnabled();
    non_atomic_marking_state()->SetLiveBytes(p, 0);
  }
  promoted_large_pages_.clear();
}

void MinorMarkCompactCollector::VisitObject(HeapObject obj) {
  main_marking_visitor_->Visit(obj.map(), obj);
}

void MinorMarkCompactCollector::SweepArrayBufferExtensions() {
  TRACE_GC(heap()->tracer(),
           GCTracer::Scope::MINOR_MC_FINISH_SWEEP_ARRAY_BUFFERS);
  heap_->array_buffer_sweeper()->RequestSweep(
      ArrayBufferSweeper::SweepingType::kYoung);
}

class YoungGenerationMigrationObserver final : public MigrationObserver {
 public:
  YoungGenerationMigrationObserver(Heap* heap,
                                   MarkCompactCollector* mark_compact_collector)
      : MigrationObserver(heap),
        mark_compact_collector_(mark_compact_collector) {}

  inline void Move(AllocationSpace dest, HeapObject src, HeapObject dst,
                   int size) final {
    // Migrate color to old generation marking in case the object survived
    // young generation garbage collection.
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

  void MarkArrayBufferExtensionPromoted(HeapObject object) final {
    if (!object.IsJSArrayBuffer()) return;
    JSArrayBuffer::cast(object).YoungMarkExtensionPromoted();
  }

 private:
  // Only record slots for host objects that are considered as live by the
  // full collector.
  inline bool IsLive(HeapObject object) {
    return collector_->non_atomic_marking_state()->IsBlack(object);
  }

  inline void RecordMigratedSlot(HeapObject host, MaybeObject value,
                                 Address slot) final {
    if (value->IsStrongOrWeak()) {
      BasicMemoryChunk* p = BasicMemoryChunk::FromAddress(value.ptr());
      if (p->InYoungGeneration()) {
        DCHECK_IMPLIES(p->IsToPage(),
                       v8_flags.minor_mc ||
                           p->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION) ||
                           p->IsLargePage());
        MemoryChunk* chunk = MemoryChunk::FromHeapObject(host);
        DCHECK(chunk->SweepingDone());
        RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(chunk, slot);
      } else if (p->IsEvacuationCandidate() && IsLive(host)) {
        if (V8_EXTERNAL_CODE_SPACE_BOOL &&
            p->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
          RememberedSet<OLD_TO_CODE>::Insert<AccessMode::NON_ATOMIC>(
              MemoryChunk::FromHeapObject(host), slot);
        } else {
          RememberedSet<OLD_TO_OLD>::Insert<AccessMode::NON_ATOMIC>(
              MemoryChunk::FromHeapObject(host), slot);
        }
      }
    }
  }
};

void MinorMarkCompactCollector::UpdatePointersAfterEvacuation() {
  TRACE_GC(heap()->tracer(),
           GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS);

  std::vector<std::unique_ptr<UpdatingItem>> updating_items;

  // Create batches of global handles.
  CollectRememberedSetUpdatingItems(this, &updating_items, heap()->old_space(),
                                    RememberedSetUpdatingMode::OLD_TO_NEW_ONLY);
  CollectRememberedSetUpdatingItems(this, &updating_items, heap()->code_space(),
                                    RememberedSetUpdatingMode::OLD_TO_NEW_ONLY);
  if (heap()->map_space()) {
    CollectRememberedSetUpdatingItems(
        this, &updating_items, heap()->map_space(),
        RememberedSetUpdatingMode::OLD_TO_NEW_ONLY);
  }
  CollectRememberedSetUpdatingItems(this, &updating_items, heap()->lo_space(),
                                    RememberedSetUpdatingMode::OLD_TO_NEW_ONLY);
  CollectRememberedSetUpdatingItems(this, &updating_items,
                                    heap()->code_lo_space(),
                                    RememberedSetUpdatingMode::OLD_TO_NEW_ONLY);

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_SLOTS);
    V8::GetCurrentPlatform()
        ->CreateJob(
            v8::TaskPriority::kUserBlocking,
            std::make_unique<PointersUpdatingJob>(
                isolate(), std::move(updating_items),
                GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_PARALLEL,
                GCTracer::Scope::MINOR_MC_BACKGROUND_EVACUATE_UPDATE_POINTERS))
        ->Join();
  }

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_WEAK);

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
      DCHECK(!MapWord::IsPacked((*p).ptr()));
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

void MinorMarkCompactCollector::Prepare() {
  // Probably requires more.
  if (!heap()->incremental_marking()->IsMarking()) {
    StartMarking();
  }

  heap()->new_space()->FreeLinearAllocationArea();
}

void MinorMarkCompactCollector::StartMarking() {
  local_marking_worklists_ =
      std::make_unique<MarkingWorklists::Local>(&marking_worklists_);
  main_marking_visitor_ = std::make_unique<YoungGenerationMainMarkingVisitor>(
      heap()->isolate(), marking_state(), local_marking_worklists());

#ifdef VERIFY_HEAP
  for (Page* page : *heap()->new_space()) {
    CHECK(page->marking_bitmap<AccessMode::NON_ATOMIC>()->IsClean());
  }
#endif  // VERIFY_HEAP
}

void MinorMarkCompactCollector::Finish() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_FINISH);

  {
    // Keep new space sweeping atomic.
    GCTracer::Scope sweep_scope(heap()->tracer(),
                                GCTracer::Scope::MC_FINISH_SWEEP_NEW,
                                ThreadKind::kMain);
    sweeper_->EnsureCompleted(Sweeper::SweepingMode::kEagerDuringGC);
    heap()->paged_new_space()->paged_space()->RefillFreeList(sweeper());
  }

  local_marking_worklists_.reset();
  main_marking_visitor_.reset();
}

void MinorMarkCompactCollector::CollectGarbage() {
  DCHECK(!heap()->mark_compact_collector()->in_use());
  DCHECK_NOT_NULL(heap()->new_space());
  // Minor MC does not support processing the ephemeron remembered set.
  DCHECK(heap()->ephemeron_remembered_set_.empty());

  heap()->array_buffer_sweeper()->EnsureFinished();

  MarkLiveObjects();
  ClearNonLiveReferences();
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) {
    YoungGenerationMarkingVerifier verifier(heap());
    verifier.Run();
  }
#endif  // VERIFY_HEAP

  Sweep();
  Evacuate();
  Finish();

#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) {
    YoungGenerationEvacuationVerifier verifier(heap());
    verifier.Run();
  }
#endif  // VERIFY_HEAP

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARKING_DEQUE);
    heap()->incremental_marking()->UpdateMarkingWorklistAfterYoungGenGC();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_RESET_LIVENESS);
    // Since we promote all surviving large objects immediately, all remaining
    // large objects must be dead.
    NonAtomicMarkingState* marking_state = non_atomic_marking_state();
    heap()->new_lo_space()->FreeDeadObjects([marking_state](HeapObject obj) {
      // New large object space is not swept and markbits for non-promoted
      // objects are still in tact.
      USE(marking_state);
      DCHECK(marking_state->IsWhite(obj));
      return true;
    });
  }

  CleanupPromotedPages();

  SweepArrayBufferExtensions();

  heap()->isolate()->global_handles()->UpdateListOfYoungNodes();
}

void MinorMarkCompactCollector::MakeIterable(
    Page* p, FreeSpaceTreatmentMode free_space_mode) {
  CHECK(!p->IsLargePage());
  // We have to clear the full collectors markbits for the areas that we
  // remove here.
  MarkCompactCollector* full_collector = heap()->mark_compact_collector();
  Address free_start = p->area_start();

  for (auto object_and_size :
       LiveObjectRange<kBlackObjects>(p, marking_state()->bitmap(p))) {
    HeapObject const object = object_and_size.first;
    DCHECK(non_atomic_marking_state()->IsBlack(object));
    Address free_end = object.address();
    if (free_end != free_start) {
      CHECK_GT(free_end, free_start);
      size_t size = static_cast<size_t>(free_end - free_start);
      full_collector->non_atomic_marking_state()->bitmap(p)->ClearRange(
          p->AddressToMarkbitIndex(free_start),
          p->AddressToMarkbitIndex(free_end));
      if (free_space_mode == FreeSpaceTreatmentMode::kZapFreeSpace) {
        ZapCode(free_start, size);
      }
      p->heap()->CreateFillerObjectAt(free_start, static_cast<int>(size));
    }
    PtrComprCageBase cage_base(p->heap()->isolate());
    Map map = object.map(cage_base, kAcquireLoad);
    int size = object.SizeFromMap(map);
    free_start = free_end + size;
  }

  if (free_start != p->area_end()) {
    CHECK_GT(p->area_end(), free_start);
    size_t size = static_cast<size_t>(p->area_end() - free_start);
    full_collector->non_atomic_marking_state()->bitmap(p)->ClearRange(
        p->AddressToMarkbitIndex(free_start),
        p->AddressToMarkbitIndex(p->area_end()));
    if (free_space_mode == FreeSpaceTreatmentMode::kZapFreeSpace) {
      ZapCode(free_start, size);
    }
    p->heap()->CreateFillerObjectAt(free_start, static_cast<int>(size));
  }
}

namespace {

// Helper class for pruning the string table.
class YoungGenerationExternalStringTableCleaner : public RootVisitor {
 public:
  explicit YoungGenerationExternalStringTableCleaner(
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
  NonAtomicMarkingState* marking_state_;
};

}  // namespace

void MinorMarkCompactCollector::ClearNonLiveReferences() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_CLEAR);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_CLEAR_STRING_TABLE);
    // Internalized strings are always stored in old space, so there is no
    // need to clean them here.
    YoungGenerationExternalStringTableCleaner external_visitor(this);
    heap()->external_string_table_.IterateYoung(&external_visitor);
    heap()->external_string_table_.CleanUpYoung();
  }
}

void MinorMarkCompactCollector::EvacuatePrologue() {
  NewSpace* new_space = heap()->new_space();
  // Append the list of new space pages to be processed.
  DCHECK_NOT_NULL(new_space);
  for (Page* p : *new_space) {
    if (non_atomic_marking_state()->live_bytes(p) > 0) {
      new_space_evacuation_pages_.push_back(p);
    }
  }

  heap()->new_lo_space()->Flip();
  heap()->new_lo_space()->ResetPendingObject();
}

void MinorMarkCompactCollector::EvacuateEpilogue() {
  heap()->new_space()->EvacuateEpilogue();
}

std::unique_ptr<UpdatingItem>
MinorMarkCompactCollector::CreateRememberedSetUpdatingItem(
    MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) {
  return std::make_unique<RememberedSetUpdatingItem<
      NonAtomicMarkingState, GarbageCollector::MINOR_MARK_COMPACTOR>>(
      heap(), non_atomic_marking_state(), chunk, updating_mode);
}

class PageMarkingItem;
class RootMarkingItem;
class YoungGenerationMarkingTask;

class YoungGenerationMarkingTask {
 public:
  YoungGenerationMarkingTask(Isolate* isolate,
                             MinorMarkCompactCollector* collector,
                             MarkingWorklists* global_worklists)
      : marking_worklists_local_(
            std::make_unique<MarkingWorklists::Local>(global_worklists)),
        marking_state_(collector->marking_state()),
        visitor_(isolate, marking_state_, marking_worklists_local()) {}

  void MarkObject(Object object) {
    if (!Heap::InYoungGeneration(object)) return;
    HeapObject heap_object = HeapObject::cast(object);
    if (marking_state_->WhiteToGrey(heap_object)) {
      visitor_.Visit(heap_object);
      // Objects transition to black when visited.
      DCHECK(marking_state_->IsBlack(heap_object));
    }
  }

  void EmptyMarkingWorklist() {
    HeapObject object;
    while (marking_worklists_local_->Pop(&object) ||
           marking_worklists_local_->PopOnHold(&object)) {
      visitor_.Visit(object);
    }
  }

  MarkingWorklists::Local* marking_worklists_local() {
    return marking_worklists_local_.get();
  }

 private:
  std::unique_ptr<MarkingWorklists::Local> marking_worklists_local_;
  MarkingState* marking_state_;
  YoungGenerationMainMarkingVisitor visitor_;
};

class PageMarkingItem : public ParallelWorkItem {
 public:
  explicit PageMarkingItem(MemoryChunk* chunk) : chunk_(chunk) {}
  ~PageMarkingItem() = default;

  void Process(YoungGenerationMarkingTask* task) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                 "PageMarkingItem::Process");
    base::MutexGuard guard(chunk_->mutex());
    MarkUntypedPointers(task);
    MarkTypedPointers(task);
  }

 private:
  inline Heap* heap() { return chunk_->heap(); }

  void MarkUntypedPointers(YoungGenerationMarkingTask* task) {
    InvalidatedSlotsFilter filter = InvalidatedSlotsFilter::OldToNew(
        chunk_, InvalidatedSlotsFilter::LivenessCheck::kNo);
    RememberedSet<OLD_TO_NEW>::Iterate(
        chunk_,
        [this, task, &filter](MaybeObjectSlot slot) {
          if (!filter.IsValid(slot.address())) return REMOVE_SLOT;
          return CheckAndMarkObject(task, slot);
        },
        SlotSet::FREE_EMPTY_BUCKETS);
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
      return KEEP_SLOT;
    }
    return REMOVE_SLOT;
  }

  MemoryChunk* chunk_;
};

class YoungGenerationMarkingJob : public v8::JobTask {
 public:
  YoungGenerationMarkingJob(Isolate* isolate,
                            MinorMarkCompactCollector* collector,
                            MarkingWorklists* global_worklists,
                            std::vector<PageMarkingItem> marking_items)
      : isolate_(isolate),
        collector_(collector),
        global_worklists_(global_worklists),
        marking_items_(std::move(marking_items)),
        remaining_marking_items_(marking_items_.size()),
        generator_(marking_items_.size()) {}

  void Run(JobDelegate* delegate) override {
    if (delegate->IsJoiningThread()) {
      TRACE_GC(collector_->heap()->tracer(),
               GCTracer::Scope::MINOR_MC_MARK_PARALLEL);
      ProcessItems(delegate);
    } else {
      TRACE_GC_EPOCH(collector_->heap()->tracer(),
                     GCTracer::Scope::MINOR_MC_BACKGROUND_MARKING,
                     ThreadKind::kBackground);
      ProcessItems(delegate);
    }
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    // Pages are not private to markers but we can still use them to estimate
    // the amount of marking that is required.
    const int kPagesPerTask = 2;
    size_t items = remaining_marking_items_.load(std::memory_order_relaxed);
    size_t num_tasks = std::max(
        (items + 1) / kPagesPerTask,
        global_worklists_->shared()->Size() +
            global_worklists_->on_hold()
                ->Size());  // TODO(v8:13012): If this is used with concurrent
                            // marking, we need to remove on_hold() here.
    if (!v8_flags.parallel_marking) {
      num_tasks = std::min<size_t>(1, num_tasks);
    }
    return std::min<size_t>(num_tasks,
                            MinorMarkCompactCollector::kMaxParallelTasks);
  }

 private:
  void ProcessItems(JobDelegate* delegate) {
    double marking_time = 0.0;
    {
      TimedScope scope(&marking_time);
      YoungGenerationMarkingTask task(isolate_, collector_, global_worklists_);
      ProcessMarkingItems(&task);
      task.EmptyMarkingWorklist();
    }
    if (v8_flags.trace_minor_mc_parallel_marking) {
      PrintIsolate(collector_->isolate(), "marking[%p]: time=%f\n",
                   static_cast<void*>(this), marking_time);
    }
  }

  void ProcessMarkingItems(YoungGenerationMarkingTask* task) {
    while (remaining_marking_items_.load(std::memory_order_relaxed) > 0) {
      base::Optional<size_t> index = generator_.GetNext();
      if (!index) return;
      for (size_t i = *index; i < marking_items_.size(); ++i) {
        auto& work_item = marking_items_[i];
        if (!work_item.TryAcquire()) break;
        work_item.Process(task);
        task->EmptyMarkingWorklist();
        if (remaining_marking_items_.fetch_sub(1, std::memory_order_relaxed) <=
            1) {
          return;
        }
      }
    }
  }

  Isolate* isolate_;
  MinorMarkCompactCollector* collector_;
  MarkingWorklists* global_worklists_;
  std::vector<PageMarkingItem> marking_items_;
  std::atomic_size_t remaining_marking_items_{0};
  IndexGenerator generator_;
};

void MinorMarkCompactCollector::MarkRootSetInParallel(
    RootMarkingVisitor* root_visitor) {
  {
    std::vector<PageMarkingItem> marking_items;

    // Seed the root set (roots + old->new set).
    {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_SEED);
      isolate()->global_handles()->ComputeWeaknessForYoungObjects(
          &JSObject::IsUnmodifiedApiObject);
      // MinorMC treats all weak roots except for global handles as strong.
      // That is why we don't set skip_weak = true here and instead visit
      // global handles separately.
      heap()->IterateRoots(
          root_visitor, base::EnumSet<SkipRoot>{SkipRoot::kExternalStringTable,
                                                SkipRoot::kGlobalHandles,
                                                SkipRoot::kOldGeneration});
      isolate()->global_handles()->IterateYoungStrongAndDependentRoots(
          root_visitor);
      // Create items for each page.
      RememberedSet<OLD_TO_NEW>::IterateMemoryChunks(
          heap(), [&marking_items](MemoryChunk* chunk) {
            marking_items.emplace_back(chunk);
          });
    }

    // Add tasks and run in parallel.
    {
      // The main thread might hold local items, while GlobalPoolSize() ==
      // 0. Flush to ensure these items are visible globally and picked up
      // by the job.
      local_marking_worklists_->Publish();
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_ROOTS);
      V8::GetCurrentPlatform()
          ->CreateJob(v8::TaskPriority::kUserBlocking,
                      std::make_unique<YoungGenerationMarkingJob>(
                          isolate(), this, marking_worklists(),
                          std::move(marking_items)))
          ->Join();

      DCHECK(local_marking_worklists_->IsEmpty());
    }
  }
}

void MinorMarkCompactCollector::MarkLiveObjects() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK);

  DCHECK_NOT_NULL(local_marking_worklists_);
  DCHECK_NOT_NULL(main_marking_visitor_);

  PostponeInterruptsScope postpone(isolate());

  bool was_marked_incrementally = false;
  {
    // TODO(v8:13012): TRACE_GC with MINOR_MC_MARK_FINISH_INCREMENTAL.
    if (heap_->incremental_marking()->Stop()) {
      MarkingBarrier::PublishAll(heap());
      // TODO(v8:13012): TRACE_GC with MINOR_MC_MARK_FULL_CLOSURE_PARALLEL_JOIN.
      // TODO(v8:13012): Instead of finishing concurrent marking here, we could
      // continue running it to replace parallel marking.
      FinishConcurrentMarking();
      was_marked_incrementally = true;
    }
  }

  RootMarkingVisitor root_visitor(this);

  MarkRootSetInParallel(&root_visitor);

  // Mark rest on the main thread.
  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_WEAK);
    DrainMarkingWorklist();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_GLOBAL_HANDLES);
    isolate()->global_handles()->ProcessWeakYoungObjects(
        &root_visitor, &IsUnmarkedObjectForYoungGeneration);
    DrainMarkingWorklist();
  }

  if (v8_flags.minor_mc_trace_fragmentation) {
    TraceFragmentation();
  }

  if (was_marked_incrementally) {
    MarkingBarrier::DeactivateAll(heap());
  }
}

void MinorMarkCompactCollector::DrainMarkingWorklist() {
  PtrComprCageBase cage_base(isolate());
  HeapObject object;
  while (local_marking_worklists_->Pop(&object)) {
    DCHECK(!object.IsFreeSpaceOrFiller(cage_base));
    DCHECK(object.IsHeapObject());
    DCHECK(heap()->Contains(object));
    DCHECK(non_atomic_marking_state()->IsBlack(object));
    main_marking_visitor_->Visit(object);
  }
  DCHECK(local_marking_worklists_->IsEmpty());
}

void MinorMarkCompactCollector::TraceFragmentation() {
  NewSpace* new_space = heap()->new_space();
  PtrComprCageBase cage_base(isolate());
  const std::array<size_t, 4> free_size_class_limits = {0, 1024, 2048, 4096};
  size_t free_bytes_of_class[free_size_class_limits.size()] = {0};
  size_t live_bytes = 0;
  size_t allocatable_bytes = 0;
  for (Page* p :
       PageRange(new_space->first_allocatable_address(), new_space->top())) {
    Address free_start = p->area_start();
    for (auto object_and_size : LiveObjectRange<kBlackObjects>(
             p, non_atomic_marking_state()->bitmap(p))) {
      HeapObject const object = object_and_size.first;
      Address free_end = object.address();
      if (free_end != free_start) {
        size_t free_bytes = free_end - free_start;
        int free_bytes_index = 0;
        for (auto free_size_class_limit : free_size_class_limits) {
          if (free_bytes >= free_size_class_limit) {
            free_bytes_of_class[free_bytes_index] += free_bytes;
          }
          free_bytes_index++;
        }
      }
      Map map = object.map(cage_base, kAcquireLoad);
      int size = object.SizeFromMap(map);
      live_bytes += size;
      free_start = free_end + size;
    }
    size_t area_end =
        p->Contains(new_space->top()) ? new_space->top() : p->area_end();
    if (free_start != area_end) {
      size_t free_bytes = area_end - free_start;
      int free_bytes_index = 0;
      for (auto free_size_class_limit : free_size_class_limits) {
        if (free_bytes >= free_size_class_limit) {
          free_bytes_of_class[free_bytes_index] += free_bytes;
        }
        free_bytes_index++;
      }
    }
    allocatable_bytes += area_end - p->area_start();
    CHECK_EQ(allocatable_bytes, live_bytes + free_bytes_of_class[0]);
  }
  PrintIsolate(isolate(),
               "Minor Mark-Compact Fragmentation: allocatable_bytes=%zu "
               "live_bytes=%zu "
               "free_bytes=%zu free_bytes_1K=%zu free_bytes_2K=%zu "
               "free_bytes_4K=%zu\n",
               allocatable_bytes, live_bytes, free_bytes_of_class[0],
               free_bytes_of_class[1], free_bytes_of_class[2],
               free_bytes_of_class[3]);
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
    if (!heap()->new_space()->EnsureCurrentCapacity()) {
      heap()->FatalProcessOutOfMemory("NewSpace::Rebalance");
    }
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_EVACUATE_CLEAN_UP);
    for (Page* p : new_space_evacuation_pages_) {
      DCHECK(!p->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION));
      if (p->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION)) {
        promoted_pages_.push_back(p);
      } else {
        // Page was not promoted. Sweep it instead.
        DCHECK_EQ(NEW_SPACE, p->owner_identity());
        sweeper()->AddPage(NEW_SPACE, p, Sweeper::REGULAR);
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
      : Evacuator(collector->heap(), &record_visitor_, &local_allocator_,
                  AlwaysPromoteYoung::kNo),
        record_visitor_(collector->heap()->mark_compact_collector()),
        local_allocator_(
            heap_, CompactionSpaceKind::kCompactionSpaceForMinorMarkCompact),
        collector_(collector) {}

  GCTracer::Scope::ScopeId GetBackgroundTracingScope() override {
    return GCTracer::Scope::MINOR_MC_BACKGROUND_EVACUATE_COPY;
  }

  GCTracer::Scope::ScopeId GetTracingScope() override {
    return GCTracer::Scope::MINOR_MC_EVACUATE_COPY_PARALLEL;
  }

 protected:
  void RawEvacuatePage(MemoryChunk* chunk, intptr_t* live_bytes) override;

  YoungGenerationRecordMigratedSlotVisitor record_visitor_;
  EvacuationAllocator local_allocator_;
  MinorMarkCompactCollector* collector_;
};

void YoungGenerationEvacuator::RawEvacuatePage(MemoryChunk* chunk,
                                               intptr_t* live_bytes) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
               "YoungGenerationEvacuator::RawEvacuatePage");
  NonAtomicMarkingState* marking_state = collector_->non_atomic_marking_state();
  *live_bytes = marking_state->live_bytes(chunk);
  DCHECK_EQ(kPageNewToOld, ComputeEvacuationMode(chunk));
  LiveObjectVisitor::VisitBlackObjectsNoFail(chunk, marking_state,
                                             &new_to_old_page_visitor_);
  new_to_old_page_visitor_.account_moved_bytes(
      marking_state->live_bytes(chunk));
  if (!chunk->IsLargePage()) {
    if (heap()->ShouldZapGarbage()) {
      collector_->MakeIterable(static_cast<Page*>(chunk),
                               FreeSpaceTreatmentMode::kZapFreeSpace);
    } else if (heap()->incremental_marking()->IsMarking()) {
      // When incremental marking is on, we need to clear the mark bits
      // of the full collector. We cannot yet discard the young
      // generation mark bits as they are still relevant for pointers
      // updating.
      collector_->MakeIterable(static_cast<Page*>(chunk),
                               FreeSpaceTreatmentMode::kIgnoreFreeSpace);
    }
  }
}

}  // namespace

void MinorMarkCompactCollector::EvacuatePagesInParallel() {
  std::vector<std::pair<ParallelWorkItem, MemoryChunk*>> evacuation_items;
  intptr_t live_bytes = 0;

  for (Page* page : new_space_evacuation_pages_) {
    intptr_t live_bytes_on_page = non_atomic_marking_state()->live_bytes(page);
    DCHECK_LT(0, live_bytes_on_page);
    live_bytes += live_bytes_on_page;
    if (ShouldMovePage(page, live_bytes_on_page, page->wasted_memory(),
                       MemoryReductionMode::kNone, AlwaysPromoteYoung::kNo)) {
      EvacuateNewSpacePageVisitor<NEW_TO_OLD>::Move(page);
      evacuation_items.emplace_back(ParallelWorkItem{}, page);
    }
  }

  // Promote young generation large objects.
  for (auto it = heap()->new_lo_space()->begin();
       it != heap()->new_lo_space()->end();) {
    LargePage* current = *it;
    it++;
    HeapObject object = current->GetObject();
    if (non_atomic_marking_state_.IsBlack(object)) {
      heap_->lo_space()->PromoteNewLargeObject(current);
      current->SetFlag(Page::PAGE_NEW_OLD_PROMOTION);
      promoted_large_pages_.push_back(current);
      evacuation_items.emplace_back(ParallelWorkItem{}, current);
    }
    heap()->new_lo_space()->set_objects_size(0);
  }
  if (evacuation_items.empty()) return;

  YoungGenerationMigrationObserver observer(heap(),
                                            heap()->mark_compact_collector());
  const auto pages_count = evacuation_items.size();
  const auto wanted_num_tasks =
      CreateAndExecuteEvacuationTasks<YoungGenerationEvacuator>(
          this, std::move(evacuation_items), &observer);

  if (v8_flags.trace_evacuation) {
    TraceEvacuation(isolate(), pages_count, wanted_num_tasks, live_bytes, 0);
  }
}

void MinorMarkCompactCollector::StartSweepNewSpace() {
  PagedSpaceBase* paged_space = heap()->paged_new_space()->paged_space();
  paged_space->ClearAllocatorState();

  int will_be_swept = 0;

  // Loop needs to support deletion if live bytes == 0 for a page.
  for (auto it = paged_space->begin(); it != paged_space->end();) {
    Page* p = *(it++);
    DCHECK(p->SweepingDone());

    if (non_atomic_marking_state()->live_bytes(p) > 0) {
      // Non-empty pages will be evacuated/promoted.
      continue;
    }

    // New space preallocates all its pages. Don't free empty pages since they
    // will just be reallocated.
    DCHECK_EQ(NEW_SPACE, paged_space->identity());
    sweeper_->AddPage(NEW_SPACE, p, Sweeper::REGULAR);
    will_be_swept++;
  }

  if (v8_flags.gc_verbose) {
    PrintIsolate(isolate(), "sweeping: space=%s initialized_for_sweeping=%d",
                 paged_space->name(), will_be_swept);
  }
}

void MinorMarkCompactCollector::Sweep() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_SWEEP);
  {
    GCTracer::Scope sweep_scope(heap()->tracer(),
                                GCTracer::Scope::MINOR_MC_SWEEP_NEW,
                                ThreadKind::kMain);
    StartSweepNewSpace();
  }
  sweeper_->StartSweeping();
}

}  // namespace internal
}  // namespace v8

// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "code-stubs.h"
#include "compilation-cache.h"
#include "cpu-profiler.h"
#include "deoptimizer.h"
#include "execution.h"
#include "gdb-jit.h"
#include "global-handles.h"
#include "heap-profiler.h"
#include "ic-inl.h"
#include "incremental-marking.h"
#include "mark-compact.h"
#include "marking-thread.h"
#include "objects-visiting.h"
#include "objects-visiting-inl.h"
#include "stub-cache.h"
#include "sweeper-thread.h"

namespace v8 {
namespace internal {


const char* Marking::kWhiteBitPattern = "00";
const char* Marking::kBlackBitPattern = "10";
const char* Marking::kGreyBitPattern = "11";
const char* Marking::kImpossibleBitPattern = "01";


// -------------------------------------------------------------------------
// MarkCompactCollector

MarkCompactCollector::MarkCompactCollector() :  // NOLINT
#ifdef DEBUG
      state_(IDLE),
#endif
      sweep_precisely_(false),
      reduce_memory_footprint_(false),
      abort_incremental_marking_(false),
      marking_parity_(ODD_MARKING_PARITY),
      compacting_(false),
      was_marked_incrementally_(false),
      sweeping_pending_(false),
      sequential_sweeping_(false),
      tracer_(NULL),
      migration_slots_buffer_(NULL),
      heap_(NULL),
      code_flusher_(NULL),
      encountered_weak_collections_(NULL),
      have_code_to_deoptimize_(false) { }

#ifdef VERIFY_HEAP
class VerifyMarkingVisitor: public ObjectVisitor {
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
    ASSERT(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
    if (!FLAG_weak_embedded_maps_in_optimized_code || !FLAG_collect_maps ||
        rinfo->host()->kind() != Code::OPTIMIZED_FUNCTION ||
        !rinfo->target_object()->IsMap() ||
        !Map::cast(rinfo->target_object())->CanTransition()) {
      VisitPointer(rinfo->target_object_address());
    }
  }

 private:
  Heap* heap_;
};


static void VerifyMarking(Heap* heap, Address bottom, Address top) {
  VerifyMarkingVisitor visitor(heap);
  HeapObject* object;
  Address next_object_must_be_here_or_later = bottom;

  for (Address current = bottom;
       current < top;
       current += kPointerSize) {
    object = HeapObject::FromAddress(current);
    if (MarkCompactCollector::IsMarked(object)) {
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
  VerifyMarking(heap->old_pointer_space());
  VerifyMarking(heap->old_data_space());
  VerifyMarking(heap->code_space());
  VerifyMarking(heap->cell_space());
  VerifyMarking(heap->property_cell_space());
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


class VerifyEvacuationVisitor: public ObjectVisitor {
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


static void VerifyEvacuation(Address bottom, Address top) {
  VerifyEvacuationVisitor visitor;
  HeapObject* object;
  Address next_object_must_be_here_or_later = bottom;

  for (Address current = bottom;
       current < top;
       current += kPointerSize) {
    object = HeapObject::FromAddress(current);
    if (MarkCompactCollector::IsMarked(object)) {
      CHECK(current >= next_object_must_be_here_or_later);
      object->Iterate(&visitor);
      next_object_must_be_here_or_later = current + object->Size();
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


static void VerifyEvacuation(PagedSpace* space) {
  PageIterator it(space);

  while (it.has_next()) {
    Page* p = it.next();
    if (p->IsEvacuationCandidate()) continue;
    VerifyEvacuation(p->area_start(), p->area_end());
  }
}


static void VerifyEvacuation(Heap* heap) {
  VerifyEvacuation(heap->old_pointer_space());
  VerifyEvacuation(heap->old_data_space());
  VerifyEvacuation(heap->code_space());
  VerifyEvacuation(heap->cell_space());
  VerifyEvacuation(heap->property_cell_space());
  VerifyEvacuation(heap->map_space());
  VerifyEvacuation(heap->new_space());

  VerifyEvacuationVisitor visitor;
  heap->IterateStrongRoots(&visitor, VISIT_ALL);
}
#endif  // VERIFY_HEAP


#ifdef DEBUG
class VerifyNativeContextSeparationVisitor: public ObjectVisitor {
 public:
  VerifyNativeContextSeparationVisitor() : current_native_context_(NULL) {}

  void VisitPointers(Object** start, Object** end) {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        if (object->IsString()) continue;
        switch (object->map()->instance_type()) {
          case JS_FUNCTION_TYPE:
            CheckContext(JSFunction::cast(object)->context());
            break;
          case JS_GLOBAL_PROXY_TYPE:
            CheckContext(JSGlobalProxy::cast(object)->native_context());
            break;
          case JS_GLOBAL_OBJECT_TYPE:
          case JS_BUILTINS_OBJECT_TYPE:
            CheckContext(GlobalObject::cast(object)->native_context());
            break;
          case JS_ARRAY_TYPE:
          case JS_DATE_TYPE:
          case JS_OBJECT_TYPE:
          case JS_REGEXP_TYPE:
            VisitPointer(HeapObject::RawField(object, JSObject::kMapOffset));
            break;
          case MAP_TYPE:
            VisitPointer(HeapObject::RawField(object, Map::kPrototypeOffset));
            VisitPointer(HeapObject::RawField(object, Map::kConstructorOffset));
            break;
          case FIXED_ARRAY_TYPE:
            if (object->IsContext()) {
              CheckContext(object);
            } else {
              FixedArray* array = FixedArray::cast(object);
              int length = array->length();
              // Set array length to zero to prevent cycles while iterating
              // over array bodies, this is easier than intrusive marking.
              array->set_length(0);
              array->IterateBody(
                  FIXED_ARRAY_TYPE, FixedArray::SizeFor(length), this);
              array->set_length(length);
            }
            break;
          case CELL_TYPE:
          case JS_PROXY_TYPE:
          case JS_VALUE_TYPE:
          case TYPE_FEEDBACK_INFO_TYPE:
            object->Iterate(this);
            break;
          case DECLARED_ACCESSOR_INFO_TYPE:
          case EXECUTABLE_ACCESSOR_INFO_TYPE:
          case BYTE_ARRAY_TYPE:
          case CALL_HANDLER_INFO_TYPE:
          case CODE_TYPE:
          case FIXED_DOUBLE_ARRAY_TYPE:
          case HEAP_NUMBER_TYPE:
          case INTERCEPTOR_INFO_TYPE:
          case ODDBALL_TYPE:
          case SCRIPT_TYPE:
          case SHARED_FUNCTION_INFO_TYPE:
            break;
          default:
            UNREACHABLE();
        }
      }
    }
  }

 private:
  void CheckContext(Object* context) {
    if (!context->IsContext()) return;
    Context* native_context = Context::cast(context)->native_context();
    if (current_native_context_ == NULL) {
      current_native_context_ = native_context;
    } else {
      CHECK_EQ(current_native_context_, native_context);
    }
  }

  Context* current_native_context_;
};


static void VerifyNativeContextSeparation(Heap* heap) {
  HeapObjectIterator it(heap->code_space());

  for (Object* object = it.Next(); object != NULL; object = it.Next()) {
    VerifyNativeContextSeparationVisitor visitor;
    Code::cast(object)->CodeIterateBody(&visitor);
  }
}
#endif


void MarkCompactCollector::TearDown() {
  AbortCompaction();
}


void MarkCompactCollector::AddEvacuationCandidate(Page* p) {
  p->MarkEvacuationCandidate();
  evacuation_candidates_.Add(p);
}


static void TraceFragmentation(PagedSpace* space) {
  int number_of_pages = space->CountTotalPages();
  intptr_t reserved = (number_of_pages * space->AreaSize());
  intptr_t free = reserved - space->SizeOfObjects();
  PrintF("[%s]: %d pages, %d (%.1f%%) free\n",
         AllocationSpaceName(space->identity()),
         number_of_pages,
         static_cast<int>(free),
         static_cast<double>(free) * 100 / reserved);
}


bool MarkCompactCollector::StartCompaction(CompactionMode mode) {
  if (!compacting_) {
    ASSERT(evacuation_candidates_.length() == 0);

#ifdef ENABLE_GDB_JIT_INTERFACE
    // If GDBJIT interface is active disable compaction.
    if (FLAG_gdbjit) return false;
#endif

    CollectEvacuationCandidates(heap()->old_pointer_space());
    CollectEvacuationCandidates(heap()->old_data_space());

    if (FLAG_compact_code_space &&
        (mode == NON_INCREMENTAL_COMPACTION ||
         FLAG_incremental_code_compaction)) {
      CollectEvacuationCandidates(heap()->code_space());
    } else if (FLAG_trace_fragmentation) {
      TraceFragmentation(heap()->code_space());
    }

    if (FLAG_trace_fragmentation) {
      TraceFragmentation(heap()->map_space());
      TraceFragmentation(heap()->cell_space());
      TraceFragmentation(heap()->property_cell_space());
    }

    heap()->old_pointer_space()->EvictEvacuationCandidatesFromFreeLists();
    heap()->old_data_space()->EvictEvacuationCandidatesFromFreeLists();
    heap()->code_space()->EvictEvacuationCandidatesFromFreeLists();

    compacting_ = evacuation_candidates_.length() > 0;
  }

  return compacting_;
}


void MarkCompactCollector::CollectGarbage() {
  // Make sure that Prepare() has been called. The individual steps below will
  // update the state as they proceed.
  ASSERT(state_ == PREPARE_GC);
  ASSERT(encountered_weak_collections_ == Smi::FromInt(0));

  MarkLiveObjects();
  ASSERT(heap_->incremental_marking()->IsStopped());

  if (FLAG_collect_maps) ClearNonLiveReferences();

  ClearWeakCollections();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    VerifyMarking(heap_);
  }
#endif

  SweepSpaces();

  if (!FLAG_collect_maps) ReattachInitialMaps();

#ifdef DEBUG
  if (FLAG_verify_native_context_separation) {
    VerifyNativeContextSeparation(heap_);
  }
#endif

#ifdef VERIFY_HEAP
  if (FLAG_collect_maps && FLAG_weak_embedded_maps_in_optimized_code &&
      heap()->weak_embedded_maps_verification_enabled()) {
    VerifyWeakEmbeddedMapsInOptimizedCode();
  }
  if (FLAG_collect_maps && FLAG_omit_map_checks_for_leaf_maps) {
    VerifyOmittedMapChecks();
  }
#endif

  Finish();

  if (marking_parity_ == EVEN_MARKING_PARITY) {
    marking_parity_ = ODD_MARKING_PARITY;
  } else {
    ASSERT(marking_parity_ == ODD_MARKING_PARITY);
    marking_parity_ = EVEN_MARKING_PARITY;
  }

  tracer_ = NULL;
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
  VerifyMarkbitsAreClean(heap_->old_pointer_space());
  VerifyMarkbitsAreClean(heap_->old_data_space());
  VerifyMarkbitsAreClean(heap_->code_space());
  VerifyMarkbitsAreClean(heap_->cell_space());
  VerifyMarkbitsAreClean(heap_->property_cell_space());
  VerifyMarkbitsAreClean(heap_->map_space());
  VerifyMarkbitsAreClean(heap_->new_space());

  LargeObjectIterator it(heap_->lo_space());
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    MarkBit mark_bit = Marking::MarkBitFrom(obj);
    CHECK(Marking::IsWhite(mark_bit));
    CHECK_EQ(0, Page::FromAddress(obj->address())->LiveBytes());
  }
}


void MarkCompactCollector::VerifyWeakEmbeddedMapsInOptimizedCode() {
  HeapObjectIterator code_iterator(heap()->code_space());
  for (HeapObject* obj = code_iterator.Next();
       obj != NULL;
       obj = code_iterator.Next()) {
    Code* code = Code::cast(obj);
    if (code->kind() != Code::OPTIMIZED_FUNCTION) continue;
    if (WillBeDeoptimized(code)) continue;
    code->VerifyEmbeddedMapsDependency();
  }
}


void MarkCompactCollector::VerifyOmittedMapChecks() {
  HeapObjectIterator iterator(heap()->map_space());
  for (HeapObject* obj = iterator.Next();
       obj != NULL;
       obj = iterator.Next()) {
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
  ClearMarkbitsInPagedSpace(heap_->old_pointer_space());
  ClearMarkbitsInPagedSpace(heap_->old_data_space());
  ClearMarkbitsInPagedSpace(heap_->cell_space());
  ClearMarkbitsInPagedSpace(heap_->property_cell_space());
  ClearMarkbitsInNewSpace(heap_->new_space());

  LargeObjectIterator it(heap_->lo_space());
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    MarkBit mark_bit = Marking::MarkBitFrom(obj);
    mark_bit.Clear();
    mark_bit.Next().Clear();
    Page::FromAddress(obj->address())->ResetProgressBar();
    Page::FromAddress(obj->address())->ResetLiveBytes();
  }
}


void MarkCompactCollector::StartSweeperThreads() {
  sweeping_pending_ = true;
  for (int i = 0; i < FLAG_sweeper_threads; i++) {
    isolate()->sweeper_threads()[i]->StartSweeping();
  }
}


void MarkCompactCollector::WaitUntilSweepingCompleted() {
  ASSERT(sweeping_pending_ == true);
  for (int i = 0; i < FLAG_sweeper_threads; i++) {
    isolate()->sweeper_threads()[i]->WaitForSweeperThread();
  }
  sweeping_pending_ = false;
  StealMemoryFromSweeperThreads(heap()->paged_space(OLD_DATA_SPACE));
  StealMemoryFromSweeperThreads(heap()->paged_space(OLD_POINTER_SPACE));
  heap()->paged_space(OLD_DATA_SPACE)->ResetUnsweptFreeBytes();
  heap()->paged_space(OLD_POINTER_SPACE)->ResetUnsweptFreeBytes();
}


intptr_t MarkCompactCollector::
             StealMemoryFromSweeperThreads(PagedSpace* space) {
  intptr_t freed_bytes = 0;
  for (int i = 0; i < FLAG_sweeper_threads; i++) {
    freed_bytes += isolate()->sweeper_threads()[i]->StealMemory(space);
  }
  space->AddToAccountingStats(freed_bytes);
  space->DecrementUnsweptFreeBytes(freed_bytes);
  return freed_bytes;
}


bool MarkCompactCollector::AreSweeperThreadsActivated() {
  return isolate()->sweeper_threads() != NULL;
}


bool MarkCompactCollector::IsConcurrentSweepingInProgress() {
  return sweeping_pending_;
}


void MarkCompactCollector::MarkInParallel() {
  for (int i = 0; i < FLAG_marking_threads; i++) {
    isolate()->marking_threads()[i]->StartMarking();
  }
}


void MarkCompactCollector::WaitUntilMarkingCompleted() {
  for (int i = 0; i < FLAG_marking_threads; i++) {
    isolate()->marking_threads()[i]->WaitForMarkingThread();
  }
}


bool Marking::TransferMark(Address old_start, Address new_start) {
  // This is only used when resizing an object.
  ASSERT(MemoryChunk::FromAddress(old_start) ==
         MemoryChunk::FromAddress(new_start));

  // If the mark doesn't move, we don't check the color of the object.
  // It doesn't matter whether the object is black, since it hasn't changed
  // size, so the adjustment to the live data count will be zero anyway.
  if (old_start == new_start) return false;

  MarkBit new_mark_bit = MarkBitFrom(new_start);
  MarkBit old_mark_bit = MarkBitFrom(old_start);

#ifdef DEBUG
  ObjectColor old_color = Color(old_mark_bit);
#endif

  if (Marking::IsBlack(old_mark_bit)) {
    old_mark_bit.Clear();
    ASSERT(IsWhite(old_mark_bit));
    Marking::MarkBlack(new_mark_bit);
    return true;
  } else if (Marking::IsGrey(old_mark_bit)) {
    ASSERT(heap_->incremental_marking()->IsMarking());
    old_mark_bit.Clear();
    old_mark_bit.Next().Clear();
    ASSERT(IsWhite(old_mark_bit));
    heap_->incremental_marking()->WhiteToGreyAndPush(
        HeapObject::FromAddress(new_start), new_mark_bit);
    heap_->incremental_marking()->RestartIfNotMarking();
  }

#ifdef DEBUG
  ObjectColor new_color = Color(new_mark_bit);
  ASSERT(new_color == old_color);
#endif

  return false;
}


const char* AllocationSpaceName(AllocationSpace space) {
  switch (space) {
    case NEW_SPACE: return "NEW_SPACE";
    case OLD_POINTER_SPACE: return "OLD_POINTER_SPACE";
    case OLD_DATA_SPACE: return "OLD_DATA_SPACE";
    case CODE_SPACE: return "CODE_SPACE";
    case MAP_SPACE: return "MAP_SPACE";
    case CELL_SPACE: return "CELL_SPACE";
    case PROPERTY_CELL_SPACE:
      return "PROPERTY_CELL_SPACE";
    case LO_SPACE: return "LO_SPACE";
    default:
      UNREACHABLE();
  }

  return NULL;
}


// Returns zero for pages that have so little fragmentation that it is not
// worth defragmenting them.  Otherwise a positive integer that gives an
// estimate of fragmentation on an arbitrary scale.
static int FreeListFragmentation(PagedSpace* space, Page* p) {
  // If page was not swept then there are no free list items on it.
  if (!p->WasSwept()) {
    if (FLAG_trace_fragmentation) {
      PrintF("%p [%s]: %d bytes live (unswept)\n",
             reinterpret_cast<void*>(p),
             AllocationSpaceName(space->identity()),
             p->LiveBytes());
    }
    return 0;
  }

  PagedSpace::SizeStats sizes;
  space->ObtainFreeListStatistics(p, &sizes);

  intptr_t ratio;
  intptr_t ratio_threshold;
  intptr_t area_size = space->AreaSize();
  if (space->identity() == CODE_SPACE) {
    ratio = (sizes.medium_size_ * 10 + sizes.large_size_ * 2) * 100 /
        area_size;
    ratio_threshold = 10;
  } else {
    ratio = (sizes.small_size_ * 5 + sizes.medium_size_) * 100 /
        area_size;
    ratio_threshold = 15;
  }

  if (FLAG_trace_fragmentation) {
    PrintF("%p [%s]: %d (%.2f%%) %d (%.2f%%) %d (%.2f%%) %d (%.2f%%) %s\n",
           reinterpret_cast<void*>(p),
           AllocationSpaceName(space->identity()),
           static_cast<int>(sizes.small_size_),
           static_cast<double>(sizes.small_size_ * 100) /
           area_size,
           static_cast<int>(sizes.medium_size_),
           static_cast<double>(sizes.medium_size_ * 100) /
           area_size,
           static_cast<int>(sizes.large_size_),
           static_cast<double>(sizes.large_size_ * 100) /
           area_size,
           static_cast<int>(sizes.huge_size_),
           static_cast<double>(sizes.huge_size_ * 100) /
           area_size,
           (ratio > ratio_threshold) ? "[fragmented]" : "");
  }

  if (FLAG_always_compact && sizes.Total() != area_size) {
    return 1;
  }

  if (ratio <= ratio_threshold) return 0;  // Not fragmented.

  return static_cast<int>(ratio - ratio_threshold);
}


void MarkCompactCollector::CollectEvacuationCandidates(PagedSpace* space) {
  ASSERT(space->identity() == OLD_POINTER_SPACE ||
         space->identity() == OLD_DATA_SPACE ||
         space->identity() == CODE_SPACE);

  static const int kMaxMaxEvacuationCandidates = 1000;
  int number_of_pages = space->CountTotalPages();
  int max_evacuation_candidates =
      static_cast<int>(sqrt(number_of_pages / 2.0) + 1);

  if (FLAG_stress_compaction || FLAG_always_compact) {
    max_evacuation_candidates = kMaxMaxEvacuationCandidates;
  }

  class Candidate {
   public:
    Candidate() : fragmentation_(0), page_(NULL) { }
    Candidate(int f, Page* p) : fragmentation_(f), page_(p) { }

    int fragmentation() { return fragmentation_; }
    Page* page() { return page_; }

   private:
    int fragmentation_;
    Page* page_;
  };

  enum CompactionMode {
    COMPACT_FREE_LISTS,
    REDUCE_MEMORY_FOOTPRINT
  };

  CompactionMode mode = COMPACT_FREE_LISTS;

  intptr_t reserved = number_of_pages * space->AreaSize();
  intptr_t over_reserved = reserved - space->SizeOfObjects();
  static const intptr_t kFreenessThreshold = 50;

  if (reduce_memory_footprint_ && over_reserved >= space->AreaSize()) {
    // If reduction of memory footprint was requested, we are aggressive
    // about choosing pages to free.  We expect that half-empty pages
    // are easier to compact so slightly bump the limit.
    mode = REDUCE_MEMORY_FOOTPRINT;
    max_evacuation_candidates += 2;
  }


  if (over_reserved > reserved / 3 && over_reserved >= 2 * space->AreaSize()) {
    // If over-usage is very high (more than a third of the space), we
    // try to free all mostly empty pages.  We expect that almost empty
    // pages are even easier to compact so bump the limit even more.
    mode = REDUCE_MEMORY_FOOTPRINT;
    max_evacuation_candidates *= 2;
  }

  if (FLAG_trace_fragmentation && mode == REDUCE_MEMORY_FOOTPRINT) {
    PrintF("Estimated over reserved memory: %.1f / %.1f MB (threshold %d), "
           "evacuation candidate limit: %d\n",
           static_cast<double>(over_reserved) / MB,
           static_cast<double>(reserved) / MB,
           static_cast<int>(kFreenessThreshold),
           max_evacuation_candidates);
  }

  intptr_t estimated_release = 0;

  Candidate candidates[kMaxMaxEvacuationCandidates];

  max_evacuation_candidates =
      Min(kMaxMaxEvacuationCandidates, max_evacuation_candidates);

  int count = 0;
  int fragmentation = 0;
  Candidate* least = NULL;

  PageIterator it(space);
  if (it.has_next()) it.next();  // Never compact the first page.

  while (it.has_next()) {
    Page* p = it.next();
    p->ClearEvacuationCandidate();

    if (FLAG_stress_compaction) {
      unsigned int counter = space->heap()->ms_count();
      uintptr_t page_number = reinterpret_cast<uintptr_t>(p) >> kPageSizeBits;
      if ((counter & 1) == (page_number & 1)) fragmentation = 1;
    } else if (mode == REDUCE_MEMORY_FOOTPRINT) {
      // Don't try to release too many pages.
      if (estimated_release >= over_reserved) {
        continue;
      }

      intptr_t free_bytes = 0;

      if (!p->WasSwept()) {
        free_bytes = (p->area_size() - p->LiveBytes());
      } else {
        PagedSpace::SizeStats sizes;
        space->ObtainFreeListStatistics(p, &sizes);
        free_bytes = sizes.Total();
      }

      int free_pct = static_cast<int>(free_bytes * 100) / p->area_size();

      if (free_pct >= kFreenessThreshold) {
        estimated_release += free_bytes;
        fragmentation = free_pct;
      } else {
        fragmentation = 0;
      }

      if (FLAG_trace_fragmentation) {
        PrintF("%p [%s]: %d (%.2f%%) free %s\n",
               reinterpret_cast<void*>(p),
               AllocationSpaceName(space->identity()),
               static_cast<int>(free_bytes),
               static_cast<double>(free_bytes * 100) / p->area_size(),
               (fragmentation > 0) ? "[fragmented]" : "");
      }
    } else {
      fragmentation = FreeListFragmentation(space, p);
    }

    if (fragmentation != 0) {
      if (count < max_evacuation_candidates) {
        candidates[count++] = Candidate(fragmentation, p);
      } else {
        if (least == NULL) {
          for (int i = 0; i < max_evacuation_candidates; i++) {
            if (least == NULL ||
                candidates[i].fragmentation() < least->fragmentation()) {
              least = candidates + i;
            }
          }
        }
        if (least->fragmentation() < fragmentation) {
          *least = Candidate(fragmentation, p);
          least = NULL;
        }
      }
    }
  }

  for (int i = 0; i < count; i++) {
    AddEvacuationCandidate(candidates[i].page());
  }

  if (count > 0 && FLAG_trace_fragmentation) {
    PrintF("Collected %d evacuation candidates for space %s\n",
           count,
           AllocationSpaceName(space->identity()));
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
    invalidated_code_.Rewind(0);
  }
  ASSERT_EQ(0, evacuation_candidates_.length());
}


void MarkCompactCollector::Prepare(GCTracer* tracer) {
  was_marked_incrementally_ = heap()->incremental_marking()->IsMarking();

  // Rather than passing the tracer around we stash it in a static member
  // variable.
  tracer_ = tracer;

#ifdef DEBUG
  ASSERT(state_ == IDLE);
  state_ = PREPARE_GC;
#endif

  ASSERT(!FLAG_never_compact || !FLAG_always_compact);

  if (IsConcurrentSweepingInProgress()) {
    // Instead of waiting we could also abort the sweeper threads here.
    WaitUntilSweepingCompleted();
  }

  // Clear marking bits if incremental marking is aborted.
  if (was_marked_incrementally_ && abort_incremental_marking_) {
    heap()->incremental_marking()->Abort();
    ClearMarkbits();
    AbortCompaction();
    was_marked_incrementally_ = false;
  }

  // Don't start compaction if we are in the middle of incremental
  // marking cycle. We did not collect any slots.
  if (!FLAG_never_compact && !was_marked_incrementally_) {
    StartCompaction(NON_INCREMENTAL_COMPACTION);
  }

  PagedSpaces spaces(heap());
  for (PagedSpace* space = spaces.next();
       space != NULL;
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
  ASSERT(state_ == SWEEP_SPACES || state_ == RELOCATE_OBJECTS);
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
  Code* lazy_compile = isolate_->builtins()->builtin(Builtins::kLazyCompile);
  Object* undefined = isolate_->heap()->undefined_value();

  JSFunction* candidate = jsfunction_candidates_head_;
  JSFunction* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    ClearNextCandidate(candidate, undefined);

    SharedFunctionInfo* shared = candidate->shared();

    Code* code = shared->code();
    MarkBit code_mark = Marking::MarkBitFrom(code);
    if (!code_mark.Get()) {
      if (FLAG_trace_code_flushing && shared->is_compiled()) {
        PrintF("[code-flushing clears: ");
        shared->ShortPrint();
        PrintF(" - age: %d]\n", code->GetAge());
      }
      shared->set_code(lazy_compile);
      candidate->set_code(lazy_compile);
    } else {
      candidate->set_code(code);
    }

    // We are in the middle of a GC cycle so the write barrier in the code
    // setter did not record the slot update and we have to do that manually.
    Address slot = candidate->address() + JSFunction::kCodeEntryOffset;
    Code* target = Code::cast(Code::GetObjectFromEntryAddress(slot));
    isolate_->heap()->mark_compact_collector()->
        RecordCodeEntrySlot(slot, target);

    Object** shared_code_slot =
        HeapObject::RawField(shared, SharedFunctionInfo::kCodeOffset);
    isolate_->heap()->mark_compact_collector()->
        RecordSlot(shared_code_slot, shared_code_slot, *shared_code_slot);

    candidate = next_candidate;
  }

  jsfunction_candidates_head_ = NULL;
}


void CodeFlusher::ProcessSharedFunctionInfoCandidates() {
  Code* lazy_compile = isolate_->builtins()->builtin(Builtins::kLazyCompile);

  SharedFunctionInfo* candidate = shared_function_info_candidates_head_;
  SharedFunctionInfo* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    ClearNextCandidate(candidate);

    Code* code = candidate->code();
    MarkBit code_mark = Marking::MarkBitFrom(code);
    if (!code_mark.Get()) {
      if (FLAG_trace_code_flushing && candidate->is_compiled()) {
        PrintF("[code-flushing clears: ");
        candidate->ShortPrint();
        PrintF(" - age: %d]\n", code->GetAge());
      }
      candidate->set_code(lazy_compile);
    }

    Object** code_slot =
        HeapObject::RawField(candidate, SharedFunctionInfo::kCodeOffset);
    isolate_->heap()->mark_compact_collector()->
        RecordSlot(code_slot, code_slot, *code_slot);

    candidate = next_candidate;
  }

  shared_function_info_candidates_head_ = NULL;
}


void CodeFlusher::ProcessOptimizedCodeMaps() {
  static const int kEntriesStart = SharedFunctionInfo::kEntriesStart;
  static const int kEntryLength = SharedFunctionInfo::kEntryLength;
  static const int kContextOffset = 0;
  static const int kCodeOffset = 1;
  static const int kLiteralsOffset = 2;
  STATIC_ASSERT(kEntryLength == 3);

  SharedFunctionInfo* holder = optimized_code_map_holder_head_;
  SharedFunctionInfo* next_holder;
  while (holder != NULL) {
    next_holder = GetNextCodeMap(holder);
    ClearNextCodeMap(holder);

    FixedArray* code_map = FixedArray::cast(holder->optimized_code_map());
    int new_length = kEntriesStart;
    int old_length = code_map->length();
    for (int i = kEntriesStart; i < old_length; i += kEntryLength) {
      Code* code = Code::cast(code_map->get(i + kCodeOffset));
      MarkBit code_mark = Marking::MarkBitFrom(code);
      if (!code_mark.Get()) {
        continue;
      }

      // Update and record the context slot in the optimized code map.
      Object** context_slot = HeapObject::RawField(code_map,
          FixedArray::OffsetOfElementAt(new_length));
      code_map->set(new_length++, code_map->get(i + kContextOffset));
      ASSERT(Marking::IsBlack(
          Marking::MarkBitFrom(HeapObject::cast(*context_slot))));
      isolate_->heap()->mark_compact_collector()->
          RecordSlot(context_slot, context_slot, *context_slot);

      // Update and record the code slot in the optimized code map.
      Object** code_slot = HeapObject::RawField(code_map,
          FixedArray::OffsetOfElementAt(new_length));
      code_map->set(new_length++, code_map->get(i + kCodeOffset));
      ASSERT(Marking::IsBlack(
          Marking::MarkBitFrom(HeapObject::cast(*code_slot))));
      isolate_->heap()->mark_compact_collector()->
          RecordSlot(code_slot, code_slot, *code_slot);

      // Update and record the literals slot in the optimized code map.
      Object** literals_slot = HeapObject::RawField(code_map,
          FixedArray::OffsetOfElementAt(new_length));
      code_map->set(new_length++, code_map->get(i + kLiteralsOffset));
      ASSERT(Marking::IsBlack(
          Marking::MarkBitFrom(HeapObject::cast(*literals_slot))));
      isolate_->heap()->mark_compact_collector()->
          RecordSlot(literals_slot, literals_slot, *literals_slot);
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
  ASSERT(!function->next_function_link()->IsUndefined());
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
  ASSERT(!FixedArray::cast(code_map_holder->optimized_code_map())->
         get(SharedFunctionInfo::kNextMapIndex)->IsUndefined());

  // Make sure previous flushing decisions are revisited.
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
  ASSERT(jsfunction_candidates_head_ == NULL);
}


void CodeFlusher::EvictSharedFunctionInfoCandidates() {
  SharedFunctionInfo* candidate = shared_function_info_candidates_head_;
  SharedFunctionInfo* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    EvictCandidate(candidate);
    candidate = next_candidate;
  }
  ASSERT(shared_function_info_candidates_head_ == NULL);
}


void CodeFlusher::EvictOptimizedCodeMaps() {
  SharedFunctionInfo* holder = optimized_code_map_holder_head_;
  SharedFunctionInfo* next_holder;
  while (holder != NULL) {
    next_holder = GetNextCodeMap(holder);
    EvictOptimizedCodeMap(holder);
    holder = next_holder;
  }
  ASSERT(optimized_code_map_holder_head_ == NULL);
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
  if (!FLAG_clever_optimizations) return object;
  Map* map = object->map();
  InstanceType type = map->instance_type();
  if ((type & kShortcutTypeMask) != kShortcutTypeTag) return object;

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
  static void ObjectStatsVisitBase(StaticVisitorBase::VisitorId id,
                                   Map* map, HeapObject* obj);

  static void ObjectStatsCountFixedArray(
      FixedArrayBase* fixed_array,
      FixedArraySubInstanceType fast_type,
      FixedArraySubInstanceType dictionary_type);

  template<MarkCompactMarkingVisitor::VisitorId id>
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
    if (!mark_bit.Get()) {
      heap->mark_compact_collector()->SetMark(object, mark_bit);
      return true;
    }
    return false;
  }

  // Mark object pointed to by p.
  INLINE(static void MarkObjectByPointer(MarkCompactCollector* collector,
                                         Object** anchor_slot,
                                         Object** p)) {
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
    ASSERT(collector->heap()->Contains(obj));
    ASSERT(!collector->heap()->mark_compact_collector()->IsMarked(obj));
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
  INLINE(static bool VisitUnmarkedObjects(Heap* heap,
                                          Object** start,
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
      if (mark.Get()) continue;
      VisitUnmarkedObject(collector, obj);
    }
    return true;
  }

  INLINE(static void BeforeVisitingSharedFunctionInfo(HeapObject* object)) {
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(object);
    shared->BeforeVisitingPointers();
  }

  static void VisitWeakCollection(Map* map, HeapObject* object) {
    MarkCompactCollector* collector = map->GetHeap()->mark_compact_collector();
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(object);

    // Enqueue weak map in linked list of encountered weak maps.
    if (weak_collection->next() == Smi::FromInt(0)) {
      weak_collection->set_next(collector->encountered_weak_collections());
      collector->set_encountered_weak_collections(weak_collection);
    }

    // Skip visiting the backing hash table containing the mappings.
    int object_size = JSWeakCollection::BodyDescriptor::SizeOf(map, object);
    BodyVisitorBase<MarkCompactMarkingVisitor>::IteratePointers(
        map->GetHeap(),
        object,
        JSWeakCollection::BodyDescriptor::kStartOffset,
        JSWeakCollection::kTableOffset);
    BodyVisitorBase<MarkCompactMarkingVisitor>::IteratePointers(
        map->GetHeap(),
        object,
        JSWeakCollection::kTableOffset + kPointerSize,
        object_size);

    // Mark the backing hash table without pushing it on the marking stack.
    Object* table_object = weak_collection->table();
    if (!table_object->IsHashTable()) return;
    ObjectHashTable* table = ObjectHashTable::cast(table_object);
    Object** table_slot =
        HeapObject::RawField(weak_collection, JSWeakCollection::kTableOffset);
    MarkBit table_mark = Marking::MarkBitFrom(table);
    collector->RecordSlot(table_slot, table_slot, table);
    if (!table_mark.Get()) collector->SetMark(table, table_mark);
    // Recording the map slot can be skipped, because maps are not compacted.
    collector->MarkObject(table->map(), Marking::MarkBitFrom(table->map()));
    ASSERT(MarkCompactCollector::IsMarked(table->map()));
  }

 private:
  template<int id>
  static inline void TrackObjectStatsAndVisit(Map* map, HeapObject* obj);

  // Code flushing support.

  static const int kRegExpCodeThreshold = 5;

  static void UpdateRegExpCodeAgeAndFlush(Heap* heap,
                                          JSRegExp* re,
                                          bool is_ascii) {
    // Make sure that the fixed array is in fact initialized on the RegExp.
    // We could potentially trigger a GC when initializing the RegExp.
    if (HeapObject::cast(re->data())->map()->instance_type() !=
            FIXED_ARRAY_TYPE) return;

    // Make sure this is a RegExp that actually contains code.
    if (re->TypeTag() != JSRegExp::IRREGEXP) return;

    Object* code = re->DataAt(JSRegExp::code_index(is_ascii));
    if (!code->IsSmi() &&
        HeapObject::cast(code)->map()->instance_type() == CODE_TYPE) {
      // Save a copy that can be reinstated if we need the code again.
      re->SetDataAt(JSRegExp::saved_code_index(is_ascii), code);

      // Saving a copy might create a pointer into compaction candidate
      // that was not observed by marker.  This might happen if JSRegExp data
      // was marked through the compilation cache before marker reached JSRegExp
      // object.
      FixedArray* data = FixedArray::cast(re->data());
      Object** slot = data->data_start() + JSRegExp::saved_code_index(is_ascii);
      heap->mark_compact_collector()->
          RecordSlot(slot, slot, code);

      // Set a number in the 0-255 range to guarantee no smi overflow.
      re->SetDataAt(JSRegExp::code_index(is_ascii),
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
        re->SetDataAt(JSRegExp::code_index(is_ascii),
                      Smi::FromInt(JSRegExp::kUninitializedValue));
        re->SetDataAt(JSRegExp::saved_code_index(is_ascii),
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
    // Flush code or set age on both ASCII and two byte code.
    UpdateRegExpCodeAgeAndFlush(heap, re, true);
    UpdateRegExpCodeAgeAndFlush(heap, re, false);
    // Visit the fields of the RegExp, including the updated FixedArray.
    VisitJSRegExp(map, object);
  }

  static VisitorDispatchTable<Callback> non_count_table_;
};


void MarkCompactMarkingVisitor::ObjectStatsCountFixedArray(
    FixedArrayBase* fixed_array,
    FixedArraySubInstanceType fast_type,
    FixedArraySubInstanceType dictionary_type) {
  Heap* heap = fixed_array->map()->GetHeap();
  if (fixed_array->map() != heap->fixed_cow_array_map() &&
      fixed_array->map() != heap->fixed_double_array_map() &&
      fixed_array != heap->empty_fixed_array()) {
    if (fixed_array->IsDictionary()) {
      heap->RecordObjectStats(FIXED_ARRAY_TYPE,
                              dictionary_type,
                              fixed_array->Size());
    } else {
      heap->RecordObjectStats(FIXED_ARRAY_TYPE,
                              fast_type,
                              fixed_array->Size());
    }
  }
}


void MarkCompactMarkingVisitor::ObjectStatsVisitBase(
    MarkCompactMarkingVisitor::VisitorId id, Map* map, HeapObject* obj) {
  Heap* heap = map->GetHeap();
  int object_size = obj->Size();
  heap->RecordObjectStats(map->instance_type(), -1, object_size);
  non_count_table_.GetVisitorById(id)(map, obj);
  if (obj->IsJSObject()) {
    JSObject* object = JSObject::cast(obj);
    ObjectStatsCountFixedArray(object->elements(),
                               DICTIONARY_ELEMENTS_SUB_TYPE,
                               FAST_ELEMENTS_SUB_TYPE);
    ObjectStatsCountFixedArray(object->properties(),
                               DICTIONARY_PROPERTIES_SUB_TYPE,
                               FAST_PROPERTIES_SUB_TYPE);
  }
}


template<MarkCompactMarkingVisitor::VisitorId id>
void MarkCompactMarkingVisitor::ObjectStatsTracker<id>::Visit(
    Map* map, HeapObject* obj) {
  ObjectStatsVisitBase(id, map, obj);
}


template<>
class MarkCompactMarkingVisitor::ObjectStatsTracker<
    MarkCompactMarkingVisitor::kVisitMap> {
 public:
  static inline void Visit(Map* map, HeapObject* obj) {
    Heap* heap = map->GetHeap();
    Map* map_obj = Map::cast(obj);
    ASSERT(map->instance_type() == MAP_TYPE);
    DescriptorArray* array = map_obj->instance_descriptors();
    if (map_obj->owns_descriptors() &&
        array != heap->empty_descriptor_array()) {
      int fixed_array_size = array->Size();
      heap->RecordObjectStats(FIXED_ARRAY_TYPE,
                              DESCRIPTOR_ARRAY_SUB_TYPE,
                              fixed_array_size);
    }
    if (map_obj->HasTransitionArray()) {
      int fixed_array_size = map_obj->transitions()->Size();
      heap->RecordObjectStats(FIXED_ARRAY_TYPE,
                              TRANSITION_ARRAY_SUB_TYPE,
                              fixed_array_size);
    }
    if (map_obj->has_code_cache()) {
      CodeCache* cache = CodeCache::cast(map_obj->code_cache());
      heap->RecordObjectStats(
          FIXED_ARRAY_TYPE,
          MAP_CODE_CACHE_SUB_TYPE,
          cache->default_cache()->Size());
      if (!cache->normal_type_cache()->IsUndefined()) {
        heap->RecordObjectStats(
            FIXED_ARRAY_TYPE,
            MAP_CODE_CACHE_SUB_TYPE,
            FixedArray::cast(cache->normal_type_cache())->Size());
      }
    }
    ObjectStatsVisitBase(kVisitMap, map, obj);
  }
};


template<>
class MarkCompactMarkingVisitor::ObjectStatsTracker<
    MarkCompactMarkingVisitor::kVisitCode> {
 public:
  static inline void Visit(Map* map, HeapObject* obj) {
    Heap* heap = map->GetHeap();
    int object_size = obj->Size();
    ASSERT(map->instance_type() == CODE_TYPE);
    heap->RecordObjectStats(CODE_TYPE, Code::cast(obj)->kind(), object_size);
    ObjectStatsVisitBase(kVisitCode, map, obj);
  }
};


template<>
class MarkCompactMarkingVisitor::ObjectStatsTracker<
    MarkCompactMarkingVisitor::kVisitSharedFunctionInfo> {
 public:
  static inline void Visit(Map* map, HeapObject* obj) {
    Heap* heap = map->GetHeap();
    SharedFunctionInfo* sfi = SharedFunctionInfo::cast(obj);
    if (sfi->scope_info() != heap->empty_fixed_array()) {
      heap->RecordObjectStats(
          FIXED_ARRAY_TYPE,
          SCOPE_INFO_SUB_TYPE,
          FixedArray::cast(sfi->scope_info())->Size());
    }
    ObjectStatsVisitBase(kVisitSharedFunctionInfo, map, obj);
  }
};


template<>
class MarkCompactMarkingVisitor::ObjectStatsTracker<
    MarkCompactMarkingVisitor::kVisitFixedArray> {
 public:
  static inline void Visit(Map* map, HeapObject* obj) {
    Heap* heap = map->GetHeap();
    FixedArray* fixed_array = FixedArray::cast(obj);
    if (fixed_array == heap->string_table()) {
      heap->RecordObjectStats(
          FIXED_ARRAY_TYPE,
          STRING_TABLE_SUB_TYPE,
          fixed_array->Size());
    }
    ObjectStatsVisitBase(kVisitFixedArray, map, obj);
  }
};


void MarkCompactMarkingVisitor::Initialize() {
  StaticMarkingVisitor<MarkCompactMarkingVisitor>::Initialize();

  table_.Register(kVisitJSRegExp,
                  &VisitRegExpAndFlushCode);

  if (FLAG_track_gc_object_stats) {
    // Copy the visitor table to make call-through possible.
    non_count_table_.CopyFrom(&table_);
#define VISITOR_ID_COUNT_FUNCTION(id)                                   \
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
  // Enable code flushing for non-incremental cycles.
  if (FLAG_flush_code && !FLAG_flush_code_incrementally) {
    EnableCodeFlushing(!was_marked_incrementally_);
  }

  // If code flushing is disabled, there is no need to prepare for it.
  if (!is_code_flushing_enabled()) return;

  // Ensure that empty descriptor array is marked. Method MarkDescriptorArray
  // relies on it being marked before any other descriptor array.
  HeapObject* descriptor_array = heap()->empty_descriptor_array();
  MarkBit descriptor_array_mark = Marking::MarkBitFrom(descriptor_array);
  MarkObject(descriptor_array, descriptor_array_mark);

  // Make sure we are not referencing the code from the stack.
  ASSERT(this == heap()->mark_compact_collector());
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
    : collector_(heap->mark_compact_collector()) { }

  void VisitPointer(Object** p) {
    MarkObjectByPointer(p);
  }

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

 private:
  void MarkObjectByPointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    // Replace flat cons strings in place.
    HeapObject* object = ShortCircuitConsString(p);
    MarkBit mark_bit = Marking::MarkBitFrom(object);
    if (mark_bit.Get()) return;

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
class StringTableCleaner : public ObjectVisitor {
 public:
  explicit StringTableCleaner(Heap* heap)
    : heap_(heap), pointers_removed_(0) { }

  virtual void VisitPointers(Object** start, Object** end) {
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (o->IsHeapObject() &&
          !Marking::MarkBitFrom(HeapObject::cast(o)).Get()) {
        // Check if the internalized string being pruned is external. We need to
        // delete the associated external data as this string is going away.

        // Since no objects have yet been moved we can safely access the map of
        // the object.
        if (o->IsExternalString()) {
          heap_->FinalizeExternalString(String::cast(*p));
        }
        // Set the entry to the_hole_value (as deleted).
        *p = heap_->the_hole_value();
        pointers_removed_++;
      }
    }
  }

  int PointersRemoved() {
    return pointers_removed_;
  }

 private:
  Heap* heap_;
  int pointers_removed_;
};


// Implementation of WeakObjectRetainer for mark compact GCs. All marked objects
// are retained.
class MarkCompactWeakObjectRetainer : public WeakObjectRetainer {
 public:
  virtual Object* RetainAs(Object* object) {
    if (Marking::MarkBitFrom(HeapObject::cast(object)).Get()) {
      return object;
    } else {
      return NULL;
    }
  }
};


// Fill the marking stack with overflowed objects returned by the given
// iterator.  Stop when the marking stack is filled or the end of the space
// is reached, whichever comes first.
template<class T>
static void DiscoverGreyObjectsWithIterator(Heap* heap,
                                            MarkingDeque* marking_deque,
                                            T* it) {
  // The caller should ensure that the marking stack is initially not full,
  // so that we don't waste effort pointlessly scanning for objects.
  ASSERT(!marking_deque->IsFull());

  Map* filler_map = heap->one_pointer_filler_map();
  for (HeapObject* object = it->Next();
       object != NULL;
       object = it->Next()) {
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
  ASSERT(!marking_deque->IsFull());
  ASSERT(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
  ASSERT(strcmp(Marking::kGreyBitPattern, "11") == 0);
  ASSERT(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  for (MarkBitCellIterator it(p); !it.Done(); it.Advance()) {
    Address cell_base = it.CurrentCellBase();
    MarkBit::CellType* cell = it.CurrentCell();

    const MarkBit::CellType current_cell = *cell;
    if (current_cell == 0) continue;

    MarkBit::CellType grey_objects;
    if (it.HasNext()) {
      const MarkBit::CellType next_cell = *(cell+1);
      grey_objects = current_cell &
          ((current_cell >> 1) | (next_cell << (Bitmap::kBitsPerCell - 1)));
    } else {
      grey_objects = current_cell & (current_cell >> 1);
    }

    int offset = 0;
    while (grey_objects != 0) {
      int trailing_zeros = CompilerIntrinsics::CountTrailingZeros(grey_objects);
      grey_objects >>= trailing_zeros;
      offset += trailing_zeros;
      MarkBit markbit(cell, 1 << offset, false);
      ASSERT(Marking::IsGrey(markbit));
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


int MarkCompactCollector::DiscoverAndPromoteBlackObjectsOnPage(
    NewSpace* new_space,
    NewSpacePage* p) {
  ASSERT(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
  ASSERT(strcmp(Marking::kGreyBitPattern, "11") == 0);
  ASSERT(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  MarkBit::CellType* cells = p->markbits()->cells();
  int survivors_size = 0;

  for (MarkBitCellIterator it(p); !it.Done(); it.Advance()) {
    Address cell_base = it.CurrentCellBase();
    MarkBit::CellType* cell = it.CurrentCell();

    MarkBit::CellType current_cell = *cell;
    if (current_cell == 0) continue;

    int offset = 0;
    while (current_cell != 0) {
      int trailing_zeros = CompilerIntrinsics::CountTrailingZeros(current_cell);
      current_cell >>= trailing_zeros;
      offset += trailing_zeros;
      Address address = cell_base + offset * kPointerSize;
      HeapObject* object = HeapObject::FromAddress(address);

      int size = object->Size();
      survivors_size += size;

      offset++;
      current_cell >>= 1;
      // Aggressively promote young survivors to the old space.
      if (TryPromoteObject(object, size)) {
        continue;
      }

      // Promotion failed. Just migrate object to another semispace.
      MaybeObject* allocation = new_space->AllocateRaw(size);
      if (allocation->IsFailure()) {
        if (!new_space->AddFreshPage()) {
          // Shouldn't happen. We are sweeping linearly, and to-space
          // has the same number of pages as from-space, so there is
          // always room.
          UNREACHABLE();
        }
        allocation = new_space->AllocateRaw(size);
        ASSERT(!allocation->IsFailure());
      }
      Object* target = allocation->ToObjectUnchecked();

      MigrateObject(HeapObject::cast(target)->address(),
                    object->address(),
                    size,
                    NEW_SPACE);
    }
    *cells = 0;
  }
  return survivors_size;
}


static void DiscoverGreyObjectsInSpace(Heap* heap,
                                       MarkingDeque* marking_deque,
                                       PagedSpace* space) {
  if (!space->was_swept_conservatively()) {
    HeapObjectIterator it(space);
    DiscoverGreyObjectsWithIterator(heap, marking_deque, &it);
  } else {
    PageIterator it(space);
    while (it.has_next()) {
      Page* p = it.next();
      DiscoverGreyObjectsOnPage(marking_deque, p);
      if (marking_deque->IsFull()) return;
    }
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
  return !mark.Get();
}


bool MarkCompactCollector::IsUnmarkedHeapObjectWithHeap(Heap* heap,
                                                        Object** p) {
  Object* o = *p;
  ASSERT(o->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(o);
  MarkBit mark = Marking::MarkBitFrom(heap_object);
  return !mark.Get();
}


void MarkCompactCollector::MarkStringTable(RootMarkingVisitor* visitor) {
  StringTable* string_table = heap()->string_table();
  // Mark the string table itself.
  MarkBit string_table_mark = Marking::MarkBitFrom(string_table);
  SetMark(string_table, string_table_mark);
  // Explicitly mark the prefix.
  string_table->IteratePrefix(visitor);
  ProcessMarkingDeque();
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


void MarkCompactCollector::MarkImplicitRefGroups() {
  List<ImplicitRefGroup*>* ref_groups =
      isolate()->global_handles()->implicit_ref_groups();

  int last = 0;
  for (int i = 0; i < ref_groups->length(); i++) {
    ImplicitRefGroup* entry = ref_groups->at(i);
    ASSERT(entry != NULL);

    if (!IsMarked(*entry->parent)) {
      (*ref_groups)[last++] = entry;
      continue;
    }

    Object*** children = entry->children;
    // A parent object is marked, so mark all child heap objects.
    for (size_t j = 0; j < entry->length; ++j) {
      if ((*children[j])->IsHeapObject()) {
        HeapObject* child = HeapObject::cast(*children[j]);
        MarkBit mark = Marking::MarkBitFrom(child);
        MarkObject(child, mark);
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
  while (!marking_deque_.IsEmpty()) {
    HeapObject* object = marking_deque_.Pop();
    ASSERT(object->IsHeapObject());
    ASSERT(heap()->Contains(object));
    ASSERT(Marking::IsBlack(Marking::MarkBitFrom(object)));

    Map* map = object->map();
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
  ASSERT(marking_deque_.overflowed());

  DiscoverGreyObjectsInNewSpace(heap(), &marking_deque_);
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap(),
                             &marking_deque_,
                             heap()->old_pointer_space());
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap(),
                             &marking_deque_,
                             heap()->old_data_space());
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap(),
                             &marking_deque_,
                             heap()->code_space());
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap(),
                             &marking_deque_,
                             heap()->map_space());
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap(),
                             &marking_deque_,
                             heap()->cell_space());
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap(),
                             &marking_deque_,
                             heap()->property_cell_space());
  if (marking_deque_.IsFull()) return;

  LargeObjectIterator lo_it(heap()->lo_space());
  DiscoverGreyObjectsWithIterator(heap(),
                                  &marking_deque_,
                                  &lo_it);
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
void MarkCompactCollector::ProcessEphemeralMarking(ObjectVisitor* visitor) {
  bool work_to_do = true;
  ASSERT(marking_deque_.IsEmpty());
  while (work_to_do) {
    isolate()->global_handles()->IterateObjectGroups(
        visitor, &IsUnmarkedHeapObjectWithHeap);
    MarkImplicitRefGroups();
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


void MarkCompactCollector::MarkLiveObjects() {
  GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_MARK);
  // The recursive GC marker detects when it is nearing stack overflow,
  // and switches to a different marking system.  JS interrupts interfere
  // with the C stack limit check.
  PostponeInterruptsScope postpone(isolate());

  bool incremental_marking_overflowed = false;
  IncrementalMarking* incremental_marking = heap_->incremental_marking();
  if (was_marked_incrementally_) {
    // Finalize the incremental marking and check whether we had an overflow.
    // Both markers use grey color to mark overflowed objects so
    // non-incremental marker can deal with them as if overflow
    // occured during normal marking.
    // But incremental marker uses a separate marking deque
    // so we have to explicitly copy its overflow state.
    incremental_marking->Finalize();
    incremental_marking_overflowed =
        incremental_marking->marking_deque()->overflowed();
    incremental_marking->marking_deque()->ClearOverflowed();
  } else {
    // Abort any pending incremental activities e.g. incremental sweeping.
    incremental_marking->Abort();
  }

#ifdef DEBUG
  ASSERT(state_ == PREPARE_GC);
  state_ = MARK_LIVE_OBJECTS;
#endif
  // The to space contains live objects, a page in from space is used as a
  // marking stack.
  Address marking_deque_start = heap()->new_space()->FromSpacePageLow();
  Address marking_deque_end = heap()->new_space()->FromSpacePageHigh();
  if (FLAG_force_marking_deque_overflows) {
    marking_deque_end = marking_deque_start + 64 * kPointerSize;
  }
  marking_deque_.Initialize(marking_deque_start,
                            marking_deque_end);
  ASSERT(!marking_deque_.overflowed());

  if (incremental_marking_overflowed) {
    // There are overflowed objects left in the heap after incremental marking.
    marking_deque_.SetOverflowed();
  }

  PrepareForCodeFlushing();

  if (was_marked_incrementally_) {
    // There is no write barrier on cells so we have to scan them now at the end
    // of the incremental marking.
    {
      HeapObjectIterator cell_iterator(heap()->cell_space());
      HeapObject* cell;
      while ((cell = cell_iterator.Next()) != NULL) {
        ASSERT(cell->IsCell());
        if (IsMarked(cell)) {
          int offset = Cell::kValueOffset;
          MarkCompactMarkingVisitor::VisitPointer(
              heap(),
              reinterpret_cast<Object**>(cell->address() + offset));
        }
      }
    }
    {
      HeapObjectIterator js_global_property_cell_iterator(
          heap()->property_cell_space());
      HeapObject* cell;
      while ((cell = js_global_property_cell_iterator.Next()) != NULL) {
        ASSERT(cell->IsPropertyCell());
        if (IsMarked(cell)) {
          MarkCompactMarkingVisitor::VisitPropertyCell(cell->map(), cell);
        }
      }
    }
  }

  RootMarkingVisitor root_visitor(heap());
  MarkRoots(&root_visitor);

  ProcessTopOptimizedFrame(&root_visitor);

  // The objects reachable from the roots are marked, yet unreachable
  // objects are unmarked.  Mark objects reachable due to host
  // application specific logic or through Harmony weak maps.
  ProcessEphemeralMarking(&root_visitor);

  // The objects reachable from the roots, weak maps or object groups
  // are marked, yet unreachable objects are unmarked.  Mark objects
  // reachable only from weak global handles.
  //
  // First we identify nonlive weak handles and mark them as pending
  // destruction.
  heap()->isolate()->global_handles()->IdentifyWeakHandles(
      &IsUnmarkedHeapObject);
  // Then we mark the objects and process the transitive closure.
  heap()->isolate()->global_handles()->IterateWeakRoots(&root_visitor);
  while (marking_deque_.overflowed()) {
    RefillMarkingDeque();
    EmptyMarkingDeque();
  }

  // Repeat host application specific and Harmony weak maps marking to
  // mark unmarked objects reachable from the weak roots.
  ProcessEphemeralMarking(&root_visitor);

  AfterMarking();
}


void MarkCompactCollector::AfterMarking() {
  // Object literal map caches reference strings (cache keys) and maps
  // (cache values). At this point still useful maps have already been
  // marked. Mark the keys for the alive values before we process the
  // string table.
  ProcessMapCaches();

  // Prune the string table removing all strings only pointed to by the
  // string table.  Cannot use string_table() here because the string
  // table is marked.
  StringTable* string_table = heap()->string_table();
  StringTableCleaner v(heap());
  string_table->IterateElements(&v);
  string_table->ElementsRemoved(v.PointersRemoved());
  heap()->external_string_table_.Iterate(&v);
  heap()->external_string_table_.CleanUp();

  // Process the weak references.
  MarkCompactWeakObjectRetainer mark_compact_object_retainer;
  heap()->ProcessWeakReferences(&mark_compact_object_retainer);

  // Remove object groups after marking phase.
  heap()->isolate()->global_handles()->RemoveObjectGroups();
  heap()->isolate()->global_handles()->RemoveImplicitRefGroups();

  // Flush code from collected candidates.
  if (is_code_flushing_enabled()) {
    code_flusher_->ProcessCandidates();
    // If incremental marker does not support code flushing, we need to
    // disable it before incremental marking steps for next cycle.
    if (FLAG_flush_code && !FLAG_flush_code_incrementally) {
      EnableCodeFlushing(false);
    }
  }

  if (!FLAG_watch_ic_patching) {
    // Clean up dead objects from the runtime profiler.
    heap()->isolate()->runtime_profiler()->RemoveDeadSamples();
  }

  if (FLAG_track_gc_object_stats) {
    heap()->CheckpointObjectStats();
  }
}


void MarkCompactCollector::ProcessMapCaches() {
  Object* raw_context = heap()->native_contexts_list_;
  while (raw_context != heap()->undefined_value()) {
    Context* context = reinterpret_cast<Context*>(raw_context);
    if (IsMarked(context)) {
      HeapObject* raw_map_cache =
          HeapObject::cast(context->get(Context::MAP_CACHE_INDEX));
      // A map cache may be reachable from the stack. In this case
      // it's already transitively marked and it's too late to clean
      // up its parts.
      if (!IsMarked(raw_map_cache) &&
          raw_map_cache != heap()->undefined_value()) {
        MapCache* map_cache = reinterpret_cast<MapCache*>(raw_map_cache);
        int existing_elements = map_cache->NumberOfElements();
        int used_elements = 0;
        for (int i = MapCache::kElementsStartIndex;
             i < map_cache->length();
             i += MapCache::kEntrySize) {
          Object* raw_key = map_cache->get(i);
          if (raw_key == heap()->undefined_value() ||
              raw_key == heap()->the_hole_value()) continue;
          STATIC_ASSERT(MapCache::kEntrySize == 2);
          Object* raw_map = map_cache->get(i + 1);
          if (raw_map->IsHeapObject() && IsMarked(raw_map)) {
            ++used_elements;
          } else {
            // Delete useless entries with unmarked maps.
            ASSERT(raw_map->IsMap());
            map_cache->set_the_hole(i);
            map_cache->set_the_hole(i + 1);
          }
        }
        if (used_elements == 0) {
          context->set(Context::MAP_CACHE_INDEX, heap()->undefined_value());
        } else {
          // Note: we don't actually shrink the cache here to avoid
          // extra complexity during GC. We rely on subsequent cache
          // usages (EnsureCapacity) to do this.
          map_cache->ElementsRemoved(existing_elements - used_elements);
          MarkBit map_cache_markbit = Marking::MarkBitFrom(map_cache);
          MarkObject(map_cache, map_cache_markbit);
        }
      }
    }
    // Move to next element in the list.
    raw_context = context->get(Context::NEXT_CONTEXT_LINK);
  }
  ProcessMarkingDeque();
}


void MarkCompactCollector::ReattachInitialMaps() {
  HeapObjectIterator map_iterator(heap()->map_space());
  for (HeapObject* obj = map_iterator.Next();
       obj != NULL;
       obj = map_iterator.Next()) {
    Map* map = Map::cast(obj);

    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    if (map->instance_type() < FIRST_JS_RECEIVER_TYPE) continue;

    if (map->attached_to_shared_function_info()) {
      JSFunction::cast(map->constructor())->shared()->AttachInitialMap(map);
    }
  }
}


void MarkCompactCollector::ClearNonLiveReferences() {
  // Iterate over the map space, setting map transitions that go from
  // a marked map to an unmarked map to null transitions.  This action
  // is carried out only on maps of JSObjects and related subtypes.
  HeapObjectIterator map_iterator(heap()->map_space());
  for (HeapObject* obj = map_iterator.Next();
       obj != NULL;
       obj = map_iterator.Next()) {
    Map* map = Map::cast(obj);

    if (!map->CanTransition()) continue;

    MarkBit map_mark = Marking::MarkBitFrom(map);
    if (map_mark.Get() && map->attached_to_shared_function_info()) {
      // This map is used for inobject slack tracking and has been detached
      // from SharedFunctionInfo during the mark phase.
      // Since it survived the GC, reattach it now.
      JSFunction::cast(map->constructor())->shared()->AttachInitialMap(map);
    }

    ClearNonLivePrototypeTransitions(map);
    ClearNonLiveMapTransitions(map, map_mark);

    if (map_mark.Get()) {
      ClearNonLiveDependentCode(map->dependent_code());
    } else {
      ClearAndDeoptimizeDependentCode(map);
    }
  }

  // Iterate over property cell space, removing dependent code that is not
  // otherwise kept alive by strong references.
  HeapObjectIterator cell_iterator(heap_->property_cell_space());
  for (HeapObject* cell = cell_iterator.Next();
       cell != NULL;
       cell = cell_iterator.Next()) {
    if (IsMarked(cell)) {
      ClearNonLiveDependentCode(PropertyCell::cast(cell)->dependent_code());
    }
  }
}


void MarkCompactCollector::ClearNonLivePrototypeTransitions(Map* map) {
  int number_of_transitions = map->NumberOfProtoTransitions();
  FixedArray* prototype_transitions = map->GetPrototypeTransitions();

  int new_number_of_transitions = 0;
  const int header = Map::kProtoTransitionHeaderSize;
  const int proto_offset = header + Map::kProtoTransitionPrototypeOffset;
  const int map_offset = header + Map::kProtoTransitionMapOffset;
  const int step = Map::kProtoTransitionElementsPerEntry;
  for (int i = 0; i < number_of_transitions; i++) {
    Object* prototype = prototype_transitions->get(proto_offset + i * step);
    Object* cached_map = prototype_transitions->get(map_offset + i * step);
    if (IsMarked(prototype) && IsMarked(cached_map)) {
      int proto_index = proto_offset + new_number_of_transitions * step;
      int map_index = map_offset + new_number_of_transitions * step;
      if (new_number_of_transitions != i) {
        prototype_transitions->set(
            proto_index,
            prototype,
            UPDATE_WRITE_BARRIER);
        prototype_transitions->set(
            map_index,
            cached_map,
            SKIP_WRITE_BARRIER);
      }
      Object** slot =
          HeapObject::RawField(prototype_transitions,
                               FixedArray::OffsetOfElementAt(proto_index));
      RecordSlot(slot, slot, prototype);
      new_number_of_transitions++;
    }
  }

  if (new_number_of_transitions != number_of_transitions) {
    map->SetNumberOfProtoTransitions(new_number_of_transitions);
  }

  // Fill slots that became free with undefined value.
  for (int i = new_number_of_transitions * step;
       i < number_of_transitions * step;
       i++) {
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
  bool current_is_alive = map_mark.Get();
  bool parent_is_alive = Marking::MarkBitFrom(parent).Get();
  if (!current_is_alive && parent_is_alive) {
    parent->ClearNonLiveTransitions(heap());
  }
}


void MarkCompactCollector::ClearAndDeoptimizeDependentCode(Map* map) {
  DisallowHeapAllocation no_allocation;
  DependentCode* entries = map->dependent_code();
  DependentCode::GroupStartIndexes starts(entries);
  int number_of_entries = starts.number_of_entries();
  if (number_of_entries == 0) return;
  for (int i = 0; i < number_of_entries; i++) {
    // If the entry is compilation info then the map must be alive,
    // and ClearAndDeoptimizeDependentCode shouldn't be called.
    ASSERT(entries->is_code_at(i));
    Code* code = entries->code_at(i);

    if (IsMarked(code) && !code->marked_for_deoptimization()) {
      code->set_marked_for_deoptimization(true);
      have_code_to_deoptimize_ = true;
    }
    entries->clear_at(i);
  }
  map->set_dependent_code(DependentCode::cast(heap()->empty_fixed_array()));
}


void MarkCompactCollector::ClearNonLiveDependentCode(DependentCode* entries) {
  DisallowHeapAllocation no_allocation;
  DependentCode::GroupStartIndexes starts(entries);
  int number_of_entries = starts.number_of_entries();
  if (number_of_entries == 0) return;
  int new_number_of_entries = 0;
  // Go through all groups, remove dead codes and compact.
  for (int g = 0; g < DependentCode::kGroupCount; g++) {
    int group_number_of_entries = 0;
    for (int i = starts.at(g); i < starts.at(g + 1); i++) {
      Object* obj = entries->object_at(i);
      ASSERT(obj->IsCode() || IsMarked(obj));
      if (IsMarked(obj) &&
          (!obj->IsCode() || !WillBeDeoptimized(Code::cast(obj)))) {
        if (new_number_of_entries + group_number_of_entries != i) {
          entries->set_object_at(
              new_number_of_entries + group_number_of_entries, obj);
        }
        Object** slot = entries->slot_at(new_number_of_entries +
                                         group_number_of_entries);
        RecordSlot(slot, slot, obj);
        group_number_of_entries++;
      }
    }
    entries->set_number_of_entries(
        static_cast<DependentCode::DependencyGroup>(g),
        group_number_of_entries);
    new_number_of_entries += group_number_of_entries;
  }
  for (int i = new_number_of_entries; i < number_of_entries; i++) {
    entries->clear_at(i);
  }
}


void MarkCompactCollector::ProcessWeakCollections() {
  GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_WEAKCOLLECTION_PROCESS);
  Object* weak_collection_obj = encountered_weak_collections();
  while (weak_collection_obj != Smi::FromInt(0)) {
    ASSERT(MarkCompactCollector::IsMarked(
        HeapObject::cast(weak_collection_obj)));
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    ObjectHashTable* table = ObjectHashTable::cast(weak_collection->table());
    Object** anchor = reinterpret_cast<Object**>(table->address());
    for (int i = 0; i < table->Capacity(); i++) {
      if (MarkCompactCollector::IsMarked(HeapObject::cast(table->KeyAt(i)))) {
        Object** key_slot =
            HeapObject::RawField(table, FixedArray::OffsetOfElementAt(
                ObjectHashTable::EntryToIndex(i)));
        RecordSlot(anchor, key_slot, *key_slot);
        Object** value_slot =
            HeapObject::RawField(table, FixedArray::OffsetOfElementAt(
                ObjectHashTable::EntryToValueIndex(i)));
        MarkCompactMarkingVisitor::MarkObjectByPointer(
            this, anchor, value_slot);
      }
    }
    weak_collection_obj = weak_collection->next();
  }
}


void MarkCompactCollector::ClearWeakCollections() {
  GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_WEAKCOLLECTION_CLEAR);
  Object* weak_collection_obj = encountered_weak_collections();
  while (weak_collection_obj != Smi::FromInt(0)) {
    ASSERT(MarkCompactCollector::IsMarked(
        HeapObject::cast(weak_collection_obj)));
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    ObjectHashTable* table = ObjectHashTable::cast(weak_collection->table());
    for (int i = 0; i < table->Capacity(); i++) {
      if (!MarkCompactCollector::IsMarked(HeapObject::cast(table->KeyAt(i)))) {
        table->RemoveEntry(i);
      }
    }
    weak_collection_obj = weak_collection->next();
    weak_collection->set_next(Smi::FromInt(0));
  }
  set_encountered_weak_collections(Smi::FromInt(0));
}


// We scavange new space simultaneously with sweeping. This is done in two
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
void MarkCompactCollector::MigrateObject(Address dst,
                                         Address src,
                                         int size,
                                         AllocationSpace dest) {
  HEAP_PROFILE(heap(), ObjectMoveEvent(src, dst));
  // TODO(hpayer): Replace these checks with asserts.
  CHECK(heap()->AllowedToBeMigrated(HeapObject::FromAddress(src), dest));
  CHECK(dest != LO_SPACE && size <= Page::kMaxNonCodeHeapObjectSize);
  if (dest == OLD_POINTER_SPACE) {
    Address src_slot = src;
    Address dst_slot = dst;
    ASSERT(IsAligned(size, kPointerSize));

    for (int remaining = size / kPointerSize; remaining > 0; remaining--) {
      Object* value = Memory::Object_at(src_slot);

      Memory::Object_at(dst_slot) = value;

      if (heap_->InNewSpace(value)) {
        heap_->store_buffer()->Mark(dst_slot);
      } else if (value->IsHeapObject() && IsOnEvacuationCandidate(value)) {
        SlotsBuffer::AddTo(&slots_buffer_allocator_,
                           &migration_slots_buffer_,
                           reinterpret_cast<Object**>(dst_slot),
                           SlotsBuffer::IGNORE_OVERFLOW);
      }

      src_slot += kPointerSize;
      dst_slot += kPointerSize;
    }

    if (compacting_ && HeapObject::FromAddress(dst)->IsJSFunction()) {
      Address code_entry_slot = dst + JSFunction::kCodeEntryOffset;
      Address code_entry = Memory::Address_at(code_entry_slot);

      if (Page::FromAddress(code_entry)->IsEvacuationCandidate()) {
        SlotsBuffer::AddTo(&slots_buffer_allocator_,
                           &migration_slots_buffer_,
                           SlotsBuffer::CODE_ENTRY_SLOT,
                           code_entry_slot,
                           SlotsBuffer::IGNORE_OVERFLOW);
      }
    }
  } else if (dest == CODE_SPACE) {
    PROFILE(isolate(), CodeMoveEvent(src, dst));
    heap()->MoveBlock(dst, src, size);
    SlotsBuffer::AddTo(&slots_buffer_allocator_,
                       &migration_slots_buffer_,
                       SlotsBuffer::RELOCATED_CODE_OBJECT,
                       dst,
                       SlotsBuffer::IGNORE_OVERFLOW);
    Code::cast(HeapObject::FromAddress(dst))->Relocate(dst - src);
  } else {
    ASSERT(dest == OLD_DATA_SPACE || dest == NEW_SPACE);
    heap()->MoveBlock(dst, src, size);
  }
  Memory::Address_at(src) = dst;
}


// Visitor for updating pointers from live objects in old spaces to new space.
// It does not expect to encounter pointers to dead objects.
class PointersUpdatingVisitor: public ObjectVisitor {
 public:
  explicit PointersUpdatingVisitor(Heap* heap) : heap_(heap) { }

  void VisitPointer(Object** p) {
    UpdatePointer(p);
  }

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) UpdatePointer(p);
  }

  void VisitEmbeddedPointer(RelocInfo* rinfo) {
    ASSERT(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
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
    ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Object* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    Object* old_target = target;
    VisitPointer(&target);
    if (target != old_target) {
      rinfo->set_target_address(Code::cast(target)->instruction_start());
    }
  }

  void VisitCodeAgeSequence(RelocInfo* rinfo) {
    ASSERT(RelocInfo::IsCodeAgeSequence(rinfo->rmode()));
    Object* stub = rinfo->code_age_stub();
    ASSERT(stub != NULL);
    VisitPointer(&stub);
    if (stub != rinfo->code_age_stub()) {
      rinfo->set_code_age_stub(Code::cast(stub));
    }
  }

  void VisitDebugTarget(RelocInfo* rinfo) {
    ASSERT((RelocInfo::IsJSReturn(rinfo->rmode()) &&
            rinfo->IsPatchedReturnSequence()) ||
           (RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
            rinfo->IsPatchedDebugBreakSlotSequence()));
    Object* target = Code::GetCodeFromTargetAddress(rinfo->call_address());
    VisitPointer(&target);
    rinfo->set_call_address(Code::cast(target)->instruction_start());
  }

  static inline void UpdateSlot(Heap* heap, Object** slot) {
    Object* obj = *slot;

    if (!obj->IsHeapObject()) return;

    HeapObject* heap_obj = HeapObject::cast(obj);

    MapWord map_word = heap_obj->map_word();
    if (map_word.IsForwardingAddress()) {
      ASSERT(heap->InFromSpace(heap_obj) ||
             MarkCompactCollector::IsOnEvacuationCandidate(heap_obj));
      HeapObject* target = map_word.ToForwardingAddress();
      *slot = target;
      ASSERT(!heap->InFromSpace(target) &&
             !MarkCompactCollector::IsOnEvacuationCandidate(target));
    }
  }

 private:
  inline void UpdatePointer(Object** p) {
    UpdateSlot(heap_, p);
  }

  Heap* heap_;
};


static void UpdatePointer(HeapObject** p, HeapObject* object) {
  ASSERT(*p == object);

  Address old_addr = object->address();

  Address new_addr = Memory::Address_at(old_addr);

  // The new space sweep will overwrite the map word of dead objects
  // with NULL. In this case we do not need to transfer this entry to
  // the store buffer which we are rebuilding.
  if (new_addr != NULL) {
    *p = HeapObject::FromAddress(new_addr);
  } else {
    // We have to zap this pointer, because the store buffer may overflow later,
    // and then we have to scan the entire heap and we don't want to find
    // spurious newspace pointers in the old space.
    // TODO(mstarzinger): This was changed to a sentinel value to track down
    // rare crashes, change it back to Smi::FromInt(0) later.
    *p = reinterpret_cast<HeapObject*>(Smi::FromInt(0x0f100d00 >> 1));  // flood
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
  // TODO(hpayer): Replace that check with an assert.
  CHECK(object_size <= Page::kMaxNonCodeHeapObjectSize);

  OldSpace* target_space = heap()->TargetSpace(object);

  ASSERT(target_space == heap()->old_pointer_space() ||
         target_space == heap()->old_data_space());
  Object* result;
  MaybeObject* maybe_result = target_space->AllocateRaw(object_size);
  if (maybe_result->ToObject(&result)) {
    HeapObject* target = HeapObject::cast(result);
    MigrateObject(target->address(),
                  object->address(),
                  object_size,
                  target_space->identity());
    heap()->mark_compact_collector()->tracer()->
        increment_promoted_objects_size(object_size);
    return true;
  }

  return false;
}


void MarkCompactCollector::EvacuateNewSpace() {
  // There are soft limits in the allocation code, designed trigger a mark
  // sweep collection by failing allocations.  But since we are already in
  // a mark-sweep allocation, there is no sense in trying to trigger one.
  AlwaysAllocateScope scope;
  heap()->CheckNewSpaceExpansionCriteria();

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
    survivors_size += DiscoverAndPromoteBlackObjectsOnPage(new_space, p);
  }

  heap_->IncrementYoungSurvivorsCounter(survivors_size);
  new_space->set_age_mark(new_space->top());
}


void MarkCompactCollector::EvacuateLiveObjectsFromPage(Page* p) {
  AlwaysAllocateScope always_allocate;
  PagedSpace* space = static_cast<PagedSpace*>(p->owner());
  ASSERT(p->IsEvacuationCandidate() && !p->WasSwept());
  p->MarkSweptPrecisely();

  int offsets[16];

  for (MarkBitCellIterator it(p); !it.Done(); it.Advance()) {
    Address cell_base = it.CurrentCellBase();
    MarkBit::CellType* cell = it.CurrentCell();

    if (*cell == 0) continue;

    int live_objects = MarkWordToObjectStarts(*cell, offsets);
    for (int i = 0; i < live_objects; i++) {
      Address object_addr = cell_base + offsets[i] * kPointerSize;
      HeapObject* object = HeapObject::FromAddress(object_addr);
      ASSERT(Marking::IsBlack(Marking::MarkBitFrom(object)));

      int size = object->Size();

      MaybeObject* target = space->AllocateRaw(size);
      if (target->IsFailure()) {
        // OS refused to give us memory.
        V8::FatalProcessOutOfMemory("Evacuation");
        return;
      }

      Object* target_object = target->ToObjectUnchecked();

      MigrateObject(HeapObject::cast(target_object)->address(),
                    object_addr,
                    size,
                    space->identity());
      ASSERT(object->map_word().IsForwardingAddress());
    }

    // Clear marking bits for current cell.
    *cell = 0;
  }
  p->ResetLiveBytes();
}


void MarkCompactCollector::EvacuatePages() {
  int npages = evacuation_candidates_.length();
  for (int i = 0; i < npages; i++) {
    Page* p = evacuation_candidates_[i];
    ASSERT(p->IsEvacuationCandidate() ||
           p->IsFlagSet(Page::RESCAN_ON_EVACUATION));
    if (p->IsEvacuationCandidate()) {
      // During compaction we might have to request a new page.
      // Check that space still have room for that.
      if (static_cast<PagedSpace*>(p->owner())->CanExpand()) {
        EvacuateLiveObjectsFromPage(p);
      } else {
        // Without room for expansion evacuation is not guaranteed to succeed.
        // Pessimistically abandon unevacuated pages.
        for (int j = i; j < npages; j++) {
          Page* page = evacuation_candidates_[j];
          slots_buffer_allocator_.DeallocateChain(page->slots_buffer_address());
          page->ClearEvacuationCandidate();
          page->SetFlag(Page::RESCAN_ON_EVACUATION);
          page->InsertAfter(static_cast<PagedSpace*>(page->owner())->anchor());
        }
        return;
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


static inline void UpdateSlot(Isolate* isolate,
                              ObjectVisitor* v,
                              SlotsBuffer::SlotType slot_type,
                              Address addr) {
  switch (slot_type) {
    case SlotsBuffer::CODE_TARGET_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::CODE_TARGET, 0, NULL);
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
    default:
      UNREACHABLE();
      break;
  }
}


enum SweepingMode {
  SWEEP_ONLY,
  SWEEP_AND_VISIT_LIVE_OBJECTS
};


enum SkipListRebuildingMode {
  REBUILD_SKIP_LIST,
  IGNORE_SKIP_LIST
};


// Sweep a space precisely.  After this has been done the space can
// be iterated precisely, hitting only the live objects.  Code space
// is always swept precisely because we want to be able to iterate
// over it.  Map space is swept precisely, because it is not compacted.
// Slots in live objects pointing into evacuation candidates are updated
// if requested.
template<SweepingMode sweeping_mode, SkipListRebuildingMode skip_list_mode>
static void SweepPrecisely(PagedSpace* space,
                           Page* p,
                           ObjectVisitor* v) {
  ASSERT(!p->IsEvacuationCandidate() && !p->WasSwept());
  ASSERT_EQ(skip_list_mode == REBUILD_SKIP_LIST,
            space->identity() == CODE_SPACE);
  ASSERT((p->skip_list() == NULL) || (skip_list_mode == REBUILD_SKIP_LIST));

  double start_time = 0.0;
  if (FLAG_print_cumulative_gc_stat) {
    start_time = OS::TimeCurrentMillis();
  }

  p->MarkSweptPrecisely();

  Address free_start = p->area_start();
  ASSERT(reinterpret_cast<intptr_t>(free_start) % (32 * kPointerSize) == 0);
  int offsets[16];

  SkipList* skip_list = p->skip_list();
  int curr_region = -1;
  if ((skip_list_mode == REBUILD_SKIP_LIST) && skip_list) {
    skip_list->Clear();
  }

  for (MarkBitCellIterator it(p); !it.Done(); it.Advance()) {
    Address cell_base = it.CurrentCellBase();
    MarkBit::CellType* cell = it.CurrentCell();
    int live_objects = MarkWordToObjectStarts(*cell, offsets);
    int live_index = 0;
    for ( ; live_objects != 0; live_objects--) {
      Address free_end = cell_base + offsets[live_index++] * kPointerSize;
      if (free_end != free_start) {
        space->Free(free_start, static_cast<int>(free_end - free_start));
#ifdef ENABLE_GDB_JIT_INTERFACE
        if (FLAG_gdbjit && space->identity() == CODE_SPACE) {
          GDBJITInterface::RemoveCodeRange(free_start, free_end);
        }
#endif
      }
      HeapObject* live_object = HeapObject::FromAddress(free_end);
      ASSERT(Marking::IsBlack(Marking::MarkBitFrom(live_object)));
      Map* map = live_object->map();
      int size = live_object->SizeFromMap(map);
      if (sweeping_mode == SWEEP_AND_VISIT_LIVE_OBJECTS) {
        live_object->IterateBody(map->instance_type(), size, v);
      }
      if ((skip_list_mode == REBUILD_SKIP_LIST) && skip_list != NULL) {
        int new_region_start =
            SkipList::RegionNumber(free_end);
        int new_region_end =
            SkipList::RegionNumber(free_end + size - kPointerSize);
        if (new_region_start != curr_region ||
            new_region_end != curr_region) {
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
    space->Free(free_start, static_cast<int>(p->area_end() - free_start));
#ifdef ENABLE_GDB_JIT_INTERFACE
    if (FLAG_gdbjit && space->identity() == CODE_SPACE) {
      GDBJITInterface::RemoveCodeRange(free_start, p->area_end());
    }
#endif
  }
  p->ResetLiveBytes();
  if (FLAG_print_cumulative_gc_stat) {
    space->heap()->AddSweepingTime(OS::TimeCurrentMillis() - start_time);
  }
}


static bool SetMarkBitsUnderInvalidatedCode(Code* code, bool value) {
  Page* p = Page::FromAddress(code->address());

  if (p->IsEvacuationCandidate() ||
      p->IsFlagSet(Page::RESCAN_ON_EVACUATION)) {
    return false;
  }

  Address code_start = code->address();
  Address code_end = code_start + code->Size();

  uint32_t start_index = MemoryChunk::FastAddressToMarkbitIndex(code_start);
  uint32_t end_index =
      MemoryChunk::FastAddressToMarkbitIndex(code_end - kPointerSize);

  Bitmap* b = p->markbits();

  MarkBit start_mark_bit = b->MarkBitFromIndex(start_index);
  MarkBit end_mark_bit = b->MarkBitFromIndex(end_index);

  MarkBit::CellType* start_cell = start_mark_bit.cell();
  MarkBit::CellType* end_cell = end_mark_bit.cell();

  if (value) {
    MarkBit::CellType start_mask = ~(start_mark_bit.mask() - 1);
    MarkBit::CellType end_mask = (end_mark_bit.mask() << 1) - 1;

    if (start_cell == end_cell) {
      *start_cell |= start_mask & end_mask;
    } else {
      *start_cell |= start_mask;
      for (MarkBit::CellType* cell = start_cell + 1; cell < end_cell; cell++) {
        *cell = ~0;
      }
      *end_cell |= end_mask;
    }
  } else {
    for (MarkBit::CellType* cell = start_cell ; cell <= end_cell; cell++) {
      *cell = 0;
    }
  }

  return true;
}


static bool IsOnInvalidatedCodeObject(Address addr) {
  // We did not record any slots in large objects thus
  // we can safely go to the page from the slot address.
  Page* p = Page::FromAddress(addr);

  // First check owner's identity because old pointer and old data spaces
  // are swept lazily and might still have non-zero mark-bits on some
  // pages.
  if (p->owner()->identity() != CODE_SPACE) return false;

  // In code space only bits on evacuation candidates (but we don't record
  // any slots on them) and under invalidated code objects are non-zero.
  MarkBit mark_bit =
      p->markbits()->MarkBitFromIndex(Page::FastAddressToMarkbitIndex(addr));

  return mark_bit.Get();
}


void MarkCompactCollector::InvalidateCode(Code* code) {
  if (heap_->incremental_marking()->IsCompacting() &&
      !ShouldSkipEvacuationSlotRecording(code)) {
    ASSERT(compacting_);

    // If the object is white than no slots were recorded on it yet.
    MarkBit mark_bit = Marking::MarkBitFrom(code);
    if (Marking::IsWhite(mark_bit)) return;

    invalidated_code_.Add(code);
  }
}


// Return true if the given code is deoptimized or will be deoptimized.
bool MarkCompactCollector::WillBeDeoptimized(Code* code) {
  return code->marked_for_deoptimization();
}


bool MarkCompactCollector::MarkInvalidatedCode() {
  bool code_marked = false;

  int length = invalidated_code_.length();
  for (int i = 0; i < length; i++) {
    Code* code = invalidated_code_[i];

    if (SetMarkBitsUnderInvalidatedCode(code, true)) {
      code_marked = true;
    }
  }

  return code_marked;
}


void MarkCompactCollector::RemoveDeadInvalidatedCode() {
  int length = invalidated_code_.length();
  for (int i = 0; i < length; i++) {
    if (!IsMarked(invalidated_code_[i])) invalidated_code_[i] = NULL;
  }
}


void MarkCompactCollector::ProcessInvalidatedCode(ObjectVisitor* visitor) {
  int length = invalidated_code_.length();
  for (int i = 0; i < length; i++) {
    Code* code = invalidated_code_[i];
    if (code != NULL) {
      code->Iterate(visitor);
      SetMarkBitsUnderInvalidatedCode(code, false);
    }
  }
  invalidated_code_.Rewind(0);
}


void MarkCompactCollector::EvacuateNewSpaceAndCandidates() {
  Heap::RelocationLock relocation_lock(heap());

  bool code_slots_filtering_required;
  { GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_SWEEP_NEWSPACE);
    code_slots_filtering_required = MarkInvalidatedCode();
    EvacuateNewSpace();
  }

  { GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_EVACUATE_PAGES);
    EvacuatePages();
  }

  // Second pass: find pointers to new space and update them.
  PointersUpdatingVisitor updating_visitor(heap());

  { GCTracer::Scope gc_scope(tracer_,
                             GCTracer::Scope::MC_UPDATE_NEW_TO_NEW_POINTERS);
    // Update pointers in to space.
    SemiSpaceIterator to_it(heap()->new_space()->bottom(),
                            heap()->new_space()->top());
    for (HeapObject* object = to_it.Next();
         object != NULL;
         object = to_it.Next()) {
      Map* map = object->map();
      object->IterateBody(map->instance_type(),
                          object->SizeFromMap(map),
                          &updating_visitor);
    }
  }

  { GCTracer::Scope gc_scope(tracer_,
                             GCTracer::Scope::MC_UPDATE_ROOT_TO_NEW_POINTERS);
    // Update roots.
    heap_->IterateRoots(&updating_visitor, VISIT_ALL_IN_SWEEP_NEWSPACE);
  }

  { GCTracer::Scope gc_scope(tracer_,
                             GCTracer::Scope::MC_UPDATE_OLD_TO_NEW_POINTERS);
    StoreBufferRebuildScope scope(heap_,
                                  heap_->store_buffer(),
                                  &Heap::ScavengeStoreBufferCallback);
    heap_->store_buffer()->IteratePointersToNewSpaceAndClearMaps(
        &UpdatePointer);
  }

  { GCTracer::Scope gc_scope(tracer_,
                             GCTracer::Scope::MC_UPDATE_POINTERS_TO_EVACUATED);
    SlotsBuffer::UpdateSlotsRecordedIn(heap_,
                                       migration_slots_buffer_,
                                       code_slots_filtering_required);
    if (FLAG_trace_fragmentation) {
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
  { GCTracer::Scope gc_scope(
      tracer_, GCTracer::Scope::MC_UPDATE_POINTERS_BETWEEN_EVACUATED);
    for (int i = 0; i < npages; i++) {
      Page* p = evacuation_candidates_[i];
      ASSERT(p->IsEvacuationCandidate() ||
             p->IsFlagSet(Page::RESCAN_ON_EVACUATION));

      if (p->IsEvacuationCandidate()) {
        SlotsBuffer::UpdateSlotsRecordedIn(heap_,
                                           p->slots_buffer(),
                                           code_slots_filtering_required);
        if (FLAG_trace_fragmentation) {
          PrintF("  page %p slots buffer: %d\n",
                 reinterpret_cast<void*>(p),
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
          case OLD_DATA_SPACE:
            SweepConservatively<SWEEP_SEQUENTIALLY>(space, NULL, p);
            break;
          case OLD_POINTER_SPACE:
            SweepPrecisely<SWEEP_AND_VISIT_LIVE_OBJECTS, IGNORE_SKIP_LIST>(
                space, p, &updating_visitor);
            break;
          case CODE_SPACE:
            SweepPrecisely<SWEEP_AND_VISIT_LIVE_OBJECTS, REBUILD_SKIP_LIST>(
                space, p, &updating_visitor);
            break;
          default:
            UNREACHABLE();
            break;
        }
      }
    }
  }

  GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_UPDATE_MISC_POINTERS);

  // Update pointers from cells.
  HeapObjectIterator cell_iterator(heap_->cell_space());
  for (HeapObject* cell = cell_iterator.Next();
       cell != NULL;
       cell = cell_iterator.Next()) {
    if (cell->IsCell()) {
      Cell::BodyDescriptor::IterateBody(cell, &updating_visitor);
    }
  }

  HeapObjectIterator js_global_property_cell_iterator(
      heap_->property_cell_space());
  for (HeapObject* cell = js_global_property_cell_iterator.Next();
       cell != NULL;
       cell = js_global_property_cell_iterator.Next()) {
    if (cell->IsPropertyCell()) {
      PropertyCell::BodyDescriptor::IterateBody(cell, &updating_visitor);
    }
  }

  // Update the head of the native contexts list in the heap.
  updating_visitor.VisitPointer(heap_->native_contexts_list_address());

  heap_->string_table()->Iterate(&updating_visitor);

  // Update pointers from external string table.
  heap_->UpdateReferencesInExternalStringTable(
      &UpdateReferenceInExternalStringTableEntry);

  if (!FLAG_watch_ic_patching) {
    // Update JSFunction pointers from the runtime profiler.
    heap()->isolate()->runtime_profiler()->UpdateSamplesAfterCompact(
        &updating_visitor);
  }

  EvacuationWeakObjectRetainer evacuation_object_retainer;
  heap()->ProcessWeakReferences(&evacuation_object_retainer);

  // Visit invalidated code (we ignored all slots on it) and clear mark-bits
  // under it.
  ProcessInvalidatedCode(&updating_visitor);

  heap_->isolate()->inner_pointer_to_code_cache()->Flush();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    VerifyEvacuation(heap_);
  }
#endif

  slots_buffer_allocator_.DeallocateChain(&migration_slots_buffer_);
  ASSERT(migration_slots_buffer_ == NULL);
}


void MarkCompactCollector::UnlinkEvacuationCandidates() {
  int npages = evacuation_candidates_.length();
  for (int i = 0; i < npages; i++) {
    Page* p = evacuation_candidates_[i];
    if (!p->IsEvacuationCandidate()) continue;
    p->Unlink();
    p->ClearSweptPrecisely();
    p->ClearSweptConservatively();
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
    space->ReleasePage(p, false);
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
  0, _, _, _, _,  // 0
  1, 0, _, _, _,  // 1
  1, 1, _, _, _,  // 2
  X, _, _, _, _,  // 3
  1, 2, _, _, _,  // 4
  2, 0, 2, _, _,  // 5
  X, _, _, _, _,  // 6
  X, _, _, _, _,  // 7
  1, 3, _, _, _,  // 8
  2, 0, 3, _, _,  // 9
  2, 1, 3, _, _,  // 10
  X, _, _, _, _,  // 11
  X, _, _, _, _,  // 12
  X, _, _, _, _,  // 13
  X, _, _, _, _,  // 14
  X, _, _, _, _,  // 15
  1, 4, _, _, _,  // 16
  2, 0, 4, _, _,  // 17
  2, 1, 4, _, _,  // 18
  X, _, _, _, _,  // 19
  2, 2, 4, _, _,  // 20
  3, 0, 2, 4, _,  // 21
  X, _, _, _, _,  // 22
  X, _, _, _, _,  // 23
  X, _, _, _, _,  // 24
  X, _, _, _, _,  // 25
  X, _, _, _, _,  // 26
  X, _, _, _, _,  // 27
  X, _, _, _, _,  // 28
  X, _, _, _, _,  // 29
  X, _, _, _, _,  // 30
  X, _, _, _, _,  // 31
  1, 5, _, _, _,  // 32
  2, 0, 5, _, _,  // 33
  2, 1, 5, _, _,  // 34
  X, _, _, _, _,  // 35
  2, 2, 5, _, _,  // 36
  3, 0, 2, 5, _,  // 37
  X, _, _, _, _,  // 38
  X, _, _, _, _,  // 39
  2, 3, 5, _, _,  // 40
  3, 0, 3, 5, _,  // 41
  3, 1, 3, 5, _,  // 42
  X, _, _, _, _,  // 43
  X, _, _, _, _,  // 44
  X, _, _, _, _,  // 45
  X, _, _, _, _,  // 46
  X, _, _, _, _,  // 47
  X, _, _, _, _,  // 48
  X, _, _, _, _,  // 49
  X, _, _, _, _,  // 50
  X, _, _, _, _,  // 51
  X, _, _, _, _,  // 52
  X, _, _, _, _,  // 53
  X, _, _, _, _,  // 54
  X, _, _, _, _,  // 55
  X, _, _, _, _,  // 56
  X, _, _, _, _,  // 57
  X, _, _, _, _,  // 58
  X, _, _, _, _,  // 59
  X, _, _, _, _,  // 60
  X, _, _, _, _,  // 61
  X, _, _, _, _,  // 62
  X, _, _, _, _,  // 63
  1, 6, _, _, _,  // 64
  2, 0, 6, _, _,  // 65
  2, 1, 6, _, _,  // 66
  X, _, _, _, _,  // 67
  2, 2, 6, _, _,  // 68
  3, 0, 2, 6, _,  // 69
  X, _, _, _, _,  // 70
  X, _, _, _, _,  // 71
  2, 3, 6, _, _,  // 72
  3, 0, 3, 6, _,  // 73
  3, 1, 3, 6, _,  // 74
  X, _, _, _, _,  // 75
  X, _, _, _, _,  // 76
  X, _, _, _, _,  // 77
  X, _, _, _, _,  // 78
  X, _, _, _, _,  // 79
  2, 4, 6, _, _,  // 80
  3, 0, 4, 6, _,  // 81
  3, 1, 4, 6, _,  // 82
  X, _, _, _, _,  // 83
  3, 2, 4, 6, _,  // 84
  4, 0, 2, 4, 6,  // 85
  X, _, _, _, _,  // 86
  X, _, _, _, _,  // 87
  X, _, _, _, _,  // 88
  X, _, _, _, _,  // 89
  X, _, _, _, _,  // 90
  X, _, _, _, _,  // 91
  X, _, _, _, _,  // 92
  X, _, _, _, _,  // 93
  X, _, _, _, _,  // 94
  X, _, _, _, _,  // 95
  X, _, _, _, _,  // 96
  X, _, _, _, _,  // 97
  X, _, _, _, _,  // 98
  X, _, _, _, _,  // 99
  X, _, _, _, _,  // 100
  X, _, _, _, _,  // 101
  X, _, _, _, _,  // 102
  X, _, _, _, _,  // 103
  X, _, _, _, _,  // 104
  X, _, _, _, _,  // 105
  X, _, _, _, _,  // 106
  X, _, _, _, _,  // 107
  X, _, _, _, _,  // 108
  X, _, _, _, _,  // 109
  X, _, _, _, _,  // 110
  X, _, _, _, _,  // 111
  X, _, _, _, _,  // 112
  X, _, _, _, _,  // 113
  X, _, _, _, _,  // 114
  X, _, _, _, _,  // 115
  X, _, _, _, _,  // 116
  X, _, _, _, _,  // 117
  X, _, _, _, _,  // 118
  X, _, _, _, _,  // 119
  X, _, _, _, _,  // 120
  X, _, _, _, _,  // 121
  X, _, _, _, _,  // 122
  X, _, _, _, _,  // 123
  X, _, _, _, _,  // 124
  X, _, _, _, _,  // 125
  X, _, _, _, _,  // 126
  X, _, _, _, _,  // 127
  1, 7, _, _, _,  // 128
  2, 0, 7, _, _,  // 129
  2, 1, 7, _, _,  // 130
  X, _, _, _, _,  // 131
  2, 2, 7, _, _,  // 132
  3, 0, 2, 7, _,  // 133
  X, _, _, _, _,  // 134
  X, _, _, _, _,  // 135
  2, 3, 7, _, _,  // 136
  3, 0, 3, 7, _,  // 137
  3, 1, 3, 7, _,  // 138
  X, _, _, _, _,  // 139
  X, _, _, _, _,  // 140
  X, _, _, _, _,  // 141
  X, _, _, _, _,  // 142
  X, _, _, _, _,  // 143
  2, 4, 7, _, _,  // 144
  3, 0, 4, 7, _,  // 145
  3, 1, 4, 7, _,  // 146
  X, _, _, _, _,  // 147
  3, 2, 4, 7, _,  // 148
  4, 0, 2, 4, 7,  // 149
  X, _, _, _, _,  // 150
  X, _, _, _, _,  // 151
  X, _, _, _, _,  // 152
  X, _, _, _, _,  // 153
  X, _, _, _, _,  // 154
  X, _, _, _, _,  // 155
  X, _, _, _, _,  // 156
  X, _, _, _, _,  // 157
  X, _, _, _, _,  // 158
  X, _, _, _, _,  // 159
  2, 5, 7, _, _,  // 160
  3, 0, 5, 7, _,  // 161
  3, 1, 5, 7, _,  // 162
  X, _, _, _, _,  // 163
  3, 2, 5, 7, _,  // 164
  4, 0, 2, 5, 7,  // 165
  X, _, _, _, _,  // 166
  X, _, _, _, _,  // 167
  3, 3, 5, 7, _,  // 168
  4, 0, 3, 5, 7,  // 169
  4, 1, 3, 5, 7   // 170
};
#undef _
#undef X


// Takes a word of mark bits.  Returns the number of objects that start in the
// range.  Puts the offsets of the words in the supplied array.
static inline int MarkWordToObjectStarts(uint32_t mark_bits, int* starts) {
  int objects = 0;
  int offset = 0;

  // No consecutive 1 bits.
  ASSERT((mark_bits & 0x180) != 0x180);
  ASSERT((mark_bits & 0x18000) != 0x18000);
  ASSERT((mark_bits & 0x1800000) != 0x1800000);

  while (mark_bits != 0) {
    int byte = (mark_bits & 0xff);
    mark_bits >>= 8;
    if (byte != 0) {
      ASSERT(byte < kStartTableLines);  // No consecutive 1 bits.
      char* table = kStartTable + byte * kStartTableEntriesPerLine;
      int objects_in_these_8_words = table[0];
      ASSERT(objects_in_these_8_words != kStartTableInvalidLine);
      ASSERT(objects_in_these_8_words < kStartTableEntriesPerLine);
      for (int i = 0; i < objects_in_these_8_words; i++) {
        starts[objects++] = offset + table[1 + i];
      }
    }
    offset += 8;
  }
  return objects;
}


static inline Address DigestFreeStart(Address approximate_free_start,
                                      uint32_t free_start_cell) {
  ASSERT(free_start_cell != 0);

  // No consecutive 1 bits.
  ASSERT((free_start_cell & (free_start_cell << 1)) == 0);

  int offsets[16];
  uint32_t cell = free_start_cell;
  int offset_of_last_live;
  if ((cell & 0x80000000u) != 0) {
    // This case would overflow below.
    offset_of_last_live = 31;
  } else {
    // Remove all but one bit, the most significant.  This is an optimization
    // that may or may not be worthwhile.
    cell |= cell >> 16;
    cell |= cell >> 8;
    cell |= cell >> 4;
    cell |= cell >> 2;
    cell |= cell >> 1;
    cell = (cell + 1) >> 1;
    int live_objects = MarkWordToObjectStarts(cell, offsets);
    ASSERT(live_objects == 1);
    offset_of_last_live = offsets[live_objects - 1];
  }
  Address last_live_start =
      approximate_free_start + offset_of_last_live * kPointerSize;
  HeapObject* last_live = HeapObject::FromAddress(last_live_start);
  Address free_start = last_live_start + last_live->Size();
  return free_start;
}


static inline Address StartOfLiveObject(Address block_address, uint32_t cell) {
  ASSERT(cell != 0);

  // No consecutive 1 bits.
  ASSERT((cell & (cell << 1)) == 0);

  int offsets[16];
  if (cell == 0x80000000u) {  // Avoid overflow below.
    return block_address + 31 * kPointerSize;
  }
  uint32_t first_set_bit = ((cell ^ (cell - 1)) + 1) >> 1;
  ASSERT((first_set_bit & cell) == first_set_bit);
  int live_objects = MarkWordToObjectStarts(first_set_bit, offsets);
  ASSERT(live_objects == 1);
  USE(live_objects);
  return block_address + offsets[0] * kPointerSize;
}


template<MarkCompactCollector::SweepingParallelism mode>
static intptr_t Free(PagedSpace* space,
                     FreeList* free_list,
                     Address start,
                     int size) {
  if (mode == MarkCompactCollector::SWEEP_SEQUENTIALLY) {
    return space->Free(start, size);
  } else {
    return size - free_list->Free(start, size);
  }
}


// Force instantiation of templatized SweepConservatively method for
// SWEEP_SEQUENTIALLY mode.
template intptr_t MarkCompactCollector::
    SweepConservatively<MarkCompactCollector::SWEEP_SEQUENTIALLY>(
        PagedSpace*, FreeList*, Page*);


// Force instantiation of templatized SweepConservatively method for
// SWEEP_IN_PARALLEL mode.
template intptr_t MarkCompactCollector::
    SweepConservatively<MarkCompactCollector::SWEEP_IN_PARALLEL>(
        PagedSpace*, FreeList*, Page*);


// Sweeps a space conservatively.  After this has been done the larger free
// spaces have been put on the free list and the smaller ones have been
// ignored and left untouched.  A free space is always either ignored or put
// on the free list, never split up into two parts.  This is important
// because it means that any FreeSpace maps left actually describe a region of
// memory that can be ignored when scanning.  Dead objects other than free
// spaces will not contain the free space map.
template<MarkCompactCollector::SweepingParallelism mode>
intptr_t MarkCompactCollector::SweepConservatively(PagedSpace* space,
                                                   FreeList* free_list,
                                                   Page* p) {
  ASSERT(!p->IsEvacuationCandidate() && !p->WasSwept());
  ASSERT((mode == MarkCompactCollector::SWEEP_IN_PARALLEL &&
         free_list != NULL) ||
         (mode == MarkCompactCollector::SWEEP_SEQUENTIALLY &&
         free_list == NULL));

  p->MarkSweptConservatively();

  intptr_t freed_bytes = 0;
  size_t size = 0;

  // Skip over all the dead objects at the start of the page and mark them free.
  Address cell_base = 0;
  MarkBit::CellType* cell = NULL;
  MarkBitCellIterator it(p);
  for (; !it.Done(); it.Advance()) {
    cell_base = it.CurrentCellBase();
    cell = it.CurrentCell();
    if (*cell != 0) break;
  }

  if (it.Done()) {
    size = p->area_end() - p->area_start();
    freed_bytes += Free<mode>(space, free_list, p->area_start(),
                              static_cast<int>(size));
    ASSERT_EQ(0, p->LiveBytes());
    return freed_bytes;
  }

  // Grow the size of the start-of-page free space a little to get up to the
  // first live object.
  Address free_end = StartOfLiveObject(cell_base, *cell);
  // Free the first free space.
  size = free_end - p->area_start();
  freed_bytes += Free<mode>(space, free_list, p->area_start(),
                            static_cast<int>(size));

  // The start of the current free area is represented in undigested form by
  // the address of the last 32-word section that contained a live object and
  // the marking bitmap for that cell, which describes where the live object
  // started.  Unless we find a large free space in the bitmap we will not
  // digest this pair into a real address.  We start the iteration here at the
  // first word in the marking bit map that indicates a live object.
  Address free_start = cell_base;
  MarkBit::CellType free_start_cell = *cell;

  for (; !it.Done(); it.Advance()) {
    cell_base = it.CurrentCellBase();
    cell = it.CurrentCell();
    if (*cell != 0) {
      // We have a live object.  Check approximately whether it is more than 32
      // words since the last live object.
      if (cell_base - free_start > 32 * kPointerSize) {
        free_start = DigestFreeStart(free_start, free_start_cell);
        if (cell_base - free_start > 32 * kPointerSize) {
          // Now that we know the exact start of the free space it still looks
          // like we have a large enough free space to be worth bothering with.
          // so now we need to find the start of the first live object at the
          // end of the free space.
          free_end = StartOfLiveObject(cell_base, *cell);
          freed_bytes += Free<mode>(space, free_list, free_start,
                                    static_cast<int>(free_end - free_start));
        }
      }
      // Update our undigested record of where the current free area started.
      free_start = cell_base;
      free_start_cell = *cell;
      // Clear marking bits for current cell.
      *cell = 0;
    }
  }

  // Handle the free space at the end of the page.
  if (cell_base - free_start > 32 * kPointerSize) {
    free_start = DigestFreeStart(free_start, free_start_cell);
    freed_bytes += Free<mode>(space, free_list, free_start,
                              static_cast<int>(p->area_end() - free_start));
  }

  p->ResetLiveBytes();
  return freed_bytes;
}


void MarkCompactCollector::SweepInParallel(PagedSpace* space,
                                           FreeList* private_free_list,
                                           FreeList* free_list) {
  PageIterator it(space);
  while (it.has_next()) {
    Page* p = it.next();

    if (p->TryParallelSweeping()) {
      SweepConservatively<SWEEP_IN_PARALLEL>(space, private_free_list, p);
      free_list->Concatenate(private_free_list);
    }
  }
}


void MarkCompactCollector::SweepSpace(PagedSpace* space, SweeperType sweeper) {
  space->set_was_swept_conservatively(sweeper == CONSERVATIVE ||
                                      sweeper == LAZY_CONSERVATIVE ||
                                      sweeper == PARALLEL_CONSERVATIVE ||
                                      sweeper == CONCURRENT_CONSERVATIVE);
  space->ClearStats();

  PageIterator it(space);

  int pages_swept = 0;
  bool lazy_sweeping_active = false;
  bool unused_page_present = false;
  bool parallel_sweeping_active = false;

  while (it.has_next()) {
    Page* p = it.next();

    ASSERT(p->parallel_sweeping() == 0);
    ASSERT(!p->IsEvacuationCandidate());

    // Clear sweeping flags indicating that marking bits are still intact.
    p->ClearSweptPrecisely();
    p->ClearSweptConservatively();

    if (p->IsFlagSet(Page::RESCAN_ON_EVACUATION)) {
      // Will be processed in EvacuateNewSpaceAndCandidates.
      ASSERT(evacuation_candidates_.length() > 0);
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
        space->ReleasePage(p, true);
        continue;
      }
      unused_page_present = true;
    }

    switch (sweeper) {
      case CONSERVATIVE: {
        if (FLAG_gc_verbose) {
          PrintF("Sweeping 0x%" V8PRIxPTR " conservatively.\n",
                 reinterpret_cast<intptr_t>(p));
        }
        SweepConservatively<SWEEP_SEQUENTIALLY>(space, NULL, p);
        pages_swept++;
        break;
      }
      case LAZY_CONSERVATIVE: {
        if (lazy_sweeping_active) {
          if (FLAG_gc_verbose) {
            PrintF("Sweeping 0x%" V8PRIxPTR " lazily postponed.\n",
                   reinterpret_cast<intptr_t>(p));
          }
          space->IncreaseUnsweptFreeBytes(p);
        } else {
          if (FLAG_gc_verbose) {
            PrintF("Sweeping 0x%" V8PRIxPTR " conservatively.\n",
                   reinterpret_cast<intptr_t>(p));
          }
          SweepConservatively<SWEEP_SEQUENTIALLY>(space, NULL, p);
          pages_swept++;
          space->SetPagesToSweep(p->next_page());
          lazy_sweeping_active = true;
        }
        break;
      }
      case CONCURRENT_CONSERVATIVE:
      case PARALLEL_CONSERVATIVE: {
        if (!parallel_sweeping_active) {
          if (FLAG_gc_verbose) {
            PrintF("Sweeping 0x%" V8PRIxPTR " conservatively.\n",
                   reinterpret_cast<intptr_t>(p));
          }
          SweepConservatively<SWEEP_SEQUENTIALLY>(space, NULL, p);
          pages_swept++;
          parallel_sweeping_active = true;
        } else {
          if (FLAG_gc_verbose) {
            PrintF("Sweeping 0x%" V8PRIxPTR " conservatively in parallel.\n",
                   reinterpret_cast<intptr_t>(p));
          }
          p->set_parallel_sweeping(1);
          space->IncreaseUnsweptFreeBytes(p);
        }
        break;
      }
      case PRECISE: {
        if (FLAG_gc_verbose) {
          PrintF("Sweeping 0x%" V8PRIxPTR " precisely.\n",
                 reinterpret_cast<intptr_t>(p));
        }
        if (space->identity() == CODE_SPACE) {
          SweepPrecisely<SWEEP_ONLY, REBUILD_SKIP_LIST>(space, p, NULL);
        } else {
          SweepPrecisely<SWEEP_ONLY, IGNORE_SKIP_LIST>(space, p, NULL);
        }
        pages_swept++;
        break;
      }
      default: {
        UNREACHABLE();
      }
    }
  }

  if (FLAG_gc_verbose) {
    PrintF("SweepSpace: %s (%d pages swept)\n",
           AllocationSpaceName(space->identity()),
           pages_swept);
  }

  // Give pages that are queued to be freed back to the OS.
  heap()->FreeQueuedChunks();
}


void MarkCompactCollector::SweepSpaces() {
  GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_SWEEP);
#ifdef DEBUG
  state_ = SWEEP_SPACES;
#endif
  SweeperType how_to_sweep =
      FLAG_lazy_sweeping ? LAZY_CONSERVATIVE : CONSERVATIVE;
  if (FLAG_parallel_sweeping) how_to_sweep = PARALLEL_CONSERVATIVE;
  if (FLAG_concurrent_sweeping) how_to_sweep = CONCURRENT_CONSERVATIVE;
  if (FLAG_expose_gc) how_to_sweep = CONSERVATIVE;
  if (sweep_precisely_) how_to_sweep = PRECISE;

  // Unlink evacuation candidates before sweeper threads access the list of
  // pages to avoid race condition.
  UnlinkEvacuationCandidates();

  // Noncompacting collections simply sweep the spaces to clear the mark
  // bits and free the nonlive blocks (for old and map spaces).  We sweep
  // the map space last because freeing non-live maps overwrites them and
  // the other spaces rely on possibly non-live maps to get the sizes for
  // non-live objects.
  SequentialSweepingScope scope(this);
  SweepSpace(heap()->old_pointer_space(), how_to_sweep);
  SweepSpace(heap()->old_data_space(), how_to_sweep);

  if (how_to_sweep == PARALLEL_CONSERVATIVE ||
      how_to_sweep == CONCURRENT_CONSERVATIVE) {
    // TODO(hpayer): fix race with concurrent sweeper
    StartSweeperThreads();
  }

  if (how_to_sweep == PARALLEL_CONSERVATIVE) {
    WaitUntilSweepingCompleted();
  }

  RemoveDeadInvalidatedCode();
  SweepSpace(heap()->code_space(), PRECISE);

  SweepSpace(heap()->cell_space(), PRECISE);
  SweepSpace(heap()->property_cell_space(), PRECISE);

  EvacuateNewSpaceAndCandidates();

  // ClearNonLiveTransitions depends on precise sweeping of map space to
  // detect whether unmarked map became dead in this collection or in one
  // of the previous ones.
  SweepSpace(heap()->map_space(), PRECISE);

  // Deallocate unmarked objects and clear marked bits for marked objects.
  heap_->lo_space()->FreeUnmarkedObjects();

  // Deallocate evacuated candidate pages.
  ReleaseEvacuationCandidates();
}


void MarkCompactCollector::EnableCodeFlushing(bool enable) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (isolate()->debug()->IsLoaded() ||
      isolate()->debug()->has_break_points()) {
    enable = false;
  }
#endif

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
#ifdef ENABLE_GDB_JIT_INTERFACE
  if (obj->IsCode()) {
    GDBJITInterface::RemoveCode(reinterpret_cast<Code*>(obj));
  }
#endif
  if (obj->IsCode()) {
    PROFILE(isolate, CodeDeleteEvent(obj->address()));
  }
}


Isolate* MarkCompactCollector::isolate() const {
  return heap_->isolate();
}


void MarkCompactCollector::Initialize() {
  MarkCompactMarkingVisitor::Initialize();
  IncrementalMarking::Initialize();
}


bool SlotsBuffer::IsTypedSlot(ObjectSlot slot) {
  return reinterpret_cast<uintptr_t>(slot) < NUMBER_OF_SLOT_TYPES;
}


bool SlotsBuffer::AddTo(SlotsBufferAllocator* allocator,
                        SlotsBuffer** buffer_address,
                        SlotType type,
                        Address addr,
                        AdditionMode mode) {
  SlotsBuffer* buffer = *buffer_address;
  if (buffer == NULL || !buffer->HasSpaceForTypedSlot()) {
    if (mode == FAIL_ON_OVERFLOW && ChainLengthThresholdReached(buffer)) {
      allocator->DeallocateChain(buffer_address);
      return false;
    }
    buffer = allocator->AllocateBuffer(buffer);
    *buffer_address = buffer;
  }
  ASSERT(buffer->HasSpaceForTypedSlot());
  buffer->Add(reinterpret_cast<ObjectSlot>(type));
  buffer->Add(reinterpret_cast<ObjectSlot>(addr));
  return true;
}


static inline SlotsBuffer::SlotType SlotTypeForRMode(RelocInfo::Mode rmode) {
  if (RelocInfo::IsCodeTarget(rmode)) {
    return SlotsBuffer::CODE_TARGET_SLOT;
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
  if (target_page->IsEvacuationCandidate() &&
      (rinfo->host() == NULL ||
       !ShouldSkipEvacuationSlotRecording(rinfo->host()))) {
    if (!SlotsBuffer::AddTo(&slots_buffer_allocator_,
                            target_page->slots_buffer_address(),
                            SlotTypeForRMode(rinfo->rmode()),
                            rinfo->pc(),
                            SlotsBuffer::FAIL_ON_OVERFLOW)) {
      EvictEvacuationCandidate(target_page);
    }
  }
}


void MarkCompactCollector::RecordCodeEntrySlot(Address slot, Code* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  if (target_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(reinterpret_cast<Object**>(slot))) {
    if (!SlotsBuffer::AddTo(&slots_buffer_allocator_,
                            target_page->slots_buffer_address(),
                            SlotsBuffer::CODE_ENTRY_SLOT,
                            slot,
                            SlotsBuffer::FAIL_ON_OVERFLOW)) {
      EvictEvacuationCandidate(target_page);
    }
  }
}


void MarkCompactCollector::RecordCodeTargetPatch(Address pc, Code* target) {
  ASSERT(heap()->gc_state() == Heap::MARK_COMPACT);
  if (is_compacting()) {
    Code* host = isolate()->inner_pointer_to_code_cache()->
        GcSafeFindCodeForInnerPointer(pc);
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
      ASSERT(slot_idx < idx_);
      UpdateSlot(heap->isolate(),
                 &v,
                 DecodeSlotType(slot),
                 reinterpret_cast<Address>(slots_[slot_idx]));
    }
  }
}


void SlotsBuffer::UpdateSlotsWithFilter(Heap* heap) {
  PointersUpdatingVisitor v(heap);

  for (int slot_idx = 0; slot_idx < idx_; ++slot_idx) {
    ObjectSlot slot = slots_[slot_idx];
    if (!IsTypedSlot(slot)) {
      if (!IsOnInvalidatedCodeObject(reinterpret_cast<Address>(slot))) {
        PointersUpdatingVisitor::UpdateSlot(heap, slot);
      }
    } else {
      ++slot_idx;
      ASSERT(slot_idx < idx_);
      Address pc = reinterpret_cast<Address>(slots_[slot_idx]);
      if (!IsOnInvalidatedCodeObject(pc)) {
        UpdateSlot(heap->isolate(),
                   &v,
                   DecodeSlotType(slot),
                   reinterpret_cast<Address>(slots_[slot_idx]));
      }
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


} }  // namespace v8::internal

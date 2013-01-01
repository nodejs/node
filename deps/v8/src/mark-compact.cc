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
#include "deoptimizer.h"
#include "execution.h"
#include "gdb-jit.h"
#include "global-handles.h"
#include "heap-profiler.h"
#include "ic-inl.h"
#include "incremental-marking.h"
#include "liveobjectlist-inl.h"
#include "mark-compact.h"
#include "objects-visiting.h"
#include "objects-visiting-inl.h"
#include "stub-cache.h"

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
      tracer_(NULL),
      migration_slots_buffer_(NULL),
      heap_(NULL),
      code_flusher_(NULL),
      encountered_weak_maps_(NULL) { }


#ifdef VERIFY_HEAP
class VerifyMarkingVisitor: public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        CHECK(HEAP->mark_compact_collector()->IsMarked(object));
      }
    }
  }
};


static void VerifyMarking(Address bottom, Address top) {
  VerifyMarkingVisitor visitor;
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
    VerifyMarking(page->area_start(), limit);
  }
}


static void VerifyMarking(PagedSpace* space) {
  PageIterator it(space);

  while (it.has_next()) {
    Page* p = it.next();
    VerifyMarking(p->area_start(), p->area_end());
  }
}


static void VerifyMarking(Heap* heap) {
  VerifyMarking(heap->old_pointer_space());
  VerifyMarking(heap->old_data_space());
  VerifyMarking(heap->code_space());
  VerifyMarking(heap->cell_space());
  VerifyMarking(heap->map_space());
  VerifyMarking(heap->new_space());

  VerifyMarkingVisitor visitor;

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
          case JS_GLOBAL_PROPERTY_CELL_TYPE:
          case JS_PROXY_TYPE:
          case JS_VALUE_TYPE:
          case TYPE_FEEDBACK_INFO_TYPE:
            object->Iterate(this);
            break;
          case ACCESSOR_INFO_TYPE:
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
  ASSERT(encountered_weak_maps_ == Smi::FromInt(0));

  MarkLiveObjects();
  ASSERT(heap_->incremental_marking()->IsStopped());

  if (FLAG_collect_maps) ClearNonLiveTransitions();

  ClearWeakMaps();

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
  VerifyMarkbitsAreClean(heap_->map_space());
  VerifyMarkbitsAreClean(heap_->new_space());

  LargeObjectIterator it(heap_->lo_space());
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    MarkBit mark_bit = Marking::MarkBitFrom(obj);
    CHECK(Marking::IsWhite(mark_bit));
    CHECK_EQ(0, Page::FromAddress(obj->address())->LiveBytes());
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

  FreeList::SizeStats sizes;
  space->CountFreeListItems(p, &sizes);

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
    PrintF("Estimated over reserved memory: %.1f / %.1f MB (threshold %d)\n",
           static_cast<double>(over_reserved) / MB,
           static_cast<double>(reserved) / MB,
           static_cast<int>(kFreenessThreshold));
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
      if (estimated_release >= ((over_reserved * 3) / 4)) {
        continue;
      }

      intptr_t free_bytes = 0;

      if (!p->WasSwept()) {
        free_bytes = (p->area_size() - p->LiveBytes());
      } else {
        FreeList::SizeStats sizes;
        space->CountFreeListItems(p, &sizes);
        free_bytes = sizes.Total();
      }

      int free_pct = static_cast<int>(free_bytes * 100) / p->area_size();

      if (free_pct >= kFreenessThreshold) {
        estimated_release += 2 * p->area_size() - free_bytes;
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

  PagedSpaces spaces;
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
  heap()->isolate()->stub_cache()->Clear();

  heap()->external_string_table_.CleanUp();
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
      shared->set_code(lazy_compile);
      candidate->set_code(lazy_compile);
    } else if (code == lazy_compile) {
      candidate->set_code(lazy_compile);
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


void CodeFlusher::EvictCandidate(JSFunction* function) {
  ASSERT(!function->next_function_link()->IsUndefined());
  Object* undefined = isolate_->heap()->undefined_value();

  // The function is no longer a candidate, make sure it gets visited
  // again so that previous flushing decisions are revisited.
  isolate_->heap()->incremental_marking()->RecordWrites(function);

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
      }

      candidate = next_candidate;
    }
  }
}


void CodeFlusher::EvictJSFunctionCandidates() {
  Object* undefined = isolate_->heap()->undefined_value();

  JSFunction* candidate = jsfunction_candidates_head_;
  JSFunction* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    ClearNextCandidate(candidate, undefined);
    candidate = next_candidate;
  }

  jsfunction_candidates_head_ = NULL;
}


void CodeFlusher::EvictSharedFunctionInfoCandidates() {
  SharedFunctionInfo* candidate = shared_function_info_candidates_head_;
  SharedFunctionInfo* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    ClearNextCandidate(candidate);
    candidate = next_candidate;
  }

  shared_function_info_candidates_head_ = NULL;
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
  // Optimization: If the heap object pointed to by p is a non-symbol
  // cons string whose right substring is HEAP->empty_string, update
  // it in place to its left substring.  Return the updated value.
  //
  // Here we assume that if we change *p, we replace it with a heap object
  // (i.e., the left substring of a cons string is always a heap object).
  //
  // The check performed is:
  //   object->IsConsString() && !object->IsSymbol() &&
  //   (ConsString::cast(object)->second() == HEAP->empty_string())
  // except the maps for the object and its possible substrings might be
  // marked.
  HeapObject* object = HeapObject::cast(*p);
  if (!FLAG_clever_optimizations) return object;
  Map* map = object->map();
  InstanceType type = map->instance_type();
  if ((type & kShortcutTypeMask) != kShortcutTypeTag) return object;

  Object* second = reinterpret_cast<ConsString*>(object)->unchecked_second();
  Heap* heap = map->GetHeap();
  if (second != heap->empty_string()) {
    return object;
  }

  // Since we don't have the object's start, it is impossible to update the
  // page dirty marks. Therefore, we only replace the string with its left
  // substring when page dirty marks do not change.
  Object* first = reinterpret_cast<ConsString*>(object)->unchecked_first();
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
    ASSERT(Isolate::Current()->heap()->Contains(obj));
    ASSERT(!HEAP->mark_compact_collector()->IsMarked(obj));
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
  static inline bool VisitUnmarkedObjects(Heap* heap,
                                          Object** start,
                                          Object** end) {
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

  static void VisitJSWeakMap(Map* map, HeapObject* object) {
    MarkCompactCollector* collector = map->GetHeap()->mark_compact_collector();
    JSWeakMap* weak_map = reinterpret_cast<JSWeakMap*>(object);

    // Enqueue weak map in linked list of encountered weak maps.
    if (weak_map->next() == Smi::FromInt(0)) {
      weak_map->set_next(collector->encountered_weak_maps());
      collector->set_encountered_weak_maps(weak_map);
    }

    // Skip visiting the backing hash table containing the mappings.
    int object_size = JSWeakMap::BodyDescriptor::SizeOf(map, object);
    BodyVisitorBase<MarkCompactMarkingVisitor>::IteratePointers(
        map->GetHeap(),
        object,
        JSWeakMap::BodyDescriptor::kStartOffset,
        JSWeakMap::kTableOffset);
    BodyVisitorBase<MarkCompactMarkingVisitor>::IteratePointers(
        map->GetHeap(),
        object,
        JSWeakMap::kTableOffset + kPointerSize,
        object_size);

    // Mark the backing hash table without pushing it on the marking stack.
    Object* table_object = weak_map->table();
    if (!table_object->IsHashTable()) return;
    ObjectHashTable* table = ObjectHashTable::cast(table_object);
    Object** table_slot =
        HeapObject::RawField(weak_map, JSWeakMap::kTableOffset);
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
    if (re->TypeTagUnchecked() != JSRegExp::IRREGEXP) return;

    Object* code = re->DataAtUnchecked(JSRegExp::code_index(is_ascii));
    if (!code->IsSmi() &&
        HeapObject::cast(code)->map()->instance_type() == CODE_TYPE) {
      // Save a copy that can be reinstated if we need the code again.
      re->SetDataAtUnchecked(JSRegExp::saved_code_index(is_ascii),
                             code,
                             heap);

      // Saving a copy might create a pointer into compaction candidate
      // that was not observed by marker.  This might happen if JSRegExp data
      // was marked through the compilation cache before marker reached JSRegExp
      // object.
      FixedArray* data = FixedArray::cast(re->data());
      Object** slot = data->data_start() + JSRegExp::saved_code_index(is_ascii);
      heap->mark_compact_collector()->
          RecordSlot(slot, slot, code);

      // Set a number in the 0-255 range to guarantee no smi overflow.
      re->SetDataAtUnchecked(JSRegExp::code_index(is_ascii),
                             Smi::FromInt(heap->sweep_generation() & 0xff),
                             heap);
    } else if (code->IsSmi()) {
      int value = Smi::cast(code)->value();
      // The regexp has not been compiled yet or there was a compilation error.
      if (value == JSRegExp::kUninitializedValue ||
          value == JSRegExp::kCompilationErrorValue) {
        return;
      }

      // Check if we should flush now.
      if (value == ((heap->sweep_generation() - kRegExpCodeThreshold) & 0xff)) {
        re->SetDataAtUnchecked(JSRegExp::code_index(is_ascii),
                               Smi::FromInt(JSRegExp::kUninitializedValue),
                               heap);
        re->SetDataAtUnchecked(JSRegExp::saved_code_index(is_ascii),
                               Smi::FromInt(JSRegExp::kUninitializedValue),
                               heap);
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
    if (map_obj->code_cache() != heap->empty_fixed_array()) {
      heap->RecordObjectStats(
          FIXED_ARRAY_TYPE,
          MAP_CODE_CACHE_SUB_TYPE,
          FixedArray::cast(map_obj->code_cache())->Size());
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
    if (fixed_array == heap->symbol_table()) {
      heap->RecordObjectStats(
          FIXED_ARRAY_TYPE,
          SYMBOL_TABLE_SUB_TYPE,
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


class MarkingVisitor : public ObjectVisitor {
 public:
  explicit MarkingVisitor(Heap* heap) : heap_(heap) { }

  void VisitPointer(Object** p) {
    MarkCompactMarkingVisitor::VisitPointer(heap_, p);
  }

  void VisitPointers(Object** start, Object** end) {
    MarkCompactMarkingVisitor::VisitPointers(heap_, start, end);
  }

 private:
  Heap* heap_;
};


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
  ASSERT(heap() == Isolate::Current()->heap());

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


// Helper class for pruning the symbol table.
class SymbolTableCleaner : public ObjectVisitor {
 public:
  explicit SymbolTableCleaner(Heap* heap)
    : heap_(heap), pointers_removed_(0) { }

  virtual void VisitPointers(Object** start, Object** end) {
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (o->IsHeapObject() &&
          !Marking::MarkBitFrom(HeapObject::cast(o)).Get()) {
        // Check if the symbol being pruned is an external symbol. We need to
        // delete the associated external data as this symbol is going away.

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


static void DiscoverGreyObjectsOnPage(MarkingDeque* marking_deque, Page* p) {
  ASSERT(!marking_deque->IsFull());
  ASSERT(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
  ASSERT(strcmp(Marking::kGreyBitPattern, "11") == 0);
  ASSERT(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  MarkBit::CellType* cells = p->markbits()->cells();

  int last_cell_index =
      Bitmap::IndexToCell(
          Bitmap::CellAlignIndex(
              p->AddressToMarkbitIndex(p->area_end())));

  Address cell_base = p->area_start();
  int cell_index = Bitmap::IndexToCell(
          Bitmap::CellAlignIndex(
              p->AddressToMarkbitIndex(cell_base)));


  for (;
       cell_index < last_cell_index;
       cell_index++, cell_base += 32 * kPointerSize) {
    ASSERT((unsigned)cell_index ==
        Bitmap::IndexToCell(
            Bitmap::CellAlignIndex(
                p->AddressToMarkbitIndex(cell_base))));

    const MarkBit::CellType current_cell = cells[cell_index];
    if (current_cell == 0) continue;

    const MarkBit::CellType next_cell = cells[cell_index + 1];
    MarkBit::CellType grey_objects = current_cell &
        ((current_cell >> 1) | (next_cell << (Bitmap::kBitsPerCell - 1)));

    int offset = 0;
    while (grey_objects != 0) {
      int trailing_zeros = CompilerIntrinsics::CountTrailingZeros(grey_objects);
      grey_objects >>= trailing_zeros;
      offset += trailing_zeros;
      MarkBit markbit(&cells[cell_index], 1 << offset, false);
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


void MarkCompactCollector::MarkSymbolTable() {
  SymbolTable* symbol_table = heap()->symbol_table();
  // Mark the symbol table itself.
  MarkBit symbol_table_mark = Marking::MarkBitFrom(symbol_table);
  SetMark(symbol_table, symbol_table_mark);
  // Explicitly mark the prefix.
  MarkingVisitor marker(heap());
  symbol_table->IteratePrefix(&marker);
  ProcessMarkingDeque();
}


void MarkCompactCollector::MarkRoots(RootMarkingVisitor* visitor) {
  // Mark the heap roots including global variables, stack variables,
  // etc., and all objects reachable from them.
  heap()->IterateStrongRoots(visitor, VISIT_ONLY_STRONG);

  // Handle the symbol table specially.
  MarkSymbolTable();

  // There may be overflowed objects in the heap.  Visit them now.
  while (marking_deque_.overflowed()) {
    RefillMarkingDeque();
    EmptyMarkingDeque();
  }
}


void MarkCompactCollector::MarkImplicitRefGroups() {
  List<ImplicitRefGroup*>* ref_groups =
      heap()->isolate()->global_handles()->implicit_ref_groups();

  int last = 0;
  for (int i = 0; i < ref_groups->length(); i++) {
    ImplicitRefGroup* entry = ref_groups->at(i);
    ASSERT(entry != NULL);

    if (!IsMarked(*entry->parent_)) {
      (*ref_groups)[last++] = entry;
      continue;
    }

    Object*** children = entry->children_;
    // A parent object is marked, so mark all child heap objects.
    for (size_t j = 0; j < entry->length_; ++j) {
      if ((*children[j])->IsHeapObject()) {
        HeapObject* child = HeapObject::cast(*children[j]);
        MarkBit mark = Marking::MarkBitFrom(child);
        MarkObject(child, mark);
      }
    }

    // Once the entire group has been marked, dispose it because it's
    // not needed anymore.
    entry->Dispose();
  }
  ref_groups->Rewind(last);
}


// Mark all objects reachable from the objects on the marking stack.
// Before: the marking stack contains zero or more heap object pointers.
// After: the marking stack is empty, and all objects reachable from the
// marking stack have been marked, or are overflowed in the heap.
void MarkCompactCollector::EmptyMarkingDeque() {
  while (!marking_deque_.IsEmpty()) {
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

    // Process encountered weak maps, mark objects only reachable by those
    // weak maps and repeat until fix-point is reached.
    ProcessWeakMaps();
  }
}


// Sweep the heap for overflowed objects, clear their overflow bits, and
// push them on the marking stack.  Stop early if the marking stack fills
// before sweeping completes.  If sweeping completes, there are no remaining
// overflowed objects in the heap so the overflow flag on the markings stack
// is cleared.
void MarkCompactCollector::RefillMarkingDeque() {
  ASSERT(marking_deque_.overflowed());

  SemiSpaceIterator new_it(heap()->new_space());
  DiscoverGreyObjectsWithIterator(heap(), &marking_deque_, &new_it);
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


void MarkCompactCollector::ProcessExternalMarking(RootMarkingVisitor* visitor) {
  bool work_to_do = true;
  ASSERT(marking_deque_.IsEmpty());
  while (work_to_do) {
    heap()->isolate()->global_handles()->IterateObjectGroups(
        visitor, &IsUnmarkedHeapObjectWithHeap);
    MarkImplicitRefGroups();
    work_to_do = !marking_deque_.IsEmpty();
    ProcessMarkingDeque();
  }
}


void MarkCompactCollector::MarkLiveObjects() {
  GCTracer::Scope gc_scope(tracer_, GCTracer::Scope::MC_MARK);
  // The recursive GC marker detects when it is nearing stack overflow,
  // and switches to a different marking system.  JS interrupts interfere
  // with the C stack limit check.
  PostponeInterruptsScope postpone(heap()->isolate());

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
        ASSERT(cell->IsJSGlobalPropertyCell());
        if (IsMarked(cell)) {
          int offset = JSGlobalPropertyCell::kValueOffset;
          MarkCompactMarkingVisitor::VisitPointer(
              heap(),
              reinterpret_cast<Object**>(cell->address() + offset));
        }
      }
    }
  }

  RootMarkingVisitor root_visitor(heap());
  MarkRoots(&root_visitor);

  // The objects reachable from the roots are marked, yet unreachable
  // objects are unmarked.  Mark objects reachable due to host
  // application specific logic.
  ProcessExternalMarking(&root_visitor);

  // The objects reachable from the roots or object groups are marked,
  // yet unreachable objects are unmarked.  Mark objects reachable
  // only from weak global handles.
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

  // Repeat host application specific marking to mark unmarked objects
  // reachable from the weak roots.
  ProcessExternalMarking(&root_visitor);

  AfterMarking();
}


void MarkCompactCollector::AfterMarking() {
  // Object literal map caches reference symbols (cache keys) and maps
  // (cache values). At this point still useful maps have already been
  // marked. Mark the keys for the alive values before we process the
  // symbol table.
  ProcessMapCaches();

  // Prune the symbol table removing all symbols only pointed to by the
  // symbol table.  Cannot use symbol_table() here because the symbol
  // table is marked.
  SymbolTable* symbol_table = heap()->symbol_table();
  SymbolTableCleaner v(heap());
  symbol_table->IterateElements(&v);
  symbol_table->ElementsRemoved(v.PointersRemoved());
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
    if (obj->IsFreeSpace()) continue;
    Map* map = Map::cast(obj);

    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    if (map->instance_type() < FIRST_JS_RECEIVER_TYPE) continue;

    if (map->attached_to_shared_function_info()) {
      JSFunction::cast(map->constructor())->shared()->AttachInitialMap(map);
    }
  }
}


void MarkCompactCollector::ClearNonLiveTransitions() {
  HeapObjectIterator map_iterator(heap()->map_space());
  // Iterate over the map space, setting map transitions that go from
  // a marked map to an unmarked map to null transitions.  This action
  // is carried out only on maps of JSObjects and related subtypes.
  for (HeapObject* obj = map_iterator.Next();
       obj != NULL; obj = map_iterator.Next()) {
    Map* map = reinterpret_cast<Map*>(obj);
    MarkBit map_mark = Marking::MarkBitFrom(map);
    if (map->IsFreeSpace()) continue;

    ASSERT(map->IsMap());
    // Only JSObject and subtypes have map transitions and back pointers.
    STATIC_ASSERT(LAST_TYPE == LAST_JS_OBJECT_TYPE);
    if (map->instance_type() < FIRST_JS_OBJECT_TYPE) continue;

    if (map_mark.Get() &&
        map->attached_to_shared_function_info()) {
      // This map is used for inobject slack tracking and has been detached
      // from SharedFunctionInfo during the mark phase.
      // Since it survived the GC, reattach it now.
      map->unchecked_constructor()->unchecked_shared()->AttachInitialMap(map);
    }

    ClearNonLivePrototypeTransitions(map);
    ClearNonLiveMapTransitions(map, map_mark);
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
        prototype_transitions->set_unchecked(
            heap_,
            proto_index,
            prototype,
            UPDATE_WRITE_BARRIER);
        prototype_transitions->set_unchecked(
            heap_,
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
    prototype_transitions->set_undefined(heap_, header + i);
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


void MarkCompactCollector::ProcessWeakMaps() {
  Object* weak_map_obj = encountered_weak_maps();
  while (weak_map_obj != Smi::FromInt(0)) {
    ASSERT(MarkCompactCollector::IsMarked(HeapObject::cast(weak_map_obj)));
    JSWeakMap* weak_map = reinterpret_cast<JSWeakMap*>(weak_map_obj);
    ObjectHashTable* table = ObjectHashTable::cast(weak_map->table());
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
    weak_map_obj = weak_map->next();
  }
}


void MarkCompactCollector::ClearWeakMaps() {
  Object* weak_map_obj = encountered_weak_maps();
  while (weak_map_obj != Smi::FromInt(0)) {
    ASSERT(MarkCompactCollector::IsMarked(HeapObject::cast(weak_map_obj)));
    JSWeakMap* weak_map = reinterpret_cast<JSWeakMap*>(weak_map_obj);
    ObjectHashTable* table = ObjectHashTable::cast(weak_map->table());
    for (int i = 0; i < table->Capacity(); i++) {
      if (!MarkCompactCollector::IsMarked(HeapObject::cast(table->KeyAt(i)))) {
        table->RemoveEntry(i);
      }
    }
    weak_map_obj = weak_map->next();
    weak_map->set_next(Smi::FromInt(0));
  }
  set_encountered_weak_maps(Smi::FromInt(0));
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
  if (dest == OLD_POINTER_SPACE || dest == LO_SPACE) {
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
    PROFILE(heap()->isolate(), CodeMoveEvent(src, dst));
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
  Object* result;

  if (object_size > Page::kMaxNonCodeHeapObjectSize) {
    MaybeObject* maybe_result =
        heap()->lo_space()->AllocateRaw(object_size, NOT_EXECUTABLE);
    if (maybe_result->ToObject(&result)) {
      HeapObject* target = HeapObject::cast(result);
      MigrateObject(target->address(),
                    object->address(),
                    object_size,
                    LO_SPACE);
      heap()->mark_compact_collector()->tracer()->
          increment_promoted_objects_size(object_size);
      return true;
    }
  } else {
    OldSpace* target_space = heap()->TargetSpace(object);

    ASSERT(target_space == heap()->old_pointer_space() ||
           target_space == heap()->old_data_space());
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
  SemiSpaceIterator from_it(from_bottom, from_top);
  for (HeapObject* object = from_it.Next();
       object != NULL;
       object = from_it.Next()) {
    MarkBit mark_bit = Marking::MarkBitFrom(object);
    if (mark_bit.Get()) {
      mark_bit.Clear();
      // Don't bother decrementing live bytes count. We'll discard the
      // entire page at the end.
      int size = object->Size();
      survivors_size += size;

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
    } else {
      // Process the dead object before we write a NULL into its header.
      LiveObjectList::ProcessNonLive(object);

      // Mark dead objects in the new space with null in their map field.
      Memory::Address_at(object->address()) = NULL;
    }
  }

  heap_->IncrementYoungSurvivorsCounter(survivors_size);
  new_space->set_age_mark(new_space->top());
}


void MarkCompactCollector::EvacuateLiveObjectsFromPage(Page* p) {
  AlwaysAllocateScope always_allocate;
  PagedSpace* space = static_cast<PagedSpace*>(p->owner());
  ASSERT(p->IsEvacuationCandidate() && !p->WasSwept());
  MarkBit::CellType* cells = p->markbits()->cells();
  p->MarkSweptPrecisely();

  int last_cell_index =
      Bitmap::IndexToCell(
          Bitmap::CellAlignIndex(
              p->AddressToMarkbitIndex(p->area_end())));

  Address cell_base = p->area_start();
  int cell_index = Bitmap::IndexToCell(
          Bitmap::CellAlignIndex(
              p->AddressToMarkbitIndex(cell_base)));

  int offsets[16];

  for (;
       cell_index < last_cell_index;
       cell_index++, cell_base += 32 * kPointerSize) {
    ASSERT((unsigned)cell_index ==
        Bitmap::IndexToCell(
            Bitmap::CellAlignIndex(
                p->AddressToMarkbitIndex(cell_base))));
    if (cells[cell_index] == 0) continue;

    int live_objects = MarkWordToObjectStarts(cells[cell_index], offsets);
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
    cells[cell_index] = 0;
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


static inline void UpdateSlot(ObjectVisitor* v,
                              SlotsBuffer::SlotType slot_type,
                              Address addr) {
  switch (slot_type) {
    case SlotsBuffer::CODE_TARGET_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::CODE_TARGET, 0, NULL);
      rinfo.Visit(v);
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
      if (rinfo.IsPatchedDebugBreakSlotSequence()) rinfo.Visit(v);
      break;
    }
    case SlotsBuffer::JS_RETURN_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::JS_RETURN, 0, NULL);
      if (rinfo.IsPatchedReturnSequence()) rinfo.Visit(v);
      break;
    }
    case SlotsBuffer::EMBEDDED_OBJECT_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::EMBEDDED_OBJECT, 0, NULL);
      rinfo.Visit(v);
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

  MarkBit::CellType* cells = p->markbits()->cells();
  p->MarkSweptPrecisely();

  int last_cell_index =
      Bitmap::IndexToCell(
          Bitmap::CellAlignIndex(
              p->AddressToMarkbitIndex(p->area_end())));

  Address free_start = p->area_start();
  int cell_index =
      Bitmap::IndexToCell(
          Bitmap::CellAlignIndex(
              p->AddressToMarkbitIndex(free_start)));

  ASSERT(reinterpret_cast<intptr_t>(free_start) % (32 * kPointerSize) == 0);
  Address object_address = free_start;
  int offsets[16];

  SkipList* skip_list = p->skip_list();
  int curr_region = -1;
  if ((skip_list_mode == REBUILD_SKIP_LIST) && skip_list) {
    skip_list->Clear();
  }

  for (;
       cell_index < last_cell_index;
       cell_index++, object_address += 32 * kPointerSize) {
    ASSERT((unsigned)cell_index ==
        Bitmap::IndexToCell(
            Bitmap::CellAlignIndex(
                p->AddressToMarkbitIndex(object_address))));
    int live_objects = MarkWordToObjectStarts(cells[cell_index], offsets);
    int live_index = 0;
    for ( ; live_objects != 0; live_objects--) {
      Address free_end = object_address + offsets[live_index++] * kPointerSize;
      if (free_end != free_start) {
        space->Free(free_start, static_cast<int>(free_end - free_start));
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
    cells[cell_index] = 0;
  }
  if (free_start != p->area_end()) {
    space->Free(free_start, static_cast<int>(p->area_end() - free_start));
  }
  p->ResetLiveBytes();
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
    LiveObjectList::IterateElements(&updating_visitor);
  }

  { GCTracer::Scope gc_scope(tracer_,
                             GCTracer::Scope::MC_UPDATE_OLD_TO_NEW_POINTERS);
    StoreBufferRebuildScope scope(heap_,
                                  heap_->store_buffer(),
                                  &Heap::ScavengeStoreBufferCallback);
    heap_->store_buffer()->IteratePointersToNewSpace(&UpdatePointer);
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
            SweepConservatively(space, p);
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
    if (cell->IsJSGlobalPropertyCell()) {
      Address value_address =
          reinterpret_cast<Address>(cell) +
          (JSGlobalPropertyCell::kValueOffset - kHeapObjectTag);
      updating_visitor.VisitPointer(reinterpret_cast<Object**>(value_address));
    }
  }

  // Update pointer from the native contexts list.
  updating_visitor.VisitPointer(heap_->native_contexts_list_address());

  heap_->symbol_table()->Iterate(&updating_visitor);

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


// Sweeps a space conservatively.  After this has been done the larger free
// spaces have been put on the free list and the smaller ones have been
// ignored and left untouched.  A free space is always either ignored or put
// on the free list, never split up into two parts.  This is important
// because it means that any FreeSpace maps left actually describe a region of
// memory that can be ignored when scanning.  Dead objects other than free
// spaces will not contain the free space map.
intptr_t MarkCompactCollector::SweepConservatively(PagedSpace* space, Page* p) {
  ASSERT(!p->IsEvacuationCandidate() && !p->WasSwept());
  MarkBit::CellType* cells = p->markbits()->cells();
  p->MarkSweptConservatively();

  int last_cell_index =
      Bitmap::IndexToCell(
          Bitmap::CellAlignIndex(
              p->AddressToMarkbitIndex(p->area_end())));

  int cell_index =
      Bitmap::IndexToCell(
          Bitmap::CellAlignIndex(
              p->AddressToMarkbitIndex(p->area_start())));

  intptr_t freed_bytes = 0;

  // This is the start of the 32 word block that we are currently looking at.
  Address block_address = p->area_start();

  // Skip over all the dead objects at the start of the page and mark them free.
  for (;
       cell_index < last_cell_index;
       cell_index++, block_address += 32 * kPointerSize) {
    if (cells[cell_index] != 0) break;
  }
  size_t size = block_address - p->area_start();
  if (cell_index == last_cell_index) {
    freed_bytes += static_cast<int>(space->Free(p->area_start(),
                                                static_cast<int>(size)));
    ASSERT_EQ(0, p->LiveBytes());
    return freed_bytes;
  }
  // Grow the size of the start-of-page free space a little to get up to the
  // first live object.
  Address free_end = StartOfLiveObject(block_address, cells[cell_index]);
  // Free the first free space.
  size = free_end - p->area_start();
  freed_bytes += space->Free(p->area_start(),
                             static_cast<int>(size));
  // The start of the current free area is represented in undigested form by
  // the address of the last 32-word section that contained a live object and
  // the marking bitmap for that cell, which describes where the live object
  // started.  Unless we find a large free space in the bitmap we will not
  // digest this pair into a real address.  We start the iteration here at the
  // first word in the marking bit map that indicates a live object.
  Address free_start = block_address;
  uint32_t free_start_cell = cells[cell_index];

  for ( ;
       cell_index < last_cell_index;
       cell_index++, block_address += 32 * kPointerSize) {
    ASSERT((unsigned)cell_index ==
        Bitmap::IndexToCell(
            Bitmap::CellAlignIndex(
                p->AddressToMarkbitIndex(block_address))));
    uint32_t cell = cells[cell_index];
    if (cell != 0) {
      // We have a live object.  Check approximately whether it is more than 32
      // words since the last live object.
      if (block_address - free_start > 32 * kPointerSize) {
        free_start = DigestFreeStart(free_start, free_start_cell);
        if (block_address - free_start > 32 * kPointerSize) {
          // Now that we know the exact start of the free space it still looks
          // like we have a large enough free space to be worth bothering with.
          // so now we need to find the start of the first live object at the
          // end of the free space.
          free_end = StartOfLiveObject(block_address, cell);
          freed_bytes += space->Free(free_start,
                                     static_cast<int>(free_end - free_start));
        }
      }
      // Update our undigested record of where the current free area started.
      free_start = block_address;
      free_start_cell = cell;
      // Clear marking bits for current cell.
      cells[cell_index] = 0;
    }
  }

  // Handle the free space at the end of the page.
  if (block_address - free_start > 32 * kPointerSize) {
    free_start = DigestFreeStart(free_start, free_start_cell);
    freed_bytes += space->Free(free_start,
                               static_cast<int>(block_address - free_start));
  }

  p->ResetLiveBytes();
  return freed_bytes;
}


void MarkCompactCollector::SweepSpace(PagedSpace* space, SweeperType sweeper) {
  space->set_was_swept_conservatively(sweeper == CONSERVATIVE ||
                                      sweeper == LAZY_CONSERVATIVE);

  space->ClearStats();

  PageIterator it(space);

  intptr_t freed_bytes = 0;
  int pages_swept = 0;
  bool lazy_sweeping_active = false;
  bool unused_page_present = false;

  while (it.has_next()) {
    Page* p = it.next();

    // Clear sweeping flags indicating that marking bits are still intact.
    p->ClearSweptPrecisely();
    p->ClearSweptConservatively();

    if (p->IsEvacuationCandidate()) {
      ASSERT(evacuation_candidates_.length() > 0);
      continue;
    }

    if (p->IsFlagSet(Page::RESCAN_ON_EVACUATION)) {
      // Will be processed in EvacuateNewSpaceAndCandidates.
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

    if (lazy_sweeping_active) {
      if (FLAG_gc_verbose) {
        PrintF("Sweeping 0x%" V8PRIxPTR " lazily postponed.\n",
               reinterpret_cast<intptr_t>(p));
      }
      space->IncreaseUnsweptFreeBytes(p);
      continue;
    }

    switch (sweeper) {
      case CONSERVATIVE: {
        if (FLAG_gc_verbose) {
          PrintF("Sweeping 0x%" V8PRIxPTR " conservatively.\n",
                 reinterpret_cast<intptr_t>(p));
        }
        SweepConservatively(space, p);
        pages_swept++;
        break;
      }
      case LAZY_CONSERVATIVE: {
        if (FLAG_gc_verbose) {
          PrintF("Sweeping 0x%" V8PRIxPTR " conservatively as needed.\n",
                 reinterpret_cast<intptr_t>(p));
        }
        freed_bytes += SweepConservatively(space, p);
        pages_swept++;
        space->SetPagesToSweep(p->next_page());
        lazy_sweeping_active = true;
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
  if (FLAG_expose_gc) how_to_sweep = CONSERVATIVE;
  if (sweep_precisely_) how_to_sweep = PRECISE;
  // Noncompacting collections simply sweep the spaces to clear the mark
  // bits and free the nonlive blocks (for old and map spaces).  We sweep
  // the map space last because freeing non-live maps overwrites them and
  // the other spaces rely on possibly non-live maps to get the sizes for
  // non-live objects.
  SweepSpace(heap()->old_pointer_space(), how_to_sweep);
  SweepSpace(heap()->old_data_space(), how_to_sweep);

  RemoveDeadInvalidatedCode();
  SweepSpace(heap()->code_space(), PRECISE);

  SweepSpace(heap()->cell_space(), PRECISE);

  EvacuateNewSpaceAndCandidates();

  // ClearNonLiveTransitions depends on precise sweeping of map space to
  // detect whether unmarked map became dead in this collection or in one
  // of the previous ones.
  SweepSpace(heap()->map_space(), PRECISE);

  // Deallocate unmarked objects and clear marked bits for marked objects.
  heap_->lo_space()->FreeUnmarkedObjects();
}


void MarkCompactCollector::EnableCodeFlushing(bool enable) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (heap()->isolate()->debug()->IsLoaded() ||
      heap()->isolate()->debug()->has_break_points()) {
    enable = false;
  }
#endif

  if (enable) {
    if (code_flusher_ != NULL) return;
    code_flusher_ = new CodeFlusher(heap()->isolate());
  } else {
    if (code_flusher_ == NULL) return;
    code_flusher_->EvictAllCandidates();
    delete code_flusher_;
    code_flusher_ = NULL;
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
    Code* host = heap()->isolate()->inner_pointer_to_code_cache()->
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
      UpdateSlot(&v,
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
        UpdateSlot(&v,
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

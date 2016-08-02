// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_H_
#define V8_HEAP_MARK_COMPACT_H_

#include <deque>

#include "src/base/bits.h"
#include "src/heap/spaces.h"
#include "src/heap/store-buffer.h"

namespace v8 {
namespace internal {

// Callback function, returns whether an object is alive. The heap size
// of the object is returned in size. It optionally updates the offset
// to the first live object in the page (only used for old and map objects).
typedef bool (*IsAliveFunction)(HeapObject* obj, int* size, int* offset);

// Callback function to mark an object in a given heap.
typedef void (*MarkObjectFunction)(Heap* heap, HeapObject* object);

// Forward declarations.
class CodeFlusher;
class MarkCompactCollector;
class MarkingVisitor;
class RootMarkingVisitor;

class Marking : public AllStatic {
 public:
  INLINE(static MarkBit MarkBitFrom(Address addr)) {
    MemoryChunk* p = MemoryChunk::FromAddress(addr);
    return p->markbits()->MarkBitFromIndex(p->AddressToMarkbitIndex(addr));
  }

  INLINE(static MarkBit MarkBitFrom(HeapObject* obj)) {
    return MarkBitFrom(reinterpret_cast<Address>(obj));
  }

  // Impossible markbits: 01
  static const char* kImpossibleBitPattern;
  INLINE(static bool IsImpossible(MarkBit mark_bit)) {
    return !mark_bit.Get() && mark_bit.Next().Get();
  }

  // Black markbits: 11
  static const char* kBlackBitPattern;
  INLINE(static bool IsBlack(MarkBit mark_bit)) {
    return mark_bit.Get() && mark_bit.Next().Get();
  }

  // White markbits: 00 - this is required by the mark bit clearer.
  static const char* kWhiteBitPattern;
  INLINE(static bool IsWhite(MarkBit mark_bit)) {
    DCHECK(!IsImpossible(mark_bit));
    return !mark_bit.Get();
  }

  // Grey markbits: 10
  static const char* kGreyBitPattern;
  INLINE(static bool IsGrey(MarkBit mark_bit)) {
    return mark_bit.Get() && !mark_bit.Next().Get();
  }

  // IsBlackOrGrey assumes that the first bit is set for black or grey
  // objects.
  INLINE(static bool IsBlackOrGrey(MarkBit mark_bit)) { return mark_bit.Get(); }

  INLINE(static void MarkBlack(MarkBit mark_bit)) {
    mark_bit.Set();
    mark_bit.Next().Set();
  }

  INLINE(static void MarkWhite(MarkBit mark_bit)) {
    mark_bit.Clear();
    mark_bit.Next().Clear();
  }

  INLINE(static void BlackToWhite(MarkBit markbit)) {
    DCHECK(IsBlack(markbit));
    markbit.Clear();
    markbit.Next().Clear();
  }

  INLINE(static void GreyToWhite(MarkBit markbit)) {
    DCHECK(IsGrey(markbit));
    markbit.Clear();
    markbit.Next().Clear();
  }

  INLINE(static void BlackToGrey(MarkBit markbit)) {
    DCHECK(IsBlack(markbit));
    markbit.Next().Clear();
  }

  INLINE(static void WhiteToGrey(MarkBit markbit)) {
    DCHECK(IsWhite(markbit));
    markbit.Set();
  }

  INLINE(static void WhiteToBlack(MarkBit markbit)) {
    DCHECK(IsWhite(markbit));
    markbit.Set();
    markbit.Next().Set();
  }

  INLINE(static void GreyToBlack(MarkBit markbit)) {
    DCHECK(IsGrey(markbit));
    markbit.Next().Set();
  }

  INLINE(static void BlackToGrey(HeapObject* obj)) {
    BlackToGrey(MarkBitFrom(obj));
  }

  INLINE(static void AnyToGrey(MarkBit markbit)) {
    markbit.Set();
    markbit.Next().Clear();
  }

  static void TransferMark(Heap* heap, Address old_start, Address new_start);

#ifdef DEBUG
  enum ObjectColor {
    BLACK_OBJECT,
    WHITE_OBJECT,
    GREY_OBJECT,
    IMPOSSIBLE_COLOR
  };

  static const char* ColorName(ObjectColor color) {
    switch (color) {
      case BLACK_OBJECT:
        return "black";
      case WHITE_OBJECT:
        return "white";
      case GREY_OBJECT:
        return "grey";
      case IMPOSSIBLE_COLOR:
        return "impossible";
    }
    return "error";
  }

  static ObjectColor Color(HeapObject* obj) {
    return Color(Marking::MarkBitFrom(obj));
  }

  static ObjectColor Color(MarkBit mark_bit) {
    if (IsBlack(mark_bit)) return BLACK_OBJECT;
    if (IsWhite(mark_bit)) return WHITE_OBJECT;
    if (IsGrey(mark_bit)) return GREY_OBJECT;
    UNREACHABLE();
    return IMPOSSIBLE_COLOR;
  }
#endif

  // Returns true if the transferred color is black.
  INLINE(static bool TransferColor(HeapObject* from, HeapObject* to)) {
    if (Page::FromAddress(to->address())->IsFlagSet(Page::BLACK_PAGE))
      return true;
    MarkBit from_mark_bit = MarkBitFrom(from);
    MarkBit to_mark_bit = MarkBitFrom(to);
    DCHECK(Marking::IsWhite(to_mark_bit));
    if (from_mark_bit.Get()) {
      to_mark_bit.Set();
      if (from_mark_bit.Next().Get()) {
        to_mark_bit.Next().Set();
        return true;
      }
    }
    return false;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Marking);
};

// ----------------------------------------------------------------------------
// Marking deque for tracing live objects.
class MarkingDeque {
 public:
  MarkingDeque()
      : array_(NULL),
        top_(0),
        bottom_(0),
        mask_(0),
        overflowed_(false),
        in_use_(false) {}

  void Initialize(Address low, Address high);
  void Uninitialize(bool aborting = false);

  inline bool IsFull() { return ((top_ + 1) & mask_) == bottom_; }

  inline bool IsEmpty() { return top_ == bottom_; }

  bool overflowed() const { return overflowed_; }

  bool in_use() const { return in_use_; }

  void ClearOverflowed() { overflowed_ = false; }

  void SetOverflowed() { overflowed_ = true; }

  // Push the object on the marking stack if there is room, otherwise mark the
  // deque as overflowed and wait for a rescan of the heap.
  INLINE(bool Push(HeapObject* object)) {
    DCHECK(object->IsHeapObject());
    if (IsFull()) {
      SetOverflowed();
      return false;
    } else {
      array_[top_] = object;
      top_ = ((top_ + 1) & mask_);
      return true;
    }
  }

  INLINE(HeapObject* Pop()) {
    DCHECK(!IsEmpty());
    top_ = ((top_ - 1) & mask_);
    HeapObject* object = array_[top_];
    DCHECK(object->IsHeapObject());
    return object;
  }

  // Unshift the object into the marking stack if there is room, otherwise mark
  // the deque as overflowed and wait for a rescan of the heap.
  INLINE(bool Unshift(HeapObject* object)) {
    DCHECK(object->IsHeapObject());
    if (IsFull()) {
      SetOverflowed();
      return false;
    } else {
      bottom_ = ((bottom_ - 1) & mask_);
      array_[bottom_] = object;
      return true;
    }
  }

  HeapObject** array() { return array_; }
  int bottom() { return bottom_; }
  int top() { return top_; }
  int mask() { return mask_; }
  void set_top(int top) { top_ = top; }

 private:
  HeapObject** array_;
  // array_[(top - 1) & mask_] is the top element in the deque.  The Deque is
  // empty when top_ == bottom_.  It is full when top_ + 1 == bottom
  // (mod mask + 1).
  int top_;
  int bottom_;
  int mask_;
  bool overflowed_;
  bool in_use_;

  DISALLOW_COPY_AND_ASSIGN(MarkingDeque);
};


// CodeFlusher collects candidates for code flushing during marking and
// processes those candidates after marking has completed in order to
// reset those functions referencing code objects that would otherwise
// be unreachable. Code objects can be referenced in two ways:
//    - SharedFunctionInfo references unoptimized code.
//    - JSFunction references either unoptimized or optimized code.
// We are not allowed to flush unoptimized code for functions that got
// optimized or inlined into optimized code, because we might bailout
// into the unoptimized code again during deoptimization.
class CodeFlusher {
 public:
  explicit CodeFlusher(Isolate* isolate)
      : isolate_(isolate),
        jsfunction_candidates_head_(nullptr),
        shared_function_info_candidates_head_(nullptr) {}

  inline void AddCandidate(SharedFunctionInfo* shared_info);
  inline void AddCandidate(JSFunction* function);

  void EvictCandidate(SharedFunctionInfo* shared_info);
  void EvictCandidate(JSFunction* function);

  void ProcessCandidates() {
    ProcessSharedFunctionInfoCandidates();
    ProcessJSFunctionCandidates();
  }

  void IteratePointersToFromSpace(ObjectVisitor* v);

 private:
  void ProcessJSFunctionCandidates();
  void ProcessSharedFunctionInfoCandidates();

  static inline JSFunction** GetNextCandidateSlot(JSFunction* candidate);
  static inline JSFunction* GetNextCandidate(JSFunction* candidate);
  static inline void SetNextCandidate(JSFunction* candidate,
                                      JSFunction* next_candidate);
  static inline void ClearNextCandidate(JSFunction* candidate,
                                        Object* undefined);

  static inline SharedFunctionInfo* GetNextCandidate(
      SharedFunctionInfo* candidate);
  static inline void SetNextCandidate(SharedFunctionInfo* candidate,
                                      SharedFunctionInfo* next_candidate);
  static inline void ClearNextCandidate(SharedFunctionInfo* candidate);

  Isolate* isolate_;
  JSFunction* jsfunction_candidates_head_;
  SharedFunctionInfo* shared_function_info_candidates_head_;

  DISALLOW_COPY_AND_ASSIGN(CodeFlusher);
};


// Defined in isolate.h.
class ThreadLocalTop;

class MarkBitCellIterator BASE_EMBEDDED {
 public:
  explicit MarkBitCellIterator(MemoryChunk* chunk) : chunk_(chunk) {
    last_cell_index_ = Bitmap::IndexToCell(Bitmap::CellAlignIndex(
        chunk_->AddressToMarkbitIndex(chunk_->area_end())));
    cell_base_ = chunk_->area_start();
    cell_index_ = Bitmap::IndexToCell(
        Bitmap::CellAlignIndex(chunk_->AddressToMarkbitIndex(cell_base_)));
    cells_ = chunk_->markbits()->cells();
  }

  inline bool Done() { return cell_index_ == last_cell_index_; }

  inline bool HasNext() { return cell_index_ < last_cell_index_ - 1; }

  inline MarkBit::CellType* CurrentCell() {
    DCHECK(cell_index_ == Bitmap::IndexToCell(Bitmap::CellAlignIndex(
                              chunk_->AddressToMarkbitIndex(cell_base_))));
    return &cells_[cell_index_];
  }

  inline Address CurrentCellBase() {
    DCHECK(cell_index_ == Bitmap::IndexToCell(Bitmap::CellAlignIndex(
                              chunk_->AddressToMarkbitIndex(cell_base_))));
    return cell_base_;
  }

  inline void Advance() {
    cell_index_++;
    cell_base_ += 32 * kPointerSize;
  }

  // Return the next mark bit cell. If there is no next it returns 0;
  inline MarkBit::CellType PeekNext() {
    if (HasNext()) {
      return cells_[cell_index_ + 1];
    }
    return 0;
  }

 private:
  MemoryChunk* chunk_;
  MarkBit::CellType* cells_;
  unsigned int last_cell_index_;
  unsigned int cell_index_;
  Address cell_base_;
};

// Grey objects can happen on black pages when black objects transition to
// grey e.g. when calling RecordWrites on them.
enum LiveObjectIterationMode {
  kBlackObjects,
  kGreyObjects,
  kAllLiveObjects
};

template <LiveObjectIterationMode T>
class LiveObjectIterator BASE_EMBEDDED {
 public:
  explicit LiveObjectIterator(MemoryChunk* chunk)
      : chunk_(chunk),
        it_(chunk_),
        cell_base_(it_.CurrentCellBase()),
        current_cell_(*it_.CurrentCell()) {
    // Black pages can not be iterated.
    DCHECK(!chunk->IsFlagSet(Page::BLACK_PAGE));
  }

  HeapObject* Next();

 private:
  MemoryChunk* chunk_;
  MarkBitCellIterator it_;
  Address cell_base_;
  MarkBit::CellType current_cell_;
};

// -------------------------------------------------------------------------
// Mark-Compact collector
class MarkCompactCollector {
 public:
  class Evacuator;

  class Sweeper {
   public:
    class SweeperTask;

    enum SweepingMode { SWEEP_ONLY, SWEEP_AND_VISIT_LIVE_OBJECTS };
    enum SkipListRebuildingMode { REBUILD_SKIP_LIST, IGNORE_SKIP_LIST };
    enum FreeSpaceTreatmentMode { IGNORE_FREE_SPACE, ZAP_FREE_SPACE };
    enum SweepingParallelism { SWEEP_ON_MAIN_THREAD, SWEEP_IN_PARALLEL };

    typedef std::deque<Page*> SweepingList;
    typedef List<Page*> SweptList;

    template <SweepingMode sweeping_mode, SweepingParallelism parallelism,
              SkipListRebuildingMode skip_list_mode,
              FreeSpaceTreatmentMode free_space_mode>
    static int RawSweep(PagedSpace* space, Page* p, ObjectVisitor* v);

    explicit Sweeper(Heap* heap)
        : heap_(heap),
          pending_sweeper_tasks_semaphore_(0),
          sweeping_in_progress_(false),
          late_pages_(false),
          num_sweeping_tasks_(0) {}

    bool sweeping_in_progress() { return sweeping_in_progress_; }
    bool contains_late_pages() { return late_pages_; }

    void AddPage(AllocationSpace space, Page* page);
    void AddLatePage(AllocationSpace space, Page* page);

    int ParallelSweepSpace(AllocationSpace identity, int required_freed_bytes,
                           int max_pages = 0);
    int ParallelSweepPage(Page* page, PagedSpace* space);

    void StartSweeping();
    void StartSweepingHelper(AllocationSpace space_to_start);
    void EnsureCompleted();
    bool IsSweepingCompleted();
    void SweepOrWaitUntilSweepingCompleted(Page* page);

    void AddSweptPageSafe(PagedSpace* space, Page* page);
    Page* GetSweptPageSafe(PagedSpace* space);

   private:
    static const int kAllocationSpaces = LAST_PAGED_SPACE + 1;

    template <typename Callback>
    void ForAllSweepingSpaces(Callback callback) {
      for (int i = 0; i < kAllocationSpaces; i++) {
        callback(static_cast<AllocationSpace>(i));
      }
    }

    Page* GetSweepingPageSafe(AllocationSpace space);
    void AddSweepingPageSafe(AllocationSpace space, Page* page);

    void PrepareToBeSweptPage(AllocationSpace space, Page* page);

    Heap* heap_;
    base::Semaphore pending_sweeper_tasks_semaphore_;
    base::Mutex mutex_;
    SweptList swept_list_[kAllocationSpaces];
    SweepingList sweeping_list_[kAllocationSpaces];
    bool sweeping_in_progress_;
    bool late_pages_;
    int num_sweeping_tasks_;
  };

  enum IterationMode {
    kKeepMarking,
    kClearMarkbits,
  };

  static void Initialize();

  void SetUp();

  void TearDown();

  void CollectEvacuationCandidates(PagedSpace* space);

  void AddEvacuationCandidate(Page* p);

  // Prepares for GC by resetting relocation info in old and map spaces and
  // choosing spaces to compact.
  void Prepare();

  // Performs a global garbage collection.
  void CollectGarbage();

  enum CompactionMode { INCREMENTAL_COMPACTION, NON_INCREMENTAL_COMPACTION };

  bool StartCompaction(CompactionMode mode);

  void AbortCompaction();

#ifdef DEBUG
  // Checks whether performing mark-compact collection.
  bool in_use() { return state_ > PREPARE_GC; }
  bool are_map_pointers_encoded() { return state_ == UPDATE_POINTERS; }
#endif

  // Determine type of object and emit deletion log event.
  static void ReportDeleteIfNeeded(HeapObject* obj, Isolate* isolate);

  // Distinguishable invalid map encodings (for single word and multiple words)
  // that indicate free regions.
  static const uint32_t kSingleFreeEncoding = 0;
  static const uint32_t kMultiFreeEncoding = 1;

  static inline bool IsMarked(Object* obj);
  static bool IsUnmarkedHeapObjectWithHeap(Heap* heap, Object** p);

  inline Heap* heap() const { return heap_; }
  inline Isolate* isolate() const;

  CodeFlusher* code_flusher() { return code_flusher_; }
  inline bool is_code_flushing_enabled() const { return code_flusher_ != NULL; }

#ifdef VERIFY_HEAP
  void VerifyValidStoreAndSlotsBufferEntries();
  void VerifyMarkbitsAreClean();
  static void VerifyMarkbitsAreClean(PagedSpace* space);
  static void VerifyMarkbitsAreClean(NewSpace* space);
  void VerifyWeakEmbeddedObjectsInCode();
  void VerifyOmittedMapChecks();
#endif

  INLINE(static bool ShouldSkipEvacuationSlotRecording(Object* host)) {
    return Page::FromAddress(reinterpret_cast<Address>(host))
        ->ShouldSkipEvacuationSlotRecording();
  }

  INLINE(static bool IsOnEvacuationCandidate(Object* obj)) {
    return Page::FromAddress(reinterpret_cast<Address>(obj))
        ->IsEvacuationCandidate();
  }

  void RecordRelocSlot(Code* host, RelocInfo* rinfo, Object* target);
  void RecordCodeEntrySlot(HeapObject* host, Address slot, Code* target);
  void RecordCodeTargetPatch(Address pc, Code* target);
  INLINE(void RecordSlot(HeapObject* object, Object** slot, Object* target));
  INLINE(void ForceRecordSlot(HeapObject* object, Object** slot,
                              Object* target));

  void UpdateSlots(SlotsBuffer* buffer);
  void UpdateSlotsRecordedIn(SlotsBuffer* buffer);

  void InvalidateCode(Code* code);

  void ClearMarkbits();

  bool is_compacting() const { return compacting_; }

  MarkingParity marking_parity() { return marking_parity_; }

  // Ensures that sweeping is finished.
  //
  // Note: Can only be called safely from main thread.
  void EnsureSweepingCompleted();

  // Help out in sweeping the corresponding space and refill memory that has
  // been regained.
  //
  // Note: Thread-safe.
  void SweepAndRefill(CompactionSpace* space);

  // Checks if sweeping is in progress right now on any space.
  bool sweeping_in_progress() { return sweeper().sweeping_in_progress(); }

  void set_evacuation(bool evacuation) { evacuation_ = evacuation; }

  bool evacuation() const { return evacuation_; }

  // Special case for processing weak references in a full collection. We need
  // to artificially keep AllocationSites alive for a time.
  void MarkAllocationSite(AllocationSite* site);

  // Mark objects in implicit references groups if their parent object
  // is marked.
  void MarkImplicitRefGroups(MarkObjectFunction mark_object);

  MarkingDeque* marking_deque() { return &marking_deque_; }

  static const size_t kMaxMarkingDequeSize = 4 * MB;
  static const size_t kMinMarkingDequeSize = 256 * KB;

  void EnsureMarkingDequeIsCommittedAndInitialize(size_t max_size) {
    if (!marking_deque_.in_use()) {
      EnsureMarkingDequeIsCommitted(max_size);
      InitializeMarkingDeque();
    }
  }

  void EnsureMarkingDequeIsCommitted(size_t max_size);
  void EnsureMarkingDequeIsReserved();

  void InitializeMarkingDeque();

  // The following two methods can just be called after marking, when the
  // whole transitive closure is known. They must be called before sweeping
  // when mark bits are still intact.
  bool IsSlotInBlackObject(MemoryChunk* p, Address slot);
  HeapObject* FindBlackObjectBySlotSlow(Address slot);

  // Removes all the slots in the slot buffers that are within the given
  // address range.
  void RemoveObjectSlots(Address start_slot, Address end_slot);

  Sweeper& sweeper() { return sweeper_; }

  std::vector<std::pair<void*, void*>>& wrappers_to_trace() {
    return wrappers_to_trace_;
  }

  void SetEmbedderHeapTracer(EmbedderHeapTracer* tracer);

  EmbedderHeapTracer* embedder_heap_tracer() { return embedder_heap_tracer_; }

  bool UsingEmbedderHeapTracer() { return embedder_heap_tracer(); }

  void TracePossibleWrapper(JSObject* js_object);

  void RegisterExternallyReferencedObject(Object** object);

 private:
  class EvacuateNewSpacePageVisitor;
  class EvacuateNewSpaceVisitor;
  class EvacuateOldSpaceVisitor;
  class EvacuateRecordOnlyVisitor;
  class EvacuateVisitorBase;
  class HeapObjectVisitor;

  explicit MarkCompactCollector(Heap* heap);

  bool WillBeDeoptimized(Code* code);
  void ClearInvalidRememberedSetSlots();

  void ComputeEvacuationHeuristics(int area_size,
                                   int* target_fragmentation_percent,
                                   int* max_evacuated_bytes);

  // Finishes GC, performs heap verification if enabled.
  void Finish();

  // -----------------------------------------------------------------------
  // Phase 1: Marking live objects.
  //
  //  Before: The heap has been prepared for garbage collection by
  //          MarkCompactCollector::Prepare() and is otherwise in its
  //          normal state.
  //
  //   After: Live objects are marked and non-live objects are unmarked.

  friend class CodeMarkingVisitor;
  friend class IncrementalMarkingMarkingVisitor;
  friend class MarkCompactMarkingVisitor;
  friend class MarkingVisitor;
  friend class RecordMigratedSlotVisitor;
  friend class RootMarkingVisitor;
  friend class SharedFunctionInfoMarkingVisitor;

  // Mark code objects that are active on the stack to prevent them
  // from being flushed.
  void PrepareThreadForCodeFlushing(Isolate* isolate, ThreadLocalTop* top);

  void PrepareForCodeFlushing();

  // Marking operations for objects reachable from roots.
  void MarkLiveObjects();

  // Pushes a black object onto the marking stack and accounts for live bytes.
  // Note that this assumes live bytes have not yet been counted.
  INLINE(void PushBlack(HeapObject* obj));

  // Unshifts a black object into the marking stack and accounts for live bytes.
  // Note that this assumes lives bytes have already been counted.
  INLINE(void UnshiftBlack(HeapObject* obj));

  // Marks the object black and pushes it on the marking stack.
  // This is for non-incremental marking only.
  INLINE(void MarkObject(HeapObject* obj, MarkBit mark_bit));

  // Marks the object black assuming that it is not yet marked.
  // This is for non-incremental marking only.
  INLINE(void SetMark(HeapObject* obj, MarkBit mark_bit));

  // Mark the heap roots and all objects reachable from them.
  void MarkRoots(RootMarkingVisitor* visitor);

  // Mark the string table specially.  References to internalized strings from
  // the string table are weak.
  void MarkStringTable(RootMarkingVisitor* visitor);

  // Mark objects reachable (transitively) from objects in the marking stack
  // or overflowed in the heap.
  void ProcessMarkingDeque();

  // Mark objects reachable (transitively) from objects in the marking stack
  // or overflowed in the heap.  This respects references only considered in
  // the final atomic marking pause including the following:
  //    - Processing of objects reachable through Harmony WeakMaps.
  //    - Objects reachable due to host application logic like object groups,
  //      implicit references' groups, or embedder heap tracing.
  void ProcessEphemeralMarking(ObjectVisitor* visitor,
                               bool only_process_harmony_weak_collections);

  // If the call-site of the top optimized code was not prepared for
  // deoptimization, then treat the maps in the code as strong pointers,
  // otherwise a map can die and deoptimize the code.
  void ProcessTopOptimizedFrame(ObjectVisitor* visitor);

  // Collects a list of dependent code from maps embedded in optimize code.
  DependentCode* DependentCodeListFromNonLiveMaps();

  // Mark objects reachable (transitively) from objects in the marking
  // stack.  This function empties the marking stack, but may leave
  // overflowed objects in the heap, in which case the marking stack's
  // overflow flag will be set.
  void EmptyMarkingDeque();

  // Refill the marking stack with overflowed objects from the heap.  This
  // function either leaves the marking stack full or clears the overflow
  // flag on the marking stack.
  void RefillMarkingDeque();

  // Helper methods for refilling the marking stack by discovering grey objects
  // on various pages of the heap. Used by {RefillMarkingDeque} only.
  template <class T>
  void DiscoverGreyObjectsWithIterator(T* it);
  void DiscoverGreyObjectsOnPage(MemoryChunk* p);
  void DiscoverGreyObjectsInSpace(PagedSpace* space);
  void DiscoverGreyObjectsInNewSpace();

  // Callback function for telling whether the object *p is an unmarked
  // heap object.
  static bool IsUnmarkedHeapObject(Object** p);

  // Clear non-live references in weak cells, transition and descriptor arrays,
  // and deoptimize dependent code of non-live maps.
  void ClearNonLiveReferences();
  void MarkDependentCodeForDeoptimization(DependentCode* list);
  // Find non-live targets of simple transitions in the given list. Clear
  // transitions to non-live targets and if needed trim descriptors arrays.
  void ClearSimpleMapTransitions(Object* non_live_map_list);
  void ClearSimpleMapTransition(Map* map, Map* dead_transition);
  // Compact every array in the global list of transition arrays and
  // trim the corresponding descriptor array if a transition target is non-live.
  void ClearFullMapTransitions();
  bool CompactTransitionArray(Map* map, TransitionArray* transitions,
                              DescriptorArray* descriptors);
  void TrimDescriptorArray(Map* map, DescriptorArray* descriptors);
  void TrimEnumCache(Map* map, DescriptorArray* descriptors);

  // Mark all values associated with reachable keys in weak collections
  // encountered so far.  This might push new object or even new weak maps onto
  // the marking stack.
  void ProcessWeakCollections();

  // After all reachable objects have been marked those weak map entries
  // with an unreachable key are removed from all encountered weak maps.
  // The linked list of all encountered weak maps is destroyed.
  void ClearWeakCollections();

  // We have to remove all encountered weak maps from the list of weak
  // collections when incremental marking is aborted.
  void AbortWeakCollections();

  void ClearWeakCells(Object** non_live_map_list,
                      DependentCode** dependent_code_list);
  void AbortWeakCells();

  void AbortTransitionArrays();

  // -----------------------------------------------------------------------
  // Phase 2: Sweeping to clear mark bits and free non-live objects for
  // a non-compacting collection.
  //
  //  Before: Live objects are marked and non-live objects are unmarked.
  //
  //   After: Live objects are unmarked, non-live regions have been added to
  //          their space's free list. Active eden semispace is compacted by
  //          evacuation.
  //

  // If we are not compacting the heap, we simply sweep the spaces except
  // for the large object space, clearing mark bits and adding unmarked
  // regions to each space's free list.
  void SweepSpaces();

  void EvacuateNewSpacePrologue();
  void EvacuateNewSpaceEpilogue();

  void EvacuatePagesInParallel();

  // The number of parallel compaction tasks, including the main thread.
  int NumberOfParallelCompactionTasks(int pages, intptr_t live_bytes);

  void EvacuateNewSpaceAndCandidates();

  void UpdatePointersAfterEvacuation();

  // Iterates through all live objects on a page using marking information.
  // Returns whether all objects have successfully been visited.
  template <class Visitor>
  bool VisitLiveObjects(MemoryChunk* page, Visitor* visitor,
                        IterationMode mode);

  void VisitLiveObjectsBody(Page* page, ObjectVisitor* visitor);

  void RecomputeLiveBytes(MemoryChunk* page);

  void ReleaseEvacuationCandidates();

  // Starts sweeping of a space by contributing on the main thread and setting
  // up other pages for sweeping.
  void StartSweepSpace(PagedSpace* space);

#ifdef DEBUG
  friend class MarkObjectVisitor;
  static void VisitObject(HeapObject* obj);

  friend class UnmarkObjectVisitor;
  static void UnmarkObject(HeapObject* obj);
#endif

  Heap* heap_;

  base::Semaphore page_parallel_job_semaphore_;

#ifdef DEBUG
  enum CollectorState {
    IDLE,
    PREPARE_GC,
    MARK_LIVE_OBJECTS,
    SWEEP_SPACES,
    ENCODE_FORWARDING_ADDRESSES,
    UPDATE_POINTERS,
    RELOCATE_OBJECTS
  };

  // The current stage of the collector.
  CollectorState state_;
#endif

  MarkingParity marking_parity_;

  bool was_marked_incrementally_;

  bool evacuation_;

  // True if we are collecting slots to perform evacuation from evacuation
  // candidates.
  bool compacting_;

  bool black_allocation_;

  bool have_code_to_deoptimize_;

  base::VirtualMemory* marking_deque_memory_;
  size_t marking_deque_memory_committed_;
  MarkingDeque marking_deque_;
  std::vector<std::pair<void*, void*>> wrappers_to_trace_;

  CodeFlusher* code_flusher_;

  EmbedderHeapTracer* embedder_heap_tracer_;

  List<Page*> evacuation_candidates_;
  List<Page*> newspace_evacuation_candidates_;

  Sweeper sweeper_;

  friend class Heap;
  friend class StoreBuffer;
};


class EvacuationScope BASE_EMBEDDED {
 public:
  explicit EvacuationScope(MarkCompactCollector* collector)
      : collector_(collector) {
    collector_->set_evacuation(true);
  }

  ~EvacuationScope() { collector_->set_evacuation(false); }

 private:
  MarkCompactCollector* collector_;
};


const char* AllocationSpaceName(AllocationSpace space);
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_H_

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

#ifndef V8_MARK_COMPACT_H_
#define V8_MARK_COMPACT_H_

#include "compiler-intrinsics.h"
#include "spaces.h"

namespace v8 {
namespace internal {

// Callback function, returns whether an object is alive. The heap size
// of the object is returned in size. It optionally updates the offset
// to the first live object in the page (only used for old and map objects).
typedef bool (*IsAliveFunction)(HeapObject* obj, int* size, int* offset);

// Forward declarations.
class CodeFlusher;
class GCTracer;
class MarkCompactCollector;
class MarkingVisitor;
class RootMarkingVisitor;


class Marking {
 public:
  explicit Marking(Heap* heap)
      : heap_(heap) {
  }

  INLINE(static MarkBit MarkBitFrom(Address addr));

  INLINE(static MarkBit MarkBitFrom(HeapObject* obj)) {
    return MarkBitFrom(reinterpret_cast<Address>(obj));
  }

  // Impossible markbits: 01
  static const char* kImpossibleBitPattern;
  INLINE(static bool IsImpossible(MarkBit mark_bit)) {
    return !mark_bit.Get() && mark_bit.Next().Get();
  }

  // Black markbits: 10 - this is required by the sweeper.
  static const char* kBlackBitPattern;
  INLINE(static bool IsBlack(MarkBit mark_bit)) {
    return mark_bit.Get() && !mark_bit.Next().Get();
  }

  // White markbits: 00 - this is required by the mark bit clearer.
  static const char* kWhiteBitPattern;
  INLINE(static bool IsWhite(MarkBit mark_bit)) {
    return !mark_bit.Get();
  }

  // Grey markbits: 11
  static const char* kGreyBitPattern;
  INLINE(static bool IsGrey(MarkBit mark_bit)) {
    return mark_bit.Get() && mark_bit.Next().Get();
  }

  INLINE(static void MarkBlack(MarkBit mark_bit)) {
    mark_bit.Set();
    mark_bit.Next().Clear();
  }

  INLINE(static void BlackToGrey(MarkBit markbit)) {
    markbit.Next().Set();
  }

  INLINE(static void WhiteToGrey(MarkBit markbit)) {
    markbit.Set();
    markbit.Next().Set();
  }

  INLINE(static void GreyToBlack(MarkBit markbit)) {
    markbit.Next().Clear();
  }

  INLINE(static void BlackToGrey(HeapObject* obj)) {
    BlackToGrey(MarkBitFrom(obj));
  }

  INLINE(static void AnyToGrey(MarkBit markbit)) {
    markbit.Set();
    markbit.Next().Set();
  }

  void TransferMark(Address old_start, Address new_start);

#ifdef DEBUG
  enum ObjectColor {
    BLACK_OBJECT,
    WHITE_OBJECT,
    GREY_OBJECT,
    IMPOSSIBLE_COLOR
  };

  static const char* ColorName(ObjectColor color) {
    switch (color) {
      case BLACK_OBJECT: return "black";
      case WHITE_OBJECT: return "white";
      case GREY_OBJECT: return "grey";
      case IMPOSSIBLE_COLOR: return "impossible";
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
  INLINE(static bool TransferColor(HeapObject* from,
                                   HeapObject* to)) {
    MarkBit from_mark_bit = MarkBitFrom(from);
    MarkBit to_mark_bit = MarkBitFrom(to);
    bool is_black = false;
    if (from_mark_bit.Get()) {
      to_mark_bit.Set();
      is_black = true;  // Looks black so far.
    }
    if (from_mark_bit.Next().Get()) {
      to_mark_bit.Next().Set();
      is_black = false;  // Was actually gray.
    }
    return is_black;
  }

 private:
  Heap* heap_;
};

// ----------------------------------------------------------------------------
// Marking deque for tracing live objects.
class MarkingDeque {
 public:
  MarkingDeque()
      : array_(NULL), top_(0), bottom_(0), mask_(0), overflowed_(false) { }

  void Initialize(Address low, Address high) {
    HeapObject** obj_low = reinterpret_cast<HeapObject**>(low);
    HeapObject** obj_high = reinterpret_cast<HeapObject**>(high);
    array_ = obj_low;
    mask_ = RoundDownToPowerOf2(static_cast<int>(obj_high - obj_low)) - 1;
    top_ = bottom_ = 0;
    overflowed_ = false;
  }

  inline bool IsFull() { return ((top_ + 1) & mask_) == bottom_; }

  inline bool IsEmpty() { return top_ == bottom_; }

  bool overflowed() const { return overflowed_; }

  void ClearOverflowed() { overflowed_ = false; }

  void SetOverflowed() { overflowed_ = true; }

  // Push the (marked) object on the marking stack if there is room,
  // otherwise mark the object as overflowed and wait for a rescan of the
  // heap.
  INLINE(void PushBlack(HeapObject* object)) {
    ASSERT(object->IsHeapObject());
    if (IsFull()) {
      Marking::BlackToGrey(object);
      MemoryChunk::IncrementLiveBytesFromGC(object->address(), -object->Size());
      SetOverflowed();
    } else {
      array_[top_] = object;
      top_ = ((top_ + 1) & mask_);
    }
  }

  INLINE(void PushGrey(HeapObject* object)) {
    ASSERT(object->IsHeapObject());
    if (IsFull()) {
      SetOverflowed();
    } else {
      array_[top_] = object;
      top_ = ((top_ + 1) & mask_);
    }
  }

  INLINE(HeapObject* Pop()) {
    ASSERT(!IsEmpty());
    top_ = ((top_ - 1) & mask_);
    HeapObject* object = array_[top_];
    ASSERT(object->IsHeapObject());
    return object;
  }

  INLINE(void UnshiftGrey(HeapObject* object)) {
    ASSERT(object->IsHeapObject());
    if (IsFull()) {
      SetOverflowed();
    } else {
      bottom_ = ((bottom_ - 1) & mask_);
      array_[bottom_] = object;
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

  DISALLOW_COPY_AND_ASSIGN(MarkingDeque);
};


class SlotsBufferAllocator {
 public:
  SlotsBuffer* AllocateBuffer(SlotsBuffer* next_buffer);
  void DeallocateBuffer(SlotsBuffer* buffer);

  void DeallocateChain(SlotsBuffer** buffer_address);
};


// SlotsBuffer records a sequence of slots that has to be updated
// after live objects were relocated from evacuation candidates.
// All slots are either untyped or typed:
//    - Untyped slots are expected to contain a tagged object pointer.
//      They are recorded by an address.
//    - Typed slots are expected to contain an encoded pointer to a heap
//      object where the way of encoding depends on the type of the slot.
//      They are recorded as a pair (SlotType, slot address).
// We assume that zero-page is never mapped this allows us to distinguish
// untyped slots from typed slots during iteration by a simple comparison:
// if element of slots buffer is less than NUMBER_OF_SLOT_TYPES then it
// is the first element of typed slot's pair.
class SlotsBuffer {
 public:
  typedef Object** ObjectSlot;

  explicit SlotsBuffer(SlotsBuffer* next_buffer)
      : idx_(0), chain_length_(1), next_(next_buffer) {
    if (next_ != NULL) {
      chain_length_ = next_->chain_length_ + 1;
    }
  }

  ~SlotsBuffer() {
  }

  void Add(ObjectSlot slot) {
    ASSERT(0 <= idx_ && idx_ < kNumberOfElements);
    slots_[idx_++] = slot;
  }

  enum SlotType {
    EMBEDDED_OBJECT_SLOT,
    RELOCATED_CODE_OBJECT,
    CODE_TARGET_SLOT,
    CODE_ENTRY_SLOT,
    DEBUG_TARGET_SLOT,
    JS_RETURN_SLOT,
    NUMBER_OF_SLOT_TYPES
  };

  static const char* SlotTypeToString(SlotType type) {
    switch (type) {
      case EMBEDDED_OBJECT_SLOT:
        return "EMBEDDED_OBJECT_SLOT";
      case RELOCATED_CODE_OBJECT:
        return "RELOCATED_CODE_OBJECT";
      case CODE_TARGET_SLOT:
        return "CODE_TARGET_SLOT";
      case CODE_ENTRY_SLOT:
        return "CODE_ENTRY_SLOT";
      case DEBUG_TARGET_SLOT:
        return "DEBUG_TARGET_SLOT";
      case JS_RETURN_SLOT:
        return "JS_RETURN_SLOT";
      case NUMBER_OF_SLOT_TYPES:
        return "NUMBER_OF_SLOT_TYPES";
    }
    return "UNKNOWN SlotType";
  }

  void UpdateSlots(Heap* heap);

  void UpdateSlotsWithFilter(Heap* heap);

  SlotsBuffer* next() { return next_; }

  static int SizeOfChain(SlotsBuffer* buffer) {
    if (buffer == NULL) return 0;
    return static_cast<int>(buffer->idx_ +
                            (buffer->chain_length_ - 1) * kNumberOfElements);
  }

  inline bool IsFull() {
    return idx_ == kNumberOfElements;
  }

  inline bool HasSpaceForTypedSlot() {
    return idx_ < kNumberOfElements - 1;
  }

  static void UpdateSlotsRecordedIn(Heap* heap,
                                    SlotsBuffer* buffer,
                                    bool code_slots_filtering_required) {
    while (buffer != NULL) {
      if (code_slots_filtering_required) {
        buffer->UpdateSlotsWithFilter(heap);
      } else {
        buffer->UpdateSlots(heap);
      }
      buffer = buffer->next();
    }
  }

  enum AdditionMode {
    FAIL_ON_OVERFLOW,
    IGNORE_OVERFLOW
  };

  static bool ChainLengthThresholdReached(SlotsBuffer* buffer) {
    return buffer != NULL && buffer->chain_length_ >= kChainLengthThreshold;
  }

  INLINE(static bool AddTo(SlotsBufferAllocator* allocator,
                           SlotsBuffer** buffer_address,
                           ObjectSlot slot,
                           AdditionMode mode)) {
    SlotsBuffer* buffer = *buffer_address;
    if (buffer == NULL || buffer->IsFull()) {
      if (mode == FAIL_ON_OVERFLOW && ChainLengthThresholdReached(buffer)) {
        allocator->DeallocateChain(buffer_address);
        return false;
      }
      buffer = allocator->AllocateBuffer(buffer);
      *buffer_address = buffer;
    }
    buffer->Add(slot);
    return true;
  }

  static bool IsTypedSlot(ObjectSlot slot);

  static bool AddTo(SlotsBufferAllocator* allocator,
                    SlotsBuffer** buffer_address,
                    SlotType type,
                    Address addr,
                    AdditionMode mode);

  static const int kNumberOfElements = 1021;

 private:
  static const int kChainLengthThreshold = 15;

  intptr_t idx_;
  intptr_t chain_length_;
  SlotsBuffer* next_;
  ObjectSlot slots_[kNumberOfElements];
};


// CodeFlusher collects candidates for code flushing during marking and
// processes those candidates after marking has completed in order to
// reset those functions referencing code objects that would otherwise
// be unreachable. Code objects can be referenced in three ways:
//    - SharedFunctionInfo references unoptimized code.
//    - JSFunction references either unoptimized or optimized code.
//    - OptimizedCodeMap references optimized code.
// We are not allowed to flush unoptimized code for functions that got
// optimized or inlined into optimized code, because we might bailout
// into the unoptimized code again during deoptimization.
class CodeFlusher {
 public:
  explicit CodeFlusher(Isolate* isolate)
      : isolate_(isolate),
        jsfunction_candidates_head_(NULL),
        shared_function_info_candidates_head_(NULL),
        optimized_code_map_holder_head_(NULL) {}

  void AddCandidate(SharedFunctionInfo* shared_info) {
    if (GetNextCandidate(shared_info) == NULL) {
      SetNextCandidate(shared_info, shared_function_info_candidates_head_);
      shared_function_info_candidates_head_ = shared_info;
    }
  }

  void AddCandidate(JSFunction* function) {
    ASSERT(function->code() == function->shared()->code());
    if (GetNextCandidate(function)->IsUndefined()) {
      SetNextCandidate(function, jsfunction_candidates_head_);
      jsfunction_candidates_head_ = function;
    }
  }

  void AddOptimizedCodeMap(SharedFunctionInfo* code_map_holder) {
    if (GetNextCodeMap(code_map_holder)->IsUndefined()) {
      SetNextCodeMap(code_map_holder, optimized_code_map_holder_head_);
      optimized_code_map_holder_head_ = code_map_holder;
    }
  }

  void EvictOptimizedCodeMap(SharedFunctionInfo* code_map_holder);
  void EvictCandidate(SharedFunctionInfo* shared_info);
  void EvictCandidate(JSFunction* function);

  void ProcessCandidates() {
    ProcessOptimizedCodeMaps();
    ProcessSharedFunctionInfoCandidates();
    ProcessJSFunctionCandidates();
  }

  void EvictAllCandidates() {
    EvictOptimizedCodeMaps();
    EvictJSFunctionCandidates();
    EvictSharedFunctionInfoCandidates();
  }

  void IteratePointersToFromSpace(ObjectVisitor* v);

 private:
  void ProcessOptimizedCodeMaps();
  void ProcessJSFunctionCandidates();
  void ProcessSharedFunctionInfoCandidates();
  void EvictOptimizedCodeMaps();
  void EvictJSFunctionCandidates();
  void EvictSharedFunctionInfoCandidates();

  static JSFunction** GetNextCandidateSlot(JSFunction* candidate) {
    return reinterpret_cast<JSFunction**>(
        HeapObject::RawField(candidate, JSFunction::kNextFunctionLinkOffset));
  }

  static JSFunction* GetNextCandidate(JSFunction* candidate) {
    Object* next_candidate = candidate->next_function_link();
    return reinterpret_cast<JSFunction*>(next_candidate);
  }

  static void SetNextCandidate(JSFunction* candidate,
                               JSFunction* next_candidate) {
    candidate->set_next_function_link(next_candidate);
  }

  static void ClearNextCandidate(JSFunction* candidate, Object* undefined) {
    ASSERT(undefined->IsUndefined());
    candidate->set_next_function_link(undefined, SKIP_WRITE_BARRIER);
  }

  static SharedFunctionInfo* GetNextCandidate(SharedFunctionInfo* candidate) {
    Object* next_candidate = candidate->code()->gc_metadata();
    return reinterpret_cast<SharedFunctionInfo*>(next_candidate);
  }

  static void SetNextCandidate(SharedFunctionInfo* candidate,
                               SharedFunctionInfo* next_candidate) {
    candidate->code()->set_gc_metadata(next_candidate);
  }

  static void ClearNextCandidate(SharedFunctionInfo* candidate) {
    candidate->code()->set_gc_metadata(NULL, SKIP_WRITE_BARRIER);
  }

  static SharedFunctionInfo* GetNextCodeMap(SharedFunctionInfo* holder) {
    FixedArray* code_map = FixedArray::cast(holder->optimized_code_map());
    Object* next_map = code_map->get(SharedFunctionInfo::kNextMapIndex);
    return reinterpret_cast<SharedFunctionInfo*>(next_map);
  }

  static void SetNextCodeMap(SharedFunctionInfo* holder,
                             SharedFunctionInfo* next_holder) {
    FixedArray* code_map = FixedArray::cast(holder->optimized_code_map());
    code_map->set(SharedFunctionInfo::kNextMapIndex, next_holder);
  }

  static void ClearNextCodeMap(SharedFunctionInfo* holder) {
    FixedArray* code_map = FixedArray::cast(holder->optimized_code_map());
    code_map->set_undefined(SharedFunctionInfo::kNextMapIndex);
  }

  Isolate* isolate_;
  JSFunction* jsfunction_candidates_head_;
  SharedFunctionInfo* shared_function_info_candidates_head_;
  SharedFunctionInfo* optimized_code_map_holder_head_;

  DISALLOW_COPY_AND_ASSIGN(CodeFlusher);
};


// Defined in isolate.h.
class ThreadLocalTop;


// -------------------------------------------------------------------------
// Mark-Compact collector
class MarkCompactCollector {
 public:
  // Type of functions to compute forwarding addresses of objects in
  // compacted spaces.  Given an object and its size, return a (non-failure)
  // Object* that will be the object after forwarding.  There is a separate
  // allocation function for each (compactable) space based on the location
  // of the object before compaction.
  typedef MaybeObject* (*AllocationFunction)(Heap* heap,
                                             HeapObject* object,
                                             int object_size);

  // Type of functions to encode the forwarding address for an object.
  // Given the object, its size, and the new (non-failure) object it will be
  // forwarded to, encode the forwarding address.  For paged spaces, the
  // 'offset' input/output parameter contains the offset of the forwarded
  // object from the forwarding address of the previous live object in the
  // page as input, and is updated to contain the offset to be used for the
  // next live object in the same page.  For spaces using a different
  // encoding (i.e., contiguous spaces), the offset parameter is ignored.
  typedef void (*EncodingFunction)(Heap* heap,
                                   HeapObject* old_object,
                                   int object_size,
                                   Object* new_object,
                                   int* offset);

  // Type of functions to process non-live objects.
  typedef void (*ProcessNonLiveFunction)(HeapObject* object, Isolate* isolate);

  // Pointer to member function, used in IterateLiveObjects.
  typedef int (MarkCompactCollector::*LiveObjectCallback)(HeapObject* obj);

  // Set the global flags, it must be called before Prepare to take effect.
  inline void SetFlags(int flags);

  static void Initialize();

  void SetUp();

  void TearDown();

  void CollectEvacuationCandidates(PagedSpace* space);

  void AddEvacuationCandidate(Page* p);

  // Prepares for GC by resetting relocation info in old and map spaces and
  // choosing spaces to compact.
  void Prepare(GCTracer* tracer);

  // Performs a global garbage collection.
  void CollectGarbage();

  enum CompactionMode {
    INCREMENTAL_COMPACTION,
    NON_INCREMENTAL_COMPACTION
  };

  bool StartCompaction(CompactionMode mode);

  void AbortCompaction();

  // During a full GC, there is a stack-allocated GCTracer that is used for
  // bookkeeping information.  Return a pointer to that tracer.
  GCTracer* tracer() { return tracer_; }

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

  inline Heap* heap() const { return heap_; }
  inline Isolate* isolate() const;

  CodeFlusher* code_flusher() { return code_flusher_; }
  inline bool is_code_flushing_enabled() const { return code_flusher_ != NULL; }
  void EnableCodeFlushing(bool enable);

  enum SweeperType {
    CONSERVATIVE,
    LAZY_CONSERVATIVE,
    PARALLEL_CONSERVATIVE,
    CONCURRENT_CONSERVATIVE,
    PRECISE
  };

  enum SweepingParallelism {
    SWEEP_SEQUENTIALLY,
    SWEEP_IN_PARALLEL
  };

#ifdef VERIFY_HEAP
  void VerifyMarkbitsAreClean();
  static void VerifyMarkbitsAreClean(PagedSpace* space);
  static void VerifyMarkbitsAreClean(NewSpace* space);
  void VerifyWeakEmbeddedObjectsInOptimizedCode();
  void VerifyOmittedMapChecks();
#endif

  // Sweep a single page from the given space conservatively.
  // Return a number of reclaimed bytes.
  template<SweepingParallelism type>
  static intptr_t SweepConservatively(PagedSpace* space,
                                      FreeList* free_list,
                                      Page* p);

  INLINE(static bool ShouldSkipEvacuationSlotRecording(Object** anchor)) {
    return Page::FromAddress(reinterpret_cast<Address>(anchor))->
        ShouldSkipEvacuationSlotRecording();
  }

  INLINE(static bool ShouldSkipEvacuationSlotRecording(Object* host)) {
    return Page::FromAddress(reinterpret_cast<Address>(host))->
        ShouldSkipEvacuationSlotRecording();
  }

  INLINE(static bool IsOnEvacuationCandidate(Object* obj)) {
    return Page::FromAddress(reinterpret_cast<Address>(obj))->
        IsEvacuationCandidate();
  }

  INLINE(void EvictEvacuationCandidate(Page* page)) {
    if (FLAG_trace_fragmentation) {
      PrintF("Page %p is too popular. Disabling evacuation.\n",
             reinterpret_cast<void*>(page));
    }

    // TODO(gc) If all evacuation candidates are too popular we
    // should stop slots recording entirely.
    page->ClearEvacuationCandidate();

    // We were not collecting slots on this page that point
    // to other evacuation candidates thus we have to
    // rescan the page after evacuation to discover and update all
    // pointers to evacuated objects.
    if (page->owner()->identity() == OLD_DATA_SPACE) {
      evacuation_candidates_.RemoveElement(page);
    } else {
      page->SetFlag(Page::RESCAN_ON_EVACUATION);
    }
  }

  void RecordRelocSlot(RelocInfo* rinfo, Object* target);
  void RecordCodeEntrySlot(Address slot, Code* target);
  void RecordCodeTargetPatch(Address pc, Code* target);

  INLINE(void RecordSlot(Object** anchor_slot,
                         Object** slot,
                         Object* object,
                         SlotsBuffer::AdditionMode mode =
                             SlotsBuffer::FAIL_ON_OVERFLOW));

  void MigrateObject(HeapObject* dst,
                     HeapObject* src,
                     int size,
                     AllocationSpace to_old_space);

  bool TryPromoteObject(HeapObject* object, int object_size);

  inline Object* encountered_weak_collections() {
    return encountered_weak_collections_;
  }
  inline void set_encountered_weak_collections(Object* weak_collection) {
    encountered_weak_collections_ = weak_collection;
  }

  void InvalidateCode(Code* code);

  void ClearMarkbits();

  bool abort_incremental_marking() const { return abort_incremental_marking_; }

  bool is_compacting() const { return compacting_; }

  MarkingParity marking_parity() { return marking_parity_; }

  // Concurrent and parallel sweeping support.
  void SweepInParallel(PagedSpace* space);

  void WaitUntilSweepingCompleted();

  intptr_t RefillFreeLists(PagedSpace* space);

  bool AreSweeperThreadsActivated();

  bool IsConcurrentSweepingInProgress();

  void set_sequential_sweeping(bool sequential_sweeping) {
    sequential_sweeping_ = sequential_sweeping;
  }

  bool sequential_sweeping() const {
    return sequential_sweeping_;
  }

  // Mark the global table which maps weak objects to dependent code without
  // marking its contents.
  void MarkWeakObjectToCodeTable();

  // Special case for processing weak references in a full collection. We need
  // to artifically keep AllocationSites alive for a time.
  void MarkAllocationSite(AllocationSite* site);

 private:
  class SweeperTask;

  explicit MarkCompactCollector(Heap* heap);
  ~MarkCompactCollector();

  bool MarkInvalidatedCode();
  bool WillBeDeoptimized(Code* code);
  void RemoveDeadInvalidatedCode();
  void ProcessInvalidatedCode(ObjectVisitor* visitor);

  void UnlinkEvacuationCandidates();
  void ReleaseEvacuationCandidates();

  void StartSweeperThreads();

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

  // Global flag that forces sweeping to be precise, so we can traverse the
  // heap.
  bool sweep_precisely_;

  bool reduce_memory_footprint_;

  bool abort_incremental_marking_;

  MarkingParity marking_parity_;

  // True if we are collecting slots to perform evacuation from evacuation
  // candidates.
  bool compacting_;

  bool was_marked_incrementally_;

  // True if concurrent or parallel sweeping is currently in progress.
  bool sweeping_pending_;

  Semaphore pending_sweeper_jobs_semaphore_;

  bool sequential_sweeping_;

  // A pointer to the current stack-allocated GC tracer object during a full
  // collection (NULL before and after).
  GCTracer* tracer_;

  SlotsBufferAllocator slots_buffer_allocator_;

  SlotsBuffer* migration_slots_buffer_;

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

  friend class RootMarkingVisitor;
  friend class MarkingVisitor;
  friend class MarkCompactMarkingVisitor;
  friend class CodeMarkingVisitor;
  friend class SharedFunctionInfoMarkingVisitor;

  // Mark code objects that are active on the stack to prevent them
  // from being flushed.
  void PrepareThreadForCodeFlushing(Isolate* isolate, ThreadLocalTop* top);

  void PrepareForCodeFlushing();

  // Marking operations for objects reachable from roots.
  void MarkLiveObjects();

  void AfterMarking();

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

  // Mark objects in implicit references groups if their parent object
  // is marked.
  void MarkImplicitRefGroups();

  // Mark objects reachable (transitively) from objects in the marking stack
  // or overflowed in the heap.
  void ProcessMarkingDeque();

  // Mark objects reachable (transitively) from objects in the marking stack
  // or overflowed in the heap.  This respects references only considered in
  // the final atomic marking pause including the following:
  //    - Processing of objects reachable through Harmony WeakMaps.
  //    - Objects reachable due to host application logic like object groups
  //      or implicit references' groups.
  void ProcessEphemeralMarking(ObjectVisitor* visitor);

  // If the call-site of the top optimized code was not prepared for
  // deoptimization, then treat the maps in the code as strong pointers,
  // otherwise a map can die and deoptimize the code.
  void ProcessTopOptimizedFrame(ObjectVisitor* visitor);

  // Mark objects reachable (transitively) from objects in the marking
  // stack.  This function empties the marking stack, but may leave
  // overflowed objects in the heap, in which case the marking stack's
  // overflow flag will be set.
  void EmptyMarkingDeque();

  // Refill the marking stack with overflowed objects from the heap.  This
  // function either leaves the marking stack full or clears the overflow
  // flag on the marking stack.
  void RefillMarkingDeque();

  // After reachable maps have been marked process per context object
  // literal map caches removing unmarked entries.
  void ProcessMapCaches();

  // Callback function for telling whether the object *p is an unmarked
  // heap object.
  static bool IsUnmarkedHeapObject(Object** p);
  static bool IsUnmarkedHeapObjectWithHeap(Heap* heap, Object** p);

  // Map transitions from a live map to a dead map must be killed.
  // We replace them with a null descriptor, with the same key.
  void ClearNonLiveReferences();
  void ClearNonLivePrototypeTransitions(Map* map);
  void ClearNonLiveMapTransitions(Map* map, MarkBit map_mark);

  void ClearAndDeoptimizeDependentCode(DependentCode* dependent_code);
  void ClearNonLiveDependentCode(DependentCode* dependent_code);

  // Marking detaches initial maps from SharedFunctionInfo objects
  // to make this reference weak. We need to reattach initial maps
  // back after collection. This is either done during
  // ClearNonLiveTransitions pass or by calling this function.
  void ReattachInitialMaps();

  // Mark all values associated with reachable keys in weak collections
  // encountered so far.  This might push new object or even new weak maps onto
  // the marking stack.
  void ProcessWeakCollections();

  // After all reachable objects have been marked those weak map entries
  // with an unreachable key are removed from all encountered weak maps.
  // The linked list of all encountered weak maps is destroyed.
  void ClearWeakCollections();

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

  int DiscoverAndPromoteBlackObjectsOnPage(NewSpace* new_space,
                                           NewSpacePage* p);

  void EvacuateNewSpace();

  void EvacuateLiveObjectsFromPage(Page* p);

  void EvacuatePages();

  void EvacuateNewSpaceAndCandidates();

  void SweepSpace(PagedSpace* space, SweeperType sweeper);

  // Finalizes the parallel sweeping phase. Marks all the pages that were
  // swept in parallel.
  void ParallelSweepSpacesComplete();

  void ParallelSweepSpaceComplete(PagedSpace* space);

#ifdef DEBUG
  friend class MarkObjectVisitor;
  static void VisitObject(HeapObject* obj);

  friend class UnmarkObjectVisitor;
  static void UnmarkObject(HeapObject* obj);
#endif

  Heap* heap_;
  MarkingDeque marking_deque_;
  CodeFlusher* code_flusher_;
  Object* encountered_weak_collections_;
  bool have_code_to_deoptimize_;

  List<Page*> evacuation_candidates_;
  List<Code*> invalidated_code_;

  SmartPointer<FreeList> free_list_old_data_space_;
  SmartPointer<FreeList> free_list_old_pointer_space_;

  friend class Heap;
};


class MarkBitCellIterator BASE_EMBEDDED {
 public:
  explicit MarkBitCellIterator(MemoryChunk* chunk)
      : chunk_(chunk) {
    last_cell_index_ = Bitmap::IndexToCell(
        Bitmap::CellAlignIndex(
            chunk_->AddressToMarkbitIndex(chunk_->area_end())));
    cell_base_ = chunk_->area_start();
    cell_index_ = Bitmap::IndexToCell(
        Bitmap::CellAlignIndex(
            chunk_->AddressToMarkbitIndex(cell_base_)));
    cells_ = chunk_->markbits()->cells();
  }

  inline bool Done() { return cell_index_ == last_cell_index_; }

  inline bool HasNext() { return cell_index_ < last_cell_index_ - 1; }

  inline MarkBit::CellType* CurrentCell() {
    ASSERT(cell_index_ == Bitmap::IndexToCell(Bitmap::CellAlignIndex(
        chunk_->AddressToMarkbitIndex(cell_base_))));
    return &cells_[cell_index_];
  }

  inline Address CurrentCellBase() {
    ASSERT(cell_index_ == Bitmap::IndexToCell(Bitmap::CellAlignIndex(
        chunk_->AddressToMarkbitIndex(cell_base_))));
    return cell_base_;
  }

  inline void Advance() {
    cell_index_++;
    cell_base_ += 32 * kPointerSize;
  }

 private:
  MemoryChunk* chunk_;
  MarkBit::CellType* cells_;
  unsigned int last_cell_index_;
  unsigned int cell_index_;
  Address cell_base_;
};


class SequentialSweepingScope BASE_EMBEDDED {
 public:
  explicit SequentialSweepingScope(MarkCompactCollector *collector) :
    collector_(collector) {
    collector_->set_sequential_sweeping(true);
  }

  ~SequentialSweepingScope() {
    collector_->set_sequential_sweeping(false);
  }

 private:
  MarkCompactCollector* collector_;
};


const char* AllocationSpaceName(AllocationSpace space);

} }  // namespace v8::internal

#endif  // V8_MARK_COMPACT_H_

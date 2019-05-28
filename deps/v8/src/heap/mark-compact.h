// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_H_
#define V8_HEAP_MARK_COMPACT_H_

#include <atomic>
#include <vector>

#include "src/heap/concurrent-marking.h"
#include "src/heap/marking.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/spaces.h"
#include "src/heap/sweeper.h"
#include "src/heap/worklist.h"
#include "src/objects/heap-object.h"   // For Worklist<HeapObject, ...>
#include "src/objects/js-weak-refs.h"  // For Worklist<WeakCell, ...>

namespace v8 {
namespace internal {

// Forward declarations.
class EvacuationJobTraits;
class HeapObjectVisitor;
class ItemParallelJob;
class MigrationObserver;
class RecordMigratedSlotVisitor;
class UpdatingItem;
class YoungGenerationMarkingVisitor;

template <typename ConcreteState, AccessMode access_mode>
class MarkingStateBase {
 public:
  V8_INLINE MarkBit MarkBitFrom(HeapObject obj) {
    return MarkBitFrom(MemoryChunk::FromHeapObject(obj), obj->ptr());
  }

  // {addr} may be tagged or aligned.
  V8_INLINE MarkBit MarkBitFrom(MemoryChunk* p, Address addr) {
    return static_cast<ConcreteState*>(this)->bitmap(p)->MarkBitFromIndex(
        p->AddressToMarkbitIndex(addr));
  }

  Marking::ObjectColor Color(HeapObject obj) {
    return Marking::Color(MarkBitFrom(obj));
  }

  V8_INLINE bool IsImpossible(HeapObject obj) {
    return Marking::IsImpossible<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsBlack(HeapObject obj) {
    return Marking::IsBlack<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsWhite(HeapObject obj) {
    return Marking::IsWhite<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsGrey(HeapObject obj) {
    return Marking::IsGrey<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsBlackOrGrey(HeapObject obj) {
    return Marking::IsBlackOrGrey<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool WhiteToGrey(HeapObject obj);
  V8_INLINE bool WhiteToBlack(HeapObject obj);
  V8_INLINE bool GreyToBlack(HeapObject obj);

  void ClearLiveness(MemoryChunk* chunk) {
    static_cast<ConcreteState*>(this)->bitmap(chunk)->Clear();
    static_cast<ConcreteState*>(this)->SetLiveBytes(chunk, 0);
  }
};

class MarkBitCellIterator {
 public:
  MarkBitCellIterator(MemoryChunk* chunk, Bitmap* bitmap) : chunk_(chunk) {
    last_cell_index_ =
        Bitmap::IndexToCell(chunk_->AddressToMarkbitIndex(chunk_->area_end()));
    cell_base_ = chunk_->address();
    cell_index_ =
        Bitmap::IndexToCell(chunk_->AddressToMarkbitIndex(cell_base_));
    cells_ = bitmap->cells();
  }

  inline bool Done() { return cell_index_ >= last_cell_index_; }

  inline bool HasNext() { return cell_index_ < last_cell_index_ - 1; }

  inline MarkBit::CellType* CurrentCell() {
    DCHECK_EQ(cell_index_, Bitmap::IndexToCell(Bitmap::CellAlignIndex(
                               chunk_->AddressToMarkbitIndex(cell_base_))));
    return &cells_[cell_index_];
  }

  inline Address CurrentCellBase() {
    DCHECK_EQ(cell_index_, Bitmap::IndexToCell(Bitmap::CellAlignIndex(
                               chunk_->AddressToMarkbitIndex(cell_base_))));
    return cell_base_;
  }

  V8_WARN_UNUSED_RESULT inline bool Advance() {
    cell_base_ += Bitmap::kBitsPerCell * kTaggedSize;
    return ++cell_index_ != last_cell_index_;
  }

  inline bool Advance(unsigned int new_cell_index) {
    if (new_cell_index != cell_index_) {
      DCHECK_GT(new_cell_index, cell_index_);
      DCHECK_LE(new_cell_index, last_cell_index_);
      unsigned int diff = new_cell_index - cell_index_;
      cell_index_ = new_cell_index;
      cell_base_ += diff * (Bitmap::kBitsPerCell * kTaggedSize);
      return true;
    }
    return false;
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

enum LiveObjectIterationMode {
  kBlackObjects,
  kGreyObjects,
  kAllLiveObjects
};

template <LiveObjectIterationMode mode>
class LiveObjectRange {
 public:
  class iterator {
   public:
    using value_type = std::pair<HeapObject, int /* size */>;
    using pointer = const value_type*;
    using reference = const value_type&;
    using iterator_category = std::forward_iterator_tag;

    inline iterator(MemoryChunk* chunk, Bitmap* bitmap, Address start);

    inline iterator& operator++();
    inline iterator operator++(int);

    bool operator==(iterator other) const {
      return current_object_ == other.current_object_;
    }

    bool operator!=(iterator other) const { return !(*this == other); }

    value_type operator*() {
      return std::make_pair(current_object_, current_size_);
    }

   private:
    inline void AdvanceToNextValidObject();

    MemoryChunk* const chunk_;
    Map const one_word_filler_map_;
    Map const two_word_filler_map_;
    Map const free_space_map_;
    MarkBitCellIterator it_;
    Address cell_base_;
    MarkBit::CellType current_cell_;
    HeapObject current_object_;
    int current_size_;
  };

  LiveObjectRange(MemoryChunk* chunk, Bitmap* bitmap)
      : chunk_(chunk),
        bitmap_(bitmap),
        start_(chunk_->area_start()),
        end_(chunk->area_end()) {
    DCHECK(!chunk->IsLargePage());
  }

  inline iterator begin();
  inline iterator end();

 private:
  MemoryChunk* const chunk_;
  Bitmap* bitmap_;
  Address start_;
  Address end_;
};

class LiveObjectVisitor : AllStatic {
 public:
  enum IterationMode {
    kKeepMarking,
    kClearMarkbits,
  };

  // Visits black objects on a MemoryChunk until the Visitor returns |false| for
  // an object. If IterationMode::kClearMarkbits is passed the markbits and
  // slots for visited objects are cleared for each successfully visited object.
  template <class Visitor, typename MarkingState>
  static bool VisitBlackObjects(MemoryChunk* chunk, MarkingState* state,
                                Visitor* visitor, IterationMode iteration_mode,
                                HeapObject* failed_object);

  // Visits black objects on a MemoryChunk. The visitor is not allowed to fail
  // visitation for an object.
  template <class Visitor, typename MarkingState>
  static void VisitBlackObjectsNoFail(MemoryChunk* chunk, MarkingState* state,
                                      Visitor* visitor,
                                      IterationMode iteration_mode);

  // Visits black objects on a MemoryChunk. The visitor is not allowed to fail
  // visitation for an object.
  template <class Visitor, typename MarkingState>
  static void VisitGreyObjectsNoFail(MemoryChunk* chunk, MarkingState* state,
                                     Visitor* visitor,
                                     IterationMode iteration_mode);

  template <typename MarkingState>
  static void RecomputeLiveBytes(MemoryChunk* chunk, MarkingState* state);
};

enum PageEvacuationMode { NEW_TO_NEW, NEW_TO_OLD };
enum MarkingTreatmentMode { KEEP, CLEAR };
enum class RememberedSetUpdatingMode { ALL, OLD_TO_NEW_ONLY };

// Base class for minor and full MC collectors.
class MarkCompactCollectorBase {
 public:
  static const int kMainThread = 0;

  virtual ~MarkCompactCollectorBase() = default;

  virtual void SetUp() = 0;
  virtual void TearDown() = 0;
  virtual void CollectGarbage() = 0;

  inline Heap* heap() const { return heap_; }
  inline Isolate* isolate();

 protected:
  explicit MarkCompactCollectorBase(Heap* heap)
      : heap_(heap), old_to_new_slots_(0) {}

  // Marking operations for objects reachable from roots.
  virtual void MarkLiveObjects() = 0;
  // Mark objects reachable (transitively) from objects in the marking
  // work list.
  virtual void ProcessMarkingWorklist() = 0;
  // Clear non-live references held in side data structures.
  virtual void ClearNonLiveReferences() = 0;
  virtual void EvacuatePrologue() = 0;
  virtual void EvacuateEpilogue() = 0;
  virtual void Evacuate() = 0;
  virtual void EvacuatePagesInParallel() = 0;
  virtual void UpdatePointersAfterEvacuation() = 0;
  virtual UpdatingItem* CreateToSpaceUpdatingItem(MemoryChunk* chunk,
                                                  Address start,
                                                  Address end) = 0;
  virtual UpdatingItem* CreateRememberedSetUpdatingItem(
      MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) = 0;

  template <class Evacuator, class Collector>
  void CreateAndExecuteEvacuationTasks(Collector* collector,
                                       ItemParallelJob* job,
                                       MigrationObserver* migration_observer,
                                       const intptr_t live_bytes);

  // Returns whether this page should be moved according to heuristics.
  bool ShouldMovePage(Page* p, intptr_t live_bytes);

  int CollectToSpaceUpdatingItems(ItemParallelJob* job);
  template <typename IterateableSpace>
  int CollectRememberedSetUpdatingItems(ItemParallelJob* job,
                                        IterateableSpace* space,
                                        RememberedSetUpdatingMode mode);

  int NumberOfParallelCompactionTasks(int pages);
  int NumberOfParallelPointerUpdateTasks(int pages, int slots);
  int NumberOfParallelToSpacePointerUpdateTasks(int pages);

  Heap* heap_;
  // Number of old to new slots. Should be computed during MarkLiveObjects.
  // -1 indicates that the value couldn't be computed.
  int old_to_new_slots_;
};

class MinorMarkingState final
    : public MarkingStateBase<MinorMarkingState, AccessMode::ATOMIC> {
 public:
  ConcurrentBitmap<AccessMode::ATOMIC>* bitmap(const MemoryChunk* chunk) const {
    return chunk->young_generation_bitmap<AccessMode::ATOMIC>();
  }

  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    chunk->young_generation_live_byte_count_ += by;
  }

  intptr_t live_bytes(MemoryChunk* chunk) const {
    return chunk->young_generation_live_byte_count_;
  }

  void SetLiveBytes(MemoryChunk* chunk, intptr_t value) {
    chunk->young_generation_live_byte_count_ = value;
  }
};

class MinorNonAtomicMarkingState final
    : public MarkingStateBase<MinorNonAtomicMarkingState,
                              AccessMode::NON_ATOMIC> {
 public:
  ConcurrentBitmap<AccessMode::NON_ATOMIC>* bitmap(
      const MemoryChunk* chunk) const {
    return chunk->young_generation_bitmap<AccessMode::NON_ATOMIC>();
  }

  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    chunk->young_generation_live_byte_count_.fetch_add(
        by, std::memory_order_relaxed);
  }

  intptr_t live_bytes(MemoryChunk* chunk) const {
    return chunk->young_generation_live_byte_count_.load(
        std::memory_order_relaxed);
  }

  void SetLiveBytes(MemoryChunk* chunk, intptr_t value) {
    chunk->young_generation_live_byte_count_.store(value,
                                                   std::memory_order_relaxed);
  }
};

// This marking state is used when concurrent marking is running.
class IncrementalMarkingState final
    : public MarkingStateBase<IncrementalMarkingState, AccessMode::ATOMIC> {
 public:
  ConcurrentBitmap<AccessMode::ATOMIC>* bitmap(const MemoryChunk* chunk) const {
    DCHECK_EQ(reinterpret_cast<intptr_t>(&chunk->marking_bitmap_) -
                  reinterpret_cast<intptr_t>(chunk),
              MemoryChunk::kMarkBitmapOffset);
    return chunk->marking_bitmap<AccessMode::ATOMIC>();
  }

  // Concurrent marking uses local live bytes so we may do these accesses
  // non-atomically.
  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    chunk->live_byte_count_ += by;
  }

  intptr_t live_bytes(MemoryChunk* chunk) const {
    return chunk->live_byte_count_;
  }

  void SetLiveBytes(MemoryChunk* chunk, intptr_t value) {
    chunk->live_byte_count_ = value;
  }
};

class MajorAtomicMarkingState final
    : public MarkingStateBase<MajorAtomicMarkingState, AccessMode::ATOMIC> {
 public:
  ConcurrentBitmap<AccessMode::ATOMIC>* bitmap(const MemoryChunk* chunk) const {
    DCHECK_EQ(reinterpret_cast<intptr_t>(&chunk->marking_bitmap_) -
                  reinterpret_cast<intptr_t>(chunk),
              MemoryChunk::kMarkBitmapOffset);
    return chunk->marking_bitmap<AccessMode::ATOMIC>();
  }

  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    std::atomic_fetch_add(
        reinterpret_cast<std::atomic<intptr_t>*>(&chunk->live_byte_count_), by);
  }
};

class MajorNonAtomicMarkingState final
    : public MarkingStateBase<MajorNonAtomicMarkingState,
                              AccessMode::NON_ATOMIC> {
 public:
  ConcurrentBitmap<AccessMode::NON_ATOMIC>* bitmap(
      const MemoryChunk* chunk) const {
    DCHECK_EQ(reinterpret_cast<intptr_t>(&chunk->marking_bitmap_) -
                  reinterpret_cast<intptr_t>(chunk),
              MemoryChunk::kMarkBitmapOffset);
    return chunk->marking_bitmap<AccessMode::NON_ATOMIC>();
  }

  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    chunk->live_byte_count_ += by;
  }

  intptr_t live_bytes(MemoryChunk* chunk) const {
    return chunk->live_byte_count_;
  }

  void SetLiveBytes(MemoryChunk* chunk, intptr_t value) {
    chunk->live_byte_count_ = value;
  }
};

struct Ephemeron {
  HeapObject key;
  HeapObject value;
};

using EphemeronWorklist = Worklist<Ephemeron, 64>;

// Weak objects encountered during marking.
struct WeakObjects {
  Worklist<TransitionArray, 64> transition_arrays;

  // Keep track of all EphemeronHashTables in the heap to process
  // them in the atomic pause.
  Worklist<EphemeronHashTable, 64> ephemeron_hash_tables;

  // Keep track of all ephemerons for concurrent marking tasks. Only store
  // ephemerons in these Worklists if both key and value are unreachable at the
  // moment.
  //
  // MarkCompactCollector::ProcessEphemeronsUntilFixpoint drains and fills these
  // worklists.
  //
  // current_ephemerons is used as draining worklist in the current fixpoint
  // iteration.
  EphemeronWorklist current_ephemerons;

  // Stores ephemerons to visit in the next fixpoint iteration.
  EphemeronWorklist next_ephemerons;

  // When draining the marking worklist new discovered ephemerons are pushed
  // into this worklist.
  EphemeronWorklist discovered_ephemerons;

  // TODO(marja): For old space, we only need the slot, not the host
  // object. Optimize this by adding a different storage for old space.
  Worklist<std::pair<HeapObject, HeapObjectSlot>, 64> weak_references;
  Worklist<std::pair<HeapObject, Code>, 64> weak_objects_in_code;

  Worklist<JSWeakRef, 64> js_weak_refs;
  Worklist<WeakCell, 64> weak_cells;

  Worklist<SharedFunctionInfo, 64> bytecode_flushing_candidates;
  Worklist<JSFunction, 64> flushed_js_functions;
};

struct EphemeronMarking {
  std::vector<HeapObject> newly_discovered;
  bool newly_discovered_overflowed;
  size_t newly_discovered_limit;
};

// Collector for young and old generation.
class MarkCompactCollector final : public MarkCompactCollectorBase {
 public:
#ifdef V8_CONCURRENT_MARKING
  using MarkingState = IncrementalMarkingState;
#else
  using MarkingState = MajorNonAtomicMarkingState;
#endif  // V8_CONCURRENT_MARKING

  using NonAtomicMarkingState = MajorNonAtomicMarkingState;

  // Wrapper for the shared worklist.
  class MarkingWorklist {
   public:
    using ConcurrentMarkingWorklist = Worklist<HeapObject, 64>;
    using EmbedderTracingWorklist = Worklist<HeapObject, 16>;

    // The heap parameter is not used but needed to match the sequential case.
    explicit MarkingWorklist(Heap* heap) {}

    void Push(HeapObject object) {
      bool success = shared_.Push(kMainThread, object);
      USE(success);
      DCHECK(success);
    }

    HeapObject Pop() {
      HeapObject result;
      if (shared_.Pop(kMainThread, &result)) return result;
#ifdef V8_CONCURRENT_MARKING
      // The expectation is that this work list is empty almost all the time
      // and we can thus avoid the emptiness checks by putting it last.
      if (on_hold_.Pop(kMainThread, &result)) return result;
#endif
      return HeapObject();
    }

    void Clear() {
      shared_.Clear();
      on_hold_.Clear();
      embedder_.Clear();
    }

    bool IsEmpty() {
      return shared_.IsLocalEmpty(kMainThread) &&
             on_hold_.IsLocalEmpty(kMainThread) &&
             shared_.IsGlobalPoolEmpty() && on_hold_.IsGlobalPoolEmpty();
    }

    bool IsEmbedderEmpty() {
      return embedder_.IsLocalEmpty(kMainThread) &&
             embedder_.IsGlobalPoolEmpty();
    }

    int Size() {
      return static_cast<int>(shared_.LocalSize(kMainThread) +
                              on_hold_.LocalSize(kMainThread));
    }

    // Calls the specified callback on each element of the deques and replaces
    // the element with the result of the callback. If the callback returns
    // nullptr then the element is removed from the deque.
    // The callback must accept HeapObject and return HeapObject.
    template <typename Callback>
    void Update(Callback callback) {
      shared_.Update(callback);
      on_hold_.Update(callback);
      embedder_.Update(callback);
    }

    void ShareWorkIfGlobalPoolIsEmpty() {
      if (!shared_.IsLocalEmpty(kMainThread) && shared_.IsGlobalPoolEmpty()) {
        shared_.FlushToGlobal(kMainThread);
      }
    }

    ConcurrentMarkingWorklist* shared() { return &shared_; }
    ConcurrentMarkingWorklist* on_hold() { return &on_hold_; }
    EmbedderTracingWorklist* embedder() { return &embedder_; }

    void Print() {
      PrintWorklist("shared", &shared_);
      PrintWorklist("on_hold", &on_hold_);
    }

   private:
    // Prints the stats about the global pool of the worklist.
    void PrintWorklist(const char* worklist_name,
                       ConcurrentMarkingWorklist* worklist);

    // Worklist used for most objects.
    ConcurrentMarkingWorklist shared_;

    // Concurrent marking uses this worklist to bail out of marking objects
    // in new space's linear allocation area. Used to avoid black allocation
    // for new space. This allow the compiler to remove write barriers
    // for freshly allocatd objects.
    ConcurrentMarkingWorklist on_hold_;

    // Worklist for objects that potentially require embedder tracing, i.e.,
    // these objects need to be handed over to the embedder to find the full
    // transitive closure.
    EmbedderTracingWorklist embedder_;
  };

  class RootMarkingVisitor;
  class CustomRootBodyMarkingVisitor;

  enum IterationMode {
    kKeepMarking,
    kClearMarkbits,
  };

  MarkingState* marking_state() { return &marking_state_; }

  NonAtomicMarkingState* non_atomic_marking_state() {
    return &non_atomic_marking_state_;
  }

  void SetUp() override;
  void TearDown() override;
  // Performs a global garbage collection.
  void CollectGarbage() override;

  void CollectEvacuationCandidates(PagedSpace* space);

  void AddEvacuationCandidate(Page* p);

  // Prepares for GC by resetting relocation info in old and map spaces and
  // choosing spaces to compact.
  void Prepare();

  // Stop concurrent marking (either by preempting it right away or waiting for
  // it to complete as requested by |stop_request|).
  void FinishConcurrentMarking(ConcurrentMarking::StopRequest stop_request);

  bool StartCompaction();

  void AbortCompaction();

  static inline bool IsOnEvacuationCandidate(Object obj) {
    return Page::FromAddress(obj->ptr())->IsEvacuationCandidate();
  }

  static bool IsOnEvacuationCandidate(MaybeObject obj);

  struct RecordRelocSlotInfo {
    MemoryChunk* memory_chunk;
    SlotType slot_type;
    bool should_record;
    uint32_t offset;
  };
  static RecordRelocSlotInfo PrepareRecordRelocSlot(Code host, RelocInfo* rinfo,
                                                    HeapObject target);
  static void RecordRelocSlot(Code host, RelocInfo* rinfo, HeapObject target);
  V8_INLINE static void RecordSlot(HeapObject object, ObjectSlot slot,
                                   HeapObject target);
  V8_INLINE static void RecordSlot(HeapObject object, HeapObjectSlot slot,
                                   HeapObject target);
  void RecordLiveSlotsOnPage(Page* page);

  void UpdateSlots(SlotsBuffer* buffer);
  void UpdateSlotsRecordedIn(SlotsBuffer* buffer);

  bool is_compacting() const { return compacting_; }

  // Ensures that sweeping is finished.
  //
  // Note: Can only be called safely from main thread.
  V8_EXPORT_PRIVATE void EnsureSweepingCompleted();

  // Checks if sweeping is in progress right now on any space.
  bool sweeping_in_progress() const { return sweeper_->sweeping_in_progress(); }

  void set_evacuation(bool evacuation) { evacuation_ = evacuation; }

  bool evacuation() const { return evacuation_; }

  MarkingWorklist* marking_worklist() { return &marking_worklist_; }

  WeakObjects* weak_objects() { return &weak_objects_; }

  inline void AddTransitionArray(TransitionArray array);

  void AddEphemeronHashTable(EphemeronHashTable table) {
    weak_objects_.ephemeron_hash_tables.Push(kMainThread, table);
  }

  void AddEphemeron(HeapObject key, HeapObject value) {
    weak_objects_.discovered_ephemerons.Push(kMainThread,
                                             Ephemeron{key, value});
  }

  void AddWeakReference(HeapObject host, HeapObjectSlot slot) {
    weak_objects_.weak_references.Push(kMainThread, std::make_pair(host, slot));
  }

  void AddWeakObjectInCode(HeapObject object, Code code) {
    weak_objects_.weak_objects_in_code.Push(kMainThread,
                                            std::make_pair(object, code));
  }

  void AddWeakRef(JSWeakRef weak_ref) {
    weak_objects_.js_weak_refs.Push(kMainThread, weak_ref);
  }

  void AddWeakCell(WeakCell weak_cell) {
    weak_objects_.weak_cells.Push(kMainThread, weak_cell);
  }

  inline void AddBytecodeFlushingCandidate(SharedFunctionInfo flush_candidate);
  inline void AddFlushedJSFunction(JSFunction flushed_function);

  void AddNewlyDiscovered(HeapObject object) {
    if (ephemeron_marking_.newly_discovered_overflowed) return;

    if (ephemeron_marking_.newly_discovered.size() <
        ephemeron_marking_.newly_discovered_limit) {
      ephemeron_marking_.newly_discovered.push_back(object);
    } else {
      ephemeron_marking_.newly_discovered_overflowed = true;
    }
  }

  void ResetNewlyDiscovered() {
    ephemeron_marking_.newly_discovered_overflowed = false;
    ephemeron_marking_.newly_discovered.clear();
  }

  Sweeper* sweeper() { return sweeper_; }

#ifdef DEBUG
  // Checks whether performing mark-compact collection.
  bool in_use() { return state_ > PREPARE_GC; }
  bool are_map_pointers_encoded() { return state_ == UPDATE_POINTERS; }
#endif

  void VerifyMarking();
#ifdef VERIFY_HEAP
  void VerifyValidStoreAndSlotsBufferEntries();
  void VerifyMarkbitsAreClean();
  void VerifyMarkbitsAreDirty(PagedSpace* space);
  void VerifyMarkbitsAreClean(PagedSpace* space);
  void VerifyMarkbitsAreClean(NewSpace* space);
  void VerifyMarkbitsAreClean(LargeObjectSpace* space);
#endif

  unsigned epoch() const { return epoch_; }

  explicit MarkCompactCollector(Heap* heap);
  ~MarkCompactCollector() override;

  // Used by wrapper tracing.
  V8_INLINE void MarkExternallyReferencedObject(HeapObject obj);

 private:
  void ComputeEvacuationHeuristics(size_t area_size,
                                   int* target_fragmentation_percent,
                                   size_t* max_evacuated_bytes);

  void RecordObjectStats();

  // Finishes GC, performs heap verification if enabled.
  void Finish();

  void MarkLiveObjects() override;

  // Marks the object black and adds it to the marking work list.
  // This is for non-incremental marking only.
  V8_INLINE void MarkObject(HeapObject host, HeapObject obj);

  // Marks the object black and adds it to the marking work list.
  // This is for non-incremental marking only.
  V8_INLINE void MarkRootObject(Root root, HeapObject obj);

  // Mark the heap roots and all objects reachable from them.
  void MarkRoots(RootVisitor* root_visitor,
                 ObjectVisitor* custom_root_body_visitor);

  // Mark the string table specially.  References to internalized strings from
  // the string table are weak.
  void MarkStringTable(ObjectVisitor* visitor);

  // Marks object reachable from harmony weak maps and wrapper tracing.
  void ProcessEphemeronMarking();

  // If the call-site of the top optimized code was not prepared for
  // deoptimization, then treat embedded pointers in the code as strong as
  // otherwise they can die and try to deoptimize the underlying code.
  void ProcessTopOptimizedFrame(ObjectVisitor* visitor);

  // Drains the main thread marking work list. Will mark all pending objects
  // if no concurrent threads are running.
  void ProcessMarkingWorklist() override;

  enum class MarkingWorklistProcessingMode {
    kDefault,
    kTrackNewlyDiscoveredObjects
  };

  template <MarkingWorklistProcessingMode mode>
  void ProcessMarkingWorklistInternal();

  // Implements ephemeron semantics: Marks value if key is already reachable.
  // Returns true if value was actually marked.
  bool ProcessEphemeron(HeapObject key, HeapObject value);

  // Marks ephemerons and drains marking worklist iteratively
  // until a fixpoint is reached.
  void ProcessEphemeronsUntilFixpoint();

  // Drains ephemeron and marking worklists. Single iteration of the
  // fixpoint iteration.
  bool ProcessEphemerons();

  // Mark ephemerons and drain marking worklist with a linear algorithm.
  // Only used if fixpoint iteration doesn't finish within a few iterations.
  void ProcessEphemeronsLinear();

  // Perform Wrapper Tracing if in use.
  void PerformWrapperTracing();

  // Callback function for telling whether the object *p is an unmarked
  // heap object.
  static bool IsUnmarkedHeapObject(Heap* heap, FullObjectSlot p);

  // Clear non-live references in weak cells, transition and descriptor arrays,
  // and deoptimize dependent code of non-live maps.
  void ClearNonLiveReferences() override;
  void MarkDependentCodeForDeoptimization();
  // Checks if the given weak cell is a simple transition from the parent map
  // of the given dead target. If so it clears the transition and trims
  // the descriptor array of the parent if needed.
  void ClearPotentialSimpleMapTransition(Map dead_target);
  void ClearPotentialSimpleMapTransition(Map map, Map dead_target);

  // Flushes a weakly held bytecode array from a shared function info.
  void FlushBytecodeFromSFI(SharedFunctionInfo shared_info);

  // Clears bytecode arrays that have not been executed for multiple
  // collections.
  void ClearOldBytecodeCandidates();

  // Resets any JSFunctions which have had their bytecode flushed.
  void ClearFlushedJsFunctions();

  // Compact every array in the global list of transition arrays and
  // trim the corresponding descriptor array if a transition target is non-live.
  void ClearFullMapTransitions();
  void TrimDescriptorArray(Map map, DescriptorArray descriptors);
  void TrimEnumCache(Map map, DescriptorArray descriptors);
  bool CompactTransitionArray(Map map, TransitionArray transitions,
                              DescriptorArray descriptors);

  // After all reachable objects have been marked those weak map entries
  // with an unreachable key are removed from all encountered weak maps.
  // The linked list of all encountered weak maps is destroyed.
  void ClearWeakCollections();

  // Goes through the list of encountered weak references and clears those with
  // dead values. If the value is a dead map and the parent map transitions to
  // the dead map via weak cell, then this function also clears the map
  // transition.
  void ClearWeakReferences();

  // Goes through the list of encountered JSWeakRefs and WeakCells and clears
  // those with dead values.
  void ClearJSWeakRefs();

  void AbortWeakObjects();

  // Starts sweeping of spaces by contributing on the main thread and setting
  // up other pages for sweeping. Does not start sweeper tasks.
  void StartSweepSpaces();
  void StartSweepSpace(PagedSpace* space);

  void EvacuatePrologue() override;
  void EvacuateEpilogue() override;
  void Evacuate() override;
  void EvacuatePagesInParallel() override;
  void UpdatePointersAfterEvacuation() override;

  UpdatingItem* CreateToSpaceUpdatingItem(MemoryChunk* chunk, Address start,
                                          Address end) override;
  UpdatingItem* CreateRememberedSetUpdatingItem(
      MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) override;

  int CollectNewSpaceArrayBufferTrackerItems(ItemParallelJob* job);
  int CollectOldSpaceArrayBufferTrackerItems(ItemParallelJob* job);

  void ReleaseEvacuationCandidates();
  void PostProcessEvacuationCandidates();
  void ReportAbortedEvacuationCandidate(HeapObject failed_object,
                                        MemoryChunk* chunk);

  static const int kEphemeronChunkSize = 8 * KB;

  int NumberOfParallelEphemeronVisitingTasks(size_t elements);

  void RightTrimDescriptorArray(DescriptorArray array, int descriptors_to_trim);

  base::Mutex mutex_;
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

  bool was_marked_incrementally_;

  bool evacuation_;

  // True if we are collecting slots to perform evacuation from evacuation
  // candidates.
  bool compacting_;

  bool black_allocation_;

  bool have_code_to_deoptimize_;

  MarkingWorklist marking_worklist_;
  WeakObjects weak_objects_;
  EphemeronMarking ephemeron_marking_;

  // Candidates for pages that should be evacuated.
  std::vector<Page*> evacuation_candidates_;
  // Pages that are actually processed during evacuation.
  std::vector<Page*> old_space_evacuation_pages_;
  std::vector<Page*> new_space_evacuation_pages_;
  std::vector<std::pair<HeapObject, Page*>> aborted_evacuation_candidates_;

  Sweeper* sweeper_;

  MarkingState marking_state_;
  NonAtomicMarkingState non_atomic_marking_state_;

  // Counts the number of mark-compact collections. This is used for marking
  // descriptor arrays. See NumberOfMarkedDescriptors. Only lower two bits are
  // used, so it is okay if this counter overflows and wraps around.
  unsigned epoch_ = 0;

  friend class FullEvacuator;
  friend class RecordMigratedSlotVisitor;
};

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
class MarkingVisitor final
    : public HeapVisitor<
          int,
          MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>> {
 public:
  using Parent = HeapVisitor<
      int, MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>>;

  V8_INLINE MarkingVisitor(MarkCompactCollector* collector,
                           MarkingState* marking_state);

  V8_INLINE bool ShouldVisitMapPointer() { return false; }

  V8_INLINE int VisitBytecodeArray(Map map, BytecodeArray object);
  V8_INLINE int VisitDescriptorArray(Map map, DescriptorArray object);
  V8_INLINE int VisitEphemeronHashTable(Map map, EphemeronHashTable object);
  V8_INLINE int VisitFixedArray(Map map, FixedArray object);
  V8_INLINE int VisitJSApiObject(Map map, JSObject object);
  V8_INLINE int VisitJSArrayBuffer(Map map, JSArrayBuffer object);
  V8_INLINE int VisitJSFunction(Map map, JSFunction object);
  V8_INLINE int VisitJSDataView(Map map, JSDataView object);
  V8_INLINE int VisitJSTypedArray(Map map, JSTypedArray object);
  V8_INLINE int VisitMap(Map map, Map object);
  V8_INLINE int VisitSharedFunctionInfo(Map map, SharedFunctionInfo object);
  V8_INLINE int VisitTransitionArray(Map map, TransitionArray object);
  V8_INLINE int VisitWeakCell(Map map, WeakCell object);
  V8_INLINE int VisitJSWeakRef(Map map, JSWeakRef object);

  // ObjectVisitor implementation.
  V8_INLINE void VisitPointer(HeapObject host, ObjectSlot p) final {
    VisitPointerImpl(host, p);
  }
  V8_INLINE void VisitPointer(HeapObject host, MaybeObjectSlot p) final {
    VisitPointerImpl(host, p);
  }
  V8_INLINE void VisitPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }
  V8_INLINE void VisitPointers(HeapObject host, MaybeObjectSlot start,
                               MaybeObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }
  V8_INLINE void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final;
  V8_INLINE void VisitCodeTarget(Code host, RelocInfo* rinfo) final;

  // Weak list pointers should be ignored during marking. The lists are
  // reconstructed after GC.
  void VisitCustomWeakPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {}

  V8_INLINE void VisitDescriptors(DescriptorArray descriptors,
                                  int number_of_own_descriptors);
  // Marks the descriptor array black without pushing it on the marking work
  // list and visits its header.
  V8_INLINE void MarkDescriptorArrayBlack(HeapObject host,
                                          DescriptorArray descriptors);

 private:
  // Granularity in which FixedArrays are scanned if |fixed_array_mode|
  // is true.
  static const int kProgressBarScanningChunk = 32 * KB;

  template <typename TSlot>
  V8_INLINE void VisitPointerImpl(HeapObject host, TSlot p);

  template <typename TSlot>
  V8_INLINE void VisitPointersImpl(HeapObject host, TSlot start, TSlot end);

  V8_INLINE int VisitFixedArrayIncremental(Map map, FixedArray object);

  template <typename T>
  V8_INLINE int VisitEmbedderTracingSubclass(Map map, T object);

  // Marks the object grey and pushes it on the marking work list.
  V8_INLINE void MarkObject(HeapObject host, HeapObject obj);

  MarkingState* marking_state() { return marking_state_; }

  MarkCompactCollector::MarkingWorklist* marking_worklist() const {
    return collector_->marking_worklist();
  }

  Heap* const heap_;
  MarkCompactCollector* const collector_;
  MarkingState* const marking_state_;
  const unsigned mark_compact_epoch_;
};

class EvacuationScope {
 public:
  explicit EvacuationScope(MarkCompactCollector* collector)
      : collector_(collector) {
    collector_->set_evacuation(true);
  }

  ~EvacuationScope() { collector_->set_evacuation(false); }

 private:
  MarkCompactCollector* collector_;
};

#ifdef ENABLE_MINOR_MC

// Collector for young-generation only.
class MinorMarkCompactCollector final : public MarkCompactCollectorBase {
 public:
  using MarkingState = MinorMarkingState;
  using NonAtomicMarkingState = MinorNonAtomicMarkingState;

  explicit MinorMarkCompactCollector(Heap* heap);
  ~MinorMarkCompactCollector() override;

  MarkingState* marking_state() { return &marking_state_; }

  NonAtomicMarkingState* non_atomic_marking_state() {
    return &non_atomic_marking_state_;
  }

  void SetUp() override;
  void TearDown() override;
  void CollectGarbage() override;

  void MakeIterable(Page* page, MarkingTreatmentMode marking_mode,
                    FreeSpaceTreatmentMode free_space_mode);
  void CleanupSweepToIteratePages();

 private:
  using MarkingWorklist = Worklist<HeapObject, 64 /* segment size */>;
  class RootMarkingVisitor;

  static const int kNumMarkers = 8;
  static const int kMainMarker = 0;

  inline MarkingWorklist* worklist() { return worklist_; }

  inline YoungGenerationMarkingVisitor* main_marking_visitor() {
    return main_marking_visitor_;
  }

  void MarkLiveObjects() override;
  void MarkRootSetInParallel(RootMarkingVisitor* root_visitor);
  V8_INLINE void MarkRootObject(HeapObject obj);
  void ProcessMarkingWorklist() override;
  void ClearNonLiveReferences() override;

  void EvacuatePrologue() override;
  void EvacuateEpilogue() override;
  void Evacuate() override;
  void EvacuatePagesInParallel() override;
  void UpdatePointersAfterEvacuation() override;

  UpdatingItem* CreateToSpaceUpdatingItem(MemoryChunk* chunk, Address start,
                                          Address end) override;
  UpdatingItem* CreateRememberedSetUpdatingItem(
      MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) override;

  int CollectNewSpaceArrayBufferTrackerItems(ItemParallelJob* job);

  int NumberOfParallelMarkingTasks(int pages);

  MarkingWorklist* worklist_;

  YoungGenerationMarkingVisitor* main_marking_visitor_;
  base::Semaphore page_parallel_job_semaphore_;
  std::vector<Page*> new_space_evacuation_pages_;
  std::vector<Page*> sweep_to_iterate_pages_;

  MarkingState marking_state_;
  NonAtomicMarkingState non_atomic_marking_state_;

  friend class YoungGenerationMarkingTask;
  friend class YoungGenerationMarkingVisitor;
};

#endif  // ENABLE_MINOR_MC

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_H_

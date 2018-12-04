// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_H_
#define V8_HEAP_MARK_COMPACT_H_

#include <vector>

#include "src/heap/concurrent-marking.h"
#include "src/heap/marking.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/spaces.h"
#include "src/heap/sweeper.h"
#include "src/heap/worklist.h"

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
  V8_INLINE MarkBit MarkBitFrom(HeapObject* obj) {
    return MarkBitFrom(MemoryChunk::FromAddress(obj->address()),
                       obj->address());
  }

  V8_INLINE MarkBit MarkBitFrom(MemoryChunk* p, Address addr) {
    return static_cast<ConcreteState*>(this)->bitmap(p)->MarkBitFromIndex(
        p->AddressToMarkbitIndex(addr));
  }

  Marking::ObjectColor Color(HeapObject* obj) {
    return Marking::Color(MarkBitFrom(obj));
  }

  V8_INLINE bool IsImpossible(HeapObject* obj) {
    return Marking::IsImpossible<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsBlack(HeapObject* obj) {
    return Marking::IsBlack<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsWhite(HeapObject* obj) {
    return Marking::IsWhite<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsGrey(HeapObject* obj) {
    return Marking::IsGrey<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool IsBlackOrGrey(HeapObject* obj) {
    return Marking::IsBlackOrGrey<access_mode>(MarkBitFrom(obj));
  }

  V8_INLINE bool WhiteToGrey(HeapObject* obj);
  V8_INLINE bool WhiteToBlack(HeapObject* obj);
  V8_INLINE bool GreyToBlack(HeapObject* obj);

  void ClearLiveness(MemoryChunk* chunk) {
    static_cast<ConcreteState*>(this)->bitmap(chunk)->Clear();
    static_cast<ConcreteState*>(this)->SetLiveBytes(chunk, 0);
  }
};

class MarkBitCellIterator {
 public:
  MarkBitCellIterator(MemoryChunk* chunk, Bitmap* bitmap) : chunk_(chunk) {
    DCHECK(Bitmap::IsCellAligned(
        chunk_->AddressToMarkbitIndex(chunk_->area_start())));
    DCHECK(Bitmap::IsCellAligned(
        chunk_->AddressToMarkbitIndex(chunk_->area_end())));
    last_cell_index_ =
        Bitmap::IndexToCell(chunk_->AddressToMarkbitIndex(chunk_->area_end()));
    cell_base_ = chunk_->area_start();
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
    cell_base_ += Bitmap::kBitsPerCell * kPointerSize;
    return ++cell_index_ != last_cell_index_;
  }

  inline bool Advance(unsigned int new_cell_index) {
    if (new_cell_index != cell_index_) {
      DCHECK_GT(new_cell_index, cell_index_);
      DCHECK_LE(new_cell_index, last_cell_index_);
      unsigned int diff = new_cell_index - cell_index_;
      cell_index_ = new_cell_index;
      cell_base_ += diff * (Bitmap::kBitsPerCell * kPointerSize);
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
    using value_type = std::pair<HeapObject*, int /* size */>;
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
    Map* const one_word_filler_map_;
    Map* const two_word_filler_map_;
    Map* const free_space_map_;
    MarkBitCellIterator it_;
    Address cell_base_;
    MarkBit::CellType current_cell_;
    HeapObject* current_object_;
    int current_size_;
  };

  LiveObjectRange(MemoryChunk* chunk, Bitmap* bitmap)
      : chunk_(chunk),
        bitmap_(bitmap),
        start_(chunk_->area_start()),
        end_(chunk->area_end()) {}

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
                                HeapObject** failed_object);

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
  virtual ~MarkCompactCollectorBase() = default;

  virtual void SetUp() = 0;
  virtual void TearDown() = 0;
  virtual void CollectGarbage() = 0;

  inline Heap* heap() const { return heap_; }
  inline Isolate* isolate();

 protected:
  static const int kMainThread = 0;
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
  void CreateAndExecuteEvacuationTasks(
      Collector* collector, ItemParallelJob* job,
      RecordMigratedSlotVisitor* record_visitor,
      MigrationObserver* migration_observer, const intptr_t live_bytes);

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
  Bitmap* bitmap(const MemoryChunk* chunk) const {
    return chunk->young_generation_bitmap_;
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
  Bitmap* bitmap(const MemoryChunk* chunk) const {
    return chunk->young_generation_bitmap_;
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

// This marking state is used when concurrent marking is running.
class IncrementalMarkingState final
    : public MarkingStateBase<IncrementalMarkingState, AccessMode::ATOMIC> {
 public:
  Bitmap* bitmap(const MemoryChunk* chunk) const {
    return Bitmap::FromAddress(chunk->address() + MemoryChunk::kHeaderSize);
  }

  // Concurrent marking uses local live bytes.
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
  Bitmap* bitmap(const MemoryChunk* chunk) const {
    return Bitmap::FromAddress(chunk->address() + MemoryChunk::kHeaderSize);
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

class MajorNonAtomicMarkingState final
    : public MarkingStateBase<MajorNonAtomicMarkingState,
                              AccessMode::NON_ATOMIC> {
 public:
  Bitmap* bitmap(const MemoryChunk* chunk) const {
    return Bitmap::FromAddress(chunk->address() + MemoryChunk::kHeaderSize);
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
  HeapObject* key;
  HeapObject* value;
};

typedef Worklist<Ephemeron, 64> EphemeronWorklist;

// Weak objects encountered during marking.
struct WeakObjects {
  Worklist<TransitionArray*, 64> transition_arrays;

  // Keep track of all EphemeronHashTables in the heap to process
  // them in the atomic pause.
  Worklist<EphemeronHashTable*, 64> ephemeron_hash_tables;

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
  Worklist<std::pair<HeapObject*, HeapObjectReference**>, 64> weak_references;
  Worklist<std::pair<HeapObject*, Code*>, 64> weak_objects_in_code;
};

struct EphemeronMarking {
  std::vector<HeapObject*> newly_discovered;
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

  // Wrapper for the shared and bailout worklists.
  class MarkingWorklist {
   public:
    using ConcurrentMarkingWorklist = Worklist<HeapObject*, 64>;
    using EmbedderTracingWorklist = Worklist<HeapObject*, 16>;

    // The heap parameter is not used but needed to match the sequential case.
    explicit MarkingWorklist(Heap* heap) {}

    void Push(HeapObject* object) {
      bool success = shared_.Push(kMainThread, object);
      USE(success);
      DCHECK(success);
    }

    void PushBailout(HeapObject* object) {
      bool success = bailout_.Push(kMainThread, object);
      USE(success);
      DCHECK(success);
    }

    HeapObject* Pop() {
      HeapObject* result;
#ifdef V8_CONCURRENT_MARKING
      if (bailout_.Pop(kMainThread, &result)) return result;
#endif
      if (shared_.Pop(kMainThread, &result)) return result;
#ifdef V8_CONCURRENT_MARKING
      // The expectation is that this work list is empty almost all the time
      // and we can thus avoid the emptiness checks by putting it last.
      if (on_hold_.Pop(kMainThread, &result)) return result;
#endif
      return nullptr;
    }

    HeapObject* PopBailout() {
#ifdef V8_CONCURRENT_MARKING
      HeapObject* result;
      if (bailout_.Pop(kMainThread, &result)) return result;
#endif
      return nullptr;
    }

    void Clear() {
      bailout_.Clear();
      shared_.Clear();
      on_hold_.Clear();
      embedder_.Clear();
    }

    bool IsBailoutEmpty() { return bailout_.IsLocalEmpty(kMainThread); }

    bool IsEmpty() {
      return bailout_.IsLocalEmpty(kMainThread) &&
             shared_.IsLocalEmpty(kMainThread) &&
             on_hold_.IsLocalEmpty(kMainThread) &&
             bailout_.IsGlobalPoolEmpty() && shared_.IsGlobalPoolEmpty() &&
             on_hold_.IsGlobalPoolEmpty();
    }

    bool IsEmbedderEmpty() {
      return embedder_.IsLocalEmpty(kMainThread) &&
             embedder_.IsGlobalPoolEmpty();
    }

    int Size() {
      return static_cast<int>(bailout_.LocalSize(kMainThread) +
                              shared_.LocalSize(kMainThread) +
                              on_hold_.LocalSize(kMainThread));
    }

    // Calls the specified callback on each element of the deques and replaces
    // the element with the result of the callback. If the callback returns
    // nullptr then the element is removed from the deque.
    // The callback must accept HeapObject* and return HeapObject*.
    template <typename Callback>
    void Update(Callback callback) {
      bailout_.Update(callback);
      shared_.Update(callback);
      on_hold_.Update(callback);
      embedder_.Update(callback);
    }

    ConcurrentMarkingWorklist* shared() { return &shared_; }
    ConcurrentMarkingWorklist* bailout() { return &bailout_; }
    ConcurrentMarkingWorklist* on_hold() { return &on_hold_; }
    EmbedderTracingWorklist* embedder() { return &embedder_; }

    void Print() {
      PrintWorklist("shared", &shared_);
      PrintWorklist("bailout", &bailout_);
      PrintWorklist("on_hold", &on_hold_);
    }

   private:
    // Prints the stats about the global pool of the worklist.
    void PrintWorklist(const char* worklist_name,
                       ConcurrentMarkingWorklist* worklist);

    // Worklist used for most objects.
    ConcurrentMarkingWorklist shared_;

    // Concurrent marking uses this worklist to bail out of concurrently
    // marking certain object types. These objects are handled later in a STW
    // pause after concurrent marking has finished.
    ConcurrentMarkingWorklist bailout_;

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

  static inline bool IsOnEvacuationCandidate(Object* obj) {
    return Page::FromAddress(reinterpret_cast<Address>(obj))
        ->IsEvacuationCandidate();
  }

  static inline bool IsOnEvacuationCandidate(MaybeObject* obj) {
    return Page::FromAddress(reinterpret_cast<Address>(obj))
        ->IsEvacuationCandidate();
  }

  void RecordRelocSlot(Code* host, RelocInfo* rinfo, Object* target);
  V8_INLINE static void RecordSlot(HeapObject* object, Object** slot,
                                   HeapObject* target);
  V8_INLINE static void RecordSlot(HeapObject* object,
                                   HeapObjectReference** slot,
                                   HeapObject* target);
  void RecordLiveSlotsOnPage(Page* page);

  void UpdateSlots(SlotsBuffer* buffer);
  void UpdateSlotsRecordedIn(SlotsBuffer* buffer);

  bool is_compacting() const { return compacting_; }

  // Ensures that sweeping is finished.
  //
  // Note: Can only be called safely from main thread.
  void EnsureSweepingCompleted();

  // Checks if sweeping is in progress right now on any space.
  bool sweeping_in_progress() const { return sweeper_->sweeping_in_progress(); }

  void set_evacuation(bool evacuation) { evacuation_ = evacuation; }

  bool evacuation() const { return evacuation_; }

  MarkingWorklist* marking_worklist() { return &marking_worklist_; }

  WeakObjects* weak_objects() { return &weak_objects_; }

  void AddTransitionArray(TransitionArray* array) {
    weak_objects_.transition_arrays.Push(kMainThread, array);
  }

  void AddEphemeronHashTable(EphemeronHashTable* table) {
    weak_objects_.ephemeron_hash_tables.Push(kMainThread, table);
  }

  void AddEphemeron(HeapObject* key, HeapObject* value) {
    weak_objects_.discovered_ephemerons.Push(kMainThread,
                                             Ephemeron{key, value});
  }

  void AddWeakReference(HeapObject* host, HeapObjectReference** slot) {
    weak_objects_.weak_references.Push(kMainThread, std::make_pair(host, slot));
  }

  void AddWeakObjectInCode(HeapObject* object, Code* code) {
    weak_objects_.weak_objects_in_code.Push(kMainThread,
                                            std::make_pair(object, code));
  }

  void AddNewlyDiscovered(HeapObject* object) {
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
#endif

 private:
  explicit MarkCompactCollector(Heap* heap);
  ~MarkCompactCollector() override;

  bool WillBeDeoptimized(Code* code);

  void ComputeEvacuationHeuristics(size_t area_size,
                                   int* target_fragmentation_percent,
                                   size_t* max_evacuated_bytes);

  void RecordObjectStats();

  // Finishes GC, performs heap verification if enabled.
  void Finish();

  void MarkLiveObjects() override;

  // Marks the object black and adds it to the marking work list.
  // This is for non-incremental marking only.
  V8_INLINE void MarkObject(HeapObject* host, HeapObject* obj);

  // Marks the object black and adds it to the marking work list.
  // This is for non-incremental marking only.
  V8_INLINE void MarkRootObject(Root root, HeapObject* obj);

  // Used by wrapper tracing.
  V8_INLINE void MarkExternallyReferencedObject(HeapObject* obj);

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

  // Collects a list of dependent code from maps embedded in optimize code.
  DependentCode* DependentCodeListFromNonLiveMaps();

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
  bool VisitEphemeron(HeapObject* key, HeapObject* value);

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
  static bool IsUnmarkedHeapObject(Heap* heap, Object** p);

  // Clear non-live references in weak cells, transition and descriptor arrays,
  // and deoptimize dependent code of non-live maps.
  void ClearNonLiveReferences() override;
  void MarkDependentCodeForDeoptimization();
  // Checks if the given weak cell is a simple transition from the parent map
  // of the given dead target. If so it clears the transition and trims
  // the descriptor array of the parent if needed.
  void ClearPotentialSimpleMapTransition(Map* dead_target);
  void ClearPotentialSimpleMapTransition(Map* map, Map* dead_target);
  // Compact every array in the global list of transition arrays and
  // trim the corresponding descriptor array if a transition target is non-live.
  void ClearFullMapTransitions();
  bool CompactTransitionArray(Map* map, TransitionArray* transitions,
                              DescriptorArray* descriptors);
  void TrimDescriptorArray(Map* map, DescriptorArray* descriptors);
  void TrimEnumCache(Map* map, DescriptorArray* descriptors);

  // After all reachable objects have been marked those weak map entries
  // with an unreachable key are removed from all encountered weak maps.
  // The linked list of all encountered weak maps is destroyed.
  void ClearWeakCollections();

  // Goes through the list of encountered weak references and clears those with
  // dead values. If the value is a dead map and the parent map transitions to
  // the dead map via weak cell, then this function also clears the map
  // transition.
  void ClearWeakReferences();
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
  void ReportAbortedEvacuationCandidate(HeapObject* failed_object, Page* page);

  static const int kEphemeronChunkSize = 8 * KB;

  int NumberOfParallelEphemeronVisitingTasks(size_t elements);

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
  std::vector<std::pair<HeapObject*, Page*>> aborted_evacuation_candidates_;

  Sweeper* sweeper_;

  MarkingState marking_state_;
  NonAtomicMarkingState non_atomic_marking_state_;

  friend class EphemeronHashTableMarkingTask;
  friend class FullEvacuator;
  friend class Heap;
  friend class RecordMigratedSlotVisitor;
};

template <FixedArrayVisitationMode fixed_array_mode,
          TraceRetainingPathMode retaining_path_mode, typename MarkingState>
class MarkingVisitor final
    : public HeapVisitor<
          int,
          MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>> {
 public:
  typedef HeapVisitor<
      int, MarkingVisitor<fixed_array_mode, retaining_path_mode, MarkingState>>
      Parent;

  V8_INLINE MarkingVisitor(MarkCompactCollector* collector,
                           MarkingState* marking_state);

  V8_INLINE bool ShouldVisitMapPointer() { return false; }

  V8_INLINE int VisitBytecodeArray(Map* map, BytecodeArray* object);
  V8_INLINE int VisitEphemeronHashTable(Map* map, EphemeronHashTable* object);
  V8_INLINE int VisitFixedArray(Map* map, FixedArray* object);
  V8_INLINE int VisitJSApiObject(Map* map, JSObject* object);
  V8_INLINE int VisitJSArrayBuffer(Map* map, JSArrayBuffer* object);
  V8_INLINE int VisitJSDataView(Map* map, JSDataView* object);
  V8_INLINE int VisitJSTypedArray(Map* map, JSTypedArray* object);
  V8_INLINE int VisitMap(Map* map, Map* object);
  V8_INLINE int VisitTransitionArray(Map* map, TransitionArray* object);

  // ObjectVisitor implementation.
  V8_INLINE void VisitPointer(HeapObject* host, Object** p) final;
  V8_INLINE void VisitPointer(HeapObject* host, MaybeObject** p) final;
  V8_INLINE void VisitPointers(HeapObject* host, Object** start,
                               Object** end) final;
  V8_INLINE void VisitPointers(HeapObject* host, MaybeObject** start,
                               MaybeObject** end) final;
  V8_INLINE void VisitEmbeddedPointer(Code* host, RelocInfo* rinfo) final;
  V8_INLINE void VisitCodeTarget(Code* host, RelocInfo* rinfo) final;

  // Weak list pointers should be ignored during marking. The lists are
  // reconstructed after GC.
  void VisitCustomWeakPointers(HeapObject* host, Object** start,
                               Object** end) final {}

 private:
  // Granularity in which FixedArrays are scanned if |fixed_array_mode|
  // is true.
  static const int kProgressBarScanningChunk = 32 * 1024;

  V8_INLINE int VisitFixedArrayIncremental(Map* map, FixedArray* object);

  template <typename T>
  V8_INLINE int VisitEmbedderTracingSubclass(Map* map, T* object);

  V8_INLINE void MarkMapContents(Map* map);

  // Marks the object black without pushing it on the marking work list. Returns
  // true if the object needed marking and false otherwise.
  V8_INLINE bool MarkObjectWithoutPush(HeapObject* host, HeapObject* object);

  // Marks the object grey and pushes it on the marking work list.
  V8_INLINE void MarkObject(HeapObject* host, HeapObject* obj);

  MarkingState* marking_state() { return marking_state_; }

  MarkCompactCollector::MarkingWorklist* marking_worklist() const {
    return collector_->marking_worklist();
  }

  Heap* const heap_;
  MarkCompactCollector* const collector_;
  MarkingState* const marking_state_;
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
  using MarkingWorklist = Worklist<HeapObject*, 64 /* segment size */>;
  class RootMarkingVisitor;

  static const int kNumMarkers = 8;
  static const int kMainMarker = 0;

  inline MarkingWorklist* worklist() { return worklist_; }

  inline YoungGenerationMarkingVisitor* main_marking_visitor() {
    return main_marking_visitor_;
  }

  void MarkLiveObjects() override;
  void MarkRootSetInParallel(RootMarkingVisitor* root_visitor);
  V8_INLINE void MarkRootObject(HeapObject* obj);
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

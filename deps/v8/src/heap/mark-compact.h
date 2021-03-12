// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_H_
#define V8_HEAP_MARK_COMPACT_H_

#include <atomic>
#include <vector>

#include "src/heap/concurrent-marking.h"
#include "src/heap/marking-visitor.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/marking.h"
#include "src/heap/memory-measurement.h"
#include "src/heap/parallel-work-item.h"
#include "src/heap/spaces.h"
#include "src/heap/sweeper.h"

namespace v8 {
namespace internal {

// Forward declarations.
class EvacuationJobTraits;
class HeapObjectVisitor;
class ItemParallelJob;
class MigrationObserver;
class ReadOnlySpace;
class RecordMigratedSlotVisitor;
class UpdatingItem;
class YoungGenerationMarkingVisitor;

class MarkBitCellIterator {
 public:
  MarkBitCellIterator(const MemoryChunk* chunk, Bitmap* bitmap)
      : chunk_(chunk) {
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
  const MemoryChunk* chunk_;
  MarkBit::CellType* cells_;
  unsigned int last_cell_index_;
  unsigned int cell_index_;
  Address cell_base_;
};

enum LiveObjectIterationMode { kBlackObjects, kGreyObjects, kAllLiveObjects };

template <LiveObjectIterationMode mode>
class LiveObjectRange {
 public:
  class iterator {
   public:
    using value_type = std::pair<HeapObject, int /* size */>;
    using pointer = const value_type*;
    using reference = const value_type&;
    using iterator_category = std::forward_iterator_tag;

    inline iterator(const MemoryChunk* chunk, Bitmap* bitmap, Address start);

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

    const MemoryChunk* const chunk_;
    Map const one_word_filler_map_;
    Map const two_word_filler_map_;
    Map const free_space_map_;
    MarkBitCellIterator it_;
    Address cell_base_;
    MarkBit::CellType current_cell_;
    HeapObject current_object_;
    int current_size_;
  };

  LiveObjectRange(const MemoryChunk* chunk, Bitmap* bitmap)
      : chunk_(chunk),
        bitmap_(bitmap),
        start_(chunk_->area_start()),
        end_(chunk->area_end()) {
    DCHECK(!chunk->IsLargePage());
  }

  inline iterator begin();
  inline iterator end();

 private:
  const MemoryChunk* const chunk_;
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
  virtual ~MarkCompactCollectorBase() = default;

  virtual void SetUp() = 0;
  virtual void TearDown() = 0;
  virtual void CollectGarbage() = 0;

  inline Heap* heap() const { return heap_; }
  inline Isolate* isolate();

 protected:
  explicit MarkCompactCollectorBase(Heap* heap) : heap_(heap) {}

  // Marking operations for objects reachable from roots.
  virtual void MarkLiveObjects() = 0;
  // Mark objects reachable (transitively) from objects in the marking
  // work list.
  virtual void DrainMarkingWorklist() = 0;
  // Clear non-live references held in side data structures.
  virtual void ClearNonLiveReferences() = 0;
  virtual void EvacuatePrologue() = 0;
  virtual void EvacuateEpilogue() = 0;
  virtual void Evacuate() = 0;
  virtual void EvacuatePagesInParallel() = 0;
  virtual void UpdatePointersAfterEvacuation() = 0;
  virtual std::unique_ptr<UpdatingItem> CreateToSpaceUpdatingItem(
      MemoryChunk* chunk, Address start, Address end) = 0;
  virtual std::unique_ptr<UpdatingItem> CreateRememberedSetUpdatingItem(
      MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) = 0;

  template <class Evacuator, class Collector>
  void CreateAndExecuteEvacuationTasks(
      Collector* collector,
      std::vector<std::pair<ParallelWorkItem, MemoryChunk*>> evacuation_items,
      MigrationObserver* migration_observer, const intptr_t live_bytes);

  // Returns whether this page should be moved according to heuristics.
  bool ShouldMovePage(Page* p, intptr_t live_bytes, bool promote_young);

  int CollectToSpaceUpdatingItems(
      std::vector<std::unique_ptr<UpdatingItem>>* items);
  template <typename IterateableSpace>
  int CollectRememberedSetUpdatingItems(
      std::vector<std::unique_ptr<UpdatingItem>>* items,
      IterateableSpace* space, RememberedSetUpdatingMode mode);

  int NumberOfParallelCompactionTasks();

  Heap* heap_;
};

class MinorMarkingState final
    : public MarkingStateBase<MinorMarkingState, AccessMode::ATOMIC> {
 public:
  ConcurrentBitmap<AccessMode::ATOMIC>* bitmap(
      const BasicMemoryChunk* chunk) const {
    return MemoryChunk::cast(chunk)
        ->young_generation_bitmap<AccessMode::ATOMIC>();
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
      const BasicMemoryChunk* chunk) const {
    return MemoryChunk::cast(chunk)
        ->young_generation_bitmap<AccessMode::NON_ATOMIC>();
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

// This is used by marking visitors.
class MajorMarkingState final
    : public MarkingStateBase<MajorMarkingState, AccessMode::ATOMIC> {
 public:
  ConcurrentBitmap<AccessMode::ATOMIC>* bitmap(
      const BasicMemoryChunk* chunk) const {
    return chunk->marking_bitmap<AccessMode::ATOMIC>();
  }

  // Concurrent marking uses local live bytes so we may do these accesses
  // non-atomically.
  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    chunk->live_byte_count_.fetch_add(by, std::memory_order_relaxed);
  }

  intptr_t live_bytes(MemoryChunk* chunk) const {
    return chunk->live_byte_count_.load(std::memory_order_relaxed);
  }

  void SetLiveBytes(MemoryChunk* chunk, intptr_t value) {
    chunk->live_byte_count_.store(value, std::memory_order_relaxed);
  }
};

// This is used by Scavenger and Evacuator in TransferColor.
// Live byte increments have to be atomic.
class MajorAtomicMarkingState final
    : public MarkingStateBase<MajorAtomicMarkingState, AccessMode::ATOMIC> {
 public:
  ConcurrentBitmap<AccessMode::ATOMIC>* bitmap(
      const BasicMemoryChunk* chunk) const {
    return chunk->marking_bitmap<AccessMode::ATOMIC>();
  }

  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    chunk->live_byte_count_.fetch_add(by);
  }
};

class MajorNonAtomicMarkingState final
    : public MarkingStateBase<MajorNonAtomicMarkingState,
                              AccessMode::NON_ATOMIC> {
 public:
  ConcurrentBitmap<AccessMode::NON_ATOMIC>* bitmap(
      const BasicMemoryChunk* chunk) const {
    return chunk->marking_bitmap<AccessMode::NON_ATOMIC>();
  }

  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    chunk->live_byte_count_.fetch_add(by, std::memory_order_relaxed);
  }

  intptr_t live_bytes(MemoryChunk* chunk) const {
    return chunk->live_byte_count_.load(std::memory_order_relaxed);
  }

  void SetLiveBytes(MemoryChunk* chunk, intptr_t value) {
    chunk->live_byte_count_.store(value, std::memory_order_relaxed);
  }
};

// This visitor is used for marking on the main thread. It is cheaper than
// the concurrent marking visitor because it does not snapshot JSObjects.
template <typename MarkingState>
class MainMarkingVisitor final
    : public MarkingVisitorBase<MainMarkingVisitor<MarkingState>,
                                MarkingState> {
 public:
  // This is used for revisiting objects that were black allocated.
  class V8_NODISCARD RevisitScope {
   public:
    explicit RevisitScope(MainMarkingVisitor* visitor) : visitor_(visitor) {
      DCHECK(!visitor->revisiting_object_);
      visitor->revisiting_object_ = true;
    }
    ~RevisitScope() {
      DCHECK(visitor_->revisiting_object_);
      visitor_->revisiting_object_ = false;
    }

   private:
    MainMarkingVisitor<MarkingState>* visitor_;
  };

  MainMarkingVisitor(MarkingState* marking_state,
                     MarkingWorklists::Local* local_marking_worklists,
                     WeakObjects* weak_objects, Heap* heap,
                     unsigned mark_compact_epoch,
                     BytecodeFlushMode bytecode_flush_mode,
                     bool embedder_tracing_enabled, bool is_forced_gc)
      : MarkingVisitorBase<MainMarkingVisitor<MarkingState>, MarkingState>(
            kMainThreadTask, local_marking_worklists, weak_objects, heap,
            mark_compact_epoch, bytecode_flush_mode, embedder_tracing_enabled,
            is_forced_gc),
        marking_state_(marking_state),
        revisiting_object_(false) {}

  // HeapVisitor override to allow revisiting of black objects.
  bool ShouldVisit(HeapObject object) {
    return marking_state_->GreyToBlack(object) ||
           V8_UNLIKELY(revisiting_object_);
  }

  void MarkDescriptorArrayFromWriteBarrier(DescriptorArray descriptors,
                                           int number_of_own_descriptors);

 private:
  // Functions required by MarkingVisitorBase.

  template <typename T, typename TBodyDescriptor = typename T::BodyDescriptor>
  int VisitJSObjectSubclass(Map map, T object);

  template <typename T>
  int VisitLeftTrimmableArray(Map map, T object);

  template <typename TSlot>
  void RecordSlot(HeapObject object, TSlot slot, HeapObject target);

  void RecordRelocSlot(Code host, RelocInfo* rinfo, HeapObject target);

  void SynchronizePageAccess(HeapObject heap_object) {
    // Nothing to do on the main thread.
  }

  MarkingState* marking_state() { return marking_state_; }

  TraceRetainingPathMode retaining_path_mode() {
    return (V8_UNLIKELY(FLAG_track_retaining_path))
               ? TraceRetainingPathMode::kEnabled
               : TraceRetainingPathMode::kDisabled;
  }

  MarkingState* const marking_state_;

  friend class MarkingVisitorBase<MainMarkingVisitor<MarkingState>,
                                  MarkingState>;
  bool revisiting_object_;
};

// Collector for young and old generation.
class MarkCompactCollector final : public MarkCompactCollectorBase {
 public:
#ifdef V8_ATOMIC_MARKING_STATE
  using MarkingState = MajorMarkingState;
#else
  using MarkingState = MajorNonAtomicMarkingState;
#endif  // V8_ATOMIC_MARKING_STATE
  using AtomicMarkingState = MajorAtomicMarkingState;
  using NonAtomicMarkingState = MajorNonAtomicMarkingState;

  using MarkingVisitor = MainMarkingVisitor<MarkingState>;

  class RootMarkingVisitor;
  class CustomRootBodyMarkingVisitor;

  enum IterationMode {
    kKeepMarking,
    kClearMarkbits,
  };

  enum class MarkingWorklistProcessingMode {
    kDefault,
    kTrackNewlyDiscoveredObjects
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
  void FinishConcurrentMarking();

  bool StartCompaction();

  void AbortCompaction();

  void StartMarking();

  static inline bool IsOnEvacuationCandidate(Object obj) {
    return Page::FromAddress(obj.ptr())->IsEvacuationCandidate();
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
  V8_INLINE static void RecordSlot(MemoryChunk* source_page,
                                   HeapObjectSlot slot, HeapObject target);
  void RecordLiveSlotsOnPage(Page* page);

  bool is_compacting() const { return compacting_; }

  // Ensures that sweeping is finished.
  //
  // Note: Can only be called safely from main thread.
  V8_EXPORT_PRIVATE void EnsureSweepingCompleted();

  void DrainSweepingWorklists();
  void DrainSweepingWorklistForSpace(AllocationSpace space);

  // Checks if sweeping is in progress right now on any space.
  bool sweeping_in_progress() const { return sweeper_->sweeping_in_progress(); }

  void set_evacuation(bool evacuation) { evacuation_ = evacuation; }

  bool evacuation() const { return evacuation_; }

  MarkingWorklists* marking_worklists() { return &marking_worklists_; }

  MarkingWorklists::Local* local_marking_worklists() {
    return local_marking_worklists_.get();
  }

  WeakObjects* weak_objects() { return &weak_objects_; }

  inline void AddTransitionArray(TransitionArray array);

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
  void VerifyMarkbitsAreClean();
  void VerifyMarkbitsAreDirty(ReadOnlySpace* space);
  void VerifyMarkbitsAreClean(PagedSpace* space);
  void VerifyMarkbitsAreClean(NewSpace* space);
  void VerifyMarkbitsAreClean(LargeObjectSpace* space);
#endif

  unsigned epoch() const { return epoch_; }

  BytecodeFlushMode bytecode_flush_mode() const { return bytecode_flush_mode_; }

  explicit MarkCompactCollector(Heap* heap);
  ~MarkCompactCollector() override;

  // Used by wrapper tracing.
  V8_INLINE void MarkExternallyReferencedObject(HeapObject obj);
  // Used by incremental marking for object that change their layout.
  void VisitObject(HeapObject obj);
  // Used by incremental marking for black-allocated objects.
  void RevisitObject(HeapObject obj);
  // Ensures that all descriptors int range [0, number_of_own_descripts)
  // are visited.
  void MarkDescriptorArrayFromWriteBarrier(DescriptorArray array,
                                           int number_of_own_descriptors);

  // Drains the main thread marking worklist until the specified number of
  // bytes are processed. If the number of bytes is zero, then the worklist
  // is drained until it is empty.
  template <MarkingWorklistProcessingMode mode =
                MarkingWorklistProcessingMode::kDefault>
  size_t ProcessMarkingWorklist(size_t bytes_to_process);

 private:
  void ComputeEvacuationHeuristics(size_t area_size,
                                   int* target_fragmentation_percent,
                                   size_t* max_evacuated_bytes);

  void RecordObjectStats();

  // Finishes GC, performs heap verification if enabled.
  void Finish();

  // Free unmarked ArrayBufferExtensions.
  void SweepArrayBufferExtensions();

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

  // Marks object reachable from harmony weak maps and wrapper tracing.
  void ProcessEphemeronMarking();

  // If the call-site of the top optimized code was not prepared for
  // deoptimization, then treat embedded pointers in the code as strong as
  // otherwise they can die and try to deoptimize the underlying code.
  void ProcessTopOptimizedFrame(ObjectVisitor* visitor);

  // Drains the main thread marking work list. Will mark all pending objects
  // if no concurrent threads are running.
  void DrainMarkingWorklist() override;

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
  bool TransitionArrayNeedsCompaction(TransitionArray transitions,
                                      int num_transitions);

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

  std::unique_ptr<UpdatingItem> CreateToSpaceUpdatingItem(MemoryChunk* chunk,
                                                          Address start,
                                                          Address end) override;
  std::unique_ptr<UpdatingItem> CreateRememberedSetUpdatingItem(
      MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) override;

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
  enum CollectorState{IDLE,
                      PREPARE_GC,
                      MARK_LIVE_OBJECTS,
                      SWEEP_SPACES,
                      ENCODE_FORWARDING_ADDRESSES,
                      UPDATE_POINTERS,
                      RELOCATE_OBJECTS};

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

  MarkingWorklists marking_worklists_;

  WeakObjects weak_objects_;
  EphemeronMarking ephemeron_marking_;

  std::unique_ptr<MarkingVisitor> marking_visitor_;
  std::unique_ptr<MarkingWorklists::Local> local_marking_worklists_;
  NativeContextInferrer native_context_inferrer_;
  NativeContextStats native_context_stats_;

  // Candidates for pages that should be evacuated.
  std::vector<Page*> evacuation_candidates_;
  // Pages that are actually processed during evacuation.
  std::vector<Page*> old_space_evacuation_pages_;
  std::vector<Page*> new_space_evacuation_pages_;
  std::vector<std::pair<HeapObject, Page*>> aborted_evacuation_candidates_;

  Sweeper* sweeper_;

  MarkingState marking_state_;
  NonAtomicMarkingState non_atomic_marking_state_;

  // Counts the number of major mark-compact collections. The counter is
  // incremented right after marking. This is used for:
  // - marking descriptor arrays. See NumberOfMarkedDescriptors. Only the lower
  //   two bits are used, so it is okay if this counter overflows and wraps
  //   around.
  unsigned epoch_ = 0;

  // Bytecode flushing is disabled when the code coverage mode is changed. Since
  // that can happen while a GC is happening and we need the
  // bytecode_flush_mode_ to remain the same through out a GC, we record this at
  // the start of each GC.
  BytecodeFlushMode bytecode_flush_mode_;

  friend class FullEvacuator;
  friend class RecordMigratedSlotVisitor;
};

class V8_NODISCARD EvacuationScope {
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
  void DrainMarkingWorklist() override;
  void TraceFragmentation();
  void ClearNonLiveReferences() override;

  void EvacuatePrologue() override;
  void EvacuateEpilogue() override;
  void Evacuate() override;
  void EvacuatePagesInParallel() override;
  void UpdatePointersAfterEvacuation() override;

  std::unique_ptr<UpdatingItem> CreateToSpaceUpdatingItem(MemoryChunk* chunk,
                                                          Address start,
                                                          Address end) override;
  std::unique_ptr<UpdatingItem> CreateRememberedSetUpdatingItem(
      MemoryChunk* chunk, RememberedSetUpdatingMode updating_mode) override;

  void SweepArrayBufferExtensions();

  MarkingWorklist* worklist_;

  YoungGenerationMarkingVisitor* main_marking_visitor_;
  base::Semaphore page_parallel_job_semaphore_;
  std::vector<Page*> new_space_evacuation_pages_;
  std::vector<Page*> sweep_to_iterate_pages_;

  MarkingState marking_state_;
  NonAtomicMarkingState non_atomic_marking_state_;

  friend class YoungGenerationMarkingTask;
  friend class YoungGenerationMarkingJob;
  friend class YoungGenerationMarkingVisitor;
};

#endif  // ENABLE_MINOR_MC

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_H_

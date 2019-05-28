// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/concurrent-marking.h"

#include <stack>
#include <unordered_map>

#include "include/v8config.h"
#include "src/base/template-utils.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/worklist.h"
#include "src/isolate.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/slots-inl.h"
#include "src/transitions-inl.h"
#include "src/utils-inl.h"
#include "src/utils.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

class ConcurrentMarkingState final
    : public MarkingStateBase<ConcurrentMarkingState, AccessMode::ATOMIC> {
 public:
  explicit ConcurrentMarkingState(MemoryChunkDataMap* memory_chunk_data)
      : memory_chunk_data_(memory_chunk_data) {}

  ConcurrentBitmap<AccessMode::ATOMIC>* bitmap(const MemoryChunk* chunk) {
    DCHECK_EQ(reinterpret_cast<intptr_t>(&chunk->marking_bitmap_) -
                  reinterpret_cast<intptr_t>(chunk),
              MemoryChunk::kMarkBitmapOffset);
    return chunk->marking_bitmap<AccessMode::ATOMIC>();
  }

  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    (*memory_chunk_data_)[chunk].live_bytes += by;
  }

  // The live_bytes and SetLiveBytes methods of the marking state are
  // not used by the concurrent marker.

 private:
  MemoryChunkDataMap* memory_chunk_data_;
};

// Helper class for storing in-object slot addresses and values.
class SlotSnapshot {
 public:
  SlotSnapshot() : number_of_slots_(0) {}
  int number_of_slots() const { return number_of_slots_; }
  ObjectSlot slot(int i) const { return snapshot_[i].first; }
  Object value(int i) const { return snapshot_[i].second; }
  void clear() { number_of_slots_ = 0; }
  void add(ObjectSlot slot, Object value) {
    snapshot_[number_of_slots_++] = {slot, value};
  }

 private:
  static const int kMaxSnapshotSize = JSObject::kMaxInstanceSize / kTaggedSize;
  int number_of_slots_;
  std::pair<ObjectSlot, Object> snapshot_[kMaxSnapshotSize];
  DISALLOW_COPY_AND_ASSIGN(SlotSnapshot);
};

class ConcurrentMarkingVisitor final
    : public HeapVisitor<int, ConcurrentMarkingVisitor> {
 public:
  using BaseClass = HeapVisitor<int, ConcurrentMarkingVisitor>;

  explicit ConcurrentMarkingVisitor(
      ConcurrentMarking::MarkingWorklist* shared,
      MemoryChunkDataMap* memory_chunk_data, WeakObjects* weak_objects,
      ConcurrentMarking::EmbedderTracingWorklist* embedder_objects, int task_id,
      bool embedder_tracing_enabled, unsigned mark_compact_epoch,
      bool is_forced_gc)
      : shared_(shared, task_id),
        weak_objects_(weak_objects),
        embedder_objects_(embedder_objects, task_id),
        marking_state_(memory_chunk_data),
        memory_chunk_data_(memory_chunk_data),
        task_id_(task_id),
        embedder_tracing_enabled_(embedder_tracing_enabled),
        mark_compact_epoch_(mark_compact_epoch),
        is_forced_gc_(is_forced_gc) {
    // It is not safe to access flags from concurrent marking visitor. So
    // set the bytecode flush mode based on the flags here
    bytecode_flush_mode_ = Heap::GetBytecodeFlushMode();
  }

  template <typename T>
  static V8_INLINE T Cast(HeapObject object) {
    return T::cast(object);
  }

  bool ShouldVisit(HeapObject object) {
    return marking_state_.GreyToBlack(object);
  }

  bool AllowDefaultJSObjectVisit() { return false; }

  template <typename THeapObjectSlot>
  void ProcessStrongHeapObject(HeapObject host, THeapObjectSlot slot,
                               HeapObject heap_object) {
    MarkObject(heap_object);
    MarkCompactCollector::RecordSlot(host, slot, heap_object);
  }

  template <typename THeapObjectSlot>
  void ProcessWeakHeapObject(HeapObject host, THeapObjectSlot slot,
                             HeapObject heap_object) {
#ifdef THREAD_SANITIZER
    // Perform a dummy acquire load to tell TSAN that there is no data race
    // in mark-bit initialization. See MemoryChunk::Initialize for the
    // corresponding release store.
    MemoryChunk* chunk = MemoryChunk::FromAddress(heap_object->address());
    CHECK_NOT_NULL(chunk->synchronized_heap());
#endif
    if (marking_state_.IsBlackOrGrey(heap_object)) {
      // Weak references with live values are directly processed here to
      // reduce the processing time of weak cells during the main GC
      // pause.
      MarkCompactCollector::RecordSlot(host, slot, heap_object);
    } else {
      // If we do not know about liveness of the value, we have to process
      // the reference when we know the liveness of the whole transitive
      // closure.
      weak_objects_->weak_references.Push(task_id_, std::make_pair(host, slot));
    }
  }

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    VisitPointersImpl(host, start, end);
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {
    VisitPointersImpl(host, start, end);
  }

  template <typename TSlot>
  V8_INLINE void VisitPointersImpl(HeapObject host, TSlot start, TSlot end) {
    using THeapObjectSlot = typename TSlot::THeapObjectSlot;
    for (TSlot slot = start; slot < end; ++slot) {
      typename TSlot::TObject object = slot.Relaxed_Load();
      HeapObject heap_object;
      if (object.GetHeapObjectIfStrong(&heap_object)) {
        // If the reference changes concurrently from strong to weak, the write
        // barrier will treat the weak reference as strong, so we won't miss the
        // weak reference.
        ProcessStrongHeapObject(host, THeapObjectSlot(slot), heap_object);
      } else if (TSlot::kCanBeWeak &&
                 object.GetHeapObjectIfWeak(&heap_object)) {
        ProcessWeakHeapObject(host, THeapObjectSlot(slot), heap_object);
      }
    }
  }

  // Weak list pointers should be ignored during marking. The lists are
  // reconstructed after GC.
  void VisitCustomWeakPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {}

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final {
    DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
    HeapObject object = rinfo->target_object();
    RecordRelocSlot(host, rinfo, object);
    if (!marking_state_.IsBlackOrGrey(object)) {
      if (host->IsWeakObject(object)) {
        weak_objects_->weak_objects_in_code.Push(task_id_,
                                                 std::make_pair(object, host));
      } else {
        MarkObject(object);
      }
    }
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) final {
    DCHECK(RelocInfo::IsCodeTargetMode(rinfo->rmode()));
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    RecordRelocSlot(host, rinfo, target);
    MarkObject(target);
  }

  void VisitPointersInSnapshot(HeapObject host, const SlotSnapshot& snapshot) {
    for (int i = 0; i < snapshot.number_of_slots(); i++) {
      ObjectSlot slot = snapshot.slot(i);
      Object object = snapshot.value(i);
      DCHECK(!HasWeakHeapObjectTag(object));
      if (!object->IsHeapObject()) continue;
      HeapObject heap_object = HeapObject::cast(object);
      MarkObject(heap_object);
      MarkCompactCollector::RecordSlot(host, slot, heap_object);
    }
  }

  // ===========================================================================
  // JS object =================================================================
  // ===========================================================================

  int VisitJSObject(Map map, JSObject object) {
    return VisitJSObjectSubclass(map, object);
  }

  int VisitJSObjectFast(Map map, JSObject object) {
    return VisitJSObjectSubclassFast(map, object);
  }

  int VisitWasmInstanceObject(Map map, WasmInstanceObject object) {
    return VisitJSObjectSubclass(map, object);
  }

  int VisitJSWeakRef(Map map, JSWeakRef weak_ref) {
    int size = VisitJSObjectSubclass(map, weak_ref);
    if (size == 0) {
      return 0;
    }
    if (weak_ref->target()->IsHeapObject()) {
      HeapObject target = HeapObject::cast(weak_ref->target());
      if (marking_state_.IsBlackOrGrey(target)) {
        // Record the slot inside the JSWeakRef, since the
        // VisitJSObjectSubclass above didn't visit it.
        ObjectSlot slot = weak_ref.RawField(JSWeakRef::kTargetOffset);
        MarkCompactCollector::RecordSlot(weak_ref, slot, target);
      } else {
        // JSWeakRef points to a potentially dead object. We have to process
        // them when we know the liveness of the whole transitive closure.
        weak_objects_->js_weak_refs.Push(task_id_, weak_ref);
      }
    }
    return size;
  }

  int VisitWeakCell(Map map, WeakCell weak_cell) {
    if (!ShouldVisit(weak_cell)) return 0;

    int size = WeakCell::BodyDescriptor::SizeOf(map, weak_cell);
    VisitMapPointer(weak_cell, weak_cell->map_slot());
    WeakCell::BodyDescriptor::IterateBody(map, weak_cell, size, this);
    if (weak_cell->target()->IsHeapObject()) {
      HeapObject target = HeapObject::cast(weak_cell->target());
      if (marking_state_.IsBlackOrGrey(target)) {
        // Record the slot inside the WeakCell, since the IterateBody above
        // didn't visit it.
        ObjectSlot slot = weak_cell.RawField(WeakCell::kTargetOffset);
        MarkCompactCollector::RecordSlot(weak_cell, slot, target);
      } else {
        // WeakCell points to a potentially dead object. We have to process
        // them when we know the liveness of the whole transitive closure.
        weak_objects_->weak_cells.Push(task_id_, weak_cell);
      }
    }
    return size;
  }

  // Some JS objects can carry back links to embedders that contain information
  // relevant to the garbage collectors.

  int VisitJSApiObject(Map map, JSObject object) {
    return VisitEmbedderTracingSubclass(map, object);
  }

  int VisitJSArrayBuffer(Map map, JSArrayBuffer object) {
    return VisitEmbedderTracingSubclass(map, object);
  }

  int VisitJSDataView(Map map, JSDataView object) {
    return VisitEmbedderTracingSubclass(map, object);
  }

  int VisitJSTypedArray(Map map, JSTypedArray object) {
    return VisitEmbedderTracingSubclass(map, object);
  }

  // ===========================================================================
  // Strings with pointers =====================================================
  // ===========================================================================

  int VisitConsString(Map map, ConsString object) {
    return VisitFullyWithSnapshot(map, object);
  }

  int VisitSlicedString(Map map, SlicedString object) {
    return VisitFullyWithSnapshot(map, object);
  }

  int VisitThinString(Map map, ThinString object) {
    return VisitFullyWithSnapshot(map, object);
  }

  // ===========================================================================
  // Strings without pointers ==================================================
  // ===========================================================================

  int VisitSeqOneByteString(Map map, SeqOneByteString object) {
    if (!ShouldVisit(object)) return 0;
    VisitMapPointer(object, object->map_slot());
    return SeqOneByteString::SizeFor(object->synchronized_length());
  }

  int VisitSeqTwoByteString(Map map, SeqTwoByteString object) {
    if (!ShouldVisit(object)) return 0;
    VisitMapPointer(object, object->map_slot());
    return SeqTwoByteString::SizeFor(object->synchronized_length());
  }

  // ===========================================================================
  // Fixed array object ========================================================
  // ===========================================================================

  int VisitFixedArrayWithProgressBar(Map map, FixedArray object,
                                     MemoryChunk* chunk) {
    // The concurrent marker can process larger chunks than the main thread
    // marker.
    const int kProgressBarScanningChunk =
        RoundUp(kMaxRegularHeapObjectSize, kTaggedSize);
    DCHECK(marking_state_.IsBlackOrGrey(object));
    marking_state_.GreyToBlack(object);
    int size = FixedArray::BodyDescriptor::SizeOf(map, object);
    size_t current_progress_bar = chunk->ProgressBar();
    if (current_progress_bar == 0) {
      // Try to move the progress bar forward to start offset. This solves the
      // problem of not being able to observe a progress bar reset when
      // processing the first kProgressBarScanningChunk.
      if (!chunk->TrySetProgressBar(0,
                                    FixedArray::BodyDescriptor::kStartOffset))
        return 0;
      current_progress_bar = FixedArray::BodyDescriptor::kStartOffset;
    }
    int start = static_cast<int>(current_progress_bar);
    int end = Min(size, start + kProgressBarScanningChunk);
    if (start < end) {
      VisitPointers(object, object.RawField(start), object.RawField(end));
      // Setting the progress bar can fail if the object that is currently
      // scanned is also revisited. In this case, there may be two tasks racing
      // on the progress counter. The looser can bail out because the progress
      // bar is reset before the tasks race on the object.
      if (chunk->TrySetProgressBar(current_progress_bar, end) && (end < size)) {
        // The object can be pushed back onto the marking worklist only after
        // progress bar was updated.
        shared_.Push(object);
      }
    }
    return end - start;
  }

  int VisitFixedArray(Map map, FixedArray object) {
    // Arrays with the progress bar are not left-trimmable because they reside
    // in the large object space.
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
    return chunk->IsFlagSet<AccessMode::ATOMIC>(MemoryChunk::HAS_PROGRESS_BAR)
               ? VisitFixedArrayWithProgressBar(map, object, chunk)
               : VisitLeftTrimmableArray(map, object);
  }

  int VisitFixedDoubleArray(Map map, FixedDoubleArray object) {
    return VisitLeftTrimmableArray(map, object);
  }

  // ===========================================================================
  // Side-effectful visitation.
  // ===========================================================================

  int VisitSharedFunctionInfo(Map map, SharedFunctionInfo shared_info) {
    if (!ShouldVisit(shared_info)) return 0;

    int size = SharedFunctionInfo::BodyDescriptor::SizeOf(map, shared_info);
    VisitMapPointer(shared_info, shared_info->map_slot());
    SharedFunctionInfo::BodyDescriptor::IterateBody(map, shared_info, size,
                                                    this);

    // If the SharedFunctionInfo has old bytecode, mark it as flushable,
    // otherwise visit the function data field strongly.
    if (shared_info->ShouldFlushBytecode(bytecode_flush_mode_)) {
      weak_objects_->bytecode_flushing_candidates.Push(task_id_, shared_info);
    } else {
      VisitPointer(shared_info, shared_info->RawField(
                                    SharedFunctionInfo::kFunctionDataOffset));
    }
    return size;
  }

  int VisitBytecodeArray(Map map, BytecodeArray object) {
    if (!ShouldVisit(object)) return 0;
    int size = BytecodeArray::BodyDescriptor::SizeOf(map, object);
    VisitMapPointer(object, object->map_slot());
    BytecodeArray::BodyDescriptor::IterateBody(map, object, size, this);
    if (!is_forced_gc_) {
      object->MakeOlder();
    }
    return size;
  }

  int VisitJSFunction(Map map, JSFunction object) {
    int size = VisitJSObjectSubclass(map, object);

    // Check if the JSFunction needs reset due to bytecode being flushed.
    if (bytecode_flush_mode_ != BytecodeFlushMode::kDoNotFlushBytecode &&
        object->NeedsResetDueToFlushedBytecode()) {
      weak_objects_->flushed_js_functions.Push(task_id_, object);
    }

    return size;
  }

  int VisitMap(Map meta_map, Map map) {
    if (!ShouldVisit(map)) return 0;
    int size = Map::BodyDescriptor::SizeOf(meta_map, map);
    if (map->CanTransition()) {
      // Maps that can transition share their descriptor arrays and require
      // special visiting logic to avoid memory leaks.
      // Since descriptor arrays are potentially shared, ensure that only the
      // descriptors that belong to this map are marked. The first time a
      // non-empty descriptor array is marked, its header is also visited. The
      // slot holding the descriptor array will be implicitly recorded when the
      // pointer fields of this map are visited.
      DescriptorArray descriptors = map->synchronized_instance_descriptors();
      MarkDescriptorArrayBlack(descriptors);
      int number_of_own_descriptors = map->NumberOfOwnDescriptors();
      if (number_of_own_descriptors) {
        // It is possible that the concurrent marker observes the
        // number_of_own_descriptors out of sync with the descriptors. In that
        // case the marking write barrier for the descriptor array will ensure
        // that all required descriptors are marked. The concurrent marker
        // just should avoid crashing in that case. That's why we need the
        // std::min<int>() below.
        VisitDescriptors(descriptors,
                         std::min<int>(number_of_own_descriptors,
                                       descriptors->number_of_descriptors()));
      }
      // Mark the pointer fields of the Map. Since the transitions array has
      // been marked already, it is fine that one of these fields contains a
      // pointer to it.
    }
    Map::BodyDescriptor::IterateBody(meta_map, map, size, this);
    return size;
  }

  void VisitDescriptors(DescriptorArray descriptor_array,
                        int number_of_own_descriptors) {
    int16_t new_marked = static_cast<int16_t>(number_of_own_descriptors);
    int16_t old_marked = descriptor_array->UpdateNumberOfMarkedDescriptors(
        mark_compact_epoch_, new_marked);
    if (old_marked < new_marked) {
      VisitPointers(
          descriptor_array,
          MaybeObjectSlot(descriptor_array->GetDescriptorSlot(old_marked)),
          MaybeObjectSlot(descriptor_array->GetDescriptorSlot(new_marked)));
    }
  }

  int VisitDescriptorArray(Map map, DescriptorArray array) {
    if (!ShouldVisit(array)) return 0;
    VisitMapPointer(array, array->map_slot());
    int size = DescriptorArray::BodyDescriptor::SizeOf(map, array);
    VisitPointers(array, array->GetFirstPointerSlot(),
                  array->GetDescriptorSlot(0));
    VisitDescriptors(array, array->number_of_descriptors());
    return size;
  }

  int VisitTransitionArray(Map map, TransitionArray array) {
    if (!ShouldVisit(array)) return 0;
    VisitMapPointer(array, array->map_slot());
    int size = TransitionArray::BodyDescriptor::SizeOf(map, array);
    TransitionArray::BodyDescriptor::IterateBody(map, array, size, this);
    weak_objects_->transition_arrays.Push(task_id_, array);
    return size;
  }

  int VisitJSWeakCollection(Map map, JSWeakCollection object) {
    return VisitJSObjectSubclass(map, object);
  }

  int VisitEphemeronHashTable(Map map, EphemeronHashTable table) {
    if (!ShouldVisit(table)) return 0;
    weak_objects_->ephemeron_hash_tables.Push(task_id_, table);

    for (int i = 0; i < table->Capacity(); i++) {
      ObjectSlot key_slot =
          table->RawFieldOfElementAt(EphemeronHashTable::EntryToIndex(i));
      HeapObject key = HeapObject::cast(table->KeyAt(i));
      MarkCompactCollector::RecordSlot(table, key_slot, key);

      ObjectSlot value_slot =
          table->RawFieldOfElementAt(EphemeronHashTable::EntryToValueIndex(i));

      if (marking_state_.IsBlackOrGrey(key)) {
        VisitPointer(table, value_slot);

      } else {
        Object value_obj = table->ValueAt(i);

        if (value_obj->IsHeapObject()) {
          HeapObject value = HeapObject::cast(value_obj);
          MarkCompactCollector::RecordSlot(table, value_slot, value);

          // Revisit ephemerons with both key and value unreachable at end
          // of concurrent marking cycle.
          if (marking_state_.IsWhite(value)) {
            weak_objects_->discovered_ephemerons.Push(task_id_,
                                                      Ephemeron{key, value});
          }
        }
      }
    }

    return table->SizeFromMap(map);
  }

  // Implements ephemeron semantics: Marks value if key is already reachable.
  // Returns true if value was actually marked.
  bool ProcessEphemeron(HeapObject key, HeapObject value) {
    if (marking_state_.IsBlackOrGrey(key)) {
      if (marking_state_.WhiteToGrey(value)) {
        shared_.Push(value);
        return true;
      }

    } else if (marking_state_.IsWhite(value)) {
      weak_objects_->next_ephemerons.Push(task_id_, Ephemeron{key, value});
    }

    return false;
  }

  void MarkObject(HeapObject object) {
#ifdef THREAD_SANITIZER
    // Perform a dummy acquire load to tell TSAN that there is no data race
    // in mark-bit initialization. See MemoryChunk::Initialize for the
    // corresponding release store.
    MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
    CHECK_NOT_NULL(chunk->synchronized_heap());
#endif
    if (marking_state_.WhiteToGrey(object)) {
      shared_.Push(object);
    }
  }

  void MarkDescriptorArrayBlack(DescriptorArray descriptors) {
    marking_state_.WhiteToGrey(descriptors);
    if (marking_state_.GreyToBlack(descriptors)) {
      VisitPointers(descriptors, descriptors->GetFirstPointerSlot(),
                    descriptors->GetDescriptorSlot(0));
    }
  }

 private:
  // Helper class for collecting in-object slot addresses and values.
  class SlotSnapshottingVisitor final : public ObjectVisitor {
   public:
    explicit SlotSnapshottingVisitor(SlotSnapshot* slot_snapshot)
        : slot_snapshot_(slot_snapshot) {
      slot_snapshot_->clear();
    }

    void VisitPointers(HeapObject host, ObjectSlot start,
                       ObjectSlot end) override {
      for (ObjectSlot p = start; p < end; ++p) {
        Object object = p.Relaxed_Load();
        slot_snapshot_->add(p, object);
      }
    }

    void VisitPointers(HeapObject host, MaybeObjectSlot start,
                       MaybeObjectSlot end) override {
      // This should never happen, because we don't use snapshotting for objects
      // which contain weak references.
      UNREACHABLE();
    }

    void VisitCodeTarget(Code host, RelocInfo* rinfo) final {
      // This should never happen, because snapshotting is performed only on
      // JSObjects (and derived classes).
      UNREACHABLE();
    }

    void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final {
      // This should never happen, because snapshotting is performed only on
      // JSObjects (and derived classes).
      UNREACHABLE();
    }

    void VisitCustomWeakPointers(HeapObject host, ObjectSlot start,
                                 ObjectSlot end) override {
      DCHECK(host->IsWeakCell() || host->IsJSWeakRef());
    }

   private:
    SlotSnapshot* slot_snapshot_;
  };

  template <typename T>
  int VisitJSObjectSubclassFast(Map map, T object) {
    DCHECK_IMPLIES(FLAG_unbox_double_fields, map->HasFastPointerLayout());
    using TBodyDescriptor = typename T::FastBodyDescriptor;
    return VisitJSObjectSubclass<T, TBodyDescriptor>(map, object);
  }

  template <typename T, typename TBodyDescriptor = typename T::BodyDescriptor>
  int VisitJSObjectSubclass(Map map, T object) {
    int size = TBodyDescriptor::SizeOf(map, object);
    int used_size = map->UsedInstanceSize();
    DCHECK_LE(used_size, size);
    DCHECK_GE(used_size, T::kHeaderSize);
    return VisitPartiallyWithSnapshot<T, TBodyDescriptor>(map, object,
                                                          used_size, size);
  }

  template <typename T>
  int VisitEmbedderTracingSubclass(Map map, T object) {
    DCHECK(object->IsApiWrapper());
    int size = VisitJSObjectSubclass(map, object);
    if (size && embedder_tracing_enabled_) {
      // Success: The object needs to be processed for embedder references on
      // the main thread.
      embedder_objects_.Push(object);
    }
    return size;
  }

  template <typename T>
  int VisitLeftTrimmableArray(Map map, T object) {
    // The synchronized_length() function checks that the length is a Smi.
    // This is not necessarily the case if the array is being left-trimmed.
    Object length = object->unchecked_synchronized_length();
    if (!ShouldVisit(object)) return 0;
    // The cached length must be the actual length as the array is not black.
    // Left trimming marks the array black before over-writing the length.
    DCHECK(length->IsSmi());
    int size = T::SizeFor(Smi::ToInt(length));
    VisitMapPointer(object, object->map_slot());
    T::BodyDescriptor::IterateBody(map, object, size, this);
    return size;
  }

  template <typename T>
  int VisitFullyWithSnapshot(Map map, T object) {
    using TBodyDescriptor = typename T::BodyDescriptor;
    int size = TBodyDescriptor::SizeOf(map, object);
    return VisitPartiallyWithSnapshot<T, TBodyDescriptor>(map, object, size,
                                                          size);
  }

  template <typename T, typename TBodyDescriptor = typename T::BodyDescriptor>
  int VisitPartiallyWithSnapshot(Map map, T object, int used_size, int size) {
    const SlotSnapshot& snapshot =
        MakeSlotSnapshot<T, TBodyDescriptor>(map, object, used_size);
    if (!ShouldVisit(object)) return 0;
    VisitPointersInSnapshot(object, snapshot);
    return size;
  }

  template <typename T, typename TBodyDescriptor>
  const SlotSnapshot& MakeSlotSnapshot(Map map, T object, int size) {
    SlotSnapshottingVisitor visitor(&slot_snapshot_);
    visitor.VisitPointer(object, ObjectSlot(object->map_slot().address()));
    TBodyDescriptor::IterateBody(map, object, size, &visitor);
    return slot_snapshot_;
  }

  void RecordRelocSlot(Code host, RelocInfo* rinfo, HeapObject target) {
    MarkCompactCollector::RecordRelocSlotInfo info =
        MarkCompactCollector::PrepareRecordRelocSlot(host, rinfo, target);
    if (info.should_record) {
      MemoryChunkData& data = (*memory_chunk_data_)[info.memory_chunk];
      if (!data.typed_slots) {
        data.typed_slots.reset(new TypedSlots());
      }
      data.typed_slots->Insert(info.slot_type, info.offset);
    }
  }

  ConcurrentMarking::MarkingWorklist::View shared_;
  WeakObjects* weak_objects_;
  ConcurrentMarking::EmbedderTracingWorklist::View embedder_objects_;
  ConcurrentMarkingState marking_state_;
  MemoryChunkDataMap* memory_chunk_data_;
  int task_id_;
  SlotSnapshot slot_snapshot_;
  bool embedder_tracing_enabled_;
  const unsigned mark_compact_epoch_;
  bool is_forced_gc_;
  BytecodeFlushMode bytecode_flush_mode_;
};

// Strings can change maps due to conversion to thin string or external strings.
// Use unchecked cast to avoid data race in slow dchecks.
template <>
ConsString ConcurrentMarkingVisitor::Cast(HeapObject object) {
  return ConsString::unchecked_cast(object);
}

template <>
SlicedString ConcurrentMarkingVisitor::Cast(HeapObject object) {
  return SlicedString::unchecked_cast(object);
}

template <>
ThinString ConcurrentMarkingVisitor::Cast(HeapObject object) {
  return ThinString::unchecked_cast(object);
}

template <>
SeqOneByteString ConcurrentMarkingVisitor::Cast(HeapObject object) {
  return SeqOneByteString::unchecked_cast(object);
}

template <>
SeqTwoByteString ConcurrentMarkingVisitor::Cast(HeapObject object) {
  return SeqTwoByteString::unchecked_cast(object);
}

// Fixed array can become a free space during left trimming.
template <>
FixedArray ConcurrentMarkingVisitor::Cast(HeapObject object) {
  return FixedArray::unchecked_cast(object);
}

class ConcurrentMarking::Task : public CancelableTask {
 public:
  Task(Isolate* isolate, ConcurrentMarking* concurrent_marking,
       TaskState* task_state, int task_id)
      : CancelableTask(isolate),
        concurrent_marking_(concurrent_marking),
        task_state_(task_state),
        task_id_(task_id) {}

  ~Task() override = default;

 private:
  // v8::internal::CancelableTask overrides.
  void RunInternal() override {
    concurrent_marking_->Run(task_id_, task_state_);
  }

  ConcurrentMarking* concurrent_marking_;
  TaskState* task_state_;
  int task_id_;
  DISALLOW_COPY_AND_ASSIGN(Task);
};

ConcurrentMarking::ConcurrentMarking(Heap* heap, MarkingWorklist* shared,
                                     MarkingWorklist* on_hold,
                                     WeakObjects* weak_objects,
                                     EmbedderTracingWorklist* embedder_objects)
    : heap_(heap),
      shared_(shared),
      on_hold_(on_hold),
      weak_objects_(weak_objects),
      embedder_objects_(embedder_objects) {
// The runtime flag should be set only if the compile time flag was set.
#ifndef V8_CONCURRENT_MARKING
  CHECK(!FLAG_concurrent_marking && !FLAG_parallel_marking);
#endif
}

void ConcurrentMarking::Run(int task_id, TaskState* task_state) {
  TRACE_BACKGROUND_GC(heap_->tracer(),
                      GCTracer::BackgroundScope::MC_BACKGROUND_MARKING);
  size_t kBytesUntilInterruptCheck = 64 * KB;
  int kObjectsUntilInterrupCheck = 1000;
  ConcurrentMarkingVisitor visitor(
      shared_, &task_state->memory_chunk_data, weak_objects_, embedder_objects_,
      task_id, heap_->local_embedder_heap_tracer()->InUse(),
      task_state->mark_compact_epoch, task_state->is_forced_gc);
  double time_ms;
  size_t marked_bytes = 0;
  if (FLAG_trace_concurrent_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "Starting concurrent marking task %d\n", task_id);
  }
  bool ephemeron_marked = false;

  {
    TimedScope scope(&time_ms);

    {
      Ephemeron ephemeron;

      while (weak_objects_->current_ephemerons.Pop(task_id, &ephemeron)) {
        if (visitor.ProcessEphemeron(ephemeron.key, ephemeron.value)) {
          ephemeron_marked = true;
        }
      }
    }

    bool done = false;
    while (!done) {
      size_t current_marked_bytes = 0;
      int objects_processed = 0;
      while (current_marked_bytes < kBytesUntilInterruptCheck &&
             objects_processed < kObjectsUntilInterrupCheck) {
        HeapObject object;
        if (!shared_->Pop(task_id, &object)) {
          done = true;
          break;
        }
        objects_processed++;
        // The order of the two loads is important.
        Address new_space_top = heap_->new_space()->original_top_acquire();
        Address new_space_limit = heap_->new_space()->original_limit_relaxed();
        Address new_large_object = heap_->new_lo_space()->pending_object();
        Address addr = object->address();
        if ((new_space_top <= addr && addr < new_space_limit) ||
            addr == new_large_object) {
          on_hold_->Push(task_id, object);
        } else {
          Map map = object->synchronized_map();
          current_marked_bytes += visitor.Visit(map, object);
        }
      }
      marked_bytes += current_marked_bytes;
      base::AsAtomicWord::Relaxed_Store<size_t>(&task_state->marked_bytes,
                                                marked_bytes);
      if (task_state->preemption_request) {
        TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                     "ConcurrentMarking::Run Preempted");
        break;
      }
    }

    if (done) {
      Ephemeron ephemeron;

      while (weak_objects_->discovered_ephemerons.Pop(task_id, &ephemeron)) {
        if (visitor.ProcessEphemeron(ephemeron.key, ephemeron.value)) {
          ephemeron_marked = true;
        }
      }
    }

    shared_->FlushToGlobal(task_id);
    on_hold_->FlushToGlobal(task_id);
    embedder_objects_->FlushToGlobal(task_id);

    weak_objects_->transition_arrays.FlushToGlobal(task_id);
    weak_objects_->ephemeron_hash_tables.FlushToGlobal(task_id);
    weak_objects_->current_ephemerons.FlushToGlobal(task_id);
    weak_objects_->next_ephemerons.FlushToGlobal(task_id);
    weak_objects_->discovered_ephemerons.FlushToGlobal(task_id);
    weak_objects_->weak_references.FlushToGlobal(task_id);
    weak_objects_->js_weak_refs.FlushToGlobal(task_id);
    weak_objects_->weak_cells.FlushToGlobal(task_id);
    weak_objects_->weak_objects_in_code.FlushToGlobal(task_id);
    weak_objects_->bytecode_flushing_candidates.FlushToGlobal(task_id);
    weak_objects_->flushed_js_functions.FlushToGlobal(task_id);
    base::AsAtomicWord::Relaxed_Store<size_t>(&task_state->marked_bytes, 0);
    total_marked_bytes_ += marked_bytes;

    if (ephemeron_marked) {
      set_ephemeron_marked(true);
    }

    {
      base::MutexGuard guard(&pending_lock_);
      is_pending_[task_id] = false;
      --pending_task_count_;
      pending_condition_.NotifyAll();
    }
  }
  if (FLAG_trace_concurrent_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "Task %d concurrently marked %dKB in %.2fms\n", task_id,
        static_cast<int>(marked_bytes / KB), time_ms);
  }
}

void ConcurrentMarking::ScheduleTasks() {
  DCHECK(FLAG_parallel_marking || FLAG_concurrent_marking);
  DCHECK(!heap_->IsTearingDown());
  base::MutexGuard guard(&pending_lock_);
  DCHECK_EQ(0, pending_task_count_);
  if (task_count_ == 0) {
    static const int num_cores =
        V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1;
#if defined(V8_OS_MACOSX)
    // Mac OSX 10.11 and prior seems to have trouble when doing concurrent
    // marking on competing hyper-threads (regresses Octane/Splay). As such,
    // only use num_cores/2, leaving one of those for the main thread.
    // TODO(ulan): Use all cores on Mac 10.12+.
    task_count_ = Max(1, Min(kMaxTasks, (num_cores / 2) - 1));
#else   // defined(OS_MACOSX)
    // On other platforms use all logical cores, leaving one for the main
    // thread.
    task_count_ = Max(1, Min(kMaxTasks, num_cores - 1));
#endif  // defined(OS_MACOSX)
  }
  // Task id 0 is for the main thread.
  for (int i = 1; i <= task_count_; i++) {
    if (!is_pending_[i]) {
      if (FLAG_trace_concurrent_marking) {
        heap_->isolate()->PrintWithTimestamp(
            "Scheduling concurrent marking task %d\n", i);
      }
      task_state_[i].preemption_request = false;
      task_state_[i].mark_compact_epoch =
          heap_->mark_compact_collector()->epoch();
      task_state_[i].is_forced_gc = heap_->is_current_gc_forced();
      is_pending_[i] = true;
      ++pending_task_count_;
      auto task =
          base::make_unique<Task>(heap_->isolate(), this, &task_state_[i], i);
      cancelable_id_[i] = task->id();
      V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
    }
  }
  DCHECK_EQ(task_count_, pending_task_count_);
}

void ConcurrentMarking::RescheduleTasksIfNeeded() {
  DCHECK(FLAG_parallel_marking || FLAG_concurrent_marking);
  if (heap_->IsTearingDown()) return;
  {
    base::MutexGuard guard(&pending_lock_);
    if (pending_task_count_ > 0) return;
  }
  if (!shared_->IsGlobalPoolEmpty() ||
      !weak_objects_->current_ephemerons.IsEmpty() ||
      !weak_objects_->discovered_ephemerons.IsEmpty()) {
    ScheduleTasks();
  }
}

bool ConcurrentMarking::Stop(StopRequest stop_request) {
  DCHECK(FLAG_parallel_marking || FLAG_concurrent_marking);
  base::MutexGuard guard(&pending_lock_);

  if (pending_task_count_ == 0) return false;

  if (stop_request != StopRequest::COMPLETE_TASKS_FOR_TESTING) {
    CancelableTaskManager* task_manager =
        heap_->isolate()->cancelable_task_manager();
    for (int i = 1; i <= task_count_; i++) {
      if (is_pending_[i]) {
        if (task_manager->TryAbort(cancelable_id_[i]) ==
            TryAbortResult::kTaskAborted) {
          is_pending_[i] = false;
          --pending_task_count_;
        } else if (stop_request == StopRequest::PREEMPT_TASKS) {
          task_state_[i].preemption_request = true;
        }
      }
    }
  }
  while (pending_task_count_ > 0) {
    pending_condition_.Wait(&pending_lock_);
  }
  for (int i = 1; i <= task_count_; i++) {
    DCHECK(!is_pending_[i]);
  }
  return true;
}

bool ConcurrentMarking::IsStopped() {
  if (!FLAG_concurrent_marking) return true;

  base::MutexGuard guard(&pending_lock_);
  return pending_task_count_ == 0;
}

void ConcurrentMarking::FlushMemoryChunkData(
    MajorNonAtomicMarkingState* marking_state) {
  DCHECK_EQ(pending_task_count_, 0);
  for (int i = 1; i <= task_count_; i++) {
    MemoryChunkDataMap& memory_chunk_data = task_state_[i].memory_chunk_data;
    for (auto& pair : memory_chunk_data) {
      // ClearLiveness sets the live bytes to zero.
      // Pages with zero live bytes might be already unmapped.
      MemoryChunk* memory_chunk = pair.first;
      MemoryChunkData& data = pair.second;
      if (data.live_bytes) {
        marking_state->IncrementLiveBytes(memory_chunk, data.live_bytes);
      }
      if (data.typed_slots) {
        RememberedSet<OLD_TO_OLD>::MergeTyped(memory_chunk,
                                              std::move(data.typed_slots));
      }
    }
    memory_chunk_data.clear();
    task_state_[i].marked_bytes = 0;
  }
  total_marked_bytes_ = 0;
}

void ConcurrentMarking::ClearMemoryChunkData(MemoryChunk* chunk) {
  for (int i = 1; i <= task_count_; i++) {
    auto it = task_state_[i].memory_chunk_data.find(chunk);
    if (it != task_state_[i].memory_chunk_data.end()) {
      it->second.live_bytes = 0;
      it->second.typed_slots.reset();
    }
  }
}

size_t ConcurrentMarking::TotalMarkedBytes() {
  size_t result = 0;
  for (int i = 1; i <= task_count_; i++) {
    result +=
        base::AsAtomicWord::Relaxed_Load<size_t>(&task_state_[i].marked_bytes);
  }
  result += total_marked_bytes_;
  return result;
}

ConcurrentMarking::PauseScope::PauseScope(ConcurrentMarking* concurrent_marking)
    : concurrent_marking_(concurrent_marking),
      resume_on_exit_(FLAG_concurrent_marking &&
                      concurrent_marking_->Stop(
                          ConcurrentMarking::StopRequest::PREEMPT_TASKS)) {
  DCHECK_IMPLIES(resume_on_exit_, FLAG_concurrent_marking);
}

ConcurrentMarking::PauseScope::~PauseScope() {
  if (resume_on_exit_) concurrent_marking_->RescheduleTasksIfNeeded();
}

}  // namespace internal
}  // namespace v8

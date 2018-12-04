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
#include "src/objects/hash-table-inl.h"
#include "src/utils-inl.h"
#include "src/utils.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

class ConcurrentMarkingState final
    : public MarkingStateBase<ConcurrentMarkingState, AccessMode::ATOMIC> {
 public:
  explicit ConcurrentMarkingState(LiveBytesMap* live_bytes)
      : live_bytes_(live_bytes) {}

  Bitmap* bitmap(const MemoryChunk* chunk) {
    return Bitmap::FromAddress(chunk->address() + MemoryChunk::kHeaderSize);
  }

  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    (*live_bytes_)[chunk] += by;
  }

  // The live_bytes and SetLiveBytes methods of the marking state are
  // not used by the concurrent marker.

 private:
  LiveBytesMap* live_bytes_;
};

// Helper class for storing in-object slot addresses and values.
class SlotSnapshot {
 public:
  SlotSnapshot() : number_of_slots_(0) {}
  int number_of_slots() const { return number_of_slots_; }
  Object** slot(int i) const { return snapshot_[i].first; }
  Object* value(int i) const { return snapshot_[i].second; }
  void clear() { number_of_slots_ = 0; }
  void add(Object** slot, Object* value) {
    snapshot_[number_of_slots_].first = slot;
    snapshot_[number_of_slots_].second = value;
    ++number_of_slots_;
  }

 private:
  static const int kMaxSnapshotSize = JSObject::kMaxInstanceSize / kPointerSize;
  int number_of_slots_;
  std::pair<Object**, Object*> snapshot_[kMaxSnapshotSize];
  DISALLOW_COPY_AND_ASSIGN(SlotSnapshot);
};

class ConcurrentMarkingVisitor final
    : public HeapVisitor<int, ConcurrentMarkingVisitor> {
 public:
  using BaseClass = HeapVisitor<int, ConcurrentMarkingVisitor>;

  explicit ConcurrentMarkingVisitor(
      ConcurrentMarking::MarkingWorklist* shared,
      ConcurrentMarking::MarkingWorklist* bailout, LiveBytesMap* live_bytes,
      WeakObjects* weak_objects,
      ConcurrentMarking::EmbedderTracingWorklist* embedder_objects, int task_id,
      bool embedder_tracing_enabled)
      : shared_(shared, task_id),
        bailout_(bailout, task_id),
        weak_objects_(weak_objects),
        embedder_objects_(embedder_objects, task_id),
        marking_state_(live_bytes),
        task_id_(task_id),
        embedder_tracing_enabled_(embedder_tracing_enabled) {}

  template <typename T>
  static V8_INLINE T* Cast(HeapObject* object) {
    return T::cast(object);
  }

  bool ShouldVisit(HeapObject* object) {
    return marking_state_.GreyToBlack(object);
  }

  bool AllowDefaultJSObjectVisit() { return false; }

  void ProcessStrongHeapObject(HeapObject* host, Object** slot,
                               HeapObject* heap_object) {
    MarkObject(heap_object);
    MarkCompactCollector::RecordSlot(host, slot, heap_object);
  }

  void ProcessWeakHeapObject(HeapObject* host, HeapObjectReference** slot,
                             HeapObject* heap_object) {
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

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    for (Object** slot = start; slot < end; slot++) {
      Object* object = base::AsAtomicPointer::Relaxed_Load(slot);
      DCHECK(!HasWeakHeapObjectTag(object));
      if (object->IsHeapObject()) {
        ProcessStrongHeapObject(host, slot, HeapObject::cast(object));
      }
    }
  }

  void VisitPointers(HeapObject* host, MaybeObject** start,
                     MaybeObject** end) override {
    for (MaybeObject** slot = start; slot < end; slot++) {
      MaybeObject* object = base::AsAtomicPointer::Relaxed_Load(slot);
      HeapObject* heap_object;
      if (object->GetHeapObjectIfStrong(&heap_object)) {
        // If the reference changes concurrently from strong to weak, the write
        // barrier will treat the weak reference as strong, so we won't miss the
        // weak reference.
        ProcessStrongHeapObject(host, reinterpret_cast<Object**>(slot),
                                heap_object);
      } else if (object->GetHeapObjectIfWeak(&heap_object)) {
        ProcessWeakHeapObject(
            host, reinterpret_cast<HeapObjectReference**>(slot), heap_object);
      }
    }
  }

  // Weak list pointers should be ignored during marking. The lists are
  // reconstructed after GC.
  void VisitCustomWeakPointers(HeapObject* host, Object** start,
                               Object** end) override {}

  void VisitPointersInSnapshot(HeapObject* host, const SlotSnapshot& snapshot) {
    for (int i = 0; i < snapshot.number_of_slots(); i++) {
      Object** slot = snapshot.slot(i);
      Object* object = snapshot.value(i);
      DCHECK(!HasWeakHeapObjectTag(object));
      if (!object->IsHeapObject()) continue;
      HeapObject* heap_object = HeapObject::cast(object);
      MarkObject(heap_object);
      MarkCompactCollector::RecordSlot(host, slot, heap_object);
    }
  }

  // ===========================================================================
  // JS object =================================================================
  // ===========================================================================

  int VisitJSObject(Map* map, JSObject* object) {
    return VisitJSObjectSubclass(map, object);
  }

  int VisitJSObjectFast(Map* map, JSObject* object) {
    return VisitJSObjectSubclass(map, object);
  }

  int VisitWasmInstanceObject(Map* map, WasmInstanceObject* object) {
    return VisitJSObjectSubclass(map, object);
  }

  // Some JS objects can carry back links to embedders that contain information
  // relevant to the garbage collectors.

  int VisitJSApiObject(Map* map, JSObject* object) {
    return VisitEmbedderTracingSubclass(map, object);
  }

  int VisitJSArrayBuffer(Map* map, JSArrayBuffer* object) {
    return VisitEmbedderTracingSubclass(map, object);
  }

  int VisitJSDataView(Map* map, JSDataView* object) {
    return VisitEmbedderTracingSubclass(map, object);
  }

  int VisitJSTypedArray(Map* map, JSTypedArray* object) {
    return VisitEmbedderTracingSubclass(map, object);
  }

  // ===========================================================================
  // Strings with pointers =====================================================
  // ===========================================================================

  int VisitConsString(Map* map, ConsString* object) {
    int size = ConsString::BodyDescriptor::SizeOf(map, object);
    const SlotSnapshot& snapshot = MakeSlotSnapshot(map, object, size);
    if (!ShouldVisit(object)) return 0;
    VisitPointersInSnapshot(object, snapshot);
    return size;
  }

  int VisitSlicedString(Map* map, SlicedString* object) {
    int size = SlicedString::BodyDescriptor::SizeOf(map, object);
    const SlotSnapshot& snapshot = MakeSlotSnapshot(map, object, size);
    if (!ShouldVisit(object)) return 0;
    VisitPointersInSnapshot(object, snapshot);
    return size;
  }

  int VisitThinString(Map* map, ThinString* object) {
    int size = ThinString::BodyDescriptor::SizeOf(map, object);
    const SlotSnapshot& snapshot = MakeSlotSnapshot(map, object, size);
    if (!ShouldVisit(object)) return 0;
    VisitPointersInSnapshot(object, snapshot);
    return size;
  }

  // ===========================================================================
  // Strings without pointers ==================================================
  // ===========================================================================

  int VisitSeqOneByteString(Map* map, SeqOneByteString* object) {
    int size = SeqOneByteString::SizeFor(object->synchronized_length());
    if (!ShouldVisit(object)) return 0;
    VisitMapPointer(object, object->map_slot());
    return size;
  }

  int VisitSeqTwoByteString(Map* map, SeqTwoByteString* object) {
    int size = SeqTwoByteString::SizeFor(object->synchronized_length());
    if (!ShouldVisit(object)) return 0;
    VisitMapPointer(object, object->map_slot());
    return size;
  }

  // ===========================================================================
  // Fixed array object ========================================================
  // ===========================================================================

  int VisitFixedArray(Map* map, FixedArray* object) {
    return VisitLeftTrimmableArray(map, object);
  }

  int VisitFixedDoubleArray(Map* map, FixedDoubleArray* object) {
    return VisitLeftTrimmableArray(map, object);
  }

  // ===========================================================================
  // Code object ===============================================================
  // ===========================================================================

  int VisitCode(Map* map, Code* object) {
    bailout_.Push(object);
    return 0;
  }

  // ===========================================================================
  // Side-effectful visitation.
  // ===========================================================================

  int VisitBytecodeArray(Map* map, BytecodeArray* object) {
    if (!ShouldVisit(object)) return 0;
    int size = BytecodeArray::BodyDescriptor::SizeOf(map, object);
    VisitMapPointer(object, object->map_slot());
    BytecodeArray::BodyDescriptor::IterateBody(map, object, size, this);
    object->MakeOlder();
    return size;
  }

  int VisitMap(Map* meta_map, Map* map) {
    if (marking_state_.IsGrey(map)) {
      // Maps have ad-hoc weakness for descriptor arrays. They also clear the
      // code-cache. Conservatively visit strong fields skipping the
      // descriptor array field and the code cache field.
      VisitMapPointer(map, map->map_slot());
      VisitPointer(map, HeapObject::RawField(map, Map::kPrototypeOffset));
      VisitPointer(
          map, HeapObject::RawField(map, Map::kConstructorOrBackPointerOffset));
      VisitPointer(map, HeapObject::RawMaybeWeakField(
                            map, Map::kTransitionsOrPrototypeInfoOffset));
      VisitPointer(map, HeapObject::RawField(map, Map::kDependentCodeOffset));
      bailout_.Push(map);
    }
    return 0;
  }

  int VisitTransitionArray(Map* map, TransitionArray* array) {
    if (!ShouldVisit(array)) return 0;
    VisitMapPointer(array, array->map_slot());
    int size = TransitionArray::BodyDescriptor::SizeOf(map, array);
    TransitionArray::BodyDescriptor::IterateBody(map, array, size, this);
    weak_objects_->transition_arrays.Push(task_id_, array);
    return size;
  }

  int VisitJSWeakCollection(Map* map, JSWeakCollection* object) {
    return VisitJSObjectSubclass(map, object);
  }

  int VisitEphemeronHashTable(Map* map, EphemeronHashTable* table) {
    if (!ShouldVisit(table)) return 0;
    weak_objects_->ephemeron_hash_tables.Push(task_id_, table);

    for (int i = 0; i < table->Capacity(); i++) {
      Object** key_slot =
          table->RawFieldOfElementAt(EphemeronHashTable::EntryToIndex(i));
      HeapObject* key = HeapObject::cast(table->KeyAt(i));
      MarkCompactCollector::RecordSlot(table, key_slot, key);

      Object** value_slot =
          table->RawFieldOfElementAt(EphemeronHashTable::EntryToValueIndex(i));

      if (marking_state_.IsBlackOrGrey(key)) {
        VisitPointer(table, value_slot);

      } else {
        Object* value_obj = table->ValueAt(i);

        if (value_obj->IsHeapObject()) {
          HeapObject* value = HeapObject::cast(value_obj);
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
  bool VisitEphemeron(HeapObject* key, HeapObject* value) {
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

  void MarkObject(HeapObject* object) {
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

 private:
  // Helper class for collecting in-object slot addresses and values.
  class SlotSnapshottingVisitor final : public ObjectVisitor {
   public:
    explicit SlotSnapshottingVisitor(SlotSnapshot* slot_snapshot)
        : slot_snapshot_(slot_snapshot) {
      slot_snapshot_->clear();
    }

    void VisitPointers(HeapObject* host, Object** start,
                       Object** end) override {
      for (Object** p = start; p < end; p++) {
        Object* object = reinterpret_cast<Object*>(
            base::Relaxed_Load(reinterpret_cast<const base::AtomicWord*>(p)));
        slot_snapshot_->add(p, object);
      }
    }

    void VisitPointers(HeapObject* host, MaybeObject** start,
                       MaybeObject** end) override {
      // This should never happen, because we don't use snapshotting for objects
      // which contain weak references.
      UNREACHABLE();
    }

   private:
    SlotSnapshot* slot_snapshot_;
  };

  template <typename T>
  int VisitJSObjectSubclass(Map* map, T* object) {
    int size = T::BodyDescriptor::SizeOf(map, object);
    int used_size = map->UsedInstanceSize();
    DCHECK_LE(used_size, size);
    DCHECK_GE(used_size, T::kHeaderSize);
    const SlotSnapshot& snapshot = MakeSlotSnapshot(map, object, used_size);
    if (!ShouldVisit(object)) return 0;
    VisitPointersInSnapshot(object, snapshot);
    return size;
  }

  template <typename T>
  int VisitEmbedderTracingSubclass(Map* map, T* object) {
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
  int VisitLeftTrimmableArray(Map* map, T* object) {
    // The synchronized_length() function checks that the length is a Smi.
    // This is not necessarily the case if the array is being left-trimmed.
    Object* length = object->unchecked_synchronized_length();
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
  const SlotSnapshot& MakeSlotSnapshot(Map* map, T* object, int size) {
    SlotSnapshottingVisitor visitor(&slot_snapshot_);
    visitor.VisitPointer(object,
                         reinterpret_cast<Object**>(object->map_slot()));
    T::BodyDescriptor::IterateBody(map, object, size, &visitor);
    return slot_snapshot_;
  }

  ConcurrentMarking::MarkingWorklist::View shared_;
  ConcurrentMarking::MarkingWorklist::View bailout_;
  WeakObjects* weak_objects_;
  ConcurrentMarking::EmbedderTracingWorklist::View embedder_objects_;
  ConcurrentMarkingState marking_state_;
  int task_id_;
  SlotSnapshot slot_snapshot_;
  bool embedder_tracing_enabled_;
};

// Strings can change maps due to conversion to thin string or external strings.
// Use reinterpret cast to avoid data race in slow dchecks.
template <>
ConsString* ConcurrentMarkingVisitor::Cast(HeapObject* object) {
  return reinterpret_cast<ConsString*>(object);
}

template <>
SlicedString* ConcurrentMarkingVisitor::Cast(HeapObject* object) {
  return reinterpret_cast<SlicedString*>(object);
}

template <>
ThinString* ConcurrentMarkingVisitor::Cast(HeapObject* object) {
  return reinterpret_cast<ThinString*>(object);
}

template <>
SeqOneByteString* ConcurrentMarkingVisitor::Cast(HeapObject* object) {
  return reinterpret_cast<SeqOneByteString*>(object);
}

template <>
SeqTwoByteString* ConcurrentMarkingVisitor::Cast(HeapObject* object) {
  return reinterpret_cast<SeqTwoByteString*>(object);
}

// Fixed array can become a free space during left trimming.
template <>
FixedArray* ConcurrentMarkingVisitor::Cast(HeapObject* object) {
  return reinterpret_cast<FixedArray*>(object);
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
                                     MarkingWorklist* bailout,
                                     MarkingWorklist* on_hold,
                                     WeakObjects* weak_objects,
                                     EmbedderTracingWorklist* embedder_objects)
    : heap_(heap),
      shared_(shared),
      bailout_(bailout),
      on_hold_(on_hold),
      weak_objects_(weak_objects),
      embedder_objects_(embedder_objects) {
// The runtime flag should be set only if the compile time flag was set.
#ifndef V8_CONCURRENT_MARKING
  CHECK(!FLAG_concurrent_marking);
#endif
}

void ConcurrentMarking::Run(int task_id, TaskState* task_state) {
  TRACE_BACKGROUND_GC(heap_->tracer(),
                      GCTracer::BackgroundScope::MC_BACKGROUND_MARKING);
  size_t kBytesUntilInterruptCheck = 64 * KB;
  int kObjectsUntilInterrupCheck = 1000;
  ConcurrentMarkingVisitor visitor(
      shared_, bailout_, &task_state->live_bytes, weak_objects_,
      embedder_objects_, task_id, heap_->local_embedder_heap_tracer()->InUse());
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
        if (visitor.VisitEphemeron(ephemeron.key, ephemeron.value)) {
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
        HeapObject* object;
        if (!shared_->Pop(task_id, &object)) {
          done = true;
          break;
        }
        objects_processed++;
        Address new_space_top = heap_->new_space()->original_top();
        Address new_space_limit = heap_->new_space()->original_limit();
        Address addr = object->address();
        if (new_space_top <= addr && addr < new_space_limit) {
          on_hold_->Push(task_id, object);
        } else {
          Map* map = object->synchronized_map();
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
        if (visitor.VisitEphemeron(ephemeron.key, ephemeron.value)) {
          ephemeron_marked = true;
        }
      }
    }

    shared_->FlushToGlobal(task_id);
    bailout_->FlushToGlobal(task_id);
    on_hold_->FlushToGlobal(task_id);
    embedder_objects_->FlushToGlobal(task_id);

    weak_objects_->transition_arrays.FlushToGlobal(task_id);
    weak_objects_->ephemeron_hash_tables.FlushToGlobal(task_id);
    weak_objects_->current_ephemerons.FlushToGlobal(task_id);
    weak_objects_->next_ephemerons.FlushToGlobal(task_id);
    weak_objects_->discovered_ephemerons.FlushToGlobal(task_id);
    weak_objects_->weak_references.FlushToGlobal(task_id);
    base::AsAtomicWord::Relaxed_Store<size_t>(&task_state->marked_bytes, 0);
    total_marked_bytes_ += marked_bytes;

    if (ephemeron_marked) {
      set_ephemeron_marked(true);
    }

    {
      base::LockGuard<base::Mutex> guard(&pending_lock_);
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
  DCHECK(!heap_->IsTearingDown());
  if (!FLAG_concurrent_marking) return;
  base::LockGuard<base::Mutex> guard(&pending_lock_);
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
  if (!FLAG_concurrent_marking || heap_->IsTearingDown()) return;
  {
    base::LockGuard<base::Mutex> guard(&pending_lock_);
    if (pending_task_count_ > 0) return;
  }
  if (!shared_->IsGlobalPoolEmpty() ||
      !weak_objects_->current_ephemerons.IsEmpty() ||
      !weak_objects_->discovered_ephemerons.IsEmpty()) {
    ScheduleTasks();
  }
}

bool ConcurrentMarking::Stop(StopRequest stop_request) {
  if (!FLAG_concurrent_marking) return false;
  base::LockGuard<base::Mutex> guard(&pending_lock_);

  if (pending_task_count_ == 0) return false;

  if (stop_request != StopRequest::COMPLETE_TASKS_FOR_TESTING) {
    CancelableTaskManager* task_manager =
        heap_->isolate()->cancelable_task_manager();
    for (int i = 1; i <= task_count_; i++) {
      if (is_pending_[i]) {
        if (task_manager->TryAbort(cancelable_id_[i]) ==
            CancelableTaskManager::kTaskAborted) {
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

  base::LockGuard<base::Mutex> guard(&pending_lock_);
  return pending_task_count_ == 0;
}

void ConcurrentMarking::FlushLiveBytes(
    MajorNonAtomicMarkingState* marking_state) {
  DCHECK_EQ(pending_task_count_, 0);
  for (int i = 1; i <= task_count_; i++) {
    LiveBytesMap& live_bytes = task_state_[i].live_bytes;
    for (auto pair : live_bytes) {
      // ClearLiveness sets the live bytes to zero.
      // Pages with zero live bytes might be already unmapped.
      if (pair.second != 0) {
        marking_state->IncrementLiveBytes(pair.first, pair.second);
      }
    }
    live_bytes.clear();
    task_state_[i].marked_bytes = 0;
  }
  total_marked_bytes_ = 0;
}

void ConcurrentMarking::ClearLiveness(MemoryChunk* chunk) {
  for (int i = 1; i <= task_count_; i++) {
    if (task_state_[i].live_bytes.count(chunk)) {
      task_state_[i].live_bytes[chunk] = 0;
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
      resume_on_exit_(concurrent_marking_->Stop(
          ConcurrentMarking::StopRequest::PREEMPT_TASKS)) {
  DCHECK_IMPLIES(resume_on_exit_, FLAG_concurrent_marking);
}

ConcurrentMarking::PauseScope::~PauseScope() {
  if (resume_on_exit_) concurrent_marking_->RescheduleTasksIfNeeded();
}

}  // namespace internal
}  // namespace v8

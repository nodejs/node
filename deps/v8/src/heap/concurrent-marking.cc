// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/concurrent-marking.h"

#include <stack>
#include <unordered_map>

#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/worklist.h"
#include "src/isolate.h"
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

  explicit ConcurrentMarkingVisitor(ConcurrentMarking::MarkingWorklist* shared,
                                    ConcurrentMarking::MarkingWorklist* bailout,
                                    LiveBytesMap* live_bytes,
                                    WeakObjects* weak_objects, int task_id)
      : shared_(shared, task_id),
        bailout_(bailout, task_id),
        weak_objects_(weak_objects),
        marking_state_(live_bytes),
        task_id_(task_id) {}

  template <typename T>
  static V8_INLINE T* Cast(HeapObject* object) {
    return T::cast(object);
  }

  bool ShouldVisit(HeapObject* object) {
    return marking_state_.GreyToBlack(object);
  }

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    for (Object** slot = start; slot < end; slot++) {
      Object* object = base::AsAtomicPointer::Relaxed_Load(slot);
      if (!object->IsHeapObject()) continue;
      MarkObject(HeapObject::cast(object));
      MarkCompactCollector::RecordSlot(host, slot, object);
    }
  }

  void VisitPointersInSnapshot(HeapObject* host, const SlotSnapshot& snapshot) {
    for (int i = 0; i < snapshot.number_of_slots(); i++) {
      Object** slot = snapshot.slot(i);
      Object* object = snapshot.value(i);
      if (!object->IsHeapObject()) continue;
      MarkObject(HeapObject::cast(object));
      MarkCompactCollector::RecordSlot(host, slot, object);
    }
  }

  // ===========================================================================
  // JS object =================================================================
  // ===========================================================================

  int VisitJSObject(Map* map, JSObject* object) {
    int size = JSObject::BodyDescriptor::SizeOf(map, object);
    int used_size = map->UsedInstanceSize();
    DCHECK_LE(used_size, size);
    DCHECK_GE(used_size, JSObject::kHeaderSize);
    const SlotSnapshot& snapshot = MakeSlotSnapshot(map, object, used_size);
    if (!ShouldVisit(object)) return 0;
    VisitPointersInSnapshot(object, snapshot);
    return size;
  }

  int VisitJSObjectFast(Map* map, JSObject* object) {
    return VisitJSObject(map, object);
  }

  int VisitJSApiObject(Map* map, JSObject* object) {
    if (marking_state_.IsGrey(object)) {
      // The main thread will do wrapper tracing in Blink.
      bailout_.Push(object);
    }
    return 0;
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
    int length = object->synchronized_length();
    int size = FixedArray::SizeFor(length);
    if (!ShouldVisit(object)) return 0;
    VisitMapPointer(object, object->map_slot());
    FixedArray::BodyDescriptor::IterateBody(object, size, this);
    return size;
  }

  // ===========================================================================
  // Code object ===============================================================
  // ===========================================================================

  int VisitCode(Map* map, Code* object) {
    bailout_.Push(object);
    return 0;
  }

  // ===========================================================================
  // Objects with weak fields and/or side-effectiful visitation.
  // ===========================================================================

  int VisitBytecodeArray(Map* map, BytecodeArray* object) {
    if (!ShouldVisit(object)) return 0;
    int size = BytecodeArray::BodyDescriptorWeak::SizeOf(map, object);
    VisitMapPointer(object, object->map_slot());
    BytecodeArray::BodyDescriptorWeak::IterateBody(object, size, this);
    object->MakeOlder();
    return size;
  }

  int VisitAllocationSite(Map* map, AllocationSite* object) {
    if (!ShouldVisit(object)) return 0;
    int size = AllocationSite::BodyDescriptorWeak::SizeOf(map, object);
    VisitMapPointer(object, object->map_slot());
    AllocationSite::BodyDescriptorWeak::IterateBody(object, size, this);
    return size;
  }

  int VisitCodeDataContainer(Map* map, CodeDataContainer* object) {
    if (!ShouldVisit(object)) return 0;
    int size = CodeDataContainer::BodyDescriptorWeak::SizeOf(map, object);
    VisitMapPointer(object, object->map_slot());
    CodeDataContainer::BodyDescriptorWeak::IterateBody(object, size, this);
    return size;
  }

  int VisitJSFunction(Map* map, JSFunction* object) {
    if (!ShouldVisit(object)) return 0;
    int size = JSFunction::BodyDescriptorWeak::SizeOf(map, object);
    VisitMapPointer(object, object->map_slot());
    JSFunction::BodyDescriptorWeak::IterateBody(object, size, this);
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
      VisitPointer(map, HeapObject::RawField(
                            map, Map::kTransitionsOrPrototypeInfoOffset));
      VisitPointer(map, HeapObject::RawField(map, Map::kDependentCodeOffset));
      VisitPointer(map, HeapObject::RawField(map, Map::kWeakCellCacheOffset));
      bailout_.Push(map);
    }
    return 0;
  }

  int VisitNativeContext(Map* map, Context* object) {
    if (!ShouldVisit(object)) return 0;
    int size = Context::BodyDescriptorWeak::SizeOf(map, object);
    VisitMapPointer(object, object->map_slot());
    Context::BodyDescriptorWeak::IterateBody(object, size, this);
    return size;
  }

  int VisitTransitionArray(Map* map, TransitionArray* array) {
    if (!ShouldVisit(array)) return 0;
    VisitMapPointer(array, array->map_slot());
    int size = TransitionArray::BodyDescriptor::SizeOf(map, array);
    TransitionArray::BodyDescriptor::IterateBody(array, size, this);
    weak_objects_->transition_arrays.Push(task_id_, array);
    return size;
  }

  int VisitWeakCell(Map* map, WeakCell* object) {
    if (!ShouldVisit(object)) return 0;
    VisitMapPointer(object, object->map_slot());
    if (!object->cleared()) {
      HeapObject* value = HeapObject::cast(object->value());
      if (marking_state_.IsBlackOrGrey(value)) {
        // Weak cells with live values are directly processed here to reduce
        // the processing time of weak cells during the main GC pause.
        Object** slot = HeapObject::RawField(object, WeakCell::kValueOffset);
        MarkCompactCollector::RecordSlot(object, slot, value);
      } else {
        // If we do not know about liveness of values of weak cells, we have to
        // process them when we know the liveness of the whole transitive
        // closure.
        weak_objects_->weak_cells.Push(task_id_, object);
      }
    }
    return WeakCell::BodyDescriptor::SizeOf(map, object);
  }

  int VisitJSWeakCollection(Map* map, JSWeakCollection* object) {
    // TODO(ulan): implement iteration of strong fields.
    bailout_.Push(object);
    return 0;
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

   private:
    SlotSnapshot* slot_snapshot_;
  };

  template <typename T>
  const SlotSnapshot& MakeSlotSnapshot(Map* map, T* object, int size) {
    // TODO(ulan): Iterate only the existing fields and skip slack at the end
    // of the object.
    SlotSnapshottingVisitor visitor(&slot_snapshot_);
    visitor.VisitPointer(object,
                         reinterpret_cast<Object**>(object->map_slot()));
    T::BodyDescriptor::IterateBody(object, size, &visitor);
    return slot_snapshot_;
  }
  ConcurrentMarking::MarkingWorklist::View shared_;
  ConcurrentMarking::MarkingWorklist::View bailout_;
  WeakObjects* weak_objects_;
  ConcurrentMarkingState marking_state_;
  int task_id_;
  SlotSnapshot slot_snapshot_;
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

class ConcurrentMarking::Task : public CancelableTask {
 public:
  Task(Isolate* isolate, ConcurrentMarking* concurrent_marking,
       TaskState* task_state, int task_id)
      : CancelableTask(isolate),
        concurrent_marking_(concurrent_marking),
        task_state_(task_state),
        task_id_(task_id) {}

  virtual ~Task() {}

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
                                     WeakObjects* weak_objects)
    : heap_(heap),
      shared_(shared),
      bailout_(bailout),
      on_hold_(on_hold),
      weak_objects_(weak_objects),
      total_marked_bytes_(0),
      pending_task_count_(0),
      task_count_(0) {
// The runtime flag should be set only if the compile time flag was set.
#ifndef V8_CONCURRENT_MARKING
  CHECK(!FLAG_concurrent_marking);
#endif
  for (int i = 0; i <= kMaxTasks; i++) {
    is_pending_[i] = false;
    task_state_[i].marked_bytes = 0;
  }
}

void ConcurrentMarking::Run(int task_id, TaskState* task_state) {
  size_t kBytesUntilInterruptCheck = 64 * KB;
  int kObjectsUntilInterrupCheck = 1000;
  LiveBytesMap* live_bytes = nullptr;
  {
    base::LockGuard<base::Mutex> guard(&task_state->lock);
    live_bytes = &task_state->live_bytes;
  }
  ConcurrentMarkingVisitor visitor(shared_, bailout_, live_bytes, weak_objects_,
                                   task_id);
  double time_ms;
  size_t marked_bytes = 0;
  if (FLAG_trace_concurrent_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "Starting concurrent marking task %d\n", task_id);
  }
  {
    TimedScope scope(&time_ms);
    bool done = false;
    while (!done) {
      base::LockGuard<base::Mutex> guard(&task_state->lock);
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
      if (task_state->interrupt_request.Value()) {
        task_state->interrupt_condition.Wait(&task_state->lock);
      }
    }
    {
      // Take the lock to synchronize with worklist update after
      // young generation GC.
      base::LockGuard<base::Mutex> guard(&task_state->lock);
      bailout_->FlushToGlobal(task_id);
      on_hold_->FlushToGlobal(task_id);
    }
    weak_objects_->weak_cells.FlushToGlobal(task_id);
    weak_objects_->transition_arrays.FlushToGlobal(task_id);
    base::AsAtomicWord::Relaxed_Store<size_t>(&task_state->marked_bytes, 0);
    total_marked_bytes_.Increment(marked_bytes);
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
  if (!FLAG_concurrent_marking) return;
  base::LockGuard<base::Mutex> guard(&pending_lock_);
  if (task_count_ == 0) {
    // TODO(ulan): Increase the number of tasks for platforms that benefit
    // from it.
    task_count_ = static_cast<int>(
        V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads() / 2);
    task_count_ = Max(Min(task_count_, kMaxTasks), 1);
  }
  // Task id 0 is for the main thread.
  for (int i = 1; i <= task_count_ && pending_task_count_ < task_count_; i++) {
    if (!is_pending_[i]) {
      if (FLAG_trace_concurrent_marking) {
        heap_->isolate()->PrintWithTimestamp(
            "Scheduling concurrent marking task %d\n", i);
      }
      task_state_[i].interrupt_request.SetValue(false);
      is_pending_[i] = true;
      ++pending_task_count_;
      Task* task = new Task(heap_->isolate(), this, &task_state_[i], i);
      cancelable_id_[i] = task->id();
      V8::GetCurrentPlatform()->CallOnBackgroundThread(
          task, v8::Platform::kShortRunningTask);
    }
  }
}

void ConcurrentMarking::RescheduleTasksIfNeeded() {
  if (!FLAG_concurrent_marking) return;
  {
    base::LockGuard<base::Mutex> guard(&pending_lock_);
    if (pending_task_count_ > 0) return;
  }
  if (!shared_->IsGlobalPoolEmpty()) {
    ScheduleTasks();
  }
}

void ConcurrentMarking::WaitForTasks() {
  if (!FLAG_concurrent_marking) return;
  base::LockGuard<base::Mutex> guard(&pending_lock_);
  while (pending_task_count_ > 0) {
    pending_condition_.Wait(&pending_lock_);
  }
}

void ConcurrentMarking::EnsureCompleted() {
  if (!FLAG_concurrent_marking) return;
  base::LockGuard<base::Mutex> guard(&pending_lock_);
  CancelableTaskManager* task_manager =
      heap_->isolate()->cancelable_task_manager();
  for (int i = 1; i <= task_count_; i++) {
    if (is_pending_[i]) {
      if (task_manager->TryAbort(cancelable_id_[i]) ==
          CancelableTaskManager::kTaskAborted) {
        is_pending_[i] = false;
        --pending_task_count_;
      }
    }
  }
  while (pending_task_count_ > 0) {
    pending_condition_.Wait(&pending_lock_);
  }
  for (int i = 1; i <= task_count_; i++) {
    DCHECK(!is_pending_[i]);
  }
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
  total_marked_bytes_.SetValue(0);
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
  result += total_marked_bytes_.Value();
  return result;
}

ConcurrentMarking::PauseScope::PauseScope(ConcurrentMarking* concurrent_marking)
    : concurrent_marking_(concurrent_marking) {
  if (!FLAG_concurrent_marking) return;
  // Request task_state for all tasks.
  for (int i = 1; i <= kMaxTasks; i++) {
    concurrent_marking_->task_state_[i].interrupt_request.SetValue(true);
  }
  // Now take a lock to ensure that the tasks are waiting.
  for (int i = 1; i <= kMaxTasks; i++) {
    concurrent_marking_->task_state_[i].lock.Lock();
  }
}

ConcurrentMarking::PauseScope::~PauseScope() {
  if (!FLAG_concurrent_marking) return;
  for (int i = kMaxTasks; i >= 1; i--) {
    concurrent_marking_->task_state_[i].interrupt_request.SetValue(false);
    concurrent_marking_->task_state_[i].interrupt_condition.NotifyAll();
    concurrent_marking_->task_state_[i].lock.Unlock();
  }
}

}  // namespace internal
}  // namespace v8

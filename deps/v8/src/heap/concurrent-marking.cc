// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/concurrent-marking.h"

#include <stack>
#include <unordered_map>

#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/marking.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/worklist.h"
#include "src/isolate.h"
#include "src/locked-queue-inl.h"
#include "src/utils-inl.h"
#include "src/utils.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

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
                                    int task_id)
      : shared_(shared, task_id), bailout_(bailout, task_id) {}

  bool ShouldVisit(HeapObject* object) {
    return ObjectMarking::GreyToBlack<AccessMode::ATOMIC>(
        object, marking_state(object));
  }

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) {
      Object* object = reinterpret_cast<Object*>(
          base::Relaxed_Load(reinterpret_cast<const base::AtomicWord*>(p)));
      if (!object->IsHeapObject()) continue;
      MarkObject(HeapObject::cast(object));
    }
  }

  void VisitPointersInSnapshot(const SlotSnapshot& snapshot) {
    for (int i = 0; i < snapshot.number_of_slots(); i++) {
      Object* object = snapshot.value(i);
      if (!object->IsHeapObject()) continue;
      MarkObject(HeapObject::cast(object));
    }
  }

  void VisitCodeEntry(JSFunction* host, Address entry_address) override {
    Address code_entry = base::AsAtomicWord::Relaxed_Load(
        reinterpret_cast<Address*>(entry_address));
    Object* code = Code::GetObjectFromCodeEntry(code_entry);
    VisitPointer(host, &code);
  }

  // ===========================================================================
  // JS object =================================================================
  // ===========================================================================

  int VisitJSObject(Map* map, JSObject* object) {
    int size = JSObject::BodyDescriptor::SizeOf(map, object);
    const SlotSnapshot& snapshot = MakeSlotSnapshot(map, object, size);
    if (!ShouldVisit(object)) return 0;
    VisitPointersInSnapshot(snapshot);
    return size;
  }

  int VisitJSObjectFast(Map* map, JSObject* object) {
    return VisitJSObject(map, object);
  }

  int VisitJSApiObject(Map* map, JSObject* object) {
    if (ObjectMarking::IsGrey<AccessMode::ATOMIC>(object,
                                                  marking_state(object))) {
      int size = JSObject::BodyDescriptor::SizeOf(map, object);
      VisitMapPointer(object, object->map_slot());
      // It is OK to iterate body of JS API object here because they do not have
      // unboxed double fields.
      DCHECK(map->HasFastPointerLayout());
      JSObject::BodyDescriptor::IterateBody(object, size, this);
      // The main thread will do wrapper tracing in Blink.
      bailout_.Push(object);
    }
    return 0;
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
    if (ObjectMarking::IsGrey<AccessMode::ATOMIC>(object,
                                                  marking_state(object))) {
      int size = BytecodeArray::BodyDescriptorWeak::SizeOf(map, object);
      VisitMapPointer(object, object->map_slot());
      BytecodeArray::BodyDescriptorWeak::IterateBody(object, size, this);
      // Aging of bytecode arrays is done on the main thread.
      bailout_.Push(object);
    }
    return 0;
  }

  int VisitAllocationSite(Map* map, AllocationSite* object) {
    if (!ShouldVisit(object)) return 0;
    int size = AllocationSite::BodyDescriptorWeak::SizeOf(map, object);
    VisitMapPointer(object, object->map_slot());
    AllocationSite::BodyDescriptorWeak::IterateBody(object, size, this);
    return size;
  }

  int VisitJSFunction(Map* map, JSFunction* object) {
    if (!ShouldVisit(object)) return 0;
    int size = JSFunction::BodyDescriptorWeak::SizeOf(map, object);
    VisitMapPointer(object, object->map_slot());
    JSFunction::BodyDescriptorWeak::IterateBody(object, size, this);
    return size;
  }

  int VisitMap(Map* map, Map* object) {
    // TODO(ulan): implement iteration of strong fields.
    bailout_.Push(object);
    return 0;
  }

  int VisitNativeContext(Map* map, Context* object) {
    if (ObjectMarking::IsGrey<AccessMode::ATOMIC>(object,
                                                  marking_state(object))) {
      int size = Context::BodyDescriptorWeak::SizeOf(map, object);
      VisitMapPointer(object, object->map_slot());
      Context::BodyDescriptorWeak::IterateBody(object, size, this);
      // TODO(ulan): implement proper weakness for normalized map cache
      // and remove this bailout.
      bailout_.Push(object);
    }
    return 0;
  }

  int VisitSharedFunctionInfo(Map* map, SharedFunctionInfo* object) {
    if (ObjectMarking::IsGrey<AccessMode::ATOMIC>(object,
                                                  marking_state(object))) {
      int size = SharedFunctionInfo::BodyDescriptorWeak::SizeOf(map, object);
      VisitMapPointer(object, object->map_slot());
      SharedFunctionInfo::BodyDescriptorWeak::IterateBody(object, size, this);
      // Resetting of IC age counter is done on the main thread.
      bailout_.Push(object);
    }
    return 0;
  }

  int VisitTransitionArray(Map* map, TransitionArray* object) {
    // TODO(ulan): implement iteration of strong fields.
    bailout_.Push(object);
    return 0;
  }

  int VisitWeakCell(Map* map, WeakCell* object) {
    // TODO(ulan): implement iteration of strong fields.
    bailout_.Push(object);
    return 0;
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
    if (ObjectMarking::WhiteToGrey<AccessMode::ATOMIC>(object,
                                                       marking_state(object))) {
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

  const SlotSnapshot& MakeSlotSnapshot(Map* map, HeapObject* object, int size) {
    // TODO(ulan): Iterate only the existing fields and skip slack at the end
    // of the object.
    SlotSnapshottingVisitor visitor(&slot_snapshot_);
    visitor.VisitPointer(object,
                         reinterpret_cast<Object**>(object->map_slot()));
    JSObject::BodyDescriptor::IterateBody(object, size, &visitor);
    return slot_snapshot_;
  }

  MarkingState marking_state(HeapObject* object) const {
    return MarkingState::Internal(object);
  }

  ConcurrentMarking::MarkingWorklist::View shared_;
  ConcurrentMarking::MarkingWorklist::View bailout_;
  SlotSnapshot slot_snapshot_;
};

class ConcurrentMarking::Task : public CancelableTask {
 public:
  Task(Isolate* isolate, ConcurrentMarking* concurrent_marking,
       base::Mutex* lock, int task_id)
      : CancelableTask(isolate),
        concurrent_marking_(concurrent_marking),
        lock_(lock),
        task_id_(task_id) {}

  virtual ~Task() {}

 private:
  // v8::internal::CancelableTask overrides.
  void RunInternal() override {
    concurrent_marking_->Run(task_id_, lock_);
  }

  ConcurrentMarking* concurrent_marking_;
  base::Mutex* lock_;
  int task_id_;
  DISALLOW_COPY_AND_ASSIGN(Task);
};

ConcurrentMarking::ConcurrentMarking(Heap* heap, MarkingWorklist* shared,
                                     MarkingWorklist* bailout)
    : heap_(heap),
      shared_(shared),
      bailout_(bailout),
      pending_task_count_(0) {
// The runtime flag should be set only if the compile time flag was set.
#ifndef V8_CONCURRENT_MARKING
  CHECK(!FLAG_concurrent_marking);
#endif
  for (int i = 0; i <= kTasks; i++) {
    is_pending_[i] = false;
  }
}

void ConcurrentMarking::Run(int task_id, base::Mutex* lock) {
  ConcurrentMarkingVisitor visitor(shared_, bailout_, task_id);
  double time_ms;
  size_t bytes_marked = 0;
  if (FLAG_trace_concurrent_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "Starting concurrent marking task %d\n", task_id);
  }
  {
    TimedScope scope(&time_ms);
    while (true) {
      base::LockGuard<base::Mutex> guard(lock);
      HeapObject* object;
      if (!shared_->Pop(task_id, &object)) break;
      Address new_space_top = heap_->new_space()->original_top();
      Address new_space_limit = heap_->new_space()->original_limit();
      Address addr = object->address();
      if (new_space_top <= addr && addr < new_space_limit) {
        bailout_->Push(task_id, object);
      } else {
        Map* map = object->synchronized_map();
        bytes_marked += visitor.Visit(map, object);
      }
    }
    {
      // Take the lock to synchronize with worklist update after
      // young generation GC.
      base::LockGuard<base::Mutex> guard(lock);
      bailout_->FlushToGlobal(task_id);
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
        static_cast<int>(bytes_marked / KB), time_ms);
  }
}

void ConcurrentMarking::ScheduleTasks() {
  if (!FLAG_concurrent_marking) return;
  base::LockGuard<base::Mutex> guard(&pending_lock_);
  if (pending_task_count_ < kTasks) {
    // Task id 0 is for the main thread.
    for (int i = 1; i <= kTasks; i++) {
      if (!is_pending_[i]) {
        if (FLAG_trace_concurrent_marking) {
          heap_->isolate()->PrintWithTimestamp(
              "Scheduling concurrent marking task %d\n", i);
        }
        is_pending_[i] = true;
        ++pending_task_count_;
        V8::GetCurrentPlatform()->CallOnBackgroundThread(
            new Task(heap_->isolate(), this, &task_lock_[i].lock, i),
            v8::Platform::kShortRunningTask);
      }
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

void ConcurrentMarking::EnsureCompleted() {
  if (!FLAG_concurrent_marking) return;
  base::LockGuard<base::Mutex> guard(&pending_lock_);
  while (pending_task_count_ > 0) {
    pending_condition_.Wait(&pending_lock_);
  }
}

ConcurrentMarking::PauseScope::PauseScope(ConcurrentMarking* concurrent_marking)
    : concurrent_marking_(concurrent_marking) {
  if (!FLAG_concurrent_marking) return;
  for (int i = 1; i <= kTasks; i++) {
    concurrent_marking_->task_lock_[i].lock.Lock();
  }
}

ConcurrentMarking::PauseScope::~PauseScope() {
  if (!FLAG_concurrent_marking) return;
  for (int i = kTasks; i >= 1; i--) {
    concurrent_marking_->task_lock_[i].lock.Unlock();
  }
}

}  // namespace internal
}  // namespace v8

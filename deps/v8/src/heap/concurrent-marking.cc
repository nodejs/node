// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/concurrent-marking.h"

#include <stack>
#include <unordered_map>

#include "include/v8config.h"
#include "src/execution/isolate.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-visitor-inl.h"
#include "src/heap/marking-visitor.h"
#include "src/heap/marking.h"
#include "src/heap/memory-measurement-inl.h"
#include "src/heap/memory-measurement.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/worklist.h"
#include "src/init/v8.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/utils/utils-inl.h"
#include "src/utils/utils.h"

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
    : public MarkingVisitorBase<ConcurrentMarkingVisitor,
                                ConcurrentMarkingState> {
 public:
  ConcurrentMarkingVisitor(int task_id, MarkingWorklists* marking_worklists,
                           WeakObjects* weak_objects, Heap* heap,
                           unsigned mark_compact_epoch,
                           BytecodeFlushMode bytecode_flush_mode,
                           bool embedder_tracing_enabled, bool is_forced_gc,
                           MemoryChunkDataMap* memory_chunk_data)
      : MarkingVisitorBase(task_id, marking_worklists, weak_objects, heap,
                           mark_compact_epoch, bytecode_flush_mode,
                           embedder_tracing_enabled, is_forced_gc),
        marking_state_(memory_chunk_data),
        memory_chunk_data_(memory_chunk_data) {}

  template <typename T>
  static V8_INLINE T Cast(HeapObject object) {
    return T::cast(object);
  }

  // HeapVisitor overrides to implement the snapshotting protocol.

  bool AllowDefaultJSObjectVisit() { return false; }

  int VisitJSObject(Map map, JSObject object) {
    return VisitJSObjectSubclass(map, object);
  }

  int VisitJSObjectFast(Map map, JSObject object) {
    return VisitJSObjectSubclassFast(map, object);
  }

  int VisitWasmInstanceObject(Map map, WasmInstanceObject object) {
    return VisitJSObjectSubclass(map, object);
  }

  int VisitJSWeakCollection(Map map, JSWeakCollection object) {
    return VisitJSObjectSubclass(map, object);
  }

  int VisitConsString(Map map, ConsString object) {
    return VisitFullyWithSnapshot(map, object);
  }

  int VisitSlicedString(Map map, SlicedString object) {
    return VisitFullyWithSnapshot(map, object);
  }

  int VisitThinString(Map map, ThinString object) {
    return VisitFullyWithSnapshot(map, object);
  }

  int VisitSeqOneByteString(Map map, SeqOneByteString object) {
    if (!ShouldVisit(object)) return 0;
    VisitMapPointer(object);
    return SeqOneByteString::SizeFor(object.synchronized_length());
  }

  int VisitSeqTwoByteString(Map map, SeqTwoByteString object) {
    if (!ShouldVisit(object)) return 0;
    VisitMapPointer(object);
    return SeqTwoByteString::SizeFor(object.synchronized_length());
  }

  // Implements ephemeron semantics: Marks value if key is already reachable.
  // Returns true if value was actually marked.
  bool ProcessEphemeron(HeapObject key, HeapObject value) {
    if (marking_state_.IsBlackOrGrey(key)) {
      if (marking_state_.WhiteToGrey(value)) {
        marking_worklists_->Push(value);
        return true;
      }

    } else if (marking_state_.IsWhite(value)) {
      weak_objects_->next_ephemerons.Push(task_id_, Ephemeron{key, value});
    }
    return false;
  }

  // HeapVisitor override.
  bool ShouldVisit(HeapObject object) {
    return marking_state_.GreyToBlack(object);
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
      DCHECK(host.IsWeakCell() || host.IsJSWeakRef());
    }

   private:
    SlotSnapshot* slot_snapshot_;
  };

  template <typename T>
  int VisitJSObjectSubclassFast(Map map, T object) {
    DCHECK_IMPLIES(FLAG_unbox_double_fields, map.HasFastPointerLayout());
    using TBodyDescriptor = typename T::FastBodyDescriptor;
    return VisitJSObjectSubclass<T, TBodyDescriptor>(map, object);
  }

  template <typename T, typename TBodyDescriptor = typename T::BodyDescriptor>
  int VisitJSObjectSubclass(Map map, T object) {
    int size = TBodyDescriptor::SizeOf(map, object);
    int used_size = map.UsedInstanceSize();
    DCHECK_LE(used_size, size);
    DCHECK_GE(used_size, JSObject::GetHeaderSize(map));
    return VisitPartiallyWithSnapshot<T, TBodyDescriptor>(map, object,
                                                          used_size, size);
  }

  template <typename T>
  int VisitLeftTrimmableArray(Map map, T object) {
    // The synchronized_length() function checks that the length is a Smi.
    // This is not necessarily the case if the array is being left-trimmed.
    Object length = object.unchecked_synchronized_length();
    if (!ShouldVisit(object)) return 0;
    // The cached length must be the actual length as the array is not black.
    // Left trimming marks the array black before over-writing the length.
    DCHECK(length.IsSmi());
    int size = T::SizeFor(Smi::ToInt(length));
    VisitMapPointer(object);
    T::BodyDescriptor::IterateBody(map, object, size, this);
    return size;
  }

  void VisitPointersInSnapshot(HeapObject host, const SlotSnapshot& snapshot) {
    for (int i = 0; i < snapshot.number_of_slots(); i++) {
      ObjectSlot slot = snapshot.slot(i);
      Object object = snapshot.value(i);
      DCHECK(!HasWeakHeapObjectTag(object));
      if (!object.IsHeapObject()) continue;
      HeapObject heap_object = HeapObject::cast(object);
      MarkObject(host, heap_object);
      RecordSlot(host, slot, heap_object);
    }
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
    visitor.VisitPointer(object, object.map_slot());
    TBodyDescriptor::IterateBody(map, object, size, &visitor);
    return slot_snapshot_;
  }

  template <typename TSlot>
  void RecordSlot(HeapObject object, TSlot slot, HeapObject target) {
    MarkCompactCollector::RecordSlot(object, slot, target);
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

  void SynchronizePageAccess(HeapObject heap_object) {
#ifdef THREAD_SANITIZER
    // This is needed because TSAN does not process the memory fence
    // emitted after page initialization.
    MemoryChunk::FromHeapObject(heap_object)->SynchronizedHeapLoad();
#endif
  }

  ConcurrentMarkingState* marking_state() { return &marking_state_; }

  TraceRetainingPathMode retaining_path_mode() {
    return TraceRetainingPathMode::kDisabled;
  }

  ConcurrentMarkingState marking_state_;
  MemoryChunkDataMap* memory_chunk_data_;
  SlotSnapshot slot_snapshot_;

  friend class MarkingVisitorBase<ConcurrentMarkingVisitor,
                                  ConcurrentMarkingState>;
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

ConcurrentMarking::ConcurrentMarking(
    Heap* heap, MarkingWorklistsHolder* marking_worklists_holder,
    WeakObjects* weak_objects)
    : heap_(heap),
      marking_worklists_holder_(marking_worklists_holder),
      weak_objects_(weak_objects) {
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
  MarkingWorklists marking_worklists(task_id, marking_worklists_holder_);
  ConcurrentMarkingVisitor visitor(
      task_id, &marking_worklists, weak_objects_, heap_,
      task_state->mark_compact_epoch, Heap::GetBytecodeFlushMode(),
      heap_->local_embedder_heap_tracer()->InUse(), task_state->is_forced_gc,
      &task_state->memory_chunk_data);
  NativeContextInferrer& native_context_inferrer =
      task_state->native_context_inferrer;
  NativeContextStats& native_context_stats = task_state->native_context_stats;
  double time_ms;
  size_t marked_bytes = 0;
  Isolate* isolate = heap_->isolate();
  if (FLAG_trace_concurrent_marking) {
    isolate->PrintWithTimestamp("Starting concurrent marking task %d\n",
                                task_id);
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
    bool is_per_context_mode = marking_worklists.IsPerContextMode();
    bool done = false;
    while (!done) {
      size_t current_marked_bytes = 0;
      int objects_processed = 0;
      while (current_marked_bytes < kBytesUntilInterruptCheck &&
             objects_processed < kObjectsUntilInterrupCheck) {
        HeapObject object;
        if (!marking_worklists.Pop(&object)) {
          done = true;
          break;
        }
        objects_processed++;
        // The order of the two loads is important.
        Address new_space_top = heap_->new_space()->original_top_acquire();
        Address new_space_limit = heap_->new_space()->original_limit_relaxed();
        Address new_large_object = heap_->new_lo_space()->pending_object();
        Address addr = object.address();
        if ((new_space_top <= addr && addr < new_space_limit) ||
            addr == new_large_object) {
          marking_worklists.PushOnHold(object);
        } else {
          Map map = object.synchronized_map(isolate);
          if (is_per_context_mode) {
            Address context;
            if (native_context_inferrer.Infer(isolate, map, object, &context)) {
              marking_worklists.SwitchToContext(context);
            }
          }
          size_t visited_size = visitor.Visit(map, object);
          if (is_per_context_mode) {
            native_context_stats.IncrementSize(marking_worklists.Context(), map,
                                               object, visited_size);
          }
          current_marked_bytes += visited_size;
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

    marking_worklists.FlushToGlobal();
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
  if (total_task_count_ == 0) {
    static const int num_cores =
        V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1;
#if defined(V8_OS_MACOSX)
    // Mac OSX 10.11 and prior seems to have trouble when doing concurrent
    // marking on competing hyper-threads (regresses Octane/Splay). As such,
    // only use num_cores/2, leaving one of those for the main thread.
    // TODO(ulan): Use all cores on Mac 10.12+.
    total_task_count_ = Max(1, Min(kMaxTasks, (num_cores / 2) - 1));
#else   // defined(OS_MACOSX)
    // On other platforms use all logical cores, leaving one for the main
    // thread.
    total_task_count_ = Max(1, Min(kMaxTasks, num_cores - 1));
#endif  // defined(OS_MACOSX)
    DCHECK_LE(total_task_count_, kMaxTasks);
    // One task is for the main thread.
    STATIC_ASSERT(kMaxTasks + 1 <= MarkingWorklist::kMaxNumTasks);
  }
  // Task id 0 is for the main thread.
  for (int i = 1; i <= total_task_count_; i++) {
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
          std::make_unique<Task>(heap_->isolate(), this, &task_state_[i], i);
      cancelable_id_[i] = task->id();
      V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
    }
  }
  DCHECK_EQ(total_task_count_, pending_task_count_);
}

void ConcurrentMarking::RescheduleTasksIfNeeded() {
  DCHECK(FLAG_parallel_marking || FLAG_concurrent_marking);
  if (heap_->IsTearingDown()) return;
  {
    base::MutexGuard guard(&pending_lock_);
    // The total task count is initialized in ScheduleTasks from
    // NumberOfWorkerThreads of the platform.
    if (total_task_count_ > 0 && pending_task_count_ == total_task_count_) {
      return;
    }
  }
  if (!marking_worklists_holder_->shared()->IsGlobalPoolEmpty() ||
      !weak_objects_->current_ephemerons.IsGlobalPoolEmpty() ||
      !weak_objects_->discovered_ephemerons.IsGlobalPoolEmpty()) {
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
    for (int i = 1; i <= total_task_count_; i++) {
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
  for (int i = 1; i <= total_task_count_; i++) {
    DCHECK(!is_pending_[i]);
  }
  return true;
}

bool ConcurrentMarking::IsStopped() {
  if (!FLAG_concurrent_marking) return true;

  base::MutexGuard guard(&pending_lock_);
  return pending_task_count_ == 0;
}

void ConcurrentMarking::FlushNativeContexts(NativeContextStats* main_stats) {
  for (int i = 1; i <= total_task_count_; i++) {
    main_stats->Merge(task_state_[i].native_context_stats);
    task_state_[i].native_context_stats.Clear();
  }
}

void ConcurrentMarking::FlushMemoryChunkData(
    MajorNonAtomicMarkingState* marking_state) {
  DCHECK_EQ(pending_task_count_, 0);
  for (int i = 1; i <= total_task_count_; i++) {
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
  for (int i = 1; i <= total_task_count_; i++) {
    auto it = task_state_[i].memory_chunk_data.find(chunk);
    if (it != task_state_[i].memory_chunk_data.end()) {
      it->second.live_bytes = 0;
      it->second.typed_slots.reset();
    }
  }
}

size_t ConcurrentMarking::TotalMarkedBytes() {
  size_t result = 0;
  for (int i = 1; i <= total_task_count_; i++) {
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

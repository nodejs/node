// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/concurrent-marking.h"

#include <stack>
#include <unordered_map>

#include "include/v8config.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-visitor-inl.h"
#include "src/heap/marking-visitor.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/memory-measurement-inl.h"
#include "src/heap/memory-measurement.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/weak-object-worklists.h"
#include "src/init/v8.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/objects/visitors.h"
#include "src/utils/utils-inl.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

class ConcurrentMarkingState final
    : public MarkingStateBase<ConcurrentMarkingState, AccessMode::ATOMIC> {
 public:
  ConcurrentMarkingState(PtrComprCageBase cage_base,
                         MemoryChunkDataMap* memory_chunk_data)
      : MarkingStateBase(cage_base), memory_chunk_data_(memory_chunk_data) {}

  ConcurrentBitmap<AccessMode::ATOMIC>* bitmap(const BasicMemoryChunk* chunk) {
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
  SlotSnapshot(const SlotSnapshot&) = delete;
  SlotSnapshot& operator=(const SlotSnapshot&) = delete;
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
};

class ConcurrentMarkingVisitor final
    : public MarkingVisitorBase<ConcurrentMarkingVisitor,
                                ConcurrentMarkingState> {
 public:
  ConcurrentMarkingVisitor(int task_id,
                           MarkingWorklists::Local* local_marking_worklists,
                           WeakObjects::Local* local_weak_objects, Heap* heap,
                           unsigned mark_compact_epoch,
                           base::EnumSet<CodeFlushMode> code_flush_mode,
                           bool embedder_tracing_enabled,
                           bool should_keep_ages_unchanged,
                           MemoryChunkDataMap* memory_chunk_data)
      : MarkingVisitorBase(local_marking_worklists, local_weak_objects, heap,
                           mark_compact_epoch, code_flush_mode,
                           embedder_tracing_enabled,
                           should_keep_ages_unchanged),
        marking_state_(heap->isolate(), memory_chunk_data),
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

  int VisitJSExternalObject(Map map, JSExternalObject object) {
    return VisitJSObjectSubclass(map, object);
  }

#if V8_ENABLE_WEBASSEMBLY
  int VisitWasmInstanceObject(Map map, WasmInstanceObject object) {
    return VisitJSObjectSubclass(map, object);
  }
  int VisitWasmSuspenderObject(Map map, WasmSuspenderObject object) {
    return VisitJSObjectSubclass(map, object);
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  int VisitJSWeakCollection(Map map, JSWeakCollection object) {
    return VisitJSObjectSubclass(map, object);
  }

  int VisitJSFinalizationRegistry(Map map, JSFinalizationRegistry object) {
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
    return SeqOneByteString::SizeFor(object.length(kAcquireLoad));
  }

  int VisitSeqTwoByteString(Map map, SeqTwoByteString object) {
    if (!ShouldVisit(object)) return 0;
    VisitMapPointer(object);
    return SeqTwoByteString::SizeFor(object.length(kAcquireLoad));
  }

  // Implements ephemeron semantics: Marks value if key is already reachable.
  // Returns true if value was actually marked.
  bool ProcessEphemeron(HeapObject key, HeapObject value) {
    if (marking_state_.IsBlackOrGrey(key)) {
      if (marking_state_.WhiteToGrey(value)) {
        local_marking_worklists_->Push(value);
        return true;
      }

    } else if (marking_state_.IsWhite(value)) {
      local_weak_objects_->next_ephemerons_local.Push(Ephemeron{key, value});
    }
    return false;
  }

  // HeapVisitor override.
  bool ShouldVisit(HeapObject object) {
    return marking_state_.GreyToBlack(object);
  }

  bool ShouldVisitUnaccounted(HeapObject object) {
    return marking_state_.GreyToBlackUnaccounted(object);
  }

 private:
  // Helper class for collecting in-object slot addresses and values.
  class SlotSnapshottingVisitor final : public ObjectVisitorWithCageBases {
   public:
    explicit SlotSnapshottingVisitor(SlotSnapshot* slot_snapshot,
                                     PtrComprCageBase cage_base,
                                     PtrComprCageBase code_cage_base)
        : ObjectVisitorWithCageBases(cage_base, code_cage_base),
          slot_snapshot_(slot_snapshot) {
      slot_snapshot_->clear();
    }

    void VisitPointers(HeapObject host, ObjectSlot start,
                       ObjectSlot end) override {
      for (ObjectSlot p = start; p < end; ++p) {
        Object object = p.Relaxed_Load(cage_base());
        slot_snapshot_->add(p, object);
      }
    }

    void VisitCodePointer(HeapObject host, CodeObjectSlot slot) override {
      CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
      Object code = slot.Relaxed_Load(code_cage_base());
      slot_snapshot_->add(ObjectSlot(slot.address()), code);
    }

    void VisitPointers(HeapObject host, MaybeObjectSlot start,
                       MaybeObjectSlot end) override {
      // This should never happen, because we don't use snapshotting for objects
      // which contain weak references.
      UNREACHABLE();
    }

    void VisitCodeTarget(Code host, RelocInfo* rinfo) final {
      // This should never happen, because snapshotting is performed only on
      // some String subclasses.
      UNREACHABLE();
    }

    void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final {
      // This should never happen, because snapshotting is performed only on
      // some String subclasses.
      UNREACHABLE();
    }

    void VisitCustomWeakPointers(HeapObject host, ObjectSlot start,
                                 ObjectSlot end) override {
      // This should never happen, because snapshotting is performed only on
      // some String subclasses.
      UNREACHABLE();
    }

   private:
    SlotSnapshot* slot_snapshot_;
  };

  template <typename T>
  int VisitJSObjectSubclassFast(Map map, T object) {
    using TBodyDescriptor = typename T::FastBodyDescriptor;
    return VisitJSObjectSubclass<T, TBodyDescriptor>(map, object);
  }

  template <typename T, typename TBodyDescriptor = typename T::BodyDescriptor>
  int VisitJSObjectSubclass(Map map, T object) {
    if (!ShouldVisit(object)) return 0;
    int size = TBodyDescriptor::SizeOf(map, object);
    int used_size = map.UsedInstanceSize();
    DCHECK_LE(used_size, size);
    DCHECK_GE(used_size, JSObject::GetHeaderSize(map));
    this->VisitMapPointer(object);
    // It is important to visit only the used field and ignore the slack fields
    // because the slack fields may be trimmed concurrently.
    TBodyDescriptor::IterateBody(map, object, used_size, this);
    return size;
  }

  template <typename T>
  int VisitLeftTrimmableArray(Map map, T object) {
    // The length() function checks that the length is a Smi.
    // This is not necessarily the case if the array is being left-trimmed.
    Object length = object.unchecked_length(kAcquireLoad);
    // No accounting here to avoid re-reading the length which could already
    // contain a non-SMI value when left-trimming happens concurrently.
    if (!ShouldVisitUnaccounted(object)) return 0;
    // The cached length must be the actual length as the array is not black.
    // Left trimming marks the array black before over-writing the length.
    DCHECK(length.IsSmi());
    int size = T::SizeFor(Smi::ToInt(length));
    marking_state_.IncrementLiveBytes(MemoryChunk::FromHeapObject(object),
                                      size);
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
      concrete_visitor()->SynchronizePageAccess(heap_object);
      BasicMemoryChunk* target_page =
          BasicMemoryChunk::FromHeapObject(heap_object);
      if (!is_shared_heap_ && target_page->InSharedHeap()) continue;
      MarkObject(host, heap_object);
      RecordSlot(host, slot, heap_object);
    }
  }

  template <typename T>
  int VisitFullyWithSnapshot(Map map, T object) {
    using TBodyDescriptor = typename T::BodyDescriptor;
    int size = TBodyDescriptor::SizeOf(map, object);
    const SlotSnapshot& snapshot =
        MakeSlotSnapshot<T, TBodyDescriptor>(map, object, size);
    if (!ShouldVisit(object)) return 0;
    VisitPointersInSnapshot(object, snapshot);
    return size;
  }

  template <typename T, typename TBodyDescriptor>
  const SlotSnapshot& MakeSlotSnapshot(Map map, T object, int size) {
    SlotSnapshottingVisitor visitor(&slot_snapshot_, cage_base(),
                                    code_cage_base());
    visitor.VisitPointer(object, object.map_slot());
    TBodyDescriptor::IterateBody(map, object, size, &visitor);
    return slot_snapshot_;
  }

  template <typename TSlot>
  void RecordSlot(HeapObject object, TSlot slot, HeapObject target) {
    MarkCompactCollector::RecordSlot(object, slot, target);
  }

  void RecordRelocSlot(Code host, RelocInfo* rinfo, HeapObject target) {
    if (!MarkCompactCollector::ShouldRecordRelocSlot(host, rinfo, target))
      return;

    MarkCompactCollector::RecordRelocSlotInfo info =
        MarkCompactCollector::ProcessRelocInfo(host, rinfo, target);

    MemoryChunkData& data = (*memory_chunk_data_)[info.memory_chunk];
    if (!data.typed_slots) {
      data.typed_slots.reset(new TypedSlots());
    }
    data.typed_slots->Insert(info.slot_type, info.offset);
  }

  void SynchronizePageAccess(HeapObject heap_object) {
#ifdef THREAD_SANITIZER
    // This is needed because TSAN does not process the memory fence
    // emitted after page initialization.
    BasicMemoryChunk::FromHeapObject(heap_object)->SynchronizedHeapLoad();
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

// The Deserializer changes the map from StrongDescriptorArray to
// DescriptorArray
template <>
StrongDescriptorArray ConcurrentMarkingVisitor::Cast(HeapObject object) {
  return StrongDescriptorArray::unchecked_cast(DescriptorArray::cast(object));
}

class ConcurrentMarking::JobTask : public v8::JobTask {
 public:
  JobTask(ConcurrentMarking* concurrent_marking, unsigned mark_compact_epoch,
          base::EnumSet<CodeFlushMode> code_flush_mode,
          bool should_keep_ages_unchanged)
      : concurrent_marking_(concurrent_marking),
        mark_compact_epoch_(mark_compact_epoch),
        code_flush_mode_(code_flush_mode),
        should_keep_ages_unchanged_(should_keep_ages_unchanged) {}

  ~JobTask() override = default;
  JobTask(const JobTask&) = delete;
  JobTask& operator=(const JobTask&) = delete;

  // v8::JobTask overrides.
  void Run(JobDelegate* delegate) override {
    if (delegate->IsJoiningThread()) {
      // TRACE_GC is not needed here because the caller opens the right scope.
      concurrent_marking_->Run(delegate, code_flush_mode_, mark_compact_epoch_,
                               should_keep_ages_unchanged_);
    } else {
      TRACE_GC_EPOCH(concurrent_marking_->heap_->tracer(),
                     GCTracer::Scope::MC_BACKGROUND_MARKING,
                     ThreadKind::kBackground);
      concurrent_marking_->Run(delegate, code_flush_mode_, mark_compact_epoch_,
                               should_keep_ages_unchanged_);
    }
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    return concurrent_marking_->GetMaxConcurrency(worker_count);
  }

 private:
  ConcurrentMarking* concurrent_marking_;
  const unsigned mark_compact_epoch_;
  base::EnumSet<CodeFlushMode> code_flush_mode_;
  const bool should_keep_ages_unchanged_;
};

ConcurrentMarking::ConcurrentMarking(Heap* heap,
                                     MarkingWorklists* marking_worklists,
                                     WeakObjects* weak_objects)
    : heap_(heap),
      marking_worklists_(marking_worklists),
      weak_objects_(weak_objects) {
#ifndef V8_ATOMIC_OBJECT_FIELD_WRITES
  // Concurrent marking requires atomic object field writes.
  CHECK(!FLAG_concurrent_marking);
#endif
}

void ConcurrentMarking::Run(JobDelegate* delegate,
                            base::EnumSet<CodeFlushMode> code_flush_mode,
                            unsigned mark_compact_epoch,
                            bool should_keep_ages_unchanged) {
  size_t kBytesUntilInterruptCheck = 64 * KB;
  int kObjectsUntilInterrupCheck = 1000;
  uint8_t task_id = delegate->GetTaskId() + 1;
  TaskState* task_state = &task_state_[task_id];
  auto* cpp_heap = CppHeap::From(heap_->cpp_heap());
  MarkingWorklists::Local local_marking_worklists(
      marking_worklists_, cpp_heap
                              ? cpp_heap->CreateCppMarkingState()
                              : MarkingWorklists::Local::kNoCppMarkingState);
  WeakObjects::Local local_weak_objects(weak_objects_);
  ConcurrentMarkingVisitor visitor(
      task_id, &local_marking_worklists, &local_weak_objects, heap_,
      mark_compact_epoch, code_flush_mode,
      heap_->local_embedder_heap_tracer()->InUse(), should_keep_ages_unchanged,
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
  bool another_ephemeron_iteration = false;

  {
    TimedScope scope(&time_ms);

    {
      Ephemeron ephemeron;
      while (local_weak_objects.current_ephemerons_local.Pop(&ephemeron)) {
        if (visitor.ProcessEphemeron(ephemeron.key, ephemeron.value)) {
          another_ephemeron_iteration = true;
        }
      }
    }
    bool is_per_context_mode = local_marking_worklists.IsPerContextMode();
    bool done = false;
    while (!done) {
      size_t current_marked_bytes = 0;
      int objects_processed = 0;
      while (current_marked_bytes < kBytesUntilInterruptCheck &&
             objects_processed < kObjectsUntilInterrupCheck) {
        HeapObject object;
        if (!local_marking_worklists.Pop(&object)) {
          done = true;
          break;
        }
        objects_processed++;

        Address new_space_top = kNullAddress;
        Address new_space_limit = kNullAddress;
        Address new_large_object = kNullAddress;

        if (heap_->new_space()) {
          // The order of the two loads is important.
          new_space_top = heap_->new_space()->original_top_acquire();
          new_space_limit = heap_->new_space()->original_limit_relaxed();
        }

        if (heap_->new_lo_space()) {
          new_large_object = heap_->new_lo_space()->pending_object();
        }

        Address addr = object.address();

        if ((new_space_top <= addr && addr < new_space_limit) ||
            addr == new_large_object) {
          local_marking_worklists.PushOnHold(object);
        } else {
          Map map = object.map(isolate, kAcquireLoad);
          if (is_per_context_mode) {
            Address context;
            if (native_context_inferrer.Infer(isolate, map, object, &context)) {
              local_marking_worklists.SwitchToContext(context);
            }
          }
          size_t visited_size = visitor.Visit(map, object);
          if (is_per_context_mode) {
            native_context_stats.IncrementSize(
                local_marking_worklists.Context(), map, object, visited_size);
          }
          current_marked_bytes += visited_size;
        }
      }
      if (objects_processed > 0) another_ephemeron_iteration = true;
      marked_bytes += current_marked_bytes;
      base::AsAtomicWord::Relaxed_Store<size_t>(&task_state->marked_bytes,
                                                marked_bytes);
      if (delegate->ShouldYield()) {
        TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                     "ConcurrentMarking::Run Preempted");
        break;
      }
    }

    if (done) {
      Ephemeron ephemeron;
      while (local_weak_objects.discovered_ephemerons_local.Pop(&ephemeron)) {
        if (visitor.ProcessEphemeron(ephemeron.key, ephemeron.value)) {
          another_ephemeron_iteration = true;
        }
      }
    }

    local_marking_worklists.Publish();
    local_weak_objects.Publish();
    base::AsAtomicWord::Relaxed_Store<size_t>(&task_state->marked_bytes, 0);
    total_marked_bytes_ += marked_bytes;

    if (another_ephemeron_iteration) {
      set_another_ephemeron_iteration(true);
    }
  }
  if (FLAG_trace_concurrent_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "Task %d concurrently marked %dKB in %.2fms\n", task_id,
        static_cast<int>(marked_bytes / KB), time_ms);
  }
}

size_t ConcurrentMarking::GetMaxConcurrency(size_t worker_count) {
  size_t marking_items = marking_worklists_->shared()->Size();
  for (auto& worklist : marking_worklists_->context_worklists())
    marking_items += worklist.worklist->Size();
  return std::min<size_t>(
      kMaxTasks,
      worker_count +
          std::max<size_t>({marking_items,
                            weak_objects_->discovered_ephemerons.Size(),
                            weak_objects_->current_ephemerons.Size()}));
}

void ConcurrentMarking::ScheduleJob(TaskPriority priority) {
  DCHECK(FLAG_parallel_marking || FLAG_concurrent_marking);
  DCHECK(!heap_->IsTearingDown());
  DCHECK(!job_handle_ || !job_handle_->IsValid());

  job_handle_ = V8::GetCurrentPlatform()->PostJob(
      priority, std::make_unique<JobTask>(
                    this, heap_->mark_compact_collector()->epoch(),
                    heap_->mark_compact_collector()->code_flush_mode(),
                    heap_->ShouldCurrentGCKeepAgesUnchanged()));
  DCHECK(job_handle_->IsValid());
}

void ConcurrentMarking::RescheduleJobIfNeeded(TaskPriority priority) {
  DCHECK(FLAG_parallel_marking || FLAG_concurrent_marking);
  if (heap_->IsTearingDown()) return;

  if (marking_worklists_->shared()->IsEmpty() &&
      weak_objects_->current_ephemerons.IsEmpty() &&
      weak_objects_->discovered_ephemerons.IsEmpty()) {
    return;
  }
  if (!job_handle_ || !job_handle_->IsValid()) {
    ScheduleJob(priority);
  } else {
    if (priority != TaskPriority::kUserVisible)
      job_handle_->UpdatePriority(priority);
    job_handle_->NotifyConcurrencyIncrease();
  }
}

void ConcurrentMarking::Join() {
  DCHECK(FLAG_parallel_marking || FLAG_concurrent_marking);
  if (!job_handle_ || !job_handle_->IsValid()) return;
  job_handle_->Join();
}

bool ConcurrentMarking::Pause() {
  DCHECK(FLAG_parallel_marking || FLAG_concurrent_marking);
  if (!job_handle_ || !job_handle_->IsValid()) return false;

  job_handle_->Cancel();
  return true;
}

bool ConcurrentMarking::IsStopped() {
  if (!FLAG_concurrent_marking) return true;

  return !job_handle_ || !job_handle_->IsValid();
}

void ConcurrentMarking::FlushNativeContexts(NativeContextStats* main_stats) {
  DCHECK(!job_handle_ || !job_handle_->IsValid());
  for (int i = 1; i <= kMaxTasks; i++) {
    main_stats->Merge(task_state_[i].native_context_stats);
    task_state_[i].native_context_stats.Clear();
  }
}

void ConcurrentMarking::FlushMemoryChunkData(
    MajorNonAtomicMarkingState* marking_state) {
  DCHECK(!job_handle_ || !job_handle_->IsValid());
  for (int i = 1; i <= kMaxTasks; i++) {
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
  DCHECK(!job_handle_ || !job_handle_->IsValid());
  for (int i = 1; i <= kMaxTasks; i++) {
    auto it = task_state_[i].memory_chunk_data.find(chunk);
    if (it != task_state_[i].memory_chunk_data.end()) {
      it->second.live_bytes = 0;
      it->second.typed_slots.reset();
    }
  }
}

size_t ConcurrentMarking::TotalMarkedBytes() {
  size_t result = 0;
  for (int i = 1; i <= kMaxTasks; i++) {
    result +=
        base::AsAtomicWord::Relaxed_Load<size_t>(&task_state_[i].marked_bytes);
  }
  result += total_marked_bytes_;
  return result;
}

ConcurrentMarking::PauseScope::PauseScope(ConcurrentMarking* concurrent_marking)
    : concurrent_marking_(concurrent_marking),
      resume_on_exit_(FLAG_concurrent_marking && concurrent_marking_->Pause()) {
  DCHECK_IMPLIES(resume_on_exit_, FLAG_concurrent_marking);
}

ConcurrentMarking::PauseScope::~PauseScope() {
  if (resume_on_exit_) concurrent_marking_->RescheduleJobIfNeeded();
}

}  // namespace internal
}  // namespace v8

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
#include "src/heap/marking-state-inl.h"
#include "src/heap/marking-visitor-inl.h"
#include "src/heap/marking-visitor.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/memory-measurement-inl.h"
#include "src/heap/memory-measurement.h"
#include "src/heap/object-lock.h"
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

// These strings can be sources of safe string transitions. Transitions are safe
// if they don't result in invalidated slots. It's safe to read the length field
// on such strings as that's common for all.
//
// No special visitors are generated for such strings.
// V(VisitorId, Typename)
#define SAFE_STRING_TRANSITION_SOURCES(V) \
  V(SeqOneByteString, SeqOneByteString)   \
  V(SeqTwoByteString, SeqTwoByteString)

// These strings can be sources of unsafe string transitions.
// V(VisitorId, TypeName)
#define UNSAFE_STRING_TRANSITION_SOURCES(V) \
  V(ExternalString, ExternalString)         \
  V(ConsString, ConsString)                 \
  V(SlicedString, SlicedString)

// V(VisitorId, TypeName)
#define UNSAFE_STRING_TRANSITION_TARGETS(V) \
  UNSAFE_STRING_TRANSITION_SOURCES(V)       \
  V(ShortcutCandidate, ConsString)          \
  V(ThinString, ThinString)

namespace v8 {
namespace internal {

class ConcurrentMarkingState final
    : public MarkingStateBase<ConcurrentMarkingState, AccessMode::ATOMIC> {
 public:
  ConcurrentMarkingState(PtrComprCageBase cage_base,
                         MemoryChunkDataMap* memory_chunk_data)
      : MarkingStateBase(cage_base), memory_chunk_data_(memory_chunk_data) {}

  ConcurrentBitmap<AccessMode::ATOMIC>* bitmap(
      const BasicMemoryChunk* chunk) const {
    return chunk->marking_bitmap<AccessMode::ATOMIC>();
  }

  void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
    DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                   IsAligned(by, kObjectAlignment8GbHeap));
    (*memory_chunk_data_)[chunk].live_bytes += by;
  }

  // The live_bytes and SetLiveBytes methods of the marking state are
  // not used by the concurrent marker.

 private:
  MemoryChunkDataMap* memory_chunk_data_;
};

class ConcurrentMarkingVisitorUtility {
 public:
  template <typename Visitor, typename T>
  static int VisitStringLocked(Visitor* visitor, T object) {
    SharedObjectLockGuard guard(object);
    CHECK(visitor->ShouldVisit(object));
    visitor->VisitMapPointerIfNeeded(object);
    // The object has been locked. At this point exclusive access is guaranteed
    // but we must re-read the map and check whether the string has
    // transitioned.
    Map map = object.map();
    int size;
    switch (map.visitor_id()) {
#define UNSAFE_STRING_TRANSITION_TARGET_CASE(VisitorId, TypeName) \
  case kVisit##VisitorId:                                         \
    size = TypeName::BodyDescriptor::SizeOf(map, object);         \
    TypeName::BodyDescriptor::IterateBody(                        \
        map, TypeName::unchecked_cast(object), size, visitor);    \
    break;

      UNSAFE_STRING_TRANSITION_TARGETS(UNSAFE_STRING_TRANSITION_TARGET_CASE)
#undef UNSAFE_STRING_TRANSITION_TARGET_CASE
      default:
        UNREACHABLE();
    }
    return size;
  }
};

class YoungGenerationConcurrentMarkingVisitor final
    : public YoungGenerationMarkingVisitorBase<
          YoungGenerationConcurrentMarkingVisitor, ConcurrentMarkingState> {
 public:
  YoungGenerationConcurrentMarkingVisitor(
      Heap* heap, MarkingWorklists::Local* worklists_local,
      MemoryChunkDataMap* memory_chunk_data)
      : YoungGenerationMarkingVisitorBase<
            YoungGenerationConcurrentMarkingVisitor, ConcurrentMarkingState>(
            heap->isolate(), worklists_local),
        marking_state_(heap->isolate(), memory_chunk_data) {}

  using YoungGenerationMarkingVisitorBase<
      YoungGenerationConcurrentMarkingVisitor,
      ConcurrentMarkingState>::VisitMapPointerIfNeeded;

  template <typename T>
  static V8_INLINE T Cast(HeapObject object) {
    return T::cast(object);
  }

  bool ShouldVisit(HeapObject object) {
    CHECK(marking_state_.GreyToBlack(object));
    return true;
  }

#define VISIT_AS_LOCKED_STRING(VisitorId, TypeName)                          \
  int Visit##TypeName(Map map, TypeName object) {                            \
    return ConcurrentMarkingVisitorUtility::VisitStringLocked(this, object); \
  }
  UNSAFE_STRING_TRANSITION_SOURCES(VISIT_AS_LOCKED_STRING)
#undef VISIT_AS_LOCKED_STRING

  void VisitMapPointer(HeapObject host) final { UNREACHABLE(); }

  template <typename TSlot>
  void RecordSlot(HeapObject object, TSlot slot, HeapObject target) {}

  ConcurrentMarkingState* marking_state() { return &marking_state_; }

 private:
  ConcurrentMarkingState marking_state_;
};

#define UNCHECKED_CAST(VisitorId, TypeName)                                   \
  template <>                                                                 \
  TypeName YoungGenerationConcurrentMarkingVisitor::Cast(HeapObject object) { \
    return TypeName::unchecked_cast(object);                                  \
  }
SAFE_STRING_TRANSITION_SOURCES(UNCHECKED_CAST)
// Casts are also needed for unsafe ones for the initial dispatch in
// HeapVisitor.
UNSAFE_STRING_TRANSITION_SOURCES(UNCHECKED_CAST)
#undef UNCHECKED_CAST

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

  using MarkingVisitorBase<ConcurrentMarkingVisitor,
                           ConcurrentMarkingState>::VisitMapPointerIfNeeded;

  template <typename T>
  static V8_INLINE T Cast(HeapObject object) {
    return T::cast(object);
  }

  bool ShouldVisit(HeapObject object) {
    CHECK(marking_state_.GreyToBlack(object));
    return true;
  }

#define VISIT_AS_LOCKED_STRING(VisitorId, TypeName)                          \
  int Visit##TypeName(Map map, TypeName object) {                            \
    return ConcurrentMarkingVisitorUtility::VisitStringLocked(this, object); \
  }
  UNSAFE_STRING_TRANSITION_SOURCES(VISIT_AS_LOCKED_STRING)
#undef VISIT_AS_LOCKED_STRING

  // Implements ephemeron semantics: Marks value if key is already reachable.
  // Returns true if value was actually marked.
  bool ProcessEphemeron(HeapObject key, HeapObject value) {
    if (marking_state_.IsBlackOrGrey(key)) {
      if (marking_state_.TryMark(value)) {
        local_marking_worklists_->Push(value);
        return true;
      }

    } else if (marking_state_.IsUnmarked(value)) {
      local_weak_objects_->next_ephemerons_local.Push(Ephemeron{key, value});
    }
    return false;
  }

  template <typename TSlot>
  void RecordSlot(HeapObject object, TSlot slot, HeapObject target) {
    MarkCompactCollector::RecordSlot(object, slot, target);
  }

  ConcurrentMarkingState* marking_state() { return &marking_state_; }

 private:
  void RecordRelocSlot(RelocInfo* rinfo, HeapObject target) {
    if (!MarkCompactCollector::ShouldRecordRelocSlot(rinfo, target)) return;

    MarkCompactCollector::RecordRelocSlotInfo info =
        MarkCompactCollector::ProcessRelocInfo(rinfo, target);

    MemoryChunkData& data = (*memory_chunk_data_)[info.memory_chunk];
    if (!data.typed_slots) {
      data.typed_slots.reset(new TypedSlots());
    }
    data.typed_slots->Insert(info.slot_type, info.offset);
  }

  TraceRetainingPathMode retaining_path_mode() {
    return TraceRetainingPathMode::kDisabled;
  }

  ConcurrentMarkingState marking_state_;
  MemoryChunkDataMap* memory_chunk_data_;

  friend class MarkingVisitorBase<ConcurrentMarkingVisitor,
                                  ConcurrentMarkingState>;
};

#define UNCHECKED_CAST(VisitorId, TypeName)                    \
  template <>                                                  \
  TypeName ConcurrentMarkingVisitor::Cast(HeapObject object) { \
    return TypeName::unchecked_cast(object);                   \
  }
SAFE_STRING_TRANSITION_SOURCES(UNCHECKED_CAST)
// Casts are also needed for unsafe ones for the initial dispatch in
// HeapVisitor.
UNSAFE_STRING_TRANSITION_SOURCES(UNCHECKED_CAST)
#undef UNCHECKED_CAST

// The Deserializer changes the map from StrongDescriptorArray to
// DescriptorArray
template <>
StrongDescriptorArray ConcurrentMarkingVisitor::Cast(HeapObject object) {
  return StrongDescriptorArray::unchecked_cast(DescriptorArray::cast(object));
}

class ConcurrentMarking::JobTaskMajor : public v8::JobTask {
 public:
  JobTaskMajor(ConcurrentMarking* concurrent_marking,
               unsigned mark_compact_epoch,
               base::EnumSet<CodeFlushMode> code_flush_mode,
               bool should_keep_ages_unchanged)
      : concurrent_marking_(concurrent_marking),
        mark_compact_epoch_(mark_compact_epoch),
        code_flush_mode_(code_flush_mode),
        should_keep_ages_unchanged_(should_keep_ages_unchanged) {}

  ~JobTaskMajor() override = default;
  JobTaskMajor(const JobTaskMajor&) = delete;
  JobTaskMajor& operator=(const JobTaskMajor&) = delete;

  // v8::JobTask overrides.
  void Run(JobDelegate* delegate) override {
    if (delegate->IsJoiningThread()) {
      // TRACE_GC is not needed here because the caller opens the right scope.
      concurrent_marking_->RunMajor(delegate, code_flush_mode_,
                                    mark_compact_epoch_,
                                    should_keep_ages_unchanged_);
    } else {
      TRACE_GC_EPOCH(concurrent_marking_->heap_->tracer(),
                     GCTracer::Scope::MC_BACKGROUND_MARKING,
                     ThreadKind::kBackground);
      concurrent_marking_->RunMajor(delegate, code_flush_mode_,
                                    mark_compact_epoch_,
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

class ConcurrentMarking::JobTaskMinor : public v8::JobTask {
 public:
  explicit JobTaskMinor(ConcurrentMarking* concurrent_marking)
      : concurrent_marking_(concurrent_marking) {}

  ~JobTaskMinor() override = default;
  JobTaskMinor(const JobTaskMinor&) = delete;
  JobTaskMinor& operator=(const JobTaskMinor&) = delete;

  // v8::JobTask overrides.
  void Run(JobDelegate* delegate) override {
    if (delegate->IsJoiningThread()) {
      // TRACE_GC is not needed here because the caller opens the right scope.
      concurrent_marking_->RunMinor(delegate);
    } else {
      TRACE_GC_EPOCH(concurrent_marking_->heap_->tracer(),
                     GCTracer::Scope::MINOR_MC_BACKGROUND_MARKING,
                     ThreadKind::kBackground);
      concurrent_marking_->RunMinor(delegate);
    }
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    return concurrent_marking_->GetMaxConcurrency(worker_count);
  }

 private:
  ConcurrentMarking* concurrent_marking_;
};

ConcurrentMarking::ConcurrentMarking(Heap* heap, WeakObjects* weak_objects)
    : heap_(heap), weak_objects_(weak_objects) {
#ifndef V8_ATOMIC_OBJECT_FIELD_WRITES
  // Concurrent marking requires atomic object field writes.
  CHECK(!v8_flags.concurrent_marking);
#endif
  int max_tasks;
  if (v8_flags.concurrent_marking_max_worker_num == 0) {
    max_tasks = V8::GetCurrentPlatform()->NumberOfWorkerThreads();
  } else {
    max_tasks = v8_flags.concurrent_marking_max_worker_num;
  }

  task_state_.reserve(max_tasks + 1);
  for (int i = 0; i <= max_tasks; ++i) {
    task_state_.emplace_back(std::make_unique<TaskState>());
  }
}

void ConcurrentMarking::RunMajor(JobDelegate* delegate,
                                 base::EnumSet<CodeFlushMode> code_flush_mode,
                                 unsigned mark_compact_epoch,
                                 bool should_keep_ages_unchanged) {
  RwxMemoryWriteScope::SetDefaultPermissionsForNewThread();
  size_t kBytesUntilInterruptCheck = 64 * KB;
  int kObjectsUntilInterruptCheck = 1000;
  uint8_t task_id = delegate->GetTaskId() + 1;
  TaskState* task_state = task_state_[task_id].get();
  auto* cpp_heap = CppHeap::From(heap_->cpp_heap());
  MarkingWorklists::Local local_marking_worklists(
      marking_worklists_, cpp_heap
                              ? cpp_heap->CreateCppMarkingState()
                              : MarkingWorklists::Local::kNoCppMarkingState);
  WeakObjects::Local local_weak_objects(weak_objects_);
  ConcurrentMarkingVisitor visitor(
      task_id, &local_marking_worklists, &local_weak_objects, heap_,
      mark_compact_epoch, code_flush_mode, heap_->cpp_heap(),
      should_keep_ages_unchanged, &task_state->memory_chunk_data);
  NativeContextInferrer& native_context_inferrer =
      task_state->native_context_inferrer;
  NativeContextStats& native_context_stats = task_state->native_context_stats;
  double time_ms;
  size_t marked_bytes = 0;
  Isolate* isolate = heap_->isolate();
  if (v8_flags.trace_concurrent_marking) {
    isolate->PrintWithTimestamp("Starting major concurrent marking task %d\n",
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
    CodePageHeaderModificationScope rwx_write_scope(
        "Marking a InstructionStream object requires write access to the "
        "Code page header");
    while (!done) {
      size_t current_marked_bytes = 0;
      int objects_processed = 0;
      while (current_marked_bytes < kBytesUntilInterruptCheck &&
             objects_processed < kObjectsUntilInterruptCheck) {
        HeapObject object;
        if (!local_marking_worklists.Pop(&object)) {
          done = true;
          break;
        }
        DCHECK(!object.InReadOnlySpace());
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
          const auto visited_size = visitor.Visit(map, object);
          visitor.marking_state()->IncrementLiveBytes(
              MemoryChunk::cast(BasicMemoryChunk::FromHeapObject(object)),
              ALIGN_TO_ALLOCATION_ALIGNMENT(visited_size));
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
                     "ConcurrentMarking::RunMajor Preempted");
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
  if (v8_flags.trace_concurrent_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "Major task %d concurrently marked %dKB in %.2fms\n", task_id,
        static_cast<int>(marked_bytes / KB), time_ms);
  }
}

void ConcurrentMarking::RunMinor(JobDelegate* delegate) {
  RwxMemoryWriteScope::SetDefaultPermissionsForNewThread();
  size_t kBytesUntilInterruptCheck = 64 * KB;
  int kObjectsUntilInterruptCheck = 1000;
  uint8_t task_id = delegate->GetTaskId() + 1;
  TaskState* task_state = task_state_[task_id].get();
  MarkingWorklists::Local local_marking_worklists(
      marking_worklists_, MarkingWorklists::Local::kNoCppMarkingState);
  YoungGenerationConcurrentMarkingVisitor visitor(
      heap_, &local_marking_worklists, &task_state->memory_chunk_data);
  double time_ms;
  size_t marked_bytes = 0;
  Isolate* isolate = heap_->isolate();
  if (v8_flags.trace_concurrent_marking) {
    isolate->PrintWithTimestamp("Starting minor concurrent marking task %d\n",
                                task_id);
  }

  {
    TimedScope scope(&time_ms);
    bool done = false;
    CodePageHeaderModificationScope rwx_write_scope(
        "Marking a InstructionStream object requires write access to the "
        "Code page header");
    while (!done) {
      size_t current_marked_bytes = 0;
      int objects_processed = 0;
      while (current_marked_bytes < kBytesUntilInterruptCheck &&
             objects_processed < kObjectsUntilInterruptCheck) {
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
          const auto visited_size = visitor.Visit(map, object);
          current_marked_bytes += visited_size;
          if (visited_size) {
            visitor.marking_state()->IncrementLiveBytes(
                MemoryChunk::cast(BasicMemoryChunk::FromHeapObject(object)),
                ALIGN_TO_ALLOCATION_ALIGNMENT(visited_size));
          }
        }
      }
      marked_bytes += current_marked_bytes;
      base::AsAtomicWord::Relaxed_Store<size_t>(&task_state->marked_bytes,
                                                marked_bytes);
      if (delegate->ShouldYield()) {
        TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                     "ConcurrentMarking::RunMinor Preempted");
        break;
      }
    }

    local_marking_worklists.Publish();
    base::AsAtomicWord::Relaxed_Store<size_t>(&task_state->marked_bytes, 0);
    total_marked_bytes_ += marked_bytes;
  }
  if (v8_flags.trace_concurrent_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "Minor task %d concurrently marked %dKB in %.2fms\n", task_id,
        static_cast<int>(marked_bytes / KB), time_ms);
  }
}

size_t ConcurrentMarking::GetMaxConcurrency(size_t worker_count) {
  size_t marking_items = marking_worklists_->shared()->Size();
  marking_items += marking_worklists_->other()->Size();
  for (auto& worklist : marking_worklists_->context_worklists())
    marking_items += worklist.worklist->Size();
  return std::min<size_t>(
      task_state_.size() - 1,
      worker_count +
          std::max<size_t>({marking_items,
                            weak_objects_->discovered_ephemerons.Size(),
                            weak_objects_->current_ephemerons.Size()}));
}

void ConcurrentMarking::ScheduleJob(GarbageCollector garbage_collector,
                                    TaskPriority priority) {
  DCHECK(v8_flags.parallel_marking || v8_flags.concurrent_marking);
  DCHECK(!heap_->IsTearingDown());
  DCHECK(IsStopped());

  garbage_collector_ = garbage_collector;
  if (garbage_collector == GarbageCollector::MARK_COMPACTOR) {
    marking_worklists_ = heap_->mark_compact_collector()->marking_worklists();
    job_handle_ = V8::GetCurrentPlatform()->PostJob(
        priority, std::make_unique<JobTaskMajor>(
                      this, heap_->mark_compact_collector()->epoch(),
                      heap_->mark_compact_collector()->code_flush_mode(),
                      heap_->ShouldCurrentGCKeepAgesUnchanged()));
  } else {
    DCHECK(garbage_collector == GarbageCollector::MINOR_MARK_COMPACTOR);
    marking_worklists_ =
        heap_->minor_mark_compact_collector()->marking_worklists();
    job_handle_ = V8::GetCurrentPlatform()->PostJob(
        priority, std::make_unique<JobTaskMinor>(this));
  }
  DCHECK(job_handle_->IsValid());
}

bool ConcurrentMarking::IsWorkLeft() {
  return !marking_worklists_->shared()->IsEmpty() ||
         !weak_objects_->current_ephemerons.IsEmpty() ||
         !weak_objects_->discovered_ephemerons.IsEmpty();
}

void ConcurrentMarking::RescheduleJobIfNeeded(
    GarbageCollector garbage_collector, TaskPriority priority) {
  DCHECK(v8_flags.parallel_marking || v8_flags.concurrent_marking);
  if (heap_->IsTearingDown()) return;

  if (IsStopped()) {
    // This DCHECK is for the case that concurrent marking was paused.
    DCHECK_IMPLIES(garbage_collector_.has_value(),
                   garbage_collector == garbage_collector_);
    ScheduleJob(garbage_collector, priority);
  } else {
    DCHECK_EQ(garbage_collector, garbage_collector_);
    if (!IsWorkLeft()) return;
    if (priority != TaskPriority::kUserVisible)
      job_handle_->UpdatePriority(priority);
    job_handle_->NotifyConcurrencyIncrease();
  }
}

void ConcurrentMarking::Join() {
  DCHECK(v8_flags.parallel_marking || v8_flags.concurrent_marking);
  if (!job_handle_ || !job_handle_->IsValid()) return;
  job_handle_->Join();
  garbage_collector_.reset();
}

bool ConcurrentMarking::Pause() {
  DCHECK(v8_flags.parallel_marking || v8_flags.concurrent_marking);
  if (!job_handle_ || !job_handle_->IsValid()) return false;

  job_handle_->Cancel();
  return true;
}

void ConcurrentMarking::Cancel() {
  Pause();
  garbage_collector_.reset();
}

bool ConcurrentMarking::IsStopped() {
  if (!v8_flags.concurrent_marking && !v8_flags.parallel_marking) return true;

  return !job_handle_ || !job_handle_->IsValid();
}

void ConcurrentMarking::Resume() {
  DCHECK(garbage_collector_.has_value());
  RescheduleJobIfNeeded(garbage_collector_.value());
}

void ConcurrentMarking::FlushNativeContexts(NativeContextStats* main_stats) {
  DCHECK(!job_handle_ || !job_handle_->IsValid());
  for (size_t i = 1; i < task_state_.size(); i++) {
    main_stats->Merge(task_state_[i]->native_context_stats);
    task_state_[i]->native_context_stats.Clear();
  }
}

void ConcurrentMarking::FlushMemoryChunkData(
    NonAtomicMarkingState* marking_state) {
  DCHECK(!job_handle_ || !job_handle_->IsValid());
  for (size_t i = 1; i < task_state_.size(); i++) {
    MemoryChunkDataMap& memory_chunk_data = task_state_[i]->memory_chunk_data;
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
    task_state_[i]->marked_bytes = 0;
  }
  total_marked_bytes_ = 0;
}

void ConcurrentMarking::ClearMemoryChunkData(MemoryChunk* chunk) {
  DCHECK(!job_handle_ || !job_handle_->IsValid());
  for (size_t i = 1; i < task_state_.size(); i++) {
    auto it = task_state_[i]->memory_chunk_data.find(chunk);
    if (it != task_state_[i]->memory_chunk_data.end()) {
      it->second.live_bytes = 0;
      it->second.typed_slots.reset();
    }
  }
}

size_t ConcurrentMarking::TotalMarkedBytes() {
  size_t result = 0;
  for (size_t i = 1; i < task_state_.size(); i++) {
    result +=
        base::AsAtomicWord::Relaxed_Load<size_t>(&task_state_[i]->marked_bytes);
  }
  result += total_marked_bytes_;
  return result;
}

ConcurrentMarking::PauseScope::PauseScope(ConcurrentMarking* concurrent_marking)
    : concurrent_marking_(concurrent_marking),
      resume_on_exit_(v8_flags.concurrent_marking &&
                      concurrent_marking_->Pause()) {
  DCHECK_IMPLIES(resume_on_exit_, v8_flags.concurrent_marking);
}

ConcurrentMarking::PauseScope::~PauseScope() {
  if (resume_on_exit_) {
    DCHECK_EQ(concurrent_marking_->garbage_collector_,
              GarbageCollector::MARK_COMPACTOR);
    concurrent_marking_->Resume();
  }
}

}  // namespace internal
}  // namespace v8

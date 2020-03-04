// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CONCURRENT_MARKING_H_
#define V8_HEAP_CONCURRENT_MARKING_H_

#include <memory>

#include "include/v8-platform.h"
#include "src/base/atomic-utils.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/heap/marking-visitor.h"
#include "src/heap/slot-set.h"
#include "src/heap/spaces.h"
#include "src/heap/worklist.h"
#include "src/init/v8.h"
#include "src/tasks/cancelable-task.h"
#include "src/utils/allocation.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;
class MajorNonAtomicMarkingState;
struct WeakObjects;

struct MemoryChunkData {
  intptr_t live_bytes;
  std::unique_ptr<TypedSlots> typed_slots;
};

using MemoryChunkDataMap =
    std::unordered_map<MemoryChunk*, MemoryChunkData, MemoryChunk::Hasher>;

class V8_EXPORT_PRIVATE ConcurrentMarking {
 public:
  // When the scope is entered, the concurrent marking tasks
  // are preempted and are not looking at the heap objects, concurrent marking
  // is resumed when the scope is exited.
  class PauseScope {
   public:
    explicit PauseScope(ConcurrentMarking* concurrent_marking);
    ~PauseScope();

   private:
    ConcurrentMarking* const concurrent_marking_;
    const bool resume_on_exit_;
  };

  enum class StopRequest {
    // Preempt ongoing tasks ASAP (and cancel unstarted tasks).
    PREEMPT_TASKS,
    // Wait for ongoing tasks to complete (and cancels unstarted tasks).
    COMPLETE_ONGOING_TASKS,
    // Wait for all scheduled tasks to complete (only use this in tests that
    // control the full stack -- otherwise tasks cancelled by the platform can
    // make this call hang).
    COMPLETE_TASKS_FOR_TESTING,
  };

  // TODO(gab): The only thing that prevents this being above 7 is
  // Worklist::kMaxNumTasks being maxed at 8 (concurrent marking doesn't use
  // task 0, reserved for the main thread).
  static constexpr int kMaxTasks = 7;

  ConcurrentMarking(Heap* heap, MarkingWorklist* marking_worklist,
                    MarkingWorklist* on_hold,
                    EmbedderTracingWorklist* embedder_worklist,
                    WeakObjects* weak_objects);

  // Schedules asynchronous tasks to perform concurrent marking. Objects in the
  // heap should not be moved while these are active (can be stopped safely via
  // Stop() or PauseScope).
  void ScheduleTasks();

  // Stops concurrent marking per |stop_request|'s semantics. Returns true
  // if concurrent marking was in progress, false otherwise.
  bool Stop(StopRequest stop_request);

  void RescheduleTasksIfNeeded();
  // Flushes memory chunk data using the given marking state.
  void FlushMemoryChunkData(MajorNonAtomicMarkingState* marking_state);
  // This function is called for a new space page that was cleared after
  // scavenge and is going to be re-used.
  void ClearMemoryChunkData(MemoryChunk* chunk);

  // Checks if all threads are stopped.
  bool IsStopped();

  size_t TotalMarkedBytes();

  void set_ephemeron_marked(bool ephemeron_marked) {
    ephemeron_marked_.store(ephemeron_marked);
  }
  bool ephemeron_marked() { return ephemeron_marked_.load(); }

 private:
  struct TaskState {
    // The main thread sets this flag to true when it wants the concurrent
    // marker to give up the worker thread.
    std::atomic<bool> preemption_request;
    MemoryChunkDataMap memory_chunk_data;
    size_t marked_bytes = 0;
    unsigned mark_compact_epoch;
    bool is_forced_gc;
    char cache_line_padding[64];
  };
  class Task;
  void Run(int task_id, TaskState* task_state);
  Heap* const heap_;
  MarkingWorklist* const marking_worklist_;
  MarkingWorklist* const on_hold_;
  EmbedderTracingWorklist* const embedder_worklist_;
  WeakObjects* const weak_objects_;
  TaskState task_state_[kMaxTasks + 1];
  std::atomic<size_t> total_marked_bytes_{0};
  std::atomic<bool> ephemeron_marked_{false};
  base::Mutex pending_lock_;
  base::ConditionVariable pending_condition_;
  int pending_task_count_ = 0;
  bool is_pending_[kMaxTasks + 1] = {};
  CancelableTaskManager::Id cancelable_id_[kMaxTasks + 1] = {};
  int total_task_count_ = 0;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CONCURRENT_MARKING_H_

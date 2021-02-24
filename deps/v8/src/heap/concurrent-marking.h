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
#include "src/heap/marking-worklist.h"
#include "src/heap/memory-measurement.h"
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
class MemoryChunk;
class WeakObjects;

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
  class V8_NODISCARD PauseScope {
   public:
    explicit PauseScope(ConcurrentMarking* concurrent_marking);
    ~PauseScope();

   private:
    ConcurrentMarking* const concurrent_marking_;
    const bool resume_on_exit_;
  };

  // TODO(gab): The only thing that prevents this being above 7 is
  // Worklist::kMaxNumTasks being maxed at 8 (concurrent marking doesn't use
  // task 0, reserved for the main thread).
  static constexpr int kMaxTasks = 7;

  ConcurrentMarking(Heap* heap, MarkingWorklists* marking_worklists,
                    WeakObjects* weak_objects);

  // Schedules asynchronous job to perform concurrent marking at |priority|.
  // Objects in the heap should not be moved while these are active (can be
  // stopped safely via Stop() or PauseScope).
  void ScheduleJob(TaskPriority priority = TaskPriority::kUserVisible);

  // Waits for scheduled job to complete.
  void Join();
  // Preempts ongoing job ASAP. Returns true if concurrent marking was in
  // progress, false otherwise.
  bool Pause();

  // Schedules asynchronous job to perform concurrent marking at |priority| if
  // not already running, otherwise adjusts the number of workers running job
  // and the priority if diffrent from the default kUserVisible.
  void RescheduleJobIfNeeded(
      TaskPriority priority = TaskPriority::kUserVisible);
  // Flushes native context sizes to the given table of the main thread.
  void FlushNativeContexts(NativeContextStats* main_stats);
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
    size_t marked_bytes = 0;
    MemoryChunkDataMap memory_chunk_data;
    NativeContextInferrer native_context_inferrer;
    NativeContextStats native_context_stats;
    char cache_line_padding[64];
  };
  class JobTask;
  void Run(JobDelegate* delegate, unsigned mark_compact_epoch,
           bool is_forced_gc);
  size_t GetMaxConcurrency(size_t worker_count);

  std::unique_ptr<JobHandle> job_handle_;
  Heap* const heap_;
  MarkingWorklists* const marking_worklists_;
  WeakObjects* const weak_objects_;
  TaskState task_state_[kMaxTasks + 1];
  std::atomic<size_t> total_marked_bytes_{0};
  std::atomic<bool> ephemeron_marked_{false};
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CONCURRENT_MARKING_H_

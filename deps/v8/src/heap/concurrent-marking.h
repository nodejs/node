// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CONCURRENT_MARKING_H_
#define V8_HEAP_CONCURRENT_MARKING_H_

#include <memory>

#include "include/v8-platform.h"
#include "src/base/atomic-utils.h"
#include "src/base/optional.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/heap/marking-visitor.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/memory-measurement.h"
#include "src/heap/slot-set.h"
#include "src/heap/spaces.h"
#include "src/init/v8.h"
#include "src/tasks/cancelable-task.h"
#include "src/utils/allocation.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;
class NonAtomicMarkingState;
class MemoryChunk;
class WeakObjects;

struct MemoryChunkData {
  intptr_t live_bytes;
  std::unique_ptr<TypedSlots> typed_slots;
};

// This class is a wrapper around an unordered_map that defines the minimum
// interface to be usable in marking. It aims to provide faster access in the
// common case where the requested element is the same as the one previously
// tried.
class MemoryChunkDataMap final {
  using MemoryChunkDataMapT =
      std::unordered_map<MemoryChunk*, MemoryChunkData, MemoryChunk::Hasher>;

 public:
  MemoryChunkDataMapT::mapped_type& operator[](
      const MemoryChunkDataMapT::key_type& key) {
    // nullptr value is used to indicate absence of a last key.
    DCHECK_NOT_NULL(key);

    if (key == last_key_) {
      return *last_mapped_;
    }

    auto it = map_.find(key);
    if (it == map_.end()) {
      auto result = map_.emplace(key, MemoryChunkData());
      DCHECK(result.second);
      it = result.first;
    }

    last_key_ = key;
    last_mapped_ = &it->second;

    return it->second;
  }

  // No iterator is cached in this class so an actual find() has to be executed
  // everytime.
  MemoryChunkDataMapT::iterator find(const MemoryChunkDataMapT::key_type& key) {
    return map_.find(key);
  }

  MemoryChunkDataMapT::iterator begin() { return map_.begin(); }
  MemoryChunkDataMapT::const_iterator end() { return map_.end(); }

  void clear() {
    last_key_ = nullptr;
    last_mapped_ = nullptr;
    map_.clear();
  }

 private:
  MemoryChunkDataMapT::key_type last_key_ = nullptr;
  MemoryChunkDataMapT::mapped_type* last_mapped_ = nullptr;
  MemoryChunkDataMapT map_;
};

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

  ConcurrentMarking(Heap* heap, WeakObjects* weak_objects);

  // Schedules asynchronous job to perform concurrent marking at |priority|.
  // Objects in the heap should not be moved while these are active (can be
  // stopped safely via Stop() or PauseScope).
  void ScheduleJob(GarbageCollector garbage_collector,
                   TaskPriority priority = TaskPriority::kUserVisible);

  // Waits for scheduled job to complete.
  void Join();
  // Preempts ongoing job ASAP. Returns true if concurrent marking was in
  // progress, false otherwise.
  bool Pause();
  void Cancel();

  // Schedules asynchronous job to perform concurrent marking at |priority| if
  // not already running, otherwise adjusts the number of workers running job
  // and the priority if different from the default kUserVisible.
  void RescheduleJobIfNeeded(
      GarbageCollector garbage_collector,
      TaskPriority priority = TaskPriority::kUserVisible);
  // Flushes native context sizes to the given table of the main thread.
  void FlushNativeContexts(NativeContextStats* main_stats);
  // Flushes memory chunk data using the given marking state.
  void FlushMemoryChunkData(NonAtomicMarkingState* marking_state);
  // This function is called for a new space page that was cleared after
  // scavenge and is going to be re-used.
  void ClearMemoryChunkData(MemoryChunk* chunk);

  // Checks if all threads are stopped.
  bool IsStopped();

  size_t TotalMarkedBytes();

  void set_another_ephemeron_iteration(bool another_ephemeron_iteration) {
    another_ephemeron_iteration_.store(another_ephemeron_iteration);
  }
  bool another_ephemeron_iteration() {
    return another_ephemeron_iteration_.load();
  }
  base::Optional<GarbageCollector> garbage_collector() const {
    return garbage_collector_;
  }

 private:
  struct TaskState {
    size_t marked_bytes = 0;
    MemoryChunkDataMap memory_chunk_data;
    NativeContextInferrer native_context_inferrer;
    NativeContextStats native_context_stats;
    char cache_line_padding[64];
  };
  class JobTaskMinor;
  class JobTaskMajor;
  void RunMinor(JobDelegate* delegate);
  void RunMajor(JobDelegate* delegate,
                base::EnumSet<CodeFlushMode> code_flush_mode,
                unsigned mark_compact_epoch, bool should_keep_ages_unchanged);
  size_t GetMaxConcurrency(size_t worker_count);
  bool IsWorkLeft();
  void Resume();

  std::unique_ptr<JobHandle> job_handle_;
  Heap* const heap_;
  base::Optional<GarbageCollector> garbage_collector_;
  MarkingWorklists* marking_worklists_;
  WeakObjects* const weak_objects_;
  std::vector<std::unique_ptr<TaskState>> task_state_;
  std::atomic<size_t> total_marked_bytes_{0};
  std::atomic<bool> another_ephemeron_iteration_{false};

  friend class Heap;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CONCURRENT_MARKING_H_

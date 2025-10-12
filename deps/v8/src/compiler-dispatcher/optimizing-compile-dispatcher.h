// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DISPATCHER_OPTIMIZING_COMPILE_DISPATCHER_H_
#define V8_COMPILER_DISPATCHER_OPTIMIZING_COMPILE_DISPATCHER_H_

#include <atomic>
#include <queue>

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/vector.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class LocalHeap;
class TurbofanCompilationJob;
class RuntimeCallStats;
class SharedFunctionInfo;

// Align the structure to the cache line size to prevent false sharing. While a
// task state is owned by a single thread, all tasks states are kept in a vector
// in OptimizingCompileDispatcher::task_states.
struct alignas(PROCESSOR_CACHE_LINE_SIZE) OptimizingCompileTaskState {
  Isolate* isolate;
  TurbofanCompilationJob* job;
};

// Circular queue of incoming recompilation tasks (including OSR).
class V8_EXPORT OptimizingCompileInputQueue {
 public:
  inline bool IsAvailable() {
    base::MutexGuard access(&mutex_);
    return queue_.size() < capacity_;
  }

  inline size_t Length() {
    base::MutexGuard access_queue(&mutex_);
    return queue_.size();
  }

  explicit OptimizingCompileInputQueue(int capacity) : capacity_(capacity) {}

  TurbofanCompilationJob* Dequeue(OptimizingCompileTaskState& task_state);
  TurbofanCompilationJob* DequeueIfIsolateMatches(
      OptimizingCompileTaskState& task_state);

  bool Enqueue(std::unique_ptr<TurbofanCompilationJob>& job);

  void FlushJobsForIsolate(Isolate* isolate);
  bool HasJobForIsolate(Isolate* isolate);

  void Prioritize(Isolate* isolate, Tagged<SharedFunctionInfo> function);

 private:
  std::deque<TurbofanCompilationJob*> queue_;
  size_t capacity_;

  base::Mutex mutex_;
  base::ConditionVariable task_finished_;

  friend class OptimizingCompileTaskExecutor;
};

// This class runs compile tasks in a thread pool. Threads grab new work from
// the input_queue_ defined in this class. Once a task is done, it will be
// enqueued into an isolate-local output queue. This class is
// not specific to a particular isolate and may be shared across multiple
// isolates in the future.
class V8_EXPORT OptimizingCompileTaskExecutor {
 public:
  OptimizingCompileTaskExecutor();
  ~OptimizingCompileTaskExecutor();

  // Creates Job with PostJob.
  void EnsureStarted();

  // Destroys the Job. Invoked after the last isolate in the IsolateGroup tears
  // down.
  void Stop();

  // Invokes and runs Turbofan for this particular job.
  void RunCompilationJob(OptimizingCompileTaskState& task_state,
                         Isolate* isolate, LocalIsolate& local_isolate,
                         TurbofanCompilationJob* job);

  // Gets the next job from the input queue.
  TurbofanCompilationJob* NextInput(OptimizingCompileTaskState& task_state);

  // Gets the next job from the input queue but only if the job is also for the
  // same isolate as in the given `OptimizingCompileTaskState`.
  TurbofanCompilationJob* NextInputIfIsolateMatches(
      OptimizingCompileTaskState& task_state);

  // Returns true when one of the currently running compilation tasks is
  // operating on the given isolate. If the return value is false, the caller
  // can also assume that no LocalHeap/LocalIsolate exists for the isolate
  // anymore as well.
  bool IsTaskRunningForIsolate(Isolate* isolate);

  // Clears the state for a task/thread once it is done with a job. This mainly
  // clears the current isolate for this task. Only invoke this after the
  // LocalHeap/LocalIsolate for this thread was destroyed as well.
  void ClearTaskState(OptimizingCompileTaskState& task_state);

  void ResetJob(OptimizingCompileTaskState& task_state);

  // Tries to append a new compilation job to the input queue. This may fail if
  // the input queue was already full.
  bool TryQueueForOptimization(std::unique_ptr<TurbofanCompilationJob>& job);

  // Waits until all running and queued compilation jobs for this isolate are
  // done.
  void WaitUntilCompilationJobsDoneForIsolate(Isolate* isolate);

  // Cancels all running compilation jobs for this isolate and then waits until
  // they stop running.
  void CancelCompilationJobsForIsolate(Isolate* isolate);

  // Returns true if there exists a currently running or queued compilation job
  // for this isolate..
  bool HasCompilationJobsForIsolate(Isolate* isolate);

 private:
  class CompileTask;

  static constexpr TaskPriority kTaskPriority = TaskPriority::kUserVisible;
  static constexpr TaskPriority kEfficiencyTaskPriority =
      TaskPriority::kBestEffort;

  OptimizingCompileInputQueue input_queue_;

  // Copy of v8_flags.concurrent_recompilation_delay that will be used from the
  // background thread.
  //
  // Since flags might get modified while the background thread is running, it
  // is not safe to access them directly.
  int recompilation_delay_;

  std::unique_ptr<JobHandle> job_handle_;

  base::OwnedVector<OptimizingCompileTaskState> task_states_;

  // Used to avoid creating the JobHandle twice.
  bool is_initialized_ = false;

  friend class OptimizingCompileDispatcher;
};

class OptimizingCompileOutputQueue {
 public:
  void Enqueue(TurbofanCompilationJob* job);
  std::unique_ptr<TurbofanCompilationJob> Dequeue();

  int InstallGeneratedBuiltins(Isolate* isolate, int installed_count);

  size_t size() const;
  bool empty() const;

 private:
  // Queue of recompilation tasks ready to be installed (excluding OSR).
  std::deque<TurbofanCompilationJob*> queue_;

  // Used for job based recompilation which has multiple producers on
  // different threads.
  base::Mutex mutex_;
};

// OptimizingCompileDispatcher is an isolate-specific class to enqueue Turbofan
// compilation jobs and retrieve results.
class V8_EXPORT_PRIVATE OptimizingCompileDispatcher {
 public:
  explicit OptimizingCompileDispatcher(
      Isolate* isolate, OptimizingCompileTaskExecutor* task_executor);

  ~OptimizingCompileDispatcher();

  // Flushes input and output queue for compilation jobs. If blocking behavior
  // is used, it will also wait until the running compilation jobs are done
  // before flushing the output queue.
  void Flush(BlockingBehavior blocking_behavior);

  // Tries to append the compilation job to the input queue. Takes ownership of
  // |job| if successful. Fails if the input queue is already full.
  bool TryQueueForOptimization(std::unique_ptr<TurbofanCompilationJob>& job);

  // Waits until all running and queued compilation jobs have finished.
  void WaitUntilCompilationJobsDone();

  void InstallOptimizedFunctions();

  // Install generated builtins in the output queue in contiguous finalization
  // order, starting with installed_count. Returns the finalization order of the
  // last job that was finalized.
  int InstallGeneratedBuiltins(int installed_count);

  // Returns true if there is space available in the input queue.
  inline bool IsQueueAvailable() { return input_queue().IsAvailable(); }

  static bool Enabled() { return v8_flags.concurrent_recompilation; }

  // This method must be called on the main thread.
  bool HasJobs();

  // Whether to finalize and thus install the optimized code.  Defaults to true.
  // Only set to false for testing (where finalization is then manually
  // requested using %FinalizeOptimization) and when compiling embedded builtins
  // concurrently. For the latter, builtins are installed manually using
  // InstallGeneratedBuiltins().
  bool finalize() const { return finalize_; }
  void set_finalize(bool finalize) {
    CHECK(!HasJobs());
    finalize_ = finalize;
  }

  void Prioritize(Tagged<SharedFunctionInfo> function);

  void StartTearDown();
  void FinishTearDown();

  void QueueFinishedJob(TurbofanCompilationJob* job);

 private:
  enum ModeFlag { COMPILE, FLUSH };

  void FlushQueues(BlockingBehavior blocking_behavior);
  void FlushInputQueue();
  void FlushOutputQueue();

  OptimizingCompileInputQueue& input_queue() {
    return task_executor_->input_queue_;
  }

  int recompilation_delay() const {
    return task_executor_->recompilation_delay_;
  }

  Isolate* isolate_;

  OptimizingCompileTaskExecutor* task_executor_;

  OptimizingCompileOutputQueue output_queue_;

  bool finalize_ = true;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_OPTIMIZING_COMPILE_DISPATCHER_H_

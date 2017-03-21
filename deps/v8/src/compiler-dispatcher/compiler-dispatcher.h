// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_H_
#define V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_H_

#include <map>
#include <memory>
#include <unordered_set>
#include <utility>

#include "src/base/atomic-utils.h"
#include "src/base/macros.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"
#include "src/globals.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace v8 {

class Platform;
enum class MemoryPressureLevel;

namespace internal {

class CancelableTaskManager;
class CompilerDispatcherJob;
class CompilerDispatcherTracer;
class Isolate;
class SharedFunctionInfo;

template <typename T>
class Handle;

// The CompilerDispatcher uses a combination of idle tasks and background tasks
// to parse and compile lazily parsed functions.
//
// As both parsing and compilation currently requires a preparation and
// finalization step that happens on the main thread, every task has to be
// advanced during idle time first. Depending on the properties of the task, it
// can then be parsed or compiled on either background threads, or during idle
// time. Last, it has to be finalized during idle time again.
//
// CompilerDispatcher::jobs_ maintains the list of all CompilerDispatcherJobs
// the CompilerDispatcher knows about.
//
// CompilerDispatcher::pending_background_jobs_ contains the set of
// CompilerDispatcherJobs that can be processed on a background thread.
//
// CompilerDispatcher::running_background_jobs_ contains the set of
// CompilerDispatcherJobs that are currently being processed on a background
// thread.
//
// CompilerDispatcher::DoIdleWork tries to advance as many jobs out of jobs_ as
// possible during idle time. If a job can't be advanced, but is suitable for
// background processing, it fires off background threads.
//
// CompilerDispatcher::DoBackgroundWork advances one of the pending jobs, and
// then spins of another idle task to potentially do the final step on the main
// thread.
class V8_EXPORT_PRIVATE CompilerDispatcher {
 public:
  enum class BlockingBehavior { kBlock, kDontBlock };

  CompilerDispatcher(Isolate* isolate, Platform* platform,
                     size_t max_stack_size);
  ~CompilerDispatcher();

  // Returns true if a job was enqueued.
  bool Enqueue(Handle<SharedFunctionInfo> function);

  // Like Enqueue, but also advances the job so that it can potentially
  // continue running on a background thread (if at all possible). Returns
  // true if the job was enqueued.
  bool EnqueueAndStep(Handle<SharedFunctionInfo> function);

  // Returns true if there is a pending job for the given function.
  bool IsEnqueued(Handle<SharedFunctionInfo> function) const;

  // Blocks until the given function is compiled (and does so as fast as
  // possible). Returns true if the compile job was succesful.
  bool FinishNow(Handle<SharedFunctionInfo> function);

  // Aborts a given job. Blocks if requested.
  void Abort(Handle<SharedFunctionInfo> function, BlockingBehavior blocking);

  // Aborts all jobs. Blocks if requested.
  void AbortAll(BlockingBehavior blocking);

  // Memory pressure notifications from the embedder.
  void MemoryPressureNotification(v8::MemoryPressureLevel level,
                                  bool is_isolate_locked);

 private:
  FRIEND_TEST(CompilerDispatcherTest, EnqueueAndStep);
  FRIEND_TEST(CompilerDispatcherTest, IdleTaskSmallIdleTime);
  FRIEND_TEST(CompilerDispatcherTest, CompileOnBackgroundThread);
  FRIEND_TEST(CompilerDispatcherTest, FinishNowWithBackgroundTask);
  FRIEND_TEST(CompilerDispatcherTest, AsyncAbortAllPendingBackgroundTask);
  FRIEND_TEST(CompilerDispatcherTest, AsyncAbortAllRunningBackgroundTask);
  FRIEND_TEST(CompilerDispatcherTest, FinishNowDuringAbortAll);

  typedef std::multimap<std::pair<int, int>,
                        std::unique_ptr<CompilerDispatcherJob>>
      JobMap;
  class AbortTask;
  class BackgroundTask;
  class IdleTask;

  void WaitForJobIfRunningOnBackground(CompilerDispatcherJob* job);
  bool IsEnabled() const;
  void AbortInactiveJobs();
  JobMap::const_iterator GetJobFor(Handle<SharedFunctionInfo> shared) const;
  void ConsiderJobForBackgroundProcessing(CompilerDispatcherJob* job);
  void ScheduleMoreBackgroundTasksIfNeeded();
  void ScheduleIdleTaskFromAnyThread();
  void ScheduleIdleTaskIfNeeded();
  void ScheduleAbortTask();
  void DoBackgroundWork();
  void DoIdleWork(double deadline_in_seconds);

  Isolate* isolate_;
  Platform* platform_;
  size_t max_stack_size_;

  // Copy of FLAG_trace_compiler_dispatcher to allow for access from any thread.
  bool trace_compiler_dispatcher_;

  std::unique_ptr<CompilerDispatcherTracer> tracer_;

  std::unique_ptr<CancelableTaskManager> task_manager_;

  // Mapping from (script id, function literal id) to job. We use a multimap,
  // as script id is not necessarily unique.
  JobMap jobs_;

  base::AtomicValue<v8::MemoryPressureLevel> memory_pressure_level_;

  // The following members can be accessed from any thread. Methods need to hold
  // the mutex |mutex_| while accessing them.
  base::Mutex mutex_;

  // True if the dispatcher is in the process of aborting running tasks.
  bool abort_;

  bool idle_task_scheduled_;

  // Number of currently scheduled BackgroundTask objects.
  size_t num_scheduled_background_tasks_;

  // The set of CompilerDispatcherJobs that can be advanced on any thread.
  std::unordered_set<CompilerDispatcherJob*> pending_background_jobs_;

  // The set of CompilerDispatcherJobs currently processed on background
  // threads.
  std::unordered_set<CompilerDispatcherJob*> running_background_jobs_;

  // If not nullptr, then the main thread waits for the task processing
  // this job, and blocks on the ConditionVariable main_thread_blocking_signal_.
  CompilerDispatcherJob* main_thread_blocking_on_job_;
  base::ConditionVariable main_thread_blocking_signal_;

  // Test support.
  base::AtomicValue<bool> block_for_testing_;
  base::Semaphore semaphore_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(CompilerDispatcher);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_H_

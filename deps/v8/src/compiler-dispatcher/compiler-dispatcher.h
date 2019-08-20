// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_H_
#define V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <unordered_set>
#include <utility>

#include "src/base/atomic-utils.h"
#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"
#include "src/common/globals.h"
#include "src/handles/maybe-handles.h"
#include "src/utils/identity-map.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {

class Platform;
enum class MemoryPressureLevel;

namespace internal {

class AstRawString;
class AstValueFactory;
class BackgroundCompileTask;
class CancelableTaskManager;
class UnoptimizedCompileJob;
class CompilerDispatcherTracer;
class DeferredHandles;
class FunctionLiteral;
class Isolate;
class ParseInfo;
class SharedFunctionInfo;
class TimedHistogram;
class WorkerThreadRuntimeCallStats;
class Zone;

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
  using JobId = uintptr_t;

  CompilerDispatcher(Isolate* isolate, Platform* platform,
                     size_t max_stack_size);
  ~CompilerDispatcher();

  // Returns true if the compiler dispatcher is enabled.
  bool IsEnabled() const;

  base::Optional<JobId> Enqueue(const ParseInfo* outer_parse_info,
                                const AstRawString* function_name,
                                const FunctionLiteral* function_literal);

  // Registers the given |function| with the compilation job |job_id|.
  void RegisterSharedFunctionInfo(JobId job_id, SharedFunctionInfo function);

  // Returns true if there is a pending job with the given id.
  bool IsEnqueued(JobId job_id) const;

  // Returns true if there is a pending job registered for the given function.
  bool IsEnqueued(Handle<SharedFunctionInfo> function) const;

  // Blocks until the given function is compiled (and does so as fast as
  // possible). Returns true if the compile job was successful.
  bool FinishNow(Handle<SharedFunctionInfo> function);

  // Aborts compilation job |job_id|.
  void AbortJob(JobId job_id);

  // Aborts all jobs, blocking until all jobs are aborted.
  void AbortAll();

 private:
  FRIEND_TEST(CompilerDispatcherTest, IdleTaskNoIdleTime);
  FRIEND_TEST(CompilerDispatcherTest, IdleTaskSmallIdleTime);
  FRIEND_TEST(CompilerDispatcherTest, FinishNowWithWorkerTask);
  FRIEND_TEST(CompilerDispatcherTest, AbortJobNotStarted);
  FRIEND_TEST(CompilerDispatcherTest, AbortJobAlreadyStarted);
  FRIEND_TEST(CompilerDispatcherTest, AsyncAbortAllPendingWorkerTask);
  FRIEND_TEST(CompilerDispatcherTest, AsyncAbortAllRunningWorkerTask);
  FRIEND_TEST(CompilerDispatcherTest, CompileMultipleOnBackgroundThread);

  struct Job {
    explicit Job(BackgroundCompileTask* task_arg);
    ~Job();

    bool IsReadyToFinalize(const base::MutexGuard&) {
      return has_run && (!function.is_null() || aborted);
    }

    bool IsReadyToFinalize(base::Mutex* mutex) {
      base::MutexGuard lock(mutex);
      return IsReadyToFinalize(lock);
    }

    std::unique_ptr<BackgroundCompileTask> task;
    MaybeHandle<SharedFunctionInfo> function;
    bool has_run;
    bool aborted;
  };

  using JobMap = std::map<JobId, std::unique_ptr<Job>>;
  using SharedToJobIdMap = IdentityMap<JobId, FreeStoreAllocationPolicy>;

  void WaitForJobIfRunningOnBackground(Job* job);
  JobMap::const_iterator GetJobFor(Handle<SharedFunctionInfo> shared) const;
  void ScheduleMoreWorkerTasksIfNeeded();
  void ScheduleIdleTaskFromAnyThread(const base::MutexGuard&);
  void DoBackgroundWork();
  void DoIdleWork(double deadline_in_seconds);
  // Returns iterator to the inserted job.
  JobMap::const_iterator InsertJob(std::unique_ptr<Job> job);
  // Returns iterator following the removed job.
  JobMap::const_iterator RemoveJob(JobMap::const_iterator job);

  Isolate* isolate_;
  AccountingAllocator* allocator_;
  WorkerThreadRuntimeCallStats* worker_thread_runtime_call_stats_;
  TimedHistogram* background_compile_timer_;
  std::shared_ptr<v8::TaskRunner> taskrunner_;
  Platform* platform_;
  size_t max_stack_size_;

  // Copy of FLAG_trace_compiler_dispatcher to allow for access from any thread.
  bool trace_compiler_dispatcher_;

  std::unique_ptr<CancelableTaskManager> task_manager_;

  // Id for next job to be added
  JobId next_job_id_;

  // Mapping from job_id to job.
  JobMap jobs_;

  // Mapping from SharedFunctionInfo to the corresponding unoptimized
  // compilation's JobId;
  SharedToJobIdMap shared_to_unoptimized_job_id_;

  // The following members can be accessed from any thread. Methods need to hold
  // the mutex |mutex_| while accessing them.
  base::Mutex mutex_;

  // True if an idle task is scheduled to be run.
  bool idle_task_scheduled_;

  // Number of scheduled or running WorkerTask objects.
  int num_worker_tasks_;

  // The set of jobs that can be run on a background thread.
  std::unordered_set<Job*> pending_background_jobs_;

  // The set of jobs currently being run on background threads.
  std::unordered_set<Job*> running_background_jobs_;

  // If not nullptr, then the main thread waits for the task processing
  // this job, and blocks on the ConditionVariable main_thread_blocking_signal_.
  Job* main_thread_blocking_on_job_;
  base::ConditionVariable main_thread_blocking_signal_;

  // Test support.
  base::AtomicValue<bool> block_for_testing_;
  base::Semaphore semaphore_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(CompilerDispatcher);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_H_

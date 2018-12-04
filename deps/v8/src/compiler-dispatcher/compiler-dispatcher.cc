// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/compiler-dispatcher.h"

#include "src/ast/ast.h"
#include "src/base/platform/time.h"
#include "src/base/template-utils.h"
#include "src/cancelable-task.h"
#include "src/compiler-dispatcher/compiler-dispatcher-job.h"
#include "src/compiler-dispatcher/compiler-dispatcher-tracer.h"
#include "src/compiler-dispatcher/unoptimized-compile-job.h"
#include "src/flags.h"
#include "src/global-handles.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

namespace {

enum class ExceptionHandling { kSwallow, kThrow };

void FinalizeJobOnMainThread(Isolate* isolate, CompilerDispatcherJob* job,
                             Handle<SharedFunctionInfo> shared,
                             ExceptionHandling exception_handling) {
  DCHECK(ThreadId::Current().Equals(isolate->thread_id()));
  DCHECK_EQ(job->status(), CompilerDispatcherJob::Status::kReadyToFinalize);

  job->FinalizeOnMainThread(isolate, shared);
  DCHECK_EQ(job->IsFailed(), isolate->has_pending_exception());
  if (job->IsFailed() && exception_handling == ExceptionHandling::kSwallow) {
    isolate->clear_pending_exception();
  }
}

// Theoretically we get 50ms of idle time max, however it's unlikely that
// we'll get all of it so try to be a conservative.
const double kMaxIdleTimeToExpectInMs = 40;

}  // namespace

CompilerDispatcher::CompilerDispatcher(Isolate* isolate, Platform* platform,
                                       size_t max_stack_size)
    : isolate_(isolate),
      allocator_(isolate->allocator()),
      worker_thread_runtime_call_stats_(
          isolate->counters()->worker_thread_runtime_call_stats()),
      background_compile_timer_(
          isolate->counters()->compile_function_on_background()),
      taskrunner_(platform->GetForegroundTaskRunner(
          reinterpret_cast<v8::Isolate*>(isolate))),
      platform_(platform),
      max_stack_size_(max_stack_size),
      trace_compiler_dispatcher_(FLAG_trace_compiler_dispatcher),
      tracer_(new CompilerDispatcherTracer(isolate_)),
      task_manager_(new CancelableTaskManager()),
      next_job_id_(0),
      shared_to_unoptimized_job_id_(isolate->heap()),
      memory_pressure_level_(MemoryPressureLevel::kNone),
      abort_(false),
      idle_task_scheduled_(false),
      num_worker_tasks_(0),
      main_thread_blocking_on_job_(nullptr),
      block_for_testing_(false),
      semaphore_for_testing_(0) {
  if (trace_compiler_dispatcher_ && !IsEnabled()) {
    PrintF("CompilerDispatcher: dispatcher is disabled\n");
  }
}

CompilerDispatcher::~CompilerDispatcher() {
  // To avoid crashing in unit tests due to unfished jobs.
  AbortAll(BlockingBehavior::kBlock);
  task_manager_->CancelAndWait();
}

bool CompilerDispatcher::CanEnqueue() {
  if (!IsEnabled()) return false;

  // TODO(rmcilroy): Investigate if MemoryPressureLevel::kNone is ever sent on
  // Android, if not, remove this check.
  if (memory_pressure_level_.Value() != MemoryPressureLevel::kNone) {
    return false;
  }

  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    if (abort_) return false;
  }

  return true;
}

base::Optional<CompilerDispatcher::JobId> CompilerDispatcher::Enqueue(
    const ParseInfo* outer_parse_info, const AstRawString* function_name,
    const FunctionLiteral* function_literal) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompilerDispatcherEnqueue");
  RuntimeCallTimerScope runtimeTimer(
      isolate_, RuntimeCallCounterId::kCompileEnqueueOnDispatcher);

  if (!CanEnqueue()) return base::nullopt;

  std::unique_ptr<CompilerDispatcherJob> job(new UnoptimizedCompileJob(
      tracer_.get(), allocator_, outer_parse_info, function_name,
      function_literal, worker_thread_runtime_call_stats_,
      background_compile_timer_, max_stack_size_));
  JobMap::const_iterator it = InsertJob(std::move(job));
  JobId id = it->first;
  if (trace_compiler_dispatcher_) {
    PrintF("CompilerDispatcher: enqueued job %zu for function literal id %d\n",
           id, function_literal->function_literal_id());
  }

  // Post a idle task and a background worker task to perform the compilation
  // either on the worker thread or during idle time (whichever is first).
  ConsiderJobForBackgroundProcessing(it->second.get());
  ScheduleIdleTaskIfNeeded();
  return base::make_optional(id);
}

bool CompilerDispatcher::IsEnabled() const { return FLAG_compiler_dispatcher; }

bool CompilerDispatcher::IsEnqueued(Handle<SharedFunctionInfo> function) const {
  if (jobs_.empty()) return false;
  return GetJobFor(function) != jobs_.end();
}

bool CompilerDispatcher::IsEnqueued(JobId job_id) const {
  return jobs_.find(job_id) != jobs_.end();
}
void CompilerDispatcher::RegisterSharedFunctionInfo(
    JobId job_id, SharedFunctionInfo* function) {
  DCHECK_NE(jobs_.find(job_id), jobs_.end());
  DCHECK_EQ(job_id_to_shared_.find(job_id), job_id_to_shared_.end());

  if (trace_compiler_dispatcher_) {
    PrintF("CompilerDispatcher: registering ");
    function->ShortPrint();
    PrintF(" with job id %zu\n", job_id);
  }

  // Make a global handle to the function.
  Handle<SharedFunctionInfo> function_handle =
      isolate_->global_handles()->Create(function);

  // Register mapping.
  job_id_to_shared_.insert(std::make_pair(job_id, function_handle));
  shared_to_unoptimized_job_id_.Set(function_handle, job_id);

  // Schedule an idle task to finalize job if it is ready.
  ScheduleIdleTaskIfNeeded();
}

void CompilerDispatcher::WaitForJobIfRunningOnBackground(
    CompilerDispatcherJob* job) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompilerDispatcherWaitForBackgroundJob");
  RuntimeCallTimerScope runtimeTimer(
      isolate_, RuntimeCallCounterId::kCompileWaitForDispatcher);

  base::LockGuard<base::Mutex> lock(&mutex_);
  if (running_background_jobs_.find(job) == running_background_jobs_.end()) {
    pending_background_jobs_.erase(job);
    return;
  }
  DCHECK_NULL(main_thread_blocking_on_job_);
  main_thread_blocking_on_job_ = job;
  while (main_thread_blocking_on_job_ != nullptr) {
    main_thread_blocking_signal_.Wait(&mutex_);
  }
  DCHECK(pending_background_jobs_.find(job) == pending_background_jobs_.end());
  DCHECK(running_background_jobs_.find(job) == running_background_jobs_.end());
}

bool CompilerDispatcher::FinishNow(Handle<SharedFunctionInfo> function) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompilerDispatcherFinishNow");
  RuntimeCallTimerScope runtimeTimer(
      isolate_, RuntimeCallCounterId::kCompileFinishNowOnDispatcher);
  if (trace_compiler_dispatcher_) {
    PrintF("CompilerDispatcher: finishing ");
    function->ShortPrint();
    PrintF(" now\n");
  }

  JobMap::const_iterator it = GetJobFor(function);
  CHECK(it != jobs_.end());
  CompilerDispatcherJob* job = it->second.get();
  WaitForJobIfRunningOnBackground(job);
  while (!job->IsFinished()) {
    switch (job->status()) {
      case CompilerDispatcherJob::Status::kInitial:
        job->Compile(false);
        break;
      case CompilerDispatcherJob::Status::kReadyToFinalize: {
        FinalizeJobOnMainThread(isolate_, job, function,
                                ExceptionHandling::kThrow);
        break;
      }
      case CompilerDispatcherJob::Status::kFailed:
      case CompilerDispatcherJob::Status::kDone:
        UNREACHABLE();
    }
  }
  DCHECK_EQ(job->IsFailed(), isolate_->has_pending_exception());
  DCHECK(job->IsFinished());
  bool result = !job->IsFailed();
  RemoveJob(it);
  return result;
}

void CompilerDispatcher::AbortAll(BlockingBehavior blocking) {
  bool background_tasks_running =
      task_manager_->TryAbortAll() == CancelableTaskManager::kTaskRunning;
  if (!background_tasks_running || blocking == BlockingBehavior::kBlock) {
    for (auto& it : jobs_) {
      WaitForJobIfRunningOnBackground(it.second.get());
      if (trace_compiler_dispatcher_) {
        PrintF("CompilerDispatcher: aborted job %zu\n", it.first);
      }
      it.second->ResetOnMainThread(isolate_);
    }
    jobs_.clear();
    shared_to_unoptimized_job_id_.Clear();
    {
      base::LockGuard<base::Mutex> lock(&mutex_);
      DCHECK(pending_background_jobs_.empty());
      DCHECK(running_background_jobs_.empty());
      abort_ = false;
    }
    return;
  }

  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    abort_ = true;
    pending_background_jobs_.clear();
    idle_task_scheduled_ = false;  // Idle task cancelled by TryAbortAll.
  }
  AbortInactiveJobs();

  // All running background jobs might already have scheduled idle tasks instead
  // of abort tasks. Schedule a single abort task here to make sure they get
  // processed as soon as possible (and not first when we have idle time).
  ScheduleAbortTask();
}

void CompilerDispatcher::AbortInactiveJobs() {
  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    // Since we schedule two abort tasks per async abort, we might end up
    // here with nothing left to do.
    if (!abort_) return;
  }
  for (auto it = jobs_.cbegin(); it != jobs_.cend();) {
    auto job = it;
    ++it;
    {
      base::LockGuard<base::Mutex> lock(&mutex_);
      if (running_background_jobs_.find(job->second.get()) !=
          running_background_jobs_.end()) {
        continue;
      }
    }
    if (trace_compiler_dispatcher_) {
      PrintF("CompilerDispatcher: aborted job %zu\n", job->first);
    }
    it = RemoveJob(job);
  }
  if (jobs_.empty()) {
    base::LockGuard<base::Mutex> lock(&mutex_);
    if (num_worker_tasks_ == 0) abort_ = false;
  }
}

void CompilerDispatcher::MemoryPressureNotification(
    v8::MemoryPressureLevel level, bool is_isolate_locked) {
  MemoryPressureLevel previous = memory_pressure_level_.Value();
  memory_pressure_level_.SetValue(level);
  // If we're already under pressure, we haven't accepted new tasks meanwhile
  // and can just return. If we're no longer under pressure, we're also done.
  if (previous != MemoryPressureLevel::kNone ||
      level == MemoryPressureLevel::kNone) {
    return;
  }
  if (trace_compiler_dispatcher_) {
    PrintF("CompilerDispatcher: received memory pressure notification\n");
  }
  if (is_isolate_locked) {
    AbortAll(BlockingBehavior::kDontBlock);
  } else {
    {
      base::LockGuard<base::Mutex> lock(&mutex_);
      if (abort_) return;
      // By going into abort mode here, and clearing the
      // pending_background_jobs_, we at keep existing background jobs from
      // picking up more work before the MemoryPressureTask gets executed.
      abort_ = true;
      pending_background_jobs_.clear();
    }
    taskrunner_->PostTask(MakeCancelableLambdaTask(task_manager_.get(), [this] {
      AbortAll(BlockingBehavior::kDontBlock);
    }));
  }
}

CompilerDispatcher::JobMap::const_iterator CompilerDispatcher::GetJobFor(
    Handle<SharedFunctionInfo> shared) const {
  JobId* job_id_ptr = shared_to_unoptimized_job_id_.Find(shared);
  JobMap::const_iterator job = jobs_.end();
  if (job_id_ptr) {
    job = jobs_.find(*job_id_ptr);
  }
  return job;
}

void CompilerDispatcher::ScheduleIdleTaskFromAnyThread() {
  if (!taskrunner_->IdleTasksEnabled()) return;
  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    if (idle_task_scheduled_ || abort_) return;
    idle_task_scheduled_ = true;
  }
  taskrunner_->PostIdleTask(MakeCancelableIdleLambdaTask(
      task_manager_.get(),
      [this](double deadline_in_seconds) { DoIdleWork(deadline_in_seconds); }));
}

void CompilerDispatcher::ScheduleIdleTaskIfNeeded() {
  if (jobs_.empty()) return;
  ScheduleIdleTaskFromAnyThread();
}

void CompilerDispatcher::ScheduleAbortTask() {
  taskrunner_->PostTask(MakeCancelableLambdaTask(
      task_manager_.get(), [this] { AbortInactiveJobs(); }));
}

void CompilerDispatcher::ConsiderJobForBackgroundProcessing(
    CompilerDispatcherJob* job) {
  if (!job->NextStepCanRunOnAnyThread()) return;
  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    pending_background_jobs_.insert(job);
  }
  ScheduleMoreWorkerTasksIfNeeded();
}

void CompilerDispatcher::ScheduleMoreWorkerTasksIfNeeded() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompilerDispatcherScheduleMoreWorkerTasksIfNeeded");
  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    if (pending_background_jobs_.empty()) return;
    if (platform_->NumberOfWorkerThreads() <= num_worker_tasks_) {
      return;
    }
    ++num_worker_tasks_;
  }
  platform_->CallOnWorkerThread(MakeCancelableLambdaTask(
      task_manager_.get(), [this] { DoBackgroundWork(); }));
}

void CompilerDispatcher::DoBackgroundWork() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompilerDispatcherDoBackgroundWork");
  for (;;) {
    CompilerDispatcherJob* job = nullptr;
    {
      base::LockGuard<base::Mutex> lock(&mutex_);
      if (!pending_background_jobs_.empty()) {
        auto it = pending_background_jobs_.begin();
        job = *it;
        pending_background_jobs_.erase(it);
        running_background_jobs_.insert(job);
      }
    }
    if (job == nullptr) break;

    if (V8_UNLIKELY(block_for_testing_.Value())) {
      block_for_testing_.SetValue(false);
      semaphore_for_testing_.Wait();
    }

    if (trace_compiler_dispatcher_) {
      PrintF("CompilerDispatcher: doing background work\n");
    }

    DCHECK(job->NextStepCanRunOnAnyThread());
    DCHECK_EQ(job->status(), CompilerDispatcherJob::Status::kInitial);
    job->Compile(true);

    // Unconditionally schedule an idle task, as all background steps have to be
    // followed by a main thread step.
    ScheduleIdleTaskFromAnyThread();

    {
      base::LockGuard<base::Mutex> lock(&mutex_);
      running_background_jobs_.erase(job);

      if (main_thread_blocking_on_job_ == job) {
        main_thread_blocking_on_job_ = nullptr;
        main_thread_blocking_signal_.NotifyOne();
      }
    }
  }

  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    --num_worker_tasks_;

    if (running_background_jobs_.empty() && abort_) {
      // This is the last background job that finished. The abort task
      // scheduled by AbortAll might already have ran, so schedule another
      // one to be on the safe side.
      ScheduleAbortTask();
    }
  }
  // Don't touch |this| anymore after this point, as it might have been
  // deleted.
}

void CompilerDispatcher::DoIdleWork(double deadline_in_seconds) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompilerDispatcherDoIdleWork");
  bool aborted = false;
  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    idle_task_scheduled_ = false;
    aborted = abort_;
  }

  if (aborted) {
    AbortInactiveJobs();
    return;
  }

  // Number of jobs that are unlikely to make progress during any idle callback
  // due to their estimated duration.
  size_t jobs_unlikely_to_progress = 0;

  // Iterate over all available jobs & remaining time. For each job, decide
  // whether to 1) skip it (if it would take too long), 2) erase it (if it's
  // finished), or 3) make progress on it if possible.
  double idle_time_in_seconds =
      deadline_in_seconds - platform_->MonotonicallyIncreasingTime();

  if (trace_compiler_dispatcher_) {
    PrintF("CompilerDispatcher: received %0.1lfms of idle time\n",
           idle_time_in_seconds *
               static_cast<double>(base::Time::kMillisecondsPerSecond));
  }
  for (auto job = jobs_.cbegin();
       job != jobs_.cend() && idle_time_in_seconds > 0.0;
       idle_time_in_seconds =
           deadline_in_seconds - platform_->MonotonicallyIncreasingTime()) {
    // Don't work on jobs that are being worked on by background tasks.
    // Similarly, remove jobs we work on from the set of available background
    // jobs.
    std::unique_ptr<base::LockGuard<base::Mutex>> lock(
        new base::LockGuard<base::Mutex>(&mutex_));
    if (running_background_jobs_.find(job->second.get()) !=
        running_background_jobs_.end()) {
      ++job;
      continue;
    }
    DCHECK(!job->second->IsFinished());
    auto it = pending_background_jobs_.find(job->second.get());
    double estimate_in_ms = job->second->EstimateRuntimeOfNextStepInMs();
    if (idle_time_in_seconds <
        (estimate_in_ms /
         static_cast<double>(base::Time::kMillisecondsPerSecond))) {
      // If there's not enough time left, try to estimate whether we would
      // have managed to finish the job in a large idle task to assess
      // whether we should ask for another idle callback.
      // TODO(rmcilroy): Consider running the job anyway when we have a long
      // idle time since this would probably be the best time to run.
      if (estimate_in_ms > kMaxIdleTimeToExpectInMs)
        ++jobs_unlikely_to_progress;
      if (it == pending_background_jobs_.end()) {
        lock.reset();
        ConsiderJobForBackgroundProcessing(job->second.get());
      }
      ++job;
    } else if (job->second->status() ==
               CompilerDispatcherJob::Status::kInitial) {
      if (it != pending_background_jobs_.end()) {
        pending_background_jobs_.erase(it);
      }
      lock.reset();
      job->second->Compile(false);
      // Don't update job so we can immediately finalize it on the next loop.
    } else {
      DCHECK_EQ(job->second->status(),
                CompilerDispatcherJob::Status::kReadyToFinalize);
      DCHECK(it == pending_background_jobs_.end());
      lock.reset();

      auto shared_it = job_id_to_shared_.find(job->first);
      if (shared_it != job_id_to_shared_.end()) {
        Handle<SharedFunctionInfo> shared = shared_it->second;
        FinalizeJobOnMainThread(isolate_, job->second.get(), shared,
                                ExceptionHandling::kSwallow);
        DCHECK(job->second->IsFinished());
        job = RemoveJob(job);
      } else {
        // If we can't step the job yet, go to the next job.
        ++jobs_unlikely_to_progress;
        ++job;
      }
    }
  }
  if (jobs_.size() > jobs_unlikely_to_progress) ScheduleIdleTaskIfNeeded();
}

CompilerDispatcher::JobMap::const_iterator CompilerDispatcher::RemoveIfFinished(
    JobMap::const_iterator job) {
  if (!job->second->IsFinished()) {
    return job;
  }

  if (trace_compiler_dispatcher_) {
    bool result = !job->second->IsFailed();
    PrintF("CompilerDispatcher: finished working on job %zu: %s\n", job->first,
           result ? "success" : "failure");
    tracer_->DumpStatistics();
  }

  return RemoveJob(job);
}

CompilerDispatcher::JobMap::const_iterator CompilerDispatcher::InsertJob(
    std::unique_ptr<CompilerDispatcherJob> job) {
  bool added;
  JobMap::const_iterator it;
  std::tie(it, added) =
      jobs_.insert(std::make_pair(next_job_id_++, std::move(job)));
  DCHECK(added);
  return it;
}

CompilerDispatcher::JobMap::const_iterator CompilerDispatcher::RemoveJob(
    CompilerDispatcher::JobMap::const_iterator it) {
  CompilerDispatcherJob* job = it->second.get();

  // Delete SFI associated with job if its been registered.
  auto shared_it = job_id_to_shared_.find(it->first);
  if (shared_it != job_id_to_shared_.end()) {
    Handle<SharedFunctionInfo> shared = shared_it->second;

    JobId deleted_id;
    shared_to_unoptimized_job_id_.Delete(shared, &deleted_id);
    DCHECK_EQ(it->first, deleted_id);

    job_id_to_shared_.erase(shared_it);
    GlobalHandles::Destroy(Handle<Object>::cast(shared).location());
  }

  job->ResetOnMainThread(isolate_);

  it = jobs_.erase(it);
  if (jobs_.empty()) {
    base::LockGuard<base::Mutex> lock(&mutex_);
    if (num_worker_tasks_ == 0) abort_ = false;
  }

  return it;
}

}  // namespace internal
}  // namespace v8

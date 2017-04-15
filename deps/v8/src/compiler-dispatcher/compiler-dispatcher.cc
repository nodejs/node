// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/compiler-dispatcher.h"

#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/base/platform/time.h"
#include "src/cancelable-task.h"
#include "src/compiler-dispatcher/compiler-dispatcher-job.h"
#include "src/compiler-dispatcher/compiler-dispatcher-tracer.h"
#include "src/flags.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

namespace {

enum class ExceptionHandling { kSwallow, kThrow };

bool DoNextStepOnMainThread(Isolate* isolate, CompilerDispatcherJob* job,
                            ExceptionHandling exception_handling) {
  DCHECK(ThreadId::Current().Equals(isolate->thread_id()));
  switch (job->status()) {
    case CompileJobStatus::kInitial:
      job->PrepareToParseOnMainThread();
      break;

    case CompileJobStatus::kReadyToParse:
      job->Parse();
      break;

    case CompileJobStatus::kParsed:
      job->FinalizeParsingOnMainThread();
      break;

    case CompileJobStatus::kReadyToAnalyse:
      job->PrepareToCompileOnMainThread();
      break;

    case CompileJobStatus::kReadyToCompile:
      job->Compile();
      break;

    case CompileJobStatus::kCompiled:
      job->FinalizeCompilingOnMainThread();
      break;

    case CompileJobStatus::kFailed:
    case CompileJobStatus::kDone:
      break;
  }

  DCHECK_EQ(job->status() == CompileJobStatus::kFailed,
            isolate->has_pending_exception());
  if (job->status() == CompileJobStatus::kFailed &&
      exception_handling == ExceptionHandling::kSwallow) {
    isolate->clear_pending_exception();
  }
  return job->status() != CompileJobStatus::kFailed;
}

bool IsFinished(CompilerDispatcherJob* job) {
  return job->status() == CompileJobStatus::kDone ||
         job->status() == CompileJobStatus::kFailed;
}

bool CanRunOnAnyThread(CompilerDispatcherJob* job) {
  return job->status() == CompileJobStatus::kReadyToParse ||
         job->status() == CompileJobStatus::kReadyToCompile;
}

void DoNextStepOnBackgroundThread(CompilerDispatcherJob* job) {
  DCHECK(CanRunOnAnyThread(job));
  switch (job->status()) {
    case CompileJobStatus::kReadyToParse:
      job->Parse();
      break;

    case CompileJobStatus::kReadyToCompile:
      job->Compile();
      break;

    default:
      UNREACHABLE();
  }
}

// Theoretically we get 50ms of idle time max, however it's unlikely that
// we'll get all of it so try to be a conservative.
const double kMaxIdleTimeToExpectInMs = 40;

class MemoryPressureTask : public CancelableTask {
 public:
  MemoryPressureTask(Isolate* isolate, CancelableTaskManager* task_manager,
                     CompilerDispatcher* dispatcher);
  ~MemoryPressureTask() override;

  // CancelableTask implementation.
  void RunInternal() override;

 private:
  CompilerDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureTask);
};

MemoryPressureTask::MemoryPressureTask(Isolate* isolate,
                                       CancelableTaskManager* task_manager,
                                       CompilerDispatcher* dispatcher)
    : CancelableTask(isolate, task_manager), dispatcher_(dispatcher) {}

MemoryPressureTask::~MemoryPressureTask() {}

void MemoryPressureTask::RunInternal() {
  dispatcher_->AbortAll(CompilerDispatcher::BlockingBehavior::kDontBlock);
}

}  // namespace

class CompilerDispatcher::AbortTask : public CancelableTask {
 public:
  AbortTask(Isolate* isolate, CancelableTaskManager* task_manager,
            CompilerDispatcher* dispatcher);
  ~AbortTask() override;

  // CancelableTask implementation.
  void RunInternal() override;

 private:
  CompilerDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(AbortTask);
};

CompilerDispatcher::AbortTask::AbortTask(Isolate* isolate,
                                         CancelableTaskManager* task_manager,
                                         CompilerDispatcher* dispatcher)
    : CancelableTask(isolate, task_manager), dispatcher_(dispatcher) {}

CompilerDispatcher::AbortTask::~AbortTask() {}

void CompilerDispatcher::AbortTask::RunInternal() {
  dispatcher_->AbortInactiveJobs();
}

class CompilerDispatcher::BackgroundTask : public CancelableTask {
 public:
  BackgroundTask(Isolate* isolate, CancelableTaskManager* task_manager,
                 CompilerDispatcher* dispatcher);
  ~BackgroundTask() override;

  // CancelableTask implementation.
  void RunInternal() override;

 private:
  CompilerDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTask);
};

CompilerDispatcher::BackgroundTask::BackgroundTask(
    Isolate* isolate, CancelableTaskManager* task_manager,
    CompilerDispatcher* dispatcher)
    : CancelableTask(isolate, task_manager), dispatcher_(dispatcher) {}

CompilerDispatcher::BackgroundTask::~BackgroundTask() {}

void CompilerDispatcher::BackgroundTask::RunInternal() {
  dispatcher_->DoBackgroundWork();
}

class CompilerDispatcher::IdleTask : public CancelableIdleTask {
 public:
  IdleTask(Isolate* isolate, CancelableTaskManager* task_manager,
           CompilerDispatcher* dispatcher);
  ~IdleTask() override;

  // CancelableIdleTask implementation.
  void RunInternal(double deadline_in_seconds) override;

 private:
  CompilerDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(IdleTask);
};

CompilerDispatcher::IdleTask::IdleTask(Isolate* isolate,
                                       CancelableTaskManager* task_manager,
                                       CompilerDispatcher* dispatcher)
    : CancelableIdleTask(isolate, task_manager), dispatcher_(dispatcher) {}

CompilerDispatcher::IdleTask::~IdleTask() {}

void CompilerDispatcher::IdleTask::RunInternal(double deadline_in_seconds) {
  dispatcher_->DoIdleWork(deadline_in_seconds);
}

CompilerDispatcher::CompilerDispatcher(Isolate* isolate, Platform* platform,
                                       size_t max_stack_size)
    : isolate_(isolate),
      platform_(platform),
      max_stack_size_(max_stack_size),
      trace_compiler_dispatcher_(FLAG_trace_compiler_dispatcher),
      tracer_(new CompilerDispatcherTracer(isolate_)),
      task_manager_(new CancelableTaskManager()),
      memory_pressure_level_(MemoryPressureLevel::kNone),
      abort_(false),
      idle_task_scheduled_(false),
      num_scheduled_background_tasks_(0),
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

bool CompilerDispatcher::Enqueue(Handle<SharedFunctionInfo> function) {
  if (!IsEnabled()) return false;

  DCHECK(FLAG_ignition);

  if (memory_pressure_level_.Value() != MemoryPressureLevel::kNone) {
    return false;
  }

  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    if (abort_) return false;
  }

  // We only handle functions (no eval / top-level code / wasm) that are
  // attached to a script.
  if (!function->script()->IsScript() || function->is_toplevel() ||
      function->asm_function() || function->native()) {
    return false;
  }

  if (IsEnqueued(function)) return true;

  if (trace_compiler_dispatcher_) {
    PrintF("CompilerDispatcher: enqueuing ");
    function->ShortPrint();
    PrintF("\n");
  }

  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      isolate_, tracer_.get(), function, max_stack_size_));
  std::pair<int, int> key(Script::cast(function->script())->id(),
                          function->function_literal_id());
  jobs_.insert(std::make_pair(key, std::move(job)));
  ScheduleIdleTaskIfNeeded();
  return true;
}

bool CompilerDispatcher::EnqueueAndStep(Handle<SharedFunctionInfo> function) {
  if (!Enqueue(function)) return false;

  if (trace_compiler_dispatcher_) {
    PrintF("CompilerDispatcher: stepping ");
    function->ShortPrint();
    PrintF("\n");
  }
  JobMap::const_iterator job = GetJobFor(function);
  DoNextStepOnMainThread(isolate_, job->second.get(),
                         ExceptionHandling::kSwallow);
  ConsiderJobForBackgroundProcessing(job->second.get());
  return true;
}

bool CompilerDispatcher::IsEnabled() const {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  return FLAG_compiler_dispatcher && platform_->IdleTasksEnabled(v8_isolate);
}

bool CompilerDispatcher::IsEnqueued(Handle<SharedFunctionInfo> function) const {
  return GetJobFor(function) != jobs_.end();
}

void CompilerDispatcher::WaitForJobIfRunningOnBackground(
    CompilerDispatcherJob* job) {
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
  JobMap::const_iterator job = GetJobFor(function);
  CHECK(job != jobs_.end());

  if (trace_compiler_dispatcher_) {
    PrintF("CompilerDispatcher: finishing ");
    function->ShortPrint();
    PrintF(" now\n");
  }

  WaitForJobIfRunningOnBackground(job->second.get());
  while (!IsFinished(job->second.get())) {
    DoNextStepOnMainThread(isolate_, job->second.get(),
                           ExceptionHandling::kThrow);
  }
  bool result = job->second->status() != CompileJobStatus::kFailed;

  if (trace_compiler_dispatcher_) {
    PrintF("CompilerDispatcher: finished working on ");
    function->ShortPrint();
    PrintF(": %s\n", result ? "success" : "failure");
    tracer_->DumpStatistics();
  }

  job->second->ResetOnMainThread();
  jobs_.erase(job);
  if (jobs_.empty()) {
    base::LockGuard<base::Mutex> lock(&mutex_);
    abort_ = false;
  }
  return result;
}

void CompilerDispatcher::AbortAll(BlockingBehavior blocking) {
  bool background_tasks_running =
      task_manager_->TryAbortAll() == CancelableTaskManager::kTaskRunning;
  if (!background_tasks_running || blocking == BlockingBehavior::kBlock) {
    for (auto& it : jobs_) {
      WaitForJobIfRunningOnBackground(it.second.get());
      if (trace_compiler_dispatcher_) {
        PrintF("CompilerDispatcher: aborted ");
        it.second->ShortPrint();
        PrintF("\n");
      }
      it.second->ResetOnMainThread();
    }
    jobs_.clear();
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
  for (auto it = jobs_.begin(); it != jobs_.end();) {
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
      PrintF("CompilerDispatcher: aborted ");
      job->second->ShortPrint();
      PrintF("\n");
    }
    job->second->ResetOnMainThread();
    jobs_.erase(job);
  }
  if (jobs_.empty()) {
    base::LockGuard<base::Mutex> lock(&mutex_);
    abort_ = false;
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
    platform_->CallOnForegroundThread(
        reinterpret_cast<v8::Isolate*>(isolate_),
        new MemoryPressureTask(isolate_, task_manager_.get(), this));
  }
}

CompilerDispatcher::JobMap::const_iterator CompilerDispatcher::GetJobFor(
    Handle<SharedFunctionInfo> shared) const {
  if (!shared->script()->IsScript()) return jobs_.end();
  std::pair<int, int> key(Script::cast(shared->script())->id(),
                          shared->function_literal_id());
  auto range = jobs_.equal_range(key);
  for (auto job = range.first; job != range.second; ++job) {
    if (job->second->IsAssociatedWith(shared)) return job;
  }
  return jobs_.end();
}

void CompilerDispatcher::ScheduleIdleTaskFromAnyThread() {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  DCHECK(platform_->IdleTasksEnabled(v8_isolate));
  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    if (idle_task_scheduled_) return;
    idle_task_scheduled_ = true;
  }
  platform_->CallIdleOnForegroundThread(
      v8_isolate, new IdleTask(isolate_, task_manager_.get(), this));
}

void CompilerDispatcher::ScheduleIdleTaskIfNeeded() {
  if (jobs_.empty()) return;
  ScheduleIdleTaskFromAnyThread();
}

void CompilerDispatcher::ScheduleAbortTask() {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  platform_->CallOnForegroundThread(
      v8_isolate, new AbortTask(isolate_, task_manager_.get(), this));
}

void CompilerDispatcher::ConsiderJobForBackgroundProcessing(
    CompilerDispatcherJob* job) {
  if (!CanRunOnAnyThread(job)) return;
  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    pending_background_jobs_.insert(job);
  }
  ScheduleMoreBackgroundTasksIfNeeded();
}

void CompilerDispatcher::ScheduleMoreBackgroundTasksIfNeeded() {
  if (FLAG_single_threaded) return;
  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    if (pending_background_jobs_.empty()) return;
    if (platform_->NumberOfAvailableBackgroundThreads() <=
        num_scheduled_background_tasks_) {
      return;
    }
    ++num_scheduled_background_tasks_;
  }
  platform_->CallOnBackgroundThread(
      new BackgroundTask(isolate_, task_manager_.get(), this),
      v8::Platform::kShortRunningTask);
}

void CompilerDispatcher::DoBackgroundWork() {
  CompilerDispatcherJob* job = nullptr;
  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    --num_scheduled_background_tasks_;
    if (!pending_background_jobs_.empty()) {
      auto it = pending_background_jobs_.begin();
      job = *it;
      pending_background_jobs_.erase(it);
      running_background_jobs_.insert(job);
    }
  }
  if (job == nullptr) return;

  if (V8_UNLIKELY(block_for_testing_.Value())) {
    block_for_testing_.SetValue(false);
    semaphore_for_testing_.Wait();
  }

  if (trace_compiler_dispatcher_) {
    PrintF("CompilerDispatcher: doing background work\n");
  }

  DoNextStepOnBackgroundThread(job);

  ScheduleMoreBackgroundTasksIfNeeded();
  // Unconditionally schedule an idle task, as all background steps have to be
  // followed by a main thread step.
  ScheduleIdleTaskFromAnyThread();

  {
    base::LockGuard<base::Mutex> lock(&mutex_);
    running_background_jobs_.erase(job);

    if (running_background_jobs_.empty() && abort_) {
      // This is the last background job that finished. The abort task
      // scheduled by AbortAll might already have ran, so schedule another
      // one to be on the safe side.
      ScheduleAbortTask();
    }

    if (main_thread_blocking_on_job_ == job) {
      main_thread_blocking_on_job_ = nullptr;
      main_thread_blocking_signal_.NotifyOne();
    }
  }
  // Don't touch |this| anymore after this point, as it might have been
  // deleted.
}

void CompilerDispatcher::DoIdleWork(double deadline_in_seconds) {
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
  size_t too_long_jobs = 0;

  // Iterate over all available jobs & remaining time. For each job, decide
  // whether to 1) skip it (if it would take too long), 2) erase it (if it's
  // finished), or 3) make progress on it.
  double idle_time_in_seconds =
      deadline_in_seconds - platform_->MonotonicallyIncreasingTime();

  if (trace_compiler_dispatcher_) {
    PrintF("CompilerDispatcher: received %0.1lfms of idle time\n",
           idle_time_in_seconds *
               static_cast<double>(base::Time::kMillisecondsPerSecond));
  }
  for (auto job = jobs_.begin();
       job != jobs_.end() && idle_time_in_seconds > 0.0;
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
    auto it = pending_background_jobs_.find(job->second.get());
    double estimate_in_ms = job->second->EstimateRuntimeOfNextStepInMs();
    if (idle_time_in_seconds <
        (estimate_in_ms /
         static_cast<double>(base::Time::kMillisecondsPerSecond))) {
      // If there's not enough time left, try to estimate whether we would
      // have managed to finish the job in a large idle task to assess
      // whether we should ask for another idle callback.
      if (estimate_in_ms > kMaxIdleTimeToExpectInMs) ++too_long_jobs;
      if (it == pending_background_jobs_.end()) {
        lock.reset();
        ConsiderJobForBackgroundProcessing(job->second.get());
      }
      ++job;
    } else if (IsFinished(job->second.get())) {
      DCHECK(it == pending_background_jobs_.end());
      if (trace_compiler_dispatcher_) {
        PrintF("CompilerDispatcher: finished working on ");
        job->second->ShortPrint();
        PrintF(": %s\n", job->second->status() == CompileJobStatus::kDone
                             ? "success"
                             : "failure");
        tracer_->DumpStatistics();
      }
      job->second->ResetOnMainThread();
      job = jobs_.erase(job);
      continue;
    } else {
      // Do one step, and keep processing the job (as we don't advance the
      // iterator).
      if (it != pending_background_jobs_.end()) {
        pending_background_jobs_.erase(it);
      }
      lock.reset();
      DoNextStepOnMainThread(isolate_, job->second.get(),
                             ExceptionHandling::kSwallow);
    }
  }
  if (jobs_.size() > too_long_jobs) ScheduleIdleTaskIfNeeded();
}

}  // namespace internal
}  // namespace v8

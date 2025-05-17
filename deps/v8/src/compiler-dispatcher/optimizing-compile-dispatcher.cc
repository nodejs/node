// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"

#include "src/base/atomicops.h"
#include "src/base/fpu.h"
#include "src/base/logging.h"
#include "src/base/platform/mutex.h"
#include "src/base/vector.h"
#include "src/codegen/compiler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/execution/isolate.h"
#include "src/execution/local-isolate-inl.h"
#include "src/handles/handles-inl.h"
#include "src/heap/local-heap-inl.h"
#include "src/init/v8.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/js-function.h"
#include "src/tasks/cancelable-task.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

class OptimizingCompileTaskExecutor::CompileTask : public v8::JobTask {
 public:
  explicit CompileTask(OptimizingCompileTaskExecutor* task_executor)
      : task_executor_(task_executor) {}

  void Run(JobDelegate* delegate) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.TurbofanTask");
    DCHECK_LT(delegate->GetTaskId(), task_executor_->task_states_.size());
    OptimizingCompileTaskState& task_state =
        task_executor_->task_states_[delegate->GetTaskId()];
    bool should_yield = delegate->ShouldYield();

    while (!should_yield) {
      // NextInput() sets the isolate for task_state to job->isolate() while
      // holding the lock.
      TurbofanCompilationJob* job = task_executor_->NextInput(task_state);
      if (!job) break;

      Isolate* const isolate = job->isolate();

      {
        base::FlushDenormalsScope flush_denormals_scope(
            isolate->flush_denormals());

        // Note that LocalIsolate's lifetime is shorter than the isolate value
        // in task_state which is only cleared after this LocalIsolate instance
        // was destroyed.
        LocalIsolate local_isolate(isolate, ThreadKind::kBackground);
        DCHECK(local_isolate.heap()->IsParked());

        do {
          task_executor_->RunCompilationJob(task_state, isolate, local_isolate,
                                            job);

          should_yield = delegate->ShouldYield();
          if (should_yield) break;

          // Reuse the LocalIsolate if the next worklist item has the same
          // isolate.
          job = task_executor_->NextInputIfIsolateMatches(task_state);
        } while (job);
      }

      // Reset the isolate in the task state to nullptr. Only do this after the
      // LocalIsolate was destroyed. This invariant is used by
      // WaitUntilTasksStoppedForIsolate() to ensure all tasks are stopped for
      // an isolate.
      task_executor_->ClearTaskState(task_state);
    }

    // Here we are allowed to read the isolate without holding a lock because
    // only this thread here will ever change this field and the main thread
    // will only ever read it.
    DCHECK_NULL(task_state.isolate);
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    size_t num_tasks = task_executor_->input_queue_.Length() + worker_count;
    return std::min(num_tasks, task_executor_->task_states_.size());
  }

 private:
  OptimizingCompileTaskExecutor* task_executor_;
};

OptimizingCompileTaskExecutor::OptimizingCompileTaskExecutor()
    : input_queue_(v8_flags.concurrent_recompilation_queue_length),
      recompilation_delay_(v8_flags.concurrent_recompilation_delay) {}

OptimizingCompileTaskExecutor::~OptimizingCompileTaskExecutor() {
  DCHECK_EQ(input_queue_.Length(), 0);

  if (job_handle_) {
    DCHECK(job_handle_->IsValid());

    // Wait for the job handle to complete, so that we know the queue
    // pointers are safe.
    job_handle_->Cancel();
  }
}

void OptimizingCompileTaskExecutor::EnsureInitialized() {
  if (is_initialized_) return;
  is_initialized_ = true;

  if (v8_flags.concurrent_recompilation ||
      v8_flags.concurrent_builtin_generation) {
    int max_tasks;

    if (v8_flags.concurrent_turbofan_max_threads == 0) {
      max_tasks = V8::GetCurrentPlatform()->NumberOfWorkerThreads();
    } else {
      max_tasks = v8_flags.concurrent_turbofan_max_threads;
    }

    task_states_ =
        base::OwnedVector<OptimizingCompileTaskState>::New(max_tasks);
    job_handle_ = V8::GetCurrentPlatform()->PostJob(
        kTaskPriority, std::make_unique<CompileTask>(this));
  }
}

TurbofanCompilationJob* OptimizingCompileTaskExecutor::NextInput(
    OptimizingCompileTaskState& task_state) {
  return input_queue_.Dequeue(task_state);
}

TurbofanCompilationJob*
OptimizingCompileTaskExecutor::NextInputIfIsolateMatches(
    OptimizingCompileTaskState& task_state) {
  return input_queue_.DequeueIfIsolateMatches(task_state);
}

void OptimizingCompileTaskExecutor::RunCompilationJob(
    OptimizingCompileTaskState& task_state, Isolate* isolate,
    LocalIsolate& local_isolate, TurbofanCompilationJob* job) {
  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                         "V8.OptimizeBackground", job->trace_id(),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  TimerEventScope<TimerEventRecompileConcurrent> timer(isolate);

  if (recompilation_delay_ != 0) {
    base::OS::Sleep(base::TimeDelta::FromMilliseconds(recompilation_delay_));
  }

  RCS_SCOPE(&local_isolate, RuntimeCallCounterId::kOptimizeBackgroundTurbofan);

  // The function may have already been optimized by OSR.  Simply continue.
  CompilationJob::Status status =
      job->ExecuteJob(local_isolate.runtime_call_stats(), &local_isolate);
  USE(status);  // Prevent an unused-variable error.

  // Remove the job first from task_state before adding it to the output queue.
  // As soon as the job is in the output queue it could be deleted any moment.
  ResetJob(task_state);

  isolate->optimizing_compile_dispatcher()->QueueFinishedJob(job);
}

bool OptimizingCompileTaskExecutor::IsTaskRunningForIsolate(Isolate* isolate) {
  input_queue_.mutex_.AssertHeld();

  for (auto& task_state : task_states_) {
    if (task_state.isolate == isolate) {
      return true;
    }
  }

  return false;
}

bool OptimizingCompileTaskExecutor::HasCompilationJobsForIsolate(
    Isolate* isolate) {
  base::MutexGuard guard(input_queue_.mutex_);
  return input_queue_.HasJobForIsolate(isolate) ||
         IsTaskRunningForIsolate(isolate);
}

void OptimizingCompileTaskExecutor::ClearTaskState(
    OptimizingCompileTaskState& task_state) {
  base::MutexGuard guard(input_queue_.mutex_);
  DCHECK_NOT_NULL(task_state.isolate);
  task_state.isolate = nullptr;
  DCHECK_NULL(task_state.job);
  input_queue_.task_finished_.NotifyAll();
}

void OptimizingCompileTaskExecutor::ResetJob(
    OptimizingCompileTaskState& task_state) {
  base::MutexGuard guard(input_queue_.mutex_);
  DCHECK_NOT_NULL(task_state.isolate);
  DCHECK_NOT_NULL(task_state.job);
  task_state.job = nullptr;
}

bool OptimizingCompileTaskExecutor::TryQueueForOptimization(
    std::unique_ptr<TurbofanCompilationJob>& job) {
  Isolate* isolate = job->isolate();
  DCHECK_NOT_NULL(isolate);

  if (input_queue_.Enqueue(job)) {
    if (job_handle_->UpdatePriorityEnabled()) {
      job_handle_->UpdatePriority(isolate->EfficiencyModeEnabledForTiering()
                                      ? kEfficiencyTaskPriority
                                      : kTaskPriority);
    }
    job_handle_->NotifyConcurrencyIncrease();
    return true;
  } else {
    return false;
  }
}

void OptimizingCompileTaskExecutor::WaitUntilCompilationJobsDoneForIsolate(
    Isolate* isolate) {
  // Once we have ensured that no task is working on the given isolate, we also
  // know that there are no more LocalHeaps for this isolate from CompileTask.
  // This is because CompileTask::Run() only updates the isolate once the
  // LocalIsolate/LocalHeap for it was destroyed.
  base::MutexGuard guard(&input_queue_.mutex_);

  while (input_queue_.HasJobForIsolate(isolate) ||
         IsTaskRunningForIsolate(isolate)) {
    input_queue_.task_finished_.Wait(&input_queue_.mutex_);
  }
}

void OptimizingCompileTaskExecutor::CancelCompilationJobsForIsolate(
    Isolate* isolate) {
  base::MutexGuard guard(&input_queue_.mutex_);
  DCHECK(!input_queue_.HasJobForIsolate(isolate));

  for (auto& task_state : task_states_) {
    if (task_state.isolate == isolate && task_state.job) {
      task_state.job->Cancel();
    }
  }
}

OptimizingCompileDispatcher::OptimizingCompileDispatcher(
    Isolate* isolate, OptimizingCompileTaskExecutor* task_executor)
    : isolate_(isolate), task_executor_(task_executor) {}

OptimizingCompileDispatcher::~OptimizingCompileDispatcher() {
  DCHECK_EQ(output_queue_.size(), 0);
}

void OptimizingCompileDispatcher::QueueFinishedJob(
    TurbofanCompilationJob* job) {
  DCHECK_EQ(isolate_, job->isolate());
  output_queue_.Enqueue(job);
  if (finalize()) isolate_->stack_guard()->RequestInstallCode();
}

void OptimizingCompileDispatcher::FlushOutputQueue() {
  for (;;) {
    std::unique_ptr<TurbofanCompilationJob> job = output_queue_.Dequeue();
    if (!job) break;
    Compiler::DisposeTurbofanCompilationJob(isolate_, job.get());
  }
}

void OptimizingCompileDispatcher::FinishTearDown() {
  task_executor_->WaitUntilCompilationJobsDoneForIsolate(isolate_);

  HandleScope handle_scope(isolate_);
  FlushOutputQueue();
}

void OptimizingCompileDispatcher::FlushInputQueue() {
  input_queue().FlushJobsForIsolate(isolate_);
}

void OptimizingCompileDispatcher::WaitUntilCompilationJobsDone() {
  AllowGarbageCollection allow_before_parking;
  isolate_->main_thread_local_isolate()->ExecuteMainThreadWhileParked([this]() {
    task_executor_->WaitUntilCompilationJobsDoneForIsolate(isolate_);
  });
}

void OptimizingCompileDispatcher::FlushQueues(
    BlockingBehavior blocking_behavior) {
  FlushInputQueue();
  task_executor_->CancelCompilationJobsForIsolate(isolate_);
  if (blocking_behavior == BlockingBehavior::kBlock) {
    WaitUntilCompilationJobsDone();
  }
  FlushOutputQueue();
}

void OptimizingCompileDispatcher::Flush(BlockingBehavior blocking_behavior) {
  HandleScope handle_scope(isolate_);
  FlushQueues(blocking_behavior);
  if (v8_flags.trace_concurrent_recompilation) {
    PrintF("  ** Flushed concurrent recompilation queues. (mode: %s)\n",
           (blocking_behavior == BlockingBehavior::kBlock) ? "blocking"
                                                           : "non blocking");
  }
}

void OptimizingCompileDispatcher::StartTearDown() {
  HandleScope handle_scope(isolate_);
  FlushInputQueue();

  task_executor_->CancelCompilationJobsForIsolate(isolate_);
}

void OptimizingCompileDispatcher::InstallOptimizedFunctions() {
  HandleScope handle_scope(isolate_);

  for (;;) {
    std::unique_ptr<TurbofanCompilationJob> job = output_queue_.Dequeue();
    if (!job) break;

    OptimizedCompilationInfo* info = job->compilation_info();
    DirectHandle<JSFunction> function(*info->closure(), isolate_);

    // If another racing task has already finished compiling and installing the
    // requested code kind on the function, throw out the current job.
    if (!info->is_osr() &&
        function->HasAvailableCodeKind(isolate_, info->code_kind())) {
      if (v8_flags.trace_concurrent_recompilation) {
        PrintF("  ** Aborting compilation for ");
        ShortPrint(*function);
        PrintF(" as it has already been optimized.\n");
      }
      Compiler::DisposeTurbofanCompilationJob(isolate_, job.get());
      continue;
    }
    // Discard code compiled for a discarded native context without
    // finalization.
    if (function->native_context()->global_object()->IsDetached()) {
      Compiler::DisposeTurbofanCompilationJob(isolate_, job.get());
      continue;
    }

    Compiler::FinalizeTurbofanCompilationJob(job.get(), isolate_);
  }
}

int OptimizingCompileDispatcher::InstallGeneratedBuiltins(int installed_count) {
  return output_queue_.InstallGeneratedBuiltins(isolate_, installed_count);
}

bool OptimizingCompileDispatcher::HasJobs() {
  DCHECK_EQ(ThreadId::Current(), isolate_->thread_id());
  if (task_executor_->HasCompilationJobsForIsolate(isolate_)) {
    return true;
  }

  return !output_queue_.empty();
}

bool OptimizingCompileDispatcher::TryQueueForOptimization(
    std::unique_ptr<TurbofanCompilationJob>& job) {
  return task_executor_->TryQueueForOptimization(job);
}

void OptimizingCompileDispatcher::Prioritize(
    Tagged<SharedFunctionInfo> function) {
  input_queue().Prioritize(isolate_, function);
}

void OptimizingCompileInputQueue::Prioritize(
    Isolate* isolate, Tagged<SharedFunctionInfo> function) {
  // Ensure that we only run this method on the main thread. This makes sure
  // that we never dereference handles during a safepoint.
  DCHECK_EQ(isolate->thread_id(), ThreadId::Current());
  base::MutexGuard access(&mutex_);
  auto it =
      std::find_if(queue_.begin(), queue_.end(),
                   [isolate, function](TurbofanCompilationJob* job) {
                     // Early bailout to avoid dereferencing handles from other
                     // isolates. The other isolate could be in a safepoint/GC
                     // and dereferencing the handle is therefore invalid.
                     if (job->isolate() != isolate) return false;
                     return *job->compilation_info()->shared_info() == function;
                   });

  if (it != queue_.end()) {
    auto first_for_isolate = std::find_if(
        queue_.begin(), queue_.end(), [isolate](TurbofanCompilationJob* job) {
          return job->isolate() == isolate;
        });
    DCHECK_NE(first_for_isolate, queue_.end());
    std::iter_swap(it, first_for_isolate);
  }
}

void OptimizingCompileInputQueue::FlushJobsForIsolate(Isolate* isolate) {
  base::MutexGuard access(&mutex_);
  std::erase_if(queue_, [isolate](TurbofanCompilationJob* job) {
    if (job->isolate() != isolate) return false;
    Compiler::DisposeTurbofanCompilationJob(isolate, job);
    delete job;
    return true;
  });
}

bool OptimizingCompileInputQueue::HasJobForIsolate(Isolate* isolate) {
  mutex_.AssertHeld();
  return std::find_if(queue_.begin(), queue_.end(),
                      [isolate](TurbofanCompilationJob* job) {
                        return job->isolate() == isolate;
                      }) != queue_.end();
}

TurbofanCompilationJob* OptimizingCompileInputQueue::Dequeue(
    OptimizingCompileTaskState& task_state) {
  base::MutexGuard access(&mutex_);
  DCHECK_NULL(task_state.isolate);
  if (queue_.empty()) return nullptr;
  TurbofanCompilationJob* job = queue_.front();
  queue_.pop_front();
  DCHECK_NOT_NULL(job);
  task_state.isolate = job->isolate();
  task_state.job = job;
  return job;
}

TurbofanCompilationJob* OptimizingCompileInputQueue::DequeueIfIsolateMatches(
    OptimizingCompileTaskState& task_state) {
  base::MutexGuard access(&mutex_);
  if (queue_.empty()) return nullptr;
  TurbofanCompilationJob* job = queue_.front();
  DCHECK_NOT_NULL(job);
  if (job->isolate() != task_state.isolate) return nullptr;
  DCHECK_NULL(task_state.job);
  task_state.job = job;
  queue_.pop_front();
  return job;
}

bool OptimizingCompileInputQueue::Enqueue(
    std::unique_ptr<TurbofanCompilationJob>& job) {
  base::MutexGuard access(&mutex_);
  if (queue_.size() < capacity_) {
    queue_.push_back(job.release());
    return true;
  } else {
    return false;
  }
}

void OptimizingCompileOutputQueue::Enqueue(TurbofanCompilationJob* job) {
  base::MutexGuard guard(&mutex_);
  queue_.push_back(job);
}

std::unique_ptr<TurbofanCompilationJob>
OptimizingCompileOutputQueue::Dequeue() {
  base::MutexGuard guard(&mutex_);
  if (queue_.empty()) return {};
  std::unique_ptr<TurbofanCompilationJob> job(queue_.front());
  queue_.pop_front();
  return job;
}

size_t OptimizingCompileOutputQueue::size() const { return queue_.size(); }

bool OptimizingCompileOutputQueue::empty() const { return queue_.empty(); }

int OptimizingCompileOutputQueue::InstallGeneratedBuiltins(
    Isolate* isolate, int installed_count) {
  // Builtin generation needs to be deterministic, meaning heap allocations must
  // happen in a deterministic order. To ensure determinism with concurrent
  // compilation, only finalize contiguous builtins in ascending order of their
  // finalization order, which is set at job creation time.

  CHECK(isolate->IsGeneratingEmbeddedBuiltins());

  base::MutexGuard guard(&mutex_);

  std::sort(queue_.begin(), queue_.end(),
            [](const TurbofanCompilationJob* job1,
               const TurbofanCompilationJob* job2) {
              return job1->FinalizeOrder() < job2->FinalizeOrder();
            });

  while (!queue_.empty()) {
    int current = queue_.front()->FinalizeOrder();
    CHECK_EQ(installed_count, current);
    std::unique_ptr<TurbofanCompilationJob> job(queue_.front());
    queue_.pop_front();
    CHECK_EQ(CompilationJob::SUCCEEDED, job->FinalizeJob(isolate));
    installed_count = current + 1;
  }
  return installed_count;
}

}  // namespace internal
}  // namespace v8

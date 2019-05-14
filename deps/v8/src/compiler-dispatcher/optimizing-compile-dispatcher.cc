// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"

#include "src/base/atomicops.h"
#include "src/base/template-utils.h"
#include "src/cancelable-task.h"
#include "src/compiler.h"
#include "src/counters.h"
#include "src/isolate.h"
#include "src/log.h"
#include "src/objects-inl.h"
#include "src/optimized-compilation-info.h"
#include "src/tracing/trace-event.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

namespace {

void DisposeCompilationJob(OptimizedCompilationJob* job,
                           bool restore_function_code) {
  if (restore_function_code) {
    Handle<JSFunction> function = job->compilation_info()->closure();
    function->set_code(function->shared()->GetCode());
    if (function->IsInOptimizationQueue()) {
      function->ClearOptimizationMarker();
    }
    // TODO(mvstanton): We can't call EnsureFeedbackVector here due to
    // allocation, but we probably shouldn't call set_code either, as this
    // sometimes runs on the worker thread!
    // JSFunction::EnsureFeedbackVector(function);
  }
  delete job;
}

}  // namespace

class OptimizingCompileDispatcher::CompileTask : public CancelableTask {
 public:
  explicit CompileTask(Isolate* isolate,
                       OptimizingCompileDispatcher* dispatcher)
      : CancelableTask(isolate),
        isolate_(isolate),
        worker_thread_runtime_call_stats_(
            isolate->counters()->worker_thread_runtime_call_stats()),
        dispatcher_(dispatcher) {
    base::MutexGuard lock_guard(&dispatcher_->ref_count_mutex_);
    ++dispatcher_->ref_count_;
  }

  ~CompileTask() override = default;

 private:
  // v8::Task overrides.
  void RunInternal() override {
    DisallowHeapAllocation no_allocation;
    DisallowHandleAllocation no_handles;
    DisallowHandleDereference no_deref;

    {
      WorkerThreadRuntimeCallStatsScope runtime_call_stats_scope(
          worker_thread_runtime_call_stats_);
      RuntimeCallTimerScope runtimeTimer(
          runtime_call_stats_scope.Get(),
          RuntimeCallCounterId::kRecompileConcurrent);

      TimerEventScope<TimerEventRecompileConcurrent> timer(isolate_);
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.RecompileConcurrent");

      if (dispatcher_->recompilation_delay_ != 0) {
        base::OS::Sleep(base::TimeDelta::FromMilliseconds(
            dispatcher_->recompilation_delay_));
      }

      dispatcher_->CompileNext(dispatcher_->NextInput(true));
    }
    {
      base::MutexGuard lock_guard(&dispatcher_->ref_count_mutex_);
      if (--dispatcher_->ref_count_ == 0) {
        dispatcher_->ref_count_zero_.NotifyOne();
      }
    }
  }

  Isolate* isolate_;
  WorkerThreadRuntimeCallStats* worker_thread_runtime_call_stats_;
  OptimizingCompileDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(CompileTask);
};

OptimizingCompileDispatcher::~OptimizingCompileDispatcher() {
#ifdef DEBUG
  {
    base::MutexGuard lock_guard(&ref_count_mutex_);
    DCHECK_EQ(0, ref_count_);
  }
#endif
  DCHECK_EQ(0, input_queue_length_);
  DeleteArray(input_queue_);
}

OptimizedCompilationJob* OptimizingCompileDispatcher::NextInput(
    bool check_if_flushing) {
  base::MutexGuard access_input_queue_(&input_queue_mutex_);
  if (input_queue_length_ == 0) return nullptr;
  OptimizedCompilationJob* job = input_queue_[InputQueueIndex(0)];
  DCHECK_NOT_NULL(job);
  input_queue_shift_ = InputQueueIndex(1);
  input_queue_length_--;
  if (check_if_flushing) {
    if (mode_ == FLUSH) {
      AllowHandleDereference allow_handle_dereference;
      DisposeCompilationJob(job, true);
      return nullptr;
    }
  }
  return job;
}

void OptimizingCompileDispatcher::CompileNext(OptimizedCompilationJob* job) {
  if (!job) return;

  // The function may have already been optimized by OSR.  Simply continue.
  CompilationJob::Status status = job->ExecuteJob();
  USE(status);  // Prevent an unused-variable error.

  {
    // The function may have already been optimized by OSR.  Simply continue.
    // Use a mutex to make sure that functions marked for install
    // are always also queued.
    base::MutexGuard access_output_queue_(&output_queue_mutex_);
    output_queue_.push(job);
  }

  isolate_->stack_guard()->RequestInstallCode();
}

void OptimizingCompileDispatcher::FlushOutputQueue(bool restore_function_code) {
  for (;;) {
    OptimizedCompilationJob* job = nullptr;
    {
      base::MutexGuard access_output_queue_(&output_queue_mutex_);
      if (output_queue_.empty()) return;
      job = output_queue_.front();
      output_queue_.pop();
    }

    DisposeCompilationJob(job, restore_function_code);
  }
}

void OptimizingCompileDispatcher::Flush(BlockingBehavior blocking_behavior) {
  if (blocking_behavior == BlockingBehavior::kDontBlock) {
    if (FLAG_block_concurrent_recompilation) Unblock();
    base::MutexGuard access_input_queue_(&input_queue_mutex_);
    while (input_queue_length_ > 0) {
      OptimizedCompilationJob* job = input_queue_[InputQueueIndex(0)];
      DCHECK_NOT_NULL(job);
      input_queue_shift_ = InputQueueIndex(1);
      input_queue_length_--;
      DisposeCompilationJob(job, true);
    }
    FlushOutputQueue(true);
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** Flushed concurrent recompilation queues (not blocking).\n");
    }
    return;
  }
  mode_ = FLUSH;
  if (FLAG_block_concurrent_recompilation) Unblock();
  {
    base::MutexGuard lock_guard(&ref_count_mutex_);
    while (ref_count_ > 0) ref_count_zero_.Wait(&ref_count_mutex_);
    mode_ = COMPILE;
  }
  FlushOutputQueue(true);
  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Flushed concurrent recompilation queues.\n");
  }
}

void OptimizingCompileDispatcher::Stop() {
  mode_ = FLUSH;
  if (FLAG_block_concurrent_recompilation) Unblock();
  {
    base::MutexGuard lock_guard(&ref_count_mutex_);
    while (ref_count_ > 0) ref_count_zero_.Wait(&ref_count_mutex_);
    mode_ = COMPILE;
  }

  if (recompilation_delay_ != 0) {
    // At this point the optimizing compiler thread's event loop has stopped.
    // There is no need for a mutex when reading input_queue_length_.
    while (input_queue_length_ > 0) CompileNext(NextInput());
    InstallOptimizedFunctions();
  } else {
    FlushOutputQueue(false);
  }
}

void OptimizingCompileDispatcher::InstallOptimizedFunctions() {
  HandleScope handle_scope(isolate_);

  for (;;) {
    OptimizedCompilationJob* job = nullptr;
    {
      base::MutexGuard access_output_queue_(&output_queue_mutex_);
      if (output_queue_.empty()) return;
      job = output_queue_.front();
      output_queue_.pop();
    }
    OptimizedCompilationInfo* info = job->compilation_info();
    Handle<JSFunction> function(*info->closure(), isolate_);
    if (function->HasOptimizedCode()) {
      if (FLAG_trace_concurrent_recompilation) {
        PrintF("  ** Aborting compilation for ");
        function->ShortPrint();
        PrintF(" as it has already been optimized.\n");
      }
      DisposeCompilationJob(job, false);
    } else {
      Compiler::FinalizeOptimizedCompilationJob(job, isolate_);
    }
  }
}

void OptimizingCompileDispatcher::QueueForOptimization(
    OptimizedCompilationJob* job) {
  DCHECK(IsQueueAvailable());
  {
    // Add job to the back of the input queue.
    base::MutexGuard access_input_queue(&input_queue_mutex_);
    DCHECK_LT(input_queue_length_, input_queue_capacity_);
    input_queue_[InputQueueIndex(input_queue_length_)] = job;
    input_queue_length_++;
  }
  if (FLAG_block_concurrent_recompilation) {
    blocked_jobs_++;
  } else {
    V8::GetCurrentPlatform()->CallOnWorkerThread(
        base::make_unique<CompileTask>(isolate_, this));
  }
}

void OptimizingCompileDispatcher::Unblock() {
  while (blocked_jobs_ > 0) {
    V8::GetCurrentPlatform()->CallOnWorkerThread(
        base::make_unique<CompileTask>(isolate_, this));
    blocked_jobs_--;
  }
}

}  // namespace internal
}  // namespace v8

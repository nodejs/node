// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/optimizing-compile-dispatcher.h"

#include "src/base/atomicops.h"
#include "src/full-codegen/full-codegen.h"
#include "src/isolate.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

namespace {

void DisposeOptimizedCompileJob(OptimizedCompileJob* job,
                                bool restore_function_code) {
  // The recompile job is allocated in the CompilationInfo's zone.
  CompilationInfo* info = job->info();
  if (restore_function_code) {
    if (info->is_osr()) {
      if (!job->IsWaitingForInstall()) {
        // Remove stack check that guards OSR entry on original code.
        Handle<Code> code = info->unoptimized_code();
        uint32_t offset = code->TranslateAstIdToPcOffset(info->osr_ast_id());
        BackEdgeTable::RemoveStackCheck(code, offset);
      }
    } else {
      Handle<JSFunction> function = info->closure();
      function->ReplaceCode(function->shared()->code());
    }
  }
  delete info;
}

}  // namespace


class OptimizingCompileDispatcher::CompileTask : public v8::Task {
 public:
  explicit CompileTask(Isolate* isolate) : isolate_(isolate) {
    OptimizingCompileDispatcher* dispatcher =
        isolate_->optimizing_compile_dispatcher();
    base::LockGuard<base::Mutex> lock_guard(&dispatcher->ref_count_mutex_);
    ++dispatcher->ref_count_;
  }

  virtual ~CompileTask() {}

 private:
  // v8::Task overrides.
  void Run() override {
    DisallowHeapAllocation no_allocation;
    DisallowHandleAllocation no_handles;
    DisallowHandleDereference no_deref;

    OptimizingCompileDispatcher* dispatcher =
        isolate_->optimizing_compile_dispatcher();
    {
      TimerEventScope<TimerEventRecompileConcurrent> timer(isolate_);

      if (dispatcher->recompilation_delay_ != 0) {
        base::OS::Sleep(base::TimeDelta::FromMilliseconds(
            dispatcher->recompilation_delay_));
      }

      dispatcher->CompileNext(dispatcher->NextInput(true));
    }
    {
      base::LockGuard<base::Mutex> lock_guard(&dispatcher->ref_count_mutex_);
      if (--dispatcher->ref_count_ == 0) {
        dispatcher->ref_count_zero_.NotifyOne();
      }
    }
  }

  Isolate* isolate_;

  DISALLOW_COPY_AND_ASSIGN(CompileTask);
};


OptimizingCompileDispatcher::~OptimizingCompileDispatcher() {
#ifdef DEBUG
  {
    base::LockGuard<base::Mutex> lock_guard(&ref_count_mutex_);
    DCHECK_EQ(0, ref_count_);
  }
#endif
  DCHECK_EQ(0, input_queue_length_);
  DeleteArray(input_queue_);
  if (FLAG_concurrent_osr) {
#ifdef DEBUG
    for (int i = 0; i < osr_buffer_capacity_; i++) {
      CHECK_NULL(osr_buffer_[i]);
    }
#endif
    DeleteArray(osr_buffer_);
  }
}


OptimizedCompileJob* OptimizingCompileDispatcher::NextInput(
    bool check_if_flushing) {
  base::LockGuard<base::Mutex> access_input_queue_(&input_queue_mutex_);
  if (input_queue_length_ == 0) return NULL;
  OptimizedCompileJob* job = input_queue_[InputQueueIndex(0)];
  DCHECK_NOT_NULL(job);
  input_queue_shift_ = InputQueueIndex(1);
  input_queue_length_--;
  if (check_if_flushing) {
    if (static_cast<ModeFlag>(base::Acquire_Load(&mode_)) == FLUSH) {
      if (!job->info()->is_osr()) {
        AllowHandleDereference allow_handle_dereference;
        DisposeOptimizedCompileJob(job, true);
      }
      return NULL;
    }
  }
  return job;
}


void OptimizingCompileDispatcher::CompileNext(OptimizedCompileJob* job) {
  if (!job) return;

  // The function may have already been optimized by OSR.  Simply continue.
  OptimizedCompileJob::Status status = job->OptimizeGraph();
  USE(status);  // Prevent an unused-variable error in release mode.
  DCHECK(status != OptimizedCompileJob::FAILED);

  // The function may have already been optimized by OSR.  Simply continue.
  // Use a mutex to make sure that functions marked for install
  // are always also queued.
  base::LockGuard<base::Mutex> access_output_queue_(&output_queue_mutex_);
  output_queue_.push(job);
  isolate_->stack_guard()->RequestInstallCode();
}


void OptimizingCompileDispatcher::FlushOutputQueue(bool restore_function_code) {
  for (;;) {
    OptimizedCompileJob* job = NULL;
    {
      base::LockGuard<base::Mutex> access_output_queue_(&output_queue_mutex_);
      if (output_queue_.empty()) return;
      job = output_queue_.front();
      output_queue_.pop();
    }

    // OSR jobs are dealt with separately.
    if (!job->info()->is_osr()) {
      DisposeOptimizedCompileJob(job, restore_function_code);
    }
  }
}


void OptimizingCompileDispatcher::FlushOsrBuffer(bool restore_function_code) {
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    if (osr_buffer_[i] != NULL) {
      DisposeOptimizedCompileJob(osr_buffer_[i], restore_function_code);
      osr_buffer_[i] = NULL;
    }
  }
}


void OptimizingCompileDispatcher::Flush() {
  base::Release_Store(&mode_, static_cast<base::AtomicWord>(FLUSH));
  if (FLAG_block_concurrent_recompilation) Unblock();
  {
    base::LockGuard<base::Mutex> lock_guard(&ref_count_mutex_);
    while (ref_count_ > 0) ref_count_zero_.Wait(&ref_count_mutex_);
    base::Release_Store(&mode_, static_cast<base::AtomicWord>(COMPILE));
  }
  FlushOutputQueue(true);
  if (FLAG_concurrent_osr) FlushOsrBuffer(true);
  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Flushed concurrent recompilation queues.\n");
  }
}


void OptimizingCompileDispatcher::Stop() {
  base::Release_Store(&mode_, static_cast<base::AtomicWord>(FLUSH));
  if (FLAG_block_concurrent_recompilation) Unblock();
  {
    base::LockGuard<base::Mutex> lock_guard(&ref_count_mutex_);
    while (ref_count_ > 0) ref_count_zero_.Wait(&ref_count_mutex_);
    base::Release_Store(&mode_, static_cast<base::AtomicWord>(COMPILE));
  }

  if (recompilation_delay_ != 0) {
    // At this point the optimizing compiler thread's event loop has stopped.
    // There is no need for a mutex when reading input_queue_length_.
    while (input_queue_length_ > 0) CompileNext(NextInput());
    InstallOptimizedFunctions();
  } else {
    FlushOutputQueue(false);
  }

  if (FLAG_concurrent_osr) FlushOsrBuffer(false);

  if ((FLAG_trace_osr || FLAG_trace_concurrent_recompilation) &&
      FLAG_concurrent_osr) {
    PrintF("[COSR hit rate %d / %d]\n", osr_hits_, osr_attempts_);
  }
}


void OptimizingCompileDispatcher::InstallOptimizedFunctions() {
  HandleScope handle_scope(isolate_);

  for (;;) {
    OptimizedCompileJob* job = NULL;
    {
      base::LockGuard<base::Mutex> access_output_queue_(&output_queue_mutex_);
      if (output_queue_.empty()) return;
      job = output_queue_.front();
      output_queue_.pop();
    }
    CompilationInfo* info = job->info();
    Handle<JSFunction> function(*info->closure());
    if (info->is_osr()) {
      if (FLAG_trace_osr) {
        PrintF("[COSR - ");
        function->ShortPrint();
        PrintF(" is ready for install and entry at AST id %d]\n",
               info->osr_ast_id().ToInt());
      }
      job->WaitForInstall();
      // Remove stack check that guards OSR entry on original code.
      Handle<Code> code = info->unoptimized_code();
      uint32_t offset = code->TranslateAstIdToPcOffset(info->osr_ast_id());
      BackEdgeTable::RemoveStackCheck(code, offset);
    } else {
      if (function->IsOptimized()) {
        if (FLAG_trace_concurrent_recompilation) {
          PrintF("  ** Aborting compilation for ");
          function->ShortPrint();
          PrintF(" as it has already been optimized.\n");
        }
        DisposeOptimizedCompileJob(job, false);
      } else {
        Handle<Code> code = Compiler::GetConcurrentlyOptimizedCode(job);
        function->ReplaceCode(code.is_null() ? function->shared()->code()
                                             : *code);
      }
    }
  }
}


void OptimizingCompileDispatcher::QueueForOptimization(
    OptimizedCompileJob* job) {
  DCHECK(IsQueueAvailable());
  CompilationInfo* info = job->info();
  if (info->is_osr()) {
    osr_attempts_++;
    AddToOsrBuffer(job);
    // Add job to the front of the input queue.
    base::LockGuard<base::Mutex> access_input_queue(&input_queue_mutex_);
    DCHECK_LT(input_queue_length_, input_queue_capacity_);
    // Move shift_ back by one.
    input_queue_shift_ = InputQueueIndex(input_queue_capacity_ - 1);
    input_queue_[InputQueueIndex(0)] = job;
    input_queue_length_++;
  } else {
    // Add job to the back of the input queue.
    base::LockGuard<base::Mutex> access_input_queue(&input_queue_mutex_);
    DCHECK_LT(input_queue_length_, input_queue_capacity_);
    input_queue_[InputQueueIndex(input_queue_length_)] = job;
    input_queue_length_++;
  }
  if (FLAG_block_concurrent_recompilation) {
    blocked_jobs_++;
  } else {
    V8::GetCurrentPlatform()->CallOnBackgroundThread(
        new CompileTask(isolate_), v8::Platform::kShortRunningTask);
  }
}


void OptimizingCompileDispatcher::Unblock() {
  while (blocked_jobs_ > 0) {
    V8::GetCurrentPlatform()->CallOnBackgroundThread(
        new CompileTask(isolate_), v8::Platform::kShortRunningTask);
    blocked_jobs_--;
  }
}


OptimizedCompileJob* OptimizingCompileDispatcher::FindReadyOSRCandidate(
    Handle<JSFunction> function, BailoutId osr_ast_id) {
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    OptimizedCompileJob* current = osr_buffer_[i];
    if (current != NULL && current->IsWaitingForInstall() &&
        current->info()->HasSameOsrEntry(function, osr_ast_id)) {
      osr_hits_++;
      osr_buffer_[i] = NULL;
      return current;
    }
  }
  return NULL;
}


bool OptimizingCompileDispatcher::IsQueuedForOSR(Handle<JSFunction> function,
                                                 BailoutId osr_ast_id) {
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    OptimizedCompileJob* current = osr_buffer_[i];
    if (current != NULL &&
        current->info()->HasSameOsrEntry(function, osr_ast_id)) {
      return !current->IsWaitingForInstall();
    }
  }
  return false;
}


bool OptimizingCompileDispatcher::IsQueuedForOSR(JSFunction* function) {
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    OptimizedCompileJob* current = osr_buffer_[i];
    if (current != NULL && *current->info()->closure() == function) {
      return !current->IsWaitingForInstall();
    }
  }
  return false;
}


void OptimizingCompileDispatcher::AddToOsrBuffer(OptimizedCompileJob* job) {
  // Find the next slot that is empty or has a stale job.
  OptimizedCompileJob* stale = NULL;
  while (true) {
    stale = osr_buffer_[osr_buffer_cursor_];
    if (stale == NULL || stale->IsWaitingForInstall()) break;
    osr_buffer_cursor_ = (osr_buffer_cursor_ + 1) % osr_buffer_capacity_;
  }

  // Add to found slot and dispose the evicted job.
  if (stale != NULL) {
    DCHECK(stale->IsWaitingForInstall());
    CompilationInfo* info = stale->info();
    if (FLAG_trace_osr) {
      PrintF("[COSR - Discarded ");
      info->closure()->PrintName();
      PrintF(", AST id %d]\n", info->osr_ast_id().ToInt());
    }
    DisposeOptimizedCompileJob(stale, false);
  }
  osr_buffer_[osr_buffer_cursor_] = job;
  osr_buffer_cursor_ = (osr_buffer_cursor_ + 1) % osr_buffer_capacity_;
}
}  // namespace internal
}  // namespace v8

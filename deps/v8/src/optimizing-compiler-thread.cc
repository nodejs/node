// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/optimizing-compiler-thread.h"

#include "src/v8.h"

#include "src/base/atomicops.h"
#include "src/full-codegen.h"
#include "src/hydrogen.h"
#include "src/isolate.h"
#include "src/v8threads.h"

namespace v8 {
namespace internal {

OptimizingCompilerThread::~OptimizingCompilerThread() {
  DCHECK_EQ(0, input_queue_length_);
  DeleteArray(input_queue_);
  if (FLAG_concurrent_osr) {
#ifdef DEBUG
    for (int i = 0; i < osr_buffer_capacity_; i++) {
      CHECK_EQ(NULL, osr_buffer_[i]);
    }
#endif
    DeleteArray(osr_buffer_);
  }
}


void OptimizingCompilerThread::Run() {
#ifdef DEBUG
  { base::LockGuard<base::Mutex> lock_guard(&thread_id_mutex_);
    thread_id_ = ThreadId::Current().ToInteger();
  }
#endif
  Isolate::SetIsolateThreadLocals(isolate_, NULL);
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  base::ElapsedTimer total_timer;
  if (FLAG_trace_concurrent_recompilation) total_timer.Start();

  while (true) {
    input_queue_semaphore_.Wait();
    TimerEventScope<TimerEventRecompileConcurrent> timer(isolate_);

    if (FLAG_concurrent_recompilation_delay != 0) {
      base::OS::Sleep(FLAG_concurrent_recompilation_delay);
    }

    switch (static_cast<StopFlag>(base::Acquire_Load(&stop_thread_))) {
      case CONTINUE:
        break;
      case STOP:
        if (FLAG_trace_concurrent_recompilation) {
          time_spent_total_ = total_timer.Elapsed();
        }
        stop_semaphore_.Signal();
        return;
      case FLUSH:
        // The main thread is blocked, waiting for the stop semaphore.
        { AllowHandleDereference allow_handle_dereference;
          FlushInputQueue(true);
        }
        base::Release_Store(&stop_thread_,
                            static_cast<base::AtomicWord>(CONTINUE));
        stop_semaphore_.Signal();
        // Return to start of consumer loop.
        continue;
    }

    base::ElapsedTimer compiling_timer;
    if (FLAG_trace_concurrent_recompilation) compiling_timer.Start();

    CompileNext();

    if (FLAG_trace_concurrent_recompilation) {
      time_spent_compiling_ += compiling_timer.Elapsed();
    }
  }
}


OptimizedCompileJob* OptimizingCompilerThread::NextInput() {
  base::LockGuard<base::Mutex> access_input_queue_(&input_queue_mutex_);
  if (input_queue_length_ == 0) return NULL;
  OptimizedCompileJob* job = input_queue_[InputQueueIndex(0)];
  DCHECK_NE(NULL, job);
  input_queue_shift_ = InputQueueIndex(1);
  input_queue_length_--;
  return job;
}


void OptimizingCompilerThread::CompileNext() {
  OptimizedCompileJob* job = NextInput();
  DCHECK_NE(NULL, job);

  // The function may have already been optimized by OSR.  Simply continue.
  OptimizedCompileJob::Status status = job->OptimizeGraph();
  USE(status);   // Prevent an unused-variable error in release mode.
  DCHECK(status != OptimizedCompileJob::FAILED);

  // The function may have already been optimized by OSR.  Simply continue.
  // Use a mutex to make sure that functions marked for install
  // are always also queued.
  output_queue_.Enqueue(job);
  isolate_->stack_guard()->RequestInstallCode();
}


static void DisposeOptimizedCompileJob(OptimizedCompileJob* job,
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


void OptimizingCompilerThread::FlushInputQueue(bool restore_function_code) {
  OptimizedCompileJob* job;
  while ((job = NextInput())) {
    // This should not block, since we have one signal on the input queue
    // semaphore corresponding to each element in the input queue.
    input_queue_semaphore_.Wait();
    // OSR jobs are dealt with separately.
    if (!job->info()->is_osr()) {
      DisposeOptimizedCompileJob(job, restore_function_code);
    }
  }
}


void OptimizingCompilerThread::FlushOutputQueue(bool restore_function_code) {
  OptimizedCompileJob* job;
  while (output_queue_.Dequeue(&job)) {
    // OSR jobs are dealt with separately.
    if (!job->info()->is_osr()) {
      DisposeOptimizedCompileJob(job, restore_function_code);
    }
  }
}


void OptimizingCompilerThread::FlushOsrBuffer(bool restore_function_code) {
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    if (osr_buffer_[i] != NULL) {
      DisposeOptimizedCompileJob(osr_buffer_[i], restore_function_code);
      osr_buffer_[i] = NULL;
    }
  }
}


void OptimizingCompilerThread::Flush() {
  DCHECK(!IsOptimizerThread());
  base::Release_Store(&stop_thread_, static_cast<base::AtomicWord>(FLUSH));
  if (FLAG_block_concurrent_recompilation) Unblock();
  input_queue_semaphore_.Signal();
  stop_semaphore_.Wait();
  FlushOutputQueue(true);
  if (FLAG_concurrent_osr) FlushOsrBuffer(true);
  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Flushed concurrent recompilation queues.\n");
  }
}


void OptimizingCompilerThread::Stop() {
  DCHECK(!IsOptimizerThread());
  base::Release_Store(&stop_thread_, static_cast<base::AtomicWord>(STOP));
  if (FLAG_block_concurrent_recompilation) Unblock();
  input_queue_semaphore_.Signal();
  stop_semaphore_.Wait();

  if (FLAG_concurrent_recompilation_delay != 0) {
    // At this point the optimizing compiler thread's event loop has stopped.
    // There is no need for a mutex when reading input_queue_length_.
    while (input_queue_length_ > 0) CompileNext();
    InstallOptimizedFunctions();
  } else {
    FlushInputQueue(false);
    FlushOutputQueue(false);
  }

  if (FLAG_concurrent_osr) FlushOsrBuffer(false);

  if (FLAG_trace_concurrent_recompilation) {
    double percentage = time_spent_compiling_.PercentOf(time_spent_total_);
    PrintF("  ** Compiler thread did %.2f%% useful work\n", percentage);
  }

  if ((FLAG_trace_osr || FLAG_trace_concurrent_recompilation) &&
      FLAG_concurrent_osr) {
    PrintF("[COSR hit rate %d / %d]\n", osr_hits_, osr_attempts_);
  }

  Join();
}


void OptimizingCompilerThread::InstallOptimizedFunctions() {
  DCHECK(!IsOptimizerThread());
  HandleScope handle_scope(isolate_);

  OptimizedCompileJob* job;
  while (output_queue_.Dequeue(&job)) {
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
        function->ReplaceCode(
            code.is_null() ? function->shared()->code() : *code);
      }
    }
  }
}


void OptimizingCompilerThread::QueueForOptimization(OptimizedCompileJob* job) {
  DCHECK(IsQueueAvailable());
  DCHECK(!IsOptimizerThread());
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
    input_queue_semaphore_.Signal();
  }
}


void OptimizingCompilerThread::Unblock() {
  DCHECK(!IsOptimizerThread());
  while (blocked_jobs_ > 0) {
    input_queue_semaphore_.Signal();
    blocked_jobs_--;
  }
}


OptimizedCompileJob* OptimizingCompilerThread::FindReadyOSRCandidate(
    Handle<JSFunction> function, BailoutId osr_ast_id) {
  DCHECK(!IsOptimizerThread());
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    OptimizedCompileJob* current = osr_buffer_[i];
    if (current != NULL &&
        current->IsWaitingForInstall() &&
        current->info()->HasSameOsrEntry(function, osr_ast_id)) {
      osr_hits_++;
      osr_buffer_[i] = NULL;
      return current;
    }
  }
  return NULL;
}


bool OptimizingCompilerThread::IsQueuedForOSR(Handle<JSFunction> function,
                                              BailoutId osr_ast_id) {
  DCHECK(!IsOptimizerThread());
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    OptimizedCompileJob* current = osr_buffer_[i];
    if (current != NULL &&
        current->info()->HasSameOsrEntry(function, osr_ast_id)) {
      return !current->IsWaitingForInstall();
    }
  }
  return false;
}


bool OptimizingCompilerThread::IsQueuedForOSR(JSFunction* function) {
  DCHECK(!IsOptimizerThread());
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    OptimizedCompileJob* current = osr_buffer_[i];
    if (current != NULL && *current->info()->closure() == function) {
      return !current->IsWaitingForInstall();
    }
  }
  return false;
}


void OptimizingCompilerThread::AddToOsrBuffer(OptimizedCompileJob* job) {
  DCHECK(!IsOptimizerThread());
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


#ifdef DEBUG
bool OptimizingCompilerThread::IsOptimizerThread(Isolate* isolate) {
  return isolate->concurrent_recompilation_enabled() &&
         isolate->optimizing_compiler_thread()->IsOptimizerThread();
}


bool OptimizingCompilerThread::IsOptimizerThread() {
  base::LockGuard<base::Mutex> lock_guard(&thread_id_mutex_);
  return ThreadId::Current().ToInteger() == thread_id_;
}
#endif


} }  // namespace v8::internal

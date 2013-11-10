// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "optimizing-compiler-thread.h"

#include "v8.h"

#include "full-codegen.h"
#include "hydrogen.h"
#include "isolate.h"
#include "v8threads.h"

namespace v8 {
namespace internal {

OptimizingCompilerThread::~OptimizingCompilerThread() {
  ASSERT_EQ(0, input_queue_length_);
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
  { LockGuard<Mutex> lock_guard(&thread_id_mutex_);
    thread_id_ = ThreadId::Current().ToInteger();
  }
#endif
  Isolate::SetIsolateThreadLocals(isolate_, NULL);
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  ElapsedTimer total_timer;
  if (FLAG_trace_concurrent_recompilation) total_timer.Start();

  while (true) {
    input_queue_semaphore_.Wait();
    Logger::TimerEventScope timer(
        isolate_, Logger::TimerEventScope::v8_recompile_concurrent);

    if (FLAG_concurrent_recompilation_delay != 0) {
      OS::Sleep(FLAG_concurrent_recompilation_delay);
    }

    switch (static_cast<StopFlag>(Acquire_Load(&stop_thread_))) {
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
        Release_Store(&stop_thread_, static_cast<AtomicWord>(CONTINUE));
        stop_semaphore_.Signal();
        // Return to start of consumer loop.
        continue;
    }

    ElapsedTimer compiling_timer;
    if (FLAG_trace_concurrent_recompilation) compiling_timer.Start();

    CompileNext();

    if (FLAG_trace_concurrent_recompilation) {
      time_spent_compiling_ += compiling_timer.Elapsed();
    }
  }
}


RecompileJob* OptimizingCompilerThread::NextInput() {
  LockGuard<Mutex> access_input_queue_(&input_queue_mutex_);
  if (input_queue_length_ == 0) return NULL;
  RecompileJob* job = input_queue_[InputQueueIndex(0)];
  ASSERT_NE(NULL, job);
  input_queue_shift_ = InputQueueIndex(1);
  input_queue_length_--;
  return job;
}


void OptimizingCompilerThread::CompileNext() {
  RecompileJob* job = NextInput();
  ASSERT_NE(NULL, job);

  // The function may have already been optimized by OSR.  Simply continue.
  RecompileJob::Status status = job->OptimizeGraph();
  USE(status);   // Prevent an unused-variable error in release mode.
  ASSERT(status != RecompileJob::FAILED);

  // The function may have already been optimized by OSR.  Simply continue.
  // Use a mutex to make sure that functions marked for install
  // are always also queued.
  output_queue_.Enqueue(job);
  isolate_->stack_guard()->RequestInstallCode();
}


static void DisposeRecompileJob(RecompileJob* job,
                                bool restore_function_code) {
  // The recompile job is allocated in the CompilationInfo's zone.
  CompilationInfo* info = job->info();
  if (restore_function_code) {
    if (info->is_osr()) {
      if (!job->IsWaitingForInstall()) BackEdgeTable::RemoveStackCheck(info);
    } else {
      Handle<JSFunction> function = info->closure();
      function->ReplaceCode(function->shared()->code());
    }
  }
  delete info;
}


void OptimizingCompilerThread::FlushInputQueue(bool restore_function_code) {
  RecompileJob* job;
  while ((job = NextInput())) {
    // This should not block, since we have one signal on the input queue
    // semaphore corresponding to each element in the input queue.
    input_queue_semaphore_.Wait();
    // OSR jobs are dealt with separately.
    if (!job->info()->is_osr()) {
      DisposeRecompileJob(job, restore_function_code);
    }
  }
}


void OptimizingCompilerThread::FlushOutputQueue(bool restore_function_code) {
  RecompileJob* job;
  while (output_queue_.Dequeue(&job)) {
    // OSR jobs are dealt with separately.
    if (!job->info()->is_osr()) {
      DisposeRecompileJob(job, restore_function_code);
    }
  }
}


void OptimizingCompilerThread::FlushOsrBuffer(bool restore_function_code) {
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    if (osr_buffer_[i] != NULL) {
      DisposeRecompileJob(osr_buffer_[i], restore_function_code);
      osr_buffer_[i] = NULL;
    }
  }
}


void OptimizingCompilerThread::Flush() {
  ASSERT(!IsOptimizerThread());
  Release_Store(&stop_thread_, static_cast<AtomicWord>(FLUSH));
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
  ASSERT(!IsOptimizerThread());
  Release_Store(&stop_thread_, static_cast<AtomicWord>(STOP));
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
  ASSERT(!IsOptimizerThread());
  HandleScope handle_scope(isolate_);

  RecompileJob* job;
  while (output_queue_.Dequeue(&job)) {
    CompilationInfo* info = job->info();
    if (info->is_osr()) {
      if (FLAG_trace_osr) {
        PrintF("[COSR - ");
        info->closure()->PrintName();
        PrintF(" is ready for install and entry at AST id %d]\n",
               info->osr_ast_id().ToInt());
      }
      job->WaitForInstall();
      BackEdgeTable::RemoveStackCheck(info);
    } else {
      Compiler::InstallOptimizedCode(job);
    }
  }
}


void OptimizingCompilerThread::QueueForOptimization(RecompileJob* job) {
  ASSERT(IsQueueAvailable());
  ASSERT(!IsOptimizerThread());
  CompilationInfo* info = job->info();
  if (info->is_osr()) {
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** Queueing ");
      info->closure()->PrintName();
      PrintF(" for concurrent on-stack replacement.\n");
    }
    osr_attempts_++;
    BackEdgeTable::AddStackCheck(info);
    AddToOsrBuffer(job);
    // Add job to the front of the input queue.
    LockGuard<Mutex> access_input_queue(&input_queue_mutex_);
    ASSERT_LT(input_queue_length_, input_queue_capacity_);
    // Move shift_ back by one.
    input_queue_shift_ = InputQueueIndex(input_queue_capacity_ - 1);
    input_queue_[InputQueueIndex(0)] = job;
    input_queue_length_++;
  } else {
    info->closure()->MarkInRecompileQueue();
    // Add job to the back of the input queue.
    LockGuard<Mutex> access_input_queue(&input_queue_mutex_);
    ASSERT_LT(input_queue_length_, input_queue_capacity_);
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
  ASSERT(!IsOptimizerThread());
  while (blocked_jobs_ > 0) {
    input_queue_semaphore_.Signal();
    blocked_jobs_--;
  }
}


RecompileJob* OptimizingCompilerThread::FindReadyOSRCandidate(
    Handle<JSFunction> function, uint32_t osr_pc_offset) {
  ASSERT(!IsOptimizerThread());
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    RecompileJob* current = osr_buffer_[i];
    if (current != NULL &&
        current->IsWaitingForInstall() &&
        current->info()->HasSameOsrEntry(function, osr_pc_offset)) {
      osr_hits_++;
      osr_buffer_[i] = NULL;
      return current;
    }
  }
  return NULL;
}


bool OptimizingCompilerThread::IsQueuedForOSR(Handle<JSFunction> function,
                                              uint32_t osr_pc_offset) {
  ASSERT(!IsOptimizerThread());
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    RecompileJob* current = osr_buffer_[i];
    if (current != NULL &&
        current->info()->HasSameOsrEntry(function, osr_pc_offset)) {
      return !current->IsWaitingForInstall();
    }
  }
  return false;
}


bool OptimizingCompilerThread::IsQueuedForOSR(JSFunction* function) {
  ASSERT(!IsOptimizerThread());
  for (int i = 0; i < osr_buffer_capacity_; i++) {
    RecompileJob* current = osr_buffer_[i];
    if (current != NULL && *current->info()->closure() == function) {
      return !current->IsWaitingForInstall();
    }
  }
  return false;
}


void OptimizingCompilerThread::AddToOsrBuffer(RecompileJob* job) {
  ASSERT(!IsOptimizerThread());
  // Find the next slot that is empty or has a stale job.
  while (true) {
    RecompileJob* stale = osr_buffer_[osr_buffer_cursor_];
    if (stale == NULL || stale->IsWaitingForInstall()) break;
    osr_buffer_cursor_ = (osr_buffer_cursor_ + 1) % osr_buffer_capacity_;
  }

  // Add to found slot and dispose the evicted job.
  RecompileJob* evicted = osr_buffer_[osr_buffer_cursor_];
  if (evicted != NULL) {
    ASSERT(evicted->IsWaitingForInstall());
    CompilationInfo* info = evicted->info();
    if (FLAG_trace_osr) {
      PrintF("[COSR - Discarded ");
      info->closure()->PrintName();
      PrintF(", AST id %d]\n", info->osr_ast_id().ToInt());
    }
    DisposeRecompileJob(evicted, false);
  }
  osr_buffer_[osr_buffer_cursor_] = job;
  osr_buffer_cursor_ = (osr_buffer_cursor_ + 1) % osr_buffer_capacity_;
}


#ifdef DEBUG
bool OptimizingCompilerThread::IsOptimizerThread() {
  if (!FLAG_concurrent_recompilation) return false;
  LockGuard<Mutex> lock_guard(&thread_id_mutex_);
  return ThreadId::Current().ToInteger() == thread_id_;
}
#endif


} }  // namespace v8::internal

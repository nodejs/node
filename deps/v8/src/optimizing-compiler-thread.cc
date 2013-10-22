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

#include "hydrogen.h"
#include "isolate.h"
#include "v8threads.h"

namespace v8 {
namespace internal {


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
        Release_Store(&queue_length_, static_cast<AtomicWord>(0));
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


void OptimizingCompilerThread::CompileNext() {
  OptimizingCompiler* optimizing_compiler = NULL;
  bool result = input_queue_.Dequeue(&optimizing_compiler);
  USE(result);
  ASSERT(result);
  Barrier_AtomicIncrement(&queue_length_, static_cast<Atomic32>(-1));

  // The function may have already been optimized by OSR.  Simply continue.
  OptimizingCompiler::Status status = optimizing_compiler->OptimizeGraph();
  USE(status);   // Prevent an unused-variable error in release mode.
  ASSERT(status != OptimizingCompiler::FAILED);

  // The function may have already been optimized by OSR.  Simply continue.
  // Use a mutex to make sure that functions marked for install
  // are always also queued.
  if (!optimizing_compiler->info()->osr_ast_id().IsNone()) {
    ASSERT(FLAG_concurrent_osr);
    LockGuard<Mutex> access_osr_lists(&osr_list_mutex_);
    osr_candidates_.RemoveElement(optimizing_compiler);
    ready_for_osr_.Add(optimizing_compiler);
  } else {
    output_queue_.Enqueue(optimizing_compiler);
    isolate_->stack_guard()->RequestInstallCode();
  }
}


void OptimizingCompilerThread::FlushInputQueue(bool restore_function_code) {
  OptimizingCompiler* optimizing_compiler;
  // The optimizing compiler is allocated in the CompilationInfo's zone.
  while (input_queue_.Dequeue(&optimizing_compiler)) {
    // This should not block, since we have one signal on the input queue
    // semaphore corresponding to each element in the input queue.
    input_queue_semaphore_.Wait();
    CompilationInfo* info = optimizing_compiler->info();
    if (restore_function_code) {
      Handle<JSFunction> function = info->closure();
      function->ReplaceCode(function->shared()->code());
    }
    delete info;
  }
}


void OptimizingCompilerThread::FlushOutputQueue(bool restore_function_code) {
  OptimizingCompiler* optimizing_compiler;
  // The optimizing compiler is allocated in the CompilationInfo's zone.
  while (output_queue_.Dequeue(&optimizing_compiler)) {
    CompilationInfo* info = optimizing_compiler->info();
    if (restore_function_code) {
      Handle<JSFunction> function = info->closure();
      function->ReplaceCode(function->shared()->code());
    }
    delete info;
  }

  osr_candidates_.Clear();
  RemoveStaleOSRCandidates(0);
}


void OptimizingCompilerThread::Flush() {
  ASSERT(!IsOptimizerThread());
  Release_Store(&stop_thread_, static_cast<AtomicWord>(FLUSH));
  input_queue_semaphore_.Signal();
  stop_semaphore_.Wait();
  FlushOutputQueue(true);
}


void OptimizingCompilerThread::Stop() {
  ASSERT(!IsOptimizerThread());
  Release_Store(&stop_thread_, static_cast<AtomicWord>(STOP));
  input_queue_semaphore_.Signal();
  stop_semaphore_.Wait();

  if (FLAG_concurrent_recompilation_delay != 0) {
    // Barrier when loading queue length is not necessary since the write
    // happens in CompileNext on the same thread.
    // This is used only for testing.
    while (NoBarrier_Load(&queue_length_) > 0) CompileNext();
    InstallOptimizedFunctions();
  } else {
    FlushInputQueue(false);
    FlushOutputQueue(false);
  }

  if (FLAG_trace_concurrent_recompilation) {
    double percentage = time_spent_compiling_.PercentOf(time_spent_total_);
    PrintF("  ** Compiler thread did %.2f%% useful work\n", percentage);
  }

  if (FLAG_trace_osr && FLAG_concurrent_osr) {
    PrintF("[COSR hit rate %d / %d]\n", osr_hits_, osr_attempts_);
  }

  Join();
}


void OptimizingCompilerThread::InstallOptimizedFunctions() {
  ASSERT(!IsOptimizerThread());
  HandleScope handle_scope(isolate_);
  OptimizingCompiler* compiler;
  while (true) {
    if (!output_queue_.Dequeue(&compiler)) return;
    Compiler::InstallOptimizedCode(compiler);
  }

  // Remove the oldest OSR candidates that are ready so that we
  // only have limited number of them waiting.
  if (FLAG_concurrent_osr) RemoveStaleOSRCandidates();
}


void OptimizingCompilerThread::QueueForOptimization(
    OptimizingCompiler* optimizing_compiler) {
  ASSERT(IsQueueAvailable());
  ASSERT(!IsOptimizerThread());
  Barrier_AtomicIncrement(&queue_length_, static_cast<Atomic32>(1));
  if (optimizing_compiler->info()->osr_ast_id().IsNone()) {
    optimizing_compiler->info()->closure()->MarkInRecompileQueue();
  } else {
    LockGuard<Mutex> access_osr_lists(&osr_list_mutex_);
    osr_candidates_.Add(optimizing_compiler);
    osr_attempts_++;
  }
  input_queue_.Enqueue(optimizing_compiler);
  input_queue_semaphore_.Signal();
}


OptimizingCompiler* OptimizingCompilerThread::FindReadyOSRCandidate(
    Handle<JSFunction> function, uint32_t osr_pc_offset) {
  ASSERT(!IsOptimizerThread());
  OptimizingCompiler* result = NULL;
  { LockGuard<Mutex> access_osr_lists(&osr_list_mutex_);
    for (int i = 0; i < ready_for_osr_.length(); i++) {
      if (ready_for_osr_[i]->info()->HasSameOsrEntry(function, osr_pc_offset)) {
        osr_hits_++;
        result = ready_for_osr_.Remove(i);
        break;
      }
    }
  }
  RemoveStaleOSRCandidates();
  return result;
}


bool OptimizingCompilerThread::IsQueuedForOSR(Handle<JSFunction> function,
                                              uint32_t osr_pc_offset) {
  ASSERT(!IsOptimizerThread());
  LockGuard<Mutex> access_osr_lists(&osr_list_mutex_);
  for (int i = 0; i < osr_candidates_.length(); i++) {
    if (osr_candidates_[i]->info()->HasSameOsrEntry(function, osr_pc_offset)) {
      return true;
    }
  }
  return false;
}


bool OptimizingCompilerThread::IsQueuedForOSR(JSFunction* function) {
  ASSERT(!IsOptimizerThread());
  LockGuard<Mutex> access_osr_lists(&osr_list_mutex_);
  for (int i = 0; i < osr_candidates_.length(); i++) {
    if (*osr_candidates_[i]->info()->closure() == function) {
      return true;
    }
  }
  return false;
}


void OptimizingCompilerThread::RemoveStaleOSRCandidates(int limit) {
  ASSERT(!IsOptimizerThread());
  LockGuard<Mutex> access_osr_lists(&osr_list_mutex_);
  while (ready_for_osr_.length() > limit) {
    OptimizingCompiler* compiler = ready_for_osr_.Remove(0);
    CompilationInfo* throw_away = compiler->info();
    if (FLAG_trace_osr) {
      PrintF("[COSR - Discarded ");
      throw_away->closure()->PrintName();
      PrintF(", AST id %d]\n",
             throw_away->osr_ast_id().ToInt());
    }
    delete throw_away;
  }
}


#ifdef DEBUG
bool OptimizingCompilerThread::IsOptimizerThread() {
  if (!FLAG_concurrent_recompilation) return false;
  LockGuard<Mutex> lock_guard(&thread_id_mutex_);
  return ThreadId::Current().ToInteger() == thread_id_;
}
#endif


} }  // namespace v8::internal

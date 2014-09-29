// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OPTIMIZING_COMPILER_THREAD_H_
#define V8_OPTIMIZING_COMPILER_THREAD_H_

#include "src/base/atomicops.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/flags.h"
#include "src/list.h"
#include "src/unbound-queue-inl.h"

namespace v8 {
namespace internal {

class HOptimizedGraphBuilder;
class OptimizedCompileJob;
class SharedFunctionInfo;

class OptimizingCompilerThread : public base::Thread {
 public:
  explicit OptimizingCompilerThread(Isolate* isolate)
      : Thread(Options("OptimizingCompilerThread")),
#ifdef DEBUG
        thread_id_(0),
#endif
        isolate_(isolate),
        stop_semaphore_(0),
        input_queue_semaphore_(0),
        input_queue_capacity_(FLAG_concurrent_recompilation_queue_length),
        input_queue_length_(0),
        input_queue_shift_(0),
        osr_buffer_capacity_(FLAG_concurrent_recompilation_queue_length + 4),
        osr_buffer_cursor_(0),
        osr_hits_(0),
        osr_attempts_(0),
        blocked_jobs_(0) {
    base::NoBarrier_Store(&stop_thread_,
                          static_cast<base::AtomicWord>(CONTINUE));
    input_queue_ = NewArray<OptimizedCompileJob*>(input_queue_capacity_);
    if (FLAG_concurrent_osr) {
      // Allocate and mark OSR buffer slots as empty.
      osr_buffer_ = NewArray<OptimizedCompileJob*>(osr_buffer_capacity_);
      for (int i = 0; i < osr_buffer_capacity_; i++) osr_buffer_[i] = NULL;
    }
  }

  ~OptimizingCompilerThread();

  void Run();
  void Stop();
  void Flush();
  void QueueForOptimization(OptimizedCompileJob* optimizing_compiler);
  void Unblock();
  void InstallOptimizedFunctions();
  OptimizedCompileJob* FindReadyOSRCandidate(Handle<JSFunction> function,
                                             BailoutId osr_ast_id);
  bool IsQueuedForOSR(Handle<JSFunction> function, BailoutId osr_ast_id);

  bool IsQueuedForOSR(JSFunction* function);

  inline bool IsQueueAvailable() {
    base::LockGuard<base::Mutex> access_input_queue(&input_queue_mutex_);
    return input_queue_length_ < input_queue_capacity_;
  }

  inline void AgeBufferedOsrJobs() {
    // Advance cursor of the cyclic buffer to next empty slot or stale OSR job.
    // Dispose said OSR job in the latter case.  Calling this on every GC
    // should make sure that we do not hold onto stale jobs indefinitely.
    AddToOsrBuffer(NULL);
  }

  static bool Enabled(int max_available) {
    return (FLAG_concurrent_recompilation && max_available > 1);
  }

#ifdef DEBUG
  static bool IsOptimizerThread(Isolate* isolate);
  bool IsOptimizerThread();
#endif

 private:
  enum StopFlag { CONTINUE, STOP, FLUSH };

  void FlushInputQueue(bool restore_function_code);
  void FlushOutputQueue(bool restore_function_code);
  void FlushOsrBuffer(bool restore_function_code);
  void CompileNext();
  OptimizedCompileJob* NextInput();

  // Add a recompilation task for OSR to the cyclic buffer, awaiting OSR entry.
  // Tasks evicted from the cyclic buffer are discarded.
  void AddToOsrBuffer(OptimizedCompileJob* compiler);

  inline int InputQueueIndex(int i) {
    int result = (i + input_queue_shift_) % input_queue_capacity_;
    DCHECK_LE(0, result);
    DCHECK_LT(result, input_queue_capacity_);
    return result;
  }

#ifdef DEBUG
  int thread_id_;
  base::Mutex thread_id_mutex_;
#endif

  Isolate* isolate_;
  base::Semaphore stop_semaphore_;
  base::Semaphore input_queue_semaphore_;

  // Circular queue of incoming recompilation tasks (including OSR).
  OptimizedCompileJob** input_queue_;
  int input_queue_capacity_;
  int input_queue_length_;
  int input_queue_shift_;
  base::Mutex input_queue_mutex_;

  // Queue of recompilation tasks ready to be installed (excluding OSR).
  UnboundQueue<OptimizedCompileJob*> output_queue_;

  // Cyclic buffer of recompilation tasks for OSR.
  OptimizedCompileJob** osr_buffer_;
  int osr_buffer_capacity_;
  int osr_buffer_cursor_;

  volatile base::AtomicWord stop_thread_;
  base::TimeDelta time_spent_compiling_;
  base::TimeDelta time_spent_total_;

  int osr_hits_;
  int osr_attempts_;

  int blocked_jobs_;
};

} }  // namespace v8::internal

#endif  // V8_OPTIMIZING_COMPILER_THREAD_H_

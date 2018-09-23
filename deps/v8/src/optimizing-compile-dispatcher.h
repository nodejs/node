// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OPTIMIZING_COMPILE_DISPATCHER_H_
#define V8_OPTIMIZING_COMPILE_DISPATCHER_H_

#include <queue>

#include "src/base/atomicops.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/flags.h"
#include "src/list.h"

namespace v8 {
namespace internal {

class HOptimizedGraphBuilder;
class OptimizedCompileJob;
class SharedFunctionInfo;

class OptimizingCompileDispatcher {
 public:
  explicit OptimizingCompileDispatcher(Isolate* isolate)
      : isolate_(isolate),
        input_queue_capacity_(FLAG_concurrent_recompilation_queue_length),
        input_queue_length_(0),
        input_queue_shift_(0),
        osr_buffer_capacity_(FLAG_concurrent_recompilation_queue_length + 4),
        osr_buffer_cursor_(0),
        osr_hits_(0),
        osr_attempts_(0),
        blocked_jobs_(0),
        ref_count_(0),
        recompilation_delay_(FLAG_concurrent_recompilation_delay) {
    base::NoBarrier_Store(&mode_, static_cast<base::AtomicWord>(COMPILE));
    input_queue_ = NewArray<OptimizedCompileJob*>(input_queue_capacity_);
    if (FLAG_concurrent_osr) {
      // Allocate and mark OSR buffer slots as empty.
      osr_buffer_ = NewArray<OptimizedCompileJob*>(osr_buffer_capacity_);
      for (int i = 0; i < osr_buffer_capacity_; i++) osr_buffer_[i] = NULL;
    }
  }

  ~OptimizingCompileDispatcher();

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

  static bool Enabled() { return FLAG_concurrent_recompilation; }

 private:
  class CompileTask;

  enum ModeFlag { COMPILE, FLUSH };

  void FlushOutputQueue(bool restore_function_code);
  void FlushOsrBuffer(bool restore_function_code);
  void CompileNext(OptimizedCompileJob* job);
  OptimizedCompileJob* NextInput(bool check_if_flushing = false);

  // Add a recompilation task for OSR to the cyclic buffer, awaiting OSR entry.
  // Tasks evicted from the cyclic buffer are discarded.
  void AddToOsrBuffer(OptimizedCompileJob* compiler);

  inline int InputQueueIndex(int i) {
    int result = (i + input_queue_shift_) % input_queue_capacity_;
    DCHECK_LE(0, result);
    DCHECK_LT(result, input_queue_capacity_);
    return result;
  }

  Isolate* isolate_;

  // Circular queue of incoming recompilation tasks (including OSR).
  OptimizedCompileJob** input_queue_;
  int input_queue_capacity_;
  int input_queue_length_;
  int input_queue_shift_;
  base::Mutex input_queue_mutex_;

  // Queue of recompilation tasks ready to be installed (excluding OSR).
  std::queue<OptimizedCompileJob*> output_queue_;
  // Used for job based recompilation which has multiple producers on
  // different threads.
  base::Mutex output_queue_mutex_;

  // Cyclic buffer of recompilation tasks for OSR.
  OptimizedCompileJob** osr_buffer_;
  int osr_buffer_capacity_;
  int osr_buffer_cursor_;

  volatile base::AtomicWord mode_;

  int osr_hits_;
  int osr_attempts_;

  int blocked_jobs_;

  int ref_count_;
  base::Mutex ref_count_mutex_;
  base::ConditionVariable ref_count_zero_;

  // Copy of FLAG_concurrent_recompilation_delay that will be used from the
  // background thread.
  //
  // Since flags might get modified while the background thread is running, it
  // is not safe to access them directly.
  int recompilation_delay_;
};
}
}  // namespace v8::internal

#endif  // V8_OPTIMIZING_COMPILE_DISPATCHER_H_

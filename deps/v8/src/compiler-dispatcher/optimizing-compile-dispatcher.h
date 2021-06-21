// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DISPATCHER_OPTIMIZING_COMPILE_DISPATCHER_H_
#define V8_COMPILER_DISPATCHER_OPTIMIZING_COMPILE_DISPATCHER_H_

#include <atomic>
#include <queue>

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class LocalHeap;
class OptimizedCompilationJob;
class RuntimeCallStats;
class SharedFunctionInfo;

class V8_EXPORT_PRIVATE OptimizingCompileDispatcher {
 public:
  explicit OptimizingCompileDispatcher(Isolate* isolate)
      : isolate_(isolate),
        input_queue_capacity_(FLAG_concurrent_recompilation_queue_length),
        input_queue_length_(0),
        input_queue_shift_(0),
        blocked_jobs_(0),
        ref_count_(0),
        recompilation_delay_(FLAG_concurrent_recompilation_delay) {
    input_queue_ = NewArray<OptimizedCompilationJob*>(input_queue_capacity_);
  }

  ~OptimizingCompileDispatcher();

  void Stop();
  void Flush(BlockingBehavior blocking_behavior);
  // Takes ownership of |job|.
  void QueueForOptimization(OptimizedCompilationJob* job);
  void Unblock();
  void InstallOptimizedFunctions();

  inline bool IsQueueAvailable() {
    base::MutexGuard access_input_queue(&input_queue_mutex_);
    return input_queue_length_ < input_queue_capacity_;
  }

  static bool Enabled() { return FLAG_concurrent_recompilation; }

  // This method must be called on the main thread.
  bool HasJobs();

 private:
  class CompileTask;

  enum ModeFlag { COMPILE, FLUSH };

  void FlushQueues(BlockingBehavior blocking_behavior,
                   bool restore_function_code);
  void FlushInputQueue();
  void FlushOutputQueue(bool restore_function_code);
  void CompileNext(OptimizedCompilationJob* job, RuntimeCallStats* stats,
                   LocalIsolate* local_isolate);
  OptimizedCompilationJob* NextInput(LocalIsolate* local_isolate);

  inline int InputQueueIndex(int i) {
    int result = (i + input_queue_shift_) % input_queue_capacity_;
    DCHECK_LE(0, result);
    DCHECK_LT(result, input_queue_capacity_);
    return result;
  }

  Isolate* isolate_;

  // Circular queue of incoming recompilation tasks (including OSR).
  OptimizedCompilationJob** input_queue_;
  int input_queue_capacity_;
  int input_queue_length_;
  int input_queue_shift_;
  base::Mutex input_queue_mutex_;

  // Queue of recompilation tasks ready to be installed (excluding OSR).
  std::queue<OptimizedCompilationJob*> output_queue_;
  // Used for job based recompilation which has multiple producers on
  // different threads.
  base::Mutex output_queue_mutex_;

  int blocked_jobs_;

  std::atomic<int> ref_count_;
  base::Mutex ref_count_mutex_;
  base::ConditionVariable ref_count_zero_;

  // Copy of FLAG_concurrent_recompilation_delay that will be used from the
  // background thread.
  //
  // Since flags might get modified while the background thread is running, it
  // is not safe to access them directly.
  int recompilation_delay_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_OPTIMIZING_COMPILE_DISPATCHER_H_

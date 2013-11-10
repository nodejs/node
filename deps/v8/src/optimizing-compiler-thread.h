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

#ifndef V8_OPTIMIZING_COMPILER_THREAD_H_
#define V8_OPTIMIZING_COMPILER_THREAD_H_

#include "atomicops.h"
#include "flags.h"
#include "list.h"
#include "platform.h"
#include "platform/mutex.h"
#include "platform/time.h"
#include "unbound-queue-inl.h"

namespace v8 {
namespace internal {

class HOptimizedGraphBuilder;
class RecompileJob;
class SharedFunctionInfo;

class OptimizingCompilerThread : public Thread {
 public:
  explicit OptimizingCompilerThread(Isolate *isolate) :
      Thread("OptimizingCompilerThread"),
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
    NoBarrier_Store(&stop_thread_, static_cast<AtomicWord>(CONTINUE));
    input_queue_ = NewArray<RecompileJob*>(input_queue_capacity_);
    if (FLAG_concurrent_osr) {
      // Allocate and mark OSR buffer slots as empty.
      osr_buffer_ = NewArray<RecompileJob*>(osr_buffer_capacity_);
      for (int i = 0; i < osr_buffer_capacity_; i++) osr_buffer_[i] = NULL;
    }
  }

  ~OptimizingCompilerThread();

  void Run();
  void Stop();
  void Flush();
  void QueueForOptimization(RecompileJob* optimizing_compiler);
  void Unblock();
  void InstallOptimizedFunctions();
  RecompileJob* FindReadyOSRCandidate(Handle<JSFunction> function,
                                      uint32_t osr_pc_offset);
  bool IsQueuedForOSR(Handle<JSFunction> function, uint32_t osr_pc_offset);

  bool IsQueuedForOSR(JSFunction* function);

  inline bool IsQueueAvailable() {
    LockGuard<Mutex> access_input_queue(&input_queue_mutex_);
    return input_queue_length_ < input_queue_capacity_;
  }

  inline void AgeBufferedOsrJobs() {
    // Advance cursor of the cyclic buffer to next empty slot or stale OSR job.
    // Dispose said OSR job in the latter case.  Calling this on every GC
    // should make sure that we do not hold onto stale jobs indefinitely.
    AddToOsrBuffer(NULL);
  }

#ifdef DEBUG
  bool IsOptimizerThread();
#endif

 private:
  enum StopFlag { CONTINUE, STOP, FLUSH };

  void FlushInputQueue(bool restore_function_code);
  void FlushOutputQueue(bool restore_function_code);
  void FlushOsrBuffer(bool restore_function_code);
  void CompileNext();
  RecompileJob* NextInput();

  // Add a recompilation task for OSR to the cyclic buffer, awaiting OSR entry.
  // Tasks evicted from the cyclic buffer are discarded.
  void AddToOsrBuffer(RecompileJob* compiler);

  inline int InputQueueIndex(int i) {
    int result = (i + input_queue_shift_) % input_queue_capacity_;
    ASSERT_LE(0, result);
    ASSERT_LT(result, input_queue_capacity_);
    return result;
  }

#ifdef DEBUG
  int thread_id_;
  Mutex thread_id_mutex_;
#endif

  Isolate* isolate_;
  Semaphore stop_semaphore_;
  Semaphore input_queue_semaphore_;

  // Circular queue of incoming recompilation tasks (including OSR).
  RecompileJob** input_queue_;
  int input_queue_capacity_;
  int input_queue_length_;
  int input_queue_shift_;
  Mutex input_queue_mutex_;

  // Queue of recompilation tasks ready to be installed (excluding OSR).
  UnboundQueue<RecompileJob*> output_queue_;

  // Cyclic buffer of recompilation tasks for OSR.
  RecompileJob** osr_buffer_;
  int osr_buffer_capacity_;
  int osr_buffer_cursor_;

  volatile AtomicWord stop_thread_;
  TimeDelta time_spent_compiling_;
  TimeDelta time_spent_total_;

  int osr_hits_;
  int osr_attempts_;

  int blocked_jobs_;
};

} }  // namespace v8::internal

#endif  // V8_OPTIMIZING_COMPILER_THREAD_H_

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
class OptimizingCompiler;
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
      osr_candidates_(2),
      ready_for_osr_(2),
      osr_hits_(0),
      osr_attempts_(0) {
    NoBarrier_Store(&stop_thread_, static_cast<AtomicWord>(CONTINUE));
    NoBarrier_Store(&queue_length_, static_cast<AtomicWord>(0));
  }
  ~OptimizingCompilerThread() {}

  void Run();
  void Stop();
  void Flush();
  void QueueForOptimization(OptimizingCompiler* optimizing_compiler);
  void InstallOptimizedFunctions();
  OptimizingCompiler* FindReadyOSRCandidate(Handle<JSFunction> function,
                                            uint32_t osr_pc_offset);
  bool IsQueuedForOSR(Handle<JSFunction> function, uint32_t osr_pc_offset);

  bool IsQueuedForOSR(JSFunction* function);

  inline bool IsQueueAvailable() {
    // We don't need a barrier since we have a data dependency right
    // after.
    Atomic32 current_length = NoBarrier_Load(&queue_length_);

    // This can be queried only from the execution thread.
    ASSERT(!IsOptimizerThread());
    // Since only the execution thread increments queue_length_ and
    // only one thread can run inside an Isolate at one time, a direct
    // doesn't introduce a race -- queue_length_ may decreased in
    // meantime, but not increased.
    return (current_length < FLAG_concurrent_recompilation_queue_length);
  }

#ifdef DEBUG
  bool IsOptimizerThread();
#endif

 private:
  enum StopFlag { CONTINUE, STOP, FLUSH };

  // Remove the oldest OSR candidates that are ready so that we
  // only have |limit| left waiting.
  void RemoveStaleOSRCandidates(int limit = kReadyForOSRLimit);

  void FlushInputQueue(bool restore_function_code);
  void FlushOutputQueue(bool restore_function_code);
  void CompileNext();

#ifdef DEBUG
  int thread_id_;
  Mutex thread_id_mutex_;
#endif

  Isolate* isolate_;
  Semaphore stop_semaphore_;
  Semaphore input_queue_semaphore_;

  // Queue of incoming recompilation tasks (including OSR).
  UnboundQueue<OptimizingCompiler*> input_queue_;
  // Queue of recompilation tasks ready to be installed (excluding OSR).
  UnboundQueue<OptimizingCompiler*> output_queue_;
  // List of all OSR related recompilation tasks (both incoming and ready ones).
  List<OptimizingCompiler*> osr_candidates_;
  // List of recompilation tasks ready for OSR.
  List<OptimizingCompiler*> ready_for_osr_;

  volatile AtomicWord stop_thread_;
  volatile Atomic32 queue_length_;
  TimeDelta time_spent_compiling_;
  TimeDelta time_spent_total_;

  Mutex osr_list_mutex_;
  int osr_hits_;
  int osr_attempts_;

  static const int kReadyForOSRLimit = 4;
};

} }  // namespace v8::internal

#endif  // V8_OPTIMIZING_COMPILER_THREAD_H_

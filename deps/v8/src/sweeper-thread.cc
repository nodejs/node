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

#include "sweeper-thread.h"

#include "v8.h"

#include "isolate.h"
#include "v8threads.h"

namespace v8 {
namespace internal {

static const int kSweeperThreadStackSize = 64 * KB;

SweeperThread::SweeperThread(Isolate* isolate)
     : Thread(Thread::Options("v8:SweeperThread", kSweeperThreadStackSize)),
       isolate_(isolate),
       heap_(isolate->heap()),
       collector_(heap_->mark_compact_collector()),
       start_sweeping_semaphore_(0),
       end_sweeping_semaphore_(0),
       stop_semaphore_(0) {
  ASSERT(!FLAG_job_based_sweeping);
  NoBarrier_Store(&stop_thread_, static_cast<AtomicWord>(false));
}


void SweeperThread::Run() {
  Isolate::SetIsolateThreadLocals(isolate_, NULL);
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  while (true) {
    start_sweeping_semaphore_.Wait();

    if (Acquire_Load(&stop_thread_)) {
      stop_semaphore_.Signal();
      return;
    }

    collector_->SweepInParallel(heap_->old_data_space());
    collector_->SweepInParallel(heap_->old_pointer_space());
    end_sweeping_semaphore_.Signal();
  }
}


void SweeperThread::Stop() {
  Release_Store(&stop_thread_, static_cast<AtomicWord>(true));
  start_sweeping_semaphore_.Signal();
  stop_semaphore_.Wait();
  Join();
}


void SweeperThread::StartSweeping() {
  start_sweeping_semaphore_.Signal();
}


void SweeperThread::WaitForSweeperThread() {
  end_sweeping_semaphore_.Wait();
}


int SweeperThread::NumberOfThreads(int max_available) {
  if (!FLAG_concurrent_sweeping && !FLAG_parallel_sweeping) return 0;
  if (FLAG_sweeper_threads > 0) return FLAG_sweeper_threads;
  if (FLAG_concurrent_sweeping) return max_available - 1;
  ASSERT(FLAG_parallel_sweeping);
  return max_available;
}

} }  // namespace v8::internal

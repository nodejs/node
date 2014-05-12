// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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


bool SweeperThread::SweepingCompleted() {
  bool value = end_sweeping_semaphore_.WaitFor(TimeDelta::FromSeconds(0));
  if (value) {
    end_sweeping_semaphore_.Signal();
  }
  return value;
}


int SweeperThread::NumberOfThreads(int max_available) {
  if (!FLAG_concurrent_sweeping && !FLAG_parallel_sweeping) return 0;
  if (FLAG_sweeper_threads > 0) return FLAG_sweeper_threads;
  if (FLAG_concurrent_sweeping) return max_available - 1;
  ASSERT(FLAG_parallel_sweeping);
  return max_available;
}

} }  // namespace v8::internal

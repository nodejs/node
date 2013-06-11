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
       start_sweeping_semaphore_(OS::CreateSemaphore(0)),
       end_sweeping_semaphore_(OS::CreateSemaphore(0)),
       stop_semaphore_(OS::CreateSemaphore(0)),
       free_list_old_data_space_(heap_->paged_space(OLD_DATA_SPACE)),
       free_list_old_pointer_space_(heap_->paged_space(OLD_POINTER_SPACE)),
       private_free_list_old_data_space_(heap_->paged_space(OLD_DATA_SPACE)),
       private_free_list_old_pointer_space_(
           heap_->paged_space(OLD_POINTER_SPACE)) {
  NoBarrier_Store(&stop_thread_, static_cast<AtomicWord>(false));
}


void SweeperThread::Run() {
  Isolate::SetIsolateThreadLocals(isolate_, NULL);
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  while (true) {
    start_sweeping_semaphore_->Wait();

    if (Acquire_Load(&stop_thread_)) {
      stop_semaphore_->Signal();
      return;
    }

    collector_->SweepInParallel(heap_->old_data_space(),
                                &private_free_list_old_data_space_,
                                &free_list_old_data_space_);
    collector_->SweepInParallel(heap_->old_pointer_space(),
                                &private_free_list_old_pointer_space_,
                                &free_list_old_pointer_space_);
    end_sweeping_semaphore_->Signal();
  }
}


intptr_t SweeperThread::StealMemory(PagedSpace* space) {
  if (space->identity() == OLD_POINTER_SPACE) {
    return space->free_list()->Concatenate(&free_list_old_pointer_space_);
  } else if (space->identity() == OLD_DATA_SPACE) {
    return space->free_list()->Concatenate(&free_list_old_data_space_);
  }
  return 0;
}


void SweeperThread::Stop() {
  Release_Store(&stop_thread_, static_cast<AtomicWord>(true));
  start_sweeping_semaphore_->Signal();
  stop_semaphore_->Wait();
}


void SweeperThread::StartSweeping() {
  start_sweeping_semaphore_->Signal();
}


void SweeperThread::WaitForSweeperThread() {
  end_sweeping_semaphore_->Wait();
}
} }  // namespace v8::internal

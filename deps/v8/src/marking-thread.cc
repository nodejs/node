// Copyright 2013 the V8 project authors. All rights reserved.
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

#include "marking-thread.h"

#include "v8.h"

#include "isolate.h"
#include "v8threads.h"

namespace v8 {
namespace internal {

MarkingThread::MarkingThread(Isolate* isolate)
     : Thread("MarkingThread"),
       isolate_(isolate),
       heap_(isolate->heap()),
       start_marking_semaphore_(OS::CreateSemaphore(0)),
       end_marking_semaphore_(OS::CreateSemaphore(0)),
       stop_semaphore_(OS::CreateSemaphore(0)) {
  NoBarrier_Store(&stop_thread_, static_cast<AtomicWord>(false));
  id_ = NoBarrier_AtomicIncrement(&id_counter_, 1);
}


Atomic32 MarkingThread::id_counter_ = -1;


void MarkingThread::Run() {
  Isolate::SetIsolateThreadLocals(isolate_, NULL);
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  while (true) {
    start_marking_semaphore_->Wait();

    if (Acquire_Load(&stop_thread_)) {
      stop_semaphore_->Signal();
      return;
    }

    end_marking_semaphore_->Signal();
  }
}


void MarkingThread::Stop() {
  Release_Store(&stop_thread_, static_cast<AtomicWord>(true));
  start_marking_semaphore_->Signal();
  stop_semaphore_->Wait();
}


void MarkingThread::StartMarking() {
  start_marking_semaphore_->Signal();
}


void MarkingThread::WaitForMarkingThread() {
  end_marking_semaphore_->Wait();
}

} }  // namespace v8::internal

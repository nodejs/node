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

#include "task-queue.h"

// TODO(jochen): We should have our own version of checks.h.
#include "../checks.h"

namespace v8 {
namespace internal {

TaskQueue::TaskQueue() : process_queue_semaphore_(0), terminated_(false) {}


TaskQueue::~TaskQueue() {
  LockGuard<Mutex> guard(&lock_);
  ASSERT(terminated_);
  ASSERT(task_queue_.empty());
}


void TaskQueue::Append(Task* task) {
  LockGuard<Mutex> guard(&lock_);
  ASSERT(!terminated_);
  task_queue_.push(task);
  process_queue_semaphore_.Signal();
}


Task* TaskQueue::GetNext() {
  for (;;) {
    {
      LockGuard<Mutex> guard(&lock_);
      if (!task_queue_.empty()) {
        Task* result = task_queue_.front();
        task_queue_.pop();
        return result;
      }
      if (terminated_) {
        process_queue_semaphore_.Signal();
        return NULL;
      }
    }
    process_queue_semaphore_.Wait();
  }
}


void TaskQueue::Terminate() {
  LockGuard<Mutex> guard(&lock_);
  ASSERT(!terminated_);
  terminated_ = true;
  process_queue_semaphore_.Signal();
}

} }  // namespace v8::internal

// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

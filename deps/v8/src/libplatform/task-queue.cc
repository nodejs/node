// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/task-queue.h"

#include "src/base/logging.h"

namespace v8 {
namespace platform {

TaskQueue::TaskQueue() : process_queue_semaphore_(0), terminated_(false) {}


TaskQueue::~TaskQueue() {
  base::LockGuard<base::Mutex> guard(&lock_);
  DCHECK(terminated_);
  DCHECK(task_queue_.empty());
}


void TaskQueue::Append(Task* task) {
  base::LockGuard<base::Mutex> guard(&lock_);
  DCHECK(!terminated_);
  task_queue_.push(task);
  process_queue_semaphore_.Signal();
}


Task* TaskQueue::GetNext() {
  for (;;) {
    {
      base::LockGuard<base::Mutex> guard(&lock_);
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
  base::LockGuard<base::Mutex> guard(&lock_);
  DCHECK(!terminated_);
  terminated_ = true;
  process_queue_semaphore_.Signal();
}

} }  // namespace v8::platform

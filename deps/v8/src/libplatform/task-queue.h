// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TASK_QUEUE_H_
#define V8_LIBPLATFORM_TASK_QUEUE_H_

#include <queue>

#include "../base/macros.h"
#include "../platform/mutex.h"
#include "../platform/semaphore.h"

namespace v8 {

class Task;

namespace internal {

class TaskQueue {
 public:
  TaskQueue();
  ~TaskQueue();

  // Appends a task to the queue. The queue takes ownership of |task|.
  void Append(Task* task);

  // Returns the next task to process. Blocks if no task is available. Returns
  // NULL if the queue is terminated.
  Task* GetNext();

  // Terminate the queue.
  void Terminate();

 private:
  Mutex lock_;
  Semaphore process_queue_semaphore_;
  std::queue<Task*> task_queue_;
  bool terminated_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueue);
};

} }  // namespace v8::internal


#endif  // V8_LIBPLATFORM_TASK_QUEUE_H_

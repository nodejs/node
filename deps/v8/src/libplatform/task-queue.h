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

#ifndef V8_LIBPLATFORM_TASK_QUEUE_H_
#define V8_LIBPLATFORM_TASK_QUEUE_H_

#include <queue>

// TODO(jochen): We should have our own version of globals.h.
#include "../globals.h"
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

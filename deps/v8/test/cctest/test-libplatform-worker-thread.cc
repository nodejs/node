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

#include "v8.h"

#include "cctest.h"
#include "libplatform/task-queue.h"
#include "libplatform/worker-thread.h"
#include "test-libplatform.h"

using namespace v8::internal;


TEST(WorkerThread) {
  TaskQueue queue;
  TaskCounter task_counter;

  TestTask* task1 = new TestTask(&task_counter, true);
  TestTask* task2 = new TestTask(&task_counter, true);
  TestTask* task3 = new TestTask(&task_counter, true);
  TestTask* task4 = new TestTask(&task_counter, true);

  WorkerThread* thread1 = new WorkerThread(&queue);
  WorkerThread* thread2 = new WorkerThread(&queue);

  CHECK_EQ(4, task_counter.GetCount());

  queue.Append(task1);
  queue.Append(task2);
  queue.Append(task3);
  queue.Append(task4);

  // TaskQueue ASSERTs that it is empty in its destructor.
  queue.Terminate();

  delete thread1;
  delete thread2;

  CHECK_EQ(0, task_counter.GetCount());
}

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
#include "test-libplatform.h"

using namespace v8::internal;


TEST(TaskQueueBasic) {
  TaskCounter task_counter;

  TaskQueue queue;

  TestTask* task = new TestTask(&task_counter);
  queue.Append(task);
  CHECK_EQ(1, task_counter.GetCount());
  CHECK_EQ(task, queue.GetNext());
  delete task;
  CHECK_EQ(0, task_counter.GetCount());

  queue.Terminate();
  CHECK_EQ(NULL, queue.GetNext());
}


class ReadQueueTask : public TestTask {
 public:
  ReadQueueTask(TaskCounter* task_counter, TaskQueue* queue)
      : TestTask(task_counter, true), queue_(queue) {}
  virtual ~ReadQueueTask() {}

  virtual void Run() V8_OVERRIDE {
    TestTask::Run();
    CHECK_EQ(NULL, queue_->GetNext());
  }

 private:
  TaskQueue* queue_;

  DISALLOW_COPY_AND_ASSIGN(ReadQueueTask);
};


TEST(TaskQueueTerminateMultipleReaders) {
  TaskQueue queue;
  TaskCounter task_counter;
  ReadQueueTask* read1 = new ReadQueueTask(&task_counter, &queue);
  ReadQueueTask* read2 = new ReadQueueTask(&task_counter, &queue);

  TestWorkerThread thread1(read1);
  TestWorkerThread thread2(read2);

  thread1.Start();
  thread2.Start();

  CHECK_EQ(2, task_counter.GetCount());

  thread1.Signal();
  thread2.Signal();

  queue.Terminate();

  thread1.Join();
  thread2.Join();

  CHECK_EQ(0, task_counter.GetCount());
}

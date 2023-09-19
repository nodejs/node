// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-platform.h"
#include "src/base/platform/platform.h"
#include "src/libplatform/task-queue.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::InSequence;
using testing::IsNull;
using testing::StrictMock;

namespace v8 {
namespace platform {
namespace task_queue_unittest {

namespace {

struct MockTask : public Task {
  MOCK_METHOD(void, Run, (), (override));
};


class TaskQueueThread final : public base::Thread {
 public:
  explicit TaskQueueThread(TaskQueue* queue)
      : Thread(Options("libplatform TaskQueueThread")), queue_(queue) {}

  void Run() override { EXPECT_THAT(queue_->GetNext(), IsNull()); }

 private:
  TaskQueue* queue_;
};

}  // namespace


TEST(TaskQueueTest, Basic) {
  TaskQueue queue;
  std::unique_ptr<Task> task(new MockTask());
  Task* ptr = task.get();
  queue.Append(std::move(task));
  EXPECT_EQ(ptr, queue.GetNext().get());
  queue.Terminate();
  EXPECT_THAT(queue.GetNext(), IsNull());
}


TEST(TaskQueueTest, TerminateMultipleReaders) {
  TaskQueue queue;
  TaskQueueThread thread1(&queue);
  TaskQueueThread thread2(&queue);
  CHECK(thread1.Start());
  CHECK(thread2.Start());
  queue.Terminate();
  thread1.Join();
  thread2.Join();
}

}  // namespace task_queue_unittest
}  // namespace platform
}  // namespace v8

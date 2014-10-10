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

namespace {

struct MockTask : public Task {
  MOCK_METHOD0(Run, void());
};


class TaskQueueThread FINAL : public base::Thread {
 public:
  explicit TaskQueueThread(TaskQueue* queue)
      : Thread(Options("libplatform TaskQueueThread")), queue_(queue) {}

  virtual void Run() OVERRIDE { EXPECT_THAT(queue_->GetNext(), IsNull()); }

 private:
  TaskQueue* queue_;
};

}  // namespace


TEST(TaskQueueTest, Basic) {
  TaskQueue queue;
  MockTask task;
  queue.Append(&task);
  EXPECT_EQ(&task, queue.GetNext());
  queue.Terminate();
  EXPECT_THAT(queue.GetNext(), IsNull());
}


TEST(TaskQueueTest, TerminateMultipleReaders) {
  TaskQueue queue;
  TaskQueueThread thread1(&queue);
  TaskQueueThread thread2(&queue);
  thread1.Start();
  thread2.Start();
  queue.Terminate();
  thread1.Join();
  thread2.Join();
}

}  // namespace platform
}  // namespace v8

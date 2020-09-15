// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-platform.h"
#include "src/libplatform/task-queue.h"
#include "src/libplatform/worker-thread.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::InSequence;
using testing::IsNull;
using testing::StrictMock;

namespace v8 {
namespace platform {

namespace {

struct MockTask : public Task {
  // See issue v8:8185
  ~MockTask() /* override */ { Die(); }
  MOCK_METHOD(void, Run, (), (override));
  MOCK_METHOD(void, Die, ());
};

}  // namespace

// Needs to be in v8::platform due to BlockUntilQueueEmptyForTesting
// being private.
TEST(WorkerThreadTest, PostSingleTask) {
  TaskQueue queue;
  WorkerThread thread1(&queue);
  WorkerThread thread2(&queue);

  InSequence s;
  std::unique_ptr<StrictMock<MockTask>> task(new StrictMock<MockTask>);
  EXPECT_CALL(*task.get(), Run());
  EXPECT_CALL(*task.get(), Die());
  queue.Append(std::move(task));

  // The next call should not time out.
  queue.BlockUntilQueueEmptyForTesting();
  queue.Terminate();
}

namespace worker_thread_unittest {

TEST(WorkerThreadTest, Basic) {
  static const size_t kNumTasks = 10;

  TaskQueue queue;
  for (size_t i = 0; i < kNumTasks; ++i) {
    InSequence s;
    std::unique_ptr<StrictMock<MockTask>> task(new StrictMock<MockTask>);
    EXPECT_CALL(*task.get(), Run());
    EXPECT_CALL(*task.get(), Die());
    queue.Append(std::move(task));
  }

  WorkerThread thread1(&queue);
  WorkerThread thread2(&queue);

  // TaskQueue DCHECKS that it's empty in its destructor.
  queue.Terminate();
}

}  // namespace worker_thread_unittest
}  // namespace platform
}  // namespace v8

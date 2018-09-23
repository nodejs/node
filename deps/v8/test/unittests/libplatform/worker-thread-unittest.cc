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
  virtual ~MockTask() { Die(); }
  MOCK_METHOD0(Run, void());
  MOCK_METHOD0(Die, void());
};

}  // namespace


TEST(WorkerThreadTest, Basic) {
  static const size_t kNumTasks = 10;

  TaskQueue queue;
  for (size_t i = 0; i < kNumTasks; ++i) {
    InSequence s;
    StrictMock<MockTask>* task = new StrictMock<MockTask>;
    EXPECT_CALL(*task, Run());
    EXPECT_CALL(*task, Die());
    queue.Append(task);
  }

  WorkerThread thread1(&queue);
  WorkerThread thread2(&queue);

  // TaskQueue DCHECKS that it's empty in its destructor.
  queue.Terminate();
}

}  // namespace platform
}  // namespace v8

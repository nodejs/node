// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-platform.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::InSequence;
using testing::StrictMock;

namespace v8 {
namespace platform {

namespace {

struct MockTask : public Task {
  virtual ~MockTask() { Die(); }
  MOCK_METHOD0(Run, void());
  MOCK_METHOD0(Die, void());
};

struct MockIdleTask : public IdleTask {
  virtual ~MockIdleTask() { Die(); }
  MOCK_METHOD1(Run, void(double deadline_in_seconds));
  MOCK_METHOD0(Die, void());
};

class DefaultPlatformWithMockTime : public DefaultPlatform {
 public:
  DefaultPlatformWithMockTime() : time_(0) {}
  double MonotonicallyIncreasingTime() override { return time_; }
  void IncreaseTime(double seconds) { time_ += seconds; }

 private:
  double time_;
};

}  // namespace


TEST(DefaultPlatformTest, PumpMessageLoop) {
  InSequence s;

  int dummy;
  Isolate* isolate = reinterpret_cast<Isolate*>(&dummy);

  DefaultPlatform platform;
  EXPECT_FALSE(platform.PumpMessageLoop(isolate));

  StrictMock<MockTask>* task = new StrictMock<MockTask>;
  platform.CallOnForegroundThread(isolate, task);
  EXPECT_CALL(*task, Run());
  EXPECT_CALL(*task, Die());
  EXPECT_TRUE(platform.PumpMessageLoop(isolate));
  EXPECT_FALSE(platform.PumpMessageLoop(isolate));
}


TEST(DefaultPlatformTest, PumpMessageLoopDelayed) {
  InSequence s;

  int dummy;
  Isolate* isolate = reinterpret_cast<Isolate*>(&dummy);

  DefaultPlatformWithMockTime platform;
  EXPECT_FALSE(platform.PumpMessageLoop(isolate));

  StrictMock<MockTask>* task1 = new StrictMock<MockTask>;
  StrictMock<MockTask>* task2 = new StrictMock<MockTask>;
  platform.CallDelayedOnForegroundThread(isolate, task2, 100);
  platform.CallDelayedOnForegroundThread(isolate, task1, 10);

  EXPECT_FALSE(platform.PumpMessageLoop(isolate));

  platform.IncreaseTime(11);
  EXPECT_CALL(*task1, Run());
  EXPECT_CALL(*task1, Die());
  EXPECT_TRUE(platform.PumpMessageLoop(isolate));

  EXPECT_FALSE(platform.PumpMessageLoop(isolate));

  platform.IncreaseTime(90);
  EXPECT_CALL(*task2, Run());
  EXPECT_CALL(*task2, Die());
  EXPECT_TRUE(platform.PumpMessageLoop(isolate));
}


TEST(DefaultPlatformTest, PumpMessageLoopNoStarvation) {
  InSequence s;

  int dummy;
  Isolate* isolate = reinterpret_cast<Isolate*>(&dummy);

  DefaultPlatformWithMockTime platform;
  EXPECT_FALSE(platform.PumpMessageLoop(isolate));

  StrictMock<MockTask>* task1 = new StrictMock<MockTask>;
  StrictMock<MockTask>* task2 = new StrictMock<MockTask>;
  StrictMock<MockTask>* task3 = new StrictMock<MockTask>;
  platform.CallOnForegroundThread(isolate, task1);
  platform.CallDelayedOnForegroundThread(isolate, task2, 10);
  platform.IncreaseTime(11);

  EXPECT_CALL(*task1, Run());
  EXPECT_CALL(*task1, Die());
  EXPECT_TRUE(platform.PumpMessageLoop(isolate));

  platform.CallOnForegroundThread(isolate, task3);

  EXPECT_CALL(*task2, Run());
  EXPECT_CALL(*task2, Die());
  EXPECT_TRUE(platform.PumpMessageLoop(isolate));
  EXPECT_CALL(*task3, Run());
  EXPECT_CALL(*task3, Die());
  EXPECT_TRUE(platform.PumpMessageLoop(isolate));
}


TEST(DefaultPlatformTest, PendingDelayedTasksAreDestroyedOnShutdown) {
  InSequence s;

  int dummy;
  Isolate* isolate = reinterpret_cast<Isolate*>(&dummy);

  {
    DefaultPlatformWithMockTime platform;
    StrictMock<MockTask>* task = new StrictMock<MockTask>;
    platform.CallDelayedOnForegroundThread(isolate, task, 10);
    EXPECT_CALL(*task, Die());
  }
}

TEST(DefaultPlatformTest, RunIdleTasks) {
  InSequence s;

  int dummy;
  Isolate* isolate = reinterpret_cast<Isolate*>(&dummy);

  DefaultPlatformWithMockTime platform;

  StrictMock<MockIdleTask>* task = new StrictMock<MockIdleTask>;
  platform.CallIdleOnForegroundThread(isolate, task);
  EXPECT_CALL(*task, Run(42.0 + 23.0));
  EXPECT_CALL(*task, Die());
  platform.IncreaseTime(23.0);
  platform.RunIdleTasks(isolate, 42.0);
}

TEST(DefaultPlatformTest, PendingIdleTasksAreDestroyedOnShutdown) {
  InSequence s;

  int dummy;
  Isolate* isolate = reinterpret_cast<Isolate*>(&dummy);

  {
    DefaultPlatformWithMockTime platform;
    StrictMock<MockIdleTask>* task = new StrictMock<MockIdleTask>;
    platform.CallIdleOnForegroundThread(isolate, task);
    EXPECT_CALL(*task, Die());
  }
}

}  // namespace platform
}  // namespace v8

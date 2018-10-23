// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-platform.h"
#include "src/base/platform/semaphore.h"
#include "src/base/platform/time.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::InSequence;
using testing::StrictMock;

namespace v8 {
namespace platform {
namespace default_platform_unittest {

namespace {

struct MockTask : public Task {
  // See issue v8:8185
  ~MockTask() /* override */ { Die(); }
  MOCK_METHOD0(Run, void());
  MOCK_METHOD0(Die, void());
};

struct MockIdleTask : public IdleTask {
  // See issue v8:8185
  ~MockIdleTask() /* override */ { Die(); }
  MOCK_METHOD1(Run, void(double deadline_in_seconds));
  MOCK_METHOD0(Die, void());
};

class DefaultPlatformWithMockTime : public DefaultPlatform {
 public:
  DefaultPlatformWithMockTime()
      : DefaultPlatform(IdleTaskSupport::kEnabled, nullptr) {
    mock_time_ = 0.0;
    SetTimeFunctionForTesting([]() { return mock_time_; });
  }
  void IncreaseTime(double seconds) { mock_time_ += seconds; }

 private:
  static double mock_time_;
};

double DefaultPlatformWithMockTime::mock_time_ = 0.0;

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

TEST(DefaultPlatformTest, PumpMessageLoopWithTaskRunner) {
  InSequence s;

  int dummy;
  Isolate* isolate = reinterpret_cast<Isolate*>(&dummy);

  DefaultPlatform platform;
  std::shared_ptr<TaskRunner> taskrunner =
      platform.GetForegroundTaskRunner(isolate);
  EXPECT_FALSE(platform.PumpMessageLoop(isolate));

  StrictMock<MockTask>* task = new StrictMock<MockTask>;
  taskrunner->PostTask(std::unique_ptr<Task>(task));
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

TEST(DefaultPlatformTest, PumpMessageLoopDelayedWithTaskRunner) {
  InSequence s;

  int dummy;
  Isolate* isolate = reinterpret_cast<Isolate*>(&dummy);

  DefaultPlatformWithMockTime platform;
  std::shared_ptr<TaskRunner> taskrunner =
      platform.GetForegroundTaskRunner(isolate);
  EXPECT_FALSE(platform.PumpMessageLoop(isolate));

  StrictMock<MockTask>* task1 = new StrictMock<MockTask>;
  StrictMock<MockTask>* task2 = new StrictMock<MockTask>;
  taskrunner->PostDelayedTask(std::unique_ptr<Task>(task2), 100);
  taskrunner->PostDelayedTask(std::unique_ptr<Task>(task1), 10);

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

TEST(DefaultPlatformTest, RunIdleTasksWithTaskRunner) {
  InSequence s;

  int dummy;
  Isolate* isolate = reinterpret_cast<Isolate*>(&dummy);

  DefaultPlatformWithMockTime platform;
  std::shared_ptr<TaskRunner> taskrunner =
      platform.GetForegroundTaskRunner(isolate);

  StrictMock<MockIdleTask>* task = new StrictMock<MockIdleTask>;
  taskrunner->PostIdleTask(std::unique_ptr<IdleTask>(task));
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

namespace {

class TestBackgroundTask : public Task {
 public:
  explicit TestBackgroundTask(base::Semaphore* sem, bool* executed)
      : sem_(sem), executed_(executed) {}

  ~TestBackgroundTask() override { Die(); }
  MOCK_METHOD0(Die, void());

  void Run() override {
    *executed_ = true;
    sem_->Signal();
  }

 private:
  base::Semaphore* sem_;
  bool* executed_;
};

}  // namespace

TEST(DefaultPlatformTest, RunBackgroundTask) {
  DefaultPlatform platform;
  platform.SetThreadPoolSize(1);

  base::Semaphore sem(0);
  bool task_executed = false;
  StrictMock<TestBackgroundTask>* task =
      new StrictMock<TestBackgroundTask>(&sem, &task_executed);
  EXPECT_CALL(*task, Die());
  platform.CallOnWorkerThread(std::unique_ptr<Task>(task));
  EXPECT_TRUE(sem.WaitFor(base::TimeDelta::FromSeconds(1)));
  EXPECT_TRUE(task_executed);
}

TEST(DefaultPlatformTest, PostForegroundTaskAfterPlatformTermination) {
  std::shared_ptr<TaskRunner> foreground_taskrunner;
  {
    DefaultPlatformWithMockTime platform;

    int dummy;
    Isolate* isolate = reinterpret_cast<Isolate*>(&dummy);

    platform.SetThreadPoolSize(1);
    foreground_taskrunner = platform.GetForegroundTaskRunner(isolate);
  }
  // It should still be possible to post foreground tasks, even when the
  // platform does not exist anymore.
  StrictMock<MockTask>* task1 = new StrictMock<MockTask>;
  EXPECT_CALL(*task1, Die());
  foreground_taskrunner->PostTask(std::unique_ptr<Task>(task1));

  StrictMock<MockTask>* task2 = new StrictMock<MockTask>;
  EXPECT_CALL(*task2, Die());
  foreground_taskrunner->PostDelayedTask(std::unique_ptr<Task>(task2), 10);

  StrictMock<MockIdleTask>* task3 = new StrictMock<MockIdleTask>;
  EXPECT_CALL(*task3, Die());
  foreground_taskrunner->PostIdleTask(std::unique_ptr<IdleTask>(task3));
}

}  // namespace default_platform_unittest
}  // namespace platform
}  // namespace v8

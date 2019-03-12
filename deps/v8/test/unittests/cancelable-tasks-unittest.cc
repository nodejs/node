// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/atomicops.h"
#include "src/base/platform/platform.h"
#include "src/cancelable-task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

using ResultType = std::atomic<CancelableTaskManager::Id>;

class CancelableTaskManagerTest;

class TestTask : public Task, public Cancelable {
 public:
  enum Mode { kDoNothing, kWaitTillCancelTriggered, kCheckNotRun };

  TestTask(CancelableTaskManagerTest* test, ResultType* result, Mode mode);

  // Task override.
  void Run() final;

 private:
  ResultType* const result_;
  const Mode mode_;
  CancelableTaskManagerTest* const test_;
};

class SequentialRunner {
 public:
  explicit SequentialRunner(std::unique_ptr<TestTask> task)
      : task_(std::move(task)), task_id_(task_->id()) {}

  void Run() {
    task_->Run();
    task_.reset();
  }

  CancelableTaskManager::Id task_id() const { return task_id_; }

 private:
  std::unique_ptr<TestTask> task_;
  const CancelableTaskManager::Id task_id_;
};

class ThreadedRunner final : public base::Thread {
 public:
  explicit ThreadedRunner(std::unique_ptr<TestTask> task)
      : Thread(Options("runner thread")),
        task_(std::move(task)),
        task_id_(task_->id()) {}

  void Run() override {
    task_->Run();
    task_.reset();
  }

  CancelableTaskManager::Id task_id() const { return task_id_; }

 private:
  std::unique_ptr<TestTask> task_;
  const CancelableTaskManager::Id task_id_;
};

class CancelableTaskManagerTest : public ::testing::Test {
 public:
  CancelableTaskManager* manager() { return &manager_; }

  std::unique_ptr<TestTask> NewTask(
      ResultType* result, TestTask::Mode mode = TestTask::kDoNothing) {
    return base::make_unique<TestTask>(this, result, mode);
  }

  void CancelAndWait() {
    cancel_triggered_.store(true);
    manager_.CancelAndWait();
  }

  TryAbortResult TryAbortAll() {
    cancel_triggered_.store(true);
    return manager_.TryAbortAll();
  }

  bool cancel_triggered() const { return cancel_triggered_.load(); }

 private:
  CancelableTaskManager manager_;
  std::atomic<bool> cancel_triggered_{false};
};

TestTask::TestTask(CancelableTaskManagerTest* test, ResultType* result,
                   Mode mode)
    : Cancelable(test->manager()), result_(result), mode_(mode), test_(test) {}

void TestTask::Run() {
  if (!TryRun()) return;

  result_->store(id());

  switch (mode_) {
    case kWaitTillCancelTriggered:
      // Simple busy wait until the main thread tried to cancel.
      while (!test_->cancel_triggered()) {
      }
      break;
    case kCheckNotRun:
      // Check that we never execute {RunInternal}.
      EXPECT_TRUE(false);
      break;
    default:
      break;
  }
}

}  // namespace

TEST_F(CancelableTaskManagerTest, EmptyCancelableTaskManager) {
  CancelAndWait();
}

TEST_F(CancelableTaskManagerTest, SequentialCancelAndWait) {
  ResultType result1{0};
  SequentialRunner runner1(NewTask(&result1, TestTask::kCheckNotRun));
  EXPECT_EQ(0u, result1);
  CancelAndWait();
  EXPECT_EQ(0u, result1);
  runner1.Run();
  EXPECT_EQ(0u, result1);
}

TEST_F(CancelableTaskManagerTest, SequentialMultipleTasks) {
  ResultType result1{0};
  ResultType result2{0};
  SequentialRunner runner1(NewTask(&result1));
  SequentialRunner runner2(NewTask(&result2));
  EXPECT_EQ(1u, runner1.task_id());
  EXPECT_EQ(2u, runner2.task_id());

  EXPECT_EQ(0u, result1);
  runner1.Run();
  EXPECT_EQ(1u, result1);

  EXPECT_EQ(0u, result2);
  runner2.Run();
  EXPECT_EQ(2u, result2);

  CancelAndWait();
  EXPECT_EQ(TryAbortResult::kTaskRemoved, manager()->TryAbort(1));
  EXPECT_EQ(TryAbortResult::kTaskRemoved, manager()->TryAbort(2));
}

TEST_F(CancelableTaskManagerTest, ThreadedMultipleTasksStarted) {
  ResultType result1{0};
  ResultType result2{0};
  ThreadedRunner runner1(NewTask(&result1, TestTask::kWaitTillCancelTriggered));
  ThreadedRunner runner2(NewTask(&result2, TestTask::kWaitTillCancelTriggered));
  runner1.Start();
  runner2.Start();
  // Busy wait on result to make sure both tasks are done.
  while (result1.load() == 0 || result2.load() == 0) {
  }
  CancelAndWait();
  runner1.Join();
  runner2.Join();
  EXPECT_EQ(1u, result1);
  EXPECT_EQ(2u, result2);
}

TEST_F(CancelableTaskManagerTest, ThreadedMultipleTasksNotRun) {
  ResultType result1{0};
  ResultType result2{0};
  ThreadedRunner runner1(NewTask(&result1, TestTask::kCheckNotRun));
  ThreadedRunner runner2(NewTask(&result2, TestTask::kCheckNotRun));
  CancelAndWait();
  // Tasks are canceled, hence the runner will bail out and not update result.
  runner1.Start();
  runner2.Start();
  runner1.Join();
  runner2.Join();
  EXPECT_EQ(0u, result1);
  EXPECT_EQ(0u, result2);
}

TEST_F(CancelableTaskManagerTest, RemoveBeforeCancelAndWait) {
  ResultType result1{0};
  ThreadedRunner runner1(NewTask(&result1, TestTask::kCheckNotRun));
  CancelableTaskManager::Id id = runner1.task_id();
  EXPECT_EQ(1u, id);
  EXPECT_EQ(TryAbortResult::kTaskAborted, manager()->TryAbort(id));
  runner1.Start();
  runner1.Join();
  CancelAndWait();
  EXPECT_EQ(0u, result1);
}

TEST_F(CancelableTaskManagerTest, RemoveAfterCancelAndWait) {
  ResultType result1{0};
  ThreadedRunner runner1(NewTask(&result1));
  CancelableTaskManager::Id id = runner1.task_id();
  EXPECT_EQ(1u, id);
  runner1.Start();
  runner1.Join();
  CancelAndWait();
  EXPECT_EQ(TryAbortResult::kTaskRemoved, manager()->TryAbort(id));
  EXPECT_EQ(1u, result1);
}

TEST_F(CancelableTaskManagerTest, RemoveUnmanagedId) {
  EXPECT_EQ(TryAbortResult::kTaskRemoved, manager()->TryAbort(1));
  EXPECT_EQ(TryAbortResult::kTaskRemoved, manager()->TryAbort(2));
  CancelAndWait();
  EXPECT_EQ(TryAbortResult::kTaskRemoved, manager()->TryAbort(1));
  EXPECT_EQ(TryAbortResult::kTaskRemoved, manager()->TryAbort(3));
}

TEST_F(CancelableTaskManagerTest, EmptyTryAbortAll) {
  EXPECT_EQ(TryAbortResult::kTaskRemoved, TryAbortAll());
  CancelAndWait();
}

TEST_F(CancelableTaskManagerTest, ThreadedMultipleTasksNotRunTryAbortAll) {
  ResultType result1{0};
  ResultType result2{0};
  ThreadedRunner runner1(NewTask(&result1, TestTask::kCheckNotRun));
  ThreadedRunner runner2(NewTask(&result2, TestTask::kCheckNotRun));
  EXPECT_EQ(TryAbortResult::kTaskAborted, TryAbortAll());
  // Tasks are canceled, hence the runner will bail out and not update result.
  runner1.Start();
  runner2.Start();
  runner1.Join();
  runner2.Join();
  EXPECT_EQ(0u, result1);
  EXPECT_EQ(0u, result2);
  CancelAndWait();
}

TEST_F(CancelableTaskManagerTest, ThreadedMultipleTasksStartedTryAbortAll) {
  ResultType result1{0};
  ResultType result2{0};
  ThreadedRunner runner1(NewTask(&result1, TestTask::kWaitTillCancelTriggered));
  ThreadedRunner runner2(NewTask(&result2, TestTask::kWaitTillCancelTriggered));
  runner1.Start();
  // Busy wait on result to make sure task1 is done.
  while (result1.load() == 0) {
  }
  // If the task saw that we triggered the cancel and finished *before* the
  // actual cancel happened, we get {kTaskAborted}. Otherwise, we get
  // {kTaskRunning}.
  EXPECT_THAT(TryAbortAll(),
              testing::AnyOf(testing::Eq(TryAbortResult::kTaskAborted),
                             testing::Eq(TryAbortResult::kTaskRunning)));
  runner2.Start();
  runner1.Join();
  runner2.Join();
  EXPECT_EQ(1u, result1);
  EXPECT_EQ(0u, result2);
  CancelAndWait();
}

}  // namespace internal
}  // namespace v8

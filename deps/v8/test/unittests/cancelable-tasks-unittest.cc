// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/atomicops.h"
#include "src/base/platform/platform.h"
#include "src/cancelable-task.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace v8 {
namespace internal {

namespace {

class TestTask : public Task, public Cancelable {
 public:
  enum Mode { kDoNothing, kWaitTillCanceledAgain, kCheckNotRun };

  TestTask(CancelableTaskManager* parent, base::AtomicWord* result,
           Mode mode = kDoNothing)
      : Cancelable(parent), result_(result), mode_(mode) {}

  // Task overrides.
  void Run() final {
    if (TryRun()) {
      RunInternal();
    }
  }

 private:
  void RunInternal() {
    base::Release_Store(result_, id());

    switch (mode_) {
      case kWaitTillCanceledAgain:
        // Simple busy wait until the main thread tried to cancel.
        while (CancelAttempts() == 0) {
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

  base::AtomicWord* result_;
  Mode mode_;
};


class SequentialRunner {
 public:
  explicit SequentialRunner(TestTask* task) : task_(task) {}

  void Run() {
    task_->Run();
    delete task_;
  }

 private:
  TestTask* task_;
};


class ThreadedRunner final : public base::Thread {
 public:
  explicit ThreadedRunner(TestTask* task)
      : Thread(Options("runner thread")), task_(task) {}

  void Run() override {
    task_->Run();
    delete task_;
  }

 private:
  TestTask* task_;
};


typedef base::AtomicWord ResultType;


intptr_t GetValue(ResultType* result) { return base::Acquire_Load(result); }

}  // namespace


TEST(CancelableTask, EmptyCancelableTaskManager) {
  CancelableTaskManager manager;
  manager.CancelAndWait();
}


TEST(CancelableTask, SequentialCancelAndWait) {
  CancelableTaskManager manager;
  ResultType result1 = 0;
  SequentialRunner runner1(
      new TestTask(&manager, &result1, TestTask::kCheckNotRun));
  EXPECT_EQ(GetValue(&result1), 0);
  manager.CancelAndWait();
  EXPECT_EQ(GetValue(&result1), 0);
  runner1.Run();  // Run to avoid leaking the Task.
  EXPECT_EQ(GetValue(&result1), 0);
}


TEST(CancelableTask, SequentialMultipleTasks) {
  CancelableTaskManager manager;
  ResultType result1 = 0;
  ResultType result2 = 0;
  TestTask* task1 = new TestTask(&manager, &result1);
  TestTask* task2 = new TestTask(&manager, &result2);
  SequentialRunner runner1(task1);
  SequentialRunner runner2(task2);
  EXPECT_EQ(task1->id(), 1u);
  EXPECT_EQ(task2->id(), 2u);

  EXPECT_EQ(GetValue(&result1), 0);
  runner1.Run();  // Don't touch task1 after running it.
  EXPECT_EQ(GetValue(&result1), 1);

  EXPECT_EQ(GetValue(&result2), 0);
  runner2.Run();  // Don't touch task2 after running it.
  EXPECT_EQ(GetValue(&result2), 2);

  manager.CancelAndWait();
  EXPECT_FALSE(manager.TryAbort(1));
  EXPECT_FALSE(manager.TryAbort(2));
}


TEST(CancelableTask, ThreadedMultipleTasksStarted) {
  CancelableTaskManager manager;
  ResultType result1 = 0;
  ResultType result2 = 0;
  TestTask* task1 =
      new TestTask(&manager, &result1, TestTask::kWaitTillCanceledAgain);
  TestTask* task2 =
      new TestTask(&manager, &result2, TestTask::kWaitTillCanceledAgain);
  ThreadedRunner runner1(task1);
  ThreadedRunner runner2(task2);
  runner1.Start();
  runner2.Start();
  // Busy wait on result to make sure both tasks are done.
  while ((GetValue(&result1) == 0) || (GetValue(&result2) == 0)) {
  }
  manager.CancelAndWait();
  runner1.Join();
  runner2.Join();
  EXPECT_EQ(GetValue(&result1), 1);
  EXPECT_EQ(GetValue(&result2), 2);
}


TEST(CancelableTask, ThreadedMultipleTasksNotRun) {
  CancelableTaskManager manager;
  ResultType result1 = 0;
  ResultType result2 = 0;
  TestTask* task1 = new TestTask(&manager, &result1, TestTask::kCheckNotRun);
  TestTask* task2 = new TestTask(&manager, &result2, TestTask::kCheckNotRun);
  ThreadedRunner runner1(task1);
  ThreadedRunner runner2(task2);
  manager.CancelAndWait();
  // Tasks are canceled, hence the runner will bail out and not update result.
  runner1.Start();
  runner2.Start();
  runner1.Join();
  runner2.Join();
  EXPECT_EQ(GetValue(&result1), 0);
  EXPECT_EQ(GetValue(&result2), 0);
}


TEST(CancelableTask, RemoveBeforeCancelAndWait) {
  CancelableTaskManager manager;
  ResultType result1 = 0;
  TestTask* task1 = new TestTask(&manager, &result1, TestTask::kCheckNotRun);
  ThreadedRunner runner1(task1);
  CancelableTaskManager::Id id = task1->id();
  EXPECT_EQ(id, 1u);
  EXPECT_TRUE(manager.TryAbort(id));
  runner1.Start();
  runner1.Join();
  manager.CancelAndWait();
  EXPECT_EQ(GetValue(&result1), 0);
}


TEST(CancelableTask, RemoveAfterCancelAndWait) {
  CancelableTaskManager manager;
  ResultType result1 = 0;
  TestTask* task1 = new TestTask(&manager, &result1);
  ThreadedRunner runner1(task1);
  CancelableTaskManager::Id id = task1->id();
  EXPECT_EQ(id, 1u);
  runner1.Start();
  runner1.Join();
  manager.CancelAndWait();
  EXPECT_FALSE(manager.TryAbort(id));
  EXPECT_EQ(GetValue(&result1), 1);
}


TEST(CancelableTask, RemoveUnmanagedId) {
  CancelableTaskManager manager;
  EXPECT_FALSE(manager.TryAbort(1));
  EXPECT_FALSE(manager.TryAbort(2));
  manager.CancelAndWait();
  EXPECT_FALSE(manager.TryAbort(1));
  EXPECT_FALSE(manager.TryAbort(3));
}

TEST(CancelableTask, EmptyTryAbortAll) {
  CancelableTaskManager manager;
  EXPECT_EQ(manager.TryAbortAll(), CancelableTaskManager::kTaskRemoved);
}

TEST(CancelableTask, ThreadedMultipleTasksNotRunTryAbortAll) {
  CancelableTaskManager manager;
  ResultType result1 = 0;
  ResultType result2 = 0;
  TestTask* task1 = new TestTask(&manager, &result1, TestTask::kCheckNotRun);
  TestTask* task2 = new TestTask(&manager, &result2, TestTask::kCheckNotRun);
  ThreadedRunner runner1(task1);
  ThreadedRunner runner2(task2);
  EXPECT_EQ(manager.TryAbortAll(), CancelableTaskManager::kTaskAborted);
  // Tasks are canceled, hence the runner will bail out and not update result.
  runner1.Start();
  runner2.Start();
  runner1.Join();
  runner2.Join();
  EXPECT_EQ(GetValue(&result1), 0);
  EXPECT_EQ(GetValue(&result2), 0);
}

TEST(CancelableTask, ThreadedMultipleTasksStartedTryAbortAll) {
  CancelableTaskManager manager;
  ResultType result1 = 0;
  ResultType result2 = 0;
  TestTask* task1 =
      new TestTask(&manager, &result1, TestTask::kWaitTillCanceledAgain);
  TestTask* task2 =
      new TestTask(&manager, &result2, TestTask::kWaitTillCanceledAgain);
  ThreadedRunner runner1(task1);
  ThreadedRunner runner2(task2);
  runner1.Start();
  // Busy wait on result to make sure task1 is done.
  while (GetValue(&result1) == 0) {
  }
  EXPECT_EQ(manager.TryAbortAll(), CancelableTaskManager::kTaskRunning);
  runner2.Start();
  runner1.Join();
  runner2.Join();
  EXPECT_EQ(GetValue(&result1), 1);
  EXPECT_EQ(GetValue(&result2), 0);
}

}  // namespace internal
}  // namespace v8

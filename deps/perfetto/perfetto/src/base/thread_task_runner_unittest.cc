/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/ext/base/thread_task_runner.h"

#include <thread>

#include "perfetto/ext/base/thread_checker.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace base {
namespace {

class ThreadTaskRunnerTest : public ::testing::Test {
 protected:
  std::atomic<bool> atomic_flag_{false};
};

TEST_F(ThreadTaskRunnerTest, ConstructedRunning) {
  ThreadTaskRunner task_runner = ThreadTaskRunner::CreateAndStart();
  task_runner.get()->PostTask([this] { atomic_flag_ = true; });
  // main thread not blocked, wait on the task explicitly
  while (!atomic_flag_) {
    std::this_thread::yield();
  }
}

TEST_F(ThreadTaskRunnerTest, RunsTasksOnOneDedicatedThread) {
  ThreadTaskRunner task_runner = ThreadTaskRunner::CreateAndStart();
  EXPECT_FALSE(task_runner.get()->RunsTasksOnCurrentThread());

  ThreadChecker thread_checker;
  task_runner.get()->PostTask([&thread_checker] {
    // make thread_checker track the task thread
    thread_checker.DetachFromThread();
    EXPECT_TRUE(thread_checker.CalledOnValidThread());
  });
  task_runner.get()->PostTask([this, &thread_checker] {
    // called on the same thread
    EXPECT_TRUE(thread_checker.CalledOnValidThread());
    atomic_flag_ = true;
  });

  while (!atomic_flag_) {
    std::this_thread::yield();
  }
}

TEST_F(ThreadTaskRunnerTest, MovableOwnership) {
  ThreadTaskRunner task_runner = ThreadTaskRunner::CreateAndStart();
  UnixTaskRunner* runner_ptr = task_runner.get();
  EXPECT_NE(runner_ptr, nullptr);

  ThreadChecker thread_checker;
  task_runner.get()->PostTask([&thread_checker] {
    // make thread_checker track the task thread
    thread_checker.DetachFromThread();
    EXPECT_TRUE(thread_checker.CalledOnValidThread());
  });

  // move ownership and destroy old instance
  ThreadTaskRunner task_runner2 = std::move(task_runner);
  EXPECT_EQ(task_runner.get(), nullptr);
  task_runner.~ThreadTaskRunner();

  // runner pointer is stable, and remains usable
  EXPECT_EQ(task_runner2.get(), runner_ptr);
  task_runner2.get()->PostTask([this, &thread_checker] {
    // task thread remains the same
    EXPECT_TRUE(thread_checker.CalledOnValidThread());
    atomic_flag_ = true;
  });

  while (!atomic_flag_) {
    std::this_thread::yield();
  }
}

// Test helper callable that remembers a copy of a given ThreadChecker, and
// checks that this class' destructor runs on the remembered thread. Note that
// it is copyable so that it can be passed as a task (i.e. std::function) to a
// task runner. Also note that all instances of this class will thread-check,
// including those that have been moved-from.
class DestructorThreadChecker {
 public:
  DestructorThreadChecker(ThreadChecker checker) : checker_(checker) {}
  DestructorThreadChecker(const DestructorThreadChecker&) = default;
  DestructorThreadChecker& operator=(const DestructorThreadChecker&) = default;
  DestructorThreadChecker(DestructorThreadChecker&&) = default;
  DestructorThreadChecker& operator=(DestructorThreadChecker&&) = default;

  ~DestructorThreadChecker() { EXPECT_TRUE(checker_.CalledOnValidThread()); }

  void operator()() { GTEST_FAIL() << "shouldn't be called"; }

  ThreadChecker checker_;
};

// Checks that the still-pending tasks (and therefore the UnixTaskRunner itself)
// are destructed on the task thread, and not the thread that destroys the
// ThreadTaskRunner.
TEST_F(ThreadTaskRunnerTest, EnqueuedTasksDestructedOnTaskThread) {
  ThreadChecker thread_checker;
  ThreadTaskRunner task_runner = ThreadTaskRunner::CreateAndStart();

  task_runner.get()->PostTask([this, &thread_checker, &task_runner] {
    // make thread_checker track the task thread
    thread_checker.DetachFromThread();
    EXPECT_TRUE(thread_checker.CalledOnValidThread());
    // Post a follow-up delayed task and unblock the main thread, which will
    // destroy the ThreadTaskRunner while this task is still pending.
    //
    // Note: DestructorThreadChecker will thread-check (at least) twice:
    // * for the temporary that was moved-from to construct the task
    //   std::function. Will pass as we're posting from a task thread.
    // * for the still-pending task once the ThreadTaskRunner destruction causes
    //   the destruction of UnixTaskRunner.
    task_runner.get()->PostDelayedTask(DestructorThreadChecker(thread_checker),
                                       100 * 1000 /*ms*/);
    atomic_flag_ = true;
  });

  while (!atomic_flag_) {
    std::this_thread::yield();
  }
}

}  // namespace
}  // namespace base
}  // namespace perfetto

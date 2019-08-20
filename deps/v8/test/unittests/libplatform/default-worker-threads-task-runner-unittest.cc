// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-worker-threads-task-runner.h"

#include <vector>

#include "include/v8-platform.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/semaphore.h"
#include "src/base/platform/time.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace platform {

class TestTask : public v8::Task {
 public:
  explicit TestTask(std::function<void()> f) : f_(std::move(f)) {}

  void Run() override { f_(); }

 private:
  std::function<void()> f_;
};

double RealTime() {
  return base::TimeTicks::HighResolutionNow().ToInternalValue() /
         static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

TEST(DefaultWorkerThreadsTaskRunnerUnittest, PostTaskOrder) {
  DefaultWorkerThreadsTaskRunner runner(1, RealTime);

  std::vector<int> order;
  base::Semaphore semaphore(0);

  std::unique_ptr<TestTask> task1 =
      base::make_unique<TestTask>([&] { order.push_back(1); });
  std::unique_ptr<TestTask> task2 =
      base::make_unique<TestTask>([&] { order.push_back(2); });
  std::unique_ptr<TestTask> task3 = base::make_unique<TestTask>([&] {
    order.push_back(3);
    semaphore.Signal();
  });

  runner.PostTask(std::move(task1));
  runner.PostTask(std::move(task2));
  runner.PostTask(std::move(task3));

  semaphore.Wait();

  runner.Terminate();
  ASSERT_EQ(3UL, order.size());
  ASSERT_EQ(1, order[0]);
  ASSERT_EQ(2, order[1]);
  ASSERT_EQ(3, order[2]);
}

TEST(DefaultWorkerThreadsTaskRunnerUnittest, PostTaskOrderMultipleWorkers) {
  DefaultWorkerThreadsTaskRunner runner(4, RealTime);

  base::Mutex vector_lock;
  std::vector<int> order;
  std::atomic_int count{0};

  std::unique_ptr<TestTask> task1 = base::make_unique<TestTask>([&] {
    base::MutexGuard guard(&vector_lock);
    order.push_back(1);
    count++;
  });
  std::unique_ptr<TestTask> task2 = base::make_unique<TestTask>([&] {
    base::MutexGuard guard(&vector_lock);
    order.push_back(2);
    count++;
  });
  std::unique_ptr<TestTask> task3 = base::make_unique<TestTask>([&] {
    base::MutexGuard guard(&vector_lock);
    order.push_back(3);
    count++;
  });
  std::unique_ptr<TestTask> task4 = base::make_unique<TestTask>([&] {
    base::MutexGuard guard(&vector_lock);
    order.push_back(4);
    count++;
  });
  std::unique_ptr<TestTask> task5 = base::make_unique<TestTask>([&] {
    base::MutexGuard guard(&vector_lock);
    order.push_back(5);
    count++;
  });

  runner.PostTask(std::move(task1));
  runner.PostTask(std::move(task2));
  runner.PostTask(std::move(task3));
  runner.PostTask(std::move(task4));
  runner.PostTask(std::move(task5));

  // We can't observe any ordering when there are multiple worker threads. The
  // tasks are guaranteed to be dispatched to workers in the input order, but
  // the workers are different threads and can be scheduled arbitrarily. Just
  // check that all of the tasks were run once.
  while (count != 5) {
  }

  runner.Terminate();
  ASSERT_EQ(5UL, order.size());
  ASSERT_EQ(1, std::count(order.begin(), order.end(), 1));
  ASSERT_EQ(1, std::count(order.begin(), order.end(), 2));
  ASSERT_EQ(1, std::count(order.begin(), order.end(), 3));
  ASSERT_EQ(1, std::count(order.begin(), order.end(), 4));
  ASSERT_EQ(1, std::count(order.begin(), order.end(), 5));
}

class FakeClock {
 public:
  static double time() { return time_.load(); }
  static void set_time(double time) { time_.store(time); }
  static void set_time_and_wake_up_runner(
      double time, DefaultWorkerThreadsTaskRunner* runner) {
    time_.store(time);
    // PostTask will cause the condition variable WaitFor() call to be notified
    // early, rather than waiting for the real amount of time. WaitFor() listens
    // to the system clock and not our FakeClock.
    runner->PostTask(base::make_unique<TestTask>([] {}));
  }

 private:
  static std::atomic<double> time_;
};

std::atomic<double> FakeClock::time_{0.0};

TEST(DefaultWorkerThreadsTaskRunnerUnittest, PostDelayedTaskOrder) {
  FakeClock::set_time(0.0);
  DefaultWorkerThreadsTaskRunner runner(1, FakeClock::time);

  std::vector<int> order;
  base::Semaphore task1_semaphore(0);
  base::Semaphore task3_semaphore(0);

  std::unique_ptr<TestTask> task1 = base::make_unique<TestTask>([&] {
    order.push_back(1);
    task1_semaphore.Signal();
  });
  std::unique_ptr<TestTask> task2 =
      base::make_unique<TestTask>([&] { order.push_back(2); });
  std::unique_ptr<TestTask> task3 = base::make_unique<TestTask>([&] {
    order.push_back(3);
    task3_semaphore.Signal();
  });

  runner.PostDelayedTask(std::move(task1), 100);
  runner.PostTask(std::move(task2));
  runner.PostTask(std::move(task3));

  FakeClock::set_time_and_wake_up_runner(99, &runner);

  task3_semaphore.Wait();
  ASSERT_EQ(2UL, order.size());
  ASSERT_EQ(2, order[0]);
  ASSERT_EQ(3, order[1]);

  FakeClock::set_time_and_wake_up_runner(101, &runner);
  task1_semaphore.Wait();

  runner.Terminate();
  ASSERT_EQ(3UL, order.size());
  ASSERT_EQ(2, order[0]);
  ASSERT_EQ(3, order[1]);
  ASSERT_EQ(1, order[2]);
}

TEST(DefaultWorkerThreadsTaskRunnerUnittest, PostDelayedTaskOrder2) {
  FakeClock::set_time(0.0);
  DefaultWorkerThreadsTaskRunner runner(1, FakeClock::time);

  std::vector<int> order;
  base::Semaphore task1_semaphore(0);
  base::Semaphore task2_semaphore(0);
  base::Semaphore task3_semaphore(0);

  std::unique_ptr<TestTask> task1 = base::make_unique<TestTask>([&] {
    order.push_back(1);
    task1_semaphore.Signal();
  });
  std::unique_ptr<TestTask> task2 = base::make_unique<TestTask>([&] {
    order.push_back(2);
    task2_semaphore.Signal();
  });
  std::unique_ptr<TestTask> task3 = base::make_unique<TestTask>([&] {
    order.push_back(3);
    task3_semaphore.Signal();
  });

  runner.PostDelayedTask(std::move(task1), 500);
  runner.PostDelayedTask(std::move(task2), 100);
  runner.PostDelayedTask(std::move(task3), 200);

  FakeClock::set_time_and_wake_up_runner(101, &runner);

  task2_semaphore.Wait();
  ASSERT_EQ(1UL, order.size());
  ASSERT_EQ(2, order[0]);

  FakeClock::set_time_and_wake_up_runner(201, &runner);

  task3_semaphore.Wait();
  ASSERT_EQ(2UL, order.size());
  ASSERT_EQ(2, order[0]);
  ASSERT_EQ(3, order[1]);

  FakeClock::set_time_and_wake_up_runner(501, &runner);

  task1_semaphore.Wait();
  runner.Terminate();
  ASSERT_EQ(3UL, order.size());
  ASSERT_EQ(2, order[0]);
  ASSERT_EQ(3, order[1]);
  ASSERT_EQ(1, order[2]);
}

TEST(DefaultWorkerThreadsTaskRunnerUnittest, PostAfterTerminate) {
  FakeClock::set_time(0.0);
  DefaultWorkerThreadsTaskRunner runner(1, FakeClock::time);

  std::vector<int> order;
  base::Semaphore task1_semaphore(0);
  base::Semaphore task2_semaphore(0);
  base::Semaphore task3_semaphore(0);

  std::unique_ptr<TestTask> task1 = base::make_unique<TestTask>([&] {
    order.push_back(1);
    task1_semaphore.Signal();
  });
  std::unique_ptr<TestTask> task2 = base::make_unique<TestTask>([&] {
    order.push_back(2);
    task2_semaphore.Signal();
  });
  std::unique_ptr<TestTask> task3 = base::make_unique<TestTask>([&] {
    order.push_back(3);
    task3_semaphore.Signal();
  });

  runner.PostTask(std::move(task1));
  runner.PostDelayedTask(std::move(task2), 100);

  task1_semaphore.Wait();
  ASSERT_EQ(1UL, order.size());
  ASSERT_EQ(1, order[0]);

  runner.Terminate();
  FakeClock::set_time_and_wake_up_runner(201, &runner);
  // OK, we can't actually prove that this never executes. But wait a bit at
  // least.
  bool signalled =
      task2_semaphore.WaitFor(base::TimeDelta::FromMilliseconds(100));
  ASSERT_FALSE(signalled);
  ASSERT_EQ(1UL, order.size());
  ASSERT_EQ(1, order[0]);

  runner.PostTask(std::move(task3));
  signalled = task3_semaphore.WaitFor(base::TimeDelta::FromMilliseconds(100));
  ASSERT_FALSE(signalled);
  ASSERT_EQ(1UL, order.size());
  ASSERT_EQ(1, order[0]);
}

TEST(DefaultWorkerThreadsTaskRunnerUnittest, NoIdleTasks) {
  DefaultWorkerThreadsTaskRunner runner(1, FakeClock::time);

  ASSERT_FALSE(runner.IdleTasksEnabled());
  runner.Terminate();
}

TEST(DefaultWorkerThreadsTaskRunnerUnittest, RunsTasksOnCurrentThread) {
  DefaultWorkerThreadsTaskRunner runner(1, RealTime);

  base::Semaphore semaphore(0);

  EXPECT_FALSE(runner.RunsTasksOnCurrentThread());

  std::unique_ptr<TestTask> task1 = base::make_unique<TestTask>([&] {
    EXPECT_TRUE(runner.RunsTasksOnCurrentThread());
    semaphore.Signal();
  });
  runner.PostTask(std::move(task1));

  semaphore.Wait();
  EXPECT_FALSE(runner.RunsTasksOnCurrentThread());

  runner.Terminate();
  EXPECT_FALSE(runner.RunsTasksOnCurrentThread());
}

}  // namespace platform
}  // namespace v8

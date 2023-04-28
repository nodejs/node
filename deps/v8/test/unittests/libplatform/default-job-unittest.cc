// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-job.h"

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/platform.h"
#include "src/libplatform/default-platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace platform {
namespace default_job_unittest {

// Verify that Cancel() on a job stops running the worker task and causes
// current workers to yield.
TEST(DefaultJobTest, CancelJob) {
  static constexpr size_t kTooManyTasks = 1000;
  static constexpr size_t kMaxTask = 4;
  DefaultPlatform platform(kMaxTask);

  // This Job notifies |threads_running| once started and loops until
  // ShouldYield() returns true, and then returns.
  class JobTest : public JobTask {
   public:
    ~JobTest() override = default;

    void Run(JobDelegate* delegate) override {
      {
        base::MutexGuard guard(&mutex);
        worker_count++;
      }
      threads_running.NotifyOne();
      while (!delegate->ShouldYield()) {
      }
    }

    size_t GetMaxConcurrency(size_t /* worker_count */) const override {
      return max_concurrency.load(std::memory_order_relaxed);
    }

    base::Mutex mutex;
    base::ConditionVariable threads_running;
    size_t worker_count = 0;
    std::atomic_size_t max_concurrency{kTooManyTasks};
  };

  auto job = std::make_unique<JobTest>();
  JobTest* job_raw = job.get();
  auto state = std::make_shared<DefaultJobState>(
      &platform, std::move(job), TaskPriority::kUserVisible, kMaxTask);
  state->NotifyConcurrencyIncrease();

  {
    base::MutexGuard guard(&job_raw->mutex);
    while (job_raw->worker_count < kMaxTask) {
      job_raw->threads_running.Wait(&job_raw->mutex);
    }
    EXPECT_EQ(kMaxTask, job_raw->worker_count);
  }
  state->CancelAndWait();
  // Workers should return and this test should not hang.
}

// Verify that Join() on a job contributes to max concurrency and waits for all
// workers to return.
TEST(DefaultJobTest, JoinJobContributes) {
  static constexpr size_t kMaxTask = 4;
  DefaultPlatform platform(kMaxTask);

  // This Job notifies |threads_running| once started and blocks on a barrier
  // until kMaxTask + 1 threads reach that point, and then returns.
  class JobTest : public JobTask {
   public:
    ~JobTest() override = default;

    void Run(JobDelegate* delegate) override {
      base::MutexGuard guard(&mutex);
      worker_count++;
      threads_running.NotifyAll();
      while (worker_count < kMaxTask + 1) threads_running.Wait(&mutex);
      --max_concurrency;
    }

    size_t GetMaxConcurrency(size_t /* worker_count */) const override {
      return max_concurrency.load(std::memory_order_relaxed);
    }

    base::Mutex mutex;
    base::ConditionVariable threads_running;
    size_t worker_count = 0;
    std::atomic_size_t max_concurrency{kMaxTask + 1};
  };

  auto job = std::make_unique<JobTest>();
  JobTest* job_raw = job.get();
  auto state = std::make_shared<DefaultJobState>(
      &platform, std::move(job), TaskPriority::kUserVisible, kMaxTask);
  state->NotifyConcurrencyIncrease();

  // The main thread contributing is necessary for |worker_count| to reach
  // kMaxTask + 1 thus, Join() should not hang.
  state->Join();
  EXPECT_EQ(0U, job_raw->max_concurrency);
}

// Verify that Join() on a job that uses |worker_count| eventually converges
// and doesn't hang.
TEST(DefaultJobTest, WorkerCount) {
  static constexpr size_t kMaxTask = 4;
  DefaultPlatform platform(kMaxTask);

  // This Job spawns a workers until the first worker task completes.
  class JobTest : public JobTask {
   public:
    ~JobTest() override = default;

    void Run(JobDelegate* delegate) override {
      base::MutexGuard guard(&mutex);
      if (max_concurrency > 0) --max_concurrency;
    }

    size_t GetMaxConcurrency(size_t worker_count) const override {
      return worker_count + max_concurrency.load(std::memory_order_relaxed);
    }

    base::Mutex mutex;
    std::atomic_size_t max_concurrency{kMaxTask};
  };

  auto job = std::make_unique<JobTest>();
  JobTest* job_raw = job.get();
  auto state = std::make_shared<DefaultJobState>(
      &platform, std::move(job), TaskPriority::kUserVisible, kMaxTask);
  state->NotifyConcurrencyIncrease();

  // GetMaxConcurrency() eventually returns 0 thus, Join() should not hang.
  state->Join();
  EXPECT_EQ(0U, job_raw->max_concurrency);
}

// Verify that calling NotifyConcurrencyIncrease() (re-)schedules tasks with the
// intended concurrency.
TEST(DefaultJobTest, JobNotifyConcurrencyIncrease) {
  static constexpr size_t kMaxTask = 4;
  DefaultPlatform platform(kMaxTask);

  // This Job notifies |threads_running| once started and blocks on a barrier
  // until kMaxTask threads reach that point, and then returns.
  class JobTest : public JobTask {
   public:
    ~JobTest() override = default;

    void Run(JobDelegate* delegate) override {
      base::MutexGuard guard(&mutex);
      worker_count++;
      threads_running.NotifyAll();
      // Wait synchronously until |kMaxTask| workers reach this point.
      while (worker_count < kMaxTask) threads_running.Wait(&mutex);
      --max_concurrency;
    }

    size_t GetMaxConcurrency(size_t /* worker_count */) const override {
      return max_concurrency.load(std::memory_order_relaxed);
    }

    base::Mutex mutex;
    base::ConditionVariable threads_running;
    bool continue_flag = false;
    size_t worker_count = 0;
    std::atomic_size_t max_concurrency{kMaxTask / 2};
  };

  auto job = std::make_unique<JobTest>();
  JobTest* job_raw = job.get();
  auto state = std::make_shared<DefaultJobState>(
      &platform, std::move(job), TaskPriority::kUserVisible, kMaxTask);
  state->NotifyConcurrencyIncrease();

  {
    base::MutexGuard guard(&job_raw->mutex);
    while (job_raw->worker_count < kMaxTask / 2)
      job_raw->threads_running.Wait(&job_raw->mutex);
    EXPECT_EQ(kMaxTask / 2, job_raw->worker_count);

    job_raw->max_concurrency = kMaxTask;
  }
  state->NotifyConcurrencyIncrease();
  // Workers should reach |continue_flag| and eventually return thus, Join()
  // should not hang.
  state->Join();
  EXPECT_EQ(0U, job_raw->max_concurrency);
}

// Verify that Join() doesn't contribute if the Job is already finished.
TEST(DefaultJobTest, FinishBeforeJoin) {
  static constexpr size_t kMaxTask = 4;
  DefaultPlatform platform(kMaxTask);

  // This Job notifies |threads_running| once started and returns.
  class JobTest : public JobTask {
   public:
    ~JobTest() override = default;

    void Run(JobDelegate* delegate) override {
      base::MutexGuard guard(&mutex);
      worker_count++;
      // Assert that main thread doesn't contribute in this test.
      EXPECT_NE(main_thread_id, base::OS::GetCurrentThreadId());
      worker_ran.NotifyAll();
      --max_concurrency;
    }

    size_t GetMaxConcurrency(size_t /* worker_count */) const override {
      return max_concurrency.load(std::memory_order_relaxed);
    }

    const int main_thread_id = base::OS::GetCurrentThreadId();
    base::Mutex mutex;
    base::ConditionVariable worker_ran;
    size_t worker_count = 0;
    std::atomic_size_t max_concurrency{kMaxTask * 5};
  };

  auto job = std::make_unique<JobTest>();
  JobTest* job_raw = job.get();
  auto state = std::make_shared<DefaultJobState>(
      &platform, std::move(job), TaskPriority::kUserVisible, kMaxTask);
  state->NotifyConcurrencyIncrease();

  {
    base::MutexGuard guard(&job_raw->mutex);
    while (job_raw->worker_count < kMaxTask * 5)
      job_raw->worker_ran.Wait(&job_raw->mutex);
    EXPECT_EQ(kMaxTask * 5, job_raw->worker_count);
  }

  state->Join();
  EXPECT_EQ(0U, job_raw->max_concurrency);
}

// Verify that destroying a DefaultJobHandle triggers a DCHECK if neither Join()
// or Cancel() was called.
TEST(DefaultJobTest, LeakHandle) {
  class JobTest : public JobTask {
   public:
    ~JobTest() override = default;

    void Run(JobDelegate* delegate) override {}

    size_t GetMaxConcurrency(size_t /* worker_count */) const override {
      return 0;
    }
  };

  DefaultPlatform platform(0);
  auto job = std::make_unique<JobTest>();
  auto state = std::make_shared<DefaultJobState>(&platform, std::move(job),
                                                 TaskPriority::kUserVisible, 1);
  auto handle = std::make_unique<DefaultJobHandle>(std::move(state));
#ifdef DEBUG
  EXPECT_DEATH_IF_SUPPORTED({ handle.reset(); }, "");
#endif  // DEBUG
  handle->Join();
}

TEST(DefaultJobTest, AcquireTaskId) {
  class JobTest : public JobTask {
   public:
    ~JobTest() override = default;

    void Run(JobDelegate* delegate) override {}

    size_t GetMaxConcurrency(size_t /* worker_count */) const override {
      return 0;
    }
  };

  DefaultPlatform platform(0);
  auto job = std::make_unique<JobTest>();
  auto state = std::make_shared<DefaultJobState>(&platform, std::move(job),
                                                 TaskPriority::kUserVisible, 1);

  EXPECT_EQ(0U, state->AcquireTaskId());
  EXPECT_EQ(1U, state->AcquireTaskId());
  EXPECT_EQ(2U, state->AcquireTaskId());
  EXPECT_EQ(3U, state->AcquireTaskId());
  EXPECT_EQ(4U, state->AcquireTaskId());
  state->ReleaseTaskId(1);
  state->ReleaseTaskId(3);
  EXPECT_EQ(1U, state->AcquireTaskId());
  EXPECT_EQ(3U, state->AcquireTaskId());
  EXPECT_EQ(5U, state->AcquireTaskId());
}

}  // namespace default_job_unittest
}  // namespace platform
}  // namespace v8

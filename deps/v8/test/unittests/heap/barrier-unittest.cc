// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/barrier.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace heap {

namespace {

// Large timeout that will not trigger in tests.
constexpr base::TimeDelta test_timeout = base::TimeDelta::FromHours(3);

}  // namespace

TEST(OneshotBarrier, InitializeNotDone) {
  OneshotBarrier barrier(test_timeout);
  EXPECT_FALSE(barrier.DoneForTesting());
}

TEST(OneshotBarrier, DoneAfterWait_Sequential) {
  OneshotBarrier barrier(test_timeout);
  barrier.Start();
  barrier.Wait();
  EXPECT_TRUE(barrier.DoneForTesting());
}

namespace {

class ThreadWaitingOnBarrier final : public base::Thread {
 public:
  ThreadWaitingOnBarrier()
      : base::Thread(Options("ThreadWaitingOnBarrier")), barrier_(nullptr) {}

  void Initialize(OneshotBarrier* barrier) { barrier_ = barrier; }

  void Run() final { barrier_->Wait(); }

 private:
  OneshotBarrier* barrier_;
};

}  // namespace

TEST(OneshotBarrier, DoneAfterWait_Concurrent) {
  const int kThreadCount = 2;
  OneshotBarrier barrier(test_timeout);
  ThreadWaitingOnBarrier threads[kThreadCount];
  for (int i = 0; i < kThreadCount; i++) {
    threads[i].Initialize(&barrier);
    // All threads need to call Wait() to be done.
    barrier.Start();
  }
  for (int i = 0; i < kThreadCount; i++) {
    threads[i].Start();
  }
  for (int i = 0; i < kThreadCount; i++) {
    threads[i].Join();
  }
  EXPECT_TRUE(barrier.DoneForTesting());
}

TEST(OneshotBarrier, EarlyFinish_Concurrent) {
  const int kThreadCount = 2;
  OneshotBarrier barrier(test_timeout);
  ThreadWaitingOnBarrier threads[kThreadCount];
  // Test that one thread that actually finishes processing work before other
  // threads call Start() will move the barrier in Done state.
  barrier.Start();
  barrier.Wait();
  EXPECT_TRUE(barrier.DoneForTesting());
  for (int i = 0; i < kThreadCount; i++) {
    threads[i].Initialize(&barrier);
    // All threads need to call Wait() to be done.
    barrier.Start();
  }
  for (int i = 0; i < kThreadCount; i++) {
    threads[i].Start();
  }
  for (int i = 0; i < kThreadCount; i++) {
    threads[i].Join();
  }
  EXPECT_TRUE(barrier.DoneForTesting());
}

namespace {

class CountingThread final : public base::Thread {
 public:
  CountingThread(OneshotBarrier* barrier, base::Mutex* mutex, size_t* work)
      : base::Thread(Options("CountingThread")),
        barrier_(barrier),
        mutex_(mutex),
        work_(work),
        processed_work_(0) {}

  void Run() final {
    do {
      ProcessWork();
    } while (!barrier_->Wait());
    // Main thread is not processing work, so we need one last step.
    ProcessWork();
  }

  size_t processed_work() const { return processed_work_; }

 private:
  void ProcessWork() {
    base::MutexGuard guard(mutex_);
    processed_work_ += *work_;
    *work_ = 0;
  }

  OneshotBarrier* const barrier_;
  base::Mutex* const mutex_;
  size_t* const work_;
  size_t processed_work_;
};

}  // namespace

TEST(OneshotBarrier, Processing_Concurrent) {
  const size_t kWorkCounter = 173173;
  OneshotBarrier barrier(test_timeout);
  base::Mutex mutex;
  size_t work = 0;
  CountingThread counting_thread(&barrier, &mutex, &work);
  barrier.Start();
  barrier.Start();
  EXPECT_FALSE(barrier.DoneForTesting());
  counting_thread.Start();

  for (size_t i = 0; i < kWorkCounter; i++) {
    {
      base::MutexGuard guard(&mutex);
      work++;
    }
    barrier.NotifyAll();
  }
  barrier.Wait();
  counting_thread.Join();
  EXPECT_TRUE(barrier.DoneForTesting());
  EXPECT_EQ(kWorkCounter, counting_thread.processed_work());
}

}  // namespace heap
}  // namespace internal
}  // namespace v8

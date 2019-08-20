// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/condition-variable.h"

#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

TEST(ConditionVariable, WaitForAfterNofityOnSameThread) {
  for (int n = 0; n < 10; ++n) {
    Mutex mutex;
    ConditionVariable cv;

    MutexGuard lock_guard(&mutex);

    cv.NotifyOne();
    EXPECT_FALSE(cv.WaitFor(&mutex, TimeDelta::FromMicroseconds(n)));

    cv.NotifyAll();
    EXPECT_FALSE(cv.WaitFor(&mutex, TimeDelta::FromMicroseconds(n)));
  }
}


namespace {

class ThreadWithMutexAndConditionVariable final : public Thread {
 public:
  ThreadWithMutexAndConditionVariable()
      : Thread(Options("ThreadWithMutexAndConditionVariable")),
        running_(false),
        finished_(false) {}

  void Run() override {
    MutexGuard lock_guard(&mutex_);
    running_ = true;
    cv_.NotifyOne();
    while (running_) {
      cv_.Wait(&mutex_);
    }
    finished_ = true;
    cv_.NotifyAll();
  }

  bool running_;
  bool finished_;
  ConditionVariable cv_;
  Mutex mutex_;
};

}  // namespace


TEST(ConditionVariable, MultipleThreadsWithSeparateConditionVariables) {
  static const int kThreadCount = 128;
  ThreadWithMutexAndConditionVariable threads[kThreadCount];

  for (int n = 0; n < kThreadCount; ++n) {
    MutexGuard lock_guard(&threads[n].mutex_);
    EXPECT_FALSE(threads[n].running_);
    EXPECT_FALSE(threads[n].finished_);
    threads[n].Start();
    // Wait for nth thread to start.
    while (!threads[n].running_) {
      threads[n].cv_.Wait(&threads[n].mutex_);
    }
  }

  for (int n = kThreadCount - 1; n >= 0; --n) {
    MutexGuard lock_guard(&threads[n].mutex_);
    EXPECT_TRUE(threads[n].running_);
    EXPECT_FALSE(threads[n].finished_);
  }

  for (int n = 0; n < kThreadCount; ++n) {
    MutexGuard lock_guard(&threads[n].mutex_);
    EXPECT_TRUE(threads[n].running_);
    EXPECT_FALSE(threads[n].finished_);
    // Tell the nth thread to quit.
    threads[n].running_ = false;
    threads[n].cv_.NotifyOne();
  }

  for (int n = kThreadCount - 1; n >= 0; --n) {
    // Wait for nth thread to quit.
    MutexGuard lock_guard(&threads[n].mutex_);
    while (!threads[n].finished_) {
      threads[n].cv_.Wait(&threads[n].mutex_);
    }
    EXPECT_FALSE(threads[n].running_);
    EXPECT_TRUE(threads[n].finished_);
  }

  for (int n = 0; n < kThreadCount; ++n) {
    threads[n].Join();
    MutexGuard lock_guard(&threads[n].mutex_);
    EXPECT_FALSE(threads[n].running_);
    EXPECT_TRUE(threads[n].finished_);
  }
}


namespace {

class ThreadWithSharedMutexAndConditionVariable final : public Thread {
 public:
  ThreadWithSharedMutexAndConditionVariable()
      : Thread(Options("ThreadWithSharedMutexAndConditionVariable")),
        running_(false),
        finished_(false),
        cv_(nullptr),
        mutex_(nullptr) {}

  void Run() override {
    MutexGuard lock_guard(mutex_);
    running_ = true;
    cv_->NotifyAll();
    while (running_) {
      cv_->Wait(mutex_);
    }
    finished_ = true;
    cv_->NotifyAll();
  }

  bool running_;
  bool finished_;
  ConditionVariable* cv_;
  Mutex* mutex_;
};

}  // namespace


TEST(ConditionVariable, MultipleThreadsWithSharedSeparateConditionVariables) {
  static const int kThreadCount = 128;
  ThreadWithSharedMutexAndConditionVariable threads[kThreadCount];
  ConditionVariable cv;
  Mutex mutex;

  for (int n = 0; n < kThreadCount; ++n) {
    threads[n].mutex_ = &mutex;
    threads[n].cv_ = &cv;
  }

  // Start all threads.
  {
    MutexGuard lock_guard(&mutex);
    for (int n = 0; n < kThreadCount; ++n) {
      EXPECT_FALSE(threads[n].running_);
      EXPECT_FALSE(threads[n].finished_);
      threads[n].Start();
    }
  }

  // Wait for all threads to start.
  {
    MutexGuard lock_guard(&mutex);
    for (int n = kThreadCount - 1; n >= 0; --n) {
      while (!threads[n].running_) {
        cv.Wait(&mutex);
      }
    }
  }

  // Make sure that all threads are running.
  {
    MutexGuard lock_guard(&mutex);
    for (int n = 0; n < kThreadCount; ++n) {
      EXPECT_TRUE(threads[n].running_);
      EXPECT_FALSE(threads[n].finished_);
    }
  }

  // Tell all threads to quit.
  {
    MutexGuard lock_guard(&mutex);
    for (int n = kThreadCount - 1; n >= 0; --n) {
      EXPECT_TRUE(threads[n].running_);
      EXPECT_FALSE(threads[n].finished_);
      // Tell the nth thread to quit.
      threads[n].running_ = false;
    }
    cv.NotifyAll();
  }

  // Wait for all threads to quit.
  {
    MutexGuard lock_guard(&mutex);
    for (int n = 0; n < kThreadCount; ++n) {
      while (!threads[n].finished_) {
        cv.Wait(&mutex);
      }
    }
  }

  // Make sure all threads are finished.
  {
    MutexGuard lock_guard(&mutex);
    for (int n = kThreadCount - 1; n >= 0; --n) {
      EXPECT_FALSE(threads[n].running_);
      EXPECT_TRUE(threads[n].finished_);
    }
  }

  // Join all threads.
  for (int n = 0; n < kThreadCount; ++n) {
    threads[n].Join();
  }
}


namespace {

class LoopIncrementThread final : public Thread {
 public:
  LoopIncrementThread(int rem, int* counter, int limit, int thread_count,
                      ConditionVariable* cv, Mutex* mutex)
      : Thread(Options("LoopIncrementThread")),
        rem_(rem),
        counter_(counter),
        limit_(limit),
        thread_count_(thread_count),
        cv_(cv),
        mutex_(mutex) {
    EXPECT_LT(rem, thread_count);
    EXPECT_EQ(0, limit % thread_count);
  }

  void Run() override {
    int last_count = -1;
    while (true) {
      MutexGuard lock_guard(mutex_);
      int count = *counter_;
      while (count % thread_count_ != rem_ && count < limit_) {
        cv_->Wait(mutex_);
        count = *counter_;
      }
      if (count >= limit_) break;
      EXPECT_EQ(*counter_, count);
      if (last_count != -1) {
        EXPECT_EQ(last_count + (thread_count_ - 1), count);
      }
      count++;
      *counter_ = count;
      last_count = count;
      cv_->NotifyAll();
    }
  }

 private:
  const int rem_;
  int* counter_;
  const int limit_;
  const int thread_count_;
  ConditionVariable* cv_;
  Mutex* mutex_;
};

}  // namespace


TEST(ConditionVariable, LoopIncrement) {
  static const int kMaxThreadCount = 16;
  Mutex mutex;
  ConditionVariable cv;
  for (int thread_count = 1; thread_count < kMaxThreadCount; ++thread_count) {
    int limit = thread_count * 10;
    int counter = 0;

    // Setup the threads.
    Thread** threads = new Thread* [thread_count];
    for (int n = 0; n < thread_count; ++n) {
      threads[n] = new LoopIncrementThread(n, &counter, limit, thread_count,
                                           &cv, &mutex);
    }

    // Start all threads.
    for (int n = thread_count - 1; n >= 0; --n) {
      threads[n]->Start();
    }

    // Join and cleanup all threads.
    for (int n = 0; n < thread_count; ++n) {
      threads[n]->Join();
      delete threads[n];
    }
    delete[] threads;

    EXPECT_EQ(limit, counter);
  }
}

}  // namespace base
}  // namespace v8

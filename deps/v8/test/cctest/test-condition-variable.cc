// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "cctest.h"
#include "platform/condition-variable.h"
#include "platform/time.h"

using namespace ::v8::internal;


TEST(WaitForAfterNofityOnSameThread) {
  for (int n = 0; n < 10; ++n) {
    Mutex mutex;
    ConditionVariable cv;

    LockGuard<Mutex> lock_guard(&mutex);

    cv.NotifyOne();
    CHECK_EQ(false, cv.WaitFor(&mutex, TimeDelta::FromMicroseconds(n)));

    cv.NotifyAll();
    CHECK_EQ(false, cv.WaitFor(&mutex, TimeDelta::FromMicroseconds(n)));
  }
}


class ThreadWithMutexAndConditionVariable V8_FINAL : public Thread {
 public:
  ThreadWithMutexAndConditionVariable()
      : Thread("ThreadWithMutexAndConditionVariable"),
        running_(false), finished_(false) {}
  virtual ~ThreadWithMutexAndConditionVariable() {}

  virtual void Run() V8_OVERRIDE {
    LockGuard<Mutex> lock_guard(&mutex_);
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


TEST(MultipleThreadsWithSeparateConditionVariables) {
  static const int kThreadCount = 128;
  ThreadWithMutexAndConditionVariable threads[kThreadCount];

  for (int n = 0; n < kThreadCount; ++n) {
    LockGuard<Mutex> lock_guard(&threads[n].mutex_);
    CHECK(!threads[n].running_);
    CHECK(!threads[n].finished_);
    threads[n].Start();
    // Wait for nth thread to start.
    while (!threads[n].running_) {
      threads[n].cv_.Wait(&threads[n].mutex_);
    }
  }

  for (int n = kThreadCount - 1; n >= 0; --n) {
    LockGuard<Mutex> lock_guard(&threads[n].mutex_);
    CHECK(threads[n].running_);
    CHECK(!threads[n].finished_);
  }

  for (int n = 0; n < kThreadCount; ++n) {
    LockGuard<Mutex> lock_guard(&threads[n].mutex_);
    CHECK(threads[n].running_);
    CHECK(!threads[n].finished_);
    // Tell the nth thread to quit.
    threads[n].running_ = false;
    threads[n].cv_.NotifyOne();
  }

  for (int n = kThreadCount - 1; n >= 0; --n) {
    // Wait for nth thread to quit.
    LockGuard<Mutex> lock_guard(&threads[n].mutex_);
    while (!threads[n].finished_) {
      threads[n].cv_.Wait(&threads[n].mutex_);
    }
    CHECK(!threads[n].running_);
    CHECK(threads[n].finished_);
  }

  for (int n = 0; n < kThreadCount; ++n) {
    threads[n].Join();
    LockGuard<Mutex> lock_guard(&threads[n].mutex_);
    CHECK(!threads[n].running_);
    CHECK(threads[n].finished_);
  }
}


class ThreadWithSharedMutexAndConditionVariable V8_FINAL : public Thread {
 public:
  ThreadWithSharedMutexAndConditionVariable()
      : Thread("ThreadWithSharedMutexAndConditionVariable"),
        running_(false), finished_(false), cv_(NULL), mutex_(NULL) {}
  virtual ~ThreadWithSharedMutexAndConditionVariable() {}

  virtual void Run() V8_OVERRIDE {
    LockGuard<Mutex> lock_guard(mutex_);
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


TEST(MultipleThreadsWithSharedSeparateConditionVariables) {
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
    LockGuard<Mutex> lock_guard(&mutex);
    for (int n = 0; n < kThreadCount; ++n) {
      CHECK(!threads[n].running_);
      CHECK(!threads[n].finished_);
      threads[n].Start();
    }
  }

  // Wait for all threads to start.
  {
    LockGuard<Mutex> lock_guard(&mutex);
    for (int n = kThreadCount - 1; n >= 0; --n) {
      while (!threads[n].running_) {
        cv.Wait(&mutex);
      }
    }
  }

  // Make sure that all threads are running.
  {
    LockGuard<Mutex> lock_guard(&mutex);
    for (int n = 0; n < kThreadCount; ++n) {
      CHECK(threads[n].running_);
      CHECK(!threads[n].finished_);
    }
  }

  // Tell all threads to quit.
  {
    LockGuard<Mutex> lock_guard(&mutex);
    for (int n = kThreadCount - 1; n >= 0; --n) {
      CHECK(threads[n].running_);
      CHECK(!threads[n].finished_);
      // Tell the nth thread to quit.
      threads[n].running_ = false;
    }
    cv.NotifyAll();
  }

  // Wait for all threads to quit.
  {
    LockGuard<Mutex> lock_guard(&mutex);
    for (int n = 0; n < kThreadCount; ++n) {
      while (!threads[n].finished_) {
        cv.Wait(&mutex);
      }
    }
  }

  // Make sure all threads are finished.
  {
    LockGuard<Mutex> lock_guard(&mutex);
    for (int n = kThreadCount - 1; n >= 0; --n) {
      CHECK(!threads[n].running_);
      CHECK(threads[n].finished_);
    }
  }

  // Join all threads.
  for (int n = 0; n < kThreadCount; ++n) {
    threads[n].Join();
  }
}


class LoopIncrementThread V8_FINAL : public Thread {
 public:
  LoopIncrementThread(int rem,
                      int* counter,
                      int limit,
                      int thread_count,
                      ConditionVariable* cv,
                      Mutex* mutex)
      : Thread("LoopIncrementThread"), rem_(rem), counter_(counter),
        limit_(limit), thread_count_(thread_count), cv_(cv), mutex_(mutex) {
    CHECK_LT(rem, thread_count);
    CHECK_EQ(0, limit % thread_count);
  }

  virtual void Run() V8_OVERRIDE {
    int last_count = -1;
    while (true) {
      LockGuard<Mutex> lock_guard(mutex_);
      int count = *counter_;
      while (count % thread_count_ != rem_ && count < limit_) {
        cv_->Wait(mutex_);
        count = *counter_;
      }
      if (count >= limit_) break;
      CHECK_EQ(*counter_, count);
      if (last_count != -1) {
        CHECK_EQ(last_count + (thread_count_ - 1), count);
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


TEST(LoopIncrement) {
  static const int kMaxThreadCount = 16;
  Mutex mutex;
  ConditionVariable cv;
  for (int thread_count = 1; thread_count < kMaxThreadCount; ++thread_count) {
    int limit = thread_count * 100;
    int counter = 0;

    // Setup the threads.
    Thread** threads = new Thread*[thread_count];
    for (int n = 0; n < thread_count; ++n) {
      threads[n] = new LoopIncrementThread(
          n, &counter, limit, thread_count, &cv, &mutex);
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

    CHECK_EQ(limit, counter);
  }
}

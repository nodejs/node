// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "src/base/platform/platform.h"
#include "src/base/platform/semaphore.h"
#include "src/base/platform/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

namespace {

static const char kAlphabet[] = "XKOAD";
static const size_t kAlphabetSize = sizeof(kAlphabet) - 1;
static const size_t kBufferSize = 987;  // GCD(buffer size, alphabet size) = 1
static const size_t kDataSize = kBufferSize * kAlphabetSize * 10;


class ProducerThread final : public Thread {
 public:
  ProducerThread(char* buffer, Semaphore* free_space, Semaphore* used_space)
      : Thread(Options("ProducerThread")),
        buffer_(buffer),
        free_space_(free_space),
        used_space_(used_space) {}

  void Run() override {
    for (size_t n = 0; n < kDataSize; ++n) {
      free_space_->Wait();
      buffer_[n % kBufferSize] = kAlphabet[n % kAlphabetSize];
      used_space_->Signal();
    }
  }

 private:
  char* buffer_;
  Semaphore* const free_space_;
  Semaphore* const used_space_;
};


class ConsumerThread final : public Thread {
 public:
  ConsumerThread(const char* buffer, Semaphore* free_space,
                 Semaphore* used_space)
      : Thread(Options("ConsumerThread")),
        buffer_(buffer),
        free_space_(free_space),
        used_space_(used_space) {}

  void Run() override {
    for (size_t n = 0; n < kDataSize; ++n) {
      used_space_->Wait();
      EXPECT_EQ(kAlphabet[n % kAlphabetSize], buffer_[n % kBufferSize]);
      free_space_->Signal();
    }
  }

 private:
  const char* buffer_;
  Semaphore* const free_space_;
  Semaphore* const used_space_;
};


class WaitAndSignalThread final : public Thread {
 public:
  explicit WaitAndSignalThread(Semaphore* semaphore)
      : Thread(Options("WaitAndSignalThread")), semaphore_(semaphore) {}

  void Run() override {
    for (int n = 0; n < 100; ++n) {
      semaphore_->Wait();
      ASSERT_FALSE(semaphore_->WaitFor(TimeDelta::FromMicroseconds(1)));
      semaphore_->Signal();
    }
  }

 private:
  Semaphore* const semaphore_;
};

}  // namespace


TEST(Semaphore, ProducerConsumer) {
  char buffer[kBufferSize];
  std::memset(buffer, 0, sizeof(buffer));
  Semaphore free_space(kBufferSize);
  Semaphore used_space(0);
  ProducerThread producer_thread(buffer, &free_space, &used_space);
  ConsumerThread consumer_thread(buffer, &free_space, &used_space);
  producer_thread.Start();
  consumer_thread.Start();
  producer_thread.Join();
  consumer_thread.Join();
}


TEST(Semaphore, WaitAndSignal) {
  Semaphore semaphore(0);
  WaitAndSignalThread t1(&semaphore);
  WaitAndSignalThread t2(&semaphore);

  t1.Start();
  t2.Start();

  // Make something available.
  semaphore.Signal();

  t1.Join();
  t2.Join();

  semaphore.Wait();

  EXPECT_FALSE(semaphore.WaitFor(TimeDelta::FromMicroseconds(1)));
}


TEST(Semaphore, WaitFor) {
  Semaphore semaphore(0);

  // Semaphore not signalled - timeout.
  ASSERT_FALSE(semaphore.WaitFor(TimeDelta::FromMicroseconds(0)));
  ASSERT_FALSE(semaphore.WaitFor(TimeDelta::FromMicroseconds(100)));
  ASSERT_FALSE(semaphore.WaitFor(TimeDelta::FromMicroseconds(1000)));

  // Semaphore signalled - no timeout.
  semaphore.Signal();
  ASSERT_TRUE(semaphore.WaitFor(TimeDelta::FromMicroseconds(0)));
  semaphore.Signal();
  ASSERT_TRUE(semaphore.WaitFor(TimeDelta::FromMicroseconds(100)));
  semaphore.Signal();
  ASSERT_TRUE(semaphore.WaitFor(TimeDelta::FromMicroseconds(1000)));
}

}  // namespace base
}  // namespace v8

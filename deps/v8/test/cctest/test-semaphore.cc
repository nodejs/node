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

#include <stdlib.h>

#include "src/v8.h"

#include "src/base/platform/platform.h"
#include "test/cctest/cctest.h"


using namespace ::v8::internal;


class WaitAndSignalThread V8_FINAL : public v8::base::Thread {
 public:
  explicit WaitAndSignalThread(v8::base::Semaphore* semaphore)
      : Thread(Options("WaitAndSignalThread")), semaphore_(semaphore) {}
  virtual ~WaitAndSignalThread() {}

  virtual void Run() V8_OVERRIDE {
    for (int n = 0; n < 1000; ++n) {
      semaphore_->Wait();
      bool result =
          semaphore_->WaitFor(v8::base::TimeDelta::FromMicroseconds(1));
      DCHECK(!result);
      USE(result);
      semaphore_->Signal();
    }
  }

 private:
  v8::base::Semaphore* semaphore_;
};


TEST(WaitAndSignal) {
  v8::base::Semaphore semaphore(0);
  WaitAndSignalThread t1(&semaphore);
  WaitAndSignalThread t2(&semaphore);

  t1.Start();
  t2.Start();

  // Make something available.
  semaphore.Signal();

  t1.Join();
  t2.Join();

  semaphore.Wait();

  bool result = semaphore.WaitFor(v8::base::TimeDelta::FromMicroseconds(1));
  DCHECK(!result);
  USE(result);
}


TEST(WaitFor) {
  bool ok;
  v8::base::Semaphore semaphore(0);

  // Semaphore not signalled - timeout.
  ok = semaphore.WaitFor(v8::base::TimeDelta::FromMicroseconds(0));
  CHECK(!ok);
  ok = semaphore.WaitFor(v8::base::TimeDelta::FromMicroseconds(100));
  CHECK(!ok);
  ok = semaphore.WaitFor(v8::base::TimeDelta::FromMicroseconds(1000));
  CHECK(!ok);

  // Semaphore signalled - no timeout.
  semaphore.Signal();
  ok = semaphore.WaitFor(v8::base::TimeDelta::FromMicroseconds(0));
  CHECK(ok);
  semaphore.Signal();
  ok = semaphore.WaitFor(v8::base::TimeDelta::FromMicroseconds(100));
  CHECK(ok);
  semaphore.Signal();
  ok = semaphore.WaitFor(v8::base::TimeDelta::FromMicroseconds(1000));
  CHECK(ok);
}


static const char alphabet[] = "XKOAD";
static const int kAlphabetSize = sizeof(alphabet) - 1;
static const int kBufferSize = 4096;  // GCD(buffer size, alphabet size) = 1
static char buffer[kBufferSize];
static const int kDataSize = kBufferSize * kAlphabetSize * 10;

static v8::base::Semaphore free_space(kBufferSize);
static v8::base::Semaphore used_space(0);


class ProducerThread V8_FINAL : public v8::base::Thread {
 public:
  ProducerThread() : Thread(Options("ProducerThread")) {}
  virtual ~ProducerThread() {}

  virtual void Run() V8_OVERRIDE {
    for (int n = 0; n < kDataSize; ++n) {
      free_space.Wait();
      buffer[n % kBufferSize] = alphabet[n % kAlphabetSize];
      used_space.Signal();
    }
  }
};


class ConsumerThread V8_FINAL : public v8::base::Thread {
 public:
  ConsumerThread() : Thread(Options("ConsumerThread")) {}
  virtual ~ConsumerThread() {}

  virtual void Run() V8_OVERRIDE {
    for (int n = 0; n < kDataSize; ++n) {
      used_space.Wait();
      DCHECK_EQ(static_cast<int>(alphabet[n % kAlphabetSize]),
                static_cast<int>(buffer[n % kBufferSize]));
      free_space.Signal();
    }
  }
};


TEST(ProducerConsumer) {
  ProducerThread producer_thread;
  ConsumerThread consumer_thread;
  producer_thread.Start();
  consumer_thread.Start();
  producer_thread.Join();
  consumer_thread.Join();
}

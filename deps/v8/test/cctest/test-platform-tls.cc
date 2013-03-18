// Copyright 2011 the V8 project authors. All rights reserved.
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
//
// Tests of fast TLS support.

#include "v8.h"

#include "cctest.h"
#include "checks.h"
#include "platform.h"

using v8::internal::Thread;

static const int kValueCount = 128;

static Thread::LocalStorageKey keys[kValueCount];

static void* GetValue(int num) {
  return reinterpret_cast<void*>(static_cast<intptr_t>(num + 1));
}

static void DoTest() {
  for (int i = 0; i < kValueCount; i++) {
    CHECK(!Thread::HasThreadLocal(keys[i]));
  }
  for (int i = 0; i < kValueCount; i++) {
    Thread::SetThreadLocal(keys[i], GetValue(i));
  }
  for (int i = 0; i < kValueCount; i++) {
    CHECK(Thread::HasThreadLocal(keys[i]));
  }
  for (int i = 0; i < kValueCount; i++) {
    CHECK_EQ(GetValue(i), Thread::GetThreadLocal(keys[i]));
    CHECK_EQ(GetValue(i), Thread::GetExistingThreadLocal(keys[i]));
  }
  for (int i = 0; i < kValueCount; i++) {
    Thread::SetThreadLocal(keys[i], GetValue(kValueCount - i - 1));
  }
  for (int i = 0; i < kValueCount; i++) {
    CHECK(Thread::HasThreadLocal(keys[i]));
  }
  for (int i = 0; i < kValueCount; i++) {
    CHECK_EQ(GetValue(kValueCount - i - 1),
             Thread::GetThreadLocal(keys[i]));
    CHECK_EQ(GetValue(kValueCount - i - 1),
             Thread::GetExistingThreadLocal(keys[i]));
  }
}

class TestThread : public Thread {
 public:
  TestThread() : Thread("TestThread") {}

  virtual void Run() {
    DoTest();
  }
};

TEST(FastTLS) {
  for (int i = 0; i < kValueCount; i++) {
    keys[i] = Thread::CreateThreadLocalKey();
  }
  DoTest();
  TestThread thread;
  thread.Start();
  thread.Join();
}

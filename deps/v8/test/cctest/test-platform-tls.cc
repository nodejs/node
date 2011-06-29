// Copyright 2011 the V8 project authors. All rights reserved.
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

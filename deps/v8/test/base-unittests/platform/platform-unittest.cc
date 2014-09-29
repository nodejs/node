// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform.h"

#if V8_OS_POSIX
#include <unistd.h>  // NOLINT
#endif

#if V8_OS_WIN
#include "src/base/win32-headers.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

TEST(OS, GetCurrentProcessId) {
#if V8_OS_POSIX
  EXPECT_EQ(static_cast<int>(getpid()), OS::GetCurrentProcessId());
#endif

#if V8_OS_WIN
  EXPECT_EQ(static_cast<int>(::GetCurrentProcessId()),
            OS::GetCurrentProcessId());
#endif
}


TEST(OS, NumberOfProcessorsOnline) {
  EXPECT_GT(OS::NumberOfProcessorsOnline(), 0);
}


namespace {

class SelfJoinThread V8_FINAL : public Thread {
 public:
  SelfJoinThread() : Thread(Options("SelfJoinThread")) {}
  virtual void Run() V8_OVERRIDE { Join(); }
};

}


TEST(Thread, SelfJoin) {
  SelfJoinThread thread;
  thread.Start();
  thread.Join();
}


namespace {

class ThreadLocalStorageTest : public Thread, public ::testing::Test {
 public:
  ThreadLocalStorageTest() : Thread(Options("ThreadLocalStorageTest")) {
    for (size_t i = 0; i < ARRAY_SIZE(keys_); ++i) {
      keys_[i] = Thread::CreateThreadLocalKey();
    }
  }
  ~ThreadLocalStorageTest() {
    for (size_t i = 0; i < ARRAY_SIZE(keys_); ++i) {
      Thread::DeleteThreadLocalKey(keys_[i]);
    }
  }

  virtual void Run() V8_FINAL V8_OVERRIDE {
    for (size_t i = 0; i < ARRAY_SIZE(keys_); i++) {
      CHECK(!Thread::HasThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < ARRAY_SIZE(keys_); i++) {
      Thread::SetThreadLocal(keys_[i], GetValue(i));
    }
    for (size_t i = 0; i < ARRAY_SIZE(keys_); i++) {
      CHECK(Thread::HasThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < ARRAY_SIZE(keys_); i++) {
      CHECK_EQ(GetValue(i), Thread::GetThreadLocal(keys_[i]));
      CHECK_EQ(GetValue(i), Thread::GetExistingThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < ARRAY_SIZE(keys_); i++) {
      Thread::SetThreadLocal(keys_[i], GetValue(ARRAY_SIZE(keys_) - i - 1));
    }
    for (size_t i = 0; i < ARRAY_SIZE(keys_); i++) {
      CHECK(Thread::HasThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < ARRAY_SIZE(keys_); i++) {
      CHECK_EQ(GetValue(ARRAY_SIZE(keys_) - i - 1),
               Thread::GetThreadLocal(keys_[i]));
      CHECK_EQ(GetValue(ARRAY_SIZE(keys_) - i - 1),
               Thread::GetExistingThreadLocal(keys_[i]));
    }
  }

 private:
  static void* GetValue(size_t x) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(x + 1));
  }

  Thread::LocalStorageKey keys_[256];
};

}


TEST_F(ThreadLocalStorageTest, DoTest) {
  Run();
  Start();
  Join();
}

}  // namespace base
}  // namespace v8

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform.h"

#include <cstring>

#include "testing/gtest/include/gtest/gtest.h"

#if V8_OS_WIN
#include <windows.h>
#endif

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

TEST(OS, RemapPages) {
  if constexpr (OS::IsRemapPageSupported()) {
    size_t size = base::OS::AllocatePageSize();
    // Data to be remapped, filled with data.
    void* data = OS::Allocate(nullptr, size, base::OS::AllocatePageSize(),
                              OS::MemoryPermission::kReadWrite);
    ASSERT_TRUE(data);
    memset(data, 0xab, size);

    // Target mapping.
    void* remapped_data =
        OS::Allocate(nullptr, size, base::OS::AllocatePageSize(),
                     OS::MemoryPermission::kReadWrite);
    ASSERT_TRUE(remapped_data);

    EXPECT_TRUE(OS::RemapPages(data, size, remapped_data,
                               OS::MemoryPermission::kReadExecute));
    EXPECT_EQ(0, memcmp(remapped_data, data, size));

    OS::Free(data, size);
    OS::Free(remapped_data, size);
  }
}

namespace {

class ThreadLocalStorageTest : public Thread, public ::testing::Test {
 public:
  ThreadLocalStorageTest() : Thread(Options("ThreadLocalStorageTest")) {
    for (size_t i = 0; i < arraysize(keys_); ++i) {
      keys_[i] = Thread::CreateThreadLocalKey();
    }
  }
  ~ThreadLocalStorageTest() override {
    for (size_t i = 0; i < arraysize(keys_); ++i) {
      Thread::DeleteThreadLocalKey(keys_[i]);
    }
  }

  void Run() final {
    for (size_t i = 0; i < arraysize(keys_); i++) {
      CHECK(!Thread::HasThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      Thread::SetThreadLocal(keys_[i], GetValue(i));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      CHECK(Thread::HasThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      CHECK_EQ(GetValue(i), Thread::GetThreadLocal(keys_[i]));
      CHECK_EQ(GetValue(i), Thread::GetExistingThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      Thread::SetThreadLocal(keys_[i], GetValue(arraysize(keys_) - i - 1));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      CHECK(Thread::HasThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      CHECK_EQ(GetValue(arraysize(keys_) - i - 1),
               Thread::GetThreadLocal(keys_[i]));
      CHECK_EQ(GetValue(arraysize(keys_) - i - 1),
               Thread::GetExistingThreadLocal(keys_[i]));
    }
  }

 private:
  static void* GetValue(size_t x) {
    return bit_cast<void*>(static_cast<uintptr_t>(x + 1));
  }

  // Older versions of Android have fewer TLS slots (nominally 64, but the
  // system uses "about 5 of them" itself).
  Thread::LocalStorageKey keys_[32];
};

}  // namespace


TEST_F(ThreadLocalStorageTest, DoTest) {
  Run();
  CHECK(Start());
  Join();
}

TEST(StackTest, GetStackStart) { EXPECT_NE(nullptr, Stack::GetStackStart()); }

TEST(StackTest, GetCurrentStackPosition) {
  EXPECT_NE(nullptr, Stack::GetCurrentStackPosition());
}

#if !V8_OS_FUCHSIA
TEST(StackTest, StackVariableInBounds) {
  void* dummy;
  ASSERT_GT(static_cast<void*>(Stack::GetStackStart()),
            Stack::GetCurrentStackPosition());
  EXPECT_GT(static_cast<void*>(Stack::GetStackStart()),
            Stack::GetRealStackAddressForSlot(&dummy));
  EXPECT_LT(static_cast<void*>(Stack::GetCurrentStackPosition()),
            Stack::GetRealStackAddressForSlot(&dummy));
}
#endif  // !V8_OS_FUCHSIA

}  // namespace base
}  // namespace v8

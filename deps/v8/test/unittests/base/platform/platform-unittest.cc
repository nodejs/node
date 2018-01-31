// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform.h"

#if V8_OS_POSIX
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>  // NOLINT
#endif

#if V8_OS_WIN
#include "src/base/win32-headers.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"

#if V8_OS_ANDROID
#define DISABLE_ON_ANDROID(Name) DISABLED_##Name
#else
#define DISABLE_ON_ANDROID(Name) Name
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


namespace {

class ThreadLocalStorageTest : public Thread, public ::testing::Test {
 public:
  ThreadLocalStorageTest() : Thread(Options("ThreadLocalStorageTest")) {
    for (size_t i = 0; i < arraysize(keys_); ++i) {
      keys_[i] = Thread::CreateThreadLocalKey();
    }
  }
  ~ThreadLocalStorageTest() {
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
  Start();
  Join();
}

#if V8_OS_POSIX
// TODO(eholk): Add a windows version of these tests

namespace {

// These tests make sure the routines to allocate memory do so with the correct
// permissions.
//
// Unfortunately, there is no API to find the protection of a memory address,
// so instead we test permissions by installing a signal handler, probing a
// memory location and recovering from the fault.
//
// We don't test the execution permission because to do so we'd have to
// dynamically generate code and test if we can execute it.

class MemoryAllocationPermissionsTest : public ::testing::Test {
  static void SignalHandler(int signal, siginfo_t* info, void*) {
    siglongjmp(continuation_, 1);
  }
  struct sigaction old_action_;
// On Mac, sometimes we get SIGBUS instead of SIGSEGV.
#if V8_OS_MACOSX
  struct sigaction old_bus_action_;
#endif

 protected:
  virtual void SetUp() {
    struct sigaction action;
    action.sa_sigaction = SignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &action, &old_action_);
#if V8_OS_MACOSX
    sigaction(SIGBUS, &action, &old_bus_action_);
#endif
  }

  virtual void TearDown() {
    // be a good citizen and restore the old signal handler.
    sigaction(SIGSEGV, &old_action_, nullptr);
#if V8_OS_MACOSX
    sigaction(SIGBUS, &old_bus_action_, nullptr);
#endif
  }

 public:
  static sigjmp_buf continuation_;

  enum class MemoryAction { kRead, kWrite };

  void ProbeMemory(volatile int* buffer, MemoryAction action,
                   bool should_succeed) {
    const int save_sigs = 1;
    if (!sigsetjmp(continuation_, save_sigs)) {
      switch (action) {
        case MemoryAction::kRead: {
          // static_cast to remove the reference and force a memory read.
          USE(static_cast<int>(*buffer));
          break;
        }
        case MemoryAction::kWrite: {
          *buffer = 0;
          break;
        }
      }
      if (should_succeed) {
        SUCCEED();
      } else {
        FAIL();
      }
      return;
    }
    if (should_succeed) {
      FAIL();
    } else {
      SUCCEED();
    }
  }

  void TestPermissions(OS::MemoryPermission permission, bool can_read,
                       bool can_write) {
    const size_t page_size = OS::AllocatePageSize();
    int* buffer = static_cast<int*>(
        OS::Allocate(nullptr, page_size, page_size, permission));
    ProbeMemory(buffer, MemoryAction::kRead, can_read);
    ProbeMemory(buffer, MemoryAction::kWrite, can_write);
    CHECK(OS::Free(buffer, page_size));
  }
};

sigjmp_buf MemoryAllocationPermissionsTest::continuation_;

TEST_F(MemoryAllocationPermissionsTest, DoTest) {
  TestPermissions(OS::MemoryPermission::kNoAccess, false, false);
  TestPermissions(OS::MemoryPermission::kReadWrite, true, true);
  TestPermissions(OS::MemoryPermission::kReadWriteExecute, true, true);
}

}  // namespace
#endif  // V8_OS_POSIX

}  // namespace base
}  // namespace v8

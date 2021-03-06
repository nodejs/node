// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/allocation.h"

#if V8_OS_POSIX
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>  // NOLINT
#endif               // V8_OS_POSIX

#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

// TODO(eholk): Add a windows version of permissions tests.
#if V8_OS_POSIX
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
  void SetUp() override {
    struct sigaction action;
    action.sa_sigaction = SignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &action, &old_action_);
#if V8_OS_MACOSX
    sigaction(SIGBUS, &action, &old_bus_action_);
#endif
  }

  void TearDown() override {
    // Be a good citizen and restore the old signal handler.
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

  void TestPermissions(PageAllocator::Permission permission, bool can_read,
                       bool can_write) {
    v8::PageAllocator* page_allocator =
        v8::internal::GetPlatformPageAllocator();
    const size_t page_size = page_allocator->AllocatePageSize();
    int* buffer = static_cast<int*>(AllocatePages(
        page_allocator, nullptr, page_size, page_size, permission));
    ProbeMemory(buffer, MemoryAction::kRead, can_read);
    ProbeMemory(buffer, MemoryAction::kWrite, can_write);
    CHECK(FreePages(page_allocator, buffer, page_size));
  }
};

sigjmp_buf MemoryAllocationPermissionsTest::continuation_;

}  // namespace

// TODO(almuthanna): This test was skipped because it causes a crash when it is
// ran on Fuchsia. This issue should be solved later on
// Ticket: https://crbug.com/1028617
#if !defined(V8_TARGET_OS_FUCHSIA)
TEST_F(MemoryAllocationPermissionsTest, DoTest) {
  TestPermissions(PageAllocator::Permission::kNoAccess, false, false);
  TestPermissions(PageAllocator::Permission::kRead, true, false);
  TestPermissions(PageAllocator::Permission::kReadWrite, true, true);
  TestPermissions(PageAllocator::Permission::kReadWriteExecute, true, true);
  TestPermissions(PageAllocator::Permission::kReadExecute, true, false);
}
#endif

#endif  // V8_OS_POSIX

// Basic tests of allocation.

class AllocationTest : public ::testing::Test {};

TEST(AllocationTest, AllocateAndFree) {
  size_t page_size = v8::internal::AllocatePageSize();
  CHECK_NE(0, page_size);

  v8::PageAllocator* page_allocator = v8::internal::GetPlatformPageAllocator();

  // A large allocation, aligned at native allocation granularity.
  const size_t kAllocationSize = 1 * v8::internal::MB;
  void* mem_addr = v8::internal::AllocatePages(
      page_allocator, page_allocator->GetRandomMmapAddr(), kAllocationSize,
      page_size, PageAllocator::Permission::kReadWrite);
  CHECK_NOT_NULL(mem_addr);
  CHECK(v8::internal::FreePages(page_allocator, mem_addr, kAllocationSize));

  // A large allocation, aligned significantly beyond native granularity.
  const size_t kBigAlignment = 64 * v8::internal::MB;
  void* aligned_mem_addr = v8::internal::AllocatePages(
      page_allocator,
      AlignedAddress(page_allocator->GetRandomMmapAddr(), kBigAlignment),
      kAllocationSize, kBigAlignment, PageAllocator::Permission::kReadWrite);
  CHECK_NOT_NULL(aligned_mem_addr);
  CHECK_EQ(aligned_mem_addr, AlignedAddress(aligned_mem_addr, kBigAlignment));
  CHECK(v8::internal::FreePages(page_allocator, aligned_mem_addr,
                                kAllocationSize));
}

TEST(AllocationTest, ReserveMemory) {
  v8::PageAllocator* page_allocator = v8::internal::GetPlatformPageAllocator();
  size_t page_size = v8::internal::AllocatePageSize();
  const size_t kAllocationSize = 1 * v8::internal::MB;
  void* mem_addr = v8::internal::AllocatePages(
      page_allocator, page_allocator->GetRandomMmapAddr(), kAllocationSize,
      page_size, PageAllocator::Permission::kReadWrite);
  CHECK_NE(0, page_size);
  CHECK_NOT_NULL(mem_addr);
  size_t commit_size = page_allocator->CommitPageSize();
  CHECK(v8::internal::SetPermissions(page_allocator, mem_addr, commit_size,
                                     PageAllocator::Permission::kReadWrite));
  // Check whether we can write to memory.
  int* addr = static_cast<int*>(mem_addr);
  addr[v8::internal::KB - 1] = 2;
  CHECK(v8::internal::SetPermissions(page_allocator, mem_addr, commit_size,
                                     PageAllocator::Permission::kNoAccess));
  CHECK(v8::internal::FreePages(page_allocator, mem_addr, kAllocationSize));
}

}  // namespace internal
}  // namespace v8

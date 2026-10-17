// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <cstring>
#include <memory>

#include "include/v8-platform.h"
#include "src/sandbox/sandbox.h"
#include "src/sandbox/testing.h"
#include "test/unittests/test-utils.h"

#if defined(V8_USE_ADDRESS_SANITIZER)
#include <sanitizer/asan_interface.h>
#endif

#ifdef V8_OS_LINUX
#include <sys/mman.h>
#include <unistd.h>
#undef MAP_TYPE
#endif

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

#ifdef V8_OS_LINUX

using SandboxCrashFilterTest = TestWithIsolate;

// Verify that the crash filter correctly identifies accesses to unaddressable
// or non-canonical pointers (e.g., 52-bit addresses on 48-bit systems) as
// harmless.
TEST_F(SandboxCrashFilterTest, SandboxCrashFilterUnaddressableAccess) {
  ASSERT_EXIT(
      {
        SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
        volatile char* p = reinterpret_cast<volatile char*>(1ULL << 62);
        char c = *p;  // Trigger fault
        (void)c;
      },
      testing::ExitedWithCode(0), "Caught harmless memory access violation");
}

#ifdef V8_USE_ADDRESS_SANITIZER

// Verifies that a crash on an unknown address is treated as a sandbox
// violation, and that the ASan crash filter does not contradict this
// classification (by e.g. ignoring it as a null address).
TEST_F(SandboxCrashFilterTest, UnmappedMemoryViolation) {
  EXPECT_EXIT(
      {
        SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
        // Allocate a page and immediately free it to guarantee an unmapped
        // address without relying on hardcoded constants.
        size_t page_size = sysconf(_SC_PAGESIZE);
        void* dest = mmap(nullptr, page_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(dest != MAP_FAILED);
        munmap(dest, page_size);

        volatile char* p = reinterpret_cast<volatile char*>(dest);
        *p = 42;
      },
      testing::ExitedWithCode(1),
      testing::AllOf(testing::HasSubstr("V8 sandbox violation detected!"),
                     testing::Not(testing::HasSubstr(
                         "Caught ASan fault without a fault address"))));
}

// Verifies that a memcpy strictly contained within the sandbox is safely
// ignored.
//
// NOTE: The test may potentially start failing after a Clang roll, if the
// compiler optimizes away the memcpy.
TEST_F(SandboxCrashFilterTest, OverlappingMemcpyInsideSandbox) {
  Sandbox* sandbox = Sandbox::current();

  Address addr = sandbox->base() + 0x100000;  // Somewhere in the sandbox
  void* src = reinterpret_cast<void*>(addr);
  void* dest = reinterpret_cast<void*>(addr + 1);

  EXPECT_EXIT(
      {
        SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
        memcpy(dest, src, 10);
      },
      testing::ExitedWithCode(0),
      testing::AllOf(
          testing::HasSubstr("AddressSanitizer: memcpy-param-overlap"),
          testing::HasSubstr("harmless ASan fault")));
}

// Verifies that an overlapping memcpy completely outside the sandbox (e.g. on
// the C++ stack) is correctly caught as a violation.
//
// NOTE: The test may potentially start failing after a Clang roll, if the
// compiler optimizes away the memcpy. We use a volatile read to attempt to
// prevent this.
TEST_F(SandboxCrashFilterTest, OverlappingMemcpyOutsideSandbox) {
  char buffer[100] = {1};
  void* src = buffer;
  void* dest = buffer + 1;

  EXPECT_EXIT(
      {
        SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
        memcpy(dest, src, 10);
        volatile char escape = buffer[1];
        (void)escape;
      },
      testing::ExitedWithCode(1),
      testing::AllOf(
          testing::HasSubstr("AddressSanitizer: memcpy-param-overlap"),
          testing::HasSubstr("V8 sandbox violation detected!")));
}

// Verifies that an ASan fault on the C++ heap (outside the sandbox) triggers
// the violation.
//
// NOTE: The test may potentially start failing after a Clang roll, if the
// compiler optimizes away the memory access. We use a volatile read to attempt
// to prevent this.
TEST_F(SandboxCrashFilterTest, ASanUseAfterPoison) {
  constexpr size_t kBufferSize = 128;

  std::unique_ptr<char[]> buffer(new char[kBufferSize]);

  EXPECT_EXIT(
      {
        SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
        __asan_poison_memory_region(buffer.get(), kBufferSize);

        volatile char* p = reinterpret_cast<volatile char*>(buffer.get());
        char c = *p;
        (void)c;
      },
      testing::ExitedWithCode(1),
      testing::AllOf(testing::HasSubstr("AddressSanitizer: use-after-poison"),
                     testing::HasSubstr("V8 sandbox violation detected!")));
}

#ifdef V8_ENABLE_MEMORY_CORRUPTION_API

// Verifies that an ASan fault inside a region registered as safe is safely
// ignored by the crash filter.
//
// NOTE: The test may potentially start failing after a Clang roll, if the
// compiler optimizes away the memory access. We use a volatile read to attempt
// to prevent this.
TEST_F(SandboxCrashFilterTest, ASanUseAfterPoisonInSafeRegion) {
  constexpr size_t kBufferSize = 128;

  std::unique_ptr<char[]> buffer(new char[kBufferSize]);

  SandboxTesting::RegisterSafeMemoryRegion(
      reinterpret_cast<Address>(buffer.get()), kBufferSize,
      SandboxTesting::kReadAndWriteAccessIsSafe);

  EXPECT_EXIT(
      {
        SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
        __asan_poison_memory_region(buffer.get(), kBufferSize);

        volatile char* p = reinterpret_cast<volatile char*>(buffer.get());
        char c = *p;
        (void)c;
      },
      testing::ExitedWithCode(0),
      testing::AllOf(testing::HasSubstr("AddressSanitizer: use-after-poison"),
                     testing::HasSubstr("harmless ASan fault")));
}

// Verifies that an ASan fault inside the sandbox address space is safely
// ignored by the crash filter.
//
// NOTE: The test may potentially start failing after a Clang roll, if the
// compiler optimizes away the memory access. We use a volatile read to attempt
// to prevent this.
TEST_F(SandboxCrashFilterTest, ASanUseAfterPoisonInsideSandbox) {
  Sandbox* sandbox = Sandbox::current();

  v8::PageAllocator* allocator = sandbox->page_allocator();
  size_t size = allocator->AllocatePageSize();
  void* dest = allocator->AllocatePages(nullptr, size, size,
                                        v8::PageAllocator::kReadWrite);
  CHECK_NOT_NULL(dest);

  EXPECT_EXIT(
      {
        SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
        __asan_poison_memory_region(dest, 10);

        volatile char* p = reinterpret_cast<volatile char*>(dest);
        char c = *p;
        (void)c;
      },
      testing::ExitedWithCode(0),
      testing::AllOf(testing::HasSubstr("AddressSanitizer: use-after-poison"),
                     testing::HasSubstr("harmless ASan fault")));

  allocator->FreePages(dest, size);
}

#endif  // V8_ENABLE_MEMORY_CORRUPTION_API

#endif  // V8_USE_ADDRESS_SANITIZER

#endif  // V8_OS_LINUX

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <cstring>

#include "src/sandbox/testing.h"
#include "test/unittests/test-utils.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

#ifdef V8_OS_LINUX

using SandboxCrashFilterTest = TestWithIsolate;

// Verify that the crash filter correctly identifies accesses to unaddressable
// or non-canonical pointers (e.g., 52-bit addresses on 48-bit systems) as
// harmless.
TEST_F(SandboxCrashFilterTest, SandboxCrashFilterUnaddressableAccess) {
  SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
  ASSERT_EXIT(
      {
        volatile char* p = reinterpret_cast<volatile char*>(1ULL << 62);
        char c = *p;  // Trigger fault
        (void)c;
      },
      testing::ExitedWithCode(0), "Caught harmless memory access violation");
}

#ifdef V8_USE_ADDRESS_SANITIZER

// Verifies that a memcpy strictly contained within the sandbox is safely
// ignored.
//
// NOTE: The test may potentially start failing after a Clang roll, if the
// compiler optimizes away the memcpy.
TEST_F(SandboxCrashFilterTest, OverlappingMemcpyInsideSandbox) {
  SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
  Sandbox* sandbox = Sandbox::current();

  Address addr = sandbox->base() + 0x100000;  // Somewhere in the sandbox
  void* src = reinterpret_cast<void*>(addr);
  void* dest = reinterpret_cast<void*>(addr + 1);

  EXPECT_EXIT(
      { memcpy(dest, src, 10); }, testing::ExitedWithCode(0),
      ".*harmless ASan fault.*");
}

// Verifies that an overlapping memcpy completely outside the sandbox (e.g. on
// the C++ stack) is correctly caught as a violation.
//
// NOTE: The test may potentially start failing after a Clang roll, if the
// compiler optimizes away the memcpy. We use a volatile read to attempt to
// prevent this.
TEST_F(SandboxCrashFilterTest, OverlappingMemcpyOutsideSandbox) {
  SandboxTesting::Enable(SandboxTesting::Mode::kForTesting);
  char buffer[100] = {1};
  void* src = buffer;
  void* dest = buffer + 1;

  EXPECT_EXIT(
      {
        memcpy(dest, src, 10);
        volatile char escape = buffer[1];
        (void)escape;
      },
      testing::ExitedWithCode(1), ".*V8 sandbox violation detected!.*");
}

#endif  // V8_USE_ADDRESS_SANITIZER

#endif  // V8_OS_LINUX

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX

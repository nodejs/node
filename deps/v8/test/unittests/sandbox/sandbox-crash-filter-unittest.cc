// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

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

#endif  // V8_OS_LINUX

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX

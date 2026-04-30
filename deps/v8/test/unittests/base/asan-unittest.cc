// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include <vector>

#include "src/base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

#if defined(V8_USE_ADDRESS_SANITIZER)
using AsanDeathTest = ::testing::Test;

// AddressSanitizer defaults to abort_on_error=1 on macOS and Android, meaning
// it will terminate the process with SIGABRT (Signal 6) instead of exiting with
// an exit code. See `abort_on_error` in `sanitizer_common/sanitizer_flags.inc`
// in the LLVM compiler-rt source code.
#if defined(V8_OS_DARWIN) || defined(V8_OS_ANDROID)
#define ASAN_EXPECTED_DEATH_STATUS ::testing::KilledBySignal(SIGABRT)
#else
#define ASAN_EXPECTED_DEATH_STATUS ::testing::ExitedWithCode(1)
#endif

// Regression test for crbug.com/495601479. It verifies that crashes exit with
// code 1; both Fuzzilli and ClusterFuzz rely on this exit code for proper
// reporting.
TEST_F(AsanDeathTest, FuzzilliCrashExitsNonZero) {
  EXPECT_EXIT(
      {
        auto* vec = new std::vector<int>(4);  // NOLINT
        delete vec;                           // NOLINT
        USE(vec->at(0));                      // NOLINT
      },
      ASAN_EXPECTED_DEATH_STATUS,
      "ERROR: AddressSanitizer: heap-use-after-free");
}

#endif  // V8_USE_ADDRESS_SANITIZER

}  // namespace base
}  // namespace v8

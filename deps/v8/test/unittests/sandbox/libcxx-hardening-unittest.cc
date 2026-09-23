// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

// We expect libc++ hardening to be enabled when the sandbox is active as it can
// mitigate a number of "classic" memory safety bug classes on the sandbox
// attack surface. These tests attempt to ensure that it is enabled.

using LibcxxHardeningTest = ::testing::Test;

TEST_F(LibcxxHardeningTest, VectorOutOfBounds) {
  std::vector<int> v = {1, 2, 3};
  ASSERT_DEATH_IF_SUPPORTED((void)v[3], "");
}

TEST_F(LibcxxHardeningTest, SpanOutOfBounds) {
  int arr[] = {1, 2, 3};
  std::span<int> s(arr);
  ASSERT_DEATH_IF_SUPPORTED((void)s[3], "");
}

TEST_F(LibcxxHardeningTest, OptionalNullAccess) {
  std::optional<int> o;
  ASSERT_DEATH_IF_SUPPORTED((void)*o, "");
}

TEST_F(LibcxxHardeningTest, StringViewOutOfBounds) {
  std::string_view sv = "abc";
  ASSERT_DEATH_IF_SUPPORTED((void)sv[3], "");
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX

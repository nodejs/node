// Copyright 2026 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/base/internal/hardening.h"

#include "gtest/gtest.h"
#include "absl/base/options.h"

namespace {

TEST(BoundsCheckTest, HardeningAssertInBounds) {
  absl::base_internal::HardeningAssertInBounds(0, 10);
}

TEST(BoundsChecksDeathTest, HardeningAssertInBounds) {
#if GTEST_HAS_DEATH_TEST && (!defined(NDEBUG) || (ABSL_OPTION_HARDENED == 1 || \
                                                  ABSL_OPTION_HARDENED == 2))
  // The underlying mechanism of termination varies, and may include SIGILL
  // or SIGABRT.
  EXPECT_DEATH(absl::base_internal::HardeningAssertInBounds(10, 10), "");
#endif
}

}  // namespace

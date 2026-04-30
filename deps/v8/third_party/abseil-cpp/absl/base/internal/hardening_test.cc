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

#include <vector>

#include "gtest/gtest.h"
#include "absl/base/macros.h"
#include "absl/base/options.h"

namespace {

bool IsHardened() {
  bool hardened = false;
  ABSL_HARDENING_ASSERT([&hardened]() {
      hardened = true;
      return true;
    }()
  );
  return hardened;
}

bool IsHardenedSlow() {
  bool hardened = false;
  ABSL_HARDENING_ASSERT_SLOW([&hardened]() {
      hardened = true;
      return true;
    }()
  );
  return hardened;
}

TEST(HardeningTest, HardeningAssertSlow) {
  absl::base_internal::HardeningAssertSlow(true);
  if (!IsHardenedSlow()) {
    absl::base_internal::HardeningAssertSlow(false);
  }
}

TEST(HardeningDeathTest, HardeningAssertSlow) {
#if GTEST_HAS_DEATH_TEST
  if (IsHardenedSlow()) {
    // The underlying mechanism of termination varies, and may include SIGILL
    // or SIGABRT.
    EXPECT_DEATH(absl::base_internal::HardeningAssertSlow(false), "");
  }
#endif
}

TEST(HardeningTest, HardeningAssertGT) {
  absl::base_internal::HardeningAssertGT(1, 0);
}

TEST(HardeningDeathTest, HardeningAssertGT) {
#if GTEST_HAS_DEATH_TEST
  if (IsHardened()) {
    // The underlying mechanism of termination varies, and may include SIGILL
    // or SIGABRT.
    EXPECT_DEATH(absl::base_internal::HardeningAssertGT(1, 1), "");
    EXPECT_DEATH(absl::base_internal::HardeningAssertGT(0, 1), "");
  }
#endif
}

TEST(HardeningTest, HardeningAssertGE) {
  absl::base_internal::HardeningAssertGE(1, 0);
  absl::base_internal::HardeningAssertGE(1, 1);
}

TEST(HardeningDeathTest, HardeningAssertGE) {
#if GTEST_HAS_DEATH_TEST
  if (IsHardened()) {
    // The underlying mechanism of termination varies, and may include SIGILL
    // or SIGABRT.
    EXPECT_DEATH(absl::base_internal::HardeningAssertGE(0, 1), "");
  }
#endif
}

TEST(HardeningTest, HardeningAssertLT) {
  absl::base_internal::HardeningAssertLT(0, 1);
}

TEST(HardeningDeathTest, HardeningAssertLT) {
#if GTEST_HAS_DEATH_TEST
  if (IsHardened()) {
    // The underlying mechanism of termination varies, and may include SIGILL
    // or SIGABRT.
    EXPECT_DEATH(absl::base_internal::HardeningAssertLT(1, 1), "");
    EXPECT_DEATH(absl::base_internal::HardeningAssertLT(1, 0), "");
  }
#endif
}

TEST(HardeningTest, HardeningAssertLE) {
  absl::base_internal::HardeningAssertLE(0, 1);
  absl::base_internal::HardeningAssertLE(1, 1);
}

TEST(HardeningDeathTest, HardeningAssertLE) {
#if GTEST_HAS_DEATH_TEST
  if (IsHardened()) {
    // The underlying mechanism of termination varies, and may include SIGILL
    // or SIGABRT.
    EXPECT_DEATH(absl::base_internal::HardeningAssertLE(1, 0), "");
  }
#endif
}

TEST(HardeningTest, HardeningAssertInBounds) {
  absl::base_internal::HardeningAssertInBounds(0, 10);
}

TEST(HardeningDeathTest, HardeningAssertInBounds) {
#if GTEST_HAS_DEATH_TEST
  if (IsHardened()) {
    // The underlying mechanism of termination varies, and may include SIGILL
    // or SIGABRT.
    EXPECT_DEATH(absl::base_internal::HardeningAssertInBounds(10, 10), "");
  }
#endif
}

TEST(HardeningTest, HardeningAssertNonEmpty) {
  std::vector<int> v = {1};
  absl::base_internal::HardeningAssertNonEmpty(v);
}

TEST(HardeningDeathTest, HardeningAssertNonEmpty) {
#if GTEST_HAS_DEATH_TEST
  if (IsHardened()) {
    // The underlying mechanism of termination varies, and may include SIGILL
    // or SIGABRT.
    std::vector<int> v = {};
    EXPECT_DEATH(absl::base_internal::HardeningAssertNonEmpty(v), "");
  }
#endif
}

TEST(HardeningTest, HardeningAssertNonNull) {
  int x = 1;
  absl::base_internal::HardeningAssertNonNull(&x);
}

TEST(HardeningDeathTest, HardeningAssertNonNull) {
#if GTEST_HAS_DEATH_TEST
  if (IsHardened()) {
    // The underlying mechanism of termination varies, and may include SIGILL
    // or SIGABRT.
    int *x = nullptr;
    EXPECT_DEATH(absl::base_internal::HardeningAssertNonNull(x), "");
  }
#endif
}

}  // namespace

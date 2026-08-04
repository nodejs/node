// Copyright 2025 The Abseil Authors
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

#include "absl/strings/resize_and_overwrite.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "gtest/gtest.h"
#include "absl/base/dynamic_annotations.h"
#include "absl/log/absl_check.h"

namespace {

struct ResizeAndOverwriteParam {
  size_t initial_size;
  size_t requested_capacity;
  size_t final_size;
};

using StringResizeAndOverwriteTest =
    ::testing::TestWithParam<ResizeAndOverwriteParam>;

TEST_P(StringResizeAndOverwriteTest, StringResizeAndOverwrite) {
  const auto& param = GetParam();
  std::string s(param.initial_size, 'a');
  absl::StringResizeAndOverwrite(
      s, param.requested_capacity, [&](char* p, size_t n) {
        ABSL_CHECK_EQ(n, param.requested_capacity);
        if (param.final_size >= param.initial_size) {
          // Append case.
          std::fill(p + param.initial_size, p + param.final_size, 'b');
        } else if (param.final_size > 0) {
          // Truncate case.
          p[param.final_size - 1] = 'b';
        }
        p[param.final_size] = '\0';
        return param.final_size;
      });

  std::string expected;
  if (param.final_size >= param.initial_size) {
    // Append case.
    expected = std::string(param.initial_size, 'a') +
               std::string(param.final_size - param.initial_size, 'b');
  } else if (param.final_size > 0) {
    // Truncate case.
    expected = std::string(param.final_size - 1, 'a') + std::string("b");
  }

  EXPECT_EQ(s, expected);
  EXPECT_EQ(s.c_str()[param.final_size], '\0');
}

TEST_P(StringResizeAndOverwriteTest, StringResizeAndOverwriteFallback) {
  const auto& param = GetParam();
  std::string s(param.initial_size, 'a');
  absl::strings_internal::StringResizeAndOverwriteFallback(
      s, param.requested_capacity, [&](char* p, size_t n) {
        ABSL_CHECK_EQ(n, param.requested_capacity);
        if (param.final_size >= param.initial_size) {
          // Append case.
          std::fill(p + param.initial_size, p + param.final_size, 'b');
        } else if (param.final_size > 0) {
          // Truncate case.
          p[param.final_size - 1] = 'b';
        }
        p[param.final_size] = '\0';
        return param.final_size;
      });

  std::string expected;
  if (param.final_size >= param.initial_size) {
    // Append case.
    expected = std::string(param.initial_size, 'a') +
               std::string(param.final_size - param.initial_size, 'b');
  } else if (param.final_size > 0) {
    // Truncate case.
    expected = std::string(param.final_size - 1, 'a') + std::string("b");
  }

  EXPECT_EQ(s, expected);
  EXPECT_EQ(s.c_str()[param.final_size], '\0');
}

#ifdef ABSL_HAVE_MEMORY_SANITIZER
constexpr bool kMSan = true;
#else
constexpr bool kMSan = false;
#endif

TEST_P(StringResizeAndOverwriteTest, Initialized) {
  if (!kMSan) {
    GTEST_SKIP() << "Skipping test without MSan.";
  }

  const auto& param = GetParam();
  std::string s(param.initial_size, 'a');

  auto op = [&]() {
    absl::StringResizeAndOverwrite(s, param.requested_capacity,
                                   [&](char*, size_t) {
                                     // Fail to initialize the buffer in full.
                                     return param.final_size;
                                   });
  };

  if (param.initial_size < param.final_size) {
#ifndef NDEBUG
    EXPECT_DEATH_IF_SUPPORTED(op(),
                              "MemorySanitizer: use-of-uninitialized-value");
#endif
  } else {
    // The string is fully initialized from the initial constructor, or we skip
    // the check in optimized builds.
    op();
  }
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(StringResizeAndOverwriteTestSuite,
                         StringResizeAndOverwriteTest,
                         ::testing::ValuesIn<ResizeAndOverwriteParam>({
                             // Append cases.
                             {0,  10,  5},
                             {10, 10, 10},
                             {10, 15, 15},
                             {10, 20, 15},
                             {10, 40, 40},
                             {10, 50, 40},
                             {30, 35, 35},
                             {30, 45, 35},
                             {10, 30, 15},
                             // Truncate cases.
                             {15, 15, 10},
                             {40, 40, 35},
                             {40, 30, 10},
                             {10, 15, 0},
                         }));
// clang-format on

}  // namespace

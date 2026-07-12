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

#include "absl/strings/internal/append_and_overwrite.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "gtest/gtest.h"
#include "absl/log/absl_check.h"

namespace {

struct AppendAndOverwriteParam {
  size_t initial_size;
  size_t append_capacity;
  size_t append_size;
};

using StringAppendAndOverwriteTest =
    ::testing::TestWithParam<AppendAndOverwriteParam>;

TEST_P(StringAppendAndOverwriteTest, StringAppendAndOverwrite) {
  const auto& param = GetParam();
  std::string s(param.initial_size, 'a');
  absl::strings_internal::StringAppendAndOverwrite(
      s, param.append_capacity, [&](char* p, size_t n) {
        ABSL_CHECK_EQ(n, param.append_capacity);
        std::fill_n(p, param.append_size, 'b');
        p[param.append_size] = '\0';
        return param.append_size;
      });

  std::string expected =
      std::string(param.initial_size, 'a') +
      std::string(param.append_size,
                  'b');

  EXPECT_EQ(s, expected);
  EXPECT_EQ(s.c_str()[s.size()], '\0');
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(StringAppendAndOverwriteTestSuite,
                         StringAppendAndOverwriteTest,
                         ::testing::ValuesIn<AppendAndOverwriteParam>({
                             {0,  10,  5},
                             {10, 10, 10},
                             {10, 15, 15},
                             {10, 20, 15},
                             {10, 40, 40},
                             {10, 50, 40},
                             {30, 35, 35},
                             {30, 45, 35},
                             {10, 30, 15},
                         }));
// clang-format on

TEST(StringAppendAndOverwrite, AmortizedComplexity) {
  std::string str;
  std::string expected;
  size_t prev_cap = str.capacity();
  int cap_increase_count = 0;
  for (int i = 0; i < 1000; ++i) {
    char c = static_cast<char>('a' + (i % 26));
    absl::strings_internal::StringAppendAndOverwrite(
        str, 1, [c](char* buf, size_t buf_size) {
          ABSL_CHECK_EQ(buf_size, 1);
          buf[0] = c;
          return size_t{1};
        });
    expected.push_back(c);
    EXPECT_EQ(str, expected);
    size_t new_cap = str.capacity();
    if (new_cap > prev_cap) {
      ++cap_increase_count;
    }
    prev_cap = new_cap;
  }
  EXPECT_LT(cap_increase_count, 50);
}

}  // namespace

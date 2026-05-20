// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/strings/string_view.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <ios>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/meta/type_traits.h"

namespace {

TEST(StringViewTest, TruncSubstr) {
  const absl::string_view hi("hi");
  EXPECT_EQ("", absl::ClippedSubstr(hi, 0, 0));
  EXPECT_EQ("h", absl::ClippedSubstr(hi, 0, 1));
  EXPECT_EQ("hi", absl::ClippedSubstr(hi, 0));
  EXPECT_EQ("i", absl::ClippedSubstr(hi, 1));
  EXPECT_EQ("", absl::ClippedSubstr(hi, 2));
  EXPECT_EQ("", absl::ClippedSubstr(hi, 3));  // truncation
  EXPECT_EQ("", absl::ClippedSubstr(hi, 3, 2));  // truncation
}

TEST(StringViewTest, NullSafeStringView) {
  {
    absl::string_view s = absl::NullSafeStringView(nullptr);
    EXPECT_EQ(nullptr, s.data());
    EXPECT_EQ(0u, s.size());
    EXPECT_EQ(absl::string_view(), s);
  }
  {
    static const char kHi[] = "hi";
    absl::string_view s = absl::NullSafeStringView(kHi);
    EXPECT_EQ(kHi, s.data());
    EXPECT_EQ(strlen(kHi), s.size());
    EXPECT_EQ(absl::string_view("hi"), s);
  }
}

TEST(StringViewTest, ConstexprNullSafeStringView) {
  {
    constexpr absl::string_view s = absl::NullSafeStringView(nullptr);
    EXPECT_EQ(nullptr, s.data());
    EXPECT_EQ(0u, s.size());
    EXPECT_EQ(absl::string_view(), s);
  }
  {
    static constexpr char kHi[] = "hi";
    absl::string_view s = absl::NullSafeStringView(kHi);
    EXPECT_EQ(kHi, s.data());
    EXPECT_EQ(strlen(kHi), s.size());
    EXPECT_EQ(absl::string_view("hi"), s);
  }
  {
    constexpr absl::string_view s = absl::NullSafeStringView("hello");
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ("hello", s);
  }
}

}  // namespace

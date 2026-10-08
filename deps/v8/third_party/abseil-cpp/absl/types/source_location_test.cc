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

#include "absl/types/source_location.h"

#include "absl/base/config.h"

#ifdef ABSL_HAVE_STD_SOURCE_LOCATION
#include <source_location>  // NOLINT(build/c++20)
#endif
#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"

namespace {

using ::absl::SourceLocation;
using ::testing::EndsWith;

TEST(SourceLocationTest, DefaultConstructionWorks) {
  static_assert(!std::is_trivially_default_constructible_v<SourceLocation>);
  static_assert(std::is_nothrow_default_constructible_v<SourceLocation>);
  constexpr SourceLocation loc1 [[maybe_unused]];
  SourceLocation loc2 [[maybe_unused]]{};
  EXPECT_EQ(loc1.line(), loc2.line());
}

#ifdef ABSL_INTERNAL_HAVE_BUILTIN_LINE_FILE

TEST(SourceLocationTest, ConstexprMembers) {
  constexpr SourceLocation loc1 = absl::SourceLocation::current();
  const SourceLocation loc2 = absl::SourceLocation::current();
  EXPECT_EQ(loc1.line(), loc2.line() - 1);
  EXPECT_EQ(absl::string_view(loc1.file_name()), loc2.file_name());
}

TEST(SourceLocationTest, ConversionFromStdSourceLocationWorks) {
#ifndef ABSL_HAVE_STD_SOURCE_LOCATION
  GTEST_SKIP() << "std::source_location is not available";
#else
  constexpr SourceLocation loc1 = std::source_location::current();
  const std::source_location loc2 = std::source_location::current();
  EXPECT_EQ(loc1.line(), loc2.line() - 1);
  EXPECT_EQ(absl::string_view(loc1.file_name()), loc2.file_name());
#endif
}

TEST(SourceLocationTest, CopyConstructionWorks) {
  constexpr SourceLocation location = absl::SourceLocation::current();
  constexpr int line = __LINE__ - 1;

  EXPECT_EQ(location.line(), line);
  EXPECT_THAT(location.file_name(), EndsWith("source_location_test.cc"));
}

TEST(SourceLocationTest, CopyAssignmentWorks) {
  SourceLocation location;
  location = absl::SourceLocation::current();
  constexpr int line = __LINE__ - 1;

  EXPECT_EQ(location.line(), line);
  EXPECT_THAT(location.file_name(), EndsWith("source_location_test.cc"));
}

SourceLocation Echo(const SourceLocation& location) { return location; }

TEST(SourceLocationTest, ExpectedUsageWorks) {
  SourceLocation location = Echo(absl::SourceLocation::current());
  constexpr int line = __LINE__ - 1;

  EXPECT_EQ(location.line(), line);
  EXPECT_THAT(location.file_name(), EndsWith("source_location_test.cc"));
}

TEST(SourceLocationTest, CurrentWorks) {
  constexpr SourceLocation location = SourceLocation::current();
  constexpr int line = __LINE__ - 1;

  EXPECT_EQ(location.line(), line);
  EXPECT_THAT(location.file_name(), EndsWith("source_location_test.cc"));
}

SourceLocation FuncWithDefaultParam(
    SourceLocation loc = SourceLocation::current()) {
  return loc;
}

TEST(SourceLocationTest, CurrentWorksAsDefaultParam) {
  SourceLocation location = FuncWithDefaultParam();
  constexpr int line = __LINE__ - 1;

  EXPECT_EQ(location.line(), line);
  EXPECT_THAT(location.file_name(), EndsWith("source_location_test.cc"));
}

#endif

template <typename T>
bool TryPassLineAndFile(decltype(T::current(0, ""))*) {
  return true;
}
template <typename T>
bool TryPassLineAndFile(decltype(T::current({}, 0, ""))*) {
  return true;
}
template <typename T>
bool TryPassLineAndFile(decltype(T::current(typename T::Tag{}, 0, ""))*) {
  return true;
}
template <typename T>
bool TryPassLineAndFile(...) {
  return false;
}

TEST(SourceLocationTest, CantPassLineAndFile) {
#ifdef ABSL_HAVE_STD_SOURCE_LOCATION
  using StdSourceLocation = std::source_location;
#else
  using StdSourceLocation = void;
#endif
  if constexpr (!std::is_same_v<absl::SourceLocation, StdSourceLocation>) {
    EXPECT_FALSE(TryPassLineAndFile<absl::SourceLocation>(nullptr));
  }
}

}  // namespace

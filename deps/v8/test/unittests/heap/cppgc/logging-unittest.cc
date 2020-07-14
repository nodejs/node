// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/logging.h"

#include <string>

#include "include/cppgc/source-location.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {
// GCC < 9 has a bug due to which calling non-constexpr functions are not
// allowed even on constexpr path:
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67026.
#if !defined(__GNUC__) || defined(__clang__)
constexpr int CheckInConstexpr(int a) {
  CPPGC_DCHECK(a > 0);
  CPPGC_CHECK(a > 0);
  return a;
}
#endif
}  // namespace

TEST(LoggingTest, Pass) {
  CPPGC_DCHECK(true);
  CPPGC_CHECK(true);
}

TEST(LoggingTest, Fail) {
#if DEBUG
  EXPECT_DEATH_IF_SUPPORTED(CPPGC_DCHECK(false), "");
#endif
  EXPECT_DEATH_IF_SUPPORTED(CPPGC_CHECK(false), "");
}

TEST(LoggingTest, DontReportUnused) {
  int a = 1;
  CPPGC_DCHECK(a);
}

#if !defined(__GNUC__) || defined(__clang__)
TEST(LoggingTest, ConstexprContext) {
  constexpr int a = CheckInConstexpr(1);
  CPPGC_DCHECK(a);
}
#endif

#if DEBUG && !defined(OFFICIAL_BUILD)
TEST(LoggingTest, Message) {
  using ::testing::ContainsRegex;
  EXPECT_DEATH_IF_SUPPORTED(CPPGC_DCHECK(5 == 7),
                            ContainsRegex("failed.*5 == 7"));
  EXPECT_DEATH_IF_SUPPORTED(CPPGC_CHECK(5 == 7),
                            ContainsRegex("failed.*5 == 7"));
}

#if CPPGC_SUPPORTS_SOURCE_LOCATION
TEST(LoggingTest, SourceLocation) {
  using ::testing::AllOf;
  using ::testing::HasSubstr;
  constexpr auto loc = SourceLocation::Current();
  EXPECT_DEATH_IF_SUPPORTED(CPPGC_DCHECK(false),
                            AllOf(HasSubstr(loc.FileName()),
                                  HasSubstr(std::to_string(loc.Line() + 3))));
  EXPECT_DEATH_IF_SUPPORTED(CPPGC_CHECK(false),
                            AllOf(HasSubstr(loc.FileName()),
                                  HasSubstr(std::to_string(loc.Line() + 6))));
}
#endif  // CPPGC_SUPPORTS_SOURCE_LOCATION

#endif  // DEBUG

}  // namespace internal
}  // namespace cppgc

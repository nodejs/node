// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/source-location.h"

#include "src/base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {
constexpr char kFileName[] = "source-location-unittest.cc";

bool Contains(const std::string& base_string, const std::string& substring) {
  return base_string.find(substring) != std::string::npos;
}

}  // namespace

TEST(SourceLocationTest, DefaultCtor) {
  constexpr SourceLocation loc;
  EXPECT_EQ(nullptr, loc.Function());
  EXPECT_EQ(nullptr, loc.FileName());
  EXPECT_EQ(0u, loc.Line());
}

void TestSourceLocationCurrent() {
  static constexpr char kFunctionName[] = "TestSourceLocationCurrent";
  static constexpr size_t kNextLine = __LINE__ + 1;
  constexpr auto loc = SourceLocation::Current();
#if !V8_SUPPORTS_SOURCE_LOCATION
  EXPECT_EQ(nullptr, loc.Function());
  EXPECT_EQ(nullptr, loc.FileName());
  EXPECT_EQ(0u, loc.Line());
  USE(kNextLine);
  return;
#endif
  EXPECT_EQ(kNextLine, loc.Line());
  EXPECT_TRUE(Contains(loc.FileName(), kFileName));
  EXPECT_TRUE(Contains(loc.Function(), kFunctionName));
}

TEST(SourceLocationTest, Current) { TestSourceLocationCurrent(); }

void TestToString() {
  static const std::string kDescriptor = std::string(__func__) + "@" +
                                         __FILE__ + ":" +
                                         std::to_string(__LINE__ + 1);
  constexpr auto loc = SourceLocation::Current();
  const auto string = loc.ToString();
  EXPECT_EQ(kDescriptor, string);
}

#if V8_SUPPORTS_SOURCE_LOCATION
TEST(SourceLocationTest, ToString) { TestToString(); }
#endif

}  // namespace internal
}  // namespace cppgc

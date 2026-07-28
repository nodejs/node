// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/source-location.h"

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
  EXPECT_EQ("", loc.Function());
  EXPECT_EQ("", loc.FileName());
  EXPECT_EQ(0u, loc.Line());
  EXPECT_EQ(false, loc);
}

void TestSourceLocationCurrent() {
  static constexpr char kFunctionName[] = "TestSourceLocationCurrent";
  static constexpr size_t kNextLine = __LINE__ + 1;
  constexpr auto loc = SourceLocation::Current();
  EXPECT_EQ(true, loc);
  EXPECT_EQ(kNextLine, loc.Line());
  EXPECT_TRUE(Contains(loc.FileName(), kFileName));
  EXPECT_TRUE(Contains(loc.Function(), kFunctionName));
}

TEST(SourceLocationTest, Current) { TestSourceLocationCurrent(); }

void TestToString() {
  static const std::string kDescriptor =
      "void cppgc::internal::TestToString()@" __FILE__ ":" +
      std::to_string(__LINE__ + 1);
  constexpr auto loc = SourceLocation::Current();
  const auto string = loc.ToString();
  EXPECT_EQ(kDescriptor, string);
}

TEST(SourceLocationTest, ToString) { TestToString(); }

}  // namespace internal
}  // namespace cppgc

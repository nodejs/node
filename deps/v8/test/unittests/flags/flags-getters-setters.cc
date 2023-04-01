// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>
#include <string>

#include "include/v8-initialization.h"
#include "testing/gtest/include/gtest/gtest.h"

std::vector<std::string> v8_available_flags = v8::V8::GetFlagsNames();

TEST(FlagGetterTest, FlagsGetter) {
  // All flags should be normalized (using '-' instead of '_')
  for (std::string& flag : v8_available_flags) {
    EXPECT_EQ(flag.find('_'), std::string::npos);
  }
}

TEST(FlagGetterTest, FlagsComment) {
  // All flags should be normalized (using '-' instead of '_')
  std::vector<std::string> v8_available_flags = v8::V8::GetFlagsNames();
  for (std::string& flag : v8_available_flags) {
    ASSERT_NE(v8::V8::GetFlagComment(flag.c_str()), nullptr);
  }
}

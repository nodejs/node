/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/containers/null_term_string_view.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

TEST(NullTermStringViewTest, Comparisions) {
  // Test the < operator.
  EXPECT_FALSE(NullTermStringView() < NullTermStringView());
  EXPECT_FALSE(NullTermStringView() < NullTermStringView(""));
  EXPECT_TRUE(NullTermStringView() < NullTermStringView("foo"));
  EXPECT_TRUE(NullTermStringView("") < NullTermStringView("foo"));
  EXPECT_FALSE(NullTermStringView("foo") < NullTermStringView("foo"));
  EXPECT_TRUE(NullTermStringView("foo") < NullTermStringView("fooo"));
  EXPECT_FALSE(NullTermStringView("fooo") < NullTermStringView("foo"));
  EXPECT_TRUE(NullTermStringView("bar") < NullTermStringView("foo"));

  // Test the <= operator.
  EXPECT_TRUE(NullTermStringView() <= NullTermStringView());
  EXPECT_TRUE(NullTermStringView() <= NullTermStringView(""));
  EXPECT_TRUE(NullTermStringView() <= NullTermStringView("foo"));
  EXPECT_TRUE(NullTermStringView("") <= NullTermStringView("foo"));
  EXPECT_TRUE(NullTermStringView("foo") <= NullTermStringView("foo"));
  EXPECT_TRUE(NullTermStringView("foo") <= NullTermStringView("fooo"));
  EXPECT_FALSE(NullTermStringView("fooo") <= NullTermStringView("foo"));
  EXPECT_TRUE(NullTermStringView("bar") <= NullTermStringView("foo"));

  // Test the > operator.
  EXPECT_FALSE(NullTermStringView() > NullTermStringView());
  EXPECT_FALSE(NullTermStringView() > NullTermStringView(""));
  EXPECT_FALSE(NullTermStringView() > NullTermStringView("foo"));
  EXPECT_FALSE(NullTermStringView("") > NullTermStringView("foo"));
  EXPECT_FALSE(NullTermStringView("foo") > NullTermStringView("foo"));
  EXPECT_FALSE(NullTermStringView("foo") > NullTermStringView("fooo"));
  EXPECT_TRUE(NullTermStringView("fooo") > NullTermStringView("foo"));
  EXPECT_FALSE(NullTermStringView("bar") > NullTermStringView("foo"));

  // Test the >= operator.
  EXPECT_TRUE(NullTermStringView() >= NullTermStringView());
  EXPECT_TRUE(NullTermStringView() >= NullTermStringView(""));
  EXPECT_FALSE(NullTermStringView() >= NullTermStringView("foo"));
  EXPECT_FALSE(NullTermStringView("") >= NullTermStringView("foo"));
  EXPECT_TRUE(NullTermStringView("foo") >= NullTermStringView("foo"));
  EXPECT_FALSE(NullTermStringView("foo") >= NullTermStringView("fooo"));
  EXPECT_TRUE(NullTermStringView("fooo") >= NullTermStringView("foo"));
  EXPECT_FALSE(NullTermStringView("bar") >= NullTermStringView("foo"));
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto

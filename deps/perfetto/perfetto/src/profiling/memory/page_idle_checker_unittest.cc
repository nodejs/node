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

#include "src/profiling/memory/page_idle_checker.h"

#include "perfetto/ext/base/utils.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace profiling {
namespace {

TEST(PageIdleCheckerTest, AllOnFirstPageAligned) {
  EXPECT_EQ(GetFirstPageShare(0, 10), 10u);
}

TEST(PageIdleCheckerTest, AllOnFirstPageUnAligned) {
  EXPECT_EQ(GetFirstPageShare(10, 10), 10u);
}

TEST(PageIdleCheckerTest, WholeFirstPageAligned) {
  EXPECT_EQ(GetFirstPageShare(0, base::kPageSize + 10), base::kPageSize);
  EXPECT_EQ(GetLastPageShare(0, base::kPageSize + 10), 10u);
}

TEST(PageIdleCheckerTest, WholeLastPageAligned) {
  EXPECT_EQ(GetFirstPageShare(0, 3 * base::kPageSize), base::kPageSize);
  EXPECT_EQ(GetLastPageShare(0, 3 * base::kPageSize), base::kPageSize);
}

TEST(PageIdleCheckerTest, SomeFirstAndLast) {
  EXPECT_EQ(GetFirstPageShare(10, 3 * base::kPageSize + 10),
            base::kPageSize - 10);
  EXPECT_EQ(GetLastPageShare(10, 3 * base::kPageSize + 10), 20u);
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto

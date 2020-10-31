/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/traced/probes/filesystem/range_tree.h"
#include "src/traced/probes/filesystem/prefix_finder.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/temp_file.h"
#include "perfetto/ext/base/utils.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

using ::testing::Contains;
using ::testing::Not;

TEST(RangeTreeTest, Basic) {
  PrefixFinder pr(1);
  pr.AddPath("/a/foo");
  pr.AddPath("/b/foo");
  pr.AddPath("/c/foo");
  pr.AddPath("/d/foo");
  pr.Finalize();

  auto a = pr.GetPrefix("/a/foo");
  auto b = pr.GetPrefix("/b/foo");
  auto c = pr.GetPrefix("/c/foo");
  auto d = pr.GetPrefix("/d/foo");

  ASSERT_EQ(a->ToString(), "/a");
  ASSERT_EQ(b->ToString(), "/b");
  ASSERT_EQ(c->ToString(), "/c");
  ASSERT_EQ(d->ToString(), "/d");

  // This test needs to be changed for other kSetSize.
  ASSERT_EQ(kSetSize, 3u);

  RangeTree t;
  t.Insert(1, a);
  t.Insert(2, a);
  t.Insert(20, b);
  t.Insert(24, a);
  t.Insert(25, c);
  t.Insert(27, d);

  EXPECT_THAT(t.Get(1), Contains("/a"));
  EXPECT_THAT(t.Get(2), Contains("/a"));
  EXPECT_THAT(t.Get(20), Contains("/b"));
  EXPECT_THAT(t.Get(24), Contains("/a"));
  EXPECT_THAT(t.Get(25), Contains("/c"));
  EXPECT_THAT(t.Get(27), Contains("/d"));
  // 27 will have overflowed kSetSize = 3;
  EXPECT_THAT(t.Get(27), Not(Contains("/a")));
  EXPECT_THAT(t.Get(27), Not(Contains("/b")));
  EXPECT_THAT(t.Get(27), Not(Contains("/c")));
}

}  // namespace
}  // namespace perfetto

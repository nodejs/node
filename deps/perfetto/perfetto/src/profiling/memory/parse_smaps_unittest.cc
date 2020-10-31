/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "perfetto/profiling/parse_smaps.h"

#include "perfetto/ext/base/scoped_file.h"
#include "src/base/test/utils.h"
#include "test/gtest_and_gmock.h"

#include <inttypes.h>

namespace perfetto {
namespace profiling {

bool operator==(const SmapsEntry& a, const SmapsEntry& b);
bool operator==(const SmapsEntry& a, const SmapsEntry& b) {
  return a.pathname == b.pathname && a.size_kb == b.size_kb &&
         a.private_dirty_kb == b.private_dirty_kb && a.swap_kb == b.swap_kb;
}

namespace {

using ::testing::ElementsAre;

TEST(ParseSmapsTest, Smoke) {
  base::ScopedFstream fd(fopen(
      base::GetTestDataPath("src/profiling/memory/test/data/cat_smaps").c_str(),
      "r"));
  std::vector<SmapsEntry> entries;
  EXPECT_TRUE(ParseSmaps(
      *fd, [&entries](const SmapsEntry& e) { entries.emplace_back(e); }));

  SmapsEntry cat1;
  cat1.pathname = "/bin/cat";
  cat1.size_kb = 8;
  cat1.private_dirty_kb = 0;
  cat1.swap_kb = 0;
  SmapsEntry cat2;
  cat2.pathname = "/bin/cat";
  cat2.size_kb = 8;
  cat2.private_dirty_kb = 0;
  cat2.swap_kb = 0;
  SmapsEntry heap;
  heap.pathname = "[heap stuff]";
  heap.size_kb = 132;
  heap.private_dirty_kb = 8;
  heap.swap_kb = 4;
  EXPECT_THAT(entries, ElementsAre(cat1, cat2, heap));
}

TEST(ParseSmapsTest, SmokeNoEol) {
  base::ScopedFstream fd(fopen(
      base::GetTestDataPath("src/profiling/memory/test/data/cat_smaps_noeol")
          .c_str(),
      "r"));
  std::vector<SmapsEntry> entries;
  EXPECT_TRUE(ParseSmaps(
      *fd, [&entries](const SmapsEntry& e) { entries.emplace_back(e); }));

  SmapsEntry cat1;
  cat1.pathname = "/bin/cat";
  cat1.size_kb = 8;
  cat1.private_dirty_kb = 0;
  cat1.swap_kb = 0;
  SmapsEntry cat2;
  cat2.pathname = "/bin/cat";
  cat2.size_kb = 8;
  cat2.private_dirty_kb = 0;
  cat2.swap_kb = 0;
  SmapsEntry heap;
  heap.pathname = "[heap stuff]";
  heap.size_kb = 132;
  heap.private_dirty_kb = 8;
  heap.swap_kb = 4;
  EXPECT_THAT(entries, ElementsAre(cat1, cat2, heap));
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto

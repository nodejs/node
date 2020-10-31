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

#include "perfetto/base/flat_set.h"

#include <random>
#include <set>

#include "test/gtest_and_gmock.h"

using testing::ElementsAre;
using testing::ElementsAreArray;

namespace perfetto {
namespace base {
namespace {

TEST(FlatSetTest, InsertAndLookup) {
  FlatSet<long> flat_set;
  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(flat_set.empty());
    EXPECT_EQ(flat_set.size(), 0u);
    EXPECT_EQ(flat_set.begin(), flat_set.end());
    EXPECT_FALSE(flat_set.count(42));
    EXPECT_EQ(flat_set.find(42), flat_set.end());
    EXPECT_EQ(flat_set.find(42), flat_set.begin());

    auto it_and_inserted = flat_set.insert(1);
    EXPECT_EQ(it_and_inserted.first, flat_set.find(1));
    EXPECT_TRUE(it_and_inserted.second);
    EXPECT_FALSE(flat_set.empty());
    EXPECT_EQ(flat_set.size(), 1u);
    {
      auto it = flat_set.find(1);
      EXPECT_EQ(it, flat_set.begin());
      EXPECT_NE(it, flat_set.end());
      EXPECT_EQ(*it, 1);
    }

    EXPECT_TRUE(flat_set.count(1));
    EXPECT_NE(flat_set.begin(), flat_set.end());
    EXPECT_EQ(std::distance(flat_set.begin(), flat_set.end()), 1);

    it_and_inserted = flat_set.insert(1);
    EXPECT_EQ(it_and_inserted.first, flat_set.find(1));
    EXPECT_FALSE(it_and_inserted.second);
    EXPECT_EQ(flat_set.size(), 1u);
    EXPECT_TRUE(flat_set.count(1));
    EXPECT_FALSE(flat_set.count(0));
    EXPECT_FALSE(flat_set.count(-1));

    EXPECT_TRUE(flat_set.erase(1));
    EXPECT_FALSE(flat_set.count(1));
    EXPECT_EQ(flat_set.size(), 0u);

    EXPECT_TRUE(flat_set.insert(7).second);
    EXPECT_TRUE(flat_set.insert(-4).second);
    EXPECT_TRUE(flat_set.insert(11).second);
    EXPECT_TRUE(flat_set.insert(-13).second);
    EXPECT_TRUE(flat_set.count(7));
    EXPECT_TRUE(flat_set.count(-4));
    EXPECT_TRUE(flat_set.count(11));
    EXPECT_TRUE(flat_set.count(-13));
    EXPECT_THAT(flat_set, ElementsAre(-13, -4, 7, 11));

    EXPECT_TRUE(flat_set.erase(-4));
    EXPECT_TRUE(flat_set.erase(7));
    EXPECT_THAT(flat_set, ElementsAre(-13, 11));

    flat_set.clear();
  }

  // Test that the initializer-ctor dedupes entries.
  flat_set = FlatSet<long>({1, 2, 1, 2, 3});
  EXPECT_EQ(flat_set.size(), 3u);
  EXPECT_THAT(flat_set, ElementsAre(1, 2, 3));
}

TEST(FlatSetTest, GoldenTest) {
  FlatSet<int> flat_set;
  std::set<int> gold_set;
  static std::minstd_rand rng(0);
  std::uniform_int_distribution<int> int_dist(-200, 200);

  for (int i = 0; i < 10000; i++) {
    const int val = int_dist(rng);
    if (i % 3) {
      auto flat_result = flat_set.insert(val);
      auto gold_result = gold_set.insert(val);
      EXPECT_EQ(flat_result.first, flat_set.find(val));
      EXPECT_EQ(gold_result.first, gold_set.find(val));
      EXPECT_EQ(flat_result.second, gold_result.second);
    } else {
      flat_set.erase(val);
      gold_set.erase(val);
    }
    ASSERT_EQ(flat_set.size(), gold_set.size());
  }

  EXPECT_THAT(flat_set, ElementsAreArray(gold_set));
}

}  // namespace
}  // namespace base
}  // namespace perfetto

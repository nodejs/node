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

#include "src/tracing/core/patch_list.h"

#include <ostream>

#include "test/gtest_and_gmock.h"

using testing::ElementsAre;

namespace perfetto {

std::ostream& operator<<(std::ostream& o, const Patch& p);
std::ostream& operator<<(std::ostream& o, const Patch& p) {
  o << p.chunk_id << "@" << p.offset << " : {" << std::hex << p.size_field[0]
    << "," << p.size_field[1] << "," << p.size_field[2] << ","
    << p.size_field[3] << "}";
  return o;
}

namespace {

TEST(PatchListTest, InsertAndRemove) {
  PatchList pl;

  ASSERT_TRUE(pl.empty());

  pl.emplace_back(ChunkID(5), 50);
  ASSERT_THAT(pl, ElementsAre(Patch(ChunkID(5), 50)));

  pl.emplace_back(ChunkID(6), 60);
  ASSERT_THAT(pl, ElementsAre(Patch(ChunkID(5), 50), Patch(ChunkID(6), 60)));

  ASSERT_EQ(pl.front(), Patch(ChunkID(5), 50));
  ASSERT_EQ(pl.back(), Patch(ChunkID(6), 60));

  pl.pop_front();
  ASSERT_EQ(pl.front(), Patch(ChunkID(6), 60));
  pl.emplace_back(ChunkID(7), 70);

  pl.pop_front();
  ASSERT_EQ(pl.front(), Patch(ChunkID(7), 70));
  ASSERT_EQ(pl.back(), Patch(ChunkID(7), 70));

  pl.pop_front();

  for (int i = 0; i < 3; i++) {
    ASSERT_TRUE(pl.empty());

    pl.emplace_back(ChunkID(8), 80);
    pl.emplace_back(ChunkID(9), 90);
    ASSERT_THAT(pl, ElementsAre(Patch(ChunkID(8), 80), Patch(ChunkID(9), 90)));

    pl.pop_front();
    pl.pop_front();
  }
}

TEST(PatchListTest, PointerStability) {
  PatchList pl;
  const uint8_t* ptrs[10]{};
  for (uint16_t i = 0; i < 1000; i++) {
    pl.emplace_back(ChunkID(i), i);
    if (i >= 1000 - 10)
      ptrs[i - (1000 - 10)] = &pl.back().size_field[0];
  }

  for (uint16_t i = 0; i < 1000 - 10; i++)
    pl.pop_front();

  auto it = pl.begin();
  for (uint16_t i = 0; it != pl.end(); it++, i++) {
    EXPECT_EQ(ptrs[i], &it->size_field[0]);
  }
}

}  // namespace
}  // namespace perfetto

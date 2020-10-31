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

#include "src/traced/probes/filesystem/lru_inode_cache.h"

#include <string>
#include <tuple>

#include "protos/perfetto/trace/filesystem/inode_file_map.pbzero.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {

namespace {

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Pointee;

const std::pair<BlockDeviceID, Inode> key1{0, 0};
const std::pair<BlockDeviceID, Inode> key2{0, 1};
const std::pair<BlockDeviceID, Inode> key3{0, 2};

InodeMapValue val1() {
  return InodeMapValue(protos::pbzero::InodeFileMap_Entry_Type_DIRECTORY,
                       std::set<std::string>{"Value 1"});
}

InodeMapValue val2() {
  return InodeMapValue(protos::pbzero::InodeFileMap_Entry_Type_UNKNOWN,
                       std::set<std::string>{"Value 2"});
}

InodeMapValue val3() {
  return InodeMapValue(protos::pbzero::InodeFileMap_Entry_Type_UNKNOWN,
                       std::set<std::string>{"Value 2"});
}

TEST(LRUInodeCacheTest, Basic) {
  LRUInodeCache cache(2);
  cache.Insert(key1, val1());
  EXPECT_THAT(cache.Get(key1), Pointee(Eq(val1())));
  cache.Insert(key2, val2());
  EXPECT_THAT(cache.Get(key1), Pointee(Eq(val1())));
  EXPECT_THAT(cache.Get(key2), Pointee(Eq(val2())));
  cache.Insert(key1, val2());
  EXPECT_THAT(cache.Get(key1), Pointee(Eq(val2())));
}

TEST(LRUInodeCacheTest, Overflow) {
  LRUInodeCache cache(2);
  cache.Insert(key1, val1());
  cache.Insert(key2, val2());
  EXPECT_THAT(cache.Get(key1), Pointee(Eq(val1())));
  EXPECT_THAT(cache.Get(key2), Pointee(Eq(val2())));
  cache.Insert(key3, val3());
  // key1 is the LRU and should be evicted.
  EXPECT_THAT(cache.Get(key1), IsNull());
  EXPECT_THAT(cache.Get(key2), Pointee(Eq(val2())));
  EXPECT_THAT(cache.Get(key3), Pointee(Eq(val3())));
}

}  // namespace
}  // namespace perfetto

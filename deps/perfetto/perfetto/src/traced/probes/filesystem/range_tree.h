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

#include <map>
#include <set>
#include <sstream>

#include <stdio.h>

#include "perfetto/ext/base/small_set.h"
#include "src/traced/probes/filesystem/inode_file_data_source.h"
#include "src/traced/probes/filesystem/prefix_finder.h"

#ifndef SRC_TRACED_PROBES_FILESYSTEM_RANGE_TREE_H_
#define SRC_TRACED_PROBES_FILESYSTEM_RANGE_TREE_H_

namespace perfetto {
namespace {

constexpr size_t kSetSize = 3;

}  // namespace

// Keep key value associations in ranges. Keeps kSetSize=3 possible values
// for every key, where one is the correct one.
// For instance,
// 1 -> a
// 2 -> b
// 3 -> c
// 4 -> d
//
// will be stored as
// [1, 4) {a, b, c}
// [4, inf) {d}
//
// This comes from the observation that close-by inode numbers tend to be
// in the same directory. We are storing multiple values to be able to
// aggregate to larger ranges and reduce memory usage.
class RangeTree {
 public:
  using DataType = PrefixFinder::Node*;

  const std::set<std::string> Get(Inode inode);
  void Insert(Inode inode, DataType value);

 private:
  std::map<Inode, SmallSet<DataType, kSetSize>> map_;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FILESYSTEM_RANGE_TREE_H_

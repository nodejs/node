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

#ifndef INCLUDE_PERFETTO_EXT_TRACED_DATA_SOURCE_TYPES_H_
#define INCLUDE_PERFETTO_EXT_TRACED_DATA_SOURCE_TYPES_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <set>
#include <string>

namespace perfetto {

// On ARM, st_ino is not ino_t but unsigned long long.
using Inode = decltype(stat::st_ino);

// On ARM, st_dev is not dev_t but unsigned long long.
using BlockDeviceID = decltype(stat::st_dev);

using InodeBlockPair = std::pair<Inode, BlockDeviceID>;

// From inode_file_map.pbzero.h
using InodeFileMap_Entry_Type = int32_t;

class InodeMapValue {
 public:
  InodeMapValue(InodeFileMap_Entry_Type entry_type, std::set<std::string> paths)
      : entry_type_(entry_type), paths_(std::move(paths)) {}

  InodeMapValue() {}

  InodeFileMap_Entry_Type type() const { return entry_type_; }
  const std::set<std::string>& paths() const { return paths_; }
  void SetType(InodeFileMap_Entry_Type entry_type) { entry_type_ = entry_type; }
  void SetPaths(std::set<std::string> paths) { paths_ = std::move(paths); }
  void AddPath(std::string path) { paths_.emplace(std::move(path)); }

  bool operator==(const perfetto::InodeMapValue& rhs) const {
    return type() == rhs.type() && paths() == rhs.paths();
  }

 private:
  InodeFileMap_Entry_Type entry_type_;
  std::set<std::string> paths_;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_TRACED_DATA_SOURCE_TYPES_H_

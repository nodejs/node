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

#ifndef SRC_TRACED_PROBES_FILESYSTEM_LRU_INODE_CACHE_H_
#define SRC_TRACED_PROBES_FILESYSTEM_LRU_INODE_CACHE_H_

#include <list>
#include <map>
#include <string>
#include <tuple>

#include "perfetto/ext/traced/data_source_types.h"

namespace perfetto {

// LRUInodeCache keeps up to |capacity| entries in a mapping from InodeKey
// to InodeMapValue. This is used to map <block device, inode> tuples to file
// paths.
class LRUInodeCache {
 public:
  using InodeKey = std::pair<BlockDeviceID, Inode>;

  explicit LRUInodeCache(size_t capacity) : capacity_(capacity) {}

  InodeMapValue* Get(const InodeKey& k);
  void Insert(InodeKey k, InodeMapValue v);

 private:
  using ItemType = std::pair<const InodeKey, InodeMapValue>;
  using ListIteratorType = std::list<ItemType>::iterator;
  using MapType = std::map<const InodeKey, ListIteratorType>;

  void Insert(MapType::iterator map_it, InodeKey k, InodeMapValue v);

  const size_t capacity_;
  MapType map_;
  std::list<ItemType> list_;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FILESYSTEM_LRU_INODE_CACHE_H_

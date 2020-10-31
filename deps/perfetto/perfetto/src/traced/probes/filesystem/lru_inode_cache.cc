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

namespace perfetto {

InodeMapValue* LRUInodeCache::Get(const InodeKey& k) {
  const auto& map_it = map_.find(k);
  if (map_it == map_.end()) {
    return nullptr;
  }
  auto list_entry = map_it->second;
  // Bump this item to the front of the cache.
  // We can borrow both elements of the pair stored in the list because
  // insert does not need them.
  Insert(map_it, std::move(list_entry->first), std::move(list_entry->second));
  return &list_.begin()->second;
}

void LRUInodeCache::Insert(InodeKey k, InodeMapValue v) {
  auto it = map_.find(k);
  return Insert(it, std::move(k), std::move(v));
}

void LRUInodeCache::Insert(typename MapType::iterator map_it,
                           InodeKey k,
                           InodeMapValue v) {
  list_.emplace_front(k, std::move(v));
  if (map_it != map_.end()) {
    ListIteratorType& list_it = map_it->second;
    list_.erase(list_it);
    list_it = list_.begin();
  } else {
    map_.emplace(std::move(k), list_.begin());
  }

  if (map_.size() > capacity_) {
    auto list_last_it = list_.end();
    list_last_it--;
    map_.erase(list_last_it->first);
    list_.erase(list_last_it);
  }
}
}  // namespace perfetto

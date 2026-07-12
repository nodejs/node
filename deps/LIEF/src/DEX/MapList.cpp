/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "LIEF/DEX/MapList.hpp"
#include "LIEF/DEX/hash.hpp"
#include "logging.hpp"

namespace LIEF {
namespace DEX {

MapList::MapList() = default;
MapList::MapList(const MapList& other) = default;

MapList& MapList::operator=(const MapList&) = default;

MapList::it_items_t MapList::items() {
  std::vector<MapItem*> items;
  items.reserve(items_.size());
  std::transform(std::begin(items_), std::end(items_),
                 std::back_inserter(items),
                 [] (MapList::items_t::value_type& p) -> MapItem* {
                   return &(p.second);
                 });
  return items;

}

MapList::it_const_items_t MapList::items() const {
  std::vector<MapItem*> items;
  items.reserve(items_.size());
  std::transform(std::begin(items_), std::end(items_),
                 std::back_inserter(items),
                 [] (const MapList::items_t::value_type& p) -> MapItem* {
                   return const_cast<MapItem*>(&(p.second));
                 });
  return items;

}


bool MapList::has(MapItem::TYPES type) const {
  return items_.count(type) > 0;
}

const MapItem& MapList::get(MapItem::TYPES type) const {
  const auto it = items_.find(type);
  CHECK(it != std::end(items_), "Can't find type!");
  return it->second;
}

MapItem& MapList::get(MapItem::TYPES type) {
  return const_cast<MapItem&>(static_cast<const MapList*>(this)->get(type));
}

const MapItem& MapList::operator[](MapItem::TYPES type) const {
  return get(type);
}

MapItem& MapList::operator[](MapItem::TYPES type) {
  return get(type);
}

void MapList::accept(Visitor& visitor) const {
  visitor.visit(*this);
}



std::ostream& operator<<(std::ostream& os, const MapList& mlist) {
  for (const MapItem& item : mlist.items()) {
    os << item << '\n';
  }
  return os;
}


MapList::~MapList() = default;

}
}

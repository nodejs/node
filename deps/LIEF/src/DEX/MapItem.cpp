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
#include "LIEF/DEX/MapItem.hpp"
#include "LIEF/DEX/hash.hpp"

#include "LIEF/DEX/EnumToString.hpp"

namespace LIEF {
namespace DEX {

MapItem::MapItem() = default;
MapItem::MapItem(const MapItem& other) = default;
MapItem& MapItem::operator=(const MapItem&) = default;

MapItem::MapItem(MapItem::TYPES type, uint32_t offset, uint32_t size, uint16_t reserved) :
  type_{type},
  reserved_{reserved},
  size_{size},
  offset_{offset}
{}

MapItem::TYPES MapItem::type() const {
  return type_;
}

uint16_t MapItem::reserved() const {
  return reserved_;
}

uint32_t MapItem::size() const {
  return size_;
}

uint32_t MapItem::offset() const {
  return offset_;
}

void MapItem::accept(Visitor& visitor) const {
  visitor.visit(*this);
}



std::ostream& operator<<(std::ostream& os, const MapItem& mitem) {
  os << to_string(mitem.type())
     << "@" << std::hex << std::showbase << mitem.offset()
     << " (" << mitem.size() << " bytes) - " << mitem.reserved();
  return os;
}


MapItem::~MapItem() = default;

}
}

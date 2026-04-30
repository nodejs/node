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

#include <utility>

#include "LIEF/utils.hpp"
#include "LIEF/Visitor.hpp"

#include "LIEF/PE/resources/ResourceStringTable.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "logging.hpp"

namespace LIEF {
namespace PE {

result<ResourceStringTable::entry_t> parse_string(BinaryStream& stream) {
  // typedef struct {
  //   WORD  wLength;
  //   WORD  wValueLength;
  //   WORD  wType;
  //   WCHAR szKey;
  //   WORD  Padding;
  //   WORD  Value;
  // } String;
  auto wLength = stream.read<uint16_t>();
  if (!wLength) { return make_error_code(wLength.error()); }

  if (*wLength == 0) {
    return make_error_code(lief_errors::read_error);
  }

  const uint32_t end_offset = stream.pos() - sizeof(uint16_t) + *wLength;

  auto wValueLength = stream.read<uint16_t>();
  if (!wValueLength) { return make_error_code(wValueLength.error()); }

  auto wType = stream.read<uint16_t>();
  if (!wType) { return make_error_code(wType.error()); }

  if (*wType != 0 && wType != 1) {
    return make_error_code(lief_errors::corrupted);
  }

  ResourceStringTable::entry_t entry;

  auto szKey = stream.read_u16string();
  if (!szKey) { return make_error_code(szKey.error()); }

  entry.key = std::move(*szKey);

  if (stream.pos() >= end_offset) {
    return entry;
  }

  stream.align(sizeof(uint32_t));

  if (stream.pos() >= end_offset) {
    return entry;
  }

  auto Value = stream.read_u16string();
  if (!Value) { return entry; }

  entry.value = std::move(*Value);

  stream.setpos(end_offset);
  return entry;
}

result<ResourceStringTable> ResourceStringTable::parse(BinaryStream& stream) {
  // typedef struct {
  //   WORD   wLength;
  //   WORD   wValueLength;
  //   WORD   wType;
  //   WCHAR  szKey;
  //   WORD   Padding;
  //   String Children;
  // } StringTable;
  ResourceStringTable table;
  auto wLength = stream.read<uint16_t>();
  if (!wLength) { return make_error_code(wLength.error()); }

  auto wValueLength = stream.read<uint16_t>();
  if (!wValueLength) { return make_error_code(wValueLength.error()); }

  auto wType = stream.read<uint16_t>();
  if (!wType) { return make_error_code(wType.error()); }

  if (*wType != 0 && wType != 1) {
    return make_error_code(lief_errors::corrupted);
  }

  auto szKey = stream.read_u16string();
  if (!szKey) { return make_error_code(szKey.error()); }

  table.key(std::move(*szKey));

  while (stream) {
    stream.align(sizeof(uint32_t));
    auto str = parse_string(stream);
    if (!str) {
      return table;
    }

    table.add_entry(std::move(*str));
  }
  return table;
}

std::string ResourceStringTable::entry_t::key_u8() const {
  return u16tou8(key);
}

std::string ResourceStringTable::entry_t::value_u8() const {
  return u16tou8(value);
}

std::string ResourceStringTable::key_u8() const {
  return u16tou8(key());
}

optional<std::string> ResourceStringTable::get(const std::string& key) const {
  auto u16 = u8tou16(key);
  if (!u16) {
    return nullopt();
  }

  if (auto value = get(*u16)) {
    return u16tou8(*value);
  }

  return nullopt();
}

void ResourceStringTable::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const ResourceStringTable& table) {
  os << fmt::format("BLOCK '{}' {{\n", table.key_u8());
  for (const ResourceStringTable::entry_t& entry : table.entries()) {
    os << "  " << entry << '\n';
  }
  os << '}';
  return os;
}

}
}

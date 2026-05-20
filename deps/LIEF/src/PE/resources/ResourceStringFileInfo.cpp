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
#include <sstream>
#include "LIEF/Visitor.hpp"

#include "LIEF/utils.hpp"

#include "LIEF/PE/resources/ResourceStringFileInfo.hpp"
#include "LIEF/PE/resources/ResourceStringTable.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "logging.hpp"
#include "internal_utils.hpp"

namespace LIEF {
namespace PE {


result<ResourceStringFileInfo> ResourceStringFileInfo::parse(BinaryStream& stream) {
  // typedef struct {
  //   WORD        wLength;
  //   WORD        wValueLength;
  //   WORD        wType;
  //   WCHAR       szKey;
  //   WORD        Padding;
  //   StringTable Children;
  // } StringFileInfo;
  ResourceStringFileInfo info;
  auto wLength = stream.read<uint16_t>();
  if (!wLength) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return make_error_code(wLength.error());
  }

  auto wValueLength = stream.read<uint16_t>();
  if (!wValueLength) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return make_error_code(wValueLength.error());
  }

  auto wType = stream.read<uint16_t>();
  if (!wType) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return make_error_code(wType.error());
  }

  if (*wType != 0 && wType != 1) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return make_error_code(lief_errors::corrupted);
  }

  auto szKey = stream.read_u16string();
  if (!szKey) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return make_error_code(wType.error());
  }

  if (u16tou8(*szKey) != "StringFileInfo") {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return make_error_code(lief_errors::corrupted);
  }

  info
    .type(*wType)
    .key(std::move(*szKey));

  stream.align(sizeof(uint32_t));
  while (stream) {
    auto item = ResourceStringTable::parse(stream);
    if (!item) {
      LIEF_WARN("Can't parse StringFileInfo.Children[{}]", info.children_.size());
      break;
    }
    info.add_child(std::move(*item));
  }
  return info;
}


std::string ResourceStringFileInfo::key_u8() const {
  return u16tou8(key());
}

void ResourceStringFileInfo::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const ResourceStringFileInfo& info) {
  os << fmt::format("BLOCK '{}' {{\n", info.key_u8());
  for (const ResourceStringTable& table : info.children()) {
    std::ostringstream oss;
    oss << table;
    os << indent(oss.str(), 2);
  }
  os << '}';
  return os;
}


}
}

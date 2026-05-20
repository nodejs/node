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

#include "LIEF/utils.hpp"
#include "LIEF/Visitor.hpp"

#include "LIEF/PE/resources/ResourceVarFileInfo.hpp"
#include "LIEF/PE/resources/ResourceVar.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "internal_utils.hpp"
#include "logging.hpp"

namespace LIEF {
namespace PE {

result<ResourceVarFileInfo> ResourceVarFileInfo::parse(BinaryStream& stream) {
  ResourceVarFileInfo info;
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
  if (!szKey) { return make_error_code(wType.error()); }

  if (u16tou8(*szKey) != "VarFileInfo") {
    return make_error_code(lief_errors::corrupted);
  }

  info
    .type(*wType)
    .key(std::move(*szKey));


  while (stream) {
    stream.align(sizeof(uint32_t));
    auto var = ResourceVar::parse(stream);
    if (!var) {
      LIEF_WARN("Can't parse resource var #{}", info.vars_.size());
      return info;
    }
    info.add_var(std::move(*var));
  }

  return info;
}

void ResourceVarFileInfo::accept(Visitor& visitor) const {
  visitor.visit(*this);
}


std::string ResourceVarFileInfo::key_u8() const {
  return u16tou8(key());
}

std::ostream& operator<<(std::ostream& os, const ResourceVarFileInfo& info) {
  os << fmt::format("BLOCK '{}' {{\n", info.key_u8());
  for (const ResourceVar& var : info.vars()) {
    std::ostringstream oss;
    oss << var;
    os << indent(oss.str(), 2);
  }
  os << '}';
  return os;
}


}
}

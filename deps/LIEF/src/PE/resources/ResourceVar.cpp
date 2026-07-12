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
#include "logging.hpp"

#include "LIEF/utils.hpp"
#include "LIEF/Visitor.hpp"

#include "LIEF/PE/resources/ResourceVar.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

namespace LIEF {
namespace PE {

result<ResourceVar> ResourceVar::parse(BinaryStream& stream) {
  // typedef struct {
  //   WORD  wLength;
  //   WORD  wValueLength;
  //   WORD  wType;
  //   WCHAR szKey;
  //   WORD  Padding;
  //   DWORD Value;
  // } Var;
  ResourceVar var;
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

  auto szKey = stream.read_u16string();
  if (!szKey) { return make_error_code(wType.error()); }

  if (u16tou8(*szKey) != "Translation") {
    return make_error_code(lief_errors::corrupted);
  }

  var
    .key(std::move(*szKey))
    .type(*wType);

  stream.align(sizeof(uint32_t));

  const size_t nb_values = *wValueLength / sizeof(uint32_t);
  LIEF_DEBUG("Nb vars: {}", nb_values);

  for (size_t i = 0; i < nb_values; ++i) {
    auto value = stream.read<uint32_t>();
    if (!value) {
      LIEF_WARN("Can't parse value #{:02}", i);
      return var;
    }

    var.add_value(*value);
  }

  stream.setpos(end_offset);
  return var;
}


std::string ResourceVar::key_u8() const {
  return u16tou8(key());
}

std::ostream& operator<<(std::ostream& os, const ResourceVar& var) {
  std::vector<std::string> values;
  values.reserve(var.values_.size());
  for (uint32_t val : var.values()) {
    values.push_back(fmt::format("0x{:08x}", val));
  }

  os << fmt::format("{}: {}", var.key_u8(), fmt::join(values, ", "));
  return os;
}


}
}

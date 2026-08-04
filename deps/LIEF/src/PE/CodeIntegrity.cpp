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
#include <spdlog/fmt/fmt.h>

#include "LIEF/PE/CodeIntegrity.hpp"
#include "LIEF/Visitor.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

namespace LIEF {
namespace PE {

result<CodeIntegrity> CodeIntegrity::parse(Parser&, BinaryStream& stream) {
  auto flags = stream.read<uint16_t>();
  if (!flags) {
    return make_error_code(flags.error());
  }

  auto catalog = stream.read<uint16_t>();
  if (!catalog) {
    return make_error_code(catalog.error());
  }

  auto catalog_offset = stream.read<uint32_t>();
  if (!catalog_offset) {
    return make_error_code(catalog_offset.error());
  }

  auto reserved = stream.read<uint32_t>();
  if (!reserved) {
    return make_error_code(reserved.error());
  }

  CodeIntegrity code_integrity;

  code_integrity
    .flags(*flags)
    .catalog(*catalog)
    .catalog_offset(*catalog_offset)
    .reserved(*reserved);

  return code_integrity;
}

void CodeIntegrity::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const CodeIntegrity& entry) {
  os << fmt::format("Flags          0x{:x}\n", entry.flags())
     << fmt::format("Catalog        0x{:x}\n", entry.catalog())
     << fmt::format("Catalog offset 0x{:x}\n", entry.catalog_offset())
     << fmt::format("Reserved       0x{:x}\n", entry.reserved());
  return os;
}

}
}

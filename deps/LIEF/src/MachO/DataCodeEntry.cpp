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
#include "LIEF/Visitor.hpp"

#include "LIEF/MachO/DataCodeEntry.hpp"
#include "MachO/Structures.hpp"

#include "frozen.hpp"

namespace LIEF {
namespace MachO {

DataCodeEntry::DataCodeEntry(const details::data_in_code_entry& entry) :
  offset_{entry.offset},
  length_{entry.length},
  type_{static_cast<TYPES>(entry.kind)}
{}

void DataCodeEntry::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const DataCodeEntry& entry) {
  os << fmt::format("{}: offset=0x{:06x}, size=0x{:x}",
                     to_string(entry.type()), entry.offset(), entry.length());
  return os;
}

const char* to_string(DataCodeEntry::TYPES e) {
  #define ENTRY(X) std::pair(DataCodeEntry::TYPES::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(DATA),
    ENTRY(JUMP_TABLE_8),
    ENTRY(JUMP_TABLE_16),
    ENTRY(JUMP_TABLE_32),
    ENTRY(ABS_JUMP_TABLE_32),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}

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
#include "logging.hpp"

#include "LIEF/config.h"
#include "LIEF/utils.hpp"
#include "LIEF/Visitor.hpp"
#include "LIEF/PE/ExportEntry.hpp"

#include "internal_utils.hpp"

namespace LIEF {
namespace PE {
std::string ExportEntry::demangled_name() const {
  logging::needs_lief_extended();

  if constexpr (lief_extended) {
    return LIEF::demangle(name()).value_or("");
  } else {
    return "";
  }
}

void ExportEntry::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const ExportEntry& entry) {
  using namespace fmt;
  os << format("{:04d} {:5} 0x{:08x} {}",
               entry.ordinal(), entry.is_extern() ? "[EXT]" : "",
               entry.address(), entry.name());
  if (entry.is_forwarded()) {
    os << format(" ({})", LIEF::to_string(entry.forward_information()));
  }
  return os;
 }

}
}

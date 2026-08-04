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
#include "LIEF/PE/ImportEntry.hpp"

namespace LIEF {
namespace PE {

std::string ImportEntry::demangled_name() const {
  logging::needs_lief_extended();

  if constexpr (lief_extended) {
    return LIEF::demangle(name()).value_or("");
  } else {
    return "";
  }
}

bool ImportEntry::is_ordinal() const {
  // See: https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#the-idata-section
  const uint64_t ORDINAL_MASK = (type_ == PE_TYPE::PE32) ? 0x80000000 : 0x8000000000000000;
  bool ordinal_bit_is_set = (data_ & ORDINAL_MASK) != 0;

  // Check that bit 31 / 63 is set
  if (!ordinal_bit_is_set) {
    return false;
  }

  // Check that bits 30-15 / 62-15 are set to 0.
  uint64_t val = (data_ & ~ORDINAL_MASK) >> 16;
  return val == 0;
}

void ImportEntry::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const ImportEntry& entry) {
  using namespace fmt;
  os << (!entry.is_ordinal() ?
        format("0x{:04x}: {}", entry.hint(), entry.name()) :
        format("0x{:04x}: {}", entry.hint(), entry.ordinal()));
  return os;
}

} // namespace PE
} // namepsace LIEF

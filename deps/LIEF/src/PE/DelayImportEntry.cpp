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
#include "spdlog/fmt/fmt.h"

#include "logging.hpp"

#include "LIEF/config.h"
#include "LIEF/utils.hpp"
#include "LIEF/Visitor.hpp"

#include "LIEF/PE/DelayImportEntry.hpp"

namespace LIEF {
namespace PE {

std::string DelayImportEntry::demangled_name() const {
  logging::needs_lief_extended();

  if constexpr (lief_extended) {
    return LIEF::demangle(name()).value_or("");
  } else {
    return "";
  }
}

bool DelayImportEntry::is_ordinal() const {
  // See: https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#the-idata-section
  const uint64_t ORDINAL_MASK = type_ == PE_TYPE::PE32 ? 0x80000000 : 0x8000000000000000;
  bool ordinal_bit_is_set = static_cast<bool>(data_ & ORDINAL_MASK);

  // Check that bit 31 / 63 is set
  if (!ordinal_bit_is_set) {
    return false;
  }
  // Check that bits 30-15 / 62-15 are set to 0.
  uint64_t val = (data_ & ~ORDINAL_MASK) >> 15;
  return val == 0;
}

void DelayImportEntry::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const DelayImportEntry& entry) {
  os << fmt::format("0x{:08x} 0x{:04x} {}",
                    entry.iat_value(), entry.hint(),
                    entry.is_ordinal() ? std::to_string(entry.ordinal()) :
                                         entry.name());
  return os;
}

} // namespace PE
} // namepsace LIEF

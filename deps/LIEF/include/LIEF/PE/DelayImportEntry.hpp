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
#ifndef LIEF_PE_DELAY_IMPORT_ENTRY_H
#define LIEF_PE_DELAY_IMPORT_ENTRY_H
#include <ostream>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/Abstract/Symbol.hpp"

#include "LIEF/PE/enums.hpp"

namespace LIEF {
namespace PE {
class Parser;
class Builder;

/// Class that represents an entry (i.e. an import) in the delay import table (DelayImport).
///
/// It extends the LIEF::Symbol generic class that exposes the LIEF::Symbol::name and
/// LIEF::Symbol::value API.
///
/// The meaning of LIEF::Symbol::value for this PE object is the address (as an RVA) in the IAT
/// where the resolution should take place.
class LIEF_API DelayImportEntry : public LIEF::Symbol {
  friend class Parser;
  friend class Builder;

  public:
  DelayImportEntry() = default;
  DelayImportEntry(uint64_t data, PE_TYPE type) :
    data_(data),
    type_(type)
  {}

  DelayImportEntry(const DelayImportEntry&) = default;
  DelayImportEntry& operator=(const DelayImportEntry&) = default;

  DelayImportEntry(DelayImportEntry&&) noexcept = default;
  DelayImportEntry& operator=(DelayImportEntry&&) noexcept = default;

  ~DelayImportEntry() override = default;

  /// Demangled representation of the symbol or an empty string if it can't
  /// be demangled
  std::string demangled_name() const;

  /// `True` if it is an import by ordinal
  bool is_ordinal() const;

  /// The ordinal value
  uint16_t ordinal() const {
    static constexpr auto MASK = 0xFFFF;
    return data_ & MASK;
  }

  /// @see DelayImportEntry::data
  uint64_t hint_name_rva() const {
    return data();
  }

  /// Index into the Export::entries that is used to speed-up
  /// the symbol resolution.
  uint16_t hint() const {
    return hint_;
  }

  /// Value of the current entry in the Import Address Table.
  uint64_t iat_value() const {
    return iat_value_;
  }

  /// Raw value
  uint64_t data() const {
    return data_;
  }

  void data(uint64_t data) {
    data_ = data;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const DelayImportEntry& entry);

  private:
  uint64_t data_ = 0;
  uint16_t hint_ = 0;
  uint64_t iat_offset_ = 0;
  uint64_t iat_value_ = 0;
  PE_TYPE  type_ = PE_TYPE::PE32_PLUS;
};

}
}

#endif /* LIEF_PE_DELAY_IMPORT_ENTRY_H */

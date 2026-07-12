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
#ifndef LIEF_COFF_SECTION_H
#define LIEF_COFF_SECTION_H
#include <cstdint>
#include <memory>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/Abstract/Section.hpp"
#include "LIEF/PE/Section.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliarySectionDefinition.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/optional.hpp"

namespace LIEF {
class BinaryStream;

namespace COFF {
class Relocation;
class Parser;
class Symbol;


/// This class represents a COFF section
class LIEF_API Section : public LIEF::Section {
  public:
  friend class Parser;
  using LIEF::Section::name;

  using COMDAT_SELECTION = AuxiliarySectionDefinition::COMDAT_SELECTION;

  /// This structure wraps comdat information which is composed of the symbol
  /// associated with the comdat section and its selection flag
  struct LIEF_API ComdatInfo {
    Symbol* symbol = nullptr;
    COMDAT_SELECTION kind = COMDAT_SELECTION::NONE;
  };

  /// Mirror Characteristics from PE
  using CHARACTERISTICS = LIEF::PE::Section::CHARACTERISTICS;

  /// Container for the relocations in this section (owned by the Binary object)
  using relocations_t = std::vector<Relocation*>;

  /// Iterator that outputs Relocation&
  using it_relocations = ref_iterator<relocations_t&, Relocation*>;

  /// Iterator that outputs const Relocation&
  using it_const_relocations = const_ref_iterator<const relocations_t&, const Relocation*>;

  /// Container for the symbols associated with this section (owned by the Binary object)
  using symbols_t = std::vector<Symbol*>;

  /// Iterator that outputs Symbol&
  using it_symbols = ref_iterator<symbols_t&, Symbol*>;

  /// Iterator that outputs const Symbol&
  using it_const_symbols = const_ref_iterator<const symbols_t&, const Symbol*>;

  /// Parse a section from the given stream
  static std::unique_ptr<Section> parse(BinaryStream& stream);

  /// Return the size of the data in the section.
  uint32_t sizeof_raw_data() const {
    return size_;
  }

  /// Virtual size of the section (should be 0)
  uint32_t virtual_size() const {
    return virtual_size_;
  }

  /// Content wrapped by this section
  span<const uint8_t> content() const override {
    return content_;
  }

  /// Offset to the section's content
  uint32_t pointerto_raw_data() const {
    return offset_;
  }

  /// Offset to the relocation table
  uint32_t pointerto_relocation() const {
    return pointer_to_relocations_;
  }

  /// The file pointer to the beginning of line-number entries for the section.
  ///
  /// This is set to zero if there are no COFF line numbers.
  /// This value should be zero for an image because COFF debugging information
  /// is deprecated and modern debug information relies on the PDB files.
  uint32_t pointerto_line_numbers() const {
    return pointer_to_linenumbers_;
  }

  /// Number of relocations.
  ///
  /// \warning If the number of relocations is greater than 0xFFFF (maximum
  ///          value for 16-bits integer), then the number of relocations is
  ///          stored in the virtual address of the **first** relocation.
  uint16_t numberof_relocations() const {
    return number_of_relocations_;
  }

  /// Number of line number entries (if any).
  uint16_t numberof_line_numbers() const {
    return number_of_linenumbers_;
  }

  /// Characteristics of the section: it provides information about
  /// the permissions of the section when mapped. It can also provide
  /// information about the *purpose* of the section (contain code, BSS-like, ...)
  uint32_t characteristics() const {
    return characteristics_;
  }

  /// Check if the section has the given CHARACTERISTICS
  bool has_characteristic(CHARACTERISTICS c) const {
    return (characteristics() & (uint32_t)c) > 0;
  }

  /// List of the section characteristics
  std::vector<CHARACTERISTICS> characteristics_list() const {
    return LIEF::PE::Section::characteristics_to_list(characteristics_);
  }

  /// True if the section can be discarded as needed.
  ///
  /// This is typically the case for debug-related sections
  bool is_discardable() const {
    return has_characteristic(CHARACTERISTICS::MEM_DISCARDABLE);
  }

  void clear(uint8_t c) {
    std::fill(content_.begin(), content_.end(), c);
  }

  /// Iterator over the relocations associated with this section
  it_relocations relocations() {
    return relocations_;
  }

  it_const_relocations relocations() const {
    return relocations_;
  }

  /// Iterator over the symbols associated with this section
  it_symbols symbols() {
    return symbols_;
  }

  it_const_symbols symbols() const {
    return symbols_;
  }

  /// Return comdat infomration (only if the section has the
  /// CHARACTERISTICS::LNK_COMDAT characteristic)
  optional<ComdatInfo> comdat_info() const;

  /// Whether there is a large number of relocations whose number need
  /// to be stored in the virtual address attribute
  bool has_extended_relocations() const {
    return has_characteristic(CHARACTERISTICS::LNK_NRELOC_OVFL) &&
           numberof_relocations() == std::numeric_limits<uint16_t>::max();
  }

  void content(const std::vector<uint8_t>& data) override {
    content_ = data;
  }

  void name(std::string name) override;

  void virtual_size(uint32_t virtual_sz) {
    virtual_size_ = virtual_sz;
  }

  void pointerto_raw_data(uint32_t ptr) {
    offset(ptr);
  }

  void pointerto_relocation(uint32_t ptr) {
    pointer_to_relocations_ = ptr;
  }

  void pointerto_line_numbers(uint32_t ptr) {
    pointer_to_linenumbers_ = ptr;
  }

  void numberof_relocations(uint16_t nb) {
    number_of_relocations_ = nb;
  }

  void numberof_line_numbers(uint16_t nb) {
    number_of_linenumbers_ = nb;
  }

  void sizeof_raw_data(uint32_t size) {
    this->size(size);
  }

  void characteristics(uint32_t characteristics) {
    characteristics_ = characteristics;
  }

  std::string to_string() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Section& sec) {
    os << sec.to_string();
    return os;
  }

  ~Section() override = default;

  private:
  Section() = default;

  std::vector<uint8_t> content_;
  uint32_t virtual_size_           = 0;
  uint32_t pointer_to_relocations_ = 0;
  uint32_t pointer_to_linenumbers_ = 0;
  uint16_t number_of_relocations_  = 0;
  uint16_t number_of_linenumbers_  = 0;
  uint32_t characteristics_        = 0;

  relocations_t relocations_;
  symbols_t symbols_;
};

inline const char* to_string(Section::CHARACTERISTICS e) {
  return LIEF::PE::to_string(e);
}

}
}
#endif

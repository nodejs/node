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
#ifndef LIEF_COFF_AUXILIARY_SEC_DEF_H
#define LIEF_COFF_AUXILIARY_SEC_DEF_H

#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/COFF/AuxiliarySymbol.hpp"

namespace LIEF {
namespace COFF {

/// This auxiliary symbol exposes information about the associated section.
///
/// It **duplicates** some information that are provided in the section header
class LIEF_API AuxiliarySectionDefinition : public AuxiliarySymbol {
  public:
  LIEF_LOCAL static std::unique_ptr<AuxiliarySectionDefinition>
    parse(const std::vector<uint8_t>& payload);

  AuxiliarySectionDefinition() :
    AuxiliarySymbol(AuxiliarySymbol::TYPE::SEC_DEF)
  {}

  AuxiliarySectionDefinition(uint32_t length, uint16_t nb_relocs,
                             uint16_t nb_lines, uint32_t checksum,
                             uint32_t sec_idx, uint8_t selection,
                             uint8_t reserved) :
    AuxiliarySymbol(AuxiliarySymbol::TYPE::SEC_DEF),
    length_(length),
    nb_relocs_(nb_relocs),
    nb_lines_(nb_lines),
    checksum_(checksum),
    sec_idx_(sec_idx),
    selection_((COMDAT_SELECTION)selection),
    reserved_(reserved)
  {}

  AuxiliarySectionDefinition(const AuxiliarySectionDefinition&) = default;
  AuxiliarySectionDefinition& operator=(const AuxiliarySectionDefinition&) = default;

  AuxiliarySectionDefinition(AuxiliarySectionDefinition&&) = default;
  AuxiliarySectionDefinition& operator=(AuxiliarySectionDefinition&&) = default;

  std::unique_ptr<AuxiliarySymbol> clone() const override {
    return std::unique_ptr<AuxiliarySectionDefinition>(new AuxiliarySectionDefinition{*this});
  }

  /// Values for the AuxiliarySectionDefinition::selection attribute
  ///
  /// See: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#comdat-sections-object-only
  enum class COMDAT_SELECTION : uint8_t {
    NONE = 0,

    /// If this symbol is already defined, the linker issues a `multiply defined symbol`
    /// error.
    NODUPLICATES = 1,

    /// Any section that defines the same COMDAT symbol can be linked; the rest
    /// are removed.
    ANY,

    /// The linker chooses an arbitrary section among the definitions for this
    /// symbol. If all definitions are not the same size, a `multiply defined symbol`
    /// error is issued.
    SAME_SIZE,

    /// The linker chooses an arbitrary section among the definitions for this
    /// symbol. If all definitions do not match exactly, a
    /// `multiply defined symbol` error is issued.
    EXACT_MATCH,

    /// The section is linked if a certain other COMDAT section is linked.
    /// This other section is indicated by the Number field of the auxiliary
    /// symbol record for the section definition. This setting is useful for
    /// definitions that have components in multiple sections
    /// (for example, code in one and data in another), but where all must be
    /// linked or discarded as a set. The other section this section is
    /// associated with must be a COMDAT section, which can be another
    /// associative COMDAT section. An associative COMDAT section's section
    /// association chain can't form a loop. The section association chain must
    /// eventually come to a COMDAT section that doesn't have
    /// COMDAT_SELECTION::ASSOCIATIVE set.
    ASSOCIATIVE,

    /// The linker chooses the largest definition from among all of the definitions
    /// for this symbol. If multiple definitions have this size, the choice
    /// between them is arbitrary.
    LARGEST
  };

  /// The size of section data. The same as `SizeOfRawData` in the section header.
  uint32_t length() const {
    return length_;
  }

  /// The number of relocation entries for the section.
  uint16_t nb_relocs() const {
    return nb_relocs_;
  }

  /// The number of line-number entries for the section.
  uint16_t nb_line_numbers() const {
    return nb_lines_;
  }

  /// The checksum for communal data. It is applicable if the
  /// `IMAGE_SCN_LNK_COMDAT` flag is set in the section header.
  uint32_t checksum() const {
    return checksum_;
  }

  /// One-based index into the section table for the associated section.
  /// This is used when the COMDAT selection setting is 5.
  uint32_t section_idx() const {
    return sec_idx_;
  }

  /// The COMDAT selection number. This is applicable if the section is a
  /// COMDAT section.
  COMDAT_SELECTION selection() const {
    return selection_;
  }

  /// Reserved value (should be 0)
  uint8_t reserved() const {
    return reserved_;
  }

  std::string to_string() const override;

  static bool classof(const AuxiliarySymbol* sym) {
    return sym->type() == AuxiliarySymbol::TYPE::SEC_DEF;
  }

  ~AuxiliarySectionDefinition() override = default;

  private:
  uint32_t length_ = 0;
  uint16_t nb_relocs_ = 0;
  uint16_t nb_lines_ = 0;
  uint32_t checksum_ = 0;
  uint32_t sec_idx_ = 0;
  COMDAT_SELECTION selection_ = COMDAT_SELECTION::NONE;
  uint8_t reserved_ = 0;
};

LIEF_API const char* to_string(AuxiliarySectionDefinition::COMDAT_SELECTION e);

}
}
#endif

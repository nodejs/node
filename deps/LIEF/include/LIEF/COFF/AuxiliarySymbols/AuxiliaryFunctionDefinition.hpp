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
#ifndef LIEF_COFF_AUXILIARY_FUNCTION_DEF_H
#define LIEF_COFF_AUXILIARY_FUNCTION_DEF_H

#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/COFF/AuxiliarySymbol.hpp"

namespace LIEF {
namespace COFF {

/// This auxiliary symbols marks the beginning of a function definition.
///
/// Reference: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#auxiliary-format-1-function-definitions
class LIEF_API AuxiliaryFunctionDefinition : public AuxiliarySymbol {
  public:
  LIEF_LOCAL static std::unique_ptr<AuxiliaryFunctionDefinition>
    parse(const std::vector<uint8_t>& payload);

  AuxiliaryFunctionDefinition() :
    AuxiliarySymbol(AuxiliarySymbol::TYPE::FUNC_DEF)
  {}

  AuxiliaryFunctionDefinition(uint32_t tagidx, uint32_t totalsz,
                              uint32_t ptr_line, uint32_t ptr_next_func,
                              uint16_t padding) :
    AuxiliarySymbol(AuxiliarySymbol::TYPE::FUNC_DEF),
    tag_index_(tagidx),
    total_size_(totalsz),
    ptr_to_linenb_(ptr_line),
    ptr_to_next_func_(ptr_next_func),
    padding_(padding)
  {}

  AuxiliaryFunctionDefinition(const AuxiliaryFunctionDefinition&) = default;
  AuxiliaryFunctionDefinition& operator=(const AuxiliaryFunctionDefinition&) = default;

  AuxiliaryFunctionDefinition(AuxiliaryFunctionDefinition&&) = default;
  AuxiliaryFunctionDefinition& operator=(AuxiliaryFunctionDefinition&&) = default;

  std::unique_ptr<AuxiliarySymbol> clone() const override {
    return std::unique_ptr<AuxiliaryFunctionDefinition>(new AuxiliaryFunctionDefinition{*this});
  }

  /// The symbol-table index of the corresponding `.bf` (begin function)
  /// symbol record.
  uint32_t tag_index() const {
    return tag_index_;
  }

  /// The size of the executable code for the function itself.
  ///
  /// If the function is in its own section, the `SizeOfRawData` in the section
  /// header is greater or equal to this field, depending on alignment
  /// considerations.
  uint32_t total_size() const {
    return total_size_;
  }

  /// The file offset of the first COFF line-number entry for the function,
  /// or zero if none exists (deprecated)
  uint32_t ptr_to_line_number() const {
    return ptr_to_linenb_;
  }

  /// The symbol-table index of the record for the next function. If the function
  /// is the last in the symbol table, this field is set to zero.
  uint32_t ptr_to_next_func() const {
    return ptr_to_next_func_;
  }

  /// Padding value (should be 0)
  uint16_t padding() const {
    return padding_;
  }

  std::string to_string() const override;

  static bool classof(const AuxiliarySymbol* sym) {
    return sym->type() == AuxiliarySymbol::TYPE::FUNC_DEF;
  }

  ~AuxiliaryFunctionDefinition() override = default;

  private:
  uint32_t tag_index_ = 0;
  uint32_t total_size_ = 0;
  uint32_t ptr_to_linenb_ = 0;
  uint32_t ptr_to_next_func_ = 0;
  uint16_t padding_ = 0;
};

}
}
#endif

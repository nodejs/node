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
#ifndef LIEF_COFF_AUXILIARY_CLR_TOKEN_H
#define LIEF_COFF_AUXILIARY_CLR_TOKEN_H

#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/COFF/AuxiliarySymbol.hpp"

namespace LIEF {
namespace COFF {
class Symbol;
class Parser;

/// Auxiliary symbol associated with the `CLR_TOKEN` storage class
class LIEF_API AuxiliaryCLRToken : public AuxiliarySymbol {
  public:
  friend class Parser;

  LIEF_LOCAL static std::unique_ptr<AuxiliaryCLRToken>
    parse(const std::vector<uint8_t>& payload);

  AuxiliaryCLRToken() :
    AuxiliarySymbol(AuxiliarySymbol::TYPE::CLR_TOKEN)
  {}

  AuxiliaryCLRToken(uint8_t aux_type, uint8_t reserved, uint32_t symbol_idx,
                    std::vector<uint8_t> rgb_reserved) :
    AuxiliarySymbol(AuxiliarySymbol::TYPE::CLR_TOKEN),
    aux_type_(aux_type),
    reserved_(reserved),
    symbol_idx_(symbol_idx),
    rgb_reserved_(std::move(rgb_reserved))
  {}

  AuxiliaryCLRToken(const AuxiliaryCLRToken&) = default;
  AuxiliaryCLRToken& operator=(const AuxiliaryCLRToken&) = default;

  AuxiliaryCLRToken(AuxiliaryCLRToken&&) = default;
  AuxiliaryCLRToken& operator=(AuxiliaryCLRToken&&) = default;

  std::unique_ptr<AuxiliarySymbol> clone() const override {
    return std::unique_ptr<AuxiliaryCLRToken>(new AuxiliaryCLRToken{*this});
  }

  /// `IMAGE_AUX_SYMBOL_TYPE` which should be `IMAGE_AUX_SYMBOL_TYPE_TOKEN_DEF` (1)
  uint8_t aux_type() const {
    return aux_type_;
  }

  /// Reserved value (should be 0)
  uint8_t reserved() const {
    return aux_type_;
  }

  /// Index in the symbol table
  uint32_t symbol_idx() const {
    return symbol_idx_;
  }

  /// Symbol referenced by symbol_idx() (if resolved)
  const Symbol* symbol() const {
    return sym_;
  }

  Symbol* symbol() {
    return sym_;
  }

  /// Reserved (padding) values. Should be 0
  span<const uint8_t> rgb_reserved() const {
    return rgb_reserved_;
  }

  std::string to_string() const override;

  static bool classof(const AuxiliarySymbol* sym) {
    return sym->type() == AuxiliarySymbol::TYPE::CLR_TOKEN;
  }

  ~AuxiliaryCLRToken() override = default;
  private:
  uint8_t aux_type_ = 0;
  uint8_t reserved_ = 0;
  uint32_t symbol_idx_ = 0;
  std::vector<uint8_t> rgb_reserved_;
  Symbol* sym_ = nullptr;
};

}
}
#endif

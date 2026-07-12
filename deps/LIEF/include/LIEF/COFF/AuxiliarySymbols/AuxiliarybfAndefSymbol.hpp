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
#ifndef LIEF_COFF_AUXILIARY_BF_AND_EF_H
#define LIEF_COFF_AUXILIARY_BF_AND_EF_H

#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/COFF/AuxiliarySymbol.hpp"

namespace LIEF {

namespace COFF {

class LIEF_API AuxiliarybfAndefSymbol : public AuxiliarySymbol {
  public:
  LIEF_LOCAL static std::unique_ptr<AuxiliarybfAndefSymbol>
    parse(Symbol& sym, const std::vector<uint8_t>& payload);

  AuxiliarybfAndefSymbol() :
    AuxiliarySymbol(AuxiliarySymbol::TYPE::BF_AND_EF)
  {}

  AuxiliarybfAndefSymbol(const AuxiliarybfAndefSymbol&) = default;
  AuxiliarybfAndefSymbol& operator=(const AuxiliarybfAndefSymbol&) = default;

  AuxiliarybfAndefSymbol(AuxiliarybfAndefSymbol&&) = default;
  AuxiliarybfAndefSymbol& operator=(AuxiliarybfAndefSymbol&&) = default;

  std::unique_ptr<AuxiliarySymbol> clone() const override {
    return std::unique_ptr<AuxiliarybfAndefSymbol>(new AuxiliarybfAndefSymbol{*this});
  }

  std::string to_string() const override {
    return "AuxiliarybfAndefSymbol";
  }

  static bool classof(const AuxiliarySymbol* sym) {
    return sym->type() == AuxiliarySymbol::TYPE::BF_AND_EF;
  }

  ~AuxiliarybfAndefSymbol() override = default;
};

}
}
#endif

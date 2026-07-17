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
#ifndef LIEF_COFF_AUXILIARY_WEAK_EXTERNAL_H
#define LIEF_COFF_AUXILIARY_WEAK_EXTERNAL_H

#include <memory>
#include <cassert>

#include "LIEF/visibility.h"
#include "LIEF/COFF/AuxiliarySymbol.hpp"

namespace LIEF {

namespace COFF {

/// "Weak externals" are a mechanism for object files that allows flexibility at
/// link time. A module can contain an unresolved external symbol (`sym1`), but
/// it can also include an auxiliary record that indicates that if `sym1` is not
/// present at link time, another external symbol (`sym2`) is used to resolve
/// references instead.
///
/// If a definition of `sym1` is linked, then an external reference to the
/// symbol is resolved normally. If a definition of `sym1` is not linked, then all
/// references to the weak external for `sym1` refer to `sym2` instead. The external
/// symbol, `sym2`, must always be linked; typically, it is defined in the module
/// that contains the weak reference to `sym1`.
///
/// Reference: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#auxiliary-format-3-weak-externals
class LIEF_API AuxiliaryWeakExternal : public AuxiliarySymbol {
  public:
  enum class CHARACTERISTICS : uint32_t {
    /// No library search for `sym1` should be performed.
    SEARCH_NOLIBRARY = 1,
    ///A library search for `sym1` should be performed.
    SEARCH_LIBRARY = 2,
    /// `sym1` is an alias for sym2
    SEARCH_ALIAS = 3,
    ANTI_DEPENDENCY = 4
  };

  LIEF_LOCAL static std::unique_ptr<AuxiliaryWeakExternal>
    parse(const std::vector<uint8_t>& payload);

  AuxiliaryWeakExternal() :
    AuxiliarySymbol(AuxiliarySymbol::TYPE::WEAK_EXTERNAL)
  {}

  AuxiliaryWeakExternal(uint32_t sym_idx, uint32_t characteristics,
                        std::vector<uint8_t> padding) :
    AuxiliarySymbol(AuxiliarySymbol::TYPE::WEAK_EXTERNAL),
    sym_idx_(sym_idx),
    characteristics_(characteristics),
    padding_(std::move(padding))
  {
    assert(padding_.size() == 10);
  }

  AuxiliaryWeakExternal(const AuxiliaryWeakExternal&) = default;
  AuxiliaryWeakExternal& operator=(const AuxiliaryWeakExternal&) = default;

  AuxiliaryWeakExternal(AuxiliaryWeakExternal&&) = default;
  AuxiliaryWeakExternal& operator=(AuxiliaryWeakExternal&&) = default;

  std::unique_ptr<AuxiliarySymbol> clone() const override {
    return std::unique_ptr<AuxiliaryWeakExternal>(new AuxiliaryWeakExternal{*this});
  }

  /// The symbol-table index of `sym2`, the symbol to be linked if `sym1` is not
  /// found.
  uint32_t sym_idx() const {
    return sym_idx_;
  }

  CHARACTERISTICS characteristics() const {
    return (CHARACTERISTICS)characteristics_;
  }

  span<const uint8_t> padding() const {
    return padding_;
  }

  std::string to_string() const override;

  static bool classof(const AuxiliarySymbol* sym) {
    return sym->type() == AuxiliarySymbol::TYPE::WEAK_EXTERNAL;
  }

  ~AuxiliaryWeakExternal() override = default;

  private:
  uint32_t sym_idx_ = 0;
  uint32_t characteristics_ = 0;
  std::vector<uint8_t> padding_;
};

LIEF_API const char* to_string(AuxiliaryWeakExternal::CHARACTERISTICS e);

}
}
#endif

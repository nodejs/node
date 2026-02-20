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
#ifndef LIEF_ELF_SYMBOL_VERSION_H
#define LIEF_ELF_SYMBOL_VERSION_H
#include <ostream>
#include <cstdint>
#include <cassert>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace ELF {
class Parser;
class SymbolVersionAux;
class SymbolVersionAuxRequirement;

/// Class which represents an entry defined in the `DT_VERSYM`
/// dynamic entry
class LIEF_API SymbolVersion : public Object {
  friend class Parser;

  public:
  static constexpr auto LOCAL_VERSION = 0;
  static constexpr auto GLOBAL_VERSION = 1;

  SymbolVersion(uint16_t value) :
    value_(value)
  {}
  SymbolVersion() = default;

  /// Generate a *local* SymbolVersion
  static SymbolVersion local() {
    return SymbolVersion(LOCAL_VERSION);
  }

  /// Generate a *global* SymbolVersion
  static SymbolVersion global() {
    return SymbolVersion(GLOBAL_VERSION);
  }

  ~SymbolVersion() override = default;

  SymbolVersion& operator=(const SymbolVersion&) = default;
  SymbolVersion(const SymbolVersion&) = default;

  /// Value associated with the symbol
  ///
  /// If the given SymbolVersion hasn't Auxiliary version:
  ///
  /// * ``0`` means **Local**
  /// * ``1`` means **Global**
  uint16_t value() const {
    return value_;
  }

  /// Whether the current SymbolVersion has an auxiliary one
  bool has_auxiliary_version() const {
    return symbol_version_auxiliary() != nullptr;
  }

  /// SymbolVersionAux associated with the current Version if any,
  /// or a nullptr
  SymbolVersionAux* symbol_version_auxiliary() {
    return symbol_aux_;
  }

  const SymbolVersionAux* symbol_version_auxiliary() const {
    return symbol_aux_;
  }

  /// Set the version's auxiliary requirement
  /// The given SymbolVersionAuxRequirement must be an existing
  /// reference in the ELF::Binary.
  ///
  /// On can add a new SymbolVersionAuxRequirement by using
  /// SymbolVersionRequirement::add_aux_requirement
  void symbol_version_auxiliary(SymbolVersionAuxRequirement& svauxr);

  /// Drop the versioning requirement and replace the value (local/global)
  void drop_version(uint16_t value) {
    if (symbol_aux_ == nullptr) {
      return;
    }
    assert(value == LOCAL_VERSION || value == GLOBAL_VERSION);
    value_ = value;
    symbol_aux_ = nullptr;
  }

  /// Redefine this version as global by dropping its auxiliary version
  ///
  /// \see as_local() drop_version()
  void as_global() {
    return drop_version(GLOBAL_VERSION);
  }

  /// Redefine this version as local by dropping its auxiliary version
  ///
  /// \see as_global() drop_version()
  void as_local() {
    return drop_version(LOCAL_VERSION);
  }

  void value(uint16_t v) {
    value_ = v;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const SymbolVersion& symv);

  private:
  uint16_t          value_ = 0;
  SymbolVersionAux* symbol_aux_ = nullptr;
};
}
}
#endif

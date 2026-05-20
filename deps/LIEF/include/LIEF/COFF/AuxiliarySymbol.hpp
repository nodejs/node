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
#ifndef LIEF_COFF_AUXILIARY_SYMBOL_H
#define LIEF_COFF_AUXILIARY_SYMBOL_H

#include <ostream>
#include <memory>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

namespace LIEF {
class BinaryStream;

namespace COFF {
class Symbol;

/// Class that represents an auxiliary symbol.
///
/// An auxiliary symbol has the same size as a regular LIEF::PE::Symbol (18
/// bytes) but its content depends on the the parent symbol.
class LIEF_API AuxiliarySymbol {
  public:
  AuxiliarySymbol() = default;
  AuxiliarySymbol(std::vector<uint8_t> payload) :
    type_(TYPE::UNKNOWN),
    payload_(std::move(payload))
  {}
  AuxiliarySymbol(const AuxiliarySymbol&) = default;
  AuxiliarySymbol& operator=(const AuxiliarySymbol&) = default;

  AuxiliarySymbol(AuxiliarySymbol&&) = default;
  AuxiliarySymbol& operator=(AuxiliarySymbol&&) = default;

  LIEF_LOCAL static std::unique_ptr<AuxiliarySymbol>
    parse(Symbol& sym, std::vector<uint8_t> payload);

  virtual std::unique_ptr<AuxiliarySymbol> clone() const {
    return std::unique_ptr<AuxiliarySymbol>(new AuxiliarySymbol(*this));
  }

  /// Type discriminator for the subclasses
  enum class TYPE {
    UNKNOWN = 0,
    CLR_TOKEN,
    /// Auxiliary Format 1 from the PE-COFF documentation
    FUNC_DEF,
    /// Auxiliary Format 2: .bf and .ef Symbols from the PE-COFF documentation
    BF_AND_EF,
    /// Auxiliary Format 3: Weak Externals from the PE-COFF documentation
    WEAK_EXTERNAL,
    /// Auxiliary Format 4: Files from the PE-COFF documentation
    FILE,
    /// Auxiliary Format 5: Section Definitions from the PE-COFF documentation
    SEC_DEF,
  };

  AuxiliarySymbol(TYPE ty) :
    type_(ty)
  {}

  static TYPE get_aux_type(const Symbol& sym);

  TYPE type() const {
    return type_;
  }

  /// For unknown type **only**, return the raw representation of this symbol
  span<const uint8_t> payload() const {
    return payload_;
  }

  span<uint8_t> payload() {
    return payload_;
  }

  virtual std::string to_string() const;

  virtual ~AuxiliarySymbol() = default;

  /// Helper to **downcast** a AuxiliarySymbol into a concrete implementation
  template<class T>
  const T* as() const {
    static_assert(std::is_base_of<AuxiliarySymbol, T>::value,
                  "Require AuxiliarySymbol inheritance");
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  template<class T>
  T* as() {
    return const_cast<T*>(static_cast<const AuxiliarySymbol*>(this)->as<T>());
  }

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const AuxiliarySymbol& aux)
  {
    os << aux.to_string();
    return os;
  }

  protected:
  TYPE type_ = TYPE::UNKNOWN;
  std::vector<uint8_t> payload_;
};

}
}
#endif

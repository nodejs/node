/* Copyright 2022 - 2026 R. Thomas
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
#ifndef LIEF_ASM_X86_OPERAND_H
#define LIEF_ASM_X86_OPERAND_H
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/compiler_attributes.hpp"

#include <memory>
#include <string>
#include <cassert>

#include <ostream>

namespace LIEF {
namespace assembly {
namespace x86 {

namespace details {
class Operand;
class OperandIt;
}

/// This class represents an operand for an x86/x86-64 instruction
class LIEF_API Operand {
  public:
  /// **Forward** iterator that lazily disassembles x86 Operand.
  class Iterator final
    : public iterator_facade_base<Iterator, std::forward_iterator_tag, Operand,
                                  std::ptrdiff_t, const Operand*, const Operand&> {
    public:
    using implementation = details::OperandIt;
    using iterator_facade_base::operator++;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::OperandIt> impl);
    LIEF_API Iterator(const Iterator&);
    LIEF_API Iterator& operator=(const Iterator&);

    LIEF_API Iterator(Iterator&&) noexcept;
    LIEF_API Iterator& operator=(Iterator&&) noexcept;

    LIEF_API ~Iterator();

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API Iterator& operator++() LIEF_LIFETIMEBOUND;

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS);

    friend bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    LIEF_API const Operand& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Operand* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the operand at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Operand> yield();

    private:
    void load() const;

    std::unique_ptr<details::OperandIt> impl_;
    mutable std::unique_ptr<Operand> cached_;
  };

  /// Pretty representation of the operand
  std::string to_string() const;

  /// This function can be used to **down cast** an Operand instance:
  ///
  /// ```cpp
  /// std::unique_ptr<assembly::x86::Operand> op = ...;
  /// if (const auto* memory = inst->as<assembly::x86::operands::Memory>()) {
  ///   const assembly::x86::REG base = memory->base();
  /// }
  /// ```
  template<class T>
  const T* as() const LIEF_LIFETIMEBOUND {
    static_assert(std::is_base_of<Operand, T>::value,
                  "Require Operand inheritance");
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  virtual ~Operand();

  /// @private
  static LIEF_LOCAL std::unique_ptr<Operand>
      create(std::unique_ptr<details::Operand> impl);

  /// @private
  LIEF_LOCAL const details::Operand& impl() const LIEF_LIFETIMEBOUND {
    assert(impl_ != nullptr);
    return *impl_;
  }

  /// @private
  LIEF_LOCAL details::Operand& impl() LIEF_LIFETIMEBOUND {
    assert(impl_ != nullptr);
    return *impl_;
  }

  friend LIEF_API std::ostream& operator<<(std::ostream& os, const Operand& op) {
    os << op.to_string();
    return os;
  }

  protected:
  LIEF_LOCAL Operand(std::unique_ptr<details::Operand> impl);
  std::unique_ptr<details::Operand> impl_;
};

}
}
}

#endif

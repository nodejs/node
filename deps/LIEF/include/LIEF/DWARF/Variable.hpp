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
#ifndef LIEF_DWARF_VARIABLE_H
#define LIEF_DWARF_VARIABLE_H

#include <memory>

#include "LIEF/iterators.hpp"
#include "LIEF/visibility.h"
#include "LIEF/compiler_attributes.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/debug_loc.hpp"
#include "LIEF/DWARF/Type.hpp"

namespace LIEF {
namespace dwarf {
class Scope;

namespace details {
class Variable;
class VariableIt;
}

/// This class represents a DWARF variable which can be owned by a
/// dwarf::Function or a dwarf::CompilationUnit
class LIEF_API Variable {
  public:
  class Iterator final
    : public iterator_facade_base<Iterator, std::bidirectional_iterator_tag,
                                  Variable, std::ptrdiff_t, const Variable*,
                                  const Variable&> {
    public:
    using implementation = details::VariableIt;
    using iterator_facade_base::operator++;
    using iterator_facade_base::operator--;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::VariableIt> impl);

    LIEF_API Iterator(const Iterator&);
    LIEF_API Iterator& operator=(const Iterator&);

    LIEF_API Iterator(Iterator&&) noexcept;
    LIEF_API Iterator& operator=(Iterator&&) noexcept;

    LIEF_API ~Iterator();

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS);
    friend bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API Iterator& operator++();

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API Iterator& operator--();

    LIEF_API const Variable& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Variable* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the variable at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Variable> yield();

    private:
    void load() const;

    std::unique_ptr<details::VariableIt> impl_;
    mutable std::unique_ptr<Variable> cached_;
  };

  Variable(std::unique_ptr<details::Variable> impl);

  /// Name of the variable (usually demangled)
  std::string name() const;

  /// The name of the variable which is used for linking (`DW_AT_linkage_name`).
  ///
  /// This name differs from name() as it is usually mangled. The function
  /// returns an empty string if the linkage name is not available.
  std::string linkage_name() const;

  /// Address of the variable.
  ///
  /// If the variable is **static**, it returns the **virtual address**
  /// where it is defined.
  /// If the variable is stack-based, it returns the **relative offset** from
  /// the frame based register.
  ///
  /// If the address can't be resolved, it returns a lief_errors.
  result<int64_t> address() const;

  /// Return the size of the variable (or a lief_errors if it can't be
  /// resolved).
  ///
  /// This size is defined by its type.
  result<uint64_t> size() const;

  /// Whether it's a `constexpr` variable
  bool is_constexpr() const;

  /// Whether this variable is allocated on the stack
  bool is_stack_based() const;

  /// The original source location where the variable is defined.
  debug_location_t debug_location() const;

  /// Return the type of this variable
  std::unique_ptr<Type> type() const LIEF_LIFETIMEBOUND;

  /// Return the scope in which this variable is defined
  std::unique_ptr<Scope> scope() const LIEF_LIFETIMEBOUND;

  /// Description (`DW_AT_description`) of the variable or an empty string
  std::string description() const;

  /// Generates a C/C++ definition for this variable
  std::string to_decl(const DeclOpt& opt = DeclOpt()) const;

  ~Variable();

  private:
  std::unique_ptr<details::Variable> impl_;
};

}
}
#endif

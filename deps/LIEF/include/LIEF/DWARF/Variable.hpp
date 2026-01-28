/* Copyright 2022 - 2025 R. Thomas
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

#include "LIEF/visibility.h"
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
  class LIEF_API Iterator {
    public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::unique_ptr<Variable>;
    using difference_type = std::ptrdiff_t;
    using pointer = Variable*;
    using reference = std::unique_ptr<Variable>&;
    using implementation = details::VariableIt;

    class LIEF_API PointerProxy {
      // Inspired from LLVM's iterator_facade_base
      friend class Iterator;
      public:
      pointer operator->() const { return R.get(); }

      private:
      value_type R;

      template <typename RefT>
      PointerProxy(RefT &&R) : R(std::forward<RefT>(R)) {} // NOLINT(bugprone-forwarding-reference-overload)
    };

    Iterator(const Iterator&);
    Iterator(Iterator&&) noexcept;
    Iterator(std::unique_ptr<details::VariableIt> impl);
    ~Iterator();

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS);
    friend LIEF_API bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    Iterator& operator++();
    Iterator& operator--();

    Iterator operator--(int) {
      Iterator tmp = *static_cast<Iterator*>(this);
      --*static_cast<Iterator *>(this);
      return tmp;
    }

    Iterator operator++(int) {
      Iterator tmp = *static_cast<Iterator*>(this);
      ++*static_cast<Iterator *>(this);
      return tmp;
    }

    std::unique_ptr<Variable> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::VariableIt> impl_;
  };

  Variable(std::unique_ptr<details::Variable> impl);

  /// Name of the variable (usually demangled)
  std::string name() const;

  /// The name of the variable which is used for linking (`DW_AT_linkage_name`).
  ///
  /// This name differs from name() as it is usually mangled. The function
  /// return an empty string if the linkage name is not available.
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

  /// The original source location where the variable is defined.
  debug_location_t debug_location() const;

  /// Return the type of this variable
  std::unique_ptr<Type> type() const;

  /// Return the scope in which this variable is defined
  std::unique_ptr<Scope> scope() const;

  ~Variable();
  private:
  std::unique_ptr<details::Variable> impl_;
};

}
}
#endif

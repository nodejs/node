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
#ifndef LIEF_DWARF_FUNCTION_H
#define LIEF_DWARF_FUNCTION_H

#include <memory>
#include <string>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/range.hpp"
#include "LIEF/DWARF/Variable.hpp"
#include "LIEF/DWARF/Type.hpp"
#include "LIEF/asm/Instruction.hpp"

namespace LIEF {
namespace dwarf {

class Scope;
class Parameter;

namespace details {
class Function;
class FunctionIt;
}

/// This class represents a DWARF function which can be associated with either:
/// `DW_TAG_subprogram` or `DW_TAG_inlined_subroutine`.
class LIEF_API Function {
  public:
  class LIEF_API Iterator {
    public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::unique_ptr<Function>;
    using difference_type = std::ptrdiff_t;
    using pointer = Function*;
    using reference = std::unique_ptr<Function>&;
    using implementation = details::FunctionIt;

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
    Iterator(std::unique_ptr<details::FunctionIt> impl);
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

    std::unique_ptr<Function> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::FunctionIt> impl_;
  };

  /// Iterator over the variables defined in the scope of this function
  using vars_it = iterator_range<Variable::Iterator>;
  using parameters_t = std::vector<std::unique_ptr<Parameter>>;
  using thrown_types_t = std::vector<std::unique_ptr<Type>>;

  using instructions_it = iterator_range<assembly::Instruction::Iterator>;

  Function(std::unique_ptr<details::Function> impl);

  /// The name of the function (`DW_AT_name`)
  std::string name() const;

  /// The name of the function which is used for linking (`DW_AT_linkage_name`).
  ///
  /// This name differs from name() as it is usually mangled. The function
  /// return an empty string if the linkage name is not available.
  std::string linkage_name() const;

  /// Return the address of the function (`DW_AT_entry_pc` or `DW_AT_low_pc`).
  result<uint64_t> address() const;

  /// Return an iterator of variables (`DW_TAG_variable`) defined within the
  /// scope of this function. This includes regular stack-based variables as
  /// well as static ones.
  vars_it variables() const;

  /// Whether this function is created by the compiler and not
  /// present in the original source code
  bool is_artificial() const;

  /// Whether the function is defined **outside** the current compilation unit
  /// (`DW_AT_external`).
  bool is_external() const;

  /// Return the size taken by this function in the binary
  uint64_t size() const;

  /// Ranges of virtual addresses owned by this function
  std::vector<range_t> ranges() const;

  /// Original source code location
  debug_location_t debug_location() const;

  /// Return the dwarf::Type associated with the **return type** of this
  /// function
  std::unique_ptr<Type> type() const;

  /// Return the function's parameters (including any template parameter)
  parameters_t parameters() const;

  /// List of exceptions (types) that can be thrown by the function.
  ///
  /// For instance, given this Swift code:
  ///
  /// ```swift
  /// func summarize(_ ratings: [Int]) throws(StatisticsError) {
  ///   // ...
  /// }
  /// ```
  ///
  /// thrown_types() returns one element associated with the Type:
  /// `StatisticsError`.
  thrown_types_t thrown_types() const;

  /// Return the scope in which this function is defined
  std::unique_ptr<Scope> scope() const;

  /// Disassemble the current function by returning an iterator over
  /// the assembly::Instruction
  instructions_it instructions() const;

  ~Function();
  private:
  std::unique_ptr<details::Function> impl_;
};

}
}
#endif

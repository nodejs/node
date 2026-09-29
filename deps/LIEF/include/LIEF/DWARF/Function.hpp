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
#ifndef LIEF_DWARF_FUNCTION_H
#define LIEF_DWARF_FUNCTION_H

#include <memory>
#include <string>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/compiler_attributes.hpp"

#include "LIEF/range.hpp"
#include "LIEF/DWARF/Variable.hpp"
#include "LIEF/DWARF/Type.hpp"
#include "LIEF/DWARF/LexicalBlock.hpp"
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
  class Iterator final
    : public iterator_facade_base<Iterator, std::bidirectional_iterator_tag,
                                  Function, std::ptrdiff_t, const Function*,
                                  const Function&> {
    public:
    using implementation = details::FunctionIt;
    using iterator_facade_base::operator++;
    using iterator_facade_base::operator--;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::FunctionIt> impl);

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

    LIEF_API const Function& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Function* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the function at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Function> yield();

    private:
    void load() const;

    std::unique_ptr<details::FunctionIt> impl_;
    mutable std::unique_ptr<Function> cached_;
  };

  /// Iterator over the variables defined in the scope of this function
  using vars_it = iterator_range<Variable::Iterator>;
  using parameters_t = std::vector<std::unique_ptr<Parameter>>;
  using thrown_types_t = std::vector<std::unique_ptr<Type>>;

  using lexical_blocks_it = iterator_range<LexicalBlock::Iterator>;

  using instructions_it = iterator_range<assembly::Instruction::Iterator>;

  Function(std::unique_ptr<details::Function> impl);

  /// The name of the function (`DW_AT_name`)
  std::string name() const;

  /// The name of the function which is used for linking (`DW_AT_linkage_name`).
  ///
  /// This name differs from name() as it is usually mangled. The function
  /// returns an empty string if the linkage name is not available.
  std::string linkage_name() const;

  /// Return the address of the function (`DW_AT_entry_pc` or `DW_AT_low_pc`).
  result<uint64_t> address() const;

  /// Return an iterator of variables (`DW_TAG_variable`) defined within the
  /// scope of this function. This includes regular stack-based variables as
  /// well as static ones.
  vars_it variables() const LIEF_LIFETIMEBOUND;

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
  std::unique_ptr<Type> type() const LIEF_LIFETIMEBOUND;

  /// Return the function's parameters (including any template parameter)
  parameters_t parameters() const LIEF_LIFETIMEBOUND;

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
  thrown_types_t thrown_types() const LIEF_LIFETIMEBOUND;

  /// Return the scope in which this function is defined
  std::unique_ptr<Scope> scope() const LIEF_LIFETIMEBOUND;

  /// Disassemble the current function by returning an iterator over
  /// the assembly::Instruction
  instructions_it instructions() const LIEF_LIFETIMEBOUND;

  /// Iterator over the LexicalBlock owned by this function
  lexical_blocks_it lexical_blocks() const LIEF_LIFETIMEBOUND;

  /// Description (`DW_AT_description`) of this function or an empty string
  std::string description() const;

  /// Generates a C/C++ definition for this function
  std::string to_decl(const DeclOpt& opt = DeclOpt()) const;

  ~Function();

  private:
  std::unique_ptr<details::Function> impl_;
};

}
}
#endif

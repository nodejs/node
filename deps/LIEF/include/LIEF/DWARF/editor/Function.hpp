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
#ifndef LIEF_DWARF_EDITOR_FUNCTION_H
#define LIEF_DWARF_EDITOR_FUNCTION_H
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace dwarf {
namespace editor {
class Type;
class Variable;

namespace details {
class Function;
class FunctionParameter;
class FunctionLexicalBlock;
class FunctionLabel;
}

/// This class represents an **editable** DWARF function (`DW_TAG_subprogram`)
class LIEF_API Function {
  public:
  struct LIEF_API range_t {
    range_t() = default;
    range_t(uint64_t start, uint64_t end) :
      start(start),
      end(end) {}
    uint64_t start = 0;
    uint64_t end = 0;
  };

  /// This class represents a parameter of the current function
  /// (`DW_TAG_formal_parameter`)
  class LIEF_API Parameter {
    public:
    Parameter() = delete;
    Parameter(std::unique_ptr<details::FunctionParameter> impl);

    ~Parameter();

    /// Assign this parameter to a specific named register.
    Parameter& assign_register(const std::string& name) LIEF_LIFETIMEBOUND;

    /// Assign this parameter to the given DWARF register id (e.g. `DW_OP_reg0`).
    Parameter& assign_register(uint64_t reg) LIEF_LIFETIMEBOUND;

    private:
    std::unique_ptr<details::FunctionParameter> impl_;
  };

  /// This class mirrors the `DW_TAG_lexical_block` DWARF tag
  class LIEF_API LexicalBlock {
    public:
    LexicalBlock() = delete;
    LexicalBlock(std::unique_ptr<details::FunctionLexicalBlock> impl);

    /// Create a sub-block with the given low/high addresses.
    std::unique_ptr<LexicalBlock> add_block(uint64_t start,
                                            uint64_t end) LIEF_LIFETIMEBOUND;

    /// Create a sub-block with the given range of addresses.
    std::unique_ptr<LexicalBlock>
        add_block(const std::vector<range_t>& range) LIEF_LIFETIMEBOUND;

    /// Create a `DW_AT_description` entry with the description
    /// provided in parameter.
    LexicalBlock&
        add_description(const std::string& description) LIEF_LIFETIMEBOUND;

    /// Create a `DW_AT_name` entry to associate a name to this entry
    LexicalBlock& add_name(const std::string& name) LIEF_LIFETIMEBOUND;

    ~LexicalBlock();

    private:
    std::unique_ptr<details::FunctionLexicalBlock> impl_;
  };

  /// This class mirrors the `DW_TAG_label` DWARF tag
  class LIEF_API Label {
    public:
    Label() = delete;
    Label(std::unique_ptr<details::FunctionLabel> impl);

    ~Label();

    private:
    std::unique_ptr<details::FunctionLabel> impl_;
  };

  Function() = delete;
  Function(std::unique_ptr<details::Function> impl);

  /// Set the address of this function by defining `DW_AT_entry_pc`
  Function& set_address(uint64_t addr) LIEF_LIFETIMEBOUND;

  /// Set the upper and lower bound addresses for this function. This assumes
  /// that the function is contiguous between `low` and `high`.
  ///
  /// Underneath, the function defines `DW_AT_low_pc` and `DW_AT_high_pc`
  Function& set_low_high(uint64_t low, uint64_t high) LIEF_LIFETIMEBOUND;

  /// Set the ranges of addresses owned by the implementation of this function
  /// by setting the `DW_AT_ranges` attribute.
  ///
  /// This setter should be used for non-contiguous functions.
  Function& set_ranges(const std::vector<range_t>& ranges) LIEF_LIFETIMEBOUND;

  /// Set the function as external by defining `DW_AT_external` to true.
  /// This means that the function is **imported** by the current compilation
  /// unit.
  Function& set_external() LIEF_LIFETIMEBOUND;

  /// Set the return type of this function
  Function& set_return_type(const Type& type) LIEF_LIFETIMEBOUND;

  /// Add a parameter to the current function
  std::unique_ptr<Parameter> add_parameter(const std::string& name,
                                           const Type& type) LIEF_LIFETIMEBOUND;

  /// Create a stack-based variable owned by the current function
  std::unique_ptr<Variable>
      create_stack_variable(const std::string& name) LIEF_LIFETIMEBOUND;

  /// Add a lexical block with the given range
  std::unique_ptr<LexicalBlock> add_lexical_block(uint64_t start,
                                                  uint64_t end) LIEF_LIFETIMEBOUND;

  /// Add a label at the given address
  std::unique_ptr<Label> add_label(uint64_t addr,
                                   const std::string& label) LIEF_LIFETIMEBOUND;

  /// Create a `DW_AT_description` entry with the description
  /// provided in parameter.
  Function& add_description(const std::string& description) LIEF_LIFETIMEBOUND;

  ~Function();

  private:
  std::unique_ptr<details::Function> impl_;
};

}
}
}
#endif

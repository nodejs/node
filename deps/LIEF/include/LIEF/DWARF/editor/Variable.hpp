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
#ifndef LIEF_DWARF_EDITOR_VARIABLE_H
#define LIEF_DWARF_EDITOR_VARIABLE_H
#include <cstdint>
#include <memory>
#include <string>

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace dwarf {
namespace editor {
class Type;

namespace details {
class Variable;
}

/// This class represents an **editable** DWARF variable which can be
/// scoped by a function or a compilation unit (`DW_TAG_variable`)
class LIEF_API Variable {
  public:
  Variable() = delete;
  Variable(std::unique_ptr<details::Variable> impl);

  /// Set the global address of this variable. Setting this address is only
  /// relevant in the case of a static global variable. For stack variable, you
  /// should use set_stack_offset.
  ///
  /// This function sets the `DW_AT_location` attribute
  Variable& set_addr(uint64_t address) LIEF_LIFETIMEBOUND;

  /// Set the stack offset of this variable.
  ///
  /// This function sets the `DW_AT_location` attribute
  Variable& set_stack_offset(uint64_t offset) LIEF_LIFETIMEBOUND;

  /// Mark this variable as **imported**
  Variable& set_external() LIEF_LIFETIMEBOUND;

  /// Set the type of the current variable
  Variable& set_type(const Type& type) LIEF_LIFETIMEBOUND;

  /// Create a `DW_AT_description` entry with the description
  /// provided in parameter.
  Variable& add_description(const std::string& description) LIEF_LIFETIMEBOUND;

  ~Variable();

  private:
  std::unique_ptr<details::Variable> impl_;
};

}
}
}
#endif

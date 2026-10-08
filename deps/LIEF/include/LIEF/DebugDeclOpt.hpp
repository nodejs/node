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
#ifndef LIEF_DEBUG_DECL_OPT_H
#define LIEF_DEBUG_DECL_OPT_H
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace details {
class DeclOpt;
}

/// Configuration options for generated code from debug info.
///
/// This structure configures how the debug information (DWARF/PDB) translated
/// into an AST is generated. You can use it to configure the indentation, and
/// the information to generate when translating DWARF/PDB into C++-like
/// definitions
class LIEF_API DeclOpt {
  public:
  /// Mapping between a type name and a user-friendly aliases. (e.g.
  /// ``std::basic_string<char,std::char_traits<char>,std::allocator<char>>`` ->
  /// ``std::string``).
  using type_aliases_t = std::unordered_map<std::string, std::string>;

  DeclOpt();

  DeclOpt(const DeclOpt& other);
  DeclOpt& operator=(const DeclOpt& other);

  DeclOpt(DeclOpt&& other) noexcept;
  DeclOpt& operator=(DeclOpt&& other) noexcept;

  ~DeclOpt();

  /// The number of spaces for indentation.
  uint32_t indentation() const;
  DeclOpt& indentation(uint32_t value) LIEF_LIFETIMEBOUND;

  /// Prefer C++ syntax over C syntax.
  ///
  /// If true, the output will use C++ features (e.g. `bool` keyword)
  bool is_cpp() const;
  DeclOpt& is_cpp(bool value) LIEF_LIFETIMEBOUND;

  /// Enable extended comments and annotations.
  ///
  /// If true, the generated code will include comments containing low-level
  /// details such as memory addresses, offsets, type sizes, and original
  /// source locations.
  bool show_extended_annotations() const;
  DeclOpt& show_extended_annotations(bool value) LIEF_LIFETIMEBOUND;

  /// Include full type definitions.
  ///
  /// If true, the output will contain the full definition of types (structs,
  /// enums, unions).
  bool include_types() const;
  DeclOpt& include_types(bool value) LIEF_LIFETIMEBOUND;

  /// Emit a function body listing its local / stack variables
  bool include_locals() const;
  DeclOpt& include_locals(bool value) LIEF_LIFETIMEBOUND;

  /// Resolve type aliases (sugar).
  ///
  /// If true, typedef and type aliases are replaced by their underlying
  /// canonical types (e.g., `uint32_t` might become `unsigned int`).
  bool desugar() const;
  DeclOpt& desugar(bool value) LIEF_LIFETIMEBOUND;

  /// Show the relative offset of each field/attribute in structures.
  ///
  /// If true, every member of a structure is prefixed with its byte offset,
  ///
  /// ```cpp
  /// struct Foo {
  ///   /* 0x00 */ int A;
  ///   /* 0x04 */ int B;
  /// };
  /// ```
  bool show_field_offsets() const;
  DeclOpt& show_field_offsets(bool value) LIEF_LIFETIMEBOUND;

  /// Mapping of type names to user-friendly aliases
  const type_aliases_t& type_aliases() const;
  DeclOpt& type_aliases(type_aliases_t aliases) LIEF_LIFETIMEBOUND;

  /// Register a single type alias.
  DeclOpt& add_type_alias(std::string name, std::string alias) LIEF_LIFETIMEBOUND;

  private:
  std::unique_ptr<details::DeclOpt> impl_;
};

}
#endif

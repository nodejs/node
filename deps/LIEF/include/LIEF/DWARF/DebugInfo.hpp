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
#ifndef LIEF_DWARF_INFO_H
#define LIEF_DWARF_INFO_H
#include <memory>
#include <string>

#include "LIEF/iterators.hpp"
#include "LIEF/Abstract/DebugInfo.hpp"
#include "LIEF/DWARF/CompilationUnit.hpp"

#include "LIEF/visibility.h"

namespace LIEF {
/// Namespace for the DWARF debug format
namespace dwarf {
class Function;
class Variable;

/// This class represents a DWARF debug information. It can embed different
/// compilation units which can be accessed through compilation_units() .
///
/// This class can be instantiated from LIEF::Binary::debug_info() or load()
class LIEF_API DebugInfo : public LIEF::DebugInfo {
  public:
  using LIEF::DebugInfo::DebugInfo;

  static std::unique_ptr<DebugInfo> from_file(const std::string& path);

  /// Iterator over the CompilationUnit
  using compilation_units_it = iterator_range<CompilationUnit::Iterator>;

  /// Try to find the function with the given name (mangled or not)
  ///
  /// ```cpp
  /// const DebugInfo& info = ...;
  /// if (auto func = info.find_function("_ZNSt6localeD1Ev")) {
  ///   // Found
  /// }
  /// if (auto func = info.find_function("std::locale::~locale()")) {
  ///   // Found
  /// }
  /// ```
  std::unique_ptr<Function> find_function(const std::string& name) const;

  /// Try to find the function at the given **virtual** address
  std::unique_ptr<Function> find_function(uint64_t addr) const;

  /// Try to find the variable with the given name. This name can be mangled or
  /// not.
  std::unique_ptr<Variable> find_variable(const std::string& name) const;

  /// Try to find the variable at the given **virtual** address
  std::unique_ptr<Variable> find_variable(uint64_t addr) const;

  /// Try to find the type with the given name
  std::unique_ptr<Type> find_type(const std::string& name) const;

  /// Iterator on the CompilationUnit embedded in this dwarf
  compilation_units_it compilation_units() const;

  /// Attempt to resolve the address of the function specified by `name`.
  optional<uint64_t> find_function_address(const std::string& name) const override;

  FORMAT format() const override {
    return LIEF::DebugInfo::FORMAT::DWARF;
  }

  static bool classof(const LIEF::DebugInfo* info) {
    return info->format() == LIEF::DebugInfo::FORMAT::DWARF;
  }

  ~DebugInfo() override = default;
};


/// Load DWARF file from the given path
inline std::unique_ptr<DebugInfo> load(const std::string& dwarf_path) {
  return DebugInfo::from_file(dwarf_path);
}

}
}
#endif

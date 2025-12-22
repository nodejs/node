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
#ifndef LIEF_PDB_INFO_H
#define LIEF_PDB_INFO_H
#include <memory>
#include <string>
#include <ostream>

#include "LIEF/iterators.hpp"
#include "LIEF/Abstract/DebugInfo.hpp"

#include "LIEF/PDB/CompilationUnit.hpp"
#include "LIEF/PDB/PublicSymbol.hpp"
#include "LIEF/PDB/Type.hpp"

#include "LIEF/visibility.h"

namespace LIEF {
namespace pdb {
class Function;

/// This class provides an interface for PDB files.
/// One can instantiate this class using LIEF::pdb::load() or
/// LIEF::pdb::DebugInfo::from_file
class LIEF_API DebugInfo : public LIEF::DebugInfo {
  public:
  using LIEF::DebugInfo::DebugInfo;

  /// Iterator over the CompilationUnit
  using compilation_units_it = iterator_range<CompilationUnit::Iterator>;

  /// Iterator over the symbols located in the PDB public symbol stream
  using public_symbols_it = iterator_range<PublicSymbol::Iterator>;

  /// Iterator over the PDB's types
  using types_it = iterator_range<Type::Iterator>;

  /// Instantiate this class from the given PDB file. It returns a nullptr
  /// if the PDB can't be processed.
  static std::unique_ptr<DebugInfo> from_file(const std::string& pdb_path);

  FORMAT format() const override {
    return LIEF::DebugInfo::FORMAT::PDB;
  }

  static bool classof(const LIEF::DebugInfo* info) {
    return info->format() == LIEF::DebugInfo::FORMAT::PDB;
  }

  /// Iterator over the CompilationUnit from the PDB's DBI stream.
  /// CompilationUnit are also named "Module" in the PDB's official documentation
  compilation_units_it compilation_units() const;

  /// Return an iterator over the public symbol stream
  public_symbols_it public_symbols() const;

  /// Return an iterator over the different types registered in this PDB.
  types_it types() const;

  /// Find the type with the given name
  std::unique_ptr<Type> find_type(const std::string& name) const;

  /// Try to find the PublicSymbol from the given name (based on the public symbol stream)
  ///
  /// The function returns a nullptr if the symbol can't be found
  ///
  /// ```cpp
  /// const DebugInfo& info = ...;
  /// if (auto found = info.find_public_symbol("MiSyncSystemPdes")) {
  ///   // FOUND!
  /// }
  /// ```
  std::unique_ptr<PublicSymbol> find_public_symbol(const std::string& name) const;

  /// Attempt to resolve the address of the function specified by `name`.
  optional<uint64_t> find_function_address(const std::string& name) const override;

  /// The number of times the PDB file has been written.
  uint32_t age() const;

  /// Unique identifier of the PDB file
  std::string guid() const;

  /// Pretty representation
  std::string to_string() const;

  friend LIEF_API
    std::ostream& operator<<(std::ostream& os, const DebugInfo& dbg)
  {
    os << dbg.to_string();
    return os;
  }

  ~DebugInfo() override = default;
};


/// Load the PDB file from the given path
inline std::unique_ptr<DebugInfo> load(const std::string& pdb_path) {
  return DebugInfo::from_file(pdb_path);
}

}
}
#endif

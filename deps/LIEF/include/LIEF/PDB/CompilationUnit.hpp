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
#ifndef LIEF_PDB_COMPILATION_UNIT_H
#define LIEF_PDB_COMPILATION_UNIT_H
#include <memory>
#include <string>
#include <vector>
#include <ostream>

#include "LIEF/iterators.hpp"
#include "LIEF/DebugDeclOpt.hpp"
#include "LIEF/PDB/Function.hpp"

#include "LIEF/visibility.h"

namespace LIEF {
namespace pdb {
class BuildMetadata;

namespace details {
class CompilationUnit;
class CompilationUnitIt;
}

/// This class represents a CompilationUnit (or Module) in a PDB file
class LIEF_API CompilationUnit {
  public:
  class Iterator final
    : public iterator_facade_base<Iterator, std::bidirectional_iterator_tag,
                                  CompilationUnit, std::ptrdiff_t,
                                  const CompilationUnit*, const CompilationUnit&> {
    public:
    using implementation = details::CompilationUnitIt;
    using iterator_facade_base::operator++;
    using iterator_facade_base::operator--;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::CompilationUnitIt> impl);

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

    LIEF_API const CompilationUnit& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const CompilationUnit* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the compilation unit at the current position
    /// to the caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<CompilationUnit> yield();

    private:
    void load() const;

    std::unique_ptr<details::CompilationUnitIt> impl_;
    mutable std::unique_ptr<CompilationUnit> cached_;
  };

  /// Iterator over the source files (std::string)
  using sources_iterator =
      iterator_range<std::vector<std::string>::const_iterator>;

  using function_iterator = iterator_range<Function::Iterator>;

  CompilationUnit(std::unique_ptr<details::CompilationUnit> impl);
  ~CompilationUnit();

  /// Name (or path) to the COFF object (`.obj`) associated with this
  /// compilation unit (e.g.
  /// `e:\obj.amd64fre\minkernel\ntos\hvl\mp\objfre\amd64\hvlp.obj`)
  std::string module_name() const;

  /// Name or path to the original binary object (COFF, Archive) in which
  /// the compilation unit was located before being linked.
  /// e.g. `e:\obj.amd64fre\minkernel\ntos\hvl\mp\objfre\amd64\hvl.lib`
  std::string object_filename() const;

  /// Iterator over the sources files that compose this compilation unit.
  /// These files also include **headers** (`.h, .hpp`, ...).
  sources_iterator sources() const LIEF_LIFETIMEBOUND;

  /// Return an iterator over the function defined in this compilation unit.
  /// If the PDB does not contain or has an empty DBI stream, it returns
  /// an empty iterator.
  function_iterator functions() const LIEF_LIFETIMEBOUND;

  /// Return build metadata such as the version of the compiler or
  /// the original source language of this compilation unit
  std::unique_ptr<BuildMetadata> build_metadata() const LIEF_LIFETIMEBOUND;

  /// Generate a C/C++ definition for the functions defined in this
  /// compilation unit.
  std::string to_decl(const DeclOpt& opt = DeclOpt()) const;

  std::string to_string() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os,
                                           const CompilationUnit& CU) {
    os << CU.to_string();
    return os;
  }

  private:
  std::unique_ptr<details::CompilationUnit> impl_;
};

}
}
#endif

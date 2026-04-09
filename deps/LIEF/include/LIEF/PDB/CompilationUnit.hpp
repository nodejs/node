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
#ifndef LIEF_PDB_COMPILATION_UNIT_H
#define LIEF_PDB_COMPILATION_UNIT_H
#include <memory>
#include <string>
#include <vector>
#include <ostream>

#include "LIEF/iterators.hpp"
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
  class LIEF_API Iterator {
    public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::unique_ptr<CompilationUnit>;
    using difference_type = std::ptrdiff_t;
    using pointer = CompilationUnit*;
    using reference = CompilationUnit&;
    using implementation = details::CompilationUnitIt;

    class PointerProxy {
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
    Iterator(Iterator&&);
    Iterator(std::unique_ptr<details::CompilationUnitIt> impl);
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

    std::unique_ptr<CompilationUnit> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::CompilationUnitIt> impl_;
  };

  /// Iterator over the sources file (std::string)
  using sources_iterator = iterator_range<std::vector<std::string>::const_iterator>;

  using function_iterator = iterator_range<Function::Iterator>;

  CompilationUnit(std::unique_ptr<details::CompilationUnit> impl);
  ~CompilationUnit();

  /// Name (or path) to the COFF object (`.obj`) associated with this
  /// compilation unit (e.g. `e:\obj.amd64fre\minkernel\ntos\hvl\mp\objfre\amd64\hvlp.obj`)
  std::string module_name() const;

  /// Name of path to the original binary object (COFF, Archive) in which
  /// the compilation unit was located before being linked.
  /// e.g. `e:\obj.amd64fre\minkernel\ntos\hvl\mp\objfre\amd64\hvl.lib`
  std::string object_filename() const;

  /// Iterator over the sources files that compose this compilation unit.
  /// These files also include **headers** (`.h, .hpp`, ...).
  sources_iterator sources() const;

  /// Return an iterator over the function defined in this compilation unit.
  /// If the PDB does not contain or has an empty DBI stream, it returns
  /// an empty iterator.
  function_iterator functions() const;

  /// Return build metadata such as the version of the compiler or
  /// the original source language of this compilation unit
  std::unique_ptr<BuildMetadata> build_metadata() const;

  std::string to_string() const;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const CompilationUnit& CU)
  {
    os << CU.to_string();
    return os;
  }

  private:
  std::unique_ptr<details::CompilationUnit> impl_;
};

}
}
#endif


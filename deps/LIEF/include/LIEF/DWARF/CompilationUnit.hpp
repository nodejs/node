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
#ifndef LIEF_DWARF_COMPILATION_UNIT_H
#define LIEF_DWARF_COMPILATION_UNIT_H
#include <memory>
#include <string>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/range.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/DWARF/Function.hpp"
#include "LIEF/DWARF/Type.hpp"

namespace LIEF {
namespace dwarf {

namespace details {
class CompilationUnit;
class CompilationUnitIt;
}

/// This class represents a DWARF compilation unit
class LIEF_API CompilationUnit {
  public:
  class LIEF_API Iterator {
    public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::unique_ptr<CompilationUnit>;
    using difference_type = std::ptrdiff_t;
    using pointer = CompilationUnit*;
    using reference = std::unique_ptr<CompilationUnit>&;
    using implementation = details::CompilationUnitIt;

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

  /// Iterator over the dwarf::Function
  using functions_it = iterator_range<Function::Iterator>;

  /// Iterator over the dwarf::Type
  using types_it = iterator_range<Type::Iterator>;

  /// Iterator over the CompilationUnit's variables
  using vars_it = iterator_range<Variable::Iterator>;

  /// Languages supported by the DWARF (v5) format.
  /// See: https://dwarfstd.org/languages.html
  ///
  /// Some languages (like C++11, C++17, ..) have a version (11, 17, ...) which
  /// is stored in a dedicated attribute: #version
  class Language {
    public:
    enum LANG : uint32_t {
      UNKNOWN = 0,
      C,
      CPP,
      RUST,
      DART,
      MODULA,
      FORTRAN,
      SWIFT,
      D,
      JAVA,
      COBOL,
    };

    /// The language itself
    LANG lang = UNKNOWN;

    /// Version of the language (e.g. 17 for C++17)
    uint32_t version = 0;

    Language() = default;
    Language(LANG lang, uint32_t version) :
      lang(lang), version(version)
    {}
    Language(LANG lang) :
      Language(lang, 0)
    {}

    Language(const Language&) = default;
    Language& operator=(const Language&) = default;
    Language(Language&&) = default;
    Language& operator=(Language&&) = default;
    ~Language() = default;
  };
  CompilationUnit(std::unique_ptr<details::CompilationUnit> impl);
  ~CompilationUnit();

  /// Name of the file associated with this compilation unit (e.g. `test.cpp`)
  /// Return an **empty** string if the name is not found or can't be resolved
  ///
  /// This value matches the `DW_AT_name` attribute
  std::string name() const;

  /// Information about the program (or library) that generated this compilation
  /// unit. For instance, it can output: `Debian clang version 17.0.6`.
  ///
  /// It returns an **empty** string if the producer is not present or can't be
  /// resolved
  ///
  /// This value matches the `DW_AT_producer` attribute
  std::string producer() const;

  /// Return the path to the directory in which the compilation took place for
  /// compiling this compilation unit (e.g. `/workdir/build`)
  ///
  /// It returns an **empty** string if the entry is not present or can't be
  /// resolved
  ///
  /// This value matches the `DW_AT_comp_dir` attribute
  std::string compilation_dir() const;

  /// Original Language of this compilation unit.
  ///
  /// This value matches the `DW_AT_language` attribute.
  Language language() const;

  /// Return the lowest virtual address owned by this compilation unit.
  uint64_t low_address() const;

  /// Return the highest virtual address owned by this compilation unit.
  uint64_t high_address() const;

  /// Return the size of the compilation unit according to its range of address.
  ///
  /// If the compilation is fragmented (i.e. there are some address ranges
  /// between the lowest address and the highest that are not owned by the CU),
  /// then it returns the sum of **all** the address ranges owned by this CU.
  ///
  /// If the compilation unit is **not** fragmented, then is basically returns
  /// `high_address - low_address`.
  uint64_t size() const;

  /// Return a list of address ranges owned by this compilation unit.
  ///
  /// If the compilation unit owns a contiguous range, it should return
  /// **a single** range.
  std::vector<range_t> ranges() const;

  /// Try to find the function whose name is given in parameter.
  ///
  /// The provided name can be demangled
  std::unique_ptr<Function> find_function(const std::string& name) const;

  /// Try to find the function at the given address
  std::unique_ptr<Function> find_function(uint64_t addr) const;

  /// Try to find the Variable at the given address
  std::unique_ptr<Variable> find_variable(uint64_t addr) const;

  /// Try to find the Variable with the given name
  std::unique_ptr<Variable> find_variable(const std::string& name) const;

  /// Return an iterator over the functions implemented in this compilation
  /// unit.
  ///
  /// Note that this iterator only iterates over the functions that have a
  /// **concrete** implementation in the compilation unit.
  ///
  /// For instance with this code:
  ///
  /// ```cpp
  /// inline const char* get_secret_env() {
  ///   return getenv("MY_SECRET_ENV");
  /// }
  ///
  /// int main() {
  ///   printf("%s", get_secret_env());
  ///   return 0;
  /// }
  /// ```
  ///
  /// The iterator will only return **one function** for `main` since
  /// `get_secret_env` is inlined and thus, its implementation is located in
  /// `main`.
  functions_it functions() const;

  /// Return an iterator over the functions **imported** in this compilation
  /// unit **but not** implemented.
  ///
  /// For instance with this code:
  ///
  /// ```cpp
  /// #include <cstdio>
  /// int main() {
  ///   printf("Hello\n");
  ///   return 0;
  /// }
  /// ```
  ///
  /// `printf` is imported from the standard libc so the function is returned by
  /// the iterator. On the other hand, `main()` is implemented in this
  /// compilation unit so it is not returned by imported_function() but
  /// functions().
  functions_it imported_functions() const;

  /// Return an iterator over the different types defined in this
  /// compilation unit.
  types_it types() const;


  /// Return an iterator over all the variables defined in the this compilation
  /// unit:
  ///
  /// ```cpp
  /// static int A = 1; // Returned by the iterator
  /// static const char* B = "Hello"; // Returned by the iterator
  ///
  /// int get() {
  ///   static int C = 2; // Returned by the iterator
  ///   return C;
  /// }
  /// ```
  vars_it variables() const;

  private:
  std::unique_ptr<details::CompilationUnit> impl_;
};

}
}
#endif


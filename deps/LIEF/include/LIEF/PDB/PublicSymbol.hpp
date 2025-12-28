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
#ifndef LIEF_PDB_PUBLIC_SYMBOL_H
#define LIEF_PDB_PUBLIC_SYMBOL_H
#include <cstdint>
#include <memory>
#include <string>
#include <ostream>

#include "LIEF/visibility.h"

namespace LIEF {
namespace pdb {

namespace details {
class PublicSymbol;
class PublicSymbolIt;
}

/// This class provides general information (RVA, name) about a symbol
/// from the PDB's public symbol stream (or Public symbol hash stream)
class LIEF_API PublicSymbol {
  public:
  class LIEF_API Iterator {
    public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::unique_ptr<PublicSymbol>;
    using difference_type = std::ptrdiff_t;
    using pointer = PublicSymbol*;
    using reference = PublicSymbol&;
    using implementation = details::PublicSymbolIt;

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
    Iterator(Iterator&&);
    Iterator(std::unique_ptr<details::PublicSymbolIt> impl);
    ~Iterator();

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS);

    friend LIEF_API bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    Iterator& operator++();

    Iterator operator++(int) {
      Iterator tmp = *static_cast<Iterator*>(this);
      ++*static_cast<Iterator *>(this);
      return tmp;
    }

    std::unique_ptr<PublicSymbol> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::PublicSymbolIt> impl_;
  };
  PublicSymbol(std::unique_ptr<details::PublicSymbol> impl);
  ~PublicSymbol();

  enum class FLAGS : uint32_t {
    NONE     = 0,
    CODE     = 1 << 0,
    FUNCTION = 1 << 1,
    MANAGED  = 1 << 2,
    MSIL     = 1 << 3,
  };

  /// Name of the symbol
  std::string name() const;

  /// Demangled representation of the symbol
  std::string demangled_name() const;

  /// Name of the section in which this symbol is defined (e.g. `.text`).
  ///
  /// This function returns an empty string if the section's name can't be found
  std::string section_name() const;

  /// **Relative** Virtual Address of this symbol.
  ///
  /// This function returns 0 if the RVA can't be computed.
  uint32_t RVA() const;

  std::string to_string() const;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const PublicSymbol& sym)
  {
    os << sym.to_string();
    return os;
  }

  private:
  std::unique_ptr<details::PublicSymbol> impl_;
};

}
}
#endif


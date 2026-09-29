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
#ifndef LIEF_PDB_TYPE_H
#define LIEF_PDB_TYPE_H
#include <cstdint>
#include <memory>
#include <string>

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/visibility.h"
#include "LIEF/optional.hpp"
#include "LIEF/DebugDeclOpt.hpp"

namespace LIEF {
namespace pdb {

namespace details {
class Type;
class TypeIt;
}

/// This is the base class for any PDB type
class LIEF_API Type {
  public:
  class Iterator final
    : public iterator_facade_base<Iterator, std::forward_iterator_tag, Type,
                                  std::ptrdiff_t, const Type*, const Type&> {
    public:
    using implementation = details::TypeIt;
    using iterator_facade_base::operator++;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::TypeIt> impl);

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

    LIEF_API const Type& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Type* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the type at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Type> yield();

    private:
    void load() const;

    std::unique_ptr<details::TypeIt> impl_;
    mutable std::unique_ptr<Type> cached_;
  };

  LIEF_LOCAL Type(std::unique_ptr<details::Type> impl);

  enum class KIND {
    UNKNOWN = 0,
    CLASS,
    POINTER,
    SIMPLE,
    ENUM,
    FUNCTION,
    MODIFIER,
    BITFIELD,
    ARRAY,
    UNION,
    STRUCTURE,
    INTERFACE,
  };

  KIND kind() const;

  /// Size of the type. This size should match the value of `sizeof(...)`
  /// applied to this type.
  optional<uint64_t> size() const;

  /// Type's name (if present)
  optional<std::string> name() const;

  /// Generates a C/C++ definition for this type
  std::string to_decl(const DeclOpt& opt = DeclOpt()) const;

  template<class T>
  const T* as() const {
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  static std::unique_ptr<Type>
      create(std::unique_ptr<details::Type> impl LIEF_LIFETIMEBOUND);

  virtual ~Type();

  protected:
  std::unique_ptr<details::Type> impl_;
};

}
}
#endif

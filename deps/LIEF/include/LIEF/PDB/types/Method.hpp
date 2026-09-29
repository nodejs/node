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
#ifndef LIEF_PDB_TYPE_METHOD_H
#define LIEF_PDB_TYPE_METHOD_H

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/visibility.h"

#include <string>
#include <memory>
#include <cstdint>

namespace LIEF {
namespace pdb {
class Type;
namespace types {

namespace details {
class Method;
class MethodIt;
}

/// This class represents a Method (`LF_ONEMETHOD`) that can be defined in
/// a ClassLike PDB type (Class, Structure, Union, Interface).
class LIEF_API Method {
  public:
  class Iterator final
    : public iterator_facade_base<Iterator, std::forward_iterator_tag, Method,
                                  std::ptrdiff_t, const Method*, const Method&> {
    public:
    using implementation = details::MethodIt;
    using iterator_facade_base::operator++;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::MethodIt> impl);

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

    LIEF_API const Method& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Method* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the method at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Method> yield();

    private:
    void load() const;

    std::unique_ptr<details::MethodIt> impl_;
    mutable std::unique_ptr<Method> cached_;
  };

  public:
  /// The type (or property) of the method.
  enum class TYPE {
    /// Regular instance method
    VANILLA = 0x00,

    /// Virtual method
    VIRTUAL = 0x01,

    /// Static method
    STATIC = 0x02,

    /// Friend method
    FRIEND = 0x03,

    /// Virtual method that introduces a new vtable slot
    INTRODUCING_VIRTUAL = 0x04,

    /// Pure virtual method (abstract)
    PURE_VIRTUAL = 0x05,

    /// Pure virtual method that introduces a new vtable slot
    PURE_INTRODUCING_VIRTUAL = 0x06,
  };

  /// Visibility access for the method.
  enum class ACCESS : uint8_t {
    NONE = 0,      /// No access specifier (or unknown)
    PRIVATE = 1,   /// Private access
    PROTECTED = 2, /// Protected access
    PUBLIC = 3,    /// Public access
  };

  Method(std::unique_ptr<details::Method> impl);

  /// Name of the method
  std::string name() const;

  /// Type/Properties of the method (virtual, static, etc.)
  TYPE type() const;

  /// Visibility access (public, private, ...)
  ACCESS access() const;

  ~Method();

  private:
  std::unique_ptr<details::Method> impl_;
};

}
}
}
#endif

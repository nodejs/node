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
#ifndef LIEF_PDB_TYPE_ATTRIBUTE_H
#define LIEF_PDB_TYPE_ATTRIBUTE_H

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/visibility.h"

#include <cstdint>
#include <string>
#include <memory>

namespace LIEF {
namespace pdb {
class Type;
namespace types {

namespace details {
class Attribute;
class AttributeIt;
}

/// This class represents an attribute (`LF_MEMBER`) in an aggregate (class,
/// struct, union, ...)
class LIEF_API Attribute {
  public:
  class Iterator final
    : public iterator_facade_base<Iterator, std::forward_iterator_tag, Attribute,
                                  std::ptrdiff_t, const Attribute*,
                                  const Attribute&> {
    public:
    using implementation = details::AttributeIt;
    using iterator_facade_base::operator++;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::AttributeIt> impl);

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

    LIEF_API const Attribute& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Attribute* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the attribute at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Attribute> yield();

    private:
    void load() const;

    std::unique_ptr<details::AttributeIt> impl_;
    mutable std::unique_ptr<Attribute> cached_;
  };

  public:
  Attribute(std::unique_ptr<details::Attribute> impl);

  /// Name of the attribute
  std::string name() const;

  /// Type of this attribute
  std::unique_ptr<Type> type() const LIEF_LIFETIMEBOUND;

  /// Offset of this attribute in the aggregate
  uint64_t field_offset() const;

  ~Attribute();

  private:
  std::unique_ptr<details::Attribute> impl_;
};

}
}
}
#endif

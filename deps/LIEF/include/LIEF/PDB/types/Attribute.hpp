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
#ifndef LIEF_PDB_TYPE_ATTRIBUTE_H
#define LIEF_PDB_TYPE_ATTRIBUTE_H

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
  class LIEF_API Iterator {
    public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::unique_ptr<Attribute>;
    using difference_type = std::ptrdiff_t;
    using pointer = Attribute*;
    using reference = Attribute&;
    using implementation = details::AttributeIt;

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
    Iterator(std::unique_ptr<details::AttributeIt> impl);
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

    std::unique_ptr<Attribute> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::AttributeIt> impl_;
  };
  public:
  Attribute(std::unique_ptr<details::Attribute> impl);

  /// Name of the attribute
  std::string name() const;

  /// Type of this attribute
  std::unique_ptr<Type> type() const;

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


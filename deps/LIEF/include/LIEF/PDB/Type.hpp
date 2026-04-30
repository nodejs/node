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
#ifndef LIEF_PDB_TYPE_H
#define LIEF_PDB_TYPE_H
#include <memory>

#include "LIEF/visibility.h"

namespace LIEF {
namespace pdb {

namespace details {
class Type;
class TypeIt;
}

/// This is the base class for any PDB type
class LIEF_API Type {
  public:
  class LIEF_API Iterator {
    public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::unique_ptr<Type>;
    using difference_type = std::ptrdiff_t;
    using pointer = Type*;
    using reference = Type&;
    using implementation = details::TypeIt;

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
    Iterator(std::unique_ptr<details::TypeIt> impl);
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

    std::unique_ptr<Type> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::TypeIt> impl_;
  };

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

  template<class T>
  const T* as() const {
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  static std::unique_ptr<Type> create(std::unique_ptr<details::Type> impl);

  virtual ~Type();

  protected:
  Type(std::unique_ptr<details::Type> impl);
  std::unique_ptr<details::Type> impl_;
};

}
}
#endif


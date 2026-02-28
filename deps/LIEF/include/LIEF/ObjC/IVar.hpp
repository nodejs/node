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
#ifndef LIEF_OBJC_IVAR_H
#define LIEF_OBJC_IVAR_H
#include <LIEF/visibility.h>
#include <LIEF/ObjC/Method.hpp>
#include <LIEF/ObjC/Property.hpp>
#include <LIEF/iterators.hpp>

#include <memory>
#include <string>

namespace LIEF {
namespace objc {

namespace details {
class IVar;
class IVarIt;
}

/// This class represents an instance variable (ivar)
class LIEF_API IVar {
  public:
  class LIEF_API Iterator {
    public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::unique_ptr<IVar>;
    using difference_type = std::ptrdiff_t;
    using pointer = IVar*;
    using reference = std::unique_ptr<IVar>&;
    using implementation = details::IVarIt;

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
    Iterator(std::unique_ptr<details::IVarIt> impl);
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

    std::unique_ptr<IVar> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::IVarIt> impl_;
  };

  public:
  IVar(std::unique_ptr<details::IVar> impl);

  /// Name of the instance variable
  std::string name() const;

  /// Type of the instance var in its mangled representation (`[29i]`)
  std::string mangled_type() const;

  ~IVar();
  private:
  std::unique_ptr<details::IVar> impl_;
};

}
}
#endif

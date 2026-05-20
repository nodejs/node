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
#ifndef LIEF_OBJC_METHOD_H
#define LIEF_OBJC_METHOD_H
#include <LIEF/visibility.h>

#include <cstdint>
#include <memory>
#include <string>

namespace LIEF {
namespace objc {

namespace details {
class Method;
class MethodIt;
}

/// This class represents an Objective-C Method
class LIEF_API Method {
  public:
  class LIEF_API Iterator {
    public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::unique_ptr<Method>;
    using difference_type = std::ptrdiff_t;
    using pointer = Method*;
    using reference = std::unique_ptr<Method>&;
    using implementation = details::MethodIt;

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
    Iterator(std::unique_ptr<details::MethodIt> impl);
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

    std::unique_ptr<Method> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::MethodIt> impl_;
  };

  public:
  Method(std::unique_ptr<details::Method> impl);

  /// Name of the method
  std::string name() const;

  /// Prototype of the method in its mangled representation (e.g. `@16@0:8`)
  std::string mangled_type() const;

  /// Virtual address where this method is implemented in the binary
  uintptr_t address() const;

  /// Whether it's an instance method
  bool is_instance() const;

  ~Method();
  private:
  std::unique_ptr<details::Method> impl_;
};

}
}
#endif

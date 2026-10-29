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
#ifndef LIEF_OBJC_METHOD_H
#define LIEF_OBJC_METHOD_H
#include <LIEF/iterators.hpp>
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
  class Iterator final
    : public iterator_facade_base<Iterator, std::bidirectional_iterator_tag,
                                  Method, std::ptrdiff_t, const Method*,
                                  const Method&> {
    public:
    using implementation = details::MethodIt;
    using iterator_facade_base::operator++;
    using iterator_facade_base::operator--;

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

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API Iterator& operator--();

    LIEF_API const Method& operator*() const;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Method* operator->() const;

    /// Transfer ownership of the method at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Method> yield();

    private:
    void load() const;

    std::unique_ptr<details::MethodIt> impl_;
    mutable std::unique_ptr<Method> cached_;
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

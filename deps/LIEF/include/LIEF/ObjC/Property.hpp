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
#ifndef LIEF_OBJC_PROPERTY_H
#define LIEF_OBJC_PROPERTY_H
#include <LIEF/iterators.hpp>
#include <LIEF/visibility.h>

#include <memory>
#include <string>

namespace LIEF {
namespace objc {

namespace details {
class Property;
class PropertyIt;
}

/// This class represents a `@property` in Objective-C
class LIEF_API Property {
  public:
  class Iterator final
    : public iterator_facade_base<Iterator, std::bidirectional_iterator_tag,
                                  Property, std::ptrdiff_t, const Property*,
                                  const Property&> {
    public:
    using implementation = details::PropertyIt;
    using iterator_facade_base::operator++;
    using iterator_facade_base::operator--;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::PropertyIt> impl);

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

    LIEF_API const Property& operator*() const;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Property* operator->() const;

    /// Transfer ownership of the property at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Property> yield();

    private:
    void load() const;

    std::unique_ptr<details::PropertyIt> impl_;
    mutable std::unique_ptr<Property> cached_;
  };

  public:
  Property(std::unique_ptr<details::Property> impl);

  /// Name of the property
  std::string name() const;

  /// (raw) property's attributes (e.g. `T@"NSString",C,D,N`)
  std::string attribute() const;

  ~Property();

  private:
  std::unique_ptr<details::Property> impl_;
};

}
}
#endif

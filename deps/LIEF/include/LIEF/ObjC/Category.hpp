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
#ifndef LIEF_OBJC_CATEGORY_H
#define LIEF_OBJC_CATEGORY_H
#include <LIEF/compiler_attributes.hpp>
#include <LIEF/visibility.h>
#include <LIEF/iterators.hpp>

#include <LIEF/ObjC/Method.hpp>
#include <LIEF/ObjC/Property.hpp>
#include <LIEF/ObjC/Protocol.hpp>
#include <LIEF/ObjC/DeclOpt.hpp>

#include <memory>
#include <string>

namespace LIEF {
namespace objc {

namespace details {
class Category;
class CategoryIt;
}

/// This class represents an Objective-C category (e.g.
/// `@interface NSString (MyAdditions)`)
class LIEF_API Category {
  public:
  class Iterator final
    : public iterator_facade_base<Iterator, std::bidirectional_iterator_tag,
                                  Category, std::ptrdiff_t, const Category*,
                                  const Category&> {
    public:
    using implementation = details::CategoryIt;
    using iterator_facade_base::operator++;
    using iterator_facade_base::operator--;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::CategoryIt> impl);

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

    LIEF_API const Category& operator*() const;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Category* operator->() const;

    /// Transfer ownership of the category at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Category> yield();

    private:
    void load() const;

    std::unique_ptr<details::CategoryIt> impl_;
    mutable std::unique_ptr<Category> cached_;
  };

  public:
  /// Iterator for the category's methods
  using methods_t = iterator_range<Method::Iterator>;

  /// Iterator for the protocols adopted by this category
  using protocols_t = iterator_range<Protocol::Iterator>;

  /// Iterator for the properties declared by this category
  using properties_t = iterator_range<Property::Iterator>;

  Category(std::unique_ptr<details::Category> impl);

  /// Name of the category
  std::string name() const;

  /// (demangled) name of the class extended by this category
  std::string class_name() const;

  /// Iterator over the different methods defined by this category
  methods_t methods() const LIEF_LIFETIMEBOUND;

  /// Iterator over the different protocols adopted by this category
  protocols_t protocols() const LIEF_LIFETIMEBOUND;

  /// Iterator over the properties of this category
  properties_t properties() const LIEF_LIFETIMEBOUND;

  /// Generate a header-like string for this specific category.
  ///
  /// The generated output can be configured with DeclOpt
  std::string to_decl(const DeclOpt& opt = DeclOpt()) const;

  ~Category();

  private:
  std::unique_ptr<details::Category> impl_;
};

}
}
#endif

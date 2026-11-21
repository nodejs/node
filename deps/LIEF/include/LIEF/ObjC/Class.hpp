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
#ifndef LIEF_OBJC_CLASS_H
#define LIEF_OBJC_CLASS_H

#include <LIEF/visibility.h>
#include <LIEF/compiler_attributes.hpp>

#include <LIEF/ObjC/IVar.hpp>
#include <LIEF/ObjC/Protocol.hpp>
#include <LIEF/ObjC/Method.hpp>
#include <LIEF/ObjC/Property.hpp>
#include <LIEF/ObjC/DeclOpt.hpp>

#include <memory>
#include <string>

namespace LIEF {
namespace objc {

namespace details {
class Class;
class ClassIt;
}

/// This class represents an Objective-C class (`@interface`)
class LIEF_API Class {
  public:
  class Iterator final
    : public iterator_facade_base<Iterator, std::bidirectional_iterator_tag, Class,
                                  std::ptrdiff_t, const Class*, const Class&> {
    public:
    using implementation = details::ClassIt;
    using iterator_facade_base::operator++;
    using iterator_facade_base::operator--;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::ClassIt> impl);

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

    LIEF_API const Class& operator*() const;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Class* operator->() const;

    /// Transfer ownership of the class at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Class> yield();

    private:
    void load() const;

    std::unique_ptr<details::ClassIt> impl_;
    mutable std::unique_ptr<Class> cached_;
  };

  public:
  /// Iterator for the class's methods
  using methods_t = iterator_range<Method::Iterator>;

  /// Iterator for the protocols implemented by this class
  using protocols_t = iterator_range<Protocol::Iterator>;

  /// Iterator for the properties declared by this class
  using properties_t = iterator_range<Property::Iterator>;

  /// Iterator for the instance variables defined by this class
  using ivars_t = iterator_range<IVar::Iterator>;

  Class(std::unique_ptr<details::Class> impl);

  /// Name of the class
  std::string name() const;

  /// Demangled name of the class
  std::string demangled_name() const;

  /// Parent class in case of inheritance.
  ///
  /// This returns the superclass object **only** when it is defined in the same
  /// binary. For root classes (e.g. `NSObject`) or superclasses imported from
  /// another image, it returns a null pointer even though the name can still be
  /// resolved through super_name() / demangled_super_name().
  std::unique_ptr<Class> super_class() const LIEF_LIFETIMEBOUND;

  /// (raw) name of the superclass (empty for root classes or when it could not
  /// be resolved).
  std::string super_name() const;

  /// Demangled name of the superclass.
  std::string demangled_super_name() const;

  bool is_meta() const;

  /// Iterator over the different methods defined by this class
  methods_t methods() const LIEF_LIFETIMEBOUND;

  /// Iterator over the different protocols implemented by this class
  protocols_t protocols() const LIEF_LIFETIMEBOUND;

  /// Iterator over the properties of this class
  properties_t properties() const LIEF_LIFETIMEBOUND;

  /// Iterator over the different instance variables defined in this class
  ivars_t ivars() const LIEF_LIFETIMEBOUND;

  /// Generate a header-like string for this specific class.
  ///
  /// The generated output can be configured with DeclOpt
  std::string to_decl(const DeclOpt& opt = DeclOpt()) const;

  ~Class();

  private:
  std::unique_ptr<details::Class> impl_;
};

}
}
#endif

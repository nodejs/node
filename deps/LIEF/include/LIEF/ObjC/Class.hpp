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
#ifndef LIEF_OBJC_CLASS_H
#define LIEF_OBJC_CLASS_H

#include <LIEF/visibility.h>

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
  class LIEF_API Iterator {
    public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::unique_ptr<Class>;
    using difference_type = std::ptrdiff_t;
    using pointer = Class*;
    using reference = std::unique_ptr<Class>&;
    using implementation = details::ClassIt;

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
    Iterator(std::unique_ptr<details::ClassIt> impl);
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

    std::unique_ptr<Class> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::ClassIt> impl_;
  };

  public:
  /// Iterator for the class's method
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

  /// Parent class in case of inheritance
  std::unique_ptr<Class> super_class() const;

  bool is_meta() const;

  /// Iterator over the different methods defined by this class
  methods_t methods() const;

  /// Iterator over the different protocols implemented by this class
  protocols_t protocols() const;

  /// Iterator over the properties of this class
  properties_t properties() const;

  /// Iterator over the different instance variables defined in this class
  ivars_t ivars() const;

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

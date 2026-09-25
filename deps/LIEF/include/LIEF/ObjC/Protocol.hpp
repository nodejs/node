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
#ifndef LIEF_OBJC_PROTOCOL_H
#define LIEF_OBJC_PROTOCOL_H
#include <LIEF/visibility.h>
#include <LIEF/iterators.hpp>

#include <LIEF/ObjC/Method.hpp>
#include <LIEF/ObjC/Property.hpp>
#include <LIEF/ObjC/DeclOpt.hpp>

#include <memory>
#include <string>

namespace LIEF {
namespace objc {

namespace details {
class Protocol;
class ProtocolIt;
}

/// This class represents an Objective-C `@protocol`
class LIEF_API Protocol {
  public:
  class LIEF_API Iterator {
    public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::unique_ptr<Protocol>;
    using difference_type = std::ptrdiff_t;
    using pointer = Protocol*;
    using reference = std::unique_ptr<Protocol>&;
    using implementation = details::ProtocolIt;

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
    Iterator(std::unique_ptr<details::ProtocolIt> impl);
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

    std::unique_ptr<Protocol> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::ProtocolIt> impl_;
  };

  public:
  using methods_it = iterator_range<Method::Iterator>;
  using properties_it = iterator_range<Property::Iterator>;

  Protocol(std::unique_ptr<details::Protocol> impl);

  /// Mangled name of the protocol
  std::string mangled_name() const;

  /// Iterator over the methods that could be overridden
  methods_it optional_methods() const;

  /// Iterator over the methods of this protocol that must be implemented
  methods_it required_methods() const;

  /// Iterator over the properties defined in this protocol
  properties_it properties() const;

  /// Generate a header-like string for this specific protocol.
  ///
  /// The generated output can be configured with DeclOpt
  std::string to_decl(const DeclOpt& opt = DeclOpt()) const;

  ~Protocol();
  private:
  std::unique_ptr<details::Protocol> impl_;
};

}
}
#endif

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
#ifndef LIEF_OBJC_PROTOCOL_H
#define LIEF_OBJC_PROTOCOL_H
#include <LIEF/compiler_attributes.hpp>
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
  class Iterator final
    : public iterator_facade_base<Iterator, std::bidirectional_iterator_tag,
                                  Protocol, std::ptrdiff_t, const Protocol*,
                                  const Protocol&> {
    public:
    using implementation = details::ProtocolIt;
    using iterator_facade_base::operator++;
    using iterator_facade_base::operator--;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::ProtocolIt> impl);

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

    LIEF_API const Protocol& operator*() const;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Protocol* operator->() const;

    /// Transfer ownership of the protocol at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Protocol> yield();

    private:
    void load() const;

    std::unique_ptr<details::ProtocolIt> impl_;
    mutable std::unique_ptr<Protocol> cached_;
  };

  public:
  using methods_it = iterator_range<Method::Iterator>;
  using properties_it = iterator_range<Property::Iterator>;
  using protocols_it = iterator_range<Iterator>;

  Protocol(std::unique_ptr<details::Protocol> impl);

  /// Mangled name of the protocol
  std::string mangled_name() const;

  /// Iterator over the protocols adopted by this protocol (e.g. the
  /// `<Bar, Baz>` in `@protocol Foo <Bar, Baz>`).
  protocols_it protocols() const LIEF_LIFETIMEBOUND;

  /// Iterator over the methods that could be overridden
  methods_it optional_methods() const LIEF_LIFETIMEBOUND;

  /// Iterator over the methods of this protocol that must be implemented
  methods_it required_methods() const LIEF_LIFETIMEBOUND;

  /// Iterator over the properties defined in this protocol
  properties_it properties() const LIEF_LIFETIMEBOUND;

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

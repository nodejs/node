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
  class Iterator final
    : public iterator_facade_base<Iterator, std::bidirectional_iterator_tag, IVar,
                                  std::ptrdiff_t, const IVar*, const IVar&> {
    public:
    using implementation = details::IVarIt;
    using iterator_facade_base::operator++;
    using iterator_facade_base::operator--;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::IVarIt> impl);

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

    LIEF_API const IVar& operator*() const;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const IVar* operator->() const;

    /// Transfer ownership of the ivar at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<IVar> yield();

    private:
    void load() const;

    std::unique_ptr<details::IVarIt> impl_;
    mutable std::unique_ptr<IVar> cached_;
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

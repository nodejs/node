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
#ifndef LIEF_DWARF_SHARED_TYPE_H
#define LIEF_DWARF_SHARED_TYPE_H

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/visibility.h"
#include "LIEF/DWARF/Type.hpp"

namespace LIEF {
namespace dwarf {
namespace types {

/// This class represents a `DW_TAG_shared_type`
class LIEF_API Shared : public Type {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<Type, Args&&...>::value>::type>
  Shared(Args&&... args) :
    Type(std::forward<Args>(args)...) {}

  Shared(const Shared&) = delete;
  Shared& operator=(const Shared&) = delete;

  Shared(Shared&&) noexcept = default;
  Shared& operator=(Shared&&) noexcept = default;

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::SHARED;
  }

  /// The underlying type referenced by this shared-type.
  const Type* underlying_type() const LIEF_LIFETIMEBOUND;

  const Type* operator->() const LIEF_LIFETIMEBOUND {
    return underlying_.get();
  }

  const Type* operator*() const LIEF_LIFETIMEBOUND {
    return underlying_.get();
  }

  ~Shared() override;

  protected:
  mutable std::unique_ptr<Type> underlying_;
};

}
}
}
#endif

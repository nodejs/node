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
#ifndef LIEF_DWARF_SUBROUTINE_TYPE_H
#define LIEF_DWARF_SUBROUTINE_TYPE_H

#include "LIEF/visibility.h"
#include "LIEF/compiler_attributes.hpp"
#include "LIEF/DWARF/Type.hpp"

namespace LIEF {
namespace dwarf {
class Parameter;

namespace types {

/// This class represents a `DW_TAG_subroutine_type`
class LIEF_API Subroutine : public Type {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<Type, Args&&...>::value>::type>
  Subroutine(Args&&... args) :
    Type(std::forward<Args>(args)...) {}

  Subroutine(const Subroutine&) = delete;
  Subroutine& operator=(const Subroutine&) = delete;

  Subroutine(Subroutine&&) noexcept = default;
  Subroutine& operator=(Subroutine&&) noexcept = default;

  using parameters_t = std::vector<std::unique_ptr<Parameter>>;

  /// Return the dwarf::Type associated with the **return type** of this
  /// function
  std::unique_ptr<Type> return_type() const LIEF_LIFETIMEBOUND;

  /// Parameters of this subroutine
  parameters_t parameters() const LIEF_LIFETIMEBOUND;

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::SUBROUTINE;
  }

  ~Subroutine() override;
};

}
}
}
#endif

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
#ifndef LIEF_DWARF_TEMPLATE_ALIAS_H
#define LIEF_DWARF_TEMPLATE_ALIAS_H

#include "LIEF/visibility.h"
#include "LIEF/compiler_attributes.hpp"
#include "LIEF/DWARF/Type.hpp"

namespace LIEF {
namespace dwarf {
class Parameter;

namespace types {

/// This class represents a `DW_TAG_template_alias`
class LIEF_API TemplateAlias : public Type {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<Type, Args&&...>::value>::type>
  TemplateAlias(Args&&... args) :
    Type(std::forward<Args>(args)...) {}

  TemplateAlias(const TemplateAlias&) = delete;
  TemplateAlias& operator=(const TemplateAlias&) = delete;

  TemplateAlias(TemplateAlias&&) noexcept = default;
  TemplateAlias& operator=(TemplateAlias&&) noexcept = default;

  using parameters_t = std::vector<std::unique_ptr<Parameter>>;

  /// The underlying type aliased by this type.
  const Type* underlying_type() const LIEF_LIFETIMEBOUND;

  const Type* operator->() const LIEF_LIFETIMEBOUND {
    return underlying_type();
  }

  const Type& operator*() const LIEF_LIFETIMEBOUND {
    return *underlying_type();
  }

  /// Parameters associated with the underlying template
  parameters_t parameters() const LIEF_LIFETIMEBOUND;

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::TEMPLATE_ALIAS;
  }

  ~TemplateAlias() override;

  protected:
  mutable std::unique_ptr<Type> underlying_;
};

}
}
}
#endif

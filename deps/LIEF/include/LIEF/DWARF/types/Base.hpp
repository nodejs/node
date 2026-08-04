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
#ifndef LIEF_DWARF_TYPE_BASE_H
#define LIEF_DWARF_TYPE_BASE_H

#include "LIEF/visibility.h"
#include "LIEF/DWARF/Type.hpp"

namespace LIEF {
namespace dwarf {
namespace types {

/// This class wraps the `DW_TAG_base_type` type which can be used -- for
/// instance -- to represent integers or primitive types.
class LIEF_API Base : public Type {
  public:
  using Type::Type;

  enum class ENCODING {
    NONE = 0,

    /// Mirror `DW_ATE_signed`
    SIGNED,

    /// Mirror `DW_ATE_signed_char`
    SIGNED_CHAR,

    /// Mirror `DW_ATE_unsigned`
    UNSIGNED,

    /// Mirror `DW_ATE_unsigned_char`
    UNSIGNED_CHAR,

    /// Mirror `DW_ATE_float`
    FLOAT,

    /// Mirror `DW_ATE_boolean`
    BOOLEAN,

    /// Mirror `DW_ATE_address`
    ADDRESS,
  };

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::BASE;
  }

  /// Describe how the base type is encoded and should be interpreted
  ENCODING encoding() const;

  ~Base() override;
};

}
}
}
#endif



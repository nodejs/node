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
#ifndef LIEF_DWARF_EDITOR_BASE_TYPE_H
#define LIEF_DWARF_EDITOR_BASE_TYPE_H

#include <cstdint>
#include "LIEF/visibility.h"
#include "LIEF/DWARF/editor/Type.hpp"

namespace LIEF {
namespace dwarf {
namespace editor {

/// This class represents a primitive type like `int, char`.
class LIEF_API BaseType : public Type {
  public:
  using Type::Type;

  enum class ENCODING : uint32_t {
    NONE = 0,
    ADDRESS,
    SIGNED,
    SIGNED_CHAR,
    UNSIGNED,
    UNSIGNED_CHAR,
    BOOLEAN,
    FLOAT
  };

  static bool classof(const Type* type);

  ~BaseType() override = default;
};

}
}
}
#endif

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
#ifndef LIEF_DWARF_EDITOR_ENUM_TYPE_H
#define LIEF_DWARF_EDITOR_ENUM_TYPE_H
#include <cstdint>
#include <string>

#include "LIEF/visibility.h"
#include "LIEF/DWARF/editor/Type.hpp"

namespace LIEF {
namespace dwarf {
namespace editor {

namespace details {
class EnumValue;
}

/// This class represents an editable enum type (`DW_TAG_enumeration_type`)
class LIEF_API EnumType : public Type {
  public:
  using Type::Type;

  /// This class represents an enum value
  class LIEF_API Value {
    public:
    Value() = delete;
    Value(std::unique_ptr<details::EnumValue> impl);

    ~Value();
    private:
    std::unique_ptr<details::EnumValue> impl_;
  };

  /// Define the number of bytes required to hold an instance of the
  /// enumeration (`DW_AT_byte_size`).
  EnumType& set_size(uint64_t size);

  /// Add an enum value by specifying its name and its integer value
  std::unique_ptr<Value> add_value(const std::string& name, int64_t value);

  static bool classof(const Type* type);

  ~EnumType() override = default;
};

}
}
}
#endif

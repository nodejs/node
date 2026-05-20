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
#ifndef LIEF_DWARF_EDITOR_STRUCT_TYPE_H
#define LIEF_DWARF_EDITOR_STRUCT_TYPE_H

#include <cstdint>
#include <string>

#include "LIEF/visibility.h"
#include "LIEF/DWARF/editor/Type.hpp"

namespace LIEF {
namespace dwarf {
namespace editor {

namespace details {
class StructMember;
}

/// This class represents a struct-like type which can be:
///
/// - `DW_TAG_class_type`
/// - `DW_TAG_structure_type`
/// - `DW_TAG_union_type`
class LIEF_API StructType : public Type {
  public:
  using Type::Type;

  enum class TYPE : uint32_t {
    CLASS, /// Discriminant for `DW_TAG_class_type`
    STRUCT, /// Discriminant for `DW_TAG_structure_type`
    UNION, /// Discriminant for `DW_TAG_union_type`
  };

  /// This class represents a member of the struct-like
  class LIEF_API Member {
    public:
    Member() = delete;
    Member(std::unique_ptr<details::StructMember> impl);

    ~Member();
    private:
    std::unique_ptr<details::StructMember> impl_;
  };

  /// Define the overall size which is equivalent to the `sizeof` of the current
  /// type.
  ///
  /// This function defines the `DW_AT_byte_size` attribute
  StructType& set_size(uint64_t size);

  /// Add a member to the current struct-like
  std::unique_ptr<Member> add_member(const std::string& name, const Type& type,
                                     int64_t offset = -1);

  static bool classof(const Type* type);

  ~StructType() override = default;
};

}
}
}
#endif

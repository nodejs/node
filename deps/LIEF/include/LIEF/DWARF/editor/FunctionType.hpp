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
#ifndef LIEF_DWARF_EDITOR_FUNCTION_TYPE_H
#define LIEF_DWARF_EDITOR_FUNCTION_TYPE_H
#include "LIEF/visibility.h"
#include "LIEF/DWARF/editor/Type.hpp"

namespace LIEF {
namespace dwarf {
namespace editor {

namespace details {
class FunctionTyParameter;
}

/// This class represents a function type (`DW_TAG_subroutine_type`)
class LIEF_API FunctionType : public Type {
  public:
  using Type::Type;

  /// This class represents a function's parameter
  class LIEF_API Parameter {
    public:
    Parameter() = delete;
    Parameter(std::unique_ptr<details::FunctionTyParameter> impl);

    ~Parameter();
    private:
    std::unique_ptr<details::FunctionTyParameter> impl_;
  };

  /// Set the return type of this function
  FunctionType& set_return_type(const Type& type);

  /// Add a parameter
  std::unique_ptr<Parameter> add_parameter(const Type& type);

  static bool classof(const Type* type);

  ~FunctionType() override = default;
};

}
}
}
#endif

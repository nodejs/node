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
#ifndef LIEF_DWARF_EDITOR_COMPILATION_UNIT_H
#define LIEF_DWARF_EDITOR_COMPILATION_UNIT_H
#include <memory>
#include <string>

#include "LIEF/visibility.h"
#include "LIEF/DWARF/editor/StructType.hpp"
#include "LIEF/DWARF/editor/BaseType.hpp"
#include "LIEF/DWARF/editor/FunctionType.hpp"
#include "LIEF/DWARF/editor/PointerType.hpp"

namespace LIEF {
namespace dwarf {
namespace editor {
class Function;
class Variable;
class Type;
class EnumType;
class TypeDef;
class ArrayType;

namespace details {
class CompilationUnit;
}

/// This class represents an **editable** DWARF compilation unit
class LIEF_API CompilationUnit {
  public:
  CompilationUnit() = delete;
  CompilationUnit(std::unique_ptr<details::CompilationUnit> impl);

  /// Set the `DW_AT_producer` producer attribute.
  ///
  /// This attribute aims to inform about the program that generated this
  /// compilation unit (e.g. `LIEF Extended`)
  CompilationUnit& set_producer(const std::string& producer);

  /// Create a new function owned by this compilation unit
  std::unique_ptr<Function> create_function(const std::string& name);

  /// Create a new **global** variable owned by this compilation unit
  std::unique_ptr<Variable> create_variable(const std::string& name);

  /// Create a `DW_TAG_unspecified_type` type with the given name
  std::unique_ptr<Type> create_generic_type(const std::string& name);

  /// Create an enum type (`DW_TAG_enumeration_type`)
  std::unique_ptr<EnumType> create_enum(const std::string& name);

  /// Create a typdef with the name provided in the first parameter which aliases
  /// the type provided in the second parameter
  std::unique_ptr<TypeDef> create_typedef(const std::string& name, const Type& type);

  /// Create a struct-like type (struct, class, union) with the given name.
  std::unique_ptr<StructType> create_structure(
    const std::string& name, StructType::TYPE kind = StructType::TYPE::STRUCT);

  /// Create a primitive type with the given name and size.
  std::unique_ptr<BaseType> create_base_type(const std::string& name, size_t size,
    BaseType::ENCODING encoding = BaseType::ENCODING::NONE);

  /// Create a function type with the given name.
  std::unique_ptr<FunctionType> create_function_type(const std::string& name);

  /// Create a pointer on the provided type
  std::unique_ptr<PointerType> create_pointer_type(const Type& ty) {
    return ty.pointer_to();
  }

  /// Create a `void` type
  std::unique_ptr<Type> create_void_type();

  /// Create an array type with the given name, type and size.
  std::unique_ptr<ArrayType>
    create_array(const std::string& name, const Type& type, size_t count);

  ~CompilationUnit();

  private:
  std::unique_ptr<details::CompilationUnit> impl_;
};

}
}
}
#endif

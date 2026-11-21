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
#ifndef LIEF_PDB_TYPE_CLASS_H
#define LIEF_PDB_TYPE_CLASS_H

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/visibility.h"
#include "LIEF/PDB/Type.hpp"
#include "LIEF/PDB/types/Attribute.hpp"
#include "LIEF/PDB/types/Method.hpp"
#include "LIEF/iterators.hpp"

#include <type_traits>

namespace LIEF {
namespace pdb {
namespace types {

/// This class abstracts the following PDB types: `LF_STRUCTURE`, `LF_INTERFACE`,
/// `LF_CLASS` or `LF_UNION`.
class LIEF_API ClassLike : public Type {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<Type, Args&&...>::value>::type>
  ClassLike(Args&&... args) :
    Type(std::forward<Args>(args)...) {}

  /// Attributes iterator
  using attributes_iterator = iterator_range<Attribute::Iterator>;

  /// Methods iterator
  using methods_iterator = iterator_range<Method::Iterator>;

  /// Mangled type name
  std::string unique_name() const;

  /// Iterator over the different attributes defined in this class-like type
  attributes_iterator attributes() const LIEF_LIFETIMEBOUND;

  /// Iterator over the different methods implemented in this class-type type
  methods_iterator methods() const LIEF_LIFETIMEBOUND;

  template<class T>
  static bool classof(
      const T*,
      typename std::enable_if<std::is_base_of<ClassLike, T>::value>::type* = 0
  ) {
    return true;
  }

  ~ClassLike() override;
};


/// Interface for the `LF_STRUCTURE` PDB type
class LIEF_API Structure : public ClassLike {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<ClassLike, Args&&...>::value>::type>
  Structure(Args&&... args) :
    ClassLike(std::forward<Args>(args)...) {}

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::STRUCTURE;
  }

  ~Structure() override;
};

/// Interface for the `LF_CLASS` PDB type
class LIEF_API Class : public ClassLike {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<ClassLike, Args&&...>::value>::type>
  Class(Args&&... args) :
    ClassLike(std::forward<Args>(args)...) {}

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::CLASS;
  }

  ~Class() override;
};

/// Interface for the `LF_INTERFACE` PDB type
class LIEF_API Interface : public ClassLike {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<ClassLike, Args&&...>::value>::type>
  Interface(Args&&... args) :
    ClassLike(std::forward<Args>(args)...) {}

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::INTERFACE;
  }

  ~Interface() override;
};


}
}
}
#endif

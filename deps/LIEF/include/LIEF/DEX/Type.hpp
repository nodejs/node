/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
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
#ifndef LIEF_DEX_TYPE_H
#define LIEF_DEX_TYPE_H

#include <vector>
#include <string>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

namespace LIEF {
namespace DEX {
class Parser;
class Class;

/// Class which represents a DEX type as described in the
/// format specifications: https://source.android.com/devices/tech/dalvik/dex-format#typedescriptor
class LIEF_API Type : public Object {
  friend class Parser;

  public:
  enum class TYPES {
    UNKNOWN   = 0,
    PRIMITIVE = 1,
    CLASS     = 2,
    ARRAY     = 3,
  };

  enum class PRIMITIVES {
    VOID_T  = 0x01,
    BOOLEAN = 0x02,
    BYTE    = 0x03,
    SHORT   = 0x04,
    CHAR    = 0x05,
    INT     = 0x06,
    LONG    = 0x07,
    FLOAT   = 0x08,
    DOUBLE  = 0x09,
  };

  using array_t = std::vector<Type>;

  public:
  static std::string pretty_name(PRIMITIVES p);

  public:
  Type();
  Type(const std::string& mangled);
  Type(const Type& other);

  /// Whether it is a primitive type, a class, ...
  TYPES type() const;

  const Class& cls() const;
  const array_t& array() const;
  const PRIMITIVES& primitive() const;

  /// **IF** the current type is a TYPES::CLASS, return the
  /// associated DEX::CLASS. Otherwise the returned value is **undefined**.
  Class& cls();

  /// **IF** the current type is a TYPES::ARRAY, return the
  /// associated array. Otherwise the returned value is **undefined**.
  array_t& array();

  /// **IF** the current type is a TYPES::PRIMITIVE, return the
  /// associated PRIMITIVES. Otherwise the returned value is **undefined**.
  PRIMITIVES& primitive();

  /// Return the array dimension if the current type is
  /// an array. Otherwise it returns 0
  size_t dim() const;

  /// In the case of a TYPES::ARRAY, return the array's type
  const Type& underlying_array_type() const;
  Type& underlying_array_type();

  void accept(Visitor& visitor) const override;


  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Type& type);

  ~Type() override;

  private:
  void parse(const std::string& type);

  TYPES type_{TYPES::UNKNOWN};
  union {
    Class* cls_{nullptr};
    array_t* array_;
    PRIMITIVES* basic_;
  };
};

} // Namespace DEX
} // Namespace LIEF
#endif

/* Copyright 2021 - 2025 R. Thomas
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
#ifndef LIEF_DEX_FIELD_H
#define LIEF_DEX_FIELD_H

#include <climits>
#include <cstdint>

#include "LIEF/DEX/enums.hpp"

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

#include "LIEF/DEX/Type.hpp"

namespace LIEF {
namespace DEX {
class Parser;
class Class;

/// Class which represent a DEX Field
class LIEF_API Field : public Object {
  friend class Parser;
  public:
  using access_flags_list_t = std::vector<ACCESS_FLAGS>;

  public:
  Field();
  Field(std::string name, Class* parent = nullptr);

  Field(const Field&);
  Field& operator=(const Field&);

  /// Name of the Field
  const std::string& name() const;

  /// True if a class is associated with this field
  /// (which should be the case)
  bool has_class() const;

  /// Class associated with this Field
  const Class* cls() const;
  Class* cls();

  /// Index in the DEX Fields pool
  size_t index() const;

  /// True if this field is a static one.
  bool is_static() const;

  /// Field's prototype
  const Type* type() const;
  Type* type();

  void accept(Visitor& visitor) const override;

  /// Check if the field has the given ACCESS_FLAGS
  bool has(ACCESS_FLAGS f) const;

  /// ACCESS_FLAGS as a list
  access_flags_list_t access_flags() const;


  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Field& mtd);

  ~Field() override;

  private:
  void set_static(bool v);

  private:
  std::string name_;
  Class* parent_ = nullptr;
  Type* type_ = nullptr;
  uint32_t access_flags_ = 0;
  uint32_t original_index_ = UINT_MAX;
  bool is_static_ = false;
};

} // Namespace DEX
} // Namespace LIEF
#endif

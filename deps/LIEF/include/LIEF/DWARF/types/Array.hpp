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
#ifndef LIEF_DWARF_TYPE_ARRAY_H
#define LIEF_DWARF_TYPE_ARRAY_H

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/visibility.h"
#include "LIEF/DWARF/Type.hpp"

namespace LIEF {
namespace dwarf {
namespace types {

/// This class represents a `DW_TAG_array_type`
class LIEF_API Array : public Type {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<Type, Args&&...>::value>::type>
  Array(Args&&... args) :
    Type(std::forward<Args>(args)...) {}

  Array(const Array&) = delete;
  Array& operator=(const Array&) = delete;

  Array(Array&&) noexcept = default;
  Array& operator=(Array&&) noexcept = default;

  /// Structure that wraps information about the dimension of this array
  struct size_info_t {
    /// Type of the **index** for this array.
    ///
    /// For instance in `uint8_t[3]` the index type could be set to a `size_t`.
    std::unique_ptr<Type> type = nullptr;

    /// Name of the index (usually not relevant like `__ARRAY_SIZE_TYPE__`)
    std::string name;

    /// Size of the array. For instance in `uint8_t[3]`, it returns 3
    size_t size = 0;

    operator bool() const {
      return size != 0;
    }
  };

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::ARRAY;
  }

  /// The underlying type of this array
  const Type* underlying_type() const LIEF_LIFETIMEBOUND;

  const Type* operator->() const LIEF_LIFETIMEBOUND {
    return underlying_type();
  }

  const Type& operator*() const LIEF_LIFETIMEBOUND {
    return *underlying_type();
  }

  /// Return information about the size of this array.
  ///
  /// This size info is usually embedded in a `DW_TAG_subrange_type` DIE which
  /// is represented by the size_info_t structure.
  size_info_t size_info() const;

  ~Array() override;

  protected:
  mutable std::unique_ptr<Type> underlying_;
};

}
}
}
#endif

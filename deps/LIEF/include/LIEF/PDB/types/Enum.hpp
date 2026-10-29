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
#ifndef LIEF_PDB_TYPE_ENUM_H
#define LIEF_PDB_TYPE_ENUM_H

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/visibility.h"
#include "LIEF/PDB/Type.hpp"

namespace LIEF {
namespace pdb {
namespace types {

namespace details {
class EnumEntry;
}

/// This class represents a `LF_ENUM` PDB type
class LIEF_API Enum : public Type {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<Type, Args&&...>::value>::type>
  Enum(Args&&... args) :
    Type(std::forward<Args>(args)...) {}

  /// This class represents an enum entry which is essentially
  /// composed of a name and its value (integer).
  class LIEF_API Entry {
    public:
    Entry(std::unique_ptr<details::EnumEntry> impl LIEF_LIFETIMEBOUND);
    Entry(Entry&& other) noexcept;
    Entry& operator=(Entry&& other) noexcept;

    /// Enum entry's name
    std::string name() const;

    /// Enum entry's value (if any)
    int64_t value() const;

    ~Entry();

    private:
    std::unique_ptr<details::EnumEntry> impl_;
  };

  /// Enum's mangled name
  std::string unique_name() const;

  /// Return the different entries associated with this enum
  std::vector<Entry> entries() const LIEF_LIFETIMEBOUND;

  /// The underlying type that is used to encode this enum
  const Type* underlying_type() const LIEF_LIFETIMEBOUND;

  /// Try to find the enum matching the given value
  optional<Entry> find_entry(int64_t value) const LIEF_LIFETIMEBOUND;

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::ENUM;
  }

  ~Enum() override;

  protected:
  mutable std::unique_ptr<Type> underlying_;
};

}
}
}
#endif

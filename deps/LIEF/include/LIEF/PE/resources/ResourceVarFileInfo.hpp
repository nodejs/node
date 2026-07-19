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
#ifndef LIEF_PE_RESOURCE_VAR_FILE_INFO_H
#define LIEF_PE_RESOURCE_VAR_FILE_INFO_H
#include <ostream>
#include <cstdint>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/PE/resources/ResourceVar.hpp"

namespace LIEF {
class BinaryStream;
namespace PE {

/// Representation of the `VarFileInfo` structure
///
/// This structure represents the organization of data in a file-version resource.
/// It contains version information not dependent on a particular language and
/// code page combination.
///
/// See: https://learn.microsoft.com/en-us/windows/win32/menurc/varfileinfo
class LIEF_API ResourceVarFileInfo : public Object {
  public:
  using vars_t = std::vector<ResourceVar>;

  using it_vars = ref_iterator<vars_t&>;
  using it_const_vars = const_ref_iterator<const vars_t&>;

  static result<ResourceVarFileInfo> parse(BinaryStream& stream);

  ResourceVarFileInfo() = default;

  ResourceVarFileInfo(const ResourceVarFileInfo&) = default;
  ResourceVarFileInfo& operator=(const ResourceVarFileInfo&) = default;

  ResourceVarFileInfo(ResourceVarFileInfo&&) = default;
  ResourceVarFileInfo& operator=(ResourceVarFileInfo&&) = default;

  ~ResourceVarFileInfo() override = default;

  /// The type of data in the version resource
  /// * `1` if it contains text data
  /// * `0` if it contains binary data
  uint16_t type() const {
    return type_;
  }

  /// Signature of the structure. Must be the unicode string "VarFileInfo"
  const std::u16string& key() const {
    return key_;
  }

  /// Key as an utf8 string
  std::string key_u8() const;

  /// Iterator over the embedded variables associated to the structure
  it_vars vars() {
    return vars_;
  }

  it_const_vars vars() const {
    return vars_;
  }

  ResourceVarFileInfo& type(uint16_t type) {
    type_ = type;
    return *this;
  }

  ResourceVarFileInfo& key(std::u16string key) {
    key_ = std::move(key);
    return *this;
  }

  void add_var(ResourceVar var) {
    vars_.push_back(std::move(var));
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const ResourceVarFileInfo& entry);

  private:
  uint16_t type_ = 0;
  std::u16string key_;
  vars_t vars_;
};
}
}

#endif

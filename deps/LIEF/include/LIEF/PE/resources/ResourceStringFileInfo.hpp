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
#ifndef LIEF_PE_RESOURCE_STRING_FILE_INFO_H
#define LIEF_PE_RESOURCE_STRING_FILE_INFO_H
#include <ostream>
#include <vector>
#include <cstdint>

#include "LIEF/visibility.h"

#include "LIEF/Object.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"

#include "LIEF/PE/resources/ResourceStringTable.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {
/// Representation of the `StringFileInfo` structure
///
/// It contains version information that can be displayed for a particular
/// language and code page.
///
/// See: https://learn.microsoft.com/en-us/windows/win32/menurc/stringfileinfo
class LIEF_API ResourceStringFileInfo : public Object {
  public:
  using elements_t = std::vector<ResourceStringTable>;
  using it_const_elements = const_ref_iterator<const elements_t&>;
  using it_elements = ref_iterator<elements_t&>;

  static result<ResourceStringFileInfo> parse(BinaryStream& stream);

  ResourceStringFileInfo() = default;

  ResourceStringFileInfo(const ResourceStringFileInfo&) = default;
  ResourceStringFileInfo& operator=(const ResourceStringFileInfo&) = default;

  ResourceStringFileInfo(ResourceStringFileInfo&&) = default;
  ResourceStringFileInfo& operator=(ResourceStringFileInfo&&) = default;

  ~ResourceStringFileInfo() override = default;

  /// The type of data in the version resource
  /// * `1` if it contains text data
  /// * `0` if it contains binary data
  uint16_t type() const {
    return type_;
  }

  /// Signature of the structure. Must be the unicode string "StringFileInfo"
  const std::u16string& key() const {
    return key_;
  }

  /// Iterator over the children values
  it_const_elements children() const {
    return children_;
  }

  it_elements children() {
    return children_;
  }

  /// The key as an utf8 string
  std::string key_u8() const;

  ResourceStringFileInfo& type(uint16_t type) {
    type_ = type;
    return *this;
  }

  ResourceStringFileInfo& key(std::u16string key) {
    key_ = std::move(key);
    return *this;
  }

  void add_child(ResourceStringTable table) {
    children_.push_back(std::move(table));
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const ResourceStringFileInfo& info);

  private:
  uint16_t type_ = 0;
  std::u16string key_;
  elements_t children_;
};




}
}


#endif

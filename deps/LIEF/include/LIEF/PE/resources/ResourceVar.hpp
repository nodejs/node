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
#ifndef LIEF_PE_RESOURCE_VAR_H
#define LIEF_PE_RESOURCE_VAR_H
#include <ostream>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"

namespace LIEF {
class BinaryStream;
namespace PE {

/// This class represents an element of the ResourceVarFileInfo structure
/// It typically contains a list of language and code page identifier pairs that
/// the version of the application or DLL supports.
///
/// See: https://learn.microsoft.com/en-us/windows/win32/menurc/var-str
class LIEF_API ResourceVar {
  public:
  /// Translation values as a vector of `uint32_t`.
  using values_t = std::vector<uint32_t>;

  static result<ResourceVar> parse(BinaryStream& stream);

  ResourceVar() = default;

  ResourceVar(const ResourceVar&) = default;
  ResourceVar& operator=(const ResourceVar&) = default;

  ResourceVar(ResourceVar&&) = default;
  ResourceVar& operator=(ResourceVar&&) = default;

  ~ResourceVar() = default;

  /// The Unicode string `L"Translation"`
  const std::u16string& key() const {
    return key_;
  }

  /// The type of data in the version resource:
  /// * `1` if it contains text data
  /// * `0` if it contains binary
  uint16_t type() const {
    return type_;
  }

  /// The key as an utf8 string
  std::string key_u8() const;

  /// Return the translation values.
  ///
  /// The low-order word of each uint32_t must contain a Microsoft language
  /// identifier, and the high-order word must contain the IBM code page number.
  /// Either high-order or low-order word can be zero, indicating that the file
  /// is language or code page independent
  const values_t& values() const {
    return values_;
  }

  values_t& values() {
    return values_;
  }

  ResourceVar& key(std::u16string key) {
    key_ = std::move(key);
    return *this;
  }

  ResourceVar& type(uint16_t ty) {
    type_ = ty;
    return *this;
  }

  void add_value(uint32_t val) {
    values_.push_back(val);
  }

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const ResourceVar& entry);

  private:
  uint16_t type_ = 0;
  std::u16string key_;
  values_t values_;
};
}
}

#endif

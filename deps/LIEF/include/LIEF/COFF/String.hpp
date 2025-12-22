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
#ifndef LIEF_COFF_STRING_H
#define LIEF_COFF_STRING_H
#include <ostream>
#include <string>
#include <cstdint>

#include "LIEF/visibility.h"

namespace LIEF {
namespace COFF {

/// This class represents a string located in the COFF string table.
///
/// Some of these strings can be used for section names that are greater than 8
/// bytes. See: LIEF::PE::Section::coff_string()
///
/// Reference: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-string-table
class LIEF_API String {
  public:
  String() = default;
  String(uint32_t offset, std::string str) :
    str_(std::move(str)),
    offset_(offset)
  {}

  String(const String&) = default;
  String& operator=(const String&) = default;

  String(String&&) = default;
  String& operator=(String&&) = default;

  ~String() = default;

  /// The actual string
  const std::string& str() const {
    return str_;
  }

  /// The offset of this string the in the COFF string table.
  /// This offset includes the first 4-bytes that holds the table size
  uint32_t offset() const {
    return offset_;
  }

  String& str(std::string str) {
    str_ = std::move(str);
    return *this;
  }

  String& offset(uint32_t value) {
    offset_ = value;
    return *this;
  }

  friend LIEF_API
    std::ostream& operator<<(std::ostream& os, const String& str)
  {
    os << str.str();
    return os;
  }

  private:
  std::string str_;
  uint32_t offset_ = 0;
};
}
}
#endif

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
#ifndef LIEF_MACHO_DATA_CODE_ENTRY_H
#define LIEF_MACHO_DATA_CODE_ENTRY_H
#include <ostream>
#include <cstdint>

#include "LIEF/visibility.h"

#include "LIEF/Object.hpp"

namespace LIEF {
namespace MachO {

namespace details {
struct data_in_code_entry;
}

/// Interface over an entry in the DataInCode command
class LIEF_API DataCodeEntry : public LIEF::Object {
  public:
  enum class TYPES {
    UNKNOWN           = 0,
    DATA              = 1,
    JUMP_TABLE_8      = 2,
    JUMP_TABLE_16     = 3,
    JUMP_TABLE_32     = 4,
    ABS_JUMP_TABLE_32 = 5,
  };

  public:
  DataCodeEntry() = default;
  DataCodeEntry(uint32_t off, uint16_t length, TYPES type) :
    offset_(off),
    length_(length),
    type_(type)
  {}
  DataCodeEntry(const details::data_in_code_entry& entry);

  DataCodeEntry& operator=(const DataCodeEntry&) = default;
  DataCodeEntry(const DataCodeEntry&) = default;

  /// Offset of the data
  uint32_t offset() const {
    return offset_;
  }

  /// Length of the data
  uint16_t length() const {
    return length_;
  }

  // Type of the data
  TYPES type() const {
    return type_;
  }

  void offset(uint32_t off) {
    offset_ = off;
  }
  void length(uint16_t length) {
    length_ = length;
  }
  void type(TYPES type) {
    type_ = type;
  }

  ~DataCodeEntry() override = default;

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const DataCodeEntry& entry);

  private:
  uint32_t offset_ = 0;
  uint16_t length_ = 0;
  TYPES type_ = TYPES::UNKNOWN;
};

LIEF_API const char* to_string(DataCodeEntry::TYPES e);

}
}

#endif

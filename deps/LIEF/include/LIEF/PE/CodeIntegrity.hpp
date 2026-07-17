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
#ifndef LIEF_PE_CODE_INTEGRITY_H
#define LIEF_PE_CODE_INTEGRITY_H
#include <ostream>
#include <cstdint>

#include "LIEF/errors.hpp"
#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
class BinaryStream;

namespace PE {
class Parser;

class LIEF_API CodeIntegrity : public Object {
  public:
  static result<CodeIntegrity> parse(Parser& ctx, BinaryStream& stream);
  CodeIntegrity() = default;

  ~CodeIntegrity() override = default;

  CodeIntegrity& operator=(const CodeIntegrity&) = default;
  CodeIntegrity(const CodeIntegrity&) = default;

  CodeIntegrity& operator=(CodeIntegrity&&) = default;
  CodeIntegrity(CodeIntegrity&&) = default;

  /// Flags to indicate if CI information is available, etc.
  uint16_t flags() const {
    return flags_;
  }

  /// 0xFFFF means not available
  uint16_t catalog() const {
    return catalog_;
  }

  uint32_t catalog_offset() const {
    return catalog_offset_;
  }

  /// Additional bitmask to be defined later
  uint32_t reserved() const {
    return reserved_;
  }

  CodeIntegrity& flags(uint16_t flags) {
    flags_ = flags;
    return *this;
  }

  CodeIntegrity& catalog(uint16_t catalog) {
    catalog_ = catalog;
    return *this;
  }

  CodeIntegrity& catalog_offset(uint32_t catalog_offset) {
    catalog_offset_ = catalog_offset;
    return *this;
  }

  CodeIntegrity& reserved(uint32_t reserved) {
    reserved_ = reserved;
    return *this;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const CodeIntegrity& entry);

  private:
  uint16_t flags_ = 0;
  uint16_t catalog_ = 0;

  uint32_t catalog_offset_ = 0;
  uint32_t reserved_ = 0;
};
}
}

#endif

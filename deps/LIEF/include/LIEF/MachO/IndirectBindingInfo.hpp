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
#ifndef LIEF_MACHO_INDIRECT_BINDING_INFO_H
#define LIEF_MACHO_INDIRECT_BINDING_INFO_H
#include <ostream>
#include <cstdint>

#include "LIEF/visibility.h"
#include "LIEF/MachO/BindingInfo.hpp"

namespace LIEF {
namespace MachO {

/// This class represents a binding operation infered from the indirect symbol
/// table.
class LIEF_API IndirectBindingInfo : public BindingInfo {
  friend class BinaryParser;
  public:

  IndirectBindingInfo(SegmentCommand& segment, Symbol& symbol, int32_t ordinal,
                      DylibCommand* dylib, uint64_t address)
  {
    segment_ = &segment;
    symbol_ = &symbol;
    library_ordinal_ = ordinal;
    library_ = dylib;
    address_ = address;
  }

  IndirectBindingInfo& operator=(const IndirectBindingInfo& other) = default;
  IndirectBindingInfo(const IndirectBindingInfo& other) = default;

  IndirectBindingInfo& operator=(IndirectBindingInfo&&) noexcept = default;
  IndirectBindingInfo(IndirectBindingInfo&&) noexcept = default;

  BindingInfo::TYPES type() const override {
    return BindingInfo::TYPES::INDIRECT_SYMBOL;
  }

  static bool classof(const BindingInfo* info) {
    return info->type() == BindingInfo::TYPES::INDIRECT_SYMBOL;
  }

  ~IndirectBindingInfo() override = default;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const IndirectBindingInfo& info) {
    os << static_cast<const BindingInfo&>(info);
    return os;
  }
};

}
}
#endif

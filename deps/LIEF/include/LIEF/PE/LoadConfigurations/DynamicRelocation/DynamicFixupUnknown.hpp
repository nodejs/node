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
#ifndef LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_UNKNOWN_H
#define LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_UNKNOWN_H
#include "LIEF/span.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixup.hpp"

#include <vector>

namespace LIEF {
namespace PE {

/// This class represents an special dynamic relocation where the format of the
/// fixups is not supported by LIEF.
class LIEF_API DynamicFixupUnknown : public DynamicFixup {
  public:
  DynamicFixupUnknown(std::vector<uint8_t> payload) :
    DynamicFixup(KIND::UNKNOWN),
    payload_(std::move(payload))
  {}

  DynamicFixupUnknown(const DynamicFixupUnknown&) = default;
  DynamicFixupUnknown& operator=(const DynamicFixupUnknown&) = default;

  DynamicFixupUnknown(DynamicFixupUnknown&&) = default;
  DynamicFixupUnknown& operator=(DynamicFixupUnknown&&) = default;

  std::unique_ptr<DynamicFixup> clone() const override {
    return std::unique_ptr<DynamicFixupUnknown>(new DynamicFixupUnknown(*this));
  }

  static bool classof(const DynamicFixup* fixup) {
    return fixup->kind() == KIND::UNKNOWN;
  }

  std::string to_string() const override {
    return "DynamicFixupUnknown(" + std::to_string(payload_.size()) + "bytes)";
  }

  /// Raw fixups
  span<const uint8_t> payload() const {
    return payload_;
  }

  span<uint8_t> payload() {
    return payload_;
  }

  ~DynamicFixupUnknown() override = default;

  private:
  std::vector<uint8_t> payload_;
};


}
}

#endif

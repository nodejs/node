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
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicRelocationBase.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixup.hpp"
#include "frozen.hpp"
namespace LIEF::PE {

DynamicRelocation::DynamicRelocation(uint32_t version) :
  version_(version)
{}

DynamicRelocation::DynamicRelocation(const DynamicRelocation& other) :
  version_(other.version_),
  symbol_(other.symbol_)
{
  if (other.fixups_ != nullptr) {
    fixups_ = other.fixups_->clone();
  }
}

DynamicRelocation& DynamicRelocation::operator=(const DynamicRelocation& other) {
  if (this == &other) {
    return *this;
  }
  version_ = other.version_;
  symbol_ = other.symbol_;

  if (other.fixups_ != nullptr) {
    fixups_ = other.fixups_->clone();
  }
  return *this;
}

DynamicRelocation::DynamicRelocation(DynamicRelocation&&) = default;
DynamicRelocation& DynamicRelocation::operator=(DynamicRelocation&&) = default;
DynamicRelocation::~DynamicRelocation() = default;


DynamicRelocation& DynamicRelocation::fixups(std::unique_ptr<DynamicFixup> F) {
  fixups_ = std::move(F);
  return *this;
}

const char* to_string(DynamicRelocation::IMAGE_DYNAMIC_RELOCATION e) {
  using IMAGE_DYNAMIC_RELOCATION = DynamicRelocation::IMAGE_DYNAMIC_RELOCATION;

  #define ENTRY(X) std::pair(IMAGE_DYNAMIC_RELOCATION::X, #X)
  STRING_MAP enums2str {
    ENTRY(RELOCATION_GUARD_RF_PROLOGUE),
    ENTRY(RELOCATION_GUARD_RF_EPILOGUE),
    ENTRY(RELOCATION_GUARD_IMPORT_CONTROL_TRANSFER),
    ENTRY(RELOCATION_GUARD_INDIR_CONTROL_TRANSFER),
    ENTRY(RELOCATION_GUARD_SWITCHTABLE_BRANCH),
    ENTRY(RELOCATION_ARM64X),
    ENTRY(RELOCATION_FUNCTION_OVERRIDE),
    ENTRY(RELOCATION_ARM64_KERNEL_IMPORT_CALL_TRANSFER),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}

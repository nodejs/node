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
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixup.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixupUnknown.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/FunctionOverride.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixupARM64X.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixupControlTransfer.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixupARM64Kernel.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixupGeneric.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicRelocationBase.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "logging.hpp"

namespace LIEF::PE {

using IMAGE_DYNAMIC_RELOCATION = DynamicRelocation::IMAGE_DYNAMIC_RELOCATION;

ok_error_t DynamicFixup::parse(Parser& ctx, SpanStream& stream, DynamicRelocation& R) {
  auto default_ret = std::make_unique<DynamicFixupUnknown>(stream.content());
  if (R.symbol() == IMAGE_DYNAMIC_RELOCATION::RELOCATION_FUNCTION_OVERRIDE) {
    LIEF_DEBUG("IMAGE_DYNAMIC_RELOCATION_FUNCTION_OVERRIDE");
    auto func = FunctionOverride::parse(ctx, stream);
    if (func != nullptr) {
      R.fixups(std::move(func));
      return ok();
    }
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    R.fixups(std::move(default_ret));
    return make_error_code(lief_errors::parsing_error);
  }

  if (R.symbol() == IMAGE_DYNAMIC_RELOCATION::RELOCATION_ARM64X) {
    LIEF_DEBUG("IMAGE_DYNAMIC_RELOCATION_ARM64X");
    auto arm64x_fixups = DynamicFixupARM64X::parse(ctx, stream);
    if (arm64x_fixups != nullptr) {
      R.fixups(std::move(arm64x_fixups));
      return ok();
    }
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    R.fixups(std::move(default_ret));
    return make_error_code(lief_errors::parsing_error);
  }

  if (R.symbol() == IMAGE_DYNAMIC_RELOCATION::RELOCATION_ARM64_KERNEL_IMPORT_CALL_TRANSFER) {
    // Note(romain): According to link.exe there is a special processing for
    // this symbol (c.f. 0x140164140 in link.exe from 14.42.34433)
    //
    // Sample: win11-arm64/sources/winsetupmon.sys
    auto fixups = DynamicFixupARM64Kernel::parse(ctx, stream);
    if (fixups != nullptr) {
      R.fixups(std::move(fixups));
      return ok();
    }
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    R.fixups(std::move(default_ret));
    return make_error_code(lief_errors::parsing_error);
  }

  if (R.symbol() == IMAGE_DYNAMIC_RELOCATION::RELOCATION_GUARD_IMPORT_CONTROL_TRANSFER) {
    LIEF_DEBUG("IMAGE_DYNAMIC_RELOCATION_GUARD_IMPORT_CONTROL_TRANSFER");
    auto fixups = DynamicFixupControlTransfer::parse(ctx, stream);
    if (fixups != nullptr) {
      R.fixups(std::move(fixups));
      return ok();
    }
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    R.fixups(std::move(default_ret));
    return make_error_code(lief_errors::parsing_error);
  }

  auto generic = DynamicFixupGeneric::parse(ctx, stream);
  if (generic != nullptr) {
    R.fixups(std::move(generic));
    return ok();
  }

  LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
  R.fixups(std::move(default_ret));
  return make_error_code(lief_errors::parsing_error);
}
}

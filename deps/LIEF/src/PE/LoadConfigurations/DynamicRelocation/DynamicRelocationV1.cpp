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
#include "LIEF/PE/Relocation.hpp"
#include "LIEF/PE/RelocationEntry.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicRelocationV1.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/FunctionOverride.hpp"

#include "PE/Structures.hpp"

#include "logging.hpp"
#include "internal_utils.hpp"

namespace LIEF::PE {

template<class PE_T>
std::unique_ptr<DynamicRelocationV1>
  DynamicRelocationV1::parse(Parser& ctx, BinaryStream& strm)
{
  using ptr_t = typename PE_T::uint;
  // typedef struct _IMAGE_DYNAMIC_RELOCATION32 {
  //     DWORD      Symbol;
  //     DWORD      BaseRelocSize;
  // //  IMAGE_BASE_RELOCATION BaseRelocations[0];
  // } IMAGE_DYNAMIC_RELOCATION32, *PIMAGE_DYNAMIC_RELOCATION32;
  //
  // typedef struct _IMAGE_DYNAMIC_RELOCATION64 {
  //     ULONGLONG  Symbol;
  //     DWORD      BaseRelocSize;
  // //  IMAGE_BASE_RELOCATION BaseRelocations[0];
  // } IMAGE_DYNAMIC_RELOCATION64, *PIMAGE_DYNAMIC_RELOCATION64;
  auto Symbol = strm.read<ptr_t>();
  if (!Symbol) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }
  auto BaseRelocSize = strm.read<uint32_t>();
  if (!BaseRelocSize) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto dyn_reloc = std::make_unique<DynamicRelocationV1>();
  dyn_reloc->symbol(*Symbol);

  LIEF_DEBUG("  Symbol={}, BaseRelocSize={}", *Symbol, *BaseRelocSize);

  std::vector<uint8_t> buffer;
  if (!strm.read_data(buffer, *BaseRelocSize)) {
    LIEF_DEBUG("Error: {}: {}", __FUNCTION__, __LINE__);
    return dyn_reloc;
  }

  SpanStream payload_strm(buffer);
  if (!DynamicFixup::parse(ctx, payload_strm, *dyn_reloc)) {
    LIEF_WARN("Dynamic relocation failed to parse fixup (Symbol=0x{:016x})",
              dyn_reloc->symbol());
  }
  return dyn_reloc;
}

std::string DynamicRelocationV1::to_string() const {
  using namespace fmt;
  std::ostringstream oss;
  oss << "Dynamic Value Relocation Table (version: 1)\n";
  if (symbol() < IMAGE_DYNAMIC_RELOCATION::_RELOC_LAST_ENTRY) {
    oss << format("Symbol VA: 0x{:016x} ({})\n", symbol(),
                  PE::to_string((IMAGE_DYNAMIC_RELOCATION)symbol()));
  } else {
    oss << format("Symbol VA: 0x{:016x}\n", symbol());
  }

  if (const DynamicFixup* F = fixups()) {
    oss << F->to_string();
  }
  return oss.str();
}

template
std::unique_ptr<DynamicRelocationV1>
  DynamicRelocationV1::parse<details::PE32>(Parser& ctx, BinaryStream&);

template
std::unique_ptr<DynamicRelocationV1>
  DynamicRelocationV1::parse<details::PE64>(Parser& ctx, BinaryStream&);
}

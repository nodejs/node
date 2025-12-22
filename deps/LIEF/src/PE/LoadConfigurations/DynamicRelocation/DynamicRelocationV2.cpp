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
#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicRelocationV2.hpp"

#include "logging.hpp"

namespace LIEF::PE {

template<class PE_T>
std::unique_ptr<DynamicRelocationV2>
  DynamicRelocationV2::parse(Parser& /*ctx*/, BinaryStream& stream)
{
  using ptr_t = typename PE_T::uint;
  auto v2 = std::make_unique<DynamicRelocationV2>();
  // typedef struct _IMAGE_DYNAMIC_RELOCATION32_V2 {
  //     DWORD      HeaderSize;
  //     DWORD      FixupInfoSize;
  //     DWORD      Symbol;
  //     DWORD      SymbolGroup;
  //     DWORD      Flags;
  //     // ...     variable length header fields
  //     // BYTE    FixupInfo[FixupInfoSize]
  // } IMAGE_DYNAMIC_RELOCATION32_V2, *PIMAGE_DYNAMIC_RELOCATION32_V2;
  //
  // typedef struct _IMAGE_DYNAMIC_RELOCATION64_V2 {
  //     DWORD      HeaderSize;
  //     DWORD      FixupInfoSize;
  //     ULONGLONG  Symbol;
  //     DWORD      SymbolGroup;
  //     DWORD      Flags;
  //     // ...     variable length header fields
  //     // BYTE    FixupInfo[FixupInfoSize]
  // } IMAGE_DYNAMIC_RELOCATION64_V2, *PIMAGE_DYNAMIC_RELOCATION64_V2;
  auto HeaderSize = stream.read<uint32_t>();
  if (!HeaderSize) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return v2;
  }

  auto FixupInfoSize = stream.read<uint32_t>();
  if (!FixupInfoSize) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return v2;
  }

  auto Symbol = stream.read<ptr_t>();
  if (!Symbol) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return v2;
  }

  auto SymbolGroup = stream.read<uint32_t>();
  if (!SymbolGroup) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return v2;
  }

  auto Flags = stream.read<uint32_t>();
  if (!Flags) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return v2;
  }

  {
    // Note(romain): As of now (2024-12) I couldn't find any PE that is using
    // the v2 format. Hence this part is incomplete/not working.
    stream
      .increment_pos(*HeaderSize)
      .increment_pos(*FixupInfoSize);
  }
}

std::string DynamicRelocationV2::to_string() const {
  return "DynamicRelocationV2";
}
}

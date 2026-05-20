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
#include "LIEF/PE/Parser.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixupARM64Kernel.hpp"

#include "logging.hpp"

namespace LIEF::PE {

namespace details {
// From SDK: 10.0.26100.0
// typedef struct _IMAGE_IMPORT_CONTROL_TRANSFER_ARM64_RELOCATION {
//  +00,00 | ULONG PageRelativeOffset : 10;  // Offset to the call instruction shifted right by 2 (4-byte aligned instruction)
//  +10,0A | ULONG IndirectCall       :  1;  // 0 if target instruction is a BR, 1 if BLR.
//  +11,0B | ULONG RegisterIndex      :  5;  // Register index used for the indirect call/jump.
//  +16,10 | ULONG ImportType         :  1;  // 0 if this refers to a static import, 1 for delayload import
//  +17,11 | ULONG IATIndex           : 15;  // IAT index of the corresponding import.
//                                     // 0x7FFF is a special value indicating no index.
// } IMAGE_IMPORT_CONTROL_TRANSFER_ARM64_RELOCATION;
// typedef IMAGE_IMPORT_CONTROL_TRANSFER_ARM64_RELOCATION UNALIGNED * PIMAGE_IMPORT_CONTROL_TRANSFER_ARM64_RELOCATION;
struct arm64e_kernel_reloc_t {
  uint32_t page_relative_offset : 10;
  uint32_t indirect_call        :  1;
  uint32_t register_index       :  5;
  uint32_t import_type          :  1;
  uint32_t iat_index            : 15;
};

static_assert(sizeof(arm64e_kernel_reloc_t) == sizeof(uint32_t));
}

std::string DynamicFixupARM64Kernel::reloc_entry_t::to_string() const {
  using namespace fmt;
  return format("RVA: 0x{:08x} Instr: {:8} delayload: {} IAT index: {:04d}",
    rva, format("{} x{}", indirect_call ? "blr" : "br", register_index),
    import_type == IMPORT_TYPE::DELAYED, iat_index);
}

std::unique_ptr<DynamicFixupARM64Kernel>
  DynamicFixupARM64Kernel::parse(Parser& /*ctx*/, SpanStream& strm)
{
  auto fixup = std::make_unique<DynamicFixupARM64Kernel>();
  while (strm) {
    auto PageRVA = strm.read<uint32_t>();
    if (!PageRVA) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return fixup;
    }

    auto BlockSize = strm.read<uint32_t>();
    if (!BlockSize) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return fixup;
    }

    if (*BlockSize == 0) {
      continue;
    }

    auto block_strm = strm.slice(strm.pos(), (*BlockSize - 8));
    if (!block_strm) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return fixup;
    }

    strm.increment_pos(*BlockSize - 8);

    while (*block_strm) {
      auto value = block_strm->read<details::arm64e_kernel_reloc_t>();
      if (!value) {
        return fixup;
      }
      const uint32_t rva = *PageRVA + (value->page_relative_offset << 2);
      fixup->entries_.push_back({
        /* rva */rva,
        /* indirect_call */(bool)value->indirect_call,
        /* register_index */(uint8_t)value->register_index,
        /* import_type */(IMPORT_TYPE)value->import_type,
        /* iat_index */(uint16_t)value->iat_index,
      });

      LIEF_DEBUG("PAGE=0x{:010x} RVA: 0x{:06x} IndirectCall: {}, RegIndex: {}, ImportType: {}, "
                 "IATIndex: {}", *PageRVA, *PageRVA + (value->page_relative_offset << 2), value->indirect_call,
                 value->register_index, value->import_type, value->iat_index);
    }
  }
  return fixup;
}

std::string DynamicFixupARM64Kernel::to_string() const {
  using namespace fmt;
  std::ostringstream oss;
  oss << "Fixup RVAs (Control Transfer ARM64 Relocation)\n";
  for (size_t i = 0; i < entries_.size(); ++i) {
    const reloc_entry_t& entry = entries_[i];
    oss << format("  [{:04d}] {}\n", i, entry.to_string());
  }
  return oss.str();
}

}

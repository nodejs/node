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
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixupControlTransfer.hpp"

#include "logging.hpp"

namespace LIEF::PE {

namespace details {
// typedef struct _IMAGE_IMPORT_CONTROL_TRANSFER_DYNAMIC_RELOCATION {
//     DWORD       PageRelativeOffset : 12;
//     DWORD       IndirectCall       : 1;
//     DWORD       IATIndex           : 19;
// } IMAGE_IMPORT_CONTROL_TRANSFER_DYNAMIC_RELOCATION;
// typedef IMAGE_IMPORT_CONTROL_TRANSFER_DYNAMIC_RELOCATION UNALIGNED * PIMAGE_IMPORT_CONTROL_TRANSFER_DYNAMIC_RELOCATION;
struct control_transfer_reloc_t {
  uint32_t page_relative_offset : 12;
  uint32_t indirect_call        :  1;
  uint32_t iat_index            : 19;
};

static_assert(sizeof(control_transfer_reloc_t) == sizeof(uint32_t));
}

std::string DynamicFixupControlTransfer::reloc_entry_t::to_string() const {
  using namespace fmt;
  return format("RVA: 0x{:08x} Instr: {:6} IAT index: {:04d}",
                rva, is_call ? "call" : "branch", iat_index);
}

std::unique_ptr<DynamicFixupControlTransfer>
  DynamicFixupControlTransfer::parse(Parser& /*ctx*/, SpanStream& strm)
{
  auto fixup = std::make_unique<DynamicFixupControlTransfer>();
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
      auto value = block_strm->read<details::control_transfer_reloc_t>();
      if (!value) {
        return fixup;
      }
      const uint32_t rva = *PageRVA + value->page_relative_offset;
      fixup->entries_.push_back({
        /* rva */rva,
        /* indirect_call */(bool)value->indirect_call,
        /* iat_index */(uint16_t)value->iat_index,
      });
    }
  }
  return fixup;
}

std::string DynamicFixupControlTransfer::to_string() const {
  using namespace fmt;
  std::ostringstream oss;
  oss << "Fixup RVAs (Guard Import Control Transfer)\n";
  for (size_t i = 0; i < entries_.size(); ++i) {
    const reloc_entry_t& entry = entries_[i];
    oss << format("  [{:04d}] {}\n", i, entry.to_string());
  }
  return oss.str();
}

}

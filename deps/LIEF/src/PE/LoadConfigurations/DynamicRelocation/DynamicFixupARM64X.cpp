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
#include "LIEF/utils.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixupARM64X.hpp"

#include "logging.hpp"
#include "internal_utils.hpp"

namespace LIEF::PE {

namespace details {

// typedef struct _IMAGE_DVRT_ARM64X_FIXUP_RECORD {
//     USHORT Offset : 12;
//     USHORT Type   :  2;
//     USHORT Size   :  2;
// } IMAGE_DVRT_ARM64X_FIXUP_RECORD, *PIMAGE_DVRT_ARM64X_FIXUP_RECORD;
struct fixup_record {
  uint16_t offset : 12;
  uint16_t type   : 2;
  uint16_t size   : 2;
};

static_assert(sizeof(fixup_record) == sizeof(uint16_t));

struct delta_record {
  uint16_t offset : 12;
  uint16_t type   :  2;
  uint16_t sign   :  1;
  uint16_t scale  :  1;
};

}

std::unique_ptr<DynamicFixupARM64X>
  DynamicFixupARM64X::parse(Parser& ctx, SpanStream& strm)
{
  auto arm64x = std::make_unique<DynamicFixupARM64X>();
  while (strm) {
    auto PageRVA = strm.read<uint32_t>();
    if (!PageRVA) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return arm64x;
    }

    auto BlockSize = strm.read<uint32_t>();
    if (!BlockSize) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return arm64x;
    }

    if (*BlockSize == 0) {
      continue;
    }

    auto block_strm = strm.slice(strm.pos(), (*BlockSize - 8));
    if (!block_strm) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return arm64x;
    }

    strm.increment_pos(*BlockSize - 8);

    while (*block_strm) {
      auto record = block_strm->read<details::fixup_record>();
      if (!record) {
        break;
      }

      const uint32_t RVA = *PageRVA + record->offset;

      switch ((FIXUP_TYPE)record->type) {
        case FIXUP_TYPE::ZEROFILL:
          {
            arm64x->entries_.push_back({
              /*rva=*/RVA, /*type=*/FIXUP_TYPE::ZEROFILL,
              /*size=*/size_t(1) << record->size, /*bytes=*/{},
              /*value=*/0,
            });
            break;
          }

        case FIXUP_TYPE::VALUE:
          {
            std::vector<uint8_t> data;
            const size_t size = 1 << record->size;
            if (!block_strm->read_data(data, size)) {
              LIEF_WARN("Can't read data associated with 'IMAGE_DVRT_ARM64X_FIXUP_TYPE_VALUE");
              break;
            }
            arm64x->entries_.push_back({
              /*rva=*/RVA, /*type=*/FIXUP_TYPE::VALUE,
              /*size=*/size, /*bytes=*/std::move(data),
              /*value=*/0,
            });

            const auto& entry = arm64x->entries_.back();
            if (ctx.config().parse_arm64x_binary) {
              ctx.record_relocation(entry.rva, entry.bytes);
            }
            break;
          }

        case FIXUP_TYPE::DELTA:
          {
            auto value = block_strm->read<uint16_t>();
            if (!value) {
              LIEF_WARN("Can't read value for IMAGE_DVRT_ARM64X_FIXUP_TYPE_DELTA");
              break;
            }

            auto& opt = *reinterpret_cast<const details::delta_record*>(&*record);
            const uint8_t size = (opt.scale ? 8llu : 4llu);
            int64_t delta = (uint64_t)(*value) * size;
            arm64x->entries_.push_back({
              /*rva=*/RVA, /*type=*/FIXUP_TYPE::DELTA,
              /*size=*/size, /*bytes=*/{},
              /*value=*/opt.sign ? -delta : delta,
            });
            const auto& entry = arm64x->entries_.back();
            if (ctx.config().parse_arm64x_binary) {
              ctx.record_delta_relocation(entry.rva, entry.value, size);
            }
            break;
          }

        default:
          {
            LIEF_WARN("Unsupported fixup type: {} (0x{:06x})",
                      (int)record->type, (int)record->type);
            break;
          }
      }
    }
  }

  return arm64x;
}

std::string DynamicFixupARM64X::reloc_entry_t::to_string() const {
  using namespace fmt;
  switch (type) {
    case FIXUP_TYPE::ZEROFILL:
      return format("RVA 0x{:08x}, {} bytes, zero fill", rva, size);

    case FIXUP_TYPE::VALUE:
      return format("RVA 0x{:08x}, {} bytes, target value {}",
                    rva, bytes.size(), hex_dump(bytes));

    case FIXUP_TYPE::DELTA:
      return format("RVA 0x{:08x}, {} bytes, delta 0x{:08x}",
                    rva, size, value);
  }
  return "<unknown>";
}

std::string DynamicFixupARM64X::to_string() const {
  using namespace fmt;
  std::ostringstream oss;
  oss << "Fixup RVAs (ARM64X)\n";
  for (size_t i = 0; i < entries_.size(); ++i) {
    const reloc_entry_t& entry = entries_[i];
    oss << format("  [{:04d}] {}\n", i, entry.to_string());
  }
  return oss.str();
}

}

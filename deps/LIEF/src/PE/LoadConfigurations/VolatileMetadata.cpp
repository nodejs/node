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
#include <sstream>
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/LoadConfigurations/VolatileMetadata.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "PE/Structures.hpp"

#include "logging.hpp"
#include "internal_utils.hpp"

namespace LIEF::PE {

std::string VolatileMetadata::to_string() const {
  using namespace fmt;
  static constexpr auto WIDTH = 30;
  std::ostringstream os;
  os << format("{:{}}: 0x{:08x}\n", "Size", WIDTH, size())
     << format("{:{}}: {} (0x{:04x})\n", "Min version", WIDTH,
               min_version() & 0x7FFF, min_version())
     << format("{:{}}: {} (0x{:04x})\n", "Max version", WIDTH,
               max_version() & 0x7FFF, max_version())
     << format("{:{}}: 0x{:08x}\n", "Volatile access table RVA", WIDTH,
               access_table_rva())
     << format("{:{}}: {}\n", "Volatile access table size", WIDTH,
               access_table_size())
     << format("{:{}}: 0x{:08x}\n", "Volatile range info table RVA", WIDTH,
               info_range_rva())
     << format("{:{}}: {}\n", "Volatile range info table size", WIDTH,
               info_ranges_size());

  if (const auto& rva_table = access_table(); !rva_table.empty()) {
    os << "\nVolatile Access RVA Table: {\n";
    for (uint32_t rva : rva_table) {
      os << format("  0x{:08x}\n", rva);
    }
    os << "}\n";
  }

  if (auto ranges = info_ranges(); !ranges.empty()) {
    os << "\nVolatile Info Range Table: {\n";
    for (const range_t& range : ranges) {
      os << format("  [0x{:08x}, 0x{:08x}] ({} bytes)\n", range.start,
                   range.end(), range.size);
    }
    os << "}\n";
  }

  return os.str();
}

std::unique_ptr<VolatileMetadata>
    VolatileMetadata::parse(Parser& ctx, BinaryStream& stream)
{
  auto size = stream.read<uint32_t>();
  if (!size) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto min_version = stream.read<uint16_t>();
  if (!min_version) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto max_version = stream.read<uint16_t>();
  if (!min_version) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto access_table_rva = stream.read<uint32_t>();
  if (!access_table_rva) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto access_table_sz = stream.read<uint32_t>();
  if (!access_table_sz) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto info_range_rva = stream.read<uint32_t>();
  if (!info_range_rva) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto info_range_sz = stream.read<uint32_t>();
  if (!info_range_sz) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto meta = std::make_unique<VolatileMetadata>();
  (*meta)
    .size(*size)
    .min_version(*min_version)
    .max_version(*max_version)
    .access_table_rva(*access_table_rva)
    .info_range_rva(*info_range_rva)
  ;

  if (*access_table_rva > 0 && *access_table_sz > 0) {
    uint32_t offset = ctx.bin().rva_to_offset(*access_table_rva);
    size_t count = *access_table_sz / sizeof(uint32_t);
    ScopedStream access_tbl_strm(ctx.stream(), offset);
    for (size_t i = 0; i < count; ++i) {
      auto rva = access_tbl_strm->read<uint32_t>();
      if (!rva) {
        break;
      }
      meta->access_table_.push_back(*rva);
    }
  }

  if (*info_range_rva > 0 && *info_range_sz > 0) {
    uint32_t offset = ctx.bin().rva_to_offset(*info_range_rva);
    size_t count = *info_range_sz / (2 * sizeof(uint32_t));
    ScopedStream range_strm(ctx.stream(), offset);
    for (size_t i = 0; i < count; ++i) {
      auto start = range_strm->read<uint32_t>();
      if (!start) {
        break;
      }
      auto size = range_strm->read<uint32_t>();
      if (!size) {
        break;
      }
      meta->info_ranges_.push_back({*start, *size});
    }
  }

  return meta;
}

}

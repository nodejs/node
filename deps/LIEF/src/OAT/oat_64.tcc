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

#include <type_traits>

#include "logging.hpp"

#include "LIEF/utils.hpp"

#include "LIEF/DEX.hpp"
#include "LIEF/OAT/Parser.hpp"
#include "LIEF/OAT/DexFile.hpp"
#include "LIEF/OAT/Binary.hpp"

#include "DEX/Structures.hpp"
#include "OAT/Structures.hpp"

namespace LIEF {
namespace OAT {

template<>
void Parser::parse_dex_files<details::OAT64_t>() {
  using oat_header = typename details::OAT64_t::oat_header;
  using dex35_header_t  = DEX::details::DEX35::dex_header;

  auto& oat = oat_binary();

  size_t nb_dex_files = oat.header_.nb_dex_files();

  uint64_t dexfiles_offset = sizeof(oat_header) + oat.header_.key_value_size();

  LIEF_DEBUG("OAT DEX file located at offset: 0x{:x}", dexfiles_offset);

  stream_->setpos(dexfiles_offset);
  for (size_t i = 0; i < nb_dex_files; ++i ) {

    LIEF_DEBUG("Dealing with OAT DEX file #{:d}", i);
    auto dex_file = std::make_unique<DexFile>();

    auto location_size = stream_->read<uint32_t>();
    if (!location_size) {
      return;
    }
    const char* loc_cstr = stream_->read_array<char>(*location_size);

    std::string location;

    if (loc_cstr != nullptr) {
      location = {loc_cstr, *location_size};
    }

    dex_file->location(location);

    if (auto res = stream_->read<uint32_t>()) {
      dex_file->checksum(*res);
    }

    auto dex_struct_offset = stream_->read<uint32_t>();
    if (!dex_struct_offset) {
      break;
    }

    dex_file->dex_offset(*dex_struct_offset);
    auto res_dex_hdr = stream_->peek<dex35_header_t>(*dex_struct_offset);
    if (!res_dex_hdr) {
      break;
    }

    const auto dex_hdr = *res_dex_hdr;

    dex_file->classes_offsets_.reserve(dex_hdr.class_defs_size);
    for (size_t cls_idx = 0; cls_idx < dex_hdr.class_defs_size; ++cls_idx) {
      if (auto res = stream_->read<uint32_t>()) {
        dex_file->classes_offsets_.push_back(*res);
      } else {
        break;
      }
    }
    oat.oat_dex_files_.push_back(std::move(dex_file));
  }


  for (size_t i = 0; i < nb_dex_files; ++i) {
    if (i >= oat.oat_dex_files_.size()) {
      LIEF_WARN("DEX file #{} is out of bound", i);
      break;
    }
    uint64_t offset = oat.oat_dex_files_[i]->dex_offset();

    LIEF_DEBUG("Dealing with OAT DEX file #{:d} at offset 0x{:x}", i, offset);

    const auto res_hdr = stream_->peek<dex35_header_t>(offset);
    if (!res_hdr) {
      break;
    }
    const auto hdr = *res_hdr;

    std::vector<uint8_t> data_v;
    const auto* data = stream_->peek_array<uint8_t>(offset, hdr.file_size);

    if (data != nullptr) {
      data_v = {data, data + hdr.file_size};
    }

    std::string name = "classes";
    if (i > 0) {
      name += std::to_string(i + 1);
    }
    name += ".dex";

    std::unique_ptr<DexFile>& oat_dex_file = oat.oat_dex_files_[i];
    if (DEX::is_dex(data_v)) {
      std::unique_ptr<DEX::File> dexfile = DEX::Parser::parse(std::move(data_v), name);
      dexfile->location(oat_dex_file->location());
      oat_dex_file->dex_file_ = dexfile.get();
      oat.dex_files_.push_back(std::move(dexfile));
    } else {
      LIEF_WARN("{} ({}) at  0x{:x} is not a DEX file", name, oat_dex_file->location(), stream_->pos());
    }
  }
}




} // Namespace OAT
} // Namespace LIEF


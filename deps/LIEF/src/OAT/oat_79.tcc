/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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

#include <memory>
#include <type_traits>

#include "logging.hpp"

#include "LIEF/utils.hpp"

#include "DEX/Structures.hpp"
#include "OAT/Structures.hpp"

#include "LIEF/OAT/EnumToString.hpp"
#include "LIEF/OAT/Parser.hpp"
#include "LIEF/OAT/DexFile.hpp"
#include "LIEF/OAT/Binary.hpp"


namespace LIEF::OAT {

template<>
void Parser::parse_dex_files<details::OAT79_t>() {
  using oat_header = typename details::OAT79_t::oat_header;
  using dex35_header_t = DEX::details::DEX35::dex_header;

  auto& oat = oat_binary();
  size_t nb_dex_files = oat.header_.nb_dex_files();

  uint64_t dexfiles_offset = sizeof(oat_header) + oat.header_.key_value_size();

  LIEF_DEBUG("OAT DEX files offset: {:#x}", dexfiles_offset);

  std::vector<uint32_t> classes_offsets_offset;
  if (dexfiles_offset < stream_->size()) {
    const uint64_t max_records =
        (stream_->size() - dexfiles_offset) / sizeof(uint32_t);
    auto reserve = std::min<size_t>(nb_dex_files, max_records);
    classes_offsets_offset.reserve(reserve);
  }

  stream_->setpos(dexfiles_offset);

  for (size_t i = 0; i < nb_dex_files; ++i) {

    LIEF_DEBUG("Processing OAT DEX file #{:d}", i);
    std::unique_ptr<DexFile> dex_file = std::make_unique<DexFile>();
    auto location_size = stream_->read<uint32_t>();
    if (!location_size) {
      break;
    }

    const char* loc_cstr = stream_->read_array<char>(*location_size);

    std::string location;

    if (loc_cstr != nullptr) {
      location = {loc_cstr, *location_size};
    }

    dex_file->location(location);

    if (auto checksum = stream_->read<uint32_t>()) {
      dex_file->checksum(*checksum);
    } else {
      break;
    }

    if (auto dex_struct_offset = stream_->read<uint32_t>()) {
      dex_file->dex_offset(*dex_struct_offset);
    } else {
      break;
    }

    if (auto class_offsets = stream_->read<uint32_t>()) {
      classes_offsets_offset.push_back(*class_offsets);
    } else {
      break;
    }

    if (auto type_lookup_offset = stream_->read<uint32_t>()) {
      dex_file->lookup_table_offset(*type_lookup_offset);
    } else {
      break;
    }

    oat.oat_dex_files_.push_back(std::move(dex_file));
  }

  for (size_t i = 0; i < nb_dex_files; ++i) {
    if (i >= oat.oat_dex_files_.size()) {
      LIEF_WARN("DEX file #{} out of bounds", i);
      break;
    }
    uint64_t offset = oat.oat_dex_files_[i]->dex_offset();

    LIEF_DEBUG("Processing DEX file #{:d} at offset {:#x}", i, offset);

    const auto res_dex_hdr = stream_->peek<dex35_header_t>(offset);
    if (!res_dex_hdr) {
      break;
    }
    const auto dex_hdr = *res_dex_hdr;

    std::vector<uint8_t> data_v;
    const auto* data = stream_->peek_array<uint8_t>(offset, dex_hdr.file_size);
    if (data != nullptr) {
      data_v = {data, data + dex_hdr.file_size};
    }

    std::string name = "classes";
    if (i > 0) {
      name += std::to_string(i + 1);
    }
    name += ".dex";

    std::unique_ptr<DexFile>& oat_dex_file = oat.oat_dex_files_[i];
    if (DEX::is_dex(data_v)) {
      std::unique_ptr<DEX::File> dexfile =
          DEX::Parser::parse(std::move(data_v), name);
      dexfile->location(oat_dex_file->location());
      const uint32_t nb_classes = dexfile->header().nb_classes();

      oat_dex_file->dex_file_ = dexfile.get();
      oat.dex_files_.push_back(std::move(dexfile));

      uint32_t classes_offset = classes_offsets_offset[i];
      const uint64_t stream_size = stream_->size();
      if (classes_offset < stream_size) {
        const uint64_t max_offsets =
            (stream_size - classes_offset) / sizeof(uint32_t);
        const auto reserve = std::min<size_t>(max_offsets, nb_classes);
        oat_dex_file->classes_offsets_.reserve(reserve);
      }

      for (size_t cls_idx = 0; cls_idx < nb_classes; ++cls_idx) {
        if (auto off = stream_->peek<uint32_t>(classes_offset +
                                               cls_idx * sizeof(uint32_t)))
        {
          oat_dex_file->classes_offsets_.push_back(*off);
        } else {
          break;
        }
      }
    } else {
      LIEF_WARN("{} ({}) at {:#x} is not a DEX file", name,
                oat_dex_file->location(), stream_->pos());
    }
  }
}


} // namespace LIEF::OAT

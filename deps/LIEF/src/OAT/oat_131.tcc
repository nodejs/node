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

#include "LIEF/OAT/EnumToString.hpp"
#include "LIEF/OAT/Parser.hpp"
#include "LIEF/OAT/DexFile.hpp"
#include "LIEF/OAT/Binary.hpp"
#include "LIEF/VDEX/File.hpp"
#include "OAT/Structures.hpp"

namespace LIEF {
namespace OAT {

template<>
void Parser::parse_dex_files<details::OAT131_t>() {
  auto& oat = oat_binary();
  size_t nb_dex_files = oat.header_.nb_dex_files();

  uint64_t oat_dex_files_offset = oat.header().oat_dex_files_offset();

  LIEF_DEBUG("OAT DEX file located at offset: 0x{:x}", oat_dex_files_offset);

  std::vector<uint32_t> classes_offsets_offset;
  classes_offsets_offset.reserve(nb_dex_files);

  stream_->setpos(oat_dex_files_offset);
  for (size_t i = 0; i < nb_dex_files; ++i) {

    LIEF_DEBUG("Dealing with OAT DEX file #{:d}", i);

    auto dex_file = std::make_unique<DexFile>();

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

    /* uint32_t dex_sections_layout_offset = */ stream_->read<uint32_t>();

    /* uint32_t method_bss_mapping_offset =  */ stream_->read<uint32_t>();

    oat.oat_dex_files_.push_back(std::move(dex_file));
  }

  if (oat_binary().has_vdex()) {
    VDEX::File::it_dex_files dexfiles = oat_binary().vdex_->dex_files();
    if (dexfiles.size() != oat.oat_dex_files_.size()) {
      LIEF_WARN("Inconsistent number of vdex files");
      return;
    }
    for (size_t i = 0; i < dexfiles.size(); ++i) {
      std::unique_ptr<DexFile>& oat_dex_file = oat.oat_dex_files_[i];
      oat_dex_file->dex_file_ = &dexfiles[i];

      const uint32_t nb_classes = dexfiles[i].header().nb_classes();

      uint32_t classes_offset = classes_offsets_offset[i];
      oat_dex_file->classes_offsets_.reserve(nb_classes);
      for (size_t cls_idx = 0; cls_idx < nb_classes; ++cls_idx) {
        if (auto off = stream_->peek<uint32_t>(classes_offset + cls_idx * sizeof(uint32_t))) {
          oat_dex_file->classes_offsets_.push_back(*off);
        } else {
          break;
        }
      }
    }
  }
}







} // Namespace OAT
} // Namespace LIEF

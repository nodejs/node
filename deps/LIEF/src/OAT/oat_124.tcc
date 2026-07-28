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
#include "LIEF/VDEX/File.hpp"

#include "LIEF/OAT/EnumToString.hpp"
#include "LIEF/OAT/Parser.hpp"
#include "LIEF/OAT/DexFile.hpp"
#include "LIEF/OAT/Binary.hpp"

#include "OAT/Structures.hpp"

namespace LIEF {
namespace OAT {

template<>
void Parser::parse_dex_files<details::OAT124_t>() {
  using oat_header = typename details::OAT124_t::oat_header;

  auto& oat = oat_binary();

  size_t nb_dex_files = oat.header().nb_dex_files();

  uint64_t dexfiles_offset = sizeof(oat_header) + oat.header_.key_value_size();

  LIEF_DEBUG("OAT DEX file located at offset: 0x{:x}", dexfiles_offset);

  std::vector<uint32_t> classes_offsets_offset;
  classes_offsets_offset.reserve(nb_dex_files);

  stream_->setpos(dexfiles_offset);
  for (size_t i = 0; i < nb_dex_files; ++i ) {

    LIEF_DEBUG("Dealing with OAT DEX file #{:d}", i);

    std::unique_ptr<DexFile> dex_file{new DexFile{}};

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

    if (auto res = stream_->read<uint32_t>()) {
      dex_file->checksum(*res);
    } else {
      break;
    }

    if (auto res = stream_->read<uint32_t>()) {
      dex_file->dex_offset(*res);
    } else {
      break;
    }

    if (auto res = stream_->read<uint32_t>()) {
      classes_offsets_offset.push_back(*res);
    } else {
      break;
    }

    if (auto res = stream_->read<uint32_t>()) {
      dex_file->lookup_table_offset(*res);
    } else {
      break;
    }

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
        if (auto res = stream_->peek<uint32_t>(classes_offset + cls_idx * sizeof(uint32_t))) {
          oat_dex_file->classes_offsets_.push_back(*res);
        } else {
          break;
        }
      }
    }
  }
}


} // Namespace OAT
} // Namespace LIEF

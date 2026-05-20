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
#include "logging.hpp"

#include "LIEF/VDEX/Parser.hpp"
#include "VDEX/Structures.hpp"
#include "DEX/Structures.hpp"
#include "LIEF/DEX/utils.hpp"
#include "LIEF/DEX/File.hpp"
#include "LIEF/DEX/Class.hpp"
#include "LIEF/DEX/Method.hpp"
#include "LIEF/DEX/Parser.hpp"
#include "LIEF/utils.hpp"

namespace LIEF {
namespace VDEX {

template<typename VDEX_T>
void Parser::parse_file() {

  parse_header<VDEX_T>();
  parse_checksums<VDEX_T>();
  parse_dex_files<VDEX_T>();
  parse_verifier_deps<VDEX_T>();
  parse_quickening_info<VDEX_T>();

}


template<typename VDEX_T>
void Parser::parse_header() {
  using vdex_header = typename VDEX_T::vdex_header;
  const auto res_hdr = stream_->peek<vdex_header>(0);
  if (!res_hdr) {
    return;
  }
  const auto hdr = std::move(*res_hdr);
  file_->header_ = &hdr;
}


template<typename VDEX_T>
void Parser::parse_checksums() {
  //TODO
}

template<typename VDEX_T>
void Parser::parse_dex_files() {
  using vdex_header = typename VDEX_T::vdex_header;
  size_t nb_dex_files = file_->header().nb_dex_files();

  uint64_t current_offset = sizeof(vdex_header) + nb_dex_files * sizeof(details::checksum_t);
  current_offset = align(current_offset, sizeof(uint32_t));

  for (size_t i = 0; i < nb_dex_files; ++i) {
    std::string name = "classes";
    if (i > 0) {
      name += std::to_string(i + 1);
    }
    name += ".dex";

    const auto res_dex_hdr = stream_->peek<DEX::details::header>(current_offset);
    if (!res_dex_hdr) {
      break;
    }
    const auto dex_hdr = *res_dex_hdr;
    const auto* data = stream_->peek_array<uint8_t>(current_offset, dex_hdr.file_size);
    if (data == nullptr) {
      LIEF_WARN("File #{:d} is corrupted!", i);
      continue;
    }

    std::vector<uint8_t> data_v = {data, data + dex_hdr.file_size};

    if (DEX::is_dex(data_v)) {
      std::unique_ptr<DEX::File> dexfile = DEX::Parser::parse(std::move(data_v), name);
      dexfile->name(name);
      file_->dex_files_.push_back(std::move(dexfile));
    } else {
      LIEF_WARN("File #{:d} is not a dex file!", i);
    }
    current_offset += dex_hdr.file_size;
    current_offset = align(current_offset, sizeof(uint32_t));
  }
}


template<typename VDEX_T>
void Parser::parse_verifier_deps() {
  using vdex_header = typename VDEX_T::vdex_header;

  uint64_t deps_offset = align(sizeof(vdex_header) + file_->header().dex_size(), sizeof(uint32_t));

  LIEF_DEBUG("Parsing Verifier deps at 0x{:x}", deps_offset);

  // 1. String table
  // ===============
  //val = stream_->read_uleb128(deps_offset);
  //deps_offset += val.second;
}


// VDEX 06
template<>
void Parser::parse_quickening_info<details::VDEX6>() {
  using vdex_header = typename details::VDEX6::vdex_header;

  uint64_t quickening_offset = sizeof(vdex_header);
  quickening_offset += file_->header().dex_size();
  quickening_offset += file_->header().nb_dex_files() * sizeof(details::checksum_t);
  quickening_offset += file_->header().verifier_deps_size();
  quickening_offset = align(quickening_offset, sizeof(uint32_t));

  LIEF_DEBUG("Parsing Quickening Info at 0x{:x}", quickening_offset);

  if (file_->header().quickening_info_size() == 0) {
    LIEF_DEBUG("No quickening info");
    return;
  }

  stream_->setpos(quickening_offset);

  for (DEX::File& dex_file : file_->dex_files()) {
    for (size_t i = 0; i < dex_file.header().nb_classes(); ++i) {
      DEX::Class* cls = dex_file.get_class(i);
      if (cls == nullptr) {
        LIEF_WARN("Class is null!");
        continue;
      }
      for (DEX::Method& method : cls->methods()) {

        if (method.bytecode().empty()) {
          continue;
        }

        auto res_quickening_size = stream_->read<uint32_t>();
        if (!res_quickening_size) {
          break;
        }

        const auto quickening_size = *res_quickening_size;
        const size_t start_offset = stream_->pos();
        if (quickening_size == 0) {
          continue;
        }

        while (stream_->pos() < (start_offset + quickening_size)) {
          auto pc = stream_->read_uleb128();
          if (!pc) {
            break;
          }

          auto index = stream_->read_uleb128();
          method.insert_dex2dex_info(static_cast<int32_t>(*pc), static_cast<uint16_t>(*index));
        }
      }

    }
  }
}

/*******************************************************
        =========================
        Quickening Info Structure
                  VDEX 10
        =========================


      +---------------------------+ <--------------------
      |                           |
   +->+---------------------------+
   |  | uint32_t code_item_offset |
   |  +---------------------------+
   |  | uint32_t quickening_off   |---+
   |  +---------------------------+   |
   |  |///////////////////////////|   |
   |  |///////////////////////////|   |
   |  |///////////////////////////|   |
   |  |///////////////////////////|   |
   |  +---------------------------+ <-+
   |  | uint32_t quickening_size  |
   |  +---------------------------+ <+
   |  | uint16_t Index Value #0   |  |
   |  | ------------------------- |  |
   |  | uint16_t Index Value #1   |  | Quickening size
   |  | ------------------------- |  |
   |  | uint16_t Index Value #2   |  |
+---->+---------------------------+ <+
|  |  |///////////////////////////|
|  |  +---------------------------+ <--+
|  +--| Dex File #0               |    |
|     +---------------------------+    | Dex Indexes
+-----| Dex File #1               |    |
      +---------------------------+ <--+

See:
  - art/runtime/vdex_file.cc:172 - QuickeningInfoIterator
  - art/runtime/dex_to_dex_decompiler.{h, cc}:
  - art/runtime/quicken_info.h


*******************************************************/




template<>
void Parser::parse_quickening_info<details::VDEX10>() {
  using vdex_header = typename details::VDEX10::vdex_header;

  const uint64_t quickening_size = file_->header().quickening_info_size();
  const size_t nb_dex_files = file_->header().nb_dex_files();

  uint64_t quickening_base = sizeof(vdex_header);
  quickening_base += file_->header().dex_size();
  quickening_base += file_->header().nb_dex_files() * sizeof(details::checksum_t);
  quickening_base += file_->header().verifier_deps_size();
  quickening_base = align(quickening_base, sizeof(uint32_t));

  LIEF_DEBUG("Parsing Quickening Info at 0x{:x}", quickening_base);

  if (quickening_size == 0) {
    LIEF_DEBUG("No quickening info");
    return;
  }


  // Offset of the "Dex Indexes" array
  uint64_t dex_file_indices_off = quickening_base + quickening_size - nb_dex_files * sizeof(uint32_t);

  if (nb_dex_files > file_->dex_files_.size()) {
    LIEF_WARN("Inconsistent number of dex files");
    return;
  }

  for (size_t i = 0; i < nb_dex_files; ++i) {
    std::unique_ptr<DEX::File>& dex_file = file_->dex_files_[i];

    // Code item offset of the first method
    auto res_current_code_item = stream_->peek<uint32_t>(dex_file_indices_off + i * sizeof(uint32_t));

    if (!res_current_code_item) {
      break;
    }

    auto current_code_item = quickening_base + *res_current_code_item;

    // End
    uint64_t code_item_end = dex_file_indices_off;
    if (i < (nb_dex_files - 1)) {
      if (auto res = stream_->peek<uint32_t>(dex_file_indices_off + (i + 1) * sizeof(uint32_t))) {
        code_item_end = quickening_base + *res;
      } else {
        break;
      }
    }

    size_t nb_code_item = (code_item_end - current_code_item) / (2 * sizeof(uint32_t)); // The array is compounded of
                                                                                        // 1. Code item offset
                                                                                        // 2. Quickening offset


    //  +---------------+         +-----------+
    //  | code_item_off |-------> | index #0  |
    //  +---------------+         +-----------+
    //                            | index #1  |
    //                            +-----------+
    //                                 ...

    std::map<uint32_t, std::vector<uint16_t>> quick_info;

    for (size_t j = 0; j < nb_code_item; ++j) {

      // code_item_offset on the diagram
      uint32_t method_code_item_offset = 0;
      if (auto res = stream_->peek<uint32_t>(current_code_item)) {
        method_code_item_offset = *res;
      } else {
        break;
      }

      // Offset of the quickening data
      uint64_t method_quickening_info_offset = quickening_base;
      if (auto res = stream_->peek<uint32_t>(current_code_item + sizeof(uint32_t))) {
        method_quickening_info_offset += *res;
      } else {
        break;
      }

      // Quickening size
      uint64_t method_quickening_info_size = 0;
      if (auto res = stream_->peek<uint32_t>(method_quickening_info_offset)) {
        method_quickening_info_size = *res;
      } else {
        break;
      }

      uint64_t quickening_offset_local = method_quickening_info_offset + sizeof(uint32_t); // + Quickening size entry

      const size_t nb_indices = method_quickening_info_size / sizeof(uint16_t); // index values are stored as uint16_t

      for (size_t quick_idx = 0; quick_idx < nb_indices; ++quick_idx) {
        if (auto index = stream_->peek<uint16_t>(quickening_offset_local + quick_idx * sizeof(uint16_t))) {
          quick_info[method_code_item_offset].push_back(*index);
        } else {
          break;
        }
      }
      current_code_item += 2 * sizeof(uint32_t); // sizeof(code_item_offset) + sizeof(quickening_base)
    }

    // Resolve methods offset
    const std::vector<uint8_t>& raw = dex_file->raw(/* deoptimize */false);
    for (DEX::Method& method : dex_file->methods()) {
      const auto it_quick = quick_info.find(method.code_offset() - sizeof(DEX::details::code_item));
      if (it_quick == std::end(quick_info)) {
        continue;
      }

      const std::vector<uint16_t>& quickinfo = it_quick->second;

      size_t nb_indexes = quickinfo.size();

      const uint8_t* inst_start = raw.data() + method.code_offset();
      const uint8_t* inst_end = inst_start + method.bytecode().size();

      const uint8_t* inst_ptr = inst_start;

      while (nb_indexes > 0 && inst_ptr < inst_end) {
        uint16_t dex_pc = (inst_ptr - inst_start) / sizeof(uint16_t);
        auto opcode = static_cast<DEX::OPCODES>(*inst_ptr);
        uint16_t index_value = quickinfo[quickinfo.size() - nb_indexes];

        // Skip packed-switch, sparse-switch, fill-array instructions
        if (DEX::is_switch_array(inst_ptr, inst_end)) {
          inst_ptr += DEX::switch_array_size(inst_ptr, inst_end);
          continue;
        }

        switch(opcode) {
          case DEX::OPCODES::OP_IGET_QUICK:
          case DEX::OPCODES::OP_IGET_WIDE_QUICK:
          case DEX::OPCODES::OP_IGET_OBJECT_QUICK:
          case DEX::OPCODES::OP_IPUT_QUICK:
          case DEX::OPCODES::OP_IPUT_WIDE_QUICK:
          case DEX::OPCODES::OP_IPUT_OBJECT_QUICK:
          case DEX::OPCODES::OP_INVOKE_VIRTUAL_QUICK:
          case DEX::OPCODES::OP_INVOKE_VIRTUAL_RANGE_QUICK:
          case DEX::OPCODES::OP_IPUT_BOOLEAN_QUICK:
          case DEX::OPCODES::OP_IPUT_BYTE_QUICK:
          case DEX::OPCODES::OP_IPUT_CHAR_QUICK:
          case DEX::OPCODES::OP_IPUT_SHORT_QUICK:
          case DEX::OPCODES::OP_IGET_BOOLEAN_QUICK:
          case DEX::OPCODES::OP_IGET_BYTE_QUICK:
          case DEX::OPCODES::OP_IGET_CHAR_QUICK:
          case DEX::OPCODES::OP_IGET_SHORT_QUICK:
            {

              method.insert_dex2dex_info(dex_pc, index_value);
              nb_indexes--;
              break;
            }
          case DEX::OPCODES::OP_NOP:
            {
              if (index_value == static_cast<uint16_t>(-1)) {
                nb_indexes--;
              } else {
                if (nb_indexes > 1) {
                  nb_indexes -= 2;
                } else {
                  nb_indexes--;
                }
              }
              break;
            }
          default:
            {
            }
        }
        inst_ptr += DEX::inst_size_from_opcode(opcode);
      }
    }
  }
}

template<class T>
void Parser::parse_quickening_info() {
  return parse_quickening_info<details::VDEX10>();
}


}
}

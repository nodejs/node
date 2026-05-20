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

#include <string>
#include <vector>

#include "LIEF/MachO/utils.hpp"
#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"

#include "Object.tcc"

#include "MachO/Structures.hpp"


#include "LIEF/BinaryStream/FileStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "logging.hpp"


namespace LIEF {
namespace MachO {

inline result<MACHO_TYPES> magic_from_stream(BinaryStream& stream,
                                             bool keep_offset = false) {
  ScopedStream scoped(stream);
  if (!keep_offset) {
    scoped->setpos(0);
  }

  if (auto magic_res = scoped->read<uint32_t>()) {
    return static_cast<MACHO_TYPES>(*magic_res);
  }
  return make_error_code(lief_errors::read_error);
}

bool is_macho(BinaryStream& stream) {
  if (auto magic_res = magic_from_stream(stream)) {
    const MACHO_TYPES magic = *magic_res;
    return (magic == MACHO_TYPES::MAGIC ||
            magic == MACHO_TYPES::CIGAM ||
            magic == MACHO_TYPES::MAGIC_64 ||
            magic == MACHO_TYPES::CIGAM_64 ||
            magic == MACHO_TYPES::MAGIC_FAT ||
            magic == MACHO_TYPES::CIGAM_FAT ||
            magic == MACHO_TYPES::NEURAL_MODEL);
  }
  return false;
}

bool is_macho(const std::string& file) {
  if (auto stream = FileStream::from_file(file)) {
    return is_macho(*stream);
  }
  return false;
}

bool is_macho(const std::vector<uint8_t>& raw) {
  if (auto stream = SpanStream::from_vector(raw)) {
    return is_macho(*stream);
  }
  return false;
}

bool is_fat(const std::string& file) {
  if (auto stream = FileStream::from_file(file)) {
    if (auto magic_res = magic_from_stream(*stream)) {
      const MACHO_TYPES magic = *magic_res;
      return magic == MACHO_TYPES::MAGIC_FAT ||
             magic == MACHO_TYPES::CIGAM_FAT;
    }
  }
  return false;
}

bool is_64(BinaryStream& stream) {
  ScopedStream scoped(stream, 0);
  if (auto magic_res = magic_from_stream(*scoped)) {
    const MACHO_TYPES magic = *magic_res;
    return magic == MACHO_TYPES::MAGIC_64 ||
           magic == MACHO_TYPES::CIGAM_64;
  }
  return false;
}

bool is_64(const std::string& file) {
  if (auto stream = FileStream::from_file(file)) {
    return is_64(*stream);
  }
  return false;
}


template<class MACHO_T>
void foreach_segment_impl(BinaryStream& stream, const segment_callback_t cbk) {
  using header_t = typename MACHO_T::header;
  using segment_command_t = typename MACHO_T::segment_command;
  auto res_hdr = stream.read<header_t>();
  if (!res_hdr) {
    return;
  }
  const auto& hdr = *res_hdr;

  for (size_t i = 0; i < hdr.ncmds; ++i) {
    const auto raw_cmd = stream.peek<details::load_command>();
    if (!raw_cmd) {
      break;
    }
    const auto cmd = LoadCommand::TYPE(raw_cmd->cmd);
    const bool is_segment = cmd == LoadCommand::TYPE::SEGMENT ||
                            cmd == LoadCommand::TYPE::SEGMENT_64;
    if (is_segment) {
      auto res_segment = stream.peek<segment_command_t>();
      if (!res_segment) {
        break;
      }
      std::string segname(res_segment->segname, 16);
      cbk(segname, res_segment->fileoff, res_segment->filesize,
          res_segment->vmaddr, res_segment->vmsize);
    }

    stream.increment_pos(raw_cmd->cmdsize);
  }
}

void foreach_segment(BinaryStream& stream, const segment_callback_t cbk) {
  ScopedStream scoped(stream);
  auto magic_res = magic_from_stream(*scoped, /*keep_offset=*/true);
  if (!magic_res) {
    return;
  }

  const MACHO_TYPES magic = *magic_res;

  if (magic == MACHO_TYPES::MAGIC_FAT || magic == MACHO_TYPES::CIGAM_FAT) {
    LIEF_WARN("Can't get the file size of a FAT Macho-O");
    return;
  }

  const bool is64 = magic == MACHO_TYPES::MAGIC_64 ||
                    magic == MACHO_TYPES::CIGAM_64;

  const bool is32 = magic == MACHO_TYPES::MAGIC ||
                    magic == MACHO_TYPES::CIGAM;

  if (!is64 && !is32) {
    return;
  }

  return is64 ? foreach_segment_impl<details::MachO64>(*scoped, cbk) :
                foreach_segment_impl<details::MachO32>(*scoped, cbk);
}

}
}


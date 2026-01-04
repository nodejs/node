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
#include "LIEF/COFF/BigObjHeader.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "COFF/structures.hpp"
#include "internal_utils.hpp"

#include <spdlog/fmt/fmt.h>
#include <sstream>

namespace LIEF::COFF {

std::unique_ptr<BigObjHeader> BigObjHeader::create(BinaryStream& stream) {
  auto raw = stream.read<details::bigobj_header>();
  if (!raw) {
    return nullptr;
  }
  auto hdr = std::make_unique<BigObjHeader>();

  // Base class
  hdr->machine_ = (Header::MACHINE_TYPES)raw->machine;
  hdr->nb_sections_ = raw->numberof_sections;
  hdr->pointerto_symbol_table_ = raw->pointerto_symbol_table;
  hdr->nb_symbols_ = raw->numberof_symbols;
  hdr->timedatestamp_ = raw->timedatestamp;

  // BigObj specific
  hdr->version_ = raw->version;
  std::copy(std::begin(raw->uuid), std::end(raw->uuid),
            std::begin(hdr->uuid_));
  hdr->sizeof_data_ = raw->unused1;
  hdr->flags_ = raw->unused2;
  hdr->metadata_size_ = raw->unused3;
  hdr->metadata_offset_ = raw->unused4;

  return hdr;
}

std::string BigObjHeader::to_string() const {
  using namespace fmt;
  std::ostringstream oss;

  static constexpr auto WIDTH = 16;
  oss << Header::to_string() << '\n';
  oss << format("{:>{}} Version\n", version(), WIDTH);
  oss << format("{:>{}} uuid: {}\n", "", WIDTH, uuid_to_str_impl(uuid_));
  oss << format("{:>#{}x} Size of data\n", sizeof_data(), WIDTH);
  oss << format("{:>#{}x} Flags\n", flags(), WIDTH);
  oss << format("{:>#{}x} Metadata size\n", metadata_size(), WIDTH);
  oss << format("{:>#{}x} Metadata offset", metadata_offset(), WIDTH);
  return oss.str();
}

}

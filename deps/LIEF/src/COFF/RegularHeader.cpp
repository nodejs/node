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
#include "LIEF/COFF/RegularHeader.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "COFF/structures.hpp"

#include "logging.hpp"

#include <spdlog/fmt/fmt.h>
#include <sstream>

namespace LIEF::COFF {
std::unique_ptr<RegularHeader> RegularHeader::create(BinaryStream& stream) {
  auto raw = stream.read<details::header>();
  if (!raw) {
    return nullptr;
  }
  auto hdr = std::make_unique<RegularHeader>();

  // Base class
  hdr->machine_ = (Header::MACHINE_TYPES)raw->machine;
  hdr->nb_sections_ = raw->numberof_sections;
  hdr->pointerto_symbol_table_ = raw->pointerto_symbol_table;
  hdr->nb_symbols_ = raw->numberof_symbols;
  hdr->timedatestamp_ = raw->timedatestamp;

  // RegularHeader specific
  hdr->characteristics_ = raw->characteristics;
  hdr->sizeof_optionalheader_ = raw->sizeof_optionalheader;

  return hdr;
}

std::string RegularHeader::to_string() const {
  using namespace fmt;
  std::ostringstream oss;

  static constexpr auto WIDTH = 16;
  oss << Header::to_string() << '\n';
  oss << format("{:>{}} Size of optional header\n", sizeof_optionalheader(), WIDTH);
  oss << format("{:>#{}x} Characteristics", characteristics(), WIDTH);
  return oss.str();
}

}

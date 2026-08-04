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
#include "LIEF/PE/debug/PDBChecksum.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "logging.hpp"

#include "internal_utils.hpp"

namespace LIEF::PE {
using HASH_ALGO = PDBChecksum::HASH_ALGO;
HASH_ALGO from_string(const std::string& str) {
  if (str == "SHA256") {
    return HASH_ALGO::SHA256;
  }

  return HASH_ALGO::UNKNOWN;
}

std::unique_ptr<PDBChecksum> PDBChecksum::parse(
  const details::pe_debug& hdr, Section* section, span<uint8_t> payload
) {
  SpanStream strm(payload);
  auto algo_name = strm.read_string();
  if (!algo_name) {
    LIEF_DEBUG("{}:{} failed", __FUNCTION__, __LINE__);
    return nullptr;
  }

  std::vector<uint8_t> hash;
  if (!strm.read_data(hash)) {
    LIEF_DEBUG("{}:{} failed", __FUNCTION__, __LINE__);
    return nullptr;
  }

  LIEF_DEBUG("PDBChecksum: {} ({} bytes)", *algo_name, hash.size());

  return std::make_unique<PDBChecksum>(hdr, section, from_string(*algo_name),
                                       std::move(hash));
}

std::string PDBChecksum::to_string() const {
  using namespace fmt;
  std::ostringstream os;
  os << Debug::to_string() << '\n'
     << "PDB Checksum:\n"
     << format("  Algorithm: {}\n", PE::to_string(algorithm()))
     << format("  Hash: {}", to_hex(hash(), 30));

  return os.str();
}

const char* to_string(PDBChecksum::HASH_ALGO e) {
  switch (e) {
    case HASH_ALGO::UNKNOWN:
    default:
      return "UNKNOWN";

    case HASH_ALGO::SHA256:
      return "SHA256";
  }
  return "UNKNOWN";
}
}

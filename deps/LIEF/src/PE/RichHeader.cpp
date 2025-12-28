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
#include <iomanip>

#include "LIEF/PE/hash.hpp"
#include "LIEF/PE/RichHeader.hpp"
#include "LIEF/iostream.hpp"
#include "LIEF/PE/EnumToString.hpp"

#include "frozen.hpp"
#include "logging.hpp"

#include "hash_stream.hpp"

namespace LIEF {
namespace PE {

void RichHeader::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::vector<uint8_t> RichHeader::raw(uint32_t xor_key) const {
  vector_iostream wstream;

  wstream
    .write(DANS_MAGIC_NUMBER ^ xor_key)
    /*
     * The first chunk needs to be aligned on 64-bit and padded
     * with 0-xor. We can't use vector_iostream::align as it would not
     * be encoded.
     */
    .write<uint32_t>(0 ^ xor_key)
    .write<uint32_t>(0 ^ xor_key)
    .write<uint32_t>(0 ^ xor_key);

  for (auto it = entries_.crbegin(); it != entries_.crend(); ++it) {
    const RichEntry& entry = *it;
    const uint32_t value = (static_cast<uint32_t>(entry.id()) << 16) | entry.build_id();
    wstream
      .write(value ^ xor_key).write(entry.count() ^ xor_key);
  }
  wstream
    .write(RICH_MAGIC, std::size(RICH_MAGIC)).write(xor_key);

  return wstream.raw();
}

std::vector<uint8_t> RichHeader::hash(ALGORITHMS algo, uint32_t xor_key) const {
  CONST_MAP(ALGORITHMS, hashstream::HASH, 5) HMAP = {
    {ALGORITHMS::MD5,     hashstream::HASH::MD5},
    {ALGORITHMS::SHA_1,   hashstream::HASH::SHA1},
    {ALGORITHMS::SHA_256, hashstream::HASH::SHA256},
    {ALGORITHMS::SHA_384, hashstream::HASH::SHA384},
    {ALGORITHMS::SHA_512, hashstream::HASH::SHA512},
  };

  const auto it_hash = HMAP.find(algo);
  if (it_hash == std::end(HMAP)) {
    LIEF_WARN("Unsupported hash algorithm: {}", to_string(algo));
    return {};
  }

  const hashstream::HASH hash_type = it_hash->second;
  hashstream hs(hash_type);
  const std::vector<uint8_t> clear_raw = raw(xor_key);
  hs.write(clear_raw.data(), clear_raw.size());
  return hs.raw();
}

std::ostream& operator<<(std::ostream& os, const RichHeader& rich_header) {
  using namespace fmt;
  os << format("Key: 0x{:08x} ({} entries)\n", rich_header.key(),
               rich_header.entries().size());
  for (const RichEntry& entry : rich_header.entries()) {
    os << "  " << entry << '\n';
  }
  return os;
}

}
}

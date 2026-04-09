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
#include <algorithm>
#include <iterator>
#include <vector>

#include "LIEF/BinaryStream/FileStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/ELF/utils.hpp"
#include "ELF/Structures.hpp"

namespace LIEF {
namespace ELF {

bool is_elf(BinaryStream& stream) {
  using magic_t = std::array<char, sizeof(details::ElfMagic)>;
  ScopedStream scoped(stream, 0);
  if (auto res = scoped->peek<magic_t>()) {
    const auto magic = *res;
    return std::equal(std::begin(magic), std::end(magic),
                      std::begin(details::ElfMagic));
  }
  return false;
}

bool is_elf(const std::string& file) {
  if (auto stream = FileStream::from_file(file)) {
    return is_elf(*stream);
  }
  return false;
}

bool is_elf(const std::vector<uint8_t>& raw) {
  if (auto stream = SpanStream::from_vector(raw)) {
    return is_elf(*stream);
  }
  return false;
}

/// SYSV hash function
unsigned long hash32(const char* name) {
  unsigned long h = 0, g;
  while (*name != 0) {
    h = (h << 4) + *name++;
    if ((g = h & 0xf0000000) != 0u) {
      h ^= g >> 24;
    }
    h &= ~g;
  }
  return h;
}

/// SYSV hash function
/// https://blogs.oracle.com/ali/entry/gnu_hash_elf_sections
unsigned long hash64(const char* name) {
  unsigned long h = 0, g;
  while (*name != 0) {
    h = (h << 4) + *name++;
    if ((g = h & 0xf0000000) != 0u) {
      h ^= g >> 24;
    }
    h &= 0x0fffffff;
  }
  return h;
}

uint32_t dl_new_hash(const char* name) {
  uint32_t h = 5381;

  for (unsigned char c = *name; c != '\0'; c = *++name) {
    h = h * 33 + c;
  }

  return h & 0xffffffff;
}

} // namespace ELF
} // namespace LIEF

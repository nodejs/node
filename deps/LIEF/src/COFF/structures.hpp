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
#ifndef LIEF_COFF_STRUCTURES_H
#define LIEF_COFF_STRUCTURES_H
#include <cstdint>
#include <array>

namespace LIEF::COFF::details {

static constexpr auto NameSize = 8;

struct header {
  uint16_t machine;
  uint16_t numberof_sections;
  uint32_t timedatestamp;
  uint32_t pointerto_symbol_table;
  uint32_t numberof_symbols;
  uint16_t sizeof_optionalheader;
  uint16_t characteristics;
};

struct bigobj_header {
  uint16_t sig1; ///< must be image_file_machine_unknown (0).
  uint16_t sig2; ///< must be 0xffff.
  uint16_t version;
  uint16_t machine;
  uint32_t timedatestamp;
  uint8_t uuid[16];
  uint32_t unused1;
  uint32_t unused2;
  uint32_t unused3;
  uint32_t unused4;
  uint32_t numberof_sections;
  uint32_t pointerto_symbol_table;
  uint32_t numberof_symbols;
};


#pragma pack(push,1)
struct relocation {
  uint32_t VirtualAddress;
  uint32_t SymbolTableIndex;
  uint16_t Type;
};

template<class T>
struct symbol {
  union {
    std::array<char, 8> short_name;
    struct {
      uint32_t zeroes = 0;
      uint32_t offset = 0;
    } offset;
  } name;

  uint32_t value;
  T sec_idx;
  uint16_t type;
  uint8_t storage_class;
  uint8_t nb_aux;
};

using symbol16 = symbol<uint16_t>;
using symbol32 = symbol<uint32_t>;
#pragma pack(pop)

static_assert(sizeof(symbol16) == 18);
static_assert(sizeof(symbol32) == 20);

}
#endif

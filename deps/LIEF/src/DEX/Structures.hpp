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
#ifndef LIEF_DEX_STRUCTURES_H
#define LIEF_DEX_STRUCTURES_H

#include <cstring>
#include <tuple>

#include "LIEF/types.hpp"
#include "LIEF/DEX/enums.hpp"
#include "LIEF/DEX/types.hpp"

namespace LIEF {
/// Namespace related to the LIEF's DEX module
namespace DEX {

namespace details {
static constexpr uint8_t magic[] = { 'd', 'e', 'x', '\n' };

static constexpr uint32_t NO_INDEX = 0xffffffff;

struct header {
  uint8_t  magic[8];
  uint32_t checksum;
  uint8_t  signature[20];
  uint32_t file_size;
  uint32_t header_size;
  uint32_t endian_tag;
  uint32_t link_size;
  uint32_t link_off;
  uint32_t map_off;
  uint32_t string_ids_size;
  uint32_t string_ids_off;
  uint32_t type_ids_size;
  uint32_t type_ids_off;
  uint32_t proto_ids_size;
  uint32_t proto_ids_off;
  uint32_t field_ids_size;
  uint32_t field_ids_off;
  uint32_t method_ids_size;
  uint32_t method_ids_off;
  uint32_t class_defs_size;
  uint32_t class_defs_off;
  uint32_t data_size;
  uint32_t data_off;
};


struct class_def_item {
  uint32_t class_idx;
  uint32_t access_flags;
  uint32_t superclass_idx;
  uint32_t interfaces_off;
  uint32_t source_file_idx;
  uint32_t annotations_off;
  uint32_t class_data_off;
  uint32_t static_values_off;
};

struct method_id_item {
  uint16_t class_idx;
  uint16_t proto_idx;
  uint32_t name_idx;
};

struct field_id_item {
  uint16_t class_idx;
  uint16_t type_idx;
  uint32_t name_idx;
};

struct string_data_item {
  uint8_t *utf16_size;
  uint8_t *data;
};

struct map_items {
  uint16_t type;
  uint16_t unused;
  uint32_t size;
  uint32_t offset;
};

struct map {
  uint32_t  size;
  map_items *items;
};

struct proto_id_item {
  uint32_t shorty_idx;
  uint32_t return_type_idx;
  uint32_t parameters_off;
};

struct code_item {
  uint16_t registers_size;
  uint16_t ins_size;
  uint16_t outs_size;
  uint16_t tries_size;
  uint32_t debug_info_off;
  uint32_t insns_size;
};


namespace DEX_35 {

using header           = DEX::details::header;
using class_def_item   = DEX::details::class_def_item;
using method_id_item   = DEX::details::method_id_item;
using field_id_item    = DEX::details::field_id_item;
using string_data_item = DEX::details::string_data_item;
using map_items        = DEX::details::map_items;
using map              = DEX::details::map;
using proto_id_item    = DEX::details::proto_id_item;
using code_item        = DEX::details::code_item;

static constexpr dex_version_t dex_version = 35;
static constexpr uint8_t magic[] = { 'd', 'e', 'x', '\n', '0', '3', '5', '\0' };

}

namespace DEX_37 {

using header           = DEX::details::header;
using class_def_item   = DEX::details::class_def_item;
using method_id_item   = DEX::details::method_id_item;
using field_id_item    = DEX::details::field_id_item;
using string_data_item = DEX::details::string_data_item;
using map_items        = DEX::details::map_items;
using map              = DEX::details::map;
using proto_id_item    = DEX::details::proto_id_item;
using code_item        = DEX::details::code_item;

static constexpr dex_version_t dex_version = 37;
static constexpr uint8_t magic[] = { 'd', 'e', 'x', '\n', '0', '3', '7', '\0' };

}

namespace DEX_38 {

using header           = DEX::details::header;
using class_def_item   = DEX::details::class_def_item;
using method_id_item   = DEX::details::method_id_item;
using field_id_item    = DEX::details::field_id_item;
using string_data_item = DEX::details::string_data_item;
using map_items        = DEX::details::map_items;
using map              = DEX::details::map;
using proto_id_item    = DEX::details::proto_id_item;
using code_item        = DEX::details::code_item;

static constexpr dex_version_t dex_version = 38;
static constexpr uint8_t magic[] = { 'd', 'e', 'x', '\n', '0', '3', '8', '\0' };

}

namespace DEX_39 {

using header           = DEX::details::header;
using class_def_item   = DEX::details::class_def_item;
using method_id_item   = DEX::details::method_id_item;
using field_id_item    = DEX::details::field_id_item;
using string_data_item = DEX::details::string_data_item;
using map_items        = DEX::details::map_items;
using map              = DEX::details::map;
using proto_id_item    = DEX::details::proto_id_item;
using code_item        = DEX::details::code_item;

static constexpr dex_version_t dex_version = 39;
static constexpr uint8_t magic[] = { 'd', 'e', 'x', '\n', '0', '3', '9', '\0' };

}


class DEX35 {
  public:
  using dex_header       = DEX_35::header;
  using class_def_item   = DEX_35::class_def_item;
  using method_id_item   = DEX_35::method_id_item;
  using field_id_item    = DEX_35::field_id_item;
  using string_data_item = DEX_35::string_data_item;
  using map_items        = DEX_35::map_items;
  using map              = DEX_35::map;
  using proto_id_item    = DEX_35::proto_id_item;
  using code_item        = DEX_35::code_item;

  static constexpr dex_version_t dex_version = DEX_35::dex_version;
};

class DEX37 {
  public:
  using dex_header       = DEX_37::header;
  using class_def_item   = DEX_37::class_def_item;
  using method_id_item   = DEX_37::method_id_item;
  using field_id_item    = DEX_37::field_id_item;
  using string_data_item = DEX_37::string_data_item;
  using map_items        = DEX_37::map_items;
  using map              = DEX_37::map;
  using proto_id_item    = DEX_37::proto_id_item;
  using code_item        = DEX_37::code_item;

  static constexpr dex_version_t dex_version = DEX_37::dex_version;
};

class DEX38 {
  public:
  using dex_header       = DEX_38::header;
  using class_def_item   = DEX_38::class_def_item;
  using method_id_item   = DEX_38::method_id_item;
  using field_id_item    = DEX_38::field_id_item;
  using string_data_item = DEX_38::string_data_item;
  using map_items        = DEX_38::map_items;
  using map              = DEX_38::map;
  using proto_id_item    = DEX_38::proto_id_item;
  using code_item        = DEX_38::code_item;

  static constexpr dex_version_t dex_version = DEX_38::dex_version;
};

class DEX39 {
  public:
  using dex_header       = DEX_39::header;
  using class_def_item   = DEX_39::class_def_item;
  using method_id_item   = DEX_39::method_id_item;
  using field_id_item    = DEX_39::field_id_item;
  using string_data_item = DEX_39::string_data_item;
  using map_items        = DEX_39::map_items;
  using map              = DEX_39::map;
  using proto_id_item    = DEX_39::proto_id_item;
  using code_item        = DEX_39::code_item;

  static constexpr dex_version_t dex_version = DEX_39::dex_version;
};
} /* end namespace details */
} /* end namespace DEX */
} /* end namespace LIEF */
#endif


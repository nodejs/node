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
#ifndef LIEF_OAT_STRUCTURES_H
#define LIEF_OAT_STRUCTURES_H

#include <cstring>
#include <tuple>

#include "LIEF/types.hpp"

// ======================
// Android 6.0.0: OAT 064
// Android 6.0.1: OAT 064
//
// Android 7.0.0: OAT 079
// Android 7.1.0: OAT 079
//
// Android 7.1.1: OAT 088
// Android 7.1.2: OAT 088
//
// Android 8.0.0: OAT 126
// Android 8.1.0: OAT 131
//
// Android 9.0.0  OAT 138
// ======================

namespace LIEF {
/// Namespace related to the LIEF's OAT module
namespace OAT {

namespace details {
using oat_version_t = uint32_t;

static constexpr uint8_t oat_magic[]   = { 'o', 'a', 't', '\n' };
static constexpr oat_version_t oat_version = 0;

// =======================
// OAT 064 - Android 6.0.1
// ========================
namespace OAT_064 {
static constexpr oat_version_t oat_version = 64;
struct oat_header {
  uint8_t magic[sizeof(oat_magic)];
  uint8_t oat_version[4];
  uint32_t adler32_checksum;
  uint32_t instruction_set;
  uint32_t instruction_set_features_bitmap;
  uint32_t dex_file_count;
  uint32_t executable_offset;
  uint32_t i2i_bridge_offset;
  uint32_t i2c_code_bridge_offset;
  uint32_t jni_dlsym_lookup_offset;
  uint32_t quick_generic_jni_trampoline_offset;
  uint32_t quick_imt_conflict_trampoline_offset;
  uint32_t quick_resolution_trampoline_offset;
  uint32_t quick_to_interpreter_bridge_offset;

  int32_t image_patch_delta;

  uint32_t image_file_location_oat_checksum;
  uint32_t image_file_location_oat_data_begin;

  uint32_t key_value_store_size;
  //uint8_t key_value_store[0];  // note variable width data at end
};

struct oat_dex_file {
  uint32_t dex_file_location_size;
  uint8_t* dex_file_location_data;
  uint32_t dex_file_location_checksum;
  uint32_t dex_file_offset;
};

struct dex_file {
  uint8_t magic[8];
  uint32_t checksum;      // See also location_checksum_
  uint8_t signature[20];
  uint32_t file_size;     // size of entire file
  uint32_t header_size;   // offset to start of next section
  uint32_t endian_tag;
  uint32_t link_size;     // unused
  uint32_t link_off;      // unused
  uint32_t map_off;       // unused
  uint32_t string_ids_size;  // number of StringIds
  uint32_t string_ids_off;  // file offset of StringIds array
  uint32_t type_ids_size;  // number of TypeIds, we don't support more than 65535
  uint32_t type_ids_off;  // file offset of TypeIds array
  uint32_t proto_ids_size;  // number of ProtoIds, we don't support more than 65535
  uint32_t proto_ids_off;  // file offset of ProtoIds array
  uint32_t field_ids_size;  // number of FieldIds
  uint32_t field_ids_off;  // file offset of FieldIds array
  uint32_t method_ids_size;  // number of MethodIds
  uint32_t method_ids_off;  // file offset of MethodIds array
  uint32_t class_defs_size;  // number of ClassDefs
  uint32_t class_defs_off;  // file offset of ClassDef array
  uint32_t data_size;  // unused
  uint32_t data_off;  // unused
};


// Defined in art/runtime/oat.h
struct oat_quick_method_header {
  uint32_t mapping_table_offset;
  uint32_t vmap_table_offset;
  uint32_t gc_map_offset;

  uint32_t frame_size_in_bytes;
  uint32_t core_spill_mask;
  uint32_t fp_spill_mask;

  uint32_t code_size;
};



}

// OAT 079 - Android 7.0.0
namespace OAT_079 {
static constexpr oat_version_t oat_version = 79;

using OAT_064::oat_header;
using OAT_064::oat_dex_file;
using OAT_064::dex_file;

// Defined in
// - art/runtime/oat_quick_method_header.h
// - art/runtime/quick_frame_info.h
struct oat_quick_method_header {
  uint32_t vmap_table_offset;

  uint32_t frame_size_in_bytes;
  uint32_t core_spill_mask;
  uint32_t fp_spill_mask;

  uint32_t code_size;
};

// - art/runtime/type_lookup_table.h
struct lookup_table_entry_t {
  uint32_t str_offset;
  uint16_t data;
  uint16_t next_pos_delta;
};

}

namespace OAT_088 {

static constexpr oat_version_t oat_version = 88;

using OAT_079::oat_header;
using OAT_079::oat_dex_file;
using OAT_079::dex_file;
using OAT_079::oat_quick_method_header;
using OAT_079::lookup_table_entry_t;


}


// OAT 124 - Android 8.0.0
namespace OAT_124 {

static constexpr oat_version_t oat_version = 126;

using OAT_088::oat_header;
using OAT_088::oat_dex_file;
using OAT_088::dex_file;

using OAT_088::lookup_table_entry_t;


// Defined in
// - art/runtime/oat_quick_method_header.h
// - art/runtime/quick_frame_info.h
struct oat_quick_method_header {
  uint32_t vmap_table_offset;
  uint32_t method_info_offset;

  uint32_t frame_size_in_bytes;
  uint32_t core_spill_mask;
  uint32_t fp_spill_mask;

  uint32_t code_size;
};
}


// OAT 131  - Android 8.1.0
namespace OAT_131 {

static constexpr oat_version_t oat_version = 131;
struct oat_header {
  uint8_t magic[sizeof(oat_magic)];
  uint8_t oat_version[4];
  uint32_t adler32_checksum;
  uint32_t instruction_set;
  uint32_t instruction_set_features_bitmap;
  uint32_t dex_file_count;
  uint32_t oat_dex_files_offset;                      // ADDED in OAT 131
  uint32_t executable_offset;
  uint32_t i2i_bridge_offset;
  uint32_t i2c_code_bridge_offset;
  uint32_t jni_dlsym_lookup_offset;
  uint32_t quick_generic_jni_trampoline_offset;
  uint32_t quick_imt_conflict_trampoline_offset;
  uint32_t quick_resolution_trampoline_offset;
  uint32_t quick_to_interpreter_bridge_offset;

  int32_t image_patch_delta;

  uint32_t image_file_location_oat_checksum;
  uint32_t image_file_location_oat_data_begin;

  uint32_t key_value_store_size;
  //uint8_t key_value_store[0];  // note variable width data at end
};

using OAT_124::oat_dex_file;
using OAT_124::dex_file;

using OAT_124::oat_quick_method_header;
using OAT_124::lookup_table_entry_t;
}


// OAT 138  - Android 9.0.0
namespace OAT_138  {
static constexpr oat_version_t oat_version = 138;
using OAT_131::oat_header;
using OAT_131::oat_dex_file;
using OAT_131::dex_file;

using OAT_131::oat_quick_method_header;
using OAT_131::lookup_table_entry_t;
}

class OAT64_t {
  public:
  static constexpr oat_version_t oat_version = OAT_064::oat_version;
  using oat_header   = OAT_064::oat_header;
  using oat_dex_file = OAT_064::oat_dex_file;
  using dex_file     = OAT_064::dex_file;
  using oat_quick_method_header = OAT_064::oat_quick_method_header;
};

class OAT79_t {
  public:
  static constexpr oat_version_t oat_version = OAT_079::oat_version;
  using oat_header   = OAT_079::oat_header;
  using oat_dex_file = OAT_079::oat_dex_file;
  using dex_file     = OAT_079::dex_file;

  using oat_quick_method_header = OAT_079::oat_quick_method_header;
  using lookup_table_entry_t = OAT_079::lookup_table_entry_t;
};

class OAT88_t {
  public:
  static constexpr oat_version_t oat_version = OAT_088::oat_version;
  using oat_header   = OAT_088::oat_header;
  using oat_dex_file = OAT_088::oat_dex_file;
  using dex_file     = OAT_088::dex_file;
  using oat_quick_method_header = OAT_088::oat_quick_method_header;

  using lookup_table_entry_t = OAT_088::lookup_table_entry_t;
};

class OAT124_t {
  public:
  static constexpr oat_version_t oat_version = OAT_124::oat_version;
  using oat_header   = OAT_124::oat_header;
  using oat_dex_file = OAT_124::oat_dex_file;
  using dex_file     = OAT_124::dex_file;
  using oat_quick_method_header = OAT_124::oat_quick_method_header;

  using lookup_table_entry_t = OAT_124::lookup_table_entry_t;
};


class OAT131_t {
  public:
  static constexpr oat_version_t oat_version = OAT_131::oat_version;
  using oat_header   = OAT_131::oat_header;
  using oat_dex_file = OAT_131::oat_dex_file;
  using dex_file     = OAT_131::dex_file;
  using oat_quick_method_header = OAT_124::oat_quick_method_header;

  using lookup_table_entry_t = OAT_131::lookup_table_entry_t;
};

class OAT138_t {
  public:
  static constexpr oat_version_t oat_version = OAT_138::oat_version;
  using oat_header              = OAT_138::oat_header;
  using oat_dex_file            = OAT_138::oat_dex_file;
  using dex_file                = OAT_138::dex_file;
  using oat_quick_method_header = OAT_138::oat_quick_method_header;
  using lookup_table_entry_t    = OAT_138::lookup_table_entry_t;
};

} /* end namespace details */
} /* end namespace OAT */
} /* end namespace LIEF */
#endif


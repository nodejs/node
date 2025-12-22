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
#include "LIEF/OAT/Header.hpp"
#include "OAT/Structures.hpp"
namespace LIEF {
namespace OAT {

template<class T>
Header::Header(const T* header) :
  magic_{},
  version_{0},
  checksum_{header->adler32_checksum},
  instruction_set_{static_cast<INSTRUCTION_SETS>(header->instruction_set)},
  instruction_set_features_bitmap_{header->instruction_set_features_bitmap},
  dex_file_count_{header->dex_file_count},
  oat_dex_files_offset_{0}, // (OAT 131)
  executable_offset_{header->executable_offset},
  i2i_bridge_offset_{header->i2i_bridge_offset},
  i2c_code_bridge_offset_{header->i2c_code_bridge_offset},
  jni_dlsym_lookup_offset_{header->jni_dlsym_lookup_offset},
  quick_generic_jni_trampoline_offset_{header->quick_generic_jni_trampoline_offset},
  quick_imt_conflict_trampoline_offset_{header->quick_imt_conflict_trampoline_offset},
  quick_resolution_trampoline_offset_{header->quick_resolution_trampoline_offset},
  quick_to_interpreter_bridge_offset_{header->quick_to_interpreter_bridge_offset},
  image_patch_delta_{header->image_patch_delta},
  image_file_location_oat_checksum_{header->image_file_location_oat_checksum},
  image_file_location_oat_data_begin_{header->image_file_location_oat_data_begin},
  key_value_store_size_{header->key_value_store_size},
  dex2oat_context_{}
{

  std::copy(std::begin(header->magic), std::end(header->magic),
            std::begin(magic_)
  );
  if (std::all_of(std::begin(header->oat_version), std::end(header->oat_version) - 1,
                  ::isdigit))
  {
    version_ = static_cast<uint32_t>(
        std::stoi(
          std::string{reinterpret_cast<const char*>(header->oat_version), sizeof(header->oat_version)}));
  }

}


template<>
Header::Header(const details::OAT_131::oat_header* header) :
  magic_{},
  version_{0},
  checksum_{header->adler32_checksum},
  instruction_set_{static_cast<INSTRUCTION_SETS>(header->instruction_set)},
  instruction_set_features_bitmap_{header->instruction_set_features_bitmap},
  dex_file_count_{header->dex_file_count},
  oat_dex_files_offset_{header->oat_dex_files_offset}, // Since OAT 131 / Android 8.1.0
  executable_offset_{header->executable_offset},
  i2i_bridge_offset_{header->i2i_bridge_offset},
  i2c_code_bridge_offset_{header->i2c_code_bridge_offset},
  jni_dlsym_lookup_offset_{header->jni_dlsym_lookup_offset},
  quick_generic_jni_trampoline_offset_{header->quick_generic_jni_trampoline_offset},
  quick_imt_conflict_trampoline_offset_{header->quick_imt_conflict_trampoline_offset},
  quick_resolution_trampoline_offset_{header->quick_resolution_trampoline_offset},
  quick_to_interpreter_bridge_offset_{header->quick_to_interpreter_bridge_offset},
  image_patch_delta_{header->image_patch_delta},
  image_file_location_oat_checksum_{header->image_file_location_oat_checksum},
  image_file_location_oat_data_begin_{header->image_file_location_oat_data_begin},
  key_value_store_size_{header->key_value_store_size},
  dex2oat_context_{}
{

  std::copy(
      std::begin(header->magic),
      std::end(header->magic),
      std::begin(magic_)
  );
  if (std::all_of(
        std::begin(header->oat_version),
        std::end(header->oat_version) - 1,
        ::isdigit))
  {
    version_ = static_cast<uint32_t>(std::stoi(std::string{reinterpret_cast<const char*>(header->oat_version), sizeof(header->oat_version)}));
  }

}


} // namespace OAT
} // namespace LIEF

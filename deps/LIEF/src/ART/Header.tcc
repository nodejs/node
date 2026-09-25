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

#include "logging.hpp"
#include "LIEF/ART/Header.hpp"
#include "LIEF/ART/EnumToString.hpp"
#include "ART/Structures.hpp"

namespace LIEF {
namespace ART {

template<>
Header::Header(const details::ART_17::header* header) :
  magic_{{'a', 'r', 't', '\n'}},
  version_{0},
  image_begin_{header->image_begin},
  image_size_{header->image_size},
  oat_checksum_{header->oat_checksum},
  oat_file_begin_{header->oat_file_begin},
  oat_file_end_{header->oat_file_end},
  oat_data_begin_{header->oat_data_begin},
  oat_data_end_{header->oat_data_end},
  patch_delta_{header->patch_delta},
  image_roots_{header->image_roots},
  pointer_size_{header->pointer_size},
  compile_pic_{static_cast<bool>(header->compile_pic)},
  nb_sections_{sizeof(header->sections) / sizeof(header->sections[0])},
  nb_methods_{sizeof(header->image_methods) / sizeof(header->image_methods[0])},

  is_pic_{false},
  boot_image_begin_{0},
  boot_image_size_{0},
  boot_oat_begin_{0},
  boot_oat_size_{0},
  storage_mode_{STORAGE_MODES::STORAGE_UNCOMPRESSED},
  data_size_{0}
{
  std::copy(
      std::begin(header->magic),
      std::end(header->magic),
      std::begin(magic_)
  );
  if (std::all_of(
        header->version,
        header->version + sizeof(header->version) - 1,
        ::isdigit))
  {
    version_ = static_cast<uint32_t>(
        std::stoi(std::string{reinterpret_cast<const char*>(header->version), sizeof(header->version)}));
  }

}

template<class T>
Header::Header(const T* header) :
  magic_{{'a', 'r', 't', '\n'}},
  version_{0},
  image_begin_{header->image_begin},
  image_size_{header->image_size},
  oat_checksum_{header->oat_checksum},
  oat_file_begin_{header->oat_file_begin},
  oat_file_end_{header->oat_file_end},
  oat_data_begin_{header->oat_data_begin},
  oat_data_end_{header->oat_data_end},
  patch_delta_{header->patch_delta},
  image_roots_{header->image_roots},
  pointer_size_{header->pointer_size},
  compile_pic_{static_cast<bool>(header->compile_pic)},
  nb_sections_{sizeof(header->sections) / sizeof(header->sections[0])},
  nb_methods_{sizeof(header->image_methods) / sizeof(header->image_methods[0])},
  is_pic_{static_cast<bool>(header->is_pic)},
  boot_image_begin_{header->boot_image_begin},
  boot_image_size_{header->boot_image_size},
  boot_oat_begin_{header->boot_oat_begin},
  boot_oat_size_{header->boot_oat_size},
  storage_mode_{static_cast<STORAGE_MODES>(header->storage_mode)},
  data_size_{header->data_size}
{
  std::copy(
      std::begin(header->magic),
      std::end(header->magic),
      std::begin(magic_)
  );
  if (std::all_of(
        header->version,
        header->version + sizeof(header->version) - 1,
        ::isdigit))
  {
    version_ = static_cast<uint32_t>(
        std::stoi(std::string{reinterpret_cast<const char*>(header->version), sizeof(header->version)}));
  }

  LIEF_DEBUG("{}", to_string(storage_mode_));

}

} // namespace ART
} // namespace LIEF

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

#include "LIEF/ART/hash.hpp"
#include "LIEF/ART/File.hpp"
#include "LIEF/ART/Header.hpp"

namespace LIEF {
namespace ART {

Hash::~Hash() = default;

size_t Hash::hash(const Object& obj) {
  return LIEF::Hash::hash<LIEF::ART::Hash>(obj);
}


void Hash::visit(const File& file) {
  process(file.header());
}

void Hash::visit(const Header& header) {
  process(header.magic());
  process(header.version());
  process(header.image_begin());
  process(header.image_size());
  process(header.oat_checksum());
  process(header.oat_file_begin());
  process(header.oat_file_end());
  process(header.oat_data_begin());
  process(header.oat_data_end());
  process(header.patch_delta());
  process(header.image_roots());
  process(header.pointer_size());
  process(static_cast<size_t>(header.compile_pic()));
  process(header.nb_sections());
  process(header.nb_methods());
  process(header.boot_image_begin());
  process(header.boot_image_size());
  process(header.boot_oat_begin());
  process(header.boot_oat_size());
  process(header.storage_mode());
  process(header.data_size());
}



} // namespace ART
} // namespace LIEF


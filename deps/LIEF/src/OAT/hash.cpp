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

#include "LIEF/OAT/hash.hpp"

#include "LIEF/OAT/Binary.hpp"
#include "LIEF/OAT/Class.hpp"
#include "LIEF/OAT/DexFile.hpp"
#include "LIEF/OAT/Header.hpp"
#include "LIEF/OAT/Method.hpp"

#include "LIEF/DEX/File.hpp"
#include "LIEF/DEX/Method.hpp"
#include "LIEF/DEX/Class.hpp"
#include "LIEF/DEX/hash.hpp"

namespace LIEF {
namespace OAT {

Hash::~Hash() = default;

size_t Hash::hash(const Object& obj) {
  return LIEF::Hash::hash<LIEF::OAT::Hash>(obj);
}


void Hash::visit(const Binary& binary) {
  process(binary.header());

  process(std::begin(binary.oat_dex_files()), std::end(binary.oat_dex_files()));
  process(std::begin(binary.classes()), std::end(binary.classes()));
  process(std::begin(binary.methods()), std::end(binary.methods()));
}


void Hash::visit(const Header& header) {
  process(header.magic());
  process(header.version());
  process(header.checksum());
  process(header.instruction_set());
  process(header.nb_dex_files());
  process(header.oat_dex_files_offset());
  process(header.executable_offset());
  process(header.i2i_bridge_offset());
  process(header.i2c_code_bridge_offset());
  process(header.jni_dlsym_lookup_offset());
  process(header.quick_generic_jni_trampoline_offset());
  process(header.quick_imt_conflict_trampoline_offset());
  process(header.quick_resolution_trampoline_offset());
  process(header.quick_to_interpreter_bridge_offset());
  process(header.image_patch_delta());
  process(header.image_file_location_oat_checksum());
  process(header.image_file_location_oat_data_begin());
  process(header.key_value_size());

  process(std::begin(header.keys()), std::end(header.keys()));
  process(std::begin(header.values()), std::end(header.values()));
}


void Hash::visit(const DexFile& dex_file) {
  process(dex_file.location());
  process(dex_file.checksum());
  process(dex_file.dex_offset());
  if (dex_file.has_dex_file()) {
    process(DEX::Hash::hash(*dex_file.dex_file()));
  }
  process(dex_file.lookup_table_offset());
  process(dex_file.classes_offsets());
}


void Hash::visit(const Class& cls) {
  if (cls.has_dex_class()) {
    process(DEX::Hash::hash(*cls.dex_class()));
  }

  process(cls.status());
  process(cls.type());
  process(cls.fullname());
  process(cls.bitmap());
  Class::it_const_methods it = cls.methods();
  process(std::begin(it), std::end(it));
}


void Hash::visit(const Method& meth) {
  if (meth.has_dex_method()) {
    process(DEX::Hash::hash(*meth.dex_method()));
  }
  process(static_cast<size_t>(meth.is_dex2dex_optimized()));
  process(static_cast<size_t>(meth.is_compiled()));
  process(meth.quick_code());
}



} // namespace OAT
} // namespace LIEF


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

#include "LIEF/VDEX/hash.hpp"
#include "LIEF/VDEX/File.hpp"
#include "LIEF/DEX/hash.hpp"
#include "LIEF/DEX/File.hpp"

namespace LIEF {
namespace VDEX {

Hash::~Hash() = default;

size_t Hash::hash(const Object& obj) {
  return LIEF::Hash::hash<LIEF::VDEX::Hash>(obj);
}


void Hash::visit(const File& file) {
  process(file.header());
  for (const DEX::File& dexfile : file.dex_files()) {
    process(DEX::Hash::hash(dexfile));
  }
}

void Hash::visit(const Header& header) {
  process(header.magic());
  process(header.version());
  process(header.nb_dex_files());
  process(header.dex_size());
  process(header.verifier_deps_size());
  process(header.quickening_info_size());
}



} // namespace VDEX
} // namespace LIEF


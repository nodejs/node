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

#include "VDEX/json_internal.hpp"
#include "DEX/json_internal.hpp"

#include "LIEF/VDEX.hpp"
#include "LIEF/DEX/File.hpp"

namespace LIEF {
namespace VDEX {

void JsonVisitor::visit(const File& file) {
  JsonVisitor vheader;
  vheader(file.header());

  std::vector<json> dexfiles;
  for (const DEX::File& dexfile : file.dex_files()) {
    dexfiles.emplace_back(DEX::to_json_obj(dexfile));
  }

  node_["header"]    = vheader.get();
  node_["dex_files"] = dexfiles;
}

void JsonVisitor::visit(const Header& header) {
  node_["magic"]                = header.magic();
  node_["version"]              = header.version();
  node_["nb_dex_files"]         = header.nb_dex_files();
  node_["dex_size"]             = header.dex_size();
  node_["verifier_deps_size"]   = header.verifier_deps_size();
  node_["quickening_info_size"] = header.quickening_info_size();
}

} // namespace VDEX
} // namespace LIEF

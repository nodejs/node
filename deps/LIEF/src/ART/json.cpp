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


#include "ART/json_internal.hpp"
#include "LIEF/ART.hpp"

namespace LIEF {
namespace ART {

void JsonVisitor::visit(const File& file) {
  JsonVisitor header_visitor;
  header_visitor(file.header());
  node_["header"]                      = header_visitor.get();
}

void JsonVisitor::visit(const Header& header) {
  node_["magic"]            = header.magic();
  node_["version"]          = header.version();
  node_["image_begin"]      = header.image_begin();
  node_["image_size"]       = header.image_size();
  node_["oat_checksum"]     = header.oat_checksum();
  node_["oat_file_begin"]   = header.oat_file_begin();
  node_["oat_file_end"]     = header.oat_file_end();
  node_["oat_data_begin"]   = header.oat_data_begin();
  node_["oat_data_end"]     = header.oat_data_end();
  node_["patch_delta"]      = header.patch_delta();
  node_["image_roots"]      = header.image_roots();
  node_["pointer_size"]     = header.pointer_size();
  node_["compile_pic"]      = header.compile_pic();
  node_["nb_sections"]      = header.nb_sections();
  node_["nb_methods"]       = header.nb_methods();
  node_["boot_image_begin"] = header.boot_image_begin();
  node_["boot_image_size"]  = header.boot_image_size();
  node_["boot_oat_begin"]   = header.boot_oat_begin();
  node_["boot_oat_size"]    = header.boot_oat_size();
  node_["storage_mode"]     = to_string(header.storage_mode());
  node_["data_size"]        = header.data_size();
}

} // namespace ART
} // namespace LIEF


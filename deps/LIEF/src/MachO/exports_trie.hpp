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
#ifndef LIEF_MACHO_EXPORTS_TRIE_H
#define LIEF_MACHO_EXPORTS_TRIE_H
#include <ostream>
#include <vector>
#include <string>
#include <memory>

namespace LIEF {
class BinaryStream;
namespace MachO {
class ExportInfo;
using exports_list_t = std::vector<std::unique_ptr<ExportInfo>>;
void show_trie(std::ostream& output, std::string output_prefix,
               BinaryStream& stream, uint64_t start, uint64_t end, const std::string& prefix);

std::vector<uint8_t> create_trie(const exports_list_t& exports, size_t pointer_size);
}
}
#endif

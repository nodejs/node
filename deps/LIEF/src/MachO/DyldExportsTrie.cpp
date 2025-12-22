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
#include "spdlog/fmt/fmt.h"
#include <sstream>

#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/Visitor.hpp"
#include "LIEF/MachO/DyldExportsTrie.hpp"
#include "LIEF/MachO/ExportInfo.hpp"

#include "MachO/Structures.hpp"
#include "MachO/exports_trie.hpp"

namespace LIEF {
namespace MachO {

DyldExportsTrie::DyldExportsTrie() = default;
DyldExportsTrie::~DyldExportsTrie() = default;
DyldExportsTrie::DyldExportsTrie(const DyldExportsTrie& other) :
  LoadCommand::LoadCommand(other),
  data_offset_{other.data_offset_},
  data_size_{other.data_size_}
{
  /* Do not copy export info */
}

DyldExportsTrie& DyldExportsTrie::operator=(DyldExportsTrie other) {
  swap(other);
  return *this;
}

DyldExportsTrie::DyldExportsTrie(const details::linkedit_data_command& cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize},
  data_offset_{cmd.dataoff},
  data_size_{cmd.datasize}
{}


void DyldExportsTrie::swap(DyldExportsTrie& other) noexcept {
  LoadCommand::swap(other);
  std::swap(data_offset_, other.data_offset_);
  std::swap(data_size_,   other.data_size_);
  std::swap(content_,     other.content_);
  std::swap(export_info_, other.export_info_);
}

void DyldExportsTrie::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

void DyldExportsTrie::add(std::unique_ptr<ExportInfo> info) {
  export_info_.push_back(std::move(info));
}

std::string DyldExportsTrie::show_export_trie() const {
  std::ostringstream output;

  SpanStream stream = content_;
  show_trie(output, "", stream, 0, content_.size(), "");

  return output.str();
}

std::ostream& DyldExportsTrie::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("offset=0x{:06x}, size=0x{:06x}",
                     data_offset(), data_size());
  return os;
}

}
}

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
#include "LIEF/Visitor.hpp"

#include "LIEF/MachO/FilesetCommand.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

FilesetCommand::FilesetCommand(const details::fileset_entry_command& cmd) :
  LoadCommand{LoadCommand::TYPE::FILESET_ENTRY, cmd.cmdsize},
  virtual_address_{cmd.vmaddr},
  file_offset_{cmd.fileoff}
{}

FilesetCommand& FilesetCommand::operator=(FilesetCommand other) {
  swap(other);
  return *this;
}

FilesetCommand::FilesetCommand(const FilesetCommand& other) :
  LoadCommand{other},
  name_{other.name_},
  virtual_address_{other.virtual_address_},
  file_offset_{other.file_offset_}
{}

void FilesetCommand::swap(FilesetCommand& other) noexcept {
  LoadCommand::swap(other);

  std::swap(virtual_address_, other.virtual_address_);
  std::swap(file_offset_,     other.file_offset_);
}

std::ostream& FilesetCommand::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("name={}, va=0x{:06x}, offset=0x{:x}",
                    name(), virtual_address(), file_offset());
  return os;
}


}
}

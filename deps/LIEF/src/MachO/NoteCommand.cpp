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
#include <spdlog/fmt/fmt.h>
#include "LIEF/Visitor.hpp"

#include "LIEF/MachO/NoteCommand.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

NoteCommand::NoteCommand(const details::note_command& cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize},
  owner_(), note_offset_(cmd.offset), note_size_(cmd.size)
{
  static_assert(sizeof(owner_) == sizeof(cmd.data_owner));
  std::copy(std::begin(cmd.data_owner), std::end(cmd.data_owner),
            std::begin(owner_));
}

void NoteCommand::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& NoteCommand::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("owner={} offset=0x{:x}, size=0x{:x}",
                    owner_str(), note_offset(), note_size());
  return os;
}

}
}

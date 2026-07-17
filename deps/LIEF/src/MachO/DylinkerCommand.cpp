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
#include <iomanip>
#include "LIEF/utils.hpp"
#include "LIEF/MachO/hash.hpp"

#include "LIEF/MachO/DylinkerCommand.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

DylinkerCommand::DylinkerCommand(const details::dylinker_command& cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize}
{}

DylinkerCommand::DylinkerCommand(std::string name) :
  LoadCommand::LoadCommand{LoadCommand::TYPE::LOAD_DYLINKER,
                           static_cast<uint32_t>(align(sizeof(details::dylinker_command) + name.size() + 1, sizeof(uint64_t)))},
  name_{std::move(name)}
{
  this->data(LoadCommand::raw_t(size(), 0));
}

void DylinkerCommand::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& DylinkerCommand::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << name();
  return os;
}

}
}

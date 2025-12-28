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

#include "LIEF/MachO/MainCommand.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

MainCommand::MainCommand(const details::entry_point_command& cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize},
  entrypoint_{cmd.entryoff},
  stack_size_{cmd.stacksize}
{}

MainCommand::MainCommand(uint64_t entrypoint, uint64_t stacksize) :
  LoadCommand::LoadCommand{LoadCommand::TYPE::MAIN, sizeof(details::entry_point_command)},
  entrypoint_{entrypoint},
  stack_size_{stacksize}
{
  this->data(LoadCommand::raw_t(size(), 0));
}

void MainCommand::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& MainCommand::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("entrypoint=0x{:x}, stack size=0x{:x}",
                    entrypoint(), stack_size());
  return os;
}

}
}

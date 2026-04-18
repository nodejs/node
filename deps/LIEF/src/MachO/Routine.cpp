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

#include "LIEF/Visitor.hpp"

#include "LIEF/MachO/Routine.hpp"
#include "MachO/Structures.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF {
namespace MachO {

template<class T>
LIEF_LOCAL Routine::Routine(const T& cmd) :
  LoadCommand::LoadCommand(LoadCommand::TYPE(cmd.cmd), cmd.cmdsize),
  init_address_(cmd.init_address),
  init_module_(cmd.init_module),
  reserved1_(cmd.reserved1),
  reserved2_(cmd.reserved2),
  reserved3_(cmd.reserved3),
  reserved4_(cmd.reserved4),
  reserved5_(cmd.reserved5),
  reserved6_(cmd.reserved6)
{}

void Routine::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& Routine::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("init_address=0x{:06x} init_module=0x{:06x}",
                    init_address(), init_module());
  return os;
}

template Routine::Routine(const details::routines_command_32&);
template Routine::Routine(const details::routines_command_64&);

}
}

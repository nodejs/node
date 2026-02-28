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

#include "LIEF/MachO/UUIDCommand.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {


UUIDCommand::UUIDCommand(const details::uuid_command& uuid) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(uuid.cmd), uuid.cmdsize}
{
  std::copy(std::begin(uuid.uuid), std::end(uuid.uuid), std::begin(uuid_));
}

void UUIDCommand::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& UUIDCommand::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  for (uint32_t x : uuid()) {
    os << fmt::format("{:02x}", x) << ' ';
  }
  os << ' ';
  return os;
}


}
}

/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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

#include "LIEF/MachO/FunctionStarts.hpp"
#include "MachO/Structures.hpp"


namespace LIEF::MachO {

FunctionStarts::FunctionStarts(const details::linkedit_data_command& cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize},
  data_offset_{cmd.dataoff},
  data_size_{cmd.datasize} {}

void FunctionStarts::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& FunctionStarts::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  const std::vector<uint64_t> funcs = functions();
  os << fmt::format("offset={:#08x}, size={:#08x}, #functions={}", data_offset(),
                    data_size(), funcs.size())
     << '\n';
  for (size_t i = 0; i < funcs.size(); ++i) {
    os << fmt::format("  [{}] __TEXT + {:#08x}\n", i, funcs[i]);
  }
  return os;
}


}

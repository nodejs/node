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

#include "LIEF/MachO/SymbolCommand.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

SymbolCommand::SymbolCommand(const details::symtab_command& cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize},
  symbols_offset_{cmd.symoff},
  nb_symbols_{cmd.nsyms},
  strings_offset_{cmd.stroff},
  strings_size_{cmd.strsize}
{}

void SymbolCommand::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& SymbolCommand::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("symbol offset=0x{:06x}, nb symbols={}",
                     symbol_offset(), numberof_symbols()) << '\n'
     << fmt::format("string offset=0x{:06x}, string size={}",
                     strings_offset(), strings_size());
  return os;
}


}
}

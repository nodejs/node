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

#include "LIEF/MachO/DynamicSymbolCommand.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

DynamicSymbolCommand::DynamicSymbolCommand() :
  LoadCommand::LoadCommand{LoadCommand::TYPE::DYSYMTAB,
                           sizeof(details::dysymtab_command)}
{}

DynamicSymbolCommand::DynamicSymbolCommand(const details::dysymtab_command& cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize},

  idx_local_symbol_{cmd.ilocalsym},
  nb_local_symbols_{cmd.nlocalsym},

  idx_external_define_symbol_{cmd.iextdefsym},
  nb_external_define_symbols_{cmd.nextdefsym},

  idx_undefined_symbol_{cmd.iundefsym},
  nb_undefined_symbols_{cmd.nundefsym},

  toc_offset_{cmd.tocoff},
  nb_toc_{cmd.ntoc},

  module_table_offset_{cmd.modtaboff},
  nb_module_table_{cmd.nmodtab},

  external_reference_symbol_offset_{cmd.extrefsymoff},
  nb_external_reference_symbols_{cmd.nextrefsyms},

  indirect_sym_offset_{cmd.indirectsymoff},
  nb_indirect_symbols_{cmd.nindirectsyms},

  external_relocation_offset_{cmd.extreloff},
  nb_external_relocations_{cmd.nextrel},

  local_relocation_offset_{cmd.locreloff},
  nb_local_relocations_{cmd.nlocrel}
{}


void DynamicSymbolCommand::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& DynamicSymbolCommand::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  static constexpr auto DEFAULT_FMT = "{:36}: 0x{:x}\n";
  os << fmt::format(DEFAULT_FMT, "First local symbol index", idx_local_symbol());
  os << fmt::format(DEFAULT_FMT, "Number of local symbols", nb_local_symbols());
  os << fmt::format(DEFAULT_FMT, "External symbol index", idx_external_define_symbol());
  os << fmt::format(DEFAULT_FMT, "Number of external symbols", nb_external_define_symbols());
  os << fmt::format(DEFAULT_FMT, "Undefined symbol index", idx_undefined_symbol());
  os << fmt::format(DEFAULT_FMT, "Number of undefined symbols", nb_undefined_symbols());
  os << fmt::format(DEFAULT_FMT, "Table of content offset", toc_offset());
  os << fmt::format(DEFAULT_FMT, "Number of entries in TOC", nb_toc());
  os << fmt::format(DEFAULT_FMT, "Module table offset", module_table_offset());
  os << fmt::format(DEFAULT_FMT, "Number of entries in module table", nb_module_table());
  os << fmt::format(DEFAULT_FMT, "External reference table offset", external_reference_symbol_offset());
  os << fmt::format(DEFAULT_FMT, "Number of external reference", nb_external_reference_symbols());
  os << fmt::format(DEFAULT_FMT, "Indirect symbols offset", indirect_symbol_offset());
  os << fmt::format(DEFAULT_FMT, "Number of indirect symbols", nb_indirect_symbols());
  os << fmt::format(DEFAULT_FMT, "External relocation offset", external_relocation_offset());
  os << fmt::format(DEFAULT_FMT, "Number of external relocations", nb_external_relocations());
  os << fmt::format(DEFAULT_FMT, "Local relocation offset", local_relocation_offset());
  os << fmt::format(DEFAULT_FMT, "Number of local relocations", nb_local_relocations());

  return os;
}

}
}

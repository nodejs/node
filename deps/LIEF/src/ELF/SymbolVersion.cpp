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
#include "LIEF/ELF/hash.hpp"

#include "LIEF/ELF/SymbolVersion.hpp"
#include "LIEF/ELF/SymbolVersionAux.hpp"
#include "LIEF/ELF/SymbolVersionAuxRequirement.hpp"

namespace LIEF {
namespace ELF {

void SymbolVersion::symbol_version_auxiliary(SymbolVersionAuxRequirement& svauxr) {
  symbol_aux_ = &svauxr;
  value_      = svauxr.other();
}

void SymbolVersion::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const ELF::SymbolVersion& symv) {
  if (symv.has_auxiliary_version()) {
    os << symv.symbol_version_auxiliary()->name() << "(" << symv.value() << ")";
  } else {
    std::string type;
    if (symv.value() == 0) {
      type = "* Local *";
    } else if (symv.value() == 1){
      type = "* Global *";
    } else {
      type = "* ERROR (" + std::to_string(symv.value()) + ") *";
    }
    os << type;
  }

  return os;
}
}
}

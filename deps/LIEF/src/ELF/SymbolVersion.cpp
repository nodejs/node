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
#include <spdlog/fmt/fmt.h>

#include "LIEF/ELF/hash.hpp"

#include "LIEF/ELF/SymbolVersion.hpp"
#include "LIEF/ELF/SymbolVersionAux.hpp"
#include "LIEF/ELF/SymbolVersionAuxRequirement.hpp"


namespace LIEF::ELF {

void SymbolVersion::symbol_version_auxiliary(SymbolVersionAuxRequirement& svauxr) {
  symbol_aux_ = &svauxr;
  value_ = svauxr.other();
}

void SymbolVersion::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const ELF::SymbolVersion& symv) {
  if (symv.has_auxiliary_version()) {
    os << fmt::format("{}({})", symv.symbol_version_auxiliary()->name(),
                      symv.value());
  } else if (symv.value() == 0) {
    os << "* Local *";
  } else if (symv.value() == 1) {
    os << "* Global *";
  } else {
    os << fmt::format("* ERROR ({}) *", symv.value());
  }

  return os;
}
}

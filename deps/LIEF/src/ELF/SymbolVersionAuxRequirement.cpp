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

#include "LIEF/ELF/SymbolVersionAuxRequirement.hpp"
#include "ELF/Structures.hpp"

namespace LIEF {
namespace ELF {

SymbolVersionAuxRequirement::SymbolVersionAuxRequirement(const details::Elf64_Vernaux& header) :
  hash_{header.vna_hash},
  flags_{header.vna_flags},
  other_{header.vna_other}
{}


SymbolVersionAuxRequirement::SymbolVersionAuxRequirement(const details::Elf32_Vernaux& header) :
  hash_{header.vna_hash},
  flags_{header.vna_flags},
  other_{header.vna_other}
{}

void SymbolVersionAuxRequirement::accept(Visitor& visitor) const {
  visitor.visit(*this);
}


}
}

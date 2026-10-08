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
#ifndef LIEF_RUNTIME_DISASSEMBLER_H
#define LIEF_RUNTIME_DISASSEMBLER_H

#include "LIEF/asm/Instruction.hpp"

namespace LIEF {
namespace runtime {

using instructions_it = iterator_range<assembly::Instruction::Iterator>;

/// Start disassembling instructions at the given **absolute** virtual address
///
/// ```cpp
/// for (const auto& inst : disassemble(0x7f0011223344)) {
///   std::cout << inst.to_string() << '\n';
/// }
/// ```
LIEF_API instructions_it disassemble(uintptr_t addr);

}
}
#endif

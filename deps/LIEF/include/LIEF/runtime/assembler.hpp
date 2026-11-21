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
#ifndef LIEF_RUNTIME_ASSEMBLER_H
#define LIEF_RUNTIME_ASSEMBLER_H
#include <LIEF/visibility.h>
#include <vector>

#include "LIEF/asm/AssemblerConfig.hpp"

namespace llvm {
class MCInst;
}

namespace LIEF {
namespace runtime {

/// Assemble the provided assembly code at the specified (absolute) virtual
/// address.
///
/// The function returns the generated assembly bytes.
///
/// ```cpp
/// #include <LIEF/runtime.hpp>
///
/// auto code = LIEF::runtime::assemble(0x7f0011223344, R"(
///   xor rax, rbx;
///   mov rcx, rax;
/// )");
/// ```
///
/// If you need to configure the assembly engine or to define addresses for
/// symbols, you can provide your own assembly::AssemblerConfig instance.
LIEF_API std::vector<uint8_t> assemble(
    uint64_t addr, const std::string& Asm,
    assembly::AssemblerConfig& config = assembly::AssemblerConfig::default_config()
);

/// Assemble the provided LLVM MCInst instruction at the specified (absolute)
/// virtual address and return the generated assembly bytes.
LIEF_API std::vector<uint8_t> assemble(uint64_t address, const llvm::MCInst& inst);

/// Assemble the provided sequence of LLVM MCInst instructions at the specified
/// (absolute) virtual address and return the generated assembly bytes.
LIEF_API std::vector<uint8_t> assemble(uint64_t address,
                                       const std::vector<llvm::MCInst>& insts);

}
}
#endif

/* Copyright 2022 - 2025 R. Thomas
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
#ifndef LIEF_ASM_X86_OPERAND_MEMORY_H
#define LIEF_ASM_X86_OPERAND_MEMORY_H
#include "LIEF/asm/x86/Operand.hpp"
#include "LIEF/asm/x86/registers.hpp"

namespace LIEF {
namespace assembly {
namespace x86 {
namespace operands {

/// This class represents a memory operand.
///
/// For instance:
///
/// ```text
/// movq xmm3, qword ptr [rip + 823864];
///
///                      |
///                      |
///                    Memory
///                      |
///          +-----------+-----------+
///          |           |           |
///      Base: rip    Scale: 1    Displacement: 823864
///
/// ```
class LIEF_API Memory : public Operand {
  public:
  using Operand::Operand;

  /// The base register.
  ///
  /// For `lea rdx, [rip + 244634]` it would return `rip`
  REG base() const;

  /// The scaled register.
  ///
  /// For `mov rdi, qword ptr [r13 + 8*r14]` it would return `r14`
  REG scaled_register() const;

  /// The segment register associated with the memory operation.
  ///
  /// For `mov eax, dword ptr gs:[0]` is would return `gs`
  REG segment_register() const;

  /// The scale value associated with the scaled_register():
  ///
  /// For `mov rdi, qword ptr [r13 + 8*r14]` it would return `8`
  uint64_t scale() const;

  /// The displacement value
  ///
  /// For `call qword ptr [rip + 248779]` it would return `248779`
  int64_t displacement() const;

  static bool classof(const Operand* op);
  ~Memory() override = default;
};
}
}
}
}
#endif

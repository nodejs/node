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
#ifndef LIEF_ASM_X86_OPERAND_REG_H
#define LIEF_ASM_X86_OPERAND_REG_H

#include "LIEF/asm/x86/Operand.hpp"
#include "LIEF/asm/x86/registers.hpp"

namespace LIEF {
namespace assembly {
namespace x86 {
namespace operands {

/// This class represents a register operand.
///
/// For instance:
///
/// ```text
/// mov r15d, edi
///      |     |
///      |     +---------> Register(EDI)
///      |
///      +---------------> Register(R15D)
/// ```
class LIEF_API Register : public Operand {
  public:
  using Operand::Operand;

  /// The effective REG wrapped by this operand
  REG value() const;

  static bool classof(const Operand* op);
  ~Register() override = default;
};
}
}
}
}
#endif

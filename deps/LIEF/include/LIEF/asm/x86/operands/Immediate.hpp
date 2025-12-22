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
#ifndef LIEF_ASM_X86_OPERAND_IMM_H
#define LIEF_ASM_X86_OPERAND_IMM_H
#include "LIEF/asm/x86/Operand.hpp"

namespace LIEF {
namespace assembly {
namespace x86 {
/// Namespace that wraps the different x86/x86-64 operands
namespace operands {


/// This class represents an immediate operand (i.e. a constant)
///
/// For instance:
///
/// ```text
/// mov edi, 1;
///          |
///          +---> Immediate(1)
/// ```
class LIEF_API Immediate : public Operand {
  public:
  using Operand::Operand;

  /// The constant value wrapped by this operand
  int64_t value() const;

  static bool classof(const Operand* op);
  ~Immediate() override = default;
};
}
}
}
}
#endif

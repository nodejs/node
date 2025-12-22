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
#ifndef LIEF_ASM_AARCH64_OPERAND_PCREL_H
#define LIEF_ASM_AARCH64_OPERAND_PCREL_H
#include "LIEF/asm/aarch64/Operand.hpp"

namespace LIEF {
namespace assembly {
namespace aarch64 {
namespace operands {

/// This class represents a PC-relative operand.
///
/// ```text
/// ldr x0, #8
///         |
///         v
///  PC Relative operand
/// ```
class LIEF_API PCRelative : public Operand {
  public:
  using Operand::Operand;

  /// The effective value that is relative to the current `pc` register
  int64_t value() const;

  static bool classof(const Operand* op);

  ~PCRelative() override = default;
};
}
}
}
}
#endif

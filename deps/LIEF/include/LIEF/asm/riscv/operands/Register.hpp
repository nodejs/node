/* Copyright 2022 - 2026 R. Thomas
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
#ifndef LIEF_ASM_RISCV_OPERAND_REG_H
#define LIEF_ASM_RISCV_OPERAND_REG_H

#include "LIEF/asm/riscv/Operand.hpp"
#include "LIEF/asm/riscv/registers.hpp"

namespace LIEF {
namespace assembly {
namespace riscv {
namespace operands {

/// This class represents a register operand.
///
/// RISC-V exposes two kinds of registers: regular registers (GPR, FPR,
/// vector, ...) and control and status registers (CSR / system registers).
///
/// ```text
/// csrr    a0, mstatus
///         |   |
///  +------+   +-------+
///  |                  |
///  v                  v
///  REG              SYSREG
/// ```
class LIEF_API Register : public Operand {
  public:
  using Operand::Operand;

  struct reg_t {
    /// Enum type used to discriminate the anonymous union
    enum class TYPE {
      NONE = 0,
      /// The union holds a sysreg attribute
      SYSREG,
      /// The union holds the reg attribute
      REG,
    };

    union {
      REG reg = REG::NoRegister;
      SYSREG sysreg;
    };
    TYPE type = TYPE::NONE;
  };

  /// The effective register as either: a REG or a SYSREG
  reg_t value() const;

  static bool classof(const Operand* op);
  ~Register() override = default;
};
}
}
}
}
#endif

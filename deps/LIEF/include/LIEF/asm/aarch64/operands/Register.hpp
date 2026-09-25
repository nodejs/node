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
#ifndef LIEF_ASM_AARCH64_OPERAND_REG_H
#define LIEF_ASM_AARCH64_OPERAND_REG_H

#include "LIEF/asm/aarch64/Operand.hpp"
#include "LIEF/asm/aarch64/registers.hpp"

namespace LIEF {
namespace assembly {
namespace aarch64 {
namespace operands {

/// This class represents a register operand.
///
/// ```text
/// mrs     x0, TPIDR_EL0
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

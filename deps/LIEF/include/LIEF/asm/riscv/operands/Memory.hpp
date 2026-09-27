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
#ifndef LIEF_ASM_RISCV_OPERAND_MEMORY_H
#define LIEF_ASM_RISCV_OPERAND_MEMORY_H
#include <cstdint>

#include "LIEF/asm/riscv/Operand.hpp"
#include "LIEF/asm/riscv/registers.hpp"

namespace LIEF {
namespace assembly {
namespace riscv {
namespace operands {

/// This class represents a memory operand.
///
/// ```text
/// lw   a0, 8(sp)
///          |  |
///          |  +----> Base: sp
///          |
///          +-------> Displacement: 8
/// ```
class LIEF_API Memory : public Operand {
  public:
  using Operand::Operand;

  /// The base register.
  ///
  /// For `lw a0, 8(sp)` it would return `sp`.
  REG base() const;

  /// The displacement value.
  ///
  /// For `lw a0, 8(sp)` it would return `8`.
  int64_t displacement() const;

  static bool classof(const Operand* op);
  ~Memory() override = default;
};
}
}
}
}
#endif

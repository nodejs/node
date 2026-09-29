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
#ifndef LIEF_ASM_MIPS_OPERAND_MEMORY_H
#define LIEF_ASM_MIPS_OPERAND_MEMORY_H
#include "LIEF/asm/mips/Operand.hpp"
#include "LIEF/asm/mips/registers.hpp"

namespace LIEF {
namespace assembly {
namespace mips {
namespace operands {

/// This class represents a memory operand.
///
/// MIPS has two addressing forms:
///
/// ```text
/// lw    $4, 8($5)            ldxc1  $f2, $4($7)
///        |  | |                      |   |  |
/// +------+  | +---+          +-------+   |  +-----+
/// |         |     |          |           |        |
/// v         v     v          v           v        v
/// Reg      Disp  Base       Reg         Index    Base
/// ```
class LIEF_API Memory : public Operand {
  public:
  using Operand::Operand;

  /// Wraps the memory offset as either an integer displacement or an index
  /// register.
  struct offset_t {
    /// Enum type used to discriminate the anonymous union
    enum class TYPE {
      NONE = 0,
      /// The *union* holds the REG attribute
      REG,
      /// The *union* holds the `displacement` attribute (`int64_t`)
      DISP,
    };

    union {
      /// Register offset (index register)
      REG reg;

      /// Integer offset
      int64_t displacement = 0;
    };
    TYPE type = TYPE::NONE;
  };

  /// The base register.
  ///
  /// For `lw $4, 8($5)` it would return `$5`.
  REG base() const;

  /// The addressing offset.
  ///
  /// It can be either:
  /// - A register (e.g. `ldxc1 $f2, $4($7)`)
  /// - A displacement (e.g. `lw $4, 8($5)`)
  offset_t offset() const;

  static bool classof(const Operand* op);
  ~Memory() override = default;
};
}
}
}
}
#endif

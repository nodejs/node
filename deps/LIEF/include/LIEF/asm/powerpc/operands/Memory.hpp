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
#ifndef LIEF_ASM_POWERPC_OPERAND_MEMORY_H
#define LIEF_ASM_POWERPC_OPERAND_MEMORY_H
#include "LIEF/asm/powerpc/Operand.hpp"
#include "LIEF/asm/powerpc/registers.hpp"

namespace LIEF {
namespace assembly {
namespace powerpc {
namespace operands {

/// This class represents a memory operand.
///
/// PowerPC has two addressing forms:
///
/// ```text
/// lwz   3, 8(4)              lwzx   3, 4, 5
///        |  |                       |  |  |
/// +------+  +---+            +------+   |  +---+
/// |             |           |          |      |
/// v             v           v          v      v
/// Disp         Base        Reg        Base   Index
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
  /// For `lwz 3, 8(4)` it would return `4`.
  REG base() const;

  /// The addressing offset.
  ///
  /// It can be either:
  /// - An index register (e.g. `lwzx 3, 4, 5`)
  /// - A displacement (e.g. `lwz 3, 8(4)`)
  offset_t offset() const;

  /// @private
  /// Whether the addressing offset is an index register (`true`) rather than
  /// a constant displacement (`false`). This is redundant with offset() (which
  /// already discriminates the two) and is kept as an internal helper.
  LIEF_LOCAL bool has_index() const;

  static bool classof(const Operand* op);
  ~Memory() override = default;
};
}
}
}
}
#endif

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
#ifndef LIEF_ASM_AARCH64_OPERAND_MEMORY_H
#define LIEF_ASM_AARCH64_OPERAND_MEMORY_H
#include "LIEF/asm/aarch64/Operand.hpp"
#include "LIEF/asm/aarch64/registers.hpp"

namespace LIEF {
namespace assembly {
namespace aarch64 {
namespace operands {

/// This class represents a memory operand.
///
/// ```text
/// ldr     x0, [x1, x2, lsl #3]
///              |   |    |
/// +------------+   |    +--------+
/// |                |             |
/// v                v             v
/// Base            Reg Offset    Shift
///
/// ```
class LIEF_API Memory : public Operand {
  public:
  using Operand::Operand;

  enum class SHIFT : int32_t {
    UNKNOWN = 0,
    LSL,
    UXTX,
    UXTW,
    SXTX,
    SXTW,
  };

  /// This structure holds shift info (type + value)
  struct shift_info_t {
    SHIFT type = SHIFT::UNKNOWN;
    int8_t value = -1;
  };

  /// Wraps a memory offset as an integer offset
  /// or as a register offset
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
      /// Register offset
      REG reg;

      /// Integer offset
      int64_t displacement = 0;
    };
    TYPE type = TYPE::NONE;
  };

  /// The base register.
  ///
  /// For `str x3, [x8, #8]` it would return `x8`.
  REG base() const;

  /// The addressing offset.
  ///
  /// It can be either:
  /// - A register (e.g. `ldr x0, [x1, x3]`)
  /// - An offset (e.g. `ldr x0, [x1, #8]`)
  offset_t offset() const;

  /// Shift information.
  ///
  /// For instance, for `ldr x1, [x2, x3, lsl #3]` it would
  /// return a SHIFT::LSL with a shift_info_t::value set to `3`.
  shift_info_t shift() const;

  static bool classof(const Operand* op);

  ~Memory() override = default;
};
}
}
}
}
#endif

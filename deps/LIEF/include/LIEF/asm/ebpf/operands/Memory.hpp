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
#ifndef LIEF_ASM_EBPF_OPERAND_MEMORY_H
#define LIEF_ASM_EBPF_OPERAND_MEMORY_H
#include <cstdint>

#include "LIEF/asm/ebpf/Operand.hpp"
#include "LIEF/asm/ebpf/registers.hpp"

namespace LIEF {
namespace assembly {
namespace ebpf {
namespace operands {

/// This class represents a memory operand.
///
/// ```text
/// *(u64 *)(r1 + 8) = r2
///           |    |
///           |    +-----> Displacement: 8
///           |
///           +----------> Base: r1
/// ```
class LIEF_API Memory : public Operand {
  public:
  using Operand::Operand;

  /// The base register.
  ///
  /// For `*(u64 *)(r1 + 8)` it would return `r1`.
  REG base() const;

  /// The displacement value.
  ///
  /// For `*(u64 *)(r1 + 8)` it would return `8`.
  int64_t displacement() const;

  static bool classof(const Operand* op);
  ~Memory() override = default;
};
}
}
}
}
#endif

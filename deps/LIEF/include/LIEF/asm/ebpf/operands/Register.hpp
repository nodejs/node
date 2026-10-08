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
#ifndef LIEF_ASM_EBPF_OPERAND_REG_H
#define LIEF_ASM_EBPF_OPERAND_REG_H

#include "LIEF/asm/ebpf/Operand.hpp"
#include "LIEF/asm/ebpf/registers.hpp"

namespace LIEF {
namespace assembly {
namespace ebpf {
namespace operands {

/// This class represents a register operand.
///
/// For instance:
///
/// ```text
/// r0 = r1
///  |    |
///  |    +---------> Register(r1)
///  |
///  +--------------> Register(r0)
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

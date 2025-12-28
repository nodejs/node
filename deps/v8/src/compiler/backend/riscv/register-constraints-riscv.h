// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_RISCV_REGISTER_CONSTRAINTS_RISCV_H_
#define V8_COMPILER_BACKEND_RISCV_REGISTER_CONSTRAINTS_RISCV_H_

namespace v8 {
namespace internal {
namespace compiler {

enum class RiscvRegisterConstraint {
  kNone = 0,
  // Destination and source operands of vector operations are not allowed to
  // overlap.
  kNoDestinationSourceOverlap,
  // Input 0 and 1 are a register group. That is, input 0 is in an even-numbered
  // register and input 1 is in the next odd-numbered register.
  kRegisterGroup,
  // Input 2 does not overlap with any temp or output register.
  kNoInput2Overlap,
  // Same as kRegisterGroup, but the group and the destination register must not
  // overlap.
  // In theory we could split this entry into two:
  // 1. a narrowing operation, where the destination would be allowed to be the
  //   same as input 0, but not input 1.
  // 2. a widening operation, where the destination would be allowed to be the
  //   same as input 1, but not input 0.
  kRegisterGroupNoOverlap,
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_RISCV_REGISTER_CONSTRAINTS_RISCV_H_

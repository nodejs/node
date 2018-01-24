// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_MIPS_H_
#define V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_MIPS_H_

#include "src/wasm/baseline/liftoff-assembler.h"

namespace v8 {
namespace internal {
namespace wasm {

void LiftoffAssembler::ReserveStackSpace(uint32_t space) { USE(stack_space_); }

void LiftoffAssembler::LoadConstant(Register reg, WasmValue value) {}

void LiftoffAssembler::LoadFromContext(Register dst, uint32_t offset,
                                       int size) {}

void LiftoffAssembler::SpillContext(Register context) {}

void LiftoffAssembler::Load(Register dst, Register src_addr,
                            uint32_t offset_imm, int size,
                            PinnedRegisterScope pinned) {}

void LiftoffAssembler::Store(Register dst_addr, uint32_t offset_imm,
                             Register src, int size,
                             PinnedRegisterScope pinned) {}

void LiftoffAssembler::LoadCallerFrameSlot(Register dst,
                                           uint32_t caller_slot_idx) {}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index) {}

void LiftoffAssembler::MoveToReturnRegister(Register reg) {}

void LiftoffAssembler::Spill(uint32_t index, Register reg) {}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {}

void LiftoffAssembler::Fill(Register reg, uint32_t index) {}

#define DEFAULT_I32_BINOP(name, internal_name)                       \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register lhs, \
                                         Register rhs) {}

// clang-format off
DEFAULT_I32_BINOP(add, add)
DEFAULT_I32_BINOP(sub, sub)
DEFAULT_I32_BINOP(mul, imul)
DEFAULT_I32_BINOP(and, and)
DEFAULT_I32_BINOP(or, or)
DEFAULT_I32_BINOP(xor, xor)
// clang-format on

#undef DEFAULT_I32_BINOP

void LiftoffAssembler::JumpIfZero(Register reg, Label* label) {}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_MIPS_H_

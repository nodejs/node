// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_X64_H_
#define V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_X64_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#include "src/assembler.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace liftoff {

inline Operand GetStackSlot(uint32_t index) {
  // rbp-8 holds the stack marker, rbp-16 is the wasm context, first stack slot
  // is located at rbp-24.
  constexpr int32_t kStackSlotSize = 8;
  constexpr int32_t kFirstStackSlotOffset = -24;
  return Operand(rbp, kFirstStackSlotOffset - index * kStackSlotSize);
}

// TODO(clemensh): Make this a constexpr variable once Operand is constexpr.
inline Operand GetContextOperand() { return Operand(rbp, -16); }

}  // namespace liftoff

void LiftoffAssembler::ReserveStackSpace(uint32_t space) {
  stack_space_ = space;
  subl(rsp, Immediate(space));
}

void LiftoffAssembler::LoadConstant(Register reg, WasmValue value) {
  switch (value.type()) {
    case kWasmI32:
      if (value.to_i32() == 0) {
        xorl(reg, reg);
      } else {
        movl(reg, Immediate(value.to_i32()));
      }
      break;
    default:
      UNIMPLEMENTED();
  }
}

void LiftoffAssembler::LoadFromContext(Register dst, uint32_t offset,
                                       int size) {
  DCHECK_LE(offset, kMaxInt);
  movp(dst, liftoff::GetContextOperand());
  DCHECK(size == 4 || size == 8);
  if (size == 4) {
    movl(dst, Operand(dst, offset));
  } else {
    movq(dst, Operand(dst, offset));
  }
}

void LiftoffAssembler::SpillContext(Register context) {
  movp(liftoff::GetContextOperand(), context);
}

void LiftoffAssembler::Load(Register dst, Register src_addr,
                            uint32_t offset_imm, int size,
                            PinnedRegisterScope pinned) {
  Operand src_op = Operand(src_addr, offset_imm);
  if (offset_imm > kMaxInt) {
    // The immediate can not be encoded in the operand. Load it to a register
    // first.
    Register src = GetUnusedRegister(kGpReg, pinned);
    movl(src, Immediate(offset_imm));
    src_op = Operand(src_addr, src, times_1, 0);
  }
  DCHECK(size == 4 || size == 8);
  if (size == 4) {
    movl(dst, src_op);
  } else {
    movq(dst, src_op);
  }
}

void LiftoffAssembler::Store(Register dst_addr, uint32_t offset_imm,
                             Register src, int size,
                             PinnedRegisterScope pinned) {
  Operand dst_op = Operand(dst_addr, offset_imm);
  if (offset_imm > kMaxInt) {
    // The immediate can not be encoded in the operand. Load it to a register
    // first.
    Register dst = GetUnusedRegister(kGpReg, pinned);
    movl(dst, Immediate(offset_imm));
    dst_op = Operand(dst_addr, dst, times_1, 0);
  }
  DCHECK(size == 4 || size == 8);
  if (size == 4) {
    movl(dst_op, src);
  } else {
    movp(dst_op, src);
  }
}

void LiftoffAssembler::LoadCallerFrameSlot(Register dst,
                                           uint32_t caller_slot_idx) {
  constexpr int32_t kStackSlotSize = 8;
  movl(dst, Operand(rbp, kStackSlotSize * (caller_slot_idx + 1)));
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index) {
  DCHECK_NE(dst_index, src_index);
  if (cache_state_.has_unused_register()) {
    Register reg = GetUnusedRegister(kGpReg);
    Fill(reg, src_index);
    Spill(dst_index, reg);
  } else {
    pushq(liftoff::GetStackSlot(src_index));
    popq(liftoff::GetStackSlot(dst_index));
  }
}

void LiftoffAssembler::MoveToReturnRegister(Register reg) {
  // TODO(clemensh): Handle different types here.
  if (reg != rax) movl(rax, reg);
}

void LiftoffAssembler::Spill(uint32_t index, Register reg) {
  // TODO(clemensh): Handle different types here.
  movl(liftoff::GetStackSlot(index), reg);
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  // TODO(clemensh): Handle different types here.
  movl(liftoff::GetStackSlot(index), Immediate(value.to_i32()));
}

void LiftoffAssembler::Fill(Register reg, uint32_t index) {
  // TODO(clemensh): Handle different types here.
  movl(reg, liftoff::GetStackSlot(index));
}

void LiftoffAssembler::emit_i32_add(Register dst, Register lhs, Register rhs) {
  if (lhs != dst) {
    leal(dst, Operand(lhs, rhs, times_1, 0));
  } else {
    addl(dst, rhs);
  }
}

void LiftoffAssembler::emit_i32_sub(Register dst, Register lhs, Register rhs) {
  if (dst == rhs) {
    negl(dst);
    addl(dst, lhs);
  } else {
    if (dst != lhs) movl(dst, lhs);
    subl(dst, rhs);
  }
}

#define COMMUTATIVE_I32_BINOP(name, instruction)                     \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register lhs, \
                                         Register rhs) {             \
    if (dst == rhs) {                                                \
      instruction##l(dst, lhs);                                      \
    } else {                                                         \
      if (dst != lhs) movl(dst, lhs);                                \
      instruction##l(dst, rhs);                                      \
    }                                                                \
  }

// clang-format off
COMMUTATIVE_I32_BINOP(mul, imul)
COMMUTATIVE_I32_BINOP(and, and)
COMMUTATIVE_I32_BINOP(or, or)
COMMUTATIVE_I32_BINOP(xor, xor)
// clang-format on

#undef DEFAULT_I32_BINOP

void LiftoffAssembler::JumpIfZero(Register reg, Label* label) {
  testl(reg, reg);
  j(zero, label);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_X64_H_

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_MIPS_LIFTOFF_ASSEMBLER_MIPS_H_
#define V8_WASM_BASELINE_MIPS_LIFTOFF_ASSEMBLER_MIPS_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#define BAILOUT(reason) bailout("mips " reason)

namespace v8 {
namespace internal {
namespace wasm {

namespace liftoff {

// sp-8 holds the stack marker, sp-16 is the wasm context, first stack slot
// is located at sp-24.
constexpr int32_t kConstantStackSpace = 16;
constexpr int32_t kFirstStackSlotOffset =
    kConstantStackSpace + LiftoffAssembler::kStackSlotSize;

inline MemOperand GetStackSlot(uint32_t index) {
  int32_t offset = index * LiftoffAssembler::kStackSlotSize;
  return MemOperand(sp, -kFirstStackSlotOffset - offset);
}

inline MemOperand GetHalfStackSlot(uint32_t half_index) {
  int32_t offset = half_index * (LiftoffAssembler::kStackSlotSize / 2);
  return MemOperand(sp, -kFirstStackSlotOffset - offset);
}

inline MemOperand GetContextOperand() { return MemOperand(sp, -16); }

}  // namespace liftoff

uint32_t LiftoffAssembler::PrepareStackFrame() {
  uint32_t offset = static_cast<uint32_t>(pc_offset());
  addiu(sp, sp, 0);
  return offset;
}

void LiftoffAssembler::PatchPrepareStackFrame(uint32_t offset,
                                              uint32_t stack_slots) {
  uint32_t bytes = liftoff::kConstantStackSpace + kStackSlotSize * stack_slots;
  DCHECK_LE(bytes, kMaxInt);
  // We can't run out of space, just pass anything big enough to not cause the
  // assembler to try to grow the buffer.
  constexpr int kAvailableSpace = 64;
  Assembler patching_assembler(isolate(), buffer_ + offset, kAvailableSpace);
  patching_assembler.addiu(sp, sp, -bytes);
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  switch (value.type()) {
    case kWasmI32:
      TurboAssembler::li(reg.gp(), Operand(value.to_i32(), rmode));
      break;
    case kWasmI64: {
      DCHECK(RelocInfo::IsNone(rmode));
      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      TurboAssembler::li(reg.low_gp(), Operand(low_word));
      TurboAssembler::li(reg.high_gp(), Operand(high_word));
      break;
    }
    case kWasmF32:
      TurboAssembler::Move(reg.fp(), value.to_f32_boxed().get_bits());
      break;
    case kWasmF64:
      TurboAssembler::Move(reg.fp(), value.to_f64_boxed().get_bits());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadFromContext(Register dst, uint32_t offset,
                                       int size) {
  DCHECK_LE(offset, kMaxInt);
  lw(dst, liftoff::GetContextOperand());
  DCHECK_EQ(4, size);
  lw(dst, MemOperand(dst, offset));
}

void LiftoffAssembler::SpillContext(Register context) {
  sw(context, liftoff::GetContextOperand());
}

void LiftoffAssembler::FillContextInto(Register dst) {
  lw(dst, liftoff::GetContextOperand());
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc) {
  // TODO(ksreten): Add check if unaligned memory access
  Register src = no_reg;
  if (offset_reg != no_reg) {
    src = GetUnusedRegister(kGpReg, pinned).gp();
    emit_ptrsize_add(src, src_addr, offset_reg);
  }
  MemOperand src_op = (offset_reg != no_reg) ? MemOperand(src, offset_imm)
                                             : MemOperand(src_addr, offset_imm);

  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
      lbu(dst.gp(), src_op);
      break;
    case LoadType::kI64Load8U:
      lbu(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI32Load8S:
      lb(dst.gp(), src_op);
      break;
    case LoadType::kI64Load8S:
      lb(dst.low_gp(), src_op);
      TurboAssembler::Move(dst.high_gp(), dst.low_gp());
      sra(dst.high_gp(), dst.high_gp(), 31);
      break;
    case LoadType::kI32Load16U:
      TurboAssembler::Ulhu(dst.gp(), src_op);
      break;
    case LoadType::kI64Load16U:
      TurboAssembler::Ulhu(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI32Load16S:
      TurboAssembler::Ulh(dst.gp(), src_op);
      break;
    case LoadType::kI64Load16S:
      TurboAssembler::Ulh(dst.low_gp(), src_op);
      TurboAssembler::Move(dst.high_gp(), dst.low_gp());
      sra(dst.high_gp(), dst.high_gp(), 31);
      break;
    case LoadType::kI32Load:
      TurboAssembler::Ulw(dst.gp(), src_op);
      break;
    case LoadType::kI64Load32U:
      TurboAssembler::Ulw(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI64Load32S:
      TurboAssembler::Ulw(dst.low_gp(), src_op);
      TurboAssembler::Move(dst.high_gp(), dst.low_gp());
      sra(dst.high_gp(), dst.high_gp(), 31);
      break;
    case LoadType::kI64Load: {
      MemOperand src_op_upper = (offset_reg != no_reg)
                                    ? MemOperand(src, offset_imm + 4)
                                    : MemOperand(src_addr, offset_imm + 4);
      TurboAssembler::Ulw(dst.high_gp(), src_op_upper);
      TurboAssembler::Ulw(dst.low_gp(), src_op);
      break;
    }
    case LoadType::kF32Load:
      TurboAssembler::Ulwc1(dst.fp(), src_op, t8);
      break;
    case LoadType::kF64Load:
      TurboAssembler::Uldc1(dst.fp(), src_op, t8);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc) {
  // TODO(ksreten): Add check if unaligned memory access
  Register dst = no_reg;
  if (offset_reg != no_reg) {
    dst = GetUnusedRegister(kGpReg, pinned).gp();
    emit_ptrsize_add(dst, dst_addr, offset_reg);
  }
  MemOperand dst_op = (offset_reg != no_reg) ? MemOperand(dst, offset_imm)
                                             : MemOperand(dst_addr, offset_imm);

  if (protected_store_pc) *protected_store_pc = pc_offset();
  switch (type.value()) {
    case StoreType::kI64Store8:
      src = src.low();
      V8_FALLTHROUGH;
    case StoreType::kI32Store8:
      sb(src.gp(), dst_op);
      break;
    case StoreType::kI64Store16:
      src = src.low();
      V8_FALLTHROUGH;
    case StoreType::kI32Store16:
      TurboAssembler::Ush(src.gp(), dst_op, t8);
      break;
    case StoreType::kI64Store32:
      src = src.low();
      V8_FALLTHROUGH;
    case StoreType::kI32Store:
      TurboAssembler::Usw(src.gp(), dst_op);
      break;
    case StoreType::kI64Store: {
      MemOperand dst_op_upper = (offset_reg != no_reg)
                                    ? MemOperand(dst, offset_imm + 4)
                                    : MemOperand(dst_addr, offset_imm + 4);
      TurboAssembler::Usw(src.high_gp(), dst_op_upper);
      TurboAssembler::Usw(src.low_gp(), dst_op);
      break;
    }
    case StoreType::kF32Store:
      TurboAssembler::Uswc1(src.fp(), dst_op, t8);
      break;
    case StoreType::kF64Store:
      TurboAssembler::Usdc1(src.fp(), dst_op, t8);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  BAILOUT("LoadCallerFrameSlot");
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index,
                                      ValueType type) {
  DCHECK_NE(dst_index, src_index);
  LiftoffRegister reg = GetUnusedRegister(reg_class_for(type));
  Fill(reg, src_index, type);
  Spill(dst_index, reg, type);
}

void LiftoffAssembler::MoveToReturnRegister(LiftoffRegister reg,
                                            ValueType type) {
  // TODO(wasm): Extract the destination register from the CallDescriptor.
  // TODO(wasm): Add multi-return support.
  LiftoffRegister dst =
      reg.is_pair()
          ? LiftoffRegister::ForPair(LiftoffRegister(v0), LiftoffRegister(v1))
          : reg.is_gp() ? LiftoffRegister(v0) : LiftoffRegister(f2);
  if (reg != dst) Move(dst, reg, type);
}

void LiftoffAssembler::Move(Register dst, Register src, ValueType type) {
  DCHECK_NE(dst, src);
  TurboAssembler::mov(dst, src);
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueType type) {
  DCHECK_NE(dst, src);
  TurboAssembler::Move(dst, src);
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg,
                             ValueType type) {
  RecordUsedSpillSlot(index);
  MemOperand dst = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      sw(reg.gp(), dst);
      break;
    case kWasmI64:
      sw(reg.low_gp(), dst);
      sw(reg.high_gp(), liftoff::GetHalfStackSlot(2 * index + 1));
      break;
    case kWasmF32:
      swc1(reg.fp(), dst);
      break;
    case kWasmF64:
      TurboAssembler::Sdc1(reg.fp(), dst);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  RecordUsedSpillSlot(index);
  MemOperand dst = liftoff::GetStackSlot(index);
  switch (value.type()) {
    case kWasmI32: {
      LiftoffRegister tmp = GetUnusedRegister(kGpReg);
      TurboAssembler::li(tmp.gp(), Operand(value.to_i32()));
      sw(tmp.gp(), dst);
      break;
    }
    case kWasmI64: {
      LiftoffRegister low = GetUnusedRegister(kGpReg);
      LiftoffRegister high = GetUnusedRegister(kGpReg);

      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      TurboAssembler::li(low.gp(), Operand(low_word));
      TurboAssembler::li(high.gp(), Operand(high_word));

      sw(low.gp(), dst);
      sw(high.gp(), liftoff::GetHalfStackSlot(2 * index + 1));
      break;
    }
    default:
      // kWasmF32 and kWasmF64 are unreachable, since those
      // constants are not tracked.
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index,
                            ValueType type) {
  MemOperand src = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      lw(reg.gp(), src);
      break;
    case kWasmI64:
      lw(reg.low_gp(), src);
      lw(reg.high_gp(), liftoff::GetHalfStackSlot(2 * index + 1));
      break;
    case kWasmF32:
      lwc1(reg.fp(), src);
      break;
    case kWasmF64:
      TurboAssembler::Ldc1(reg.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register reg, uint32_t half_index) {
  lw(reg, liftoff::GetHalfStackSlot(half_index));
}

void LiftoffAssembler::emit_i32_mul(Register dst, Register lhs, Register rhs) {
  TurboAssembler::Mul(dst, lhs, rhs);
}

#define I32_BINOP(name, instruction)                                 \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register lhs, \
                                         Register rhs) {             \
    instruction(dst, lhs, rhs);                                      \
  }

// clang-format off
I32_BINOP(add, addu)
I32_BINOP(sub, subu)
I32_BINOP(and, and_)
I32_BINOP(or, or_)
I32_BINOP(xor, xor_)
// clang-format on

#undef I32_BINOP

void LiftoffAssembler::emit_ptrsize_add(Register dst, Register lhs,
                                        Register rhs) {
  emit_i32_add(dst, lhs, rhs);
}

bool LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  TurboAssembler::Clz(dst, src);
  return true;
}

bool LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  TurboAssembler::Ctz(dst, src);
  return true;
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  TurboAssembler::Popcnt(dst, src);
  return true;
}

#define I32_SHIFTOP(name, instruction)                                   \
  void LiftoffAssembler::emit_i32_##name(                                \
      Register dst, Register lhs, Register rhs, LiftoffRegList pinned) { \
    instruction(dst, lhs, rhs);                                          \
  }

I32_SHIFTOP(shl, sllv)
I32_SHIFTOP(sar, srav)
I32_SHIFTOP(shr, srlv)

#undef I32_SHIFTOP

#define FP_BINOP(name, instruction)                                          \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    instruction(dst, lhs, rhs);                                              \
  }
#define UNIMPLEMENTED_FP_UNOP(name)                                            \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    BAILOUT("fp unop");                                                        \
  }

FP_BINOP(f32_add, add_s)
FP_BINOP(f32_sub, sub_s)
FP_BINOP(f32_mul, mul_s)
UNIMPLEMENTED_FP_UNOP(f32_neg)
FP_BINOP(f64_add, add_d)
FP_BINOP(f64_sub, sub_d)
FP_BINOP(f64_mul, mul_d)
UNIMPLEMENTED_FP_UNOP(f64_neg)

#undef FP_BINOP
#undef UNIMPLEMENTED_FP_BINOP

void LiftoffAssembler::emit_jump(Label* label) {
  TurboAssembler::Branch(label);
}

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  if (rhs != no_reg) {
    TurboAssembler::Branch(label, cond, lhs, Operand(rhs));
  } else {
    TurboAssembler::Branch(label, cond, lhs, Operand(zero_reg));
  }
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  Label true_label;
  if (dst != lhs) {
    ori(dst, zero_reg, 0x1);
  }

  if (rhs != no_reg) {
    TurboAssembler::Branch(&true_label, cond, lhs, Operand(rhs));
  } else {
    TurboAssembler::Branch(&true_label, cond, lhs, Operand(zero_reg));
  }
  // If not true, set on 0.
  TurboAssembler::mov(dst, zero_reg);

  if (dst != lhs) {
    bind(&true_label);
  } else {
    Label end_label;
    TurboAssembler::Branch(&end_label);
    bind(&true_label);

    ori(dst, zero_reg, 0x1);
    bind(&end_label);
  }
}

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  BAILOUT("emit_f32_set_cond");
}

void LiftoffAssembler::StackCheck(Label* ool_code) { BAILOUT("StackCheck"); }

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0, GetUnusedRegister(kGpReg).gp());
  CallCFunction(
      ExternalReference::wasm_call_trap_callback_for_testing(isolate()), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  BAILOUT("AssertUnreachable");
}

void LiftoffAssembler::PushCallerFrameSlot(const VarState& src,
                                           uint32_t src_index,
                                           RegPairHalf half) {
  BAILOUT("PushCallerFrameSlot");
}

void LiftoffAssembler::PushCallerFrameSlot(LiftoffRegister reg,
                                           ValueType type) {
  BAILOUT("PushCallerFrameSlot reg");
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  BAILOUT("PushRegisters");
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  BAILOUT("PopRegisters");
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  DCHECK_LT(num_stack_slots, (1 << 16) / kPointerSize);  // 16 bit immediate
  TurboAssembler::DropAndRet(static_cast<int>(num_stack_slots * kPointerSize));
}

void LiftoffAssembler::PrepareCCall(uint32_t num_params, const Register* args) {
  BAILOUT("PrepareCCall");
}

void LiftoffAssembler::SetCCallRegParamAddr(Register dst, uint32_t param_idx,
                                            uint32_t num_params) {
  BAILOUT("SetCCallRegParamAddr");
}

void LiftoffAssembler::SetCCallStackParamAddr(uint32_t stack_param_idx,
                                              uint32_t param_idx,
                                              uint32_t num_params) {
  BAILOUT("SetCCallStackParamAddr");
}

void LiftoffAssembler::CallC(ExternalReference ext_ref, uint32_t num_params) {
  BAILOUT("CallC");
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  BAILOUT("CallNativeWasmCode");
}

void LiftoffAssembler::CallRuntime(Zone* zone, Runtime::FunctionId fid) {
  BAILOUT("CallRuntime");
}

void LiftoffAssembler::CallIndirect(wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  BAILOUT("CallIndirect");
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  BAILOUT("AllocateStackSlot");
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  BAILOUT("DeallocateStackSlot");
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef BAILOUT

#endif  // V8_WASM_BASELINE_MIPS_LIFTOFF_ASSEMBLER_MIPS_H_

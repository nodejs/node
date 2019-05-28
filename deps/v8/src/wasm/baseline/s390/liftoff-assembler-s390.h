// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_H_
#define V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#define BAILOUT(reason) bailout("s390 " reason)

namespace v8 {
namespace internal {
namespace wasm {

int LiftoffAssembler::PrepareStackFrame() {
  BAILOUT("PrepareStackFrame");
  return 0;
}

void LiftoffAssembler::PatchPrepareStackFrame(int offset,
                                              uint32_t stack_slots) {
  BAILOUT("PatchPrepareStackFrame");
}

void LiftoffAssembler::FinishCode() {}

void LiftoffAssembler::AbortCompilation() {}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  BAILOUT("LoadConstant");
}

void LiftoffAssembler::LoadFromInstance(Register dst, uint32_t offset,
                                        int size) {
  BAILOUT("LoadFromInstance");
}

void LiftoffAssembler::LoadTaggedPointerFromInstance(Register dst,
                                                     uint32_t offset) {
  BAILOUT("LoadTaggedPointerFromInstance");
}

void LiftoffAssembler::SpillInstance(Register instance) {
  BAILOUT("SpillInstance");
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  BAILOUT("FillInstanceInto");
}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         uint32_t offset_imm,
                                         LiftoffRegList pinned) {
  BAILOUT("LoadTaggedPointer");
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc, bool is_load_mem) {
  BAILOUT("Load");
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  BAILOUT("Store");
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  BAILOUT("LoadCallerFrameSlot");
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index,
                                      ValueType type) {
  BAILOUT("MoveStackValue");
}

void LiftoffAssembler::Move(Register dst, Register src, ValueType type) {
  BAILOUT("Move Register");
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueType type) {
  BAILOUT("Move DoubleRegister");
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg,
                             ValueType type) {
  BAILOUT("Spill register");
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  BAILOUT("Spill value");
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index,
                            ValueType type) {
  BAILOUT("Fill");
}

void LiftoffAssembler::FillI64Half(Register, uint32_t index, RegPairHalf) {
  BAILOUT("FillI64Half");
}

#define UNIMPLEMENTED_GP_BINOP(name)                             \
  void LiftoffAssembler::emit_##name(Register dst, Register lhs, \
                                     Register rhs) {             \
    BAILOUT("gp binop: " #name);                                 \
  }
#define UNIMPLEMENTED_I64_BINOP(name)                                          \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister lhs, \
                                     LiftoffRegister rhs) {                    \
    BAILOUT("i64 binop: " #name);                                              \
  }
#define UNIMPLEMENTED_GP_UNOP(name)                                \
  bool LiftoffAssembler::emit_##name(Register dst, Register src) { \
    BAILOUT("gp unop: " #name);                                    \
    return true;                                                   \
  }
#define UNIMPLEMENTED_FP_BINOP(name)                                         \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    BAILOUT("fp binop: " #name);                                             \
  }
#define UNIMPLEMENTED_FP_UNOP(name)                                            \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    BAILOUT("fp unop: " #name);                                                \
  }
#define UNIMPLEMENTED_FP_UNOP_RETURN_TRUE(name)                                \
  bool LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    BAILOUT("fp unop: " #name);                                                \
    return true;                                                               \
  }
#define UNIMPLEMENTED_I32_SHIFTOP(name)                                        \
  void LiftoffAssembler::emit_##name(Register dst, Register src,               \
                                     Register amount, LiftoffRegList pinned) { \
    BAILOUT("i32 shiftop: " #name);                                            \
  }
#define UNIMPLEMENTED_I64_SHIFTOP(name)                                        \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister src, \
                                     Register amount, LiftoffRegList pinned) { \
    BAILOUT("i64 shiftop: " #name);                                            \
  }

UNIMPLEMENTED_GP_BINOP(i32_add)
UNIMPLEMENTED_GP_BINOP(i32_sub)
UNIMPLEMENTED_GP_BINOP(i32_mul)
UNIMPLEMENTED_GP_BINOP(i32_and)
UNIMPLEMENTED_GP_BINOP(i32_or)
UNIMPLEMENTED_GP_BINOP(i32_xor)
UNIMPLEMENTED_I32_SHIFTOP(i32_shl)
UNIMPLEMENTED_I32_SHIFTOP(i32_sar)
UNIMPLEMENTED_I32_SHIFTOP(i32_shr)
UNIMPLEMENTED_I64_BINOP(i64_add)
UNIMPLEMENTED_I64_BINOP(i64_sub)
UNIMPLEMENTED_I64_BINOP(i64_mul)
#ifdef V8_TARGET_ARCH_S390X
UNIMPLEMENTED_I64_BINOP(i64_and)
UNIMPLEMENTED_I64_BINOP(i64_or)
UNIMPLEMENTED_I64_BINOP(i64_xor)
#endif
UNIMPLEMENTED_I64_SHIFTOP(i64_shl)
UNIMPLEMENTED_I64_SHIFTOP(i64_sar)
UNIMPLEMENTED_I64_SHIFTOP(i64_shr)
UNIMPLEMENTED_GP_UNOP(i32_clz)
UNIMPLEMENTED_GP_UNOP(i32_ctz)
UNIMPLEMENTED_GP_UNOP(i32_popcnt)
UNIMPLEMENTED_FP_BINOP(f32_add)
UNIMPLEMENTED_FP_BINOP(f32_sub)
UNIMPLEMENTED_FP_BINOP(f32_mul)
UNIMPLEMENTED_FP_BINOP(f32_div)
UNIMPLEMENTED_FP_BINOP(f32_min)
UNIMPLEMENTED_FP_BINOP(f32_max)
UNIMPLEMENTED_FP_BINOP(f32_copysign)
UNIMPLEMENTED_FP_UNOP(f32_abs)
UNIMPLEMENTED_FP_UNOP(f32_neg)
UNIMPLEMENTED_FP_UNOP_RETURN_TRUE(f32_ceil)
UNIMPLEMENTED_FP_UNOP_RETURN_TRUE(f32_floor)
UNIMPLEMENTED_FP_UNOP_RETURN_TRUE(f32_trunc)
UNIMPLEMENTED_FP_UNOP_RETURN_TRUE(f32_nearest_int)
UNIMPLEMENTED_FP_UNOP(f32_sqrt)
UNIMPLEMENTED_FP_BINOP(f64_add)
UNIMPLEMENTED_FP_BINOP(f64_sub)
UNIMPLEMENTED_FP_BINOP(f64_mul)
UNIMPLEMENTED_FP_BINOP(f64_div)
UNIMPLEMENTED_FP_BINOP(f64_min)
UNIMPLEMENTED_FP_BINOP(f64_max)
UNIMPLEMENTED_FP_BINOP(f64_copysign)
UNIMPLEMENTED_FP_UNOP(f64_abs)
UNIMPLEMENTED_FP_UNOP(f64_neg)
UNIMPLEMENTED_FP_UNOP_RETURN_TRUE(f64_ceil)
UNIMPLEMENTED_FP_UNOP_RETURN_TRUE(f64_floor)
UNIMPLEMENTED_FP_UNOP_RETURN_TRUE(f64_trunc)
UNIMPLEMENTED_FP_UNOP_RETURN_TRUE(f64_nearest_int)
UNIMPLEMENTED_FP_UNOP(f64_sqrt)

#undef UNIMPLEMENTED_GP_BINOP
#undef UNIMPLEMENTED_I64_BINOP
#undef UNIMPLEMENTED_GP_UNOP
#undef UNIMPLEMENTED_FP_BINOP
#undef UNIMPLEMENTED_FP_UNOP
#undef UNIMPLEMENTED_FP_UNOP_RETURN_TRUE
#undef UNIMPLEMENTED_I32_SHIFTOP
#undef UNIMPLEMENTED_I64_SHIFTOP

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  BAILOUT("i32_divs");
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  BAILOUT("i32_divu");
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  BAILOUT("i32_rems");
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  BAILOUT("i32_remu");
}

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    int32_t imm) {
  BAILOUT("i64_add");
}

void LiftoffAssembler::emit_i32_add(Register dst, Register lhs, int32_t imm) {
  BAILOUT("i32_add");
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register lhs, int amount) {
  BAILOUT("i32_shr");
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  BAILOUT("i64_divs");
  return true;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  BAILOUT("i64_divu");
  return true;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  BAILOUT("i64_rems");
  return true;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  BAILOUT("i64_remu");
  return true;
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister lhs,
                                    int amount) {
  BAILOUT("i64_shr");
}

void LiftoffAssembler::emit_i32_to_intptr(Register dst, Register src) {
#ifdef V8_TARGET_ARCH_S390X
  BAILOUT("emit_i32_to_intptr");
#else
// This is a nop on s390.
#endif
}

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  BAILOUT("emit_type_conversion");
  return true;
}

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  BAILOUT("emit_i32_signextend_i8");
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  BAILOUT("emit_i32_signextend_i16");
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  BAILOUT("emit_i64_signextend_i8");
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  BAILOUT("emit_i64_signextend_i16");
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  BAILOUT("emit_i64_signextend_i32");
}

void LiftoffAssembler::emit_jump(Label* label) { BAILOUT("emit_jump"); }

void LiftoffAssembler::emit_jump(Register target) { BAILOUT("emit_jump"); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  BAILOUT("emit_cond_jump");
}

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  BAILOUT("emit_i32_eqz");
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  BAILOUT("emit_i32_set_cond");
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  BAILOUT("emit_i64_eqz");
}

void LiftoffAssembler::emit_i64_set_cond(Condition cond, Register dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  BAILOUT("emit_i64_set_cond");
}

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  BAILOUT("emit_f32_set_cond");
}

void LiftoffAssembler::emit_f64_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  BAILOUT("emit_f64_set_cond");
}

void LiftoffAssembler::StackCheck(Label* ool_code, Register limit_address) {
  BAILOUT("StackCheck");
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  BAILOUT("CallTrapCallbackForTesting");
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  BAILOUT("AssertUnreachable");
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  BAILOUT("PushRegisters");
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  BAILOUT("PopRegisters");
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  BAILOUT("DropStackSlotsAndRet");
}

void LiftoffAssembler::CallC(wasm::FunctionSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueType out_argument_type, int stack_bytes,
                             ExternalReference ext_ref) {
  BAILOUT("CallC");
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  BAILOUT("CallNativeWasmCode");
}

void LiftoffAssembler::CallIndirect(wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  BAILOUT("CallIndirect");
}

void LiftoffAssembler::CallRuntimeStub(WasmCode::RuntimeStubId sid) {
  BAILOUT("CallRuntimeStub");
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  BAILOUT("AllocateStackSlot");
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  BAILOUT("DeallocateStackSlot");
}

void LiftoffStackSlots::Construct() {
  asm_->BAILOUT("LiftoffStackSlots::Construct");
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef BAILOUT

#endif  // V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_H_

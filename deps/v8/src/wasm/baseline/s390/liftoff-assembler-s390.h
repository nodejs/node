// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_H_
#define V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_H_

#include "src/wasm/baseline/liftoff-assembler.h"


namespace v8 {
namespace internal {
namespace wasm {

int LiftoffAssembler::PrepareStackFrame() {
  bailout(kUnsupportedArchitecture, "PrepareStackFrame");
  return 0;
}

void LiftoffAssembler::PatchPrepareStackFrame(int offset,
                                              uint32_t stack_slots) {
  bailout(kUnsupportedArchitecture, "PatchPrepareStackFrame");
}

void LiftoffAssembler::FinishCode() {}

void LiftoffAssembler::AbortCompilation() {}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  bailout(kUnsupportedArchitecture, "LoadConstant");
}

void LiftoffAssembler::LoadFromInstance(Register dst, uint32_t offset,
                                        int size) {
  bailout(kUnsupportedArchitecture, "LoadFromInstance");
}

void LiftoffAssembler::LoadTaggedPointerFromInstance(Register dst,
                                                     uint32_t offset) {
  bailout(kUnsupportedArchitecture, "LoadTaggedPointerFromInstance");
}

void LiftoffAssembler::SpillInstance(Register instance) {
  bailout(kUnsupportedArchitecture, "SpillInstance");
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  bailout(kUnsupportedArchitecture, "FillInstanceInto");
}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         uint32_t offset_imm,
                                         LiftoffRegList pinned) {
  bailout(kUnsupportedArchitecture, "LoadTaggedPointer");
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc, bool is_load_mem) {
  bailout(kUnsupportedArchitecture, "Load");
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  bailout(kUnsupportedArchitecture, "Store");
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  bailout(kUnsupportedArchitecture, "LoadCallerFrameSlot");
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index,
                                      ValueType type) {
  bailout(kUnsupportedArchitecture, "MoveStackValue");
}

void LiftoffAssembler::Move(Register dst, Register src, ValueType type) {
  bailout(kUnsupportedArchitecture, "Move Register");
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueType type) {
  bailout(kUnsupportedArchitecture, "Move DoubleRegister");
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg,
                             ValueType type) {
  bailout(kUnsupportedArchitecture, "Spill register");
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  bailout(kUnsupportedArchitecture, "Spill value");
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index,
                            ValueType type) {
  bailout(kUnsupportedArchitecture, "Fill");
}

void LiftoffAssembler::FillI64Half(Register, uint32_t index, RegPairHalf) {
  bailout(kUnsupportedArchitecture, "FillI64Half");
}

#define UNIMPLEMENTED_I32_BINOP(name)                            \
  void LiftoffAssembler::emit_##name(Register dst, Register lhs, \
                                     Register rhs) {             \
    bailout(kUnsupportedArchitecture, "i32 binop: " #name);      \
  }
#define UNIMPLEMENTED_I32_BINOP_I(name)                          \
  UNIMPLEMENTED_I32_BINOP(name)                                  \
  void LiftoffAssembler::emit_##name(Register dst, Register lhs, \
                                     int32_t imm) {              \
    bailout(kUnsupportedArchitecture, "i32 binop_i: " #name);    \
  }
#define UNIMPLEMENTED_I64_BINOP(name)                                          \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister lhs, \
                                     LiftoffRegister rhs) {                    \
    bailout(kUnsupportedArchitecture, "i64 binop: " #name);                    \
  }
#define UNIMPLEMENTED_I64_BINOP_I(name)                                        \
  UNIMPLEMENTED_I64_BINOP(name)                                                \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister lhs, \
                                     int32_t imm) {                            \
    bailout(kUnsupportedArchitecture, "i64 binop_i: " #name);                  \
  }
#define UNIMPLEMENTED_GP_UNOP(name)                                \
  bool LiftoffAssembler::emit_##name(Register dst, Register src) { \
    bailout(kUnsupportedArchitecture, "gp unop: " #name);          \
    return true;                                                   \
  }
#define UNIMPLEMENTED_FP_BINOP(name)                                         \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    bailout(kUnsupportedArchitecture, "fp binop: " #name);                   \
  }
#define UNIMPLEMENTED_FP_UNOP(name)                                            \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    bailout(kUnsupportedArchitecture, "fp unop: " #name);                      \
  }
#define UNIMPLEMENTED_FP_UNOP_RETURN_TRUE(name)                                \
  bool LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    bailout(kUnsupportedArchitecture, "fp unop: " #name);                      \
    return true;                                                               \
  }
#define UNIMPLEMENTED_I32_SHIFTOP(name)                                        \
  void LiftoffAssembler::emit_##name(Register dst, Register src,               \
                                     Register amount, LiftoffRegList pinned) { \
    bailout(kUnsupportedArchitecture, "i32 shiftop: " #name);                  \
  }
#define UNIMPLEMENTED_I64_SHIFTOP(name)                                        \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister src, \
                                     Register amount, LiftoffRegList pinned) { \
    bailout(kUnsupportedArchitecture, "i64 shiftop: " #name);                  \
  }

UNIMPLEMENTED_I32_BINOP_I(i32_add)
UNIMPLEMENTED_I32_BINOP(i32_sub)
UNIMPLEMENTED_I32_BINOP(i32_mul)
UNIMPLEMENTED_I32_BINOP_I(i32_and)
UNIMPLEMENTED_I32_BINOP_I(i32_or)
UNIMPLEMENTED_I32_BINOP_I(i32_xor)
UNIMPLEMENTED_I32_SHIFTOP(i32_shl)
UNIMPLEMENTED_I32_SHIFTOP(i32_sar)
UNIMPLEMENTED_I32_SHIFTOP(i32_shr)
UNIMPLEMENTED_I64_BINOP_I(i64_add)
UNIMPLEMENTED_I64_BINOP(i64_sub)
UNIMPLEMENTED_I64_BINOP(i64_mul)
#ifdef V8_TARGET_ARCH_S390X
UNIMPLEMENTED_I64_BINOP_I(i64_and)
UNIMPLEMENTED_I64_BINOP_I(i64_or)
UNIMPLEMENTED_I64_BINOP_I(i64_xor)
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

#undef UNIMPLEMENTED_I32_BINOP
#undef UNIMPLEMENTED_I32_BINOP_I
#undef UNIMPLEMENTED_I64_BINOP
#undef UNIMPLEMENTED_I64_BINOP_I
#undef UNIMPLEMENTED_GP_UNOP
#undef UNIMPLEMENTED_FP_BINOP
#undef UNIMPLEMENTED_FP_UNOP
#undef UNIMPLEMENTED_FP_UNOP_RETURN_TRUE
#undef UNIMPLEMENTED_I32_SHIFTOP
#undef UNIMPLEMENTED_I64_SHIFTOP

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  bailout(kUnsupportedArchitecture, "i32_divs");
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  bailout(kUnsupportedArchitecture, "i32_divu");
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  bailout(kUnsupportedArchitecture, "i32_rems");
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  bailout(kUnsupportedArchitecture, "i32_remu");
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register lhs, int amount) {
  bailout(kUnsupportedArchitecture, "i32_shr");
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  bailout(kUnsupportedArchitecture, "i64_divs");
  return true;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  bailout(kUnsupportedArchitecture, "i64_divu");
  return true;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  bailout(kUnsupportedArchitecture, "i64_rems");
  return true;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  bailout(kUnsupportedArchitecture, "i64_remu");
  return true;
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister lhs,
                                    int amount) {
  bailout(kUnsupportedArchitecture, "i64_shr");
}

void LiftoffAssembler::emit_i32_to_intptr(Register dst, Register src) {
#ifdef V8_TARGET_ARCH_S390X
  bailout(kUnsupportedArchitecture, "emit_i32_to_intptr");
#else
// This is a nop on s390.
#endif
}

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  bailout(kUnsupportedArchitecture, "emit_type_conversion");
  return true;
}

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  bailout(kUnsupportedArchitecture, "emit_i32_signextend_i8");
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  bailout(kUnsupportedArchitecture, "emit_i32_signextend_i16");
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i64_signextend_i8");
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i64_signextend_i16");
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i64_signextend_i32");
}

void LiftoffAssembler::emit_jump(Label* label) {
  bailout(kUnsupportedArchitecture, "emit_jump");
}

void LiftoffAssembler::emit_jump(Register target) {
  bailout(kUnsupportedArchitecture, "emit_jump");
}

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  bailout(kUnsupportedArchitecture, "emit_cond_jump");
}

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  bailout(kUnsupportedArchitecture, "emit_i32_eqz");
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  bailout(kUnsupportedArchitecture, "emit_i32_set_cond");
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  bailout(kUnsupportedArchitecture, "emit_i64_eqz");
}

void LiftoffAssembler::emit_i64_set_cond(Condition cond, Register dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_i64_set_cond");
}

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f32_set_cond");
}

void LiftoffAssembler::emit_f64_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  bailout(kUnsupportedArchitecture, "emit_f64_set_cond");
}

void LiftoffAssembler::StackCheck(Label* ool_code, Register limit_address) {
  bailout(kUnsupportedArchitecture, "StackCheck");
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  bailout(kUnsupportedArchitecture, "CallTrapCallbackForTesting");
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  bailout(kUnsupportedArchitecture, "AssertUnreachable");
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  bailout(kUnsupportedArchitecture, "PushRegisters");
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  bailout(kUnsupportedArchitecture, "PopRegisters");
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  bailout(kUnsupportedArchitecture, "DropStackSlotsAndRet");
}

void LiftoffAssembler::CallC(wasm::FunctionSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueType out_argument_type, int stack_bytes,
                             ExternalReference ext_ref) {
  bailout(kUnsupportedArchitecture, "CallC");
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  bailout(kUnsupportedArchitecture, "CallNativeWasmCode");
}

void LiftoffAssembler::CallIndirect(wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  bailout(kUnsupportedArchitecture, "CallIndirect");
}

void LiftoffAssembler::CallRuntimeStub(WasmCode::RuntimeStubId sid) {
  bailout(kUnsupportedArchitecture, "CallRuntimeStub");
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  bailout(kUnsupportedArchitecture, "AllocateStackSlot");
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  bailout(kUnsupportedArchitecture, "DeallocateStackSlot");
}

void LiftoffStackSlots::Construct() {
  asm_->bailout(kUnsupportedArchitecture, "LiftoffStackSlots::Construct");
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef BAILOUT

#endif  // V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_H_

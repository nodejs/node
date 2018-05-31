// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_ARM_LIFTOFF_ASSEMBLER_ARM_H_
#define V8_WASM_BASELINE_ARM_LIFTOFF_ASSEMBLER_ARM_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#define BAILOUT(reason) bailout("arm " reason)

namespace v8 {
namespace internal {
namespace wasm {

uint32_t LiftoffAssembler::PrepareStackFrame() {
  BAILOUT("PrepareStackFrame");
  return 0;
}

void LiftoffAssembler::PatchPrepareStackFrame(uint32_t offset,
                                              uint32_t stack_slots) {
  BAILOUT("PatchPrepareStackFrame");
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  BAILOUT("LoadConstant");
}

void LiftoffAssembler::LoadFromInstance(Register dst, uint32_t offset,
                                        int size) {
  BAILOUT("LoadFromInstance");
}

void LiftoffAssembler::SpillInstance(Register instance) {
  BAILOUT("SpillInstance");
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  BAILOUT("FillInstanceInto");
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc) {
  BAILOUT("Load");
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc) {
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

void LiftoffAssembler::MoveToReturnRegister(LiftoffRegister reg,
                                            ValueType type) {
  BAILOUT("MoveToReturnRegister");
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

void LiftoffAssembler::FillI64Half(Register, uint32_t half_index) {
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
UNIMPLEMENTED_FP_UNOP(f32_abs)
UNIMPLEMENTED_FP_UNOP(f32_neg)
UNIMPLEMENTED_FP_UNOP(f32_ceil)
UNIMPLEMENTED_FP_UNOP(f32_floor)
UNIMPLEMENTED_FP_UNOP(f32_trunc)
UNIMPLEMENTED_FP_UNOP(f32_nearest_int)
UNIMPLEMENTED_FP_UNOP(f32_sqrt)
UNIMPLEMENTED_FP_BINOP(f64_add)
UNIMPLEMENTED_FP_BINOP(f64_sub)
UNIMPLEMENTED_FP_BINOP(f64_mul)
UNIMPLEMENTED_FP_BINOP(f64_div)
UNIMPLEMENTED_FP_UNOP(f64_abs)
UNIMPLEMENTED_FP_UNOP(f64_neg)
UNIMPLEMENTED_FP_UNOP(f64_ceil)
UNIMPLEMENTED_FP_UNOP(f64_floor)
UNIMPLEMENTED_FP_UNOP(f64_trunc)
UNIMPLEMENTED_FP_UNOP(f64_nearest_int)
UNIMPLEMENTED_FP_UNOP(f64_sqrt)

#undef UNIMPLEMENTED_GP_BINOP
#undef UNIMPLEMENTED_I64_BINOP
#undef UNIMPLEMENTED_GP_UNOP
#undef UNIMPLEMENTED_FP_BINOP
#undef UNIMPLEMENTED_FP_UNOP
#undef UNIMPLEMENTED_I32_SHIFTOP
#undef UNIMPLEMENTED_I64_SHIFTOP

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src) {
  BAILOUT("emit_type_conversion");
  return true;
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

void LiftoffAssembler::StackCheck(Label* ool_code) { BAILOUT("StackCheck"); }

void LiftoffAssembler::CallTrapCallbackForTesting() {
  BAILOUT("CallTrapCallbackForTesting");
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
  BAILOUT("DropStackSlotsAndRet");
}

void LiftoffAssembler::PrepareCCall(wasm::FunctionSig* sig,
                                    const LiftoffRegister* args,
                                    ValueType out_argument_type) {
  BAILOUT("PrepareCCall");
}

void LiftoffAssembler::SetCCallRegParamAddr(Register dst, int param_byte_offset,
                                            ValueType type) {
  BAILOUT("SetCCallRegParamAddr");
}

void LiftoffAssembler::SetCCallStackParamAddr(int stack_param_idx,
                                              int param_byte_offset,
                                              ValueType type) {
  BAILOUT("SetCCallStackParamAddr");
}

void LiftoffAssembler::LoadCCallOutArgument(LiftoffRegister dst, ValueType type,
                                            int param_byte_offset) {
  BAILOUT("LoadCCallOutArgument");
}

void LiftoffAssembler::CallC(ExternalReference ext_ref, uint32_t num_params) {
  BAILOUT("CallC");
}

void LiftoffAssembler::FinishCCall() { BAILOUT("FinishCCall"); }

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

#endif  // V8_WASM_BASELINE_ARM_LIFTOFF_ASSEMBLER_ARM_H_

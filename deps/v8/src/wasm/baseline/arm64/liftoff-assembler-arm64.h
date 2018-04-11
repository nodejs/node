// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_ARM64_LIFTOFF_ASSEMBLER_ARM64_H_
#define V8_WASM_BASELINE_ARM64_LIFTOFF_ASSEMBLER_ARM64_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#define BAILOUT(reason) bailout("arm64 " reason)

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

void LiftoffAssembler::LoadFromContext(Register dst, uint32_t offset,
                                       int size) {
  BAILOUT("LoadFromContext");
}

void LiftoffAssembler::SpillContext(Register context) {
  BAILOUT("SpillContext");
}

void LiftoffAssembler::FillContextInto(Register dst) {
  BAILOUT("FillContextInto");
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
    BAILOUT("gp binop");                                         \
  }
#define UNIMPLEMENTED_GP_UNOP(name)                                \
  bool LiftoffAssembler::emit_##name(Register dst, Register src) { \
    BAILOUT("gp unop");                                            \
    return true;                                                   \
  }
#define UNIMPLEMENTED_FP_BINOP(name)                                         \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    BAILOUT("fp binop");                                                     \
  }
#define UNIMPLEMENTED_FP_UNOP(name)                                            \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    BAILOUT("fp unop");                                                        \
  }
#define UNIMPLEMENTED_SHIFTOP(name)                                            \
  void LiftoffAssembler::emit_##name(Register dst, Register lhs, Register rhs, \
                                     LiftoffRegList pinned) {                  \
    BAILOUT("shiftop");                                                        \
  }

UNIMPLEMENTED_GP_BINOP(i32_add)
UNIMPLEMENTED_GP_BINOP(i32_sub)
UNIMPLEMENTED_GP_BINOP(i32_mul)
UNIMPLEMENTED_GP_BINOP(i32_and)
UNIMPLEMENTED_GP_BINOP(i32_or)
UNIMPLEMENTED_GP_BINOP(i32_xor)
UNIMPLEMENTED_SHIFTOP(i32_shl)
UNIMPLEMENTED_SHIFTOP(i32_sar)
UNIMPLEMENTED_SHIFTOP(i32_shr)
UNIMPLEMENTED_GP_UNOP(i32_clz)
UNIMPLEMENTED_GP_UNOP(i32_ctz)
UNIMPLEMENTED_GP_UNOP(i32_popcnt)
UNIMPLEMENTED_GP_BINOP(ptrsize_add)
UNIMPLEMENTED_FP_BINOP(f32_add)
UNIMPLEMENTED_FP_BINOP(f32_sub)
UNIMPLEMENTED_FP_BINOP(f32_mul)
UNIMPLEMENTED_FP_UNOP(f32_neg)
UNIMPLEMENTED_FP_BINOP(f64_add)
UNIMPLEMENTED_FP_BINOP(f64_sub)
UNIMPLEMENTED_FP_BINOP(f64_mul)
UNIMPLEMENTED_FP_UNOP(f64_neg)

#undef UNIMPLEMENTED_GP_BINOP
#undef UNIMPLEMENTED_GP_UNOP
#undef UNIMPLEMENTED_FP_BINOP
#undef UNIMPLEMENTED_FP_UNOP
#undef UNIMPLEMENTED_SHIFTOP

void LiftoffAssembler::emit_jump(Label* label) { BAILOUT("emit_jump"); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  BAILOUT("emit_cond_jump");
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  BAILOUT("emit_i32_set_cond");
}

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  BAILOUT("emit_f32_set_cond");
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

#endif  // V8_WASM_BASELINE_ARM64_LIFTOFF_ASSEMBLER_ARM64_H_

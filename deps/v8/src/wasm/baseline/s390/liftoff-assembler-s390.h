// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_S390_H_
#define V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_S390_H_

#include "src/wasm/baseline/liftoff-assembler.h"

namespace v8 {
namespace internal {
namespace wasm {

void LiftoffAssembler::ReserveStackSpace(uint32_t stack_slots) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::LoadFromContext(Register dst, uint32_t offset,
                                       int size) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::SpillContext(Register context) { UNIMPLEMENTED(); }

void LiftoffAssembler::FillContextInto(Register dst) { UNIMPLEMENTED(); }

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index,
                                      ValueType type) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::MoveToReturnRegister(LiftoffRegister reg) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::Move(LiftoffRegister dst, LiftoffRegister src) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg,
                             ValueType type) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index,
                            ValueType type) {
  UNIMPLEMENTED();
}

#define UNIMPLEMENTED_GP_BINOP(name)                             \
  void LiftoffAssembler::emit_##name(Register dst, Register lhs, \
                                     Register rhs) {             \
    UNIMPLEMENTED();                                             \
  }
#define UNIMPLEMENTED_GP_UNOP(name)                                \
  bool LiftoffAssembler::emit_##name(Register dst, Register src) { \
    UNIMPLEMENTED();                                               \
  }
#define UNIMPLEMENTED_FP_BINOP(name)                                         \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    UNIMPLEMENTED();                                                         \
  }
#define UNIMPLEMENTED_SHIFTOP(name)                                            \
  void LiftoffAssembler::emit_##name(Register dst, Register lhs, Register rhs, \
                                     LiftoffRegList pinned) {                  \
    UNIMPLEMENTED();                                                           \
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

#undef UNIMPLEMENTED_GP_BINOP
#undef UNIMPLEMENTED_GP_UNOP
#undef UNIMPLEMENTED_FP_BINOP
#undef UNIMPLEMENTED_SHIFTOP

void LiftoffAssembler::emit_i32_test(Register reg) { UNIMPLEMENTED(); }

void LiftoffAssembler::emit_i32_compare(Register lhs, Register rhs) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::emit_ptrsize_compare(Register lhs, Register rhs) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::emit_jump(Label* label) { UNIMPLEMENTED(); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::StackCheck(Label* ool_code) { UNIMPLEMENTED(); }

void LiftoffAssembler::CallTrapCallbackForTesting() { UNIMPLEMENTED(); }

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::PushCallerFrameSlot(const VarState& src,
                                           uint32_t src_index) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::PushCallerFrameSlot(LiftoffRegister reg) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) { UNIMPLEMENTED(); }

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) { UNIMPLEMENTED(); }

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::PrepareCCall(uint32_t num_params, const Register* args) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::SetCCallRegParamAddr(Register dst, uint32_t param_idx,
                                            uint32_t num_params) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::SetCCallStackParamAddr(uint32_t stack_param_idx,
                                              uint32_t param_idx,
                                              uint32_t num_params) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::CallC(ExternalReference ext_ref, uint32_t num_params) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) { UNIMPLEMENTED(); }

void LiftoffAssembler::CallRuntime(Zone* zone, Runtime::FunctionId fid) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::CallIndirect(wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_desc,
                                    Register target,
                                    uint32_t* max_used_spill_slot) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  UNIMPLEMENTED();
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) { UNIMPLEMENTED(); }

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_S390_H_

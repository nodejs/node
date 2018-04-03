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
  constexpr int32_t kFirstStackSlotOffset = -24;
  return Operand(
      rbp, kFirstStackSlotOffset - index * LiftoffAssembler::kStackSlotSize);
}

// TODO(clemensh): Make this a constexpr variable once Operand is constexpr.
inline Operand GetContextOperand() { return Operand(rbp, -16); }

// Use this register to store the address of the last argument pushed on the
// stack for a call to C.
static constexpr Register kCCallLastArgAddrReg = rax;

}  // namespace liftoff

void LiftoffAssembler::ReserveStackSpace(uint32_t bytes) {
  DCHECK_LE(bytes, kMaxInt);
  subp(rsp, Immediate(bytes));
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value) {
  switch (value.type()) {
    case kWasmI32:
      if (value.to_i32() == 0) {
        xorl(reg.gp(), reg.gp());
      } else {
        movl(reg.gp(), Immediate(value.to_i32()));
      }
      break;
    case kWasmF32:
      TurboAssembler::Move(reg.fp(), value.to_f32_boxed().get_bits());
      break;
    default:
      UNREACHABLE();
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

void LiftoffAssembler::FillContextInto(Register dst) {
  movp(dst, liftoff::GetContextOperand());
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc) {
  Operand src_op = offset_reg == no_reg
                       ? Operand(src_addr, offset_imm)
                       : Operand(src_addr, offset_reg, times_1, offset_imm);
  if (offset_imm > kMaxInt) {
    // The immediate can not be encoded in the operand. Load it to a register
    // first.
    Register src = GetUnusedRegister(kGpReg, pinned).gp();
    movl(src, Immediate(offset_imm));
    if (offset_reg != no_reg) {
      emit_ptrsize_add(src, src, offset_reg);
    }
    src_op = Operand(src_addr, src, times_1, 0);
  }
  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
      movzxbl(dst.gp(), src_op);
      break;
    case LoadType::kI32Load8S:
      movsxbl(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16U:
      movzxwl(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16S:
      movsxwl(dst.gp(), src_op);
      break;
    case LoadType::kI32Load:
      movl(dst.gp(), src_op);
      break;
    case LoadType::kI64Load:
      movq(dst.gp(), src_op);
      break;
    case LoadType::kF32Load:
      Movss(dst.fp(), src_op);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc) {
  Operand dst_op = offset_reg == no_reg
                       ? Operand(dst_addr, offset_imm)
                       : Operand(dst_addr, offset_reg, times_1, offset_imm);
  if (offset_imm > kMaxInt) {
    // The immediate can not be encoded in the operand. Load it to a register
    // first.
    Register dst = GetUnusedRegister(kGpReg, pinned).gp();
    movl(dst, Immediate(offset_imm));
    if (offset_reg != no_reg) {
      emit_ptrsize_add(dst, dst, offset_reg);
    }
    dst_op = Operand(dst_addr, dst, times_1, 0);
  }
  if (protected_store_pc) *protected_store_pc = pc_offset();
  switch (type.value()) {
    case StoreType::kI32Store8:
      movb(dst_op, src.gp());
      break;
    case StoreType::kI32Store16:
      movw(dst_op, src.gp());
      break;
    case StoreType::kI32Store:
      movl(dst_op, src.gp());
      break;
    case StoreType::kI64Store:
      movq(dst_op, src.gp());
      break;
    case StoreType::kF32Store:
      Movss(dst_op, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx) {
  Operand src(rbp, kPointerSize * (caller_slot_idx + 1));
  // TODO(clemensh): Handle different sizes here.
  if (dst.is_gp()) {
    movq(dst.gp(), src);
  } else {
    Movsd(dst.fp(), src);
  }
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index) {
  DCHECK_NE(dst_index, src_index);
  if (cache_state_.has_unused_register(kGpReg)) {
    LiftoffRegister reg = GetUnusedRegister(kGpReg);
    Fill(reg, src_index);
    Spill(dst_index, reg);
  } else {
    pushq(liftoff::GetStackSlot(src_index));
    popq(liftoff::GetStackSlot(dst_index));
  }
}

void LiftoffAssembler::MoveToReturnRegister(LiftoffRegister reg) {
  // TODO(wasm): Extract the destination register from the CallDescriptor.
  // TODO(wasm): Add multi-return support.
  LiftoffRegister dst =
      reg.is_gp() ? LiftoffRegister(rax) : LiftoffRegister(xmm1);
  if (reg != dst) Move(dst, reg);
}

void LiftoffAssembler::Move(LiftoffRegister dst, LiftoffRegister src) {
  // The caller should check that the registers are not equal. For most
  // occurences, this is already guaranteed, so no need to check within this
  // method.
  DCHECK_NE(dst, src);
  DCHECK_EQ(dst.reg_class(), src.reg_class());
  // TODO(clemensh): Handle different sizes here.
  if (dst.is_gp()) {
    movq(dst.gp(), src.gp());
  } else {
    Movsd(dst.fp(), src.fp());
  }
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg) {
  Operand dst = liftoff::GetStackSlot(index);
  // TODO(clemensh): Handle different sizes here.
  if (reg.is_gp()) {
    movq(dst, reg.gp());
  } else {
    Movsd(dst, reg.fp());
  }
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  Operand dst = liftoff::GetStackSlot(index);
  switch (value.type()) {
    case kWasmI32:
      movl(dst, Immediate(value.to_i32()));
      break;
    case kWasmF32:
      movl(dst, Immediate(value.to_f32_boxed().get_bits()));
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index) {
  Operand src = liftoff::GetStackSlot(index);
  // TODO(clemensh): Handle different sizes here.
  if (reg.is_gp()) {
    movq(reg.gp(), src);
  } else {
    Movsd(reg.fp(), src);
  }
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

#undef COMMUTATIVE_I32_BINOP

namespace liftoff {
inline void EmitShiftOperation(LiftoffAssembler* assm, Register dst,
                               Register lhs, Register rhs,
                               void (Assembler::*emit_shift)(Register)) {
  // If dst is rcx, compute into the scratch register first, then move to rcx.
  if (dst == rcx) {
    assm->movl(kScratchRegister, lhs);
    if (rhs != rcx) assm->movl(rcx, rhs);
    (assm->*emit_shift)(kScratchRegister);
    assm->movl(rcx, kScratchRegister);
    return;
  }

  // Move rhs into rcx. If rcx is in use, move its content into the scratch
  // register. If lhs is rcx, lhs is now the scratch register.
  bool use_scratch = false;
  if (rhs != rcx) {
    use_scratch =
        lhs == rcx || assm->cache_state()->is_used(LiftoffRegister(rcx));
    if (use_scratch) assm->movl(kScratchRegister, rcx);
    if (lhs == rcx) lhs = kScratchRegister;
    assm->movl(rcx, rhs);
  }

  // Do the actual shift.
  if (dst != lhs) assm->movl(dst, lhs);
  (assm->*emit_shift)(dst);

  // Restore rcx if needed.
  if (use_scratch) assm->movl(rcx, kScratchRegister);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_shl(Register dst, Register lhs, Register rhs) {
  liftoff::EmitShiftOperation(this, dst, lhs, rhs, &Assembler::shll_cl);
}

void LiftoffAssembler::emit_i32_sar(Register dst, Register lhs, Register rhs) {
  liftoff::EmitShiftOperation(this, dst, lhs, rhs, &Assembler::sarl_cl);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register lhs, Register rhs) {
  liftoff::EmitShiftOperation(this, dst, lhs, rhs, &Assembler::shrl_cl);
}

bool LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  testl(src, src);
  setcc(zero, dst);
  movzxbl(dst, dst);
  return true;
}

bool LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  Label nonzero_input;
  Label continuation;
  testl(src, src);
  j(not_zero, &nonzero_input, Label::kNear);
  movl(dst, Immediate(32));
  jmp(&continuation, Label::kNear);

  bind(&nonzero_input);
  // Get most significant bit set (MSBS).
  bsrl(dst, src);
  // CLZ = 31 - MSBS = MSBS ^ 31.
  xorl(dst, Immediate(31));

  bind(&continuation);
  return true;
}

bool LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  Label nonzero_input;
  Label continuation;
  testl(src, src);
  j(not_zero, &nonzero_input, Label::kNear);
  movl(dst, Immediate(32));
  jmp(&continuation, Label::kNear);

  bind(&nonzero_input);
  // Get least significant bit set, which equals number of trailing zeros.
  bsfl(dst, src);

  bind(&continuation);
  return true;
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  if (!CpuFeatures::IsSupported(POPCNT)) return false;
  CpuFeatureScope scope(this, POPCNT);
  popcntl(dst, src);
  return true;
}

void LiftoffAssembler::emit_ptrsize_add(Register dst, Register lhs,
                                        Register rhs) {
  if (lhs != dst) {
    leap(dst, Operand(lhs, rhs, times_1, 0));
  } else {
    addp(dst, rhs);
  }
}

void LiftoffAssembler::emit_f32_add(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vaddss(dst, lhs, rhs);
  } else if (dst == rhs) {
    addss(dst, lhs);
  } else {
    if (dst != lhs) movss(dst, lhs);
    addss(dst, rhs);
  }
}

void LiftoffAssembler::emit_f32_sub(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vsubss(dst, lhs, rhs);
  } else if (dst == rhs) {
    movss(kScratchDoubleReg, rhs);
    movss(dst, lhs);
    subss(dst, kScratchDoubleReg);
  } else {
    if (dst != lhs) movss(dst, lhs);
    subss(dst, rhs);
  }
}

void LiftoffAssembler::emit_f32_mul(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmulss(dst, lhs, rhs);
  } else if (dst == rhs) {
    mulss(dst, lhs);
  } else {
    if (dst != lhs) movss(dst, lhs);
    mulss(dst, rhs);
  }
}

void LiftoffAssembler::emit_i32_test(Register reg) { testl(reg, reg); }

void LiftoffAssembler::emit_i32_compare(Register lhs, Register rhs) {
  cmpl(lhs, rhs);
}

void LiftoffAssembler::emit_jump(Label* label) { jmp(label); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label) {
  j(cond, label);
}

void LiftoffAssembler::StackCheck(Label* ool_code) {
  Register limit = GetUnusedRegister(kGpReg).gp();
  LoadAddress(limit, ExternalReference::address_of_stack_limit(isolate()));
  cmpp(rsp, Operand(limit, 0));
  j(below_equal, ool_code);
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0);
  CallCFunction(
      ExternalReference::wasm_call_trap_callback_for_testing(isolate()), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  TurboAssembler::AssertUnreachable(reason);
}

void LiftoffAssembler::PushCallerFrameSlot(const VarState& src,
                                           uint32_t src_index) {
  switch (src.loc()) {
    case VarState::kStack:
      pushq(liftoff::GetStackSlot(src_index));
      break;
    case VarState::kRegister:
      PushCallerFrameSlot(src.reg());
      break;
    case VarState::kI32Const:
      pushq(Immediate(src.i32_const()));
      break;
  }
}

void LiftoffAssembler::PushCallerFrameSlot(LiftoffRegister reg) {
  if (reg.is_gp()) {
    pushq(reg.gp());
  } else {
    subp(rsp, Immediate(kPointerSize));
    Movsd(Operand(rsp, 0), reg.fp());
  }
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetFirstRegSet();
    pushq(reg.gp());
    gp_regs.clear(reg);
  }
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned num_fp_regs = fp_regs.GetNumRegsSet();
  if (num_fp_regs) {
    subp(rsp, Immediate(num_fp_regs * kStackSlotSize));
    unsigned offset = 0;
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetFirstRegSet();
      Movsd(Operand(rsp, offset), reg.fp());
      fp_regs.clear(reg);
      offset += sizeof(double);
    }
    DCHECK_EQ(offset, num_fp_regs * sizeof(double));
  }
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned fp_offset = 0;
  while (!fp_regs.is_empty()) {
    LiftoffRegister reg = fp_regs.GetFirstRegSet();
    Movsd(reg.fp(), Operand(rsp, fp_offset));
    fp_regs.clear(reg);
    fp_offset += sizeof(double);
  }
  if (fp_offset) addp(rsp, Immediate(fp_offset));
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetLastRegSet();
    popq(reg.gp());
    gp_regs.clear(reg);
  }
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  DCHECK_LT(num_stack_slots, (1 << 16) / kPointerSize);  // 16 bit immediate
  ret(static_cast<int>(num_stack_slots * kPointerSize));
}

void LiftoffAssembler::PrepareCCall(uint32_t num_params, const Register* args) {
  for (size_t param = 0; param < num_params; ++param) {
    pushq(args[param]);
  }
  movq(liftoff::kCCallLastArgAddrReg, rsp);
  PrepareCallCFunction(num_params);
}

void LiftoffAssembler::SetCCallRegParamAddr(Register dst, uint32_t param_idx,
                                            uint32_t num_params) {
  int offset = kPointerSize * static_cast<int>(num_params - 1 - param_idx);
  leaq(dst, Operand(liftoff::kCCallLastArgAddrReg, offset));
}

void LiftoffAssembler::SetCCallStackParamAddr(uint32_t stack_param_idx,
                                              uint32_t param_idx,
                                              uint32_t num_params) {
  // On x64, all C call arguments fit in registers.
  UNREACHABLE();
}

void LiftoffAssembler::CallC(ExternalReference ext_ref, uint32_t num_params) {
  CallCFunction(ext_ref, static_cast<int>(num_params));
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  near_call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallRuntime(Zone* zone, Runtime::FunctionId fid) {
  // Set context to zero.
  xorp(rsi, rsi);
  CallRuntimeDelayed(zone, fid);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  subp(rsp, Immediate(size));
  movp(addr, rsp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  addp(rsp, Immediate(size));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_X64_H_

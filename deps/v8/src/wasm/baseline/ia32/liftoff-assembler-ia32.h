// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_IA32_H_
#define V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_IA32_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#include "src/assembler.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace liftoff {

inline Operand GetStackSlot(uint32_t index) {
  // ebp-8 holds the stack marker, ebp-16 is the wasm context, first stack slot
  // is located at ebp-24.
  constexpr int32_t kFirstStackSlotOffset = -24;
  return Operand(
      ebp, kFirstStackSlotOffset - index * LiftoffAssembler::kStackSlotSize);
}

// TODO(clemensh): Make this a constexpr variable once Operand is constexpr.
inline Operand GetContextOperand() { return Operand(ebp, -16); }

static constexpr LiftoffRegList kByteRegs =
    LiftoffRegList::FromBits<Register::ListOf<eax, ecx, edx, ebx>()>();
static_assert(kByteRegs.GetNumRegsSet() == 4, "should have four byte regs");
static_assert((kByteRegs & kGpCacheRegList) == kByteRegs,
              "kByteRegs only contains gp cache registers");

// Use this register to store the address of the last argument pushed on the
// stack for a call to C.
static constexpr Register kCCallLastArgAddrReg = eax;

}  // namespace liftoff

static constexpr DoubleRegister kScratchDoubleReg = xmm7;

void LiftoffAssembler::ReserveStackSpace(uint32_t bytes) {
  DCHECK_LE(bytes, kMaxInt);
  sub(esp, Immediate(bytes));
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value) {
  switch (value.type()) {
    case kWasmI32:
      if (value.to_i32() == 0) {
        xor_(reg.gp(), reg.gp());
      } else {
        mov(reg.gp(), Immediate(value.to_i32()));
      }
      break;
    case kWasmF32: {
      Register tmp = GetUnusedRegister(kGpReg).gp();
      mov(tmp, Immediate(value.to_f32_boxed().get_bits()));
      movd(reg.fp(), tmp);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadFromContext(Register dst, uint32_t offset,
                                       int size) {
  DCHECK_LE(offset, kMaxInt);
  mov(dst, liftoff::GetContextOperand());
  DCHECK_EQ(4, size);
  mov(dst, Operand(dst, offset));
}

void LiftoffAssembler::SpillContext(Register context) {
  mov(liftoff::GetContextOperand(), context);
}

void LiftoffAssembler::FillContextInto(Register dst) {
  mov(dst, liftoff::GetContextOperand());
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
    mov(src, Immediate(offset_imm));
    if (offset_reg != no_reg) {
      emit_ptrsize_add(src, src, offset_reg);
    }
    src_op = Operand(src_addr, src, times_1, 0);
  }
  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
      movzx_b(dst.gp(), src_op);
      break;
    case LoadType::kI32Load8S:
      movsx_b(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16U:
      movzx_w(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16S:
      movsx_w(dst.gp(), src_op);
      break;
    case LoadType::kI32Load:
      mov(dst.gp(), src_op);
      break;
    case LoadType::kF32Load:
      movss(dst.fp(), src_op);
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
    Register dst = pinned.set(GetUnusedRegister(kGpReg, pinned).gp());
    mov(dst, Immediate(offset_imm));
    if (offset_reg != no_reg) {
      emit_ptrsize_add(dst, dst, offset_reg);
    }
    dst_op = Operand(dst_addr, dst, times_1, 0);
  }
  if (protected_store_pc) *protected_store_pc = pc_offset();
  switch (type.value()) {
    case StoreType::kI32Store8:
      // Only the lower 4 registers can be addressed as 8-bit registers.
      if (src.gp().is_byte_register()) {
        mov_b(dst_op, src.gp());
      } else {
        Register byte_src = GetUnusedRegister(liftoff::kByteRegs, pinned).gp();
        mov(byte_src, src.gp());
        mov_b(dst_op, byte_src);
      }
      break;
    case StoreType::kI32Store16:
      mov_w(dst_op, src.gp());
      break;
    case StoreType::kI32Store:
      mov(dst_op, src.gp());
      break;
    case StoreType::kF32Store:
      movss(dst_op, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx) {
  Operand src(ebp, kPointerSize * (caller_slot_idx + 1));
  if (dst.is_gp()) {
    mov(dst.gp(), src);
  } else {
    movss(dst.fp(), src);
  }
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index) {
  DCHECK_NE(dst_index, src_index);
  if (cache_state_.has_unused_register(kGpReg)) {
    LiftoffRegister reg = GetUnusedRegister(kGpReg);
    Fill(reg, src_index);
    Spill(dst_index, reg);
  } else {
    push(liftoff::GetStackSlot(src_index));
    pop(liftoff::GetStackSlot(dst_index));
  }
}

void LiftoffAssembler::MoveToReturnRegister(LiftoffRegister reg) {
  // TODO(wasm): Extract the destination register from the CallDescriptor.
  // TODO(wasm): Add multi-return support.
  LiftoffRegister dst =
      reg.is_gp() ? LiftoffRegister(eax) : LiftoffRegister(xmm1);
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
    mov(dst.gp(), src.gp());
  } else {
    movsd(dst.fp(), src.fp());
  }
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg) {
  Operand dst = liftoff::GetStackSlot(index);
  // TODO(clemensh): Handle different sizes here.
  if (reg.is_gp()) {
    mov(dst, reg.gp());
  } else {
    movsd(dst, reg.fp());
  }
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  Operand dst = liftoff::GetStackSlot(index);
  switch (value.type()) {
    case kWasmI32:
      mov(dst, Immediate(value.to_i32()));
      break;
    case kWasmF32:
      mov(dst, Immediate(value.to_f32_boxed().get_bits()));
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index) {
  Operand src = liftoff::GetStackSlot(index);
  // TODO(clemensh): Handle different sizes here.
  if (reg.is_gp()) {
    mov(reg.gp(), src);
  } else {
    movsd(reg.fp(), src);
  }
}

void LiftoffAssembler::emit_i32_add(Register dst, Register lhs, Register rhs) {
  if (lhs != dst) {
    lea(dst, Operand(lhs, rhs, times_1, 0));
  } else {
    add(dst, rhs);
  }
}

void LiftoffAssembler::emit_i32_sub(Register dst, Register lhs, Register rhs) {
  if (dst == rhs) {
    neg(dst);
    add(dst, lhs);
  } else {
    if (dst != lhs) mov(dst, lhs);
    sub(dst, rhs);
  }
}

#define COMMUTATIVE_I32_BINOP(name, instruction)                     \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register lhs, \
                                         Register rhs) {             \
    if (dst == rhs) {                                                \
      instruction(dst, lhs);                                         \
    } else {                                                         \
      if (dst != lhs) mov(dst, lhs);                                 \
      instruction(dst, rhs);                                         \
    }                                                                \
  }

// clang-format off
COMMUTATIVE_I32_BINOP(mul, imul)
COMMUTATIVE_I32_BINOP(and, and_)
COMMUTATIVE_I32_BINOP(or, or_)
COMMUTATIVE_I32_BINOP(xor, xor_)
// clang-format on

#undef COMMUTATIVE_I32_BINOP

namespace liftoff {
inline void EmitShiftOperation(LiftoffAssembler* assm, Register dst,
                               Register lhs, Register rhs,
                               void (Assembler::*emit_shift)(Register)) {
  LiftoffRegList pinned = LiftoffRegList::ForRegs(dst, lhs, rhs);
  // If dst is ecx, compute into a tmp register first, then move to ecx.
  if (dst == ecx) {
    Register tmp = assm->GetUnusedRegister(kGpReg, pinned).gp();
    assm->mov(tmp, lhs);
    if (rhs != ecx) assm->mov(ecx, rhs);
    (assm->*emit_shift)(tmp);
    assm->mov(ecx, tmp);
    return;
  }

  // Move rhs into ecx. If ecx is in use, move its content to a tmp register
  // first. If lhs is ecx, lhs is now the tmp register.
  Register tmp_reg = no_reg;
  if (rhs != ecx) {
    if (lhs == ecx || assm->cache_state()->is_used(LiftoffRegister(ecx))) {
      tmp_reg = assm->GetUnusedRegister(kGpReg, pinned).gp();
      assm->mov(tmp_reg, ecx);
      if (lhs == ecx) lhs = tmp_reg;
    }
    assm->mov(ecx, rhs);
  }

  // Do the actual shift.
  if (dst != lhs) assm->mov(dst, lhs);
  (assm->*emit_shift)(dst);

  // Restore ecx if needed.
  if (tmp_reg.is_valid()) assm->mov(ecx, tmp_reg);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_shl(Register dst, Register lhs, Register rhs) {
  liftoff::EmitShiftOperation(this, dst, lhs, rhs, &Assembler::shl_cl);
}

void LiftoffAssembler::emit_i32_sar(Register dst, Register lhs, Register rhs) {
  liftoff::EmitShiftOperation(this, dst, lhs, rhs, &Assembler::sar_cl);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register lhs, Register rhs) {
  liftoff::EmitShiftOperation(this, dst, lhs, rhs, &Assembler::shr_cl);
}

bool LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  Register tmp_byte_reg = dst;
  // Only the lower 4 registers can be addressed as 8-bit registers.
  if (!dst.is_byte_register()) {
    LiftoffRegList pinned = LiftoffRegList::ForRegs(src);
    tmp_byte_reg = GetUnusedRegister(liftoff::kByteRegs, pinned).gp();
  }

  test(src, src);
  setcc(zero, tmp_byte_reg);
  movzx_b(dst, tmp_byte_reg);
  return true;
}

bool LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  Label nonzero_input;
  Label continuation;
  test(src, src);
  j(not_zero, &nonzero_input, Label::kNear);
  mov(dst, Immediate(32));
  jmp(&continuation, Label::kNear);

  bind(&nonzero_input);
  // Get most significant bit set (MSBS).
  bsr(dst, src);
  // CLZ = 31 - MSBS = MSBS ^ 31.
  xor_(dst, 31);

  bind(&continuation);
  return true;
}

bool LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  Label nonzero_input;
  Label continuation;
  test(src, src);
  j(not_zero, &nonzero_input, Label::kNear);
  mov(dst, Immediate(32));
  jmp(&continuation, Label::kNear);

  bind(&nonzero_input);
  // Get least significant bit set, which equals number of trailing zeros.
  bsf(dst, src);

  bind(&continuation);
  return true;
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  if (!CpuFeatures::IsSupported(POPCNT)) return false;
  CpuFeatureScope scope(this, POPCNT);
  popcnt(dst, src);
  return true;
}

void LiftoffAssembler::emit_ptrsize_add(Register dst, Register lhs,
                                        Register rhs) {
  emit_i32_add(dst, lhs, rhs);
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

void LiftoffAssembler::emit_i32_test(Register reg) { test(reg, reg); }

void LiftoffAssembler::emit_i32_compare(Register lhs, Register rhs) {
  cmp(lhs, rhs);
}

void LiftoffAssembler::emit_jump(Label* label) { jmp(label); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label) {
  j(cond, label);
}

void LiftoffAssembler::StackCheck(Label* ool_code) {
  Register limit = GetUnusedRegister(kGpReg).gp();
  mov(limit, Immediate(ExternalReference::address_of_stack_limit(isolate())));
  cmp(esp, Operand(limit, 0));
  j(below_equal, ool_code);
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0, GetUnusedRegister(kGpReg).gp());
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
      DCHECK_NE(kWasmF64, src.type());  // TODO(clemensh): Implement this.
      push(liftoff::GetStackSlot(src_index));
      break;
    case VarState::kRegister:
      PushCallerFrameSlot(src.reg());
      break;
    case VarState::kI32Const:
      push(Immediate(src.i32_const()));
      break;
  }
}

void LiftoffAssembler::PushCallerFrameSlot(LiftoffRegister reg) {
  if (reg.is_gp()) {
    push(reg.gp());
  } else {
    sub(esp, Immediate(kPointerSize));
    movss(Operand(esp, 0), reg.fp());
  }
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetFirstRegSet();
    push(reg.gp());
    gp_regs.clear(reg);
  }
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned num_fp_regs = fp_regs.GetNumRegsSet();
  if (num_fp_regs) {
    sub(esp, Immediate(num_fp_regs * kStackSlotSize));
    unsigned offset = 0;
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetFirstRegSet();
      movsd(Operand(esp, offset), reg.fp());
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
    movsd(reg.fp(), Operand(esp, fp_offset));
    fp_regs.clear(reg);
    fp_offset += sizeof(double);
  }
  if (fp_offset) add(esp, Immediate(fp_offset));
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetLastRegSet();
    pop(reg.gp());
    gp_regs.clear(reg);
  }
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  DCHECK_LT(num_stack_slots, (1 << 16) / kPointerSize);  // 16 bit immediate
  ret(static_cast<int>(num_stack_slots * kPointerSize));
}

void LiftoffAssembler::PrepareCCall(uint32_t num_params, const Register* args) {
  for (size_t param = 0; param < num_params; ++param) {
    push(args[param]);
  }
  mov(liftoff::kCCallLastArgAddrReg, esp);
  constexpr Register kScratch = ebx;
  static_assert(kScratch != liftoff::kCCallLastArgAddrReg, "collision");
  PrepareCallCFunction(num_params, kScratch);
}

void LiftoffAssembler::SetCCallRegParamAddr(Register dst, uint32_t param_idx,
                                            uint32_t num_params) {
  int offset = kPointerSize * static_cast<int>(num_params - 1 - param_idx);
  lea(dst, Operand(liftoff::kCCallLastArgAddrReg, offset));
}

void LiftoffAssembler::SetCCallStackParamAddr(uint32_t stack_param_idx,
                                              uint32_t param_idx,
                                              uint32_t num_params) {
  constexpr Register kScratch = ebx;
  static_assert(kScratch != liftoff::kCCallLastArgAddrReg, "collision");
  int offset = kPointerSize * static_cast<int>(num_params - 1 - param_idx);
  lea(kScratch, Operand(liftoff::kCCallLastArgAddrReg, offset));
  mov(Operand(esp, param_idx * kPointerSize), kScratch);
}

void LiftoffAssembler::CallC(ExternalReference ext_ref, uint32_t num_params) {
  CallCFunction(ext_ref, static_cast<int>(num_params));
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  wasm_call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallRuntime(Zone* zone, Runtime::FunctionId fid) {
  // Set context to zero.
  xor_(esi, esi);
  CallRuntimeDelayed(zone, fid);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  sub(esp, Immediate(size));
  mov(addr, esp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  add(esp, Immediate(size));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_IA32_H_

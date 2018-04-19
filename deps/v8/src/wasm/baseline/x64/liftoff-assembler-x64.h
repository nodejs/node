// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_X64_LIFTOFF_ASSEMBLER_X64_H_
#define V8_WASM_BASELINE_X64_LIFTOFF_ASSEMBLER_X64_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#include "src/assembler.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace liftoff {

// rbp-8 holds the stack marker, rbp-16 is the wasm context, first stack slot
// is located at rbp-24.
constexpr int32_t kConstantStackSpace = 16;
constexpr int32_t kFirstStackSlotOffset =
    kConstantStackSpace + LiftoffAssembler::kStackSlotSize;

inline Operand GetStackSlot(uint32_t index) {
  int32_t offset = index * LiftoffAssembler::kStackSlotSize;
  return Operand(rbp, -kFirstStackSlotOffset - offset);
}

inline Operand GetHalfStackSlot(uint32_t half_index) {
  int32_t offset = half_index * (LiftoffAssembler::kStackSlotSize / 2);
  return Operand(rbp, -kFirstStackSlotOffset - offset);
}

// TODO(clemensh): Make this a constexpr variable once Operand is constexpr.
inline Operand GetContextOperand() { return Operand(rbp, -16); }

// Use this register to store the address of the last argument pushed on the
// stack for a call to C.
static constexpr Register kCCallLastArgAddrReg = rax;

inline Operand GetMemOp(LiftoffAssembler* assm, Register addr, Register offset,
                        uint32_t offset_imm, LiftoffRegList pinned) {
  // Wasm memory is limited to a size <2GB, so all offsets can be encoded as
  // immediate value (in 31 bits, interpreted as signed value).
  // If the offset is bigger, we always trap and this code is not reached.
  DCHECK(is_uint31(offset_imm));
  if (offset == no_reg) return Operand(addr, offset_imm);
  return Operand(addr, offset, times_1, offset_imm);
}

}  // namespace liftoff

uint32_t LiftoffAssembler::PrepareStackFrame() {
  uint32_t offset = static_cast<uint32_t>(pc_offset());
  sub_sp_32(0);
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
  patching_assembler.sub_sp_32(bytes);
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  switch (value.type()) {
    case kWasmI32:
      if (value.to_i32() == 0 && RelocInfo::IsNone(rmode)) {
        xorl(reg.gp(), reg.gp());
      } else {
        movl(reg.gp(), Immediate(value.to_i32(), rmode));
      }
      break;
    case kWasmI64:
      if (RelocInfo::IsNone(rmode)) {
        TurboAssembler::Set(reg.gp(), value.to_i64());
      } else {
        movq(reg.gp(), value.to_i64(), rmode);
      }
      break;
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
  Operand src_op =
      liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm, pinned);
  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U:
      movzxbl(dst.gp(), src_op);
      break;
    case LoadType::kI32Load8S:
      movsxbl(dst.gp(), src_op);
      break;
    case LoadType::kI64Load8S:
      movsxbq(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      movzxwl(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16S:
      movsxwl(dst.gp(), src_op);
      break;
    case LoadType::kI64Load16S:
      movsxwq(dst.gp(), src_op);
      break;
    case LoadType::kI32Load:
    case LoadType::kI64Load32U:
      movl(dst.gp(), src_op);
      break;
    case LoadType::kI64Load32S:
      movsxlq(dst.gp(), src_op);
      break;
    case LoadType::kI64Load:
      movq(dst.gp(), src_op);
      break;
    case LoadType::kF32Load:
      Movss(dst.fp(), src_op);
      break;
    case LoadType::kF64Load:
      Movsd(dst.fp(), src_op);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc) {
  Operand dst_op =
      liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm, pinned);
  if (protected_store_pc) *protected_store_pc = pc_offset();
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      movb(dst_op, src.gp());
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      movw(dst_op, src.gp());
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      movl(dst_op, src.gp());
      break;
    case StoreType::kI64Store:
      movq(dst_op, src.gp());
      break;
    case StoreType::kF32Store:
      Movss(dst_op, src.fp());
      break;
    case StoreType::kF64Store:
      Movsd(dst_op, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  Operand src(rbp, kPointerSize * (caller_slot_idx + 1));
  switch (type) {
    case kWasmI32:
      movl(dst.gp(), src);
      break;
    case kWasmI64:
      movq(dst.gp(), src);
      break;
    case kWasmF32:
      Movss(dst.fp(), src);
      break;
    case kWasmF64:
      Movsd(dst.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index,
                                      ValueType type) {
  DCHECK_NE(dst_index, src_index);
  if (cache_state_.has_unused_register(kGpReg)) {
    LiftoffRegister reg = GetUnusedRegister(kGpReg);
    Fill(reg, src_index, type);
    Spill(dst_index, reg, type);
  } else {
    pushq(liftoff::GetStackSlot(src_index));
    popq(liftoff::GetStackSlot(dst_index));
  }
}

void LiftoffAssembler::MoveToReturnRegister(LiftoffRegister reg,
                                            ValueType type) {
  // TODO(wasm): Extract the destination register from the CallDescriptor.
  // TODO(wasm): Add multi-return support.
  LiftoffRegister dst =
      reg.is_gp() ? LiftoffRegister(rax) : LiftoffRegister(xmm1);
  if (reg != dst) Move(dst, reg, type);
}

void LiftoffAssembler::Move(Register dst, Register src, ValueType type) {
  DCHECK_NE(dst, src);
  if (type == kWasmI32) {
    movl(dst, src);
  } else {
    DCHECK_EQ(kWasmI64, type);
    movq(dst, src);
  }
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueType type) {
  DCHECK_NE(dst, src);
  if (type == kWasmF32) {
    Movss(dst, src);
  } else {
    DCHECK_EQ(kWasmF64, type);
    Movsd(dst, src);
  }
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg,
                             ValueType type) {
  RecordUsedSpillSlot(index);
  Operand dst = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      movl(dst, reg.gp());
      break;
    case kWasmI64:
      movq(dst, reg.gp());
      break;
    case kWasmF32:
      Movss(dst, reg.fp());
      break;
    case kWasmF64:
      Movsd(dst, reg.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  RecordUsedSpillSlot(index);
  Operand dst = liftoff::GetStackSlot(index);
  switch (value.type()) {
    case kWasmI32:
      movl(dst, Immediate(value.to_i32()));
      break;
    case kWasmI64: {
      // We could use movq, but this would require a temporary register. For
      // simplicity (and to avoid potentially having to spill another register),
      // we use two movl instructions.
      int32_t low_word = static_cast<int32_t>(value.to_i64());
      int32_t high_word = static_cast<int32_t>(value.to_i64() >> 32);
      movl(dst, Immediate(low_word));
      movl(liftoff::GetHalfStackSlot(2 * index + 1), Immediate(high_word));
      break;
    }
    case kWasmF32:
      movl(dst, Immediate(value.to_f32_boxed().get_bits()));
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index,
                            ValueType type) {
  Operand src = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      movl(reg.gp(), src);
      break;
    case kWasmI64:
      movq(reg.gp(), src);
      break;
    case kWasmF32:
      Movss(reg.fp(), src);
      break;
    case kWasmF64:
      Movsd(reg.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register, uint32_t half_index) {
  UNREACHABLE();
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
                               void (Assembler::*emit_shift)(Register),
                               LiftoffRegList pinned) {
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
    use_scratch = lhs == rcx ||
                  assm->cache_state()->is_used(LiftoffRegister(rcx)) ||
                  pinned.has(LiftoffRegister(rcx));
    if (use_scratch) assm->movq(kScratchRegister, rcx);
    if (lhs == rcx) lhs = kScratchRegister;
    assm->movl(rcx, rhs);
  }

  // Do the actual shift.
  if (dst != lhs) assm->movl(dst, lhs);
  (assm->*emit_shift)(dst);

  // Restore rcx if needed.
  if (use_scratch) assm->movq(rcx, kScratchRegister);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_shl(Register dst, Register lhs, Register rhs,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation(this, dst, lhs, rhs, &Assembler::shll_cl, pinned);
}

void LiftoffAssembler::emit_i32_sar(Register dst, Register lhs, Register rhs,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation(this, dst, lhs, rhs, &Assembler::sarl_cl, pinned);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register lhs, Register rhs,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation(this, dst, lhs, rhs, &Assembler::shrl_cl, pinned);
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

void LiftoffAssembler::emit_f32_neg(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint32_t kSignBit = uint32_t{1} << 31;
  if (dst == src) {
    TurboAssembler::Move(kScratchDoubleReg, kSignBit);
    Xorps(dst, kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit);
    Xorps(dst, src);
  }
}

void LiftoffAssembler::emit_f64_add(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vaddsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    addsd(dst, lhs);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    addsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_sub(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vsubsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    movsd(kScratchDoubleReg, rhs);
    movsd(dst, lhs);
    subsd(dst, kScratchDoubleReg);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    subsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_mul(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmulsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    mulsd(dst, lhs);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    mulsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_neg(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint64_t kSignBit = uint64_t{1} << 63;
  if (dst == src) {
    TurboAssembler::Move(kScratchDoubleReg, kSignBit);
    Xorpd(dst, kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit);
    Xorpd(dst, src);
  }
}

void LiftoffAssembler::emit_jump(Label* label) { jmp(label); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  if (rhs != no_reg) {
    switch (type) {
      case kWasmI32:
        cmpl(lhs, rhs);
        break;
      case kWasmI64:
        cmpq(lhs, rhs);
        break;
      default:
        UNREACHABLE();
    }
  } else {
    DCHECK_EQ(type, kWasmI32);
    testl(lhs, lhs);
  }

  j(cond, label);
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  if (rhs != no_reg) {
    cmpl(lhs, rhs);
  } else {
    testl(lhs, lhs);
  }

  setcc(cond, dst);
  movzxbl(dst, dst);
}

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  Label cont;
  Label not_nan;

  Ucomiss(lhs, rhs);
  // IF PF is one, one of the operands was Nan. This needs special handling.
  j(parity_odd, &not_nan, Label::kNear);
  // Return 1 for f32.ne, 0 for all other cases.
  if (cond == not_equal) {
    movl(dst, Immediate(1));
  } else {
    xorl(dst, dst);
  }
  jmp(&cont, Label::kNear);
  bind(&not_nan);

  setcc(cond, dst);
  movzxbl(dst, dst);
  bind(&cont);
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
                                           uint32_t src_index, RegPairHalf) {
  switch (src.loc()) {
    case VarState::kStack:
      pushq(liftoff::GetStackSlot(src_index));
      break;
    case VarState::kRegister:
      PushCallerFrameSlot(src.reg(), src.type());
      break;
    case VarState::KIntConst:
      pushq(Immediate(src.i32_const()));
      break;
  }
}

void LiftoffAssembler::PushCallerFrameSlot(LiftoffRegister reg,
                                           ValueType type) {
  switch (type) {
    case kWasmI32:
    case kWasmI64:
      pushq(reg.gp());
      break;
    case kWasmF32:
      subp(rsp, Immediate(kPointerSize));
      Movss(Operand(rsp, 0), reg.fp());
      break;
    case kWasmF64:
      subp(rsp, Immediate(kPointerSize));
      Movsd(Operand(rsp, 0), reg.fp());
      break;
    default:
      UNREACHABLE();
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

void LiftoffAssembler::CallIndirect(wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  if (target == no_reg) {
    popq(kScratchRegister);
    target = kScratchRegister;
  }
  call(target);
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

#endif  // V8_WASM_BASELINE_X64_LIFTOFF_ASSEMBLER_X64_H_

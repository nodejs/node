// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_IA32_LIFTOFF_ASSEMBLER_IA32_H_
#define V8_WASM_BASELINE_IA32_LIFTOFF_ASSEMBLER_IA32_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#include "src/assembler.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

#define REQUIRE_CPU_FEATURE(name)                                   \
  if (!CpuFeatures::IsSupported(name)) return bailout("no " #name); \
  CpuFeatureScope feature(this, name);

namespace liftoff {

// ebp-4 holds the stack marker, ebp-8 is the instance parameter, first stack
// slot is located at ebp-16.
constexpr int32_t kConstantStackSpace = 8;
constexpr int32_t kFirstStackSlotOffset =
    kConstantStackSpace + LiftoffAssembler::kStackSlotSize;

inline Operand GetStackSlot(uint32_t index) {
  int32_t offset = index * LiftoffAssembler::kStackSlotSize;
  return Operand(ebp, -kFirstStackSlotOffset - offset);
}

inline Operand GetHalfStackSlot(uint32_t half_index) {
  int32_t offset = half_index * (LiftoffAssembler::kStackSlotSize / 2);
  return Operand(ebp, -kFirstStackSlotOffset - offset);
}

// TODO(clemensh): Make this a constexpr variable once Operand is constexpr.
inline Operand GetInstanceOperand() { return Operand(ebp, -8); }

static constexpr LiftoffRegList kByteRegs =
    LiftoffRegList::FromBits<Register::ListOf<eax, ecx, edx, ebx>()>();
static_assert(kByteRegs.GetNumRegsSet() == 4, "should have four byte regs");
static_assert((kByteRegs & kGpCacheRegList) == kByteRegs,
              "kByteRegs only contains gp cache registers");

// Use this register to store the address of the last argument pushed on the
// stack for a call to C. This register must be callee saved according to the c
// calling convention.
static constexpr Register kCCallLastArgAddrReg = ebx;

inline void Load(LiftoffAssembler* assm, LiftoffRegister dst, Operand src,
                 ValueType type) {
  switch (type) {
    case kWasmI32:
      assm->mov(dst.gp(), src);
      break;
    case kWasmF32:
      assm->movss(dst.fp(), src);
      break;
    case kWasmF64:
      assm->movsd(dst.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueType type) {
  switch (type) {
    case kWasmI32:
      assm->push(reg.gp());
      break;
    case kWasmI64:
      assm->push(reg.high_gp());
      assm->push(reg.low_gp());
      break;
    case kWasmF32:
      assm->sub(esp, Immediate(sizeof(float)));
      assm->movss(Operand(esp, 0), reg.fp());
      break;
    case kWasmF64:
      assm->sub(esp, Immediate(sizeof(double)));
      assm->movsd(Operand(esp, 0), reg.fp());
      break;
    default:
      UNREACHABLE();
  }
}

}  // namespace liftoff

static constexpr DoubleRegister kScratchDoubleReg = xmm7;

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
      TurboAssembler::Move(
          reg.gp(),
          Immediate(reinterpret_cast<Address>(value.to_i32()), rmode));
      break;
    case kWasmI64: {
      DCHECK(RelocInfo::IsNone(rmode));
      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      TurboAssembler::Move(reg.low_gp(), Immediate(low_word));
      TurboAssembler::Move(reg.high_gp(), Immediate(high_word));
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

void LiftoffAssembler::LoadFromInstance(Register dst, uint32_t offset,
                                        int size) {
  DCHECK_LE(offset, kMaxInt);
  mov(dst, liftoff::GetInstanceOperand());
  DCHECK_EQ(4, size);
  mov(dst, Operand(dst, offset));
}

void LiftoffAssembler::SpillInstance(Register instance) {
  mov(liftoff::GetInstanceOperand(), instance);
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  mov(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc) {
  DCHECK_EQ(type.value_type() == kWasmI64, dst.is_pair());
  // Wasm memory is limited to a size <2GB, so all offsets can be encoded as
  // immediate value (in 31 bits, interpreted as signed value).
  // If the offset is bigger, we always trap and this code is not reached.
  DCHECK(is_uint31(offset_imm));
  Operand src_op = offset_reg == no_reg
                       ? Operand(src_addr, offset_imm)
                       : Operand(src_addr, offset_reg, times_1, offset_imm);
  if (protected_load_pc) *protected_load_pc = pc_offset();

  switch (type.value()) {
    case LoadType::kI32Load8U:
      movzx_b(dst.gp(), src_op);
      break;
    case LoadType::kI32Load8S:
      movsx_b(dst.gp(), src_op);
      break;
    case LoadType::kI64Load8U:
      movzx_b(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI64Load8S:
      movsx_b(dst.low_gp(), src_op);
      mov(dst.high_gp(), dst.low_gp());
      sar(dst.high_gp(), 31);
      break;
    case LoadType::kI32Load16U:
      movzx_w(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16S:
      movsx_w(dst.gp(), src_op);
      break;
    case LoadType::kI64Load16U:
      movzx_w(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI64Load16S:
      movsx_w(dst.low_gp(), src_op);
      mov(dst.high_gp(), dst.low_gp());
      sar(dst.high_gp(), 31);
      break;
    case LoadType::kI32Load:
      mov(dst.gp(), src_op);
      break;
    case LoadType::kI64Load32U:
      mov(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI64Load32S:
      mov(dst.low_gp(), src_op);
      mov(dst.high_gp(), dst.low_gp());
      sar(dst.high_gp(), 31);
      break;
    case LoadType::kI64Load: {
      // Compute the operand for the load of the upper half.
      DCHECK(is_uint31(offset_imm + 4));
      Operand upper_src_op =
          offset_reg == no_reg
              ? Operand(src_addr, offset_imm + 4)
              : Operand(src_addr, offset_reg, times_1, offset_imm + 4);
      // The high word has to be mov'ed first, such that this is the protected
      // instruction. The mov of the low word cannot segfault.
      mov(dst.high_gp(), upper_src_op);
      mov(dst.low_gp(), src_op);
      break;
    }
    case LoadType::kF32Load:
      movss(dst.fp(), src_op);
      break;
    case LoadType::kF64Load:
      movsd(dst.fp(), src_op);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc) {
  DCHECK_EQ(type.value_type() == kWasmI64, src.is_pair());
  // Wasm memory is limited to a size <2GB, so all offsets can be encoded as
  // immediate value (in 31 bits, interpreted as signed value).
  // If the offset is bigger, we always trap and this code is not reached.
  DCHECK(is_uint31(offset_imm));
  Operand dst_op = offset_reg == no_reg
                       ? Operand(dst_addr, offset_imm)
                       : Operand(dst_addr, offset_reg, times_1, offset_imm);
  if (protected_store_pc) *protected_store_pc = pc_offset();

  switch (type.value()) {
    case StoreType::kI64Store8:
      src = src.low();
      V8_FALLTHROUGH;
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
    case StoreType::kI64Store16:
      src = src.low();
      V8_FALLTHROUGH;
    case StoreType::kI32Store16:
      mov_w(dst_op, src.gp());
      break;
    case StoreType::kI64Store32:
      src = src.low();
      V8_FALLTHROUGH;
    case StoreType::kI32Store:
      mov(dst_op, src.gp());
      break;
    case StoreType::kI64Store: {
      // Compute the operand for the store of the upper half.
      DCHECK(is_uint31(offset_imm + 4));
      Operand upper_dst_op =
          offset_reg == no_reg
              ? Operand(dst_addr, offset_imm + 4)
              : Operand(dst_addr, offset_reg, times_1, offset_imm + 4);
      // The high word has to be mov'ed first, such that this is the protected
      // instruction. The mov of the low word cannot segfault.
      mov(upper_dst_op, src.high_gp());
      mov(dst_op, src.low_gp());
      break;
    }
    case StoreType::kF32Store:
      movss(dst_op, src.fp());
      break;
    case StoreType::kF64Store:
      movsd(dst_op, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  Operand src(ebp, kPointerSize * (caller_slot_idx + 1));
  liftoff::Load(this, dst, src, type);
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index,
                                      ValueType type) {
  DCHECK_NE(dst_index, src_index);
  if (cache_state_.has_unused_register(kGpReg)) {
    LiftoffRegister reg = GetUnusedRegister(kGpReg);
    Fill(reg, src_index, type);
    Spill(dst_index, reg, type);
  } else {
    push(liftoff::GetStackSlot(src_index));
    pop(liftoff::GetStackSlot(dst_index));
  }
}

void LiftoffAssembler::MoveToReturnRegister(LiftoffRegister reg,
                                            ValueType type) {
  // TODO(wasm): Extract the destination register from the CallDescriptor.
  // TODO(wasm): Add multi-return support.
  LiftoffRegister dst = reg.is_pair() ? LiftoffRegister::ForPair(eax, edx)
                                      : reg.is_gp() ? LiftoffRegister(eax)
                                                    : LiftoffRegister(xmm1);
  if (reg != dst) Move(dst, reg, type);
}

void LiftoffAssembler::Move(Register dst, Register src, ValueType type) {
  DCHECK_NE(dst, src);
  DCHECK_EQ(kWasmI32, type);
  mov(dst, src);
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueType type) {
  DCHECK_NE(dst, src);
  if (type == kWasmF32) {
    movss(dst, src);
  } else {
    DCHECK_EQ(kWasmF64, type);
    movsd(dst, src);
  }
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg,
                             ValueType type) {
  RecordUsedSpillSlot(index);
  Operand dst = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      mov(dst, reg.gp());
      break;
    case kWasmI64:
      mov(dst, reg.low_gp());
      mov(liftoff::GetHalfStackSlot(2 * index - 1), reg.high_gp());
      break;
    case kWasmF32:
      movss(dst, reg.fp());
      break;
    case kWasmF64:
      movsd(dst, reg.fp());
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
      mov(dst, Immediate(value.to_i32()));
      break;
    case kWasmI64: {
      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      mov(dst, Immediate(low_word));
      mov(liftoff::GetHalfStackSlot(2 * index - 1), Immediate(high_word));
      break;
    }
    default:
      // We do not track f32 and f64 constants, hence they are unreachable.
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index,
                            ValueType type) {
  Operand src = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      mov(reg.gp(), src);
      break;
    case kWasmI64:
      mov(reg.low_gp(), src);
      mov(reg.high_gp(), liftoff::GetHalfStackSlot(2 * index - 1));
      break;
    case kWasmF32:
      movss(reg.fp(), src);
      break;
    case kWasmF64:
      movsd(reg.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register reg, uint32_t half_index) {
  mov(reg, liftoff::GetHalfStackSlot(half_index));
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
                               Register src, Register amount,
                               void (Assembler::*emit_shift)(Register),
                               LiftoffRegList pinned) {
  pinned.set(dst);
  pinned.set(src);
  pinned.set(amount);
  // If dst is ecx, compute into a tmp register first, then move to ecx.
  if (dst == ecx) {
    Register tmp = assm->GetUnusedRegister(kGpReg, pinned).gp();
    assm->mov(tmp, src);
    if (amount != ecx) assm->mov(ecx, amount);
    (assm->*emit_shift)(tmp);
    assm->mov(ecx, tmp);
    return;
  }

  // Move amount into ecx. If ecx is in use, move its content to a tmp register
  // first. If src is ecx, src is now the tmp register.
  Register tmp_reg = no_reg;
  if (amount != ecx) {
    if (assm->cache_state()->is_used(LiftoffRegister(ecx)) ||
        pinned.has(LiftoffRegister(ecx))) {
      tmp_reg = assm->GetUnusedRegister(kGpReg, pinned).gp();
      assm->mov(tmp_reg, ecx);
      if (src == ecx) src = tmp_reg;
    }
    assm->mov(ecx, amount);
  }

  // Do the actual shift.
  if (dst != src) assm->mov(dst, src);
  (assm->*emit_shift)(dst);

  // Restore ecx if needed.
  if (tmp_reg.is_valid()) assm->mov(ecx, tmp_reg);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_shl(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation(this, dst, src, amount, &Assembler::shl_cl,
                              pinned);
}

void LiftoffAssembler::emit_i32_sar(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation(this, dst, src, amount, &Assembler::sar_cl,
                              pinned);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation(this, dst, src, amount, &Assembler::shr_cl,
                              pinned);
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

namespace liftoff {
template <void (Assembler::*op)(Register, Register),
          void (Assembler::*op_with_carry)(Register, Register)>
inline void OpWithCarry(LiftoffAssembler* assm, LiftoffRegister dst,
                        LiftoffRegister lhs, LiftoffRegister rhs) {
  // First, compute the low half of the result, potentially into a temporary dst
  // register if {dst.low_gp()} equals {rhs.low_gp()} or any register we need to
  // keep alive for computing the upper half.
  LiftoffRegList keep_alive = LiftoffRegList::ForRegs(lhs.high_gp(), rhs);
  Register dst_low = keep_alive.has(dst.low_gp())
                         ? assm->GetUnusedRegister(kGpReg, keep_alive).gp()
                         : dst.low_gp();

  if (dst_low != lhs.low_gp()) assm->mov(dst_low, lhs.low_gp());
  (assm->*op)(dst_low, rhs.low_gp());

  // Now compute the upper half, while keeping alive the previous result.
  keep_alive = LiftoffRegList::ForRegs(dst_low, rhs.high_gp());
  Register dst_high = keep_alive.has(dst.high_gp())
                          ? assm->GetUnusedRegister(kGpReg, keep_alive).gp()
                          : dst.high_gp();

  if (dst_high != lhs.high_gp()) assm->mov(dst_high, lhs.high_gp());
  (assm->*op_with_carry)(dst_high, rhs.high_gp());

  // If necessary, move result into the right registers.
  LiftoffRegister tmp_result = LiftoffRegister::ForPair(dst_low, dst_high);
  if (tmp_result != dst) assm->Move(dst, tmp_result, kWasmI64);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::OpWithCarry<&Assembler::add, &Assembler::adc>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i64_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::OpWithCarry<&Assembler::sub, &Assembler::sbb>(this, dst, lhs, rhs);
}

namespace liftoff {
inline bool PairContains(LiftoffRegister pair, Register reg) {
  return pair.low_gp() == reg || pair.high_gp() == reg;
}

inline LiftoffRegister ReplaceInPair(LiftoffRegister pair, Register old_reg,
                                     Register new_reg) {
  if (pair.low_gp() == old_reg) {
    return LiftoffRegister::ForPair(new_reg, pair.high_gp());
  }
  if (pair.high_gp() == old_reg) {
    return LiftoffRegister::ForPair(pair.low_gp(), new_reg);
  }
  return pair;
}

inline void Emit64BitShiftOperation(
    LiftoffAssembler* assm, LiftoffRegister dst, LiftoffRegister src,
    Register amount, void (TurboAssembler::*emit_shift)(Register, Register),
    LiftoffRegList pinned) {
  pinned.set(dst);
  pinned.set(src);
  pinned.set(amount);
  // If {dst} contains {ecx}, replace it by an unused register, which is then
  // moved to {ecx} in the end.
  Register ecx_replace = no_reg;
  if (PairContains(dst, ecx)) {
    ecx_replace = pinned.set(assm->GetUnusedRegister(kGpReg, pinned)).gp();
    dst = ReplaceInPair(dst, ecx, ecx_replace);
    // If {amount} needs to be moved to {ecx}, but {ecx} is in use (and not part
    // of {dst}, hence overwritten anyway), move {ecx} to a tmp register and
    // restore it at the end.
  } else if (amount != ecx &&
             assm->cache_state()->is_used(LiftoffRegister(ecx))) {
    ecx_replace = assm->GetUnusedRegister(kGpReg, pinned).gp();
    assm->mov(ecx_replace, ecx);
  }

  assm->ParallelRegisterMove(
      {{dst, src, kWasmI64},
       {LiftoffRegister{ecx}, LiftoffRegister{amount}, kWasmI32}});

  // Do the actual shift.
  (assm->*emit_shift)(dst.high_gp(), dst.low_gp());

  // Restore {ecx} if needed.
  if (ecx_replace != no_reg) assm->mov(ecx, ecx_replace);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::ShlPair_cl, pinned);
}

void LiftoffAssembler::emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::SarPair_cl, pinned);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::ShrPair_cl, pinned);
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

void LiftoffAssembler::emit_f32_div(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vdivss(dst, lhs, rhs);
  } else if (dst == rhs) {
    movss(kScratchDoubleReg, rhs);
    movss(dst, lhs);
    divss(dst, kScratchDoubleReg);
  } else {
    if (dst != lhs) movss(dst, lhs);
    divss(dst, rhs);
  }
}

void LiftoffAssembler::emit_f32_abs(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint32_t kSignBit = uint32_t{1} << 31;
  if (dst == src) {
    TurboAssembler::Move(kScratchDoubleReg, kSignBit - 1);
    Andps(dst, kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit - 1);
    Andps(dst, src);
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

void LiftoffAssembler::emit_f32_ceil(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundUp);
}

void LiftoffAssembler::emit_f32_floor(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundDown);
}

void LiftoffAssembler::emit_f32_trunc(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundToZero);
}

void LiftoffAssembler::emit_f32_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundToNearest);
}

void LiftoffAssembler::emit_f32_sqrt(DoubleRegister dst, DoubleRegister src) {
  Sqrtss(dst, src);
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

void LiftoffAssembler::emit_f64_div(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vdivsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    movsd(kScratchDoubleReg, rhs);
    movsd(dst, lhs);
    divsd(dst, kScratchDoubleReg);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    divsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_abs(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint64_t kSignBit = uint64_t{1} << 63;
  if (dst == src) {
    TurboAssembler::Move(kScratchDoubleReg, kSignBit - 1);
    Andpd(dst, kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit - 1);
    Andpd(dst, src);
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

void LiftoffAssembler::emit_f64_ceil(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundsd(dst, src, kRoundUp);
}

void LiftoffAssembler::emit_f64_floor(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundsd(dst, src, kRoundDown);
}

void LiftoffAssembler::emit_f64_trunc(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundsd(dst, src, kRoundToZero);
}

void LiftoffAssembler::emit_f64_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  roundsd(dst, src, kRoundToNearest);
}

void LiftoffAssembler::emit_f64_sqrt(DoubleRegister dst, DoubleRegister src) {
  Sqrtsd(dst, src);
}

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src) {
  switch (opcode) {
    case kExprI32ConvertI64:
      if (dst.gp() != src.low_gp()) mov(dst.gp(), src.low_gp());
      return true;
    case kExprI32ReinterpretF32:
      Movd(dst.gp(), src.fp());
      return true;
    case kExprI64SConvertI32:
      if (dst.low_gp() != src.gp()) mov(dst.low_gp(), src.gp());
      mov(dst.high_gp(), src.gp());
      sar(dst.high_gp(), 31);
      return true;
    case kExprI64UConvertI32:
      if (dst.low_gp() != src.gp()) mov(dst.low_gp(), src.gp());
      xor_(dst.high_gp(), dst.high_gp());
      return true;
    case kExprI64ReinterpretF64:
      // Push src to the stack.
      sub(esp, Immediate(8));
      movsd(Operand(esp, 0), src.fp());
      // Pop to dst.
      pop(dst.low_gp());
      pop(dst.high_gp());
      return true;
    case kExprF32SConvertI32:
      cvtsi2ss(dst.fp(), src.gp());
      return true;
    case kExprF32UConvertI32: {
      LiftoffRegList pinned = LiftoffRegList::ForRegs(dst, src);
      Register scratch = GetUnusedRegister(kGpReg, pinned).gp();
      Cvtui2ss(dst.fp(), src.gp(), scratch);
      return true;
    }
    case kExprF32ConvertF64:
      cvtsd2ss(dst.fp(), src.fp());
      return true;
    case kExprF32ReinterpretI32:
      Movd(dst.fp(), src.gp());
      return true;
    case kExprF64SConvertI32:
      Cvtsi2sd(dst.fp(), src.gp());
      return true;
    case kExprF64UConvertI32:
      LoadUint32(dst.fp(), src.gp());
      return true;
    case kExprF64ConvertF32:
      cvtss2sd(dst.fp(), src.fp());
      return true;
    case kExprF64ReinterpretI64:
      // Push src to the stack.
      push(src.high_gp());
      push(src.low_gp());
      // Pop to dst.
      movsd(dst.fp(), Operand(esp, 0));
      add(esp, Immediate(8));
      return true;
    default:
      return false;
  }
}

void LiftoffAssembler::emit_jump(Label* label) { jmp(label); }

void LiftoffAssembler::emit_jump(Register target) { jmp(target); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  if (rhs != no_reg) {
    switch (type) {
      case kWasmI32:
        cmp(lhs, rhs);
        break;
      default:
        UNREACHABLE();
    }
  } else {
    DCHECK_EQ(type, kWasmI32);
    test(lhs, lhs);
  }

  j(cond, label);
}

namespace liftoff {

// Get a temporary byte register, using {candidate} if possible.
// Might spill, but always keeps status flags intact.
inline Register GetTmpByteRegister(LiftoffAssembler* assm, Register candidate) {
  if (candidate.is_byte_register()) return candidate;
  LiftoffRegList pinned = LiftoffRegList::ForRegs(candidate);
  // {GetUnusedRegister()} may insert move instructions to spill registers to
  // the stack. This is OK because {mov} does not change the status flags.
  return assm->GetUnusedRegister(liftoff::kByteRegs, pinned).gp();
}

// Setcc into dst register, given a scratch byte register (might be the same as
// dst). Never spills.
inline void setcc_32_no_spill(LiftoffAssembler* assm, Condition cond,
                              Register dst, Register tmp_byte_reg) {
  assm->setcc(cond, tmp_byte_reg);
  assm->movzx_b(dst, tmp_byte_reg);
}

// Setcc into dst register (no contraints). Might spill.
inline void setcc_32(LiftoffAssembler* assm, Condition cond, Register dst) {
  Register tmp_byte_reg = GetTmpByteRegister(assm, dst);
  setcc_32_no_spill(assm, cond, dst, tmp_byte_reg);
}

}  // namespace liftoff

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  test(src, src);
  liftoff::setcc_32(this, equal, dst);
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  cmp(lhs, rhs);
  liftoff::setcc_32(this, cond, dst);
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  // Compute the OR of both registers in the src pair, using dst as scratch
  // register. Then check whether the result is equal to zero.
  if (src.low_gp() == dst) {
    or_(dst, src.high_gp());
  } else {
    if (src.high_gp() != dst) mov(dst, src.high_gp());
    or_(dst, src.low_gp());
  }
  liftoff::setcc_32(this, equal, dst);
}

namespace liftoff {
inline Condition cond_make_unsigned(Condition cond) {
  switch (cond) {
    case kSignedLessThan:
      return kUnsignedLessThan;
    case kSignedLessEqual:
      return kUnsignedLessEqual;
    case kSignedGreaterThan:
      return kUnsignedGreaterThan;
    case kSignedGreaterEqual:
      return kUnsignedGreaterEqual;
    default:
      return cond;
  }
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_set_cond(Condition cond, Register dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  // Get the tmp byte register out here, such that we don't conditionally spill
  // (this cannot be reflected in the cache state).
  Register tmp_byte_reg = liftoff::GetTmpByteRegister(this, dst);

  // For signed i64 comparisons, we still need to use unsigned comparison for
  // the low word (the only bit carrying signedness information is the MSB in
  // the high word).
  Condition unsigned_cond = liftoff::cond_make_unsigned(cond);
  Label setcc;
  Label cont;
  // Compare high word first. If it differs, use if for the setcc. If it's
  // equal, compare the low word and use that for setcc.
  cmp(lhs.high_gp(), rhs.high_gp());
  j(not_equal, &setcc, Label::kNear);
  cmp(lhs.low_gp(), rhs.low_gp());
  if (unsigned_cond != cond) {
    // If the condition predicate for the low differs from that for the high
    // word, emit a separete setcc sequence for the low word.
    liftoff::setcc_32_no_spill(this, unsigned_cond, dst, tmp_byte_reg);
    jmp(&cont);
  }
  bind(&setcc);
  liftoff::setcc_32_no_spill(this, cond, dst, tmp_byte_reg);
  bind(&cont);
}

namespace liftoff {
template <void (Assembler::*cmp_op)(DoubleRegister, DoubleRegister)>
void EmitFloatSetCond(LiftoffAssembler* assm, Condition cond, Register dst,
                      DoubleRegister lhs, DoubleRegister rhs) {
  Label cont;
  Label not_nan;

  // Get the tmp byte register out here, such that we don't conditionally spill
  // (this cannot be reflected in the cache state).
  Register tmp_byte_reg = GetTmpByteRegister(assm, dst);

  (assm->*cmp_op)(lhs, rhs);
  // If PF is one, one of the operands was Nan. This needs special handling.
  assm->j(parity_odd, &not_nan, Label::kNear);
  // Return 1 for f32.ne, 0 for all other cases.
  if (cond == not_equal) {
    assm->mov(dst, Immediate(1));
  } else {
    assm->xor_(dst, dst);
  }
  assm->jmp(&cont, Label::kNear);
  assm->bind(&not_nan);

  setcc_32_no_spill(assm, cond, dst, tmp_byte_reg);
  assm->bind(&cont);
}
}  // namespace liftoff

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  liftoff::EmitFloatSetCond<&Assembler::ucomiss>(this, cond, dst, lhs, rhs);
}

void LiftoffAssembler::emit_f64_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  liftoff::EmitFloatSetCond<&Assembler::ucomisd>(this, cond, dst, lhs, rhs);
}

void LiftoffAssembler::StackCheck(Label* ool_code) {
  cmp(esp,
      Operand(Immediate(ExternalReference::address_of_stack_limit(isolate()))));
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
                                           uint32_t src_index,
                                           RegPairHalf half) {
  switch (src.loc()) {
    case VarState::kStack:
      if (src.type() == kWasmF64) {
        DCHECK_EQ(kLowWord, half);
        push(liftoff::GetHalfStackSlot(2 * src_index - 1));
      }
      push(liftoff::GetHalfStackSlot(2 * src_index -
                                     (half == kLowWord ? 0 : 1)));
      break;
    case VarState::kRegister:
      if (src.type() == kWasmI64) {
        PushCallerFrameSlot(
            half == kLowWord ? src.reg().low() : src.reg().high(), kWasmI32);
      } else {
        PushCallerFrameSlot(src.reg(), src.type());
      }
      break;
    case VarState::KIntConst:
      // The high word is the sign extension of the low word.
      push(Immediate(half == kLowWord ? src.i32_const()
                                      : src.i32_const() >> 31));
      break;
  }
}

void LiftoffAssembler::PushCallerFrameSlot(LiftoffRegister reg,
                                           ValueType type) {
  liftoff::push(this, reg, type);
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

void LiftoffAssembler::PrepareCCall(wasm::FunctionSig* sig,
                                    const LiftoffRegister* args,
                                    ValueType out_argument_type) {
  int pushed_bytes = 0;
  for (ValueType param_type : sig->parameters()) {
    pushed_bytes += RoundUp<kPointerSize>(WasmOpcodes::MemSize(param_type));
    liftoff::push(this, *args++, param_type);
  }
  if (out_argument_type != kWasmStmt) {
    int size = RoundUp<kPointerSize>(WasmOpcodes::MemSize(out_argument_type));
    sub(esp, Immediate(size));
    pushed_bytes += size;
  }
  // Save the original sp (before the first push), such that we can later
  // compute pointers to the pushed values. Do this only *after* pushing the
  // values, because {kCCallLastArgAddrReg} might collide with an arg register.
  lea(liftoff::kCCallLastArgAddrReg, Operand(esp, pushed_bytes));
  constexpr Register kScratch = ecx;
  static_assert(kScratch != liftoff::kCCallLastArgAddrReg, "collision");
  int num_c_call_arguments = static_cast<int>(sig->parameter_count()) +
                             (out_argument_type != kWasmStmt);
  PrepareCallCFunction(num_c_call_arguments, kScratch);
}

void LiftoffAssembler::SetCCallRegParamAddr(Register dst, int param_byte_offset,
                                            ValueType type) {
  // Check that we don't accidentally override kCCallLastArgAddrReg.
  DCHECK_NE(liftoff::kCCallLastArgAddrReg, dst);
  lea(dst, Operand(liftoff::kCCallLastArgAddrReg, -param_byte_offset));
}

void LiftoffAssembler::SetCCallStackParamAddr(int stack_param_idx,
                                              int param_byte_offset,
                                              ValueType type) {
  static constexpr Register kScratch = ecx;
  SetCCallRegParamAddr(kScratch, param_byte_offset, type);
  mov(Operand(esp, stack_param_idx * kPointerSize), kScratch);
}

void LiftoffAssembler::LoadCCallOutArgument(LiftoffRegister dst, ValueType type,
                                            int param_byte_offset) {
  // Check that we don't accidentally override kCCallLastArgAddrReg.
  DCHECK_NE(LiftoffRegister(liftoff::kCCallLastArgAddrReg), dst);
  Operand src(liftoff::kCCallLastArgAddrReg, -param_byte_offset);
  liftoff::Load(this, dst, src, type);
}

void LiftoffAssembler::CallC(ExternalReference ext_ref, uint32_t num_params) {
  CallCFunction(ext_ref, static_cast<int>(num_params));
}

void LiftoffAssembler::FinishCCall() {
  mov(esp, liftoff::kCCallLastArgAddrReg);
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  wasm_call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallRuntime(Zone* zone, Runtime::FunctionId fid) {
  // Set instance to zero.
  xor_(esi, esi);
  CallRuntimeDelayed(zone, fid);
}

void LiftoffAssembler::CallIndirect(wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  if (target == no_reg) {
    add(esp, Immediate(kPointerSize));
    call(Operand(esp, -4));
  } else {
    call(target);
  }
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  sub(esp, Immediate(size));
  mov(addr, esp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  add(esp, Immediate(size));
}

#undef REQUIRE_CPU_FEATURE

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_IA32_LIFTOFF_ASSEMBLER_IA32_H_

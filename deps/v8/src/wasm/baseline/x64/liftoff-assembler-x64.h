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

#define REQUIRE_CPU_FEATURE(name)                                   \
  if (!CpuFeatures::IsSupported(name)) return bailout("no " #name); \
  CpuFeatureScope feature(this, name);

namespace liftoff {

// rbp-8 holds the stack marker, rbp-16 is the instance parameter, first stack
// slot is located at rbp-24.
constexpr int32_t kConstantStackSpace = 16;
constexpr int32_t kFirstStackSlotOffset =
    kConstantStackSpace + LiftoffAssembler::kStackSlotSize;

inline Operand GetStackSlot(uint32_t index) {
  int32_t offset = index * LiftoffAssembler::kStackSlotSize;
  return Operand(rbp, -kFirstStackSlotOffset - offset);
}

// TODO(clemensh): Make this a constexpr variable once Operand is constexpr.
inline Operand GetInstanceOperand() { return Operand(rbp, -16); }

// Use this register to store the address of the last argument pushed on the
// stack for a call to C. This register must be callee saved according to the c
// calling convention.
static constexpr Register kCCallLastArgAddrReg = rbx;

inline Operand GetMemOp(LiftoffAssembler* assm, Register addr, Register offset,
                        uint32_t offset_imm, LiftoffRegList pinned) {
  // Wasm memory is limited to a size <2GB, so all offsets can be encoded as
  // immediate value (in 31 bits, interpreted as signed value).
  // If the offset is bigger, we always trap and this code is not reached.
  DCHECK(is_uint31(offset_imm));
  if (offset == no_reg) return Operand(addr, offset_imm);
  return Operand(addr, offset, times_1, offset_imm);
}

inline void Load(LiftoffAssembler* assm, LiftoffRegister dst, Operand src,
                 ValueType type) {
  switch (type) {
    case kWasmI32:
      assm->movl(dst.gp(), src);
      break;
    case kWasmI64:
      assm->movq(dst.gp(), src);
      break;
    case kWasmF32:
      assm->Movss(dst.fp(), src);
      break;
    case kWasmF64:
      assm->Movsd(dst.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueType type) {
  switch (type) {
    case kWasmI32:
    case kWasmI64:
      assm->pushq(reg.gp());
      break;
    case kWasmF32:
      assm->subp(rsp, Immediate(kPointerSize));
      assm->Movss(Operand(rsp, 0), reg.fp());
      break;
    case kWasmF64:
      assm->subp(rsp, Immediate(kPointerSize));
      assm->Movsd(Operand(rsp, 0), reg.fp());
      break;
    default:
      UNREACHABLE();
  }
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

void LiftoffAssembler::LoadFromInstance(Register dst, uint32_t offset,
                                        int size) {
  DCHECK_LE(offset, kMaxInt);
  movp(dst, liftoff::GetInstanceOperand());
  DCHECK(size == 4 || size == 8);
  if (size == 4) {
    movl(dst, Operand(dst, offset));
  } else {
    movq(dst, Operand(dst, offset));
  }
}

void LiftoffAssembler::SpillInstance(Register instance) {
  movp(liftoff::GetInstanceOperand(), instance);
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  movp(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc) {
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
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
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
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
      if (is_int32(value.to_i64())) {
        // Sign extend low word.
        movq(dst, Immediate(static_cast<int32_t>(value.to_i64())));
      } else if (is_uint32(value.to_i64())) {
        // Zero extend low word.
        movl(kScratchRegister, Immediate(static_cast<int32_t>(value.to_i64())));
        movq(dst, kScratchRegister);
      } else {
        movq(kScratchRegister, value.to_i64());
        movq(dst, kScratchRegister);
      }
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

namespace liftoff {
template <void (Assembler::*op)(Register, Register),
          void (Assembler::*mov)(Register, Register)>
void EmitCommutativeBinOp(LiftoffAssembler* assm, Register dst, Register lhs,
                          Register rhs) {
  if (dst == rhs) {
    (assm->*op)(dst, lhs);
  } else {
    if (dst != lhs) (assm->*mov)(dst, lhs);
    (assm->*op)(dst, rhs);
  }
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_mul(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::imull, &Assembler::movl>(this, dst,
                                                                     lhs, rhs);
}

void LiftoffAssembler::emit_i32_and(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::andl, &Assembler::movl>(this, dst,
                                                                    lhs, rhs);
}

void LiftoffAssembler::emit_i32_or(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::orl, &Assembler::movl>(this, dst,
                                                                   lhs, rhs);
}

void LiftoffAssembler::emit_i32_xor(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::xorl, &Assembler::movl>(this, dst,
                                                                    lhs, rhs);
}

namespace liftoff {
template <ValueType type>
inline void EmitShiftOperation(LiftoffAssembler* assm, Register dst,
                               Register src, Register amount,
                               void (Assembler::*emit_shift)(Register),
                               LiftoffRegList pinned) {
  // If dst is rcx, compute into the scratch register first, then move to rcx.
  if (dst == rcx) {
    assm->Move(kScratchRegister, src, type);
    if (amount != rcx) assm->Move(rcx, amount, type);
    (assm->*emit_shift)(kScratchRegister);
    assm->Move(rcx, kScratchRegister, type);
    return;
  }

  // Move amount into rcx. If rcx is in use, move its content into the scratch
  // register. If src is rcx, src is now the scratch register.
  bool use_scratch = false;
  if (amount != rcx) {
    use_scratch = src == rcx ||
                  assm->cache_state()->is_used(LiftoffRegister(rcx)) ||
                  pinned.has(LiftoffRegister(rcx));
    if (use_scratch) assm->movq(kScratchRegister, rcx);
    if (src == rcx) src = kScratchRegister;
    assm->Move(rcx, amount, type);
  }

  // Do the actual shift.
  if (dst != src) assm->Move(dst, src, type);
  (assm->*emit_shift)(dst);

  // Restore rcx if needed.
  if (use_scratch) assm->movq(rcx, kScratchRegister);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_shl(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI32>(this, dst, src, amount,
                                        &Assembler::shll_cl, pinned);
}

void LiftoffAssembler::emit_i32_sar(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI32>(this, dst, src, amount,
                                        &Assembler::sarl_cl, pinned);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI32>(this, dst, src, amount,
                                        &Assembler::shrl_cl, pinned);
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

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  if (lhs.gp() != dst.gp()) {
    leap(dst.gp(), Operand(lhs.gp(), rhs.gp(), times_1, 0));
  } else {
    addp(dst.gp(), rhs.gp());
  }
}

void LiftoffAssembler::emit_i64_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  if (dst.gp() == rhs.gp()) {
    negq(dst.gp());
    addq(dst.gp(), lhs.gp());
  } else {
    if (dst.gp() != lhs.gp()) movq(dst.gp(), lhs.gp());
    subq(dst.gp(), rhs.gp());
  }
}

void LiftoffAssembler::emit_i64_and(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::andq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), rhs.gp());
}

void LiftoffAssembler::emit_i64_or(LiftoffRegister dst, LiftoffRegister lhs,
                                   LiftoffRegister rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::orq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), rhs.gp());
}

void LiftoffAssembler::emit_i64_xor(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::xorq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), rhs.gp());
}

void LiftoffAssembler::emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI64>(this, dst.gp(), src.gp(), amount,
                                        &Assembler::shlq_cl, pinned);
}

void LiftoffAssembler::emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI64>(this, dst.gp(), src.gp(), amount,
                                        &Assembler::sarq_cl, pinned);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI64>(this, dst.gp(), src.gp(), amount,
                                        &Assembler::shrq_cl, pinned);
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
  Roundss(dst, src, kRoundUp);
}

void LiftoffAssembler::emit_f32_floor(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  Roundss(dst, src, kRoundDown);
}

void LiftoffAssembler::emit_f32_trunc(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  Roundss(dst, src, kRoundToZero);
}

void LiftoffAssembler::emit_f32_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  Roundss(dst, src, kRoundToNearest);
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
  Roundsd(dst, src, kRoundUp);
}

void LiftoffAssembler::emit_f64_floor(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  Roundsd(dst, src, kRoundDown);
}

void LiftoffAssembler::emit_f64_trunc(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  Roundsd(dst, src, kRoundToZero);
}

void LiftoffAssembler::emit_f64_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1);
  Roundsd(dst, src, kRoundToNearest);
}

void LiftoffAssembler::emit_f64_sqrt(DoubleRegister dst, DoubleRegister src) {
  Sqrtsd(dst, src);
}

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src) {
  switch (opcode) {
    case kExprI32ConvertI64:
      movl(dst.gp(), src.gp());
      return true;
    case kExprI32ReinterpretF32:
      Movd(dst.gp(), src.fp());
      return true;
    case kExprI64SConvertI32:
      movsxlq(dst.gp(), src.gp());
      return true;
    case kExprI64UConvertI32:
      AssertZeroExtended(src.gp());
      if (dst.gp() != src.gp()) movl(dst.gp(), src.gp());
      return true;
    case kExprI64ReinterpretF64:
      Movq(dst.gp(), src.fp());
      return true;
    case kExprF32SConvertI32:
      Cvtlsi2ss(dst.fp(), src.gp());
      return true;
    case kExprF32UConvertI32:
      movl(kScratchRegister, src.gp());
      Cvtqsi2ss(dst.fp(), kScratchRegister);
      return true;
    case kExprF32SConvertI64:
      Cvtqsi2ss(dst.fp(), src.gp());
      return true;
    case kExprF32UConvertI64:
      Cvtqui2ss(dst.fp(), src.gp(), kScratchRegister);
      return true;
    case kExprF32ConvertF64:
      Cvtsd2ss(dst.fp(), src.fp());
      return true;
    case kExprF32ReinterpretI32:
      Movd(dst.fp(), src.gp());
      return true;
    case kExprF64SConvertI32:
      Cvtlsi2sd(dst.fp(), src.gp());
      return true;
    case kExprF64UConvertI32:
      movl(kScratchRegister, src.gp());
      Cvtqsi2sd(dst.fp(), kScratchRegister);
      return true;
    case kExprF64SConvertI64:
      Cvtqsi2sd(dst.fp(), src.gp());
      return true;
    case kExprF64UConvertI64:
      Cvtqui2sd(dst.fp(), src.gp(), kScratchRegister);
      return true;
    case kExprF64ConvertF32:
      Cvtss2sd(dst.fp(), src.fp());
      return true;
    case kExprF64ReinterpretI64:
      Movq(dst.fp(), src.gp());
      return true;
    default:
      UNREACHABLE();
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

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  testl(src, src);
  setcc(equal, dst);
  movzxbl(dst, dst);
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  cmpl(lhs, rhs);
  setcc(cond, dst);
  movzxbl(dst, dst);
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  testq(src.gp(), src.gp());
  setcc(equal, dst);
  movzxbl(dst, dst);
}

void LiftoffAssembler::emit_i64_set_cond(Condition cond, Register dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  cmpq(lhs.gp(), rhs.gp());
  setcc(cond, dst);
  movzxbl(dst, dst);
}

namespace liftoff {
template <void (TurboAssembler::*cmp_op)(DoubleRegister, DoubleRegister)>
void EmitFloatSetCond(LiftoffAssembler* assm, Condition cond, Register dst,
                      DoubleRegister lhs, DoubleRegister rhs) {
  Label cont;
  Label not_nan;

  (assm->*cmp_op)(lhs, rhs);
  // IF PF is one, one of the operands was Nan. This needs special handling.
  assm->j(parity_odd, &not_nan, Label::kNear);
  // Return 1 for f32.ne, 0 for all other cases.
  if (cond == not_equal) {
    assm->movl(dst, Immediate(1));
  } else {
    assm->xorl(dst, dst);
  }
  assm->jmp(&cont, Label::kNear);
  assm->bind(&not_nan);

  assm->setcc(cond, dst);
  assm->movzxbl(dst, dst);
  assm->bind(&cont);
}
}  // namespace liftoff

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  liftoff::EmitFloatSetCond<&TurboAssembler::Ucomiss>(this, cond, dst, lhs,
                                                      rhs);
}

void LiftoffAssembler::emit_f64_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  liftoff::EmitFloatSetCond<&TurboAssembler::Ucomisd>(this, cond, dst, lhs,
                                                      rhs);
}

void LiftoffAssembler::StackCheck(Label* ool_code) {
  Operand stack_limit = ExternalOperand(
      ExternalReference::address_of_stack_limit(isolate()), kScratchRegister);
  cmpp(rsp, stack_limit);
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
  liftoff::push(this, reg, type);
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

void LiftoffAssembler::PrepareCCall(wasm::FunctionSig* sig,
                                    const LiftoffRegister* args,
                                    ValueType out_argument_type) {
  for (ValueType param_type : sig->parameters()) {
    liftoff::push(this, *args++, param_type);
  }
  if (out_argument_type != kWasmStmt) {
    subq(rsp, Immediate(kPointerSize));
  }
  // Save the original sp (before the first push), such that we can later
  // compute pointers to the pushed values. Do this only *after* pushing the
  // values, because {kCCallLastArgAddrReg} might collide with an arg register.
  int num_c_call_arguments = static_cast<int>(sig->parameter_count()) +
                             (out_argument_type != kWasmStmt);
  int pushed_bytes = kPointerSize * num_c_call_arguments;
  leaq(liftoff::kCCallLastArgAddrReg, Operand(rsp, pushed_bytes));
  PrepareCallCFunction(num_c_call_arguments);
}

void LiftoffAssembler::SetCCallRegParamAddr(Register dst, int param_byte_offset,
                                            ValueType type) {
  // Check that we don't accidentally override kCCallLastArgAddrReg.
  DCHECK_NE(liftoff::kCCallLastArgAddrReg, dst);
  leaq(dst, Operand(liftoff::kCCallLastArgAddrReg, -param_byte_offset));
}

void LiftoffAssembler::SetCCallStackParamAddr(int stack_param_idx,
                                              int param_byte_offset,
                                              ValueType type) {
  // On x64, all C call arguments fit in registers.
  UNREACHABLE();
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
  movp(rsp, liftoff::kCCallLastArgAddrReg);
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  near_call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallRuntime(Zone* zone, Runtime::FunctionId fid) {
  // Set instance to zero.
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

#undef REQUIRE_CPU_FEATURE

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_X64_LIFTOFF_ASSEMBLER_X64_H_

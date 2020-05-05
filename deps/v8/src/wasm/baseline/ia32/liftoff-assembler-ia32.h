// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_IA32_LIFTOFF_ASSEMBLER_IA32_H_
#define V8_WASM_BASELINE_IA32_LIFTOFF_ASSEMBLER_IA32_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#include "src/codegen/assembler.h"
#include "src/wasm/value-type.h"

namespace v8 {
namespace internal {
namespace wasm {

#define RETURN_FALSE_IF_MISSING_CPU_FEATURE(name)    \
  if (!CpuFeatures::IsSupported(name)) return false; \
  CpuFeatureScope feature(this, name);

namespace liftoff {

// ebp-4 holds the stack marker, ebp-8 is the instance parameter.
constexpr int kInstanceOffset = 8;

inline Operand GetStackSlot(int offset) { return Operand(ebp, -offset); }

inline MemOperand GetHalfStackSlot(int offset, RegPairHalf half) {
  int32_t half_offset =
      half == kLowWord ? 0 : LiftoffAssembler::kStackSlotSize / 2;
  return Operand(ebp, -offset + half_offset);
}

// TODO(clemensb): Make this a constexpr variable once Operand is constexpr.
inline Operand GetInstanceOperand() { return GetStackSlot(kInstanceOffset); }

static constexpr LiftoffRegList kByteRegs =
    LiftoffRegList::FromBits<Register::ListOf(eax, ecx, edx)>();

inline void Load(LiftoffAssembler* assm, LiftoffRegister dst, Register base,
                 int32_t offset, ValueType type) {
  Operand src(base, offset);
  switch (type.kind()) {
    case ValueType::kI32:
      assm->mov(dst.gp(), src);
      break;
    case ValueType::kI64:
      assm->mov(dst.low_gp(), src);
      assm->mov(dst.high_gp(), Operand(base, offset + 4));
      break;
    case ValueType::kF32:
      assm->movss(dst.fp(), src);
      break;
    case ValueType::kF64:
      assm->movsd(dst.fp(), src);
      break;
    case ValueType::kS128:
      assm->movdqu(dst.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

inline void Store(LiftoffAssembler* assm, Register base, int32_t offset,
                  LiftoffRegister src, ValueType type) {
  Operand dst(base, offset);
  switch (type.kind()) {
    case ValueType::kI32:
      assm->mov(dst, src.gp());
      break;
    case ValueType::kI64:
      assm->mov(dst, src.low_gp());
      assm->mov(Operand(base, offset + 4), src.high_gp());
      break;
    case ValueType::kF32:
      assm->movss(dst, src.fp());
      break;
    case ValueType::kF64:
      assm->movsd(dst, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueType type) {
  switch (type.kind()) {
    case ValueType::kI32:
      assm->push(reg.gp());
      break;
    case ValueType::kI64:
      assm->push(reg.high_gp());
      assm->push(reg.low_gp());
      break;
    case ValueType::kF32:
      assm->AllocateStackSpace(sizeof(float));
      assm->movss(Operand(esp, 0), reg.fp());
      break;
    case ValueType::kF64:
      assm->AllocateStackSpace(sizeof(double));
      assm->movsd(Operand(esp, 0), reg.fp());
      break;
    case ValueType::kS128:
      assm->AllocateStackSpace(sizeof(double) * 2);
      assm->movdqu(Operand(esp, 0), reg.fp());
      break;
    default:
      UNREACHABLE();
  }
}

template <typename... Regs>
inline void SpillRegisters(LiftoffAssembler* assm, Regs... regs) {
  for (LiftoffRegister r : {LiftoffRegister(regs)...}) {
    if (assm->cache_state()->is_used(r)) assm->SpillRegister(r);
  }
}

inline void SignExtendI32ToI64(Assembler* assm, LiftoffRegister reg) {
  assm->mov(reg.high_gp(), reg.low_gp());
  assm->sar(reg.high_gp(), 31);
}

// Get a temporary byte register, using {candidate} if possible.
// Might spill, but always keeps status flags intact.
inline Register GetTmpByteRegister(LiftoffAssembler* assm, Register candidate) {
  if (candidate.is_byte_register()) return candidate;
  // {GetUnusedRegister()} may insert move instructions to spill registers to
  // the stack. This is OK because {mov} does not change the status flags.
  return assm->GetUnusedRegister(liftoff::kByteRegs).gp();
}

inline void MoveStackValue(LiftoffAssembler* assm, const Operand& src,
                           const Operand& dst) {
  if (assm->cache_state()->has_unused_register(kGpReg)) {
    Register tmp = assm->cache_state()->unused_register(kGpReg).gp();
    assm->mov(tmp, src);
    assm->mov(dst, tmp);
  } else {
    // No free register, move via the stack.
    assm->push(src);
    assm->pop(dst);
  }
}

constexpr DoubleRegister kScratchDoubleReg = xmm7;

constexpr int kSubSpSize = 6;  // 6 bytes for "sub esp, <imm32>"

}  // namespace liftoff

int LiftoffAssembler::PrepareStackFrame() {
  int offset = pc_offset();
  sub_sp_32(0);
  DCHECK_EQ(liftoff::kSubSpSize, pc_offset() - offset);
  return offset;
}

void LiftoffAssembler::PatchPrepareStackFrame(int offset, int frame_size) {
  DCHECK_EQ(frame_size % kSystemPointerSize, 0);
  // We can't run out of space, just pass anything big enough to not cause the
  // assembler to try to grow the buffer.
  constexpr int kAvailableSpace = 64;
  Assembler patching_assembler(
      AssemblerOptions{},
      ExternalAssemblerBuffer(buffer_start_ + offset, kAvailableSpace));
#if V8_OS_WIN
  if (frame_size > kStackPageSize) {
    // Generate OOL code (at the end of the function, where the current
    // assembler is pointing) to do the explicit stack limit check (see
    // https://docs.microsoft.com/en-us/previous-versions/visualstudio/
    // visual-studio-6.0/aa227153(v=vs.60)).
    // At the function start, emit a jump to that OOL code (from {offset} to
    // {pc_offset()}).
    int ool_offset = pc_offset() - offset;
    patching_assembler.jmp_rel(ool_offset);
    DCHECK_GE(liftoff::kSubSpSize, patching_assembler.pc_offset());
    patching_assembler.Nop(liftoff::kSubSpSize -
                           patching_assembler.pc_offset());

    // Now generate the OOL code.
    AllocateStackSpace(frame_size);
    // Jump back to the start of the function (from {pc_offset()} to {offset +
    // kSubSpSize}).
    int func_start_offset = offset + liftoff::kSubSpSize - pc_offset();
    jmp_rel(func_start_offset);
    return;
  }
#endif
  patching_assembler.sub_sp_32(frame_size);
  DCHECK_EQ(liftoff::kSubSpSize, patching_assembler.pc_offset());
}

void LiftoffAssembler::FinishCode() {}

void LiftoffAssembler::AbortCompilation() {}

// static
constexpr int LiftoffAssembler::StaticStackFrameSize() {
  return liftoff::kInstanceOffset;
}

int LiftoffAssembler::SlotSizeForType(ValueType type) {
  return type.element_size_bytes();
}

bool LiftoffAssembler::NeedsAlignment(ValueType type) { return false; }

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  switch (value.type().kind()) {
    case ValueType::kI32:
      TurboAssembler::Move(reg.gp(), Immediate(value.to_i32(), rmode));
      break;
    case ValueType::kI64: {
      DCHECK(RelocInfo::IsNone(rmode));
      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      TurboAssembler::Move(reg.low_gp(), Immediate(low_word));
      TurboAssembler::Move(reg.high_gp(), Immediate(high_word));
      break;
    }
    case ValueType::kF32:
      TurboAssembler::Move(reg.fp(), value.to_f32_boxed().get_bits());
      break;
    case ValueType::kF64:
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

void LiftoffAssembler::LoadTaggedPointerFromInstance(Register dst,
                                                     uint32_t offset) {
  LoadFromInstance(dst, offset, kTaggedSize);
}

void LiftoffAssembler::SpillInstance(Register instance) {
  mov(liftoff::GetInstanceOperand(), instance);
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  mov(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         uint32_t offset_imm,
                                         LiftoffRegList pinned) {
  STATIC_ASSERT(kTaggedSize == kInt32Size);
  Load(LiftoffRegister(dst), src_addr, offset_reg, offset_imm,
       LoadType::kI32Load, pinned);
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc, bool is_load_mem) {
  DCHECK_EQ(type.value_type() == kWasmI64, dst.is_gp_pair());
  DCHECK_LE(offset_imm, std::numeric_limits<int32_t>::max());
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
      liftoff::SignExtendI32ToI64(this, dst);
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
      liftoff::SignExtendI32ToI64(this, dst);
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
      liftoff::SignExtendI32ToI64(this, dst);
      break;
    case LoadType::kI64Load: {
      // Compute the operand for the load of the upper half.
      Operand upper_src_op =
          offset_reg == no_reg
              ? Operand(src_addr, bit_cast<int32_t>(offset_imm + 4))
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
    case LoadType::kS128Load:
      movdqu(dst.fp(), src_op);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  DCHECK_EQ(type.value_type() == kWasmI64, src.is_gp_pair());
  DCHECK_LE(offset_imm, std::numeric_limits<int32_t>::max());
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
        // We know that {src} is not a byte register, so the only pinned byte
        // registers (beside the outer {pinned}) are {dst_addr} and potentially
        // {offset_reg}.
        LiftoffRegList pinned_byte = pinned | LiftoffRegList::ForRegs(dst_addr);
        if (offset_reg != no_reg) pinned_byte.set(offset_reg);
        Register byte_src =
            GetUnusedRegister(liftoff::kByteRegs, pinned_byte).gp();
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
      Operand upper_dst_op =
          offset_reg == no_reg
              ? Operand(dst_addr, bit_cast<int32_t>(offset_imm + 4))
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
    case StoreType::kS128Store:
      Movdqu(dst_op, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicLoad(LiftoffRegister dst, Register src_addr,
                                  Register offset_reg, uint32_t offset_imm,
                                  LoadType type, LiftoffRegList pinned) {
  if (type.value() != LoadType::kI64Load) {
    Load(dst, src_addr, offset_reg, offset_imm, type, pinned, nullptr, true);
    return;
  }

  DCHECK_EQ(type.value_type() == kWasmI64, dst.is_gp_pair());
  DCHECK_LE(offset_imm, std::numeric_limits<int32_t>::max());
  Operand src_op = offset_reg == no_reg
                       ? Operand(src_addr, offset_imm)
                       : Operand(src_addr, offset_reg, times_1, offset_imm);

  movsd(liftoff::kScratchDoubleReg, src_op);
  Pextrd(dst.low().gp(), liftoff::kScratchDoubleReg, 0);
  Pextrd(dst.high().gp(), liftoff::kScratchDoubleReg, 1);
}

void LiftoffAssembler::AtomicStore(Register dst_addr, Register offset_reg,
                                   uint32_t offset_imm, LiftoffRegister src,
                                   StoreType type, LiftoffRegList pinned) {
  DCHECK_NE(offset_reg, no_reg);
  DCHECK_LE(offset_imm, std::numeric_limits<int32_t>::max());
  Operand dst_op = Operand(dst_addr, offset_reg, times_1, offset_imm);

  // i64 store uses a totally different approach, hence implement it separately.
  if (type.value() == StoreType::kI64Store) {
    auto scratch2 = GetUnusedRegister(kFpReg, pinned).fp();
    movd(liftoff::kScratchDoubleReg, src.low().gp());
    movd(scratch2, src.high().gp());
    Punpckldq(liftoff::kScratchDoubleReg, scratch2);
    movsd(dst_op, liftoff::kScratchDoubleReg);
    // This lock+or is needed to achieve sequential consistency.
    lock();
    or_(Operand(esp, 0), Immediate(0));
    return;
  }

  // Other i64 stores actually only use the low word.
  if (src.is_pair()) src = src.low();
  Register src_gp = src.gp();

  bool is_byte_store = type.size() == 1;
  LiftoffRegList src_candidates =
      is_byte_store ? liftoff::kByteRegs : kGpCacheRegList;
  pinned = pinned | LiftoffRegList::ForRegs(dst_addr, src, offset_reg);

  // Ensure that {src} is a valid and otherwise unused register.
  if (!src_candidates.has(src) || cache_state_.is_used(src)) {
    // If there are no unused candidate registers, but {src} is a candidate,
    // then spill other uses of {src}. Otherwise spill any candidate register
    // and use that.
    if (!cache_state_.has_unused_register(src_candidates, pinned) &&
        src_candidates.has(src)) {
      SpillRegister(src);
    } else {
      Register safe_src = GetUnusedRegister(src_candidates, pinned).gp();
      mov(safe_src, src_gp);
      src_gp = safe_src;
    }
  }

  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      xchg_b(src_gp, dst_op);
      return;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      xchg_w(src_gp, dst_op);
      return;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      xchg(src_gp, dst_op);
      return;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicAdd(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 StoreType type) {
  bailout(kAtomics, "AtomicAdd");
}

void LiftoffAssembler::AtomicSub(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 StoreType type) {
  bailout(kAtomics, "AtomicSub");
}

void LiftoffAssembler::AtomicAnd(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 StoreType type) {
  bailout(kAtomics, "AtomicAnd");
}

void LiftoffAssembler::AtomicOr(Register dst_addr, Register offset_reg,
                                uint32_t offset_imm, LiftoffRegister value,
                                StoreType type) {
  bailout(kAtomics, "AtomicOr");
}

void LiftoffAssembler::AtomicXor(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 StoreType type) {
  bailout(kAtomics, "AtomicXor");
}

void LiftoffAssembler::AtomicExchange(Register dst_addr, Register offset_reg,
                                      uint32_t offset_imm,
                                      LiftoffRegister value, StoreType type) {
  bailout(kAtomics, "AtomicExchange");
}

void LiftoffAssembler::AtomicCompareExchange(
    Register dst_addr, Register offset_reg, uint32_t offset_imm,
    LiftoffRegister expected, LiftoffRegister new_value, LiftoffRegister result,
    StoreType type) {
  bailout(kAtomics, "AtomicCompareExchange");
}

void LiftoffAssembler::AtomicFence() { mfence(); }

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  liftoff::Load(this, dst, ebp, kSystemPointerSize * (caller_slot_idx + 1),
                type);
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_offset, uint32_t src_offset,
                                      ValueType type) {
  if (needs_gp_reg_pair(type)) {
    liftoff::MoveStackValue(this,
                            liftoff::GetHalfStackSlot(src_offset, kLowWord),
                            liftoff::GetHalfStackSlot(dst_offset, kLowWord));
    liftoff::MoveStackValue(this,
                            liftoff::GetHalfStackSlot(src_offset, kHighWord),
                            liftoff::GetHalfStackSlot(dst_offset, kHighWord));
  } else {
    liftoff::MoveStackValue(this, liftoff::GetStackSlot(src_offset),
                            liftoff::GetStackSlot(dst_offset));
  }
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
  } else if (type == kWasmF64) {
    movsd(dst, src);
  } else {
    DCHECK_EQ(kWasmS128, type);
    movapd(dst, src);
  }
}

void LiftoffAssembler::Spill(int offset, LiftoffRegister reg, ValueType type) {
  RecordUsedSpillOffset(offset);
  Operand dst = liftoff::GetStackSlot(offset);
  switch (type.kind()) {
    case ValueType::kI32:
      mov(dst, reg.gp());
      break;
    case ValueType::kI64:
      mov(liftoff::GetHalfStackSlot(offset, kLowWord), reg.low_gp());
      mov(liftoff::GetHalfStackSlot(offset, kHighWord), reg.high_gp());
      break;
    case ValueType::kF32:
      movss(dst, reg.fp());
      break;
    case ValueType::kF64:
      movsd(dst, reg.fp());
      break;
    case ValueType::kS128:
      movdqu(dst, reg.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(int offset, WasmValue value) {
  RecordUsedSpillOffset(offset);
  Operand dst = liftoff::GetStackSlot(offset);
  switch (value.type().kind()) {
    case ValueType::kI32:
      mov(dst, Immediate(value.to_i32()));
      break;
    case ValueType::kI64: {
      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      mov(liftoff::GetHalfStackSlot(offset, kLowWord), Immediate(low_word));
      mov(liftoff::GetHalfStackSlot(offset, kHighWord), Immediate(high_word));
      break;
    }
    default:
      // We do not track f32 and f64 constants, hence they are unreachable.
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, int offset, ValueType type) {
  Operand src = liftoff::GetStackSlot(offset);
  switch (type.kind()) {
    case ValueType::kI32:
      mov(reg.gp(), src);
      break;
    case ValueType::kI64:
      mov(reg.low_gp(), liftoff::GetHalfStackSlot(offset, kLowWord));
      mov(reg.high_gp(), liftoff::GetHalfStackSlot(offset, kHighWord));
      break;
    case ValueType::kF32:
      movss(reg.fp(), src);
      break;
    case ValueType::kF64:
      movsd(reg.fp(), src);
      break;
    case ValueType::kS128:
      movdqu(reg.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register reg, int offset, RegPairHalf half) {
  mov(reg, liftoff::GetHalfStackSlot(offset, half));
}

void LiftoffAssembler::FillStackSlotsWithZero(int start, int size) {
  DCHECK_LT(0, size);
  DCHECK_EQ(0, size % 4);
  RecordUsedSpillOffset(start + size);

  if (size <= 12) {
    // Special straight-line code for up to three words (6-9 bytes per word:
    // C7 <1-4 bytes operand> <4 bytes imm>, makes 18-27 bytes total).
    for (int offset = 4; offset <= size; offset += 4) {
      mov(liftoff::GetHalfStackSlot(start + offset, kLowWord), Immediate(0));
    }
  } else {
    // General case for bigger counts.
    // This sequence takes 19-22 bytes (3 for pushes, 3-6 for lea, 2 for xor, 5
    // for mov, 3 for repstosq, 3 for pops).
    // Note: rep_stos fills ECX doublewords at [EDI] with EAX.
    push(eax);
    push(ecx);
    push(edi);
    lea(edi, liftoff::GetStackSlot(start + size));
    xor_(eax, eax);
    // Size is in bytes, convert to doublewords (4-bytes).
    mov(ecx, Immediate(size / 4));
    rep_stos();
    pop(edi);
    pop(ecx);
    pop(eax);
  }
}

void LiftoffAssembler::emit_i32_add(Register dst, Register lhs, Register rhs) {
  if (lhs != dst) {
    lea(dst, Operand(lhs, rhs, times_1, 0));
  } else {
    add(dst, rhs);
  }
}

void LiftoffAssembler::emit_i32_add(Register dst, Register lhs, int32_t imm) {
  if (lhs != dst) {
    lea(dst, Operand(lhs, imm));
  } else {
    add(dst, Immediate(imm));
  }
}

void LiftoffAssembler::emit_i32_sub(Register dst, Register lhs, Register rhs) {
  if (dst != rhs) {
    // Default path.
    if (dst != lhs) mov(dst, lhs);
    sub(dst, rhs);
  } else if (lhs == rhs) {
    // Degenerate case.
    xor_(dst, dst);
  } else {
    // Emit {dst = lhs + -rhs} if dst == rhs.
    neg(dst);
    add(dst, lhs);
  }
}

namespace liftoff {
template <void (Assembler::*op)(Register, Register)>
void EmitCommutativeBinOp(LiftoffAssembler* assm, Register dst, Register lhs,
                          Register rhs) {
  if (dst == rhs) {
    (assm->*op)(dst, lhs);
  } else {
    if (dst != lhs) assm->mov(dst, lhs);
    (assm->*op)(dst, rhs);
  }
}

template <void (Assembler::*op)(Register, int32_t)>
void EmitCommutativeBinOpImm(LiftoffAssembler* assm, Register dst, Register lhs,
                             int32_t imm) {
  if (dst != lhs) assm->mov(dst, lhs);
  (assm->*op)(dst, imm);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_mul(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::imul>(this, dst, lhs, rhs);
}

namespace liftoff {
enum class DivOrRem : uint8_t { kDiv, kRem };
template <bool is_signed, DivOrRem div_or_rem>
void EmitInt32DivOrRem(LiftoffAssembler* assm, Register dst, Register lhs,
                       Register rhs, Label* trap_div_by_zero,
                       Label* trap_div_unrepresentable) {
  constexpr bool needs_unrepresentable_check =
      is_signed && div_or_rem == DivOrRem::kDiv;
  constexpr bool special_case_minus_1 =
      is_signed && div_or_rem == DivOrRem::kRem;
  DCHECK_EQ(needs_unrepresentable_check, trap_div_unrepresentable != nullptr);

  // For division, the lhs is always taken from {edx:eax}. Thus, make sure that
  // these registers are unused. If {rhs} is stored in one of them, move it to
  // another temporary register.
  // Do all this before any branch, such that the code is executed
  // unconditionally, as the cache state will also be modified unconditionally.
  liftoff::SpillRegisters(assm, eax, edx);
  if (rhs == eax || rhs == edx) {
    LiftoffRegList unavailable = LiftoffRegList::ForRegs(eax, edx, lhs);
    Register tmp = assm->GetUnusedRegister(kGpReg, unavailable).gp();
    assm->mov(tmp, rhs);
    rhs = tmp;
  }

  // Check for division by zero.
  assm->test(rhs, rhs);
  assm->j(zero, trap_div_by_zero);

  Label done;
  if (needs_unrepresentable_check) {
    // Check for {kMinInt / -1}. This is unrepresentable.
    Label do_div;
    assm->cmp(rhs, -1);
    assm->j(not_equal, &do_div);
    assm->cmp(lhs, kMinInt);
    assm->j(equal, trap_div_unrepresentable);
    assm->bind(&do_div);
  } else if (special_case_minus_1) {
    // {lhs % -1} is always 0 (needs to be special cased because {kMinInt / -1}
    // cannot be computed).
    Label do_rem;
    assm->cmp(rhs, -1);
    assm->j(not_equal, &do_rem);
    assm->xor_(dst, dst);
    assm->jmp(&done);
    assm->bind(&do_rem);
  }

  // Now move {lhs} into {eax}, then zero-extend or sign-extend into {edx}, then
  // do the division.
  if (lhs != eax) assm->mov(eax, lhs);
  if (is_signed) {
    assm->cdq();
    assm->idiv(rhs);
  } else {
    assm->xor_(edx, edx);
    assm->div(rhs);
  }

  // Move back the result (in {eax} or {edx}) into the {dst} register.
  constexpr Register kResultReg = div_or_rem == DivOrRem::kDiv ? eax : edx;
  if (dst != kResultReg) assm->mov(dst, kResultReg);
  if (special_case_minus_1) assm->bind(&done);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  liftoff::EmitInt32DivOrRem<true, liftoff::DivOrRem::kDiv>(
      this, dst, lhs, rhs, trap_div_by_zero, trap_div_unrepresentable);
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitInt32DivOrRem<false, liftoff::DivOrRem::kDiv>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitInt32DivOrRem<true, liftoff::DivOrRem::kRem>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitInt32DivOrRem<false, liftoff::DivOrRem::kRem>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_and(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::and_>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_and(Register dst, Register lhs, int32_t imm) {
  liftoff::EmitCommutativeBinOpImm<&Assembler::and_>(this, dst, lhs, imm);
}

void LiftoffAssembler::emit_i32_or(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::or_>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_or(Register dst, Register lhs, int32_t imm) {
  liftoff::EmitCommutativeBinOpImm<&Assembler::or_>(this, dst, lhs, imm);
}

void LiftoffAssembler::emit_i32_xor(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::xor_>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_xor(Register dst, Register lhs, int32_t imm) {
  liftoff::EmitCommutativeBinOpImm<&Assembler::xor_>(this, dst, lhs, imm);
}

namespace liftoff {
inline void EmitShiftOperation(LiftoffAssembler* assm, Register dst,
                               Register src, Register amount,
                               void (Assembler::*emit_shift)(Register)) {
  LiftoffRegList pinned = LiftoffRegList::ForRegs(dst, src, amount);
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

void LiftoffAssembler::emit_i32_shl(Register dst, Register src,
                                    Register amount) {
  liftoff::EmitShiftOperation(this, dst, src, amount, &Assembler::shl_cl);
}

void LiftoffAssembler::emit_i32_shl(Register dst, Register src,
                                    int32_t amount) {
  if (dst != src) mov(dst, src);
  shl(dst, amount & 31);
}

void LiftoffAssembler::emit_i32_sar(Register dst, Register src,
                                    Register amount) {
  liftoff::EmitShiftOperation(this, dst, src, amount, &Assembler::sar_cl);
}

void LiftoffAssembler::emit_i32_sar(Register dst, Register src,
                                    int32_t amount) {
  if (dst != src) mov(dst, src);
  sar(dst, amount & 31);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register src,
                                    Register amount) {
  liftoff::EmitShiftOperation(this, dst, src, amount, &Assembler::shr_cl);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register src,
                                    int32_t amount) {
  if (dst != src) mov(dst, src);
  shr(dst, amount & 31);
}

void LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  Lzcnt(dst, src);
}

void LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  Tzcnt(dst, src);
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

template <void (Assembler::*op)(Register, const Immediate&),
          void (Assembler::*op_with_carry)(Register, int32_t)>
inline void OpWithCarryI(LiftoffAssembler* assm, LiftoffRegister dst,
                         LiftoffRegister lhs, int32_t imm) {
  // First, compute the low half of the result, potentially into a temporary dst
  // register if {dst.low_gp()} equals any register we need to
  // keep alive for computing the upper half.
  LiftoffRegList keep_alive = LiftoffRegList::ForRegs(lhs.high_gp());
  Register dst_low = keep_alive.has(dst.low_gp())
                         ? assm->GetUnusedRegister(kGpReg, keep_alive).gp()
                         : dst.low_gp();

  if (dst_low != lhs.low_gp()) assm->mov(dst_low, lhs.low_gp());
  (assm->*op)(dst_low, Immediate(imm));

  // Now compute the upper half, while keeping alive the previous result.
  keep_alive = LiftoffRegList::ForRegs(dst_low);
  Register dst_high = keep_alive.has(dst.high_gp())
                          ? assm->GetUnusedRegister(kGpReg, keep_alive).gp()
                          : dst.high_gp();

  if (dst_high != lhs.high_gp()) assm->mov(dst_high, lhs.high_gp());
  // Top half of the immediate sign extended, either 0 or -1.
  int32_t sign_extend = imm < 0 ? -1 : 0;
  (assm->*op_with_carry)(dst_high, sign_extend);

  // If necessary, move result into the right registers.
  LiftoffRegister tmp_result = LiftoffRegister::ForPair(dst_low, dst_high);
  if (tmp_result != dst) assm->Move(dst, tmp_result, kWasmI64);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::OpWithCarry<&Assembler::add, &Assembler::adc>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    int32_t imm) {
  liftoff::OpWithCarryI<&Assembler::add, &Assembler::adc>(this, dst, lhs, imm);
}

void LiftoffAssembler::emit_i64_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::OpWithCarry<&Assembler::sub, &Assembler::sbb>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i64_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  // Idea:
  //        [           lhs_hi  |           lhs_lo  ] * [  rhs_hi  |  rhs_lo  ]
  //    =   [  lhs_hi * rhs_lo  |                   ]  (32 bit mul, shift 32)
  //      + [  lhs_lo * rhs_hi  |                   ]  (32 bit mul, shift 32)
  //      + [             lhs_lo * rhs_lo           ]  (32x32->64 mul, shift 0)

  // For simplicity, we move lhs and rhs into fixed registers.
  Register dst_hi = edx;
  Register dst_lo = eax;
  Register lhs_hi = ecx;
  Register lhs_lo = dst_lo;
  Register rhs_hi = dst_hi;
  Register rhs_lo = esi;

  // Spill all these registers if they are still holding other values.
  liftoff::SpillRegisters(this, dst_hi, dst_lo, lhs_hi, rhs_lo);

  // Move lhs and rhs into the respective registers.
  ParallelRegisterMoveTuple reg_moves[]{
      {LiftoffRegister::ForPair(lhs_lo, lhs_hi), lhs, kWasmI64},
      {LiftoffRegister::ForPair(rhs_lo, rhs_hi), rhs, kWasmI64}};
  ParallelRegisterMove(ArrayVector(reg_moves));

  // First mul: lhs_hi' = lhs_hi * rhs_lo.
  imul(lhs_hi, rhs_lo);
  // Second mul: rhi_hi' = rhs_hi * lhs_lo.
  imul(rhs_hi, lhs_lo);
  // Add them: lhs_hi'' = lhs_hi' + rhs_hi' = lhs_hi * rhs_lo + rhs_hi * lhs_lo.
  add(lhs_hi, rhs_hi);
  // Third mul: edx:eax (dst_hi:dst_lo) = eax * esi (lhs_lo * rhs_lo).
  mul(rhs_lo);
  // Add lhs_hi'' to dst_hi.
  add(dst_hi, lhs_hi);

  // Finally, move back the temporary result to the actual dst register pair.
  LiftoffRegister dst_tmp = LiftoffRegister::ForPair(dst_lo, dst_hi);
  if (dst != dst_tmp) Move(dst, dst_tmp, kWasmI64);
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  return false;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  return false;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  return false;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  return false;
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
    Register amount, void (TurboAssembler::*emit_shift)(Register, Register)) {
  // Temporary registers cannot overlap with {dst}.
  LiftoffRegList pinned = LiftoffRegList::ForRegs(dst);

  constexpr size_t kMaxRegMoves = 3;
  base::SmallVector<LiftoffAssembler::ParallelRegisterMoveTuple, kMaxRegMoves>
      reg_moves;

  // If {dst} contains {ecx}, replace it by an unused register, which is then
  // moved to {ecx} in the end.
  Register ecx_replace = no_reg;
  if (PairContains(dst, ecx)) {
    ecx_replace = assm->GetUnusedRegister(kGpReg, pinned).gp();
    dst = ReplaceInPair(dst, ecx, ecx_replace);
    // If {amount} needs to be moved to {ecx}, but {ecx} is in use (and not part
    // of {dst}, hence overwritten anyway), move {ecx} to a tmp register and
    // restore it at the end.
  } else if (amount != ecx &&
             (assm->cache_state()->is_used(LiftoffRegister(ecx)) ||
              pinned.has(LiftoffRegister(ecx)))) {
    ecx_replace = assm->GetUnusedRegister(kGpReg, pinned).gp();
    reg_moves.emplace_back(ecx_replace, ecx, kWasmI32);
  }

  reg_moves.emplace_back(dst, src, kWasmI64);
  reg_moves.emplace_back(ecx, amount, kWasmI32);
  assm->ParallelRegisterMove(VectorOf(reg_moves));

  // Do the actual shift.
  (assm->*emit_shift)(dst.high_gp(), dst.low_gp());

  // Restore {ecx} if needed.
  if (ecx_replace != no_reg) assm->mov(ecx, ecx_replace);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::ShlPair_cl);
}

void LiftoffAssembler::emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                                    int32_t amount) {
  amount &= 63;
  if (amount >= 32) {
    if (dst.high_gp() != src.low_gp()) mov(dst.high_gp(), src.low_gp());
    if (amount != 32) shl(dst.high_gp(), amount - 32);
    xor_(dst.low_gp(), dst.low_gp());
  } else {
    if (dst != src) Move(dst, src, kWasmI64);
    ShlPair(dst.high_gp(), dst.low_gp(), amount);
  }
}

void LiftoffAssembler::emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::SarPair_cl);
}

void LiftoffAssembler::emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                                    int32_t amount) {
  amount &= 63;
  if (amount >= 32) {
    if (dst.low_gp() != src.high_gp()) mov(dst.low_gp(), src.high_gp());
    if (dst.high_gp() != src.high_gp()) mov(dst.high_gp(), src.high_gp());
    if (amount != 32) sar(dst.low_gp(), amount - 32);
    sar(dst.high_gp(), 31);
  } else {
    if (dst != src) Move(dst, src, kWasmI64);
    SarPair(dst.high_gp(), dst.low_gp(), amount);
  }
}
void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::ShrPair_cl);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    int32_t amount) {
  amount &= 63;
  if (amount >= 32) {
    if (dst.low_gp() != src.high_gp()) mov(dst.low_gp(), src.high_gp());
    if (amount != 32) shr(dst.low_gp(), amount - 32);
    xor_(dst.high_gp(), dst.high_gp());
  } else {
    if (dst != src) Move(dst, src, kWasmI64);
    ShrPair(dst.high_gp(), dst.low_gp(), amount);
  }
}

void LiftoffAssembler::emit_i64_clz(LiftoffRegister dst, LiftoffRegister src) {
  // return high == 0 ? 32 + CLZ32(low) : CLZ32(high);
  Label done;
  Register safe_dst = dst.low_gp();
  if (src.low_gp() == safe_dst) safe_dst = dst.high_gp();
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcnt(safe_dst, src.high_gp());  // Sets CF if high == 0.
    j(not_carry, &done, Label::kNear);
    lzcnt(safe_dst, src.low_gp());
    add(safe_dst, Immediate(32));  // 32 + CLZ32(low)
  } else {
    // CLZ32(x) =^ x == 0 ? 32 : 31 - BSR32(x)
    Label high_is_zero;
    bsr(safe_dst, src.high_gp());  // Sets ZF is high == 0.
    j(zero, &high_is_zero, Label::kNear);
    xor_(safe_dst, Immediate(31));  // for x in [0..31], 31^x == 31-x.
    jmp(&done, Label::kNear);

    bind(&high_is_zero);
    Label low_not_zero;
    bsr(safe_dst, src.low_gp());
    j(not_zero, &low_not_zero, Label::kNear);
    mov(safe_dst, Immediate(64 ^ 63));  // 64, after the xor below.
    bind(&low_not_zero);
    xor_(safe_dst, 63);  // for x in [0..31], 63^x == 63-x.
  }

  bind(&done);
  if (safe_dst != dst.low_gp()) mov(dst.low_gp(), safe_dst);
  xor_(dst.high_gp(), dst.high_gp());  // High word of result is always 0.
}

void LiftoffAssembler::emit_i64_ctz(LiftoffRegister dst, LiftoffRegister src) {
  // return low == 0 ? 32 + CTZ32(high) : CTZ32(low);
  Label done;
  Register safe_dst = dst.low_gp();
  if (src.high_gp() == safe_dst) safe_dst = dst.high_gp();
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcnt(safe_dst, src.low_gp());  // Sets CF if low == 0.
    j(not_carry, &done, Label::kNear);
    tzcnt(safe_dst, src.high_gp());
    add(safe_dst, Immediate(32));  // 32 + CTZ32(high)
  } else {
    // CTZ32(x) =^ x == 0 ? 32 : BSF32(x)
    bsf(safe_dst, src.low_gp());  // Sets ZF is low == 0.
    j(not_zero, &done, Label::kNear);

    Label high_not_zero;
    bsf(safe_dst, src.high_gp());
    j(not_zero, &high_not_zero, Label::kNear);
    mov(safe_dst, 64);  // low == 0 and high == 0
    jmp(&done);
    bind(&high_not_zero);
    add(safe_dst, Immediate(32));  // 32 + CTZ32(high)
  }

  bind(&done);
  if (safe_dst != dst.low_gp()) mov(dst.low_gp(), safe_dst);
  xor_(dst.high_gp(), dst.high_gp());  // High word of result is always 0.
}

bool LiftoffAssembler::emit_i64_popcnt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  if (!CpuFeatures::IsSupported(POPCNT)) return false;
  CpuFeatureScope scope(this, POPCNT);
  // Produce partial popcnts in the two dst registers.
  Register src1 = src.high_gp() == dst.low_gp() ? src.high_gp() : src.low_gp();
  Register src2 = src.high_gp() == dst.low_gp() ? src.low_gp() : src.high_gp();
  popcnt(dst.low_gp(), src1);
  popcnt(dst.high_gp(), src2);
  // Add the two into the lower dst reg, clear the higher dst reg.
  add(dst.low_gp(), dst.high_gp());
  xor_(dst.high_gp(), dst.high_gp());
  return true;
}

void LiftoffAssembler::emit_u32_to_intptr(Register dst, Register src) {
  // This is a nop on ia32.
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
    movss(liftoff::kScratchDoubleReg, rhs);
    movss(dst, lhs);
    subss(dst, liftoff::kScratchDoubleReg);
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
    movss(liftoff::kScratchDoubleReg, rhs);
    movss(dst, lhs);
    divss(dst, liftoff::kScratchDoubleReg);
  } else {
    if (dst != lhs) movss(dst, lhs);
    divss(dst, rhs);
  }
}

namespace liftoff {
enum class MinOrMax : uint8_t { kMin, kMax };
template <typename type>
inline void EmitFloatMinOrMax(LiftoffAssembler* assm, DoubleRegister dst,
                              DoubleRegister lhs, DoubleRegister rhs,
                              MinOrMax min_or_max) {
  Label is_nan;
  Label lhs_below_rhs;
  Label lhs_above_rhs;
  Label done;

  // We need one tmp register to extract the sign bit. Get it right at the
  // beginning, such that the spilling code is not accidentially jumped over.
  Register tmp = assm->GetUnusedRegister(kGpReg).gp();

#define dop(name, ...)            \
  do {                            \
    if (sizeof(type) == 4) {      \
      assm->name##s(__VA_ARGS__); \
    } else {                      \
      assm->name##d(__VA_ARGS__); \
    }                             \
  } while (false)

  // Check the easy cases first: nan (e.g. unordered), smaller and greater.
  // NaN has to be checked first, because PF=1 implies CF=1.
  dop(ucomis, lhs, rhs);
  assm->j(parity_even, &is_nan, Label::kNear);   // PF=1
  assm->j(below, &lhs_below_rhs, Label::kNear);  // CF=1
  assm->j(above, &lhs_above_rhs, Label::kNear);  // CF=0 && ZF=0

  // If we get here, then either
  // a) {lhs == rhs},
  // b) {lhs == -0.0} and {rhs == 0.0}, or
  // c) {lhs == 0.0} and {rhs == -0.0}.
  // For a), it does not matter whether we return {lhs} or {rhs}. Check the sign
  // bit of {rhs} to differentiate b) and c).
  dop(movmskp, tmp, rhs);
  assm->test(tmp, Immediate(1));
  assm->j(zero, &lhs_below_rhs, Label::kNear);
  assm->jmp(&lhs_above_rhs, Label::kNear);

  assm->bind(&is_nan);
  // Create a NaN output.
  dop(xorp, dst, dst);
  dop(divs, dst, dst);
  assm->jmp(&done, Label::kNear);

  assm->bind(&lhs_below_rhs);
  DoubleRegister lhs_below_rhs_src = min_or_max == MinOrMax::kMin ? lhs : rhs;
  if (dst != lhs_below_rhs_src) dop(movs, dst, lhs_below_rhs_src);
  assm->jmp(&done, Label::kNear);

  assm->bind(&lhs_above_rhs);
  DoubleRegister lhs_above_rhs_src = min_or_max == MinOrMax::kMin ? rhs : lhs;
  if (dst != lhs_above_rhs_src) dop(movs, dst, lhs_above_rhs_src);

  assm->bind(&done);
}
}  // namespace liftoff

void LiftoffAssembler::emit_f32_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<float>(this, dst, lhs, rhs,
                                    liftoff::MinOrMax::kMin);
}

void LiftoffAssembler::emit_f32_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<float>(this, dst, lhs, rhs,
                                    liftoff::MinOrMax::kMax);
}

void LiftoffAssembler::emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  static constexpr int kF32SignBit = 1 << 31;
  Register scratch = GetUnusedRegister(kGpReg).gp();
  Register scratch2 =
      GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(scratch)).gp();
  Movd(scratch, lhs);                      // move {lhs} into {scratch}.
  and_(scratch, Immediate(~kF32SignBit));  // clear sign bit in {scratch}.
  Movd(scratch2, rhs);                     // move {rhs} into {scratch2}.
  and_(scratch2, Immediate(kF32SignBit));  // isolate sign bit in {scratch2}.
  or_(scratch, scratch2);                  // combine {scratch2} into {scratch}.
  Movd(dst, scratch);                      // move result into {dst}.
}

void LiftoffAssembler::emit_f32_abs(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint32_t kSignBit = uint32_t{1} << 31;
  if (dst == src) {
    TurboAssembler::Move(liftoff::kScratchDoubleReg, kSignBit - 1);
    Andps(dst, liftoff::kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit - 1);
    Andps(dst, src);
  }
}

void LiftoffAssembler::emit_f32_neg(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint32_t kSignBit = uint32_t{1} << 31;
  if (dst == src) {
    TurboAssembler::Move(liftoff::kScratchDoubleReg, kSignBit);
    Xorps(dst, liftoff::kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit);
    Xorps(dst, src);
  }
}

bool LiftoffAssembler::emit_f32_ceil(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundUp);
  return true;
}

bool LiftoffAssembler::emit_f32_floor(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundDown);
  return true;
}

bool LiftoffAssembler::emit_f32_trunc(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundToZero);
  return true;
}

bool LiftoffAssembler::emit_f32_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  roundss(dst, src, kRoundToNearest);
  return true;
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
    movsd(liftoff::kScratchDoubleReg, rhs);
    movsd(dst, lhs);
    subsd(dst, liftoff::kScratchDoubleReg);
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
    movsd(liftoff::kScratchDoubleReg, rhs);
    movsd(dst, lhs);
    divsd(dst, liftoff::kScratchDoubleReg);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    divsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<double>(this, dst, lhs, rhs,
                                     liftoff::MinOrMax::kMin);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  static constexpr int kF32SignBit = 1 << 31;
  // On ia32, we cannot hold the whole f64 value in a gp register, so we just
  // operate on the upper half (UH).
  Register scratch = GetUnusedRegister(kGpReg).gp();
  Register scratch2 =
      GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(scratch)).gp();

  Pextrd(scratch, lhs, 1);                 // move UH of {lhs} into {scratch}.
  and_(scratch, Immediate(~kF32SignBit));  // clear sign bit in {scratch}.
  Pextrd(scratch2, rhs, 1);                // move UH of {rhs} into {scratch2}.
  and_(scratch2, Immediate(kF32SignBit));  // isolate sign bit in {scratch2}.
  or_(scratch, scratch2);                  // combine {scratch2} into {scratch}.
  movsd(dst, lhs);                         // move {lhs} into {dst}.
  Pinsrd(dst, scratch, 1);                 // insert {scratch} into UH of {dst}.
}

void LiftoffAssembler::emit_f64_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<double>(this, dst, lhs, rhs,
                                     liftoff::MinOrMax::kMax);
}

void LiftoffAssembler::emit_f64_abs(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint64_t kSignBit = uint64_t{1} << 63;
  if (dst == src) {
    TurboAssembler::Move(liftoff::kScratchDoubleReg, kSignBit - 1);
    Andpd(dst, liftoff::kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit - 1);
    Andpd(dst, src);
  }
}

void LiftoffAssembler::emit_f64_neg(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint64_t kSignBit = uint64_t{1} << 63;
  if (dst == src) {
    TurboAssembler::Move(liftoff::kScratchDoubleReg, kSignBit);
    Xorpd(dst, liftoff::kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit);
    Xorpd(dst, src);
  }
}

bool LiftoffAssembler::emit_f64_ceil(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  roundsd(dst, src, kRoundUp);
  return true;
}

bool LiftoffAssembler::emit_f64_floor(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  roundsd(dst, src, kRoundDown);
  return true;
}

bool LiftoffAssembler::emit_f64_trunc(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  roundsd(dst, src, kRoundToZero);
  return true;
}

bool LiftoffAssembler::emit_f64_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  roundsd(dst, src, kRoundToNearest);
  return true;
}

void LiftoffAssembler::emit_f64_sqrt(DoubleRegister dst, DoubleRegister src) {
  Sqrtsd(dst, src);
}

namespace liftoff {
// Used for float to int conversions. If the value in {converted_back} equals
// {src} afterwards, the conversion succeeded.
template <typename dst_type, typename src_type>
inline void ConvertFloatToIntAndBack(LiftoffAssembler* assm, Register dst,
                                     DoubleRegister src,
                                     DoubleRegister converted_back,
                                     LiftoffRegList pinned) {
  if (std::is_same<double, src_type>::value) {  // f64
    if (std::is_signed<dst_type>::value) {      // f64 -> i32
      assm->cvttsd2si(dst, src);
      assm->Cvtsi2sd(converted_back, dst);
    } else {  // f64 -> u32
      assm->Cvttsd2ui(dst, src, liftoff::kScratchDoubleReg);
      assm->Cvtui2sd(converted_back, dst,
                     assm->GetUnusedRegister(kGpReg, pinned).gp());
    }
  } else {                                  // f32
    if (std::is_signed<dst_type>::value) {  // f32 -> i32
      assm->cvttss2si(dst, src);
      assm->Cvtsi2ss(converted_back, dst);
    } else {  // f32 -> u32
      assm->Cvttss2ui(dst, src, liftoff::kScratchDoubleReg);
      assm->Cvtui2ss(converted_back, dst,
                     assm->GetUnusedRegister(kGpReg, pinned).gp());
    }
  }
}

template <typename dst_type, typename src_type>
inline bool EmitTruncateFloatToInt(LiftoffAssembler* assm, Register dst,
                                   DoubleRegister src, Label* trap) {
  if (!CpuFeatures::IsSupported(SSE4_1)) {
    assm->bailout(kMissingCPUFeature, "no SSE4.1");
    return true;
  }
  CpuFeatureScope feature(assm, SSE4_1);

  LiftoffRegList pinned = LiftoffRegList::ForRegs(src, dst);
  DoubleRegister rounded =
      pinned.set(assm->GetUnusedRegister(kFpReg, pinned)).fp();
  DoubleRegister converted_back =
      pinned.set(assm->GetUnusedRegister(kFpReg, pinned)).fp();

  if (std::is_same<double, src_type>::value) {  // f64
    assm->roundsd(rounded, src, kRoundToZero);
  } else {  // f32
    assm->roundss(rounded, src, kRoundToZero);
  }
  ConvertFloatToIntAndBack<dst_type, src_type>(assm, dst, rounded,
                                               converted_back, pinned);
  if (std::is_same<double, src_type>::value) {  // f64
    assm->ucomisd(converted_back, rounded);
  } else {  // f32
    assm->ucomiss(converted_back, rounded);
  }

  // Jump to trap if PF is 0 (one of the operands was NaN) or they are not
  // equal.
  assm->j(parity_even, trap);
  assm->j(not_equal, trap);
  return true;
}
}  // namespace liftoff

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      if (dst.gp() != src.low_gp()) mov(dst.gp(), src.low_gp());
      return true;
    case kExprI32SConvertF32:
      return liftoff::EmitTruncateFloatToInt<int32_t, float>(this, dst.gp(),
                                                             src.fp(), trap);
    case kExprI32UConvertF32:
      return liftoff::EmitTruncateFloatToInt<uint32_t, float>(this, dst.gp(),
                                                              src.fp(), trap);
    case kExprI32SConvertF64:
      return liftoff::EmitTruncateFloatToInt<int32_t, double>(this, dst.gp(),
                                                              src.fp(), trap);
    case kExprI32UConvertF64:
      return liftoff::EmitTruncateFloatToInt<uint32_t, double>(this, dst.gp(),
                                                               src.fp(), trap);
    case kExprI32ReinterpretF32:
      Movd(dst.gp(), src.fp());
      return true;
    case kExprI64SConvertI32:
      if (dst.low_gp() != src.gp()) mov(dst.low_gp(), src.gp());
      if (dst.high_gp() != src.gp()) mov(dst.high_gp(), src.gp());
      sar(dst.high_gp(), 31);
      return true;
    case kExprI64UConvertI32:
      if (dst.low_gp() != src.gp()) mov(dst.low_gp(), src.gp());
      xor_(dst.high_gp(), dst.high_gp());
      return true;
    case kExprI64ReinterpretF64:
      // Push src to the stack.
      AllocateStackSpace(8);
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
    case kExprF64UConvertI32: {
      LiftoffRegList pinned = LiftoffRegList::ForRegs(dst, src);
      Register scratch = GetUnusedRegister(kGpReg, pinned).gp();
      Cvtui2sd(dst.fp(), src.gp(), scratch);
      return true;
    }
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

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  Register byte_reg = liftoff::GetTmpByteRegister(this, src);
  if (byte_reg != src) mov(byte_reg, src);
  movsx_b(dst, byte_reg);
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  movsx_w(dst, src);
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  Register byte_reg = liftoff::GetTmpByteRegister(this, src.low_gp());
  if (byte_reg != src.low_gp()) mov(byte_reg, src.low_gp());
  movsx_b(dst.low_gp(), byte_reg);
  liftoff::SignExtendI32ToI64(this, dst);
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  movsx_w(dst.low_gp(), src.low_gp());
  liftoff::SignExtendI32ToI64(this, dst);
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  if (dst.low_gp() != src.low_gp()) mov(dst.low_gp(), src.low_gp());
  liftoff::SignExtendI32ToI64(this, dst);
}

void LiftoffAssembler::emit_jump(Label* label) { jmp(label); }

void LiftoffAssembler::emit_jump(Register target) { jmp(target); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  if (rhs != no_reg) {
    switch (type.kind()) {
      case ValueType::kI32:
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

namespace liftoff {
template <void (Assembler::*avx_op)(XMMRegister, XMMRegister, XMMRegister),
          void (Assembler::*sse_op)(XMMRegister, XMMRegister)>
void EmitSimdCommutativeBinOp(
    LiftoffAssembler* assm, LiftoffRegister dst, LiftoffRegister lhs,
    LiftoffRegister rhs, base::Optional<CpuFeature> feature = base::nullopt) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(assm, AVX);
    (assm->*avx_op)(dst.fp(), lhs.fp(), rhs.fp());
    return;
  }

  base::Optional<CpuFeatureScope> sse_scope;
  if (feature.has_value()) sse_scope.emplace(assm, *feature);

  if (dst.fp() == rhs.fp()) {
    (assm->*sse_op)(dst.fp(), lhs.fp());
  } else {
    if (dst.fp() != lhs.fp()) (assm->movaps)(dst.fp(), lhs.fp());
    (assm->*sse_op)(dst.fp(), rhs.fp());
  }
}

template <void (Assembler::*avx_op)(XMMRegister, XMMRegister, XMMRegister),
          void (Assembler::*sse_op)(XMMRegister, XMMRegister)>
void EmitSimdSub(LiftoffAssembler* assm, LiftoffRegister dst,
                 LiftoffRegister lhs, LiftoffRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(assm, AVX);
    (assm->*avx_op)(dst.fp(), lhs.fp(), rhs.fp());
  } else if (lhs.fp() == rhs.fp()) {
    assm->pxor(dst.fp(), dst.fp());
  } else if (dst.fp() == rhs.fp()) {
    assm->movaps(kScratchDoubleReg, rhs.fp());
    assm->movaps(dst.fp(), lhs.fp());
    (assm->*sse_op)(dst.fp(), kScratchDoubleReg);
  } else {
    if (dst.fp() != lhs.fp()) assm->movaps(dst.fp(), lhs.fp());
    (assm->*sse_op)(dst.fp(), rhs.fp());
  }
}
}  // namespace liftoff

void LiftoffAssembler::emit_f64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Movddup(dst.fp(), src.fp());
}

void LiftoffAssembler::emit_f64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vshufpd(dst.fp(), lhs.fp(), lhs.fp(), imm_lane_idx);
  } else {
    if (dst.fp() != lhs.fp()) movaps(dst.fp(), lhs.fp());
    if (imm_lane_idx != 0) shufpd(dst.fp(), dst.fp(), imm_lane_idx);
  }
}

void LiftoffAssembler::emit_f64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  // TODO(fanchenk): Use movlhps and blendpd
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    if (imm_lane_idx == 0) {
      vinsertps(dst.fp(), src1.fp(), src2.fp(), 0b00000000);
      vinsertps(dst.fp(), dst.fp(), src2.fp(), 0b01010000);
    } else {
      vinsertps(dst.fp(), src1.fp(), src2.fp(), 0b00100000);
      vinsertps(dst.fp(), dst.fp(), src2.fp(), 0b01110000);
    }
  } else {
    CpuFeatureScope scope(this, SSE4_1);
    if (dst.fp() != src1.fp()) movaps(dst.fp(), src1.fp());
    if (imm_lane_idx == 0) {
      insertps(dst.fp(), src2.fp(), 0b00000000);
      insertps(dst.fp(), src2.fp(), 0b01010000);
    } else {
      insertps(dst.fp(), src2.fp(), 0b00100000);
      insertps(dst.fp(), src2.fp(), 0b01110000);
    }
  }
}

void LiftoffAssembler::emit_f64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdCommutativeBinOp<&Assembler::vaddpd, &Assembler::addpd>(
      this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_f64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdSub<&Assembler::vsubpd, &Assembler::subpd>(this, dst, lhs,
                                                              rhs);
}

void LiftoffAssembler::emit_f64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdCommutativeBinOp<&Assembler::vmulpd, &Assembler::mulpd>(
      this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_f32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vshufps(dst.fp(), src.fp(), src.fp(), 0);
  } else {
    if (dst.fp() != src.fp()) {
      movss(dst.fp(), src.fp());
    }
    shufps(dst.fp(), src.fp(), 0);
  }
}

void LiftoffAssembler::emit_f32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vshufps(dst.fp(), lhs.fp(), lhs.fp(), imm_lane_idx);
  } else {
    if (dst.fp() != lhs.fp()) movaps(dst.fp(), lhs.fp());
    if (imm_lane_idx != 0) shufps(dst.fp(), dst.fp(), imm_lane_idx);
  }
}

void LiftoffAssembler::emit_f32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vinsertps(dst.fp(), src1.fp(), src2.fp(), (imm_lane_idx << 4) & 0x30);
  } else {
    CpuFeatureScope scope(this, SSE4_1);
    if (dst.fp() != src1.fp()) movaps(dst.fp(), src1.fp());
    insertps(dst.fp(), src2.fp(), (imm_lane_idx << 4) & 0x30);
  }
}

void LiftoffAssembler::emit_f32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdCommutativeBinOp<&Assembler::vaddps, &Assembler::addps>(
      this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_f32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdSub<&Assembler::vsubps, &Assembler::subps>(this, dst, lhs,
                                                              rhs);
}

void LiftoffAssembler::emit_f32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdCommutativeBinOp<&Assembler::vmulps, &Assembler::mulps>(
      this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Pinsrd(dst.fp(), src.low_gp(), 0);
  Pinsrd(dst.fp(), src.high_gp(), 1);
  Pshufd(dst.fp(), dst.fp(), 0x44);
}

void LiftoffAssembler::emit_i64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  Pextrd(dst.low_gp(), lhs.fp(), imm_lane_idx * 2);
  Pextrd(dst.high_gp(), lhs.fp(), imm_lane_idx * 2 + 1);
}

void LiftoffAssembler::emit_i64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpinsrd(dst.fp(), src1.fp(), src2.low_gp(), imm_lane_idx * 2);
    vpinsrd(dst.fp(), dst.fp(), src2.high_gp(), imm_lane_idx * 2 + 1);
  } else {
    CpuFeatureScope scope(this, SSE4_1);
    if (dst.fp() != src1.fp()) movaps(dst.fp(), src1.fp());
    pinsrd(dst.fp(), src2.low_gp(), imm_lane_idx * 2);
    pinsrd(dst.fp(), src2.high_gp(), imm_lane_idx * 2 + 1);
  }
}

void LiftoffAssembler::emit_i64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdCommutativeBinOp<&Assembler::vpaddq, &Assembler::paddq>(
      this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdSub<&Assembler::vpsubq, &Assembler::psubq>(this, dst, lhs,
                                                              rhs);
}

void LiftoffAssembler::emit_i64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  static constexpr RegClass tmp_rc = reg_class_for(ValueType::kS128);
  LiftoffRegister tmp1 =
      GetUnusedRegister(tmp_rc, LiftoffRegList::ForRegs(dst, lhs, rhs));
  LiftoffRegister tmp2 =
      GetUnusedRegister(tmp_rc, LiftoffRegList::ForRegs(dst, lhs, rhs, tmp1));
  Movaps(tmp1.fp(), lhs.fp());
  Movaps(tmp2.fp(), rhs.fp());
  // Multiply high dword of each qword of left with right.
  Psrlq(tmp1.fp(), 32);
  Pmuludq(tmp1.fp(), tmp1.fp(), rhs.fp());
  // Multiply high dword of each qword of right with left.
  Psrlq(tmp2.fp(), 32);
  Pmuludq(tmp2.fp(), tmp2.fp(), lhs.fp());
  Paddq(tmp2.fp(), tmp2.fp(), tmp1.fp());
  Psllq(tmp2.fp(), tmp2.fp(), 32);
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpmuludq(dst.fp(), lhs.fp(), rhs.fp());
  } else {
    if (dst.fp() != lhs.fp()) movaps(dst.fp(), lhs.fp());
    pmuludq(dst.fp(), rhs.fp());
  }
  Paddq(dst.fp(), dst.fp(), tmp2.fp());
}

void LiftoffAssembler::emit_i32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Movd(dst.fp(), src.gp());
  Pshufd(dst.fp(), dst.fp(), 0);
}

void LiftoffAssembler::emit_i32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  Pextrd(dst.gp(), lhs.fp(), imm_lane_idx);
}

void LiftoffAssembler::emit_i32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpinsrd(dst.fp(), src1.fp(), src2.gp(), imm_lane_idx);
  } else {
    CpuFeatureScope scope(this, SSE4_1);
    if (dst.fp() != src1.fp()) movaps(dst.fp(), src1.fp());
    pinsrd(dst.fp(), src2.gp(), imm_lane_idx);
  }
}

void LiftoffAssembler::emit_i32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdCommutativeBinOp<&Assembler::vpaddd, &Assembler::paddd>(
      this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdSub<&Assembler::vpsubd, &Assembler::psubd>(this, dst, lhs,
                                                              rhs);
}

void LiftoffAssembler::emit_i32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdCommutativeBinOp<&Assembler::vpmulld, &Assembler::pmulld>(
      this, dst, lhs, rhs, base::Optional<CpuFeature>(SSE4_1));
}

void LiftoffAssembler::emit_i16x8_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Movd(dst.fp(), src.gp());
  Pshuflw(dst.fp(), dst.fp(), 0);
  Pshufd(dst.fp(), dst.fp(), 0);
}

void LiftoffAssembler::emit_i16x8_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  Pextrw(dst.gp(), lhs.fp(), imm_lane_idx);
}

void LiftoffAssembler::emit_i16x8_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  Pextrw(dst.gp(), lhs.fp(), imm_lane_idx);
  movsx_w(dst.gp(), dst.gp());
}

void LiftoffAssembler::emit_i16x8_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpinsrw(dst.fp(), src1.fp(), src2.gp(), imm_lane_idx);
  } else {
    if (dst.fp() != src1.fp()) movaps(dst.fp(), src1.fp());
    pinsrw(dst.fp(), src2.gp(), imm_lane_idx);
  }
}

void LiftoffAssembler::emit_i16x8_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdCommutativeBinOp<&Assembler::vpaddw, &Assembler::paddw>(
      this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i16x8_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdSub<&Assembler::vpsubw, &Assembler::psubw>(this, dst, lhs,
                                                              rhs);
}

void LiftoffAssembler::emit_i16x8_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdCommutativeBinOp<&Assembler::vpmullw, &Assembler::pmullw>(
      this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i8x16_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Movd(dst.fp(), src.gp());
  Pxor(liftoff::kScratchDoubleReg, liftoff::kScratchDoubleReg);
  Pshufb(dst.fp(), liftoff::kScratchDoubleReg);
}

void LiftoffAssembler::emit_i8x16_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  Pextrb(dst.gp(), lhs.fp(), imm_lane_idx);
}

void LiftoffAssembler::emit_i8x16_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  Pextrb(dst.gp(), lhs.fp(), imm_lane_idx);
  movsx_b(dst.gp(), dst.gp());
}

void LiftoffAssembler::emit_i8x16_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpinsrb(dst.fp(), src1.fp(), src2.gp(), imm_lane_idx);
  } else {
    CpuFeatureScope scope(this, SSE4_1);
    if (dst.fp() != src1.fp()) movaps(dst.fp(), src1.fp());
    pinsrb(dst.fp(), src2.gp(), imm_lane_idx);
  }
}

void LiftoffAssembler::emit_i8x16_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdCommutativeBinOp<&Assembler::vpaddb, &Assembler::paddb>(
      this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i8x16_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdSub<&Assembler::vpsubb, &Assembler::psubb>(this, dst, lhs,
                                                              rhs);
}

void LiftoffAssembler::emit_i8x16_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  static constexpr RegClass tmp_rc = reg_class_for(ValueType::kS128);
  LiftoffRegister tmp =
      GetUnusedRegister(tmp_rc, LiftoffRegList::ForRegs(dst, lhs, rhs));
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    // I16x8 view of I8x16
    // left = AAaa AAaa ... AAaa AAaa
    // right= BBbb BBbb ... BBbb BBbb
    // t = 00AA 00AA ... 00AA 00AA
    // s = 00BB 00BB ... 00BB 00BB
    vpsrlw(tmp.fp(), lhs.fp(), 8);
    vpsrlw(liftoff::kScratchDoubleReg, rhs.fp(), 8);
    // t = I16x8Mul(t0, t1)
    //    => __PP __PP ...  __PP  __PP
    vpmullw(tmp.fp(), tmp.fp(), liftoff::kScratchDoubleReg);
    // s = left * 256
    vpsllw(liftoff::kScratchDoubleReg, lhs.fp(), 8);
    // dst = I16x8Mul(left * 256, right)
    //    => pp__ pp__ ...  pp__  pp__
    vpmullw(dst.fp(), liftoff::kScratchDoubleReg, rhs.fp());
    // dst = I16x8Shr(dst, 8)
    //    => 00pp 00pp ...  00pp  00pp
    vpsrlw(dst.fp(), dst.fp(), 8);
    // t = I16x8Shl(t, 8)
    //    => PP00 PP00 ...  PP00  PP00
    vpsllw(tmp.fp(), tmp.fp(), 8);
    // dst = I16x8Or(dst, t)
    //    => PPpp PPpp ...  PPpp  PPpp
    vpor(dst.fp(), dst.fp(), tmp.fp());
  } else {
    if (dst.fp() != lhs.fp()) movaps(dst.fp(), lhs.fp());
    // I16x8 view of I8x16
    // left = AAaa AAaa ... AAaa AAaa
    // right= BBbb BBbb ... BBbb BBbb
    // t = 00AA 00AA ... 00AA 00AA
    // s = 00BB 00BB ... 00BB 00BB
    movaps(tmp.fp(), dst.fp());
    movaps(liftoff::kScratchDoubleReg, rhs.fp());
    psrlw(tmp.fp(), 8);
    psrlw(liftoff::kScratchDoubleReg, 8);
    // dst = left * 256
    psllw(dst.fp(), 8);
    // t = I16x8Mul(t, s)
    //    => __PP __PP ...  __PP  __PP
    pmullw(tmp.fp(), liftoff::kScratchDoubleReg);
    // dst = I16x8Mul(left * 256, right)
    //    => pp__ pp__ ...  pp__  pp__
    pmullw(dst.fp(), rhs.fp());
    // t = I16x8Shl(t, 8)
    //    => PP00 PP00 ...  PP00  PP00
    psllw(tmp.fp(), 8);
    // dst = I16x8Shr(dst, 8)
    //    => 00pp 00pp ...  00pp  00pp
    psrlw(dst.fp(), 8);
    // dst = I16x8Or(dst, t)
    //    => PPpp PPpp ...  PPpp  PPpp
    por(dst.fp(), tmp.fp());
  }
}

void LiftoffAssembler::StackCheck(Label* ool_code, Register limit_address) {
  cmp(esp, Operand(limit_address, 0));
  j(below_equal, ool_code);
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0, GetUnusedRegister(kGpReg).gp());
  CallCFunction(ExternalReference::wasm_call_trap_callback_for_testing(), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  TurboAssembler::AssertUnreachable(reason);
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
    AllocateStackSpace(num_fp_regs * kSimd128Size);
    unsigned offset = 0;
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetFirstRegSet();
      Movdqu(Operand(esp, offset), reg.fp());
      fp_regs.clear(reg);
      offset += kSimd128Size;
    }
    DCHECK_EQ(offset, num_fp_regs * kSimd128Size);
  }
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned fp_offset = 0;
  while (!fp_regs.is_empty()) {
    LiftoffRegister reg = fp_regs.GetFirstRegSet();
    Movdqu(reg.fp(), Operand(esp, fp_offset));
    fp_regs.clear(reg);
    fp_offset += kSimd128Size;
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
  DCHECK_LT(num_stack_slots,
            (1 << 16) / kSystemPointerSize);  // 16 bit immediate
  ret(static_cast<int>(num_stack_slots * kSystemPointerSize));
}

void LiftoffAssembler::CallC(const wasm::FunctionSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueType out_argument_type, int stack_bytes,
                             ExternalReference ext_ref) {
  AllocateStackSpace(stack_bytes);

  int arg_bytes = 0;
  for (ValueType param_type : sig->parameters()) {
    liftoff::Store(this, esp, arg_bytes, *args++, param_type);
    arg_bytes += param_type.element_size_bytes();
  }
  DCHECK_LE(arg_bytes, stack_bytes);

  constexpr Register kScratch = eax;
  constexpr Register kArgumentBuffer = ecx;
  constexpr int kNumCCallArgs = 1;
  mov(kArgumentBuffer, esp);
  PrepareCallCFunction(kNumCCallArgs, kScratch);

  // Pass a pointer to the buffer with the arguments to the C function. ia32
  // does not use registers here, so push to the stack.
  mov(Operand(esp, 0), kArgumentBuffer);

  // Now call the C function.
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* next_result_reg = rets;
  if (sig->return_count() > 0) {
    DCHECK_EQ(1, sig->return_count());
    constexpr Register kReturnReg = eax;
    if (kReturnReg != next_result_reg->gp()) {
      Move(*next_result_reg, LiftoffRegister(kReturnReg), sig->GetReturn(0));
    }
    ++next_result_reg;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_type != kWasmStmt) {
    liftoff::Load(this, *next_result_reg, esp, 0, out_argument_type);
  }

  add(esp, Immediate(stack_bytes));
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  wasm_call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallIndirect(const wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  // Since we have more cache registers than parameter registers, the
  // {LiftoffCompiler} should always be able to place {target} in a register.
  DCHECK(target.is_valid());
  if (FLAG_untrusted_code_mitigations) {
    RetpolineCall(target);
  } else {
    call(target);
  }
}

void LiftoffAssembler::CallRuntimeStub(WasmCode::RuntimeStubId sid) {
  // A direct call to a wasm runtime stub defined in this module.
  // Just encode the stub index. This will be patched at relocation.
  wasm_call(static_cast<Address>(sid), RelocInfo::WASM_STUB_CALL);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  AllocateStackSpace(size);
  mov(addr, esp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  add(esp, Immediate(size));
}

void LiftoffStackSlots::Construct() {
  for (auto& slot : slots_) {
    const LiftoffAssembler::VarState& src = slot.src_;
    switch (src.loc()) {
      case LiftoffAssembler::VarState::kStack:
        // The combination of AllocateStackSpace and 2 movdqu is usually smaller
        // in code size than doing 4 pushes.
        if (src.type() == kWasmS128) {
          asm_->AllocateStackSpace(sizeof(double) * 2);
          asm_->movdqu(liftoff::kScratchDoubleReg,
                       liftoff::GetStackSlot(slot.src_offset_));
          asm_->movdqu(Operand(esp, 0), liftoff::kScratchDoubleReg);
          break;
        }
        if (src.type() == kWasmF64) {
          DCHECK_EQ(kLowWord, slot.half_);
          asm_->push(liftoff::GetHalfStackSlot(slot.src_offset_, kHighWord));
        }
        asm_->push(liftoff::GetHalfStackSlot(slot.src_offset_, slot.half_));
        break;
      case LiftoffAssembler::VarState::kRegister:
        if (src.type() == kWasmI64) {
          liftoff::push(
              asm_, slot.half_ == kLowWord ? src.reg().low() : src.reg().high(),
              kWasmI32);
        } else {
          liftoff::push(asm_, src.reg(), src.type());
        }
        break;
      case LiftoffAssembler::VarState::kIntConst:
        // The high word is the sign extension of the low word.
        asm_->push(Immediate(slot.half_ == kLowWord ? src.i32_const()
                                                    : src.i32_const() >> 31));
        break;
    }
  }
}

#undef RETURN_FALSE_IF_MISSING_CPU_FEATURE

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_IA32_LIFTOFF_ASSEMBLER_IA32_H_

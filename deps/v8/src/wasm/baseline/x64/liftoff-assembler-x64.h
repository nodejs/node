// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_X64_LIFTOFF_ASSEMBLER_X64_H_
#define V8_WASM_BASELINE_X64_LIFTOFF_ASSEMBLER_X64_H_

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

constexpr Register kScratchRegister2 = r11;
static_assert(kScratchRegister != kScratchRegister2, "collision");
static_assert((kLiftoffAssemblerGpCacheRegs &
               Register::ListOf(kScratchRegister, kScratchRegister2)) == 0,
              "scratch registers must not be used as cache registers");

constexpr DoubleRegister kScratchDoubleReg2 = xmm14;
static_assert(kScratchDoubleReg != kScratchDoubleReg2, "collision");
static_assert((kLiftoffAssemblerFpCacheRegs &
               DoubleRegister::ListOf(kScratchDoubleReg, kScratchDoubleReg2)) ==
                  0,
              "scratch registers must not be used as cache registers");

// rbp-8 holds the stack marker, rbp-16 is the instance parameter.
constexpr int kInstanceOffset = 16;

inline Operand GetStackSlot(int offset) { return Operand(rbp, -offset); }

// TODO(clemensb): Make this a constexpr variable once Operand is constexpr.
inline Operand GetInstanceOperand() { return GetStackSlot(kInstanceOffset); }

inline Operand GetMemOp(LiftoffAssembler* assm, Register addr, Register offset,
                        uint32_t offset_imm) {
  if (is_uint31(offset_imm)) {
    if (offset == no_reg) return Operand(addr, offset_imm);
    return Operand(addr, offset, times_1, offset_imm);
  }
  // Offset immediate does not fit in 31 bits.
  Register scratch = kScratchRegister;
  assm->movl(scratch, Immediate(offset_imm));
  if (offset != no_reg) {
    assm->addq(scratch, offset);
  }
  return Operand(addr, scratch, times_1, 0);
}

inline void Load(LiftoffAssembler* assm, LiftoffRegister dst, Operand src,
                 ValueType type) {
  switch (type.kind()) {
    case ValueType::kI32:
      assm->movl(dst.gp(), src);
      break;
    case ValueType::kI64:
      assm->movq(dst.gp(), src);
      break;
    case ValueType::kF32:
      assm->Movss(dst.fp(), src);
      break;
    case ValueType::kF64:
      assm->Movsd(dst.fp(), src);
      break;
    case ValueType::kS128:
      assm->Movdqu(dst.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

inline void Store(LiftoffAssembler* assm, Operand dst, LiftoffRegister src,
                  ValueType type) {
  switch (type.kind()) {
    case ValueType::kI32:
      assm->movl(dst, src.gp());
      break;
    case ValueType::kI64:
      assm->movq(dst, src.gp());
      break;
    case ValueType::kF32:
      assm->Movss(dst, src.fp());
      break;
    case ValueType::kF64:
      assm->Movsd(dst, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueType type) {
  switch (type.kind()) {
    case ValueType::kI32:
    case ValueType::kI64:
      assm->pushq(reg.gp());
      break;
    case ValueType::kF32:
      assm->AllocateStackSpace(kSystemPointerSize);
      assm->Movss(Operand(rsp, 0), reg.fp());
      break;
    case ValueType::kF64:
      assm->AllocateStackSpace(kSystemPointerSize);
      assm->Movsd(Operand(rsp, 0), reg.fp());
      break;
    case ValueType::kS128:
      assm->AllocateStackSpace(kSystemPointerSize * 2);
      assm->Movdqu(Operand(rsp, 0), reg.fp());
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

constexpr int kSubSpSize = 7;  // 7 bytes for "subq rsp, <imm32>"

}  // namespace liftoff

int LiftoffAssembler::PrepareStackFrame() {
  int offset = pc_offset();
  sub_sp_32(0);
  DCHECK_EQ(liftoff::kSubSpSize, pc_offset() - offset);
  return offset;
}

void LiftoffAssembler::PatchPrepareStackFrame(int offset, int frame_size) {
  // Need to align sp to system pointer size.
  frame_size = RoundUp(frame_size, kSystemPointerSize);
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
    // https://docs.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-6.0/aa227153(v=vs.60)).
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
      if (value.to_i32() == 0 && RelocInfo::IsNone(rmode)) {
        xorl(reg.gp(), reg.gp());
      } else {
        movl(reg.gp(), Immediate(value.to_i32(), rmode));
      }
      break;
    case ValueType::kI64:
      if (RelocInfo::IsNone(rmode)) {
        TurboAssembler::Set(reg.gp(), value.to_i64());
      } else {
        movq(reg.gp(), Immediate64(value.to_i64(), rmode));
      }
      break;
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
  movq(dst, liftoff::GetInstanceOperand());
  DCHECK(size == 4 || size == 8);
  if (size == 4) {
    movl(dst, Operand(dst, offset));
  } else {
    movq(dst, Operand(dst, offset));
  }
}

void LiftoffAssembler::LoadTaggedPointerFromInstance(Register dst,
                                                     uint32_t offset) {
  DCHECK_LE(offset, kMaxInt);
  movq(dst, liftoff::GetInstanceOperand());
  LoadTaggedPointerField(dst, Operand(dst, offset));
}

void LiftoffAssembler::SpillInstance(Register instance) {
  movq(liftoff::GetInstanceOperand(), instance);
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  movq(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         uint32_t offset_imm,
                                         LiftoffRegList pinned) {
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
  Operand src_op = liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm);
  LoadTaggedPointerField(dst, src_op);
}

void LiftoffAssembler::AtomicLoad(LiftoffRegister dst, Register src_addr,
                                  Register offset_reg, uint32_t offset_imm,
                                  LoadType type, LiftoffRegList pinned) {
  Load(dst, src_addr, offset_reg, offset_imm, type, pinned, nullptr, true);
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc, bool is_load_mem) {
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
  Operand src_op = liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm);
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
    case LoadType::kS128Load:
      Movdqu(dst.fp(), src_op);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList /* pinned */,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
  Operand dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm);
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
    case StoreType::kS128Store:
      Movdqu(dst_op, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicStore(Register dst_addr, Register offset_reg,
                                   uint32_t offset_imm, LiftoffRegister src,
                                   StoreType type, LiftoffRegList pinned) {
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
  Operand dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm);
  Register src_reg = src.gp();
  if (cache_state()->is_used(src)) {
    movq(kScratchRegister, src_reg);
    src_reg = kScratchRegister;
  }
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      xchgb(src_reg, dst_op);
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      xchgw(src_reg, dst_op);
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      xchgl(src_reg, dst_op);
      break;
    case StoreType::kI64Store:
      xchgq(src_reg, dst_op);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicAdd(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 StoreType type) {
  DCHECK(!cache_state()->is_used(value));
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
  Operand dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm);
  lock();
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      xaddb(dst_op, value.gp());
      movzxbq(value.gp(), value.gp());
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      xaddw(dst_op, value.gp());
      movzxwq(value.gp(), value.gp());
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      xaddl(dst_op, value.gp());
      break;
    case StoreType::kI64Store:
      xaddq(dst_op, value.gp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicSub(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 StoreType type) {
  DCHECK(!cache_state()->is_used(value));
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
  Operand dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm);
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      negb(value.gp());
      lock();
      xaddb(dst_op, value.gp());
      movzxbq(value.gp(), value.gp());
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      negw(value.gp());
      lock();
      xaddw(dst_op, value.gp());
      movzxwq(value.gp(), value.gp());
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      negl(value.gp());
      lock();
      xaddl(dst_op, value.gp());
      break;
    case StoreType::kI64Store:
      negq(value.gp());
      lock();
      xaddq(dst_op, value.gp());
      break;
    default:
      UNREACHABLE();
  }
}

namespace liftoff {
#define __ lasm->
// Checks if a register in {possible_uses} uses {reg}. If so, it allocates a
// replacement register for that use, and moves the content of {reg} to {use}.
// The replacement register is written into the pointer stored in
// {possible_uses}.
inline void ClearRegister(LiftoffAssembler* lasm, Register reg,
                          std::initializer_list<Register*> possible_uses,
                          LiftoffRegList pinned) {
  liftoff::SpillRegisters(lasm, reg);
  Register replacement = no_reg;
  for (Register* use : possible_uses) {
    if (reg != *use) continue;
    if (replacement == no_reg) {
      replacement = __ GetUnusedRegister(kGpReg, pinned).gp();
      __ movq(replacement, reg);
    }
    // We cannot leave this loop early. There may be multiple uses of {reg}.
    *use = replacement;
  }
}

inline void AtomicBinop(LiftoffAssembler* lasm,
                        void (Assembler::*opl)(Register, Register),
                        void (Assembler::*opq)(Register, Register),
                        Register dst_addr, Register offset_reg,
                        uint32_t offset_imm, LiftoffRegister value,
                        StoreType type) {
  DCHECK(!__ cache_state()->is_used(value));
  Register value_reg = value.gp();
  // The cmpxchg instruction uses rax to store the old value of the
  // compare-exchange primitive. Therefore we have to spill the register and
  // move any use to another register.
  LiftoffRegList pinned =
      LiftoffRegList::ForRegs(dst_addr, offset_reg, value_reg);
  ClearRegister(lasm, rax, {&dst_addr, &offset_reg, &value_reg}, pinned);
  if (__ emit_debug_code() && offset_reg != no_reg) {
    __ AssertZeroExtended(offset_reg);
  }
  Operand dst_op = liftoff::GetMemOp(lasm, dst_addr, offset_reg, offset_imm);

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      Label binop;
      __ xorq(rax, rax);
      __ movb(rax, dst_op);
      __ bind(&binop);
      __ movl(kScratchRegister, rax);
      (lasm->*opl)(kScratchRegister, value_reg);
      __ lock();
      __ cmpxchgb(dst_op, kScratchRegister);
      __ j(not_equal, &binop);
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
      Label binop;
      __ xorq(rax, rax);
      __ movw(rax, dst_op);
      __ bind(&binop);
      __ movl(kScratchRegister, rax);
      (lasm->*opl)(kScratchRegister, value_reg);
      __ lock();
      __ cmpxchgw(dst_op, kScratchRegister);
      __ j(not_equal, &binop);
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
      Label binop;
      __ movl(rax, dst_op);
      __ bind(&binop);
      __ movl(kScratchRegister, rax);
      (lasm->*opl)(kScratchRegister, value_reg);
      __ lock();
      __ cmpxchgl(dst_op, kScratchRegister);
      __ j(not_equal, &binop);
      break;
    }
    case StoreType::kI64Store: {
      Label binop;
      __ movq(rax, dst_op);
      __ bind(&binop);
      __ movq(kScratchRegister, rax);
      (lasm->*opq)(kScratchRegister, value_reg);
      __ lock();
      __ cmpxchgq(dst_op, kScratchRegister);
      __ j(not_equal, &binop);
      break;
    }
    default:
      UNREACHABLE();
  }

  if (value.gp() != rax) {
    __ movq(value.gp(), rax);
  }
}
#undef __
}  // namespace liftoff

void LiftoffAssembler::AtomicAnd(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 StoreType type) {
  liftoff::AtomicBinop(this, &Assembler::andl, &Assembler::andq, dst_addr,
                       offset_reg, offset_imm, value, type);
}

void LiftoffAssembler::AtomicOr(Register dst_addr, Register offset_reg,
                                uint32_t offset_imm, LiftoffRegister value,
                                StoreType type) {
  liftoff::AtomicBinop(this, &Assembler::orl, &Assembler::orq, dst_addr,
                       offset_reg, offset_imm, value, type);
}

void LiftoffAssembler::AtomicXor(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 StoreType type) {
  liftoff::AtomicBinop(this, &Assembler::xorl, &Assembler::xorq, dst_addr,
                       offset_reg, offset_imm, value, type);
}

void LiftoffAssembler::AtomicExchange(Register dst_addr, Register offset_reg,
                                      uint32_t offset_imm,
                                      LiftoffRegister value, StoreType type) {
  DCHECK(!cache_state()->is_used(value));
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
  Operand dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm);
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      xchgb(value.gp(), dst_op);
      movzxbq(value.gp(), value.gp());
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      xchgw(value.gp(), dst_op);
      movzxwq(value.gp(), value.gp());
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      xchgl(value.gp(), dst_op);
      break;
    case StoreType::kI64Store:
      xchgq(value.gp(), dst_op);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicCompareExchange(
    Register dst_addr, Register offset_reg, uint32_t offset_imm,
    LiftoffRegister expected, LiftoffRegister new_value, LiftoffRegister result,
    StoreType type) {
  Register value_reg = new_value.gp();
  // The cmpxchg instruction uses rax to store the old value of the
  // compare-exchange primitive. Therefore we have to spill the register and
  // move any use to another register.
  LiftoffRegList pinned =
      LiftoffRegList::ForRegs(dst_addr, offset_reg, expected, value_reg);
  liftoff::ClearRegister(this, rax, {&dst_addr, &offset_reg, &value_reg},
                         pinned);
  if (expected.gp() != rax) {
    movq(rax, expected.gp());
  }

  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
  Operand dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm);

  lock();
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      cmpxchgb(dst_op, value_reg);
      movzxbq(result.gp(), rax);
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
      cmpxchgw(dst_op, value_reg);
      movzxwq(result.gp(), rax);
      break;
    }
    case StoreType::kI32Store: {
      cmpxchgl(dst_op, value_reg);
      if (result.gp() != rax) {
        movl(result.gp(), rax);
      }
      break;
    }
    case StoreType::kI64Store32: {
      cmpxchgl(dst_op, value_reg);
      // Zero extension.
      movl(result.gp(), rax);
      break;
    }
    case StoreType::kI64Store: {
      cmpxchgq(dst_op, value_reg);
      if (result.gp() != rax) {
        movq(result.gp(), rax);
      }
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicFence() { mfence(); }

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  Operand src(rbp, kSystemPointerSize * (caller_slot_idx + 1));
  liftoff::Load(this, dst, src, type);
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_offset, uint32_t src_offset,
                                      ValueType type) {
  DCHECK_NE(dst_offset, src_offset);
  Operand dst = liftoff::GetStackSlot(dst_offset);
  Operand src = liftoff::GetStackSlot(src_offset);
  if (type.element_size_log2() == 2) {
    movl(kScratchRegister, src);
    movl(dst, kScratchRegister);
  } else {
    DCHECK_EQ(3, type.element_size_log2());
    movq(kScratchRegister, src);
    movq(dst, kScratchRegister);
  }
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
  } else if (type == kWasmF64) {
    Movsd(dst, src);
  } else {
    DCHECK_EQ(kWasmS128, type);
    Movapd(dst, src);
  }
}

void LiftoffAssembler::Spill(int offset, LiftoffRegister reg, ValueType type) {
  RecordUsedSpillOffset(offset);
  Operand dst = liftoff::GetStackSlot(offset);
  switch (type.kind()) {
    case ValueType::kI32:
      movl(dst, reg.gp());
      break;
    case ValueType::kI64:
      movq(dst, reg.gp());
      break;
    case ValueType::kF32:
      Movss(dst, reg.fp());
      break;
    case ValueType::kF64:
      Movsd(dst, reg.fp());
      break;
    case ValueType::kS128:
      Movdqu(dst, reg.fp());
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
      movl(dst, Immediate(value.to_i32()));
      break;
    case ValueType::kI64: {
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

void LiftoffAssembler::Fill(LiftoffRegister reg, int offset, ValueType type) {
  Operand src = liftoff::GetStackSlot(offset);
  switch (type.kind()) {
    case ValueType::kI32:
      movl(reg.gp(), src);
      break;
    case ValueType::kI64:
      movq(reg.gp(), src);
      break;
    case ValueType::kF32:
      Movss(reg.fp(), src);
      break;
    case ValueType::kF64:
      Movsd(reg.fp(), src);
      break;
    case ValueType::kS128:
      Movdqu(reg.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register, int offset, RegPairHalf) {
  UNREACHABLE();
}

void LiftoffAssembler::FillStackSlotsWithZero(int start, int size) {
  DCHECK_LT(0, size);
  RecordUsedSpillOffset(start + size);

  if (size <= 3 * kStackSlotSize) {
    // Special straight-line code for up to three slots
    // (7-10 bytes per slot: REX C7 <1-4 bytes op> <4 bytes imm>),
    // And a movd (6-9 byte) when size % 8 != 0;
    uint32_t remainder = size;
    for (; remainder >= kStackSlotSize; remainder -= kStackSlotSize) {
      movq(liftoff::GetStackSlot(start + remainder), Immediate(0));
    }
    DCHECK(remainder == 4 || remainder == 0);
    if (remainder) {
      movl(liftoff::GetStackSlot(start + remainder), Immediate(0));
    }
  } else {
    // General case for bigger counts.
    // This sequence takes 19-22 bytes (3 for pushes, 4-7 for lea, 2 for xor, 5
    // for mov, 2 for repstosl, 3 for pops).
    pushq(rax);
    pushq(rcx);
    pushq(rdi);
    leaq(rdi, liftoff::GetStackSlot(start + size));
    xorl(rax, rax);
    // Convert size (bytes) to doublewords (4-bytes).
    movl(rcx, Immediate(size / 4));
    repstosl();
    popq(rdi);
    popq(rcx);
    popq(rax);
  }
}

void LiftoffAssembler::emit_i32_add(Register dst, Register lhs, Register rhs) {
  if (lhs != dst) {
    leal(dst, Operand(lhs, rhs, times_1, 0));
  } else {
    addl(dst, rhs);
  }
}

void LiftoffAssembler::emit_i32_add(Register dst, Register lhs, int32_t imm) {
  if (lhs != dst) {
    leal(dst, Operand(lhs, imm));
  } else {
    addl(dst, Immediate(imm));
  }
}

void LiftoffAssembler::emit_i32_sub(Register dst, Register lhs, Register rhs) {
  if (dst != rhs) {
    // Default path.
    if (dst != lhs) movl(dst, lhs);
    subl(dst, rhs);
  } else if (lhs == rhs) {
    // Degenerate case.
    xorl(dst, dst);
  } else {
    // Emit {dst = lhs + -rhs} if dst == rhs.
    negl(dst);
    addl(dst, lhs);
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

template <void (Assembler::*op)(Register, Immediate),
          void (Assembler::*mov)(Register, Register)>
void EmitCommutativeBinOpImm(LiftoffAssembler* assm, Register dst, Register lhs,
                             int32_t imm) {
  if (dst != lhs) (assm->*mov)(dst, lhs);
  (assm->*op)(dst, Immediate(imm));
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_mul(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::imull, &Assembler::movl>(this, dst,
                                                                     lhs, rhs);
}

namespace liftoff {
enum class DivOrRem : uint8_t { kDiv, kRem };
template <typename type, DivOrRem div_or_rem>
void EmitIntDivOrRem(LiftoffAssembler* assm, Register dst, Register lhs,
                     Register rhs, Label* trap_div_by_zero,
                     Label* trap_div_unrepresentable) {
  constexpr bool needs_unrepresentable_check =
      std::is_signed<type>::value && div_or_rem == DivOrRem::kDiv;
  constexpr bool special_case_minus_1 =
      std::is_signed<type>::value && div_or_rem == DivOrRem::kRem;
  DCHECK_EQ(needs_unrepresentable_check, trap_div_unrepresentable != nullptr);

#define iop(name, ...)            \
  do {                            \
    if (sizeof(type) == 4) {      \
      assm->name##l(__VA_ARGS__); \
    } else {                      \
      assm->name##q(__VA_ARGS__); \
    }                             \
  } while (false)

  // For division, the lhs is always taken from {edx:eax}. Thus, make sure that
  // these registers are unused. If {rhs} is stored in one of them, move it to
  // another temporary register.
  // Do all this before any branch, such that the code is executed
  // unconditionally, as the cache state will also be modified unconditionally.
  liftoff::SpillRegisters(assm, rdx, rax);
  if (rhs == rax || rhs == rdx) {
    iop(mov, kScratchRegister, rhs);
    rhs = kScratchRegister;
  }

  // Check for division by zero.
  iop(test, rhs, rhs);
  assm->j(zero, trap_div_by_zero);

  Label done;
  if (needs_unrepresentable_check) {
    // Check for {kMinInt / -1}. This is unrepresentable.
    Label do_div;
    iop(cmp, rhs, Immediate(-1));
    assm->j(not_equal, &do_div);
    // {lhs} is min int if {lhs - 1} overflows.
    iop(cmp, lhs, Immediate(1));
    assm->j(overflow, trap_div_unrepresentable);
    assm->bind(&do_div);
  } else if (special_case_minus_1) {
    // {lhs % -1} is always 0 (needs to be special cased because {kMinInt / -1}
    // cannot be computed).
    Label do_rem;
    iop(cmp, rhs, Immediate(-1));
    assm->j(not_equal, &do_rem);
    // clang-format off
    // (conflicts with presubmit checks because it is confused about "xor")
    iop(xor, dst, dst);
    // clang-format on
    assm->jmp(&done);
    assm->bind(&do_rem);
  }

  // Now move {lhs} into {eax}, then zero-extend or sign-extend into {edx}, then
  // do the division.
  if (lhs != rax) iop(mov, rax, lhs);
  if (std::is_same<int32_t, type>::value) {  // i32
    assm->cdq();
    assm->idivl(rhs);
  } else if (std::is_same<uint32_t, type>::value) {  // u32
    assm->xorl(rdx, rdx);
    assm->divl(rhs);
  } else if (std::is_same<int64_t, type>::value) {  // i64
    assm->cqo();
    assm->idivq(rhs);
  } else {  // u64
    assm->xorq(rdx, rdx);
    assm->divq(rhs);
  }

  // Move back the result (in {eax} or {edx}) into the {dst} register.
  constexpr Register kResultReg = div_or_rem == DivOrRem::kDiv ? rax : rdx;
  if (dst != kResultReg) {
    iop(mov, dst, kResultReg);
  }
  if (special_case_minus_1) assm->bind(&done);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  liftoff::EmitIntDivOrRem<int32_t, liftoff::DivOrRem::kDiv>(
      this, dst, lhs, rhs, trap_div_by_zero, trap_div_unrepresentable);
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<uint32_t, liftoff::DivOrRem::kDiv>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<int32_t, liftoff::DivOrRem::kRem>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<uint32_t, liftoff::DivOrRem::kRem>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_and(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::andl, &Assembler::movl>(this, dst,
                                                                    lhs, rhs);
}

void LiftoffAssembler::emit_i32_and(Register dst, Register lhs, int32_t imm) {
  liftoff::EmitCommutativeBinOpImm<&Assembler::andl, &Assembler::movl>(
      this, dst, lhs, imm);
}

void LiftoffAssembler::emit_i32_or(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::orl, &Assembler::movl>(this, dst,
                                                                   lhs, rhs);
}

void LiftoffAssembler::emit_i32_or(Register dst, Register lhs, int32_t imm) {
  liftoff::EmitCommutativeBinOpImm<&Assembler::orl, &Assembler::movl>(this, dst,
                                                                      lhs, imm);
}

void LiftoffAssembler::emit_i32_xor(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::xorl, &Assembler::movl>(this, dst,
                                                                    lhs, rhs);
}

void LiftoffAssembler::emit_i32_xor(Register dst, Register lhs, int32_t imm) {
  liftoff::EmitCommutativeBinOpImm<&Assembler::xorl, &Assembler::movl>(
      this, dst, lhs, imm);
}

namespace liftoff {
template <ValueType::Kind type>
inline void EmitShiftOperation(LiftoffAssembler* assm, Register dst,
                               Register src, Register amount,
                               void (Assembler::*emit_shift)(Register)) {
  // If dst is rcx, compute into the scratch register first, then move to rcx.
  if (dst == rcx) {
    assm->Move(kScratchRegister, src, ValueType(type));
    if (amount != rcx) assm->Move(rcx, amount, ValueType(type));
    (assm->*emit_shift)(kScratchRegister);
    assm->Move(rcx, kScratchRegister, ValueType(type));
    return;
  }

  // Move amount into rcx. If rcx is in use, move its content into the scratch
  // register. If src is rcx, src is now the scratch register.
  bool use_scratch = false;
  if (amount != rcx) {
    use_scratch =
        src == rcx || assm->cache_state()->is_used(LiftoffRegister(rcx));
    if (use_scratch) assm->movq(kScratchRegister, rcx);
    if (src == rcx) src = kScratchRegister;
    assm->Move(rcx, amount, ValueType(type));
  }

  // Do the actual shift.
  if (dst != src) assm->Move(dst, src, ValueType(type));
  (assm->*emit_shift)(dst);

  // Restore rcx if needed.
  if (use_scratch) assm->movq(rcx, kScratchRegister);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_shl(Register dst, Register src,
                                    Register amount) {
  liftoff::EmitShiftOperation<ValueType::kI32>(this, dst, src, amount,
                                               &Assembler::shll_cl);
}

void LiftoffAssembler::emit_i32_shl(Register dst, Register src,
                                    int32_t amount) {
  if (dst != src) movl(dst, src);
  shll(dst, Immediate(amount & 31));
}

void LiftoffAssembler::emit_i32_sar(Register dst, Register src,
                                    Register amount) {
  liftoff::EmitShiftOperation<ValueType::kI32>(this, dst, src, amount,
                                               &Assembler::sarl_cl);
}

void LiftoffAssembler::emit_i32_sar(Register dst, Register src,
                                    int32_t amount) {
  if (dst != src) movl(dst, src);
  sarl(dst, Immediate(amount & 31));
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register src,
                                    Register amount) {
  liftoff::EmitShiftOperation<ValueType::kI32>(this, dst, src, amount,
                                               &Assembler::shrl_cl);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register src,
                                    int32_t amount) {
  if (dst != src) movl(dst, src);
  shrl(dst, Immediate(amount & 31));
}

void LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  Lzcntl(dst, src);
}

void LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  Tzcntl(dst, src);
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
    leaq(dst.gp(), Operand(lhs.gp(), rhs.gp(), times_1, 0));
  } else {
    addq(dst.gp(), rhs.gp());
  }
}

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    int32_t imm) {
  if (lhs.gp() != dst.gp()) {
    leaq(dst.gp(), Operand(lhs.gp(), imm));
  } else {
    addq(dst.gp(), Immediate(imm));
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

void LiftoffAssembler::emit_i64_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::imulq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), rhs.gp());
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  liftoff::EmitIntDivOrRem<int64_t, liftoff::DivOrRem::kDiv>(
      this, dst.gp(), lhs.gp(), rhs.gp(), trap_div_by_zero,
      trap_div_unrepresentable);
  return true;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<uint64_t, liftoff::DivOrRem::kDiv>(
      this, dst.gp(), lhs.gp(), rhs.gp(), trap_div_by_zero, nullptr);
  return true;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<int64_t, liftoff::DivOrRem::kRem>(
      this, dst.gp(), lhs.gp(), rhs.gp(), trap_div_by_zero, nullptr);
  return true;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<uint64_t, liftoff::DivOrRem::kRem>(
      this, dst.gp(), lhs.gp(), rhs.gp(), trap_div_by_zero, nullptr);
  return true;
}

void LiftoffAssembler::emit_i64_and(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::andq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), rhs.gp());
}

void LiftoffAssembler::emit_i64_and(LiftoffRegister dst, LiftoffRegister lhs,
                                    int32_t imm) {
  liftoff::EmitCommutativeBinOpImm<&Assembler::andq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), imm);
}

void LiftoffAssembler::emit_i64_or(LiftoffRegister dst, LiftoffRegister lhs,
                                   LiftoffRegister rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::orq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), rhs.gp());
}

void LiftoffAssembler::emit_i64_or(LiftoffRegister dst, LiftoffRegister lhs,
                                   int32_t imm) {
  liftoff::EmitCommutativeBinOpImm<&Assembler::orq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), imm);
}

void LiftoffAssembler::emit_i64_xor(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::xorq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), rhs.gp());
}

void LiftoffAssembler::emit_i64_xor(LiftoffRegister dst, LiftoffRegister lhs,
                                    int32_t imm) {
  liftoff::EmitCommutativeBinOpImm<&Assembler::xorq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), imm);
}

void LiftoffAssembler::emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  liftoff::EmitShiftOperation<ValueType::kI64>(this, dst.gp(), src.gp(), amount,
                                               &Assembler::shlq_cl);
}

void LiftoffAssembler::emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                                    int32_t amount) {
  if (dst.gp() != src.gp()) movq(dst.gp(), src.gp());
  shlq(dst.gp(), Immediate(amount & 63));
}

void LiftoffAssembler::emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  liftoff::EmitShiftOperation<ValueType::kI64>(this, dst.gp(), src.gp(), amount,
                                               &Assembler::sarq_cl);
}

void LiftoffAssembler::emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                                    int32_t amount) {
  if (dst.gp() != src.gp()) movq(dst.gp(), src.gp());
  sarq(dst.gp(), Immediate(amount & 63));
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  liftoff::EmitShiftOperation<ValueType::kI64>(this, dst.gp(), src.gp(), amount,
                                               &Assembler::shrq_cl);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    int32_t amount) {
  if (dst != src) movq(dst.gp(), src.gp());
  shrq(dst.gp(), Immediate(amount & 63));
}

void LiftoffAssembler::emit_i64_clz(LiftoffRegister dst, LiftoffRegister src) {
  Lzcntq(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_i64_ctz(LiftoffRegister dst, LiftoffRegister src) {
  Tzcntq(dst.gp(), src.gp());
}

bool LiftoffAssembler::emit_i64_popcnt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  if (!CpuFeatures::IsSupported(POPCNT)) return false;
  CpuFeatureScope scope(this, POPCNT);
  popcntq(dst.gp(), src.gp());
  return true;
}

void LiftoffAssembler::emit_u32_to_intptr(Register dst, Register src) {
  movl(dst, src);
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
  dop(Ucomis, lhs, rhs);
  assm->j(parity_even, &is_nan, Label::kNear);   // PF=1
  assm->j(below, &lhs_below_rhs, Label::kNear);  // CF=1
  assm->j(above, &lhs_above_rhs, Label::kNear);  // CF=0 && ZF=0

  // If we get here, then either
  // a) {lhs == rhs},
  // b) {lhs == -0.0} and {rhs == 0.0}, or
  // c) {lhs == 0.0} and {rhs == -0.0}.
  // For a), it does not matter whether we return {lhs} or {rhs}. Check the sign
  // bit of {rhs} to differentiate b) and c).
  dop(Movmskp, kScratchRegister, rhs);
  assm->testl(kScratchRegister, Immediate(1));
  assm->j(zero, &lhs_below_rhs, Label::kNear);
  assm->jmp(&lhs_above_rhs, Label::kNear);

  assm->bind(&is_nan);
  // Create a NaN output.
  dop(Xorp, dst, dst);
  dop(Divs, dst, dst);
  assm->jmp(&done, Label::kNear);

  assm->bind(&lhs_below_rhs);
  DoubleRegister lhs_below_rhs_src = min_or_max == MinOrMax::kMin ? lhs : rhs;
  if (dst != lhs_below_rhs_src) dop(Movs, dst, lhs_below_rhs_src);
  assm->jmp(&done, Label::kNear);

  assm->bind(&lhs_above_rhs);
  DoubleRegister lhs_above_rhs_src = min_or_max == MinOrMax::kMin ? rhs : lhs;
  if (dst != lhs_above_rhs_src) dop(Movs, dst, lhs_above_rhs_src);

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
  Movd(kScratchRegister, lhs);
  andl(kScratchRegister, Immediate(~kF32SignBit));
  Movd(liftoff::kScratchRegister2, rhs);
  andl(liftoff::kScratchRegister2, Immediate(kF32SignBit));
  orl(kScratchRegister, liftoff::kScratchRegister2);
  Movd(dst, kScratchRegister);
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

bool LiftoffAssembler::emit_f32_ceil(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  Roundss(dst, src, kRoundUp);
  return true;
}

bool LiftoffAssembler::emit_f32_floor(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  Roundss(dst, src, kRoundDown);
  return true;
}

bool LiftoffAssembler::emit_f32_trunc(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  Roundss(dst, src, kRoundToZero);
  return true;
}

bool LiftoffAssembler::emit_f32_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  Roundss(dst, src, kRoundToNearest);
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

void LiftoffAssembler::emit_f64_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<double>(this, dst, lhs, rhs,
                                     liftoff::MinOrMax::kMin);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  // Extract sign bit from {rhs} into {kScratchRegister2}.
  Movq(liftoff::kScratchRegister2, rhs);
  shrq(liftoff::kScratchRegister2, Immediate(63));
  shlq(liftoff::kScratchRegister2, Immediate(63));
  // Reset sign bit of {lhs} (in {kScratchRegister}).
  Movq(kScratchRegister, lhs);
  btrq(kScratchRegister, Immediate(63));
  // Combine both values into {kScratchRegister} and move into {dst}.
  orq(kScratchRegister, liftoff::kScratchRegister2);
  Movq(dst, kScratchRegister);
}

void LiftoffAssembler::emit_f64_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<double>(this, dst, lhs, rhs,
                                     liftoff::MinOrMax::kMax);
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

bool LiftoffAssembler::emit_f64_ceil(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  Roundsd(dst, src, kRoundUp);
  return true;
}

bool LiftoffAssembler::emit_f64_floor(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  Roundsd(dst, src, kRoundDown);
  return true;
}

bool LiftoffAssembler::emit_f64_trunc(DoubleRegister dst, DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  Roundsd(dst, src, kRoundToZero);
  return true;
}

bool LiftoffAssembler::emit_f64_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
  Roundsd(dst, src, kRoundToNearest);
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
                                     DoubleRegister converted_back) {
  if (std::is_same<double, src_type>::value) {  // f64
    if (std::is_same<int32_t, dst_type>::value) {  // f64 -> i32
      assm->Cvttsd2si(dst, src);
      assm->Cvtlsi2sd(converted_back, dst);
    } else if (std::is_same<uint32_t, dst_type>::value) {  // f64 -> u32
      assm->Cvttsd2siq(dst, src);
      assm->movl(dst, dst);
      assm->Cvtqsi2sd(converted_back, dst);
    } else if (std::is_same<int64_t, dst_type>::value) {  // f64 -> i64
      assm->Cvttsd2siq(dst, src);
      assm->Cvtqsi2sd(converted_back, dst);
    } else {
      UNREACHABLE();
    }
  } else {                                  // f32
    if (std::is_same<int32_t, dst_type>::value) {  // f32 -> i32
      assm->Cvttss2si(dst, src);
      assm->Cvtlsi2ss(converted_back, dst);
    } else if (std::is_same<uint32_t, dst_type>::value) {  // f32 -> u32
      assm->Cvttss2siq(dst, src);
      assm->movl(dst, dst);
      assm->Cvtqsi2ss(converted_back, dst);
    } else if (std::is_same<int64_t, dst_type>::value) {  // f32 -> i64
      assm->Cvttss2siq(dst, src);
      assm->Cvtqsi2ss(converted_back, dst);
    } else {
      UNREACHABLE();
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

  DoubleRegister rounded = kScratchDoubleReg;
  DoubleRegister converted_back = kScratchDoubleReg2;

  if (std::is_same<double, src_type>::value) {  // f64
    assm->Roundsd(rounded, src, kRoundToZero);
  } else {  // f32
    assm->Roundss(rounded, src, kRoundToZero);
  }
  ConvertFloatToIntAndBack<dst_type, src_type>(assm, dst, rounded,
                                               converted_back);
  if (std::is_same<double, src_type>::value) {  // f64
    assm->Ucomisd(converted_back, rounded);
  } else {  // f32
    assm->Ucomiss(converted_back, rounded);
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
      movl(dst.gp(), src.gp());
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
      movsxlq(dst.gp(), src.gp());
      return true;
    case kExprI64SConvertF32:
      return liftoff::EmitTruncateFloatToInt<int64_t, float>(this, dst.gp(),
                                                             src.fp(), trap);
    case kExprI64UConvertF32: {
      RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
      Cvttss2uiq(dst.gp(), src.fp(), trap);
      return true;
    }
    case kExprI64SConvertF64:
      return liftoff::EmitTruncateFloatToInt<int64_t, double>(this, dst.gp(),
                                                              src.fp(), trap);
    case kExprI64UConvertF64: {
      RETURN_FALSE_IF_MISSING_CPU_FEATURE(SSE4_1);
      Cvttsd2uiq(dst.gp(), src.fp(), trap);
      return true;
    }
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
      Cvtqui2ss(dst.fp(), src.gp());
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
      Cvtqui2sd(dst.fp(), src.gp());
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

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  movsxbl(dst, src);
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  movsxwl(dst, src);
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  movsxbq(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  movsxwq(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  movsxlq(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_jump(Label* label) { jmp(label); }

void LiftoffAssembler::emit_jump(Register target) { jmp(target); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  if (rhs != no_reg) {
    switch (type.kind()) {
      case ValueType::kI32:
        cmpl(lhs, rhs);
        break;
      case ValueType::kI64:
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
  // If PF is one, one of the operands was NaN. This needs special handling.
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

// TODO(fanchenk): Distinguish mov* if data bypass delay matter.
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
  Pextrq(kScratchRegister, lhs.fp(), static_cast<int8_t>(imm_lane_idx));
  Movq(dst.fp(), kScratchRegister);
}

void LiftoffAssembler::emit_f64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    if (imm_lane_idx == 0) {
      vpblendw(dst.fp(), src1.fp(), src2.fp(), 0b00001111);
    } else {
      vmovlhps(dst.fp(), src1.fp(), src2.fp());
    }
  } else {
    CpuFeatureScope scope(this, SSE4_1);
    if (dst.fp() != src1.fp()) movaps(dst.fp(), src1.fp());
    if (imm_lane_idx == 0) {
      pblendw(dst.fp(), src2.fp(), 0b00001111);
    } else {
      movlhps(dst.fp(), src2.fp());
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
  if (dst.fp() != src.fp()) {
    Movss(dst.fp(), src.fp());
  }
  Shufps(dst.fp(), src.fp(), static_cast<byte>(0));
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
  Movq(dst.fp(), src.gp());
  Movddup(dst.fp(), dst.fp());
}

void LiftoffAssembler::emit_i64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  Pextrq(dst.gp(), lhs.fp(), static_cast<int8_t>(imm_lane_idx));
}

void LiftoffAssembler::emit_i64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpinsrq(dst.fp(), src1.fp(), src2.gp(), imm_lane_idx);
  } else {
    CpuFeatureScope scope(this, SSE4_1);
    if (dst.fp() != src1.fp()) movaps(dst.fp(), src1.fp());
    pinsrq(dst.fp(), src2.gp(), imm_lane_idx);
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
  Pmuludq(tmp1.fp(), rhs.fp());
  // Multiply high dword of each qword of right with left.
  Psrlq(tmp2.fp(), 32);
  Pmuludq(tmp2.fp(), lhs.fp());
  Paddq(tmp2.fp(), tmp1.fp());
  Psllq(tmp2.fp(), 32);
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpmuludq(dst.fp(), lhs.fp(), rhs.fp());
  } else {
    if (dst.fp() != lhs.fp()) movaps(dst.fp(), lhs.fp());
    pmuludq(dst.fp(), rhs.fp());
  }
  Paddq(dst.fp(), tmp2.fp());
}

void LiftoffAssembler::emit_i32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Movd(dst.fp(), src.gp());
  Pshufd(dst.fp(), dst.fp(), static_cast<uint8_t>(0));
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
  Pshuflw(dst.fp(), dst.fp(), static_cast<uint8_t>(0));
  Pshufd(dst.fp(), dst.fp(), static_cast<uint8_t>(0));
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
  movsxwl(dst.gp(), dst.gp());
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
  Pxor(kScratchDoubleReg, kScratchDoubleReg);
  Pshufb(dst.fp(), kScratchDoubleReg);
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
  movsxbl(dst.gp(), dst.gp());
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
    vpsrlw(kScratchDoubleReg, rhs.fp(), 8);
    // t = I16x8Mul(t0, t1)
    //    => __PP __PP ...  __PP  __PP
    vpmullw(tmp.fp(), tmp.fp(), kScratchDoubleReg);
    // s = left * 256
    vpsllw(kScratchDoubleReg, lhs.fp(), 8);
    // dst = I16x8Mul(left * 256, right)
    //    => pp__ pp__ ...  pp__  pp__
    vpmullw(dst.fp(), kScratchDoubleReg, rhs.fp());
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
    movaps(kScratchDoubleReg, rhs.fp());
    psrlw(tmp.fp(), 8);
    psrlw(kScratchDoubleReg, 8);
    // dst = left * 256
    psllw(dst.fp(), 8);
    // t = I16x8Mul(t, s)
    //    => __PP __PP ...  __PP  __PP
    pmullw(tmp.fp(), kScratchDoubleReg);
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
  cmpq(rsp, Operand(limit_address, 0));
  j(below_equal, ool_code);
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0);
  CallCFunction(ExternalReference::wasm_call_trap_callback_for_testing(), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  TurboAssembler::AssertUnreachable(reason);
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
    AllocateStackSpace(num_fp_regs * kSimd128Size);
    unsigned offset = 0;
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetFirstRegSet();
      Movdqu(Operand(rsp, offset), reg.fp());
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
    Movdqu(reg.fp(), Operand(rsp, fp_offset));
    fp_regs.clear(reg);
    fp_offset += kSimd128Size;
  }
  if (fp_offset) addq(rsp, Immediate(fp_offset));
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetLastRegSet();
    popq(reg.gp());
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
    liftoff::Store(this, Operand(rsp, arg_bytes), *args++, param_type);
    arg_bytes += param_type.element_size_bytes();
  }
  DCHECK_LE(arg_bytes, stack_bytes);

  // Pass a pointer to the buffer with the arguments to the C function.
  movq(arg_reg_1, rsp);

  constexpr int kNumCCallArgs = 1;

  // Now call the C function.
  PrepareCallCFunction(kNumCCallArgs);
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* next_result_reg = rets;
  if (sig->return_count() > 0) {
    DCHECK_EQ(1, sig->return_count());
    constexpr Register kReturnReg = rax;
    if (kReturnReg != next_result_reg->gp()) {
      Move(*next_result_reg, LiftoffRegister(kReturnReg), sig->GetReturn(0));
    }
    ++next_result_reg;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_type != kWasmStmt) {
    liftoff::Load(this, *next_result_reg, Operand(rsp, 0), out_argument_type);
  }

  addq(rsp, Immediate(stack_bytes));
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  near_call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallIndirect(const wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  if (target == no_reg) {
    popq(kScratchRegister);
    target = kScratchRegister;
  }
  if (FLAG_untrusted_code_mitigations) {
    RetpolineCall(target);
  } else {
    call(target);
  }
}

void LiftoffAssembler::CallRuntimeStub(WasmCode::RuntimeStubId sid) {
  // A direct call to a wasm runtime stub defined in this module.
  // Just encode the stub index. This will be patched at relocation.
  near_call(static_cast<Address>(sid), RelocInfo::WASM_STUB_CALL);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  AllocateStackSpace(size);
  movq(addr, rsp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  addq(rsp, Immediate(size));
}

void LiftoffStackSlots::Construct() {
  for (auto& slot : slots_) {
    const LiftoffAssembler::VarState& src = slot.src_;
    switch (src.loc()) {
      case LiftoffAssembler::VarState::kStack:
        if (src.type() == kWasmI32) {
          // Load i32 values to a register first to ensure they are zero
          // extended.
          asm_->movl(kScratchRegister, liftoff::GetStackSlot(slot.src_offset_));
          asm_->pushq(kScratchRegister);
        } else if (src.type() == kWasmS128) {
          // Since offsets are subtracted from sp, we need a smaller offset to
          // push the top of a s128 value.
          asm_->pushq(liftoff::GetStackSlot(slot.src_offset_ - 8));
          asm_->pushq(liftoff::GetStackSlot(slot.src_offset_));
        } else {
          // For all other types, just push the whole (8-byte) stack slot.
          // This is also ok for f32 values (even though we copy 4 uninitialized
          // bytes), because f32 and f64 values are clearly distinguished in
          // Turbofan, so the uninitialized bytes are never accessed.
          asm_->pushq(liftoff::GetStackSlot(slot.src_offset_));
        }
        break;
      case LiftoffAssembler::VarState::kRegister:
        liftoff::push(asm_, src.reg(), src.type());
        break;
      case LiftoffAssembler::VarState::kIntConst:
        asm_->pushq(Immediate(src.i32_const()));
        break;
    }
  }
}

#undef RETURN_FALSE_IF_MISSING_CPU_FEATURE

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_X64_LIFTOFF_ASSEMBLER_X64_H_

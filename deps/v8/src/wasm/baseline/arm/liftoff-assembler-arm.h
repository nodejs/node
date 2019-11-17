// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_ARM_LIFTOFF_ASSEMBLER_ARM_H_
#define V8_WASM_BASELINE_ARM_LIFTOFF_ASSEMBLER_ARM_H_

#include "src/wasm/baseline/liftoff-assembler.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace liftoff {

//  half
//  slot        Frame
//  -----+--------------------+---------------------------
//  n+3  |   parameter n      |
//  ...  |       ...          |
//   4   |   parameter 1      | or parameter 2
//   3   |   parameter 0      | or parameter 1
//   2   |  (result address)  | or parameter 0
//  -----+--------------------+---------------------------
//   1   | return addr (lr)   |
//   0   | previous frame (fp)|
//  -----+--------------------+  <-- frame ptr (fp)
//  -1   | 0xa: WASM_COMPILED |
//  -2   |     instance       |
//  -----+--------------------+---------------------------
//  -3   |    slot 0 (high)   |   ^
//  -4   |    slot 0 (low)    |   |
//  -5   |    slot 1 (high)   | Frame slots
//  -6   |    slot 1 (low)    |   |
//       |                    |   v
//  -----+--------------------+  <-- stack ptr (sp)
//
static_assert(2 * kSystemPointerSize == LiftoffAssembler::kStackSlotSize,
              "Slot size should be twice the size of the 32 bit pointer.");
constexpr int32_t kInstanceOffset = 2 * kSystemPointerSize;
constexpr int32_t kFirstStackSlotOffset =
    kInstanceOffset + 2 * kSystemPointerSize;
constexpr int32_t kConstantStackSpace = kSystemPointerSize;
// kPatchInstructionsRequired sets a maximum limit of how many instructions that
// PatchPrepareStackFrame will use in order to increase the stack appropriately.
// Three instructions are required to sub a large constant, movw + movt + sub.
constexpr int32_t kPatchInstructionsRequired = 3;

inline int GetStackSlotOffset(uint32_t index) {
  return kFirstStackSlotOffset + index * LiftoffAssembler::kStackSlotSize;
}

inline MemOperand GetStackSlot(uint32_t index) {
  return MemOperand(fp, -GetStackSlotOffset(index));
}

inline MemOperand GetHalfStackSlot(uint32_t index, RegPairHalf half) {
  int32_t half_offset =
      half == kLowWord ? 0 : LiftoffAssembler::kStackSlotSize / 2;
  int32_t offset = kFirstStackSlotOffset +
                   index * LiftoffAssembler::kStackSlotSize - half_offset;
  return MemOperand(fp, -offset);
}

inline MemOperand GetInstanceOperand() {
  return MemOperand(fp, -kInstanceOffset);
}

inline MemOperand GetMemOp(LiftoffAssembler* assm,
                           UseScratchRegisterScope* temps, Register addr,
                           Register offset, int32_t offset_imm) {
  if (offset != no_reg) {
    if (offset_imm == 0) return MemOperand(addr, offset);
    Register tmp = temps->Acquire();
    assm->add(tmp, offset, Operand(offset_imm));
    return MemOperand(addr, tmp);
  }
  return MemOperand(addr, offset_imm);
}

inline Register CalculateActualAddress(LiftoffAssembler* assm,
                                       UseScratchRegisterScope* temps,
                                       Register addr_reg, Register offset_reg,
                                       int32_t offset_imm) {
  if (offset_reg == no_reg && offset_imm == 0) {
    return addr_reg;
  }
  Register actual_addr_reg = temps->Acquire();
  if (offset_reg == no_reg) {
    assm->add(actual_addr_reg, addr_reg, Operand(offset_imm));
  } else {
    assm->add(actual_addr_reg, addr_reg, Operand(offset_reg));
    if (offset_imm != 0) {
      assm->add(actual_addr_reg, actual_addr_reg, Operand(offset_imm));
    }
  }
  return actual_addr_reg;
}

inline Condition MakeUnsigned(Condition cond) {
  switch (cond) {
    case kSignedLessThan:
      return kUnsignedLessThan;
    case kSignedLessEqual:
      return kUnsignedLessEqual;
    case kSignedGreaterThan:
      return kUnsignedGreaterThan;
    case kSignedGreaterEqual:
      return kUnsignedGreaterEqual;
    case kEqual:
    case kUnequal:
    case kUnsignedLessThan:
    case kUnsignedLessEqual:
    case kUnsignedGreaterThan:
    case kUnsignedGreaterEqual:
      return cond;
    default:
      UNREACHABLE();
  }
}

template <void (Assembler::*op)(Register, Register, Register, SBit, Condition),
          void (Assembler::*op_with_carry)(Register, Register, const Operand&,
                                           SBit, Condition)>
inline void I64Binop(LiftoffAssembler* assm, LiftoffRegister dst,
                     LiftoffRegister lhs, LiftoffRegister rhs) {
  UseScratchRegisterScope temps(assm);
  Register scratch = dst.low_gp();
  bool can_use_dst =
      dst.low_gp() != lhs.high_gp() && dst.low_gp() != rhs.high_gp();
  if (!can_use_dst) {
    scratch = temps.Acquire();
  }
  (assm->*op)(scratch, lhs.low_gp(), rhs.low_gp(), SetCC, al);
  (assm->*op_with_carry)(dst.high_gp(), lhs.high_gp(), Operand(rhs.high_gp()),
                         LeaveCC, al);
  if (!can_use_dst) {
    assm->mov(dst.low_gp(), scratch);
  }
}

template <void (Assembler::*op)(Register, Register, const Operand&, SBit,
                                Condition),
          void (Assembler::*op_with_carry)(Register, Register, const Operand&,
                                           SBit, Condition)>
inline void I64BinopI(LiftoffAssembler* assm, LiftoffRegister dst,
                      LiftoffRegister lhs, int32_t imm) {
  UseScratchRegisterScope temps(assm);
  Register scratch = dst.low_gp();
  bool can_use_dst = dst.low_gp() != lhs.high_gp();
  if (!can_use_dst) {
    scratch = temps.Acquire();
  }
  (assm->*op)(scratch, lhs.low_gp(), Operand(imm), SetCC, al);
  // Top half of the immediate sign extended, either 0 or -1.
  int32_t sign_extend = imm < 0 ? -1 : 0;
  (assm->*op_with_carry)(dst.high_gp(), lhs.high_gp(), Operand(sign_extend),
                         LeaveCC, al);
  if (!can_use_dst) {
    assm->mov(dst.low_gp(), scratch);
  }
}

template <void (TurboAssembler::*op)(Register, Register, Register, Register,
                                     Register),
          bool is_left_shift>
inline void I64Shiftop(LiftoffAssembler* assm, LiftoffRegister dst,
                       LiftoffRegister src, Register amount,
                       LiftoffRegList pinned) {
  Register src_low = src.low_gp();
  Register src_high = src.high_gp();
  Register dst_low = dst.low_gp();
  Register dst_high = dst.high_gp();
  // Left shift writes {dst_high} then {dst_low}, right shifts write {dst_low}
  // then {dst_high}.
  Register clobbered_dst_reg = is_left_shift ? dst_high : dst_low;
  pinned.set(clobbered_dst_reg);
  pinned.set(src);
  Register amount_capped =
      pinned.set(assm->GetUnusedRegister(kGpReg, pinned)).gp();
  assm->and_(amount_capped, amount, Operand(0x3F));

  // Ensure that writing the first half of {dst} does not overwrite the still
  // needed half of {src}.
  Register* later_src_reg = is_left_shift ? &src_low : &src_high;
  if (*later_src_reg == clobbered_dst_reg) {
    *later_src_reg = assm->GetUnusedRegister(kGpReg, pinned).gp();
    assm->TurboAssembler::Move(*later_src_reg, clobbered_dst_reg);
  }

  (assm->*op)(dst_low, dst_high, src_low, src_high, amount_capped);
}

inline FloatRegister GetFloatRegister(DoubleRegister reg) {
  DCHECK_LT(reg.code(), kDoubleCode_d16);
  return LowDwVfpRegister::from_code(reg.code()).low();
}

enum class MinOrMax : uint8_t { kMin, kMax };
template <typename RegisterType>
inline void EmitFloatMinOrMax(LiftoffAssembler* assm, RegisterType dst,
                              RegisterType lhs, RegisterType rhs,
                              MinOrMax min_or_max) {
  DCHECK(RegisterType::kSizeInBytes == 4 || RegisterType::kSizeInBytes == 8);
  if (lhs == rhs) {
    assm->TurboAssembler::Move(dst, lhs);
    return;
  }
  Label done, is_nan;
  if (min_or_max == MinOrMax::kMin) {
    assm->TurboAssembler::FloatMin(dst, lhs, rhs, &is_nan);
  } else {
    assm->TurboAssembler::FloatMax(dst, lhs, rhs, &is_nan);
  }
  assm->b(&done);
  assm->bind(&is_nan);
  // Create a NaN output.
  assm->vadd(dst, lhs, rhs);
  assm->bind(&done);
}

}  // namespace liftoff

int LiftoffAssembler::PrepareStackFrame() {
  if (!CpuFeatures::IsSupported(ARMv7)) {
    bailout(kUnsupportedArchitecture, "Armv6 not supported");
    return 0;
  }
  uint32_t offset = static_cast<uint32_t>(pc_offset());
  // PatchPrepareStackFrame will patch this in order to increase the stack
  // appropriately. Additional nops are required as the bytes operand might
  // require extra moves to encode.
  for (int i = 0; i < liftoff::kPatchInstructionsRequired; i++) {
    nop();
  }
  DCHECK_EQ(offset + liftoff::kPatchInstructionsRequired * kInstrSize,
            pc_offset());
  return offset;
}

void LiftoffAssembler::PatchPrepareStackFrame(int offset,
                                              uint32_t stack_slots) {
  // Allocate space for instance plus what is needed for the frame slots.
  uint32_t bytes = liftoff::kConstantStackSpace + kStackSlotSize * stack_slots;
#ifdef USE_SIMULATOR
  // When using the simulator, deal with Liftoff which allocates the stack
  // before checking it.
  // TODO(arm): Remove this when the stack check mechanism will be updated.
  if (bytes > KB / 2) {
    bailout(kOtherReason,
            "Stack limited to 512 bytes to avoid a bug in StackCheck");
    return;
  }
#endif
  PatchingAssembler patching_assembler(AssemblerOptions{},
                                       buffer_start_ + offset,
                                       liftoff::kPatchInstructionsRequired);
#if V8_OS_WIN
  if (bytes > kStackPageSize) {
    // Generate OOL code (at the end of the function, where the current
    // assembler is pointing) to do the explicit stack limit check (see
    // https://docs.microsoft.com/en-us/previous-versions/visualstudio/
    // visual-studio-6.0/aa227153(v=vs.60)).
    // At the function start, emit a jump to that OOL code (from {offset} to
    // {pc_offset()}).
    int ool_offset = pc_offset() - offset;
    patching_assembler.b(ool_offset - Instruction::kPcLoadDelta);
    patching_assembler.PadWithNops();

    // Now generate the OOL code.
    AllocateStackSpace(bytes);
    // Jump back to the start of the function (from {pc_offset()} to {offset +
    // liftoff::kPatchInstructionsRequired * kInstrSize}).
    int func_start_offset =
        offset + liftoff::kPatchInstructionsRequired * kInstrSize - pc_offset();
    b(func_start_offset - Instruction::kPcLoadDelta);
    return;
  }
#endif
  patching_assembler.sub(sp, sp, Operand(bytes));
  patching_assembler.PadWithNops();
}

void LiftoffAssembler::FinishCode() { CheckConstPool(true, false); }

void LiftoffAssembler::AbortCompilation() { AbortedCodeGeneration(); }

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  switch (value.type()) {
    case kWasmI32:
      TurboAssembler::Move(reg.gp(), Operand(value.to_i32(), rmode));
      break;
    case kWasmI64: {
      DCHECK(RelocInfo::IsNone(rmode));
      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      TurboAssembler::Move(reg.low_gp(), Operand(low_word));
      TurboAssembler::Move(reg.high_gp(), Operand(high_word));
      break;
    }
    case kWasmF32:
      vmov(liftoff::GetFloatRegister(reg.fp()), value.to_f32_boxed());
      break;
    case kWasmF64: {
      Register extra_scratch = GetUnusedRegister(kGpReg).gp();
      vmov(reg.fp(), Double(value.to_f64_boxed().get_scalar()), extra_scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadFromInstance(Register dst, uint32_t offset,
                                        int size) {
  DCHECK_LE(offset, kMaxInt);
  DCHECK_EQ(4, size);
  ldr(dst, liftoff::GetInstanceOperand());
  ldr(dst, MemOperand(dst, offset));
}

void LiftoffAssembler::LoadTaggedPointerFromInstance(Register dst,
                                                     uint32_t offset) {
  LoadFromInstance(dst, offset, kTaggedSize);
}

void LiftoffAssembler::SpillInstance(Register instance) {
  str(instance, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  ldr(dst, liftoff::GetInstanceOperand());
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
  DCHECK_IMPLIES(type.value_type() == kWasmI64, dst.is_pair());
  // If offset_imm cannot be converted to int32 safely, we abort as a separate
  // check should cause this code to never be executed.
  // TODO(7881): Support when >2GB is required.
  if (!is_uint31(offset_imm)) {
    TurboAssembler::Abort(AbortReason::kOffsetOutOfRange);
    return;
  }
  UseScratchRegisterScope temps(this);
  if (type.value() == LoadType::kF64Load ||
      type.value() == LoadType::kF32Load) {
    Register actual_src_addr = liftoff::CalculateActualAddress(
        this, &temps, src_addr, offset_reg, offset_imm);
    if (type.value() == LoadType::kF64Load) {
      // Armv6 is not supported so Neon can be used to avoid alignment issues.
      CpuFeatureScope scope(this, NEON);
      vld1(Neon64, NeonListOperand(dst.fp()), NeonMemOperand(actual_src_addr));
    } else {
      // TODO(arm): Use vld1 for f32 when implemented in simulator as used for
      // f64. It supports unaligned access.
      Register scratch =
          (actual_src_addr == src_addr) ? temps.Acquire() : actual_src_addr;
      ldr(scratch, MemOperand(actual_src_addr));
      vmov(liftoff::GetFloatRegister(dst.fp()), scratch);
    }
  } else {
    MemOperand src_op =
        liftoff::GetMemOp(this, &temps, src_addr, offset_reg, offset_imm);
    if (protected_load_pc) *protected_load_pc = pc_offset();
    switch (type.value()) {
      case LoadType::kI32Load8U:
        ldrb(dst.gp(), src_op);
        break;
      case LoadType::kI64Load8U:
        ldrb(dst.low_gp(), src_op);
        mov(dst.high_gp(), Operand(0));
        break;
      case LoadType::kI32Load8S:
        ldrsb(dst.gp(), src_op);
        break;
      case LoadType::kI64Load8S:
        ldrsb(dst.low_gp(), src_op);
        asr(dst.high_gp(), dst.low_gp(), Operand(31));
        break;
      case LoadType::kI32Load16U:
        ldrh(dst.gp(), src_op);
        break;
      case LoadType::kI64Load16U:
        ldrh(dst.low_gp(), src_op);
        mov(dst.high_gp(), Operand(0));
        break;
      case LoadType::kI32Load16S:
        ldrsh(dst.gp(), src_op);
        break;
      case LoadType::kI32Load:
        ldr(dst.gp(), src_op);
        break;
      case LoadType::kI64Load16S:
        ldrsh(dst.low_gp(), src_op);
        asr(dst.high_gp(), dst.low_gp(), Operand(31));
        break;
      case LoadType::kI64Load32U:
        ldr(dst.low_gp(), src_op);
        mov(dst.high_gp(), Operand(0));
        break;
      case LoadType::kI64Load32S:
        ldr(dst.low_gp(), src_op);
        asr(dst.high_gp(), dst.low_gp(), Operand(31));
        break;
      case LoadType::kI64Load:
        ldr(dst.low_gp(), src_op);
        // GetMemOp may use a scratch register as the offset register, in which
        // case, calling GetMemOp again will fail due to the assembler having
        // ran out of scratch registers.
        if (temps.CanAcquire()) {
          src_op = liftoff::GetMemOp(this, &temps, src_addr, offset_reg,
                                     offset_imm + kSystemPointerSize);
        } else {
          add(src_op.rm(), src_op.rm(), Operand(kSystemPointerSize));
        }
        ldr(dst.high_gp(), src_op);
        break;
      default:
        UNREACHABLE();
    }
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  // If offset_imm cannot be converted to int32 safely, we abort as a separate
  // check should cause this code to never be executed.
  // TODO(7881): Support when >2GB is required.
  if (!is_uint31(offset_imm)) {
    TurboAssembler::Abort(AbortReason::kOffsetOutOfRange);
    return;
  }
  UseScratchRegisterScope temps(this);
  if (type.value() == StoreType::kF64Store) {
    Register actual_dst_addr = liftoff::CalculateActualAddress(
        this, &temps, dst_addr, offset_reg, offset_imm);
    // Armv6 is not supported so Neon can be used to avoid alignment issues.
    CpuFeatureScope scope(this, NEON);
    vst1(Neon64, NeonListOperand(src.fp()), NeonMemOperand(actual_dst_addr));
  } else if (type.value() == StoreType::kF32Store) {
    // TODO(arm): Use vst1 for f32 when implemented in simulator as used for
    // f64. It supports unaligned access.
    // CalculateActualAddress will only not use a scratch register if the
    // following condition holds, otherwise another register must be
    // retrieved.
    Register scratch = (offset_reg == no_reg && offset_imm == 0)
                           ? temps.Acquire()
                           : GetUnusedRegister(kGpReg, pinned).gp();
    Register actual_dst_addr = liftoff::CalculateActualAddress(
        this, &temps, dst_addr, offset_reg, offset_imm);
    vmov(scratch, liftoff::GetFloatRegister(src.fp()));
    str(scratch, MemOperand(actual_dst_addr));
  } else {
    MemOperand dst_op =
        liftoff::GetMemOp(this, &temps, dst_addr, offset_reg, offset_imm);
    if (protected_store_pc) *protected_store_pc = pc_offset();
    switch (type.value()) {
      case StoreType::kI64Store8:
        src = src.low();
        V8_FALLTHROUGH;
      case StoreType::kI32Store8:
        strb(src.gp(), dst_op);
        break;
      case StoreType::kI64Store16:
        src = src.low();
        V8_FALLTHROUGH;
      case StoreType::kI32Store16:
        strh(src.gp(), dst_op);
        break;
      case StoreType::kI64Store32:
        src = src.low();
        V8_FALLTHROUGH;
      case StoreType::kI32Store:
        str(src.gp(), dst_op);
        break;
      case StoreType::kI64Store:
        str(src.low_gp(), dst_op);
        // GetMemOp may use a scratch register as the offset register, in which
        // case, calling GetMemOp again will fail due to the assembler having
        // ran out of scratch registers.
        if (temps.CanAcquire()) {
          dst_op = liftoff::GetMemOp(this, &temps, dst_addr, offset_reg,
                                     offset_imm + kSystemPointerSize);
        } else {
          add(dst_op.rm(), dst_op.rm(), Operand(kSystemPointerSize));
        }
        str(src.high_gp(), dst_op);
        break;
      default:
        UNREACHABLE();
    }
  }
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  int32_t offset = (caller_slot_idx + 1) * kSystemPointerSize;
  MemOperand src(fp, offset);
  switch (type) {
    case kWasmI32:
      ldr(dst.gp(), src);
      break;
    case kWasmI64:
      ldr(dst.low_gp(), src);
      ldr(dst.high_gp(), MemOperand(fp, offset + kSystemPointerSize));
      break;
    case kWasmF32:
      vldr(liftoff::GetFloatRegister(dst.fp()), src);
      break;
    case kWasmF64:
      vldr(dst.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index,
                                      ValueType type) {
  DCHECK_NE(dst_index, src_index);
  LiftoffRegister reg = GetUnusedRegister(reg_class_for(type));
  Fill(reg, src_index, type);
  Spill(dst_index, reg, type);
}

void LiftoffAssembler::Move(Register dst, Register src, ValueType type) {
  DCHECK_NE(dst, src);
  DCHECK_EQ(type, kWasmI32);
  TurboAssembler::Move(dst, src);
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueType type) {
  DCHECK_NE(dst, src);
  if (type == kWasmF32) {
    vmov(liftoff::GetFloatRegister(dst), liftoff::GetFloatRegister(src));
  } else {
    DCHECK_EQ(kWasmF64, type);
    vmov(dst, src);
  }
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg,
                             ValueType type) {
  RecordUsedSpillSlot(index);
  MemOperand dst = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      str(reg.gp(), dst);
      break;
    case kWasmI64:
      str(reg.low_gp(), liftoff::GetHalfStackSlot(index, kLowWord));
      str(reg.high_gp(), liftoff::GetHalfStackSlot(index, kHighWord));
      break;
    case kWasmF32:
      vstr(liftoff::GetFloatRegister(reg.fp()), dst);
      break;
    case kWasmF64:
      vstr(reg.fp(), dst);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  RecordUsedSpillSlot(index);
  MemOperand dst = liftoff::GetStackSlot(index);
  UseScratchRegisterScope temps(this);
  Register src = no_reg;
  // The scratch register will be required by str if multiple instructions
  // are required to encode the offset, and so we cannot use it in that case.
  if (!ImmediateFitsAddrMode2Instruction(dst.offset())) {
    src = GetUnusedRegister(kGpReg).gp();
  } else {
    src = temps.Acquire();
  }
  switch (value.type()) {
    case kWasmI32:
      mov(src, Operand(value.to_i32()));
      str(src, dst);
      break;
    case kWasmI64: {
      int32_t low_word = value.to_i64();
      mov(src, Operand(low_word));
      str(src, liftoff::GetHalfStackSlot(index, kLowWord));
      int32_t high_word = value.to_i64() >> 32;
      mov(src, Operand(high_word));
      str(src, liftoff::GetHalfStackSlot(index, kHighWord));
      break;
    }
    default:
      // We do not track f32 and f64 constants, hence they are unreachable.
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index,
                            ValueType type) {
  switch (type) {
    case kWasmI32:
      ldr(reg.gp(), liftoff::GetStackSlot(index));
      break;
    case kWasmI64:
      ldr(reg.low_gp(), liftoff::GetHalfStackSlot(index, kLowWord));
      ldr(reg.high_gp(), liftoff::GetHalfStackSlot(index, kHighWord));
      break;
    case kWasmF32:
      vldr(liftoff::GetFloatRegister(reg.fp()), liftoff::GetStackSlot(index));
      break;
    case kWasmF64:
      vldr(reg.fp(), liftoff::GetStackSlot(index));
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register reg, uint32_t index,
                                   RegPairHalf half) {
  ldr(reg, liftoff::GetHalfStackSlot(index, half));
}

void LiftoffAssembler::FillStackSlotsWithZero(uint32_t index, uint32_t count) {
  DCHECK_LT(0, count);
  uint32_t last_stack_slot = index + count - 1;
  RecordUsedSpillSlot(last_stack_slot);

  // We need a zero reg. Always use r0 for that, and push it before to restore
  // its value afterwards.
  push(r0);
  mov(r0, Operand(0));

  if (count <= 5) {
    // Special straight-line code for up to five slots. Generates two
    // instructions per slot.
    for (uint32_t offset = 0; offset < count; ++offset) {
      str(r0, liftoff::GetHalfStackSlot(index + offset, kLowWord));
      str(r0, liftoff::GetHalfStackSlot(index + offset, kHighWord));
    }
  } else {
    // General case for bigger counts (9 instructions).
    // Use r1 for start address (inclusive), r2 for end address (exclusive).
    push(r1);
    push(r2);
    sub(r1, fp, Operand(liftoff::GetStackSlotOffset(last_stack_slot)));
    sub(r2, fp, Operand(liftoff::GetStackSlotOffset(index) - kStackSlotSize));

    Label loop;
    bind(&loop);
    str(r0, MemOperand(r1, /* offset */ kSystemPointerSize, PostIndex));
    cmp(r1, r2);
    b(&loop, ne);

    pop(r2);
    pop(r1);
  }

  pop(r0);
}

#define I32_BINOP(name, instruction)                             \
  void LiftoffAssembler::emit_##name(Register dst, Register lhs, \
                                     Register rhs) {             \
    instruction(dst, lhs, rhs);                                  \
  }
#define I32_BINOP_I(name, instruction)                           \
  I32_BINOP(name, instruction)                                   \
  void LiftoffAssembler::emit_##name(Register dst, Register lhs, \
                                     int32_t imm) {              \
    instruction(dst, lhs, Operand(imm));                         \
  }
#define I32_SHIFTOP(name, instruction)                                         \
  void LiftoffAssembler::emit_##name(Register dst, Register src,               \
                                     Register amount, LiftoffRegList pinned) { \
    UseScratchRegisterScope temps(this);                                       \
    Register scratch = temps.Acquire();                                        \
    and_(scratch, amount, Operand(0x1f));                                      \
    instruction(dst, src, Operand(scratch));                                   \
  }
#define FP32_UNOP(name, instruction)                                           \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    instruction(liftoff::GetFloatRegister(dst),                                \
                liftoff::GetFloatRegister(src));                               \
  }
#define FP32_BINOP(name, instruction)                                        \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    instruction(liftoff::GetFloatRegister(dst),                              \
                liftoff::GetFloatRegister(lhs),                              \
                liftoff::GetFloatRegister(rhs));                             \
  }
#define FP64_UNOP(name, instruction)                                           \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    instruction(dst, src);                                                     \
  }
#define FP64_BINOP(name, instruction)                                        \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    instruction(dst, lhs, rhs);                                              \
  }

I32_BINOP_I(i32_add, add)
I32_BINOP(i32_sub, sub)
I32_BINOP(i32_mul, mul)
I32_BINOP_I(i32_and, and_)
I32_BINOP_I(i32_or, orr)
I32_BINOP_I(i32_xor, eor)
I32_SHIFTOP(i32_shl, lsl)
I32_SHIFTOP(i32_sar, asr)
I32_SHIFTOP(i32_shr, lsr)
FP32_BINOP(f32_add, vadd)
FP32_BINOP(f32_sub, vsub)
FP32_BINOP(f32_mul, vmul)
FP32_BINOP(f32_div, vdiv)
FP32_UNOP(f32_abs, vabs)
FP32_UNOP(f32_neg, vneg)
FP32_UNOP(f32_sqrt, vsqrt)
FP64_BINOP(f64_add, vadd)
FP64_BINOP(f64_sub, vsub)
FP64_BINOP(f64_mul, vmul)
FP64_BINOP(f64_div, vdiv)
FP64_UNOP(f64_abs, vabs)
FP64_UNOP(f64_neg, vneg)
FP64_UNOP(f64_sqrt, vsqrt)

#undef I32_BINOP
#undef I32_SHIFTOP
#undef FP32_UNOP
#undef FP32_BINOP
#undef FP64_UNOP
#undef FP64_BINOP

bool LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  clz(dst, src);
  return true;
}

bool LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  rbit(dst, src);
  clz(dst, dst);
  return true;
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  {
    UseScratchRegisterScope temps(this);
    LiftoffRegList pinned = LiftoffRegList::ForRegs(dst);
    Register scratch = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
    Register scratch_2 = GetUnusedRegister(kGpReg, pinned).gp();
    // x = x - ((x & (0x55555555 << 1)) >> 1)
    and_(scratch, src, Operand(0xaaaaaaaa));
    sub(dst, src, Operand(scratch, LSR, 1));
    // x = (x & 0x33333333) + ((x & (0x33333333 << 2)) >> 2)
    mov(scratch, Operand(0x33333333));
    and_(scratch_2, dst, Operand(scratch, LSL, 2));
    and_(scratch, dst, scratch);
    add(dst, scratch, Operand(scratch_2, LSR, 2));
  }
  // x = (x + (x >> 4)) & 0x0F0F0F0F
  add(dst, dst, Operand(dst, LSR, 4));
  and_(dst, dst, Operand(0x0f0f0f0f));
  // x = x + (x >> 8)
  add(dst, dst, Operand(dst, LSR, 8));
  // x = x + (x >> 16)
  add(dst, dst, Operand(dst, LSR, 16));
  // x = x & 0x3F
  and_(dst, dst, Operand(0x3f));
  return true;
}

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  if (!CpuFeatures::IsSupported(SUDIV)) {
    bailout(kMissingCPUFeature, "i32_divs");
    return;
  }
  CpuFeatureScope scope(this, SUDIV);
  // Issue division early so we can perform the trapping checks whilst it
  // completes.
  bool speculative_sdiv = dst != lhs && dst != rhs;
  if (speculative_sdiv) {
    sdiv(dst, lhs, rhs);
  }
  Label noTrap;
  // Check for division by zero.
  cmp(rhs, Operand(0));
  b(trap_div_by_zero, eq);
  // Check for kMinInt / -1. This is unrepresentable.
  cmp(rhs, Operand(-1));
  b(&noTrap, ne);
  cmp(lhs, Operand(kMinInt));
  b(trap_div_unrepresentable, eq);
  bind(&noTrap);
  if (!speculative_sdiv) {
    sdiv(dst, lhs, rhs);
  }
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  if (!CpuFeatures::IsSupported(SUDIV)) {
    bailout(kMissingCPUFeature, "i32_divu");
    return;
  }
  CpuFeatureScope scope(this, SUDIV);
  // Check for division by zero.
  cmp(rhs, Operand(0));
  b(trap_div_by_zero, eq);
  udiv(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  if (!CpuFeatures::IsSupported(SUDIV)) {
    // When this case is handled, a check for ARMv7 is required to use mls.
    // Mls support is implied with SUDIV support.
    bailout(kMissingCPUFeature, "i32_rems");
    return;
  }
  CpuFeatureScope scope(this, SUDIV);
  // No need to check kMinInt / -1 because the result is kMinInt and then
  // kMinInt * -1 -> kMinInt. In this case, the Msub result is therefore 0.
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  sdiv(scratch, lhs, rhs);
  // Check for division by zero.
  cmp(rhs, Operand(0));
  b(trap_div_by_zero, eq);
  // Compute remainder.
  mls(dst, scratch, rhs, lhs);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  if (!CpuFeatures::IsSupported(SUDIV)) {
    // When this case is handled, a check for ARMv7 is required to use mls.
    // Mls support is implied with SUDIV support.
    bailout(kMissingCPUFeature, "i32_remu");
    return;
  }
  CpuFeatureScope scope(this, SUDIV);
  // No need to check kMinInt / -1 because the result is kMinInt and then
  // kMinInt * -1 -> kMinInt. In this case, the Msub result is therefore 0.
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  udiv(scratch, lhs, rhs);
  // Check for division by zero.
  cmp(rhs, Operand(0));
  b(trap_div_by_zero, eq);
  // Compute remainder.
  mls(dst, scratch, rhs, lhs);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register src, int amount) {
  DCHECK(is_uint5(amount));
  lsr(dst, src, Operand(amount));
}

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::I64Binop<&Assembler::add, &Assembler::adc>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    int32_t imm) {
  liftoff::I64BinopI<&Assembler::add, &Assembler::adc>(this, dst, lhs, imm);
}

void LiftoffAssembler::emit_i64_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::I64Binop<&Assembler::sub, &Assembler::sbc>(this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i64_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  // Idea:
  //        [           lhs_hi  |           lhs_lo  ] * [  rhs_hi  |  rhs_lo  ]
  //    =   [  lhs_hi * rhs_lo  |                   ]  (32 bit mul, shift 32)
  //      + [  lhs_lo * rhs_hi  |                   ]  (32 bit mul, shift 32)
  //      + [             lhs_lo * rhs_lo           ]  (32x32->64 mul, shift 0)
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  // scratch = lhs_hi * rhs_lo
  mul(scratch, lhs.high_gp(), rhs.low_gp());
  // scratch += lhs_lo * rhs_hi
  mla(scratch, lhs.low_gp(), rhs.high_gp(), scratch);
  // TODO(arm): use umlal once implemented correctly in the simulator.
  // [dst_hi|dst_lo] = lhs_lo * rhs_lo
  umull(dst.low_gp(), dst.high_gp(), lhs.low_gp(), rhs.low_gp());
  // dst_hi += scratch
  add(dst.high_gp(), dst.high_gp(), scratch);
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

void LiftoffAssembler::emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::I64Shiftop<&TurboAssembler::LslPair, true>(this, dst, src, amount,
                                                      pinned);
}

void LiftoffAssembler::emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::I64Shiftop<&TurboAssembler::AsrPair, false>(this, dst, src, amount,
                                                       pinned);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::I64Shiftop<&TurboAssembler::LsrPair, false>(this, dst, src, amount,
                                                       pinned);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    int amount) {
  DCHECK(is_uint6(amount));
  UseScratchRegisterScope temps(this);
  Register src_high = src.high_gp();
  // {src.high_gp()} will still be needed after writing {dst.low_gp()}.
  if (src_high == dst.low_gp()) {
    src_high = GetUnusedRegister(kGpReg).gp();
    TurboAssembler::Move(src_high, dst.low_gp());
  }

  LsrPair(dst.low_gp(), dst.high_gp(), src.low_gp(), src_high, amount);
}

bool LiftoffAssembler::emit_f32_ceil(DoubleRegister dst, DoubleRegister src) {
  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(this, ARMv8);
    vrintp(liftoff::GetFloatRegister(dst), liftoff::GetFloatRegister(src));
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f32_floor(DoubleRegister dst, DoubleRegister src) {
  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(this, ARMv8);
    vrintm(liftoff::GetFloatRegister(dst), liftoff::GetFloatRegister(src));
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f32_trunc(DoubleRegister dst, DoubleRegister src) {
  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(this, ARMv8);
    vrintz(liftoff::GetFloatRegister(dst), liftoff::GetFloatRegister(src));
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f32_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(this, ARMv8);
    vrintn(liftoff::GetFloatRegister(dst), liftoff::GetFloatRegister(src));
    return true;
  }
  return false;
}

void LiftoffAssembler::emit_f32_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax(
      this, liftoff::GetFloatRegister(dst), liftoff::GetFloatRegister(lhs),
      liftoff::GetFloatRegister(rhs), liftoff::MinOrMax::kMin);
}

void LiftoffAssembler::emit_f32_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax(
      this, liftoff::GetFloatRegister(dst), liftoff::GetFloatRegister(lhs),
      liftoff::GetFloatRegister(rhs), liftoff::MinOrMax::kMax);
}

bool LiftoffAssembler::emit_f64_ceil(DoubleRegister dst, DoubleRegister src) {
  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(this, ARMv8);
    vrintp(dst, src);
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f64_floor(DoubleRegister dst, DoubleRegister src) {
  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(this, ARMv8);
    vrintm(dst, src);
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f64_trunc(DoubleRegister dst, DoubleRegister src) {
  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(this, ARMv8);
    vrintz(dst, src);
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f64_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(this, ARMv8);
    vrintn(dst, src);
    return true;
  }
  return false;
}

void LiftoffAssembler::emit_f64_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax(this, dst, lhs, rhs, liftoff::MinOrMax::kMin);
}

void LiftoffAssembler::emit_f64_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax(this, dst, lhs, rhs, liftoff::MinOrMax::kMax);
}

void LiftoffAssembler::emit_i32_to_intptr(Register dst, Register src) {
  // This is a nop on arm.
}

void LiftoffAssembler::emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  constexpr uint32_t kF32SignBit = uint32_t{1} << 31;
  UseScratchRegisterScope temps(this);
  Register scratch = GetUnusedRegister(kGpReg).gp();
  Register scratch2 = temps.Acquire();
  VmovLow(scratch, lhs);
  // Clear sign bit in {scratch}.
  bic(scratch, scratch, Operand(kF32SignBit));
  VmovLow(scratch2, rhs);
  // Isolate sign bit in {scratch2}.
  and_(scratch2, scratch2, Operand(kF32SignBit));
  // Combine {scratch2} into {scratch}.
  orr(scratch, scratch, scratch2);
  VmovLow(dst, scratch);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  constexpr uint32_t kF64SignBitHighWord = uint32_t{1} << 31;
  // On arm, we cannot hold the whole f64 value in a gp register, so we just
  // operate on the upper half (UH).
  UseScratchRegisterScope temps(this);
  Register scratch = GetUnusedRegister(kGpReg).gp();
  Register scratch2 = temps.Acquire();
  VmovHigh(scratch, lhs);
  // Clear sign bit in {scratch}.
  bic(scratch, scratch, Operand(kF64SignBitHighWord));
  VmovHigh(scratch2, rhs);
  // Isolate sign bit in {scratch2}.
  and_(scratch2, scratch2, Operand(kF64SignBitHighWord));
  // Combine {scratch2} into {scratch}.
  orr(scratch, scratch, scratch2);
  vmov(dst, lhs);
  VmovHigh(dst, scratch);
}

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      TurboAssembler::Move(dst.gp(), src.low_gp());
      return true;
    case kExprI32SConvertF32: {
      UseScratchRegisterScope temps(this);
      SwVfpRegister scratch_f = temps.AcquireS();
      vcvt_s32_f32(
          scratch_f,
          liftoff::GetFloatRegister(src.fp()));  // f32 -> i32 round to zero.
      vmov(dst.gp(), scratch_f);
      // Check underflow and NaN.
      vmov(scratch_f, Float32(static_cast<float>(INT32_MIN)));
      VFPCompareAndSetFlags(liftoff::GetFloatRegister(src.fp()), scratch_f);
      b(trap, lt);
      // Check overflow.
      cmp(dst.gp(), Operand(-1));
      b(trap, vs);
      return true;
    }
    case kExprI32UConvertF32: {
      UseScratchRegisterScope temps(this);
      SwVfpRegister scratch_f = temps.AcquireS();
      vcvt_u32_f32(
          scratch_f,
          liftoff::GetFloatRegister(src.fp()));  // f32 -> i32 round to zero.
      vmov(dst.gp(), scratch_f);
      // Check underflow and NaN.
      vmov(scratch_f, Float32(-1.0f));
      VFPCompareAndSetFlags(liftoff::GetFloatRegister(src.fp()), scratch_f);
      b(trap, le);
      // Check overflow.
      cmp(dst.gp(), Operand(-1));
      b(trap, eq);
      return true;
    }
    case kExprI32SConvertF64: {
      UseScratchRegisterScope temps(this);
      SwVfpRegister scratch_f = temps.AcquireS();
      vcvt_s32_f64(scratch_f, src.fp());  // f64 -> i32 round to zero.
      vmov(dst.gp(), scratch_f);
      // Check underflow and NaN.
      DwVfpRegister scratch_d = temps.AcquireD();
      vmov(scratch_d, Double(static_cast<double>(INT32_MIN - 1.0)));
      VFPCompareAndSetFlags(src.fp(), scratch_d);
      b(trap, le);
      // Check overflow.
      vmov(scratch_d, Double(static_cast<double>(INT32_MAX + 1.0)));
      VFPCompareAndSetFlags(src.fp(), scratch_d);
      b(trap, ge);
      return true;
    }
    case kExprI32UConvertF64: {
      UseScratchRegisterScope temps(this);
      SwVfpRegister scratch_f = temps.AcquireS();
      vcvt_u32_f64(scratch_f, src.fp());  // f64 -> i32 round to zero.
      vmov(dst.gp(), scratch_f);
      // Check underflow and NaN.
      DwVfpRegister scratch_d = temps.AcquireD();
      vmov(scratch_d, Double(static_cast<double>(-1.0)));
      VFPCompareAndSetFlags(src.fp(), scratch_d);
      b(trap, le);
      // Check overflow.
      vmov(scratch_d, Double(static_cast<double>(UINT32_MAX + 1.0)));
      VFPCompareAndSetFlags(src.fp(), scratch_d);
      b(trap, ge);
      return true;
    }
    case kExprI32ReinterpretF32:
      vmov(dst.gp(), liftoff::GetFloatRegister(src.fp()));
      return true;
    case kExprI64SConvertI32:
      if (dst.low_gp() != src.gp()) mov(dst.low_gp(), src.gp());
      mov(dst.high_gp(), Operand(src.gp(), ASR, 31));
      return true;
    case kExprI64UConvertI32:
      if (dst.low_gp() != src.gp()) mov(dst.low_gp(), src.gp());
      mov(dst.high_gp(), Operand(0));
      return true;
    case kExprI64ReinterpretF64:
      vmov(dst.low_gp(), dst.high_gp(), src.fp());
      return true;
    case kExprF32SConvertI32: {
      SwVfpRegister dst_float = liftoff::GetFloatRegister(dst.fp());
      vmov(dst_float, src.gp());
      vcvt_f32_s32(dst_float, dst_float);
      return true;
    }
    case kExprF32UConvertI32: {
      SwVfpRegister dst_float = liftoff::GetFloatRegister(dst.fp());
      vmov(dst_float, src.gp());
      vcvt_f32_u32(dst_float, dst_float);
      return true;
    }
    case kExprF32ConvertF64:
      vcvt_f32_f64(liftoff::GetFloatRegister(dst.fp()), src.fp());
      return true;
    case kExprF32ReinterpretI32:
      vmov(liftoff::GetFloatRegister(dst.fp()), src.gp());
      return true;
    case kExprF64SConvertI32: {
      vmov(liftoff::GetFloatRegister(dst.fp()), src.gp());
      vcvt_f64_s32(dst.fp(), liftoff::GetFloatRegister(dst.fp()));
      return true;
    }
    case kExprF64UConvertI32: {
      vmov(liftoff::GetFloatRegister(dst.fp()), src.gp());
      vcvt_f64_u32(dst.fp(), liftoff::GetFloatRegister(dst.fp()));
      return true;
    }
    case kExprF64ConvertF32:
      vcvt_f64_f32(dst.fp(), liftoff::GetFloatRegister(src.fp()));
      return true;
    case kExprF64ReinterpretI64:
      vmov(dst.fp(), src.low_gp(), src.high_gp());
      return true;
    case kExprF64SConvertI64:
    case kExprF64UConvertI64:
    case kExprI64SConvertF32:
    case kExprI64UConvertF32:
    case kExprF32SConvertI64:
    case kExprF32UConvertI64:
    case kExprI64SConvertF64:
    case kExprI64UConvertF64:
      // These cases can be handled by the C fallback function.
      return false;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  sxtb(dst, src);
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  sxth(dst, src);
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  emit_i32_signextend_i8(dst.low_gp(), src.low_gp());
  mov(dst.high_gp(), Operand(dst.low_gp(), ASR, 31));
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  emit_i32_signextend_i16(dst.low_gp(), src.low_gp());
  mov(dst.high_gp(), Operand(dst.low_gp(), ASR, 31));
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  TurboAssembler::Move(dst.low_gp(), src.low_gp());
  mov(dst.high_gp(), Operand(src.low_gp(), ASR, 31));
}

void LiftoffAssembler::emit_jump(Label* label) { b(label); }

void LiftoffAssembler::emit_jump(Register target) { bx(target); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  DCHECK_EQ(type, kWasmI32);
  if (rhs == no_reg) {
    cmp(lhs, Operand(0));
  } else {
    cmp(lhs, rhs);
  }
  b(label, cond);
}

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  clz(dst, src);
  mov(dst, Operand(dst, LSR, kRegSizeInBitsLog2));
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  cmp(lhs, rhs);
  mov(dst, Operand(0), LeaveCC);
  mov(dst, Operand(1), LeaveCC, cond);
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  orr(dst, src.low_gp(), src.high_gp());
  clz(dst, dst);
  mov(dst, Operand(dst, LSR, 5));
}

void LiftoffAssembler::emit_i64_set_cond(Condition cond, Register dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  // For signed i64 comparisons, we still need to use unsigned comparison for
  // the low word (the only bit carrying signedness information is the MSB in
  // the high word).
  Condition unsigned_cond = liftoff::MakeUnsigned(cond);
  Label set_cond;
  Label cont;
  LiftoffRegister dest = LiftoffRegister(dst);
  bool speculative_move = !dest.overlaps(lhs) && !dest.overlaps(rhs);
  if (speculative_move) {
    mov(dst, Operand(0));
  }
  // Compare high word first. If it differs, use it for the set_cond. If it's
  // equal, compare the low word and use that for set_cond.
  cmp(lhs.high_gp(), rhs.high_gp());
  if (unsigned_cond == cond) {
    cmp(lhs.low_gp(), rhs.low_gp(), kEqual);
    if (!speculative_move) {
      mov(dst, Operand(0));
    }
    mov(dst, Operand(1), LeaveCC, cond);
  } else {
    // If the condition predicate for the low differs from that for the high
    // word, the conditional move instructions must be separated.
    b(ne, &set_cond);
    cmp(lhs.low_gp(), rhs.low_gp());
    if (!speculative_move) {
      mov(dst, Operand(0));
    }
    mov(dst, Operand(1), LeaveCC, unsigned_cond);
    b(&cont);
    bind(&set_cond);
    if (!speculative_move) {
      mov(dst, Operand(0));
    }
    mov(dst, Operand(1), LeaveCC, cond);
    bind(&cont);
  }
}

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  VFPCompareAndSetFlags(liftoff::GetFloatRegister(lhs),
                        liftoff::GetFloatRegister(rhs));
  mov(dst, Operand(0), LeaveCC);
  mov(dst, Operand(1), LeaveCC, cond);
  if (cond != ne) {
    // If V flag set, at least one of the arguments was a Nan -> false.
    mov(dst, Operand(0), LeaveCC, vs);
  }
}

void LiftoffAssembler::emit_f64_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  VFPCompareAndSetFlags(lhs, rhs);
  mov(dst, Operand(0), LeaveCC);
  mov(dst, Operand(1), LeaveCC, cond);
  if (cond != ne) {
    // If V flag set, at least one of the arguments was a Nan -> false.
    mov(dst, Operand(0), LeaveCC, vs);
  }
}

void LiftoffAssembler::StackCheck(Label* ool_code, Register limit_address) {
  ldr(limit_address, MemOperand(limit_address));
  cmp(sp, limit_address);
  b(ool_code, ls);
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0, 0);
  CallCFunction(ExternalReference::wasm_call_trap_callback_for_testing(), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  // Asserts unreachable within the wasm code.
  TurboAssembler::AssertUnreachable(reason);
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  RegList core_regs = regs.GetGpList();
  if (core_regs != 0) {
    stm(db_w, sp, core_regs);
  }
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  while (!fp_regs.is_empty()) {
    LiftoffRegister reg = fp_regs.GetFirstRegSet();
    DoubleRegister first = reg.fp();
    DoubleRegister last = first;
    fp_regs.clear(reg);
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetFirstRegSet();
      int code = reg.fp().code();
      // vstm can not push more than 16 registers. We have to make sure the
      // condition is met.
      if ((code != last.code() + 1) || ((code - first.code() + 1) > 16)) break;
      last = reg.fp();
      fp_regs.clear(reg);
    }
    vstm(db_w, sp, first, last);
  }
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  while (!fp_regs.is_empty()) {
    LiftoffRegister reg = fp_regs.GetLastRegSet();
    DoubleRegister last = reg.fp();
    DoubleRegister first = last;
    fp_regs.clear(reg);
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetLastRegSet();
      int code = reg.fp().code();
      if ((code != first.code() - 1) || ((last.code() - code + 1) > 16)) break;
      first = reg.fp();
      fp_regs.clear(reg);
    }
    vldm(ia_w, sp, first, last);
  }
  RegList core_regs = regs.GetGpList();
  if (core_regs != 0) {
    ldm(ia_w, sp, core_regs);
  }
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  Drop(num_stack_slots);
  Ret();
}

void LiftoffAssembler::CallC(wasm::FunctionSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueType out_argument_type, int stack_bytes,
                             ExternalReference ext_ref) {
  // Arguments are passed by pushing them all to the stack and then passing
  // a pointer to them.
  DCHECK(IsAligned(stack_bytes, kSystemPointerSize));
  // Reserve space in the stack.
  AllocateStackSpace(stack_bytes);

  int arg_bytes = 0;
  for (ValueType param_type : sig->parameters()) {
    switch (param_type) {
      case kWasmI32:
        str(args->gp(), MemOperand(sp, arg_bytes));
        break;
      case kWasmI64:
        str(args->low_gp(), MemOperand(sp, arg_bytes));
        str(args->high_gp(), MemOperand(sp, arg_bytes + kSystemPointerSize));
        break;
      case kWasmF32:
        vstr(liftoff::GetFloatRegister(args->fp()), MemOperand(sp, arg_bytes));
        break;
      case kWasmF64:
        vstr(args->fp(), MemOperand(sp, arg_bytes));
        break;
      default:
        UNREACHABLE();
    }
    args++;
    arg_bytes += ValueTypes::MemSize(param_type);
  }
  DCHECK_LE(arg_bytes, stack_bytes);

  // Pass a pointer to the buffer with the arguments to the C function.
  mov(r0, sp);

  // Now call the C function.
  constexpr int kNumCCallArgs = 1;
  PrepareCallCFunction(kNumCCallArgs);
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* result_reg = rets;
  if (sig->return_count() > 0) {
    DCHECK_EQ(1, sig->return_count());
    constexpr Register kReturnReg = r0;
    if (kReturnReg != rets->gp()) {
      Move(*rets, LiftoffRegister(kReturnReg), sig->GetReturn(0));
    }
    result_reg++;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_type != kWasmStmt) {
    switch (out_argument_type) {
      case kWasmI32:
        ldr(result_reg->gp(), MemOperand(sp));
        break;
      case kWasmI64:
        ldr(result_reg->low_gp(), MemOperand(sp));
        ldr(result_reg->high_gp(), MemOperand(sp, kSystemPointerSize));
        break;
      case kWasmF32:
        vldr(liftoff::GetFloatRegister(result_reg->fp()), MemOperand(sp));
        break;
      case kWasmF64:
        vldr(result_reg->fp(), MemOperand(sp));
        break;
      default:
        UNREACHABLE();
    }
  }
  add(sp, sp, Operand(stack_bytes));
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  Call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallIndirect(wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  DCHECK(target != no_reg);
  Call(target);
}

void LiftoffAssembler::CallRuntimeStub(WasmCode::RuntimeStubId sid) {
  // A direct call to a wasm runtime stub defined in this module.
  // Just encode the stub index. This will be patched at relocation.
  Call(static_cast<Address>(sid), RelocInfo::WASM_STUB_CALL);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  AllocateStackSpace(size);
  mov(addr, sp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  add(sp, sp, Operand(size));
}

void LiftoffStackSlots::Construct() {
  for (auto& slot : slots_) {
    const LiftoffAssembler::VarState& src = slot.src_;
    switch (src.loc()) {
      case LiftoffAssembler::VarState::kStack: {
        switch (src.type()) {
          // i32 and i64 can be treated as similar cases, i64 being previously
          // split into two i32 registers
          case kWasmI32:
          case kWasmI64:
          case kWasmF32: {
            UseScratchRegisterScope temps(asm_);
            Register scratch = temps.Acquire();
            asm_->ldr(scratch,
                      liftoff::GetHalfStackSlot(slot.src_index_, slot.half_));
            asm_->Push(scratch);
          } break;
          case kWasmF64: {
            UseScratchRegisterScope temps(asm_);
            DwVfpRegister scratch = temps.AcquireD();
            asm_->vldr(scratch, liftoff::GetStackSlot(slot.src_index_));
            asm_->vpush(scratch);
          } break;
          default:
            UNREACHABLE();
        }
        break;
      }
      case LiftoffAssembler::VarState::kRegister:
        switch (src.type()) {
          case kWasmI64: {
            LiftoffRegister reg =
                slot.half_ == kLowWord ? src.reg().low() : src.reg().high();
            asm_->push(reg.gp());
          } break;
          case kWasmI32:
            asm_->push(src.reg().gp());
            break;
          case kWasmF32:
            asm_->vpush(liftoff::GetFloatRegister(src.reg().fp()));
            break;
          case kWasmF64:
            asm_->vpush(src.reg().fp());
            break;
          default:
            UNREACHABLE();
        }
        break;
      case LiftoffAssembler::VarState::kIntConst: {
        DCHECK(src.type() == kWasmI32 || src.type() == kWasmI64);
        UseScratchRegisterScope temps(asm_);
        Register scratch = temps.Acquire();
        // The high word is the sign extension of the low word.
        asm_->mov(scratch,
                  Operand(slot.half_ == kLowWord ? src.i32_const()
                                                 : src.i32_const() >> 31));
        asm_->push(scratch);
        break;
      }
    }
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_ARM_LIFTOFF_ASSEMBLER_ARM_H_

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_MIPS_LIFTOFF_ASSEMBLER_MIPS_H_
#define V8_WASM_BASELINE_MIPS_LIFTOFF_ASSEMBLER_MIPS_H_

#include "src/base/platform/wrappers.h"
#include "src/heap/memory-chunk.h"
#include "src/wasm/baseline/liftoff-assembler.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace liftoff {

inline constexpr Condition ToCondition(LiftoffCondition liftoff_cond) {
  switch (liftoff_cond) {
    case kEqual:
      return eq;
    case kUnequal:
      return ne;
    case kSignedLessThan:
      return lt;
    case kSignedLessEqual:
      return le;
    case kSignedGreaterThan:
      return gt;
    case kSignedGreaterEqual:
      return ge;
    case kUnsignedLessThan:
      return ult;
    case kUnsignedLessEqual:
      return ule;
    case kUnsignedGreaterThan:
      return ugt;
    case kUnsignedGreaterEqual:
      return uge;
  }
}

//  half
//  slot        Frame
//  -----+--------------------+---------------------------
//  n+3  |   parameter n      |
//  ...  |       ...          |
//   4   |   parameter 1      | or parameter 2
//   3   |   parameter 0      | or parameter 1
//   2   |  (result address)  | or parameter 0
//  -----+--------------------+---------------------------
//   1   | return addr (ra)   |
//   0   | previous frame (fp)|
//  -----+--------------------+  <-- frame ptr (fp)
//  -1   | StackFrame::WASM   |
//  -2   |    instance        |
//  -3   |    feedback vector |
//  -4   |    tiering budget  |
//  -----+--------------------+---------------------------
//  -5   |    slot 0 (high)   |   ^
//  -6   |    slot 0 (low)    |   |
//  -7   |    slot 1 (high)   | Frame slots
//  -8   |    slot 1 (low)    |   |
//       |                    |   v
//  -----+--------------------+  <-- stack ptr (sp)
//
#if defined(V8_TARGET_BIG_ENDIAN)
constexpr int32_t kLowWordOffset = 4;
constexpr int32_t kHighWordOffset = 0;
#else
constexpr int32_t kLowWordOffset = 0;
constexpr int32_t kHighWordOffset = 4;
#endif

constexpr int kInstanceOffset = 2 * kSystemPointerSize;
constexpr int kFeedbackVectorOffset = 3 * kSystemPointerSize;
constexpr int kTierupBudgetOffset = 4 * kSystemPointerSize;

inline MemOperand GetStackSlot(int offset) { return MemOperand(fp, -offset); }

inline MemOperand GetHalfStackSlot(int offset, RegPairHalf half) {
  int32_t half_offset =
      half == kLowWord ? 0 : LiftoffAssembler::kStackSlotSize / 2;
  return MemOperand(offset > 0 ? fp : sp, -offset + half_offset);
}

inline MemOperand GetInstanceOperand() { return GetStackSlot(kInstanceOffset); }

inline void Load(LiftoffAssembler* assm, LiftoffRegister dst, Register base,
                 int32_t offset, ValueKind kind) {
  MemOperand src(base, offset);
  switch (kind) {
    case kI32:
    case kRef:
    case kOptRef:
    case kRtt:
      assm->lw(dst.gp(), src);
      break;
    case kI64:
      assm->lw(dst.low_gp(),
               MemOperand(base, offset + liftoff::kLowWordOffset));
      assm->lw(dst.high_gp(),
               MemOperand(base, offset + liftoff::kHighWordOffset));
      break;
    case kF32:
      assm->lwc1(dst.fp(), src);
      break;
    case kF64:
      assm->Ldc1(dst.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

inline void Store(LiftoffAssembler* assm, Register base, int32_t offset,
                  LiftoffRegister src, ValueKind kind) {
  MemOperand dst(base, offset);
  switch (kind) {
    case kI32:
    case kOptRef:
    case kRef:
    case kRtt:
      assm->Usw(src.gp(), dst);
      break;
    case kI64:
      assm->Usw(src.low_gp(),
                MemOperand(base, offset + liftoff::kLowWordOffset));
      assm->Usw(src.high_gp(),
                MemOperand(base, offset + liftoff::kHighWordOffset));
      break;
    case kF32:
      assm->Uswc1(src.fp(), dst, t8);
      break;
    case kF64:
      assm->Usdc1(src.fp(), dst, t8);
      break;
    default:
      UNREACHABLE();
  }
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueKind kind) {
  switch (kind) {
    case kI32:
    case kOptRef:
    case kRef:
    case kRtt:
      assm->push(reg.gp());
      break;
    case kI64:
      assm->Push(reg.high_gp(), reg.low_gp());
      break;
    case kF32:
      assm->addiu(sp, sp, -sizeof(float));
      assm->swc1(reg.fp(), MemOperand(sp, 0));
      break;
    case kF64:
      assm->addiu(sp, sp, -sizeof(double));
      assm->Sdc1(reg.fp(), MemOperand(sp, 0));
      break;
    default:
      UNREACHABLE();
  }
}

inline Register EnsureNoAlias(Assembler* assm, Register reg,
                              LiftoffRegister must_not_alias,
                              UseScratchRegisterScope* temps) {
  if (reg != must_not_alias.low_gp() && reg != must_not_alias.high_gp())
    return reg;
  Register tmp = temps->Acquire();
  DCHECK_NE(must_not_alias.low_gp(), tmp);
  DCHECK_NE(must_not_alias.high_gp(), tmp);
  assm->movz(tmp, reg, zero_reg);
  return tmp;
}

#if defined(V8_TARGET_BIG_ENDIAN)
inline void ChangeEndiannessLoad(LiftoffAssembler* assm, LiftoffRegister dst,
                                 LoadType type, LiftoffRegList pinned) {
  bool is_float = false;
  LiftoffRegister tmp = dst;
  switch (type.value()) {
    case LoadType::kI64Load8U:
    case LoadType::kI64Load8S:
    case LoadType::kI32Load8U:
    case LoadType::kI32Load8S:
      // No need to change endianness for byte size.
      return;
    case LoadType::kF32Load:
      is_float = true;
      tmp = assm->GetUnusedRegister(kGpReg, pinned);
      assm->emit_type_conversion(kExprI32ReinterpretF32, tmp, dst);
      V8_FALLTHROUGH;
    case LoadType::kI32Load:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 4);
      break;
    case LoadType::kI32Load16S:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 2);
      break;
    case LoadType::kI32Load16U:
      assm->TurboAssembler::ByteSwapUnsigned(tmp.gp(), tmp.gp(), 2);
      break;
    case LoadType::kF64Load:
      is_float = true;
      tmp = assm->GetUnusedRegister(kGpRegPair, pinned);
      assm->emit_type_conversion(kExprI64ReinterpretF64, tmp, dst);
      V8_FALLTHROUGH;
    case LoadType::kI64Load:
      assm->TurboAssembler::Move(kScratchReg, tmp.low_gp());
      assm->TurboAssembler::ByteSwapSigned(tmp.low_gp(), tmp.high_gp(), 4);
      assm->TurboAssembler::ByteSwapSigned(tmp.high_gp(), kScratchReg, 4);
      break;
    case LoadType::kI64Load16U:
      assm->TurboAssembler::ByteSwapUnsigned(tmp.low_gp(), tmp.low_gp(), 2);
      assm->TurboAssembler::Move(tmp.high_gp(), zero_reg);
      break;
    case LoadType::kI64Load16S:
      assm->TurboAssembler::ByteSwapSigned(tmp.low_gp(), tmp.low_gp(), 2);
      assm->sra(tmp.high_gp(), tmp.low_gp(), 31);
      break;
    case LoadType::kI64Load32U:
      assm->TurboAssembler::ByteSwapSigned(tmp.low_gp(), tmp.low_gp(), 4);
      assm->TurboAssembler::Move(tmp.high_gp(), zero_reg);
      break;
    case LoadType::kI64Load32S:
      assm->TurboAssembler::ByteSwapSigned(tmp.low_gp(), tmp.low_gp(), 4);
      assm->sra(tmp.high_gp(), tmp.low_gp(), 31);
      break;
    default:
      UNREACHABLE();
  }

  if (is_float) {
    switch (type.value()) {
      case LoadType::kF32Load:
        assm->emit_type_conversion(kExprF32ReinterpretI32, dst, tmp);
        break;
      case LoadType::kF64Load:
        assm->emit_type_conversion(kExprF64ReinterpretI64, dst, tmp);
        break;
      default:
        UNREACHABLE();
    }
  }
}

inline void ChangeEndiannessStore(LiftoffAssembler* assm, LiftoffRegister src,
                                  StoreType type, LiftoffRegList pinned) {
  bool is_float = false;
  LiftoffRegister tmp = src;
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      // No need to change endianness for byte size.
      return;
    case StoreType::kF32Store:
      is_float = true;
      tmp = assm->GetUnusedRegister(kGpReg, pinned);
      assm->emit_type_conversion(kExprI32ReinterpretF32, tmp, src);
      V8_FALLTHROUGH;
    case StoreType::kI32Store:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 4);
      break;
    case StoreType::kI32Store16:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 2);
      break;
    case StoreType::kF64Store:
      is_float = true;
      tmp = assm->GetUnusedRegister(kGpRegPair, pinned);
      assm->emit_type_conversion(kExprI64ReinterpretF64, tmp, src);
      V8_FALLTHROUGH;
    case StoreType::kI64Store:
      assm->TurboAssembler::Move(kScratchReg, tmp.low_gp());
      assm->TurboAssembler::ByteSwapSigned(tmp.low_gp(), tmp.high_gp(), 4);
      assm->TurboAssembler::ByteSwapSigned(tmp.high_gp(), kScratchReg, 4);
      break;
    case StoreType::kI64Store32:
      assm->TurboAssembler::ByteSwapSigned(tmp.low_gp(), tmp.low_gp(), 4);
      break;
    case StoreType::kI64Store16:
      assm->TurboAssembler::ByteSwapSigned(tmp.low_gp(), tmp.low_gp(), 2);
      break;
    default:
      UNREACHABLE();
  }

  if (is_float) {
    switch (type.value()) {
      case StoreType::kF32Store:
        assm->emit_type_conversion(kExprF32ReinterpretI32, src, tmp);
        break;
      case StoreType::kF64Store:
        assm->emit_type_conversion(kExprF64ReinterpretI64, src, tmp);
        break;
      default:
        UNREACHABLE();
    }
  }
}
#endif  // V8_TARGET_BIG_ENDIAN

}  // namespace liftoff

int LiftoffAssembler::PrepareStackFrame() {
  int offset = pc_offset();
  // When the frame size is bigger than 4KB, we need seven instructions for
  // stack checking, so we reserve space for this case.
  addiu(sp, sp, 0);
  nop();
  nop();
  nop();
  nop();
  nop();
  nop();
  return offset;
}

void LiftoffAssembler::PrepareTailCall(int num_callee_stack_params,
                                       int stack_param_delta) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();

  // Push the return address and frame pointer to complete the stack frame.
  Lw(scratch, MemOperand(fp, 4));
  Push(scratch);
  Lw(scratch, MemOperand(fp, 0));
  Push(scratch);

  // Shift the whole frame upwards.
  int slot_count = num_callee_stack_params + 2;
  for (int i = slot_count - 1; i >= 0; --i) {
    Lw(scratch, MemOperand(sp, i * 4));
    Sw(scratch, MemOperand(fp, (i - stack_param_delta) * 4));
  }

  // Set the new stack and frame pointer.
  addiu(sp, fp, -stack_param_delta * 4);
  Pop(ra, fp);
}

void LiftoffAssembler::AlignFrameSize() {}

void LiftoffAssembler::PatchPrepareStackFrame(
    int offset, SafepointTableBuilder* safepoint_table_builder) {
  // The frame_size includes the frame marker and the instance slot. Both are
  // pushed as part of frame construction, so we don't need to allocate memory
  // for them anymore.
  int frame_size = GetTotalFrameSize() - 2 * kSystemPointerSize;

  // We can't run out of space, just pass anything big enough to not cause the
  // assembler to try to grow the buffer.
  constexpr int kAvailableSpace = 256;
  TurboAssembler patching_assembler(
      nullptr, AssemblerOptions{}, CodeObjectRequired::kNo,
      ExternalAssemblerBuffer(buffer_start_ + offset, kAvailableSpace));

  if (V8_LIKELY(frame_size < 4 * KB)) {
    // This is the standard case for small frames: just subtract from SP and be
    // done with it.
    patching_assembler.Addu(sp, sp, Operand(-frame_size));
    return;
  }

  // The frame size is bigger than 4KB, so we might overflow the available stack
  // space if we first allocate the frame and then do the stack check (we will
  // need some remaining stack space for throwing the exception). That's why we
  // check the available stack space before we allocate the frame. To do this we
  // replace the {__ Addu(sp, sp, -framesize)} with a jump to OOL code that does
  // this "extended stack check".
  //
  // The OOL code can simply be generated here with the normal assembler,
  // because all other code generation, including OOL code, has already finished
  // when {PatchPrepareStackFrame} is called. The function prologue then jumps
  // to the current {pc_offset()} to execute the OOL code for allocating the
  // large frame.
  // Emit the unconditional branch in the function prologue (from {offset} to
  // {pc_offset()}).

  int imm32 = pc_offset() - offset - 3 * kInstrSize;
  patching_assembler.BranchLong(imm32);

  // If the frame is bigger than the stack, we throw the stack overflow
  // exception unconditionally. Thereby we can avoid the integer overflow
  // check in the condition code.
  RecordComment("OOL: stack check for large frame");
  Label continuation;
  if (frame_size < FLAG_stack_size * 1024) {
    Register stack_limit = kScratchReg;
    Lw(stack_limit,
       FieldMemOperand(kWasmInstanceRegister,
                       WasmInstanceObject::kRealStackLimitAddressOffset));
    Lw(stack_limit, MemOperand(stack_limit));
    Addu(stack_limit, stack_limit, Operand(frame_size));
    Branch(&continuation, uge, sp, Operand(stack_limit));
  }

  Call(wasm::WasmCode::kWasmStackOverflow, RelocInfo::WASM_STUB_CALL);
  // The call will not return; just define an empty safepoint.
  safepoint_table_builder->DefineSafepoint(this);
  if (FLAG_debug_code) stop();

  bind(&continuation);

  // Now allocate the stack space. Note that this might do more than just
  // decrementing the SP;
  Addu(sp, sp, Operand(-frame_size));

  // Jump back to the start of the function, from {pc_offset()} to
  // right after the reserved space for the {__ Addu(sp, sp, -framesize)} (which
  // is a jump now).
  int func_start_offset = offset + 7 * kInstrSize;
  imm32 = func_start_offset - pc_offset() - 3 * kInstrSize;
  BranchLong(imm32);
}

void LiftoffAssembler::FinishCode() {}

void LiftoffAssembler::AbortCompilation() {}

// static
constexpr int LiftoffAssembler::StaticStackFrameSize() {
  return liftoff::kTierupBudgetOffset;
}

int LiftoffAssembler::SlotSizeForType(ValueKind kind) {
  switch (kind) {
    case kS128:
      return value_kind_size(kind);
    default:
      return kStackSlotSize;
  }
}

bool LiftoffAssembler::NeedsAlignment(ValueKind kind) {
  return kind == kS128 || is_reference(kind);
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  switch (value.type().kind()) {
    case kI32:
      TurboAssembler::li(reg.gp(), Operand(value.to_i32(), rmode));
      break;
    case kI64: {
      DCHECK(RelocInfo::IsNoInfo(rmode));
      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      TurboAssembler::li(reg.low_gp(), Operand(low_word));
      TurboAssembler::li(reg.high_gp(), Operand(high_word));
      break;
    }
    case kF32:
      TurboAssembler::Move(reg.fp(), value.to_f32_boxed().get_bits());
      break;
    case kF64:
      TurboAssembler::Move(reg.fp(), value.to_f64_boxed().get_bits());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadInstanceFromFrame(Register dst) {
  lw(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::LoadFromInstance(Register dst, Register instance,
                                        int32_t offset, int size) {
  DCHECK_LE(0, offset);
  switch (size) {
    case 1:
      lb(dst, MemOperand(instance, offset));
      break;
    case 4:
      lw(dst, MemOperand(instance, offset));
      break;
    default:
      UNIMPLEMENTED();
  }
}

void LiftoffAssembler::LoadTaggedPointerFromInstance(Register dst,
                                                     Register instance,
                                                     int32_t offset) {
  STATIC_ASSERT(kTaggedSize == kSystemPointerSize);
  lw(dst, MemOperand(instance, offset));
}

void LiftoffAssembler::SpillInstance(Register instance) {
  sw(instance, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::ResetOSRTarget() {}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         int32_t offset_imm,
                                         LiftoffRegList pinned) {
  STATIC_ASSERT(kTaggedSize == kInt32Size);
  Load(LiftoffRegister(dst), src_addr, offset_reg,
       static_cast<uint32_t>(offset_imm), LoadType::kI32Load, pinned);
}

void LiftoffAssembler::LoadFullPointer(Register dst, Register src_addr,
                                       int32_t offset_imm) {
  MemOperand src_op = MemOperand(src_addr, offset_imm);
  lw(dst, src_op);
}

void LiftoffAssembler::StoreTaggedPointer(Register dst_addr,
                                          Register offset_reg,
                                          int32_t offset_imm,
                                          LiftoffRegister src,
                                          LiftoffRegList pinned,
                                          SkipWriteBarrier skip_write_barrier) {
  STATIC_ASSERT(kTaggedSize == kInt32Size);
  Register dst = no_reg;
  if (offset_reg != no_reg) {
    dst = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
    emit_ptrsize_add(dst, dst_addr, offset_reg);
  }
  MemOperand dst_op = (offset_reg != no_reg) ? MemOperand(dst, offset_imm)
                                             : MemOperand(dst_addr, offset_imm);
  Sw(src.gp(), dst_op);

  if (skip_write_barrier || FLAG_disable_write_barriers) return;

  // The write barrier.
  Label write_barrier;
  Label exit;
  Register scratch = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  CheckPageFlag(dst_addr, scratch,
                MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                &write_barrier);
  Branch(&exit);
  bind(&write_barrier);
  JumpIfSmi(src.gp(), &exit);
  CheckPageFlag(src.gp(), scratch,
                MemoryChunk::kPointersToHereAreInterestingMask, eq, &exit);
  Addu(scratch, dst_op.rm(), dst_op.offset());
  CallRecordWriteStubSaveRegisters(
      dst_addr, scratch, RememberedSetAction::kEmit, SaveFPRegsMode::kSave,
      StubCallMode::kCallWasmRuntimeStub);
  bind(&exit);
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc, bool is_load_mem,
                            bool i64_offset) {
  Register src = no_reg;
  if (offset_reg != no_reg) {
    src = GetUnusedRegister(kGpReg, pinned).gp();
    emit_ptrsize_add(src, src_addr, offset_reg);
  }
  MemOperand src_op = (offset_reg != no_reg) ? MemOperand(src, offset_imm)
                                             : MemOperand(src_addr, offset_imm);

  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
      lbu(dst.gp(), src_op);
      break;
    case LoadType::kI64Load8U:
      lbu(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI32Load8S:
      lb(dst.gp(), src_op);
      break;
    case LoadType::kI64Load8S:
      lb(dst.low_gp(), src_op);
      TurboAssembler::Move(dst.high_gp(), dst.low_gp());
      sra(dst.high_gp(), dst.high_gp(), 31);
      break;
    case LoadType::kI32Load16U:
      TurboAssembler::Ulhu(dst.gp(), src_op);
      break;
    case LoadType::kI64Load16U:
      TurboAssembler::Ulhu(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI32Load16S:
      TurboAssembler::Ulh(dst.gp(), src_op);
      break;
    case LoadType::kI64Load16S:
      TurboAssembler::Ulh(dst.low_gp(), src_op);
      TurboAssembler::Move(dst.high_gp(), dst.low_gp());
      sra(dst.high_gp(), dst.high_gp(), 31);
      break;
    case LoadType::kI32Load:
      TurboAssembler::Ulw(dst.gp(), src_op);
      break;
    case LoadType::kI64Load32U:
      TurboAssembler::Ulw(dst.low_gp(), src_op);
      xor_(dst.high_gp(), dst.high_gp(), dst.high_gp());
      break;
    case LoadType::kI64Load32S:
      TurboAssembler::Ulw(dst.low_gp(), src_op);
      TurboAssembler::Move(dst.high_gp(), dst.low_gp());
      sra(dst.high_gp(), dst.high_gp(), 31);
      break;
    case LoadType::kI64Load: {
      MemOperand src_op =
          (offset_reg != no_reg)
              ? MemOperand(src, offset_imm + liftoff::kLowWordOffset)
              : MemOperand(src_addr, offset_imm + liftoff::kLowWordOffset);
      MemOperand src_op_upper =
          (offset_reg != no_reg)
              ? MemOperand(src, offset_imm + liftoff::kHighWordOffset)
              : MemOperand(src_addr, offset_imm + liftoff::kHighWordOffset);
      {
        UseScratchRegisterScope temps(this);
        Register temp = dst.low_gp();
        if (dst.low_gp() == src_op_upper.rm()) temp = temps.Acquire();
        TurboAssembler::Ulw(temp, src_op);
        TurboAssembler::Ulw(dst.high_gp(), src_op_upper);
        if (dst.low_gp() == src_op_upper.rm()) mov(dst.low_gp(), temp);
      }
      break;
    }
    case LoadType::kF32Load:
      TurboAssembler::Ulwc1(dst.fp(), src_op, t8);
      break;
    case LoadType::kF64Load:
      TurboAssembler::Uldc1(dst.fp(), src_op, t8);
      break;
    default:
      UNREACHABLE();
  }

#if defined(V8_TARGET_BIG_ENDIAN)
  if (is_load_mem) {
    pinned.set(src_op.rm());
    liftoff::ChangeEndiannessLoad(this, dst, type, pinned);
  }
#endif
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  Register dst = no_reg;
  MemOperand dst_op = MemOperand(dst_addr, offset_imm);
  if (offset_reg != no_reg) {
    if (is_store_mem) {
      pinned.set(src);
    }
    dst = GetUnusedRegister(kGpReg, pinned).gp();
    emit_ptrsize_add(dst, dst_addr, offset_reg);
    dst_op = MemOperand(dst, offset_imm);
  }

#if defined(V8_TARGET_BIG_ENDIAN)
  if (is_store_mem) {
    pinned = pinned | LiftoffRegList{dst_op.rm(), src};
    LiftoffRegister tmp = GetUnusedRegister(src.reg_class(), pinned);
    // Save original value.
    Move(tmp, src, type.value_type());

    src = tmp;
    pinned.set(tmp);
    liftoff::ChangeEndiannessStore(this, src, type, pinned);
  }
#endif

  if (protected_store_pc) *protected_store_pc = pc_offset();
  switch (type.value()) {
    case StoreType::kI64Store8:
      src = src.low();
      V8_FALLTHROUGH;
    case StoreType::kI32Store8:
      sb(src.gp(), dst_op);
      break;
    case StoreType::kI64Store16:
      src = src.low();
      V8_FALLTHROUGH;
    case StoreType::kI32Store16:
      TurboAssembler::Ush(src.gp(), dst_op, t8);
      break;
    case StoreType::kI64Store32:
      src = src.low();
      V8_FALLTHROUGH;
    case StoreType::kI32Store:
      TurboAssembler::Usw(src.gp(), dst_op);
      break;
    case StoreType::kI64Store: {
      MemOperand dst_op_lower(dst_op.rm(),
                              offset_imm + liftoff::kLowWordOffset);
      MemOperand dst_op_upper(dst_op.rm(),
                              offset_imm + liftoff::kHighWordOffset);
      TurboAssembler::Usw(src.low_gp(), dst_op_lower);
      TurboAssembler::Usw(src.high_gp(), dst_op_upper);
      break;
    }
    case StoreType::kF32Store:
      TurboAssembler::Uswc1(src.fp(), dst_op, t8);
      break;
    case StoreType::kF64Store:
      TurboAssembler::Usdc1(src.fp(), dst_op, t8);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicLoad(LiftoffRegister dst, Register src_addr,
                                  Register offset_reg, uint32_t offset_imm,
                                  LoadType type, LiftoffRegList pinned) {
  bailout(kAtomics, "AtomicLoad");
}

void LiftoffAssembler::AtomicStore(Register dst_addr, Register offset_reg,
                                   uint32_t offset_imm, LiftoffRegister src,
                                   StoreType type, LiftoffRegList pinned) {
  bailout(kAtomics, "AtomicStore");
}

void LiftoffAssembler::AtomicAdd(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  bailout(kAtomics, "AtomicAdd");
}

void LiftoffAssembler::AtomicSub(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  bailout(kAtomics, "AtomicSub");
}

void LiftoffAssembler::AtomicAnd(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  bailout(kAtomics, "AtomicAnd");
}

void LiftoffAssembler::AtomicOr(Register dst_addr, Register offset_reg,
                                uint32_t offset_imm, LiftoffRegister value,
                                LiftoffRegister result, StoreType type) {
  bailout(kAtomics, "AtomicOr");
}

void LiftoffAssembler::AtomicXor(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  bailout(kAtomics, "AtomicXor");
}

void LiftoffAssembler::AtomicExchange(Register dst_addr, Register offset_reg,
                                      uint32_t offset_imm,
                                      LiftoffRegister value,
                                      LiftoffRegister result, StoreType type) {
  bailout(kAtomics, "AtomicExchange");
}

void LiftoffAssembler::AtomicCompareExchange(
    Register dst_addr, Register offset_reg, uint32_t offset_imm,
    LiftoffRegister expected, LiftoffRegister new_value, LiftoffRegister result,
    StoreType type) {
  bailout(kAtomics, "AtomicCompareExchange");
}

void LiftoffAssembler::AtomicFence() { sync(); }

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueKind kind) {
  int32_t offset = kSystemPointerSize * (caller_slot_idx + 1);
  liftoff::Load(this, dst, fp, offset, kind);
}

void LiftoffAssembler::StoreCallerFrameSlot(LiftoffRegister src,
                                            uint32_t caller_slot_idx,
                                            ValueKind kind) {
  int32_t offset = kSystemPointerSize * (caller_slot_idx + 1);
  liftoff::Store(this, fp, offset, src, kind);
}

void LiftoffAssembler::LoadReturnStackSlot(LiftoffRegister dst, int offset,
                                           ValueKind kind) {
  liftoff::Load(this, dst, sp, offset, kind);
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_offset, uint32_t src_offset,
                                      ValueKind kind) {
  DCHECK_NE(dst_offset, src_offset);
  LiftoffRegister reg = GetUnusedRegister(reg_class_for(kind), {});
  Fill(reg, src_offset, kind);
  Spill(dst_offset, reg, kind);
}

void LiftoffAssembler::Move(Register dst, Register src, ValueKind kind) {
  DCHECK_NE(dst, src);
  TurboAssembler::mov(dst, src);
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueKind kind) {
  DCHECK_NE(dst, src);
  TurboAssembler::Move(dst, src);
}

void LiftoffAssembler::Spill(int offset, LiftoffRegister reg, ValueKind kind) {
  RecordUsedSpillOffset(offset);
  MemOperand dst = liftoff::GetStackSlot(offset);
  switch (kind) {
    case kI32:
    case kRef:
    case kOptRef:
    case kRtt:
      sw(reg.gp(), dst);
      break;
    case kI64:
      sw(reg.low_gp(), liftoff::GetHalfStackSlot(offset, kLowWord));
      sw(reg.high_gp(), liftoff::GetHalfStackSlot(offset, kHighWord));
      break;
    case kF32:
      swc1(reg.fp(), dst);
      break;
    case kF64:
      TurboAssembler::Sdc1(reg.fp(), dst);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(int offset, WasmValue value) {
  RecordUsedSpillOffset(offset);
  MemOperand dst = liftoff::GetStackSlot(offset);
  switch (value.type().kind()) {
    case kI32:
    case kRef:
    case kOptRef: {
      LiftoffRegister tmp = GetUnusedRegister(kGpReg, {});
      TurboAssembler::li(tmp.gp(), Operand(value.to_i32()));
      sw(tmp.gp(), dst);
      break;
    }
    case kI64: {
      LiftoffRegister tmp = GetUnusedRegister(kGpRegPair, {});

      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      TurboAssembler::li(tmp.low_gp(), Operand(low_word));
      TurboAssembler::li(tmp.high_gp(), Operand(high_word));

      sw(tmp.low_gp(), liftoff::GetHalfStackSlot(offset, kLowWord));
      sw(tmp.high_gp(), liftoff::GetHalfStackSlot(offset, kHighWord));
      break;
    }
    default:
      // kWasmF32 and kWasmF64 are unreachable, since those
      // constants are not tracked.
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, int offset, ValueKind kind) {
  MemOperand src = liftoff::GetStackSlot(offset);
  switch (kind) {
    case kI32:
    case kRef:
    case kOptRef:
      lw(reg.gp(), src);
      break;
    case kI64:
      lw(reg.low_gp(), liftoff::GetHalfStackSlot(offset, kLowWord));
      lw(reg.high_gp(), liftoff::GetHalfStackSlot(offset, kHighWord));
      break;
    case kF32:
      lwc1(reg.fp(), src);
      break;
    case kF64:
      TurboAssembler::Ldc1(reg.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register reg, int offset, RegPairHalf half) {
  lw(reg, liftoff::GetHalfStackSlot(offset, half));
}

void LiftoffAssembler::FillStackSlotsWithZero(int start, int size) {
  DCHECK_LT(0, size);
  DCHECK_EQ(0, size % 4);
  RecordUsedSpillOffset(start + size);

  if (size <= 48) {
    // Special straight-line code for up to 12 words. Generates one
    // instruction per word (<=12 instructions total).
    for (int offset = 4; offset <= size; offset += 4) {
      Sw(zero_reg, liftoff::GetStackSlot(start + offset));
    }
  } else {
    // General case for bigger counts (12 instructions).
    // Use a0 for start address (inclusive), a1 for end address (exclusive).
    Push(a1, a0);
    Addu(a0, fp, Operand(-start - size));
    Addu(a1, fp, Operand(-start));

    Label loop;
    bind(&loop);
    Sw(zero_reg, MemOperand(a0));
    addiu(a0, a0, kSystemPointerSize);
    BranchShort(&loop, ne, a0, Operand(a1));

    Pop(a1, a0);
  }
}

void LiftoffAssembler::emit_i32_mul(Register dst, Register lhs, Register rhs) {
  TurboAssembler::Mul(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));

  // Check if lhs == kMinInt and rhs == -1, since this case is unrepresentable.
  TurboAssembler::li(kScratchReg, 1);
  TurboAssembler::li(kScratchReg2, 1);
  TurboAssembler::LoadZeroOnCondition(kScratchReg, lhs, Operand(kMinInt), eq);
  TurboAssembler::LoadZeroOnCondition(kScratchReg2, rhs, Operand(-1), eq);
  addu(kScratchReg, kScratchReg, kScratchReg2);
  TurboAssembler::Branch(trap_div_unrepresentable, eq, kScratchReg,
                         Operand(zero_reg));

  TurboAssembler::Div(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Divu(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Mod(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Modu(dst, lhs, rhs);
}

#define I32_BINOP(name, instruction)                                 \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register lhs, \
                                         Register rhs) {             \
    instruction(dst, lhs, rhs);                                      \
  }

// clang-format off
I32_BINOP(add, addu)
I32_BINOP(sub, subu)
I32_BINOP(and, and_)
I32_BINOP(or, or_)
I32_BINOP(xor, xor_)
// clang-format on

#undef I32_BINOP

#define I32_BINOP_I(name, instruction)                                  \
  void LiftoffAssembler::emit_i32_##name##i(Register dst, Register lhs, \
                                            int32_t imm) {              \
    instruction(dst, lhs, Operand(imm));                                \
  }

// clang-format off
I32_BINOP_I(add, Addu)
I32_BINOP_I(sub, Subu)
I32_BINOP_I(and, And)
I32_BINOP_I(or, Or)
I32_BINOP_I(xor, Xor)
// clang-format on

#undef I32_BINOP_I

void LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  TurboAssembler::Clz(dst, src);
}

void LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  TurboAssembler::Ctz(dst, src);
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  TurboAssembler::Popcnt(dst, src);
  return true;
}

#define I32_SHIFTOP(name, instruction)                               \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register src, \
                                         Register amount) {          \
    instruction(dst, src, amount);                                   \
  }
#define I32_SHIFTOP_I(name, instruction)                                \
  I32_SHIFTOP(name, instruction##v)                                     \
  void LiftoffAssembler::emit_i32_##name##i(Register dst, Register src, \
                                            int amount) {               \
    DCHECK(is_uint5(amount));                                           \
    instruction(dst, src, amount);                                      \
  }

I32_SHIFTOP_I(shl, sll)
I32_SHIFTOP_I(sar, sra)
I32_SHIFTOP_I(shr, srl)

#undef I32_SHIFTOP
#undef I32_SHIFTOP_I

void LiftoffAssembler::emit_i64_addi(LiftoffRegister dst, LiftoffRegister lhs,
                                     int64_t imm) {
  LiftoffRegister imm_reg =
      GetUnusedRegister(kGpRegPair, LiftoffRegList{dst, lhs});
  int32_t imm_low_word = static_cast<int32_t>(imm);
  int32_t imm_high_word = static_cast<int32_t>(imm >> 32);
  TurboAssembler::li(imm_reg.low_gp(), imm_low_word);
  TurboAssembler::li(imm_reg.high_gp(), imm_high_word);
  TurboAssembler::AddPair(dst.low_gp(), dst.high_gp(), lhs.low_gp(),
                          lhs.high_gp(), imm_reg.low_gp(), imm_reg.high_gp(),
                          kScratchReg, kScratchReg2);
}

void LiftoffAssembler::emit_i64_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  TurboAssembler::MulPair(dst.low_gp(), dst.high_gp(), lhs.low_gp(),
                          lhs.high_gp(), rhs.low_gp(), rhs.high_gp(),
                          kScratchReg, kScratchReg2);
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

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  TurboAssembler::AddPair(dst.low_gp(), dst.high_gp(), lhs.low_gp(),
                          lhs.high_gp(), rhs.low_gp(), rhs.high_gp(),
                          kScratchReg, kScratchReg2);
}

void LiftoffAssembler::emit_i64_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  TurboAssembler::SubPair(dst.low_gp(), dst.high_gp(), lhs.low_gp(),
                          lhs.high_gp(), rhs.low_gp(), rhs.high_gp(),
                          kScratchReg, kScratchReg2);
}

namespace liftoff {

inline bool IsRegInRegPair(LiftoffRegister pair, Register reg) {
  DCHECK(pair.is_gp_pair());
  return pair.low_gp() == reg || pair.high_gp() == reg;
}

inline void Emit64BitShiftOperation(
    LiftoffAssembler* assm, LiftoffRegister dst, LiftoffRegister src,
    Register amount,
    void (TurboAssembler::*emit_shift)(Register, Register, Register, Register,
                                       Register, Register, Register)) {
  Label move, done;
  LiftoffRegList pinned = {dst, src, amount};

  // If some of destination registers are in use, get another, unused pair.
  // That way we prevent overwriting some input registers while shifting.
  // Do this before any branch so that the cache state will be correct for
  // all conditions.
  LiftoffRegister tmp = assm->GetUnusedRegister(kGpRegPair, pinned);

  // If shift amount is 0, don't do the shifting.
  assm->TurboAssembler::Branch(&move, eq, amount, Operand(zero_reg));

  if (liftoff::IsRegInRegPair(dst, amount) || dst.overlaps(src)) {
    // Do the actual shift.
    (assm->*emit_shift)(tmp.low_gp(), tmp.high_gp(), src.low_gp(),
                        src.high_gp(), amount, kScratchReg, kScratchReg2);

    // Place result in destination register.
    assm->TurboAssembler::Move(dst.high_gp(), tmp.high_gp());
    assm->TurboAssembler::Move(dst.low_gp(), tmp.low_gp());
  } else {
    (assm->*emit_shift)(dst.low_gp(), dst.high_gp(), src.low_gp(),
                        src.high_gp(), amount, kScratchReg, kScratchReg2);
  }
  assm->TurboAssembler::Branch(&done);

  // If shift amount is 0, move src to dst.
  assm->bind(&move);
  assm->TurboAssembler::Move(dst.high_gp(), src.high_gp());
  assm->TurboAssembler::Move(dst.low_gp(), src.low_gp());

  assm->bind(&done);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::ShlPair);
}

void LiftoffAssembler::emit_i64_shli(LiftoffRegister dst, LiftoffRegister src,
                                     int32_t amount) {
  UseScratchRegisterScope temps(this);
  // {src.low_gp()} will still be needed after writing {dst.high_gp()} and
  // {dst.low_gp()}.
  Register src_low = liftoff::EnsureNoAlias(this, src.low_gp(), dst, &temps);
  Register src_high = src.high_gp();
  // {src.high_gp()} will still be needed after writing {dst.high_gp()}.
  if (src_high == dst.high_gp()) {
    mov(kScratchReg, src_high);
    src_high = kScratchReg;
  }
  DCHECK_NE(dst.low_gp(), kScratchReg);
  DCHECK_NE(dst.high_gp(), kScratchReg);

  ShlPair(dst.low_gp(), dst.high_gp(), src_low, src_high, amount, kScratchReg);
}

void LiftoffAssembler::emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::SarPair);
}

void LiftoffAssembler::emit_i64_sari(LiftoffRegister dst, LiftoffRegister src,
                                     int32_t amount) {
  UseScratchRegisterScope temps(this);
  // {src.high_gp()} will still be needed after writing {dst.high_gp()} and
  // {dst.low_gp()}.
  Register src_high = liftoff::EnsureNoAlias(this, src.high_gp(), dst, &temps);
  DCHECK_NE(dst.low_gp(), kScratchReg);
  DCHECK_NE(dst.high_gp(), kScratchReg);

  SarPair(dst.low_gp(), dst.high_gp(), src.low_gp(), src_high, amount,
          kScratchReg);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::ShrPair);
}

void LiftoffAssembler::emit_i64_shri(LiftoffRegister dst, LiftoffRegister src,
                                     int32_t amount) {
  UseScratchRegisterScope temps(this);
  // {src.high_gp()} will still be needed after writing {dst.high_gp()} and
  // {dst.low_gp()}.
  Register src_high = liftoff::EnsureNoAlias(this, src.high_gp(), dst, &temps);
  DCHECK_NE(dst.low_gp(), kScratchReg);
  DCHECK_NE(dst.high_gp(), kScratchReg);

  ShrPair(dst.low_gp(), dst.high_gp(), src.low_gp(), src_high, amount,
          kScratchReg);
}

void LiftoffAssembler::emit_i64_clz(LiftoffRegister dst, LiftoffRegister src) {
  // return high == 0 ? 32 + CLZ32(low) : CLZ32(high);
  Label done;
  Label high_is_zero;
  Branch(&high_is_zero, eq, src.high_gp(), Operand(zero_reg));

  clz(dst.low_gp(), src.high_gp());
  jmp(&done);

  bind(&high_is_zero);
  clz(dst.low_gp(), src.low_gp());
  Addu(dst.low_gp(), dst.low_gp(), Operand(32));

  bind(&done);
  mov(dst.high_gp(), zero_reg);  // High word of result is always 0.
}

void LiftoffAssembler::emit_i64_ctz(LiftoffRegister dst, LiftoffRegister src) {
  // return low == 0 ? 32 + CTZ32(high) : CTZ32(low);
  Label done;
  Label low_is_zero;
  Branch(&low_is_zero, eq, src.low_gp(), Operand(zero_reg));

  Ctz(dst.low_gp(), src.low_gp());
  jmp(&done);

  bind(&low_is_zero);
  Ctz(dst.low_gp(), src.high_gp());
  Addu(dst.low_gp(), dst.low_gp(), Operand(32));

  bind(&done);
  mov(dst.high_gp(), zero_reg);  // High word of result is always 0.
}

bool LiftoffAssembler::emit_i64_popcnt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  // Produce partial popcnts in the two dst registers.
  Register src1 = src.high_gp() == dst.low_gp() ? src.high_gp() : src.low_gp();
  Register src2 = src.high_gp() == dst.low_gp() ? src.low_gp() : src.high_gp();
  TurboAssembler::Popcnt(dst.low_gp(), src1);
  TurboAssembler::Popcnt(dst.high_gp(), src2);
  // Now add the two into the lower dst reg and clear the higher dst reg.
  addu(dst.low_gp(), dst.low_gp(), dst.high_gp());
  mov(dst.high_gp(), zero_reg);
  return true;
}

void LiftoffAssembler::IncrementSmi(LiftoffRegister dst, int offset) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  lw(scratch, MemOperand(dst.gp(), offset));
  Addu(scratch, scratch, Operand(Smi::FromInt(1)));
  sw(scratch, MemOperand(dst.gp(), offset));
}

void LiftoffAssembler::emit_f32_neg(DoubleRegister dst, DoubleRegister src) {
  TurboAssembler::Neg_s(dst, src);
}

void LiftoffAssembler::emit_f64_neg(DoubleRegister dst, DoubleRegister src) {
  TurboAssembler::Neg_d(dst, src);
}

void LiftoffAssembler::emit_f32_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  TurboAssembler::Float32Min(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  TurboAssembler::Float32MinOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f32_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  TurboAssembler::Float32Max(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  TurboAssembler::Float32MaxOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  bailout(kComplexOperation, "f32_copysign");
}

void LiftoffAssembler::emit_f64_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  TurboAssembler::Float64Min(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  TurboAssembler::Float64MinOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f64_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  TurboAssembler::Float64Max(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  TurboAssembler::Float64MaxOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  bailout(kComplexOperation, "f64_copysign");
}

#define FP_BINOP(name, instruction)                                          \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    instruction(dst, lhs, rhs);                                              \
  }
#define FP_UNOP(name, instruction)                                             \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    instruction(dst, src);                                                     \
  }
#define FP_UNOP_RETURN_TRUE(name, instruction)                                 \
  bool LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    instruction(dst, src);                                                     \
    return true;                                                               \
  }

FP_BINOP(f32_add, add_s)
FP_BINOP(f32_sub, sub_s)
FP_BINOP(f32_mul, mul_s)
FP_BINOP(f32_div, div_s)
FP_UNOP(f32_abs, abs_s)
FP_UNOP_RETURN_TRUE(f32_ceil, Ceil_s_s)
FP_UNOP_RETURN_TRUE(f32_floor, Floor_s_s)
FP_UNOP_RETURN_TRUE(f32_trunc, Trunc_s_s)
FP_UNOP_RETURN_TRUE(f32_nearest_int, Round_s_s)
FP_UNOP(f32_sqrt, sqrt_s)
FP_BINOP(f64_add, add_d)
FP_BINOP(f64_sub, sub_d)
FP_BINOP(f64_mul, mul_d)
FP_BINOP(f64_div, div_d)
FP_UNOP(f64_abs, abs_d)
FP_UNOP(f64_sqrt, sqrt_d)

#undef FP_BINOP
#undef FP_UNOP

bool LiftoffAssembler::emit_f64_ceil(DoubleRegister dst, DoubleRegister src) {
  if ((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
      IsFp64Mode()) {
    Ceil_d_d(dst, src);
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f64_floor(DoubleRegister dst, DoubleRegister src) {
  if ((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
      IsFp64Mode()) {
    Floor_d_d(dst, src);
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f64_trunc(DoubleRegister dst, DoubleRegister src) {
  if ((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
      IsFp64Mode()) {
    Trunc_d_d(dst, src);
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f64_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  if ((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
      IsFp64Mode()) {
    Round_d_d(dst, src);
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      TurboAssembler::Move(dst.gp(), src.low_gp());
      return true;
    case kExprI32SConvertF32: {
      LiftoffRegister rounded = GetUnusedRegister(kFpReg, LiftoffRegList{src});
      LiftoffRegister converted_back =
          GetUnusedRegister(kFpReg, LiftoffRegList{src, rounded});

      // Real conversion.
      TurboAssembler::Trunc_s_s(rounded.fp(), src.fp());
      trunc_w_s(kScratchDoubleReg, rounded.fp());
      mfc1(dst.gp(), kScratchDoubleReg);
      // Avoid INT32_MAX as an overflow indicator and use INT32_MIN instead,
      // because INT32_MIN allows easier out-of-bounds detection.
      TurboAssembler::Addu(kScratchReg, dst.gp(), 1);
      TurboAssembler::Slt(kScratchReg2, kScratchReg, dst.gp());
      TurboAssembler::Movn(dst.gp(), kScratchReg, kScratchReg2);

      // Checking if trap.
      mtc1(dst.gp(), kScratchDoubleReg);
      cvt_s_w(converted_back.fp(), kScratchDoubleReg);
      TurboAssembler::CompareF32(EQ, rounded.fp(), converted_back.fp());
      TurboAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI32UConvertF32: {
      LiftoffRegister rounded = GetUnusedRegister(kFpReg, LiftoffRegList{src});
      LiftoffRegister converted_back =
          GetUnusedRegister(kFpReg, LiftoffRegList{src, rounded});

      // Real conversion.
      TurboAssembler::Trunc_s_s(rounded.fp(), src.fp());
      TurboAssembler::Trunc_uw_s(dst.gp(), rounded.fp(), kScratchDoubleReg);
      // Avoid UINT32_MAX as an overflow indicator and use 0 instead,
      // because 0 allows easier out-of-bounds detection.
      TurboAssembler::Addu(kScratchReg, dst.gp(), 1);
      TurboAssembler::Movz(dst.gp(), zero_reg, kScratchReg);

      // Checking if trap.
      TurboAssembler::Cvt_d_uw(converted_back.fp(), dst.gp(),
                               kScratchDoubleReg);
      cvt_s_d(converted_back.fp(), converted_back.fp());
      TurboAssembler::CompareF32(EQ, rounded.fp(), converted_back.fp());
      TurboAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI32SConvertF64: {
      LiftoffRegister scratch = GetUnusedRegister(kGpReg, LiftoffRegList{dst});

      // Try a conversion to a signed integer.
      trunc_w_d(kScratchDoubleReg, src.fp());
      mfc1(dst.gp(), kScratchDoubleReg);
      // Retrieve the FCSR.
      cfc1(scratch.gp(), FCSR);
      // Check for overflow and NaNs.
      And(scratch.gp(), scratch.gp(),
          kFCSROverflowCauseMask | kFCSRUnderflowCauseMask |
              kFCSRInvalidOpCauseMask);
      // If we had exceptions we are trap.
      Branch(trap, ne, scratch.gp(), Operand(zero_reg));
      return true;
    }
    case kExprI32UConvertF64: {
      if ((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
          IsFp64Mode()) {
        LiftoffRegister rounded =
            GetUnusedRegister(kFpReg, LiftoffRegList{src});
        LiftoffRegister converted_back =
            GetUnusedRegister(kFpReg, LiftoffRegList{src, rounded});

        // Real conversion.
        TurboAssembler::Trunc_d_d(rounded.fp(), src.fp());
        TurboAssembler::Trunc_uw_d(dst.gp(), rounded.fp(), kScratchDoubleReg);

        // Checking if trap.
        TurboAssembler::Cvt_d_uw(converted_back.fp(), dst.gp(),
                                 kScratchDoubleReg);
        TurboAssembler::CompareF64(EQ, rounded.fp(), converted_back.fp());
        TurboAssembler::BranchFalseF(trap);
        return true;
      }
      bailout(kUnsupportedArchitecture, "kExprI32UConvertF64");
      return true;
    }
    case kExprI32SConvertSatF32:
      bailout(kNonTrappingFloatToInt, "kExprI32SConvertSatF32");
      return true;
    case kExprI32UConvertSatF32:
      bailout(kNonTrappingFloatToInt, "kExprI32UConvertSatF32");
      return true;
    case kExprI32SConvertSatF64:
      bailout(kNonTrappingFloatToInt, "kExprI32SConvertSatF64");
      return true;
    case kExprI32UConvertSatF64:
      bailout(kNonTrappingFloatToInt, "kExprI32UConvertSatF64");
      return true;
    case kExprI64SConvertSatF32:
      bailout(kNonTrappingFloatToInt, "kExprI64SConvertSatF32");
      return true;
    case kExprI64UConvertSatF32:
      bailout(kNonTrappingFloatToInt, "kExprI64UConvertSatF32");
      return true;
    case kExprI64SConvertSatF64:
      bailout(kNonTrappingFloatToInt, "kExprI64SConvertSatF64");
      return true;
    case kExprI64UConvertSatF64:
      bailout(kNonTrappingFloatToInt, "kExprI64UConvertSatF64");
      return true;
    case kExprI32ReinterpretF32:
      mfc1(dst.gp(), src.fp());
      return true;
    case kExprI64SConvertI32:
      TurboAssembler::Move(dst.low_gp(), src.gp());
      TurboAssembler::Move(dst.high_gp(), src.gp());
      sra(dst.high_gp(), dst.high_gp(), 31);
      return true;
    case kExprI64UConvertI32:
      TurboAssembler::Move(dst.low_gp(), src.gp());
      TurboAssembler::Move(dst.high_gp(), zero_reg);
      return true;
    case kExprI64ReinterpretF64:
      mfc1(dst.low_gp(), src.fp());
      TurboAssembler::Mfhc1(dst.high_gp(), src.fp());
      return true;
    case kExprF32SConvertI32: {
      LiftoffRegister scratch = GetUnusedRegister(kFpReg, LiftoffRegList{dst});
      mtc1(src.gp(), scratch.fp());
      cvt_s_w(dst.fp(), scratch.fp());
      return true;
    }
    case kExprF32UConvertI32: {
      LiftoffRegister scratch = GetUnusedRegister(kFpReg, LiftoffRegList{dst});
      TurboAssembler::Cvt_d_uw(dst.fp(), src.gp(), scratch.fp());
      cvt_s_d(dst.fp(), dst.fp());
      return true;
    }
    case kExprF32ConvertF64:
      cvt_s_d(dst.fp(), src.fp());
      return true;
    case kExprF32ReinterpretI32:
      TurboAssembler::FmoveLow(dst.fp(), src.gp());
      return true;
    case kExprF64SConvertI32: {
      LiftoffRegister scratch = GetUnusedRegister(kFpReg, LiftoffRegList{dst});
      mtc1(src.gp(), scratch.fp());
      cvt_d_w(dst.fp(), scratch.fp());
      return true;
    }
    case kExprF64UConvertI32: {
      LiftoffRegister scratch = GetUnusedRegister(kFpReg, LiftoffRegList{dst});
      TurboAssembler::Cvt_d_uw(dst.fp(), src.gp(), scratch.fp());
      return true;
    }
    case kExprF64ConvertF32:
      cvt_d_s(dst.fp(), src.fp());
      return true;
    case kExprF64ReinterpretI64:
      mtc1(src.low_gp(), dst.fp());
      TurboAssembler::Mthc1(src.high_gp(), dst.fp());
      return true;
    default:
      return false;
  }
}

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  bailout(kComplexOperation, "i32_signextend_i8");
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  bailout(kComplexOperation, "i32_signextend_i16");
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  bailout(kComplexOperation, "i64_signextend_i8");
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  bailout(kComplexOperation, "i64_signextend_i16");
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  bailout(kComplexOperation, "i64_signextend_i32");
}

void LiftoffAssembler::emit_jump(Label* label) {
  TurboAssembler::Branch(label);
}

void LiftoffAssembler::emit_jump(Register target) {
  TurboAssembler::Jump(target);
}

void LiftoffAssembler::emit_cond_jump(LiftoffCondition liftoff_cond,
                                      Label* label, ValueKind kind,
                                      Register lhs, Register rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  if (rhs == no_reg) {
    DCHECK_EQ(kind, kI32);
    TurboAssembler::Branch(label, cond, lhs, Operand(zero_reg));
  } else {
    DCHECK(kind == kI32 || (is_reference(kind) && (liftoff_cond == kEqual ||
                                                   liftoff_cond == kUnequal)));
    TurboAssembler::Branch(label, cond, lhs, Operand(rhs));
  }
}

void LiftoffAssembler::emit_i32_cond_jumpi(LiftoffCondition liftoff_cond,
                                           Label* label, Register lhs,
                                           int32_t imm) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  TurboAssembler::Branch(label, cond, lhs, Operand(imm));
}

void LiftoffAssembler::emit_i32_subi_jump_negative(Register value,
                                                   int subtrahend,
                                                   Label* result_negative) {
  TurboAssembler::Subu(value, value, Operand(subtrahend));
  TurboAssembler::Branch(result_negative, less, value, Operand(zero_reg));
}

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  sltiu(dst, src, 1);
}

void LiftoffAssembler::emit_i32_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, Register lhs,
                                         Register rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  Register tmp = dst;
  if (dst == lhs || dst == rhs) {
    tmp = GetUnusedRegister(kGpReg, LiftoffRegList{lhs, rhs}).gp();
  }
  // Write 1 as result.
  TurboAssembler::li(tmp, 1);

  // If negative condition is true, write 0 as result.
  Condition neg_cond = NegateCondition(cond);
  TurboAssembler::LoadZeroOnCondition(tmp, lhs, Operand(rhs), neg_cond);

  // If tmp != dst, result will be moved.
  TurboAssembler::Move(dst, tmp);
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  Register tmp = GetUnusedRegister(kGpReg, LiftoffRegList{src, dst}).gp();
  sltiu(tmp, src.low_gp(), 1);
  sltiu(dst, src.high_gp(), 1);
  and_(dst, dst, tmp);
}

namespace liftoff {
inline LiftoffCondition cond_make_unsigned(LiftoffCondition cond) {
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

void LiftoffAssembler::emit_i64_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  Label low, cont;

  // For signed i64 comparisons, we still need to use unsigned comparison for
  // the low word (the only bit carrying signedness information is the MSB in
  // the high word).
  Condition unsigned_cond =
      liftoff::ToCondition(liftoff::cond_make_unsigned(liftoff_cond));

  Register tmp = dst;
  if (liftoff::IsRegInRegPair(lhs, dst) || liftoff::IsRegInRegPair(rhs, dst)) {
    tmp = GetUnusedRegister(kGpReg, LiftoffRegList{dst, lhs, rhs}).gp();
  }

  // Write 1 initially in tmp register.
  TurboAssembler::li(tmp, 1);

  // If high words are equal, then compare low words, else compare high.
  Branch(&low, eq, lhs.high_gp(), Operand(rhs.high_gp()));

  TurboAssembler::LoadZeroOnCondition(
      tmp, lhs.high_gp(), Operand(rhs.high_gp()), NegateCondition(cond));
  Branch(&cont);

  bind(&low);
  TurboAssembler::LoadZeroOnCondition(tmp, lhs.low_gp(), Operand(rhs.low_gp()),
                                      NegateCondition(unsigned_cond));

  bind(&cont);
  // Move result to dst register if needed.
  TurboAssembler::Move(dst, tmp);
}

namespace liftoff {

inline FPUCondition ConditionToConditionCmpFPU(LiftoffCondition condition,
                                               bool* predicate) {
  switch (condition) {
    case kEqual:
      *predicate = true;
      return EQ;
    case kUnequal:
      *predicate = false;
      return EQ;
    case kUnsignedLessThan:
      *predicate = true;
      return OLT;
    case kUnsignedGreaterEqual:
      *predicate = false;
      return OLT;
    case kUnsignedLessEqual:
      *predicate = true;
      return OLE;
    case kUnsignedGreaterThan:
      *predicate = false;
      return OLE;
    default:
      *predicate = true;
      break;
  }
  UNREACHABLE();
}

}  // namespace liftoff

void LiftoffAssembler::emit_f32_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  Label not_nan, cont;
  TurboAssembler::CompareIsNanF32(lhs, rhs);
  TurboAssembler::BranchFalseF(&not_nan);
  // If one of the operands is NaN, return 1 for f32.ne, else 0.
  if (cond == ne) {
    TurboAssembler::li(dst, 1);
  } else {
    TurboAssembler::Move(dst, zero_reg);
  }
  TurboAssembler::Branch(&cont);

  bind(&not_nan);

  TurboAssembler::li(dst, 1);
  bool predicate;
  FPUCondition fcond =
      liftoff::ConditionToConditionCmpFPU(liftoff_cond, &predicate);
  TurboAssembler::CompareF32(fcond, lhs, rhs);
  if (predicate) {
    TurboAssembler::LoadZeroIfNotFPUCondition(dst);
  } else {
    TurboAssembler::LoadZeroIfFPUCondition(dst);
  }

  bind(&cont);
}

void LiftoffAssembler::emit_f64_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  Label not_nan, cont;
  TurboAssembler::CompareIsNanF64(lhs, rhs);
  TurboAssembler::BranchFalseF(&not_nan);
  // If one of the operands is NaN, return 1 for f64.ne, else 0.
  if (cond == ne) {
    TurboAssembler::li(dst, 1);
  } else {
    TurboAssembler::Move(dst, zero_reg);
  }
  TurboAssembler::Branch(&cont);

  bind(&not_nan);

  TurboAssembler::li(dst, 1);
  bool predicate;
  FPUCondition fcond =
      liftoff::ConditionToConditionCmpFPU(liftoff_cond, &predicate);
  TurboAssembler::CompareF64(fcond, lhs, rhs);
  if (predicate) {
    TurboAssembler::LoadZeroIfNotFPUCondition(dst);
  } else {
    TurboAssembler::LoadZeroIfFPUCondition(dst);
  }

  bind(&cont);
}

bool LiftoffAssembler::emit_select(LiftoffRegister dst, Register condition,
                                   LiftoffRegister true_value,
                                   LiftoffRegister false_value,
                                   ValueKind kind) {
  return false;
}

void LiftoffAssembler::emit_smi_check(Register obj, Label* target,
                                      SmiCheckMode mode) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  And(scratch, obj, Operand(kSmiTagMask));
  Condition condition = mode == kJumpOnSmi ? eq : ne;
  Branch(target, condition, scratch, Operand(zero_reg));
}

void LiftoffAssembler::LoadTransform(LiftoffRegister dst, Register src_addr,
                                     Register offset_reg, uintptr_t offset_imm,
                                     LoadType type,
                                     LoadTransformationKind transform,
                                     uint32_t* protected_load_pc) {
  bailout(kSimd, "load extend and load splat unimplemented");
}

void LiftoffAssembler::StoreLane(Register dst, Register offset,
                                 uintptr_t offset_imm, LiftoffRegister src,
                                 StoreType type, uint8_t lane,
                                 uint32_t* protected_store_pc) {
  bailout(kSimd, "storelane");
}

void LiftoffAssembler::LoadLane(LiftoffRegister dst, LiftoffRegister src,
                                Register addr, Register offset_reg,
                                uintptr_t offset_imm, LoadType type,
                                uint8_t laneidx, uint32_t* protected_load_pc) {
  bailout(kSimd, "loadlane");
}

void LiftoffAssembler::emit_i8x16_shuffle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs,
                                          const uint8_t shuffle[16],
                                          bool is_swizzle) {
  bailout(kSimd, "emit_i8x16_shuffle");
}

void LiftoffAssembler::emit_i8x16_swizzle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_swizzle");
}

void LiftoffAssembler::emit_i8x16_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kSimd, "emit_i8x16_splat");
}

void LiftoffAssembler::emit_i16x8_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kSimd, "emit_i16x8_splat");
}

void LiftoffAssembler::emit_i32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_splat");
}

void LiftoffAssembler::emit_i64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kSimd, "emit_i64x2_splat");
}

void LiftoffAssembler::emit_f32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kSimd, "emit_f32x4_splat");
}

void LiftoffAssembler::emit_f64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  bailout(kSimd, "emit_f64x2_splat");
}

#define SIMD_BINOP(name, ilv_instr, dotp_instr)                          \
  void LiftoffAssembler::emit_##name(                                    \
      LiftoffRegister dst, LiftoffRegister src1, LiftoffRegister src2) { \
    MSARegister dst_msa = MSARegister::from_code(dst.liftoff_code());    \
    MSARegister src1_msa = MSARegister::from_code(src1.liftoff_code());  \
    MSARegister src2_msa = MSARegister::from_code(src2.liftoff_code());  \
    xor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);            \
    ilv_instr(kSimd128ScratchReg, kSimd128RegZero, src1_msa);            \
    ilv_instr(kSimd128RegZero, kSimd128RegZero, src2_msa);               \
    dotp_instr(dst_msa, kSimd128ScratchReg, kSimd128RegZero);            \
  }

SIMD_BINOP(i16x8_extmul_low_i8x16_s, ilvr_b, dotp_s_h)
SIMD_BINOP(i16x8_extmul_high_i8x16_s, ilvl_b, dotp_s_h)
SIMD_BINOP(i16x8_extmul_low_i8x16_u, ilvr_b, dotp_u_h)
SIMD_BINOP(i16x8_extmul_high_i8x16_u, ilvl_b, dotp_u_h)

SIMD_BINOP(i32x4_extmul_low_i16x8_s, ilvr_h, dotp_s_w)
SIMD_BINOP(i32x4_extmul_high_i16x8_s, ilvl_h, dotp_s_w)
SIMD_BINOP(i32x4_extmul_low_i16x8_u, ilvr_h, dotp_u_w)
SIMD_BINOP(i32x4_extmul_high_i16x8_u, ilvl_h, dotp_u_w)

SIMD_BINOP(i64x2_extmul_low_i32x4_s, ilvr_w, dotp_s_d)
SIMD_BINOP(i64x2_extmul_high_i32x4_s, ilvl_w, dotp_s_d)
SIMD_BINOP(i64x2_extmul_low_i32x4_u, ilvr_w, dotp_u_d)
SIMD_BINOP(i64x2_extmul_high_i32x4_u, ilvl_w, dotp_u_d)

#undef SIMD_BINOP

void LiftoffAssembler::emit_i16x8_q15mulr_sat_s(LiftoffRegister dst,
                                                LiftoffRegister src1,
                                                LiftoffRegister src2) {
  bailout(kSimd, "i16x8_q15mulr_sat_s");
}

void LiftoffAssembler::emit_i8x16_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_eq");
}

void LiftoffAssembler::emit_i8x16_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_ne");
}

void LiftoffAssembler::emit_i8x16_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_gt_s");
}

void LiftoffAssembler::emit_i8x16_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_gt_u");
}

void LiftoffAssembler::emit_i8x16_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_ge_s");
}

void LiftoffAssembler::emit_i8x16_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_ge_u");
}

void LiftoffAssembler::emit_i16x8_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_eq");
}

void LiftoffAssembler::emit_i16x8_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_ne");
}

void LiftoffAssembler::emit_i16x8_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_gt_s");
}

void LiftoffAssembler::emit_i16x8_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_gt_u");
}

void LiftoffAssembler::emit_i16x8_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_ge_s");
}

void LiftoffAssembler::emit_i16x8_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_ge_u");
}

void LiftoffAssembler::emit_i32x4_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_eq");
}

void LiftoffAssembler::emit_i32x4_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_ne");
}

void LiftoffAssembler::emit_i32x4_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_gt_s");
}

void LiftoffAssembler::emit_i32x4_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_gt_u");
}

void LiftoffAssembler::emit_i32x4_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_ge_s");
}

void LiftoffAssembler::emit_i32x4_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_ge_u");
}

void LiftoffAssembler::emit_f32x4_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_eq");
}

void LiftoffAssembler::emit_f32x4_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_ne");
}

void LiftoffAssembler::emit_f32x4_lt(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_lt");
}

void LiftoffAssembler::emit_f32x4_le(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_le");
}

void LiftoffAssembler::emit_i64x2_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_i64x2_eq");
}

void LiftoffAssembler::emit_i64x2_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_i64x2_ne");
}

void LiftoffAssembler::emit_i64x2_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i64x2_abs");
}

void LiftoffAssembler::emit_f64x2_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_eq");
}

void LiftoffAssembler::emit_f64x2_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_ne");
}

void LiftoffAssembler::emit_f64x2_lt(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_lt");
}

void LiftoffAssembler::emit_f64x2_le(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_le");
}

void LiftoffAssembler::emit_s128_const(LiftoffRegister dst,
                                       const uint8_t imms[16]) {
  bailout(kSimd, "emit_s128_const");
}

void LiftoffAssembler::emit_s128_not(LiftoffRegister dst, LiftoffRegister src) {
  bailout(kSimd, "emit_s128_not");
}

void LiftoffAssembler::emit_s128_and(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_s128_and");
}

void LiftoffAssembler::emit_s128_or(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  bailout(kSimd, "emit_s128_or");
}

void LiftoffAssembler::emit_s128_xor(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_s128_xor");
}

void LiftoffAssembler::emit_s128_and_not(LiftoffRegister dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  bailout(kSimd, "emit_s128_and_not");
}

void LiftoffAssembler::emit_s128_select(LiftoffRegister dst,
                                        LiftoffRegister src1,
                                        LiftoffRegister src2,
                                        LiftoffRegister mask) {
  bailout(kSimd, "emit_s128_select");
}

void LiftoffAssembler::emit_i8x16_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i8x16_neg");
}

void LiftoffAssembler::emit_v128_anytrue(LiftoffRegister dst,
                                         LiftoffRegister src) {
  bailout(kSimd, "emit_v128_anytrue");
}

void LiftoffAssembler::emit_i8x16_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i8x16_alltrue");
}

void LiftoffAssembler::emit_i8x16_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i8x16_bitmask");
}

void LiftoffAssembler::emit_i8x16_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_shl");
}

void LiftoffAssembler::emit_i8x16_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  bailout(kSimd, "emit_i8x16_shli");
}

void LiftoffAssembler::emit_i8x16_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_shr_s");
}

void LiftoffAssembler::emit_i8x16_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "emit_i8x16_shri_s");
}

void LiftoffAssembler::emit_i8x16_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_shr_u");
}

void LiftoffAssembler::emit_i8x16_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "emit_i8x16_shri_u");
}

void LiftoffAssembler::emit_i8x16_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_add");
}

void LiftoffAssembler::emit_i8x16_add_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_add_sat_s");
}

void LiftoffAssembler::emit_i8x16_add_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_add_sat_u");
}

void LiftoffAssembler::emit_i8x16_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_sub");
}

void LiftoffAssembler::emit_i8x16_sub_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_sub_sat_s");
}

void LiftoffAssembler::emit_i8x16_sub_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_sub_sat_u");
}

void LiftoffAssembler::emit_i8x16_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_min_s");
}

void LiftoffAssembler::emit_i8x16_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_min_u");
}

void LiftoffAssembler::emit_i8x16_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_max_s");
}

void LiftoffAssembler::emit_i8x16_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_max_u");
}

void LiftoffAssembler::emit_i8x16_popcnt(LiftoffRegister dst,
                                         LiftoffRegister src) {
  bailout(kSimd, "emit_i8x16_popcnt");
}

void LiftoffAssembler::emit_i16x8_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i16x8_neg");
}

void LiftoffAssembler::emit_i16x8_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i16x8_alltrue");
}

void LiftoffAssembler::emit_i16x8_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i16x8_bitmask");
}

void LiftoffAssembler::emit_i16x8_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_shl");
}

void LiftoffAssembler::emit_i16x8_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  bailout(kSimd, "emit_i16x8_shli");
}

void LiftoffAssembler::emit_i16x8_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_shr_s");
}

void LiftoffAssembler::emit_i16x8_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "emit_i16x8_shri_s");
}

void LiftoffAssembler::emit_i16x8_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_shr_u");
}

void LiftoffAssembler::emit_i16x8_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "emit_i16x8_shri_u");
}

void LiftoffAssembler::emit_i16x8_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_add");
}

void LiftoffAssembler::emit_i16x8_add_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_add_sat_s");
}

void LiftoffAssembler::emit_i16x8_add_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_add_sat_u");
}

void LiftoffAssembler::emit_i16x8_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_sub");
}

void LiftoffAssembler::emit_i16x8_sub_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_sub_sat_s");
}

void LiftoffAssembler::emit_i16x8_sub_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_sub_sat_u");
}

void LiftoffAssembler::emit_i16x8_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_mul");
}

void LiftoffAssembler::emit_i16x8_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_min_s");
}

void LiftoffAssembler::emit_i16x8_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_min_u");
}

void LiftoffAssembler::emit_i16x8_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_max_s");
}

void LiftoffAssembler::emit_i16x8_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_max_u");
}

void LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_s(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i16x8_extadd_pairwise_i8x16_s");
}

void LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_u(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i16x8_extadd_pairwise_i8x16_u");
}

void LiftoffAssembler::emit_i32x4_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_neg");
}

void LiftoffAssembler::emit_i32x4_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_alltrue");
}

void LiftoffAssembler::emit_i32x4_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_bitmask");
}

void LiftoffAssembler::emit_i32x4_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_shl");
}

void LiftoffAssembler::emit_i32x4_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  bailout(kSimd, "emit_i32x4_shli");
}

void LiftoffAssembler::emit_i32x4_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_shr_s");
}

void LiftoffAssembler::emit_i32x4_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "emit_i32x4_shri_s");
}

void LiftoffAssembler::emit_i32x4_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_shr_u");
}

void LiftoffAssembler::emit_i32x4_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "emit_i32x4_shri_u");
}

void LiftoffAssembler::emit_i32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_add");
}

void LiftoffAssembler::emit_i32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_sub");
}

void LiftoffAssembler::emit_i32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_mul");
}

void LiftoffAssembler::emit_i32x4_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_min_s");
}

void LiftoffAssembler::emit_i32x4_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_min_u");
}

void LiftoffAssembler::emit_i32x4_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_max_s");
}

void LiftoffAssembler::emit_i32x4_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_max_u");
}

void LiftoffAssembler::emit_i32x4_dot_i16x8_s(LiftoffRegister dst,
                                              LiftoffRegister lhs,
                                              LiftoffRegister rhs) {
  bailout(kSimd, "emit_i32x4_dot_i16x8_s");
}

void LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_s(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_extadd_pairwise_i16x8_s");
}

void LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_u(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_extadd_pairwise_i16x8_u");
}

void LiftoffAssembler::emit_i64x2_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i64x2_neg");
}

void LiftoffAssembler::emit_i64x2_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i64x2_alltrue");
}

void LiftoffAssembler::emit_i64x2_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  bailout(kSimd, "emit_i64x2_bitmask");
}

void LiftoffAssembler::emit_i64x2_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i64x2_shl");
}

void LiftoffAssembler::emit_i64x2_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  bailout(kSimd, "emit_i64x2_shli");
}

void LiftoffAssembler::emit_i64x2_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i64x2_shr_s");
}

void LiftoffAssembler::emit_i64x2_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "emit_i64x2_shri_s");
}

void LiftoffAssembler::emit_i64x2_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  bailout(kSimd, "emit_i64x2_shr_u");
}

void LiftoffAssembler::emit_i64x2_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  bailout(kSimd, "emit_i64x2_shri_u");
}

void LiftoffAssembler::emit_i64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i64x2_add");
}

void LiftoffAssembler::emit_i64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i64x2_sub");
}

void LiftoffAssembler::emit_i64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_i64x2_mul");
}

void LiftoffAssembler::emit_i64x2_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i64x2_gt_s");
}

void LiftoffAssembler::emit_i64x2_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_i64x2_ge_s");
}

void LiftoffAssembler::emit_f32x4_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_f32x4_abs");
}

void LiftoffAssembler::emit_f32x4_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_f32x4_neg");
}

void LiftoffAssembler::emit_f32x4_sqrt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  bailout(kSimd, "emit_f32x4_sqrt");
}

bool LiftoffAssembler::emit_f32x4_ceil(LiftoffRegister dst,
                                       LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f32x4_floor(LiftoffRegister dst,
                                        LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f32x4_trunc(LiftoffRegister dst,
                                        LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f32x4_nearest_int(LiftoffRegister dst,
                                              LiftoffRegister src) {
  return false;
}

void LiftoffAssembler::emit_f32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_add");
}

void LiftoffAssembler::emit_f32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_sub");
}

void LiftoffAssembler::emit_f32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_mul");
}

void LiftoffAssembler::emit_f32x4_div(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_div");
}

void LiftoffAssembler::emit_f32x4_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_min");
}

void LiftoffAssembler::emit_f32x4_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_max");
}

void LiftoffAssembler::emit_f32x4_pmin(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_pmin");
}

void LiftoffAssembler::emit_f32x4_pmax(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_f32x4_pmax");
}

void LiftoffAssembler::emit_f64x2_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_f64x2_abs");
}

void LiftoffAssembler::emit_f64x2_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_f64x2_neg");
}

void LiftoffAssembler::emit_f64x2_sqrt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  bailout(kSimd, "emit_f64x2_sqrt");
}

bool LiftoffAssembler::emit_f64x2_ceil(LiftoffRegister dst,
                                       LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f64x2_floor(LiftoffRegister dst,
                                        LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f64x2_trunc(LiftoffRegister dst,
                                        LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f64x2_nearest_int(LiftoffRegister dst,
                                              LiftoffRegister src) {
  return false;
}

void LiftoffAssembler::emit_f64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_add");
}

void LiftoffAssembler::emit_f64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_sub");
}

void LiftoffAssembler::emit_f64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_mul");
}

void LiftoffAssembler::emit_f64x2_div(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_div");
}

void LiftoffAssembler::emit_f64x2_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_min");
}

void LiftoffAssembler::emit_f64x2_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_max");
}

void LiftoffAssembler::emit_f64x2_pmin(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_pmin");
}

void LiftoffAssembler::emit_f64x2_pmax(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  bailout(kSimd, "emit_f64x2_pmax");
}

void LiftoffAssembler::emit_f64x2_convert_low_i32x4_s(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "emit_f64x2_convert_low_i32x4_s");
}

void LiftoffAssembler::emit_f64x2_convert_low_i32x4_u(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "emit_f64x2_convert_low_i32x4_u");
}

void LiftoffAssembler::emit_f64x2_promote_low_f32x4(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  bailout(kSimd, "emit_f64x2_promote_low_f32x4");
}

void LiftoffAssembler::emit_i32x4_sconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_sconvert_f32x4");
}

void LiftoffAssembler::emit_i32x4_uconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_uconvert_f32x4");
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_s_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_trunc_sat_f64x2_s_zero");
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_u_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_trunc_sat_f64x2_u_zero");
}

void LiftoffAssembler::emit_f32x4_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  bailout(kSimd, "emit_f32x4_sconvert_i32x4");
}

void LiftoffAssembler::emit_f32x4_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  bailout(kSimd, "emit_f32x4_uconvert_i32x4");
}

void LiftoffAssembler::emit_f32x4_demote_f64x2_zero(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  bailout(kSimd, "emit_f32x4_demote_f64x2_zero");
}

void LiftoffAssembler::emit_i8x16_sconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_sconvert_i16x8");
}

void LiftoffAssembler::emit_i8x16_uconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_uconvert_i16x8");
}

void LiftoffAssembler::emit_i16x8_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_sconvert_i32x4");
}

void LiftoffAssembler::emit_i16x8_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_uconvert_i32x4");
}

void LiftoffAssembler::emit_i16x8_sconvert_i8x16_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kSimd, "emit_i16x8_sconvert_i8x16_low");
}

void LiftoffAssembler::emit_i16x8_sconvert_i8x16_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i16x8_sconvert_i8x16_high");
}

void LiftoffAssembler::emit_i16x8_uconvert_i8x16_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kSimd, "emit_i16x8_uconvert_i8x16_low");
}

void LiftoffAssembler::emit_i16x8_uconvert_i8x16_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i16x8_uconvert_i8x16_high");
}

void LiftoffAssembler::emit_i32x4_sconvert_i16x8_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_sconvert_i16x8_low");
}

void LiftoffAssembler::emit_i32x4_sconvert_i16x8_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_sconvert_i16x8_high");
}

void LiftoffAssembler::emit_i32x4_uconvert_i16x8_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_uconvert_i16x8_low");
}

void LiftoffAssembler::emit_i32x4_uconvert_i16x8_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_uconvert_i16x8_high");
}

void LiftoffAssembler::emit_i64x2_sconvert_i32x4_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kSimd, "emit_i64x2_sconvert_i32x4_low");
}

void LiftoffAssembler::emit_i64x2_sconvert_i32x4_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i64x2_sconvert_i32x4_high");
}

void LiftoffAssembler::emit_i64x2_uconvert_i32x4_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  bailout(kSimd, "emit_i64x2_uconvert_i32x4_low");
}

void LiftoffAssembler::emit_i64x2_uconvert_i32x4_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i64x2_uconvert_i32x4_high");
}

void LiftoffAssembler::emit_i8x16_rounding_average_u(LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_i8x16_rounding_average_u");
}

void LiftoffAssembler::emit_i16x8_rounding_average_u(LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
  bailout(kSimd, "emit_i16x8_rounding_average_u");
}

void LiftoffAssembler::emit_i8x16_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i8x16_abs");
}

void LiftoffAssembler::emit_i16x8_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i16x8_abs");
}

void LiftoffAssembler::emit_i32x4_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  bailout(kSimd, "emit_i32x4_abs");
}

void LiftoffAssembler::emit_i8x16_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_i8x16_extract_lane_s");
}

void LiftoffAssembler::emit_i8x16_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_i8x16_extract_lane_u");
}

void LiftoffAssembler::emit_i16x8_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_i16x8_extract_lane_s");
}

void LiftoffAssembler::emit_i16x8_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_i16x8_extract_lane_u");
}

void LiftoffAssembler::emit_i32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_i32x4_extract_lane");
}

void LiftoffAssembler::emit_i64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_i64x2_extract_lane");
}

void LiftoffAssembler::emit_f32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_f32x4_extract_lane");
}

void LiftoffAssembler::emit_f64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_f64x2_extract_lane");
}

void LiftoffAssembler::emit_i8x16_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_i8x16_replace_lane");
}

void LiftoffAssembler::emit_i16x8_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_i16x8_replace_lane");
}

void LiftoffAssembler::emit_i32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_i32x4_replace_lane");
}

void LiftoffAssembler::emit_i64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_i64x2_replace_lane");
}

void LiftoffAssembler::emit_f32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_f32x4_replace_lane");
}

void LiftoffAssembler::emit_f64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  bailout(kSimd, "emit_f64x2_replace_lane");
}

void LiftoffAssembler::StackCheck(Label* ool_code, Register limit_address) {
  TurboAssembler::Ulw(limit_address, MemOperand(limit_address));
  TurboAssembler::Branch(ool_code, ule, sp, Operand(limit_address));
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0, GetUnusedRegister(kGpReg, {}).gp());
  CallCFunction(ExternalReference::wasm_call_trap_callback_for_testing(), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  if (FLAG_debug_code) Abort(reason);
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  unsigned num_gp_regs = gp_regs.GetNumRegsSet();
  if (num_gp_regs) {
    unsigned offset = num_gp_regs * kSystemPointerSize;
    addiu(sp, sp, -offset);
    while (!gp_regs.is_empty()) {
      LiftoffRegister reg = gp_regs.GetFirstRegSet();
      offset -= kSystemPointerSize;
      sw(reg.gp(), MemOperand(sp, offset));
      gp_regs.clear(reg);
    }
    DCHECK_EQ(offset, 0);
  }
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned num_fp_regs = fp_regs.GetNumRegsSet();
  if (num_fp_regs) {
    addiu(sp, sp, -(num_fp_regs * kStackSlotSize));
    unsigned offset = 0;
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetFirstRegSet();
      TurboAssembler::Sdc1(reg.fp(), MemOperand(sp, offset));
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
    TurboAssembler::Ldc1(reg.fp(), MemOperand(sp, fp_offset));
    fp_regs.clear(reg);
    fp_offset += sizeof(double);
  }
  if (fp_offset) addiu(sp, sp, fp_offset);
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  unsigned gp_offset = 0;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetLastRegSet();
    lw(reg.gp(), MemOperand(sp, gp_offset));
    gp_regs.clear(reg);
    gp_offset += kSystemPointerSize;
  }
  addiu(sp, sp, gp_offset);
}

void LiftoffAssembler::RecordSpillsInSafepoint(
    SafepointTableBuilder::Safepoint& safepoint, LiftoffRegList all_spills,
    LiftoffRegList ref_spills, int spill_offset) {
  int spill_space_size = 0;
  while (!all_spills.is_empty()) {
    LiftoffRegister reg = all_spills.GetFirstRegSet();
    if (ref_spills.has(reg)) {
      safepoint.DefineTaggedStackSlot(spill_offset);
    }
    all_spills.clear(reg);
    ++spill_offset;
    spill_space_size += kSystemPointerSize;
  }
  // Record the number of additional spill slots.
  RecordOolSpillSpaceSize(spill_space_size);
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  DCHECK_LT(num_stack_slots,
            (1 << 16) / kSystemPointerSize);  // 16 bit immediate
  TurboAssembler::DropAndRet(static_cast<int>(num_stack_slots));
}

void LiftoffAssembler::CallC(const ValueKindSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueKind out_argument_kind, int stack_bytes,
                             ExternalReference ext_ref) {
  addiu(sp, sp, -stack_bytes);

  int arg_bytes = 0;
  for (ValueKind param_kind : sig->parameters()) {
    liftoff::Store(this, sp, arg_bytes, *args++, param_kind);
    arg_bytes += value_kind_size(param_kind);
  }
  DCHECK_LE(arg_bytes, stack_bytes);

  // Pass a pointer to the buffer with the arguments to the C function.
  // On mips, the first argument is passed in {a0}.
  constexpr Register kFirstArgReg = a0;
  mov(kFirstArgReg, sp);

  // Now call the C function.
  constexpr int kNumCCallArgs = 1;
  PrepareCallCFunction(kNumCCallArgs, kScratchReg);
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* next_result_reg = rets;
  if (sig->return_count() > 0) {
    DCHECK_EQ(1, sig->return_count());
    constexpr Register kReturnReg = v0;
    if (kReturnReg != next_result_reg->gp()) {
      Move(*next_result_reg, LiftoffRegister(kReturnReg), sig->GetReturn(0));
    }
    ++next_result_reg;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_kind != kVoid) {
    liftoff::Load(this, *next_result_reg, sp, 0, out_argument_kind);
  }

  addiu(sp, sp, stack_bytes);
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  Call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::TailCallNativeWasmCode(Address addr) {
  Jump(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallIndirect(const ValueKindSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  if (target == no_reg) {
    pop(kScratchReg);
    Call(kScratchReg);
  } else {
    Call(target);
  }
}

void LiftoffAssembler::TailCallIndirect(Register target) {
  if (target == no_reg) {
    Pop(kScratchReg);
    Jump(kScratchReg);
  } else {
    Jump(target);
  }
}

void LiftoffAssembler::CallRuntimeStub(WasmCode::RuntimeStubId sid) {
  // A direct call to a wasm runtime stub defined in this module.
  // Just encode the stub index. This will be patched at relocation.
  Call(static_cast<Address>(sid), RelocInfo::WASM_STUB_CALL);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  addiu(sp, sp, -size);
  TurboAssembler::Move(addr, sp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  addiu(sp, sp, size);
}

void LiftoffAssembler::MaybeOSR() {}

void LiftoffAssembler::emit_set_if_nan(Register dst, FPURegister src,
                                       ValueKind kind) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Label not_nan;
  if (kind == kF32) {
    CompareIsNanF32(src, src);
  } else {
    DCHECK_EQ(kind, kF64);
    CompareIsNanF64(src, src);
  }
  BranchFalseShortF(&not_nan, USE_DELAY_SLOT);
  li(scratch, 1);
  sw(scratch, MemOperand(dst));
  bind(&not_nan);
}

void LiftoffAssembler::emit_s128_set_if_nan(Register dst, LiftoffRegister src,
                                            Register tmp_gp,
                                            LiftoffRegister tmp_s128,
                                            ValueKind lane_kind) {
  UNIMPLEMENTED();
}

void LiftoffStackSlots::Construct(int param_slots) {
  DCHECK_LT(0, slots_.size());
  SortInPushOrder();
  int last_stack_slot = param_slots;
  for (auto& slot : slots_) {
    const int stack_slot = slot.dst_slot_;
    int stack_decrement = (last_stack_slot - stack_slot) * kSystemPointerSize;
    DCHECK_LT(0, stack_decrement);
    last_stack_slot = stack_slot;
    const LiftoffAssembler::VarState& src = slot.src_;
    switch (src.loc()) {
      case LiftoffAssembler::VarState::kStack: {
        if (src.kind() == kF64) {
          asm_->AllocateStackSpace(stack_decrement - kDoubleSize);
          DCHECK_EQ(kLowWord, slot.half_);
          asm_->lw(kScratchReg,
                   liftoff::GetHalfStackSlot(slot.src_offset_, kHighWord));
          asm_->push(kScratchReg);
        } else {
          asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
        }
        asm_->lw(kScratchReg,
                 liftoff::GetHalfStackSlot(slot.src_offset_, slot.half_));
        asm_->push(kScratchReg);
        break;
      }
      case LiftoffAssembler::VarState::kRegister: {
        int pushed_bytes = SlotSizeInBytes(slot);
        asm_->AllocateStackSpace(stack_decrement - pushed_bytes);
        if (src.kind() == kI64) {
          liftoff::push(
              asm_, slot.half_ == kLowWord ? src.reg().low() : src.reg().high(),
              kI32);
        } else {
          liftoff::push(asm_, src.reg(), src.kind());
        }
        break;
      }
      case LiftoffAssembler::VarState::kIntConst: {
        // The high word is the sign extension of the low word.
        asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
        asm_->li(kScratchReg,
                 Operand(slot.half_ == kLowWord ? src.i32_const()
                                                : src.i32_const() >> 31));
        asm_->push(kScratchReg);
        break;
      }
    }
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_MIPS_LIFTOFF_ASSEMBLER_MIPS_H_

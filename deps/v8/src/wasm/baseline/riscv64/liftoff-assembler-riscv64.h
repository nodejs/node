// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_RISCV64_LIFTOFF_ASSEMBLER_RISCV64_H_
#define V8_WASM_BASELINE_RISCV64_LIFTOFF_ASSEMBLER_RISCV64_H_

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

// Liftoff Frames.
//
//  slot      Frame
//       +--------------------+---------------------------
//  n+4  | optional padding slot to keep the stack 16 byte aligned.
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
//  -2   |     instance       |
//  -3   |     feedback vector|
//  -4   |     tiering budget |
//  -----+--------------------+---------------------------
//  -5   |     slot 0         |   ^
//  -6   |     slot 1         |   |
//       |                    | Frame slots
//       |                    |   |
//       |                    |   v
//       | optional padding slot to keep the stack 16 byte aligned.
//  -----+--------------------+  <-- stack ptr (sp)
//

// fp-8 holds the stack marker, fp-16 is the instance parameter.
constexpr int kInstanceOffset = 2 * kSystemPointerSize;
constexpr int kFeedbackVectorOffset = 3 * kSystemPointerSize;
constexpr int kTierupBudgetOffset = 4 * kSystemPointerSize;

inline MemOperand GetStackSlot(int offset) { return MemOperand(fp, -offset); }

inline MemOperand GetInstanceOperand() { return GetStackSlot(kInstanceOffset); }

inline MemOperand GetMemOp(LiftoffAssembler* assm, Register addr,
                           Register offset, uintptr_t offset_imm) {
  if (is_uint31(offset_imm)) {
    int32_t offset_imm32 = static_cast<int32_t>(offset_imm);
    if (offset == no_reg) return MemOperand(addr, offset_imm32);
    assm->Add64(kScratchReg2, addr, offset);
    return MemOperand(kScratchReg2, offset_imm32);
  }
  // Offset immediate does not fit in 31 bits.
  assm->li(kScratchReg2, offset_imm);
  assm->Add64(kScratchReg2, kScratchReg2, addr);
  if (offset != no_reg) {
    assm->Add64(kScratchReg2, kScratchReg2, offset);
  }
  return MemOperand(kScratchReg2, 0);
}

inline void Load(LiftoffAssembler* assm, LiftoffRegister dst, MemOperand src,
                 ValueKind kind) {
  switch (kind) {
    case kI32:
      assm->Lw(dst.gp(), src);
      break;
    case kI64:
    case kRef:
    case kOptRef:
    case kRtt:
      assm->Ld(dst.gp(), src);
      break;
    case kF32:
      assm->LoadFloat(dst.fp(), src);
      break;
    case kF64:
      assm->LoadDouble(dst.fp(), src);
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
      assm->Sw(src.gp(), dst);
      break;
    case kI64:
    case kOptRef:
    case kRef:
    case kRtt:
      assm->Sd(src.gp(), dst);
      break;
    case kF32:
      assm->StoreFloat(src.fp(), dst);
      break;
    case kF64:
      assm->StoreDouble(src.fp(), dst);
      break;
    default:
      UNREACHABLE();
  }
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueKind kind) {
  switch (kind) {
    case kI32:
      assm->addi(sp, sp, -kSystemPointerSize);
      assm->Sw(reg.gp(), MemOperand(sp, 0));
      break;
    case kI64:
    case kOptRef:
    case kRef:
    case kRtt:
      assm->push(reg.gp());
      break;
    case kF32:
      assm->addi(sp, sp, -kSystemPointerSize);
      assm->StoreFloat(reg.fp(), MemOperand(sp, 0));
      break;
    case kF64:
      assm->addi(sp, sp, -kSystemPointerSize);
      assm->StoreDouble(reg.fp(), MemOperand(sp, 0));
      break;
    default:
      UNREACHABLE();
  }
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
    case LoadType::kI64Load32U:
      assm->TurboAssembler::ByteSwapUnsigned(tmp.gp(), tmp.gp(), 4);
      break;
    case LoadType::kI32Load:
    case LoadType::kI64Load32S:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 4);
      break;
    case LoadType::kI32Load16S:
    case LoadType::kI64Load16S:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 2);
      break;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      assm->TurboAssembler::ByteSwapUnsigned(tmp.gp(), tmp.gp(), 2);
      break;
    case LoadType::kF64Load:
      is_float = true;
      tmp = assm->GetUnusedRegister(kGpReg, pinned);
      assm->emit_type_conversion(kExprI64ReinterpretF64, tmp, dst);
      V8_FALLTHROUGH;
    case LoadType::kI64Load:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 8);
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
      tmp = assm->GetUnusedRegister(kGpReg, pinned);
      assm->emit_type_conversion(kExprI64ReinterpretF64, tmp, src);
      V8_FALLTHROUGH;
    case StoreType::kI64Store:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 8);
      break;
    case StoreType::kI64Store32:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 4);
      break;
    case StoreType::kI64Store16:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 2);
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
  // When the frame size is bigger than 4KB, we need two instructions for
  // stack checking, so we reserve space for this case.
  addi(sp, sp, 0);
  nop();
  nop();
  return offset;
}

void LiftoffAssembler::PrepareTailCall(int num_callee_stack_params,
                                       int stack_param_delta) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();

  // Push the return address and frame pointer to complete the stack frame.
  Ld(scratch, MemOperand(fp, 8));
  Push(scratch);
  Ld(scratch, MemOperand(fp, 0));
  Push(scratch);

  // Shift the whole frame upwards.
  int slot_count = num_callee_stack_params + 2;
  for (int i = slot_count - 1; i >= 0; --i) {
    Ld(scratch, MemOperand(sp, i * 8));
    Sd(scratch, MemOperand(fp, (i - stack_param_delta) * 8));
  }

  // Set the new stack and frame pointer.
  Add64(sp, fp, -stack_param_delta * 8);
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
    patching_assembler.Add64(sp, sp, Operand(-frame_size));
    return;
  }

  // The frame size is bigger than 4KB, so we might overflow the available stack
  // space if we first allocate the frame and then do the stack check (we will
  // need some remaining stack space for throwing the exception). That's why we
  // check the available stack space before we allocate the frame. To do this we
  // replace the {__ Add64(sp, sp, -frame_size)} with a jump to OOL code that
  // does this "extended stack check".
  //
  // The OOL code can simply be generated here with the normal assembler,
  // because all other code generation, including OOL code, has already finished
  // when {PatchPrepareStackFrame} is called. The function prologue then jumps
  // to the current {pc_offset()} to execute the OOL code for allocating the
  // large frame.
  // Emit the unconditional branch in the function prologue (from {offset} to
  // {pc_offset()}).

  int imm32 = pc_offset() - offset;
  patching_assembler.GenPCRelativeJump(kScratchReg, imm32);

  // If the frame is bigger than the stack, we throw the stack overflow
  // exception unconditionally. Thereby we can avoid the integer overflow
  // check in the condition code.
  RecordComment("OOL: stack check for large frame");
  Label continuation;
  if (frame_size < FLAG_stack_size * 1024) {
    Register stack_limit = kScratchReg;
    Ld(stack_limit,
       FieldMemOperand(kWasmInstanceRegister,
                       WasmInstanceObject::kRealStackLimitAddressOffset));
    Ld(stack_limit, MemOperand(stack_limit));
    Add64(stack_limit, stack_limit, Operand(frame_size));
    Branch(&continuation, uge, sp, Operand(stack_limit));
  }

  Call(wasm::WasmCode::kWasmStackOverflow, RelocInfo::WASM_STUB_CALL);
  // The call will not return; just define an empty safepoint.
  safepoint_table_builder->DefineSafepoint(this);
  if (FLAG_debug_code) stop();

  bind(&continuation);

  // Now allocate the stack space. Note that this might do more than just
  // decrementing the SP;
  Add64(sp, sp, Operand(-frame_size));

  // Jump back to the start of the function, from {pc_offset()} to
  // right after the reserved space for the {__ Add64(sp, sp, -framesize)}
  // (which is a Branch now).
  int func_start_offset = offset + 2 * kInstrSize;
  imm32 = func_start_offset - pc_offset();
  GenPCRelativeJump(kScratchReg, imm32);
}

void LiftoffAssembler::FinishCode() { ForceConstantPoolEmissionWithoutJump(); }

void LiftoffAssembler::AbortCompilation() { AbortedCodeGeneration(); }

// static
constexpr int LiftoffAssembler::StaticStackFrameSize() {
  return liftoff::kFeedbackVectorOffset;
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
  switch (kind) {
    case kS128:
      return true;
    default:
      // No alignment because all other types are kStackSlotSize.
      return false;
  }
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  switch (value.type().kind()) {
    case kI32:
      TurboAssembler::li(reg.gp(), Operand(value.to_i32(), rmode));
      break;
    case kI64:
      TurboAssembler::li(reg.gp(), Operand(value.to_i64(), rmode));
      break;
    case kF32:
      TurboAssembler::LoadFPRImmediate(reg.fp(),
                                       value.to_f32_boxed().get_bits());
      break;
    case kF64:
      TurboAssembler::LoadFPRImmediate(reg.fp(),
                                       value.to_f64_boxed().get_bits());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadInstanceFromFrame(Register dst) {
  Ld(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::LoadFromInstance(Register dst, Register instance,
                                        int offset, int size) {
  DCHECK_LE(0, offset);
  MemOperand src{instance, offset};
  switch (size) {
    case 1:
      Lb(dst, MemOperand(src));
      break;
    case 4:
      Lw(dst, MemOperand(src));
      break;
    case 8:
      Ld(dst, MemOperand(src));
      break;
    default:
      UNIMPLEMENTED();
  }
}

void LiftoffAssembler::LoadTaggedPointerFromInstance(Register dst,
                                                     Register instance,
                                                     int offset) {
  DCHECK_LE(0, offset);
  LoadTaggedPointerField(dst, MemOperand{instance, offset});
}

void LiftoffAssembler::SpillInstance(Register instance) {
  Sd(instance, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::ResetOSRTarget() {}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         int32_t offset_imm,
                                         LiftoffRegList pinned) {
  MemOperand src_op = liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm);
  LoadTaggedPointerField(dst, src_op);
}

void LiftoffAssembler::LoadFullPointer(Register dst, Register src_addr,
                                       int32_t offset_imm) {
  MemOperand src_op = liftoff::GetMemOp(this, src_addr, no_reg, offset_imm);
  Ld(dst, src_op);
}

void LiftoffAssembler::StoreTaggedPointer(Register dst_addr,
                                          Register offset_reg,
                                          int32_t offset_imm,
                                          LiftoffRegister src,
                                          LiftoffRegList pinned,
                                          SkipWriteBarrier skip_write_barrier) {
  Register scratch = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  MemOperand dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm);
  StoreTaggedField(src.gp(), dst_op);

  if (skip_write_barrier || FLAG_disable_write_barriers) return;

  Label write_barrier;
  Label exit;
  CheckPageFlag(dst_addr, scratch,
                MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                &write_barrier);
  Branch(&exit);
  bind(&write_barrier);
  JumpIfSmi(src.gp(), &exit);
  CheckPageFlag(src.gp(), scratch,
                MemoryChunk::kPointersToHereAreInterestingMask, eq, &exit);
  Add64(scratch, dst_op.rm(), dst_op.offset());
  CallRecordWriteStubSaveRegisters(
      dst_addr, scratch, RememberedSetAction::kEmit, SaveFPRegsMode::kSave,
      StubCallMode::kCallWasmRuntimeStub);
  bind(&exit);
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uintptr_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc, bool is_load_mem,
                            bool i64_offset) {
  MemOperand src_op = liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm);

  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U:
      Lbu(dst.gp(), src_op);
      break;
    case LoadType::kI32Load8S:
    case LoadType::kI64Load8S:
      Lb(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      TurboAssembler::Lhu(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16S:
    case LoadType::kI64Load16S:
      TurboAssembler::Lh(dst.gp(), src_op);
      break;
    case LoadType::kI64Load32U:
      TurboAssembler::Lwu(dst.gp(), src_op);
      break;
    case LoadType::kI32Load:
    case LoadType::kI64Load32S:
      TurboAssembler::Lw(dst.gp(), src_op);
      break;
    case LoadType::kI64Load:
      TurboAssembler::Ld(dst.gp(), src_op);
      break;
    case LoadType::kF32Load:
      TurboAssembler::LoadFloat(dst.fp(), src_op);
      break;
    case LoadType::kF64Load:
      TurboAssembler::LoadDouble(dst.fp(), src_op);
      break;
    case LoadType::kS128Load: {
      VU.set(kScratchReg, E8, m1);
      Register src_reg = src_op.offset() == 0 ? src_op.rm() : kScratchReg;
      if (src_op.offset() != 0) {
        TurboAssembler::Add64(src_reg, src_op.rm(), src_op.offset());
      }
      vl(dst.fp().toV(), src_reg, 0, E8);
      break;
    }
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
                             uintptr_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  MemOperand dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm);

#if defined(V8_TARGET_BIG_ENDIAN)
  if (is_store_mem) {
    pinned.set(dst_op.rm());
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
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      Sb(src.gp(), dst_op);
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      TurboAssembler::Sh(src.gp(), dst_op);
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      TurboAssembler::Sw(src.gp(), dst_op);
      break;
    case StoreType::kI64Store:
      TurboAssembler::Sd(src.gp(), dst_op);
      break;
    case StoreType::kF32Store:
      TurboAssembler::StoreFloat(src.fp(), dst_op);
      break;
    case StoreType::kF64Store:
      TurboAssembler::StoreDouble(src.fp(), dst_op);
      break;
    case StoreType::kS128Store: {
      VU.set(kScratchReg, E8, m1);
      Register dst_reg = dst_op.offset() == 0 ? dst_op.rm() : kScratchReg;
      if (dst_op.offset() != 0) {
        Add64(kScratchReg, dst_op.rm(), dst_op.offset());
      }
      vs(src.fp().toV(), dst_reg, 0, VSew::E8);
      break;
    }
    default:
      UNREACHABLE();
  }
}

namespace liftoff {
#define __ lasm->

inline Register CalculateActualAddress(LiftoffAssembler* lasm,
                                       Register addr_reg, Register offset_reg,
                                       uintptr_t offset_imm,
                                       Register result_reg) {
  DCHECK_NE(offset_reg, no_reg);
  DCHECK_NE(addr_reg, no_reg);
  __ Add64(result_reg, addr_reg, Operand(offset_reg));
  if (offset_imm != 0) {
    __ Add64(result_reg, result_reg, Operand(offset_imm));
  }
  return result_reg;
}

enum class Binop { kAdd, kSub, kAnd, kOr, kXor, kExchange };

inline void AtomicBinop(LiftoffAssembler* lasm, Register dst_addr,
                        Register offset_reg, uintptr_t offset_imm,
                        LiftoffRegister value, LiftoffRegister result,
                        StoreType type, Binop op) {
  LiftoffRegList pinned = {dst_addr, offset_reg, value, result};
  Register store_result = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();

  // Make sure that {result} is unique.
  Register result_reg = result.gp();
  if (result_reg == value.gp() || result_reg == dst_addr ||
      result_reg == offset_reg) {
    result_reg = __ GetUnusedRegister(kGpReg, pinned).gp();
  }

  UseScratchRegisterScope temps(lasm);
  Register actual_addr = liftoff::CalculateActualAddress(
      lasm, dst_addr, offset_reg, offset_imm, temps.Acquire());

  // Allocate an additional {temp} register to hold the result that should be
  // stored to memory. Note that {temp} and {store_result} are not allowed to be
  // the same register.
  Register temp = temps.Acquire();

  Label retry;
  __ bind(&retry);
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      __ lbu(result_reg, actual_addr, 0);
      __ sync();
      break;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      __ lhu(result_reg, actual_addr, 0);
      __ sync();
      break;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      __ lr_w(true, false, result_reg, actual_addr);
      break;
    case StoreType::kI64Store:
      __ lr_d(true, false, result_reg, actual_addr);
      break;
    default:
      UNREACHABLE();
  }

  switch (op) {
    case Binop::kAdd:
      __ add(temp, result_reg, value.gp());
      break;
    case Binop::kSub:
      __ sub(temp, result_reg, value.gp());
      break;
    case Binop::kAnd:
      __ and_(temp, result_reg, value.gp());
      break;
    case Binop::kOr:
      __ or_(temp, result_reg, value.gp());
      break;
    case Binop::kXor:
      __ xor_(temp, result_reg, value.gp());
      break;
    case Binop::kExchange:
      __ mv(temp, value.gp());
      break;
  }
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      __ sync();
      __ sb(temp, actual_addr, 0);
      __ sync();
      __ mv(store_result, zero_reg);
      break;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      __ sync();
      __ sh(temp, actual_addr, 0);
      __ sync();
      __ mv(store_result, zero_reg);
      break;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      __ sc_w(false, true, store_result, actual_addr, temp);
      break;
    case StoreType::kI64Store:
      __ sc_w(false, true, store_result, actual_addr, temp);
      break;
    default:
      UNREACHABLE();
  }

  __ bnez(store_result, &retry);
  if (result_reg != result.gp()) {
    __ mv(result.gp(), result_reg);
  }
}

#undef __
}  // namespace liftoff

void LiftoffAssembler::AtomicLoad(LiftoffRegister dst, Register src_addr,
                                  Register offset_reg, uintptr_t offset_imm,
                                  LoadType type, LiftoffRegList pinned) {
  UseScratchRegisterScope temps(this);
  Register src_reg = liftoff::CalculateActualAddress(
      this, src_addr, offset_reg, offset_imm, temps.Acquire());
  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U:
      fence(PSR | PSW, PSR | PSW);
      lbu(dst.gp(), src_reg, 0);
      fence(PSR, PSR | PSW);
      return;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      fence(PSR | PSW, PSR | PSW);
      lhu(dst.gp(), src_reg, 0);
      fence(PSR, PSR | PSW);
      return;
    case LoadType::kI32Load:
    case LoadType::kI64Load32U:
      fence(PSR | PSW, PSR | PSW);
      lw(dst.gp(), src_reg, 0);
      fence(PSR, PSR | PSW);
      return;
    case LoadType::kI64Load:
      fence(PSR | PSW, PSR | PSW);
      ld(dst.gp(), src_reg, 0);
      fence(PSR, PSR | PSW);
      return;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicStore(Register dst_addr, Register offset_reg,
                                   uintptr_t offset_imm, LiftoffRegister src,
                                   StoreType type, LiftoffRegList pinned) {
  UseScratchRegisterScope temps(this);
  Register dst_reg = liftoff::CalculateActualAddress(
      this, dst_addr, offset_reg, offset_imm, temps.Acquire());
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      fence(PSR | PSW, PSW);
      sb(src.gp(), dst_reg, 0);
      return;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      fence(PSR | PSW, PSW);
      sh(src.gp(), dst_reg, 0);
      return;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      fence(PSR | PSW, PSW);
      sw(src.gp(), dst_reg, 0);
      return;
    case StoreType::kI64Store:
      fence(PSR | PSW, PSW);
      sd(src.gp(), dst_reg, 0);
      return;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicAdd(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kAdd);
}

void LiftoffAssembler::AtomicSub(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kSub);
}

void LiftoffAssembler::AtomicAnd(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kAnd);
}

void LiftoffAssembler::AtomicOr(Register dst_addr, Register offset_reg,
                                uintptr_t offset_imm, LiftoffRegister value,
                                LiftoffRegister result, StoreType type) {
  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kOr);
}

void LiftoffAssembler::AtomicXor(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kXor);
}

void LiftoffAssembler::AtomicExchange(Register dst_addr, Register offset_reg,
                                      uintptr_t offset_imm,
                                      LiftoffRegister value,
                                      LiftoffRegister result, StoreType type) {
  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kExchange);
}

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(load_linked,       \
                                                 store_conditional) \
  do {                                                              \
    Label compareExchange;                                          \
    Label exit;                                                     \
    sync();                                                         \
    bind(&compareExchange);                                         \
    load_linked(result.gp(), MemOperand(temp0, 0));                 \
    BranchShort(&exit, ne, expected.gp(), Operand(result.gp()));    \
    mv(temp2, new_value.gp());                                      \
    store_conditional(temp2, MemOperand(temp0, 0));                 \
    BranchShort(&compareExchange, eq, temp2, Operand(zero_reg));    \
    bind(&exit);                                                    \
    sync();                                                         \
  } while (0)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(            \
    load_linked, store_conditional, size, aligned)               \
  do {                                                           \
    Label compareExchange;                                       \
    Label exit;                                                  \
    andi(temp1, temp0, aligned);                                 \
    Sub64(temp0, temp0, Operand(temp1));                         \
    Sll32(temp1, temp1, 3);                                      \
    sync();                                                      \
    bind(&compareExchange);                                      \
    load_linked(temp2, MemOperand(temp0, 0));                    \
    ExtractBits(result.gp(), temp2, temp1, size, false);         \
    ExtractBits(temp2, expected.gp(), zero_reg, size, false);    \
    BranchShort(&exit, ne, temp2, Operand(result.gp()));         \
    InsertBits(temp2, new_value.gp(), temp1, size);              \
    store_conditional(temp2, MemOperand(temp0, 0));              \
    BranchShort(&compareExchange, eq, temp2, Operand(zero_reg)); \
    bind(&exit);                                                 \
    sync();                                                      \
  } while (0)

void LiftoffAssembler::AtomicCompareExchange(
    Register dst_addr, Register offset_reg, uintptr_t offset_imm,
    LiftoffRegister expected, LiftoffRegister new_value, LiftoffRegister result,
    StoreType type) {
  LiftoffRegList pinned = {dst_addr, offset_reg, expected, new_value, result};
  Register temp0 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  Register temp1 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  Register temp2 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  MemOperand dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm);
  Add64(temp0, dst_op.rm(), dst_op.offset());
  switch (type.value()) {
    case StoreType::kI64Store8:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll, Sc, 8, 7);
      break;
    case StoreType::kI32Store8:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll, Sc, 8, 3);
      break;
    case StoreType::kI64Store16:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll, Sc, 16, 7);
      break;
    case StoreType::kI32Store16:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll, Sc, 16, 3);
      break;
    case StoreType::kI64Store32:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Lld, Scd, 32, 7);
      break;
    case StoreType::kI32Store:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(Ll, Sc);
      break;
    case StoreType::kI64Store:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(Lld, Scd);
      break;
    default:
      UNREACHABLE();
  }
}
#undef ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER
#undef ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT

void LiftoffAssembler::AtomicFence() { sync(); }

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueKind kind) {
  MemOperand src(fp, kSystemPointerSize * (caller_slot_idx + 1));
  liftoff::Load(this, dst, src, kind);
}

void LiftoffAssembler::StoreCallerFrameSlot(LiftoffRegister src,
                                            uint32_t caller_slot_idx,
                                            ValueKind kind) {
  int32_t offset = kSystemPointerSize * (caller_slot_idx + 1);
  liftoff::Store(this, fp, offset, src, kind);
}

void LiftoffAssembler::LoadReturnStackSlot(LiftoffRegister dst, int offset,
                                           ValueKind kind) {
  liftoff::Load(this, dst, MemOperand(sp, offset), kind);
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
  // TODO(ksreten): Handle different sizes here.
  TurboAssembler::Move(dst, src);
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueKind kind) {
  DCHECK_NE(dst, src);
  if (kind != kS128) {
    TurboAssembler::Move(dst, src);
  } else {
    TurboAssembler::vmv_vv(dst.toV(), dst.toV());
  }
}

void LiftoffAssembler::Spill(int offset, LiftoffRegister reg, ValueKind kind) {
  RecordUsedSpillOffset(offset);
  MemOperand dst = liftoff::GetStackSlot(offset);
  switch (kind) {
    case kI32:
      Sw(reg.gp(), dst);
      break;
    case kI64:
    case kRef:
    case kOptRef:
    case kRtt:
      Sd(reg.gp(), dst);
      break;
    case kF32:
      StoreFloat(reg.fp(), dst);
      break;
    case kF64:
      TurboAssembler::StoreDouble(reg.fp(), dst);
      break;
    case kS128: {
      VU.set(kScratchReg, E8, m1);
      Register dst_reg = dst.offset() == 0 ? dst.rm() : kScratchReg;
      if (dst.offset() != 0) {
        Add64(kScratchReg, dst.rm(), dst.offset());
      }
      vs(reg.fp().toV(), dst_reg, 0, VSew::E8);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(int offset, WasmValue value) {
  RecordUsedSpillOffset(offset);
  MemOperand dst = liftoff::GetStackSlot(offset);
  switch (value.type().kind()) {
    case kI32: {
      LiftoffRegister tmp = GetUnusedRegister(kGpReg, {});
      TurboAssembler::li(tmp.gp(), Operand(value.to_i32()));
      Sw(tmp.gp(), dst);
      break;
    }
    case kI64:
    case kRef:
    case kOptRef: {
      LiftoffRegister tmp = GetUnusedRegister(kGpReg, {});
      TurboAssembler::li(tmp.gp(), value.to_i64());
      Sd(tmp.gp(), dst);
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
      Lw(reg.gp(), src);
      break;
    case kI64:
    case kRef:
    case kOptRef:
      Ld(reg.gp(), src);
      break;
    case kF32:
      LoadFloat(reg.fp(), src);
      break;
    case kF64:
      TurboAssembler::LoadDouble(reg.fp(), src);
      break;
    case kS128: {
      VU.set(kScratchReg, E8, m1);
      Register src_reg = src.offset() == 0 ? src.rm() : kScratchReg;
      if (src.offset() != 0) {
        TurboAssembler::Add64(src_reg, src.rm(), src.offset());
      }
      vl(reg.fp().toV(), src_reg, 0, E8);
      break;
    }
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

  if (size <= 12 * kStackSlotSize) {
    // Special straight-line code for up to 12 slots. Generates one
    // instruction per slot (<= 12 instructions total).
    uint32_t remainder = size;
    for (; remainder >= kStackSlotSize; remainder -= kStackSlotSize) {
      Sd(zero_reg, liftoff::GetStackSlot(start + remainder));
    }
    DCHECK(remainder == 4 || remainder == 0);
    if (remainder) {
      Sw(zero_reg, liftoff::GetStackSlot(start + remainder));
    }
  } else {
    // General case for bigger counts (12 instructions).
    // Use a0 for start address (inclusive), a1 for end address (exclusive).
    Push(a1, a0);
    Add64(a0, fp, Operand(-start - size));
    Add64(a1, fp, Operand(-start));

    Label loop;
    bind(&loop);
    Sd(zero_reg, MemOperand(a0));
    addi(a0, a0, kSystemPointerSize);
    BranchShort(&loop, ne, a0, Operand(a1));

    Pop(a1, a0);
  }
}

void LiftoffAssembler::emit_i64_clz(LiftoffRegister dst, LiftoffRegister src) {
  TurboAssembler::Clz64(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_i64_ctz(LiftoffRegister dst, LiftoffRegister src) {
  TurboAssembler::Ctz64(dst.gp(), src.gp());
}

bool LiftoffAssembler::emit_i64_popcnt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  TurboAssembler::Popcnt64(dst.gp(), src.gp(), kScratchReg);
  return true;
}

void LiftoffAssembler::emit_i32_mul(Register dst, Register lhs, Register rhs) {
  TurboAssembler::Mul32(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));

  // Check if lhs == kMinInt and rhs == -1, since this case is unrepresentable.
  TurboAssembler::CompareI(kScratchReg, lhs, Operand(kMinInt), ne);
  TurboAssembler::CompareI(kScratchReg2, rhs, Operand(-1), ne);
  add(kScratchReg, kScratchReg, kScratchReg2);
  TurboAssembler::Branch(trap_div_unrepresentable, eq, kScratchReg,
                         Operand(zero_reg));

  TurboAssembler::Div32(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Divu32(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Mod32(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Modu32(dst, lhs, rhs);
}

#define I32_BINOP(name, instruction)                                 \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register lhs, \
                                         Register rhs) {             \
    instruction(dst, lhs, rhs);                                      \
  }

// clang-format off
I32_BINOP(add, addw)
I32_BINOP(sub, subw)
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
I32_BINOP_I(add, Add32)
I32_BINOP_I(sub, Sub32)
I32_BINOP_I(and, And)
I32_BINOP_I(or, Or)
I32_BINOP_I(xor, Xor)
// clang-format on

#undef I32_BINOP_I

void LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  TurboAssembler::Clz32(dst, src);
}

void LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  TurboAssembler::Ctz32(dst, src);
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  TurboAssembler::Popcnt32(dst, src, kScratchReg);
  return true;
}

#define I32_SHIFTOP(name, instruction)                               \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register src, \
                                         Register amount) {          \
    instruction(dst, src, amount);                                   \
  }
#define I32_SHIFTOP_I(name, instruction)                                \
  void LiftoffAssembler::emit_i32_##name##i(Register dst, Register src, \
                                            int amount) {               \
    instruction(dst, src, amount & 31);                                 \
  }

I32_SHIFTOP(shl, sllw)
I32_SHIFTOP(sar, sraw)
I32_SHIFTOP(shr, srlw)

I32_SHIFTOP_I(shl, slliw)
I32_SHIFTOP_I(sar, sraiw)
I32_SHIFTOP_I(shr, srliw)

#undef I32_SHIFTOP
#undef I32_SHIFTOP_I

void LiftoffAssembler::emit_i64_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  TurboAssembler::Mul64(dst.gp(), lhs.gp(), rhs.gp());
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));

  // Check if lhs == MinInt64 and rhs == -1, since this case is unrepresentable.
  TurboAssembler::CompareI(kScratchReg, lhs.gp(),
                           Operand(std::numeric_limits<int64_t>::min()), ne);
  TurboAssembler::CompareI(kScratchReg2, rhs.gp(), Operand(-1), ne);
  add(kScratchReg, kScratchReg, kScratchReg2);
  TurboAssembler::Branch(trap_div_unrepresentable, eq, kScratchReg,
                         Operand(zero_reg));

  TurboAssembler::Div64(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));
  TurboAssembler::Divu64(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));
  TurboAssembler::Mod64(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));
  TurboAssembler::Modu64(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

#define I64_BINOP(name, instruction)                                   \
  void LiftoffAssembler::emit_i64_##name(                              \
      LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
    instruction(dst.gp(), lhs.gp(), rhs.gp());                         \
  }

// clang-format off
I64_BINOP(add, add)
I64_BINOP(sub, sub)
I64_BINOP(and, and_)
I64_BINOP(or, or_)
I64_BINOP(xor, xor_)
// clang-format on

#undef I64_BINOP

#define I64_BINOP_I(name, instruction)                         \
  void LiftoffAssembler::emit_i64_##name##i(                   \
      LiftoffRegister dst, LiftoffRegister lhs, int32_t imm) { \
    instruction(dst.gp(), lhs.gp(), Operand(imm));             \
  }

// clang-format off
I64_BINOP_I(and, And)
I64_BINOP_I(or, Or)
I64_BINOP_I(xor, Xor)
// clang-format on

#undef I64_BINOP_I

#define I64_SHIFTOP(name, instruction)                             \
  void LiftoffAssembler::emit_i64_##name(                          \
      LiftoffRegister dst, LiftoffRegister src, Register amount) { \
    instruction(dst.gp(), src.gp(), amount);                       \
  }

I64_SHIFTOP(shl, sll)
I64_SHIFTOP(sar, sra)
I64_SHIFTOP(shr, srl)
#undef I64_SHIFTOP

void LiftoffAssembler::emit_i64_shli(LiftoffRegister dst, LiftoffRegister src,
                                     int amount) {
  if (is_uint6(amount)) {
    slli(dst.gp(), src.gp(), amount);
  } else {
    li(kScratchReg, amount);
    sll(dst.gp(), src.gp(), kScratchReg);
  }
}

void LiftoffAssembler::emit_i64_sari(LiftoffRegister dst, LiftoffRegister src,
                                     int amount) {
  if (is_uint6(amount)) {
    srai(dst.gp(), src.gp(), amount);
  } else {
    li(kScratchReg, amount);
    sra(dst.gp(), src.gp(), kScratchReg);
  }
}

void LiftoffAssembler::emit_i64_shri(LiftoffRegister dst, LiftoffRegister src,
                                     int amount) {
  if (is_uint6(amount)) {
    srli(dst.gp(), src.gp(), amount);
  } else {
    li(kScratchReg, amount);
    srl(dst.gp(), src.gp(), kScratchReg);
  }
}

void LiftoffAssembler::emit_i64_addi(LiftoffRegister dst, LiftoffRegister lhs,
                                     int64_t imm) {
  TurboAssembler::Add64(dst.gp(), lhs.gp(), Operand(imm));
}
void LiftoffAssembler::emit_u32_to_uintptr(Register dst, Register src) {
  addw(dst, src, zero_reg);
}

void LiftoffAssembler::emit_f32_neg(DoubleRegister dst, DoubleRegister src) {
  TurboAssembler::Neg_s(dst, src);
}

void LiftoffAssembler::emit_f64_neg(DoubleRegister dst, DoubleRegister src) {
  TurboAssembler::Neg_d(dst, src);
}

void LiftoffAssembler::emit_f32_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  TurboAssembler::Float32Min(dst, lhs, rhs);
}

void LiftoffAssembler::emit_f32_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  TurboAssembler::Float32Max(dst, lhs, rhs);
}

void LiftoffAssembler::emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  fsgnj_s(dst, lhs, rhs);
}

void LiftoffAssembler::emit_f64_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  TurboAssembler::Float64Min(dst, lhs, rhs);
}

void LiftoffAssembler::emit_f64_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  TurboAssembler::Float64Max(dst, lhs, rhs);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  fsgnj_d(dst, lhs, rhs);
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
    instruction(dst, src, kScratchDoubleReg);                                  \
    return true;                                                               \
  }

FP_BINOP(f32_add, fadd_s)
FP_BINOP(f32_sub, fsub_s)
FP_BINOP(f32_mul, fmul_s)
FP_BINOP(f32_div, fdiv_s)
FP_UNOP(f32_abs, fabs_s)
FP_UNOP_RETURN_TRUE(f32_ceil, Ceil_s_s)
FP_UNOP_RETURN_TRUE(f32_floor, Floor_s_s)
FP_UNOP_RETURN_TRUE(f32_trunc, Trunc_s_s)
FP_UNOP_RETURN_TRUE(f32_nearest_int, Round_s_s)
FP_UNOP(f32_sqrt, fsqrt_s)
FP_BINOP(f64_add, fadd_d)
FP_BINOP(f64_sub, fsub_d)
FP_BINOP(f64_mul, fmul_d)
FP_BINOP(f64_div, fdiv_d)
FP_UNOP(f64_abs, fabs_d)
FP_UNOP_RETURN_TRUE(f64_ceil, Ceil_d_d)
FP_UNOP_RETURN_TRUE(f64_floor, Floor_d_d)
FP_UNOP_RETURN_TRUE(f64_trunc, Trunc_d_d)
FP_UNOP_RETURN_TRUE(f64_nearest_int, Round_d_d)
FP_UNOP(f64_sqrt, fsqrt_d)

#undef FP_BINOP
#undef FP_UNOP
#undef FP_UNOP_RETURN_TRUE

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      // According to WebAssembly spec, if I64 value does not fit the range of
      // I32, the value is undefined. Therefore, We use sign extension to
      // implement I64 to I32 truncation
      TurboAssembler::SignExtendWord(dst.gp(), src.gp());
      return true;
    case kExprI32SConvertF32:
    case kExprI32UConvertF32:
    case kExprI32SConvertF64:
    case kExprI32UConvertF64:
    case kExprI64SConvertF32:
    case kExprI64UConvertF32:
    case kExprI64SConvertF64:
    case kExprI64UConvertF64:
    case kExprF32ConvertF64: {
      // real conversion, if src is out-of-bound of target integer types,
      // kScratchReg is set to 0
      switch (opcode) {
        case kExprI32SConvertF32:
          Trunc_w_s(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprI32UConvertF32:
          Trunc_uw_s(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprI32SConvertF64:
          Trunc_w_d(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprI32UConvertF64:
          Trunc_uw_d(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprI64SConvertF32:
          Trunc_l_s(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprI64UConvertF32:
          Trunc_ul_s(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprI64SConvertF64:
          Trunc_l_d(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprI64UConvertF64:
          Trunc_ul_d(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprF32ConvertF64:
          fcvt_s_d(dst.fp(), src.fp());
          break;
        default:
          UNREACHABLE();
      }

      // Checking if trap.
      if (trap != nullptr) {
        TurboAssembler::Branch(trap, eq, kScratchReg, Operand(zero_reg));
      }

      return true;
    }
    case kExprI32ReinterpretF32:
      TurboAssembler::ExtractLowWordFromF64(dst.gp(), src.fp());
      return true;
    case kExprI64SConvertI32:
      TurboAssembler::SignExtendWord(dst.gp(), src.gp());
      return true;
    case kExprI64UConvertI32:
      TurboAssembler::ZeroExtendWord(dst.gp(), src.gp());
      return true;
    case kExprI64ReinterpretF64:
      fmv_x_d(dst.gp(), src.fp());
      return true;
    case kExprF32SConvertI32: {
      TurboAssembler::Cvt_s_w(dst.fp(), src.gp());
      return true;
    }
    case kExprF32UConvertI32:
      TurboAssembler::Cvt_s_uw(dst.fp(), src.gp());
      return true;
    case kExprF32ReinterpretI32:
      fmv_w_x(dst.fp(), src.gp());
      return true;
    case kExprF64SConvertI32: {
      TurboAssembler::Cvt_d_w(dst.fp(), src.gp());
      return true;
    }
    case kExprF64UConvertI32:
      TurboAssembler::Cvt_d_uw(dst.fp(), src.gp());
      return true;
    case kExprF64ConvertF32:
      fcvt_d_s(dst.fp(), src.fp());
      return true;
    case kExprF64ReinterpretI64:
      fmv_d_x(dst.fp(), src.gp());
      return true;
    case kExprI32SConvertSatF32: {
      fcvt_w_s(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_s(dst.gp(), src.fp());
      return true;
    }
    case kExprI32UConvertSatF32: {
      fcvt_wu_s(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_s(dst.gp(), src.fp());
      return true;
    }
    case kExprI32SConvertSatF64: {
      fcvt_w_d(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_d(dst.gp(), src.fp());
      return true;
    }
    case kExprI32UConvertSatF64: {
      fcvt_wu_d(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_d(dst.gp(), src.fp());
      return true;
    }
    case kExprI64SConvertSatF32: {
      fcvt_l_s(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_s(dst.gp(), src.fp());
      return true;
    }
    case kExprI64UConvertSatF32: {
      fcvt_lu_s(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_s(dst.gp(), src.fp());
      return true;
    }
    case kExprI64SConvertSatF64: {
      fcvt_l_d(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_d(dst.gp(), src.fp());
      return true;
    }
    case kExprI64UConvertSatF64: {
      fcvt_lu_d(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_d(dst.gp(), src.fp());
      return true;
    }
    default:
      return false;
  }
}

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  slliw(dst, src, 32 - 8);
  sraiw(dst, dst, 32 - 8);
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  slliw(dst, src, 32 - 16);
  sraiw(dst, dst, 32 - 16);
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  slli(dst.gp(), src.gp(), 64 - 8);
  srai(dst.gp(), dst.gp(), 64 - 8);
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  slli(dst.gp(), src.gp(), 64 - 16);
  srai(dst.gp(), dst.gp(), 64 - 16);
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  slli(dst.gp(), src.gp(), 64 - 32);
  srai(dst.gp(), dst.gp(), 64 - 32);
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
    DCHECK(kind == kI32 || kind == kI64);
    TurboAssembler::Branch(label, cond, lhs, Operand(zero_reg));
  } else {
    DCHECK((kind == kI32 || kind == kI64) ||
           (is_reference(kind) &&
            (liftoff_cond == kEqual || liftoff_cond == kUnequal)));
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
  Sub64(value, value, Operand(subtrahend));
  TurboAssembler::Branch(result_negative, lt, value, Operand(zero_reg));
}

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  TurboAssembler::Sltu(dst, src, 1);
}

void LiftoffAssembler::emit_i32_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, Register lhs,
                                         Register rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  TurboAssembler::CompareI(dst, lhs, Operand(rhs), cond);
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  TurboAssembler::Sltu(dst, src.gp(), 1);
}

void LiftoffAssembler::emit_i64_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  TurboAssembler::CompareI(dst, lhs.gp(), Operand(rhs.gp()), cond);
}

static FPUCondition ConditionToConditionCmpFPU(LiftoffCondition condition) {
  switch (condition) {
    case kEqual:
      return EQ;
    case kUnequal:
      return NE;
    case kUnsignedLessThan:
      return LT;
    case kUnsignedGreaterEqual:
      return GE;
    case kUnsignedLessEqual:
      return LE;
    case kUnsignedGreaterThan:
      return GT;
    default:
      break;
  }
  UNREACHABLE();
}

void LiftoffAssembler::emit_f32_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  FPUCondition fcond = ConditionToConditionCmpFPU(liftoff_cond);
  TurboAssembler::CompareF32(dst, fcond, lhs, rhs);
}

void LiftoffAssembler::emit_f64_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  FPUCondition fcond = ConditionToConditionCmpFPU(liftoff_cond);
  TurboAssembler::CompareF64(dst, fcond, lhs, rhs);
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

void LiftoffAssembler::IncrementSmi(LiftoffRegister dst, int offset) {
  UseScratchRegisterScope temps(this);
  if (COMPRESS_POINTERS_BOOL) {
    DCHECK(SmiValuesAre31Bits());
    Register scratch = temps.Acquire();
    Lw(scratch, MemOperand(dst.gp(), offset));
    Add32(scratch, scratch, Operand(Smi::FromInt(1)));
    Sw(scratch, MemOperand(dst.gp(), offset));
  } else {
    Register scratch = temps.Acquire();
    SmiUntag(scratch, MemOperand(dst.gp(), offset));
    Add64(scratch, scratch, Operand(1));
    SmiTag(scratch);
    Sd(scratch, MemOperand(dst.gp(), offset));
  }
}

void LiftoffAssembler::LoadTransform(LiftoffRegister dst, Register src_addr,
                                     Register offset_reg, uintptr_t offset_imm,
                                     LoadType type,
                                     LoadTransformationKind transform,
                                     uint32_t* protected_load_pc) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MemOperand src_op = liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm);
  VRegister dst_v = dst.fp().toV();
  *protected_load_pc = pc_offset();

  MachineType memtype = type.mem_type();
  if (transform == LoadTransformationKind::kExtend) {
    Ld(scratch, src_op);
    if (memtype == MachineType::Int8()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      VU.set(kScratchReg, E16, m1);
      vsext_vf2(dst_v, kSimd128ScratchReg);
    } else if (memtype == MachineType::Uint8()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      VU.set(kScratchReg, E16, m1);
      vzext_vf2(dst_v, kSimd128ScratchReg);
    } else if (memtype == MachineType::Int16()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      VU.set(kScratchReg, E32, m1);
      vsext_vf2(dst_v, kSimd128ScratchReg);
    } else if (memtype == MachineType::Uint16()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      VU.set(kScratchReg, E32, m1);
      vzext_vf2(dst_v, kSimd128ScratchReg);
    } else if (memtype == MachineType::Int32()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      vsext_vf2(dst_v, kSimd128ScratchReg);
    } else if (memtype == MachineType::Uint32()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      vzext_vf2(dst_v, kSimd128ScratchReg);
    }
  } else if (transform == LoadTransformationKind::kZeroExtend) {
    vxor_vv(dst_v, dst_v, dst_v);
    if (memtype == MachineType::Int32()) {
      VU.set(kScratchReg, E32, m1);
      Lwu(scratch, src_op);
      vmv_sx(dst_v, scratch);
    } else {
      DCHECK_EQ(MachineType::Int64(), memtype);
      VU.set(kScratchReg, E64, m1);
      Ld(scratch, src_op);
      vmv_sx(dst_v, scratch);
    }
  } else {
    DCHECK_EQ(LoadTransformationKind::kSplat, transform);
    if (memtype == MachineType::Int8()) {
      VU.set(kScratchReg, E8, m1);
      Lb(scratch, src_op);
      vmv_vx(dst_v, scratch);
    } else if (memtype == MachineType::Int16()) {
      VU.set(kScratchReg, E16, m1);
      Lh(scratch, src_op);
      vmv_vx(dst_v, scratch);
    } else if (memtype == MachineType::Int32()) {
      VU.set(kScratchReg, E32, m1);
      Lw(scratch, src_op);
      vmv_vx(dst_v, scratch);
    } else if (memtype == MachineType::Int64()) {
      VU.set(kScratchReg, E64, m1);
      Ld(scratch, src_op);
      vmv_vx(dst_v, scratch);
    }
  }
}

void LiftoffAssembler::LoadLane(LiftoffRegister dst, LiftoffRegister src,
                                Register addr, Register offset_reg,
                                uintptr_t offset_imm, LoadType type,
                                uint8_t laneidx, uint32_t* protected_load_pc) {
  MemOperand src_op = liftoff::GetMemOp(this, addr, offset_reg, offset_imm);
  MachineType mem_type = type.mem_type();
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  *protected_load_pc = pc_offset();
  if (mem_type == MachineType::Int8()) {
    Lbu(scratch, src_op);
    VU.set(kScratchReg, E64, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    VU.set(kScratchReg, E8, m1);
    vmerge_vx(dst.fp().toV(), scratch, dst.fp().toV());
  } else if (mem_type == MachineType::Int16()) {
    Lhu(scratch, src_op);
    VU.set(kScratchReg, E16, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    vmerge_vx(dst.fp().toV(), scratch, dst.fp().toV());
  } else if (mem_type == MachineType::Int32()) {
    Lwu(scratch, src_op);
    VU.set(kScratchReg, E32, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    vmerge_vx(dst.fp().toV(), scratch, dst.fp().toV());
  } else if (mem_type == MachineType::Int64()) {
    Ld(scratch, src_op);
    VU.set(kScratchReg, E64, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    vmerge_vx(dst.fp().toV(), scratch, dst.fp().toV());
  } else {
    UNREACHABLE();
  }
}

void LiftoffAssembler::StoreLane(Register dst, Register offset,
                                 uintptr_t offset_imm, LiftoffRegister src,
                                 StoreType type, uint8_t lane,
                                 uint32_t* protected_store_pc) {
  MemOperand dst_op = liftoff::GetMemOp(this, dst, offset, offset_imm);
  if (protected_store_pc) *protected_store_pc = pc_offset();
  MachineRepresentation rep = type.mem_rep();
  if (rep == MachineRepresentation::kWord8) {
    VU.set(kScratchReg, E8, m1);
    vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), lane);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sb(kScratchReg, dst_op);
  } else if (rep == MachineRepresentation::kWord16) {
    VU.set(kScratchReg, E16, m1);
    vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), lane);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sh(kScratchReg, dst_op);
  } else if (rep == MachineRepresentation::kWord32) {
    VU.set(kScratchReg, E32, m1);
    vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), lane);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sw(kScratchReg, dst_op);
  } else {
    DCHECK_EQ(MachineRepresentation::kWord64, rep);
    VU.set(kScratchReg, E64, m1);
    vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), lane);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sd(kScratchReg, dst_op);
  }
}

void LiftoffAssembler::emit_i8x16_shuffle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs,
                                          const uint8_t shuffle[16],
                                          bool is_swizzle) {
  VRegister dst_v = dst.fp().toV();
  VRegister lhs_v = lhs.fp().toV();
  VRegister rhs_v = rhs.fp().toV();

  uint64_t imm1 = *(reinterpret_cast<const uint64_t*>(shuffle));
  uint64_t imm2 = *((reinterpret_cast<const uint64_t*>(shuffle)) + 1);
  VU.set(kScratchReg, VSew::E64, Vlmul::m1);
  li(kScratchReg, imm2);
  vmv_sx(kSimd128ScratchReg2, kScratchReg);
  vslideup_vi(kSimd128ScratchReg, kSimd128ScratchReg2, 1);
  li(kScratchReg, imm1);
  vmv_sx(kSimd128ScratchReg, kScratchReg);

  VU.set(kScratchReg, E8, m1);
  VRegister temp =
      GetUnusedRegister(kFpReg, LiftoffRegList{lhs, rhs}).fp().toV();
  if (dst_v == lhs_v) {
    vmv_vv(temp, lhs_v);
    lhs_v = temp;
  } else if (dst_v == rhs_v) {
    vmv_vv(temp, rhs_v);
    rhs_v = temp;
  }
  vrgather_vv(dst_v, lhs_v, kSimd128ScratchReg);
  vadd_vi(kSimd128ScratchReg, kSimd128ScratchReg,
          -16);  // The indices in range [16, 31] select the i - 16-th element
                 // of rhs
  vrgather_vv(kSimd128ScratchReg2, rhs_v, kSimd128ScratchReg);
  vor_vv(dst_v, dst_v, kSimd128ScratchReg2);
}

void LiftoffAssembler::emit_i8x16_popcnt(LiftoffRegister dst,
                                         LiftoffRegister src) {
  VRegister src_v = src.fp().toV();
  VRegister dst_v = dst.fp().toV();
  Label t;

  VU.set(kScratchReg, E8, m1);
  vmv_vv(kSimd128ScratchReg, src_v);
  vmv_vv(dst_v, kSimd128RegZero);

  bind(&t);
  vmsne_vv(v0, kSimd128ScratchReg, kSimd128RegZero);
  vadd_vi(dst_v, dst_v, 1, Mask);
  vadd_vi(kSimd128ScratchReg2, kSimd128ScratchReg, -1, Mask);
  vand_vv(kSimd128ScratchReg, kSimd128ScratchReg, kSimd128ScratchReg2);
  // kScratchReg = -1 if kSimd128ScratchReg == 0 i.e. no active element
  vfirst_m(kScratchReg, kSimd128ScratchReg);
  bgez(kScratchReg, &t);
}

void LiftoffAssembler::emit_i8x16_swizzle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  if (dst == lhs) {
    vrgather_vv(kSimd128ScratchReg, lhs.fp().toV(), rhs.fp().toV());
    vmv_vv(dst.fp().toV(), kSimd128ScratchReg);
  } else {
    vrgather_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
  }
}

void LiftoffAssembler::emit_i8x16_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VU.set(kScratchReg, E8, m1);
  vmv_vx(dst.fp().toV(), src.gp());
}

void LiftoffAssembler::emit_i16x8_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VU.set(kScratchReg, E16, m1);
  vmv_vx(dst.fp().toV(), src.gp());
}

void LiftoffAssembler::emit_i32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  vmv_vx(dst.fp().toV(), src.gp());
}

void LiftoffAssembler::emit_i64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  vmv_vx(dst.fp().toV(), src.gp());
}

void LiftoffAssembler::emit_i64x2_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  WasmRvvEq(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E64, m1);
}

void LiftoffAssembler::emit_i64x2_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  WasmRvvNe(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E64, m1);
}

void LiftoffAssembler::emit_i64x2_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGtS(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E64, m1);
}

void LiftoffAssembler::emit_i64x2_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGeS(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E64, m1);
}

void LiftoffAssembler::emit_f32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  fmv_x_w(kScratchReg, src.fp());
  vmv_vx(dst.fp().toV(), kScratchReg);
}

void LiftoffAssembler::emit_f64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  fmv_x_d(kScratchReg, src.fp());
  vmv_vx(dst.fp().toV(), kScratchReg);
}

void LiftoffAssembler::emit_i64x2_extmul_low_i32x4_s(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  VU.set(kScratchReg, E32, mf2);
  VRegister dst_v = dst.fp().toV();
  if (dst == src1 || dst == src2) {
    dst_v = kSimd128ScratchReg3;
  }
  vwmul_vv(dst_v, src2.fp().toV(), src1.fp().toV());
  if (dst == src1 || dst == src2) {
    VU.set(kScratchReg, E64, m1);
    vmv_vv(dst.fp().toV(), dst_v);
  }
}

void LiftoffAssembler::emit_i64x2_extmul_low_i32x4_u(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  VU.set(kScratchReg, E32, mf2);
  VRegister dst_v = dst.fp().toV();
  if (dst == src1 || dst == src2) {
    dst_v = kSimd128ScratchReg3;
  }
  vwmulu_vv(dst_v, src2.fp().toV(), src1.fp().toV());
  if (dst == src1 || dst == src2) {
    VU.set(kScratchReg, E64, m1);
    vmv_vv(dst.fp().toV(), dst_v);
  }
}

void LiftoffAssembler::emit_i64x2_extmul_high_i32x4_s(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  VU.set(kScratchReg, E32, m1);
  vslidedown_vi(kSimd128ScratchReg, src1.fp().toV(), 2);
  vslidedown_vi(kSimd128ScratchReg2, src2.fp().toV(), 2);
  VU.set(kScratchReg, E32, mf2);
  vwmul_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
}

void LiftoffAssembler::emit_i64x2_extmul_high_i32x4_u(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  VU.set(kScratchReg, E32, m1);
  vslidedown_vi(kSimd128ScratchReg, src1.fp().toV(), 2);
  vslidedown_vi(kSimd128ScratchReg2, src2.fp().toV(), 2);
  VU.set(kScratchReg, E32, mf2);
  vwmulu_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
}

void LiftoffAssembler::emit_i32x4_extmul_low_i16x8_s(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  VU.set(kScratchReg, E16, mf2);
  VRegister dst_v = dst.fp().toV();
  if (dst == src1 || dst == src2) {
    dst_v = kSimd128ScratchReg3;
  }
  vwmul_vv(dst_v, src2.fp().toV(), src1.fp().toV());
  if (dst == src1 || dst == src2) {
    VU.set(kScratchReg, E16, m1);
    vmv_vv(dst.fp().toV(), dst_v);
  }
}

void LiftoffAssembler::emit_i32x4_extmul_low_i16x8_u(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  VU.set(kScratchReg, E16, mf2);
  VRegister dst_v = dst.fp().toV();
  if (dst == src1 || dst == src2) {
    dst_v = kSimd128ScratchReg3;
  }
  vwmulu_vv(dst_v, src2.fp().toV(), src1.fp().toV());
  if (dst == src1 || dst == src2) {
    VU.set(kScratchReg, E16, m1);
    vmv_vv(dst.fp().toV(), dst_v);
  }
}

void LiftoffAssembler::emit_i32x4_extmul_high_i16x8_s(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  VU.set(kScratchReg, E16, m1);
  vslidedown_vi(kSimd128ScratchReg, src1.fp().toV(), 4);
  vslidedown_vi(kSimd128ScratchReg2, src2.fp().toV(), 4);
  VU.set(kScratchReg, E16, mf2);
  vwmul_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
}

void LiftoffAssembler::emit_i32x4_extmul_high_i16x8_u(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  VU.set(kScratchReg, E16, m1);
  vslidedown_vi(kSimd128ScratchReg, src1.fp().toV(), 4);
  vslidedown_vi(kSimd128ScratchReg2, src2.fp().toV(), 4);
  VU.set(kScratchReg, E16, mf2);
  vwmulu_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
}

void LiftoffAssembler::emit_i16x8_extmul_low_i8x16_s(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  VU.set(kScratchReg, E8, mf2);
  VRegister dst_v = dst.fp().toV();
  if (dst == src1 || dst == src2) {
    dst_v = kSimd128ScratchReg3;
  }
  vwmul_vv(dst_v, src2.fp().toV(), src1.fp().toV());
  if (dst == src1 || dst == src2) {
    VU.set(kScratchReg, E8, m1);
    vmv_vv(dst.fp().toV(), dst_v);
  }
}

void LiftoffAssembler::emit_i16x8_extmul_low_i8x16_u(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  VU.set(kScratchReg, E8, mf2);
  VRegister dst_v = dst.fp().toV();
  if (dst == src1 || dst == src2) {
    dst_v = kSimd128ScratchReg3;
  }
  vwmulu_vv(dst_v, src2.fp().toV(), src1.fp().toV());
  if (dst == src1 || dst == src2) {
    VU.set(kScratchReg, E8, m1);
    vmv_vv(dst.fp().toV(), dst_v);
  }
}

void LiftoffAssembler::emit_i16x8_extmul_high_i8x16_s(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  VU.set(kScratchReg, E8, m1);
  vslidedown_vi(kSimd128ScratchReg, src1.fp().toV(), 8);
  vslidedown_vi(kSimd128ScratchReg2, src2.fp().toV(), 8);
  VU.set(kScratchReg, E8, mf2);
  vwmul_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
}

void LiftoffAssembler::emit_i16x8_extmul_high_i8x16_u(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  VU.set(kScratchReg, E8, m1);
  vslidedown_vi(kSimd128ScratchReg, src1.fp().toV(), 8);
  vslidedown_vi(kSimd128ScratchReg2, src2.fp().toV(), 8);
  VU.set(kScratchReg, E8, mf2);
  vwmulu_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
}

#undef SIMD_BINOP

void LiftoffAssembler::emit_i16x8_q15mulr_sat_s(LiftoffRegister dst,
                                                LiftoffRegister src1,
                                                LiftoffRegister src2) {
  VU.set(kScratchReg, E16, m1);
  vsmul_vv(dst.fp().toV(), src1.fp().toV(), src2.fp().toV());
}

void LiftoffAssembler::emit_i64x2_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  vmv_vx(kSimd128RegZero, zero_reg);
  vmslt_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128RegZero);
  VU.set(kScratchReg, E32, m1);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i64x2_sconvert_i32x4_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  vmv_vv(kSimd128ScratchReg, src.fp().toV());
  vsext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i64x2_sconvert_i32x4_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), 2);
  VU.set(kScratchReg, E64, m1);
  vsext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i64x2_uconvert_i32x4_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  vmv_vv(kSimd128ScratchReg, src.fp().toV());
  vzext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i64x2_uconvert_i32x4_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), 2);
  VU.set(kScratchReg, E64, m1);
  vzext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i8x16_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  WasmRvvEq(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E8, m1);
}

void LiftoffAssembler::emit_i8x16_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  WasmRvvNe(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E8, m1);
}

void LiftoffAssembler::emit_i8x16_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGtS(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E8, m1);
}

void LiftoffAssembler::emit_i8x16_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGtU(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E8, m1);
}

void LiftoffAssembler::emit_i8x16_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGeS(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E8, m1);
}

void LiftoffAssembler::emit_i8x16_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGeU(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E8, m1);
}

void LiftoffAssembler::emit_i16x8_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  WasmRvvEq(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E16, m1);
}

void LiftoffAssembler::emit_i16x8_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  WasmRvvNe(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E16, m1);
}

void LiftoffAssembler::emit_i16x8_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGtS(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E16, m1);
}

void LiftoffAssembler::emit_i16x8_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGtU(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E16, m1);
}

void LiftoffAssembler::emit_i16x8_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGeS(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E16, m1);
}

void LiftoffAssembler::emit_i16x8_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGeU(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E16, m1);
}

void LiftoffAssembler::emit_i32x4_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  WasmRvvEq(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E32, m1);
}

void LiftoffAssembler::emit_i32x4_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  WasmRvvNe(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E32, m1);
}

void LiftoffAssembler::emit_i32x4_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGtS(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E32, m1);
}

void LiftoffAssembler::emit_i32x4_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGtU(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E32, m1);
}

void LiftoffAssembler::emit_i32x4_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGeS(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E32, m1);
}

void LiftoffAssembler::emit_i32x4_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  WasmRvvGeU(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV(), E32, m1);
}

void LiftoffAssembler::emit_f32x4_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vmfeq_vv(v0, rhs.fp().toV(), lhs.fp().toV());
  vmv_vx(dst.fp().toV(), zero_reg);
  vmerge_vi(dst.fp().toV(), -1, dst.fp().toV());
}

void LiftoffAssembler::emit_f32x4_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vmfne_vv(v0, rhs.fp().toV(), lhs.fp().toV());
  vmv_vx(dst.fp().toV(), zero_reg);
  vmerge_vi(dst.fp().toV(), -1, dst.fp().toV());
}

void LiftoffAssembler::emit_f32x4_lt(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vmflt_vv(v0, lhs.fp().toV(), rhs.fp().toV());
  vmv_vx(dst.fp().toV(), zero_reg);
  vmerge_vi(dst.fp().toV(), -1, dst.fp().toV());
}

void LiftoffAssembler::emit_f32x4_le(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vmfle_vv(v0, lhs.fp().toV(), rhs.fp().toV());
  vmv_vx(dst.fp().toV(), zero_reg);
  vmerge_vi(dst.fp().toV(), -1, dst.fp().toV());
}

void LiftoffAssembler::emit_f64x2_convert_low_i32x4_s(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  VU.set(kScratchReg, E32, mf2);
  if (dst.fp().toV() != src.fp().toV()) {
    vfwcvt_f_x_v(dst.fp().toV(), src.fp().toV());
  } else {
    vfwcvt_f_x_v(kSimd128ScratchReg3, src.fp().toV());
    VU.set(kScratchReg, E64, m1);
    vmv_vv(dst.fp().toV(), kSimd128ScratchReg3);
  }
}

void LiftoffAssembler::emit_f64x2_convert_low_i32x4_u(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  VU.set(kScratchReg, E32, mf2);
  if (dst.fp().toV() != src.fp().toV()) {
    vfwcvt_f_xu_v(dst.fp().toV(), src.fp().toV());
  } else {
    vfwcvt_f_xu_v(kSimd128ScratchReg3, src.fp().toV());
    VU.set(kScratchReg, E64, m1);
    vmv_vv(dst.fp().toV(), kSimd128ScratchReg3);
  }
}

void LiftoffAssembler::emit_f64x2_promote_low_f32x4(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  VU.set(kScratchReg, E32, mf2);
  if (dst.fp().toV() != src.fp().toV()) {
    vfwcvt_f_f_v(dst.fp().toV(), src.fp().toV());
  } else {
    vfwcvt_f_f_v(kSimd128ScratchReg3, src.fp().toV());
    VU.set(kScratchReg, E64, m1);
    vmv_vv(dst.fp().toV(), kSimd128ScratchReg3);
  }
}

void LiftoffAssembler::emit_f32x4_demote_f64x2_zero(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  VU.set(kScratchReg, E32, mf2);
  vfncvt_f_f_w(dst.fp().toV(), src.fp().toV());
  VU.set(kScratchReg, E32, m1);
  vmv_vi(v0, 12);
  vmerge_vx(dst.fp().toV(), zero_reg, dst.fp().toV());
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_s_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  vmv_vx(kSimd128ScratchReg, zero_reg);
  vmfeq_vv(v0, src.fp().toV(), src.fp().toV());
  vmv_vv(kSimd128ScratchReg3, src.fp().toV());
  VU.set(kScratchReg, E32, m1);
  VU.set(RoundingMode::RTZ);
  vfncvt_x_f_w(kSimd128ScratchReg, kSimd128ScratchReg3, MaskType::Mask);
  vmv_vv(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_u_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  vmv_vx(kSimd128ScratchReg, zero_reg);
  vmfeq_vv(v0, src.fp().toV(), src.fp().toV());
  vmv_vv(kSimd128ScratchReg3, src.fp().toV());
  VU.set(kScratchReg, E32, m1);
  VU.set(RoundingMode::RTZ);
  vfncvt_xu_f_w(kSimd128ScratchReg, kSimd128ScratchReg3, MaskType::Mask);
  vmv_vv(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_f64x2_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  vmfeq_vv(v0, rhs.fp().toV(), lhs.fp().toV());
  vmv_vx(dst.fp().toV(), zero_reg);
  vmerge_vi(dst.fp().toV(), -1, dst.fp().toV());
}

void LiftoffAssembler::emit_f64x2_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  vmfne_vv(v0, rhs.fp().toV(), lhs.fp().toV());
  vmv_vx(dst.fp().toV(), zero_reg);
  vmerge_vi(dst.fp().toV(), -1, dst.fp().toV());
}

void LiftoffAssembler::emit_f64x2_lt(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  vmflt_vv(v0, lhs.fp().toV(), rhs.fp().toV());
  vmv_vx(dst.fp().toV(), zero_reg);
  vmerge_vi(dst.fp().toV(), -1, dst.fp().toV());
}

void LiftoffAssembler::emit_f64x2_le(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  vmfle_vv(v0, lhs.fp().toV(), rhs.fp().toV());
  vmv_vx(dst.fp().toV(), zero_reg);
  vmerge_vi(dst.fp().toV(), -1, dst.fp().toV());
}

void LiftoffAssembler::emit_s128_const(LiftoffRegister dst,
                                       const uint8_t imms[16]) {
  WasmRvvS128const(dst.fp().toV(), imms);
}

void LiftoffAssembler::emit_s128_not(LiftoffRegister dst, LiftoffRegister src) {
  VU.set(kScratchReg, E8, m1);
  vnot_vv(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_s128_and(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vand_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_s128_or(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vor_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_s128_xor(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vxor_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_s128_and_not(LiftoffRegister dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vnot_vv(dst.fp().toV(), rhs.fp().toV());
  vand_vv(dst.fp().toV(), lhs.fp().toV(), dst.fp().toV());
}

void LiftoffAssembler::emit_s128_select(LiftoffRegister dst,
                                        LiftoffRegister src1,
                                        LiftoffRegister src2,
                                        LiftoffRegister mask) {
  VU.set(kScratchReg, E8, m1);
  vand_vv(kSimd128ScratchReg, src1.fp().toV(), mask.fp().toV());
  vnot_vv(kSimd128ScratchReg2, mask.fp().toV());
  vand_vv(kSimd128ScratchReg2, src2.fp().toV(), kSimd128ScratchReg2);
  vor_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
}

void LiftoffAssembler::emit_i8x16_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E8, m1);
  vneg_vv(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_v128_anytrue(LiftoffRegister dst,
                                         LiftoffRegister src) {
  VU.set(kScratchReg, E8, m1);
  Label t;
  vmv_sx(kSimd128ScratchReg, zero_reg);
  vredmaxu_vs(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
  beq(dst.gp(), zero_reg, &t);
  li(dst.gp(), 1);
  bind(&t);
}

void LiftoffAssembler::emit_i8x16_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VU.set(kScratchReg, E8, m1);
  Label alltrue;
  li(kScratchReg, -1);
  vmv_sx(kSimd128ScratchReg, kScratchReg);
  vredminu_vs(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
  beqz(dst.gp(), &alltrue);
  li(dst.gp(), 1);
  bind(&alltrue);
}

void LiftoffAssembler::emit_i8x16_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VU.set(kScratchReg, E8, m1);
  vmv_vx(kSimd128RegZero, zero_reg);
  vmslt_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128RegZero);
  VU.set(kScratchReg, E32, m1);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i8x16_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  andi(rhs.gp(), rhs.gp(), 8 - 1);
  vsll_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i8x16_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  DCHECK(is_uint5(rhs));
  VU.set(kScratchReg, E8, m1);
  vsll_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 8);
}

void LiftoffAssembler::emit_i8x16_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  andi(rhs.gp(), rhs.gp(), 8 - 1);
  vsra_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i8x16_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  VU.set(kScratchReg, E8, m1);
  vsra_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 8);
}

void LiftoffAssembler::emit_i8x16_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  andi(rhs.gp(), rhs.gp(), 8 - 1);
  vsrl_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i8x16_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  VU.set(kScratchReg, E8, m1);
  vsrl_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 8);
}

void LiftoffAssembler::emit_i8x16_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vadd_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_add_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vsadd_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_add_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vsaddu_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vsub_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_sub_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vssub_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_sub_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vssubu_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vmin_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vminu_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vmax_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vmaxu_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E16, m1);
  vneg_vv(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i16x8_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VU.set(kScratchReg, E16, m1);
  Label alltrue;
  li(kScratchReg, -1);
  vmv_sx(kSimd128ScratchReg, kScratchReg);
  vredminu_vs(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
  beqz(dst.gp(), &alltrue);
  li(dst.gp(), 1);
  bind(&alltrue);
}

void LiftoffAssembler::emit_i16x8_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VU.set(kScratchReg, E16, m1);
  vmv_vx(kSimd128RegZero, zero_reg);
  vmslt_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128RegZero);
  VU.set(kScratchReg, E32, m1);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i16x8_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  andi(rhs.gp(), rhs.gp(), 16 - 1);
  vsll_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i16x8_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  VU.set(kScratchReg, E16, m1);
  vsll_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 16);
}

void LiftoffAssembler::emit_i16x8_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  andi(rhs.gp(), rhs.gp(), 16 - 1);
  vsra_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i16x8_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  VU.set(kScratchReg, E16, m1);
  vsra_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 16);
}

void LiftoffAssembler::emit_i16x8_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  andi(rhs.gp(), rhs.gp(), 16 - 1);
  vsrl_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i16x8_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  DCHECK(is_uint5(rhs));
  VU.set(kScratchReg, E16, m1);
  vsrl_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 16);
}

void LiftoffAssembler::emit_i16x8_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vadd_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_add_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vsadd_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_add_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vsaddu_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vsub_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_sub_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vssub_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_sub_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vssubu_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vmul_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vmin_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vminu_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vmax_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vmaxu_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  vneg_vv(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i32x4_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  Label alltrue;
  li(kScratchReg, -1);
  vmv_sx(kSimd128ScratchReg, kScratchReg);
  vredminu_vs(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
  beqz(dst.gp(), &alltrue);
  li(dst.gp(), 1);
  bind(&alltrue);
}

void LiftoffAssembler::emit_i32x4_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  vmv_vx(kSimd128RegZero, zero_reg);
  vmslt_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128RegZero);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i32x4_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  andi(rhs.gp(), rhs.gp(), 32 - 1);
  vsll_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i32x4_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  if (is_uint5(rhs % 32)) {
    vsll_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 32);
  } else {
    li(kScratchReg, rhs % 32);
    vsll_vx(dst.fp().toV(), lhs.fp().toV(), kScratchReg);
  }
}

void LiftoffAssembler::emit_i32x4_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  andi(rhs.gp(), rhs.gp(), 32 - 1);
  vsra_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i32x4_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  VU.set(kScratchReg, E32, m1);
  if (is_uint5(rhs % 32)) {
    vsra_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 32);
  } else {
    li(kScratchReg, rhs % 32);
    vsra_vx(dst.fp().toV(), lhs.fp().toV(), kScratchReg);
  }
}

void LiftoffAssembler::emit_i32x4_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  andi(rhs.gp(), rhs.gp(), 32 - 1);
  vsrl_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i32x4_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  VU.set(kScratchReg, E32, m1);
  if (is_uint5(rhs % 32)) {
    vsrl_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 32);
  } else {
    li(kScratchReg, rhs % 32);
    vsrl_vx(dst.fp().toV(), lhs.fp().toV(), kScratchReg);
  }
}

void LiftoffAssembler::emit_i32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vadd_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vsub_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vmul_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vmin_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vminu_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vmax_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vmaxu_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_dot_i16x8_s(LiftoffRegister dst,
                                              LiftoffRegister lhs,
                                              LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vwmul_vv(kSimd128ScratchReg3, lhs.fp().toV(), rhs.fp().toV());
  VU.set(kScratchReg, E32, m2);
  li(kScratchReg, 0b01010101);
  vmv_sx(v0, kScratchReg);
  vcompress_vv(kSimd128ScratchReg, kSimd128ScratchReg3, v0);

  li(kScratchReg, 0b10101010);
  vmv_sx(kSimd128ScratchReg2, kScratchReg);
  vcompress_vv(v0, kSimd128ScratchReg3, kSimd128ScratchReg2);
  VU.set(kScratchReg, E32, m1);
  vadd_vv(dst.fp().toV(), kSimd128ScratchReg, v0);
}

void LiftoffAssembler::emit_i64x2_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  vneg_vv(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i64x2_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  Label alltrue;
  li(kScratchReg, -1);
  vmv_sx(kSimd128ScratchReg, kScratchReg);
  vredminu_vs(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
  beqz(dst.gp(), &alltrue);
  li(dst.gp(), 1);
  bind(&alltrue);
}

void LiftoffAssembler::emit_i64x2_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  andi(rhs.gp(), rhs.gp(), 64 - 1);
  vsll_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i64x2_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  VU.set(kScratchReg, E64, m1);
  if (is_uint5(rhs % 64)) {
    vsll_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 64);
  } else {
    li(kScratchReg, rhs % 64);
    vsll_vx(dst.fp().toV(), lhs.fp().toV(), kScratchReg);
  }
}

void LiftoffAssembler::emit_i64x2_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  andi(rhs.gp(), rhs.gp(), 64 - 1);
  vsra_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i64x2_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  VU.set(kScratchReg, E64, m1);
  if (is_uint5(rhs % 64)) {
    vsra_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 64);
  } else {
    li(kScratchReg, rhs % 64);
    vsra_vx(dst.fp().toV(), lhs.fp().toV(), kScratchReg);
  }
}

void LiftoffAssembler::emit_i64x2_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  andi(rhs.gp(), rhs.gp(), 64 - 1);
  vsrl_vx(dst.fp().toV(), lhs.fp().toV(), rhs.gp());
}

void LiftoffAssembler::emit_i64x2_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  VU.set(kScratchReg, E64, m1);
  if (is_uint5(rhs % 64)) {
    vsrl_vi(dst.fp().toV(), lhs.fp().toV(), rhs % 64);
  } else {
    li(kScratchReg, rhs % 64);
    vsrl_vx(dst.fp().toV(), lhs.fp().toV(), kScratchReg);
  }
}

void LiftoffAssembler::emit_i64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  vadd_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  vsub_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  vmul_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  vfabs_vv(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_f32x4_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  vfneg_vv(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_f32x4_sqrt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  vfsqrt_v(dst.fp().toV(), src.fp().toV());
}

bool LiftoffAssembler::emit_f32x4_ceil(LiftoffRegister dst,
                                       LiftoffRegister src) {
  Ceil_f(dst.fp().toV(), src.fp().toV(), kScratchReg, kSimd128ScratchReg);
  return true;
}

bool LiftoffAssembler::emit_f32x4_floor(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Floor_f(dst.fp().toV(), src.fp().toV(), kScratchReg, kSimd128ScratchReg);
  return true;
}

bool LiftoffAssembler::emit_f32x4_trunc(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Trunc_f(dst.fp().toV(), src.fp().toV(), kScratchReg, kSimd128ScratchReg);
  return true;
}

bool LiftoffAssembler::emit_f32x4_nearest_int(LiftoffRegister dst,
                                              LiftoffRegister src) {
  Round_f(dst.fp().toV(), src.fp().toV(), kScratchReg, kSimd128ScratchReg);
  return true;
}

void LiftoffAssembler::emit_f32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vfadd_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vfsub_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  VU.set(RoundingMode::RTZ);
  vfmul_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_div(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vfdiv_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  const int32_t kNaN = 0x7FC00000;
  VU.set(kScratchReg, E32, m1);
  vmfeq_vv(v0, lhs.fp().toV(), lhs.fp().toV());
  vmfeq_vv(kSimd128ScratchReg, rhs.fp().toV(), rhs.fp().toV());
  vand_vv(v0, v0, kSimd128ScratchReg);
  li(kScratchReg, kNaN);
  vmv_vx(kSimd128ScratchReg, kScratchReg);
  vfmin_vv(kSimd128ScratchReg, rhs.fp().toV(), lhs.fp().toV(), Mask);
  vmv_vv(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_f32x4_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  const int32_t kNaN = 0x7FC00000;
  VU.set(kScratchReg, E32, m1);
  vmfeq_vv(v0, lhs.fp().toV(), lhs.fp().toV());
  vmfeq_vv(kSimd128ScratchReg, rhs.fp().toV(), rhs.fp().toV());
  vand_vv(v0, v0, kSimd128ScratchReg);
  li(kScratchReg, kNaN);
  vmv_vx(kSimd128ScratchReg, kScratchReg);
  vfmax_vv(kSimd128ScratchReg, rhs.fp().toV(), lhs.fp().toV(), Mask);
  vmv_vv(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_f32x4_pmin(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  // b < a ? b : a
  vmflt_vv(v0, rhs.fp().toV(), lhs.fp().toV());
  vmerge_vv(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_pmax(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  // a < b ? b : a
  vmflt_vv(v0, lhs.fp().toV(), rhs.fp().toV());
  vmerge_vv(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  vfabs_vv(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_f64x2_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  vfneg_vv(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_f64x2_sqrt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  vfsqrt_v(dst.fp().toV(), src.fp().toV());
}

bool LiftoffAssembler::emit_f64x2_ceil(LiftoffRegister dst,
                                       LiftoffRegister src) {
  Ceil_d(dst.fp().toV(), src.fp().toV(), kScratchReg, kSimd128ScratchReg);
  return true;
}

bool LiftoffAssembler::emit_f64x2_floor(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Floor_d(dst.fp().toV(), src.fp().toV(), kScratchReg, kSimd128ScratchReg);
  return true;
}

bool LiftoffAssembler::emit_f64x2_trunc(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Trunc_d(dst.fp().toV(), src.fp().toV(), kScratchReg, kSimd128ScratchReg);
  return true;
}

bool LiftoffAssembler::emit_f64x2_nearest_int(LiftoffRegister dst,
                                              LiftoffRegister src) {
  Round_d(dst.fp().toV(), src.fp().toV(), kScratchReg, kSimd128ScratchReg);
  return true;
}

void LiftoffAssembler::emit_f64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  vfadd_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  vfsub_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  vfmul_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_div(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  vfdiv_vv(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  const int64_t kNaN = 0x7ff8000000000000L;
  vmfeq_vv(v0, lhs.fp().toV(), lhs.fp().toV());
  vmfeq_vv(kSimd128ScratchReg, rhs.fp().toV(), rhs.fp().toV());
  vand_vv(v0, v0, kSimd128ScratchReg);
  li(kScratchReg, kNaN);
  vmv_vx(kSimd128ScratchReg, kScratchReg);
  vfmin_vv(kSimd128ScratchReg, rhs.fp().toV(), lhs.fp().toV(), Mask);
  vmv_vv(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_f64x2_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  const int64_t kNaN = 0x7ff8000000000000L;
  vmfeq_vv(v0, lhs.fp().toV(), lhs.fp().toV());
  vmfeq_vv(kSimd128ScratchReg, rhs.fp().toV(), rhs.fp().toV());
  vand_vv(v0, v0, kSimd128ScratchReg);
  li(kScratchReg, kNaN);
  vmv_vx(kSimd128ScratchReg, kScratchReg);
  vfmax_vv(kSimd128ScratchReg, rhs.fp().toV(), lhs.fp().toV(), Mask);
  vmv_vv(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_f64x2_pmin(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  // b < a ? b : a
  vmflt_vv(v0, rhs.fp().toV(), lhs.fp().toV());
  vmerge_vv(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_pmax(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  VU.set(kScratchReg, E64, m1);
  // a < b ? b : a
  vmflt_vv(v0, lhs.fp().toV(), rhs.fp().toV());
  vmerge_vv(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_sconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  VU.set(RoundingMode::RTZ);
  vmfeq_vv(v0, src.fp().toV(), src.fp().toV());
  vmv_vx(dst.fp().toV(), zero_reg);
  vfcvt_x_f_v(dst.fp().toV(), src.fp().toV(), Mask);
}

void LiftoffAssembler::emit_i32x4_uconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  VU.set(RoundingMode::RTZ);
  vmfeq_vv(v0, src.fp().toV(), src.fp().toV());
  vmv_vx(dst.fp().toV(), zero_reg);
  vfcvt_xu_f_v(dst.fp().toV(), src.fp().toV(), Mask);
}

void LiftoffAssembler::emit_f32x4_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  VU.set(RoundingMode::RTZ);
  vfcvt_f_x_v(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_f32x4_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  VU.set(RoundingMode::RTZ);
  vfcvt_f_xu_v(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i8x16_sconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vmv_vv(v26, lhs.fp().toV());
  vmv_vv(v27, lhs.fp().toV());
  VU.set(kScratchReg, E8, m1);
  VU.set(RoundingMode::RNE);
  vnclip_vi(dst.fp().toV(), v26, 0);
}

void LiftoffAssembler::emit_i8x16_uconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  VU.set(kScratchReg, E16, m1);
  vmv_vv(v26, lhs.fp().toV());
  vmv_vv(v27, lhs.fp().toV());
  VU.set(kScratchReg, E16, m2);
  vmax_vx(v26, v26, zero_reg);
  VU.set(kScratchReg, E8, m1);
  VU.set(RoundingMode::RNE);
  vnclipu_vi(dst.fp().toV(), v26, 0);
}

void LiftoffAssembler::emit_i16x8_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vmv_vv(v26, lhs.fp().toV());
  vmv_vv(v27, lhs.fp().toV());
  VU.set(kScratchReg, E16, m1);
  VU.set(RoundingMode::RNE);
  vnclip_vi(dst.fp().toV(), v26, 0);
}

void LiftoffAssembler::emit_i16x8_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  VU.set(kScratchReg, E32, m1);
  vmv_vv(v26, lhs.fp().toV());
  vmv_vv(v27, lhs.fp().toV());
  VU.set(kScratchReg, E32, m2);
  vmax_vx(v26, v26, zero_reg);
  VU.set(kScratchReg, E16, m1);
  VU.set(RoundingMode::RNE);
  vnclipu_vi(dst.fp().toV(), v26, 0);
}

void LiftoffAssembler::emit_i16x8_sconvert_i8x16_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  VU.set(kScratchReg, E16, m1);
  vmv_vv(kSimd128ScratchReg, src.fp().toV());
  vsext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i16x8_sconvert_i8x16_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  VU.set(kScratchReg, E8, m1);
  vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), 8);
  VU.set(kScratchReg, E16, m1);
  vsext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i16x8_uconvert_i8x16_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  VU.set(kScratchReg, E16, m1);
  vmv_vv(kSimd128ScratchReg, src.fp().toV());
  vzext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i16x8_uconvert_i8x16_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  VU.set(kScratchReg, E8, m1);
  vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), 8);
  VU.set(kScratchReg, E16, m1);
  vzext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i32x4_sconvert_i16x8_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  vmv_vv(kSimd128ScratchReg, src.fp().toV());
  vsext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i32x4_sconvert_i16x8_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  VU.set(kScratchReg, E16, m1);
  vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), 4);
  VU.set(kScratchReg, E32, m1);
  vsext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i32x4_uconvert_i16x8_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  vmv_vv(kSimd128ScratchReg, src.fp().toV());
  vzext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i32x4_uconvert_i16x8_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  VU.set(kScratchReg, E16, m1);
  vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), 4);
  VU.set(kScratchReg, E32, m1);
  vzext_vf2(dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i8x16_rounding_average_u(LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
  VU.set(kScratchReg, E8, m1);
  vwaddu_vv(kSimd128ScratchReg, lhs.fp().toV(), rhs.fp().toV());
  li(kScratchReg, 1);
  vwaddu_wx(kSimd128ScratchReg3, kSimd128ScratchReg, kScratchReg);
  li(kScratchReg, 2);
  VU.set(kScratchReg2, E16, m2);
  vdivu_vx(kSimd128ScratchReg3, kSimd128ScratchReg3, kScratchReg);
  VU.set(kScratchReg2, E8, m1);
  vnclipu_vi(dst.fp().toV(), kSimd128ScratchReg3, 0);
}
void LiftoffAssembler::emit_i16x8_rounding_average_u(LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
  VU.set(kScratchReg2, E16, m1);
  vwaddu_vv(kSimd128ScratchReg, lhs.fp().toV(), rhs.fp().toV());
  li(kScratchReg, 1);
  vwaddu_wx(kSimd128ScratchReg3, kSimd128ScratchReg, kScratchReg);
  li(kScratchReg, 2);
  VU.set(kScratchReg2, E32, m2);
  vdivu_vx(kSimd128ScratchReg3, kSimd128ScratchReg3, kScratchReg);
  VU.set(kScratchReg2, E16, m1);
  vnclipu_vi(dst.fp().toV(), kSimd128ScratchReg3, 0);
}

void LiftoffAssembler::emit_i8x16_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E8, m1);
  vmv_vx(kSimd128RegZero, zero_reg);
  vmv_vv(dst.fp().toV(), src.fp().toV());
  vmslt_vv(v0, src.fp().toV(), kSimd128RegZero);
  vneg_vv(dst.fp().toV(), src.fp().toV(), MaskType::Mask);
}

void LiftoffAssembler::emit_i16x8_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E16, m1);
  vmv_vx(kSimd128RegZero, zero_reg);
  vmv_vv(dst.fp().toV(), src.fp().toV());
  vmslt_vv(v0, src.fp().toV(), kSimd128RegZero);
  vneg_vv(dst.fp().toV(), src.fp().toV(), MaskType::Mask);
}

void LiftoffAssembler::emit_i64x2_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  vmv_vx(kSimd128RegZero, zero_reg);
  vmv_vv(dst.fp().toV(), src.fp().toV());
  vmslt_vv(v0, src.fp().toV(), kSimd128RegZero);
  vneg_vv(dst.fp().toV(), src.fp().toV(), MaskType::Mask);
}

void LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_s(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  li(kScratchReg, 0x0006000400020000);
  vmv_sx(kSimd128ScratchReg, kScratchReg);
  li(kScratchReg, 0x0007000500030001);
  vmv_sx(kSimd128ScratchReg3, kScratchReg);
  VU.set(kScratchReg, E16, m1);
  vrgather_vv(kSimd128ScratchReg2, src.fp().toV(), kSimd128ScratchReg);
  vrgather_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg3);
  VU.set(kScratchReg, E16, mf2);
  vwadd_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
}

void LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_u(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  li(kScratchReg, 0x0006000400020000);
  vmv_sx(kSimd128ScratchReg, kScratchReg);
  li(kScratchReg, 0x0007000500030001);
  vmv_sx(kSimd128ScratchReg3, kScratchReg);
  VU.set(kScratchReg, E16, m1);
  vrgather_vv(kSimd128ScratchReg2, src.fp().toV(), kSimd128ScratchReg);
  vrgather_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg3);
  VU.set(kScratchReg, E16, mf2);
  vwaddu_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
}

void LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_s(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  li(kScratchReg, 0x0E0C0A0806040200);
  vmv_sx(kSimd128ScratchReg, kScratchReg);
  li(kScratchReg, 0x0F0D0B0907050301);
  vmv_sx(kSimd128ScratchReg3, kScratchReg);
  VU.set(kScratchReg, E8, m1);
  vrgather_vv(kSimd128ScratchReg2, src.fp().toV(), kSimd128ScratchReg);
  vrgather_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg3);
  VU.set(kScratchReg, E8, mf2);
  vwadd_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
}

void LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_u(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  VU.set(kScratchReg, E64, m1);
  li(kScratchReg, 0x0E0C0A0806040200);
  vmv_sx(kSimd128ScratchReg, kScratchReg);
  li(kScratchReg, 0x0F0D0B0907050301);
  vmv_sx(kSimd128ScratchReg3, kScratchReg);
  VU.set(kScratchReg, E8, m1);
  vrgather_vv(kSimd128ScratchReg2, src.fp().toV(), kSimd128ScratchReg);
  vrgather_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg3);
  VU.set(kScratchReg, E8, mf2);
  vwaddu_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
}

void LiftoffAssembler::emit_i32x4_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  VU.set(kScratchReg, E32, m1);
  vmv_vx(kSimd128RegZero, zero_reg);
  vmv_vv(dst.fp().toV(), src.fp().toV());
  vmslt_vv(v0, src.fp().toV(), kSimd128RegZero);
  vneg_vv(dst.fp().toV(), src.fp().toV(), MaskType::Mask);
}

void LiftoffAssembler::emit_i8x16_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E8, m1);
  vslidedown_vi(kSimd128ScratchReg, lhs.fp().toV(), imm_lane_idx);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i8x16_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E8, m1);
  vslidedown_vi(kSimd128ScratchReg, lhs.fp().toV(), imm_lane_idx);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
  slli(dst.gp(), dst.gp(), 64 - 8);
  srli(dst.gp(), dst.gp(), 64 - 8);
}

void LiftoffAssembler::emit_i16x8_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E16, m1);
  vslidedown_vi(kSimd128ScratchReg, lhs.fp().toV(), imm_lane_idx);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i16x8_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E16, m1);
  vslidedown_vi(kSimd128ScratchReg, lhs.fp().toV(), imm_lane_idx);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
  slli(dst.gp(), dst.gp(), 64 - 16);
  srli(dst.gp(), dst.gp(), 64 - 16);
}

void LiftoffAssembler::emit_i32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E32, m1);
  vslidedown_vi(kSimd128ScratchReg, lhs.fp().toV(), imm_lane_idx);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E64, m1);
  vslidedown_vi(kSimd128ScratchReg, lhs.fp().toV(), imm_lane_idx);
  vmv_xs(dst.gp(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_f32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E32, m1);
  vslidedown_vi(kSimd128ScratchReg, lhs.fp().toV(), imm_lane_idx);
  vfmv_fs(dst.fp(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_f64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E64, m1);
  vslidedown_vi(kSimd128ScratchReg, lhs.fp().toV(), imm_lane_idx);
  vfmv_fs(dst.fp(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i8x16_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E64, m1);
  li(kScratchReg, 0x1 << imm_lane_idx);
  vmv_sx(v0, kScratchReg);
  VU.set(kScratchReg, E8, m1);
  vmerge_vx(dst.fp().toV(), src2.gp(), src1.fp().toV());
}

void LiftoffAssembler::emit_i16x8_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E16, m1);
  li(kScratchReg, 0x1 << imm_lane_idx);
  vmv_sx(v0, kScratchReg);
  vmerge_vx(dst.fp().toV(), src2.gp(), src1.fp().toV());
}

void LiftoffAssembler::emit_i32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E32, m1);
  li(kScratchReg, 0x1 << imm_lane_idx);
  vmv_sx(v0, kScratchReg);
  vmerge_vx(dst.fp().toV(), src2.gp(), src1.fp().toV());
}

void LiftoffAssembler::emit_i64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E64, m1);
  li(kScratchReg, 0x1 << imm_lane_idx);
  vmv_sx(v0, kScratchReg);
  vmerge_vx(dst.fp().toV(), src2.gp(), src1.fp().toV());
}

void LiftoffAssembler::emit_f32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E32, m1);
  li(kScratchReg, 0x1 << imm_lane_idx);
  vmv_sx(v0, kScratchReg);
  fmv_x_w(kScratchReg, src2.fp());
  vmerge_vx(dst.fp().toV(), kScratchReg, src1.fp().toV());
}

void LiftoffAssembler::emit_f64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  VU.set(kScratchReg, E64, m1);
  li(kScratchReg, 0x1 << imm_lane_idx);
  vmv_sx(v0, kScratchReg);
  fmv_x_d(kScratchReg, src2.fp());
  vmerge_vx(dst.fp().toV(), kScratchReg, src1.fp().toV());
}

void LiftoffAssembler::emit_s128_set_if_nan(Register dst, LiftoffRegister src,
                                            Register tmp_gp,
                                            LiftoffRegister tmp_s128,
                                            ValueKind lane_kind) {
  DoubleRegister tmp_fp = tmp_s128.fp();
  vfredmax_vs(kSimd128ScratchReg, src.fp().toV(), src.fp().toV());
  vfmv_fs(tmp_fp, kSimd128ScratchReg);
  if (lane_kind == kF32) {
    feq_s(kScratchReg, tmp_fp, tmp_fp);  // scratch <- !IsNan(tmp_fp)
  } else {
    DCHECK_EQ(lane_kind, kF64);
    feq_d(kScratchReg, tmp_fp, tmp_fp);  // scratch <- !IsNan(tmp_fp)
  }
  not_(kScratchReg, kScratchReg);
  Sw(kScratchReg, MemOperand(dst));
}

void LiftoffAssembler::StackCheck(Label* ool_code, Register limit_address) {
  TurboAssembler::Ld(limit_address, MemOperand(limit_address));
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
  int32_t num_gp_regs = gp_regs.GetNumRegsSet();
  if (num_gp_regs) {
    int32_t offset = num_gp_regs * kSystemPointerSize;
    Add64(sp, sp, Operand(-offset));
    while (!gp_regs.is_empty()) {
      LiftoffRegister reg = gp_regs.GetFirstRegSet();
      offset -= kSystemPointerSize;
      Sd(reg.gp(), MemOperand(sp, offset));
      gp_regs.clear(reg);
    }
    DCHECK_EQ(offset, 0);
  }
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  int32_t num_fp_regs = fp_regs.GetNumRegsSet();
  if (num_fp_regs) {
    Add64(sp, sp, Operand(-(num_fp_regs * kStackSlotSize)));
    int32_t offset = 0;
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetFirstRegSet();
      TurboAssembler::StoreDouble(reg.fp(), MemOperand(sp, offset));
      fp_regs.clear(reg);
      offset += sizeof(double);
    }
    DCHECK_EQ(offset, num_fp_regs * sizeof(double));
  }
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  int32_t fp_offset = 0;
  while (!fp_regs.is_empty()) {
    LiftoffRegister reg = fp_regs.GetFirstRegSet();
    TurboAssembler::LoadDouble(reg.fp(), MemOperand(sp, fp_offset));
    fp_regs.clear(reg);
    fp_offset += sizeof(double);
  }
  if (fp_offset) Add64(sp, sp, Operand(fp_offset));
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  int32_t gp_offset = 0;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetLastRegSet();
    Ld(reg.gp(), MemOperand(sp, gp_offset));
    gp_regs.clear(reg);
    gp_offset += kSystemPointerSize;
  }
  Add64(sp, sp, Operand(gp_offset));
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
  TurboAssembler::DropAndRet(static_cast<int>(num_stack_slots));
}

void LiftoffAssembler::CallC(const ValueKindSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueKind out_argument_kind, int stack_bytes,
                             ExternalReference ext_ref) {
  Add64(sp, sp, Operand(-stack_bytes));

  int arg_bytes = 0;
  for (ValueKind param_kind : sig->parameters()) {
    liftoff::Store(this, sp, arg_bytes, *args++, param_kind);
    arg_bytes += value_kind_size(param_kind);
  }
  DCHECK_LE(arg_bytes, stack_bytes);

  // Pass a pointer to the buffer with the arguments to the C function.
  // On RISC-V, the first argument is passed in {a0}.
  constexpr Register kFirstArgReg = a0;
  mv(kFirstArgReg, sp);

  // Now call the C function.
  constexpr int kNumCCallArgs = 1;
  PrepareCallCFunction(kNumCCallArgs, kScratchReg);
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* next_result_reg = rets;
  if (sig->return_count() > 0) {
    DCHECK_EQ(1, sig->return_count());
    constexpr Register kReturnReg = a0;
    if (kReturnReg != next_result_reg->gp()) {
      Move(*next_result_reg, LiftoffRegister(kReturnReg), sig->GetReturn(0));
    }
    ++next_result_reg;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_kind != kVoid) {
    liftoff::Load(this, *next_result_reg, MemOperand(sp, 0), out_argument_kind);
  }

  Add64(sp, sp, Operand(stack_bytes));
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
    pop(t6);
    Call(t6);
  } else {
    Call(target);
  }
}

void LiftoffAssembler::TailCallIndirect(Register target) {
  if (target == no_reg) {
    Pop(t6);
    Jump(t6);
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
  Add64(sp, sp, Operand(-size));
  TurboAssembler::Move(addr, sp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  Add64(sp, sp, Operand(size));
}

void LiftoffAssembler::MaybeOSR() {}

void LiftoffAssembler::emit_set_if_nan(Register dst, FPURegister src,
                                       ValueKind kind) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  li(scratch, 1);
  if (kind == kF32) {
    feq_s(scratch, src, src);  // rd <- !isNan(src)
  } else {
    DCHECK_EQ(kind, kF64);
    feq_d(scratch, src, src);  // rd <- !isNan(src)
  }
  not_(scratch, scratch);
  Sd(scratch, MemOperand(dst));
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
      case LiftoffAssembler::VarState::kStack:
        if (src.kind() != kS128) {
          asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
          asm_->Ld(kScratchReg, liftoff::GetStackSlot(slot.src_offset_));
          asm_->push(kScratchReg);
        } else {
          asm_->AllocateStackSpace(stack_decrement - kSimd128Size);
          asm_->Ld(kScratchReg, liftoff::GetStackSlot(slot.src_offset_ - 8));
          asm_->push(kScratchReg);
          asm_->Ld(kScratchReg, liftoff::GetStackSlot(slot.src_offset_));
          asm_->push(kScratchReg);
        }
        break;
      case LiftoffAssembler::VarState::kRegister: {
        int pushed_bytes = SlotSizeInBytes(slot);
        asm_->AllocateStackSpace(stack_decrement - pushed_bytes);
        liftoff::push(asm_, src.reg(), src.kind());
        break;
      }
      case LiftoffAssembler::VarState::kIntConst: {
        asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
        asm_->li(kScratchReg, Operand(src.i32_const()));
        asm_->push(kScratchReg);
        break;
      }
    }
  }
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_RISCV64_LIFTOFF_ASSEMBLER_RISCV64_H_

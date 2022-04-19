// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_ARM64_LIFTOFF_ASSEMBLER_ARM64_H_
#define V8_WASM_BASELINE_ARM64_LIFTOFF_ASSEMBLER_ARM64_H_

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
      return lo;
    case kUnsignedLessEqual:
      return ls;
    case kUnsignedGreaterThan:
      return hi;
    case kUnsignedGreaterEqual:
      return hs;
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
//   1   | return addr (lr)   |
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

constexpr int kInstanceOffset = 2 * kSystemPointerSize;
constexpr int kFeedbackVectorOffset = 3 * kSystemPointerSize;
constexpr int kTierupBudgetOffset = 4 * kSystemPointerSize;

inline MemOperand GetStackSlot(int offset) { return MemOperand(fp, -offset); }

inline MemOperand GetInstanceOperand() { return GetStackSlot(kInstanceOffset); }

inline CPURegister GetRegFromType(const LiftoffRegister& reg, ValueKind kind) {
  switch (kind) {
    case kI32:
      return reg.gp().W();
    case kI64:
    case kRef:
    case kOptRef:
    case kRtt:
      return reg.gp().X();
    case kF32:
      return reg.fp().S();
    case kF64:
      return reg.fp().D();
    case kS128:
      return reg.fp().Q();
    default:
      UNREACHABLE();
  }
}

inline CPURegList PadRegList(RegList list) {
  if ((list.Count() & 1) != 0) list.set(padreg);
  return CPURegList(kXRegSizeInBits, list);
}

inline CPURegList PadVRegList(DoubleRegList list) {
  if ((list.Count() & 1) != 0) list.set(fp_scratch);
  return CPURegList(kQRegSizeInBits, list);
}

inline CPURegister AcquireByType(UseScratchRegisterScope* temps,
                                 ValueKind kind) {
  switch (kind) {
    case kI32:
      return temps->AcquireW();
    case kI64:
    case kRef:
    case kOptRef:
      return temps->AcquireX();
    case kF32:
      return temps->AcquireS();
    case kF64:
      return temps->AcquireD();
    case kS128:
      return temps->AcquireQ();
    default:
      UNREACHABLE();
  }
}

template <typename T>
inline MemOperand GetMemOp(LiftoffAssembler* assm,
                           UseScratchRegisterScope* temps, Register addr,
                           Register offset, T offset_imm,
                           bool i64_offset = false) {
  if (!offset.is_valid()) return MemOperand(addr.X(), offset_imm);
  Register effective_addr = addr.X();
  if (offset_imm) {
    effective_addr = temps->AcquireX();
    assm->Add(effective_addr, addr.X(), offset_imm);
  }
  return i64_offset ? MemOperand(effective_addr, offset.X())
                    : MemOperand(effective_addr, offset.W(), UXTW);
}

// Compute the effective address (sum of |addr|, |offset| (if given) and
// |offset_imm|) into a temporary register. This is needed for certain load
// instructions that do not support an offset (register or immediate).
// Returns |addr| if both |offset| and |offset_imm| are zero.
inline Register GetEffectiveAddress(LiftoffAssembler* assm,
                                    UseScratchRegisterScope* temps,
                                    Register addr, Register offset,
                                    uintptr_t offset_imm) {
  if (!offset.is_valid() && offset_imm == 0) return addr;
  Register tmp = temps->AcquireX();
  if (offset.is_valid()) {
    // TODO(clemensb): This needs adaption for memory64.
    assm->Add(tmp, addr, Operand(offset, UXTW));
    addr = tmp;
  }
  if (offset_imm != 0) assm->Add(tmp, addr, offset_imm);
  return tmp;
}

enum class ShiftDirection : bool { kLeft, kRight };

enum class ShiftSign : bool { kSigned, kUnsigned };

template <ShiftDirection dir, ShiftSign sign = ShiftSign::kSigned>
inline void EmitSimdShift(LiftoffAssembler* assm, VRegister dst, VRegister lhs,
                          Register rhs, VectorFormat format) {
  DCHECK_IMPLIES(dir == ShiftDirection::kLeft, sign == ShiftSign::kSigned);
  DCHECK(dst.IsSameFormat(lhs));
  DCHECK_EQ(dst.LaneCount(), LaneCountFromFormat(format));

  UseScratchRegisterScope temps(assm);
  VRegister tmp = temps.AcquireV(format);
  Register shift = dst.Is2D() ? temps.AcquireX() : temps.AcquireW();
  int mask = LaneSizeInBitsFromFormat(format) - 1;
  assm->And(shift, rhs, mask);
  assm->Dup(tmp, shift);

  if (dir == ShiftDirection::kRight) {
    assm->Neg(tmp, tmp);
  }

  if (sign == ShiftSign::kSigned) {
    assm->Sshl(dst, lhs, tmp);
  } else {
    assm->Ushl(dst, lhs, tmp);
  }
}

template <VectorFormat format, ShiftSign sign>
inline void EmitSimdShiftRightImmediate(LiftoffAssembler* assm, VRegister dst,
                                        VRegister lhs, int32_t rhs) {
  // Sshr and Ushr does not allow shifts to be 0, so check for that here.
  int mask = LaneSizeInBitsFromFormat(format) - 1;
  int32_t shift = rhs & mask;
  if (!shift) {
    if (dst != lhs) {
      assm->Mov(dst, lhs);
    }
    return;
  }

  if (sign == ShiftSign::kSigned) {
    assm->Sshr(dst, lhs, rhs & mask);
  } else {
    assm->Ushr(dst, lhs, rhs & mask);
  }
}

inline void EmitAnyTrue(LiftoffAssembler* assm, LiftoffRegister dst,
                        LiftoffRegister src) {
  // AnyTrue does not depend on the number of lanes, so we can use V4S for all.
  UseScratchRegisterScope scope(assm);
  VRegister temp = scope.AcquireV(kFormatS);
  assm->Umaxv(temp, src.fp().V4S());
  assm->Umov(dst.gp().W(), temp, 0);
  assm->Cmp(dst.gp().W(), 0);
  assm->Cset(dst.gp().W(), ne);
}

inline void EmitAllTrue(LiftoffAssembler* assm, LiftoffRegister dst,
                        LiftoffRegister src, VectorFormat format) {
  UseScratchRegisterScope scope(assm);
  VRegister temp = scope.AcquireV(ScalarFormatFromFormat(format));
  assm->Uminv(temp, VRegister::Create(src.fp().code(), format));
  assm->Umov(dst.gp().W(), temp, 0);
  assm->Cmp(dst.gp().W(), 0);
  assm->Cset(dst.gp().W(), ne);
}

}  // namespace liftoff

int LiftoffAssembler::PrepareStackFrame() {
  int offset = pc_offset();
  InstructionAccurateScope scope(this, 1);
  // Next we reserve the memory for the whole stack frame. We do not know yet
  // how big the stack frame will be so we just emit a placeholder instruction.
  // PatchPrepareStackFrame will patch this in order to increase the stack
  // appropriately.
  sub(sp, sp, 0);
  return offset;
}

void LiftoffAssembler::PrepareTailCall(int num_callee_stack_params,
                                       int stack_param_delta) {
  UseScratchRegisterScope temps(this);
  temps.Exclude(x16, x17);

  // This is the previous stack pointer value (before we push the lr and the
  // fp). We need to keep it to autenticate the lr and adjust the new stack
  // pointer afterwards.
  Add(x16, fp, 16);

  // Load the fp and lr of the old frame, they will be pushed in the new frame
  // during the actual call.
#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
  Ldp(fp, x17, MemOperand(fp));
  Autib1716();
  Mov(lr, x17);
#else
  Ldp(fp, lr, MemOperand(fp));
#endif

  temps.Include(x17);

  Register scratch = temps.AcquireX();

  // Shift the whole frame upwards, except for fp and lr.
  int slot_count = num_callee_stack_params;
  for (int i = slot_count - 1; i >= 0; --i) {
    ldr(scratch, MemOperand(sp, i * 8));
    str(scratch, MemOperand(x16, (i - stack_param_delta) * 8));
  }

  // Set the new stack pointer.
  Sub(sp, x16, stack_param_delta * 8);
}

void LiftoffAssembler::AlignFrameSize() {
  // The frame_size includes the frame marker. The frame marker has already been
  // pushed on the stack though, so we don't need to allocate memory for it
  // anymore.
  int initial_frame_size = GetTotalFrameSize() - 2 * kSystemPointerSize;
  int frame_size = initial_frame_size;

  static_assert(kStackSlotSize == kXRegSize,
                "kStackSlotSize must equal kXRegSize");
  // The stack pointer is required to be quadword aligned.
  // Misalignment will cause a stack alignment fault.
  frame_size = RoundUp(frame_size, kQuadWordSizeInBytes);
  if (!IsImmAddSub(frame_size)) {
    // Round the stack to a page to try to fit a add/sub immediate.
    frame_size = RoundUp(frame_size, 0x1000);
    if (!IsImmAddSub(frame_size)) {
      // Stack greater than 4M! Because this is a quite improbable case, we
      // just fallback to TurboFan.
      bailout(kOtherReason, "Stack too big");
      return;
    }
  }
  if (frame_size > initial_frame_size) {
    // Record the padding, as it is needed for GC offsets later.
    max_used_spill_offset_ += (frame_size - initial_frame_size);
  }
}

void LiftoffAssembler::PatchPrepareStackFrame(
    int offset, SafepointTableBuilder* safepoint_table_builder) {
  // The frame_size includes the frame marker and the instance slot. Both are
  // pushed as part of frame construction, so we don't need to allocate memory
  // for them anymore.
  int frame_size = GetTotalFrameSize() - 2 * kSystemPointerSize;

  // The stack pointer is required to be quadword aligned.
  // Misalignment will cause a stack alignment fault.
  DCHECK_EQ(frame_size, RoundUp(frame_size, kQuadWordSizeInBytes));
  DCHECK(IsImmAddSub(frame_size));

  PatchingAssembler patching_assembler(AssemblerOptions{},
                                       buffer_start_ + offset, 1);

  if (V8_LIKELY(frame_size < 4 * KB)) {
    // This is the standard case for small frames: just subtract from SP and be
    // done with it.
    patching_assembler.PatchSubSp(frame_size);
    return;
  }

  // The frame size is bigger than 4KB, so we might overflow the available stack
  // space if we first allocate the frame and then do the stack check (we will
  // need some remaining stack space for throwing the exception). That's why we
  // check the available stack space before we allocate the frame. To do this we
  // replace the {__ sub(sp, sp, framesize)} with a jump to OOL code that does
  // this "extended stack check".
  //
  // The OOL code can simply be generated here with the normal assembler,
  // because all other code generation, including OOL code, has already finished
  // when {PatchPrepareStackFrame} is called. The function prologue then jumps
  // to the current {pc_offset()} to execute the OOL code for allocating the
  // large frame.

  // Emit the unconditional branch in the function prologue (from {offset} to
  // {pc_offset()}).
  patching_assembler.b((pc_offset() - offset) >> kInstrSizeLog2);

  // If the frame is bigger than the stack, we throw the stack overflow
  // exception unconditionally. Thereby we can avoid the integer overflow
  // check in the condition code.
  RecordComment("OOL: stack check for large frame");
  Label continuation;
  if (frame_size < FLAG_stack_size * 1024) {
    UseScratchRegisterScope temps(this);
    Register stack_limit = temps.AcquireX();
    Ldr(stack_limit,
        FieldMemOperand(kWasmInstanceRegister,
                        WasmInstanceObject::kRealStackLimitAddressOffset));
    Ldr(stack_limit, MemOperand(stack_limit));
    Add(stack_limit, stack_limit, Operand(frame_size));
    Cmp(sp, stack_limit);
    B(hs /* higher or same */, &continuation);
  }

  Call(wasm::WasmCode::kWasmStackOverflow, RelocInfo::WASM_STUB_CALL);
  // The call will not return; just define an empty safepoint.
  safepoint_table_builder->DefineSafepoint(this);
  if (FLAG_debug_code) Brk(0);

  bind(&continuation);

  // Now allocate the stack space. Note that this might do more than just
  // decrementing the SP; consult {TurboAssembler::Claim}.
  Claim(frame_size, 1);

  // Jump back to the start of the function, from {pc_offset()} to
  // right after the reserved space for the {__ sub(sp, sp, framesize)} (which
  // is a branch now).
  int func_start_offset = offset + kInstrSize;
  b((func_start_offset - pc_offset()) >> kInstrSizeLog2);
}

void LiftoffAssembler::FinishCode() { ForceConstantPoolEmissionWithoutJump(); }

void LiftoffAssembler::AbortCompilation() { AbortedCodeGeneration(); }

// static
constexpr int LiftoffAssembler::StaticStackFrameSize() {
  return liftoff::kTierupBudgetOffset;
}

int LiftoffAssembler::SlotSizeForType(ValueKind kind) {
  // TODO(zhin): Unaligned access typically take additional cycles, we should do
  // some performance testing to see how big an effect it will take.
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
      Mov(reg.gp().W(), Immediate(value.to_i32(), rmode));
      break;
    case kI64:
      Mov(reg.gp().X(), Immediate(value.to_i64(), rmode));
      break;
    case kF32:
      Fmov(reg.fp().S(), value.to_f32_boxed().get_scalar());
      break;
    case kF64:
      Fmov(reg.fp().D(), value.to_f64_boxed().get_scalar());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadInstanceFromFrame(Register dst) {
  Ldr(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::LoadFromInstance(Register dst, Register instance,
                                        int offset, int size) {
  DCHECK_LE(0, offset);
  MemOperand src{instance, offset};
  switch (size) {
    case 1:
      Ldrb(dst.W(), src);
      break;
    case 4:
      Ldr(dst.W(), src);
      break;
    case 8:
      Ldr(dst, src);
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

void LiftoffAssembler::LoadExternalPointer(Register dst, Register instance,
                                           int offset, ExternalPointerTag tag,
                                           Register isolate_root) {
  LoadExternalPointerField(dst, FieldMemOperand(instance, offset), tag,
                           isolate_root);
}

void LiftoffAssembler::SpillInstance(Register instance) {
  Str(instance, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::ResetOSRTarget() {}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         int32_t offset_imm,
                                         LiftoffRegList pinned) {
  UseScratchRegisterScope temps(this);
  MemOperand src_op =
      liftoff::GetMemOp(this, &temps, src_addr, offset_reg, offset_imm);
  LoadTaggedPointerField(dst, src_op);
}

void LiftoffAssembler::LoadFullPointer(Register dst, Register src_addr,
                                       int32_t offset_imm) {
  UseScratchRegisterScope temps(this);
  MemOperand src_op =
      liftoff::GetMemOp(this, &temps, src_addr, no_reg, offset_imm);
  Ldr(dst.X(), src_op);
}

void LiftoffAssembler::StoreTaggedPointer(Register dst_addr,
                                          Register offset_reg,
                                          int32_t offset_imm,
                                          LiftoffRegister src,
                                          LiftoffRegList pinned,
                                          SkipWriteBarrier skip_write_barrier) {
  UseScratchRegisterScope temps(this);
  Operand offset_op = offset_reg.is_valid() ? Operand(offset_reg.W(), UXTW)
                                            : Operand(offset_imm);
  // For the write barrier (below), we cannot have both an offset register and
  // an immediate offset. Add them to a 32-bit offset initially, but in a 64-bit
  // register, because that's needed in the MemOperand below.
  if (offset_reg.is_valid() && offset_imm) {
    Register effective_offset = temps.AcquireX();
    Add(effective_offset.W(), offset_reg.W(), offset_imm);
    offset_op = effective_offset;
  }
  StoreTaggedField(src.gp(), MemOperand(dst_addr.X(), offset_op));

  if (skip_write_barrier || FLAG_disable_write_barriers) return;

  // The write barrier.
  Label write_barrier;
  Label exit;
  CheckPageFlag(dst_addr, MemoryChunk::kPointersFromHereAreInterestingMask, eq,
                &write_barrier);
  b(&exit);
  bind(&write_barrier);
  JumpIfSmi(src.gp(), &exit);
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTaggedPointer(src.gp(), src.gp());
  }
  CheckPageFlag(src.gp(), MemoryChunk::kPointersToHereAreInterestingMask, ne,
                &exit);
  CallRecordWriteStubSaveRegisters(
      dst_addr, offset_op, RememberedSetAction::kEmit, SaveFPRegsMode::kSave,
      StubCallMode::kCallWasmRuntimeStub);
  bind(&exit);
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uintptr_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc, bool is_load_mem,
                            bool i64_offset) {
  UseScratchRegisterScope temps(this);
  MemOperand src_op = liftoff::GetMemOp(this, &temps, src_addr, offset_reg,
                                        offset_imm, i64_offset);
  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U:
      Ldrb(dst.gp().W(), src_op);
      break;
    case LoadType::kI32Load8S:
      Ldrsb(dst.gp().W(), src_op);
      break;
    case LoadType::kI64Load8S:
      Ldrsb(dst.gp().X(), src_op);
      break;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      Ldrh(dst.gp().W(), src_op);
      break;
    case LoadType::kI32Load16S:
      Ldrsh(dst.gp().W(), src_op);
      break;
    case LoadType::kI64Load16S:
      Ldrsh(dst.gp().X(), src_op);
      break;
    case LoadType::kI32Load:
    case LoadType::kI64Load32U:
      Ldr(dst.gp().W(), src_op);
      break;
    case LoadType::kI64Load32S:
      Ldrsw(dst.gp().X(), src_op);
      break;
    case LoadType::kI64Load:
      Ldr(dst.gp().X(), src_op);
      break;
    case LoadType::kF32Load:
      Ldr(dst.fp().S(), src_op);
      break;
    case LoadType::kF64Load:
      Ldr(dst.fp().D(), src_op);
      break;
    case LoadType::kS128Load:
      Ldr(dst.fp().Q(), src_op);
      break;
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uintptr_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  UseScratchRegisterScope temps(this);
  MemOperand dst_op =
      liftoff::GetMemOp(this, &temps, dst_addr, offset_reg, offset_imm);
  if (protected_store_pc) *protected_store_pc = pc_offset();
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      Strb(src.gp().W(), dst_op);
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      Strh(src.gp().W(), dst_op);
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      Str(src.gp().W(), dst_op);
      break;
    case StoreType::kI64Store:
      Str(src.gp().X(), dst_op);
      break;
    case StoreType::kF32Store:
      Str(src.fp().S(), dst_op);
      break;
    case StoreType::kF64Store:
      Str(src.fp().D(), dst_op);
      break;
    case StoreType::kS128Store:
      Str(src.fp().Q(), dst_op);
      break;
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
  __ Add(result_reg, addr_reg, Operand(offset_reg));
  if (offset_imm != 0) {
    __ Add(result_reg, result_reg, Operand(offset_imm));
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

  // {LiftoffCompiler::AtomicBinop} ensures that {result} is unique.
  DCHECK(result.gp() != value.gp() && result.gp() != dst_addr &&
         result.gp() != offset_reg);

  UseScratchRegisterScope temps(lasm);
  Register actual_addr = liftoff::CalculateActualAddress(
      lasm, dst_addr, offset_reg, offset_imm, temps.AcquireX());

  // Allocate an additional {temp} register to hold the result that should be
  // stored to memory. Note that {temp} and {store_result} are not allowed to be
  // the same register.
  Register temp = temps.AcquireX();

  Label retry;
  __ Bind(&retry);
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      __ ldaxrb(result.gp().W(), actual_addr);
      break;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      __ ldaxrh(result.gp().W(), actual_addr);
      break;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      __ ldaxr(result.gp().W(), actual_addr);
      break;
    case StoreType::kI64Store:
      __ ldaxr(result.gp().X(), actual_addr);
      break;
    default:
      UNREACHABLE();
  }

  switch (op) {
    case Binop::kAdd:
      __ add(temp, result.gp(), value.gp());
      break;
    case Binop::kSub:
      __ sub(temp, result.gp(), value.gp());
      break;
    case Binop::kAnd:
      __ and_(temp, result.gp(), value.gp());
      break;
    case Binop::kOr:
      __ orr(temp, result.gp(), value.gp());
      break;
    case Binop::kXor:
      __ eor(temp, result.gp(), value.gp());
      break;
    case Binop::kExchange:
      __ mov(temp, value.gp());
      break;
  }

  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      __ stlxrb(store_result.W(), temp.W(), actual_addr);
      break;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      __ stlxrh(store_result.W(), temp.W(), actual_addr);
      break;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      __ stlxr(store_result.W(), temp.W(), actual_addr);
      break;
    case StoreType::kI64Store:
      __ stlxr(store_result.W(), temp.X(), actual_addr);
      break;
    default:
      UNREACHABLE();
  }

  __ Cbnz(store_result.W(), &retry);
}

#undef __
}  // namespace liftoff

void LiftoffAssembler::AtomicLoad(LiftoffRegister dst, Register src_addr,
                                  Register offset_reg, uintptr_t offset_imm,
                                  LoadType type, LiftoffRegList pinned) {
  UseScratchRegisterScope temps(this);
  Register src_reg = liftoff::CalculateActualAddress(
      this, src_addr, offset_reg, offset_imm, temps.AcquireX());
  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U:
      Ldarb(dst.gp().W(), src_reg);
      return;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      Ldarh(dst.gp().W(), src_reg);
      return;
    case LoadType::kI32Load:
    case LoadType::kI64Load32U:
      Ldar(dst.gp().W(), src_reg);
      return;
    case LoadType::kI64Load:
      Ldar(dst.gp().X(), src_reg);
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
      this, dst_addr, offset_reg, offset_imm, temps.AcquireX());
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      Stlrb(src.gp().W(), dst_reg);
      return;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      Stlrh(src.gp().W(), dst_reg);
      return;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      Stlr(src.gp().W(), dst_reg);
      return;
    case StoreType::kI64Store:
      Stlr(src.gp().X(), dst_reg);
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

void LiftoffAssembler::AtomicCompareExchange(
    Register dst_addr, Register offset_reg, uintptr_t offset_imm,
    LiftoffRegister expected, LiftoffRegister new_value, LiftoffRegister result,
    StoreType type) {
  LiftoffRegList pinned = {dst_addr, offset_reg, expected, new_value};

  Register result_reg = result.gp();
  if (pinned.has(result)) {
    result_reg = GetUnusedRegister(kGpReg, pinned).gp();
  }

  UseScratchRegisterScope temps(this);

  Register actual_addr = liftoff::CalculateActualAddress(
      this, dst_addr, offset_reg, offset_imm, temps.AcquireX());

  Register store_result = temps.AcquireW();

  Label retry;
  Label done;
  Bind(&retry);
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      ldaxrb(result_reg.W(), actual_addr);
      Cmp(result.gp().W(), Operand(expected.gp().W(), UXTB));
      B(ne, &done);
      stlxrb(store_result.W(), new_value.gp().W(), actual_addr);
      break;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      ldaxrh(result_reg.W(), actual_addr);
      Cmp(result.gp().W(), Operand(expected.gp().W(), UXTH));
      B(ne, &done);
      stlxrh(store_result.W(), new_value.gp().W(), actual_addr);
      break;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      ldaxr(result_reg.W(), actual_addr);
      Cmp(result.gp().W(), Operand(expected.gp().W(), UXTW));
      B(ne, &done);
      stlxr(store_result.W(), new_value.gp().W(), actual_addr);
      break;
    case StoreType::kI64Store:
      ldaxr(result_reg.X(), actual_addr);
      Cmp(result.gp().X(), Operand(expected.gp().X(), UXTX));
      B(ne, &done);
      stlxr(store_result.W(), new_value.gp().X(), actual_addr);
      break;
    default:
      UNREACHABLE();
  }

  Cbnz(store_result.W(), &retry);
  Bind(&done);

  if (result_reg != result.gp()) {
    mov(result.gp(), result_reg);
  }
}

void LiftoffAssembler::AtomicFence() { Dmb(InnerShareable, BarrierAll); }

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueKind kind) {
  int32_t offset = (caller_slot_idx + 1) * LiftoffAssembler::kStackSlotSize;
  Ldr(liftoff::GetRegFromType(dst, kind), MemOperand(fp, offset));
}

void LiftoffAssembler::StoreCallerFrameSlot(LiftoffRegister src,
                                            uint32_t caller_slot_idx,
                                            ValueKind kind) {
  int32_t offset = (caller_slot_idx + 1) * LiftoffAssembler::kStackSlotSize;
  Str(liftoff::GetRegFromType(src, kind), MemOperand(fp, offset));
}

void LiftoffAssembler::LoadReturnStackSlot(LiftoffRegister dst, int offset,
                                           ValueKind kind) {
  Ldr(liftoff::GetRegFromType(dst, kind), MemOperand(sp, offset));
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_offset, uint32_t src_offset,
                                      ValueKind kind) {
  UseScratchRegisterScope temps(this);
  CPURegister scratch = liftoff::AcquireByType(&temps, kind);
  Ldr(scratch, liftoff::GetStackSlot(src_offset));
  Str(scratch, liftoff::GetStackSlot(dst_offset));
}

void LiftoffAssembler::Move(Register dst, Register src, ValueKind kind) {
  if (kind == kI32) {
    Mov(dst.W(), src.W());
  } else {
    DCHECK(kI64 == kind || is_reference(kind));
    Mov(dst.X(), src.X());
  }
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueKind kind) {
  if (kind == kF32) {
    Fmov(dst.S(), src.S());
  } else if (kind == kF64) {
    Fmov(dst.D(), src.D());
  } else {
    DCHECK_EQ(kS128, kind);
    Mov(dst.Q(), src.Q());
  }
}

void LiftoffAssembler::Spill(int offset, LiftoffRegister reg, ValueKind kind) {
  RecordUsedSpillOffset(offset);
  MemOperand dst = liftoff::GetStackSlot(offset);
  Str(liftoff::GetRegFromType(reg, kind), dst);
}

void LiftoffAssembler::Spill(int offset, WasmValue value) {
  RecordUsedSpillOffset(offset);
  MemOperand dst = liftoff::GetStackSlot(offset);
  UseScratchRegisterScope temps(this);
  CPURegister src = CPURegister::no_reg();
  switch (value.type().kind()) {
    case kI32:
      if (value.to_i32() == 0) {
        src = wzr;
      } else {
        src = temps.AcquireW();
        Mov(src.W(), value.to_i32());
      }
      break;
    case kI64:
      if (value.to_i64() == 0) {
        src = xzr;
      } else {
        src = temps.AcquireX();
        Mov(src.X(), value.to_i64());
      }
      break;
    default:
      // We do not track f32 and f64 constants, hence they are unreachable.
      UNREACHABLE();
  }
  Str(src, dst);
}

void LiftoffAssembler::Fill(LiftoffRegister reg, int offset, ValueKind kind) {
  MemOperand src = liftoff::GetStackSlot(offset);
  Ldr(liftoff::GetRegFromType(reg, kind), src);
}

void LiftoffAssembler::FillI64Half(Register, int offset, RegPairHalf) {
  UNREACHABLE();
}

void LiftoffAssembler::FillStackSlotsWithZero(int start, int size) {
  // Zero 'size' bytes *below* start, byte at offset 'start' is untouched.
  DCHECK_LE(0, start);
  DCHECK_LT(0, size);
  DCHECK_EQ(0, size % 4);
  RecordUsedSpillOffset(start + size);

  int max_stp_offset = -start - size;
  // We check IsImmLSUnscaled(-start-12) because str only allows for unscaled
  // 9-bit immediate offset [-256,256]. If start is large enough, which can
  // happen when a function has many params (>=32 i64), str cannot be encoded
  // properly. We can use Str, which will generate more instructions, so
  // fallback to the general case below.
  if (size <= 12 * kStackSlotSize &&
      IsImmLSPair(max_stp_offset, kXRegSizeLog2) &&
      IsImmLSUnscaled(-start - 12)) {
    // Special straight-line code for up to 12 slots. Generates one
    // instruction per two slots (<= 7 instructions total).
    STATIC_ASSERT(kStackSlotSize == kSystemPointerSize);
    uint32_t remainder = size;
    for (; remainder >= 2 * kStackSlotSize; remainder -= 2 * kStackSlotSize) {
      stp(xzr, xzr, liftoff::GetStackSlot(start + remainder));
    }

    DCHECK_GE(12, remainder);
    switch (remainder) {
      case 12:
        str(xzr, liftoff::GetStackSlot(start + remainder));
        str(wzr, liftoff::GetStackSlot(start + remainder - 8));
        break;
      case 8:
        str(xzr, liftoff::GetStackSlot(start + remainder));
        break;
      case 4:
        str(wzr, liftoff::GetStackSlot(start + remainder));
        break;
      case 0:
        break;
      default:
        UNREACHABLE();
    }
  } else {
    // General case for bigger counts (5-8 instructions).
    UseScratchRegisterScope temps(this);
    Register address_reg = temps.AcquireX();
    // This {Sub} might use another temp register if the offset is too large.
    Sub(address_reg, fp, start + size);
    Register count_reg = temps.AcquireX();
    Mov(count_reg, size / 4);

    Label loop;
    bind(&loop);
    sub(count_reg, count_reg, 1);
    str(wzr, MemOperand(address_reg, kSystemPointerSize / 2, PostIndex));
    cbnz(count_reg, &loop);
  }
}

#define I32_BINOP(name, instruction)                             \
  void LiftoffAssembler::emit_##name(Register dst, Register lhs, \
                                     Register rhs) {             \
    instruction(dst.W(), lhs.W(), rhs.W());                      \
  }
#define I32_BINOP_I(name, instruction)                              \
  I32_BINOP(name, instruction)                                      \
  void LiftoffAssembler::emit_##name##i(Register dst, Register lhs, \
                                        int32_t imm) {              \
    instruction(dst.W(), lhs.W(), Immediate(imm));                  \
  }
#define I64_BINOP(name, instruction)                                           \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister lhs, \
                                     LiftoffRegister rhs) {                    \
    instruction(dst.gp().X(), lhs.gp().X(), rhs.gp().X());                     \
  }
#define I64_BINOP_I(name, instruction)                                      \
  I64_BINOP(name, instruction)                                              \
  void LiftoffAssembler::emit_##name##i(LiftoffRegister dst,                \
                                        LiftoffRegister lhs, int32_t imm) { \
    instruction(dst.gp().X(), lhs.gp().X(), imm);                           \
  }
#define FP32_BINOP(name, instruction)                                        \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    instruction(dst.S(), lhs.S(), rhs.S());                                  \
  }
#define FP32_UNOP(name, instruction)                                           \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    instruction(dst.S(), src.S());                                             \
  }
#define FP32_UNOP_RETURN_TRUE(name, instruction)                               \
  bool LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    instruction(dst.S(), src.S());                                             \
    return true;                                                               \
  }
#define FP64_BINOP(name, instruction)                                        \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    instruction(dst.D(), lhs.D(), rhs.D());                                  \
  }
#define FP64_UNOP(name, instruction)                                           \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    instruction(dst.D(), src.D());                                             \
  }
#define FP64_UNOP_RETURN_TRUE(name, instruction)                               \
  bool LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    instruction(dst.D(), src.D());                                             \
    return true;                                                               \
  }
#define I32_SHIFTOP(name, instruction)                              \
  void LiftoffAssembler::emit_##name(Register dst, Register src,    \
                                     Register amount) {             \
    instruction(dst.W(), src.W(), amount.W());                      \
  }                                                                 \
  void LiftoffAssembler::emit_##name##i(Register dst, Register src, \
                                        int32_t amount) {           \
    instruction(dst.W(), src.W(), amount & 31);                     \
  }
#define I64_SHIFTOP(name, instruction)                                         \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister src, \
                                     Register amount) {                        \
    instruction(dst.gp().X(), src.gp().X(), amount.X());                       \
  }                                                                            \
  void LiftoffAssembler::emit_##name##i(LiftoffRegister dst,                   \
                                        LiftoffRegister src, int32_t amount) { \
    instruction(dst.gp().X(), src.gp().X(), amount & 63);                      \
  }

I32_BINOP_I(i32_add, Add)
I32_BINOP_I(i32_sub, Sub)
I32_BINOP(i32_mul, Mul)
I32_BINOP_I(i32_and, And)
I32_BINOP_I(i32_or, Orr)
I32_BINOP_I(i32_xor, Eor)
I32_SHIFTOP(i32_shl, Lsl)
I32_SHIFTOP(i32_sar, Asr)
I32_SHIFTOP(i32_shr, Lsr)
I64_BINOP(i64_add, Add)
I64_BINOP(i64_sub, Sub)
I64_BINOP(i64_mul, Mul)
I64_BINOP_I(i64_and, And)
I64_BINOP_I(i64_or, Orr)
I64_BINOP_I(i64_xor, Eor)
I64_SHIFTOP(i64_shl, Lsl)
I64_SHIFTOP(i64_sar, Asr)
I64_SHIFTOP(i64_shr, Lsr)
FP32_BINOP(f32_add, Fadd)
FP32_BINOP(f32_sub, Fsub)
FP32_BINOP(f32_mul, Fmul)
FP32_BINOP(f32_div, Fdiv)
FP32_BINOP(f32_min, Fmin)
FP32_BINOP(f32_max, Fmax)
FP32_UNOP(f32_abs, Fabs)
FP32_UNOP(f32_neg, Fneg)
FP32_UNOP_RETURN_TRUE(f32_ceil, Frintp)
FP32_UNOP_RETURN_TRUE(f32_floor, Frintm)
FP32_UNOP_RETURN_TRUE(f32_trunc, Frintz)
FP32_UNOP_RETURN_TRUE(f32_nearest_int, Frintn)
FP32_UNOP(f32_sqrt, Fsqrt)
FP64_BINOP(f64_add, Fadd)
FP64_BINOP(f64_sub, Fsub)
FP64_BINOP(f64_mul, Fmul)
FP64_BINOP(f64_div, Fdiv)
FP64_BINOP(f64_min, Fmin)
FP64_BINOP(f64_max, Fmax)
FP64_UNOP(f64_abs, Fabs)
FP64_UNOP(f64_neg, Fneg)
FP64_UNOP_RETURN_TRUE(f64_ceil, Frintp)
FP64_UNOP_RETURN_TRUE(f64_floor, Frintm)
FP64_UNOP_RETURN_TRUE(f64_trunc, Frintz)
FP64_UNOP_RETURN_TRUE(f64_nearest_int, Frintn)
FP64_UNOP(f64_sqrt, Fsqrt)

#undef I32_BINOP
#undef I64_BINOP
#undef FP32_BINOP
#undef FP32_UNOP
#undef FP64_BINOP
#undef FP64_UNOP
#undef FP64_UNOP_RETURN_TRUE
#undef I32_SHIFTOP
#undef I64_SHIFTOP

void LiftoffAssembler::emit_i64_addi(LiftoffRegister dst, LiftoffRegister lhs,
                                     int64_t imm) {
  Add(dst.gp().X(), lhs.gp().X(), imm);
}

void LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  Clz(dst.W(), src.W());
}

void LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  Rbit(dst.W(), src.W());
  Clz(dst.W(), dst.W());
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  PopcntHelper(dst.W(), src.W());
  return true;
}

void LiftoffAssembler::emit_i64_clz(LiftoffRegister dst, LiftoffRegister src) {
  Clz(dst.gp().X(), src.gp().X());
}

void LiftoffAssembler::emit_i64_ctz(LiftoffRegister dst, LiftoffRegister src) {
  Rbit(dst.gp().X(), src.gp().X());
  Clz(dst.gp().X(), dst.gp().X());
}

bool LiftoffAssembler::emit_i64_popcnt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  PopcntHelper(dst.gp().X(), src.gp().X());
  return true;
}

void LiftoffAssembler::IncrementSmi(LiftoffRegister dst, int offset) {
  UseScratchRegisterScope temps(this);
  if (COMPRESS_POINTERS_BOOL) {
    DCHECK(SmiValuesAre31Bits());
    Register scratch = temps.AcquireW();
    Ldr(scratch, MemOperand(dst.gp(), offset));
    Add(scratch, scratch, Operand(Smi::FromInt(1)));
    Str(scratch, MemOperand(dst.gp(), offset));
  } else {
    Register scratch = temps.AcquireX();
    SmiUntag(scratch, MemOperand(dst.gp(), offset));
    Add(scratch, scratch, Operand(1));
    SmiTag(scratch);
    Str(scratch, MemOperand(dst.gp(), offset));
  }
}

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  Register dst_w = dst.W();
  Register lhs_w = lhs.W();
  Register rhs_w = rhs.W();
  bool can_use_dst = !dst_w.Aliases(lhs_w) && !dst_w.Aliases(rhs_w);
  if (can_use_dst) {
    // Do div early.
    Sdiv(dst_w, lhs_w, rhs_w);
  }
  // Check for division by zero.
  Cbz(rhs_w, trap_div_by_zero);
  // Check for kMinInt / -1. This is unrepresentable.
  Cmp(rhs_w, -1);
  Ccmp(lhs_w, 1, NoFlag, eq);
  B(trap_div_unrepresentable, vs);
  if (!can_use_dst) {
    // Do div.
    Sdiv(dst_w, lhs_w, rhs_w);
  }
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  // Check for division by zero.
  Cbz(rhs.W(), trap_div_by_zero);
  // Do div.
  Udiv(dst.W(), lhs.W(), rhs.W());
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  Register dst_w = dst.W();
  Register lhs_w = lhs.W();
  Register rhs_w = rhs.W();
  // Do early div.
  // No need to check kMinInt / -1 because the result is kMinInt and then
  // kMinInt * -1 -> kMinInt. In this case, the Msub result is therefore 0.
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireW();
  Sdiv(scratch, lhs_w, rhs_w);
  // Check for division by zero.
  Cbz(rhs_w, trap_div_by_zero);
  // Compute remainder.
  Msub(dst_w, scratch, rhs_w, lhs_w);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  Register dst_w = dst.W();
  Register lhs_w = lhs.W();
  Register rhs_w = rhs.W();
  // Do early div.
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireW();
  Udiv(scratch, lhs_w, rhs_w);
  // Check for division by zero.
  Cbz(rhs_w, trap_div_by_zero);
  // Compute remainder.
  Msub(dst_w, scratch, rhs_w, lhs_w);
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  Register dst_x = dst.gp().X();
  Register lhs_x = lhs.gp().X();
  Register rhs_x = rhs.gp().X();
  bool can_use_dst = !dst_x.Aliases(lhs_x) && !dst_x.Aliases(rhs_x);
  if (can_use_dst) {
    // Do div early.
    Sdiv(dst_x, lhs_x, rhs_x);
  }
  // Check for division by zero.
  Cbz(rhs_x, trap_div_by_zero);
  // Check for kMinInt / -1. This is unrepresentable.
  Cmp(rhs_x, -1);
  Ccmp(lhs_x, 1, NoFlag, eq);
  B(trap_div_unrepresentable, vs);
  if (!can_use_dst) {
    // Do div.
    Sdiv(dst_x, lhs_x, rhs_x);
  }
  return true;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  // Check for division by zero.
  Cbz(rhs.gp().X(), trap_div_by_zero);
  // Do div.
  Udiv(dst.gp().X(), lhs.gp().X(), rhs.gp().X());
  return true;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  Register dst_x = dst.gp().X();
  Register lhs_x = lhs.gp().X();
  Register rhs_x = rhs.gp().X();
  // Do early div.
  // No need to check kMinInt / -1 because the result is kMinInt and then
  // kMinInt * -1 -> kMinInt. In this case, the Msub result is therefore 0.
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireX();
  Sdiv(scratch, lhs_x, rhs_x);
  // Check for division by zero.
  Cbz(rhs_x, trap_div_by_zero);
  // Compute remainder.
  Msub(dst_x, scratch, rhs_x, lhs_x);
  return true;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  Register dst_x = dst.gp().X();
  Register lhs_x = lhs.gp().X();
  Register rhs_x = rhs.gp().X();
  // Do early div.
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireX();
  Udiv(scratch, lhs_x, rhs_x);
  // Check for division by zero.
  Cbz(rhs_x, trap_div_by_zero);
  // Compute remainder.
  Msub(dst_x, scratch, rhs_x, lhs_x);
  return true;
}

void LiftoffAssembler::emit_u32_to_uintptr(Register dst, Register src) {
  Uxtw(dst, src);
}

void LiftoffAssembler::emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  UseScratchRegisterScope temps(this);
  DoubleRegister scratch = temps.AcquireD();
  Ushr(scratch.V2S(), rhs.V2S(), 31);
  if (dst != lhs) {
    Fmov(dst.S(), lhs.S());
  }
  Sli(dst.V2S(), scratch.V2S(), 31);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  UseScratchRegisterScope temps(this);
  DoubleRegister scratch = temps.AcquireD();
  Ushr(scratch.V1D(), rhs.V1D(), 63);
  if (dst != lhs) {
    Fmov(dst.D(), lhs.D());
  }
  Sli(dst.V1D(), scratch.V1D(), 63);
}

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      Mov(dst.gp().W(), src.gp().W());
      return true;
    case kExprI32SConvertF32:
      Fcvtzs(dst.gp().W(), src.fp().S());  // f32 -> i32 round to zero.
      // Check underflow and NaN.
      Fcmp(src.fp().S(), static_cast<float>(INT32_MIN));
      // Check overflow.
      Ccmp(dst.gp().W(), -1, VFlag, ge);
      B(trap, vs);
      return true;
    case kExprI32UConvertF32:
      Fcvtzu(dst.gp().W(), src.fp().S());  // f32 -> i32 round to zero.
      // Check underflow and NaN.
      Fcmp(src.fp().S(), -1.0);
      // Check overflow.
      Ccmp(dst.gp().W(), -1, ZFlag, gt);
      B(trap, eq);
      return true;
    case kExprI32SConvertF64: {
      // INT32_MIN and INT32_MAX are valid results, we cannot test the result
      // to detect the overflows. We could have done two immediate floating
      // point comparisons but it would have generated two conditional branches.
      UseScratchRegisterScope temps(this);
      VRegister fp_ref = temps.AcquireD();
      VRegister fp_cmp = temps.AcquireD();
      Fcvtzs(dst.gp().W(), src.fp().D());  // f64 -> i32 round to zero.
      Frintz(fp_ref, src.fp().D());        // f64 -> f64 round to zero.
      Scvtf(fp_cmp, dst.gp().W());         // i32 -> f64.
      // If comparison fails, we have an overflow or a NaN.
      Fcmp(fp_cmp, fp_ref);
      B(trap, ne);
      return true;
    }
    case kExprI32UConvertF64: {
      // INT32_MAX is a valid result, we cannot test the result to detect the
      // overflows. We could have done two immediate floating point comparisons
      // but it would have generated two conditional branches.
      UseScratchRegisterScope temps(this);
      VRegister fp_ref = temps.AcquireD();
      VRegister fp_cmp = temps.AcquireD();
      Fcvtzu(dst.gp().W(), src.fp().D());  // f64 -> i32 round to zero.
      Frintz(fp_ref, src.fp().D());        // f64 -> f64 round to zero.
      Ucvtf(fp_cmp, dst.gp().W());         // i32 -> f64.
      // If comparison fails, we have an overflow or a NaN.
      Fcmp(fp_cmp, fp_ref);
      B(trap, ne);
      return true;
    }
    case kExprI32SConvertSatF32:
      Fcvtzs(dst.gp().W(), src.fp().S());
      return true;
    case kExprI32UConvertSatF32:
      Fcvtzu(dst.gp().W(), src.fp().S());
      return true;
    case kExprI32SConvertSatF64:
      Fcvtzs(dst.gp().W(), src.fp().D());
      return true;
    case kExprI32UConvertSatF64:
      Fcvtzu(dst.gp().W(), src.fp().D());
      return true;
    case kExprI64SConvertSatF32:
      Fcvtzs(dst.gp().X(), src.fp().S());
      return true;
    case kExprI64UConvertSatF32:
      Fcvtzu(dst.gp().X(), src.fp().S());
      return true;
    case kExprI64SConvertSatF64:
      Fcvtzs(dst.gp().X(), src.fp().D());
      return true;
    case kExprI64UConvertSatF64:
      Fcvtzu(dst.gp().X(), src.fp().D());
      return true;
    case kExprI32ReinterpretF32:
      Fmov(dst.gp().W(), src.fp().S());
      return true;
    case kExprI64SConvertI32:
      Sxtw(dst.gp().X(), src.gp().W());
      return true;
    case kExprI64SConvertF32:
      Fcvtzs(dst.gp().X(), src.fp().S());  // f32 -> i64 round to zero.
      // Check underflow and NaN.
      Fcmp(src.fp().S(), static_cast<float>(INT64_MIN));
      // Check overflow.
      Ccmp(dst.gp().X(), -1, VFlag, ge);
      B(trap, vs);
      return true;
    case kExprI64UConvertF32:
      Fcvtzu(dst.gp().X(), src.fp().S());  // f32 -> i64 round to zero.
      // Check underflow and NaN.
      Fcmp(src.fp().S(), -1.0);
      // Check overflow.
      Ccmp(dst.gp().X(), -1, ZFlag, gt);
      B(trap, eq);
      return true;
    case kExprI64SConvertF64:
      Fcvtzs(dst.gp().X(), src.fp().D());  // f64 -> i64 round to zero.
      // Check underflow and NaN.
      Fcmp(src.fp().D(), static_cast<float>(INT64_MIN));
      // Check overflow.
      Ccmp(dst.gp().X(), -1, VFlag, ge);
      B(trap, vs);
      return true;
    case kExprI64UConvertF64:
      Fcvtzu(dst.gp().X(), src.fp().D());  // f64 -> i64 round to zero.
      // Check underflow and NaN.
      Fcmp(src.fp().D(), -1.0);
      // Check overflow.
      Ccmp(dst.gp().X(), -1, ZFlag, gt);
      B(trap, eq);
      return true;
    case kExprI64UConvertI32:
      Mov(dst.gp().W(), src.gp().W());
      return true;
    case kExprI64ReinterpretF64:
      Fmov(dst.gp().X(), src.fp().D());
      return true;
    case kExprF32SConvertI32:
      Scvtf(dst.fp().S(), src.gp().W());
      return true;
    case kExprF32UConvertI32:
      Ucvtf(dst.fp().S(), src.gp().W());
      return true;
    case kExprF32SConvertI64:
      Scvtf(dst.fp().S(), src.gp().X());
      return true;
    case kExprF32UConvertI64:
      Ucvtf(dst.fp().S(), src.gp().X());
      return true;
    case kExprF32ConvertF64:
      Fcvt(dst.fp().S(), src.fp().D());
      return true;
    case kExprF32ReinterpretI32:
      Fmov(dst.fp().S(), src.gp().W());
      return true;
    case kExprF64SConvertI32:
      Scvtf(dst.fp().D(), src.gp().W());
      return true;
    case kExprF64UConvertI32:
      Ucvtf(dst.fp().D(), src.gp().W());
      return true;
    case kExprF64SConvertI64:
      Scvtf(dst.fp().D(), src.gp().X());
      return true;
    case kExprF64UConvertI64:
      Ucvtf(dst.fp().D(), src.gp().X());
      return true;
    case kExprF64ConvertF32:
      Fcvt(dst.fp().D(), src.fp().S());
      return true;
    case kExprF64ReinterpretI64:
      Fmov(dst.fp().D(), src.gp().X());
      return true;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  sxtb(dst.W(), src.W());
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  sxth(dst.W(), src.W());
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  sxtb(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  sxth(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  sxtw(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_jump(Label* label) { B(label); }

void LiftoffAssembler::emit_jump(Register target) { Br(target); }

void LiftoffAssembler::emit_cond_jump(LiftoffCondition liftoff_cond,
                                      Label* label, ValueKind kind,
                                      Register lhs, Register rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  switch (kind) {
    case kI32:
      if (rhs.is_valid()) {
        Cmp(lhs.W(), rhs.W());
      } else {
        Cmp(lhs.W(), wzr);
      }
      break;
    case kRef:
    case kOptRef:
    case kRtt:
      DCHECK(rhs.is_valid());
      DCHECK(liftoff_cond == kEqual || liftoff_cond == kUnequal);
      V8_FALLTHROUGH;
    case kI64:
      if (rhs.is_valid()) {
        Cmp(lhs.X(), rhs.X());
      } else {
        Cmp(lhs.X(), xzr);
      }
      break;
    default:
      UNREACHABLE();
  }
  B(label, cond);
}

void LiftoffAssembler::emit_i32_cond_jumpi(LiftoffCondition liftoff_cond,
                                           Label* label, Register lhs,
                                           int32_t imm) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  Cmp(lhs.W(), Operand(imm));
  B(label, cond);
}

void LiftoffAssembler::emit_i32_subi_jump_negative(Register value,
                                                   int subtrahend,
                                                   Label* result_negative) {
  Subs(value.W(), value.W(), Immediate(subtrahend));
  B(result_negative, mi);
}

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  Cmp(src.W(), wzr);
  Cset(dst.W(), eq);
}

void LiftoffAssembler::emit_i32_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, Register lhs,
                                         Register rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  Cmp(lhs.W(), rhs.W());
  Cset(dst.W(), cond);
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  Cmp(src.gp().X(), xzr);
  Cset(dst.W(), eq);
}

void LiftoffAssembler::emit_i64_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  Cmp(lhs.gp().X(), rhs.gp().X());
  Cset(dst.W(), cond);
}

void LiftoffAssembler::emit_f32_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  Fcmp(lhs.S(), rhs.S());
  Cset(dst.W(), cond);
  if (cond != ne) {
    // If V flag set, at least one of the arguments was a Nan -> false.
    Csel(dst.W(), wzr, dst.W(), vs);
  }
}

void LiftoffAssembler::emit_f64_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  Fcmp(lhs.D(), rhs.D());
  Cset(dst.W(), cond);
  if (cond != ne) {
    // If V flag set, at least one of the arguments was a Nan -> false.
    Csel(dst.W(), wzr, dst.W(), vs);
  }
}

bool LiftoffAssembler::emit_select(LiftoffRegister dst, Register condition,
                                   LiftoffRegister true_value,
                                   LiftoffRegister false_value,
                                   ValueKind kind) {
  return false;
}

void LiftoffAssembler::emit_smi_check(Register obj, Label* target,
                                      SmiCheckMode mode) {
  Label* smi_label = mode == kJumpOnSmi ? target : nullptr;
  Label* not_smi_label = mode == kJumpOnNotSmi ? target : nullptr;
  JumpIfSmi(obj, smi_label, not_smi_label);
}

void LiftoffAssembler::LoadTransform(LiftoffRegister dst, Register src_addr,
                                     Register offset_reg, uintptr_t offset_imm,
                                     LoadType type,
                                     LoadTransformationKind transform,
                                     uint32_t* protected_load_pc) {
  UseScratchRegisterScope temps(this);
  MemOperand src_op =
      transform == LoadTransformationKind::kSplat
          ? MemOperand{liftoff::GetEffectiveAddress(this, &temps, src_addr,
                                                    offset_reg, offset_imm)}
          : liftoff::GetMemOp(this, &temps, src_addr, offset_reg, offset_imm);
  *protected_load_pc = pc_offset();
  MachineType memtype = type.mem_type();

  if (transform == LoadTransformationKind::kExtend) {
    if (memtype == MachineType::Int8()) {
      Ldr(dst.fp().D(), src_op);
      Sxtl(dst.fp().V8H(), dst.fp().V8B());
    } else if (memtype == MachineType::Uint8()) {
      Ldr(dst.fp().D(), src_op);
      Uxtl(dst.fp().V8H(), dst.fp().V8B());
    } else if (memtype == MachineType::Int16()) {
      Ldr(dst.fp().D(), src_op);
      Sxtl(dst.fp().V4S(), dst.fp().V4H());
    } else if (memtype == MachineType::Uint16()) {
      Ldr(dst.fp().D(), src_op);
      Uxtl(dst.fp().V4S(), dst.fp().V4H());
    } else if (memtype == MachineType::Int32()) {
      Ldr(dst.fp().D(), src_op);
      Sxtl(dst.fp().V2D(), dst.fp().V2S());
    } else if (memtype == MachineType::Uint32()) {
      Ldr(dst.fp().D(), src_op);
      Uxtl(dst.fp().V2D(), dst.fp().V2S());
    }
  } else if (transform == LoadTransformationKind::kZeroExtend) {
    if (memtype == MachineType::Int32()) {
      Ldr(dst.fp().S(), src_op);
    } else {
      DCHECK_EQ(MachineType::Int64(), memtype);
      Ldr(dst.fp().D(), src_op);
    }
  } else {
    DCHECK_EQ(LoadTransformationKind::kSplat, transform);
    if (memtype == MachineType::Int8()) {
      ld1r(dst.fp().V16B(), src_op);
    } else if (memtype == MachineType::Int16()) {
      ld1r(dst.fp().V8H(), src_op);
    } else if (memtype == MachineType::Int32()) {
      ld1r(dst.fp().V4S(), src_op);
    } else if (memtype == MachineType::Int64()) {
      ld1r(dst.fp().V2D(), src_op);
    }
  }
}

void LiftoffAssembler::LoadLane(LiftoffRegister dst, LiftoffRegister src,
                                Register addr, Register offset_reg,
                                uintptr_t offset_imm, LoadType type,
                                uint8_t laneidx, uint32_t* protected_load_pc) {
  UseScratchRegisterScope temps(this);
  MemOperand src_op{
      liftoff::GetEffectiveAddress(this, &temps, addr, offset_reg, offset_imm)};

  MachineType mem_type = type.mem_type();
  if (dst != src) {
    Mov(dst.fp().Q(), src.fp().Q());
  }

  *protected_load_pc = pc_offset();
  if (mem_type == MachineType::Int8()) {
    ld1(dst.fp().B(), laneidx, src_op);
  } else if (mem_type == MachineType::Int16()) {
    ld1(dst.fp().H(), laneidx, src_op);
  } else if (mem_type == MachineType::Int32()) {
    ld1(dst.fp().S(), laneidx, src_op);
  } else if (mem_type == MachineType::Int64()) {
    ld1(dst.fp().D(), laneidx, src_op);
  } else {
    UNREACHABLE();
  }
}

void LiftoffAssembler::StoreLane(Register dst, Register offset,
                                 uintptr_t offset_imm, LiftoffRegister src,
                                 StoreType type, uint8_t lane,
                                 uint32_t* protected_store_pc) {
  UseScratchRegisterScope temps(this);
  MemOperand dst_op{
      liftoff::GetEffectiveAddress(this, &temps, dst, offset, offset_imm)};
  if (protected_store_pc) *protected_store_pc = pc_offset();

  MachineRepresentation rep = type.mem_rep();
  if (rep == MachineRepresentation::kWord8) {
    st1(src.fp().B(), lane, dst_op);
  } else if (rep == MachineRepresentation::kWord16) {
    st1(src.fp().H(), lane, dst_op);
  } else if (rep == MachineRepresentation::kWord32) {
    st1(src.fp().S(), lane, dst_op);
  } else {
    DCHECK_EQ(MachineRepresentation::kWord64, rep);
    st1(src.fp().D(), lane, dst_op);
  }
}

void LiftoffAssembler::emit_i8x16_swizzle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs) {
  Tbl(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_f64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Dup(dst.fp().V2D(), src.fp().D(), 0);
}

void LiftoffAssembler::emit_f64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  Mov(dst.fp().D(), lhs.fp().V2D(), imm_lane_idx);
}

void LiftoffAssembler::emit_f64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (dst != src1) {
    Mov(dst.fp().V2D(), src1.fp().V2D());
  }
  Mov(dst.fp().V2D(), imm_lane_idx, src2.fp().V2D(), 0);
}

void LiftoffAssembler::emit_f64x2_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Fabs(dst.fp().V2D(), src.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Fneg(dst.fp().V2D(), src.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_sqrt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  Fsqrt(dst.fp().V2D(), src.fp().V2D());
}

bool LiftoffAssembler::emit_f64x2_ceil(LiftoffRegister dst,
                                       LiftoffRegister src) {
  Frintp(dst.fp().V2D(), src.fp().V2D());
  return true;
}

bool LiftoffAssembler::emit_f64x2_floor(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Frintm(dst.fp().V2D(), src.fp().V2D());
  return true;
}

bool LiftoffAssembler::emit_f64x2_trunc(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Frintz(dst.fp().V2D(), src.fp().V2D());
  return true;
}

bool LiftoffAssembler::emit_f64x2_nearest_int(LiftoffRegister dst,
                                              LiftoffRegister src) {
  Frintn(dst.fp().V2D(), src.fp().V2D());
  return true;
}

void LiftoffAssembler::emit_f64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fadd(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fsub(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fmul(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_div(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fdiv(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fmin(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fmax(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_pmin(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  UseScratchRegisterScope temps(this);

  VRegister tmp = dst.fp();
  if (dst == lhs || dst == rhs) {
    tmp = temps.AcquireV(kFormat2D);
  }

  Fcmgt(tmp.V2D(), lhs.fp().V2D(), rhs.fp().V2D());
  Bsl(tmp.V16B(), rhs.fp().V16B(), lhs.fp().V16B());

  if (dst == lhs || dst == rhs) {
    Mov(dst.fp().V2D(), tmp);
  }
}

void LiftoffAssembler::emit_f64x2_pmax(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  UseScratchRegisterScope temps(this);

  VRegister tmp = dst.fp();
  if (dst == lhs || dst == rhs) {
    tmp = temps.AcquireV(kFormat2D);
  }

  Fcmgt(tmp.V2D(), rhs.fp().V2D(), lhs.fp().V2D());
  Bsl(tmp.V16B(), rhs.fp().V16B(), lhs.fp().V16B());

  if (dst == lhs || dst == rhs) {
    Mov(dst.fp().V2D(), tmp);
  }
}

void LiftoffAssembler::emit_f64x2_convert_low_i32x4_s(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  Sxtl(dst.fp().V2D(), src.fp().V2S());
  Scvtf(dst.fp().V2D(), dst.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_convert_low_i32x4_u(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  Uxtl(dst.fp().V2D(), src.fp().V2S());
  Ucvtf(dst.fp().V2D(), dst.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_promote_low_f32x4(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  Fcvtl(dst.fp().V2D(), src.fp().V2S());
}

void LiftoffAssembler::emit_f32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Dup(dst.fp().V4S(), src.fp().S(), 0);
}

void LiftoffAssembler::emit_f32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  Mov(dst.fp().S(), lhs.fp().V4S(), imm_lane_idx);
}

void LiftoffAssembler::emit_f32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (dst != src1) {
    Mov(dst.fp().V4S(), src1.fp().V4S());
  }
  Mov(dst.fp().V4S(), imm_lane_idx, src2.fp().V4S(), 0);
}

void LiftoffAssembler::emit_f32x4_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Fabs(dst.fp().V4S(), src.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Fneg(dst.fp().V4S(), src.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_sqrt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  Fsqrt(dst.fp().V4S(), src.fp().V4S());
}

bool LiftoffAssembler::emit_f32x4_ceil(LiftoffRegister dst,
                                       LiftoffRegister src) {
  Frintp(dst.fp().V4S(), src.fp().V4S());
  return true;
}

bool LiftoffAssembler::emit_f32x4_floor(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Frintm(dst.fp().V4S(), src.fp().V4S());
  return true;
}

bool LiftoffAssembler::emit_f32x4_trunc(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Frintz(dst.fp().V4S(), src.fp().V4S());
  return true;
}

bool LiftoffAssembler::emit_f32x4_nearest_int(LiftoffRegister dst,
                                              LiftoffRegister src) {
  Frintn(dst.fp().V4S(), src.fp().V4S());
  return true;
}

void LiftoffAssembler::emit_f32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fadd(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fsub(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fmul(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_div(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fdiv(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fmin(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Fmax(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_pmin(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  UseScratchRegisterScope temps(this);

  VRegister tmp = dst.fp();
  if (dst == lhs || dst == rhs) {
    tmp = temps.AcquireV(kFormat4S);
  }

  Fcmgt(tmp.V4S(), lhs.fp().V4S(), rhs.fp().V4S());
  Bsl(tmp.V16B(), rhs.fp().V16B(), lhs.fp().V16B());

  if (dst == lhs || dst == rhs) {
    Mov(dst.fp().V4S(), tmp);
  }
}

void LiftoffAssembler::emit_f32x4_pmax(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  UseScratchRegisterScope temps(this);

  VRegister tmp = dst.fp();
  if (dst == lhs || dst == rhs) {
    tmp = temps.AcquireV(kFormat4S);
  }

  Fcmgt(tmp.V4S(), rhs.fp().V4S(), lhs.fp().V4S());
  Bsl(tmp.V16B(), rhs.fp().V16B(), lhs.fp().V16B());

  if (dst == lhs || dst == rhs) {
    Mov(dst.fp().V4S(), tmp);
  }
}

void LiftoffAssembler::emit_i64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Dup(dst.fp().V2D(), src.gp().X());
}

void LiftoffAssembler::emit_i64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  Mov(dst.gp().X(), lhs.fp().V2D(), imm_lane_idx);
}

void LiftoffAssembler::emit_i64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (dst != src1) {
    Mov(dst.fp().V2D(), src1.fp().V2D());
  }
  Mov(dst.fp().V2D(), imm_lane_idx, src2.gp().X());
}

void LiftoffAssembler::emit_i64x2_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Neg(dst.fp().V2D(), src.fp().V2D());
}

void LiftoffAssembler::emit_i64x2_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  I64x2AllTrue(dst.gp(), src.fp());
}

void LiftoffAssembler::emit_i64x2_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kLeft>(
      this, dst.fp().V2D(), lhs.fp().V2D(), rhs.gp(), kFormat2D);
}

void LiftoffAssembler::emit_i64x2_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  Shl(dst.fp().V2D(), lhs.fp().V2D(), rhs & 63);
}

void LiftoffAssembler::emit_i64x2_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kRight,
                         liftoff::ShiftSign::kSigned>(
      this, dst.fp().V2D(), lhs.fp().V2D(), rhs.gp(), kFormat2D);
}

void LiftoffAssembler::emit_i64x2_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  liftoff::EmitSimdShiftRightImmediate<kFormat2D, liftoff::ShiftSign::kSigned>(
      this, dst.fp().V2D(), lhs.fp().V2D(), rhs);
}

void LiftoffAssembler::emit_i64x2_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kRight,
                         liftoff::ShiftSign::kUnsigned>(
      this, dst.fp().V2D(), lhs.fp().V2D(), rhs.gp(), kFormat2D);
}

void LiftoffAssembler::emit_i64x2_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  liftoff::EmitSimdShiftRightImmediate<kFormat2D,
                                       liftoff::ShiftSign::kUnsigned>(
      this, dst.fp().V2D(), lhs.fp().V2D(), rhs);
}

void LiftoffAssembler::emit_i64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Add(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_i64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Sub(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_i64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  UseScratchRegisterScope temps(this);
  VRegister tmp1 = temps.AcquireV(kFormat2D);
  VRegister tmp2 = temps.AcquireV(kFormat2D);

  // Algorithm copied from code-generator-arm64.cc with minor modifications:
  // - 2 (max number of scratch registers in Liftoff) temporaries instead of 3
  // - 1 more Umull instruction to calculate | cg | ae |,
  // - so, we can no longer use Umlal in the last step, and use Add instead.
  // Refer to comments there for details.
  Xtn(tmp1.V2S(), lhs.fp().V2D());
  Xtn(tmp2.V2S(), rhs.fp().V2D());
  Umull(tmp1.V2D(), tmp1.V2S(), tmp2.V2S());
  Rev64(tmp2.V4S(), rhs.fp().V4S());
  Mul(tmp2.V4S(), tmp2.V4S(), lhs.fp().V4S());
  Addp(tmp2.V4S(), tmp2.V4S(), tmp2.V4S());
  Shll(dst.fp().V2D(), tmp2.V2S(), 32);
  Add(dst.fp().V2D(), dst.fp().V2D(), tmp1.V2D());
}

void LiftoffAssembler::emit_i64x2_extmul_low_i32x4_s(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  Smull(dst.fp().V2D(), src1.fp().V2S(), src2.fp().V2S());
}

void LiftoffAssembler::emit_i64x2_extmul_low_i32x4_u(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  Umull(dst.fp().V2D(), src1.fp().V2S(), src2.fp().V2S());
}

void LiftoffAssembler::emit_i64x2_extmul_high_i32x4_s(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  Smull2(dst.fp().V2D(), src1.fp().V4S(), src2.fp().V4S());
}

void LiftoffAssembler::emit_i64x2_extmul_high_i32x4_u(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  Umull2(dst.fp().V2D(), src1.fp().V4S(), src2.fp().V4S());
}

void LiftoffAssembler::emit_i64x2_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  I64x2BitMask(dst.gp(), src.fp());
}

void LiftoffAssembler::emit_i64x2_sconvert_i32x4_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  Sxtl(dst.fp().V2D(), src.fp().V2S());
}

void LiftoffAssembler::emit_i64x2_sconvert_i32x4_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  Sxtl2(dst.fp().V2D(), src.fp().V4S());
}

void LiftoffAssembler::emit_i64x2_uconvert_i32x4_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  Uxtl(dst.fp().V2D(), src.fp().V2S());
}

void LiftoffAssembler::emit_i64x2_uconvert_i32x4_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  Uxtl2(dst.fp().V2D(), src.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Dup(dst.fp().V4S(), src.gp().W());
}

void LiftoffAssembler::emit_i32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  Mov(dst.gp().W(), lhs.fp().V4S(), imm_lane_idx);
}

void LiftoffAssembler::emit_i32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (dst != src1) {
    Mov(dst.fp().V4S(), src1.fp().V4S());
  }
  Mov(dst.fp().V4S(), imm_lane_idx, src2.gp().W());
}

void LiftoffAssembler::emit_i32x4_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Neg(dst.fp().V4S(), src.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  liftoff::EmitAllTrue(this, dst, src, kFormat4S);
}

void LiftoffAssembler::emit_i32x4_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  UseScratchRegisterScope temps(this);
  VRegister tmp = temps.AcquireQ();
  VRegister mask = temps.AcquireQ();

  Sshr(tmp.V4S(), src.fp().V4S(), 31);
  // Set i-th bit of each lane i. When AND with tmp, the lanes that
  // are signed will have i-th bit set, unsigned will be 0.
  Movi(mask.V2D(), 0x0000'0008'0000'0004, 0x0000'0002'0000'0001);
  And(tmp.V16B(), mask.V16B(), tmp.V16B());
  Addv(tmp.S(), tmp.V4S());
  Mov(dst.gp().W(), tmp.V4S(), 0);
}

void LiftoffAssembler::emit_i32x4_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kLeft>(
      this, dst.fp().V4S(), lhs.fp().V4S(), rhs.gp(), kFormat4S);
}

void LiftoffAssembler::emit_i32x4_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  Shl(dst.fp().V4S(), lhs.fp().V4S(), rhs & 31);
}

void LiftoffAssembler::emit_i32x4_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kRight,
                         liftoff::ShiftSign::kSigned>(
      this, dst.fp().V4S(), lhs.fp().V4S(), rhs.gp(), kFormat4S);
}

void LiftoffAssembler::emit_i32x4_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  liftoff::EmitSimdShiftRightImmediate<kFormat4S, liftoff::ShiftSign::kSigned>(
      this, dst.fp().V4S(), lhs.fp().V4S(), rhs);
}

void LiftoffAssembler::emit_i32x4_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kRight,
                         liftoff::ShiftSign::kUnsigned>(
      this, dst.fp().V4S(), lhs.fp().V4S(), rhs.gp(), kFormat4S);
}

void LiftoffAssembler::emit_i32x4_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  liftoff::EmitSimdShiftRightImmediate<kFormat4S,
                                       liftoff::ShiftSign::kUnsigned>(
      this, dst.fp().V4S(), lhs.fp().V4S(), rhs);
}

void LiftoffAssembler::emit_i32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Add(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Sub(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Mul(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Smin(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Umin(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Smax(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Umax(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_dot_i16x8_s(LiftoffRegister dst,
                                              LiftoffRegister lhs,
                                              LiftoffRegister rhs) {
  UseScratchRegisterScope scope(this);
  VRegister tmp1 = scope.AcquireV(kFormat4S);
  VRegister tmp2 = scope.AcquireV(kFormat4S);
  Smull(tmp1, lhs.fp().V4H(), rhs.fp().V4H());
  Smull2(tmp2, lhs.fp().V8H(), rhs.fp().V8H());
  Addp(dst.fp().V4S(), tmp1, tmp2);
}

void LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_s(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  Saddlp(dst.fp().V4S(), src.fp().V8H());
}

void LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_u(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  Uaddlp(dst.fp().V4S(), src.fp().V8H());
}

void LiftoffAssembler::emit_i32x4_extmul_low_i16x8_s(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  Smull(dst.fp().V4S(), src1.fp().V4H(), src2.fp().V4H());
}

void LiftoffAssembler::emit_i32x4_extmul_low_i16x8_u(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  Umull(dst.fp().V4S(), src1.fp().V4H(), src2.fp().V4H());
}

void LiftoffAssembler::emit_i32x4_extmul_high_i16x8_s(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  Smull2(dst.fp().V4S(), src1.fp().V8H(), src2.fp().V8H());
}

void LiftoffAssembler::emit_i32x4_extmul_high_i16x8_u(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  Umull2(dst.fp().V4S(), src1.fp().V8H(), src2.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Dup(dst.fp().V8H(), src.gp().W());
}

void LiftoffAssembler::emit_i16x8_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  Umov(dst.gp().W(), lhs.fp().V8H(), imm_lane_idx);
}

void LiftoffAssembler::emit_i16x8_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  Smov(dst.gp().W(), lhs.fp().V8H(), imm_lane_idx);
}

void LiftoffAssembler::emit_i16x8_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (dst != src1) {
    Mov(dst.fp().V8H(), src1.fp().V8H());
  }
  Mov(dst.fp().V8H(), imm_lane_idx, src2.gp().W());
}

void LiftoffAssembler::emit_i16x8_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Neg(dst.fp().V8H(), src.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  liftoff::EmitAllTrue(this, dst, src, kFormat8H);
}

void LiftoffAssembler::emit_i16x8_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  UseScratchRegisterScope temps(this);
  VRegister tmp = temps.AcquireQ();
  VRegister mask = temps.AcquireQ();

  Sshr(tmp.V8H(), src.fp().V8H(), 15);
  // Set i-th bit of each lane i. When AND with tmp, the lanes that
  // are signed will have i-th bit set, unsigned will be 0.
  Movi(mask.V2D(), 0x0080'0040'0020'0010, 0x0008'0004'0002'0001);
  And(tmp.V16B(), mask.V16B(), tmp.V16B());
  Addv(tmp.H(), tmp.V8H());
  Mov(dst.gp().W(), tmp.V8H(), 0);
}

void LiftoffAssembler::emit_i16x8_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kLeft>(
      this, dst.fp().V8H(), lhs.fp().V8H(), rhs.gp(), kFormat8H);
}

void LiftoffAssembler::emit_i16x8_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  Shl(dst.fp().V8H(), lhs.fp().V8H(), rhs & 15);
}

void LiftoffAssembler::emit_i16x8_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kRight,
                         liftoff::ShiftSign::kSigned>(
      this, dst.fp().V8H(), lhs.fp().V8H(), rhs.gp(), kFormat8H);
}

void LiftoffAssembler::emit_i16x8_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  liftoff::EmitSimdShiftRightImmediate<kFormat8H, liftoff::ShiftSign::kSigned>(
      this, dst.fp().V8H(), lhs.fp().V8H(), rhs);
}

void LiftoffAssembler::emit_i16x8_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kRight,
                         liftoff::ShiftSign::kUnsigned>(
      this, dst.fp().V8H(), lhs.fp().V8H(), rhs.gp(), kFormat8H);
}

void LiftoffAssembler::emit_i16x8_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  liftoff::EmitSimdShiftRightImmediate<kFormat8H,
                                       liftoff::ShiftSign::kUnsigned>(
      this, dst.fp().V8H(), lhs.fp().V8H(), rhs);
}

void LiftoffAssembler::emit_i16x8_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Add(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_add_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  Sqadd(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Sub(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_sub_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  Sqsub(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_sub_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  Uqsub(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Mul(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_add_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  Uqadd(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Smin(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Umin(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Smax(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Umax(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i8x16_shuffle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs,
                                          const uint8_t shuffle[16],
                                          bool is_swizzle) {
  VRegister src1 = lhs.fp();
  VRegister src2 = rhs.fp();
  VRegister temp = dst.fp();
  if (dst == lhs || dst == rhs) {
    // dst overlaps with lhs or rhs, so we need a temporary.
    temp = GetUnusedRegister(kFpReg, LiftoffRegList{lhs, rhs}).fp();
  }

  UseScratchRegisterScope scope(this);

  if (src1 != src2 && !AreConsecutive(src1, src2)) {
    // Tbl needs consecutive registers, which our scratch registers are.
    src1 = scope.AcquireV(kFormat16B);
    src2 = scope.AcquireV(kFormat16B);
    DCHECK(AreConsecutive(src1, src2));
    Mov(src1.Q(), lhs.fp().Q());
    Mov(src2.Q(), rhs.fp().Q());
  }

  int64_t imms[2] = {0, 0};
  for (int i = 7; i >= 0; i--) {
    imms[0] = (imms[0] << 8) | (shuffle[i]);
    imms[1] = (imms[1] << 8) | (shuffle[i + 8]);
  }
  DCHECK_EQ(0, (imms[0] | imms[1]) &
                   (lhs == rhs ? 0xF0F0F0F0F0F0F0F0 : 0xE0E0E0E0E0E0E0E0));

  Movi(temp.V16B(), imms[1], imms[0]);

  if (src1 == src2) {
    Tbl(dst.fp().V16B(), src1.V16B(), temp.V16B());
  } else {
    Tbl(dst.fp().V16B(), src1.V16B(), src2.V16B(), temp.V16B());
  }
}

void LiftoffAssembler::emit_i8x16_popcnt(LiftoffRegister dst,
                                         LiftoffRegister src) {
  Cnt(dst.fp().V16B(), src.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  Dup(dst.fp().V16B(), src.gp().W());
}

void LiftoffAssembler::emit_i8x16_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  Umov(dst.gp().W(), lhs.fp().V16B(), imm_lane_idx);
}

void LiftoffAssembler::emit_i8x16_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  Smov(dst.gp().W(), lhs.fp().V16B(), imm_lane_idx);
}

void LiftoffAssembler::emit_i8x16_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (dst != src1) {
    Mov(dst.fp().V16B(), src1.fp().V16B());
  }
  Mov(dst.fp().V16B(), imm_lane_idx, src2.gp().W());
}

void LiftoffAssembler::emit_i8x16_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Neg(dst.fp().V16B(), src.fp().V16B());
}

void LiftoffAssembler::emit_v128_anytrue(LiftoffRegister dst,
                                         LiftoffRegister src) {
  liftoff::EmitAnyTrue(this, dst, src);
}

void LiftoffAssembler::emit_i8x16_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  liftoff::EmitAllTrue(this, dst, src, kFormat16B);
}

void LiftoffAssembler::emit_i8x16_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  UseScratchRegisterScope temps(this);
  VRegister tmp = temps.AcquireQ();
  VRegister mask = temps.AcquireQ();

  // Set i-th bit of each lane i. When AND with tmp, the lanes that
  // are signed will have i-th bit set, unsigned will be 0.
  Sshr(tmp.V16B(), src.fp().V16B(), 7);
  Movi(mask.V2D(), 0x8040'2010'0804'0201);
  And(tmp.V16B(), mask.V16B(), tmp.V16B());
  Ext(mask.V16B(), tmp.V16B(), tmp.V16B(), 8);
  Zip1(tmp.V16B(), tmp.V16B(), mask.V16B());
  Addv(tmp.H(), tmp.V8H());
  Mov(dst.gp().W(), tmp.V8H(), 0);
}

void LiftoffAssembler::emit_i8x16_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kLeft>(
      this, dst.fp().V16B(), lhs.fp().V16B(), rhs.gp(), kFormat16B);
}

void LiftoffAssembler::emit_i8x16_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  Shl(dst.fp().V16B(), lhs.fp().V16B(), rhs & 7);
}

void LiftoffAssembler::emit_i8x16_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kRight,
                         liftoff::ShiftSign::kSigned>(
      this, dst.fp().V16B(), lhs.fp().V16B(), rhs.gp(), kFormat16B);
}

void LiftoffAssembler::emit_i8x16_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  liftoff::EmitSimdShiftRightImmediate<kFormat16B, liftoff::ShiftSign::kSigned>(
      this, dst.fp().V16B(), lhs.fp().V16B(), rhs);
}

void LiftoffAssembler::emit_i8x16_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  liftoff::EmitSimdShift<liftoff::ShiftDirection::kRight,
                         liftoff::ShiftSign::kUnsigned>(
      this, dst.fp().V16B(), lhs.fp().V16B(), rhs.gp(), kFormat16B);
}

void LiftoffAssembler::emit_i8x16_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  liftoff::EmitSimdShiftRightImmediate<kFormat16B,
                                       liftoff::ShiftSign::kUnsigned>(
      this, dst.fp().V16B(), lhs.fp().V16B(), rhs);
}

void LiftoffAssembler::emit_i8x16_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Add(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_add_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  Sqadd(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  Sub(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_sub_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  Sqsub(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_sub_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  Uqsub(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_add_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  Uqadd(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Smin(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Umin(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Smax(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  Umax(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Cmeq(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Cmeq(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
  Mvn(dst.fp().V16B(), dst.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmgt(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmhi(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmge(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmhs(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i16x8_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Cmeq(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Cmeq(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
  Mvn(dst.fp().V8H(), dst.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmgt(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmhi(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmge(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmhs(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i32x4_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Cmeq(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Cmeq(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
  Mvn(dst.fp().V4S(), dst.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmgt(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmhi(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmge(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmhs(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_i64x2_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Cmeq(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_i64x2_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Cmeq(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
  Mvn(dst.fp().V2D(), dst.fp().V2D());
}

void LiftoffAssembler::emit_i64x2_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmgt(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_i64x2_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  Cmge(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_f32x4_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Fcmeq(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Fcmeq(dst.fp().V4S(), lhs.fp().V4S(), rhs.fp().V4S());
  Mvn(dst.fp().V4S(), dst.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_lt(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Fcmgt(dst.fp().V4S(), rhs.fp().V4S(), lhs.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_le(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Fcmge(dst.fp().V4S(), rhs.fp().V4S(), lhs.fp().V4S());
}

void LiftoffAssembler::emit_f64x2_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Fcmeq(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Fcmeq(dst.fp().V2D(), lhs.fp().V2D(), rhs.fp().V2D());
  Mvn(dst.fp().V2D(), dst.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_lt(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Fcmgt(dst.fp().V2D(), rhs.fp().V2D(), lhs.fp().V2D());
}

void LiftoffAssembler::emit_f64x2_le(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Fcmge(dst.fp().V2D(), rhs.fp().V2D(), lhs.fp().V2D());
}

void LiftoffAssembler::emit_s128_const(LiftoffRegister dst,
                                       const uint8_t imms[16]) {
  uint64_t vals[2];
  memcpy(vals, imms, sizeof(vals));
  Movi(dst.fp().V16B(), vals[1], vals[0]);
}

void LiftoffAssembler::emit_s128_not(LiftoffRegister dst, LiftoffRegister src) {
  Mvn(dst.fp().V16B(), src.fp().V16B());
}

void LiftoffAssembler::emit_s128_and(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  And(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_s128_or(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  Orr(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_s128_xor(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  Eor(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_s128_select(LiftoffRegister dst,
                                        LiftoffRegister src1,
                                        LiftoffRegister src2,
                                        LiftoffRegister mask) {
  if (dst != mask) {
    Mov(dst.fp().V16B(), mask.fp().V16B());
  }
  Bsl(dst.fp().V16B(), src1.fp().V16B(), src2.fp().V16B());
}

void LiftoffAssembler::emit_i32x4_sconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  Fcvtzs(dst.fp().V4S(), src.fp().V4S());
}

void LiftoffAssembler::emit_i32x4_uconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  Fcvtzu(dst.fp().V4S(), src.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  Scvtf(dst.fp().V4S(), src.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  Ucvtf(dst.fp().V4S(), src.fp().V4S());
}

void LiftoffAssembler::emit_f32x4_demote_f64x2_zero(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  Fcvtn(dst.fp().V2S(), src.fp().V2D());
}

void LiftoffAssembler::emit_i8x16_sconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  UseScratchRegisterScope temps(this);
  VRegister tmp = temps.AcquireV(kFormat8H);
  VRegister right = rhs.fp().V8H();
  if (dst == rhs) {
    Mov(tmp, right);
    right = tmp;
  }
  Sqxtn(dst.fp().V8B(), lhs.fp().V8H());
  Sqxtn2(dst.fp().V16B(), right);
}

void LiftoffAssembler::emit_i8x16_uconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  UseScratchRegisterScope temps(this);
  VRegister tmp = temps.AcquireV(kFormat8H);
  VRegister right = rhs.fp().V8H();
  if (dst == rhs) {
    Mov(tmp, right);
    right = tmp;
  }
  Sqxtun(dst.fp().V8B(), lhs.fp().V8H());
  Sqxtun2(dst.fp().V16B(), right);
}

void LiftoffAssembler::emit_i16x8_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  UseScratchRegisterScope temps(this);
  VRegister tmp = temps.AcquireV(kFormat4S);
  VRegister right = rhs.fp().V4S();
  if (dst == rhs) {
    Mov(tmp, right);
    right = tmp;
  }
  Sqxtn(dst.fp().V4H(), lhs.fp().V4S());
  Sqxtn2(dst.fp().V8H(), right);
}

void LiftoffAssembler::emit_i16x8_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  UseScratchRegisterScope temps(this);
  VRegister tmp = temps.AcquireV(kFormat4S);
  VRegister right = rhs.fp().V4S();
  if (dst == rhs) {
    Mov(tmp, right);
    right = tmp;
  }
  Sqxtun(dst.fp().V4H(), lhs.fp().V4S());
  Sqxtun2(dst.fp().V8H(), right);
}

void LiftoffAssembler::emit_i16x8_sconvert_i8x16_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  Sxtl(dst.fp().V8H(), src.fp().V8B());
}

void LiftoffAssembler::emit_i16x8_sconvert_i8x16_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  Sxtl2(dst.fp().V8H(), src.fp().V16B());
}

void LiftoffAssembler::emit_i16x8_uconvert_i8x16_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  Uxtl(dst.fp().V8H(), src.fp().V8B());
}

void LiftoffAssembler::emit_i16x8_uconvert_i8x16_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  Uxtl2(dst.fp().V8H(), src.fp().V16B());
}

void LiftoffAssembler::emit_i32x4_sconvert_i16x8_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  Sxtl(dst.fp().V4S(), src.fp().V4H());
}

void LiftoffAssembler::emit_i32x4_sconvert_i16x8_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  Sxtl2(dst.fp().V4S(), src.fp().V8H());
}

void LiftoffAssembler::emit_i32x4_uconvert_i16x8_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  Uxtl(dst.fp().V4S(), src.fp().V4H());
}

void LiftoffAssembler::emit_i32x4_uconvert_i16x8_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  Uxtl2(dst.fp().V4S(), src.fp().V8H());
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_s_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  Fcvtzs(dst.fp().V2D(), src.fp().V2D());
  Sqxtn(dst.fp().V2S(), dst.fp().V2D());
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_u_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  Fcvtzu(dst.fp().V2D(), src.fp().V2D());
  Uqxtn(dst.fp().V2S(), dst.fp().V2D());
}

void LiftoffAssembler::emit_s128_and_not(LiftoffRegister dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  Bic(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i8x16_rounding_average_u(LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
  Urhadd(dst.fp().V16B(), lhs.fp().V16B(), rhs.fp().V16B());
}

void LiftoffAssembler::emit_i16x8_rounding_average_u(LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
  Urhadd(dst.fp().V8H(), lhs.fp().V8H(), rhs.fp().V8H());
}

void LiftoffAssembler::emit_i8x16_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Abs(dst.fp().V16B(), src.fp().V16B());
}

void LiftoffAssembler::emit_i16x8_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Abs(dst.fp().V8H(), src.fp().V8H());
}

void LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_s(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  Saddlp(dst.fp().V8H(), src.fp().V16B());
}

void LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_u(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  Uaddlp(dst.fp().V8H(), src.fp().V16B());
}

void LiftoffAssembler::emit_i16x8_extmul_low_i8x16_s(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  Smull(dst.fp().V8H(), src1.fp().V8B(), src2.fp().V8B());
}

void LiftoffAssembler::emit_i16x8_extmul_low_i8x16_u(LiftoffRegister dst,
                                                     LiftoffRegister src1,
                                                     LiftoffRegister src2) {
  Umull(dst.fp().V8H(), src1.fp().V8B(), src2.fp().V8B());
}

void LiftoffAssembler::emit_i16x8_extmul_high_i8x16_s(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  Smull2(dst.fp().V8H(), src1.fp().V16B(), src2.fp().V16B());
}

void LiftoffAssembler::emit_i16x8_extmul_high_i8x16_u(LiftoffRegister dst,
                                                      LiftoffRegister src1,
                                                      LiftoffRegister src2) {
  Umull2(dst.fp().V8H(), src1.fp().V16B(), src2.fp().V16B());
}

void LiftoffAssembler::emit_i16x8_q15mulr_sat_s(LiftoffRegister dst,
                                                LiftoffRegister src1,
                                                LiftoffRegister src2) {
  Sqrdmulh(dst.fp().V8H(), src1.fp().V8H(), src2.fp().V8H());
}

void LiftoffAssembler::emit_i32x4_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Abs(dst.fp().V4S(), src.fp().V4S());
}

void LiftoffAssembler::emit_i64x2_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  Abs(dst.fp().V2D(), src.fp().V2D());
}

void LiftoffAssembler::StackCheck(Label* ool_code, Register limit_address) {
  Ldr(limit_address, MemOperand(limit_address));
  Cmp(sp, limit_address);
  B(ool_code, ls);
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  CallCFunction(ExternalReference::wasm_call_trap_callback_for_testing(), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  TurboAssembler::AssertUnreachable(reason);
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  PushCPURegList(liftoff::PadRegList(regs.GetGpList()));
  PushCPURegList(liftoff::PadVRegList(regs.GetFpList()));
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  PopCPURegList(liftoff::PadVRegList(regs.GetFpList()));
  PopCPURegList(liftoff::PadRegList(regs.GetGpList()));
}

void LiftoffAssembler::RecordSpillsInSafepoint(
    SafepointTableBuilder::Safepoint& safepoint, LiftoffRegList all_spills,
    LiftoffRegList ref_spills, int spill_offset) {
  int spill_space_size = 0;
  bool needs_padding = (all_spills.GetGpList().Count() & 1) != 0;
  if (needs_padding) {
    spill_space_size += kSystemPointerSize;
    ++spill_offset;
  }
  while (!all_spills.is_empty()) {
    LiftoffRegister reg = all_spills.GetLastRegSet();
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
  DropSlots(num_stack_slots);
  Ret();
}

void LiftoffAssembler::CallC(const ValueKindSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueKind out_argument_kind, int stack_bytes,
                             ExternalReference ext_ref) {
  // The stack pointer is required to be quadword aligned.
  int total_size = RoundUp(stack_bytes, kQuadWordSizeInBytes);
  // Reserve space in the stack.
  Claim(total_size, 1);

  int arg_bytes = 0;
  for (ValueKind param_kind : sig->parameters()) {
    Poke(liftoff::GetRegFromType(*args++, param_kind), arg_bytes);
    arg_bytes += value_kind_size(param_kind);
  }
  DCHECK_LE(arg_bytes, stack_bytes);

  // Pass a pointer to the buffer with the arguments to the C function.
  Mov(x0, sp);

  // Now call the C function.
  constexpr int kNumCCallArgs = 1;
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* next_result_reg = rets;
  if (sig->return_count() > 0) {
    DCHECK_EQ(1, sig->return_count());
    constexpr Register kReturnReg = x0;
    if (kReturnReg != next_result_reg->gp()) {
      Move(*next_result_reg, LiftoffRegister(kReturnReg), sig->GetReturn(0));
    }
    ++next_result_reg;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_kind != kVoid) {
    Peek(liftoff::GetRegFromType(*next_result_reg, out_argument_kind), 0);
  }

  Drop(total_size, 1);
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
  // For Arm64, we have more cache registers than wasm parameters. That means
  // that target will always be in a register.
  DCHECK(target.is_valid());
  Call(target);
}

void LiftoffAssembler::TailCallIndirect(Register target) {
  DCHECK(target.is_valid());
  // When control flow integrity is enabled, the target is a "bti c"
  // instruction, which enforces that the jump instruction is either a "blr", or
  // a "br" with x16 or x17 as its destination.
  UseScratchRegisterScope temps(this);
  temps.Exclude(x17);
  Mov(x17, target);
  Jump(x17);
}

void LiftoffAssembler::CallRuntimeStub(WasmCode::RuntimeStubId sid) {
  // A direct call to a wasm runtime stub defined in this module.
  // Just encode the stub index. This will be patched at relocation.
  Call(static_cast<Address>(sid), RelocInfo::WASM_STUB_CALL);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  // The stack pointer is required to be quadword aligned.
  size = RoundUp(size, kQuadWordSizeInBytes);
  Claim(size, 1);
  Mov(addr, sp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  // The stack pointer is required to be quadword aligned.
  size = RoundUp(size, kQuadWordSizeInBytes);
  Drop(size, 1);
}

void LiftoffAssembler::MaybeOSR() {}

void LiftoffAssembler::emit_set_if_nan(Register dst, DoubleRegister src,
                                       ValueKind kind) {
  Label not_nan;
  if (kind == kF32) {
    Fcmp(src.S(), src.S());
    B(eq, &not_nan);  // x != x iff isnan(x)
    // If it's a NaN, it must be non-zero, so store that as the set value.
    Str(src.S(), MemOperand(dst));
  } else {
    DCHECK_EQ(kind, kF64);
    Fcmp(src.D(), src.D());
    B(eq, &not_nan);  // x != x iff isnan(x)
    // Double-precision NaNs must be non-zero in the most-significant 32
    // bits, so store that.
    St1(src.V4S(), 1, MemOperand(dst));
  }
  Bind(&not_nan);
}

void LiftoffAssembler::emit_s128_set_if_nan(Register dst, LiftoffRegister src,
                                            Register tmp_gp,
                                            LiftoffRegister tmp_s128,
                                            ValueKind lane_kind) {
  DoubleRegister tmp_fp = tmp_s128.fp();
  if (lane_kind == kF32) {
    Fmaxv(tmp_fp.S(), src.fp().V4S());
  } else {
    DCHECK_EQ(lane_kind, kF64);
    Fmaxp(tmp_fp.D(), src.fp().V2D());
  }
  emit_set_if_nan(dst, tmp_fp, lane_kind);
}

void LiftoffStackSlots::Construct(int param_slots) {
  DCHECK_LT(0, slots_.size());
  // The stack pointer is required to be quadword aligned.
  asm_->Claim(RoundUp(param_slots, 2));
  for (auto& slot : slots_) {
    int poke_offset = slot.dst_slot_ * kSystemPointerSize;
    switch (slot.src_.loc()) {
      case LiftoffAssembler::VarState::kStack: {
        UseScratchRegisterScope temps(asm_);
        CPURegister scratch = liftoff::AcquireByType(&temps, slot.src_.kind());
        asm_->Ldr(scratch, liftoff::GetStackSlot(slot.src_offset_));
        asm_->Poke(scratch, poke_offset);
        break;
      }
      case LiftoffAssembler::VarState::kRegister:
        asm_->Poke(liftoff::GetRegFromType(slot.src_.reg(), slot.src_.kind()),
                   poke_offset);
        break;
      case LiftoffAssembler::VarState::kIntConst:
        DCHECK(slot.src_.kind() == kI32 || slot.src_.kind() == kI64);
        if (slot.src_.i32_const() == 0) {
          Register zero_reg = slot.src_.kind() == kI32 ? wzr : xzr;
          asm_->Poke(zero_reg, poke_offset);
        } else {
          UseScratchRegisterScope temps(asm_);
          Register scratch =
              slot.src_.kind() == kI32 ? temps.AcquireW() : temps.AcquireX();
          asm_->Mov(scratch, int64_t{slot.src_.i32_const()});
          asm_->Poke(scratch, poke_offset);
        }
        break;
    }
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_ARM64_LIFTOFF_ASSEMBLER_ARM64_H_

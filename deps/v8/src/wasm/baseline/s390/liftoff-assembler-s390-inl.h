// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_INL_H_
#define V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_INL_H_

#include "src/codegen/assembler.h"
#include "src/heap/mutable-page-metadata.h"
#include "src/wasm/baseline/liftoff-assembler.h"
#include "src/wasm/baseline/parallel-move-inl.h"
#include "src/wasm/object-access.h"
#include "src/wasm/simd-shuffle.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"

namespace v8::internal::wasm {

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
inline MemOperand GetStackSlot(uint32_t offset) {
  return MemOperand(fp, -offset);
}

inline MemOperand GetInstanceDataOperand() {
  return GetStackSlot(WasmLiftoffFrameConstants::kInstanceDataOffset);
}

inline void StoreToMemory(LiftoffAssembler* assm, MemOperand dst,
                          const LiftoffAssembler::VarState& src,
                          Register scratch) {
  if (src.is_reg()) {
    switch (src.kind()) {
      case kI16:
        assm->StoreU16(src.reg().gp(), dst);
        break;
      case kI32:
        assm->StoreU32(src.reg().gp(), dst);
        break;
      case kI64:
        assm->StoreU64(src.reg().gp(), dst);
        break;
      case kF32:
        assm->StoreF32(src.reg().fp(), dst);
        break;
      case kF64:
        assm->StoreF64(src.reg().fp(), dst);
        break;
      case kS128:
        assm->StoreV128(src.reg().fp(), dst, scratch);
        break;
      default:
        UNREACHABLE();
    }
  } else if (src.is_const()) {
    if (src.kind() == kI32) {
      assm->mov(scratch, Operand(src.i32_const()));
      assm->StoreU32(scratch, dst);
    } else {
      assm->mov(scratch, Operand(static_cast<int64_t>(src.i32_const())));
      assm->StoreU64(scratch, dst);
    }
  } else if (value_kind_size(src.kind()) == 4) {
    assm->LoadU32(scratch, liftoff::GetStackSlot(src.offset()), scratch);
    assm->StoreU32(scratch, dst);
  } else {
    DCHECK_EQ(8, value_kind_size(src.kind()));
    assm->LoadU64(scratch, liftoff::GetStackSlot(src.offset()), scratch);
    assm->StoreU64(scratch, dst);
  }
}

}  // namespace liftoff

int LiftoffAssembler::PrepareStackFrame() {
  int offset = pc_offset();
  lay(sp, MemOperand(sp));
  return offset;
}

void LiftoffAssembler::CallFrameSetupStub(int declared_function_index) {
// The standard library used by gcc tryjobs does not consider `std::find` to be
// `constexpr`, so wrap it in a `#ifdef __clang__` block.
#ifdef __clang__
  static_assert(std::find(std::begin(wasm::kGpParamRegisters),
                          std::end(wasm::kGpParamRegisters),
                          kLiftoffFrameSetupFunctionReg) ==
                std::end(wasm::kGpParamRegisters));
#endif

  // On ARM, we must push at least {lr} before calling the stub, otherwise
  // it would get clobbered with no possibility to recover it.
  Register scratch = ip;
  mov(scratch, Operand(StackFrame::TypeToMarker(StackFrame::WASM)));
  PushCommonFrame(scratch);
  LoadConstant(LiftoffRegister(kLiftoffFrameSetupFunctionReg),
               WasmValue(declared_function_index));
  CallBuiltin(Builtin::kWasmLiftoffFrameSetup);
}

void LiftoffAssembler::PrepareTailCall(int num_callee_stack_params,
                                       int stack_param_delta) {
  Register scratch = r1;
  // Push the return address and frame pointer to complete the stack frame.
  lay(sp, MemOperand(sp, -2 * kSystemPointerSize));
  LoadU64(scratch, MemOperand(fp, kSystemPointerSize));
  StoreU64(scratch, MemOperand(sp, kSystemPointerSize));
  LoadU64(scratch, MemOperand(fp));
  StoreU64(scratch, MemOperand(sp));

  // Shift the whole frame upwards.
  int slot_count = num_callee_stack_params + 2;
  for (int i = slot_count - 1; i >= 0; --i) {
    LoadU64(scratch, MemOperand(sp, i * kSystemPointerSize));
    StoreU64(scratch,
             MemOperand(fp, (i - stack_param_delta) * kSystemPointerSize));
  }

  // Set the new stack and frame pointer.
  lay(sp, MemOperand(fp, -stack_param_delta * kSystemPointerSize));
  Pop(r14, fp);
}

void LiftoffAssembler::AlignFrameSize() {}

void LiftoffAssembler::PatchPrepareStackFrame(
    int offset, SafepointTableBuilder* safepoint_table_builder,
    bool feedback_vector_slot, size_t stack_param_slots) {
  int frame_size = GetTotalFrameSize() - 2 * kSystemPointerSize;
  // The frame setup builtin also pushes the feedback vector.
  if (feedback_vector_slot) {
    frame_size -= kSystemPointerSize;
  }

  constexpr int LayInstrSize = 6;

  Assembler patching_assembler(
      AssemblerOptions{},
      ExternalAssemblerBuffer(buffer_start_ + offset, LayInstrSize + kGap));
  if (V8_LIKELY(frame_size < 4 * KB)) {
    patching_assembler.lay(sp, MemOperand(sp, -frame_size));
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

  int jump_offset = pc_offset() - offset;
  patching_assembler.branchOnCond(al, jump_offset, true, true);

  // If the frame is bigger than the stack, we throw the stack overflow
  // exception unconditionally. Thereby we can avoid the integer overflow
  // check in the condition code.
  RecordComment("OOL: stack check for large frame");
  Label continuation;
  if (frame_size < v8_flags.stack_size * 1024) {
    Register stack_limit = ip;
    LoadStackLimit(stack_limit, StackLimitKind::kRealStackLimit);
    AddU64(stack_limit, Operand(frame_size));
    CmpU64(sp, stack_limit);
    bge(&continuation);
  }

  Call(static_cast<Address>(Builtin::kWasmStackOverflow),
       RelocInfo::WASM_STUB_CALL);
  // The call will not return; just define an empty safepoint.
  safepoint_table_builder->DefineSafepoint(this);
  if (v8_flags.debug_code) stop();

  bind(&continuation);

  // Now allocate the stack space. Note that this might do more than just
  // decrementing the SP; consult {MacroAssembler::AllocateStackSpace}.
  lay(sp, MemOperand(sp, -frame_size));

  // Jump back to the start of the function, from {pc_offset()} to
  // right after the reserved space for the {__ sub(sp, sp, framesize)} (which
  // is a branch now).
  jump_offset = offset - pc_offset() + 6;
  branchOnCond(al, jump_offset, true);
}

void LiftoffAssembler::FinishCode() {}

void LiftoffAssembler::AbortCompilation() { AbortedCodeGeneration(); }

// static
constexpr int LiftoffAssembler::StaticStackFrameSize() {
  return WasmLiftoffFrameConstants::kFeedbackVectorOffset;
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
  return (kind == kS128 || is_reference(kind));
}

void LiftoffAssembler::CheckTierUp(int declared_func_index, int budget_used,
                                   Label* ool_label,
                                   const FreezeCacheState& frozen) {
  Register budget_array = ip;
  Register instance_data = cache_state_.cached_instance_data;

  if (instance_data == no_reg) {
    instance_data = budget_array;  // Reuse the temp register.
    LoadInstanceDataFromFrame(instance_data);
  }

  constexpr int kArrayOffset = wasm::ObjectAccess::ToTagged(
      WasmTrustedInstanceData::kTieringBudgetArrayOffset);
  LoadU64(budget_array, MemOperand(instance_data, kArrayOffset));

  int budget_arr_offset = kInt32Size * declared_func_index;
  Register budget = r1;
  MemOperand budget_addr(budget_array, budget_arr_offset);
  LoadS32(budget, budget_addr);
  SubS32(budget, Operand(budget_used));
  StoreU32(budget, budget_addr);
  blt(ool_label);
}

Register LiftoffAssembler::LoadOldFramePointer() { return fp; }

void LiftoffAssembler::CheckStackShrink() {
  // TODO(irezvov): 42202153
  UNIMPLEMENTED();
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value) {
  switch (value.type().kind()) {
    case kI32:
      mov(reg.gp(), Operand(value.to_i32()));
      break;
    case kI64:
      mov(reg.gp(), Operand(value.to_i64()));
      break;
    case kF32: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      LoadF32(reg.fp(), value.to_f32(), scratch);
      break;
    }
    case kF64: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      LoadF64(reg.fp(), value.to_f64(), scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadInstanceDataFromFrame(Register dst) {
  LoadU64(dst, liftoff::GetInstanceDataOperand());
}

void LiftoffAssembler::LoadTrustedPointer(Register dst, Register src_addr,
                                          int offset, IndirectPointerTag tag) {
  LoadTaggedField(dst, MemOperand{src_addr, offset});
}

void LiftoffAssembler::LoadFromInstance(Register dst, Register instance,
                                        int offset, int size) {
  DCHECK_LE(0, offset);
  switch (size) {
    case 1:
      LoadU8(dst, MemOperand(instance, offset));
      break;
    case 4:
      LoadU32(dst, MemOperand(instance, offset));
      break;
    case 8:
      LoadU64(dst, MemOperand(instance, offset));
      break;
    default:
      UNIMPLEMENTED();
  }
}

void LiftoffAssembler::LoadTaggedPointerFromInstance(Register dst,
                                                     Register instance,
                                                     int offset) {
  DCHECK_LE(0, offset);
  LoadTaggedField(dst, MemOperand(instance, offset));
}

void LiftoffAssembler::SpillInstanceData(Register instance) {
  StoreU64(instance, liftoff::GetInstanceDataOperand());
}

void LiftoffAssembler::ResetOSRTarget() {}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         int32_t offset_imm,
                                         uint32_t* protected_load_pc,
                                         bool needs_shift) {
  CHECK(is_int20(offset_imm));
  unsigned shift_amount = !needs_shift ? 0 : COMPRESS_POINTERS_BOOL ? 2 : 3;
  if (offset_reg != no_reg && shift_amount != 0) {
    ShiftLeftU64(ip, offset_reg, Operand(shift_amount));
    offset_reg = ip;
  }
  if (protected_load_pc) *protected_load_pc = pc_offset();
  LoadTaggedField(
      dst,
      MemOperand(src_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));
}

void LiftoffAssembler::LoadProtectedPointer(Register dst, Register src_addr,
                                            int32_t offset) {
  static_assert(!V8_ENABLE_SANDBOX_BOOL);
  LoadTaggedPointer(dst, src_addr, no_reg, offset);
}

void LiftoffAssembler::LoadFullPointer(Register dst, Register src_addr,
                                       int32_t offset_imm) {
  UseScratchRegisterScope temps(this);
  LoadU64(dst, MemOperand(src_addr, offset_imm), r1);
}

void LiftoffAssembler::StoreTaggedPointer(Register dst_addr,
                                          Register offset_reg,
                                          int32_t offset_imm, Register src,
                                          LiftoffRegList /* pinned */,
                                          uint32_t* protected_store_pc,
                                          SkipWriteBarrier skip_write_barrier) {
  MemOperand dst_op =
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm);
  if (protected_store_pc) *protected_store_pc = pc_offset();
  StoreTaggedField(src, dst_op);

  if (skip_write_barrier || v8_flags.disable_write_barriers) return;

  Label exit;
  CheckPageFlag(dst_addr, r1, MemoryChunk::kPointersFromHereAreInterestingMask,
                to_condition(kZero), &exit);
  JumpIfSmi(src, &exit);
  CheckPageFlag(src, r1, MemoryChunk::kPointersToHereAreInterestingMask, eq,
                &exit);
  lay(r1, dst_op);
  CallRecordWriteStubSaveRegisters(dst_addr, r1, SaveFPRegsMode::kSave,
                                   StubCallMode::kCallWasmRuntimeStub);
  bind(&exit);
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uintptr_t offset_imm,
                            LoadType type, uint32_t* protected_load_pc,
                            bool is_load_mem, bool i64_offset,
                            bool needs_shift) {
  UseScratchRegisterScope temps(this);
  if (offset_reg != no_reg && !i64_offset) {
    // Clear the upper 32 bits of the 64 bit offset register.
    llgfr(ip, offset_reg);
    offset_reg = ip;
  }
  unsigned shift_amount = needs_shift ? type.size_log_2() : 0;
  if (offset_reg != no_reg && shift_amount != 0) {
    ShiftLeftU64(ip, offset_reg, Operand(shift_amount));
    offset_reg = ip;
  }
  if (!is_int20(offset_imm)) {
    if (offset_reg != no_reg) {
      mov(r0, Operand(offset_imm));
      AddS64(r0, offset_reg);
      mov(ip, r0);
    } else {
      mov(ip, Operand(offset_imm));
    }
    offset_reg = ip;
    offset_imm = 0;
  }
  MemOperand src_op =
      MemOperand(src_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm);
  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U:
      LoadU8(dst.gp(), src_op);
      break;
    case LoadType::kI32Load8S:
    case LoadType::kI64Load8S:
      LoadS8(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      if (is_load_mem) {
        LoadU16LE(dst.gp(), src_op);
      } else {
        LoadU16(dst.gp(), src_op);
      }
      break;
    case LoadType::kI32Load16S:
    case LoadType::kI64Load16S:
      if (is_load_mem) {
        LoadS16LE(dst.gp(), src_op);
      } else {
        LoadS16(dst.gp(), src_op);
      }
      break;
    case LoadType::kI64Load32U:
      if (is_load_mem) {
        LoadU32LE(dst.gp(), src_op);
      } else {
        LoadU32(dst.gp(), src_op);
      }
      break;
    case LoadType::kI32Load:
    case LoadType::kI64Load32S:
      if (is_load_mem) {
        LoadS32LE(dst.gp(), src_op);
      } else {
        LoadS32(dst.gp(), src_op);
      }
      break;
    case LoadType::kI64Load:
      if (is_load_mem) {
        LoadU64LE(dst.gp(), src_op);
      } else {
        LoadU64(dst.gp(), src_op);
      }
      break;
    case LoadType::kF32Load:
      if (is_load_mem) {
        LoadF32LE(dst.fp(), src_op, r0);
      } else {
        LoadF32(dst.fp(), src_op);
      }
      break;
    case LoadType::kF64Load:
      if (is_load_mem) {
        LoadF64LE(dst.fp(), src_op, r0);
      } else {
        LoadF64(dst.fp(), src_op);
      }
      break;
    case LoadType::kS128Load:
      if (is_load_mem) {
        LoadV128LE(dst.fp(), src_op, r1, r0);
      } else {
        LoadV128(dst.fp(), src_op, r1);
      }
      break;
    default:
      UNREACHABLE();
  }
}

#define PREP_MEM_OPERAND(offset_reg, offset_imm, scratch)       \
  if (offset_reg != no_reg && !i64_offset) {                    \
    /* Clear the upper 32 bits of the 64 bit offset register.*/ \
    llgfr(scratch, offset_reg);                                 \
    offset_reg = scratch;                                       \
  }                                                             \
  if (!is_int20(offset_imm)) {                                  \
    if (offset_reg != no_reg) {                                 \
      mov(r0, Operand(offset_imm));                             \
      AddS64(r0, offset_reg);                                   \
      mov(scratch, r0);                                         \
    } else {                                                    \
      mov(scratch, Operand(offset_imm));                        \
    }                                                           \
    offset_reg = scratch;                                       \
    offset_imm = 0;                                             \
  }
void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uintptr_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList /* pinned */,
                             uint32_t* protected_store_pc, bool is_store_mem,
                             bool i64_offset) {
  PREP_MEM_OPERAND(offset_reg, offset_imm, ip)
  MemOperand dst_op =
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm);
  if (protected_store_pc) *protected_store_pc = pc_offset();
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      StoreU8(src.gp(), dst_op);
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      if (is_store_mem) {
        StoreU16LE(src.gp(), dst_op, r1);
      } else {
        StoreU16(src.gp(), dst_op, r1);
      }
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      if (is_store_mem) {
        StoreU32LE(src.gp(), dst_op, r1);
      } else {
        StoreU32(src.gp(), dst_op, r1);
      }
      break;
    case StoreType::kI64Store:
      if (is_store_mem) {
        StoreU64LE(src.gp(), dst_op, r1);
      } else {
        StoreU64(src.gp(), dst_op, r1);
      }
      break;
    case StoreType::kF32Store:
      if (is_store_mem) {
        StoreF32LE(src.fp(), dst_op, r1);
      } else {
        StoreF32(src.fp(), dst_op);
      }
      break;
    case StoreType::kF64Store:
      if (is_store_mem) {
        StoreF64LE(src.fp(), dst_op, r1);
      } else {
        StoreF64(src.fp(), dst_op);
      }
      break;
    case StoreType::kS128Store: {
      if (is_store_mem) {
        StoreV128LE(src.fp(), dst_op, r1, r0);
      } else {
        StoreV128(src.fp(), dst_op, r1);
      }
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicLoad(LiftoffRegister dst, Register src_addr,
                                  Register offset_reg, uintptr_t offset_imm,
                                  LoadType type, LiftoffRegList /* pinned */,
                                  bool i64_offset) {
  Load(dst, src_addr, offset_reg, offset_imm, type, nullptr, true, i64_offset);
}

void LiftoffAssembler::AtomicStore(Register dst_addr, Register offset_reg,
                                   uintptr_t offset_imm, LiftoffRegister src,
                                   StoreType type, LiftoffRegList /* pinned */,
                                   bool i64_offset) {
  PREP_MEM_OPERAND(offset_reg, offset_imm, ip)
  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      AtomicExchangeU8(ip, src.gp(), r1, r0);
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(r1, src.gp());
      ShiftRightU32(r1, r1, Operand(16));
#else
      LoadU16(r1, src.gp());
#endif
      Push(r2);
      AtomicExchangeU16(ip, r1, r2, r0);
      Pop(r2);
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(r1, src.gp());
#else
      LoadU32(r1, src.gp());
#endif
      Label do_cs;
      bind(&do_cs);
      cs(r0, r1, MemOperand(ip));
      bne(&do_cs, Label::kNear);
      break;
    }
    case StoreType::kI64Store: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(r1, src.gp());
#else
      mov(r1, src.gp());
#endif
      Label do_cs;
      bind(&do_cs);
      csg(r0, r1, MemOperand(ip));
      bne(&do_cs, Label::kNear);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicAdd(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type,
                                 bool i64_offset) {
  LiftoffRegList pinned = LiftoffRegList{dst_addr, value, result};
  if (offset_reg != no_reg) pinned.set(offset_reg);
  Register tmp1 = GetUnusedRegister(kGpReg, pinned).gp();
  pinned.set(tmp1);
  Register tmp2 = GetUnusedRegister(kGpReg, pinned).gp();

  PREP_MEM_OPERAND(offset_reg, offset_imm, ip)
  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      Label doadd;
      bind(&doadd);
      LoadU8(tmp1, MemOperand(ip));
      AddS32(tmp2, tmp1, value.gp());
      AtomicCmpExchangeU8(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &doadd);
      LoadU8(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
      Label doadd;
      bind(&doadd);
      LoadU16(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      ShiftRightU32(tmp2, tmp2, Operand(16));
      AddS32(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
      ShiftRightU32(tmp2, tmp2, Operand(16));
#else
      AddS32(tmp2, tmp1, value.gp());
#endif
      AtomicCmpExchangeU16(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &doadd);
      LoadU16(result.gp(), result.gp());
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
      ShiftRightU32(result.gp(), result.gp(), Operand(16));
#endif
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
      Label doadd;
      bind(&doadd);
      LoadU32(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      AddS32(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
#else
      AddS32(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &doadd);
      LoadU32(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
#endif
      break;
    }
    case StoreType::kI64Store: {
      Label doadd;
      bind(&doadd);
      LoadU64(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(tmp2, tmp1);
      AddS64(tmp2, tmp2, value.gp());
      lrvgr(tmp2, tmp2);
#else
      AddS64(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap64(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &doadd);
      mov(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(result.gp(), result.gp());
#endif
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicSub(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type,
                                 bool i64_offset) {
  LiftoffRegList pinned = LiftoffRegList{dst_addr, value, result};
  if (offset_reg != no_reg) pinned.set(offset_reg);
  Register tmp1 = GetUnusedRegister(kGpReg, pinned).gp();
  pinned.set(tmp1);
  Register tmp2 = GetUnusedRegister(kGpReg, pinned).gp();

  PREP_MEM_OPERAND(offset_reg, offset_imm, ip)
  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      Label do_again;
      bind(&do_again);
      LoadU8(tmp1, MemOperand(ip));
      SubS32(tmp2, tmp1, value.gp());
      AtomicCmpExchangeU8(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &do_again);
      LoadU8(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
      Label do_again;
      bind(&do_again);
      LoadU16(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      ShiftRightU32(tmp2, tmp2, Operand(16));
      SubS32(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
      ShiftRightU32(tmp2, tmp2, Operand(16));
#else
      SubS32(tmp2, tmp1, value.gp());
#endif
      AtomicCmpExchangeU16(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &do_again);
      LoadU16(result.gp(), result.gp());
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
      ShiftRightU32(result.gp(), result.gp(), Operand(16));
#endif
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
      Label do_again;
      bind(&do_again);
      LoadU32(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      SubS32(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
#else
      SubS32(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &do_again);
      LoadU32(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
#endif
      break;
    }
    case StoreType::kI64Store: {
      Label do_again;
      bind(&do_again);
      LoadU64(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(tmp2, tmp1);
      SubS64(tmp2, tmp2, value.gp());
      lrvgr(tmp2, tmp2);
#else
      SubS64(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap64(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &do_again);
      mov(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(result.gp(), result.gp());
#endif
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicAnd(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type,
                                 bool i64_offset) {
  LiftoffRegList pinned = LiftoffRegList{dst_addr, value, result};
  if (offset_reg != no_reg) pinned.set(offset_reg);
  Register tmp1 = GetUnusedRegister(kGpReg, pinned).gp();
  pinned.set(tmp1);
  Register tmp2 = GetUnusedRegister(kGpReg, pinned).gp();

  PREP_MEM_OPERAND(offset_reg, offset_imm, ip)
  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      Label do_again;
      bind(&do_again);
      LoadU8(tmp1, MemOperand(ip));
      AndP(tmp2, tmp1, value.gp());
      AtomicCmpExchangeU8(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &do_again);
      LoadU8(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
      Label do_again;
      bind(&do_again);
      LoadU16(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      ShiftRightU32(tmp2, tmp2, Operand(16));
      AndP(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
      ShiftRightU32(tmp2, tmp2, Operand(16));
#else
      AndP(tmp2, tmp1, value.gp());
#endif
      AtomicCmpExchangeU16(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &do_again);
      LoadU16(result.gp(), result.gp());
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
      ShiftRightU32(result.gp(), result.gp(), Operand(16));
#endif
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
      Label do_again;
      bind(&do_again);
      LoadU32(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      AndP(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
#else
      AndP(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &do_again);
      LoadU32(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
#endif
      break;
    }
    case StoreType::kI64Store: {
      Label do_again;
      bind(&do_again);
      LoadU64(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(tmp2, tmp1);
      AndP(tmp2, tmp2, value.gp());
      lrvgr(tmp2, tmp2);
#else
      AndP(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap64(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &do_again);
      mov(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(result.gp(), result.gp());
#endif
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicOr(Register dst_addr, Register offset_reg,
                                uintptr_t offset_imm, LiftoffRegister value,
                                LiftoffRegister result, StoreType type,
                                bool i64_offset) {
  LiftoffRegList pinned = LiftoffRegList{dst_addr, value, result};
  if (offset_reg != no_reg) pinned.set(offset_reg);
  Register tmp1 = GetUnusedRegister(kGpReg, pinned).gp();
  pinned.set(tmp1);
  Register tmp2 = GetUnusedRegister(kGpReg, pinned).gp();

  PREP_MEM_OPERAND(offset_reg, offset_imm, ip)
  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      Label do_again;
      bind(&do_again);
      LoadU8(tmp1, MemOperand(ip));
      OrP(tmp2, tmp1, value.gp());
      AtomicCmpExchangeU8(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &do_again);
      LoadU8(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
      Label do_again;
      bind(&do_again);
      LoadU16(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      ShiftRightU32(tmp2, tmp2, Operand(16));
      OrP(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
      ShiftRightU32(tmp2, tmp2, Operand(16));
#else
      OrP(tmp2, tmp1, value.gp());
#endif
      AtomicCmpExchangeU16(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &do_again);
      LoadU16(result.gp(), result.gp());
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
      ShiftRightU32(result.gp(), result.gp(), Operand(16));
#endif
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
      Label do_again;
      bind(&do_again);
      LoadU32(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      OrP(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
#else
      OrP(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &do_again);
      LoadU32(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
#endif
      break;
    }
    case StoreType::kI64Store: {
      Label do_again;
      bind(&do_again);
      LoadU64(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(tmp2, tmp1);
      OrP(tmp2, tmp2, value.gp());
      lrvgr(tmp2, tmp2);
#else
      OrP(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap64(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &do_again);
      mov(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(result.gp(), result.gp());
#endif
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicXor(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type,
                                 bool i64_offset) {
  LiftoffRegList pinned = LiftoffRegList{dst_addr, value, result};
  if (offset_reg != no_reg) pinned.set(offset_reg);
  Register tmp1 = GetUnusedRegister(kGpReg, pinned).gp();
  pinned.set(tmp1);
  Register tmp2 = GetUnusedRegister(kGpReg, pinned).gp();

  PREP_MEM_OPERAND(offset_reg, offset_imm, ip)
  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      Label do_again;
      bind(&do_again);
      LoadU8(tmp1, MemOperand(ip));
      XorP(tmp2, tmp1, value.gp());
      AtomicCmpExchangeU8(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &do_again);
      LoadU8(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
      Label do_again;
      bind(&do_again);
      LoadU16(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      ShiftRightU32(tmp2, tmp2, Operand(16));
      XorP(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
      ShiftRightU32(tmp2, tmp2, Operand(16));
#else
      XorP(tmp2, tmp1, value.gp());
#endif
      AtomicCmpExchangeU16(ip, result.gp(), tmp1, tmp2, r0, r1);
      b(Condition(4), &do_again);
      LoadU16(result.gp(), result.gp());
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
      ShiftRightU32(result.gp(), result.gp(), Operand(16));
#endif
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
      Label do_again;
      bind(&do_again);
      LoadU32(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp2, tmp1);
      XorP(tmp2, tmp2, value.gp());
      lrvr(tmp2, tmp2);
#else
      XorP(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &do_again);
      LoadU32(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
#endif
      break;
    }
    case StoreType::kI64Store: {
      Label do_again;
      bind(&do_again);
      LoadU64(tmp1, MemOperand(ip));
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(tmp2, tmp1);
      XorP(tmp2, tmp2, value.gp());
      lrvgr(tmp2, tmp2);
#else
      XorP(tmp2, tmp1, value.gp());
#endif
      CmpAndSwap64(tmp1, tmp2, MemOperand(ip));
      b(Condition(4), &do_again);
      mov(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(result.gp(), result.gp());
#endif
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicExchange(Register dst_addr, Register offset_reg,
                                      uintptr_t offset_imm,
                                      LiftoffRegister value,
                                      LiftoffRegister result, StoreType type,
                                      bool i64_offset) {
  PREP_MEM_OPERAND(offset_reg, offset_imm, ip)
  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      AtomicExchangeU8(ip, value.gp(), result.gp(), r0);
      LoadU8(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(r1, value.gp());
      ShiftRightU32(r1, r1, Operand(16));
#else
      LoadU16(r1, value.gp());
#endif
      AtomicExchangeU16(ip, r1, result.gp(), r0);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
      ShiftRightU32(result.gp(), result.gp(), Operand(16));
#else
      LoadU16(result.gp(), result.gp());
#endif
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(r1, value.gp());
#else
      LoadU32(r1, value.gp());
#endif
      Label do_cs;
      bind(&do_cs);
      cs(result.gp(), r1, MemOperand(ip));
      bne(&do_cs, Label::kNear);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
#endif
      LoadU32(result.gp(), result.gp());
      break;
    }
    case StoreType::kI64Store: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(r1, value.gp());
#else
      mov(r1, value.gp());
#endif
      Label do_cs;
      bind(&do_cs);
      csg(result.gp(), r1, MemOperand(ip));
      bne(&do_cs, Label::kNear);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(result.gp(), result.gp());
#endif
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicCompareExchange(
    Register dst_addr, Register offset_reg, uintptr_t offset_imm,
    LiftoffRegister expected, LiftoffRegister new_value, LiftoffRegister result,
    StoreType type, bool i64_offset) {

  LiftoffRegList pinned = LiftoffRegList{dst_addr, expected, new_value, result};
  if (offset_reg != no_reg) pinned.set(offset_reg);
  Register tmp1 = GetUnusedRegister(kGpReg, pinned).gp();
  pinned.set(tmp1);
  Register tmp2 = GetUnusedRegister(kGpReg, pinned).gp();

  PREP_MEM_OPERAND(offset_reg, offset_imm, ip)
  lay(ip,
      MemOperand(dst_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm));

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8: {
      AtomicCmpExchangeU8(ip, result.gp(), expected.gp(), new_value.gp(), r0,
                          r1);
      LoadU8(result.gp(), result.gp());
      break;
    }
    case StoreType::kI32Store16:
    case StoreType::kI64Store16: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp1, expected.gp());
      lrvr(tmp2, new_value.gp());
      ShiftRightU32(tmp1, tmp1, Operand(16));
      ShiftRightU32(tmp2, tmp2, Operand(16));
#else
      LoadU16(tmp1, expected.gp());
      LoadU16(tmp2, new_value.gp());
#endif
      AtomicCmpExchangeU16(ip, result.gp(), tmp1, tmp2, r0, r1);
      LoadU16(result.gp(), result.gp());
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
      ShiftRightU32(result.gp(), result.gp(), Operand(16));
#endif
      break;
    }
    case StoreType::kI32Store:
    case StoreType::kI64Store32: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(tmp1, expected.gp());
      lrvr(tmp2, new_value.gp());
#else
      LoadU32(tmp1, expected.gp());
      LoadU32(tmp2, new_value.gp());
#endif
      CmpAndSwap(tmp1, tmp2, MemOperand(ip));
      LoadU32(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvr(result.gp(), result.gp());
#endif
      break;
    }
    case StoreType::kI64Store: {
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(tmp1, expected.gp());
      lrvgr(tmp2, new_value.gp());
#else
      mov(tmp1, expected.gp());
      mov(tmp2, new_value.gp());
#endif
      CmpAndSwap64(tmp1, tmp2, MemOperand(ip));
      mov(result.gp(), tmp1);
#ifdef V8_TARGET_BIG_ENDIAN
      lrvgr(result.gp(), result.gp());
#endif
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicFence() { bailout(kAtomics, "AtomicFence"); }

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueKind kind) {
  int32_t offset = (caller_slot_idx + 1) * 8;
  switch (kind) {
    case kI32: {
#if defined(V8_TARGET_BIG_ENDIAN)
      LoadS32(dst.gp(), MemOperand(fp, offset + 4));
      break;
#else
      LoadS32(dst.gp(), MemOperand(fp, offset));
      break;
#endif
    }
    case kRef:
    case kRtt:
    case kRefNull:
    case kI64: {
      LoadU64(dst.gp(), MemOperand(fp, offset));
      break;
    }
    case kF32: {
      LoadF32(dst.fp(), MemOperand(fp, offset));
      break;
    }
    case kF64: {
      LoadF64(dst.fp(), MemOperand(fp, offset));
      break;
    }
    case kS128: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      LoadV128(dst.fp(), MemOperand(fp, offset), scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::StoreCallerFrameSlot(LiftoffRegister src,
                                            uint32_t caller_slot_idx,
                                            ValueKind kind,
                                            Register frame_pointer) {
  int32_t offset = (caller_slot_idx + 1) * 8;
  switch (kind) {
    case kI32: {
#if defined(V8_TARGET_BIG_ENDIAN)
      StoreU32(src.gp(), MemOperand(fp, offset + 4));
      break;
#else
      StoreU32(src.gp(), MemOperand(fp, offset));
      break;
#endif
    }
    case kRef:
    case kRtt:
    case kRefNull:
    case kI64: {
      StoreU64(src.gp(), MemOperand(fp, offset));
      break;
    }
    case kF32: {
      StoreF32(src.fp(), MemOperand(fp, offset));
      break;
    }
    case kF64: {
      StoreF64(src.fp(), MemOperand(fp, offset));
      break;
    }
    case kS128: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      StoreV128(src.fp(), MemOperand(fp, offset), scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadReturnStackSlot(LiftoffRegister dst, int offset,
                                           ValueKind kind) {
  switch (kind) {
    case kI32: {
#if defined(V8_TARGET_BIG_ENDIAN)
      LoadS32(dst.gp(), MemOperand(sp, offset + 4));
      break;
#else
      LoadS32(dst.gp(), MemOperand(sp, offset));
      break;
#endif
    }
    case kRef:
    case kRtt:
    case kRefNull:
    case kI64: {
      LoadU64(dst.gp(), MemOperand(sp, offset));
      break;
    }
    case kF32: {
      LoadF32(dst.fp(), MemOperand(sp, offset));
      break;
    }
    case kF64: {
      LoadF64(dst.fp(), MemOperand(sp, offset));
      break;
    }
    case kS128: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      LoadV128(dst.fp(), MemOperand(sp, offset), scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

#ifdef V8_TARGET_BIG_ENDIAN
constexpr int stack_bias = -4;
#else
constexpr int stack_bias = 0;
#endif

void LiftoffAssembler::MoveStackValue(uint32_t dst_offset, uint32_t src_offset,
                                      ValueKind kind) {
  DCHECK_NE(dst_offset, src_offset);
  int length = 0;
  switch (kind) {
    case kI32:
    case kF32:
      length = 4;
      break;
    case kI64:
    case kRefNull:
    case kRef:
    case kRtt:
    case kF64:
      length = 8;
      break;
    case kS128:
      length = 16;
      break;
    default:
      UNREACHABLE();
  }

  dst_offset += (length == 4 ? stack_bias : 0);
  src_offset += (length == 4 ? stack_bias : 0);

  if (is_int20(dst_offset)) {
    lay(ip, liftoff::GetStackSlot(dst_offset));
  } else {
    mov(ip, Operand(-dst_offset));
    lay(ip, MemOperand(fp, ip));
  }

  if (is_int20(src_offset)) {
    lay(r1, liftoff::GetStackSlot(src_offset));
  } else {
    mov(r1, Operand(-src_offset));
    lay(r1, MemOperand(fp, r1));
  }

  MoveChar(MemOperand(ip), MemOperand(r1), Operand(length));
}

void LiftoffAssembler::Move(Register dst, Register src, ValueKind kind) {
  mov(dst, src);
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueKind kind) {
  DCHECK_NE(dst, src);
  if (kind == kF32) {
    ler(dst, src);
  } else if (kind == kF64) {
    ldr(dst, src);
  } else {
    DCHECK_EQ(kS128, kind);
    vlr(dst, src, Condition(0), Condition(0), Condition(0));
  }
}

void LiftoffAssembler::Spill(int offset, LiftoffRegister reg, ValueKind kind) {
  DCHECK_LT(0, offset);
  RecordUsedSpillOffset(offset);

  switch (kind) {
    case kI32:
      StoreU32(reg.gp(), liftoff::GetStackSlot(offset + stack_bias));
      break;
    case kI64:
    case kRefNull:
    case kRef:
    case kRtt:
      StoreU64(reg.gp(), liftoff::GetStackSlot(offset));
      break;
    case kF32:
      StoreF32(reg.fp(), liftoff::GetStackSlot(offset + stack_bias));
      break;
    case kF64:
      StoreF64(reg.fp(), liftoff::GetStackSlot(offset));
      break;
    case kS128: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      StoreV128(reg.fp(), liftoff::GetStackSlot(offset), scratch);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(int offset, WasmValue value) {
  RecordUsedSpillOffset(offset);
  UseScratchRegisterScope temps(this);
  Register src = no_reg;
  src = ip;
  switch (value.type().kind()) {
    case kI32: {
      mov(src, Operand(value.to_i32()));
      StoreU32(src, liftoff::GetStackSlot(offset + stack_bias));
      break;
    }
    case kI64: {
      mov(src, Operand(value.to_i64()));
      StoreU64(src, liftoff::GetStackSlot(offset));
      break;
    }
    default:
      // We do not track f32 and f64 constants, hence they are unreachable.
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, int offset, ValueKind kind) {
  switch (kind) {
    case kI32:
      LoadS32(reg.gp(), liftoff::GetStackSlot(offset + stack_bias));
      break;
    case kI64:
    case kRef:
    case kRefNull:
    case kRtt:
      LoadU64(reg.gp(), liftoff::GetStackSlot(offset));
      break;
    case kF32:
      LoadF32(reg.fp(), liftoff::GetStackSlot(offset + stack_bias));
      break;
    case kF64:
      LoadF64(reg.fp(), liftoff::GetStackSlot(offset));
      break;
    case kS128: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      LoadV128(reg.fp(), liftoff::GetStackSlot(offset), scratch);
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
  DCHECK_EQ(0, size % 4);
  RecordUsedSpillOffset(start + size);

  // We need a zero reg. Always use r0 for that, and push it before to restore
  // its value afterwards.
  push(r0);
  mov(r0, Operand(0));

  if (size <= 5 * kStackSlotSize) {
    // Special straight-line code for up to five slots. Generates two
    // instructions per slot.
    uint32_t remainder = size;
    for (; remainder >= kStackSlotSize; remainder -= kStackSlotSize) {
      StoreU64(r0, liftoff::GetStackSlot(start + remainder));
    }
    DCHECK(remainder == 4 || remainder == 0);
    if (remainder) {
      StoreU32(r0, liftoff::GetStackSlot(start + remainder));
    }
  } else {
    // General case for bigger counts (9 instructions).
    // Use r3 for start address (inclusive), r4 for end address (exclusive).
    push(r3);
    push(r4);

    lay(r3, MemOperand(fp, -start - size));
    lay(r4, MemOperand(fp, -start));

    Label loop;
    bind(&loop);
    StoreU64(r0, MemOperand(r3));
    lay(r3, MemOperand(r3, kSystemPointerSize));
    CmpU64(r3, r4);
    bne(&loop);
    pop(r4);
    pop(r3);
  }

  pop(r0);
}

void LiftoffAssembler::LoadSpillAddress(Register dst, int offset,
                                        ValueKind kind) {
  if (kind == kI32) offset = offset + stack_bias;
  SubS64(dst, fp, Operand(offset));
}

#define SIGN_EXT(r) lgfr(r, r)
#define INT32_AND_WITH_1F(x) Operand(x & 0x1f)
#define REGISTER_AND_WITH_1F    \
  ([&](Register rhs) {          \
    AndP(r1, rhs, Operand(31)); \
    return r1;                  \
  })

#define LFR_TO_REG(reg) reg.gp()

// V(name, instr, dtype, stype, dcast, scast, rcast, return_val, return_type)
#define UNOP_LIST(V)                                                           \
  V(i32_popcnt, Popcnt32, Register, Register, , , USE, true, bool)             \
  V(i64_popcnt, Popcnt64, LiftoffRegister, LiftoffRegister, LFR_TO_REG,        \
    LFR_TO_REG, USE, true, bool)                                               \
  V(u32_to_uintptr, LoadU32, Register, Register, , , USE, , void)              \
  V(i32_signextend_i8, lbr, Register, Register, , , USE, , void)               \
  V(i32_signextend_i16, lhr, Register, Register, , , USE, , void)              \
  V(i64_signextend_i8, lgbr, LiftoffRegister, LiftoffRegister, LFR_TO_REG,     \
    LFR_TO_REG, USE, , void)                                                   \
  V(i64_signextend_i16, lghr, LiftoffRegister, LiftoffRegister, LFR_TO_REG,    \
    LFR_TO_REG, USE, , void)                                                   \
  V(i64_signextend_i32, LoadS32, LiftoffRegister, LiftoffRegister, LFR_TO_REG, \
    LFR_TO_REG, USE, , void)                                                   \
  V(i32_clz, CountLeadingZerosU32, Register, Register, , , USE, , void)        \
  V(i32_ctz, CountTrailingZerosU32, Register, Register, , , USE, , void)       \
  V(i64_clz, CountLeadingZerosU64, LiftoffRegister, LiftoffRegister,           \
    LFR_TO_REG, LFR_TO_REG, USE, , void)                                       \
  V(i64_ctz, CountTrailingZerosU64, LiftoffRegister, LiftoffRegister,          \
    LFR_TO_REG, LFR_TO_REG, USE, , void)                                       \
  V(f32_ceil, CeilF32, DoubleRegister, DoubleRegister, , , USE, true, bool)    \
  V(f32_floor, FloorF32, DoubleRegister, DoubleRegister, , , USE, true, bool)  \
  V(f32_trunc, TruncF32, DoubleRegister, DoubleRegister, , , USE, true, bool)  \
  V(f32_nearest_int, NearestIntF32, DoubleRegister, DoubleRegister, , , USE,   \
    true, bool)                                                                \
  V(f32_abs, lpebr, DoubleRegister, DoubleRegister, , , USE, , void)           \
  V(f32_neg, lcebr, DoubleRegister, DoubleRegister, , , USE, , void)           \
  V(f32_sqrt, sqebr, DoubleRegister, DoubleRegister, , , USE, , void)          \
  V(f64_ceil, CeilF64, DoubleRegister, DoubleRegister, , , USE, true, bool)    \
  V(f64_floor, FloorF64, DoubleRegister, DoubleRegister, , , USE, true, bool)  \
  V(f64_trunc, TruncF64, DoubleRegister, DoubleRegister, , , USE, true, bool)  \
  V(f64_nearest_int, NearestIntF64, DoubleRegister, DoubleRegister, , , USE,   \
    true, bool)                                                                \
  V(f64_abs, lpdbr, DoubleRegister, DoubleRegister, , , USE, , void)           \
  V(f64_neg, lcdbr, DoubleRegister, DoubleRegister, , , USE, , void)           \
  V(f64_sqrt, sqdbr, DoubleRegister, DoubleRegister, , , USE, , void)

#define EMIT_UNOP_FUNCTION(name, instr, dtype, stype, dcast, scast, rcast, \
                           ret, return_type)                               \
  return_type LiftoffAssembler::emit_##name(dtype dst, stype src) {        \
    auto _dst = dcast(dst);                                                \
    auto _src = scast(src);                                                \
    instr(_dst, _src);                                                     \
    rcast(_dst);                                                           \
    return ret;                                                            \
  }
UNOP_LIST(EMIT_UNOP_FUNCTION)
#undef EMIT_UNOP_FUNCTION
#undef UNOP_LIST

// V(name, instr, dtype, stype1, stype2, dcast, scast1, scast2, rcast,
// return_val, return_type)
#define BINOP_LIST(V)                                                          \
  V(f32_min, FloatMin, DoubleRegister, DoubleRegister, DoubleRegister, , , ,   \
    USE, , void)                                                               \
  V(f32_max, FloatMax, DoubleRegister, DoubleRegister, DoubleRegister, , , ,   \
    USE, , void)                                                               \
  V(f64_min, DoubleMin, DoubleRegister, DoubleRegister, DoubleRegister, , , ,  \
    USE, , void)                                                               \
  V(f64_max, DoubleMax, DoubleRegister, DoubleRegister, DoubleRegister, , , ,  \
    USE, , void)                                                               \
  V(f64_add, AddF64, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f64_sub, SubF64, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f64_mul, MulF64, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f64_div, DivF64, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f32_add, AddF32, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f32_sub, SubF32, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f32_mul, MulF32, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(f32_div, DivF32, DoubleRegister, DoubleRegister, DoubleRegister, , , ,     \
    USE, , void)                                                               \
  V(i32_shli, ShiftLeftU32, Register, Register, int32_t, , ,                   \
    INT32_AND_WITH_1F, SIGN_EXT, , void)                                       \
  V(i32_sari, ShiftRightS32, Register, Register, int32_t, , ,                  \
    INT32_AND_WITH_1F, SIGN_EXT, , void)                                       \
  V(i32_shri, ShiftRightU32, Register, Register, int32_t, , ,                  \
    INT32_AND_WITH_1F, SIGN_EXT, , void)                                       \
  V(i32_shl, ShiftLeftU32, Register, Register, Register, , ,                   \
    REGISTER_AND_WITH_1F, SIGN_EXT, , void)                                    \
  V(i32_sar, ShiftRightS32, Register, Register, Register, , ,                  \
    REGISTER_AND_WITH_1F, SIGN_EXT, , void)                                    \
  V(i32_shr, ShiftRightU32, Register, Register, Register, , ,                  \
    REGISTER_AND_WITH_1F, SIGN_EXT, , void)                                    \
  V(i32_addi, AddS32, Register, Register, int32_t, , , Operand, SIGN_EXT, ,    \
    void)                                                                      \
  V(i32_subi, SubS32, Register, Register, int32_t, , , Operand, SIGN_EXT, ,    \
    void)                                                                      \
  V(i32_andi, And, Register, Register, int32_t, , , Operand, SIGN_EXT, , void) \
  V(i32_ori, Or, Register, Register, int32_t, , , Operand, SIGN_EXT, , void)   \
  V(i32_xori, Xor, Register, Register, int32_t, , , Operand, SIGN_EXT, , void) \
  V(i32_add, AddS32, Register, Register, Register, , , , SIGN_EXT, , void)     \
  V(i32_sub, SubS32, Register, Register, Register, , , , SIGN_EXT, , void)     \
  V(i32_and, And, Register, Register, Register, , , , SIGN_EXT, , void)        \
  V(i32_or, Or, Register, Register, Register, , , , SIGN_EXT, , void)          \
  V(i32_xor, Xor, Register, Register, Register, , , , SIGN_EXT, , void)        \
  V(i32_mul, MulS32, Register, Register, Register, , , , SIGN_EXT, , void)     \
  V(i64_add, AddS64, LiftoffRegister, LiftoffRegister, LiftoffRegister,        \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_sub, SubS64, LiftoffRegister, LiftoffRegister, LiftoffRegister,        \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_mul, MulS64, LiftoffRegister, LiftoffRegister, LiftoffRegister,        \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_and, AndP, LiftoffRegister, LiftoffRegister, LiftoffRegister,          \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_or, OrP, LiftoffRegister, LiftoffRegister, LiftoffRegister,            \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_xor, XorP, LiftoffRegister, LiftoffRegister, LiftoffRegister,          \
    LFR_TO_REG, LFR_TO_REG, LFR_TO_REG, USE, , void)                           \
  V(i64_shl, ShiftLeftU64, LiftoffRegister, LiftoffRegister, Register,         \
    LFR_TO_REG, LFR_TO_REG, , USE, , void)                                     \
  V(i64_sar, ShiftRightS64, LiftoffRegister, LiftoffRegister, Register,        \
    LFR_TO_REG, LFR_TO_REG, , USE, , void)                                     \
  V(i64_shr, ShiftRightU64, LiftoffRegister, LiftoffRegister, Register,        \
    LFR_TO_REG, LFR_TO_REG, , USE, , void)                                     \
  V(i64_addi, AddS64, LiftoffRegister, LiftoffRegister, int64_t, LFR_TO_REG,   \
    LFR_TO_REG, Operand, USE, , void)                                          \
  V(i64_andi, AndP, LiftoffRegister, LiftoffRegister, int32_t, LFR_TO_REG,     \
    LFR_TO_REG, Operand, USE, , void)                                          \
  V(i64_ori, OrP, LiftoffRegister, LiftoffRegister, int32_t, LFR_TO_REG,       \
    LFR_TO_REG, Operand, USE, , void)                                          \
  V(i64_xori, XorP, LiftoffRegister, LiftoffRegister, int32_t, LFR_TO_REG,     \
    LFR_TO_REG, Operand, USE, , void)                                          \
  V(i64_shli, ShiftLeftU64, LiftoffRegister, LiftoffRegister, int32_t,         \
    LFR_TO_REG, LFR_TO_REG, Operand, USE, , void)                              \
  V(i64_sari, ShiftRightS64, LiftoffRegister, LiftoffRegister, int32_t,        \
    LFR_TO_REG, LFR_TO_REG, Operand, USE, , void)                              \
  V(i64_shri, ShiftRightU64, LiftoffRegister, LiftoffRegister, int32_t,        \
    LFR_TO_REG, LFR_TO_REG, Operand, USE, , void)

#define EMIT_BINOP_FUNCTION(name, instr, dtype, stype1, stype2, dcast, scast1, \
                            scast2, rcast, ret, return_type)                   \
  return_type LiftoffAssembler::emit_##name(dtype dst, stype1 lhs,             \
                                            stype2 rhs) {                      \
    auto _dst = dcast(dst);                                                    \
    auto _lhs = scast1(lhs);                                                   \
    auto _rhs = scast2(rhs);                                                   \
    instr(_dst, _lhs, _rhs);                                                   \
    rcast(_dst);                                                               \
    return ret;                                                                \
  }

BINOP_LIST(EMIT_BINOP_FUNCTION)
#undef BINOP_LIST
#undef EMIT_BINOP_FUNCTION
#undef SIGN_EXT
#undef INT32_AND_WITH_1F
#undef REGISTER_AND_WITH_1F
#undef LFR_TO_REG

void LiftoffAssembler::IncrementSmi(LiftoffRegister dst, int offset) {
  UseScratchRegisterScope temps(this);
  if (COMPRESS_POINTERS_BOOL) {
    DCHECK(SmiValuesAre31Bits());
    Register scratch = temps.Acquire();
    LoadS32(scratch, MemOperand(dst.gp(), offset));
    AddU32(scratch, Operand(Smi::FromInt(1)));
    StoreU32(scratch, MemOperand(dst.gp(), offset));
  } else {
    Register scratch = temps.Acquire();
    SmiUntag(scratch, MemOperand(dst.gp(), offset));
    AddU64(scratch, Operand(1));
    SmiTag(scratch);
    StoreU64(scratch, MemOperand(dst.gp(), offset));
  }
}

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  Label cont;

  // Check for division by zero.
  ltr(r0, rhs);
  b(eq, trap_div_by_zero);

  // Check for kMinInt / -1. This is unrepresentable.
  CmpS32(rhs, Operand(-1));
  bne(&cont);
  CmpS32(lhs, Operand(kMinInt));
  b(eq, trap_div_unrepresentable);

  bind(&cont);
  DivS32(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  // Check for division by zero.
  ltr(r0, rhs);
  beq(trap_div_by_zero);
  DivU32(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  Label cont;
  Label done;
  Label trap_div_unrepresentable;
  // Check for division by zero.
  ltr(r0, rhs);
  beq(trap_div_by_zero);

  // Check kMinInt/-1 case.
  CmpS32(rhs, Operand(-1));
  bne(&cont);
  CmpS32(lhs, Operand(kMinInt));
  beq(&trap_div_unrepresentable);

  // Continue noraml calculation.
  bind(&cont);
  ModS32(dst, lhs, rhs);
  bne(&done);

  // trap by kMinInt/-1 case.
  bind(&trap_div_unrepresentable);
  mov(dst, Operand(0));
  bind(&done);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  // Check for division by zero.
  ltr(r0, rhs);
  beq(trap_div_by_zero);
  ModU32(dst, lhs, rhs);
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  // Use r0 to check for kMinInt / -1.
  constexpr int64_t kMinInt64 = static_cast<int64_t>(1) << 63;
  Label cont;
  // Check for division by zero.
  ltgr(r0, rhs.gp());
  beq(trap_div_by_zero);

  // Check for kMinInt / -1. This is unrepresentable.
  CmpS64(rhs.gp(), Operand(-1));
  bne(&cont);
  mov(r0, Operand(kMinInt64));
  CmpS64(lhs.gp(), r0);
  b(eq, trap_div_unrepresentable);

  bind(&cont);
  DivS64(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  ltgr(r0, rhs.gp());
  b(eq, trap_div_by_zero);
  // Do div.
  DivU64(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  constexpr int64_t kMinInt64 = static_cast<int64_t>(1) << 63;

  Label trap_div_unrepresentable;
  Label done;
  Label cont;

  // Check for division by zero.
  ltgr(r0, rhs.gp());
  beq(trap_div_by_zero);

  // Check for kMinInt / -1. This is unrepresentable.
  CmpS64(rhs.gp(), Operand(-1));
  bne(&cont);
  mov(r0, Operand(kMinInt64));
  CmpS64(lhs.gp(), r0);
  beq(&trap_div_unrepresentable);

  bind(&cont);
  ModS64(dst.gp(), lhs.gp(), rhs.gp());
  bne(&done);

  bind(&trap_div_unrepresentable);
  mov(dst.gp(), Operand(0));
  bind(&done);
  return true;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  // Check for division by zero.
  ltgr(r0, rhs.gp());
  beq(trap_div_by_zero);
  ModU64(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

void LiftoffAssembler::emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  constexpr uint64_t kF64SignBit = uint64_t{1} << 63;
  UseScratchRegisterScope temps(this);
  Register scratch2 = temps.Acquire();
  MovDoubleToInt64(r0, lhs);
  // Clear sign bit in {r0}.
  AndP(r0, Operand(~kF64SignBit));

  MovDoubleToInt64(scratch2, rhs);
  // Isolate sign bit in {scratch2}.
  AndP(scratch2, Operand(kF64SignBit));
  // Combine {scratch2} into {r0}.
  OrP(r0, r0, scratch2);
  MovInt64ToDouble(dst, r0);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  constexpr uint64_t kF64SignBit = uint64_t{1} << 63;
  UseScratchRegisterScope temps(this);
  Register scratch2 = temps.Acquire();
  MovDoubleToInt64(r0, lhs);
  // Clear sign bit in {r0}.
  AndP(r0, Operand(~kF64SignBit));

  MovDoubleToInt64(scratch2, rhs);
  // Isolate sign bit in {scratch2}.
  AndP(scratch2, Operand(kF64SignBit));
  // Combine {scratch2} into {r0}.
  OrP(r0, r0, scratch2);
  MovInt64ToDouble(dst, r0);
}

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      lgfr(dst.gp(), src.gp());
      return true;
    case kExprI32SConvertF32: {
      ConvertFloat32ToInt32(dst.gp(), src.fp(),
                            kRoundToZero);  // f32 -> i32 round to zero.
      b(Condition(1), trap);
      return true;
    }
    case kExprI32UConvertF32: {
      ConvertFloat32ToUnsignedInt32(dst.gp(), src.fp(), kRoundToZero);
      b(Condition(1), trap);
      return true;
    }
    case kExprI32SConvertF64: {
      ConvertDoubleToInt32(dst.gp(), src.fp());
      b(Condition(1), trap);
      return true;
    }
    case kExprI32UConvertF64: {
      ConvertDoubleToUnsignedInt32(dst.gp(), src.fp(), kRoundToZero);
      b(Condition(1), trap);
      return true;
    }
    case kExprI32SConvertSatF32: {
      Label done, src_is_nan;
      lzer(kScratchDoubleReg);
      cebr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      // source is a finite number
      ConvertFloat32ToInt32(dst.gp(), src.fp(),
                            kRoundToZero);  // f32 -> i32 round to zero.
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI32UConvertSatF32: {
      Label done, src_is_nan;
      lzer(kScratchDoubleReg);
      cebr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      // source is a finite number
      ConvertFloat32ToUnsignedInt32(dst.gp(), src.fp(), kRoundToZero);
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI32SConvertSatF64: {
      Label done, src_is_nan;
      lzdr(kScratchDoubleReg, r0);
      cdbr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      ConvertDoubleToInt32(dst.gp(), src.fp());
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI32UConvertSatF64: {
      Label done, src_is_nan;
      lzdr(kScratchDoubleReg, r0);
      cdbr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      ConvertDoubleToUnsignedInt32(dst.gp(), src.fp());
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI32ReinterpretF32:
      lgdr(dst.gp(), src.fp());
      srlg(dst.gp(), dst.gp(), Operand(32));
      return true;
    case kExprI64SConvertI32:
      LoadS32(dst.gp(), src.gp());
      return true;
    case kExprI64UConvertI32:
      llgfr(dst.gp(), src.gp());
      return true;
    case kExprI64ReinterpretF64:
      lgdr(dst.gp(), src.fp());
      return true;
    case kExprF32SConvertI32: {
      ConvertIntToFloat(dst.fp(), src.gp());
      return true;
    }
    case kExprF32UConvertI32: {
      ConvertUnsignedIntToFloat(dst.fp(), src.gp());
      return true;
    }
    case kExprF32ConvertF64:
      ledbr(dst.fp(), src.fp());
      return true;
    case kExprF32ReinterpretI32: {
      sllg(r0, src.gp(), Operand(32));
      ldgr(dst.fp(), r0);
      return true;
    }
    case kExprF64SConvertI32: {
      ConvertIntToDouble(dst.fp(), src.gp());
      return true;
    }
    case kExprF64UConvertI32: {
      ConvertUnsignedIntToDouble(dst.fp(), src.gp());
      return true;
    }
    case kExprF64ConvertF32:
      ldebr(dst.fp(), src.fp());
      return true;
    case kExprF64ReinterpretI64:
      ldgr(dst.fp(), src.gp());
      return true;
    case kExprF64SConvertI64:
      ConvertInt64ToDouble(dst.fp(), src.gp());
      return true;
    case kExprF64UConvertI64:
      ConvertUnsignedInt64ToDouble(dst.fp(), src.gp());
      return true;
    case kExprI64SConvertF32: {
      ConvertFloat32ToInt64(dst.gp(), src.fp());  // f32 -> i64 round to zero.
      b(Condition(1), trap);
      return true;
    }
    case kExprI64UConvertF32: {
      ConvertFloat32ToUnsignedInt64(dst.gp(),
                                    src.fp());  // f32 -> i64 round to zero.
      b(Condition(1), trap);
      return true;
    }
    case kExprF32SConvertI64:
      ConvertInt64ToFloat(dst.fp(), src.gp());
      return true;
    case kExprF32UConvertI64:
      ConvertUnsignedInt64ToFloat(dst.fp(), src.gp());
      return true;
    case kExprI64SConvertF64: {
      ConvertDoubleToInt64(dst.gp(), src.fp());  // f64 -> i64 round to zero.
      b(Condition(1), trap);
      return true;
    }
    case kExprI64UConvertF64: {
      ConvertDoubleToUnsignedInt64(dst.gp(),
                                   src.fp());  // f64 -> i64 round to zero.
      b(Condition(1), trap);
      return true;
    }
    case kExprI64SConvertSatF32: {
      Label done, src_is_nan;
      lzer(kScratchDoubleReg);
      cebr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      // source is a finite number
      ConvertFloat32ToInt64(dst.gp(), src.fp());  // f32 -> i64 round to zero.
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI64UConvertSatF32: {
      Label done, src_is_nan;
      lzer(kScratchDoubleReg);
      cebr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      // source is a finite number
      ConvertFloat32ToUnsignedInt64(dst.gp(),
                                    src.fp());  // f32 -> i64 round to zero.
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI64SConvertSatF64: {
      Label done, src_is_nan;
      lzdr(kScratchDoubleReg, r0);
      cdbr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      ConvertDoubleToInt64(dst.gp(), src.fp());  // f64 -> i64 round to zero.
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    case kExprI64UConvertSatF64: {
      Label done, src_is_nan;
      lzdr(kScratchDoubleReg, r0);
      cdbr(src.fp(), kScratchDoubleReg);
      b(Condition(1), &src_is_nan);

      ConvertDoubleToUnsignedInt64(dst.gp(),
                                   src.fp());  // f64 -> i64 round to zero.
      b(&done);

      bind(&src_is_nan);
      lghi(dst.gp(), Operand::Zero());

      bind(&done);
      return true;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::emit_jump(Label* label) { b(al, label); }

void LiftoffAssembler::emit_jump(Register target) { Jump(target); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueKind kind, Register lhs,
                                      Register rhs,
                                      const FreezeCacheState& frozen) {
  bool use_signed = is_signed(cond);

  if (rhs != no_reg) {
    switch (kind) {
      case kI32:
        if (use_signed) {
          CmpS32(lhs, rhs);
        } else {
          CmpU32(lhs, rhs);
        }
        break;
      case kRef:
      case kRefNull:
      case kRtt:
        DCHECK(cond == kEqual || cond == kNotEqual);
#if defined(V8_COMPRESS_POINTERS)
        if (use_signed) {
          CmpS32(lhs, rhs);
        } else {
          CmpU32(lhs, rhs);
        }
#else
        if (use_signed) {
          CmpS64(lhs, rhs);
        } else {
          CmpU64(lhs, rhs);
        }
#endif
        break;
      case kI64:
        if (use_signed) {
          CmpS64(lhs, rhs);
        } else {
          CmpU64(lhs, rhs);
        }
        break;
      default:
        UNREACHABLE();
    }
  } else {
    DCHECK_EQ(kind, kI32);
    CHECK(use_signed);
    CmpS32(lhs, Operand::Zero());
  }

  b(to_condition(cond), label);
}

void LiftoffAssembler::emit_i32_cond_jumpi(Condition cond, Label* label,
                                           Register lhs, int32_t imm,
                                           const FreezeCacheState& frozen) {
  bool use_signed = is_signed(cond);
  if (use_signed) {
    CmpS32(lhs, Operand(imm));
  } else {
    CmpU32(lhs, Operand(imm));
  }
  b(to_condition(cond), label);
}

void LiftoffAssembler::emit_ptrsize_cond_jumpi(Condition cond, Label* label,
                                               Register lhs, int32_t imm,
                                               const FreezeCacheState& frozen) {
  bool use_signed = is_signed(cond);
  if (use_signed) {
    CmpS64(lhs, Operand(imm));
  } else {
    CmpU64(lhs, Operand(imm));
  }
  b(to_condition(cond), label);
}

#define EMIT_EQZ(test, src) \
  {                         \
    Label done;             \
    test(r0, src);          \
    mov(dst, Operand(1));   \
    beq(&done);             \
    mov(dst, Operand(0));   \
    bind(&done);            \
  }

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  EMIT_EQZ(ltr, src);
}

#define EMIT_SET_CONDITION(dst, cond) \
  {                                   \
    Label done;                       \
    lghi(dst, Operand(1));            \
    b(cond, &done);                   \
    lghi(dst, Operand(0));            \
    bind(&done);                      \
  }

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  bool use_signed = is_signed(cond);
  if (use_signed) {
    CmpS32(lhs, rhs);
  } else {
    CmpU32(lhs, rhs);
  }

  EMIT_SET_CONDITION(dst, to_condition(cond));
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  EMIT_EQZ(ltgr, src.gp());
}

void LiftoffAssembler::emit_i64_set_cond(Condition cond, Register dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  bool use_signed = is_signed(cond);
  if (use_signed) {
    CmpS64(lhs.gp(), rhs.gp());
  } else {
    CmpU64(lhs.gp(), rhs.gp());
  }

  EMIT_SET_CONDITION(dst, to_condition(cond));
}

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  cebr(lhs, rhs);
  EMIT_SET_CONDITION(dst, to_condition(cond));
}

void LiftoffAssembler::emit_f64_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  cdbr(lhs, rhs);
  EMIT_SET_CONDITION(dst, to_condition(cond));
}

void LiftoffAssembler::emit_i64_muli(LiftoffRegister dst, LiftoffRegister lhs,
                                     int32_t imm) {
  if (base::bits::IsPowerOfTwo(imm)) {
    emit_i64_shli(dst, lhs, base::bits::WhichPowerOfTwo(imm));
    return;
  }
  mov(r0, Operand(imm));
  MulS64(dst.gp(), lhs.gp(), r0);
}

bool LiftoffAssembler::emit_select(LiftoffRegister dst, Register condition,
                                   LiftoffRegister true_value,
                                   LiftoffRegister false_value,
                                   ValueKind kind) {
  return false;
}

void LiftoffAssembler::emit_smi_check(Register obj, Label* target,
                                      SmiCheckMode mode,
                                      const FreezeCacheState& frozen) {
  TestIfSmi(obj);
  Condition condition = mode == kJumpOnSmi ? eq : ne;
  b(condition, target);  // branch if SMI
}

void LiftoffAssembler::clear_i32_upper_half(Register dst) { LoadU32(dst, dst); }

#define SIMD_BINOP_RR_LIST(V)                        \
  V(f64x2_add, F64x2Add)                             \
  V(f64x2_sub, F64x2Sub)                             \
  V(f64x2_mul, F64x2Mul)                             \
  V(f64x2_div, F64x2Div)                             \
  V(f64x2_min, F64x2Min)                             \
  V(f64x2_max, F64x2Max)                             \
  V(f64x2_eq, F64x2Eq)                               \
  V(f64x2_ne, F64x2Ne)                               \
  V(f64x2_lt, F64x2Lt)                               \
  V(f64x2_le, F64x2Le)                               \
  V(f64x2_pmin, F64x2Pmin)                           \
  V(f64x2_pmax, F64x2Pmax)                           \
  V(f32x4_add, F32x4Add)                             \
  V(f32x4_sub, F32x4Sub)                             \
  V(f32x4_mul, F32x4Mul)                             \
  V(f32x4_div, F32x4Div)                             \
  V(f32x4_min, F32x4Min)                             \
  V(f32x4_max, F32x4Max)                             \
  V(f32x4_eq, F32x4Eq)                               \
  V(f32x4_ne, F32x4Ne)                               \
  V(f32x4_lt, F32x4Lt)                               \
  V(f32x4_le, F32x4Le)                               \
  V(f32x4_pmin, F32x4Pmin)                           \
  V(f32x4_pmax, F32x4Pmax)                           \
  V(i64x2_add, I64x2Add)                             \
  V(i64x2_sub, I64x2Sub)                             \
  V(i64x2_eq, I64x2Eq)                               \
  V(i64x2_ne, I64x2Ne)                               \
  V(i64x2_gt_s, I64x2GtS)                            \
  V(i64x2_ge_s, I64x2GeS)                            \
  V(i32x4_add, I32x4Add)                             \
  V(i32x4_sub, I32x4Sub)                             \
  V(i32x4_mul, I32x4Mul)                             \
  V(i32x4_eq, I32x4Eq)                               \
  V(i32x4_ne, I32x4Ne)                               \
  V(i32x4_gt_s, I32x4GtS)                            \
  V(i32x4_ge_s, I32x4GeS)                            \
  V(i32x4_gt_u, I32x4GtU)                            \
  V(i32x4_min_s, I32x4MinS)                          \
  V(i32x4_min_u, I32x4MinU)                          \
  V(i32x4_max_s, I32x4MaxS)                          \
  V(i32x4_max_u, I32x4MaxU)                          \
  V(i16x8_add, I16x8Add)                             \
  V(i16x8_sub, I16x8Sub)                             \
  V(i16x8_mul, I16x8Mul)                             \
  V(i16x8_eq, I16x8Eq)                               \
  V(i16x8_ne, I16x8Ne)                               \
  V(i16x8_gt_s, I16x8GtS)                            \
  V(i16x8_ge_s, I16x8GeS)                            \
  V(i16x8_gt_u, I16x8GtU)                            \
  V(i16x8_min_s, I16x8MinS)                          \
  V(i16x8_min_u, I16x8MinU)                          \
  V(i16x8_max_s, I16x8MaxS)                          \
  V(i16x8_max_u, I16x8MaxU)                          \
  V(i16x8_rounding_average_u, I16x8RoundingAverageU) \
  V(i8x16_add, I8x16Add)                             \
  V(i8x16_sub, I8x16Sub)                             \
  V(i8x16_eq, I8x16Eq)                               \
  V(i8x16_ne, I8x16Ne)                               \
  V(i8x16_gt_s, I8x16GtS)                            \
  V(i8x16_ge_s, I8x16GeS)                            \
  V(i8x16_gt_u, I8x16GtU)                            \
  V(i8x16_min_s, I8x16MinS)                          \
  V(i8x16_min_u, I8x16MinU)                          \
  V(i8x16_max_s, I8x16MaxS)                          \
  V(i8x16_max_u, I8x16MaxU)                          \
  V(i8x16_rounding_average_u, I8x16RoundingAverageU) \
  V(s128_and, S128And)                               \
  V(s128_or, S128Or)                                 \
  V(s128_xor, S128Xor)                               \
  V(s128_and_not, S128AndNot)

#define EMIT_SIMD_BINOP_RR(name, op)                                           \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister lhs, \
                                     LiftoffRegister rhs) {                    \
    op(dst.fp(), lhs.fp(), rhs.fp());                                          \
  }
SIMD_BINOP_RR_LIST(EMIT_SIMD_BINOP_RR)
#undef EMIT_SIMD_BINOP_RR
#undef SIMD_BINOP_RR_LIST

#define SIMD_SHIFT_RR_LIST(V) \
  V(i64x2_shl, I64x2Shl)      \
  V(i64x2_shr_s, I64x2ShrS)   \
  V(i64x2_shr_u, I64x2ShrU)   \
  V(i32x4_shl, I32x4Shl)      \
  V(i32x4_shr_s, I32x4ShrS)   \
  V(i32x4_shr_u, I32x4ShrU)   \
  V(i16x8_shl, I16x8Shl)      \
  V(i16x8_shr_s, I16x8ShrS)   \
  V(i16x8_shr_u, I16x8ShrU)   \
  V(i8x16_shl, I8x16Shl)      \
  V(i8x16_shr_s, I8x16ShrS)   \
  V(i8x16_shr_u, I8x16ShrU)

#define EMIT_SIMD_SHIFT_RR(name, op)                                           \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister lhs, \
                                     LiftoffRegister rhs) {                    \
    op(dst.fp(), lhs.fp(), rhs.gp(), kScratchDoubleReg);                       \
  }
SIMD_SHIFT_RR_LIST(EMIT_SIMD_SHIFT_RR)
#undef EMIT_SIMD_SHIFT_RR
#undef SIMD_SHIFT_RR_LIST

#define SIMD_SHIFT_RI_LIST(V) \
  V(i64x2_shli, I64x2Shl)     \
  V(i64x2_shri_s, I64x2ShrS)  \
  V(i64x2_shri_u, I64x2ShrU)  \
  V(i32x4_shli, I32x4Shl)     \
  V(i32x4_shri_s, I32x4ShrS)  \
  V(i32x4_shri_u, I32x4ShrU)  \
  V(i16x8_shli, I16x8Shl)     \
  V(i16x8_shri_s, I16x8ShrS)  \
  V(i16x8_shri_u, I16x8ShrU)  \
  V(i8x16_shli, I8x16Shl)     \
  V(i8x16_shri_s, I8x16ShrS)  \
  V(i8x16_shri_u, I8x16ShrU)

#define EMIT_SIMD_SHIFT_RI(name, op)                                           \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister lhs, \
                                     int32_t rhs) {                            \
    op(dst.fp(), lhs.fp(), Operand(rhs), r0, kScratchDoubleReg);               \
  }
SIMD_SHIFT_RI_LIST(EMIT_SIMD_SHIFT_RI)
#undef EMIT_SIMD_SHIFT_RI
#undef SIMD_SHIFT_RI_LIST

#define SIMD_UNOP_LIST(V)                                              \
  V(f64x2_splat, F64x2Splat, fp, fp, , void)                           \
  V(f64x2_abs, F64x2Abs, fp, fp, , void)                               \
  V(f64x2_neg, F64x2Neg, fp, fp, , void)                               \
  V(f64x2_sqrt, F64x2Sqrt, fp, fp, , void)                             \
  V(f64x2_ceil, F64x2Ceil, fp, fp, true, bool)                         \
  V(f64x2_floor, F64x2Floor, fp, fp, true, bool)                       \
  V(f64x2_trunc, F64x2Trunc, fp, fp, true, bool)                       \
  V(f64x2_nearest_int, F64x2NearestInt, fp, fp, true, bool)            \
  V(f64x2_convert_low_i32x4_s, F64x2ConvertLowI32x4S, fp, fp, , void)  \
  V(f64x2_convert_low_i32x4_u, F64x2ConvertLowI32x4U, fp, fp, , void)  \
  V(f32x4_abs, F32x4Abs, fp, fp, , void)                               \
  V(f32x4_splat, F32x4Splat, fp, fp, , void)                           \
  V(f32x4_neg, F32x4Neg, fp, fp, , void)                               \
  V(f32x4_sqrt, F32x4Sqrt, fp, fp, , void)                             \
  V(f32x4_ceil, F32x4Ceil, fp, fp, true, bool)                         \
  V(f32x4_floor, F32x4Floor, fp, fp, true, bool)                       \
  V(f32x4_trunc, F32x4Trunc, fp, fp, true, bool)                       \
  V(f32x4_nearest_int, F32x4NearestInt, fp, fp, true, bool)            \
  V(i64x2_abs, I64x2Abs, fp, fp, , void)                               \
  V(i64x2_splat, I64x2Splat, fp, gp, , void)                           \
  V(i64x2_neg, I64x2Neg, fp, fp, , void)                               \
  V(i64x2_sconvert_i32x4_low, I64x2SConvertI32x4Low, fp, fp, , void)   \
  V(i64x2_sconvert_i32x4_high, I64x2SConvertI32x4High, fp, fp, , void) \
  V(i64x2_uconvert_i32x4_low, I64x2UConvertI32x4Low, fp, fp, , void)   \
  V(i64x2_uconvert_i32x4_high, I64x2UConvertI32x4High, fp, fp, , void) \
  V(i32x4_abs, I32x4Abs, fp, fp, , void)                               \
  V(i32x4_neg, I32x4Neg, fp, fp, , void)                               \
  V(i32x4_splat, I32x4Splat, fp, gp, , void)                           \
  V(i32x4_sconvert_i16x8_low, I32x4SConvertI16x8Low, fp, fp, , void)   \
  V(i32x4_sconvert_i16x8_high, I32x4SConvertI16x8High, fp, fp, , void) \
  V(i32x4_uconvert_i16x8_low, I32x4UConvertI16x8Low, fp, fp, , void)   \
  V(i32x4_uconvert_i16x8_high, I32x4UConvertI16x8High, fp, fp, , void) \
  V(i16x8_abs, I16x8Abs, fp, fp, , void)                               \
  V(i16x8_neg, I16x8Neg, fp, fp, , void)                               \
  V(i16x8_splat, I16x8Splat, fp, gp, , void)                           \
  V(i16x8_sconvert_i8x16_low, I16x8SConvertI8x16Low, fp, fp, , void)   \
  V(i16x8_sconvert_i8x16_high, I16x8SConvertI8x16High, fp, fp, , void) \
  V(i16x8_uconvert_i8x16_low, I16x8UConvertI8x16Low, fp, fp, , void)   \
  V(i16x8_uconvert_i8x16_high, I16x8UConvertI8x16High, fp, fp, , void) \
  V(i8x16_abs, I8x16Abs, fp, fp, , void)                               \
  V(i8x16_neg, I8x16Neg, fp, fp, , void)                               \
  V(i8x16_splat, I8x16Splat, fp, gp, , void)                           \
  V(i8x16_popcnt, I8x16Popcnt, fp, fp, , void)                         \
  V(s128_not, S128Not, fp, fp, , void)

#define EMIT_SIMD_UNOP(name, op, dtype, stype, return_val, return_type) \
  return_type LiftoffAssembler::emit_##name(LiftoffRegister dst,        \
                                            LiftoffRegister src) {      \
    op(dst.dtype(), src.stype());                                       \
    return return_val;                                                  \
  }
SIMD_UNOP_LIST(EMIT_SIMD_UNOP)
#undef EMIT_SIMD_UNOP
#undef SIMD_UNOP_LIST

#define SIMD_EXTRACT_LANE_LIST(V)                \
  V(f64x2_extract_lane, F64x2ExtractLane, fp)    \
  V(f32x4_extract_lane, F32x4ExtractLane, fp)    \
  V(i64x2_extract_lane, I64x2ExtractLane, gp)    \
  V(i32x4_extract_lane, I32x4ExtractLane, gp)    \
  V(i16x8_extract_lane_u, I16x8ExtractLaneU, gp) \
  V(i16x8_extract_lane_s, I16x8ExtractLaneS, gp) \
  V(i8x16_extract_lane_u, I8x16ExtractLaneU, gp) \
  V(i8x16_extract_lane_s, I8x16ExtractLaneS, gp)

#define EMIT_SIMD_EXTRACT_LANE(name, op, dtype)                                \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister src, \
                                     uint8_t imm_lane_idx) {                   \
    op(dst.dtype(), src.fp(), imm_lane_idx, r0);                               \
  }
SIMD_EXTRACT_LANE_LIST(EMIT_SIMD_EXTRACT_LANE)
#undef EMIT_SIMD_EXTRACT_LANE
#undef SIMD_EXTRACT_LANE_LIST

#define SIMD_REPLACE_LANE_LIST(V)             \
  V(f64x2_replace_lane, F64x2ReplaceLane, fp) \
  V(f32x4_replace_lane, F32x4ReplaceLane, fp) \
  V(i64x2_replace_lane, I64x2ReplaceLane, gp) \
  V(i32x4_replace_lane, I32x4ReplaceLane, gp) \
  V(i16x8_replace_lane, I16x8ReplaceLane, gp) \
  V(i8x16_replace_lane, I8x16ReplaceLane, gp)

#define EMIT_SIMD_REPLACE_LANE(name, op, stype)                        \
  void LiftoffAssembler::emit_##name(                                  \
      LiftoffRegister dst, LiftoffRegister src1, LiftoffRegister src2, \
      uint8_t imm_lane_idx) {                                          \
    op(dst.fp(), src1.fp(), src2.stype(), imm_lane_idx, r0);           \
  }
SIMD_REPLACE_LANE_LIST(EMIT_SIMD_REPLACE_LANE)
#undef EMIT_SIMD_REPLACE_LANE
#undef SIMD_REPLACE_LANE_LIST

#define SIMD_EXT_MUL_LIST(V)                          \
  V(i64x2_extmul_low_i32x4_s, I64x2ExtMulLowI32x4S)   \
  V(i64x2_extmul_low_i32x4_u, I64x2ExtMulLowI32x4U)   \
  V(i64x2_extmul_high_i32x4_s, I64x2ExtMulHighI32x4S) \
  V(i64x2_extmul_high_i32x4_u, I64x2ExtMulHighI32x4U) \
  V(i32x4_extmul_low_i16x8_s, I32x4ExtMulLowI16x8S)   \
  V(i32x4_extmul_low_i16x8_u, I32x4ExtMulLowI16x8U)   \
  V(i32x4_extmul_high_i16x8_s, I32x4ExtMulHighI16x8S) \
  V(i32x4_extmul_high_i16x8_u, I32x4ExtMulHighI16x8U) \
  V(i16x8_extmul_low_i8x16_s, I16x8ExtMulLowI8x16S)   \
  V(i16x8_extmul_low_i8x16_u, I16x8ExtMulLowI8x16U)   \
  V(i16x8_extmul_high_i8x16_s, I16x8ExtMulHighI8x16S) \
  V(i16x8_extmul_high_i8x16_u, I16x8ExtMulHighI8x16U)

#define EMIT_SIMD_EXT_MUL(name, op)                                      \
  void LiftoffAssembler::emit_##name(                                    \
      LiftoffRegister dst, LiftoffRegister src1, LiftoffRegister src2) { \
    op(dst.fp(), src1.fp(), src2.fp(), kScratchDoubleReg);               \
  }
SIMD_EXT_MUL_LIST(EMIT_SIMD_EXT_MUL)
#undef EMIT_SIMD_EXT_MUL
#undef SIMD_EXT_MUL_LIST

#define SIMD_ALL_TRUE_LIST(V)    \
  V(i64x2_alltrue, I64x2AllTrue) \
  V(i32x4_alltrue, I32x4AllTrue) \
  V(i16x8_alltrue, I16x8AllTrue) \
  V(i8x16_alltrue, I8x16AllTrue)

#define EMIT_SIMD_ALL_TRUE(name, op)                        \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst,   \
                                     LiftoffRegister src) { \
    op(dst.gp(), src.fp(), r0, kScratchDoubleReg);          \
  }
SIMD_ALL_TRUE_LIST(EMIT_SIMD_ALL_TRUE)
#undef EMIT_SIMD_ALL_TRUE
#undef SIMD_ALL_TRUE_LIST

#define SIMD_ADD_SUB_SAT_LIST(V)   \
  V(i16x8_add_sat_s, I16x8AddSatS) \
  V(i16x8_sub_sat_s, I16x8SubSatS) \
  V(i16x8_add_sat_u, I16x8AddSatU) \
  V(i16x8_sub_sat_u, I16x8SubSatU) \
  V(i8x16_add_sat_s, I8x16AddSatS) \
  V(i8x16_sub_sat_s, I8x16SubSatS) \
  V(i8x16_add_sat_u, I8x16AddSatU) \
  V(i8x16_sub_sat_u, I8x16SubSatU)

#define EMIT_SIMD_ADD_SUB_SAT(name, op)                                        \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister lhs, \
                                     LiftoffRegister rhs) {                    \
    Simd128Register src1 = lhs.fp();                                           \
    Simd128Register src2 = rhs.fp();                                           \
    Simd128Register dest = dst.fp();                                           \
    /* lhs and rhs are unique based on their selection under liftoff-compiler  \
     * `EmitBinOp`. */                                                         \
    /* Make sure dst and temp are also unique. */                              \
    if (dest == src1 || dest == src2) {                                        \
      dest = GetUnusedRegister(kFpReg, LiftoffRegList{src1, src2}).fp();       \
    }                                                                          \
    Simd128Register temp =                                                     \
        GetUnusedRegister(kFpReg, LiftoffRegList{dest, src1, src2}).fp();      \
    op(dest, src1, src2, kScratchDoubleReg, temp);                             \
    /* Original dst register needs to be populated. */                         \
    if (dest != dst.fp()) {                                                    \
      vlr(dst.fp(), dest, Condition(0), Condition(0), Condition(0));           \
    }                                                                          \
  }
SIMD_ADD_SUB_SAT_LIST(EMIT_SIMD_ADD_SUB_SAT)
#undef EMIT_SIMD_ADD_SUB_SAT
#undef SIMD_ADD_SUB_SAT_LIST

#define SIMD_EXT_ADD_PAIRWISE_LIST(V)                         \
  V(i32x4_extadd_pairwise_i16x8_s, I32x4ExtAddPairwiseI16x8S) \
  V(i32x4_extadd_pairwise_i16x8_u, I32x4ExtAddPairwiseI16x8U) \
  V(i16x8_extadd_pairwise_i8x16_s, I16x8ExtAddPairwiseI8x16S) \
  V(i16x8_extadd_pairwise_i8x16_u, I16x8ExtAddPairwiseI8x16U)

#define EMIT_SIMD_EXT_ADD_PAIRWISE(name, op)                         \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst,            \
                                     LiftoffRegister src) {          \
    Simd128Register src1 = src.fp();                                 \
    Simd128Register dest = dst.fp();                                 \
    /* Make sure dst and temp are unique. */                         \
    if (dest == src1) {                                              \
      dest = GetUnusedRegister(kFpReg, LiftoffRegList{src1}).fp();   \
    }                                                                \
    Simd128Register temp =                                           \
        GetUnusedRegister(kFpReg, LiftoffRegList{dest, src1}).fp();  \
    op(dest, src1, kScratchDoubleReg, temp);                         \
    if (dest != dst.fp()) {                                          \
      vlr(dst.fp(), dest, Condition(0), Condition(0), Condition(0)); \
    }                                                                \
  }
SIMD_EXT_ADD_PAIRWISE_LIST(EMIT_SIMD_EXT_ADD_PAIRWISE)
#undef EMIT_SIMD_EXT_ADD_PAIRWISE
#undef SIMD_EXT_ADD_PAIRWISE_LIST

#define SIMD_QFM_LIST(V)   \
  V(f64x2_qfma, F64x2Qfma) \
  V(f64x2_qfms, F64x2Qfms) \
  V(f32x4_qfma, F32x4Qfma) \
  V(f32x4_qfms, F32x4Qfms)

#define EMIT_SIMD_QFM(name, op)                                        \
  void LiftoffAssembler::emit_##name(                                  \
      LiftoffRegister dst, LiftoffRegister src1, LiftoffRegister src2, \
      LiftoffRegister src3) {                                          \
    op(dst.fp(), src1.fp(), src2.fp(), src3.fp());                     \
  }
SIMD_QFM_LIST(EMIT_SIMD_QFM)
#undef EMIT_SIMD_QFM
#undef SIMD_QFM_LIST

#define SIMD_RELAXED_BINOP_LIST(V)        \
  V(i8x16_relaxed_swizzle, i8x16_swizzle) \
  V(f64x2_relaxed_min, f64x2_pmin)        \
  V(f64x2_relaxed_max, f64x2_pmax)        \
  V(f32x4_relaxed_min, f32x4_pmin)        \
  V(f32x4_relaxed_max, f32x4_pmax)        \
  V(i16x8_relaxed_q15mulr_s, i16x8_q15mulr_sat_s)

#define SIMD_VISIT_RELAXED_BINOP(name, op)                                     \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister lhs, \
                                     LiftoffRegister rhs) {                    \
    emit_##op(dst, lhs, rhs);                                                  \
  }
SIMD_RELAXED_BINOP_LIST(SIMD_VISIT_RELAXED_BINOP)
#undef SIMD_VISIT_RELAXED_BINOP
#undef SIMD_RELAXED_BINOP_LIST

#define SIMD_RELAXED_UNOP_LIST(V)                                   \
  V(i32x4_relaxed_trunc_f32x4_s, i32x4_sconvert_f32x4)              \
  V(i32x4_relaxed_trunc_f32x4_u, i32x4_uconvert_f32x4)              \
  V(i32x4_relaxed_trunc_f64x2_s_zero, i32x4_trunc_sat_f64x2_s_zero) \
  V(i32x4_relaxed_trunc_f64x2_u_zero, i32x4_trunc_sat_f64x2_u_zero)

#define SIMD_VISIT_RELAXED_UNOP(name, op)                   \
  void LiftoffAssembler::emit_##name(LiftoffRegister dst,   \
                                     LiftoffRegister src) { \
    emit_##op(dst, src);                                    \
  }
SIMD_RELAXED_UNOP_LIST(SIMD_VISIT_RELAXED_UNOP)
#undef SIMD_VISIT_RELAXED_UNOP
#undef SIMD_RELAXED_UNOP_LIST

#define F16_UNOP_LIST(V)     \
  V(f16x8_splat)             \
  V(f16x8_abs)               \
  V(f16x8_neg)               \
  V(f16x8_sqrt)              \
  V(f16x8_ceil)              \
  V(f16x8_floor)             \
  V(f16x8_trunc)             \
  V(f16x8_nearest_int)       \
  V(i16x8_sconvert_f16x8)    \
  V(i16x8_uconvert_f16x8)    \
  V(f16x8_sconvert_i16x8)    \
  V(f16x8_uconvert_i16x8)    \
  V(f16x8_demote_f32x4_zero) \
  V(f32x4_promote_low_f16x8) \
  V(f16x8_demote_f64x2_zero)

#define VISIT_F16_UNOP(name)                                \
  bool LiftoffAssembler::emit_##name(LiftoffRegister dst,   \
                                     LiftoffRegister src) { \
    return false;                                           \
  }
F16_UNOP_LIST(VISIT_F16_UNOP)
#undef VISIT_F16_UNOP
#undef F16_UNOP_LIST

#define F16_BINOP_LIST(V) \
  V(f16x8_eq)             \
  V(f16x8_ne)             \
  V(f16x8_lt)             \
  V(f16x8_le)             \
  V(f16x8_add)            \
  V(f16x8_sub)            \
  V(f16x8_mul)            \
  V(f16x8_div)            \
  V(f16x8_min)            \
  V(f16x8_max)            \
  V(f16x8_pmin)           \
  V(f16x8_pmax)

#define VISIT_F16_BINOP(name)                                                  \
  bool LiftoffAssembler::emit_##name(LiftoffRegister dst, LiftoffRegister lhs, \
                                     LiftoffRegister rhs) {                    \
    return false;                                                              \
  }
F16_BINOP_LIST(VISIT_F16_BINOP)
#undef VISIT_F16_BINOP
#undef F16_BINOP_LIST

bool LiftoffAssembler::supports_f16_mem_access() { return false; }

bool LiftoffAssembler::emit_f16x8_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_qfma(LiftoffRegister dst,
                                       LiftoffRegister src1,
                                       LiftoffRegister src2,
                                       LiftoffRegister src3) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_qfms(LiftoffRegister dst,
                                       LiftoffRegister src1,
                                       LiftoffRegister src2,
                                       LiftoffRegister src3) {
  return false;
}

void LiftoffAssembler::LoadTransform(LiftoffRegister dst, Register src_addr,
                                     Register offset_reg, uintptr_t offset_imm,
                                     LoadType type,
                                     LoadTransformationKind transform,
                                     uint32_t* protected_load_pc) {
  if (!is_int20(offset_imm)) {
    mov(ip, Operand(offset_imm));
    if (offset_reg != no_reg) {
      AddS64(ip, offset_reg);
    }
    offset_reg = ip;
    offset_imm = 0;
  }
  MemOperand src_op =
      MemOperand(src_addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm);
  *protected_load_pc = pc_offset();
  MachineType memtype = type.mem_type();
  if (transform == LoadTransformationKind::kExtend) {
    if (memtype == MachineType::Int8()) {
      LoadAndExtend8x8SLE(dst.fp(), src_op, r1);
    } else if (memtype == MachineType::Uint8()) {
      LoadAndExtend8x8ULE(dst.fp(), src_op, r1);
    } else if (memtype == MachineType::Int16()) {
      LoadAndExtend16x4SLE(dst.fp(), src_op, r1);
    } else if (memtype == MachineType::Uint16()) {
      LoadAndExtend16x4ULE(dst.fp(), src_op, r1);
    } else if (memtype == MachineType::Int32()) {
      LoadAndExtend32x2SLE(dst.fp(), src_op, r1);
    } else if (memtype == MachineType::Uint32()) {
      LoadAndExtend32x2ULE(dst.fp(), src_op, r1);
    }
  } else if (transform == LoadTransformationKind::kZeroExtend) {
    if (memtype == MachineType::Int32()) {
      LoadV32ZeroLE(dst.fp(), src_op, r1);
    } else {
      DCHECK_EQ(MachineType::Int64(), memtype);
      LoadV64ZeroLE(dst.fp(), src_op, r1);
    }
  } else {
    DCHECK_EQ(LoadTransformationKind::kSplat, transform);
    if (memtype == MachineType::Int8()) {
      LoadAndSplat8x16LE(dst.fp(), src_op, r1);
    } else if (memtype == MachineType::Int16()) {
      LoadAndSplat16x8LE(dst.fp(), src_op, r1);
    } else if (memtype == MachineType::Int32()) {
      LoadAndSplat32x4LE(dst.fp(), src_op, r1);
    } else if (memtype == MachineType::Int64()) {
      LoadAndSplat64x2LE(dst.fp(), src_op, r1);
    }
  }
}

void LiftoffAssembler::LoadLane(LiftoffRegister dst, LiftoffRegister src,
                                Register addr, Register offset_reg,
                                uintptr_t offset_imm, LoadType type,
                                uint8_t laneidx, uint32_t* protected_load_pc,
                                bool i64_offset) {
  PREP_MEM_OPERAND(offset_reg, offset_imm, ip)
  MemOperand src_op =
      MemOperand(addr, offset_reg == no_reg ? r0 : offset_reg, offset_imm);

  MachineType mem_type = type.mem_type();
  if (dst != src) {
    vlr(dst.fp(), src.fp(), Condition(0), Condition(0), Condition(0));
  }

  if (protected_load_pc) *protected_load_pc = pc_offset();
  if (mem_type == MachineType::Int8()) {
    LoadLane8LE(dst.fp(), src_op, 15 - laneidx, r1);
  } else if (mem_type == MachineType::Int16()) {
    LoadLane16LE(dst.fp(), src_op, 7 - laneidx, r1);
  } else if (mem_type == MachineType::Int32()) {
    LoadLane32LE(dst.fp(), src_op, 3 - laneidx, r1);
  } else {
    DCHECK_EQ(MachineType::Int64(), mem_type);
    LoadLane64LE(dst.fp(), src_op, 1 - laneidx, r1);
  }
}

void LiftoffAssembler::StoreLane(Register dst, Register offset,
                                 uintptr_t offset_imm, LiftoffRegister src,
                                 StoreType type, uint8_t lane,
                                 uint32_t* protected_store_pc,
                                 bool i64_offset) {
  PREP_MEM_OPERAND(offset, offset_imm, ip)
  MemOperand dst_op =
      MemOperand(dst, offset == no_reg ? r0 : offset, offset_imm);

  if (protected_store_pc) *protected_store_pc = pc_offset();

  MachineRepresentation rep = type.mem_rep();
  if (rep == MachineRepresentation::kWord8) {
    StoreLane8LE(src.fp(), dst_op, 15 - lane, r1);
  } else if (rep == MachineRepresentation::kWord16) {
    StoreLane16LE(src.fp(), dst_op, 7 - lane, r1);
  } else if (rep == MachineRepresentation::kWord32) {
    StoreLane32LE(src.fp(), dst_op, 3 - lane, r1);
  } else {
    DCHECK_EQ(MachineRepresentation::kWord64, rep);
    StoreLane64LE(src.fp(), dst_op, 1 - lane, r1);
  }
}

void LiftoffAssembler::emit_i64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  I64x2Mul(dst.fp(), lhs.fp(), rhs.fp(), r0, r1, ip);
}

void LiftoffAssembler::emit_i32x4_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  I32x4GeU(dst.fp(), lhs.fp(), rhs.fp(), kScratchDoubleReg);
}

void LiftoffAssembler::emit_i16x8_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  I16x8GeU(dst.fp(), lhs.fp(), rhs.fp(), kScratchDoubleReg);
}

void LiftoffAssembler::emit_i8x16_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  I8x16GeU(dst.fp(), lhs.fp(), rhs.fp(), kScratchDoubleReg);
}

void LiftoffAssembler::emit_i8x16_swizzle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs) {
  Simd128Register src1 = lhs.fp();
  Simd128Register src2 = rhs.fp();
  Simd128Register dest = dst.fp();
  I8x16Swizzle(dest, src1, src2, r0, r1, kScratchDoubleReg);
}

void LiftoffAssembler::emit_f64x2_promote_low_f32x4(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  F64x2PromoteLowF32x4(dst.fp(), src.fp(), kScratchDoubleReg, r0, r1, ip);
}

void LiftoffAssembler::emit_i64x2_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  I64x2BitMask(dst.gp(), src.fp(), r0, kScratchDoubleReg);
}

void LiftoffAssembler::emit_i32x4_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  I32x4BitMask(dst.gp(), src.fp(), r0, kScratchDoubleReg);
}

void LiftoffAssembler::emit_i32x4_dot_i16x8_s(LiftoffRegister dst,
                                              LiftoffRegister lhs,
                                              LiftoffRegister rhs) {
  I32x4DotI16x8S(dst.fp(), lhs.fp(), rhs.fp(), kScratchDoubleReg);
}

void LiftoffAssembler::emit_i16x8_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  I16x8BitMask(dst.gp(), src.fp(), r0, kScratchDoubleReg);
}

void LiftoffAssembler::emit_i16x8_q15mulr_sat_s(LiftoffRegister dst,
                                                LiftoffRegister src1,
                                                LiftoffRegister src2) {
  Simd128Register s1 = src1.fp();
  Simd128Register s2 = src2.fp();
  Simd128Register dest = dst.fp();
  // Make sure temp registers are unique.
  Simd128Register temp1 =
      GetUnusedRegister(kFpReg, LiftoffRegList{dest, s1, s2}).fp();
  Simd128Register temp2 =
      GetUnusedRegister(kFpReg, LiftoffRegList{dest, s1, s2, temp1}).fp();
  I16x8Q15MulRSatS(dest, s1, s2, kScratchDoubleReg, temp1, temp2);
}

void LiftoffAssembler::emit_i16x8_dot_i8x16_i7x16_s(LiftoffRegister dst,
                                                    LiftoffRegister lhs,
                                                    LiftoffRegister rhs) {
  I16x8DotI8x16S(dst.fp(), lhs.fp(), rhs.fp(), kScratchDoubleReg);
}

void LiftoffAssembler::emit_i32x4_dot_i8x16_i7x16_add_s(LiftoffRegister dst,
                                                        LiftoffRegister lhs,
                                                        LiftoffRegister rhs,
                                                        LiftoffRegister acc) {
  // Make sure temp register is unique.
  Simd128Register temp =
      GetUnusedRegister(kFpReg, LiftoffRegList{dst, lhs, rhs, acc}).fp();
  I32x4DotI8x16AddS(dst.fp(), lhs.fp(), rhs.fp(), acc.fp(), kScratchDoubleReg,
                    temp);
}

void LiftoffAssembler::emit_i8x16_shuffle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs,
                                          const uint8_t shuffle[16],
                                          bool is_swizzle) {
  // Remap the shuffle indices to match IBM lane numbering.
  // TODO(miladfarca): Put this in a function and share it with the instrction
  // selector.
  int max_index = 15;
  int total_lane_count = 2 * kSimd128Size;
  uint8_t shuffle_remapped[kSimd128Size];
  for (int i = 0; i < kSimd128Size; i++) {
    uint8_t current_index = shuffle[i];
    shuffle_remapped[i] = (current_index <= max_index
                               ? max_index - current_index
                               : total_lane_count - current_index + max_index);
  }
  uint64_t vals[2];
  memcpy(vals, shuffle_remapped, sizeof(shuffle_remapped));
#ifdef V8_TARGET_BIG_ENDIAN
  vals[0] = ByteReverse(vals[0]);
  vals[1] = ByteReverse(vals[1]);
#endif
  I8x16Shuffle(dst.fp(), lhs.fp(), rhs.fp(), vals[1], vals[0], r0, ip,
               kScratchDoubleReg);
}

void LiftoffAssembler::emit_v128_anytrue(LiftoffRegister dst,
                                         LiftoffRegister src) {
  V128AnyTrue(dst.gp(), src.fp(), r0);
}

void LiftoffAssembler::emit_i8x16_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  I8x16BitMask(dst.gp(), src.fp(), r0, ip, kScratchDoubleReg);
}

void LiftoffAssembler::emit_s128_const(LiftoffRegister dst,
                                       const uint8_t imms[16]) {
  uint64_t vals[2];
  memcpy(vals, imms, sizeof(vals));
#ifdef V8_TARGET_BIG_ENDIAN
  vals[0] = ByteReverse(vals[0]);
  vals[1] = ByteReverse(vals[1]);
#endif
  S128Const(dst.fp(), vals[1], vals[0], r0, ip);
}

void LiftoffAssembler::emit_s128_select(LiftoffRegister dst,
                                        LiftoffRegister src1,
                                        LiftoffRegister src2,
                                        LiftoffRegister mask) {
  S128Select(dst.fp(), src1.fp(), src2.fp(), mask.fp());
}

void LiftoffAssembler::emit_i32x4_sconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  I32x4SConvertF32x4(dst.fp(), src.fp(), kScratchDoubleReg, r0);
}

void LiftoffAssembler::emit_i32x4_uconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  I32x4UConvertF32x4(dst.fp(), src.fp(), kScratchDoubleReg, r0);
}

void LiftoffAssembler::emit_f32x4_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  F32x4SConvertI32x4(dst.fp(), src.fp(), kScratchDoubleReg, r0);
}

void LiftoffAssembler::emit_f32x4_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  F32x4UConvertI32x4(dst.fp(), src.fp(), kScratchDoubleReg, r0);
}

void LiftoffAssembler::emit_f32x4_demote_f64x2_zero(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  F32x4DemoteF64x2Zero(dst.fp(), src.fp(), kScratchDoubleReg, r0, r1, ip);
}

void LiftoffAssembler::emit_i8x16_sconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  I8x16SConvertI16x8(dst.fp(), lhs.fp(), rhs.fp());
}

void LiftoffAssembler::emit_i8x16_uconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  I8x16UConvertI16x8(dst.fp(), lhs.fp(), rhs.fp(), kScratchDoubleReg);
}

void LiftoffAssembler::emit_i16x8_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  I16x8SConvertI32x4(dst.fp(), lhs.fp(), rhs.fp());
}

void LiftoffAssembler::emit_i16x8_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  I16x8UConvertI32x4(dst.fp(), lhs.fp(), rhs.fp(), kScratchDoubleReg);
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_s_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  I32x4TruncSatF64x2SZero(dst.fp(), src.fp(), kScratchDoubleReg);
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_u_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  I32x4TruncSatF64x2UZero(dst.fp(), src.fp(), kScratchDoubleReg);
}

void LiftoffAssembler::emit_s128_relaxed_laneselect(LiftoffRegister dst,
                                                    LiftoffRegister src1,
                                                    LiftoffRegister src2,
                                                    LiftoffRegister mask,
                                                    int lane_width) {
  // S390 uses bytewise selection for all lane widths.
  emit_s128_select(dst, src1, src2, mask);
}

void LiftoffAssembler::set_trap_on_oob_mem64(Register index, uint64_t oob_size,
                                             uint64_t oob_index) {
  UNREACHABLE();
}

void LiftoffAssembler::StackCheck(Label* ool_code) {
  Register limit_address = ip;
  LoadStackLimit(limit_address, StackLimitKind::kInterruptStackLimit);
  CmpU64(sp, limit_address);
  b(le, ool_code);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  // Asserts unreachable within the wasm code.
  MacroAssembler::AssertUnreachable(reason);
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  MultiPush(regs.GetGpList());
  MultiPushF64OrV128(regs.GetFpList(), ip);
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  MultiPopF64OrV128(regs.GetFpList(), ip);
  MultiPop(regs.GetGpList());
}

void LiftoffAssembler::RecordSpillsInSafepoint(
    SafepointTableBuilder::Safepoint& safepoint, LiftoffRegList all_spills,
    LiftoffRegList ref_spills, int spill_offset) {
  LiftoffRegList fp_spills = all_spills & kFpCacheRegList;
  int spill_space_size = fp_spills.GetNumRegsSet() * kSimd128Size;
  LiftoffRegList gp_spills = all_spills & kGpCacheRegList;
  while (!gp_spills.is_empty()) {
    LiftoffRegister reg = gp_spills.GetLastRegSet();
    if (ref_spills.has(reg)) {
      safepoint.DefineTaggedStackSlot(spill_offset);
    }
    gp_spills.clear(reg);
    ++spill_offset;
    spill_space_size += kSystemPointerSize;
  }
  // Record the number of additional spill slots.
  RecordOolSpillSpaceSize(spill_space_size);
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  Drop(num_stack_slots);
  Ret();
}

void LiftoffAssembler::CallCWithStackBuffer(
    const std::initializer_list<VarState> args, const LiftoffRegister* rets,
    ValueKind return_kind, ValueKind out_argument_kind, int stack_bytes,
    ExternalReference ext_ref) {
  int total_size = RoundUp(stack_bytes, 8);

  int size = total_size;
  constexpr int kStackPageSize = 4 * KB;

  // Reserve space in the stack.
  while (size > kStackPageSize) {
    lay(sp, MemOperand(sp, -kStackPageSize));
    StoreU64(r0, MemOperand(sp));
    size -= kStackPageSize;
  }

  lay(sp, MemOperand(sp, -size));

  int arg_offset = 0;
  for (const VarState& arg : args) {
    MemOperand dst{sp, arg_offset};
    liftoff::StoreToMemory(this, dst, arg, ip);
    arg_offset += value_kind_size(arg.kind());
  }
  DCHECK_LE(arg_offset, stack_bytes);

  // Pass a pointer to the buffer with the arguments to the C function.
  mov(r2, sp);

  // Now call the C function.
  constexpr int kNumCCallArgs = 1;
  PrepareCallCFunction(kNumCCallArgs, no_reg);
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* result_reg = rets;
  if (return_kind != kVoid) {
    constexpr Register kReturnReg = r2;
    if (kReturnReg != rets->gp()) {
      Move(*rets, LiftoffRegister(kReturnReg), return_kind);
    }
    result_reg++;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_kind != kVoid) {
    switch (out_argument_kind) {
      case kI16:
        LoadS16(result_reg->gp(), MemOperand(sp));
        break;
      case kI32:
        LoadS32(result_reg->gp(), MemOperand(sp));
        break;
      case kI64:
      case kRefNull:
      case kRef:
      case kRtt:
        LoadU64(result_reg->gp(), MemOperand(sp));
        break;
      case kF32:
        LoadF32(result_reg->fp(), MemOperand(sp));
        break;
      case kF64:
        LoadF64(result_reg->fp(), MemOperand(sp));
        break;
      case kS128:
        LoadV128(result_reg->fp(), MemOperand(sp), ip);
        break;
      default:
        UNREACHABLE();
    }
  }
  lay(sp, MemOperand(sp, total_size));
}

void LiftoffAssembler::CallC(const std::initializer_list<VarState> args,
                             ExternalReference ext_ref) {
  // First, prepare the stack for the C call.
  int num_args = static_cast<int>(args.size());
  PrepareCallCFunction(num_args, r0);

  // Then execute the parallel register move and also move values to parameter
  // stack slots.
  int reg_args = 0;
  int stack_args = 0;
  ParallelMove parallel_move{this};
  for (const VarState& arg : args) {
    if (reg_args < int{arraysize(kCArgRegs)}) {
      parallel_move.LoadIntoRegister(LiftoffRegister{kCArgRegs[reg_args]}, arg);
      ++reg_args;
    } else {
      int bias = 0;
      // On BE machines values with less than 8 bytes are right justified.
      // bias here is relative to the stack pointer.
      if (arg.kind() == kI32 || arg.kind() == kF32) bias = -stack_bias;
      int offset =
          (kStackFrameExtraParamSlot + stack_args) * kSystemPointerSize;
      MemOperand dst{sp, offset + bias};
      liftoff::StoreToMemory(this, dst, arg, ip);
      ++stack_args;
    }
  }
  parallel_move.Execute();

  // Now call the C function.
  CallCFunction(ext_ref, num_args);
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
  DCHECK(target != no_reg);
  Call(target);
}

void LiftoffAssembler::TailCallIndirect(Register target) {
  DCHECK(target != no_reg);
  Jump(target);
}

void LiftoffAssembler::CallBuiltin(Builtin builtin) {
  // A direct call to a builtin. Just encode the builtin index. This will be
  // patched at relocation.
  Call(static_cast<Address>(builtin), RelocInfo::WASM_STUB_CALL);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  lay(sp, MemOperand(sp, -size));
  MacroAssembler::Move(addr, sp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  lay(sp, MemOperand(sp, size));
}

void LiftoffAssembler::MaybeOSR() {}

void LiftoffAssembler::emit_set_if_nan(Register dst, DoubleRegister src,
                                       ValueKind kind) {
  Label return_nan, done;
  if (kind == kF32) {
    cebr(src, src);
    bunordered(&return_nan);
  } else {
    DCHECK_EQ(kind, kF64);
    cdbr(src, src);
    bunordered(&return_nan);
  }
  b(&done);
  bind(&return_nan);
  StoreF32(src, MemOperand(dst));
  bind(&done);
}

void LiftoffAssembler::emit_s128_set_if_nan(Register dst, LiftoffRegister src,
                                            Register tmp_gp,
                                            LiftoffRegister tmp_s128,
                                            ValueKind lane_kind) {
  Label return_nan, done;
  if (lane_kind == kF32) {
    vfce(tmp_s128.fp(), src.fp(), src.fp(), Condition(1), Condition(0),
         Condition(2));
    b(Condition(0x5), &return_nan);  // If any or all are NaN.
  } else {
    DCHECK_EQ(lane_kind, kF64);
    vfce(tmp_s128.fp(), src.fp(), src.fp(), Condition(1), Condition(0),
         Condition(3));
    b(Condition(0x5), &return_nan);
  }
  b(&done);
  bind(&return_nan);
  mov(r0, Operand(1));
  StoreU32(r0, MemOperand(dst));
  bind(&done);
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
        switch (src.kind()) {
          case kI32:
          case kRef:
          case kRefNull:
          case kRtt:
          case kI64: {
            asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
            UseScratchRegisterScope temps(asm_);
            Register scratch = temps.Acquire();
            asm_->LoadU64(scratch, liftoff::GetStackSlot(slot.src_offset_));
            asm_->Push(scratch);
            break;
          }
          case kF32: {
            asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
            asm_->LoadF32(kScratchDoubleReg,
                          liftoff::GetStackSlot(slot.src_offset_ + stack_bias));
            asm_->lay(sp, MemOperand(sp, -kSystemPointerSize));
            asm_->StoreF32(kScratchDoubleReg, MemOperand(sp));
            break;
          }
          case kF64: {
            asm_->AllocateStackSpace(stack_decrement - kDoubleSize);
            asm_->LoadF64(kScratchDoubleReg,
                          liftoff::GetStackSlot(slot.src_offset_));
            asm_->push(kScratchDoubleReg);
            break;
          }
          case kS128: {
            asm_->AllocateStackSpace(stack_decrement - kSimd128Size);
            UseScratchRegisterScope temps(asm_);
            Register scratch = temps.Acquire();
            asm_->LoadV128(kScratchDoubleReg,
                           liftoff::GetStackSlot(slot.src_offset_), scratch);
            asm_->lay(sp, MemOperand(sp, -kSimd128Size));
            asm_->StoreV128(kScratchDoubleReg, MemOperand(sp), scratch);
            break;
          }
          default:
            UNREACHABLE();
        }
        break;
      }
      case LiftoffAssembler::VarState::kRegister: {
        int pushed_bytes = SlotSizeInBytes(slot);
        asm_->AllocateStackSpace(stack_decrement - pushed_bytes);
        switch (src.kind()) {
          case kI64:
          case kI32:
          case kRef:
          case kRefNull:
          case kRtt:
            asm_->push(src.reg().gp());
            break;
          case kF32:
            asm_->lay(sp, MemOperand(sp, -kSystemPointerSize));
            asm_->StoreF32(src.reg().fp(), MemOperand(sp));
            break;
          case kF64:
            asm_->push(src.reg().fp());
            break;
          case kS128: {
            UseScratchRegisterScope temps(asm_);
            Register scratch = temps.Acquire();
            asm_->lay(sp, MemOperand(sp, -kSimd128Size));
            asm_->StoreV128(src.reg().fp(), MemOperand(sp), scratch);
            break;
          }
          default:
            UNREACHABLE();
        }
        break;
      }
      case LiftoffAssembler::VarState::kIntConst: {
        asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
        DCHECK(src.kind() == kI32 || src.kind() == kI64);
        UseScratchRegisterScope temps(asm_);
        Register scratch = temps.Acquire();

        switch (src.kind()) {
          case kI32:
            asm_->mov(scratch, Operand(src.i32_const()));
            break;
          case kI64:
            asm_->mov(scratch, Operand(int64_t{slot.src_.i32_const()}));
            break;
          default:
            UNREACHABLE();
        }
        asm_->push(scratch);
        break;
      }
    }
  }
}

}  // namespace v8::internal::wasm

#undef BAILOUT

#endif  // V8_WASM_BASELINE_S390_LIFTOFF_ASSEMBLER_S390_INL_H_

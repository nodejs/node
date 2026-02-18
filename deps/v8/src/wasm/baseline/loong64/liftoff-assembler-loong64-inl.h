// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LOONG64_LIFTOFF_ASSEMBLER_LOONG64_INL_H_
#define V8_WASM_BASELINE_LOONG64_LIFTOFF_ASSEMBLER_LOONG64_INL_H_

#include "src/codegen/atomic-memory-order.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/loong64/assembler-loong64-inl.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/linkage.h"
#include "src/heap/mutable-page.h"
#include "src/wasm/baseline/liftoff-assembler.h"
#include "src/wasm/baseline/parallel-move-inl.h"
#include "src/wasm/object-access.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"

namespace v8::internal::wasm {

namespace liftoff {

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
//  -----+--------------------+---------------------------
//  -4   |     slot 0         |   ^
//  -5   |     slot 1         |   |
//       |                    | Frame slots
//       |                    |   |
//       |                    |   v
//       | optional padding slot to keep the stack 16 byte aligned.
//  -----+--------------------+  <-- stack ptr (sp)
//

inline MemOperand GetStackSlot(int offset) { return MemOperand(fp, -offset); }

inline MemOperand GetInstanceDataOperand() {
  return GetStackSlot(WasmLiftoffFrameConstants::kInstanceDataOffset);
}

inline Register CalculateActualAddress(LiftoffAssembler* lasm,
                                       UseScratchRegisterScope& temps,
                                       Register addr_reg, Register offset_reg,
                                       uintptr_t offset_imm) {
  DCHECK_NE(addr_reg, no_reg);
  if (offset_reg == no_reg && offset_imm == 0) return addr_reg;
  Register result = temps.Acquire();
  if (offset_reg == no_reg) {
    lasm->Add_d(result, addr_reg, Operand(offset_imm));
  } else {
    lasm->Add_d(result, addr_reg, Operand(offset_reg));
    if (offset_imm != 0) lasm->Add_d(result, result, Operand(offset_imm));
  }
  return result;
}

template <typename T>
inline MemOperand GetMemOp(LiftoffAssembler* assm, Register addr,
                           Register offset, T offset_imm,
                           bool i64_offset = false, unsigned shift_amount = 0) {
  if (offset != no_reg) {
    if (!i64_offset) {
      assm->bstrpick_d(kScratchReg, offset, 31, 0);
      offset = kScratchReg;
    }
    if (shift_amount != 0) {
      assm->alsl_d(kScratchReg, offset, addr, shift_amount);
    } else {
      assm->add_d(kScratchReg, offset, addr);
    }
    addr = kScratchReg;
  }
  if (is_int31(offset_imm)) {
    int32_t offset_imm32 = static_cast<int32_t>(offset_imm);
    return MemOperand(addr, offset_imm32);
  } else {
    assm->li(kScratchReg2, Operand(offset_imm));
    assm->add_d(kScratchReg2, addr, kScratchReg2);
    return MemOperand(kScratchReg2, 0);
  }
}

inline void Load(LiftoffAssembler* assm, LiftoffRegister dst, MemOperand src,
                 ValueKind kind) {
  switch (kind) {
    case kI16:
      assm->Ld_h(dst.gp(), src);
      break;
    case kI32:
      assm->Ld_w(dst.gp(), src);
      break;
    case kI64:
    case kRef:
    case kRefNull:
      assm->Ld_d(dst.gp(), src);
      break;
    case kF32:
      assm->Fld_s(dst.fp(), src);
      break;
    case kF64:
      assm->Fld_d(dst.fp(), src);
      break;
    case kS128:
      assm->Vld(dst.fp().toV(), src);
      break;
    default:
      UNREACHABLE();
  }
}

inline void Store(LiftoffAssembler* assm, MemOperand dst, LiftoffRegister src,
                  ValueKind kind) {
  switch (kind) {
    case kI16:
      assm->St_h(src.gp(), dst);
      break;
    case kI32:
      assm->St_w(src.gp(), dst);
      break;
    case kI64:
    case kRefNull:
    case kRef:
      assm->St_d(src.gp(), dst);
      break;
    case kF32:
      assm->Fst_s(src.fp(), dst);
      break;
    case kF64:
      assm->Fst_d(src.fp(), dst);
      break;
    case kS128:
      assm->Vst(src.fp().toV(), dst);
      break;
    default:
      UNREACHABLE();
  }
}

inline void Store(LiftoffAssembler* assm, Register base, int32_t offset,
                  LiftoffRegister src, ValueKind kind) {
  MemOperand dst(base, offset);
  Store(assm, dst, src, kind);
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueKind kind) {
  switch (kind) {
    case kI32:
      assm->addi_d(sp, sp, -kSystemPointerSize);
      assm->St_w(reg.gp(), MemOperand(sp, 0));
      break;
    case kI64:
    case kRefNull:
    case kRef:
      assm->Push(reg.gp());
      break;
    case kF32:
      assm->addi_d(sp, sp, -kSystemPointerSize);
      assm->Fst_s(reg.fp(), MemOperand(sp, 0));
      break;
    case kF64:
      assm->addi_d(sp, sp, -kSystemPointerSize);
      assm->Fst_d(reg.fp(), MemOperand(sp, 0));
      break;
    case kS128:
      assm->addi_d(sp, sp, -kSimd128Size);
      assm->Vst(reg.fp().toV(), MemOperand(sp, 0));
      break;
    default:
      UNREACHABLE();
  }
}

inline void StoreToMemory(LiftoffAssembler* assm, MemOperand dst,
                          const LiftoffAssembler::VarState& src) {
  if (src.is_reg()) {
    Store(assm, dst, src.reg(), src.kind());
    return;
  }

  UseScratchRegisterScope temps(assm);
  Register temp = temps.Acquire();
  if (src.is_const()) {
    if (src.i32_const() == 0) {
      temp = zero_reg;
    } else {
      assm->li(temp, static_cast<int64_t>(src.i32_const()));
    }
  } else {
    DCHECK(src.is_stack());
    if (value_kind_size(src.kind()) == 4) {
      assm->Ld_w(temp, liftoff::GetStackSlot(src.offset()));
    } else {
      assm->Ld_d(temp, liftoff::GetStackSlot(src.offset()));
    }
  }

  if (value_kind_size(src.kind()) == 4) {
    assm->St_w(temp, dst);
  } else {
    DCHECK_EQ(8, value_kind_size(src.kind()));
    assm->St_d(temp, dst);
  }
}

static_assert(!kLiftoffAssemblerFpCacheRegs.has(kScratchDoubleReg));
static_assert(!kLiftoffAssemblerFpCacheRegs.has(kScratchDoubleReg2));

}  // namespace liftoff

int LiftoffAssembler::PrepareStackFrame() {
  int offset = pc_offset();
  // When constant that represents size of stack frame can't be represented
  // as 16bit we need three instructions to add it to sp, so we reserve space
  // for this case.
  addi_d(sp, sp, 0);
  nop();
  nop();
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

  // On LOONG64, we must push at least {ra} before calling the stub, otherwise
  // it would get clobbered with no possibility to recover it. So just set
  // up the frame here.
  EnterFrame(StackFrame::WASM);
  LoadConstant(LiftoffRegister(kLiftoffFrameSetupFunctionReg),
               WasmValue(declared_function_index));
  CallBuiltin(Builtin::kWasmLiftoffFrameSetup);
}

void LiftoffAssembler::PrepareTailCall(int num_callee_stack_params,
                                       int stack_param_delta) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();

  // Push the return address and frame pointer to complete the stack frame.
  Ld_d(scratch, MemOperand(fp, 8));
  Push(scratch);
  Ld_d(scratch, MemOperand(fp, 0));
  Push(scratch);

  // Shift the whole frame upwards.
  int slot_count = num_callee_stack_params + 2;
  for (int i = slot_count - 1; i >= 0; --i) {
    Ld_d(scratch, MemOperand(sp, i * 8));
    St_d(scratch, MemOperand(fp, (i - stack_param_delta) * 8));
  }

  // Set the new stack and frame pointer.
  addi_d(sp, fp, -stack_param_delta * 8);
  Pop(ra, fp);
}

void LiftoffAssembler::AlignFrameSize() {}

void LiftoffAssembler::PatchPrepareStackFrame(
    int offset, SafepointTableBuilder* safepoint_table_builder,
    bool feedback_vector_slot, size_t stack_param_slots) {
  // The frame_size includes the frame marker and the instance slot. Both are
  // pushed as part of frame construction, so we don't need to allocate memory
  // for them anymore.
  int frame_size = GetTotalFrameSize() - 2 * kSystemPointerSize;
  // The frame setup builtin also pushes the feedback vector.
  if (feedback_vector_slot) {
    frame_size -= kSystemPointerSize;
  }

  // We can't run out of space, just pass anything big enough to not cause the
  // assembler to try to grow the buffer.
  constexpr int kAvailableSpace = 256;
  MacroAssembler patching_assembler(
      zone(), AssemblerOptions{}, CodeObjectRequired::kNo,
      ExternalAssemblerBuffer(buffer_start_ + offset, kAvailableSpace));

  if (V8_LIKELY(frame_size < 4 * KB)) {
    // This is the standard case for small frames: just subtract from SP and be
    // done with it.
    patching_assembler.Add_d(sp, sp, Operand(-frame_size));
    return;
  }

  // The frame size is bigger than 4KB, so we might overflow the available stack
  // space if we first allocate the frame and then do the stack check (we will
  // need some remaining stack space for throwing the exception). That's why we
  // check the available stack space before we allocate the frame. To do this we
  // replace the {__ Add_d(sp, sp, -frame_size)} with a jump to OOL code that
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
  CHECK(is_int26(imm32));
  patching_assembler.b(imm32 >> 2);

  // If the frame is bigger than the stack, we throw the stack overflow
  // exception unconditionally. Thereby we can avoid the integer overflow
  // check in the condition code.
  RecordComment("OOL: stack check for large frame");
  Label continuation;
  if (frame_size < v8_flags.stack_size * 1024) {
    Register stack_limit = kScratchReg;
    LoadStackLimit(stack_limit, StackLimitKind::kRealStackLimit);
    Add_d(stack_limit, stack_limit, Operand(frame_size));
    Branch(&continuation, uge, sp, Operand(stack_limit));
  }

  if (v8_flags.experimental_wasm_growable_stacks) {
    LiftoffRegList regs_to_save;
    regs_to_save.set(WasmHandleStackOverflowDescriptor::GapRegister());
    regs_to_save.set(WasmHandleStackOverflowDescriptor::FrameBaseRegister());
    for (auto reg : kGpParamRegisters) regs_to_save.set(reg);
    for (auto reg : kFpParamRegisters) regs_to_save.set(reg);
    PushRegisters(regs_to_save);
    li(WasmHandleStackOverflowDescriptor::GapRegister(), frame_size);
    Add_d(WasmHandleStackOverflowDescriptor::FrameBaseRegister(), fp,
          Operand(stack_param_slots * kSystemPointerSize +
                  CommonFrameConstants::kFixedFrameSizeAboveFp));
    CallBuiltin(Builtin::kWasmHandleStackOverflow);
    safepoint_table_builder->DefineSafepoint(this);
    PopRegisters(regs_to_save);
  } else {
    Call(static_cast<Address>(Builtin::kWasmStackOverflow),
         RelocInfo::WASM_STUB_CALL);
    // The call will not return; just define an empty safepoint.
    safepoint_table_builder->DefineSafepoint(this);
    if (v8_flags.debug_code) stop();
  }

  bind(&continuation);

  // Now allocate the stack space. Note that this might do more than just
  // decrementing the SP;
  Add_d(sp, sp, Operand(-frame_size));

  // Jump back to the start of the function, from {pc_offset()} to
  // right after the reserved space for the {__ Add_d(sp, sp, -framesize)}
  // (which is a Branch now).
  int func_start_offset = offset + 3 * kInstrSize;
  imm32 = func_start_offset - pc_offset();
  CHECK(is_int26(imm32));
  b(imm32 >> 2);
}

void LiftoffAssembler::FinishCode() {}

void LiftoffAssembler::AbortCompilation() {}

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
  return kind == kS128 || is_reference(kind);
}

void LiftoffAssembler::CheckTierUp(int declared_func_index, int budget_used,
                                   Label* ool_label,
                                   const FreezeCacheState& frozen) {
  Register budget_array = kScratchReg;

  Register instance_data = cache_state_.cached_instance_data;
  if (instance_data == no_reg) {
    instance_data = budget_array;  // Reuse the scratch register.
    LoadInstanceDataFromFrame(instance_data);
  }

  constexpr int kArrayOffset = wasm::ObjectAccess::ToTagged(
      WasmTrustedInstanceData::kTieringBudgetArrayOffset);
  Ld_d(budget_array, MemOperand(instance_data, kArrayOffset));

  int budget_arr_offset = kInt32Size * declared_func_index;

  Register budget = kScratchReg2;
  MemOperand budget_addr(budget_array, budget_arr_offset);
  Ld_w(budget, budget_addr);
  Sub_w(budget, budget, budget_used);
  St_w(budget, budget_addr);

  Branch(ool_label, less, budget, Operand(zero_reg));
}

Register LiftoffAssembler::LoadOldFramePointer() {
  if (!v8_flags.experimental_wasm_growable_stacks) {
    return fp;
  }

  LiftoffRegister old_fp = GetUnusedRegister(RegClass::kGpReg, {});
  FreezeCacheState frozen(*this);
  Label done, call_runtime;
  Ld_d(old_fp.gp(), MemOperand(fp, TypedFrameConstants::kFrameTypeOffset));
  BranchShort(
      &call_runtime, eq, old_fp.gp(),
      Operand(StackFrame::TypeToMarker(StackFrame::WASM_SEGMENT_START)));
  mov(old_fp.gp(), fp);
  jmp(&done);

  bind(&call_runtime);
  LiftoffRegList regs_to_save = cache_state()->used_registers;
  PushRegisters(regs_to_save);
  li(kCArgRegs[0], ExternalReference::isolate_address());
  PrepareCallCFunction(1, kScratchReg);
  CallCFunction(ExternalReference::wasm_load_old_fp(), 1);
  if (old_fp.gp() != kReturnRegister0) {
    mov(old_fp.gp(), kReturnRegister0);
  }
  PopRegisters(regs_to_save);

  bind(&done);
  return old_fp.gp();
}

void LiftoffAssembler::CheckStackShrink() {
  Label done;
  {
    UseScratchRegisterScope temps{this};
    Register scratch = temps.Acquire();
    Ld_d(scratch, MemOperand(fp, TypedFrameConstants::kFrameTypeOffset));
    BranchShort(
        &done, ne, scratch,
        Operand(StackFrame::TypeToMarker(StackFrame::WASM_SEGMENT_START)));
  }
  LiftoffRegList regs_to_save;
  for (auto reg : kGpReturnRegisters) regs_to_save.set(reg);
  for (auto reg : kFpReturnRegisters) regs_to_save.set(reg);
  PushRegisters(regs_to_save);
  li(kCArgRegs[0], ExternalReference::isolate_address());
  PrepareCallCFunction(1, kScratchReg);
  CallCFunction(ExternalReference::wasm_shrink_stack(), 1);
  mov(fp, kReturnRegister0);
  PopRegisters(regs_to_save);
  bind(&done);
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value) {
  switch (value.type().kind()) {
    case kI32:
      MacroAssembler::li(reg.gp(), Operand(value.to_i32()));
      break;
    case kI64:
      MacroAssembler::li(reg.gp(), Operand(value.to_i64()));
      break;
    case kF32:
      MacroAssembler::Move(reg.fp(), value.to_f32_boxed().get_bits());
      break;
    case kF64:
      MacroAssembler::Move(reg.fp(), value.to_f64_boxed().get_bits());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadInstanceDataFromFrame(Register dst) {
  Ld_d(dst, liftoff::GetInstanceDataOperand());
}

void LiftoffAssembler::LoadTrustedPointer(Register dst, Register src_addr,
                                          int offset, IndirectPointerTag tag) {
  MemOperand src{src_addr, offset};
  LoadTrustedPointerField(dst, src, tag);
}

void LiftoffAssembler::LoadFromInstance(Register dst, Register instance,
                                        int offset, int size) {
  DCHECK_LE(0, offset);
  switch (size) {
    case 1:
      Ld_b(dst, MemOperand(instance, offset));
      break;
    case 4:
      Ld_w(dst, MemOperand(instance, offset));
      break;
    case 8:
      Ld_d(dst, MemOperand(instance, offset));
      break;
    default:
      UNIMPLEMENTED();
  }
}

void LiftoffAssembler::LoadTaggedPointerFromInstance(Register dst,
                                                     Register instance,
                                                     int32_t offset) {
  LoadTaggedField(dst, MemOperand(instance, offset));
}

void LiftoffAssembler::SpillInstanceData(Register instance) {
  St_d(instance, liftoff::GetInstanceDataOperand());
}

void LiftoffAssembler::ResetOSRTarget() {}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         int32_t offset_imm,
                                         uint32_t* protected_load_pc,
                                         bool needs_shift) {
  unsigned shift_amount = !needs_shift ? 0 : COMPRESS_POINTERS_BOOL ? 2 : 3;
  MemOperand src_op = liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm,
                                        false, shift_amount);
  LoadTaggedField(dst, src_op, reinterpret_cast<int*>(protected_load_pc));
  DCHECK_IMPLIES(protected_load_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(*protected_load_pc +
                                                            buffer_start_))
                     ->IsLoad());
}

void LiftoffAssembler::LoadProtectedPointer(Register dst, Register src_addr,
                                            int32_t offset_imm) {
  LoadProtectedPointerField(dst, MemOperand{src_addr, offset_imm});
}

void LiftoffAssembler::LoadFullPointer(Register dst, Register src_addr,
                                       int32_t offset_imm) {
  MemOperand src_op = liftoff::GetMemOp(this, src_addr, no_reg, offset_imm);
  Ld_d(dst, src_op);
}

#ifdef V8_ENABLE_SANDBOX
void LiftoffAssembler::LoadCodeEntrypointViaCodePointer(Register dst,
                                                        Register src_addr,
                                                        int32_t offset_imm) {
  MemOperand src_op = liftoff::GetMemOp(this, src_addr, no_reg, offset_imm);
  MacroAssembler::LoadCodeEntrypointViaCodePointer(dst, src_op,
                                                   kWasmEntrypointTag);
}
#endif

void LiftoffAssembler::StoreTaggedPointer(Register dst_addr,
                                          Register offset_reg,
                                          int32_t offset_imm, Register src,
                                          LiftoffRegList pinned,
                                          uint32_t* protected_store_pc,
                                          SkipWriteBarrier skip_write_barrier) {
  UseScratchRegisterScope temps(this);
  Operand offset_op = Operand(offset_imm);
  // For the write barrier (below), we cannot have both an offset register and
  // an immediate offset. Add them to a 32-bit offset initially, but in a 64-bit
  // register, because that's needed in the MemOperand below.
  if (offset_reg.is_valid()) {
    Register effective_offset = temps.Acquire();
    // TODO(loong64): Check if zero-extension is needed here.
    bstrpick_d(effective_offset, offset_reg, 31, 0);
    if (offset_imm) {
      Add_d(effective_offset, effective_offset, Operand(offset_imm));
    }
    offset_op = Operand(effective_offset);
  }

  if (offset_op.is_reg()) {
    StoreTaggedField(src, MemOperand(dst_addr, offset_op.rm()),
                     reinterpret_cast<int*>(protected_store_pc));
  } else {
    StoreTaggedField(src, MemOperand(dst_addr, offset_imm),
                     reinterpret_cast<int*>(protected_store_pc));
  }
  DCHECK_IMPLIES(protected_store_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(
                                     *protected_store_pc + buffer_start_))
                     ->IsStore());

  if (v8_flags.disable_write_barriers) return;

  if (skip_write_barrier) {
    if (v8_flags.verify_write_barriers) {
      CallVerifySkippedWriteBarrierStubSaveRegisters(dst_addr, src,
                                                     SaveFPRegsMode::kSave);
    }
    return;
  }

  Label exit;
  JumpIfSmi(src, &exit);
  CheckPageFlag(dst_addr, MemoryChunk::kPointersFromHereAreInterestingMask,
                kZero, &exit);
  CheckPageFlag(src, MemoryChunk::kPointersToHereAreInterestingMask, kZero,
                &exit);
  CallRecordWriteStubSaveRegisters(dst_addr, offset_op, SaveFPRegsMode::kSave,
                                   StubCallMode::kCallWasmRuntimeStub);
  bind(&exit);
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uintptr_t offset_imm,
                            LoadType type, uint32_t* protected_load_pc,
                            bool is_load_mem, bool i64_offset,
                            bool needs_shift) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  unsigned shift_amount = needs_shift ? type.size_log_2() : 0;
  MemOperand src_op = liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm,
                                        i64_offset, shift_amount);

  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U:
      Ld_bu(dst.gp(), src_op, reinterpret_cast<int*>(protected_load_pc));
      break;
    case LoadType::kI32Load8S:
    case LoadType::kI64Load8S:
      Ld_b(dst.gp(), src_op, reinterpret_cast<int*>(protected_load_pc));
      break;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      MacroAssembler::Ld_hu(dst.gp(), src_op,
                            reinterpret_cast<int*>(protected_load_pc));
      break;
    case LoadType::kI32Load16S:
    case LoadType::kI64Load16S:
      MacroAssembler::Ld_h(dst.gp(), src_op,
                           reinterpret_cast<int*>(protected_load_pc));
      break;
    case LoadType::kI64Load32U:
      MacroAssembler::Ld_wu(dst.gp(), src_op,
                            reinterpret_cast<int*>(protected_load_pc));
      break;
    case LoadType::kI32Load:
    case LoadType::kI64Load32S:
      MacroAssembler::Ld_w(dst.gp(), src_op,
                           reinterpret_cast<int*>(protected_load_pc));
      break;
    case LoadType::kI64Load:
      MacroAssembler::Ld_d(dst.gp(), src_op,
                           reinterpret_cast<int*>(protected_load_pc));
      break;
    case LoadType::kF32Load:
      MacroAssembler::Fld_s(dst.fp(), src_op,
                            reinterpret_cast<int*>(protected_load_pc));
      break;
    case LoadType::kF32LoadF16:
      UNIMPLEMENTED();
      break;
    case LoadType::kF64Load:
      MacroAssembler::Fld_d(dst.fp(), src_op,
                            reinterpret_cast<int*>(protected_load_pc));
      break;
    case LoadType::kS128Load:
      MacroAssembler::Vld(dst.fp().toV(), src_op,
                          reinterpret_cast<int*>(protected_load_pc));
      break;
    default:
      UNREACHABLE();
  }
  DCHECK_IMPLIES(protected_load_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(*protected_load_pc +
                                                            buffer_start_))
                     ->IsLoad());
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uintptr_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem,
                             bool i64_offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  MemOperand dst_op =
      liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm, i64_offset);

  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      St_b(src.gp(), dst_op, reinterpret_cast<int*>(protected_store_pc));
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      MacroAssembler::St_h(src.gp(), dst_op,
                           reinterpret_cast<int*>(protected_store_pc));
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      MacroAssembler::St_w(src.gp(), dst_op,
                           reinterpret_cast<int*>(protected_store_pc));
      break;
    case StoreType::kI64Store:
      MacroAssembler::St_d(src.gp(), dst_op,
                           reinterpret_cast<int*>(protected_store_pc));
      break;
    case StoreType::kF32Store:
      MacroAssembler::Fst_s(src.fp(), dst_op,
                            reinterpret_cast<int*>(protected_store_pc));
      break;
    case StoreType::kF32StoreF16:
      UNIMPLEMENTED();
      break;
    case StoreType::kF64Store:
      MacroAssembler::Fst_d(src.fp(), dst_op,
                            reinterpret_cast<int*>(protected_store_pc));
      break;
    case StoreType::kS128Store:
      MacroAssembler::Vst(src.fp().toV(), dst_op,
                          reinterpret_cast<int*>(protected_store_pc));
      break;
    default:
      UNREACHABLE();
  }
  DCHECK_IMPLIES(protected_store_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(
                                     *protected_store_pc + buffer_start_))
                     ->IsStore());
}

void LiftoffAssembler::AtomicLoad(LiftoffRegister dst, Register src_addr,
                                  Register offset_reg, uintptr_t offset_imm,
                                  LoadType type, uint32_t* protected_load_pc,
                                  AtomicMemoryOrder memory_order,
                                  LiftoffRegList pinned, bool i64_offset,
                                  Endianness /* endianness */) {
  DCHECK(memory_order == AtomicMemoryOrder::kSeqCst ||
         memory_order == AtomicMemoryOrder::kAcqRel);
  int dbar_hint = memory_order == AtomicMemoryOrder::kSeqCst ? 0x10 : 0x14;
  BlockTrampolinePoolScope block_trampoline_pool(this);
  UseScratchRegisterScope temps(this);
  MemOperand src_op =
      liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm, i64_offset);

  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U: {
      Ld_bu(dst.gp(), src_op, reinterpret_cast<int*>(protected_load_pc));
      dbar(dbar_hint);
      break;
    }
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U: {
      Ld_hu(dst.gp(), src_op, reinterpret_cast<int*>(protected_load_pc));
      dbar(dbar_hint);
      break;
    }
    case LoadType::kI32Load: {
      Ld_w(dst.gp(), src_op, reinterpret_cast<int*>(protected_load_pc));
      dbar(dbar_hint);
      break;
    }
    case LoadType::kI64Load32U: {
      Ld_wu(dst.gp(), src_op, reinterpret_cast<int*>(protected_load_pc));
      dbar(dbar_hint);
      break;
    }
    case LoadType::kI64Load: {
      Ld_d(dst.gp(), src_op, reinterpret_cast<int*>(protected_load_pc));
      dbar(dbar_hint);
      break;
    }
    case LoadType::kI32Load8S:
      Ld_b(dst.gp(), src_op, reinterpret_cast<int*>(protected_load_pc));
      dbar(dbar_hint);
      break;
    case LoadType::kI32Load16S:
      Ld_h(dst.gp(), src_op, reinterpret_cast<int*>(protected_load_pc));
      dbar(dbar_hint);
      break;
    default:
      UNREACHABLE();
  }
  DCHECK_IMPLIES(protected_load_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(*protected_load_pc +
                                                            buffer_start_))
                     ->IsLoad());
}

void LiftoffAssembler::AtomicLoadTaggedPointer(Register dst, Register src_addr,
                                               Register offset_reg,
                                               int32_t offset_imm,
                                               AtomicMemoryOrder memory_order,
                                               uint32_t* protected_load_pc,
                                               bool needs_shift) {
  DCHECK(memory_order == AtomicMemoryOrder::kSeqCst ||
         memory_order == AtomicMemoryOrder::kAcqRel);
  int dbar_hint = memory_order == AtomicMemoryOrder::kSeqCst ? 0x10 : 0x14;
  BlockTrampolinePoolScope block_trampoline_pool(this);
  MemOperand src_op = liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm);

#if V8_COMPRESS_POINTERS
  Ld_w(dst, src_op, reinterpret_cast<int*>(protected_load_pc));
  dbar(dbar_hint);
  DecompressTagged(dst, dst);
#else
  Ld_d(dst, src_op, reinterpret_cast<int*>(protected_load_pc));
  dbar(dbar_hint);
#endif
  DCHECK_IMPLIES(protected_load_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(*protected_load_pc +
                                                            buffer_start_))
                     ->IsLoad());
}

void LiftoffAssembler::AtomicStore(Register dst_addr, Register offset_reg,
                                   uintptr_t offset_imm, LiftoffRegister src,
                                   StoreType type, uint32_t* protected_store_pc,
                                   AtomicMemoryOrder memory_order,
                                   LiftoffRegList pinned, bool i64_offset,
                                   Endianness /* endianness */) {
  DCHECK(memory_order == AtomicMemoryOrder::kSeqCst ||
         memory_order == AtomicMemoryOrder::kAcqRel);
  int dbar_hint = memory_order == AtomicMemoryOrder::kSeqCst ? 0x10 : 0x12;
  BlockTrampolinePoolScope block_trampoline_pool(this);
  UseScratchRegisterScope temps(this);
  MemOperand dst_op =
      liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm, i64_offset);

  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8: {
      dbar(dbar_hint);
      St_b(src.gp(), dst_op, reinterpret_cast<int*>(protected_store_pc));
      break;
    }
    case StoreType::kI64Store16:
    case StoreType::kI32Store16: {
      dbar(dbar_hint);
      St_h(src.gp(), dst_op, reinterpret_cast<int*>(protected_store_pc));
      break;
    }
    case StoreType::kI64Store32:
    case StoreType::kI32Store: {
      dbar(dbar_hint);
      St_w(src.gp(), dst_op, reinterpret_cast<int*>(protected_store_pc));
      break;
    }
    case StoreType::kI64Store: {
      dbar(dbar_hint);
      St_d(src.gp(), dst_op, reinterpret_cast<int*>(protected_store_pc));
      break;
    }
    default:
      UNREACHABLE();
  }
  if (memory_order == AtomicMemoryOrder::kSeqCst) dbar(dbar_hint);
  DCHECK_IMPLIES(protected_store_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(
                                     *protected_store_pc + buffer_start_))
                     ->IsStore());
}

void LiftoffAssembler::AtomicStoreTaggedPointer(
    Register dst_addr, Register offset_reg, int32_t offset_imm, Register src,
    LiftoffRegList pinned, AtomicMemoryOrder memory_order,
    uint32_t* protected_store_pc) {
  DCHECK(memory_order == AtomicMemoryOrder::kSeqCst ||
         memory_order == AtomicMemoryOrder::kAcqRel);
  int dbar_hint = memory_order == AtomicMemoryOrder::kSeqCst ? 0x10 : 0x12;
  BlockTrampolinePoolScope block_trampoline_pool(this);
  MemOperand dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm);

  if (COMPRESS_POINTERS_BOOL) {
    dbar(dbar_hint);
    St_w(src, dst_op, reinterpret_cast<int*>(protected_store_pc));
  } else {
    dbar(dbar_hint);
    St_d(src, dst_op, reinterpret_cast<int*>(protected_store_pc));
  }
  if (memory_order == AtomicMemoryOrder::kSeqCst) dbar(dbar_hint);
  DCHECK_IMPLIES(protected_store_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(
                                     *protected_store_pc + buffer_start_))
                     ->IsStore());

  if (v8_flags.disable_write_barriers) return;
  // The write barrier.
  Label exit;
  JumpIfSmi(src, &exit);
  CheckPageFlag(dst_addr, MemoryChunk::kPointersFromHereAreInterestingMask,
                kZero, &exit);
  CheckPageFlag(src, MemoryChunk::kPointersToHereAreInterestingMask, kZero,
                &exit);

  Operand offset_op = Operand(offset_imm);
  UseScratchRegisterScope temps(this);
  if (offset_reg.is_valid()) {
    Register scratch = temps.Acquire();
    bstrpick_d(scratch, offset_reg, 31, 0);
    if (offset_imm) {
      Add_d(scratch, scratch, Operand(offset_imm));
    }
    offset_op = Operand(scratch);
  }
  CallRecordWriteStubSaveRegisters(dst_addr, offset_op, SaveFPRegsMode::kSave,
                                   StubCallMode::kCallWasmRuntimeStub);
  bind(&exit);
}

#define ASSEMBLE_ATOMIC_BINOP_EXT(load_linked, store_conditional, size, \
                                  bin_instr, aligned)                   \
  do {                                                                  \
    Label binop;                                                        \
    andi(temp3, temp0, aligned);                                        \
    Sub_d(temp0, temp0, Operand(temp3));                                \
    slli_w(temp3, temp3, 3);                                            \
    dbar(0);                                                            \
    bind(&binop);                                                       \
    load_linked(temp1, MemOperand(temp0, 0),                            \
                reinterpret_cast<int*>(protected_load_pc));             \
    ExtractBits(result.gp(), temp1, temp3, size, false);                \
    bin_instr(temp2, result.gp(), Operand(value.gp()));                 \
    InsertBits(temp1, temp2, temp3, size);                              \
    store_conditional(temp1, MemOperand(temp0, 0));                     \
    BranchShort(&binop, eq, temp1, Operand(zero_reg));                  \
    dbar(0);                                                            \
  } while (0)

#define ATOMIC_BINOP_CASE(name, inst32, inst64, opcode)                        \
  void LiftoffAssembler::Atomic##name(                                         \
      Register dst_addr, Register offset_reg, uintptr_t offset_imm,            \
      LiftoffRegister value, LiftoffRegister result, StoreType type,           \
      uint32_t* protected_load_pc, bool i64_offset,                            \
      Endianness /* endianness */) {                                           \
    LiftoffRegList pinned{dst_addr, value, result};                            \
    if (offset_reg != no_reg) pinned.set(offset_reg);                          \
    Register temp0 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();       \
    Register temp1 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();       \
    Register temp2 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();       \
    Register temp3 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();       \
    MemOperand dst_op =                                                        \
        liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm, i64_offset); \
    Add_d(temp0, dst_op.base(), dst_op.offset());                              \
    switch (type.value()) {                                                    \
      case StoreType::kI64Store8:                                              \
        DCHECK_NULL(protected_load_pc);                                        \
        ASSEMBLE_ATOMIC_BINOP_EXT(Ll_d, Sc_d, 8, inst64, 7);                   \
        break;                                                                 \
      case StoreType::kI32Store8:                                              \
        DCHECK_NULL(protected_load_pc);                                        \
        ASSEMBLE_ATOMIC_BINOP_EXT(Ll_w, Sc_w, 8, inst32, 3);                   \
        break;                                                                 \
      case StoreType::kI64Store16:                                             \
        DCHECK_NULL(protected_load_pc);                                        \
        ASSEMBLE_ATOMIC_BINOP_EXT(Ll_d, Sc_d, 16, inst64, 7);                  \
        break;                                                                 \
      case StoreType::kI32Store16:                                             \
        DCHECK_NULL(protected_load_pc);                                        \
        ASSEMBLE_ATOMIC_BINOP_EXT(Ll_w, Sc_w, 16, inst32, 3);                  \
        break;                                                                 \
      case StoreType::kI64Store32:                                             \
        ASSEMBLE_ATOMIC_BINOP_EXT(Ll_d, Sc_d, 32, inst64, 7);                  \
        break;                                                                 \
      case StoreType::kI32Store:                                               \
        if (protected_load_pc) *protected_load_pc = pc_offset();               \
        am##opcode##_db_w(result.gp(), value.gp(), temp0);                     \
        break;                                                                 \
      case StoreType::kI64Store:                                               \
        if (protected_load_pc) *protected_load_pc = pc_offset();               \
        am##opcode##_db_d(result.gp(), value.gp(), temp0);                     \
        break;                                                                 \
      default:                                                                 \
        UNREACHABLE();                                                         \
    }                                                                          \
    DCHECK_IMPLIES(protected_load_pc != nullptr,                               \
                   Instruction::At(reinterpret_cast<uint8_t*>(                 \
                                       *protected_load_pc + buffer_start_))    \
                       ->IsLoad());                                            \
  }

ATOMIC_BINOP_CASE(Add, Add_w, Add_d, add)
ATOMIC_BINOP_CASE(And, And, And, and)
ATOMIC_BINOP_CASE(Or, Or, Or, or)
ATOMIC_BINOP_CASE(Xor, Xor, Xor, xor)

#define ASSEMBLE_ATOMIC_BINOP(load_linked, store_conditional, bin_instr) \
  do {                                                                   \
    Label binop;                                                         \
    dbar(0);                                                             \
    bind(&binop);                                                        \
    load_linked(result.gp(), MemOperand(temp0, 0),                       \
                reinterpret_cast<int*>(protected_load_pc));              \
    bin_instr(temp1, result.gp(), Operand(value.gp()));                  \
    store_conditional(temp1, MemOperand(temp0, 0));                      \
    BranchShort(&binop, eq, temp1, Operand(zero_reg));                   \
    dbar(0);                                                             \
  } while (0)

void LiftoffAssembler::AtomicSub(Register dst_addr, Register offset_reg,
                                 uintptr_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type,
                                 uint32_t* protected_load_pc, bool i64_offset,
                                 Endianness /* endianness */) {
  LiftoffRegList pinned{dst_addr, value, result};
  if (offset_reg != no_reg) pinned.set(offset_reg);
  Register temp0 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  Register temp1 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  Register temp2 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  Register temp3 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  MemOperand dst_op =
      liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm, i64_offset);
  Add_d(temp0, dst_op.base(), dst_op.offset());
  switch (type.value()) {
    case StoreType::kI64Store8:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_BINOP_EXT(Ll_d, Sc_d, 8, Sub_d, 7);
      break;
    case StoreType::kI32Store8:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_BINOP_EXT(Ll_w, Sc_w, 8, Sub_w, 3);
      break;
    case StoreType::kI64Store16:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_BINOP_EXT(Ll_d, Sc_d, 16, Sub_d, 7);
      break;
    case StoreType::kI32Store16:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_BINOP_EXT(Ll_w, Sc_w, 16, Sub_w, 3);
      break;
    case StoreType::kI64Store32:
      ASSEMBLE_ATOMIC_BINOP_EXT(Ll_d, Sc_d, 32, Sub_d, 7);
      break;
    case StoreType::kI32Store:
      ASSEMBLE_ATOMIC_BINOP(Ll_w, Sc_w, Sub_w);
      break;
    case StoreType::kI64Store:
      ASSEMBLE_ATOMIC_BINOP(Ll_d, Sc_d, Sub_d);
      break;
    default:
      UNREACHABLE();
  }
  DCHECK_IMPLIES(protected_load_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(*protected_load_pc +
                                                            buffer_start_))
                     ->IsLoad());
}
#undef ASSEMBLE_ATOMIC_BINOP
#undef ASSEMBLE_ATOMIC_BINOP_EXT
#undef ATOMIC_BINOP_CASE

#define ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(load_linked, store_conditional, \
                                             size, aligned)                  \
  do {                                                                       \
    Label exchange;                                                          \
    andi(temp1, temp0, aligned);                                             \
    Sub_d(temp0, temp0, Operand(temp1));                                     \
    slli_w(temp1, temp1, 3);                                                 \
    dbar(0);                                                                 \
    bind(&exchange);                                                         \
    load_linked(temp2, MemOperand(temp0, 0),                                 \
                reinterpret_cast<int*>(protected_load_pc));                  \
    ExtractBits(result.gp(), temp2, temp1, size, false);                     \
    InsertBits(temp2, value.gp(), temp1, size);                              \
    store_conditional(temp2, MemOperand(temp0, 0));                          \
    BranchShort(&exchange, eq, temp2, Operand(zero_reg));                    \
    dbar(0);                                                                 \
  } while (0)

void LiftoffAssembler::AtomicExchange(
    Register dst_addr, Register offset_reg, uintptr_t offset_imm,
    LiftoffRegister value, LiftoffRegister result, StoreType type,
    uint32_t* protected_load_pc, bool i64_offset, Endianness /* endianness */) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  LiftoffRegList pinned{dst_addr, value, result};
  if (offset_reg != no_reg) pinned.set(offset_reg);
  Register temp0 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  Register temp1 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  Register temp2 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  MemOperand dst_op =
      liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm, i64_offset);
  Add_d(temp0, dst_op.base(), dst_op.offset());
  switch (type.value()) {
    case StoreType::kI64Store8:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Ll_d, Sc_d, 8, 7);
      break;
    case StoreType::kI32Store8:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Ll_w, Sc_w, 8, 3);
      break;
    case StoreType::kI64Store16:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Ll_d, Sc_d, 16, 7);
      break;
    case StoreType::kI32Store16:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Ll_w, Sc_w, 16, 3);
      break;
    case StoreType::kI64Store32:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Ll_d, Sc_d, 32, 7);
      break;
    case StoreType::kI32Store:
      if (protected_load_pc) *protected_load_pc = pc_offset();
      amswap_db_w(result.gp(), value.gp(), temp0);
      break;
    case StoreType::kI64Store:
      if (protected_load_pc) *protected_load_pc = pc_offset();
      amswap_db_d(result.gp(), value.gp(), temp0);
      break;
    default:
      UNREACHABLE();
  }
  DCHECK_IMPLIES(protected_load_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(*protected_load_pc +
                                                            buffer_start_))
                     ->IsLoad());
}
#undef ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT

void LiftoffAssembler::AtomicExchangeTaggedPointer(
    Register dst_addr, Register offset_reg, uintptr_t offset_imm,
    LiftoffRegister value, LiftoffRegister result, uint32_t* protected_load_pc,
    LiftoffRegList pinned) {
  // Perform the atomic exchange.
  {
    UseScratchRegisterScope temps(this);
    Register actual_addr = liftoff::CalculateActualAddress(
        this, temps, dst_addr, offset_reg, offset_imm);
    if (protected_load_pc) *protected_load_pc = pc_offset();
    if constexpr (COMPRESS_POINTERS_BOOL) {
      amswap_db_w(result.gp(), value.gp(), actual_addr);
      Bstrpick_d(result.gp(), result.gp(), 31, 0);
      add_d(result.gp(), result.gp(), kPtrComprCageBaseRegister);
    } else {
      amswap_db_d(result.gp(), value.gp(), actual_addr);
    }
  }
  DCHECK_IMPLIES(protected_load_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(*protected_load_pc +
                                                            buffer_start_))
                     ->IsLoad());

  if (v8_flags.disable_write_barriers) return;
  // Emit the write barrier.
  Label exit;
  JumpIfSmi(value.gp(), &exit);
  CheckPageFlag(dst_addr, MemoryChunk::kPointersFromHereAreInterestingMask,
                kZero, &exit);
  CheckPageFlag(value.gp(), MemoryChunk::kPointersToHereAreInterestingMask,
                kZero, &exit);

  Operand offset_op = Operand(offset_imm);
  UseScratchRegisterScope temps(this);
  if (offset_reg.is_valid()) {
    Register scratch = temps.Acquire();
    bstrpick_d(scratch, offset_reg, 31, 0);
    if (offset_imm) {
      Add_d(scratch, scratch, Operand(offset_imm));
    }
    offset_op = Operand(scratch);
  }
  CallRecordWriteStubSaveRegisters(dst_addr, offset_op, SaveFPRegsMode::kSave,
                                   StubCallMode::kCallWasmRuntimeStub);
  bind(&exit);
}

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(load_linked,       \
                                                 store_conditional) \
  do {                                                              \
    Label compareExchange;                                          \
    Label exit;                                                     \
    dbar(0);                                                        \
    bind(&compareExchange);                                         \
    load_linked(result.gp(), MemOperand(temp0, 0),                  \
                reinterpret_cast<int*>(protected_load_pc));         \
    BranchShort(&exit, ne, expected_reg, Operand(result.gp()));     \
    mov(temp2, new_value.gp());                                     \
    store_conditional(temp2, MemOperand(temp0, 0));                 \
    BranchShort(&compareExchange, eq, temp2, Operand(zero_reg));    \
    bind(&exit);                                                    \
    dbar(0);                                                        \
  } while (0)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(            \
    load_linked, store_conditional, size, aligned)               \
  do {                                                           \
    Label compareExchange;                                       \
    Label exit;                                                  \
    andi(temp1, temp0, aligned);                                 \
    Sub_d(temp0, temp0, Operand(temp1));                         \
    slli_w(temp1, temp1, 3);                                     \
    dbar(0);                                                     \
    bind(&compareExchange);                                      \
    load_linked(temp2, MemOperand(temp0, 0),                     \
                reinterpret_cast<int*>(protected_load_pc));      \
    ExtractBits(result.gp(), temp2, temp1, size, false);         \
    ExtractBits(temp2, expected_reg, zero_reg, size, false);     \
    BranchShort(&exit, ne, temp2, Operand(result.gp()));         \
    InsertBits(temp2, new_value.gp(), temp1, size);              \
    store_conditional(temp2, MemOperand(temp0, 0));              \
    BranchShort(&compareExchange, eq, temp2, Operand(zero_reg)); \
    bind(&exit);                                                 \
    dbar(0);                                                     \
  } while (0)

void LiftoffAssembler::AtomicCompareExchange(
    Register dst_addr, Register offset_reg, uintptr_t offset_imm,
    LiftoffRegister expected, LiftoffRegister new_value, LiftoffRegister result,
    StoreType type, uint32_t* protected_load_pc, bool i64_offset,
    Endianness /* endianness */) {
  LiftoffRegList pinned{dst_addr, expected, new_value, result};
  if (offset_reg != no_reg) pinned.set(offset_reg);
  Register expected_reg = expected.gp();
  Register temp0 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  Register temp1 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  Register temp2 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  MemOperand dst_op =
      liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm, i64_offset);
  Add_d(temp0, dst_op.base(), dst_op.offset());
  switch (type.value()) {
    case StoreType::kI64Store8:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll_d, Sc_d, 8, 7);
      break;
    case StoreType::kI32Store8:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll_w, Sc_w, 8, 3);
      break;
    case StoreType::kI64Store16:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll_d, Sc_d, 16, 7);
      break;
    case StoreType::kI32Store16:
      DCHECK_NULL(protected_load_pc);
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll_w, Sc_w, 16, 3);
      break;
    case StoreType::kI64Store32:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll_d, Sc_d, 32, 7);
      break;
    case StoreType::kI32Store: {
      slli_w(temp1, expected.gp(), 0);
      expected_reg = temp1;
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(Ll_w, Sc_w);
      break;
    }
    case StoreType::kI64Store:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(Ll_d, Sc_d);
      break;
    default:
      UNREACHABLE();
  }
  DCHECK_IMPLIES(protected_load_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(*protected_load_pc +
                                                            buffer_start_))
                     ->IsLoad());
}
#undef ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER
#undef ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT

void LiftoffAssembler::AtomicCompareExchangeTaggedPointer(
    Register dst_addr, Register offset_reg, uintptr_t offset_imm,
    LiftoffRegister expected, LiftoffRegister new_value, LiftoffRegister result,
    uint32_t* protected_load_pc, LiftoffRegList pinned) {
  AtomicCompareExchange(
      dst_addr, offset_reg, offset_imm, expected, new_value, result,
      COMPRESS_POINTERS_BOOL ? StoreType::kI32Store : StoreType::kI64Store,
      protected_load_pc, false);

  if constexpr (COMPRESS_POINTERS_BOOL) {
    add_d(result.gp(), result.gp(), kPtrComprCageBaseRegister);
  }

  if (v8_flags.disable_write_barriers) return;
  // Emit the write barrier.
  Label exit;
  JumpIfSmi(new_value.gp(), &exit);
  CheckPageFlag(dst_addr, MemoryChunk::kPointersFromHereAreInterestingMask,
                kZero, &exit);
  CheckPageFlag(new_value.gp(), MemoryChunk::kPointersToHereAreInterestingMask,
                kZero, &exit);

  UseScratchRegisterScope temps(this);
  Operand offset_op = Operand(offset_imm);
  if (offset_reg.is_valid()) {
    Register scratch = temps.Acquire();
    bstrpick_d(scratch, offset_reg, 31, 0);
    if (offset_imm) {
      Add_d(scratch, scratch, offset_op);
    }
    offset_op = Operand(scratch);
  }
  CallRecordWriteStubSaveRegisters(dst_addr, offset_op, SaveFPRegsMode::kSave,
                                   StubCallMode::kCallWasmRuntimeStub);
  bind(&exit);
}

void LiftoffAssembler::AtomicFence() { dbar(0); }

void LiftoffAssembler::Pause() { ibar(0); }

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueKind kind) {
  MemOperand src(fp, kSystemPointerSize * (caller_slot_idx + 1));
  liftoff::Load(this, dst, src, kind);
}

void LiftoffAssembler::StoreCallerFrameSlot(LiftoffRegister src,
                                            uint32_t caller_slot_idx,
                                            ValueKind kind,
                                            Register frame_pointer) {
  int32_t offset = kSystemPointerSize * (caller_slot_idx + 1);
  liftoff::Store(this, frame_pointer, offset, src, kind);
}

void LiftoffAssembler::LoadReturnStackSlot(LiftoffRegister dst, int offset,
                                           ValueKind kind) {
  liftoff::Load(this, dst, MemOperand(sp, offset), kind);
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_offset, uint32_t src_offset,
                                      ValueKind kind) {
  DCHECK_NE(dst_offset, src_offset);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();

  switch (kind) {
    case kI32:
    case kF32:
      Ld_w(scratch, liftoff::GetStackSlot(src_offset));
      St_w(scratch, liftoff::GetStackSlot(dst_offset));
      break;
    case kI64:
    case kRefNull:
    case kRef:
    case kF64:
      Ld_d(scratch, liftoff::GetStackSlot(src_offset));
      St_d(scratch, liftoff::GetStackSlot(dst_offset));
      break;
    case kS128:
      Vld(kSimd128ScratchReg, liftoff::GetStackSlot(src_offset));
      Vst(kSimd128ScratchReg, liftoff::GetStackSlot(src_offset));
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Move(Register dst, Register src, ValueKind kind) {
  DCHECK_NE(dst, src);
  // TODO(ksreten): Handle different sizes here.
  MacroAssembler::Move(dst, src);
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueKind kind) {
  DCHECK_NE(dst, src);
  if (kind != kS128) {
    MacroAssembler::Move(dst, src);
  } else {
    MacroAssembler::Vmove(dst.toV(), src.toV());
  }
}

void LiftoffAssembler::Spill(int offset, LiftoffRegister reg, ValueKind kind) {
  RecordUsedSpillOffset(offset);
  MemOperand dst = liftoff::GetStackSlot(offset);
  switch (kind) {
    case kI32:
      St_w(reg.gp(), dst);
      break;
    case kI64:
    case kRef:
    case kRefNull:
      St_d(reg.gp(), dst);
      break;
    case kF32:
      Fst_s(reg.fp(), dst);
      break;
    case kF64:
      MacroAssembler::Fst_d(reg.fp(), dst);
      break;
    case kS128:
      MacroAssembler::Vst(reg.fp().toV(), dst);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(int offset, WasmValue value) {
  RecordUsedSpillOffset(offset);
  MemOperand dst = liftoff::GetStackSlot(offset);
  switch (value.type().kind()) {
    case kI32: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      MacroAssembler::li(scratch, Operand(value.to_i32()));
      St_w(scratch, dst);
      break;
    }
    case kI64:
    case kRef:
    case kRefNull: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      MacroAssembler::li(scratch, value.to_i64());
      St_d(scratch, dst);
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
      Ld_w(reg.gp(), src);
      break;
    case kI64:
    case kRef:
    case kRefNull:
      Ld_d(reg.gp(), src);
      break;
    case kF32:
      Fld_s(reg.fp(), src);
      break;
    case kF64:
      MacroAssembler::Fld_d(reg.fp(), src);
      break;
    case kS128:
      MacroAssembler::Vld(reg.fp().toV(), src);
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

  if (size <= 12 * kStackSlotSize) {
    // Special straight-line code for up to 12 slots. Generates one
    // instruction per slot (<= 12 instructions total).
    uint32_t remainder = size;
    for (; remainder >= kStackSlotSize; remainder -= kStackSlotSize) {
      St_d(zero_reg, liftoff::GetStackSlot(start + remainder));
    }
    DCHECK(remainder == 4 || remainder == 0);
    if (remainder) {
      St_w(zero_reg, liftoff::GetStackSlot(start + remainder));
    }
  } else {
    // General case for bigger counts (12 instructions).
    // Use a0 for start address (inclusive), a1 for end address (exclusive).
    Push(a1, a0);
    Add_d(a0, fp, Operand(-start - size));
    Add_d(a1, fp, Operand(-start));

    Label loop;
    bind(&loop);
    St_d(zero_reg, MemOperand(a0, 0));
    addi_d(a0, a0, kSystemPointerSize);
    BranchShort(&loop, ne, a0, Operand(a1));

    Pop(a1, a0);
  }
}

void LiftoffAssembler::LoadSpillAddress(Register dst, int offset,
                                        ValueKind /* kind */) {
  Sub_d(dst, fp, Operand(offset));
}

void LiftoffAssembler::emit_i64_clz(LiftoffRegister dst, LiftoffRegister src) {
  MacroAssembler::Clz_d(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_i64_ctz(LiftoffRegister dst, LiftoffRegister src) {
  MacroAssembler::Ctz_d(dst.gp(), src.gp());
}

bool LiftoffAssembler::emit_i64_popcnt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  MacroAssembler::Popcnt_d(dst.gp(), src.gp());
  return true;
}

void LiftoffAssembler::IncrementSmi(LiftoffRegister dst, int offset) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  if (COMPRESS_POINTERS_BOOL) {
    DCHECK(SmiValuesAre31Bits());
    Ld_w(scratch, MemOperand(dst.gp(), offset));
    Add_w(scratch, scratch, Operand(Smi::FromInt(1)));
    St_w(scratch, MemOperand(dst.gp(), offset));
  } else {
    SmiUntag(scratch, MemOperand(dst.gp(), offset));
    Add_d(scratch, scratch, Operand(1));
    SmiTag(scratch);
    St_d(scratch, MemOperand(dst.gp(), offset));
  }
}

void LiftoffAssembler::emit_i32_mul(Register dst, Register lhs, Register rhs) {
  MacroAssembler::Mul_w(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  MacroAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));

  // Check if lhs == kMinInt and rhs == -1, since this case is unrepresentable.
  rotri_w(kScratchReg, lhs, 31);
  xori(kScratchReg, kScratchReg, 1);
  // If lhs == kMinInt, move rhs to kScratchReg.
  masknez(kScratchReg, rhs, kScratchReg);
  addi_w(kScratchReg, kScratchReg, 1);
  MacroAssembler::Branch(trap_div_unrepresentable, eq, kScratchReg,
                         Operand(zero_reg));

  MacroAssembler::Div_w(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  MacroAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  MacroAssembler::Div_wu(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  MacroAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  MacroAssembler::Mod_w(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  MacroAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  MacroAssembler::Mod_wu(dst, lhs, rhs);
}

#define I32_BINOP(name, instruction)                                 \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register lhs, \
                                         Register rhs) {             \
    instruction(dst, lhs, rhs);                                      \
  }

// clang-format off
I32_BINOP(add, add_w)
I32_BINOP(sub, sub_w)
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
I32_BINOP_I(add, Add_w)
I32_BINOP_I(sub, Sub_w)
I32_BINOP_I(and, And)
I32_BINOP_I(or, Or)
I32_BINOP_I(xor, Xor)
// clang-format on

#undef I32_BINOP_I

void LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  MacroAssembler::Clz_w(dst, src);
}

void LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  MacroAssembler::Ctz_w(dst, src);
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  MacroAssembler::Popcnt_w(dst, src);
  return true;
}

#define I32_SHIFTOP(name, instruction)                               \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register src, \
                                         Register amount) {          \
    instruction(dst, src, amount);                                   \
  }
#define I32_SHIFTOP_I(name, instruction, instruction1)                  \
  I32_SHIFTOP(name, instruction)                                        \
  void LiftoffAssembler::emit_i32_##name##i(Register dst, Register src, \
                                            int amount) {               \
    instruction1(dst, src, amount & 0x1f);                              \
  }

I32_SHIFTOP_I(shl, sll_w, slli_w)
I32_SHIFTOP_I(sar, sra_w, srai_w)
I32_SHIFTOP_I(shr, srl_w, srli_w)

#undef I32_SHIFTOP
#undef I32_SHIFTOP_I

void LiftoffAssembler::emit_i64_addi(LiftoffRegister dst, LiftoffRegister lhs,
                                     int64_t imm) {
  MacroAssembler::Add_d(dst.gp(), lhs.gp(), Operand(imm));
}

void LiftoffAssembler::emit_i64_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  MacroAssembler::Mul_d(dst.gp(), lhs.gp(), rhs.gp());
}

void LiftoffAssembler::emit_i64_muli(LiftoffRegister dst, LiftoffRegister lhs,
                                     int32_t imm) {
  if (base::bits::IsPowerOfTwo(imm)) {
    emit_i64_shli(dst, lhs, base::bits::WhichPowerOfTwo(imm));
    return;
  }
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MacroAssembler::li(scratch, Operand(imm));
  MacroAssembler::Mul_d(dst.gp(), lhs.gp(), scratch);
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  MacroAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));

  // Check if lhs == MinInt64 and rhs == -1, since this case is unrepresentable.
  rotri_d(kScratchReg, lhs.gp(), 63);
  xori(kScratchReg, kScratchReg, 1);
  // If lhs == MinInt64, move rhs to kScratchReg.
  masknez(kScratchReg, rhs.gp(), kScratchReg);
  addi_d(kScratchReg, kScratchReg, 1);
  MacroAssembler::Branch(trap_div_unrepresentable, eq, kScratchReg,
                         Operand(zero_reg));

  MacroAssembler::Div_d(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  MacroAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));
  MacroAssembler::Div_du(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  MacroAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));
  MacroAssembler::Mod_d(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  MacroAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));
  MacroAssembler::Mod_du(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

#define I64_BINOP(name, instruction)                                   \
  void LiftoffAssembler::emit_i64_##name(                              \
      LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
    instruction(dst.gp(), lhs.gp(), rhs.gp());                         \
  }

// clang-format off
I64_BINOP(add, Add_d)
I64_BINOP(sub, Sub_d)
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
#define I64_SHIFTOP_I(name, instruction, instructioni)                         \
  I64_SHIFTOP(name, instruction)                                               \
  void LiftoffAssembler::emit_i64_##name##i(LiftoffRegister dst,               \
                                            LiftoffRegister src, int amount) { \
    instructioni(dst.gp(), src.gp(), amount & 63);                             \
  }

I64_SHIFTOP_I(shl, sll_d, slli_d)
I64_SHIFTOP_I(sar, sra_d, srai_d)
I64_SHIFTOP_I(shr, srl_d, srli_d)

#undef I64_SHIFTOP
#undef I64_SHIFTOP_I

void LiftoffAssembler::emit_u32_to_uintptr(Register dst, Register src) {
  bstrpick_d(dst, src, 31, 0);
}

void LiftoffAssembler::clear_i32_upper_half(Register dst) {
  // Don't need to clear the upper halves of i32 values for sandbox on
  // LoongArch64, because we'll explicitly zero-extend their lower halves before
  // using them for memory accesses anyway.
}

void LiftoffAssembler::emit_f32_neg(DoubleRegister dst, DoubleRegister src) {
  MacroAssembler::Neg_s(dst, src);
}

void LiftoffAssembler::emit_f64_neg(DoubleRegister dst, DoubleRegister src) {
  MacroAssembler::Neg_d(dst, src);
}

void LiftoffAssembler::emit_f32_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  MacroAssembler::Float32Min(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  MacroAssembler::Float32MinOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f32_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  MacroAssembler::Float32Max(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  MacroAssembler::Float32MaxOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  fcopysign_s(dst, lhs, rhs);
}

void LiftoffAssembler::emit_f64_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  MacroAssembler::Float64Min(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  MacroAssembler::Float64MinOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f64_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  MacroAssembler::Float64Max(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  MacroAssembler::Float64MaxOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  fcopysign_d(dst, lhs, rhs);
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

FP_BINOP(f32_add, fadd_s)
FP_BINOP(f32_sub, fsub_s)
FP_BINOP(f32_mul, fmul_s)
FP_BINOP(f32_div, fdiv_s)
FP_UNOP(f32_abs, fabs_s)
FP_UNOP_RETURN_TRUE(f32_ceil, Ceil_s)
FP_UNOP_RETURN_TRUE(f32_floor, Floor_s)
FP_UNOP_RETURN_TRUE(f32_trunc, Trunc_s)
FP_UNOP_RETURN_TRUE(f32_nearest_int, Round_s)
FP_UNOP(f32_sqrt, fsqrt_s)
FP_BINOP(f64_add, fadd_d)
FP_BINOP(f64_sub, fsub_d)
FP_BINOP(f64_mul, fmul_d)
FP_BINOP(f64_div, fdiv_d)
FP_UNOP(f64_abs, fabs_d)
FP_UNOP_RETURN_TRUE(f64_ceil, Ceil_d)
FP_UNOP_RETURN_TRUE(f64_floor, Floor_d)
FP_UNOP_RETURN_TRUE(f64_trunc, Trunc_d)
FP_UNOP_RETURN_TRUE(f64_nearest_int, Round_d)
FP_UNOP(f64_sqrt, fsqrt_d)

#undef FP_BINOP
#undef FP_UNOP
#undef FP_UNOP_RETURN_TRUE

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      MacroAssembler::bstrpick_w(dst.gp(), src.gp(), 31, 0);
      return true;
    case kExprI32SConvertF32: {
      DoubleRegister rounded = kScratchDoubleReg;

      // Real conversion.
      MacroAssembler::Trunc_s(rounded, src.fp());
      ftintrz_w_s(kScratchDoubleReg2, rounded);
      movfr2gr_s(dst.gp(), kScratchDoubleReg2);
      // Avoid INT32_MAX as an overflow indicator and use INT32_MIN instead,
      // because INT32_MIN allows easier out-of-bounds detection.
      MacroAssembler::Add_w(kScratchReg, dst.gp(), 1);
      MacroAssembler::Slt(kScratchReg2, kScratchReg, dst.gp());
      MacroAssembler::Movn(dst.gp(), kScratchReg, kScratchReg2);

      // Checking if trap.
      movgr2fr_w(kScratchDoubleReg2, dst.gp());
      ffint_s_w(kScratchDoubleReg2, kScratchDoubleReg2);
      MacroAssembler::CompareF32(rounded, kScratchDoubleReg2, CEQ);
      MacroAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI32UConvertF32: {
      DoubleRegister rounded = kScratchDoubleReg;

      // Real conversion.
      MacroAssembler::Trunc_s(rounded, src.fp());
      MacroAssembler::Ftintrz_uw_s(dst.gp(), rounded, kScratchDoubleReg2);
      // Avoid UINT32_MAX as an overflow indicator and use 0 instead,
      // because 0 allows easier out-of-bounds detection.
      MacroAssembler::Add_w(kScratchReg, dst.gp(), 1);
      MacroAssembler::Movz(dst.gp(), zero_reg, kScratchReg);

      // Checking if trap.
      MacroAssembler::Ffint_d_uw(kScratchDoubleReg2, dst.gp());
      fcvt_s_d(kScratchDoubleReg2, kScratchDoubleReg2);
      MacroAssembler::CompareF32(rounded, kScratchDoubleReg2, CEQ);
      MacroAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI32SConvertF64: {
      DoubleRegister rounded = kScratchDoubleReg;

      // Real conversion.
      MacroAssembler::Trunc_d(rounded, src.fp());
      ftintrz_w_d(kScratchDoubleReg2, rounded);
      movfr2gr_s(dst.gp(), kScratchDoubleReg2);

      // Checking if trap.
      ffint_d_w(kScratchDoubleReg2, kScratchDoubleReg2);
      MacroAssembler::CompareF64(rounded, kScratchDoubleReg2, CEQ);
      MacroAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI32UConvertF64: {
      DoubleRegister rounded = kScratchDoubleReg;

      // Real conversion.
      MacroAssembler::Trunc_d(rounded, src.fp());
      MacroAssembler::Ftintrz_uw_d(dst.gp(), rounded, kScratchDoubleReg2);

      // Checking if trap.
      MacroAssembler::Ffint_d_uw(kScratchDoubleReg2, dst.gp());
      MacroAssembler::CompareF64(rounded, kScratchDoubleReg2, CEQ);
      MacroAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI32ReinterpretF32:
      MacroAssembler::FmoveLow(dst.gp(), src.fp());
      return true;
    case kExprI64SConvertI32:
      slli_w(dst.gp(), src.gp(), 0);
      return true;
    case kExprI64UConvertI32:
      MacroAssembler::bstrpick_d(dst.gp(), src.gp(), 31, 0);
      return true;
    case kExprI64SConvertF32: {
      DoubleRegister rounded = kScratchDoubleReg;

      // Real conversion.
      MacroAssembler::Trunc_s(rounded, src.fp());
      ftintrz_l_s(kScratchDoubleReg2, rounded);
      movfr2gr_d(dst.gp(), kScratchDoubleReg2);
      // Avoid INT64_MAX as an overflow indicator and use INT64_MIN instead,
      // because INT64_MIN allows easier out-of-bounds detection.
      MacroAssembler::Add_d(kScratchReg, dst.gp(), 1);
      MacroAssembler::Slt(kScratchReg2, kScratchReg, dst.gp());
      MacroAssembler::Movn(dst.gp(), kScratchReg, kScratchReg2);

      // Checking if trap.
      movgr2fr_d(kScratchDoubleReg2, dst.gp());
      ffint_s_l(kScratchDoubleReg2, kScratchDoubleReg2);
      MacroAssembler::CompareF32(rounded, kScratchDoubleReg2, CEQ);
      MacroAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI64UConvertF32: {
      // Real conversion.
      MacroAssembler::Ftintrz_ul_s(dst.gp(), src.fp(), kScratchDoubleReg,
                                   kScratchReg);

      // Checking if trap.
      MacroAssembler::Branch(trap, eq, kScratchReg, Operand(zero_reg));
      return true;
    }
    case kExprI64SConvertF64: {
      DoubleRegister rounded = kScratchDoubleReg;

      // Real conversion.
      MacroAssembler::Trunc_d(rounded, src.fp());
      ftintrz_l_d(kScratchDoubleReg2, rounded);
      movfr2gr_d(dst.gp(), kScratchDoubleReg2);
      // Avoid INT64_MAX as an overflow indicator and use INT64_MIN instead,
      // because INT64_MIN allows easier out-of-bounds detection.
      MacroAssembler::Add_d(kScratchReg, dst.gp(), 1);
      MacroAssembler::Slt(kScratchReg2, kScratchReg, dst.gp());
      MacroAssembler::Movn(dst.gp(), kScratchReg, kScratchReg2);

      // Checking if trap.
      movgr2fr_d(kScratchDoubleReg2, dst.gp());
      ffint_d_l(kScratchDoubleReg2, kScratchDoubleReg2);
      MacroAssembler::CompareF64(rounded, kScratchDoubleReg2, CEQ);
      MacroAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI64UConvertF64: {
      // Real conversion.
      MacroAssembler::Ftintrz_ul_d(dst.gp(), src.fp(), kScratchDoubleReg,
                                   kScratchReg);

      // Checking if trap.
      MacroAssembler::Branch(trap, eq, kScratchReg, Operand(zero_reg));
      return true;
    }
    case kExprI64ReinterpretF64:
      movfr2gr_d(dst.gp(), src.fp());
      return true;
    case kExprF32SConvertI32: {
      LiftoffRegister scratch = GetUnusedRegister(kFpReg, LiftoffRegList{dst});
      movgr2fr_w(scratch.fp(), src.gp());
      ffint_s_w(dst.fp(), scratch.fp());
      return true;
    }
    case kExprF32UConvertI32:
      MacroAssembler::Ffint_s_uw(dst.fp(), src.gp());
      return true;
    case kExprF32ConvertF64:
      fcvt_s_d(dst.fp(), src.fp());
      return true;
    case kExprF32ReinterpretI32:
      MacroAssembler::FmoveLow(dst.fp(), src.gp());
      return true;
    case kExprF64SConvertI32: {
      LiftoffRegister scratch = GetUnusedRegister(kFpReg, LiftoffRegList{dst});
      movgr2fr_w(scratch.fp(), src.gp());
      ffint_d_w(dst.fp(), scratch.fp());
      return true;
    }
    case kExprF64UConvertI32:
      MacroAssembler::Ffint_d_uw(dst.fp(), src.gp());
      return true;
    case kExprF64ConvertF32:
      fcvt_d_s(dst.fp(), src.fp());
      return true;
    case kExprF64ReinterpretI64:
      movgr2fr_d(dst.fp(), src.gp());
      return true;
    case kExprI32SConvertSatF32:
      ftintrz_w_s(kScratchDoubleReg, src.fp());
      movfr2gr_s(dst.gp(), kScratchDoubleReg);
      return true;
    case kExprI32UConvertSatF32: {
      Label isnan_or_lessthan_or_equal_zero;
      mov(dst.gp(), zero_reg);
      MacroAssembler::Move(kScratchDoubleReg, static_cast<float>(0.0));
      CompareF32(src.fp(), kScratchDoubleReg, CULE);
      BranchTrueShortF(&isnan_or_lessthan_or_equal_zero);
      Ftintrz_uw_s(dst.gp(), src.fp(), kScratchDoubleReg);
      bind(&isnan_or_lessthan_or_equal_zero);
      return true;
    }
    case kExprI32SConvertSatF64:
      ftintrz_w_d(kScratchDoubleReg, src.fp());
      movfr2gr_s(dst.gp(), kScratchDoubleReg);
      return true;
    case kExprI32UConvertSatF64: {
      Label isnan_or_lessthan_or_equal_zero;
      mov(dst.gp(), zero_reg);
      MacroAssembler::Move(kScratchDoubleReg, static_cast<double>(0.0));
      CompareF64(src.fp(), kScratchDoubleReg, CULE);
      BranchTrueShortF(&isnan_or_lessthan_or_equal_zero);
      Ftintrz_uw_d(dst.gp(), src.fp(), kScratchDoubleReg);
      bind(&isnan_or_lessthan_or_equal_zero);
      return true;
    }
    case kExprI64SConvertSatF32:
      ftintrz_l_s(kScratchDoubleReg, src.fp());
      movfr2gr_d(dst.gp(), kScratchDoubleReg);
      return true;
    case kExprI64UConvertSatF32: {
      Label isnan_or_lessthan_or_equal_zero;
      mov(dst.gp(), zero_reg);
      MacroAssembler::Move(kScratchDoubleReg, static_cast<float>(0.0));
      CompareF32(src.fp(), kScratchDoubleReg, CULE);
      BranchTrueShortF(&isnan_or_lessthan_or_equal_zero);
      Ftintrz_ul_s(dst.gp(), src.fp(), kScratchDoubleReg);
      bind(&isnan_or_lessthan_or_equal_zero);
      return true;
    }
    case kExprI64SConvertSatF64:
      ftintrz_l_d(kScratchDoubleReg, src.fp());
      movfr2gr_d(dst.gp(), kScratchDoubleReg);
      return true;
    case kExprI64UConvertSatF64: {
      Label isnan_or_lessthan_or_equal_zero;
      mov(dst.gp(), zero_reg);
      MacroAssembler::Move(kScratchDoubleReg, static_cast<double>(0.0));
      CompareF64(src.fp(), kScratchDoubleReg, CULE);
      BranchTrueShortF(&isnan_or_lessthan_or_equal_zero);
      Ftintrz_ul_d(dst.gp(), src.fp(), kScratchDoubleReg);
      bind(&isnan_or_lessthan_or_equal_zero);
      return true;
    }
    default:
      return false;
  }
}

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  ext_w_b(dst, src);
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  ext_w_h(dst, src);
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  ext_w_b(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  ext_w_h(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  slli_w(dst.gp(), src.gp(), 0);
}

void LiftoffAssembler::emit_jump(Label* label) {
  MacroAssembler::Branch(label);
}

void LiftoffAssembler::emit_jump(Register target) {
  MacroAssembler::Jump(target);
}

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueKind kind, Register lhs,
                                      Register rhs,
                                      const FreezeCacheState& frozen) {
  if (rhs == no_reg) {
    if (kind == kI32) {
      UseScratchRegisterScope temps(this);
      Register scratch0 = temps.Acquire();
      slli_w(scratch0, lhs, 0);
      MacroAssembler::Branch(label, cond, scratch0, Operand(zero_reg));
    } else {
      DCHECK(kind == kI64);
      MacroAssembler::Branch(label, cond, lhs, Operand(zero_reg));
    }
  } else {
    if (kind == kI64) {
      MacroAssembler::Branch(label, cond, lhs, Operand(rhs));
    } else {
      DCHECK((kind == kI32) || (kind == kRef) || (kind == kRefNull));
      MacroAssembler::CompareTaggedAndBranch(label, cond, lhs, Operand(rhs));
    }
  }
}

void LiftoffAssembler::emit_i32_cond_jumpi(Condition cond, Label* label,
                                           Register lhs, int32_t imm,
                                           const FreezeCacheState& frozen) {
  MacroAssembler::CompareTaggedAndBranch(label, cond, lhs, Operand(imm));
}

void LiftoffAssembler::emit_ptrsize_cond_jumpi(Condition cond, Label* label,
                                               Register lhs, int32_t imm,
                                               const FreezeCacheState& frozen) {
  MacroAssembler::Branch(label, cond, lhs, Operand(imm));
}

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  slli_w(dst, src, 0);
  sltui(dst, dst, 1);
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  UseScratchRegisterScope temps(this);
  Register scratch0 = temps.Acquire();
  Register scratch1 = kScratchReg;

  slli_w(scratch0, lhs, 0);
  slli_w(scratch1, rhs, 0);

  CompareWord(cond, dst, scratch0, Operand(scratch1));
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  sltui(dst, src.gp(), 1);
}

void LiftoffAssembler::emit_i64_set_cond(Condition cond, Register dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  CompareWord(cond, dst, lhs.gp(), Operand(rhs.gp()));
}

namespace liftoff {

inline FPUCondition ConditionToConditionCmpFPU(Condition condition,
                                               bool* predicate) {
  switch (condition) {
    case kEqual:
      *predicate = true;
      return CEQ;
    case kNotEqual:
      *predicate = false;
      return CEQ;
    case kUnsignedLessThan:
      *predicate = true;
      return CLT;
    case kUnsignedGreaterThanEqual:
      *predicate = false;
      return CLT;
    case kUnsignedLessThanEqual:
      *predicate = true;
      return CLE;
    case kUnsignedGreaterThan:
      *predicate = false;
      return CLE;
    default:
      *predicate = true;
      break;
  }
  UNREACHABLE();
}

inline void EmitAnyTrue(LiftoffAssembler* assm, LiftoffRegister dst,
                        LiftoffRegister src) {
  Label all_false;
  assm->li(dst.gp(), 0l);
  assm->BranchLSX(&all_false, LSX_BRANCH_V, all_zero, src.fp().toV());
  assm->li(dst.gp(), 1);
  assm->bind(&all_false);
}

inline void EmitAllTrue(LiftoffAssembler* assm, LiftoffRegister dst,
                        LiftoffRegister src, LSXBranchDF lsx_branch_df) {
  Label all_true;
  assm->li(dst.gp(), 1);
  assm->BranchLSX(&all_true, lsx_branch_df, all_not_zero, src.fp().toV());
  assm->li(dst.gp(), 0l);
  assm->bind(&all_true);
}

}  // namespace liftoff

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  Label not_nan, cont;
  MacroAssembler::CompareIsNanF32(lhs, rhs);
  MacroAssembler::BranchFalseF(&not_nan);
  // If one of the operands is NaN, return 1 for f32.ne, else 0.
  if (cond == ne) {
    MacroAssembler::li(dst, 1);
  } else {
    MacroAssembler::Move(dst, zero_reg);
  }
  MacroAssembler::Branch(&cont);

  bind(&not_nan);

  MacroAssembler::li(dst, 1);
  bool predicate;
  FPUCondition fcond = liftoff::ConditionToConditionCmpFPU(cond, &predicate);
  MacroAssembler::CompareF32(lhs, rhs, fcond);
  if (predicate) {
    MacroAssembler::LoadZeroIfNotFPUCondition(dst);
  } else {
    MacroAssembler::LoadZeroIfFPUCondition(dst);
  }

  bind(&cont);
}

void LiftoffAssembler::emit_f64_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  Label not_nan, cont;
  MacroAssembler::CompareIsNanF64(lhs, rhs);
  MacroAssembler::BranchFalseF(&not_nan);
  // If one of the operands is NaN, return 1 for f64.ne, else 0.
  if (cond == ne) {
    MacroAssembler::li(dst, 1);
  } else {
    MacroAssembler::Move(dst, zero_reg);
  }
  MacroAssembler::Branch(&cont);

  bind(&not_nan);

  MacroAssembler::li(dst, 1);
  bool predicate;
  FPUCondition fcond = liftoff::ConditionToConditionCmpFPU(cond, &predicate);
  MacroAssembler::CompareF64(lhs, rhs, fcond);
  if (predicate) {
    MacroAssembler::LoadZeroIfNotFPUCondition(dst);
  } else {
    MacroAssembler::LoadZeroIfFPUCondition(dst);
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
                                      SmiCheckMode mode,
                                      const FreezeCacheState& frozen) {
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
                                     uint32_t* protected_load_pc,
                                     bool i64_offset) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MemOperand src_op =
      liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm, i64_offset);
  VRegister dstReg = dst.fp().toV();
  MachineType memtype = type.mem_type();

  if (transform == LoadTransformationKind::kExtend) {
    Ld_d(scratch, src_op, reinterpret_cast<int*>(protected_load_pc));
    vreplgr2vr_d(kSimd128ScratchReg, scratch);
    if (memtype == MachineType::Int8()) {
      vexth_h_b(dstReg, kSimd128ScratchReg);
    } else if (memtype == MachineType::Uint8()) {
      vexth_hu_bu(dstReg, kSimd128ScratchReg);
    } else if (memtype == MachineType::Int16()) {
      vexth_w_h(dstReg, kSimd128ScratchReg);
    } else if (memtype == MachineType::Uint16()) {
      vexth_wu_hu(dstReg, kSimd128ScratchReg);
    } else if (memtype == MachineType::Int32()) {
      vexth_d_w(dstReg, kSimd128ScratchReg);
    } else if (memtype == MachineType::Uint32()) {
      vexth_du_wu(dstReg, kSimd128ScratchReg);
    }
  } else if (transform == LoadTransformationKind::kZeroExtend) {
    vxor_v(dstReg, dstReg, dstReg);
    if (memtype == MachineType::Int32()) {
      Ld_wu(scratch, src_op, reinterpret_cast<int*>(protected_load_pc));
      vinsgr2vr_w(dstReg, scratch, 0);
    } else {
      DCHECK_EQ(MachineType::Int64(), memtype);
      Ld_d(scratch, src_op, reinterpret_cast<int*>(protected_load_pc));
      vinsgr2vr_d(dstReg, scratch, 0);
    }
  } else {
    DCHECK_EQ(LoadTransformationKind::kSplat, transform);
    if (memtype == MachineType::Int8()) {
      Ld_b(scratch, src_op, reinterpret_cast<int*>(protected_load_pc));
      vreplgr2vr_b(dstReg, scratch);
    } else if (memtype == MachineType::Int16()) {
      Ld_h(scratch, src_op, reinterpret_cast<int*>(protected_load_pc));
      vreplgr2vr_h(dstReg, scratch);
    } else if (memtype == MachineType::Int32()) {
      Ld_w(scratch, src_op, reinterpret_cast<int*>(protected_load_pc));
      vreplgr2vr_w(dstReg, scratch);
    } else if (memtype == MachineType::Int64()) {
      Ld_d(scratch, src_op, reinterpret_cast<int*>(protected_load_pc));
      vreplgr2vr_d(dstReg, scratch);
    }
  }
}

void LiftoffAssembler::LoadLane(LiftoffRegister dst, LiftoffRegister src,
                                Register addr, Register offset_reg,
                                uintptr_t offset_imm, LoadType type,
                                uint8_t laneidx, uint32_t* protected_load_pc,
                                bool i64_offset) {
  MemOperand src_op =
      liftoff::GetMemOp(this, addr, offset_reg, offset_imm, i64_offset);
  if (dst != src) {
    Vmove(dst.fp().toV(), src.fp().toV());
  }

  LoadStoreLaneParams load_params(type.mem_type().representation(), laneidx);
  MacroAssembler::LoadLane(load_params.sz, dst.fp().toV(), laneidx, src_op,
                           reinterpret_cast<int*>(protected_load_pc));
  DCHECK_IMPLIES(protected_load_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(*protected_load_pc +
                                                            buffer_start_))
                     ->IsLoad());
}

void LiftoffAssembler::StoreLane(Register dst, Register offset,
                                 uintptr_t offset_imm, LiftoffRegister src,
                                 StoreType type, uint8_t lane,
                                 uint32_t* protected_store_pc,
                                 bool i64_offset) {
  MemOperand dst_op =
      liftoff::GetMemOp(this, dst, offset, offset_imm, i64_offset);
  LoadStoreLaneParams store_params(type.mem_rep(), lane);
  MacroAssembler::StoreLane(store_params.sz, src.fp().toV(), lane, dst_op,
                            reinterpret_cast<int*>(protected_store_pc));
  DCHECK_IMPLIES(protected_store_pc != nullptr,
                 Instruction::At(reinterpret_cast<uint8_t*>(
                                     *protected_store_pc + buffer_start_))
                     ->IsStore());
}

void LiftoffAssembler::emit_i8x16_shuffle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs,
                                          const uint8_t shuffle[16],
                                          bool is_swizzle) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  VRegister idx1Reg = kSimd128ScratchReg;

  uint64_t control_hi = 0;
  uint64_t control_low = 0;
  for (int i = 7; i >= 0; i--) {
    control_hi <<= 8;
    control_hi |= shuffle[i + 8];
    control_low <<= 8;
    control_low |= shuffle[i];
  }

  DCHECK_EQ(0, (control_hi | control_low) & 0xE0E0E0E0E0E0E0E0);
  li(kScratchReg, control_low);
  vinsgr2vr_d(idx1Reg, kScratchReg, 0);
  li(kScratchReg, control_hi);
  vinsgr2vr_d(idx1Reg, kScratchReg, 1);
  vshuf_b(dstReg, rhsReg, lhsReg, idx1Reg);
}

void LiftoffAssembler::emit_i8x16_swizzle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  VRegister idx1Reg = kSimd128ScratchReg;
  VRegister idx2Reg = kSimd128ScratchReg1;

  // The value of kSimd128RegZero is invalid, it will be masked in vand_v.
  vshuf_b(idx1Reg, kSimd128RegZero, lhsReg, rhsReg);
  vslei_bu(idx2Reg, rhsReg, 0xF);
  vand_v(dstReg, idx1Reg, idx2Reg);
}

void LiftoffAssembler::emit_i8x16_relaxed_swizzle(LiftoffRegister dst,
                                                  LiftoffRegister lhs,
                                                  LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  VRegister idx1Reg = kSimd128ScratchReg;
  VRegister idx2Reg = kSimd128ScratchReg1;

  // The value of kSimd128RegZero is invalid, it will be masked in vand_v.
  vshuf_b(idx1Reg, kSimd128RegZero, lhsReg, rhsReg);
  vslei_bu(idx2Reg, rhsReg, 0xF);
  vand_v(dstReg, idx1Reg, idx2Reg);
}

void LiftoffAssembler::emit_i32x4_relaxed_trunc_f32x4_s(LiftoffRegister dst,
                                                        LiftoffRegister src) {
  vftintrz_w_s(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i32x4_relaxed_trunc_f32x4_u(LiftoffRegister dst,
                                                        LiftoffRegister src) {
  vftintrz_wu_s(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i32x4_relaxed_trunc_f64x2_s_zero(
    LiftoffRegister dst, LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vftintrz_l_d(kSimd128ScratchReg, src.fp().toV());
  vsat_d(kSimd128ScratchReg, kSimd128ScratchReg, 31);
  vpickev_w(dst.fp().toV(), kSimd128RegZero, kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i32x4_relaxed_trunc_f64x2_u_zero(
    LiftoffRegister dst, LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vftintrz_lu_d(kSimd128ScratchReg, src.fp().toV());
  vsat_du(kSimd128ScratchReg, kSimd128ScratchReg, 31);
  vpickev_w(dst.fp().toV(), kSimd128RegZero, kSimd128ScratchReg);
}

void LiftoffAssembler::emit_s128_relaxed_laneselect(LiftoffRegister dst,
                                                    LiftoffRegister src1,
                                                    LiftoffRegister src2,
                                                    LiftoffRegister mask,
                                                    int lane_width) {
  // LoongArch uses bytewise selection for all lane widths.
  vbitsel_v(dst.fp().toV(), src2.fp().toV(), src1.fp().toV(), mask.fp().toV());
}

void LiftoffAssembler::emit_i8x16_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VRegister dstReg = dst.fp().toV();
  Register srcReg = src.gp();
  vreplgr2vr_b(dstReg, srcReg);
}

void LiftoffAssembler::emit_i16x8_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VRegister dstReg = dst.fp().toV();
  Register srcReg = src.gp();
  vreplgr2vr_h(dstReg, srcReg);
}

void LiftoffAssembler::emit_i32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VRegister dstReg = dst.fp().toV();
  Register srcReg = src.gp();
  vreplgr2vr_w(dstReg, srcReg);
}

void LiftoffAssembler::emit_i64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VRegister dstReg = dst.fp().toV();
  Register srcReg = src.gp();
  vreplgr2vr_d(dstReg, srcReg);
}

void LiftoffAssembler::emit_f32x4_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VRegister dstReg = dst.fp().toV();
  MacroAssembler::FmoveLow(kScratchReg, src.fp());
  vreplgr2vr_w(dstReg, kScratchReg);
}

void LiftoffAssembler::emit_f64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  VRegister dstReg = dst.fp().toV();
  MacroAssembler::Move(kScratchReg, src.fp());
  vreplgr2vr_d(dstReg, kScratchReg);
}

#define SIMD_BINOP(name1, name2, type)                                   \
  void LiftoffAssembler::emit_##name1##_extmul_low_##name2(              \
      LiftoffRegister dst, LiftoffRegister src1, LiftoffRegister src2) { \
    MacroAssembler::ExtMulLow(type, dst.fp().toV(), src1.fp().toV(),     \
                              src2.fp().toV());                          \
  }                                                                      \
  void LiftoffAssembler::emit_##name1##_extmul_high_##name2(             \
      LiftoffRegister dst, LiftoffRegister src1, LiftoffRegister src2) { \
    MacroAssembler::ExtMulHigh(type, dst.fp().toV(), src1.fp().toV(),    \
                               src2.fp().toV());                         \
  }

SIMD_BINOP(i16x8, i8x16_s, LSXS8)
SIMD_BINOP(i16x8, i8x16_u, LSXU8)

SIMD_BINOP(i32x4, i16x8_s, LSXS16)
SIMD_BINOP(i32x4, i16x8_u, LSXU16)

SIMD_BINOP(i64x2, i32x4_s, LSXS32)
SIMD_BINOP(i64x2, i32x4_u, LSXU32)

#undef SIMD_BINOP

#define SIMD_BINOP(name1, name2, type)                                    \
  void LiftoffAssembler::emit_##name1##_extadd_pairwise_##name2(          \
      LiftoffRegister dst, LiftoffRegister src) {                         \
    MacroAssembler::ExtAddPairwise(type, dst.fp().toV(), src.fp().toV()); \
  }

SIMD_BINOP(i16x8, i8x16_s, LSXS8)
SIMD_BINOP(i16x8, i8x16_u, LSXU8)
SIMD_BINOP(i32x4, i16x8_s, LSXS16)
SIMD_BINOP(i32x4, i16x8_u, LSXU16)
#undef SIMD_BINOP

void LiftoffAssembler::emit_i16x8_q15mulr_sat_s(LiftoffRegister dst,
                                                LiftoffRegister src1,
                                                LiftoffRegister src2) {
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  VRegister scratch2 = kSimd128ScratchReg1;
  vxor_v(scratch0, scratch0, scratch0);
  vbitseti_w(scratch1, scratch0, 14);
  vbitseti_w(scratch2, scratch0, 14);
  vmaddwod_w_h(scratch1, src1.fp().toV(), src2.fp().toV());
  vmaddwev_w_h(scratch2, src1.fp().toV(), src2.fp().toV());
  vssrani_h_w(scratch1, scratch1, 15);
  vssrani_h_w(scratch2, scratch2, 15);
  vilvl_h(dst.fp().toV(), scratch1, scratch2);
}

void LiftoffAssembler::emit_i16x8_relaxed_q15mulr_s(LiftoffRegister dst,
                                                    LiftoffRegister src1,
                                                    LiftoffRegister src2) {
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  VRegister scratch2 = kSimd128ScratchReg1;
  vxor_v(scratch0, scratch0, scratch0);
  vbitseti_w(scratch1, scratch0, 14);
  vbitseti_w(scratch2, scratch0, 14);
  vmaddwod_w_h(scratch1, src1.fp().toV(), src2.fp().toV());
  vmaddwev_w_h(scratch2, src1.fp().toV(), src2.fp().toV());
  vssrani_h_w(scratch1, scratch1, 15);
  vssrani_h_w(scratch2, scratch2, 15);
  vilvl_h(dst.fp().toV(), scratch1, scratch2);
}

void LiftoffAssembler::emit_i16x8_dot_i8x16_i7x16_s(LiftoffRegister dst,
                                                    LiftoffRegister lhs,
                                                    LiftoffRegister rhs) {
  Dotp_h(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_dot_i8x16_i7x16_add_s(LiftoffRegister dst,
                                                        LiftoffRegister lhs,
                                                        LiftoffRegister rhs,
                                                        LiftoffRegister acc) {
  DCHECK_NE(dst, acc);
  Dotp_h(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
  vhaddw_w_h(dst.fp().toV(), dst.fp().toV(), dst.fp().toV());
  vadd_w(dst.fp().toV(), dst.fp().toV(), acc.fp().toV());
}

void LiftoffAssembler::emit_i8x16_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vseq_b(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vseq_b(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
  vnor_v(dst.fp().toV(), dst.fp().toV(), dst.fp().toV());
}

void LiftoffAssembler::emit_i8x16_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vslt_b(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vslt_bu(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vsle_b(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vsle_bu(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vseq_h(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vseq_h(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
  vnor_v(dst.fp().toV(), dst.fp().toV(), dst.fp().toV());
}

void LiftoffAssembler::emit_i16x8_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vslt_h(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vslt_hu(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vsle_h(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vsle_hu(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vseq_w(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vseq_w(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
  vnor_v(dst.fp().toV(), dst.fp().toV(), dst.fp().toV());
}

void LiftoffAssembler::emit_i32x4_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vslt_w(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_gt_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vslt_wu(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vsle_w(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_ge_u(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vsle_wu(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vfcmp_cond_s(CEQ, dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vfcmp_cond_s(CUNE, dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_lt(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vfcmp_cond_s(CLT, dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_le(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vfcmp_cond_s(CLE, dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i64x2_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vseq_d(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i64x2_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vseq_d(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
  vnor_v(dst.fp().toV(), dst.fp().toV(), dst.fp().toV());
}

void LiftoffAssembler::emit_i64x2_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vadda_d(dst.fp().toV(), src.fp().toV(), kSimd128RegZero);
}

void LiftoffAssembler::emit_f64x2_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vfcmp_cond_d(CEQ, dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vfcmp_cond_d(CUNE, dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_lt(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vfcmp_cond_d(CLT, dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_le(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vfcmp_cond_d(CLE, dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_s128_const(LiftoffRegister dst,
                                       const uint8_t imms[16]) {
  VRegister dstReg = dst.fp().toV();
  uint64_t vals[2];
  memcpy(vals, imms, sizeof(vals));
  li(kScratchReg, vals[0]);
  vinsgr2vr_d(dstReg, kScratchReg, 0);
  li(kScratchReg, vals[1]);
  vinsgr2vr_d(dstReg, kScratchReg, 1);
}

void LiftoffAssembler::emit_s128_not(LiftoffRegister dst, LiftoffRegister src) {
  vnor_v(dst.fp().toV(), src.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_s128_and(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vand_v(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_s128_or(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  vor_v(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_s128_xor(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  vxor_v(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_s128_and_not(LiftoffRegister dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  vnor_v(kSimd128ScratchReg, rhs.fp().toV(), rhs.fp().toV());
  vand_v(dst.fp().toV(), kSimd128ScratchReg, lhs.fp().toV());
}

void LiftoffAssembler::emit_s128_select(LiftoffRegister dst,
                                        LiftoffRegister src1,
                                        LiftoffRegister src2,
                                        LiftoffRegister mask) {
  vbitsel_v(dst.fp().toV(), src2.fp().toV(), src1.fp().toV(), mask.fp().toV());
}

void LiftoffAssembler::emit_i8x16_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vneg_b(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_v128_anytrue(LiftoffRegister dst,
                                         LiftoffRegister src) {
  liftoff::EmitAnyTrue(this, dst, src);
}

void LiftoffAssembler::emit_i8x16_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  liftoff::EmitAllTrue(this, dst, src, LSX_BRANCH_B);
}

void LiftoffAssembler::emit_i8x16_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  vsrli_b(scratch0, src.fp().toV(), 7);
  vsrli_h(scratch1, scratch0, 7);
  vor_v(scratch0, scratch0, scratch1);
  vsrli_w(scratch1, scratch0, 14);
  vor_v(scratch0, scratch0, scratch1);
  vsrli_d(scratch1, scratch0, 28);
  vor_v(scratch0, scratch0, scratch1);
  vshuf4i_w(scratch1, scratch0, 0x0E);
  vpackev_b(scratch0, scratch1, scratch0);
  vpickve2gr_hu(dst.gp(), scratch0, 0);
}

void LiftoffAssembler::emit_i8x16_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vreplgr2vr_b(kSimd128ScratchReg, rhs.gp());
  vsll_b(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i8x16_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  vslli_b(dst.fp().toV(), lhs.fp().toV(), rhs & 7);
}

void LiftoffAssembler::emit_i8x16_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vreplgr2vr_b(kSimd128ScratchReg, rhs.gp());
  vsra_b(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i8x16_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  vsrai_b(dst.fp().toV(), lhs.fp().toV(), rhs & 7);
}

void LiftoffAssembler::emit_i8x16_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vreplgr2vr_b(kSimd128ScratchReg, rhs.gp());
  vsrl_b(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i8x16_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  vsrli_b(dst.fp().toV(), lhs.fp().toV(), rhs & 7);
}

void LiftoffAssembler::emit_i8x16_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vadd_b(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_add_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  vsadd_b(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_add_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  vsadd_bu(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vsub_b(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_sub_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  vssub_b(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_sub_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  vssub_bu(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmin_b(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmin_bu(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmax_b(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmax_bu(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_popcnt(LiftoffRegister dst,
                                         LiftoffRegister src) {
  vpcnt_b(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i16x8_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vneg_h(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i16x8_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  liftoff::EmitAllTrue(this, dst, src, LSX_BRANCH_H);
}

void LiftoffAssembler::emit_i16x8_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  vsrli_h(scratch0, src.fp().toV(), 15);
  vsrli_w(scratch1, scratch0, 15);
  vor_v(scratch0, scratch0, scratch1);
  vsrli_d(scratch1, scratch0, 30);
  vor_v(scratch0, scratch0, scratch1);
  vshuf4i_w(scratch1, scratch0, 0x0E);
  vslli_d(scratch1, scratch1, 4);
  vor_v(scratch0, scratch0, scratch1);
  vpickve2gr_bu(dst.gp(), scratch0, 0);
}

void LiftoffAssembler::emit_i16x8_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vreplgr2vr_h(kSimd128ScratchReg, rhs.gp());
  vsll_h(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i16x8_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  vslli_h(dst.fp().toV(), lhs.fp().toV(), rhs & 15);
}

void LiftoffAssembler::emit_i16x8_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vreplgr2vr_h(kSimd128ScratchReg, rhs.gp());
  vsra_h(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i16x8_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  vsrai_h(dst.fp().toV(), lhs.fp().toV(), rhs & 15);
}

void LiftoffAssembler::emit_i16x8_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vreplgr2vr_h(kSimd128ScratchReg, rhs.gp());
  vsrl_h(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i16x8_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  vsrli_h(dst.fp().toV(), lhs.fp().toV(), rhs & 15);
}

void LiftoffAssembler::emit_i16x8_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vadd_h(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_add_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  vsadd_h(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_add_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  vsadd_hu(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vsub_h(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_sub_sat_s(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  vssub_h(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_sub_sat_u(LiftoffRegister dst,
                                            LiftoffRegister lhs,
                                            LiftoffRegister rhs) {
  vssub_hu(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vmul_h(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmin_h(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmin_hu(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmax_h(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmax_hu(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vneg_w(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i32x4_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  liftoff::EmitAllTrue(this, dst, src, LSX_BRANCH_W);
}

void LiftoffAssembler::emit_i32x4_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  vsrli_w(scratch0, src.fp().toV(), 31);
  vsrli_d(scratch1, scratch0, 31);
  vor_v(scratch0, scratch0, scratch1);
  vshuf4i_w(scratch1, scratch0, 0x0E);
  vslli_d(scratch1, scratch1, 2);
  vor_v(scratch0, scratch0, scratch1);
  vpickve2gr_bu(dst.gp(), scratch0, 0);
}

void LiftoffAssembler::emit_i32x4_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vreplgr2vr_w(kSimd128ScratchReg, rhs.gp());
  vsll_w(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i32x4_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  vslli_w(dst.fp().toV(), lhs.fp().toV(), rhs & 31);
}

void LiftoffAssembler::emit_i32x4_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vreplgr2vr_w(kSimd128ScratchReg, rhs.gp());
  vsra_w(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i32x4_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  vsrai_w(dst.fp().toV(), lhs.fp().toV(), rhs & 31);
}

void LiftoffAssembler::emit_i32x4_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vreplgr2vr_w(kSimd128ScratchReg, rhs.gp());
  vsrl_w(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i32x4_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  vsrli_w(dst.fp().toV(), lhs.fp().toV(), rhs & 31);
}

void LiftoffAssembler::emit_i32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vadd_w(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vsub_w(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vmul_w(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_min_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmin_w(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_min_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmin_wu(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_max_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmax_w(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_max_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vmax_wu(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i32x4_dot_i16x8_s(LiftoffRegister dst,
                                              LiftoffRegister lhs,
                                              LiftoffRegister rhs) {
  Dotp_w(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i64x2_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vneg_d(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i64x2_alltrue(LiftoffRegister dst,
                                          LiftoffRegister src) {
  liftoff::EmitAllTrue(this, dst, src, LSX_BRANCH_D);
}

void LiftoffAssembler::emit_i64x2_bitmask(LiftoffRegister dst,
                                          LiftoffRegister src) {
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  vsrli_d(scratch0, src.fp().toV(), 63);
  vshuf4i_w(scratch1, scratch0, 0x02);
  vslli_d(scratch1, scratch1, 1);
  vor_v(scratch0, scratch0, scratch1);
  vpickve2gr_bu(dst.gp(), scratch0, 0);
}

void LiftoffAssembler::emit_i64x2_shl(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vreplgr2vr_d(kSimd128ScratchReg, rhs.gp());
  vsll_d(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i64x2_shli(LiftoffRegister dst, LiftoffRegister lhs,
                                       int32_t rhs) {
  vslli_d(dst.fp().toV(), lhs.fp().toV(), rhs & 63);
}

void LiftoffAssembler::emit_i64x2_shr_s(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vreplgr2vr_d(kSimd128ScratchReg, rhs.gp());
  vsra_d(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i64x2_shri_s(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  vsrai_d(dst.fp().toV(), lhs.fp().toV(), rhs & 63);
}

void LiftoffAssembler::emit_i64x2_shr_u(LiftoffRegister dst,
                                        LiftoffRegister lhs,
                                        LiftoffRegister rhs) {
  vreplgr2vr_d(kSimd128ScratchReg, rhs.gp());
  vsrl_d(dst.fp().toV(), lhs.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i64x2_shri_u(LiftoffRegister dst,
                                         LiftoffRegister lhs, int32_t rhs) {
  vsrli_d(dst.fp().toV(), lhs.fp().toV(), rhs & 63);
}

void LiftoffAssembler::emit_i64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vadd_d(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vsub_d(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vmul_d(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_i64x2_gt_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vslt_d(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i64x2_ge_s(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  vsle_d(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vbitclri_w(dst.fp().toV(), src.fp().toV(), 31);
}

void LiftoffAssembler::emit_f32x4_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vbitrevi_w(dst.fp().toV(), src.fp().toV(), 31);
}

void LiftoffAssembler::emit_f32x4_sqrt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  vfsqrt_s(dst.fp().toV(), src.fp().toV());
}

bool LiftoffAssembler::emit_f32x4_ceil(LiftoffRegister dst,
                                       LiftoffRegister src) {
  LSXRoundW(dst.fp().toV(), src.fp().toV(), kRoundToPlusInf);
  return true;
}

bool LiftoffAssembler::emit_f32x4_floor(LiftoffRegister dst,
                                        LiftoffRegister src) {
  LSXRoundW(dst.fp().toV(), src.fp().toV(), kRoundToMinusInf);
  return true;
}

bool LiftoffAssembler::emit_f32x4_trunc(LiftoffRegister dst,
                                        LiftoffRegister src) {
  LSXRoundW(dst.fp().toV(), src.fp().toV(), kRoundToZero);
  return true;
}

bool LiftoffAssembler::emit_f32x4_nearest_int(LiftoffRegister dst,
                                              LiftoffRegister src) {
  LSXRoundW(dst.fp().toV(), src.fp().toV(), kRoundToNearest);
  return true;
}

void LiftoffAssembler::emit_f32x4_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vfadd_s(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vfsub_s(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vfmul_s(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_div(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vfdiv_s(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f32x4_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  // If inputs are -0.0. and +0.0, then write -0.0 to scratch1.
  // scratch1 = (lhsReg == rhsReg) ?  (lhsReg | rhsReg) : (rhsReg | rhsReg).
  vfcmp_cond_s(CEQ, scratch0, lhsReg, rhsReg);
  vbitsel_v(scratch0, rhsReg, lhsReg, scratch0);
  vor_v(scratch1, scratch0, rhsReg);
  // scratch0 = isNaN(lhsReg) ? lhsReg : scratch1.
  vfcmp_cond_s(CEQ, scratch0, lhsReg, lhsReg);
  vbitsel_v(scratch0, lhsReg, scratch1, scratch0);
  // scratch1 = (lhsReg < scratch0) ? lhsReg : scratch0.
  vfcmp_cond_s(CLT, scratch1, lhsReg, scratch0);
  vbitsel_v(scratch1, scratch0, lhsReg, scratch1);
  // Canonicalize the result.
  vfmin_s(dstReg, scratch1, scratch1);
}

void LiftoffAssembler::emit_f32x4_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  // If inputs are -0.0. and +0.0, then write +0.0 to scratch1.
  // scratch1 = (lhsReg == rhsReg) ?  (lhsReg & rhsReg) : (rhsReg & rhsReg).
  vfcmp_cond_s(CEQ, scratch0, lhsReg, rhsReg);
  vbitsel_v(scratch0, rhsReg, lhsReg, scratch0);
  vand_v(scratch1, scratch0, rhsReg);
  // scratch0 = isNaN(lhsReg) ? lhsReg : scratch1.
  vfcmp_cond_s(CEQ, scratch0, lhsReg, lhsReg);
  vbitsel_v(scratch0, lhsReg, scratch1, scratch0);
  // scratch1 = (scratch0 < lhsReg) ? lhsReg : scratch0.
  vfcmp_cond_s(CLT, scratch1, scratch0, lhsReg);
  vbitsel_v(scratch1, scratch0, lhsReg, scratch1);
  // Canonicalize the result.
  vfmax_s(dstReg, scratch1, scratch1);
}

void LiftoffAssembler::emit_f32x4_relaxed_min(LiftoffRegister dst,
                                              LiftoffRegister lhs,
                                              LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  // If inputs are -0.0. and +0.0, then write -0.0 to scratch1.
  // scratch1 = (lhsReg == rhsReg) ?  (lhsReg | rhsReg) : (rhsReg | rhsReg).
  vfcmp_cond_s(CEQ, scratch0, lhsReg, rhsReg);
  vbitsel_v(scratch0, rhsReg, lhsReg, scratch0);
  vor_v(scratch1, scratch0, rhsReg);
  // scratch0 = isNaN(lhsReg) ? lhsReg : scratch1.
  vfcmp_cond_s(CEQ, scratch0, lhsReg, lhsReg);
  vbitsel_v(scratch0, lhsReg, scratch1, scratch0);
  // scratch1 = (lhsReg < scratch0) ? lhsReg : scratch0.
  vfcmp_cond_s(CLT, scratch1, lhsReg, scratch0);
  vbitsel_v(scratch1, scratch0, lhsReg, scratch1);
  // Canonicalize the result.
  vfmin_s(dstReg, scratch1, scratch1);
}

void LiftoffAssembler::emit_f32x4_relaxed_max(LiftoffRegister dst,
                                              LiftoffRegister lhs,
                                              LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  // If inputs are -0.0. and +0.0, then write +0.0 to scratch1.
  // scratch1 = (lhsReg == rhsReg) ?  (lhsReg & rhsReg) : (rhsReg & rhsReg).
  vfcmp_cond_s(CEQ, scratch0, lhsReg, rhsReg);
  vbitsel_v(scratch0, rhsReg, lhsReg, scratch0);
  vand_v(scratch1, scratch0, rhsReg);
  // scratch0 = isNaN(lhsReg) ? lhsReg : scratch1.
  vfcmp_cond_s(CEQ, scratch0, lhsReg, lhsReg);
  vbitsel_v(scratch0, lhsReg, scratch1, scratch0);
  // scratch1 = (scratch0 < lhsReg) ? lhsReg : scratch0.
  vfcmp_cond_s(CLT, scratch1, scratch0, lhsReg);
  vbitsel_v(scratch1, scratch0, lhsReg, scratch1);
  // Canonicalize the result.
  vfmax_s(dstReg, scratch1, scratch1);
}

void LiftoffAssembler::emit_f32x4_pmin(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  vfcmp_cond_s(CLT, dstReg, rhsReg, lhsReg);
  vbitsel_v(dstReg, lhsReg, rhsReg, dstReg);
}

void LiftoffAssembler::emit_f32x4_pmax(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  vfcmp_cond_s(CLT, dstReg, lhsReg, rhsReg);
  vbitsel_v(dstReg, lhsReg, rhsReg, dstReg);
}

void LiftoffAssembler::emit_f64x2_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vbitclri_d(dst.fp().toV(), src.fp().toV(), 63);
}

void LiftoffAssembler::emit_f64x2_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vbitrevi_d(dst.fp().toV(), src.fp().toV(), 63);
}

void LiftoffAssembler::emit_f64x2_sqrt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  vfsqrt_d(dst.fp().toV(), src.fp().toV());
}

bool LiftoffAssembler::emit_f64x2_ceil(LiftoffRegister dst,
                                       LiftoffRegister src) {
  LSXRoundD(dst.fp().toV(), src.fp().toV(), kRoundToPlusInf);
  return true;
}

bool LiftoffAssembler::emit_f64x2_floor(LiftoffRegister dst,
                                        LiftoffRegister src) {
  LSXRoundD(dst.fp().toV(), src.fp().toV(), kRoundToMinusInf);
  return true;
}

bool LiftoffAssembler::emit_f64x2_trunc(LiftoffRegister dst,
                                        LiftoffRegister src) {
  LSXRoundD(dst.fp().toV(), src.fp().toV(), kRoundToZero);
  return true;
}

bool LiftoffAssembler::emit_f64x2_nearest_int(LiftoffRegister dst,
                                              LiftoffRegister src) {
  LSXRoundD(dst.fp().toV(), src.fp().toV(), kRoundToNearest);
  return true;
}

void LiftoffAssembler::emit_f64x2_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vfadd_d(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vfsub_d(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vfmul_d(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_div(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  vfdiv_d(dst.fp().toV(), lhs.fp().toV(), rhs.fp().toV());
}

void LiftoffAssembler::emit_f64x2_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  // If inputs are -0.0. and +0.0, then write -0.0 to scratch1.
  // scratch1 = (lhs == rhs) ?  (lhs | rhs) : (rhs | rhs).
  vfcmp_cond_d(CEQ, scratch0, lhsReg, rhsReg);
  vbitsel_v(scratch0, rhsReg, lhsReg, scratch0);
  vor_v(scratch1, scratch0, rhsReg);
  // scratch0 = isNaN(lhs) ? lhs : scratch1.
  vfcmp_cond_d(CEQ, scratch0, lhsReg, lhsReg);
  vbitsel_v(scratch0, lhsReg, scratch1, scratch0);
  // scratch1 = (lhs < scratch0) ? lhs : scratch0.
  vfcmp_cond_d(CLT, scratch1, lhsReg, scratch0);
  vbitsel_v(scratch1, scratch0, lhsReg, scratch1);
  // Canonicalize the result.
  vfmin_d(dstReg, scratch1, scratch1);
}

void LiftoffAssembler::emit_f64x2_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  // If inputs are -0.0. and +0.0, then write +0.0 to scratch1.
  // scratch1 = (lhsReg == rhsReg) ?  (lhsReg & rhsReg) : (rhsReg & rhsReg).
  vfcmp_cond_d(CEQ, scratch0, lhsReg, rhsReg);
  vbitsel_v(scratch0, rhsReg, lhsReg, scratch0);
  vand_v(scratch1, scratch0, rhsReg);
  // scratch0 = isNaN(lhsReg) ? lhsReg : scratch1.
  vfcmp_cond_d(CEQ, scratch0, lhsReg, lhsReg);
  vbitsel_v(scratch0, lhsReg, scratch1, scratch0);
  // scratch1 = (scratch0 < lhsReg) ? lhsReg : scratch0.
  vfcmp_cond_d(CLT, scratch1, scratch0, lhsReg);
  vbitsel_v(scratch1, scratch0, lhsReg, scratch1);
  // Canonicalize the result.
  vfmax_d(dstReg, scratch1, scratch1);
}

void LiftoffAssembler::emit_f64x2_pmin(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  vfcmp_cond_d(CLT, kSimd128ScratchReg, rhsReg, lhsReg);
  vbitsel_v(dstReg, lhsReg, rhsReg, kSimd128ScratchReg);
}

void LiftoffAssembler::emit_f64x2_pmax(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  vfcmp_cond_d(CLT, kSimd128ScratchReg, lhsReg, rhsReg);
  vbitsel_v(dstReg, lhsReg, rhsReg, kSimd128ScratchReg);
}

void LiftoffAssembler::emit_f64x2_relaxed_min(LiftoffRegister dst,
                                              LiftoffRegister lhs,
                                              LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  // If inputs are -0.0. and +0.0, then write -0.0 to scratch1.
  // scratch1 = (lhs == rhs) ?  (lhs | rhs) : (rhs | rhs).
  vfcmp_cond_d(CEQ, scratch0, lhsReg, rhsReg);
  vbitsel_v(scratch0, rhsReg, lhsReg, scratch0);
  vor_v(scratch1, scratch0, rhsReg);
  // scratch0 = isNaN(lhs) ? lhs : scratch1.
  vfcmp_cond_d(CEQ, scratch0, lhsReg, lhsReg);
  vbitsel_v(scratch0, lhsReg, scratch1, scratch0);
  // scratch1 = (lhs < scratch0) ? lhs : scratch0.
  vfcmp_cond_d(CLT, scratch1, lhsReg, scratch0);
  vbitsel_v(scratch1, scratch0, lhsReg, scratch1);
  // Canonicalize the result.
  vfmin_d(dstReg, scratch1, scratch1);
}

void LiftoffAssembler::emit_f64x2_relaxed_max(LiftoffRegister dst,
                                              LiftoffRegister lhs,
                                              LiftoffRegister rhs) {
  VRegister dstReg = dst.fp().toV();
  VRegister lhsReg = lhs.fp().toV();
  VRegister rhsReg = rhs.fp().toV();
  VRegister scratch0 = kSimd128RegZero;
  VRegister scratch1 = kSimd128ScratchReg;
  // If inputs are -0.0. and +0.0, then write +0.0 to scratch1.
  // scratch1 = (lhsReg == rhsReg) ?  (lhsReg & rhsReg) : (rhsReg & rhsReg).
  vfcmp_cond_d(CEQ, scratch0, lhsReg, rhsReg);
  vbitsel_v(scratch0, rhsReg, lhsReg, scratch0);
  vand_v(scratch1, scratch0, rhsReg);
  // scratch0 = isNaN(lhsReg) ? lhsReg : scratch1.
  vfcmp_cond_d(CEQ, scratch0, lhsReg, lhsReg);
  vbitsel_v(scratch0, lhsReg, scratch1, scratch0);
  // scratch1 = (scratch0 < lhsReg) ? lhsReg : scratch0.
  vfcmp_cond_d(CLT, scratch1, scratch0, lhsReg);
  vbitsel_v(scratch1, scratch0, lhsReg, scratch1);
  // Canonicalize the result.
  vfmax_d(dstReg, scratch1, scratch1);
}

void LiftoffAssembler::emit_f64x2_convert_low_i32x4_s(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vilvl_w(kSimd128RegZero, kSimd128RegZero, src.fp().toV());
  vslli_d(kSimd128RegZero, kSimd128RegZero, 32);
  vsrai_d(kSimd128RegZero, kSimd128RegZero, 32);
  vffint_d_l(dst.fp().toV(), kSimd128RegZero);
}

void LiftoffAssembler::emit_f64x2_convert_low_i32x4_u(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vilvl_w(kSimd128RegZero, kSimd128RegZero, src.fp().toV());
  vffint_d_lu(dst.fp().toV(), kSimd128RegZero);
}

void LiftoffAssembler::emit_f64x2_promote_low_f32x4(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  vfcvtl_d_s(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i32x4_sconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  vftintrz_w_s(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i32x4_uconvert_f32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  vftintrz_wu_s(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_s_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vftintrz_l_d(kSimd128ScratchReg, src.fp().toV());
  vsat_d(kSimd128ScratchReg, kSimd128ScratchReg, 31);
  vpickev_w(dst.fp().toV(), kSimd128RegZero, kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_u_zero(LiftoffRegister dst,
                                                         LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vftintrz_lu_d(kSimd128ScratchReg, src.fp().toV());
  vsat_du(kSimd128ScratchReg, kSimd128ScratchReg, 31);
  vpickev_w(dst.fp().toV(), kSimd128RegZero, kSimd128ScratchReg);
}

void LiftoffAssembler::emit_f32x4_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  vffint_s_w(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_f32x4_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  vffint_s_wu(dst.fp().toV(), src.fp().toV());
}

void LiftoffAssembler::emit_f32x4_demote_f64x2_zero(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vfcvt_s_d(dst.fp().toV(), kSimd128RegZero, src.fp().toV());
}

void LiftoffAssembler::emit_i8x16_sconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  vsat_h(kSimd128ScratchReg, lhs.fp().toV(), 7);
  vsat_h(kSimd128RegZero, rhs.fp().toV(), 7);
  vpickev_b(dst.fp().toV(), kSimd128RegZero, kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i8x16_uconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vmax_h(kSimd128ScratchReg, kSimd128RegZero, lhs.fp().toV());
  vsat_hu(kSimd128ScratchReg, kSimd128ScratchReg, 7);
  vmax_h(dst.fp().toV(), kSimd128RegZero, rhs.fp().toV());
  vsat_hu(dst.fp().toV(), dst.fp().toV(), 7);
  vpickev_b(dst.fp().toV(), dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i16x8_sconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  vsat_w(kSimd128ScratchReg, lhs.fp().toV(), 15);
  vsat_w(kSimd128RegZero, rhs.fp().toV(), 15);
  vpickev_h(dst.fp().toV(), kSimd128RegZero, kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i16x8_uconvert_i32x4(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 LiftoffRegister rhs) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vmax_w(kSimd128ScratchReg, kSimd128RegZero, lhs.fp().toV());
  vsat_wu(kSimd128ScratchReg, kSimd128ScratchReg, 15);
  vmax_w(dst.fp().toV(), kSimd128RegZero, rhs.fp().toV());
  vsat_wu(dst.fp().toV(), dst.fp().toV(), 15);
  vpickev_h(dst.fp().toV(), dst.fp().toV(), kSimd128ScratchReg);
}

void LiftoffAssembler::emit_i16x8_sconvert_i8x16_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  vilvl_b(kSimd128ScratchReg, src.fp().toV(), src.fp().toV());
  vslli_h(dst.fp().toV(), kSimd128ScratchReg, 8);
  vsrai_h(dst.fp().toV(), dst.fp().toV(), 8);
}

void LiftoffAssembler::emit_i16x8_sconvert_i8x16_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  vilvh_b(kSimd128ScratchReg, src.fp().toV(), src.fp().toV());
  vslli_h(dst.fp().toV(), kSimd128ScratchReg, 8);
  vsrai_h(dst.fp().toV(), dst.fp().toV(), 8);
}

void LiftoffAssembler::emit_i16x8_uconvert_i8x16_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vilvl_b(dst.fp().toV(), kSimd128RegZero, src.fp().toV());
}

void LiftoffAssembler::emit_i16x8_uconvert_i8x16_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vilvh_b(dst.fp().toV(), kSimd128RegZero, src.fp().toV());
}

void LiftoffAssembler::emit_i32x4_sconvert_i16x8_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  vilvl_h(kSimd128ScratchReg, src.fp().toV(), src.fp().toV());
  vslli_w(dst.fp().toV(), kSimd128ScratchReg, 16);
  vsrai_w(dst.fp().toV(), dst.fp().toV(), 16);
}

void LiftoffAssembler::emit_i32x4_sconvert_i16x8_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  vilvh_h(kSimd128ScratchReg, src.fp().toV(), src.fp().toV());
  vslli_w(dst.fp().toV(), kSimd128ScratchReg, 16);
  vsrai_w(dst.fp().toV(), dst.fp().toV(), 16);
}

void LiftoffAssembler::emit_i32x4_uconvert_i16x8_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vilvl_h(dst.fp().toV(), kSimd128RegZero, src.fp().toV());
}

void LiftoffAssembler::emit_i32x4_uconvert_i16x8_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vilvh_h(dst.fp().toV(), kSimd128RegZero, src.fp().toV());
}

void LiftoffAssembler::emit_i64x2_sconvert_i32x4_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  vilvl_w(kSimd128ScratchReg, src.fp().toV(), src.fp().toV());
  vslli_d(dst.fp().toV(), kSimd128ScratchReg, 32);
  vsrai_d(dst.fp().toV(), dst.fp().toV(), 32);
}

void LiftoffAssembler::emit_i64x2_sconvert_i32x4_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  vilvh_w(kSimd128ScratchReg, src.fp().toV(), src.fp().toV());
  vslli_d(dst.fp().toV(), kSimd128ScratchReg, 32);
  vsrai_d(dst.fp().toV(), dst.fp().toV(), 32);
}

void LiftoffAssembler::emit_i64x2_uconvert_i32x4_low(LiftoffRegister dst,
                                                     LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vilvl_w(dst.fp().toV(), kSimd128RegZero, src.fp().toV());
}

void LiftoffAssembler::emit_i64x2_uconvert_i32x4_high(LiftoffRegister dst,
                                                      LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vilvh_w(dst.fp().toV(), kSimd128RegZero, src.fp().toV());
}

void LiftoffAssembler::emit_i8x16_rounding_average_u(LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
  vavgr_bu(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i16x8_rounding_average_u(LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
  vavgr_hu(dst.fp().toV(), rhs.fp().toV(), lhs.fp().toV());
}

void LiftoffAssembler::emit_i8x16_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vadda_b(dst.fp().toV(), src.fp().toV(), kSimd128RegZero);
}

void LiftoffAssembler::emit_i16x8_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vadda_h(dst.fp().toV(), src.fp().toV(), kSimd128RegZero);
}

void LiftoffAssembler::emit_i32x4_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  vxor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
  vadda_w(dst.fp().toV(), src.fp().toV(), kSimd128RegZero);
}

void LiftoffAssembler::emit_i8x16_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  vpickve2gr_b(dst.gp(), lhs.fp().toV(), imm_lane_idx);
}

void LiftoffAssembler::emit_i8x16_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  vpickve2gr_bu(dst.gp(), lhs.fp().toV(), imm_lane_idx);
}

void LiftoffAssembler::emit_i16x8_extract_lane_s(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  vpickve2gr_h(dst.gp(), lhs.fp().toV(), imm_lane_idx);
}

void LiftoffAssembler::emit_i16x8_extract_lane_u(LiftoffRegister dst,
                                                 LiftoffRegister lhs,
                                                 uint8_t imm_lane_idx) {
  vpickve2gr_hu(dst.gp(), lhs.fp().toV(), imm_lane_idx);
}

void LiftoffAssembler::emit_i32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  vpickve2gr_w(dst.gp(), lhs.fp().toV(), imm_lane_idx);
}

void LiftoffAssembler::emit_i64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  vpickve2gr_d(dst.gp(), lhs.fp().toV(), imm_lane_idx);
}

void LiftoffAssembler::emit_f32x4_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  vpickve2gr_wu(kScratchReg, lhs.fp().toV(), imm_lane_idx);
  MacroAssembler::FmoveLow(dst.fp(), kScratchReg);
}

void LiftoffAssembler::emit_f64x2_extract_lane(LiftoffRegister dst,
                                               LiftoffRegister lhs,
                                               uint8_t imm_lane_idx) {
  vpickve2gr_d(kScratchReg, lhs.fp().toV(), imm_lane_idx);
  MacroAssembler::Move(dst.fp(), kScratchReg);
}

void LiftoffAssembler::emit_i8x16_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (dst != src1) {
    vaddi_bu(dst.fp().toV(), src1.fp().toV(), 0);
  }
  vinsgr2vr_b(dst.fp().toV(), src2.gp(), imm_lane_idx);
}

void LiftoffAssembler::emit_i16x8_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (dst != src1) {
    vaddi_hu(dst.fp().toV(), src1.fp().toV(), 0);
  }
  vinsgr2vr_h(dst.fp().toV(), src2.gp(), imm_lane_idx);
}

void LiftoffAssembler::emit_i32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (dst != src1) {
    vaddi_wu(dst.fp().toV(), src1.fp().toV(), 0);
  }
  vinsgr2vr_w(dst.fp().toV(), src2.gp(), imm_lane_idx);
}

void LiftoffAssembler::emit_i64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  if (dst != src1) {
    vaddi_du(dst.fp().toV(), src1.fp().toV(), 0);
  }
  vinsgr2vr_d(dst.fp().toV(), src2.gp(), imm_lane_idx);
}

void LiftoffAssembler::emit_f32x4_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  MacroAssembler::FmoveLow(kScratchReg, src2.fp());
  if (dst != src1) {
    vaddi_wu(dst.fp().toV(), src1.fp().toV(), 0);
  }
  vinsgr2vr_w(dst.fp().toV(), kScratchReg, imm_lane_idx);
}

void LiftoffAssembler::emit_f64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  MacroAssembler::Move(kScratchReg, src2.fp());
  if (dst != src1) {
    vaddi_du(dst.fp().toV(), src1.fp().toV(), 0);
  }
  vinsgr2vr_d(dst.fp().toV(), kScratchReg, imm_lane_idx);
}

void LiftoffAssembler::emit_f32x4_qfma(LiftoffRegister dst,
                                       LiftoffRegister src1,
                                       LiftoffRegister src2,
                                       LiftoffRegister src3) {
  vfmadd_s(dst.fp().toV(), src1.fp().toV(), src2.fp().toV(), src3.fp().toV());
}

void LiftoffAssembler::emit_f32x4_qfms(LiftoffRegister dst,
                                       LiftoffRegister src1,
                                       LiftoffRegister src2,
                                       LiftoffRegister src3) {
  vfnmsub_s(dst.fp().toV(), src1.fp().toV(), src2.fp().toV(), src3.fp().toV());
}

void LiftoffAssembler::emit_f64x2_qfma(LiftoffRegister dst,
                                       LiftoffRegister src1,
                                       LiftoffRegister src2,
                                       LiftoffRegister src3) {
  vfmadd_d(dst.fp().toV(), src1.fp().toV(), src2.fp().toV(), src3.fp().toV());
}

void LiftoffAssembler::emit_f64x2_qfms(LiftoffRegister dst,
                                       LiftoffRegister src1,
                                       LiftoffRegister src2,
                                       LiftoffRegister src3) {
  vfnmsub_d(dst.fp().toV(), src1.fp().toV(), src2.fp().toV(), src3.fp().toV());
}

bool LiftoffAssembler::emit_f16x8_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  return false;
}

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

bool LiftoffAssembler::emit_f16x8_abs(LiftoffRegister dst,
                                      LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_neg(LiftoffRegister dst,
                                      LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_sqrt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_ceil(LiftoffRegister dst,
                                       LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_floor(LiftoffRegister dst,
                                        LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_trunc(LiftoffRegister dst,
                                        LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_nearest_int(LiftoffRegister dst,
                                              LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_eq(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_ne(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_lt(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_le(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_add(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_div(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_pmin(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_pmax(LiftoffRegister dst, LiftoffRegister lhs,
                                       LiftoffRegister rhs) {
  return false;
}

bool LiftoffAssembler::emit_i16x8_sconvert_f16x8(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_i16x8_uconvert_f16x8(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_sconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_uconvert_i16x8(LiftoffRegister dst,
                                                 LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_demote_f32x4_zero(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f16x8_demote_f64x2_zero(LiftoffRegister dst,
                                                    LiftoffRegister src) {
  return false;
}

bool LiftoffAssembler::emit_f32x4_promote_low_f16x8(LiftoffRegister dst,
                                                    LiftoffRegister src) {
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

bool LiftoffAssembler::supports_f16_mem_access() { return false; }

void LiftoffAssembler::set_trap_on_oob_mem64(Register index, uint64_t max_index,
                                             Label* trap_label) {
  Branch(trap_label, kUnsignedGreaterThanEqual, index, Operand(max_index));
}

void LiftoffAssembler::emit_inc_i32_at(Address address) {
  UseScratchRegisterScope temps(this);
  Register counter_addr = temps.Acquire();
  Register value = temps.Acquire();
  li(counter_addr, Operand(static_cast<uint64_t>(address)));
  Ld_w(value, MemOperand(counter_addr, 0));
  Add_w(value, value, Operand(1));
  St_w(value, MemOperand(counter_addr, 0));
}

void LiftoffAssembler::StackCheck(Label* ool_code) {
  Register limit_address = kScratchReg;
  LoadStackLimit(limit_address, StackLimitKind::kInterruptStackLimit);
  Branch(ool_code, ule, sp, Operand(limit_address));
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  MacroAssembler::AssertUnreachable(reason);
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  unsigned num_gp_regs = gp_regs.GetNumRegsSet();
  if (num_gp_regs) {
    unsigned offset = num_gp_regs * kSystemPointerSize;
    addi_d(sp, sp, -offset);
    while (!gp_regs.is_empty()) {
      LiftoffRegister reg = gp_regs.GetFirstRegSet();
      offset -= kSystemPointerSize;
      St_d(reg.gp(), MemOperand(sp, offset));
      gp_regs.clear(reg);
    }
    DCHECK_EQ(offset, 0);
  }
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned num_fp_regs = fp_regs.GetNumRegsSet();
  if (num_fp_regs) {
    unsigned slot_size = IsEnabled(LSX) ? 16 : 8;
    addi_d(sp, sp, -(num_fp_regs * slot_size));
    unsigned offset = 0;
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetFirstRegSet();
      if (IsEnabled(LSX)) {
        MacroAssembler::Vst(reg.fp().toV(), MemOperand(sp, offset));
      } else {
        MacroAssembler::Fst_d(reg.fp(), MemOperand(sp, offset));
      }
      fp_regs.clear(reg);
      offset += slot_size;
    }
    DCHECK_EQ(offset, num_fp_regs * slot_size);
  }
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned fp_offset = 0;
  while (!fp_regs.is_empty()) {
    LiftoffRegister reg = fp_regs.GetFirstRegSet();
    if (IsEnabled(LSX)) {
      MacroAssembler::Vld(reg.fp().toV(), MemOperand(sp, fp_offset));
    } else {
      MacroAssembler::Fld_d(reg.fp(), MemOperand(sp, fp_offset));
    }
    fp_regs.clear(reg);
    fp_offset += (IsEnabled(LSX) ? 16 : 8);
  }
  if (fp_offset) addi_d(sp, sp, fp_offset);
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  unsigned gp_offset = 0;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetLastRegSet();
    Ld_d(reg.gp(), MemOperand(sp, gp_offset));
    gp_regs.clear(reg);
    gp_offset += kSystemPointerSize;
  }
  addi_d(sp, sp, gp_offset);
}

void LiftoffAssembler::RecordSpillsInSafepoint(
    SafepointTableBuilder::Safepoint& safepoint, LiftoffRegList all_spills,
    LiftoffRegList ref_spills, int spill_offset) {
  LiftoffRegList fp_spills = all_spills & kFpCacheRegList;
  int spill_space_size = fp_spills.GetNumRegsSet() * kSimd128Size;
  LiftoffRegList gp_spills = all_spills & kGpCacheRegList;
  while (!gp_spills.is_empty()) {
    LiftoffRegister reg = gp_spills.GetFirstRegSet();
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
  DCHECK_LT(num_stack_slots,
            (1 << 16) / kSystemPointerSize);  // 16 bit immediate
  Drop(static_cast<int>(num_stack_slots));
  Ret();
}

void LiftoffAssembler::CallCWithStackBuffer(
    const std::initializer_list<VarState> args, const LiftoffRegister* rets,
    ValueKind return_kind, ValueKind out_argument_kind, int stack_bytes,
    ExternalReference ext_ref) {
  addi_d(sp, sp, -stack_bytes);

  int arg_offset = 0;
  for (const VarState& arg : args) {
    liftoff::StoreToMemory(this, MemOperand{sp, arg_offset}, arg);
    arg_offset += value_kind_size(arg.kind());
  }
  DCHECK_LE(arg_offset, stack_bytes);

  // Pass a pointer to the buffer with the arguments to the C function.
  // On LoongArch, the first argument is passed in {a0}.
  constexpr Register kFirstArgReg = a0;
  mov(kFirstArgReg, sp);

  // Now call the C function.
  constexpr int kNumCCallArgs = 1;
  PrepareCallCFunction(kNumCCallArgs, kScratchReg);
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* next_result_reg = rets;
  if (return_kind != kVoid) {
    constexpr Register kReturnReg = a0;
#ifdef USE_SIMULATOR
    // When calling a host function in the simulator, if the function returns an
    // int32 value, the simulator does not sign-extend it to int64 because in
    // the simulator we do not know whether the function returns an int32 or
    // an int64. So we need to sign extend it here.
    if (return_kind == kI32) {
      slli_w(next_result_reg->gp(), kReturnReg, 0);
    } else if (kReturnReg != next_result_reg->gp()) {
      Move(*next_result_reg, LiftoffRegister(kReturnReg), return_kind);
    }
#else
    if (kReturnReg != next_result_reg->gp()) {
      Move(*next_result_reg, LiftoffRegister(kReturnReg), return_kind);
    }
#endif
    ++next_result_reg;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_kind != kVoid) {
    liftoff::Load(this, *next_result_reg, MemOperand(sp, 0), out_argument_kind);
  }

  addi_d(sp, sp, stack_bytes);
}

void LiftoffAssembler::CallC(const std::initializer_list<VarState> args_list,
                             ExternalReference ext_ref) {
  // First, prepare the stack for the C call.
  const int num_args = static_cast<int>(args_list.size());
  PrepareCallCFunction(num_args, kScratchReg);

  // Note: If we ever need more than eight arguments we would need to load the
  // stack arguments to registers (via LoadToRegister), then push them to the
  // stack.

  // Execute the parallel register move for register parameters.
  DCHECK_GE(arraysize(kCArgRegs), num_args);
  const VarState* const args = args_list.begin();
  ParallelMove parallel_move{this};
  for (int reg_arg = 0; reg_arg < num_args; ++reg_arg) {
    parallel_move.LoadIntoRegister(LiftoffRegister{kCArgRegs[reg_arg]},
                                   args[reg_arg]);
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
  // For loong64, we have more cache registers than wasm parameters. That means
  // that target will always be in a register.
  DCHECK(target.is_valid());
  CallWasmCodePointer(target, call_descriptor->signature_hash());
}

void LiftoffAssembler::TailCallIndirect(
    compiler::CallDescriptor* call_descriptor, Register target) {
  DCHECK(target.is_valid());
  CallWasmCodePointer(target, call_descriptor->signature_hash(),
                      CallJumpMode::kTailCall);
}

void LiftoffAssembler::CallBuiltin(Builtin builtin) {
  // A direct call to a builtin. Just encode the builtin index. This will be
  // patched at relocation.
  Call(static_cast<Address>(builtin), RelocInfo::WASM_STUB_CALL);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  addi_d(sp, sp, -size);
  MacroAssembler::Move(addr, sp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  addi_d(sp, sp, size);
}

void LiftoffAssembler::MaybeOSR() {}

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
          asm_->Ld_d(kScratchReg, liftoff::GetStackSlot(slot.src_offset_));
          asm_->Push(kScratchReg);
        } else {
          asm_->AllocateStackSpace(stack_decrement - kSimd128Size);
          asm_->Vld(kScratchDoubleReg.toV(),
                    liftoff::GetStackSlot(slot.src_offset_));
          asm_->Sub_d(sp, sp, Operand(kSimd128Size));
          asm_->Vst(kScratchDoubleReg.toV(), MemOperand(sp, 0));
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
        asm_->Push(kScratchReg);
        break;
      }
    }
  }
}

}  // namespace v8::internal::wasm

#endif  // V8_WASM_BASELINE_LOONG64_LIFTOFF_ASSEMBLER_LOONG64_INL_H_

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>  // For assert
#include <limits.h>  // For LONG_MIN, LONG_MAX.

#if V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/codegen/callable.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/external-reference-table.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/register-configuration.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frames-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/snapshot.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-code-manager.h"
#endif  // V8_ENABLE_WEBASSEMBLY

// Satisfy cpplint check, but don't include platform-specific header. It is
// included recursively via macro-assembler.h.
#if 0
#include "src/codegen/ppc/macro-assembler-ppc.h"
#endif

namespace v8 {
namespace internal {

namespace {

// Simd and Floating Pointer registers are not shared. For WebAssembly we save
// both registers, If we are not running Wasm, we can get away with only saving
// FP registers.
#if V8_ENABLE_WEBASSEMBLY
constexpr int kStackSavedSavedFPSizeInBytes =
    (kNumCallerSavedDoubles * kSimd128Size) +
    (kNumCallerSavedDoubles * kDoubleSize);
#else
constexpr int kStackSavedSavedFPSizeInBytes =
    kNumCallerSavedDoubles * kDoubleSize;
#endif  // V8_ENABLE_WEBASSEMBLY

}  // namespace

int TurboAssembler::RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                                    Register exclusion1,
                                                    Register exclusion2,
                                                    Register exclusion3) const {
  int bytes = 0;

  RegList exclusions = {exclusion1, exclusion2, exclusion3};
  RegList list = kJSCallerSaved - exclusions;
  bytes += list.Count() * kSystemPointerSize;

  if (fp_mode == SaveFPRegsMode::kSave) {
    bytes += kStackSavedSavedFPSizeInBytes;
  }

  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                    Register exclusion2, Register exclusion3) {
  int bytes = 0;

  RegList exclusions = {exclusion1, exclusion2, exclusion3};
  RegList list = kJSCallerSaved - exclusions;
  MultiPush(list);
  bytes += list.Count() * kSystemPointerSize;

  if (fp_mode == SaveFPRegsMode::kSave) {
    MultiPushF64AndV128(kCallerSavedDoubles, kCallerSavedSimd128s);
    bytes += kStackSavedSavedFPSizeInBytes;
  }

  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                   Register exclusion2, Register exclusion3) {
  int bytes = 0;
  if (fp_mode == SaveFPRegsMode::kSave) {
    MultiPopF64AndV128(kCallerSavedDoubles, kCallerSavedSimd128s);
    bytes += kStackSavedSavedFPSizeInBytes;
  }

  RegList exclusions = {exclusion1, exclusion2, exclusion3};
  RegList list = kJSCallerSaved - exclusions;
  MultiPop(list);
  bytes += list.Count() * kSystemPointerSize;

  return bytes;
}

void TurboAssembler::Jump(Register target) {
  mtctr(target);
  bctr();
}

void TurboAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kBuiltinsConstantsTable));

  DCHECK_NE(destination, r0);
  LoadRoot(destination, RootIndex::kBuiltinsConstantsTable);
  LoadTaggedPointerField(
      destination,
      FieldMemOperand(destination,
                      FixedArray::OffsetOfElementAt(constant_index)),
      r0);
}

void TurboAssembler::LoadRootRelative(Register destination, int32_t offset) {
  LoadU64(destination, MemOperand(kRootRegister, offset), r0);
}

void TurboAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  if (offset == 0) {
    mr(destination, kRootRegister);
  } else {
    AddS64(destination, kRootRegister, Operand(offset), destination);
  }
}

void TurboAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond, CRegister cr) {
  Label skip;

  if (cond != al) b(NegateCondition(cond), &skip, cr);

  mov(ip, Operand(target, rmode));
  mtctr(ip);
  bctr();

  bind(&skip);
}

void TurboAssembler::Jump(Address target, RelocInfo::Mode rmode, Condition cond,
                          CRegister cr) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(static_cast<intptr_t>(target), rmode, cond, cr);
}

void TurboAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond, CRegister cr) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code));

  Builtin builtin = Builtin::kNoBuiltinId;
  bool target_is_builtin =
      isolate()->builtins()->IsBuiltinHandle(code, &builtin);

  if (root_array_available_ && options().isolate_independent_code) {
    Label skip;
    Register scratch = ip;
    int offset = IsolateData::BuiltinEntrySlotOffset(code->builtin_id());
    LoadU64(scratch, MemOperand(kRootRegister, offset), r0);
    if (cond != al) b(NegateCondition(cond), &skip, cr);
    Jump(scratch);
    bind(&skip);
    return;
  } else if (options().inline_offheap_trampolines && target_is_builtin) {
    // Inline the trampoline.
    Label skip;
    RecordCommentForOffHeapTrampoline(builtin);
    // Use ip directly instead of using UseScratchRegisterScope, as we do
    // not preserve scratch registers across calls.
    mov(ip, Operand(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET));
    if (cond != al) b(NegateCondition(cond), &skip, cr);
    Jump(ip);
    bind(&skip);
    return;
  }
  int32_t target_index = AddCodeTarget(code);
  Jump(static_cast<intptr_t>(target_index), rmode, cond, cr);
}

void TurboAssembler::Jump(const ExternalReference& reference) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Move(scratch, reference);
  if (ABI_USES_FUNCTION_DESCRIPTORS) {
    // AIX uses a function descriptor. When calling C code be
    // aware of this descriptor and pick up values from it.
    LoadU64(ToRegister(ABI_TOC_REGISTER),
            MemOperand(scratch, kSystemPointerSize));
    LoadU64(scratch, MemOperand(scratch, 0));
  }
  Jump(scratch);
}

void TurboAssembler::Call(Register target) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  // branch via link register and set LK bit for return point
  mtctr(target);
  bctrl();
}

void MacroAssembler::CallJSEntry(Register target) {
  CHECK(target == r5);
  Call(target);
}

int MacroAssembler::CallSizeNotPredictableCodeSize(Address target,
                                                   RelocInfo::Mode rmode,
                                                   Condition cond) {
  return (2 + kMovInstructionsNoConstantPool) * kInstrSize;
}

void TurboAssembler::Call(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(cond == al);

  // This can likely be optimized to make use of bc() with 24bit relative
  //
  // RecordRelocInfo(x.rmode_, x.immediate);
  // bc( BA, .... offset, LKset);
  //

  mov(ip, Operand(target, rmode));
  mtctr(ip);
  bctrl();
}

void TurboAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code));
  DCHECK_IMPLIES(options().use_pc_relative_calls_and_jumps,
                 Builtins::IsIsolateIndependentBuiltin(*code));

  Builtin builtin = Builtin::kNoBuiltinId;
  bool target_is_builtin =
      isolate()->builtins()->IsBuiltinHandle(code, &builtin);

  if (root_array_available_ && options().isolate_independent_code) {
    Label skip;
    int offset = IsolateData::BuiltinEntrySlotOffset(code->builtin_id());
    LoadU64(ip, MemOperand(kRootRegister, offset));
    if (cond != al) b(NegateCondition(cond), &skip);
    Call(ip);
    bind(&skip);
    return;
  } else if (options().inline_offheap_trampolines && target_is_builtin) {
    // Inline the trampoline.
    CallBuiltin(builtin, cond);
    return;
  }
  DCHECK(code->IsExecutable());
  int32_t target_index = AddCodeTarget(code);
  Call(static_cast<Address>(target_index), rmode, cond);
}

void TurboAssembler::CallBuiltin(Builtin builtin, Condition cond) {
  ASM_CODE_COMMENT_STRING(this, CommentForOffHeapTrampoline("call", builtin));
  DCHECK(Builtins::IsBuiltinId(builtin));
  // Use ip directly instead of using UseScratchRegisterScope, as we do not
  // preserve scratch registers across calls.
  mov(ip, Operand(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET));
  Label skip;
  if (cond != al) b(NegateCondition(cond), &skip);
  Call(ip);
  bind(&skip);
}

void TurboAssembler::TailCallBuiltin(Builtin builtin) {
  ASM_CODE_COMMENT_STRING(this,
                          CommentForOffHeapTrampoline("tail call", builtin));
  mov(ip, Operand(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET));
  Jump(ip);
}

void TurboAssembler::Drop(int count) {
  if (count > 0) {
    AddS64(sp, sp, Operand(count * kSystemPointerSize), r0);
  }
}

void TurboAssembler::Drop(Register count, Register scratch) {
  ShiftLeftU64(scratch, count, Operand(kSystemPointerSizeLog2));
  add(sp, sp, scratch);
}

void TurboAssembler::Call(Label* target) { b(target, SetLK); }

void TurboAssembler::Push(Handle<HeapObject> handle) {
  mov(r0, Operand(handle));
  push(r0);
}

void TurboAssembler::Push(Smi smi) {
  mov(r0, Operand(smi));
  push(r0);
}

void TurboAssembler::PushArray(Register array, Register size, Register scratch,
                               Register scratch2, PushArrayOrder order) {
  Label loop, done;

  if (order == kNormal) {
    cmpi(size, Operand::Zero());
    beq(&done);
    ShiftLeftU64(scratch, size, Operand(kSystemPointerSizeLog2));
    add(scratch, array, scratch);
    mtctr(size);

    bind(&loop);
    LoadU64WithUpdate(scratch2, MemOperand(scratch, -kSystemPointerSize));
    StoreU64WithUpdate(scratch2, MemOperand(sp, -kSystemPointerSize));
    bdnz(&loop);

    bind(&done);
  } else {
    cmpi(size, Operand::Zero());
    beq(&done);

    mtctr(size);
    subi(scratch, array, Operand(kSystemPointerSize));

    bind(&loop);
    LoadU64WithUpdate(scratch2, MemOperand(scratch, kSystemPointerSize));
    StoreU64WithUpdate(scratch2, MemOperand(sp, -kSystemPointerSize));
    bdnz(&loop);
    bind(&done);
  }
}

void TurboAssembler::Move(Register dst, Handle<HeapObject> value,
                          RelocInfo::Mode rmode) {
  // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
  // non-isolate-independent code. In many cases it might be cheaper than
  // embedding the relocatable value.
  if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadConstant(dst, value);
    return;
  } else if (RelocInfo::IsCompressedEmbeddedObject(rmode)) {
    EmbeddedObjectIndex index = AddEmbeddedObject(value);
    DCHECK(is_uint32(index));
    mov(dst, Operand(static_cast<int>(index), rmode));
  } else {
    DCHECK(RelocInfo::IsFullEmbeddedObject(rmode));
    mov(dst, Operand(value.address(), rmode));
  }
}

void TurboAssembler::Move(Register dst, ExternalReference reference) {
  // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
  // non-isolate-independent code. In many cases it might be cheaper than
  // embedding the relocatable value.
  if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadExternalReference(dst, reference);
    return;
  }
  mov(dst, Operand(reference));
}

void TurboAssembler::Move(Register dst, Register src, Condition cond) {
  DCHECK(cond == al);
  if (dst != src) {
    mr(dst, src);
  }
}

void TurboAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  if (dst != src) {
    fmr(dst, src);
  }
}

void TurboAssembler::MultiPush(RegList regs, Register location) {
  int16_t num_to_push = regs.Count();
  int16_t stack_offset = num_to_push * kSystemPointerSize;

  subi(location, location, Operand(stack_offset));
  for (int16_t i = Register::kNumRegisters - 1; i >= 0; i--) {
    if ((regs.bits() & (1 << i)) != 0) {
      stack_offset -= kSystemPointerSize;
      StoreU64(ToRegister(i), MemOperand(location, stack_offset));
    }
  }
}

void TurboAssembler::MultiPop(RegList regs, Register location) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < Register::kNumRegisters; i++) {
    if ((regs.bits() & (1 << i)) != 0) {
      LoadU64(ToRegister(i), MemOperand(location, stack_offset));
      stack_offset += kSystemPointerSize;
    }
  }
  addi(location, location, Operand(stack_offset));
}

void TurboAssembler::MultiPushDoubles(DoubleRegList dregs, Register location) {
  int16_t num_to_push = dregs.Count();
  int16_t stack_offset = num_to_push * kDoubleSize;

  subi(location, location, Operand(stack_offset));
  for (int16_t i = DoubleRegister::kNumRegisters - 1; i >= 0; i--) {
    if ((dregs.bits() & (1 << i)) != 0) {
      DoubleRegister dreg = DoubleRegister::from_code(i);
      stack_offset -= kDoubleSize;
      stfd(dreg, MemOperand(location, stack_offset));
    }
  }
}

void TurboAssembler::MultiPushV128(Simd128RegList simd_regs,
                                   Register location) {
  int16_t num_to_push = simd_regs.Count();
  int16_t stack_offset = num_to_push * kSimd128Size;

  subi(location, location, Operand(stack_offset));
  for (int16_t i = Simd128Register::kNumRegisters - 1; i >= 0; i--) {
    if ((simd_regs.bits() & (1 << i)) != 0) {
      Simd128Register simd_reg = Simd128Register::from_code(i);
      stack_offset -= kSimd128Size;
      li(ip, Operand(stack_offset));
      StoreSimd128(simd_reg, MemOperand(location, ip));
    }
  }
}

void TurboAssembler::MultiPopDoubles(DoubleRegList dregs, Register location) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < DoubleRegister::kNumRegisters; i++) {
    if ((dregs.bits() & (1 << i)) != 0) {
      DoubleRegister dreg = DoubleRegister::from_code(i);
      lfd(dreg, MemOperand(location, stack_offset));
      stack_offset += kDoubleSize;
    }
  }
  addi(location, location, Operand(stack_offset));
}

void TurboAssembler::MultiPopV128(Simd128RegList simd_regs, Register location) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < Simd128Register::kNumRegisters; i++) {
    if ((simd_regs.bits() & (1 << i)) != 0) {
      Simd128Register simd_reg = Simd128Register::from_code(i);
      li(ip, Operand(stack_offset));
      LoadSimd128(simd_reg, MemOperand(location, ip));
      stack_offset += kSimd128Size;
    }
  }
  addi(location, location, Operand(stack_offset));
}

void TurboAssembler::MultiPushF64AndV128(DoubleRegList dregs,
                                         Simd128RegList simd_regs,
                                         Register location) {
  MultiPushDoubles(dregs);
#if V8_ENABLE_WEBASSEMBLY
  bool generating_bultins =
      isolate() && isolate()->IsGeneratingEmbeddedBuiltins();
  if (generating_bultins) {
    // V8 uses the same set of fp param registers as Simd param registers.
    // As these registers are two different sets on ppc we must make
    // sure to also save them when Simd is enabled.
    // Check the comments under crrev.com/c/2645694 for more details.
    Label push_empty_simd, simd_pushed;
    Move(ip, ExternalReference::supports_wasm_simd_128_address());
    LoadU8(ip, MemOperand(ip), r0);
    cmpi(ip, Operand::Zero());  // If > 0 then simd is available.
    ble(&push_empty_simd);
    MultiPushV128(simd_regs);
    b(&simd_pushed);
    bind(&push_empty_simd);
    // We still need to allocate empty space on the stack even if we
    // are not pushing Simd registers (see kFixedFrameSizeFromFp).
    addi(sp, sp,
         Operand(-static_cast<int8_t>(simd_regs.Count()) * kSimd128Size));
    bind(&simd_pushed);
  } else {
    if (CpuFeatures::SupportsWasmSimd128()) {
      MultiPushV128(simd_regs);
    } else {
      addi(sp, sp,
           Operand(-static_cast<int8_t>(simd_regs.Count()) * kSimd128Size));
    }
  }
#endif
}

void TurboAssembler::MultiPopF64AndV128(DoubleRegList dregs,
                                        Simd128RegList simd_regs,
                                        Register location) {
#if V8_ENABLE_WEBASSEMBLY
  bool generating_bultins =
      isolate() && isolate()->IsGeneratingEmbeddedBuiltins();
  if (generating_bultins) {
    Label pop_empty_simd, simd_popped;
    Move(ip, ExternalReference::supports_wasm_simd_128_address());
    LoadU8(ip, MemOperand(ip), r0);
    cmpi(ip, Operand::Zero());  // If > 0 then simd is available.
    ble(&pop_empty_simd);
    MultiPopV128(simd_regs);
    b(&simd_popped);
    bind(&pop_empty_simd);
    addi(sp, sp,
         Operand(static_cast<int8_t>(simd_regs.Count()) * kSimd128Size));
    bind(&simd_popped);
  } else {
    if (CpuFeatures::SupportsWasmSimd128()) {
      MultiPopV128(simd_regs);
    } else {
      addi(sp, sp,
           Operand(static_cast<int8_t>(simd_regs.Count()) * kSimd128Size));
    }
  }
#endif
  MultiPopDoubles(dregs);
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index,
                              Condition cond) {
  DCHECK(cond == al);
  LoadU64(destination,
          MemOperand(kRootRegister, RootRegisterOffsetForRootIndex(index)), r0);
}

void TurboAssembler::LoadTaggedPointerField(const Register& destination,
                                            const MemOperand& field_operand,
                                            const Register& scratch) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTaggedPointer(destination, field_operand);
  } else {
    LoadU64(destination, field_operand, scratch);
  }
}

void TurboAssembler::LoadAnyTaggedField(const Register& destination,
                                        const MemOperand& field_operand,
                                        const Register& scratch) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressAnyTagged(destination, field_operand);
  } else {
    LoadU64(destination, field_operand, scratch);
  }
}

void TurboAssembler::SmiUntag(Register dst, const MemOperand& src, RCBit rc,
                              Register scratch) {
  if (SmiValuesAre31Bits()) {
    LoadU32(dst, src, scratch);
  } else {
    LoadU64(dst, src, scratch);
  }

  SmiUntag(dst, rc);
}

void TurboAssembler::StoreTaggedField(const Register& value,
                                      const MemOperand& dst_field_operand,
                                      const Register& scratch) {
  if (COMPRESS_POINTERS_BOOL) {
    RecordComment("[ StoreTagged");
    StoreU32(value, dst_field_operand, scratch);
    RecordComment("]");
  } else {
    StoreU64(value, dst_field_operand, scratch);
  }
}

void TurboAssembler::DecompressTaggedSigned(Register destination,
                                            Register src) {
  RecordComment("[ DecompressTaggedSigned");
  ZeroExtWord32(destination, src);
  RecordComment("]");
}

void TurboAssembler::DecompressTaggedSigned(Register destination,
                                            MemOperand field_operand) {
  RecordComment("[ DecompressTaggedSigned");
  LoadU32(destination, field_operand, r0);
  RecordComment("]");
}

void TurboAssembler::DecompressTaggedPointer(Register destination,
                                             Register source) {
  RecordComment("[ DecompressTaggedPointer");
  ZeroExtWord32(destination, source);
  add(destination, destination, kRootRegister);
  RecordComment("]");
}

void TurboAssembler::DecompressTaggedPointer(Register destination,
                                             MemOperand field_operand) {
  RecordComment("[ DecompressTaggedPointer");
  LoadU32(destination, field_operand, r0);
  add(destination, destination, kRootRegister);
  RecordComment("]");
}

void TurboAssembler::DecompressAnyTagged(Register destination,
                                         MemOperand field_operand) {
  RecordComment("[ DecompressAnyTagged");
  LoadU32(destination, field_operand, r0);
  add(destination, destination, kRootRegister);
  RecordComment("]");
}

void TurboAssembler::DecompressAnyTagged(Register destination,
                                         Register source) {
  RecordComment("[ DecompressAnyTagged");
  ZeroExtWord32(destination, source);
  add(destination, destination, kRootRegister);
  RecordComment("]");
}

void TurboAssembler::LoadTaggedSignedField(Register destination,
                                           MemOperand field_operand,
                                           Register scratch) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTaggedSigned(destination, field_operand);
  } else {
    LoadU64(destination, field_operand, scratch);
  }
}

void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value, Register slot_address,
                                      LinkRegisterStatus lr_status,
                                      SaveFPRegsMode save_fp,
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == SmiCheck::kInline) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kSystemPointerSize.
  DCHECK(IsAligned(offset, kTaggedSize));

  AddS64(slot_address, object, Operand(offset - kHeapObjectTag), r0);
  if (FLAG_debug_code) {
    Label ok;
    andi(r0, slot_address, Operand(kTaggedSize - 1));
    beq(&ok, cr0);
    stop();
    bind(&ok);
  }

  RecordWrite(object, slot_address, value, lr_status, save_fp,
              remembered_set_action, SmiCheck::kOmit);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (FLAG_debug_code) {
    mov(value, Operand(bit_cast<intptr_t>(kZapValue + 4)));
    mov(slot_address, Operand(bit_cast<intptr_t>(kZapValue + 8)));
  }
}

void TurboAssembler::MaybeSaveRegisters(RegList registers) {
  if (registers.is_empty()) return;
  MultiPush(registers);
}

void TurboAssembler::MaybeRestoreRegisters(RegList registers) {
  if (registers.is_empty()) return;
  MultiPop(registers);
}

void TurboAssembler::CallEphemeronKeyBarrier(Register object,
                                             Register slot_address,
                                             SaveFPRegsMode fp_mode) {
  DCHECK(!AreAliased(object, slot_address));
  RegList registers =
      WriteBarrierDescriptor::ComputeSavedRegisters(object, slot_address);
  MaybeSaveRegisters(registers);

  Register object_parameter = WriteBarrierDescriptor::ObjectRegister();
  Register slot_address_parameter =
      WriteBarrierDescriptor::SlotAddressRegister();

  push(object);
  push(slot_address);
  pop(slot_address_parameter);
  pop(object_parameter);

  Call(isolate()->builtins()->code_handle(
           Builtins::GetEphemeronKeyBarrierStub(fp_mode)),
       RelocInfo::CODE_TARGET);
  MaybeRestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStubSaveRegisters(
    Register object, Register slot_address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode,
    StubCallMode mode) {
  DCHECK(!AreAliased(object, slot_address));
  RegList registers =
      WriteBarrierDescriptor::ComputeSavedRegisters(object, slot_address);
  MaybeSaveRegisters(registers);

  Register object_parameter = WriteBarrierDescriptor::ObjectRegister();
  Register slot_address_parameter =
      WriteBarrierDescriptor::SlotAddressRegister();

  push(object);
  push(slot_address);
  pop(slot_address_parameter);
  pop(object_parameter);

  CallRecordWriteStub(object_parameter, slot_address_parameter,
                      remembered_set_action, fp_mode, mode);

  MaybeRestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register slot_address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode,
    StubCallMode mode) {
  // Use CallRecordWriteStubSaveRegisters if the object and slot registers
  // need to be caller saved.
  DCHECK_EQ(WriteBarrierDescriptor::ObjectRegister(), object);
  DCHECK_EQ(WriteBarrierDescriptor::SlotAddressRegister(), slot_address);
#if V8_ENABLE_WEBASSEMBLY
  if (mode == StubCallMode::kCallWasmRuntimeStub) {
    // Use {near_call} for direct Wasm call within a module.
    auto wasm_target =
        wasm::WasmCode::GetRecordWriteStub(remembered_set_action, fp_mode);
    Call(wasm_target, RelocInfo::WASM_STUB_CALL);
#else
  if (false) {
#endif
  } else {
    auto builtin_index =
        Builtins::GetRecordWriteStub(remembered_set_action, fp_mode);
    if (options().inline_offheap_trampolines) {
      RecordCommentForOffHeapTrampoline(builtin_index);
      // Use ip directly instead of using UseScratchRegisterScope, as we do
      // not preserve scratch registers across calls.
      mov(ip, Operand(BuiltinEntry(builtin_index), RelocInfo::OFF_HEAP_TARGET));
      Call(ip);
    } else {
      Handle<Code> code_target =
          isolate()->builtins()->code_handle(builtin_index);
      Call(code_target, RelocInfo::CODE_TARGET);
    }
  }
}

// Will clobber 4 registers: object, address, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(Register object, Register slot_address,
                                 Register value, LinkRegisterStatus lr_status,
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  DCHECK(!AreAliased(object, value, slot_address));
  if (FLAG_debug_code) {
    LoadTaggedPointerField(r0, MemOperand(slot_address));
    CmpS64(r0, value);
    Check(eq, AbortReason::kWrongAddressOrValuePassedToRecordWrite);
  }

  if ((remembered_set_action == RememberedSetAction::kOmit &&
       !FLAG_incremental_marking) ||
      FLAG_disable_write_barriers) {
    return;
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == SmiCheck::kInline) {
    JumpIfSmi(value, &done);
  }

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask, eq, &done);
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask, eq, &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    mflr(r0);
    push(r0);
  }
  CallRecordWriteStubSaveRegisters(object, slot_address, remembered_set_action,
                                   fp_mode);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(r0);
    mtlr(r0);
  }

  if (FLAG_debug_code) mov(slot_address, Operand(kZapValue));

  bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (FLAG_debug_code) {
    mov(slot_address, Operand(bit_cast<intptr_t>(kZapValue + 12)));
    mov(value, Operand(bit_cast<intptr_t>(kZapValue + 16)));
  }
}

void TurboAssembler::PushCommonFrame(Register marker_reg) {
  int fp_delta = 0;
  mflr(r0);
  if (FLAG_enable_embedded_constant_pool) {
    if (marker_reg.is_valid()) {
      Push(r0, fp, kConstantPoolRegister, marker_reg);
      fp_delta = 2;
    } else {
      Push(r0, fp, kConstantPoolRegister);
      fp_delta = 1;
    }
  } else {
    if (marker_reg.is_valid()) {
      Push(r0, fp, marker_reg);
      fp_delta = 1;
    } else {
      Push(r0, fp);
      fp_delta = 0;
    }
  }
  addi(fp, sp, Operand(fp_delta * kSystemPointerSize));
}

void TurboAssembler::PushStandardFrame(Register function_reg) {
  int fp_delta = 0;
  mflr(r0);
  if (FLAG_enable_embedded_constant_pool) {
    if (function_reg.is_valid()) {
      Push(r0, fp, kConstantPoolRegister, cp, function_reg);
      fp_delta = 3;
    } else {
      Push(r0, fp, kConstantPoolRegister, cp);
      fp_delta = 2;
    }
  } else {
    if (function_reg.is_valid()) {
      Push(r0, fp, cp, function_reg);
      fp_delta = 2;
    } else {
      Push(r0, fp, cp);
      fp_delta = 1;
    }
  }
  addi(fp, sp, Operand(fp_delta * kSystemPointerSize));
  Push(kJavaScriptCallArgCountRegister);
}

void TurboAssembler::RestoreFrameStateForTailCall() {
  if (FLAG_enable_embedded_constant_pool) {
    LoadU64(kConstantPoolRegister,
            MemOperand(fp, StandardFrameConstants::kConstantPoolOffset));
    set_constant_pool_available(false);
  }
  LoadU64(r0, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  LoadU64(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  mtlr(r0);
}

void TurboAssembler::CanonicalizeNaN(const DoubleRegister dst,
                                     const DoubleRegister src) {
  // Turn potential sNaN into qNaN.
  fsub(dst, src, kDoubleRegZero);
}

void TurboAssembler::ConvertIntToDouble(Register src, DoubleRegister dst) {
  MovIntToDouble(dst, src, r0);
  fcfid(dst, dst);
}

void TurboAssembler::ConvertUnsignedIntToDouble(Register src,
                                                DoubleRegister dst) {
  MovUnsignedIntToDouble(dst, src, r0);
  fcfid(dst, dst);
}

void TurboAssembler::ConvertIntToFloat(Register src, DoubleRegister dst) {
  MovIntToDouble(dst, src, r0);
  fcfids(dst, dst);
}

void TurboAssembler::ConvertUnsignedIntToFloat(Register src,
                                               DoubleRegister dst) {
  MovUnsignedIntToDouble(dst, src, r0);
  fcfids(dst, dst);
}

#if V8_TARGET_ARCH_PPC64
void TurboAssembler::ConvertInt64ToDouble(Register src,
                                          DoubleRegister double_dst) {
  MovInt64ToDouble(double_dst, src);
  fcfid(double_dst, double_dst);
}

void TurboAssembler::ConvertUnsignedInt64ToFloat(Register src,
                                                 DoubleRegister double_dst) {
  MovInt64ToDouble(double_dst, src);
  fcfidus(double_dst, double_dst);
}

void TurboAssembler::ConvertUnsignedInt64ToDouble(Register src,
                                                  DoubleRegister double_dst) {
  MovInt64ToDouble(double_dst, src);
  fcfidu(double_dst, double_dst);
}

void TurboAssembler::ConvertInt64ToFloat(Register src,
                                         DoubleRegister double_dst) {
  MovInt64ToDouble(double_dst, src);
  fcfids(double_dst, double_dst);
}
#endif

void TurboAssembler::ConvertDoubleToInt64(const DoubleRegister double_input,
#if !V8_TARGET_ARCH_PPC64
                                          const Register dst_hi,
#endif
                                          const Register dst,
                                          const DoubleRegister double_dst,
                                          FPRoundingMode rounding_mode) {
  if (rounding_mode == kRoundToZero) {
    fctidz(double_dst, double_input);
  } else {
    SetRoundingMode(rounding_mode);
    fctid(double_dst, double_input);
    ResetRoundingMode();
  }

  MovDoubleToInt64(
#if !V8_TARGET_ARCH_PPC64
      dst_hi,
#endif
      dst, double_dst);
}

#if V8_TARGET_ARCH_PPC64
void TurboAssembler::ConvertDoubleToUnsignedInt64(
    const DoubleRegister double_input, const Register dst,
    const DoubleRegister double_dst, FPRoundingMode rounding_mode) {
  if (rounding_mode == kRoundToZero) {
    fctiduz(double_dst, double_input);
  } else {
    SetRoundingMode(rounding_mode);
    fctidu(double_dst, double_input);
    ResetRoundingMode();
  }

  MovDoubleToInt64(dst, double_dst);
}
#endif

#if !V8_TARGET_ARCH_PPC64
void TurboAssembler::ShiftLeftPair(Register dst_low, Register dst_high,
                                   Register src_low, Register src_high,
                                   Register scratch, Register shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_high, src_low));
  DCHECK(!AreAliased(dst_low, dst_high, shift));
  Label less_than_32;
  Label done;
  cmpi(shift, Operand(32));
  blt(&less_than_32);
  // If shift >= 32
  andi(scratch, shift, Operand(0x1F));
  ShiftLeftU32(dst_high, src_low, scratch);
  li(dst_low, Operand::Zero());
  b(&done);
  bind(&less_than_32);
  // If shift < 32
  subfic(scratch, shift, Operand(32));
  ShiftLeftU32(dst_high, src_high, shift);
  srw(scratch, src_low, scratch);
  orx(dst_high, dst_high, scratch);
  ShiftLeftU32(dst_low, src_low, shift);
  bind(&done);
}

void TurboAssembler::ShiftLeftPair(Register dst_low, Register dst_high,
                                   Register src_low, Register src_high,
                                   uint32_t shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_high, src_low));
  if (shift == 32) {
    Move(dst_high, src_low);
    li(dst_low, Operand::Zero());
  } else if (shift > 32) {
    shift &= 0x1F;
    ShiftLeftU32(dst_high, src_low, Operand(shift));
    li(dst_low, Operand::Zero());
  } else if (shift == 0) {
    Move(dst_low, src_low);
    Move(dst_high, src_high);
  } else {
    ShiftLeftU32(dst_high, src_high, Operand(shift));
    rlwimi(dst_high, src_low, shift, 32 - shift, 31);
    ShiftLeftU32(dst_low, src_low, Operand(shift));
  }
}

void TurboAssembler::ShiftRightPair(Register dst_low, Register dst_high,
                                    Register src_low, Register src_high,
                                    Register scratch, Register shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_high, src_low));
  DCHECK(!AreAliased(dst_low, dst_high, shift));
  Label less_than_32;
  Label done;
  cmpi(shift, Operand(32));
  blt(&less_than_32);
  // If shift >= 32
  andi(scratch, shift, Operand(0x1F));
  srw(dst_low, src_high, scratch);
  li(dst_high, Operand::Zero());
  b(&done);
  bind(&less_than_32);
  // If shift < 32
  subfic(scratch, shift, Operand(32));
  srw(dst_low, src_low, shift);
  ShiftLeftU32(scratch, src_high, scratch);
  orx(dst_low, dst_low, scratch);
  srw(dst_high, src_high, shift);
  bind(&done);
}

void TurboAssembler::ShiftRightPair(Register dst_low, Register dst_high,
                                    Register src_low, Register src_high,
                                    uint32_t shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_high, src_low));
  if (shift == 32) {
    Move(dst_low, src_high);
    li(dst_high, Operand::Zero());
  } else if (shift > 32) {
    shift &= 0x1F;
    srwi(dst_low, src_high, Operand(shift));
    li(dst_high, Operand::Zero());
  } else if (shift == 0) {
    Move(dst_low, src_low);
    Move(dst_high, src_high);
  } else {
    srwi(dst_low, src_low, Operand(shift));
    rlwimi(dst_low, src_high, 32 - shift, 0, shift - 1);
    srwi(dst_high, src_high, Operand(shift));
  }
}

void TurboAssembler::ShiftRightAlgPair(Register dst_low, Register dst_high,
                                       Register src_low, Register src_high,
                                       Register scratch, Register shift) {
  DCHECK(!AreAliased(dst_low, src_high, shift));
  DCHECK(!AreAliased(dst_high, src_low, shift));
  Label less_than_32;
  Label done;
  cmpi(shift, Operand(32));
  blt(&less_than_32);
  // If shift >= 32
  andi(scratch, shift, Operand(0x1F));
  sraw(dst_low, src_high, scratch);
  srawi(dst_high, src_high, 31);
  b(&done);
  bind(&less_than_32);
  // If shift < 32
  subfic(scratch, shift, Operand(32));
  srw(dst_low, src_low, shift);
  ShiftLeftU32(scratch, src_high, scratch);
  orx(dst_low, dst_low, scratch);
  sraw(dst_high, src_high, shift);
  bind(&done);
}

void TurboAssembler::ShiftRightAlgPair(Register dst_low, Register dst_high,
                                       Register src_low, Register src_high,
                                       uint32_t shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_high, src_low));
  if (shift == 32) {
    Move(dst_low, src_high);
    srawi(dst_high, src_high, 31);
  } else if (shift > 32) {
    shift &= 0x1F;
    srawi(dst_low, src_high, shift);
    srawi(dst_high, src_high, 31);
  } else if (shift == 0) {
    Move(dst_low, src_low);
    Move(dst_high, src_high);
  } else {
    srwi(dst_low, src_low, Operand(shift));
    rlwimi(dst_low, src_high, 32 - shift, 0, shift - 1);
    srawi(dst_high, src_high, shift);
  }
}
#endif

void TurboAssembler::LoadConstantPoolPointerRegisterFromCodeTargetAddress(
    Register code_target_address) {
  // Builtins do not use the constant pool (see is_constant_pool_available).
  STATIC_ASSERT(Code::kOnHeapBodyIsContiguous);

  lwz(r0, MemOperand(code_target_address,
                     Code::kInstructionSizeOffset - Code::kHeaderSize));
  lwz(kConstantPoolRegister,
      MemOperand(code_target_address,
                 Code::kConstantPoolOffsetOffset - Code::kHeaderSize));
  add(kConstantPoolRegister, kConstantPoolRegister, code_target_address);
  add(kConstantPoolRegister, kConstantPoolRegister, r0);
}

void TurboAssembler::LoadPC(Register dst) {
  b(4, SetLK);
  mflr(dst);
}

void TurboAssembler::ComputeCodeStartAddress(Register dst) {
  mflr(r0);
  LoadPC(dst);
  subi(dst, dst, Operand(pc_offset() - kInstrSize));
  mtlr(r0);
}

void TurboAssembler::LoadConstantPoolPointerRegister() {
  //
  // Builtins do not use the constant pool (see is_constant_pool_available).
  STATIC_ASSERT(Code::kOnHeapBodyIsContiguous);

  LoadPC(kConstantPoolRegister);
  int32_t delta = -pc_offset() + 4;
  add_label_offset(kConstantPoolRegister, kConstantPoolRegister,
                   ConstantPoolPosition(), delta);
}

void TurboAssembler::StubPrologue(StackFrame::Type type) {
  {
    ConstantPoolUnavailableScope constant_pool_unavailable(this);
    mov(r11, Operand(StackFrame::TypeToMarker(type)));
    PushCommonFrame(r11);
  }
  if (FLAG_enable_embedded_constant_pool) {
    LoadConstantPoolPointerRegister();
    set_constant_pool_available(true);
  }
}

void TurboAssembler::Prologue() {
  PushStandardFrame(r4);
  if (FLAG_enable_embedded_constant_pool) {
    // base contains prologue address
    LoadConstantPoolPointerRegister();
    set_constant_pool_available(true);
  }
}

void TurboAssembler::DropArguments(Register count, ArgumentsCountType type,
                                   ArgumentsCountMode mode) {
  int receiver_bytes =
      (mode == kCountExcludesReceiver) ? kSystemPointerSize : 0;
  switch (type) {
    case kCountIsInteger: {
      ShiftLeftU64(ip, count, Operand(kSystemPointerSizeLog2));
      add(sp, sp, ip);
      break;
    }
    case kCountIsSmi: {
      STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
      SmiToPtrArrayOffset(count, count);
      add(sp, sp, count);
      break;
    }
    case kCountIsBytes: {
      add(sp, sp, count);
      break;
    }
  }
  if (receiver_bytes != 0) {
    addi(sp, sp, Operand(receiver_bytes));
  }
}

void TurboAssembler::DropArgumentsAndPushNewReceiver(Register argc,
                                                     Register receiver,
                                                     ArgumentsCountType type,
                                                     ArgumentsCountMode mode) {
  DCHECK(!AreAliased(argc, receiver));
  if (mode == kCountExcludesReceiver) {
    // Drop arguments without receiver and override old receiver.
    DropArguments(argc, type, kCountIncludesReceiver);
    StoreU64(receiver, MemOperand(sp));
  } else {
    DropArguments(argc, type, mode);
    push(receiver);
  }
}

void TurboAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool_pointer_reg) {
  if (FLAG_enable_embedded_constant_pool && load_constant_pool_pointer_reg) {
    // Push type explicitly so we can leverage the constant pool.
    // This path cannot rely on ip containing code entry.
    PushCommonFrame();
    LoadConstantPoolPointerRegister();
    mov(ip, Operand(StackFrame::TypeToMarker(type)));
    push(ip);
  } else {
    mov(ip, Operand(StackFrame::TypeToMarker(type)));
    PushCommonFrame(ip);
  }
#if V8_ENABLE_WEBASSEMBLY
  if (type == StackFrame::WASM) Push(kWasmInstanceRegister);
#endif  // V8_ENABLE_WEBASSEMBLY
}

int TurboAssembler::LeaveFrame(StackFrame::Type type, int stack_adjustment) {
  ConstantPoolUnavailableScope constant_pool_unavailable(this);
  // r3: preserved
  // r4: preserved
  // r5: preserved

  // Drop the execution stack down to the frame pointer and restore
  // the caller's state.
  int frame_ends;
  LoadU64(r0, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  LoadU64(ip, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  if (FLAG_enable_embedded_constant_pool) {
    LoadU64(kConstantPoolRegister,
            MemOperand(fp, StandardFrameConstants::kConstantPoolOffset));
  }
  mtlr(r0);
  frame_ends = pc_offset();
  AddS64(sp, fp,
         Operand(StandardFrameConstants::kCallerSPOffset + stack_adjustment),
         r0);
  mr(fp, ip);
  return frame_ends;
}

// ExitFrame layout (probably wrongish.. needs updating)
//
//  SP -> previousSP
//        LK reserved
//        sp_on_exit (for debug?)
// oldSP->prev SP
//        LK
//        <parameters on stack>

// Prior to calling EnterExitFrame, we've got a bunch of parameters
// on the stack that we need to wrap a real frame around.. so first
// we reserve a slot for LK and push the previous SP which is captured
// in the fp register (r31)
// Then - we buy a new frame

void MacroAssembler::EnterExitFrame(bool save_doubles, int stack_space,
                                    StackFrame::Type frame_type) {
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);
  // Set up the frame structure on the stack.
  DCHECK_EQ(2 * kSystemPointerSize, ExitFrameConstants::kCallerSPDisplacement);
  DCHECK_EQ(1 * kSystemPointerSize, ExitFrameConstants::kCallerPCOffset);
  DCHECK_EQ(0 * kSystemPointerSize, ExitFrameConstants::kCallerFPOffset);
  DCHECK_GT(stack_space, 0);

  // This is an opportunity to build a frame to wrap
  // all of the pushes that have happened inside of V8
  // since we were called from C code

  mov(ip, Operand(StackFrame::TypeToMarker(frame_type)));
  PushCommonFrame(ip);
  // Reserve room for saved entry sp.
  subi(sp, fp, Operand(ExitFrameConstants::kFixedFrameSizeFromFp));

  if (FLAG_debug_code) {
    li(r8, Operand::Zero());
    StoreU64(r8, MemOperand(fp, ExitFrameConstants::kSPOffset));
  }
  if (FLAG_enable_embedded_constant_pool) {
    StoreU64(kConstantPoolRegister,
             MemOperand(fp, ExitFrameConstants::kConstantPoolOffset));
  }

  // Save the frame pointer and the context in top.
  Move(r8, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                     isolate()));
  StoreU64(fp, MemOperand(r8));
  Move(r8,
       ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  StoreU64(cp, MemOperand(r8));

  // Optionally save all volatile double registers.
  if (save_doubles) {
    MultiPushDoubles(kCallerSavedDoubles);
    // Note that d0 will be accessible at
    //   fp - ExitFrameConstants::kFrameSize -
    //   kNumCallerSavedDoubles * kDoubleSize,
    // since the sp slot and code slot were pushed after the fp.
  }

  AddS64(sp, sp, Operand(-stack_space * kSystemPointerSize));

  // Allocate and align the frame preparing for calling the runtime
  // function.
  const int frame_alignment = ActivationFrameAlignment();
  if (frame_alignment > kSystemPointerSize) {
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    ClearRightImm(sp, sp,
                  Operand(base::bits::WhichPowerOfTwo(frame_alignment)));
  }
  li(r0, Operand::Zero());
  StoreU64WithUpdate(
      r0, MemOperand(sp, -kNumRequiredStackFrameSlots * kSystemPointerSize));

  // Set the exit frame sp value to point just before the return address
  // location.
  AddS64(r8, sp, Operand((kStackFrameExtraParamSlot + 1) * kSystemPointerSize),
         r0);
  StoreU64(r8, MemOperand(fp, ExitFrameConstants::kSPOffset));
}

int TurboAssembler::ActivationFrameAlignment() {
#if !defined(USE_SIMULATOR)
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one PPC
  // platform for another PPC platform with a different alignment.
  return base::OS::ActivationFrameAlignment();
#else  // Simulated
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif
}

void MacroAssembler::LeaveExitFrame(bool save_doubles, Register argument_count,
                                    bool argument_count_is_length) {
  ConstantPoolUnavailableScope constant_pool_unavailable(this);
  // Optionally restore all double registers.
  if (save_doubles) {
    // Calculate the stack location of the saved doubles and restore them.
    const int kNumRegs = kNumCallerSavedDoubles;
    const int offset =
        (ExitFrameConstants::kFixedFrameSizeFromFp + kNumRegs * kDoubleSize);
    AddS64(r6, fp, Operand(-offset), r0);
    MultiPopDoubles(kCallerSavedDoubles, r6);
  }

  // Clear top frame.
  li(r6, Operand::Zero());
  Move(ip, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                     isolate()));
  StoreU64(r6, MemOperand(ip));

  // Restore current context from top and clear it in debug mode.
  Move(ip,
       ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  LoadU64(cp, MemOperand(ip));

#ifdef DEBUG
  mov(r6, Operand(Context::kInvalidContext));
  Move(ip,
       ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  StoreU64(r6, MemOperand(ip));
#endif

  // Tear down the exit frame, pop the arguments, and return.
  LeaveFrame(StackFrame::EXIT);

  if (argument_count.is_valid()) {
    if (!argument_count_is_length) {
      ShiftLeftU64(argument_count, argument_count,
                   Operand(kSystemPointerSizeLog2));
    }
    add(sp, sp, argument_count);
  }
}

void TurboAssembler::MovFromFloatResult(const DoubleRegister dst) {
  Move(dst, d1);
}

void TurboAssembler::MovFromFloatParameter(const DoubleRegister dst) {
  Move(dst, d1);
}

void MacroAssembler::LoadStackLimit(Register destination, StackLimitKind kind) {
  DCHECK(root_array_available());
  Isolate* isolate = this->isolate();
  ExternalReference limit =
      kind == StackLimitKind::kRealStackLimit
          ? ExternalReference::address_of_real_jslimit(isolate)
          : ExternalReference::address_of_jslimit(isolate);
  DCHECK(TurboAssembler::IsAddressableThroughRootRegister(isolate, limit));

  intptr_t offset =
      TurboAssembler::RootRegisterOffsetForExternalReference(isolate, limit);
  CHECK(is_int32(offset));
  LoadU64(destination, MemOperand(kRootRegister, offset), r0);
}

void MacroAssembler::StackOverflowCheck(Register num_args, Register scratch,
                                        Label* stack_overflow) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  LoadStackLimit(scratch, StackLimitKind::kRealStackLimit);
  // Make scratch the space we have left. The stack might already be overflowed
  // here which will cause scratch to become negative.
  sub(scratch, sp, scratch);
  // Check if the arguments will overflow the stack.
  ShiftLeftU64(r0, num_args, Operand(kSystemPointerSizeLog2));
  CmpS64(scratch, r0);
  ble(stack_overflow);  // Signed comparison.
}

void MacroAssembler::InvokePrologue(Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    Label* done, InvokeType type) {
  Label regular_invoke;

  //  r3: actual arguments count
  //  r4: function (passed through to callee)
  //  r5: expected arguments count

  DCHECK_EQ(actual_parameter_count, r3);
  DCHECK_EQ(expected_parameter_count, r5);

  // If the expected parameter count is equal to the adaptor sentinel, no need
  // to push undefined value as arguments.
  if (kDontAdaptArgumentsSentinel != 0) {
    mov(r0, Operand(kDontAdaptArgumentsSentinel));
    CmpS64(expected_parameter_count, r0);
    beq(&regular_invoke);
  }

  // If overapplication or if the actual argument count is equal to the
  // formal parameter count, no need to push extra undefined values.
  sub(expected_parameter_count, expected_parameter_count,
      actual_parameter_count, LeaveOE, SetRC);
  ble(&regular_invoke, cr0);

  Label stack_overflow;
  Register scratch = r7;
  StackOverflowCheck(expected_parameter_count, scratch, &stack_overflow);

  // Underapplication. Move the arguments already in the stack, including the
  // receiver and the return address.
  {
    Label copy, skip;
    Register src = r9, dest = r8;
    addi(src, sp, Operand(-kSystemPointerSize));
    ShiftLeftU64(r0, expected_parameter_count, Operand(kSystemPointerSizeLog2));
    sub(sp, sp, r0);
    // Update stack pointer.
    addi(dest, sp, Operand(-kSystemPointerSize));
    mr(r0, actual_parameter_count);
    cmpi(r0, Operand::Zero());
    ble(&skip);
    mtctr(r0);

    bind(&copy);
    LoadU64WithUpdate(r0, MemOperand(src, kSystemPointerSize));
    StoreU64WithUpdate(r0, MemOperand(dest, kSystemPointerSize));
    bdnz(&copy);
    bind(&skip);
  }

  // Fill remaining expected arguments with undefined values.
  LoadRoot(scratch, RootIndex::kUndefinedValue);
  {
    mtctr(expected_parameter_count);

    Label loop;
    bind(&loop);
    StoreU64WithUpdate(scratch, MemOperand(r8, kSystemPointerSize));
    bdnz(&loop);
  }
  b(&regular_invoke);

  bind(&stack_overflow);
  {
    FrameScope frame(
        this, has_frame() ? StackFrame::NO_FRAME_TYPE : StackFrame::INTERNAL);
    CallRuntime(Runtime::kThrowStackOverflow);
    bkpt(0);
  }

  bind(&regular_invoke);
}

void MacroAssembler::CheckDebugHook(Register fun, Register new_target,
                                    Register expected_parameter_count,
                                    Register actual_parameter_count) {
  Label skip_hook;

  ExternalReference debug_hook_active =
      ExternalReference::debug_hook_on_function_call_address(isolate());
  Move(r7, debug_hook_active);
  LoadU8(r7, MemOperand(r7), r0);
  extsb(r7, r7);
  CmpSmiLiteral(r7, Smi::zero(), r0);
  beq(&skip_hook);

  {
    // Load receiver to pass it later to DebugOnFunctionCall hook.
    LoadReceiver(r7, actual_parameter_count);
    FrameScope frame(
        this, has_frame() ? StackFrame::NO_FRAME_TYPE : StackFrame::INTERNAL);

    SmiTag(expected_parameter_count);
    Push(expected_parameter_count);

    SmiTag(actual_parameter_count);
    Push(actual_parameter_count);

    if (new_target.is_valid()) {
      Push(new_target);
    }
    Push(fun, fun, r7);
    CallRuntime(Runtime::kDebugOnFunctionCall);
    Pop(fun);
    if (new_target.is_valid()) {
      Pop(new_target);
    }

    Pop(actual_parameter_count);
    SmiUntag(actual_parameter_count);

    Pop(expected_parameter_count);
    SmiUntag(expected_parameter_count);
  }
  bind(&skip_hook);
}

void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        Register expected_parameter_count,
                                        Register actual_parameter_count,
                                        InvokeType type) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(type == InvokeType::kCall, has_frame());
  DCHECK_EQ(function, r4);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == r6);

  // On function call, call into the debugger if necessary.
  CheckDebugHook(function, new_target, expected_parameter_count,
                 actual_parameter_count);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(r6, RootIndex::kUndefinedValue);
  }

  Label done;
  InvokePrologue(expected_parameter_count, actual_parameter_count, &done, type);
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  Register code = kJavaScriptCallCodeStartRegister;
  LoadTaggedPointerField(
      code, FieldMemOperand(function, JSFunction::kCodeOffset), r0);
  switch (type) {
    case InvokeType::kCall:
      CallCodeObject(code);
      break;
    case InvokeType::kJump:
      JumpCodeObject(code);
      break;
  }

    // Continue here if InvokePrologue does handle the invocation due to
    // mismatched parameter counts.
    bind(&done);
}

void MacroAssembler::InvokeFunctionWithNewTarget(
    Register fun, Register new_target, Register actual_parameter_count,
    InvokeType type) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(type == InvokeType::kCall, has_frame());

  // Contract with called JS functions requires that function is passed in r4.
  DCHECK_EQ(fun, r4);

  Register expected_reg = r5;
  Register temp_reg = r7;

  LoadTaggedPointerField(
      temp_reg, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset), r0);
  LoadTaggedPointerField(cp, FieldMemOperand(r4, JSFunction::kContextOffset),
                         r0);
  LoadU16(expected_reg,
          FieldMemOperand(temp_reg,
                          SharedFunctionInfo::kFormalParameterCountOffset));

  InvokeFunctionCode(fun, new_target, expected_reg, actual_parameter_count,
                     type);
}

void MacroAssembler::InvokeFunction(Register function,
                                    Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    InvokeType type) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(type == InvokeType::kCall, has_frame());

  // Contract with called JS functions requires that function is passed in r4.
  DCHECK_EQ(function, r4);

  // Get the function and setup the context.
  LoadTaggedPointerField(cp, FieldMemOperand(r4, JSFunction::kContextOffset),
                         r0);

  InvokeFunctionCode(r4, no_reg, expected_parameter_count,
                     actual_parameter_count, type);
}

void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kSystemPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kSystemPointerSize);

  Push(Smi::zero());  // Padding.

  // Link the current handler as the next handler.
  // Preserve r4-r8.
  Move(r3,
       ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate()));
  LoadU64(r0, MemOperand(r3));
  push(r0);

  // Set this new handler as the current one.
  StoreU64(sp, MemOperand(r3));
}

void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kSystemPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  pop(r4);
  Move(ip,
       ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate()));
  StoreU64(r4, MemOperand(ip));

  Drop(1);  // Drop padding.
}

void MacroAssembler::CompareObjectType(Register object, Register map,
                                       Register type_reg, InstanceType type) {
  const Register temp = type_reg == no_reg ? r0 : type_reg;

  LoadMap(map, object);
  CompareInstanceType(map, temp, type);
}

void MacroAssembler::CompareInstanceType(Register map, Register type_reg,
                                         InstanceType type) {
  STATIC_ASSERT(Map::kInstanceTypeOffset < 4096);
  STATIC_ASSERT(LAST_TYPE <= 0xFFFF);
  lhz(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  cmpi(type_reg, Operand(type));
}

void MacroAssembler::CompareRange(Register value, unsigned lower_limit,
                                  unsigned higher_limit) {
  ASM_CODE_COMMENT(this);
  DCHECK_LT(lower_limit, higher_limit);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  if (lower_limit != 0) {
    mov(scratch, Operand(lower_limit));
    sub(scratch, value, scratch);
    cmpli(scratch, Operand(higher_limit - lower_limit));
  } else {
    mov(scratch, Operand(higher_limit));
    CmpU64(value, scratch);
  }
}

void MacroAssembler::CompareInstanceTypeRange(Register map, Register type_reg,
                                              InstanceType lower_limit,
                                              InstanceType higher_limit) {
  DCHECK_LT(lower_limit, higher_limit);
  LoadU16(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  CompareRange(type_reg, lower_limit, higher_limit);
}

void MacroAssembler::CompareRoot(Register obj, RootIndex index) {
  DCHECK(obj != r0);
  LoadRoot(r0, index);
  CmpS64(obj, r0);
}

void TurboAssembler::AddAndCheckForOverflow(Register dst, Register left,
                                            Register right,
                                            Register overflow_dst,
                                            Register scratch) {
  DCHECK(dst != overflow_dst);
  DCHECK(dst != scratch);
  DCHECK(overflow_dst != scratch);
  DCHECK(overflow_dst != left);
  DCHECK(overflow_dst != right);

  bool left_is_right = left == right;
  RCBit xorRC = left_is_right ? SetRC : LeaveRC;

  // C = A+B; C overflows if A/B have same sign and C has diff sign than A
  if (dst == left) {
    mr(scratch, left);                        // Preserve left.
    add(dst, left, right);                    // Left is overwritten.
    xor_(overflow_dst, dst, scratch, xorRC);  // Original left.
    if (!left_is_right) xor_(scratch, dst, right);
  } else if (dst == right) {
    mr(scratch, right);     // Preserve right.
    add(dst, left, right);  // Right is overwritten.
    xor_(overflow_dst, dst, left, xorRC);
    if (!left_is_right) xor_(scratch, dst, scratch);  // Original right.
  } else {
    add(dst, left, right);
    xor_(overflow_dst, dst, left, xorRC);
    if (!left_is_right) xor_(scratch, dst, right);
  }
  if (!left_is_right) and_(overflow_dst, scratch, overflow_dst, SetRC);
}

void TurboAssembler::AddAndCheckForOverflow(Register dst, Register left,
                                            intptr_t right,
                                            Register overflow_dst,
                                            Register scratch) {
  Register original_left = left;
  DCHECK(dst != overflow_dst);
  DCHECK(dst != scratch);
  DCHECK(overflow_dst != scratch);
  DCHECK(overflow_dst != left);

  // C = A+B; C overflows if A/B have same sign and C has diff sign than A
  if (dst == left) {
    // Preserve left.
    original_left = overflow_dst;
    mr(original_left, left);
  }
  AddS64(dst, left, Operand(right), scratch);
  xor_(overflow_dst, dst, original_left);
  if (right >= 0) {
    and_(overflow_dst, overflow_dst, dst, SetRC);
  } else {
    andc(overflow_dst, overflow_dst, dst, SetRC);
  }
}

void TurboAssembler::SubAndCheckForOverflow(Register dst, Register left,
                                            Register right,
                                            Register overflow_dst,
                                            Register scratch) {
  DCHECK(dst != overflow_dst);
  DCHECK(dst != scratch);
  DCHECK(overflow_dst != scratch);
  DCHECK(overflow_dst != left);
  DCHECK(overflow_dst != right);

  // C = A-B; C overflows if A/B have diff signs and C has diff sign than A
  if (dst == left) {
    mr(scratch, left);      // Preserve left.
    sub(dst, left, right);  // Left is overwritten.
    xor_(overflow_dst, dst, scratch);
    xor_(scratch, scratch, right);
    and_(overflow_dst, overflow_dst, scratch, SetRC);
  } else if (dst == right) {
    mr(scratch, right);     // Preserve right.
    sub(dst, left, right);  // Right is overwritten.
    xor_(overflow_dst, dst, left);
    xor_(scratch, left, scratch);
    and_(overflow_dst, overflow_dst, scratch, SetRC);
  } else {
    sub(dst, left, right);
    xor_(overflow_dst, dst, left);
    xor_(scratch, left, right);
    and_(overflow_dst, scratch, overflow_dst, SetRC);
  }
}

void TurboAssembler::MinF64(DoubleRegister dst, DoubleRegister lhs,
                            DoubleRegister rhs, DoubleRegister scratch) {
  Label check_zero, return_left, return_right, return_nan, done;
  fcmpu(lhs, rhs);
  bunordered(&return_nan);
  if (CpuFeatures::IsSupported(PPC_7_PLUS)) {
    xsmindp(dst, lhs, rhs);
    b(&done);
  }
  beq(&check_zero);
  ble(&return_left);
  b(&return_right);

  bind(&check_zero);
  fcmpu(lhs, kDoubleRegZero);
  /* left == right != 0. */
  bne(&return_left);
  /* At this point, both left and right are either 0 or -0. */
  /* Min: The algorithm is: -((-L) + (-R)), which in case of L and R */
  /* being different registers is most efficiently expressed */
  /* as -((-L) - R). */
  fneg(scratch, lhs);
  if (scratch == rhs) {
    fadd(dst, scratch, rhs);
  } else {
    fsub(dst, scratch, rhs);
  }
  fneg(dst, dst);
  b(&done);

  bind(&return_nan);
  /* If left or right are NaN, fadd propagates the appropriate one.*/
  fadd(dst, lhs, rhs);
  b(&done);

  bind(&return_right);
  if (rhs != dst) {
    fmr(dst, rhs);
  }
  b(&done);

  bind(&return_left);
  if (lhs != dst) {
    fmr(dst, lhs);
  }
  bind(&done);
}

void TurboAssembler::MaxF64(DoubleRegister dst, DoubleRegister lhs,
                            DoubleRegister rhs, DoubleRegister scratch) {
  Label check_zero, return_left, return_right, return_nan, done;
  fcmpu(lhs, rhs);
  bunordered(&return_nan);
  if (CpuFeatures::IsSupported(PPC_7_PLUS)) {
    xsmaxdp(dst, lhs, rhs);
    b(&done);
  }
  beq(&check_zero);
  bge(&return_left);
  b(&return_right);

  bind(&check_zero);
  fcmpu(lhs, kDoubleRegZero);
  /* left == right != 0. */
  bne(&return_left);
  /* At this point, both left and right are either 0 or -0. */
  fadd(dst, lhs, rhs);
  b(&done);

  bind(&return_nan);
  /* If left or right are NaN, fadd propagates the appropriate one.*/
  fadd(dst, lhs, rhs);
  b(&done);

  bind(&return_right);
  if (rhs != dst) {
    fmr(dst, rhs);
  }
  b(&done);

  bind(&return_left);
  if (lhs != dst) {
    fmr(dst, lhs);
  }
  bind(&done);
}

void MacroAssembler::JumpIfIsInRange(Register value, unsigned lower_limit,
                                     unsigned higher_limit,
                                     Label* on_in_range) {
  CompareRange(value, lower_limit, higher_limit);
  ble(on_in_range);
}

void TurboAssembler::TruncateDoubleToI(Isolate* isolate, Zone* zone,
                                       Register result,
                                       DoubleRegister double_input,
                                       StubCallMode stub_mode) {
  Label done;

  TryInlineTruncateDoubleToI(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  mflr(r0);
  push(r0);
  // Put input on stack.
  stfdu(double_input, MemOperand(sp, -kDoubleSize));

#if V8_ENABLE_WEBASSEMBLY
  if (stub_mode == StubCallMode::kCallWasmRuntimeStub) {
    Call(wasm::WasmCode::kDoubleToI, RelocInfo::WASM_STUB_CALL);
#else
  // For balance.
  if (false) {
#endif  // V8_ENABLE_WEBASSEMBLY
  } else {
    Call(BUILTIN_CODE(isolate, DoubleToI), RelocInfo::CODE_TARGET);
  }

  LoadU64(result, MemOperand(sp));
  addi(sp, sp, Operand(kDoubleSize));
  pop(r0);
  mtlr(r0);

  bind(&done);
}

void TurboAssembler::TryInlineTruncateDoubleToI(Register result,
                                                DoubleRegister double_input,
                                                Label* done) {
  DoubleRegister double_scratch = kScratchDoubleReg;
#if !V8_TARGET_ARCH_PPC64
  Register scratch = ip;
#endif

  ConvertDoubleToInt64(double_input,
#if !V8_TARGET_ARCH_PPC64
                       scratch,
#endif
                       result, double_scratch);

// Test for overflow
#if V8_TARGET_ARCH_PPC64
  TestIfInt32(result, r0);
#else
  TestIfInt32(scratch, result, r0);
#endif
  beq(done);
}

void MacroAssembler::CallRuntime(const Runtime::Function* f, int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // All parameters are on the stack.  r3 has the return value after call.

  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r3, Operand(num_arguments));
  Move(r4, ExternalReference::Create(f));
#if V8_TARGET_ARCH_PPC64
  Handle<Code> code =
      CodeFactory::CEntry(isolate(), f->result_size, save_doubles);
#else
  Handle<Code> code = CodeFactory::CEntry(isolate(), 1, save_doubles);
#endif
  Call(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    mov(r3, Operand(function->nargs));
  }
  JumpToExternalReference(ExternalReference::Create(fid));
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin,
                                             bool builtin_exit_frame) {
  Move(r4, builtin);
  Handle<Code> code = CodeFactory::CEntry(isolate(), 1, SaveFPRegsMode::kIgnore,
                                          ArgvMode::kStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::JumpToOffHeapInstructionStream(Address entry) {
  mov(kOffHeapTrampolineRegister, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
  Jump(kOffHeapTrampolineRegister);
}

void MacroAssembler::LoadWeakValue(Register out, Register in,
                                   Label* target_if_cleared) {
  CmpS32(in, Operand(kClearedWeakHeapObjectLower32), r0);
  beq(target_if_cleared);

  mov(r0, Operand(~kWeakHeapObjectMask));
  and_(out, in, r0);
}

void MacroAssembler::EmitIncrementCounter(StatsCounter* counter, int value,
                                          Register scratch1,
                                          Register scratch2) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    Move(scratch2, ExternalReference::Create(counter));
    lwz(scratch1, MemOperand(scratch2));
    addi(scratch1, scratch1, Operand(value));
    stw(scratch1, MemOperand(scratch2));
  }
}

void MacroAssembler::EmitDecrementCounter(StatsCounter* counter, int value,
                                          Register scratch1,
                                          Register scratch2) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    Move(scratch2, ExternalReference::Create(counter));
    lwz(scratch1, MemOperand(scratch2));
    subi(scratch1, scratch1, Operand(value));
    stw(scratch1, MemOperand(scratch2));
  }
}

void TurboAssembler::Assert(Condition cond, AbortReason reason, CRegister cr) {
  if (FLAG_debug_code) Check(cond, reason, cr);
}

void TurboAssembler::Check(Condition cond, AbortReason reason, CRegister cr) {
  Label L;
  b(cond, &L, cr);
  Abort(reason);
  // will not return here
  bind(&L);
}

void TurboAssembler::Abort(AbortReason reason) {
  Label abort_start;
  bind(&abort_start);
  if (FLAG_code_comments) {
    const char* msg = GetAbortReason(reason);
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  // Avoid emitting call to builtin if requested.
  if (trap_on_abort()) {
    stop();
    return;
  }

  if (should_abort_hard()) {
    // We don't care if we constructed a frame. Just pretend we did.
    FrameScope assume_frame(this, StackFrame::NO_FRAME_TYPE);
    mov(r3, Operand(static_cast<int>(reason)));
    PrepareCallCFunction(1, r4);
    CallCFunction(ExternalReference::abort_with_reason(), 1);
    return;
  }

  LoadSmiLiteral(r4, Smi::FromInt(static_cast<int>(reason)));

  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NO_FRAME_TYPE);
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  } else {
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  }
  // will not return here
}

void TurboAssembler::LoadMap(Register destination, Register object) {
  LoadTaggedPointerField(destination,
                         FieldMemOperand(object, HeapObject::kMapOffset), r0);
}

void MacroAssembler::LoadNativeContextSlot(Register dst, int index) {
  LoadMap(dst, cp);
  LoadTaggedPointerField(
      dst,
      FieldMemOperand(dst, Map::kConstructorOrBackPointerOrNativeContextOffset),
      r0);
  LoadTaggedPointerField(dst, MemOperand(dst, Context::SlotOffset(index)), r0);
}

void TurboAssembler::AssertNotSmi(Register object) {
  if (FLAG_debug_code) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
    Check(ne, AbortReason::kOperandIsASmi, cr0);
  }
}

void TurboAssembler::AssertSmi(Register object) {
  if (FLAG_debug_code) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
    Check(eq, AbortReason::kOperandIsNotASmi, cr0);
  }
}

void MacroAssembler::AssertConstructor(Register object) {
  if (FLAG_debug_code) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
    Check(ne, AbortReason::kOperandIsASmiAndNotAConstructor, cr0);
    push(object);
    LoadMap(object, object);
    lbz(object, FieldMemOperand(object, Map::kBitFieldOffset));
    andi(object, object, Operand(Map::Bits1::IsConstructorBit::kMask));
    pop(object);
    Check(ne, AbortReason::kOperandIsNotAConstructor, cr0);
  }
}

void MacroAssembler::AssertFunction(Register object) {
  if (FLAG_debug_code) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
    Check(ne, AbortReason::kOperandIsASmiAndNotAFunction, cr0);
    push(object);
    LoadMap(object, object);
    CompareInstanceTypeRange(object, object, FIRST_JS_FUNCTION_TYPE,
                             LAST_JS_FUNCTION_TYPE);
    pop(object);
    Check(le, AbortReason::kOperandIsNotAFunction);
  }
}

void MacroAssembler::AssertCallableFunction(Register object) {
  if (!FLAG_debug_code) return;
  ASM_CODE_COMMENT(this);
  STATIC_ASSERT(kSmiTag == 0);
  TestIfSmi(object, r0);
  Check(ne, AbortReason::kOperandIsASmiAndNotAFunction, cr0);
  push(object);
  LoadMap(object, object);
  CompareInstanceTypeRange(object, object, FIRST_CALLABLE_JS_FUNCTION_TYPE,
                           LAST_CALLABLE_JS_FUNCTION_TYPE);
  pop(object);
  Check(le, AbortReason::kOperandIsNotACallableFunction);
}

void MacroAssembler::AssertBoundFunction(Register object) {
  if (FLAG_debug_code) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
    Check(ne, AbortReason::kOperandIsASmiAndNotABoundFunction, cr0);
    push(object);
    CompareObjectType(object, object, object, JS_BOUND_FUNCTION_TYPE);
    pop(object);
    Check(eq, AbortReason::kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!FLAG_debug_code) return;
  TestIfSmi(object, r0);
  Check(ne, AbortReason::kOperandIsASmiAndNotAGeneratorObject, cr0);

  // Load map
  Register map = object;
  push(object);
  LoadMap(map, object);

  // Check if JSGeneratorObject
  Label do_check;
  Register instance_type = object;
  CompareInstanceType(map, instance_type, JS_GENERATOR_OBJECT_TYPE);
  beq(&do_check);

  // Check if JSAsyncFunctionObject (See MacroAssembler::CompareInstanceType)
  cmpi(instance_type, Operand(JS_ASYNC_FUNCTION_OBJECT_TYPE));
  beq(&do_check);

  // Check if JSAsyncGeneratorObject (See MacroAssembler::CompareInstanceType)
  cmpi(instance_type, Operand(JS_ASYNC_GENERATOR_OBJECT_TYPE));

  bind(&do_check);
  // Restore generator object to register and perform assertion
  pop(object);
  Check(eq, AbortReason::kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (FLAG_debug_code) {
    Label done_checking;
    AssertNotSmi(object);
    CompareRoot(object, RootIndex::kUndefinedValue);
    beq(&done_checking);
    LoadMap(scratch, object);
    CompareInstanceType(scratch, scratch, ALLOCATION_SITE_TYPE);
    Assert(eq, AbortReason::kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}

static const int kRegisterPassedArguments = 8;

int TurboAssembler::CalculateStackPassedWords(int num_reg_arguments,
                                              int num_double_arguments) {
  int stack_passed_words = 0;
  if (num_double_arguments > DoubleRegister::kNumRegisters) {
    stack_passed_words +=
        2 * (num_double_arguments - DoubleRegister::kNumRegisters);
  }
  // Up to 8 simple arguments are passed in registers r3..r10.
  if (num_reg_arguments > kRegisterPassedArguments) {
    stack_passed_words += num_reg_arguments - kRegisterPassedArguments;
  }
  return stack_passed_words;
}

void TurboAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          int num_double_arguments,
                                          Register scratch) {
  int frame_alignment = ActivationFrameAlignment();
  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);
  int stack_space = kNumRequiredStackFrameSlots;

  if (frame_alignment > kSystemPointerSize) {
    // Make stack end at alignment and make room for stack arguments
    // -- preserving original value of sp.
    mr(scratch, sp);
    AddS64(sp, sp, Operand(-(stack_passed_arguments + 1) * kSystemPointerSize),
           scratch);
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    ClearRightImm(sp, sp,
                  Operand(base::bits::WhichPowerOfTwo(frame_alignment)));
    StoreU64(scratch,
             MemOperand(sp, stack_passed_arguments * kSystemPointerSize));
  } else {
    // Make room for stack arguments
    stack_space += stack_passed_arguments;
  }

  // Allocate frame with required slots to make ABI work.
  li(r0, Operand::Zero());
  StoreU64WithUpdate(r0, MemOperand(sp, -stack_space * kSystemPointerSize));
}

void TurboAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          Register scratch) {
  PrepareCallCFunction(num_reg_arguments, 0, scratch);
}

void TurboAssembler::MovToFloatParameter(DoubleRegister src) { Move(d1, src); }

void TurboAssembler::MovToFloatResult(DoubleRegister src) { Move(d1, src); }

void TurboAssembler::MovToFloatParameters(DoubleRegister src1,
                                          DoubleRegister src2) {
  if (src2 == d1) {
    DCHECK(src1 != d2);
    Move(d2, src2);
    Move(d1, src1);
  } else {
    Move(d1, src1);
    Move(d2, src2);
  }
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments,
                                   bool has_function_descriptor) {
  Move(ip, function);
  CallCFunctionHelper(ip, num_reg_arguments, num_double_arguments,
                      has_function_descriptor);
}

void TurboAssembler::CallCFunction(Register function, int num_reg_arguments,
                                   int num_double_arguments,
                                   bool has_function_descriptor) {
  CallCFunctionHelper(function, num_reg_arguments, num_double_arguments,
                      has_function_descriptor);
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments,
                                   bool has_function_descriptor) {
  CallCFunction(function, num_arguments, 0, has_function_descriptor);
}

void TurboAssembler::CallCFunction(Register function, int num_arguments,
                                   bool has_function_descriptor) {
  CallCFunction(function, num_arguments, 0, has_function_descriptor);
}

void TurboAssembler::CallCFunctionHelper(Register function,
                                         int num_reg_arguments,
                                         int num_double_arguments,
                                         bool has_function_descriptor) {
  DCHECK_LE(num_reg_arguments + num_double_arguments, kMaxCParameters);
  DCHECK(has_frame());

  // Save the frame pointer and PC so that the stack layout remains iterable,
  // even without an ExitFrame which normally exists between JS and C frames.
  Register addr_scratch = r7;
  Register scratch = r8;
  Push(scratch);
  mflr(scratch);
  // See x64 code for reasoning about how to address the isolate data fields.
  if (root_array_available()) {
    LoadPC(r0);
    StoreU64(r0, MemOperand(kRootRegister,
                            IsolateData::fast_c_call_caller_pc_offset()));
    StoreU64(fp, MemOperand(kRootRegister,
                            IsolateData::fast_c_call_caller_fp_offset()));
  } else {
    DCHECK_NOT_NULL(isolate());
    Push(addr_scratch);

    Move(addr_scratch,
         ExternalReference::fast_c_call_caller_pc_address(isolate()));
    LoadPC(r0);
    StoreU64(r0, MemOperand(addr_scratch));
    Move(addr_scratch,
         ExternalReference::fast_c_call_caller_fp_address(isolate()));
    StoreU64(fp, MemOperand(addr_scratch));
    Pop(addr_scratch);
  }
  mtlr(scratch);
  Pop(scratch);

  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.
  Register dest = function;
  if (ABI_USES_FUNCTION_DESCRIPTORS && has_function_descriptor) {
    // AIX/PPC64BE Linux uses a function descriptor. When calling C code be
    // aware of this descriptor and pick up values from it
    LoadU64(ToRegister(ABI_TOC_REGISTER),
            MemOperand(function, kSystemPointerSize));
    LoadU64(ip, MemOperand(function, 0));
    dest = ip;
  } else if (ABI_CALL_VIA_IP) {
    // pLinux and Simualtor, not AIX
    Move(ip, function);
    dest = ip;
  }

  Call(dest);

  // We don't unset the PC; the FP is the source of truth.
  Register zero_scratch = r0;
  mov(zero_scratch, Operand::Zero());

  if (root_array_available()) {
    StoreU64(
        zero_scratch,
        MemOperand(kRootRegister, IsolateData::fast_c_call_caller_fp_offset()));
  } else {
    DCHECK_NOT_NULL(isolate());
    Push(addr_scratch);
    Move(addr_scratch,
         ExternalReference::fast_c_call_caller_fp_address(isolate()));
    StoreU64(zero_scratch, MemOperand(addr_scratch));
    Pop(addr_scratch);
  }

  // Remove frame bought in PrepareCallCFunction
  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);
  int stack_space = kNumRequiredStackFrameSlots + stack_passed_arguments;
  if (ActivationFrameAlignment() > kSystemPointerSize) {
    LoadU64(sp, MemOperand(sp, stack_space * kSystemPointerSize), r0);
  } else {
    AddS64(sp, sp, Operand(stack_space * kSystemPointerSize), r0);
  }
}

void TurboAssembler::CheckPageFlag(
    Register object,
    Register scratch,  // scratch may be same register as object
    int mask, Condition cc, Label* condition_met) {
  DCHECK(cc == ne || cc == eq);
  DCHECK(scratch != r0);
  ClearRightImm(scratch, object, Operand(kPageSizeBits));
  LoadU64(scratch, MemOperand(scratch, BasicMemoryChunk::kFlagsOffset), r0);

  mov(r0, Operand(mask));
  and_(r0, scratch, r0, SetRC);

  if (cc == ne) {
    bne(condition_met, cr0);
  }
  if (cc == eq) {
    beq(condition_met, cr0);
  }
}

void TurboAssembler::SetRoundingMode(FPRoundingMode RN) { mtfsfi(7, RN); }

void TurboAssembler::ResetRoundingMode() {
  mtfsfi(7, kRoundToNearest);  // reset (default is kRoundToNearest)
}

////////////////////////////////////////////////////////////////////////////////
//
// New MacroAssembler Interfaces added for PPC
//
////////////////////////////////////////////////////////////////////////////////
void TurboAssembler::LoadIntLiteral(Register dst, int value) {
  mov(dst, Operand(value));
}

void TurboAssembler::LoadSmiLiteral(Register dst, Smi smi) {
  mov(dst, Operand(smi));
}

void TurboAssembler::LoadDoubleLiteral(DoubleRegister result,
                                       base::Double value, Register scratch) {
  if (FLAG_enable_embedded_constant_pool && is_constant_pool_available() &&
      !(scratch == r0 && ConstantPoolAccessIsInOverflow())) {
    ConstantPoolEntry::Access access = ConstantPoolAddEntry(value);
    if (access == ConstantPoolEntry::OVERFLOWED) {
      addis(scratch, kConstantPoolRegister, Operand::Zero());
      lfd(result, MemOperand(scratch, 0));
    } else {
      lfd(result, MemOperand(kConstantPoolRegister, 0));
    }
    return;
  }

  // avoid gcc strict aliasing error using union cast
  union {
    uint64_t dval;
#if V8_TARGET_ARCH_PPC64
    intptr_t ival;
#else
    intptr_t ival[2];
#endif
  } litVal;

  litVal.dval = value.AsUint64();

#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    mov(scratch, Operand(litVal.ival));
    mtfprd(result, scratch);
    return;
  }
#endif

  addi(sp, sp, Operand(-kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  mov(scratch, Operand(litVal.ival));
  std(scratch, MemOperand(sp));
#else
  LoadIntLiteral(scratch, litVal.ival[0]);
  stw(scratch, MemOperand(sp, 0));
  LoadIntLiteral(scratch, litVal.ival[1]);
  stw(scratch, MemOperand(sp, 4));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(result, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovIntToDouble(DoubleRegister dst, Register src,
                                    Register scratch) {
// sign-extend src to 64-bit
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    mtfprwa(dst, src);
    return;
  }
#endif

  DCHECK(src != scratch);
  subi(sp, sp, Operand(kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  extsw(scratch, src);
  std(scratch, MemOperand(sp, 0));
#else
  srawi(scratch, src, 31);
  stw(scratch, MemOperand(sp, Register::kExponentOffset));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovUnsignedIntToDouble(DoubleRegister dst, Register src,
                                            Register scratch) {
// zero-extend src to 64-bit
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    mtfprwz(dst, src);
    return;
  }
#endif

  DCHECK(src != scratch);
  subi(sp, sp, Operand(kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  clrldi(scratch, src, Operand(32));
  std(scratch, MemOperand(sp, 0));
#else
  li(scratch, Operand::Zero());
  stw(scratch, MemOperand(sp, Register::kExponentOffset));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovInt64ToDouble(DoubleRegister dst,
#if !V8_TARGET_ARCH_PPC64
                                      Register src_hi,
#endif
                                      Register src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    mtfprd(dst, src);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  std(src, MemOperand(sp, 0));
#else
  stw(src_hi, MemOperand(sp, Register::kExponentOffset));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}

#if V8_TARGET_ARCH_PPC64
void TurboAssembler::MovInt64ComponentsToDouble(DoubleRegister dst,
                                                Register src_hi,
                                                Register src_lo,
                                                Register scratch) {
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    ShiftLeftU64(scratch, src_hi, Operand(32));
    rldimi(scratch, src_lo, 0, 32);
    mtfprd(dst, scratch);
    return;
  }

  subi(sp, sp, Operand(kDoubleSize));
  stw(src_hi, MemOperand(sp, Register::kExponentOffset));
  stw(src_lo, MemOperand(sp, Register::kMantissaOffset));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp));
  addi(sp, sp, Operand(kDoubleSize));
}
#endif

void TurboAssembler::InsertDoubleLow(DoubleRegister dst, Register src,
                                     Register scratch) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    mffprd(scratch, dst);
    rldimi(scratch, src, 0, 32);
    mtfprd(dst, scratch);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(dst, MemOperand(sp));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::InsertDoubleHigh(DoubleRegister dst, Register src,
                                      Register scratch) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    mffprd(scratch, dst);
    rldimi(scratch, src, 32, 0);
    mtfprd(dst, scratch);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(dst, MemOperand(sp));
  stw(src, MemOperand(sp, Register::kExponentOffset));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovDoubleLowToInt(Register dst, DoubleRegister src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    mffprwz(dst, src);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(src, MemOperand(sp));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lwz(dst, MemOperand(sp, Register::kMantissaOffset));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovDoubleHighToInt(Register dst, DoubleRegister src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    mffprd(dst, src);
    srdi(dst, dst, Operand(32));
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(src, MemOperand(sp));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lwz(dst, MemOperand(sp, Register::kExponentOffset));
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovDoubleToInt64(
#if !V8_TARGET_ARCH_PPC64
    Register dst_hi,
#endif
    Register dst, DoubleRegister src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    mffprd(dst, src);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(src, MemOperand(sp));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
#if V8_TARGET_ARCH_PPC64
  ld(dst, MemOperand(sp, 0));
#else
  lwz(dst_hi, MemOperand(sp, Register::kExponentOffset));
  lwz(dst, MemOperand(sp, Register::kMantissaOffset));
#endif
  addi(sp, sp, Operand(kDoubleSize));
}

void TurboAssembler::MovIntToFloat(DoubleRegister dst, Register src,
                                   Register scratch) {
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    ShiftLeftU64(scratch, src, Operand(32));
    mtfprd(dst, scratch);
    xscvspdpn(dst, dst);
    return;
  }
  subi(sp, sp, Operand(kFloatSize));
  stw(src, MemOperand(sp, 0));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfs(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kFloatSize));
}

void TurboAssembler::MovFloatToInt(Register dst, DoubleRegister src,
                                   DoubleRegister scratch) {
  if (CpuFeatures::IsSupported(PPC_8_PLUS)) {
    xscvdpspn(scratch, src);
    mffprwz(dst, scratch);
    return;
  }
  subi(sp, sp, Operand(kFloatSize));
  stfs(src, MemOperand(sp, 0));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lwz(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kFloatSize));
}

void TurboAssembler::AddS64(Register dst, Register src, Register value, OEBit s,
                            RCBit r) {
  add(dst, src, value, s, r);
}

void TurboAssembler::AddS64(Register dst, Register src, const Operand& value,
                            Register scratch, OEBit s, RCBit r) {
  if (is_int16(value.immediate()) && s == LeaveOE && r == LeaveRC) {
    addi(dst, src, value);
  } else {
    mov(scratch, value);
    add(dst, src, scratch, s, r);
  }
}

void TurboAssembler::SubS64(Register dst, Register src, Register value, OEBit s,
                            RCBit r) {
  sub(dst, src, value, s, r);
}

void TurboAssembler::SubS64(Register dst, Register src, const Operand& value,
                            Register scratch, OEBit s, RCBit r) {
  if (is_int16(value.immediate()) && s == LeaveOE && r == LeaveRC) {
    subi(dst, src, value);
  } else {
    mov(scratch, value);
    sub(dst, src, scratch, s, r);
  }
}

void TurboAssembler::AddS32(Register dst, Register src, Register value,
                            RCBit r) {
  AddS64(dst, src, value, LeaveOE, r);
  extsw(dst, dst, r);
}

void TurboAssembler::AddS32(Register dst, Register src, const Operand& value,
                            Register scratch, RCBit r) {
  AddS64(dst, src, value, scratch, LeaveOE, r);
  extsw(dst, dst, r);
}

void TurboAssembler::SubS32(Register dst, Register src, Register value,
                            RCBit r) {
  SubS64(dst, src, value, LeaveOE, r);
  extsw(dst, dst, r);
}

void TurboAssembler::SubS32(Register dst, Register src, const Operand& value,
                            Register scratch, RCBit r) {
  SubS64(dst, src, value, scratch, LeaveOE, r);
  extsw(dst, dst, r);
}

void TurboAssembler::MulS64(Register dst, Register src, const Operand& value,
                            Register scratch, OEBit s, RCBit r) {
  if (is_int16(value.immediate()) && s == LeaveOE && r == LeaveRC) {
    mulli(dst, src, value);
  } else {
    mov(scratch, value);
    mulld(dst, src, scratch, s, r);
  }
}

void TurboAssembler::MulS64(Register dst, Register src, Register value, OEBit s,
                            RCBit r) {
  mulld(dst, src, value, s, r);
}

void TurboAssembler::MulS32(Register dst, Register src, const Operand& value,
                            Register scratch, OEBit s, RCBit r) {
  MulS64(dst, src, value, scratch, s, r);
  extsw(dst, dst, r);
}

void TurboAssembler::MulS32(Register dst, Register src, Register value, OEBit s,
                            RCBit r) {
  MulS64(dst, src, value, s, r);
  extsw(dst, dst, r);
}

void TurboAssembler::DivS64(Register dst, Register src, Register value, OEBit s,
                            RCBit r) {
  divd(dst, src, value, s, r);
}

void TurboAssembler::DivU64(Register dst, Register src, Register value, OEBit s,
                            RCBit r) {
  divdu(dst, src, value, s, r);
}

void TurboAssembler::DivS32(Register dst, Register src, Register value, OEBit s,
                            RCBit r) {
  divw(dst, src, value, s, r);
  extsw(dst, dst);
}
void TurboAssembler::DivU32(Register dst, Register src, Register value, OEBit s,
                            RCBit r) {
  divwu(dst, src, value, s, r);
  ZeroExtWord32(dst, dst);
}

void TurboAssembler::ModS64(Register dst, Register src, Register value) {
  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    modsd(dst, src, value);
  } else {
    Register scratch = GetRegisterThatIsNotOneOf(dst, src, value);
    Push(scratch);
    divd(scratch, src, value);
    mulld(scratch, scratch, value);
    sub(dst, src, scratch);
    Pop(scratch);
  }
}

void TurboAssembler::ModU64(Register dst, Register src, Register value) {
  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    modud(dst, src, value);
  } else {
    Register scratch = GetRegisterThatIsNotOneOf(dst, src, value);
    Push(scratch);
    divdu(scratch, src, value);
    mulld(scratch, scratch, value);
    sub(dst, src, scratch);
    Pop(scratch);
  }
}

void TurboAssembler::ModS32(Register dst, Register src, Register value) {
  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    modsw(dst, src, value);
  } else {
    Register scratch = GetRegisterThatIsNotOneOf(dst, src, value);
    Push(scratch);
    divw(scratch, src, value);
    mullw(scratch, scratch, value);
    sub(dst, src, scratch);
    Pop(scratch);
  }
  extsw(dst, dst);
}
void TurboAssembler::ModU32(Register dst, Register src, Register value) {
  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    moduw(dst, src, value);
  } else {
    Register scratch = GetRegisterThatIsNotOneOf(dst, src, value);
    Push(scratch);
    divwu(scratch, src, value);
    mullw(scratch, scratch, value);
    sub(dst, src, scratch);
    Pop(scratch);
  }
  ZeroExtWord32(dst, dst);
}

void TurboAssembler::AndU64(Register dst, Register src, const Operand& value,
                            Register scratch, RCBit r) {
  if (is_uint16(value.immediate()) && r == SetRC) {
    andi(dst, src, value);
  } else {
    mov(scratch, value);
    and_(dst, src, scratch, r);
  }
}

void TurboAssembler::AndU64(Register dst, Register src, Register value,
                            RCBit r) {
  and_(dst, src, value, r);
}

void TurboAssembler::OrU64(Register dst, Register src, const Operand& value,
                           Register scratch, RCBit r) {
  if (is_int16(value.immediate()) && r == LeaveRC) {
    ori(dst, src, value);
  } else {
    mov(scratch, value);
    orx(dst, src, scratch, r);
  }
}

void TurboAssembler::OrU64(Register dst, Register src, Register value,
                           RCBit r) {
  orx(dst, src, value, r);
}

void TurboAssembler::XorU64(Register dst, Register src, const Operand& value,
                            Register scratch, RCBit r) {
  if (is_int16(value.immediate()) && r == LeaveRC) {
    xori(dst, src, value);
  } else {
    mov(scratch, value);
    xor_(dst, src, scratch, r);
  }
}

void TurboAssembler::XorU64(Register dst, Register src, Register value,
                            RCBit r) {
  xor_(dst, src, value, r);
}

void TurboAssembler::AndU32(Register dst, Register src, const Operand& value,
                            Register scratch, RCBit r) {
  AndU64(dst, src, value, scratch, r);
  extsw(dst, dst, r);
}

void TurboAssembler::AndU32(Register dst, Register src, Register value,
                            RCBit r) {
  AndU64(dst, src, value, r);
  extsw(dst, dst, r);
}

void TurboAssembler::OrU32(Register dst, Register src, const Operand& value,
                           Register scratch, RCBit r) {
  OrU64(dst, src, value, scratch, r);
  extsw(dst, dst, r);
}

void TurboAssembler::OrU32(Register dst, Register src, Register value,
                           RCBit r) {
  OrU64(dst, src, value, r);
  extsw(dst, dst, r);
}

void TurboAssembler::XorU32(Register dst, Register src, const Operand& value,
                            Register scratch, RCBit r) {
  XorU64(dst, src, value, scratch, r);
  extsw(dst, dst, r);
}

void TurboAssembler::XorU32(Register dst, Register src, Register value,
                            RCBit r) {
  XorU64(dst, src, value, r);
  extsw(dst, dst, r);
}

void TurboAssembler::ShiftLeftU64(Register dst, Register src,
                                  const Operand& value, RCBit r) {
  sldi(dst, src, value, r);
}

void TurboAssembler::ShiftRightU64(Register dst, Register src,
                                   const Operand& value, RCBit r) {
  srdi(dst, src, value, r);
}

void TurboAssembler::ShiftRightS64(Register dst, Register src,
                                   const Operand& value, RCBit r) {
  sradi(dst, src, value.immediate(), r);
}

void TurboAssembler::ShiftLeftU32(Register dst, Register src,
                                  const Operand& value, RCBit r) {
  slwi(dst, src, value, r);
}

void TurboAssembler::ShiftRightU32(Register dst, Register src,
                                   const Operand& value, RCBit r) {
  srwi(dst, src, value, r);
}

void TurboAssembler::ShiftRightS32(Register dst, Register src,
                                   const Operand& value, RCBit r) {
  srawi(dst, src, value.immediate(), r);
}

void TurboAssembler::ShiftLeftU64(Register dst, Register src, Register value,
                                  RCBit r) {
  sld(dst, src, value, r);
}

void TurboAssembler::ShiftRightU64(Register dst, Register src, Register value,
                                   RCBit r) {
  srd(dst, src, value, r);
}

void TurboAssembler::ShiftRightS64(Register dst, Register src, Register value,
                                   RCBit r) {
  srad(dst, src, value, r);
}

void TurboAssembler::ShiftLeftU32(Register dst, Register src, Register value,
                                  RCBit r) {
  slw(dst, src, value, r);
}

void TurboAssembler::ShiftRightU32(Register dst, Register src, Register value,
                                   RCBit r) {
  srw(dst, src, value, r);
}

void TurboAssembler::ShiftRightS32(Register dst, Register src, Register value,
                                   RCBit r) {
  sraw(dst, src, value, r);
}

void TurboAssembler::CmpS64(Register src1, Register src2, CRegister cr) {
  cmp(src1, src2, cr);
}

void TurboAssembler::CmpS64(Register src1, const Operand& src2,
                            Register scratch, CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_int16(value)) {
    cmpi(src1, src2, cr);
  } else {
    mov(scratch, src2);
    CmpS64(src1, scratch, cr);
  }
}

void TurboAssembler::CmpU64(Register src1, const Operand& src2,
                            Register scratch, CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_uint16(value)) {
    cmpli(src1, src2, cr);
  } else {
    mov(scratch, src2);
    CmpU64(src1, scratch, cr);
  }
}

void TurboAssembler::CmpU64(Register src1, Register src2, CRegister cr) {
  cmpl(src1, src2, cr);
}

void TurboAssembler::CmpS32(Register src1, const Operand& src2,
                            Register scratch, CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_int16(value)) {
    cmpwi(src1, src2, cr);
  } else {
    mov(scratch, src2);
    CmpS32(src1, scratch, cr);
  }
}

void TurboAssembler::CmpS32(Register src1, Register src2, CRegister cr) {
  cmpw(src1, src2, cr);
}

void TurboAssembler::CmpU32(Register src1, const Operand& src2,
                            Register scratch, CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_uint16(value)) {
    cmplwi(src1, src2, cr);
  } else {
    mov(scratch, src2);
    cmplw(src1, scratch, cr);
  }
}

void TurboAssembler::CmpU32(Register src1, Register src2, CRegister cr) {
  cmplw(src1, src2, cr);
}

void TurboAssembler::AddF64(DoubleRegister dst, DoubleRegister lhs,
                            DoubleRegister rhs, RCBit r) {
  fadd(dst, lhs, rhs, r);
}

void TurboAssembler::SubF64(DoubleRegister dst, DoubleRegister lhs,
                            DoubleRegister rhs, RCBit r) {
  fsub(dst, lhs, rhs, r);
}

void TurboAssembler::MulF64(DoubleRegister dst, DoubleRegister lhs,
                            DoubleRegister rhs, RCBit r) {
  fmul(dst, lhs, rhs, r);
}

void TurboAssembler::DivF64(DoubleRegister dst, DoubleRegister lhs,
                            DoubleRegister rhs, RCBit r) {
  fdiv(dst, lhs, rhs, r);
}

void TurboAssembler::AddF32(DoubleRegister dst, DoubleRegister lhs,
                            DoubleRegister rhs, RCBit r) {
  fadd(dst, lhs, rhs, r);
  frsp(dst, dst, r);
}

void TurboAssembler::SubF32(DoubleRegister dst, DoubleRegister lhs,
                            DoubleRegister rhs, RCBit r) {
  fsub(dst, lhs, rhs, r);
  frsp(dst, dst, r);
}

void TurboAssembler::MulF32(DoubleRegister dst, DoubleRegister lhs,
                            DoubleRegister rhs, RCBit r) {
  fmul(dst, lhs, rhs, r);
  frsp(dst, dst, r);
}

void TurboAssembler::DivF32(DoubleRegister dst, DoubleRegister lhs,
                            DoubleRegister rhs, RCBit r) {
  fdiv(dst, lhs, rhs, r);
  frsp(dst, dst, r);
}

void TurboAssembler::CopySignF64(DoubleRegister dst, DoubleRegister lhs,
                                 DoubleRegister rhs, RCBit r) {
  fcpsgn(dst, rhs, lhs, r);
}

void MacroAssembler::CmpSmiLiteral(Register src1, Smi smi, Register scratch,
                                   CRegister cr) {
#if defined(V8_COMPRESS_POINTERS) || defined(V8_31BIT_SMIS_ON_64BIT_ARCH)
  CmpS32(src1, Operand(smi), scratch, cr);
#else
  LoadSmiLiteral(scratch, smi);
  CmpS64(src1, scratch, cr);
#endif
}

void MacroAssembler::CmplSmiLiteral(Register src1, Smi smi, Register scratch,
                                    CRegister cr) {
#if defined(V8_COMPRESS_POINTERS) || defined(V8_31BIT_SMIS_ON_64BIT_ARCH)
  CmpU64(src1, Operand(smi), scratch, cr);
#else
  LoadSmiLiteral(scratch, smi);
  CmpU64(src1, scratch, cr);
#endif
}

void MacroAssembler::AddSmiLiteral(Register dst, Register src, Smi smi,
                                   Register scratch) {
#if defined(V8_COMPRESS_POINTERS) || defined(V8_31BIT_SMIS_ON_64BIT_ARCH)
  AddS64(dst, src, Operand(smi.ptr()), scratch);
#else
  LoadSmiLiteral(scratch, smi);
  add(dst, src, scratch);
#endif
}

void MacroAssembler::SubSmiLiteral(Register dst, Register src, Smi smi,
                                   Register scratch) {
#if defined(V8_COMPRESS_POINTERS) || defined(V8_31BIT_SMIS_ON_64BIT_ARCH)
  AddS64(dst, src, Operand(-(static_cast<intptr_t>(smi.ptr()))), scratch);
#else
  LoadSmiLiteral(scratch, smi);
  sub(dst, src, scratch);
#endif
}

void MacroAssembler::AndSmiLiteral(Register dst, Register src, Smi smi,
                                   Register scratch, RCBit rc) {
#if defined(V8_COMPRESS_POINTERS) || defined(V8_31BIT_SMIS_ON_64BIT_ARCH)
  AndU64(dst, src, Operand(smi), scratch, rc);
#else
  LoadSmiLiteral(scratch, smi);
  and_(dst, src, scratch, rc);
#endif
}

#define GenerateMemoryOperation(reg, mem, ri_op, rr_op) \
  {                                                     \
    int64_t offset = mem.offset();                      \
                                                        \
    if (mem.rb() == no_reg) {                           \
      if (!is_int16(offset)) {                          \
        /* cannot use d-form */                         \
        CHECK_NE(scratch, no_reg);                      \
        mov(scratch, Operand(offset));                  \
        rr_op(reg, MemOperand(mem.ra(), scratch));      \
      } else {                                          \
        ri_op(reg, mem);                                \
      }                                                 \
    } else {                                            \
      if (offset == 0) {                                \
        rr_op(reg, mem);                                \
      } else if (is_int16(offset)) {                    \
        CHECK_NE(scratch, no_reg);                      \
        addi(scratch, mem.rb(), Operand(offset));       \
        rr_op(reg, MemOperand(mem.ra(), scratch));      \
      } else {                                          \
        CHECK_NE(scratch, no_reg);                      \
        mov(scratch, Operand(offset));                  \
        add(scratch, scratch, mem.rb());                \
        rr_op(reg, MemOperand(mem.ra(), scratch));      \
      }                                                 \
    }                                                   \
  }

#define GenerateMemoryOperationWithAlign(reg, mem, ri_op, rr_op) \
  {                                                              \
    int64_t offset = mem.offset();                               \
    int misaligned = (offset & 3);                               \
                                                                 \
    if (mem.rb() == no_reg) {                                    \
      if (!is_int16(offset) || misaligned) {                     \
        /* cannot use d-form */                                  \
        CHECK_NE(scratch, no_reg);                               \
        mov(scratch, Operand(offset));                           \
        rr_op(reg, MemOperand(mem.ra(), scratch));               \
      } else {                                                   \
        ri_op(reg, mem);                                         \
      }                                                          \
    } else {                                                     \
      if (offset == 0) {                                         \
        rr_op(reg, mem);                                         \
      } else if (is_int16(offset)) {                             \
        CHECK_NE(scratch, no_reg);                               \
        addi(scratch, mem.rb(), Operand(offset));                \
        rr_op(reg, MemOperand(mem.ra(), scratch));               \
      } else {                                                   \
        CHECK_NE(scratch, no_reg);                               \
        mov(scratch, Operand(offset));                           \
        add(scratch, scratch, mem.rb());                         \
        rr_op(reg, MemOperand(mem.ra(), scratch));               \
      }                                                          \
    }                                                            \
  }

#define MEM_OP_WITH_ALIGN_LIST(V) \
  V(LoadU64, ld, ldx)             \
  V(LoadS32, lwa, lwax)           \
  V(StoreU64, std, stdx)          \
  V(StoreU64WithUpdate, stdu, stdux)

#define MEM_OP_WITH_ALIGN_FUNCTION(name, ri_op, rr_op)           \
  void TurboAssembler::name(Register reg, const MemOperand& mem, \
                            Register scratch) {                  \
    GenerateMemoryOperationWithAlign(reg, mem, ri_op, rr_op);    \
  }
MEM_OP_WITH_ALIGN_LIST(MEM_OP_WITH_ALIGN_FUNCTION)
#undef MEM_OP_WITH_ALIGN_LIST
#undef MEM_OP_WITH_ALIGN_FUNCTION

#define MEM_OP_LIST(V)                                 \
  V(LoadU32, Register, lwz, lwzx)                      \
  V(LoadS16, Register, lha, lhax)                      \
  V(LoadU16, Register, lhz, lhzx)                      \
  V(LoadU8, Register, lbz, lbzx)                       \
  V(StoreU32, Register, stw, stwx)                     \
  V(StoreU16, Register, sth, sthx)                     \
  V(StoreU8, Register, stb, stbx)                      \
  V(LoadF64, DoubleRegister, lfd, lfdx)                \
  V(LoadF32, DoubleRegister, lfs, lfsx)                \
  V(StoreF64, DoubleRegister, stfd, stfdx)             \
  V(StoreF32, DoubleRegister, stfs, stfsx)             \
  V(LoadU64WithUpdate, Register, ldu, ldux)            \
  V(LoadF64WithUpdate, DoubleRegister, lfdu, lfdux)    \
  V(LoadF32WithUpdate, DoubleRegister, lfsu, lfsux)    \
  V(StoreF64WithUpdate, DoubleRegister, stfdu, stfdux) \
  V(StoreF32WithUpdate, DoubleRegister, stfsu, stfsux)

#define MEM_OP_FUNCTION(name, result_t, ri_op, rr_op)            \
  void TurboAssembler::name(result_t reg, const MemOperand& mem, \
                            Register scratch) {                  \
    GenerateMemoryOperation(reg, mem, ri_op, rr_op);             \
  }
MEM_OP_LIST(MEM_OP_FUNCTION)
#undef MEM_OP_LIST
#undef MEM_OP_FUNCTION

void TurboAssembler::LoadS8(Register dst, const MemOperand& mem,
                            Register scratch) {
  LoadU8(dst, mem, scratch);
  extsb(dst, dst);
}

void TurboAssembler::LoadSimd128(Simd128Register src, const MemOperand& mem) {
  DCHECK(mem.rb().is_valid());
  lxvx(src, mem);
}

void TurboAssembler::StoreSimd128(Simd128Register src, const MemOperand& mem) {
  DCHECK(mem.rb().is_valid());
  stxvx(src, mem);
}

#define GenerateMemoryLEOperation(reg, mem, op)                \
  {                                                            \
    if (mem.offset() == 0) {                                   \
      if (mem.rb() != no_reg)                                  \
        op(reg, mem);                                          \
      else                                                     \
        op(reg, MemOperand(r0, mem.ra()));                     \
    } else if (is_int16(mem.offset())) {                       \
      if (mem.rb() != no_reg)                                  \
        addi(scratch, mem.rb(), Operand(mem.offset()));        \
      else                                                     \
        mov(scratch, Operand(mem.offset()));                   \
      op(reg, MemOperand(mem.ra(), scratch));                  \
    } else {                                                   \
      mov(scratch, Operand(mem.offset()));                     \
      if (mem.rb() != no_reg) add(scratch, scratch, mem.rb()); \
      op(reg, MemOperand(mem.ra(), scratch));                  \
    }                                                          \
  }

#define MEM_LE_OP_LIST(V) \
  V(LoadU64, ldbrx)       \
  V(LoadU32, lwbrx)       \
  V(LoadU16, lhbrx)       \
  V(StoreU64, stdbrx)     \
  V(StoreU32, stwbrx)     \
  V(StoreU16, sthbrx)

#ifdef V8_TARGET_BIG_ENDIAN
#define MEM_LE_OP_FUNCTION(name, op)                                 \
  void TurboAssembler::name##LE(Register reg, const MemOperand& mem, \
                                Register scratch) {                  \
    GenerateMemoryLEOperation(reg, mem, op);                         \
  }
#else
#define MEM_LE_OP_FUNCTION(name, op)                                 \
  void TurboAssembler::name##LE(Register reg, const MemOperand& mem, \
                                Register scratch) {                  \
    name(reg, mem, scratch);                                         \
  }
#endif

MEM_LE_OP_LIST(MEM_LE_OP_FUNCTION)
#undef MEM_LE_OP_FUNCTION
#undef MEM_LE_OP_LIST

void TurboAssembler::LoadS32LE(Register dst, const MemOperand& mem,
                               Register scratch) {
#ifdef V8_TARGET_BIG_ENDIAN
  LoadU32LE(dst, mem, scratch);
  extsw(dst, dst);
#else
  LoadS32(dst, mem, scratch);
#endif
}

void TurboAssembler::LoadS16LE(Register dst, const MemOperand& mem,
                               Register scratch) {
#ifdef V8_TARGET_BIG_ENDIAN
  LoadU16LE(dst, mem, scratch);
  extsh(dst, dst);
#else
  LoadS16(dst, mem, scratch);
#endif
}

void TurboAssembler::LoadF64LE(DoubleRegister dst, const MemOperand& mem,
                               Register scratch, Register scratch2) {
#ifdef V8_TARGET_BIG_ENDIAN
  LoadU64LE(scratch, mem, scratch2);
  push(scratch);
  LoadF64(dst, MemOperand(sp), scratch2);
  pop(scratch);
#else
  LoadF64(dst, mem, scratch);
#endif
}

void TurboAssembler::LoadF32LE(DoubleRegister dst, const MemOperand& mem,
                               Register scratch, Register scratch2) {
#ifdef V8_TARGET_BIG_ENDIAN
  LoadU32LE(scratch, mem, scratch2);
  push(scratch);
  LoadF32(dst, MemOperand(sp, 4), scratch2);
  pop(scratch);
#else
  LoadF32(dst, mem, scratch);
#endif
}

void TurboAssembler::StoreF64LE(DoubleRegister dst, const MemOperand& mem,
                                Register scratch, Register scratch2) {
#ifdef V8_TARGET_BIG_ENDIAN
  StoreF64(dst, mem, scratch2);
  LoadU64(scratch, mem, scratch2);
  StoreU64LE(scratch, mem, scratch2);
#else
  StoreF64(dst, mem, scratch);
#endif
}

void TurboAssembler::StoreF32LE(DoubleRegister dst, const MemOperand& mem,
                                Register scratch, Register scratch2) {
#ifdef V8_TARGET_BIG_ENDIAN
  StoreF32(dst, mem, scratch2);
  LoadU32(scratch, mem, scratch2);
  StoreU32LE(scratch, mem, scratch2);
#else
  StoreF32(dst, mem, scratch);
#endif
}

Register GetRegisterThatIsNotOneOf(Register reg1, Register reg2, Register reg3,
                                   Register reg4, Register reg5,
                                   Register reg6) {
  RegList regs = {reg1, reg2, reg3, reg4, reg5, reg6};

  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_general_registers(); ++i) {
    int code = config->GetAllocatableGeneralCode(i);
    Register candidate = Register::from_code(code);
    if (regs.has(candidate)) continue;
    return candidate;
  }
  UNREACHABLE();
}

void TurboAssembler::SwapP(Register src, Register dst, Register scratch) {
  if (src == dst) return;
  DCHECK(!AreAliased(src, dst, scratch));
  mr(scratch, src);
  mr(src, dst);
  mr(dst, scratch);
}

void TurboAssembler::SwapP(Register src, MemOperand dst, Register scratch) {
  if (dst.ra() != r0 && dst.ra().is_valid())
    DCHECK(!AreAliased(src, dst.ra(), scratch));
  if (dst.rb() != r0 && dst.rb().is_valid())
    DCHECK(!AreAliased(src, dst.rb(), scratch));
  DCHECK(!AreAliased(src, scratch));
  mr(scratch, src);
  LoadU64(src, dst, r0);
  StoreU64(scratch, dst, r0);
}

void TurboAssembler::SwapP(MemOperand src, MemOperand dst, Register scratch_0,
                           Register scratch_1) {
  if (src.ra() != r0 && src.ra().is_valid())
    DCHECK(!AreAliased(src.ra(), scratch_0, scratch_1));
  if (src.rb() != r0 && src.rb().is_valid())
    DCHECK(!AreAliased(src.rb(), scratch_0, scratch_1));
  if (dst.ra() != r0 && dst.ra().is_valid())
    DCHECK(!AreAliased(dst.ra(), scratch_0, scratch_1));
  if (dst.rb() != r0 && dst.rb().is_valid())
    DCHECK(!AreAliased(dst.rb(), scratch_0, scratch_1));
  DCHECK(!AreAliased(scratch_0, scratch_1));
  if (is_int16(src.offset()) || is_int16(dst.offset())) {
    if (!is_int16(src.offset())) {
      // swap operand
      MemOperand temp = src;
      src = dst;
      dst = temp;
    }
    LoadU64(scratch_1, dst, scratch_0);
    LoadU64(scratch_0, src);
    StoreU64(scratch_1, src);
    StoreU64(scratch_0, dst, scratch_1);
  } else {
    LoadU64(scratch_1, dst, scratch_0);
    push(scratch_1);
    LoadU64(scratch_0, src, scratch_1);
    StoreU64(scratch_0, dst, scratch_1);
    pop(scratch_1);
    StoreU64(scratch_1, src, scratch_0);
  }
}

void TurboAssembler::SwapFloat32(DoubleRegister src, DoubleRegister dst,
                                 DoubleRegister scratch) {
  if (src == dst) return;
  DCHECK(!AreAliased(src, dst, scratch));
  fmr(scratch, src);
  fmr(src, dst);
  fmr(dst, scratch);
}

void TurboAssembler::SwapFloat32(DoubleRegister src, MemOperand dst,
                                 DoubleRegister scratch) {
  DCHECK(!AreAliased(src, scratch));
  fmr(scratch, src);
  LoadF32(src, dst, r0);
  StoreF32(scratch, dst, r0);
}

void TurboAssembler::SwapFloat32(MemOperand src, MemOperand dst,
                                 DoubleRegister scratch_0,
                                 DoubleRegister scratch_1) {
  DCHECK(!AreAliased(scratch_0, scratch_1));
  LoadF32(scratch_0, src, r0);
  LoadF32(scratch_1, dst, r0);
  StoreF32(scratch_0, dst, r0);
  StoreF32(scratch_1, src, r0);
}

void TurboAssembler::SwapDouble(DoubleRegister src, DoubleRegister dst,
                                DoubleRegister scratch) {
  if (src == dst) return;
  DCHECK(!AreAliased(src, dst, scratch));
  fmr(scratch, src);
  fmr(src, dst);
  fmr(dst, scratch);
}

void TurboAssembler::SwapDouble(DoubleRegister src, MemOperand dst,
                                DoubleRegister scratch) {
  DCHECK(!AreAliased(src, scratch));
  fmr(scratch, src);
  LoadF64(src, dst, r0);
  StoreF64(scratch, dst, r0);
}

void TurboAssembler::SwapDouble(MemOperand src, MemOperand dst,
                                DoubleRegister scratch_0,
                                DoubleRegister scratch_1) {
  DCHECK(!AreAliased(scratch_0, scratch_1));
  LoadF64(scratch_0, src, r0);
  LoadF64(scratch_1, dst, r0);
  StoreF64(scratch_0, dst, r0);
  StoreF64(scratch_1, src, r0);
}

void TurboAssembler::SwapSimd128(Simd128Register src, Simd128Register dst,
                                 Simd128Register scratch) {
  if (src == dst) return;
  vor(scratch, src, src);
  vor(src, dst, dst);
  vor(dst, scratch, scratch);
}

void TurboAssembler::SwapSimd128(Simd128Register src, MemOperand dst,
                                 Simd128Register scratch) {
  DCHECK(src != scratch);
  // push v0, to be used as scratch
  addi(sp, sp, Operand(-kSimd128Size));
  StoreSimd128(v0, MemOperand(r0, sp));
  mov(ip, Operand(dst.offset()));
  LoadSimd128(v0, MemOperand(dst.ra(), ip));
  StoreSimd128(src, MemOperand(dst.ra(), ip));
  vor(src, v0, v0);
  // restore v0
  LoadSimd128(v0, MemOperand(r0, sp));
  addi(sp, sp, Operand(kSimd128Size));
}

void TurboAssembler::SwapSimd128(MemOperand src, MemOperand dst,
                                 Simd128Register scratch) {
  // push v0 and v1, to be used as scratch
  addi(sp, sp, Operand(2 * -kSimd128Size));
  StoreSimd128(v0, MemOperand(r0, sp));
  li(ip, Operand(kSimd128Size));
  StoreSimd128(v1, MemOperand(ip, sp));

  mov(ip, Operand(src.offset()));
  LoadSimd128(v0, MemOperand(src.ra(), ip));
  mov(ip, Operand(dst.offset()));
  LoadSimd128(v1, MemOperand(dst.ra(), ip));

  StoreSimd128(v0, MemOperand(dst.ra(), ip));
  mov(ip, Operand(src.offset()));
  StoreSimd128(v1, MemOperand(src.ra(), ip));

  // restore v0 and v1
  LoadSimd128(v0, MemOperand(r0, sp));
  li(ip, Operand(kSimd128Size));
  LoadSimd128(v1, MemOperand(ip, sp));
  addi(sp, sp, Operand(2 * kSimd128Size));
}

void TurboAssembler::ByteReverseU16(Register dst, Register val,
                                    Register scratch) {
  if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
    brh(dst, val);
    ZeroExtHalfWord(dst, dst);
    return;
  }
  rlwinm(scratch, val, 8, 16, 23);
  rlwinm(dst, val, 24, 24, 31);
  orx(dst, scratch, dst);
  ZeroExtHalfWord(dst, dst);
}

void TurboAssembler::ByteReverseU32(Register dst, Register val,
                                    Register scratch) {
  if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
    brw(dst, val);
    ZeroExtWord32(dst, dst);
    return;
  }
  rotlwi(scratch, val, 8);
  rlwimi(scratch, val, 24, 0, 7);
  rlwimi(scratch, val, 24, 16, 23);
  ZeroExtWord32(dst, scratch);
}

void TurboAssembler::ByteReverseU64(Register dst, Register val, Register) {
  if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
    brd(dst, val);
    return;
  }
  subi(sp, sp, Operand(kSystemPointerSize));
  std(val, MemOperand(sp));
  ldbrx(dst, MemOperand(r0, sp));
  addi(sp, sp, Operand(kSystemPointerSize));
}

void TurboAssembler::JumpIfEqual(Register x, int32_t y, Label* dest) {
  CmpS64(x, Operand(y), r0);
  beq(dest);
}

void TurboAssembler::JumpIfLessThan(Register x, int32_t y, Label* dest) {
  CmpS64(x, Operand(y), r0);
  blt(dest);
}

void TurboAssembler::LoadEntryFromBuiltinIndex(Register builtin_index) {
  STATIC_ASSERT(kSystemPointerSize == 8);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);

  // The builtin_index register contains the builtin index as a Smi.
  if (SmiValuesAre32Bits()) {
    ShiftRightS64(builtin_index, builtin_index,
                  Operand(kSmiShift - kSystemPointerSizeLog2));
  } else {
    DCHECK(SmiValuesAre31Bits());
    ShiftLeftU64(builtin_index, builtin_index,
                 Operand(kSystemPointerSizeLog2 - kSmiShift));
  }
  AddS64(builtin_index, builtin_index,
         Operand(IsolateData::builtin_entry_table_offset()));
  LoadU64(builtin_index, MemOperand(kRootRegister, builtin_index));
}

void TurboAssembler::CallBuiltinByIndex(Register builtin_index) {
  LoadEntryFromBuiltinIndex(builtin_index);
  Call(builtin_index);
}

void TurboAssembler::LoadEntryFromBuiltin(Builtin builtin,
                                          Register destination) {
  ASM_CODE_COMMENT(this);
  LoadU64(destination, EntryFromBuiltinAsOperand(builtin));
}

MemOperand TurboAssembler::EntryFromBuiltinAsOperand(Builtin builtin) {
  ASM_CODE_COMMENT(this);
  DCHECK(root_array_available());
  return MemOperand(kRootRegister,
                    IsolateData::BuiltinEntrySlotOffset(builtin));
}

void TurboAssembler::LoadCodeObjectEntry(Register destination,
                                         Register code_object) {
  // Code objects are called differently depending on whether we are generating
  // builtin code (which will later be embedded into the binary) or compiling
  // user JS code at runtime.
  // * Builtin code runs in --jitless mode and thus must not call into on-heap
  //   Code targets. Instead, we dispatch through the builtins entry table.
  // * Codegen at runtime does not have this restriction and we can use the
  //   shorter, branchless instruction sequence. The assumption here is that
  //   targets are usually generated code and not builtin Code objects.

  if (options().isolate_independent_code) {
    DCHECK(root_array_available());
    Label if_code_is_off_heap, out;

    Register scratch = r11;

    DCHECK(!AreAliased(destination, scratch));
    DCHECK(!AreAliased(code_object, scratch));

    // Check whether the Code object is an off-heap trampoline. If so, call its
    // (off-heap) entry point directly without going through the (on-heap)
    // trampoline.  Otherwise, just call the Code object as always.
    LoadS32(scratch, FieldMemOperand(code_object, Code::kFlagsOffset), r0);
    mov(r0, Operand(Code::IsOffHeapTrampoline::kMask));
    and_(r0, scratch, r0, SetRC);
    bne(&if_code_is_off_heap, cr0);

    // Not an off-heap trampoline, the entry point is at
    // Code::raw_instruction_start().
    addi(destination, code_object, Operand(Code::kHeaderSize - kHeapObjectTag));
    b(&out);

    // An off-heap trampoline, the entry point is loaded from the builtin entry
    // table.
    bind(&if_code_is_off_heap);
    LoadS32(scratch, FieldMemOperand(code_object, Code::kBuiltinIndexOffset),
            r0);
    ShiftLeftU64(destination, scratch, Operand(kSystemPointerSizeLog2));
    add(destination, destination, kRootRegister);
    LoadU64(destination,
            MemOperand(destination, IsolateData::builtin_entry_table_offset()),
            r0);

    bind(&out);
  } else {
    addi(destination, code_object, Operand(Code::kHeaderSize - kHeapObjectTag));
  }
}

void TurboAssembler::CallCodeObject(Register code_object) {
  LoadCodeObjectEntry(code_object, code_object);
  Call(code_object);
}

void TurboAssembler::JumpCodeObject(Register code_object, JumpMode jump_mode) {
  DCHECK_EQ(JumpMode::kJump, jump_mode);
  LoadCodeObjectEntry(code_object, code_object);
  Jump(code_object);
}

void TurboAssembler::StoreReturnAddressAndCall(Register target) {
  // This generates the final instruction sequence for calls to C functions
  // once an exit frame has been constructed.
  //
  // Note that this assumes the caller code (i.e. the Code object currently
  // being generated) is immovable or that the callee function cannot trigger
  // GC, since the callee function will return to it.

  static constexpr int after_call_offset = 5 * kInstrSize;
  Label start_call;
  Register dest = target;

  if (ABI_USES_FUNCTION_DESCRIPTORS) {
    // AIX/PPC64BE Linux uses a function descriptor. When calling C code be
    // aware of this descriptor and pick up values from it
    LoadU64(ToRegister(ABI_TOC_REGISTER),
            MemOperand(target, kSystemPointerSize));
    LoadU64(ip, MemOperand(target, 0));
    dest = ip;
  } else if (ABI_CALL_VIA_IP && dest != ip) {
    Move(ip, target);
    dest = ip;
  }

  LoadPC(r7);
  bind(&start_call);
  addi(r7, r7, Operand(after_call_offset));
  StoreU64(r7, MemOperand(sp, kStackFrameExtraParamSlot * kSystemPointerSize));
  Call(dest);

  DCHECK_EQ(after_call_offset - kInstrSize,
            SizeOfCodeGeneratedSince(&start_call));
}

void TurboAssembler::CallForDeoptimization(Builtin target, int, Label* exit,
                                           DeoptimizeKind kind, Label* ret,
                                           Label*) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  CHECK_LE(target, Builtins::kLastTier0);
  LoadU64(ip, MemOperand(kRootRegister,
                         IsolateData::BuiltinEntrySlotOffset(target)));
  Call(ip);
  DCHECK_EQ(SizeOfCodeGeneratedSince(exit),
            (kind == DeoptimizeKind::kLazy) ? Deoptimizer::kLazyDeoptExitSize
                                            : Deoptimizer::kEagerDeoptExitSize);
}

void TurboAssembler::ZeroExtByte(Register dst, Register src) {
  clrldi(dst, src, Operand(56));
}

void TurboAssembler::ZeroExtHalfWord(Register dst, Register src) {
  clrldi(dst, src, Operand(48));
}

void TurboAssembler::ZeroExtWord32(Register dst, Register src) {
  clrldi(dst, src, Operand(32));
}

void TurboAssembler::Trap() { stop(); }
void TurboAssembler::DebugBreak() { stop(); }

void TurboAssembler::Popcnt32(Register dst, Register src) { popcntw(dst, src); }

void TurboAssembler::Popcnt64(Register dst, Register src) { popcntd(dst, src); }

void TurboAssembler::CountLeadingZerosU32(Register dst, Register src, RCBit r) {
  cntlzw(dst, src, r);
}

void TurboAssembler::CountLeadingZerosU64(Register dst, Register src, RCBit r) {
  cntlzd(dst, src, r);
}

#define COUNT_TRAILING_ZEROES_SLOW(max_count, scratch1, scratch2) \
  Label loop, done;                                               \
  li(scratch1, Operand(max_count));                               \
  mtctr(scratch1);                                                \
  mr(scratch1, src);                                              \
  li(dst, Operand::Zero());                                       \
  bind(&loop); /* while ((src & 1) == 0) */                       \
  andi(scratch2, scratch1, Operand(1));                           \
  bne(&done, cr0);                                                \
  srdi(scratch1, scratch1, Operand(1)); /* src >>= 1;*/           \
  addi(dst, dst, Operand(1));           /* dst++ */               \
  bdnz(&loop);                                                    \
  bind(&done);
void TurboAssembler::CountTrailingZerosU32(Register dst, Register src,
                                           Register scratch1, Register scratch2,
                                           RCBit r) {
  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    cnttzw(dst, src, r);
  } else {
    COUNT_TRAILING_ZEROES_SLOW(32, scratch1, scratch2);
  }
}

void TurboAssembler::CountTrailingZerosU64(Register dst, Register src,
                                           Register scratch1, Register scratch2,
                                           RCBit r) {
  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    cnttzd(dst, src, r);
  } else {
    COUNT_TRAILING_ZEROES_SLOW(64, scratch1, scratch2);
  }
}
#undef COUNT_TRAILING_ZEROES_SLOW

void TurboAssembler::ClearByteU64(Register dst, int byte_idx) {
  CHECK(0 <= byte_idx && byte_idx <= 7);
  int shift = byte_idx*8;
  rldicl(dst, dst, shift, 8);
  rldicl(dst, dst, 64-shift, 0);
}

void TurboAssembler::ReverseBitsU64(Register dst, Register src,
                                    Register scratch1, Register scratch2) {
  ByteReverseU64(dst, src);
  for (int i = 0; i < 8; i++) {
    ReverseBitsInSingleByteU64(dst, dst, scratch1, scratch2, i);
  }
}

void TurboAssembler::ReverseBitsU32(Register dst, Register src,
                                    Register scratch1, Register scratch2) {
  ByteReverseU32(dst, src, scratch1);
  for (int i = 4; i < 8; i++) {
    ReverseBitsInSingleByteU64(dst, dst, scratch1, scratch2, i);
  }
}

// byte_idx=7 refers to least significant byte
void TurboAssembler::ReverseBitsInSingleByteU64(Register dst, Register src,
                                                Register scratch1,
                                                Register scratch2,
                                                int byte_idx) {
  CHECK(0 <= byte_idx && byte_idx <= 7);
  int j = byte_idx;
  // zero all bits of scratch1
  li(scratch2, Operand(0));
  for (int i = 0; i <= 7; i++) {
    // zero all bits of scratch1
    li(scratch1, Operand(0));
    // move bit (j+1)*8-i-1 of src to bit j*8+i of scratch1, erase bits
    // (j*8+i+1):end of scratch1
    int shift = 7 - (2*i);
    if (shift < 0) shift += 64;
    rldicr(scratch1, src, shift, j*8+i);
    // erase bits start:(j*8-1+i) of scratch1 (inclusive)
    rldicl(scratch1, scratch1, 0, j*8+i);
    // scratch2 = scratch2|scratch1
    orx(scratch2, scratch2, scratch1);
  }
  // clear jth byte of dst and insert jth byte of scratch2
  ClearByteU64(dst, j);
  orx(dst, dst, scratch2);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64

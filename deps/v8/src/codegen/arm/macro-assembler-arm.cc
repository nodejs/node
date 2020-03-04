// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>  // For LONG_MIN, LONG_MAX.

#if V8_TARGET_ARCH_ARM

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/base/utils/random-number-generator.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/callable.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/external-reference-table.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/register-configuration.h"
#include "src/debug/debug.h"
#include "src/execution/frames-inl.h"
#include "src/heap/heap-inl.h"  // For MemoryChunk.
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/numbers/double.h"
#include "src/objects/objects-inl.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/snapshot/snapshot.h"
#include "src/wasm/wasm-code-manager.h"

// Satisfy cpplint check, but don't include platform-specific header. It is
// included recursively via macro-assembler.h.
#if 0
#include "src/codegen/arm/macro-assembler-arm.h"
#endif

namespace v8 {
namespace internal {

int TurboAssembler::RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                                    Register exclusion1,
                                                    Register exclusion2,
                                                    Register exclusion3) const {
  int bytes = 0;
  RegList exclusions = 0;
  if (exclusion1 != no_reg) {
    exclusions |= exclusion1.bit();
    if (exclusion2 != no_reg) {
      exclusions |= exclusion2.bit();
      if (exclusion3 != no_reg) {
        exclusions |= exclusion3.bit();
      }
    }
  }

  RegList list = (kCallerSaved | lr.bit()) & ~exclusions;

  bytes += NumRegs(list) * kPointerSize;

  if (fp_mode == kSaveFPRegs) {
    bytes += DwVfpRegister::NumRegisters() * DwVfpRegister::kSizeInBytes;
  }

  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                    Register exclusion2, Register exclusion3) {
  int bytes = 0;
  RegList exclusions = 0;
  if (exclusion1 != no_reg) {
    exclusions |= exclusion1.bit();
    if (exclusion2 != no_reg) {
      exclusions |= exclusion2.bit();
      if (exclusion3 != no_reg) {
        exclusions |= exclusion3.bit();
      }
    }
  }

  RegList list = (kCallerSaved | lr.bit()) & ~exclusions;
  stm(db_w, sp, list);

  bytes += NumRegs(list) * kPointerSize;

  if (fp_mode == kSaveFPRegs) {
    SaveFPRegs(sp, lr);
    bytes += DwVfpRegister::NumRegisters() * DwVfpRegister::kSizeInBytes;
  }

  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                   Register exclusion2, Register exclusion3) {
  int bytes = 0;
  if (fp_mode == kSaveFPRegs) {
    RestoreFPRegs(sp, lr);
    bytes += DwVfpRegister::NumRegisters() * DwVfpRegister::kSizeInBytes;
  }

  RegList exclusions = 0;
  if (exclusion1 != no_reg) {
    exclusions |= exclusion1.bit();
    if (exclusion2 != no_reg) {
      exclusions |= exclusion2.bit();
      if (exclusion3 != no_reg) {
        exclusions |= exclusion3.bit();
      }
    }
  }

  RegList list = (kCallerSaved | lr.bit()) & ~exclusions;
  ldm(ia_w, sp, list);

  bytes += NumRegs(list) * kPointerSize;

  return bytes;
}

void TurboAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kBuiltinsConstantsTable));

  // The ldr call below could end up clobbering ip when the offset does not fit
  // into 12 bits (and thus needs to be loaded from the constant pool). In that
  // case, we need to be extra-careful and temporarily use another register as
  // the target.

  const uint32_t offset =
      FixedArray::kHeaderSize + constant_index * kPointerSize - kHeapObjectTag;
  const bool could_clobber_ip = !is_uint12(offset);

  Register reg = destination;
  if (could_clobber_ip) {
    Push(r7);
    reg = r7;
  }

  LoadRoot(reg, RootIndex::kBuiltinsConstantsTable);
  ldr(destination, MemOperand(reg, offset));

  if (could_clobber_ip) {
    DCHECK_EQ(reg, r7);
    Pop(r7);
  }
}

void TurboAssembler::LoadRootRelative(Register destination, int32_t offset) {
  ldr(destination, MemOperand(kRootRegister, offset));
}

void TurboAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  if (offset == 0) {
    Move(destination, kRootRegister);
  } else {
    add(destination, kRootRegister, Operand(offset));
  }
}

void TurboAssembler::Jump(Register target, Condition cond) { bx(target, cond); }

void TurboAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond) {
  mov(pc, Operand(target, rmode), LeaveCC, cond);
}

void TurboAssembler::Jump(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(static_cast<intptr_t>(target), rmode, cond);
}

void TurboAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code));
  DCHECK_IMPLIES(options().use_pc_relative_calls_and_jumps,
                 Builtins::IsIsolateIndependentBuiltin(*code));

  int builtin_index = Builtins::kNoBuiltinId;
  bool target_is_isolate_independent_builtin =
      isolate()->builtins()->IsBuiltinHandle(code, &builtin_index) &&
      Builtins::IsIsolateIndependent(builtin_index);

  if (options().use_pc_relative_calls_and_jumps &&
      target_is_isolate_independent_builtin) {
    int32_t code_target_index = AddCodeTarget(code);
    b(code_target_index * kInstrSize, cond, RelocInfo::RELATIVE_CODE_TARGET);
    return;
  } else if (root_array_available_ && options().isolate_independent_code) {
    // This branch is taken only for specific cctests, where we force isolate
    // creation at runtime. At this point, Code space isn't restricted to a
    // size s.t. pc-relative calls may be used.
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    int offset = code->builtin_index() * kSystemPointerSize +
                 IsolateData::builtin_entry_table_offset();
    ldr(scratch, MemOperand(kRootRegister, offset));
    Jump(scratch, cond);
    return;
  } else if (options().inline_offheap_trampolines &&
             target_is_isolate_independent_builtin) {
    // Inline the trampoline.
    RecordCommentForOffHeapTrampoline(builtin_index);
    EmbeddedData d = EmbeddedData::FromBlob();
    Address entry = d.InstructionStartOfBuiltin(builtin_index);
    // Use ip directly instead of using UseScratchRegisterScope, as we do not
    // preserve scratch registers across calls.
    mov(ip, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
    Jump(ip, cond);
    return;
  }

  // 'code' is always generated ARM code, never THUMB code
  Jump(static_cast<intptr_t>(code.address()), rmode, cond);
}

void TurboAssembler::Jump(const ExternalReference& reference) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Move(scratch, reference);
  Jump(scratch);
}

void TurboAssembler::Call(Register target, Condition cond) {
  // Block constant pool for the call instruction sequence.
  BlockConstPoolScope block_const_pool(this);
  blx(target, cond);
}

void TurboAssembler::Call(Address target, RelocInfo::Mode rmode, Condition cond,
                          TargetAddressStorageMode mode,
                          bool check_constant_pool) {
  // Check if we have to emit the constant pool before we block it.
  if (check_constant_pool) MaybeCheckConstPool();
  // Block constant pool for the call instruction sequence.
  BlockConstPoolScope block_const_pool(this);

  bool old_predictable_code_size = predictable_code_size();
  if (mode == NEVER_INLINE_TARGET_ADDRESS) {
    set_predictable_code_size(true);
  }

  // Use ip directly instead of using UseScratchRegisterScope, as we do not
  // preserve scratch registers across calls.

  // Call sequence on V7 or later may be :
  //  movw  ip, #... @ call address low 16
  //  movt  ip, #... @ call address high 16
  //  blx   ip
  //                      @ return address
  // Or for pre-V7 or values that may be back-patched
  // to avoid ICache flushes:
  //  ldr   ip, [pc, #...] @ call address
  //  blx   ip
  //                      @ return address

  mov(ip, Operand(target, rmode));
  blx(ip, cond);

  if (mode == NEVER_INLINE_TARGET_ADDRESS) {
    set_predictable_code_size(old_predictable_code_size);
  }
}

void TurboAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond, TargetAddressStorageMode mode,
                          bool check_constant_pool) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code));
  DCHECK_IMPLIES(options().use_pc_relative_calls_and_jumps,
                 Builtins::IsIsolateIndependentBuiltin(*code));

  int builtin_index = Builtins::kNoBuiltinId;
  bool target_is_isolate_independent_builtin =
      isolate()->builtins()->IsBuiltinHandle(code, &builtin_index) &&
      Builtins::IsIsolateIndependent(builtin_index);

  if (target_is_isolate_independent_builtin &&
      options().use_pc_relative_calls_and_jumps) {
    int32_t code_target_index = AddCodeTarget(code);
    bl(code_target_index * kInstrSize, cond, RelocInfo::RELATIVE_CODE_TARGET);
    return;
  } else if (root_array_available_ && options().isolate_independent_code) {
    // This branch is taken only for specific cctests, where we force isolate
    // creation at runtime. At this point, Code space isn't restricted to a
    // size s.t. pc-relative calls may be used.
    int offset = code->builtin_index() * kSystemPointerSize +
                 IsolateData::builtin_entry_table_offset();
    ldr(ip, MemOperand(kRootRegister, offset));
    Call(ip, cond);
    return;
  } else if (target_is_isolate_independent_builtin &&
             options().inline_offheap_trampolines) {
    // Inline the trampoline.
    CallBuiltin(builtin_index);
    return;
  }

  // 'code' is always generated ARM code, never THUMB code
  DCHECK(code->IsExecutable());
  Call(code.address(), rmode, cond, mode);
}

void TurboAssembler::LoadEntryFromBuiltinIndex(Register builtin_index) {
  STATIC_ASSERT(kSystemPointerSize == 4);
  STATIC_ASSERT(kSmiShiftSize == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);

  // The builtin_index register contains the builtin index as a Smi.
  // Untagging is folded into the indexing operand below.
  mov(builtin_index,
      Operand(builtin_index, LSL, kSystemPointerSizeLog2 - kSmiTagSize));
  add(builtin_index, builtin_index,
      Operand(IsolateData::builtin_entry_table_offset()));
  ldr(builtin_index, MemOperand(kRootRegister, builtin_index));
}

void TurboAssembler::CallBuiltinByIndex(Register builtin_index) {
  LoadEntryFromBuiltinIndex(builtin_index);
  Call(builtin_index);
}

void TurboAssembler::CallBuiltin(int builtin_index, Condition cond) {
  DCHECK(Builtins::IsBuiltinId(builtin_index));
  RecordCommentForOffHeapTrampoline(builtin_index);
  EmbeddedData d = EmbeddedData::FromBlob();
  Address entry = d.InstructionStartOfBuiltin(builtin_index);
  // Use ip directly instead of using UseScratchRegisterScope, as we do not
  // preserve scratch registers across calls.
  mov(ip, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
  Call(ip, cond);
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

    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();

    DCHECK(!AreAliased(destination, scratch));
    DCHECK(!AreAliased(code_object, scratch));

    // Check whether the Code object is an off-heap trampoline. If so, call its
    // (off-heap) entry point directly without going through the (on-heap)
    // trampoline.  Otherwise, just call the Code object as always.
    ldr(scratch, FieldMemOperand(code_object, Code::kFlagsOffset));
    tst(scratch, Operand(Code::IsOffHeapTrampoline::kMask));
    b(ne, &if_code_is_off_heap);

    // Not an off-heap trampoline, the entry point is at
    // Code::raw_instruction_start().
    add(destination, code_object, Operand(Code::kHeaderSize - kHeapObjectTag));
    jmp(&out);

    // An off-heap trampoline, the entry point is loaded from the builtin entry
    // table.
    bind(&if_code_is_off_heap);
    ldr(scratch, FieldMemOperand(code_object, Code::kBuiltinIndexOffset));
    lsl(destination, scratch, Operand(kSystemPointerSizeLog2));
    add(destination, destination, kRootRegister);
    ldr(destination,
        MemOperand(destination, IsolateData::builtin_entry_table_offset()));

    bind(&out);
  } else {
    add(destination, code_object, Operand(Code::kHeaderSize - kHeapObjectTag));
  }
}

void TurboAssembler::CallCodeObject(Register code_object) {
  LoadCodeObjectEntry(code_object, code_object);
  Call(code_object);
}

void TurboAssembler::JumpCodeObject(Register code_object) {
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

  // Compute the return address in lr to return to after the jump below. The pc
  // is already at '+ 8' from the current instruction; but return is after three
  // instructions, so add another 4 to pc to get the return address.
  Assembler::BlockConstPoolScope block_const_pool(this);
  add(lr, pc, Operand(4));
  str(lr, MemOperand(sp));
  Call(target);
}

void TurboAssembler::Ret(Condition cond) { bx(lr, cond); }

void TurboAssembler::Drop(int count, Condition cond) {
  if (count > 0) {
    add(sp, sp, Operand(count * kPointerSize), LeaveCC, cond);
  }
}

void TurboAssembler::Drop(Register count, Condition cond) {
  add(sp, sp, Operand(count, LSL, kPointerSizeLog2), LeaveCC, cond);
}

void TurboAssembler::Ret(int drop, Condition cond) {
  Drop(drop, cond);
  Ret(cond);
}

void TurboAssembler::Call(Label* target) { bl(target); }

void TurboAssembler::Push(Handle<HeapObject> handle) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  mov(scratch, Operand(handle));
  push(scratch);
}

void TurboAssembler::Push(Smi smi) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  mov(scratch, Operand(smi));
  push(scratch);
}

void TurboAssembler::Move(Register dst, Smi smi) { mov(dst, Operand(smi)); }

void TurboAssembler::Move(Register dst, Handle<HeapObject> value) {
  // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
  // non-isolate-independent code. In many cases it might be cheaper than
  // embedding the relocatable value.
  if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadConstant(dst, value);
    return;
  }
  mov(dst, Operand(value));
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
  if (dst != src) {
    mov(dst, src, LeaveCC, cond);
  }
}

void TurboAssembler::Move(SwVfpRegister dst, SwVfpRegister src,
                          Condition cond) {
  if (dst != src) {
    vmov(dst, src, cond);
  }
}

void TurboAssembler::Move(DwVfpRegister dst, DwVfpRegister src,
                          Condition cond) {
  if (dst != src) {
    vmov(dst, src, cond);
  }
}

void TurboAssembler::Move(QwNeonRegister dst, QwNeonRegister src) {
  if (dst != src) {
    vmov(dst, src);
  }
}

void TurboAssembler::MovePair(Register dst0, Register src0, Register dst1,
                              Register src1) {
  DCHECK_NE(dst0, dst1);
  if (dst0 != src1) {
    Move(dst0, src0);
    Move(dst1, src1);
  } else if (dst1 != src0) {
    // Swap the order of the moves to resolve the overlap.
    Move(dst1, src1);
    Move(dst0, src0);
  } else {
    // Worse case scenario, this is a swap.
    Swap(dst0, src0);
  }
}

void TurboAssembler::Swap(Register srcdst0, Register srcdst1) {
  DCHECK(srcdst0 != srcdst1);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  mov(scratch, srcdst0);
  mov(srcdst0, srcdst1);
  mov(srcdst1, scratch);
}

void TurboAssembler::Swap(DwVfpRegister srcdst0, DwVfpRegister srcdst1) {
  DCHECK(srcdst0 != srcdst1);
  DCHECK(VfpRegisterIsAvailable(srcdst0));
  DCHECK(VfpRegisterIsAvailable(srcdst1));

  if (CpuFeatures::IsSupported(NEON)) {
    vswp(srcdst0, srcdst1);
  } else {
    UseScratchRegisterScope temps(this);
    DwVfpRegister scratch = temps.AcquireD();
    vmov(scratch, srcdst0);
    vmov(srcdst0, srcdst1);
    vmov(srcdst1, scratch);
  }
}

void TurboAssembler::Swap(QwNeonRegister srcdst0, QwNeonRegister srcdst1) {
  DCHECK(srcdst0 != srcdst1);
  vswp(srcdst0, srcdst1);
}

void MacroAssembler::Mls(Register dst, Register src1, Register src2,
                         Register srcA, Condition cond) {
  if (CpuFeatures::IsSupported(ARMv7)) {
    CpuFeatureScope scope(this, ARMv7);
    mls(dst, src1, src2, srcA, cond);
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    DCHECK(srcA != scratch);
    mul(scratch, src1, src2, LeaveCC, cond);
    sub(dst, srcA, scratch, LeaveCC, cond);
  }
}

void MacroAssembler::And(Register dst, Register src1, const Operand& src2,
                         Condition cond) {
  if (!src2.IsRegister() && !src2.MustOutputRelocInfo(this) &&
      src2.immediate() == 0) {
    mov(dst, Operand::Zero(), LeaveCC, cond);
  } else if (!(src2.InstructionsRequired(this) == 1) &&
             !src2.MustOutputRelocInfo(this) &&
             CpuFeatures::IsSupported(ARMv7) &&
             base::bits::IsPowerOfTwo(src2.immediate() + 1)) {
    CpuFeatureScope scope(this, ARMv7);
    ubfx(dst, src1, 0,
         base::bits::WhichPowerOfTwo(static_cast<uint32_t>(src2.immediate()) +
                                     1),
         cond);
  } else {
    and_(dst, src1, src2, LeaveCC, cond);
  }
}

void MacroAssembler::Ubfx(Register dst, Register src1, int lsb, int width,
                          Condition cond) {
  DCHECK_LT(lsb, 32);
  if (!CpuFeatures::IsSupported(ARMv7) || predictable_code_size()) {
    int mask = (1u << (width + lsb)) - 1u - ((1u << lsb) - 1u);
    and_(dst, src1, Operand(mask), LeaveCC, cond);
    if (lsb != 0) {
      mov(dst, Operand(dst, LSR, lsb), LeaveCC, cond);
    }
  } else {
    CpuFeatureScope scope(this, ARMv7);
    ubfx(dst, src1, lsb, width, cond);
  }
}

void MacroAssembler::Sbfx(Register dst, Register src1, int lsb, int width,
                          Condition cond) {
  DCHECK_LT(lsb, 32);
  if (!CpuFeatures::IsSupported(ARMv7) || predictable_code_size()) {
    int mask = (1 << (width + lsb)) - 1 - ((1 << lsb) - 1);
    and_(dst, src1, Operand(mask), LeaveCC, cond);
    int shift_up = 32 - lsb - width;
    int shift_down = lsb + shift_up;
    if (shift_up != 0) {
      mov(dst, Operand(dst, LSL, shift_up), LeaveCC, cond);
    }
    if (shift_down != 0) {
      mov(dst, Operand(dst, ASR, shift_down), LeaveCC, cond);
    }
  } else {
    CpuFeatureScope scope(this, ARMv7);
    sbfx(dst, src1, lsb, width, cond);
  }
}

void TurboAssembler::Bfc(Register dst, Register src, int lsb, int width,
                         Condition cond) {
  DCHECK_LT(lsb, 32);
  if (!CpuFeatures::IsSupported(ARMv7) || predictable_code_size()) {
    int mask = (1 << (width + lsb)) - 1 - ((1 << lsb) - 1);
    bic(dst, src, Operand(mask));
  } else {
    CpuFeatureScope scope(this, ARMv7);
    Move(dst, src, cond);
    bfc(dst, lsb, width, cond);
  }
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index,
                              Condition cond) {
  ldr(destination,
      MemOperand(kRootRegister, RootRegisterOffsetForRootIndex(index)), cond);
}

void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value,
                                      LinkRegisterStatus lr_status,
                                      SaveFPRegsMode save_fp,
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kPointerSize.
  DCHECK(IsAligned(offset, kPointerSize));

  if (emit_debug_code()) {
    Label ok;
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    add(scratch, object, Operand(offset - kHeapObjectTag));
    tst(scratch, Operand(kPointerSize - 1));
    b(eq, &ok);
    stop();
    bind(&ok);
  }

  RecordWrite(object, Operand(offset - kHeapObjectTag), value, lr_status,
              save_fp, remembered_set_action, OMIT_SMI_CHECK);

  bind(&done);
}

void TurboAssembler::SaveRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  RegList regs = 0;
  for (int i = 0; i < Register::kNumRegisters; ++i) {
    if ((registers >> i) & 1u) {
      regs |= Register::from_code(i).bit();
    }
  }

  stm(db_w, sp, regs);
}

void TurboAssembler::RestoreRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  RegList regs = 0;
  for (int i = 0; i < Register::kNumRegisters; ++i) {
    if ((registers >> i) & 1u) {
      regs |= Register::from_code(i).bit();
    }
  }
  ldm(ia_w, sp, regs);
}

void TurboAssembler::CallEphemeronKeyBarrier(Register object, Operand offset,
                                             SaveFPRegsMode fp_mode) {
  EphemeronKeyBarrierDescriptor descriptor;
  RegList registers = descriptor.allocatable_registers();

  SaveRegisters(registers);

  Register object_parameter(
      descriptor.GetRegisterParameter(EphemeronKeyBarrierDescriptor::kObject));
  Register slot_parameter(descriptor.GetRegisterParameter(
      EphemeronKeyBarrierDescriptor::kSlotAddress));
  Register fp_mode_parameter(
      descriptor.GetRegisterParameter(EphemeronKeyBarrierDescriptor::kFPMode));

  MoveObjectAndSlot(object_parameter, slot_parameter, object, offset);
  Move(fp_mode_parameter, Smi::FromEnum(fp_mode));
  Call(isolate()->builtins()->builtin_handle(Builtins::kEphemeronKeyBarrier),
       RelocInfo::CODE_TARGET);
  RestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Operand offset, RememberedSetAction remembered_set_action,
    SaveFPRegsMode fp_mode) {
  CallRecordWriteStub(
      object, offset, remembered_set_action, fp_mode,
      isolate()->builtins()->builtin_handle(Builtins::kRecordWrite),
      kNullAddress);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Operand offset, RememberedSetAction remembered_set_action,
    SaveFPRegsMode fp_mode, Address wasm_target) {
  CallRecordWriteStub(object, offset, remembered_set_action, fp_mode,
                      Handle<Code>::null(), wasm_target);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Operand offset, RememberedSetAction remembered_set_action,
    SaveFPRegsMode fp_mode, Handle<Code> code_target, Address wasm_target) {
  DCHECK_NE(code_target.is_null(), wasm_target == kNullAddress);
  // TODO(albertnetymk): For now we ignore remembered_set_action and fp_mode,
  // i.e. always emit remember set and save FP registers in RecordWriteStub. If
  // large performance regression is observed, we should use these values to
  // avoid unnecessary work.

  RecordWriteDescriptor descriptor;
  RegList registers = descriptor.allocatable_registers();

  SaveRegisters(registers);

  Register object_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kObject));
  Register slot_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kSlot));
  Register remembered_set_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kRememberedSet));
  Register fp_mode_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kFPMode));

  MoveObjectAndSlot(object_parameter, slot_parameter, object, offset);

  Move(remembered_set_parameter, Smi::FromEnum(remembered_set_action));
  Move(fp_mode_parameter, Smi::FromEnum(fp_mode));
  if (code_target.is_null()) {
    Call(wasm_target, RelocInfo::WASM_STUB_CALL);
  } else {
    Call(code_target, RelocInfo::CODE_TARGET);
  }

  RestoreRegisters(registers);
}

void TurboAssembler::MoveObjectAndSlot(Register dst_object, Register dst_slot,
                                       Register object, Operand offset) {
  DCHECK_NE(dst_object, dst_slot);
  DCHECK(offset.IsRegister() || offset.IsImmediate());
  // If `offset` is a register, it cannot overlap with `object`.
  DCHECK_IMPLIES(offset.IsRegister(), offset.rm() != object);

  // If the slot register does not overlap with the object register, we can
  // overwrite it.
  if (dst_slot != object) {
    add(dst_slot, object, offset);
    Move(dst_object, object);
    return;
  }

  DCHECK_EQ(dst_slot, object);

  // If the destination object register does not overlap with the offset
  // register, we can overwrite it.
  if (!offset.IsRegister() || (offset.rm() != dst_object)) {
    Move(dst_object, dst_slot);
    add(dst_slot, dst_slot, offset);
    return;
  }

  DCHECK_EQ(dst_object, offset.rm());

  // We only have `dst_slot` and `dst_object` left as distinct registers so we
  // have to swap them. We write this as a add+sub sequence to avoid using a
  // scratch register.
  add(dst_slot, dst_slot, dst_object);
  sub(dst_object, dst_slot, dst_object);
}

// The register 'object' contains a heap object pointer. The heap object tag is
// shifted away. A scratch register also needs to be available.
void MacroAssembler::RecordWrite(Register object, Operand offset,
                                 Register value, LinkRegisterStatus lr_status,
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  DCHECK_NE(object, value);
  if (emit_debug_code()) {
    {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      add(scratch, object, offset);
      ldr(scratch, MemOperand(scratch));
      cmp(scratch, value);
    }
    Check(eq, AbortReason::kWrongAddressOrValuePassedToRecordWrite);
  }

  if ((remembered_set_action == OMIT_REMEMBERED_SET &&
       !FLAG_incremental_marking) ||
      FLAG_disable_write_barriers) {
    return;
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  CheckPageFlag(value, MemoryChunk::kPointersToHereAreInterestingMask, eq,
                &done);
  CheckPageFlag(object, MemoryChunk::kPointersFromHereAreInterestingMask, eq,
                &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    push(lr);
  }
  CallRecordWriteStub(object, offset, remembered_set_action, fp_mode);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(lr);
  }

  bind(&done);
}

void TurboAssembler::PushCommonFrame(Register marker_reg) {
  if (marker_reg.is_valid()) {
    if (marker_reg.code() > fp.code()) {
      stm(db_w, sp, fp.bit() | lr.bit());
      mov(fp, Operand(sp));
      Push(marker_reg);
    } else {
      stm(db_w, sp, marker_reg.bit() | fp.bit() | lr.bit());
      add(fp, sp, Operand(kPointerSize));
    }
  } else {
    stm(db_w, sp, fp.bit() | lr.bit());
    mov(fp, sp);
  }
}

void TurboAssembler::PushStandardFrame(Register function_reg) {
  DCHECK(!function_reg.is_valid() || function_reg.code() < cp.code());
  stm(db_w, sp,
      (function_reg.is_valid() ? function_reg.bit() : 0) | cp.bit() | fp.bit() |
          lr.bit());
  int offset = -StandardFrameConstants::kContextOffset;
  offset += function_reg.is_valid() ? kPointerSize : 0;
  add(fp, sp, Operand(offset));
}

int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // The registers are pushed starting with the highest encoding,
  // which means that lowest encodings are closest to the stack pointer.
  DCHECK(reg_code >= 0 && reg_code < kNumSafepointRegisters);
  return reg_code;
}

void TurboAssembler::VFPCanonicalizeNaN(const DwVfpRegister dst,
                                        const DwVfpRegister src,
                                        const Condition cond) {
  // Subtracting 0.0 preserves all inputs except for signalling NaNs, which
  // become quiet NaNs. We use vsub rather than vadd because vsub preserves -0.0
  // inputs: -0.0 + 0.0 = 0.0, but -0.0 - 0.0 = -0.0.
  vsub(dst, src, kDoubleRegZero, cond);
}

void TurboAssembler::VFPCompareAndSetFlags(const SwVfpRegister src1,
                                           const SwVfpRegister src2,
                                           const Condition cond) {
  // Compare and move FPSCR flags to the normal condition flags.
  VFPCompareAndLoadFlags(src1, src2, pc, cond);
}

void TurboAssembler::VFPCompareAndSetFlags(const SwVfpRegister src1,
                                           const float src2,
                                           const Condition cond) {
  // Compare and move FPSCR flags to the normal condition flags.
  VFPCompareAndLoadFlags(src1, src2, pc, cond);
}

void TurboAssembler::VFPCompareAndSetFlags(const DwVfpRegister src1,
                                           const DwVfpRegister src2,
                                           const Condition cond) {
  // Compare and move FPSCR flags to the normal condition flags.
  VFPCompareAndLoadFlags(src1, src2, pc, cond);
}

void TurboAssembler::VFPCompareAndSetFlags(const DwVfpRegister src1,
                                           const double src2,
                                           const Condition cond) {
  // Compare and move FPSCR flags to the normal condition flags.
  VFPCompareAndLoadFlags(src1, src2, pc, cond);
}

void TurboAssembler::VFPCompareAndLoadFlags(const SwVfpRegister src1,
                                            const SwVfpRegister src2,
                                            const Register fpscr_flags,
                                            const Condition cond) {
  // Compare and load FPSCR.
  vcmp(src1, src2, cond);
  vmrs(fpscr_flags, cond);
}

void TurboAssembler::VFPCompareAndLoadFlags(const SwVfpRegister src1,
                                            const float src2,
                                            const Register fpscr_flags,
                                            const Condition cond) {
  // Compare and load FPSCR.
  vcmp(src1, src2, cond);
  vmrs(fpscr_flags, cond);
}

void TurboAssembler::VFPCompareAndLoadFlags(const DwVfpRegister src1,
                                            const DwVfpRegister src2,
                                            const Register fpscr_flags,
                                            const Condition cond) {
  // Compare and load FPSCR.
  vcmp(src1, src2, cond);
  vmrs(fpscr_flags, cond);
}

void TurboAssembler::VFPCompareAndLoadFlags(const DwVfpRegister src1,
                                            const double src2,
                                            const Register fpscr_flags,
                                            const Condition cond) {
  // Compare and load FPSCR.
  vcmp(src1, src2, cond);
  vmrs(fpscr_flags, cond);
}

void TurboAssembler::VmovHigh(Register dst, DwVfpRegister src) {
  if (src.code() < 16) {
    const LowDwVfpRegister loc = LowDwVfpRegister::from_code(src.code());
    vmov(dst, loc.high());
  } else {
    vmov(NeonS32, dst, src, 1);
  }
}

void TurboAssembler::VmovHigh(DwVfpRegister dst, Register src) {
  if (dst.code() < 16) {
    const LowDwVfpRegister loc = LowDwVfpRegister::from_code(dst.code());
    vmov(loc.high(), src);
  } else {
    vmov(NeonS32, dst, 1, src);
  }
}

void TurboAssembler::VmovLow(Register dst, DwVfpRegister src) {
  if (src.code() < 16) {
    const LowDwVfpRegister loc = LowDwVfpRegister::from_code(src.code());
    vmov(dst, loc.low());
  } else {
    vmov(NeonS32, dst, src, 0);
  }
}

void TurboAssembler::VmovLow(DwVfpRegister dst, Register src) {
  if (dst.code() < 16) {
    const LowDwVfpRegister loc = LowDwVfpRegister::from_code(dst.code());
    vmov(loc.low(), src);
  } else {
    vmov(NeonS32, dst, 0, src);
  }
}

void TurboAssembler::VmovExtended(Register dst, int src_code) {
  DCHECK_LE(SwVfpRegister::kNumRegisters, src_code);
  DCHECK_GT(SwVfpRegister::kNumRegisters * 2, src_code);
  if (src_code & 0x1) {
    VmovHigh(dst, DwVfpRegister::from_code(src_code / 2));
  } else {
    VmovLow(dst, DwVfpRegister::from_code(src_code / 2));
  }
}

void TurboAssembler::VmovExtended(int dst_code, Register src) {
  DCHECK_LE(SwVfpRegister::kNumRegisters, dst_code);
  DCHECK_GT(SwVfpRegister::kNumRegisters * 2, dst_code);
  if (dst_code & 0x1) {
    VmovHigh(DwVfpRegister::from_code(dst_code / 2), src);
  } else {
    VmovLow(DwVfpRegister::from_code(dst_code / 2), src);
  }
}

void TurboAssembler::VmovExtended(int dst_code, int src_code) {
  if (src_code == dst_code) return;

  if (src_code < SwVfpRegister::kNumRegisters &&
      dst_code < SwVfpRegister::kNumRegisters) {
    // src and dst are both s-registers.
    vmov(SwVfpRegister::from_code(dst_code),
         SwVfpRegister::from_code(src_code));
    return;
  }
  DwVfpRegister dst_d_reg = DwVfpRegister::from_code(dst_code / 2);
  DwVfpRegister src_d_reg = DwVfpRegister::from_code(src_code / 2);
  int dst_offset = dst_code & 1;
  int src_offset = src_code & 1;
  if (CpuFeatures::IsSupported(NEON)) {
    UseScratchRegisterScope temps(this);
    DwVfpRegister scratch = temps.AcquireD();
    // On Neon we can shift and insert from d-registers.
    if (src_offset == dst_offset) {
      // Offsets are the same, use vdup to copy the source to the opposite lane.
      vdup(Neon32, scratch, src_d_reg, src_offset);
      // Here we are extending the lifetime of scratch.
      src_d_reg = scratch;
      src_offset = dst_offset ^ 1;
    }
    if (dst_offset) {
      if (dst_d_reg == src_d_reg) {
        vdup(Neon32, dst_d_reg, src_d_reg, 0);
      } else {
        vsli(Neon64, dst_d_reg, src_d_reg, 32);
      }
    } else {
      if (dst_d_reg == src_d_reg) {
        vdup(Neon32, dst_d_reg, src_d_reg, 1);
      } else {
        vsri(Neon64, dst_d_reg, src_d_reg, 32);
      }
    }
    return;
  }

  // Without Neon, use the scratch registers to move src and/or dst into
  // s-registers.
  UseScratchRegisterScope temps(this);
  LowDwVfpRegister d_scratch = temps.AcquireLowD();
  LowDwVfpRegister d_scratch2 = temps.AcquireLowD();
  int s_scratch_code = d_scratch.low().code();
  int s_scratch_code2 = d_scratch2.low().code();
  if (src_code < SwVfpRegister::kNumRegisters) {
    // src is an s-register, dst is not.
    vmov(d_scratch, dst_d_reg);
    vmov(SwVfpRegister::from_code(s_scratch_code + dst_offset),
         SwVfpRegister::from_code(src_code));
    vmov(dst_d_reg, d_scratch);
  } else if (dst_code < SwVfpRegister::kNumRegisters) {
    // dst is an s-register, src is not.
    vmov(d_scratch, src_d_reg);
    vmov(SwVfpRegister::from_code(dst_code),
         SwVfpRegister::from_code(s_scratch_code + src_offset));
  } else {
    // Neither src or dst are s-registers. Both scratch double registers are
    // available when there are 32 VFP registers.
    vmov(d_scratch, src_d_reg);
    vmov(d_scratch2, dst_d_reg);
    vmov(SwVfpRegister::from_code(s_scratch_code + dst_offset),
         SwVfpRegister::from_code(s_scratch_code2 + src_offset));
    vmov(dst_d_reg, d_scratch2);
  }
}

void TurboAssembler::VmovExtended(int dst_code, const MemOperand& src) {
  if (dst_code < SwVfpRegister::kNumRegisters) {
    vldr(SwVfpRegister::from_code(dst_code), src);
  } else {
    UseScratchRegisterScope temps(this);
    LowDwVfpRegister scratch = temps.AcquireLowD();
    // TODO(bbudge) If Neon supported, use load single lane form of vld1.
    int dst_s_code = scratch.low().code() + (dst_code & 1);
    vmov(scratch, DwVfpRegister::from_code(dst_code / 2));
    vldr(SwVfpRegister::from_code(dst_s_code), src);
    vmov(DwVfpRegister::from_code(dst_code / 2), scratch);
  }
}

void TurboAssembler::VmovExtended(const MemOperand& dst, int src_code) {
  if (src_code < SwVfpRegister::kNumRegisters) {
    vstr(SwVfpRegister::from_code(src_code), dst);
  } else {
    // TODO(bbudge) If Neon supported, use store single lane form of vst1.
    UseScratchRegisterScope temps(this);
    LowDwVfpRegister scratch = temps.AcquireLowD();
    int src_s_code = scratch.low().code() + (src_code & 1);
    vmov(scratch, DwVfpRegister::from_code(src_code / 2));
    vstr(SwVfpRegister::from_code(src_s_code), dst);
  }
}

void TurboAssembler::ExtractLane(Register dst, QwNeonRegister src,
                                 NeonDataType dt, int lane) {
  int size = NeonSz(dt);  // 0, 1, 2
  int byte = lane << size;
  int double_word = byte >> kDoubleSizeLog2;
  int double_byte = byte & (kDoubleSize - 1);
  int double_lane = double_byte >> size;
  DwVfpRegister double_source =
      DwVfpRegister::from_code(src.code() * 2 + double_word);
  vmov(dt, dst, double_source, double_lane);
}

void TurboAssembler::ExtractLane(Register dst, DwVfpRegister src,
                                 NeonDataType dt, int lane) {
  int size = NeonSz(dt);  // 0, 1, 2
  int byte = lane << size;
  int double_byte = byte & (kDoubleSize - 1);
  int double_lane = double_byte >> size;
  vmov(dt, dst, src, double_lane);
}

void TurboAssembler::ExtractLane(SwVfpRegister dst, QwNeonRegister src,
                                 int lane) {
  int s_code = src.code() * 4 + lane;
  VmovExtended(dst.code(), s_code);
}

void TurboAssembler::ExtractLane(DwVfpRegister dst, QwNeonRegister src,
                                 int lane) {
  DwVfpRegister double_dst = DwVfpRegister::from_code(src.code() * 2 + lane);
  vmov(dst, double_dst);
}

void TurboAssembler::ReplaceLane(QwNeonRegister dst, QwNeonRegister src,
                                 Register src_lane, NeonDataType dt, int lane) {
  Move(dst, src);
  int size = NeonSz(dt);  // 0, 1, 2
  int byte = lane << size;
  int double_word = byte >> kDoubleSizeLog2;
  int double_byte = byte & (kDoubleSize - 1);
  int double_lane = double_byte >> size;
  DwVfpRegister double_dst =
      DwVfpRegister::from_code(dst.code() * 2 + double_word);
  vmov(dt, double_dst, double_lane, src_lane);
}

void TurboAssembler::ReplaceLane(QwNeonRegister dst, QwNeonRegister src,
                                 SwVfpRegister src_lane, int lane) {
  Move(dst, src);
  int s_code = dst.code() * 4 + lane;
  VmovExtended(s_code, src_lane.code());
}

void TurboAssembler::ReplaceLane(QwNeonRegister dst, QwNeonRegister src,
                                 DwVfpRegister src_lane, int lane) {
  Move(dst, src);
  DwVfpRegister double_dst = DwVfpRegister::from_code(dst.code() * 2 + lane);
  vmov(double_dst, src_lane);
}

void TurboAssembler::LslPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             Register shift) {
  DCHECK(!AreAliased(dst_high, src_low));
  DCHECK(!AreAliased(dst_high, shift));
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();

  Label less_than_32;
  Label done;
  rsb(scratch, shift, Operand(32), SetCC);
  b(gt, &less_than_32);
  // If shift >= 32
  and_(scratch, shift, Operand(0x1F));
  lsl(dst_high, src_low, Operand(scratch));
  mov(dst_low, Operand(0));
  jmp(&done);
  bind(&less_than_32);
  // If shift < 32
  lsl(dst_high, src_high, Operand(shift));
  orr(dst_high, dst_high, Operand(src_low, LSR, scratch));
  lsl(dst_low, src_low, Operand(shift));
  bind(&done);
}

void TurboAssembler::LslPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             uint32_t shift) {
  DCHECK_GE(63, shift);
  DCHECK(!AreAliased(dst_high, src_low));

  if (shift == 0) {
    Move(dst_high, src_high);
    Move(dst_low, src_low);
  } else if (shift == 32) {
    Move(dst_high, src_low);
    Move(dst_low, Operand(0));
  } else if (shift >= 32) {
    shift &= 0x1F;
    lsl(dst_high, src_low, Operand(shift));
    mov(dst_low, Operand(0));
  } else {
    lsl(dst_high, src_high, Operand(shift));
    orr(dst_high, dst_high, Operand(src_low, LSR, 32 - shift));
    lsl(dst_low, src_low, Operand(shift));
  }
}

void TurboAssembler::LsrPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             Register shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_low, shift));
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();

  Label less_than_32;
  Label done;
  rsb(scratch, shift, Operand(32), SetCC);
  b(gt, &less_than_32);
  // If shift >= 32
  and_(scratch, shift, Operand(0x1F));
  lsr(dst_low, src_high, Operand(scratch));
  mov(dst_high, Operand(0));
  jmp(&done);
  bind(&less_than_32);
  // If shift < 32

  lsr(dst_low, src_low, Operand(shift));
  orr(dst_low, dst_low, Operand(src_high, LSL, scratch));
  lsr(dst_high, src_high, Operand(shift));
  bind(&done);
}

void TurboAssembler::LsrPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             uint32_t shift) {
  DCHECK_GE(63, shift);
  DCHECK(!AreAliased(dst_low, src_high));

  if (shift == 32) {
    mov(dst_low, src_high);
    mov(dst_high, Operand(0));
  } else if (shift > 32) {
    shift &= 0x1F;
    lsr(dst_low, src_high, Operand(shift));
    mov(dst_high, Operand(0));
  } else if (shift == 0) {
    Move(dst_low, src_low);
    Move(dst_high, src_high);
  } else {
    lsr(dst_low, src_low, Operand(shift));
    orr(dst_low, dst_low, Operand(src_high, LSL, 32 - shift));
    lsr(dst_high, src_high, Operand(shift));
  }
}

void TurboAssembler::AsrPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             Register shift) {
  DCHECK(!AreAliased(dst_low, src_high));
  DCHECK(!AreAliased(dst_low, shift));
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();

  Label less_than_32;
  Label done;
  rsb(scratch, shift, Operand(32), SetCC);
  b(gt, &less_than_32);
  // If shift >= 32
  and_(scratch, shift, Operand(0x1F));
  asr(dst_low, src_high, Operand(scratch));
  asr(dst_high, src_high, Operand(31));
  jmp(&done);
  bind(&less_than_32);
  // If shift < 32
  lsr(dst_low, src_low, Operand(shift));
  orr(dst_low, dst_low, Operand(src_high, LSL, scratch));
  asr(dst_high, src_high, Operand(shift));
  bind(&done);
}

void TurboAssembler::AsrPair(Register dst_low, Register dst_high,
                             Register src_low, Register src_high,
                             uint32_t shift) {
  DCHECK_GE(63, shift);
  DCHECK(!AreAliased(dst_low, src_high));

  if (shift == 32) {
    mov(dst_low, src_high);
    asr(dst_high, src_high, Operand(31));
  } else if (shift > 32) {
    shift &= 0x1F;
    asr(dst_low, src_high, Operand(shift));
    asr(dst_high, src_high, Operand(31));
  } else if (shift == 0) {
    Move(dst_low, src_low);
    Move(dst_high, src_high);
  } else {
    lsr(dst_low, src_low, Operand(shift));
    orr(dst_low, dst_low, Operand(src_high, LSL, 32 - shift));
    asr(dst_high, src_high, Operand(shift));
  }
}

void TurboAssembler::StubPrologue(StackFrame::Type type) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  mov(scratch, Operand(StackFrame::TypeToMarker(type)));
  PushCommonFrame(scratch);
}

void TurboAssembler::Prologue() { PushStandardFrame(r1); }

void TurboAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool_pointer_reg) {
  // r0-r3: preserved
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  mov(scratch, Operand(StackFrame::TypeToMarker(type)));
  PushCommonFrame(scratch);
}

int TurboAssembler::LeaveFrame(StackFrame::Type type) {
  // r0: preserved
  // r1: preserved
  // r2: preserved

  // Drop the execution stack down to the frame pointer and restore
  // the caller frame pointer and return address.
  mov(sp, fp);
  int frame_ends = pc_offset();
  ldm(ia_w, sp, fp.bit() | lr.bit());
  return frame_ends;
}

#ifdef V8_OS_WIN
void TurboAssembler::AllocateStackSpace(Register bytes_scratch) {
  // "Functions that allocate 4 KB or more on the stack must ensure that each
  // page prior to the final page is touched in order." Source:
  // https://docs.microsoft.com/en-us/cpp/build/overview-of-arm-abi-conventions?view=vs-2019#stack
  UseScratchRegisterScope temps(this);
  DwVfpRegister scratch = temps.AcquireD();
  Label check_offset;
  Label touch_next_page;
  jmp(&check_offset);
  bind(&touch_next_page);
  sub(sp, sp, Operand(kStackPageSize));
  // Just to touch the page, before we increment further.
  vldr(scratch, MemOperand(sp));
  sub(bytes_scratch, bytes_scratch, Operand(kStackPageSize));

  bind(&check_offset);
  cmp(bytes_scratch, Operand(kStackPageSize));
  b(gt, &touch_next_page);

  sub(sp, sp, bytes_scratch);
}

void TurboAssembler::AllocateStackSpace(int bytes) {
  UseScratchRegisterScope temps(this);
  DwVfpRegister scratch = no_dreg;
  while (bytes > kStackPageSize) {
    if (scratch == no_dreg) {
      scratch = temps.AcquireD();
    }
    sub(sp, sp, Operand(kStackPageSize));
    vldr(scratch, MemOperand(sp));
    bytes -= kStackPageSize;
  }
  sub(sp, sp, Operand(bytes));
}
#endif

void MacroAssembler::EnterExitFrame(bool save_doubles, int stack_space,
                                    StackFrame::Type frame_type) {
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();

  // Set up the frame structure on the stack.
  DCHECK_EQ(2 * kPointerSize, ExitFrameConstants::kCallerSPDisplacement);
  DCHECK_EQ(1 * kPointerSize, ExitFrameConstants::kCallerPCOffset);
  DCHECK_EQ(0 * kPointerSize, ExitFrameConstants::kCallerFPOffset);
  mov(scratch, Operand(StackFrame::TypeToMarker(frame_type)));
  PushCommonFrame(scratch);
  // Reserve room for saved entry sp.
  sub(sp, fp, Operand(ExitFrameConstants::kFixedFrameSizeFromFp));
  if (emit_debug_code()) {
    mov(scratch, Operand::Zero());
    str(scratch, MemOperand(fp, ExitFrameConstants::kSPOffset));
  }

  // Save the frame pointer and the context in top.
  Move(scratch, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                          isolate()));
  str(fp, MemOperand(scratch));
  Move(scratch,
       ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  str(cp, MemOperand(scratch));

  // Optionally save all double registers.
  if (save_doubles) {
    SaveFPRegs(sp, scratch);
    // Note that d0 will be accessible at
    //   fp - ExitFrameConstants::kFrameSize -
    //   DwVfpRegister::kNumRegisters * kDoubleSize,
    // since the sp slot and code slot were pushed after the fp.
  }

  // Reserve place for the return address and stack space and align the frame
  // preparing for calling the runtime function.
  const int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  AllocateStackSpace((stack_space + 1) * kPointerSize);
  if (frame_alignment > 0) {
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    and_(sp, sp, Operand(-frame_alignment));
  }

  // Set the exit frame sp value to point just before the return address
  // location.
  add(scratch, sp, Operand(kPointerSize));
  str(scratch, MemOperand(fp, ExitFrameConstants::kSPOffset));
}

int TurboAssembler::ActivationFrameAlignment() {
#if V8_HOST_ARCH_ARM
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one ARM
  // platform for another ARM platform with a different alignment.
  return base::OS::ActivationFrameAlignment();
#else   // V8_HOST_ARCH_ARM
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif  // V8_HOST_ARCH_ARM
}

void MacroAssembler::LeaveExitFrame(bool save_doubles, Register argument_count,
                                    bool argument_count_is_length) {
  ConstantPoolUnavailableScope constant_pool_unavailable(this);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();

  // Optionally restore all double registers.
  if (save_doubles) {
    // Calculate the stack location of the saved doubles and restore them.
    const int offset = ExitFrameConstants::kFixedFrameSizeFromFp;
    sub(r3, fp, Operand(offset + DwVfpRegister::kNumRegisters * kDoubleSize));
    RestoreFPRegs(r3, scratch);
  }

  // Clear top frame.
  mov(r3, Operand::Zero());
  Move(scratch, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                          isolate()));
  str(r3, MemOperand(scratch));

  // Restore current context from top and clear it in debug mode.
  Move(scratch,
       ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  ldr(cp, MemOperand(scratch));
#ifdef DEBUG
  mov(r3, Operand(Context::kInvalidContext));
  Move(scratch,
       ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  str(r3, MemOperand(scratch));
#endif

  // Tear down the exit frame, pop the arguments, and return.
  mov(sp, Operand(fp));
  ldm(ia_w, sp, fp.bit() | lr.bit());
  if (argument_count.is_valid()) {
    if (argument_count_is_length) {
      add(sp, sp, argument_count);
    } else {
      add(sp, sp, Operand(argument_count, LSL, kPointerSizeLog2));
    }
  }
}

void TurboAssembler::MovFromFloatResult(const DwVfpRegister dst) {
  if (use_eabi_hardfloat()) {
    Move(dst, d0);
  } else {
    vmov(dst, r0, r1);
  }
}

// On ARM this is just a synonym to make the purpose clear.
void TurboAssembler::MovFromFloatParameter(DwVfpRegister dst) {
  MovFromFloatResult(dst);
}

void TurboAssembler::PrepareForTailCall(Register callee_args_count,
                                        Register caller_args_count,
                                        Register scratch0, Register scratch1) {
  DCHECK(!AreAliased(callee_args_count, caller_args_count, scratch0, scratch1));

  // Calculate the end of destination area where we will put the arguments
  // after we drop current frame. We add kPointerSize to count the receiver
  // argument which is not included into formal parameters count.
  Register dst_reg = scratch0;
  add(dst_reg, fp, Operand(caller_args_count, LSL, kPointerSizeLog2));
  add(dst_reg, dst_reg,
      Operand(StandardFrameConstants::kCallerSPOffset + kPointerSize));

  Register src_reg = caller_args_count;
  // Calculate the end of source area. +kPointerSize is for the receiver.
  add(src_reg, sp, Operand(callee_args_count, LSL, kPointerSizeLog2));
  add(src_reg, src_reg, Operand(kPointerSize));

  if (FLAG_debug_code) {
    cmp(src_reg, dst_reg);
    Check(lo, AbortReason::kStackAccessBelowStackPointer);
  }

  // Restore caller's frame pointer and return address now as they will be
  // overwritten by the copying loop.
  ldr(lr, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  ldr(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).

  // Both src_reg and dst_reg are pointing to the word after the one to copy,
  // so they must be pre-decremented in the loop.
  Register tmp_reg = scratch1;
  Label loop, entry;
  b(&entry);
  bind(&loop);
  ldr(tmp_reg, MemOperand(src_reg, -kPointerSize, PreIndex));
  str(tmp_reg, MemOperand(dst_reg, -kPointerSize, PreIndex));
  bind(&entry);
  cmp(sp, src_reg);
  b(ne, &loop);

  // Leave current frame.
  mov(sp, dst_reg);
}

void MacroAssembler::InvokePrologue(Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    Label* done, InvokeFlag flag) {
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. If not,
  // setup registers according to contract with ArgumentsAdaptorTrampoline:
  //  r0: actual arguments count
  //  r1: function (passed through to callee)
  //  r2: expected arguments count

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract.
  DCHECK_EQ(actual_parameter_count, r0);
  DCHECK_EQ(expected_parameter_count, r2);

  cmp(expected_parameter_count, actual_parameter_count);
  b(eq, &regular_invoke);

  Handle<Code> adaptor = BUILTIN_CODE(isolate(), ArgumentsAdaptorTrampoline);
  if (flag == CALL_FUNCTION) {
    Call(adaptor);
    b(done);
  } else {
    Jump(adaptor, RelocInfo::CODE_TARGET);
  }
    bind(&regular_invoke);
}

void MacroAssembler::CallDebugOnFunctionCall(Register fun, Register new_target,
                                             Register expected_parameter_count,
                                             Register actual_parameter_count) {
  // Load receiver to pass it later to DebugOnFunctionCall hook.
  ldr(r4, MemOperand(sp, actual_parameter_count, LSL, kPointerSizeLog2));
  FrameScope frame(this, has_frame() ? StackFrame::NONE : StackFrame::INTERNAL);

  SmiTag(expected_parameter_count);
  Push(expected_parameter_count);

  SmiTag(actual_parameter_count);
  Push(actual_parameter_count);

  if (new_target.is_valid()) {
    Push(new_target);
  }
  Push(fun);
  Push(fun);
  Push(r4);
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

void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        Register expected_parameter_count,
                                        Register actual_parameter_count,
                                        InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(flag == CALL_FUNCTION, has_frame());
  DCHECK_EQ(function, r1);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == r3);

  // On function call, call into the debugger if necessary.
  Label debug_hook, continue_after_hook;
  {
    ExternalReference debug_hook_active =
        ExternalReference::debug_hook_on_function_call_address(isolate());
    Move(r4, debug_hook_active);
    ldrsb(r4, MemOperand(r4));
    cmp(r4, Operand(0));
    b(ne, &debug_hook);
  }
  bind(&continue_after_hook);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(r3, RootIndex::kUndefinedValue);
  }

  Label done;
  InvokePrologue(expected_parameter_count, actual_parameter_count, &done, flag);
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  Register code = kJavaScriptCallCodeStartRegister;
  ldr(code, FieldMemOperand(function, JSFunction::kCodeOffset));
  if (flag == CALL_FUNCTION) {
    CallCodeObject(code);
  } else {
    DCHECK(flag == JUMP_FUNCTION);
    JumpCodeObject(code);
  }
  b(&done);

  // Deferred debug hook.
  bind(&debug_hook);
  CallDebugOnFunctionCall(function, new_target, expected_parameter_count,
                          actual_parameter_count);
  b(&continue_after_hook);

  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  bind(&done);
}

void MacroAssembler::InvokeFunctionWithNewTarget(
    Register fun, Register new_target, Register actual_parameter_count,
    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(flag == CALL_FUNCTION, has_frame());

  // Contract with called JS functions requires that function is passed in r1.
  DCHECK_EQ(fun, r1);

  Register expected_reg = r2;
  Register temp_reg = r4;

  ldr(temp_reg, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));
  ldrh(expected_reg,
       FieldMemOperand(temp_reg,
                       SharedFunctionInfo::kFormalParameterCountOffset));

  InvokeFunctionCode(fun, new_target, expected_reg, actual_parameter_count,
                     flag);
}

void MacroAssembler::InvokeFunction(Register function,
                                    Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(flag == CALL_FUNCTION, has_frame());

  // Contract with called JS functions requires that function is passed in r1.
  DCHECK_EQ(function, r1);

  // Get the function and setup the context.
  ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  InvokeFunctionCode(r1, no_reg, expected_parameter_count,
                     actual_parameter_count, flag);
}

void MacroAssembler::MaybeDropFrames() {
  // Check whether we need to drop frames to restart a function on the stack.
  ExternalReference restart_fp =
      ExternalReference::debug_restart_fp_address(isolate());
  Move(r1, restart_fp);
  ldr(r1, MemOperand(r1));
  tst(r1, r1);
  Jump(BUILTIN_CODE(isolate(), FrameDropperTrampoline), RelocInfo::CODE_TARGET,
       ne);
}

void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);

  Push(Smi::zero());  // Padding.
  // Link the current handler as the next handler.
  Move(r6,
       ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate()));
  ldr(r5, MemOperand(r6));
  push(r5);
  // Set this new handler as the current one.
  str(sp, MemOperand(r6));
}

void MacroAssembler::PopStackHandler() {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  pop(r1);
  Move(scratch,
       ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate()));
  str(r1, MemOperand(scratch));
  add(sp, sp, Operand(StackHandlerConstants::kSize - kPointerSize));
}

void MacroAssembler::CompareObjectType(Register object, Register map,
                                       Register type_reg, InstanceType type) {
  UseScratchRegisterScope temps(this);
  const Register temp = type_reg == no_reg ? temps.Acquire() : type_reg;

  LoadMap(map, object);
  CompareInstanceType(map, temp, type);
}

void MacroAssembler::CompareInstanceType(Register map, Register type_reg,
                                         InstanceType type) {
  ldrh(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  cmp(type_reg, Operand(type));
}

void MacroAssembler::CompareRoot(Register obj, RootIndex index) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  DCHECK(obj != scratch);
  LoadRoot(scratch, index);
  cmp(obj, scratch);
}

void MacroAssembler::JumpIfIsInRange(Register value, unsigned lower_limit,
                                     unsigned higher_limit,
                                     Label* on_in_range) {
  if (lower_limit != 0) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    sub(scratch, value, Operand(lower_limit));
    cmp(scratch, Operand(higher_limit - lower_limit));
  } else {
    cmp(value, Operand(higher_limit));
  }
  b(ls, on_in_range);
}

void TurboAssembler::TryInlineTruncateDoubleToI(Register result,
                                                DwVfpRegister double_input,
                                                Label* done) {
  UseScratchRegisterScope temps(this);
  SwVfpRegister single_scratch = SwVfpRegister::no_reg();
  if (temps.CanAcquireVfp<SwVfpRegister>()) {
    single_scratch = temps.AcquireS();
  } else {
    // Re-use the input as a scratch register. However, we can only do this if
    // the input register is d0-d15 as there are no s32+ registers.
    DCHECK_LT(double_input.code(), LowDwVfpRegister::kNumRegisters);
    LowDwVfpRegister double_scratch =
        LowDwVfpRegister::from_code(double_input.code());
    single_scratch = double_scratch.low();
  }
  vcvt_s32_f64(single_scratch, double_input);
  vmov(result, single_scratch);

  Register scratch = temps.Acquire();
  // If result is not saturated (0x7FFFFFFF or 0x80000000), we are done.
  sub(scratch, result, Operand(1));
  cmp(scratch, Operand(0x7FFFFFFE));
  b(lt, done);
}

void TurboAssembler::TruncateDoubleToI(Isolate* isolate, Zone* zone,
                                       Register result,
                                       DwVfpRegister double_input,
                                       StubCallMode stub_mode) {
  Label done;

  TryInlineTruncateDoubleToI(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  push(lr);
  AllocateStackSpace(kDoubleSize);  // Put input on stack.
  vstr(double_input, MemOperand(sp, 0));

  if (stub_mode == StubCallMode::kCallWasmRuntimeStub) {
    Call(wasm::WasmCode::kDoubleToI, RelocInfo::WASM_STUB_CALL);
  } else if (options().inline_offheap_trampolines) {
    CallBuiltin(Builtins::kDoubleToI);
  } else {
    Call(BUILTIN_CODE(isolate, DoubleToI), RelocInfo::CODE_TARGET);
  }
  ldr(result, MemOperand(sp, 0));

  add(sp, sp, Operand(kDoubleSize));
  pop(lr);

  bind(&done);
}

void MacroAssembler::CallRuntime(const Runtime::Function* f, int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // All parameters are on the stack.  r0 has the return value after call.

  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r0, Operand(num_arguments));
  Move(r1, ExternalReference::Create(f));
  Handle<Code> code =
      CodeFactory::CEntry(isolate(), f->result_size, save_doubles);
  Call(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    // TODO(1236192): Most runtime routines don't need the number of
    // arguments passed in because it is constant. At some point we
    // should remove this need and make the runtime routine entry code
    // smarter.
    mov(r0, Operand(function->nargs));
  }
  JumpToExternalReference(ExternalReference::Create(fid));
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin,
                                             bool builtin_exit_frame) {
#if defined(__thumb__)
  // Thumb mode builtin.
  DCHECK_EQ(builtin.address() & 1, 1);
#endif
  Move(r1, builtin);
  Handle<Code> code = CodeFactory::CEntry(isolate(), 1, kDontSaveFPRegs,
                                          kArgvOnStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::JumpToInstructionStream(Address entry) {
  mov(kOffHeapTrampolineRegister, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
  Jump(kOffHeapTrampolineRegister);
}

void MacroAssembler::LoadWeakValue(Register out, Register in,
                                   Label* target_if_cleared) {
  cmp(in, Operand(kClearedWeakHeapObjectLower32));
  b(eq, target_if_cleared);

  and_(out, in, Operand(~kWeakHeapObjectMask));
}

void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Move(scratch2, ExternalReference::Create(counter));
    ldr(scratch1, MemOperand(scratch2));
    add(scratch1, scratch1, Operand(value));
    str(scratch1, MemOperand(scratch2));
  }
}

void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Move(scratch2, ExternalReference::Create(counter));
    ldr(scratch1, MemOperand(scratch2));
    sub(scratch1, scratch1, Operand(value));
    str(scratch1, MemOperand(scratch2));
  }
}

void TurboAssembler::Assert(Condition cond, AbortReason reason) {
  if (emit_debug_code()) Check(cond, reason);
}

void TurboAssembler::AssertUnreachable(AbortReason reason) {
  if (emit_debug_code()) Abort(reason);
}

void TurboAssembler::Check(Condition cond, AbortReason reason) {
  Label L;
  b(cond, &L);
  Abort(reason);
  // will not return here
  bind(&L);
}

void TurboAssembler::Abort(AbortReason reason) {
  Label abort_start;
  bind(&abort_start);
#ifdef DEBUG
  const char* msg = GetAbortReason(reason);
  RecordComment("Abort message: ");
  RecordComment(msg);
#endif

  // Avoid emitting call to builtin if requested.
  if (trap_on_abort()) {
    stop();
    return;
  }

  if (should_abort_hard()) {
    // We don't care if we constructed a frame. Just pretend we did.
    FrameScope assume_frame(this, StackFrame::NONE);
    Move32BitImmediate(r0, Operand(static_cast<int>(reason)));
    PrepareCallCFunction(1, 0, r1);
    Move(r1, ExternalReference::abort_with_reason());
    // Use Call directly to avoid any unneeded overhead. The function won't
    // return anyway.
    Call(r1);
    return;
  }

  Move(r1, Smi::FromInt(static_cast<int>(reason)));

  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame()) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  } else {
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  }
  // will not return here
}

void MacroAssembler::LoadMap(Register destination, Register object) {
  ldr(destination, FieldMemOperand(object, HeapObject::kMapOffset));
}

void MacroAssembler::LoadGlobalProxy(Register dst) {
  LoadNativeContextSlot(Context::GLOBAL_PROXY_INDEX, dst);
}

void MacroAssembler::LoadNativeContextSlot(int index, Register dst) {
  LoadMap(dst, cp);
  ldr(dst, FieldMemOperand(
               dst, Map::kConstructorOrBackPointerOrNativeContextOffset));
  ldr(dst, MemOperand(dst, Context::SlotOffset(index)));
}

void TurboAssembler::InitializeRootRegister() {
  ExternalReference isolate_root = ExternalReference::isolate_root(isolate());
  mov(kRootRegister, Operand(isolate_root));
}

void MacroAssembler::SmiTag(Register reg, SBit s) {
  add(reg, reg, Operand(reg), s);
}

void MacroAssembler::SmiTag(Register dst, Register src, SBit s) {
  add(dst, src, Operand(src), s);
}

void MacroAssembler::SmiTst(Register value) {
  tst(value, Operand(kSmiTagMask));
}

void TurboAssembler::JumpIfSmi(Register value, Label* smi_label) {
  tst(value, Operand(kSmiTagMask));
  b(eq, smi_label);
}

void TurboAssembler::JumpIfEqual(Register x, int32_t y, Label* dest) {
  cmp(x, Operand(y));
  b(eq, dest);
}

void TurboAssembler::JumpIfLessThan(Register x, int32_t y, Label* dest) {
  cmp(x, Operand(y));
  b(lt, dest);
}

void MacroAssembler::JumpIfNotSmi(Register value, Label* not_smi_label) {
  tst(value, Operand(kSmiTagMask));
  b(ne, not_smi_label);
}

void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    tst(object, Operand(kSmiTagMask));
    Check(ne, AbortReason::kOperandIsASmi);
  }
}

void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    tst(object, Operand(kSmiTagMask));
    Check(eq, AbortReason::kOperandIsNotASmi);
  }
}

void MacroAssembler::AssertConstructor(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    tst(object, Operand(kSmiTagMask));
    Check(ne, AbortReason::kOperandIsASmiAndNotAConstructor);
    push(object);
    LoadMap(object, object);
    ldrb(object, FieldMemOperand(object, Map::kBitFieldOffset));
    tst(object, Operand(Map::IsConstructorBit::kMask));
    pop(object);
    Check(ne, AbortReason::kOperandIsNotAConstructor);
  }
}

void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    tst(object, Operand(kSmiTagMask));
    Check(ne, AbortReason::kOperandIsASmiAndNotAFunction);
    push(object);
    CompareObjectType(object, object, object, JS_FUNCTION_TYPE);
    pop(object);
    Check(eq, AbortReason::kOperandIsNotAFunction);
  }
}

void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    tst(object, Operand(kSmiTagMask));
    Check(ne, AbortReason::kOperandIsASmiAndNotABoundFunction);
    push(object);
    CompareObjectType(object, object, object, JS_BOUND_FUNCTION_TYPE);
    pop(object);
    Check(eq, AbortReason::kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!emit_debug_code()) return;
  tst(object, Operand(kSmiTagMask));
  Check(ne, AbortReason::kOperandIsASmiAndNotAGeneratorObject);

  // Load map
  Register map = object;
  push(object);
  LoadMap(map, object);

  // Check if JSGeneratorObject
  Label do_check;
  Register instance_type = object;
  CompareInstanceType(map, instance_type, JS_GENERATOR_OBJECT_TYPE);
  b(eq, &do_check);

  // Check if JSAsyncFunctionObject (See MacroAssembler::CompareInstanceType)
  cmp(instance_type, Operand(JS_ASYNC_FUNCTION_OBJECT_TYPE));
  b(eq, &do_check);

  // Check if JSAsyncGeneratorObject (See MacroAssembler::CompareInstanceType)
  cmp(instance_type, Operand(JS_ASYNC_GENERATOR_OBJECT_TYPE));

  bind(&do_check);
  // Restore generator object to register and perform assertion
  pop(object);
  Check(eq, AbortReason::kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    CompareRoot(object, RootIndex::kUndefinedValue);
    b(eq, &done_checking);
    LoadMap(scratch, object);
    CompareInstanceType(scratch, scratch, ALLOCATION_SITE_TYPE);
    Assert(eq, AbortReason::kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}

void TurboAssembler::CheckFor32DRegs(Register scratch) {
  Move(scratch, ExternalReference::cpu_features());
  ldr(scratch, MemOperand(scratch));
  tst(scratch, Operand(1u << VFP32DREGS));
}

void TurboAssembler::SaveFPRegs(Register location, Register scratch) {
  CpuFeatureScope scope(this, VFP32DREGS, CpuFeatureScope::kDontCheckSupported);
  CheckFor32DRegs(scratch);
  vstm(db_w, location, d16, d31, ne);
  sub(location, location, Operand(16 * kDoubleSize), LeaveCC, eq);
  vstm(db_w, location, d0, d15);
}

void TurboAssembler::RestoreFPRegs(Register location, Register scratch) {
  CpuFeatureScope scope(this, VFP32DREGS, CpuFeatureScope::kDontCheckSupported);
  CheckFor32DRegs(scratch);
  vldm(ia_w, location, d0, d15);
  vldm(ia_w, location, d16, d31, ne);
  add(location, location, Operand(16 * kDoubleSize), LeaveCC, eq);
}

template <typename T>
void TurboAssembler::FloatMaxHelper(T result, T left, T right,
                                    Label* out_of_line) {
  // This trivial case is caught sooner, so that the out-of-line code can be
  // completely avoided.
  DCHECK(left != right);

  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(this, ARMv8);
    VFPCompareAndSetFlags(left, right);
    b(vs, out_of_line);
    vmaxnm(result, left, right);
  } else {
    Label done;
    VFPCompareAndSetFlags(left, right);
    b(vs, out_of_line);
    // Avoid a conditional instruction if the result register is unique.
    bool aliased_result_reg = result == left || result == right;
    Move(result, right, aliased_result_reg ? mi : al);
    Move(result, left, gt);
    b(ne, &done);
    // Left and right are equal, but check for +/-0.
    VFPCompareAndSetFlags(left, 0.0);
    b(eq, out_of_line);
    // The arguments are equal and not zero, so it doesn't matter which input we
    // pick. We have already moved one input into the result (if it didn't
    // already alias) so there's nothing more to do.
    bind(&done);
  }
}

template <typename T>
void TurboAssembler::FloatMaxOutOfLineHelper(T result, T left, T right) {
  DCHECK(left != right);

  // ARMv8: At least one of left and right is a NaN.
  // Anything else: At least one of left and right is a NaN, or both left and
  // right are zeroes with unknown sign.

  // If left and right are +/-0, select the one with the most positive sign.
  // If left or right are NaN, vadd propagates the appropriate one.
  vadd(result, left, right);
}

template <typename T>
void TurboAssembler::FloatMinHelper(T result, T left, T right,
                                    Label* out_of_line) {
  // This trivial case is caught sooner, so that the out-of-line code can be
  // completely avoided.
  DCHECK(left != right);

  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(this, ARMv8);
    VFPCompareAndSetFlags(left, right);
    b(vs, out_of_line);
    vminnm(result, left, right);
  } else {
    Label done;
    VFPCompareAndSetFlags(left, right);
    b(vs, out_of_line);
    // Avoid a conditional instruction if the result register is unique.
    bool aliased_result_reg = result == left || result == right;
    Move(result, left, aliased_result_reg ? mi : al);
    Move(result, right, gt);
    b(ne, &done);
    // Left and right are equal, but check for +/-0.
    VFPCompareAndSetFlags(left, 0.0);
    // If the arguments are equal and not zero, it doesn't matter which input we
    // pick. We have already moved one input into the result (if it didn't
    // already alias) so there's nothing more to do.
    b(ne, &done);
    // At this point, both left and right are either 0 or -0.
    // We could use a single 'vorr' instruction here if we had NEON support.
    // The algorithm used is -((-L) + (-R)), which is most efficiently expressed
    // as -((-L) - R).
    if (left == result) {
      DCHECK(right != result);
      vneg(result, left);
      vsub(result, result, right);
      vneg(result, result);
    } else {
      DCHECK(left != result);
      vneg(result, right);
      vsub(result, result, left);
      vneg(result, result);
    }
    bind(&done);
  }
}

template <typename T>
void TurboAssembler::FloatMinOutOfLineHelper(T result, T left, T right) {
  DCHECK(left != right);

  // At least one of left and right is a NaN. Use vadd to propagate the NaN
  // appropriately. +/-0 is handled inline.
  vadd(result, left, right);
}

void TurboAssembler::FloatMax(SwVfpRegister result, SwVfpRegister left,
                              SwVfpRegister right, Label* out_of_line) {
  FloatMaxHelper(result, left, right, out_of_line);
}

void TurboAssembler::FloatMin(SwVfpRegister result, SwVfpRegister left,
                              SwVfpRegister right, Label* out_of_line) {
  FloatMinHelper(result, left, right, out_of_line);
}

void TurboAssembler::FloatMax(DwVfpRegister result, DwVfpRegister left,
                              DwVfpRegister right, Label* out_of_line) {
  FloatMaxHelper(result, left, right, out_of_line);
}

void TurboAssembler::FloatMin(DwVfpRegister result, DwVfpRegister left,
                              DwVfpRegister right, Label* out_of_line) {
  FloatMinHelper(result, left, right, out_of_line);
}

void TurboAssembler::FloatMaxOutOfLine(SwVfpRegister result, SwVfpRegister left,
                                       SwVfpRegister right) {
  FloatMaxOutOfLineHelper(result, left, right);
}

void TurboAssembler::FloatMinOutOfLine(SwVfpRegister result, SwVfpRegister left,
                                       SwVfpRegister right) {
  FloatMinOutOfLineHelper(result, left, right);
}

void TurboAssembler::FloatMaxOutOfLine(DwVfpRegister result, DwVfpRegister left,
                                       DwVfpRegister right) {
  FloatMaxOutOfLineHelper(result, left, right);
}

void TurboAssembler::FloatMinOutOfLine(DwVfpRegister result, DwVfpRegister left,
                                       DwVfpRegister right) {
  FloatMinOutOfLineHelper(result, left, right);
}

static const int kRegisterPassedArguments = 4;

int TurboAssembler::CalculateStackPassedWords(int num_reg_arguments,
                                              int num_double_arguments) {
  int stack_passed_words = 0;
  if (use_eabi_hardfloat()) {
    // In the hard floating point calling convention, we can use
    // all double registers to pass doubles.
    if (num_double_arguments > DoubleRegister::NumRegisters()) {
      stack_passed_words +=
          2 * (num_double_arguments - DoubleRegister::NumRegisters());
    }
  } else {
    // In the soft floating point calling convention, every double
    // argument is passed using two registers.
    num_reg_arguments += 2 * num_double_arguments;
  }
  // Up to four simple arguments are passed in registers r0..r3.
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
  if (frame_alignment > kPointerSize) {
    UseScratchRegisterScope temps(this);
    if (!scratch.is_valid()) scratch = temps.Acquire();
    // Make stack end at alignment and make room for num_arguments - 4 words
    // and the original value of sp.
    mov(scratch, sp);
    AllocateStackSpace((stack_passed_arguments + 1) * kPointerSize);
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    and_(sp, sp, Operand(-frame_alignment));
    str(scratch, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else if (stack_passed_arguments > 0) {
    AllocateStackSpace(stack_passed_arguments * kPointerSize);
  }
}

void TurboAssembler::MovToFloatParameter(DwVfpRegister src) {
  DCHECK(src == d0);
  if (!use_eabi_hardfloat()) {
    vmov(r0, r1, src);
  }
}

// On ARM this is just a synonym to make the purpose clear.
void TurboAssembler::MovToFloatResult(DwVfpRegister src) {
  MovToFloatParameter(src);
}

void TurboAssembler::MovToFloatParameters(DwVfpRegister src1,
                                          DwVfpRegister src2) {
  DCHECK(src1 == d0);
  DCHECK(src2 == d1);
  if (!use_eabi_hardfloat()) {
    vmov(r0, r1, src1);
    vmov(r2, r3, src2);
  }
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Move(scratch, function);
  CallCFunctionHelper(scratch, num_reg_arguments, num_double_arguments);
}

void TurboAssembler::CallCFunction(Register function, int num_reg_arguments,
                                   int num_double_arguments) {
  CallCFunctionHelper(function, num_reg_arguments, num_double_arguments);
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}

void TurboAssembler::CallCFunction(Register function, int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}

void TurboAssembler::CallCFunctionHelper(Register function,
                                         int num_reg_arguments,
                                         int num_double_arguments) {
  DCHECK_LE(num_reg_arguments + num_double_arguments, kMaxCParameters);
  DCHECK(has_frame());
  // Make sure that the stack is aligned before calling a C function unless
  // running in the simulator. The simulator has its own alignment check which
  // provides more information.
#if V8_HOST_ARCH_ARM
  if (emit_debug_code()) {
    int frame_alignment = base::OS::ActivationFrameAlignment();
    int frame_alignment_mask = frame_alignment - 1;
    if (frame_alignment > kPointerSize) {
      DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
      Label alignment_as_expected;
      tst(sp, Operand(frame_alignment_mask));
      b(eq, &alignment_as_expected);
      // Don't use Check here, as it will call Runtime_Abort possibly
      // re-entering here.
      stop();
      bind(&alignment_as_expected);
    }
  }
#endif

  // Save the frame pointer and PC so that the stack layout remains iterable,
  // even without an ExitFrame which normally exists between JS and C frames.
  if (isolate() != nullptr) {
    Register scratch = r4;
    Push(scratch);

    Move(scratch, ExternalReference::fast_c_call_caller_pc_address(isolate()));
    str(pc, MemOperand(scratch));
    Move(scratch, ExternalReference::fast_c_call_caller_fp_address(isolate()));
    str(fp, MemOperand(scratch));
    Pop(scratch);
  }

  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.
  Call(function);

  if (isolate() != nullptr) {
    // We don't unset the PC; the FP is the source of truth.
    Register scratch1 = r4;
    Register scratch2 = r5;
    Push(scratch1);
    Push(scratch2);
    Move(scratch1, ExternalReference::fast_c_call_caller_fp_address(isolate()));
    mov(scratch2, Operand::Zero());
    str(scratch2, MemOperand(scratch1));
    Pop(scratch2);
    Pop(scratch1);
  }

  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);
  if (ActivationFrameAlignment() > kPointerSize) {
    ldr(sp, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    add(sp, sp, Operand(stack_passed_arguments * kPointerSize));
  }
}

void TurboAssembler::CheckPageFlag(Register object, int mask, Condition cc,
                                   Label* condition_met) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  DCHECK(cc == eq || cc == ne);
  Bfc(scratch, object, 0, kPageSizeBits);
  ldr(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
  tst(scratch, Operand(mask));
  b(cc, condition_met);
}

Register GetRegisterThatIsNotOneOf(Register reg1, Register reg2, Register reg3,
                                   Register reg4, Register reg5,
                                   Register reg6) {
  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();

  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_general_registers(); ++i) {
    int code = config->GetAllocatableGeneralCode(i);
    Register candidate = Register::from_code(code);
    if (regs & candidate.bit()) continue;
    return candidate;
  }
  UNREACHABLE();
}

void TurboAssembler::ComputeCodeStartAddress(Register dst) {
  // We can use the register pc - 8 for the address of the current instruction.
  sub(dst, pc, Operand(pc_offset() + Instruction::kPcLoadDelta));
}

void TurboAssembler::ResetSpeculationPoisonRegister() {
  mov(kSpeculationPoisonRegister, Operand(-1));
}

void TurboAssembler::CallForDeoptimization(Address target, int deopt_id) {
  NoRootArrayScope no_root_array(this);

  // Save the deopt id in r10 (we don't need the roots array from now on).
  DCHECK_LE(deopt_id, 0xFFFF);
  if (CpuFeatures::IsSupported(ARMv7)) {
    // On ARMv7, we can use movw (with a maximum immediate of 0xFFFF)
    movw(r10, deopt_id);
  } else {
    // On ARMv6, we might need two instructions.
    mov(r10, Operand(deopt_id & 0xFF));  // Set the low byte.
    if (deopt_id >= 0xFF) {
      orr(r10, r10, Operand(deopt_id & 0xFF00));  // Set the high byte.
    }
  }

  Call(target, RelocInfo::RUNTIME_ENTRY);
  CheckConstPool(false, false);
}

void TurboAssembler::Trap() { stop(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

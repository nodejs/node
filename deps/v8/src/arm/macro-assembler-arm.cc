// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>  // For LONG_MIN, LONG_MAX.

#if V8_TARGET_ARCH_ARM

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/register-configuration.h"
#include "src/runtime/runtime.h"

#include "src/arm/macro-assembler-arm.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* arg_isolate, void* buffer, int size,
                               CodeObjectRequired create_code_object)
    : Assembler(arg_isolate, buffer, size),
      generating_stub_(false),
      has_frame_(false) {
  if (create_code_object == CodeObjectRequired::kYes) {
    code_object_ =
        Handle<Object>::New(isolate()->heap()->undefined_value(), isolate());
  }
}


void MacroAssembler::Jump(Register target, Condition cond) {
  bx(target, cond);
}


void MacroAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  mov(pc, Operand(target, rmode), LeaveCC, cond);
}


void MacroAssembler::Jump(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(target), rmode, cond);
}


void MacroAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  // 'code' is always generated ARM code, never THUMB code
  AllowDeferredHandleDereference embedding_raw_address;
  Jump(reinterpret_cast<intptr_t>(code.location()), rmode, cond);
}


int MacroAssembler::CallSize(Register target, Condition cond) {
  return kInstrSize;
}


void MacroAssembler::Call(Register target, Condition cond) {
  // Block constant pool for the call instruction sequence.
  BlockConstPoolScope block_const_pool(this);
  Label start;
  bind(&start);
  blx(target, cond);
  DCHECK_EQ(CallSize(target, cond), SizeOfCodeGeneratedSince(&start));
}


int MacroAssembler::CallSize(
    Address target, RelocInfo::Mode rmode, Condition cond) {
  Instr mov_instr = cond | MOV | LeaveCC;
  Operand mov_operand = Operand(reinterpret_cast<intptr_t>(target), rmode);
  return kInstrSize +
         mov_operand.instructions_required(this, mov_instr) * kInstrSize;
}


int MacroAssembler::CallStubSize(
    CodeStub* stub, TypeFeedbackId ast_id, Condition cond) {
  return CallSize(stub->GetCode(), RelocInfo::CODE_TARGET, ast_id, cond);
}


int MacroAssembler::CallSizeNotPredictableCodeSize(Isolate* isolate,
                                                   Address target,
                                                   RelocInfo::Mode rmode,
                                                   Condition cond) {
  Instr mov_instr = cond | MOV | LeaveCC;
  Operand mov_operand = Operand(reinterpret_cast<intptr_t>(target), rmode);
  return kInstrSize +
         mov_operand.instructions_required(NULL, mov_instr) * kInstrSize;
}


void MacroAssembler::Call(Address target,
                          RelocInfo::Mode rmode,
                          Condition cond,
                          TargetAddressStorageMode mode) {
  // Block constant pool for the call instruction sequence.
  BlockConstPoolScope block_const_pool(this);
  Label start;
  bind(&start);

  bool old_predictable_code_size = predictable_code_size();
  if (mode == NEVER_INLINE_TARGET_ADDRESS) {
    set_predictable_code_size(true);
  }

#ifdef DEBUG
  // Check the expected size before generating code to ensure we assume the same
  // constant pool availability (e.g., whether constant pool is full or not).
  int expected_size = CallSize(target, rmode, cond);
#endif

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

  // Statement positions are expected to be recorded when the target
  // address is loaded. The mov method will automatically record
  // positions when pc is the target, since this is not the case here
  // we have to do it explicitly.
  positions_recorder()->WriteRecordedPositions();

  mov(ip, Operand(reinterpret_cast<int32_t>(target), rmode));
  blx(ip, cond);

  DCHECK_EQ(expected_size, SizeOfCodeGeneratedSince(&start));
  if (mode == NEVER_INLINE_TARGET_ADDRESS) {
    set_predictable_code_size(old_predictable_code_size);
  }
}


int MacroAssembler::CallSize(Handle<Code> code,
                             RelocInfo::Mode rmode,
                             TypeFeedbackId ast_id,
                             Condition cond) {
  AllowDeferredHandleDereference using_raw_address;
  return CallSize(reinterpret_cast<Address>(code.location()), rmode, cond);
}


void MacroAssembler::Call(Handle<Code> code,
                          RelocInfo::Mode rmode,
                          TypeFeedbackId ast_id,
                          Condition cond,
                          TargetAddressStorageMode mode) {
  Label start;
  bind(&start);
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  if (rmode == RelocInfo::CODE_TARGET && !ast_id.IsNone()) {
    SetRecordedAstId(ast_id);
    rmode = RelocInfo::CODE_TARGET_WITH_ID;
  }
  // 'code' is always generated ARM code, never THUMB code
  AllowDeferredHandleDereference embedding_raw_address;
  Call(reinterpret_cast<Address>(code.location()), rmode, cond, mode);
}


void MacroAssembler::Ret(Condition cond) {
  bx(lr, cond);
}


void MacroAssembler::Drop(int count, Condition cond) {
  if (count > 0) {
    add(sp, sp, Operand(count * kPointerSize), LeaveCC, cond);
  }
}


void MacroAssembler::Ret(int drop, Condition cond) {
  Drop(drop, cond);
  Ret(cond);
}


void MacroAssembler::Swap(Register reg1,
                          Register reg2,
                          Register scratch,
                          Condition cond) {
  if (scratch.is(no_reg)) {
    eor(reg1, reg1, Operand(reg2), LeaveCC, cond);
    eor(reg2, reg2, Operand(reg1), LeaveCC, cond);
    eor(reg1, reg1, Operand(reg2), LeaveCC, cond);
  } else {
    mov(scratch, reg1, LeaveCC, cond);
    mov(reg1, reg2, LeaveCC, cond);
    mov(reg2, scratch, LeaveCC, cond);
  }
}


void MacroAssembler::Call(Label* target) {
  bl(target);
}


void MacroAssembler::Push(Handle<Object> handle) {
  mov(ip, Operand(handle));
  push(ip);
}


void MacroAssembler::Move(Register dst, Handle<Object> value) {
  AllowDeferredHandleDereference smi_check;
  if (value->IsSmi()) {
    mov(dst, Operand(value));
  } else {
    DCHECK(value->IsHeapObject());
    if (isolate()->heap()->InNewSpace(*value)) {
      Handle<Cell> cell = isolate()->factory()->NewCell(value);
      mov(dst, Operand(cell));
      ldr(dst, FieldMemOperand(dst, Cell::kValueOffset));
    } else {
      mov(dst, Operand(value));
    }
  }
}


void MacroAssembler::Move(Register dst, Register src, Condition cond) {
  if (!dst.is(src)) {
    mov(dst, src, LeaveCC, cond);
  }
}


void MacroAssembler::Move(DwVfpRegister dst, DwVfpRegister src) {
  if (!dst.is(src)) {
    vmov(dst, src);
  }
}


void MacroAssembler::Mls(Register dst, Register src1, Register src2,
                         Register srcA, Condition cond) {
  if (CpuFeatures::IsSupported(MLS)) {
    CpuFeatureScope scope(this, MLS);
    mls(dst, src1, src2, srcA, cond);
  } else {
    DCHECK(!srcA.is(ip));
    mul(ip, src1, src2, LeaveCC, cond);
    sub(dst, srcA, ip, LeaveCC, cond);
  }
}


void MacroAssembler::And(Register dst, Register src1, const Operand& src2,
                         Condition cond) {
  if (!src2.is_reg() &&
      !src2.must_output_reloc_info(this) &&
      src2.immediate() == 0) {
    mov(dst, Operand::Zero(), LeaveCC, cond);
  } else if (!(src2.instructions_required(this) == 1) &&
             !src2.must_output_reloc_info(this) &&
             CpuFeatures::IsSupported(ARMv7) &&
             base::bits::IsPowerOfTwo32(src2.immediate() + 1)) {
    ubfx(dst, src1, 0,
        WhichPowerOf2(static_cast<uint32_t>(src2.immediate()) + 1), cond);
  } else {
    and_(dst, src1, src2, LeaveCC, cond);
  }
}


void MacroAssembler::Ubfx(Register dst, Register src1, int lsb, int width,
                          Condition cond) {
  DCHECK(lsb < 32);
  if (!CpuFeatures::IsSupported(ARMv7) || predictable_code_size()) {
    int mask = (1 << (width + lsb)) - 1 - ((1 << lsb) - 1);
    and_(dst, src1, Operand(mask), LeaveCC, cond);
    if (lsb != 0) {
      mov(dst, Operand(dst, LSR, lsb), LeaveCC, cond);
    }
  } else {
    ubfx(dst, src1, lsb, width, cond);
  }
}


void MacroAssembler::Sbfx(Register dst, Register src1, int lsb, int width,
                          Condition cond) {
  DCHECK(lsb < 32);
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
    sbfx(dst, src1, lsb, width, cond);
  }
}


void MacroAssembler::Bfi(Register dst,
                         Register src,
                         Register scratch,
                         int lsb,
                         int width,
                         Condition cond) {
  DCHECK(0 <= lsb && lsb < 32);
  DCHECK(0 <= width && width < 32);
  DCHECK(lsb + width < 32);
  DCHECK(!scratch.is(dst));
  if (width == 0) return;
  if (!CpuFeatures::IsSupported(ARMv7) || predictable_code_size()) {
    int mask = (1 << (width + lsb)) - 1 - ((1 << lsb) - 1);
    bic(dst, dst, Operand(mask));
    and_(scratch, src, Operand((1 << width) - 1));
    mov(scratch, Operand(scratch, LSL, lsb));
    orr(dst, dst, scratch);
  } else {
    bfi(dst, src, lsb, width, cond);
  }
}


void MacroAssembler::Bfc(Register dst, Register src, int lsb, int width,
                         Condition cond) {
  DCHECK(lsb < 32);
  if (!CpuFeatures::IsSupported(ARMv7) || predictable_code_size()) {
    int mask = (1 << (width + lsb)) - 1 - ((1 << lsb) - 1);
    bic(dst, src, Operand(mask));
  } else {
    Move(dst, src, cond);
    bfc(dst, lsb, width, cond);
  }
}


void MacroAssembler::Usat(Register dst, int satpos, const Operand& src,
                          Condition cond) {
  if (!CpuFeatures::IsSupported(ARMv7) || predictable_code_size()) {
    DCHECK(!dst.is(pc) && !src.rm().is(pc));
    DCHECK((satpos >= 0) && (satpos <= 31));

    // These asserts are required to ensure compatibility with the ARMv7
    // implementation.
    DCHECK((src.shift_op() == ASR) || (src.shift_op() == LSL));
    DCHECK(src.rs().is(no_reg));

    Label done;
    int satval = (1 << satpos) - 1;

    if (cond != al) {
      b(NegateCondition(cond), &done);  // Skip saturate if !condition.
    }
    if (!(src.is_reg() && dst.is(src.rm()))) {
      mov(dst, src);
    }
    tst(dst, Operand(~satval));
    b(eq, &done);
    mov(dst, Operand::Zero(), LeaveCC, mi);  // 0 if negative.
    mov(dst, Operand(satval), LeaveCC, pl);  // satval if positive.
    bind(&done);
  } else {
    usat(dst, satpos, src, cond);
  }
}


void MacroAssembler::Load(Register dst,
                          const MemOperand& src,
                          Representation r) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8()) {
    ldrsb(dst, src);
  } else if (r.IsUInteger8()) {
    ldrb(dst, src);
  } else if (r.IsInteger16()) {
    ldrsh(dst, src);
  } else if (r.IsUInteger16()) {
    ldrh(dst, src);
  } else {
    ldr(dst, src);
  }
}


void MacroAssembler::Store(Register src,
                           const MemOperand& dst,
                           Representation r) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8() || r.IsUInteger8()) {
    strb(src, dst);
  } else if (r.IsInteger16() || r.IsUInteger16()) {
    strh(src, dst);
  } else {
    if (r.IsHeapObject()) {
      AssertNotSmi(src);
    } else if (r.IsSmi()) {
      AssertSmi(src);
    }
    str(src, dst);
  }
}


void MacroAssembler::LoadRoot(Register destination,
                              Heap::RootListIndex index,
                              Condition cond) {
  if (CpuFeatures::IsSupported(MOVW_MOVT_IMMEDIATE_LOADS) &&
      isolate()->heap()->RootCanBeTreatedAsConstant(index) &&
      !predictable_code_size()) {
    // The CPU supports fast immediate values, and this root will never
    // change. We will load it as a relocatable immediate value.
    Handle<Object> root = isolate()->heap()->root_handle(index);
    mov(destination, Operand(root), LeaveCC, cond);
    return;
  }
  ldr(destination, MemOperand(kRootRegister, index << kPointerSizeLog2), cond);
}


void MacroAssembler::StoreRoot(Register source,
                               Heap::RootListIndex index,
                               Condition cond) {
  DCHECK(Heap::RootCanBeWrittenAfterInitialization(index));
  str(source, MemOperand(kRootRegister, index << kPointerSizeLog2), cond);
}


void MacroAssembler::InNewSpace(Register object,
                                Register scratch,
                                Condition cond,
                                Label* branch) {
  DCHECK(cond == eq || cond == ne);
  and_(scratch, object, Operand(ExternalReference::new_space_mask(isolate())));
  cmp(scratch, Operand(ExternalReference::new_space_start(isolate())));
  b(cond, branch);
}


void MacroAssembler::RecordWriteField(
    Register object,
    int offset,
    Register value,
    Register dst,
    LinkRegisterStatus lr_status,
    SaveFPRegsMode save_fp,
    RememberedSetAction remembered_set_action,
    SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
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

  add(dst, object, Operand(offset - kHeapObjectTag));
  if (emit_debug_code()) {
    Label ok;
    tst(dst, Operand((1 << kPointerSizeLog2) - 1));
    b(eq, &ok);
    stop("Unaligned cell in write barrier");
    bind(&ok);
  }

  RecordWrite(object,
              dst,
              value,
              lr_status,
              save_fp,
              remembered_set_action,
              OMIT_SMI_CHECK,
              pointers_to_here_check_for_value);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Operand(bit_cast<int32_t>(kZapValue + 4)));
    mov(dst, Operand(bit_cast<int32_t>(kZapValue + 8)));
  }
}


// Will clobber 4 registers: object, map, dst, ip.  The
// register 'object' contains a heap object pointer.
void MacroAssembler::RecordWriteForMap(Register object,
                                       Register map,
                                       Register dst,
                                       LinkRegisterStatus lr_status,
                                       SaveFPRegsMode fp_mode) {
  if (emit_debug_code()) {
    ldr(dst, FieldMemOperand(map, HeapObject::kMapOffset));
    cmp(dst, Operand(isolate()->factory()->meta_map()));
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  if (!FLAG_incremental_marking) {
    return;
  }

  if (emit_debug_code()) {
    ldr(ip, FieldMemOperand(object, HeapObject::kMapOffset));
    cmp(ip, map);
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  Label done;

  // A single check of the map's pages interesting flag suffices, since it is
  // only set during incremental collection, and then it's also guaranteed that
  // the from object's page's interesting flag is also set.  This optimization
  // relies on the fact that maps can never be in new space.
  CheckPageFlag(map,
                map,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask,
                eq,
                &done);

  add(dst, object, Operand(HeapObject::kMapOffset - kHeapObjectTag));
  if (emit_debug_code()) {
    Label ok;
    tst(dst, Operand((1 << kPointerSizeLog2) - 1));
    b(eq, &ok);
    stop("Unaligned cell in write barrier");
    bind(&ok);
  }

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    push(lr);
  }
  RecordWriteStub stub(isolate(), object, map, dst, OMIT_REMEMBERED_SET,
                       fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(lr);
  }

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1, ip, dst);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(dst, Operand(bit_cast<int32_t>(kZapValue + 12)));
    mov(map, Operand(bit_cast<int32_t>(kZapValue + 16)));
  }
}


// Will clobber 4 registers: object, address, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(
    Register object,
    Register address,
    Register value,
    LinkRegisterStatus lr_status,
    SaveFPRegsMode fp_mode,
    RememberedSetAction remembered_set_action,
    SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  DCHECK(!object.is(value));
  if (emit_debug_code()) {
    ldr(ip, MemOperand(address));
    cmp(ip, value);
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  if (remembered_set_action == OMIT_REMEMBERED_SET &&
      !FLAG_incremental_marking) {
    return;
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  if (pointers_to_here_check_for_value != kPointersToHereAreAlwaysInteresting) {
    CheckPageFlag(value,
                  value,  // Used as scratch.
                  MemoryChunk::kPointersToHereAreInterestingMask,
                  eq,
                  &done);
  }
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask,
                eq,
                &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    push(lr);
  }
  RecordWriteStub stub(isolate(), object, value, address, remembered_set_action,
                       fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(lr);
  }

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1, ip,
                   value);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(address, Operand(bit_cast<int32_t>(kZapValue + 12)));
    mov(value, Operand(bit_cast<int32_t>(kZapValue + 16)));
  }
}


void MacroAssembler::RememberedSetHelper(Register object,  // For debug tests.
                                         Register address,
                                         Register scratch,
                                         SaveFPRegsMode fp_mode,
                                         RememberedSetFinalAction and_then) {
  Label done;
  if (emit_debug_code()) {
    Label ok;
    JumpIfNotInNewSpace(object, scratch, &ok);
    stop("Remembered set pointer is in new space");
    bind(&ok);
  }
  // Load store buffer top.
  ExternalReference store_buffer =
      ExternalReference::store_buffer_top(isolate());
  mov(ip, Operand(store_buffer));
  ldr(scratch, MemOperand(ip));
  // Store pointer to buffer and increment buffer top.
  str(address, MemOperand(scratch, kPointerSize, PostIndex));
  // Write back new top of buffer.
  str(scratch, MemOperand(ip));
  // Call stub on end of buffer.
  // Check for end of buffer.
  tst(scratch, Operand(StoreBuffer::kStoreBufferOverflowBit));
  if (and_then == kFallThroughAtEnd) {
    b(eq, &done);
  } else {
    DCHECK(and_then == kReturnAtEnd);
    Ret(eq);
  }
  push(lr);
  StoreBufferOverflowStub store_buffer_overflow(isolate(), fp_mode);
  CallStub(&store_buffer_overflow);
  pop(lr);
  bind(&done);
  if (and_then == kReturnAtEnd) {
    Ret();
  }
}


void MacroAssembler::PushFixedFrame(Register marker_reg) {
  DCHECK(!marker_reg.is_valid() || marker_reg.code() < cp.code());
  stm(db_w, sp, (marker_reg.is_valid() ? marker_reg.bit() : 0) | cp.bit() |
                    (FLAG_enable_embedded_constant_pool ? pp.bit() : 0) |
                    fp.bit() | lr.bit());
}


void MacroAssembler::PopFixedFrame(Register marker_reg) {
  DCHECK(!marker_reg.is_valid() || marker_reg.code() < cp.code());
  ldm(ia_w, sp, (marker_reg.is_valid() ? marker_reg.bit() : 0) | cp.bit() |
                    (FLAG_enable_embedded_constant_pool ? pp.bit() : 0) |
                    fp.bit() | lr.bit());
}


// Push and pop all registers that can hold pointers.
void MacroAssembler::PushSafepointRegisters() {
  // Safepoints expect a block of contiguous register values starting with r0.
  // except when FLAG_enable_embedded_constant_pool, which omits pp.
  DCHECK(kSafepointSavedRegisters ==
         (FLAG_enable_embedded_constant_pool
              ? ((1 << (kNumSafepointSavedRegisters + 1)) - 1) & ~pp.bit()
              : (1 << kNumSafepointSavedRegisters) - 1));
  // Safepoints expect a block of kNumSafepointRegisters values on the
  // stack, so adjust the stack for unsaved registers.
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  DCHECK(num_unsaved >= 0);
  sub(sp, sp, Operand(num_unsaved * kPointerSize));
  stm(db_w, sp, kSafepointSavedRegisters);
}


void MacroAssembler::PopSafepointRegisters() {
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  ldm(ia_w, sp, kSafepointSavedRegisters);
  add(sp, sp, Operand(num_unsaved * kPointerSize));
}


void MacroAssembler::StoreToSafepointRegisterSlot(Register src, Register dst) {
  str(src, SafepointRegisterSlot(dst));
}


void MacroAssembler::LoadFromSafepointRegisterSlot(Register dst, Register src) {
  ldr(dst, SafepointRegisterSlot(src));
}


int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // The registers are pushed starting with the highest encoding,
  // which means that lowest encodings are closest to the stack pointer.
  if (FLAG_enable_embedded_constant_pool && reg_code > pp.code()) {
    // RegList omits pp.
    reg_code -= 1;
  }
  DCHECK(reg_code >= 0 && reg_code < kNumSafepointRegisters);
  return reg_code;
}


MemOperand MacroAssembler::SafepointRegisterSlot(Register reg) {
  return MemOperand(sp, SafepointRegisterStackIndex(reg.code()) * kPointerSize);
}


MemOperand MacroAssembler::SafepointRegistersAndDoublesSlot(Register reg) {
  // Number of d-regs not known at snapshot time.
  DCHECK(!serializer_enabled());
  // General purpose registers are pushed last on the stack.
  const RegisterConfiguration* config =
      RegisterConfiguration::ArchDefault(RegisterConfiguration::CRANKSHAFT);
  int doubles_size = config->num_allocatable_double_registers() * kDoubleSize;
  int register_offset = SafepointRegisterStackIndex(reg.code()) * kPointerSize;
  return MemOperand(sp, doubles_size + register_offset);
}


void MacroAssembler::Ldrd(Register dst1, Register dst2,
                          const MemOperand& src, Condition cond) {
  DCHECK(src.rm().is(no_reg));
  DCHECK(!dst1.is(lr));  // r14.

  // V8 does not use this addressing mode, so the fallback code
  // below doesn't support it yet.
  DCHECK((src.am() != PreIndex) && (src.am() != NegPreIndex));

  // Generate two ldr instructions if ldrd is not available.
  if (CpuFeatures::IsSupported(ARMv7) && !predictable_code_size() &&
      (dst1.code() % 2 == 0) && (dst1.code() + 1 == dst2.code())) {
    CpuFeatureScope scope(this, ARMv7);
    ldrd(dst1, dst2, src, cond);
  } else {
    if ((src.am() == Offset) || (src.am() == NegOffset)) {
      MemOperand src2(src);
      src2.set_offset(src2.offset() + 4);
      if (dst1.is(src.rn())) {
        ldr(dst2, src2, cond);
        ldr(dst1, src, cond);
      } else {
        ldr(dst1, src, cond);
        ldr(dst2, src2, cond);
      }
    } else {  // PostIndex or NegPostIndex.
      DCHECK((src.am() == PostIndex) || (src.am() == NegPostIndex));
      if (dst1.is(src.rn())) {
        ldr(dst2, MemOperand(src.rn(), 4, Offset), cond);
        ldr(dst1, src, cond);
      } else {
        MemOperand src2(src);
        src2.set_offset(src2.offset() - 4);
        ldr(dst1, MemOperand(src.rn(), 4, PostIndex), cond);
        ldr(dst2, src2, cond);
      }
    }
  }
}


void MacroAssembler::Strd(Register src1, Register src2,
                          const MemOperand& dst, Condition cond) {
  DCHECK(dst.rm().is(no_reg));
  DCHECK(!src1.is(lr));  // r14.

  // V8 does not use this addressing mode, so the fallback code
  // below doesn't support it yet.
  DCHECK((dst.am() != PreIndex) && (dst.am() != NegPreIndex));

  // Generate two str instructions if strd is not available.
  if (CpuFeatures::IsSupported(ARMv7) && !predictable_code_size() &&
      (src1.code() % 2 == 0) && (src1.code() + 1 == src2.code())) {
    CpuFeatureScope scope(this, ARMv7);
    strd(src1, src2, dst, cond);
  } else {
    MemOperand dst2(dst);
    if ((dst.am() == Offset) || (dst.am() == NegOffset)) {
      dst2.set_offset(dst2.offset() + 4);
      str(src1, dst, cond);
      str(src2, dst2, cond);
    } else {  // PostIndex or NegPostIndex.
      DCHECK((dst.am() == PostIndex) || (dst.am() == NegPostIndex));
      dst2.set_offset(dst2.offset() - 4);
      str(src1, MemOperand(dst.rn(), 4, PostIndex), cond);
      str(src2, dst2, cond);
    }
  }
}


void MacroAssembler::VFPEnsureFPSCRState(Register scratch) {
  // If needed, restore wanted bits of FPSCR.
  Label fpscr_done;
  vmrs(scratch);
  if (emit_debug_code()) {
    Label rounding_mode_correct;
    tst(scratch, Operand(kVFPRoundingModeMask));
    b(eq, &rounding_mode_correct);
    // Don't call Assert here, since Runtime_Abort could re-enter here.
    stop("Default rounding mode not set");
    bind(&rounding_mode_correct);
  }
  tst(scratch, Operand(kVFPDefaultNaNModeControlBit));
  b(ne, &fpscr_done);
  orr(scratch, scratch, Operand(kVFPDefaultNaNModeControlBit));
  vmsr(scratch);
  bind(&fpscr_done);
}


void MacroAssembler::VFPCanonicalizeNaN(const DwVfpRegister dst,
                                        const DwVfpRegister src,
                                        const Condition cond) {
  vsub(dst, src, kDoubleRegZero, cond);
}


void MacroAssembler::VFPCompareAndSetFlags(const SwVfpRegister src1,
                                           const SwVfpRegister src2,
                                           const Condition cond) {
  // Compare and move FPSCR flags to the normal condition flags.
  VFPCompareAndLoadFlags(src1, src2, pc, cond);
}

void MacroAssembler::VFPCompareAndSetFlags(const SwVfpRegister src1,
                                           const float src2,
                                           const Condition cond) {
  // Compare and move FPSCR flags to the normal condition flags.
  VFPCompareAndLoadFlags(src1, src2, pc, cond);
}


void MacroAssembler::VFPCompareAndSetFlags(const DwVfpRegister src1,
                                           const DwVfpRegister src2,
                                           const Condition cond) {
  // Compare and move FPSCR flags to the normal condition flags.
  VFPCompareAndLoadFlags(src1, src2, pc, cond);
}

void MacroAssembler::VFPCompareAndSetFlags(const DwVfpRegister src1,
                                           const double src2,
                                           const Condition cond) {
  // Compare and move FPSCR flags to the normal condition flags.
  VFPCompareAndLoadFlags(src1, src2, pc, cond);
}


void MacroAssembler::VFPCompareAndLoadFlags(const SwVfpRegister src1,
                                            const SwVfpRegister src2,
                                            const Register fpscr_flags,
                                            const Condition cond) {
  // Compare and load FPSCR.
  vcmp(src1, src2, cond);
  vmrs(fpscr_flags, cond);
}

void MacroAssembler::VFPCompareAndLoadFlags(const SwVfpRegister src1,
                                            const float src2,
                                            const Register fpscr_flags,
                                            const Condition cond) {
  // Compare and load FPSCR.
  vcmp(src1, src2, cond);
  vmrs(fpscr_flags, cond);
}


void MacroAssembler::VFPCompareAndLoadFlags(const DwVfpRegister src1,
                                            const DwVfpRegister src2,
                                            const Register fpscr_flags,
                                            const Condition cond) {
  // Compare and load FPSCR.
  vcmp(src1, src2, cond);
  vmrs(fpscr_flags, cond);
}

void MacroAssembler::VFPCompareAndLoadFlags(const DwVfpRegister src1,
                                            const double src2,
                                            const Register fpscr_flags,
                                            const Condition cond) {
  // Compare and load FPSCR.
  vcmp(src1, src2, cond);
  vmrs(fpscr_flags, cond);
}


void MacroAssembler::Vmov(const DwVfpRegister dst,
                          const double imm,
                          const Register scratch) {
  static const DoubleRepresentation minus_zero(-0.0);
  static const DoubleRepresentation zero(0.0);
  DoubleRepresentation value_rep(imm);
  // Handle special values first.
  if (value_rep == zero) {
    vmov(dst, kDoubleRegZero);
  } else if (value_rep == minus_zero) {
    vneg(dst, kDoubleRegZero);
  } else {
    vmov(dst, imm, scratch);
  }
}


void MacroAssembler::VmovHigh(Register dst, DwVfpRegister src) {
  if (src.code() < 16) {
    const LowDwVfpRegister loc = LowDwVfpRegister::from_code(src.code());
    vmov(dst, loc.high());
  } else {
    vmov(dst, VmovIndexHi, src);
  }
}


void MacroAssembler::VmovHigh(DwVfpRegister dst, Register src) {
  if (dst.code() < 16) {
    const LowDwVfpRegister loc = LowDwVfpRegister::from_code(dst.code());
    vmov(loc.high(), src);
  } else {
    vmov(dst, VmovIndexHi, src);
  }
}


void MacroAssembler::VmovLow(Register dst, DwVfpRegister src) {
  if (src.code() < 16) {
    const LowDwVfpRegister loc = LowDwVfpRegister::from_code(src.code());
    vmov(dst, loc.low());
  } else {
    vmov(dst, VmovIndexLo, src);
  }
}


void MacroAssembler::VmovLow(DwVfpRegister dst, Register src) {
  if (dst.code() < 16) {
    const LowDwVfpRegister loc = LowDwVfpRegister::from_code(dst.code());
    vmov(loc.low(), src);
  } else {
    vmov(dst, VmovIndexLo, src);
  }
}


void MacroAssembler::LoadConstantPoolPointerRegisterFromCodeTargetAddress(
    Register code_target_address) {
  DCHECK(FLAG_enable_embedded_constant_pool);
  ldr(pp, MemOperand(code_target_address,
                     Code::kConstantPoolOffset - Code::kHeaderSize));
  add(pp, pp, code_target_address);
}


void MacroAssembler::LoadConstantPoolPointerRegister() {
  DCHECK(FLAG_enable_embedded_constant_pool);
  int entry_offset = pc_offset() + Instruction::kPCReadOffset;
  sub(ip, pc, Operand(entry_offset));
  LoadConstantPoolPointerRegisterFromCodeTargetAddress(ip);
}


void MacroAssembler::StubPrologue() {
  PushFixedFrame();
  Push(Smi::FromInt(StackFrame::STUB));
  // Adjust FP to point to saved FP.
  add(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));
  if (FLAG_enable_embedded_constant_pool) {
    LoadConstantPoolPointerRegister();
    set_constant_pool_available(true);
  }
}


void MacroAssembler::Prologue(bool code_pre_aging) {
  { PredictableCodeSizeScope predictible_code_size_scope(
        this, kNoCodeAgeSequenceLength);
    // The following three instructions must remain together and unmodified
    // for code aging to work properly.
    if (code_pre_aging) {
      // Pre-age the code.
      Code* stub = Code::GetPreAgedCodeAgeStub(isolate());
      add(r0, pc, Operand(-8));
      ldr(pc, MemOperand(pc, -4));
      emit_code_stub_address(stub);
    } else {
      PushFixedFrame(r1);
      nop(ip.code());
      // Adjust FP to point to saved FP.
      add(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));
    }
  }
  if (FLAG_enable_embedded_constant_pool) {
    LoadConstantPoolPointerRegister();
    set_constant_pool_available(true);
  }
}


void MacroAssembler::EmitLoadTypeFeedbackVector(Register vector) {
  ldr(vector, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  ldr(vector, FieldMemOperand(vector, JSFunction::kSharedFunctionInfoOffset));
  ldr(vector,
      FieldMemOperand(vector, SharedFunctionInfo::kFeedbackVectorOffset));
}


void MacroAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool_pointer_reg) {
  // r0-r3: preserved
  PushFixedFrame();
  if (FLAG_enable_embedded_constant_pool && load_constant_pool_pointer_reg) {
    LoadConstantPoolPointerRegister();
  }
  mov(ip, Operand(Smi::FromInt(type)));
  push(ip);
  mov(ip, Operand(CodeObject()));
  push(ip);
  // Adjust FP to point to saved FP.
  add(fp, sp,
      Operand(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize));
}


int MacroAssembler::LeaveFrame(StackFrame::Type type) {
  // r0: preserved
  // r1: preserved
  // r2: preserved

  // Drop the execution stack down to the frame pointer and restore
  // the caller frame pointer, return address and constant pool pointer
  // (if FLAG_enable_embedded_constant_pool).
  int frame_ends;
  if (FLAG_enable_embedded_constant_pool) {
    add(sp, fp, Operand(StandardFrameConstants::kConstantPoolOffset));
    frame_ends = pc_offset();
    ldm(ia_w, sp, pp.bit() | fp.bit() | lr.bit());
  } else {
    mov(sp, fp);
    frame_ends = pc_offset();
    ldm(ia_w, sp, fp.bit() | lr.bit());
  }
  return frame_ends;
}


void MacroAssembler::EnterExitFrame(bool save_doubles, int stack_space) {
  // Set up the frame structure on the stack.
  DCHECK_EQ(2 * kPointerSize, ExitFrameConstants::kCallerSPDisplacement);
  DCHECK_EQ(1 * kPointerSize, ExitFrameConstants::kCallerPCOffset);
  DCHECK_EQ(0 * kPointerSize, ExitFrameConstants::kCallerFPOffset);
  Push(lr, fp);
  mov(fp, Operand(sp));  // Set up new frame pointer.
  // Reserve room for saved entry sp and code object.
  sub(sp, sp, Operand(ExitFrameConstants::kFrameSize));
  if (emit_debug_code()) {
    mov(ip, Operand::Zero());
    str(ip, MemOperand(fp, ExitFrameConstants::kSPOffset));
  }
  if (FLAG_enable_embedded_constant_pool) {
    str(pp, MemOperand(fp, ExitFrameConstants::kConstantPoolOffset));
  }
  mov(ip, Operand(CodeObject()));
  str(ip, MemOperand(fp, ExitFrameConstants::kCodeOffset));

  // Save the frame pointer and the context in top.
  mov(ip, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  str(fp, MemOperand(ip));
  mov(ip, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
  str(cp, MemOperand(ip));

  // Optionally save all double registers.
  if (save_doubles) {
    SaveFPRegs(sp, ip);
    // Note that d0 will be accessible at
    //   fp - ExitFrameConstants::kFrameSize -
    //   DwVfpRegister::kMaxNumRegisters * kDoubleSize,
    // since the sp slot, code slot and constant pool slot (if
    // FLAG_enable_embedded_constant_pool) were pushed after the fp.
  }

  // Reserve place for the return address and stack space and align the frame
  // preparing for calling the runtime function.
  const int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  sub(sp, sp, Operand((stack_space + 1) * kPointerSize));
  if (frame_alignment > 0) {
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
    and_(sp, sp, Operand(-frame_alignment));
  }

  // Set the exit frame sp value to point just before the return address
  // location.
  add(ip, sp, Operand(kPointerSize));
  str(ip, MemOperand(fp, ExitFrameConstants::kSPOffset));
}


void MacroAssembler::InitializeNewString(Register string,
                                         Register length,
                                         Heap::RootListIndex map_index,
                                         Register scratch1,
                                         Register scratch2) {
  SmiTag(scratch1, length);
  LoadRoot(scratch2, map_index);
  str(scratch1, FieldMemOperand(string, String::kLengthOffset));
  mov(scratch1, Operand(String::kEmptyHashField));
  str(scratch2, FieldMemOperand(string, HeapObject::kMapOffset));
  str(scratch1, FieldMemOperand(string, String::kHashFieldOffset));
}


int MacroAssembler::ActivationFrameAlignment() {
#if V8_HOST_ARCH_ARM
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one ARM
  // platform for another ARM platform with a different alignment.
  return base::OS::ActivationFrameAlignment();
#else  // V8_HOST_ARCH_ARM
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif  // V8_HOST_ARCH_ARM
}


void MacroAssembler::LeaveExitFrame(bool save_doubles, Register argument_count,
                                    bool restore_context,
                                    bool argument_count_is_length) {
  ConstantPoolUnavailableScope constant_pool_unavailable(this);

  // Optionally restore all double registers.
  if (save_doubles) {
    // Calculate the stack location of the saved doubles and restore them.
    const int offset = ExitFrameConstants::kFrameSize;
    sub(r3, fp,
        Operand(offset + DwVfpRegister::kMaxNumRegisters * kDoubleSize));
    RestoreFPRegs(r3, ip);
  }

  // Clear top frame.
  mov(r3, Operand::Zero());
  mov(ip, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  str(r3, MemOperand(ip));

  // Restore current context from top and clear it in debug mode.
  if (restore_context) {
    mov(ip, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
    ldr(cp, MemOperand(ip));
  }
#ifdef DEBUG
  mov(ip, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
  str(r3, MemOperand(ip));
#endif

  // Tear down the exit frame, pop the arguments, and return.
  if (FLAG_enable_embedded_constant_pool) {
    ldr(pp, MemOperand(fp, ExitFrameConstants::kConstantPoolOffset));
  }
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


void MacroAssembler::MovFromFloatResult(const DwVfpRegister dst) {
  if (use_eabi_hardfloat()) {
    Move(dst, d0);
  } else {
    vmov(dst, r0, r1);
  }
}


// On ARM this is just a synonym to make the purpose clear.
void MacroAssembler::MovFromFloatParameter(DwVfpRegister dst) {
  MovFromFloatResult(dst);
}


void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. If not,
  // setup registers according to contract with ArgumentsAdaptorTrampoline:
  //  r0: actual arguments count
  //  r1: function (passed through to callee)
  //  r2: expected arguments count

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract if values are
  // passed in registers.
  DCHECK(actual.is_immediate() || actual.reg().is(r0));
  DCHECK(expected.is_immediate() || expected.reg().is(r2));

  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
    mov(r0, Operand(actual.immediate()));
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      const int sentinel = SharedFunctionInfo::kDontAdaptArgumentsSentinel;
      if (expected.immediate() == sentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        *definitely_mismatches = true;
        mov(r2, Operand(expected.immediate()));
      }
    }
  } else {
    if (actual.is_immediate()) {
      mov(r0, Operand(actual.immediate()));
      cmp(expected.reg(), Operand(actual.immediate()));
      b(eq, &regular_invoke);
    } else {
      cmp(expected.reg(), Operand(actual.reg()));
      b(eq, &regular_invoke);
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor =
        isolate()->builtins()->ArgumentsAdaptorTrampoline();
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(adaptor));
      Call(adaptor);
      call_wrapper.AfterCall();
      if (!*definitely_mismatches) {
        b(done);
      }
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&regular_invoke);
  }
}


void MacroAssembler::FloodFunctionIfStepping(Register fun, Register new_target,
                                             const ParameterCount& expected,
                                             const ParameterCount& actual) {
  Label skip_flooding;
  ExternalReference step_in_enabled =
      ExternalReference::debug_step_in_enabled_address(isolate());
  mov(r4, Operand(step_in_enabled));
  ldrb(r4, MemOperand(r4));
  cmp(r4, Operand(0));
  b(eq, &skip_flooding);
  {
    FrameScope frame(this,
                     has_frame() ? StackFrame::NONE : StackFrame::INTERNAL);
    if (expected.is_reg()) {
      SmiTag(expected.reg());
      Push(expected.reg());
    }
    if (actual.is_reg()) {
      SmiTag(actual.reg());
      Push(actual.reg());
    }
    if (new_target.is_valid()) {
      Push(new_target);
    }
    Push(fun);
    Push(fun);
    CallRuntime(Runtime::kDebugPrepareStepInIfStepping, 1);
    Pop(fun);
    if (new_target.is_valid()) {
      Pop(new_target);
    }
    if (actual.is_reg()) {
      Pop(actual.reg());
      SmiUntag(actual.reg());
    }
    if (expected.is_reg()) {
      Pop(expected.reg());
      SmiUntag(expected.reg());
    }
  }
  bind(&skip_flooding);
}


void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        const ParameterCount& expected,
                                        const ParameterCount& actual,
                                        InvokeFlag flag,
                                        const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());
  DCHECK(function.is(r1));
  DCHECK_IMPLIES(new_target.is_valid(), new_target.is(r3));

  if (call_wrapper.NeedsDebugStepCheck()) {
    FloodFunctionIfStepping(function, new_target, expected, actual);
  }

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(r3, Heap::kUndefinedValueRootIndex);
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, &done, &definitely_mismatches, flag,
                 call_wrapper);
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    Register code = r4;
    ldr(code, FieldMemOperand(function, JSFunction::kCodeEntryOffset));
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      Call(code);
      call_wrapper.AfterCall();
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      Jump(code);
    }

    // Continue here if InvokePrologue does handle the invocation due to
    // mismatched parameter counts.
    bind(&done);
  }
}


void MacroAssembler::InvokeFunction(Register fun,
                                    Register new_target,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r1.
  DCHECK(fun.is(r1));

  Register expected_reg = r2;
  Register temp_reg = r4;

  ldr(temp_reg, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));
  ldr(expected_reg,
      FieldMemOperand(temp_reg,
                      SharedFunctionInfo::kFormalParameterCountOffset));
  SmiUntag(expected_reg);

  ParameterCount expected(expected_reg);
  InvokeFunctionCode(fun, new_target, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r1.
  DCHECK(function.is(r1));

  // Get the function and setup the context.
  ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  InvokeFunctionCode(r1, no_reg, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  Move(r1, function);
  InvokeFunction(r1, expected, actual, flag, call_wrapper);
}


void MacroAssembler::IsObjectJSStringType(Register object,
                                          Register scratch,
                                          Label* fail) {
  DCHECK(kNotStringTag != 0);

  ldr(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  tst(scratch, Operand(kIsNotStringMask));
  b(ne, fail);
}


void MacroAssembler::IsObjectNameType(Register object,
                                      Register scratch,
                                      Label* fail) {
  ldr(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  cmp(scratch, Operand(LAST_NAME_TYPE));
  b(hi, fail);
}


void MacroAssembler::DebugBreak() {
  mov(r0, Operand::Zero());
  mov(r1,
      Operand(ExternalReference(Runtime::kHandleDebuggerStatement, isolate())));
  CEntryStub ces(isolate(), 1);
  DCHECK(AllowThisStubCall(&ces));
  Call(ces.GetCode(), RelocInfo::DEBUGGER_STATEMENT);
}


void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);

  // Link the current handler as the next handler.
  mov(r6, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  ldr(r5, MemOperand(r6));
  push(r5);

  // Set this new handler as the current one.
  str(sp, MemOperand(r6));
}


void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  pop(r1);
  mov(ip, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  add(sp, sp, Operand(StackHandlerConstants::kSize - kPointerSize));
  str(r1, MemOperand(ip));
}


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch,
                                            Label* miss) {
  Label same_contexts;

  DCHECK(!holder_reg.is(scratch));
  DCHECK(!holder_reg.is(ip));
  DCHECK(!scratch.is(ip));

  // Load current lexical context from the stack frame.
  ldr(scratch, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // In debug mode, make sure the lexical context is set.
#ifdef DEBUG
  cmp(scratch, Operand::Zero());
  Check(ne, kWeShouldNotHaveAnEmptyLexicalContext);
#endif

  // Load the native context of the current context.
  ldr(scratch, ContextMemOperand(scratch, Context::NATIVE_CONTEXT_INDEX));

  // Check the context is a native context.
  if (emit_debug_code()) {
    // Cannot use ip as a temporary in this verification code. Due to the fact
    // that ip is clobbered as part of cmp with an object Operand.
    push(holder_reg);  // Temporarily save holder on the stack.
    // Read the first word and compare to the native_context_map.
    ldr(holder_reg, FieldMemOperand(scratch, HeapObject::kMapOffset));
    LoadRoot(ip, Heap::kNativeContextMapRootIndex);
    cmp(holder_reg, ip);
    Check(eq, kJSGlobalObjectNativeContextShouldBeANativeContext);
    pop(holder_reg);  // Restore holder.
  }

  // Check if both contexts are the same.
  ldr(ip, FieldMemOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  cmp(scratch, Operand(ip));
  b(eq, &same_contexts);

  // Check the context is a native context.
  if (emit_debug_code()) {
    // Cannot use ip as a temporary in this verification code. Due to the fact
    // that ip is clobbered as part of cmp with an object Operand.
    push(holder_reg);  // Temporarily save holder on the stack.
    mov(holder_reg, ip);  // Move ip to its holding place.
    LoadRoot(ip, Heap::kNullValueRootIndex);
    cmp(holder_reg, ip);
    Check(ne, kJSGlobalProxyContextShouldNotBeNull);

    ldr(holder_reg, FieldMemOperand(holder_reg, HeapObject::kMapOffset));
    LoadRoot(ip, Heap::kNativeContextMapRootIndex);
    cmp(holder_reg, ip);
    Check(eq, kJSGlobalObjectNativeContextShouldBeANativeContext);
    // Restore ip is not needed. ip is reloaded below.
    pop(holder_reg);  // Restore holder.
    // Restore ip to holder's context.
    ldr(ip, FieldMemOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  }

  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.
  int token_offset = Context::kHeaderSize +
                     Context::SECURITY_TOKEN_INDEX * kPointerSize;

  ldr(scratch, FieldMemOperand(scratch, token_offset));
  ldr(ip, FieldMemOperand(ip, token_offset));
  cmp(scratch, Operand(ip));
  b(ne, miss);

  bind(&same_contexts);
}


// Compute the hash code from the untagged key.  This must be kept in sync with
// ComputeIntegerHash in utils.h and KeyedLoadGenericStub in
// code-stub-hydrogen.cc
void MacroAssembler::GetNumberHash(Register t0, Register scratch) {
  // First of all we assign the hash seed to scratch.
  LoadRoot(scratch, Heap::kHashSeedRootIndex);
  SmiUntag(scratch);

  // Xor original key with a seed.
  eor(t0, t0, Operand(scratch));

  // Compute the hash code from the untagged key.  This must be kept in sync
  // with ComputeIntegerHash in utils.h.
  //
  // hash = ~hash + (hash << 15);
  mvn(scratch, Operand(t0));
  add(t0, scratch, Operand(t0, LSL, 15));
  // hash = hash ^ (hash >> 12);
  eor(t0, t0, Operand(t0, LSR, 12));
  // hash = hash + (hash << 2);
  add(t0, t0, Operand(t0, LSL, 2));
  // hash = hash ^ (hash >> 4);
  eor(t0, t0, Operand(t0, LSR, 4));
  // hash = hash * 2057;
  mov(scratch, Operand(t0, LSL, 11));
  add(t0, t0, Operand(t0, LSL, 3));
  add(t0, t0, scratch);
  // hash = hash ^ (hash >> 16);
  eor(t0, t0, Operand(t0, LSR, 16));
  bic(t0, t0, Operand(0xc0000000u));
}


void MacroAssembler::LoadFromNumberDictionary(Label* miss,
                                              Register elements,
                                              Register key,
                                              Register result,
                                              Register t0,
                                              Register t1,
                                              Register t2) {
  // Register use:
  //
  // elements - holds the slow-case elements of the receiver on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // key      - holds the smi key on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // result   - holds the result on exit if the load succeeded.
  //            Allowed to be the same as 'key' or 'result'.
  //            Unchanged on bailout so 'key' or 'result' can be used
  //            in further computation.
  //
  // Scratch registers:
  //
  // t0 - holds the untagged key on entry and holds the hash once computed.
  //
  // t1 - used to hold the capacity mask of the dictionary
  //
  // t2 - used for the index into the dictionary.
  Label done;

  GetNumberHash(t0, t1);

  // Compute the capacity mask.
  ldr(t1, FieldMemOperand(elements, SeededNumberDictionary::kCapacityOffset));
  SmiUntag(t1);
  sub(t1, t1, Operand(1));

  // Generate an unrolled loop that performs a few probes before giving up.
  for (int i = 0; i < kNumberDictionaryProbes; i++) {
    // Use t2 for index calculations and keep the hash intact in t0.
    mov(t2, t0);
    // Compute the masked index: (hash + i + i * i) & mask.
    if (i > 0) {
      add(t2, t2, Operand(SeededNumberDictionary::GetProbeOffset(i)));
    }
    and_(t2, t2, Operand(t1));

    // Scale the index by multiplying by the element size.
    DCHECK(SeededNumberDictionary::kEntrySize == 3);
    add(t2, t2, Operand(t2, LSL, 1));  // t2 = t2 * 3

    // Check if the key is identical to the name.
    add(t2, elements, Operand(t2, LSL, kPointerSizeLog2));
    ldr(ip, FieldMemOperand(t2, SeededNumberDictionary::kElementsStartOffset));
    cmp(key, Operand(ip));
    if (i != kNumberDictionaryProbes - 1) {
      b(eq, &done);
    } else {
      b(ne, miss);
    }
  }

  bind(&done);
  // Check that the value is a field property.
  // t2: elements + (index * kPointerSize)
  const int kDetailsOffset =
      SeededNumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  ldr(t1, FieldMemOperand(t2, kDetailsOffset));
  DCHECK_EQ(DATA, 0);
  tst(t1, Operand(Smi::FromInt(PropertyDetails::TypeField::kMask)));
  b(ne, miss);

  // Get the value at the masked, scaled index and return.
  const int kValueOffset =
      SeededNumberDictionary::kElementsStartOffset + kPointerSize;
  ldr(result, FieldMemOperand(t2, kValueOffset));
}


void MacroAssembler::Allocate(int object_size,
                              Register result,
                              Register scratch1,
                              Register scratch2,
                              Label* gc_required,
                              AllocationFlags flags) {
  DCHECK(object_size <= Page::kMaxRegularHeapObjectSize);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      mov(result, Operand(0x7091));
      mov(scratch1, Operand(0x7191));
      mov(scratch2, Operand(0x7291));
    }
    jmp(gc_required);
    return;
  }

  DCHECK(!AreAliased(result, scratch1, scratch2, ip));

  // Make object size into bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    object_size *= kPointerSize;
  }
  DCHECK_EQ(0, object_size & kObjectAlignmentMask);

  // Check relative positions of allocation top and limit addresses.
  // The values must be adjacent in memory to allow the use of LDM.
  // Also, assert that the registers are numbered such that the values
  // are loaded in the correct order.
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  intptr_t top = reinterpret_cast<intptr_t>(allocation_top.address());
  intptr_t limit = reinterpret_cast<intptr_t>(allocation_limit.address());
  DCHECK((limit - top) == kPointerSize);
  DCHECK(result.code() < ip.code());

  // Set up allocation top address register.
  Register top_address = scratch1;
  // This code stores a temporary value in ip. This is OK, as the code below
  // does not need ip for implicit literal generation.
  Register alloc_limit = ip;
  Register result_end = scratch2;
  mov(top_address, Operand(allocation_top));

  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and allocation limit into alloc_limit.
    ldm(ia, top_address, result.bit() | alloc_limit.bit());
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry.
      ldr(alloc_limit, MemOperand(top_address));
      cmp(result, alloc_limit);
      Check(eq, kUnexpectedAllocationTop);
    }
    // Load allocation limit. Result already contains allocation top.
    ldr(alloc_limit, MemOperand(top_address, limit - top));
  }

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    // Align the next allocation. Storing the filler map without checking top is
    // safe in new-space because the limit of the heap is aligned there.
    STATIC_ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    and_(result_end, result, Operand(kDoubleAlignmentMask), SetCC);
    Label aligned;
    b(eq, &aligned);
    if ((flags & PRETENURE) != 0) {
      cmp(result, Operand(alloc_limit));
      b(hs, gc_required);
    }
    mov(result_end, Operand(isolate()->factory()->one_pointer_filler_map()));
    str(result_end, MemOperand(result, kDoubleSize / 2, PostIndex));
    bind(&aligned);
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top. We must preserve the ip register at this
  // point, so we cannot just use add().
  DCHECK(object_size > 0);
  Register source = result;
  Condition cond = al;
  int shift = 0;
  while (object_size != 0) {
    if (((object_size >> shift) & 0x03) == 0) {
      shift += 2;
    } else {
      int bits = object_size & (0xff << shift);
      object_size -= bits;
      shift += 8;
      Operand bits_operand(bits);
      DCHECK(bits_operand.instructions_required(this) == 1);
      add(result_end, source, bits_operand, SetCC, cond);
      source = result_end;
      cond = cc;
    }
  }
  b(cs, gc_required);
  cmp(result_end, Operand(alloc_limit));
  b(hi, gc_required);
  str(result_end, MemOperand(top_address));

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    add(result, result, Operand(kHeapObjectTag));
  }
}


void MacroAssembler::Allocate(Register object_size, Register result,
                              Register result_end, Register scratch,
                              Label* gc_required, AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      mov(result, Operand(0x7091));
      mov(scratch, Operand(0x7191));
      mov(result_end, Operand(0x7291));
    }
    jmp(gc_required);
    return;
  }

  // |object_size| and |result_end| may overlap if the DOUBLE_ALIGNMENT flag
  // is not specified. Other registers must not overlap.
  DCHECK(!AreAliased(object_size, result, scratch, ip));
  DCHECK(!AreAliased(result_end, result, scratch, ip));
  DCHECK((flags & DOUBLE_ALIGNMENT) == 0 || !object_size.is(result_end));

  // Check relative positions of allocation top and limit addresses.
  // The values must be adjacent in memory to allow the use of LDM.
  // Also, assert that the registers are numbered such that the values
  // are loaded in the correct order.
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  intptr_t top = reinterpret_cast<intptr_t>(allocation_top.address());
  intptr_t limit = reinterpret_cast<intptr_t>(allocation_limit.address());
  DCHECK((limit - top) == kPointerSize);
  DCHECK(result.code() < ip.code());

  // Set up allocation top address and allocation limit registers.
  Register top_address = scratch;
  // This code stores a temporary value in ip. This is OK, as the code below
  // does not need ip for implicit literal generation.
  Register alloc_limit = ip;
  mov(top_address, Operand(allocation_top));

  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and allocation limit into alloc_limit.
    ldm(ia, top_address, result.bit() | alloc_limit.bit());
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry.
      ldr(alloc_limit, MemOperand(top_address));
      cmp(result, alloc_limit);
      Check(eq, kUnexpectedAllocationTop);
    }
    // Load allocation limit. Result already contains allocation top.
    ldr(alloc_limit, MemOperand(top_address, limit - top));
  }

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    // Align the next allocation. Storing the filler map without checking top is
    // safe in new-space because the limit of the heap is aligned there.
    DCHECK(kPointerAlignment * 2 == kDoubleAlignment);
    and_(result_end, result, Operand(kDoubleAlignmentMask), SetCC);
    Label aligned;
    b(eq, &aligned);
    if ((flags & PRETENURE) != 0) {
      cmp(result, Operand(alloc_limit));
      b(hs, gc_required);
    }
    mov(result_end, Operand(isolate()->factory()->one_pointer_filler_map()));
    str(result_end, MemOperand(result, kDoubleSize / 2, PostIndex));
    bind(&aligned);
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top. Object size may be in words so a shift is
  // required to get the number of bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    add(result_end, result, Operand(object_size, LSL, kPointerSizeLog2), SetCC);
  } else {
    add(result_end, result, Operand(object_size), SetCC);
  }
  b(cs, gc_required);
  cmp(result_end, Operand(alloc_limit));
  b(hi, gc_required);

  // Update allocation top. result temporarily holds the new top.
  if (emit_debug_code()) {
    tst(result_end, Operand(kObjectAlignmentMask));
    Check(eq, kUnalignedAllocationInNewSpace);
  }
  str(result_end, MemOperand(top_address));

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    add(result, result, Operand(kHeapObjectTag));
  }
}


void MacroAssembler::AllocateTwoByteString(Register result,
                                           Register length,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  DCHECK((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  mov(scratch1, Operand(length, LSL, 1));  // Length in bytes, not chars.
  add(scratch1, scratch1,
      Operand(kObjectAlignmentMask + SeqTwoByteString::kHeaderSize));
  and_(scratch1, scratch1, Operand(~kObjectAlignmentMask));

  // Allocate two-byte string in new space.
  Allocate(scratch1,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result,
                      length,
                      Heap::kStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateOneByteString(Register result, Register length,
                                           Register scratch1, Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  DCHECK((SeqOneByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  DCHECK(kCharSize == 1);
  add(scratch1, length,
      Operand(kObjectAlignmentMask + SeqOneByteString::kHeaderSize));
  and_(scratch1, scratch1, Operand(~kObjectAlignmentMask));

  // Allocate one-byte string in new space.
  Allocate(scratch1,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result, length, Heap::kOneByteStringMapRootIndex,
                      scratch1, scratch2);
}


void MacroAssembler::AllocateTwoByteConsString(Register result,
                                               Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  Allocate(ConsString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kConsStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateOneByteConsString(Register result, Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           TAG_OBJECT);

  InitializeNewString(result, length, Heap::kConsOneByteStringMapRootIndex,
                      scratch1, scratch2);
}


void MacroAssembler::AllocateTwoByteSlicedString(Register result,
                                                 Register length,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kSlicedStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateOneByteSlicedString(Register result,
                                                 Register length,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result, length, Heap::kSlicedOneByteStringMapRootIndex,
                      scratch1, scratch2);
}


void MacroAssembler::CompareObjectType(Register object,
                                       Register map,
                                       Register type_reg,
                                       InstanceType type) {
  const Register temp = type_reg.is(no_reg) ? ip : type_reg;

  ldr(map, FieldMemOperand(object, HeapObject::kMapOffset));
  CompareInstanceType(map, temp, type);
}


void MacroAssembler::CompareInstanceType(Register map,
                                         Register type_reg,
                                         InstanceType type) {
  // Registers map and type_reg can be ip. These two lines assert
  // that ip can be used with the two instructions (the constants
  // will never need ip).
  STATIC_ASSERT(Map::kInstanceTypeOffset < 4096);
  STATIC_ASSERT(LAST_TYPE < 256);
  ldrb(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  cmp(type_reg, Operand(type));
}


void MacroAssembler::CompareRoot(Register obj,
                                 Heap::RootListIndex index) {
  DCHECK(!obj.is(ip));
  LoadRoot(ip, index);
  cmp(obj, ip);
}


void MacroAssembler::CheckFastElements(Register map,
                                       Register scratch,
                                       Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  ldrb(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  cmp(scratch, Operand(Map::kMaximumBitField2FastHoleyElementValue));
  b(hi, fail);
}


void MacroAssembler::CheckFastObjectElements(Register map,
                                             Register scratch,
                                             Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  ldrb(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  cmp(scratch, Operand(Map::kMaximumBitField2FastHoleySmiElementValue));
  b(ls, fail);
  cmp(scratch, Operand(Map::kMaximumBitField2FastHoleyElementValue));
  b(hi, fail);
}


void MacroAssembler::CheckFastSmiElements(Register map,
                                          Register scratch,
                                          Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  ldrb(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  cmp(scratch, Operand(Map::kMaximumBitField2FastHoleySmiElementValue));
  b(hi, fail);
}


void MacroAssembler::StoreNumberToDoubleElements(
                                      Register value_reg,
                                      Register key_reg,
                                      Register elements_reg,
                                      Register scratch1,
                                      LowDwVfpRegister double_scratch,
                                      Label* fail,
                                      int elements_offset) {
  DCHECK(!AreAliased(value_reg, key_reg, elements_reg, scratch1));
  Label smi_value, store;

  // Handle smi values specially.
  JumpIfSmi(value_reg, &smi_value);

  // Ensure that the object is a heap number
  CheckMap(value_reg,
           scratch1,
           isolate()->factory()->heap_number_map(),
           fail,
           DONT_DO_SMI_CHECK);

  vldr(double_scratch, FieldMemOperand(value_reg, HeapNumber::kValueOffset));
  // Force a canonical NaN.
  if (emit_debug_code()) {
    vmrs(ip);
    tst(ip, Operand(kVFPDefaultNaNModeControlBit));
    Assert(ne, kDefaultNaNModeNotSet);
  }
  VFPCanonicalizeNaN(double_scratch);
  b(&store);

  bind(&smi_value);
  SmiToDouble(double_scratch, value_reg);

  bind(&store);
  add(scratch1, elements_reg, Operand::DoubleOffsetFromSmiKey(key_reg));
  vstr(double_scratch,
       FieldMemOperand(scratch1,
                       FixedDoubleArray::kHeaderSize - elements_offset));
}


void MacroAssembler::CompareMap(Register obj,
                                Register scratch,
                                Handle<Map> map,
                                Label* early_success) {
  ldr(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  CompareMap(scratch, map, early_success);
}


void MacroAssembler::CompareMap(Register obj_map,
                                Handle<Map> map,
                                Label* early_success) {
  cmp(obj_map, Operand(map));
}


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Handle<Map> map,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }

  Label success;
  CompareMap(obj, scratch, map, &success);
  b(ne, fail);
  bind(&success);
}


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Heap::RootListIndex index,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }
  ldr(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  LoadRoot(ip, index);
  cmp(scratch, ip);
  b(ne, fail);
}


void MacroAssembler::DispatchWeakMap(Register obj, Register scratch1,
                                     Register scratch2, Handle<WeakCell> cell,
                                     Handle<Code> success,
                                     SmiCheckType smi_check_type) {
  Label fail;
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, &fail);
  }
  ldr(scratch1, FieldMemOperand(obj, HeapObject::kMapOffset));
  CmpWeakValue(scratch1, cell, scratch2);
  Jump(success, RelocInfo::CODE_TARGET, eq);
  bind(&fail);
}


void MacroAssembler::CmpWeakValue(Register value, Handle<WeakCell> cell,
                                  Register scratch) {
  mov(scratch, Operand(cell));
  ldr(scratch, FieldMemOperand(scratch, WeakCell::kValueOffset));
  cmp(value, scratch);
}


void MacroAssembler::GetWeakValue(Register value, Handle<WeakCell> cell) {
  mov(value, Operand(cell));
  ldr(value, FieldMemOperand(value, WeakCell::kValueOffset));
}


void MacroAssembler::LoadWeakValue(Register value, Handle<WeakCell> cell,
                                   Label* miss) {
  GetWeakValue(value, cell);
  JumpIfSmi(value, miss);
}


void MacroAssembler::GetMapConstructor(Register result, Register map,
                                       Register temp, Register temp2) {
  Label done, loop;
  ldr(result, FieldMemOperand(map, Map::kConstructorOrBackPointerOffset));
  bind(&loop);
  JumpIfSmi(result, &done);
  CompareObjectType(result, temp, temp2, MAP_TYPE);
  b(ne, &done);
  ldr(result, FieldMemOperand(result, Map::kConstructorOrBackPointerOffset));
  b(&loop);
  bind(&done);
}


void MacroAssembler::TryGetFunctionPrototype(Register function, Register result,
                                             Register scratch, Label* miss) {
  // Get the prototype or initial map from the function.
  ldr(result,
      FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and
  // simply miss the cache instead. This will allow us to allocate a
  // prototype object on-demand in the runtime system.
  LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  cmp(result, ip);
  b(eq, miss);

  // If the function does not have an initial map, we're done.
  Label done;
  CompareObjectType(result, scratch, scratch, MAP_TYPE);
  b(ne, &done);

  // Get the prototype from the initial map.
  ldr(result, FieldMemOperand(result, Map::kPrototypeOffset));

  // All done.
  bind(&done);
}


void MacroAssembler::CallStub(CodeStub* stub,
                              TypeFeedbackId ast_id,
                              Condition cond) {
  DCHECK(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(), RelocInfo::CODE_TARGET, ast_id, cond);
}


void MacroAssembler::TailCallStub(CodeStub* stub, Condition cond) {
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET, cond);
}


bool MacroAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame_ || !stub->SometimesSetsUpAFrame();
}


void MacroAssembler::IndexFromHash(Register hash, Register index) {
  // If the hash field contains an array index pick it out. The assert checks
  // that the constants for the maximum number of digits for an array index
  // cached in the hash field and the number of bits reserved for it does not
  // conflict.
  DCHECK(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  DecodeFieldToSmi<String::ArrayIndexValueBits>(index, hash);
}


void MacroAssembler::SmiToDouble(LowDwVfpRegister value, Register smi) {
  if (CpuFeatures::IsSupported(VFP3)) {
    vmov(value.low(), smi);
    vcvt_f64_s32(value, 1);
  } else {
    SmiUntag(ip, smi);
    vmov(value.low(), ip);
    vcvt_f64_s32(value, value.low());
  }
}


void MacroAssembler::TestDoubleIsInt32(DwVfpRegister double_input,
                                       LowDwVfpRegister double_scratch) {
  DCHECK(!double_input.is(double_scratch));
  vcvt_s32_f64(double_scratch.low(), double_input);
  vcvt_f64_s32(double_scratch, double_scratch.low());
  VFPCompareAndSetFlags(double_input, double_scratch);
}


void MacroAssembler::TryDoubleToInt32Exact(Register result,
                                           DwVfpRegister double_input,
                                           LowDwVfpRegister double_scratch) {
  DCHECK(!double_input.is(double_scratch));
  vcvt_s32_f64(double_scratch.low(), double_input);
  vmov(result, double_scratch.low());
  vcvt_f64_s32(double_scratch, double_scratch.low());
  VFPCompareAndSetFlags(double_input, double_scratch);
}


void MacroAssembler::TryInt32Floor(Register result,
                                   DwVfpRegister double_input,
                                   Register input_high,
                                   LowDwVfpRegister double_scratch,
                                   Label* done,
                                   Label* exact) {
  DCHECK(!result.is(input_high));
  DCHECK(!double_input.is(double_scratch));
  Label negative, exception;

  VmovHigh(input_high, double_input);

  // Test for NaN and infinities.
  Sbfx(result, input_high,
       HeapNumber::kExponentShift, HeapNumber::kExponentBits);
  cmp(result, Operand(-1));
  b(eq, &exception);
  // Test for values that can be exactly represented as a
  // signed 32-bit integer.
  TryDoubleToInt32Exact(result, double_input, double_scratch);
  // If exact, return (result already fetched).
  b(eq, exact);
  cmp(input_high, Operand::Zero());
  b(mi, &negative);

  // Input is in ]+0, +inf[.
  // If result equals 0x7fffffff input was out of range or
  // in ]0x7fffffff, 0x80000000[. We ignore this last case which
  // could fits into an int32, that means we always think input was
  // out of range and always go to exception.
  // If result < 0x7fffffff, go to done, result fetched.
  cmn(result, Operand(1));
  b(mi, &exception);
  b(done);

  // Input is in ]-inf, -0[.
  // If x is a non integer negative number,
  // floor(x) <=> round_to_zero(x) - 1.
  bind(&negative);
  sub(result, result, Operand(1), SetCC);
  // If result is still negative, go to done, result fetched.
  // Else, we had an overflow and we fall through exception.
  b(mi, done);
  bind(&exception);
}

void MacroAssembler::TryInlineTruncateDoubleToI(Register result,
                                                DwVfpRegister double_input,
                                                Label* done) {
  LowDwVfpRegister double_scratch = kScratchDoubleReg;
  vcvt_s32_f64(double_scratch.low(), double_input);
  vmov(result, double_scratch.low());

  // If result is not saturated (0x7fffffff or 0x80000000), we are done.
  sub(ip, result, Operand(1));
  cmp(ip, Operand(0x7ffffffe));
  b(lt, done);
}


void MacroAssembler::TruncateDoubleToI(Register result,
                                       DwVfpRegister double_input) {
  Label done;

  TryInlineTruncateDoubleToI(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  push(lr);
  sub(sp, sp, Operand(kDoubleSize));  // Put input on stack.
  vstr(double_input, MemOperand(sp, 0));

  DoubleToIStub stub(isolate(), sp, result, 0, true, true);
  CallStub(&stub);

  add(sp, sp, Operand(kDoubleSize));
  pop(lr);

  bind(&done);
}


void MacroAssembler::TruncateHeapNumberToI(Register result,
                                           Register object) {
  Label done;
  LowDwVfpRegister double_scratch = kScratchDoubleReg;
  DCHECK(!result.is(object));

  vldr(double_scratch,
       MemOperand(object, HeapNumber::kValueOffset - kHeapObjectTag));
  TryInlineTruncateDoubleToI(result, double_scratch, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  push(lr);
  DoubleToIStub stub(isolate(),
                     object,
                     result,
                     HeapNumber::kValueOffset - kHeapObjectTag,
                     true,
                     true);
  CallStub(&stub);
  pop(lr);

  bind(&done);
}


void MacroAssembler::TruncateNumberToI(Register object,
                                       Register result,
                                       Register heap_number_map,
                                       Register scratch1,
                                       Label* not_number) {
  Label done;
  DCHECK(!result.is(object));

  UntagAndJumpIfSmi(result, object, &done);
  JumpIfNotHeapNumber(object, heap_number_map, scratch1, not_number);
  TruncateHeapNumberToI(result, object);

  bind(&done);
}


void MacroAssembler::GetLeastBitsFromSmi(Register dst,
                                         Register src,
                                         int num_least_bits) {
  if (CpuFeatures::IsSupported(ARMv7) && !predictable_code_size()) {
    ubfx(dst, src, kSmiTagSize, num_least_bits);
  } else {
    SmiUntag(dst, src);
    and_(dst, dst, Operand((1 << num_least_bits) - 1));
  }
}


void MacroAssembler::GetLeastBitsFromInt32(Register dst,
                                           Register src,
                                           int num_least_bits) {
  and_(dst, src, Operand((1 << num_least_bits) - 1));
}


void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments,
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
  mov(r1, Operand(ExternalReference(f, isolate())));
  CEntryStub stub(isolate(), 1, save_doubles);
  CallStub(&stub);
}


void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments) {
  mov(r0, Operand(num_arguments));
  mov(r1, Operand(ext));

  CEntryStub stub(isolate(), 1);
  CallStub(&stub);
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
  JumpToExternalReference(ExternalReference(fid, isolate()));
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin) {
#if defined(__thumb__)
  // Thumb mode builtin.
  DCHECK((reinterpret_cast<intptr_t>(builtin.address()) & 1) == 1);
#endif
  mov(r1, Operand(builtin));
  CEntryStub stub(isolate(), 1);
  Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::InvokeBuiltin(int native_context_index, InvokeFlag flag,
                                   const CallWrapper& call_wrapper) {
  // You can't call a builtin without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Fake a parameter count to avoid emitting code to do the check.
  ParameterCount expected(0);
  LoadNativeContextSlot(native_context_index, r1);
  InvokeFunctionCode(r1, no_reg, expected, expected, flag, call_wrapper);
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value,
                                Register scratch1, Register scratch2) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch1, Operand(value));
    mov(scratch2, Operand(ExternalReference(counter)));
    str(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch2, Operand(ExternalReference(counter)));
    ldr(scratch1, MemOperand(scratch2));
    add(scratch1, scratch1, Operand(value));
    str(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch2, Operand(ExternalReference(counter)));
    ldr(scratch1, MemOperand(scratch2));
    sub(scratch1, scratch1, Operand(value));
    str(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::Assert(Condition cond, BailoutReason reason) {
  if (emit_debug_code())
    Check(cond, reason);
}


void MacroAssembler::AssertFastElements(Register elements) {
  if (emit_debug_code()) {
    DCHECK(!elements.is(ip));
    Label ok;
    push(elements);
    ldr(elements, FieldMemOperand(elements, HeapObject::kMapOffset));
    LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
    cmp(elements, ip);
    b(eq, &ok);
    LoadRoot(ip, Heap::kFixedDoubleArrayMapRootIndex);
    cmp(elements, ip);
    b(eq, &ok);
    LoadRoot(ip, Heap::kFixedCOWArrayMapRootIndex);
    cmp(elements, ip);
    b(eq, &ok);
    Abort(kJSObjectWithFastElementsMapHasSlowElements);
    bind(&ok);
    pop(elements);
  }
}


void MacroAssembler::Check(Condition cond, BailoutReason reason) {
  Label L;
  b(cond, &L);
  Abort(reason);
  // will not return here
  bind(&L);
}


void MacroAssembler::Abort(BailoutReason reason) {
  Label abort_start;
  bind(&abort_start);
#ifdef DEBUG
  const char* msg = GetBailoutReason(reason);
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  if (FLAG_trap_on_abort) {
    stop(msg);
    return;
  }
#endif

  mov(r0, Operand(Smi::FromInt(reason)));
  push(r0);

  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    CallRuntime(Runtime::kAbort, 1);
  } else {
    CallRuntime(Runtime::kAbort, 1);
  }
  // will not return here
  if (is_const_pool_blocked()) {
    // If the calling code cares about the exact number of
    // instructions generated, we insert padding here to keep the size
    // of the Abort macro constant.
    static const int kExpectedAbortInstructions = 7;
    int abort_instructions = InstructionsGeneratedSince(&abort_start);
    DCHECK(abort_instructions <= kExpectedAbortInstructions);
    while (abort_instructions++ < kExpectedAbortInstructions) {
      nop();
    }
  }
}


void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    ldr(dst, MemOperand(cp, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    for (int i = 1; i < context_chain_length; i++) {
      ldr(dst, MemOperand(dst, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    }
  } else {
    // Slot is in the current function context.  Move it into the
    // destination register in case we store into it (the write barrier
    // cannot be allowed to destroy the context in esi).
    mov(dst, cp);
  }
}


void MacroAssembler::LoadTransitionedArrayMapConditional(
    ElementsKind expected_kind,
    ElementsKind transitioned_kind,
    Register map_in_out,
    Register scratch,
    Label* no_map_match) {
  DCHECK(IsFastElementsKind(expected_kind));
  DCHECK(IsFastElementsKind(transitioned_kind));

  // Check that the function's map is the same as the expected cached map.
  ldr(scratch, NativeContextMemOperand());
  ldr(ip, ContextMemOperand(scratch, Context::ArrayMapIndex(expected_kind)));
  cmp(map_in_out, ip);
  b(ne, no_map_match);

  // Use the transitioned cached map.
  ldr(map_in_out,
      ContextMemOperand(scratch, Context::ArrayMapIndex(transitioned_kind)));
}


void MacroAssembler::LoadNativeContextSlot(int index, Register dst) {
  ldr(dst, NativeContextMemOperand());
  ldr(dst, ContextMemOperand(dst, index));
}


void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map,
                                                  Register scratch) {
  // Load the initial map. The global functions all have initial maps.
  ldr(map, FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  if (emit_debug_code()) {
    Label ok, fail;
    CheckMap(map, scratch, Heap::kMetaMapRootIndex, &fail, DO_SMI_CHECK);
    b(&ok);
    bind(&fail);
    Abort(kGlobalFunctionsMustHaveInitialMap);
    bind(&ok);
  }
}


void MacroAssembler::JumpIfNotPowerOfTwoOrZero(
    Register reg,
    Register scratch,
    Label* not_power_of_two_or_zero) {
  sub(scratch, reg, Operand(1), SetCC);
  b(mi, not_power_of_two_or_zero);
  tst(scratch, reg);
  b(ne, not_power_of_two_or_zero);
}


void MacroAssembler::JumpIfNotPowerOfTwoOrZeroAndNeg(
    Register reg,
    Register scratch,
    Label* zero_and_neg,
    Label* not_power_of_two) {
  sub(scratch, reg, Operand(1), SetCC);
  b(mi, zero_and_neg);
  tst(scratch, reg);
  b(ne, not_power_of_two);
}


void MacroAssembler::JumpIfNotBothSmi(Register reg1,
                                      Register reg2,
                                      Label* on_not_both_smi) {
  STATIC_ASSERT(kSmiTag == 0);
  tst(reg1, Operand(kSmiTagMask));
  tst(reg2, Operand(kSmiTagMask), eq);
  b(ne, on_not_both_smi);
}


void MacroAssembler::UntagAndJumpIfSmi(
    Register dst, Register src, Label* smi_case) {
  STATIC_ASSERT(kSmiTag == 0);
  SmiUntag(dst, src, SetCC);
  b(cc, smi_case);  // Shifter carry is not set for a smi.
}


void MacroAssembler::UntagAndJumpIfNotSmi(
    Register dst, Register src, Label* non_smi_case) {
  STATIC_ASSERT(kSmiTag == 0);
  SmiUntag(dst, src, SetCC);
  b(cs, non_smi_case);  // Shifter carry is set for a non-smi.
}


void MacroAssembler::JumpIfEitherSmi(Register reg1,
                                     Register reg2,
                                     Label* on_either_smi) {
  STATIC_ASSERT(kSmiTag == 0);
  tst(reg1, Operand(kSmiTagMask));
  tst(reg2, Operand(kSmiTagMask), ne);
  b(eq, on_either_smi);
}


void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    tst(object, Operand(kSmiTagMask));
    Check(ne, kOperandIsASmi);
  }
}


void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    tst(object, Operand(kSmiTagMask));
    Check(eq, kOperandIsNotSmi);
  }
}


void MacroAssembler::AssertString(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    tst(object, Operand(kSmiTagMask));
    Check(ne, kOperandIsASmiAndNotAString);
    push(object);
    ldr(object, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(object, object, FIRST_NONSTRING_TYPE);
    pop(object);
    Check(lo, kOperandIsNotAString);
  }
}


void MacroAssembler::AssertName(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    tst(object, Operand(kSmiTagMask));
    Check(ne, kOperandIsASmiAndNotAName);
    push(object);
    ldr(object, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(object, object, LAST_NAME_TYPE);
    pop(object);
    Check(le, kOperandIsNotAName);
  }
}


void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    tst(object, Operand(kSmiTagMask));
    Check(ne, kOperandIsASmiAndNotAFunction);
    push(object);
    CompareObjectType(object, object, object, JS_FUNCTION_TYPE);
    pop(object);
    Check(eq, kOperandIsNotAFunction);
  }
}


void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    tst(object, Operand(kSmiTagMask));
    Check(ne, kOperandIsASmiAndNotABoundFunction);
    push(object);
    CompareObjectType(object, object, object, JS_BOUND_FUNCTION_TYPE);
    pop(object);
    Check(eq, kOperandIsNotABoundFunction);
  }
}


void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    CompareRoot(object, Heap::kUndefinedValueRootIndex);
    b(eq, &done_checking);
    ldr(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareRoot(scratch, Heap::kAllocationSiteMapRootIndex);
    Assert(eq, kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}


void MacroAssembler::AssertIsRoot(Register reg, Heap::RootListIndex index) {
  if (emit_debug_code()) {
    CompareRoot(reg, index);
    Check(eq, kHeapNumberMapRegisterClobbered);
  }
}


void MacroAssembler::JumpIfNotHeapNumber(Register object,
                                         Register heap_number_map,
                                         Register scratch,
                                         Label* on_not_heap_number) {
  ldr(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  AssertIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  cmp(scratch, heap_number_map);
  b(ne, on_not_heap_number);
}


void MacroAssembler::JumpIfNonSmisNotBothSequentialOneByteStrings(
    Register first, Register second, Register scratch1, Register scratch2,
    Label* failure) {
  // Test that both first and second are sequential one-byte strings.
  // Assume that they are non-smis.
  ldr(scratch1, FieldMemOperand(first, HeapObject::kMapOffset));
  ldr(scratch2, FieldMemOperand(second, HeapObject::kMapOffset));
  ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  ldrb(scratch2, FieldMemOperand(scratch2, Map::kInstanceTypeOffset));

  JumpIfBothInstanceTypesAreNotSequentialOneByte(scratch1, scratch2, scratch1,
                                                 scratch2, failure);
}

void MacroAssembler::JumpIfNotBothSequentialOneByteStrings(Register first,
                                                           Register second,
                                                           Register scratch1,
                                                           Register scratch2,
                                                           Label* failure) {
  // Check that neither is a smi.
  and_(scratch1, first, Operand(second));
  JumpIfSmi(scratch1, failure);
  JumpIfNonSmisNotBothSequentialOneByteStrings(first, second, scratch1,
                                               scratch2, failure);
}


void MacroAssembler::JumpIfNotUniqueNameInstanceType(Register reg,
                                                     Label* not_unique_name) {
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  Label succeed;
  tst(reg, Operand(kIsNotStringMask | kIsNotInternalizedMask));
  b(eq, &succeed);
  cmp(reg, Operand(SYMBOL_TYPE));
  b(ne, not_unique_name);

  bind(&succeed);
}


// Allocates a heap number or jumps to the need_gc label if the young space
// is full and a scavenge is needed.
void MacroAssembler::AllocateHeapNumber(Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Register heap_number_map,
                                        Label* gc_required,
                                        TaggingMode tagging_mode,
                                        MutableMode mode) {
  // Allocate an object in the heap for the heap number and tag it as a heap
  // object.
  Allocate(HeapNumber::kSize, result, scratch1, scratch2, gc_required,
           tagging_mode == TAG_RESULT ? TAG_OBJECT : NO_ALLOCATION_FLAGS);

  Heap::RootListIndex map_index = mode == MUTABLE
      ? Heap::kMutableHeapNumberMapRootIndex
      : Heap::kHeapNumberMapRootIndex;
  AssertIsRoot(heap_number_map, map_index);

  // Store heap number map in the allocated object.
  if (tagging_mode == TAG_RESULT) {
    str(heap_number_map, FieldMemOperand(result, HeapObject::kMapOffset));
  } else {
    str(heap_number_map, MemOperand(result, HeapObject::kMapOffset));
  }
}


void MacroAssembler::AllocateHeapNumberWithValue(Register result,
                                                 DwVfpRegister value,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Register heap_number_map,
                                                 Label* gc_required) {
  AllocateHeapNumber(result, scratch1, scratch2, heap_number_map, gc_required);
  sub(scratch1, result, Operand(kHeapObjectTag));
  vstr(value, scratch1, HeapNumber::kValueOffset);
}


void MacroAssembler::AllocateJSValue(Register result, Register constructor,
                                     Register value, Register scratch1,
                                     Register scratch2, Label* gc_required) {
  DCHECK(!result.is(constructor));
  DCHECK(!result.is(scratch1));
  DCHECK(!result.is(scratch2));
  DCHECK(!result.is(value));

  // Allocate JSValue in new space.
  Allocate(JSValue::kSize, result, scratch1, scratch2, gc_required, TAG_OBJECT);

  // Initialize the JSValue.
  LoadGlobalFunctionInitialMap(constructor, scratch1, scratch2);
  str(scratch1, FieldMemOperand(result, HeapObject::kMapOffset));
  LoadRoot(scratch1, Heap::kEmptyFixedArrayRootIndex);
  str(scratch1, FieldMemOperand(result, JSObject::kPropertiesOffset));
  str(scratch1, FieldMemOperand(result, JSObject::kElementsOffset));
  str(value, FieldMemOperand(result, JSValue::kValueOffset));
  STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);
}


void MacroAssembler::CopyBytes(Register src,
                               Register dst,
                               Register length,
                               Register scratch) {
  Label align_loop_1, word_loop, byte_loop, byte_loop_1, done;

  // Align src before copying in word size chunks.
  cmp(length, Operand(kPointerSize));
  b(le, &byte_loop);

  bind(&align_loop_1);
  tst(src, Operand(kPointerSize - 1));
  b(eq, &word_loop);
  ldrb(scratch, MemOperand(src, 1, PostIndex));
  strb(scratch, MemOperand(dst, 1, PostIndex));
  sub(length, length, Operand(1), SetCC);
  b(&align_loop_1);
  // Copy bytes in word size chunks.
  bind(&word_loop);
  if (emit_debug_code()) {
    tst(src, Operand(kPointerSize - 1));
    Assert(eq, kExpectingAlignmentForCopyBytes);
  }
  cmp(length, Operand(kPointerSize));
  b(lt, &byte_loop);
  ldr(scratch, MemOperand(src, kPointerSize, PostIndex));
  if (CpuFeatures::IsSupported(UNALIGNED_ACCESSES)) {
    str(scratch, MemOperand(dst, kPointerSize, PostIndex));
  } else {
    strb(scratch, MemOperand(dst, 1, PostIndex));
    mov(scratch, Operand(scratch, LSR, 8));
    strb(scratch, MemOperand(dst, 1, PostIndex));
    mov(scratch, Operand(scratch, LSR, 8));
    strb(scratch, MemOperand(dst, 1, PostIndex));
    mov(scratch, Operand(scratch, LSR, 8));
    strb(scratch, MemOperand(dst, 1, PostIndex));
  }
  sub(length, length, Operand(kPointerSize));
  b(&word_loop);

  // Copy the last bytes if any left.
  bind(&byte_loop);
  cmp(length, Operand::Zero());
  b(eq, &done);
  bind(&byte_loop_1);
  ldrb(scratch, MemOperand(src, 1, PostIndex));
  strb(scratch, MemOperand(dst, 1, PostIndex));
  sub(length, length, Operand(1), SetCC);
  b(ne, &byte_loop_1);
  bind(&done);
}


void MacroAssembler::InitializeFieldsWithFiller(Register current_address,
                                                Register end_address,
                                                Register filler) {
  Label loop, entry;
  b(&entry);
  bind(&loop);
  str(filler, MemOperand(current_address, kPointerSize, PostIndex));
  bind(&entry);
  cmp(current_address, end_address);
  b(lo, &loop);
}


void MacroAssembler::CheckFor32DRegs(Register scratch) {
  mov(scratch, Operand(ExternalReference::cpu_features()));
  ldr(scratch, MemOperand(scratch));
  tst(scratch, Operand(1u << VFP32DREGS));
}


void MacroAssembler::SaveFPRegs(Register location, Register scratch) {
  CheckFor32DRegs(scratch);
  vstm(db_w, location, d16, d31, ne);
  sub(location, location, Operand(16 * kDoubleSize), LeaveCC, eq);
  vstm(db_w, location, d0, d15);
}


void MacroAssembler::RestoreFPRegs(Register location, Register scratch) {
  CheckFor32DRegs(scratch);
  vldm(ia_w, location, d0, d15);
  vldm(ia_w, location, d16, d31, ne);
  add(location, location, Operand(16 * kDoubleSize), LeaveCC, eq);
}


void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialOneByte(
    Register first, Register second, Register scratch1, Register scratch2,
    Label* failure) {
  const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatOneByteStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  and_(scratch1, first, Operand(kFlatOneByteStringMask));
  and_(scratch2, second, Operand(kFlatOneByteStringMask));
  cmp(scratch1, Operand(kFlatOneByteStringTag));
  // Ignore second test if first test failed.
  cmp(scratch2, Operand(kFlatOneByteStringTag), eq);
  b(ne, failure);
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialOneByte(Register type,
                                                              Register scratch,
                                                              Label* failure) {
  const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatOneByteStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  and_(scratch, type, Operand(kFlatOneByteStringMask));
  cmp(scratch, Operand(kFlatOneByteStringTag));
  b(ne, failure);
}

static const int kRegisterPassedArguments = 4;


int MacroAssembler::CalculateStackPassedWords(int num_reg_arguments,
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


void MacroAssembler::EmitSeqStringSetCharCheck(Register string,
                                               Register index,
                                               Register value,
                                               uint32_t encoding_mask) {
  Label is_object;
  SmiTst(string);
  Check(ne, kNonObject);

  ldr(ip, FieldMemOperand(string, HeapObject::kMapOffset));
  ldrb(ip, FieldMemOperand(ip, Map::kInstanceTypeOffset));

  and_(ip, ip, Operand(kStringRepresentationMask | kStringEncodingMask));
  cmp(ip, Operand(encoding_mask));
  Check(eq, kUnexpectedStringType);

  // The index is assumed to be untagged coming in, tag it to compare with the
  // string length without using a temp register, it is restored at the end of
  // this function.
  Label index_tag_ok, index_tag_bad;
  TrySmiTag(index, index, &index_tag_bad);
  b(&index_tag_ok);
  bind(&index_tag_bad);
  Abort(kIndexIsTooLarge);
  bind(&index_tag_ok);

  ldr(ip, FieldMemOperand(string, String::kLengthOffset));
  cmp(index, ip);
  Check(lt, kIndexIsTooLarge);

  cmp(index, Operand(Smi::FromInt(0)));
  Check(ge, kIndexIsNegative);

  SmiUntag(index, index);
}


void MacroAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          int num_double_arguments,
                                          Register scratch) {
  int frame_alignment = ActivationFrameAlignment();
  int stack_passed_arguments = CalculateStackPassedWords(
      num_reg_arguments, num_double_arguments);
  if (frame_alignment > kPointerSize) {
    // Make stack end at alignment and make room for num_arguments - 4 words
    // and the original value of sp.
    mov(scratch, sp);
    sub(sp, sp, Operand((stack_passed_arguments + 1) * kPointerSize));
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
    and_(sp, sp, Operand(-frame_alignment));
    str(scratch, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    sub(sp, sp, Operand(stack_passed_arguments * kPointerSize));
  }
}


void MacroAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          Register scratch) {
  PrepareCallCFunction(num_reg_arguments, 0, scratch);
}


void MacroAssembler::MovToFloatParameter(DwVfpRegister src) {
  DCHECK(src.is(d0));
  if (!use_eabi_hardfloat()) {
    vmov(r0, r1, src);
  }
}


// On ARM this is just a synonym to make the purpose clear.
void MacroAssembler::MovToFloatResult(DwVfpRegister src) {
  MovToFloatParameter(src);
}


void MacroAssembler::MovToFloatParameters(DwVfpRegister src1,
                                          DwVfpRegister src2) {
  DCHECK(src1.is(d0));
  DCHECK(src2.is(d1));
  if (!use_eabi_hardfloat()) {
    vmov(r0, r1, src1);
    vmov(r2, r3, src2);
  }
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  mov(ip, Operand(function));
  CallCFunctionHelper(ip, num_reg_arguments, num_double_arguments);
}


void MacroAssembler::CallCFunction(Register function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  CallCFunctionHelper(function, num_reg_arguments, num_double_arguments);
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}


void MacroAssembler::CallCFunction(Register function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}


void MacroAssembler::CallCFunctionHelper(Register function,
                                         int num_reg_arguments,
                                         int num_double_arguments) {
  DCHECK(has_frame());
  // Make sure that the stack is aligned before calling a C function unless
  // running in the simulator. The simulator has its own alignment check which
  // provides more information.
#if V8_HOST_ARCH_ARM
  if (emit_debug_code()) {
    int frame_alignment = base::OS::ActivationFrameAlignment();
    int frame_alignment_mask = frame_alignment - 1;
    if (frame_alignment > kPointerSize) {
      DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
      Label alignment_as_expected;
      tst(sp, Operand(frame_alignment_mask));
      b(eq, &alignment_as_expected);
      // Don't use Check here, as it will call Runtime_Abort possibly
      // re-entering here.
      stop("Unexpected alignment");
      bind(&alignment_as_expected);
    }
  }
#endif

  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.
  Call(function);
  int stack_passed_arguments = CalculateStackPassedWords(
      num_reg_arguments, num_double_arguments);
  if (ActivationFrameAlignment() > kPointerSize) {
    ldr(sp, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    add(sp, sp, Operand(stack_passed_arguments * kPointerSize));
  }
}


void MacroAssembler::CheckPageFlag(
    Register object,
    Register scratch,
    int mask,
    Condition cc,
    Label* condition_met) {
  Bfc(scratch, object, 0, kPageSizeBits);
  ldr(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
  tst(scratch, Operand(mask));
  b(cc, condition_met);
}


void MacroAssembler::JumpIfBlack(Register object,
                                 Register scratch0,
                                 Register scratch1,
                                 Label* on_black) {
  HasColor(object, scratch0, scratch1, on_black, 1, 1);  // kBlackBitPattern.
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
}


void MacroAssembler::HasColor(Register object,
                              Register bitmap_scratch,
                              Register mask_scratch,
                              Label* has_color,
                              int first_bit,
                              int second_bit) {
  DCHECK(!AreAliased(object, bitmap_scratch, mask_scratch, no_reg));

  GetMarkBits(object, bitmap_scratch, mask_scratch);

  Label other_color, word_boundary;
  ldr(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  tst(ip, Operand(mask_scratch));
  b(first_bit == 1 ? eq : ne, &other_color);
  // Shift left 1 by adding.
  add(mask_scratch, mask_scratch, Operand(mask_scratch), SetCC);
  b(eq, &word_boundary);
  tst(ip, Operand(mask_scratch));
  b(second_bit == 1 ? ne : eq, has_color);
  jmp(&other_color);

  bind(&word_boundary);
  ldr(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize + kPointerSize));
  tst(ip, Operand(1));
  b(second_bit == 1 ? ne : eq, has_color);
  bind(&other_color);
}


void MacroAssembler::GetMarkBits(Register addr_reg,
                                 Register bitmap_reg,
                                 Register mask_reg) {
  DCHECK(!AreAliased(addr_reg, bitmap_reg, mask_reg, no_reg));
  and_(bitmap_reg, addr_reg, Operand(~Page::kPageAlignmentMask));
  Ubfx(mask_reg, addr_reg, kPointerSizeLog2, Bitmap::kBitsPerCellLog2);
  const int kLowBits = kPointerSizeLog2 + Bitmap::kBitsPerCellLog2;
  Ubfx(ip, addr_reg, kLowBits, kPageSizeBits - kLowBits);
  add(bitmap_reg, bitmap_reg, Operand(ip, LSL, kPointerSizeLog2));
  mov(ip, Operand(1));
  mov(mask_reg, Operand(ip, LSL, mask_reg));
}


void MacroAssembler::JumpIfWhite(Register value, Register bitmap_scratch,
                                 Register mask_scratch, Register load_scratch,
                                 Label* value_is_white) {
  DCHECK(!AreAliased(value, bitmap_scratch, mask_scratch, ip));
  GetMarkBits(value, bitmap_scratch, mask_scratch);

  // If the value is black or grey we don't need to do anything.
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  ldr(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  tst(mask_scratch, load_scratch);
  b(eq, value_is_white);
}


void MacroAssembler::ClampUint8(Register output_reg, Register input_reg) {
  Usat(output_reg, 8, Operand(input_reg));
}


void MacroAssembler::ClampDoubleToUint8(Register result_reg,
                                        DwVfpRegister input_reg,
                                        LowDwVfpRegister double_scratch) {
  Label done;

  // Handle inputs >= 255 (including +infinity).
  Vmov(double_scratch, 255.0, result_reg);
  mov(result_reg, Operand(255));
  VFPCompareAndSetFlags(input_reg, double_scratch);
  b(ge, &done);

  // For inputs < 255 (including negative) vcvt_u32_f64 with round-to-nearest
  // rounding mode will provide the correct result.
  vcvt_u32_f64(double_scratch.low(), input_reg, kFPSCRRounding);
  vmov(result_reg, double_scratch.low());

  bind(&done);
}


void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  ldr(descriptors, FieldMemOperand(map, Map::kDescriptorsOffset));
}


void MacroAssembler::NumberOfOwnDescriptors(Register dst, Register map) {
  ldr(dst, FieldMemOperand(map, Map::kBitField3Offset));
  DecodeField<Map::NumberOfOwnDescriptorsBits>(dst);
}


void MacroAssembler::EnumLength(Register dst, Register map) {
  STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
  ldr(dst, FieldMemOperand(map, Map::kBitField3Offset));
  and_(dst, dst, Operand(Map::EnumLengthBits::kMask));
  SmiTag(dst);
}


void MacroAssembler::LoadAccessor(Register dst, Register holder,
                                  int accessor_index,
                                  AccessorComponent accessor) {
  ldr(dst, FieldMemOperand(holder, HeapObject::kMapOffset));
  LoadInstanceDescriptors(dst, dst);
  ldr(dst,
      FieldMemOperand(dst, DescriptorArray::GetValueOffset(accessor_index)));
  int offset = accessor == ACCESSOR_GETTER ? AccessorPair::kGetterOffset
                                           : AccessorPair::kSetterOffset;
  ldr(dst, FieldMemOperand(dst, offset));
}


void MacroAssembler::CheckEnumCache(Register null_value, Label* call_runtime) {
  Register  empty_fixed_array_value = r6;
  LoadRoot(empty_fixed_array_value, Heap::kEmptyFixedArrayRootIndex);
  Label next, start;
  mov(r2, r0);

  // Check if the enum length field is properly initialized, indicating that
  // there is an enum cache.
  ldr(r1, FieldMemOperand(r2, HeapObject::kMapOffset));

  EnumLength(r3, r1);
  cmp(r3, Operand(Smi::FromInt(kInvalidEnumCacheSentinel)));
  b(eq, call_runtime);

  jmp(&start);

  bind(&next);
  ldr(r1, FieldMemOperand(r2, HeapObject::kMapOffset));

  // For all objects but the receiver, check that the cache is empty.
  EnumLength(r3, r1);
  cmp(r3, Operand(Smi::FromInt(0)));
  b(ne, call_runtime);

  bind(&start);

  // Check that there are no elements. Register r2 contains the current JS
  // object we've reached through the prototype chain.
  Label no_elements;
  ldr(r2, FieldMemOperand(r2, JSObject::kElementsOffset));
  cmp(r2, empty_fixed_array_value);
  b(eq, &no_elements);

  // Second chance, the object may be using the empty slow element dictionary.
  CompareRoot(r2, Heap::kEmptySlowElementDictionaryRootIndex);
  b(ne, call_runtime);

  bind(&no_elements);
  ldr(r2, FieldMemOperand(r1, Map::kPrototypeOffset));
  cmp(r2, null_value);
  b(ne, &next);
}


void MacroAssembler::TestJSArrayForAllocationMemento(
    Register receiver_reg,
    Register scratch_reg,
    Label* no_memento_found) {
  ExternalReference new_space_start =
      ExternalReference::new_space_start(isolate());
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());
  add(scratch_reg, receiver_reg,
      Operand(JSArray::kSize + AllocationMemento::kSize - kHeapObjectTag));
  cmp(scratch_reg, Operand(new_space_start));
  b(lt, no_memento_found);
  mov(ip, Operand(new_space_allocation_top));
  ldr(ip, MemOperand(ip));
  cmp(scratch_reg, ip);
  b(gt, no_memento_found);
  ldr(scratch_reg, MemOperand(scratch_reg, -AllocationMemento::kSize));
  cmp(scratch_reg,
      Operand(isolate()->factory()->allocation_memento_map()));
}


Register GetRegisterThatIsNotOneOf(Register reg1,
                                   Register reg2,
                                   Register reg3,
                                   Register reg4,
                                   Register reg5,
                                   Register reg6) {
  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();

  const RegisterConfiguration* config =
      RegisterConfiguration::ArchDefault(RegisterConfiguration::CRANKSHAFT);
  for (int i = 0; i < config->num_allocatable_general_registers(); ++i) {
    int code = config->GetAllocatableGeneralCode(i);
    Register candidate = Register::from_code(code);
    if (regs & candidate.bit()) continue;
    return candidate;
  }
  UNREACHABLE();
  return no_reg;
}


void MacroAssembler::JumpIfDictionaryInPrototypeChain(
    Register object,
    Register scratch0,
    Register scratch1,
    Label* found) {
  DCHECK(!scratch1.is(scratch0));
  Register current = scratch0;
  Label loop_again, end;

  // scratch contained elements pointer.
  mov(current, object);
  ldr(current, FieldMemOperand(current, HeapObject::kMapOffset));
  ldr(current, FieldMemOperand(current, Map::kPrototypeOffset));
  CompareRoot(current, Heap::kNullValueRootIndex);
  b(eq, &end);

  // Loop based on the map going up the prototype chain.
  bind(&loop_again);
  ldr(current, FieldMemOperand(current, HeapObject::kMapOffset));

  STATIC_ASSERT(JS_PROXY_TYPE < JS_OBJECT_TYPE);
  STATIC_ASSERT(JS_VALUE_TYPE < JS_OBJECT_TYPE);
  ldrb(scratch1, FieldMemOperand(current, Map::kInstanceTypeOffset));
  cmp(scratch1, Operand(JS_OBJECT_TYPE));
  b(lo, found);

  ldr(scratch1, FieldMemOperand(current, Map::kBitField2Offset));
  DecodeField<Map::ElementsKindBits>(scratch1);
  cmp(scratch1, Operand(DICTIONARY_ELEMENTS));
  b(eq, found);
  ldr(current, FieldMemOperand(current, Map::kPrototypeOffset));
  CompareRoot(current, Heap::kNullValueRootIndex);
  b(ne, &loop_again);

  bind(&end);
}


#ifdef DEBUG
bool AreAliased(Register reg1,
                Register reg2,
                Register reg3,
                Register reg4,
                Register reg5,
                Register reg6,
                Register reg7,
                Register reg8) {
  int n_of_valid_regs = reg1.is_valid() + reg2.is_valid() +
      reg3.is_valid() + reg4.is_valid() + reg5.is_valid() + reg6.is_valid() +
      reg7.is_valid() + reg8.is_valid();

  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();
  if (reg7.is_valid()) regs |= reg7.bit();
  if (reg8.is_valid()) regs |= reg8.bit();
  int n_of_non_aliasing_regs = NumRegs(regs);

  return n_of_valid_regs != n_of_non_aliasing_regs;
}
#endif


CodePatcher::CodePatcher(Isolate* isolate, byte* address, int instructions,
                         FlushICache flush_cache)
    : address_(address),
      size_(instructions * Assembler::kInstrSize),
      masm_(isolate, address, size_ + Assembler::kGap, CodeObjectRequired::kNo),
      flush_cache_(flush_cache) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  if (flush_cache_ == FLUSH) {
    Assembler::FlushICache(masm_.isolate(), address_, size_);
  }

  // Check that the code was patched as expected.
  DCHECK(masm_.pc_ == address_ + size_);
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


void CodePatcher::Emit(Instr instr) {
  masm()->emit(instr);
}


void CodePatcher::Emit(Address addr) {
  masm()->emit(reinterpret_cast<Instr>(addr));
}


void CodePatcher::EmitCondition(Condition cond) {
  Instr instr = Assembler::instr_at(masm_.pc_);
  instr = (instr & ~kCondMask) | cond;
  masm_.emit(instr);
}


void MacroAssembler::TruncatingDiv(Register result,
                                   Register dividend,
                                   int32_t divisor) {
  DCHECK(!dividend.is(result));
  DCHECK(!dividend.is(ip));
  DCHECK(!result.is(ip));
  base::MagicNumbersForDivision<uint32_t> mag =
      base::SignedDivisionByConstant(bit_cast<uint32_t>(divisor));
  mov(ip, Operand(mag.multiplier));
  bool neg = (mag.multiplier & (1U << 31)) != 0;
  if (divisor > 0 && neg) {
    smmla(result, dividend, ip, dividend);
  } else {
    smmul(result, dividend, ip);
    if (divisor < 0 && !neg && mag.multiplier > 0) {
      sub(result, result, Operand(dividend));
    }
  }
  if (mag.shift > 0) mov(result, Operand(result, ASR, mag.shift));
  add(result, result, Operand(dividend, LSR, 31));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

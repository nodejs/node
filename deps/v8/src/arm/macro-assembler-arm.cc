// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <limits.h>  // For LONG_MIN, LONG_MAX.

#include "v8.h"

#if V8_TARGET_ARCH_ARM

#include "bootstrapper.h"
#include "codegen.h"
#include "cpu-profiler.h"
#include "debug.h"
#include "isolate-inl.h"
#include "runtime.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* arg_isolate, void* buffer, int size)
    : Assembler(arg_isolate, buffer, size),
      generating_stub_(false),
      has_frame_(false) {
  if (isolate() != NULL) {
    code_object_ = Handle<Object>(isolate()->heap()->undefined_value(),
                                  isolate());
  }
}


void MacroAssembler::Jump(Register target, Condition cond) {
  bx(target, cond);
}


void MacroAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond) {
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  mov(pc, Operand(target, rmode), LeaveCC, cond);
}


void MacroAssembler::Jump(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  ASSERT(!RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(target), rmode, cond);
}


void MacroAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  ASSERT(RelocInfo::IsCodeTarget(rmode));
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
  ASSERT_EQ(CallSize(target, cond), SizeOfCodeGeneratedSince(&start));
}


int MacroAssembler::CallSize(
    Address target, RelocInfo::Mode rmode, Condition cond) {
  int size = 2 * kInstrSize;
  Instr mov_instr = cond | MOV | LeaveCC;
  intptr_t immediate = reinterpret_cast<intptr_t>(target);
  if (!Operand(immediate, rmode).is_single_instruction(this, mov_instr)) {
    size += kInstrSize;
  }
  return size;
}


int MacroAssembler::CallSizeNotPredictableCodeSize(
    Address target, RelocInfo::Mode rmode, Condition cond) {
  int size = 2 * kInstrSize;
  Instr mov_instr = cond | MOV | LeaveCC;
  intptr_t immediate = reinterpret_cast<intptr_t>(target);
  if (!Operand(immediate, rmode).is_single_instruction(NULL, mov_instr)) {
    size += kInstrSize;
  }
  return size;
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

  ASSERT_EQ(expected_size, SizeOfCodeGeneratedSince(&start));
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
  ASSERT(RelocInfo::IsCodeTarget(rmode));
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
    ASSERT(value->IsHeapObject());
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


void MacroAssembler::And(Register dst, Register src1, const Operand& src2,
                         Condition cond) {
  if (!src2.is_reg() &&
      !src2.must_output_reloc_info(this) &&
      src2.immediate() == 0) {
    mov(dst, Operand::Zero(), LeaveCC, cond);
  } else if (!src2.is_single_instruction(this) &&
             !src2.must_output_reloc_info(this) &&
             CpuFeatures::IsSupported(ARMv7) &&
             IsPowerOf2(src2.immediate() + 1)) {
    ubfx(dst, src1, 0,
        WhichPowerOf2(static_cast<uint32_t>(src2.immediate()) + 1), cond);
  } else {
    and_(dst, src1, src2, LeaveCC, cond);
  }
}


void MacroAssembler::Ubfx(Register dst, Register src1, int lsb, int width,
                          Condition cond) {
  ASSERT(lsb < 32);
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
  ASSERT(lsb < 32);
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
  ASSERT(0 <= lsb && lsb < 32);
  ASSERT(0 <= width && width < 32);
  ASSERT(lsb + width < 32);
  ASSERT(!scratch.is(dst));
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
  ASSERT(lsb < 32);
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
    ASSERT(!dst.is(pc) && !src.rm().is(pc));
    ASSERT((satpos >= 0) && (satpos <= 31));

    // These asserts are required to ensure compatibility with the ARMv7
    // implementation.
    ASSERT((src.shift_op() == ASR) || (src.shift_op() == LSL));
    ASSERT(src.rs().is(no_reg));

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
  ASSERT(!r.IsDouble());
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
  ASSERT(!r.IsDouble());
  if (r.IsInteger8() || r.IsUInteger8()) {
    strb(src, dst);
  } else if (r.IsInteger16() || r.IsUInteger16()) {
    strh(src, dst);
  } else {
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
    Handle<Object> root(&isolate()->heap()->roots_array_start()[index]);
    mov(destination, Operand(root), LeaveCC, cond);
    return;
  }
  ldr(destination, MemOperand(kRootRegister, index << kPointerSizeLog2), cond);
}


void MacroAssembler::StoreRoot(Register source,
                               Heap::RootListIndex index,
                               Condition cond) {
  str(source, MemOperand(kRootRegister, index << kPointerSizeLog2), cond);
}


void MacroAssembler::InNewSpace(Register object,
                                Register scratch,
                                Condition cond,
                                Label* branch) {
  ASSERT(cond == eq || cond == ne);
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
  ASSERT(IsAligned(offset, kPointerSize));

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
              OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Operand(BitCast<int32_t>(kZapValue + 4)));
    mov(dst, Operand(BitCast<int32_t>(kZapValue + 8)));
  }
}


// Will clobber 4 registers: object, address, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(Register object,
                                 Register address,
                                 Register value,
                                 LinkRegisterStatus lr_status,
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  ASSERT(!object.is(value));
  if (emit_debug_code()) {
    ldr(ip, MemOperand(address));
    cmp(ip, value);
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  // TODO(mstarzinger): Dynamic counter missing.

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask,
                eq,
                &done);
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask,
                eq,
                &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    push(lr);
  }
  RecordWriteStub stub(object, value, address, remembered_set_action, fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(lr);
  }

  bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(address, Operand(BitCast<int32_t>(kZapValue + 12)));
    mov(value, Operand(BitCast<int32_t>(kZapValue + 16)));
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
    ASSERT(and_then == kReturnAtEnd);
    Ret(eq);
  }
  push(lr);
  StoreBufferOverflowStub store_buffer_overflow =
      StoreBufferOverflowStub(fp_mode);
  CallStub(&store_buffer_overflow);
  pop(lr);
  bind(&done);
  if (and_then == kReturnAtEnd) {
    Ret();
  }
}


void MacroAssembler::PushFixedFrame(Register marker_reg) {
  ASSERT(!marker_reg.is_valid() || marker_reg.code() < cp.code());
  stm(db_w, sp, (marker_reg.is_valid() ? marker_reg.bit() : 0) |
                cp.bit() |
                (FLAG_enable_ool_constant_pool ? pp.bit() : 0) |
                fp.bit() |
                lr.bit());
}


void MacroAssembler::PopFixedFrame(Register marker_reg) {
  ASSERT(!marker_reg.is_valid() || marker_reg.code() < cp.code());
  ldm(ia_w, sp, (marker_reg.is_valid() ? marker_reg.bit() : 0) |
                cp.bit() |
                (FLAG_enable_ool_constant_pool ? pp.bit() : 0) |
                fp.bit() |
                lr.bit());
}


// Push and pop all registers that can hold pointers.
void MacroAssembler::PushSafepointRegisters() {
  // Safepoints expect a block of contiguous register values starting with r0:
  ASSERT(((1 << kNumSafepointSavedRegisters) - 1) == kSafepointSavedRegisters);
  // Safepoints expect a block of kNumSafepointRegisters values on the
  // stack, so adjust the stack for unsaved registers.
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  ASSERT(num_unsaved >= 0);
  sub(sp, sp, Operand(num_unsaved * kPointerSize));
  stm(db_w, sp, kSafepointSavedRegisters);
}


void MacroAssembler::PopSafepointRegisters() {
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  ldm(ia_w, sp, kSafepointSavedRegisters);
  add(sp, sp, Operand(num_unsaved * kPointerSize));
}


void MacroAssembler::PushSafepointRegistersAndDoubles() {
  // Number of d-regs not known at snapshot time.
  ASSERT(!Serializer::enabled());
  PushSafepointRegisters();
  // Only save allocatable registers.
  ASSERT(kScratchDoubleReg.is(d15) && kDoubleRegZero.is(d14));
  ASSERT(DwVfpRegister::NumReservedRegisters() == 2);
  if (CpuFeatures::IsSupported(VFP32DREGS)) {
    vstm(db_w, sp, d16, d31);
  }
  vstm(db_w, sp, d0, d13);
}


void MacroAssembler::PopSafepointRegistersAndDoubles() {
  // Number of d-regs not known at snapshot time.
  ASSERT(!Serializer::enabled());
  // Only save allocatable registers.
  ASSERT(kScratchDoubleReg.is(d15) && kDoubleRegZero.is(d14));
  ASSERT(DwVfpRegister::NumReservedRegisters() == 2);
  vldm(ia_w, sp, d0, d13);
  if (CpuFeatures::IsSupported(VFP32DREGS)) {
    vldm(ia_w, sp, d16, d31);
  }
  PopSafepointRegisters();
}

void MacroAssembler::StoreToSafepointRegistersAndDoublesSlot(Register src,
                                                             Register dst) {
  str(src, SafepointRegistersAndDoublesSlot(dst));
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
  ASSERT(reg_code >= 0 && reg_code < kNumSafepointRegisters);
  return reg_code;
}


MemOperand MacroAssembler::SafepointRegisterSlot(Register reg) {
  return MemOperand(sp, SafepointRegisterStackIndex(reg.code()) * kPointerSize);
}


MemOperand MacroAssembler::SafepointRegistersAndDoublesSlot(Register reg) {
  // Number of d-regs not known at snapshot time.
  ASSERT(!Serializer::enabled());
  // General purpose registers are pushed last on the stack.
  int doubles_size = DwVfpRegister::NumAllocatableRegisters() * kDoubleSize;
  int register_offset = SafepointRegisterStackIndex(reg.code()) * kPointerSize;
  return MemOperand(sp, doubles_size + register_offset);
}


void MacroAssembler::Ldrd(Register dst1, Register dst2,
                          const MemOperand& src, Condition cond) {
  ASSERT(src.rm().is(no_reg));
  ASSERT(!dst1.is(lr));  // r14.

  // V8 does not use this addressing mode, so the fallback code
  // below doesn't support it yet.
  ASSERT((src.am() != PreIndex) && (src.am() != NegPreIndex));

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
      ASSERT((src.am() == PostIndex) || (src.am() == NegPostIndex));
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
  ASSERT(dst.rm().is(no_reg));
  ASSERT(!src1.is(lr));  // r14.

  // V8 does not use this addressing mode, so the fallback code
  // below doesn't support it yet.
  ASSERT((dst.am() != PreIndex) && (dst.am() != NegPreIndex));

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
      ASSERT((dst.am() == PostIndex) || (dst.am() == NegPostIndex));
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


void MacroAssembler::LoadConstantPoolPointerRegister() {
  if (FLAG_enable_ool_constant_pool) {
    int constant_pool_offset = Code::kConstantPoolOffset - Code::kHeaderSize -
        pc_offset() - Instruction::kPCReadOffset;
    ASSERT(ImmediateFitsAddrMode2Instruction(constant_pool_offset));
    ldr(pp, MemOperand(pc, constant_pool_offset));
  }
}


void MacroAssembler::Prologue(PrologueFrameMode frame_mode) {
  if (frame_mode == BUILD_STUB_FRAME) {
    PushFixedFrame();
    Push(Smi::FromInt(StackFrame::STUB));
    // Adjust FP to point to saved FP.
    add(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));
  } else {
    PredictableCodeSizeScope predictible_code_size_scope(
        this, kNoCodeAgeSequenceLength * Assembler::kInstrSize);
    // The following three instructions must remain together and unmodified
    // for code aging to work properly.
    if (isolate()->IsCodePreAgingActive()) {
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
  if (FLAG_enable_ool_constant_pool) {
    LoadConstantPoolPointerRegister();
    set_constant_pool_available(true);
  }
}


void MacroAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool) {
  // r0-r3: preserved
  PushFixedFrame();
  if (FLAG_enable_ool_constant_pool && load_constant_pool) {
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
  // (if FLAG_enable_ool_constant_pool).
  int frame_ends;
  if (FLAG_enable_ool_constant_pool) {
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
  ASSERT_EQ(2 * kPointerSize, ExitFrameConstants::kCallerSPDisplacement);
  ASSERT_EQ(1 * kPointerSize, ExitFrameConstants::kCallerPCOffset);
  ASSERT_EQ(0 * kPointerSize, ExitFrameConstants::kCallerFPOffset);
  Push(lr, fp);
  mov(fp, Operand(sp));  // Set up new frame pointer.
  // Reserve room for saved entry sp and code object.
  sub(sp, sp, Operand(ExitFrameConstants::kFrameSize));
  if (emit_debug_code()) {
    mov(ip, Operand::Zero());
    str(ip, MemOperand(fp, ExitFrameConstants::kSPOffset));
  }
  if (FLAG_enable_ool_constant_pool) {
    str(pp, MemOperand(fp, ExitFrameConstants::kConstantPoolOffset));
    LoadConstantPoolPointerRegister();
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
    // FLAG_enable_ool_constant_pool) were pushed after the fp.
  }

  // Reserve place for the return address and stack space and align the frame
  // preparing for calling the runtime function.
  const int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  sub(sp, sp, Operand((stack_space + 1) * kPointerSize));
  if (frame_alignment > 0) {
    ASSERT(IsPowerOf2(frame_alignment));
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
  return OS::ActivationFrameAlignment();
#else  // V8_HOST_ARCH_ARM
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif  // V8_HOST_ARCH_ARM
}


void MacroAssembler::LeaveExitFrame(bool save_doubles,
                                    Register argument_count,
                                    bool restore_context) {
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
  if (FLAG_enable_ool_constant_pool) {
    ldr(pp, MemOperand(fp, ExitFrameConstants::kConstantPoolOffset));
  }
  mov(sp, Operand(fp));
  ldm(ia_w, sp, fp.bit() | lr.bit());
  if (argument_count.is_valid()) {
    add(sp, sp, Operand(argument_count, LSL, kPointerSizeLog2));
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
                                    Handle<Code> code_constant,
                                    Register code_reg,
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
  ASSERT(actual.is_immediate() || actual.reg().is(r0));
  ASSERT(expected.is_immediate() || expected.reg().is(r2));
  ASSERT((!code_constant.is_null() && code_reg.is(no_reg)) || code_reg.is(r3));

  if (expected.is_immediate()) {
    ASSERT(actual.is_immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      mov(r0, Operand(actual.immediate()));
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
      cmp(expected.reg(), Operand(actual.immediate()));
      b(eq, &regular_invoke);
      mov(r0, Operand(actual.immediate()));
    } else {
      cmp(expected.reg(), Operand(actual.reg()));
      b(eq, &regular_invoke);
    }
  }

  if (!definitely_matches) {
    if (!code_constant.is_null()) {
      mov(r3, Operand(code_constant));
      add(r3, r3, Operand(Code::kHeaderSize - kHeapObjectTag));
    }

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


void MacroAssembler::InvokeCode(Register code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                InvokeFlag flag,
                                const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, Handle<Code>::null(), code,
                 &done, &definitely_mismatches, flag,
                 call_wrapper);
  if (!definitely_mismatches) {
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      Call(code);
      call_wrapper.AfterCall();
    } else {
      ASSERT(flag == JUMP_FUNCTION);
      Jump(code);
    }

    // Continue here if InvokePrologue does handle the invocation due to
    // mismatched parameter counts.
    bind(&done);
  }
}


void MacroAssembler::InvokeFunction(Register fun,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r1.
  ASSERT(fun.is(r1));

  Register expected_reg = r2;
  Register code_reg = r3;

  ldr(code_reg, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));
  ldr(expected_reg,
      FieldMemOperand(code_reg,
                      SharedFunctionInfo::kFormalParameterCountOffset));
  SmiUntag(expected_reg);
  ldr(code_reg,
      FieldMemOperand(r1, JSFunction::kCodeEntryOffset));

  ParameterCount expected(expected_reg);
  InvokeCode(code_reg, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r1.
  ASSERT(function.is(r1));

  // Get the function and setup the context.
  ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  ldr(r3, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
  InvokeCode(r3, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  Move(r1, function);
  InvokeFunction(r1, expected, actual, flag, call_wrapper);
}


void MacroAssembler::IsObjectJSObjectType(Register heap_object,
                                          Register map,
                                          Register scratch,
                                          Label* fail) {
  ldr(map, FieldMemOperand(heap_object, HeapObject::kMapOffset));
  IsInstanceJSObjectType(map, scratch, fail);
}


void MacroAssembler::IsInstanceJSObjectType(Register map,
                                            Register scratch,
                                            Label* fail) {
  ldrb(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
  cmp(scratch, Operand(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
  b(lt, fail);
  cmp(scratch, Operand(LAST_NONCALLABLE_SPEC_OBJECT_TYPE));
  b(gt, fail);
}


void MacroAssembler::IsObjectJSStringType(Register object,
                                          Register scratch,
                                          Label* fail) {
  ASSERT(kNotStringTag != 0);

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


#ifdef ENABLE_DEBUGGER_SUPPORT
void MacroAssembler::DebugBreak() {
  mov(r0, Operand::Zero());
  mov(r1, Operand(ExternalReference(Runtime::kDebugBreak, isolate())));
  CEntryStub ces(1);
  ASSERT(AllowThisStubCall(&ces));
  Call(ces.GetCode(isolate()), RelocInfo::DEBUG_BREAK);
}
#endif


void MacroAssembler::PushTryHandler(StackHandler::Kind kind,
                                    int handler_index) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // For the JSEntry handler, we must preserve r0-r4, r5-r6 are available.
  // We will build up the handler from the bottom by pushing on the stack.
  // Set up the code object (r5) and the state (r6) for pushing.
  unsigned state =
      StackHandler::IndexField::encode(handler_index) |
      StackHandler::KindField::encode(kind);
  mov(r5, Operand(CodeObject()));
  mov(r6, Operand(state));

  // Push the frame pointer, context, state, and code object.
  if (kind == StackHandler::JS_ENTRY) {
    mov(cp, Operand(Smi::FromInt(0)));  // Indicates no context.
    mov(ip, Operand::Zero());  // NULL frame pointer.
    stm(db_w, sp, r5.bit() | r6.bit() | cp.bit() | ip.bit());
  } else {
    stm(db_w, sp, r5.bit() | r6.bit() | cp.bit() | fp.bit());
  }

  // Link the current handler as the next handler.
  mov(r6, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  ldr(r5, MemOperand(r6));
  push(r5);
  // Set this new handler as the current one.
  str(sp, MemOperand(r6));
}


void MacroAssembler::PopTryHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  pop(r1);
  mov(ip, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  add(sp, sp, Operand(StackHandlerConstants::kSize - kPointerSize));
  str(r1, MemOperand(ip));
}


void MacroAssembler::JumpToHandlerEntry() {
  // Compute the handler entry address and jump to it.  The handler table is
  // a fixed array of (smi-tagged) code offsets.
  // r0 = exception, r1 = code object, r2 = state.

  ConstantPoolUnavailableScope constant_pool_unavailable(this);
  if (FLAG_enable_ool_constant_pool) {
    ldr(pp, FieldMemOperand(r1, Code::kConstantPoolOffset));  // Constant pool.
  }
  ldr(r3, FieldMemOperand(r1, Code::kHandlerTableOffset));  // Handler table.
  add(r3, r3, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  mov(r2, Operand(r2, LSR, StackHandler::kKindWidth));  // Handler index.
  ldr(r2, MemOperand(r3, r2, LSL, kPointerSizeLog2));  // Smi-tagged offset.
  add(r1, r1, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start.
  add(pc, r1, Operand::SmiUntag(r2));  // Jump
}


void MacroAssembler::Throw(Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // The exception is expected in r0.
  if (!value.is(r0)) {
    mov(r0, value);
  }
  // Drop the stack pointer to the top of the top handler.
  mov(r3, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  ldr(sp, MemOperand(r3));
  // Restore the next handler.
  pop(r2);
  str(r2, MemOperand(r3));

  // Get the code object (r1) and state (r2).  Restore the context and frame
  // pointer.
  ldm(ia_w, sp, r1.bit() | r2.bit() | cp.bit() | fp.bit());

  // If the handler is a JS frame, restore the context to the frame.
  // (kind == ENTRY) == (fp == 0) == (cp == 0), so we could test either fp
  // or cp.
  tst(cp, cp);
  str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset), ne);

  JumpToHandlerEntry();
}


void MacroAssembler::ThrowUncatchable(Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // The exception is expected in r0.
  if (!value.is(r0)) {
    mov(r0, value);
  }
  // Drop the stack pointer to the top of the top stack handler.
  mov(r3, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  ldr(sp, MemOperand(r3));

  // Unwind the handlers until the ENTRY handler is found.
  Label fetch_next, check_kind;
  jmp(&check_kind);
  bind(&fetch_next);
  ldr(sp, MemOperand(sp, StackHandlerConstants::kNextOffset));

  bind(&check_kind);
  STATIC_ASSERT(StackHandler::JS_ENTRY == 0);
  ldr(r2, MemOperand(sp, StackHandlerConstants::kStateOffset));
  tst(r2, Operand(StackHandler::KindField::kMask));
  b(ne, &fetch_next);

  // Set the top handler address to next handler past the top ENTRY handler.
  pop(r2);
  str(r2, MemOperand(r3));
  // Get the code object (r1) and state (r2).  Clear the context and frame
  // pointer (0 was saved in the handler).
  ldm(ia_w, sp, r1.bit() | r2.bit() | cp.bit() | fp.bit());

  JumpToHandlerEntry();
}


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch,
                                            Label* miss) {
  Label same_contexts;

  ASSERT(!holder_reg.is(scratch));
  ASSERT(!holder_reg.is(ip));
  ASSERT(!scratch.is(ip));

  // Load current lexical context from the stack frame.
  ldr(scratch, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // In debug mode, make sure the lexical context is set.
#ifdef DEBUG
  cmp(scratch, Operand::Zero());
  Check(ne, kWeShouldNotHaveAnEmptyLexicalContext);
#endif

  // Load the native context of the current context.
  int offset =
      Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
  ldr(scratch, FieldMemOperand(scratch, offset));
  ldr(scratch, FieldMemOperand(scratch, GlobalObject::kNativeContextOffset));

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
// ComputeIntegerHash in utils.h and KeyedLoadGenericElementStub in
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
    ASSERT(SeededNumberDictionary::kEntrySize == 3);
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
  // Check that the value is a normal property.
  // t2: elements + (index * kPointerSize)
  const int kDetailsOffset =
      SeededNumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  ldr(t1, FieldMemOperand(t2, kDetailsOffset));
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
  ASSERT(object_size <= Page::kMaxRegularHeapObjectSize);
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

  ASSERT(!result.is(scratch1));
  ASSERT(!result.is(scratch2));
  ASSERT(!scratch1.is(scratch2));
  ASSERT(!scratch1.is(ip));
  ASSERT(!scratch2.is(ip));

  // Make object size into bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    object_size *= kPointerSize;
  }
  ASSERT_EQ(0, object_size & kObjectAlignmentMask);

  // Check relative positions of allocation top and limit addresses.
  // The values must be adjacent in memory to allow the use of LDM.
  // Also, assert that the registers are numbered such that the values
  // are loaded in the correct order.
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  intptr_t top   =
      reinterpret_cast<intptr_t>(allocation_top.address());
  intptr_t limit =
      reinterpret_cast<intptr_t>(allocation_limit.address());
  ASSERT((limit - top) == kPointerSize);
  ASSERT(result.code() < ip.code());

  // Set up allocation top address register.
  Register topaddr = scratch1;
  mov(topaddr, Operand(allocation_top));

  // This code stores a temporary value in ip. This is OK, as the code below
  // does not need ip for implicit literal generation.
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and allocation limit into ip.
    ldm(ia, topaddr, result.bit() | ip.bit());
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry. ip is used
      // immediately below so this use of ip does not cause difference with
      // respect to register content between debug and release mode.
      ldr(ip, MemOperand(topaddr));
      cmp(result, ip);
      Check(eq, kUnexpectedAllocationTop);
    }
    // Load allocation limit into ip. Result already contains allocation top.
    ldr(ip, MemOperand(topaddr, limit - top));
  }

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    // Align the next allocation. Storing the filler map without checking top is
    // safe in new-space because the limit of the heap is aligned there.
    ASSERT((flags & PRETENURE_OLD_POINTER_SPACE) == 0);
    STATIC_ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    and_(scratch2, result, Operand(kDoubleAlignmentMask), SetCC);
    Label aligned;
    b(eq, &aligned);
    if ((flags & PRETENURE_OLD_DATA_SPACE) != 0) {
      cmp(result, Operand(ip));
      b(hs, gc_required);
    }
    mov(scratch2, Operand(isolate()->factory()->one_pointer_filler_map()));
    str(scratch2, MemOperand(result, kDoubleSize / 2, PostIndex));
    bind(&aligned);
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top. We must preserve the ip register at this
  // point, so we cannot just use add().
  ASSERT(object_size > 0);
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
      ASSERT(bits_operand.is_single_instruction(this));
      add(scratch2, source, bits_operand, SetCC, cond);
      source = scratch2;
      cond = cc;
    }
  }
  b(cs, gc_required);
  cmp(scratch2, Operand(ip));
  b(hi, gc_required);
  str(scratch2, MemOperand(topaddr));

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    add(result, result, Operand(kHeapObjectTag));
  }
}


void MacroAssembler::Allocate(Register object_size,
                              Register result,
                              Register scratch1,
                              Register scratch2,
                              Label* gc_required,
                              AllocationFlags flags) {
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

  // Assert that the register arguments are different and that none of
  // them are ip. ip is used explicitly in the code generated below.
  ASSERT(!result.is(scratch1));
  ASSERT(!result.is(scratch2));
  ASSERT(!scratch1.is(scratch2));
  ASSERT(!object_size.is(ip));
  ASSERT(!result.is(ip));
  ASSERT(!scratch1.is(ip));
  ASSERT(!scratch2.is(ip));

  // Check relative positions of allocation top and limit addresses.
  // The values must be adjacent in memory to allow the use of LDM.
  // Also, assert that the registers are numbered such that the values
  // are loaded in the correct order.
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  intptr_t top =
      reinterpret_cast<intptr_t>(allocation_top.address());
  intptr_t limit =
      reinterpret_cast<intptr_t>(allocation_limit.address());
  ASSERT((limit - top) == kPointerSize);
  ASSERT(result.code() < ip.code());

  // Set up allocation top address.
  Register topaddr = scratch1;
  mov(topaddr, Operand(allocation_top));

  // This code stores a temporary value in ip. This is OK, as the code below
  // does not need ip for implicit literal generation.
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and allocation limit into ip.
    ldm(ia, topaddr, result.bit() | ip.bit());
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry. ip is used
      // immediately below so this use of ip does not cause difference with
      // respect to register content between debug and release mode.
      ldr(ip, MemOperand(topaddr));
      cmp(result, ip);
      Check(eq, kUnexpectedAllocationTop);
    }
    // Load allocation limit into ip. Result already contains allocation top.
    ldr(ip, MemOperand(topaddr, limit - top));
  }

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    // Align the next allocation. Storing the filler map without checking top is
    // safe in new-space because the limit of the heap is aligned there.
    ASSERT((flags & PRETENURE_OLD_POINTER_SPACE) == 0);
    ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    and_(scratch2, result, Operand(kDoubleAlignmentMask), SetCC);
    Label aligned;
    b(eq, &aligned);
    if ((flags & PRETENURE_OLD_DATA_SPACE) != 0) {
      cmp(result, Operand(ip));
      b(hs, gc_required);
    }
    mov(scratch2, Operand(isolate()->factory()->one_pointer_filler_map()));
    str(scratch2, MemOperand(result, kDoubleSize / 2, PostIndex));
    bind(&aligned);
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top. Object size may be in words so a shift is
  // required to get the number of bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    add(scratch2, result, Operand(object_size, LSL, kPointerSizeLog2), SetCC);
  } else {
    add(scratch2, result, Operand(object_size), SetCC);
  }
  b(cs, gc_required);
  cmp(scratch2, Operand(ip));
  b(hi, gc_required);

  // Update allocation top. result temporarily holds the new top.
  if (emit_debug_code()) {
    tst(scratch2, Operand(kObjectAlignmentMask));
    Check(eq, kUnalignedAllocationInNewSpace);
  }
  str(scratch2, MemOperand(topaddr));

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    add(result, result, Operand(kHeapObjectTag));
  }
}


void MacroAssembler::UndoAllocationInNewSpace(Register object,
                                              Register scratch) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());

  // Make sure the object has no tag before resetting top.
  and_(object, object, Operand(~kHeapObjectTagMask));
#ifdef DEBUG
  // Check that the object un-allocated is below the current top.
  mov(scratch, Operand(new_space_allocation_top));
  ldr(scratch, MemOperand(scratch));
  cmp(object, scratch);
  Check(lt, kUndoAllocationOfNonAllocatedMemory);
#endif
  // Write the address of the object to un-allocate as the current top.
  mov(scratch, Operand(new_space_allocation_top));
  str(object, MemOperand(scratch));
}


void MacroAssembler::AllocateTwoByteString(Register result,
                                           Register length,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
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


void MacroAssembler::AllocateAsciiString(Register result,
                                         Register length,
                                         Register scratch1,
                                         Register scratch2,
                                         Register scratch3,
                                         Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  ASSERT((SeqOneByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  ASSERT(kCharSize == 1);
  add(scratch1, length,
      Operand(kObjectAlignmentMask + SeqOneByteString::kHeaderSize));
  and_(scratch1, scratch1, Operand(~kObjectAlignmentMask));

  // Allocate ASCII string in new space.
  Allocate(scratch1,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result,
                      length,
                      Heap::kAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
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


void MacroAssembler::AllocateAsciiConsString(Register result,
                                             Register length,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* gc_required) {
  Label allocate_new_space, install_map;
  AllocationFlags flags = TAG_OBJECT;

  ExternalReference high_promotion_mode = ExternalReference::
      new_space_high_promotion_mode_active_address(isolate());
  mov(scratch1, Operand(high_promotion_mode));
  ldr(scratch1, MemOperand(scratch1, 0));
  cmp(scratch1, Operand::Zero());
  b(eq, &allocate_new_space);

  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           static_cast<AllocationFlags>(flags | PRETENURE_OLD_POINTER_SPACE));

  jmp(&install_map);

  bind(&allocate_new_space);
  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           flags);

  bind(&install_map);

  InitializeNewString(result,
                      length,
                      Heap::kConsAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
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


void MacroAssembler::AllocateAsciiSlicedString(Register result,
                                               Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kSlicedAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::CompareObjectType(Register object,
                                       Register map,
                                       Register type_reg,
                                       InstanceType type) {
  const Register temp = type_reg.is(no_reg) ? ip : type_reg;

  ldr(map, FieldMemOperand(object, HeapObject::kMapOffset));
  CompareInstanceType(map, temp, type);
}


void MacroAssembler::CheckObjectTypeRange(Register object,
                                          Register map,
                                          InstanceType min_type,
                                          InstanceType max_type,
                                          Label* false_label) {
  STATIC_ASSERT(Map::kInstanceTypeOffset < 4096);
  STATIC_ASSERT(LAST_TYPE < 256);
  ldr(map, FieldMemOperand(object, HeapObject::kMapOffset));
  ldrb(ip, FieldMemOperand(map, Map::kInstanceTypeOffset));
  sub(ip, ip, Operand(min_type));
  cmp(ip, Operand(max_type - min_type));
  b(hi, false_label);
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
  ASSERT(!obj.is(ip));
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


void MacroAssembler::DispatchMap(Register obj,
                                 Register scratch,
                                 Handle<Map> map,
                                 Handle<Code> success,
                                 SmiCheckType smi_check_type) {
  Label fail;
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, &fail);
  }
  ldr(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  mov(ip, Operand(map));
  cmp(scratch, ip);
  Jump(success, RelocInfo::CODE_TARGET, eq);
  bind(&fail);
}


void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Register scratch,
                                             Label* miss,
                                             bool miss_on_bound_function) {
  // Check that the receiver isn't a smi.
  JumpIfSmi(function, miss);

  // Check that the function really is a function.  Load map into result reg.
  CompareObjectType(function, result, scratch, JS_FUNCTION_TYPE);
  b(ne, miss);

  if (miss_on_bound_function) {
    ldr(scratch,
        FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
    ldr(scratch,
        FieldMemOperand(scratch, SharedFunctionInfo::kCompilerHintsOffset));
    tst(scratch,
        Operand(Smi::FromInt(1 << SharedFunctionInfo::kBoundFunction)));
    b(ne, miss);
  }

  // Make sure that the function has an instance prototype.
  Label non_instance;
  ldrb(scratch, FieldMemOperand(result, Map::kBitFieldOffset));
  tst(scratch, Operand(1 << Map::kHasNonInstancePrototype));
  b(ne, &non_instance);

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
  jmp(&done);

  // Non-instance prototype: Fetch prototype from constructor field
  // in initial map.
  bind(&non_instance);
  ldr(result, FieldMemOperand(result, Map::kConstructorOffset));

  // All done.
  bind(&done);
}


void MacroAssembler::CallStub(CodeStub* stub,
                              TypeFeedbackId ast_id,
                              Condition cond) {
  ASSERT(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(isolate()), RelocInfo::CODE_TARGET, ast_id, cond);
}


void MacroAssembler::TailCallStub(CodeStub* stub, Condition cond) {
  Jump(stub->GetCode(isolate()), RelocInfo::CODE_TARGET, cond);
}


static int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  return ref0.address() - ref1.address();
}


void MacroAssembler::CallApiFunctionAndReturn(
    Register function_address,
    ExternalReference thunk_ref,
    int stack_space,
    MemOperand return_value_operand,
    MemOperand* context_restore_operand) {
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate());
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(isolate()),
      next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(isolate()),
      next_address);

  ASSERT(function_address.is(r1) || function_address.is(r2));

  Label profiler_disabled;
  Label end_profiler_check;
  bool* is_profiling_flag =
      isolate()->cpu_profiler()->is_profiling_address();
  STATIC_ASSERT(sizeof(*is_profiling_flag) == 1);
  mov(r9, Operand(reinterpret_cast<int32_t>(is_profiling_flag)));
  ldrb(r9, MemOperand(r9, 0));
  cmp(r9, Operand(0));
  b(eq, &profiler_disabled);

  // Additional parameter is the address of the actual callback.
  mov(r3, Operand(thunk_ref));
  jmp(&end_profiler_check);

  bind(&profiler_disabled);
  Move(r3, function_address);
  bind(&end_profiler_check);

  // Allocate HandleScope in callee-save registers.
  mov(r9, Operand(next_address));
  ldr(r4, MemOperand(r9, kNextOffset));
  ldr(r5, MemOperand(r9, kLimitOffset));
  ldr(r6, MemOperand(r9, kLevelOffset));
  add(r6, r6, Operand(1));
  str(r6, MemOperand(r9, kLevelOffset));

  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    PrepareCallCFunction(1, r0);
    mov(r0, Operand(ExternalReference::isolate_address(isolate())));
    CallCFunction(ExternalReference::log_enter_external_function(isolate()), 1);
    PopSafepointRegisters();
  }

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
  DirectCEntryStub stub;
  stub.GenerateCall(this, r3);

  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    PrepareCallCFunction(1, r0);
    mov(r0, Operand(ExternalReference::isolate_address(isolate())));
    CallCFunction(ExternalReference::log_leave_external_function(isolate()), 1);
    PopSafepointRegisters();
  }

  Label promote_scheduled_exception;
  Label exception_handled;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // load value from ReturnValue
  ldr(r0, return_value_operand);
  bind(&return_value_loaded);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  str(r4, MemOperand(r9, kNextOffset));
  if (emit_debug_code()) {
    ldr(r1, MemOperand(r9, kLevelOffset));
    cmp(r1, r6);
    Check(eq, kUnexpectedLevelAfterReturnFromApiCall);
  }
  sub(r6, r6, Operand(1));
  str(r6, MemOperand(r9, kLevelOffset));
  ldr(ip, MemOperand(r9, kLimitOffset));
  cmp(r5, ip);
  b(ne, &delete_allocated_handles);

  // Check if the function scheduled an exception.
  bind(&leave_exit_frame);
  LoadRoot(r4, Heap::kTheHoleValueRootIndex);
  mov(ip, Operand(ExternalReference::scheduled_exception_address(isolate())));
  ldr(r5, MemOperand(ip));
  cmp(r4, r5);
  b(ne, &promote_scheduled_exception);
  bind(&exception_handled);

  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    ldr(cp, *context_restore_operand);
  }
  // LeaveExitFrame expects unwind space to be in a register.
  mov(r4, Operand(stack_space));
  LeaveExitFrame(false, r4, !restore_context);
  mov(pc, lr);

  bind(&promote_scheduled_exception);
  {
    FrameScope frame(this, StackFrame::INTERNAL);
    CallExternalReference(
        ExternalReference(Runtime::kHiddenPromoteScheduledException, isolate()),
        0);
  }
  jmp(&exception_handled);

  // HandleScope limit has changed. Delete allocated extensions.
  bind(&delete_allocated_handles);
  str(r5, MemOperand(r9, kLimitOffset));
  mov(r4, r0);
  PrepareCallCFunction(1, r5);
  mov(r0, Operand(ExternalReference::isolate_address(isolate())));
  CallCFunction(
      ExternalReference::delete_handle_scope_extensions(isolate()), 1);
  mov(r0, r4);
  jmp(&leave_exit_frame);
}


bool MacroAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame_ || !stub->SometimesSetsUpAFrame();
}


void MacroAssembler::IllegalOperation(int num_arguments) {
  if (num_arguments > 0) {
    add(sp, sp, Operand(num_arguments * kPointerSize));
  }
  LoadRoot(r0, Heap::kUndefinedValueRootIndex);
}


void MacroAssembler::IndexFromHash(Register hash, Register index) {
  // If the hash field contains an array index pick it out. The assert checks
  // that the constants for the maximum number of digits for an array index
  // cached in the hash field and the number of bits reserved for it does not
  // conflict.
  ASSERT(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  // We want the smi-tagged index in key.  kArrayIndexValueMask has zeros in
  // the low kHashShift bits.
  Ubfx(hash, hash, String::kHashShift, String::kArrayIndexValueBits);
  SmiTag(index, hash);
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
  ASSERT(!double_input.is(double_scratch));
  vcvt_s32_f64(double_scratch.low(), double_input);
  vcvt_f64_s32(double_scratch, double_scratch.low());
  VFPCompareAndSetFlags(double_input, double_scratch);
}


void MacroAssembler::TryDoubleToInt32Exact(Register result,
                                           DwVfpRegister double_input,
                                           LowDwVfpRegister double_scratch) {
  ASSERT(!double_input.is(double_scratch));
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
  ASSERT(!result.is(input_high));
  ASSERT(!double_input.is(double_scratch));
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

  DoubleToIStub stub(sp, result, 0, true, true);
  CallStub(&stub);

  add(sp, sp, Operand(kDoubleSize));
  pop(lr);

  bind(&done);
}


void MacroAssembler::TruncateHeapNumberToI(Register result,
                                           Register object) {
  Label done;
  LowDwVfpRegister double_scratch = kScratchDoubleReg;
  ASSERT(!result.is(object));

  vldr(double_scratch,
       MemOperand(object, HeapNumber::kValueOffset - kHeapObjectTag));
  TryInlineTruncateDoubleToI(result, double_scratch, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  push(lr);
  DoubleToIStub stub(object,
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
  ASSERT(!result.is(object));

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
  if (f->nargs >= 0 && f->nargs != num_arguments) {
    IllegalOperation(num_arguments);
    return;
  }

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r0, Operand(num_arguments));
  mov(r1, Operand(ExternalReference(f, isolate())));
  CEntryStub stub(1, save_doubles);
  CallStub(&stub);
}


void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments) {
  mov(r0, Operand(num_arguments));
  mov(r1, Operand(ext));

  CEntryStub stub(1);
  CallStub(&stub);
}


void MacroAssembler::TailCallExternalReference(const ExternalReference& ext,
                                               int num_arguments,
                                               int result_size) {
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r0, Operand(num_arguments));
  JumpToExternalReference(ext);
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid,
                                     int num_arguments,
                                     int result_size) {
  TailCallExternalReference(ExternalReference(fid, isolate()),
                            num_arguments,
                            result_size);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin) {
#if defined(__thumb__)
  // Thumb mode builtin.
  ASSERT((reinterpret_cast<intptr_t>(builtin.address()) & 1) == 1);
#endif
  mov(r1, Operand(builtin));
  CEntryStub stub(1);
  Jump(stub.GetCode(isolate()), RelocInfo::CODE_TARGET);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeFlag flag,
                                   const CallWrapper& call_wrapper) {
  // You can't call a builtin without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  GetBuiltinEntry(r2, id);
  if (flag == CALL_FUNCTION) {
    call_wrapper.BeforeCall(CallSize(r2));
    Call(r2);
    call_wrapper.AfterCall();
  } else {
    ASSERT(flag == JUMP_FUNCTION);
    Jump(r2);
  }
}


void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the builtins object into target register.
  ldr(target,
      MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  ldr(target, FieldMemOperand(target, GlobalObject::kBuiltinsOffset));
  // Load the JavaScript builtin function from the builtins object.
  ldr(target, FieldMemOperand(target,
                          JSBuiltinsObject::OffsetOfFunctionWithId(id)));
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  ASSERT(!target.is(r1));
  GetBuiltinFunction(r1, id);
  // Load the code entry point from the builtins object.
  ldr(target, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
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
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch2, Operand(ExternalReference(counter)));
    ldr(scratch1, MemOperand(scratch2));
    add(scratch1, scratch1, Operand(value));
    str(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  ASSERT(value > 0);
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
    ASSERT(!elements.is(ip));
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
    ASSERT(abort_instructions <= kExpectedAbortInstructions);
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
  // Load the global or builtins object from the current context.
  ldr(scratch,
      MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  ldr(scratch, FieldMemOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check that the function's map is the same as the expected cached map.
  ldr(scratch,
      MemOperand(scratch,
                 Context::SlotOffset(Context::JS_ARRAY_MAPS_INDEX)));
  size_t offset = expected_kind * kPointerSize +
      FixedArrayBase::kHeaderSize;
  ldr(ip, FieldMemOperand(scratch, offset));
  cmp(map_in_out, ip);
  b(ne, no_map_match);

  // Use the transitioned cached map.
  offset = transitioned_kind * kPointerSize +
      FixedArrayBase::kHeaderSize;
  ldr(map_in_out, FieldMemOperand(scratch, offset));
}


void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the global or builtins object from the current context.
  ldr(function,
      MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  ldr(function, FieldMemOperand(function,
                                GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  ldr(function, MemOperand(function, Context::SlotOffset(index)));
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


void MacroAssembler::LookupNumberStringCache(Register object,
                                             Register result,
                                             Register scratch1,
                                             Register scratch2,
                                             Register scratch3,
                                             Label* not_found) {
  // Use of registers. Register result is used as a temporary.
  Register number_string_cache = result;
  Register mask = scratch3;

  // Load the number string cache.
  LoadRoot(number_string_cache, Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  ldr(mask, FieldMemOperand(number_string_cache, FixedArray::kLengthOffset));
  // Divide length by two (length is a smi).
  mov(mask, Operand(mask, ASR, kSmiTagSize + 1));
  sub(mask, mask, Operand(1));  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  Label is_smi;
  Label load_result_from_cache;
  JumpIfSmi(object, &is_smi);
  CheckMap(object,
           scratch1,
           Heap::kHeapNumberMapRootIndex,
           not_found,
           DONT_DO_SMI_CHECK);

  STATIC_ASSERT(8 == kDoubleSize);
  add(scratch1,
      object,
      Operand(HeapNumber::kValueOffset - kHeapObjectTag));
  ldm(ia, scratch1, scratch1.bit() | scratch2.bit());
  eor(scratch1, scratch1, Operand(scratch2));
  and_(scratch1, scratch1, Operand(mask));

  // Calculate address of entry in string cache: each entry consists
  // of two pointer sized fields.
  add(scratch1,
      number_string_cache,
      Operand(scratch1, LSL, kPointerSizeLog2 + 1));

  Register probe = mask;
  ldr(probe, FieldMemOperand(scratch1, FixedArray::kHeaderSize));
  JumpIfSmi(probe, not_found);
  sub(scratch2, object, Operand(kHeapObjectTag));
  vldr(d0, scratch2, HeapNumber::kValueOffset);
  sub(probe, probe, Operand(kHeapObjectTag));
  vldr(d1, probe, HeapNumber::kValueOffset);
  VFPCompareAndSetFlags(d0, d1);
  b(ne, not_found);  // The cache did not contain this value.
  b(&load_result_from_cache);

  bind(&is_smi);
  Register scratch = scratch1;
  and_(scratch, mask, Operand(object, ASR, 1));
  // Calculate address of entry in string cache: each entry consists
  // of two pointer sized fields.
  add(scratch,
      number_string_cache,
      Operand(scratch, LSL, kPointerSizeLog2 + 1));

  // Check if the entry is the smi we are looking for.
  ldr(probe, FieldMemOperand(scratch, FixedArray::kHeaderSize));
  cmp(object, probe);
  b(ne, not_found);

  // Get the result from the cache.
  bind(&load_result_from_cache);
  ldr(result, FieldMemOperand(scratch, FixedArray::kHeaderSize + kPointerSize));
  IncrementCounter(isolate()->counters()->number_to_string_native(),
                   1,
                   scratch1,
                   scratch2);
}


void MacroAssembler::JumpIfNonSmisNotBothSequentialAsciiStrings(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure) {
  // Test that both first and second are sequential ASCII strings.
  // Assume that they are non-smis.
  ldr(scratch1, FieldMemOperand(first, HeapObject::kMapOffset));
  ldr(scratch2, FieldMemOperand(second, HeapObject::kMapOffset));
  ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  ldrb(scratch2, FieldMemOperand(scratch2, Map::kInstanceTypeOffset));

  JumpIfBothInstanceTypesAreNotSequentialAscii(scratch1,
                                               scratch2,
                                               scratch1,
                                               scratch2,
                                               failure);
}

void MacroAssembler::JumpIfNotBothSequentialAsciiStrings(Register first,
                                                         Register second,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         Label* failure) {
  // Check that neither is a smi.
  and_(scratch1, first, Operand(second));
  JumpIfSmi(scratch1, failure);
  JumpIfNonSmisNotBothSequentialAsciiStrings(first,
                                             second,
                                             scratch1,
                                             scratch2,
                                             failure);
}


void MacroAssembler::JumpIfNotUniqueName(Register reg,
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
                                        TaggingMode tagging_mode) {
  // Allocate an object in the heap for the heap number and tag it as a heap
  // object.
  Allocate(HeapNumber::kSize, result, scratch1, scratch2, gc_required,
           tagging_mode == TAG_RESULT ? TAG_OBJECT : NO_ALLOCATION_FLAGS);

  // Store heap number map in the allocated object.
  AssertIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
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


// Copies a fixed number of fields of heap objects from src to dst.
void MacroAssembler::CopyFields(Register dst,
                                Register src,
                                LowDwVfpRegister double_scratch,
                                int field_count) {
  int double_count = field_count / (DwVfpRegister::kSizeInBytes / kPointerSize);
  for (int i = 0; i < double_count; i++) {
    vldr(double_scratch, FieldMemOperand(src, i * DwVfpRegister::kSizeInBytes));
    vstr(double_scratch, FieldMemOperand(dst, i * DwVfpRegister::kSizeInBytes));
  }

  STATIC_ASSERT(SwVfpRegister::kSizeInBytes == kPointerSize);
  STATIC_ASSERT(2 * SwVfpRegister::kSizeInBytes == DwVfpRegister::kSizeInBytes);

  int remain = field_count % (DwVfpRegister::kSizeInBytes / kPointerSize);
  if (remain != 0) {
    vldr(double_scratch.low(),
         FieldMemOperand(src, (field_count - 1) * kPointerSize));
    vstr(double_scratch.low(),
         FieldMemOperand(dst, (field_count - 1) * kPointerSize));
  }
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


void MacroAssembler::InitializeFieldsWithFiller(Register start_offset,
                                                Register end_offset,
                                                Register filler) {
  Label loop, entry;
  b(&entry);
  bind(&loop);
  str(filler, MemOperand(start_offset, kPointerSize, PostIndex));
  bind(&entry);
  cmp(start_offset, end_offset);
  b(lt, &loop);
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


void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialAscii(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure) {
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatAsciiStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  and_(scratch1, first, Operand(kFlatAsciiStringMask));
  and_(scratch2, second, Operand(kFlatAsciiStringMask));
  cmp(scratch1, Operand(kFlatAsciiStringTag));
  // Ignore second test if first test failed.
  cmp(scratch2, Operand(kFlatAsciiStringTag), eq);
  b(ne, failure);
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialAscii(Register type,
                                                            Register scratch,
                                                            Label* failure) {
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatAsciiStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  and_(scratch, type, Operand(kFlatAsciiStringMask));
  cmp(scratch, Operand(kFlatAsciiStringTag));
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
    ASSERT(IsPowerOf2(frame_alignment));
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
  ASSERT(src.is(d0));
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
  ASSERT(src1.is(d0));
  ASSERT(src2.is(d1));
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
  ASSERT(has_frame());
  // Make sure that the stack is aligned before calling a C function unless
  // running in the simulator. The simulator has its own alignment check which
  // provides more information.
#if V8_HOST_ARCH_ARM
  if (emit_debug_code()) {
    int frame_alignment = OS::ActivationFrameAlignment();
    int frame_alignment_mask = frame_alignment - 1;
    if (frame_alignment > kPointerSize) {
      ASSERT(IsPowerOf2(frame_alignment));
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
    add(sp, sp, Operand(stack_passed_arguments * sizeof(kPointerSize)));
  }
}


void MacroAssembler::GetRelocatedValueLocation(Register ldr_location,
                                               Register result) {
  const uint32_t kLdrOffsetMask = (1 << 12) - 1;
  ldr(result, MemOperand(ldr_location));
  if (emit_debug_code()) {
    // Check that the instruction is a ldr reg, [<pc or pp> + offset] .
    if (FLAG_enable_ool_constant_pool) {
      and_(result, result, Operand(kLdrPpPattern));
      cmp(result, Operand(kLdrPpPattern));
      Check(eq, kTheInstructionToPatchShouldBeALoadFromPp);
    } else {
      and_(result, result, Operand(kLdrPCPattern));
      cmp(result, Operand(kLdrPCPattern));
      Check(eq, kTheInstructionToPatchShouldBeALoadFromPc);
    }
    // Result was clobbered. Restore it.
    ldr(result, MemOperand(ldr_location));
  }
  // Get the address of the constant.
  and_(result, result, Operand(kLdrOffsetMask));
  if (FLAG_enable_ool_constant_pool) {
    add(result, pp, Operand(result));
  } else {
    add(result, ldr_location, Operand(result));
    add(result, result, Operand(Instruction::kPCReadOffset));
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


void MacroAssembler::CheckMapDeprecated(Handle<Map> map,
                                        Register scratch,
                                        Label* if_deprecated) {
  if (map->CanBeDeprecated()) {
    mov(scratch, Operand(map));
    ldr(scratch, FieldMemOperand(scratch, Map::kBitField3Offset));
    tst(scratch, Operand(Smi::FromInt(Map::Deprecated::kMask)));
    b(ne, if_deprecated);
  }
}


void MacroAssembler::JumpIfBlack(Register object,
                                 Register scratch0,
                                 Register scratch1,
                                 Label* on_black) {
  HasColor(object, scratch0, scratch1, on_black, 1, 0);  // kBlackBitPattern.
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
}


void MacroAssembler::HasColor(Register object,
                              Register bitmap_scratch,
                              Register mask_scratch,
                              Label* has_color,
                              int first_bit,
                              int second_bit) {
  ASSERT(!AreAliased(object, bitmap_scratch, mask_scratch, no_reg));

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


// Detect some, but not all, common pointer-free objects.  This is used by the
// incremental write barrier which doesn't care about oddballs (they are always
// marked black immediately so this code is not hit).
void MacroAssembler::JumpIfDataObject(Register value,
                                      Register scratch,
                                      Label* not_data_object) {
  Label is_data_object;
  ldr(scratch, FieldMemOperand(value, HeapObject::kMapOffset));
  CompareRoot(scratch, Heap::kHeapNumberMapRootIndex);
  b(eq, &is_data_object);
  ASSERT(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  ASSERT(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  tst(scratch, Operand(kIsIndirectStringMask | kIsNotStringMask));
  b(ne, not_data_object);
  bind(&is_data_object);
}


void MacroAssembler::GetMarkBits(Register addr_reg,
                                 Register bitmap_reg,
                                 Register mask_reg) {
  ASSERT(!AreAliased(addr_reg, bitmap_reg, mask_reg, no_reg));
  and_(bitmap_reg, addr_reg, Operand(~Page::kPageAlignmentMask));
  Ubfx(mask_reg, addr_reg, kPointerSizeLog2, Bitmap::kBitsPerCellLog2);
  const int kLowBits = kPointerSizeLog2 + Bitmap::kBitsPerCellLog2;
  Ubfx(ip, addr_reg, kLowBits, kPageSizeBits - kLowBits);
  add(bitmap_reg, bitmap_reg, Operand(ip, LSL, kPointerSizeLog2));
  mov(ip, Operand(1));
  mov(mask_reg, Operand(ip, LSL, mask_reg));
}


void MacroAssembler::EnsureNotWhite(
    Register value,
    Register bitmap_scratch,
    Register mask_scratch,
    Register load_scratch,
    Label* value_is_white_and_not_data) {
  ASSERT(!AreAliased(value, bitmap_scratch, mask_scratch, ip));
  GetMarkBits(value, bitmap_scratch, mask_scratch);

  // If the value is black or grey we don't need to do anything.
  ASSERT(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
  ASSERT(strcmp(Marking::kGreyBitPattern, "11") == 0);
  ASSERT(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  Label done;

  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  ldr(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  tst(mask_scratch, load_scratch);
  b(ne, &done);

  if (emit_debug_code()) {
    // Check for impossible bit pattern.
    Label ok;
    // LSL may overflow, making the check conservative.
    tst(load_scratch, Operand(mask_scratch, LSL, 1));
    b(eq, &ok);
    stop("Impossible marking bit pattern");
    bind(&ok);
  }

  // Value is white.  We check whether it is data that doesn't need scanning.
  // Currently only checks for HeapNumber and non-cons strings.
  Register map = load_scratch;  // Holds map while checking type.
  Register length = load_scratch;  // Holds length of object after testing type.
  Label is_data_object;

  // Check for heap-number
  ldr(map, FieldMemOperand(value, HeapObject::kMapOffset));
  CompareRoot(map, Heap::kHeapNumberMapRootIndex);
  mov(length, Operand(HeapNumber::kSize), LeaveCC, eq);
  b(eq, &is_data_object);

  // Check for strings.
  ASSERT(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  ASSERT(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  Register instance_type = load_scratch;
  ldrb(instance_type, FieldMemOperand(map, Map::kInstanceTypeOffset));
  tst(instance_type, Operand(kIsIndirectStringMask | kIsNotStringMask));
  b(ne, value_is_white_and_not_data);
  // It's a non-indirect (non-cons and non-slice) string.
  // If it's external, the length is just ExternalString::kSize.
  // Otherwise it's String::kHeaderSize + string->length() * (1 or 2).
  // External strings are the only ones with the kExternalStringTag bit
  // set.
  ASSERT_EQ(0, kSeqStringTag & kExternalStringTag);
  ASSERT_EQ(0, kConsStringTag & kExternalStringTag);
  tst(instance_type, Operand(kExternalStringTag));
  mov(length, Operand(ExternalString::kSize), LeaveCC, ne);
  b(ne, &is_data_object);

  // Sequential string, either ASCII or UC16.
  // For ASCII (char-size of 1) we shift the smi tag away to get the length.
  // For UC16 (char-size of 2) we just leave the smi tag in place, thereby
  // getting the length multiplied by 2.
  ASSERT(kOneByteStringTag == 4 && kStringEncodingMask == 4);
  ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  ldr(ip, FieldMemOperand(value, String::kLengthOffset));
  tst(instance_type, Operand(kStringEncodingMask));
  mov(ip, Operand(ip, LSR, 1), LeaveCC, ne);
  add(length, ip, Operand(SeqString::kHeaderSize + kObjectAlignmentMask));
  and_(length, length, Operand(~kObjectAlignmentMask));

  bind(&is_data_object);
  // Value is a data object, and it is white.  Mark it black.  Since we know
  // that the object is white we can make it black by flipping one bit.
  ldr(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  orr(ip, ip, Operand(mask_scratch));
  str(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));

  and_(bitmap_scratch, bitmap_scratch, Operand(~Page::kPageAlignmentMask));
  ldr(ip, MemOperand(bitmap_scratch, MemoryChunk::kLiveBytesOffset));
  add(ip, ip, Operand(length));
  str(ip, MemOperand(bitmap_scratch, MemoryChunk::kLiveBytesOffset));

  bind(&done);
}


void MacroAssembler::ClampUint8(Register output_reg, Register input_reg) {
  Usat(output_reg, 8, Operand(input_reg));
}


void MacroAssembler::ClampDoubleToUint8(Register result_reg,
                                        DwVfpRegister input_reg,
                                        LowDwVfpRegister double_scratch) {
  Label above_zero;
  Label done;
  Label in_bounds;

  VFPCompareAndSetFlags(input_reg, 0.0);
  b(gt, &above_zero);

  // Double value is less than zero, NaN or Inf, return 0.
  mov(result_reg, Operand::Zero());
  b(al, &done);

  // Double value is >= 255, return 255.
  bind(&above_zero);
  Vmov(double_scratch, 255.0, result_reg);
  VFPCompareAndSetFlags(input_reg, double_scratch);
  b(le, &in_bounds);
  mov(result_reg, Operand(255));
  b(al, &done);

  // In 0-255 range, round and truncate.
  bind(&in_bounds);
  // Save FPSCR.
  vmrs(ip);
  // Set rounding mode to round to the nearest integer by clearing bits[23:22].
  bic(result_reg, ip, Operand(kVFPRoundingModeMask));
  vmsr(result_reg);
  vcvt_s32_f64(double_scratch.low(), input_reg, kFPSCRRounding);
  vmov(result_reg, double_scratch.low());
  // Restore FPSCR.
  vmsr(ip);
  bind(&done);
}


void MacroAssembler::Throw(BailoutReason reason) {
  Label throw_start;
  bind(&throw_start);
#ifdef DEBUG
  const char* msg = GetBailoutReason(reason);
  if (msg != NULL) {
    RecordComment("Throw message: ");
    RecordComment(msg);
  }
#endif

  mov(r0, Operand(Smi::FromInt(reason)));
  push(r0);
  // Disable stub call restrictions to always allow calls to throw.
  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    CallRuntime(Runtime::kHiddenThrowMessage, 1);
  } else {
    CallRuntime(Runtime::kHiddenThrowMessage, 1);
  }
  // will not return here
  if (is_const_pool_blocked()) {
    // If the calling code cares throw the exact number of
    // instructions generated, we insert padding here to keep the size
    // of the ThrowMessage macro constant.
    static const int kExpectedThrowMessageInstructions = 10;
    int throw_instructions = InstructionsGeneratedSince(&throw_start);
    ASSERT(throw_instructions <= kExpectedThrowMessageInstructions);
    while (throw_instructions++ < kExpectedThrowMessageInstructions) {
      nop();
    }
  }
}


void MacroAssembler::ThrowIf(Condition cc, BailoutReason reason) {
  Label L;
  b(NegateCondition(cc), &L);
  Throw(reason);
  // will not return here
  bind(&L);
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
  and_(dst, dst, Operand(Smi::FromInt(Map::EnumLengthBits::kMask)));
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

  for (int i = 0; i < Register::NumAllocatableRegisters(); i++) {
    Register candidate = Register::FromAllocationIndex(i);
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
  ASSERT(!scratch1.is(scratch0));
  Factory* factory = isolate()->factory();
  Register current = scratch0;
  Label loop_again;

  // scratch contained elements pointer.
  mov(current, object);

  // Loop based on the map going up the prototype chain.
  bind(&loop_again);
  ldr(current, FieldMemOperand(current, HeapObject::kMapOffset));
  ldr(scratch1, FieldMemOperand(current, Map::kBitField2Offset));
  Ubfx(scratch1, scratch1, Map::kElementsKindShift, Map::kElementsKindBitCount);
  cmp(scratch1, Operand(DICTIONARY_ELEMENTS));
  b(eq, found);
  ldr(current, FieldMemOperand(current, Map::kPrototypeOffset));
  cmp(current, Operand(factory->null_value()));
  b(ne, &loop_again);
}


#ifdef DEBUG
bool AreAliased(Register reg1,
                Register reg2,
                Register reg3,
                Register reg4,
                Register reg5,
                Register reg6) {
  int n_of_valid_regs = reg1.is_valid() + reg2.is_valid() +
    reg3.is_valid() + reg4.is_valid() + reg5.is_valid() + reg6.is_valid();

  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();
  int n_of_non_aliasing_regs = NumRegs(regs);

  return n_of_valid_regs != n_of_non_aliasing_regs;
}
#endif


CodePatcher::CodePatcher(byte* address,
                         int instructions,
                         FlushICache flush_cache)
    : address_(address),
      size_(instructions * Assembler::kInstrSize),
      masm_(NULL, address, size_ + Assembler::kGap),
      flush_cache_(flush_cache) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  ASSERT(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  if (flush_cache_ == FLUSH) {
    CPU::FlushICache(address_, size_);
  }

  // Check that the code was patched as expected.
  ASSERT(masm_.pc_ == address_ + size_);
  ASSERT(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
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
  ASSERT(!dividend.is(result));
  ASSERT(!dividend.is(ip));
  ASSERT(!result.is(ip));
  MultiplierAndShift ms(divisor);
  mov(ip, Operand(ms.multiplier()));
  smull(ip, result, dividend, ip);
  if (divisor > 0 && ms.multiplier() < 0) {
    add(result, result, Operand(dividend));
  }
  if (divisor < 0 && ms.multiplier() > 0) {
    sub(result, result, Operand(dividend));
  }
  if (ms.shift() > 0) mov(result, Operand(result, ASR, ms.shift()));
  add(result, result, Operand(dividend, LSR, 31));
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM

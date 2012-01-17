// Copyright 2011 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_ARM)

#include "bootstrapper.h"
#include "codegen.h"
#include "debug.h"
#include "runtime.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* arg_isolate, void* buffer, int size)
    : Assembler(arg_isolate, buffer, size),
      generating_stub_(false),
      allow_stub_calls_(true) {
  if (isolate() != NULL) {
    code_object_ = Handle<Object>(isolate()->heap()->undefined_value(),
                                  isolate());
  }
}


// We always generate arm code, never thumb code, even if V8 is compiled to
// thumb, so we require inter-working support
#if defined(__thumb__) && !defined(USE_THUMB_INTERWORK)
#error "flag -mthumb-interwork missing"
#endif


// We do not support thumb inter-working with an arm architecture not supporting
// the blx instruction (below v5t).  If you know what CPU you are compiling for
// you can use -march=armv7 or similar.
#if defined(USE_THUMB_INTERWORK) && !defined(CAN_USE_THUMB_INSTRUCTIONS)
# error "For thumb inter-working we require an architecture which supports blx"
#endif


// Using bx does not yield better code, so use it only when required
#if defined(USE_THUMB_INTERWORK)
#define USE_BX 1
#endif


void MacroAssembler::Jump(Register target, Condition cond) {
#if USE_BX
  bx(target, cond);
#else
  mov(pc, Operand(target), LeaveCC, cond);
#endif
}


void MacroAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond) {
#if USE_BX
  mov(ip, Operand(target, rmode));
  bx(ip, cond);
#else
  mov(pc, Operand(target, rmode), LeaveCC, cond);
#endif
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
  Jump(reinterpret_cast<intptr_t>(code.location()), rmode, cond);
}


int MacroAssembler::CallSize(Register target, Condition cond) {
#if USE_BLX
  return kInstrSize;
#else
  return 2 * kInstrSize;
#endif
}


void MacroAssembler::Call(Register target, Condition cond) {
  // Block constant pool for the call instruction sequence.
  BlockConstPoolScope block_const_pool(this);
  Label start;
  bind(&start);
#if USE_BLX
  blx(target, cond);
#else
  // set lr for return at current pc + 8
  mov(lr, Operand(pc), LeaveCC, cond);
  mov(pc, Operand(target), LeaveCC, cond);
#endif
  ASSERT_EQ(CallSize(target, cond), SizeOfCodeGeneratedSince(&start));
}


int MacroAssembler::CallSize(
    Address target, RelocInfo::Mode rmode, Condition cond) {
  int size = 2 * kInstrSize;
  Instr mov_instr = cond | MOV | LeaveCC;
  intptr_t immediate = reinterpret_cast<intptr_t>(target);
  if (!Operand(immediate, rmode).is_single_instruction(mov_instr)) {
    size += kInstrSize;
  }
  return size;
}


void MacroAssembler::Call(Address target,
                          RelocInfo::Mode rmode,
                          Condition cond) {
  // Block constant pool for the call instruction sequence.
  BlockConstPoolScope block_const_pool(this);
  Label start;
  bind(&start);
#if USE_BLX
  // On ARMv5 and after the recommended call sequence is:
  //  ldr ip, [pc, #...]
  //  blx ip

  // Statement positions are expected to be recorded when the target
  // address is loaded. The mov method will automatically record
  // positions when pc is the target, since this is not the case here
  // we have to do it explicitly.
  positions_recorder()->WriteRecordedPositions();

  mov(ip, Operand(reinterpret_cast<int32_t>(target), rmode));
  blx(ip, cond);

  ASSERT(kCallTargetAddressOffset == 2 * kInstrSize);
#else
  // Set lr for return at current pc + 8.
  mov(lr, Operand(pc), LeaveCC, cond);
  // Emit a ldr<cond> pc, [pc + offset of target in constant pool].
  mov(pc, Operand(reinterpret_cast<int32_t>(target), rmode), LeaveCC, cond);
  ASSERT(kCallTargetAddressOffset == kInstrSize);
#endif
  ASSERT_EQ(CallSize(target, rmode, cond), SizeOfCodeGeneratedSince(&start));
}


int MacroAssembler::CallSize(Handle<Code> code,
                             RelocInfo::Mode rmode,
                             unsigned ast_id,
                             Condition cond) {
  return CallSize(reinterpret_cast<Address>(code.location()), rmode, cond);
}


void MacroAssembler::Call(Handle<Code> code,
                          RelocInfo::Mode rmode,
                          unsigned ast_id,
                          Condition cond) {
  Label start;
  bind(&start);
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  if (rmode == RelocInfo::CODE_TARGET && ast_id != kNoASTId) {
    SetRecordedAstId(ast_id);
    rmode = RelocInfo::CODE_TARGET_WITH_ID;
  }
  // 'code' is always generated ARM code, never THUMB code
  Call(reinterpret_cast<Address>(code.location()), rmode, cond);
  ASSERT_EQ(CallSize(code, rmode, ast_id, cond),
            SizeOfCodeGeneratedSince(&start));
}


void MacroAssembler::Ret(Condition cond) {
#if USE_BX
  bx(lr, cond);
#else
  mov(pc, Operand(lr), LeaveCC, cond);
#endif
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
  mov(dst, Operand(value));
}


void MacroAssembler::Move(Register dst, Register src, Condition cond) {
  if (!dst.is(src)) {
    mov(dst, src, LeaveCC, cond);
  }
}


void MacroAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  ASSERT(CpuFeatures::IsSupported(VFP3));
  CpuFeatures::Scope scope(VFP3);
  if (!dst.is(src)) {
    vmov(dst, src);
  }
}


void MacroAssembler::And(Register dst, Register src1, const Operand& src2,
                         Condition cond) {
  if (!src2.is_reg() &&
      !src2.must_use_constant_pool() &&
      src2.immediate() == 0) {
    mov(dst, Operand(0, RelocInfo::NONE), LeaveCC, cond);

  } else if (!src2.is_single_instruction() &&
             !src2.must_use_constant_pool() &&
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
  if (!CpuFeatures::IsSupported(ARMv7)) {
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
  if (!CpuFeatures::IsSupported(ARMv7)) {
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
  if (!CpuFeatures::IsSupported(ARMv7)) {
    int mask = (1 << (width + lsb)) - 1 - ((1 << lsb) - 1);
    bic(dst, dst, Operand(mask));
    and_(scratch, src, Operand((1 << width) - 1));
    mov(scratch, Operand(scratch, LSL, lsb));
    orr(dst, dst, scratch);
  } else {
    bfi(dst, src, lsb, width, cond);
  }
}


void MacroAssembler::Bfc(Register dst, int lsb, int width, Condition cond) {
  ASSERT(lsb < 32);
  if (!CpuFeatures::IsSupported(ARMv7)) {
    int mask = (1 << (width + lsb)) - 1 - ((1 << lsb) - 1);
    bic(dst, dst, Operand(mask));
  } else {
    bfc(dst, lsb, width, cond);
  }
}


void MacroAssembler::Usat(Register dst, int satpos, const Operand& src,
                          Condition cond) {
  if (!CpuFeatures::IsSupported(ARMv7)) {
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
    mov(dst, Operand(0, RelocInfo::NONE), LeaveCC, mi);  // 0 if negative.
    mov(dst, Operand(satval), LeaveCC, pl);  // satval if positive.
    bind(&done);
  } else {
    usat(dst, satpos, src, cond);
  }
}


void MacroAssembler::LoadRoot(Register destination,
                              Heap::RootListIndex index,
                              Condition cond) {
  ldr(destination, MemOperand(kRootRegister, index << kPointerSizeLog2), cond);
}


void MacroAssembler::StoreRoot(Register source,
                               Heap::RootListIndex index,
                               Condition cond) {
  str(source, MemOperand(kRootRegister, index << kPointerSizeLog2), cond);
}


void MacroAssembler::RecordWriteHelper(Register object,
                                       Register address,
                                       Register scratch) {
  if (emit_debug_code()) {
    // Check that the object is not in new space.
    Label not_in_new_space;
    InNewSpace(object, scratch, ne, &not_in_new_space);
    Abort("new-space object passed to RecordWriteHelper");
    bind(&not_in_new_space);
  }

  // Calculate page address.
  Bfc(object, 0, kPageSizeBits);

  // Calculate region number.
  Ubfx(address, address, Page::kRegionSizeLog2,
       kPageSizeBits - Page::kRegionSizeLog2);

  // Mark region dirty.
  ldr(scratch, MemOperand(object, Page::kDirtyFlagOffset));
  mov(ip, Operand(1));
  orr(scratch, scratch, Operand(ip, LSL, address));
  str(scratch, MemOperand(object, Page::kDirtyFlagOffset));
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


// Will clobber 4 registers: object, offset, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(Register object,
                                 Operand offset,
                                 Register scratch0,
                                 Register scratch1) {
  // The compiled code assumes that record write doesn't change the
  // context register, so we check that none of the clobbered
  // registers are cp.
  ASSERT(!object.is(cp) && !scratch0.is(cp) && !scratch1.is(cp));

  Label done;

  // First, test that the object is not in the new space.  We cannot set
  // region marks for new space pages.
  InNewSpace(object, scratch0, eq, &done);

  // Add offset into the object.
  add(scratch0, object, offset);

  // Record the actual write.
  RecordWriteHelper(object, scratch0, scratch1);

  bind(&done);

  // Clobber all input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(object, Operand(BitCast<int32_t>(kZapValue)));
    mov(scratch0, Operand(BitCast<int32_t>(kZapValue)));
    mov(scratch1, Operand(BitCast<int32_t>(kZapValue)));
  }
}


// Will clobber 4 registers: object, address, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(Register object,
                                 Register address,
                                 Register scratch) {
  // The compiled code assumes that record write doesn't change the
  // context register, so we check that none of the clobbered
  // registers are cp.
  ASSERT(!object.is(cp) && !address.is(cp) && !scratch.is(cp));

  Label done;

  // First, test that the object is not in the new space.  We cannot set
  // region marks for new space pages.
  InNewSpace(object, scratch, eq, &done);

  // Record the actual write.
  RecordWriteHelper(object, address, scratch);

  bind(&done);

  // Clobber all input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(object, Operand(BitCast<int32_t>(kZapValue)));
    mov(address, Operand(BitCast<int32_t>(kZapValue)));
    mov(scratch, Operand(BitCast<int32_t>(kZapValue)));
  }
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
  PushSafepointRegisters();
  sub(sp, sp, Operand(DwVfpRegister::kNumAllocatableRegisters *
                      kDoubleSize));
  for (int i = 0; i < DwVfpRegister::kNumAllocatableRegisters; i++) {
    vstr(DwVfpRegister::FromAllocationIndex(i), sp, i * kDoubleSize);
  }
}


void MacroAssembler::PopSafepointRegistersAndDoubles() {
  for (int i = 0; i < DwVfpRegister::kNumAllocatableRegisters; i++) {
    vldr(DwVfpRegister::FromAllocationIndex(i), sp, i * kDoubleSize);
  }
  add(sp, sp, Operand(DwVfpRegister::kNumAllocatableRegisters *
                      kDoubleSize));
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
  // General purpose registers are pushed last on the stack.
  int doubles_size = DwVfpRegister::kNumAllocatableRegisters * kDoubleSize;
  int register_offset = SafepointRegisterStackIndex(reg.code()) * kPointerSize;
  return MemOperand(sp, doubles_size + register_offset);
}


void MacroAssembler::Ldrd(Register dst1, Register dst2,
                          const MemOperand& src, Condition cond) {
  ASSERT(src.rm().is(no_reg));
  ASSERT(!dst1.is(lr));  // r14.
  ASSERT_EQ(0, dst1.code() % 2);
  ASSERT_EQ(dst1.code() + 1, dst2.code());

  // V8 does not use this addressing mode, so the fallback code
  // below doesn't support it yet.
  ASSERT((src.am() != PreIndex) && (src.am() != NegPreIndex));

  // Generate two ldr instructions if ldrd is not available.
  if (CpuFeatures::IsSupported(ARMv7)) {
    CpuFeatures::Scope scope(ARMv7);
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
  ASSERT_EQ(0, src1.code() % 2);
  ASSERT_EQ(src1.code() + 1, src2.code());

  // V8 does not use this addressing mode, so the fallback code
  // below doesn't support it yet.
  ASSERT((dst.am() != PreIndex) && (dst.am() != NegPreIndex));

  // Generate two str instructions if strd is not available.
  if (CpuFeatures::IsSupported(ARMv7)) {
    CpuFeatures::Scope scope(ARMv7);
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


void MacroAssembler::ClearFPSCRBits(const uint32_t bits_to_clear,
                                    const Register scratch,
                                    const Condition cond) {
  vmrs(scratch, cond);
  bic(scratch, scratch, Operand(bits_to_clear), LeaveCC, cond);
  vmsr(scratch, cond);
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
                          const Condition cond) {
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  static const DoubleRepresentation minus_zero(-0.0);
  static const DoubleRepresentation zero(0.0);
  DoubleRepresentation value(imm);
  // Handle special values first.
  if (value.bits == zero.bits) {
    vmov(dst, kDoubleRegZero, cond);
  } else if (value.bits == minus_zero.bits) {
    vneg(dst, kDoubleRegZero, cond);
  } else {
    vmov(dst, imm, cond);
  }
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  // r0-r3: preserved
  stm(db_w, sp, cp.bit() | fp.bit() | lr.bit());
  mov(ip, Operand(Smi::FromInt(type)));
  push(ip);
  mov(ip, Operand(CodeObject()));
  push(ip);
  add(fp, sp, Operand(3 * kPointerSize));  // Adjust FP to point to saved FP.
}


void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  // r0: preserved
  // r1: preserved
  // r2: preserved

  // Drop the execution stack down to the frame pointer and restore
  // the caller frame pointer and return address.
  mov(sp, fp);
  ldm(ia_w, sp, fp.bit() | lr.bit());
}


void MacroAssembler::EnterExitFrame(bool save_doubles, int stack_space) {
  // Setup the frame structure on the stack.
  ASSERT_EQ(2 * kPointerSize, ExitFrameConstants::kCallerSPDisplacement);
  ASSERT_EQ(1 * kPointerSize, ExitFrameConstants::kCallerPCOffset);
  ASSERT_EQ(0 * kPointerSize, ExitFrameConstants::kCallerFPOffset);
  Push(lr, fp);
  mov(fp, Operand(sp));  // Setup new frame pointer.
  // Reserve room for saved entry sp and code object.
  sub(sp, sp, Operand(2 * kPointerSize));
  if (emit_debug_code()) {
    mov(ip, Operand(0));
    str(ip, MemOperand(fp, ExitFrameConstants::kSPOffset));
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
    DwVfpRegister first = d0;
    DwVfpRegister last =
        DwVfpRegister::from_code(DwVfpRegister::kNumRegisters - 1);
    vstm(db_w, sp, first, last);
    // Note that d0 will be accessible at
    //   fp - 2 * kPointerSize - DwVfpRegister::kNumRegisters * kDoubleSize,
    // since the sp slot and code slot were pushed after the fp.
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
  mov(scratch1, Operand(length, LSL, kSmiTagSize));
  LoadRoot(scratch2, map_index);
  str(scratch1, FieldMemOperand(string, String::kLengthOffset));
  mov(scratch1, Operand(String::kEmptyHashField));
  str(scratch2, FieldMemOperand(string, HeapObject::kMapOffset));
  str(scratch1, FieldMemOperand(string, String::kHashFieldOffset));
}


int MacroAssembler::ActivationFrameAlignment() {
#if defined(V8_HOST_ARCH_ARM)
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one ARM
  // platform for another ARM platform with a different alignment.
  return OS::ActivationFrameAlignment();
#else  // defined(V8_HOST_ARCH_ARM)
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif  // defined(V8_HOST_ARCH_ARM)
}


void MacroAssembler::LeaveExitFrame(bool save_doubles,
                                    Register argument_count) {
  // Optionally restore all double registers.
  if (save_doubles) {
    // Calculate the stack location of the saved doubles and restore them.
    const int offset = 2 * kPointerSize;
    sub(r3, fp, Operand(offset + DwVfpRegister::kNumRegisters * kDoubleSize));
    DwVfpRegister first = d0;
    DwVfpRegister last =
        DwVfpRegister::from_code(DwVfpRegister::kNumRegisters - 1);
    vldm(ia, r3, first, last);
  }

  // Clear top frame.
  mov(r3, Operand(0, RelocInfo::NONE));
  mov(ip, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  str(r3, MemOperand(ip));

  // Restore current context from top and clear it in debug mode.
  mov(ip, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
  ldr(cp, MemOperand(ip));
#ifdef DEBUG
  str(r3, MemOperand(ip));
#endif

  // Tear down the exit frame, pop the arguments, and return.
  mov(sp, Operand(fp));
  ldm(ia_w, sp, fp.bit() | lr.bit());
  if (argument_count.is_valid()) {
    add(sp, sp, Operand(argument_count, LSL, kPointerSizeLog2));
  }
}

void MacroAssembler::GetCFunctionDoubleResult(const DoubleRegister dst) {
  if (use_eabi_hardfloat()) {
    Move(dst, d0);
  } else {
    vmov(dst, r0, r1);
  }
}


void MacroAssembler::SetCallKind(Register dst, CallKind call_kind) {
  // This macro takes the dst register to make the code more readable
  // at the call sites. However, the dst register has to be r5 to
  // follow the calling convention which requires the call type to be
  // in r5.
  ASSERT(dst.is(r5));
  if (call_kind == CALL_AS_FUNCTION) {
    mov(dst, Operand(Smi::FromInt(1)));
  } else {
    mov(dst, Operand(Smi::FromInt(0)));
  }
}


void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    Register code_reg,
                                    Label* done,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper,
                                    CallKind call_kind) {
  bool definitely_matches = false;
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. If not,
  // setup registers according to contract with ArgumentsAdaptorTrampoline:
  //  r0: actual arguments count
  //  r1: function (passed through to callee)
  //  r2: expected arguments count
  //  r3: callee code entry

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
      SetCallKind(r5, call_kind);
      Call(adaptor);
      call_wrapper.AfterCall();
      b(done);
    } else {
      SetCallKind(r5, call_kind);
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&regular_invoke);
  }
}


void MacroAssembler::InvokeCode(Register code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                InvokeFlag flag,
                                const CallWrapper& call_wrapper,
                                CallKind call_kind) {
  Label done;

  InvokePrologue(expected, actual, Handle<Code>::null(), code, &done, flag,
                 call_wrapper, call_kind);
  if (flag == CALL_FUNCTION) {
    call_wrapper.BeforeCall(CallSize(code));
    SetCallKind(r5, call_kind);
    Call(code);
    call_wrapper.AfterCall();
  } else {
    ASSERT(flag == JUMP_FUNCTION);
    SetCallKind(r5, call_kind);
    Jump(code);
  }

  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  bind(&done);
}


void MacroAssembler::InvokeCode(Handle<Code> code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                RelocInfo::Mode rmode,
                                InvokeFlag flag,
                                CallKind call_kind) {
  Label done;

  InvokePrologue(expected, actual, code, no_reg, &done, flag,
                 NullCallWrapper(), call_kind);
  if (flag == CALL_FUNCTION) {
    SetCallKind(r5, call_kind);
    Call(code, rmode);
  } else {
    SetCallKind(r5, call_kind);
    Jump(code, rmode);
  }

  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  bind(&done);
}


void MacroAssembler::InvokeFunction(Register fun,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper,
                                    CallKind call_kind) {
  // Contract with called JS functions requires that function is passed in r1.
  ASSERT(fun.is(r1));

  Register expected_reg = r2;
  Register code_reg = r3;

  ldr(code_reg, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));
  ldr(expected_reg,
      FieldMemOperand(code_reg,
                      SharedFunctionInfo::kFormalParameterCountOffset));
  mov(expected_reg, Operand(expected_reg, ASR, kSmiTagSize));
  ldr(code_reg,
      FieldMemOperand(r1, JSFunction::kCodeEntryOffset));

  ParameterCount expected(expected_reg);
  InvokeCode(code_reg, expected, actual, flag, call_wrapper, call_kind);
}


void MacroAssembler::InvokeFunction(JSFunction* function,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    CallKind call_kind) {
  ASSERT(function->is_compiled());

  // Get the function and setup the context.
  mov(r1, Operand(Handle<JSFunction>(function)));
  ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // Invoke the cached code.
  Handle<Code> code(function->code());
  ParameterCount expected(function->shared()->formal_parameter_count());
  if (V8::UseCrankshaft()) {
    // TODO(kasperl): For now, we always call indirectly through the
    // code field in the function to allow recompilation to take effect
    // without changing any of the call sites.
    ldr(r3, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
    InvokeCode(r3, expected, actual, flag, NullCallWrapper(), call_kind);
  } else {
    InvokeCode(code, expected, actual, RelocInfo::CODE_TARGET, flag, call_kind);
  }
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


#ifdef ENABLE_DEBUGGER_SUPPORT
void MacroAssembler::DebugBreak() {
  ASSERT(allow_stub_calls());
  mov(r0, Operand(0, RelocInfo::NONE));
  mov(r1, Operand(ExternalReference(Runtime::kDebugBreak, isolate())));
  CEntryStub ces(1);
  Call(ces.GetCode(), RelocInfo::DEBUG_BREAK);
}
#endif


void MacroAssembler::PushTryHandler(CodeLocation try_location,
                                    HandlerType type) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kPCOffset == 4 * kPointerSize);

  // The pc (return address) is passed in register lr.
  if (try_location == IN_JAVASCRIPT) {
    if (type == TRY_CATCH_HANDLER) {
      mov(r3, Operand(StackHandler::TRY_CATCH));
    } else {
      mov(r3, Operand(StackHandler::TRY_FINALLY));
    }
    stm(db_w, sp, r3.bit() | cp.bit() | fp.bit() | lr.bit());
    // Save the current handler as the next handler.
    mov(r3, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
    ldr(r1, MemOperand(r3));
    push(r1);
    // Link this handler as the new current one.
    str(sp, MemOperand(r3));
  } else {
    // Must preserve r0-r4, r5-r7 are available.
    ASSERT(try_location == IN_JS_ENTRY);
    // The frame pointer does not point to a JS frame so we save NULL
    // for fp. We expect the code throwing an exception to check fp
    // before dereferencing it to restore the context.
    mov(r5, Operand(StackHandler::ENTRY));  // State.
    mov(r6, Operand(Smi::FromInt(0)));  // Indicates no context.
    mov(r7, Operand(0, RelocInfo::NONE));  // NULL frame pointer.
    stm(db_w, sp, r5.bit() | r6.bit() | r7.bit() | lr.bit());
    // Save the current handler as the next handler.
    mov(r7, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
    ldr(r6, MemOperand(r7));
    push(r6);
    // Link this handler as the new current one.
    str(sp, MemOperand(r7));
  }
}


void MacroAssembler::PopTryHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  pop(r1);
  mov(ip, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  add(sp, sp, Operand(StackHandlerConstants::kSize - kPointerSize));
  str(r1, MemOperand(ip));
}


void MacroAssembler::Throw(Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kPCOffset == 4 * kPointerSize);
  // r0 is expected to hold the exception.
  if (!value.is(r0)) {
    mov(r0, value);
  }

  // Drop the sp to the top of the handler.
  mov(r3, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  ldr(sp, MemOperand(r3));

  // Restore the next handler.
  pop(r2);
  str(r2, MemOperand(r3));

  // Restore context and frame pointer, discard state (r3).
  ldm(ia_w, sp, r3.bit() | cp.bit() | fp.bit());

  // If the handler is a JS frame, restore the context to the frame.
  // (r3 == ENTRY) == (fp == 0) == (cp == 0), so we could test any
  // of them.
  cmp(r3, Operand(StackHandler::ENTRY));
  str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset), ne);

#ifdef DEBUG
  if (emit_debug_code()) {
    mov(lr, Operand(pc));
  }
#endif
  pop(pc);
}


void MacroAssembler::ThrowUncatchable(UncatchableExceptionType type,
                                      Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kPCOffset == 4 * kPointerSize);
  // r0 is expected to hold the exception.
  if (!value.is(r0)) {
    mov(r0, value);
  }

  // Drop sp to the top stack handler.
  mov(r3, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  ldr(sp, MemOperand(r3));

  // Unwind the handlers until the ENTRY handler is found.
  Label loop, done;
  bind(&loop);
  // Load the type of the current stack handler.
  const int kStateOffset = StackHandlerConstants::kStateOffset;
  ldr(r2, MemOperand(sp, kStateOffset));
  cmp(r2, Operand(StackHandler::ENTRY));
  b(eq, &done);
  // Fetch the next handler in the list.
  const int kNextOffset = StackHandlerConstants::kNextOffset;
  ldr(sp, MemOperand(sp, kNextOffset));
  jmp(&loop);
  bind(&done);

  // Set the top handler address to next handler past the current ENTRY handler.
  pop(r2);
  str(r2, MemOperand(r3));

  if (type == OUT_OF_MEMORY) {
    // Set external caught exception to false.
    ExternalReference external_caught(
        Isolate::kExternalCaughtExceptionAddress, isolate());
    mov(r0, Operand(false, RelocInfo::NONE));
    mov(r2, Operand(external_caught));
    str(r0, MemOperand(r2));

    // Set pending exception and r0 to out of memory exception.
    Failure* out_of_memory = Failure::OutOfMemoryException();
    mov(r0, Operand(reinterpret_cast<int32_t>(out_of_memory)));
    mov(r2, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                      isolate())));
    str(r0, MemOperand(r2));
  }

  // Stack layout at this point. See also StackHandlerConstants.
  // sp ->   state (ENTRY)
  //         cp
  //         fp
  //         lr

  // Restore context and frame pointer, discard state (r2).
  ldm(ia_w, sp, r2.bit() | cp.bit() | fp.bit());
#ifdef DEBUG
  if (emit_debug_code()) {
    mov(lr, Operand(pc));
  }
#endif
  pop(pc);
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
  cmp(scratch, Operand(0, RelocInfo::NONE));
  Check(ne, "we should not have an empty lexical context");
#endif

  // Load the global context of the current context.
  int offset = Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
  ldr(scratch, FieldMemOperand(scratch, offset));
  ldr(scratch, FieldMemOperand(scratch, GlobalObject::kGlobalContextOffset));

  // Check the context is a global context.
  if (emit_debug_code()) {
    // TODO(119): avoid push(holder_reg)/pop(holder_reg)
    // Cannot use ip as a temporary in this verification code. Due to the fact
    // that ip is clobbered as part of cmp with an object Operand.
    push(holder_reg);  // Temporarily save holder on the stack.
    // Read the first word and compare to the global_context_map.
    ldr(holder_reg, FieldMemOperand(scratch, HeapObject::kMapOffset));
    LoadRoot(ip, Heap::kGlobalContextMapRootIndex);
    cmp(holder_reg, ip);
    Check(eq, "JSGlobalObject::global_context should be a global context.");
    pop(holder_reg);  // Restore holder.
  }

  // Check if both contexts are the same.
  ldr(ip, FieldMemOperand(holder_reg, JSGlobalProxy::kContextOffset));
  cmp(scratch, Operand(ip));
  b(eq, &same_contexts);

  // Check the context is a global context.
  if (emit_debug_code()) {
    // TODO(119): avoid push(holder_reg)/pop(holder_reg)
    // Cannot use ip as a temporary in this verification code. Due to the fact
    // that ip is clobbered as part of cmp with an object Operand.
    push(holder_reg);  // Temporarily save holder on the stack.
    mov(holder_reg, ip);  // Move ip to its holding place.
    LoadRoot(ip, Heap::kNullValueRootIndex);
    cmp(holder_reg, ip);
    Check(ne, "JSGlobalProxy::context() should not be null.");

    ldr(holder_reg, FieldMemOperand(holder_reg, HeapObject::kMapOffset));
    LoadRoot(ip, Heap::kGlobalContextMapRootIndex);
    cmp(holder_reg, ip);
    Check(eq, "JSGlobalObject::global_context should be a global context.");
    // Restore ip is not needed. ip is reloaded below.
    pop(holder_reg);  // Restore holder.
    // Restore ip to holder's context.
    ldr(ip, FieldMemOperand(holder_reg, JSGlobalProxy::kContextOffset));
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
  mov(scratch, Operand(2057));
  mul(t0, t0, scratch);
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
  mov(t1, Operand(t1, ASR, kSmiTagSize));  // convert smi to int
  sub(t1, t1, Operand(1));

  // Generate an unrolled loop that performs a few probes before giving up.
  static const int kProbes = 4;
  for (int i = 0; i < kProbes; i++) {
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
    if (i != kProbes - 1) {
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


void MacroAssembler::AllocateInNewSpace(int object_size,
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
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address(isolate());
  intptr_t top   =
      reinterpret_cast<intptr_t>(new_space_allocation_top.address());
  intptr_t limit =
      reinterpret_cast<intptr_t>(new_space_allocation_limit.address());
  ASSERT((limit - top) == kPointerSize);
  ASSERT(result.code() < ip.code());

  // Set up allocation top address and object size registers.
  Register topaddr = scratch1;
  Register obj_size_reg = scratch2;
  mov(topaddr, Operand(new_space_allocation_top));
  mov(obj_size_reg, Operand(object_size));

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
      Check(eq, "Unexpected allocation top");
    }
    // Load allocation limit into ip. Result already contains allocation top.
    ldr(ip, MemOperand(topaddr, limit - top));
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top.
  add(scratch2, result, Operand(obj_size_reg), SetCC);
  b(cs, gc_required);
  cmp(scratch2, Operand(ip));
  b(hi, gc_required);
  str(scratch2, MemOperand(topaddr));

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    add(result, result, Operand(kHeapObjectTag));
  }
}


void MacroAssembler::AllocateInNewSpace(Register object_size,
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
  ASSERT(!result.is(ip));
  ASSERT(!scratch1.is(ip));
  ASSERT(!scratch2.is(ip));

  // Check relative positions of allocation top and limit addresses.
  // The values must be adjacent in memory to allow the use of LDM.
  // Also, assert that the registers are numbered such that the values
  // are loaded in the correct order.
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address(isolate());
  intptr_t top =
      reinterpret_cast<intptr_t>(new_space_allocation_top.address());
  intptr_t limit =
      reinterpret_cast<intptr_t>(new_space_allocation_limit.address());
  ASSERT((limit - top) == kPointerSize);
  ASSERT(result.code() < ip.code());

  // Set up allocation top address.
  Register topaddr = scratch1;
  mov(topaddr, Operand(new_space_allocation_top));

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
      Check(eq, "Unexpected allocation top");
    }
    // Load allocation limit into ip. Result already contains allocation top.
    ldr(ip, MemOperand(topaddr, limit - top));
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
    Check(eq, "Unaligned allocation in new space");
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
  Check(lt, "Undo allocation of non allocated memory");
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
  AllocateInNewSpace(scratch1,
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
  ASSERT((SeqAsciiString::kHeaderSize & kObjectAlignmentMask) == 0);
  ASSERT(kCharSize == 1);
  add(scratch1, length,
      Operand(kObjectAlignmentMask + SeqAsciiString::kHeaderSize));
  and_(scratch1, scratch1, Operand(~kObjectAlignmentMask));

  // Allocate ASCII string in new space.
  AllocateInNewSpace(scratch1,
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
  AllocateInNewSpace(ConsString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
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
  AllocateInNewSpace(ConsString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);

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
  AllocateInNewSpace(SlicedString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
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
  AllocateInNewSpace(SlicedString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
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
  ldr(map, FieldMemOperand(object, HeapObject::kMapOffset));
  CompareInstanceType(map, type_reg, type);
}


void MacroAssembler::CompareInstanceType(Register map,
                                         Register type_reg,
                                         InstanceType type) {
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
  STATIC_ASSERT(FAST_ELEMENTS == 0);
  ldrb(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  cmp(scratch, Operand(Map::kMaximumBitField2FastElementValue));
  b(hi, fail);
}


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Handle<Map> map,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }
  ldr(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  mov(ip, Operand(map));
  cmp(scratch, ip);
  b(ne, fail);
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
                                             Label* miss) {
  // Check that the receiver isn't a smi.
  JumpIfSmi(function, miss);

  // Check that the function really is a function.  Load map into result reg.
  CompareObjectType(function, result, scratch, JS_FUNCTION_TYPE);
  b(ne, miss);

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


void MacroAssembler::CallStub(CodeStub* stub, Condition cond) {
  ASSERT(allow_stub_calls());  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(), RelocInfo::CODE_TARGET, kNoASTId, cond);
}


MaybeObject* MacroAssembler::TryCallStub(CodeStub* stub, Condition cond) {
  ASSERT(allow_stub_calls());  // Stub calls are not allowed in some stubs.
  Object* result;
  { MaybeObject* maybe_result = stub->TryGetCode();
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  Handle<Code> code(Code::cast(result));
  Call(code, RelocInfo::CODE_TARGET, kNoASTId, cond);
  return result;
}


void MacroAssembler::TailCallStub(CodeStub* stub, Condition cond) {
  ASSERT(allow_stub_calls());  // Stub calls are not allowed in some stubs.
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET, cond);
}


MaybeObject* MacroAssembler::TryTailCallStub(CodeStub* stub, Condition cond) {
  ASSERT(allow_stub_calls());  // Stub calls are not allowed in some stubs.
  Object* result;
  { MaybeObject* maybe_result = stub->TryGetCode();
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  Jump(Handle<Code>(Code::cast(result)), RelocInfo::CODE_TARGET, cond);
  return result;
}


static int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  return ref0.address() - ref1.address();
}


MaybeObject* MacroAssembler::TryCallApiFunctionAndReturn(
    ExternalReference function, int stack_space) {
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address();
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(),
      next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(),
      next_address);

  // Allocate HandleScope in callee-save registers.
  mov(r7, Operand(next_address));
  ldr(r4, MemOperand(r7, kNextOffset));
  ldr(r5, MemOperand(r7, kLimitOffset));
  ldr(r6, MemOperand(r7, kLevelOffset));
  add(r6, r6, Operand(1));
  str(r6, MemOperand(r7, kLevelOffset));

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
  DirectCEntryStub stub;
  stub.GenerateCall(this, function);

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;

  // If result is non-zero, dereference to get the result value
  // otherwise set it to undefined.
  cmp(r0, Operand(0));
  LoadRoot(r0, Heap::kUndefinedValueRootIndex, eq);
  ldr(r0, MemOperand(r0), ne);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  str(r4, MemOperand(r7, kNextOffset));
  if (emit_debug_code()) {
    ldr(r1, MemOperand(r7, kLevelOffset));
    cmp(r1, r6);
    Check(eq, "Unexpected level after return from api call");
  }
  sub(r6, r6, Operand(1));
  str(r6, MemOperand(r7, kLevelOffset));
  ldr(ip, MemOperand(r7, kLimitOffset));
  cmp(r5, ip);
  b(ne, &delete_allocated_handles);

  // Check if the function scheduled an exception.
  bind(&leave_exit_frame);
  LoadRoot(r4, Heap::kTheHoleValueRootIndex);
  mov(ip, Operand(ExternalReference::scheduled_exception_address(isolate())));
  ldr(r5, MemOperand(ip));
  cmp(r4, r5);
  b(ne, &promote_scheduled_exception);

  // LeaveExitFrame expects unwind space to be in a register.
  mov(r4, Operand(stack_space));
  LeaveExitFrame(false, r4);
  mov(pc, lr);

  bind(&promote_scheduled_exception);
  MaybeObject* result
      = TryTailCallExternalReference(
          ExternalReference(Runtime::kPromoteScheduledException, isolate()),
          0,
          1);
  if (result->IsFailure()) {
    return result;
  }

  // HandleScope limit has changed. Delete allocated extensions.
  bind(&delete_allocated_handles);
  str(r5, MemOperand(r7, kLimitOffset));
  mov(r4, r0);
  PrepareCallCFunction(1, r5);
  mov(r0, Operand(ExternalReference::isolate_address()));
  CallCFunction(
      ExternalReference::delete_handle_scope_extensions(isolate()), 1);
  mov(r0, r4);
  jmp(&leave_exit_frame);

  return result;
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
  STATIC_ASSERT(kSmiTag == 0);
  Ubfx(hash, hash, String::kHashShift, String::kArrayIndexValueBits);
  mov(index, Operand(hash, LSL, kSmiTagSize));
}


void MacroAssembler::IntegerToDoubleConversionWithVFP3(Register inReg,
                                                       Register outHighReg,
                                                       Register outLowReg) {
  // ARMv7 VFP3 instructions to implement integer to double conversion.
  mov(r7, Operand(inReg, ASR, kSmiTagSize));
  vmov(s15, r7);
  vcvt_f64_s32(d7, s15);
  vmov(outLowReg, outHighReg, d7);
}


void MacroAssembler::ObjectToDoubleVFPRegister(Register object,
                                               DwVfpRegister result,
                                               Register scratch1,
                                               Register scratch2,
                                               Register heap_number_map,
                                               SwVfpRegister scratch3,
                                               Label* not_number,
                                               ObjectToDoubleFlags flags) {
  Label done;
  if ((flags & OBJECT_NOT_SMI) == 0) {
    Label not_smi;
    JumpIfNotSmi(object, &not_smi);
    // Remove smi tag and convert to double.
    mov(scratch1, Operand(object, ASR, kSmiTagSize));
    vmov(scratch3, scratch1);
    vcvt_f64_s32(result, scratch3);
    b(&done);
    bind(&not_smi);
  }
  // Check for heap number and load double value from it.
  ldr(scratch1, FieldMemOperand(object, HeapObject::kMapOffset));
  sub(scratch2, object, Operand(kHeapObjectTag));
  cmp(scratch1, heap_number_map);
  b(ne, not_number);
  if ((flags & AVOID_NANS_AND_INFINITIES) != 0) {
    // If exponent is all ones the number is either a NaN or +/-Infinity.
    ldr(scratch1, FieldMemOperand(object, HeapNumber::kExponentOffset));
    Sbfx(scratch1,
         scratch1,
         HeapNumber::kExponentShift,
         HeapNumber::kExponentBits);
    // All-one value sign extend to -1.
    cmp(scratch1, Operand(-1));
    b(eq, not_number);
  }
  vldr(result, scratch2, HeapNumber::kValueOffset);
  bind(&done);
}


void MacroAssembler::SmiToDoubleVFPRegister(Register smi,
                                            DwVfpRegister value,
                                            Register scratch1,
                                            SwVfpRegister scratch2) {
  mov(scratch1, Operand(smi, ASR, kSmiTagSize));
  vmov(scratch2, scratch1);
  vcvt_f64_s32(value, scratch2);
}


// Tries to get a signed int32 out of a double precision floating point heap
// number. Rounds towards 0. Branch to 'not_int32' if the double is out of the
// 32bits signed integer range.
void MacroAssembler::ConvertToInt32(Register source,
                                    Register dest,
                                    Register scratch,
                                    Register scratch2,
                                    DwVfpRegister double_scratch,
                                    Label *not_int32) {
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    sub(scratch, source, Operand(kHeapObjectTag));
    vldr(double_scratch, scratch, HeapNumber::kValueOffset);
    vcvt_s32_f64(double_scratch.low(), double_scratch);
    vmov(dest, double_scratch.low());
    // Signed vcvt instruction will saturate to the minimum (0x80000000) or
    // maximun (0x7fffffff) signed 32bits integer when the double is out of
    // range. When substracting one, the minimum signed integer becomes the
    // maximun signed integer.
    sub(scratch, dest, Operand(1));
    cmp(scratch, Operand(LONG_MAX - 1));
    // If equal then dest was LONG_MAX, if greater dest was LONG_MIN.
    b(ge, not_int32);
  } else {
    // This code is faster for doubles that are in the ranges -0x7fffffff to
    // -0x40000000 or 0x40000000 to 0x7fffffff. This corresponds almost to
    // the range of signed int32 values that are not Smis.  Jumps to the label
    // 'not_int32' if the double isn't in the range -0x80000000.0 to
    // 0x80000000.0 (excluding the endpoints).
    Label right_exponent, done;
    // Get exponent word.
    ldr(scratch, FieldMemOperand(source, HeapNumber::kExponentOffset));
    // Get exponent alone in scratch2.
    Ubfx(scratch2,
         scratch,
         HeapNumber::kExponentShift,
         HeapNumber::kExponentBits);
    // Load dest with zero.  We use this either for the final shift or
    // for the answer.
    mov(dest, Operand(0, RelocInfo::NONE));
    // Check whether the exponent matches a 32 bit signed int that is not a Smi.
    // A non-Smi integer is 1.xxx * 2^30 so the exponent is 30 (biased). This is
    // the exponent that we are fastest at and also the highest exponent we can
    // handle here.
    const uint32_t non_smi_exponent = HeapNumber::kExponentBias + 30;
    // The non_smi_exponent, 0x41d, is too big for ARM's immediate field so we
    // split it up to avoid a constant pool entry.  You can't do that in general
    // for cmp because of the overflow flag, but we know the exponent is in the
    // range 0-2047 so there is no overflow.
    int fudge_factor = 0x400;
    sub(scratch2, scratch2, Operand(fudge_factor));
    cmp(scratch2, Operand(non_smi_exponent - fudge_factor));
    // If we have a match of the int32-but-not-Smi exponent then skip some
    // logic.
    b(eq, &right_exponent);
    // If the exponent is higher than that then go to slow case.  This catches
    // numbers that don't fit in a signed int32, infinities and NaNs.
    b(gt, not_int32);

    // We know the exponent is smaller than 30 (biased).  If it is less than
    // 0 (biased) then the number is smaller in magnitude than 1.0 * 2^0, ie
    // it rounds to zero.
    const uint32_t zero_exponent = HeapNumber::kExponentBias + 0;
    sub(scratch2, scratch2, Operand(zero_exponent - fudge_factor), SetCC);
    // Dest already has a Smi zero.
    b(lt, &done);

    // We have an exponent between 0 and 30 in scratch2.  Subtract from 30 to
    // get how much to shift down.
    rsb(dest, scratch2, Operand(30));

    bind(&right_exponent);
    // Get the top bits of the mantissa.
    and_(scratch2, scratch, Operand(HeapNumber::kMantissaMask));
    // Put back the implicit 1.
    orr(scratch2, scratch2, Operand(1 << HeapNumber::kExponentShift));
    // Shift up the mantissa bits to take up the space the exponent used to
    // take. We just orred in the implicit bit so that took care of one and
    // we want to leave the sign bit 0 so we subtract 2 bits from the shift
    // distance.
    const int shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 2;
    mov(scratch2, Operand(scratch2, LSL, shift_distance));
    // Put sign in zero flag.
    tst(scratch, Operand(HeapNumber::kSignMask));
    // Get the second half of the double. For some exponents we don't
    // actually need this because the bits get shifted out again, but
    // it's probably slower to test than just to do it.
    ldr(scratch, FieldMemOperand(source, HeapNumber::kMantissaOffset));
    // Shift down 22 bits to get the last 10 bits.
    orr(scratch, scratch2, Operand(scratch, LSR, 32 - shift_distance));
    // Move down according to the exponent.
    mov(dest, Operand(scratch, LSR, dest));
    // Fix sign if sign bit was set.
    rsb(dest, dest, Operand(0, RelocInfo::NONE), LeaveCC, ne);
    bind(&done);
  }
}


void MacroAssembler::EmitVFPTruncate(VFPRoundingMode rounding_mode,
                                     SwVfpRegister result,
                                     DwVfpRegister double_input,
                                     Register scratch1,
                                     Register scratch2,
                                     CheckForInexactConversion check_inexact) {
  ASSERT(CpuFeatures::IsSupported(VFP3));
  CpuFeatures::Scope scope(VFP3);
  Register prev_fpscr = scratch1;
  Register scratch = scratch2;

  int32_t check_inexact_conversion =
    (check_inexact == kCheckForInexactConversion) ? kVFPInexactExceptionBit : 0;

  // Set custom FPCSR:
  //  - Set rounding mode.
  //  - Clear vfp cumulative exception flags.
  //  - Make sure Flush-to-zero mode control bit is unset.
  vmrs(prev_fpscr);
  bic(scratch,
      prev_fpscr,
      Operand(kVFPExceptionMask |
              check_inexact_conversion |
              kVFPRoundingModeMask |
              kVFPFlushToZeroMask));
  // 'Round To Nearest' is encoded by 0b00 so no bits need to be set.
  if (rounding_mode != kRoundToNearest) {
    orr(scratch, scratch, Operand(rounding_mode));
  }
  vmsr(scratch);

  // Convert the argument to an integer.
  vcvt_s32_f64(result,
               double_input,
               (rounding_mode == kRoundToZero) ? kDefaultRoundToZero
                                               : kFPSCRRounding);

  // Retrieve FPSCR.
  vmrs(scratch);
  // Restore FPSCR.
  vmsr(prev_fpscr);
  // Check for vfp exceptions.
  tst(scratch, Operand(kVFPExceptionMask | check_inexact_conversion));
}


void MacroAssembler::EmitOutOfInt32RangeTruncate(Register result,
                                                 Register input_high,
                                                 Register input_low,
                                                 Register scratch) {
  Label done, normal_exponent, restore_sign;

  // Extract the biased exponent in result.
  Ubfx(result,
       input_high,
       HeapNumber::kExponentShift,
       HeapNumber::kExponentBits);

  // Check for Infinity and NaNs, which should return 0.
  cmp(result, Operand(HeapNumber::kExponentMask));
  mov(result, Operand(0), LeaveCC, eq);
  b(eq, &done);

  // Express exponent as delta to (number of mantissa bits + 31).
  sub(result,
      result,
      Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits + 31),
      SetCC);

  // If the delta is strictly positive, all bits would be shifted away,
  // which means that we can return 0.
  b(le, &normal_exponent);
  mov(result, Operand(0));
  b(&done);

  bind(&normal_exponent);
  const int kShiftBase = HeapNumber::kNonMantissaBitsInTopWord - 1;
  // Calculate shift.
  add(scratch, result, Operand(kShiftBase + HeapNumber::kMantissaBits), SetCC);

  // Save the sign.
  Register sign = result;
  result = no_reg;
  and_(sign, input_high, Operand(HeapNumber::kSignMask));

  // Set the implicit 1 before the mantissa part in input_high.
  orr(input_high,
      input_high,
      Operand(1 << HeapNumber::kMantissaBitsInTopWord));
  // Shift the mantissa bits to the correct position.
  // We don't need to clear non-mantissa bits as they will be shifted away.
  // If they weren't, it would mean that the answer is in the 32bit range.
  mov(input_high, Operand(input_high, LSL, scratch));

  // Replace the shifted bits with bits from the lower mantissa word.
  Label pos_shift, shift_done;
  rsb(scratch, scratch, Operand(32), SetCC);
  b(&pos_shift, ge);

  // Negate scratch.
  rsb(scratch, scratch, Operand(0));
  mov(input_low, Operand(input_low, LSL, scratch));
  b(&shift_done);

  bind(&pos_shift);
  mov(input_low, Operand(input_low, LSR, scratch));

  bind(&shift_done);
  orr(input_high, input_high, Operand(input_low));
  // Restore sign if necessary.
  cmp(sign, Operand(0));
  result = sign;
  sign = no_reg;
  rsb(result, input_high, Operand(0), LeaveCC, ne);
  mov(result, input_high, LeaveCC, eq);
  bind(&done);
}


void MacroAssembler::EmitECMATruncate(Register result,
                                      DwVfpRegister double_input,
                                      SwVfpRegister single_scratch,
                                      Register scratch,
                                      Register input_high,
                                      Register input_low) {
  CpuFeatures::Scope scope(VFP3);
  ASSERT(!input_high.is(result));
  ASSERT(!input_low.is(result));
  ASSERT(!input_low.is(input_high));
  ASSERT(!scratch.is(result) &&
         !scratch.is(input_high) &&
         !scratch.is(input_low));
  ASSERT(!single_scratch.is(double_input.low()) &&
         !single_scratch.is(double_input.high()));

  Label done;

  // Clear cumulative exception flags.
  ClearFPSCRBits(kVFPExceptionMask, scratch);
  // Try a conversion to a signed integer.
  vcvt_s32_f64(single_scratch, double_input);
  vmov(result, single_scratch);
  // Retrieve he FPSCR.
  vmrs(scratch);
  // Check for overflow and NaNs.
  tst(scratch, Operand(kVFPOverflowExceptionBit |
                       kVFPUnderflowExceptionBit |
                       kVFPInvalidOpExceptionBit));
  // If we had no exceptions we are done.
  b(eq, &done);

  // Load the double value and perform a manual truncation.
  vmov(input_low, input_high, double_input);
  EmitOutOfInt32RangeTruncate(result,
                              input_high,
                              input_low,
                              scratch);
  bind(&done);
}


void MacroAssembler::GetLeastBitsFromSmi(Register dst,
                                         Register src,
                                         int num_least_bits) {
  if (CpuFeatures::IsSupported(ARMv7)) {
    ubfx(dst, src, kSmiTagSize, num_least_bits);
  } else {
    mov(dst, Operand(src, ASR, kSmiTagSize));
    and_(dst, dst, Operand((1 << num_least_bits) - 1));
  }
}


void MacroAssembler::GetLeastBitsFromInt32(Register dst,
                                           Register src,
                                           int num_least_bits) {
  and_(dst, src, Operand((1 << num_least_bits) - 1));
}


void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments) {
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
  CEntryStub stub(1);
  CallStub(&stub);
}


void MacroAssembler::CallRuntime(Runtime::FunctionId fid, int num_arguments) {
  CallRuntime(Runtime::FunctionForId(fid), num_arguments);
}


void MacroAssembler::CallRuntimeSaveDoubles(Runtime::FunctionId id) {
  const Runtime::Function* function = Runtime::FunctionForId(id);
  mov(r0, Operand(function->nargs));
  mov(r1, Operand(ExternalReference(function, isolate())));
  CEntryStub stub(1);
  stub.SaveDoubles();
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


MaybeObject* MacroAssembler::TryTailCallExternalReference(
    const ExternalReference& ext, int num_arguments, int result_size) {
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r0, Operand(num_arguments));
  return TryJumpToExternalReference(ext);
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
  Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


MaybeObject* MacroAssembler::TryJumpToExternalReference(
    const ExternalReference& builtin) {
#if defined(__thumb__)
  // Thumb mode builtin.
  ASSERT((reinterpret_cast<intptr_t>(builtin.address()) & 1) == 1);
#endif
  mov(r1, Operand(builtin));
  CEntryStub stub(1);
  return TryTailCallStub(&stub);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeFlag flag,
                                   const CallWrapper& call_wrapper) {
  GetBuiltinEntry(r2, id);
  if (flag == CALL_FUNCTION) {
    call_wrapper.BeforeCall(CallSize(r2));
    SetCallKind(r5, CALL_AS_METHOD);
    Call(r2);
    call_wrapper.AfterCall();
  } else {
    ASSERT(flag == JUMP_FUNCTION);
    SetCallKind(r5, CALL_AS_METHOD);
    Jump(r2);
  }
}


void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the builtins object into target register.
  ldr(target, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
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


void MacroAssembler::Assert(Condition cond, const char* msg) {
  if (emit_debug_code())
    Check(cond, msg);
}


void MacroAssembler::AssertRegisterIsRoot(Register reg,
                                          Heap::RootListIndex index) {
  if (emit_debug_code()) {
    LoadRoot(ip, index);
    cmp(reg, ip);
    Check(eq, "Register did not match expected root");
  }
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
    Abort("JSObject with fast elements map has slow elements");
    bind(&ok);
    pop(elements);
  }
}


void MacroAssembler::Check(Condition cond, const char* msg) {
  Label L;
  b(cond, &L);
  Abort(msg);
  // will not return here
  bind(&L);
}


void MacroAssembler::Abort(const char* msg) {
  Label abort_start;
  bind(&abort_start);
  // We want to pass the msg string like a smi to avoid GC
  // problems, however msg is not guaranteed to be aligned
  // properly. Instead, we pass an aligned pointer that is
  // a proper v8 smi, but also pass the alignment difference
  // from the real pointer as a smi.
  intptr_t p1 = reinterpret_cast<intptr_t>(msg);
  intptr_t p0 = (p1 & ~kSmiTagMask) + kSmiTag;
  ASSERT(reinterpret_cast<Object*>(p0)->IsSmi());
#ifdef DEBUG
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }
#endif
  // Disable stub call restrictions to always allow calls to abort.
  AllowStubCallsScope allow_scope(this, true);

  mov(r0, Operand(p0));
  push(r0);
  mov(r0, Operand(Smi::FromInt(p1 - p0)));
  push(r0);
  CallRuntime(Runtime::kAbort, 2);
  // will not return here
  if (is_const_pool_blocked()) {
    // If the calling code cares about the exact number of
    // instructions generated, we insert padding here to keep the size
    // of the Abort macro constant.
    static const int kExpectedAbortInstructions = 10;
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


void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the global or builtins object from the current context.
  ldr(function, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  // Load the global context from the global or builtins object.
  ldr(function, FieldMemOperand(function,
                                GlobalObject::kGlobalContextOffset));
  // Load the function from the global context.
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
    Abort("Global functions must have initial map");
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


void MacroAssembler::JumpIfEitherSmi(Register reg1,
                                     Register reg2,
                                     Label* on_either_smi) {
  STATIC_ASSERT(kSmiTag == 0);
  tst(reg1, Operand(kSmiTagMask));
  tst(reg2, Operand(kSmiTagMask), ne);
  b(eq, on_either_smi);
}


void MacroAssembler::AbortIfSmi(Register object) {
  STATIC_ASSERT(kSmiTag == 0);
  tst(object, Operand(kSmiTagMask));
  Assert(ne, "Operand is a smi");
}


void MacroAssembler::AbortIfNotSmi(Register object) {
  STATIC_ASSERT(kSmiTag == 0);
  tst(object, Operand(kSmiTagMask));
  Assert(eq, "Operand is not smi");
}


void MacroAssembler::AbortIfNotString(Register object) {
  STATIC_ASSERT(kSmiTag == 0);
  tst(object, Operand(kSmiTagMask));
  Assert(ne, "Operand is not a string");
  push(object);
  ldr(object, FieldMemOperand(object, HeapObject::kMapOffset));
  CompareInstanceType(object, object, FIRST_NONSTRING_TYPE);
  pop(object);
  Assert(lo, "Operand is not a string");
}



void MacroAssembler::AbortIfNotRootValue(Register src,
                                         Heap::RootListIndex root_value_index,
                                         const char* message) {
  CompareRoot(src, root_value_index);
  Assert(eq, message);
}


void MacroAssembler::JumpIfNotHeapNumber(Register object,
                                         Register heap_number_map,
                                         Register scratch,
                                         Label* on_not_heap_number) {
  ldr(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  cmp(scratch, heap_number_map);
  b(ne, on_not_heap_number);
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
  STATIC_ASSERT(kSmiTag == 0);
  and_(scratch1, first, Operand(second));
  JumpIfSmi(scratch1, failure);
  JumpIfNonSmisNotBothSequentialAsciiStrings(first,
                                             second,
                                             scratch1,
                                             scratch2,
                                             failure);
}


// Allocates a heap number or jumps to the need_gc label if the young space
// is full and a scavenge is needed.
void MacroAssembler::AllocateHeapNumber(Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Register heap_number_map,
                                        Label* gc_required) {
  // Allocate an object in the heap for the heap number and tag it as a heap
  // object.
  AllocateInNewSpace(HeapNumber::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);

  // Store heap number map in the allocated object.
  AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  str(heap_number_map, FieldMemOperand(result, HeapObject::kMapOffset));
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
                                RegList temps,
                                int field_count) {
  // At least one bit set in the first 15 registers.
  ASSERT((temps & ((1 << 15) - 1)) != 0);
  ASSERT((temps & dst.bit()) == 0);
  ASSERT((temps & src.bit()) == 0);
  // Primitive implementation using only one temporary register.

  Register tmp = no_reg;
  // Find a temp register in temps list.
  for (int i = 0; i < 15; i++) {
    if ((temps & (1 << i)) != 0) {
      tmp.set_code(i);
      break;
    }
  }
  ASSERT(!tmp.is(no_reg));

  for (int i = 0; i < field_count; i++) {
    ldr(tmp, FieldMemOperand(src, i * kPointerSize));
    str(tmp, FieldMemOperand(dst, i * kPointerSize));
  }
}


void MacroAssembler::CopyBytes(Register src,
                               Register dst,
                               Register length,
                               Register scratch) {
  Label align_loop, align_loop_1, word_loop, byte_loop, byte_loop_1, done;

  // Align src before copying in word size chunks.
  bind(&align_loop);
  cmp(length, Operand(0));
  b(eq, &done);
  bind(&align_loop_1);
  tst(src, Operand(kPointerSize - 1));
  b(eq, &word_loop);
  ldrb(scratch, MemOperand(src, 1, PostIndex));
  strb(scratch, MemOperand(dst, 1, PostIndex));
  sub(length, length, Operand(1), SetCC);
  b(ne, &byte_loop_1);

  // Copy bytes in word size chunks.
  bind(&word_loop);
  if (emit_debug_code()) {
    tst(src, Operand(kPointerSize - 1));
    Assert(eq, "Expecting alignment for CopyBytes");
  }
  cmp(length, Operand(kPointerSize));
  b(lt, &byte_loop);
  ldr(scratch, MemOperand(src, kPointerSize, PostIndex));
#if CAN_USE_UNALIGNED_ACCESSES
  str(scratch, MemOperand(dst, kPointerSize, PostIndex));
#else
  strb(scratch, MemOperand(dst, 1, PostIndex));
  mov(scratch, Operand(scratch, LSR, 8));
  strb(scratch, MemOperand(dst, 1, PostIndex));
  mov(scratch, Operand(scratch, LSR, 8));
  strb(scratch, MemOperand(dst, 1, PostIndex));
  mov(scratch, Operand(scratch, LSR, 8));
  strb(scratch, MemOperand(dst, 1, PostIndex));
#endif
  sub(length, length, Operand(kPointerSize));
  b(&word_loop);

  // Copy the last bytes if any left.
  bind(&byte_loop);
  cmp(length, Operand(0));
  b(eq, &done);
  bind(&byte_loop_1);
  ldrb(scratch, MemOperand(src, 1, PostIndex));
  strb(scratch, MemOperand(dst, 1, PostIndex));
  sub(length, length, Operand(1), SetCC);
  b(ne, &byte_loop_1);
  bind(&done);
}


void MacroAssembler::CountLeadingZeros(Register zeros,   // Answer.
                                       Register source,  // Input.
                                       Register scratch) {
  ASSERT(!zeros.is(source) || !source.is(scratch));
  ASSERT(!zeros.is(scratch));
  ASSERT(!scratch.is(ip));
  ASSERT(!source.is(ip));
  ASSERT(!zeros.is(ip));
#ifdef CAN_USE_ARMV5_INSTRUCTIONS
  clz(zeros, source);  // This instruction is only supported after ARM5.
#else
  mov(zeros, Operand(0, RelocInfo::NONE));
  Move(scratch, source);
  // Top 16.
  tst(scratch, Operand(0xffff0000));
  add(zeros, zeros, Operand(16), LeaveCC, eq);
  mov(scratch, Operand(scratch, LSL, 16), LeaveCC, eq);
  // Top 8.
  tst(scratch, Operand(0xff000000));
  add(zeros, zeros, Operand(8), LeaveCC, eq);
  mov(scratch, Operand(scratch, LSL, 8), LeaveCC, eq);
  // Top 4.
  tst(scratch, Operand(0xf0000000));
  add(zeros, zeros, Operand(4), LeaveCC, eq);
  mov(scratch, Operand(scratch, LSL, 4), LeaveCC, eq);
  // Top 2.
  tst(scratch, Operand(0xc0000000));
  add(zeros, zeros, Operand(2), LeaveCC, eq);
  mov(scratch, Operand(scratch, LSL, 2), LeaveCC, eq);
  // Top bit.
  tst(scratch, Operand(0x80000000u));
  add(zeros, zeros, Operand(1), LeaveCC, eq);
#endif
}


void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialAscii(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure) {
  int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  int kFlatAsciiStringTag = ASCII_STRING_TYPE;
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
  int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  int kFlatAsciiStringTag = ASCII_STRING_TYPE;
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
    if (num_double_arguments > DoubleRegister::kNumRegisters) {
      stack_passed_words +=
          2 * (num_double_arguments - DoubleRegister::kNumRegisters);
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


void MacroAssembler::SetCallCDoubleArguments(DoubleRegister dreg) {
  if (use_eabi_hardfloat()) {
    Move(d0, dreg);
  } else {
    vmov(r0, r1, dreg);
  }
}


void MacroAssembler::SetCallCDoubleArguments(DoubleRegister dreg1,
                                             DoubleRegister dreg2) {
  if (use_eabi_hardfloat()) {
    if (dreg2.is(d0)) {
      ASSERT(!dreg1.is(d1));
      Move(d1, dreg2);
      Move(d0, dreg1);
    } else {
      Move(d0, dreg1);
      Move(d1, dreg2);
    }
  } else {
    vmov(r0, r1, dreg1);
    vmov(r2, r3, dreg2);
  }
}


void MacroAssembler::SetCallCDoubleArguments(DoubleRegister dreg,
                                             Register reg) {
  if (use_eabi_hardfloat()) {
    Move(d0, dreg);
    Move(r0, reg);
  } else {
    Move(r2, reg);
    vmov(r0, r1, dreg);
  }
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  CallCFunctionHelper(no_reg,
                      function,
                      ip,
                      num_reg_arguments,
                      num_double_arguments);
}


void MacroAssembler::CallCFunction(Register function,
                                     Register scratch,
                                     int num_reg_arguments,
                                     int num_double_arguments) {
  CallCFunctionHelper(function,
                      ExternalReference::the_hole_value_location(isolate()),
                      scratch,
                      num_reg_arguments,
                      num_double_arguments);
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}


void MacroAssembler::CallCFunction(Register function,
                                   Register scratch,
                                   int num_arguments) {
  CallCFunction(function, scratch, num_arguments, 0);
}


void MacroAssembler::CallCFunctionHelper(Register function,
                                         ExternalReference function_reference,
                                         Register scratch,
                                         int num_reg_arguments,
                                         int num_double_arguments) {
  // Make sure that the stack is aligned before calling a C function unless
  // running in the simulator. The simulator has its own alignment check which
  // provides more information.
#if defined(V8_HOST_ARCH_ARM)
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
  if (function.is(no_reg)) {
    mov(scratch, Operand(function_reference));
    function = scratch;
  }
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
  const int32_t kPCRegOffset = 2 * kPointerSize;
  ldr(result, MemOperand(ldr_location));
  if (emit_debug_code()) {
    // Check that the instruction is a ldr reg, [pc + offset] .
    and_(result, result, Operand(kLdrPCPattern));
    cmp(result, Operand(kLdrPCPattern));
    Check(eq, "The instruction to patch should be a load from pc.");
    // Result was clobbered. Restore it.
    ldr(result, MemOperand(ldr_location));
  }
  // Get the address of the constant.
  and_(result, result, Operand(kLdrOffsetMask));
  add(result, ldr_location, Operand(result));
  add(result, result, Operand(kPCRegOffset));
}


void MacroAssembler::ClampUint8(Register output_reg, Register input_reg) {
  Usat(output_reg, 8, Operand(input_reg));
}


void MacroAssembler::ClampDoubleToUint8(Register result_reg,
                                        DoubleRegister input_reg,
                                        DoubleRegister temp_double_reg) {
  Label above_zero;
  Label done;
  Label in_bounds;

  Vmov(temp_double_reg, 0.0);
  VFPCompareAndSetFlags(input_reg, temp_double_reg);
  b(gt, &above_zero);

  // Double value is less than zero, NaN or Inf, return 0.
  mov(result_reg, Operand(0));
  b(al, &done);

  // Double value is >= 255, return 255.
  bind(&above_zero);
  Vmov(temp_double_reg, 255.0);
  VFPCompareAndSetFlags(input_reg, temp_double_reg);
  b(le, &in_bounds);
  mov(result_reg, Operand(255));
  b(al, &done);

  // In 0-255 range, round and truncate.
  bind(&in_bounds);
  Vmov(temp_double_reg, 0.5);
  vadd(temp_double_reg, input_reg, temp_double_reg);
  vcvt_u32_f64(s0, temp_double_reg);
  vmov(result_reg, s0);
  bind(&done);
}


void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  ldr(descriptors,
      FieldMemOperand(map, Map::kInstanceDescriptorsOrBitField3Offset));
  Label not_smi;
  JumpIfNotSmi(descriptors, &not_smi);
  mov(descriptors, Operand(FACTORY->empty_descriptor_array()));
  bind(&not_smi);
}


CodePatcher::CodePatcher(byte* address, int instructions)
    : address_(address),
      instructions_(instructions),
      size_(instructions * Assembler::kInstrSize),
      masm_(Isolate::Current(), address, size_ + Assembler::kGap) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  ASSERT(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  CPU::FlushICache(address_, size_);

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


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM

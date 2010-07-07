// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#include "v8.h"

#if defined(V8_TARGET_ARCH_ARM)

#include "bootstrapper.h"
#include "codegen-inl.h"
#include "debug.h"
#include "runtime.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(void* buffer, int size)
    : Assembler(buffer, size),
      generating_stub_(false),
      allow_stub_calls_(true),
      code_object_(Heap::undefined_value()) {
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
  mov(ip, Operand(target, rmode), LeaveCC, cond);
  bx(ip, cond);
#else
  mov(pc, Operand(target, rmode), LeaveCC, cond);
#endif
}


void MacroAssembler::Jump(byte* target, RelocInfo::Mode rmode,
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


void MacroAssembler::Call(Register target, Condition cond) {
#if USE_BLX
  blx(target, cond);
#else
  // set lr for return at current pc + 8
  mov(lr, Operand(pc), LeaveCC, cond);
  mov(pc, Operand(target), LeaveCC, cond);
#endif
}


void MacroAssembler::Call(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond) {
#if USE_BLX
  // On ARMv5 and after the recommended call sequence is:
  //  ldr ip, [pc, #...]
  //  blx ip

  // The two instructions (ldr and blx) could be separated by a constant
  // pool and the code would still work. The issue comes from the
  // patching code which expect the ldr to be just above the blx.
  { BlockConstPoolScope block_const_pool(this);
    // Statement positions are expected to be recorded when the target
    // address is loaded. The mov method will automatically record
    // positions when pc is the target, since this is not the case here
    // we have to do it explicitly.
    WriteRecordedPositions();

    mov(ip, Operand(target, rmode), LeaveCC, cond);
    blx(ip, cond);
  }

  ASSERT(kCallTargetAddressOffset == 2 * kInstrSize);
#else
  // Set lr for return at current pc + 8.
  mov(lr, Operand(pc), LeaveCC, cond);
  // Emit a ldr<cond> pc, [pc + offset of target in constant pool].
  mov(pc, Operand(target, rmode), LeaveCC, cond);

  ASSERT(kCallTargetAddressOffset == kInstrSize);
#endif
}


void MacroAssembler::Call(byte* target, RelocInfo::Mode rmode,
                          Condition cond) {
  ASSERT(!RelocInfo::IsCodeTarget(rmode));
  Call(reinterpret_cast<intptr_t>(target), rmode, cond);
}


void MacroAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  // 'code' is always generated ARM code, never THUMB code
  Call(reinterpret_cast<intptr_t>(code.location()), rmode, cond);
}


void MacroAssembler::Ret(Condition cond) {
#if USE_BX
  bx(lr, cond);
#else
  mov(pc, Operand(lr), LeaveCC, cond);
#endif
}


void MacroAssembler::StackLimitCheck(Label* on_stack_overflow) {
  LoadRoot(ip, Heap::kStackLimitRootIndex);
  cmp(sp, Operand(ip));
  b(lo, on_stack_overflow);
}


void MacroAssembler::Drop(int count, Condition cond) {
  if (count > 0) {
    add(sp, sp, Operand(count * kPointerSize), LeaveCC, cond);
  }
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


void MacroAssembler::Move(Register dst, Handle<Object> value) {
  mov(dst, Operand(value));
}


void MacroAssembler::Move(Register dst, Register src) {
  if (!dst.is(src)) {
    mov(dst, src);
  }
}


void MacroAssembler::And(Register dst, Register src1, const Operand& src2,
                         Condition cond) {
  if (!CpuFeatures::IsSupported(ARMv7) || src2.is_single_instruction()) {
    and_(dst, src1, src2, LeaveCC, cond);
    return;
  }
  int32_t immediate = src2.immediate();
  if (immediate == 0) {
    mov(dst, Operand(0), LeaveCC, cond);
    return;
  }
  if (IsPowerOf2(immediate + 1) && ((immediate & 1) != 0)) {
    ubfx(dst, src1, 0, WhichPowerOf2(immediate + 1), cond);
    return;
  }
  and_(dst, src1, src2, LeaveCC, cond);
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


void MacroAssembler::Bfc(Register dst, int lsb, int width, Condition cond) {
  ASSERT(lsb < 32);
  if (!CpuFeatures::IsSupported(ARMv7)) {
    int mask = (1 << (width + lsb)) - 1 - ((1 << lsb) - 1);
    bic(dst, dst, Operand(mask));
  } else {
    bfc(dst, lsb, width, cond);
  }
}


void MacroAssembler::SmiJumpTable(Register index, Vector<Label*> targets) {
  // Empty the const pool.
  CheckConstPool(true, true);
  add(pc, pc, Operand(index,
                      LSL,
                      assembler::arm::Instr::kInstrSizeLog2 - kSmiTagSize));
  BlockConstPoolBefore(pc_offset() + (targets.length() + 1) * kInstrSize);
  nop();  // Jump table alignment.
  for (int i = 0; i < targets.length(); i++) {
    b(targets[i]);
  }
}


void MacroAssembler::LoadRoot(Register destination,
                              Heap::RootListIndex index,
                              Condition cond) {
  ldr(destination, MemOperand(roots, index << kPointerSizeLog2), cond);
}


void MacroAssembler::StoreRoot(Register source,
                               Heap::RootListIndex index,
                               Condition cond) {
  str(source, MemOperand(roots, index << kPointerSizeLog2), cond);
}


void MacroAssembler::RecordWriteHelper(Register object,
                                       Register address,
                                       Register scratch) {
  if (FLAG_debug_code) {
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
                                Condition cc,
                                Label* branch) {
  ASSERT(cc == eq || cc == ne);
  and_(scratch, object, Operand(ExternalReference::new_space_mask()));
  cmp(scratch, Operand(ExternalReference::new_space_start()));
  b(cc, branch);
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
  if (FLAG_debug_code) {
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
  if (FLAG_debug_code) {
    mov(object, Operand(BitCast<int32_t>(kZapValue)));
    mov(address, Operand(BitCast<int32_t>(kZapValue)));
    mov(scratch, Operand(BitCast<int32_t>(kZapValue)));
  }
}


void MacroAssembler::Ldrd(Register dst1, Register dst2,
                          const MemOperand& src, Condition cond) {
  ASSERT(src.rm().is(no_reg));
  ASSERT(!dst1.is(lr));  // r14.
  ASSERT_EQ(0, dst1.code() % 2);
  ASSERT_EQ(dst1.code() + 1, dst2.code());

  // Generate two ldr instructions if ldrd is not available.
  if (CpuFeatures::IsSupported(ARMv7)) {
    CpuFeatures::Scope scope(ARMv7);
    ldrd(dst1, dst2, src, cond);
  } else {
    MemOperand src2(src);
    src2.set_offset(src2.offset() + 4);
    if (dst1.is(src.rn())) {
      ldr(dst2, src2, cond);
      ldr(dst1, src, cond);
    } else {
      ldr(dst1, src, cond);
      ldr(dst2, src2, cond);
    }
  }
}


void MacroAssembler::Strd(Register src1, Register src2,
                          const MemOperand& dst, Condition cond) {
  ASSERT(dst.rm().is(no_reg));
  ASSERT(!src1.is(lr));  // r14.
  ASSERT_EQ(0, src1.code() % 2);
  ASSERT_EQ(src1.code() + 1, src2.code());

  // Generate two str instructions if strd is not available.
  if (CpuFeatures::IsSupported(ARMv7)) {
    CpuFeatures::Scope scope(ARMv7);
    strd(src1, src2, dst, cond);
  } else {
    MemOperand dst2(dst);
    dst2.set_offset(dst2.offset() + 4);
    str(src1, dst, cond);
    str(src2, dst2, cond);
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


void MacroAssembler::EnterExitFrame(ExitFrame::Mode mode) {
  // Compute the argv pointer and keep it in a callee-saved register.
  // r0 is argc.
  add(r6, sp, Operand(r0, LSL, kPointerSizeLog2));
  sub(r6, r6, Operand(kPointerSize));

  // Compute callee's stack pointer before making changes and save it as
  // ip register so that it is restored as sp register on exit, thereby
  // popping the args.

  // ip = sp + kPointerSize * #args;
  add(ip, sp, Operand(r0, LSL, kPointerSizeLog2));

  // Prepare the stack to be aligned when calling into C. After this point there
  // are 5 pushes before the call into C, so the stack needs to be aligned after
  // 5 pushes.
  int frame_alignment = ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (frame_alignment != kPointerSize) {
    // The following code needs to be more general if this assert does not hold.
    ASSERT(frame_alignment == 2 * kPointerSize);
    // With 5 pushes left the frame must be unaligned at this point.
    mov(r7, Operand(Smi::FromInt(0)));
    tst(sp, Operand((frame_alignment - kPointerSize) & frame_alignment_mask));
    push(r7, eq);  // Push if aligned to make it unaligned.
  }

  // Push in reverse order: caller_fp, sp_on_exit, and caller_pc.
  stm(db_w, sp, fp.bit() | ip.bit() | lr.bit());
  mov(fp, Operand(sp));  // Setup new frame pointer.

  mov(ip, Operand(CodeObject()));
  push(ip);  // Accessed from ExitFrame::code_slot.

  // Save the frame pointer and the context in top.
  mov(ip, Operand(ExternalReference(Top::k_c_entry_fp_address)));
  str(fp, MemOperand(ip));
  mov(ip, Operand(ExternalReference(Top::k_context_address)));
  str(cp, MemOperand(ip));

  // Setup argc and the builtin function in callee-saved registers.
  mov(r4, Operand(r0));
  mov(r5, Operand(r1));


#ifdef ENABLE_DEBUGGER_SUPPORT
  // Save the state of all registers to the stack from the memory
  // location. This is needed to allow nested break points.
  if (mode == ExitFrame::MODE_DEBUG) {
    // Use sp as base to push.
    CopyRegistersFromMemoryToStack(sp, kJSCallerSaved);
  }
#endif
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


void MacroAssembler::LeaveExitFrame(ExitFrame::Mode mode) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  // Restore the memory copy of the registers by digging them out from
  // the stack. This is needed to allow nested break points.
  if (mode == ExitFrame::MODE_DEBUG) {
    // This code intentionally clobbers r2 and r3.
    const int kCallerSavedSize = kNumJSCallerSaved * kPointerSize;
    const int kOffset = ExitFrameConstants::kCodeOffset - kCallerSavedSize;
    add(r3, fp, Operand(kOffset));
    CopyRegistersFromStackToMemory(r3, r2, kJSCallerSaved);
  }
#endif

  // Clear top frame.
  mov(r3, Operand(0));
  mov(ip, Operand(ExternalReference(Top::k_c_entry_fp_address)));
  str(r3, MemOperand(ip));

  // Restore current context from top and clear it in debug mode.
  mov(ip, Operand(ExternalReference(Top::k_context_address)));
  ldr(cp, MemOperand(ip));
#ifdef DEBUG
  str(r3, MemOperand(ip));
#endif

  // Pop the arguments, restore registers, and return.
  mov(sp, Operand(fp));  // respect ABI stack constraint
  ldm(ia, sp, fp.bit() | sp.bit() | pc.bit());
}


void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    Register code_reg,
                                    Label* done,
                                    InvokeFlag flag) {
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
        Handle<Code>(Builtins::builtin(Builtins::ArgumentsAdaptorTrampoline));
    if (flag == CALL_FUNCTION) {
      Call(adaptor, RelocInfo::CODE_TARGET);
      b(done);
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&regular_invoke);
  }
}


void MacroAssembler::InvokeCode(Register code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                InvokeFlag flag) {
  Label done;

  InvokePrologue(expected, actual, Handle<Code>::null(), code, &done, flag);
  if (flag == CALL_FUNCTION) {
    Call(code);
  } else {
    ASSERT(flag == JUMP_FUNCTION);
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
                                InvokeFlag flag) {
  Label done;

  InvokePrologue(expected, actual, code, no_reg, &done, flag);
  if (flag == CALL_FUNCTION) {
    Call(code, rmode);
  } else {
    Jump(code, rmode);
  }

  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  bind(&done);
}


void MacroAssembler::InvokeFunction(Register fun,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
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
      MemOperand(code_reg, SharedFunctionInfo::kCodeOffset - kHeapObjectTag));
  add(code_reg, code_reg, Operand(Code::kHeaderSize - kHeapObjectTag));

  ParameterCount expected(expected_reg);
  InvokeCode(code_reg, expected, actual, flag);
}


void MacroAssembler::InvokeFunction(JSFunction* function,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  ASSERT(function->is_compiled());

  // Get the function and setup the context.
  mov(r1, Operand(Handle<JSFunction>(function)));
  ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // Invoke the cached code.
  Handle<Code> code(function->code());
  ParameterCount expected(function->shared()->formal_parameter_count());
  InvokeCode(code, expected, actual, RelocInfo::CODE_TARGET, flag);
}

#ifdef ENABLE_DEBUGGER_SUPPORT
void MacroAssembler::SaveRegistersToMemory(RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Copy the content of registers to memory location.
  for (int i = 0; i < kNumJSCallerSaved; i++) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      Register reg = { r };
      mov(ip, Operand(ExternalReference(Debug_Address::Register(i))));
      str(reg, MemOperand(ip));
    }
  }
}


void MacroAssembler::RestoreRegistersFromMemory(RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Copy the content of memory location to registers.
  for (int i = kNumJSCallerSaved; --i >= 0;) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      Register reg = { r };
      mov(ip, Operand(ExternalReference(Debug_Address::Register(i))));
      ldr(reg, MemOperand(ip));
    }
  }
}


void MacroAssembler::CopyRegistersFromMemoryToStack(Register base,
                                                    RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Copy the content of the memory location to the stack and adjust base.
  for (int i = kNumJSCallerSaved; --i >= 0;) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      mov(ip, Operand(ExternalReference(Debug_Address::Register(i))));
      ldr(ip, MemOperand(ip));
      str(ip, MemOperand(base, 4, NegPreIndex));
    }
  }
}


void MacroAssembler::CopyRegistersFromStackToMemory(Register base,
                                                    Register scratch,
                                                    RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Copy the content of the stack to the memory location and adjust base.
  for (int i = 0; i < kNumJSCallerSaved; i++) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      mov(ip, Operand(ExternalReference(Debug_Address::Register(i))));
      ldr(scratch, MemOperand(base, 4, PostIndex));
      str(scratch, MemOperand(ip));
    }
  }
}


void MacroAssembler::DebugBreak() {
  ASSERT(allow_stub_calls());
  mov(r0, Operand(0));
  mov(r1, Operand(ExternalReference(Runtime::kDebugBreak)));
  CEntryStub ces(1);
  Call(ces.GetCode(), RelocInfo::DEBUG_BREAK);
}
#endif


void MacroAssembler::PushTryHandler(CodeLocation try_location,
                                    HandlerType type) {
  // Adjust this code if not the case.
  ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize);
  // The pc (return address) is passed in register lr.
  if (try_location == IN_JAVASCRIPT) {
    if (type == TRY_CATCH_HANDLER) {
      mov(r3, Operand(StackHandler::TRY_CATCH));
    } else {
      mov(r3, Operand(StackHandler::TRY_FINALLY));
    }
    ASSERT(StackHandlerConstants::kStateOffset == 1 * kPointerSize
           && StackHandlerConstants::kFPOffset == 2 * kPointerSize
           && StackHandlerConstants::kPCOffset == 3 * kPointerSize);
    stm(db_w, sp, r3.bit() | fp.bit() | lr.bit());
    // Save the current handler as the next handler.
    mov(r3, Operand(ExternalReference(Top::k_handler_address)));
    ldr(r1, MemOperand(r3));
    ASSERT(StackHandlerConstants::kNextOffset == 0);
    push(r1);
    // Link this handler as the new current one.
    str(sp, MemOperand(r3));
  } else {
    // Must preserve r0-r4, r5-r7 are available.
    ASSERT(try_location == IN_JS_ENTRY);
    // The frame pointer does not point to a JS frame so we save NULL
    // for fp. We expect the code throwing an exception to check fp
    // before dereferencing it to restore the context.
    mov(ip, Operand(0));  // To save a NULL frame pointer.
    mov(r6, Operand(StackHandler::ENTRY));
    ASSERT(StackHandlerConstants::kStateOffset == 1 * kPointerSize
           && StackHandlerConstants::kFPOffset == 2 * kPointerSize
           && StackHandlerConstants::kPCOffset == 3 * kPointerSize);
    stm(db_w, sp, r6.bit() | ip.bit() | lr.bit());
    // Save the current handler as the next handler.
    mov(r7, Operand(ExternalReference(Top::k_handler_address)));
    ldr(r6, MemOperand(r7));
    ASSERT(StackHandlerConstants::kNextOffset == 0);
    push(r6);
    // Link this handler as the new current one.
    str(sp, MemOperand(r7));
  }
}


void MacroAssembler::PopTryHandler() {
  ASSERT_EQ(0, StackHandlerConstants::kNextOffset);
  pop(r1);
  mov(ip, Operand(ExternalReference(Top::k_handler_address)));
  add(sp, sp, Operand(StackHandlerConstants::kSize - kPointerSize));
  str(r1, MemOperand(ip));
}


Register MacroAssembler::CheckMaps(JSObject* object, Register object_reg,
                                   JSObject* holder, Register holder_reg,
                                   Register scratch,
                                   int save_at_depth,
                                   Label* miss) {
  // Make sure there's no overlap between scratch and the other
  // registers.
  ASSERT(!scratch.is(object_reg) && !scratch.is(holder_reg));

  // Keep track of the current object in register reg.
  Register reg = object_reg;
  int depth = 0;

  if (save_at_depth == depth) {
    str(reg, MemOperand(sp));
  }

  // Check the maps in the prototype chain.
  // Traverse the prototype chain from the object and do map checks.
  while (object != holder) {
    depth++;

    // Only global objects and objects that do not require access
    // checks are allowed in stubs.
    ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

    // Get the map of the current object.
    ldr(scratch, FieldMemOperand(reg, HeapObject::kMapOffset));
    cmp(scratch, Operand(Handle<Map>(object->map())));

    // Branch on the result of the map check.
    b(ne, miss);

    // Check access rights to the global object.  This has to happen
    // after the map check so that we know that the object is
    // actually a global object.
    if (object->IsJSGlobalProxy()) {
      CheckAccessGlobalProxy(reg, scratch, miss);
      // Restore scratch register to be the map of the object.  In the
      // new space case below, we load the prototype from the map in
      // the scratch register.
      ldr(scratch, FieldMemOperand(reg, HeapObject::kMapOffset));
    }

    reg = holder_reg;  // from now the object is in holder_reg
    JSObject* prototype = JSObject::cast(object->GetPrototype());
    if (Heap::InNewSpace(prototype)) {
      // The prototype is in new space; we cannot store a reference
      // to it in the code. Load it from the map.
      ldr(reg, FieldMemOperand(scratch, Map::kPrototypeOffset));
    } else {
      // The prototype is in old space; load it directly.
      mov(reg, Operand(Handle<JSObject>(prototype)));
    }

    if (save_at_depth == depth) {
      str(reg, MemOperand(sp));
    }

    // Go to the next object in the prototype chain.
    object = prototype;
  }

  // Check the holder map.
  ldr(scratch, FieldMemOperand(reg, HeapObject::kMapOffset));
  cmp(scratch, Operand(Handle<Map>(object->map())));
  b(ne, miss);

  // Log the check depth.
  LOG(IntEvent("check-maps-depth", depth + 1));

  // Perform security check for access to the global object and return
  // the holder register.
  ASSERT(object == holder);
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());
  if (object->IsJSGlobalProxy()) {
    CheckAccessGlobalProxy(reg, scratch, miss);
  }
  return reg;
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
  cmp(scratch, Operand(0));
  Check(ne, "we should not have an empty lexical context");
#endif

  // Load the global context of the current context.
  int offset = Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
  ldr(scratch, FieldMemOperand(scratch, offset));
  ldr(scratch, FieldMemOperand(scratch, GlobalObject::kGlobalContextOffset));

  // Check the context is a global context.
  if (FLAG_debug_code) {
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
  if (FLAG_debug_code) {
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


void MacroAssembler::AllocateInNewSpace(int object_size,
                                        Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  ASSERT(!result.is(scratch1));
  ASSERT(!scratch1.is(scratch2));

  // Make object size into bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    object_size *= kPointerSize;
  }
  ASSERT_EQ(0, object_size & kObjectAlignmentMask);

  // Load address of new object into result and allocation top address into
  // scratch1.
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();
  mov(scratch1, Operand(new_space_allocation_top));
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    ldr(result, MemOperand(scratch1));
  } else if (FLAG_debug_code) {
    // Assert that result actually contains top on entry. scratch2 is used
    // immediately below so this use of scratch2 does not cause difference with
    // respect to register content between debug and release mode.
    ldr(scratch2, MemOperand(scratch1));
    cmp(result, scratch2);
    Check(eq, "Unexpected allocation top");
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top.
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address();
  mov(scratch2, Operand(new_space_allocation_limit));
  ldr(scratch2, MemOperand(scratch2));
  add(result, result, Operand(object_size));
  cmp(result, Operand(scratch2));
  b(hi, gc_required);
  str(result, MemOperand(scratch1));

  // Tag and adjust back to start of new object.
  if ((flags & TAG_OBJECT) != 0) {
    sub(result, result, Operand(object_size - kHeapObjectTag));
  } else {
    sub(result, result, Operand(object_size));
  }
}


void MacroAssembler::AllocateInNewSpace(Register object_size,
                                        Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  ASSERT(!result.is(scratch1));
  ASSERT(!scratch1.is(scratch2));

  // Load address of new object into result and allocation top address into
  // scratch1.
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();
  mov(scratch1, Operand(new_space_allocation_top));
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    ldr(result, MemOperand(scratch1));
  } else if (FLAG_debug_code) {
    // Assert that result actually contains top on entry. scratch2 is used
    // immediately below so this use of scratch2 does not cause difference with
    // respect to register content between debug and release mode.
    ldr(scratch2, MemOperand(scratch1));
    cmp(result, scratch2);
    Check(eq, "Unexpected allocation top");
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top. Object size is in words so a shift is required to
  // get the number of bytes
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address();
  mov(scratch2, Operand(new_space_allocation_limit));
  ldr(scratch2, MemOperand(scratch2));
  if ((flags & SIZE_IN_WORDS) != 0) {
    add(result, result, Operand(object_size, LSL, kPointerSizeLog2));
  } else {
    add(result, result, Operand(object_size));
  }
  cmp(result, Operand(scratch2));
  b(hi, gc_required);

  // Update allocation top. result temporarily holds the new top.
  if (FLAG_debug_code) {
    tst(result, Operand(kObjectAlignmentMask));
    Check(eq, "Unaligned allocation in new space");
  }
  str(result, MemOperand(scratch1));

  // Adjust back to start of new object.
  if ((flags & SIZE_IN_WORDS) != 0) {
    sub(result, result, Operand(object_size, LSL, kPointerSizeLog2));
  } else {
    sub(result, result, Operand(object_size));
  }

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    add(result, result, Operand(kHeapObjectTag));
  }
}


void MacroAssembler::UndoAllocationInNewSpace(Register object,
                                              Register scratch) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();

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


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Handle<Map> map,
                              Label* fail,
                              bool is_heap_object) {
  if (!is_heap_object) {
    BranchOnSmi(obj, fail);
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
                              bool is_heap_object) {
  if (!is_heap_object) {
    BranchOnSmi(obj, fail);
  }
  ldr(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  LoadRoot(ip, index);
  cmp(scratch, ip);
  b(ne, fail);
}


void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Register scratch,
                                             Label* miss) {
  // Check that the receiver isn't a smi.
  BranchOnSmi(function, miss);

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
  ASSERT(allow_stub_calls());  // stub calls are not allowed in some stubs
  Call(stub->GetCode(), RelocInfo::CODE_TARGET, cond);
}


void MacroAssembler::TailCallStub(CodeStub* stub, Condition cond) {
  ASSERT(allow_stub_calls());  // stub calls are not allowed in some stubs
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET, cond);
}


void MacroAssembler::StubReturn(int argc, Condition cond) {
  ASSERT(argc >= 1 && generating_stub());
  if (argc > 1) {
    add(sp, sp, Operand((argc - 1) * kPointerSize), LeaveCC, cond);
  }
  Ret(cond);
}


void MacroAssembler::IllegalOperation(int num_arguments) {
  if (num_arguments > 0) {
    add(sp, sp, Operand(num_arguments * kPointerSize));
  }
  LoadRoot(r0, Heap::kUndefinedValueRootIndex);
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
    BranchOnNotSmi(object, &not_smi);
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


void MacroAssembler::CallRuntime(Runtime::Function* f, int num_arguments) {
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
  mov(r1, Operand(ExternalReference(f)));
  CEntryStub stub(1);
  CallStub(&stub);
}


void MacroAssembler::CallRuntime(Runtime::FunctionId fid, int num_arguments) {
  CallRuntime(Runtime::FunctionForId(fid), num_arguments);
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
  TailCallExternalReference(ExternalReference(fid), num_arguments, result_size);
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


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeJSFlags flags) {
  GetBuiltinEntry(r2, id);
  if (flags == CALL_JS) {
    Call(r2);
  } else {
    ASSERT(flags == JUMP_JS);
    Jump(r2);
  }
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  ASSERT(!target.is(r1));

  // Load the builtins object into target register.
  ldr(target, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  ldr(target, FieldMemOperand(target, GlobalObject::kBuiltinsOffset));

  // Load the JavaScript builtin function from the builtins object.
  ldr(r1, FieldMemOperand(target,
                          JSBuiltinsObject::OffsetOfFunctionWithId(id)));

  // Load the code entry point from the builtins object.
  ldr(target, FieldMemOperand(target,
                              JSBuiltinsObject::OffsetOfCodeWithId(id)));
  if (FLAG_debug_code) {
    // Make sure the code objects in the builtins object and in the
    // builtin function are the same.
    push(r1);
    ldr(r1, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    ldr(r1, FieldMemOperand(r1, SharedFunctionInfo::kCodeOffset));
    cmp(r1, target);
    Assert(eq, "Builtin code object changed");
    pop(r1);
  }
  add(target, target, Operand(Code::kHeaderSize - kHeapObjectTag));
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


void MacroAssembler::Assert(Condition cc, const char* msg) {
  if (FLAG_debug_code)
    Check(cc, msg);
}


void MacroAssembler::AssertRegisterIsRoot(Register reg,
                                          Heap::RootListIndex index) {
  if (FLAG_debug_code) {
    LoadRoot(ip, index);
    cmp(reg, ip);
    Check(eq, "Register did not match expected root");
  }
}


void MacroAssembler::Check(Condition cc, const char* msg) {
  Label L;
  b(cc, &L);
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
  set_allow_stub_calls(true);

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
    ldr(dst, MemOperand(cp, Context::SlotOffset(Context::CLOSURE_INDEX)));
    // Load the function context (which is the incoming, outer context).
    ldr(dst, FieldMemOperand(dst, JSFunction::kContextOffset));
    for (int i = 1; i < context_chain_length; i++) {
      ldr(dst, MemOperand(dst, Context::SlotOffset(Context::CLOSURE_INDEX)));
      ldr(dst, FieldMemOperand(dst, JSFunction::kContextOffset));
    }
    // The context may be an intermediate context, not a function context.
    ldr(dst, MemOperand(dst, Context::SlotOffset(Context::FCONTEXT_INDEX)));
  } else {  // Slot is in the current function context.
    // The context may be an intermediate context, not a function context.
    ldr(dst, MemOperand(cp, Context::SlotOffset(Context::FCONTEXT_INDEX)));
  }
}


void MacroAssembler::JumpIfNotBothSmi(Register reg1,
                                      Register reg2,
                                      Label* on_not_both_smi) {
  ASSERT_EQ(0, kSmiTag);
  tst(reg1, Operand(kSmiTagMask));
  tst(reg2, Operand(kSmiTagMask), eq);
  b(ne, on_not_both_smi);
}


void MacroAssembler::JumpIfEitherSmi(Register reg1,
                                     Register reg2,
                                     Label* on_either_smi) {
  ASSERT_EQ(0, kSmiTag);
  tst(reg1, Operand(kSmiTagMask));
  tst(reg2, Operand(kSmiTagMask), ne);
  b(eq, on_either_smi);
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
  ASSERT_EQ(0, kSmiTag);
  and_(scratch1, first, Operand(second));
  tst(scratch1, Operand(kSmiTagMask));
  b(eq, failure);
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


void MacroAssembler::CountLeadingZeros(Register zeros,   // Answer.
                                       Register source,  // Input.
                                       Register scratch) {
  ASSERT(!zeros.is(source) || !source.is(zeros));
  ASSERT(!zeros.is(scratch));
  ASSERT(!scratch.is(ip));
  ASSERT(!source.is(ip));
  ASSERT(!zeros.is(ip));
#ifdef CAN_USE_ARMV5_INSTRUCTIONS
  clz(zeros, source);  // This instruction is only supported after ARM5.
#else
  mov(zeros, Operand(0));
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


void MacroAssembler::PrepareCallCFunction(int num_arguments, Register scratch) {
  int frame_alignment = ActivationFrameAlignment();
  // Up to four simple arguments are passed in registers r0..r3.
  int stack_passed_arguments = (num_arguments <= 4) ? 0 : num_arguments - 4;
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


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  mov(ip, Operand(function));
  CallCFunction(ip, num_arguments);
}


void MacroAssembler::CallCFunction(Register function, int num_arguments) {
  // Make sure that the stack is aligned before calling a C function unless
  // running in the simulator. The simulator has its own alignment check which
  // provides more information.
#if defined(V8_HOST_ARCH_ARM)
  if (FLAG_debug_code) {
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
  int stack_passed_arguments = (num_arguments <= 4) ? 0 : num_arguments - 4;
  if (OS::ActivationFrameAlignment() > kPointerSize) {
    ldr(sp, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    add(sp, sp, Operand(stack_passed_arguments * sizeof(kPointerSize)));
  }
}


#ifdef ENABLE_DEBUGGER_SUPPORT
CodePatcher::CodePatcher(byte* address, int instructions)
    : address_(address),
      instructions_(instructions),
      size_(instructions * Assembler::kInstrSize),
      masm_(address, size_ + Assembler::kGap) {
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


void CodePatcher::Emit(Instr x) {
  masm()->emit(x);
}


void CodePatcher::Emit(Address addr) {
  masm()->emit(reinterpret_cast<Instr>(addr));
}
#endif  // ENABLE_DEBUGGER_SUPPORT


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM

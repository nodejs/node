// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "bootstrapper.h"
#include "codegen-inl.h"
#include "debug.h"
#include "runtime.h"

namespace v8 { namespace internal {

// Give alias names to registers
Register cp = {  8 };  // JavaScript context pointer
Register pp = { 10 };  // parameter pointer


MacroAssembler::MacroAssembler(void* buffer, int size)
    : Assembler(buffer, size),
      unresolved_(0),
      generating_stub_(false),
      allow_stub_calls_(true),
      code_object_(Heap::undefined_value()) {
}


// We always generate arm code, never thumb code, even if V8 is compiled to
// thumb, so we require inter-working support
#if defined(__thumb__) && !defined(__THUMB_INTERWORK__)
#error "flag -mthumb-interwork missing"
#endif


// We do not support thumb inter-working with an arm architecture not supporting
// the blx instruction (below v5t)
#if defined(__THUMB_INTERWORK__)
#if !defined(__ARM_ARCH_5T__) && !defined(__ARM_ARCH_5TE__)
// add tests for other versions above v5t as required
#error "for thumb inter-working we require architecture v5t or above"
#endif
#endif


// Using blx may yield better code, so use it when required or when available
#if defined(__THUMB_INTERWORK__) || defined(__ARM_ARCH_5__)
#define USE_BLX 1
#endif

// Using bx does not yield better code, so use it only when required
#if defined(__THUMB_INTERWORK__)
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
#if !defined(__arm__)
  if (rmode == RelocInfo::RUNTIME_ENTRY) {
    mov(r2, Operand(target, rmode), LeaveCC, cond);
    // Set lr for return at current pc + 8.
    mov(lr, Operand(pc), LeaveCC, cond);
    // Emit a ldr<cond> pc, [pc + offset of target in constant pool].
    // Notify the simulator of the transition to C code.
    swi(assembler::arm::call_rt_r2);
  } else {
    // set lr for return at current pc + 8
    mov(lr, Operand(pc), LeaveCC, cond);
    // emit a ldr<cond> pc, [pc + offset of target in constant pool]
    mov(pc, Operand(target, rmode), LeaveCC, cond);
  }
#else
  // Set lr for return at current pc + 8.
  mov(lr, Operand(pc), LeaveCC, cond);
  // Emit a ldr<cond> pc, [pc + offset of target in constant pool].
  mov(pc, Operand(target, rmode), LeaveCC, cond);
#endif  // !defined(__arm__)
  // If USE_BLX is defined, we could emit a 'mov ip, target', followed by a
  // 'blx ip'; however, the code would not be shorter than the above sequence
  // and the target address of the call would be referenced by the first
  // instruction rather than the second one, which would make it harder to patch
  // (two instructions before the return address, instead of one).
  ASSERT(kTargetAddrToReturnAddrDist == sizeof(Instr));
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


void MacroAssembler::SmiJumpTable(Register index, Vector<Label*> targets) {
  // Empty the const pool.
  CheckConstPool(true, true);
  add(pc, pc, Operand(index,
                      LSL,
                      assembler::arm::Instr::kInstrSizeLog2 - kSmiTagSize));
  BlockConstPoolBefore(pc_offset() + (targets.length() + 1) * sizeof(Instr));
  nop();  // Jump table alignment.
  for (int i = 0; i < targets.length(); i++) {
    b(targets[i]);
  }
}


// Will clobber 4 registers: object, offset, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(Register object, Register offset,
                                 Register scratch) {
  // This is how much we shift the remembered set bit offset to get the
  // offset of the word in the remembered set.  We divide by kBitsPerInt (32,
  // shift right 5) and then multiply by kIntSize (4, shift left 2).
  const int kRSetWordShift = 3;

  Label fast, done;

  // First, test that the object is not in the new space.  We cannot set
  // remembered set bits in the new space.
  // object: heap object pointer (with tag)
  // offset: offset to store location from the object
  and_(scratch, object, Operand(Heap::NewSpaceMask()));
  cmp(scratch, Operand(ExternalReference::new_space_start()));
  b(eq, &done);

  // Compute the bit offset in the remembered set.
  // object: heap object pointer (with tag)
  // offset: offset to store location from the object
  mov(ip, Operand(Page::kPageAlignmentMask));  // load mask only once
  and_(scratch, object, Operand(ip));  // offset into page of the object
  add(offset, scratch, Operand(offset));  // add offset into the object
  mov(offset, Operand(offset, LSR, kObjectAlignmentBits));

  // Compute the page address from the heap object pointer.
  // object: heap object pointer (with tag)
  // offset: bit offset of store position in the remembered set
  bic(object, object, Operand(ip));

  // If the bit offset lies beyond the normal remembered set range, it is in
  // the extra remembered set area of a large object.
  // object: page start
  // offset: bit offset of store position in the remembered set
  cmp(offset, Operand(Page::kPageSize / kPointerSize));
  b(lt, &fast);

  // Adjust the bit offset to be relative to the start of the extra
  // remembered set and the start address to be the address of the extra
  // remembered set.
  sub(offset, offset, Operand(Page::kPageSize / kPointerSize));
  // Load the array length into 'scratch' and multiply by four to get the
  // size in bytes of the elements.
  ldr(scratch, MemOperand(object, Page::kObjectStartOffset
                                  + FixedArray::kLengthOffset));
  mov(scratch, Operand(scratch, LSL, kObjectAlignmentBits));
  // Add the page header (including remembered set), array header, and array
  // body size to the page address.
  add(object, object, Operand(Page::kObjectStartOffset
                              + Array::kHeaderSize));
  add(object, object, Operand(scratch));

  bind(&fast);
  // Get address of the rset word.
  // object: start of the remembered set (page start for the fast case)
  // offset: bit offset of store position in the remembered set
  bic(scratch, offset, Operand(kBitsPerInt - 1));  // clear the bit offset
  add(object, object, Operand(scratch, LSR, kRSetWordShift));
  // Get bit offset in the rset word.
  // object: address of remembered set word
  // offset: bit offset of store position
  and_(offset, offset, Operand(kBitsPerInt - 1));

  ldr(scratch, MemOperand(object));
  mov(ip, Operand(1));
  orr(scratch, scratch, Operand(ip, LSL, offset));
  str(scratch, MemOperand(object));

  bind(&done);
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


void MacroAssembler::EnterExitFrame(StackFrame::Type type) {
  ASSERT(type == StackFrame::EXIT || type == StackFrame::EXIT_DEBUG);
  // Compute parameter pointer before making changes and save it as ip
  // register so that it is restored as sp register on exit, thereby
  // popping the args.

  // ip = sp + kPointerSize * #args;
  add(ip, sp, Operand(r0, LSL, kPointerSizeLog2));

  // Push in reverse order: caller_fp, sp_on_exit, and caller_pc.
  stm(db_w, sp, fp.bit() | ip.bit() | lr.bit());
  mov(fp, Operand(sp));  // setup new frame pointer

  // Push debug marker.
  mov(ip, Operand(type == StackFrame::EXIT_DEBUG ? 1 : 0));
  push(ip);

  // Save the frame pointer and the context in top.
  mov(ip, Operand(ExternalReference(Top::k_c_entry_fp_address)));
  str(fp, MemOperand(ip));
  mov(ip, Operand(ExternalReference(Top::k_context_address)));
  str(cp, MemOperand(ip));

  // Setup argc and the builtin function in callee-saved registers.
  mov(r4, Operand(r0));
  mov(r5, Operand(r1));

  // Compute the argv pointer and keep it in a callee-saved register.
  add(r6, fp, Operand(r4, LSL, kPointerSizeLog2));
  add(r6, r6, Operand(ExitFrameConstants::kPPDisplacement - kPointerSize));

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Save the state of all registers to the stack from the memory
  // location. This is needed to allow nested break points.
  if (type == StackFrame::EXIT_DEBUG) {
    // Use sp as base to push.
    CopyRegistersFromMemoryToStack(sp, kJSCallerSaved);
  }
#endif
}


void MacroAssembler::LeaveExitFrame(StackFrame::Type type) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  // Restore the memory copy of the registers by digging them out from
  // the stack. This is needed to allow nested break points.
  if (type == StackFrame::EXIT_DEBUG) {
    // This code intentionally clobbers r2 and r3.
    const int kCallerSavedSize = kNumJSCallerSaved * kPointerSize;
    const int kOffset = ExitFrameConstants::kDebugMarkOffset - kCallerSavedSize;
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
  ldr(code_reg,
      MemOperand(code_reg, SharedFunctionInfo::kCodeOffset - kHeapObjectTag));
  add(code_reg, code_reg, Operand(Code::kHeaderSize - kHeapObjectTag));

  ParameterCount expected(expected_reg);
  InvokeCode(code_reg, expected, actual, flag);
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
#endif

void MacroAssembler::PushTryHandler(CodeLocation try_location,
                                    HandlerType type) {
  ASSERT(StackHandlerConstants::kSize == 6 * kPointerSize);  // adjust this code
  // The pc (return address) is passed in register lr.
  if (try_location == IN_JAVASCRIPT) {
    stm(db_w, sp, pp.bit() | fp.bit() | lr.bit());
    if (type == TRY_CATCH_HANDLER) {
      mov(r3, Operand(StackHandler::TRY_CATCH));
    } else {
      mov(r3, Operand(StackHandler::TRY_FINALLY));
    }
    push(r3);  // state
    mov(r3, Operand(ExternalReference(Top::k_handler_address)));
    ldr(r1, MemOperand(r3));
    push(r1);  // next sp
    str(sp, MemOperand(r3));  // chain handler
    mov(r0, Operand(Smi::FromInt(StackHandler::kCodeNotPresent)));  // new TOS
    push(r0);
  } else {
    // Must preserve r0-r4, r5-r7 are available.
    ASSERT(try_location == IN_JS_ENTRY);
    // The parameter pointer is meaningless here and fp does not point to a JS
    // frame. So we save NULL for both pp and fp. We expect the code throwing an
    // exception to check fp before dereferencing it to restore the context.
    mov(pp, Operand(0));  // set pp to NULL
    mov(ip, Operand(0));  // to save a NULL fp
    stm(db_w, sp, pp.bit() | ip.bit() | lr.bit());
    mov(r6, Operand(StackHandler::ENTRY));
    push(r6);  // state
    mov(r7, Operand(ExternalReference(Top::k_handler_address)));
    ldr(r6, MemOperand(r7));
    push(r6);  // next sp
    str(sp, MemOperand(r7));  // chain handler
    mov(r5, Operand(Smi::FromInt(StackHandler::kCodeNotPresent)));  // new TOS
    push(r5);  // flush TOS
  }
}


Register MacroAssembler::CheckMaps(JSObject* object, Register object_reg,
                                   JSObject* holder, Register holder_reg,
                                   Register scratch,
                                   Label* miss) {
  // Make sure there's no overlap between scratch and the other
  // registers.
  ASSERT(!scratch.is(object_reg) && !scratch.is(holder_reg));

  // Keep track of the current object in register reg.
  Register reg = object_reg;
  int depth = 1;

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

    // Go to the next object in the prototype chain.
    object = prototype;
  }

  // Check the holder map.
  ldr(scratch, FieldMemOperand(reg, HeapObject::kMapOffset));
  cmp(scratch, Operand(Handle<Map>(object->map())));
  b(ne, miss);

  // Log the check depth.
  LOG(IntEvent("check-maps-depth", depth));

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
    cmp(holder_reg, Operand(Factory::global_context_map()));
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
    cmp(holder_reg, Operand(Factory::null_value()));
    Check(ne, "JSGlobalProxy::context() should not be null.");

    ldr(holder_reg, FieldMemOperand(holder_reg, HeapObject::kMapOffset));
    cmp(holder_reg, Operand(Factory::global_context_map()));
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


void MacroAssembler::CallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls());  // stub calls are not allowed in some stubs
  Call(stub->GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::StubReturn(int argc) {
  ASSERT(argc >= 1 && generating_stub());
  if (argc > 1)
    add(sp, sp, Operand((argc - 1) * kPointerSize));
  Ret();
}


void MacroAssembler::IllegalOperation(int num_arguments) {
  if (num_arguments > 0) {
    add(sp, sp, Operand(num_arguments * kPointerSize));
  }
  mov(r0, Operand(Factory::undefined_value()));
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

  Runtime::FunctionId function_id =
      static_cast<Runtime::FunctionId>(f->stub_id);
  RuntimeStub stub(function_id, num_arguments);
  CallStub(&stub);
}


void MacroAssembler::CallRuntime(Runtime::FunctionId fid, int num_arguments) {
  CallRuntime(Runtime::FunctionForId(fid), num_arguments);
}


void MacroAssembler::TailCallRuntime(const ExternalReference& ext,
                                     int num_arguments) {
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r0, Operand(num_arguments));
  JumpToBuiltin(ext);
}


void MacroAssembler::JumpToBuiltin(const ExternalReference& builtin) {
#if defined(__thumb__)
  // Thumb mode builtin.
  ASSERT((reinterpret_cast<intptr_t>(builtin.address()) & 1) == 1);
#endif
  mov(r1, Operand(builtin));
  CEntryStub stub;
  Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


Handle<Code> MacroAssembler::ResolveBuiltin(Builtins::JavaScript id,
                                            bool* resolved) {
  // Contract with compiled functions is that the function is passed in r1.
  int builtins_offset =
      JSBuiltinsObject::kJSBuiltinsOffset + (id * kPointerSize);
  ldr(r1, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  ldr(r1, FieldMemOperand(r1, GlobalObject::kBuiltinsOffset));
  ldr(r1, FieldMemOperand(r1, builtins_offset));

  return Builtins::GetCode(id, resolved);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeJSFlags flags) {
  bool resolved;
  Handle<Code> code = ResolveBuiltin(id, &resolved);

  if (flags == CALL_JS) {
    Call(code, RelocInfo::CODE_TARGET);
  } else {
    ASSERT(flags == JUMP_JS);
    Jump(code, RelocInfo::CODE_TARGET);
  }

  if (!resolved) {
    const char* name = Builtins::GetName(id);
    int argc = Builtins::GetArgumentsCount(id);
    uint32_t flags =
        Bootstrapper::FixupFlagsArgumentsCount::encode(argc) |
        Bootstrapper::FixupFlagsIsPCRelative::encode(true) |
        Bootstrapper::FixupFlagsUseCodeObject::encode(false);
    Unresolved entry = { pc_offset() - sizeof(Instr), flags, name };
    unresolved_.Add(entry);
  }
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  bool resolved;
  Handle<Code> code = ResolveBuiltin(id, &resolved);

  mov(target, Operand(code));
  if (!resolved) {
    const char* name = Builtins::GetName(id);
    int argc = Builtins::GetArgumentsCount(id);
    uint32_t flags =
        Bootstrapper::FixupFlagsArgumentsCount::encode(argc) |
        Bootstrapper::FixupFlagsIsPCRelative::encode(true) |
        Bootstrapper::FixupFlagsUseCodeObject::encode(true);
    Unresolved entry = { pc_offset() - sizeof(Instr), flags, name };
    unresolved_.Add(entry);
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


void MacroAssembler::Check(Condition cc, const char* msg) {
  Label L;
  b(cc, &L);
  Abort(msg);
  // will not return here
  bind(&L);
}


void MacroAssembler::Abort(const char* msg) {
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
  mov(r0, Operand(p0));
  push(r0);
  mov(r0, Operand(Smi::FromInt(p1 - p0)));
  push(r0);
  CallRuntime(Runtime::kAbort, 2);
  // will not return here
}

} }  // namespace v8::internal

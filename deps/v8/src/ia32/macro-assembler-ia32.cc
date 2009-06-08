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
#include "serialize.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// MacroAssembler implementation.

MacroAssembler::MacroAssembler(void* buffer, int size)
    : Assembler(buffer, size),
      unresolved_(0),
      generating_stub_(false),
      allow_stub_calls_(true),
      code_object_(Heap::undefined_value()) {
}


static void RecordWriteHelper(MacroAssembler* masm,
                              Register object,
                              Register addr,
                              Register scratch) {
  Label fast;

  // Compute the page address from the heap object pointer, leave it
  // in 'object'.
  masm->and_(object, ~Page::kPageAlignmentMask);

  // Compute the bit addr in the remembered set, leave it in "addr".
  masm->sub(addr, Operand(object));
  masm->shr(addr, kObjectAlignmentBits);

  // If the bit offset lies beyond the normal remembered set range, it is in
  // the extra remembered set area of a large object.
  masm->cmp(addr, Page::kPageSize / kPointerSize);
  masm->j(less, &fast);

  // Adjust 'addr' to be relative to the start of the extra remembered set
  // and the page address in 'object' to be the address of the extra
  // remembered set.
  masm->sub(Operand(addr), Immediate(Page::kPageSize / kPointerSize));
  // Load the array length into 'scratch' and multiply by four to get the
  // size in bytes of the elements.
  masm->mov(scratch, Operand(object, Page::kObjectStartOffset
                                     + FixedArray::kLengthOffset));
  masm->shl(scratch, kObjectAlignmentBits);
  // Add the page header, array header, and array body size to the page
  // address.
  masm->add(Operand(object), Immediate(Page::kObjectStartOffset
                                       + Array::kHeaderSize));
  masm->add(object, Operand(scratch));


  // NOTE: For now, we use the bit-test-and-set (bts) x86 instruction
  // to limit code size. We should probably evaluate this decision by
  // measuring the performance of an equivalent implementation using
  // "simpler" instructions
  masm->bind(&fast);
  masm->bts(Operand(object, 0), addr);
}


class RecordWriteStub : public CodeStub {
 public:
  RecordWriteStub(Register object, Register addr, Register scratch)
      : object_(object), addr_(addr), scratch_(scratch) { }

  void Generate(MacroAssembler* masm);

 private:
  Register object_;
  Register addr_;
  Register scratch_;

#ifdef DEBUG
  void Print() {
    PrintF("RecordWriteStub (object reg %d), (addr reg %d), (scratch reg %d)\n",
           object_.code(), addr_.code(), scratch_.code());
  }
#endif

  // Minor key encoding in 12 bits of three registers (object, address and
  // scratch) OOOOAAAASSSS.
  class ScratchBits: public BitField<uint32_t, 0, 4> {};
  class AddressBits: public BitField<uint32_t, 4, 4> {};
  class ObjectBits: public BitField<uint32_t, 8, 4> {};

  Major MajorKey() { return RecordWrite; }

  int MinorKey() {
    // Encode the registers.
    return ObjectBits::encode(object_.code()) |
           AddressBits::encode(addr_.code()) |
           ScratchBits::encode(scratch_.code());
  }
};


void RecordWriteStub::Generate(MacroAssembler* masm) {
  RecordWriteHelper(masm, object_, addr_, scratch_);
  masm->ret(0);
}


// Set the remembered set bit for [object+offset].
// object is the object being stored into, value is the object being stored.
// If offset is zero, then the scratch register contains the array index into
// the elements array represented as a Smi.
// All registers are clobbered by the operation.
void MacroAssembler::RecordWrite(Register object, int offset,
                                 Register value, Register scratch) {
  // First, check if a remembered set write is even needed. The tests below
  // catch stores of Smis and stores into young gen (which does not have space
  // for the remembered set bits.
  Label done;

  // This optimization cannot survive serialization and deserialization,
  // so we disable as long as serialization can take place.
  int32_t new_space_start =
      reinterpret_cast<int32_t>(ExternalReference::new_space_start().address());
  if (Serializer::enabled() || new_space_start < 0) {
    // Cannot do smart bit-twiddling. Need to do two consecutive checks.
    // Check for Smi first.
    test(value, Immediate(kSmiTagMask));
    j(zero, &done);
    // Test that the object address is not in the new space.  We cannot
    // set remembered set bits in the new space.
    mov(value, Operand(object));
    and_(value, Heap::NewSpaceMask());
    cmp(Operand(value), Immediate(ExternalReference::new_space_start()));
    j(equal, &done);
  } else {
    // move the value SmiTag into the sign bit
    shl(value, 31);
    // combine the object with value SmiTag
    or_(value, Operand(object));
    // remove the uninteresing bits inside the page
    and_(value, Heap::NewSpaceMask() | (1 << 31));
    // xor has two effects:
    // - if the value was a smi, then the result will be negative
    // - if the object is pointing into new space area the page bits will
    //   all be zero
    xor_(value, new_space_start | (1 << 31));
    // Check for both conditions in one branch
    j(less_equal, &done);
  }

  if ((offset > 0) && (offset < Page::kMaxHeapObjectSize)) {
    // Compute the bit offset in the remembered set, leave it in 'value'.
    mov(value, Operand(object));
    and_(value, Page::kPageAlignmentMask);
    add(Operand(value), Immediate(offset));
    shr(value, kObjectAlignmentBits);

    // Compute the page address from the heap object pointer, leave it in
    // 'object'.
    and_(object, ~Page::kPageAlignmentMask);

    // NOTE: For now, we use the bit-test-and-set (bts) x86 instruction
    // to limit code size. We should probably evaluate this decision by
    // measuring the performance of an equivalent implementation using
    // "simpler" instructions
    bts(Operand(object, 0), value);
  } else {
    Register dst = scratch;
    if (offset != 0) {
      lea(dst, Operand(object, offset));
    } else {
      // array access: calculate the destination address in the same manner as
      // KeyedStoreIC::GenerateGeneric
      lea(dst,
          Operand(object, dst, times_2, Array::kHeaderSize - kHeapObjectTag));
    }
    // If we are already generating a shared stub, not inlining the
    // record write code isn't going to save us any memory.
    if (generating_stub()) {
      RecordWriteHelper(this, object, dst, value);
    } else {
      RecordWriteStub stub(object, dst, value);
      CallStub(&stub);
    }
  }

  bind(&done);
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void MacroAssembler::SaveRegistersToMemory(RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Copy the content of registers to memory location.
  for (int i = 0; i < kNumJSCallerSaved; i++) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      Register reg = { r };
      ExternalReference reg_addr =
          ExternalReference(Debug_Address::Register(i));
      mov(Operand::StaticVariable(reg_addr), reg);
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
      ExternalReference reg_addr =
          ExternalReference(Debug_Address::Register(i));
      mov(reg, Operand::StaticVariable(reg_addr));
    }
  }
}


void MacroAssembler::PushRegistersFromMemory(RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Push the content of the memory location to the stack.
  for (int i = 0; i < kNumJSCallerSaved; i++) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      ExternalReference reg_addr =
          ExternalReference(Debug_Address::Register(i));
      push(Operand::StaticVariable(reg_addr));
    }
  }
}


void MacroAssembler::PopRegistersToMemory(RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Pop the content from the stack to the memory location.
  for (int i = kNumJSCallerSaved; --i >= 0;) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      ExternalReference reg_addr =
          ExternalReference(Debug_Address::Register(i));
      pop(Operand::StaticVariable(reg_addr));
    }
  }
}


void MacroAssembler::CopyRegistersFromStackToMemory(Register base,
                                                    Register scratch,
                                                    RegList regs) {
  ASSERT((regs & ~kJSCallerSaved) == 0);
  // Copy the content of the stack to the memory location and adjust base.
  for (int i = kNumJSCallerSaved; --i >= 0;) {
    int r = JSCallerSavedCode(i);
    if ((regs & (1 << r)) != 0) {
      mov(scratch, Operand(base, 0));
      ExternalReference reg_addr =
          ExternalReference(Debug_Address::Register(i));
      mov(Operand::StaticVariable(reg_addr), scratch);
      lea(base, Operand(base, kPointerSize));
    }
  }
}
#endif

void MacroAssembler::Set(Register dst, const Immediate& x) {
  if (x.is_zero()) {
    xor_(dst, Operand(dst));  // shorter than mov
  } else {
    mov(dst, x);
  }
}


void MacroAssembler::Set(const Operand& dst, const Immediate& x) {
  mov(dst, x);
}


void MacroAssembler::CmpObjectType(Register heap_object,
                                   InstanceType type,
                                   Register map) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  CmpInstanceType(map, type);
}


void MacroAssembler::CmpInstanceType(Register map, InstanceType type) {
  cmpb(FieldOperand(map, Map::kInstanceTypeOffset),
       static_cast<int8_t>(type));
}


void MacroAssembler::FCmp() {
  fcompp();
  push(eax);
  fnstsw_ax();
  sahf();
  pop(eax);
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  push(ebp);
  mov(ebp, Operand(esp));
  push(esi);
  push(Immediate(Smi::FromInt(type)));
  push(Immediate(CodeObject()));
  if (FLAG_debug_code) {
    cmp(Operand(esp, 0), Immediate(Factory::undefined_value()));
    Check(not_equal, "code object not properly patched");
  }
}


void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  if (FLAG_debug_code) {
    cmp(Operand(ebp, StandardFrameConstants::kMarkerOffset),
        Immediate(Smi::FromInt(type)));
    Check(equal, "stack frame types must match");
  }
  leave();
}


void MacroAssembler::EnterExitFrame(StackFrame::Type type) {
  ASSERT(type == StackFrame::EXIT || type == StackFrame::EXIT_DEBUG);

  // Setup the frame structure on the stack.
  ASSERT(ExitFrameConstants::kPPDisplacement == +2 * kPointerSize);
  ASSERT(ExitFrameConstants::kCallerPCOffset == +1 * kPointerSize);
  ASSERT(ExitFrameConstants::kCallerFPOffset ==  0 * kPointerSize);
  push(ebp);
  mov(ebp, Operand(esp));

  // Reserve room for entry stack pointer and push the debug marker.
  ASSERT(ExitFrameConstants::kSPOffset  == -1 * kPointerSize);
  push(Immediate(0));  // saved entry sp, patched before call
  push(Immediate(type == StackFrame::EXIT_DEBUG ? 1 : 0));

  // Save the frame pointer and the context in top.
  ExternalReference c_entry_fp_address(Top::k_c_entry_fp_address);
  ExternalReference context_address(Top::k_context_address);
  mov(Operand::StaticVariable(c_entry_fp_address), ebp);
  mov(Operand::StaticVariable(context_address), esi);

  // Setup argc and argv in callee-saved registers.
  int offset = StandardFrameConstants::kCallerSPOffset - kPointerSize;
  mov(edi, Operand(eax));
  lea(esi, Operand(ebp, eax, times_4, offset));

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Save the state of all registers to the stack from the memory
  // location. This is needed to allow nested break points.
  if (type == StackFrame::EXIT_DEBUG) {
    // TODO(1243899): This should be symmetric to
    // CopyRegistersFromStackToMemory() but it isn't! esp is assumed
    // correct here, but computed for the other call. Very error
    // prone! FIX THIS.  Actually there are deeper problems with
    // register saving than this asymmetry (see the bug report
    // associated with this issue).
    PushRegistersFromMemory(kJSCallerSaved);
  }
#endif

  // Reserve space for two arguments: argc and argv.
  sub(Operand(esp), Immediate(2 * kPointerSize));

  // Get the required frame alignment for the OS.
  static const int kFrameAlignment = OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    ASSERT(IsPowerOf2(kFrameAlignment));
    and_(esp, -kFrameAlignment);
  }

  // Patch the saved entry sp.
  mov(Operand(ebp, ExitFrameConstants::kSPOffset), esp);
}


void MacroAssembler::LeaveExitFrame(StackFrame::Type type) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  // Restore the memory copy of the registers by digging them out from
  // the stack. This is needed to allow nested break points.
  if (type == StackFrame::EXIT_DEBUG) {
    // It's okay to clobber register ebx below because we don't need
    // the function pointer after this.
    const int kCallerSavedSize = kNumJSCallerSaved * kPointerSize;
    int kOffset = ExitFrameConstants::kDebugMarkOffset - kCallerSavedSize;
    lea(ebx, Operand(ebp, kOffset));
    CopyRegistersFromStackToMemory(ebx, ecx, kJSCallerSaved);
  }
#endif

  // Get the return address from the stack and restore the frame pointer.
  mov(ecx, Operand(ebp, 1 * kPointerSize));
  mov(ebp, Operand(ebp, 0 * kPointerSize));

  // Pop the arguments and the receiver from the caller stack.
  lea(esp, Operand(esi, 1 * kPointerSize));

  // Restore current context from top and clear it in debug mode.
  ExternalReference context_address(Top::k_context_address);
  mov(esi, Operand::StaticVariable(context_address));
#ifdef DEBUG
  mov(Operand::StaticVariable(context_address), Immediate(0));
#endif

  // Push the return address to get ready to return.
  push(ecx);

  // Clear the top frame.
  ExternalReference c_entry_fp_address(Top::k_c_entry_fp_address);
  mov(Operand::StaticVariable(c_entry_fp_address), Immediate(0));
}


void MacroAssembler::PushTryHandler(CodeLocation try_location,
                                    HandlerType type) {
  ASSERT(StackHandlerConstants::kSize == 6 * kPointerSize);  // adjust this code
  // The pc (return address) is already on TOS.
  if (try_location == IN_JAVASCRIPT) {
    if (type == TRY_CATCH_HANDLER) {
      push(Immediate(StackHandler::TRY_CATCH));
    } else {
      push(Immediate(StackHandler::TRY_FINALLY));
    }
    push(Immediate(Smi::FromInt(StackHandler::kCodeNotPresent)));
    push(ebp);
    push(edi);
  } else {
    ASSERT(try_location == IN_JS_ENTRY);
    // The parameter pointer is meaningless here and ebp does not
    // point to a JS frame. So we save NULL for both pp and ebp. We
    // expect the code throwing an exception to check ebp before
    // dereferencing it to restore the context.
    push(Immediate(StackHandler::ENTRY));
    push(Immediate(Smi::FromInt(StackHandler::kCodeNotPresent)));
    push(Immediate(0));  // NULL frame pointer
    push(Immediate(0));  // NULL parameter pointer
  }
  // Cached TOS.
  mov(eax, Operand::StaticVariable(ExternalReference(Top::k_handler_address)));
  // Link this handler.
  mov(Operand::StaticVariable(ExternalReference(Top::k_handler_address)), esp);
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

    JSObject* prototype = JSObject::cast(object->GetPrototype());
    if (Heap::InNewSpace(prototype)) {
      // Get the map of the current object.
      mov(scratch, FieldOperand(reg, HeapObject::kMapOffset));
      cmp(Operand(scratch), Immediate(Handle<Map>(object->map())));
      // Branch on the result of the map check.
      j(not_equal, miss, not_taken);
      // Check access rights to the global object.  This has to happen
      // after the map check so that we know that the object is
      // actually a global object.
      if (object->IsJSGlobalProxy()) {
        CheckAccessGlobalProxy(reg, scratch, miss);

        // Restore scratch register to be the map of the object.
        // We load the prototype from the map in the scratch register.
        mov(scratch, FieldOperand(reg, HeapObject::kMapOffset));
      }
      // The prototype is in new space; we cannot store a reference
      // to it in the code. Load it from the map.
      reg = holder_reg;  // from now the object is in holder_reg
      mov(reg, FieldOperand(scratch, Map::kPrototypeOffset));

    } else {
      // Check the map of the current object.
      cmp(FieldOperand(reg, HeapObject::kMapOffset),
          Immediate(Handle<Map>(object->map())));
      // Branch on the result of the map check.
      j(not_equal, miss, not_taken);
      // Check access rights to the global object.  This has to happen
      // after the map check so that we know that the object is
      // actually a global object.
      if (object->IsJSGlobalProxy()) {
        CheckAccessGlobalProxy(reg, scratch, miss);
      }
      // The prototype is in old space; load it directly.
      reg = holder_reg;  // from now the object is in holder_reg
      mov(reg, Handle<JSObject>(prototype));
    }

    // Go to the next object in the prototype chain.
    object = prototype;
  }

  // Check the holder map.
  cmp(FieldOperand(reg, HeapObject::kMapOffset),
      Immediate(Handle<Map>(holder->map())));
  j(not_equal, miss, not_taken);

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

  // Load current lexical context from the stack frame.
  mov(scratch, Operand(ebp, StandardFrameConstants::kContextOffset));

  // When generating debug code, make sure the lexical context is set.
  if (FLAG_debug_code) {
    cmp(Operand(scratch), Immediate(0));
    Check(not_equal, "we should not have an empty lexical context");
  }
  // Load the global context of the current context.
  int offset = Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
  mov(scratch, FieldOperand(scratch, offset));
  mov(scratch, FieldOperand(scratch, GlobalObject::kGlobalContextOffset));

  // Check the context is a global context.
  if (FLAG_debug_code) {
    push(scratch);
    // Read the first word and compare to global_context_map.
    mov(scratch, FieldOperand(scratch, HeapObject::kMapOffset));
    cmp(scratch, Factory::global_context_map());
    Check(equal, "JSGlobalObject::global_context should be a global context.");
    pop(scratch);
  }

  // Check if both contexts are the same.
  cmp(scratch, FieldOperand(holder_reg, JSGlobalProxy::kContextOffset));
  j(equal, &same_contexts, taken);

  // Compare security tokens, save holder_reg on the stack so we can use it
  // as a temporary register.
  //
  // TODO(119): avoid push(holder_reg)/pop(holder_reg)
  push(holder_reg);
  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.
  mov(holder_reg, FieldOperand(holder_reg, JSGlobalProxy::kContextOffset));

  // Check the context is a global context.
  if (FLAG_debug_code) {
    cmp(holder_reg, Factory::null_value());
    Check(not_equal, "JSGlobalProxy::context() should not be null.");

    push(holder_reg);
    // Read the first word and compare to global_context_map(),
    mov(holder_reg, FieldOperand(holder_reg, HeapObject::kMapOffset));
    cmp(holder_reg, Factory::global_context_map());
    Check(equal, "JSGlobalObject::global_context should be a global context.");
    pop(holder_reg);
  }

  int token_offset = Context::kHeaderSize +
                     Context::SECURITY_TOKEN_INDEX * kPointerSize;
  mov(scratch, FieldOperand(scratch, token_offset));
  cmp(scratch, FieldOperand(holder_reg, token_offset));
  pop(holder_reg);
  j(not_equal, miss, not_taken);

  bind(&same_contexts);
}


void MacroAssembler::NegativeZeroTest(CodeGenerator* cgen,
                                      Register result,
                                      Register op,
                                      JumpTarget* then_target) {
  JumpTarget ok;
  test(result, Operand(result));
  ok.Branch(not_zero, taken);
  test(op, Operand(op));
  then_target->Branch(sign, not_taken);
  ok.Bind();
}


void MacroAssembler::NegativeZeroTest(Register result,
                                      Register op,
                                      Label* then_label) {
  Label ok;
  test(result, Operand(result));
  j(not_zero, &ok, taken);
  test(op, Operand(op));
  j(sign, then_label, not_taken);
  bind(&ok);
}


void MacroAssembler::NegativeZeroTest(Register result,
                                      Register op1,
                                      Register op2,
                                      Register scratch,
                                      Label* then_label) {
  Label ok;
  test(result, Operand(result));
  j(not_zero, &ok, taken);
  mov(scratch, Operand(op1));
  or_(scratch, Operand(op2));
  j(sign, then_label, not_taken);
  bind(&ok);
}


void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Register scratch,
                                             Label* miss) {
  // Check that the receiver isn't a smi.
  test(function, Immediate(kSmiTagMask));
  j(zero, miss, not_taken);

  // Check that the function really is a function.
  CmpObjectType(function, JS_FUNCTION_TYPE, result);
  j(not_equal, miss, not_taken);

  // Make sure that the function has an instance prototype.
  Label non_instance;
  movzx_b(scratch, FieldOperand(result, Map::kBitFieldOffset));
  test(scratch, Immediate(1 << Map::kHasNonInstancePrototype));
  j(not_zero, &non_instance, not_taken);

  // Get the prototype or initial map from the function.
  mov(result,
      FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and
  // simply miss the cache instead. This will allow us to allocate a
  // prototype object on-demand in the runtime system.
  cmp(Operand(result), Immediate(Factory::the_hole_value()));
  j(equal, miss, not_taken);

  // If the function does not have an initial map, we're done.
  Label done;
  CmpObjectType(result, MAP_TYPE, scratch);
  j(not_equal, &done);

  // Get the prototype from the initial map.
  mov(result, FieldOperand(result, Map::kPrototypeOffset));
  jmp(&done);

  // Non-instance prototype: Fetch prototype from constructor field
  // in initial map.
  bind(&non_instance);
  mov(result, FieldOperand(result, Map::kConstructorOffset));

  // All done.
  bind(&done);
}


void MacroAssembler::CallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls());  // calls are not allowed in some stubs
  call(stub->GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::StubReturn(int argc) {
  ASSERT(argc >= 1 && generating_stub());
  ret((argc - 1) * kPointerSize);
}


void MacroAssembler::IllegalOperation(int num_arguments) {
  if (num_arguments > 0) {
    add(Operand(esp), Immediate(num_arguments * kPointerSize));
  }
  mov(eax, Immediate(Factory::undefined_value()));
}


void MacroAssembler::CallRuntime(Runtime::FunctionId id, int num_arguments) {
  CallRuntime(Runtime::FunctionForId(id), num_arguments);
}


void MacroAssembler::CallRuntime(Runtime::Function* f, int num_arguments) {
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


void MacroAssembler::TailCallRuntime(const ExternalReference& ext,
                                     int num_arguments) {
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Set(eax, Immediate(num_arguments));
  JumpToBuiltin(ext);
}


void MacroAssembler::JumpToBuiltin(const ExternalReference& ext) {
  // Set the entry point and jump to the C entry runtime stub.
  mov(ebx, Immediate(ext));
  CEntryStub ces;
  jmp(ces.GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    const Operand& code_operand,
                                    Label* done,
                                    InvokeFlag flag) {
  bool definitely_matches = false;
  Label invoke;
  if (expected.is_immediate()) {
    ASSERT(actual.is_immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      mov(eax, actual.immediate());
      const int sentinel = SharedFunctionInfo::kDontAdaptArgumentsSentinel;
      if (expected.immediate() == sentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        mov(ebx, expected.immediate());
      }
    }
  } else {
    if (actual.is_immediate()) {
      // Expected is in register, actual is immediate. This is the
      // case when we invoke function values without going through the
      // IC mechanism.
      cmp(expected.reg(), actual.immediate());
      j(equal, &invoke);
      ASSERT(expected.reg().is(ebx));
      mov(eax, actual.immediate());
    } else if (!expected.reg().is(actual.reg())) {
      // Both expected and actual are in (different) registers. This
      // is the case when we invoke functions using call and apply.
      cmp(expected.reg(), Operand(actual.reg()));
      j(equal, &invoke);
      ASSERT(actual.reg().is(eax));
      ASSERT(expected.reg().is(ebx));
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor =
        Handle<Code>(Builtins::builtin(Builtins::ArgumentsAdaptorTrampoline));
    if (!code_constant.is_null()) {
      mov(edx, Immediate(code_constant));
      add(Operand(edx), Immediate(Code::kHeaderSize - kHeapObjectTag));
    } else if (!code_operand.is_reg(edx)) {
      mov(edx, code_operand);
    }

    if (flag == CALL_FUNCTION) {
      call(adaptor, RelocInfo::CODE_TARGET);
      jmp(done);
    } else {
      jmp(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&invoke);
  }
}


void MacroAssembler::InvokeCode(const Operand& code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                InvokeFlag flag) {
  Label done;
  InvokePrologue(expected, actual, Handle<Code>::null(), code, &done, flag);
  if (flag == CALL_FUNCTION) {
    call(code);
  } else {
    ASSERT(flag == JUMP_FUNCTION);
    jmp(code);
  }
  bind(&done);
}


void MacroAssembler::InvokeCode(Handle<Code> code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                RelocInfo::Mode rmode,
                                InvokeFlag flag) {
  Label done;
  Operand dummy(eax);
  InvokePrologue(expected, actual, code, dummy, &done, flag);
  if (flag == CALL_FUNCTION) {
    call(code, rmode);
  } else {
    ASSERT(flag == JUMP_FUNCTION);
    jmp(code, rmode);
  }
  bind(&done);
}


void MacroAssembler::InvokeFunction(Register fun,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  ASSERT(fun.is(edi));
  mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  mov(ebx, FieldOperand(edx, SharedFunctionInfo::kFormalParameterCountOffset));
  mov(edx, FieldOperand(edx, SharedFunctionInfo::kCodeOffset));
  lea(edx, FieldOperand(edx, Code::kHeaderSize));

  ParameterCount expected(ebx);
  InvokeCode(Operand(edx), expected, actual, flag);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id, InvokeFlag flag) {
  bool resolved;
  Handle<Code> code = ResolveBuiltin(id, &resolved);

  // Calls are not allowed in some stubs.
  ASSERT(flag == JUMP_FUNCTION || allow_stub_calls());

  // Rely on the assertion to check that the number of provided
  // arguments match the expected number of arguments. Fake a
  // parameter count to avoid emitting code to do the check.
  ParameterCount expected(0);
  InvokeCode(Handle<Code>(code), expected, expected,
             RelocInfo::CODE_TARGET, flag);

  const char* name = Builtins::GetName(id);
  int argc = Builtins::GetArgumentsCount(id);

  if (!resolved) {
    uint32_t flags =
        Bootstrapper::FixupFlagsArgumentsCount::encode(argc) |
        Bootstrapper::FixupFlagsIsPCRelative::encode(true) |
        Bootstrapper::FixupFlagsUseCodeObject::encode(false);
    Unresolved entry = { pc_offset() - sizeof(int32_t), flags, name };
    unresolved_.Add(entry);
  }
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  bool resolved;
  Handle<Code> code = ResolveBuiltin(id, &resolved);

  const char* name = Builtins::GetName(id);
  int argc = Builtins::GetArgumentsCount(id);

  mov(Operand(target), Immediate(code));
  if (!resolved) {
    uint32_t flags =
        Bootstrapper::FixupFlagsArgumentsCount::encode(argc) |
        Bootstrapper::FixupFlagsIsPCRelative::encode(false) |
        Bootstrapper::FixupFlagsUseCodeObject::encode(true);
    Unresolved entry = { pc_offset() - sizeof(int32_t), flags, name };
    unresolved_.Add(entry);
  }
  add(Operand(target), Immediate(Code::kHeaderSize - kHeapObjectTag));
}


Handle<Code> MacroAssembler::ResolveBuiltin(Builtins::JavaScript id,
                                            bool* resolved) {
  // Move the builtin function into the temporary function slot by
  // reading it from the builtins object. NOTE: We should be able to
  // reduce this to two instructions by putting the function table in
  // the global object instead of the "builtins" object and by using a
  // real register for the function.
  mov(edx, Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  mov(edx, FieldOperand(edx, GlobalObject::kBuiltinsOffset));
  int builtins_offset =
      JSBuiltinsObject::kJSBuiltinsOffset + (id * kPointerSize);
  mov(edi, FieldOperand(edx, builtins_offset));


  return Builtins::GetCode(id, resolved);
}


void MacroAssembler::Ret() {
  ret(0);
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(Operand::StaticVariable(ExternalReference(counter)), Immediate(value));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand operand = Operand::StaticVariable(ExternalReference(counter));
    if (value == 1) {
      inc(operand);
    } else {
      add(operand, Immediate(value));
    }
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand operand = Operand::StaticVariable(ExternalReference(counter));
    if (value == 1) {
      dec(operand);
    } else {
      sub(operand, Immediate(value));
    }
  }
}


void MacroAssembler::Assert(Condition cc, const char* msg) {
  if (FLAG_debug_code) Check(cc, msg);
}


void MacroAssembler::Check(Condition cc, const char* msg) {
  Label L;
  j(cc, &L, taken);
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
  push(eax);
  push(Immediate(p0));
  push(Immediate(reinterpret_cast<intptr_t>(Smi::FromInt(p1 - p0))));
  CallRuntime(Runtime::kAbort, 2);
  // will not return here
}


CodePatcher::CodePatcher(byte* address, int size)
  : address_(address), size_(size), masm_(address, size + Assembler::kGap) {
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


} }  // namespace v8::internal

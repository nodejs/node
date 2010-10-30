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

#if defined(V8_TARGET_ARCH_IA32)

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
      generating_stub_(false),
      allow_stub_calls_(true),
      code_object_(Heap::undefined_value()) {
}


void MacroAssembler::RecordWriteHelper(Register object,
                                       Register addr,
                                       Register scratch) {
  if (FLAG_debug_code) {
    // Check that the object is not in new space.
    Label not_in_new_space;
    InNewSpace(object, scratch, not_equal, &not_in_new_space);
    Abort("new-space object passed to RecordWriteHelper");
    bind(&not_in_new_space);
  }

  // Compute the page start address from the heap object pointer, and reuse
  // the 'object' register for it.
  and_(object, ~Page::kPageAlignmentMask);

  // Compute number of region covering addr. See Page::GetRegionNumberForAddress
  // method for more details.
  and_(addr, Page::kPageAlignmentMask);
  shr(addr, Page::kRegionSizeLog2);

  // Set dirty mark for region.
  bts(Operand(object, Page::kDirtyFlagOffset), addr);
}


void MacroAssembler::InNewSpace(Register object,
                                Register scratch,
                                Condition cc,
                                Label* branch) {
  ASSERT(cc == equal || cc == not_equal);
  if (Serializer::enabled()) {
    // Can't do arithmetic on external references if it might get serialized.
    mov(scratch, Operand(object));
    // The mask isn't really an address.  We load it as an external reference in
    // case the size of the new space is different between the snapshot maker
    // and the running system.
    and_(Operand(scratch), Immediate(ExternalReference::new_space_mask()));
    cmp(Operand(scratch), Immediate(ExternalReference::new_space_start()));
    j(cc, branch);
  } else {
    int32_t new_space_start = reinterpret_cast<int32_t>(
        ExternalReference::new_space_start().address());
    lea(scratch, Operand(object, -new_space_start));
    and_(scratch, Heap::NewSpaceMask());
    j(cc, branch);
  }
}


void MacroAssembler::RecordWrite(Register object,
                                 int offset,
                                 Register value,
                                 Register scratch) {
  // The compiled code assumes that record write doesn't change the
  // context register, so we check that none of the clobbered
  // registers are esi.
  ASSERT(!object.is(esi) && !value.is(esi) && !scratch.is(esi));

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis and stores into young gen.
  Label done;

  // Skip barrier if writing a smi.
  ASSERT_EQ(0, kSmiTag);
  test(value, Immediate(kSmiTagMask));
  j(zero, &done);

  InNewSpace(object, value, equal, &done);

  // The offset is relative to a tagged or untagged HeapObject pointer,
  // so either offset or offset + kHeapObjectTag must be a
  // multiple of kPointerSize.
  ASSERT(IsAligned(offset, kPointerSize) ||
         IsAligned(offset + kHeapObjectTag, kPointerSize));

  Register dst = scratch;
  if (offset != 0) {
    lea(dst, Operand(object, offset));
  } else {
    // Array access: calculate the destination address in the same manner as
    // KeyedStoreIC::GenerateGeneric.  Multiply a smi by 2 to get an offset
    // into an array of words.
    ASSERT_EQ(1, kSmiTagSize);
    ASSERT_EQ(0, kSmiTag);
    lea(dst, Operand(object, dst, times_half_pointer_size,
                     FixedArray::kHeaderSize - kHeapObjectTag));
  }
  RecordWriteHelper(object, dst, value);

  bind(&done);

  // Clobber all input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (FLAG_debug_code) {
    mov(object, Immediate(BitCast<int32_t>(kZapValue)));
    mov(value, Immediate(BitCast<int32_t>(kZapValue)));
    mov(scratch, Immediate(BitCast<int32_t>(kZapValue)));
  }
}


void MacroAssembler::RecordWrite(Register object,
                                 Register address,
                                 Register value) {
  // The compiled code assumes that record write doesn't change the
  // context register, so we check that none of the clobbered
  // registers are esi.
  ASSERT(!object.is(esi) && !value.is(esi) && !address.is(esi));

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis and stores into young gen.
  Label done;

  // Skip barrier if writing a smi.
  ASSERT_EQ(0, kSmiTag);
  test(value, Immediate(kSmiTagMask));
  j(zero, &done);

  InNewSpace(object, value, equal, &done);

  RecordWriteHelper(object, address, value);

  bind(&done);

  // Clobber all input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (FLAG_debug_code) {
    mov(object, Immediate(BitCast<int32_t>(kZapValue)));
    mov(address, Immediate(BitCast<int32_t>(kZapValue)));
    mov(value, Immediate(BitCast<int32_t>(kZapValue)));
  }
}


void MacroAssembler::StackLimitCheck(Label* on_stack_overflow) {
  cmp(esp,
      Operand::StaticVariable(ExternalReference::address_of_stack_limit()));
  j(below, on_stack_overflow);
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void MacroAssembler::DebugBreak() {
  Set(eax, Immediate(0));
  mov(ebx, Immediate(ExternalReference(Runtime::kDebugBreak)));
  CEntryStub ces(1);
  call(ces.GetCode(), RelocInfo::DEBUG_BREAK);
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


void MacroAssembler::CheckMap(Register obj,
                              Handle<Map> map,
                              Label* fail,
                              bool is_heap_object) {
  if (!is_heap_object) {
    test(obj, Immediate(kSmiTagMask));
    j(zero, fail);
  }
  cmp(FieldOperand(obj, HeapObject::kMapOffset), Immediate(map));
  j(not_equal, fail);
}


Condition MacroAssembler::IsObjectStringType(Register heap_object,
                                             Register map,
                                             Register instance_type) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  movzx_b(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  ASSERT(kNotStringTag != 0);
  test(instance_type, Immediate(kIsNotStringMask));
  return zero;
}


void MacroAssembler::IsObjectJSObjectType(Register heap_object,
                                          Register map,
                                          Register scratch,
                                          Label* fail) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  IsInstanceJSObjectType(map, scratch, fail);
}


void MacroAssembler::IsInstanceJSObjectType(Register map,
                                            Register scratch,
                                            Label* fail) {
  movzx_b(scratch, FieldOperand(map, Map::kInstanceTypeOffset));
  sub(Operand(scratch), Immediate(FIRST_JS_OBJECT_TYPE));
  cmp(scratch, LAST_JS_OBJECT_TYPE - FIRST_JS_OBJECT_TYPE);
  j(above, fail);
}


void MacroAssembler::FCmp() {
  if (CpuFeatures::IsSupported(CMOV)) {
    fucomip();
    ffree(0);
    fincstp();
  } else {
    fucompp();
    push(eax);
    fnstsw_ax();
    sahf();
    pop(eax);
  }
}


void MacroAssembler::AbortIfNotNumber(Register object) {
  Label ok;
  test(object, Immediate(kSmiTagMask));
  j(zero, &ok);
  cmp(FieldOperand(object, HeapObject::kMapOffset),
      Factory::heap_number_map());
  Assert(equal, "Operand not a number");
  bind(&ok);
}


void MacroAssembler::AbortIfNotSmi(Register object) {
  test(object, Immediate(kSmiTagMask));
  Assert(equal, "Operand is not a smi");
}


void MacroAssembler::AbortIfNotString(Register object) {
  test(object, Immediate(kSmiTagMask));
  Assert(not_equal, "Operand is not a string");
  push(object);
  mov(object, FieldOperand(object, HeapObject::kMapOffset));
  CmpInstanceType(object, FIRST_NONSTRING_TYPE);
  pop(object);
  Assert(below, "Operand is not a string");
}


void MacroAssembler::AbortIfSmi(Register object) {
  test(object, Immediate(kSmiTagMask));
  Assert(not_equal, "Operand is a smi");
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


void MacroAssembler::EnterExitFramePrologue() {
  // Setup the frame structure on the stack.
  ASSERT(ExitFrameConstants::kCallerSPDisplacement == +2 * kPointerSize);
  ASSERT(ExitFrameConstants::kCallerPCOffset == +1 * kPointerSize);
  ASSERT(ExitFrameConstants::kCallerFPOffset ==  0 * kPointerSize);
  push(ebp);
  mov(ebp, Operand(esp));

  // Reserve room for entry stack pointer and push the code object.
  ASSERT(ExitFrameConstants::kSPOffset  == -1 * kPointerSize);
  push(Immediate(0));  // Saved entry sp, patched before call.
  push(Immediate(CodeObject()));  // Accessed from ExitFrame::code_slot.

  // Save the frame pointer and the context in top.
  ExternalReference c_entry_fp_address(Top::k_c_entry_fp_address);
  ExternalReference context_address(Top::k_context_address);
  mov(Operand::StaticVariable(c_entry_fp_address), ebp);
  mov(Operand::StaticVariable(context_address), esi);
}


void MacroAssembler::EnterExitFrameEpilogue(int argc) {
  // Reserve space for arguments.
  sub(Operand(esp), Immediate(argc * kPointerSize));

  // Get the required frame alignment for the OS.
  static const int kFrameAlignment = OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    ASSERT(IsPowerOf2(kFrameAlignment));
    and_(esp, -kFrameAlignment);
  }

  // Patch the saved entry sp.
  mov(Operand(ebp, ExitFrameConstants::kSPOffset), esp);
}


void MacroAssembler::EnterExitFrame() {
  EnterExitFramePrologue();

  // Setup argc and argv in callee-saved registers.
  int offset = StandardFrameConstants::kCallerSPOffset - kPointerSize;
  mov(edi, Operand(eax));
  lea(esi, Operand(ebp, eax, times_4, offset));

  EnterExitFrameEpilogue(2);
}


void MacroAssembler::EnterApiExitFrame(int stack_space,
                                       int argc) {
  EnterExitFramePrologue();

  int offset = StandardFrameConstants::kCallerSPOffset - kPointerSize;
  lea(esi, Operand(ebp, (stack_space * kPointerSize) + offset));

  EnterExitFrameEpilogue(argc);
}


void MacroAssembler::LeaveExitFrame() {
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
  // Adjust this code if not the case.
  ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize);
  // The pc (return address) is already on TOS.
  if (try_location == IN_JAVASCRIPT) {
    if (type == TRY_CATCH_HANDLER) {
      push(Immediate(StackHandler::TRY_CATCH));
    } else {
      push(Immediate(StackHandler::TRY_FINALLY));
    }
    push(ebp);
  } else {
    ASSERT(try_location == IN_JS_ENTRY);
    // The frame pointer does not point to a JS frame so we save NULL
    // for ebp. We expect the code throwing an exception to check ebp
    // before dereferencing it to restore the context.
    push(Immediate(StackHandler::ENTRY));
    push(Immediate(0));  // NULL frame pointer.
  }
  // Save the current handler as the next handler.
  push(Operand::StaticVariable(ExternalReference(Top::k_handler_address)));
  // Link this handler as the new current one.
  mov(Operand::StaticVariable(ExternalReference(Top::k_handler_address)), esp);
}


void MacroAssembler::PopTryHandler() {
  ASSERT_EQ(0, StackHandlerConstants::kNextOffset);
  pop(Operand::StaticVariable(ExternalReference(Top::k_handler_address)));
  add(Operand(esp), Immediate(StackHandlerConstants::kSize - kPointerSize));
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


void MacroAssembler::LoadAllocationTopHelper(Register result,
                                             Register result_end,
                                             Register scratch,
                                             AllocationFlags flags) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();

  // Just return if allocation top is already known.
  if ((flags & RESULT_CONTAINS_TOP) != 0) {
    // No use of scratch if allocation top is provided.
    ASSERT(scratch.is(no_reg));
#ifdef DEBUG
    // Assert that result actually contains top on entry.
    cmp(result, Operand::StaticVariable(new_space_allocation_top));
    Check(equal, "Unexpected allocation top");
#endif
    return;
  }

  // Move address of new object to result. Use scratch register if available.
  if (scratch.is(no_reg)) {
    mov(result, Operand::StaticVariable(new_space_allocation_top));
  } else {
    ASSERT(!scratch.is(result_end));
    mov(Operand(scratch), Immediate(new_space_allocation_top));
    mov(result, Operand(scratch, 0));
  }
}


void MacroAssembler::UpdateAllocationTopHelper(Register result_end,
                                               Register scratch) {
  if (FLAG_debug_code) {
    test(result_end, Immediate(kObjectAlignmentMask));
    Check(zero, "Unaligned allocation in new space");
  }

  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();

  // Update new top. Use scratch if available.
  if (scratch.is(no_reg)) {
    mov(Operand::StaticVariable(new_space_allocation_top), result_end);
  } else {
    mov(Operand(scratch, 0), result_end);
  }
}


void MacroAssembler::AllocateInNewSpace(int object_size,
                                        Register result,
                                        Register result_end,
                                        Register scratch,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (FLAG_debug_code) {
      // Trash the registers to simulate an allocation failure.
      mov(result, Immediate(0x7091));
      if (result_end.is_valid()) {
        mov(result_end, Immediate(0x7191));
      }
      if (scratch.is_valid()) {
        mov(scratch, Immediate(0x7291));
      }
    }
    jmp(gc_required);
    return;
  }
  ASSERT(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, result_end, scratch, flags);

  Register top_reg = result_end.is_valid() ? result_end : result;

  // Calculate new top and bail out if new space is exhausted.
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address();

  if (top_reg.is(result)) {
    add(Operand(top_reg), Immediate(object_size));
  } else {
    lea(top_reg, Operand(result, object_size));
  }
  cmp(top_reg, Operand::StaticVariable(new_space_allocation_limit));
  j(above, gc_required, not_taken);

  // Update allocation top.
  UpdateAllocationTopHelper(top_reg, scratch);

  // Tag result if requested.
  if (top_reg.is(result)) {
    if ((flags & TAG_OBJECT) != 0) {
      sub(Operand(result), Immediate(object_size - kHeapObjectTag));
    } else {
      sub(Operand(result), Immediate(object_size));
    }
  } else if ((flags & TAG_OBJECT) != 0) {
    add(Operand(result), Immediate(kHeapObjectTag));
  }
}


void MacroAssembler::AllocateInNewSpace(int header_size,
                                        ScaleFactor element_size,
                                        Register element_count,
                                        Register result,
                                        Register result_end,
                                        Register scratch,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (FLAG_debug_code) {
      // Trash the registers to simulate an allocation failure.
      mov(result, Immediate(0x7091));
      mov(result_end, Immediate(0x7191));
      if (scratch.is_valid()) {
        mov(scratch, Immediate(0x7291));
      }
      // Register element_count is not modified by the function.
    }
    jmp(gc_required);
    return;
  }
  ASSERT(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, result_end, scratch, flags);

  // Calculate new top and bail out if new space is exhausted.
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address();
  lea(result_end, Operand(result, element_count, element_size, header_size));
  cmp(result_end, Operand::StaticVariable(new_space_allocation_limit));
  j(above, gc_required);

  // Tag result if requested.
  if ((flags & TAG_OBJECT) != 0) {
    lea(result, Operand(result, kHeapObjectTag));
  }

  // Update allocation top.
  UpdateAllocationTopHelper(result_end, scratch);
}


void MacroAssembler::AllocateInNewSpace(Register object_size,
                                        Register result,
                                        Register result_end,
                                        Register scratch,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (FLAG_debug_code) {
      // Trash the registers to simulate an allocation failure.
      mov(result, Immediate(0x7091));
      mov(result_end, Immediate(0x7191));
      if (scratch.is_valid()) {
        mov(scratch, Immediate(0x7291));
      }
      // object_size is left unchanged by this function.
    }
    jmp(gc_required);
    return;
  }
  ASSERT(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, result_end, scratch, flags);

  // Calculate new top and bail out if new space is exhausted.
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address();
  if (!object_size.is(result_end)) {
    mov(result_end, object_size);
  }
  add(result_end, Operand(result));
  cmp(result_end, Operand::StaticVariable(new_space_allocation_limit));
  j(above, gc_required, not_taken);

  // Tag result if requested.
  if ((flags & TAG_OBJECT) != 0) {
    lea(result, Operand(result, kHeapObjectTag));
  }

  // Update allocation top.
  UpdateAllocationTopHelper(result_end, scratch);
}


void MacroAssembler::UndoAllocationInNewSpace(Register object) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();

  // Make sure the object has no tag before resetting top.
  and_(Operand(object), Immediate(~kHeapObjectTagMask));
#ifdef DEBUG
  cmp(object, Operand::StaticVariable(new_space_allocation_top));
  Check(below, "Undo allocation of non allocated memory");
#endif
  mov(Operand::StaticVariable(new_space_allocation_top), object);
}


void MacroAssembler::AllocateHeapNumber(Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required) {
  // Allocate heap number in new space.
  AllocateInNewSpace(HeapNumber::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);

  // Set the map.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(Factory::heap_number_map()));
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
  ASSERT(kShortSize == 2);
  // scratch1 = length * 2 + kObjectAlignmentMask.
  lea(scratch1, Operand(length, length, times_1, kObjectAlignmentMask));
  and_(Operand(scratch1), Immediate(~kObjectAlignmentMask));

  // Allocate two byte string in new space.
  AllocateInNewSpace(SeqTwoByteString::kHeaderSize,
                     times_1,
                     scratch1,
                     result,
                     scratch2,
                     scratch3,
                     gc_required,
                     TAG_OBJECT);

  // Set the map, length and hash field.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(Factory::string_map()));
  mov(scratch1, length);
  SmiTag(scratch1);
  mov(FieldOperand(result, String::kLengthOffset), scratch1);
  mov(FieldOperand(result, String::kHashFieldOffset),
      Immediate(String::kEmptyHashField));
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
  mov(scratch1, length);
  ASSERT(kCharSize == 1);
  add(Operand(scratch1), Immediate(kObjectAlignmentMask));
  and_(Operand(scratch1), Immediate(~kObjectAlignmentMask));

  // Allocate ascii string in new space.
  AllocateInNewSpace(SeqAsciiString::kHeaderSize,
                     times_1,
                     scratch1,
                     result,
                     scratch2,
                     scratch3,
                     gc_required,
                     TAG_OBJECT);

  // Set the map, length and hash field.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(Factory::ascii_string_map()));
  mov(scratch1, length);
  SmiTag(scratch1);
  mov(FieldOperand(result, String::kLengthOffset), scratch1);
  mov(FieldOperand(result, String::kHashFieldOffset),
      Immediate(String::kEmptyHashField));
}


void MacroAssembler::AllocateAsciiString(Register result,
                                         int length,
                                         Register scratch1,
                                         Register scratch2,
                                         Label* gc_required) {
  ASSERT(length > 0);

  // Allocate ascii string in new space.
  AllocateInNewSpace(SeqAsciiString::SizeFor(length),
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);

  // Set the map, length and hash field.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(Factory::ascii_string_map()));
  mov(FieldOperand(result, String::kLengthOffset),
      Immediate(Smi::FromInt(length)));
  mov(FieldOperand(result, String::kHashFieldOffset),
      Immediate(String::kEmptyHashField));
}


void MacroAssembler::AllocateConsString(Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required) {
  // Allocate heap number in new space.
  AllocateInNewSpace(ConsString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(Factory::cons_string_map()));
}


void MacroAssembler::AllocateAsciiConsString(Register result,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* gc_required) {
  // Allocate heap number in new space.
  AllocateInNewSpace(ConsString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(Factory::cons_ascii_string_map()));
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
  ASSERT(allow_stub_calls());  // Calls are not allowed in some stubs.
  call(stub->GetCode(), RelocInfo::CODE_TARGET);
}


MaybeObject* MacroAssembler::TryCallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls());  // Calls are not allowed in some stubs.
  Object* result;
  { MaybeObject* maybe_result = stub->TryGetCode();
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  call(Handle<Code>(Code::cast(result)), RelocInfo::CODE_TARGET);
  return result;
}


void MacroAssembler::TailCallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls());  // Calls are not allowed in some stubs.
  jmp(stub->GetCode(), RelocInfo::CODE_TARGET);
}


MaybeObject* MacroAssembler::TryTailCallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls());  // Calls are not allowed in some stubs.
  Object* result;
  { MaybeObject* maybe_result = stub->TryGetCode();
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  jmp(Handle<Code>(Code::cast(result)), RelocInfo::CODE_TARGET);
  return result;
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


void MacroAssembler::IndexFromHash(Register hash, Register index) {
  // The assert checks that the constants for the maximum number of digits
  // for an array index cached in the hash field and the number of bits
  // reserved for it does not conflict.
  ASSERT(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  // We want the smi-tagged index in key.  kArrayIndexValueMask has zeros in
  // the low kHashShift bits.
  and_(hash, String::kArrayIndexValueMask);
  STATIC_ASSERT(String::kHashShift >= kSmiTagSize && kSmiTag == 0);
  if (String::kHashShift > kSmiTagSize) {
    shr(hash, String::kHashShift - kSmiTagSize);
  }
  if (!index.is(hash)) {
    mov(index, hash);
  }
}


void MacroAssembler::CallRuntime(Runtime::FunctionId id, int num_arguments) {
  CallRuntime(Runtime::FunctionForId(id), num_arguments);
}


MaybeObject* MacroAssembler::TryCallRuntime(Runtime::FunctionId id,
                                            int num_arguments) {
  return TryCallRuntime(Runtime::FunctionForId(id), num_arguments);
}


void MacroAssembler::CallRuntime(Runtime::Function* f, int num_arguments) {
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
  Set(eax, Immediate(num_arguments));
  mov(ebx, Immediate(ExternalReference(f)));
  CEntryStub ces(1);
  CallStub(&ces);
}


MaybeObject* MacroAssembler::TryCallRuntime(Runtime::Function* f,
                                            int num_arguments) {
  if (f->nargs >= 0 && f->nargs != num_arguments) {
    IllegalOperation(num_arguments);
    // Since we did not call the stub, there was no allocation failure.
    // Return some non-failure object.
    return Heap::undefined_value();
  }

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Set(eax, Immediate(num_arguments));
  mov(ebx, Immediate(ExternalReference(f)));
  CEntryStub ces(1);
  return TryCallStub(&ces);
}


void MacroAssembler::CallExternalReference(ExternalReference ref,
                                           int num_arguments) {
  mov(eax, Immediate(num_arguments));
  mov(ebx, Immediate(ref));

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
  Set(eax, Immediate(num_arguments));
  JumpToExternalReference(ext);
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid,
                                     int num_arguments,
                                     int result_size) {
  TailCallExternalReference(ExternalReference(fid), num_arguments, result_size);
}


// If true, a Handle<T> passed by value is passed and returned by
// using the location_ field directly.  If false, it is passed and
// returned as a pointer to a handle.
#ifdef USING_BSD_ABI
static const bool kPassHandlesDirectly = true;
#else
static const bool kPassHandlesDirectly = false;
#endif


Operand ApiParameterOperand(int index) {
  return Operand(esp, (index + (kPassHandlesDirectly ? 0 : 1)) * kPointerSize);
}


void MacroAssembler::PrepareCallApiFunction(int stack_space, int argc) {
  if (kPassHandlesDirectly) {
    EnterApiExitFrame(stack_space, argc);
    // When handles as passed directly we don't have to allocate extra
    // space for and pass an out parameter.
  } else {
    // We allocate two additional slots: return value and pointer to it.
    EnterApiExitFrame(stack_space, argc + 2);
  }
}


void MacroAssembler::CallApiFunctionAndReturn(ApiFunction* function, int argc) {
  if (!kPassHandlesDirectly) {
    // The argument slots are filled as follows:
    //
    //   n + 1: output cell
    //   n: arg n
    //   ...
    //   1: arg1
    //   0: pointer to the output cell
    //
    // Note that this is one more "argument" than the function expects
    // so the out cell will have to be popped explicitly after returning
    // from the function. The out cell contains Handle.
    lea(eax, Operand(esp, (argc + 1) * kPointerSize));  // pointer to out cell.
    mov(Operand(esp, 0 * kPointerSize), eax);  // output.
    mov(Operand(esp, (argc + 1) * kPointerSize), Immediate(0));  // out cell.
  }

  ExternalReference next_address =
      ExternalReference::handle_scope_next_address();
  ExternalReference limit_address =
      ExternalReference::handle_scope_limit_address();
  ExternalReference level_address =
      ExternalReference::handle_scope_level_address();

  // Allocate HandleScope in callee-save registers.
  mov(ebx, Operand::StaticVariable(next_address));
  mov(edi, Operand::StaticVariable(limit_address));
  add(Operand::StaticVariable(level_address), Immediate(1));

  // Call the api function!
  call(function->address(), RelocInfo::RUNTIME_ENTRY);

  if (!kPassHandlesDirectly) {
    // The returned value is a pointer to the handle holding the result.
    // Dereference this to get to the location.
    mov(eax, Operand(eax, 0));
  }

  Label empty_handle;
  Label prologue;
  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;

  // Check if the result handle holds 0.
  test(eax, Operand(eax));
  j(zero, &empty_handle, not_taken);
  // It was non-zero.  Dereference to get the result value.
  mov(eax, Operand(eax, 0));
  bind(&prologue);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  mov(Operand::StaticVariable(next_address), ebx);
  sub(Operand::StaticVariable(level_address), Immediate(1));
  Assert(above_equal, "Invalid HandleScope level");
  cmp(edi, Operand::StaticVariable(limit_address));
  j(not_equal, &delete_allocated_handles, not_taken);
  bind(&leave_exit_frame);

  // Check if the function scheduled an exception.
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address();
  cmp(Operand::StaticVariable(scheduled_exception_address),
         Immediate(Factory::the_hole_value()));
  j(not_equal, &promote_scheduled_exception, not_taken);
  LeaveExitFrame();
  ret(0);
  bind(&promote_scheduled_exception);
  TailCallRuntime(Runtime::kPromoteScheduledException, 0, 1);
  bind(&empty_handle);
  // It was zero; the result is undefined.
  mov(eax, Factory::undefined_value());
  jmp(&prologue);

  // HandleScope limit has changed. Delete allocated extensions.
  bind(&delete_allocated_handles);
  mov(Operand::StaticVariable(limit_address), edi);
  mov(edi, eax);
  mov(eax, Immediate(ExternalReference::delete_handle_scope_extensions()));
  call(Operand(eax));
  mov(eax, edi);
  jmp(&leave_exit_frame);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& ext) {
  // Set the entry point and jump to the C entry runtime stub.
  mov(ebx, Immediate(ext));
  CEntryStub ces(1);
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
  SmiUntag(ebx);

  ParameterCount expected(ebx);
  InvokeCode(FieldOperand(edi, JSFunction::kCodeEntryOffset),
             expected, actual, flag);
}


void MacroAssembler::InvokeFunction(JSFunction* function,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  ASSERT(function->is_compiled());
  // Get the function and setup the context.
  mov(edi, Immediate(Handle<JSFunction>(function)));
  mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  // Invoke the cached code.
  Handle<Code> code(function->code());
  ParameterCount expected(function->shared()->formal_parameter_count());
  InvokeCode(code, expected, actual, RelocInfo::CODE_TARGET, flag);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id, InvokeFlag flag) {
  // Calls are not allowed in some stubs.
  ASSERT(flag == JUMP_FUNCTION || allow_stub_calls());

  // Rely on the assertion to check that the number of provided
  // arguments match the expected number of arguments. Fake a
  // parameter count to avoid emitting code to do the check.
  ParameterCount expected(0);
  GetBuiltinFunction(edi, id);
  InvokeCode(FieldOperand(edi, JSFunction::kCodeEntryOffset),
           expected, expected, flag);
}

void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the JavaScript builtin function from the builtins object.
  mov(target, Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  mov(target, FieldOperand(target, GlobalObject::kBuiltinsOffset));
  mov(target, FieldOperand(target,
                           JSBuiltinsObject::OffsetOfFunctionWithId(id)));
}

void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  ASSERT(!target.is(edi));
  // Load the JavaScript builtin function from the builtins object.
  GetBuiltinFunction(edi, id);
  // Load the code entry point from the function into the target register.
  mov(target, FieldOperand(edi, JSFunction::kCodeEntryOffset));
}


void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    mov(dst, Operand(esi, Context::SlotOffset(Context::CLOSURE_INDEX)));
    // Load the function context (which is the incoming, outer context).
    mov(dst, FieldOperand(dst, JSFunction::kContextOffset));
    for (int i = 1; i < context_chain_length; i++) {
      mov(dst, Operand(dst, Context::SlotOffset(Context::CLOSURE_INDEX)));
      mov(dst, FieldOperand(dst, JSFunction::kContextOffset));
    }
    // The context may be an intermediate context, not a function context.
    mov(dst, Operand(dst, Context::SlotOffset(Context::FCONTEXT_INDEX)));
  } else {  // Slot is in the current function context.
    // The context may be an intermediate context, not a function context.
    mov(dst, Operand(esi, Context::SlotOffset(Context::FCONTEXT_INDEX)));
  }
}


void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the global or builtins object from the current context.
  mov(function, Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  // Load the global context from the global or builtins object.
  mov(function, FieldOperand(function, GlobalObject::kGlobalContextOffset));
  // Load the function from the global context.
  mov(function, Operand(function, Context::SlotOffset(index)));
}


void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map) {
  // Load the initial map.  The global functions all have initial maps.
  mov(map, FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  if (FLAG_debug_code) {
    Label ok, fail;
    CheckMap(map, Factory::meta_map(), &fail, false);
    jmp(&ok);
    bind(&fail);
    Abort("Global functions must have initial map");
    bind(&ok);
  }
}


void MacroAssembler::Ret() {
  ret(0);
}


void MacroAssembler::Drop(int stack_elements) {
  if (stack_elements > 0) {
    add(Operand(esp), Immediate(stack_elements * kPointerSize));
  }
}


void MacroAssembler::Move(Register dst, Register src) {
  if (!dst.is(src)) {
    mov(dst, src);
  }
}


void MacroAssembler::Move(Register dst, Handle<Object> value) {
  mov(dst, value);
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


void MacroAssembler::IncrementCounter(Condition cc,
                                      StatsCounter* counter,
                                      int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Label skip;
    j(NegateCondition(cc), &skip);
    pushfd();
    IncrementCounter(counter, value);
    popfd();
    bind(&skip);
  }
}


void MacroAssembler::DecrementCounter(Condition cc,
                                      StatsCounter* counter,
                                      int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Label skip;
    j(NegateCondition(cc), &skip);
    pushfd();
    DecrementCounter(counter, value);
    popfd();
    bind(&skip);
  }
}


void MacroAssembler::Assert(Condition cc, const char* msg) {
  if (FLAG_debug_code) Check(cc, msg);
}


void MacroAssembler::AssertFastElements(Register elements) {
  if (FLAG_debug_code) {
    Label ok;
    cmp(FieldOperand(elements, HeapObject::kMapOffset),
        Immediate(Factory::fixed_array_map()));
    j(equal, &ok);
    cmp(FieldOperand(elements, HeapObject::kMapOffset),
        Immediate(Factory::fixed_cow_array_map()));
    j(equal, &ok);
    Abort("JSObject with fast elements map has slow elements");
    bind(&ok);
  }
}


void MacroAssembler::Check(Condition cc, const char* msg) {
  Label L;
  j(cc, &L, taken);
  Abort(msg);
  // will not return here
  bind(&L);
}


void MacroAssembler::CheckStackAlignment() {
  int frame_alignment = OS::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (frame_alignment > kPointerSize) {
    ASSERT(IsPowerOf2(frame_alignment));
    Label alignment_as_expected;
    test(esp, Immediate(frame_alignment_mask));
    j(zero, &alignment_as_expected);
    // Abort if stack is not aligned.
    int3();
    bind(&alignment_as_expected);
  }
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
  // Disable stub call restrictions to always allow calls to abort.
  set_allow_stub_calls(true);

  push(eax);
  push(Immediate(p0));
  push(Immediate(reinterpret_cast<intptr_t>(Smi::FromInt(p1 - p0))));
  CallRuntime(Runtime::kAbort, 2);
  // will not return here
  int3();
}


void MacroAssembler::JumpIfNotNumber(Register reg,
                                     TypeInfo info,
                                     Label* on_not_number) {
  if (FLAG_debug_code) AbortIfSmi(reg);
  if (!info.IsNumber()) {
    cmp(FieldOperand(reg, HeapObject::kMapOffset),
        Factory::heap_number_map());
    j(not_equal, on_not_number);
  }
}


void MacroAssembler::ConvertToInt32(Register dst,
                                    Register source,
                                    Register scratch,
                                    TypeInfo info,
                                    Label* on_not_int32) {
  if (FLAG_debug_code) {
    AbortIfSmi(source);
    AbortIfNotNumber(source);
  }
  if (info.IsInteger32()) {
    cvttsd2si(dst, FieldOperand(source, HeapNumber::kValueOffset));
  } else {
    Label done;
    bool push_pop = (scratch.is(no_reg) && dst.is(source));
    ASSERT(!scratch.is(source));
    if (push_pop) {
      push(dst);
      scratch = dst;
    }
    if (scratch.is(no_reg)) scratch = dst;
    cvttsd2si(scratch, FieldOperand(source, HeapNumber::kValueOffset));
    cmp(scratch, 0x80000000u);
    if (push_pop) {
      j(not_equal, &done);
      pop(dst);
      jmp(on_not_int32);
    } else {
      j(equal, on_not_int32);
    }

    bind(&done);
    if (push_pop) {
      add(Operand(esp), Immediate(kPointerSize));  // Pop.
    }
    if (!scratch.is(dst)) {
      mov(dst, scratch);
    }
  }
}


void MacroAssembler::LoadPowerOf2(XMMRegister dst,
                                  Register scratch,
                                  int power) {
  ASSERT(is_uintn(power + HeapNumber::kExponentBias,
                  HeapNumber::kExponentBits));
  mov(scratch, Immediate(power + HeapNumber::kExponentBias));
  movd(dst, Operand(scratch));
  psllq(dst, HeapNumber::kMantissaBits);
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialAscii(
    Register instance_type,
    Register scratch,
    Label* failure) {
  if (!scratch.is(instance_type)) {
    mov(scratch, instance_type);
  }
  and_(scratch,
       kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask);
  cmp(scratch, kStringTag | kSeqStringTag | kAsciiStringTag);
  j(not_equal, failure);
}


void MacroAssembler::JumpIfNotBothSequentialAsciiStrings(Register object1,
                                                         Register object2,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         Label* failure) {
  // Check that both objects are not smis.
  ASSERT_EQ(0, kSmiTag);
  mov(scratch1, Operand(object1));
  and_(scratch1, Operand(object2));
  test(scratch1, Immediate(kSmiTagMask));
  j(zero, failure);

  // Load instance type for both strings.
  mov(scratch1, FieldOperand(object1, HeapObject::kMapOffset));
  mov(scratch2, FieldOperand(object2, HeapObject::kMapOffset));
  movzx_b(scratch1, FieldOperand(scratch1, Map::kInstanceTypeOffset));
  movzx_b(scratch2, FieldOperand(scratch2, Map::kInstanceTypeOffset));

  // Check that both are flat ascii strings.
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;
  const int kFlatAsciiStringTag = ASCII_STRING_TYPE;
  // Interleave bits from both instance types and compare them in one check.
  ASSERT_EQ(0, kFlatAsciiStringMask & (kFlatAsciiStringMask << 3));
  and_(scratch1, kFlatAsciiStringMask);
  and_(scratch2, kFlatAsciiStringMask);
  lea(scratch1, Operand(scratch1, scratch2, times_8, 0));
  cmp(scratch1, kFlatAsciiStringTag | (kFlatAsciiStringTag << 3));
  j(not_equal, failure);
}


void MacroAssembler::PrepareCallCFunction(int num_arguments, Register scratch) {
  int frameAlignment = OS::ActivationFrameAlignment();
  if (frameAlignment != 0) {
    // Make stack end at alignment and make room for num_arguments words
    // and the original value of esp.
    mov(scratch, esp);
    sub(Operand(esp), Immediate((num_arguments + 1) * kPointerSize));
    ASSERT(IsPowerOf2(frameAlignment));
    and_(esp, -frameAlignment);
    mov(Operand(esp, num_arguments * kPointerSize), scratch);
  } else {
    sub(Operand(esp), Immediate(num_arguments * kPointerSize));
  }
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  // Trashing eax is ok as it will be the return value.
  mov(Operand(eax), Immediate(function));
  CallCFunction(eax, num_arguments);
}


void MacroAssembler::CallCFunction(Register function,
                                   int num_arguments) {
  // Check stack alignment.
  if (FLAG_debug_code) {
    CheckStackAlignment();
  }

  call(Operand(function));
  if (OS::ActivationFrameAlignment() != 0) {
    mov(esp, Operand(esp, num_arguments * kPointerSize));
  } else {
    add(Operand(esp), Immediate(num_arguments * sizeof(int32_t)));
  }
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

#endif  // V8_TARGET_ARCH_IA32

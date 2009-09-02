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

#include "codegen-inl.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


void Builtins::Generate_Adaptor(MacroAssembler* masm, CFunctionId id) {
  // TODO(428): Don't pass the function in a static variable.
  ExternalReference passed = ExternalReference::builtin_passed_function();
  __ mov(Operand::StaticVariable(passed), edi);

  // The actual argument count has already been loaded into register
  // eax, but JumpToBuiltin expects eax to contain the number of
  // arguments including the receiver.
  __ inc(eax);
  __ JumpToBuiltin(ExternalReference(id));
}


void Builtins::Generate_JSConstructCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax: number of arguments
  //  -- edi: constructor function
  // -----------------------------------

  Label non_function_call;
  // Check that function is not a smi.
  __ test(edi, Immediate(kSmiTagMask));
  __ j(zero, &non_function_call);
  // Check that function is a JSFunction.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &non_function_call);

  // Jump to the function-specific construct stub.
  __ mov(ebx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ebx, FieldOperand(ebx, SharedFunctionInfo::kConstructStubOffset));
  __ lea(ebx, FieldOperand(ebx, Code::kHeaderSize));
  __ jmp(Operand(ebx));

  // edi: called object
  // eax: number of arguments
  __ bind(&non_function_call);

  // Set expected number of arguments to zero (not changing eax).
  __ Set(ebx, Immediate(0));
  __ GetBuiltinEntry(edx, Builtins::CALL_NON_FUNCTION_AS_CONSTRUCTOR);
  __ jmp(Handle<Code>(builtin(ArgumentsAdaptorTrampoline)),
         RelocInfo::CODE_TARGET);
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // Enter a construct frame.
  __ EnterConstructFrame();

  // Store a smi-tagged arguments count on the stack.
  __ shl(eax, kSmiTagSize);
  __ push(eax);

  // Push the function to invoke on the stack.
  __ push(edi);

  // Try to allocate the object without transitioning into C code. If any of the
  // preconditions is not met, the code bails out to the runtime call.
  Label rt_call, allocated;
  if (FLAG_inline_new) {
    Label undo_allocation;
#ifdef ENABLE_DEBUGGER_SUPPORT
    ExternalReference debug_step_in_fp =
        ExternalReference::debug_step_in_fp_address();
    __ cmp(Operand::StaticVariable(debug_step_in_fp), Immediate(0));
    __ j(not_equal, &rt_call);
#endif

    // Verified that the constructor is a JSFunction.
    // Load the initial map and verify that it is in fact a map.
    // edi: constructor
    __ mov(eax, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi
    __ test(eax, Immediate(kSmiTagMask));
    __ j(zero, &rt_call);
    // edi: constructor
    // eax: initial map (if proven valid below)
    __ CmpObjectType(eax, MAP_TYPE, ebx);
    __ j(not_equal, &rt_call);

    // Check that the constructor is not constructing a JSFunction (see comments
    // in Runtime_NewObject in runtime.cc). In which case the initial map's
    // instance type would be JS_FUNCTION_TYPE.
    // edi: constructor
    // eax: initial map
    __ CmpInstanceType(eax, JS_FUNCTION_TYPE);
    __ j(equal, &rt_call);

    // Now allocate the JSObject on the heap.
    // edi: constructor
    // eax: initial map
    __ movzx_b(edi, FieldOperand(eax, Map::kInstanceSizeOffset));
    __ shl(edi, kPointerSizeLog2);
    // Make sure that the maximum heap object size will never cause us
    // problem here, because it is always greater than the maximum
    // instance size that can be represented in a byte.
    ASSERT(Heap::MaxObjectSizeInPagedSpace() >= JSObject::kMaxInstanceSize);
    __ AllocateObjectInNewSpace(edi, ebx, edi, no_reg, &rt_call, false);
    // Allocated the JSObject, now initialize the fields.
    // eax: initial map
    // ebx: JSObject
    // edi: start of next object
    __ mov(Operand(ebx, JSObject::kMapOffset), eax);
    __ mov(ecx, Factory::empty_fixed_array());
    __ mov(Operand(ebx, JSObject::kPropertiesOffset), ecx);
    __ mov(Operand(ebx, JSObject::kElementsOffset), ecx);
    // Set extra fields in the newly allocated object.
    // eax: initial map
    // ebx: JSObject
    // edi: start of next object
    { Label loop, entry;
      __ mov(edx, Factory::undefined_value());
      __ lea(ecx, Operand(ebx, JSObject::kHeaderSize));
      __ jmp(&entry);
      __ bind(&loop);
      __ mov(Operand(ecx, 0), edx);
      __ add(Operand(ecx), Immediate(kPointerSize));
      __ bind(&entry);
      __ cmp(ecx, Operand(edi));
      __ j(less, &loop);
    }

    // Add the object tag to make the JSObject real, so that we can continue and
    // jump into the continuation code at any time from now on. Any failures
    // need to undo the allocation, so that the heap is in a consistent state
    // and verifiable.
    // eax: initial map
    // ebx: JSObject
    // edi: start of next object
    __ or_(Operand(ebx), Immediate(kHeapObjectTag));

    // Check if a non-empty properties array is needed.
    // Allocate and initialize a FixedArray if it is.
    // eax: initial map
    // ebx: JSObject
    // edi: start of next object
    // Calculate the total number of properties described by the map.
    __ movzx_b(edx, FieldOperand(eax, Map::kUnusedPropertyFieldsOffset));
    __ movzx_b(ecx, FieldOperand(eax, Map::kPreAllocatedPropertyFieldsOffset));
    __ add(edx, Operand(ecx));
    // Calculate unused properties past the end of the in-object properties.
    __ movzx_b(ecx, FieldOperand(eax, Map::kInObjectPropertiesOffset));
    __ sub(edx, Operand(ecx));
    // Done if no extra properties are to be allocated.
    __ j(zero, &allocated);
    __ Assert(positive, "Property allocation count failed.");

    // Scale the number of elements by pointer size and add the header for
    // FixedArrays to the start of the next object calculation from above.
    // ebx: JSObject
    // edi: start of next object (will be start of FixedArray)
    // edx: number of elements in properties array
    ASSERT(Heap::MaxObjectSizeInPagedSpace() >
           (FixedArray::kHeaderSize + 255*kPointerSize));
    __ AllocateObjectInNewSpace(FixedArray::kHeaderSize,
                                times_pointer_size,
                                edx,
                                edi,
                                ecx,
                                no_reg,
                                &undo_allocation,
                                true);

    // Initialize the FixedArray.
    // ebx: JSObject
    // edi: FixedArray
    // edx: number of elements
    // ecx: start of next object
    __ mov(eax, Factory::fixed_array_map());
    __ mov(Operand(edi, JSObject::kMapOffset), eax);  // setup the map
    __ mov(Operand(edi, Array::kLengthOffset), edx);  // and length

    // Initialize the fields to undefined.
    // ebx: JSObject
    // edi: FixedArray
    // ecx: start of next object
    { Label loop, entry;
      __ mov(edx, Factory::undefined_value());
      __ lea(eax, Operand(edi, FixedArray::kHeaderSize));
      __ jmp(&entry);
      __ bind(&loop);
      __ mov(Operand(eax, 0), edx);
      __ add(Operand(eax), Immediate(kPointerSize));
      __ bind(&entry);
      __ cmp(eax, Operand(ecx));
      __ j(below, &loop);
    }

    // Store the initialized FixedArray into the properties field of
    // the JSObject
    // ebx: JSObject
    // edi: FixedArray
    __ or_(Operand(edi), Immediate(kHeapObjectTag));  // add the heap tag
    __ mov(FieldOperand(ebx, JSObject::kPropertiesOffset), edi);


    // Continue with JSObject being successfully allocated
    // ebx: JSObject
    __ jmp(&allocated);

    // Undo the setting of the new top so that the heap is verifiable. For
    // example, the map's unused properties potentially do not match the
    // allocated objects unused properties.
    // ebx: JSObject (previous new top)
    __ bind(&undo_allocation);
    __ UndoAllocationInNewSpace(ebx);
  }

  // Allocate the new receiver object using the runtime call.
  // edi: function (constructor)
  __ bind(&rt_call);
  // Must restore edi (constructor) before calling runtime.
  __ mov(edi, Operand(esp, 0));
  __ push(edi);
  __ CallRuntime(Runtime::kNewObject, 1);
  __ mov(ebx, Operand(eax));  // store result in ebx

  // New object allocated.
  // ebx: newly allocated object
  __ bind(&allocated);
  // Retrieve the function from the stack.
  __ pop(edi);

  // Retrieve smi-tagged arguments count from the stack.
  __ mov(eax, Operand(esp, 0));
  __ shr(eax, kSmiTagSize);

  // Push the allocated receiver to the stack. We need two copies
  // because we may have to return the original one and the calling
  // conventions dictate that the called function pops the receiver.
  __ push(ebx);
  __ push(ebx);

  // Setup pointer to last argument.
  __ lea(ebx, Operand(ebp, StandardFrameConstants::kCallerSPOffset));

  // Copy arguments and receiver to the expression stack.
  Label loop, entry;
  __ mov(ecx, Operand(eax));
  __ jmp(&entry);
  __ bind(&loop);
  __ push(Operand(ebx, ecx, times_4, 0));
  __ bind(&entry);
  __ dec(ecx);
  __ j(greater_equal, &loop);

  // Call the function.
  ParameterCount actual(eax);
  __ InvokeFunction(edi, actual, CALL_FUNCTION);

  // Restore context from the frame.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));

  // If the result is an object (in the ECMA sense), we should get rid
  // of the receiver and use the result; see ECMA-262 section 13.2.2-7
  // on page 74.
  Label use_receiver, exit;

  // If the result is a smi, it is *not* an object in the ECMA sense.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &use_receiver, not_taken);

  // If the type of the result (stored in its map) is less than
  // FIRST_JS_OBJECT_TYPE, it is not an object in the ECMA sense.
  __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
  __ j(greater_equal, &exit, not_taken);

  // Throw away the result of the constructor invocation and use the
  // on-stack receiver as the result.
  __ bind(&use_receiver);
  __ mov(eax, Operand(esp, 0));

  // Restore the arguments count and leave the construct frame.
  __ bind(&exit);
  __ mov(ebx, Operand(esp, kPointerSize));  // get arguments count
  __ LeaveConstructFrame();

  // Remove caller arguments from the stack and return.
  ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ pop(ecx);
  __ lea(esp, Operand(esp, ebx, times_2, 1 * kPointerSize));  // 1 ~ receiver
  __ push(ecx);
  __ IncrementCounter(&Counters::constructed_objects, 1);
  __ ret(0);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Clear the context before we push it when entering the JS frame.
  __ xor_(esi, Operand(esi));  // clear esi

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Load the previous frame pointer (ebx) to access C arguments
  __ mov(ebx, Operand(ebp, 0));

  // Get the function from the frame and setup the context.
  __ mov(ecx, Operand(ebx, EntryFrameConstants::kFunctionArgOffset));
  __ mov(esi, FieldOperand(ecx, JSFunction::kContextOffset));

  // Push the function and the receiver onto the stack.
  __ push(ecx);
  __ push(Operand(ebx, EntryFrameConstants::kReceiverArgOffset));

  // Load the number of arguments and setup pointer to the arguments.
  __ mov(eax, Operand(ebx, EntryFrameConstants::kArgcOffset));
  __ mov(ebx, Operand(ebx, EntryFrameConstants::kArgvOffset));

  // Copy arguments to the stack in a loop.
  Label loop, entry;
  __ xor_(ecx, Operand(ecx));  // clear ecx
  __ jmp(&entry);
  __ bind(&loop);
  __ mov(edx, Operand(ebx, ecx, times_4, 0));  // push parameter from argv
  __ push(Operand(edx, 0));  // dereference handle
  __ inc(Operand(ecx));
  __ bind(&entry);
  __ cmp(ecx, Operand(eax));
  __ j(not_equal, &loop);

  // Get the function from the stack and call it.
  __ mov(edi, Operand(esp, eax, times_4, +1 * kPointerSize));  // +1 ~ receiver

  // Invoke the code.
  if (is_construct) {
    __ call(Handle<Code>(Builtins::builtin(Builtins::JSConstructCall)),
            RelocInfo::CODE_TARGET);
  } else {
    ParameterCount actual(eax);
    __ InvokeFunction(edi, actual, CALL_FUNCTION);
  }

  // Exit the JS frame. Notice that this also removes the empty
  // context and the function left on the stack by the code
  // invocation.
  __ LeaveInternalFrame();
  __ ret(1 * kPointerSize);  // remove receiver
}


void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}


void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  { Label done;
    __ test(eax, Operand(eax));
    __ j(not_zero, &done, taken);
    __ pop(ebx);
    __ push(Immediate(Factory::undefined_value()));
    __ push(ebx);
    __ inc(eax);
    __ bind(&done);
  }

  // 2. Get the function to call from the stack.
  { Label done, non_function, function;
    // +1 ~ return address.
    __ mov(edi, Operand(esp, eax, times_4, +1 * kPointerSize));
    __ test(edi, Immediate(kSmiTagMask));
    __ j(zero, &non_function, not_taken);
    __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
    __ j(equal, &function, taken);

    // Non-function called: Clear the function to force exception.
    __ bind(&non_function);
    __ xor_(edi, Operand(edi));
    __ jmp(&done);

    // Function called: Change context eagerly to get the right global object.
    __ bind(&function);
    __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

    __ bind(&done);
  }

  // 3. Make sure first argument is an object; convert if necessary.
  { Label call_to_object, use_global_receiver, patch_receiver, done;
    __ mov(ebx, Operand(esp, eax, times_4, 0));

    __ test(ebx, Immediate(kSmiTagMask));
    __ j(zero, &call_to_object);

    __ cmp(ebx, Factory::null_value());
    __ j(equal, &use_global_receiver);
    __ cmp(ebx, Factory::undefined_value());
    __ j(equal, &use_global_receiver);

    __ mov(ecx, FieldOperand(ebx, HeapObject::kMapOffset));
    __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
    __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
    __ j(less, &call_to_object);
    __ cmp(ecx, LAST_JS_OBJECT_TYPE);
    __ j(less_equal, &done);

    __ bind(&call_to_object);
    __ EnterInternalFrame();  // preserves eax, ebx, edi

    // Store the arguments count on the stack (smi tagged).
    ASSERT(kSmiTag == 0);
    __ shl(eax, kSmiTagSize);
    __ push(eax);

    __ push(edi);  // save edi across the call
    __ push(ebx);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ mov(ebx, eax);
    __ pop(edi);  // restore edi after the call

    // Get the arguments count and untag it.
    __ pop(eax);
    __ shr(eax, kSmiTagSize);

    __ LeaveInternalFrame();
    __ jmp(&patch_receiver);

    // Use the global receiver object from the called function as the receiver.
    __ bind(&use_global_receiver);
    const int kGlobalIndex =
        Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
    __ mov(ebx, FieldOperand(esi, kGlobalIndex));
    __ mov(ebx, FieldOperand(ebx, GlobalObject::kGlobalReceiverOffset));

    __ bind(&patch_receiver);
    __ mov(Operand(esp, eax, times_4, 0), ebx);

    __ bind(&done);
  }

  // 4. Shift stuff one slot down the stack.
  { Label loop;
    __ lea(ecx, Operand(eax, +1));  // +1 ~ copy receiver too
    __ bind(&loop);
    __ mov(ebx, Operand(esp, ecx, times_4, 0));
    __ mov(Operand(esp, ecx, times_4, kPointerSize), ebx);
    __ dec(ecx);
    __ j(not_zero, &loop);
  }

  // 5. Remove TOS (copy of last arguments), but keep return address.
  __ pop(ebx);
  __ pop(ecx);
  __ push(ebx);
  __ dec(eax);

  // 6. Check that function really was a function and get the code to
  //    call from the function and check that the number of expected
  //    arguments matches what we're providing.
  { Label invoke;
    __ test(edi, Operand(edi));
    __ j(not_zero, &invoke, taken);
    __ xor_(ebx, Operand(ebx));
    __ GetBuiltinEntry(edx, Builtins::CALL_NON_FUNCTION);
    __ jmp(Handle<Code>(builtin(ArgumentsAdaptorTrampoline)),
           RelocInfo::CODE_TARGET);

    __ bind(&invoke);
    __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ mov(ebx,
           FieldOperand(edx, SharedFunctionInfo::kFormalParameterCountOffset));
    __ mov(edx, FieldOperand(edx, SharedFunctionInfo::kCodeOffset));
    __ lea(edx, FieldOperand(edx, Code::kHeaderSize));
    __ cmp(eax, Operand(ebx));
    __ j(not_equal, Handle<Code>(builtin(ArgumentsAdaptorTrampoline)));
  }

  // 7. Jump (tail-call) to the code in register edx without checking arguments.
  ParameterCount expected(0);
  __ InvokeCode(Operand(edx), expected, expected, JUMP_FUNCTION);
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  __ EnterInternalFrame();

  __ push(Operand(ebp, 4 * kPointerSize));  // push this
  __ push(Operand(ebp, 2 * kPointerSize));  // push arguments
  __ InvokeBuiltin(Builtins::APPLY_PREPARE, CALL_FUNCTION);

  if (FLAG_check_stack) {
    // We need to catch preemptions right here, otherwise an unlucky preemption
    // could show up as a failed apply.
    ExternalReference stack_guard_limit =
        ExternalReference::address_of_stack_guard_limit();
    Label retry_preemption;
    Label no_preemption;
    __ bind(&retry_preemption);
    __ mov(edi, Operand::StaticVariable(stack_guard_limit));
    __ cmp(esp, Operand(edi));
    __ j(above, &no_preemption, taken);

    // Preemption!
    // Because builtins always remove the receiver from the stack, we
    // have to fake one to avoid underflowing the stack.
    __ push(eax);
    __ push(Immediate(Smi::FromInt(0)));

    // Do call to runtime routine.
    __ CallRuntime(Runtime::kStackGuard, 1);
    __ pop(eax);
    __ jmp(&retry_preemption);

    __ bind(&no_preemption);

    Label okay;
    // Make ecx the space we have left.
    __ mov(ecx, Operand(esp));
    __ sub(ecx, Operand(edi));
    // Make edx the space we need for the array when it is unrolled onto the
    // stack.
    __ mov(edx, Operand(eax));
    __ shl(edx, kPointerSizeLog2 - kSmiTagSize);
    __ cmp(ecx, Operand(edx));
    __ j(greater, &okay, taken);

    // Too bad: Out of stack space.
    __ push(Operand(ebp, 4 * kPointerSize));  // push this
    __ push(eax);
    __ InvokeBuiltin(Builtins::APPLY_OVERFLOW, CALL_FUNCTION);
    __ bind(&okay);
  }

  // Push current index and limit.
  const int kLimitOffset =
      StandardFrameConstants::kExpressionsOffset - 1 * kPointerSize;
  const int kIndexOffset = kLimitOffset - 1 * kPointerSize;
  __ push(eax);  // limit
  __ push(Immediate(0));  // index

  // Change context eagerly to get the right global object if
  // necessary.
  __ mov(edi, Operand(ebp, 4 * kPointerSize));
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Compute the receiver.
  Label call_to_object, use_global_receiver, push_receiver;
  __ mov(ebx, Operand(ebp, 3 * kPointerSize));
  __ test(ebx, Immediate(kSmiTagMask));
  __ j(zero, &call_to_object);
  __ cmp(ebx, Factory::null_value());
  __ j(equal, &use_global_receiver);
  __ cmp(ebx, Factory::undefined_value());
  __ j(equal, &use_global_receiver);

  // If given receiver is already a JavaScript object then there's no
  // reason for converting it.
  __ mov(ecx, FieldOperand(ebx, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
  __ j(less, &call_to_object);
  __ cmp(ecx, LAST_JS_OBJECT_TYPE);
  __ j(less_equal, &push_receiver);

  // Convert the receiver to an object.
  __ bind(&call_to_object);
  __ push(ebx);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
  __ mov(ebx, Operand(eax));
  __ jmp(&push_receiver);

  // Use the current global receiver object as the receiver.
  __ bind(&use_global_receiver);
  const int kGlobalOffset =
      Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
  __ mov(ebx, FieldOperand(esi, kGlobalOffset));
  __ mov(ebx, FieldOperand(ebx, GlobalObject::kGlobalReceiverOffset));

  // Push the receiver.
  __ bind(&push_receiver);
  __ push(ebx);

  // Copy all arguments from the array to the stack.
  Label entry, loop;
  __ mov(eax, Operand(ebp, kIndexOffset));
  __ jmp(&entry);
  __ bind(&loop);
  __ mov(ecx, Operand(ebp, 2 * kPointerSize));  // load arguments
  __ push(ecx);
  __ push(eax);

  // Use inline caching to speed up access to arguments.
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  // It is important that we do not have a test instruction after the
  // call.  A test instruction after the call is used to indicate that
  // we have generated an inline version of the keyed load.  In this
  // case, we know that we are not generating a test instruction next.

  // Remove IC arguments from the stack and push the nth argument.
  __ add(Operand(esp), Immediate(2 * kPointerSize));
  __ push(eax);

  // Update the index on the stack and in register eax.
  __ mov(eax, Operand(ebp, kIndexOffset));
  __ add(Operand(eax), Immediate(1 << kSmiTagSize));
  __ mov(Operand(ebp, kIndexOffset), eax);

  __ bind(&entry);
  __ cmp(eax, Operand(ebp, kLimitOffset));
  __ j(not_equal, &loop);

  // Invoke the function.
  ParameterCount actual(eax);
  __ shr(eax, kSmiTagSize);
  __ mov(edi, Operand(ebp, 4 * kPointerSize));
  __ InvokeFunction(edi, actual, CALL_FUNCTION);

  __ LeaveInternalFrame();
  __ ret(3 * kPointerSize);  // remove this, receiver, and arguments
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ push(ebp);
  __ mov(ebp, Operand(esp));

  // Store the arguments adaptor context sentinel.
  __ push(Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Push the function on the stack.
  __ push(edi);

  // Preserve the number of arguments on the stack. Must preserve both
  // eax and ebx because these registers are used when copying the
  // arguments and the receiver.
  ASSERT(kSmiTagSize == 1);
  __ lea(ecx, Operand(eax, eax, times_1, kSmiTag));
  __ push(ecx);
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // Retrieve the number of arguments from the stack.
  __ mov(ebx, Operand(ebp, ArgumentsAdaptorFrameConstants::kLengthOffset));

  // Leave the frame.
  __ leave();

  // Remove caller arguments from the stack.
  ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ pop(ecx);
  __ lea(esp, Operand(esp, ebx, times_2, 1 * kPointerSize));  // 1 ~ receiver
  __ push(ecx);
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : actual number of arguments
  //  -- ebx : expected number of arguments
  //  -- edx : code entry to call
  // -----------------------------------

  Label invoke, dont_adapt_arguments;
  __ IncrementCounter(&Counters::arguments_adaptors, 1);

  Label enough, too_few;
  __ cmp(eax, Operand(ebx));
  __ j(less, &too_few);
  __ cmp(ebx, SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  __ j(equal, &dont_adapt_arguments);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Copy receiver and all expected arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(eax, Operand(ebp, eax, times_4, offset));
    __ mov(ecx, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ inc(ecx);
    __ push(Operand(eax, 0));
    __ sub(Operand(eax), Immediate(kPointerSize));
    __ cmp(ecx, Operand(ebx));
    __ j(less, &copy);
    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);

    // Copy receiver and all actual arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(edi, Operand(ebp, eax, times_4, offset));
    __ mov(ecx, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ inc(ecx);
    __ push(Operand(edi, 0));
    __ sub(Operand(edi), Immediate(kPointerSize));
    __ cmp(ecx, Operand(eax));
    __ j(less, &copy);

    // Fill remaining expected arguments with undefined values.
    Label fill;
    __ bind(&fill);
    __ inc(ecx);
    __ push(Immediate(Factory::undefined_value()));
    __ cmp(ecx, Operand(ebx));
    __ j(less, &fill);

    // Restore function pointer.
    __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ call(Operand(edx));

  // Leave frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ ret(0);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ jmp(Operand(edx));
}


#undef __

} }  // namespace v8::internal

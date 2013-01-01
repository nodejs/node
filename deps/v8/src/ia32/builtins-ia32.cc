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

#include "v8.h"

#if defined(V8_TARGET_ARCH_IA32)

#include "codegen.h"
#include "deoptimizer.h"
#include "full-codegen.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


void Builtins::Generate_Adaptor(MacroAssembler* masm,
                                CFunctionId id,
                                BuiltinExtraArguments extra_args) {
  // ----------- S t a t e -------------
  //  -- eax                : number of arguments excluding receiver
  //  -- edi                : called function (only guaranteed when
  //                          extra_args requires it)
  //  -- esi                : context
  //  -- esp[0]             : return address
  //  -- esp[4]             : last argument
  //  -- ...
  //  -- esp[4 * argc]      : first argument (argc == eax)
  //  -- esp[4 * (argc +1)] : receiver
  // -----------------------------------

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args == NEEDS_CALLED_FUNCTION) {
    num_extra_args = 1;
    Register scratch = ebx;
    __ pop(scratch);  // Save return address.
    __ push(edi);
    __ push(scratch);  // Restore return address.
  } else {
    ASSERT(extra_args == NO_EXTRA_ARGUMENTS);
  }

  // JumpToExternalReference expects eax to contain the number of arguments
  // including the receiver and the extra arguments.
  __ add(eax, Immediate(num_extra_args + 1));
  __ JumpToExternalReference(ExternalReference(id, masm->isolate()));
}


static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(eax, FieldOperand(eax, SharedFunctionInfo::kCodeOffset));
  __ lea(eax, FieldOperand(eax, Code::kHeaderSize));
  __ jmp(eax);
}


void Builtins::Generate_InRecompileQueue(MacroAssembler* masm) {
  GenerateTailCallToSharedCode(masm);
}


void Builtins::Generate_ParallelRecompile(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push a copy of the function onto the stack.
    __ push(edi);
    // Push call kind information.
    __ push(ecx);

    __ push(edi);  // Function is also the parameter to the runtime call.
    __ CallRuntime(Runtime::kParallelRecompile, 1);

    // Restore call kind information.
    __ pop(ecx);
    // Restore receiver.
    __ pop(edi);

    // Tear down internal frame.
  }

  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool count_constructions) {
  // ----------- S t a t e -------------
  //  -- eax: number of arguments
  //  -- edi: constructor function
  // -----------------------------------

  // Should never count constructions for api objects.
  ASSERT(!is_api_function || !count_constructions);

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Store a smi-tagged arguments count on the stack.
    __ SmiTag(eax);
    __ push(eax);

    // Push the function to invoke on the stack.
    __ push(edi);

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      Label undo_allocation;
#ifdef ENABLE_DEBUGGER_SUPPORT
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(masm->isolate());
      __ cmp(Operand::StaticVariable(debug_step_in_fp), Immediate(0));
      __ j(not_equal, &rt_call);
#endif

      // Verified that the constructor is a JSFunction.
      // Load the initial map and verify that it is in fact a map.
      // edi: constructor
      __ mov(eax, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
      // Will both indicate a NULL and a Smi
      __ JumpIfSmi(eax, &rt_call);
      // edi: constructor
      // eax: initial map (if proven valid below)
      __ CmpObjectType(eax, MAP_TYPE, ebx);
      __ j(not_equal, &rt_call);

      // Check that the constructor is not constructing a JSFunction (see
      // comments in Runtime_NewObject in runtime.cc). In which case the
      // initial map's instance type would be JS_FUNCTION_TYPE.
      // edi: constructor
      // eax: initial map
      __ CmpInstanceType(eax, JS_FUNCTION_TYPE);
      __ j(equal, &rt_call);

      if (count_constructions) {
        Label allocate;
        // Decrease generous allocation count.
        __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
        __ dec_b(FieldOperand(ecx,
                              SharedFunctionInfo::kConstructionCountOffset));
        __ j(not_zero, &allocate);

        __ push(eax);
        __ push(edi);

        __ push(edi);  // constructor
        // The call will replace the stub, so the countdown is only done once.
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);

        __ pop(edi);
        __ pop(eax);

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      // edi: constructor
      // eax: initial map
      __ movzx_b(edi, FieldOperand(eax, Map::kInstanceSizeOffset));
      __ shl(edi, kPointerSizeLog2);
      __ AllocateInNewSpace(
          edi, ebx, edi, no_reg, &rt_call, NO_ALLOCATION_FLAGS);
      // Allocated the JSObject, now initialize the fields.
      // eax: initial map
      // ebx: JSObject
      // edi: start of next object
      __ mov(Operand(ebx, JSObject::kMapOffset), eax);
      Factory* factory = masm->isolate()->factory();
      __ mov(ecx, factory->empty_fixed_array());
      __ mov(Operand(ebx, JSObject::kPropertiesOffset), ecx);
      __ mov(Operand(ebx, JSObject::kElementsOffset), ecx);
      // Set extra fields in the newly allocated object.
      // eax: initial map
      // ebx: JSObject
      // edi: start of next object
      __ lea(ecx, Operand(ebx, JSObject::kHeaderSize));
      __ mov(edx, factory->undefined_value());
      if (count_constructions) {
        __ movzx_b(esi,
                   FieldOperand(eax, Map::kPreAllocatedPropertyFieldsOffset));
        __ lea(esi,
               Operand(ebx, esi, times_pointer_size, JSObject::kHeaderSize));
        // esi: offset of first field after pre-allocated fields
        if (FLAG_debug_code) {
          __ cmp(esi, edi);
          __ Assert(less_equal,
                    "Unexpected number of pre-allocated property fields.");
        }
        __ InitializeFieldsWithFiller(ecx, esi, edx);
        __ mov(edx, factory->one_pointer_filler_map());
      }
      __ InitializeFieldsWithFiller(ecx, edi, edx);

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on. Any
      // failures need to undo the allocation, so that the heap is in a
      // consistent state and verifiable.
      // eax: initial map
      // ebx: JSObject
      // edi: start of next object
      __ or_(ebx, Immediate(kHeapObjectTag));

      // Check if a non-empty properties array is needed.
      // Allocate and initialize a FixedArray if it is.
      // eax: initial map
      // ebx: JSObject
      // edi: start of next object
      // Calculate the total number of properties described by the map.
      __ movzx_b(edx, FieldOperand(eax, Map::kUnusedPropertyFieldsOffset));
      __ movzx_b(ecx,
                 FieldOperand(eax, Map::kPreAllocatedPropertyFieldsOffset));
      __ add(edx, ecx);
      // Calculate unused properties past the end of the in-object properties.
      __ movzx_b(ecx, FieldOperand(eax, Map::kInObjectPropertiesOffset));
      __ sub(edx, ecx);
      // Done if no extra properties are to be allocated.
      __ j(zero, &allocated);
      __ Assert(positive, "Property allocation count failed.");

      // Scale the number of elements by pointer size and add the header for
      // FixedArrays to the start of the next object calculation from above.
      // ebx: JSObject
      // edi: start of next object (will be start of FixedArray)
      // edx: number of elements in properties array
      __ AllocateInNewSpace(FixedArray::kHeaderSize,
                            times_pointer_size,
                            edx,
                            edi,
                            ecx,
                            no_reg,
                            &undo_allocation,
                            RESULT_CONTAINS_TOP);

      // Initialize the FixedArray.
      // ebx: JSObject
      // edi: FixedArray
      // edx: number of elements
      // ecx: start of next object
      __ mov(eax, factory->fixed_array_map());
      __ mov(Operand(edi, FixedArray::kMapOffset), eax);  // setup the map
      __ SmiTag(edx);
      __ mov(Operand(edi, FixedArray::kLengthOffset), edx);  // and length

      // Initialize the fields to undefined.
      // ebx: JSObject
      // edi: FixedArray
      // ecx: start of next object
      { Label loop, entry;
        __ mov(edx, factory->undefined_value());
        __ lea(eax, Operand(edi, FixedArray::kHeaderSize));
        __ jmp(&entry);
        __ bind(&loop);
        __ mov(Operand(eax, 0), edx);
        __ add(eax, Immediate(kPointerSize));
        __ bind(&entry);
        __ cmp(eax, ecx);
        __ j(below, &loop);
      }

      // Store the initialized FixedArray into the properties field of
      // the JSObject
      // ebx: JSObject
      // edi: FixedArray
      __ or_(edi, Immediate(kHeapObjectTag));  // add the heap tag
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
    __ bind(&rt_call);
    // Must restore edi (constructor) before calling runtime.
    __ mov(edi, Operand(esp, 0));
    // edi: function (constructor)
    __ push(edi);
    __ CallRuntime(Runtime::kNewObject, 1);
    __ mov(ebx, eax);  // store result in ebx

    // New object allocated.
    // ebx: newly allocated object
    __ bind(&allocated);
    // Retrieve the function from the stack.
    __ pop(edi);

    // Retrieve smi-tagged arguments count from the stack.
    __ mov(eax, Operand(esp, 0));
    __ SmiUntag(eax);

    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ push(ebx);
    __ push(ebx);

    // Set up pointer to last argument.
    __ lea(ebx, Operand(ebp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(ecx, eax);
    __ jmp(&entry);
    __ bind(&loop);
    __ push(Operand(ebx, ecx, times_4, 0));
    __ bind(&entry);
    __ dec(ecx);
    __ j(greater_equal, &loop);

    // Call the function.
    if (is_api_function) {
      __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
      Handle<Code> code =
          masm->isolate()->builtins()->HandleApiCallConstruct();
      ParameterCount expected(0);
      __ InvokeCode(code, expected, expected, RelocInfo::CODE_TARGET,
                    CALL_FUNCTION, NullCallWrapper(), CALL_AS_METHOD);
    } else {
      ParameterCount actual(eax);
      __ InvokeFunction(edi, actual, CALL_FUNCTION,
                        NullCallWrapper(), CALL_AS_METHOD);
    }

    // Store offset of return address for deoptimizer.
    if (!is_api_function && !count_constructions) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, exit;

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(eax, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_SPEC_OBJECT_TYPE, it is not an object in the ECMA sense.
    __ CmpObjectType(eax, FIRST_SPEC_OBJECT_TYPE, ecx);
    __ j(above_equal, &exit);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ mov(eax, Operand(esp, 0));

    // Restore the arguments count and leave the construct frame.
    __ bind(&exit);
    __ mov(ebx, Operand(esp, kPointerSize));  // Get arguments count.

    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ pop(ecx);
  __ lea(esp, Operand(esp, ebx, times_2, 1 * kPointerSize));  // 1 ~ receiver
  __ push(ecx);
  __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1);
  __ ret(0);
}


void Builtins::Generate_JSConstructStubCountdown(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, true);
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, false);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true, false);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Clear the context before we push it when entering the internal frame.
  __ Set(esi, Immediate(0));

  {
    FrameScope scope(masm, StackFrame::INTERNAL);

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
    __ Set(ecx, Immediate(0));
    __ jmp(&entry);
    __ bind(&loop);
    __ mov(edx, Operand(ebx, ecx, times_4, 0));  // push parameter from argv
    __ push(Operand(edx, 0));  // dereference handle
    __ inc(ecx);
    __ bind(&entry);
    __ cmp(ecx, eax);
    __ j(not_equal, &loop);

    // Get the function from the stack and call it.
    // kPointerSize for the receiver.
    __ mov(edi, Operand(esp, eax, times_4, kPointerSize));

    // Invoke the code.
    if (is_construct) {
      CallConstructStub stub(NO_CALL_FUNCTION_FLAGS);
      __ CallStub(&stub);
    } else {
      ParameterCount actual(eax);
      __ InvokeFunction(edi, actual, CALL_FUNCTION,
                        NullCallWrapper(), CALL_AS_METHOD);
    }

    // Exit the internal frame. Notice that this also removes the empty.
    // context and the function left on the stack by the code
    // invocation.
  }
  __ ret(kPointerSize);  // Remove receiver.
}


void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}


void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}


void Builtins::Generate_LazyCompile(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push a copy of the function.
    __ push(edi);
    // Push call kind information.
    __ push(ecx);

    __ push(edi);  // Function is also the parameter to the runtime call.
    __ CallRuntime(Runtime::kLazyCompile, 1);

    // Restore call kind information.
    __ pop(ecx);
    // Restore receiver.
    __ pop(edi);

    // Tear down internal frame.
  }

  // Do a tail-call of the compiled function.
  __ lea(eax, FieldOperand(eax, Code::kHeaderSize));
  __ jmp(eax);
}


void Builtins::Generate_LazyRecompile(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push a copy of the function onto the stack.
    __ push(edi);
    // Push call kind information.
    __ push(ecx);

    __ push(edi);  // Function is also the parameter to the runtime call.
    __ CallRuntime(Runtime::kLazyRecompile, 1);

    // Restore call kind information.
    __ pop(ecx);
    // Restore receiver.
    __ pop(edi);

    // Tear down internal frame.
  }

  // Do a tail-call of the compiled function.
  __ lea(eax, FieldOperand(eax, Code::kHeaderSize));
  __ jmp(eax);
}


static void GenerateMakeCodeYoungAgainCommon(MacroAssembler* masm) {
  // For now, we are relying on the fact that make_code_young doesn't do any
  // garbage collection which allows us to save/restore the registers without
  // worrying about which of them contain pointers. We also don't build an
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Re-execute the code that was patched back to the young age when
  // the stub returns.
  __ sub(Operand(esp, 0), Immediate(5));
  __ pushad();
  __ mov(eax, Operand(esp, 8 * kPointerSize));
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(1, ebx);
    __ mov(Operand(esp, 0), eax);
    __ CallCFunction(
        ExternalReference::get_make_code_young_function(masm->isolate()), 1);
  }
  __ popad();
  __ ret(0);
}

#define DEFINE_CODE_AGE_BUILTIN_GENERATOR(C)                 \
void Builtins::Generate_Make##C##CodeYoungAgainEvenMarking(  \
    MacroAssembler* masm) {                                  \
  GenerateMakeCodeYoungAgainCommon(masm);                    \
}                                                            \
void Builtins::Generate_Make##C##CodeYoungAgainOddMarking(   \
    MacroAssembler* masm) {                                  \
  GenerateMakeCodeYoungAgainCommon(masm);                    \
}
CODE_AGE_LIST(DEFINE_CODE_AGE_BUILTIN_GENERATOR)
#undef DEFINE_CODE_AGE_BUILTIN_GENERATOR


static void Generate_NotifyDeoptimizedHelper(MacroAssembler* masm,
                                             Deoptimizer::BailoutType type) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Pass deoptimization type to the runtime system.
    __ push(Immediate(Smi::FromInt(static_cast<int>(type))));
    __ CallRuntime(Runtime::kNotifyDeoptimized, 1);

    // Tear down internal frame.
  }

  // Get the full codegen state from the stack and untag it.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));
  __ SmiUntag(ecx);

  // Switch on the state.
  Label not_no_registers, not_tos_eax;
  __ cmp(ecx, FullCodeGenerator::NO_REGISTERS);
  __ j(not_equal, &not_no_registers, Label::kNear);
  __ ret(1 * kPointerSize);  // Remove state.

  __ bind(&not_no_registers);
  __ mov(eax, Operand(esp, 2 * kPointerSize));
  __ cmp(ecx, FullCodeGenerator::TOS_REG);
  __ j(not_equal, &not_tos_eax, Label::kNear);
  __ ret(2 * kPointerSize);  // Remove state, eax.

  __ bind(&not_tos_eax);
  __ Abort("no cases left");
}


void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::EAGER);
}


void Builtins::Generate_NotifyLazyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::LAZY);
}


void Builtins::Generate_NotifyOSR(MacroAssembler* masm) {
  // TODO(kasperl): Do we need to save/restore the XMM registers too?

  // For now, we are relying on the fact that Runtime::NotifyOSR
  // doesn't do any garbage collection which allows us to save/restore
  // the registers without worrying about which of them contain
  // pointers. This seems a bit fragile.
  __ pushad();
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyOSR, 0);
  }
  __ popad();
  __ ret(0);
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  Factory* factory = masm->isolate()->factory();

  // 1. Make sure we have at least one argument.
  { Label done;
    __ test(eax, eax);
    __ j(not_zero, &done);
    __ pop(ebx);
    __ push(Immediate(factory->undefined_value()));
    __ push(ebx);
    __ inc(eax);
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack, check
  //    if it is a function.
  Label slow, non_function;
  // 1 ~ return address.
  __ mov(edi, Operand(esp, eax, times_4, 1 * kPointerSize));
  __ JumpIfSmi(edi, &non_function);
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &slow);


  // 3a. Patch the first argument if necessary when calling a function.
  Label shift_arguments;
  __ Set(edx, Immediate(0));  // indicate regular JS_FUNCTION
  { Label convert_to_object, use_global_receiver, patch_receiver;
    // Change context eagerly in case we need the global receiver.
    __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

    // Do not transform the receiver for strict mode functions.
    __ mov(ebx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ test_b(FieldOperand(ebx, SharedFunctionInfo::kStrictModeByteOffset),
              1 << SharedFunctionInfo::kStrictModeBitWithinByte);
    __ j(not_equal, &shift_arguments);

    // Do not transform the receiver for natives (shared already in ebx).
    __ test_b(FieldOperand(ebx, SharedFunctionInfo::kNativeByteOffset),
              1 << SharedFunctionInfo::kNativeBitWithinByte);
    __ j(not_equal, &shift_arguments);

    // Compute the receiver in non-strict mode.
    __ mov(ebx, Operand(esp, eax, times_4, 0));  // First argument.

    // Call ToObject on the receiver if it is not an object, or use the
    // global object if it is null or undefined.
    __ JumpIfSmi(ebx, &convert_to_object);
    __ cmp(ebx, factory->null_value());
    __ j(equal, &use_global_receiver);
    __ cmp(ebx, factory->undefined_value());
    __ j(equal, &use_global_receiver);
    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CmpObjectType(ebx, FIRST_SPEC_OBJECT_TYPE, ecx);
    __ j(above_equal, &shift_arguments);

    __ bind(&convert_to_object);

    { // In order to preserve argument count.
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(eax);
      __ push(eax);

      __ push(ebx);
      __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
      __ mov(ebx, eax);
      __ Set(edx, Immediate(0));  // restore

      __ pop(eax);
      __ SmiUntag(eax);
    }

    // Restore the function to edi.
    __ mov(edi, Operand(esp, eax, times_4, 1 * kPointerSize));
    __ jmp(&patch_receiver);

    // Use the global receiver object from the called function as the
    // receiver.
    __ bind(&use_global_receiver);
    const int kGlobalIndex =
        Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
    __ mov(ebx, FieldOperand(esi, kGlobalIndex));
    __ mov(ebx, FieldOperand(ebx, GlobalObject::kNativeContextOffset));
    __ mov(ebx, FieldOperand(ebx, kGlobalIndex));
    __ mov(ebx, FieldOperand(ebx, GlobalObject::kGlobalReceiverOffset));

    __ bind(&patch_receiver);
    __ mov(Operand(esp, eax, times_4, 0), ebx);

    __ jmp(&shift_arguments);
  }

  // 3b. Check for function proxy.
  __ bind(&slow);
  __ Set(edx, Immediate(1));  // indicate function proxy
  __ CmpInstanceType(ecx, JS_FUNCTION_PROXY_TYPE);
  __ j(equal, &shift_arguments);
  __ bind(&non_function);
  __ Set(edx, Immediate(2));  // indicate non-function

  // 3c. Patch the first argument when calling a non-function.  The
  //     CALL_NON_FUNCTION builtin expects the non-function callee as
  //     receiver, so overwrite the first argument which will ultimately
  //     become the receiver.
  __ mov(Operand(esp, eax, times_4, 0), edi);

  // 4. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  __ bind(&shift_arguments);
  { Label loop;
    __ mov(ecx, eax);
    __ bind(&loop);
    __ mov(ebx, Operand(esp, ecx, times_4, 0));
    __ mov(Operand(esp, ecx, times_4, kPointerSize), ebx);
    __ dec(ecx);
    __ j(not_sign, &loop);  // While non-negative (to copy return address).
    __ pop(ebx);  // Discard copy of return address.
    __ dec(eax);  // One fewer argument (first argument is new receiver).
  }

  // 5a. Call non-function via tail call to CALL_NON_FUNCTION builtin,
  //     or a function proxy via CALL_FUNCTION_PROXY.
  { Label function, non_proxy;
    __ test(edx, edx);
    __ j(zero, &function);
    __ Set(ebx, Immediate(0));
    __ cmp(edx, Immediate(1));
    __ j(not_equal, &non_proxy);

    __ pop(edx);   // return address
    __ push(edi);  // re-add proxy object as additional argument
    __ push(edx);
    __ inc(eax);
    __ SetCallKind(ecx, CALL_AS_FUNCTION);
    __ GetBuiltinEntry(edx, Builtins::CALL_FUNCTION_PROXY);
    __ jmp(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
           RelocInfo::CODE_TARGET);

    __ bind(&non_proxy);
    __ SetCallKind(ecx, CALL_AS_METHOD);
    __ GetBuiltinEntry(edx, Builtins::CALL_NON_FUNCTION);
    __ jmp(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
           RelocInfo::CODE_TARGET);
    __ bind(&function);
  }

  // 5b. Get the code to call from the function and check that the number of
  //     expected arguments matches what we're providing.  If so, jump
  //     (tail-call) to the code in register edx without checking arguments.
  __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ebx,
         FieldOperand(edx, SharedFunctionInfo::kFormalParameterCountOffset));
  __ mov(edx, FieldOperand(edi, JSFunction::kCodeEntryOffset));
  __ SmiUntag(ebx);
  __ SetCallKind(ecx, CALL_AS_METHOD);
  __ cmp(eax, ebx);
  __ j(not_equal,
       masm->isolate()->builtins()->ArgumentsAdaptorTrampoline());

  ParameterCount expected(0);
  __ InvokeCode(edx, expected, expected, JUMP_FUNCTION, NullCallWrapper(),
                CALL_AS_METHOD);
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  static const int kArgumentsOffset = 2 * kPointerSize;
  static const int kReceiverOffset = 3 * kPointerSize;
  static const int kFunctionOffset = 4 * kPointerSize;
  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);

    __ push(Operand(ebp, kFunctionOffset));  // push this
    __ push(Operand(ebp, kArgumentsOffset));  // push arguments
    __ InvokeBuiltin(Builtins::APPLY_PREPARE, CALL_FUNCTION);

    // Check the stack for overflow. We are not trying to catch
    // interruptions (e.g. debug break and preemption) here, so the "real stack
    // limit" is checked.
    Label okay;
    ExternalReference real_stack_limit =
        ExternalReference::address_of_real_stack_limit(masm->isolate());
    __ mov(edi, Operand::StaticVariable(real_stack_limit));
    // Make ecx the space we have left. The stack might already be overflowed
    // here which will cause ecx to become negative.
    __ mov(ecx, esp);
    __ sub(ecx, edi);
    // Make edx the space we need for the array when it is unrolled onto the
    // stack.
    __ mov(edx, eax);
    __ shl(edx, kPointerSizeLog2 - kSmiTagSize);
    // Check if the arguments will overflow the stack.
    __ cmp(ecx, edx);
    __ j(greater, &okay);  // Signed comparison.

    // Out of stack space.
    __ push(Operand(ebp, 4 * kPointerSize));  // push this
    __ push(eax);
    __ InvokeBuiltin(Builtins::APPLY_OVERFLOW, CALL_FUNCTION);
    __ bind(&okay);
    // End of stack check.

    // Push current index and limit.
    const int kLimitOffset =
        StandardFrameConstants::kExpressionsOffset - 1 * kPointerSize;
    const int kIndexOffset = kLimitOffset - 1 * kPointerSize;
    __ push(eax);  // limit
    __ push(Immediate(0));  // index

    // Get the receiver.
    __ mov(ebx, Operand(ebp, kReceiverOffset));

    // Check that the function is a JS function (otherwise it must be a proxy).
    Label push_receiver;
    __ mov(edi, Operand(ebp, kFunctionOffset));
    __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
    __ j(not_equal, &push_receiver);

    // Change context eagerly to get the right global object if necessary.
    __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

    // Compute the receiver.
    // Do not transform the receiver for strict mode functions.
    Label call_to_object, use_global_receiver;
    __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ test_b(FieldOperand(ecx, SharedFunctionInfo::kStrictModeByteOffset),
              1 << SharedFunctionInfo::kStrictModeBitWithinByte);
    __ j(not_equal, &push_receiver);

    Factory* factory = masm->isolate()->factory();

    // Do not transform the receiver for natives (shared already in ecx).
    __ test_b(FieldOperand(ecx, SharedFunctionInfo::kNativeByteOffset),
              1 << SharedFunctionInfo::kNativeBitWithinByte);
    __ j(not_equal, &push_receiver);

    // Compute the receiver in non-strict mode.
    // Call ToObject on the receiver if it is not an object, or use the
    // global object if it is null or undefined.
    __ JumpIfSmi(ebx, &call_to_object);
    __ cmp(ebx, factory->null_value());
    __ j(equal, &use_global_receiver);
    __ cmp(ebx, factory->undefined_value());
    __ j(equal, &use_global_receiver);
    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CmpObjectType(ebx, FIRST_SPEC_OBJECT_TYPE, ecx);
    __ j(above_equal, &push_receiver);

    __ bind(&call_to_object);
    __ push(ebx);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ mov(ebx, eax);
    __ jmp(&push_receiver);

    // Use the current global receiver object as the receiver.
    __ bind(&use_global_receiver);
    const int kGlobalOffset =
        Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
    __ mov(ebx, FieldOperand(esi, kGlobalOffset));
    __ mov(ebx, FieldOperand(ebx, GlobalObject::kNativeContextOffset));
    __ mov(ebx, FieldOperand(ebx, kGlobalOffset));
    __ mov(ebx, FieldOperand(ebx, GlobalObject::kGlobalReceiverOffset));

    // Push the receiver.
    __ bind(&push_receiver);
    __ push(ebx);

    // Copy all arguments from the array to the stack.
    Label entry, loop;
    __ mov(ecx, Operand(ebp, kIndexOffset));
    __ jmp(&entry);
    __ bind(&loop);
    __ mov(edx, Operand(ebp, kArgumentsOffset));  // load arguments

    // Use inline caching to speed up access to arguments.
    Handle<Code> ic = masm->isolate()->builtins()->KeyedLoadIC_Initialize();
    __ call(ic, RelocInfo::CODE_TARGET);
    // It is important that we do not have a test instruction after the
    // call.  A test instruction after the call is used to indicate that
    // we have generated an inline version of the keyed load.  In this
    // case, we know that we are not generating a test instruction next.

    // Push the nth argument.
    __ push(eax);

    // Update the index on the stack and in register eax.
    __ mov(ecx, Operand(ebp, kIndexOffset));
    __ add(ecx, Immediate(1 << kSmiTagSize));
    __ mov(Operand(ebp, kIndexOffset), ecx);

    __ bind(&entry);
    __ cmp(ecx, Operand(ebp, kLimitOffset));
    __ j(not_equal, &loop);

    // Invoke the function.
    Label call_proxy;
    __ mov(eax, ecx);
    ParameterCount actual(eax);
    __ SmiUntag(eax);
    __ mov(edi, Operand(ebp, kFunctionOffset));
    __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
    __ j(not_equal, &call_proxy);
    __ InvokeFunction(edi, actual, CALL_FUNCTION,
                      NullCallWrapper(), CALL_AS_METHOD);

    frame_scope.GenerateLeaveFrame();
    __ ret(3 * kPointerSize);  // remove this, receiver, and arguments

    // Invoke the function proxy.
    __ bind(&call_proxy);
    __ push(edi);  // add function proxy as last argument
    __ inc(eax);
    __ Set(ebx, Immediate(0));
    __ SetCallKind(ecx, CALL_AS_METHOD);
    __ GetBuiltinEntry(edx, Builtins::CALL_FUNCTION_PROXY);
    __ call(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);

    // Leave internal frame.
  }
  __ ret(3 * kPointerSize);  // remove this, receiver, and arguments
}


// Allocate an empty JSArray. The allocated array is put into the result
// register. If the parameter initial_capacity is larger than zero an elements
// backing store is allocated with this size and filled with the hole values.
// Otherwise the elements backing store is set to the empty FixedArray.
static void AllocateEmptyJSArray(MacroAssembler* masm,
                                 Register array_function,
                                 Register result,
                                 Register scratch1,
                                 Register scratch2,
                                 Register scratch3,
                                 Label* gc_required) {
  const int initial_capacity = JSArray::kPreallocatedArrayElements;
  STATIC_ASSERT(initial_capacity >= 0);

  __ LoadInitialArrayMap(array_function, scratch2, scratch1, false);

  // Allocate the JSArray object together with space for a fixed array with the
  // requested elements.
  int size = JSArray::kSize;
  if (initial_capacity > 0) {
    size += FixedArray::SizeFor(initial_capacity);
  }
  __ AllocateInNewSpace(size,
                        result,
                        scratch2,
                        scratch3,
                        gc_required,
                        TAG_OBJECT);

  // Allocated the JSArray. Now initialize the fields except for the elements
  // array.
  // result: JSObject
  // scratch1: initial map
  // scratch2: start of next object
  __ mov(FieldOperand(result, JSObject::kMapOffset), scratch1);
  Factory* factory = masm->isolate()->factory();
  __ mov(FieldOperand(result, JSArray::kPropertiesOffset),
         factory->empty_fixed_array());
  // Field JSArray::kElementsOffset is initialized later.
  __ mov(FieldOperand(result, JSArray::kLengthOffset), Immediate(0));

  // If no storage is requested for the elements array just set the empty
  // fixed array.
  if (initial_capacity == 0) {
    __ mov(FieldOperand(result, JSArray::kElementsOffset),
           factory->empty_fixed_array());
    return;
  }

  // Calculate the location of the elements array and set elements array member
  // of the JSArray.
  // result: JSObject
  // scratch2: start of next object
  __ lea(scratch1, Operand(result, JSArray::kSize));
  __ mov(FieldOperand(result, JSArray::kElementsOffset), scratch1);

  // Initialize the FixedArray and fill it with holes. FixedArray length is
  // stored as a smi.
  // result: JSObject
  // scratch1: elements array
  // scratch2: start of next object
  __ mov(FieldOperand(scratch1, FixedArray::kMapOffset),
         factory->fixed_array_map());
  __ mov(FieldOperand(scratch1, FixedArray::kLengthOffset),
         Immediate(Smi::FromInt(initial_capacity)));

  // Fill the FixedArray with the hole value. Inline the code if short.
  // Reconsider loop unfolding if kPreallocatedArrayElements gets changed.
  static const int kLoopUnfoldLimit = 4;
  if (initial_capacity <= kLoopUnfoldLimit) {
    // Use a scratch register here to have only one reloc info when unfolding
    // the loop.
    __ mov(scratch3, factory->the_hole_value());
    for (int i = 0; i < initial_capacity; i++) {
      __ mov(FieldOperand(scratch1,
                          FixedArray::kHeaderSize + i * kPointerSize),
             scratch3);
    }
  } else {
    Label loop, entry;
    __ mov(scratch2, Immediate(initial_capacity));
    __ jmp(&entry);
    __ bind(&loop);
    __ mov(FieldOperand(scratch1,
                        scratch2,
                        times_pointer_size,
                        FixedArray::kHeaderSize),
           factory->the_hole_value());
    __ bind(&entry);
    __ dec(scratch2);
    __ j(not_sign, &loop);
  }
}


// Allocate a JSArray with the number of elements stored in a register. The
// register array_function holds the built-in Array function and the register
// array_size holds the size of the array as a smi. The allocated array is put
// into the result register and beginning and end of the FixedArray elements
// storage is put into registers elements_array and elements_array_end  (see
// below for when that is not the case). If the parameter fill_with_holes is
// true the allocated elements backing store is filled with the hole values
// otherwise it is left uninitialized. When the backing store is filled the
// register elements_array is scratched.
static void AllocateJSArray(MacroAssembler* masm,
                            Register array_function,  // Array function.
                            Register array_size,  // As a smi, cannot be 0.
                            Register result,
                            Register elements_array,
                            Register elements_array_end,
                            Register scratch,
                            bool fill_with_hole,
                            Label* gc_required) {
  ASSERT(scratch.is(edi));  // rep stos destination
  ASSERT(!fill_with_hole || array_size.is(ecx));  // rep stos count
  ASSERT(!fill_with_hole || !result.is(eax));  // result is never eax

  __ LoadInitialArrayMap(array_function, scratch,
                         elements_array, fill_with_hole);

  // Allocate the JSArray object together with space for a FixedArray with the
  // requested elements.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ AllocateInNewSpace(JSArray::kSize + FixedArray::kHeaderSize,
                        times_half_pointer_size,  // array_size is a smi.
                        array_size,
                        result,
                        elements_array_end,
                        scratch,
                        gc_required,
                        TAG_OBJECT);

  // Allocated the JSArray. Now initialize the fields except for the elements
  // array.
  // result: JSObject
  // elements_array: initial map
  // elements_array_end: start of next object
  // array_size: size of array (smi)
  __ mov(FieldOperand(result, JSObject::kMapOffset), elements_array);
  Factory* factory = masm->isolate()->factory();
  __ mov(elements_array, factory->empty_fixed_array());
  __ mov(FieldOperand(result, JSArray::kPropertiesOffset), elements_array);
  // Field JSArray::kElementsOffset is initialized later.
  __ mov(FieldOperand(result, JSArray::kLengthOffset), array_size);

  // Calculate the location of the elements array and set elements array member
  // of the JSArray.
  // result: JSObject
  // elements_array_end: start of next object
  // array_size: size of array (smi)
  __ lea(elements_array, Operand(result, JSArray::kSize));
  __ mov(FieldOperand(result, JSArray::kElementsOffset), elements_array);

  // Initialize the fixed array. FixedArray length is stored as a smi.
  // result: JSObject
  // elements_array: elements array
  // elements_array_end: start of next object
  // array_size: size of array (smi)
  __ mov(FieldOperand(elements_array, FixedArray::kMapOffset),
         factory->fixed_array_map());
  // For non-empty JSArrays the length of the FixedArray and the JSArray is the
  // same.
  __ mov(FieldOperand(elements_array, FixedArray::kLengthOffset), array_size);

  // Fill the allocated FixedArray with the hole value if requested.
  // result: JSObject
  // elements_array: elements array
  if (fill_with_hole) {
    __ SmiUntag(array_size);
    __ lea(edi, Operand(elements_array,
                        FixedArray::kHeaderSize - kHeapObjectTag));
    __ mov(eax, factory->the_hole_value());
    __ cld();
    // Do not use rep stos when filling less than kRepStosThreshold
    // words.
    const int kRepStosThreshold = 16;
    Label loop, entry, done;
    __ cmp(ecx, kRepStosThreshold);
    __ j(below, &loop);  // Note: ecx > 0.
    __ rep_stos();
    __ jmp(&done);
    __ bind(&loop);
    __ stos();
    __ bind(&entry);
    __ cmp(edi, elements_array_end);
    __ j(below, &loop);
    __ bind(&done);
  }
}


// Create a new array for the built-in Array function. This function allocates
// the JSArray object and the FixedArray elements array and initializes these.
// If the Array cannot be constructed in native code the runtime is called. This
// function assumes the following state:
//   edi: constructor (built-in Array function)
//   eax: argc
//   esp[0]: return address
//   esp[4]: last argument
// This function is used for both construct and normal calls of Array. Whether
// it is a construct call or not is indicated by the construct_call parameter.
// The only difference between handling a construct call and a normal call is
// that for a construct call the constructor function in edi needs to be
// preserved for entering the generic code. In both cases argc in eax needs to
// be preserved.
static void ArrayNativeCode(MacroAssembler* masm,
                            bool construct_call,
                            Label* call_generic_code) {
  Label argc_one_or_more, argc_two_or_more, prepare_generic_code_call,
      empty_array, not_empty_array, finish, cant_transition_map, not_double;

  // Push the constructor and argc. No need to tag argc as a smi, as there will
  // be no garbage collection with this on the stack.
  int push_count = 0;
  if (construct_call) {
    push_count++;
    __ push(edi);
  }
  push_count++;
  __ push(eax);

  // Check for array construction with zero arguments.
  __ test(eax, eax);
  __ j(not_zero, &argc_one_or_more);

  __ bind(&empty_array);
  // Handle construction of an empty array.
  AllocateEmptyJSArray(masm,
                       edi,
                       eax,
                       ebx,
                       ecx,
                       edi,
                       &prepare_generic_code_call);
  __ IncrementCounter(masm->isolate()->counters()->array_function_native(), 1);
  __ pop(ebx);
  if (construct_call) {
    __ pop(edi);
  }
  __ ret(kPointerSize);

  // Check for one argument. Bail out if argument is not smi or if it is
  // negative.
  __ bind(&argc_one_or_more);
  __ cmp(eax, 1);
  __ j(not_equal, &argc_two_or_more);
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(ecx, Operand(esp, (push_count + 1) * kPointerSize));
  __ test(ecx, ecx);
  __ j(not_zero, &not_empty_array);

  // The single argument passed is zero, so we jump to the code above used to
  // handle the case of no arguments passed. To adapt the stack for that we move
  // the return address and the pushed constructor (if pushed) one stack slot up
  // thereby removing the passed argument. Argc is also on the stack - at the
  // bottom - and it needs to be changed from 1 to 0 to have the call into the
  // runtime system work in case a GC is required.
  for (int i = push_count; i > 0; i--) {
    __ mov(eax, Operand(esp, i * kPointerSize));
    __ mov(Operand(esp, (i + 1) * kPointerSize), eax);
  }
  __ Drop(2);  // Drop two stack slots.
  __ push(Immediate(0));  // Treat this as a call with argc of zero.
  __ jmp(&empty_array);

  __ bind(&not_empty_array);
  __ test(ecx, Immediate(kIntptrSignBit | kSmiTagMask));
  __ j(not_zero, &prepare_generic_code_call);

  // Handle construction of an empty array of a certain size. Get the size from
  // the stack and bail out if size is to large to actually allocate an elements
  // array.
  __ cmp(ecx, JSObject::kInitialMaxFastElementArray << kSmiTagSize);
  __ j(greater_equal, &prepare_generic_code_call);

  // edx: array_size (smi)
  // edi: constructor
  // esp[0]: argc (cannot be 0 here)
  // esp[4]: constructor (only if construct_call)
  // esp[8]: return address
  // esp[C]: argument
  AllocateJSArray(masm,
                  edi,
                  ecx,
                  ebx,
                  eax,
                  edx,
                  edi,
                  true,
                  &prepare_generic_code_call);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->array_function_native(), 1);
  __ mov(eax, ebx);
  __ pop(ebx);
  if (construct_call) {
    __ pop(edi);
  }
  __ ret(2 * kPointerSize);

  // Handle construction of an array from a list of arguments.
  __ bind(&argc_two_or_more);
  STATIC_ASSERT(kSmiTag == 0);
  __ SmiTag(eax);  // Convet argc to a smi.
  // eax: array_size (smi)
  // edi: constructor
  // esp[0] : argc
  // esp[4]: constructor (only if construct_call)
  // esp[8] : return address
  // esp[C] : last argument
  AllocateJSArray(masm,
                  edi,
                  eax,
                  ebx,
                  ecx,
                  edx,
                  edi,
                  false,
                  &prepare_generic_code_call);
  __ IncrementCounter(counters->array_function_native(), 1);
  __ push(ebx);
  __ mov(ebx, Operand(esp, kPointerSize));
  // ebx: argc
  // edx: elements_array_end (untagged)
  // esp[0]: JSArray
  // esp[4]: argc
  // esp[8]: constructor (only if construct_call)
  // esp[12]: return address
  // esp[16]: last argument

  // Location of the last argument
  int last_arg_offset = (construct_call ? 4 : 3) * kPointerSize;
  __ lea(edi, Operand(esp, last_arg_offset));

  // Location of the first array element (Parameter fill_with_holes to
  // AllocateJSArray is false, so the FixedArray is returned in ecx).
  __ lea(edx, Operand(ecx, FixedArray::kHeaderSize - kHeapObjectTag));

  Label has_non_smi_element;

  // ebx: argc
  // edx: location of the first array element
  // edi: location of the last argument
  // esp[0]: JSArray
  // esp[4]: argc
  // esp[8]: constructor (only if construct_call)
  // esp[12]: return address
  // esp[16]: last argument
  Label loop, entry;
  __ mov(ecx, ebx);
  __ jmp(&entry);
  __ bind(&loop);
  __ mov(eax, Operand(edi, ecx, times_pointer_size, 0));
  if (FLAG_smi_only_arrays) {
    __ JumpIfNotSmi(eax, &has_non_smi_element);
  }
  __ mov(Operand(edx, 0), eax);
  __ add(edx, Immediate(kPointerSize));
  __ bind(&entry);
  __ dec(ecx);
  __ j(greater_equal, &loop);

  // Remove caller arguments from the stack and return.
  // ebx: argc
  // esp[0]: JSArray
  // esp[4]: argc
  // esp[8]: constructor (only if construct_call)
  // esp[12]: return address
  // esp[16]: last argument
  __ bind(&finish);
  __ mov(ecx, Operand(esp, last_arg_offset - kPointerSize));
  __ pop(eax);
  __ pop(ebx);
  __ lea(esp, Operand(esp, ebx, times_pointer_size,
                      last_arg_offset - kPointerSize));
  __ jmp(ecx);

  __ bind(&has_non_smi_element);
  // Double values are handled by the runtime.
  __ CheckMap(eax,
              masm->isolate()->factory()->heap_number_map(),
              &not_double,
              DONT_DO_SMI_CHECK);
  __ bind(&cant_transition_map);
  // Throw away the array that's only been partially constructed.
  __ pop(eax);
  __ UndoAllocationInNewSpace(eax);
  __ jmp(&prepare_generic_code_call);

  __ bind(&not_double);
  // Transition FAST_SMI_ELEMENTS to FAST_ELEMENTS.
  __ mov(ebx, Operand(esp, 0));
  __ mov(edi, FieldOperand(ebx, HeapObject::kMapOffset));
  __ LoadTransitionedArrayMapConditional(
      FAST_SMI_ELEMENTS,
      FAST_ELEMENTS,
      edi,
      eax,
      &cant_transition_map);
  __ mov(FieldOperand(ebx, HeapObject::kMapOffset), edi);
  __ RecordWriteField(ebx, HeapObject::kMapOffset, edi, eax,
                      kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

  // Prepare to re-enter the loop
  __ lea(edi, Operand(esp, last_arg_offset));

  // Finish the array initialization loop.
  Label loop2;
  __ bind(&loop2);
  __ mov(eax, Operand(edi, ecx, times_pointer_size, 0));
  __ mov(Operand(edx, 0), eax);
  __ add(edx, Immediate(kPointerSize));
  __ dec(ecx);
  __ j(greater_equal, &loop2);
  __ jmp(&finish);

  // Restore argc and constructor before running the generic code.
  __ bind(&prepare_generic_code_call);
  __ pop(eax);
  if (construct_call) {
    __ pop(edi);
  }
  __ jmp(call_generic_code);
}


void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argc
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------
  Label generic_array_code;

  // Get the InternalArray function.
  __ LoadGlobalFunction(Context::INTERNAL_ARRAY_FUNCTION_INDEX, edi);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray function should be a map.
    __ mov(ebx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ test(ebx, Immediate(kSmiTagMask));
    __ Assert(not_zero, "Unexpected initial map for InternalArray function");
    __ CmpObjectType(ebx, MAP_TYPE, ecx);
    __ Assert(equal, "Unexpected initial map for InternalArray function");
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  ArrayNativeCode(masm, false, &generic_array_code);

  // Jump to the generic internal array code in case the specialized code cannot
  // handle the construction.
  __ bind(&generic_array_code);
  Handle<Code> array_code =
      masm->isolate()->builtins()->InternalArrayCodeGeneric();
  __ jmp(array_code, RelocInfo::CODE_TARGET);
}


void Builtins::Generate_ArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argc
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------
  Label generic_array_code;

  // Get the Array function.
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, edi);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array function should be a map.
    __ mov(ebx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ test(ebx, Immediate(kSmiTagMask));
    __ Assert(not_zero, "Unexpected initial map for Array function");
    __ CmpObjectType(ebx, MAP_TYPE, ecx);
    __ Assert(equal, "Unexpected initial map for Array function");
  }

  // Run the native code for the Array function called as a normal function.
  ArrayNativeCode(masm, false, &generic_array_code);

  // Jump to the generic array code in case the specialized code cannot handle
  // the construction.
  __ bind(&generic_array_code);
  Handle<Code> array_code =
      masm->isolate()->builtins()->ArrayCodeGeneric();
  __ jmp(array_code, RelocInfo::CODE_TARGET);
}


void Builtins::Generate_ArrayConstructCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argc
  //  -- edi : constructor
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------
  Label generic_constructor;

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ mov(ebx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ test(ebx, Immediate(kSmiTagMask));
    __ Assert(not_zero, "Unexpected initial map for Array function");
    __ CmpObjectType(ebx, MAP_TYPE, ecx);
    __ Assert(equal, "Unexpected initial map for Array function");
  }

  // Run the native code for the Array function called as constructor.
  ArrayNativeCode(masm, true, &generic_constructor);

  // Jump to the generic construct code in case the specialized code cannot
  // handle the construction.
  __ bind(&generic_constructor);
  Handle<Code> generic_construct_stub =
      masm->isolate()->builtins()->JSConstructStubGeneric();
  __ jmp(generic_construct_stub, RelocInfo::CODE_TARGET);
}


void Builtins::Generate_StringConstructCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax                 : number of arguments
  //  -- edi                 : constructor function
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_ctor_calls(), 1);

  if (FLAG_debug_code) {
    __ LoadGlobalFunction(Context::STRING_FUNCTION_INDEX, ecx);
    __ cmp(edi, ecx);
    __ Assert(equal, "Unexpected String function");
  }

  // Load the first argument into eax and get rid of the rest
  // (including the receiver).
  Label no_arguments;
  __ test(eax, eax);
  __ j(zero, &no_arguments);
  __ mov(ebx, Operand(esp, eax, times_pointer_size, 0));
  __ pop(ecx);
  __ lea(esp, Operand(esp, eax, times_pointer_size, kPointerSize));
  __ push(ecx);
  __ mov(eax, ebx);

  // Lookup the argument in the number to string cache.
  Label not_cached, argument_is_string;
  NumberToStringStub::GenerateLookupNumberStringCache(
      masm,
      eax,  // Input.
      ebx,  // Result.
      ecx,  // Scratch 1.
      edx,  // Scratch 2.
      false,  // Input is known to be smi?
      &not_cached);
  __ IncrementCounter(counters->string_ctor_cached_number(), 1);
  __ bind(&argument_is_string);
  // ----------- S t a t e -------------
  //  -- ebx    : argument converted to string
  //  -- edi    : constructor function
  //  -- esp[0] : return address
  // -----------------------------------

  // Allocate a JSValue and put the tagged pointer into eax.
  Label gc_required;
  __ AllocateInNewSpace(JSValue::kSize,
                        eax,  // Result.
                        ecx,  // New allocation top (we ignore it).
                        no_reg,
                        &gc_required,
                        TAG_OBJECT);

  // Set the map.
  __ LoadGlobalFunctionInitialMap(edi, ecx);
  if (FLAG_debug_code) {
    __ cmpb(FieldOperand(ecx, Map::kInstanceSizeOffset),
            JSValue::kSize >> kPointerSizeLog2);
    __ Assert(equal, "Unexpected string wrapper instance size");
    __ cmpb(FieldOperand(ecx, Map::kUnusedPropertyFieldsOffset), 0);
    __ Assert(equal, "Unexpected unused properties of string wrapper");
  }
  __ mov(FieldOperand(eax, HeapObject::kMapOffset), ecx);

  // Set properties and elements.
  Factory* factory = masm->isolate()->factory();
  __ Set(ecx, Immediate(factory->empty_fixed_array()));
  __ mov(FieldOperand(eax, JSObject::kPropertiesOffset), ecx);
  __ mov(FieldOperand(eax, JSObject::kElementsOffset), ecx);

  // Set the value.
  __ mov(FieldOperand(eax, JSValue::kValueOffset), ebx);

  // Ensure the object is fully initialized.
  STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);

  // We're done. Return.
  __ ret(0);

  // The argument was not found in the number to string cache. Check
  // if it's a string already before calling the conversion builtin.
  Label convert_argument;
  __ bind(&not_cached);
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(eax, &convert_argument);
  Condition is_string = masm->IsObjectStringType(eax, ebx, ecx);
  __ j(NegateCondition(is_string), &convert_argument);
  __ mov(ebx, eax);
  __ IncrementCounter(counters->string_ctor_string_value(), 1);
  __ jmp(&argument_is_string);

  // Invoke the conversion builtin and put the result into ebx.
  __ bind(&convert_argument);
  __ IncrementCounter(counters->string_ctor_conversions(), 1);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(edi);  // Preserve the function.
    __ push(eax);
    __ InvokeBuiltin(Builtins::TO_STRING, CALL_FUNCTION);
    __ pop(edi);
  }
  __ mov(ebx, eax);
  __ jmp(&argument_is_string);

  // Load the empty string into ebx, remove the receiver from the
  // stack, and jump back to the case where the argument is a string.
  __ bind(&no_arguments);
  __ Set(ebx, Immediate(factory->empty_string()));
  __ pop(ecx);
  __ lea(esp, Operand(esp, kPointerSize));
  __ push(ecx);
  __ jmp(&argument_is_string);

  // At this point the argument is already a string. Call runtime to
  // create a string wrapper.
  __ bind(&gc_required);
  __ IncrementCounter(counters->string_ctor_gc_required(), 1);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(ebx);
    __ CallRuntime(Runtime::kNewStringWrapper, 1);
  }
  __ ret(0);
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ push(ebp);
  __ mov(ebp, esp);

  // Store the arguments adaptor context sentinel.
  __ push(Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Push the function on the stack.
  __ push(edi);

  // Preserve the number of arguments on the stack. Must preserve eax,
  // ebx and ecx because these registers are used when copying the
  // arguments and the receiver.
  STATIC_ASSERT(kSmiTagSize == 1);
  __ lea(edi, Operand(eax, eax, times_1, kSmiTag));
  __ push(edi);
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // Retrieve the number of arguments from the stack.
  __ mov(ebx, Operand(ebp, ArgumentsAdaptorFrameConstants::kLengthOffset));

  // Leave the frame.
  __ leave();

  // Remove caller arguments from the stack.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ pop(ecx);
  __ lea(esp, Operand(esp, ebx, times_2, 1 * kPointerSize));  // 1 ~ receiver
  __ push(ecx);
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : actual number of arguments
  //  -- ebx : expected number of arguments
  //  -- ecx : call kind information
  //  -- edx : code entry to call
  // -----------------------------------

  Label invoke, dont_adapt_arguments;
  __ IncrementCounter(masm->isolate()->counters()->arguments_adaptors(), 1);

  Label enough, too_few;
  __ cmp(eax, ebx);
  __ j(less, &too_few);
  __ cmp(ebx, SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  __ j(equal, &dont_adapt_arguments);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Copy receiver and all expected arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(eax, Operand(ebp, eax, times_4, offset));
    __ mov(edi, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ inc(edi);
    __ push(Operand(eax, 0));
    __ sub(eax, Immediate(kPointerSize));
    __ cmp(edi, ebx);
    __ j(less, &copy);
    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);

    // Copy receiver and all actual arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(edi, Operand(ebp, eax, times_4, offset));
    // ebx = expected - actual.
    __ sub(ebx, eax);
    // eax = -actual - 1
    __ neg(eax);
    __ sub(eax, Immediate(1));

    Label copy;
    __ bind(&copy);
    __ inc(eax);
    __ push(Operand(edi, 0));
    __ sub(edi, Immediate(kPointerSize));
    __ test(eax, eax);
    __ j(not_zero, &copy);

    // Fill remaining expected arguments with undefined values.
    Label fill;
    __ bind(&fill);
    __ inc(eax);
    __ push(Immediate(masm->isolate()->factory()->undefined_value()));
    __ cmp(eax, ebx);
    __ j(less, &fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  // Restore function pointer.
  __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ call(edx);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Leave frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ ret(0);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ jmp(edx);
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  CpuFeatures::TryForceFeatureScope scope(SSE2);
  if (!CpuFeatures::IsSupported(SSE2) && FLAG_debug_code) {
    __ Abort("Unreachable code: Cannot optimize without SSE2 support.");
    return;
  }

  // Get the loop depth of the stack guard check. This is recorded in
  // a test(eax, depth) instruction right after the call.
  Label stack_check;
  __ mov(ebx, Operand(esp, 0));  // return address
  if (FLAG_debug_code) {
    __ cmpb(Operand(ebx, 0), Assembler::kTestAlByte);
    __ Assert(equal, "test eax instruction not found after loop stack check");
  }
  __ movzx_b(ebx, Operand(ebx, 1));  // depth

  // Get the loop nesting level at which we allow OSR from the
  // unoptimized code and check if we want to do OSR yet. If not we
  // should perform a stack guard check so we can get interrupts while
  // waiting for on-stack replacement.
  __ mov(eax, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(ecx, FieldOperand(eax, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ecx, FieldOperand(ecx, SharedFunctionInfo::kCodeOffset));
  __ cmpb(ebx, FieldOperand(ecx, Code::kAllowOSRAtLoopNestingLevelOffset));
  __ j(greater, &stack_check);

  // Pass the function to optimize as the argument to the on-stack
  // replacement runtime function.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(eax);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement, 1);
  }

  // If the result was -1 it means that we couldn't optimize the
  // function. Just return and continue in the unoptimized version.
  Label skip;
  __ cmp(eax, Immediate(Smi::FromInt(-1)));
  __ j(not_equal, &skip, Label::kNear);
  __ ret(0);

  // Insert a stack guard check so that if we decide not to perform
  // on-stack replacement right away, the function calling this stub can
  // still be interrupted.
  __ bind(&stack_check);
  Label ok;
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(masm->isolate());
  __ cmp(esp, Operand::StaticVariable(stack_limit));
  __ j(above_equal, &ok, Label::kNear);
  StackCheckStub stub;
  __ TailCallStub(&stub);
  if (FLAG_debug_code) {
    __ Abort("Unreachable code: returned from tail call.");
  }
  __ bind(&ok);
  __ ret(0);

  __ bind(&skip);
  // Untag the AST id and push it on the stack.
  __ SmiUntag(eax);
  __ push(eax);

  // Generate the code for doing the frame-to-frame translation using
  // the deoptimizer infrastructure.
  Deoptimizer::EntryGenerator generator(masm, Deoptimizer::OSR);
  generator.Generate();
}


#undef __
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32

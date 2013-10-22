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

#if V8_TARGET_ARCH_X64

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
  //  -- rax                 : number of arguments excluding receiver
  //  -- rdi                 : called function (only guaranteed when
  //                           extra_args requires it)
  //  -- rsi                 : context
  //  -- rsp[0]              : return address
  //  -- rsp[8]              : last argument
  //  -- ...
  //  -- rsp[8 * argc]       : first argument (argc == rax)
  //  -- rsp[8 * (argc + 1)] : receiver
  // -----------------------------------

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args == NEEDS_CALLED_FUNCTION) {
    num_extra_args = 1;
    __ PopReturnAddressTo(kScratchRegister);
    __ push(rdi);
    __ PushReturnAddressFrom(kScratchRegister);
  } else {
    ASSERT(extra_args == NO_EXTRA_ARGUMENTS);
  }

  // JumpToExternalReference expects rax to contain the number of arguments
  // including the receiver and the extra arguments.
  __ addq(rax, Immediate(num_extra_args + 1));
  __ JumpToExternalReference(ExternalReference(id, masm->isolate()), 1);
}


static void CallRuntimePassFunction(MacroAssembler* masm,
                                    Runtime::FunctionId function_id) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function onto the stack.
  __ push(rdi);
  // Push call kind information.
  __ push(rcx);
  // Function is also the parameter to the runtime call.
  __ push(rdi);

  __ CallRuntime(function_id, 1);
  // Restore call kind information.
  __ pop(rcx);
  // Restore receiver.
  __ pop(rdi);
}


static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ movq(kScratchRegister,
          FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movq(kScratchRegister,
          FieldOperand(kScratchRegister, SharedFunctionInfo::kCodeOffset));
  __ lea(kScratchRegister, FieldOperand(kScratchRegister, Code::kHeaderSize));
  __ jmp(kScratchRegister);
}


void Builtins::Generate_InRecompileQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
  __ j(above_equal, &ok);

  CallRuntimePassFunction(masm, Runtime::kTryInstallRecompiledCode);
  // Tail call to returned code.
  __ lea(rax, FieldOperand(rax, Code::kHeaderSize));
  __ jmp(rax);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}


void Builtins::Generate_ConcurrentRecompile(MacroAssembler* masm) {
  CallRuntimePassFunction(masm, Runtime::kConcurrentRecompile);
  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool count_constructions) {
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  //  -- rdi: constructor function
  // -----------------------------------

  // Should never count constructions for api objects.
  ASSERT(!is_api_function || !count_constructions);

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Store a smi-tagged arguments count on the stack.
    __ Integer32ToSmi(rax, rax);
    __ push(rax);

    // Push the function to invoke on the stack.
    __ push(rdi);

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      Label undo_allocation;

#ifdef ENABLE_DEBUGGER_SUPPORT
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(masm->isolate());
      __ movq(kScratchRegister, debug_step_in_fp);
      __ cmpq(Operand(kScratchRegister, 0), Immediate(0));
      __ j(not_equal, &rt_call);
#endif

      // Verified that the constructor is a JSFunction.
      // Load the initial map and verify that it is in fact a map.
      // rdi: constructor
      __ movq(rax, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
      // Will both indicate a NULL and a Smi
      ASSERT(kSmiTag == 0);
      __ JumpIfSmi(rax, &rt_call);
      // rdi: constructor
      // rax: initial map (if proven valid below)
      __ CmpObjectType(rax, MAP_TYPE, rbx);
      __ j(not_equal, &rt_call);

      // Check that the constructor is not constructing a JSFunction (see
      // comments in Runtime_NewObject in runtime.cc). In which case the
      // initial map's instance type would be JS_FUNCTION_TYPE.
      // rdi: constructor
      // rax: initial map
      __ CmpInstanceType(rax, JS_FUNCTION_TYPE);
      __ j(equal, &rt_call);

      if (count_constructions) {
        Label allocate;
        // Decrease generous allocation count.
        __ movq(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
        __ decb(FieldOperand(rcx,
                             SharedFunctionInfo::kConstructionCountOffset));
        __ j(not_zero, &allocate);

        __ push(rax);
        __ push(rdi);

        __ push(rdi);  // constructor
        // The call will replace the stub, so the countdown is only done once.
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);

        __ pop(rdi);
        __ pop(rax);

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      __ movzxbq(rdi, FieldOperand(rax, Map::kInstanceSizeOffset));
      __ shl(rdi, Immediate(kPointerSizeLog2));
      // rdi: size of new object
      __ Allocate(rdi,
                  rbx,
                  rdi,
                  no_reg,
                  &rt_call,
                  NO_ALLOCATION_FLAGS);
      // Allocated the JSObject, now initialize the fields.
      // rax: initial map
      // rbx: JSObject (not HeapObject tagged - the actual address).
      // rdi: start of next object
      __ movq(Operand(rbx, JSObject::kMapOffset), rax);
      __ LoadRoot(rcx, Heap::kEmptyFixedArrayRootIndex);
      __ movq(Operand(rbx, JSObject::kPropertiesOffset), rcx);
      __ movq(Operand(rbx, JSObject::kElementsOffset), rcx);
      // Set extra fields in the newly allocated object.
      // rax: initial map
      // rbx: JSObject
      // rdi: start of next object
      __ lea(rcx, Operand(rbx, JSObject::kHeaderSize));
      __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
      if (count_constructions) {
        __ movzxbq(rsi,
                   FieldOperand(rax, Map::kPreAllocatedPropertyFieldsOffset));
        __ lea(rsi,
               Operand(rbx, rsi, times_pointer_size, JSObject::kHeaderSize));
        // rsi: offset of first field after pre-allocated fields
        if (FLAG_debug_code) {
          __ cmpq(rsi, rdi);
          __ Assert(less_equal,
                    kUnexpectedNumberOfPreAllocatedPropertyFields);
        }
        __ InitializeFieldsWithFiller(rcx, rsi, rdx);
        __ LoadRoot(rdx, Heap::kOnePointerFillerMapRootIndex);
      }
      __ InitializeFieldsWithFiller(rcx, rdi, rdx);

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on. Any
      // failures need to undo the allocation, so that the heap is in a
      // consistent state and verifiable.
      // rax: initial map
      // rbx: JSObject
      // rdi: start of next object
      __ or_(rbx, Immediate(kHeapObjectTag));

      // Check if a non-empty properties array is needed.
      // Allocate and initialize a FixedArray if it is.
      // rax: initial map
      // rbx: JSObject
      // rdi: start of next object
      // Calculate total properties described map.
      __ movzxbq(rdx, FieldOperand(rax, Map::kUnusedPropertyFieldsOffset));
      __ movzxbq(rcx,
                 FieldOperand(rax, Map::kPreAllocatedPropertyFieldsOffset));
      __ addq(rdx, rcx);
      // Calculate unused properties past the end of the in-object properties.
      __ movzxbq(rcx, FieldOperand(rax, Map::kInObjectPropertiesOffset));
      __ subq(rdx, rcx);
      // Done if no extra properties are to be allocated.
      __ j(zero, &allocated);
      __ Assert(positive, kPropertyAllocationCountFailed);

      // Scale the number of elements by pointer size and add the header for
      // FixedArrays to the start of the next object calculation from above.
      // rbx: JSObject
      // rdi: start of next object (will be start of FixedArray)
      // rdx: number of elements in properties array
      __ Allocate(FixedArray::kHeaderSize,
                  times_pointer_size,
                  rdx,
                  rdi,
                  rax,
                  no_reg,
                  &undo_allocation,
                  RESULT_CONTAINS_TOP);

      // Initialize the FixedArray.
      // rbx: JSObject
      // rdi: FixedArray
      // rdx: number of elements
      // rax: start of next object
      __ LoadRoot(rcx, Heap::kFixedArrayMapRootIndex);
      __ movq(Operand(rdi, HeapObject::kMapOffset), rcx);  // setup the map
      __ Integer32ToSmi(rdx, rdx);
      __ movq(Operand(rdi, FixedArray::kLengthOffset), rdx);  // and length

      // Initialize the fields to undefined.
      // rbx: JSObject
      // rdi: FixedArray
      // rax: start of next object
      // rdx: number of elements
      { Label loop, entry;
        __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
        __ lea(rcx, Operand(rdi, FixedArray::kHeaderSize));
        __ jmp(&entry);
        __ bind(&loop);
        __ movq(Operand(rcx, 0), rdx);
        __ addq(rcx, Immediate(kPointerSize));
        __ bind(&entry);
        __ cmpq(rcx, rax);
        __ j(below, &loop);
      }

      // Store the initialized FixedArray into the properties field of
      // the JSObject
      // rbx: JSObject
      // rdi: FixedArray
      __ or_(rdi, Immediate(kHeapObjectTag));  // add the heap tag
      __ movq(FieldOperand(rbx, JSObject::kPropertiesOffset), rdi);


      // Continue with JSObject being successfully allocated
      // rbx: JSObject
      __ jmp(&allocated);

      // Undo the setting of the new top so that the heap is verifiable. For
      // example, the map's unused properties potentially do not match the
      // allocated objects unused properties.
      // rbx: JSObject (previous new top)
      __ bind(&undo_allocation);
      __ UndoAllocationInNewSpace(rbx);
    }

    // Allocate the new receiver object using the runtime call.
    // rdi: function (constructor)
    __ bind(&rt_call);
    // Must restore rdi (constructor) before calling runtime.
    __ movq(rdi, Operand(rsp, 0));
    __ push(rdi);
    __ CallRuntime(Runtime::kNewObject, 1);
    __ movq(rbx, rax);  // store result in rbx

    // New object allocated.
    // rbx: newly allocated object
    __ bind(&allocated);
    // Retrieve the function from the stack.
    __ pop(rdi);

    // Retrieve smi-tagged arguments count from the stack.
    __ movq(rax, Operand(rsp, 0));
    __ SmiToInteger32(rax, rax);

    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ push(rbx);
    __ push(rbx);

    // Set up pointer to last argument.
    __ lea(rbx, Operand(rbp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ movq(rcx, rax);
    __ jmp(&entry);
    __ bind(&loop);
    __ push(Operand(rbx, rcx, times_pointer_size, 0));
    __ bind(&entry);
    __ decq(rcx);
    __ j(greater_equal, &loop);

    // Call the function.
    if (is_api_function) {
      __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));
      Handle<Code> code =
          masm->isolate()->builtins()->HandleApiCallConstruct();
      ParameterCount expected(0);
      __ InvokeCode(code, expected, expected, RelocInfo::CODE_TARGET,
                    CALL_FUNCTION, NullCallWrapper(), CALL_AS_METHOD);
    } else {
      ParameterCount actual(rax);
      __ InvokeFunction(rdi, actual, CALL_FUNCTION,
                        NullCallWrapper(), CALL_AS_METHOD);
    }

    // Store offset of return address for deoptimizer.
    if (!is_api_function && !count_constructions) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, exit;
    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(rax, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_SPEC_OBJECT_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rcx);
    __ j(above_equal, &exit);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ movq(rax, Operand(rsp, 0));

    // Restore the arguments count and leave the construct frame.
    __ bind(&exit);
    __ movq(rbx, Operand(rsp, kPointerSize));  // Get arguments count.

    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ lea(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->constructed_objects(), 1);
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
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Expects five C++ function parameters.
  // - Address entry (ignored)
  // - JSFunction* function (
  // - Object* receiver
  // - int argc
  // - Object*** argv
  // (see Handle::Invoke in execution.cc).

  // Open a C++ scope for the FrameScope.
  {
    // Platform specific argument handling. After this, the stack contains
    // an internal frame and the pushed function and receiver, and
    // register rax and rbx holds the argument count and argument array,
    // while rdi holds the function pointer and rsi the context.

#ifdef _WIN64
    // MSVC parameters in:
    // rcx        : entry (ignored)
    // rdx        : function
    // r8         : receiver
    // r9         : argc
    // [rsp+0x20] : argv

    // Clear the context before we push it when entering the internal frame.
    __ Set(rsi, 0);
    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Load the function context into rsi.
    __ movq(rsi, FieldOperand(rdx, JSFunction::kContextOffset));

    // Push the function and the receiver onto the stack.
    __ push(rdx);
    __ push(r8);

    // Load the number of arguments and setup pointer to the arguments.
    __ movq(rax, r9);
    // Load the previous frame pointer to access C argument on stack
    __ movq(kScratchRegister, Operand(rbp, 0));
    __ movq(rbx, Operand(kScratchRegister, EntryFrameConstants::kArgvOffset));
    // Load the function pointer into rdi.
    __ movq(rdi, rdx);
#else  // _WIN64
    // GCC parameters in:
    // rdi : entry (ignored)
    // rsi : function
    // rdx : receiver
    // rcx : argc
    // r8  : argv

    __ movq(rdi, rsi);
    // rdi : function

    // Clear the context before we push it when entering the internal frame.
    __ Set(rsi, 0);
    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push the function and receiver and setup the context.
    __ push(rdi);
    __ push(rdx);
    __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

    // Load the number of arguments and setup pointer to the arguments.
    __ movq(rax, rcx);
    __ movq(rbx, r8);
#endif  // _WIN64

    // Current stack contents:
    // [rsp + 2 * kPointerSize ... ] : Internal frame
    // [rsp + kPointerSize]          : function
    // [rsp]                         : receiver
    // Current register contents:
    // rax : argc
    // rbx : argv
    // rsi : context
    // rdi : function

    // Copy arguments to the stack in a loop.
    // Register rbx points to array of pointers to handle locations.
    // Push the values of these handles.
    Label loop, entry;
    __ Set(rcx, 0);  // Set loop variable to 0.
    __ jmp(&entry);
    __ bind(&loop);
    __ movq(kScratchRegister, Operand(rbx, rcx, times_pointer_size, 0));
    __ push(Operand(kScratchRegister, 0));  // dereference handle
    __ addq(rcx, Immediate(1));
    __ bind(&entry);
    __ cmpq(rcx, rax);
    __ j(not_equal, &loop);

    // Invoke the code.
    if (is_construct) {
      // No type feedback cell is available
      Handle<Object> undefined_sentinel(
          masm->isolate()->factory()->undefined_value());
      __ Move(rbx, undefined_sentinel);
      // Expects rdi to hold function pointer.
      CallConstructStub stub(NO_CALL_FUNCTION_FLAGS);
      __ CallStub(&stub);
    } else {
      ParameterCount actual(rax);
      // Function must be in rdi.
      __ InvokeFunction(rdi, actual, CALL_FUNCTION,
                        NullCallWrapper(), CALL_AS_METHOD);
    }
    // Exit the internal frame. Notice that this also removes the empty
    // context and the function left on the stack by the code
    // invocation.
  }

  // TODO(X64): Is argument correct? Is there a receiver to remove?
  __ ret(1 * kPointerSize);  // Remove receiver.
}


void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}


void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}


void Builtins::Generate_LazyCompile(MacroAssembler* masm) {
  CallRuntimePassFunction(masm, Runtime::kLazyCompile);
  // Do a tail-call of the compiled function.
  __ lea(rax, FieldOperand(rax, Code::kHeaderSize));
  __ jmp(rax);
}


void Builtins::Generate_LazyRecompile(MacroAssembler* masm) {
  CallRuntimePassFunction(masm, Runtime::kLazyRecompile);
  // Do a tail-call of the compiled function.
  __ lea(rax, FieldOperand(rax, Code::kHeaderSize));
  __ jmp(rax);
}


static void GenerateMakeCodeYoungAgainCommon(MacroAssembler* masm) {
  // For now, we are relying on the fact that make_code_young doesn't do any
  // garbage collection which allows us to save/restore the registers without
  // worrying about which of them contain pointers. We also don't build an
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Re-execute the code that was patched back to the young age when
  // the stub returns.
  __ subq(Operand(rsp, 0), Immediate(5));
  __ Pushad();
  __ movq(arg_reg_1, Operand(rsp, kNumSafepointRegisters * kPointerSize));
  {  // NOLINT
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(1);
    __ CallCFunction(
        ExternalReference::get_make_code_young_function(masm->isolate()), 1);
  }
  __ Popad();
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


void Builtins::Generate_NotifyStubFailure(MacroAssembler* masm) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Preserve registers across notification, this is important for compiled
    // stubs that tail call the runtime on deopts passing their parameters in
    // registers.
    __ Pushad();
    __ CallRuntime(Runtime::kNotifyStubFailure, 0);
    __ Popad();
    // Tear down internal frame.
  }

  __ pop(MemOperand(rsp, 0));  // Ignore state offset
  __ ret(0);  // Return to IC Miss stub, continuation still on stack.
}


static void Generate_NotifyDeoptimizedHelper(MacroAssembler* masm,
                                             Deoptimizer::BailoutType type) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Pass the deoptimization type to the runtime system.
    __ Push(Smi::FromInt(static_cast<int>(type)));

    __ CallRuntime(Runtime::kNotifyDeoptimized, 1);
    // Tear down internal frame.
  }

  // Get the full codegen state from the stack and untag it.
  __ SmiToInteger32(r10, Operand(rsp, kPCOnStackSize));

  // Switch on the state.
  Label not_no_registers, not_tos_rax;
  __ cmpq(r10, Immediate(FullCodeGenerator::NO_REGISTERS));
  __ j(not_equal, &not_no_registers, Label::kNear);
  __ ret(1 * kPointerSize);  // Remove state.

  __ bind(&not_no_registers);
  __ movq(rax, Operand(rsp, kPCOnStackSize + kPointerSize));
  __ cmpq(r10, Immediate(FullCodeGenerator::TOS_REG));
  __ j(not_equal, &not_tos_rax, Label::kNear);
  __ ret(2 * kPointerSize);  // Remove state, rax.

  __ bind(&not_tos_rax);
  __ Abort(kNoCasesLeft);
}


void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::EAGER);
}


void Builtins::Generate_NotifySoftDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::SOFT);
}


void Builtins::Generate_NotifyLazyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::LAZY);
}


void Builtins::Generate_NotifyOSR(MacroAssembler* masm) {
  // For now, we are relying on the fact that Runtime::NotifyOSR
  // doesn't do any garbage collection which allows us to save/restore
  // the registers without worrying about which of them contain
  // pointers. This seems a bit fragile.
  __ Pushad();
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyOSR, 0);
  }
  __ Popad();
  __ ret(0);
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // Stack Layout:
  // rsp[0]           : Return address
  // rsp[8]           : Argument n
  // rsp[16]          : Argument n-1
  //  ...
  // rsp[8 * n]       : Argument 1
  // rsp[8 * (n + 1)] : Receiver (function to call)
  //
  // rax contains the number of arguments, n, not counting the receiver.
  //
  // 1. Make sure we have at least one argument.
  { Label done;
    __ testq(rax, rax);
    __ j(not_zero, &done);
    __ PopReturnAddressTo(rbx);
    __ Push(masm->isolate()->factory()->undefined_value());
    __ PushReturnAddressFrom(rbx);
    __ incq(rax);
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack, check
  //    if it is a function.
  Label slow, non_function;
  StackArgumentsAccessor args(rsp, rax);
  __ movq(rdi, args.GetReceiverOperand());
  __ JumpIfSmi(rdi, &non_function);
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(not_equal, &slow);

  // 3a. Patch the first argument if necessary when calling a function.
  Label shift_arguments;
  __ Set(rdx, 0);  // indicate regular JS_FUNCTION
  { Label convert_to_object, use_global_receiver, patch_receiver;
    // Change context eagerly in case we need the global receiver.
    __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

    // Do not transform the receiver for strict mode functions.
    __ movq(rbx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ testb(FieldOperand(rbx, SharedFunctionInfo::kStrictModeByteOffset),
             Immediate(1 << SharedFunctionInfo::kStrictModeBitWithinByte));
    __ j(not_equal, &shift_arguments);

    // Do not transform the receiver for natives.
    // SharedFunctionInfo is already loaded into rbx.
    __ testb(FieldOperand(rbx, SharedFunctionInfo::kNativeByteOffset),
             Immediate(1 << SharedFunctionInfo::kNativeBitWithinByte));
    __ j(not_zero, &shift_arguments);

    // Compute the receiver in non-strict mode.
    __ movq(rbx, args.GetArgumentOperand(1));
    __ JumpIfSmi(rbx, &convert_to_object, Label::kNear);

    __ CompareRoot(rbx, Heap::kNullValueRootIndex);
    __ j(equal, &use_global_receiver);
    __ CompareRoot(rbx, Heap::kUndefinedValueRootIndex);
    __ j(equal, &use_global_receiver);

    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CmpObjectType(rbx, FIRST_SPEC_OBJECT_TYPE, rcx);
    __ j(above_equal, &shift_arguments);

    __ bind(&convert_to_object);
    {
      // Enter an internal frame in order to preserve argument count.
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Integer32ToSmi(rax, rax);
      __ push(rax);

      __ push(rbx);
      __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
      __ movq(rbx, rax);
      __ Set(rdx, 0);  // indicate regular JS_FUNCTION

      __ pop(rax);
      __ SmiToInteger32(rax, rax);
    }

    // Restore the function to rdi.
    __ movq(rdi, args.GetReceiverOperand());
    __ jmp(&patch_receiver, Label::kNear);

    // Use the global receiver object from the called function as the
    // receiver.
    __ bind(&use_global_receiver);
    const int kGlobalIndex =
        Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
    __ movq(rbx, FieldOperand(rsi, kGlobalIndex));
    __ movq(rbx, FieldOperand(rbx, GlobalObject::kNativeContextOffset));
    __ movq(rbx, FieldOperand(rbx, kGlobalIndex));
    __ movq(rbx, FieldOperand(rbx, GlobalObject::kGlobalReceiverOffset));

    __ bind(&patch_receiver);
    __ movq(args.GetArgumentOperand(1), rbx);

    __ jmp(&shift_arguments);
  }

  // 3b. Check for function proxy.
  __ bind(&slow);
  __ Set(rdx, 1);  // indicate function proxy
  __ CmpInstanceType(rcx, JS_FUNCTION_PROXY_TYPE);
  __ j(equal, &shift_arguments);
  __ bind(&non_function);
  __ Set(rdx, 2);  // indicate non-function

  // 3c. Patch the first argument when calling a non-function.  The
  //     CALL_NON_FUNCTION builtin expects the non-function callee as
  //     receiver, so overwrite the first argument which will ultimately
  //     become the receiver.
  __ movq(args.GetArgumentOperand(1), rdi);

  // 4. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  __ bind(&shift_arguments);
  { Label loop;
    __ movq(rcx, rax);
    __ bind(&loop);
    __ movq(rbx, Operand(rsp, rcx, times_pointer_size, 0));
    __ movq(Operand(rsp, rcx, times_pointer_size, 1 * kPointerSize), rbx);
    __ decq(rcx);
    __ j(not_sign, &loop);  // While non-negative (to copy return address).
    __ pop(rbx);  // Discard copy of return address.
    __ decq(rax);  // One fewer argument (first argument is new receiver).
  }

  // 5a. Call non-function via tail call to CALL_NON_FUNCTION builtin,
  //     or a function proxy via CALL_FUNCTION_PROXY.
  { Label function, non_proxy;
    __ testq(rdx, rdx);
    __ j(zero, &function);
    __ Set(rbx, 0);
    __ SetCallKind(rcx, CALL_AS_METHOD);
    __ cmpq(rdx, Immediate(1));
    __ j(not_equal, &non_proxy);

    __ PopReturnAddressTo(rdx);
    __ push(rdi);  // re-add proxy object as additional argument
    __ PushReturnAddressFrom(rdx);
    __ incq(rax);
    __ GetBuiltinEntry(rdx, Builtins::CALL_FUNCTION_PROXY);
    __ jmp(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
           RelocInfo::CODE_TARGET);

    __ bind(&non_proxy);
    __ GetBuiltinEntry(rdx, Builtins::CALL_NON_FUNCTION);
    __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);
    __ bind(&function);
  }

  // 5b. Get the code to call from the function and check that the number of
  //     expected arguments matches what we're providing.  If so, jump
  //     (tail-call) to the code in register edx without checking arguments.
  __ movq(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movsxlq(rbx,
             FieldOperand(rdx,
                          SharedFunctionInfo::kFormalParameterCountOffset));
  __ movq(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
  __ SetCallKind(rcx, CALL_AS_METHOD);
  __ cmpq(rax, rbx);
  __ j(not_equal,
       masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
       RelocInfo::CODE_TARGET);

  ParameterCount expected(0);
  __ InvokeCode(rdx, expected, expected, JUMP_FUNCTION,
                NullCallWrapper(), CALL_AS_METHOD);
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  // Stack at entry:
  // rsp     : return address
  // rsp[8]  : arguments
  // rsp[16] : receiver ("this")
  // rsp[24] : function
  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);
    // Stack frame:
    // rbp     : Old base pointer
    // rbp[8]  : return address
    // rbp[16] : function arguments
    // rbp[24] : receiver
    // rbp[32] : function
    static const int kArgumentsOffset = 2 * kPointerSize;
    static const int kReceiverOffset = 3 * kPointerSize;
    static const int kFunctionOffset = 4 * kPointerSize;

    __ push(Operand(rbp, kFunctionOffset));
    __ push(Operand(rbp, kArgumentsOffset));
    __ InvokeBuiltin(Builtins::APPLY_PREPARE, CALL_FUNCTION);

    // Check the stack for overflow. We are not trying to catch
    // interruptions (e.g. debug break and preemption) here, so the "real stack
    // limit" is checked.
    Label okay;
    __ LoadRoot(kScratchRegister, Heap::kRealStackLimitRootIndex);
    __ movq(rcx, rsp);
    // Make rcx the space we have left. The stack might already be overflowed
    // here which will cause rcx to become negative.
    __ subq(rcx, kScratchRegister);
    // Make rdx the space we need for the array when it is unrolled onto the
    // stack.
    __ PositiveSmiTimesPowerOfTwoToInteger64(rdx, rax, kPointerSizeLog2);
    // Check if the arguments will overflow the stack.
    __ cmpq(rcx, rdx);
    __ j(greater, &okay);  // Signed comparison.

    // Out of stack space.
    __ push(Operand(rbp, kFunctionOffset));
    __ push(rax);
    __ InvokeBuiltin(Builtins::APPLY_OVERFLOW, CALL_FUNCTION);
    __ bind(&okay);
    // End of stack check.

    // Push current index and limit.
    const int kLimitOffset =
        StandardFrameConstants::kExpressionsOffset - 1 * kPointerSize;
    const int kIndexOffset = kLimitOffset - 1 * kPointerSize;
    __ push(rax);  // limit
    __ push(Immediate(0));  // index

    // Get the receiver.
    __ movq(rbx, Operand(rbp, kReceiverOffset));

    // Check that the function is a JS function (otherwise it must be a proxy).
    Label push_receiver;
    __ movq(rdi, Operand(rbp, kFunctionOffset));
    __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
    __ j(not_equal, &push_receiver);

    // Change context eagerly to get the right global object if necessary.
    __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

    // Do not transform the receiver for strict mode functions.
    Label call_to_object, use_global_receiver;
    __ movq(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ testb(FieldOperand(rdx, SharedFunctionInfo::kStrictModeByteOffset),
             Immediate(1 << SharedFunctionInfo::kStrictModeBitWithinByte));
    __ j(not_equal, &push_receiver);

    // Do not transform the receiver for natives.
    __ testb(FieldOperand(rdx, SharedFunctionInfo::kNativeByteOffset),
             Immediate(1 << SharedFunctionInfo::kNativeBitWithinByte));
    __ j(not_equal, &push_receiver);

    // Compute the receiver in non-strict mode.
    __ JumpIfSmi(rbx, &call_to_object, Label::kNear);
    __ CompareRoot(rbx, Heap::kNullValueRootIndex);
    __ j(equal, &use_global_receiver);
    __ CompareRoot(rbx, Heap::kUndefinedValueRootIndex);
    __ j(equal, &use_global_receiver);

    // If given receiver is already a JavaScript object then there's no
    // reason for converting it.
    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CmpObjectType(rbx, FIRST_SPEC_OBJECT_TYPE, rcx);
    __ j(above_equal, &push_receiver);

    // Convert the receiver to an object.
    __ bind(&call_to_object);
    __ push(rbx);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ movq(rbx, rax);
    __ jmp(&push_receiver, Label::kNear);

    // Use the current global receiver object as the receiver.
    __ bind(&use_global_receiver);
    const int kGlobalOffset =
        Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
    __ movq(rbx, FieldOperand(rsi, kGlobalOffset));
    __ movq(rbx, FieldOperand(rbx, GlobalObject::kNativeContextOffset));
    __ movq(rbx, FieldOperand(rbx, kGlobalOffset));
    __ movq(rbx, FieldOperand(rbx, GlobalObject::kGlobalReceiverOffset));

    // Push the receiver.
    __ bind(&push_receiver);
    __ push(rbx);

    // Copy all arguments from the array to the stack.
    Label entry, loop;
    __ movq(rax, Operand(rbp, kIndexOffset));
    __ jmp(&entry);
    __ bind(&loop);
    __ movq(rdx, Operand(rbp, kArgumentsOffset));  // load arguments

    // Use inline caching to speed up access to arguments.
    Handle<Code> ic =
        masm->isolate()->builtins()->KeyedLoadIC_Initialize();
    __ Call(ic, RelocInfo::CODE_TARGET);
    // It is important that we do not have a test instruction after the
    // call.  A test instruction after the call is used to indicate that
    // we have generated an inline version of the keyed load.  In this
    // case, we know that we are not generating a test instruction next.

    // Push the nth argument.
    __ push(rax);

    // Update the index on the stack and in register rax.
    __ movq(rax, Operand(rbp, kIndexOffset));
    __ SmiAddConstant(rax, rax, Smi::FromInt(1));
    __ movq(Operand(rbp, kIndexOffset), rax);

    __ bind(&entry);
    __ cmpq(rax, Operand(rbp, kLimitOffset));
    __ j(not_equal, &loop);

    // Invoke the function.
    Label call_proxy;
    ParameterCount actual(rax);
    __ SmiToInteger32(rax, rax);
    __ movq(rdi, Operand(rbp, kFunctionOffset));
    __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
    __ j(not_equal, &call_proxy);
    __ InvokeFunction(rdi, actual, CALL_FUNCTION,
                      NullCallWrapper(), CALL_AS_METHOD);

    frame_scope.GenerateLeaveFrame();
    __ ret(3 * kPointerSize);  // remove this, receiver, and arguments

    // Invoke the function proxy.
    __ bind(&call_proxy);
    __ push(rdi);  // add function proxy as last argument
    __ incq(rax);
    __ Set(rbx, 0);
    __ SetCallKind(rcx, CALL_AS_METHOD);
    __ GetBuiltinEntry(rdx, Builtins::CALL_FUNCTION_PROXY);
    __ call(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);

    // Leave internal frame.
  }
  __ ret(3 * kPointerSize);  // remove this, receiver, and arguments
}


void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argc
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------
  Label generic_array_code;

  // Get the InternalArray function.
  __ LoadGlobalFunction(Context::INTERNAL_ARRAY_FUNCTION_INDEX, rdi);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ movq(rbx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rbx));
    __ Check(not_smi, kUnexpectedInitialMapForInternalArrayFunction);
    __ CmpObjectType(rbx, MAP_TYPE, rcx);
    __ Check(equal, kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // tail call a stub
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void Builtins::Generate_ArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argc
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------
  Label generic_array_code;

  // Get the Array function.
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, rdi);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ movq(rbx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rbx));
    __ Check(not_smi, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(rbx, MAP_TYPE, rcx);
    __ Check(equal, kUnexpectedInitialMapForArrayFunction);
  }

  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  Handle<Object> undefined_sentinel(
      masm->isolate()->heap()->undefined_value(),
      masm->isolate());
  __ Move(rbx, undefined_sentinel);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void Builtins::Generate_StringConstructCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments
  //  -- rdi                 : constructor function
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_ctor_calls(), 1);

  if (FLAG_debug_code) {
    __ LoadGlobalFunction(Context::STRING_FUNCTION_INDEX, rcx);
    __ cmpq(rdi, rcx);
    __ Assert(equal, kUnexpectedStringFunction);
  }

  // Load the first argument into rax and get rid of the rest
  // (including the receiver).
  StackArgumentsAccessor args(rsp, rax);
  Label no_arguments;
  __ testq(rax, rax);
  __ j(zero, &no_arguments);
  __ movq(rbx, args.GetArgumentOperand(1));
  __ PopReturnAddressTo(rcx);
  __ lea(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
  __ PushReturnAddressFrom(rcx);
  __ movq(rax, rbx);

  // Lookup the argument in the number to string cache.
  Label not_cached, argument_is_string;
  NumberToStringStub::GenerateLookupNumberStringCache(
      masm,
      rax,  // Input.
      rbx,  // Result.
      rcx,  // Scratch 1.
      rdx,  // Scratch 2.
      &not_cached);
  __ IncrementCounter(counters->string_ctor_cached_number(), 1);
  __ bind(&argument_is_string);

  // ----------- S t a t e -------------
  //  -- rbx    : argument converted to string
  //  -- rdi    : constructor function
  //  -- rsp[0] : return address
  // -----------------------------------

  // Allocate a JSValue and put the tagged pointer into rax.
  Label gc_required;
  __ Allocate(JSValue::kSize,
              rax,  // Result.
              rcx,  // New allocation top (we ignore it).
              no_reg,
              &gc_required,
              TAG_OBJECT);

  // Set the map.
  __ LoadGlobalFunctionInitialMap(rdi, rcx);
  if (FLAG_debug_code) {
    __ cmpb(FieldOperand(rcx, Map::kInstanceSizeOffset),
            Immediate(JSValue::kSize >> kPointerSizeLog2));
    __ Assert(equal, kUnexpectedStringWrapperInstanceSize);
    __ cmpb(FieldOperand(rcx, Map::kUnusedPropertyFieldsOffset), Immediate(0));
    __ Assert(equal, kUnexpectedUnusedPropertiesOfStringWrapper);
  }
  __ movq(FieldOperand(rax, HeapObject::kMapOffset), rcx);

  // Set properties and elements.
  __ LoadRoot(rcx, Heap::kEmptyFixedArrayRootIndex);
  __ movq(FieldOperand(rax, JSObject::kPropertiesOffset), rcx);
  __ movq(FieldOperand(rax, JSObject::kElementsOffset), rcx);

  // Set the value.
  __ movq(FieldOperand(rax, JSValue::kValueOffset), rbx);

  // Ensure the object is fully initialized.
  STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);

  // We're done. Return.
  __ ret(0);

  // The argument was not found in the number to string cache. Check
  // if it's a string already before calling the conversion builtin.
  Label convert_argument;
  __ bind(&not_cached);
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(rax, &convert_argument);
  Condition is_string = masm->IsObjectStringType(rax, rbx, rcx);
  __ j(NegateCondition(is_string), &convert_argument);
  __ movq(rbx, rax);
  __ IncrementCounter(counters->string_ctor_string_value(), 1);
  __ jmp(&argument_is_string);

  // Invoke the conversion builtin and put the result into rbx.
  __ bind(&convert_argument);
  __ IncrementCounter(counters->string_ctor_conversions(), 1);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(rdi);  // Preserve the function.
    __ push(rax);
    __ InvokeBuiltin(Builtins::TO_STRING, CALL_FUNCTION);
    __ pop(rdi);
  }
  __ movq(rbx, rax);
  __ jmp(&argument_is_string);

  // Load the empty string into rbx, remove the receiver from the
  // stack, and jump back to the case where the argument is a string.
  __ bind(&no_arguments);
  __ LoadRoot(rbx, Heap::kempty_stringRootIndex);
  __ PopReturnAddressTo(rcx);
  __ lea(rsp, Operand(rsp, kPointerSize));
  __ PushReturnAddressFrom(rcx);
  __ jmp(&argument_is_string);

  // At this point the argument is already a string. Call runtime to
  // create a string wrapper.
  __ bind(&gc_required);
  __ IncrementCounter(counters->string_ctor_gc_required(), 1);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(rbx);
    __ CallRuntime(Runtime::kNewStringWrapper, 1);
  }
  __ ret(0);
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ push(rbp);
  __ movq(rbp, rsp);

  // Store the arguments adaptor context sentinel.
  __ Push(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));

  // Push the function on the stack.
  __ push(rdi);

  // Preserve the number of arguments on the stack. Must preserve rax,
  // rbx and rcx because these registers are used when copying the
  // arguments and the receiver.
  __ Integer32ToSmi(r8, rax);
  __ push(r8);
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // Retrieve the number of arguments from the stack. Number is a Smi.
  __ movq(rbx, Operand(rbp, ArgumentsAdaptorFrameConstants::kLengthOffset));

  // Leave the frame.
  __ movq(rsp, rbp);
  __ pop(rbp);

  // Remove caller arguments from the stack.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ lea(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : actual number of arguments
  //  -- rbx : expected number of arguments
  //  -- rcx : call kind information
  //  -- rdx : code entry to call
  // -----------------------------------

  Label invoke, dont_adapt_arguments;
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->arguments_adaptors(), 1);

  Label enough, too_few;
  __ cmpq(rax, rbx);
  __ j(less, &too_few);
  __ cmpq(rbx, Immediate(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ j(equal, &dont_adapt_arguments);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Copy receiver and all expected arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(rax, Operand(rbp, rax, times_pointer_size, offset));
    __ Set(r8, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incq(r8);
    __ push(Operand(rax, 0));
    __ subq(rax, Immediate(kPointerSize));
    __ cmpq(r8, rbx);
    __ j(less, &copy);
    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);

    // Copy receiver and all actual arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(rdi, Operand(rbp, rax, times_pointer_size, offset));
    __ Set(r8, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incq(r8);
    __ push(Operand(rdi, 0));
    __ subq(rdi, Immediate(kPointerSize));
    __ cmpq(r8, rax);
    __ j(less, &copy);

    // Fill remaining expected arguments with undefined values.
    Label fill;
    __ LoadRoot(kScratchRegister, Heap::kUndefinedValueRootIndex);
    __ bind(&fill);
    __ incq(r8);
    __ push(kScratchRegister);
    __ cmpq(r8, rbx);
    __ j(less, &fill);

    // Restore function pointer.
    __ movq(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ call(rdx);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Leave frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ ret(0);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ jmp(rdx);
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ movq(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Lookup and calculate pc offset.
    __ movq(rdx, Operand(rbp, StandardFrameConstants::kCallerPCOffset));
    __ movq(rbx, FieldOperand(rax, JSFunction::kSharedFunctionInfoOffset));
    __ subq(rdx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ subq(rdx, FieldOperand(rbx, SharedFunctionInfo::kCodeOffset));
    __ Integer32ToSmi(rdx, rdx);

    // Pass both function and pc offset as arguments.
    __ push(rax);
    __ push(rdx);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement, 2);
  }

  Label skip;
  // If the code object is null, just return to the unoptimized code.
  __ cmpq(rax, Immediate(0));
  __ j(not_equal, &skip, Label::kNear);
  __ ret(0);

  __ bind(&skip);

  // Load deoptimization data from the code object.
  __ movq(rbx, Operand(rax, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  __ SmiToInteger32(rbx, Operand(rbx, FixedArray::OffsetOfElementAt(
      DeoptimizationInputData::kOsrPcOffsetIndex) - kHeapObjectTag));

  // Compute the target address = code_obj + header_size + osr_offset
  __ lea(rax, Operand(rax, rbx, times_1, Code::kHeaderSize - kHeapObjectTag));

  // Overwrite the return address on the stack.
  __ movq(Operand(rsp, 0), rax);

  // And "return" to the OSR entry point of the function.
  __ ret(0);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64

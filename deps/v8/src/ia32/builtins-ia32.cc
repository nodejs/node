// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/code-factory.h"
#include "src/codegen.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"
#include "src/ia32/frames-ia32.h"

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
  //  -- esp[0]             : return address
  //  -- esp[4]             : last argument
  //  -- ...
  //  -- esp[4 * argc]      : first argument (argc == eax)
  //  -- esp[4 * (argc +1)] : receiver
  // -----------------------------------
  __ AssertFunction(edi);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  // TODO(bmeurer): Can we make this more robust?
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args == NEEDS_CALLED_FUNCTION) {
    num_extra_args = 1;
    Register scratch = ebx;
    __ pop(scratch);  // Save return address.
    __ push(edi);
    __ push(scratch);  // Restore return address.
  } else {
    DCHECK(extra_args == NO_EXTRA_ARGUMENTS);
  }

  // JumpToExternalReference expects eax to contain the number of arguments
  // including the receiver and the extra arguments.
  __ add(eax, Immediate(num_extra_args + 1));
  __ JumpToExternalReference(ExternalReference(id, masm->isolate()));
}


static void CallRuntimePassFunction(
    MacroAssembler* masm, Runtime::FunctionId function_id) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function.
  __ push(edi);
  // Function is also the parameter to the runtime call.
  __ push(edi);

  __ CallRuntime(function_id, 1);
  // Restore receiver.
  __ pop(edi);
}


static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(eax, FieldOperand(eax, SharedFunctionInfo::kCodeOffset));
  __ lea(eax, FieldOperand(eax, Code::kHeaderSize));
  __ jmp(eax);
}


static void GenerateTailCallToReturnedCode(MacroAssembler* masm) {
  __ lea(eax, FieldOperand(eax, Code::kHeaderSize));
  __ jmp(eax);
}


void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(masm->isolate());
  __ cmp(esp, Operand::StaticVariable(stack_limit));
  __ j(above_equal, &ok, Label::kNear);

  CallRuntimePassFunction(masm, Runtime::kTryInstallOptimizedCode);
  GenerateTailCallToReturnedCode(masm);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function) {
  // ----------- S t a t e -------------
  //  -- eax: number of arguments
  //  -- edi: constructor function
  //  -- ebx: allocation site or undefined
  //  -- edx: original constructor
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ AssertUndefinedOrAllocationSite(ebx);
    __ push(ebx);
    __ SmiTag(eax);
    __ push(eax);
    __ push(edi);
    __ push(edx);

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(masm->isolate());
      __ cmp(Operand::StaticVariable(debug_step_in_fp), Immediate(0));
      __ j(not_equal, &rt_call);

      // Fall back to runtime if the original constructor and function differ.
      __ cmp(edx, edi);
      __ j(not_equal, &rt_call);

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

      if (!is_api_function) {
        Label allocate;
        // The code below relies on these assumptions.
        STATIC_ASSERT(Map::Counter::kShift + Map::Counter::kSize == 32);
        // Check if slack tracking is enabled.
        __ mov(esi, FieldOperand(eax, Map::kBitField3Offset));
        __ shr(esi, Map::Counter::kShift);
        __ cmp(esi, Map::kSlackTrackingCounterEnd);
        __ j(less, &allocate);
        // Decrease generous allocation count.
        __ sub(FieldOperand(eax, Map::kBitField3Offset),
               Immediate(1 << Map::Counter::kShift));

        __ cmp(esi, Map::kSlackTrackingCounterEnd);
        __ j(not_equal, &allocate);

        __ push(eax);
        __ push(edx);
        __ push(edi);

        __ push(edi);  // constructor
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);

        __ pop(edi);
        __ pop(edx);
        __ pop(eax);
        __ mov(esi, Map::kSlackTrackingCounterEnd - 1);

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      // edi: constructor
      // eax: initial map
      __ movzx_b(edi, FieldOperand(eax, Map::kInstanceSizeOffset));
      __ shl(edi, kPointerSizeLog2);

      __ Allocate(edi, ebx, edi, no_reg, &rt_call, NO_ALLOCATION_FLAGS);

      Factory* factory = masm->isolate()->factory();

      // Allocated the JSObject, now initialize the fields.
      // eax: initial map
      // ebx: JSObject
      // edi: start of next object
      __ mov(Operand(ebx, JSObject::kMapOffset), eax);
      __ mov(ecx, factory->empty_fixed_array());
      __ mov(Operand(ebx, JSObject::kPropertiesOffset), ecx);
      __ mov(Operand(ebx, JSObject::kElementsOffset), ecx);
      // Set extra fields in the newly allocated object.
      // eax: initial map
      // ebx: JSObject
      // edi: start of next object
      // esi: slack tracking counter (non-API function case)
      __ mov(edx, factory->undefined_value());
      __ lea(ecx, Operand(ebx, JSObject::kHeaderSize));
      if (!is_api_function) {
        Label no_inobject_slack_tracking;

        // Check if slack tracking is enabled.
        __ cmp(esi, Map::kSlackTrackingCounterEnd);
        __ j(less, &no_inobject_slack_tracking);

        // Allocate object with a slack.
        __ movzx_b(
            esi,
            FieldOperand(
                eax, Map::kInObjectPropertiesOrConstructorFunctionIndexOffset));
        __ movzx_b(eax, FieldOperand(eax, Map::kUnusedPropertyFieldsOffset));
        __ sub(esi, eax);
        __ lea(esi,
               Operand(ebx, esi, times_pointer_size, JSObject::kHeaderSize));
        // esi: offset of first field after pre-allocated fields
        if (FLAG_debug_code) {
          __ cmp(esi, edi);
          __ Assert(less_equal,
                    kUnexpectedNumberOfPreAllocatedPropertyFields);
        }
        __ InitializeFieldsWithFiller(ecx, esi, edx);
        __ mov(edx, factory->one_pointer_filler_map());
        // Fill the remaining fields with one pointer filler map.

        __ bind(&no_inobject_slack_tracking);
      }

      __ InitializeFieldsWithFiller(ecx, edi, edx);

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on.
      // ebx: JSObject (untagged)
      __ or_(ebx, Immediate(kHeapObjectTag));

      // Continue with JSObject being successfully allocated
      // ebx: JSObject (tagged)
      __ jmp(&allocated);
    }

    // Allocate the new receiver object using the runtime call.
    // edx: original constructor
    __ bind(&rt_call);
    int offset = kPointerSize;

    // Must restore esi (context) and edi (constructor) before calling
    // runtime.
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
    __ mov(edi, Operand(esp, offset));
    __ push(edi);  // argument 2/1: constructor function
    __ push(edx);  // argument 3/2: original constructor
    __ CallRuntime(Runtime::kNewObject, 2);
    __ mov(ebx, eax);  // store result in ebx

    // New object allocated.
    // ebx: newly allocated object
    __ bind(&allocated);

    // Restore the parameters.
    __ pop(edx);  // new.target
    __ pop(edi);  // Constructor function.

    // Retrieve smi-tagged arguments count from the stack.
    __ mov(eax, Operand(esp, 0));
    __ SmiUntag(eax);

    // Push new.target onto the construct frame. This is stored just below the
    // receiver on the stack.
    __ push(edx);

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
      __ call(code, RelocInfo::CODE_TARGET);
    } else {
      ParameterCount actual(eax);
      __ InvokeFunction(edi, actual, CALL_FUNCTION,
                        NullCallWrapper());
    }

    // Store offset of return address for deoptimizer.
    if (!is_api_function) {
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

    // Restore the arguments count and leave the construct frame. The arguments
    // count is stored below the reciever and the new.target.
    __ bind(&exit);
    __ mov(ebx, Operand(esp, 2 * kPointerSize));

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


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true);
}


void Builtins::Generate_JSConstructStubForDerived(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax: number of arguments
  //  -- edi: constructor function
  //  -- ebx: allocation site or undefined
  //  -- edx: original constructor
  // -----------------------------------

  {
    FrameScope frame_scope(masm, StackFrame::CONSTRUCT);

    // Preserve allocation site.
    __ AssertUndefinedOrAllocationSite(ebx);
    __ push(ebx);

    // Preserve actual arguments count.
    __ SmiTag(eax);
    __ push(eax);
    __ SmiUntag(eax);

    // Push new.target.
    __ push(edx);

    // receiver is the hole.
    __ push(Immediate(masm->isolate()->factory()->the_hole_value()));

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

    // Handle step in.
    Label skip_step_in;
    ExternalReference debug_step_in_fp =
        ExternalReference::debug_step_in_fp_address(masm->isolate());
    __ cmp(Operand::StaticVariable(debug_step_in_fp), Immediate(0));
    __ j(equal, &skip_step_in);

    __ push(eax);
    __ push(edi);
    __ push(edi);
    __ CallRuntime(Runtime::kHandleStepInForDerivedConstructors, 1);
    __ pop(edi);
    __ pop(eax);

    __ bind(&skip_step_in);

    // Invoke function.
    ParameterCount actual(eax);
    __ InvokeFunction(edi, actual, CALL_FUNCTION, NullCallWrapper());

    // Restore context from the frame.
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));

    // Get arguments count, skipping over new.target.
    __ mov(ebx, Operand(esp, kPointerSize));
  }

  __ pop(ecx);  // Return address.
  __ lea(esp, Operand(esp, ebx, times_2, 1 * kPointerSize));
  __ push(ecx);
  __ ret(0);
}


enum IsTagged { kEaxIsSmiTagged, kEaxIsUntaggedInt };


// Clobbers ecx, edx, edi; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm,
                                        IsTagged eax_is_tagged) {
  // eax   : the number of items to be pushed to the stack
  //
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
  int smi_tag = eax_is_tagged == kEaxIsSmiTagged ? kSmiTagSize : 0;
  __ shl(edx, kPointerSizeLog2 - smi_tag);
  // Check if the arguments will overflow the stack.
  __ cmp(ecx, edx);
  __ j(greater, &okay);  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow, 0);

  __ bind(&okay);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Clear the context before we push it when entering the internal frame.
  __ Move(esi, Immediate(0));

  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ mov(esi, Operand::StaticVariable(context_address));

    // Load the previous frame pointer (ebx) to access C arguments
    __ mov(ebx, Operand(ebp, 0));

    // Push the function and the receiver onto the stack.
    __ push(Operand(ebx, EntryFrameConstants::kFunctionArgOffset));
    __ push(Operand(ebx, EntryFrameConstants::kReceiverArgOffset));

    // Load the number of arguments and setup pointer to the arguments.
    __ mov(eax, Operand(ebx, EntryFrameConstants::kArgcOffset));
    __ mov(ebx, Operand(ebx, EntryFrameConstants::kArgvOffset));

    // Check if we have enough stack space to push all arguments.
    // Expects argument count in eax. Clobbers ecx, edx, edi.
    Generate_CheckStackOverflow(masm, kEaxIsUntaggedInt);

    // Copy arguments to the stack in a loop.
    Label loop, entry;
    __ Move(ecx, Immediate(0));
    __ jmp(&entry, Label::kNear);
    __ bind(&loop);
    __ mov(edx, Operand(ebx, ecx, times_4, 0));  // push parameter from argv
    __ push(Operand(edx, 0));  // dereference handle
    __ inc(ecx);
    __ bind(&entry);
    __ cmp(ecx, eax);
    __ j(not_equal, &loop);

    // Load the previous frame pointer (ebx) to access C arguments
    __ mov(ebx, Operand(ebp, 0));

    // Get the new.target and function from the frame.
    __ mov(edx, Operand(ebx, EntryFrameConstants::kNewTargetArgOffset));
    __ mov(edi, Operand(ebx, EntryFrameConstants::kFunctionArgOffset));

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? masm->isolate()->builtins()->Construct()
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

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


// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o edi: the JS function object being called
//   o esi: our context
//   o ebp: the caller's frame pointer
//   o esp: stack pointer (pointing to return address)
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-ia32.h for its layout.
// TODO(rmcilroy): We will need to include the current bytecode pointer in the
// frame.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ push(ebp);  // Caller's frame pointer.
  __ mov(ebp, esp);
  __ push(esi);  // Callee's context.
  __ push(edi);  // Callee's JS function.

  // Get the bytecode array from the function object and load the pointer to the
  // first entry into edi (InterpreterBytecodeRegister).
  __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(kInterpreterBytecodeArrayRegister,
         FieldOperand(eax, SharedFunctionInfo::kFunctionDataOffset));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     eax);
    __ Assert(equal, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    __ mov(ebx, FieldOperand(kInterpreterBytecodeArrayRegister,
                             BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ mov(ecx, esp);
    __ sub(ecx, ebx);
    ExternalReference stack_limit =
        ExternalReference::address_of_real_stack_limit(masm->isolate());
    __ cmp(ecx, Operand::StaticVariable(stack_limit));
    __ j(above_equal, &ok);
    __ CallRuntime(Runtime::kThrowStackOverflow, 0);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ mov(eax, Immediate(masm->isolate()->factory()->undefined_value()));
    __ jmp(&loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(eax);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ sub(ebx, Immediate(kPointerSize));
    __ j(greater_equal, &loop_header);
  }

  // TODO(rmcilroy): List of things not currently dealt with here but done in
  // fullcodegen's prologue:
  //  - Support profiler (specifically profiling_counter).
  //  - Call ProfileEntryHookStub when isolate has a function_entry_hook.
  //  - Allow simulator stop operations if FLAG_stop_at is set.
  //  - Deal with sloppy mode functions which need to replace the
  //    receiver with the global proxy when called as functions (without an
  //    explicit receiver object).
  //  - Code aging of the BytecodeArray object.
  //  - Supporting FLAG_trace.
  //
  // The following items are also not done here, and will probably be done using
  // explicit bytecodes instead:
  //  - Allocating a new local context if applicable.
  //  - Setting up a local binding to the this function, which is used in
  //    derived constructors with super calls.
  //  - Setting new.target if required.
  //  - Dealing with REST parameters (only if
  //    https://codereview.chromium.org/1235153006 doesn't land by then).
  //  - Dealing with argument objects.

  // Perform stack guard check.
  {
    Label ok;
    ExternalReference stack_limit =
        ExternalReference::address_of_stack_limit(masm->isolate());
    __ cmp(esp, Operand::StaticVariable(stack_limit));
    __ j(above_equal, &ok);
    __ CallRuntime(Runtime::kStackGuard, 0);
    __ bind(&ok);
  }

  // Load accumulator, register file, bytecode offset, dispatch table into
  // registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ mov(kInterpreterRegisterFileRegister, ebp);
  __ sub(
      kInterpreterRegisterFileRegister,
      Immediate(kPointerSize + StandardFrameConstants::kFixedFrameSizeFromFp));
  __ mov(kInterpreterBytecodeOffsetRegister,
         Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag));
  // Since the dispatch table root might be set after builtins are generated,
  // load directly from the roots table.
  __ LoadRoot(kInterpreterDispatchTableRegister,
              Heap::kInterpreterTableRootIndex);
  __ add(kInterpreterDispatchTableRegister,
         Immediate(FixedArray::kHeaderSize - kHeapObjectTag));

  // Push context as a stack located parameter to the bytecode handler.
  DCHECK_EQ(-1, kInterpreterContextSpillSlot);
  __ push(esi);

  // Dispatch to the first bytecode handler for the function.
  __ movzx_b(esi, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ mov(esi, Operand(kInterpreterDispatchTableRegister, esi,
                      times_pointer_size, 0));
  // TODO(rmcilroy): Make dispatch table point to code entrys to avoid untagging
  // and header removal.
  __ add(esi, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(esi);
}


void Builtins::Generate_InterpreterExitTrampoline(MacroAssembler* masm) {
  // TODO(rmcilroy): List of things not currently dealt with here but done in
  // fullcodegen's EmitReturnSequence.
  //  - Supporting FLAG_trace for Runtime::TraceExit.
  //  - Support profiler (specifically decrementing profiling_counter
  //    appropriately and calling out to HandleInterrupts if necessary).

  // The return value is in accumulator, which is already in rax.

  // Leave the frame (also dropping the register file).
  __ leave();

  // Drop receiver + arguments and return.
  __ mov(ebx, FieldOperand(kInterpreterBytecodeArrayRegister,
                           BytecodeArray::kParameterSizeOffset));
  __ pop(ecx);
  __ add(esp, ebx);
  __ push(ecx);
  __ ret(0);
}


void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  CallRuntimePassFunction(masm, Runtime::kCompileLazy);
  GenerateTailCallToReturnedCode(masm);
}



static void CallCompileOptimized(MacroAssembler* masm, bool concurrent) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function.
  __ push(edi);
  // Function is also the parameter to the runtime call.
  __ push(edi);
  // Whether to compile in a background thread.
  __ Push(masm->isolate()->factory()->ToBoolean(concurrent));

  __ CallRuntime(Runtime::kCompileOptimized, 2);
  // Restore receiver.
  __ pop(edi);
}


void Builtins::Generate_CompileOptimized(MacroAssembler* masm) {
  CallCompileOptimized(masm, false);
  GenerateTailCallToReturnedCode(masm);
}


void Builtins::Generate_CompileOptimizedConcurrent(MacroAssembler* masm) {
  CallCompileOptimized(masm, true);
  GenerateTailCallToReturnedCode(masm);
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
    __ PrepareCallCFunction(2, ebx);
    __ mov(Operand(esp, 1 * kPointerSize),
           Immediate(ExternalReference::isolate_address(masm->isolate())));
    __ mov(Operand(esp, 0), eax);
    __ CallCFunction(
        ExternalReference::get_make_code_young_function(masm->isolate()), 2);
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


void Builtins::Generate_MarkCodeAsExecutedOnce(MacroAssembler* masm) {
  // For now, as in GenerateMakeCodeYoungAgainCommon, we are relying on the fact
  // that make_code_young doesn't do any garbage collection which allows us to
  // save/restore the registers without worrying about which of them contain
  // pointers.
  __ pushad();
  __ mov(eax, Operand(esp, 8 * kPointerSize));
  __ sub(eax, Immediate(Assembler::kCallInstructionLength));
  {  // NOLINT
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(2, ebx);
    __ mov(Operand(esp, 1 * kPointerSize),
           Immediate(ExternalReference::isolate_address(masm->isolate())));
    __ mov(Operand(esp, 0), eax);
    __ CallCFunction(
        ExternalReference::get_mark_code_as_executed_function(masm->isolate()),
        2);
  }
  __ popad();

  // Perform prologue operations usually performed by the young code stub.
  __ pop(eax);   // Pop return address into scratch register.
  __ push(ebp);  // Caller's frame pointer.
  __ mov(ebp, esp);
  __ push(esi);  // Callee's context.
  __ push(edi);  // Callee's JS Function.
  __ push(eax);  // Push return address after frame prologue.

  // Jump to point after the code-age stub.
  __ ret(0);
}


void Builtins::Generate_MarkCodeAsExecutedTwice(MacroAssembler* masm) {
  GenerateMakeCodeYoungAgainCommon(masm);
}


void Builtins::Generate_MarkCodeAsToBeExecutedOnce(MacroAssembler* masm) {
  Generate_MarkCodeAsExecutedOnce(masm);
}


static void Generate_NotifyStubFailureHelper(MacroAssembler* masm,
                                             SaveFPRegsMode save_doubles) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Preserve registers across notification, this is important for compiled
    // stubs that tail call the runtime on deopts passing their parameters in
    // registers.
    __ pushad();
    __ CallRuntime(Runtime::kNotifyStubFailure, 0, save_doubles);
    __ popad();
    // Tear down internal frame.
  }

  __ pop(MemOperand(esp, 0));  // Ignore state offset
  __ ret(0);  // Return to IC Miss stub, continuation still on stack.
}


void Builtins::Generate_NotifyStubFailure(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kDontSaveFPRegs);
}


void Builtins::Generate_NotifyStubFailureSaveDoubles(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kSaveFPRegs);
}


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


// static
void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // Stack Layout:
  // esp[0]           : Return address
  // esp[8]           : Argument n
  // esp[16]          : Argument n-1
  //  ...
  // esp[8 * n]       : Argument 1
  // esp[8 * (n + 1)] : Receiver (callable to call)
  //
  // eax contains the number of arguments, n, not counting the receiver.
  //
  // 1. Make sure we have at least one argument.
  {
    Label done;
    __ test(eax, eax);
    __ j(not_zero, &done, Label::kNear);
    __ PopReturnAddressTo(ebx);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ PushReturnAddressFrom(ebx);
    __ inc(eax);
    __ bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  __ mov(edi, Operand(esp, eax, times_pointer_size, kPointerSize));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  {
    Label loop;
    __ mov(ecx, eax);
    __ bind(&loop);
    __ mov(ebx, Operand(esp, ecx, times_pointer_size, 0));
    __ mov(Operand(esp, ecx, times_pointer_size, kPointerSize), ebx);
    __ dec(ecx);
    __ j(not_sign, &loop);  // While non-negative (to copy return address).
    __ pop(ebx);            // Discard copy of return address.
    __ dec(eax);  // One fewer argument (first argument is new receiver).
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


static void Generate_PushAppliedArguments(MacroAssembler* masm,
                                          const int vectorOffset,
                                          const int argumentsOffset,
                                          const int indexOffset,
                                          const int limitOffset) {
  // Copy all arguments from the array to the stack.
  Label entry, loop;
  Register receiver = LoadDescriptor::ReceiverRegister();
  Register key = LoadDescriptor::NameRegister();
  Register slot = LoadDescriptor::SlotRegister();
  Register vector = LoadWithVectorDescriptor::VectorRegister();
  __ mov(key, Operand(ebp, indexOffset));
  __ jmp(&entry);
  __ bind(&loop);
  __ mov(receiver, Operand(ebp, argumentsOffset));  // load arguments

  // Use inline caching to speed up access to arguments.
  int slot_index = TypeFeedbackVector::PushAppliedArgumentsIndex();
  __ mov(slot, Immediate(Smi::FromInt(slot_index)));
  __ mov(vector, Operand(ebp, vectorOffset));
  Handle<Code> ic =
      KeyedLoadICStub(masm->isolate(), LoadICState(kNoExtraICState)).GetCode();
  __ call(ic, RelocInfo::CODE_TARGET);
  // It is important that we do not have a test instruction after the
  // call.  A test instruction after the call is used to indicate that
  // we have generated an inline version of the keyed load.  In this
  // case, we know that we are not generating a test instruction next.

  // Push the nth argument.
  __ push(eax);

  // Update the index on the stack and in register key.
  __ mov(key, Operand(ebp, indexOffset));
  __ add(key, Immediate(1 << kSmiTagSize));
  __ mov(Operand(ebp, indexOffset), key);

  __ bind(&entry);
  __ cmp(key, Operand(ebp, limitOffset));
  __ j(not_equal, &loop);

  // On exit, the pushed arguments count is in eax, untagged
  __ Move(eax, key);
  __ SmiUntag(eax);
}


// Used by FunctionApply and ReflectApply
static void Generate_ApplyHelper(MacroAssembler* masm, bool targetIsArgument) {
  const int kFormalParameters = targetIsArgument ? 3 : 2;
  const int kStackSize = kFormalParameters + 1;

  // Stack at entry:
  // esp     : return address
  // esp[4]  : arguments
  // esp[8]  : receiver ("this")
  // esp[12] : function
  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);
    // Stack frame:
    // ebp     : Old base pointer
    // ebp[4]  : return address
    // ebp[8]  : function arguments
    // ebp[12] : receiver
    // ebp[16] : function
    static const int kArgumentsOffset = kFPOnStackSize + kPCOnStackSize;
    static const int kReceiverOffset = kArgumentsOffset + kPointerSize;
    static const int kFunctionOffset = kReceiverOffset + kPointerSize;
    static const int kVectorOffset =
        InternalFrameConstants::kCodeOffset - 1 * kPointerSize;

    // Push the vector.
    __ mov(edi, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ mov(edi, FieldOperand(edi, SharedFunctionInfo::kFeedbackVectorOffset));
    __ push(edi);

    __ push(Operand(ebp, kFunctionOffset));   // push this
    __ push(Operand(ebp, kArgumentsOffset));  // push arguments
    if (targetIsArgument) {
      __ InvokeBuiltin(Context::REFLECT_APPLY_PREPARE_BUILTIN_INDEX,
                       CALL_FUNCTION);
    } else {
      __ InvokeBuiltin(Context::APPLY_PREPARE_BUILTIN_INDEX, CALL_FUNCTION);
    }

    Generate_CheckStackOverflow(masm, kEaxIsSmiTagged);

    // Push current index and limit.
    const int kLimitOffset = kVectorOffset - 1 * kPointerSize;
    const int kIndexOffset = kLimitOffset - 1 * kPointerSize;
    __ Push(eax);                            // limit
    __ Push(Immediate(0));                   // index
    __ Push(Operand(ebp, kReceiverOffset));  // receiver

    // Loop over the arguments array, pushing each value to the stack
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // Call the callable.
    // TODO(bmeurer): This should be a tail call according to ES6.
    __ mov(edi, Operand(ebp, kFunctionOffset));
    __ Call(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);

    // Leave internal frame.
  }
  __ ret(kStackSize * kPointerSize);  // remove this, receiver, and arguments
}


// Used by ReflectConstruct
static void Generate_ConstructHelper(MacroAssembler* masm) {
  const int kFormalParameters = 3;
  const int kStackSize = kFormalParameters + 1;

  // Stack at entry:
  // esp     : return address
  // esp[4]  : original constructor (new.target)
  // esp[8]  : arguments
  // esp[16] : constructor
  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);
    // Stack frame:
    // ebp     : Old base pointer
    // ebp[4]  : return address
    // ebp[8]  : original constructor (new.target)
    // ebp[12] : arguments
    // ebp[16] : constructor
    static const int kNewTargetOffset = kFPOnStackSize + kPCOnStackSize;
    static const int kArgumentsOffset = kNewTargetOffset + kPointerSize;
    static const int kFunctionOffset = kArgumentsOffset + kPointerSize;
    static const int kVectorOffset =
        InternalFrameConstants::kCodeOffset - 1 * kPointerSize;

    // Push the vector.
    __ mov(edi, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ mov(edi, FieldOperand(edi, SharedFunctionInfo::kFeedbackVectorOffset));
    __ push(edi);

    // If newTarget is not supplied, set it to constructor
    Label validate_arguments;
    __ mov(eax, Operand(ebp, kNewTargetOffset));
    __ CompareRoot(eax, Heap::kUndefinedValueRootIndex);
    __ j(not_equal, &validate_arguments, Label::kNear);
    __ mov(eax, Operand(ebp, kFunctionOffset));
    __ mov(Operand(ebp, kNewTargetOffset), eax);

    // Validate arguments
    __ bind(&validate_arguments);
    __ push(Operand(ebp, kFunctionOffset));
    __ push(Operand(ebp, kArgumentsOffset));
    __ push(Operand(ebp, kNewTargetOffset));
    __ InvokeBuiltin(Context::REFLECT_CONSTRUCT_PREPARE_BUILTIN_INDEX,
                     CALL_FUNCTION);

    Generate_CheckStackOverflow(masm, kEaxIsSmiTagged);

    // Push current index and limit.
    const int kLimitOffset = kVectorOffset - 1 * kPointerSize;
    const int kIndexOffset = kLimitOffset - 1 * kPointerSize;
    __ Push(eax);  // limit
    __ push(Immediate(0));  // index
    // Push the constructor function as callee.
    __ push(Operand(ebp, kFunctionOffset));

    // Loop over the arguments array, pushing each value to the stack
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // Use undefined feedback vector
    __ LoadRoot(ebx, Heap::kUndefinedValueRootIndex);
    __ mov(edi, Operand(ebp, kFunctionOffset));
    __ mov(ecx, Operand(ebp, kNewTargetOffset));

    // Call the function.
    CallConstructStub stub(masm->isolate(), SUPER_CONSTRUCTOR_CALL);
    __ call(stub.GetCode(), RelocInfo::CONSTRUCT_CALL);

    // Leave internal frame.
  }
  // remove this, target, arguments, and newTarget
  __ ret(kStackSize * kPointerSize);
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  Generate_ApplyHelper(masm, false);
}


void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  Generate_ApplyHelper(masm, true);
}


void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  Generate_ConstructHelper(masm);
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
    __ Assert(not_zero, kUnexpectedInitialMapForInternalArrayFunction);
    __ CmpObjectType(ebx, MAP_TYPE, ecx);
    __ Assert(equal, kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // tail call a stub
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
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
  __ mov(edx, edi);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array function should be a map.
    __ mov(ebx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ test(ebx, Immediate(kSmiTagMask));
    __ Assert(not_zero, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(ebx, MAP_TYPE, ecx);
    __ Assert(equal, kUnexpectedInitialMapForArrayFunction);
  }

  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ mov(ebx, masm->isolate()->factory()->undefined_value());
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax                 : number of arguments
  //  -- edi                 : constructor function
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // 1. Load the first argument into eax and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    __ test(eax, eax);
    __ j(zero, &no_arguments, Label::kNear);
    __ mov(ebx, Operand(esp, eax, times_pointer_size, 0));
    __ PopReturnAddressTo(ecx);
    __ lea(esp, Operand(esp, eax, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(ecx);
    __ mov(eax, ebx);
  }

  // 2a. At least one argument, return eax if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(eax, &to_string, Label::kNear);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, edx);
    __ j(above, &to_string, Label::kNear);
    __ j(equal, &symbol_descriptive_string, Label::kNear);
    __ Ret();
  }

  // 2b. No arguments, return the empty string (and pop the receiver).
  __ bind(&no_arguments);
  {
    __ LoadRoot(eax, Heap::kempty_stringRootIndex);
    __ ret(1 * kPointerSize);
  }

  // 3a. Convert eax to a string.
  __ bind(&to_string);
  {
    ToStringStub stub(masm->isolate());
    __ TailCallStub(&stub);
  }

  // 3b. Convert symbol in eax to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ PopReturnAddressTo(ecx);
    __ Push(eax);
    __ PushReturnAddressFrom(ecx);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString, 1, 1);
  }
}


// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax                 : number of arguments
  //  -- edi                 : constructor function
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // 1. Load the first argument into ebx and get rid of the rest (including the
  // receiver).
  {
    Label no_arguments, done;
    __ test(eax, eax);
    __ j(zero, &no_arguments, Label::kNear);
    __ mov(ebx, Operand(esp, eax, times_pointer_size, 0));
    __ jmp(&done, Label::kNear);
    __ bind(&no_arguments);
    __ LoadRoot(ebx, Heap::kempty_stringRootIndex);
    __ bind(&done);
    __ PopReturnAddressTo(ecx);
    __ lea(esp, Operand(esp, eax, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(ecx);
  }

  // 2. Make sure ebx is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(ebx, &convert, Label::kNear);
    __ CmpObjectType(ebx, FIRST_NONSTRING_TYPE, edx);
    __ j(below, &done_convert);
    __ bind(&convert);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      ToStringStub stub(masm->isolate());
      __ Push(edi);
      __ Move(eax, ebx);
      __ CallStub(&stub);
      __ Move(ebx, eax);
      __ Pop(edi);
    }
    __ bind(&done_convert);
  }

  // 3. Allocate a JSValue wrapper for the string.
  {
    // ----------- S t a t e -------------
    //  -- ebx : the first argument
    //  -- edi : constructor function
    // -----------------------------------

    Label allocate, done_allocate;
    __ Allocate(JSValue::kSize, eax, ecx, no_reg, &allocate, TAG_OBJECT);
    __ bind(&done_allocate);

    // Initialize the JSValue in eax.
    __ LoadGlobalFunctionInitialMap(edi, ecx);
    __ mov(FieldOperand(eax, HeapObject::kMapOffset), ecx);
    __ mov(FieldOperand(eax, JSObject::kPropertiesOffset),
           masm->isolate()->factory()->empty_fixed_array());
    __ mov(FieldOperand(eax, JSObject::kElementsOffset),
           masm->isolate()->factory()->empty_fixed_array());
    __ mov(FieldOperand(eax, JSValue::kValueOffset), ebx);
    STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);
    __ Ret();

    // Fallback to the runtime to allocate in new space.
    __ bind(&allocate);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(ebx);
      __ Push(edi);
      __ Push(Smi::FromInt(JSValue::kSize));
      __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
      __ Pop(edi);
      __ Pop(ebx);
    }
    __ jmp(&done_allocate);
  }
}


static void ArgumentsAdaptorStackCheck(MacroAssembler* masm,
                                       Label* stack_overflow) {
  // ----------- S t a t e -------------
  //  -- eax : actual number of arguments
  //  -- ebx : expected number of arguments
  //  -- edi : function (passed through to callee)
  // -----------------------------------
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  ExternalReference real_stack_limit =
      ExternalReference::address_of_real_stack_limit(masm->isolate());
  __ mov(edx, Operand::StaticVariable(real_stack_limit));
  // Make ecx the space we have left. The stack might already be overflowed
  // here which will cause ecx to become negative.
  __ mov(ecx, esp);
  __ sub(ecx, edx);
  // Make edx the space we need for the array when it is unrolled onto the
  // stack.
  __ mov(edx, ebx);
  __ shl(edx, kPointerSizeLog2);
  // Check if the arguments will overflow the stack.
  __ cmp(ecx, edx);
  __ j(less_equal, stack_overflow);  // Signed comparison.
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


// static
void Builtins::Generate_CallFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the function to call (checked to be a JSFunction)
  // -----------------------------------

  Label convert, convert_global_proxy, convert_to_object, done_convert;
  __ AssertFunction(edi);
  // TODO(bmeurer): Throw a TypeError if function's [[FunctionKind]] internal
  // slot is "classConstructor".
  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  __ test_b(FieldOperand(edx, SharedFunctionInfo::kNativeByteOffset),
            (1 << SharedFunctionInfo::kNativeBitWithinByte) |
                (1 << SharedFunctionInfo::kStrictModeBitWithinByte));
  __ j(not_zero, &done_convert);
  {
    __ mov(ecx, Operand(esp, eax, times_pointer_size, kPointerSize));

    // ----------- S t a t e -------------
    //  -- eax : the number of arguments (not including the receiver)
    //  -- ecx : the receiver
    //  -- edx : the shared function info.
    //  -- edi : the function to call (checked to be a JSFunction)
    //  -- esi : the function context.
    // -----------------------------------

    Label convert_receiver;
    __ JumpIfSmi(ecx, &convert_to_object, Label::kNear);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CmpObjectType(ecx, FIRST_JS_RECEIVER_TYPE, ebx);
    __ j(above_equal, &done_convert);
    __ JumpIfRoot(ecx, Heap::kUndefinedValueRootIndex, &convert_global_proxy,
                  Label::kNear);
    __ JumpIfNotRoot(ecx, Heap::kNullValueRootIndex, &convert_to_object,
                     Label::kNear);
    __ bind(&convert_global_proxy);
    {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(ecx);
    }
    __ jmp(&convert_receiver);
    __ bind(&convert_to_object);
    {
      // Convert receiver using ToObject.
      // TODO(bmeurer): Inline the allocation here to avoid building the frame
      // in the fast case? (fall back to AllocateInNewSpace?)
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(eax);
      __ Push(eax);
      __ Push(edi);
      __ mov(eax, ecx);
      ToObjectStub stub(masm->isolate());
      __ CallStub(&stub);
      __ mov(ecx, eax);
      __ Pop(edi);
      __ Pop(eax);
      __ SmiUntag(eax);
    }
    __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ bind(&convert_receiver);
    __ mov(Operand(esp, eax, times_pointer_size, kPointerSize), ecx);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the shared function info.
  //  -- edi : the function to call (checked to be a JSFunction)
  //  -- esi : the function context.
  // -----------------------------------

  __ mov(ebx,
         FieldOperand(edx, SharedFunctionInfo::kFormalParameterCountOffset));
  __ SmiUntag(ebx);
  ParameterCount actual(eax);
  ParameterCount expected(ebx);
  __ InvokeCode(FieldOperand(edi, JSFunction::kCodeEntryOffset), expected,
                actual, JUMP_FUNCTION, NullCallWrapper());
}


// static
void Builtins::Generate_Call(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(edi, &non_callable);
  __ bind(&non_smi);
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(equal, masm->isolate()->builtins()->CallFunction(),
       RelocInfo::CODE_TARGET);
  __ CmpInstanceType(ecx, JS_FUNCTION_PROXY_TYPE);
  __ j(not_equal, &non_function);

  // 1. Call to function proxy.
  // TODO(neis): This doesn't match the ES6 spec for [[Call]] on proxies.
  __ mov(edi, FieldOperand(edi, JSFunctionProxy::kCallTrapOffset));
  __ AssertNotSmi(edi);
  __ jmp(&non_smi);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Check if target has a [[Call]] internal method.
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset), 1 << Map::kIsCallable);
  __ j(zero, &non_callable, Label::kNear);
  // Overwrite the original receiver with the (original) target.
  __ mov(Operand(esp, eax, times_pointer_size, kPointerSize), edi);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadGlobalFunction(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, edi);
  __ Jump(masm->isolate()->builtins()->CallFunction(), RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(edi);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the original constructor (checked to be a JSFunction)
  //  -- edi : the constructor to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(edx);
  __ AssertFunction(edi);

  // Calling convention for function specific ConstructStubs require
  // ebx to contain either an AllocationSite or undefined.
  __ LoadRoot(ebx, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ecx, FieldOperand(ecx, SharedFunctionInfo::kConstructStubOffset));
  __ lea(ecx, FieldOperand(ecx, Code::kHeaderSize));
  __ jmp(ecx);
}


// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the original constructor (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- edi : the constructor to call (checked to be a JSFunctionProxy)
  // -----------------------------------

  // TODO(neis): This doesn't match the ES6 spec for [[Construct]] on proxies.
  __ mov(edi, FieldOperand(edi, JSFunctionProxy::kConstructTrapOffset));
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the original constructor (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- edi : the constructor to call (can be any Object)
  // -----------------------------------

  // Check if target has a [[Construct]] internal method.
  Label non_constructor;
  __ JumpIfSmi(edi, &non_constructor, Label::kNear);
  __ mov(ecx, FieldOperand(edi, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset), 1 << Map::kIsConstructor);
  __ j(zero, &non_constructor, Label::kNear);

  // Dispatch based on instance type.
  __ CmpInstanceType(ecx, JS_FUNCTION_TYPE);
  __ j(equal, masm->isolate()->builtins()->ConstructFunction(),
       RelocInfo::CODE_TARGET);
  __ CmpInstanceType(ecx, JS_FUNCTION_PROXY_TYPE);
  __ j(equal, masm->isolate()->builtins()->ConstructProxy(),
       RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ mov(Operand(esp, eax, times_pointer_size, kPointerSize), edi);
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadGlobalFunction(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, edi);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(edi);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


// static
void Builtins::Generate_PushArgsAndCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- ebx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  //  -- edi : the target to call (can be any Object).

  // Pop return address to allow tail-call after pushing arguments.
  __ Pop(edx);

  // Find the address of the last argument.
  __ mov(ecx, eax);
  __ add(ecx, Immediate(1));  // Add one for receiver.
  __ shl(ecx, kPointerSizeLog2);
  __ neg(ecx);
  __ add(ecx, ebx);

  // Push the arguments.
  Label loop_header, loop_check;
  __ jmp(&loop_check);
  __ bind(&loop_header);
  __ Push(Operand(ebx, 0));
  __ sub(ebx, Immediate(kPointerSize));
  __ bind(&loop_check);
  __ cmp(ebx, ecx);
  __ j(greater, &loop_header, Label::kNear);

  // Call the target.
  __ Push(edx);  // Re-push return address.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : actual number of arguments
  //  -- ebx : expected number of arguments
  //  -- edi : function (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments;
  __ IncrementCounter(masm->isolate()->counters()->arguments_adaptors(), 1);

  Label stack_overflow;
  ArgumentsAdaptorStackCheck(masm, &stack_overflow);

  Label enough, too_few;
  __ mov(edx, FieldOperand(edi, JSFunction::kCodeEntryOffset));
  __ cmp(eax, ebx);
  __ j(less, &too_few);
  __ cmp(ebx, SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  __ j(equal, &dont_adapt_arguments);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Copy receiver and all expected arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(edi, Operand(ebp, eax, times_4, offset));
    __ mov(eax, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ inc(eax);
    __ push(Operand(edi, 0));
    __ sub(edi, Immediate(kPointerSize));
    __ cmp(eax, ebx);
    __ j(less, &copy);
    // eax now contains the expected number of arguments.
    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);

    // If the function is strong we need to throw an error.
    Label no_strong_error;
    __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ test_b(FieldOperand(ecx, SharedFunctionInfo::kStrongModeByteOffset),
              1 << SharedFunctionInfo::kStrongModeBitWithinByte);
    __ j(equal, &no_strong_error, Label::kNear);

    // What we really care about is the required number of arguments.
    __ mov(ecx, FieldOperand(ecx, SharedFunctionInfo::kLengthOffset));
    __ SmiUntag(ecx);
    __ cmp(eax, ecx);
    __ j(greater_equal, &no_strong_error, Label::kNear);

    {
      FrameScope frame(masm, StackFrame::MANUAL);
      EnterArgumentsAdaptorFrame(masm);
      __ CallRuntime(Runtime::kThrowStrongModeTooFewArguments, 0);
    }

    __ bind(&no_strong_error);
    EnterArgumentsAdaptorFrame(masm);

    // Remember expected arguments in ecx.
    __ mov(ecx, ebx);

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

    // Restore expected arguments.
    __ mov(eax, ecx);
  }

  // Call the entry point.
  __ bind(&invoke);
  // Restore function pointer.
  __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  // eax : expected number of arguments
  // edi : function (passed through to callee)
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

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    EnterArgumentsAdaptorFrame(masm);
    __ CallRuntime(Runtime::kThrowStackOverflow, 0);
    __ int3();
  }
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ mov(eax, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(eax);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement, 1);
  }

  Label skip;
  // If the code object is null, just return to the unoptimized code.
  __ cmp(eax, Immediate(0));
  __ j(not_equal, &skip, Label::kNear);
  __ ret(0);

  __ bind(&skip);

  // Load deoptimization data from the code object.
  __ mov(ebx, Operand(eax, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  __ mov(ebx, Operand(ebx, FixedArray::OffsetOfElementAt(
      DeoptimizationInputData::kOsrPcOffsetIndex) - kHeapObjectTag));
  __ SmiUntag(ebx);

  // Compute the target address = code_obj + header_size + osr_offset
  __ lea(eax, Operand(eax, ebx, times_1, Code::kHeaderSize - kHeapObjectTag));

  // Overwrite the return address on the stack.
  __ mov(Operand(esp, 0), eax);

  // And "return" to the OSR entry point of the function.
  __ ret(0);
}


void Builtins::Generate_OsrAfterStackCheck(MacroAssembler* masm) {
  // We check the stack limit as indicator that recompilation might be done.
  Label ok;
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(masm->isolate());
  __ cmp(esp, Operand::StaticVariable(stack_limit));
  __ j(above_equal, &ok, Label::kNear);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kStackGuard, 0);
  }
  __ jmp(masm->isolate()->builtins()->OnStackReplacement(),
         RelocInfo::CODE_TARGET);

  __ bind(&ok);
  __ ret(0);
}

#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32

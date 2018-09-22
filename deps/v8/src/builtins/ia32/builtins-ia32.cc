// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/base/adapters.h"
#include "src/code-factory.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/objects-inl.h"
#include "src/objects/js-generator.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
  __ mov(kJavaScriptCallExtraArg1Register,
         Immediate(ExternalReference::Create(address)));
  if (exit_frame_type == BUILTIN_EXIT) {
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK(exit_frame_type == EXIT);
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithExitFrame),
            RelocInfo::CODE_TARGET);
  }
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- eax : argument count (preserved for callee)
  //  -- edx : new target (preserved for callee)
  //  -- edi : target function (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push the number of arguments to the callee.
    __ SmiTag(eax);
    __ push(eax);
    // Push a copy of the target function and the new target.
    __ push(edi);
    __ push(edx);
    // Function is also the parameter to the runtime call.
    __ push(edi);

    __ CallRuntime(function_id, 1);
    __ mov(ecx, eax);

    // Restore target function and new target.
    __ pop(edx);
    __ pop(edi);
    __ pop(eax);
    __ SmiUntag(eax);
  }

  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  __ lea(ecx, FieldOperand(ecx, Code::kHeaderSize));
  __ jmp(ecx);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax: number of arguments
  //  -- edi: constructor function
  //  -- edx: new target
  //  -- esi: context
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(eax);
    __ push(esi);
    __ push(eax);
    __ SmiUntag(eax);

    // The receiver for the builtin/api call.
    __ PushRoot(Heap::kTheHoleValueRootIndex);

    // Set up pointer to last argument.
    __ lea(ebx, Operand(ebp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(ecx, eax);
    // ----------- S t a t e -------------
    //  --                eax: number of arguments (untagged)
    //  --                edi: constructor function
    //  --                edx: new target
    //  --                ebx: pointer to last argument
    //  --                ecx: counter
    //  -- sp[0*kPointerSize]: the hole (receiver)
    //  -- sp[1*kPointerSize]: number of arguments (tagged)
    //  -- sp[2*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry);
    __ bind(&loop);
    __ push(Operand(ebx, ecx, times_4, 0));
    __ bind(&entry);
    __ dec(ecx);
    __ j(greater_equal, &loop);

    // Call the function.
    // eax: number of arguments (untagged)
    // edi: constructor function
    // edx: new target
    ParameterCount actual(eax);
    __ InvokeFunction(edi, edx, actual, CALL_FUNCTION);

    // Restore context from the frame.
    __ mov(esi, Operand(ebp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ mov(ebx, Operand(ebp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ pop(ecx);
  __ lea(esp, Operand(esp, ebx, times_2, 1 * kPointerSize));  // 1 ~ receiver
  __ push(ecx);
  __ ret(0);
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax: number of arguments (untagged)
  //  -- edi: constructor function
  //  -- edx: new target
  //  -- esi: context
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ mov(ecx, eax);
    __ SmiTag(ecx);
    __ Push(esi);
    __ Push(ecx);
    __ Push(edi);
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    __ Push(edx);

    // ----------- S t a t e -------------
    //  --         sp[0*kPointerSize]: new target
    //  --         sp[1*kPointerSize]: padding
    //  -- edi and sp[2*kPointerSize]: constructor function
    //  --         sp[3*kPointerSize]: argument count
    //  --         sp[4*kPointerSize]: context
    // -----------------------------------

    __ mov(ebx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ test(FieldOperand(ebx, SharedFunctionInfo::kFlagsOffset),
            Immediate(SharedFunctionInfo::IsDerivedConstructorBit::kMask));
    __ j(not_zero, &not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ jmp(&post_instantiation_deopt_entry, Label::kNear);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(eax, Heap::kTheHoleValueRootIndex);

    // ----------- S t a t e -------------
    //  --                         eax: implicit receiver
    //  -- Slot 4 / sp[0*kPointerSize]: new target
    //  -- Slot 3 / sp[1*kPointerSize]: padding
    //  -- Slot 2 / sp[2*kPointerSize]: constructor function
    //  -- Slot 1 / sp[3*kPointerSize]: number of arguments (tagged)
    //  -- Slot 0 / sp[4*kPointerSize]: context
    // -----------------------------------
    // Deoptimizer enters here.
    masm->isolate()->heap()->SetConstructStubCreateDeoptPCOffset(
        masm->pc_offset());
    __ bind(&post_instantiation_deopt_entry);

    // Restore new target.
    __ Pop(edx);

    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(eax);
    __ Push(eax);

    // ----------- S t a t e -------------
    //  --                edx: new target
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: implicit receiver
    //  -- sp[2*kPointerSize]: padding
    //  -- sp[3*kPointerSize]: constructor function
    //  -- sp[4*kPointerSize]: number of arguments (tagged)
    //  -- sp[5*kPointerSize]: context
    // -----------------------------------

    // Restore constructor function and argument count.
    __ mov(edi, Operand(ebp, ConstructFrameConstants::kConstructorOffset));
    __ mov(eax, Operand(ebp, ConstructFrameConstants::kLengthOffset));
    __ SmiUntag(eax);

    // Set up pointer to last argument.
    __ lea(ebx, Operand(ebp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(ecx, eax);
    // ----------- S t a t e -------------
    //  --                        eax: number of arguments (untagged)
    //  --                        edx: new target
    //  --                        ebx: pointer to last argument
    //  --                        ecx: counter (tagged)
    //  --         sp[0*kPointerSize]: implicit receiver
    //  --         sp[1*kPointerSize]: implicit receiver
    //  --         sp[2*kPointerSize]: padding
    //  -- edi and sp[3*kPointerSize]: constructor function
    //  --         sp[4*kPointerSize]: number of arguments (tagged)
    //  --         sp[5*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry, Label::kNear);
    __ bind(&loop);
    __ Push(Operand(ebx, ecx, times_pointer_size, 0));
    __ bind(&entry);
    __ dec(ecx);
    __ j(greater_equal, &loop);

    // Call the function.
    ParameterCount actual(eax);
    __ InvokeFunction(edi, edx, actual, CALL_FUNCTION);

    // ----------- S t a t e -------------
    //  --                eax: constructor result
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: padding
    //  -- sp[2*kPointerSize]: constructor function
    //  -- sp[3*kPointerSize]: number of arguments
    //  -- sp[4*kPointerSize]: context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore context from the frame.
    __ mov(esi, Operand(ebp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(eax, Heap::kUndefinedValueRootIndex, &use_receiver,
                  Label::kNear);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(eax, &use_receiver, Label::kNear);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, ecx);
    __ j(above_equal, &leave_frame, Label::kNear);
    __ jmp(&use_receiver, Label::kNear);

    __ bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ mov(eax, Operand(esp, 0 * kPointerSize));
    __ JumpIfRoot(eax, Heap::kTheHoleValueRootIndex, &do_throw);

    __ bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ mov(ebx, Operand(ebp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ pop(ecx);
  __ lea(esp, Operand(esp, ebx, times_2, 1 * kPointerSize));  // 1 ~ receiver
  __ push(ecx);
  __ ret(0);
}

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ push(edi);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

static void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                        Register scratch1, Register scratch2,
                                        Label* stack_overflow,
                                        bool include_receiver = false) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  ExternalReference real_stack_limit =
      ExternalReference::address_of_real_stack_limit(masm->isolate());
  __ mov(scratch1, __ StaticVariable(real_stack_limit));
  // Make scratch2 the space we have left. The stack might already be overflowed
  // here which will cause scratch2 to become negative.
  __ mov(scratch2, esp);
  __ sub(scratch2, scratch1);
  // Make scratch1 the space we need for the array when it is unrolled onto the
  // stack.
  __ mov(scratch1, num_args);
  if (include_receiver) {
    __ add(scratch1, Immediate(1));
  }
  __ shl(scratch1, kPointerSizeLog2);
  // Check if the arguments will overflow the stack.
  __ cmp(scratch2, scratch1);
  __ j(less_equal, stack_overflow);  // Signed comparison.
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address = ExternalReference::Create(
        IsolateAddressId::kContextAddress, masm->isolate());
    __ mov(esi, __ StaticVariable(context_address));

    // Load the previous frame pointer (ebx) to access C arguments
    __ mov(ebx, Operand(ebp, 0));

    // Push the function and the receiver onto the stack.
    __ push(Operand(ebx, EntryFrameConstants::kFunctionArgOffset));
    __ push(Operand(ebx, EntryFrameConstants::kReceiverArgOffset));

    // Load the number of arguments and setup pointer to the arguments.
    __ mov(eax, Operand(ebx, EntryFrameConstants::kArgcOffset));
    __ mov(ebx, Operand(ebx, EntryFrameConstants::kArgvOffset));

    // Check if we have enough stack space to push all arguments.
    // Argument count in eax. Clobbers ecx and edx.
    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, eax, ecx, edx, &stack_overflow);
    __ jmp(&enough_stack_space);

    __ bind(&stack_overflow);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // This should be unreachable.
    __ int3();

    __ bind(&enough_stack_space);

    // Copy arguments to the stack in a loop.
    Label loop, entry;
    __ Move(ecx, Immediate(0));
    __ jmp(&entry, Label::kNear);
    __ bind(&loop);
    __ mov(edx, Operand(ebx, ecx, times_4, 0));  // push parameter from argv
    __ push(Operand(edx, 0));                    // dereference handle
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
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the internal frame. Notice that this also removes the empty.
    // context and the function left on the stack by the code
    // invocation.
  }
  __ ret(0);
}

void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}

void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}

static void GetSharedFunctionInfoBytecode(MacroAssembler* masm,
                                          Register sfi_data,
                                          Register scratch1) {
  Label done;

  __ CmpObjectType(sfi_data, INTERPRETER_DATA_TYPE, scratch1);
  __ j(not_equal, &done, Label::kNear);
  __ mov(sfi_data,
         FieldOperand(sfi_data, InterpreterData::kBytecodeArrayOffset));

  __ bind(&done);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : the value to pass to the generator
  //  -- edx    : the JSGeneratorObject to resume
  //  -- esp[0] : return address
  // -----------------------------------
  __ AssertGeneratorObject(edx);

  // Store input value into generator object.
  __ mov(FieldOperand(edx, JSGeneratorObject::kInputOrDebugPosOffset), eax);
  __ RecordWriteField(edx, JSGeneratorObject::kInputOrDebugPosOffset, eax, ecx,
                      kDontSaveFPRegs);

  // Load suspended function and context.
  __ mov(edi, FieldOperand(edx, JSGeneratorObject::kFunctionOffset));
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ cmpb(__ StaticVariable(debug_hook), Immediate(0));
  __ j(not_equal, &prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ cmp(edx, __ StaticVariable(debug_suspended_generator));
  __ j(equal, &prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ CompareRoot(esp, ecx, Heap::kRealStackLimitRootIndex);
  __ j(below, &stack_overflow);

  // Pop return address.
  __ PopReturnAddressTo(eax);

  // Push receiver.
  __ Push(FieldOperand(edx, JSGeneratorObject::kReceiverOffset));

  // ----------- S t a t e -------------
  //  -- eax    : return address
  //  -- edx    : the JSGeneratorObject to resume
  //  -- edi    : generator function
  //  -- esi    : generator context
  //  -- esp[0] : generator receiver
  // -----------------------------------

  // Copy the function arguments from the generator object's register file.
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ movzx_w(
      ecx, FieldOperand(ecx, SharedFunctionInfo::kFormalParameterCountOffset));
  __ mov(ebx,
         FieldOperand(edx, JSGeneratorObject::kParametersAndRegistersOffset));
  {
    Label done_loop, loop;
    __ Set(edi, 0);

    __ bind(&loop);
    __ cmp(edi, ecx);
    __ j(greater_equal, &done_loop);
    __ Push(
        FieldOperand(ebx, edi, times_pointer_size, FixedArray::kHeaderSize));
    __ add(edi, Immediate(1));
    __ jmp(&loop);

    __ bind(&done_loop);
    __ mov(edi, FieldOperand(edx, JSGeneratorObject::kFunctionOffset));
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ mov(ecx, FieldOperand(ecx, SharedFunctionInfo::kFunctionDataOffset));
    __ Push(eax);
    GetSharedFunctionInfoBytecode(masm, ecx, eax);
    __ Pop(eax);
    __ CmpObjectType(ecx, BYTECODE_ARRAY_TYPE, ecx);
    __ Assert(equal, AbortReason::kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ PushReturnAddressFrom(eax);
    __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ movzx_w(eax, FieldOperand(
                        eax, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
    __ mov(ecx, FieldOperand(edi, JSFunction::kCodeOffset));
    __ add(ecx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(ecx);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(edx);
    __ Push(edi);
    // Push hole as receiver since we do not use it for stepping.
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(edx);
    __ mov(edi, FieldOperand(edx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(edx);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(edx);
    __ mov(edi, FieldOperand(edx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();  // This should be unreachable.
  }
}

static void ReplaceClosureCodeWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {

  // Store the optimized code in the closure.
  __ mov(FieldOperand(closure, JSFunction::kCodeOffset), optimized_code);
  __ mov(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch1,
                                  Register scratch2) {
  Register args_count = scratch1;
  Register return_pc = scratch2;

  // Get the arguments + receiver count.
  __ mov(args_count,
         Operand(ebp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ mov(args_count,
         FieldOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ leave();

  // Drop receiver + arguments.
  __ pop(return_pc);
  __ add(esp, args_count);
  __ push(return_pc);
}

// Tail-call |function_id| if |smi_entry| == |marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register smi_entry,
                                          OptimizationMarker marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ cmp(smi_entry, Immediate(Smi::FromEnum(marker)));
  __ j(not_equal, &no_match, Label::kNear);
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void MaybeTailCallOptimizedCodeSlot(MacroAssembler* masm,
                                           Register feedback_vector,
                                           Register scratch) {
  // ----------- S t a t e -------------
  //  -- eax : argument count (preserved for callee if needed, and caller)
  //  -- edx : new target (preserved for callee if needed, and caller)
  //  -- edi : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  // -----------------------------------
  DCHECK(!AreAliased(feedback_vector, eax, edx, edi, scratch));

  Label optimized_code_slot_is_weak_ref, fallthrough;

  Register closure = edi;
  Register optimized_code_entry = scratch;

  __ mov(optimized_code_entry,
         FieldOperand(feedback_vector, FeedbackVector::kOptimizedCodeOffset));

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret it as a weak reference to a code
  // object.
  __ JumpIfNotSmi(optimized_code_entry, &optimized_code_slot_is_weak_ref);

  {
    // Optimized code slot is an optimization marker.

    // Fall through if no optimization trigger.
    __ cmp(optimized_code_entry,
           Immediate(Smi::FromEnum(OptimizationMarker::kNone)));
    __ j(equal, &fallthrough);

    TailCallRuntimeIfMarkerEquals(masm, optimized_code_entry,
                                  OptimizationMarker::kLogFirstExecution,
                                  Runtime::kFunctionFirstExecution);
    TailCallRuntimeIfMarkerEquals(masm, optimized_code_entry,
                                  OptimizationMarker::kCompileOptimized,
                                  Runtime::kCompileOptimized_NotConcurrent);
    TailCallRuntimeIfMarkerEquals(
        masm, optimized_code_entry,
        OptimizationMarker::kCompileOptimizedConcurrent,
        Runtime::kCompileOptimized_Concurrent);

    {
      // Otherwise, the marker is InOptimizationQueue, so fall through hoping
      // that an interrupt will eventually update the slot with optimized code.
      if (FLAG_debug_code) {
        __ cmp(
            optimized_code_entry,
            Immediate(Smi::FromEnum(OptimizationMarker::kInOptimizationQueue)));
        __ Assert(equal, AbortReason::kExpectedOptimizationSentinel);
      }
      __ jmp(&fallthrough);
    }
  }

  {
    // Optimized code slot is a weak reference.
    __ bind(&optimized_code_slot_is_weak_ref);

    __ LoadWeakValue(optimized_code_entry, &fallthrough);

    __ push(eax);
    __ push(edx);

    // Check if the optimized code is marked for deopt. If it is, bailout to a
    // given label.
    Label found_deoptimized_code;
    __ mov(eax,
           FieldOperand(optimized_code_entry, Code::kCodeDataContainerOffset));
    __ test(FieldOperand(eax, CodeDataContainer::kKindSpecificFlagsOffset),
            Immediate(1 << Code::kMarkedForDeoptimizationBit));
    __ j(not_zero, &found_deoptimized_code);

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    // The feedback vector is no longer used, so re-use it as a scratch
    // register.
    ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                        edx, eax, feedback_vector);
    static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
    __ Move(ecx, optimized_code_entry);
    __ add(ecx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ pop(edx);
    __ pop(eax);
    __ jmp(ecx);

    // Optimized code slot contains deoptimized code, evict it and re-enter the
    // closure's code.
    __ bind(&found_deoptimized_code);
    __ pop(edx);
    __ pop(eax);
    GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
  }

  // Fall-through if the optimized code cell is clear and there is no
  // optimization marker.
  __ bind(&fallthrough);
}

// Advance the current bytecode offset. This simulates what all bytecode
// handlers do upon completion of the underlying operation. Will bail out to a
// label if the bytecode (without prefix) is a return bytecode.
static void AdvanceBytecodeOffsetOrReturn(MacroAssembler* masm,
                                          Register bytecode_array,
                                          Register bytecode_offset,
                                          Register bytecode, Register scratch1,
                                          Label* if_return) {
  Register bytecode_size_table = scratch1;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode));

  __ Move(bytecode_size_table,
          Immediate(ExternalReference::bytecode_size_table_address()));

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  STATIC_ASSERT(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  STATIC_ASSERT(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ cmpb(bytecode, Immediate(0x3));
  __ j(above, &process_bytecode, Label::kNear);
  __ test(bytecode, Immediate(0x1));
  __ j(not_equal, &extra_wide, Label::kNear);

  // Load the next bytecode and update table to the wide scaled table.
  __ inc(bytecode_offset);
  __ movzx_b(bytecode, Operand(bytecode_array, bytecode_offset, times_1, 0));
  __ add(bytecode_size_table,
         Immediate(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&process_bytecode, Label::kNear);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ inc(bytecode_offset);
  __ movzx_b(bytecode, Operand(bytecode_array, bytecode_offset, times_1, 0));
  __ add(bytecode_size_table,
         Immediate(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));

  __ bind(&process_bytecode);

// Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)                                             \
  __ cmpb(bytecode,                                                     \
          Immediate(static_cast<int>(interpreter::Bytecode::k##NAME))); \
  __ j(equal, if_return);
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // Otherwise, load the size of the current bytecode and advance the offset.
  __ add(bytecode_offset, Operand(bytecode_size_table, bytecode, times_4, 0));
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o edi: the JS function object being called
//   o edx: the incoming new target or generator object
//   o esi: our context
//   o ebp: the caller's frame pointer
//   o esp: stack pointer (pointing to return address)
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  Register closure = edi;
  Register feedback_vector = ebx;

  // Load the feedback vector from the closure.
  __ mov(feedback_vector,
         FieldOperand(closure, JSFunction::kFeedbackCellOffset));
  __ mov(feedback_vector, FieldOperand(feedback_vector, Cell::kValueOffset));
  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, ecx);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set
  // up the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ push(ebp);  // Caller's frame pointer.
  __ mov(ebp, esp);
  __ push(esi);  // Callee's context.
  __ push(edi);  // Callee's JS function.

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(kInterpreterBytecodeArrayRegister,
         FieldOperand(eax, SharedFunctionInfo::kFunctionDataOffset));
  __ Push(eax);
  GetSharedFunctionInfoBytecode(masm, kInterpreterBytecodeArrayRegister, eax);
  __ Pop(eax);

  __ inc(FieldOperand(feedback_vector, FeedbackVector::kInvocationCountOffset));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     eax);
    __ Assert(
        equal,
        AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Reset code age.
  __ mov_b(FieldOperand(kInterpreterBytecodeArrayRegister,
                        BytecodeArray::kBytecodeAgeOffset),
           Immediate(BytecodeArray::kNoAgeBytecodeAge));

  // Push bytecode array.
  __ push(kInterpreterBytecodeArrayRegister);
  // Push Smi tagged initial bytecode array offset.
  __ push(Immediate(Smi::FromInt(BytecodeArray::kHeaderSize - kHeapObjectTag)));

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
    __ cmp(ecx, __ StaticVariable(stack_limit));
    __ j(above_equal, &ok);
    __ CallRuntime(Runtime::kThrowStackOverflow);
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

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in edx.
  Label no_incoming_new_target_or_generator_register;
  __ mov(eax, FieldOperand(
                  kInterpreterBytecodeArrayRegister,
                  BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ test(eax, eax);
  __ j(zero, &no_incoming_new_target_or_generator_register);
  __ mov(Operand(ebp, eax, times_pointer_size, 0), edx);
  __ bind(&no_incoming_new_target_or_generator_register);

  // Load accumulator and bytecode offset into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ mov(kInterpreterBytecodeOffsetRegister,
         Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ mov(kInterpreterDispatchTableRegister,
         Immediate(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));
  __ movzx_b(ebx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ mov(
      kJavaScriptCallCodeStartRegister,
      Operand(kInterpreterDispatchTableRegister, ebx, times_pointer_size, 0));
  __ call(kJavaScriptCallCodeStartRegister);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ mov(kInterpreterBytecodeArrayRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Either return, or advance to the next bytecode and dispatch.
  Label do_return;
  __ movzx_b(ebx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, ebx, ecx,
                                &do_return);
  __ jmp(&do_dispatch);

  __ bind(&do_return);
  // The return value is in eax.
  LeaveInterpreterFrame(masm, ebx, ecx);
  __ ret(0);
}


static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register array_limit,
                                         Register start_address) {
  // ----------- S t a t e -------------
  //  -- start_address : Pointer to the last argument in the args array.
  //  -- array_limit : Pointer to one before the first argument in the
  //                   args array.
  // -----------------------------------
  Label loop_header, loop_check;
  __ jmp(&loop_check);
  __ bind(&loop_header);
  __ Push(Operand(start_address, 0));
  __ sub(start_address, Immediate(kPointerSize));
  __ bind(&loop_check);
  __ cmp(start_address, array_limit);
  __ j(greater, &loop_header, Label::kNear);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- ebx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  //  -- edi : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;
  // Compute the expected number of arguments.
  __ mov(ecx, eax);
  __ add(ecx, Immediate(1));  // Add one for receiver.

  // Add a stack check before pushing the arguments. We need an extra register
  // to perform a stack check. So push it onto the stack temporarily. This
  // might cause stack overflow, but it will be detected by the check.
  __ Push(edi);
  Generate_StackOverflowCheck(masm, ecx, edx, edi, &stack_overflow);
  __ Pop(edi);

  // Pop return address to allow tail-call after pushing arguments.
  __ Pop(edx);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ sub(ecx, Immediate(1));  // Subtract one for receiver.
  }

  // Find the address of the last argument.
  __ shl(ecx, kPointerSizeLog2);
  __ neg(ecx);
  __ add(ecx, ebx);
  Generate_InterpreterPushArgs(masm, ecx, ebx);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(ebx);                // Pass the spread in a register
    __ sub(eax, Immediate(1));  // Subtract one for spread
  }

  // Call the target.
  __ Push(edx);  // Re-push return address.

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(BUILTIN_CODE(masm->isolate(), CallWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny),
            RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    // Pop the temporary registers, so that return address is on top of stack.
    __ Pop(edi);

    __ TailCallRuntime(Runtime::kThrowStackOverflow);

    // This should be unreachable.
    __ int3();
  }
}

namespace {

// This function modified start_addr, and only reads the contents of num_args
// register. scratch1 and scratch2 are used as temporary registers. Their
// original values are restored after the use.
void Generate_InterpreterPushZeroAndArgsAndReturnAddress(
    MacroAssembler* masm, Register num_args, Register start_addr,
    Register scratch1, Register scratch2, int num_slots_above_ret_addr,
    Label* stack_overflow) {
  // We have to move return address and the temporary registers above it
  // before we can copy arguments onto the stack. To achieve this:
  // Step 1: Increment the stack pointer by num_args + 1 (for receiver).
  // Step 2: Move the return address and values above it to the top of stack.
  // Step 3: Copy the arguments into the correct locations.
  //  current stack    =====>    required stack layout
  // |             |            | scratch1      | (2) <-- esp(1)
  // |             |            | ....          | (2)
  // |             |            | scratch-n     | (2)
  // |             |            | return addr   | (2)
  // |             |            | arg N         | (3)
  // | scratch1    | <-- esp    | ....          |
  // | ....        |            | arg 1         |
  // | scratch-n   |            | arg 0         |
  // | return addr |            | receiver slot |

  // Check for stack overflow before we increment the stack pointer.
  Generate_StackOverflowCheck(masm, num_args, scratch1, scratch2,
                              stack_overflow, true);

  // Step 1 - Update the stack pointer. scratch1 already contains the required
  // increment to the stack. i.e. num_args + 1 stack slots. This is computed in
  // Generate_StackOverflowCheck.

  __ AllocateStackFrame(scratch1);

  // Step 2 move return_address and slots above it to the correct locations.
  // Move from top to bottom, otherwise we may overwrite when num_args = 0 or 1,
  // basically when the source and destination overlap. We at least need one
  // extra slot for receiver, so no extra checks are required to avoid copy.
  for (int i = 0; i < num_slots_above_ret_addr + 1; i++) {
    __ mov(scratch1,
           Operand(esp, num_args, times_pointer_size, (i + 1) * kPointerSize));
    __ mov(Operand(esp, i * kPointerSize), scratch1);
  }

  // Step 3 copy arguments to correct locations.
  // Slot meant for receiver contains return address. Reset it so that
  // we will not incorrectly interpret return address as an object.
  __ mov(Operand(esp, num_args, times_pointer_size,
                 (num_slots_above_ret_addr + 1) * kPointerSize),
         Immediate(0));
  __ mov(scratch1, num_args);

  Label loop_header, loop_check;
  __ jmp(&loop_check);
  __ bind(&loop_header);
  __ mov(scratch2, Operand(start_addr, 0));
  __ mov(Operand(esp, scratch1, times_pointer_size,
                 num_slots_above_ret_addr * kPointerSize),
         scratch2);
  __ sub(start_addr, Immediate(kPointerSize));
  __ sub(scratch1, Immediate(1));
  __ bind(&loop_check);
  __ cmp(scratch1, Immediate(0));
  __ j(greater, &loop_header, Label::kNear);
}

}  // end anonymous namespace

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the new target
  //  -- edi : the constructor
  //  -- ebx : allocation site feedback (if available or undefined)
  //  -- ecx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  // -----------------------------------
  Label stack_overflow;
  // We need two scratch registers. Push edi and edx onto stack.
  __ Push(edi);
  __ Push(edx);

  // Push arguments and move return address to the top of stack.
  // The eax register is readonly. The ecx register will be modified. The edx
  // and edi registers will be modified but restored to their original values.
  Generate_InterpreterPushZeroAndArgsAndReturnAddress(masm, eax, ecx, edx, edi,
                                                      2, &stack_overflow);

  // Restore edi and edx
  __ Pop(edx);
  __ Pop(edi);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ PopReturnAddressTo(ecx);
    __ Pop(ebx);  // Pass the spread in a register
    __ PushReturnAddressFrom(ecx);
    __ sub(eax, Immediate(1));  // Subtract one for spread
  } else {
    __ AssertUndefinedOrAllocationSite(ebx);
  }

  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    // Tail call to the array construct stub (still in the caller
    // context at this point).
    __ AssertFunction(edi);
    // TODO(v8:6666): When rewriting ia32 ASM builtins to not clobber the
    // kRootRegister ebx, this useless move can be removed.
    __ Move(kJavaScriptCallExtraArg1Register, ebx);
    Handle<Code> code = BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl);
    __ Jump(code, RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with unmodified eax, edi, edx values.
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with unmodified eax, edi, edx values.
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    // Pop the temporary registers, so that return address is on top of stack.
    __ Pop(edx);
    __ Pop(edi);

    __ TailCallRuntime(Runtime::kThrowStackOverflow);

    // This should be unreachable.
    __ int3();
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Label builtin_trampoline, trampoline_loaded;
  Smi* interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);

  // If the SFI function_data is an InterpreterData, get the trampoline stored
  // in it, otherwise get the trampoline from the builtins list.
  __ mov(ebx, Operand(ebp, StandardFrameConstants::kFunctionOffset));
  __ mov(ebx, FieldOperand(ebx, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ebx, FieldOperand(ebx, SharedFunctionInfo::kFunctionDataOffset));
  __ Push(eax);
  __ CmpObjectType(ebx, INTERPRETER_DATA_TYPE, eax);
  __ j(not_equal, &builtin_trampoline, Label::kNear);

  __ mov(ebx, FieldOperand(ebx, InterpreterData::kInterpreterTrampolineOffset));
  __ jmp(&trampoline_loaded, Label::kNear);

  __ bind(&builtin_trampoline);
  __ Move(ebx, BUILTIN_CODE(masm->isolate(), InterpreterEntryTrampoline));

  __ bind(&trampoline_loaded);
  __ Pop(eax);
  __ add(ebx, Immediate(interpreter_entry_return_pc_offset->value() +
                        Code::kHeaderSize - kHeapObjectTag));
  __ push(ebx);

  // Initialize the dispatch table register.
  __ mov(kInterpreterDispatchTableRegister,
         Immediate(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ mov(kInterpreterBytecodeArrayRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     ebx);
    __ Assert(
        equal,
        AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ movzx_b(ebx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ mov(
      kJavaScriptCallCodeStartRegister,
      Operand(kInterpreterDispatchTableRegister, ebx, times_pointer_size, 0));
  __ jmp(kJavaScriptCallCodeStartRegister);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Get bytecode array and bytecode offset from the stack frame.
  __ mov(kInterpreterBytecodeArrayRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Load the current bytecode
  __ movzx_b(ebx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));

  // Advance to the next bytecode.
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, ebx, ecx,
                                &if_return);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ mov(ebx, kInterpreterBytecodeOffsetRegister);
  __ SmiTag(ebx);
  __ mov(Operand(ebp, InterpreterFrameConstants::kBytecodeOffsetFromFp), ebx);

  Generate_InterpreterEnterBytecode(masm);

  // We should never take the if_return path.
  __ bind(&if_return);
  __ Abort(AbortReason::kInvalidBytecodeAdvance);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InstantiateAsmJs(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argument count (preserved for callee)
  //  -- edx : new target (preserved for callee)
  //  -- edi : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ mov(ecx, eax);
    // Push the number of arguments to the callee.
    __ SmiTag(eax);
    __ push(eax);
    // Push a copy of the target function and the new target.
    __ push(edi);
    __ push(edx);

    // The function.
    __ push(edi);
    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ cmp(ecx, Immediate(j));
        __ j(not_equal, &over, Label::kNear);
      }
      for (int i = j - 1; i >= 0; --i) {
        __ Push(Operand(
            ebp, StandardFrameConstants::kCallerSPOffset + i * kPointerSize));
      }
      for (int i = 0; i < 3 - j; ++i) {
        __ PushRoot(Heap::kUndefinedValueRootIndex);
      }
      if (j < 3) {
        __ jmp(&args_done, Label::kNear);
        __ bind(&over);
      }
    }
    __ bind(&args_done);

    // Call runtime, on success unwind frame, and parent frame.
    __ CallRuntime(Runtime::kInstantiateAsmJs, 4);
    // A smi 0 is returned on failure, an object on success.
    __ JumpIfSmi(eax, &failed, Label::kNear);

    __ Drop(2);
    __ Pop(ecx);
    __ SmiUntag(ecx);
    scope.GenerateLeaveFrame();

    __ PopReturnAddressTo(ebx);
    __ inc(ecx);
    __ lea(esp, Operand(esp, ecx, times_pointer_size, 0));
    __ PushReturnAddressFrom(ebx);
    __ ret(0);

    __ bind(&failed);
    // Restore target function and new target.
    __ pop(edx);
    __ pop(edi);
    __ pop(eax);
    __ SmiUntag(eax);
  }
  // On failure, tail call back to regular js by re-calling the function
  // which has be reset to the compile lazy builtin.
  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  __ mov(ecx, FieldOperand(edi, JSFunction::kCodeOffset));
  __ add(ecx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(ecx);
}

namespace {
void Generate_ContinueToBuiltinHelper(MacroAssembler* masm,
                                      bool java_script_builtin,
                                      bool with_result) {
  const RegisterConfiguration* config(RegisterConfiguration::Default());
  int allocatable_register_count = config->num_allocatable_general_registers();
  if (with_result) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point.
    __ mov(Operand(esp,
                   config->num_allocatable_general_registers() * kPointerSize +
                       BuiltinContinuationFrameConstants::kFixedFrameSize),
           eax);
  }
  for (int i = allocatable_register_count - 1; i >= 0; --i) {
    int code = config->GetAllocatableGeneralCode(i);
    __ pop(Register::from_code(code));
    if (java_script_builtin && code == kJavaScriptCallArgCountRegister.code()) {
      __ SmiUntag(Register::from_code(code));
    }
  }
  __ mov(
      ebp,
      Operand(esp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  const int offsetToPC =
      BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp - kPointerSize;
  __ pop(Operand(esp, offsetToPC));
  __ Drop(offsetToPC / kPointerSize);
  __ add(Operand(esp, 0), Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ ret(0);
}
}  // namespace

void Builtins::Generate_ContinueToCodeStubBuiltin(MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, false, false);
}

void Builtins::Generate_ContinueToCodeStubBuiltinWithResult(
    MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, false, true);
}

void Builtins::Generate_ContinueToJavaScriptBuiltin(MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, true, false);
}

void Builtins::Generate_ContinueToJavaScriptBuiltinWithResult(
    MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, true, true);
}

void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
    // Tear down internal frame.
  }

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), eax.code());
  __ mov(eax, Operand(esp, 1 * kPointerSize));
  __ ret(1 * kPointerSize);  // Remove eax.
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax     : argc
  //  -- esp[0]  : return address
  //  -- esp[4]  : argArray
  //  -- esp[8]  : thisArg
  //  -- esp[12] : receiver
  // -----------------------------------

  // 1. Load receiver into edi, argArray into ebx (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label no_arg_array, no_this_arg;
    __ LoadRoot(edx, Heap::kUndefinedValueRootIndex);
    __ mov(ebx, edx);
    __ mov(edi, Operand(esp, eax, times_pointer_size, kPointerSize));
    __ test(eax, eax);
    __ j(zero, &no_this_arg, Label::kNear);
    {
      __ mov(edx, Operand(esp, eax, times_pointer_size, 0));
      __ cmp(eax, Immediate(1));
      __ j(equal, &no_arg_array, Label::kNear);
      __ mov(ebx, Operand(esp, eax, times_pointer_size, -kPointerSize));
      __ bind(&no_arg_array);
    }
    __ bind(&no_this_arg);
    __ PopReturnAddressTo(ecx);
    __ lea(esp, Operand(esp, eax, times_pointer_size, kPointerSize));
    __ Push(edx);
    __ PushReturnAddressFrom(ecx);
  }

  // ----------- S t a t e -------------
  //  -- ebx    : argArray
  //  -- edi    : receiver
  //  -- esp[0] : return address
  //  -- esp[4] : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(ebx, Heap::kNullValueRootIndex, &no_arguments, Label::kNear);
  __ JumpIfRoot(ebx, Heap::kUndefinedValueRootIndex, &no_arguments,
                Label::kNear);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ Set(eax, 0);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
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

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax     : argc
  //  -- esp[0]  : return address
  //  -- esp[4]  : argumentsList
  //  -- esp[8]  : thisArgument
  //  -- esp[12] : target
  //  -- esp[16] : receiver
  // -----------------------------------

  // 1. Load target into edi (if present), argumentsList into ebx (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label done;
    __ LoadRoot(edi, Heap::kUndefinedValueRootIndex);
    __ mov(edx, edi);
    __ mov(ebx, edi);
    __ cmp(eax, Immediate(1));
    __ j(below, &done, Label::kNear);
    __ mov(edi, Operand(esp, eax, times_pointer_size, -0 * kPointerSize));
    __ j(equal, &done, Label::kNear);
    __ mov(edx, Operand(esp, eax, times_pointer_size, -1 * kPointerSize));
    __ cmp(eax, Immediate(3));
    __ j(below, &done, Label::kNear);
    __ mov(ebx, Operand(esp, eax, times_pointer_size, -2 * kPointerSize));
    __ bind(&done);
    __ PopReturnAddressTo(ecx);
    __ lea(esp, Operand(esp, eax, times_pointer_size, kPointerSize));
    __ Push(edx);
    __ PushReturnAddressFrom(ecx);
  }

  // ----------- S t a t e -------------
  //  -- ebx    : argumentsList
  //  -- edi    : target
  //  -- esp[0] : return address
  //  -- esp[4] : thisArgument
  // -----------------------------------

  // 2. We don't need to check explicitly for callable target here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Apply the target to the given argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax     : argc
  //  -- esp[0]  : return address
  //  -- esp[4]  : new.target (optional)
  //  -- esp[8]  : argumentsList
  //  -- esp[12] : target
  //  -- esp[16] : receiver
  // -----------------------------------

  // 1. Load target into edi (if present), argumentsList into ebx (if present),
  // new.target into edx (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label done;
    __ LoadRoot(edi, Heap::kUndefinedValueRootIndex);
    __ mov(edx, edi);
    __ mov(ebx, edi);
    __ cmp(eax, Immediate(1));
    __ j(below, &done, Label::kNear);
    __ mov(edi, Operand(esp, eax, times_pointer_size, -0 * kPointerSize));
    __ mov(edx, edi);
    __ j(equal, &done, Label::kNear);
    __ mov(ebx, Operand(esp, eax, times_pointer_size, -1 * kPointerSize));
    __ cmp(eax, Immediate(3));
    __ j(below, &done, Label::kNear);
    __ mov(edx, Operand(esp, eax, times_pointer_size, -2 * kPointerSize));
    __ bind(&done);
    __ PopReturnAddressTo(ecx);
    __ lea(esp, Operand(esp, eax, times_pointer_size, kPointerSize));
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ PushReturnAddressFrom(ecx);
  }

  // ----------- S t a t e -------------
  //  -- ebx    : argumentsList
  //  -- edx    : new.target
  //  -- edi    : target
  //  -- esp[0] : return address
  //  -- esp[4] : receiver (undefined)
  // -----------------------------------

  // 2. We don't need to check explicitly for constructor target here,
  // since that's the first thing the Construct/ConstructWithArrayLike
  // builtins will do.

  // 3. We don't need to check explicitly for constructor new.target here,
  // since that's the second thing the Construct/ConstructWithArrayLike
  // builtins will do.

  // 4. Construct the target with the given new.target and argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithArrayLike),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_InternalArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argc
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------
  Label generic_array_code;

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray function should be a map.
    __ mov(ebx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    __ test(ebx, Immediate(kSmiTagMask));
    __ Assert(not_zero,
              AbortReason::kUnexpectedInitialMapForInternalArrayFunction);
    __ CmpObjectType(ebx, MAP_TYPE, ecx);
    __ Assert(equal,
              AbortReason::kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  __ mov(ebx, masm->isolate()->factory()->undefined_value());
  __ Jump(BUILTIN_CODE(masm->isolate(), InternalArrayConstructorImpl),
          RelocInfo::CODE_TARGET);
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ push(ebp);
  __ mov(ebp, esp);

  // Store the arguments adaptor context sentinel.
  __ push(Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));

  // Push the function on the stack.
  __ push(edi);

  // Preserve the number of arguments on the stack. Must preserve eax,
  // ebx and ecx because these registers are used when copying the
  // arguments and the receiver.
  STATIC_ASSERT(kSmiTagSize == 1);
  __ lea(edi, Operand(eax, eax, times_1, kSmiTag));
  __ push(edi);

  __ Push(Immediate(0));  // Padding.
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
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- edi    : target
  //  -- eax    : number of parameters on the stack (not including the receiver)
  //  -- ebx    : arguments list (a FixedArray)
  //  -- ecx    : len (number of elements to from args)
  //  -- edx    : new.target (checked to be constructor or undefined)
  //  -- esp[0] : return address.
  // -----------------------------------

  // We need to preserve eax, edi and ebx.
  __ movd(xmm0, edx);
  __ movd(xmm1, edi);
  __ movd(xmm2, eax);

  if (masm->emit_debug_code()) {
    // Allow ebx to be a FixedArray, or a FixedDoubleArray if ecx == 0.
    Label ok, fail;
    __ AssertNotSmi(ebx);
    __ mov(edx, FieldOperand(ebx, HeapObject::kMapOffset));
    __ CmpInstanceType(edx, FIXED_ARRAY_TYPE);
    __ j(equal, &ok);
    __ CmpInstanceType(edx, FIXED_DOUBLE_ARRAY_TYPE);
    __ j(not_equal, &fail);
    __ cmp(ecx, 0);
    __ j(equal, &ok);
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    ExternalReference real_stack_limit =
        ExternalReference::address_of_real_stack_limit(masm->isolate());
    __ mov(edx, __ StaticVariable(real_stack_limit));
    // Make edx the space we have left. The stack might already be overflowed
    // here which will cause edx to become negative.
    __ neg(edx);
    __ add(edx, esp);
    __ sar(edx, kPointerSizeLog2);
    // Check if the arguments will overflow the stack.
    __ cmp(edx, ecx);
    __ j(greater, &done, Label::kNear);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // Push additional arguments onto the stack.
  {
    __ PopReturnAddressTo(edx);
    __ Move(eax, Immediate(0));
    Label done, push, loop;
    __ bind(&loop);
    __ cmp(eax, ecx);
    __ j(equal, &done, Label::kNear);
    // Turn the hole into undefined as we go.
    __ mov(edi,
           FieldOperand(ebx, eax, times_pointer_size, FixedArray::kHeaderSize));
    __ CompareRoot(edi, Heap::kTheHoleValueRootIndex);
    __ j(not_equal, &push, Label::kNear);
    __ LoadRoot(edi, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ Push(edi);
    __ inc(eax);
    __ jmp(&loop);
    __ bind(&done);
    __ PushReturnAddressFrom(edx);
  }

  // Restore eax, edi and edx.
  __ movd(eax, xmm2);
  __ movd(edi, xmm1);
  __ movd(edx, xmm0);

  // Compute the actual parameter count.
  __ add(eax, ecx);

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the target to call (can be any Object)
  //  -- edx : the new target (for [[Construct]] calls)
  //  -- ecx : start index (to support rest parameters)
  // -----------------------------------

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(edx, &new_target_not_constructor, Label::kNear);
    __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
    __ test_b(FieldOperand(ebx, Map::kBitFieldOffset),
              Immediate(Map::IsConstructorBit::kMask));
    __ j(not_zero, &new_target_constructor, Label::kNear);
    __ bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(edx);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ bind(&new_target_constructor);
  }

  // Preserve new.target (in case of [[Construct]]).
  __ movd(xmm0, edx);

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ mov(ebx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ cmp(Operand(ebx, CommonFrameConstants::kContextOrFrameTypeOffset),
         Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &arguments_adaptor, Label::kNear);
  {
    __ mov(edx, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    __ mov(edx, FieldOperand(edx, JSFunction::kSharedFunctionInfoOffset));
    __ movzx_w(edx, FieldOperand(
                        edx, SharedFunctionInfo::kFormalParameterCountOffset));
    __ mov(ebx, ebp);
  }
  __ jmp(&arguments_done, Label::kNear);
  __ bind(&arguments_adaptor);
  {
    // Just load the length from the ArgumentsAdaptorFrame.
    __ mov(edx, Operand(ebx, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ SmiUntag(edx);
  }
  __ bind(&arguments_done);

  Label stack_done;
  __ sub(edx, ecx);
  __ j(less_equal, &stack_done);
  {
    // Check for stack overflow.
    {
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      Label done;
      __ LoadRoot(ecx, Heap::kRealStackLimitRootIndex);
      // Make ecx the space we have left. The stack might already be
      // overflowed here which will cause ecx to become negative.
      __ neg(ecx);
      __ add(ecx, esp);
      __ sar(ecx, kPointerSizeLog2);
      // Check if the arguments will overflow the stack.
      __ cmp(ecx, edx);
      __ j(greater, &done, Label::kNear);  // Signed comparison.
      __ TailCallRuntime(Runtime::kThrowStackOverflow);
      __ bind(&done);
    }

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ add(eax, edx);
      __ PopReturnAddressTo(ecx);
      __ bind(&loop);
      {
        __ Push(Operand(ebx, edx, times_pointer_size, 1 * kPointerSize));
        __ dec(edx);
        __ j(not_zero, &loop);
      }
      __ PushReturnAddressFrom(ecx);
    }
  }
  __ bind(&stack_done);

  // Restore new.target (in case of [[Construct]]).
  __ movd(edx, xmm0);

  // Tail-call to the {code} handler.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(edi);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ test(FieldOperand(edx, SharedFunctionInfo::kFlagsOffset),
          Immediate(SharedFunctionInfo::IsClassConstructorBit::kMask));
  __ j(not_zero, &class_constructor);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ test(FieldOperand(edx, SharedFunctionInfo::kFlagsOffset),
          Immediate(SharedFunctionInfo::IsNativeBit::kMask |
                    SharedFunctionInfo::IsStrictBit::kMask));
  __ j(not_zero, &done_convert);
  {
    // ----------- S t a t e -------------
    //  -- eax : the number of arguments (not including the receiver)
    //  -- edx : the shared function info.
    //  -- edi : the function to call (checked to be a JSFunction)
    //  -- esi : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(ecx);
    } else {
      Label convert_to_object, convert_receiver;
      __ mov(ecx, Operand(esp, eax, times_pointer_size, kPointerSize));
      __ JumpIfSmi(ecx, &convert_to_object, Label::kNear);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CmpObjectType(ecx, FIRST_JS_RECEIVER_TYPE, ebx);
      __ j(above_equal, &done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(ecx, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy, Label::kNear);
        __ JumpIfNotRoot(ecx, Heap::kNullValueRootIndex, &convert_to_object,
                         Label::kNear);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(ecx);
        }
        __ jmp(&convert_receiver);
      }
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
        __ Push(esi);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(esi);
        __ mov(ecx, eax);
        __ Pop(edi);
        __ Pop(eax);
        __ SmiUntag(eax);
      }
      __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ mov(Operand(esp, eax, times_pointer_size, kPointerSize), ecx);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the shared function info.
  //  -- edi : the function to call (checked to be a JSFunction)
  //  -- esi : the function context.
  // -----------------------------------

  __ movzx_w(
      ebx, FieldOperand(edx, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(eax);
  ParameterCount expected(ebx);
  __ InvokeFunctionCode(edi, no_reg, expected, actual, JUMP_FUNCTION);
  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ push(edi);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : new.target (only in case of [[Construct]])
  //  -- edi : target (checked to be a JSBoundFunction)
  // -----------------------------------

  // Load [[BoundArguments]] into ecx and length of that into ebx.
  Label no_bound_arguments;
  __ mov(ecx, FieldOperand(edi, JSBoundFunction::kBoundArgumentsOffset));
  __ mov(ebx, FieldOperand(ecx, FixedArray::kLengthOffset));
  __ SmiUntag(ebx);
  __ test(ebx, ebx);
  __ j(zero, &no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- eax : the number of arguments (not including the receiver)
    //  -- edx : new.target (only in case of [[Construct]])
    //  -- edi : target (checked to be a JSBoundFunction)
    //  -- ecx : the [[BoundArguments]] (implemented as FixedArray)
    //  -- ebx : the number of [[BoundArguments]]
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ lea(ecx, Operand(ebx, times_pointer_size, 0));
      __ sub(esp, ecx);
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRoot(esp, ecx, Heap::kRealStackLimitRootIndex);
      __ j(greater, &done, Label::kNear);  // Signed comparison.
      // Restore the stack pointer.
      __ lea(esp, Operand(esp, ebx, times_pointer_size, 0));
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Adjust effective number of arguments to include return address.
    __ inc(eax);

    // Relocate arguments and return address down the stack.
    {
      Label loop;
      __ Set(ecx, 0);
      __ lea(ebx, Operand(esp, ebx, times_pointer_size, 0));
      __ bind(&loop);
      __ movd(xmm0, Operand(ebx, ecx, times_pointer_size, 0));
      __ movd(Operand(esp, ecx, times_pointer_size, 0), xmm0);
      __ inc(ecx);
      __ cmp(ecx, eax);
      __ j(less, &loop);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ mov(ecx, FieldOperand(edi, JSBoundFunction::kBoundArgumentsOffset));
      __ mov(ebx, FieldOperand(ecx, FixedArray::kLengthOffset));
      __ SmiUntag(ebx);
      __ bind(&loop);
      __ dec(ebx);
      __ movd(xmm0, FieldOperand(ecx, ebx, times_pointer_size,
                                 FixedArray::kHeaderSize));
      __ movd(Operand(esp, eax, times_pointer_size, 0), xmm0);
      __ lea(eax, Operand(eax, 1));
      __ j(greater, &loop);
    }

    // Adjust effective number of arguments (eax contains the number of
    // arguments from the call plus return address plus the number of
    // [[BoundArguments]]), so we need to subtract one for the return address.
    __ dec(eax);
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(edi);

  // Patch the receiver to [[BoundThis]].
  __ mov(ebx, FieldOperand(edi, JSBoundFunction::kBoundThisOffset));
  __ mov(Operand(esp, eax, times_pointer_size, kPointerSize), ebx);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ mov(edi, FieldOperand(edi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(edi, &non_callable);
  __ bind(&non_smi);
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(equal, masm->isolate()->builtins()->CallFunction(mode),
       RelocInfo::CODE_TARGET);
  __ CmpInstanceType(ecx, JS_BOUND_FUNCTION_TYPE);
  __ j(equal, BUILTIN_CODE(masm->isolate(), CallBoundFunction),
       RelocInfo::CODE_TARGET);

  // Check if target is a proxy and call CallProxy external builtin
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
            Immediate(Map::IsCallableBit::kMask));
  __ j(zero, &non_callable);

  // Call CallProxy external builtin
  __ CmpInstanceType(ecx, JS_PROXY_TYPE);
  __ j(not_equal, &non_function);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ mov(Operand(esp, eax, times_pointer_size, kPointerSize), edi);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadGlobalFunction(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, edi);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(edi);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the new target (checked to be a constructor)
  //  -- edi : the constructor to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertConstructor(edi);
  __ AssertFunction(edi);

  // Calling convention for function specific ConstructStubs require
  // ebx to contain either an AllocationSite or undefined.
  __ LoadRoot(ebx, Heap::kUndefinedValueRootIndex);

  Label call_generic_stub;

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ test(FieldOperand(ecx, SharedFunctionInfo::kFlagsOffset),
          Immediate(SharedFunctionInfo::ConstructAsBuiltinBit::kMask));
  __ j(zero, &call_generic_stub, Label::kNear);

  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET);

  __ bind(&call_generic_stub);
  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the new target (checked to be a constructor)
  //  -- edi : the constructor to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertConstructor(edi);
  __ AssertBoundFunction(edi);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label done;
    __ cmp(edi, edx);
    __ j(not_equal, &done, Label::kNear);
    __ mov(edx, FieldOperand(edi, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&done);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ mov(edi, FieldOperand(edi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- edi : the constructor to call (can be any Object)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor, non_proxy;
  __ JumpIfSmi(edi, &non_constructor, Label::kNear);

  // Check if target has a [[Construct]] internal method.
  __ mov(ecx, FieldOperand(edi, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
            Immediate(Map::IsConstructorBit::kMask));
  __ j(zero, &non_constructor, Label::kNear);

  // Dispatch based on instance type.
  __ CmpInstanceType(ecx, JS_FUNCTION_TYPE);
  __ j(equal, BUILTIN_CODE(masm->isolate(), ConstructFunction),
       RelocInfo::CODE_TARGET);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ CmpInstanceType(ecx, JS_BOUND_FUNCTION_TYPE);
  __ j(equal, BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
       RelocInfo::CODE_TARGET);

  // Only dispatch to proxies after checking whether they are constructors.
  __ CmpInstanceType(ecx, JS_PROXY_TYPE);
  __ j(not_equal, &non_proxy);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
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
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructedNonConstructable),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : actual number of arguments
  //  -- ebx : expected number of arguments
  //  -- edx : new target (passed through to callee)
  //  -- edi : function (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;
  __ IncrementCounter(masm->isolate()->counters()->arguments_adaptors(), 1);

  Label enough, too_few;
  __ cmp(ebx, SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  __ j(equal, &dont_adapt_arguments);
  __ cmp(eax, ebx);
  __ j(less, &too_few);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    // edi is used as a scratch register. It should be restored from the frame
    // when needed.
    Generate_StackOverflowCheck(masm, ebx, ecx, edi, &stack_overflow);

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
    EnterArgumentsAdaptorFrame(masm);
    // edi is used as a scratch register. It should be restored from the frame
    // when needed.
    Generate_StackOverflowCheck(masm, ebx, ecx, edi, &stack_overflow);

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
  __ mov(edi, Operand(ebp, ArgumentsAdaptorFrameConstants::kFunctionOffset));
  // eax : expected number of arguments
  // edx : new target (passed through to callee)
  // edi : function (passed through to callee)
  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  __ mov(ecx, FieldOperand(edi, JSFunction::kCodeOffset));
  __ add(ecx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(ecx);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Leave frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ ret(0);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  __ mov(ecx, FieldOperand(edi, JSFunction::kCodeOffset));
  __ add(ecx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(ecx);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();
  }
}

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ mov(eax, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
    __ mov(eax, Operand(eax, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ mov(eax, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  }

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(eax);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  Label skip;
  // If the code object is null, just return to the caller.
  __ cmp(eax, Immediate(0));
  __ j(not_equal, &skip, Label::kNear);
  __ ret(0);

  __ bind(&skip);

  // Drop any potential handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  if (has_handler_frame) {
    __ leave();
  }

  // Load deoptimization data from the code object.
  __ mov(ebx, Operand(eax, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  __ mov(ebx, Operand(ebx, FixedArray::OffsetOfElementAt(
                               DeoptimizationData::kOsrPcOffsetIndex) -
                               kHeapObjectTag));
  __ SmiUntag(ebx);

  // Compute the target address = code_obj + header_size + osr_offset
  __ lea(eax, Operand(eax, ebx, times_1, Code::kHeaderSize - kHeapObjectTag));

  // Overwrite the return address on the stack.
  __ mov(Operand(esp, 0), eax);

  // And "return" to the OSR entry point of the function.
  __ ret(0);
}

void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  Generate_OnStackReplacementHelper(masm, false);
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  Generate_OnStackReplacementHelper(masm, true);
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was put in edi by the jump table trampoline.
  // Convert to Smi for the runtime call.
  __ SmiTag(edi);
  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameScope scope(masm, StackFrame::WASM_COMPILE_LAZY);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    static_assert(WasmCompileLazyFrameConstants::kNumberOfSavedGpParamRegs ==
                      arraysize(wasm::kGpParamRegisters),
                  "frame size mismatch");
    for (Register reg : wasm::kGpParamRegisters) {
      __ Push(reg);
    }
    static_assert(WasmCompileLazyFrameConstants::kNumberOfSavedFpParamRegs ==
                      arraysize(wasm::kFpParamRegisters),
                  "frame size mismatch");
    __ sub(esp, Immediate(kSimd128Size * arraysize(wasm::kFpParamRegisters)));
    int offset = 0;
    for (DoubleRegister reg : wasm::kFpParamRegisters) {
      __ movdqu(Operand(esp, offset), reg);
      offset += kSimd128Size;
    }

    // Push the WASM instance as an explicit argument to WasmCompileLazy.
    __ Push(kWasmInstanceRegister);
    // Push the function index as second argument.
    __ Push(edi);
    // Load the correct CEntry builtin from the instance object.
    __ mov(ecx, FieldOperand(kWasmInstanceRegister,
                             WasmInstanceObject::kCEntryStubOffset));
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Move(kContextRegister, Smi::kZero);
    __ CallRuntimeWithCEntry(Runtime::kWasmCompileLazy, ecx);
    // The entrypoint address is the return value.
    __ mov(edi, kReturnRegister0);

    // Restore registers.
    for (DoubleRegister reg : base::Reversed(wasm::kFpParamRegisters)) {
      offset -= kSimd128Size;
      __ movdqu(reg, Operand(esp, offset));
    }
    DCHECK_EQ(0, offset);
    __ add(esp, Immediate(kSimd128Size * arraysize(wasm::kFpParamRegisters)));
    for (Register reg : base::Reversed(wasm::kGpParamRegisters)) {
      __ Pop(reg);
    }
  }
  // Finally, jump to the entrypoint.
  __ jmp(edi);
}

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               SaveFPRegsMode save_doubles, ArgvMode argv_mode,
                               bool builtin_exit_frame) {
  // eax: number of arguments including receiver
  // edx: pointer to C function
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // esi: current context (C callee-saved)
  // edi: JS function of the caller (C callee-saved)
  //
  // If argv_mode == kArgvInRegister:
  // ecx: pointer to the first argument

  STATIC_ASSERT(eax == kRuntimeCallArgCountRegister);
  STATIC_ASSERT(ecx == kRuntimeCallArgvRegister);
  STATIC_ASSERT(edx == kRuntimeCallFunctionRegister);
  STATIC_ASSERT(esi == kContextRegister);
  STATIC_ASSERT(edi == kJSFunctionRegister);

  DCHECK(!AreAliased(kRuntimeCallArgCountRegister, kRuntimeCallArgvRegister,
                     kRuntimeCallFunctionRegister, kContextRegister,
                     kJSFunctionRegister, kRootRegister));

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Reserve space on the stack for the three arguments passed to the call. If
  // result size is greater than can be returned in registers, also reserve
  // space for the hidden argument for the result location, and space for the
  // result itself.
  int arg_stack_space = 3;

  // Enter the exit frame that transitions from JavaScript to C++.
  if (argv_mode == kArgvInRegister) {
    DCHECK(save_doubles == kDontSaveFPRegs);
    DCHECK(!builtin_exit_frame);
    __ EnterApiExitFrame(arg_stack_space);

    // Move argc and argv into the correct registers.
    __ mov(esi, ecx);
    __ mov(edi, eax);
  } else {
    __ EnterExitFrame(
        arg_stack_space, save_doubles == kSaveFPRegs,
        builtin_exit_frame ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);
  }

  // edx: pointer to C function
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // edi: number of arguments including receiver  (C callee-saved)
  // esi: pointer to the first argument (C callee-saved)

  // Result returned in eax, or eax+edx if result size is 2.

  // Check stack alignment.
  if (FLAG_debug_code) {
    __ CheckStackAlignment();
  }
  // Call C function.
  __ mov(Operand(esp, 0 * kPointerSize), edi);  // argc.
  __ mov(Operand(esp, 1 * kPointerSize), esi);  // argv.
  __ mov(Operand(esp, 2 * kPointerSize),
         Immediate(ExternalReference::isolate_address(masm->isolate())));
  __ call(kRuntimeCallFunctionRegister);

  // Result is in eax or edx:eax - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ cmp(eax, masm->isolate()->factory()->exception());
  __ j(equal, &exception_returned);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    __ push(edx);
    __ mov(edx, Immediate(masm->isolate()->factory()->the_hole_value()));
    Label okay;
    ExternalReference pending_exception_address = ExternalReference::Create(
        IsolateAddressId::kPendingExceptionAddress, masm->isolate());
    __ cmp(edx, __ StaticVariable(pending_exception_address));
    // Cannot use check here as it attempts to generate call into runtime.
    __ j(equal, &okay, Label::kNear);
    __ int3();
    __ bind(&okay);
    __ pop(edx);
  }

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(save_doubles == kSaveFPRegs, argv_mode == kArgvOnStack);
  __ ret(0);

  // Handling of exception.
  __ bind(&exception_returned);

  ExternalReference pending_handler_context_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerContextAddress, masm->isolate());
  ExternalReference pending_handler_entrypoint_address =
      ExternalReference::Create(
          IsolateAddressId::kPendingHandlerEntrypointAddress, masm->isolate());
  ExternalReference pending_handler_fp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerFPAddress, masm->isolate());
  ExternalReference pending_handler_sp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerSPAddress, masm->isolate());

  // Ask the runtime for help to determine the handler. This will set eax to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler =
      ExternalReference::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, eax);
    __ mov(Operand(esp, 0 * kPointerSize), Immediate(0));  // argc.
    __ mov(Operand(esp, 1 * kPointerSize), Immediate(0));  // argv.
    __ mov(Operand(esp, 2 * kPointerSize),
           Immediate(ExternalReference::isolate_address(masm->isolate())));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ mov(esi, __ StaticVariable(pending_handler_context_address));
  __ mov(esp, __ StaticVariable(pending_handler_sp_address));
  __ mov(ebp, __ StaticVariable(pending_handler_fp_address));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (esi == 0) for non-JS frames.
  Label skip;
  __ test(esi, esi);
  __ j(zero, &skip, Label::kNear);
  __ mov(Operand(ebp, StandardFrameConstants::kContextOffset), esi);
  __ bind(&skip);

  // Reset the masking register. This is done independent of the underlying
  // feature flag {FLAG_branch_load_poisoning} to make the snapshot work with
  // both configurations. It is safe to always do this, because the underlying
  // register is caller-saved and can be arbitrarily clobbered.
  __ ResetSpeculationPoisonRegister();

  // Compute the handler entry address and jump to it.
  __ mov(edi, __ StaticVariable(pending_handler_entrypoint_address));
  __ jmp(edi);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label check_negative, process_64_bits, done;

  // Account for return address and saved regs.
  const int kArgumentOffset = 4 * kPointerSize;

  MemOperand mantissa_operand(MemOperand(esp, kArgumentOffset));
  MemOperand exponent_operand(
      MemOperand(esp, kArgumentOffset + kDoubleSize / 2));

  // The result is returned on the stack.
  MemOperand return_operand = mantissa_operand;

  Register scratch1 = ebx;

  // Since we must use ecx for shifts below, use some other register (eax)
  // to calculate the result.
  Register result_reg = eax;
  // Save ecx if it isn't the return register and therefore volatile, or if it
  // is the return register, then save the temp register we use in its stead for
  // the result.
  Register save_reg = eax;
  __ push(ecx);
  __ push(scratch1);
  __ push(save_reg);

  __ mov(scratch1, mantissa_operand);
  if (CpuFeatures::IsSupported(SSE3)) {
    CpuFeatureScope scope(masm, SSE3);
    // Load x87 register with heap number.
    __ fld_d(mantissa_operand);
  }
  __ mov(ecx, exponent_operand);

  __ and_(ecx, HeapNumber::kExponentMask);
  __ shr(ecx, HeapNumber::kExponentShift);
  __ lea(result_reg, MemOperand(ecx, -HeapNumber::kExponentBias));
  __ cmp(result_reg, Immediate(HeapNumber::kMantissaBits));
  __ j(below, &process_64_bits);

  // Result is entirely in lower 32-bits of mantissa
  int delta = HeapNumber::kExponentBias + Double::kPhysicalSignificandSize;
  if (CpuFeatures::IsSupported(SSE3)) {
    __ fstp(0);
  }
  __ sub(ecx, Immediate(delta));
  __ xor_(result_reg, result_reg);
  __ cmp(ecx, Immediate(31));
  __ j(above, &done);
  __ shl_cl(scratch1);
  __ jmp(&check_negative);

  __ bind(&process_64_bits);
  if (CpuFeatures::IsSupported(SSE3)) {
    CpuFeatureScope scope(masm, SSE3);
    // Reserve space for 64 bit answer.
    __ sub(esp, Immediate(kDoubleSize));  // Nolint.
    // Do conversion, which cannot fail because we checked the exponent.
    __ fisttp_d(Operand(esp, 0));
    __ mov(result_reg, Operand(esp, 0));  // Load low word of answer as result
    __ add(esp, Immediate(kDoubleSize));
    __ jmp(&done);
  } else {
    // Result must be extracted from shifted 32-bit mantissa
    __ sub(ecx, Immediate(delta));
    __ neg(ecx);
    __ mov(result_reg, exponent_operand);
    __ and_(result_reg,
            Immediate(static_cast<uint32_t>(Double::kSignificandMask >> 32)));
    __ add(result_reg,
           Immediate(static_cast<uint32_t>(Double::kHiddenBit >> 32)));
    __ shrd_cl(scratch1, result_reg);
    __ shr_cl(result_reg);
    __ test(ecx, Immediate(32));
    __ cmov(not_equal, scratch1, result_reg);
  }

  // If the double was negative, negate the integer result.
  __ bind(&check_negative);
  __ mov(result_reg, scratch1);
  __ neg(result_reg);
  __ cmp(exponent_operand, Immediate(0));
  __ cmov(greater, result_reg, scratch1);

  // Restore registers
  __ bind(&done);
  __ mov(return_operand, result_reg);
  __ pop(save_reg);
  __ pop(scratch1);
  __ pop(ecx);
  __ ret(0);
}

void Builtins::Generate_MathPowInternal(MacroAssembler* masm) {
  const Register exponent = eax;
  const Register scratch = ecx;
  const XMMRegister double_result = xmm3;
  const XMMRegister double_base = xmm2;
  const XMMRegister double_exponent = xmm1;
  const XMMRegister double_scratch = xmm4;

  Label call_runtime, done, exponent_not_smi, int_exponent;

  // Save 1 in double_result - we need this several times later on.
  __ mov(scratch, Immediate(1));
  __ Cvtsi2sd(double_result, scratch);

  Label fast_power, try_arithmetic_simplification;
  __ DoubleToI(exponent, double_exponent, double_scratch,
               &try_arithmetic_simplification, &try_arithmetic_simplification);
  __ jmp(&int_exponent);

  __ bind(&try_arithmetic_simplification);
  // Skip to runtime if possibly NaN (indicated by the indefinite integer).
  __ cvttsd2si(exponent, Operand(double_exponent));
  __ cmp(exponent, Immediate(0x1));
  __ j(overflow, &call_runtime);

  // Using FPU instructions to calculate power.
  Label fast_power_failed;
  __ bind(&fast_power);
  __ fnclex();  // Clear flags to catch exceptions later.
  // Transfer (B)ase and (E)xponent onto the FPU register stack.
  __ sub(esp, Immediate(kDoubleSize));
  __ movsd(Operand(esp, 0), double_exponent);
  __ fld_d(Operand(esp, 0));  // E
  __ movsd(Operand(esp, 0), double_base);
  __ fld_d(Operand(esp, 0));  // B, E

  // Exponent is in st(1) and base is in st(0)
  // B ^ E = (2^(E * log2(B)) - 1) + 1 = (2^X - 1) + 1 for X = E * log2(B)
  // FYL2X calculates st(1) * log2(st(0))
  __ fyl2x();    // X
  __ fld(0);     // X, X
  __ frndint();  // rnd(X), X
  __ fsub(1);    // rnd(X), X-rnd(X)
  __ fxch(1);    // X - rnd(X), rnd(X)
  // F2XM1 calculates 2^st(0) - 1 for -1 < st(0) < 1
  __ f2xm1();   // 2^(X-rnd(X)) - 1, rnd(X)
  __ fld1();    // 1, 2^(X-rnd(X)) - 1, rnd(X)
  __ faddp(1);  // 2^(X-rnd(X)), rnd(X)
  // FSCALE calculates st(0) * 2^st(1)
  __ fscale();  // 2^X, rnd(X)
  __ fstp(1);   // 2^X
  // Bail out to runtime in case of exceptions in the status word.
  __ fnstsw_ax();
  __ test_b(eax, Immediate(0x5F));  // We check for all but precision exception.
  __ j(not_zero, &fast_power_failed, Label::kNear);
  __ fstp_d(Operand(esp, 0));
  __ movsd(double_result, Operand(esp, 0));
  __ add(esp, Immediate(kDoubleSize));
  __ jmp(&done);

  __ bind(&fast_power_failed);
  __ fninit();
  __ add(esp, Immediate(kDoubleSize));
  __ jmp(&call_runtime);

  // Calculate power with integer exponent.
  __ bind(&int_exponent);
  const XMMRegister double_scratch2 = double_exponent;
  __ mov(scratch, exponent);                 // Back up exponent.
  __ movsd(double_scratch, double_base);     // Back up base.
  __ movsd(double_scratch2, double_result);  // Load double_exponent with 1.

  // Get absolute value of exponent.
  Label no_neg, while_true, while_false;
  __ test(scratch, scratch);
  __ j(positive, &no_neg, Label::kNear);
  __ neg(scratch);
  __ bind(&no_neg);

  __ j(zero, &while_false, Label::kNear);
  __ shr(scratch, 1);
  // Above condition means CF==0 && ZF==0.  This means that the
  // bit that has been shifted out is 0 and the result is not 0.
  __ j(above, &while_true, Label::kNear);
  __ movsd(double_result, double_scratch);
  __ j(zero, &while_false, Label::kNear);

  __ bind(&while_true);
  __ shr(scratch, 1);
  __ mulsd(double_scratch, double_scratch);
  __ j(above, &while_true, Label::kNear);
  __ mulsd(double_result, double_scratch);
  __ j(not_zero, &while_true);

  __ bind(&while_false);
  // scratch has the original value of the exponent - if the exponent is
  // negative, return 1/result.
  __ test(exponent, exponent);
  __ j(positive, &done);
  __ divsd(double_scratch2, double_result);
  __ movsd(double_result, double_scratch2);
  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ xorps(double_scratch2, double_scratch2);
  __ ucomisd(double_scratch2, double_result);  // Result cannot be NaN.
  // double_exponent aliased as double_scratch2 has already been overwritten
  // and may not have contained the exponent value in the first place when the
  // exponent is a smi.  We reset it with exponent value before bailing out.
  __ j(not_equal, &done);
  __ Cvtsi2sd(double_exponent, exponent);

  // Returning or bailing out.
  __ bind(&call_runtime);
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ PrepareCallCFunction(4, scratch);
    __ movsd(Operand(esp, 0 * kDoubleSize), double_base);
    __ movsd(Operand(esp, 1 * kDoubleSize), double_exponent);
    __ CallCFunction(ExternalReference::power_double_double_function(), 4);
  }
  // Return value is in st(0) on ia32.
  // Store it into the (fixed) result register.
  __ sub(esp, Immediate(kDoubleSize));
  __ fstp_d(Operand(esp, 0));
  __ movsd(double_result, Operand(esp, 0));
  __ add(esp, Immediate(kDoubleSize));

  __ bind(&done);
  __ ret(0);
}

namespace {

void GenerateInternalArrayConstructorCase(MacroAssembler* masm,
                                          ElementsKind kind) {
  Label not_zero_case, not_one_case;
  Label normal_sequence;

  __ test(eax, eax);
  __ j(not_zero, &not_zero_case);
  __ Jump(CodeFactory::InternalArrayNoArgumentConstructor(masm->isolate(), kind)
              .code(),
          RelocInfo::CODE_TARGET);

  __ bind(&not_zero_case);
  __ cmp(eax, 1);
  __ j(greater, &not_one_case);

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument
    __ mov(ecx, Operand(esp, kPointerSize));
    __ test(ecx, ecx);
    __ j(zero, &normal_sequence);

    __ Jump(CodeFactory::InternalArraySingleArgumentConstructor(
                masm->isolate(), GetHoleyElementsKind(kind))
                .code(),
            RelocInfo::CODE_TARGET);
  }

  __ bind(&normal_sequence);
  __ Jump(
      CodeFactory::InternalArraySingleArgumentConstructor(masm->isolate(), kind)
          .code(),
      RelocInfo::CODE_TARGET);

  __ bind(&not_one_case);
  // TODO(v8:6666): When rewriting ia32 ASM builtins to not clobber the
  // kRootRegister ebx, this useless move can be removed.
  __ Move(kJavaScriptCallExtraArg1Register, ebx);
  Handle<Code> code = BUILTIN_CODE(masm->isolate(), ArrayNArgumentsConstructor);
  __ Jump(code, RelocInfo::CODE_TARGET);
}

}  // namespace

void Builtins::Generate_InternalArrayConstructorImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argc
  //  -- edi : constructor
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_zero, AbortReason::kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(ecx, MAP_TYPE, ecx);
    __ Assert(equal, AbortReason::kUnexpectedInitialMapForArrayFunction);
  }

  // Figure out the right elements kind
  __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));

  // Load the map's "bit field 2" into |result|. We only need the first byte,
  // but the following masking takes care of that anyway.
  __ mov(ecx, FieldOperand(ecx, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ DecodeField<Map::ElementsKindBits>(ecx);

  if (FLAG_debug_code) {
    Label done;
    __ cmp(ecx, Immediate(PACKED_ELEMENTS));
    __ j(equal, &done);
    __ cmp(ecx, Immediate(HOLEY_ELEMENTS));
    __ Assert(
        equal,
        AbortReason::kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ cmp(ecx, Immediate(PACKED_ELEMENTS));
  __ j(equal, &fast_elements_case);
  GenerateInternalArrayConstructorCase(masm, HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateInternalArrayConstructorCase(masm, PACKED_ELEMENTS);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32

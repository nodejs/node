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

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
  // ----------- S t a t e -------------
  //  -- eax                : number of arguments excluding receiver
  //  -- edi                : target
  //  -- edx                : new.target
  //  -- esp[0]             : return address
  //  -- esp[4]             : last argument
  //  -- ...
  //  -- esp[4 * argc]      : first argument
  //  -- esp[4 * (argc +1)] : receiver
  // -----------------------------------
  __ AssertFunction(edi);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // JumpToExternalReference expects eax to contain the number of arguments
  // including the receiver and the extra arguments.
  const int num_extra_args = 3;
  __ add(eax, Immediate(num_extra_args + 1));

  // Insert extra arguments.
  __ PopReturnAddressTo(ecx);
  __ SmiTag(eax);
  __ Push(eax);
  __ SmiUntag(eax);
  __ Push(edi);
  __ Push(edx);
  __ PushReturnAddressFrom(ecx);

  __ JumpToExternalReference(ExternalReference(address, masm->isolate()),
                             exit_frame_type == BUILTIN_EXIT);
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
    __ mov(ebx, eax);

    // Restore target function and new target.
    __ pop(edx);
    __ pop(edi);
    __ pop(eax);
    __ SmiUntag(eax);
  }

  __ lea(ebx, FieldOperand(ebx, Code::kHeaderSize));
  __ jmp(ebx);
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ mov(ebx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ebx, FieldOperand(ebx, SharedFunctionInfo::kCodeOffset));
  __ lea(ebx, FieldOperand(ebx, Code::kHeaderSize));
  __ jmp(ebx);
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

  GenerateTailCallToReturnedCode(masm, Runtime::kTryInstallOptimizedCode);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
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
    __ InvokeFunction(edi, edx, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

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

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Generate_JSConstructStubGeneric(MacroAssembler* masm,
                                     bool restrict_constructor_return) {
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
    __ Push(edx);

    // ----------- S t a t e -------------
    //  --         sp[0*kPointerSize]: new target
    //  -- edi and sp[1*kPointerSize]: constructor function
    //  --         sp[2*kPointerSize]: argument count
    //  --         sp[3*kPointerSize]: context
    // -----------------------------------

    __ mov(ebx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ test_b(FieldOperand(ebx, SharedFunctionInfo::kFunctionKindByteOffset),
              Immediate(SharedFunctionInfo::kDerivedConstructorBitsWithinByte));
    __ j(not_zero, &not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1);
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ jmp(&post_instantiation_deopt_entry, Label::kNear);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(eax, Heap::kTheHoleValueRootIndex);

    // ----------- S t a t e -------------
    //  --                         eax: implicit receiver
    //  -- Slot 3 / sp[0*kPointerSize]: new target
    //  -- Slot 2 / sp[1*kPointerSize]: constructor function
    //  -- Slot 1 / sp[2*kPointerSize]: number of arguments (tagged)
    //  -- Slot 0 / sp[3*kPointerSize]: context
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
    //  -- sp[2*kPointerSize]: constructor function
    //  -- sp[3*kPointerSize]: number of arguments (tagged)
    //  -- sp[4*kPointerSize]: context
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
    //  -- edi and sp[2*kPointerSize]: constructor function
    //  --         sp[3*kPointerSize]: number of arguments (tagged)
    //  --         sp[4*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry, Label::kNear);
    __ bind(&loop);
    __ Push(Operand(ebx, ecx, times_pointer_size, 0));
    __ bind(&entry);
    __ dec(ecx);
    __ j(greater_equal, &loop);

    // Call the function.
    ParameterCount actual(eax);
    __ InvokeFunction(edi, edx, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // ----------- S t a t e -------------
    //  --                eax: constructor result
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: constructor function
    //  -- sp[2*kPointerSize]: number of arguments
    //  -- sp[3*kPointerSize]: context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore context from the frame.
    __ mov(esi, Operand(ebp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, other_result, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(eax, Heap::kUndefinedValueRootIndex, &use_receiver,
                  Label::kNear);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(eax, &other_result, Label::kNear);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, ecx);
    __ j(above_equal, &leave_frame, Label::kNear);

    __ bind(&other_result);
    // The result is now neither undefined nor an object.
    if (restrict_constructor_return) {
      // Throw if constructor function is a class constructor
      __ mov(ebx, Operand(ebp, ConstructFrameConstants::kConstructorOffset));
      __ mov(ebx, FieldOperand(ebx, JSFunction::kSharedFunctionInfoOffset));
      __ test_b(FieldOperand(ebx, SharedFunctionInfo::kFunctionKindByteOffset),
                Immediate(SharedFunctionInfo::kClassConstructorBitsWithinByte));
      __ j(Condition::zero, &use_receiver, Label::kNear);
    } else {
      __ jmp(&use_receiver, Label::kNear);
    }

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
}  // namespace

void Builtins::Generate_JSConstructStubGenericRestrictedReturn(
    MacroAssembler* masm) {
  return Generate_JSConstructStubGeneric(masm, true);
}
void Builtins::Generate_JSConstructStubGenericUnrestrictedReturn(
    MacroAssembler* masm) {
  return Generate_JSConstructStubGeneric(masm, false);
}
void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}
void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ push(edi);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
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
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

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

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : the value to pass to the generator
  //  -- ebx    : the JSGeneratorObject to resume
  //  -- edx    : the resume mode (tagged)
  //  -- ecx    : the SuspendFlags of the earlier suspend call (tagged)
  //  -- esp[0] : return address
  // -----------------------------------
  __ SmiUntag(ecx);
  __ AssertGeneratorObject(ebx, ecx);

  // Store input value into generator object.
  Label async_await, done_store_input;

  __ and_(ecx, Immediate(static_cast<int>(SuspendFlags::kAsyncGeneratorAwait)));
  __ cmpb(ecx, Immediate(static_cast<int>(SuspendFlags::kAsyncGeneratorAwait)));
  __ j(equal, &async_await, Label::kNear);

  __ mov(FieldOperand(ebx, JSGeneratorObject::kInputOrDebugPosOffset), eax);
  __ RecordWriteField(ebx, JSGeneratorObject::kInputOrDebugPosOffset, eax, ecx,
                      kDontSaveFPRegs);
  __ jmp(&done_store_input, Label::kNear);

  __ bind(&async_await);
  __ mov(FieldOperand(ebx, JSAsyncGeneratorObject::kAwaitInputOrDebugPosOffset),
         eax);
  __ RecordWriteField(ebx, JSAsyncGeneratorObject::kAwaitInputOrDebugPosOffset,
                      eax, ecx, kDontSaveFPRegs);
  __ jmp(&done_store_input, Label::kNear);

  __ bind(&done_store_input);
  // `ecx` no longer holds SuspendFlags

  // Store resume mode into generator object.
  __ mov(FieldOperand(ebx, JSGeneratorObject::kResumeModeOffset), edx);

  // Load suspended function and context.
  __ mov(edi, FieldOperand(ebx, JSGeneratorObject::kFunctionOffset));
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ cmpb(Operand::StaticVariable(debug_hook), Immediate(0));
  __ j(not_equal, &prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ cmp(ebx, Operand::StaticVariable(debug_suspended_generator));
  __ j(equal, &prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Pop return address.
  __ PopReturnAddressTo(eax);

  // Push receiver.
  __ Push(FieldOperand(ebx, JSGeneratorObject::kReceiverOffset));

  // ----------- S t a t e -------------
  //  -- eax    : return address
  //  -- ebx    : the JSGeneratorObject to resume
  //  -- edx    : the resume mode (tagged)
  //  -- edi    : generator function
  //  -- esi    : generator context
  //  -- esp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ecx,
         FieldOperand(ecx, SharedFunctionInfo::kFormalParameterCountOffset));
  {
    Label done_loop, loop;
    __ bind(&loop);
    __ sub(ecx, Immediate(Smi::FromInt(1)));
    __ j(carry, &done_loop, Label::kNear);
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    __ jmp(&loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ mov(ecx, FieldOperand(ecx, SharedFunctionInfo::kFunctionDataOffset));
    __ CmpObjectType(ecx, BYTECODE_ARRAY_TYPE, ecx);
    __ Assert(equal, kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ PushReturnAddressFrom(eax);
    __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ mov(eax,
           FieldOperand(eax, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ mov(edx, ebx);
    __ jmp(FieldOperand(edi, JSFunction::kCodeEntryOffset));
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(ebx);
    __ Push(edx);
    __ Push(edi);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(edx);
    __ Pop(ebx);
    __ mov(edi, FieldOperand(ebx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(ebx);
    __ Push(edx);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(edx);
    __ Pop(ebx);
    __ mov(edi, FieldOperand(ebx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);
}

static void ReplaceClosureEntryWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code_entry, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {
  Register native_context = scratch1;

  // Store the optimized code in the closure.
  __ lea(optimized_code_entry,
         FieldOperand(optimized_code_entry, Code::kHeaderSize));
  __ mov(FieldOperand(closure, JSFunction::kCodeEntryOffset),
         optimized_code_entry);
  __ RecordWriteCodeEntryField(closure, optimized_code_entry, scratch2);

  // Link the closure into the optimized function list.
  __ mov(native_context, NativeContextOperand());
  __ mov(scratch3,
         ContextOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  __ mov(FieldOperand(closure, JSFunction::kNextFunctionLinkOffset), scratch3);
  __ RecordWriteField(closure, JSFunction::kNextFunctionLinkOffset, scratch3,
                      scratch2, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  const int function_list_offset =
      Context::SlotOffset(Context::OPTIMIZED_FUNCTIONS_LIST);
  __ mov(ContextOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST),
         closure);
  // Save closure before the write barrier.
  __ mov(scratch3, closure);
  __ RecordWriteContextSlot(native_context, function_list_offset, closure,
                            scratch2, kDontSaveFPRegs);
  __ mov(closure, scratch3);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch1,
                                  Register scratch2) {
  Register args_count = scratch1;
  Register return_pc = scratch2;

  // Get the arguments + reciever count.
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

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o edi: the JS function object being called
//   o edx: the new target
//   o esi: our context
//   o ebp: the caller's frame pointer
//   o esp: stack pointer (pointing to return address)
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ push(ebp);  // Caller's frame pointer.
  __ mov(ebp, esp);
  __ push(esi);  // Callee's context.
  __ push(edi);  // Callee's JS function.
  __ push(edx);  // Callee's new target.

  // First check if there is optimized code in the feedback vector which we
  // could call instead.
  Label switch_to_optimized_code;
  Register optimized_code_entry = ecx;
  __ mov(ebx, FieldOperand(edi, JSFunction::kFeedbackVectorOffset));
  __ mov(ebx, FieldOperand(ebx, Cell::kValueOffset));
  __ mov(optimized_code_entry,
         FieldOperand(ebx, FeedbackVector::kOptimizedCodeIndex * kPointerSize +
                               FeedbackVector::kHeaderSize));
  __ mov(optimized_code_entry,
         FieldOperand(optimized_code_entry, WeakCell::kValueOffset));
  __ JumpIfNotSmi(optimized_code_entry, &switch_to_optimized_code);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  Label load_debug_bytecode_array, bytecode_array_loaded;
  __ JumpIfNotSmi(FieldOperand(eax, SharedFunctionInfo::kDebugInfoOffset),
                  &load_debug_bytecode_array);
  __ mov(kInterpreterBytecodeArrayRegister,
         FieldOperand(eax, SharedFunctionInfo::kFunctionDataOffset));
  __ bind(&bytecode_array_loaded);

  // Check whether we should continue to use the interpreter.
  // TODO(rmcilroy) Remove self healing once liveedit only has to deal with
  // Ignition bytecode.
  Label switch_to_different_code_kind;
  __ Move(ecx, masm->CodeObject());  // Self-reference to this code.
  __ cmp(ecx, FieldOperand(eax, SharedFunctionInfo::kCodeOffset));
  __ j(not_equal, &switch_to_different_code_kind);

  // Increment invocation count for the function.
  __ EmitLoadFeedbackVector(ecx);
  __ add(
      FieldOperand(ecx, FeedbackVector::kInvocationCountIndex * kPointerSize +
                            FeedbackVector::kHeaderSize),
      Immediate(Smi::FromInt(1)));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     eax);
    __ Assert(equal, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
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
    __ cmp(ecx, Operand::StaticVariable(stack_limit));
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

  // Load accumulator, bytecode offset and dispatch table into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ mov(kInterpreterBytecodeOffsetRegister,
         Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ mov(kInterpreterDispatchTableRegister,
         Immediate(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Dispatch to the first bytecode handler for the function.
  __ movzx_b(ebx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ mov(ebx, Operand(kInterpreterDispatchTableRegister, ebx,
                      times_pointer_size, 0));
  __ call(ebx);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // The return value is in eax.
  LeaveInterpreterFrame(masm, ebx, ecx);
  __ ret(0);

  // Load debug copy of the bytecode array.
  __ bind(&load_debug_bytecode_array);
  Register debug_info = kInterpreterBytecodeArrayRegister;
  __ mov(debug_info, FieldOperand(eax, SharedFunctionInfo::kDebugInfoOffset));
  __ mov(kInterpreterBytecodeArrayRegister,
         FieldOperand(debug_info, DebugInfo::kDebugBytecodeArrayIndex));
  __ jmp(&bytecode_array_loaded);

  // If the shared code is no longer this entry trampoline, then the underlying
  // function has been switched to a different kind of code and we heal the
  // closure by switching the code entry field over to the new code as well.
  __ bind(&switch_to_different_code_kind);
  __ pop(edx);  // Callee's new target.
  __ pop(edi);  // Callee's JS function.
  __ pop(esi);  // Callee's context.
  __ leave();   // Leave the frame so we can tail call.
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ecx, FieldOperand(ecx, SharedFunctionInfo::kCodeOffset));
  __ lea(ecx, FieldOperand(ecx, Code::kHeaderSize));
  __ mov(FieldOperand(edi, JSFunction::kCodeEntryOffset), ecx);
  __ RecordWriteCodeEntryField(edi, ecx, ebx);
  __ jmp(ecx);

  // If there is optimized code on the type feedback vector, check if it is good
  // to run, and if so, self heal the closure and call the optimized code.
  __ bind(&switch_to_optimized_code);
  Label gotta_call_runtime;

  // Check if the optimized code is marked for deopt.
  __ test(FieldOperand(optimized_code_entry, Code::kKindSpecificFlags1Offset),
          Immediate(1 << Code::kMarkedForDeoptimizationBit));
  __ j(not_zero, &gotta_call_runtime);

  // Optimized code is good, get it into the closure and link the closure into
  // the optimized functions list, then tail call the optimized code.
  __ push(edx);
  ReplaceClosureEntryWithOptimizedCode(masm, optimized_code_entry, edi, edx,
                                       eax, ebx);
  __ pop(edx);
  __ leave();
  __ jmp(optimized_code_entry);

  // Optimized code is marked for deopt, bailout to the CompileLazy runtime
  // function which will clear the feedback vector's optimized code slot.
  __ bind(&gotta_call_runtime);
  __ leave();
  GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
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
  __ mov(scratch1, Operand::StaticVariable(real_stack_limit));
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
    TailCallMode tail_call_mode, InterpreterPushArgsMode mode) {
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

  // Call the target.
  __ Push(edx);  // Re-push return address.

  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ Jump(masm->isolate()->builtins()->CallFunction(ConvertReceiverMode::kAny,
                                                      tail_call_mode),
            RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(masm->isolate()->builtins()->CallWithSpread(),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                              tail_call_mode),
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
// the Generate_StackOverflowCheck.

#ifdef _MSC_VER
  // TODO(mythria): Move it to macro assembler.
  // In windows, we cannot increment the stack size by more than one page
  // (mimimum page size is 4KB) without accessing at least one byte on the
  // page. Check this:
  // https://msdn.microsoft.com/en-us/library/aa227153(v=vs.60).aspx.
  const int page_size = 4 * 1024;
  Label check_offset, update_stack_pointer;
  __ bind(&check_offset);
  __ cmp(scratch1, page_size);
  __ j(less, &update_stack_pointer);
  __ sub(esp, Immediate(page_size));
  // Just to touch the page, before we increment further.
  __ mov(Operand(esp, 0), Immediate(0));
  __ sub(scratch1, Immediate(page_size));
  __ jmp(&check_offset);
  __ bind(&update_stack_pointer);
#endif

  __ sub(esp, scratch1);

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

  __ AssertUndefinedOrAllocationSite(ebx);
  if (mode == InterpreterPushArgsMode::kJSFunction) {
    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ AssertFunction(edi);

    __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ mov(ecx, FieldOperand(ecx, SharedFunctionInfo::kConstructStubOffset));
    __ lea(ecx, FieldOperand(ecx, Code::kHeaderSize));
    __ jmp(ecx);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with unmodified eax, edi, edx values.
    __ Jump(masm->isolate()->builtins()->ConstructWithSpread(),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with unmodified eax, edi, edx values.
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
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

// static
void Builtins::Generate_InterpreterPushArgsThenConstructArray(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the target to call checked to be Array function.
  //  -- ebx : the allocation site feedback
  //  -- ecx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  // -----------------------------------
  Label stack_overflow;
  // We need two scratch registers. Register edi is available, push edx onto
  // stack.
  __ Push(edx);

  // Push arguments and move return address to the top of stack.
  // The eax register is readonly. The ecx register will be modified. The edx
  // and edi registers will be modified but restored to their original values.
  Generate_InterpreterPushZeroAndArgsAndReturnAddress(masm, eax, ecx, edx, edi,
                                                      1, &stack_overflow);

  // Restore edx.
  __ Pop(edx);

  // Array constructor expects constructor in edi. It is same as edx here.
  __ Move(edi, edx);

  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);

  __ bind(&stack_overflow);
  {
    // Pop the temporary registers, so that return address is on top of stack.
    __ Pop(edx);

    __ TailCallRuntime(Runtime::kThrowStackOverflow);

    // This should be unreachable.
    __ int3();
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Smi* interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);
  __ LoadHeapObject(ebx,
                    masm->isolate()->builtins()->InterpreterEntryTrampoline());
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
    __ Assert(equal, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ movzx_b(ebx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ mov(ebx, Operand(kInterpreterDispatchTableRegister, ebx,
                      times_pointer_size, 0));
  __ jmp(ebx);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ mov(ebx, Operand(ebp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ mov(edx, Operand(ebp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(kInterpreterAccumulatorRegister);
    __ Push(ebx);  // First argument is the bytecode array.
    __ Push(edx);  // Second argument is the bytecode offset.
    __ CallRuntime(Runtime::kInterpreterAdvanceBytecodeOffset);
    __ Move(edx, eax);  // Result is the new bytecode offset.
    __ Pop(kInterpreterAccumulatorRegister);
  }
  __ mov(Operand(ebp, InterpreterFrameConstants::kBytecodeOffsetFromFp), edx);

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argument count (preserved for callee)
  //  -- edx : new target (preserved for callee)
  //  -- edi : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;
  Label try_shared;

  Register closure = edi;
  Register new_target = edx;
  Register argument_count = eax;

  // Do we have a valid feedback vector?
  __ mov(ebx, FieldOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ mov(ebx, FieldOperand(ebx, Cell::kValueOffset));
  __ JumpIfRoot(ebx, Heap::kUndefinedValueRootIndex, &gotta_call_runtime);

  // Is optimized code available in the feedback vector?
  Register entry = ecx;
  __ mov(entry,
         FieldOperand(ebx, FeedbackVector::kOptimizedCodeIndex * kPointerSize +
                               FeedbackVector::kHeaderSize));
  __ mov(entry, FieldOperand(entry, WeakCell::kValueOffset));
  __ JumpIfSmi(entry, &try_shared);

  // Found code, check if it is marked for deopt, if so call into runtime to
  // clear the optimized code slot.
  __ test(FieldOperand(entry, Code::kKindSpecificFlags1Offset),
          Immediate(1 << Code::kMarkedForDeoptimizationBit));
  __ j(not_zero, &gotta_call_runtime);

  // Code is good, get it into the closure and tail call.
  __ push(argument_count);
  __ push(new_target);
  ReplaceClosureEntryWithOptimizedCode(masm, entry, closure, edx, eax, ebx);
  __ pop(new_target);
  __ pop(argument_count);
  __ jmp(entry);

  // We found no optimized code.
  __ bind(&try_shared);
  __ mov(entry, FieldOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Is the shared function marked for tier up?
  __ test_b(FieldOperand(entry, SharedFunctionInfo::kMarkedForTierUpByteOffset),
            Immediate(1 << SharedFunctionInfo::kMarkedForTierUpBitWithinByte));
  __ j(not_zero, &gotta_call_runtime);

  // If SFI points to anything other than CompileLazy, install that.
  __ mov(entry, FieldOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ Move(ebx, masm->CodeObject());
  __ cmp(entry, ebx);
  __ j(equal, &gotta_call_runtime);

  // Install the SFI's code entry.
  __ lea(entry, FieldOperand(entry, Code::kHeaderSize));
  __ mov(FieldOperand(closure, JSFunction::kCodeEntryOffset), entry);
  __ RecordWriteCodeEntryField(closure, entry, ebx);
  __ jmp(entry);

  __ bind(&gotta_call_runtime);

  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

void Builtins::Generate_CompileOptimized(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm,
                                 Runtime::kCompileOptimized_NotConcurrent);
}

void Builtins::Generate_CompileOptimizedConcurrent(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileOptimized_Concurrent);
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
  // On failure, tail call back to regular js.
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
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

#define DEFINE_CODE_AGE_BUILTIN_GENERATOR(C)                              \
  void Builtins::Generate_Make##C##CodeYoungAgain(MacroAssembler* masm) { \
    GenerateMakeCodeYoungAgainCommon(masm);                               \
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
    __ CallRuntime(Runtime::kNotifyStubFailure, save_doubles);
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
    __ CallRuntime(Runtime::kNotifyDeoptimized);

    // Tear down internal frame.
  }

  // Get the full codegen state from the stack and untag it.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));
  __ SmiUntag(ecx);

  // Switch on the state.
  Label not_no_registers, not_tos_eax;
  __ cmp(ecx, static_cast<int>(Deoptimizer::BailoutState::NO_REGISTERS));
  __ j(not_equal, &not_no_registers, Label::kNear);
  __ ret(1 * kPointerSize);  // Remove state.

  __ bind(&not_no_registers);
  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), eax.code());
  __ mov(eax, Operand(esp, 2 * kPointerSize));
  __ cmp(ecx, static_cast<int>(Deoptimizer::BailoutState::TOS_REGISTER));
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
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax     : argc
  //  -- esp[0]  : return address
  //  -- esp[4]  : argArray
  //  -- esp[8]  : thisArg
  //  -- esp[12] : receiver
  // -----------------------------------

  // 1. Load receiver into edi, argArray into eax (if present), remove all
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
    __ Move(eax, ebx);
  }

  // ----------- S t a t e -------------
  //  -- eax    : argArray
  //  -- edi    : receiver
  //  -- esp[0] : return address
  //  -- esp[4] : thisArg
  // -----------------------------------

  // 2. Make sure the receiver is actually callable.
  Label receiver_not_callable;
  __ JumpIfSmi(edi, &receiver_not_callable, Label::kNear);
  __ mov(ecx, FieldOperand(edi, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
            Immediate(1 << Map::kIsCallable));
  __ j(zero, &receiver_not_callable, Label::kNear);

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(eax, Heap::kNullValueRootIndex, &no_arguments, Label::kNear);
  __ JumpIfRoot(eax, Heap::kUndefinedValueRootIndex, &no_arguments,
                Label::kNear);

  // 4a. Apply the receiver to the given argArray (passing undefined for
  // new.target).
  __ LoadRoot(edx, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ Set(eax, 0);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }

  // 4c. The receiver is not callable, throw an appropriate TypeError.
  __ bind(&receiver_not_callable);
  {
    __ mov(Operand(esp, kPointerSize), edi);
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
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

  // 1. Load target into edi (if present), argumentsList into eax (if present),
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
    __ Move(eax, ebx);
  }

  // ----------- S t a t e -------------
  //  -- eax    : argumentsList
  //  -- edi    : target
  //  -- esp[0] : return address
  //  -- esp[4] : thisArgument
  // -----------------------------------

  // 2. Make sure the target is actually callable.
  Label target_not_callable;
  __ JumpIfSmi(edi, &target_not_callable, Label::kNear);
  __ mov(ecx, FieldOperand(edi, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
            Immediate(1 << Map::kIsCallable));
  __ j(zero, &target_not_callable, Label::kNear);

  // 3a. Apply the target to the given argumentsList (passing undefined for
  // new.target).
  __ LoadRoot(edx, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 3b. The target is not callable, throw an appropriate TypeError.
  __ bind(&target_not_callable);
  {
    __ mov(Operand(esp, kPointerSize), edi);
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
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

  // 1. Load target into edi (if present), argumentsList into eax (if present),
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
    __ Move(eax, ebx);
  }

  // ----------- S t a t e -------------
  //  -- eax    : argumentsList
  //  -- edx    : new.target
  //  -- edi    : target
  //  -- esp[0] : return address
  //  -- esp[4] : receiver (undefined)
  // -----------------------------------

  // 2. Make sure the target is actually a constructor.
  Label target_not_constructor;
  __ JumpIfSmi(edi, &target_not_constructor, Label::kNear);
  __ mov(ecx, FieldOperand(edi, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
            Immediate(1 << Map::kIsConstructor));
  __ j(zero, &target_not_constructor, Label::kNear);

  // 3. Make sure the target is actually a constructor.
  Label new_target_not_constructor;
  __ JumpIfSmi(edx, &new_target_not_constructor, Label::kNear);
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
            Immediate(1 << Map::kIsConstructor));
  __ j(zero, &new_target_not_constructor, Label::kNear);

  // 4a. Construct the target with the given new.target and argumentsList.
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The target is not a constructor, throw an appropriate TypeError.
  __ bind(&target_not_constructor);
  {
    __ mov(Operand(esp, kPointerSize), edi);
    __ TailCallRuntime(Runtime::kThrowNotConstructor);
  }

  // 4c. The new.target is not a constructor, throw an appropriate TypeError.
  __ bind(&new_target_not_constructor);
  {
    __ mov(Operand(esp, kPointerSize), edx);
    __ TailCallRuntime(Runtime::kThrowNotConstructor);
  }
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
void Builtins::Generate_NumberConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax                 : number of arguments
  //  -- edi                 : constructor function
  //  -- esi                 : context
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // 1. Load the first argument into ebx.
  Label no_arguments;
  {
    __ test(eax, eax);
    __ j(zero, &no_arguments, Label::kNear);
    __ mov(ebx, Operand(esp, eax, times_pointer_size, 0));
  }

  // 2a. Convert the first argument to a number.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(eax);
    __ EnterBuiltinFrame(esi, edi, eax);
    __ mov(eax, ebx);
    __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(esi, edi, ebx);  // Argc popped to ebx.
    __ SmiUntag(ebx);
  }

  {
    // Drop all arguments including the receiver.
    __ PopReturnAddressTo(ecx);
    __ lea(esp, Operand(esp, ebx, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(ecx);
    __ Ret();
  }

  // 2b. No arguments, return +0 (already in eax).
  __ bind(&no_arguments);
  __ ret(1 * kPointerSize);
}

// static
void Builtins::Generate_NumberConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax                 : number of arguments
  //  -- edi                 : constructor function
  //  -- edx                 : new target
  //  -- esi                 : context
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Store argc in r8.
  __ mov(ecx, eax);
  __ SmiTag(ecx);

  // 2. Load the first argument into ebx.
  {
    Label no_arguments, done;
    __ test(eax, eax);
    __ j(zero, &no_arguments, Label::kNear);
    __ mov(ebx, Operand(esp, eax, times_pointer_size, 0));
    __ jmp(&done, Label::kNear);
    __ bind(&no_arguments);
    __ Move(ebx, Smi::kZero);
    __ bind(&done);
  }

  // 3. Make sure ebx is a number.
  {
    Label done_convert;
    __ JumpIfSmi(ebx, &done_convert);
    __ CompareRoot(FieldOperand(ebx, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(equal, &done_convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterBuiltinFrame(esi, edi, ecx);
      __ Push(edx);
      __ Move(eax, ebx);
      __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
      __ Move(ebx, eax);
      __ Pop(edx);
      __ LeaveBuiltinFrame(esi, edi, ecx);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, done_alloc, new_object;
  __ cmp(edx, edi);
  __ j(not_equal, &new_object);

  // 5. Allocate a JSValue wrapper for the number.
  __ AllocateJSValue(eax, edi, ebx, esi, &done_alloc);
  __ jmp(&drop_frame_and_ret);

  __ bind(&done_alloc);
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));  // Restore esi.

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ EnterBuiltinFrame(esi, edi, ecx);
    __ Push(ebx);  // the first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(FieldOperand(eax, JSValue::kValueOffset));
    __ LeaveBuiltinFrame(esi, edi, ecx);
  }

  __ bind(&drop_frame_and_ret);
  {
    // Drop all arguments including the receiver.
    __ PopReturnAddressTo(esi);
    __ SmiUntag(ecx);
    __ lea(esp, Operand(esp, ecx, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(esi);
    __ Ret();
  }
}

// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax                 : number of arguments
  //  -- edi                 : constructor function
  //  -- esi                 : context
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // 1. Load the first argument into eax.
  Label no_arguments;
  {
    __ mov(ebx, eax);  // Store argc in ebx.
    __ test(eax, eax);
    __ j(zero, &no_arguments, Label::kNear);
    __ mov(eax, Operand(esp, eax, times_pointer_size, 0));
  }

  // 2a. At least one argument, return eax if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label drop_frame_and_ret, to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(eax, &to_string, Label::kNear);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, edx);
    __ j(above, &to_string, Label::kNear);
    __ j(equal, &symbol_descriptive_string, Label::kNear);
    __ jmp(&drop_frame_and_ret, Label::kNear);
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
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(ebx);
    __ EnterBuiltinFrame(esi, edi, ebx);
    __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(esi, edi, ebx);
    __ SmiUntag(ebx);
  }
  __ jmp(&drop_frame_and_ret, Label::kNear);

  // 3b. Convert symbol in eax to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ PopReturnAddressTo(ecx);
    __ lea(esp, Operand(esp, ebx, times_pointer_size, kPointerSize));
    __ Push(eax);
    __ PushReturnAddressFrom(ecx);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString);
  }

  __ bind(&drop_frame_and_ret);
  {
    // Drop all arguments including the receiver.
    __ PopReturnAddressTo(ecx);
    __ lea(esp, Operand(esp, ebx, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(ecx);
    __ Ret();
  }
}

// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax                 : number of arguments
  //  -- edi                 : constructor function
  //  -- edx                 : new target
  //  -- esi                 : context
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  __ mov(ebx, eax);

  // 2. Load the first argument into eax.
  {
    Label no_arguments, done;
    __ test(ebx, ebx);
    __ j(zero, &no_arguments, Label::kNear);
    __ mov(eax, Operand(esp, ebx, times_pointer_size, 0));
    __ jmp(&done, Label::kNear);
    __ bind(&no_arguments);
    __ LoadRoot(eax, Heap::kempty_stringRootIndex);
    __ bind(&done);
  }

  // 3. Make sure eax is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(eax, &convert, Label::kNear);
    __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, ecx);
    __ j(below, &done_convert);
    __ bind(&convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(ebx);
      __ EnterBuiltinFrame(esi, edi, ebx);
      __ Push(edx);
      __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
      __ Pop(edx);
      __ LeaveBuiltinFrame(esi, edi, ebx);
      __ SmiUntag(ebx);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, done_alloc, new_object;
  __ cmp(edx, edi);
  __ j(not_equal, &new_object);

  // 5. Allocate a JSValue wrapper for the string.
  // AllocateJSValue can't handle src == dst register. Reuse esi and restore it
  // as needed after the call.
  __ mov(esi, eax);
  __ AllocateJSValue(eax, edi, esi, ecx, &done_alloc);
  __ jmp(&drop_frame_and_ret);

  __ bind(&done_alloc);
  {
    // Restore eax to the first argument and esi to the context.
    __ mov(eax, esi);
    __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  }

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(ebx);
    __ EnterBuiltinFrame(esi, edi, ebx);
    __ Push(eax);  // the first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(FieldOperand(eax, JSValue::kValueOffset));
    __ LeaveBuiltinFrame(esi, edi, ebx);
    __ SmiUntag(ebx);
  }

  __ bind(&drop_frame_and_ret);
  {
    // Drop all arguments including the receiver.
    __ PopReturnAddressTo(ecx);
    __ lea(esp, Operand(esp, ebx, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(ecx);
    __ Ret();
  }
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
void Builtins::Generate_Apply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : argumentsList
  //  -- edi    : target
  //  -- edx    : new.target (checked to be constructor or undefined)
  //  -- esp[0] : return address.
  //  -- esp[4] : thisArgument
  // -----------------------------------

  // Create the list of arguments from the array-like argumentsList.
  {
    Label create_arguments, create_array, create_holey_array, create_runtime,
        done_create;
    __ JumpIfSmi(eax, &create_runtime);

    // Load the map of argumentsList into ecx.
    __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));

    // Load native context into ebx.
    __ mov(ebx, NativeContextOperand());

    // Check if argumentsList is an (unmodified) arguments object.
    __ cmp(ecx, ContextOperand(ebx, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    __ j(equal, &create_arguments);
    __ cmp(ecx, ContextOperand(ebx, Context::STRICT_ARGUMENTS_MAP_INDEX));
    __ j(equal, &create_arguments);

    // Check if argumentsList is a fast JSArray.
    __ CmpInstanceType(ecx, JS_ARRAY_TYPE);
    __ j(equal, &create_array);

    // Ask the runtime to create the list (actually a FixedArray).
    __ bind(&create_runtime);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(edi);
      __ Push(edx);
      __ Push(eax);
      __ CallRuntime(Runtime::kCreateListFromArrayLike);
      __ Pop(edx);
      __ Pop(edi);
      __ mov(ebx, FieldOperand(eax, FixedArray::kLengthOffset));
      __ SmiUntag(ebx);
    }
    __ jmp(&done_create);

    // Try to create the list from an arguments object.
    __ bind(&create_arguments);
    __ mov(ebx, FieldOperand(eax, JSArgumentsObject::kLengthOffset));
    __ mov(ecx, FieldOperand(eax, JSObject::kElementsOffset));
    __ cmp(ebx, FieldOperand(ecx, FixedArray::kLengthOffset));
    __ j(not_equal, &create_runtime);
    __ SmiUntag(ebx);
    __ mov(eax, ecx);
    __ jmp(&done_create);

    // For holey JSArrays we need to check that the array prototype chain
    // protector is intact and our prototype is the Array.prototype actually.
    __ bind(&create_holey_array);
    __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
    __ mov(ecx, FieldOperand(ecx, Map::kPrototypeOffset));
    __ cmp(ecx, ContextOperand(ebx, Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
    __ j(not_equal, &create_runtime);
    __ LoadRoot(ecx, Heap::kArrayProtectorRootIndex);
    __ cmp(FieldOperand(ecx, PropertyCell::kValueOffset),
           Immediate(Smi::FromInt(Isolate::kProtectorValid)));
    __ j(not_equal, &create_runtime);
    __ mov(ebx, FieldOperand(eax, JSArray::kLengthOffset));
    __ SmiUntag(ebx);
    __ mov(eax, FieldOperand(eax, JSArray::kElementsOffset));
    __ jmp(&done_create);

    // Try to create the list from a JSArray object.
    __ bind(&create_array);
    __ mov(ecx, FieldOperand(ecx, Map::kBitField2Offset));
    __ DecodeField<Map::ElementsKindBits>(ecx);
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    __ cmp(ecx, Immediate(FAST_HOLEY_SMI_ELEMENTS));
    __ j(equal, &create_holey_array, Label::kNear);
    __ cmp(ecx, Immediate(FAST_HOLEY_ELEMENTS));
    __ j(equal, &create_holey_array, Label::kNear);
    __ j(above, &create_runtime);
    __ mov(ebx, FieldOperand(eax, JSArray::kLengthOffset));
    __ SmiUntag(ebx);
    __ mov(eax, FieldOperand(eax, JSArray::kElementsOffset));

    __ bind(&done_create);
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    ExternalReference real_stack_limit =
        ExternalReference::address_of_real_stack_limit(masm->isolate());
    __ mov(ecx, Operand::StaticVariable(real_stack_limit));
    // Make ecx the space we have left. The stack might already be overflowed
    // here which will cause ecx to become negative.
    __ neg(ecx);
    __ add(ecx, esp);
    __ sar(ecx, kPointerSizeLog2);
    // Check if the arguments will overflow the stack.
    __ cmp(ecx, ebx);
    __ j(greater, &done, Label::kNear);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // ----------- S t a t e -------------
  //  -- edi    : target
  //  -- eax    : args (a FixedArray built from argumentsList)
  //  -- ebx    : len (number of elements to push from args)
  //  -- edx    : new.target (checked to be constructor or undefined)
  //  -- esp[0] : return address.
  //  -- esp[4] : thisArgument
  // -----------------------------------

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    __ movd(xmm0, edx);
    __ movd(xmm1, edi);
    __ PopReturnAddressTo(edx);
    __ Move(ecx, Immediate(0));
    Label done, push, loop;
    __ bind(&loop);
    __ cmp(ecx, ebx);
    __ j(equal, &done, Label::kNear);
    // Turn the hole into undefined as we go.
    __ mov(edi,
           FieldOperand(eax, ecx, times_pointer_size, FixedArray::kHeaderSize));
    __ CompareRoot(edi, Heap::kTheHoleValueRootIndex);
    __ j(not_equal, &push, Label::kNear);
    __ LoadRoot(edi, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ Push(edi);
    __ inc(ecx);
    __ jmp(&loop);
    __ bind(&done);
    __ PushReturnAddressFrom(edx);
    __ movd(edi, xmm1);
    __ movd(edx, xmm0);
    __ Move(eax, ebx);
  }

  // Dispatch to Call or Construct depending on whether new.target is undefined.
  {
    __ CompareRoot(edx, Heap::kUndefinedValueRootIndex);
    __ j(equal, masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_ForwardVarargs(MacroAssembler* masm,
                                       Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the target to call (can be any Object)
  //  -- edx : the new target (for [[Construct]] calls)
  //  -- ecx : start index (to support rest parameters)
  // -----------------------------------

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
    __ mov(edx,
           FieldOperand(edx, SharedFunctionInfo::kFormalParameterCountOffset));
    __ mov(ebx, ebp);
  }
  __ jmp(&arguments_done, Label::kNear);
  __ bind(&arguments_adaptor);
  {
    // Just load the length from the ArgumentsAdaptorFrame.
    __ mov(edx, Operand(ebx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  }
  __ bind(&arguments_done);

  Label stack_done;
  __ SmiUntag(edx);
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

namespace {

// Drops top JavaScript frame and an arguments adaptor frame below it (if
// present) preserving all the arguments prepared for current call.
// Does nothing if debugger is currently active.
// ES6 14.6.3. PrepareForTailCall
//
// Stack structure for the function g() tail calling f():
//
// ------- Caller frame: -------
// |  ...
// |  g()'s arg M
// |  ...
// |  g()'s arg 1
// |  g()'s receiver arg
// |  g()'s caller pc
// ------- g()'s frame: -------
// |  g()'s caller fp      <- fp
// |  g()'s context
// |  function pointer: g
// |  -------------------------
// |  ...
// |  ...
// |  f()'s arg N
// |  ...
// |  f()'s arg 1
// |  f()'s receiver arg
// |  f()'s caller pc      <- sp
// ----------------------
//
void PrepareForTailCall(MacroAssembler* masm, Register args_reg,
                        Register scratch1, Register scratch2,
                        Register scratch3) {
  DCHECK(!AreAliased(args_reg, scratch1, scratch2, scratch3));
  Comment cmnt(masm, "[ PrepareForTailCall");

  // Prepare for tail call only if ES2015 tail call elimination is enabled.
  Label done;
  ExternalReference is_tail_call_elimination_enabled =
      ExternalReference::is_tail_call_elimination_enabled_address(
          masm->isolate());
  __ movzx_b(scratch1,
             Operand::StaticVariable(is_tail_call_elimination_enabled));
  __ cmp(scratch1, Immediate(0));
  __ j(equal, &done, Label::kNear);

  // Drop possible interpreter handler/stub frame.
  {
    Label no_interpreter_frame;
    __ cmp(Operand(ebp, CommonFrameConstants::kContextOrFrameTypeOffset),
           Immediate(StackFrame::TypeToMarker(StackFrame::STUB)));
    __ j(not_equal, &no_interpreter_frame, Label::kNear);
    __ mov(ebp, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
    __ bind(&no_interpreter_frame);
  }

  // Check if next frame is an arguments adaptor frame.
  Register caller_args_count_reg = scratch1;
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ mov(scratch2, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ cmp(Operand(scratch2, CommonFrameConstants::kContextOrFrameTypeOffset),
         Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &no_arguments_adaptor, Label::kNear);

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ mov(ebp, scratch2);
  __ mov(caller_args_count_reg,
         Operand(ebp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);
  __ jmp(&formal_parameter_count_loaded, Label::kNear);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ mov(scratch1, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(scratch1,
         FieldOperand(scratch1, JSFunction::kSharedFunctionInfoOffset));
  __ mov(
      caller_args_count_reg,
      FieldOperand(scratch1, SharedFunctionInfo::kFormalParameterCountOffset));
  __ SmiUntag(caller_args_count_reg);

  __ bind(&formal_parameter_count_loaded);

  ParameterCount callee_args_count(args_reg);
  __ PrepareForTailCall(callee_args_count, caller_args_count_reg, scratch2,
                        scratch3, ReturnAddressState::kOnStack, 0);
  __ bind(&done);
}
}  // namespace

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode,
                                     TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(edi);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ test_b(FieldOperand(edx, SharedFunctionInfo::kFunctionKindByteOffset),
            Immediate(SharedFunctionInfo::kClassConstructorBitsWithinByte));
  __ j(not_zero, &class_constructor);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ test_b(FieldOperand(edx, SharedFunctionInfo::kNativeByteOffset),
            Immediate((1 << SharedFunctionInfo::kNativeBitWithinByte) |
                      (1 << SharedFunctionInfo::kStrictModeBitWithinByte)));
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
        __ Call(masm->isolate()->builtins()->ToObject(),
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

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, eax, ebx, ecx, edx);
    // Reload shared function info.
    __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  }

  __ mov(ebx,
         FieldOperand(edx, SharedFunctionInfo::kFormalParameterCountOffset));
  __ SmiUntag(ebx);
  ParameterCount actual(eax);
  ParameterCount expected(ebx);
  __ InvokeFunctionCode(edi, no_reg, expected, actual, JUMP_FUNCTION,
                        CheckDebugStepCallWrapper());
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
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm,
                                              TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(edi);

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, eax, ebx, ecx, edx);
  }

  // Patch the receiver to [[BoundThis]].
  __ mov(ebx, FieldOperand(edi, JSBoundFunction::kBoundThisOffset));
  __ mov(Operand(esp, eax, times_pointer_size, kPointerSize), ebx);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ mov(edi, FieldOperand(edi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ mov(ecx, Operand::StaticVariable(ExternalReference(
                  Builtins::kCall_ReceiverIsAny, masm->isolate())));
  __ lea(ecx, FieldOperand(ecx, Code::kHeaderSize));
  __ jmp(ecx);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode,
                             TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(edi, &non_callable);
  __ bind(&non_smi);
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(equal, masm->isolate()->builtins()->CallFunction(mode, tail_call_mode),
       RelocInfo::CODE_TARGET);
  __ CmpInstanceType(ecx, JS_BOUND_FUNCTION_TYPE);
  __ j(equal, masm->isolate()->builtins()->CallBoundFunction(tail_call_mode),
       RelocInfo::CODE_TARGET);

  // Check if target has a [[Call]] internal method.
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
            Immediate(1 << Map::kIsCallable));
  __ j(zero, &non_callable);

  __ CmpInstanceType(ecx, JS_PROXY_TYPE);
  __ j(not_equal, &non_function);

  // 0. Prepare for tail call if necessary.
  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, eax, ebx, ecx, edx);
  }

  // 1. Runtime fallback for Proxy [[Call]].
  __ PopReturnAddressTo(ecx);
  __ Push(edi);
  __ PushReturnAddressFrom(ecx);
  // Increase the arguments size to include the pushed function and the
  // existing receiver on the stack.
  __ add(eax, Immediate(2));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyCall, masm->isolate()));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ mov(Operand(esp, eax, times_pointer_size, kPointerSize), edi);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadGlobalFunction(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, edi);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(edi);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

static void CheckSpreadAndPushToStack(MacroAssembler* masm) {
  // Free up some registers.
  __ movd(xmm0, edx);
  __ movd(xmm1, edi);

  Register argc = eax;

  Register scratch = ecx;
  Register scratch2 = edi;

  Register spread = ebx;
  Register spread_map = edx;

  Register spread_len = edx;

  Label runtime_call, push_args;
  __ mov(spread, Operand(esp, kPointerSize));
  __ JumpIfSmi(spread, &runtime_call);
  __ mov(spread_map, FieldOperand(spread, HeapObject::kMapOffset));

  // Check that the spread is an array.
  __ CmpInstanceType(spread_map, JS_ARRAY_TYPE);
  __ j(not_equal, &runtime_call);

  // Check that we have the original ArrayPrototype.
  __ mov(scratch, FieldOperand(spread_map, Map::kPrototypeOffset));
  __ mov(scratch2, NativeContextOperand());
  __ cmp(scratch,
         ContextOperand(scratch2, Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
  __ j(not_equal, &runtime_call);

  // Check that the ArrayPrototype hasn't been modified in a way that would
  // affect iteration.
  __ LoadRoot(scratch, Heap::kArrayIteratorProtectorRootIndex);
  __ cmp(FieldOperand(scratch, PropertyCell::kValueOffset),
         Immediate(Smi::FromInt(Isolate::kProtectorValid)));
  __ j(not_equal, &runtime_call);

  // Check that the map of the initial array iterator hasn't changed.
  __ mov(scratch2, NativeContextOperand());
  __ mov(scratch,
         ContextOperand(scratch2,
                        Context::INITIAL_ARRAY_ITERATOR_PROTOTYPE_INDEX));
  __ mov(scratch, FieldOperand(scratch, HeapObject::kMapOffset));
  __ cmp(scratch,
         ContextOperand(scratch2,
                        Context::INITIAL_ARRAY_ITERATOR_PROTOTYPE_MAP_INDEX));
  __ j(not_equal, &runtime_call);

  // For FastPacked kinds, iteration will have the same effect as simply
  // accessing each property in order.
  Label no_protector_check;
  __ mov(scratch, FieldOperand(spread_map, Map::kBitField2Offset));
  __ DecodeField<Map::ElementsKindBits>(scratch);
  __ cmp(scratch, Immediate(FAST_HOLEY_ELEMENTS));
  __ j(above, &runtime_call);
  // For non-FastHoley kinds, we can skip the protector check.
  __ cmp(scratch, Immediate(FAST_SMI_ELEMENTS));
  __ j(equal, &no_protector_check);
  __ cmp(scratch, Immediate(FAST_ELEMENTS));
  __ j(equal, &no_protector_check);
  // Check the ArrayProtector cell.
  __ LoadRoot(scratch, Heap::kArrayProtectorRootIndex);
  __ cmp(FieldOperand(scratch, PropertyCell::kValueOffset),
         Immediate(Smi::FromInt(Isolate::kProtectorValid)));
  __ j(not_equal, &runtime_call);

  __ bind(&no_protector_check);
  // Load the FixedArray backing store, but use the length from the array.
  __ mov(spread_len, FieldOperand(spread, JSArray::kLengthOffset));
  __ SmiUntag(spread_len);
  __ mov(spread, FieldOperand(spread, JSArray::kElementsOffset));
  __ jmp(&push_args);

  __ bind(&runtime_call);
  {
    // Call the builtin for the result of the spread.
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Need to save these on the stack.
    __ movd(edi, xmm1);
    __ movd(edx, xmm0);
    __ Push(edi);
    __ Push(edx);
    __ SmiTag(argc);
    __ Push(argc);
    __ Push(spread);
    __ CallRuntime(Runtime::kSpreadIterableFixed);
    __ mov(spread, eax);
    __ Pop(argc);
    __ SmiUntag(argc);
    __ Pop(edx);
    __ Pop(edi);
    // Free up some registers.
    __ movd(xmm0, edx);
    __ movd(xmm1, edi);
  }

  {
    // Calculate the new nargs including the result of the spread.
    __ mov(spread_len, FieldOperand(spread, FixedArray::kLengthOffset));
    __ SmiUntag(spread_len);

    __ bind(&push_args);
    // argc += spread_len - 1. Subtract 1 for the spread itself.
    __ lea(argc, Operand(argc, spread_len, times_1, -1));
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(scratch, Heap::kRealStackLimitRootIndex);
    // Make scratch the space we have left. The stack might already be
    // overflowed here which will cause scratch to become negative.
    __ neg(scratch);
    __ add(scratch, esp);
    __ sar(scratch, kPointerSizeLog2);
    // Check if the arguments will overflow the stack.
    __ cmp(scratch, spread_len);
    __ j(greater, &done, Label::kNear);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // Put the evaluated spread onto the stack as additional arguments.
  {
    Register return_address = edi;
    // Pop the return address and spread argument.
    __ PopReturnAddressTo(return_address);
    __ Pop(scratch);

    Register scratch2 = esi;
    __ movd(xmm2, esi);

    __ mov(scratch, Immediate(0));
    Label done, push, loop;
    __ bind(&loop);
    __ cmp(scratch, spread_len);
    __ j(equal, &done, Label::kNear);
    __ mov(scratch2, FieldOperand(spread, scratch, times_pointer_size,
                                  FixedArray::kHeaderSize));
    __ CompareRoot(scratch2, Heap::kTheHoleValueRootIndex);
    __ j(not_equal, &push, Label::kNear);
    __ LoadRoot(scratch2, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ Push(scratch2);
    __ inc(scratch);
    __ jmp(&loop);
    __ bind(&done);
    __ PushReturnAddressFrom(return_address);
    __ movd(esi, xmm2);
    __ movd(edi, xmm1);
    __ movd(edx, xmm0);
  }
}

// static
void Builtins::Generate_CallWithSpread(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the target to call (can be any Object)
  // -----------------------------------

  // CheckSpreadAndPushToStack will push edx to save it.
  __ LoadRoot(edx, Heap::kUndefinedValueRootIndex);
  CheckSpreadAndPushToStack(masm);
  __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                            TailCallMode::kDisallow),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the new target (checked to be a constructor)
  //  -- edi : the constructor to call (checked to be a JSFunction)
  // -----------------------------------
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
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the new target (checked to be a constructor)
  //  -- edi : the constructor to call (checked to be a JSBoundFunction)
  // -----------------------------------
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
  __ mov(ecx, Operand::StaticVariable(
                  ExternalReference(Builtins::kConstruct, masm->isolate())));
  __ lea(ecx, FieldOperand(ecx, Code::kHeaderSize));
  __ jmp(ecx);
}

// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the constructor to call (checked to be a JSProxy)
  //  -- edx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Call into the Runtime for Proxy [[Construct]].
  __ PopReturnAddressTo(ecx);
  __ Push(edi);
  __ Push(edx);
  __ PushReturnAddressFrom(ecx);
  // Include the pushed new_target, constructor and the receiver.
  __ add(eax, Immediate(3));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyConstruct, masm->isolate()));
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
  Label non_constructor;
  __ JumpIfSmi(edi, &non_constructor, Label::kNear);

  // Dispatch based on instance type.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(equal, masm->isolate()->builtins()->ConstructFunction(),
       RelocInfo::CODE_TARGET);

  // Check if target has a [[Construct]] internal method.
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
            Immediate(1 << Map::kIsConstructor));
  __ j(zero, &non_constructor, Label::kNear);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ CmpInstanceType(ecx, JS_BOUND_FUNCTION_TYPE);
  __ j(equal, masm->isolate()->builtins()->ConstructBoundFunction(),
       RelocInfo::CODE_TARGET);

  // Only dispatch to proxies after checking whether they are constructors.
  __ CmpInstanceType(ecx, JS_PROXY_TYPE);
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
  __ Jump(masm->isolate()->builtins()->ConstructedNonConstructable(),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructWithSpread(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- edi : the constructor to call (can be any Object)
  // -----------------------------------

  CheckSpreadAndPushToStack(masm);
  __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_AllocateInNewSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edx    : requested object size (untagged)
  //  -- esp[0] : return address
  // -----------------------------------
  __ SmiTag(edx);
  __ PopReturnAddressTo(ecx);
  __ Push(edx);
  __ PushReturnAddressFrom(ecx);
  __ Move(esi, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edx    : requested object size (untagged)
  //  -- esp[0] : return address
  // -----------------------------------
  __ SmiTag(edx);
  __ PopReturnAddressTo(ecx);
  __ Push(edx);
  __ Push(Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ PushReturnAddressFrom(ecx);
  __ Move(esi, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

// static
void Builtins::Generate_Abort(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edx    : message_id as Smi
  //  -- esp[0] : return address
  // -----------------------------------
  __ PopReturnAddressTo(ecx);
  __ Push(edx);
  __ PushReturnAddressFrom(ecx);
  __ Move(esi, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbort);
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
  __ cmp(eax, ebx);
  __ j(less, &too_few);
  __ cmp(ebx, SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  __ j(equal, &dont_adapt_arguments);

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
  __ mov(ecx, FieldOperand(edi, JSFunction::kCodeEntryOffset));
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
  __ mov(ecx, FieldOperand(edi, JSFunction::kCodeEntryOffset));
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
                               DeoptimizationInputData::kOsrPcOffsetIndex) -
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
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    constexpr Register gp_regs[]{eax, ebx, ecx, edx, esi};
    constexpr XMMRegister xmm_regs[]{xmm1, xmm2, xmm3, xmm4, xmm5, xmm6};

    for (auto reg : gp_regs) {
      __ Push(reg);
    }
    __ sub(esp, Immediate(16 * arraysize(xmm_regs)));
    for (int i = 0, e = arraysize(xmm_regs); i < e; ++i) {
      __ movdqu(Operand(esp, 16 * i), xmm_regs[i]);
    }

    // Initialize rsi register with kZero, CEntryStub will use it to set the
    // current context on the isolate.
    __ Move(esi, Smi::kZero);
    __ CallRuntime(Runtime::kWasmCompileLazy);
    // Store returned instruction start in edi.
    __ lea(edi, FieldOperand(eax, Code::kHeaderSize));

    // Restore registers.
    for (int i = arraysize(xmm_regs) - 1; i >= 0; --i) {
      __ movdqu(xmm_regs[i], Operand(esp, 16 * i));
    }
    __ add(esp, Immediate(16 * arraysize(xmm_regs)));
    for (int i = arraysize(gp_regs) - 1; i >= 0; --i) {
      __ Pop(gp_regs[i]);
    }
  }
  // Now jump to the instructions of the returned code object.
  __ jmp(edi);
}

#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32

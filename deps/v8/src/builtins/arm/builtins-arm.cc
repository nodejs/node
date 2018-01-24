// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include "src/assembler-inl.h"
#include "src/code-stubs.h"
#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/objects-inl.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
#if defined(__thumb__)
  // Thumb mode builtin.
  DCHECK_EQ(1, reinterpret_cast<intptr_t>(
                   ExternalReference(address, masm->isolate()).address()) &
                   1);
#endif
  __ mov(r5, Operand(ExternalReference(address, masm->isolate())));
  if (exit_frame_type == BUILTIN_EXIT) {
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK(exit_frame_type == EXIT);
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithExitFrame),
            RelocInfo::CODE_TARGET);
  }
}

namespace {

void AdaptorWithExitFrameType(MacroAssembler* masm,
                              Builtins::ExitFrameType exit_frame_type) {
  // ----------- S t a t e -------------
  //  -- r0                 : number of arguments excluding receiver
  //  -- r1                 : target
  //  -- r3                 : new.target
  //  -- r5                 : entry point
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------
  __ AssertFunction(r1);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // CEntryStub expects r0 to contain the number of arguments including the
  // receiver and the extra arguments.
  __ add(r0, r0, Operand(BuiltinExitFrameConstants::kNumExtraArgsWithReceiver));

  // Insert extra arguments.
  __ PushRoot(Heap::kTheHoleValueRootIndex);  // Padding.
  __ SmiTag(r0);
  __ Push(r0, r1, r3);
  __ SmiUntag(r0);

  // Jump to the C entry runtime stub directly here instead of using
  // JumpToExternalReference. We have already loaded entry point to r5
  // in Generate_adaptor.
  __ mov(r1, r5);
  CEntryStub stub(masm->isolate(), 1, kDontSaveFPRegs, kArgvOnStack,
                  exit_frame_type == Builtins::BUILTIN_EXIT);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}
}  // namespace

void Builtins::Generate_AdaptorWithExitFrame(MacroAssembler* masm) {
  AdaptorWithExitFrameType(masm, EXIT);
}

void Builtins::Generate_AdaptorWithBuiltinExitFrame(MacroAssembler* masm) {
  AdaptorWithExitFrameType(masm, BUILTIN_EXIT);
}

// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the InternalArray function from the current native context.
  __ LoadNativeContextSlot(Context::INTERNAL_ARRAY_FUNCTION_INDEX, result);
}

// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the Array function from the current native context.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, result);
}

void Builtins::Generate_InternalArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, r1);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(r2);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction);
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // tail call a stub
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

void Builtins::Generate_ArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, r1);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(r2);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction);
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
  }

  __ mov(r3, r1);
  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r2, FieldMemOperand(r2, SharedFunctionInfo::kCodeOffset));
  __ add(r2, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(r2);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- r0 : argument count (preserved for callee)
  //  -- r1 : target function (preserved for callee)
  //  -- r3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Push the number of arguments to the callee.
    __ SmiTag(r0);
    __ push(r0);
    // Push a copy of the target function and the new target.
    __ push(r1);
    __ push(r3);
    // Push function as parameter to the runtime call.
    __ Push(r1);

    __ CallRuntime(function_id, 1);
    __ mov(r2, r0);

    // Restore target function and new target.
    __ pop(r3);
    __ pop(r1);
    __ pop(r0);
    __ SmiUntag(r0, r0);
  }
  __ add(r2, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(r2);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- r1     : constructor function
  //  -- r3     : new target
  //  -- cp     : context
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  Register scratch = r2;

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(r0);
    __ Push(cp, r0);
    __ SmiUntag(r0);

    // The receiver for the builtin/api call.
    __ PushRoot(Heap::kTheHoleValueRootIndex);

    // Set up pointer to last argument.
    __ add(r4, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(r5, r0);
    // ----------- S t a t e -------------
    //  --                 r0: number of arguments (untagged)
    //  --                 r1: constructor function
    //  --                 r3: new target
    //  --                 r4: pointer to last argument
    //  --                 r5: counter
    //  -- sp[0*kPointerSize]: the hole (receiver)
    //  -- sp[1*kPointerSize]: number of arguments (tagged)
    //  -- sp[2*kPointerSize]: context
    // -----------------------------------
    __ b(&entry);
    __ bind(&loop);
    __ ldr(scratch, MemOperand(r4, r5, LSL, kPointerSizeLog2));
    __ push(scratch);
    __ bind(&entry);
    __ sub(r5, r5, Operand(1), SetCC);
    __ b(ge, &loop);

    // Call the function.
    // r0: number of arguments (untagged)
    // r1: constructor function
    // r3: new target
    ParameterCount actual(r0);
    __ InvokeFunction(r1, r3, actual, CALL_FUNCTION);

    // Restore context from the frame.
    __ ldr(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ ldr(scratch, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ add(sp, sp, Operand(scratch, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ add(sp, sp, Operand(kPointerSize));
  __ Jump(lr);
}

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Generate_JSConstructStubGeneric(MacroAssembler* masm,
                                     bool restrict_constructor_return) {
  // ----------- S t a t e -------------
  //  --      r0: number of arguments (untagged)
  //  --      r1: constructor function
  //  --      r3: new target
  //  --      cp: context
  //  --      lr: return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ SmiTag(r0);
    __ Push(cp, r0, r1, r3);

    // ----------- S t a t e -------------
    //  --        sp[0*kPointerSize]: new target
    //  -- r1 and sp[1*kPointerSize]: constructor function
    //  --        sp[2*kPointerSize]: number of arguments (tagged)
    //  --        sp[3*kPointerSize]: context
    // -----------------------------------

    __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r4, FieldMemOperand(r4, SharedFunctionInfo::kCompilerHintsOffset));
    __ tst(r4, Operand(SharedFunctionInfo::kDerivedConstructorMask));
    __ b(ne, &not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        r4, r5);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ b(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(r0, Heap::kTheHoleValueRootIndex);

    // ----------- S t a t e -------------
    //  --                          r0: receiver
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
    __ Pop(r3);
    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(r0, r0);

    // ----------- S t a t e -------------
    //  --                 r3: new target
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: implicit receiver
    //  -- sp[2*kPointerSize]: constructor function
    //  -- sp[3*kPointerSize]: number of arguments (tagged)
    //  -- sp[4*kPointerSize]: context
    // -----------------------------------

    // Restore constructor function and argument count.
    __ ldr(r1, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ ldr(r0, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    __ SmiUntag(r0);

    // Set up pointer to last argument.
    __ add(r4, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(r5, r0);
    // ----------- S t a t e -------------
    //  --                        r0: number of arguments (untagged)
    //  --                        r3: new target
    //  --                        r4: pointer to last argument
    //  --                        r5: counter
    //  --        sp[0*kPointerSize]: implicit receiver
    //  --        sp[1*kPointerSize]: implicit receiver
    //  -- r1 and sp[2*kPointerSize]: constructor function
    //  --        sp[3*kPointerSize]: number of arguments (tagged)
    //  --        sp[4*kPointerSize]: context
    // -----------------------------------
    __ b(&entry);

    __ bind(&loop);
    __ ldr(r6, MemOperand(r4, r5, LSL, kPointerSizeLog2));
    __ push(r6);
    __ bind(&entry);
    __ sub(r5, r5, Operand(1), SetCC);
    __ b(ge, &loop);

    // Call the function.
    ParameterCount actual(r0);
    __ InvokeFunction(r1, r3, actual, CALL_FUNCTION);

    // ----------- S t a t e -------------
    //  --                 r0: constructor result
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: constructor function
    //  -- sp[2*kPointerSize]: number of arguments
    //  -- sp[3*kPointerSize]: context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore the context from the frame.
    __ ldr(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, other_result, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(r0, Heap::kUndefinedValueRootIndex, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(r0, &other_result);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(r0, r4, r5, FIRST_JS_RECEIVER_TYPE);
    __ b(ge, &leave_frame);

    // The result is now neither undefined nor an object.
    __ bind(&other_result);
    __ ldr(r4, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ ldr(r4, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r4, FieldMemOperand(r4, SharedFunctionInfo::kCompilerHintsOffset));
    __ tst(r4, Operand(SharedFunctionInfo::kClassConstructorMask));

    if (restrict_constructor_return) {
      // Throw if constructor function is a class constructor
      __ b(eq, &use_receiver);
    } else {
      __ b(ne, &use_receiver);
      __ CallRuntime(
          Runtime::kIncrementUseCounterConstructorReturnNonUndefinedPrimitive);
      __ b(&use_receiver);
    }

    __ bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ ldr(r0, MemOperand(sp, 0 * kPointerSize));
    __ JumpIfRoot(r0, Heap::kTheHoleValueRootIndex, &do_throw);

    __ bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ ldr(r1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ add(sp, sp, Operand(r1, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ add(sp, sp, Operand(kPointerSize));
  __ Jump(lr);
}
}  // namespace

void Builtins::Generate_JSConstructStubGenericRestrictedReturn(
    MacroAssembler* masm) {
  Generate_JSConstructStubGeneric(masm, true);
}
void Builtins::Generate_JSConstructStubGenericUnrestrictedReturn(
    MacroAssembler* masm) {
  Generate_JSConstructStubGeneric(masm, false);
}
void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}
void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the value to pass to the generator
  //  -- r1 : the JSGeneratorObject to resume
  //  -- lr : return address
  // -----------------------------------
  __ AssertGeneratorObject(r1);

  // Store input value into generator object.
  __ str(r0, FieldMemOperand(r1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(r1, JSGeneratorObject::kInputOrDebugPosOffset, r0, r3,
                      kLRHasNotBeenSaved, kDontSaveFPRegs);

  // Load suspended function and context.
  __ ldr(r4, FieldMemOperand(r1, JSGeneratorObject::kFunctionOffset));
  __ ldr(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  Register scratch = r5;

  // Flood function if we are stepping.
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ mov(scratch, Operand(debug_hook));
  __ ldrsb(scratch, MemOperand(scratch));
  __ cmp(scratch, Operand(0));
  __ b(ne, &prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended
  // generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ mov(scratch, Operand(debug_suspended_generator));
  __ ldr(scratch, MemOperand(scratch));
  __ cmp(scratch, Operand(r1));
  __ b(eq, &prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ CompareRoot(sp, Heap::kRealStackLimitRootIndex);
  __ b(lo, &stack_overflow);

  // Push receiver.
  __ ldr(scratch, FieldMemOperand(r1, JSGeneratorObject::kReceiverOffset));
  __ Push(scratch);

  // ----------- S t a t e -------------
  //  -- r1    : the JSGeneratorObject to resume
  //  -- r4    : generator function
  //  -- cp    : generator context
  //  -- lr    : return address
  //  -- sp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ ldr(r3, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r3,
         FieldMemOperand(r3, SharedFunctionInfo::kFormalParameterCountOffset));
  {
    Label done_loop, loop;
    __ bind(&loop);
    __ sub(r3, r3, Operand(1), SetCC);
    __ b(mi, &done_loop);
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    __ b(&loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ ldr(r3, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r3, FieldMemOperand(r3, SharedFunctionInfo::kFunctionDataOffset));
    __ CompareObjectType(r3, r3, r3, BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ ldr(r0, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r0, FieldMemOperand(
                   r0, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Move(r3, r1);
    __ Move(r1, r4);
    __ ldr(scratch, FieldMemOperand(r1, JSFunction::kCodeOffset));
    __ add(scratch, scratch, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(scratch);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r1, r4);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(r1);
    __ ldr(r4, FieldMemOperand(r1, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r1);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(r1);
    __ ldr(r4, FieldMemOperand(r1, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bkpt(0);  // This should be unreachable.
  }
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ push(r1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

static void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                        Register scratch,
                                        Label* stack_overflow) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(scratch, Heap::kRealStackLimitRootIndex);
  // Make scratch the space we have left. The stack might already be overflowed
  // here which will cause scratch to become negative.
  __ sub(scratch, sp, scratch);
  // Check if the arguments will overflow the stack.
  __ cmp(scratch, Operand(num_args, LSL, kPointerSizeLog2));
  __ b(le, stack_overflow);  // Signed comparison.
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r0: new.target
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  // r5-r6, r8 and cp may be clobbered
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(IsolateAddressId::kContextAddress,
                                      masm->isolate());
    __ mov(cp, Operand(context_address));
    __ ldr(cp, MemOperand(cp));

    __ InitializeRootRegister();

    // Push the function and the receiver onto the stack.
    __ Push(r1, r2);

    // Check if we have enough stack space to push all arguments.
    // Clobbers r2.
    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, r3, r2, &stack_overflow);
    __ b(&enough_stack_space);
    __ bind(&stack_overflow);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ bkpt(0);

    __ bind(&enough_stack_space);

    // Remember new.target.
    __ mov(r5, r0);

    // Copy arguments to the stack in a loop.
    // r1: function
    // r3: argc
    // r4: argv, i.e. points to first arg
    Label loop, entry;
    __ add(r2, r4, Operand(r3, LSL, kPointerSizeLog2));
    // r2 points past last arg.
    __ b(&entry);
    __ bind(&loop);
    __ ldr(r0, MemOperand(r4, kPointerSize, PostIndex));  // read next parameter
    __ ldr(r0, MemOperand(r0));                           // dereference handle
    __ push(r0);                                          // push parameter
    __ bind(&entry);
    __ cmp(r4, r2);
    __ b(ne, &loop);

    // Setup new.target and argc.
    __ mov(r0, Operand(r3));
    __ mov(r3, Operand(r5));

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
    __ mov(r5, Operand(r4));
    __ mov(r6, Operand(r4));
    __ mov(r8, Operand(r4));
    if (kR9Available == 1) {
      __ mov(r9, Operand(r4));
    }

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the JS frame and remove the parameters (except function), and
    // return.
    // Respect ABI stack constraint.
  }
  __ Jump(lr);

  // r0: result
}

void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}

void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}

static void ReplaceClosureCodeWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {
  // Store code entry in the closure.
  __ str(optimized_code, FieldMemOperand(closure, JSFunction::kCodeOffset));
  __ mov(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_count = scratch;

  // Get the arguments + receiver count.
  __ ldr(args_count,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ ldr(args_count,
         FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  // Drop receiver + arguments.
  __ add(sp, sp, args_count, LeaveCC);
}

// Tail-call |function_id| if |smi_entry| == |marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register smi_entry,
                                          OptimizationMarker marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ cmp(smi_entry, Operand(Smi::FromEnum(marker)));
  __ b(ne, &no_match);
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void MaybeTailCallOptimizedCodeSlot(MacroAssembler* masm,
                                           Register feedback_vector,
                                           Register scratch1, Register scratch2,
                                           Register scratch3) {
  // ----------- S t a t e -------------
  //  -- r0 : argument count (preserved for callee if needed, and caller)
  //  -- r3 : new target (preserved for callee if needed, and caller)
  //  -- r1 : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  // -----------------------------------
  DCHECK(
      !AreAliased(feedback_vector, r0, r1, r3, scratch1, scratch2, scratch3));

  Label optimized_code_slot_is_cell, fallthrough;

  Register closure = r1;
  Register optimized_code_entry = scratch1;

  __ ldr(
      optimized_code_entry,
      FieldMemOperand(feedback_vector, FeedbackVector::kOptimizedCodeOffset));

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret is as a weak cell to a code
  // object.
  __ JumpIfNotSmi(optimized_code_entry, &optimized_code_slot_is_cell);

  {
    // Optimized code slot is a Smi optimization marker.

    // Fall through if no optimization trigger.
    __ cmp(optimized_code_entry,
           Operand(Smi::FromEnum(OptimizationMarker::kNone)));
    __ b(eq, &fallthrough);

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
            Operand(Smi::FromEnum(OptimizationMarker::kInOptimizationQueue)));
        __ Assert(eq, kExpectedOptimizationSentinel);
      }
      __ jmp(&fallthrough);
    }
  }

  {
    // Optimized code slot is a WeakCell.
    __ bind(&optimized_code_slot_is_cell);

    __ ldr(optimized_code_entry,
           FieldMemOperand(optimized_code_entry, WeakCell::kValueOffset));
    __ JumpIfSmi(optimized_code_entry, &fallthrough);

    // Check if the optimized code is marked for deopt. If it is, call the
    // runtime to clear it.
    Label found_deoptimized_code;
    __ ldr(scratch2, FieldMemOperand(optimized_code_entry,
                                     Code::kCodeDataContainerOffset));
    __ ldr(
        scratch2,
        FieldMemOperand(scratch2, CodeDataContainer::kKindSpecificFlagsOffset));
    __ tst(scratch2, Operand(1 << Code::kMarkedForDeoptimizationBit));
    __ b(ne, &found_deoptimized_code);

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    // The feedback vector is no longer used, so re-use it as a scratch
    // register.
    ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                        scratch2, scratch3, feedback_vector);
    __ add(optimized_code_entry, optimized_code_entry,
           Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(optimized_code_entry);

    // Optimized code slot contains deoptimized code, evict it and re-enter the
    // closure's code.
    __ bind(&found_deoptimized_code);
    GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
  }

  // Fall-through if the optimized code cell is clear and there is no
  // optimization marker.
  __ bind(&fallthrough);
}

// Advance the current bytecode offset. This simulates what all bytecode
// handlers do upon completion of the underlying operation.
static void AdvanceBytecodeOffset(MacroAssembler* masm, Register bytecode_array,
                                  Register bytecode_offset, Register bytecode,
                                  Register scratch1) {
  Register bytecode_size_table = scratch1;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode));

  __ mov(
      bytecode_size_table,
      Operand(ExternalReference::bytecode_size_table_address(masm->isolate())));

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label load_size, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  __ cmp(bytecode, Operand(0x1));
  __ b(hi, &load_size);
  __ b(eq, &extra_wide);

  // Load the next bytecode and update table to the wide scaled table.
  __ add(bytecode_offset, bytecode_offset, Operand(1));
  __ ldrb(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ add(bytecode_size_table, bytecode_size_table,
         Operand(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&load_size);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ add(bytecode_offset, bytecode_offset, Operand(1));
  __ ldrb(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ add(bytecode_size_table, bytecode_size_table,
         Operand(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&load_size);

  // Load the size of the current bytecode.
  __ bind(&load_size);
  __ ldr(scratch1, MemOperand(bytecode_size_table, bytecode, LSL, 2));
  __ add(bytecode_offset, bytecode_offset, scratch1);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o r1: the JS function object being called.
//   o r3: the incoming new target or generator object
//   o cp: our context
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  Register closure = r1;
  Register feedback_vector = r2;

  // Load the feedback vector from the closure.
  __ ldr(feedback_vector,
         FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ ldr(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));
  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, r4, r6, r5);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(closure);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  Label maybe_load_debug_bytecode_array, bytecode_array_loaded;
  __ ldr(r0, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(r0, SharedFunctionInfo::kFunctionDataOffset));
  __ ldr(r4, FieldMemOperand(r0, SharedFunctionInfo::kDebugInfoOffset));
  __ SmiTst(r4);
  __ b(ne, &maybe_load_debug_bytecode_array);
  __ bind(&bytecode_array_loaded);

  // Increment invocation count for the function.
  __ ldr(r9, FieldMemOperand(feedback_vector,
                             FeedbackVector::kInvocationCountOffset));
  __ add(r9, r9, Operand(1));
  __ str(r9, FieldMemOperand(feedback_vector,
                             FeedbackVector::kInvocationCountOffset));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ SmiTst(kInterpreterBytecodeArrayRegister);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r0, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Reset code age.
  __ mov(r9, Operand(BytecodeArray::kNoAgeBytecodeAge));
  __ strb(r9, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kBytecodeAgeOffset));

  // Load the initial bytecode offset.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(r0, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, r0);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    __ ldr(r4, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                               BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ sub(r9, sp, Operand(r4));
    __ LoadRoot(r2, Heap::kRealStackLimitRootIndex);
    __ cmp(r9, Operand(r2));
    __ b(hs, &ok);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(r9, Heap::kUndefinedValueRootIndex);
    __ b(&loop_check, al);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(r9);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ sub(r4, r4, Operand(kPointerSize), SetCC);
    __ b(&loop_header, ge);
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in r3.
  __ ldr(r9, FieldMemOperand(
                 kInterpreterBytecodeArrayRegister,
                 BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ cmp(r9, Operand::Zero());
  __ str(r3, MemOperand(fp, r9, LSL, kPointerSizeLog2), ne);

  // Load accumulator with undefined.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));
  __ ldrb(r1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ ldr(r4, MemOperand(kInterpreterDispatchTableRegister, r1, LSL,
                        kPointerSizeLog2));
  __ Call(r4);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ ldr(kInterpreterBytecodeOffsetRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Check if we should return.
  Label do_return;
  __ ldrb(r1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ cmp(r1, Operand(static_cast<int>(interpreter::Bytecode::kReturn)));
  __ b(&do_return, eq);

  // Advance to the next bytecode and dispatch.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, r1, r2);
  __ jmp(&do_dispatch);

  __ bind(&do_return);
  // The return value is in r0.
  LeaveInterpreterFrame(masm, r2);
  __ Jump(lr);

  // Load debug copy of the bytecode array if it exists.
  // kInterpreterBytecodeArrayRegister is already loaded with
  // SharedFunctionInfo::kFunctionDataOffset.
  __ bind(&maybe_load_debug_bytecode_array);
  __ ldr(r9, FieldMemOperand(r4, DebugInfo::kFlagsOffset));
  __ SmiUntag(r9);
  __ tst(r9, Operand(DebugInfo::kHasBreakInfo));
  __ ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(r4, DebugInfo::kDebugBytecodeArrayOffset), ne);
  __ b(&bytecode_array_loaded);
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args, Register index,
                                         Register limit, Register scratch) {
  // Find the address of the last argument.
  __ mov(limit, num_args);
  __ mov(limit, Operand(limit, LSL, kPointerSizeLog2));
  __ sub(limit, index, limit);

  Label loop_header, loop_check;
  __ b(al, &loop_check);
  __ bind(&loop_header);
  __ ldr(scratch, MemOperand(index, -kPointerSize, PostIndex));
  __ push(scratch);
  __ bind(&loop_check);
  __ cmp(index, limit);
  __ b(gt, &loop_header);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- r1 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  __ add(r3, r0, Operand(1));  // Add one for receiver.

  Generate_StackOverflowCheck(masm, r3, r4, &stack_overflow);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ mov(r3, r0);  // Argument count is correct.
  }

  // Push the arguments. r2, r4, r5 will be modified.
  Generate_InterpreterPushArgs(masm, r3, r2, r4, r5);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(r2);                  // Pass the spread in a register
    __ sub(r0, r0, Operand(1));  // Subtract one for spread
  }

  // Call the target.
  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ Jump(
        masm->isolate()->builtins()->CallFunction(ConvertReceiverMode::kAny),
        RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(BUILTIN_CODE(masm->isolate(), CallWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny),
            RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ bkpt(0);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  // -- r0 : argument count (not including receiver)
  // -- r3 : new target
  // -- r1 : constructor to call
  // -- r2 : allocation site feedback if available, undefined otherwise.
  // -- r4 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver to be constructed.
  __ mov(r5, Operand::Zero());
  __ push(r5);

  Generate_StackOverflowCheck(masm, r0, r5, &stack_overflow);

  // Push the arguments. r5, r4, r6 will be modified.
  Generate_InterpreterPushArgs(masm, r0, r4, r5, r6);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(r2);                  // Pass the spread in a register
    __ sub(r0, r0, Operand(1));  // Subtract one for spread
  } else {
    __ AssertUndefinedOrAllocationSite(r2, r5);
  }

  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ AssertFunction(r1);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r4, FieldMemOperand(r4, SharedFunctionInfo::kConstructStubOffset));
    // Jump to the construct function.
    __ add(pc, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with r0, r1, and r3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with r0, r1, and r3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ bkpt(0);
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Smi* interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);
  __ Move(r2, BUILTIN_CODE(masm->isolate(), InterpreterEntryTrampoline));
  __ add(lr, r2, Operand(interpreter_entry_return_pc_offset->value() +
                         Code::kHeaderSize - kHeapObjectTag));

  // Initialize the dispatch table register.
  __ mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ SmiTst(kInterpreterBytecodeArrayRegister);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r1, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ ldr(kInterpreterBytecodeOffsetRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ ldrb(r1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ ldr(scratch, MemOperand(kInterpreterDispatchTableRegister, r1, LSL,
                             kPointerSizeLog2));
  __ Jump(scratch);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Get bytecode array and bytecode offset from the stack frame.
  __ ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ ldr(kInterpreterBytecodeOffsetRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Load the current bytecode.
  __ ldrb(r1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));

  // Advance to the next bytecode.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, r1, r2);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(r2, kInterpreterBytecodeOffsetRegister);
  __ str(r2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CheckOptimizationMarker(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : argument count (preserved for callee)
  //  -- r3 : new target (preserved for callee)
  //  -- r1 : target function (preserved for callee)
  // -----------------------------------
  Register closure = r1;

  // Get the feedback vector.
  Register feedback_vector = r2;
  __ ldr(feedback_vector,
         FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ ldr(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));

  // The feedback vector must be defined.
  if (FLAG_debug_code) {
    __ CompareRoot(feedback_vector, Heap::kUndefinedValueRootIndex);
    __ Assert(ne, BailoutReason::kExpectedFeedbackVector);
  }

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, r4, r6, r5);

  // Otherwise, tail call the SFI code.
  GenerateTailCallToSharedCode(masm);
}

void Builtins::Generate_CompileLazyDeoptimizedCode(MacroAssembler* masm) {
  // Set the code slot inside the JSFunction to the trampoline to the
  // interpreter entry.
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r2, FieldMemOperand(r2, SharedFunctionInfo::kCodeOffset));
  __ str(r2, FieldMemOperand(r1, JSFunction::kCodeOffset));
  __ RecordWriteField(r1, JSFunction::kCodeOffset, r2, r4, kLRHasNotBeenSaved,
                      kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  // Jump to compile lazy.
  Generate_CompileLazy(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : argument count (preserved for callee)
  //  -- r3 : new target (preserved for callee)
  //  -- r1 : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;

  Register closure = r1;
  Register feedback_vector = r2;

  // Do we have a valid feedback vector?
  __ ldr(feedback_vector,
         FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ ldr(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));
  __ JumpIfRoot(feedback_vector, Heap::kUndefinedValueRootIndex,
                &gotta_call_runtime);

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, r4, r6, r5);

  // We found no optimized code.
  Register entry = r4;
  __ ldr(entry,
         FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));

  // If SFI points to anything other than CompileLazy, install that.
  __ ldr(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ Move(r5, masm->CodeObject());
  __ cmp(entry, r5);
  __ b(eq, &gotta_call_runtime);

  // Install the SFI's code entry.
  __ str(entry, FieldMemOperand(closure, JSFunction::kCodeOffset));
  __ mov(r9, entry);  // Write barrier clobbers r9 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, r9, r5,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ add(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(entry);

  __ bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

// Lazy deserialization design doc: http://goo.gl/dxkYDZ.
void Builtins::Generate_DeserializeLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : argument count (preserved for callee)
  //  -- r3 : new target (preserved for callee)
  //  -- r1 : target function (preserved for callee)
  // -----------------------------------

  Label deserialize_in_runtime;

  Register target = r1;  // Must be preserved
  Register scratch0 = r2;
  Register scratch1 = r4;

  CHECK(scratch0 != r0 && scratch0 != r3 && scratch0 != r1);
  CHECK(scratch1 != r0 && scratch1 != r3 && scratch1 != r1);
  CHECK(scratch0 != scratch1);

  // Load the builtin id for lazy deserialization from SharedFunctionInfo.

  __ AssertFunction(target);
  __ ldr(scratch0,
         FieldMemOperand(target, JSFunction::kSharedFunctionInfoOffset));

  __ ldr(scratch1,
         FieldMemOperand(scratch0, SharedFunctionInfo::kFunctionDataOffset));
  __ AssertSmi(scratch1);

  // The builtin may already have been deserialized. If that is the case, it is
  // stored in the builtins table, and we can copy to correct code object to
  // both the shared function info and function without calling into runtime.
  //
  // Otherwise, we need to call into runtime to deserialize.

  {
    // Load the code object at builtins_table[builtin_id] into scratch1.

    __ SmiUntag(scratch1);
    __ Move(scratch0,
            Operand(ExternalReference::builtins_address(masm->isolate())));
    __ ldr(scratch1, MemOperand(scratch0, scratch1, LSL, kPointerSizeLog2));

    // Check if the loaded code object has already been deserialized. This is
    // the case iff it does not equal DeserializeLazy.

    __ Move(scratch0, masm->CodeObject());
    __ cmp(scratch1, scratch0);
    __ b(eq, &deserialize_in_runtime);
  }

  {
    // If we've reached this spot, the target builtin has been deserialized and
    // we simply need to copy it over. First to the shared function info.

    Register target_builtin = scratch1;
    Register shared = scratch0;

    __ ldr(shared,
           FieldMemOperand(target, JSFunction::kSharedFunctionInfoOffset));

    CHECK(r5 != target && r5 != scratch0 && r5 != scratch1);
    CHECK(r9 != target && r9 != scratch0 && r9 != scratch1);

    __ str(target_builtin,
           FieldMemOperand(shared, SharedFunctionInfo::kCodeOffset));
    __ mov(r9, target_builtin);  // Write barrier clobbers r9 below.
    __ RecordWriteField(shared, SharedFunctionInfo::kCodeOffset, r9, r5,
                        kLRHasNotBeenSaved, kDontSaveFPRegs,
                        OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // And second to the target function.

    __ str(target_builtin, FieldMemOperand(target, JSFunction::kCodeOffset));
    __ mov(r9, target_builtin);  // Write barrier clobbers r9 below.
    __ RecordWriteField(target, JSFunction::kCodeOffset, r9, r5,
                        kLRHasNotBeenSaved, kDontSaveFPRegs,
                        OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // All copying is done. Jump to the deserialized code object.

    __ add(target_builtin, target_builtin,
           Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(target_builtin);
  }

  __ bind(&deserialize_in_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kDeserializeLazy);
}

void Builtins::Generate_InstantiateAsmJs(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : argument count (preserved for callee)
  //  -- r1 : new target (preserved for callee)
  //  -- r3 : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ Move(r4, r0);
    // Push the number of arguments to the callee.
    __ SmiTag(r0);
    __ push(r0);
    // Push a copy of the target function and the new target.
    __ push(r1);
    __ push(r3);

    // The function.
    __ push(r1);
    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ cmp(r4, Operand(j));
        __ b(ne, &over);
      }
      for (int i = j - 1; i >= 0; --i) {
        __ ldr(r4, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                      i * kPointerSize));
        __ push(r4);
      }
      for (int i = 0; i < 3 - j; ++i) {
        __ PushRoot(Heap::kUndefinedValueRootIndex);
      }
      if (j < 3) {
        __ jmp(&args_done);
        __ bind(&over);
      }
    }
    __ bind(&args_done);

    // Call runtime, on success unwind frame, and parent frame.
    __ CallRuntime(Runtime::kInstantiateAsmJs, 4);
    // A smi 0 is returned on failure, an object on success.
    __ JumpIfSmi(r0, &failed);

    __ Drop(2);
    __ pop(r4);
    __ SmiUntag(r4);
    scope.GenerateLeaveFrame();

    __ add(r4, r4, Operand(1));
    __ Drop(r4);
    __ Ret();

    __ bind(&failed);
    // Restore target function and new target.
    __ pop(r3);
    __ pop(r1);
    __ pop(r0);
    __ SmiUntag(r0);
  }
  // On failure, tail call back to regular js by re-calling the function
  // which has be reset to the compile lazy builtin.
  __ ldr(r4, FieldMemOperand(r1, JSFunction::kCodeOffset));
  __ add(r4, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(r4);
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
    __ str(r0,
           MemOperand(
               sp, config->num_allocatable_general_registers() * kPointerSize +
                       BuiltinContinuationFrameConstants::kFixedFrameSize));
  }
  for (int i = allocatable_register_count - 1; i >= 0; --i) {
    int code = config->GetAllocatableGeneralCode(i);
    __ Pop(Register::from_code(code));
    if (java_script_builtin && code == kJavaScriptCallArgCountRegister.code()) {
      __ SmiUntag(Register::from_code(code));
    }
  }
  __ ldr(fp, MemOperand(
                 sp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));

  UseScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ Pop(scratch);
  __ add(sp, sp,
         Operand(BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(lr);
  __ add(pc, scratch, Operand(Code::kHeaderSize - kHeapObjectTag));
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
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), r0.code());
  __ pop(r0);
  __ Ret();
}

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ ldr(r0, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ ldr(r0, MemOperand(r0, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }

  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(r0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  Label skip;
  __ cmp(r0, Operand(Smi::kZero));
  __ b(ne, &skip);
  __ Ret();

  __ bind(&skip);

  // Drop any potential handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  if (has_handler_frame) {
    __ LeaveFrame(StackFrame::STUB);
  }

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ ldr(r1, FieldMemOperand(r0, Code::kDeoptimizationDataOffset));

  {
    ConstantPoolUnavailableScope constant_pool_unavailable(masm);
    __ add(r0, r0, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start

    // Load the OSR entrypoint offset from the deoptimization data.
    // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
    __ ldr(r1, FieldMemOperand(r1, FixedArray::OffsetOfElementAt(
                                       DeoptimizationData::kOsrPcOffsetIndex)));

    // Compute the target address = code start + osr_offset
    __ add(lr, r0, Operand::SmiUntag(r1));

    // And "return" to the OSR entry point of the function.
    __ Ret();
  }
}

void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  Generate_OnStackReplacementHelper(masm, false);
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  Generate_OnStackReplacementHelper(masm, true);
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0    : argc
  //  -- sp[0] : argArray
  //  -- sp[4] : thisArg
  //  -- sp[8] : receiver
  // -----------------------------------

  // 1. Load receiver into r1, argArray into r2 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
    __ mov(r2, r5);
    __ ldr(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));  // receiver
    __ sub(r4, r0, Operand(1), SetCC);
    __ ldr(r5, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // thisArg
    __ sub(r4, r4, Operand(1), SetCC, ge);
    __ ldr(r2, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // argArray
    __ add(sp, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ str(r5, MemOperand(sp, 0));
  }

  // ----------- S t a t e -------------
  //  -- r2    : argArray
  //  -- r1    : receiver
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(r2, Heap::kNullValueRootIndex, &no_arguments);
  __ JumpIfRoot(r2, Heap::kUndefinedValueRootIndex, &no_arguments);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ mov(r0, Operand(0));
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r0: actual number of arguments
  {
    Label done;
    __ cmp(r0, Operand::Zero());
    __ b(ne, &done);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ add(r0, r0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  // r0: actual number of arguments
  __ ldr(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // r0: actual number of arguments
  // r1: callable
  {
    Register scratch = r3;
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ add(r2, sp, Operand(r0, LSL, kPointerSizeLog2));

    __ bind(&loop);
    __ ldr(scratch, MemOperand(r2, -kPointerSize));
    __ str(scratch, MemOperand(r2));
    __ sub(r2, r2, Operand(kPointerSize));
    __ cmp(r2, sp);
    __ b(ne, &loop);
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ sub(r0, r0, Operand(1));
    __ pop();
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : argc
  //  -- sp[0]  : argumentsList
  //  -- sp[4]  : thisArgument
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r1 (if present), argumentsList into r2 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
    __ mov(r5, r1);
    __ mov(r2, r1);
    __ sub(r4, r0, Operand(1), SetCC);
    __ ldr(r1, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // target
    __ sub(r4, r4, Operand(1), SetCC, ge);
    __ ldr(r5, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // thisArgument
    __ sub(r4, r4, Operand(1), SetCC, ge);
    __ ldr(r2, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // argumentsList
    __ add(sp, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ str(r5, MemOperand(sp, 0));
  }

  // ----------- S t a t e -------------
  //  -- r2    : argumentsList
  //  -- r1    : target
  //  -- sp[0] : thisArgument
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
  //  -- r0     : argc
  //  -- sp[0]  : new.target (optional)
  //  -- sp[4]  : argumentsList
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r1 (if present), argumentsList into r2 (if present),
  // new.target into r3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
    __ mov(r2, r1);
    __ str(r2, MemOperand(sp, r0, LSL, kPointerSizeLog2));  // receiver
    __ sub(r4, r0, Operand(1), SetCC);
    __ ldr(r1, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // target
    __ mov(r3, r1);  // new.target defaults to target
    __ sub(r4, r4, Operand(1), SetCC, ge);
    __ ldr(r2, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // argumentsList
    __ sub(r4, r4, Operand(1), SetCC, ge);
    __ ldr(r3, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // new.target
    __ add(sp, sp, Operand(r0, LSL, kPointerSizeLog2));
  }

  // ----------- S t a t e -------------
  //  -- r2    : argumentsList
  //  -- r3    : new.target
  //  -- r1    : target
  //  -- sp[0] : receiver (undefined)
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

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ SmiTag(r0);
  __ mov(r4, Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ stm(db_w, sp, r0.bit() | r1.bit() | r4.bit() |
                       fp.bit() | lr.bit());
  __ add(fp, sp,
         Operand(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize));
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ ldr(r1, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                              kPointerSize)));

  __ LeaveFrame(StackFrame::ARGUMENTS_ADAPTOR);
  __ add(sp, sp, Operand::PointerOffsetFromSmiKey(r1));
  __ add(sp, sp, Operand(kPointerSize));  // adjust for receiver
}

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- r1 : target
  //  -- r0 : number of parameters on the stack (not including the receiver)
  //  -- r2 : arguments list (a FixedArray)
  //  -- r4 : len (number of elements to push from args)
  //  -- r3 : new.target (for [[Construct]])
  // -----------------------------------
  __ AssertFixedArray(r2);

  Register scratch = r8;

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(scratch, Heap::kRealStackLimitRootIndex);
    // The stack might already be overflowed here which will cause 'scratch' to
    // become negative.
    __ sub(scratch, sp, scratch);
    // Check if the arguments will overflow the stack.
    __ cmp(scratch, Operand(r4, LSL, kPointerSizeLog2));
    __ b(gt, &done);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    __ mov(r6, Operand(0));
    __ LoadRoot(r5, Heap::kTheHoleValueRootIndex);
    Label done, loop;
    __ bind(&loop);
    __ cmp(r6, r4);
    __ b(eq, &done);
    __ add(scratch, r2, Operand(r6, LSL, kPointerSizeLog2));
    __ ldr(scratch, FieldMemOperand(scratch, FixedArray::kHeaderSize));
    __ cmp(scratch, r5);
    __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex, eq);
    __ Push(scratch);
    __ add(r6, r6, Operand(1));
    __ b(&loop);
    __ bind(&done);
    __ add(r0, r0, r6);
  }

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r3 : the new.target (for [[Construct]] calls)
  //  -- r1 : the target to call (can be any Object)
  //  -- r2 : start index (to support rest parameters)
  // -----------------------------------

  Register scratch = r6;

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(r3, &new_target_not_constructor);
    __ ldr(scratch, FieldMemOperand(r3, HeapObject::kMapOffset));
    __ ldrb(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ tst(scratch, Operand(1 << Map::kIsConstructor));
    __ b(ne, &new_target_constructor);
    __ bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(r3);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ bind(&new_target_constructor);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ ldr(r4, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(scratch,
         MemOperand(r4, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ cmp(scratch,
         Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(eq, &arguments_adaptor);
  {
    __ ldr(r5, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    __ ldr(r5, FieldMemOperand(r5, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r5, FieldMemOperand(
                   r5, SharedFunctionInfo::kFormalParameterCountOffset));
    __ mov(r4, fp);
  }
  __ b(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    // Load the length from the ArgumentsAdaptorFrame.
    __ ldr(r5, MemOperand(r4, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ SmiUntag(r5);
  }
  __ bind(&arguments_done);

  Label stack_done, stack_overflow;
  __ sub(r5, r5, r2, SetCC);
  __ b(le, &stack_done);
  {
    // Check for stack overflow.
    Generate_StackOverflowCheck(masm, r5, r2, &stack_overflow);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ add(r4, r4, Operand(kPointerSize));
      __ add(r0, r0, r5);
      __ bind(&loop);
      {
        __ ldr(scratch, MemOperand(r4, r5, LSL, kPointerSizeLog2));
        __ push(scratch);
        __ sub(r5, r5, Operand(1), SetCC);
        __ b(ne, &loop);
      }
    }
  }
  __ b(&stack_done);
  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ bind(&stack_done);

  // Tail-call to the {code} handler.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(r1);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r3, FieldMemOperand(r2, SharedFunctionInfo::kCompilerHintsOffset));
  __ tst(r3, Operand(SharedFunctionInfo::kClassConstructorMask));
  __ b(ne, &class_constructor);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ ldr(r3, FieldMemOperand(r2, SharedFunctionInfo::kCompilerHintsOffset));
  __ tst(r3, Operand(SharedFunctionInfo::IsNativeBit::kMask |
                     SharedFunctionInfo::IsStrictBit::kMask));
  __ b(ne, &done_convert);
  {
    // ----------- S t a t e -------------
    //  -- r0 : the number of arguments (not including the receiver)
    //  -- r1 : the function to call (checked to be a JSFunction)
    //  -- r2 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(r3);
    } else {
      Label convert_to_object, convert_receiver;
      __ ldr(r3, MemOperand(sp, r0, LSL, kPointerSizeLog2));
      __ JumpIfSmi(r3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CompareObjectType(r3, r4, r4, FIRST_JS_RECEIVER_TYPE);
      __ b(hs, &done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(r3, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy);
        __ JumpIfNotRoot(r3, Heap::kNullValueRootIndex, &convert_to_object);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(r3);
        }
        __ b(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(r0);
        __ Push(r0, r1);
        __ mov(r0, r3);
        __ Push(cp);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ mov(r3, r0);
        __ Pop(r0, r1);
        __ SmiUntag(r0);
      }
      __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ str(r3, MemOperand(sp, r0, LSL, kPointerSizeLog2));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the function to call (checked to be a JSFunction)
  //  -- r2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ ldr(r2,
         FieldMemOperand(r2, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(r0);
  ParameterCount expected(r2);
  __ InvokeFunctionCode(r1, no_reg, expected, actual, JUMP_FUNCTION);

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ push(r1);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : target (checked to be a JSBoundFunction)
  //  -- r3 : new.target (only in case of [[Construct]])
  // -----------------------------------

  // Load [[BoundArguments]] into r2 and length of that into r4.
  Label no_bound_arguments;
  __ ldr(r2, FieldMemOperand(r1, JSBoundFunction::kBoundArgumentsOffset));
  __ ldr(r4, FieldMemOperand(r2, FixedArray::kLengthOffset));
  __ SmiUntag(r4);
  __ cmp(r4, Operand(0));
  __ b(eq, &no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- r0 : the number of arguments (not including the receiver)
    //  -- r1 : target (checked to be a JSBoundFunction)
    //  -- r2 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- r3 : new.target (only in case of [[Construct]])
    //  -- r4 : the number of [[BoundArguments]]
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ sub(sp, sp, Operand(r4, LSL, kPointerSizeLog2));
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRoot(sp, Heap::kRealStackLimitRootIndex);
      __ b(gt, &done);  // Signed comparison.
      // Restore the stack pointer.
      __ add(sp, sp, Operand(r4, LSL, kPointerSizeLog2));
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    Register scratch = r6;

    // Relocate arguments down the stack.
    {
      Label loop, done_loop;
      __ mov(r5, Operand(0));
      __ bind(&loop);
      __ cmp(r5, r0);
      __ b(gt, &done_loop);
      __ ldr(scratch, MemOperand(sp, r4, LSL, kPointerSizeLog2));
      __ str(scratch, MemOperand(sp, r5, LSL, kPointerSizeLog2));
      __ add(r4, r4, Operand(1));
      __ add(r5, r5, Operand(1));
      __ b(&loop);
      __ bind(&done_loop);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ ldr(r4, FieldMemOperand(r2, FixedArray::kLengthOffset));
      __ SmiUntag(r4);
      __ add(r2, r2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
      __ bind(&loop);
      __ sub(r4, r4, Operand(1), SetCC);
      __ ldr(scratch, MemOperand(r2, r4, LSL, kPointerSizeLog2));
      __ str(scratch, MemOperand(sp, r0, LSL, kPointerSizeLog2));
      __ add(r0, r0, Operand(1));
      __ b(gt, &loop);
    }
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(r1);

  // Patch the receiver to [[BoundThis]].
  __ ldr(r3, FieldMemOperand(r1, JSBoundFunction::kBoundThisOffset));
  __ str(r3, MemOperand(sp, r0, LSL, kPointerSizeLog2));

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ ldr(r1, FieldMemOperand(r1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(r1, &non_callable);
  __ bind(&non_smi);
  __ CompareObjectType(r1, r4, r5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, eq);
  __ cmp(r5, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  __ ldrb(r4, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ tst(r4, Operand(1 << Map::kIsCallable));
  __ b(eq, &non_callable);

  // Check if target is a proxy and call CallProxy external builtin
  __ cmp(r5, Operand(JS_PROXY_TYPE));
  __ b(ne, &non_function);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver the (original) target.
  __ str(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, r1);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the constructor to call (checked to be a JSFunction)
  //  -- r3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertFunction(r1);

  // Calling convention for function specific ConstructStubs require
  // r2 to contain either an AllocationSite or undefined.
  __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r4, FieldMemOperand(r4, SharedFunctionInfo::kConstructStubOffset));
  __ add(pc, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the function to call (checked to be a JSBoundFunction)
  //  -- r3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertBoundFunction(r1);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  __ cmp(r1, r3);
  __ ldr(r3, FieldMemOperand(r1, JSBoundFunction::kBoundTargetFunctionOffset),
         eq);

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ ldr(r1, FieldMemOperand(r1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the constructor to call (can be any Object)
  //  -- r3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor, non_proxy;
  __ JumpIfSmi(r1, &non_constructor);

  // Dispatch based on instance type.
  __ CompareObjectType(r1, r4, r5, JS_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Construct]] internal method.
  __ ldrb(r2, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ tst(r2, Operand(1 << Map::kIsConstructor));
  __ b(eq, &non_constructor);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ cmp(r5, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to proxies after checking whether they are constructors.
  __ cmp(r5, Operand(JS_PROXY_TYPE));
  __ b(ne, &non_proxy);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
  {
    // Overwrite the original receiver with the (original) target.
    __ str(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, r1);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructedNonConstructable),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_AllocateInNewSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r1 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r1);
  __ Push(r1);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r1 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r1);
  __ Move(r2, Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ Push(r1, r2);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

// static
void Builtins::Generate_Abort(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r1 : message_id as Smi
  //  -- lr : return address
  // -----------------------------------
  __ Push(r1);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbort);
}

// static
void Builtins::Generate_AbortJS(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r1 : message as String object
  //  -- lr : return address
  // -----------------------------------
  __ Push(r1);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbortJS);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : actual number of arguments
  //  -- r1 : function (passed through to callee)
  //  -- r2 : expected number of arguments
  //  -- r3 : new target (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ cmp(r0, r2);
  __ b(lt, &too_few);
  __ cmp(r2, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ b(eq, &dont_adapt_arguments);

  Register scratch = r5;

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r2, scratch, &stack_overflow);

    // Calculate copy start address into r0 and copy end address into r4.
    // r0: actual number of arguments as a smi
    // r1: function
    // r2: expected number of arguments
    // r3: new target (passed through to callee)
    __ add(r0, fp, Operand::PointerOffsetFromSmiKey(r0));
    // adjust for return address and receiver
    __ add(r0, r0, Operand(2 * kPointerSize));
    __ sub(r4, r0, Operand(r2, LSL, kPointerSizeLog2));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r0: copy start address
    // r1: function
    // r2: expected number of arguments
    // r3: new target (passed through to callee)
    // r4: copy end address

    Label copy;
    __ bind(&copy);
    __ ldr(scratch, MemOperand(r0, 0));
    __ push(scratch);
    __ cmp(r0, r4);  // Compare before moving to next argument.
    __ sub(r0, r0, Operand(kPointerSize));
    __ b(ne, &copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r2, scratch, &stack_overflow);

    // Calculate copy start address into r0 and copy end address is fp.
    // r0: actual number of arguments as a smi
    // r1: function
    // r2: expected number of arguments
    // r3: new target (passed through to callee)
    __ add(r0, fp, Operand::PointerOffsetFromSmiKey(r0));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r0: copy start address
    // r1: function
    // r2: expected number of arguments
    // r3: new target (passed through to callee)
    Label copy;
    __ bind(&copy);

    // Adjust load for return address and receiver.
    __ ldr(scratch, MemOperand(r0, 2 * kPointerSize));
    __ push(scratch);

    __ cmp(r0, fp);  // Compare before moving to next argument.
    __ sub(r0, r0, Operand(kPointerSize));
    __ b(ne, &copy);

    // Fill the remaining expected arguments with undefined.
    // r1: function
    // r2: expected number of arguments
    // r3: new target (passed through to callee)
    __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
    __ sub(r4, fp, Operand(r2, LSL, kPointerSizeLog2));
    // Adjust for frame.
    __ sub(r4, r4, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                           2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ push(scratch);
    __ cmp(sp, r4);
    __ b(ne, &fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mov(r0, r2);
  // r0 : expected number of arguments
  // r1 : function (passed through to callee)
  // r3 : new target (passed through to callee)
  __ ldr(r4, FieldMemOperand(r1, JSFunction::kCodeOffset));
  __ add(r4, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Call(r4);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Jump(lr);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ ldr(r4, FieldMemOperand(r1, JSFunction::kCodeOffset));
  __ add(r4, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(r4);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bkpt(0);
  }
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    constexpr RegList gp_regs = Register::ListOf<r0, r1, r2, r3>();
    constexpr DwVfpRegister lowest_fp_reg = d0;
    constexpr DwVfpRegister highest_fp_reg = d7;

    __ stm(db_w, sp, gp_regs);
    __ vstm(db_w, sp, lowest_fp_reg, highest_fp_reg);

    // Initialize cp register with kZero, CEntryStub will use it to set the
    // current context on the isolate.
    __ Move(cp, Smi::kZero);
    __ CallRuntime(Runtime::kWasmCompileLazy);
    // Store returned instruction start in r8.
    __ add(r8, r0, Operand(Code::kHeaderSize - kHeapObjectTag));

    // Restore registers.
    __ vldm(ia_w, sp, lowest_fp_reg, highest_fp_reg);
    __ ldm(ia_w, sp, gp_regs);
  }
  // Now jump to the instructions of the returned code object.
  __ Jump(r8);
}
#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

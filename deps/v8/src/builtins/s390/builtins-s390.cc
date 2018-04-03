// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/assembler-inl.h"
#include "src/code-stubs.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
  __ mov(r7, Operand(ExternalReference(address, masm->isolate())));
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
  //  -- r2                 : number of arguments excluding receiver
  //  -- r3                 : target
  //  -- r5                 : new.target
  //  -- r7                 : entry point
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------
  __ AssertFunction(r3);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));

  // CEntryStub expects r2 to contain the number of arguments including the
  // receiver and the extra arguments.
  __ AddP(r2, r2,
          Operand(BuiltinExitFrameConstants::kNumExtraArgsWithReceiver));

  // Insert extra arguments.
  __ PushRoot(Heap::kTheHoleValueRootIndex);  // Padding.
  __ SmiTag(r2);
  __ Push(r2, r3, r5);
  __ SmiUntag(r2);

  // Jump to the C entry runtime stub directly here instead of using
  // JumpToExternalReference. We have already loaded entry point to r7
  // in Generate_adaptor.
  __ LoadRR(r3, r7);
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
  //  -- r2     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, r3);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ LoadP(r4, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r4);
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForInternalArrayFunction,
              cr0);
    __ CompareObjectType(r4, r5, r6, MAP_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // tail call a stub
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

void Builtins::Generate_ArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, r3);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ LoadP(r4, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r4);
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r4, r5, r6, MAP_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedInitialMapForArrayFunction);
  }

  __ LoadRR(r5, r3);
  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ LoadP(ip, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(ip, FieldMemOperand(ip, SharedFunctionInfo::kCodeOffset));
  __ AddP(ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- r2 : argument count (preserved for callee)
  //  -- r3 : target function (preserved for callee)
  //  -- r5 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Push the number of arguments to the callee.
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ SmiTag(r2);
    __ Push(r2, r3, r5, r3);

    __ CallRuntime(function_id, 1);
    __ LoadRR(r4, r2);

    // Restore target function and new target.
    __ Pop(r2, r3, r5);
    __ SmiUntag(r2);
  }
  __ AddP(ip, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  Label post_instantiation_deopt_entry;
  // ----------- S t a t e -------------
  //  -- r2     : number of arguments
  //  -- r3     : constructor function
  //  -- r5     : new target
  //  -- cp     : context
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(r2);
    __ Push(cp, r2);
    __ SmiUntag(r2);
    // The receiver for the builtin/api call.
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    // Set up pointer to last argument.
    __ la(r6, MemOperand(fp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // r2: number of arguments
    // r3: constructor function
    // r4: address of last argument (caller sp)
    // r5: new target
    // cr0: condition indicating whether r2 is zero
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: number of arguments (smi-tagged)
    Label loop, no_args;
    __ beq(&no_args);
    __ ShiftLeftP(ip, r2, Operand(kPointerSizeLog2));
    __ SubP(sp, sp, ip);
    __ LoadRR(r1, r2);
    __ bind(&loop);
    __ lay(ip, MemOperand(ip, -kPointerSize));
    __ LoadP(r0, MemOperand(ip, r6));
    __ StoreP(r0, MemOperand(ip, sp));
    __ BranchOnCount(r1, &loop);
    __ bind(&no_args);

    // Call the function.
    // r2: number of arguments
    // r3: constructor function
    // r5: new target

    ParameterCount actual(r2);
    __ InvokeFunction(r3, r5, actual, CALL_FUNCTION);

    // Restore context from the frame.
    __ LoadP(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ LoadP(r3, MemOperand(fp, ConstructFrameConstants::kLengthOffset));

    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);

  __ SmiToPtrArrayOffset(r3, r3);
  __ AddP(sp, sp, r3);
  __ AddP(sp, sp, Operand(kPointerSize));
  __ Ret();
}

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Generate_JSConstructStubGeneric(MacroAssembler* masm,
                                     bool restrict_constructor_return) {
  // ----------- S t a t e -------------
  //  --      r2: number of arguments (untagged)
  //  --      r3: constructor function
  //  --      r5: new target
  //  --      cp: context
  //  --      lr: return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ SmiTag(r2);
    __ Push(cp, r2, r3);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ Push(r5);

    // ----------- S t a t e -------------
    //  --        sp[0*kPointerSize]: new target
    //  --        sp[1*kPointerSize]: padding
    //  -- r3 and sp[2*kPointerSize]: constructor function
    //  --        sp[3*kPointerSize]: number of arguments (tagged)
    //  --        sp[4*kPointerSize]: context
    // -----------------------------------

    __ LoadP(r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
    __ LoadlW(r6,
              FieldMemOperand(r6, SharedFunctionInfo::kCompilerHintsOffset));
    __ TestBitMask(r6, SharedFunctionInfo::kDerivedConstructorMask, r0);
    __ bne(&not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        r6, r7);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ b(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(r2, Heap::kTheHoleValueRootIndex);

    // ----------- S t a t e -------------
    //  --                          r2: receiver
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
    __ Pop(r5);
    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(r2, r2);

    // ----------- S t a t e -------------
    //  --                 r5: new target
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: implicit receiver
    //  -- sp[2*kPointerSize]: padding
    //  -- sp[3*kPointerSize]: constructor function
    //  -- sp[4*kPointerSize]: number of arguments (tagged)
    //  -- sp[5*kPointerSize]: context
    // -----------------------------------

    // Restore constructor function and argument count.
    __ LoadP(r3, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ LoadP(r2, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    __ SmiUntag(r2);

    // Set up pointer to last argument.
    __ la(r6, MemOperand(fp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, no_args;
    // ----------- S t a t e -------------
    //  --                        r2: number of arguments (untagged)
    //  --                        r5: new target
    //  --                        r6: pointer to last argument
    //  --                        cr0: condition indicating whether r2 is zero
    //  --        sp[0*kPointerSize]: implicit receiver
    //  --        sp[1*kPointerSize]: implicit receiver
    //  --        sp[2*kPointerSize]: padding
    //  -- r3 and sp[3*kPointerSize]: constructor function
    //  --        sp[4*kPointerSize]: number of arguments (tagged)
    //  --        sp[5*kPointerSize]: context
    // -----------------------------------

    __ beq(&no_args);
    __ ShiftLeftP(ip, r2, Operand(kPointerSizeLog2));
    __ SubP(sp, sp, ip);
    __ LoadRR(r1, r2);
    __ bind(&loop);
    __ lay(ip, MemOperand(ip, -kPointerSize));
    __ LoadP(r0, MemOperand(ip, r6));
    __ StoreP(r0, MemOperand(ip, sp));
    __ BranchOnCount(r1, &loop);
    __ bind(&no_args);

    // Call the function.
    ParameterCount actual(r2);
    __ InvokeFunction(r3, r5, actual, CALL_FUNCTION);

    // ----------- S t a t e -------------
    //  --                 r0: constructor result
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: padding
    //  -- sp[2*kPointerSize]: constructor function
    //  -- sp[3*kPointerSize]: number of arguments
    //  -- sp[4*kPointerSize]: context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore the context from the frame.
    __ LoadP(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, other_result, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(r2, Heap::kUndefinedValueRootIndex, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(r2, &other_result);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(r2, r6, r6, FIRST_JS_RECEIVER_TYPE);
    __ bge(&leave_frame);

    __ bind(&other_result);
    // The result is now neither undefined nor an object.
    if (restrict_constructor_return) {
      // Throw if constructor function is a class constructor
      __ LoadP(r6, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
      __ LoadP(r6, FieldMemOperand(r6, JSFunction::kSharedFunctionInfoOffset));
      __ LoadlW(r6,
                FieldMemOperand(r6, SharedFunctionInfo::kCompilerHintsOffset));
      __ TestBitMask(r6, SharedFunctionInfo::kClassConstructorMask, r0);
      __ beq(&use_receiver);
    } else {
      __ b(&use_receiver);
    }
    __ bind(&do_throw);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);
    }

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ LoadP(r2, MemOperand(sp));
    __ JumpIfRoot(r2, Heap::kTheHoleValueRootIndex, &do_throw);

    __ bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ LoadP(r3, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);

  __ SmiToPtrArrayOffset(r3, r3);
  __ AddP(sp, sp, r3);
  __ AddP(sp, sp, Operand(kPointerSize));
  __ Ret();
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
  //  -- r2 : the value to pass to the generator
  //  -- r3 : the JSGeneratorObject to resume
  //  -- lr : return address
  // -----------------------------------
  __ AssertGeneratorObject(r3);

  // Store input value into generator object.
  __ StoreP(r2, FieldMemOperand(r3, JSGeneratorObject::kInputOrDebugPosOffset),
            r0);
  __ RecordWriteField(r3, JSGeneratorObject::kInputOrDebugPosOffset, r2, r5,
                      kLRHasNotBeenSaved, kDontSaveFPRegs);

  // Load suspended function and context.
  __ LoadP(r6, FieldMemOperand(r3, JSGeneratorObject::kFunctionOffset));
  __ LoadP(cp, FieldMemOperand(r6, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ mov(ip, Operand(debug_hook));
  __ LoadB(ip, MemOperand(ip));
  __ CmpSmiLiteral(ip, Smi::kZero, r0);
  __ bne(&prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.

  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());

  __ mov(ip, Operand(debug_suspended_generator));
  __ LoadP(ip, MemOperand(ip));
  __ CmpP(ip, r3);
  __ beq(&prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ CompareRoot(sp, Heap::kRealStackLimitRootIndex);
  __ blt(&stack_overflow);

  // Push receiver.
  __ LoadP(ip, FieldMemOperand(r3, JSGeneratorObject::kReceiverOffset));
  __ Push(ip);

  // ----------- S t a t e -------------
  //  -- r3    : the JSGeneratorObject to resume
  //  -- r6    : generator function
  //  -- cp    : generator context
  //  -- lr    : return address
  //  -- sp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ LoadP(r5, FieldMemOperand(r6, JSFunction::kSharedFunctionInfoOffset));
  __ LoadW(
      r2, FieldMemOperand(r5, SharedFunctionInfo::kFormalParameterCountOffset));
  {
    Label loop, done_loop;
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
#if V8_TARGET_ARCH_S390X
    __ CmpP(r2, Operand::Zero());
    __ beq(&done_loop);
#else
    __ LoadAndTestP(r2, r2);
    __ beq(&done_loop);
#endif
    __ LoadRR(r1, r2);
    __ bind(&loop);
    __ push(ip);
    __ BranchOnCount(r1, &loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ LoadP(r5, FieldMemOperand(r5, SharedFunctionInfo::kFunctionDataOffset));
    __ CompareObjectType(r5, r5, r5, BYTECODE_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ LoadRR(r5, r3);
    __ LoadRR(r3, r6);
    __ LoadP(ip, FieldMemOperand(r3, JSFunction::kCodeOffset));
    __ AddP(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ JumpToJSEntry(ip);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r3, r6);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(r3);
    __ LoadP(r6, FieldMemOperand(r3, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r3);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(r3);
    __ LoadP(r6, FieldMemOperand(r3, JSGeneratorObject::kFunctionOffset));
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
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
  __ push(r3);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

// Clobbers r4; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(r4, Heap::kRealStackLimitRootIndex);
  // Make r4 the space we have left. The stack might already be overflowed
  // here which will cause r4 to become negative.
  __ SubP(r4, sp, r4);
  // Check if the arguments will overflow the stack.
  __ ShiftLeftP(r0, argc, Operand(kPointerSizeLog2));
  __ CmpP(r4, r0);
  __ bgt(&okay);  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r2: new.target
  // r3: function
  // r4: receiver
  // r5: argc
  // r6: argv
  // r0,r7-r9, cp may be clobbered
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Enter an internal frame.
  {
    // FrameScope ends up calling MacroAssembler::EnterFrame here
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(IsolateAddressId::kContextAddress,
                                      masm->isolate());
    __ mov(cp, Operand(context_address));
    __ LoadP(cp, MemOperand(cp));

    // Push the function and the receiver onto the stack.
    __ Push(r3, r4);

    // Check if we have enough stack space to push all arguments.
    // Clobbers r4.
    Generate_CheckStackOverflow(masm, r5);

    // Copy arguments to the stack in a loop from argv to sp.
    // The arguments are actually placed in reverse order on sp
    // compared to argv (i.e. arg1 is highest memory in sp).
    // r3: function
    // r5: argc
    // r6: argv, i.e. points to first arg
    // r7: scratch reg to hold scaled argc
    // r8: scratch reg to hold arg handle
    // r9: scratch reg to hold index into argv
    Label argLoop, argExit;
    intptr_t zero = 0;
    __ ShiftLeftP(r7, r5, Operand(kPointerSizeLog2));
    __ SubRR(sp, r7);                // Buy the stack frame to fit args
    __ LoadImmP(r9, Operand(zero));  // Initialize argv index
    __ bind(&argLoop);
    __ CmpPH(r7, Operand(zero));
    __ beq(&argExit, Label::kNear);
    __ lay(r7, MemOperand(r7, -kPointerSize));
    __ LoadP(r8, MemOperand(r9, r6));         // read next parameter
    __ la(r9, MemOperand(r9, kPointerSize));  // r9++;
    __ LoadP(r0, MemOperand(r8));             // dereference handle
    __ StoreP(r0, MemOperand(r7, sp));        // push parameter
    __ b(&argLoop);
    __ bind(&argExit);

    // Setup new.target and argc.
    __ LoadRR(r6, r2);
    __ LoadRR(r2, r5);
    __ LoadRR(r5, r6);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
    __ LoadRR(r7, r6);
    __ LoadRR(r8, r6);
    __ LoadRR(r9, r6);

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the JS frame and remove the parameters (except function), and
    // return.
  }
  __ b(r14);

  // r2: result
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
  __ StoreP(optimized_code, FieldMemOperand(closure, JSFunction::kCodeOffset),
            r0);
  __ LoadRR(scratch1,
            optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_count = scratch;

  // Get the arguments + receiver count.
  __ LoadP(args_count,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadlW(args_count,
            FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  __ AddP(sp, sp, args_count);
}

// Tail-call |function_id| if |smi_entry| == |marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register smi_entry,
                                          OptimizationMarker marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ CmpSmiLiteral(smi_entry, Smi::FromEnum(marker), r0);
  __ bne(&no_match);
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
      !AreAliased(feedback_vector, r2, r3, r5, scratch1, scratch2, scratch3));

  Label optimized_code_slot_is_cell, fallthrough;

  Register closure = r3;
  Register optimized_code_entry = scratch1;

  __ LoadP(
      optimized_code_entry,
      FieldMemOperand(feedback_vector, FeedbackVector::kOptimizedCodeOffset));

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret is as a weak cell to a code
  // object.
  __ JumpIfNotSmi(optimized_code_entry, &optimized_code_slot_is_cell);

  {
    // Optimized code slot is a Smi optimization marker.

    // Fall through if no optimization trigger.
    __ CmpSmiLiteral(optimized_code_entry,
                     Smi::FromEnum(OptimizationMarker::kNone), r0);
    __ beq(&fallthrough);

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
        __ CmpSmiLiteral(
            optimized_code_entry,
            Smi::FromEnum(OptimizationMarker::kInOptimizationQueue), r0);
        __ Assert(eq, AbortReason::kExpectedOptimizationSentinel);
      }
      __ b(&fallthrough, Label::kNear);
    }
  }

  {
    // Optimized code slot is a WeakCell.
    __ bind(&optimized_code_slot_is_cell);

    __ LoadP(optimized_code_entry,
             FieldMemOperand(optimized_code_entry, WeakCell::kValueOffset));
    __ JumpIfSmi(optimized_code_entry, &fallthrough);

    // Check if the optimized code is marked for deopt. If it is, call the
    // runtime to clear it.
    Label found_deoptimized_code;
    __ LoadP(scratch2, FieldMemOperand(optimized_code_entry,
                                       Code::kCodeDataContainerOffset));
    __ LoadW(
        scratch2,
        FieldMemOperand(scratch2, CodeDataContainer::kKindSpecificFlagsOffset));
    __ TestBit(scratch2, Code::kMarkedForDeoptimizationBit, r0);
    __ bne(&found_deoptimized_code);

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    // The feedback vector is no longer used, so re-use it as a scratch
    // register.
    ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                        scratch2, scratch3, feedback_vector);
    __ AddP(optimized_code_entry, optimized_code_entry,
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
  Register scratch2 = bytecode;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode));
  __ mov(
      bytecode_size_table,
      Operand(ExternalReference::bytecode_size_table_address(masm->isolate())));

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label load_size, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  __ CmpP(bytecode, Operand(0x1));
  __ bgt(&load_size);
  __ beq(&extra_wide);

  // Load the next bytecode and update table to the wide scaled table.
  __ AddP(bytecode_offset, bytecode_offset, Operand(1));
  __ LoadlB(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ AddP(bytecode_size_table, bytecode_size_table,
          Operand(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ b(&load_size);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ AddP(bytecode_offset, bytecode_offset, Operand(1));
  __ LoadlB(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ AddP(bytecode_size_table, bytecode_size_table,
          Operand(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));

  // Load the size of the current bytecode.
  __ bind(&load_size);
  __ ShiftLeftP(scratch2, bytecode, Operand(2));
  __ LoadlW(scratch2, MemOperand(bytecode_size_table, scratch2));
  __ AddP(bytecode_offset, bytecode_offset, scratch2);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o r3: the JS function object being called.
//   o r5: the incoming new target or generator object
//   o cp: our context
//   o pp: the caller's constant pool pointer (if enabled)
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  Register closure = r3;
  Register feedback_vector = r4;

  // Load the feedback vector from the closure.
  __ LoadP(feedback_vector,
           FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ LoadP(feedback_vector,
           FieldMemOperand(feedback_vector, Cell::kValueOffset));
  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, r6, r8, r7);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(closure);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  Label maybe_load_debug_bytecode_array, bytecode_array_loaded;
  __ LoadP(r2, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Load original bytecode array or the debug copy.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           FieldMemOperand(r2, SharedFunctionInfo::kFunctionDataOffset));
  __ LoadP(r6, FieldMemOperand(r2, SharedFunctionInfo::kDebugInfoOffset));
  __ TestIfSmi(r6);
  __ bne(&maybe_load_debug_bytecode_array);
  __ bind(&bytecode_array_loaded);

  // Increment invocation count for the function.
  __ LoadW(r1, FieldMemOperand(feedback_vector,
                               FeedbackVector::kInvocationCountOffset));
  __ AddP(r1, r1, Operand(1));
  __ StoreW(r1, FieldMemOperand(feedback_vector,
                                FeedbackVector::kInvocationCountOffset));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ TestIfSmi(kInterpreterBytecodeArrayRegister);
    __ Assert(
        ne, AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r2, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(
        eq, AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Reset code age.
  __ mov(r1, Operand(BytecodeArray::kNoAgeBytecodeAge));
  __ StoreByte(r1, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                   BytecodeArray::kBytecodeAgeOffset),
               r0);

  // Load the initial bytecode offset.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(r4, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, r4);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size (word) from the BytecodeArray object.
    __ LoadlW(r4, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                  BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ SubP(r8, sp, r4);
    __ LoadRoot(r0, Heap::kRealStackLimitRootIndex);
    __ CmpLogicalP(r8, r0);
    __ bge(&ok);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    Label loop, no_args;
    __ LoadRoot(r8, Heap::kUndefinedValueRootIndex);
    __ ShiftRightP(r4, r4, Operand(kPointerSizeLog2));
    __ LoadAndTestP(r4, r4);
    __ beq(&no_args);
    __ LoadRR(r1, r4);
    __ bind(&loop);
    __ push(r8);
    __ SubP(r1, Operand(1));
    __ bne(&loop);
    __ bind(&no_args);
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in r6.
  Label no_incoming_new_target_or_generator_register;
  __ LoadW(r8, FieldMemOperand(
                   kInterpreterBytecodeArrayRegister,
                   BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ CmpP(r8, Operand::Zero());
  __ beq(&no_incoming_new_target_or_generator_register);
  __ ShiftLeftP(r8, r8, Operand(kPointerSizeLog2));
  __ StoreP(r5, MemOperand(fp, r8));
  __ bind(&no_incoming_new_target_or_generator_register);

  // Load accumulator with undefined.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  __ LoadlB(r3, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftP(ip, r3, Operand(kPointerSizeLog2));
  __ LoadP(ip, MemOperand(kInterpreterDispatchTableRegister, ip));
  __ Call(ip);

  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadP(kInterpreterBytecodeOffsetRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Check if we should return.
  Label do_return;
  __ LoadlB(r3, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));
  __ CmpP(r3, Operand(static_cast<int>(interpreter::Bytecode::kReturn)));
  __ beq(&do_return);

  // Advance to the next bytecode and dispatch.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, r3, r4);
  __ b(&do_dispatch);

  __ bind(&do_return);
  // The return value is in r2.
  LeaveInterpreterFrame(masm, r4);
  __ Ret();

  // Load debug copy of the bytecode array if it exists.
  // kInterpreterBytecodeArrayRegister is already loaded with
  // SharedFunctionInfo::kFunctionDataOffset.
  Label done;
  __ bind(&maybe_load_debug_bytecode_array);
  __ LoadP(ip, FieldMemOperand(r6, DebugInfo::kFlagsOffset));
  __ SmiUntag(ip);
  __ tmll(ip, Operand(DebugInfo::kHasBreakInfo));
  __ beq(&done);
  __ LoadP(kInterpreterBytecodeArrayRegister,
           FieldMemOperand(r6, DebugInfo::kDebugBytecodeArrayOffset));
  __ bind(&done);
  __ b(&bytecode_array_loaded);
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
  __ SubP(scratch, sp, scratch);
  // Check if the arguments will overflow the stack.
  __ ShiftLeftP(r0, num_args, Operand(kPointerSizeLog2));
  __ CmpP(scratch, r0);
  __ ble(stack_overflow);  // Signed comparison.
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args, Register index,
                                         Register count, Register scratch) {
  Label loop, skip;
  __ CmpP(count, Operand::Zero());
  __ beq(&skip);
  __ AddP(index, index, Operand(kPointerSize));  // Bias up for LoadPU
  __ LoadRR(r0, count);
  __ bind(&loop);
  __ LoadP(scratch, MemOperand(index, -kPointerSize));
  __ lay(index, MemOperand(index, -kPointerSize));
  __ push(scratch);
  __ SubP(r0, Operand(1));
  __ bne(&loop);
  __ bind(&skip);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r4 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- r3 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  // Calculate number of arguments (AddP one for receiver).
  __ AddP(r5, r2, Operand(1));
  Generate_StackOverflowCheck(masm, r5, ip, &stack_overflow);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ LoadRR(r5, r2);  // Argument count is correct.
  }

  // Push the arguments.
  Generate_InterpreterPushArgs(masm, r5, r4, r5, r6);
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(r4);                   // Pass the spread in a register
    __ SubP(r2, r2, Operand(1));  // Subtract one for spread
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
    // Unreachable Code.
    __ bkpt(0);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  // -- r2 : argument count (not including receiver)
  // -- r5 : new target
  // -- r3 : constructor to call
  // -- r4 : allocation site feedback if available, undefined otherwise.
  // -- r6 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver to be constructed.
  __ LoadImmP(r0, Operand::Zero());
  __ push(r0);

  // Push the arguments (skip if none).
  Label skip;
  __ CmpP(r2, Operand::Zero());
  __ beq(&skip);
  Generate_StackOverflowCheck(masm, r2, ip, &stack_overflow);
  Generate_InterpreterPushArgs(masm, r2, r6, r2, r7);
  __ bind(&skip);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(r4);                   // Pass the spread in a register
    __ SubP(r2, r2, Operand(1));  // Subtract one for spread
  } else {
    __ AssertUndefinedOrAllocationSite(r4, r7);
  }
  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ AssertFunction(r3);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ LoadP(r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
    __ LoadP(r6, FieldMemOperand(r6, SharedFunctionInfo::kConstructStubOffset));
    // Jump to the construct function.
    __ AddP(ip, r6, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(ip);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with r2, r3, and r5 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with r2, r3, and r5 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable Code.
    __ bkpt(0);
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Smi* interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);
  __ Move(r4, BUILTIN_CODE(masm->isolate(), InterpreterEntryTrampoline));
  __ AddP(r14, r4, Operand(interpreter_entry_return_pc_offset->value() +
                           Code::kHeaderSize - kHeapObjectTag));

  // Initialize the dispatch table register.
  __ mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ TestIfSmi(kInterpreterBytecodeArrayRegister);
    __ Assert(
        ne, AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r3, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(
        eq, AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ LoadP(kInterpreterBytecodeOffsetRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ LoadlB(r3, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftP(ip, r3, Operand(kPointerSizeLog2));
  __ LoadP(ip, MemOperand(kInterpreterDispatchTableRegister, ip));
  __ Jump(ip);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Get bytecode array and bytecode offset from the stack frame.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadP(kInterpreterBytecodeOffsetRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Load the current bytecode.
  __ LoadlB(r3, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));

  // Advance to the next bytecode.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, r3, r4);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(r4, kInterpreterBytecodeOffsetRegister);
  __ StoreP(r4,
            MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CheckOptimizationMarker(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : argument count (preserved for callee)
  //  -- r6 : new target (preserved for callee)
  //  -- r4 : target function (preserved for callee)
  // -----------------------------------
  Register closure = r3;

  // Get the feedback vector.
  Register feedback_vector = r4;
  __ LoadP(feedback_vector,
           FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ LoadP(feedback_vector,
           FieldMemOperand(feedback_vector, Cell::kValueOffset));

  // The feedback vector must be defined.
  if (FLAG_debug_code) {
    __ CompareRoot(feedback_vector, Heap::kUndefinedValueRootIndex);
    __ Assert(ne, AbortReason::kExpectedFeedbackVector);
  }

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, r6, r8, r7);

  // Otherwise, tail call the SFI code.
  GenerateTailCallToSharedCode(masm);
}

void Builtins::Generate_CompileLazyDeoptimizedCode(MacroAssembler* masm) {
  // Set the code slot inside the JSFunction to the trampoline to the
  // interpreter entry.
  __ LoadP(r4, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r4, FieldMemOperand(r4, SharedFunctionInfo::kCodeOffset));
  __ StoreP(r4, FieldMemOperand(r3, JSFunction::kCodeOffset));
  __ RecordWriteField(r3, JSFunction::kCodeOffset, r4, r6, kLRHasNotBeenSaved,
                      kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  // Jump to compile lazy.
  Generate_CompileLazy(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : argument count (preserved for callee)
  //  -- r5 : new target (preserved for callee)
  //  -- r3 : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;

  Register closure = r3;
  Register feedback_vector = r4;

  // Do we have a valid feedback vector?
  __ LoadP(feedback_vector,
           FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ LoadP(feedback_vector,
           FieldMemOperand(feedback_vector, Cell::kValueOffset));
  __ JumpIfRoot(feedback_vector, Heap::kUndefinedValueRootIndex,
                &gotta_call_runtime);

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, r6, r8, r7);

  // We found no optimized code.
  Register entry = r6;
  __ LoadP(entry,
           FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));

  // If SFI points to anything other than CompileLazy, install that.
  __ LoadP(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ mov(r7, Operand(masm->CodeObject()));
  __ CmpP(entry, r7);
  __ beq(&gotta_call_runtime);

  // Install the SFI's code entry.
  __ StoreP(entry, FieldMemOperand(closure, JSFunction::kCodeOffset), r0);
  __ LoadRR(r8, entry);  // Write barrier clobbers r8 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, r8, r7,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ AddP(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(entry);

  __ bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

// Lazy deserialization design doc: http://goo.gl/dxkYDZ.
void Builtins::Generate_DeserializeLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : argument count (preserved for callee)
  //  -- r5 : new target (preserved for callee)
  //  -- r3 : target function (preserved for callee)
  // -----------------------------------

  Label deserialize_in_runtime;

  Register target = r3;  // Must be preserved
  Register scratch0 = r4;
  Register scratch1 = r6;

  CHECK(scratch0 != r2 && scratch0 != r5 && scratch0 != r3);
  CHECK(scratch1 != r2 && scratch1 != r5 && scratch1 != r3);
  CHECK(scratch0 != scratch1);

  // Load the builtin id for lazy deserialization from SharedFunctionInfo.

  __ AssertFunction(target);
  __ LoadP(scratch0,
           FieldMemOperand(target, JSFunction::kSharedFunctionInfoOffset));

  __ LoadP(scratch1,
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
    __ mov(scratch0,
           Operand(ExternalReference::builtins_address(masm->isolate())));
    __ ShiftLeftP(scratch1, scratch1, Operand(kPointerSizeLog2));
    __ LoadP(scratch1, MemOperand(scratch0, scratch1));

    // Check if the loaded code object has already been deserialized. This is
    // the case iff it does not equal DeserializeLazy.

    __ Move(scratch0, masm->CodeObject());
    __ CmpP(scratch1, scratch0);
    __ beq(&deserialize_in_runtime);
  }
  {
    // If we've reached this spot, the target builtin has been deserialized and
    // we simply need to copy it over. First to the shared function info.

    Register target_builtin = scratch1;
    Register shared = scratch0;

    __ LoadP(shared,
             FieldMemOperand(target, JSFunction::kSharedFunctionInfoOffset));

    CHECK(r7 != target && r7 != scratch0 && r7 != scratch1);
    CHECK(r8 != target && r8 != scratch0 && r8 != scratch1);

    __ StoreP(target_builtin,
              FieldMemOperand(shared, SharedFunctionInfo::kCodeOffset));
    __ LoadRR(r8, target_builtin);  // Write barrier clobbers r9 below.
    __ RecordWriteField(shared, SharedFunctionInfo::kCodeOffset, r8, r7,
                        kLRHasNotBeenSaved, kDontSaveFPRegs,
                        OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // And second to the target function.

    __ StoreP(target_builtin, FieldMemOperand(target, JSFunction::kCodeOffset));
    __ LoadRR(r8, target_builtin);  // Write barrier clobbers r9 below.
    __ RecordWriteField(target, JSFunction::kCodeOffset, r8, r7,
                        kLRHasNotBeenSaved, kDontSaveFPRegs,
                        OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // All copying is done. Jump to the deserialized code object.

    __ AddP(target_builtin, target_builtin,
            Operand(Code::kHeaderSize - kHeapObjectTag));
    __ LoadRR(ip, target_builtin);
    __ Jump(ip);
  }

  __ bind(&deserialize_in_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kDeserializeLazy);
}

void Builtins::Generate_InstantiateAsmJs(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : argument count (preserved for callee)
  //  -- r3 : new target (preserved for callee)
  //  -- r5 : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ Move(r6, r2);
    // Push a copy of the target function and the new target.
    __ SmiTag(r2);
    // Push another copy as a parameter to the runtime call.
    __ Push(r2, r3, r5, r3);

    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ CmpP(r6, Operand(j));
        __ b(ne, &over);
      }
      for (int i = j - 1; i >= 0; --i) {
        __ LoadP(r6, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                        i * kPointerSize));
        __ push(r6);
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
    __ JumpIfSmi(r2, &failed);

    __ Drop(2);
    __ pop(r6);
    __ SmiUntag(r6);
    scope.GenerateLeaveFrame();

    __ AddP(r6, r6, Operand(1));
    __ Drop(r6);
    __ Ret();

    __ bind(&failed);
    // Restore target function and new target.
    __ Pop(r2, r3, r5);
    __ SmiUntag(r2);
  }
  // On failure, tail call back to regular js by re-calling the function
  // which has be reset to the compile lazy builtin.
  __ LoadP(ip, FieldMemOperand(r3, JSFunction::kCodeOffset));
  __ AddP(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
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
    __ StoreP(
        r2, MemOperand(
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
  __ LoadP(
      fp,
      MemOperand(sp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(ip);
  __ AddP(sp, sp,
          Operand(BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(r0);
  __ LoadRR(r14, r0);
  __ AddP(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(ip);
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
  }

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), r2.code());
  __ pop(r2);
  __ Ret();
}

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ LoadP(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ LoadP(r2, MemOperand(r2, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ LoadP(r2, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(r2);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  Label skip;
  __ CmpSmiLiteral(r2, Smi::kZero, r0);
  __ bne(&skip);
  __ Ret();

  __ bind(&skip);

  // Drop any potential handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  if (has_handler_frame) {
    __ LeaveFrame(StackFrame::STUB);
  }

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ LoadP(r3, FieldMemOperand(r2, Code::kDeoptimizationDataOffset));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ LoadP(r3, FieldMemOperand(r3, FixedArray::OffsetOfElementAt(
                                       DeoptimizationData::kOsrPcOffsetIndex)));
  __ SmiUntag(r3);

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ AddP(r2, r3);
  __ AddP(r0, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ LoadRR(r14, r0);

  // And "return" to the OSR entry point of the function.
  __ Ret();
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
  //  -- r2    : argc
  //  -- sp[0] : argArray
  //  -- sp[4] : thisArg
  //  -- sp[8] : receiver
  // -----------------------------------

  // 1. Load receiver into r3, argArray into r4 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label skip;
    Register arg_size = r7;
    Register new_sp = r5;
    Register scratch = r6;
    __ ShiftLeftP(arg_size, r2, Operand(kPointerSizeLog2));
    __ AddP(new_sp, sp, arg_size);
    __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
    __ LoadRR(r4, scratch);
    __ LoadP(r3, MemOperand(new_sp, 0));  // receiver
    __ CmpP(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(scratch, MemOperand(new_sp, 1 * -kPointerSize));  // thisArg
    __ beq(&skip);
    __ LoadP(r4, MemOperand(new_sp, 2 * -kPointerSize));  // argArray
    __ bind(&skip);
    __ LoadRR(sp, new_sp);
    __ StoreP(scratch, MemOperand(sp, 0));
  }

  // ----------- S t a t e -------------
  //  -- r4    : argArray
  //  -- r3    : receiver
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(r4, Heap::kNullValueRootIndex, &no_arguments);
  __ JumpIfRoot(r4, Heap::kUndefinedValueRootIndex, &no_arguments);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ LoadImmP(r2, Operand::Zero());
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r2: actual number of arguments
  {
    Label done;
    __ CmpP(r2, Operand::Zero());
    __ bne(&done, Label::kNear);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ AddP(r2, Operand(1));
    __ bind(&done);
  }

  // r2: actual number of arguments
  // 2. Get the callable to call (passed as receiver) from the stack.
  __ ShiftLeftP(r4, r2, Operand(kPointerSizeLog2));
  __ LoadP(r3, MemOperand(sp, r4));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // r2: actual number of arguments
  // r3: callable
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ AddP(r4, sp, r4);

    __ bind(&loop);
    __ LoadP(ip, MemOperand(r4, -kPointerSize));
    __ StoreP(ip, MemOperand(r4));
    __ SubP(r4, Operand(kPointerSize));
    __ CmpP(r4, sp);
    __ bne(&loop);
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ SubP(r2, Operand(1));
    __ pop();
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : argc
  //  -- sp[0]  : argumentsList
  //  -- sp[4]  : thisArgument
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r3 (if present), argumentsList into r4 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label skip;
    Register arg_size = r7;
    Register new_sp = r5;
    Register scratch = r6;
    __ ShiftLeftP(arg_size, r2, Operand(kPointerSizeLog2));
    __ AddP(new_sp, sp, arg_size);
    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
    __ LoadRR(scratch, r3);
    __ LoadRR(r4, r3);
    __ CmpP(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(r3, MemOperand(new_sp, 1 * -kPointerSize));  // target
    __ beq(&skip);
    __ LoadP(scratch, MemOperand(new_sp, 2 * -kPointerSize));  // thisArgument
    __ CmpP(arg_size, Operand(2 * kPointerSize));
    __ beq(&skip);
    __ LoadP(r4, MemOperand(new_sp, 3 * -kPointerSize));  // argumentsList
    __ bind(&skip);
    __ LoadRR(sp, new_sp);
    __ StoreP(scratch, MemOperand(sp, 0));
  }

  // ----------- S t a t e -------------
  //  -- r4    : argumentsList
  //  -- r3    : target
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // 2. We don't need to check explicitly for callable target here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3 Apply the target to the given argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : argc
  //  -- sp[0]  : new.target (optional)
  //  -- sp[4]  : argumentsList
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r3 (if present), argumentsList into r4 (if present),
  // new.target into r5 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label skip;
    Register arg_size = r7;
    Register new_sp = r6;
    __ ShiftLeftP(arg_size, r2, Operand(kPointerSizeLog2));
    __ AddP(new_sp, sp, arg_size);
    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
    __ LoadRR(r4, r3);
    __ LoadRR(r5, r3);
    __ StoreP(r3, MemOperand(new_sp, 0));  // receiver (undefined)
    __ CmpP(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(r3, MemOperand(new_sp, 1 * -kPointerSize));  // target
    __ LoadRR(r5, r3);  // new.target defaults to target
    __ beq(&skip);
    __ LoadP(r4, MemOperand(new_sp, 2 * -kPointerSize));  // argumentsList
    __ CmpP(arg_size, Operand(2 * kPointerSize));
    __ beq(&skip);
    __ LoadP(r5, MemOperand(new_sp, 3 * -kPointerSize));  // new.target
    __ bind(&skip);
    __ LoadRR(sp, new_sp);
  }

  // ----------- S t a t e -------------
  //  -- r4    : argumentsList
  //  -- r5    : new.target
  //  -- r3    : target
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
  __ SmiTag(r2);
  __ Load(r6, Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  // Stack updated as such:
  //    old SP --->
  //                 R14 Return Addr
  //                 Old FP                     <--- New FP
  //                 Argument Adapter SMI
  //                 Function
  //                 ArgC as SMI
  //                 Padding                    <--- New SP
  __ lay(sp, MemOperand(sp, -5 * kPointerSize));

  // Cleanse the top nibble of 31-bit pointers.
  __ CleanseP(r14);
  __ StoreP(r14, MemOperand(sp, 4 * kPointerSize));
  __ StoreP(fp, MemOperand(sp, 3 * kPointerSize));
  __ StoreP(r6, MemOperand(sp, 2 * kPointerSize));
  __ StoreP(r3, MemOperand(sp, 1 * kPointerSize));
  __ StoreP(r2, MemOperand(sp, 0 * kPointerSize));
  __ Push(Smi::kZero);  // Padding.
  __ la(fp,
        MemOperand(sp, ArgumentsAdaptorFrameConstants::kFixedFrameSizeFromFp));
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ LoadP(r3, MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  int stack_adjustment = kPointerSize;  // adjust for receiver
  __ LeaveFrame(StackFrame::ARGUMENTS_ADAPTOR, stack_adjustment);
  __ SmiToPtrArrayOffset(r3, r3);
  __ lay(sp, MemOperand(sp, r3));
}

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- r3 : target
  //  -- r2 : number of parameters on the stack (not including the receiver)
  //  -- r4 : arguments list (a FixedArray)
  //  -- r6 : len (number of elements to push from args)
  //  -- r5 : new.target (for [[Construct]])
  // -----------------------------------

  __ AssertFixedArray(r4);
  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(ip, Heap::kRealStackLimitRootIndex);
    // Make ip the space we have left. The stack might already be overflowed
    // here which will cause ip to become negative.
    __ SubP(ip, sp, ip);
    // Check if the arguments will overflow the stack.
    __ ShiftLeftP(r0, r6, Operand(kPointerSizeLog2));
    __ CmpP(ip, r0);  // Signed comparison.
    __ bgt(&done);
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    Label loop, no_args, skip;
    __ CmpP(r6, Operand::Zero());
    __ beq(&no_args);
    __ AddP(r4, r4,
            Operand(FixedArray::kHeaderSize - kHeapObjectTag - kPointerSize));
    __ LoadRR(r1, r6);
    __ bind(&loop);
    __ LoadP(ip, MemOperand(r4, kPointerSize));
    __ la(r4, MemOperand(r4, kPointerSize));
    __ CompareRoot(ip, Heap::kTheHoleValueRootIndex);
    __ bne(&skip, Label::kNear);
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ bind(&skip);
    __ push(ip);
    __ BranchOnCount(r1, &loop);
    __ bind(&no_args);
    __ AddP(r2, r2, r6);
  }

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r5 : the new.target (for [[Construct]] calls)
  //  -- r3 : the target to call (can be any Object)
  //  -- r4 : start index (to support rest parameters)
  // -----------------------------------

  Register scratch = r8;

  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(r5, &new_target_not_constructor);
    __ LoadP(scratch, FieldMemOperand(r5, HeapObject::kMapOffset));
    __ LoadlB(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ tmll(scratch, Operand(Map::IsConstructorBit::kShift));
    __ bne(&new_target_constructor);
    __ bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(r5);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ bind(&new_target_constructor);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ LoadP(r6, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(ip, MemOperand(r6, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ CmpP(ip, Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ beq(&arguments_adaptor);
  {
    __ LoadP(r7, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    __ LoadP(r7, FieldMemOperand(r7, JSFunction::kSharedFunctionInfoOffset));
    __ LoadW(r7, FieldMemOperand(
                     r7, SharedFunctionInfo::kFormalParameterCountOffset));
    __ LoadRR(r6, fp);
  }
  __ b(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    // Load the length from the ArgumentsAdaptorFrame.
    __ LoadP(r7, MemOperand(r6, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ SmiUntag(r7);
  }
  __ bind(&arguments_done);

  Label stack_done, stack_overflow;
  __ SubP(r7, r7, r4);
  __ CmpP(r7, Operand::Zero());
  __ ble(&stack_done);
  {
    // Check for stack overflow.
    Generate_StackOverflowCheck(masm, r7, r4, &stack_overflow);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ AddP(r6, r6, Operand(kPointerSize));
      __ AddP(r2, r2, r7);
      __ bind(&loop);
      {
        __ ShiftLeftP(ip, r7, Operand(kPointerSizeLog2));
        __ LoadP(ip, MemOperand(r6, ip));
        __ push(ip);
        __ SubP(r7, r7, Operand(1));
        __ CmpP(r7, Operand::Zero());
        __ bne(&loop);
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
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(r3);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ LoadP(r4, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadlW(r5, FieldMemOperand(r4, SharedFunctionInfo::kCompilerHintsOffset));
  __ TestBitMask(r5, SharedFunctionInfo::kClassConstructorMask, r0);
  __ bne(&class_constructor);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ AndP(r0, r5,
          Operand(SharedFunctionInfo::IsStrictBit::kMask |
                  SharedFunctionInfo::IsNativeBit::kMask));
  __ bne(&done_convert);
  {
    // ----------- S t a t e -------------
    //  -- r2 : the number of arguments (not including the receiver)
    //  -- r3 : the function to call (checked to be a JSFunction)
    //  -- r4 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(r5);
    } else {
      Label convert_to_object, convert_receiver;
      __ ShiftLeftP(r5, r2, Operand(kPointerSizeLog2));
      __ LoadP(r5, MemOperand(sp, r5));
      __ JumpIfSmi(r5, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CompareObjectType(r5, r6, r6, FIRST_JS_RECEIVER_TYPE);
      __ bge(&done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(r5, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy);
        __ JumpIfNotRoot(r5, Heap::kNullValueRootIndex, &convert_to_object);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(r5);
        }
        __ b(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(r2);
        __ Push(r2, r3);
        __ LoadRR(r2, r5);
        __ Push(cp);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ LoadRR(r5, r2);
        __ Pop(r2, r3);
        __ SmiUntag(r2);
      }
      __ LoadP(r4, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ ShiftLeftP(r6, r2, Operand(kPointerSizeLog2));
    __ StoreP(r5, MemOperand(sp, r6));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the function to call (checked to be a JSFunction)
  //  -- r4 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ LoadW(
      r4, FieldMemOperand(r4, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(r2);
  ParameterCount expected(r4);
  __ InvokeFunctionCode(r3, no_reg, expected, actual, JUMP_FUNCTION);

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameAndConstantPoolScope frame(masm, StackFrame::INTERNAL);
    __ push(r3);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : target (checked to be a JSBoundFunction)
  //  -- r5 : new.target (only in case of [[Construct]])
  // -----------------------------------

  // Load [[BoundArguments]] into r4 and length of that into r6.
  Label no_bound_arguments;
  __ LoadP(r4, FieldMemOperand(r3, JSBoundFunction::kBoundArgumentsOffset));
  __ LoadP(r6, FieldMemOperand(r4, FixedArray::kLengthOffset));
  __ SmiUntag(r6);
  __ LoadAndTestP(r6, r6);
  __ beq(&no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- r2 : the number of arguments (not including the receiver)
    //  -- r3 : target (checked to be a JSBoundFunction)
    //  -- r4 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- r5 : new.target (only in case of [[Construct]])
    //  -- r6 : the number of [[BoundArguments]]
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ LoadRR(r8, sp);  // preserve previous stack pointer
      __ ShiftLeftP(r9, r6, Operand(kPointerSizeLog2));
      __ SubP(sp, sp, r9);
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRoot(sp, Heap::kRealStackLimitRootIndex);
      __ bgt(&done);  // Signed comparison.
      // Restore the stack pointer.
      __ LoadRR(sp, r8);
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Relocate arguments down the stack.
    //  -- r2 : the number of arguments (not including the receiver)
    //  -- r8 : the previous stack pointer
    //  -- r9: the size of the [[BoundArguments]]
    {
      Label skip, loop;
      __ LoadImmP(r7, Operand::Zero());
      __ CmpP(r2, Operand::Zero());
      __ beq(&skip);
      __ LoadRR(r1, r2);
      __ bind(&loop);
      __ LoadP(r0, MemOperand(r8, r7));
      __ StoreP(r0, MemOperand(sp, r7));
      __ AddP(r7, r7, Operand(kPointerSize));
      __ BranchOnCount(r1, &loop);
      __ bind(&skip);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ AddP(r4, r4, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
      __ AddP(r4, r4, r9);
      __ LoadRR(r1, r6);
      __ bind(&loop);
      __ LoadP(r0, MemOperand(r4, -kPointerSize));
      __ lay(r4, MemOperand(r4, -kPointerSize));
      __ StoreP(r0, MemOperand(sp, r7));
      __ AddP(r7, r7, Operand(kPointerSize));
      __ BranchOnCount(r1, &loop);
      __ AddP(r2, r2, r6);
    }
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(r3);

  // Patch the receiver to [[BoundThis]].
  __ LoadP(ip, FieldMemOperand(r3, JSBoundFunction::kBoundThisOffset));
  __ ShiftLeftP(r1, r2, Operand(kPointerSizeLog2));
  __ StoreP(ip, MemOperand(sp, r1));

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ LoadP(r3,
           FieldMemOperand(r3, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(r3, &non_callable);
  __ bind(&non_smi);
  __ CompareObjectType(r3, r6, r7, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, eq);
  __ CmpP(r7, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  __ LoadlB(r6, FieldMemOperand(r6, Map::kBitFieldOffset));
  __ TestBit(r6, Map::IsCallableBit::kShift);
  __ beq(&non_callable);

  // Check if target is a proxy and call CallProxy external builtin
  __ CmpP(r7, Operand(JS_PROXY_TYPE));
  __ bne(&non_function);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver the (original) target.
  __ ShiftLeftP(r7, r2, Operand(kPointerSizeLog2));
  __ StoreP(r3, MemOperand(sp, r7));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, r3);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r3);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the constructor to call (checked to be a JSFunction)
  //  -- r5 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertFunction(r3);

  // Calling convention for function specific ConstructStubs require
  // r4 to contain either an AllocationSite or undefined.
  __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ LoadP(r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r6, FieldMemOperand(r6, SharedFunctionInfo::kConstructStubOffset));
  __ AddP(ip, r6, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the function to call (checked to be a JSBoundFunction)
  //  -- r5 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertBoundFunction(r3);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  Label skip;
  __ CmpP(r3, r5);
  __ bne(&skip);
  __ LoadP(r5,
           FieldMemOperand(r3, JSBoundFunction::kBoundTargetFunctionOffset));
  __ bind(&skip);

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ LoadP(r3,
           FieldMemOperand(r3, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the constructor to call (can be any Object)
  //  -- r5 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor, non_proxy;
  __ JumpIfSmi(r3, &non_constructor);

  // Dispatch based on instance type.
  __ CompareObjectType(r3, r6, r7, JS_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Construct]] internal method.
  __ LoadlB(r4, FieldMemOperand(r6, Map::kBitFieldOffset));
  __ TestBit(r4, Map::IsConstructorBit::kShift);
  __ beq(&non_constructor);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ CmpP(r7, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to proxies after checking whether they are constructors.
  __ CmpP(r7, Operand(JS_PROXY_TYPE));
  __ bne(&non_proxy);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
  {
    // Overwrite the original receiver with the (original) target.
    __ ShiftLeftP(r7, r2, Operand(kPointerSizeLog2));
    __ StoreP(r3, MemOperand(sp, r7));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, r3);
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
  //  -- r3 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r3);
  __ Push(r3);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r3);
  __ LoadSmiLiteral(r4, Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ Push(r3, r4);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

// static
void Builtins::Generate_Abort(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : message_id as Smi
  //  -- lr : return address
  // -----------------------------------
  __ push(r3);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbort);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : actual number of arguments
  //  -- r3 : function (passed through to callee)
  //  -- r4 : expected number of arguments
  //  -- r5 : new target (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ LoadP(ip, FieldMemOperand(r3, JSFunction::kCodeOffset));
  __ AddP(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ CmpP(r2, r4);
  __ blt(&too_few);
  __ CmpP(r4, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ beq(&dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r4, r7, &stack_overflow);

    // Calculate copy start address into r2 and copy end address into r6.
    // r2: actual number of arguments as a smi
    // r3: function
    // r4: expected number of arguments
    // r5: new target (passed through to callee)
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r2, r2);
    __ AddP(r2, fp);
    // adjust for return address and receiver
    __ AddP(r2, r2, Operand(2 * kPointerSize));
    __ ShiftLeftP(r6, r4, Operand(kPointerSizeLog2));
    __ SubP(r6, r2, r6);

    // Copy the arguments (including the receiver) to the new stack frame.
    // r2: copy start address
    // r3: function
    // r4: expected number of arguments
    // r5: new target (passed through to callee)
    // r6: copy end address
    // ip: code entry to call

    Label copy;
    __ bind(&copy);
    __ LoadP(r0, MemOperand(r2, 0));
    __ push(r0);
    __ CmpP(r2, r6);  // Compare before moving to next argument.
    __ lay(r2, MemOperand(r2, -kPointerSize));
    __ bne(&copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);

    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r4, r7, &stack_overflow);

    // Calculate copy start address into r0 and copy end address is fp.
    // r2: actual number of arguments as a smi
    // r3: function
    // r4: expected number of arguments
    // r5: new target (passed through to callee)
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r2, r2);
    __ lay(r2, MemOperand(r2, fp));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r2: copy start address
    // r3: function
    // r4: expected number of arguments
    // r5: new target (passed through to callee)
    // ip: code entry to call
    Label copy;
    __ bind(&copy);
    // Adjust load for return address and receiver.
    __ LoadP(r0, MemOperand(r2, 2 * kPointerSize));
    __ push(r0);
    __ CmpP(r2, fp);  // Compare before moving to next argument.
    __ lay(r2, MemOperand(r2, -kPointerSize));
    __ bne(&copy);

    // Fill the remaining expected arguments with undefined.
    // r3: function
    // r4: expected number of argumentus
    // ip: code entry to call
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    __ ShiftLeftP(r6, r4, Operand(kPointerSizeLog2));
    __ SubP(r6, fp, r6);
    // Adjust for frame.
    __ SubP(r6, r6,
            Operand(ArgumentsAdaptorFrameConstants::kFixedFrameSizeFromFp +
                    kPointerSize));

    Label fill;
    __ bind(&fill);
    __ push(r0);
    __ CmpP(sp, r6);
    __ bne(&fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ LoadRR(r2, r4);
  // r2 : expected number of arguments
  // r3 : function (passed through to callee)
  // r5 : new target (passed through to callee)
  __ CallJSEntry(ip);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ JumpToJSEntry(ip);

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
    constexpr RegList gp_regs = Register::ListOf<r2, r3, r4, r5, r6>();
#if V8_TARGET_ARCH_S390X
    constexpr RegList fp_regs = DoubleRegister::ListOf<d0, d2, d4, d6>();
#else
    constexpr RegList fp_regs = DoubleRegister::ListOf<d0, d2>();
#endif
    __ MultiPush(gp_regs);
    __ MultiPushDoubles(fp_regs);

    // Initialize cp register with kZero, CEntryStub will use it to set the
    // current context on the isolate.
    __ LoadSmiLiteral(cp, Smi::kZero);
    __ CallRuntime(Runtime::kWasmCompileLazy);
    // Store returned instruction start in ip.
    __ AddP(ip, r2, Operand(Code::kHeaderSize - kHeapObjectTag));

    // Restore registers.
    __ MultiPopDoubles(fp_regs);
    __ MultiPop(gp_regs);
  }
  // Now jump to the instructions of the returned code object.
  __ Jump(ip);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390

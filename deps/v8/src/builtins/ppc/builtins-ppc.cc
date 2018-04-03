// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

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
  __ mov(r15, Operand(ExternalReference(address, masm->isolate())));
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
  //  -- r3                 : number of arguments excluding receiver
  //  -- r4                 : target
  //  -- r6                 : new.target
  //  -- r15                : entry point
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------
  __ AssertFunction(r4);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  // CEntryStub expects r3 to contain the number of arguments including the
  // receiver and the extra arguments.
  __ addi(r3, r3,
          Operand(BuiltinExitFrameConstants::kNumExtraArgsWithReceiver));

  // Insert extra arguments.
  __ PushRoot(Heap::kTheHoleValueRootIndex);  // Padding.
  __ SmiTag(r3);
  __ Push(r3, r4, r6);
  __ SmiUntag(r3);

  // Jump to the C entry runtime stub directly here instead of using
  // JumpToExternalReference. We have already loaded entry point to r15
  // in Generate_adaptor.
  __ mr(r4, r15);
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
  //  -- r3     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, r4);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ LoadP(r5, FieldMemOperand(r4, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r5, r0);
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForInternalArrayFunction,
              cr0);
    __ CompareObjectType(r5, r6, r7, MAP_TYPE);
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
  //  -- r3     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, r4);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ LoadP(r5, FieldMemOperand(r4, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r5, r0);
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r5, r6, r7, MAP_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedInitialMapForArrayFunction);
  }

  __ mr(r6, r4);
  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ LoadP(ip, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(ip, FieldMemOperand(ip, SharedFunctionInfo::kCodeOffset));
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- r3 : argument count (preserved for callee)
  //  -- r4 : target function (preserved for callee)
  //  -- r6 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Push the number of arguments to the callee.
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ SmiTag(r3);
    __ Push(r3, r4, r6, r4);

    __ CallRuntime(function_id, 1);
    __ mr(r5, r3);

    // Restore target function and new target.
    __ Pop(r3, r4, r6);
    __ SmiUntag(r3);
  }
  __ addi(ip, r5, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  Label post_instantiation_deopt_entry;
  // ----------- S t a t e -------------
  //  -- r3     : number of arguments
  //  -- r4     : constructor function
  //  -- r6     : new target
  //  -- cp     : context
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.

    __ SmiTag(r3);
    __ Push(cp, r3);
    __ SmiUntag(r3, SetRC);
    // The receiver for the builtin/api call.
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    // Set up pointer to last argument.
    __ addi(r7, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.

    Label loop, no_args;
    // ----------- S t a t e -------------
    //  --                 r3: number of arguments (untagged)
    //  --                 r4: constructor function
    //  --                 r6: new target
    //  --                 r7: pointer to last argument
    //  --                 cr0: condition indicating whether r3 is zero
    //  -- sp[0*kPointerSize]: the hole (receiver)
    //  -- sp[1*kPointerSize]: number of arguments (tagged)
    //  -- sp[2*kPointerSize]: context
    // -----------------------------------
    __ beq(&no_args, cr0);
    __ ShiftLeftImm(ip, r3, Operand(kPointerSizeLog2));
    __ sub(sp, sp, ip);
    __ mtctr(r3);
    __ bind(&loop);
    __ subi(ip, ip, Operand(kPointerSize));
    __ LoadPX(r0, MemOperand(r7, ip));
    __ StorePX(r0, MemOperand(sp, ip));
    __ bdnz(&loop);
    __ bind(&no_args);

    // Call the function.
    // r3: number of arguments (untagged)
    // r4: constructor function
    // r6: new target
    {
      ConstantPoolUnavailableScope constant_pool_unavailable(masm);
      ParameterCount actual(r3);
      __ InvokeFunction(r4, r6, actual, CALL_FUNCTION);
    }

    // Restore context from the frame.
    __ LoadP(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ LoadP(r4, MemOperand(fp, ConstructFrameConstants::kLengthOffset));

    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);

  __ SmiToPtrArrayOffset(r4, r4);
  __ add(sp, sp, r4);
  __ addi(sp, sp, Operand(kPointerSize));
  __ blr();
}

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Generate_JSConstructStubGeneric(MacroAssembler* masm,
                                     bool restrict_constructor_return) {
  // ----------- S t a t e -------------
  //  --      r3: number of arguments (untagged)
  //  --      r4: constructor function
  //  --      r6: new target
  //  --      cp: context
  //  --      lr: return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ SmiTag(r3);
    __ Push(cp, r3, r4);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ Push(r6);

    // ----------- S t a t e -------------
    //  --        sp[0*kPointerSize]: new target
    //  --        sp[1*kPointerSize]: padding
    //  -- r4 and sp[2*kPointerSize]: constructor function
    //  --        sp[3*kPointerSize]: number of arguments (tagged)
    //  --        sp[4*kPointerSize]: context
    // -----------------------------------

    __ LoadP(r7, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ lwz(r7, FieldMemOperand(r7, SharedFunctionInfo::kCompilerHintsOffset));
    __ TestBitMask(r7, SharedFunctionInfo::kDerivedConstructorMask, r0);
    __ bne(&not_create_implicit_receiver, cr0);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        r7, r8);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ b(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(r3, Heap::kTheHoleValueRootIndex);

    // ----------- S t a t e -------------
    //  --                          r3: receiver
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
    __ Pop(r6);
    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(r3, r3);

    // ----------- S t a t e -------------
    //  --                 r6: new target
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: implicit receiver
    //  -- sp[2*kPointerSize]: padding
    //  -- sp[3*kPointerSize]: constructor function
    //  -- sp[4*kPointerSize]: number of arguments (tagged)
    //  -- sp[5*kPointerSize]: context
    // -----------------------------------

    // Restore constructor function and argument count.
    __ LoadP(r4, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ LoadP(r3, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    __ SmiUntag(r3, SetRC);

    // Set up pointer to last argument.
    __ addi(r7, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, no_args;
    // ----------- S t a t e -------------
    //  --                        r3: number of arguments (untagged)
    //  --                        r6: new target
    //  --                        r7: pointer to last argument
    //  --                        cr0: condition indicating whether r3 is zero
    //  --        sp[0*kPointerSize]: implicit receiver
    //  --        sp[1*kPointerSize]: implicit receiver
    //  --        sp[2*kPointerSize]: padding
    //  -- r4 and sp[3*kPointerSize]: constructor function
    //  --        sp[4*kPointerSize]: number of arguments (tagged)
    //  --        sp[5*kPointerSize]: context
    // -----------------------------------
    __ beq(&no_args, cr0);
    __ ShiftLeftImm(ip, r3, Operand(kPointerSizeLog2));
    __ sub(sp, sp, ip);
    __ mtctr(r3);
    __ bind(&loop);
    __ subi(ip, ip, Operand(kPointerSize));
    __ LoadPX(r0, MemOperand(r7, ip));
    __ StorePX(r0, MemOperand(sp, ip));
    __ bdnz(&loop);
    __ bind(&no_args);

    // Call the function.
    {
      ConstantPoolUnavailableScope constant_pool_unavailable(masm);
      ParameterCount actual(r3);
      __ InvokeFunction(r4, r6, actual, CALL_FUNCTION);
    }

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
    __ JumpIfRoot(r3, Heap::kUndefinedValueRootIndex, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(r3, &other_result);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(r3, r7, r7, FIRST_JS_RECEIVER_TYPE);
    __ bge(&leave_frame);

    __ bind(&other_result);
    // The result is now neither undefined nor an object.
    if (restrict_constructor_return) {
      // Throw if constructor function is a class constructor
      __ LoadP(r7, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
      __ LoadP(r7, FieldMemOperand(r7, JSFunction::kSharedFunctionInfoOffset));
      __ lwz(r7, FieldMemOperand(r7, SharedFunctionInfo::kCompilerHintsOffset));
      __ TestBitMask(r7, SharedFunctionInfo::kClassConstructorMask, r0);
      __ beq(&use_receiver, cr0);

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
    __ LoadP(r3, MemOperand(sp));
    __ JumpIfRoot(r3, Heap::kTheHoleValueRootIndex, &do_throw);

    __ bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ LoadP(r4, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);

  __ SmiToPtrArrayOffset(r4, r4);
  __ add(sp, sp, r4);
  __ addi(sp, sp, Operand(kPointerSize));
  __ blr();
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
  //  -- r3 : the value to pass to the generator
  //  -- r4 : the JSGeneratorObject to resume
  //  -- lr : return address
  // -----------------------------------
  __ AssertGeneratorObject(r4);

  // Store input value into generator object.
  __ StoreP(r3, FieldMemOperand(r4, JSGeneratorObject::kInputOrDebugPosOffset),
            r0);
  __ RecordWriteField(r4, JSGeneratorObject::kInputOrDebugPosOffset, r3, r6,
                      kLRHasNotBeenSaved, kDontSaveFPRegs);

  // Load suspended function and context.
  __ LoadP(r7, FieldMemOperand(r4, JSGeneratorObject::kFunctionOffset));
  __ LoadP(cp, FieldMemOperand(r7, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ mov(ip, Operand(debug_hook));
  __ LoadByte(ip, MemOperand(ip), r0);
  __ extsb(ip, ip);
  __ CmpSmiLiteral(ip, Smi::kZero, r0);
  __ bne(&prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.

  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());

  __ mov(ip, Operand(debug_suspended_generator));
  __ LoadP(ip, MemOperand(ip));
  __ cmp(ip, r4);
  __ beq(&prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ CompareRoot(sp, Heap::kRealStackLimitRootIndex);
  __ blt(&stack_overflow);

  // Push receiver.
  __ LoadP(ip, FieldMemOperand(r4, JSGeneratorObject::kReceiverOffset));
  __ Push(ip);

  // ----------- S t a t e -------------
  //  -- r4    : the JSGeneratorObject to resume
  //  -- r7    : generator function
  //  -- cp    : generator context
  //  -- lr    : return address
  //  -- sp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ LoadP(r6, FieldMemOperand(r7, JSFunction::kSharedFunctionInfoOffset));
  __ LoadWordArith(
      r3, FieldMemOperand(r6, SharedFunctionInfo::kFormalParameterCountOffset));
  {
    Label loop, done_loop;
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmpi(r3, Operand::Zero());
    __ beq(&done_loop);
    __ mtctr(r3);
    __ bind(&loop);
    __ push(ip);
    __ bdnz(&loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ LoadP(r6, FieldMemOperand(r6, SharedFunctionInfo::kFunctionDataOffset));
    __ CompareObjectType(r6, r6, r6, BYTECODE_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ mr(r6, r4);
    __ mr(r4, r7);
    __ LoadP(ip, FieldMemOperand(r4, JSFunction::kCodeOffset));
    __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ JumpToJSEntry(ip);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4, r7);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(r4);
    __ LoadP(r7, FieldMemOperand(r4, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(r4);
    __ LoadP(r7, FieldMemOperand(r4, JSGeneratorObject::kFunctionOffset));
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
  __ push(r4);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

// Clobbers r5; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(r5, Heap::kRealStackLimitRootIndex);
  // Make r5 the space we have left. The stack might already be overflowed
  // here which will cause r5 to become negative.
  __ sub(r5, sp, r5);
  // Check if the arguments will overflow the stack.
  __ ShiftLeftImm(r0, argc, Operand(kPointerSizeLog2));
  __ cmp(r5, r0);
  __ bgt(&okay);  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r3: new.target
  // r4: function
  // r5: receiver
  // r6: argc
  // r7: argv
  // r0,r8-r9, cp may be clobbered
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(IsolateAddressId::kContextAddress,
                                      masm->isolate());
    __ mov(cp, Operand(context_address));
    __ LoadP(cp, MemOperand(cp));

    // Push the function and the receiver onto the stack.
    __ Push(r4, r5);

    // Check if we have enough stack space to push all arguments.
    // Clobbers r5.
    Generate_CheckStackOverflow(masm, r6);

    // Copy arguments to the stack in a loop.
    // r4: function
    // r6: argc
    // r7: argv, i.e. points to first arg
    Label loop, entry;
    __ ShiftLeftImm(r0, r6, Operand(kPointerSizeLog2));
    __ add(r5, r7, r0);
    // r5 points past last arg.
    __ b(&entry);
    __ bind(&loop);
    __ LoadP(r8, MemOperand(r7));  // read next parameter
    __ addi(r7, r7, Operand(kPointerSize));
    __ LoadP(r0, MemOperand(r8));  // dereference handle
    __ push(r0);                   // push parameter
    __ bind(&entry);
    __ cmp(r7, r5);
    __ bne(&loop);

    // Setup new.target and argc.
    __ mr(r7, r3);
    __ mr(r3, r6);
    __ mr(r6, r7);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r7, Heap::kUndefinedValueRootIndex);
    __ mr(r14, r7);
    __ mr(r15, r7);
    __ mr(r16, r7);
    __ mr(r17, r7);

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the JS frame and remove the parameters (except function), and
    // return.
  }
  __ blr();

  // r3: result
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
  __ mr(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_count = scratch;

  // Get the arguments + receiver count.
  __ LoadP(args_count,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ lwz(args_count,
         FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  __ add(sp, sp, args_count);
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
      !AreAliased(feedback_vector, r3, r4, r6, scratch1, scratch2, scratch3));

  Label optimized_code_slot_is_cell, fallthrough;

  Register closure = r4;
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
      __ b(&fallthrough);
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
    __ LoadWordArith(
        scratch2,
        FieldMemOperand(scratch2, CodeDataContainer::kKindSpecificFlagsOffset));
    __ TestBit(scratch2, Code::kMarkedForDeoptimizationBit, r0);
    __ bne(&found_deoptimized_code, cr0);

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    // The feedback vector is no longer used, so re-use it as a scratch
    // register.
    ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                        scratch2, scratch3, feedback_vector);
    __ addi(optimized_code_entry, optimized_code_entry,
            Operand(Code::kHeaderSize - kHeapObjectTag));
    __ mr(ip, optimized_code_entry);
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
  __ cmpi(bytecode, Operand(0x1));
  __ bgt(&load_size);
  __ beq(&extra_wide);

  // Load the next bytecode and update table to the wide scaled table.
  __ addi(bytecode_offset, bytecode_offset, Operand(1));
  __ lbzx(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ addi(bytecode_size_table, bytecode_size_table,
          Operand(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ b(&load_size);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ addi(bytecode_offset, bytecode_offset, Operand(1));
  __ lbzx(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ addi(bytecode_size_table, bytecode_size_table,
          Operand(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));

  // Load the size of the current bytecode.
  __ bind(&load_size);
  __ ShiftLeftImm(scratch2, bytecode, Operand(2));
  __ lwzx(scratch2, MemOperand(bytecode_size_table, scratch2));
  __ add(bytecode_offset, bytecode_offset, scratch2);
}
// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o r4: the JS function object being called.
//   o r6: the incoming new target or generator object
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

  Register closure = r4;
  Register feedback_vector = r5;

  // Load the feedback vector from the closure.
  __ LoadP(feedback_vector,
           FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ LoadP(feedback_vector,
           FieldMemOperand(feedback_vector, Cell::kValueOffset));
  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, r7, r9, r8);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(closure);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  Label maybe_load_debug_bytecode_array, bytecode_array_loaded;
  __ LoadP(r3, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Load original bytecode array or the debug copy.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           FieldMemOperand(r3, SharedFunctionInfo::kFunctionDataOffset));
  __ LoadP(r7, FieldMemOperand(r3, SharedFunctionInfo::kDebugInfoOffset));
  __ TestIfSmi(r7, r0);
  __ bne(&maybe_load_debug_bytecode_array, cr0);
  __ bind(&bytecode_array_loaded);

  // Increment invocation count for the function.
  __ LoadWord(
      r8,
      FieldMemOperand(feedback_vector, FeedbackVector::kInvocationCountOffset),
      r0);
  __ addi(r8, r8, Operand(1));
  __ StoreWord(
      r8,
      FieldMemOperand(feedback_vector, FeedbackVector::kInvocationCountOffset),
      r0);

  // Check function data field is actually a BytecodeArray object.

  if (FLAG_debug_code) {
    __ TestIfSmi(kInterpreterBytecodeArrayRegister, r0);
    __ Assert(ne,
              AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry,
              cr0);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r3, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(
        eq, AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Reset code age.
  __ mov(r8, Operand(BytecodeArray::kNoAgeBytecodeAge));
  __ StoreByte(r8, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                   BytecodeArray::kBytecodeAgeOffset),
               r0);

  // Load initial bytecode offset.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(r3, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, r3);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size (word) from the BytecodeArray object.
    __ lwz(r5, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                               BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ sub(r8, sp, r5);
    __ LoadRoot(r0, Heap::kRealStackLimitRootIndex);
    __ cmpl(r8, r0);
    __ bge(&ok);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    Label loop, no_args;
    __ LoadRoot(r8, Heap::kUndefinedValueRootIndex);
    __ ShiftRightImm(r5, r5, Operand(kPointerSizeLog2), SetRC);
    __ beq(&no_args, cr0);
    __ mtctr(r5);
    __ bind(&loop);
    __ push(r8);
    __ bdnz(&loop);
    __ bind(&no_args);
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in r6.
  Label no_incoming_new_target_or_generator_register;
  __ LoadWordArith(
      r8, FieldMemOperand(
              kInterpreterBytecodeArrayRegister,
              BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ cmpi(r8, Operand::Zero());
  __ beq(&no_incoming_new_target_or_generator_register);
  __ ShiftLeftImm(r8, r8, Operand(kPointerSizeLog2));
  __ StorePX(r6, MemOperand(fp, r8));
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
  __ lbzx(r4, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftImm(ip, r4, Operand(kPointerSizeLog2));
  __ LoadPX(ip, MemOperand(kInterpreterDispatchTableRegister, ip));
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
  __ lbzx(r4, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ cmpi(r4, Operand(static_cast<int>(interpreter::Bytecode::kReturn)));
  __ beq(&do_return);

  // Advance to the next bytecode and dispatch.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, r4, r5);
  __ b(&do_dispatch);

  __ bind(&do_return);
  // The return value is in r3.
  LeaveInterpreterFrame(masm, r5);
  __ blr();

  // Load debug copy of the bytecode array if it exists.
  // kInterpreterBytecodeArrayRegister is already loaded with
  // SharedFunctionInfo::kFunctionDataOffset.
  Label done;
  __ bind(&maybe_load_debug_bytecode_array);
  __ LoadP(ip, FieldMemOperand(r7, DebugInfo::kFlagsOffset));
  __ SmiUntag(ip);
  __ andi(r0, ip, Operand(DebugInfo::kHasBreakInfo));
  __ beq(&done, cr0);
  __ LoadP(kInterpreterBytecodeArrayRegister,
           FieldMemOperand(r7, DebugInfo::kDebugBytecodeArrayOffset));
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
  __ sub(scratch, sp, scratch);
  // Check if the arguments will overflow the stack.
  __ ShiftLeftImm(r0, num_args, Operand(kPointerSizeLog2));
  __ cmp(scratch, r0);
  __ ble(stack_overflow);  // Signed comparison.
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args, Register index,
                                         Register count, Register scratch) {
  Label loop, skip;
  __ cmpi(count, Operand::Zero());
  __ beq(&skip);
  __ addi(index, index, Operand(kPointerSize));  // Bias up for LoadPU
  __ mtctr(count);
  __ bind(&loop);
  __ LoadPU(scratch, MemOperand(index, -kPointerSize));
  __ push(scratch);
  __ bdnz(&loop);
  __ bind(&skip);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r5 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- r4 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  // Calculate number of arguments (add one for receiver).
  __ addi(r6, r3, Operand(1));

  Generate_StackOverflowCheck(masm, r6, ip, &stack_overflow);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ mr(r6, r3);  // Argument count is correct.
  }

  // Push the arguments. r5, r6, r7 will be modified.
  Generate_InterpreterPushArgs(masm, r6, r5, r6, r7);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(r5);                   // Pass the spread in a register
    __ subi(r3, r3, Operand(1));  // Subtract one for spread
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
  // -- r3 : argument count (not including receiver)
  // -- r6 : new target
  // -- r4 : constructor to call
  // -- r5 : allocation site feedback if available, undefined otherwise.
  // -- r7 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver to be constructed.
  __ li(r0, Operand::Zero());
  __ push(r0);

  // Push the arguments (skip if none).
  Label skip;
  __ cmpi(r3, Operand::Zero());
  __ beq(&skip);
  Generate_StackOverflowCheck(masm, r3, ip, &stack_overflow);
  // Push the arguments. r8, r7, r9 will be modified.
  Generate_InterpreterPushArgs(masm, r3, r7, r3, r9);
  __ bind(&skip);
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(r5);                   // Pass the spread in a register
    __ subi(r3, r3, Operand(1));  // Subtract one for spread
  } else {
    __ AssertUndefinedOrAllocationSite(r5, r8);
  }
  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ AssertFunction(r4);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ LoadP(r7, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ LoadP(r7, FieldMemOperand(r7, SharedFunctionInfo::kConstructStubOffset));
    // Jump to the construct function.
    __ addi(ip, r7, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(ip);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with r3, r4, and r6 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with r3, r4, and r6 unmodified.
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
  __ Move(r5, BUILTIN_CODE(masm->isolate(), InterpreterEntryTrampoline));
  __ addi(r0, r5, Operand(interpreter_entry_return_pc_offset->value() +
                          Code::kHeaderSize - kHeapObjectTag));
  __ mtlr(r0);

  // Initialize the dispatch table register.
  __ mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ TestIfSmi(kInterpreterBytecodeArrayRegister, r0);
    __ Assert(ne,
              AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry,
              cr0);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r4, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(
        eq, AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ LoadP(kInterpreterBytecodeOffsetRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ lbzx(r4, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftImm(ip, r4, Operand(kPointerSizeLog2));
  __ LoadPX(ip, MemOperand(kInterpreterDispatchTableRegister, ip));
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
  __ lbzx(r4, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));

  // Advance to the next bytecode.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, r4, r5);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(r5, kInterpreterBytecodeOffsetRegister);
  __ StoreP(r5,
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
  Register closure = r4;

  // Get the feedback vector.
  Register feedback_vector = r5;
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
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, r7, r9, r8);

  // Otherwise, tail call the SFI code.
  GenerateTailCallToSharedCode(masm);
}

void Builtins::Generate_CompileLazyDeoptimizedCode(MacroAssembler* masm) {
  // Set the code slot inside the JSFunction to the trampoline to the
  // interpreter entry.
  __ LoadP(r5, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r5, FieldMemOperand(r5, SharedFunctionInfo::kCodeOffset));
  __ StoreP(r5, FieldMemOperand(r4, JSFunction::kCodeOffset), r0);
  __ RecordWriteField(r4, JSFunction::kCodeOffset, r5, r7, kLRHasNotBeenSaved,
                      kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  // Jump to compile lazy.
  Generate_CompileLazy(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : argument count (preserved for callee)
  //  -- r6 : new target (preserved for callee)
  //  -- r4 : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;

  Register closure = r4;
  Register feedback_vector = r5;

  // Do we have a valid feedback vector?
  __ LoadP(feedback_vector,
           FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ LoadP(feedback_vector,
           FieldMemOperand(feedback_vector, Cell::kValueOffset));
  __ JumpIfRoot(feedback_vector, Heap::kUndefinedValueRootIndex,
                &gotta_call_runtime);

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, r7, r9, r8);

  // We found no optimized code.
  Register entry = r7;
  __ LoadP(entry,
           FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));

  // If SFI points to anything other than CompileLazy, install that.
  __ LoadP(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ mov(r8, Operand(masm->CodeObject()));
  __ cmp(entry, r8);
  __ beq(&gotta_call_runtime);

  // Install the SFI's code entry.
  __ StoreP(entry, FieldMemOperand(closure, JSFunction::kCodeOffset), r0);
  __ mr(r9, entry);  // Write barrier clobbers ip below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, r9, r8,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ addi(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(entry);

  __ bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

// Lazy deserialization design doc: http://goo.gl/dxkYDZ.
void Builtins::Generate_DeserializeLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : argument count (preserved for callee)
  //  -- r6 : new target (preserved for callee)
  //  -- r4 : target function (preserved for callee)
  // -----------------------------------

  Label deserialize_in_runtime;

  Register target = r4;  // Must be preserved
  Register scratch0 = r5;
  Register scratch1 = r7;

  CHECK(scratch0 != r3 && scratch0 != r6 && scratch0 != r4);
  CHECK(scratch1 != r3 && scratch1 != r6 && scratch1 != r4);
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
    __ ShiftLeftImm(scratch1, scratch1, Operand(kPointerSizeLog2));
    __ LoadPX(scratch1, MemOperand(scratch0, scratch1));

    // Check if the loaded code object has already been deserialized. This is
    // the case iff it does not equal DeserializeLazy.

    __ Move(scratch0, masm->CodeObject());
    __ cmp(scratch1, scratch0);
    __ beq(&deserialize_in_runtime);
  }
  {
    // If we've reached this spot, the target builtin has been deserialized and
    // we simply need to copy it over. First to the shared function info.

    Register target_builtin = scratch1;
    Register shared = scratch0;

    __ LoadP(shared,
             FieldMemOperand(target, JSFunction::kSharedFunctionInfoOffset));

    CHECK(r8 != target && r8 != scratch0 && r8 != scratch1);
    CHECK(r9 != target && r9 != scratch0 && r9 != scratch1);

    __ StoreP(target_builtin,
              FieldMemOperand(shared, SharedFunctionInfo::kCodeOffset), r0);
    __ mr(r9, target_builtin);  // Write barrier clobbers r9 below.
    __ RecordWriteField(shared, SharedFunctionInfo::kCodeOffset, r9, r8,
                        kLRHasNotBeenSaved, kDontSaveFPRegs,
                        OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // And second to the target function.

    __ StoreP(target_builtin, FieldMemOperand(target, JSFunction::kCodeOffset),
              r0);
    __ mr(r9, target_builtin);  // Write barrier clobbers r9 below.
    __ RecordWriteField(target, JSFunction::kCodeOffset, r9, r8,
                        kLRHasNotBeenSaved, kDontSaveFPRegs,
                        OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // All copying is done. Jump to the deserialized code object.

    __ addi(target_builtin, target_builtin,
            Operand(Code::kHeaderSize - kHeapObjectTag));
    __ mr(ip, target_builtin);
    __ Jump(ip);
  }

  __ bind(&deserialize_in_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kDeserializeLazy);
}

void Builtins::Generate_InstantiateAsmJs(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : argument count (preserved for callee)
  //  -- r4 : new target (preserved for callee)
  //  -- r6 : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ Move(r7, r3);
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ SmiTag(r3);
    __ Push(r3, r4, r6, r4);

    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ cmpi(r7, Operand(j));
        __ bne(&over);
      }
      for (int i = j - 1; i >= 0; --i) {
        __ LoadP(r7, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                        i * kPointerSize));
        __ push(r7);
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
    __ JumpIfSmi(r3, &failed);

    __ Drop(2);
    __ pop(r7);
    __ SmiUntag(r7);
    scope.GenerateLeaveFrame();

    __ addi(r7, r7, Operand(1));
    __ Drop(r7);
    __ Ret();

    __ bind(&failed);
    // Restore target function and new target.
    __ Pop(r3, r4, r6);
    __ SmiUntag(r3);
  }
  // On failure, tail call back to regular js by re-calling the function
  // which has be reset to the compile lazy builtin.
  __ LoadP(ip, FieldMemOperand(r4, JSFunction::kCodeOffset));
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
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
        r3, MemOperand(
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
  __ addi(sp, sp,
          Operand(BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(r0);
  __ mtlr(r0);
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
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
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), r3.code());
  __ LoadP(r3, MemOperand(sp, 0 * kPointerSize));
  __ addi(sp, sp, Operand(1 * kPointerSize));
  __ Ret();
}

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ LoadP(r3, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ LoadP(r3, MemOperand(r3, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ LoadP(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }

  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(r3);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  Label skip;
  __ CmpSmiLiteral(r3, Smi::kZero, r0);
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
  __ LoadP(r4, FieldMemOperand(r3, Code::kDeoptimizationDataOffset));

  {
    ConstantPoolUnavailableScope constant_pool_unavailable(masm);
    __ addi(r3, r3, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start

    if (FLAG_enable_embedded_constant_pool) {
      __ LoadConstantPoolPointerRegisterFromCodeTargetAddress(r3);
    }

    // Load the OSR entrypoint offset from the deoptimization data.
    // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
    __ LoadP(r4,
             FieldMemOperand(r4, FixedArray::OffsetOfElementAt(
                                     DeoptimizationData::kOsrPcOffsetIndex)));
    __ SmiUntag(r4);

    // Compute the target address = code start + osr_offset
    __ add(r0, r3, r4);

    // And "return" to the OSR entry point of the function.
    __ mtlr(r0);
    __ blr();
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
  //  -- r3    : argc
  //  -- sp[0] : argArray
  //  -- sp[4] : thisArg
  //  -- sp[8] : receiver
  // -----------------------------------

  // 1. Load receiver into r4, argArray into r5 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label skip;
    Register arg_size = r8;
    Register new_sp = r6;
    Register scratch = r7;
    __ ShiftLeftImm(arg_size, r3, Operand(kPointerSizeLog2));
    __ add(new_sp, sp, arg_size);
    __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
    __ mr(r5, scratch);
    __ LoadP(r4, MemOperand(new_sp, 0));  // receiver
    __ cmpi(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(scratch, MemOperand(new_sp, 1 * -kPointerSize));  // thisArg
    __ beq(&skip);
    __ LoadP(r5, MemOperand(new_sp, 2 * -kPointerSize));  // argArray
    __ bind(&skip);
    __ mr(sp, new_sp);
    __ StoreP(scratch, MemOperand(sp, 0));
  }

  // ----------- S t a t e -------------
  //  -- r5    : argArray
  //  -- r4    : receiver
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(r5, Heap::kNullValueRootIndex, &no_arguments);
  __ JumpIfRoot(r5, Heap::kUndefinedValueRootIndex, &no_arguments);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ li(r3, Operand::Zero());
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r3: actual number of arguments
  {
    Label done;
    __ cmpi(r3, Operand::Zero());
    __ bne(&done);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ addi(r3, r3, Operand(1));
    __ bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  // r3: actual number of arguments
  __ ShiftLeftImm(r5, r3, Operand(kPointerSizeLog2));
  __ LoadPX(r4, MemOperand(sp, r5));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // r3: actual number of arguments
  // r4: callable
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ add(r5, sp, r5);

    __ mtctr(r3);
    __ bind(&loop);
    __ LoadP(ip, MemOperand(r5, -kPointerSize));
    __ StoreP(ip, MemOperand(r5));
    __ subi(r5, r5, Operand(kPointerSize));
    __ bdnz(&loop);
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ subi(r3, r3, Operand(1));
    __ pop();
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3     : argc
  //  -- sp[0]  : argumentsList
  //  -- sp[4]  : thisArgument
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r4 (if present), argumentsList into r5 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label skip;
    Register arg_size = r8;
    Register new_sp = r6;
    Register scratch = r7;
    __ ShiftLeftImm(arg_size, r3, Operand(kPointerSizeLog2));
    __ add(new_sp, sp, arg_size);
    __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
    __ mr(scratch, r4);
    __ mr(r5, r4);
    __ cmpi(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(r4, MemOperand(new_sp, 1 * -kPointerSize));  // target
    __ beq(&skip);
    __ LoadP(scratch, MemOperand(new_sp, 2 * -kPointerSize));  // thisArgument
    __ cmpi(arg_size, Operand(2 * kPointerSize));
    __ beq(&skip);
    __ LoadP(r5, MemOperand(new_sp, 3 * -kPointerSize));  // argumentsList
    __ bind(&skip);
    __ mr(sp, new_sp);
    __ StoreP(scratch, MemOperand(sp, 0));
  }

  // ----------- S t a t e -------------
  //  -- r5    : argumentsList
  //  -- r4    : target
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
  //  -- r3     : argc
  //  -- sp[0]  : new.target (optional)
  //  -- sp[4]  : argumentsList
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r4 (if present), argumentsList into r5 (if present),
  // new.target into r6 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label skip;
    Register arg_size = r8;
    Register new_sp = r7;
    __ ShiftLeftImm(arg_size, r3, Operand(kPointerSizeLog2));
    __ add(new_sp, sp, arg_size);
    __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
    __ mr(r5, r4);
    __ mr(r6, r4);
    __ StoreP(r4, MemOperand(new_sp, 0));  // receiver (undefined)
    __ cmpi(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(r4, MemOperand(new_sp, 1 * -kPointerSize));  // target
    __ mr(r6, r4);  // new.target defaults to target
    __ beq(&skip);
    __ LoadP(r5, MemOperand(new_sp, 2 * -kPointerSize));  // argumentsList
    __ cmpi(arg_size, Operand(2 * kPointerSize));
    __ beq(&skip);
    __ LoadP(r6, MemOperand(new_sp, 3 * -kPointerSize));  // new.target
    __ bind(&skip);
    __ mr(sp, new_sp);
  }

  // ----------- S t a t e -------------
  //  -- r5    : argumentsList
  //  -- r6    : new.target
  //  -- r4    : target
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
  __ SmiTag(r3);
  __ mov(r7, Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ mflr(r0);
  __ push(r0);
  if (FLAG_enable_embedded_constant_pool) {
    __ Push(fp, kConstantPoolRegister, r7, r4, r3);
  } else {
    __ Push(fp, r7, r4, r3);
  }
  __ Push(Smi::kZero);  // Padding.
  __ addi(fp, sp,
          Operand(ArgumentsAdaptorFrameConstants::kFixedFrameSizeFromFp));
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ LoadP(r4, MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  int stack_adjustment = kPointerSize;  // adjust for receiver
  __ LeaveFrame(StackFrame::ARGUMENTS_ADAPTOR, stack_adjustment);
  __ SmiToPtrArrayOffset(r0, r4);
  __ add(sp, sp, r0);
}

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- r4 : target
  //  -- r3 : number of parameters on the stack (not including the receiver)
  //  -- r5 : arguments list (a FixedArray)
  //  -- r7 : len (number of elements to push from args)
  //  -- r6 : new.target (for [[Construct]])
  // -----------------------------------

  __ AssertFixedArray(r5);
  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(ip, Heap::kRealStackLimitRootIndex);
    // Make ip the space we have left. The stack might already be overflowed
    // here which will cause ip to become negative.
    __ sub(ip, sp, ip);
    // Check if the arguments will overflow the stack.
    __ ShiftLeftImm(r0, r7, Operand(kPointerSizeLog2));
    __ cmp(ip, r0);  // Signed comparison.
    __ bgt(&done);
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    Label loop, no_args, skip;
    __ cmpi(r7, Operand::Zero());
    __ beq(&no_args);
    __ addi(r5, r5,
            Operand(FixedArray::kHeaderSize - kHeapObjectTag - kPointerSize));
    __ mtctr(r7);
    __ bind(&loop);
    __ LoadPU(ip, MemOperand(r5, kPointerSize));
    __ CompareRoot(ip, Heap::kTheHoleValueRootIndex);
    __ bne(&skip);
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ bind(&skip);
    __ push(ip);
    __ bdnz(&loop);
    __ bind(&no_args);
    __ add(r3, r3, r7);
  }

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r6 : the new.target (for [[Construct]] calls)
  //  -- r4 : the target to call (can be any Object)
  //  -- r5 : start index (to support rest parameters)
  // -----------------------------------

  Register scratch = r9;

  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(r6, &new_target_not_constructor);
    __ LoadP(scratch, FieldMemOperand(r6, HeapObject::kMapOffset));
    __ lbz(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ TestBit(scratch, Map::IsConstructorBit::kShift, r0);
    __ bne(&new_target_constructor, cr0);
    __ bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(r6);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ bind(&new_target_constructor);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ LoadP(r7, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(ip, MemOperand(r7, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ cmpi(ip, Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ beq(&arguments_adaptor);
  {
    __ LoadP(r8, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    __ LoadP(r8, FieldMemOperand(r8, JSFunction::kSharedFunctionInfoOffset));
    __ LoadWordArith(
        r8,
        FieldMemOperand(r8, SharedFunctionInfo::kFormalParameterCountOffset));
    __ mr(r7, fp);
  }
  __ b(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    // Load the length from the ArgumentsAdaptorFrame.
    __ LoadP(r8, MemOperand(r7, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ SmiUntag(r8);
  }
  __ bind(&arguments_done);

  Label stack_done, stack_overflow;
  __ sub(r8, r8, r5);
  __ cmpi(r8, Operand::Zero());
  __ ble(&stack_done);
  {
    // Check for stack overflow.
    Generate_StackOverflowCheck(masm, r8, r5, &stack_overflow);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ addi(r7, r7, Operand(kPointerSize));
      __ add(r3, r3, r8);
      __ bind(&loop);
      {
        __ ShiftLeftImm(ip, r8, Operand(kPointerSizeLog2));
        __ LoadPX(ip, MemOperand(r7, ip));
        __ push(ip);
        __ subi(r8, r8, Operand(1));
        __ cmpi(r8, Operand::Zero());
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
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(r4);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ LoadP(r5, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ lwz(r6, FieldMemOperand(r5, SharedFunctionInfo::kCompilerHintsOffset));
  __ TestBitMask(r6, SharedFunctionInfo::kClassConstructorMask, r0);
  __ bne(&class_constructor, cr0);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ andi(r0, r6,
          Operand(SharedFunctionInfo::IsStrictBit::kMask |
                  SharedFunctionInfo::IsNativeBit::kMask));
  __ bne(&done_convert, cr0);
  {
    // ----------- S t a t e -------------
    //  -- r3 : the number of arguments (not including the receiver)
    //  -- r4 : the function to call (checked to be a JSFunction)
    //  -- r5 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(r6);
    } else {
      Label convert_to_object, convert_receiver;
      __ ShiftLeftImm(r6, r3, Operand(kPointerSizeLog2));
      __ LoadPX(r6, MemOperand(sp, r6));
      __ JumpIfSmi(r6, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CompareObjectType(r6, r7, r7, FIRST_JS_RECEIVER_TYPE);
      __ bge(&done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(r6, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy);
        __ JumpIfNotRoot(r6, Heap::kNullValueRootIndex, &convert_to_object);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(r6);
        }
        __ b(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(r3);
        __ Push(r3, r4);
        __ mr(r3, r6);
        __ Push(cp);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ mr(r6, r3);
        __ Pop(r3, r4);
        __ SmiUntag(r3);
      }
      __ LoadP(r5, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ ShiftLeftImm(r7, r3, Operand(kPointerSizeLog2));
    __ StorePX(r6, MemOperand(sp, r7));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSFunction)
  //  -- r5 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ LoadWordArith(
      r5, FieldMemOperand(r5, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(r3);
  ParameterCount expected(r5);
  __ InvokeFunctionCode(r4, no_reg, expected, actual, JUMP_FUNCTION);

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameAndConstantPoolScope frame(masm, StackFrame::INTERNAL);
    __ push(r4);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : target (checked to be a JSBoundFunction)
  //  -- r6 : new.target (only in case of [[Construct]])
  // -----------------------------------

  // Load [[BoundArguments]] into r5 and length of that into r7.
  Label no_bound_arguments;
  __ LoadP(r5, FieldMemOperand(r4, JSBoundFunction::kBoundArgumentsOffset));
  __ LoadP(r7, FieldMemOperand(r5, FixedArray::kLengthOffset));
  __ SmiUntag(r7, SetRC);
  __ beq(&no_bound_arguments, cr0);
  {
    // ----------- S t a t e -------------
    //  -- r3 : the number of arguments (not including the receiver)
    //  -- r4 : target (checked to be a JSBoundFunction)
    //  -- r5 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- r6 : new.target (only in case of [[Construct]])
    //  -- r7 : the number of [[BoundArguments]]
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ mr(r9, sp);  // preserve previous stack pointer
      __ ShiftLeftImm(r10, r7, Operand(kPointerSizeLog2));
      __ sub(sp, sp, r10);
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRoot(sp, Heap::kRealStackLimitRootIndex);
      __ bgt(&done);  // Signed comparison.
      // Restore the stack pointer.
      __ mr(sp, r9);
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Relocate arguments down the stack.
    //  -- r3 : the number of arguments (not including the receiver)
    //  -- r9 : the previous stack pointer
    //  -- r10: the size of the [[BoundArguments]]
    {
      Label skip, loop;
      __ li(r8, Operand::Zero());
      __ cmpi(r3, Operand::Zero());
      __ beq(&skip);
      __ mtctr(r3);
      __ bind(&loop);
      __ LoadPX(r0, MemOperand(r9, r8));
      __ StorePX(r0, MemOperand(sp, r8));
      __ addi(r8, r8, Operand(kPointerSize));
      __ bdnz(&loop);
      __ bind(&skip);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ addi(r5, r5, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
      __ add(r5, r5, r10);
      __ mtctr(r7);
      __ bind(&loop);
      __ LoadPU(r0, MemOperand(r5, -kPointerSize));
      __ StorePX(r0, MemOperand(sp, r8));
      __ addi(r8, r8, Operand(kPointerSize));
      __ bdnz(&loop);
      __ add(r3, r3, r7);
    }
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(r4);

  // Patch the receiver to [[BoundThis]].
  __ LoadP(ip, FieldMemOperand(r4, JSBoundFunction::kBoundThisOffset));
  __ ShiftLeftImm(r0, r3, Operand(kPointerSizeLog2));
  __ StorePX(ip, MemOperand(sp, r0));

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ LoadP(r4,
           FieldMemOperand(r4, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(r4, &non_callable);
  __ bind(&non_smi);
  __ CompareObjectType(r4, r7, r8, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, eq);
  __ cmpi(r8, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  __ lbz(r7, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r7, Map::IsCallableBit::kShift, r0);
  __ beq(&non_callable, cr0);

  // Check if target is a proxy and call CallProxy external builtin
  __ cmpi(r8, Operand(JS_PROXY_TYPE));
  __ bne(&non_function);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver the (original) target.
  __ ShiftLeftImm(r8, r3, Operand(kPointerSizeLog2));
  __ StorePX(r4, MemOperand(sp, r8));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, r4);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the constructor to call (checked to be a JSFunction)
  //  -- r6 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertFunction(r4);

  // Calling convention for function specific ConstructStubs require
  // r5 to contain either an AllocationSite or undefined.
  __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ LoadP(r7, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r7, FieldMemOperand(r7, SharedFunctionInfo::kConstructStubOffset));
  __ addi(ip, r7, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSBoundFunction)
  //  -- r6 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertBoundFunction(r4);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  Label skip;
  __ cmp(r4, r6);
  __ bne(&skip);
  __ LoadP(r6,
           FieldMemOperand(r4, JSBoundFunction::kBoundTargetFunctionOffset));
  __ bind(&skip);

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ LoadP(r4,
           FieldMemOperand(r4, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the constructor to call (can be any Object)
  //  -- r6 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor, non_proxy;
  __ JumpIfSmi(r4, &non_constructor);

  // Dispatch based on instance type.
  __ CompareObjectType(r4, r7, r8, JS_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Construct]] internal method.
  __ lbz(r5, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r5, Map::IsConstructorBit::kShift, r0);
  __ beq(&non_constructor, cr0);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ cmpi(r8, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to proxies after checking whether they are constructors.
  __ cmpi(r8, Operand(JS_PROXY_TYPE));
  __ bne(&non_proxy);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
  {
    // Overwrite the original receiver with the (original) target.
    __ ShiftLeftImm(r8, r3, Operand(kPointerSizeLog2));
    __ StorePX(r4, MemOperand(sp, r8));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, r4);
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
  //  -- r4 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r4);
  __ Push(r4);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r4 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r4);
  __ LoadSmiLiteral(r5, Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ Push(r4, r5);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

// static
void Builtins::Generate_Abort(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r4 : message_id as Smi
  //  -- lr : return address
  // -----------------------------------
  __ push(r4);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbort);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : actual number of arguments
  //  -- r4 : function (passed through to callee)
  //  -- r5 : expected number of arguments
  //  -- r6 : new target (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ LoadP(ip, FieldMemOperand(r4, JSFunction::kCodeOffset));
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ cmp(r3, r5);
  __ blt(&too_few);
  __ cmpi(r5, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ beq(&dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r5, r8, &stack_overflow);

    // Calculate copy start address into r3 and copy end address into r7.
    // r3: actual number of arguments as a smi
    // r4: function
    // r5: expected number of arguments
    // r6: new target (passed through to callee)
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r3, r3);
    __ add(r3, r3, fp);
    // adjust for return address and receiver
    __ addi(r3, r3, Operand(2 * kPointerSize));
    __ ShiftLeftImm(r7, r5, Operand(kPointerSizeLog2));
    __ sub(r7, r3, r7);

    // Copy the arguments (including the receiver) to the new stack frame.
    // r3: copy start address
    // r4: function
    // r5: expected number of arguments
    // r6: new target (passed through to callee)
    // r7: copy end address
    // ip: code entry to call

    Label copy;
    __ bind(&copy);
    __ LoadP(r0, MemOperand(r3, 0));
    __ push(r0);
    __ cmp(r3, r7);  // Compare before moving to next argument.
    __ subi(r3, r3, Operand(kPointerSize));
    __ bne(&copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);

    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r5, r8, &stack_overflow);

    // Calculate copy start address into r0 and copy end address is fp.
    // r3: actual number of arguments as a smi
    // r4: function
    // r5: expected number of arguments
    // r6: new target (passed through to callee)
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r3, r3);
    __ add(r3, r3, fp);

    // Copy the arguments (including the receiver) to the new stack frame.
    // r3: copy start address
    // r4: function
    // r5: expected number of arguments
    // r6: new target (passed through to callee)
    // ip: code entry to call
    Label copy;
    __ bind(&copy);
    // Adjust load for return address and receiver.
    __ LoadP(r0, MemOperand(r3, 2 * kPointerSize));
    __ push(r0);
    __ cmp(r3, fp);  // Compare before moving to next argument.
    __ subi(r3, r3, Operand(kPointerSize));
    __ bne(&copy);

    // Fill the remaining expected arguments with undefined.
    // r4: function
    // r5: expected number of arguments
    // r6: new target (passed through to callee)
    // ip: code entry to call
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    __ ShiftLeftImm(r7, r5, Operand(kPointerSizeLog2));
    __ sub(r7, fp, r7);
    // Adjust for frame.
    __ subi(r7, r7,
            Operand(ArgumentsAdaptorFrameConstants::kFixedFrameSizeFromFp +
                    kPointerSize));

    Label fill;
    __ bind(&fill);
    __ push(r0);
    __ cmp(sp, r7);
    __ bne(&fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mr(r3, r5);
  // r3 : expected number of arguments
  // r4 : function (passed through to callee)
  // r6 : new target (passed through to callee)
  __ CallJSEntry(ip);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ blr();

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
    constexpr RegList gp_regs =
        Register::ListOf<r3, r4, r5, r6, r7, r8, r9, r10>();
    constexpr RegList fp_regs =
        DoubleRegister::ListOf<d1, d2, d3, d4, d5, d6, d7, d8>();
    __ MultiPush(gp_regs);
    __ MultiPushDoubles(fp_regs);

    // Initialize cp register with kZero, CEntryStub will use it to set the
    // current context on the isolate.
    __ LoadSmiLiteral(cp, Smi::kZero);
    __ CallRuntime(Runtime::kWasmCompileLazy);
    // Store returned instruction start in r11.
    __ addi(r11, r3, Operand(Code::kHeaderSize - kHeapObjectTag));

    // Restore registers.
    __ MultiPopDoubles(fp_regs);
    __ MultiPop(gp_regs);
  }
  // Now jump to the instructions of the returned code object.
  __ Jump(r11);
}

#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC

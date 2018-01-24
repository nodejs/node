// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS

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
  __ li(s2, Operand(ExternalReference(address, masm->isolate())));
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
  //  -- a0                 : number of arguments excluding receiver
  //  -- a1                 : target
  //  -- a3                 : new.target
  //  -- s2                 : entry point
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * agrc]       : receiver
  // -----------------------------------
  __ AssertFunction(a1);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // CEntryStub expects a0 to contain the number of arguments including the
  // receiver and the extra arguments.
  __ Addu(a0, a0, BuiltinExitFrameConstants::kNumExtraArgsWithReceiver);

  // Insert extra arguments.
  __ PushRoot(Heap::kTheHoleValueRootIndex);  // Padding.
  __ SmiTag(a0);
  __ Push(a0, a1, a3);
  __ SmiUntag(a0);

  // Jump to the C entry runtime stub directly here instead of using
  // JumpToExternalReference. We have already loaded entry point to s2
  // in Generate_adaptor.
  __ mov(a1, s2);
  CEntryStub stub(masm->isolate(), 1, kDontSaveFPRegs, kArgvOnStack,
                  exit_frame_type == Builtins::BUILTIN_EXIT);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET, al, zero_reg,
          Operand(zero_reg), PROTECT);
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
  // Load the InternalArray function from the native context.
  __ LoadNativeContextSlot(Context::INTERNAL_ARRAY_FUNCTION_INDEX, result);
}

// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the Array function from the native context.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, result);
}

void Builtins::Generate_InternalArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, a1);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ lw(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(a2, t0);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction, t0,
              Operand(zero_reg));
    __ GetObjectType(a2, a3, t0);
    __ Assert(eq, kUnexpectedInitialMapForInternalArrayFunction, t0,
              Operand(MAP_TYPE));
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // Tail call a stub.
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

void Builtins::Generate_ArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, a1);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ lw(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(a2, t0);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction1, t0,
              Operand(zero_reg));
    __ GetObjectType(a2, a3, t0);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction2, t0,
              Operand(MAP_TYPE));
  }

  // Run the native code for the Array function called as a normal function.
  // Tail call a stub.
  __ mov(a3, a1);
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a2, FieldMemOperand(a2, SharedFunctionInfo::kCodeOffset));
  __ Jump(at, a2, Code::kHeaderSize - kHeapObjectTag);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- a0 : argument count (preserved for callee)
  //  -- a1 : target function (preserved for callee)
  //  -- a3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ SmiTag(a0);
    __ Push(a0, a1, a3, a1);

    __ CallRuntime(function_id, 1);

    // Restore target function and new target.
    __ Pop(a0, a1, a3);
    __ SmiUntag(a0);
  }

  __ Jump(at, v0, Code::kHeaderSize - kHeapObjectTag);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- a3     : new target
  //  -- cp     : context
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(a0);
    __ Push(cp, a0);
    __ SmiUntag(a0);

    // The receiver for the builtin/api call.
    __ PushRoot(Heap::kTheHoleValueRootIndex);

    // Set up pointer to last argument.
    __ Addu(t2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(t3, a0);
    // ----------- S t a t e -------------
    //  --                        a0: number of arguments (untagged)
    //  --                        a3: new target
    //  --                        t2: pointer to last argument
    //  --                        t3: counter
    //  --        sp[0*kPointerSize]: the hole (receiver)
    //  --        sp[1*kPointerSize]: number of arguments (tagged)
    //  --        sp[2*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry);
    __ bind(&loop);
    __ Lsa(t0, t2, t3, kPointerSizeLog2);
    __ lw(t1, MemOperand(t0));
    __ push(t1);
    __ bind(&entry);
    __ Addu(t3, t3, Operand(-1));
    __ Branch(&loop, greater_equal, t3, Operand(zero_reg));

    // Call the function.
    // a0: number of arguments (untagged)
    // a1: constructor function
    // a3: new target
    ParameterCount actual(a0);
    __ InvokeFunction(a1, a3, actual, CALL_FUNCTION);

    // Restore context from the frame.
    __ lw(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ lw(a1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ Lsa(sp, sp, a1, kPointerSizeLog2 - 1);
  __ Addu(sp, sp, kPointerSize);
  __ Ret();
}

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Generate_JSConstructStubGeneric(MacroAssembler* masm,
                                     bool restrict_constructor_return) {
  // ----------- S t a t e -------------
  //  --      a0: number of arguments (untagged)
  //  --      a1: constructor function
  //  --      a3: new target
  //  --      cp: context
  //  --      ra: return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ SmiTag(a0);
    __ Push(cp, a0, a1, a3);

    // ----------- S t a t e -------------
    //  --        sp[0*kPointerSize]: new target
    //  -- a1 and sp[1*kPointerSize]: constructor function
    //  --        sp[2*kPointerSize]: number of arguments (tagged)
    //  --        sp[3*kPointerSize]: context
    // -----------------------------------

    __ lw(t2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lw(t2, FieldMemOperand(t2, SharedFunctionInfo::kCompilerHintsOffset));
    __ And(t2, t2, Operand(SharedFunctionInfo::kDerivedConstructorMask));
    __ Branch(&not_create_implicit_receiver, ne, t2, Operand(zero_reg));

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        t2, t3);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ Branch(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(v0, Heap::kTheHoleValueRootIndex);

    // ----------- S t a t e -------------
    //  --                          v0: receiver
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
    __ Pop(a3);
    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(v0, v0);

    // ----------- S t a t e -------------
    //  --                 r3: new target
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: implicit receiver
    //  -- sp[2*kPointerSize]: constructor function
    //  -- sp[3*kPointerSize]: number of arguments (tagged)
    //  -- sp[4*kPointerSize]: context
    // -----------------------------------

    // Restore constructor function and argument count.
    __ lw(a1, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ lw(a0, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    __ SmiUntag(a0);

    // Set up pointer to last argument.
    __ Addu(t2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(t3, a0);
    // ----------- S t a t e -------------
    //  --                        a0: number of arguments (untagged)
    //  --                        a3: new target
    //  --                        t2: pointer to last argument
    //  --                        t3: counter
    //  --        sp[0*kPointerSize]: implicit receiver
    //  --        sp[1*kPointerSize]: implicit receiver
    //  -- a1 and sp[2*kPointerSize]: constructor function
    //  --        sp[3*kPointerSize]: number of arguments (tagged)
    //  --        sp[4*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry);
    __ bind(&loop);
    __ Lsa(t0, t2, t3, kPointerSizeLog2);
    __ lw(t1, MemOperand(t0));
    __ push(t1);
    __ bind(&entry);
    __ Addu(t3, t3, Operand(-1));
    __ Branch(&loop, greater_equal, t3, Operand(zero_reg));

    // Call the function.
    ParameterCount actual(a0);
    __ InvokeFunction(a1, a3, actual, CALL_FUNCTION);

    // ----------- S t a t e -------------
    //  --                 v0: constructor result
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: constructor function
    //  -- sp[2*kPointerSize]: number of arguments
    //  -- sp[3*kPointerSize]: context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore the context from the frame.
    __ lw(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, other_result, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(v0, Heap::kUndefinedValueRootIndex, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(v0, &other_result);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    __ GetObjectType(v0, t2, t2);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ Branch(&leave_frame, greater_equal, t2, Operand(FIRST_JS_RECEIVER_TYPE));

    // The result is now neither undefined nor an object.
    __ bind(&other_result);
    __ lw(a1, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ lw(t2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lw(t2, FieldMemOperand(t2, SharedFunctionInfo::kCompilerHintsOffset));
    __ And(t2, t2, Operand(SharedFunctionInfo::kClassConstructorMask));

    if (restrict_constructor_return) {
      // Throw if constructor function is a class constructor
      __ Branch(&use_receiver, eq, t2, Operand(zero_reg));
    } else {
      __ Branch(&use_receiver, ne, t2, Operand(zero_reg));
      __ CallRuntime(
          Runtime::kIncrementUseCounterConstructorReturnNonUndefinedPrimitive);
      __ Branch(&use_receiver);
    }

    __ bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ lw(v0, MemOperand(sp, 0 * kPointerSize));
    __ JumpIfRoot(v0, Heap::kTheHoleValueRootIndex, &do_throw);

    __ bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ lw(a1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  __ Lsa(sp, sp, a1, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(sp, sp, kPointerSize);
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

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ Push(a1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

// Clobbers a2; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(a2, Heap::kRealStackLimitRootIndex);
  // Make a2 the space we have left. The stack might already be overflowed
  // here which will cause a2 to become negative.
  __ Subu(a2, sp, a2);
  // Check if the arguments will overflow the stack.
  __ sll(t3, argc, kPointerSizeLog2);
  // Signed comparison.
  __ Branch(&okay, gt, a2, Operand(t3));

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from JSEntryStub::GenerateBody

  // ----------- S t a t e -------------
  //  -- a0: new.target
  //  -- a1: function
  //  -- a2: receiver_pointer
  //  -- a3: argc
  //  -- s0: argv
  // -----------------------------------
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(IsolateAddressId::kContextAddress,
                                      masm->isolate());
    __ li(cp, Operand(context_address));
    __ lw(cp, MemOperand(cp));

    // Push the function and the receiver onto the stack.
    __ Push(a1, a2);

    // Check if we have enough stack space to push all arguments.
    // Clobbers a2.
    Generate_CheckStackOverflow(masm, a3);

    // Remember new.target.
    __ mov(t1, a0);

    // Copy arguments to the stack in a loop.
    // a3: argc
    // s0: argv, i.e. points to first arg
    Label loop, entry;
    __ Lsa(t2, s0, a3, kPointerSizeLog2);
    __ b(&entry);
    __ nop();  // Branch delay slot nop.
    // t2 points past last arg.
    __ bind(&loop);
    __ lw(t0, MemOperand(s0));  // Read next parameter.
    __ addiu(s0, s0, kPointerSize);
    __ lw(t0, MemOperand(t0));  // Dereference handle.
    __ push(t0);                // Push parameter.
    __ bind(&entry);
    __ Branch(&loop, ne, s0, Operand(t2));

    // Setup new.target and argc.
    __ mov(a0, a3);
    __ mov(a3, t1);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
    __ mov(s1, t0);
    __ mov(s2, t0);
    __ mov(s3, t0);
    __ mov(s4, t0);
    __ mov(s5, t0);
    // s6 holds the root address. Do not clobber.
    // s7 is cp. Do not init.

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Leave internal frame.
  }

  __ Jump(ra);
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
  //  -- v0 : the value to pass to the generator
  //  -- a1 : the JSGeneratorObject to resume
  //  -- ra : return address
  // -----------------------------------

  __ AssertGeneratorObject(a1);

  // Store input value into generator object.
  __ sw(v0, FieldMemOperand(a1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(a1, JSGeneratorObject::kInputOrDebugPosOffset, v0, a3,
                      kRAHasNotBeenSaved, kDontSaveFPRegs);

  // Load suspended function and context.
  __ lw(t0, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
  __ lw(cp, FieldMemOperand(t0, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ li(t1, Operand(debug_hook));
  __ lb(t1, MemOperand(t1));
  __ Branch(&prepare_step_in_if_stepping, ne, t1, Operand(zero_reg));

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ li(t1, Operand(debug_suspended_generator));
  __ lw(t1, MemOperand(t1));
  __ Branch(&prepare_step_in_suspended_generator, eq, a1, Operand(t1));
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ LoadRoot(at, Heap::kRealStackLimitRootIndex);
  __ Branch(&stack_overflow, lo, sp, Operand(at));

  // Push receiver.
  __ lw(t1, FieldMemOperand(a1, JSGeneratorObject::kReceiverOffset));
  __ Push(t1);

  // ----------- S t a t e -------------
  //  -- a1    : the JSGeneratorObject to resume
  //  -- t0    : generator function
  //  -- cp    : generator context
  //  -- ra    : return address
  //  -- sp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ lw(a3, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a3,
        FieldMemOperand(a3, SharedFunctionInfo::kFormalParameterCountOffset));
  {
    Label done_loop, loop;
    __ bind(&loop);
    __ Subu(a3, a3, Operand(1));
    __ Branch(&done_loop, lt, a3, Operand(zero_reg));
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ lw(a3, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
    __ lw(a3, FieldMemOperand(a3, SharedFunctionInfo::kFunctionDataOffset));
    __ GetObjectType(a3, a3, a3);
    __ Assert(eq, kMissingBytecodeArray, a3, Operand(BYTECODE_ARRAY_TYPE));
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ lw(a0, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
    __ lw(a0,
          FieldMemOperand(a0, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Move(a3, a1);
    __ Move(a1, t0);
    __ lw(a2, FieldMemOperand(a1, JSFunction::kCodeOffset));
    __ Jump(a2, Code::kHeaderSize - kHeapObjectTag);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1, t0);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(a1);
  }
  __ Branch(USE_DELAY_SLOT, &stepping_prepared);
  __ lw(t0, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(a1);
  }
  __ Branch(USE_DELAY_SLOT, &stepping_prepared);
  __ lw(t0, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ break_(0xCC);  // This should be unreachable.
  }
}

static void ReplaceClosureCodeWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {
  // Store code entry in the closure.
  __ sw(optimized_code, FieldMemOperand(closure, JSFunction::kCodeOffset));
  __ mov(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kRAHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_count = scratch;

  // Get the arguments + receiver count.
  __ lw(args_count,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ lw(args_count,
        FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  // Drop receiver + arguments.
  __ Addu(sp, sp, args_count);
}

// Tail-call |function_id| if |smi_entry| == |marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register smi_entry,
                                          OptimizationMarker marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ Branch(&no_match, ne, smi_entry, Operand(Smi::FromEnum(marker)));
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void MaybeTailCallOptimizedCodeSlot(MacroAssembler* masm,
                                           Register feedback_vector,
                                           Register scratch1, Register scratch2,
                                           Register scratch3) {
  // ----------- S t a t e -------------
  //  -- a0 : argument count (preserved for callee if needed, and caller)
  //  -- a3 : new target (preserved for callee if needed, and caller)
  //  -- a1 : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  // -----------------------------------
  DCHECK(
      !AreAliased(feedback_vector, a0, a1, a3, scratch1, scratch2, scratch3));

  Label optimized_code_slot_is_cell, fallthrough;

  Register closure = a1;
  Register optimized_code_entry = scratch1;

  __ lw(optimized_code_entry,
        FieldMemOperand(feedback_vector, FeedbackVector::kOptimizedCodeOffset));

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret is as a weak cell to a code
  // object.
  __ JumpIfNotSmi(optimized_code_entry, &optimized_code_slot_is_cell);

  {
    // Optimized code slot is a Smi optimization marker.

    // Fall through if no optimization trigger.
    __ Branch(&fallthrough, eq, optimized_code_entry,
              Operand(Smi::FromEnum(OptimizationMarker::kNone)));

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
        __ Assert(
            eq, kExpectedOptimizationSentinel, optimized_code_entry,
            Operand(Smi::FromEnum(OptimizationMarker::kInOptimizationQueue)));
      }
      __ jmp(&fallthrough);
    }
  }

  {
    // Optimized code slot is a WeakCell.
    __ bind(&optimized_code_slot_is_cell);

    __ lw(optimized_code_entry,
          FieldMemOperand(optimized_code_entry, WeakCell::kValueOffset));
    __ JumpIfSmi(optimized_code_entry, &fallthrough);

    // Check if the optimized code is marked for deopt. If it is, call the
    // runtime to clear it.
    Label found_deoptimized_code;
    __ lw(scratch2, FieldMemOperand(optimized_code_entry,
                                    Code::kCodeDataContainerOffset));
    __ lw(scratch2, FieldMemOperand(
                        scratch2, CodeDataContainer::kKindSpecificFlagsOffset));
    __ And(scratch2, scratch2, Operand(1 << Code::kMarkedForDeoptimizationBit));
    __ Branch(&found_deoptimized_code, ne, scratch2, Operand(zero_reg));

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    // The feedback vector is no longer used, so re-use it as a scratch
    // register.
    ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                        scratch2, scratch3, feedback_vector);
    __ Jump(optimized_code_entry, Code::kHeaderSize - kHeapObjectTag);

    // Optimized code slot contains deoptimized code, evict it and re-enter the
    // losure's code.
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
                                  Register scratch1, Register scratch2) {
  Register bytecode_size_table = scratch1;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode));

  __ li(
      bytecode_size_table,
      Operand(ExternalReference::bytecode_size_table_address(masm->isolate())));

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label load_size, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  __ Branch(&load_size, hi, bytecode, Operand(1));
  __ Branch(&extra_wide, eq, bytecode, Operand(1));

  // Load the next bytecode and update table to the wide scaled table.
  __ Addu(bytecode_offset, bytecode_offset, Operand(1));
  __ Addu(scratch2, bytecode_array, bytecode_offset);
  __ lbu(bytecode, MemOperand(scratch2));
  __ Addu(bytecode_size_table, bytecode_size_table,
          Operand(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&load_size);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ Addu(bytecode_offset, bytecode_offset, Operand(1));
  __ Addu(scratch2, bytecode_array, bytecode_offset);
  __ lbu(bytecode, MemOperand(scratch2));
  __ Addu(bytecode_size_table, bytecode_size_table,
          Operand(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&load_size);

  // Load the size of the current bytecode.
  __ bind(&load_size);
  __ Lsa(scratch2, bytecode_size_table, bytecode, 2);
  __ lw(scratch2, MemOperand(scratch2));
  __ Addu(bytecode_offset, bytecode_offset, scratch2);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o a1: the JS function object being called.
//   o a3: the incoming new target or generator object
//   o cp: our context
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o ra: return address
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  Register closure = a1;
  Register feedback_vector = a2;

  // Load the feedback vector from the closure.
  __ lw(feedback_vector,
        FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ lw(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));
  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, t0, t3, t1);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(closure);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  Label maybe_load_debug_bytecode_array, bytecode_array_loaded;
  __ lw(a0, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ lw(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(a0, SharedFunctionInfo::kFunctionDataOffset));
  __ lw(t0, FieldMemOperand(a0, SharedFunctionInfo::kDebugInfoOffset));
  __ JumpIfNotSmi(t0, &maybe_load_debug_bytecode_array);
  __ bind(&bytecode_array_loaded);

  // Increment invocation count for the function.
  __ lw(t0, FieldMemOperand(feedback_vector,
                            FeedbackVector::kInvocationCountOffset));
  __ Addu(t0, t0, Operand(1));
  __ sw(t0, FieldMemOperand(feedback_vector,
                            FeedbackVector::kInvocationCountOffset));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ SmiTst(kInterpreterBytecodeArrayRegister, t0);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, t0,
              Operand(zero_reg));
    __ GetObjectType(kInterpreterBytecodeArrayRegister, t0, t0);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, t0,
              Operand(BYTECODE_ARRAY_TYPE));
  }

  // Reset code age.
  DCHECK_EQ(0, BytecodeArray::kNoAgeBytecodeAge);
  __ sb(zero_reg, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                  BytecodeArray::kBytecodeAgeOffset));

  // Load initial bytecode offset.
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(t0, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, t0);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    __ lw(t0, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ Subu(t1, sp, Operand(t0));
    __ LoadRoot(a2, Heap::kRealStackLimitRootIndex);
    __ Branch(&ok, hs, t1, Operand(a2));
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(t1, Heap::kUndefinedValueRootIndex);
    __ Branch(&loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(t1);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ Subu(t0, t0, Operand(kPointerSize));
    __ Branch(&loop_header, ge, t0, Operand(zero_reg));
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in r3.
  Label no_incoming_new_target_or_generator_register;
  __ lw(t1, FieldMemOperand(
                kInterpreterBytecodeArrayRegister,
                BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ Branch(&no_incoming_new_target_or_generator_register, eq, t1,
            Operand(zero_reg));
  __ Lsa(t1, fp, t1, kPointerSizeLog2);
  __ sw(a3, MemOperand(t1));
  __ bind(&no_incoming_new_target_or_generator_register);

  // Load accumulator with undefined.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ li(kInterpreterDispatchTableRegister,
        Operand(ExternalReference::interpreter_dispatch_table_address(
            masm->isolate())));
  __ Addu(a0, kInterpreterBytecodeArrayRegister,
          kInterpreterBytecodeOffsetRegister);
  __ lbu(a0, MemOperand(a0));
  __ Lsa(at, kInterpreterDispatchTableRegister, a0, kPointerSizeLog2);
  __ lw(at, MemOperand(at));
  __ Call(at);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ lw(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ lw(kInterpreterBytecodeOffsetRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Check if we should return.
  Label do_return;
  __ Addu(a1, kInterpreterBytecodeArrayRegister,
          kInterpreterBytecodeOffsetRegister);
  __ lbu(a1, MemOperand(a1));
  __ Branch(&do_return, eq, a1,
            Operand(static_cast<int>(interpreter::Bytecode::kReturn)));

  // Advance to the next bytecode and dispatch.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, a1, a2, a3);
  __ jmp(&do_dispatch);

  __ bind(&do_return);
  // The return value is in v0.
  LeaveInterpreterFrame(masm, t0);
  __ Jump(ra);

  // Load debug copy of the bytecode array if it exists.
  // kInterpreterBytecodeArrayRegister is already loaded with
  // SharedFunctionInfo::kFunctionDataOffset.
  __ bind(&maybe_load_debug_bytecode_array);
  __ lw(t1, FieldMemOperand(t0, DebugInfo::kFlagsOffset));
  __ SmiUntag(t1);
  __ And(t1, t1, Operand(DebugInfo::kHasBreakInfo));
  __ Branch(&bytecode_array_loaded, eq, t1, Operand(zero_reg));
  __ lw(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(t0, DebugInfo::kDebugBytecodeArrayOffset));
  __ Branch(&bytecode_array_loaded);
}


static void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                        Register scratch1, Register scratch2,
                                        Label* stack_overflow) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(scratch1, Heap::kRealStackLimitRootIndex);
  // Make scratch1 the space we have left. The stack might already be overflowed
  // here which will cause scratch1 to become negative.
  __ subu(scratch1, sp, scratch1);
  // Check if the arguments will overflow the stack.
  __ sll(scratch2, num_args, kPointerSizeLog2);
  // Signed comparison.
  __ Branch(stack_overflow, le, scratch1, Operand(scratch2));
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args, Register index,
                                         Register scratch, Register scratch2) {
  // Find the address of the last argument.
  __ mov(scratch2, num_args);
  __ sll(scratch2, scratch2, kPointerSizeLog2);
  __ Subu(scratch2, index, Operand(scratch2));

  // Push the arguments.
  Label loop_header, loop_check;
  __ Branch(&loop_check);
  __ bind(&loop_header);
  __ lw(scratch, MemOperand(index));
  __ Addu(index, index, Operand(-kPointerSize));
  __ push(scratch);
  __ bind(&loop_check);
  __ Branch(&loop_header, gt, index, Operand(scratch2));
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  __ Addu(t0, a0, Operand(1));  // Add one for receiver.

  Generate_StackOverflowCheck(masm, t0, t4, t1, &stack_overflow);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ mov(t0, a0);  // No receiver.
  }

  // This function modifies a2, t4 and t1.
  Generate_InterpreterPushArgs(masm, t0, a2, t4, t1);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(a2);                   // Pass the spread in a register
    __ Subu(a0, a0, Operand(1));  // Subtract one for spread
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
    __ break_(0xCC);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  // -- a0 : argument count (not including receiver)
  // -- a3 : new target
  // -- a1 : constructor to call
  // -- a2 : allocation site feedback if available, undefined otherwise.
  // -- t4 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver.
  __ push(zero_reg);

  Generate_StackOverflowCheck(masm, a0, t1, t0, &stack_overflow);

  // This function modified t4, t1 and t0.
  Generate_InterpreterPushArgs(masm, a0, t4, t1, t0);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(a2);                   // Pass the spread in a register
    __ Subu(a0, a0, Operand(1));  // Subtract one for spread
  } else {
    __ AssertUndefinedOrAllocationSite(a2, t0);
  }

  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ AssertFunction(a1);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ lw(t0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lw(t0, FieldMemOperand(t0, SharedFunctionInfo::kConstructStubOffset));
    __ Jump(at, t0, Code::kHeaderSize - kHeapObjectTag);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with a0, a1, and a3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with a0, a1, and a3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ break_(0xCC);
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Smi* interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);
  __ li(t0, Operand(BUILTIN_CODE(masm->isolate(), InterpreterEntryTrampoline)));
  __ Addu(ra, t0, Operand(interpreter_entry_return_pc_offset->value() +
                          Code::kHeaderSize - kHeapObjectTag));

  // Initialize the dispatch table register.
  __ li(kInterpreterDispatchTableRegister,
        Operand(ExternalReference::interpreter_dispatch_table_address(
            masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ lw(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ SmiTst(kInterpreterBytecodeArrayRegister, at);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, at,
              Operand(zero_reg));
    __ GetObjectType(kInterpreterBytecodeArrayRegister, a1, a1);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, a1,
              Operand(BYTECODE_ARRAY_TYPE));
  }

  // Get the target bytecode offset from the frame.
  __ lw(kInterpreterBytecodeOffsetRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ Addu(a1, kInterpreterBytecodeArrayRegister,
          kInterpreterBytecodeOffsetRegister);
  __ lbu(a1, MemOperand(a1));
  __ Lsa(a1, kInterpreterDispatchTableRegister, a1, kPointerSizeLog2);
  __ lw(a1, MemOperand(a1));
  __ Jump(a1);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ lw(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ lw(kInterpreterBytecodeOffsetRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Load the current bytecode.
  __ Addu(a1, kInterpreterBytecodeArrayRegister,
          kInterpreterBytecodeOffsetRegister);
  __ lbu(a1, MemOperand(a1));

  // Advance to the next bytecode.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, a1, a2, a3);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(a2, kInterpreterBytecodeOffsetRegister);
  __ sw(a2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CheckOptimizationMarker(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : argument count (preserved for callee)
  //  -- a3 : new target (preserved for callee)
  //  -- a1 : target function (preserved for callee)
  // -----------------------------------
  Register closure = a1;

  // Get the feedback vector.
  Register feedback_vector = a2;
  __ lw(feedback_vector,
        FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ lw(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));

  // The feedback vector must be defined.
  if (FLAG_debug_code) {
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    __ Assert(ne, BailoutReason::kExpectedFeedbackVector, feedback_vector,
              Operand(at));
  }

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, t0, t3, t1);

  // Otherwise, tail call the SFI code.
  GenerateTailCallToSharedCode(masm);
}

void Builtins::Generate_CompileLazyDeoptimizedCode(MacroAssembler* masm) {
  // Set the code slot inside the JSFunction to the trampoline to the
  // interpreter entry.
  __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a2, FieldMemOperand(a2, SharedFunctionInfo::kCodeOffset));
  __ sw(a2, FieldMemOperand(a1, JSFunction::kCodeOffset));
  __ RecordWriteField(a1, JSFunction::kCodeOffset, a2, t0, kRAHasNotBeenSaved,
                      kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  // Jump to compile lazy.
  Generate_CompileLazy(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : argument count (preserved for callee)
  //  -- a3 : new target (preserved for callee)
  //  -- a1 : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;

  Register closure = a1;
  Register feedback_vector = a2;

  // Do we have a valid feedback vector?
  __ lw(feedback_vector,
        FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ lw(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));
  __ JumpIfRoot(feedback_vector, Heap::kUndefinedValueRootIndex,
                &gotta_call_runtime);

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, t0, t3, t1);

  // We found no optimized code.
  Register entry = t0;
  __ lw(entry, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));

  // If SFI points to anything other than CompileLazy, install that.
  __ lw(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ Move(t1, masm->CodeObject());
  __ Branch(&gotta_call_runtime, eq, entry, Operand(t1));

  // Install the SFI's code entry.
  __ sw(entry, FieldMemOperand(closure, JSFunction::kCodeOffset));
  __ mov(t3, entry);  // Write barrier clobbers t3 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, t3, t1,
                      kRAHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ Jump(entry, Code::kHeaderSize - kHeapObjectTag);

  __ bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

// Lazy deserialization design doc: http://goo.gl/dxkYDZ.
void Builtins::Generate_DeserializeLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : argument count (preserved for callee)
  //  -- a3 : new target (preserved for callee)
  //  -- a1 : target function (preserved for callee)
  // -----------------------------------

  Label deserialize_in_runtime;

  Register target = a1;  // Must be preserved
  Register scratch0 = a2;
  Register scratch1 = t0;

  CHECK(scratch0 != a0 && scratch0 != a3 && scratch0 != a1);
  CHECK(scratch1 != a0 && scratch1 != a3 && scratch1 != a1);
  CHECK(scratch0 != scratch1);

  // Load the builtin id for lazy deserialization from SharedFunctionInfo.

  __ AssertFunction(target);
  __ lw(scratch0,
        FieldMemOperand(target, JSFunction::kSharedFunctionInfoOffset));

  __ lw(scratch1,
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
    __ li(scratch0,
          Operand(ExternalReference::builtins_address(masm->isolate())));
    __ Lsa(scratch1, scratch0, scratch1, kPointerSizeLog2);
    __ lw(scratch1, MemOperand(scratch1));

    // Check if the loaded code object has already been deserialized. This is
    // the case iff it does not equal DeserializeLazy.

    __ Move(scratch0, masm->CodeObject());
    __ Branch(&deserialize_in_runtime, eq, scratch1, Operand(scratch0));
  }

  {
    // If we've reached this spot, the target builtin has been deserialized and
    // we simply need to copy it over. First to the shared function info.

    Register target_builtin = scratch1;
    Register shared = scratch0;

    __ lw(shared,
          FieldMemOperand(target, JSFunction::kSharedFunctionInfoOffset));

    CHECK(t1 != target && t1 != scratch0 && t1 != scratch1);
    CHECK(t3 != target && t3 != scratch0 && t3 != scratch1);

    __ sw(target_builtin,
          FieldMemOperand(shared, SharedFunctionInfo::kCodeOffset));
    __ mov(t3, target_builtin);  // Write barrier clobbers t3 below.
    __ RecordWriteField(shared, SharedFunctionInfo::kCodeOffset, t3, t1,
                        kRAHasNotBeenSaved, kDontSaveFPRegs,
                        OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // And second to the target function.

    __ sw(target_builtin, FieldMemOperand(target, JSFunction::kCodeOffset));
    __ mov(t3, target_builtin);  // Write barrier clobbers t3 below.
    __ RecordWriteField(target, JSFunction::kCodeOffset, t3, t1,
                        kRAHasNotBeenSaved, kDontSaveFPRegs,
                        OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // All copying is done. Jump to the deserialized code object.

    __ Jump(target_builtin, Code::kHeaderSize - kHeapObjectTag);
  }

  __ bind(&deserialize_in_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kDeserializeLazy);
}

void Builtins::Generate_InstantiateAsmJs(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : argument count (preserved for callee)
  //  -- a1 : new target (preserved for callee)
  //  -- a3 : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ Move(t4, a0);
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ SmiTag(a0);
    __ Push(a0, a1, a3, a1);

    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ Branch(&over, ne, t4, Operand(j));
      }
      for (int i = j - 1; i >= 0; --i) {
        __ lw(t4, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                     i * kPointerSize));
        __ push(t4);
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
    __ JumpIfSmi(v0, &failed);

    __ Drop(2);
    __ pop(t4);
    __ SmiUntag(t4);
    scope.GenerateLeaveFrame();

    __ Addu(t4, t4, Operand(1));
    __ Lsa(sp, sp, t4, kPointerSizeLog2);
    __ Ret();

    __ bind(&failed);
    // Restore target function and new target.
    __ Pop(a0, a1, a3);
    __ SmiUntag(a0);
  }
  // On failure, tail call back to regular js by re-calling the function
  // which has be reset to the compile lazy builtin.
  __ lw(t0, FieldMemOperand(a1, JSFunction::kCodeOffset));
  __ Jump(t0, Code::kHeaderSize - kHeapObjectTag);
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
    __ sw(v0,
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
  __ lw(fp, MemOperand(
                sp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(t0);
  __ Addu(sp, sp,
          Operand(BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(ra);
  __ Addu(t0, t0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t0);
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

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), v0.code());
  __ lw(v0, MemOperand(sp, 0 * kPointerSize));
  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Addu(sp, sp, Operand(1 * kPointerSize));  // Remove accumulator.
}

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ lw(a0, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ lw(a0, MemOperand(a0, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ lw(a0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(a0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  __ Ret(eq, v0, Operand(Smi::kZero));

  // Drop any potential handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  if (has_handler_frame) {
    __ LeaveFrame(StackFrame::STUB);
  }

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ lw(a1, MemOperand(v0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ lw(a1, MemOperand(a1, FixedArray::OffsetOfElementAt(
                               DeoptimizationData::kOsrPcOffsetIndex) -
                               kHeapObjectTag));
  __ SmiUntag(a1);

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ Addu(v0, v0, a1);
  __ addiu(ra, v0, Code::kHeaderSize - kHeapObjectTag);

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
  //  -- a0    : argc
  //  -- sp[0] : argArray
  //  -- sp[4] : thisArg
  //  -- sp[8] : receiver
  // -----------------------------------

  // 1. Load receiver into a1, argArray into a0 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label no_arg;
    Register scratch = t0;
    __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
    __ mov(a3, a2);
    // Lsa() cannot be used hare as scratch value used later.
    __ sll(scratch, a0, kPointerSizeLog2);
    __ Addu(a0, sp, Operand(scratch));
    __ lw(a1, MemOperand(a0));  // receiver
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a2, MemOperand(a0));  // thisArg
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a3, MemOperand(a0));  // argArray
    __ bind(&no_arg);
    __ Addu(sp, sp, Operand(scratch));
    __ sw(a2, MemOperand(sp));
    __ mov(a2, a3);
  }

  // ----------- S t a t e -------------
  //  -- a2    : argArray
  //  -- a1    : receiver
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(a2, Heap::kNullValueRootIndex, &no_arguments);
  __ JumpIfRoot(a2, Heap::kUndefinedValueRootIndex, &no_arguments);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ mov(a0, zero_reg);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // a0: actual number of arguments
  {
    Label done;
    __ Branch(&done, ne, a0, Operand(zero_reg));
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ Addu(a0, a0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack.
  // a0: actual number of arguments
  __ Lsa(at, sp, a0, kPointerSizeLog2);
  __ lw(a1, MemOperand(at));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // a0: actual number of arguments
  // a1: function
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ Lsa(a2, sp, a0, kPointerSizeLog2);

    __ bind(&loop);
    __ lw(at, MemOperand(a2, -kPointerSize));
    __ sw(at, MemOperand(a2));
    __ Subu(a2, a2, Operand(kPointerSize));
    __ Branch(&loop, ne, a2, Operand(sp));
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ Subu(a0, a0, Operand(1));
    __ Pop();
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]  : argumentsList
  //  -- sp[4]  : thisArgument
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into a1 (if present), argumentsList into a0 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label no_arg;
    Register scratch = t0;
    __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
    __ mov(a2, a1);
    __ mov(a3, a1);
    __ sll(scratch, a0, kPointerSizeLog2);
    __ mov(a0, scratch);
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(zero_reg));
    __ Addu(a0, sp, Operand(a0));
    __ lw(a1, MemOperand(a0));  // target
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a2, MemOperand(a0));  // thisArgument
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a3, MemOperand(a0));  // argumentsList
    __ bind(&no_arg);
    __ Addu(sp, sp, Operand(scratch));
    __ sw(a2, MemOperand(sp));
    __ mov(a2, a3);
  }

  // ----------- S t a t e -------------
  //  -- a2    : argumentsList
  //  -- a1    : target
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
  //  -- a0     : argc
  //  -- sp[0]  : new.target (optional)
  //  -- sp[4]  : argumentsList
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into a1 (if present), argumentsList into a0 (if present),
  // new.target into a3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label no_arg;
    Register scratch = t0;
    __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
    __ mov(a2, a1);
    // Lsa() cannot be used hare as scratch value used later.
    __ sll(scratch, a0, kPointerSizeLog2);
    __ Addu(a0, sp, Operand(scratch));
    __ sw(a2, MemOperand(a0));  // receiver
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a1, MemOperand(a0));  // target
    __ mov(a3, a1);             // new.target defaults to target
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a2, MemOperand(a0));  // argumentsList
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a3, MemOperand(a0));  // new.target
    __ bind(&no_arg);
    __ Addu(sp, sp, Operand(scratch));
  }

  // ----------- S t a t e -------------
  //  -- a2    : argumentsList
  //  -- a3    : new.target
  //  -- a1    : target
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
  __ sll(a0, a0, kSmiTagSize);
  __ li(t0, Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ MultiPush(a0.bit() | a1.bit() | t0.bit() | fp.bit() | ra.bit());
  __ Addu(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                          kPointerSize));
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ lw(a1, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                             kPointerSize)));
  __ mov(sp, fp);
  __ MultiPop(fp.bit() | ra.bit());
  __ Lsa(sp, sp, a1, kPointerSizeLog2 - kSmiTagSize);
  // Adjust for the receiver.
  __ Addu(sp, sp, Operand(kPointerSize));
}

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- a1 : target
  //  -- a0 : number of parameters on the stack (not including the receiver)
  //  -- a2 : arguments list (a FixedArray)
  //  -- t0 : len (number of elements to push from args)
  //  -- a3 : new.target (for [[Construct]])
  // -----------------------------------
  __ AssertFixedArray(a2);

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(t1, Heap::kRealStackLimitRootIndex);
    // Make ip the space we have left. The stack might already be overflowed
    // here which will cause ip to become negative.
    __ Subu(t1, sp, t1);
    // Check if the arguments will overflow the stack.
    __ sll(at, t0, kPointerSizeLog2);
    __ Branch(&done, gt, t1, Operand(at));  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    __ mov(t2, zero_reg);
    Label done, push, loop;
    __ LoadRoot(t1, Heap::kTheHoleValueRootIndex);
    __ bind(&loop);
    __ Branch(&done, eq, t2, Operand(t0));
    __ Lsa(at, a2, t2, kPointerSizeLog2);
    __ lw(at, FieldMemOperand(at, FixedArray::kHeaderSize));
    __ Branch(&push, ne, t1, Operand(at));
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ Push(at);
    __ Addu(t2, t2, Operand(1));
    __ Branch(&loop);
    __ bind(&done);
    __ Addu(a0, a0, t2);
  }

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a3 : the new.target (for [[Construct]] calls)
  //  -- a1 : the target to call (can be any Object)
  //  -- a2 : start index (to support rest parameters)
  // -----------------------------------

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(a3, &new_target_not_constructor);
    __ lw(t1, FieldMemOperand(a3, HeapObject::kMapOffset));
    __ lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
    __ And(t1, t1, Operand(1 << Map::kIsConstructor));
    __ Branch(&new_target_constructor, ne, t1, Operand(zero_reg));
    __ bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(a3);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ bind(&new_target_constructor);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ lw(t3, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ lw(t2, MemOperand(t3, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Branch(&arguments_adaptor, eq, t2,
            Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  {
    __ lw(t2, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    __ lw(t2, FieldMemOperand(t2, JSFunction::kSharedFunctionInfoOffset));
    __ lw(t2,
          FieldMemOperand(t2, SharedFunctionInfo::kFormalParameterCountOffset));
    __ mov(t3, fp);
  }
  __ Branch(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    // Just get the length from the ArgumentsAdaptorFrame.
    __ lw(t2, MemOperand(t3, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ SmiUntag(t2);
  }
  __ bind(&arguments_done);

  Label stack_done, stack_overflow;
  __ Subu(t2, t2, a2);
  __ Branch(&stack_done, le, t2, Operand(zero_reg));
  {
    // Check for stack overflow.
    Generate_StackOverflowCheck(masm, t2, t0, t1, &stack_overflow);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ Addu(a0, a0, t2);
      __ bind(&loop);
      {
        __ Lsa(at, t3, t2, kPointerSizeLog2);
        __ lw(at, MemOperand(at, 1 * kPointerSize));
        __ push(at);
        __ Subu(t2, t2, Operand(1));
        __ Branch(&loop, ne, t2, Operand(zero_reg));
      }
    }
  }
  __ Branch(&stack_done);
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
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(a1);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a3, FieldMemOperand(a2, SharedFunctionInfo::kCompilerHintsOffset));
  __ And(at, a3, Operand(SharedFunctionInfo::kClassConstructorMask));
  __ Branch(&class_constructor, ne, at, Operand(zero_reg));

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ lw(a3, FieldMemOperand(a2, SharedFunctionInfo::kCompilerHintsOffset));
  __ And(at, a3,
         Operand(SharedFunctionInfo::IsNativeBit::kMask |
                 SharedFunctionInfo::IsStrictBit::kMask));
  __ Branch(&done_convert, ne, at, Operand(zero_reg));
  {
    // ----------- S t a t e -------------
    //  -- a0 : the number of arguments (not including the receiver)
    //  -- a1 : the function to call (checked to be a JSFunction)
    //  -- a2 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(a3);
    } else {
      Label convert_to_object, convert_receiver;
      __ Lsa(at, sp, a0, kPointerSizeLog2);
      __ lw(a3, MemOperand(at));
      __ JumpIfSmi(a3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ GetObjectType(a3, t0, t0);
      __ Branch(&done_convert, hs, t0, Operand(FIRST_JS_RECEIVER_TYPE));
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(a3, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy);
        __ JumpIfNotRoot(a3, Heap::kNullValueRootIndex, &convert_to_object);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(a3);
        }
        __ Branch(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ sll(a0, a0, kSmiTagSize);  // Smi tagged.
        __ Push(a0, a1);
        __ mov(a0, a3);
        __ Push(cp);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ mov(a3, v0);
        __ Pop(a0, a1);
        __ sra(a0, a0, kSmiTagSize);  // Un-tag.
      }
      __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ Lsa(at, sp, a0, kPointerSizeLog2);
    __ sw(a3, MemOperand(at));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSFunction)
  //  -- a2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ lw(a2,
        FieldMemOperand(a2, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(a0);
  ParameterCount expected(a2);
  __ InvokeFunctionCode(a1, no_reg, expected, actual, JUMP_FUNCTION);

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(a1);

  // Patch the receiver to [[BoundThis]].
  {
    __ lw(at, FieldMemOperand(a1, JSBoundFunction::kBoundThisOffset));
    __ Lsa(t0, sp, a0, kPointerSizeLog2);
    __ sw(at, MemOperand(t0));
  }

  // Load [[BoundArguments]] into a2 and length of that into t0.
  __ lw(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
  __ SmiUntag(t0);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- t0 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ sll(t1, t0, kPointerSizeLog2);
    __ Subu(sp, sp, Operand(t1));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    __ LoadRoot(at, Heap::kRealStackLimitRootIndex);
    __ Branch(&done, gt, sp, Operand(at));  // Signed comparison.
    // Restore the stack pointer.
    __ Addu(sp, sp, Operand(t1));
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowStackOverflow);
    }
    __ bind(&done);
  }

  // Relocate arguments down the stack.
  {
    Label loop, done_loop;
    __ mov(t1, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, gt, t1, Operand(a0));
    __ Lsa(t2, sp, t0, kPointerSizeLog2);
    __ lw(at, MemOperand(t2));
    __ Lsa(t2, sp, t1, kPointerSizeLog2);
    __ sw(at, MemOperand(t2));
    __ Addu(t0, t0, Operand(1));
    __ Addu(t1, t1, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ SmiUntag(t0);
    __ Addu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Subu(t0, t0, Operand(1));
    __ Branch(&done_loop, lt, t0, Operand(zero_reg));
    __ Lsa(t1, a2, t0, kPointerSizeLog2);
    __ lw(at, MemOperand(t1));
    __ Lsa(t1, sp, a0, kPointerSizeLog2);
    __ sw(at, MemOperand(t1));
    __ Addu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ lw(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(a1, &non_callable);
  __ bind(&non_smi);
  __ GetObjectType(a1, t1, t2);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_BOUND_FUNCTION_TYPE));

  // Check if target has a [[Call]] internal method.
  __ lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t1, t1, Operand(1 << Map::kIsCallable));
  __ Branch(&non_callable, eq, t1, Operand(zero_reg));

  // Check if target is a proxy and call CallProxy external builtin
  __ Branch(&non_function, ne, t2, Operand(JS_PROXY_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ Lsa(at, sp, a0, kPointerSizeLog2);
  __ sw(a1, MemOperand(at));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, a1);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (checked to be a JSFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertFunction(a1);

  // Calling convention for function specific ConstructStubs require
  // a2 to contain either an AllocationSite or undefined.
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ lw(t0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(t0, FieldMemOperand(t0, SharedFunctionInfo::kConstructStubOffset));
  __ Jump(at, t0, Code::kHeaderSize - kHeapObjectTag);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertBoundFunction(a1);

  // Load [[BoundArguments]] into a2 and length of that into t0.
  __ lw(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
  __ SmiUntag(t0);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- a3 : the new target (checked to be a constructor)
  //  -- t0 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ sll(t1, t0, kPointerSizeLog2);
    __ Subu(sp, sp, Operand(t1));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    __ LoadRoot(at, Heap::kRealStackLimitRootIndex);
    __ Branch(&done, gt, sp, Operand(at));  // Signed comparison.
    // Restore the stack pointer.
    __ Addu(sp, sp, Operand(t1));
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowStackOverflow);
    }
    __ bind(&done);
  }

  // Relocate arguments down the stack.
  {
    Label loop, done_loop;
    __ mov(t1, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, ge, t1, Operand(a0));
    __ Lsa(t2, sp, t0, kPointerSizeLog2);
    __ lw(at, MemOperand(t2));
    __ Lsa(t2, sp, t1, kPointerSizeLog2);
    __ sw(at, MemOperand(t2));
    __ Addu(t0, t0, Operand(1));
    __ Addu(t1, t1, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ SmiUntag(t0);
    __ Addu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Subu(t0, t0, Operand(1));
    __ Branch(&done_loop, lt, t0, Operand(zero_reg));
    __ Lsa(t1, a2, t0, kPointerSizeLog2);
    __ lw(at, MemOperand(t1));
    __ Lsa(t1, sp, a0, kPointerSizeLog2);
    __ sw(at, MemOperand(t1));
    __ Addu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label skip_load;
    __ Branch(&skip_load, ne, a1, Operand(a3));
    __ lw(a3, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&skip_load);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ lw(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (can be any Object)
  //  -- a3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor, non_proxy;
  __ JumpIfSmi(a1, &non_constructor);

  // Dispatch based on instance type.
  __ GetObjectType(a1, t1, t2);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));

  // Check if target has a [[Construct]] internal method.
  __ lbu(t3, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t3, t3, Operand(1 << Map::kIsConstructor));
  __ Branch(&non_constructor, eq, t3, Operand(zero_reg));

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_BOUND_FUNCTION_TYPE));

  // Only dispatch to proxies after checking whether they are constructors.
  __ Branch(&non_proxy, ne, t2, Operand(JS_PROXY_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
  {
    // Overwrite the original receiver with the (original) target.
    __ Lsa(at, sp, a0, kPointerSizeLog2);
    __ sw(a1, MemOperand(at));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, a1);
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
  //  -- a0 : requested object size (untagged)
  //  -- ra : return address
  // -----------------------------------
  __ SmiTag(a0);
  __ Push(a0);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : requested object size (untagged)
  //  -- ra : return address
  // -----------------------------------
  __ SmiTag(a0);
  __ Move(a1, Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ Push(a0, a1);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

// static
void Builtins::Generate_Abort(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : message_id as Smi
  //  -- ra : return address
  // -----------------------------------
  __ Push(a0);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbort);
}

// static
void Builtins::Generate_AbortJS(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : message as String object
  //  -- ra : return address
  // -----------------------------------
  __ Push(a0);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbortJS);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // State setup as expected by MacroAssembler::InvokePrologue.
  // ----------- S t a t e -------------
  //  -- a0: actual arguments count
  //  -- a1: function (passed through to callee)
  //  -- a2: expected arguments count
  //  -- a3: new target (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ Branch(&dont_adapt_arguments, eq, a2,
            Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  // We use Uless as the number of argument should always be greater than 0.
  __ Branch(&too_few, Uless, a0, Operand(a2));

  {  // Enough parameters: actual >= expected.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, a2, t1, at, &stack_overflow);

    // Calculate copy start address into a0 and copy end address into t1.
    __ Lsa(a0, fp, a0, kPointerSizeLog2 - kSmiTagSize);
    // Adjust for return address and receiver.
    __ Addu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address.
    __ sll(t1, a2, kPointerSizeLog2);
    __ subu(t1, a0, t1);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    // t1: copy end address

    Label copy;
    __ bind(&copy);
    __ lw(t0, MemOperand(a0));
    __ push(t0);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(t1));
    __ addiu(a0, a0, -kPointerSize);  // In delay slot.

    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, a2, t1, at, &stack_overflow);

    // Calculate copy start address into a0 and copy end address into t3.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ Lsa(a0, fp, a0, kPointerSizeLog2 - kSmiTagSize);
    // Adjust for return address and receiver.
    __ Addu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address. Also adjust for return address.
    __ Addu(t3, fp, kPointerSize);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    // t3: copy end address
    Label copy;
    __ bind(&copy);
    __ lw(t0, MemOperand(a0));  // Adjusted above for return addr and receiver.
    __ Subu(sp, sp, kPointerSize);
    __ Subu(a0, a0, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(t3));
    __ sw(t0, MemOperand(sp));  // In the delay slot.

    // Fill the remaining expected arguments with undefined.
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
    __ sll(t2, a2, kPointerSizeLog2);
    __ Subu(t1, fp, Operand(t2));
    // Adjust for frame.
    __ Subu(t1, t1, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                            2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ Subu(sp, sp, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &fill, ne, sp, Operand(t1));
    __ sw(t0, MemOperand(sp));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mov(a0, a2);
  // a0 : expected number of arguments
  // a1 : function (passed through to callee)
  // a3 : new target (passed through to callee)
  __ lw(t0, FieldMemOperand(a1, JSFunction::kCodeOffset));
  __ Call(t0, Code::kHeaderSize - kHeapObjectTag);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // -------------------------------------------
  // Don't adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ lw(t0, FieldMemOperand(a1, JSFunction::kCodeOffset));
  __ Jump(t0, Code::kHeaderSize - kHeapObjectTag);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ break_(0xCC);
  }
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    constexpr RegList gp_regs = Register::ListOf<a0, a1, a2, a3>();
    constexpr RegList fp_regs =
        DoubleRegister::ListOf<f2, f4, f6, f8, f10, f12, f14>();
    __ MultiPush(gp_regs);
    __ MultiPushFPU(fp_regs);

    __ Move(kContextRegister, Smi::kZero);
    __ CallRuntime(Runtime::kWasmCompileLazy);

    // Restore registers.
    __ MultiPopFPU(fp_regs);
    __ MultiPop(gp_regs);
  }
  // Now jump to the instructions of the returned code object.
  __ Jump(at, v0, Code::kHeaderSize - kHeapObjectTag);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS

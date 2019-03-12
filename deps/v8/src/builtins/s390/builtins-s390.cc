// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/api-arguments.h"
#include "src/code-factory.h"
#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/macro-assembler-inl.h"
#include "src/objects/cell.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-generator.h"
#include "src/objects/smi.h"
#include "src/register-configuration.h"
#include "src/runtime/runtime.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
  __ Move(kJavaScriptCallExtraArg1Register, ExternalReference::Create(address));
  if (exit_frame_type == BUILTIN_EXIT) {
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK(exit_frame_type == EXIT);
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithExitFrame),
            RelocInfo::CODE_TARGET);
  }
}

void Builtins::Generate_InternalArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

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
  __ Jump(BUILTIN_CODE(masm->isolate(), InternalArrayConstructorImpl),
          RelocInfo::CODE_TARGET);
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
  static_assert(kJavaScriptCallCodeStartRegister == r4, "ABI mismatch");
  __ JumpCodeObject(r4);
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
    __ PushRoot(RootIndex::kTheHoleValue);
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

void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                 Register scratch, Label* stack_overflow) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(scratch, RootIndex::kRealStackLimit);
  // Make scratch the space we have left. The stack might already be overflowed
  // here which will cause scratch to become negative.
  __ SubP(scratch, sp, scratch);
  // Check if the arguments will overflow the stack.
  __ ShiftLeftP(r0, num_args, Operand(kPointerSizeLog2));
  __ CmpP(scratch, r0);
  __ ble(stack_overflow);  // Signed comparison.
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
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
    __ PushRoot(RootIndex::kUndefinedValue);
    __ Push(r5);

    // ----------- S t a t e -------------
    //  --        sp[0*kPointerSize]: new target
    //  --        sp[1*kPointerSize]: padding
    //  -- r3 and sp[2*kPointerSize]: constructor function
    //  --        sp[3*kPointerSize]: number of arguments (tagged)
    //  --        sp[4*kPointerSize]: context
    // -----------------------------------

    __ LoadP(r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
    __ LoadlW(r6, FieldMemOperand(r6, SharedFunctionInfo::kFlagsOffset));
    __ TestBitMask(r6, SharedFunctionInfo::IsDerivedConstructorBit::kMask, r0);
    __ bne(&not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        r6, r7);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ b(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(r2, RootIndex::kTheHoleValue);

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

    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, r2, r7, &stack_overflow);
    __ b(&enough_stack_space);

    __ bind(&stack_overflow);
    // Restore the context from the frame.
    __ LoadP(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ bkpt(0);

    __ bind(&enough_stack_space);

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

    __ ltgr(r2, r2);
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
    Label use_receiver, do_throw, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(r2, RootIndex::kUndefinedValue, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(r2, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(r2, r6, r6, FIRST_JS_RECEIVER_TYPE);
    __ bge(&leave_frame);
    __ b(&use_receiver);

    __ bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ LoadP(r2, MemOperand(sp));
    __ JumpIfRoot(r2, RootIndex::kTheHoleValue, &do_throw);

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

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

static void GetSharedFunctionInfoBytecode(MacroAssembler* masm,
                                          Register sfi_data,
                                          Register scratch1) {
  Label done;

  __ CompareObjectType(sfi_data, scratch1, scratch1, INTERPRETER_DATA_TYPE);
  __ bne(&done, Label::kNear);
  __ LoadP(sfi_data,
           FieldMemOperand(sfi_data, InterpreterData::kBytecodeArrayOffset));
  __ bind(&done);
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
  __ Move(ip, debug_hook);
  __ LoadB(ip, MemOperand(ip));
  __ CmpSmiLiteral(ip, Smi::zero(), r0);
  __ bne(&prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.

  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());

  __ Move(ip, debug_suspended_generator);
  __ LoadP(ip, MemOperand(ip));
  __ CmpP(ip, r3);
  __ beq(&prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ CompareRoot(sp, RootIndex::kRealStackLimit);
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

  // Copy the function arguments from the generator object's register file.
  __ LoadP(r5, FieldMemOperand(r6, JSFunction::kSharedFunctionInfoOffset));
  __ LoadLogicalHalfWordP(
      r5, FieldMemOperand(r5, SharedFunctionInfo::kFormalParameterCountOffset));
  __ LoadP(r4, FieldMemOperand(
                   r3, JSGeneratorObject::kParametersAndRegistersOffset));
  {
    Label loop, done_loop;
    __ ShiftLeftP(r5, r5, Operand(kPointerSizeLog2));
    __ SubP(sp, r5);

    // ip = stack offset
    // r5 = parameter array offset
    __ LoadImmP(ip, Operand::Zero());
    __ SubP(r5, Operand(kPointerSize));
    __ blt(&done_loop);

    __ lgfi(r1, Operand(-kPointerSize));

    __ bind(&loop);

    // parameter copy loop
    __ LoadP(r0, FieldMemOperand(r4, r5, FixedArray::kHeaderSize));
    __ StoreP(r0, MemOperand(sp, ip));

    // update offsets
    __ lay(ip, MemOperand(ip, kPointerSize));

    __ BranchRelativeOnIdxHighP(r5, r1, &loop);

    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ LoadP(r5, FieldMemOperand(r6, JSFunction::kSharedFunctionInfoOffset));
    __ LoadP(r5, FieldMemOperand(r5, SharedFunctionInfo::kFunctionDataOffset));
    GetSharedFunctionInfoBytecode(masm, r5, ip);
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
    static_assert(kJavaScriptCallCodeStartRegister == r4, "ABI mismatch");
    __ LoadP(r4, FieldMemOperand(r3, JSFunction::kCodeOffset));
    __ JumpCodeObject(r4);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r3, r6);
    // Push hole as receiver since we do not use it for stepping.
    __ PushRoot(RootIndex::kTheHoleValue);
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

namespace {

constexpr int kPushedStackSpace =
    (kNumCalleeSaved + 2) * kPointerSize +
    kNumCalleeSavedDoubles * kDoubleSize + 5 * kPointerSize +
    EntryFrameConstants::kCallerFPOffset - kPointerSize;

// Called with the native C calling convention. The corresponding function
// signature is either:
//
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, Address new_target, Address target,
//       Address receiver, intptr_t argc, Address** args)>;
// or
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, MicrotaskQueue* microtask_queue)>;
void Generate_JSEntryVariant(MacroAssembler* masm, StackFrame::Type type,
                             Builtins::Name entry_trampoline) {
  // The register state is either:
  //   r2:                             root register value
  //   r3:                             code entry
  //   r4:                             function
  //   r5:                             receiver
  //   r6:                             argc
  //   [sp + 20 * kSystemPointerSize]: argv
  // or
  //   r2: root_register_value
  //   r3: microtask_queue

  Label invoke, handler_entry, exit;

  int pushed_stack_space = 0;
  {
    NoRootArrayScope no_root_array(masm);

    // saving floating point registers
    // 64bit ABI requires f8 to f15 be saved
    // http://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_zSeries.html
    __ lay(sp, MemOperand(sp, -8 * kDoubleSize));
    __ std(d8, MemOperand(sp));
    __ std(d9, MemOperand(sp, 1 * kDoubleSize));
    __ std(d10, MemOperand(sp, 2 * kDoubleSize));
    __ std(d11, MemOperand(sp, 3 * kDoubleSize));
    __ std(d12, MemOperand(sp, 4 * kDoubleSize));
    __ std(d13, MemOperand(sp, 5 * kDoubleSize));
    __ std(d14, MemOperand(sp, 6 * kDoubleSize));
    __ std(d15, MemOperand(sp, 7 * kDoubleSize));
    pushed_stack_space += kNumCalleeSavedDoubles * kDoubleSize;

    // zLinux ABI
    //    Incoming parameters:
    //          r2: root register value
    //          r3: code entry
    //          r4: function
    //          r5: receiver
    //          r6: argc
    // [sp + 20 * kSystemPointerSize]: argv
    //    Requires us to save the callee-preserved registers r6-r13
    //    General convention is to also save r14 (return addr) and
    //    sp/r15 as well in a single STM/STMG
    __ lay(sp, MemOperand(sp, -10 * kPointerSize));
    __ StoreMultipleP(r6, sp, MemOperand(sp, 0));
    pushed_stack_space += (kNumCalleeSaved + 2) * kPointerSize;

    // Initialize the root register.
    // C calling convention. The first argument is passed in r2.
    __ LoadRR(kRootRegister, r2);
  }

  // save r6 to r1
  __ LoadRR(r1, r6);

  // Push a frame with special values setup to mark it as an entry frame.
  //   Bad FP (-1)
  //   SMI Marker
  //   SMI Marker
  //   kCEntryFPAddress
  //   Frame type
  __ lay(sp, MemOperand(sp, -5 * kPointerSize));
  pushed_stack_space += 5 * kPointerSize;

  // Push a bad frame pointer to fail if it is used.
  __ LoadImmP(r9, Operand(-1));

  __ mov(r8, Operand(StackFrame::TypeToMarker(type)));
  __ mov(r7, Operand(StackFrame::TypeToMarker(type)));
  // Save copies of the top frame descriptor on the stack.
  __ Move(r6, ExternalReference::Create(
                 IsolateAddressId::kCEntryFPAddress, masm->isolate()));
  __ LoadP(r6, MemOperand(r6));
  __ StoreMultipleP(r6, r9, MemOperand(sp, kPointerSize));
  // Set up frame pointer for the frame to be pushed.
  // Need to add kPointerSize, because sp has one extra
  // frame already for the frame type being pushed later.
  __ lay(fp, MemOperand(
                 sp, -EntryFrameConstants::kCallerFPOffset + kPointerSize));
  pushed_stack_space += EntryFrameConstants::kCallerFPOffset - kPointerSize;

  // restore r6
  __ LoadRR(r6, r1);

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp =
      ExternalReference::Create(IsolateAddressId::kJSEntrySPAddress,
                                masm->isolate());
  __ Move(r7, js_entry_sp);
  __ LoadAndTestP(r8, MemOperand(r7));
  __ bne(&non_outermost_js, Label::kNear);
  __ StoreP(fp, MemOperand(r7));
  __ Load(ip, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont, Label::kNear);
  __ bind(&non_outermost_js);
  __ Load(ip, Operand(StackFrame::INNER_JSENTRY_FRAME));

  __ bind(&cont);
  __ StoreP(ip, MemOperand(sp));  // frame-type

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ b(&invoke, Label::kNear);

  __ bind(&handler_entry);

  // Store the current pc as the handler offset. It's used later to create the
  // handler table.
  masm->isolate()->builtins()->SetJSEntryHandlerOffset(handler_entry.pos());

  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushStackHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ Move(ip, ExternalReference::Create(
                 IsolateAddressId::kPendingExceptionAddress, masm->isolate()));

  __ StoreP(r2, MemOperand(ip));
  __ LoadRoot(r2, RootIndex::kException);
  __ b(&exit, Label::kNear);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  // Must preserve r2-r6.
  __ PushStackHandler();
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the b(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return.
  Handle<Code> trampoline_code =
      masm->isolate()->builtins()->builtin_handle(entry_trampoline);
  DCHECK_EQ(kPushedStackSpace, pushed_stack_space);
  __ Call(trampoline_code, RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();
  __ bind(&exit);  // r2 holds result

  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(r7);
  __ CmpP(r7, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ bne(&non_outermost_js_2, Label::kNear);
  __ mov(r8, Operand::Zero());
  __ Move(r7, js_entry_sp);
  __ StoreP(r8, MemOperand(r7));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(r5);
  __ Move(ip, ExternalReference::Create(
                 IsolateAddressId::kCEntryFPAddress, masm->isolate()));
  __ StoreP(r5, MemOperand(ip));

  // Reset the stack to the callee saved registers.
  __ lay(sp, MemOperand(sp, -EntryFrameConstants::kCallerFPOffset));

  // Reload callee-saved preserved regs, return address reg (r14) and sp
  __ LoadMultipleP(r6, sp, MemOperand(sp, 0));
  __ la(sp, MemOperand(sp, 10 * kPointerSize));

// saving floating point registers
#if V8_TARGET_ARCH_S390X
  // 64bit ABI requires f8 to f15 be saved
  __ ld(d8, MemOperand(sp));
  __ ld(d9, MemOperand(sp, 1 * kDoubleSize));
  __ ld(d10, MemOperand(sp, 2 * kDoubleSize));
  __ ld(d11, MemOperand(sp, 3 * kDoubleSize));
  __ ld(d12, MemOperand(sp, 4 * kDoubleSize));
  __ ld(d13, MemOperand(sp, 5 * kDoubleSize));
  __ ld(d14, MemOperand(sp, 6 * kDoubleSize));
  __ ld(d15, MemOperand(sp, 7 * kDoubleSize));
  __ la(sp, MemOperand(sp, 8 * kDoubleSize));
#else
  // 31bit ABI requires you to store f4 and f6:
  // http://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_s390.html#AEN417
  __ ld(d4, MemOperand(sp));
  __ ld(d6, MemOperand(sp, kDoubleSize));
  __ la(sp, MemOperand(sp, 2 * kDoubleSize));
#endif

  __ b(r14);
}

}  // namespace

void Builtins::Generate_JSEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY,
                          Builtins::kJSEntryTrampoline);
}

void Builtins::Generate_JSConstructEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::CONSTRUCT_ENTRY,
                          Builtins::kJSConstructEntryTrampoline);
}

void Builtins::Generate_JSRunMicrotasksEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY,
                          Builtins::kRunMicrotasksTrampoline);
}

// Clobbers scratch1 and scratch2; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        Register scratch1, Register scratch2) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(scratch1, RootIndex::kRealStackLimit);
  // Make scratch1 the space we have left. The stack might already be overflowed
  // here which will cause scratch1 to become negative.
  __ SubP(scratch1, sp, scratch1);
  // Check if the arguments will overflow the stack.
  __ ShiftLeftP(scratch2, argc, Operand(kPointerSizeLog2));
  __ CmpP(scratch1, scratch2);
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
  // [fp + kPushedStackSpace + 20 * kPointerSize]: argv
  // r0,r2,r7-r9, cp may be clobbered

  // Enter an internal frame.
  {
    // FrameScope ends up calling MacroAssembler::EnterFrame here
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address = ExternalReference::Create(
        IsolateAddressId::kContextAddress, masm->isolate());
    __ Move(cp, context_address);
    __ LoadP(cp, MemOperand(cp));

    // Push the function and the receiver onto the stack.
    __ Push(r4, r5);

    // Check if we have enough stack space to push all arguments.
    // Clobbers r5 and r0.
    Generate_CheckStackOverflow(masm, r6, r5, r0);

    // r3: new.target
    // r4: function
    // r6: argc
    // [fp + kPushedStackSpace + 20 * kPointerSize]: argv
    // r0,r2,r5,r7-r9, cp may be clobbered

    // Setup new.target, argc and function.
    __ LoadRR(r2, r6);
    __ LoadRR(r5, r3);
    __ LoadRR(r3, r4);

    // Load argv from the stack.
    __ LoadP(r6, MemOperand(fp));
    __ LoadP(r6, MemOperand(
                     r6, kPushedStackSpace + EntryFrameConstants::kArgvOffset));

    // r2: argc
    // r3: function
    // r5: new.target
    // r6: argv
    // r0,r4,r7-r9, cp may be clobbered

    // Copy arguments to the stack in a loop from argv to sp.
    // The arguments are actually placed in reverse order on sp
    // compared to argv (i.e. arg1 is highest memory in sp).
    // r2: argc
    // r3: function
    // r5: new.target
    // r6: argv, i.e. points to first arg
    // r7: scratch reg to hold scaled argc
    // r8: scratch reg to hold arg handle
    // r9: scratch reg to hold index into argv
    Label argLoop, argExit;
    intptr_t zero = 0;
    __ ShiftLeftP(r7, r2, Operand(kPointerSizeLog2));
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

    // r2: argc
    // r3: function
    // r5: new.target

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r4, RootIndex::kUndefinedValue);
    __ LoadRR(r6, r4);
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

void Builtins::Generate_RunMicrotasksTrampoline(MacroAssembler* masm) {
  // This expects two C++ function parameters passed by Invoke() in
  // execution.cc.
  //   r2: root_register_value
  //   r3: microtask_queue

  __ LoadRR(RunMicrotasksDescriptor::MicrotaskQueueRegister(), r3);
  __ Jump(BUILTIN_CODE(masm->isolate(), RunMicrotasks), RelocInfo::CODE_TARGET);
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

  Label optimized_code_slot_is_weak_ref, fallthrough;

  Register closure = r3;
  Register optimized_code_entry = scratch1;

  __ LoadP(
      optimized_code_entry,
      FieldMemOperand(feedback_vector, FeedbackVector::kOptimizedCodeOffset));

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret it as a weak reference to a code
  // object.
  __ JumpIfNotSmi(optimized_code_entry, &optimized_code_slot_is_weak_ref);

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
    // Optimized code slot is a weak reference.
    __ bind(&optimized_code_slot_is_weak_ref);

    __ LoadWeakValue(optimized_code_entry, optimized_code_entry, &fallthrough);

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
    static_assert(kJavaScriptCallCodeStartRegister == r4, "ABI mismatch");
    __ LoadCodeObjectEntry(r4, optimized_code_entry);
    __ Jump(r4);

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
// handlers do upon completion of the underlying operation. Will bail out to a
// label if the bytecode (without prefix) is a return bytecode.
static void AdvanceBytecodeOffsetOrReturn(MacroAssembler* masm,
                                          Register bytecode_array,
                                          Register bytecode_offset,
                                          Register bytecode, Register scratch1,
                                          Label* if_return) {
  Register bytecode_size_table = scratch1;
  Register scratch2 = bytecode;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode));
  __ Move(bytecode_size_table,
          ExternalReference::bytecode_size_table_address());

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  STATIC_ASSERT(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  STATIC_ASSERT(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ CmpP(bytecode, Operand(0x3));
  __ bgt(&process_bytecode);
  __ tmll(bytecode, Operand(0x1));
  __ bne(&extra_wide);

  // Load the next bytecode and update table to the wide scaled table.
  __ AddP(bytecode_offset, bytecode_offset, Operand(1));
  __ LoadlB(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ AddP(bytecode_size_table, bytecode_size_table,
          Operand(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ b(&process_bytecode);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ AddP(bytecode_offset, bytecode_offset, Operand(1));
  __ LoadlB(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ AddP(bytecode_size_table, bytecode_size_table,
          Operand(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));

  // Load the size of the current bytecode.
  __ bind(&process_bytecode);

// Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)                                           \
  __ CmpP(bytecode,                                                   \
          Operand(static_cast<int>(interpreter::Bytecode::k##NAME))); \
  __ beq(if_return);
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // Otherwise, load the size of the current bytecode and advance the offset.
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
  Register closure = r3;
  Register feedback_vector = r4;

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  __ LoadP(r2, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Load original bytecode array or the debug copy.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           FieldMemOperand(r2, SharedFunctionInfo::kFunctionDataOffset));
  GetSharedFunctionInfoBytecode(masm, kInterpreterBytecodeArrayRegister, r6);

  // The bytecode array could have been flushed from the shared function info,
  // if so, call into CompileLazy.
  Label compile_lazy;
  __ CompareObjectType(kInterpreterBytecodeArrayRegister, r2, no_reg,
                       BYTECODE_ARRAY_TYPE);
  __ bne(&compile_lazy);

  // Load the feedback vector from the closure.
  __ LoadP(feedback_vector,
           FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ LoadP(feedback_vector,
           FieldMemOperand(feedback_vector, Cell::kValueOffset));
  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, r6, r8, r7);

  // Increment invocation count for the function.
  __ LoadW(r1, FieldMemOperand(feedback_vector,
                               FeedbackVector::kInvocationCountOffset));
  __ AddP(r1, r1, Operand(1));
  __ StoreW(r1, FieldMemOperand(feedback_vector,
                                FeedbackVector::kInvocationCountOffset));

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(closure);

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
    __ LoadRoot(r0, RootIndex::kRealStackLimit);
    __ CmpLogicalP(r8, r0);
    __ bge(&ok);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    Label loop, no_args;
    __ LoadRoot(r8, RootIndex::kUndefinedValue);
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
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  __ LoadlB(r5, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftP(r5, r5, Operand(kPointerSizeLog2));
  __ LoadP(kJavaScriptCallCodeStartRegister,
           MemOperand(kInterpreterDispatchTableRegister, r5));
  __ Call(kJavaScriptCallCodeStartRegister);

  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadP(kInterpreterBytecodeOffsetRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Either return, or advance to the next bytecode and dispatch.
  Label do_return;
  __ LoadlB(r3, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, r3, r4,
                                &do_return);
  __ b(&do_dispatch);

  __ bind(&do_return);
  // The return value is in r2.
  LeaveInterpreterFrame(masm, r4);
  __ Ret();

  __ bind(&compile_lazy);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
  __ bkpt(0);  // Should not return.
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
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
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
    __ PushRoot(RootIndex::kUndefinedValue);
    __ LoadRR(r5, r2);  // Argument count is correct.
  }

  // Push the arguments.
  Generate_InterpreterPushArgs(masm, r5, r4, r5, r6);
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(r4);                   // Pass the spread in a register
    __ SubP(r2, r2, Operand(1));  // Subtract one for spread
  }

  // Call the target.
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
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
  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    __ AssertFunction(r3);

    // Tail call to the array construct stub (still in the caller
    // context at this point).
    Handle<Code> code = BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl);
    __ Jump(code, RelocInfo::CODE_TARGET);
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
  Label builtin_trampoline, trampoline_loaded;
  Smi interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::zero());

  // If the SFI function_data is an InterpreterData, the function will have a
  // custom copy of the interpreter entry trampoline for profiling. If so,
  // get the custom trampoline, otherwise grab the entry address of the global
  // trampoline.
  __ LoadP(r4, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadP(r4, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r4, FieldMemOperand(r4, SharedFunctionInfo::kFunctionDataOffset));
  __ CompareObjectType(r4, kInterpreterDispatchTableRegister,
                       kInterpreterDispatchTableRegister,
                       INTERPRETER_DATA_TYPE);
  __ bne(&builtin_trampoline);

  __ LoadP(r4,
           FieldMemOperand(r4, InterpreterData::kInterpreterTrampolineOffset));
  __ AddP(r4, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ b(&trampoline_loaded);

  __ bind(&builtin_trampoline);
  __ Move(r4, ExternalReference::
                  address_of_interpreter_entry_trampoline_instruction_start(
                      masm->isolate()));
  __ LoadP(r4, MemOperand(r4));

  __ bind(&trampoline_loaded);
  __ AddP(r14, r4, Operand(interpreter_entry_return_pc_offset->value()));

  // Initialize the dispatch table register.
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

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
  __ LoadlB(ip, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftP(ip, ip, Operand(kPointerSizeLog2));
  __ LoadP(kJavaScriptCallCodeStartRegister,
           MemOperand(kInterpreterDispatchTableRegister, ip));
  __ Jump(kJavaScriptCallCodeStartRegister);
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
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, r3, r4,
                                &if_return);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(r4, kInterpreterBytecodeOffsetRegister);
  __ StoreP(r4,
            MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

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
        __ PushRoot(RootIndex::kUndefinedValue);
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
  static_assert(kJavaScriptCallCodeStartRegister == r4, "ABI mismatch");
  __ LoadP(r4, FieldMemOperand(r3, JSFunction::kCodeOffset));
  __ JumpCodeObject(r4);
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

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ LoadP(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r2, MemOperand(r2, JavaScriptFrameConstants::kFunctionOffset));

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(r2);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  Label skip;
  __ CmpSmiLiteral(r2, Smi::zero(), r0);
  __ bne(&skip);
  __ Ret();

  __ bind(&skip);

  // Drop the handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  __ LeaveFrame(StackFrame::STUB);

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
    __ LoadRoot(scratch, RootIndex::kUndefinedValue);
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
  __ JumpIfRoot(r4, RootIndex::kNullValue, &no_arguments);
  __ JumpIfRoot(r4, RootIndex::kUndefinedValue, &no_arguments);

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
    __ PushRoot(RootIndex::kUndefinedValue);
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
    __ LoadRoot(r3, RootIndex::kUndefinedValue);
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
    __ LoadRoot(r3, RootIndex::kUndefinedValue);
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
  __ Push(Smi::zero());  // Padding.
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

  Register scratch = ip;

  if (masm->emit_debug_code()) {
    // Allow r4 to be a FixedArray, or a FixedDoubleArray if r6 == 0.
    Label ok, fail;
    __ AssertNotSmi(r4);
    __ LoadP(scratch, FieldMemOperand(r4, HeapObject::kMapOffset));
    __ LoadHalfWordP(scratch,
                     FieldMemOperand(scratch, Map::kInstanceTypeOffset));
    __ CmpP(scratch, Operand(FIXED_ARRAY_TYPE));
    __ beq(&ok);
    __ CmpP(scratch, Operand(FIXED_DOUBLE_ARRAY_TYPE));
    __ bne(&fail);
    __ CmpP(r6, Operand::Zero());
    __ beq(&ok);
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  // Check for stack overflow.
  Label stack_overflow;
  Generate_StackOverflowCheck(masm, r6, ip, &stack_overflow);

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
    __ CompareRoot(ip, RootIndex::kTheHoleValue);
    __ bne(&skip, Label::kNear);
    __ LoadRoot(ip, RootIndex::kUndefinedValue);
    __ bind(&skip);
    __ push(ip);
    __ BranchOnCount(r1, &loop);
    __ bind(&no_args);
    __ AddP(r2, r2, r6);
  }

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);

  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
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
    __ LoadLogicalHalfWordP(
        r7,
        FieldMemOperand(r7, SharedFunctionInfo::kFormalParameterCountOffset));
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
  __ LoadlW(r5, FieldMemOperand(r4, SharedFunctionInfo::kFlagsOffset));
  __ TestBitMask(r5, SharedFunctionInfo::IsClassConstructorBit::kMask, r0);
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
        __ JumpIfRoot(r5, RootIndex::kUndefinedValue, &convert_global_proxy);
        __ JumpIfNotRoot(r5, RootIndex::kNullValue, &convert_to_object);
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

  __ LoadLogicalHalfWordP(
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
      __ CompareRoot(sp, RootIndex::kRealStackLimit);
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
  __ AssertConstructor(r3, r1);
  __ AssertFunction(r3);

  // Calling convention for function specific ConstructStubs require
  // r4 to contain either an AllocationSite or undefined.
  __ LoadRoot(r4, RootIndex::kUndefinedValue);

  Label call_generic_stub;

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  __ LoadP(r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadlW(r6, FieldMemOperand(r6, SharedFunctionInfo::kFlagsOffset));
  __ AndP(r6, Operand(SharedFunctionInfo::ConstructAsBuiltinBit::kMask));
  __ beq(&call_generic_stub);

  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET);

  __ bind(&call_generic_stub);
  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the function to call (checked to be a JSBoundFunction)
  //  -- r5 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(r3, r1);
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

  // Check if target has a [[Construct]] internal method.
  __ LoadP(r6, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadlB(r4, FieldMemOperand(r6, Map::kBitFieldOffset));
  __ TestBit(r4, Map::IsConstructorBit::kShift);
  __ beq(&non_constructor);

  // Dispatch based on instance type.
  __ CompareInstanceType(r6, r7, JS_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, eq);

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

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : actual number of arguments
  //  -- r3 : function (passed through to callee)
  //  -- r4 : expected number of arguments
  //  -- r5 : new target (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ tmll(r4, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ b(Condition(1), &dont_adapt_arguments);
  __ CmpLogicalP(r2, r4);
  __ blt(&too_few);

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r4, r7, &stack_overflow);

    // Calculate copy start address into r2 and copy end address into r6.
    // r2: actual number of arguments as a smi
    // r3: function
    // r4: expected number of arguments
    // r5: new target (passed through to callee)
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
    __ SmiToPtrArrayOffset(r2, r2);
    __ lay(r2, MemOperand(r2, fp));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r2: copy start address
    // r3: function
    // r4: expected number of arguments
    // r5: new target (passed through to callee)
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
    __ LoadRoot(r0, RootIndex::kUndefinedValue);
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
  static_assert(kJavaScriptCallCodeStartRegister == r4, "ABI mismatch");
  __ LoadP(r4, FieldMemOperand(r3, JSFunction::kCodeOffset));
  __ CallCodeObject(r4);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  static_assert(kJavaScriptCallCodeStartRegister == r4, "ABI mismatch");
  __ LoadP(r4, FieldMemOperand(r3, JSFunction::kCodeOffset));
  __ JumpCodeObject(r4);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bkpt(0);
  }
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was put in a register by the jump table trampoline.
  // Convert to Smi for the runtime call.
  __ SmiTag(kWasmCompileLazyFuncIndexRegister,
            kWasmCompileLazyFuncIndexRegister);
  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameAndConstantPoolScope scope(masm, StackFrame::WASM_COMPILE_LAZY);

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

    // Pass instance and function index as explicit arguments to the runtime
    // function.
    __ Push(kWasmInstanceRegister, r7);
    // Load the correct CEntry builtin from the instance object.
    __ LoadP(r4, FieldMemOperand(kWasmInstanceRegister,
                                 WasmInstanceObject::kCEntryStubOffset));
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ LoadSmiLiteral(cp, Smi::zero());
    __ CallRuntimeWithCEntry(Runtime::kWasmCompileLazy, r4);
    // The entrypoint address is the return value.
    __ LoadRR(ip, r2);

    // Restore registers.
    __ MultiPopDoubles(fp_regs);
    __ MultiPop(gp_regs);
  }
  // Finally, jump to the entrypoint.
  __ Jump(ip);
}

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               SaveFPRegsMode save_doubles, ArgvMode argv_mode,
                               bool builtin_exit_frame) {
  // Called from JavaScript; parameters are on stack as if calling JS function.
  // r2: number of arguments including receiver
  // r3: pointer to builtin function
  // fp: frame pointer  (restored after C call)
  // sp: stack pointer  (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)
  //
  // If argv_mode == kArgvInRegister:
  // r4: pointer to the first argument

  __ LoadRR(r7, r3);

  if (argv_mode == kArgvInRegister) {
    // Move argv into the correct register.
    __ LoadRR(r3, r4);
  } else {
    // Compute the argv pointer.
    __ ShiftLeftP(r3, r2, Operand(kPointerSizeLog2));
    __ lay(r3, MemOperand(r3, sp, -kPointerSize));
  }

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);

  // Need at least one extra slot for return address location.
  int arg_stack_space = 1;

  // Pass buffer for return value on stack if necessary
  bool needs_return_buffer =
      result_size == 2 && !ABI_RETURNS_OBJECTPAIR_IN_REGS;
  if (needs_return_buffer) {
    arg_stack_space += result_size;
  }

#if V8_TARGET_ARCH_S390X
  // 64-bit linux pass Argument object by reference not value
  arg_stack_space += 2;
#endif

  __ EnterExitFrame(
      save_doubles, arg_stack_space,
      builtin_exit_frame ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);

  // Store a copy of argc, argv in callee-saved registers for later.
  __ LoadRR(r6, r2);
  __ LoadRR(r8, r3);
  // r2, r6: number of arguments including receiver  (C callee-saved)
  // r3, r8: pointer to the first argument
  // r7: pointer to builtin function  (C callee-saved)

  // Result returned in registers or stack, depending on result size and ABI.

  Register isolate_reg = r4;
  if (needs_return_buffer) {
    // The return value is 16-byte non-scalar value.
    // Use frame storage reserved by calling function to pass return
    // buffer as implicit first argument in R2.  Shfit original parameters
    // by one register each.
    __ LoadRR(r4, r3);
    __ LoadRR(r3, r2);
    __ la(r2, MemOperand(sp, (kStackFrameExtraParamSlot + 1) * kPointerSize));
    isolate_reg = r5;
    // Clang doesn't preserve r2 (result buffer)
    // write to r8 (preserved) before entry
    __ LoadRR(r8, r2);
  }
  // Call C built-in.
  __ Move(isolate_reg, ExternalReference::isolate_address(masm->isolate()));

  __ StoreReturnAddressAndCall(r7);

  // If return value is on the stack, pop it to registers.
  if (needs_return_buffer) {
    __ LoadRR(r2, r8);
    __ LoadP(r3, MemOperand(r2, kPointerSize));
    __ LoadP(r2, MemOperand(r2));
  }

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(r2, RootIndex::kException);
  __ beq(&exception_returned, Label::kNear);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    ExternalReference pending_exception_address = ExternalReference::Create(
        IsolateAddressId::kPendingExceptionAddress, masm->isolate());
    __ Move(r1, pending_exception_address);
    __ LoadP(r1, MemOperand(r1));
    __ CompareRoot(r1, RootIndex::kTheHoleValue);
    // Cannot use check here as it attempts to generate call into runtime.
    __ beq(&okay, Label::kNear);
    __ stop("Unexpected pending exception");
    __ bind(&okay);
  }

  // Exit C frame and return.
  // r2:r3: result
  // sp: stack pointer
  // fp: frame pointer
  Register argc = argv_mode == kArgvInRegister
                      // We don't want to pop arguments so set argc to no_reg.
                      ? no_reg
                      // r6: still holds argc (callee-saved).
                      : r6;
  __ LeaveExitFrame(save_doubles, argc);
  __ b(r14);

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

  // Ask the runtime for help to determine the handler. This will set r3 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler =
      ExternalReference::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, 0, r2);
    __ LoadImmP(r2, Operand::Zero());
    __ LoadImmP(r3, Operand::Zero());
    __ Move(r4, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ Move(cp, pending_handler_context_address);
  __ LoadP(cp, MemOperand(cp));
  __ Move(sp, pending_handler_sp_address);
  __ LoadP(sp, MemOperand(sp));
  __ Move(fp, pending_handler_fp_address);
  __ LoadP(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label skip;
  __ CmpP(cp, Operand::Zero());
  __ beq(&skip, Label::kNear);
  __ StoreP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ bind(&skip);

  // Reset the masking register. This is done independent of the underlying
  // feature flag {FLAG_untrusted_code_mitigations} to make the snapshot work
  // with both configurations. It is safe to always do this, because the
  // underlying register is caller-saved and can be arbitrarily clobbered.
  __ ResetSpeculationPoisonRegister();

  // Compute the handler entry address and jump to it.
  __ Move(r3, pending_handler_entrypoint_address);
  __ LoadP(r3, MemOperand(r3));
  __ Jump(r3);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label out_of_range, only_low, negate, done, fastpath_done;
  Register result_reg = r2;

  HardAbortScope hard_abort(masm);  // Avoid calls to Abort.

  // Immediate values for this stub fit in instructions, so it's safe to use ip.
  Register scratch = GetRegisterThatIsNotOneOf(result_reg);
  Register scratch_low = GetRegisterThatIsNotOneOf(result_reg, scratch);
  Register scratch_high =
      GetRegisterThatIsNotOneOf(result_reg, scratch, scratch_low);
  DoubleRegister double_scratch = kScratchDoubleReg;

  __ Push(result_reg, scratch);
  // Account for saved regs.
  int argument_offset = 2 * kPointerSize;

  // Load double input.
  __ LoadDouble(double_scratch, MemOperand(sp, argument_offset));

  // Do fast-path convert from double to int.
  __ ConvertDoubleToInt64(result_reg, double_scratch);

  // Test for overflow
  __ TestIfInt32(result_reg);
  __ beq(&fastpath_done, Label::kNear);

  __ Push(scratch_high, scratch_low);
  // Account for saved regs.
  argument_offset += 2 * kPointerSize;

  __ LoadlW(scratch_high,
            MemOperand(sp, argument_offset + Register::kExponentOffset));
  __ LoadlW(scratch_low,
            MemOperand(sp, argument_offset + Register::kMantissaOffset));

  __ ExtractBitMask(scratch, scratch_high, HeapNumber::kExponentMask);
  // Load scratch with exponent - 1. This is faster than loading
  // with exponent because Bias + 1 = 1024 which is a *S390* immediate value.
  STATIC_ASSERT(HeapNumber::kExponentBias + 1 == 1024);
  __ SubP(scratch, Operand(HeapNumber::kExponentBias + 1));
  // If exponent is greater than or equal to 84, the 32 less significant
  // bits are 0s (2^84 = 1, 52 significant bits, 32 uncoded bits),
  // the result is 0.
  // Compare exponent with 84 (compare exponent - 1 with 83).
  __ CmpP(scratch, Operand(83));
  __ bge(&out_of_range, Label::kNear);

  // If we reach this code, 31 <= exponent <= 83.
  // So, we don't have to handle cases where 0 <= exponent <= 20 for
  // which we would need to shift right the high part of the mantissa.
  // Scratch contains exponent - 1.
  // Load scratch with 52 - exponent (load with 51 - (exponent - 1)).
  __ Load(r0, Operand(51));
  __ SubP(scratch, r0, scratch);
  __ CmpP(scratch, Operand::Zero());
  __ ble(&only_low, Label::kNear);
  // 21 <= exponent <= 51, shift scratch_low and scratch_high
  // to generate the result.
  __ ShiftRight(scratch_low, scratch_low, scratch);
  // Scratch contains: 52 - exponent.
  // We needs: exponent - 20.
  // So we use: 32 - scratch = 32 - 52 + exponent = exponent - 20.
  __ Load(r0, Operand(32));
  __ SubP(scratch, r0, scratch);
  __ ExtractBitMask(result_reg, scratch_high, HeapNumber::kMantissaMask);
  // Set the implicit 1 before the mantissa part in scratch_high.
  STATIC_ASSERT(HeapNumber::kMantissaBitsInTopWord >= 16);
  __ Load(r0, Operand(1 << ((HeapNumber::kMantissaBitsInTopWord)-16)));
  __ ShiftLeftP(r0, r0, Operand(16));
  __ OrP(result_reg, result_reg, r0);
  __ ShiftLeft(r0, result_reg, scratch);
  __ OrP(result_reg, scratch_low, r0);
  __ b(&negate, Label::kNear);

  __ bind(&out_of_range);
  __ mov(result_reg, Operand::Zero());
  __ b(&done, Label::kNear);

  __ bind(&only_low);
  // 52 <= exponent <= 83, shift only scratch_low.
  // On entry, scratch contains: 52 - exponent.
  __ LoadComplementRR(scratch, scratch);
  __ ShiftLeft(result_reg, scratch_low, scratch);

  __ bind(&negate);
  // If input was positive, scratch_high ASR 31 equals 0 and
  // scratch_high LSR 31 equals zero.
  // New result = (result eor 0) + 0 = result.
  // If the input was negative, we have to negate the result.
  // Input_high ASR 31 equals 0xFFFFFFFF and scratch_high LSR 31 equals 1.
  // New result = (result eor 0xFFFFFFFF) + 1 = 0 - result.
  __ ShiftRightArith(r0, scratch_high, Operand(31));
#if V8_TARGET_ARCH_S390X
  __ lgfr(r0, r0);
  __ ShiftRightP(r0, r0, Operand(32));
#endif
  __ XorP(result_reg, r0);
  __ ShiftRight(r0, scratch_high, Operand(31));
  __ AddP(result_reg, r0);

  __ bind(&done);
  __ Pop(scratch_high, scratch_low);
  argument_offset -= 2 * kPointerSize;

  __ bind(&fastpath_done);
  __ StoreP(result_reg, MemOperand(sp, argument_offset));
  __ Pop(result_reg, scratch);

  __ Ret();
}

void Builtins::Generate_MathPowInternal(MacroAssembler* masm) {
  const Register exponent = r4;
  const DoubleRegister double_base = d1;
  const DoubleRegister double_exponent = d2;
  const DoubleRegister double_result = d3;
  const DoubleRegister double_scratch = d0;
  const Register scratch = r1;
  const Register scratch2 = r9;

  Label call_runtime, done, int_exponent;

  // Detect integer exponents stored as double.
  __ TryDoubleToInt32Exact(scratch, double_exponent, scratch2, double_scratch);
  __ beq(&int_exponent, Label::kNear);

  __ push(r14);
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ PrepareCallCFunction(0, 2, scratch);
    __ MovToFloatParameters(double_base, double_exponent);
    __ CallCFunction(ExternalReference::power_double_double_function(), 0, 2);
  }
  __ pop(r14);
  __ MovFromFloatResult(double_result);
  __ b(&done);

  // Calculate power with integer exponent.
  __ bind(&int_exponent);

  // Get two copies of exponent in the registers scratch and exponent.
  // Exponent has previously been stored into scratch as untagged integer.
  __ LoadRR(exponent, scratch);

  __ ldr(double_scratch, double_base);  // Back up base.
  __ LoadImmP(scratch2, Operand(1));
  __ ConvertIntToDouble(double_result, scratch2);

  // Get absolute value of exponent.
  Label positive_exponent;
  __ CmpP(scratch, Operand::Zero());
  __ bge(&positive_exponent, Label::kNear);
  __ LoadComplementRR(scratch, scratch);
  __ bind(&positive_exponent);

  Label while_true, no_carry, loop_end;
  __ bind(&while_true);
  __ mov(scratch2, Operand(1));
  __ AndP(scratch2, scratch);
  __ beq(&no_carry, Label::kNear);
  __ mdbr(double_result, double_scratch);
  __ bind(&no_carry);
  __ ShiftRightP(scratch, scratch, Operand(1));
  __ LoadAndTestP(scratch, scratch);
  __ beq(&loop_end, Label::kNear);
  __ mdbr(double_scratch, double_scratch);
  __ b(&while_true);
  __ bind(&loop_end);

  __ CmpP(exponent, Operand::Zero());
  __ bge(&done);

  // get 1/double_result:
  __ ldr(double_scratch, double_result);
  __ LoadImmP(scratch2, Operand(1));
  __ ConvertIntToDouble(double_result, scratch2);
  __ ddbr(double_result, double_scratch);

  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ lzdr(kDoubleRegZero);
  __ cdbr(double_result, kDoubleRegZero);
  __ bne(&done, Label::kNear);
  // double_exponent may not containe the exponent value if the input was a
  // smi.  We set it with exponent value before bailing out.
  __ ConvertIntToDouble(double_exponent, exponent);

  // Returning or bailing out.
  __ push(r14);
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ PrepareCallCFunction(0, 2, scratch);
    __ MovToFloatParameters(double_base, double_exponent);
    __ CallCFunction(ExternalReference::power_double_double_function(), 0, 2);
  }
  __ pop(r14);
  __ MovFromFloatResult(double_result);

  __ bind(&done);
  __ Ret();
}

void Builtins::Generate_InternalArrayConstructorImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : argc
  //  -- r3 : constructor
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ LoadP(r5, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    __ TestIfSmi(r5);
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r5, r5, r6, MAP_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedInitialMapForArrayFunction);

    // Figure out the right elements kind
    __ LoadP(r5, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    // Load the map's "bit field 2" into |result|.
    __ LoadlB(r5, FieldMemOperand(r5, Map::kBitField2Offset));
    // Retrieve elements_kind from bit field 2.
    __ DecodeField<Map::ElementsKindBits>(r5);

    __ CmpP(r5, Operand(PACKED_ELEMENTS));
    __ Assert(eq, AbortReason::kInvalidElementsKindForInternalPackedArray);
  }

  __ Jump(
      BUILTIN_CODE(masm->isolate(), InternalArrayNoArgumentConstructor_Packed),
      RelocInfo::CODE_TARGET);
}

namespace {

static int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  return ref0.address() - ref1.address();
}

// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Restores context.  stack_space
// - space to be unwound on exit (includes the call JS arguments space and
// the additional space allocated for the fast call).
static void CallApiFunctionAndReturn(MacroAssembler* masm,
                                     Register function_address,
                                     ExternalReference thunk_ref,
                                     int stack_space,
                                     MemOperand* stack_space_operand,
                                     MemOperand return_value_operand) {
  Isolate* isolate = masm->isolate();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(isolate), next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(isolate), next_address);

  // Additional parameter is the address of the actual callback.
  DCHECK(function_address == r3 || function_address == r4);
  Register scratch = r5;

  __ Move(scratch, ExternalReference::is_profiling_address(isolate));
  __ LoadlB(scratch, MemOperand(scratch, 0));
  __ CmpP(scratch, Operand::Zero());

  Label profiler_disabled;
  Label end_profiler_check;
  __ beq(&profiler_disabled, Label::kNear);
  __ Move(scratch, thunk_ref);
  __ b(&end_profiler_check, Label::kNear);
  __ bind(&profiler_disabled);
  __ LoadRR(scratch, function_address);
  __ bind(&end_profiler_check);

  // Allocate HandleScope in callee-save registers.
  // r9 - next_address
  // r6 - next_address->kNextOffset
  // r7 - next_address->kLimitOffset
  // r8 - next_address->kLevelOffset
  __ Move(r9, next_address);
  __ LoadP(r6, MemOperand(r9, kNextOffset));
  __ LoadP(r7, MemOperand(r9, kLimitOffset));
  __ LoadlW(r8, MemOperand(r9, kLevelOffset));
  __ AddP(r8, Operand(1));
  __ StoreW(r8, MemOperand(r9, kLevelOffset));

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, r2);
    __ Move(r2, ExternalReference::isolate_address(isolate));
    __ CallCFunction(ExternalReference::log_enter_external_function(), 1);
    __ PopSafepointRegisters();
  }

  __ StoreReturnAddressAndCall(scratch);

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, r2);
    __ Move(r2, ExternalReference::isolate_address(isolate));
    __ CallCFunction(ExternalReference::log_leave_external_function(), 1);
    __ PopSafepointRegisters();
  }

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // load value from ReturnValue
  __ LoadP(r2, return_value_operand);
  __ bind(&return_value_loaded);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ StoreP(r6, MemOperand(r9, kNextOffset));
  if (__ emit_debug_code()) {
    __ LoadlW(r3, MemOperand(r9, kLevelOffset));
    __ CmpP(r3, r8);
    __ Check(eq, AbortReason::kUnexpectedLevelAfterReturnFromApiCall);
  }
  __ SubP(r8, Operand(1));
  __ StoreW(r8, MemOperand(r9, kLevelOffset));
  __ CmpP(r7, MemOperand(r9, kLimitOffset));
  __ bne(&delete_allocated_handles, Label::kNear);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  // LeaveExitFrame expects unwind space to be in a register.
  if (stack_space_operand == nullptr) {
    DCHECK_NE(stack_space, 0);
    __ mov(r6, Operand(stack_space));
  } else {
    DCHECK_EQ(stack_space, 0);
    __ LoadP(r6, *stack_space_operand);
  }
  __ LeaveExitFrame(false, r6, stack_space_operand != nullptr);

  // Check if the function scheduled an exception.
  __ Move(r7, ExternalReference::scheduled_exception_address(isolate));
  __ LoadP(r7, MemOperand(r7));
  __ CompareRoot(r7, RootIndex::kTheHoleValue);
  __ bne(&promote_scheduled_exception, Label::kNear);

  __ b(r14);

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ StoreP(r7, MemOperand(r9, kLimitOffset));
  __ LoadRR(r6, r2);
  __ PrepareCallCFunction(1, r7);
  __ Move(r2, ExternalReference::isolate_address(isolate));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(), 1);
  __ LoadRR(r2, r6);
  __ b(&leave_exit_frame, Label::kNear);
}

}  // namespace

void Builtins::Generate_CallApiCallback(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- cp                  : kTargetContext
  //  -- r3                  : kApiFunctionAddress
  //  -- r4                  : kArgc
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1) * 4]  : first argument
  //  -- sp[(argc + 0) * 4]  : receiver
  //  -- sp[(argc + 1) * 4]  : kHolder
  //  -- sp[(argc + 2) * 4]  : kCallData
  // -----------------------------------

  Register api_function_address = r3;
  Register argc = r4;
  Register scratch = r6;
  Register index = r7;  // For indexing MemOperands.

  DCHECK(!AreAliased(api_function_address, argc, scratch, index));

  // Stack offsets (without argc).
  static constexpr int kReceiverOffset = 0;
  static constexpr int kHolderOffset = kReceiverOffset + 1;
  static constexpr int kCallDataOffset = kHolderOffset + 1;

  // Extra stack arguments are: the receiver, kHolder, kCallData.
  static constexpr int kExtraStackArgumentCount = 3;

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kArgsLength == 6);
  STATIC_ASSERT(FCA::kNewTargetIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);

  // Set up FunctionCallbackInfo's implicit_args on the stack as follows:
  //
  // Target state:
  //   sp[0 * kPointerSize]: kHolder
  //   sp[1 * kPointerSize]: kIsolate
  //   sp[2 * kPointerSize]: undefined (kReturnValueDefaultValue)
  //   sp[3 * kPointerSize]: undefined (kReturnValue)
  //   sp[4 * kPointerSize]: kData
  //   sp[5 * kPointerSize]: undefined (kNewTarget)

  // Reserve space on the stack.
  __ lay(sp, MemOperand(sp, -(FCA::kArgsLength * kPointerSize)));

  // kHolder.
  __ AddP(index, argc, Operand(FCA::kArgsLength + kHolderOffset));
  __ ShiftLeftP(r1, index, Operand(kPointerSizeLog2));
  __ LoadP(scratch, MemOperand(sp, r1));
  __ StoreP(scratch, MemOperand(sp, 0 * kPointerSize));

  // kIsolate.
  __ Move(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ StoreP(scratch, MemOperand(sp, 1 * kPointerSize));

  // kReturnValueDefaultValue, kReturnValue, and kNewTarget.
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ StoreP(scratch, MemOperand(sp, 2 * kPointerSize));
  __ StoreP(scratch, MemOperand(sp, 3 * kPointerSize));
  __ StoreP(scratch, MemOperand(sp, 5 * kPointerSize));

  // kData.
  __ AddP(index, argc, Operand(FCA::kArgsLength + kCallDataOffset));
  __ ShiftLeftP(r1, index, Operand(kPointerSizeLog2));
  __ LoadP(scratch, MemOperand(sp, r1));
  __ StoreP(scratch, MemOperand(sp, 4 * kPointerSize));

  // Keep a pointer to kHolder (= implicit_args) in a scratch register.
  // We use it below to set up the FunctionCallbackInfo object.
  __ LoadRR(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  // S390 LINUX ABI:
  //
  // Create 4 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1-3] FunctionCallbackInfo
  //    [4] number of bytes to drop from the stack after returning
  static constexpr int kApiStackSpace = 5;
  static constexpr bool kDontSaveDoubles = false;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(kDontSaveDoubles, kApiStackSpace);

  // FunctionCallbackInfo::implicit_args_ (points at kHolder as set up above).
  // Arguments are after the return address (pushed by EnterExitFrame()).
  __ StoreP(scratch,
            MemOperand(sp, (kStackFrameExtraParamSlot + 1) * kPointerSize));

  // FunctionCallbackInfo::values_ (points at the first varargs argument passed
  // on the stack).
  __ AddP(scratch, scratch, Operand((FCA::kArgsLength - 1) * kPointerSize));
  __ ShiftLeftP(r1, argc, Operand(kPointerSizeLog2));
  __ AddP(scratch, scratch, r1);
  __ StoreP(scratch,
            MemOperand(sp, (kStackFrameExtraParamSlot + 2) * kPointerSize));

  // FunctionCallbackInfo::length_.
  __ StoreW(argc,
            MemOperand(sp, (kStackFrameExtraParamSlot + 3) * kPointerSize));

  // We also store the number of bytes to drop from the stack after returning
  // from the API function here.
  __ mov(scratch,
         Operand((FCA::kArgsLength + kExtraStackArgumentCount) * kPointerSize));
  __ ShiftLeftP(r1, argc, Operand(kPointerSizeLog2));
  __ AddP(scratch, r1);
  __ StoreP(scratch,
            MemOperand(sp, (kStackFrameExtraParamSlot + 4) * kPointerSize));

  // v8::InvocationCallback's argument.
  __ lay(r2,
         MemOperand(sp, (kStackFrameExtraParamSlot + 1) * kPointerSize));

  ExternalReference thunk_ref = ExternalReference::invoke_function_callback();

  // There are two stack slots above the arguments we constructed on the stack.
  // TODO(jgruber): Document what these arguments are.
  static constexpr int kStackSlotsAboveFCA = 2;
  MemOperand return_value_operand(
      fp, (kStackSlotsAboveFCA + FCA::kReturnValueOffset) * kPointerSize);

  static constexpr int kUseStackSpaceOperand = 0;
  MemOperand stack_space_operand(
      sp, (kStackFrameExtraParamSlot + 4) * kPointerSize);

  AllowExternalCallThatCantCauseGC scope(masm);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kUseStackSpaceOperand, &stack_space_operand,
                           return_value_operand);
}

void Builtins::Generate_CallApiGetter(MacroAssembler* masm) {
  int arg0Slot = 0;
  int accessorInfoSlot = 0;
  int apiStackSpace = 0;
  // Build v8::PropertyCallbackInfo::args_ array on the stack and push property
  // name below the exit frame to make GC aware of them.
  STATIC_ASSERT(PropertyCallbackArguments::kShouldThrowOnErrorIndex == 0);
  STATIC_ASSERT(PropertyCallbackArguments::kHolderIndex == 1);
  STATIC_ASSERT(PropertyCallbackArguments::kIsolateIndex == 2);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueDefaultValueIndex == 3);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueOffset == 4);
  STATIC_ASSERT(PropertyCallbackArguments::kDataIndex == 5);
  STATIC_ASSERT(PropertyCallbackArguments::kThisIndex == 6);
  STATIC_ASSERT(PropertyCallbackArguments::kArgsLength == 7);

  Register receiver = ApiGetterDescriptor::ReceiverRegister();
  Register holder = ApiGetterDescriptor::HolderRegister();
  Register callback = ApiGetterDescriptor::CallbackRegister();
  Register scratch = r6;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  Register api_function_address = r4;

  __ push(receiver);
  // Push data from AccessorInfo.
  __ LoadP(scratch, FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ push(scratch);
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ Push(scratch, scratch);
  __ Move(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ Push(scratch, holder);
  __ Push(Smi::zero());  // should_throw_on_error -> false
  __ LoadP(scratch, FieldMemOperand(callback, AccessorInfo::kNameOffset));
  __ push(scratch);

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ LoadRR(r2, sp);                           // r2 = Handle<Name>
  __ AddP(r3, r2, Operand(1 * kPointerSize));  // r3 = v8::PCI::args_

  // If ABI passes Handles (pointer-sized struct) in a register:
  //
  // Create 2 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1] AccessorInfo&
  //
  // Otherwise:
  //
  // Create 3 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1] copy of Handle (first arg)
  //    [2] AccessorInfo&
  if (ABI_PASSES_HANDLES_IN_REGS) {
    accessorInfoSlot = kStackFrameExtraParamSlot + 1;
    apiStackSpace = 2;
  } else {
    arg0Slot = kStackFrameExtraParamSlot + 1;
    accessorInfoSlot = arg0Slot + 1;
    apiStackSpace = 3;
  }

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, apiStackSpace);

  if (!ABI_PASSES_HANDLES_IN_REGS) {
    // pass 1st arg by reference
    __ StoreP(r2, MemOperand(sp, arg0Slot * kPointerSize));
    __ AddP(r2, sp, Operand(arg0Slot * kPointerSize));
  }

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ StoreP(r3, MemOperand(sp, accessorInfoSlot * kPointerSize));
  __ AddP(r3, sp, Operand(accessorInfoSlot * kPointerSize));
  // r3 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback();

  __ LoadP(scratch, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ LoadP(api_function_address,
           FieldMemOperand(scratch, Foreign::kForeignAddressOffset));

  // +3 is to skip prolog, return address and name handle.
  MemOperand return_value_operand(
      fp, (PropertyCallbackArguments::kReturnValueOffset + 3) * kPointerSize);
  MemOperand* const kUseStackSpaceConstant = nullptr;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kStackUnwindSpace, kUseStackSpaceConstant,
                           return_value_operand);
}

void Builtins::Generate_DirectCEntry(MacroAssembler* masm) {
  // Unused.
  __ stop(0);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390

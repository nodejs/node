// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_RISCV64

#include "src/api/api-arguments.h"
#include "src/codegen/code-factory.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"
#include "src/logging/counters.h"
// For interpreter_entry_return_pc_offset. TODO(jkummerow): Drop.
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/register-configuration.h"
#include "src/codegen/riscv64/constants-riscv64.h"
#include "src/heap/heap-inl.h"
#include "src/objects/cell.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-generator.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/runtime/runtime.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address) {
  __ li(kJavaScriptCallExtraArg1Register, ExternalReference::Create(address));
  __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
          RelocInfo::CODE_TARGET);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- a0 : actual argument count
  //  -- a1 : target function (preserved for callee)
  //  -- a3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the target function, the new target and the actual
    // argument count.
    // Push function as parameter to the runtime call.
    __ SmiTag(kJavaScriptCallArgCountRegister);
    __ Push(kJavaScriptCallTargetRegister, kJavaScriptCallNewTargetRegister,
            kJavaScriptCallArgCountRegister, kJavaScriptCallTargetRegister);

    __ CallRuntime(function_id, 1);
    // Use the return value before restoring a0
    __ Add64(a2, a0, Operand(Code::kHeaderSize - kHeapObjectTag));
    // Restore target function, new target and actual argument count.
    __ Pop(kJavaScriptCallTargetRegister, kJavaScriptCallNewTargetRegister,
           kJavaScriptCallArgCountRegister);
    __ SmiUntag(kJavaScriptCallArgCountRegister);
  }

  static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
  __ Jump(a2);
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

    // Set up pointer to last argument (skip receiver).
    __ Add64(
        t2, fp,
        Operand(StandardFrameConstants::kCallerSPOffset + kSystemPointerSize));
    // Copy arguments and receiver to the expression stack.
    __ PushArray(t2, a0);
    // The receiver for the builtin/api call.
    __ PushRoot(RootIndex::kTheHoleValue);

    // Call the function.
    // a0: number of arguments (untagged)
    // a1: constructor function
    // a3: new target
    __ InvokeFunctionWithNewTarget(a1, a3, a0, CALL_FUNCTION);

    // Restore context from the frame.
    __ Ld(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ Ld(t0, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ SmiScale(t0, t0, kPointerSizeLog2);
  __ Add64(sp, sp, t0);
  __ Add64(sp, sp, kPointerSize);
  __ Ret();
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  --      a0: number of arguments (untagged)
  //  --      a1: constructor function
  //  --      a3: new target
  //  --      cp: context
  //  --      ra: return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  FrameScope scope(masm, StackFrame::MANUAL);
  Label post_instantiation_deopt_entry, not_create_implicit_receiver;
  __ EnterFrame(StackFrame::CONSTRUCT);

  // Preserve the incoming parameters on the stack.
  __ SmiTag(a0);
  __ Push(cp, a0, a1);
  __ PushRoot(RootIndex::kUndefinedValue);
  __ Push(a3);

  // ----------- S t a t e -------------
  //  --        sp[0*kPointerSize]: new target
  //  --        sp[1*kPointerSize]: padding
  //  -- a1 and sp[2*kPointerSize]: constructor function
  //  --        sp[3*kPointerSize]: number of arguments (tagged)
  //  --        sp[4*kPointerSize]: context
  // -----------------------------------

  __ Ld(t2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ Lwu(t2, FieldMemOperand(t2, SharedFunctionInfo::kFlagsOffset));
  __ DecodeField<SharedFunctionInfo::FunctionKindBits>(t2);
  __ JumpIfIsInRange(t2, kDefaultDerivedConstructor, kDerivedConstructor,
                     &not_create_implicit_receiver);

  // If not derived class constructor: Allocate the new receiver object.
  __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1, t2,
                      t4);
  __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject), RelocInfo::CODE_TARGET);
  __ Branch(&post_instantiation_deopt_entry);

  // Else: use TheHoleValue as receiver for constructor call
  __ bind(&not_create_implicit_receiver);
  __ LoadRoot(a0, RootIndex::kTheHoleValue);

  // ----------- S t a t e -------------
  //  --                          a0: receiver
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
  __ Pop(a3);

  // Push the allocated receiver to the stack.
  __ Push(a0);

  // We need two copies because we may have to return the original one
  // and the calling conventions dictate that the called function pops the
  // receiver. The second copy is pushed after the arguments, we saved in a6
  // since a0 will store the return value of callRuntime.
  __ Move(a6, a0);

  // Set up pointer to last argument.
  __ Add64(
      t2, fp,
      Operand(StandardFrameConstants::kCallerSPOffset + kSystemPointerSize));

  // ----------- S t a t e -------------
  //  --                 r3: new target
  //  -- sp[0*kPointerSize]: implicit receiver
  //  -- sp[1*kPointerSize]: implicit receiver
  //  -- sp[2*kPointerSize]: padding
  //  -- sp[3*kPointerSize]: constructor function
  //  -- sp[4*kPointerSize]: number of arguments (tagged)
  //  -- sp[5*kPointerSize]: context
  // -----------------------------------

  // Restore constructor function and argument count.
  __ Ld(a1, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
  __ Ld(a0, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
  __ SmiUntag(a0);

  Label stack_overflow;
  __ StackOverflowCheck(a0, t0, t1, &stack_overflow);

  // TODO(victorgomes): When the arguments adaptor is completely removed, we
  // should get the formal parameter count and copy the arguments in its
  // correct position (including any undefined), instead of delaying this to
  // InvokeFunction.

  // Copy arguments and receiver to the expression stack.
  __ PushArray(t2, a0);
  // We need two copies because we may have to return the original one
  // and the calling conventions dictate that the called function pops the
  // receiver. The second copy is pushed after the arguments,
  __ Push(a6);

  // Call the function.
  __ InvokeFunctionWithNewTarget(a1, a3, a0, CALL_FUNCTION);

  // ----------- S t a t e -------------
  //  --                 a0: constructor result
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
  __ Ld(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

  // If the result is an object (in the ECMA sense), we should get rid
  // of the receiver and use the result; see ECMA-262 section 13.2.2-7
  // on page 74.
  Label use_receiver, do_throw, leave_and_return, check_receiver;

  // If the result is undefined, we jump out to using the implicit receiver.
  __ JumpIfNotRoot(a0, RootIndex::kUndefinedValue, &check_receiver);

  // Otherwise we do a smi check and fall through to check if the return value
  // is a valid receiver.

  // Throw away the result of the constructor invocation and use the
  // on-stack receiver as the result.
  __ bind(&use_receiver);
  __ Ld(a0, MemOperand(sp, 0 * kPointerSize));
  __ JumpIfRoot(a0, RootIndex::kTheHoleValue, &do_throw);

  __ bind(&leave_and_return);
  // Restore smi-tagged arguments count from the frame.
  __ Ld(a1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
  // Leave construct frame.
  __ LeaveFrame(StackFrame::CONSTRUCT);

  // Remove caller arguments from the stack and return.
  __ SmiScale(a4, a1, kPointerSizeLog2);
  __ Add64(sp, sp, a4);
  __ Add64(sp, sp, kPointerSize);
  __ Ret();

  __ bind(&check_receiver);
  __ JumpIfSmi(a0, &use_receiver);

  // If the type of the result (stored in its map) is less than
  // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
  __ GetObjectType(a0, t2, t2);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ Branch(&leave_and_return, greater_equal, t2,
            Operand(FIRST_JS_RECEIVER_TYPE));
  __ Branch(&use_receiver);

  __ bind(&do_throw);
  // Restore the context from the frame.
  __ Ld(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
  __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);
  __ break_(0xCC);

  __ bind(&stack_overflow);
  // Restore the context from the frame.
  __ Ld(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
  __ CallRuntime(Runtime::kThrowStackOverflow);
  __ break_(0xCC);
}

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

static void GetSharedFunctionInfoBytecode(MacroAssembler* masm,
                                          Register sfi_data,
                                          Register scratch1) {
  Label done;

  __ GetObjectType(sfi_data, scratch1, scratch1);
  __ Branch(&done, ne, scratch1, Operand(INTERPRETER_DATA_TYPE));
  __ Ld(sfi_data,
        FieldMemOperand(sfi_data, InterpreterData::kBytecodeArrayOffset));

  __ bind(&done);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the value to pass to the generator
  //  -- a1 : the JSGeneratorObject to resume
  //  -- ra : return address
  // -----------------------------------
  __ AssertGeneratorObject(a1);

  // Store input value into generator object.
  __ Sd(a0, FieldMemOperand(a1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(a1, JSGeneratorObject::kInputOrDebugPosOffset, a0, a3,
                      kRAHasNotBeenSaved, kDontSaveFPRegs);

  // Load suspended function and context.
  __ Ld(a4, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
  __ Ld(cp, FieldMemOperand(a4, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ li(a5, debug_hook);
  __ Lb(a5, MemOperand(a5));
  __ Branch(&prepare_step_in_if_stepping, ne, a5, Operand(zero_reg));

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ li(a5, debug_suspended_generator);
  __ Ld(a5, MemOperand(a5));
  __ Branch(&prepare_step_in_suspended_generator, eq, a1, Operand(a5));
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ LoadStackLimit(kScratchReg,
                    MacroAssembler::StackLimitKind::kRealStackLimit);
  __ Branch(&stack_overflow, Uless, sp, Operand(kScratchReg));

  // ----------- S t a t e -------------
  //  -- a1    : the JSGeneratorObject to resume
  //  -- a4    : generator function
  //  -- cp    : generator context
  //  -- ra    : return address
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ Ld(a3, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
  __ Lhu(a3,
         FieldMemOperand(a3, SharedFunctionInfo::kFormalParameterCountOffset));
  __ Ld(t1,
        FieldMemOperand(a1, JSGeneratorObject::kParametersAndRegistersOffset));
  {
    Label done_loop, loop;
    __ bind(&loop);
    __ Sub64(a3, a3, Operand(1));
    __ Branch(&done_loop, lt, a3, Operand(zero_reg));
    __ CalcScaledAddress(kScratchReg, t1, a3, kPointerSizeLog2);
    __ Ld(kScratchReg, FieldMemOperand(kScratchReg, FixedArray::kHeaderSize));
    __ Push(kScratchReg);
    __ Branch(&loop);
    __ bind(&done_loop);
    // Push receiver.
    __ Ld(kScratchReg, FieldMemOperand(a1, JSGeneratorObject::kReceiverOffset));
    __ Push(kScratchReg);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ Ld(a3, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
    __ Ld(a3, FieldMemOperand(a3, SharedFunctionInfo::kFunctionDataOffset));
    GetSharedFunctionInfoBytecode(masm, a3, a0);
    __ GetObjectType(a3, a3, a3);
    __ Assert(eq, AbortReason::kMissingBytecodeArray, a3,
              Operand(BYTECODE_ARRAY_TYPE));
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ Ld(a0, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
    __ Lhu(a0, FieldMemOperand(
                   a0, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Move(a3, a1);
    __ Move(a1, a4);
    static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
    __ Ld(a2, FieldMemOperand(a1, JSFunction::kCodeOffset));
    __ JumpCodeObject(a2);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1, a4);
    // Push hole as receiver since we do not use it for stepping.
    __ PushRoot(RootIndex::kTheHoleValue);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(a1);
  }
  __ Ld(a4, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
  __ Branch(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(a1);
  }
  __ Ld(a4, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
  __ Branch(&stepping_prepared);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ break_(0xCC);  // This should be unreachable.
  }
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ Push(a1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

// Clobbers scratch1 and scratch2; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        Register scratch1, Register scratch2) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadStackLimit(scratch1, MacroAssembler::StackLimitKind::kRealStackLimit);
  // Make a2 the space we have left. The stack might already be overflowed
  // here which will cause r2 to become negative.
  __ Sub64(scratch1, sp, scratch1);
  // Check if the arguments will overflow the stack.
  __ Sll64(scratch2, argc, kPointerSizeLog2);
  __ Branch(&okay, gt, scratch1, Operand(scratch2));  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}

namespace {

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
  Label invoke, handler_entry, exit;

  {
    NoRootArrayScope no_root_array(masm);

    // TODO(plind): unify the ABI description here.
    // Registers:
    //  either
    //   a0: root register value
    //   a1: entry address
    //   a2: function
    //   a3: receiver
    //   a4: argc
    //   a5: argv
    //  or
    //   a0: root register value
    //   a1: microtask_queue

    // Save callee saved registers on the stack.
    __ MultiPush(kCalleeSaved | ra.bit());

    // Save callee-saved FPU registers.
    __ MultiPushFPU(kCalleeSavedFPU);
    // Set up the reserved register for 0.0.
    __ LoadFPRImmediate(kDoubleRegZero, 0.0);

    // Initialize the root register.
    // C calling convention. The first argument is passed in a0.
    __ Move(kRootRegister, a0);
  }

  // a1: entry address
  // a2: function
  // a3: receiver
  // a4: argc
  // a5: argv

  // We build an EntryFrame.
  __ li(s1, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  __ li(s2, Operand(StackFrame::TypeToMarker(type)));
  __ li(s3, Operand(StackFrame::TypeToMarker(type)));
  ExternalReference c_entry_fp = ExternalReference::Create(
      IsolateAddressId::kCEntryFPAddress, masm->isolate());
  __ li(s4, c_entry_fp);
  __ Ld(s4, MemOperand(s4));
  __ Push(s1, s2, s3, s4);
  // Set up frame pointer for the frame to be pushed.
  __ Add64(fp, sp, -EntryFrameConstants::kCallerFPOffset);
  // Registers:
  //  either
  //   a1: entry address
  //   a2: function
  //   a3: receiver
  //   a4: argc
  //   a5: argv
  //  or
  //   a1: microtask_queue
  //
  // Stack:
  // caller fp          |
  // function slot      | entry frame
  // context slot       |
  // bad fp (0xFF...F)  |
  // callee saved registers + ra
  // [ O32: 4 args slots]
  // args

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp = ExternalReference::Create(
      IsolateAddressId::kJSEntrySPAddress, masm->isolate());
  __ li(s1, js_entry_sp);
  __ Ld(s2, MemOperand(s1));
  __ Branch(&non_outermost_js, ne, s2, Operand(zero_reg));
  __ Sd(fp, MemOperand(s1));
  __ li(s3, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ Branch(&cont);
  __ bind(&non_outermost_js);
  __ li(s3, Operand(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);
  __ push(s3);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ Branch(&invoke);
  __ bind(&handler_entry);

  // Store the current pc as the handler offset. It's used later to create the
  // handler table.
  masm->isolate()->builtins()->SetJSEntryHandlerOffset(handler_entry.pos());

  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushStackHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ li(s1, ExternalReference::Create(
                IsolateAddressId::kPendingExceptionAddress, masm->isolate()));
  __ Sd(a0, MemOperand(s1));  // We come back from 'invoke'. result is in a0.
  __ LoadRoot(a0, RootIndex::kException);
  __ Branch(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler();
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the bal(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.
  //
  // Registers:
  //  either
  //   a0: root register value
  //   a1: entry address
  //   a2: function
  //   a3: receiver
  //   a4: argc
  //   a5: argv
  //  or
  //   a0: root register value
  //   a1: microtask_queue
  //
  // Stack:
  // handler frame
  // entry frame
  // callee saved registers + ra
  // [ O32: 4 args slots]
  // args
  //
  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return.

  Handle<Code> trampoline_code =
      masm->isolate()->builtins()->builtin_handle(entry_trampoline);
  __ Call(trampoline_code, RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // a0 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(a5);
  __ Branch(&non_outermost_js_2, ne, a5,
            Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ li(a5, js_entry_sp);
  __ Sd(zero_reg, MemOperand(a5));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(a5);
  __ li(a4, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                      masm->isolate()));
  __ Sd(a5, MemOperand(a4));

  // Reset the stack to the callee saved registers.
  __ Add64(sp, sp, -EntryFrameConstants::kCallerFPOffset);

  // Restore callee-saved fpu registers.
  __ MultiPopFPU(kCalleeSavedFPU);

  // Restore callee saved registers from the stack.
  __ MultiPop(kCalleeSaved | ra.bit());
  // Return.
  __ Jump(ra);
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

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // ----------- S t a t e -------------
  //  -- a1: new.target
  //  -- a2: function
  //  -- a3: receiver_pointer
  //  -- a4: argc
  //  -- a5: argv
  // -----------------------------------

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address = ExternalReference::Create(
        IsolateAddressId::kContextAddress, masm->isolate());
    __ li(cp, context_address);
    __ Ld(cp, MemOperand(cp));

    // Push the function onto the stack.
    __ Push(a2);

    // Check if we have enough stack space to push all arguments.
    __ Add64(a6, a4, 1);
    Generate_CheckStackOverflow(masm, a6, a0, s2);

    // Copy arguments to the stack in a loop.
    // a4: argc
    // a5: argv, i.e. points to first arg
    Label loop, entry;
    __ CalcScaledAddress(s1, a5, a4, kPointerSizeLog2);
    __ Branch(&entry);
    // s1 points past last arg.
    __ bind(&loop);
    __ Add64(s1, s1, -kPointerSize);
    __ Ld(s2, MemOperand(s1));  // Read next parameter.
    __ Ld(s2, MemOperand(s2));  // Dereference handle.
    __ push(s2);                // Push parameter.
    __ bind(&entry);
    __ Branch(&loop, ne, a5, Operand(s1));

    // Push the receive.
    __ Push(a3);

    // a0: argc
    // a1: function
    // a3: new.target
    __ Move(a3, a1);
    __ Move(a1, a2);
    __ Move(a0, a4);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(a4, RootIndex::kUndefinedValue);
    __ Move(a5, a4);
    __ Move(s1, a4);
    __ Move(s2, a4);
    __ Move(s3, a4);
    __ Move(s4, a4);
    __ Move(s5, a4);
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

void Builtins::Generate_RunMicrotasksTrampoline(MacroAssembler* masm) {
  // a1: microtask_queue
  __ Move(RunMicrotasksDescriptor::MicrotaskQueueRegister(), a1);
  __ Jump(BUILTIN_CODE(masm->isolate(), RunMicrotasks), RelocInfo::CODE_TARGET);
}

static void ReplaceClosureCodeWithOptimizedCode(MacroAssembler* masm,
                                                Register optimized_code,
                                                Register closure,
                                                Register scratch1,
                                                Register scratch2) {
  // Store code entry in the closure.
  __ Sd(optimized_code, FieldMemOperand(closure, JSFunction::kCodeOffset));
  __ Move(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kRAHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch1,
                                  Register scratch2) {
  Register params_size = scratch1;

  // Get the size of the formal parameters + receiver (in bytes).
  __ Ld(params_size,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Lw(params_size,
        FieldMemOperand(params_size, BytecodeArray::kParameterSizeOffset));

  Register actual_params_size = scratch2;
  Label L1;
  // Compute the size of the actual parameters + receiver (in bytes).
  __ Ld(actual_params_size,
        MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ Sll64(actual_params_size, actual_params_size, kPointerSizeLog2);
  __ Add64(actual_params_size, actual_params_size, Operand(kSystemPointerSize));

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  __ Branch(&L1, le, actual_params_size, Operand(params_size));
  __ Move(params_size, actual_params_size);
  __ bind(&L1);

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  // Drop receiver + arguments.
  __ Add64(sp, sp, params_size);
}

// Tail-call |function_id| if |actual_marker| == |expected_marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register actual_marker,
                                          OptimizationMarker expected_marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ Branch(&no_match, ne, actual_marker, Operand(expected_marker));
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void TailCallOptimizedCodeSlot(MacroAssembler* masm,
                                      Register optimized_code_entry,
                                      Register scratch1, Register scratch2) {
  // ----------- S t a t e -------------
  //  -- a0 : actual argument count
  //  -- a3 : new target (preserved for callee if needed, and caller)
  //  -- a1 : target function (preserved for callee if needed, and caller)
  // -----------------------------------
  DCHECK(!AreAliased(optimized_code_entry, a1, a3, scratch1, scratch2));

  Register closure = a1;
  Label heal_optimized_code_slot;

  // If the optimized code is cleared, go to runtime to update the optimization
  // marker field.
  __ LoadWeakValue(optimized_code_entry, optimized_code_entry,
                   &heal_optimized_code_slot);

  // Check if the optimized code is marked for deopt. If it is, call the
  // runtime to clear it.
  __ Ld(a5,
        FieldMemOperand(optimized_code_entry, Code::kCodeDataContainerOffset));
  __ Lw(a5, FieldMemOperand(a5, CodeDataContainer::kKindSpecificFlagsOffset));
  __ And(a5, a5, Operand(1 << Code::kMarkedForDeoptimizationBit));
  __ Branch(&heal_optimized_code_slot, ne, a5, Operand(zero_reg));

  // Optimized code is good, get it into the closure and link the closure into
  // the optimized functions list, then tail call the optimized code.
  // The feedback vector is no longer used, so re-use it as a scratch
  // register.
  ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                      scratch1, scratch2);

  static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
  __ LoadCodeObjectEntry(a2, optimized_code_entry);
  __ Jump(a2);

  // Optimized code slot contains deoptimized code or code is cleared and
  // optimized code marker isn't updated. Evict the code, update the marker
  // and re-enter the closure's code.
  __ bind(&heal_optimized_code_slot);
  GenerateTailCallToReturnedCode(masm, Runtime::kHealOptimizedCodeSlot);
}

static void MaybeOptimizeCode(MacroAssembler* masm, Register feedback_vector,
                              Register optimization_marker) {
  // ----------- S t a t e -------------
  //  -- a0 : actual argument count
  //  -- a3 : new target (preserved for callee if needed, and caller)
  //  -- a1 : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  //  -- optimization_marker : a int32 containing a non-zero optimization
  //  marker.
  // -----------------------------------
  DCHECK(!AreAliased(feedback_vector, a1, a3, optimization_marker));

  // TODO(v8:8394): The logging of first execution will break if
  // feedback vectors are not allocated. We need to find a different way of
  // logging these events if required.
  TailCallRuntimeIfMarkerEquals(masm, optimization_marker,
                                OptimizationMarker::kLogFirstExecution,
                                Runtime::kFunctionFirstExecution);
  TailCallRuntimeIfMarkerEquals(masm, optimization_marker,
                                OptimizationMarker::kCompileOptimized,
                                Runtime::kCompileOptimized_NotConcurrent);
  TailCallRuntimeIfMarkerEquals(masm, optimization_marker,
                                OptimizationMarker::kCompileOptimizedConcurrent,
                                Runtime::kCompileOptimized_Concurrent);

  // Marker should be one of LogFirstExecution / CompileOptimized /
  // CompileOptimizedConcurrent. InOptimizationQueue and None shouldn't reach
  // here.
  if (FLAG_debug_code) {
    __ stop();
  }
}

// Advance the current bytecode offset. This simulates what all bytecode
// handlers do upon completion of the underlying operation. Will bail out to a
// label if the bytecode (without prefix) is a return bytecode. Will not advance
// the bytecode offset if the current bytecode is a JumpLoop, instead just
// re-executing the JumpLoop to jump to the correct bytecode.
static void AdvanceBytecodeOffsetOrReturn(MacroAssembler* masm,
                                          Register bytecode_array,
                                          Register bytecode_offset,
                                          Register bytecode, Register scratch1,
                                          Register scratch2, Register scratch3,
                                          Label* if_return) {
  Register bytecode_size_table = scratch1;

  // The bytecode offset value will be increased by one in wide and extra wide
  // cases. In the case of having a wide or extra wide JumpLoop bytecode, we
  // will restore the original bytecode. In order to simplify the code, we have
  // a backup of it.
  Register original_bytecode_offset = scratch3;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode,
                     bytecode_size_table, original_bytecode_offset));
  __ Move(original_bytecode_offset, bytecode_offset);
  __ li(bytecode_size_table, ExternalReference::bytecode_size_table_address());

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  STATIC_ASSERT(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  STATIC_ASSERT(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ Branch(&process_bytecode, Ugreater, bytecode, Operand(3));
  __ And(scratch2, bytecode, Operand(1));
  __ Branch(&extra_wide, ne, scratch2, Operand(zero_reg));

  // Load the next bytecode and update table to the wide scaled table.
  __ Add64(bytecode_offset, bytecode_offset, Operand(1));
  __ Add64(scratch2, bytecode_array, bytecode_offset);
  __ Lbu(bytecode, MemOperand(scratch2));
  __ Add64(bytecode_size_table, bytecode_size_table,
           Operand(kByteSize * interpreter::Bytecodes::kBytecodeCount));
  __ Branch(&process_bytecode);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ Add64(bytecode_offset, bytecode_offset, Operand(1));
  __ Add64(scratch2, bytecode_array, bytecode_offset);
  __ Lbu(bytecode, MemOperand(scratch2));
  __ Add64(bytecode_size_table, bytecode_size_table,
           Operand(2 * kByteSize * interpreter::Bytecodes::kBytecodeCount));

  __ bind(&process_bytecode);

// Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)          \
  __ Branch(if_return, eq, bytecode, \
            Operand(static_cast<int>(interpreter::Bytecode::k##NAME)));
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // If this is a JumpLoop, re-execute it to perform the jump to the beginning
  // of the loop.
  Label end, not_jump_loop;
  __ Branch(&not_jump_loop, ne, bytecode,
            Operand(static_cast<int>(interpreter::Bytecode::kJumpLoop)));
  // We need to restore the original bytecode_offset since we might have
  // increased it to skip the wide / extra-wide prefix bytecode.
  __ Move(bytecode_offset, original_bytecode_offset);
  __ Branch(&end);

  __ bind(&not_jump_loop);
  // Otherwise, load the size of the current bytecode and advance the offset.
  __ Add64(scratch2, bytecode_size_table, bytecode);
  __ Lb(scratch2, MemOperand(scratch2));
  __ Add64(bytecode_offset, bytecode_offset, scratch2);

  __ bind(&end);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.
//
// The live registers are:
//   o a0 : actual argument count (not including the receiver)
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
  Register closure = a1;
  Register feedback_vector = a2;

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  __ Ld(kScratchReg,
        FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ Ld(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(kScratchReg, SharedFunctionInfo::kFunctionDataOffset));
  GetSharedFunctionInfoBytecode(masm, kInterpreterBytecodeArrayRegister,
                                kScratchReg);

  // The bytecode array could have been flushed from the shared function info,
  // if so, call into CompileLazy.
  Label compile_lazy;
  __ GetObjectType(kInterpreterBytecodeArrayRegister, kScratchReg, kScratchReg);
  __ Branch(&compile_lazy, ne, kScratchReg, Operand(BYTECODE_ARRAY_TYPE));

  // Load the feedback vector from the closure.
  __ Ld(feedback_vector,
        FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ Ld(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));

  Label push_stack_frame;
  // Check if feedback vector is valid. If valid, check for optimized code
  // and update invocation count. Otherwise, setup the stack frame.
  __ Ld(a4, FieldMemOperand(feedback_vector, HeapObject::kMapOffset));
  __ Lhu(a4, FieldMemOperand(a4, Map::kInstanceTypeOffset));
  __ Branch(&push_stack_frame, ne, a4, Operand(FEEDBACK_VECTOR_TYPE));

  // Read off the optimization state in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  Register optimization_state = a4;
  __ Lw(optimization_state,
        FieldMemOperand(feedback_vector, FeedbackVector::kFlagsOffset));

  // Check if the optimized code slot is not empty or has a optimization marker.
  Label has_optimized_code_or_marker;

  __ And(t0, optimization_state,
         FeedbackVector::kHasOptimizedCodeOrCompileOptimizedMarkerMask);
  __ Branch(&has_optimized_code_or_marker, ne, t0, Operand(zero_reg));

  Label not_optimized;
  __ bind(&not_optimized);

  // Increment invocation count for the function.
  __ Lw(a4, FieldMemOperand(feedback_vector,
                            FeedbackVector::kInvocationCountOffset));
  __ Add32(a4, a4, Operand(1));
  __ Sw(a4, FieldMemOperand(feedback_vector,
                            FeedbackVector::kInvocationCountOffset));

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  __ bind(&push_stack_frame);
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(closure);

  // Reset code age and the OSR arming. The OSR field and BytecodeAgeOffset are
  // 8-bit fields next to each other, so we could just optimize by writing a
  // 16-bit. These static asserts guard our assumption is valid.
  STATIC_ASSERT(BytecodeArray::kBytecodeAgeOffset ==
                BytecodeArray::kOsrNestingLevelOffset + kCharSize);
  STATIC_ASSERT(BytecodeArray::kNoAgeBytecodeAge == 0);
  __ Sh(zero_reg, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                  BytecodeArray::kOsrNestingLevelOffset));

  // Load initial bytecode offset.
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(a4, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, a4);

  // Allocate the local and temporary register file on the stack.
  Label stack_overflow;
  {
    // Load frame size (word) from the BytecodeArray object.
    __ Lw(a4, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    __ Sub64(a5, sp, Operand(a4));
    __ LoadStackLimit(a2, MacroAssembler::StackLimitKind::kRealStackLimit);
    __ Branch(&stack_overflow, Uless, a5, Operand(a2));

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(a5, RootIndex::kUndefinedValue);
    __ Branch(&loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(a5);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ Sub64(a4, a4, Operand(kPointerSize));
    __ Branch(&loop_header, ge, a4, Operand(zero_reg));
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in r3.
  Label no_incoming_new_target_or_generator_register;
  __ Lw(a5, FieldMemOperand(
                kInterpreterBytecodeArrayRegister,
                BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ Branch(&no_incoming_new_target_or_generator_register, eq, a5,
            Operand(zero_reg));
  __ CalcScaledAddress(a5, fp, a5, kPointerSizeLog2);
  __ Sd(a3, MemOperand(a5));
  __ bind(&no_incoming_new_target_or_generator_register);

  // Perform interrupt stack check.
  // TODO(solanes): Merge with the real stack limit check above.
  Label stack_check_interrupt, after_stack_check_interrupt;
  __ LoadStackLimit(a5, MacroAssembler::StackLimitKind::kInterruptStackLimit);
  __ Branch(&stack_check_interrupt, Uless, sp, Operand(a5));
  __ bind(&after_stack_check_interrupt);

  // Load accumulator as undefined.
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ li(kInterpreterDispatchTableRegister,
        ExternalReference::interpreter_dispatch_table_address(masm->isolate()));
  __ Add64(a1, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ Lbu(a7, MemOperand(a1));
  __ CalcScaledAddress(kScratchReg, kInterpreterDispatchTableRegister, a7,
                       kPointerSizeLog2);
  __ Ld(kJavaScriptCallCodeStartRegister, MemOperand(kScratchReg));
  __ Call(kJavaScriptCallCodeStartRegister);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ Ld(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Ld(kInterpreterBytecodeOffsetRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Either return, or advance to the next bytecode and dispatch.
  Label do_return;
  __ Add64(a1, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ Lbu(a1, MemOperand(a1));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, a1, a2, a3,
                                a4, &do_return);
  __ Branch(&do_dispatch);

  __ bind(&do_return);
  // The return value is in a0.
  LeaveInterpreterFrame(masm, t0, t1);
  __ Jump(ra);

  __ bind(&stack_check_interrupt);
  // Modify the bytecode offset in the stack to be kFunctionEntryBytecodeOffset
  // for the call to the StackGuard.
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(Smi::FromInt(BytecodeArray::kHeaderSize - kHeapObjectTag +
                             kFunctionEntryBytecodeOffset)));
  __ Sd(kInterpreterBytecodeOffsetRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ CallRuntime(Runtime::kStackGuard);

  // After the call, restore the bytecode array, bytecode offset and accumulator
  // registers again. Also, restore the bytecode offset in the stack to its
  // previous value.
  __ Ld(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);

  __ SmiTag(a5, kInterpreterBytecodeOffsetRegister);
  __ Sd(a5, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  __ Branch(&after_stack_check_interrupt);

  __ bind(&has_optimized_code_or_marker);
  Label maybe_has_optimized_code;
  // Check if optimized code marker is available
  __ And(t0, optimization_state, FeedbackVector::OptimizationTierBits::kMask);
  __ Branch(&maybe_has_optimized_code, ne, t0, Operand(zero_reg));

  Register optimization_marker = optimization_state;
  __ DecodeField<FeedbackVector::OptimizationMarkerBits>(optimization_marker);
  MaybeOptimizeCode(masm, feedback_vector, optimization_marker);
  // Fall through if there's no runnable optimized code.
  __ Branch(&not_optimized);

  __ bind(&maybe_has_optimized_code);
  Register optimized_code_entry = optimization_state;
  __ Ld(optimization_marker,
        FieldMemOperand(feedback_vector,
                        FeedbackVector::kMaybeOptimizedCodeOffset));

  TailCallOptimizedCodeSlot(masm, optimized_code_entry, t4, a5);

  __ bind(&compile_lazy);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
  // Unreachable code.
  __ break_(0xCC);

  __ bind(&stack_overflow);
  __ CallRuntime(Runtime::kThrowStackOverflow);
  // Unreachable code.
  __ break_(0xCC);
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args,
                                         Register start_address,
                                         Register scratch) {
  // Find the address of the last argument.
  __ Sub64(scratch, num_args, Operand(1));
  __ Sll64(scratch, scratch, kPointerSizeLog2);
  __ Sub64(start_address, start_address, scratch);

  // Push the arguments.
  __ PushArray(start_address, num_args,
               TurboAssembler::PushArrayOrder::kReverse);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // The spread argument should not be pushed.
    __ Sub64(a0, a0, Operand(1));
  }

  __ Add64(a3, a0, Operand(1));  // Add one for receiver.

  __ StackOverflowCheck(a3, a4, t0, &stack_overflow);

  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    // Don't copy receiver.
    __ Move(a3, a0);
  }

  // This function modifies a2 and a4.
  Generate_InterpreterPushArgs(masm, a3, a2, a4);

  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(RootIndex::kUndefinedValue);
  }

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Pass the spread in the register a2.
    // a2 already points to the penultime argument, the spread
    // is below that.
    __ Ld(a2, MemOperand(a2, -kSystemPointerSize));
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
  // -- a4 : address of the first argument
  // -----------------------------------
  Label stack_overflow;
  __ Add64(a6, a0, 1);
  __ StackOverflowCheck(a6, a5, t0, &stack_overflow);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // The spread argument should not be pushed.
    __ Sub64(a0, a0, Operand(1));
  }

  // Push the arguments, This function modifies a4 and a5.
  Generate_InterpreterPushArgs(masm, a0, a4, a5);

  // Push a slot for the receiver.
  __ push(zero_reg);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Pass the spread in the register a2.
    // a4 already points to the penultimate argument, the spread
    // lies in the next interpreter register.
    __ Ld(a2, MemOperand(a4, -kSystemPointerSize));
  } else {
    __ AssertUndefinedOrAllocationSite(a2, t0);
  }

  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    __ AssertFunction(a1);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ Jump(BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl),
            RelocInfo::CODE_TARGET);
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
  Label builtin_trampoline, trampoline_loaded;
  Smi interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::zero());

  // If the SFI function_data is an InterpreterData, the function will have a
  // custom copy of the interpreter entry trampoline for profiling. If so,
  // get the custom trampoline, otherwise grab the entry address of the global
  // trampoline.
  __ Ld(t0, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ Ld(t0, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
  __ Ld(t0, FieldMemOperand(t0, SharedFunctionInfo::kFunctionDataOffset));
  __ GetObjectType(t0, kInterpreterDispatchTableRegister,
                   kInterpreterDispatchTableRegister);
  __ Branch(&builtin_trampoline, ne, kInterpreterDispatchTableRegister,
            Operand(INTERPRETER_DATA_TYPE));

  __ Ld(t0, FieldMemOperand(t0, InterpreterData::kInterpreterTrampolineOffset));
  __ Add64(t0, t0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Branch(&trampoline_loaded);

  __ bind(&builtin_trampoline);
  __ li(t0, ExternalReference::
                address_of_interpreter_entry_trampoline_instruction_start(
                    masm->isolate()));
  __ Ld(t0, MemOperand(t0));

  __ bind(&trampoline_loaded);
  __ Add64(ra, t0, Operand(interpreter_entry_return_pc_offset.value()));

  // Initialize the dispatch table register.
  __ li(kInterpreterDispatchTableRegister,
        ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Get the bytecode array pointer from the frame.
  __ Ld(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ SmiTst(kInterpreterBytecodeArrayRegister, kScratchReg);
    __ Assert(ne,
              AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry,
              kScratchReg, Operand(zero_reg));
    __ GetObjectType(kInterpreterBytecodeArrayRegister, a1, a1);
    __ Assert(eq,
              AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry,
              a1, Operand(BYTECODE_ARRAY_TYPE));
  }

  // Get the target bytecode offset from the frame.
  __ SmiUntag(kInterpreterBytecodeOffsetRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  if (FLAG_debug_code) {
    Label okay;
    __ Branch(&okay, ge, kInterpreterBytecodeOffsetRegister,
              Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
    // Unreachable code.
    __ break_(0xCC);
    __ bind(&okay);
  }

  // Dispatch to the target bytecode.
  __ Add64(a1, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ Lbu(a7, MemOperand(a1));
  __ CalcScaledAddress(a1, kInterpreterDispatchTableRegister, a7,
                       kPointerSizeLog2);
  __ Ld(kJavaScriptCallCodeStartRegister, MemOperand(a1));
  __ Jump(kJavaScriptCallCodeStartRegister);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ Ld(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Ld(kInterpreterBytecodeOffsetRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  Label enter_bytecode, function_entry_bytecode;
  __ Branch(&function_entry_bytecode, eq, kInterpreterBytecodeOffsetRegister,
            Operand(BytecodeArray::kHeaderSize - kHeapObjectTag +
                    kFunctionEntryBytecodeOffset));

  // Load the current bytecode.
  __ Add64(a1, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ Lbu(a1, MemOperand(a1));

  // Advance to the next bytecode.
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, a1, a2, a3,
                                a4, &if_return);

  __ bind(&enter_bytecode);
  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(a2, kInterpreterBytecodeOffsetRegister);
  __ Sd(a2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);

  __ bind(&function_entry_bytecode);
  // If the code deoptimizes during the implicit function entry stack interrupt
  // check, it will have a bailout ID of kFunctionEntryBytecodeOffset, which is
  // not a valid bytecode offset. Detect this case and advance to the first
  // actual bytecode.
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ Branch(&enter_bytecode);

  // We should never take the if_return path.
  __ bind(&if_return);
  __ Abort(AbortReason::kInvalidBytecodeAdvance);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

namespace {
void Generate_ContinueToBuiltinHelper(MacroAssembler* masm,
                                      bool java_script_builtin,
                                      bool with_result) {
  const RegisterConfiguration* config(RegisterConfiguration::Default());
  int allocatable_register_count = config->num_allocatable_general_registers();
  UseScratchRegisterScope temp(masm);
  Register scratch = temp.Acquire();
  if (with_result) {
    if (java_script_builtin) {
      __ Move(scratch, a0);
    } else {
      // Overwrite the hole inserted by the deoptimizer with the return value
      // from the LAZY deopt point.
      __ Sd(a0,
            MemOperand(
                sp, config->num_allocatable_general_registers() * kPointerSize +
                        BuiltinContinuationFrameConstants::kFixedFrameSize));
    }
  }
  for (int i = allocatable_register_count - 1; i >= 0; --i) {
    int code = config->GetAllocatableGeneralCode(i);
    __ Pop(Register::from_code(code));
    if (java_script_builtin && code == kJavaScriptCallArgCountRegister.code()) {
      __ SmiUntag(Register::from_code(code));
    }
  }

  if (with_result && java_script_builtin) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point. t0 contains the arguments count, the return value
    // from LAZY is always the last argument.
    __ Add64(a0, a0,
             Operand(BuiltinContinuationFrameConstants::kFixedSlotCount));
    __ CalcScaledAddress(t0, sp, a0, kSystemPointerSizeLog2);
    __ Sd(scratch, MemOperand(t0));
    // Recover arguments count.
    __ Sub64(a0, a0,
             Operand(BuiltinContinuationFrameConstants::kFixedSlotCount));
  }

  __ Ld(fp, MemOperand(
                sp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  // Load builtin index (stored as a Smi) and use it to get the builtin start
  // address from the builtins table.
  __ Pop(t0);
  __ Add64(sp, sp,
           Operand(BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(ra);
  __ LoadEntryFromBuiltinIndex(t0);
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

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), a0.code());
  __ Ld(a0, MemOperand(sp, 0 * kPointerSize));
  __ Add64(sp, sp, Operand(1 * kPointerSize));  // Remove state.
  __ Ret();
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  __ Ret(eq, a0, Operand(Smi::zero()));

  // Drop the handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  __ LeaveFrame(StackFrame::STUB);

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ Ld(a1, MemOperand(a0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ SmiUntag(a1, MemOperand(a1, FixedArray::OffsetOfElementAt(
                                     DeoptimizationData::kOsrPcOffsetIndex) -
                                     kHeapObjectTag));

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ Add64(a0, a0, a1);
  __ Add64(ra, a0, Code::kHeaderSize - kHeapObjectTag);
  // And "return" to the OSR entry point of the function.
  __ Ret();
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0    : argc
  //  -- sp[0] : receiver
  //  -- sp[4] : thisArg
  //  -- sp[8] : argArray
  // -----------------------------------

  Register argc = a0;
  Register arg_array = a2;
  Register receiver = a1;
  Register this_arg = a5;
  Register undefined_value = a3;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load receiver into a1, argArray into a2 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    // Claim (2 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.

    __ Ld(this_arg, MemOperand(sp, kPointerSize));
    __ Ld(arg_array, MemOperand(sp, 2 * kPointerSize));

    Label done0, done1;
    __ Branch(&done0, ne, argc, Operand(zero_reg));
    __ Move(arg_array, undefined_value);  // if argc == 0
    __ Move(this_arg, undefined_value);   // if argc == 0
    __ bind(&done0);                      // else (i.e., argc > 0)

    __ Branch(&done1, ne, argc, Operand(1));
    __ Move(arg_array, undefined_value);  // if argc == 1
    __ bind(&done1);                      // else (i.e., argc > 1)

    __ Ld(receiver, MemOperand(sp));
    __ CalcScaledAddress(sp, sp, argc, kSystemPointerSizeLog2);
    __ Sd(this_arg, MemOperand(sp));
  }

  // ----------- S t a t e -------------
  //  -- a2    : argArray
  //  -- a1    : receiver
  //  -- a3    : undefined root value
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(arg_array, RootIndex::kNullValue, &no_arguments);
  __ Branch(&no_arguments, eq, arg_array, Operand(undefined_value));

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ Move(a0, zero_reg);
    DCHECK(receiver == a1);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Get the callable to call (passed as receiver) from the stack.
  { __ Pop(a1); }

  // 2. Make sure we have at least one argument.
  // a0: actual number of arguments
  {
    Label done;
    __ Branch(&done, ne, a0, Operand(zero_reg));
    __ PushRoot(RootIndex::kUndefinedValue);
    __ Add64(a0, a0, Operand(1));
    __ bind(&done);
  }

  // 3. Adjust the actual number of arguments.
  __ Add64(a0, a0, -1);

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]  : receiver
  //  -- sp[8]  : target         (if argc >= 1)
  //  -- sp[16] : thisArgument   (if argc >= 2)
  //  -- sp[24] : argumentsList  (if argc == 3)
  // -----------------------------------

  Register argc = a0;
  Register arguments_list = a2;
  Register target = a1;
  Register this_argument = a5;
  Register undefined_value = a3;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load target into a1 (if present), argumentsList into a2 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    // Claim (3 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.

    __ Ld(target, MemOperand(sp, kPointerSize));
    __ Ld(this_argument, MemOperand(sp, 2 * kPointerSize));
    __ Ld(arguments_list, MemOperand(sp, 3 * kPointerSize));

    Label done0, done1, done2;
    __ Branch(&done0, ne, argc, Operand(zero_reg));
    __ Move(arguments_list, undefined_value);  // if argc == 0
    __ Move(this_argument, undefined_value);   // if argc == 0
    __ Move(target, undefined_value);          // if argc == 0
    __ bind(&done0);                           // argc != 0

    __ Branch(&done1, ne, argc, Operand(1));
    __ Move(arguments_list, undefined_value);  // if argc == 1
    __ Move(this_argument, undefined_value);   // if argc == 1
    __ bind(&done1);                           // argc > 1

    __ Branch(&done2, ne, argc, Operand(2));
    __ Move(arguments_list, undefined_value);  // if argc == 2
    __ bind(&done2);                           // argc > 2

    __ CalcScaledAddress(sp, sp, argc, kSystemPointerSizeLog2);
    __ Sd(this_argument, MemOperand(sp, 0));  // Overwrite receiver
  }

  // ----------- S t a t e -------------
  //  -- a2    : argumentsList
  //  -- a1    : target
  //  -- a3    : undefined root value
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
  //  -- sp[0]   : receiver
  //  -- sp[8]   : target
  //  -- sp[16]  : argumentsList
  //  -- sp[24]  : new.target (optional)
  // -----------------------------------
  Register argc = a0;
  Register arguments_list = a2;
  Register target = a1;
  Register new_target = a3;
  Register undefined_value = a4;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load target into a1 (if present), argumentsList into a2 (if present),
  // new.target into a3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    // Claim (3 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.
    __ Ld(target, MemOperand(sp, kPointerSize));
    __ Ld(arguments_list, MemOperand(sp, 2 * kPointerSize));
    __ Ld(new_target, MemOperand(sp, 3 * kPointerSize));

    Label done0, done1, done2;
    __ Branch(&done0, ne, argc, Operand(zero_reg));
    __ Move(arguments_list, undefined_value);  // if argc == 0
    __ Move(new_target, undefined_value);      // if argc == 0
    __ Move(target, undefined_value);          // if argc == 0
    __ bind(&done0);

    __ Branch(&done1, ne, argc, Operand(1));
    __ Move(arguments_list, undefined_value);  // if argc == 1
    __ Move(new_target, target);               // if argc == 1
    __ bind(&done1);

    __ Branch(&done2, ne, argc, Operand(2));
    __ Move(new_target, target);  // if argc == 2
    __ bind(&done2);

    __ CalcScaledAddress(sp, sp, argc, kSystemPointerSizeLog2);
    __ Sd(undefined_value, MemOperand(sp, 0));  // Overwrite receiver
  }

  // ----------- S t a t e -------------
  //  -- a2    : argumentsList
  //  -- a1    : target
  //  -- a3    : new.target
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

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- a1 : target
  //  -- a0 : number of parameters on the stack (not including the receiver)
  //  -- a2 : arguments list (a FixedArray)
  //  -- a4 : len (number of elements to push from args)
  //  -- a3 : new.target (for [[Construct]])
  // -----------------------------------
  if (masm->emit_debug_code()) {
    // Allow a2 to be a FixedArray, or a FixedDoubleArray if a4 == 0.
    Label ok, fail;
    __ AssertNotSmi(a2);
    __ GetObjectType(a2, kScratchReg, kScratchReg);
    __ Branch(&ok, eq, kScratchReg, Operand(FIXED_ARRAY_TYPE));
    __ Branch(&fail, ne, kScratchReg, Operand(FIXED_DOUBLE_ARRAY_TYPE));
    __ Branch(&ok, eq, a4, Operand(zero_reg));
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  Register args = a2;
  Register len = a4;

  // Check for stack overflow.
  Label stack_overflow;
  __ StackOverflowCheck(len, kScratchReg, a5, &stack_overflow);

  // Move the arguments already in the stack,
  // including the receiver and the return address.
  {
    Label copy;
    Register src = a6, dest = a7;
    __ Move(src, sp);
    __ Sll64(t0, a4, kSystemPointerSizeLog2);
    __ Sub64(sp, sp, Operand(t0));
    // Update stack pointer.
    __ Move(dest, sp);
    __ Add64(t0, a0, Operand(zero_reg));

    __ bind(&copy);
    __ Ld(t1, MemOperand(src, 0));
    __ Sd(t1, MemOperand(dest, 0));
    __ Sub64(t0, t0, Operand(1));
    __ Add64(src, src, Operand(kSystemPointerSize));
    __ Add64(dest, dest, Operand(kSystemPointerSize));
    __ Branch(&copy, ge, t0, Operand(zero_reg));
  }

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    Label done, push, loop;
    Register src = a6;
    Register scratch = len;
    __ Add64(src, args, FixedArray::kHeaderSize - kHeapObjectTag);
    __ Add64(a0, a0, len);  // The 'len' argument for Call() or Construct().
    __ Branch(&done, eq, len, Operand(zero_reg));
    __ Sll64(scratch, len, kPointerSizeLog2);
    __ Sub64(scratch, sp, Operand(scratch));
    __ LoadRoot(t1, RootIndex::kTheHoleValue);
    __ bind(&loop);
    __ Ld(a5, MemOperand(src));
    __ Add64(src, src, kPointerSize);
    __ Branch(&push, ne, a5, Operand(t1));
    __ LoadRoot(a5, RootIndex::kUndefinedValue);
    __ bind(&push);
    __ Sd(a5, MemOperand(a7, 0));
    __ Add64(a7, a7, Operand(kSystemPointerSize));
    __ Add64(scratch, scratch, Operand(kSystemPointerSize));
    __ Branch(&loop, ne, scratch, Operand(sp));
    __ bind(&done);
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
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a3 : the new.target (for [[Construct]] calls)
  //  -- a1 : the target to call (can be any Object)
  //  -- a2 : start index (to support rest parameters)
  // -----------------------------------

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(a3, &new_target_not_constructor);
    __ Ld(t1, FieldMemOperand(a3, HeapObject::kMapOffset));
    __ Lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
    __ And(t1, t1, Operand(Map::Bits1::IsConstructorBit::kMask));
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

  // TODO(victorgomes): Remove this copy when all the arguments adaptor frame
  // code is erased.
  __ Move(a6, fp);
  __ Ld(a7, MemOperand(fp, StandardFrameConstants::kArgCOffset));

  Label stack_done, stack_overflow;
  __ Sub32(a7, a7, a2);
  __ Branch(&stack_done, le, a7, Operand(zero_reg));
  {
    // Check for stack overflow.
    __ StackOverflowCheck(a7, a4, a5, &stack_overflow);

    // Forward the arguments from the caller frame.

    // Point to the first argument to copy (skipping the receiver).
    __ Add64(a6, a6,
             Operand(CommonFrameConstants::kFixedFrameSizeAboveFp +
                     kSystemPointerSize));
    __ CalcScaledAddress(a6, a6, a2, kSystemPointerSizeLog2);

    // Move the arguments already in the stack,
    // including the receiver and the return address.
    {
      Label copy;
      Register src = t0, dest = a2;
      __ Move(src, sp);
      // Update stack pointer.
      __ Sll64(t1, a7, kSystemPointerSizeLog2);
      __ Sub64(sp, sp, Operand(t1));
      __ Move(dest, sp);
      __ Add64(t2, a0, Operand(zero_reg));

      __ bind(&copy);
      __ Ld(t1, MemOperand(src, 0));
      __ Sd(t1, MemOperand(dest, 0));
      __ Sub64(t2, t2, Operand(1));
      __ Add64(src, src, Operand(kSystemPointerSize));
      __ Add64(dest, dest, Operand(kSystemPointerSize));
      __ Branch(&copy, ge, t2, Operand(zero_reg));
    }

    // Copy arguments from the caller frame.
    // TODO(victorgomes): Consider using forward order as potentially more cache
    // friendly.
    {
      Label loop;
      __ Add64(a0, a0, a7);
      __ bind(&loop);
      {
        __ Sub32(a7, a7, Operand(1));
        __ CalcScaledAddress(t0, a6, a7, kPointerSizeLog2);
        __ Ld(kScratchReg, MemOperand(t0));
        __ CalcScaledAddress(t0, a2, a7, kPointerSizeLog2);
        __ Sd(kScratchReg, MemOperand(t0));
        __ Branch(&loop, ne, a7, Operand(zero_reg));
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
  // Check that function is not a "classConstructor".
  Label class_constructor;
  __ Ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ Lwu(a3, FieldMemOperand(a2, SharedFunctionInfo::kFlagsOffset));
  __ And(kScratchReg, a3,
         Operand(SharedFunctionInfo::IsClassConstructorBit::kMask));
  __ Branch(&class_constructor, ne, kScratchReg, Operand(zero_reg));

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ Ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ Lwu(a3, FieldMemOperand(a2, SharedFunctionInfo::kFlagsOffset));
  __ And(kScratchReg, a3,
         Operand(SharedFunctionInfo::IsNativeBit::kMask |
                 SharedFunctionInfo::IsStrictBit::kMask));
  __ Branch(&done_convert, ne, kScratchReg, Operand(zero_reg));
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
      __ LoadReceiver(a3, a0);
      __ JumpIfSmi(a3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ GetObjectType(a3, a4, a4);
      __ Branch(&done_convert, Ugreater_equal, a4,
                Operand(FIRST_JS_RECEIVER_TYPE));
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(a3, RootIndex::kUndefinedValue, &convert_global_proxy);
        __ JumpIfNotRoot(a3, RootIndex::kNullValue, &convert_to_object);
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
        __ SmiTag(a0);
        __ Push(a0, a1);
        __ Move(a0, a3);
        __ Push(cp);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ Move(a3, a0);
        __ Pop(a0, a1);
        __ SmiUntag(a0);
      }
      __ Ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ StoreReceiver(a3, a0, kScratchReg);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSFunction)
  //  -- a2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ Lhu(a2,
         FieldMemOperand(a2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ InvokeFunctionCode(a1, no_reg, a2, a0, JUMP_FUNCTION);

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
    __ Ld(t0, FieldMemOperand(a1, JSBoundFunction::kBoundThisOffset));
    __ StoreReceiver(t0, a0, kScratchReg);
  }

  // Load [[BoundArguments]] into a2 and length of that into a4.
  __ Ld(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ SmiUntag(a4, FieldMemOperand(a2, FixedArray::kLengthOffset));

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- a4 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ Sll64(a5, a4, kPointerSizeLog2);
    __ Sub64(t0, sp, Operand(a5));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    __ LoadStackLimit(kScratchReg,
                      MacroAssembler::StackLimitKind::kRealStackLimit);
    __ Branch(&done, Ugreater_equal, t0, Operand(kScratchReg));
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowStackOverflow);
    }
    __ bind(&done);
  }

  // Pop receiver.
  __ Pop(t0);

  // Push [[BoundArguments]].
  {
    Label loop, done_loop;
    __ SmiUntag(a4, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ Add64(a0, a0, Operand(a4));
    __ Add64(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Sub64(a4, a4, Operand(1));
    __ Branch(&done_loop, lt, a4, Operand(zero_reg));
    __ CalcScaledAddress(a5, a2, a4, kPointerSizeLog2);
    __ Ld(kScratchReg, MemOperand(a5));
    __ Push(kScratchReg);
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Push receiver.
  __ Push(t0);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ Ld(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_smi;
  __ JumpIfSmi(a1, &non_callable);
  __ bind(&non_smi);
  __ LoadMap(t1, a1);
  __ GetInstanceTypeRange(t1, t2, FIRST_JS_FUNCTION_TYPE, t4);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, Uless_equal, t4,
          Operand(LAST_JS_FUNCTION_TYPE - FIRST_JS_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_BOUND_FUNCTION_TYPE));

  // Check if target has a [[Call]] internal method.
  __ Lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t1, t1, Operand(Map::Bits1::IsCallableBit::kMask));
  __ Branch(&non_callable, eq, t1, Operand(zero_reg));

  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET, eq,
          t2, Operand(JS_PROXY_TYPE));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  // Overwrite the original receiver with the (original) target.
  __ StoreReceiver(a1, a0, kScratchReg);
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

void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (checked to be a JSFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(a1);
  __ AssertFunction(a1);

  // Calling convention for function specific ConstructStubs require
  // a2 to contain either an AllocationSite or undefined.
  __ LoadRoot(a2, RootIndex::kUndefinedValue);

  Label call_generic_stub;

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  __ Ld(a4, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ Lwu(a4, FieldMemOperand(a4, SharedFunctionInfo::kFlagsOffset));
  __ And(a4, a4, Operand(SharedFunctionInfo::ConstructAsBuiltinBit::kMask));
  __ Branch(&call_generic_stub, eq, a4, Operand(zero_reg));

  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET);

  __ bind(&call_generic_stub);
  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(a1);
  __ AssertBoundFunction(a1);

  // Load [[BoundArguments]] into a2 and length of that into a4.
  __ Ld(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ SmiUntag(a4, FieldMemOperand(a2, FixedArray::kLengthOffset));

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- a3 : the new target (checked to be a constructor)
  //  -- a4 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ Sll64(a5, a4, kPointerSizeLog2);
    __ Sub64(t0, sp, Operand(a5));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    __ LoadStackLimit(kScratchReg,
                      MacroAssembler::StackLimitKind::kRealStackLimit);
    __ Branch(&done, Ugreater_equal, t0, Operand(kScratchReg));
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowStackOverflow);
    }
    __ bind(&done);
  }

  // Pop receiver.
  __ Pop(t0);

  // Push [[BoundArguments]].
  {
    Label loop, done_loop;
    __ SmiUntag(a4, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ Add64(a0, a0, Operand(a4));
    __ Add64(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Sub64(a4, a4, Operand(1));
    __ Branch(&done_loop, lt, a4, Operand(zero_reg));
    __ CalcScaledAddress(a5, a2, a4, kPointerSizeLog2);
    __ Ld(kScratchReg, MemOperand(a5));
    __ Push(kScratchReg);
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Push receiver.
  __ Push(t0);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label skip_load;
    __ Branch(&skip_load, ne, a1, Operand(a3));
    __ Ld(a3, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&skip_load);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ Ld(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
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

  // Check if target has a [[Construct]] internal method.
  __ Ld(t1, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Lbu(t4, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t4, t4, Operand(Map::Bits1::IsConstructorBit::kMask));
  __ Branch(&non_constructor, eq, t4, Operand(zero_reg));

  // Dispatch based on instance type.
  __ GetInstanceTypeRange(t1, t2, FIRST_JS_FUNCTION_TYPE, t0);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, Uless_equal, t0,
          Operand(LAST_JS_FUNCTION_TYPE - FIRST_JS_FUNCTION_TYPE));

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
    __ StoreReceiver(a1, a0, kScratchReg);
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

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was put in t0 by the jump table trampoline.
  // Convert to Smi for the runtime call
  __ SmiTag(kWasmCompileLazyFuncIndexRegister);
  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameScope scope(masm, StackFrame::WASM_COMPILE_LAZY);

    // Save all parameter registers (see kGpParamRegisters in wasm-linkage.cc).
    // They might be overwritten in the runtime call below. We don't have any
    // callee-saved registers in wasm, so no need to store anything else.
    RegList gp_regs = 0;
    for (Register gp_param_reg : wasm::kGpParamRegisters) {
      gp_regs |= gp_param_reg.bit();
    }
    // Also push x1, because we must push multiples of 16 bytes (see
    // {TurboAssembler::PushCPURegList}.
    CHECK_EQ(0, NumRegs(gp_regs) % 2);

    RegList fp_regs = 0;
    for (DoubleRegister fp_param_reg : wasm::kFpParamRegisters) {
      fp_regs |= fp_param_reg.bit();
    }

    CHECK_EQ(NumRegs(gp_regs), arraysize(wasm::kGpParamRegisters));
    CHECK_EQ(NumRegs(fp_regs), arraysize(wasm::kFpParamRegisters));
    CHECK_EQ(WasmCompileLazyFrameConstants::kNumberOfSavedGpParamRegs,
             NumRegs(gp_regs));
    CHECK_EQ(WasmCompileLazyFrameConstants::kNumberOfSavedFpParamRegs,
             NumRegs(fp_regs));
    __ MultiPush(gp_regs);
    __ MultiPushFPU(fp_regs);

    // Pass instance and function index as an explicit arguments to the runtime
    // function.
    __ Push(kWasmInstanceRegister, kWasmCompileLazyFuncIndexRegister);
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Move(kContextRegister, Smi::zero());
    __ CallRuntime(Runtime::kWasmCompileLazy, 2);

    __ Move(s1, a0);  // move return value to s1 since a0 will be restored to
                      // the value before the call

    // Restore registers.
    __ MultiPopFPU(fp_regs);
    __ MultiPop(gp_regs);
  }
  // Finally, jump to the entrypoint.
  __ Jump(s1);
}

void Builtins::Generate_WasmDebugBreak(MacroAssembler* masm) {
  HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
  {
    FrameScope scope(masm, StackFrame::WASM_DEBUG_BREAK);

    // Save all parameter registers. They might hold live values, we restore
    // them after the runtime call.
    __ MultiPush(WasmDebugBreakFrameConstants::kPushedGpRegs);
    __ MultiPushFPU(WasmDebugBreakFrameConstants::kPushedFpRegs);

    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Move(cp, Smi::zero());
    __ CallRuntime(Runtime::kWasmDebugBreak, 0);

    // Restore registers.
    __ MultiPopFPU(WasmDebugBreakFrameConstants::kPushedFpRegs);
    __ MultiPop(WasmDebugBreakFrameConstants::kPushedGpRegs);
  }
  __ Ret();
}

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               SaveFPRegsMode save_doubles, ArgvMode argv_mode,
                               bool builtin_exit_frame) {
  // Called from JavaScript; parameters are on stack as if calling JS function
  // a0: number of arguments including receiver
  // a1: pointer to builtin function
  // fp: frame pointer    (restored after C call)
  // sp: stack pointer    (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)
  //
  // If argv_mode == kArgvInRegister:
  // a2: pointer to the first argument

  if (argv_mode == kArgvInRegister) {
    // Move argv into the correct register.
    __ Move(s1, a2);
  } else {
    // Compute the argv pointer in a callee-saved register.
    __ CalcScaledAddress(s1, sp, a0, kPointerSizeLog2);
    __ Sub64(s1, s1, kPointerSize);
  }

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(
      save_doubles == kSaveFPRegs, 0,
      builtin_exit_frame ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);

  // s3: number of arguments  including receiver (C callee-saved)
  // s1: pointer to first argument (C callee-saved)
  // s2: pointer to builtin function (C callee-saved)

  // Prepare arguments for C routine.
  // a0 = argc
  __ Move(s3, a0);
  __ Move(s2, a1);

  // We are calling compiled C/C++ code. a0 and a1 hold our two arguments. We
  // also need to reserve the 4 argument slots on the stack.

  __ AssertStackIsAligned();

  // a0 = argc, a1 = argv, a2 = isolate
  __ li(a2, ExternalReference::isolate_address(masm->isolate()));
  __ Move(a1, s1);

  __ StoreReturnAddressAndCall(s2);

  // Result returned in a0 or a1:a0 - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ LoadRoot(a4, RootIndex::kException);
  __ Branch(&exception_returned, eq, a4, Operand(a0));

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    ExternalReference pending_exception_address = ExternalReference::Create(
        IsolateAddressId::kPendingExceptionAddress, masm->isolate());
    __ li(a2, pending_exception_address);
    __ Ld(a2, MemOperand(a2));
    __ LoadRoot(a4, RootIndex::kTheHoleValue);
    // Cannot use check here as it attempts to generate call into runtime.
    __ Branch(&okay, eq, a4, Operand(a2));
    __ stop();
    __ bind(&okay);
  }

  // Exit C frame and return.
  // a0:a1: result
  // sp: stack pointer
  // fp: frame pointer
  Register argc = argv_mode == kArgvInRegister
                      // We don't want to pop arguments so set argc to no_reg.
                      ? no_reg
                      // s3: still holds argc (callee-saved).
                      : s3;
  __ LeaveExitFrame(save_doubles == kSaveFPRegs, argc, EMIT_RETURN);

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

  // Ask the runtime for help to determine the handler. This will set a0 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler =
      ExternalReference::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, 0, a0);
    __ Move(a0, zero_reg);
    __ Move(a1, zero_reg);
    __ li(a2, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ li(cp, pending_handler_context_address);
  __ Ld(cp, MemOperand(cp));
  __ li(sp, pending_handler_sp_address);
  __ Ld(sp, MemOperand(sp));
  __ li(fp, pending_handler_fp_address);
  __ Ld(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label zero;
  __ Branch(&zero, eq, cp, Operand(zero_reg));
  __ Sd(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ bind(&zero);

  // Reset the masking register. This is done independent of the underlying
  // feature flag {FLAG_untrusted_code_mitigations} to make the snapshot work
  // with both configurations. It is safe to always do this, because the
  // underlying register is caller-saved and can be arbitrarily clobbered.
  __ ResetSpeculationPoisonRegister();

  // Compute the handler entry address and jump to it.
  UseScratchRegisterScope temp(masm);
  Register scratch = temp.Acquire();
  __ li(scratch, pending_handler_entrypoint_address);
  __ Ld(scratch, MemOperand(scratch));
  __ Jump(scratch);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label done;
  Register result_reg = t0;

  Register scratch = GetRegisterThatIsNotOneOf(result_reg);
  Register scratch2 = GetRegisterThatIsNotOneOf(result_reg, scratch);
  Register scratch3 = GetRegisterThatIsNotOneOf(result_reg, scratch, scratch2);
  DoubleRegister double_scratch = kScratchDoubleReg;

  // Account for saved regs.
  const int kArgumentOffset = 4 * kPointerSize;

  __ Push(result_reg);
  __ Push(scratch, scratch2, scratch3);

  // Load double input.
  __ LoadDouble(double_scratch, MemOperand(sp, kArgumentOffset));

  // Try a conversion to a signed integer, if exception occurs, scratch is
  // set to 0
  __ Trunc_w_d(scratch3, double_scratch, scratch);

  // If we had no exceptions then set result_reg and we are done.
  Label error;
  __ Branch(&error, eq, scratch, Operand(zero_reg));
  __ Move(result_reg, scratch3);
  __ Branch(&done);
  __ bind(&error);

  // Load the double value and perform a manual truncation.
  Register input_high = scratch2;
  Register input_low = scratch3;

  __ Lw(input_low, MemOperand(sp, kArgumentOffset + Register::kMantissaOffset));
  __ Lw(input_high,
        MemOperand(sp, kArgumentOffset + Register::kExponentOffset));

  Label normal_exponent;
  // Extract the biased exponent in result.
  __ ExtractBits(result_reg, input_high, HeapNumber::kExponentShift,
                 HeapNumber::kExponentBits);

  // Check for Infinity and NaNs, which should return 0.
  __ Sub32(scratch, result_reg, HeapNumber::kExponentMask);
  __ LoadZeroIfConditionZero(
      result_reg,
      scratch);  // result_reg = scratch == 0 ? 0 : result_reg
  __ Branch(&done, eq, scratch, Operand(zero_reg));

  // Express exponent as delta to (number of mantissa bits + 31).
  __ Sub32(result_reg, result_reg,
           Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits + 31));

  // If the delta is strictly positive, all bits would be shifted away,
  // which means that we can return 0.
  __ Branch(&normal_exponent, le, result_reg, Operand(zero_reg));
  __ Move(result_reg, zero_reg);
  __ Branch(&done);

  __ bind(&normal_exponent);
  const int kShiftBase = HeapNumber::kNonMantissaBitsInTopWord - 1;
  // Calculate shift.
  __ Add32(scratch, result_reg,
           Operand(kShiftBase + HeapNumber::kMantissaBits));

  // Save the sign.
  Register sign = result_reg;
  result_reg = no_reg;
  __ And(sign, input_high, Operand(HeapNumber::kSignMask));

  // We must specially handle shifts greater than 31.
  Label high_shift_needed, high_shift_done;
  __ Branch(&high_shift_needed, lt, scratch, Operand(32));
  __ Move(input_high, zero_reg);
  __ Branch(&high_shift_done);
  __ bind(&high_shift_needed);

  // Set the implicit 1 before the mantissa part in input_high.
  __ Or(input_high, input_high,
        Operand(1 << HeapNumber::kMantissaBitsInTopWord));
  // Shift the mantissa bits to the correct position.
  // We don't need to clear non-mantissa bits as they will be shifted away.
  // If they weren't, it would mean that the answer is in the 32bit range.
  __ Sll32(input_high, input_high, scratch);

  __ bind(&high_shift_done);

  // Replace the shifted bits with bits from the lower mantissa word.
  Label pos_shift, shift_done, sign_negative;
  __ li(kScratchReg, 32);
  __ subw(scratch, kScratchReg, scratch);
  __ Branch(&pos_shift, ge, scratch, Operand(zero_reg));

  // Negate scratch.
  __ Sub32(scratch, zero_reg, scratch);
  __ Sll32(input_low, input_low, scratch);
  __ Branch(&shift_done);

  __ bind(&pos_shift);
  __ srlw(input_low, input_low, scratch);

  __ bind(&shift_done);
  __ Or(input_high, input_high, Operand(input_low));
  // Restore sign if necessary.
  __ Move(scratch, sign);
  result_reg = sign;
  sign = no_reg;
  __ Sub32(result_reg, zero_reg, input_high);
  __ Branch(&sign_negative, ne, scratch, Operand(zero_reg));
  __ Move(result_reg, input_high);
  __ bind(&sign_negative);

  __ bind(&done);

  __ Sd(result_reg, MemOperand(sp, kArgumentOffset));
  __ Pop(scratch, scratch2, scratch3);
  __ Pop(result_reg);
  __ Ret();
}

void Builtins::Generate_GenericJSToWasmWrapper(MacroAssembler* masm) {
  // TODO(v8:10701): Implement for this platform.
  __ Trap();
}

namespace {

int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  int64_t offset = (ref0.address() - ref1.address());
  DCHECK(static_cast<int>(offset) == offset);
  return static_cast<int>(offset);
}

// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Restores context.  stack_space
// - space to be unwound on exit (includes the call JS arguments space and
// the additional space allocated for the fast call).
void CallApiFunctionAndReturn(MacroAssembler* masm, Register function_address,
                              ExternalReference thunk_ref, int stack_space,
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

  DCHECK(function_address == a1 || function_address == a2);

  Label profiler_enabled, end_profiler_check;
  {
    UseScratchRegisterScope temp(masm);
    Register scratch = temp.Acquire();
    __ li(scratch, ExternalReference::is_profiling_address(isolate));
    __ Lb(scratch, MemOperand(scratch, 0));
    __ Branch(&profiler_enabled, ne, scratch, Operand(zero_reg));
    __ li(scratch, ExternalReference::address_of_runtime_stats_flag());
    __ Lw(scratch, MemOperand(scratch, 0));
    __ Branch(&profiler_enabled, ne, scratch, Operand(zero_reg));
    {
      // Call the api function directly.
      __ Move(scratch, function_address);
      __ Branch(&end_profiler_check);
    }

    __ bind(&profiler_enabled);
    {
      // Additional parameter is the address of the actual callback.
      __ li(scratch, thunk_ref);
    }
    __ bind(&end_profiler_check);

    // Allocate HandleScope in callee-save registers.
    __ li(s5, next_address);
    __ Ld(s3, MemOperand(s5, kNextOffset));
    __ Ld(s1, MemOperand(s5, kLimitOffset));
    __ Lw(s2, MemOperand(s5, kLevelOffset));
    __ Add32(s2, s2, Operand(1));
    __ Sw(s2, MemOperand(s5, kLevelOffset));

    __ StoreReturnAddressAndCall(scratch);
  }

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // Load value from ReturnValue.
  __ Ld(a0, return_value_operand);
  __ bind(&return_value_loaded);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ Sd(s3, MemOperand(s5, kNextOffset));
  if (__ emit_debug_code()) {
    __ Lw(a1, MemOperand(s5, kLevelOffset));
    __ Check(eq, AbortReason::kUnexpectedLevelAfterReturnFromApiCall, a1,
             Operand(s2));
  }
  __ Sub32(s2, s2, Operand(1));
  __ Sw(s2, MemOperand(s5, kLevelOffset));
  __ Ld(kScratchReg, MemOperand(s5, kLimitOffset));
  __ Branch(&delete_allocated_handles, ne, s1, Operand(kScratchReg));

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);

  if (stack_space_operand == nullptr) {
    DCHECK_NE(stack_space, 0);
    __ li(s3, Operand(stack_space));
  } else {
    DCHECK_EQ(stack_space, 0);
    STATIC_ASSERT(kCArgSlotCount == 0);
    __ Ld(s3, *stack_space_operand);
  }

  static constexpr bool kDontSaveDoubles = false;
  static constexpr bool kRegisterContainsSlotCount = false;
  __ LeaveExitFrame(kDontSaveDoubles, s3, NO_EMIT_RETURN,
                    kRegisterContainsSlotCount);

  // Check if the function scheduled an exception.
  __ LoadRoot(a4, RootIndex::kTheHoleValue);
  __ li(kScratchReg, ExternalReference::scheduled_exception_address(isolate));
  __ Ld(a5, MemOperand(kScratchReg));
  __ Branch(&promote_scheduled_exception, ne, a4, Operand(a5));

  __ Ret();

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ Sd(s1, MemOperand(s5, kLimitOffset));
  __ Move(s3, a0);
  __ PrepareCallCFunction(1, s1);
  __ li(a0, ExternalReference::isolate_address(isolate));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(), 1);
  __ Move(a0, s3);
  __ Branch(&leave_exit_frame);
}

}  // namespace

void Builtins::Generate_CallApiCallback(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- cp                  : context
  //  -- a1                  : api function address
  //  -- a2                  : arguments count (not including the receiver)
  //  -- a3                  : call data
  //  -- a0                  : holder
  //  --
  //  -- sp[0]               : receiver
  //  -- sp[8]               : first argument
  //  -- ...
  //  -- sp[(argc) * 8]      : last argument
  // -----------------------------------

  Register api_function_address = a1;
  Register argc = a2;
  Register call_data = a3;
  Register holder = a0;
  Register scratch = t0;
  Register base = t1;  // For addressing MemOperands on the stack.

  DCHECK(!AreAliased(api_function_address, argc, call_data, holder, scratch,
                     base));

  using FCA = FunctionCallbackArguments;

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

  // Set up the base register for addressing through MemOperands. It will point
  // at the receiver (located at sp + argc * kPointerSize).
  __ CalcScaledAddress(base, sp, argc, kPointerSizeLog2);

  // Reserve space on the stack.
  __ Sub64(sp, sp, Operand(FCA::kArgsLength * kPointerSize));

  // kHolder.
  __ Sd(holder, MemOperand(sp, 0 * kPointerSize));

  // kIsolate.
  __ li(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ Sd(scratch, MemOperand(sp, 1 * kPointerSize));

  // kReturnValueDefaultValue and kReturnValue.
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ Sd(scratch, MemOperand(sp, 2 * kPointerSize));
  __ Sd(scratch, MemOperand(sp, 3 * kPointerSize));

  // kData.
  __ Sd(call_data, MemOperand(sp, 4 * kPointerSize));

  // kNewTarget.
  __ Sd(scratch, MemOperand(sp, 5 * kPointerSize));

  // Keep a pointer to kHolder (= implicit_args) in a scratch register.
  // We use it below to set up the FunctionCallbackInfo object.
  __ Move(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  static constexpr int kApiStackSpace = 4;
  static constexpr bool kDontSaveDoubles = false;
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(kDontSaveDoubles, kApiStackSpace);

  // EnterExitFrame may align the sp.

  // FunctionCallbackInfo::implicit_args_ (points at kHolder as set up above).
  // Arguments are after the return address (pushed by EnterExitFrame()).
  __ Sd(scratch, MemOperand(sp, 1 * kPointerSize));

  // FunctionCallbackInfo::values_ (points at the first varargs argument passed
  // on the stack).
  __ Add64(scratch, scratch,
           Operand((FCA::kArgsLength + 1) * kSystemPointerSize));
  __ Sd(scratch, MemOperand(sp, 2 * kPointerSize));

  // FunctionCallbackInfo::length_.
  // Stored as int field, 32-bit integers within struct on stack always left
  // justified by n64 ABI.
  __ Sw(argc, MemOperand(sp, 3 * kPointerSize));

  // We also store the number of bytes to drop from the stack after returning
  // from the API function here.
  // Note: Unlike on other architectures, this stores the number of slots to
  // drop, not the number of bytes.
  __ Add64(scratch, argc, Operand(FCA::kArgsLength + 1 /* receiver */));
  __ Sd(scratch, MemOperand(sp, 4 * kPointerSize));

  // v8::InvocationCallback's argument.
  DCHECK(!AreAliased(api_function_address, scratch, a0));
  __ Add64(a0, sp, Operand(1 * kPointerSize));

  ExternalReference thunk_ref = ExternalReference::invoke_function_callback();

  // There are two stack slots above the arguments we constructed on the stack.
  // TODO(jgruber): Document what these arguments are.
  static constexpr int kStackSlotsAboveFCA = 2;
  MemOperand return_value_operand(
      fp, (kStackSlotsAboveFCA + FCA::kReturnValueOffset) * kPointerSize);

  static constexpr int kUseStackSpaceOperand = 0;
  MemOperand stack_space_operand(sp, 4 * kPointerSize);

  AllowExternalCallThatCantCauseGC scope(masm);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kUseStackSpaceOperand, &stack_space_operand,
                           return_value_operand);
}

void Builtins::Generate_CallApiGetter(MacroAssembler* masm) {
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
  Register scratch = a4;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  Register api_function_address = a2;

  // Here and below +1 is for name() pushed after the args_ array.
  using PCA = PropertyCallbackArguments;
  __ Sub64(sp, sp, (PCA::kArgsLength + 1) * kPointerSize);
  __ Sd(receiver, MemOperand(sp, (PCA::kThisIndex + 1) * kPointerSize));
  __ Ld(scratch, FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ Sd(scratch, MemOperand(sp, (PCA::kDataIndex + 1) * kPointerSize));
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ Sd(scratch, MemOperand(sp, (PCA::kReturnValueOffset + 1) * kPointerSize));
  __ Sd(scratch, MemOperand(sp, (PCA::kReturnValueDefaultValueIndex + 1) *
                                    kPointerSize));
  __ li(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ Sd(scratch, MemOperand(sp, (PCA::kIsolateIndex + 1) * kPointerSize));
  __ Sd(holder, MemOperand(sp, (PCA::kHolderIndex + 1) * kPointerSize));
  // should_throw_on_error -> false
  DCHECK_EQ(0, Smi::zero().ptr());
  __ Sd(zero_reg,
        MemOperand(sp, (PCA::kShouldThrowOnErrorIndex + 1) * kPointerSize));
  __ Ld(scratch, FieldMemOperand(callback, AccessorInfo::kNameOffset));
  __ Sd(scratch, MemOperand(sp, 0 * kPointerSize));

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ Move(a0, sp);                              // a0 = Handle<Name>
  __ Add64(a1, a0, Operand(1 * kPointerSize));  // a1 = v8::PCI::args_

  const int kApiStackSpace = 1;
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ Sd(a1, MemOperand(sp, 1 * kPointerSize));
  __ Add64(a1, sp, Operand(1 * kPointerSize));
  // a1 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback();

  __ Ld(scratch, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ Ld(api_function_address,
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
  // The sole purpose of DirectCEntry is for movable callers (e.g. any general
  // purpose Code object) to be able to call into C functions that may trigger
  // GC and thus move the caller.
  //
  // DirectCEntry places the return address on the stack (updated by the GC),
  // making the call GC safe. The irregexp backend relies on this.

  // Make place for arguments to fit C calling convention. Callers use
  // EnterExitFrame/LeaveExitFrame so they handle stack restoring and we don't
  // have to do that here. Any caller must drop kCArgsSlotsSize stack space
  // after the call.
  __ Add64(sp, sp, -kCArgsSlotsSize);

  __ Sd(ra, MemOperand(sp, kCArgsSlotsSize));  // Store the return address.
  __ Call(t6);                                 // Call the C++ function.
  __ Ld(t6, MemOperand(sp, kCArgsSlotsSize));  // Return to calling code.

  if (FLAG_debug_code && FLAG_enable_slow_asserts) {
    // In case of an error the return address may point to a memory area
    // filled with kZapValue by the GC. Dereference the address and check for
    // this.
    __ Uld(a4, MemOperand(t6));
    __ Assert(ne, AbortReason::kReceivedInvalidReturnAddress, a4,
              Operand(reinterpret_cast<uint64_t>(kZapValue)));
  }

  __ Jump(t6);
}

namespace {

// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Generate_DeoptimizationEntry(MacroAssembler* masm,
                                  DeoptimizeKind deopt_kind) {
  Isolate* isolate = masm->isolate();

  // Unlike on ARM we don't save all the registers, just the useful ones.
  // For the rest, there are gaps on the stack, so the offsets remain the same.
  const int kNumberOfRegisters = Register::kNumRegisters;

  RegList restored_regs = kJSCallerSaved | kCalleeSaved;
  RegList saved_regs = restored_regs | sp.bit() | ra.bit();

  const int kDoubleRegsSize = kDoubleSize * DoubleRegister::kNumRegisters;

  // Save all double FPU registers before messing with them.
  __ Sub64(sp, sp, Operand(kDoubleRegsSize));
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister fpu_reg = DoubleRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ StoreDouble(fpu_reg, MemOperand(sp, offset));
  }

  // Push saved_regs (needed to populate FrameDescription::registers_).
  // Leave gaps for other registers.
  __ Sub64(sp, sp, kNumberOfRegisters * kPointerSize);
  for (int16_t i = kNumberOfRegisters - 1; i >= 0; i--) {
    if ((saved_regs & (1 << i)) != 0) {
      __ Sd(ToRegister(i), MemOperand(sp, kPointerSize * i));
    }
  }

  __ li(a2,
        ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate));
  __ Sd(fp, MemOperand(a2));

  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kPointerSize) + kDoubleRegsSize;

  __ li(a2, Operand(Deoptimizer::kFixedExitSizeMarker));
  // Get the address of the location in the code object (a3) (return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register a4.
  __ Move(a3, ra);
  __ Add64(a4, sp, Operand(kSavedRegistersAreaSize));

  __ Sub64(a4, fp, a4);

  // Allocate a new deoptimizer object.
  __ PrepareCallCFunction(6, a5);
  // Pass six arguments, according to n64 ABI.
  __ Move(a0, zero_reg);
  Label context_check;
  __ Ld(a1, MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(a1, &context_check);
  __ Ld(a0, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ li(a1, Operand(static_cast<int>(deopt_kind)));
  // a2: bailout id already loaded.
  // a3: code address or 0 already loaded.
  // a4: already has fp-to-sp delta.
  __ li(a5, ExternalReference::isolate_address(isolate));

  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::new_deoptimizer_function(), 6);
  }

  // Preserve "deoptimizer" object in register a0 and get the input
  // frame descriptor pointer to a1 (deoptimizer->input_);
  __ Ld(a1, MemOperand(a0, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  DCHECK_EQ(Register::kNumRegisters, kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    if ((saved_regs & (1 << i)) != 0) {
      __ Ld(a2, MemOperand(sp, i * kPointerSize));
      __ Sd(a2, MemOperand(a1, offset));
    } else if (FLAG_debug_code) {
      __ li(a2, kDebugZapValue);
      __ Sd(a2, MemOperand(a1, offset));
    }
  }

  int double_regs_offset = FrameDescription::double_registers_offset();
  // Copy FPU registers to
  // double_registers_[DoubleRegister::kNumAllocatableRegisters]
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    int dst_offset = code * kDoubleSize + double_regs_offset;
    int src_offset = code * kDoubleSize + kNumberOfRegisters * kPointerSize;
    __ LoadDouble(ft0, MemOperand(sp, src_offset));
    __ StoreDouble(ft0, MemOperand(a1, dst_offset));
  }

  // Remove the saved registers from the stack.
  __ Add64(sp, sp, Operand(kSavedRegistersAreaSize));

  // Compute a pointer to the unwinding limit in register a2; that is
  // the first stack slot not part of the input frame.
  __ Ld(a2, MemOperand(a1, FrameDescription::frame_size_offset()));
  __ Add64(a2, a2, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ Add64(a3, a1, Operand(FrameDescription::frame_content_offset()));
  Label pop_loop;
  Label pop_loop_header;
  __ BranchShort(&pop_loop_header);
  __ bind(&pop_loop);
  __ pop(a4);
  __ Sd(a4, MemOperand(a3, 0));
  __ Add64(a3, a3, sizeof(uint64_t));
  __ bind(&pop_loop_header);
  __ BranchShort(&pop_loop, ne, a2, Operand(sp));
  // Compute the output frame in the deoptimizer.
  __ push(a0);  // Preserve deoptimizer object across call.
  // a0: deoptimizer object; a1: scratch.
  __ PrepareCallCFunction(1, a1);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
  }
  __ pop(a0);  // Restore deoptimizer object (class Deoptimizer).

  __ Ld(sp, MemOperand(a0, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop, outer_loop_header, inner_loop_header;
  // Outer loop state: a4 = current "FrameDescription** output_",
  // a1 = one past the last FrameDescription**.
  __ Lw(a1, MemOperand(a0, Deoptimizer::output_count_offset()));
  __ Ld(a4, MemOperand(a0, Deoptimizer::output_offset()));  // a4 is output_.
  __ CalcScaledAddress(a1, a4, a1, kPointerSizeLog2);
  __ BranchShort(&outer_loop_header);
  __ bind(&outer_push_loop);
  // Inner loop state: a2 = current FrameDescription*, a3 = loop index.
  __ Ld(a2, MemOperand(a4, 0));  // output_[ix]
  __ Ld(a3, MemOperand(a2, FrameDescription::frame_size_offset()));
  __ BranchShort(&inner_loop_header);
  __ bind(&inner_push_loop);
  __ Sub64(a3, a3, Operand(sizeof(uint64_t)));
  __ Add64(a6, a2, Operand(a3));
  __ Ld(a7, MemOperand(a6, FrameDescription::frame_content_offset()));
  __ push(a7);
  __ bind(&inner_loop_header);
  __ BranchShort(&inner_push_loop, ne, a3, Operand(zero_reg));

  __ Add64(a4, a4, Operand(kPointerSize));
  __ bind(&outer_loop_header);
  __ BranchShort(&outer_push_loop, lt, a4, Operand(a1));

  __ Ld(a1, MemOperand(a0, Deoptimizer::input_offset()));
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister fpu_reg = DoubleRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ LoadDouble(fpu_reg, MemOperand(a1, src_offset));
  }

  // Push pc and continuation from the last output frame.
  __ Ld(a6, MemOperand(a2, FrameDescription::pc_offset()));
  __ push(a6);
  __ Ld(a6, MemOperand(a2, FrameDescription::continuation_offset()));
  __ push(a6);

  // Technically restoring 't3' should work unless zero_reg is also restored
  // but it's safer to check for this.
  DCHECK(!(t3.bit() & restored_regs));
  // Restore the registers from the last output frame.
  __ Move(t3, a2);
  for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    if ((restored_regs & (1 << i)) != 0) {
      __ Ld(ToRegister(i), MemOperand(t3, offset));
    }
  }

  __ pop(t3);  // Get continuation, leave pc on stack.
  __ pop(ra);
  __ Jump(t3);
  __ stop();
}

}  // namespace

void Builtins::Generate_DeoptimizationEntry_Eager(MacroAssembler* masm) {
  Generate_DeoptimizationEntry(masm, DeoptimizeKind::kEager);
}

void Builtins::Generate_DeoptimizationEntry_Soft(MacroAssembler* masm) {
  Generate_DeoptimizationEntry(masm, DeoptimizeKind::kSoft);
}

void Builtins::Generate_DeoptimizationEntry_Bailout(MacroAssembler* masm) {
  Generate_DeoptimizationEntry(masm, DeoptimizeKind::kBailout);
}

void Builtins::Generate_DeoptimizationEntry_Lazy(MacroAssembler* masm) {
  Generate_DeoptimizationEntry(masm, DeoptimizeKind::kLazy);
}

void Builtins::Generate_DynamicCheckMapsTrampoline(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterFrame(StackFrame::INTERNAL);

  // Only save the registers that the DynamicMapChecks builtin can clobber.
  DynamicCheckMapsDescriptor descriptor;
  RegList registers = descriptor.allocatable_registers();
  // FLAG_debug_code is enabled CSA checks will call C function and so we need
  // to save all CallerSaved registers too.
  if (FLAG_debug_code) registers |= kJSCallerSaved;
  __ SaveRegisters(registers);

  // Load the immediate arguments from the deopt exit to pass to the builtin.
  Register slot_arg =
      descriptor.GetRegisterParameter(DynamicCheckMapsDescriptor::kSlot);
  Register handler_arg =
      descriptor.GetRegisterParameter(DynamicCheckMapsDescriptor::kHandler);
  __ Ld(handler_arg, MemOperand(fp, CommonFrameConstants::kCallerPCOffset));
  __ Uld(slot_arg, MemOperand(handler_arg,
                              Deoptimizer::kEagerWithResumeImmedArgs1PcOffset));
  __ Uld(
      handler_arg,
      MemOperand(handler_arg, Deoptimizer::kEagerWithResumeImmedArgs2PcOffset));
  __ Call(BUILTIN_CODE(masm->isolate(), DynamicCheckMaps),
          RelocInfo::CODE_TARGET);

  Label deopt, bailout;
  __ Branch(&deopt, ne, a0,
            Operand(static_cast<int>(DynamicCheckMapsStatus::kSuccess)));

  __ RestoreRegisters(registers);
  __ LeaveFrame(StackFrame::INTERNAL);
  __ Ret();

  __ bind(&deopt);
  __ Branch(&bailout, eq, a0,
            Operand(static_cast<int>(DynamicCheckMapsStatus::kBailout)));

  if (FLAG_debug_code) {
    __ Assert(eq, AbortReason::kUnexpectedDynamicCheckMapsStatus, a0,
              Operand(static_cast<int>(DynamicCheckMapsStatus::kDeopt)));
  }
  __ RestoreRegisters(registers);
  __ LeaveFrame(StackFrame::INTERNAL);
  Handle<Code> deopt_eager = masm->isolate()->builtins()->builtin_handle(
      Deoptimizer::GetDeoptimizationEntry(DeoptimizeKind::kEager));
  __ Jump(deopt_eager, RelocInfo::CODE_TARGET);

  __ bind(&bailout);
  __ RestoreRegisters(registers);
  __ LeaveFrame(StackFrame::INTERNAL);
  Handle<Code> deopt_bailout = masm->isolate()->builtins()->builtin_handle(
      Deoptimizer::GetDeoptimizationEntry(DeoptimizeKind::kBailout));
  __ Jump(deopt_bailout, RelocInfo::CODE_TARGET);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_RISCV64

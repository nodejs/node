// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64

#include "src/api/api-arguments.h"
#include "src/codegen/code-factory.h"
// For interpreter_entry_return_pc_offset. TODO(jkummerow): Drop.
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/register-configuration.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"
#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"
#include "src/objects/cell.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-generator.h"
#include "src/objects/smi.h"
#include "src/runtime/runtime.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address) {
  __ Move(kJavaScriptCallExtraArg1Register, ExternalReference::Create(address));
  __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
          RelocInfo::CODE_TARGET);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- r3 : actual argument count
  //  -- r4 : target function (preserved for callee)
  //  -- r6 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the target function, the new target and the actual
    // argument count.
    // Push function as parameter to the runtime call.
    __ SmiTag(kJavaScriptCallArgCountRegister);
    __ Push(kJavaScriptCallTargetRegister, kJavaScriptCallNewTargetRegister,
            kJavaScriptCallArgCountRegister, kJavaScriptCallTargetRegister);

    __ CallRuntime(function_id, 1);
    __ mr(r5, r3);

    // Restore target function, new target and actual argument count.
    __ Pop(kJavaScriptCallTargetRegister, kJavaScriptCallNewTargetRegister,
           kJavaScriptCallArgCountRegister);
    __ SmiUntag(kJavaScriptCallArgCountRegister);
  }
  static_assert(kJavaScriptCallCodeStartRegister == r5, "ABI mismatch");
  __ JumpCodeObject(r5);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3     : number of arguments
  //  -- r4     : constructor function
  //  -- r6     : new target
  //  -- cp     : context
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  Register scratch = r5;

  Label stack_overflow;

  __ StackOverflowCheck(r3, scratch, &stack_overflow);
  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.

    __ SmiTag(r3);
    __ Push(cp, r3);
    __ SmiUntag(r3, SetRC);

    // TODO(victorgomes): When the arguments adaptor is completely removed, we
    // should get the formal parameter count and copy the arguments in its
    // correct position (including any undefined), instead of delaying this to
    // InvokeFunction.

    // Set up pointer to last argument (skip receiver).
    __ addi(
        r7, fp,
        Operand(StandardFrameConstants::kCallerSPOffset + kSystemPointerSize));
    // Copy arguments and receiver to the expression stack.
    __ PushArray(r7, r3, r8, r0);
    // The receiver for the builtin/api call.
    __ PushRoot(RootIndex::kTheHoleValue);

    // Call the function.
    // r3: number of arguments (untagged)
    // r4: constructor function
    // r6: new target
    {
      ConstantPoolUnavailableScope constant_pool_unavailable(masm);
      __ InvokeFunctionWithNewTarget(r4, r6, r3, CALL_FUNCTION);
    }

    // Restore context from the frame.
    __ LoadP(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ LoadP(scratch, MemOperand(fp, ConstructFrameConstants::kLengthOffset));

    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);

  __ SmiToPtrArrayOffset(scratch, scratch);
  __ add(sp, sp, scratch);
  __ addi(sp, sp, Operand(kSystemPointerSize));
  __ blr();

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bkpt(0);  // Unreachable code.
  }
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  --      r3: number of arguments (untagged)
  //  --      r4: constructor function
  //  --      r6: new target
  //  --      cp: context
  //  --      lr: return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  FrameScope scope(masm, StackFrame::MANUAL);
  // Enter a construct frame.
  Label post_instantiation_deopt_entry, not_create_implicit_receiver;
  __ EnterFrame(StackFrame::CONSTRUCT);

  // Preserve the incoming parameters on the stack.
  __ SmiTag(r3);
  __ Push(cp, r3, r4);
  __ PushRoot(RootIndex::kUndefinedValue);
  __ Push(r6);

  // ----------- S t a t e -------------
  //  --        sp[0*kSystemPointerSize]: new target
  //  --        sp[1*kSystemPointerSize]: padding
  //  -- r4 and sp[2*kSystemPointerSize]: constructor function
  //  --        sp[3*kSystemPointerSize]: number of arguments (tagged)
  //  --        sp[4*kSystemPointerSize]: context
  // -----------------------------------

  __ LoadTaggedPointerField(
      r7, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ lwz(r7, FieldMemOperand(r7, SharedFunctionInfo::kFlagsOffset));
  __ DecodeField<SharedFunctionInfo::FunctionKindBits>(r7);
  __ JumpIfIsInRange(r7, kDefaultDerivedConstructor, kDerivedConstructor,
                     &not_create_implicit_receiver);

  // If not derived class constructor: Allocate the new receiver object.
  __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1, r7,
                      r8);
  __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject), RelocInfo::CODE_TARGET);
  __ b(&post_instantiation_deopt_entry);

  // Else: use TheHoleValue as receiver for constructor call
  __ bind(&not_create_implicit_receiver);
  __ LoadRoot(r3, RootIndex::kTheHoleValue);

  // ----------- S t a t e -------------
  //  --                          r3: receiver
  //  -- Slot 4 / sp[0*kSystemPointerSize]: new target
  //  -- Slot 3 / sp[1*kSystemPointerSize]: padding
  //  -- Slot 2 / sp[2*kSystemPointerSize]: constructor function
  //  -- Slot 1 / sp[3*kSystemPointerSize]: number of arguments (tagged)
  //  -- Slot 0 / sp[4*kSystemPointerSize]: context
  // -----------------------------------
  // Deoptimizer enters here.
  masm->isolate()->heap()->SetConstructStubCreateDeoptPCOffset(
      masm->pc_offset());
  __ bind(&post_instantiation_deopt_entry);

  // Restore new target.
  __ Pop(r6);

  // Push the allocated receiver to the stack.
  __ Push(r3);
  // We need two copies because we may have to return the original one
  // and the calling conventions dictate that the called function pops the
  // receiver. The second copy is pushed after the arguments, we saved in r6
  // since r0 needs to store the number of arguments before
  // InvokingFunction.
  __ mr(r9, r3);

  // Set up pointer to first argument (skip receiver).
  __ addi(
      r7, fp,
      Operand(StandardFrameConstants::kCallerSPOffset + kSystemPointerSize));

  // ----------- S t a t e -------------
  //  --                 r6: new target
  //  -- sp[0*kSystemPointerSize]: implicit receiver
  //  -- sp[1*kSystemPointerSize]: implicit receiver
  //  -- sp[2*kSystemPointerSize]: padding
  //  -- sp[3*kSystemPointerSize]: constructor function
  //  -- sp[4*kSystemPointerSize]: number of arguments (tagged)
  //  -- sp[5*kSystemPointerSize]: context
  // -----------------------------------

  // Restore constructor function and argument count.
  __ LoadP(r4, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
  __ LoadP(r3, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
  __ SmiUntag(r3);

  Label stack_overflow;
  __ StackOverflowCheck(r3, r8, &stack_overflow);

  // Copy arguments and receiver to the expression stack.
  __ PushArray(r7, r3, r8, r0);

  // Push implicit receiver.
  __ Push(r9);

  // Call the function.
  {
    ConstantPoolUnavailableScope constant_pool_unavailable(masm);
    __ InvokeFunctionWithNewTarget(r4, r6, r3, CALL_FUNCTION);
  }

  // ----------- S t a t e -------------
  //  --                 r0: constructor result
  //  -- sp[0*kSystemPointerSize]: implicit receiver
  //  -- sp[1*kSystemPointerSize]: padding
  //  -- sp[2*kSystemPointerSize]: constructor function
  //  -- sp[3*kSystemPointerSize]: number of arguments
  //  -- sp[4*kSystemPointerSize]: context
  // -----------------------------------

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
      masm->pc_offset());

  // If the result is an object (in the ECMA sense), we should get rid
  // of the receiver and use the result; see ECMA-262 section 13.2.2-7
  // on page 74.
  Label use_receiver, do_throw, leave_and_return, check_receiver;

  // If the result is undefined, we jump out to using the implicit receiver.
  __ JumpIfNotRoot(r3, RootIndex::kUndefinedValue, &check_receiver);

  // Otherwise we do a smi check and fall through to check if the return value
  // is a valid receiver.

  // Throw away the result of the constructor invocation and use the
  // on-stack receiver as the result.
  __ bind(&use_receiver);
  __ LoadP(r3, MemOperand(sp));
  __ JumpIfRoot(r3, RootIndex::kTheHoleValue, &do_throw);

  __ bind(&leave_and_return);
  // Restore smi-tagged arguments count from the frame.
  __ LoadP(r4, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
  // Leave construct frame.
  __ LeaveFrame(StackFrame::CONSTRUCT);

  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);

  __ SmiToPtrArrayOffset(r4, r4);
  __ add(sp, sp, r4);
  __ addi(sp, sp, Operand(kSystemPointerSize));
  __ blr();

  __ bind(&check_receiver);
  // If the result is a smi, it is *not* an object in the ECMA sense.
  __ JumpIfSmi(r3, &use_receiver);

  // If the type of the result (stored in its map) is less than
  // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ CompareObjectType(r3, r7, r7, FIRST_JS_RECEIVER_TYPE);
  __ bge(&leave_and_return);
  __ b(&use_receiver);

  __ bind(&do_throw);
  // Restore the context from the frame.
  __ LoadP(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
  __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);
  __ bkpt(0);

  __ bind(&stack_overflow);
  // Restore the context from the frame.
  __ LoadP(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
  __ CallRuntime(Runtime::kThrowStackOverflow);
  // Unreachable code.
  __ bkpt(0);
}

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

static void GetSharedFunctionInfoBytecode(MacroAssembler* masm,
                                          Register sfi_data,
                                          Register scratch1) {
  Label done;

  __ CompareObjectType(sfi_data, scratch1, scratch1, INTERPRETER_DATA_TYPE);
  __ bne(&done);
  __ LoadTaggedPointerField(
      sfi_data,
      FieldMemOperand(sfi_data, InterpreterData::kBytecodeArrayOffset));
  __ bind(&done);
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
  __ StoreTaggedField(
      r3, FieldMemOperand(r4, JSGeneratorObject::kInputOrDebugPosOffset), r0);
  __ RecordWriteField(r4, JSGeneratorObject::kInputOrDebugPosOffset, r3, r6,
                      kLRHasNotBeenSaved, kDontSaveFPRegs);

  // Load suspended function and context.
  __ LoadTaggedPointerField(
      r7, FieldMemOperand(r4, JSGeneratorObject::kFunctionOffset));
  __ LoadTaggedPointerField(cp,
                            FieldMemOperand(r7, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  Register scratch = r8;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ Move(scratch, debug_hook);
  __ LoadByte(scratch, MemOperand(scratch), r0);
  __ extsb(scratch, scratch);
  __ CmpSmiLiteral(scratch, Smi::zero(), r0);
  __ bne(&prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.

  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());

  __ Move(scratch, debug_suspended_generator);
  __ LoadP(scratch, MemOperand(scratch));
  __ cmp(scratch, r4);
  __ beq(&prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ LoadStackLimit(scratch, StackLimitKind::kRealStackLimit);
  __ cmpl(sp, scratch);
  __ blt(&stack_overflow);

  // ----------- S t a t e -------------
  //  -- r4    : the JSGeneratorObject to resume
  //  -- r7    : generator function
  //  -- cp    : generator context
  //  -- lr    : return address
  // -----------------------------------

  // Copy the function arguments from the generator object's register file.
  __ LoadTaggedPointerField(
      r6, FieldMemOperand(r7, JSFunction::kSharedFunctionInfoOffset));
  __ LoadHalfWord(
      r6, FieldMemOperand(r6, SharedFunctionInfo::kFormalParameterCountOffset));
  __ LoadTaggedPointerField(
      r5,
      FieldMemOperand(r4, JSGeneratorObject::kParametersAndRegistersOffset));
  {
    Label done_loop, loop;
    __ mr(r9, r6);

    __ bind(&loop);
    __ subi(r9, r9, Operand(1));
    __ cmpi(r9, Operand::Zero());
    __ blt(&done_loop);
    __ ShiftLeftImm(r10, r9, Operand(kTaggedSizeLog2));
    __ add(scratch, r5, r10);
    __ LoadAnyTaggedField(scratch,
                          FieldMemOperand(scratch, FixedArray::kHeaderSize));
    __ Push(scratch);
    __ b(&loop);

    __ bind(&done_loop);

    // Push receiver.
    __ LoadAnyTaggedField(
        scratch, FieldMemOperand(r4, JSGeneratorObject::kReceiverOffset));
    __ Push(scratch);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ LoadTaggedPointerField(
        r6, FieldMemOperand(r7, JSFunction::kSharedFunctionInfoOffset));
    __ LoadTaggedPointerField(
        r6, FieldMemOperand(r6, SharedFunctionInfo::kFunctionDataOffset));
    GetSharedFunctionInfoBytecode(masm, r6, r3);
    __ CompareObjectType(r6, r6, r6, BYTECODE_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ LoadTaggedPointerField(
        r3, FieldMemOperand(r7, JSFunction::kSharedFunctionInfoOffset));
    __ LoadHalfWord(
        r3,
        FieldMemOperand(r3, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ mr(r6, r4);
    __ mr(r4, r7);
    static_assert(kJavaScriptCallCodeStartRegister == r5, "ABI mismatch");
    __ LoadTaggedPointerField(r5, FieldMemOperand(r4, JSFunction::kCodeOffset));
    __ JumpCodeObject(r5);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4, r7);
    // Push hole as receiver since we do not use it for stepping.
    __ PushRoot(RootIndex::kTheHoleValue);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(r4);
    __ LoadTaggedPointerField(
        r7, FieldMemOperand(r4, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(r4);
    __ LoadTaggedPointerField(
        r7, FieldMemOperand(r4, JSGeneratorObject::kFunctionOffset));
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
  // The register state is either:
  //   r3: root_register_value
  //   r4: code entry
  //   r5: function
  //   r6: receiver
  //   r7: argc
  //   r8: argv
  // or
  //   r3: root_register_value
  //   r4: microtask_queue

  Label invoke, handler_entry, exit;

  {
    NoRootArrayScope no_root_array(masm);

    // PPC LINUX ABI:
    // preserve LR in pre-reserved slot in caller's frame
    __ mflr(r0);
    __ StoreP(r0, MemOperand(sp, kStackFrameLRSlot * kSystemPointerSize));

    // Save callee saved registers on the stack.
    __ MultiPush(kCalleeSaved);

    // Save callee-saved double registers.
    __ MultiPushDoubles(kCalleeSavedDoubles);
    // Set up the reserved register for 0.0.
    __ LoadDoubleLiteral(kDoubleRegZero, Double(0.0), r0);

    // Initialize the root register.
    // C calling convention. The first argument is passed in r3.
    __ mr(kRootRegister, r3);
  }

  // Push a frame with special values setup to mark it as an entry frame.
  // r4: code entry
  // r5: function
  // r6: receiver
  // r7: argc
  // r8: argv
  __ li(r0, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  __ push(r0);
  if (FLAG_enable_embedded_constant_pool) {
    __ li(kConstantPoolRegister, Operand::Zero());
    __ push(kConstantPoolRegister);
  }
  __ mov(r0, Operand(StackFrame::TypeToMarker(type)));
  __ push(r0);
  __ push(r0);
  // Save copies of the top frame descriptor on the stack.
  __ Move(r3, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                        masm->isolate()));
  __ LoadP(r0, MemOperand(r3));
  __ push(r0);

  // Clear c_entry_fp, now we've pushed its previous value to the stack.
  // If the c_entry_fp is not already zero and we don't clear it, the
  // SafeStackFrameIterator will assume we are executing C++ and miss the JS
  // frames on top.
  __ li(r0, Operand::Zero());
  __ StoreP(r0, MemOperand(r3));

  Register scratch = r9;
  // Set up frame pointer for the frame to be pushed.
  __ addi(fp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp =
      ExternalReference::Create(IsolateAddressId::kJSEntrySPAddress,
                                masm->isolate());
  __ Move(r3, js_entry_sp);
  __ LoadP(scratch, MemOperand(r3));
  __ cmpi(scratch, Operand::Zero());
  __ bne(&non_outermost_js);
  __ StoreP(fp, MemOperand(r3));
  __ mov(scratch, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont);
  __ bind(&non_outermost_js);
  __ mov(scratch, Operand(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);
  __ push(scratch);  // frame-type

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ b(&invoke);

  // Block literal pool emission whilst taking the position of the handler
  // entry. This avoids making the assumption that literal pools are always
  // emitted after an instruction is emitted, rather than before.
  {
    ConstantPoolUnavailableScope constant_pool_unavailable(masm);
    __ bind(&handler_entry);

    // Store the current pc as the handler offset. It's used later to create the
    // handler table.
    masm->isolate()->builtins()->SetJSEntryHandlerOffset(handler_entry.pos());

    // Caught exception: Store result (exception) in the pending exception
    // field in the JSEnv and return a failure sentinel.  Coming in here the
    // fp will be invalid because the PushStackHandler below sets it to 0 to
    // signal the existence of the JSEntry frame.
    __ Move(scratch,
            ExternalReference::Create(
                IsolateAddressId::kPendingExceptionAddress, masm->isolate()));
  }

  __ StoreP(r3, MemOperand(scratch));
  __ LoadRoot(r3, RootIndex::kException);
  __ b(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  // Must preserve r4-r8.
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
  __ Call(trampoline_code, RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // r3 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(r8);
  __ cmpi(r8, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ bne(&non_outermost_js_2);
  __ mov(scratch, Operand::Zero());
  __ Move(r8, js_entry_sp);
  __ StoreP(scratch, MemOperand(r8));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(r6);
  __ Move(scratch, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                             masm->isolate()));
  __ StoreP(r6, MemOperand(scratch));

  // Reset the stack to the callee saved registers.
  __ addi(sp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

  // Restore callee-saved double registers.
  __ MultiPopDoubles(kCalleeSavedDoubles);

  // Restore callee-saved registers.
  __ MultiPop(kCalleeSaved);

  // Return
  __ LoadP(r0, MemOperand(sp, kStackFrameLRSlot * kSystemPointerSize));
  __ mtlr(r0);
  __ blr();
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
  // Called from Generate_JS_Entry
  // r4: new.target
  // r5: function
  // r6: receiver
  // r7: argc
  // r8: argv
  // r0,r3,r9, cp may be clobbered

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address = ExternalReference::Create(
        IsolateAddressId::kContextAddress, masm->isolate());
    __ Move(cp, context_address);
    __ LoadP(cp, MemOperand(cp));

    // Push the function.
    __ Push(r5);

    // Check if we have enough stack space to push all arguments.
    Label enough_stack_space, stack_overflow;
    __ addi(r3, r7, Operand(1));
    __ StackOverflowCheck(r3, r9, &stack_overflow);
    __ b(&enough_stack_space);
    __ bind(&stack_overflow);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ bkpt(0);

    __ bind(&enough_stack_space);

    // Copy arguments to the stack in a loop.
    // r4: function
    // r7: argc
    // r8: argv, i.e. points to first arg
    Label loop, done;
    __ cmpi(r7, Operand::Zero());
    __ beq(&done);

    __ ShiftLeftImm(r9, r7, Operand(kSystemPointerSizeLog2));
    __ add(r8, r8, r9);  // point to last arg

    __ mtctr(r7);
    __ bind(&loop);
    __ LoadPU(r9, MemOperand(r8, -kSystemPointerSize));  // read next parameter
    __ LoadP(r0, MemOperand(r9));                        // dereference handle
    __ push(r0);                                         // push parameter
    __ bdnz(&loop);
    __ bind(&done);

    // Push the receiver.
    __ Push(r6);

    // r3: argc
    // r4: function
    // r6: new.target
    __ mr(r3, r7);
    __ mr(r6, r4);
    __ mr(r4, r5);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r7, RootIndex::kUndefinedValue);
    __ mr(r8, r7);
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

void Builtins::Generate_RunMicrotasksTrampoline(MacroAssembler* masm) {
  // This expects two C++ function parameters passed by Invoke() in
  // execution.cc.
  //   r3: root_register_value
  //   r4: microtask_queue

  __ mr(RunMicrotasksDescriptor::MicrotaskQueueRegister(), r4);
  __ Jump(BUILTIN_CODE(masm->isolate(), RunMicrotasks), RelocInfo::CODE_TARGET);
}

static void ReplaceClosureCodeWithOptimizedCode(MacroAssembler* masm,
                                                Register optimized_code,
                                                Register closure,
                                                Register scratch1,
                                                Register scratch2) {
  // Store code entry in the closure.
  __ StoreTaggedField(optimized_code,
                      FieldMemOperand(closure, JSFunction::kCodeOffset), r0);
  __ mr(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch1,
                                  Register scratch2) {
  Register params_size = scratch1;
  // Get the size of the formal parameters + receiver (in bytes).
  __ LoadP(params_size,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ lwz(params_size,
         FieldMemOperand(params_size, BytecodeArray::kParameterSizeOffset));

  Register actual_params_size = scratch2;
  // Compute the size of the actual parameters + receiver (in bytes).
  __ LoadP(actual_params_size,
           MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ ShiftLeftImm(actual_params_size, actual_params_size,
                  Operand(kSystemPointerSizeLog2));
  __ addi(actual_params_size, actual_params_size, Operand(kSystemPointerSize));

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label corrected_args_count;
  __ cmp(params_size, actual_params_size);
  __ bge(&corrected_args_count);
  __ mr(params_size, actual_params_size);
  __ bind(&corrected_args_count);
  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  __ add(sp, sp, params_size);
}

// Tail-call |function_id| if |actual_marker| == |expected_marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register actual_marker,
                                          OptimizationMarker expected_marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ cmpi(actual_marker, Operand(expected_marker));
  __ bne(&no_match);
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void TailCallOptimizedCodeSlot(MacroAssembler* masm,
                                      Register optimized_code_entry,
                                      Register scratch) {
  // ----------- S t a t e -------------
  //  -- r3 : actual argument count
  //  -- r6 : new target (preserved for callee if needed, and caller)
  //  -- r4 : target function (preserved for callee if needed, and caller)
  // -----------------------------------
  DCHECK(!AreAliased(r4, r6, optimized_code_entry, scratch));

  Register closure = r4;
  Label heal_optimized_code_slot;

  // If the optimized code is cleared, go to runtime to update the optimization
  // marker field.
  __ LoadWeakValue(optimized_code_entry, optimized_code_entry,
                   &heal_optimized_code_slot);

  // Check if the optimized code is marked for deopt. If it is, call the
  // runtime to clear it.
  __ LoadTaggedPointerField(
      scratch,
      FieldMemOperand(optimized_code_entry, Code::kCodeDataContainerOffset));
  __ LoadWordArith(
      scratch,
      FieldMemOperand(scratch, CodeDataContainer::kKindSpecificFlagsOffset));
  __ TestBit(scratch, Code::kMarkedForDeoptimizationBit, r0);
  __ bne(&heal_optimized_code_slot, cr0);

  // Optimized code is good, get it into the closure and link the closure
  // into the optimized functions list, then tail call the optimized code.
  ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                      scratch, r8);
  static_assert(kJavaScriptCallCodeStartRegister == r5, "ABI mismatch");
  __ LoadCodeObjectEntry(r5, optimized_code_entry);
  __ Jump(r5);

  // Optimized code slot contains deoptimized code or code is cleared and
  // optimized code marker isn't updated. Evict the code, update the marker
  // and re-enter the closure's code.
  __ bind(&heal_optimized_code_slot);
  GenerateTailCallToReturnedCode(masm, Runtime::kHealOptimizedCodeSlot);
}

static void MaybeOptimizeCode(MacroAssembler* masm, Register feedback_vector,
                              Register optimization_marker) {
  // ----------- S t a t e -------------
  //  -- r3 : actual argument count
  //  -- r6 : new target (preserved for callee if needed, and caller)
  //  -- r4 : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  //  -- optimization_marker : a int32 containing a non-zero optimization
  //  marker.
  // -----------------------------------
  DCHECK(!AreAliased(feedback_vector, r4, r6, optimization_marker));

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
                                          Register scratch2, Label* if_return) {
  Register bytecode_size_table = scratch1;
  Register scratch3 = bytecode;

  // The bytecode offset value will be increased by one in wide and extra wide
  // cases. In the case of having a wide or extra wide JumpLoop bytecode, we
  // will restore the original bytecode. In order to simplify the code, we have
  // a backup of it.
  Register original_bytecode_offset = scratch2;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode, original_bytecode_offset));
  __ Move(bytecode_size_table,
          ExternalReference::bytecode_size_table_address());
  __ Move(original_bytecode_offset, bytecode_offset);

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  STATIC_ASSERT(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  STATIC_ASSERT(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ cmpi(bytecode, Operand(0x3));
  __ bgt(&process_bytecode);
  __ andi(r0, bytecode, Operand(0x1));
  __ bne(&extra_wide, cr0);

  // Load the next bytecode and update table to the wide scaled table.
  __ addi(bytecode_offset, bytecode_offset, Operand(1));
  __ lbzx(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ addi(bytecode_size_table, bytecode_size_table,
          Operand(kByteSize * interpreter::Bytecodes::kBytecodeCount));
  __ b(&process_bytecode);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ addi(bytecode_offset, bytecode_offset, Operand(1));
  __ lbzx(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ addi(bytecode_size_table, bytecode_size_table,
          Operand(2 * kByteSize * interpreter::Bytecodes::kBytecodeCount));

  // Load the size of the current bytecode.
  __ bind(&process_bytecode);

  // Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)                                           \
  __ cmpi(bytecode,                                                   \
          Operand(static_cast<int>(interpreter::Bytecode::k##NAME))); \
  __ beq(if_return);
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // If this is a JumpLoop, re-execute it to perform the jump to the beginning
  // of the loop.
  Label end, not_jump_loop;
  __ cmpi(bytecode,
          Operand(static_cast<int>(interpreter::Bytecode::kJumpLoop)));
  __ bne(&not_jump_loop);
  // We need to restore the original bytecode_offset since we might have
  // increased it to skip the wide / extra-wide prefix bytecode.
  __ Move(bytecode_offset, original_bytecode_offset);
  __ b(&end);

  __ bind(&not_jump_loop);
  // Otherwise, load the size of the current bytecode and advance the offset.
  __ lbzx(scratch3, MemOperand(bytecode_size_table, bytecode));
  __ add(bytecode_offset, bytecode_offset, scratch3);

  __ bind(&end);
}

static void MaybeOptimizeCodeOrTailCallOptimizedCodeSlot(
    MacroAssembler* masm, Register optimization_state,
    Register feedback_vector) {
  Label maybe_has_optimized_code;
  // Check if optimized code is available
  __ TestBitMask(optimization_state,
                 FeedbackVector::kHasCompileOptimizedOrLogFirstExecutionMarker,
                 r0);
  __ beq(&maybe_has_optimized_code, cr0);

  Register optimization_marker = optimization_state;
  __ DecodeField<FeedbackVector::OptimizationMarkerBits>(optimization_marker);
  MaybeOptimizeCode(masm, feedback_vector, optimization_marker);

  __ bind(&maybe_has_optimized_code);
  Register optimized_code_entry = optimization_state;
  __ LoadAnyTaggedField(
      optimization_marker,
      FieldMemOperand(feedback_vector,
                      FeedbackVector::kMaybeOptimizedCodeOffset));
  TailCallOptimizedCodeSlot(masm, optimized_code_entry, r9);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.
//
// The live registers are:
//   o r3: actual argument count (not including the receiver)
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
  Register closure = r4;
  Register feedback_vector = r5;

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  __ LoadTaggedPointerField(
      r7, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Load original bytecode array or the debug copy.
  __ LoadTaggedPointerField(
      kInterpreterBytecodeArrayRegister,
      FieldMemOperand(r7, SharedFunctionInfo::kFunctionDataOffset));
  GetSharedFunctionInfoBytecode(masm, kInterpreterBytecodeArrayRegister, ip);

  // The bytecode array could have been flushed from the shared function info,
  // if so, call into CompileLazy.
  Label compile_lazy;
  __ CompareObjectType(kInterpreterBytecodeArrayRegister, r7, no_reg,
                       BYTECODE_ARRAY_TYPE);
  __ bne(&compile_lazy);

  // Load the feedback vector from the closure.
  __ LoadTaggedPointerField(
      feedback_vector,
      FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ LoadTaggedPointerField(
      feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));

  Label push_stack_frame;
  // Check if feedback vector is valid. If valid, check for optimized code
  // and update invocation count. Otherwise, setup the stack frame.
  __ LoadTaggedPointerField(
      r7, FieldMemOperand(feedback_vector, HeapObject::kMapOffset));
  __ LoadHalfWord(r7, FieldMemOperand(r7, Map::kInstanceTypeOffset));
  __ cmpi(r7, Operand(FEEDBACK_VECTOR_TYPE));
  __ bne(&push_stack_frame);

  Register optimization_state = r7;

  // Read off the optimization state in the feedback vector.
  __ LoadWord(optimization_state,
              FieldMemOperand(feedback_vector, FeedbackVector::kFlagsOffset),
              r0);

  // Check if the optimized code slot is not empty or has a optimization marker.
  Label has_optimized_code_or_marker;
  __ TestBitMask(optimization_state,
                 FeedbackVector::kHasOptimizedCodeOrCompileOptimizedMarkerMask,
                 r0);
  __ bne(&has_optimized_code_or_marker, cr0);

  Label not_optimized;
  __ bind(&not_optimized);

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
  __ li(r8, Operand(0));
  __ StoreHalfWord(r8,
                   FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                   BytecodeArray::kOsrNestingLevelOffset),
                   r0);

  // Load initial bytecode offset.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(r7, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, r7);

  // Allocate the local and temporary register file on the stack.
  Label stack_overflow;
  {
    // Load frame size (word) from the BytecodeArray object.
    __ lwz(r5, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                               BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    __ sub(r8, sp, r5);
    __ LoadStackLimit(r0, StackLimitKind::kRealStackLimit);
    __ cmpl(r8, r0);
    __ blt(&stack_overflow);

    // If ok, push undefined as the initial value for all register file entries.
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    Label loop, no_args;
    __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
    __ ShiftRightImm(r5, r5, Operand(kSystemPointerSizeLog2), SetRC);
    __ beq(&no_args, cr0);
    __ mtctr(r5);
    __ bind(&loop);
    __ push(kInterpreterAccumulatorRegister);
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
  __ ShiftLeftImm(r8, r8, Operand(kSystemPointerSizeLog2));
  __ StorePX(r6, MemOperand(fp, r8));
  __ bind(&no_incoming_new_target_or_generator_register);

  // Perform interrupt stack check.
  // TODO(solanes): Merge with the real stack limit check above.
  Label stack_check_interrupt, after_stack_check_interrupt;
  __ LoadStackLimit(r0, StackLimitKind::kInterruptStackLimit);
  __ cmpl(sp, r0);
  __ blt(&stack_check_interrupt);
  __ bind(&after_stack_check_interrupt);

  // The accumulator is already loaded with undefined.

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));
  __ lbzx(r6, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftImm(r6, r6, Operand(kSystemPointerSizeLog2));
  __ LoadPX(kJavaScriptCallCodeStartRegister,
            MemOperand(kInterpreterDispatchTableRegister, r6));
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
  __ lbzx(r4, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, r4, r5, r6,
                                &do_return);
  __ b(&do_dispatch);

  __ bind(&do_return);
  // The return value is in r3.
  LeaveInterpreterFrame(masm, r5, r7);
  __ blr();

  __ bind(&stack_check_interrupt);
  // Modify the bytecode offset in the stack to be kFunctionEntryBytecodeOffset
  // for the call to the StackGuard.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(Smi::FromInt(BytecodeArray::kHeaderSize - kHeapObjectTag +
                              kFunctionEntryBytecodeOffset)));
  __ StoreP(kInterpreterBytecodeOffsetRegister,
            MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ CallRuntime(Runtime::kStackGuard);

  // After the call, restore the bytecode array, bytecode offset and accumulator
  // registers again. Also, restore the bytecode offset in the stack to its
  // previous value.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);

  __ SmiTag(r0, kInterpreterBytecodeOffsetRegister);
  __ StoreP(r0,
            MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  __ jmp(&after_stack_check_interrupt);

  __ bind(&has_optimized_code_or_marker);
  MaybeOptimizeCodeOrTailCallOptimizedCodeSlot(masm, optimization_state,
                                               feedback_vector);

  __ bind(&compile_lazy);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);

  __ bind(&stack_overflow);
  __ CallRuntime(Runtime::kThrowStackOverflow);
  __ bkpt(0);  // Should not return.
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args,
                                         Register start_address,
                                         Register scratch) {
  __ subi(scratch, num_args, Operand(1));
  __ ShiftLeftImm(scratch, scratch, Operand(kSystemPointerSizeLog2));
  __ sub(start_address, start_address, scratch);
  // Push the arguments.
  __ PushArray(start_address, num_args, scratch, r0,
               TurboAssembler::PushArrayOrder::kReverse);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r5 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- r4 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // The spread argument should not be pushed.
    __ subi(r3, r3, Operand(1));
  }

  // Calculate number of arguments (add one for receiver).
  __ addi(r6, r3, Operand(1));
  __ StackOverflowCheck(r6, ip, &stack_overflow);

  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    // Don't copy receiver. Argument count is correct.
    __ mr(r6, r3);
  }

  // Push the arguments.
  Generate_InterpreterPushArgs(masm, r6, r5, r7);

  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(RootIndex::kUndefinedValue);
  }

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Pass the spread in the register r3.
    // r2 already points to the penultimate argument, the spread
    // lies in the next interpreter register.
    __ LoadP(r5, MemOperand(r5, -kSystemPointerSize));
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
  // -- r3 : argument count (not including receiver)
  // -- r6 : new target
  // -- r4 : constructor to call
  // -- r5 : allocation site feedback if available, undefined otherwise.
  // -- r7 : address of the first argument
  // -----------------------------------
  Label stack_overflow;
  __ addi(r8, r3, Operand(1));
  __ StackOverflowCheck(r8, ip, &stack_overflow);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // The spread argument should not be pushed.
    __ subi(r3, r3, Operand(1));
  }

  // Push the arguments.
  Generate_InterpreterPushArgs(masm, r3, r7, r8);

  // Push a slot for the receiver to be constructed.
  __ li(r0, Operand::Zero());
  __ push(r0);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Pass the spread in the register r2.
    // r4 already points to the penultimate argument, the spread
    // lies in the next interpreter register.
    __ subi(r7, r7, Operand(kSystemPointerSize));
    __ LoadP(r5, MemOperand(r7));
  } else {
    __ AssertUndefinedOrAllocationSite(r5, r8);
  }

  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    __ AssertFunction(r4);

    // Tail call to the array construct stub (still in the caller
    // context at this point).
    Handle<Code> code = BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl);
    __ Jump(code, RelocInfo::CODE_TARGET);
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
  Label builtin_trampoline, trampoline_loaded;
  Smi interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::zero());

  // If the SFI function_data is an InterpreterData, the function will have a
  // custom copy of the interpreter entry trampoline for profiling. If so,
  // get the custom trampoline, otherwise grab the entry address of the global
  // trampoline.
  __ LoadP(r5, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      r5, FieldMemOperand(r5, JSFunction::kSharedFunctionInfoOffset));
  __ LoadTaggedPointerField(
      r5, FieldMemOperand(r5, SharedFunctionInfo::kFunctionDataOffset));
  __ CompareObjectType(r5, kInterpreterDispatchTableRegister,
                       kInterpreterDispatchTableRegister,
                       INTERPRETER_DATA_TYPE);
  __ bne(&builtin_trampoline);

  __ LoadTaggedPointerField(
      r5, FieldMemOperand(r5, InterpreterData::kInterpreterTrampolineOffset));
  __ addi(r5, r5, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ b(&trampoline_loaded);

  __ bind(&builtin_trampoline);
  __ Move(r5, ExternalReference::
                  address_of_interpreter_entry_trampoline_instruction_start(
                      masm->isolate()));
  __ LoadP(r5, MemOperand(r5));

  __ bind(&trampoline_loaded);
  __ addi(r0, r5, Operand(interpreter_entry_return_pc_offset.value()));
  __ mtlr(r0);

  // Initialize the dispatch table register.
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

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

  if (FLAG_debug_code) {
    Label okay;
    __ cmpi(kInterpreterBytecodeOffsetRegister,
            Operand(BytecodeArray::kHeaderSize - kHeapObjectTag +
                    kFunctionEntryBytecodeOffset));
    __ bge(&okay);
    __ bkpt(0);
    __ bind(&okay);
  }

  // Dispatch to the target bytecode.
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ lbzx(ip, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftImm(scratch, scratch, Operand(kSystemPointerSizeLog2));
  __ LoadPX(kJavaScriptCallCodeStartRegister,
            MemOperand(kInterpreterDispatchTableRegister, scratch));
  __ Jump(kJavaScriptCallCodeStartRegister);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Get bytecode array and bytecode offset from the stack frame.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadP(kInterpreterBytecodeOffsetRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  Label enter_bytecode, function_entry_bytecode;
  __ cmpi(kInterpreterBytecodeOffsetRegister,
          Operand(BytecodeArray::kHeaderSize - kHeapObjectTag +
                  kFunctionEntryBytecodeOffset));
  __ beq(&function_entry_bytecode);

  // Load the current bytecode.
  __ lbzx(r4, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));

  // Advance to the next bytecode.
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, r4, r5, r6,
                                &if_return);

  __ bind(&enter_bytecode);
  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(r5, kInterpreterBytecodeOffsetRegister);
  __ StoreP(r5,
            MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);

  __ bind(&function_entry_bytecode);
  // If the code deoptimizes during the implicit function entry stack interrupt
  // check, it will have a bailout ID of kFunctionEntryBytecodeOffset, which is
  // not a valid bytecode offset. Detect this case and advance to the first
  // actual bytecode.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ b(&enter_bytecode);

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
  Register scratch = ip;
  if (with_result) {
    if (java_script_builtin) {
      __ mr(scratch, r3);
    } else {
      // Overwrite the hole inserted by the deoptimizer with the return value
      // from the LAZY deopt point.
      __ StoreP(
          r3, MemOperand(
                  sp, config->num_allocatable_general_registers() *
                              kSystemPointerSize +
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
  if (java_script_builtin && with_result) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point. r0 contains the arguments count, the return value
    // from LAZY is always the last argument.
    __ addi(r3, r3,
            Operand(BuiltinContinuationFrameConstants::kFixedSlotCount));
    __ ShiftLeftImm(r0, r3, Operand(kSystemPointerSizeLog2));
    __ StorePX(scratch, MemOperand(sp, r0));
    // Recover arguments count.
    __ subi(r3, r3,
            Operand(BuiltinContinuationFrameConstants::kFixedSlotCount));
  }
  __ LoadP(
      fp,
      MemOperand(sp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  // Load builtin index (stored as a Smi) and use it to get the builtin start
  // address from the builtins table.
  UseScratchRegisterScope temps(masm);
  Register builtin = temps.Acquire();
  __ Pop(builtin);
  __ addi(sp, sp,
          Operand(BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(r0);
  __ mtlr(r0);
  __ LoadEntryFromBuiltinIndex(builtin);
  __ Jump(builtin);
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
  __ LoadP(r3, MemOperand(sp, 0 * kSystemPointerSize));
  __ addi(sp, sp, Operand(1 * kSystemPointerSize));
  __ Ret();
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  Label skip;
  __ CmpSmiLiteral(r3, Smi::zero(), r0);
  __ bne(&skip);
  __ Ret();

  __ bind(&skip);

  // Drop the handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  __ LeaveFrame(StackFrame::STUB);

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ LoadTaggedPointerField(
      r4, FieldMemOperand(r3, Code::kDeoptimizationDataOffset));

  {
    ConstantPoolUnavailableScope constant_pool_unavailable(masm);
    __ addi(r3, r3, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start

    if (FLAG_enable_embedded_constant_pool) {
      __ LoadConstantPoolPointerRegisterFromCodeTargetAddress(r3);
    }

    // Load the OSR entrypoint offset from the deoptimization data.
    // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
    __ SmiUntagField(
        r4, FieldMemOperand(r4, FixedArray::OffsetOfElementAt(
                                    DeoptimizationData::kOsrPcOffsetIndex)));

    // Compute the target address = code start + osr_offset
    __ add(r0, r3, r4);

    // And "return" to the OSR entry point of the function.
    __ mtlr(r0);
    __ blr();
  }
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3    : argc
  //  -- sp[0] : receiver
  //  -- sp[4] : thisArg
  //  -- sp[8] : argArray
  // -----------------------------------

  // 1. Load receiver into r4, argArray into r5 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    __ LoadRoot(r8, RootIndex::kUndefinedValue);
    __ mr(r5, r8);

    Label done;
    __ LoadP(r4, MemOperand(sp));  // receiver
    __ cmpi(r3, Operand(1));
    __ blt(&done);
    __ LoadP(r8, MemOperand(sp, kSystemPointerSize));  // thisArg
    __ cmpi(r3, Operand(2));
    __ blt(&done);
    __ LoadP(r5, MemOperand(sp, 2 * kSystemPointerSize));  // argArray

    __ bind(&done);
    __ ShiftLeftImm(ip, r3, Operand(kSystemPointerSizeLog2));
    __ add(sp, sp, ip);
    __ StoreP(r8, MemOperand(sp));
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
  __ JumpIfRoot(r5, RootIndex::kNullValue, &no_arguments);
  __ JumpIfRoot(r5, RootIndex::kUndefinedValue, &no_arguments);

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
  // 1. Get the callable to call (passed as receiver) from the stack.
  __ Pop(r4);

  // 2. Make sure we have at least one argument.
  // r3: actual number of arguments
  {
    Label done;
    __ cmpi(r3, Operand::Zero());
    __ bne(&done);
    __ PushRoot(RootIndex::kUndefinedValue);
    __ addi(r3, r3, Operand(1));
    __ bind(&done);
  }

  // 3. Adjust the actual number of arguments.
  __ subi(r3, r3, Operand(1));

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3     : argc
  //  -- sp[0]  : receiver
  //  -- sp[4]  : target         (if argc >= 1)
  //  -- sp[8]  : thisArgument   (if argc >= 2)
  //  -- sp[12] : argumentsList  (if argc == 3)
  // -----------------------------------

  // 1. Load target into r4 (if present), argumentsList into r5 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    __ LoadRoot(r4, RootIndex::kUndefinedValue);
    __ mr(r8, r4);
    __ mr(r5, r4);

    Label done;
    __ cmpi(r3, Operand(1));
    __ blt(&done);
    __ LoadP(r4, MemOperand(sp, kSystemPointerSize));  // thisArg
    __ cmpi(r3, Operand(2));
    __ blt(&done);
    __ LoadP(r8, MemOperand(sp, 2 * kSystemPointerSize));  // argArray
    __ cmpi(r3, Operand(3));
    __ blt(&done);
    __ LoadP(r5, MemOperand(sp, 3 * kSystemPointerSize));  // argArray

    __ bind(&done);
    __ ShiftLeftImm(ip, r3, Operand(kSystemPointerSizeLog2));
    __ add(sp, sp, ip);
    __ StoreP(r8, MemOperand(sp));
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
  //  -- sp[0]  : receiver
  //  -- sp[4]  : target
  //  -- sp[8]  : argumentsList
  //  -- sp[12] : new.target (optional)
  // -----------------------------------

  // 1. Load target into r4 (if present), argumentsList into r5 (if present),
  // new.target into r6 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    __ LoadRoot(r4, RootIndex::kUndefinedValue);
    __ mr(r5, r4);

    Label done;
    __ mr(r7, r4);
    __ cmpi(r3, Operand(1));
    __ blt(&done);
    __ LoadP(r4, MemOperand(sp, kSystemPointerSize));  // thisArg
    __ mr(r6, r4);
    __ cmpi(r3, Operand(2));
    __ blt(&done);
    __ LoadP(r5, MemOperand(sp, 2 * kSystemPointerSize));  // argArray
    __ cmpi(r3, Operand(3));
    __ blt(&done);
    __ LoadP(r6, MemOperand(sp, 3 * kSystemPointerSize));  // argArray
    __ bind(&done);
    __ ShiftLeftImm(r0, r3, Operand(kSystemPointerSizeLog2));
    __ add(sp, sp, r0);
    __ StoreP(r7, MemOperand(sp));
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

  Register scratch = ip;

  if (masm->emit_debug_code()) {
    // Allow r5 to be a FixedArray, or a FixedDoubleArray if r7 == 0.
    Label ok, fail;
    __ AssertNotSmi(r5);
    __ LoadTaggedPointerField(scratch,
                              FieldMemOperand(r5, HeapObject::kMapOffset));
    __ LoadHalfWord(scratch,
                    FieldMemOperand(scratch, Map::kInstanceTypeOffset));
    __ cmpi(scratch, Operand(FIXED_ARRAY_TYPE));
    __ beq(&ok);
    __ cmpi(scratch, Operand(FIXED_DOUBLE_ARRAY_TYPE));
    __ bne(&fail);
    __ cmpi(r7, Operand::Zero());
    __ beq(&ok);
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  // Check for stack overflow.
  Label stack_overflow;
  __ StackOverflowCheck(r7, scratch, &stack_overflow);

  // Move the arguments already in the stack,
  // including the receiver and the return address.
  {
    Label copy;
    Register src = r9, dest = r8;
    __ addi(src, sp, Operand(-kSystemPointerSize));
    __ ShiftLeftImm(r0, r7, Operand(kSystemPointerSizeLog2));
    __ sub(sp, sp, r0);
    // Update stack pointer.
    __ addi(dest, sp, Operand(-kSystemPointerSize));
    __ addi(r0, r3, Operand(1));
    __ mtctr(r0);

    __ bind(&copy);
    __ LoadPU(r0, MemOperand(src, kSystemPointerSize));
    __ StorePU(r0, MemOperand(dest, kSystemPointerSize));
    __ bdnz(&copy);
  }

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    Label loop, no_args, skip;
    __ cmpi(r7, Operand::Zero());
    __ beq(&no_args);
    __ addi(r5, r5,
            Operand(FixedArray::kHeaderSize - kHeapObjectTag - kTaggedSize));
    __ mtctr(r7);
    __ bind(&loop);
    __ LoadTaggedPointerField(scratch, MemOperand(r5, kTaggedSize));
    __ addi(r5, r5, Operand(kTaggedSize));
    __ CompareRoot(scratch, RootIndex::kTheHoleValue);
    __ bne(&skip);
    __ LoadRoot(scratch, RootIndex::kUndefinedValue);
    __ bind(&skip);
    __ StorePU(scratch, MemOperand(r8, kSystemPointerSize));
    __ bdnz(&loop);
    __ bind(&no_args);
    __ add(r3, r3, r7);
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
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r6 : the new.target (for [[Construct]] calls)
  //  -- r4 : the target to call (can be any Object)
  //  -- r5 : start index (to support rest parameters)
  // -----------------------------------

  Register scratch = r9;

  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(r6, &new_target_not_constructor);
    __ LoadTaggedPointerField(scratch,
                              FieldMemOperand(r6, HeapObject::kMapOffset));
    __ lbz(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ TestBit(scratch, Map::Bits1::IsConstructorBit::kShift, r0);
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

  Label stack_done, stack_overflow;
  __ LoadP(r8, MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ sub(r8, r8, r5, LeaveOE, SetRC);
  __ ble(&stack_done, cr0);
  {
    // ----------- S t a t e -------------
    //  -- r3 : the number of arguments already in the stack (not including the
    //  receiver)
    //  -- r4 : the target to call (can be any Object)
    //  -- r5 : start index (to support rest parameters)
    //  -- r6 : the new.target (for [[Construct]] calls)
    //  -- fp : point to the caller stack frame
    //  -- r8 : number of arguments to copy, i.e. arguments count - start index
    // -----------------------------------

    // Check for stack overflow.
    __ StackOverflowCheck(r8, scratch, &stack_overflow);

    // Forward the arguments from the caller frame.
    // Point to the first argument to copy (skipping the receiver).
    __ addi(r7, fp,
            Operand(CommonFrameConstants::kFixedFrameSizeAboveFp +
                    kSystemPointerSize));
    __ ShiftLeftImm(scratch, r5, Operand(kSystemPointerSizeLog2));
    __ add(r7, r7, scratch);

    // Move the arguments already in the stack,
    // including the receiver and the return address.
    {
      Label copy;
      Register src = ip, dest = r5;  // r7 and r10 are context and root.
      __ addi(src, sp, Operand(-kSystemPointerSize));
      // Update stack pointer.
      __ ShiftLeftImm(scratch, r8, Operand(kSystemPointerSizeLog2));
      __ sub(sp, sp, scratch);
      __ addi(dest, sp, Operand(-kSystemPointerSize));
      __ addi(r0, r3, Operand(1));
      __ mtctr(r0);

      __ bind(&copy);
      __ LoadPU(r0, MemOperand(src, kSystemPointerSize));
      __ StorePU(r0, MemOperand(dest, kSystemPointerSize));
      __ bdnz(&copy);
    }
    // Copy arguments from the caller frame.
    // TODO(victorgomes): Consider using forward order as potentially more cache
    // friendly.
    {
      Label loop;
      __ add(r3, r3, r8);
      __ addi(r5, r5, Operand(kSystemPointerSize));
      __ bind(&loop);
      {
        __ subi(r8, r8, Operand(1));
        __ ShiftLeftImm(scratch, r8, Operand(kSystemPointerSizeLog2));
        __ LoadPX(r0, MemOperand(r7, scratch));
        __ StorePX(r0, MemOperand(r5, scratch));
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
  __ LoadTaggedPointerField(
      r5, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ lwz(r6, FieldMemOperand(r5, SharedFunctionInfo::kFlagsOffset));
  __ TestBitMask(r6, SharedFunctionInfo::IsClassConstructorBit::kMask, r0);
  __ bne(&class_constructor, cr0);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ LoadTaggedPointerField(cp,
                            FieldMemOperand(r4, JSFunction::kContextOffset));
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
      __ LoadReceiver(r6, r3);
      __ JumpIfSmi(r6, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CompareObjectType(r6, r7, r7, FIRST_JS_RECEIVER_TYPE);
      __ bge(&done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(r6, RootIndex::kUndefinedValue, &convert_global_proxy);
        __ JumpIfNotRoot(r6, RootIndex::kNullValue, &convert_to_object);
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
      __ LoadTaggedPointerField(
          r5, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ StoreReceiver(r6, r3, r7);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSFunction)
  //  -- r5 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ LoadHalfWord(
      r5, FieldMemOperand(r5, SharedFunctionInfo::kFormalParameterCountOffset));
  __ InvokeFunctionCode(r4, no_reg, r5, r3, JUMP_FUNCTION);

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
  __ LoadTaggedPointerField(
      r5, FieldMemOperand(r4, JSBoundFunction::kBoundArgumentsOffset));
  __ SmiUntagField(r7, FieldMemOperand(r5, FixedArray::kLengthOffset), SetRC);
  __ beq(&no_bound_arguments, cr0);
  {
    // ----------- S t a t e -------------
    //  -- r3 : the number of arguments (not including the receiver)
    //  -- r4 : target (checked to be a JSBoundFunction)
    //  -- r5 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- r6 : new.target (only in case of [[Construct]])
    //  -- r7 : the number of [[BoundArguments]]
    // -----------------------------------

    Register scratch = r9;
    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ ShiftLeftImm(r10, r7, Operand(kSystemPointerSizeLog2));
      __ sub(r0, sp, r10);
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      {
        __ LoadStackLimit(scratch, StackLimitKind::kRealStackLimit);
        __ cmpl(r0, scratch);
      }
      __ bgt(&done);  // Signed comparison.
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Pop receiver.
    __ Pop(r8);

    // Push [[BoundArguments]].
    {
      Label loop, done;
      __ add(r3, r3, r7);  // Adjust effective number of arguments.
      __ addi(r5, r5, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
      __ mtctr(r7);

      __ bind(&loop);
      __ subi(r7, r7, Operand(1));
      __ ShiftLeftImm(scratch, r7, Operand(kTaggedSizeLog2));
      __ add(scratch, scratch, r5);
      __ LoadAnyTaggedField(scratch, MemOperand(scratch));
      __ Push(scratch);
      __ bdnz(&loop);
      __ bind(&done);
    }

    // Push receiver.
    __ Push(r8);
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
  __ LoadAnyTaggedField(r6,
                        FieldMemOperand(r4, JSBoundFunction::kBoundThisOffset));
  __ StoreReceiver(r6, r3, ip);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ LoadTaggedPointerField(
      r4, FieldMemOperand(r4, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_smi;
  __ JumpIfSmi(r4, &non_callable);
  __ bind(&non_smi);
  __ LoadMap(r7, r4);
  __ CompareInstanceTypeRange(r7, r8, FIRST_JS_FUNCTION_TYPE,
                              LAST_JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, le);
  __ cmpi(r8, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  __ lbz(r7, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r7, Map::Bits1::IsCallableBit::kShift, r0);
  __ beq(&non_callable, cr0);

  // Check if target is a proxy and call CallProxy external builtin
  __ cmpi(r8, Operand(JS_PROXY_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET, eq);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  // Overwrite the original receiver the (original) target.
  __ StoreReceiver(r4, r3, r8);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(r4, Context::CALL_AS_FUNCTION_DELEGATE_INDEX);
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
  __ AssertConstructor(r4);
  __ AssertFunction(r4);

  // Calling convention for function specific ConstructStubs require
  // r5 to contain either an AllocationSite or undefined.
  __ LoadRoot(r5, RootIndex::kUndefinedValue);

  Label call_generic_stub;

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  __ LoadTaggedPointerField(
      r7, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ lwz(r7, FieldMemOperand(r7, SharedFunctionInfo::kFlagsOffset));
  __ mov(ip, Operand(SharedFunctionInfo::ConstructAsBuiltinBit::kMask));
  __ and_(r7, r7, ip, SetRC);
  __ beq(&call_generic_stub, cr0);

  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET);

  __ bind(&call_generic_stub);
  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSBoundFunction)
  //  -- r6 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(r4);
  __ AssertBoundFunction(r4);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  Label skip;
  __ CompareTagged(r4, r6);
  __ bne(&skip);
  __ LoadTaggedPointerField(
      r6, FieldMemOperand(r4, JSBoundFunction::kBoundTargetFunctionOffset));
  __ bind(&skip);

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ LoadTaggedPointerField(
      r4, FieldMemOperand(r4, JSBoundFunction::kBoundTargetFunctionOffset));
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

  // Check if target has a [[Construct]] internal method.
  __ LoadTaggedPointerField(r7, FieldMemOperand(r4, HeapObject::kMapOffset));
  __ lbz(r5, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r5, Map::Bits1::IsConstructorBit::kShift, r0);
  __ beq(&non_constructor, cr0);

  // Dispatch based on instance type.
  __ CompareInstanceTypeRange(r7, r8, FIRST_JS_FUNCTION_TYPE,
                              LAST_JS_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, le);

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
    __ StoreReceiver(r4, r3, r8);
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(r4, Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructedNonConstructable),
          RelocInfo::CODE_TARGET);
}

#if V8_ENABLE_WEBASSEMBLY
void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was put in a register by the jump table trampoline.
  // Convert to Smi for the runtime call.
  __ SmiTag(kWasmCompileLazyFuncIndexRegister,
            kWasmCompileLazyFuncIndexRegister);
  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameAndConstantPoolScope scope(masm, StackFrame::WASM_COMPILE_LAZY);

    // Save all parameter registers (see wasm-linkage.h). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    RegList gp_regs = 0;
    for (Register gp_param_reg : wasm::kGpParamRegisters) {
      gp_regs |= gp_param_reg.bit();
    }

    RegList fp_regs = 0;
    for (DoubleRegister fp_param_reg : wasm::kFpParamRegisters) {
      fp_regs |= fp_param_reg.bit();
    }

    // List must match register numbers under kFpParamRegisters.
    constexpr RegList simd_regs =
        Simd128Register::ListOf(v1, v2, v3, v4, v5, v6, v7, v8);

    CHECK_EQ(NumRegs(gp_regs), arraysize(wasm::kGpParamRegisters));
    CHECK_EQ(NumRegs(fp_regs), arraysize(wasm::kFpParamRegisters));
    CHECK_EQ(NumRegs(simd_regs), arraysize(wasm::kFpParamRegisters));
    CHECK_EQ(WasmCompileLazyFrameConstants::kNumberOfSavedGpParamRegs,
             NumRegs(gp_regs));
    CHECK_EQ(WasmCompileLazyFrameConstants::kNumberOfSavedFpParamRegs,
             NumRegs(fp_regs));
    CHECK_EQ(WasmCompileLazyFrameConstants::kNumberOfSavedFpParamRegs,
             NumRegs(simd_regs));

    __ MultiPush(gp_regs);
    __ MultiPushDoubles(fp_regs);
    // V8 uses the same set of fp param registers as Simd param registers.
    // As these registers are two different sets on ppc we must make
    // sure to also save them when Simd is enabled.
    // Check the comments under crrev.com/c/2645694 for more details.
    Label push_empty_simd, simd_pushed;
    __ Move(ip, ExternalReference::supports_wasm_simd_128_address());
    __ LoadByte(ip, MemOperand(ip), r0);
    __ cmpi(ip, Operand::Zero());  // If > 0 then simd is available.
    __ ble(&push_empty_simd);
    __ MultiPushV128(simd_regs);
    __ b(&simd_pushed);
    __ bind(&push_empty_simd);
    // kFixedFrameSizeFromFp is hard coded to include space for Simd
    // registers, so we still need to allocate space on the stack even if we
    // are not pushing them.
    __ addi(
        sp, sp,
        Operand(-static_cast<int8_t>(base::bits::CountPopulation(simd_regs)) *
                kSimd128Size));
    __ bind(&simd_pushed);

    // Pass instance and function index as explicit arguments to the runtime
    // function.
    __ Push(kWasmInstanceRegister, kWasmCompileLazyFuncIndexRegister);
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ LoadSmiLiteral(cp, Smi::zero());
    __ CallRuntime(Runtime::kWasmCompileLazy, 2);
    // The entrypoint address is the return value.
    __ mr(r11, kReturnRegister0);

    // Restore registers.
    Label pop_empty_simd, simd_popped;
    __ Move(ip, ExternalReference::supports_wasm_simd_128_address());
    __ LoadByte(ip, MemOperand(ip), r0);
    __ cmpi(ip, Operand::Zero());  // If > 0 then simd is available.
    __ ble(&pop_empty_simd);
    __ MultiPopV128(simd_regs);
    __ b(&simd_popped);
    __ bind(&pop_empty_simd);
    __ addi(
        sp, sp,
        Operand(static_cast<int8_t>(base::bits::CountPopulation(simd_regs)) *
                kSimd128Size));
    __ bind(&simd_popped);
    __ MultiPopDoubles(fp_regs);
    __ MultiPop(gp_regs);
  }
  // Finally, jump to the entrypoint.
  __ Jump(r11);
}

void Builtins::Generate_WasmDebugBreak(MacroAssembler* masm) {
  HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::WASM_DEBUG_BREAK);

    // Save all parameter registers. They might hold live values, we restore
    // them after the runtime call.
    __ MultiPush(WasmDebugBreakFrameConstants::kPushedGpRegs);
    __ MultiPushDoubles(WasmDebugBreakFrameConstants::kPushedFpRegs);

    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ LoadSmiLiteral(cp, Smi::zero());
    __ CallRuntime(Runtime::kWasmDebugBreak, 0);

    // Restore registers.
    __ MultiPopDoubles(WasmDebugBreakFrameConstants::kPushedFpRegs);
    __ MultiPop(WasmDebugBreakFrameConstants::kPushedGpRegs);
  }
  __ Ret();
}

void Builtins::Generate_GenericJSToWasmWrapper(MacroAssembler* masm) {
  // TODO(v8:10701): Implement for this platform.
  __ Trap();
}
#endif  // V8_ENABLE_WEBASSEMBLY

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               SaveFPRegsMode save_doubles, ArgvMode argv_mode,
                               bool builtin_exit_frame) {
  // Called from JavaScript; parameters are on stack as if calling JS function.
  // r3: number of arguments including receiver
  // r4: pointer to builtin function
  // fp: frame pointer  (restored after C call)
  // sp: stack pointer  (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)
  //
  // If argv_mode == kArgvInRegister:
  // r5: pointer to the first argument

  __ mr(r15, r4);

  if (argv_mode == kArgvInRegister) {
    // Move argv into the correct register.
    __ mr(r4, r5);
  } else {
    // Compute the argv pointer.
    __ ShiftLeftImm(r4, r3, Operand(kSystemPointerSizeLog2));
    __ add(r4, r4, sp);
    __ subi(r4, r4, Operand(kSystemPointerSize));
  }

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);

  // Need at least one extra slot for return address location.
  int arg_stack_space = 1;

  // Pass buffer for return value on stack if necessary
  bool needs_return_buffer =
      (result_size == 2 && !ABI_RETURNS_OBJECT_PAIRS_IN_REGS);
  if (needs_return_buffer) {
    arg_stack_space += result_size;
  }

  __ EnterExitFrame(
      save_doubles, arg_stack_space,
      builtin_exit_frame ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);

  // Store a copy of argc in callee-saved registers for later.
  __ mr(r14, r3);

  // r3, r14: number of arguments including receiver  (C callee-saved)
  // r4: pointer to the first argument
  // r15: pointer to builtin function  (C callee-saved)

  // Result returned in registers or stack, depending on result size and ABI.

  Register isolate_reg = r5;
  if (needs_return_buffer) {
    // The return value is a non-scalar value.
    // Use frame storage reserved by calling function to pass return
    // buffer as implicit first argument.
    __ mr(r5, r4);
    __ mr(r4, r3);
    __ addi(r3, sp,
            Operand((kStackFrameExtraParamSlot + 1) * kSystemPointerSize));
    isolate_reg = r6;
  }

  // Call C built-in.
  __ Move(isolate_reg, ExternalReference::isolate_address(masm->isolate()));

  Register target = r15;
  __ StoreReturnAddressAndCall(target);

  // If return value is on the stack, pop it to registers.
  if (needs_return_buffer) {
    __ LoadP(r4, MemOperand(r3, kSystemPointerSize));
    __ LoadP(r3, MemOperand(r3));
  }

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(r3, RootIndex::kException);
  __ beq(&exception_returned);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    ExternalReference pending_exception_address = ExternalReference::Create(
        IsolateAddressId::kPendingExceptionAddress, masm->isolate());

    __ Move(r6, pending_exception_address);
    __ LoadP(r6, MemOperand(r6));
    __ CompareRoot(r6, RootIndex::kTheHoleValue);
    // Cannot use check here as it attempts to generate call into runtime.
    __ beq(&okay);
    __ stop();
    __ bind(&okay);
  }

  // Exit C frame and return.
  // r3:r4: result
  // sp: stack pointer
  // fp: frame pointer
  Register argc = argv_mode == kArgvInRegister
                      // We don't want to pop arguments so set argc to no_reg.
                      ? no_reg
                      // r14: still holds argc (callee-saved).
                      : r14;
  __ LeaveExitFrame(save_doubles, argc);
  __ blr();

  // Handling of exception.
  __ bind(&exception_returned);

  ExternalReference pending_handler_context_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerContextAddress, masm->isolate());
  ExternalReference pending_handler_entrypoint_address =
      ExternalReference::Create(
          IsolateAddressId::kPendingHandlerEntrypointAddress, masm->isolate());
  ExternalReference pending_handler_constant_pool_address =
      ExternalReference::Create(
          IsolateAddressId::kPendingHandlerConstantPoolAddress,
          masm->isolate());
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
    __ PrepareCallCFunction(3, 0, r3);
    __ li(r3, Operand::Zero());
    __ li(r4, Operand::Zero());
    __ Move(r5, ExternalReference::isolate_address(masm->isolate()));
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
  __ cmpi(cp, Operand::Zero());
  __ beq(&skip);
  __ StoreP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ bind(&skip);

  // Reset the masking register. This is done independent of the underlying
  // feature flag {FLAG_untrusted_code_mitigations} to make the snapshot work
  // with both configurations. It is safe to always do this, because the
  // underlying register is caller-saved and can be arbitrarily clobbered.
  __ ResetSpeculationPoisonRegister();

  // Clear c_entry_fp, like we do in `LeaveExitFrame`.
  {
    UseScratchRegisterScope temps(masm);
    __ Move(ip, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                          masm->isolate()));
    __ mov(r0, Operand::Zero());
    __ StoreP(r0, MemOperand(ip));
  }

  // Compute the handler entry address and jump to it.
  ConstantPoolUnavailableScope constant_pool_unavailable(masm);
  __ Move(ip, pending_handler_entrypoint_address);
  __ LoadP(ip, MemOperand(ip));
  if (FLAG_enable_embedded_constant_pool) {
    __ Move(kConstantPoolRegister, pending_handler_constant_pool_address);
    __ LoadP(kConstantPoolRegister, MemOperand(kConstantPoolRegister));
  }
  __ Jump(ip);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label out_of_range, only_low, negate, done, fastpath_done;
  Register result_reg = r3;

  HardAbortScope hard_abort(masm);  // Avoid calls to Abort.

  // Immediate values for this stub fit in instructions, so it's safe to use ip.
  Register scratch = GetRegisterThatIsNotOneOf(result_reg);
  Register scratch_low = GetRegisterThatIsNotOneOf(result_reg, scratch);
  Register scratch_high =
      GetRegisterThatIsNotOneOf(result_reg, scratch, scratch_low);
  DoubleRegister double_scratch = kScratchDoubleReg;

  __ Push(result_reg, scratch);
  // Account for saved regs.
  int argument_offset = 2 * kSystemPointerSize;

  // Load double input.
  __ lfd(double_scratch, MemOperand(sp, argument_offset));

  // Do fast-path convert from double to int.
  __ ConvertDoubleToInt64(double_scratch,
#if !V8_TARGET_ARCH_PPC64
                          scratch,
#endif
                          result_reg, d0);

// Test for overflow
#if V8_TARGET_ARCH_PPC64
  __ TestIfInt32(result_reg, r0);
#else
  __ TestIfInt32(scratch, result_reg, r0);
#endif
  __ beq(&fastpath_done);

  __ Push(scratch_high, scratch_low);
  // Account for saved regs.
  argument_offset += 2 * kSystemPointerSize;

  __ lwz(scratch_high,
         MemOperand(sp, argument_offset + Register::kExponentOffset));
  __ lwz(scratch_low,
         MemOperand(sp, argument_offset + Register::kMantissaOffset));

  __ ExtractBitMask(scratch, scratch_high, HeapNumber::kExponentMask);
  // Load scratch with exponent - 1. This is faster than loading
  // with exponent because Bias + 1 = 1024 which is a *PPC* immediate value.
  STATIC_ASSERT(HeapNumber::kExponentBias + 1 == 1024);
  __ subi(scratch, scratch, Operand(HeapNumber::kExponentBias + 1));
  // If exponent is greater than or equal to 84, the 32 less significant
  // bits are 0s (2^84 = 1, 52 significant bits, 32 uncoded bits),
  // the result is 0.
  // Compare exponent with 84 (compare exponent - 1 with 83).
  __ cmpi(scratch, Operand(83));
  __ bge(&out_of_range);

  // If we reach this code, 31 <= exponent <= 83.
  // So, we don't have to handle cases where 0 <= exponent <= 20 for
  // which we would need to shift right the high part of the mantissa.
  // Scratch contains exponent - 1.
  // Load scratch with 52 - exponent (load with 51 - (exponent - 1)).
  __ subfic(scratch, scratch, Operand(51));
  __ cmpi(scratch, Operand::Zero());
  __ ble(&only_low);
  // 21 <= exponent <= 51, shift scratch_low and scratch_high
  // to generate the result.
  __ srw(scratch_low, scratch_low, scratch);
  // Scratch contains: 52 - exponent.
  // We needs: exponent - 20.
  // So we use: 32 - scratch = 32 - 52 + exponent = exponent - 20.
  __ subfic(scratch, scratch, Operand(32));
  __ ExtractBitMask(result_reg, scratch_high, HeapNumber::kMantissaMask);
  // Set the implicit 1 before the mantissa part in scratch_high.
  STATIC_ASSERT(HeapNumber::kMantissaBitsInTopWord >= 16);
  __ oris(result_reg, result_reg,
          Operand(1 << ((HeapNumber::kMantissaBitsInTopWord)-16)));
  __ slw(r0, result_reg, scratch);
  __ orx(result_reg, scratch_low, r0);
  __ b(&negate);

  __ bind(&out_of_range);
  __ mov(result_reg, Operand::Zero());
  __ b(&done);

  __ bind(&only_low);
  // 52 <= exponent <= 83, shift only scratch_low.
  // On entry, scratch contains: 52 - exponent.
  __ neg(scratch, scratch);
  __ slw(result_reg, scratch_low, scratch);

  __ bind(&negate);
  // If input was positive, scratch_high ASR 31 equals 0 and
  // scratch_high LSR 31 equals zero.
  // New result = (result eor 0) + 0 = result.
  // If the input was negative, we have to negate the result.
  // Input_high ASR 31 equals 0xFFFFFFFF and scratch_high LSR 31 equals 1.
  // New result = (result eor 0xFFFFFFFF) + 1 = 0 - result.
  __ srawi(r0, scratch_high, 31);
#if V8_TARGET_ARCH_PPC64
  __ srdi(r0, r0, Operand(32));
#endif
  __ xor_(result_reg, result_reg, r0);
  __ srwi(r0, scratch_high, Operand(31));
  __ add(result_reg, result_reg, r0);

  __ bind(&done);
  __ Pop(scratch_high, scratch_low);
  // Account for saved regs.
  argument_offset -= 2 * kSystemPointerSize;

  __ bind(&fastpath_done);
  __ StoreP(result_reg, MemOperand(sp, argument_offset));
  __ Pop(result_reg, scratch);

  __ Ret();
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
  DCHECK(function_address == r4 || function_address == r5);
  Register scratch = r6;

  __ Move(scratch, ExternalReference::is_profiling_address(isolate));
  __ lbz(scratch, MemOperand(scratch, 0));
  __ cmpi(scratch, Operand::Zero());

  if (CpuFeatures::IsSupported(ISELECT)) {
    __ Move(scratch, thunk_ref);
    __ isel(eq, scratch, function_address, scratch);
  } else {
    Label profiler_enabled, end_profiler_check;
    __ bne(&profiler_enabled);
    __ Move(scratch, ExternalReference::address_of_runtime_stats_flag());
    __ lwz(scratch, MemOperand(scratch, 0));
    __ cmpi(scratch, Operand::Zero());
    __ bne(&profiler_enabled);
    {
      // Call the api function directly.
      __ mr(scratch, function_address);
      __ b(&end_profiler_check);
    }
    __ bind(&profiler_enabled);
    {
      // Additional parameter is the address of the actual callback.
      __ Move(scratch, thunk_ref);
    }
    __ bind(&end_profiler_check);
  }

  // Allocate HandleScope in callee-save registers.
  // r17 - next_address
  // r14 - next_address->kNextOffset
  // r15 - next_address->kLimitOffset
  // r16 - next_address->kLevelOffset
  __ Move(r17, next_address);
  __ LoadP(r14, MemOperand(r17, kNextOffset));
  __ LoadP(r15, MemOperand(r17, kLimitOffset));
  __ lwz(r16, MemOperand(r17, kLevelOffset));
  __ addi(r16, r16, Operand(1));
  __ stw(r16, MemOperand(r17, kLevelOffset));

  __ StoreReturnAddressAndCall(scratch);

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // load value from ReturnValue
  __ LoadP(r3, return_value_operand);
  __ bind(&return_value_loaded);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ StoreP(r14, MemOperand(r17, kNextOffset));
  if (__ emit_debug_code()) {
    __ lwz(r4, MemOperand(r17, kLevelOffset));
    __ cmp(r4, r16);
    __ Check(eq, AbortReason::kUnexpectedLevelAfterReturnFromApiCall);
  }
  __ subi(r16, r16, Operand(1));
  __ stw(r16, MemOperand(r17, kLevelOffset));
  __ LoadP(r0, MemOperand(r17, kLimitOffset));
  __ cmp(r15, r0);
  __ bne(&delete_allocated_handles);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  // LeaveExitFrame expects unwind space to be in a register.
  if (stack_space_operand != nullptr) {
    __ LoadP(r14, *stack_space_operand);
  } else {
    __ mov(r14, Operand(stack_space));
  }
  __ LeaveExitFrame(false, r14, stack_space_operand != nullptr);

  // Check if the function scheduled an exception.
  __ LoadRoot(r14, RootIndex::kTheHoleValue);
  __ Move(r15, ExternalReference::scheduled_exception_address(isolate));
  __ LoadP(r15, MemOperand(r15));
  __ cmp(r14, r15);
  __ bne(&promote_scheduled_exception);

  __ blr();

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ StoreP(r15, MemOperand(r17, kLimitOffset));
  __ mr(r14, r3);
  __ PrepareCallCFunction(1, r15);
  __ Move(r3, ExternalReference::isolate_address(isolate));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(), 1);
  __ mr(r3, r14);
  __ b(&leave_exit_frame);
}

}  // namespace

void Builtins::Generate_CallApiCallback(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- cp                  : context
  //  -- r4                  : api function address
  //  -- r5                  : arguments count (not including the receiver)
  //  -- r6                  : call data
  //  -- r3                  : holder
  //  -- sp[0]               : receiver
  //  -- sp[8]               : first argument
  //  -- ...
  //  -- sp[(argc) * 8]      : last argument
  // -----------------------------------

  Register api_function_address = r4;
  Register argc = r5;
  Register call_data = r6;
  Register holder = r3;
  Register scratch = r7;
  DCHECK(!AreAliased(api_function_address, argc, call_data, holder, scratch));

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
  //   sp[0 * kSystemPointerSize]: kHolder
  //   sp[1 * kSystemPointerSize]: kIsolate
  //   sp[2 * kSystemPointerSize]: undefined (kReturnValueDefaultValue)
  //   sp[3 * kSystemPointerSize]: undefined (kReturnValue)
  //   sp[4 * kSystemPointerSize]: kData
  //   sp[5 * kSystemPointerSize]: undefined (kNewTarget)

  // Reserve space on the stack.
  __ subi(sp, sp, Operand(FCA::kArgsLength * kSystemPointerSize));

  // kHolder.
  __ StoreP(holder, MemOperand(sp, 0 * kSystemPointerSize));

  // kIsolate.
  __ Move(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ StoreP(scratch, MemOperand(sp, 1 * kSystemPointerSize));

  // kReturnValueDefaultValue and kReturnValue.
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ StoreP(scratch, MemOperand(sp, 2 * kSystemPointerSize));
  __ StoreP(scratch, MemOperand(sp, 3 * kSystemPointerSize));

  // kData.
  __ StoreP(call_data, MemOperand(sp, 4 * kSystemPointerSize));

  // kNewTarget.
  __ StoreP(scratch, MemOperand(sp, 5 * kSystemPointerSize));

  // Keep a pointer to kHolder (= implicit_args) in a scratch register.
  // We use it below to set up the FunctionCallbackInfo object.
  __ mr(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  // PPC LINUX ABI:
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
  __ StoreP(scratch, MemOperand(sp, (kStackFrameExtraParamSlot + 1) *
                                        kSystemPointerSize));

  // FunctionCallbackInfo::values_ (points at the first varargs argument passed
  // on the stack).
  __ addi(scratch, scratch,
          Operand((FCA::kArgsLength + 1) * kSystemPointerSize));
  __ StoreP(scratch, MemOperand(sp, (kStackFrameExtraParamSlot + 2) *
                                        kSystemPointerSize));

  // FunctionCallbackInfo::length_.
  __ stw(argc,
         MemOperand(sp, (kStackFrameExtraParamSlot + 3) * kSystemPointerSize));

  // We also store the number of bytes to drop from the stack after returning
  // from the API function here.
  __ mov(scratch,
         Operand((FCA::kArgsLength + 1 /* receiver */) * kSystemPointerSize));
  __ ShiftLeftImm(ip, argc, Operand(kSystemPointerSizeLog2));
  __ add(scratch, scratch, ip);
  __ StoreP(scratch, MemOperand(sp, (kStackFrameExtraParamSlot + 4) *
                                        kSystemPointerSize));

  // v8::InvocationCallback's argument.
  __ addi(r3, sp,
          Operand((kStackFrameExtraParamSlot + 1) * kSystemPointerSize));

  ExternalReference thunk_ref = ExternalReference::invoke_function_callback();

  // There are two stack slots above the arguments we constructed on the stack.
  // TODO(jgruber): Document what these arguments are.
  static constexpr int kStackSlotsAboveFCA = 2;
  MemOperand return_value_operand(
      fp, (kStackSlotsAboveFCA + FCA::kReturnValueOffset) * kSystemPointerSize);

  static constexpr int kUseStackSpaceOperand = 0;
  MemOperand stack_space_operand(
      sp, (kStackFrameExtraParamSlot + 4) * kSystemPointerSize);

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
  Register scratch = r7;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  Register api_function_address = r5;

  __ push(receiver);
  // Push data from AccessorInfo.
  __ LoadAnyTaggedField(scratch,
                        FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ push(scratch);
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ Push(scratch, scratch);
  __ Move(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ Push(scratch, holder);
  __ Push(Smi::zero());  // should_throw_on_error -> false
  __ LoadTaggedPointerField(
      scratch, FieldMemOperand(callback, AccessorInfo::kNameOffset));
  __ push(scratch);

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ mr(r3, sp);                               // r3 = Handle<Name>
  __ addi(r4, r3, Operand(1 * kSystemPointerSize));  // r4 = v8::PCI::args_

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
    __ StoreP(r3, MemOperand(sp, arg0Slot * kSystemPointerSize));
    __ addi(r3, sp, Operand(arg0Slot * kSystemPointerSize));
  }

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ StoreP(r4, MemOperand(sp, accessorInfoSlot * kSystemPointerSize));
  __ addi(r4, sp, Operand(accessorInfoSlot * kSystemPointerSize));
  // r4 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback();

  __ LoadTaggedPointerField(
      scratch, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ LoadP(api_function_address,
        FieldMemOperand(scratch, Foreign::kForeignAddressOffset));

  // +3 is to skip prolog, return address and name handle.
  MemOperand return_value_operand(
      fp,
      (PropertyCallbackArguments::kReturnValueOffset + 3) * kSystemPointerSize);
  MemOperand* const kUseStackSpaceConstant = nullptr;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kStackUnwindSpace, kUseStackSpaceConstant,
                           return_value_operand);
}

void Builtins::Generate_DirectCEntry(MacroAssembler* masm) {
  UseScratchRegisterScope temps(masm);
  Register temp2 = temps.Acquire();
  // Place the return address on the stack, making the call
  // GC safe. The RegExp backend also relies on this.
  __ mflr(r0);
  __ StoreP(r0, MemOperand(sp, kStackFrameExtraParamSlot * kSystemPointerSize));

  if (ABI_USES_FUNCTION_DESCRIPTORS) {
    // AIX/PPC64BE Linux use a function descriptor;
    __ LoadP(ToRegister(ABI_TOC_REGISTER),
             MemOperand(temp2, kSystemPointerSize));
    __ LoadP(temp2, MemOperand(temp2, 0));  // Instruction address
  }

  __ Call(temp2);  // Call the C++ function.
  __ LoadP(r0, MemOperand(sp, kStackFrameExtraParamSlot * kSystemPointerSize));
  __ mtlr(r0);
  __ blr();
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
  RegList saved_regs = restored_regs | sp.bit();

  const int kDoubleRegsSize = kDoubleSize * DoubleRegister::kNumRegisters;

  // Save all double registers before messing with them.
  __ subi(sp, sp, Operand(kDoubleRegsSize));
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister dreg = DoubleRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ stfd(dreg, MemOperand(sp, offset));
  }

  // Push saved_regs (needed to populate FrameDescription::registers_).
  // Leave gaps for other registers.
  __ subi(sp, sp, Operand(kNumberOfRegisters * kSystemPointerSize));
  for (int16_t i = kNumberOfRegisters - 1; i >= 0; i--) {
    if ((saved_regs & (1 << i)) != 0) {
      __ StoreP(ToRegister(i), MemOperand(sp, kSystemPointerSize * i));
    }
  }
  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ Move(scratch, ExternalReference::Create(
                         IsolateAddressId::kCEntryFPAddress, isolate));
    __ StoreP(fp, MemOperand(scratch));
  }
  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kSystemPointerSize) + kDoubleRegsSize;

  // Get the bailout id is passed as r29 by the caller.
  __ mr(r5, r29);

  __ mov(r5, Operand(Deoptimizer::kFixedExitSizeMarker));
  // Get the address of the location in the code object (r6) (return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register r7.
  __ mflr(r6);
  __ addi(r7, sp, Operand(kSavedRegistersAreaSize));
  __ sub(r7, fp, r7);

  // Allocate a new deoptimizer object.
  // Pass six arguments in r3 to r8.
  __ PrepareCallCFunction(6, r8);
  __ li(r3, Operand::Zero());
  Label context_check;
  __ LoadP(r4, MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(r4, &context_check);
  __ LoadP(r3, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ li(r4, Operand(static_cast<int>(deopt_kind)));
  // r5: bailout id already loaded.
  // r6: code address or 0 already loaded.
  // r7: Fp-to-sp delta.
  __ Move(r8, ExternalReference::isolate_address(isolate));
  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::new_deoptimizer_function(), 6);
  }

  // Preserve "deoptimizer" object in register r3 and get the input
  // frame descriptor pointer to r4 (deoptimizer->input_);
  __ LoadP(r4, MemOperand(r3, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  DCHECK_EQ(Register::kNumRegisters, kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset =
        (i * kSystemPointerSize) + FrameDescription::registers_offset();
    __ LoadP(r5, MemOperand(sp, i * kSystemPointerSize));
    __ StoreP(r5, MemOperand(r4, offset));
  }

  int double_regs_offset = FrameDescription::double_registers_offset();
  // Copy double registers to
  // double_registers_[DoubleRegister::kNumRegisters]
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    int dst_offset = code * kDoubleSize + double_regs_offset;
    int src_offset =
        code * kDoubleSize + kNumberOfRegisters * kSystemPointerSize;
    __ lfd(d0, MemOperand(sp, src_offset));
    __ stfd(d0, MemOperand(r4, dst_offset));
  }

  // Mark the stack as not iterable for the CPU profiler which won't be able to
  // walk the stack without the return address.
  {
    UseScratchRegisterScope temps(masm);
    Register is_iterable = temps.Acquire();
    Register zero = r7;
    __ Move(is_iterable, ExternalReference::stack_is_iterable_address(isolate));
    __ li(zero, Operand(0));
    __ stb(zero, MemOperand(is_iterable));
  }

  // Remove the saved registers from the stack.
  __ addi(sp, sp, Operand(kSavedRegistersAreaSize));

  // Compute a pointer to the unwinding limit in register r5; that is
  // the first stack slot not part of the input frame.
  __ LoadP(r5, MemOperand(r4, FrameDescription::frame_size_offset()));
  __ add(r5, r5, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ addi(r6, r4, Operand(FrameDescription::frame_content_offset()));
  Label pop_loop;
  Label pop_loop_header;
  __ b(&pop_loop_header);
  __ bind(&pop_loop);
  __ pop(r7);
  __ StoreP(r7, MemOperand(r6, 0));
  __ addi(r6, r6, Operand(kSystemPointerSize));
  __ bind(&pop_loop_header);
  __ cmp(r5, sp);
  __ bne(&pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(r3);  // Preserve deoptimizer object across call.
  // r3: deoptimizer object; r4: scratch.
  __ PrepareCallCFunction(1, r4);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
  }
  __ pop(r3);  // Restore deoptimizer object (class Deoptimizer).

  __ LoadP(sp, MemOperand(r3, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop, outer_loop_header, inner_loop_header;
  // Outer loop state: r7 = current "FrameDescription** output_",
  // r4 = one past the last FrameDescription**.
  __ lwz(r4, MemOperand(r3, Deoptimizer::output_count_offset()));
  __ LoadP(r7, MemOperand(r3, Deoptimizer::output_offset()));  // r7 is output_.
  __ ShiftLeftImm(r4, r4, Operand(kSystemPointerSizeLog2));
  __ add(r4, r7, r4);
  __ b(&outer_loop_header);

  __ bind(&outer_push_loop);
  // Inner loop state: r5 = current FrameDescription*, r6 = loop index.
  __ LoadP(r5, MemOperand(r7, 0));  // output_[ix]
  __ LoadP(r6, MemOperand(r5, FrameDescription::frame_size_offset()));
  __ b(&inner_loop_header);

  __ bind(&inner_push_loop);
  __ addi(r6, r6, Operand(-sizeof(intptr_t)));
  __ add(r9, r5, r6);
  __ LoadP(r9, MemOperand(r9, FrameDescription::frame_content_offset()));
  __ push(r9);

  __ bind(&inner_loop_header);
  __ cmpi(r6, Operand::Zero());
  __ bne(&inner_push_loop);  // test for gt?

  __ addi(r7, r7, Operand(kSystemPointerSize));
  __ bind(&outer_loop_header);
  __ cmp(r7, r4);
  __ blt(&outer_push_loop);

  __ LoadP(r4, MemOperand(r3, Deoptimizer::input_offset()));
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister dreg = DoubleRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ lfd(dreg, MemOperand(r4, src_offset));
  }

  // Push pc, and continuation from the last output frame.
  __ LoadP(r9, MemOperand(r5, FrameDescription::pc_offset()));
  __ push(r9);
  __ LoadP(r9, MemOperand(r5, FrameDescription::continuation_offset()));
  __ push(r9);

  // Restore the registers from the last output frame.
  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    DCHECK(!(scratch.bit() & restored_regs));
    __ mr(scratch, r5);
    for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
      int offset =
          (i * kSystemPointerSize) + FrameDescription::registers_offset();
      if ((restored_regs & (1 << i)) != 0) {
        __ LoadP(ToRegister(i), MemOperand(scratch, offset));
      }
    }
  }

  {
    UseScratchRegisterScope temps(masm);
    Register is_iterable = temps.Acquire();
    Register one = r7;
    __ Move(is_iterable, ExternalReference::stack_is_iterable_address(isolate));
    __ li(one, Operand(1));
    __ stb(one, MemOperand(is_iterable));
  }

  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ pop(scratch);  // get continuation, leave pc on stack
    __ pop(r0);
    __ mtlr(r0);
    __ Jump(scratch);
  }

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

void Builtins::Generate_BaselineEnterAtBytecode(MacroAssembler* masm) {
  // Implement on this platform, https://crrev.com/c/2695591.
  __ bkpt(0);
}

void Builtins::Generate_BaselineEnterAtNextBytecode(MacroAssembler* masm) {
  // Implement on this platform, https://crrev.com/c/2695591.
  __ bkpt(0);
}

void Builtins::Generate_InterpreterOnStackReplacement_ToBaseline(
    MacroAssembler* masm) {
  // Implement on this platform, https://crrev.com/c/2800112.
  __ bkpt(0);
}

void Builtins::Generate_DynamicCheckMapsTrampoline(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterFrame(StackFrame::INTERNAL);

  // Only save the registers that the DynamicCheckMaps builtin can clobber.
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
  __ LoadP(handler_arg, MemOperand(fp, CommonFrameConstants::kCallerPCOffset));
  __ LoadP(
      slot_arg,
      MemOperand(handler_arg, Deoptimizer::kEagerWithResumeImmedArgs1PcOffset));
  __ LoadP(
      handler_arg,
      MemOperand(handler_arg, Deoptimizer::kEagerWithResumeImmedArgs2PcOffset));

  __ Call(BUILTIN_CODE(masm->isolate(), DynamicCheckMaps),
          RelocInfo::CODE_TARGET);

  Label deopt, bailout;
  __ cmpi(r3, Operand(static_cast<int>(DynamicCheckMapsStatus::kSuccess)));
  __ bne(&deopt);

  __ RestoreRegisters(registers);
  __ LeaveFrame(StackFrame::INTERNAL);
  __ Ret();

  __ bind(&deopt);
  __ cmpi(r3, Operand(static_cast<int>(DynamicCheckMapsStatus::kBailout)));
  __ beq(&bailout);

  if (FLAG_debug_code) {
    __ cmpi(r3, Operand(static_cast<int>(DynamicCheckMapsStatus::kDeopt)));
    __ Assert(eq, AbortReason::kUnexpectedDynamicCheckMapsStatus);
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

#endif  // V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_PPC64

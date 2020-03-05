// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

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
#include "src/heap/heap-inl.h"
#include "src/objects/cell.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-generator.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/runtime/runtime.h"
#include "src/wasm/wasm-objects.h"

#if defined(V8_OS_WIN)
#include "src/diagnostics/unwinding-info-win64.h"
#endif  // V8_OS_WIN

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address) {
  __ Mov(kJavaScriptCallExtraArg1Register, ExternalReference::Create(address));
  __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
          RelocInfo::CODE_TARGET);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- x1 : target function (preserved for callee)
  //  -- x3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the target function and the new target.
    // Push another copy as a parameter to the runtime call.
    __ Push(x1, x3);
    __ PushArgument(x1);

    __ CallRuntime(function_id, 1);
    __ Mov(x2, x0);

    // Restore target function and new target.
    __ Pop(x3, x1);
  }

  static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
  __ JumpCodeObject(x2);
}

namespace {

void LoadRealStackLimit(MacroAssembler* masm, Register destination) {
  DCHECK(masm->root_array_available());
  Isolate* isolate = masm->isolate();
  ExternalReference limit = ExternalReference::address_of_real_jslimit(isolate);
  DCHECK(TurboAssembler::IsAddressableThroughRootRegister(isolate, limit));

  intptr_t offset =
      TurboAssembler::RootRegisterOffsetForExternalReference(isolate, limit);
  __ Ldr(destination, MemOperand(kRootRegister, offset));
}

void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                 Label* stack_overflow) {
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.AcquireX();

  // Check the stack for overflow.
  // We are not trying to catch interruptions (e.g. debug break and
  // preemption) here, so the "real stack limit" is checked.

  LoadRealStackLimit(masm, scratch);
  // Make scratch the space we have left. The stack might already be overflowed
  // here which will cause scratch to become negative.
  __ Sub(scratch, sp, scratch);
  // Check if the arguments will overflow the stack.
  __ Cmp(scratch, Operand(num_args, LSL, kSystemPointerSizeLog2));
  __ B(le, stack_overflow);
}

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- x1     : constructor function
  //  -- x3     : new target
  //  -- cp     : context
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  ASM_LOCATION("Builtins::Generate_JSConstructStubHelper");
  Label stack_overflow;

  Generate_StackOverflowCheck(masm, x0, &stack_overflow);

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label already_aligned;
    Register argc = x0;

    if (__ emit_debug_code()) {
      // Check that FrameScope pushed the context on to the stack already.
      __ Peek(x2, 0);
      __ Cmp(x2, cp);
      __ Check(eq, AbortReason::kUnexpectedValue);
    }

    // Push number of arguments.
    __ SmiTag(x11, argc);
    __ Push(x11, padreg);

    // Add a slot for the receiver, and round up to maintain alignment.
    Register slot_count = x2;
    Register slot_count_without_rounding = x12;
    __ Add(slot_count_without_rounding, argc, 2);
    __ Bic(slot_count, slot_count_without_rounding, 1);
    __ Claim(slot_count);

    // Preserve the incoming parameters on the stack.
    __ LoadRoot(x10, RootIndex::kTheHoleValue);

    // Compute a pointer to the slot immediately above the location on the
    // stack to which arguments will be later copied.
    __ SlotAddress(x2, argc);

    // Poke the hole (receiver) in the highest slot.
    __ Str(x10, MemOperand(x2));
    __ Tbnz(slot_count_without_rounding, 0, &already_aligned);

    // Store padding, if needed.
    __ Str(padreg, MemOperand(x2, 1 * kSystemPointerSize));
    __ Bind(&already_aligned);

    // Copy arguments to the expression stack.
    {
      Register count = x2;
      Register dst = x10;
      Register src = x11;
      __ Mov(count, argc);
      __ SlotAddress(dst, 0);
      __ Add(src, fp, StandardFrameConstants::kCallerSPOffset);
      __ CopyDoubleWords(dst, src, count);
    }

    // ----------- S t a t e -------------
    //  --                           x0: number of arguments (untagged)
    //  --                           x1: constructor function
    //  --                           x3: new target
    // If argc is odd:
    //  --     sp[0*kSystemPointerSize]: argument n - 1
    //  --             ...
    //  -- sp[(n-1)*kSystemPointerSize]: argument 0
    //  -- sp[(n+0)*kSystemPointerSize]: the hole (receiver)
    //  -- sp[(n+1)*kSystemPointerSize]: padding
    //  -- sp[(n+2)*kSystemPointerSize]: padding
    //  -- sp[(n+3)*kSystemPointerSize]: number of arguments (tagged)
    //  -- sp[(n+4)*kSystemPointerSize]: context (pushed by FrameScope)
    // If argc is even:
    //  --     sp[0*kSystemPointerSize]: argument n - 1
    //  --             ...
    //  -- sp[(n-1)*kSystemPointerSize]: argument 0
    //  -- sp[(n+0)*kSystemPointerSize]: the hole (receiver)
    //  -- sp[(n+1)*kSystemPointerSize]: padding
    //  -- sp[(n+2)*kSystemPointerSize]: number of arguments (tagged)
    //  -- sp[(n+3)*kSystemPointerSize]: context (pushed by FrameScope)
    // -----------------------------------

    // Call the function.
    __ InvokeFunctionWithNewTarget(x1, x3, argc, CALL_FUNCTION);

    // Restore the context from the frame.
    __ Ldr(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame. Use fp relative
    // addressing to avoid the circular dependency between padding existence and
    // argc parity.
    __ SmiUntag(x1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ DropArguments(x1, TurboAssembler::kCountExcludesReceiver);
  __ Ret();

  __ Bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();
  }
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- x1     : constructor function
  //  -- x3     : new target
  //  -- lr     : return address
  //  -- cp     : context pointer
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  ASM_LOCATION("Builtins::Generate_JSConstructStubGeneric");

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    if (__ emit_debug_code()) {
      // Check that FrameScope pushed the context on to the stack already.
      __ Peek(x2, 0);
      __ Cmp(x2, cp);
      __ Check(eq, AbortReason::kUnexpectedValue);
    }

    // Preserve the incoming parameters on the stack.
    __ SmiTag(x0);
    __ Push(x0, x1, padreg, x3);

    // ----------- S t a t e -------------
    //  --        sp[0*kSystemPointerSize]: new target
    //  --        sp[1*kSystemPointerSize]: padding
    //  -- x1 and sp[2*kSystemPointerSize]: constructor function
    //  --        sp[3*kSystemPointerSize]: number of arguments (tagged)
    //  --        sp[4*kSystemPointerSize]: context (pushed by FrameScope)
    // -----------------------------------

    __ LoadTaggedPointerField(
        x4, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(w4, FieldMemOperand(x4, SharedFunctionInfo::kFlagsOffset));
    __ DecodeField<SharedFunctionInfo::FunctionKindBits>(w4);
    __ JumpIfIsInRange(w4, kDefaultDerivedConstructor, kDerivedConstructor,
                       &not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        x4, x5);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ B(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ Bind(&not_create_implicit_receiver);
    __ LoadRoot(x0, RootIndex::kTheHoleValue);

    // ----------- S t a t e -------------
    //  --                                x0: receiver
    //  -- Slot 4 / sp[0*kSystemPointerSize]: new target
    //  -- Slot 3 / sp[1*kSystemPointerSize]: padding
    //  -- Slot 2 / sp[2*kSystemPointerSize]: constructor function
    //  -- Slot 1 / sp[3*kSystemPointerSize]: number of arguments (tagged)
    //  -- Slot 0 / sp[4*kSystemPointerSize]: context
    // -----------------------------------
    // Deoptimizer enters here.
    masm->isolate()->heap()->SetConstructStubCreateDeoptPCOffset(
        masm->pc_offset());

    __ Bind(&post_instantiation_deopt_entry);

    // Restore new target from the top of the stack.
    __ Peek(x3, 0 * kSystemPointerSize);

    // Restore constructor function and argument count.
    __ Ldr(x1, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ SmiUntag(x12, MemOperand(fp, ConstructFrameConstants::kLengthOffset));

    // Copy arguments to the expression stack. The called function pops the
    // receiver along with its arguments, so we need an extra receiver on the
    // stack, in case we have to return it later.

    // Overwrite the new target with a receiver.
    __ Poke(x0, 0);

    // Push two further copies of the receiver. One will be popped by the called
    // function. The second acts as padding if the number of arguments plus
    // receiver is odd - pushing receiver twice avoids branching. It also means
    // that we don't have to handle the even and odd cases specially on
    // InvokeFunction's return, as top of stack will be the receiver in either
    // case.
    __ Push(x0, x0);

    // ----------- S t a t e -------------
    //  --                              x3: new target
    //  --                             x12: number of arguments (untagged)
    //  --        sp[0*kSystemPointerSize]: implicit receiver (overwrite if argc
    //  odd)
    //  --        sp[1*kSystemPointerSize]: implicit receiver
    //  --        sp[2*kSystemPointerSize]: implicit receiver
    //  --        sp[3*kSystemPointerSize]: padding
    //  -- x1 and sp[4*kSystemPointerSize]: constructor function
    //  --        sp[5*kSystemPointerSize]: number of arguments (tagged)
    //  --        sp[6*kSystemPointerSize]: context
    // -----------------------------------

    // Round the number of arguments down to the next even number, and claim
    // slots for the arguments. If the number of arguments was odd, the last
    // argument will overwrite one of the receivers pushed above.
    __ Bic(x10, x12, 1);

    // Check if we have enough stack space to push all arguments.
    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, x10, &stack_overflow);
    __ B(&enough_stack_space);

    __ Bind(&stack_overflow);
    // Restore the context from the frame.
    __ Ldr(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();

    __ Bind(&enough_stack_space);
    __ Claim(x10);

    // Copy the arguments.
    {
      Register count = x2;
      Register dst = x10;
      Register src = x11;
      __ Mov(count, x12);
      __ SlotAddress(dst, 0);
      __ Add(src, fp, StandardFrameConstants::kCallerSPOffset);
      __ CopyDoubleWords(dst, src, count);
    }

    // Call the function.
    __ Mov(x0, x12);
    __ InvokeFunctionWithNewTarget(x1, x3, x0, CALL_FUNCTION);

    // ----------- S t a t e -------------
    //  -- sp[0*kSystemPointerSize]: implicit receiver
    //  -- sp[1*kSystemPointerSize]: padding
    //  -- sp[2*kSystemPointerSize]: constructor function
    //  -- sp[3*kSystemPointerSize]: number of arguments
    //  -- sp[4*kSystemPointerSize]: context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore the context from the frame.
    __ Ldr(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ CompareRoot(x0, RootIndex::kUndefinedValue);
    __ B(eq, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(x0, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ JumpIfObjectType(x0, x4, x5, FIRST_JS_RECEIVER_TYPE, &leave_frame, ge);
    __ B(&use_receiver);

    __ Bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ Bind(&use_receiver);
    __ Peek(x0, 0 * kSystemPointerSize);
    __ CompareRoot(x0, RootIndex::kTheHoleValue);
    __ B(eq, &do_throw);

    __ Bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ SmiUntag(x1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  __ DropArguments(x1, TurboAssembler::kCountExcludesReceiver);
  __ Ret();
}
void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ PushArgument(x1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

static void GetSharedFunctionInfoBytecode(MacroAssembler* masm,
                                          Register sfi_data,
                                          Register scratch1) {
  Label done;
  __ CompareObjectType(sfi_data, scratch1, scratch1, INTERPRETER_DATA_TYPE);
  __ B(ne, &done);
  __ LoadTaggedPointerField(
      sfi_data,
      FieldMemOperand(sfi_data, InterpreterData::kBytecodeArrayOffset));
  __ Bind(&done);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the value to pass to the generator
  //  -- x1 : the JSGeneratorObject to resume
  //  -- lr : return address
  // -----------------------------------
  __ AssertGeneratorObject(x1);

  // Store input value into generator object.
  __ StoreTaggedField(
      x0, FieldMemOperand(x1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(x1, JSGeneratorObject::kInputOrDebugPosOffset, x0,
                      kLRHasNotBeenSaved, kDontSaveFPRegs);

  // Load suspended function and context.
  __ LoadTaggedPointerField(
      x4, FieldMemOperand(x1, JSGeneratorObject::kFunctionOffset));
  __ LoadTaggedPointerField(cp,
                            FieldMemOperand(x4, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ Mov(x10, debug_hook);
  __ Ldrsb(x10, MemOperand(x10));
  __ CompareAndBranch(x10, Operand(0), ne, &prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ Mov(x10, debug_suspended_generator);
  __ Ldr(x10, MemOperand(x10));
  __ CompareAndBranch(x10, Operand(x1), eq,
                      &prepare_step_in_suspended_generator);
  __ Bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  LoadRealStackLimit(masm, x10);
  __ Cmp(sp, x10);
  __ B(lo, &stack_overflow);

  // Get number of arguments for generator function.
  __ LoadTaggedPointerField(
      x10, FieldMemOperand(x4, JSFunction::kSharedFunctionInfoOffset));
  __ Ldrh(w10, FieldMemOperand(
                   x10, SharedFunctionInfo::kFormalParameterCountOffset));

  // Claim slots for arguments and receiver (rounded up to a multiple of two).
  __ Add(x11, x10, 2);
  __ Bic(x11, x11, 1);
  __ Claim(x11);

  // Store padding (which might be replaced by the receiver).
  __ Sub(x11, x11, 1);
  __ Poke(padreg, Operand(x11, LSL, kSystemPointerSizeLog2));

  // Poke receiver into highest claimed slot.
  __ LoadTaggedPointerField(
      x5, FieldMemOperand(x1, JSGeneratorObject::kReceiverOffset));
  __ Poke(x5, Operand(x10, LSL, kSystemPointerSizeLog2));

  // ----------- S t a t e -------------
  //  -- x1                       : the JSGeneratorObject to resume
  //  -- x4                       : generator function
  //  -- x10                      : argument count
  //  -- cp                       : generator context
  //  -- lr                       : return address
  //  -- sp[arg count]            : generator receiver
  //  -- sp[0 .. arg count - 1]   : claimed for args
  // -----------------------------------

  // Copy the function arguments from the generator object's register file.

  __ LoadTaggedPointerField(
      x5,
      FieldMemOperand(x1, JSGeneratorObject::kParametersAndRegistersOffset));
  {
    Label loop, done;
    __ Cbz(x10, &done);
    __ Mov(x12, 0);

    __ Bind(&loop);
    __ Sub(x10, x10, 1);
    __ Add(x11, x5, Operand(x12, LSL, kTaggedSizeLog2));
    __ LoadAnyTaggedField(x11, FieldMemOperand(x11, FixedArray::kHeaderSize));
    __ Poke(x11, Operand(x10, LSL, kSystemPointerSizeLog2));
    __ Add(x12, x12, 1);
    __ Cbnz(x10, &loop);
    __ Bind(&done);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ LoadTaggedPointerField(
        x3, FieldMemOperand(x4, JSFunction::kSharedFunctionInfoOffset));
    __ LoadTaggedPointerField(
        x3, FieldMemOperand(x3, SharedFunctionInfo::kFunctionDataOffset));
    GetSharedFunctionInfoBytecode(masm, x3, x0);
    __ CompareObjectType(x3, x3, x3, BYTECODE_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ LoadTaggedPointerField(
        x0, FieldMemOperand(x4, JSFunction::kSharedFunctionInfoOffset));
    __ Ldrh(w0, FieldMemOperand(
                    x0, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Mov(x3, x1);
    __ Mov(x1, x4);
    static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
    __ LoadTaggedPointerField(x2, FieldMemOperand(x1, JSFunction::kCodeOffset));
    __ JumpCodeObject(x2);
  }

  __ Bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push hole as receiver since we do not use it for stepping.
    __ LoadRoot(x5, RootIndex::kTheHoleValue);
    __ Push(x1, padreg, x4, x5);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(padreg, x1);
    __ LoadTaggedPointerField(
        x4, FieldMemOperand(x1, JSGeneratorObject::kFunctionOffset));
  }
  __ B(&stepping_prepared);

  __ Bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(x1, padreg);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(padreg, x1);
    __ LoadTaggedPointerField(
        x4, FieldMemOperand(x1, JSGeneratorObject::kFunctionOffset));
  }
  __ B(&stepping_prepared);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();  // This should be unreachable.
  }
}

namespace {

// Called with the native C calling convention. The corresponding function
// signature is either:
//
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, Address new_target, Address target,
//       Address receiver, intptr_t argc, Address** argv)>;
// or
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, MicrotaskQueue* microtask_queue)>;
//
// Input is either:
//   x0: root_register_value.
//   x1: new_target.
//   x2: target.
//   x3: receiver.
//   x4: argc.
//   x5: argv.
// or
//   x0: root_register_value.
//   x1: microtask_queue.
// Output:
//   x0: result.
void Generate_JSEntryVariant(MacroAssembler* masm, StackFrame::Type type,
                             Builtins::Name entry_trampoline) {
  Label invoke, handler_entry, exit;

  {
    NoRootArrayScope no_root_array(masm);

#if defined(V8_OS_WIN)
    // Windows ARM64 relies on a frame pointer (fp/x29 which are aliases to each
    // other) chain to do stack unwinding, but JSEntry breaks that by setting fp
    // to point to bad_frame_pointer below. To fix unwind information for this
    // case, JSEntry registers the offset (from current fp to the caller's fp
    // saved by PushCalleeSavedRegisters on stack) to xdata_encoder which then
    // emits the offset value as part of result unwind data accordingly.
    win64_unwindinfo::XdataEncoder* xdata_encoder = masm->GetXdataEncoder();
    if (xdata_encoder) {
      xdata_encoder->onFramePointerAdjustment(
          EntryFrameConstants::kDirectCallerFPOffset,
          EntryFrameConstants::kDirectCallerSPOffset);
    }
#endif

    __ PushCalleeSavedRegisters();

    // Set up the reserved register for 0.0.
    __ Fmov(fp_zero, 0.0);

    // Initialize the root register.
    // C calling convention. The first argument is passed in x0.
    __ Mov(kRootRegister, x0);
  }

  // Build an entry frame (see layout below).
  int64_t bad_frame_pointer = -1L;  // Bad frame pointer to fail if it is used.
  __ Mov(x13, bad_frame_pointer);
  __ Mov(x12, StackFrame::TypeToMarker(type));
  __ Mov(x11, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                        masm->isolate()));
  __ Ldr(x10, MemOperand(x11));

  // x13 (the bad frame pointer) is the first item pushed.
  STATIC_ASSERT(EntryFrameConstants::kOffsetToCalleeSavedRegisters ==
                1 * kSystemPointerSize);

  __ Push(x13, x12, xzr, x10);
  // Set up fp.
  __ Sub(fp, sp, EntryFrameConstants::kCallerFPOffset);

  // Push the JS entry frame marker. Also set js_entry_sp if this is the
  // outermost JS call.
  Label done;
  ExternalReference js_entry_sp = ExternalReference::Create(
      IsolateAddressId::kJSEntrySPAddress, masm->isolate());
  __ Mov(x10, js_entry_sp);
  __ Ldr(x11, MemOperand(x10));

  // Select between the inner and outermost frame marker, based on the JS entry
  // sp. We assert that the inner marker is zero, so we can use xzr to save a
  // move instruction.
  DCHECK_EQ(StackFrame::INNER_JSENTRY_FRAME, 0);
  __ Cmp(x11, 0);  // If x11 is zero, this is the outermost frame.
  __ Csel(x12, xzr, StackFrame::OUTERMOST_JSENTRY_FRAME, ne);
  __ B(ne, &done);
  __ Str(fp, MemOperand(x10));

  __ Bind(&done);
  __ Push(x12, padreg);

  // The frame set up looks like this:
  // sp[0] : padding.
  // sp[1] : JS entry frame marker.
  // sp[2] : C entry FP.
  // sp[3] : stack frame marker.
  // sp[4] : stack frame marker.
  // sp[5] : bad frame pointer 0xFFF...FF   <- fp points here.

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ B(&invoke);

  // Prevent the constant pool from being emitted between the record of the
  // handler_entry position and the first instruction of the sequence here.
  // There is no risk because Assembler::Emit() emits the instruction before
  // checking for constant pool emission, but we do not want to depend on
  // that.
  {
    Assembler::BlockPoolsScope block_pools(masm);
    __ bind(&handler_entry);

    // Store the current pc as the handler offset. It's used later to create the
    // handler table.
    masm->isolate()->builtins()->SetJSEntryHandlerOffset(handler_entry.pos());

    // Caught exception: Store result (exception) in the pending exception
    // field in the JSEnv and return a failure sentinel. Coming in here the
    // fp will be invalid because the PushTryHandler below sets it to 0 to
    // signal the existence of the JSEntry frame.
    __ Mov(x10,
           ExternalReference::Create(IsolateAddressId::kPendingExceptionAddress,
                                     masm->isolate()));
  }
  __ Str(x0, MemOperand(x10));
  __ LoadRoot(x0, RootIndex::kException);
  __ B(&exit);

  // Invoke: Link this frame into the handler chain.
  __ Bind(&invoke);

  // Push new stack handler.
  static_assert(StackHandlerConstants::kSize == 2 * kSystemPointerSize,
                "Unexpected offset for StackHandlerConstants::kSize");
  static_assert(StackHandlerConstants::kNextOffset == 0 * kSystemPointerSize,
                "Unexpected offset for StackHandlerConstants::kNextOffset");

  // Link the current handler as the next handler.
  __ Mov(x11, ExternalReference::Create(IsolateAddressId::kHandlerAddress,
                                        masm->isolate()));
  __ Ldr(x10, MemOperand(x11));
  __ Push(padreg, x10);

  // Set this new handler as the current one.
  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.AcquireX();
    __ Mov(scratch, sp);
    __ Str(scratch, MemOperand(x11));
  }

  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the B(&invoke) above, which
  // restores all callee-saved registers (including cp and fp) to their
  // saved values before returning a failure to C.
  //
  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return.
  Handle<Code> trampoline_code =
      masm->isolate()->builtins()->builtin_handle(entry_trampoline);
  __ Call(trampoline_code, RelocInfo::CODE_TARGET);

  // Pop the stack handler and unlink this frame from the handler chain.
  static_assert(StackHandlerConstants::kNextOffset == 0 * kSystemPointerSize,
                "Unexpected offset for StackHandlerConstants::kNextOffset");
  __ Pop(x10, padreg);
  __ Mov(x11, ExternalReference::Create(IsolateAddressId::kHandlerAddress,
                                        masm->isolate()));
  __ Drop(StackHandlerConstants::kSlotCount - 2);
  __ Str(x10, MemOperand(x11));

  __ Bind(&exit);
  // x0 holds the result.
  // The stack pointer points to the top of the entry frame pushed on entry from
  // C++ (at the beginning of this stub):
  // sp[0] : padding.
  // sp[1] : JS entry frame marker.
  // sp[2] : C entry FP.
  // sp[3] : stack frame marker.
  // sp[4] : stack frame marker.
  // sp[5] : bad frame pointer 0xFFF...FF   <- fp points here.

  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  {
    Register c_entry_fp = x11;
    __ PeekPair(x10, c_entry_fp, 1 * kSystemPointerSize);
    __ Cmp(x10, StackFrame::OUTERMOST_JSENTRY_FRAME);
    __ B(ne, &non_outermost_js_2);
    __ Mov(x12, js_entry_sp);
    __ Str(xzr, MemOperand(x12));
    __ Bind(&non_outermost_js_2);

    // Restore the top frame descriptors from the stack.
    __ Mov(x12, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                          masm->isolate()));
    __ Str(c_entry_fp, MemOperand(x12));
  }

  // Reset the stack to the callee saved registers.
  static_assert(
      EntryFrameConstants::kFixedFrameSize % (2 * kSystemPointerSize) == 0,
      "Size of entry frame is not a multiple of 16 bytes");
  __ Drop(EntryFrameConstants::kFixedFrameSize / kSystemPointerSize);
  // Restore the callee-saved registers and return.
  __ PopCalleeSavedRegisters();
  __ Ret();
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

// Input:
//   x1: new.target.
//   x2: function.
//   x3: receiver.
//   x4: argc.
//   x5: argv.
// Output:
//   x0: result.
static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  Register new_target = x1;
  Register function = x2;
  Register receiver = x3;
  Register argc = x4;
  Register argv = x5;
  Register scratch = x10;
  Register slots_to_claim = x11;

  {
    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    __ Mov(scratch, ExternalReference::Create(IsolateAddressId::kContextAddress,
                                              masm->isolate()));
    __ Ldr(cp, MemOperand(scratch));

    // Claim enough space for the arguments, the receiver and the function,
    // including an optional slot of padding.
    __ Add(slots_to_claim, argc, 3);
    __ Bic(slots_to_claim, slots_to_claim, 1);

    // Check if we have enough stack space to push all arguments.
    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, slots_to_claim, &stack_overflow);
    __ B(&enough_stack_space);

    __ Bind(&stack_overflow);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();

    __ Bind(&enough_stack_space);
    __ Claim(slots_to_claim);

    // Store padding (which might be overwritten).
    __ SlotAddress(scratch, slots_to_claim);
    __ Str(padreg, MemOperand(scratch, -kSystemPointerSize));

    // Store receiver and function on the stack.
    __ SlotAddress(scratch, argc);
    __ Stp(receiver, function, MemOperand(scratch));

    // Copy arguments to the stack in a loop, in reverse order.
    // x4: argc.
    // x5: argv.
    Label loop, done;

    // Skip the argument set up if we have no arguments.
    __ Cbz(argc, &done);

    // scratch has been set to point to the location of the receiver, which
    // marks the end of the argument copy.

    __ Bind(&loop);
    // Load the handle.
    __ Ldr(x11, MemOperand(argv, kSystemPointerSize, PostIndex));
    // Dereference the handle.
    __ Ldr(x11, MemOperand(x11));
    // Poke the result into the stack.
    __ Str(x11, MemOperand(scratch, -kSystemPointerSize, PreIndex));
    // Loop if we've not reached the end of copy marker.
    __ Cmp(sp, scratch);
    __ B(lt, &loop);

    __ Bind(&done);

    __ Mov(x0, argc);
    __ Mov(x3, new_target);
    __ Mov(x1, function);
    // x0: argc.
    // x1: function.
    // x3: new.target.

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    // The original values have been saved in JSEntry.
    __ LoadRoot(x19, RootIndex::kUndefinedValue);
    __ Mov(x20, x19);
    __ Mov(x21, x19);
    __ Mov(x22, x19);
    __ Mov(x23, x19);
    __ Mov(x24, x19);
    __ Mov(x25, x19);
    __ Mov(x28, x19);
    // Don't initialize the reserved registers.
    // x26 : root register (kRootRegister).
    // x27 : context pointer (cp).
    // x29 : frame pointer (fp).

    Handle<Code> builtin = is_construct
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the JS internal frame and remove the parameters (except function),
    // and return.
  }

  // Result is in x0. Return.
  __ Ret();
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
  //   x0: root_register_value
  //   x1: microtask_queue

  __ Mov(RunMicrotasksDescriptor::MicrotaskQueueRegister(), x1);
  __ Jump(BUILTIN_CODE(masm->isolate(), RunMicrotasks), RelocInfo::CODE_TARGET);
}

static void ReplaceClosureCodeWithOptimizedCode(MacroAssembler* masm,
                                                Register optimized_code,
                                                Register closure) {
  // Store code entry in the closure.
  __ StoreTaggedField(optimized_code,
                      FieldMemOperand(closure, JSFunction::kCodeOffset));
  __ RecordWriteField(closure, JSFunction::kCodeOffset, optimized_code,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_size = scratch;

  // Get the arguments + receiver count.
  __ Ldr(args_size,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Ldr(args_size.W(),
         FieldMemOperand(args_size, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  // Drop receiver + arguments.
  if (__ emit_debug_code()) {
    __ Tst(args_size, kSystemPointerSize - 1);
    __ Check(eq, AbortReason::kUnexpectedValue);
  }
  __ Lsr(args_size, args_size, kSystemPointerSizeLog2);
  __ DropArguments(args_size);
}

// Tail-call |function_id| if |smi_entry| == |marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register smi_entry,
                                          OptimizationMarker marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ CompareTaggedAndBranch(smi_entry, Operand(Smi::FromEnum(marker)), ne,
                            &no_match);
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void TailCallOptimizedCodeSlot(MacroAssembler* masm,
                                      Register optimized_code_entry,
                                      Register scratch) {
  // ----------- S t a t e -------------
  //  -- x3 : new target (preserved for callee if needed, and caller)
  //  -- x1 : target function (preserved for callee if needed, and caller)
  // -----------------------------------
  DCHECK(!AreAliased(x1, x3, optimized_code_entry, scratch));

  Register closure = x1;

  // Check if the optimized code is marked for deopt. If it is, call the
  // runtime to clear it.
  Label found_deoptimized_code;
  __ LoadTaggedPointerField(
      scratch,
      FieldMemOperand(optimized_code_entry, Code::kCodeDataContainerOffset));
  __ Ldr(scratch.W(),
         FieldMemOperand(scratch, CodeDataContainer::kKindSpecificFlagsOffset));
  __ Tbnz(scratch.W(), Code::kMarkedForDeoptimizationBit,
          &found_deoptimized_code);

  // Optimized code is good, get it into the closure and link the closure into
  // the optimized functions list, then tail call the optimized code.
  ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure);
  static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
  __ LoadCodeObjectEntry(x2, optimized_code_entry);
  __ Jump(x2);

  // Optimized code slot contains deoptimized code, evict it and re-enter the
  // closure's code.
  __ bind(&found_deoptimized_code);
  GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
}

static void MaybeOptimizeCode(MacroAssembler* masm, Register feedback_vector,
                              Register optimization_marker) {
  // ----------- S t a t e -------------
  //  -- x3 : new target (preserved for callee if needed, and caller)
  //  -- x1 : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  //  -- optimization_marker : a Smi containing a non-zero optimization marker.
  // -----------------------------------
  DCHECK(!AreAliased(feedback_vector, x1, x3, optimization_marker));

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

  // Otherwise, the marker is InOptimizationQueue, so fall through hoping
  // that an interrupt will eventually update the slot with optimized code.
  if (FLAG_debug_code) {
    __ CmpTagged(
        optimization_marker,
        Operand(Smi::FromEnum(OptimizationMarker::kInOptimizationQueue)));
    __ Assert(eq, AbortReason::kExpectedOptimizationSentinel);
  }
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

  __ Mov(bytecode_size_table, ExternalReference::bytecode_size_table_address());

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  STATIC_ASSERT(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  STATIC_ASSERT(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ Cmp(bytecode, Operand(0x3));
  __ B(hi, &process_bytecode);
  __ Tst(bytecode, Operand(0x1));
  // The code to load the next bytecode is common to both wide and extra wide.
  // We can hoist them up here since they do not modify the flags after Tst.
  __ Add(bytecode_offset, bytecode_offset, Operand(1));
  __ Ldrb(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ B(ne, &extra_wide);

  // Update table to the wide scaled table.
  __ Add(bytecode_size_table, bytecode_size_table,
         Operand(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ B(&process_bytecode);

  __ Bind(&extra_wide);
  // Update table to the extra wide scaled table.
  __ Add(bytecode_size_table, bytecode_size_table,
         Operand(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));

  __ Bind(&process_bytecode);

// Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)                                              \
  __ Cmp(x1, Operand(static_cast<int>(interpreter::Bytecode::k##NAME))); \
  __ B(if_return, eq);
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // Otherwise, load the size of the current bytecode and advance the offset.
  __ Ldr(scratch1.W(), MemOperand(bytecode_size_table, bytecode, LSL, 2));
  __ Add(bytecode_offset, bytecode_offset, scratch1);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   - x1: the JS function object being called.
//   - x3: the incoming new target or generator object
//   - cp: our context.
//   - fp: our caller's frame pointer.
//   - lr: return address.
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  Register closure = x1;
  Register feedback_vector = x2;

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  __ LoadTaggedPointerField(
      x0, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ LoadTaggedPointerField(
      kInterpreterBytecodeArrayRegister,
      FieldMemOperand(x0, SharedFunctionInfo::kFunctionDataOffset));
  GetSharedFunctionInfoBytecode(masm, kInterpreterBytecodeArrayRegister, x11);

  // The bytecode array could have been flushed from the shared function info,
  // if so, call into CompileLazy.
  Label compile_lazy;
  __ CompareObjectType(kInterpreterBytecodeArrayRegister, x0, x0,
                       BYTECODE_ARRAY_TYPE);
  __ B(ne, &compile_lazy);

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
      x7, FieldMemOperand(feedback_vector, HeapObject::kMapOffset));
  __ Ldrh(x7, FieldMemOperand(x7, Map::kInstanceTypeOffset));
  __ Cmp(x7, FEEDBACK_VECTOR_TYPE);
  __ B(ne, &push_stack_frame);

  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  Register optimized_code_entry = x7;
  __ LoadAnyTaggedField(
      optimized_code_entry,
      FieldMemOperand(feedback_vector,
                      FeedbackVector::kOptimizedCodeWeakOrSmiOffset));

  // Check if the optimized code slot is not empty.
  Label optimized_code_slot_not_empty;
  __ CompareTaggedAndBranch(optimized_code_entry,
                            Operand(Smi::FromEnum(OptimizationMarker::kNone)),
                            ne, &optimized_code_slot_not_empty);

  Label not_optimized;
  __ bind(&not_optimized);

  // Increment invocation count for the function.
  __ Ldr(w10, FieldMemOperand(feedback_vector,
                              FeedbackVector::kInvocationCountOffset));
  __ Add(w10, w10, Operand(1));
  __ Str(w10, FieldMemOperand(feedback_vector,
                              FeedbackVector::kInvocationCountOffset));

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  __ Bind(&push_stack_frame);
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ Push(lr, fp, cp, closure);
  __ Add(fp, sp, StandardFrameConstants::kFixedFrameSizeFromFp);

  // Reset code age.
  // Reset code age and the OSR arming. The OSR field and BytecodeAgeOffset are
  // 8-bit fields next to each other, so we could just optimize by writing a
  // 16-bit. These static asserts guard our assumption is valid.
  STATIC_ASSERT(BytecodeArray::kBytecodeAgeOffset ==
                BytecodeArray::kOsrNestingLevelOffset + kCharSize);
  STATIC_ASSERT(BytecodeArray::kNoAgeBytecodeAge == 0);
  __ Strh(wzr, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                               BytecodeArray::kOsrNestingLevelOffset));

  // Load the initial bytecode offset.
  __ Mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(x0, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, x0);

  // Allocate the local and temporary register file on the stack.
  Label stack_overflow;
  {
    // Load frame size from the BytecodeArray object.
    __ Ldr(w11, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    __ Sub(x10, sp, Operand(x11));
    {
      UseScratchRegisterScope temps(masm);
      Register scratch = temps.AcquireX();
      LoadRealStackLimit(masm, scratch);
      __ Cmp(x10, scratch);
    }
    __ B(lo, &stack_overflow);

    // If ok, push undefined as the initial value for all register file entries.
    // Note: there should always be at least one stack slot for the return
    // register in the register file.
    Label loop_header;
    __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
    __ Lsr(x11, x11, kSystemPointerSizeLog2);
    // Round up the number of registers to a multiple of 2, to align the stack
    // to 16 bytes.
    __ Add(x11, x11, 1);
    __ Bic(x11, x11, 1);
    __ PushMultipleTimes(kInterpreterAccumulatorRegister, x11);
    __ Bind(&loop_header);
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in x3.
  Label no_incoming_new_target_or_generator_register;
  __ Ldrsw(x10,
           FieldMemOperand(
               kInterpreterBytecodeArrayRegister,
               BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ Cbz(x10, &no_incoming_new_target_or_generator_register);
  __ Str(x3, MemOperand(fp, x10, LSL, kSystemPointerSizeLog2));
  __ Bind(&no_incoming_new_target_or_generator_register);

  // The accumulator is already loaded with undefined.

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ Mov(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));
  __ Ldrb(x23, MemOperand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister));
  __ Mov(x1, Operand(x23, LSL, kSystemPointerSizeLog2));
  __ Ldr(kJavaScriptCallCodeStartRegister,
         MemOperand(kInterpreterDispatchTableRegister, x1));
  __ Call(kJavaScriptCallCodeStartRegister);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ Ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  // Either return, or advance to the next bytecode and dispatch.
  Label do_return;
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, x1, x2,
                                &do_return);
  __ B(&do_dispatch);

  __ bind(&do_return);
  // The return value is in x0.
  LeaveInterpreterFrame(masm, x2);
  __ Ret();

  __ bind(&optimized_code_slot_not_empty);
  Label maybe_has_optimized_code;
  // Check if optimized code marker is actually a weak reference to the
  // optimized code as opposed to an optimization marker.
  __ JumpIfNotSmi(optimized_code_entry, &maybe_has_optimized_code);
  MaybeOptimizeCode(masm, feedback_vector, optimized_code_entry);
  // Fall through if there's no runnable optimized code.
  __ jmp(&not_optimized);

  __ bind(&maybe_has_optimized_code);
  // Load code entry from the weak reference, if it was cleared, resume
  // execution of unoptimized code.
  __ LoadWeakValue(optimized_code_entry, optimized_code_entry, &not_optimized);
  TailCallOptimizedCodeSlot(masm, optimized_code_entry, x4);

  __ bind(&compile_lazy);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
  __ Unreachable();  // Should not return.

  __ bind(&stack_overflow);
  __ CallRuntime(Runtime::kThrowStackOverflow);
  __ Unreachable();  // Should not return.
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args,
                                         Register first_arg_index,
                                         Register spread_arg_out,
                                         ConvertReceiverMode receiver_mode,
                                         InterpreterPushArgsMode mode) {
  Register last_arg_addr = x10;
  Register stack_addr = x11;
  Register slots_to_claim = x12;
  Register slots_to_copy = x13;  // May include receiver, unlike num_args.

  DCHECK(!AreAliased(num_args, first_arg_index, last_arg_addr, stack_addr,
                     slots_to_claim, slots_to_copy));
  // spread_arg_out may alias with the first_arg_index input.
  DCHECK(!AreAliased(spread_arg_out, last_arg_addr, stack_addr, slots_to_claim,
                     slots_to_copy));

  // Add one slot for the receiver.
  __ Add(slots_to_claim, num_args, 1);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Exclude final spread from slots to claim and the number of arguments.
    __ Sub(slots_to_claim, slots_to_claim, 1);
    __ Sub(num_args, num_args, 1);
  }

  // Add a stack check before pushing arguments.
  Label stack_overflow, done;
  Generate_StackOverflowCheck(masm, slots_to_claim, &stack_overflow);
  __ B(&done);
  __ Bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ Unreachable();
  __ Bind(&done);

  // Round up to an even number of slots and claim them.
  __ Add(slots_to_claim, slots_to_claim, 1);
  __ Bic(slots_to_claim, slots_to_claim, 1);
  __ Claim(slots_to_claim);

  {
    // Store padding, which may be overwritten.
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.AcquireX();
    __ Sub(scratch, slots_to_claim, 1);
    __ Poke(padreg, Operand(scratch, LSL, kSystemPointerSizeLog2));
  }

  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    // Store "undefined" as the receiver arg if we need to.
    Register receiver = x14;
    __ LoadRoot(receiver, RootIndex::kUndefinedValue);
    __ SlotAddress(stack_addr, num_args);
    __ Str(receiver, MemOperand(stack_addr));
    __ Mov(slots_to_copy, num_args);
  } else {
    // If we're not given an explicit receiver to store, we'll need to copy it
    // together with the rest of the arguments.
    __ Add(slots_to_copy, num_args, 1);
  }

  __ Sub(last_arg_addr, first_arg_index,
         Operand(slots_to_copy, LSL, kSystemPointerSizeLog2));
  __ Add(last_arg_addr, last_arg_addr, kSystemPointerSize);

  // Load the final spread argument into spread_arg_out, if necessary.
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Ldr(spread_arg_out, MemOperand(last_arg_addr, -kSystemPointerSize));
  }

  // Copy the rest of the arguments.
  __ SlotAddress(stack_addr, 0);
  __ CopyDoubleWords(stack_addr, last_arg_addr, slots_to_copy);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- x1 : the target to call (can be any Object).
  // -----------------------------------

  // Push the arguments. num_args may be updated according to mode.
  // spread_arg_out will be updated to contain the last spread argument, when
  // mode == InterpreterPushArgsMode::kWithFinalSpread.
  Register num_args = x0;
  Register first_arg_index = x2;
  Register spread_arg_out =
      (mode == InterpreterPushArgsMode::kWithFinalSpread) ? x2 : no_reg;
  Generate_InterpreterPushArgs(masm, num_args, first_arg_index, spread_arg_out,
                               receiver_mode, mode);

  // Call the target.
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(BUILTIN_CODE(masm->isolate(), CallWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny),
            RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  // -- x0 : argument count (not including receiver)
  // -- x3 : new target
  // -- x1 : constructor to call
  // -- x2 : allocation site feedback if available, undefined otherwise
  // -- x4 : address of the first argument
  // -----------------------------------
  __ AssertUndefinedOrAllocationSite(x2);

  // Push the arguments. num_args may be updated according to mode.
  // spread_arg_out will be updated to contain the last spread argument, when
  // mode == InterpreterPushArgsMode::kWithFinalSpread.
  Register num_args = x0;
  Register first_arg_index = x4;
  Register spread_arg_out =
      (mode == InterpreterPushArgsMode::kWithFinalSpread) ? x2 : no_reg;
  Generate_InterpreterPushArgs(masm, num_args, first_arg_index, spread_arg_out,
                               ConvertReceiverMode::kNullOrUndefined, mode);

  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    __ AssertFunction(x1);

    // Tail call to the array construct stub (still in the caller
    // context at this point).
    Handle<Code> code = BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl);
    __ Jump(code, RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with x0, x1, and x3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with x0, x1, and x3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
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
  __ Ldr(x1, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      x1, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ LoadTaggedPointerField(
      x1, FieldMemOperand(x1, SharedFunctionInfo::kFunctionDataOffset));
  __ CompareObjectType(x1, kInterpreterDispatchTableRegister,
                       kInterpreterDispatchTableRegister,
                       INTERPRETER_DATA_TYPE);
  __ B(ne, &builtin_trampoline);

  __ LoadTaggedPointerField(
      x1, FieldMemOperand(x1, InterpreterData::kInterpreterTrampolineOffset));
  __ Add(x1, x1, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ B(&trampoline_loaded);

  __ Bind(&builtin_trampoline);
  __ Mov(x1, ExternalReference::
                 address_of_interpreter_entry_trampoline_instruction_start(
                     masm->isolate()));
  __ Ldr(x1, MemOperand(x1));

  __ Bind(&trampoline_loaded);
  __ Add(lr, x1, Operand(interpreter_entry_return_pc_offset.value()));

  // Initialize the dispatch table register.
  __ Mov(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Get the bytecode array pointer from the frame.
  __ Ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(
        kInterpreterBytecodeArrayRegister,
        AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, x1, x1,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(
        eq, AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ SmiUntag(kInterpreterBytecodeOffsetRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  // Dispatch to the target bytecode.
  __ Ldrb(x23, MemOperand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister));
  __ Mov(x1, Operand(x23, LSL, kSystemPointerSizeLog2));
  __ Ldr(kJavaScriptCallCodeStartRegister,
         MemOperand(kInterpreterDispatchTableRegister, x1));
  __ Jump(kJavaScriptCallCodeStartRegister);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Get bytecode array and bytecode offset from the stack frame.
  __ ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  // Load the current bytecode.
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));

  // Advance to the next bytecode.
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, x1, x2,
                                &if_return);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(x2, kInterpreterBytecodeOffsetRegister);
  __ Str(x2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);

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
  int frame_size = BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp +
                   (allocatable_register_count +
                    BuiltinContinuationFrameConstants::PaddingSlotCount(
                        allocatable_register_count)) *
                       kSystemPointerSize;

  // Set up frame pointer.
  __ Add(fp, sp, frame_size);

  if (with_result) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point.
    __ Str(x0,
           MemOperand(fp, BuiltinContinuationFrameConstants::kCallerSPOffset));
  }

  // Restore registers in pairs.
  int offset = -BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp -
               allocatable_register_count * kSystemPointerSize;
  for (int i = allocatable_register_count - 1; i > 0; i -= 2) {
    int code1 = config->GetAllocatableGeneralCode(i);
    int code2 = config->GetAllocatableGeneralCode(i - 1);
    Register reg1 = Register::from_code(code1);
    Register reg2 = Register::from_code(code2);
    __ Ldp(reg1, reg2, MemOperand(fp, offset));
    offset += 2 * kSystemPointerSize;
  }

  // Restore first register separately, if number of registers is odd.
  if (allocatable_register_count % 2 != 0) {
    int code = config->GetAllocatableGeneralCode(0);
    __ Ldr(Register::from_code(code), MemOperand(fp, offset));
  }

  if (java_script_builtin) __ SmiUntag(kJavaScriptCallArgCountRegister);

  // Load builtin index (stored as a Smi) and use it to get the builtin start
  // address from the builtins table.
  UseScratchRegisterScope temps(masm);
  Register builtin = temps.AcquireX();
  __ Ldr(
      builtin,
      MemOperand(fp, BuiltinContinuationFrameConstants::kBuiltinIndexOffset));

  // Restore fp, lr.
  __ Mov(sp, fp);
  __ Pop(fp, lr);

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
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  // Pop TOS register and padding.
  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), x0.code());
  __ Pop(x0, padreg);
  __ Ret();
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  Label skip;
  __ CompareTaggedAndBranch(x0, Smi::zero(), ne, &skip);
  __ Ret();

  __ Bind(&skip);

  // Drop the handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  __ LeaveFrame(StackFrame::STUB);

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ LoadTaggedPointerField(
      x1, FieldMemOperand(x0, Code::kDeoptimizationDataOffset));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ SmiUntagField(
      x1, FieldMemOperand(x1, FixedArray::OffsetOfElementAt(
                                  DeoptimizationData::kOsrPcOffsetIndex)));

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ Add(x0, x0, x1);
  __ Add(lr, x0, Code::kHeaderSize - kHeapObjectTag);

  // And "return" to the OSR entry point of the function.
  __ Ret();
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0       : argc
  //  -- sp[0]    : argArray (if argc == 2)
  //  -- sp[8]    : thisArg  (if argc >= 1)
  //  -- sp[16]   : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_FunctionPrototypeApply");

  Register argc = x0;
  Register arg_array = x2;
  Register receiver = x1;
  Register this_arg = x0;
  Register undefined_value = x3;
  Register null_value = x4;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);
  __ LoadRoot(null_value, RootIndex::kNullValue);

  // 1. Load receiver into x1, argArray into x2 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Register saved_argc = x10;
    Register scratch = x11;

    // Push two undefined values on the stack, to put it in a consistent state
    // so that we can always read three arguments from it.
    __ Push(undefined_value, undefined_value);

    // The state of the stack (with arrows pointing to the slots we will read)
    // is as follows:
    //
    //       argc = 0               argc = 1                argc = 2
    // -> sp[16]: receiver    -> sp[24]: receiver     -> sp[32]: receiver
    // -> sp[8]:  undefined   -> sp[16]: this_arg     -> sp[24]: this_arg
    // -> sp[0]:  undefined   -> sp[8]:  undefined    -> sp[16]: arg_array
    //                           sp[0]:  undefined       sp[8]:  undefined
    //                                                   sp[0]:  undefined
    //
    // There are now always three arguments to read, in the slots starting from
    // slot argc.
    __ SlotAddress(scratch, argc);

    __ Mov(saved_argc, argc);
    __ Ldp(arg_array, this_arg, MemOperand(scratch));  // Overwrites argc.
    __ Ldr(receiver, MemOperand(scratch, 2 * kSystemPointerSize));

    __ Drop(2);  // Drop the undefined values we pushed above.
    __ DropArguments(saved_argc, TurboAssembler::kCountExcludesReceiver);

    __ PushArgument(this_arg);
  }

  // ----------- S t a t e -------------
  //  -- x2      : argArray
  //  -- x1      : receiver
  //  -- sp[0]   : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ CmpTagged(arg_array, null_value);
  __ CcmpTagged(arg_array, undefined_value, ZFlag, ne);
  __ B(eq, &no_arguments);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ Bind(&no_arguments);
  {
    __ Mov(x0, 0);
    DCHECK_EQ(receiver, x1);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  Register argc = x0;
  Register function = x1;

  ASM_LOCATION("Builtins::Generate_FunctionPrototypeCall");

  // 1. Get the callable to call (passed as receiver) from the stack.
  __ Peek(function, Operand(argc, LSL, kXRegSizeLog2));

  // 2. Handle case with no arguments.
  {
    Label non_zero;
    Register scratch = x10;
    __ Cbnz(argc, &non_zero);
    __ LoadRoot(scratch, RootIndex::kUndefinedValue);
    // Overwrite receiver with undefined, which will be the new receiver.
    // We do not need to overwrite the padding slot above it with anything.
    __ Poke(scratch, 0);
    // Call function. The argument count is already zero.
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
    __ Bind(&non_zero);
  }

  // 3. Overwrite the receiver with padding. If argc is odd, this is all we
  //    need to do.
  Label arguments_ready;
  __ Poke(padreg, Operand(argc, LSL, kXRegSizeLog2));
  __ Tbnz(argc, 0, &arguments_ready);

  // 4. If argc is even:
  //    Copy arguments two slots higher in memory, overwriting the original
  //    receiver and padding.
  {
    Register copy_from = x10;
    Register copy_to = x11;
    Register count = x12;
    Register last_arg_slot = x13;
    __ Mov(count, argc);
    __ Sub(last_arg_slot, argc, 1);
    __ SlotAddress(copy_from, last_arg_slot);
    __ Add(copy_to, copy_from, 2 * kSystemPointerSize);
    __ CopyDoubleWords(copy_to, copy_from, count,
                       TurboAssembler::kSrcLessThanDst);
    // Drop two slots. These are copies of the last two arguments.
    __ Drop(2);
  }

  // 5. Adjust argument count to make the original first argument the new
  //    receiver and call the callable.
  __ Bind(&arguments_ready);
  __ Sub(argc, argc, 1);
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0       : argc
  //  -- sp[0]    : argumentsList (if argc == 3)
  //  -- sp[8]    : thisArgument  (if argc >= 2)
  //  -- sp[16]   : target        (if argc >= 1)
  //  -- sp[24]   : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_ReflectApply");

  Register argc = x0;
  Register arguments_list = x2;
  Register target = x1;
  Register this_argument = x4;
  Register undefined_value = x3;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load target into x1 (if present), argumentsList into x2 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    // Push four undefined values on the stack, to put it in a consistent state
    // so that we can always read the three arguments we need from it. The
    // fourth value is used for stack alignment.
    __ Push(undefined_value, undefined_value, undefined_value, undefined_value);

    // The state of the stack (with arrows pointing to the slots we will read)
    // is as follows:
    //
    //       argc = 0               argc = 1                argc = 2
    //    sp[32]: receiver       sp[40]: receiver        sp[48]: receiver
    // -> sp[24]: undefined   -> sp[32]: target       -> sp[40]: target
    // -> sp[16]: undefined   -> sp[24]: undefined    -> sp[32]: this_argument
    // -> sp[8]:  undefined   -> sp[16]: undefined    -> sp[24]: undefined
    //    sp[0]:  undefined      sp[8]:  undefined       sp[16]: undefined
    //                           sp[0]:  undefined       sp[8]:  undefined
    //                                                   sp[0]:  undefined
    //       argc = 3
    //    sp[56]: receiver
    // -> sp[48]: target
    // -> sp[40]: this_argument
    // -> sp[32]: arguments_list
    //    sp[24]: undefined
    //    sp[16]: undefined
    //    sp[8]:  undefined
    //    sp[0]:  undefined
    //
    // There are now always three arguments to read, in the slots starting from
    // slot (argc + 1).
    Register scratch = x10;
    __ SlotAddress(scratch, argc);
    __ Ldp(arguments_list, this_argument,
           MemOperand(scratch, 1 * kSystemPointerSize));
    __ Ldr(target, MemOperand(scratch, 3 * kSystemPointerSize));

    __ Drop(4);  // Drop the undefined values we pushed above.
    __ DropArguments(argc, TurboAssembler::kCountExcludesReceiver);

    __ PushArgument(this_argument);
  }

  // ----------- S t a t e -------------
  //  -- x2      : argumentsList
  //  -- x1      : target
  //  -- sp[0]   : thisArgument
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
  //  -- x0       : argc
  //  -- sp[0]    : new.target (optional)
  //  -- sp[8]    : argumentsList
  //  -- sp[16]   : target
  //  -- sp[24]   : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_ReflectConstruct");

  Register argc = x0;
  Register arguments_list = x2;
  Register target = x1;
  Register new_target = x3;
  Register undefined_value = x4;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load target into x1 (if present), argumentsList into x2 (if present),
  // new.target into x3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    // Push four undefined values on the stack, to put it in a consistent state
    // so that we can always read the three arguments we need from it. The
    // fourth value is used for stack alignment.
    __ Push(undefined_value, undefined_value, undefined_value, undefined_value);

    // The state of the stack (with arrows pointing to the slots we will read)
    // is as follows:
    //
    //       argc = 0               argc = 1                argc = 2
    //    sp[32]: receiver       sp[40]: receiver        sp[48]: receiver
    // -> sp[24]: undefined   -> sp[32]: target       -> sp[40]: target
    // -> sp[16]: undefined   -> sp[24]: undefined    -> sp[32]: arguments_list
    // -> sp[8]:  undefined   -> sp[16]: undefined    -> sp[24]: undefined
    //    sp[0]:  undefined      sp[8]:  undefined       sp[16]: undefined
    //                           sp[0]:  undefined       sp[8]:  undefined
    //                                                   sp[0]:  undefined
    //       argc = 3
    //    sp[56]: receiver
    // -> sp[48]: target
    // -> sp[40]: arguments_list
    // -> sp[32]: new_target
    //    sp[24]: undefined
    //    sp[16]: undefined
    //    sp[8]:  undefined
    //    sp[0]:  undefined
    //
    // There are now always three arguments to read, in the slots starting from
    // slot (argc + 1).
    Register scratch = x10;
    __ SlotAddress(scratch, argc);
    __ Ldp(new_target, arguments_list,
           MemOperand(scratch, 1 * kSystemPointerSize));
    __ Ldr(target, MemOperand(scratch, 3 * kSystemPointerSize));

    __ Cmp(argc, 2);
    __ CmovX(new_target, target, ls);  // target if argc <= 2.

    __ Drop(4);  // Drop the undefined values we pushed above.
    __ DropArguments(argc, TurboAssembler::kCountExcludesReceiver);

    // Push receiver (undefined).
    __ PushArgument(undefined_value);
  }

  // ----------- S t a t e -------------
  //  -- x2      : argumentsList
  //  -- x1      : target
  //  -- x3      : new.target
  //  -- sp[0]   : receiver (undefined)
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

namespace {

void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ Push(lr, fp);
  __ Mov(x11, StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR));
  __ Push(x11, x1);  // x1: function
  __ SmiTag(x11, x0);  // x0: number of arguments.
  __ Push(x11, padreg);
  __ Add(fp, sp, ArgumentsAdaptorFrameConstants::kFixedFrameSizeFromFp);
}

void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then drop the parameters and the receiver.
  __ Ldr(x10, MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ Mov(sp, fp);
  __ Pop(fp, lr);

  // Drop actual parameters and receiver.
  __ SmiUntag(x10);
  __ DropArguments(x10, TurboAssembler::kCountExcludesReceiver);
}

// Prepares the stack for copying the varargs. First we claim the necessary
// slots, taking care of potential padding. Then we copy the existing arguments
// one slot up or one slot down, as needed.
void Generate_PrepareForCopyingVarargs(MacroAssembler* masm, Register argc,
                                       Register len) {
  Label len_odd, exit;
  Register slots_to_copy = x10;  // If needed.
  __ Add(slots_to_copy, argc, 1);
  __ Add(argc, argc, len);
  __ Tbnz(len, 0, &len_odd);
  __ Claim(len);
  __ B(&exit);

  __ Bind(&len_odd);
  // Claim space we need. If argc is even, slots_to_claim = len + 1, as we need
  // one extra padding slot. If argc is odd, we know that the original arguments
  // will have a padding slot we can reuse (since len is odd), so
  // slots_to_claim = len - 1.
  {
    Register scratch = x11;
    Register slots_to_claim = x12;
    __ Add(slots_to_claim, len, 1);
    __ And(scratch, argc, 1);
    __ Sub(slots_to_claim, slots_to_claim, Operand(scratch, LSL, 1));
    __ Claim(slots_to_claim);
  }

  Label copy_down;
  __ Tbz(slots_to_copy, 0, &copy_down);

  // Copy existing arguments one slot up.
  {
    Register src = x11;
    Register dst = x12;
    Register scratch = x13;
    __ Sub(scratch, argc, 1);
    __ SlotAddress(src, scratch);
    __ SlotAddress(dst, argc);
    __ CopyDoubleWords(dst, src, slots_to_copy,
                       TurboAssembler::kSrcLessThanDst);
  }
  __ B(&exit);

  // Copy existing arguments one slot down and add padding.
  __ Bind(&copy_down);
  {
    Register src = x11;
    Register dst = x12;
    Register scratch = x13;
    __ Add(src, len, 1);
    __ Mov(dst, len);  // CopySlots will corrupt dst.
    __ CopySlots(dst, src, slots_to_copy);
    __ Add(scratch, argc, 1);
    __ Poke(padreg,
            Operand(scratch, LSL, kSystemPointerSizeLog2));  // Store padding.
  }

  __ Bind(&exit);
}

}  // namespace

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- x1 : target
  //  -- x0 : number of parameters on the stack (not including the receiver)
  //  -- x2 : arguments list (a FixedArray)
  //  -- x4 : len (number of elements to push from args)
  //  -- x3 : new.target (for [[Construct]])
  // -----------------------------------
  if (masm->emit_debug_code()) {
    // Allow x2 to be a FixedArray, or a FixedDoubleArray if x4 == 0.
    Label ok, fail;
    __ AssertNotSmi(x2, AbortReason::kOperandIsNotAFixedArray);
    __ LoadTaggedPointerField(x10, FieldMemOperand(x2, HeapObject::kMapOffset));
    __ Ldrh(x13, FieldMemOperand(x10, Map::kInstanceTypeOffset));
    __ Cmp(x13, FIXED_ARRAY_TYPE);
    __ B(eq, &ok);
    __ Cmp(x13, FIXED_DOUBLE_ARRAY_TYPE);
    __ B(ne, &fail);
    __ Cmp(x4, 0);
    __ B(eq, &ok);
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  Register arguments_list = x2;
  Register argc = x0;
  Register len = x4;

  Label stack_overflow;
  Generate_StackOverflowCheck(masm, len, &stack_overflow);

  // Skip argument setup if we don't need to push any varargs.
  Label done;
  __ Cbz(len, &done);

  Generate_PrepareForCopyingVarargs(masm, argc, len);

  // Push varargs.
  {
    Label loop;
    Register src = x10;
    Register the_hole_value = x11;
    Register undefined_value = x12;
    Register scratch = x13;
    __ Add(src, arguments_list, FixedArray::kHeaderSize - kHeapObjectTag);
    __ LoadRoot(the_hole_value, RootIndex::kTheHoleValue);
    __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);
    // We do not use the CompareRoot macro as it would do a LoadRoot behind the
    // scenes and we want to avoid that in a loop.
    // TODO(all): Consider using Ldp and Stp.
    __ Bind(&loop);
    __ Sub(len, len, 1);
    __ LoadAnyTaggedField(scratch, MemOperand(src, kTaggedSize, PostIndex));
    __ CmpTagged(scratch, the_hole_value);
    __ Csel(scratch, scratch, undefined_value, ne);
    __ Poke(scratch, Operand(len, LSL, kSystemPointerSizeLog2));
    __ Cbnz(len, &loop);
  }
  __ Bind(&done);

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
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x3 : the new.target (for [[Construct]] calls)
  //  -- x1 : the target to call (can be any Object)
  //  -- x2 : start index (to support rest parameters)
  // -----------------------------------

  Register argc = x0;
  Register start_index = x2;

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(x3, &new_target_not_constructor);
    __ LoadTaggedPointerField(x5, FieldMemOperand(x3, HeapObject::kMapOffset));
    __ Ldrb(x5, FieldMemOperand(x5, Map::kBitFieldOffset));
    __ TestAndBranchIfAnySet(x5, Map::Bits1::IsConstructorBit::kMask,
                             &new_target_constructor);
    __ Bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ PushArgument(x3);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ Bind(&new_target_constructor);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  // args_fp will point to the frame that contains the actual arguments, which
  // will be the current frame unless we have an arguments adaptor frame, in
  // which case args_fp points to the arguments adaptor frame.
  Register args_fp = x5;
  Register len = x6;
  {
    Label arguments_adaptor, arguments_done;
    Register scratch = x10;
    __ Ldr(args_fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ Ldr(x4, MemOperand(args_fp,
                          CommonFrameConstants::kContextOrFrameTypeOffset));
    __ CmpTagged(x4, StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR));
    __ B(eq, &arguments_adaptor);
    {
      __ Ldr(scratch, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
      __ LoadTaggedPointerField(
          scratch,
          FieldMemOperand(scratch, JSFunction::kSharedFunctionInfoOffset));
      __ Ldrh(len,
              FieldMemOperand(scratch,
                              SharedFunctionInfo::kFormalParameterCountOffset));
      __ Mov(args_fp, fp);
    }
    __ B(&arguments_done);
    __ Bind(&arguments_adaptor);
    {
      // Just load the length from ArgumentsAdaptorFrame.
      __ SmiUntag(
          len,
          MemOperand(args_fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
    }
    __ Bind(&arguments_done);
  }

  Label stack_done, stack_overflow;
  __ Subs(len, len, start_index);
  __ B(le, &stack_done);
  // Check for stack overflow.
  Generate_StackOverflowCheck(masm, x6, &stack_overflow);

  Generate_PrepareForCopyingVarargs(masm, argc, len);

  // Push varargs.
  {
    Register dst = x13;
    __ Add(args_fp, args_fp, 2 * kSystemPointerSize);
    __ SlotAddress(dst, 0);
    __ CopyDoubleWords(dst, args_fp, len);
  }
  __ B(&stack_done);

  __ Bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ Bind(&stack_done);

  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode) {
  ASM_LOCATION("Builtins::Generate_CallFunction");
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(x1);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that function is not a "classConstructor".
  Label class_constructor;
  __ LoadTaggedPointerField(
      x2, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(w3, FieldMemOperand(x2, SharedFunctionInfo::kFlagsOffset));
  __ TestAndBranchIfAnySet(w3, SharedFunctionInfo::IsClassConstructorBit::kMask,
                           &class_constructor);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ LoadTaggedPointerField(cp,
                            FieldMemOperand(x1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ TestAndBranchIfAnySet(w3,
                           SharedFunctionInfo::IsNativeBit::kMask |
                               SharedFunctionInfo::IsStrictBit::kMask,
                           &done_convert);
  {
    // ----------- S t a t e -------------
    //  -- x0 : the number of arguments (not including the receiver)
    //  -- x1 : the function to call (checked to be a JSFunction)
    //  -- x2 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(x3);
    } else {
      Label convert_to_object, convert_receiver;
      __ Peek(x3, Operand(x0, LSL, kXRegSizeLog2));
      __ JumpIfSmi(x3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CompareObjectType(x3, x4, x4, FIRST_JS_RECEIVER_TYPE);
      __ B(hs, &done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(x3, RootIndex::kUndefinedValue, &convert_global_proxy);
        __ JumpIfNotRoot(x3, RootIndex::kNullValue, &convert_to_object);
        __ Bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(x3);
        }
        __ B(&convert_receiver);
      }
      __ Bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(x0);
        __ Push(padreg, x0, x1, cp);
        __ Mov(x0, x3);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Mov(x3, x0);
        __ Pop(cp, x1, x0, padreg);
        __ SmiUntag(x0);
      }
      __ LoadTaggedPointerField(
          x2, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
      __ Bind(&convert_receiver);
    }
    __ Poke(x3, Operand(x0, LSL, kXRegSizeLog2));
  }
  __ Bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSFunction)
  //  -- x2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ Ldrh(x2,
          FieldMemOperand(x2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ InvokeFunctionCode(x1, no_reg, x2, x0, JUMP_FUNCTION);

  // The function is a "classConstructor", need to raise an exception.
  __ Bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ PushArgument(x1);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : target (checked to be a JSBoundFunction)
  //  -- x3 : new.target (only in case of [[Construct]])
  // -----------------------------------

  Register bound_argc = x4;
  Register bound_argv = x2;

  // Load [[BoundArguments]] into x2 and length of that into x4.
  Label no_bound_arguments;
  __ LoadTaggedPointerField(
      bound_argv, FieldMemOperand(x1, JSBoundFunction::kBoundArgumentsOffset));
  __ SmiUntagField(bound_argc,
                   FieldMemOperand(bound_argv, FixedArray::kLengthOffset));
  __ Cbz(bound_argc, &no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- x0 : the number of arguments (not including the receiver)
    //  -- x1 : target (checked to be a JSBoundFunction)
    //  -- x2 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- x3 : new.target (only in case of [[Construct]])
    //  -- x4 : the number of [[BoundArguments]]
    // -----------------------------------

    Register argc = x0;

    // Check for stack overflow.
    {
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      Label done;
      LoadRealStackLimit(masm, x10);
      // Make x10 the space we have left. The stack might already be overflowed
      // here which will cause x10 to become negative.
      __ Sub(x10, sp, x10);
      // Check if the arguments will overflow the stack.
      __ Cmp(x10, Operand(bound_argc, LSL, kSystemPointerSizeLog2));
      __ B(gt, &done);
      __ TailCallRuntime(Runtime::kThrowStackOverflow);
      __ Bind(&done);
    }

    // Check if we need padding.
    Label copy_args, copy_bound_args;
    Register total_argc = x15;
    Register slots_to_claim = x12;
    __ Add(total_argc, argc, bound_argc);
    __ Mov(slots_to_claim, bound_argc);
    __ Tbz(bound_argc, 0, &copy_args);

    // Load receiver before we start moving the arguments. We will only
    // need this in this path because the bound arguments are odd.
    Register receiver = x14;
    __ Peek(receiver, Operand(argc, LSL, kSystemPointerSizeLog2));

    // Claim space we need. If argc is even, slots_to_claim = bound_argc + 1,
    // as we need one extra padding slot. If argc is odd, we know that the
    // original arguments will have a padding slot we can reuse (since
    // bound_argc is odd), so slots_to_claim = bound_argc - 1.
    {
      Register scratch = x11;
      __ Add(slots_to_claim, bound_argc, 1);
      __ And(scratch, total_argc, 1);
      __ Sub(slots_to_claim, slots_to_claim, Operand(scratch, LSL, 1));
    }

    // Copy bound arguments.
    __ Bind(&copy_args);
    // Skip claim and copy of existing arguments in the special case where we
    // do not need to claim any slots (this will be the case when
    // bound_argc == 1 and the existing arguments have padding we can reuse).
    __ Cbz(slots_to_claim, &copy_bound_args);
    __ Claim(slots_to_claim);
    {
      Register count = x10;
      // Relocate arguments to a lower address.
      __ Mov(count, argc);
      __ CopySlots(0, slots_to_claim, count);

      __ Bind(&copy_bound_args);
      // Copy [[BoundArguments]] to the stack (below the arguments). The first
      // element of the array is copied to the highest address.
      {
        Label loop;
        Register counter = x10;
        Register scratch = x11;
        Register copy_to = x12;
        __ Add(bound_argv, bound_argv,
               FixedArray::kHeaderSize - kHeapObjectTag);
        __ SlotAddress(copy_to, argc);
        __ Add(argc, argc,
               bound_argc);  // Update argc to include bound arguments.
        __ Lsl(counter, bound_argc, kTaggedSizeLog2);
        __ Bind(&loop);
        __ Sub(counter, counter, kTaggedSize);
        __ LoadAnyTaggedField(scratch, MemOperand(bound_argv, counter));
        // Poke into claimed area of stack.
        __ Str(scratch, MemOperand(copy_to, kSystemPointerSize, PostIndex));
        __ Cbnz(counter, &loop);
      }

      {
        Label done;
        Register scratch = x10;
        __ Tbz(bound_argc, 0, &done);
        // Store receiver.
        __ Add(scratch, sp, Operand(total_argc, LSL, kSystemPointerSizeLog2));
        __ Str(receiver, MemOperand(scratch, kSystemPointerSize, PostIndex));
        __ Tbnz(total_argc, 0, &done);
        // Store padding.
        __ Str(padreg, MemOperand(scratch));
        __ Bind(&done);
      }
    }
  }
  __ Bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(x1);

  // Patch the receiver to [[BoundThis]].
  __ LoadAnyTaggedField(x10,
                        FieldMemOperand(x1, JSBoundFunction::kBoundThisOffset));
  __ Poke(x10, Operand(x0, LSL, kSystemPointerSizeLog2));

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ LoadTaggedPointerField(
      x1, FieldMemOperand(x1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_smi;
  __ JumpIfSmi(x1, &non_callable);
  __ Bind(&non_smi);
  __ CompareObjectType(x1, x4, x5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, eq);
  __ Cmp(x5, JS_BOUND_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  __ Ldrb(x4, FieldMemOperand(x4, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x4, Map::Bits1::IsCallableBit::kMask,
                             &non_callable);

  // Check if target is a proxy and call CallProxy external builtin
  __ Cmp(x5, JS_PROXY_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET, eq);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  // Overwrite the original receiver with the (original) target.
  __ Poke(x1, Operand(x0, LSL, kXRegSizeLog2));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, x1);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ PushArgument(x1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (checked to be a JSFunction)
  //  -- x3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(x1);
  __ AssertFunction(x1);

  // Calling convention for function specific ConstructStubs require
  // x2 to contain either an AllocationSite or undefined.
  __ LoadRoot(x2, RootIndex::kUndefinedValue);

  Label call_generic_stub;

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  __ LoadTaggedPointerField(
      x4, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(w4, FieldMemOperand(x4, SharedFunctionInfo::kFlagsOffset));
  __ TestAndBranchIfAllClear(
      w4, SharedFunctionInfo::ConstructAsBuiltinBit::kMask, &call_generic_stub);

  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET);

  __ bind(&call_generic_stub);
  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSBoundFunction)
  //  -- x3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(x1);
  __ AssertBoundFunction(x1);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label done;
    __ CmpTagged(x1, x3);
    __ B(ne, &done);
    __ LoadTaggedPointerField(
        x3, FieldMemOperand(x1, JSBoundFunction::kBoundTargetFunctionOffset));
    __ Bind(&done);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ LoadTaggedPointerField(
      x1, FieldMemOperand(x1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (can be any Object)
  //  -- x3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor, non_proxy;
  __ JumpIfSmi(x1, &non_constructor);

  // Check if target has a [[Construct]] internal method.
  __ LoadTaggedPointerField(x4, FieldMemOperand(x1, HeapObject::kMapOffset));
  __ Ldrb(x2, FieldMemOperand(x4, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x2, Map::Bits1::IsConstructorBit::kMask,
                             &non_constructor);

  // Dispatch based on instance type.
  __ CompareInstanceType(x4, x5, JS_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ Cmp(x5, JS_BOUND_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to proxies after checking whether they are constructors.
  __ Cmp(x5, JS_PROXY_TYPE);
  __ B(ne, &non_proxy);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
  {
    // Overwrite the original receiver with the (original) target.
    __ Poke(x1, Operand(x0, LSL, kXRegSizeLog2));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, x1);
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
  ASM_LOCATION("Builtins::Generate_ArgumentsAdaptorTrampoline");
  // ----------- S t a t e -------------
  //  -- x0 : actual number of arguments
  //  -- x1 : function (passed through to callee)
  //  -- x2 : expected number of arguments
  //  -- x3 : new target (passed through to callee)
  // -----------------------------------

  // The frame we are about to construct will look like:
  //
  //  slot      Adaptor frame
  //       +-----------------+--------------------------------
  //  -n-1 |    receiver     |                            ^
  //       |  (parameter 0)  |                            |
  //       |- - - - - - - - -|                            |
  //  -n   |                 |                          Caller
  //  ...  |       ...       |                       frame slots --> actual args
  //  -2   |  parameter n-1  |                            |
  //       |- - - - - - - - -|                            |
  //  -1   |   parameter n   |                            v
  //  -----+-----------------+--------------------------------
  //   0   |   return addr   |                            ^
  //       |- - - - - - - - -|                            |
  //   1   | saved frame ptr | <-- frame ptr              |
  //       |- - - - - - - - -|                            |
  //   2   |Frame Type Marker|                            |
  //       |- - - - - - - - -|                            |
  //   3   |    function     |                          Callee
  //       |- - - - - - - - -|                        frame slots
  //   4   |     num of      |                            |
  //       |   actual args   |                            |
  //       |- - - - - - - - -|                            |
  //   5   |     padding     |                            |
  //       |-----------------+----                        |
  //  [6]  |    [padding]    |   ^                        |
  //       |- - - - - - - - -|   |                        |
  // 6+pad |    receiver     |   |                        |
  //       |  (parameter 0)  |   |                        |
  //       |- - - - - - - - -|   |                        |
  // 7+pad |   parameter 1   |   |                        |
  //       |- - - - - - - - -| Frame slots ----> expected args
  // 8+pad |   parameter 2   |   |                        |
  //       |- - - - - - - - -|   |                        |
  //       |                 |   |                        |
  //  ...  |       ...       |   |                        |
  //       |   parameter m   |   |                        |
  //       |- - - - - - - - -|   |                        |
  //       |   [undefined]   |   |                        |
  //       |- - - - - - - - -|   |                        |
  //       |                 |   |                        |
  //       |       ...       |   |                        |
  //       |   [undefined]   |   v   <-- stack ptr        v
  //  -----+-----------------+---------------------------------
  //
  // There is an optional slot of padding above the receiver to ensure stack
  // alignment of the arguments.
  // If the number of expected arguments is larger than the number of actual
  // arguments, the remaining expected slots will be filled with undefined.

  Register argc_actual = x0;    // Excluding the receiver.
  Register argc_expected = x2;  // Excluding the receiver.
  Register function = x1;
  Register argc_actual_minus_expected = x5;

  Label create_adaptor_frame, dont_adapt_arguments, stack_overflow,
      adapt_arguments_in_place;

  __ Cmp(argc_expected, SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  __ B(eq, &dont_adapt_arguments);

  // When the difference between argc_actual and argc_expected is odd, we
  // create an arguments adaptor frame.
  __ Sub(argc_actual_minus_expected, argc_actual, argc_expected);
  __ Tbnz(argc_actual_minus_expected, 0, &create_adaptor_frame);

  // When the difference is even, check if we are allowed to adjust the
  // existing frame instead.
  __ LoadTaggedPointerField(
      x4, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(w4, FieldMemOperand(x4, SharedFunctionInfo::kFlagsOffset));
  __ TestAndBranchIfAnySet(
      w4, SharedFunctionInfo::IsSafeToSkipArgumentsAdaptorBit::kMask,
      &adapt_arguments_in_place);

  // -------------------------------------------
  // Create an arguments adaptor frame.
  // -------------------------------------------
  __ Bind(&create_adaptor_frame);
  {
    __ RecordComment("-- Adapt arguments --");
    EnterArgumentsAdaptorFrame(masm);

    Register copy_from = x10;
    Register copy_end = x11;
    Register copy_to = x12;
    Register argc_to_copy = x13;
    Register argc_unused_actual = x14;
    Register scratch1 = x15, scratch2 = x16;

    // We need slots for the expected arguments, with one extra slot for the
    // receiver.
    __ RecordComment("-- Stack check --");
    __ Add(scratch1, argc_expected, 1);
    Generate_StackOverflowCheck(masm, scratch1, &stack_overflow);

    // Round up number of slots to be even, to maintain stack alignment.
    __ RecordComment("-- Allocate callee frame slots --");
    __ Add(scratch1, scratch1, 1);
    __ Bic(scratch1, scratch1, 1);
    __ Claim(scratch1, kSystemPointerSize);

    __ Mov(copy_to, sp);

    // Preparing the expected arguments is done in four steps, the order of
    // which is chosen so we can use LDP/STP and avoid conditional branches as
    // much as possible.

    // (1) If we don't have enough arguments, fill the remaining expected
    // arguments with undefined, otherwise skip this step.
    Label enough_arguments;
    __ Subs(scratch1, argc_actual, argc_expected);
    __ Csel(argc_unused_actual, xzr, scratch1, lt);
    __ Csel(argc_to_copy, argc_expected, argc_actual, ge);
    __ B(ge, &enough_arguments);

    // Fill the remaining expected arguments with undefined.
    __ RecordComment("-- Fill slots with undefined --");
    __ Sub(copy_end, copy_to, Operand(scratch1, LSL, kSystemPointerSizeLog2));
    __ LoadRoot(scratch1, RootIndex::kUndefinedValue);

    Label fill;
    __ Bind(&fill);
    __ Stp(scratch1, scratch1,
           MemOperand(copy_to, 2 * kSystemPointerSize, PostIndex));
    // We might write one slot extra, but that is ok because we'll overwrite it
    // below.
    __ Cmp(copy_end, copy_to);
    __ B(hi, &fill);

    // Correct copy_to, for the case where we wrote one additional slot.
    __ Mov(copy_to, copy_end);

    __ Bind(&enough_arguments);
    // (2) Copy all of the actual arguments, or as many as we need.
    Label skip_copy;
    __ RecordComment("-- Copy actual arguments --");
    __ Cbz(argc_to_copy, &skip_copy);
    __ Add(copy_end, copy_to,
           Operand(argc_to_copy, LSL, kSystemPointerSizeLog2));
    __ Add(copy_from, fp, 2 * kSystemPointerSize);
    // Adjust for difference between actual and expected arguments.
    __ Add(copy_from, copy_from,
           Operand(argc_unused_actual, LSL, kSystemPointerSizeLog2));

    // Copy arguments. We use load/store pair instructions, so we might
    // overshoot by one slot, but since we copy the arguments starting from the
    // last one, if we do overshoot, the extra slot will be overwritten later by
    // the receiver.
    Label copy_2_by_2;
    __ Bind(&copy_2_by_2);
    __ Ldp(scratch1, scratch2,
           MemOperand(copy_from, 2 * kSystemPointerSize, PostIndex));
    __ Stp(scratch1, scratch2,
           MemOperand(copy_to, 2 * kSystemPointerSize, PostIndex));
    __ Cmp(copy_end, copy_to);
    __ B(hi, &copy_2_by_2);
    __ Bind(&skip_copy);

    // (3) Store padding, which might be overwritten by the receiver, if it is
    // not necessary.
    __ RecordComment("-- Store padding --");
    __ Str(padreg, MemOperand(fp, -5 * kSystemPointerSize));

    // (4) Store receiver. Calculate target address from the sp to avoid
    // checking for padding. Storing the receiver will overwrite either the
    // extra slot we copied with the actual arguments, if we did copy one, or
    // the padding we stored above.
    __ RecordComment("-- Store receiver --");
    __ Add(copy_from, fp, 2 * kSystemPointerSize);
    __ Ldr(scratch1,
           MemOperand(copy_from, argc_actual, LSL, kSystemPointerSizeLog2));
    __ Str(scratch1,
           MemOperand(sp, argc_expected, LSL, kSystemPointerSizeLog2));

    // Arguments have been adapted. Now call the entry point.
    __ RecordComment("-- Call entry point --");
    __ Mov(argc_actual, argc_expected);
    // x0 : expected number of arguments
    // x1 : function (passed through to callee)
    // x3 : new target (passed through to callee)
    static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
    __ LoadTaggedPointerField(
        x2, FieldMemOperand(function, JSFunction::kCodeOffset));
    __ CallCodeObject(x2);

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(
        masm->pc_offset());

    // Exit frame and return.
    LeaveArgumentsAdaptorFrame(masm);
    __ Ret();
  }

  // -----------------------------------------
  // Adapt arguments in the existing frame.
  // -----------------------------------------
  __ Bind(&adapt_arguments_in_place);
  {
    __ RecordComment("-- Update arguments in place --");
    // The callee cannot observe the actual arguments, so it's safe to just
    // pass the expected arguments by massaging the stack appropriately. See
    // http://bit.ly/v8-faster-calls-with-arguments-mismatch for details.
    Label under_application, over_application;
    __ Tbnz(argc_actual_minus_expected, kXSignBit, &under_application);

    __ Bind(&over_application);
    {
      // Remove superfluous arguments from the stack. The number of superflous
      // arguments is even.
      __ RecordComment("-- Over-application --");
      __ Mov(argc_actual, argc_expected);
      __ Drop(argc_actual_minus_expected);
      __ B(&dont_adapt_arguments);
    }

    __ Bind(&under_application);
    {
      // Fill remaining expected arguments with undefined values.
      __ RecordComment("-- Under-application --");
      Label fill;
      Register undef_value = x16;
      __ LoadRoot(undef_value, RootIndex::kUndefinedValue);
      __ Bind(&fill);
      __ Add(argc_actual, argc_actual, 2);
      __ Push(undef_value, undef_value);
      __ Cmp(argc_actual, argc_expected);
      __ B(lt, &fill);
      __ B(&dont_adapt_arguments);
    }
  }

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ Bind(&dont_adapt_arguments);
  {
    // Call the entry point without adapting the arguments.
    __ RecordComment("-- Call without adapting args --");
    static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
    __ LoadTaggedPointerField(
        x2, FieldMemOperand(function, JSFunction::kCodeOffset));
    __ JumpCodeObject(x2);
  }

  __ Bind(&stack_overflow);
  __ RecordComment("-- Stack overflow --");
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();
  }
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was put in w8 by the jump table trampoline.
  // Sign extend and convert to Smi for the runtime call.
  __ sxtw(kWasmCompileLazyFuncIndexRegister,
          kWasmCompileLazyFuncIndexRegister.W());
  __ SmiTag(kWasmCompileLazyFuncIndexRegister,
            kWasmCompileLazyFuncIndexRegister);
  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameScope scope(masm, StackFrame::WASM_COMPILE_LAZY);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    constexpr RegList gp_regs =
        Register::ListOf(x0, x1, x2, x3, x4, x5, x6, x7);
    constexpr RegList fp_regs =
        Register::ListOf(d0, d1, d2, d3, d4, d5, d6, d7);
    __ PushXRegList(gp_regs);
    __ PushDRegList(fp_regs);

    // Pass instance and function index as explicit arguments to the runtime
    // function.
    __ Push(kWasmInstanceRegister, kWasmCompileLazyFuncIndexRegister);
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Mov(cp, Smi::zero());
    __ CallRuntime(Runtime::kWasmCompileLazy, 2);
    // The entrypoint address is the return value.
    __ mov(x8, kReturnRegister0);

    // Restore registers.
    __ PopDRegList(fp_regs);
    __ PopXRegList(gp_regs);
  }
  // Finally, jump to the entrypoint.
  __ Jump(x8);
}

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               SaveFPRegsMode save_doubles, ArgvMode argv_mode,
                               bool builtin_exit_frame) {
  // The Abort mechanism relies on CallRuntime, which in turn relies on
  // CEntry, so until this stub has been generated, we have to use a
  // fall-back Abort mechanism.
  //
  // Note that this stub must be generated before any use of Abort.
  HardAbortScope hard_aborts(masm);

  ASM_LOCATION("CEntry::Generate entry");

  // Register parameters:
  //    x0: argc (including receiver, untagged)
  //    x1: target
  // If argv_mode == kArgvInRegister:
  //    x11: argv (pointer to first argument)
  //
  // The stack on entry holds the arguments and the receiver, with the receiver
  // at the highest address:
  //
  //    sp]argc-1]: receiver
  //    sp[argc-2]: arg[argc-2]
  //    ...           ...
  //    sp[1]:      arg[1]
  //    sp[0]:      arg[0]
  //
  // The arguments are in reverse order, so that arg[argc-2] is actually the
  // first argument to the target function and arg[0] is the last.
  const Register& argc_input = x0;
  const Register& target_input = x1;

  // Calculate argv, argc and the target address, and store them in
  // callee-saved registers so we can retry the call without having to reload
  // these arguments.
  // TODO(jbramley): If the first call attempt succeeds in the common case (as
  // it should), then we might be better off putting these parameters directly
  // into their argument registers, rather than using callee-saved registers and
  // preserving them on the stack.
  const Register& argv = x21;
  const Register& argc = x22;
  const Register& target = x23;

  // Derive argv from the stack pointer so that it points to the first argument
  // (arg[argc-2]), or just below the receiver in case there are no arguments.
  //  - Adjust for the arg[] array.
  Register temp_argv = x11;
  if (argv_mode == kArgvOnStack) {
    __ SlotAddress(temp_argv, x0);
    //  - Adjust for the receiver.
    __ Sub(temp_argv, temp_argv, 1 * kSystemPointerSize);
  }

  // Reserve three slots to preserve x21-x23 callee-saved registers.
  int extra_stack_space = 3;
  // Enter the exit frame.
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(
      save_doubles == kSaveFPRegs, x10, extra_stack_space,
      builtin_exit_frame ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);

  // Poke callee-saved registers into reserved space.
  __ Poke(argv, 1 * kSystemPointerSize);
  __ Poke(argc, 2 * kSystemPointerSize);
  __ Poke(target, 3 * kSystemPointerSize);

  // We normally only keep tagged values in callee-saved registers, as they
  // could be pushed onto the stack by called stubs and functions, and on the
  // stack they can confuse the GC. However, we're only calling C functions
  // which can push arbitrary data onto the stack anyway, and so the GC won't
  // examine that part of the stack.
  __ Mov(argc, argc_input);
  __ Mov(target, target_input);
  __ Mov(argv, temp_argv);

  // x21 : argv
  // x22 : argc
  // x23 : call target
  //
  // The stack (on entry) holds the arguments and the receiver, with the
  // receiver at the highest address:
  //
  //         argv[8]:     receiver
  // argv -> argv[0]:     arg[argc-2]
  //         ...          ...
  //         argv[...]:   arg[1]
  //         argv[...]:   arg[0]
  //
  // Immediately below (after) this is the exit frame, as constructed by
  // EnterExitFrame:
  //         fp[8]:    CallerPC (lr)
  //   fp -> fp[0]:    CallerFP (old fp)
  //         fp[-8]:   Space reserved for SPOffset.
  //         fp[-16]:  CodeObject()
  //         sp[...]:  Saved doubles, if saved_doubles is true.
  //         sp[32]:   Alignment padding, if necessary.
  //         sp[24]:   Preserved x23 (used for target).
  //         sp[16]:   Preserved x22 (used for argc).
  //         sp[8]:    Preserved x21 (used for argv).
  //   sp -> sp[0]:    Space reserved for the return address.
  //
  // After a successful call, the exit frame, preserved registers (x21-x23) and
  // the arguments (including the receiver) are dropped or popped as
  // appropriate. The stub then returns.
  //
  // After an unsuccessful call, the exit frame and suchlike are left
  // untouched, and the stub either throws an exception by jumping to one of
  // the exception_returned label.

  // Prepare AAPCS64 arguments to pass to the builtin.
  __ Mov(x0, argc);
  __ Mov(x1, argv);
  __ Mov(x2, ExternalReference::isolate_address(masm->isolate()));

  __ StoreReturnAddressAndCall(target);

  // Result returned in x0 or x1:x0 - do not destroy these registers!

  //  x0    result0      The return code from the call.
  //  x1    result1      For calls which return ObjectPair.
  //  x21   argv
  //  x22   argc
  //  x23   target
  const Register& result = x0;

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(result, RootIndex::kException);
  __ B(eq, &exception_returned);

  // The call succeeded, so unwind the stack and return.

  // Restore callee-saved registers x21-x23.
  __ Mov(x11, argc);

  __ Peek(argv, 1 * kSystemPointerSize);
  __ Peek(argc, 2 * kSystemPointerSize);
  __ Peek(target, 3 * kSystemPointerSize);

  __ LeaveExitFrame(save_doubles == kSaveFPRegs, x10, x9);
  if (argv_mode == kArgvOnStack) {
    // Drop the remaining stack slots and return from the stub.
    __ DropArguments(x11);
  }
  __ AssertFPCRState();
  __ Ret();

  // Handling of exception.
  __ Bind(&exception_returned);

  ExternalReference pending_handler_context_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerContextAddress, masm->isolate());
  ExternalReference pending_handler_entrypoint_address =
      ExternalReference::Create(
          IsolateAddressId::kPendingHandlerEntrypointAddress, masm->isolate());
  ExternalReference pending_handler_fp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerFPAddress, masm->isolate());
  ExternalReference pending_handler_sp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerSPAddress, masm->isolate());

  // Ask the runtime for help to determine the handler. This will set x0 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler =
      ExternalReference::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ Mov(x0, 0);  // argc.
    __ Mov(x1, 0);  // argv.
    __ Mov(x2, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ Mov(cp, pending_handler_context_address);
  __ Ldr(cp, MemOperand(cp));
  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.AcquireX();
    __ Mov(scratch, pending_handler_sp_address);
    __ Ldr(scratch, MemOperand(scratch));
    __ Mov(sp, scratch);
  }
  __ Mov(fp, pending_handler_fp_address);
  __ Ldr(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label not_js_frame;
  __ Cbz(cp, &not_js_frame);
  __ Str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ Bind(&not_js_frame);

  // Reset the masking register. This is done independent of the underlying
  // feature flag {FLAG_untrusted_code_mitigations} to make the snapshot work
  // with both configurations. It is safe to always do this, because the
  // underlying register is caller-saved and can be arbitrarily clobbered.
  __ ResetSpeculationPoisonRegister();

  // Compute the handler entry address and jump to it.
  __ Mov(x10, pending_handler_entrypoint_address);
  __ Ldr(x10, MemOperand(x10));
  __ Br(x10);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label done;
  Register result = x7;

  DCHECK(result.Is64Bits());

  HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
  UseScratchRegisterScope temps(masm);
  Register scratch1 = temps.AcquireX();
  Register scratch2 = temps.AcquireX();
  DoubleRegister double_scratch = temps.AcquireD();

  // Account for saved regs.
  const int kArgumentOffset = 2 * kSystemPointerSize;

  __ Push(result, scratch1);  // scratch1 is also pushed to preserve alignment.
  __ Peek(double_scratch, kArgumentOffset);

  // Try to convert with a FPU convert instruction.  This handles all
  // non-saturating cases.
  __ TryConvertDoubleToInt64(result, double_scratch, &done);
  __ Fmov(result, double_scratch);

  // If we reach here we need to manually convert the input to an int32.

  // Extract the exponent.
  Register exponent = scratch1;
  __ Ubfx(exponent, result, HeapNumber::kMantissaBits,
          HeapNumber::kExponentBits);

  // It the exponent is >= 84 (kMantissaBits + 32), the result is always 0 since
  // the mantissa gets shifted completely out of the int32_t result.
  __ Cmp(exponent, HeapNumber::kExponentBias + HeapNumber::kMantissaBits + 32);
  __ CzeroX(result, ge);
  __ B(ge, &done);

  // The Fcvtzs sequence handles all cases except where the conversion causes
  // signed overflow in the int64_t target. Since we've already handled
  // exponents >= 84, we can guarantee that 63 <= exponent < 84.

  if (masm->emit_debug_code()) {
    __ Cmp(exponent, HeapNumber::kExponentBias + 63);
    // Exponents less than this should have been handled by the Fcvt case.
    __ Check(ge, AbortReason::kUnexpectedValue);
  }

  // Isolate the mantissa bits, and set the implicit '1'.
  Register mantissa = scratch2;
  __ Ubfx(mantissa, result, 0, HeapNumber::kMantissaBits);
  __ Orr(mantissa, mantissa, 1ULL << HeapNumber::kMantissaBits);

  // Negate the mantissa if necessary.
  __ Tst(result, kXSignMask);
  __ Cneg(mantissa, mantissa, ne);

  // Shift the mantissa bits in the correct place. We know that we have to shift
  // it left here, because exponent >= 63 >= kMantissaBits.
  __ Sub(exponent, exponent,
         HeapNumber::kExponentBias + HeapNumber::kMantissaBits);
  __ Lsl(result, mantissa, exponent);

  __ Bind(&done);
  __ Poke(result, kArgumentOffset);
  __ Pop(scratch1, result);
  __ Ret();
}

namespace {

// The number of register that CallApiFunctionAndReturn will need to save on
// the stack. The space for these registers need to be allocated in the
// ExitFrame before calling CallApiFunctionAndReturn.
constexpr int kCallApiFunctionSpillSpace = 4;

int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  return static_cast<int>(ref0.address() - ref1.address());
}

// Calls an API function. Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.
// 'stack_space' is the space to be unwound on exit (includes the call JS
// arguments space and the additional space allocated for the fast call).
// 'spill_offset' is the offset from the stack pointer where
// CallApiFunctionAndReturn can spill registers.
void CallApiFunctionAndReturn(MacroAssembler* masm, Register function_address,
                              ExternalReference thunk_ref, int stack_space,
                              MemOperand* stack_space_operand, int spill_offset,
                              MemOperand return_value_operand) {
  ASM_LOCATION("CallApiFunctionAndReturn");
  Isolate* isolate = masm->isolate();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(isolate), next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(isolate), next_address);

  DCHECK(function_address == x1 || function_address == x2);

  Label profiler_enabled, end_profiler_check;
  __ Mov(x10, ExternalReference::is_profiling_address(isolate));
  __ Ldrb(w10, MemOperand(x10));
  __ Cbnz(w10, &profiler_enabled);
  __ Mov(x10, ExternalReference::address_of_runtime_stats_flag());
  __ Ldrsw(w10, MemOperand(x10));
  __ Cbnz(w10, &profiler_enabled);
  {
    // Call the api function directly.
    __ Mov(x3, function_address);
    __ B(&end_profiler_check);
  }
  __ Bind(&profiler_enabled);
  {
    // Additional parameter is the address of the actual callback.
    __ Mov(x3, thunk_ref);
  }
  __ Bind(&end_profiler_check);

  // Save the callee-save registers we are going to use.
  // TODO(all): Is this necessary? ARM doesn't do it.
  STATIC_ASSERT(kCallApiFunctionSpillSpace == 4);
  __ Poke(x19, (spill_offset + 0) * kXRegSize);
  __ Poke(x20, (spill_offset + 1) * kXRegSize);
  __ Poke(x21, (spill_offset + 2) * kXRegSize);
  __ Poke(x22, (spill_offset + 3) * kXRegSize);

  // Allocate HandleScope in callee-save registers.
  // We will need to restore the HandleScope after the call to the API function,
  // by allocating it in callee-save registers they will be preserved by C code.
  Register handle_scope_base = x22;
  Register next_address_reg = x19;
  Register limit_reg = x20;
  Register level_reg = w21;

  __ Mov(handle_scope_base, next_address);
  __ Ldr(next_address_reg, MemOperand(handle_scope_base, kNextOffset));
  __ Ldr(limit_reg, MemOperand(handle_scope_base, kLimitOffset));
  __ Ldr(level_reg, MemOperand(handle_scope_base, kLevelOffset));
  __ Add(level_reg, level_reg, 1);
  __ Str(level_reg, MemOperand(handle_scope_base, kLevelOffset));

  __ Mov(x10, x3);  // TODO(arm64): Load target into x10 directly.
  __ StoreReturnAddressAndCall(x10);

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // Load value from ReturnValue.
  __ Ldr(x0, return_value_operand);
  __ Bind(&return_value_loaded);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ Str(next_address_reg, MemOperand(handle_scope_base, kNextOffset));
  if (__ emit_debug_code()) {
    __ Ldr(w1, MemOperand(handle_scope_base, kLevelOffset));
    __ Cmp(w1, level_reg);
    __ Check(eq, AbortReason::kUnexpectedLevelAfterReturnFromApiCall);
  }
  __ Sub(level_reg, level_reg, 1);
  __ Str(level_reg, MemOperand(handle_scope_base, kLevelOffset));
  __ Ldr(x1, MemOperand(handle_scope_base, kLimitOffset));
  __ Cmp(limit_reg, x1);
  __ B(ne, &delete_allocated_handles);

  // Leave the API exit frame.
  __ Bind(&leave_exit_frame);
  // Restore callee-saved registers.
  __ Peek(x19, (spill_offset + 0) * kXRegSize);
  __ Peek(x20, (spill_offset + 1) * kXRegSize);
  __ Peek(x21, (spill_offset + 2) * kXRegSize);
  __ Peek(x22, (spill_offset + 3) * kXRegSize);

  if (stack_space_operand != nullptr) {
    DCHECK_EQ(stack_space, 0);
    // Load the number of stack slots to drop before LeaveExitFrame modifies sp.
    __ Ldr(x19, *stack_space_operand);
  }

  __ LeaveExitFrame(false, x1, x5);

  // Check if the function scheduled an exception.
  __ Mov(x5, ExternalReference::scheduled_exception_address(isolate));
  __ Ldr(x5, MemOperand(x5));
  __ JumpIfNotRoot(x5, RootIndex::kTheHoleValue, &promote_scheduled_exception);

  if (stack_space_operand == nullptr) {
    DCHECK_NE(stack_space, 0);
    __ DropSlots(stack_space);
  } else {
    DCHECK_EQ(stack_space, 0);
    __ DropArguments(x19);
  }

  __ Ret();

  // Re-throw by promoting a scheduled exception.
  __ Bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ Bind(&delete_allocated_handles);
  __ Str(limit_reg, MemOperand(handle_scope_base, kLimitOffset));
  // Save the return value in a callee-save register.
  Register saved_result = x19;
  __ Mov(saved_result, x0);
  __ Mov(x0, ExternalReference::isolate_address(isolate));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(), 1);
  __ Mov(x0, saved_result);
  __ B(&leave_exit_frame);
}

}  // namespace

void Builtins::Generate_CallApiCallback(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- cp                  : context
  //  -- x1                  : api function address
  //  -- x2                  : arguments count (not including the receiver)
  //  -- x3                  : call data
  //  -- x0                  : holder
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1) * 8]  : first argument
  //  -- sp[(argc + 0) * 8]  : receiver
  // -----------------------------------

  Register api_function_address = x1;
  Register argc = x2;
  Register call_data = x3;
  Register holder = x0;
  Register scratch = x4;

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
  __ Claim(FCA::kArgsLength, kSystemPointerSize);

  // kHolder.
  __ Str(holder, MemOperand(sp, 0 * kSystemPointerSize));

  // kIsolate.
  __ Mov(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ Str(scratch, MemOperand(sp, 1 * kSystemPointerSize));

  // kReturnValueDefaultValue and kReturnValue.
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ Str(scratch, MemOperand(sp, 2 * kSystemPointerSize));
  __ Str(scratch, MemOperand(sp, 3 * kSystemPointerSize));

  // kData.
  __ Str(call_data, MemOperand(sp, 4 * kSystemPointerSize));

  // kNewTarget.
  __ Str(scratch, MemOperand(sp, 5 * kSystemPointerSize));

  // Keep a pointer to kHolder (= implicit_args) in a scratch register.
  // We use it below to set up the FunctionCallbackInfo object.
  __ Mov(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space, since it's
  // not controlled by GC.
  static constexpr int kApiStackSpace = 4;
  static constexpr bool kDontSaveDoubles = false;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(kDontSaveDoubles, x10,
                    kApiStackSpace + kCallApiFunctionSpillSpace);

  // FunctionCallbackInfo::implicit_args_ (points at kHolder as set up above).
  // Arguments are after the return address (pushed by EnterExitFrame()).
  __ Str(scratch, MemOperand(sp, 1 * kSystemPointerSize));

  // FunctionCallbackInfo::values_ (points at the first varargs argument passed
  // on the stack).
  __ Add(scratch, scratch,
         Operand((FCA::kArgsLength - 1) * kSystemPointerSize));
  __ Add(scratch, scratch, Operand(argc, LSL, kSystemPointerSizeLog2));
  __ Str(scratch, MemOperand(sp, 2 * kSystemPointerSize));

  // FunctionCallbackInfo::length_.
  __ Str(argc, MemOperand(sp, 3 * kSystemPointerSize));

  // We also store the number of slots to drop from the stack after returning
  // from the API function here.
  // Note: Unlike on other architectures, this stores the number of slots to
  // drop, not the number of bytes. arm64 must always drop a slot count that is
  // a multiple of two, and related helper functions (DropArguments) expect a
  // register containing the slot count.
  __ Add(scratch, argc, Operand(FCA::kArgsLength + 1 /*receiver*/));
  __ Str(scratch, MemOperand(sp, 4 * kSystemPointerSize));

  // v8::InvocationCallback's argument.
  DCHECK(!AreAliased(x0, api_function_address));
  __ add(x0, sp, Operand(1 * kSystemPointerSize));

  ExternalReference thunk_ref = ExternalReference::invoke_function_callback();

  // The current frame needs to be aligned.
  DCHECK_EQ(FCA::kArgsLength % 2, 0);

  // There are two stack slots above the arguments we constructed on the stack.
  // TODO(jgruber): Document what these arguments are.
  static constexpr int kStackSlotsAboveFCA = 2;
  MemOperand return_value_operand(
      fp, (kStackSlotsAboveFCA + FCA::kReturnValueOffset) * kSystemPointerSize);

  static constexpr int kSpillOffset = 1 + kApiStackSpace;
  static constexpr int kUseStackSpaceOperand = 0;
  MemOperand stack_space_operand(sp, 4 * kSystemPointerSize);

  AllowExternalCallThatCantCauseGC scope(masm);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kUseStackSpaceOperand, &stack_space_operand,
                           kSpillOffset, return_value_operand);
}

void Builtins::Generate_CallApiGetter(MacroAssembler* masm) {
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
  Register data = x4;
  Register undef = x5;
  Register isolate_address = x6;
  Register name = x7;
  DCHECK(!AreAliased(receiver, holder, callback, data, undef, isolate_address,
                     name));

  __ LoadAnyTaggedField(data,
                        FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ LoadRoot(undef, RootIndex::kUndefinedValue);
  __ Mov(isolate_address, ExternalReference::isolate_address(masm->isolate()));
  __ LoadTaggedPointerField(
      name, FieldMemOperand(callback, AccessorInfo::kNameOffset));

  // PropertyCallbackArguments:
  //   receiver, data, return value, return value default, isolate, holder,
  //   should_throw_on_error
  // These are followed by the property name, which is also pushed below the
  // exit frame to make the GC aware of it.
  __ Push(receiver, data, undef, undef, isolate_address, holder, xzr, name);

  // v8::PropertyCallbackInfo::args_ array and name handle.
  static const int kStackUnwindSpace =
      PropertyCallbackArguments::kArgsLength + 1;
  static_assert(kStackUnwindSpace % 2 == 0,
                "slots must be a multiple of 2 for stack pointer alignment");

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ Mov(x0, sp);                          // x0 = Handle<Name>
  __ Add(x1, x0, 1 * kSystemPointerSize);  // x1 = v8::PCI::args_

  const int kApiStackSpace = 1;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, x10, kApiStackSpace + kCallApiFunctionSpillSpace);

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ Poke(x1, 1 * kSystemPointerSize);
  __ SlotAddress(x1, 1);
  // x1 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback();

  Register api_function_address = x2;
  Register js_getter = x4;
  __ LoadTaggedPointerField(
      js_getter, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ Ldr(api_function_address,
         FieldMemOperand(js_getter, Foreign::kForeignAddressOffset));

  const int spill_offset = 1 + kApiStackSpace;
  // +3 is to skip prolog, return address and name handle.
  MemOperand return_value_operand(
      fp,
      (PropertyCallbackArguments::kReturnValueOffset + 3) * kSystemPointerSize);
  MemOperand* const kUseStackSpaceConstant = nullptr;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kStackUnwindSpace, kUseStackSpaceConstant,
                           spill_offset, return_value_operand);
}

void Builtins::Generate_DirectCEntry(MacroAssembler* masm) {
  // The sole purpose of DirectCEntry is for movable callers (e.g. any general
  // purpose Code object) to be able to call into C functions that may trigger
  // GC and thus move the caller.
  //
  // DirectCEntry places the return address on the stack (updated by the GC),
  // making the call GC safe. The irregexp backend relies on this.

  __ Poke(lr, 0);  // Store the return address.
  __ Blr(x10);     // Call the C++ function.
  __ Peek(lr, 0);  // Return to calling code.
  __ AssertFPCRState();
  __ Ret();
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

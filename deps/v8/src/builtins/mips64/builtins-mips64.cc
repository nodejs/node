// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS64

#include "src/api/api-arguments.h"
#include "src/codegen/code-factory.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"
#include "src/logging/counters.h"
// For interpreter_entry_return_pc_offset. TODO(jkummerow): Drop.
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/mips64/constants-mips64.h"
#include "src/codegen/register-configuration.h"
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
  //  -- a1 : target function (preserved for callee)
  //  -- a3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the function onto the stack.
    // Push a copy of the target function and the new target.
    __ Push(a1, a3, a1);

    __ CallRuntime(function_id, 1);
    // Restore target function and new target.
    __ Pop(a1, a3);
  }

  static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
  __ Daddu(a2, v0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(a2);
}

namespace {

enum StackLimitKind { kInterruptStackLimit, kRealStackLimit };

void LoadStackLimit(MacroAssembler* masm, Register destination,
                    StackLimitKind kind) {
  DCHECK(masm->root_array_available());
  Isolate* isolate = masm->isolate();
  ExternalReference limit =
      kind == StackLimitKind::kRealStackLimit
          ? ExternalReference::address_of_real_jslimit(isolate)
          : ExternalReference::address_of_jslimit(isolate);
  DCHECK(TurboAssembler::IsAddressableThroughRootRegister(isolate, limit));

  intptr_t offset =
      TurboAssembler::RootRegisterOffsetForExternalReference(isolate, limit);
  CHECK(is_int32(offset));
  __ Ld(destination, MemOperand(kRootRegister, static_cast<int32_t>(offset)));
}

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
    __ PushRoot(RootIndex::kTheHoleValue);

    // Set up pointer to last argument.
    __ Daddu(t2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

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
    __ Dlsa(t0, t2, t3, kPointerSizeLog2);
    __ Ld(t1, MemOperand(t0));
    __ push(t1);
    __ bind(&entry);
    __ Daddu(t3, t3, Operand(-1));
    __ Branch(&loop, greater_equal, t3, Operand(zero_reg));

    // Call the function.
    // a0: number of arguments (untagged)
    // a1: constructor function
    // a3: new target
    __ InvokeFunctionWithNewTarget(a1, a3, a0, CALL_FUNCTION);

    // Restore context from the frame.
    __ Ld(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ Ld(a1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ SmiScale(a4, a1, kPointerSizeLog2);
  __ Daddu(sp, sp, a4);
  __ Daddu(sp, sp, kPointerSize);
  __ Ret();
}

static void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                        Register scratch1, Register scratch2,
                                        Label* stack_overflow) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  LoadStackLimit(masm, scratch1, StackLimitKind::kRealStackLimit);
  // Make scratch1 the space we have left. The stack might already be overflowed
  // here which will cause scratch1 to become negative.
  __ dsubu(scratch1, sp, scratch1);
  // Check if the arguments will overflow the stack.
  __ dsll(scratch2, num_args, kPointerSizeLog2);
  // Signed comparison.
  __ Branch(stack_overflow, le, scratch1, Operand(scratch2));
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
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ SmiTag(a0);
    __ Push(cp, a0, a1);
    __ PushRoot(RootIndex::kTheHoleValue);
    __ Push(a3);

    // ----------- S t a t e -------------
    //  --        sp[0*kPointerSize]: new target
    //  --        sp[1*kPointerSize]: padding
    //  -- a1 and sp[2*kPointerSize]: constructor function
    //  --        sp[3*kPointerSize]: number of arguments (tagged)
    //  --        sp[4*kPointerSize]: context
    // -----------------------------------

    __ Ld(t2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lwu(t2, FieldMemOperand(t2, SharedFunctionInfo::kFlagsOffset));
    __ DecodeField<SharedFunctionInfo::FunctionKindBits>(t2);
    __ JumpIfIsInRange(t2, kDefaultDerivedConstructor, kDerivedConstructor,
                       &not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        t2, t3);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ Branch(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(v0, RootIndex::kTheHoleValue);

    // ----------- S t a t e -------------
    //  --                          v0: receiver
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
    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(v0, v0);

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

    // Set up pointer to last argument.
    __ Daddu(t2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, a0, t0, t1, &stack_overflow);
    __ Branch(&enough_stack_space);

    __ bind(&stack_overflow);
    // Restore the context from the frame.
    __ Ld(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ break_(0xCC);

    __ bind(&enough_stack_space);

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
    //  --        sp[2*kPointerSize]: padding
    //  -- a1 and sp[3*kPointerSize]: constructor function
    //  --        sp[4*kPointerSize]: number of arguments (tagged)
    //  --        sp[5*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry);
    __ bind(&loop);
    __ Dlsa(t0, t2, t3, kPointerSizeLog2);
    __ Ld(t1, MemOperand(t0));
    __ push(t1);
    __ bind(&entry);
    __ Daddu(t3, t3, Operand(-1));
    __ Branch(&loop, greater_equal, t3, Operand(zero_reg));

    // Call the function.
    __ InvokeFunctionWithNewTarget(a1, a3, a0, CALL_FUNCTION);

    // ----------- S t a t e -------------
    //  --                 v0: constructor result
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
    Label use_receiver, do_throw, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(v0, RootIndex::kUndefinedValue, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(v0, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    __ GetObjectType(v0, t2, t2);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ Branch(&leave_frame, greater_equal, t2, Operand(FIRST_JS_RECEIVER_TYPE));
    __ Branch(&use_receiver);

    __ bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ Ld(v0, MemOperand(sp, 0 * kPointerSize));
    __ JumpIfRoot(v0, RootIndex::kTheHoleValue, &do_throw);

    __ bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ Ld(a1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  __ SmiScale(a4, a1, kPointerSizeLog2);
  __ Daddu(sp, sp, a4);
  __ Daddu(sp, sp, kPointerSize);
  __ Ret();
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
  //  -- v0 : the value to pass to the generator
  //  -- a1 : the JSGeneratorObject to resume
  //  -- ra : return address
  // -----------------------------------
  __ AssertGeneratorObject(a1);

  // Store input value into generator object.
  __ Sd(v0, FieldMemOperand(a1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(a1, JSGeneratorObject::kInputOrDebugPosOffset, v0, a3,
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
  LoadStackLimit(masm, kScratchReg, StackLimitKind::kRealStackLimit);
  __ Branch(&stack_overflow, lo, sp, Operand(kScratchReg));

  // Push receiver.
  __ Ld(a5, FieldMemOperand(a1, JSGeneratorObject::kReceiverOffset));
  __ Push(a5);

  // ----------- S t a t e -------------
  //  -- a1    : the JSGeneratorObject to resume
  //  -- a4    : generator function
  //  -- cp    : generator context
  //  -- ra    : return address
  //  -- sp[0] : generator receiver
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
    __ Move(t2, zero_reg);
    __ bind(&loop);
    __ Dsubu(a3, a3, Operand(1));
    __ Branch(&done_loop, lt, a3, Operand(zero_reg));
    __ Dlsa(kScratchReg, t1, t2, kPointerSizeLog2);
    __ Ld(kScratchReg, FieldMemOperand(kScratchReg, FixedArray::kHeaderSize));
    __ Push(kScratchReg);
    __ Daddu(t2, t2, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
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
    __ Daddu(a2, a2, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(a2);
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
  __ Branch(USE_DELAY_SLOT, &stepping_prepared);
  __ Ld(a4, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(a1);
  }
  __ Branch(USE_DELAY_SLOT, &stepping_prepared);
  __ Ld(a4, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));

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
  LoadStackLimit(masm, scratch1, StackLimitKind::kRealStackLimit);
  // Make a2 the space we have left. The stack might already be overflowed
  // here which will cause r2 to become negative.
  __ dsubu(scratch1, sp, scratch1);
  // Check if the arguments will overflow the stack.
  __ dsll(scratch2, argc, kPointerSizeLog2);
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
    //
    // Stack:
    // 0 arg slots on mips64 (4 args slots on mips)

    // Save callee saved registers on the stack.
    __ MultiPush(kCalleeSaved | ra.bit());

    // Save callee-saved FPU registers.
    __ MultiPushFPU(kCalleeSavedFPU);
    // Set up the reserved register for 0.0.
    __ Move(kDoubleRegZero, 0.0);

    // Initialize the root register.
    // C calling convention. The first argument is passed in a0.
    __ mov(kRootRegister, a0);
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
  __ daddiu(fp, sp, -EntryFrameConstants::kCallerFPOffset);

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
  __ b(&cont);
  __ nop();  // Branch delay slot nop.
  __ bind(&non_outermost_js);
  __ li(s3, Operand(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);
  __ push(s3);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
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
  __ Sd(v0, MemOperand(s1));  // We come back from 'invoke'. result is in v0.
  __ LoadRoot(v0, RootIndex::kException);
  __ b(&exit);  // b exposes branch delay slot.
  __ nop();     // Branch delay slot nop.

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

  __ bind(&exit);  // v0 holds result
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
  __ daddiu(sp, sp, -EntryFrameConstants::kCallerFPOffset);

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

    // Push the function and the receiver onto the stack.
    __ Push(a2, a3);

    // Check if we have enough stack space to push all arguments.
    // Clobbers a0 and a3.
    Generate_CheckStackOverflow(masm, a4, a0, a3);

    // Setup new.target, function and argc.
    __ mov(a3, a1);
    __ mov(a1, a2);
    __ mov(a0, a4);

    // a0: argc
    // a1: function
    // a3: new.target
    // a5: argv

    // Copy arguments to the stack in a loop.
    // a3: argc
    // a5: argv, i.e. points to first arg
    Label loop, entry;
    __ Dlsa(s1, a5, a4, kPointerSizeLog2);
    __ b(&entry);
    __ nop();  // Branch delay slot nop.
    // s1 points past last arg.
    __ bind(&loop);
    __ Ld(s2, MemOperand(a5));  // Read next parameter.
    __ daddiu(a5, a5, kPointerSize);
    __ Ld(s2, MemOperand(s2));  // Dereference handle.
    __ push(s2);                // Push parameter.
    __ bind(&entry);
    __ Branch(&loop, ne, a5, Operand(s1));

    // a0: argc
    // a1: function
    // a3: new.target

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(a4, RootIndex::kUndefinedValue);
    __ mov(a5, a4);
    __ mov(s1, a4);
    __ mov(s2, a4);
    __ mov(s3, a4);
    __ mov(s4, a4);
    __ mov(s5, a4);
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
  __ mov(RunMicrotasksDescriptor::MicrotaskQueueRegister(), a1);
  __ Jump(BUILTIN_CODE(masm->isolate(), RunMicrotasks), RelocInfo::CODE_TARGET);
}

static void ReplaceClosureCodeWithOptimizedCode(MacroAssembler* masm,
                                                Register optimized_code,
                                                Register closure,
                                                Register scratch1,
                                                Register scratch2) {
  // Store code entry in the closure.
  __ Sd(optimized_code, FieldMemOperand(closure, JSFunction::kCodeOffset));
  __ mov(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kRAHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_count = scratch;

  // Get the arguments + receiver count.
  __ Ld(args_count,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Lw(t0, FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  // Drop receiver + arguments.
  __ Daddu(sp, sp, args_count);
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

static void TailCallOptimizedCodeSlot(MacroAssembler* masm,
                                      Register optimized_code_entry,
                                      Register scratch1, Register scratch2) {
  // ----------- S t a t e -------------
  //  -- a3 : new target (preserved for callee if needed, and caller)
  //  -- a1 : target function (preserved for callee if needed, and caller)
  // -----------------------------------
  DCHECK(!AreAliased(optimized_code_entry, a1, a3, scratch1, scratch2));

  Register closure = a1;

  // Check if the optimized code is marked for deopt. If it is, call the
  // runtime to clear it.
  Label found_deoptimized_code;
  __ Ld(a5,
        FieldMemOperand(optimized_code_entry, Code::kCodeDataContainerOffset));
  __ Lw(a5, FieldMemOperand(a5, CodeDataContainer::kKindSpecificFlagsOffset));
  __ And(a5, a5, Operand(1 << Code::kMarkedForDeoptimizationBit));
  __ Branch(&found_deoptimized_code, ne, a5, Operand(zero_reg));

  // Optimized code is good, get it into the closure and link the closure into
  // the optimized functions list, then tail call the optimized code.
  // The feedback vector is no longer used, so re-use it as a scratch
  // register.
  ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                      scratch1, scratch2);

  static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
  __ Daddu(a2, optimized_code_entry,
           Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(a2);

  // Optimized code slot contains deoptimized code, evict it and re-enter the
  // closure's code.
  __ bind(&found_deoptimized_code);
  GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
}

static void MaybeOptimizeCode(MacroAssembler* masm, Register feedback_vector,
                              Register optimization_marker) {
  // ----------- S t a t e -------------
  //  -- a3 : new target (preserved for callee if needed, and caller)
  //  -- a1 : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  //  -- optimization_marker : a Smi containing a non-zero optimization marker.
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

  // Otherwise, the marker is InOptimizationQueue, so fall through hoping
  // that an interrupt will eventually update the slot with optimized code.
  if (FLAG_debug_code) {
    __ Assert(eq, AbortReason::kExpectedOptimizationSentinel,
              optimization_marker,
              Operand(Smi::FromEnum(OptimizationMarker::kInOptimizationQueue)));
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
  __ Branch(&process_bytecode, hi, bytecode, Operand(3));
  __ And(scratch2, bytecode, Operand(1));
  __ Branch(&extra_wide, ne, scratch2, Operand(zero_reg));

  // Load the next bytecode and update table to the wide scaled table.
  __ Daddu(bytecode_offset, bytecode_offset, Operand(1));
  __ Daddu(scratch2, bytecode_array, bytecode_offset);
  __ Lbu(bytecode, MemOperand(scratch2));
  __ Daddu(bytecode_size_table, bytecode_size_table,
           Operand(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&process_bytecode);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ Daddu(bytecode_offset, bytecode_offset, Operand(1));
  __ Daddu(scratch2, bytecode_array, bytecode_offset);
  __ Lbu(bytecode, MemOperand(scratch2));
  __ Daddu(bytecode_size_table, bytecode_size_table,
           Operand(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));

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
  __ jmp(&end);

  __ bind(&not_jump_loop);
  // Otherwise, load the size of the current bytecode and advance the offset.
  __ Dlsa(scratch2, bytecode_size_table, bytecode, 2);
  __ Lw(scratch2, MemOperand(scratch2));
  __ Daddu(bytecode_offset, bytecode_offset, scratch2);

  __ bind(&end);
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
  Register closure = a1;
  Register feedback_vector = a2;

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  __ Ld(a0, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ Ld(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(a0, SharedFunctionInfo::kFunctionDataOffset));
  GetSharedFunctionInfoBytecode(masm, kInterpreterBytecodeArrayRegister, a4);

  // The bytecode array could have been flushed from the shared function info,
  // if so, call into CompileLazy.
  Label compile_lazy;
  __ GetObjectType(kInterpreterBytecodeArrayRegister, a0, a0);
  __ Branch(&compile_lazy, ne, a0, Operand(BYTECODE_ARRAY_TYPE));

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

  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  Register optimized_code_entry = a4;
  __ Ld(optimized_code_entry,
        FieldMemOperand(feedback_vector,
                        FeedbackVector::kOptimizedCodeWeakOrSmiOffset));

  // Check if the optimized code slot is not empty.
  Label optimized_code_slot_not_empty;

  __ Branch(&optimized_code_slot_not_empty, ne, optimized_code_entry,
            Operand(Smi::FromEnum(OptimizationMarker::kNone)));

  Label not_optimized;
  __ bind(&not_optimized);

  // Increment invocation count for the function.
  __ Lw(a4, FieldMemOperand(feedback_vector,
                            FeedbackVector::kInvocationCountOffset));
  __ Addu(a4, a4, Operand(1));
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
  __ sh(zero_reg, FieldMemOperand(kInterpreterBytecodeArrayRegister,
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
    __ Dsubu(a5, sp, Operand(a4));
    LoadStackLimit(masm, a2, StackLimitKind::kRealStackLimit);
    __ Branch(&stack_overflow, lo, a5, Operand(a2));

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
    __ Dsubu(a4, a4, Operand(kPointerSize));
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
  __ Dlsa(a5, fp, a5, kPointerSizeLog2);
  __ Sd(a3, MemOperand(a5));
  __ bind(&no_incoming_new_target_or_generator_register);

  // Perform interrupt stack check.
  // TODO(solanes): Merge with the real stack limit check above.
  Label stack_check_interrupt, after_stack_check_interrupt;
  LoadStackLimit(masm, a5, StackLimitKind::kInterruptStackLimit);
  __ Branch(&stack_check_interrupt, lo, sp, Operand(a5));
  __ bind(&after_stack_check_interrupt);

  // Load accumulator as undefined.
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ li(kInterpreterDispatchTableRegister,
        ExternalReference::interpreter_dispatch_table_address(masm->isolate()));
  __ Daddu(a0, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ Lbu(a7, MemOperand(a0));
  __ Dlsa(kScratchReg, kInterpreterDispatchTableRegister, a7, kPointerSizeLog2);
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
  __ Daddu(a1, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ Lbu(a1, MemOperand(a1));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, a1, a2, a3,
                                a4, &do_return);
  __ jmp(&do_dispatch);

  __ bind(&do_return);
  // The return value is in v0.
  LeaveInterpreterFrame(masm, t0);
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

  __ jmp(&after_stack_check_interrupt);

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
  TailCallOptimizedCodeSlot(masm, optimized_code_entry, t3, a5);

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
                                         Register num_args, Register index,
                                         Register scratch, Register scratch2) {
  // Find the address of the last argument.
  __ mov(scratch2, num_args);
  __ dsll(scratch2, scratch2, kPointerSizeLog2);
  __ Dsubu(scratch2, index, Operand(scratch2));

  // Push the arguments.
  Label loop_header, loop_check;
  __ Branch(&loop_check);
  __ bind(&loop_header);
  __ Ld(scratch, MemOperand(index));
  __ Daddu(index, index, Operand(-kPointerSize));
  __ push(scratch);
  __ bind(&loop_check);
  __ Branch(&loop_header, hi, index, Operand(scratch2));
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

  __ Daddu(a3, a0, Operand(1));  // Add one for receiver.

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(RootIndex::kUndefinedValue);
    __ Dsubu(a3, a3, Operand(1));  // Subtract one for receiver.
  }

  Generate_StackOverflowCheck(masm, a3, a4, t0, &stack_overflow);

  // This function modifies a2, t0 and a4.
  Generate_InterpreterPushArgs(masm, a3, a2, a4, t0);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(a2);                   // Pass the spread in a register
    __ Dsubu(a0, a0, Operand(1));  // Subtract one for spread
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

  // Push a slot for the receiver.
  __ push(zero_reg);

  Generate_StackOverflowCheck(masm, a0, a5, t0, &stack_overflow);

  // This function modifies t0, a4 and a5.
  Generate_InterpreterPushArgs(masm, a0, a4, a5, t0);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(a2);                   // Pass the spread in a register
    __ Dsubu(a0, a0, Operand(1));  // Subtract one for spread
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
  __ Daddu(t0, t0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Branch(&trampoline_loaded);

  __ bind(&builtin_trampoline);
  __ li(t0, ExternalReference::
                address_of_interpreter_entry_trampoline_instruction_start(
                    masm->isolate()));
  __ Ld(t0, MemOperand(t0));

  __ bind(&trampoline_loaded);
  __ Daddu(ra, t0, Operand(interpreter_entry_return_pc_offset.value()));

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
  __ Daddu(a1, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ Lbu(a7, MemOperand(a1));
  __ Dlsa(a1, kInterpreterDispatchTableRegister, a7, kPointerSizeLog2);
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
  __ Daddu(a1, kInterpreterBytecodeArrayRegister,
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
  if (with_result) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point.
    __ Sd(v0,
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
  __ Ld(fp, MemOperand(
                sp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  // Load builtin index (stored as a Smi) and use it to get the builtin start
  // address from the builtins table.
  __ Pop(t0);
  __ Daddu(sp, sp,
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

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), v0.code());
  __ Ld(v0, MemOperand(sp, 0 * kPointerSize));
  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Daddu(sp, sp, Operand(1 * kPointerSize));  // Remove state.
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  __ Ret(eq, v0, Operand(Smi::zero()));

  // Drop the handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  __ LeaveFrame(StackFrame::STUB);

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ Ld(a1, MemOperand(v0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ SmiUntag(a1, MemOperand(a1, FixedArray::OffsetOfElementAt(
                                     DeoptimizationData::kOsrPcOffsetIndex) -
                                     kHeapObjectTag));

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ Daddu(v0, v0, a1);
  __ daddiu(ra, v0, Code::kHeaderSize - kHeapObjectTag);

  // And "return" to the OSR entry point of the function.
  __ Ret();
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0    : argc
  //  -- sp[0] : argArray
  //  -- sp[4] : thisArg
  //  -- sp[8] : receiver
  // -----------------------------------

  Register argc = a0;
  Register arg_array = a2;
  Register receiver = a1;
  Register this_arg = a5;
  Register undefined_value = a3;
  Register scratch = a4;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load receiver into a1, argArray into a2 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    // Claim (2 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.

    __ Dsubu(sp, sp, Operand(2 * kPointerSize));
    __ Dlsa(sp, sp, argc, kPointerSizeLog2);
    __ mov(scratch, argc);
    __ Pop(this_arg, arg_array);                   // Overwrite argc
    __ Movz(arg_array, undefined_value, scratch);  // if argc == 0
    __ Movz(this_arg, undefined_value, scratch);   // if argc == 0
    __ Dsubu(scratch, scratch, Operand(1));
    __ Movz(arg_array, undefined_value, scratch);  // if argc == 1
    __ Ld(receiver, MemOperand(sp));
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
    __ mov(a0, zero_reg);
    DCHECK(receiver == a1);
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
    __ PushRoot(RootIndex::kUndefinedValue);
    __ Daddu(a0, a0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack.
  // a0: actual number of arguments
  __ Dlsa(kScratchReg, sp, a0, kPointerSizeLog2);
  __ Ld(a1, MemOperand(kScratchReg));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // a0: actual number of arguments
  // a1: function
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ Dlsa(a2, sp, a0, kPointerSizeLog2);

    __ bind(&loop);
    __ Ld(kScratchReg, MemOperand(a2, -kPointerSize));
    __ Sd(kScratchReg, MemOperand(a2));
    __ Dsubu(a2, a2, Operand(kPointerSize));
    __ Branch(&loop, ne, a2, Operand(sp));
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ Dsubu(a0, a0, Operand(1));
    __ Pop();
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]  : argumentsList  (if argc ==3)
  //  -- sp[4]  : thisArgument   (if argc >=2)
  //  -- sp[8]  : target         (if argc >=1)
  //  -- sp[12] : receiver
  // -----------------------------------

  Register argc = a0;
  Register arguments_list = a2;
  Register target = a1;
  Register this_argument = a5;
  Register undefined_value = a3;
  Register scratch = a4;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load target into a1 (if present), argumentsList into a2 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    // Claim (3 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.

    __ Dsubu(sp, sp, Operand(3 * kPointerSize));
    __ Dlsa(sp, sp, argc, kPointerSizeLog2);
    __ mov(scratch, argc);
    __ Pop(target, this_argument, arguments_list);
    __ Movz(arguments_list, undefined_value, scratch);  // if argc == 0
    __ Movz(this_argument, undefined_value, scratch);   // if argc == 0
    __ Movz(target, undefined_value, scratch);          // if argc == 0
    __ Dsubu(scratch, scratch, Operand(1));
    __ Movz(arguments_list, undefined_value, scratch);  // if argc == 1
    __ Movz(this_argument, undefined_value, scratch);   // if argc == 1
    __ Dsubu(scratch, scratch, Operand(1));
    __ Movz(arguments_list, undefined_value, scratch);  // if argc == 2

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
  //  -- sp[0]  : new.target (optional) (dummy value if argc <= 2)
  //  -- sp[4]  : argumentsList         (dummy value if argc <= 1)
  //  -- sp[8]  : target                (dummy value if argc == 0)
  //  -- sp[12] : receiver
  // -----------------------------------
  Register argc = a0;
  Register arguments_list = a2;
  Register target = a1;
  Register new_target = a3;
  Register undefined_value = a4;
  Register scratch = a5;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load target into a1 (if present), argumentsList into a2 (if present),
  // new.target into a3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    // Claim (3 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.

    __ Dsubu(sp, sp, Operand(3 * kPointerSize));
    __ Dlsa(sp, sp, argc, kPointerSizeLog2);
    __ mov(scratch, argc);
    __ Pop(target, arguments_list, new_target);
    __ Movz(arguments_list, undefined_value, scratch);  // if argc == 0
    __ Movz(new_target, undefined_value, scratch);      // if argc == 0
    __ Movz(target, undefined_value, scratch);          // if argc == 0
    __ Dsubu(scratch, scratch, Operand(1));
    __ Movz(arguments_list, undefined_value, scratch);  // if argc == 1
    __ Movz(new_target, target, scratch);               // if argc == 1
    __ Dsubu(scratch, scratch, Operand(1));
    __ Movz(new_target, target, scratch);  // if argc == 2

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

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ SmiTag(a0);
  __ li(a4, Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ MultiPush(a0.bit() | a1.bit() | a4.bit() | fp.bit() | ra.bit());
  __ Push(Smi::zero());  // Padding.
  __ Daddu(fp, sp,
           Operand(ArgumentsAdaptorFrameConstants::kFixedFrameSizeFromFp));
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ Ld(a1, MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ mov(sp, fp);
  __ MultiPop(fp.bit() | ra.bit());
  __ SmiScale(a4, a1, kPointerSizeLog2);
  __ Daddu(sp, sp, a4);
  // Adjust for the receiver.
  __ Daddu(sp, sp, Operand(kPointerSize));
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
    __ GetObjectType(a2, t8, t8);
    __ Branch(&ok, eq, t8, Operand(FIXED_ARRAY_TYPE));
    __ Branch(&fail, ne, t8, Operand(FIXED_DOUBLE_ARRAY_TYPE));
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
  Generate_StackOverflowCheck(masm, len, kScratchReg, a5, &stack_overflow);

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    Label done, push, loop;
    Register src = a6;
    Register scratch = len;

    __ daddiu(src, args, FixedArray::kHeaderSize - kHeapObjectTag);
    __ Branch(&done, eq, len, Operand(zero_reg), i::USE_DELAY_SLOT);
    __ Daddu(a0, a0, len);  // The 'len' argument for Call() or Construct().
    __ dsll(scratch, len, kPointerSizeLog2);
    __ Dsubu(scratch, sp, Operand(scratch));
    __ LoadRoot(t1, RootIndex::kTheHoleValue);
    __ bind(&loop);
    __ Ld(a5, MemOperand(src));
    __ Branch(&push, ne, a5, Operand(t1));
    __ LoadRoot(a5, RootIndex::kUndefinedValue);
    __ bind(&push);
    __ daddiu(src, src, kPointerSize);
    __ Push(a5);
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
    __ ld(t1, FieldMemOperand(a3, HeapObject::kMapOffset));
    __ lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
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

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ Ld(a6, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ Ld(a7, MemOperand(a6, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Branch(&arguments_adaptor, eq, a7,
            Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  {
    __ Ld(a7, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
    __ Ld(a7, FieldMemOperand(a7, JSFunction::kSharedFunctionInfoOffset));
    __ Lhu(a7, FieldMemOperand(
                   a7, SharedFunctionInfo::kFormalParameterCountOffset));
    __ mov(a6, fp);
  }
  __ Branch(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    // Just get the length from the ArgumentsAdaptorFrame.
    __ SmiUntag(a7,
                MemOperand(a6, ArgumentsAdaptorFrameConstants::kLengthOffset));
  }
  __ bind(&arguments_done);

  Label stack_done, stack_overflow;
  __ Subu(a7, a7, a2);
  __ Branch(&stack_done, le, a7, Operand(zero_reg));
  {
    // Check for stack overflow.
    Generate_StackOverflowCheck(masm, a7, a4, a5, &stack_overflow);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ Daddu(a0, a0, a7);
      __ bind(&loop);
      {
        __ Dlsa(kScratchReg, a6, a7, kPointerSizeLog2);
        __ Ld(kScratchReg, MemOperand(kScratchReg, 1 * kPointerSize));
        __ push(kScratchReg);
        __ Subu(a7, a7, Operand(1));
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
      __ Dlsa(kScratchReg, sp, a0, kPointerSizeLog2);
      __ Ld(a3, MemOperand(kScratchReg));
      __ JumpIfSmi(a3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ GetObjectType(a3, a4, a4);
      __ Branch(&done_convert, hs, a4, Operand(FIRST_JS_RECEIVER_TYPE));
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
        __ mov(a0, a3);
        __ Push(cp);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ mov(a3, v0);
        __ Pop(a0, a1);
        __ SmiUntag(a0);
      }
      __ Ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ Dlsa(kScratchReg, sp, a0, kPointerSizeLog2);
    __ Sd(a3, MemOperand(kScratchReg));
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
    __ Ld(kScratchReg, FieldMemOperand(a1, JSBoundFunction::kBoundThisOffset));
    __ Dlsa(a4, sp, a0, kPointerSizeLog2);
    __ Sd(kScratchReg, MemOperand(a4));
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
    __ dsll(a5, a4, kPointerSizeLog2);
    __ Dsubu(sp, sp, Operand(a5));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    LoadStackLimit(masm, kScratchReg, StackLimitKind::kRealStackLimit);
    __ Branch(&done, hs, sp, Operand(kScratchReg));
    // Restore the stack pointer.
    __ Daddu(sp, sp, Operand(a5));
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
    __ mov(a5, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, gt, a5, Operand(a0));
    __ Dlsa(a6, sp, a4, kPointerSizeLog2);
    __ Ld(kScratchReg, MemOperand(a6));
    __ Dlsa(a6, sp, a5, kPointerSizeLog2);
    __ Sd(kScratchReg, MemOperand(a6));
    __ Daddu(a4, a4, Operand(1));
    __ Daddu(a5, a5, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ SmiUntag(a4, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ Daddu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Dsubu(a4, a4, Operand(1));
    __ Branch(&done_loop, lt, a4, Operand(zero_reg));
    __ Dlsa(a5, a2, a4, kPointerSizeLog2);
    __ Ld(kScratchReg, MemOperand(a5));
    __ Dlsa(a5, sp, a0, kPointerSizeLog2);
    __ Sd(kScratchReg, MemOperand(a5));
    __ Daddu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

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
  __ GetObjectType(a1, t1, t2);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_BOUND_FUNCTION_TYPE));

  // Check if target has a [[Call]] internal method.
  __ Lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t1, t1, Operand(Map::Bits1::IsCallableBit::kMask));
  __ Branch(&non_callable, eq, t1, Operand(zero_reg));

  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_PROXY_TYPE));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  // Overwrite the original receiver with the (original) target.
  __ Dlsa(kScratchReg, sp, a0, kPointerSizeLog2);
  __ Sd(a1, MemOperand(kScratchReg));
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
  __ lwu(a4, FieldMemOperand(a4, SharedFunctionInfo::kFlagsOffset));
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
    __ dsll(a5, a4, kPointerSizeLog2);
    __ Dsubu(sp, sp, Operand(a5));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    LoadStackLimit(masm, kScratchReg, StackLimitKind::kRealStackLimit);
    __ Branch(&done, hs, sp, Operand(kScratchReg));
    // Restore the stack pointer.
    __ Daddu(sp, sp, Operand(a5));
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
    __ mov(a5, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, ge, a5, Operand(a0));
    __ Dlsa(a6, sp, a4, kPointerSizeLog2);
    __ Ld(kScratchReg, MemOperand(a6));
    __ Dlsa(a6, sp, a5, kPointerSizeLog2);
    __ Sd(kScratchReg, MemOperand(a6));
    __ Daddu(a4, a4, Operand(1));
    __ Daddu(a5, a5, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ SmiUntag(a4, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ Daddu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Dsubu(a4, a4, Operand(1));
    __ Branch(&done_loop, lt, a4, Operand(zero_reg));
    __ Dlsa(a5, a2, a4, kPointerSizeLog2);
    __ Ld(kScratchReg, MemOperand(a5));
    __ Dlsa(a5, sp, a0, kPointerSizeLog2);
    __ Sd(kScratchReg, MemOperand(a5));
    __ Daddu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

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
  __ ld(t1, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Lbu(t3, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t3, t3, Operand(Map::Bits1::IsConstructorBit::kMask));
  __ Branch(&non_constructor, eq, t3, Operand(zero_reg));

  // Dispatch based on instance type.
  __ Lhu(t2, FieldMemOperand(t1, Map::kInstanceTypeOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));

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
    __ Dlsa(kScratchReg, sp, a0, kPointerSizeLog2);
    __ Sd(a1, MemOperand(kScratchReg));
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
            Operand(kDontAdaptArgumentsSentinel));
  // We use Uless as the number of argument should always be greater than 0.
  __ Branch(&too_few, Uless, a0, Operand(a2));

  {  // Enough parameters: actual >= expected.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, a2, a5, kScratchReg, &stack_overflow);

    // Calculate copy start address into a0 and copy end address into a4.
    __ SmiScale(a0, a0, kPointerSizeLog2);
    __ Daddu(a0, fp, a0);
    // Adjust for return address and receiver.
    __ Daddu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address.
    __ dsll(a4, a2, kPointerSizeLog2);
    __ dsubu(a4, a0, a4);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    // a4: copy end address

    Label copy;
    __ bind(&copy);
    __ Ld(a5, MemOperand(a0));
    __ push(a5);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(a4));
    __ daddiu(a0, a0, -kPointerSize);  // In delay slot.

    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, a2, a5, kScratchReg, &stack_overflow);

    // Calculate copy start address into a0 and copy end address into a7.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ SmiScale(a0, a0, kPointerSizeLog2);
    __ Daddu(a0, fp, a0);
    // Adjust for return address and receiver.
    __ Daddu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address. Also adjust for return address.
    __ Daddu(a7, fp, kPointerSize);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    // a7: copy end address
    Label copy;
    __ bind(&copy);
    __ Ld(a4, MemOperand(a0));  // Adjusted above for return addr and receiver.
    __ Dsubu(sp, sp, kPointerSize);
    __ Dsubu(a0, a0, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(a7));
    __ Sd(a4, MemOperand(sp));  // In the delay slot.

    // Fill the remaining expected arguments with undefined.
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ LoadRoot(a5, RootIndex::kUndefinedValue);
    __ dsll(a6, a2, kPointerSizeLog2);
    __ Dsubu(a4, fp, Operand(a6));
    // Adjust for frame.
    __ Dsubu(a4, a4,
             Operand(ArgumentsAdaptorFrameConstants::kFixedFrameSizeFromFp +
                     kPointerSize));

    Label fill;
    __ bind(&fill);
    __ Dsubu(sp, sp, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &fill, ne, sp, Operand(a4));
    __ Sd(a5, MemOperand(sp));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mov(a0, a2);
  // a0 : expected number of arguments
  // a1 : function (passed through to callee)
  // a3: new target (passed through to callee)
  static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
  __ Ld(a2, FieldMemOperand(a1, JSFunction::kCodeOffset));
  __ Daddu(a2, a2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Call(a2);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // -------------------------------------------
  // Don't adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
  __ Ld(a2, FieldMemOperand(a1, JSFunction::kCodeOffset));
  __ Daddu(a2, a2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(a2);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ break_(0xCC);
  }
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was put in t0 by the jump table trampoline.
  // Convert to Smi for the runtime call
  __ SmiTag(kWasmCompileLazyFuncIndexRegister);
  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameScope scope(masm, StackFrame::WASM_COMPILE_LAZY);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    constexpr RegList gp_regs =
        Register::ListOf(a0, a2, a3, a4, a5, a6, a7);
    constexpr RegList fp_regs =
        DoubleRegister::ListOf(f2, f4, f6, f8, f10, f12, f14);
    __ MultiPush(gp_regs);
    __ MultiPushFPU(fp_regs);

    // Pass instance and function index as an explicit arguments to the runtime
    // function.
    __ Push(kWasmInstanceRegister, kWasmCompileLazyFuncIndexRegister);
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Move(kContextRegister, Smi::zero());
    __ CallRuntime(Runtime::kWasmCompileLazy, 2);

    // Restore registers.
    __ MultiPopFPU(fp_regs);
    __ MultiPop(gp_regs);
  }
  // Finally, jump to the entrypoint.
  __ Jump(v0);
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
    __ mov(s1, a2);
  } else {
    // Compute the argv pointer in a callee-saved register.
    __ Dlsa(s1, sp, a0, kPointerSizeLog2);
    __ Dsubu(s1, s1, kPointerSize);
  }

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(
      save_doubles == kSaveFPRegs, 0,
      builtin_exit_frame ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);

  // s0: number of arguments  including receiver (C callee-saved)
  // s1: pointer to first argument (C callee-saved)
  // s2: pointer to builtin function (C callee-saved)

  // Prepare arguments for C routine.
  // a0 = argc
  __ mov(s0, a0);
  __ mov(s2, a1);

  // We are calling compiled C/C++ code. a0 and a1 hold our two arguments. We
  // also need to reserve the 4 argument slots on the stack.

  __ AssertStackIsAligned();

  // a0 = argc, a1 = argv, a2 = isolate
  __ li(a2, ExternalReference::isolate_address(masm->isolate()));
  __ mov(a1, s1);

  __ StoreReturnAddressAndCall(s2);

  // Result returned in v0 or v1:v0 - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ LoadRoot(a4, RootIndex::kException);
  __ Branch(&exception_returned, eq, a4, Operand(v0));

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
  // v0:v1: result
  // sp: stack pointer
  // fp: frame pointer
  Register argc = argv_mode == kArgvInRegister
                      // We don't want to pop arguments so set argc to no_reg.
                      ? no_reg
                      // s0: still holds argc (callee-saved).
                      : s0;
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

  // Ask the runtime for help to determine the handler. This will set v0 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler =
      ExternalReference::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, 0, a0);
    __ mov(a0, zero_reg);
    __ mov(a1, zero_reg);
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
  __ li(t9, pending_handler_entrypoint_address);
  __ Ld(t9, MemOperand(t9));
  __ Jump(t9);
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
  __ Ldc1(double_scratch, MemOperand(sp, kArgumentOffset));

  // Clear cumulative exception flags and save the FCSR.
  __ cfc1(scratch2, FCSR);
  __ ctc1(zero_reg, FCSR);

  // Try a conversion to a signed integer.
  __ Trunc_w_d(double_scratch, double_scratch);
  // Move the converted value into the result register.
  __ mfc1(scratch3, double_scratch);

  // Retrieve and restore the FCSR.
  __ cfc1(scratch, FCSR);
  __ ctc1(scratch2, FCSR);

  // Check for overflow and NaNs.
  __ And(
      scratch, scratch,
      kFCSROverflowFlagMask | kFCSRUnderflowFlagMask | kFCSRInvalidOpFlagMask);
  // If we had no exceptions then set result_reg and we are done.
  Label error;
  __ Branch(&error, ne, scratch, Operand(zero_reg));
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
  __ Ext(result_reg, input_high, HeapNumber::kExponentShift,
         HeapNumber::kExponentBits);

  // Check for Infinity and NaNs, which should return 0.
  __ Subu(scratch, result_reg, HeapNumber::kExponentMask);
  __ Movz(result_reg, zero_reg, scratch);
  __ Branch(&done, eq, scratch, Operand(zero_reg));

  // Express exponent as delta to (number of mantissa bits + 31).
  __ Subu(result_reg, result_reg,
          Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits + 31));

  // If the delta is strictly positive, all bits would be shifted away,
  // which means that we can return 0.
  __ Branch(&normal_exponent, le, result_reg, Operand(zero_reg));
  __ mov(result_reg, zero_reg);
  __ Branch(&done);

  __ bind(&normal_exponent);
  const int kShiftBase = HeapNumber::kNonMantissaBitsInTopWord - 1;
  // Calculate shift.
  __ Addu(scratch, result_reg, Operand(kShiftBase + HeapNumber::kMantissaBits));

  // Save the sign.
  Register sign = result_reg;
  result_reg = no_reg;
  __ And(sign, input_high, Operand(HeapNumber::kSignMask));

  // On ARM shifts > 31 bits are valid and will result in zero. On MIPS we need
  // to check for this specific case.
  Label high_shift_needed, high_shift_done;
  __ Branch(&high_shift_needed, lt, scratch, Operand(32));
  __ mov(input_high, zero_reg);
  __ Branch(&high_shift_done);
  __ bind(&high_shift_needed);

  // Set the implicit 1 before the mantissa part in input_high.
  __ Or(input_high, input_high,
        Operand(1 << HeapNumber::kMantissaBitsInTopWord));
  // Shift the mantissa bits to the correct position.
  // We don't need to clear non-mantissa bits as they will be shifted away.
  // If they weren't, it would mean that the answer is in the 32bit range.
  __ sllv(input_high, input_high, scratch);

  __ bind(&high_shift_done);

  // Replace the shifted bits with bits from the lower mantissa word.
  Label pos_shift, shift_done;
  __ li(kScratchReg, 32);
  __ subu(scratch, kScratchReg, scratch);
  __ Branch(&pos_shift, ge, scratch, Operand(zero_reg));

  // Negate scratch.
  __ Subu(scratch, zero_reg, scratch);
  __ sllv(input_low, input_low, scratch);
  __ Branch(&shift_done);

  __ bind(&pos_shift);
  __ srlv(input_low, input_low, scratch);

  __ bind(&shift_done);
  __ Or(input_high, input_high, Operand(input_low));
  // Restore sign if necessary.
  __ mov(scratch, sign);
  result_reg = sign;
  sign = no_reg;
  __ Subu(result_reg, zero_reg, input_high);
  __ Movz(result_reg, input_high, scratch);

  __ bind(&done);

  __ Sd(result_reg, MemOperand(sp, kArgumentOffset));
  __ Pop(scratch, scratch2, scratch3);
  __ Pop(result_reg);
  __ Ret();
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
  __ li(t9, ExternalReference::is_profiling_address(isolate));
  __ Lb(t9, MemOperand(t9, 0));
  __ Branch(&profiler_enabled, ne, t9, Operand(zero_reg));
  __ li(t9, ExternalReference::address_of_runtime_stats_flag());
  __ Lw(t9, MemOperand(t9, 0));
  __ Branch(&profiler_enabled, ne, t9, Operand(zero_reg));
  {
    // Call the api function directly.
    __ mov(t9, function_address);
    __ Branch(&end_profiler_check);
  }

  __ bind(&profiler_enabled);
  {
    // Additional parameter is the address of the actual callback.
    __ li(t9, thunk_ref);
  }
  __ bind(&end_profiler_check);

  // Allocate HandleScope in callee-save registers.
  __ li(s5, next_address);
  __ Ld(s0, MemOperand(s5, kNextOffset));
  __ Ld(s1, MemOperand(s5, kLimitOffset));
  __ Lw(s2, MemOperand(s5, kLevelOffset));
  __ Addu(s2, s2, Operand(1));
  __ Sw(s2, MemOperand(s5, kLevelOffset));

  __ StoreReturnAddressAndCall(t9);

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // Load value from ReturnValue.
  __ Ld(v0, return_value_operand);
  __ bind(&return_value_loaded);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ Sd(s0, MemOperand(s5, kNextOffset));
  if (__ emit_debug_code()) {
    __ Lw(a1, MemOperand(s5, kLevelOffset));
    __ Check(eq, AbortReason::kUnexpectedLevelAfterReturnFromApiCall, a1,
             Operand(s2));
  }
  __ Subu(s2, s2, Operand(1));
  __ Sw(s2, MemOperand(s5, kLevelOffset));
  __ Ld(kScratchReg, MemOperand(s5, kLimitOffset));
  __ Branch(&delete_allocated_handles, ne, s1, Operand(kScratchReg));

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);

  if (stack_space_operand == nullptr) {
    DCHECK_NE(stack_space, 0);
    __ li(s0, Operand(stack_space));
  } else {
    DCHECK_EQ(stack_space, 0);
    STATIC_ASSERT(kCArgSlotCount == 0);
    __ Ld(s0, *stack_space_operand);
  }

  static constexpr bool kDontSaveDoubles = false;
  static constexpr bool kRegisterContainsSlotCount = false;
  __ LeaveExitFrame(kDontSaveDoubles, s0, NO_EMIT_RETURN,
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
  __ mov(s0, v0);
  __ mov(a0, v0);
  __ PrepareCallCFunction(1, s1);
  __ li(a0, ExternalReference::isolate_address(isolate));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(), 1);
  __ mov(v0, s0);
  __ jmp(&leave_exit_frame);
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
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1) * 8]  : first argument
  //  -- sp[(argc + 0) * 8]  : receiver
  // -----------------------------------

  Register api_function_address = a1;
  Register argc = a2;
  Register call_data = a3;
  Register holder = a0;
  Register scratch = t0;
  Register base = t1;  // For addressing MemOperands on the stack.

  DCHECK(!AreAliased(api_function_address, argc, call_data,
                     holder, scratch, base));

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
  __ Dlsa(base, sp, argc, kPointerSizeLog2);

  // Reserve space on the stack.
  __ Dsubu(sp, sp, Operand(FCA::kArgsLength * kPointerSize));

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
  __ mov(scratch, sp);

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
  __ Dsubu(scratch, base, Operand(1 * kPointerSize));
  __ Sd(scratch, MemOperand(sp, 2 * kPointerSize));

  // FunctionCallbackInfo::length_.
  // Stored as int field, 32-bit integers within struct on stack always left
  // justified by n64 ABI.
  __ Sw(argc, MemOperand(sp, 3 * kPointerSize));

  // We also store the number of bytes to drop from the stack after returning
  // from the API function here.
  // Note: Unlike on other architectures, this stores the number of slots to
  // drop, not the number of bytes.
  __ Daddu(scratch, argc, Operand(FCA::kArgsLength + 1 /* receiver */));
  __ Sd(scratch, MemOperand(sp, 4 * kPointerSize));

  // v8::InvocationCallback's argument.
  DCHECK(!AreAliased(api_function_address, scratch, a0));
  __ Daddu(a0, sp, Operand(1 * kPointerSize));

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
  __ Dsubu(sp, sp, (PCA::kArgsLength + 1) * kPointerSize);
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
  __ mov(a0, sp);                               // a0 = Handle<Name>
  __ Daddu(a1, a0, Operand(1 * kPointerSize));  // a1 = v8::PCI::args_

  const int kApiStackSpace = 1;
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ Sd(a1, MemOperand(sp, 1 * kPointerSize));
  __ Daddu(a1, sp, Operand(1 * kPointerSize));
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
  __ daddiu(sp, sp, -kCArgsSlotsSize);

  __ Sd(ra, MemOperand(sp, kCArgsSlotsSize));  // Store the return address.
  __ Call(t9);                                 // Call the C++ function.
  __ Ld(t9, MemOperand(sp, kCArgsSlotsSize));  // Return to calling code.

  if (FLAG_debug_code && FLAG_enable_slow_asserts) {
    // In case of an error the return address may point to a memory area
    // filled with kZapValue by the GC. Dereference the address and check for
    // this.
    __ Uld(a4, MemOperand(t9));
    __ Assert(ne, AbortReason::kReceivedInvalidReturnAddress, a4,
              Operand(reinterpret_cast<uint64_t>(kZapValue)));
  }

  __ Jump(t9);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64

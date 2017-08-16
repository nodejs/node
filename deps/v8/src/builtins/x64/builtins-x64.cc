// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/code-factory.h"
#include "src/codegen.h"
#include "src/counters.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments excluding receiver
  //  -- rdi                 : target
  //  -- rdx                 : new.target
  //  -- rsp[0]              : return address
  //  -- rsp[8]              : last argument
  //  -- ...
  //  -- rsp[8 * argc]       : first argument
  //  -- rsp[8 * (argc + 1)] : receiver
  // -----------------------------------
  __ AssertFunction(rdi);

  // The logic contained here is mirrored for TurboFan inlining in
  // JSTypedLowering::ReduceJSCall{Function,Construct}. Keep these in sync.

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // JumpToExternalReference expects rax to contain the number of arguments
  // including the receiver and the extra arguments.
  const int num_extra_args = 3;
  __ addp(rax, Immediate(num_extra_args + 1));

  // Unconditionally insert argc, target and new target as extra arguments. They
  // will be used by stack frame iterators when constructing the stack trace.
  __ PopReturnAddressTo(kScratchRegister);
  __ Integer32ToSmi(rax, rax);
  __ Push(rax);
  __ SmiToInteger32(rax, rax);
  __ Push(rdi);
  __ Push(rdx);
  __ PushReturnAddressFrom(kScratchRegister);

  __ JumpToExternalReference(ExternalReference(address, masm->isolate()),
                             exit_frame_type == BUILTIN_EXIT);
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ movp(kScratchRegister,
          FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movp(kScratchRegister,
          FieldOperand(kScratchRegister, SharedFunctionInfo::kCodeOffset));
  __ leap(kScratchRegister, FieldOperand(kScratchRegister, Code::kHeaderSize));
  __ jmp(kScratchRegister);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- rax : argument count (preserved for callee)
  //  -- rdx : new target (preserved for callee)
  //  -- rdi : target function (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push the number of arguments to the callee.
    __ Integer32ToSmi(rax, rax);
    __ Push(rax);
    // Push a copy of the target function and the new target.
    __ Push(rdi);
    __ Push(rdx);
    // Function is also the parameter to the runtime call.
    __ Push(rdi);

    __ CallRuntime(function_id, 1);
    __ movp(rbx, rax);

    // Restore target function and new target.
    __ Pop(rdx);
    __ Pop(rdi);
    __ Pop(rax);
    __ SmiToInteger32(rax, rax);
  }
  __ leap(rbx, FieldOperand(rbx, Code::kHeaderSize));
  __ jmp(rbx);
}

void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
  __ j(above_equal, &ok);

  GenerateTailCallToReturnedCode(masm, Runtime::kTryInstallOptimizedCode);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  //  -- rdi: constructor function
  //  -- rdx: new target
  //  -- rsi: context
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ Integer32ToSmi(rcx, rax);
    __ Push(rsi);
    __ Push(rcx);

    // The receiver for the builtin/api call.
    __ PushRoot(Heap::kTheHoleValueRootIndex);

    // Set up pointer to last argument.
    __ leap(rbx, Operand(rbp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ movp(rcx, rax);
    // ----------- S t a t e -------------
    //  --                rax: number of arguments (untagged)
    //  --                rdi: constructor function
    //  --                rdx: new target
    //  --                rbx: pointer to last argument
    //  --                rcx: counter
    //  -- sp[0*kPointerSize]: the hole (receiver)
    //  -- sp[1*kPointerSize]: number of arguments (tagged)
    //  -- sp[2*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry);
    __ bind(&loop);
    __ Push(Operand(rbx, rcx, times_pointer_size, 0));
    __ bind(&entry);
    __ decp(rcx);
    __ j(greater_equal, &loop);

    // Call the function.
    // rax: number of arguments (untagged)
    // rdi: constructor function
    // rdx: new target
    ParameterCount actual(rax);
    __ InvokeFunction(rdi, rdx, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // Restore context from the frame.
    __ movp(rsi, Operand(rbp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ movp(rbx, Operand(rbp, ConstructFrameConstants::kLengthOffset));

    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ leap(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);

  __ ret(0);
}

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Generate_JSConstructStubGeneric(MacroAssembler* masm,
                                     bool restrict_constructor_return) {
  // ----------- S t a t e -------------
  //  -- rax: number of arguments (untagged)
  //  -- rdi: constructor function
  //  -- rdx: new target
  //  -- rsi: context
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ Integer32ToSmi(rcx, rax);
    __ Push(rsi);
    __ Push(rcx);
    __ Push(rdi);
    __ Push(rdx);

    // ----------- S t a t e -------------
    //  --         sp[0*kPointerSize]: new target
    //  -- rdi and sp[1*kPointerSize]: constructor function
    //  --         sp[2*kPointerSize]: argument count
    //  --         sp[3*kPointerSize]: context
    // -----------------------------------

    __ movp(rbx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ testb(FieldOperand(rbx, SharedFunctionInfo::kFunctionKindByteOffset),
             Immediate(SharedFunctionInfo::kDerivedConstructorBitsWithinByte));
    __ j(not_zero, &not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1);
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ jmp(&post_instantiation_deopt_entry, Label::kNear);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(rax, Heap::kTheHoleValueRootIndex);

    // ----------- S t a t e -------------
    //  -- rax                          implicit receiver
    //  -- Slot 3 / sp[0*kPointerSize]  new target
    //  -- Slot 2 / sp[1*kPointerSize]  constructor function
    //  -- Slot 1 / sp[2*kPointerSize]  number of arguments (tagged)
    //  -- Slot 0 / sp[3*kPointerSize]  context
    // -----------------------------------
    // Deoptimizer enters here.
    masm->isolate()->heap()->SetConstructStubCreateDeoptPCOffset(
        masm->pc_offset());
    __ bind(&post_instantiation_deopt_entry);

    // Restore new target.
    __ Pop(rdx);

    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(rax);
    __ Push(rax);

    // ----------- S t a t e -------------
    //  -- sp[0*kPointerSize]  implicit receiver
    //  -- sp[1*kPointerSize]  implicit receiver
    //  -- sp[2*kPointerSize]  constructor function
    //  -- sp[3*kPointerSize]  number of arguments (tagged)
    //  -- sp[4*kPointerSize]  context
    // -----------------------------------

    // Restore constructor function and argument count.
    __ movp(rdi, Operand(rbp, ConstructFrameConstants::kConstructorOffset));
    __ SmiToInteger32(rax,
                      Operand(rbp, ConstructFrameConstants::kLengthOffset));

    // Set up pointer to last argument.
    __ leap(rbx, Operand(rbp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ movp(rcx, rax);
    // ----------- S t a t e -------------
    //  --                        rax: number of arguments (untagged)
    //  --                        rdx: new target
    //  --                        rbx: pointer to last argument
    //  --                        rcx: counter (tagged)
    //  --         sp[0*kPointerSize]: implicit receiver
    //  --         sp[1*kPointerSize]: implicit receiver
    //  -- rdi and sp[2*kPointerSize]: constructor function
    //  --         sp[3*kPointerSize]: number of arguments (tagged)
    //  --         sp[4*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry, Label::kNear);
    __ bind(&loop);
    __ Push(Operand(rbx, rcx, times_pointer_size, 0));
    __ bind(&entry);
    __ decp(rcx);
    __ j(greater_equal, &loop);

    // Call the function.
    ParameterCount actual(rax);
    __ InvokeFunction(rdi, rdx, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // ----------- S t a t e -------------
    //  -- rax                 constructor result
    //  -- sp[0*kPointerSize]  implicit receiver
    //  -- sp[1*kPointerSize]  constructor function
    //  -- sp[2*kPointerSize]  number of arguments
    //  -- sp[3*kPointerSize]  context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore context from the frame.
    __ movp(rsi, Operand(rbp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, other_result, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(rax, Heap::kUndefinedValueRootIndex, &use_receiver,
                  Label::kNear);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(rax, &other_result, Label::kNear);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CmpObjectType(rax, FIRST_JS_RECEIVER_TYPE, rcx);
    __ j(above_equal, &leave_frame, Label::kNear);

    __ bind(&other_result);
    // The result is now neither undefined nor an object.
    if (restrict_constructor_return) {
      // Throw if constructor function is a class constructor
      __ movp(rbx, Operand(rbp, ConstructFrameConstants::kConstructorOffset));
      __ movp(rbx, FieldOperand(rbx, JSFunction::kSharedFunctionInfoOffset));
      __ testb(FieldOperand(rbx, SharedFunctionInfo::kFunctionKindByteOffset),
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
    __ movp(rax, Operand(rsp, 0 * kPointerSize));
    __ JumpIfRoot(rax, Heap::kTheHoleValueRootIndex, &do_throw);

    __ bind(&leave_frame);
    // Restore the arguments count.
    __ movp(rbx, Operand(rbp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ leap(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);
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
  __ Push(rdi);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

enum IsTagged { kRaxIsSmiTagged, kRaxIsUntaggedInt };

// Clobbers rcx, r11, kScratchRegister; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm,
                                        IsTagged rax_is_tagged) {
  // rax   : the number of items to be pushed to the stack
  //
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(kScratchRegister, Heap::kRealStackLimitRootIndex);
  __ movp(rcx, rsp);
  // Make rcx the space we have left. The stack might already be overflowed
  // here which will cause rcx to become negative.
  __ subp(rcx, kScratchRegister);
  // Make r11 the space we need for the array when it is unrolled onto the
  // stack.
  if (rax_is_tagged == kRaxIsSmiTagged) {
    __ PositiveSmiTimesPowerOfTwoToInteger64(r11, rax, kPointerSizeLog2);
  } else {
    DCHECK(rax_is_tagged == kRaxIsUntaggedInt);
    __ movp(r11, rax);
    __ shlq(r11, Immediate(kPointerSizeLog2));
  }
  // Check if the arguments will overflow the stack.
  __ cmpp(rcx, r11);
  __ j(greater, &okay);  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Expects five C++ function parameters.
  // - Object* new_target
  // - JSFunction* function
  // - Object* receiver
  // - int argc
  // - Object*** argv
  // (see Handle::Invoke in execution.cc).

  // Open a C++ scope for the FrameScope.
  {
// Platform specific argument handling. After this, the stack contains
// an internal frame and the pushed function and receiver, and
// register rax and rbx holds the argument count and argument array,
// while rdi holds the function pointer, rsi the context, and rdx the
// new.target.

#ifdef _WIN64
    // MSVC parameters in:
    // rcx        : new_target
    // rdx        : function
    // r8         : receiver
    // r9         : argc
    // [rsp+0x20] : argv

    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ movp(rsi, masm->ExternalOperand(context_address));

    // Push the function and the receiver onto the stack.
    __ Push(rdx);
    __ Push(r8);

    // Load the number of arguments and setup pointer to the arguments.
    __ movp(rax, r9);
    // Load the previous frame pointer to access C argument on stack
    __ movp(kScratchRegister, Operand(rbp, 0));
    __ movp(rbx, Operand(kScratchRegister, EntryFrameConstants::kArgvOffset));
    // Load the function pointer into rdi.
    __ movp(rdi, rdx);
    // Load the new.target into rdx.
    __ movp(rdx, rcx);
#else   // _WIN64
    // GCC parameters in:
    // rdi : new_target
    // rsi : function
    // rdx : receiver
    // rcx : argc
    // r8  : argv

    __ movp(r11, rdi);
    __ movp(rdi, rsi);
    // rdi : function
    // r11 : new_target

    // Clear the context before we push it when entering the internal frame.
    __ Set(rsi, 0);

    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ movp(rsi, masm->ExternalOperand(context_address));

    // Push the function and receiver onto the stack.
    __ Push(rdi);
    __ Push(rdx);

    // Load the number of arguments and setup pointer to the arguments.
    __ movp(rax, rcx);
    __ movp(rbx, r8);

    // Load the new.target into rdx.
    __ movp(rdx, r11);
#endif  // _WIN64

    // Current stack contents:
    // [rsp + 2 * kPointerSize ... ] : Internal frame
    // [rsp + kPointerSize]          : function
    // [rsp]                         : receiver
    // Current register contents:
    // rax : argc
    // rbx : argv
    // rsi : context
    // rdi : function
    // rdx : new.target

    // Check if we have enough stack space to push all arguments.
    // Expects argument count in rax. Clobbers rcx, r11.
    Generate_CheckStackOverflow(masm, kRaxIsUntaggedInt);

    // Copy arguments to the stack in a loop.
    // Register rbx points to array of pointers to handle locations.
    // Push the values of these handles.
    Label loop, entry;
    __ Set(rcx, 0);  // Set loop variable to 0.
    __ jmp(&entry, Label::kNear);
    __ bind(&loop);
    __ movp(kScratchRegister, Operand(rbx, rcx, times_pointer_size, 0));
    __ Push(Operand(kScratchRegister, 0));  // dereference handle
    __ addp(rcx, Immediate(1));
    __ bind(&entry);
    __ cmpp(rcx, rax);
    __ j(not_equal, &loop);

    // Invoke the builtin code.
    Handle<Code> builtin = is_construct
                               ? masm->isolate()->builtins()->Construct()
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the internal frame. Notice that this also removes the empty
    // context and the function left on the stack by the code
    // invocation.
  }

  // TODO(X64): Is argument correct? Is there a receiver to remove?
  __ ret(1 * kPointerSize);  // Remove receiver.
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
  //  -- rax    : the value to pass to the generator
  //  -- rbx    : the JSGeneratorObject to resume
  //  -- rdx    : the resume mode (tagged)
  //  -- rcx    : the SuspendFlags of the earlier suspend call (tagged)
  //  -- rsp[0] : return address
  // -----------------------------------
  // Untag rcx
  __ shrq(rcx, Immediate(kSmiTagSize + kSmiShiftSize));
  __ AssertGeneratorObject(rbx, rcx);

  // Store input value into generator object.
  Label async_await, done_store_input;

  __ andb(rcx, Immediate(static_cast<int>(SuspendFlags::kAsyncGeneratorAwait)));
  __ cmpb(rcx, Immediate(static_cast<int>(SuspendFlags::kAsyncGeneratorAwait)));
  __ j(equal, &async_await);

  __ movp(FieldOperand(rbx, JSGeneratorObject::kInputOrDebugPosOffset), rax);
  __ RecordWriteField(rbx, JSGeneratorObject::kInputOrDebugPosOffset, rax, rcx,
                      kDontSaveFPRegs);
  __ j(always, &done_store_input, Label::kNear);

  __ bind(&async_await);
  __ movp(
      FieldOperand(rbx, JSAsyncGeneratorObject::kAwaitInputOrDebugPosOffset),
      rax);
  __ RecordWriteField(rbx, JSAsyncGeneratorObject::kAwaitInputOrDebugPosOffset,
                      rax, rcx, kDontSaveFPRegs);

  __ bind(&done_store_input);
  // `rcx` no longer holds SuspendFlags

  // Store resume mode into generator object.
  __ movp(FieldOperand(rbx, JSGeneratorObject::kResumeModeOffset), rdx);

  // Load suspended function and context.
  __ movp(rdi, FieldOperand(rbx, JSGeneratorObject::kFunctionOffset));
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  Operand debug_hook_operand = masm->ExternalOperand(debug_hook);
  __ cmpb(debug_hook_operand, Immediate(0));
  __ j(not_equal, &prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  Operand debug_suspended_generator_operand =
      masm->ExternalOperand(debug_suspended_generator);
  __ cmpp(rbx, debug_suspended_generator_operand);
  __ j(equal, &prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Pop return address.
  __ PopReturnAddressTo(rax);

  // Push receiver.
  __ Push(FieldOperand(rbx, JSGeneratorObject::kReceiverOffset));

  // ----------- S t a t e -------------
  //  -- rax    : return address
  //  -- rbx    : the JSGeneratorObject to resume
  //  -- rdx    : the resume mode (tagged)
  //  -- rdi    : generator function
  //  -- rsi    : generator context
  //  -- rsp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ LoadSharedFunctionInfoSpecialField(
      rcx, rcx, SharedFunctionInfo::kFormalParameterCountOffset);
  {
    Label done_loop, loop;
    __ bind(&loop);
    __ subl(rcx, Immediate(1));
    __ j(carry, &done_loop, Label::kNear);
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    __ jmp(&loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ movp(rcx, FieldOperand(rcx, SharedFunctionInfo::kFunctionDataOffset));
    __ CmpObjectType(rcx, BYTECODE_ARRAY_TYPE, rcx);
    __ Assert(equal, kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ PushReturnAddressFrom(rax);
    __ movp(rax, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ LoadSharedFunctionInfoSpecialField(
        rax, rax, SharedFunctionInfo::kFormalParameterCountOffset);
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ movp(rdx, rbx);
    __ jmp(FieldOperand(rdi, JSFunction::kCodeEntryOffset));
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rbx);
    __ Push(rdx);
    __ Push(rdi);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(rdx);
    __ Pop(rbx);
    __ movp(rdi, FieldOperand(rbx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rbx);
    __ Push(rdx);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(rdx);
    __ Pop(rbx);
    __ movp(rdi, FieldOperand(rbx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);
}

static void ReplaceClosureEntryWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code_entry, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {
  Register native_context = scratch1;

  // Store the optimized code in the closure.
  __ leap(optimized_code_entry,
          FieldOperand(optimized_code_entry, Code::kHeaderSize));
  __ movp(FieldOperand(closure, JSFunction::kCodeEntryOffset),
          optimized_code_entry);
  __ RecordWriteCodeEntryField(closure, optimized_code_entry, scratch2);

  // Link the closure into the optimized function list.
  __ movp(native_context, NativeContextOperand());
  __ movp(scratch3,
          ContextOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  __ movp(FieldOperand(closure, JSFunction::kNextFunctionLinkOffset), scratch3);
  __ RecordWriteField(closure, JSFunction::kNextFunctionLinkOffset, scratch3,
                      scratch2, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  const int function_list_offset =
      Context::SlotOffset(Context::OPTIMIZED_FUNCTIONS_LIST);
  __ movp(ContextOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST),
          closure);
  // Save closure before the write barrier.
  __ movp(scratch3, closure);
  __ RecordWriteContextSlot(native_context, function_list_offset, closure,
                            scratch2, kDontSaveFPRegs);
  __ movp(closure, scratch3);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch1,
                                  Register scratch2) {
  Register args_count = scratch1;
  Register return_pc = scratch2;

  // Get the arguments + receiver count.
  __ movp(args_count,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ movl(args_count,
          FieldOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ leave();

  // Drop receiver + arguments.
  __ PopReturnAddressTo(return_pc);
  __ addp(rsp, args_count);
  __ PushReturnAddressFrom(return_pc);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o rdi: the JS function object being called
//   o rdx: the new target
//   o rsi: our context
//   o rbp: the caller's frame pointer
//   o rsp: stack pointer (pointing to return address)
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ pushq(rbp);  // Caller's frame pointer.
  __ movp(rbp, rsp);
  __ Push(rsi);  // Callee's context.
  __ Push(rdi);  // Callee's JS function.
  __ Push(rdx);  // Callee's new target.

  // First check if there is optimized code in the feedback vector which we
  // could call instead.
  Label switch_to_optimized_code;
  Register optimized_code_entry = rcx;
  __ movp(rbx, FieldOperand(rdi, JSFunction::kFeedbackVectorOffset));
  __ movp(rbx, FieldOperand(rbx, Cell::kValueOffset));
  __ movp(rbx,
          FieldOperand(rbx, FeedbackVector::kOptimizedCodeIndex * kPointerSize +
                                FeedbackVector::kHeaderSize));
  __ movp(optimized_code_entry, FieldOperand(rbx, WeakCell::kValueOffset));
  __ JumpIfNotSmi(optimized_code_entry, &switch_to_optimized_code);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  __ movp(rax, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  Label load_debug_bytecode_array, bytecode_array_loaded;
  __ JumpIfNotSmi(FieldOperand(rax, SharedFunctionInfo::kDebugInfoOffset),
                  &load_debug_bytecode_array);
  __ movp(kInterpreterBytecodeArrayRegister,
          FieldOperand(rax, SharedFunctionInfo::kFunctionDataOffset));
  __ bind(&bytecode_array_loaded);

  // Check whether we should continue to use the interpreter.
  // TODO(rmcilroy) Remove self healing once liveedit only has to deal with
  // Ignition bytecode.
  Label switch_to_different_code_kind;
  __ Move(rcx, masm->CodeObject());  // Self-reference to this code.
  __ cmpp(rcx, FieldOperand(rax, SharedFunctionInfo::kCodeOffset));
  __ j(not_equal, &switch_to_different_code_kind);

  // Increment invocation count for the function.
  __ movp(rcx, FieldOperand(rdi, JSFunction::kFeedbackVectorOffset));
  __ movp(rcx, FieldOperand(rcx, Cell::kValueOffset));
  __ SmiAddConstant(
      FieldOperand(rcx, FeedbackVector::kInvocationCountIndex * kPointerSize +
                            FeedbackVector::kHeaderSize),
      Smi::FromInt(1));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     rax);
    __ Assert(equal, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Reset code age.
  __ movb(FieldOperand(kInterpreterBytecodeArrayRegister,
                       BytecodeArray::kBytecodeAgeOffset),
          Immediate(BytecodeArray::kNoAgeBytecodeAge));

  // Load initial bytecode offset.
  __ movp(kInterpreterBytecodeOffsetRegister,
          Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode offset.
  __ Push(kInterpreterBytecodeArrayRegister);
  __ Integer32ToSmi(rcx, kInterpreterBytecodeOffsetRegister);
  __ Push(rcx);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    __ movl(rcx, FieldOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ movp(rdx, rsp);
    __ subp(rdx, rcx);
    __ CompareRoot(rdx, Heap::kRealStackLimitRootIndex);
    __ j(above_equal, &ok, Label::kNear);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
    __ j(always, &loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ Push(rdx);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ subp(rcx, Immediate(kPointerSize));
    __ j(greater_equal, &loop_header, Label::kNear);
  }

  // Load accumulator and dispatch table into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Dispatch to the first bytecode handler for the function.
  __ movzxbp(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ movp(rbx, Operand(kInterpreterDispatchTableRegister, rbx,
                       times_pointer_size, 0));
  __ call(rbx);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // The return value is in rax.
  LeaveInterpreterFrame(masm, rbx, rcx);
  __ ret(0);

  // Load debug copy of the bytecode array.
  __ bind(&load_debug_bytecode_array);
  Register debug_info = kInterpreterBytecodeArrayRegister;
  __ movp(debug_info, FieldOperand(rax, SharedFunctionInfo::kDebugInfoOffset));
  __ movp(kInterpreterBytecodeArrayRegister,
          FieldOperand(debug_info, DebugInfo::kDebugBytecodeArrayIndex));
  __ jmp(&bytecode_array_loaded);

  // If the shared code is no longer this entry trampoline, then the underlying
  // function has been switched to a different kind of code and we heal the
  // closure by switching the code entry field over to the new code as well.
  __ bind(&switch_to_different_code_kind);
  __ leave();  // Leave the frame so we can tail call.
  __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movp(rcx, FieldOperand(rcx, SharedFunctionInfo::kCodeOffset));
  __ leap(rcx, FieldOperand(rcx, Code::kHeaderSize));
  __ movp(FieldOperand(rdi, JSFunction::kCodeEntryOffset), rcx);
  __ RecordWriteCodeEntryField(rdi, rcx, r15);
  __ jmp(rcx);

  // If there is optimized code on the type feedback vector, check if it is good
  // to run, and if so, self heal the closure and call the optimized code.
  __ bind(&switch_to_optimized_code);
  __ leave();
  Label gotta_call_runtime;

  // Check if the optimized code is marked for deopt.
  __ testl(FieldOperand(optimized_code_entry, Code::kKindSpecificFlags1Offset),
           Immediate(1 << Code::kMarkedForDeoptimizationBit));
  __ j(not_zero, &gotta_call_runtime);

  // Optimized code is good, get it into the closure and link the closure into
  // the optimized functions list, then tail call the optimized code.
  ReplaceClosureEntryWithOptimizedCode(masm, optimized_code_entry, rdi, r14,
                                       r15, rbx);
  __ jmp(optimized_code_entry);

  // Optimized code is marked for deopt, bailout to the CompileLazy runtime
  // function which will clear the feedback vector's optimized code slot.
  __ bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
}

static void Generate_StackOverflowCheck(
    MacroAssembler* masm, Register num_args, Register scratch,
    Label* stack_overflow,
    Label::Distance stack_overflow_distance = Label::kFar) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(kScratchRegister, Heap::kRealStackLimitRootIndex);
  __ movp(scratch, rsp);
  // Make scratch the space we have left. The stack might already be overflowed
  // here which will cause scratch to become negative.
  __ subp(scratch, kScratchRegister);
  __ sarp(scratch, Immediate(kPointerSizeLog2));
  // Check if the arguments will overflow the stack.
  __ cmpp(scratch, num_args);
  // Signed comparison.
  __ j(less_equal, stack_overflow, stack_overflow_distance);
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args,
                                         Register start_address,
                                         Register scratch) {
  // Find the address of the last argument.
  __ Move(scratch, num_args);
  __ shlp(scratch, Immediate(kPointerSizeLog2));
  __ negp(scratch);
  __ addp(scratch, start_address);

  // Push the arguments.
  Label loop_header, loop_check;
  __ j(always, &loop_check);
  __ bind(&loop_header);
  __ Push(Operand(start_address, 0));
  __ subp(start_address, Immediate(kPointerSize));
  __ bind(&loop_check);
  __ cmpp(start_address, scratch);
  __ j(greater, &loop_header, Label::kNear);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    TailCallMode tail_call_mode, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rbx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  //  -- rdi : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  // Number of values to be pushed.
  __ Move(rcx, rax);
  __ addp(rcx, Immediate(1));  // Add one for receiver.

  // Add a stack check before pushing arguments.
  Generate_StackOverflowCheck(masm, rcx, rdx, &stack_overflow);

  // Pop return address to allow tail-call after pushing arguments.
  __ PopReturnAddressTo(kScratchRegister);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ subp(rcx, Immediate(1));  // Subtract one for receiver.
  }

  // rbx and rdx will be modified.
  Generate_InterpreterPushArgs(masm, rcx, rbx, rdx);

  // Call the target.
  __ PushReturnAddressFrom(kScratchRegister);  // Re-push return address.

  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ Jump(masm->isolate()->builtins()->CallFunction(receiver_mode,
                                                      tail_call_mode),
            RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(masm->isolate()->builtins()->CallWithSpread(),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(receiver_mode, tail_call_mode),
            RelocInfo::CODE_TARGET);
  }

  // Throw stack overflow exception.
  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // This should be unreachable.
    __ int3();
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- rdi : the constructor to call (can be any Object)
  //  -- rbx : the allocation site feedback if available, undefined otherwise
  //  -- rcx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  // -----------------------------------
  Label stack_overflow;

  // Add a stack check before pushing arguments.
  Generate_StackOverflowCheck(masm, rax, r8, &stack_overflow);

  // Pop return address to allow tail-call after pushing arguments.
  __ PopReturnAddressTo(kScratchRegister);

  // Push slot for the receiver to be constructed.
  __ Push(Immediate(0));

  // rcx and r8 will be modified.
  Generate_InterpreterPushArgs(masm, rax, rcx, r8);

  // Push return address in preparation for the tail-call.
  __ PushReturnAddressFrom(kScratchRegister);

  __ AssertUndefinedOrAllocationSite(rbx);
  if (mode == InterpreterPushArgsMode::kJSFunction) {
    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ AssertFunction(rdi);

    __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ movp(rcx, FieldOperand(rcx, SharedFunctionInfo::kConstructStubOffset));
    __ leap(rcx, FieldOperand(rcx, Code::kHeaderSize));
    // Jump to the constructor function (rax, rbx, rdx passed on).
    __ jmp(rcx);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor (rax, rdx, rdi passed on).
    __ Jump(masm->isolate()->builtins()->ConstructWithSpread(),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor (rax, rdx, rdi passed on).
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }

  // Throw stack overflow exception.
  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // This should be unreachable.
    __ int3();
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructArray(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the target to call checked to be Array function.
  //  -- rbx : the allocation site feedback
  //  -- rcx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  // -----------------------------------
  Label stack_overflow;

  // Number of values to be pushed.
  __ Move(r8, rax);

  // Add a stack check before pushing arguments.
  Generate_StackOverflowCheck(masm, r8, rdi, &stack_overflow);

  // Pop return address to allow tail-call after pushing arguments.
  __ PopReturnAddressTo(kScratchRegister);

  // Push slot for the receiver to be constructed.
  __ Push(Immediate(0));

  // rcx and rdi will be modified.
  Generate_InterpreterPushArgs(masm, r8, rcx, rdi);

  // Push return address in preparation for the tail-call.
  __ PushReturnAddressFrom(kScratchRegister);

  // Array constructor expects constructor in rdi. It is same as rdx here.
  __ Move(rdi, rdx);

  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);

  // Throw stack overflow exception.
  __ bind(&stack_overflow);
  {
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
  __ Move(rbx, masm->isolate()->builtins()->InterpreterEntryTrampoline());
  __ addp(rbx, Immediate(interpreter_entry_return_pc_offset->value() +
                         Code::kHeaderSize - kHeapObjectTag));
  __ Push(rbx);

  // Initialize dispatch table register.
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Get the bytecode array pointer from the frame.
  __ movp(kInterpreterBytecodeArrayRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     rbx);
    __ Assert(equal, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ movp(kInterpreterBytecodeOffsetRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiToInteger32(kInterpreterBytecodeOffsetRegister,
                    kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ movzxbp(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ movp(rbx, Operand(kInterpreterDispatchTableRegister, rbx,
                       times_pointer_size, 0));
  __ jmp(rbx);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ movp(rbx, Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ movp(rdx, Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(kInterpreterAccumulatorRegister);
    __ Push(rbx);  // First argument is the bytecode array.
    __ Push(rdx);  // Second argument is the bytecode offset.
    __ CallRuntime(Runtime::kInterpreterAdvanceBytecodeOffset);
    __ Move(rdx, rax);  // Result is the new bytecode offset.
    __ Pop(kInterpreterAccumulatorRegister);
  }
  __ movp(Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp), rdx);

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : argument count (preserved for callee)
  //  -- rdx : new target (preserved for callee)
  //  -- rdi : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;
  Label try_shared;

  Register closure = rdi;

  // Do we have a valid feedback vector?
  __ movp(rbx, FieldOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ movp(rbx, FieldOperand(rbx, Cell::kValueOffset));
  __ JumpIfRoot(rbx, Heap::kUndefinedValueRootIndex, &gotta_call_runtime);

  // Is optimized code available in the feedback vector?
  Register entry = rcx;
  __ movp(entry,
          FieldOperand(rbx, FeedbackVector::kOptimizedCodeIndex * kPointerSize +
                                FeedbackVector::kHeaderSize));
  __ movp(entry, FieldOperand(entry, WeakCell::kValueOffset));
  __ JumpIfSmi(entry, &try_shared);

  // Found code, check if it is marked for deopt, if so call into runtime to
  // clear the optimized code slot.
  __ testl(FieldOperand(entry, Code::kKindSpecificFlags1Offset),
           Immediate(1 << Code::kMarkedForDeoptimizationBit));
  __ j(not_zero, &gotta_call_runtime);

  // Code is good, get it into the closure and tail call.
  ReplaceClosureEntryWithOptimizedCode(masm, entry, closure, r14, r15, rbx);
  __ jmp(entry);

  // We found no optimized code.
  __ bind(&try_shared);
  __ movp(entry, FieldOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Is the shared function marked for tier up?
  __ testb(FieldOperand(entry, SharedFunctionInfo::kMarkedForTierUpByteOffset),
           Immediate(1 << SharedFunctionInfo::kMarkedForTierUpBitWithinByte));
  __ j(not_zero, &gotta_call_runtime);

  // If SFI points to anything other than CompileLazy, install that.
  __ movp(entry, FieldOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ Move(rbx, masm->CodeObject());
  __ cmpp(entry, rbx);
  __ j(equal, &gotta_call_runtime);

  // Install the SFI's code entry.
  __ leap(entry, FieldOperand(entry, Code::kHeaderSize));
  __ movp(FieldOperand(closure, JSFunction::kCodeEntryOffset), entry);
  __ RecordWriteCodeEntryField(closure, entry, r15);
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
  //  -- rax : argument count (preserved for callee)
  //  -- rdx : new target (preserved for callee)
  //  -- rdi : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ movp(rcx, rax);
    // Push the number of arguments to the callee.
    __ Integer32ToSmi(rax, rax);
    __ Push(rax);
    // Push a copy of the target function and the new target.
    __ Push(rdi);
    __ Push(rdx);

    // The function.
    __ Push(rdi);
    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ cmpp(rcx, Immediate(j));
        __ j(not_equal, &over, Label::kNear);
      }
      for (int i = j - 1; i >= 0; --i) {
        __ Push(Operand(
            rbp, StandardFrameConstants::kCallerSPOffset + i * kPointerSize));
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
    __ JumpIfSmi(rax, &failed, Label::kNear);

    __ Drop(2);
    __ Pop(rcx);
    __ SmiToInteger32(rcx, rcx);
    scope.GenerateLeaveFrame();

    __ PopReturnAddressTo(rbx);
    __ incp(rcx);
    __ leap(rsp, Operand(rsp, rcx, times_pointer_size, 0));
    __ PushReturnAddressFrom(rbx);
    __ ret(0);

    __ bind(&failed);
    // Restore target function and new target.
    __ Pop(rdx);
    __ Pop(rdi);
    __ Pop(rax);
    __ SmiToInteger32(rax, rax);
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
  __ subp(Operand(rsp, 0), Immediate(5));
  __ Pushad();
  __ Move(arg_reg_2, ExternalReference::isolate_address(masm->isolate()));
  __ movp(arg_reg_1, Operand(rsp, kNumSafepointRegisters * kPointerSize));
  {  // NOLINT
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(2);
    __ CallCFunction(
        ExternalReference::get_make_code_young_function(masm->isolate()), 2);
  }
  __ Popad();
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
  __ Pushad();
  __ Move(arg_reg_2, ExternalReference::isolate_address(masm->isolate()));
  __ movp(arg_reg_1, Operand(rsp, kNumSafepointRegisters * kPointerSize));
  __ subp(arg_reg_1, Immediate(Assembler::kShortCallInstructionLength));
  {  // NOLINT
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(2);
    __ CallCFunction(
        ExternalReference::get_mark_code_as_executed_function(masm->isolate()),
        2);
  }
  __ Popad();

  // Perform prologue operations usually performed by the young code stub.
  __ PopReturnAddressTo(kScratchRegister);
  __ pushq(rbp);  // Caller's frame pointer.
  __ movp(rbp, rsp);
  __ Push(rsi);  // Callee's context.
  __ Push(rdi);  // Callee's JS Function.
  __ PushReturnAddressFrom(kScratchRegister);

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
    __ Pushad();
    __ CallRuntime(Runtime::kNotifyStubFailure, save_doubles);
    __ Popad();
    // Tear down internal frame.
  }

  __ DropUnderReturnAddress(1);  // Ignore state offset
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
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Pass the deoptimization type to the runtime system.
    __ Push(Smi::FromInt(static_cast<int>(type)));

    __ CallRuntime(Runtime::kNotifyDeoptimized);
    // Tear down internal frame.
  }

  // Get the full codegen state from the stack and untag it.
  __ SmiToInteger32(kScratchRegister, Operand(rsp, kPCOnStackSize));

  // Switch on the state.
  Label not_no_registers, not_tos_rax;
  __ cmpp(kScratchRegister,
          Immediate(static_cast<int>(Deoptimizer::BailoutState::NO_REGISTERS)));
  __ j(not_equal, &not_no_registers, Label::kNear);
  __ ret(1 * kPointerSize);  // Remove state.

  __ bind(&not_no_registers);
  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), rax.code());
  __ movp(rax, Operand(rsp, kPCOnStackSize + kPointerSize));
  __ cmpp(kScratchRegister,
          Immediate(static_cast<int>(Deoptimizer::BailoutState::TOS_REGISTER)));
  __ j(not_equal, &not_tos_rax, Label::kNear);
  __ ret(2 * kPointerSize);  // Remove state, rax.

  __ bind(&not_tos_rax);
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
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : argArray
  //  -- rsp[16] : thisArg
  //  -- rsp[24] : receiver
  // -----------------------------------

  // 1. Load receiver into rdi, argArray into rax (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label no_arg_array, no_this_arg;
    StackArgumentsAccessor args(rsp, rax);
    __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
    __ movp(rbx, rdx);
    __ movp(rdi, args.GetReceiverOperand());
    __ testp(rax, rax);
    __ j(zero, &no_this_arg, Label::kNear);
    {
      __ movp(rdx, args.GetArgumentOperand(1));
      __ cmpp(rax, Immediate(1));
      __ j(equal, &no_arg_array, Label::kNear);
      __ movp(rbx, args.GetArgumentOperand(2));
      __ bind(&no_arg_array);
    }
    __ bind(&no_this_arg);
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ Push(rdx);
    __ PushReturnAddressFrom(rcx);
    __ movp(rax, rbx);
  }

  // ----------- S t a t e -------------
  //  -- rax     : argArray
  //  -- rdi     : receiver
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : thisArg
  // -----------------------------------

  // 2. Make sure the receiver is actually callable.
  Label receiver_not_callable;
  __ JumpIfSmi(rdi, &receiver_not_callable, Label::kNear);
  __ movp(rcx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsCallable));
  __ j(zero, &receiver_not_callable, Label::kNear);

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(rax, Heap::kNullValueRootIndex, &no_arguments, Label::kNear);
  __ JumpIfRoot(rax, Heap::kUndefinedValueRootIndex, &no_arguments,
                Label::kNear);

  // 4a. Apply the receiver to the given argArray (passing undefined for
  // new.target).
  __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver. Since we did not create a frame for
  // Function.prototype.apply() yet, we use a normal Call builtin here.
  __ bind(&no_arguments);
  {
    __ Set(rax, 0);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }

  // 4c. The receiver is not callable, throw an appropriate TypeError.
  __ bind(&receiver_not_callable);
  {
    StackArgumentsAccessor args(rsp, 0);
    __ movp(args.GetReceiverOperand(), rdi);
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // Stack Layout:
  // rsp[0]           : Return address
  // rsp[8]           : Argument n
  // rsp[16]          : Argument n-1
  //  ...
  // rsp[8 * n]       : Argument 1
  // rsp[8 * (n + 1)] : Receiver (callable to call)
  //
  // rax contains the number of arguments, n, not counting the receiver.
  //
  // 1. Make sure we have at least one argument.
  {
    Label done;
    __ testp(rax, rax);
    __ j(not_zero, &done, Label::kNear);
    __ PopReturnAddressTo(rbx);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ PushReturnAddressFrom(rbx);
    __ incp(rax);
    __ bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  {
    StackArgumentsAccessor args(rsp, rax);
    __ movp(rdi, args.GetReceiverOperand());
  }

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  {
    Label loop;
    __ movp(rcx, rax);
    StackArgumentsAccessor args(rsp, rcx);
    __ bind(&loop);
    __ movp(rbx, args.GetArgumentOperand(1));
    __ movp(args.GetArgumentOperand(0), rbx);
    __ decp(rcx);
    __ j(not_zero, &loop);              // While non-zero.
    __ DropUnderReturnAddress(1, rbx);  // Drop one slot under return address.
    __ decp(rax);  // One fewer argument (first argument is new receiver).
  }

  // 4. Call the callable.
  // Since we did not create a frame for Function.prototype.call() yet,
  // we use a normal Call builtin here.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : argumentsList
  //  -- rsp[16] : thisArgument
  //  -- rsp[24] : target
  //  -- rsp[32] : receiver
  // -----------------------------------

  // 1. Load target into rdi (if present), argumentsList into rax (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label done;
    StackArgumentsAccessor args(rsp, rax);
    __ LoadRoot(rdi, Heap::kUndefinedValueRootIndex);
    __ movp(rdx, rdi);
    __ movp(rbx, rdi);
    __ cmpp(rax, Immediate(1));
    __ j(below, &done, Label::kNear);
    __ movp(rdi, args.GetArgumentOperand(1));  // target
    __ j(equal, &done, Label::kNear);
    __ movp(rdx, args.GetArgumentOperand(2));  // thisArgument
    __ cmpp(rax, Immediate(3));
    __ j(below, &done, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(3));  // argumentsList
    __ bind(&done);
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ Push(rdx);
    __ PushReturnAddressFrom(rcx);
    __ movp(rax, rbx);
  }

  // ----------- S t a t e -------------
  //  -- rax     : argumentsList
  //  -- rdi     : target
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : thisArgument
  // -----------------------------------

  // 2. Make sure the target is actually callable.
  Label target_not_callable;
  __ JumpIfSmi(rdi, &target_not_callable, Label::kNear);
  __ movp(rcx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsCallable));
  __ j(zero, &target_not_callable, Label::kNear);

  // 3a. Apply the target to the given argumentsList (passing undefined for
  // new.target).
  __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 3b. The target is not callable, throw an appropriate TypeError.
  __ bind(&target_not_callable);
  {
    StackArgumentsAccessor args(rsp, 0);
    __ movp(args.GetReceiverOperand(), rdi);
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : new.target (optional)
  //  -- rsp[16] : argumentsList
  //  -- rsp[24] : target
  //  -- rsp[32] : receiver
  // -----------------------------------

  // 1. Load target into rdi (if present), argumentsList into rax (if present),
  // new.target into rdx (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label done;
    StackArgumentsAccessor args(rsp, rax);
    __ LoadRoot(rdi, Heap::kUndefinedValueRootIndex);
    __ movp(rdx, rdi);
    __ movp(rbx, rdi);
    __ cmpp(rax, Immediate(1));
    __ j(below, &done, Label::kNear);
    __ movp(rdi, args.GetArgumentOperand(1));  // target
    __ movp(rdx, rdi);                         // new.target defaults to target
    __ j(equal, &done, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(2));  // argumentsList
    __ cmpp(rax, Immediate(3));
    __ j(below, &done, Label::kNear);
    __ movp(rdx, args.GetArgumentOperand(3));  // new.target
    __ bind(&done);
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ PushReturnAddressFrom(rcx);
    __ movp(rax, rbx);
  }

  // ----------- S t a t e -------------
  //  -- rax     : argumentsList
  //  -- rdx     : new.target
  //  -- rdi     : target
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : receiver (undefined)
  // -----------------------------------

  // 2. Make sure the target is actually a constructor.
  Label target_not_constructor;
  __ JumpIfSmi(rdi, &target_not_constructor, Label::kNear);
  __ movp(rcx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsConstructor));
  __ j(zero, &target_not_constructor, Label::kNear);

  // 3. Make sure the target is actually a constructor.
  Label new_target_not_constructor;
  __ JumpIfSmi(rdx, &new_target_not_constructor, Label::kNear);
  __ movp(rcx, FieldOperand(rdx, HeapObject::kMapOffset));
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsConstructor));
  __ j(zero, &new_target_not_constructor, Label::kNear);

  // 4a. Construct the target with the given new.target and argumentsList.
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The target is not a constructor, throw an appropriate TypeError.
  __ bind(&target_not_constructor);
  {
    StackArgumentsAccessor args(rsp, 0);
    __ movp(args.GetReceiverOperand(), rdi);
    __ TailCallRuntime(Runtime::kThrowNotConstructor);
  }

  // 4c. The new.target is not a constructor, throw an appropriate TypeError.
  __ bind(&new_target_not_constructor);
  {
    StackArgumentsAccessor args(rsp, 0);
    __ movp(args.GetReceiverOperand(), rdx);
    __ TailCallRuntime(Runtime::kThrowNotConstructor);
  }
}

void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argc
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------
  Label generic_array_code;

  // Get the InternalArray function.
  __ LoadNativeContextSlot(Context::INTERNAL_ARRAY_FUNCTION_INDEX, rdi);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ movp(rbx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rbx));
    __ Check(not_smi, kUnexpectedInitialMapForInternalArrayFunction);
    __ CmpObjectType(rbx, MAP_TYPE, rcx);
    __ Check(equal, kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // tail call a stub
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

void Builtins::Generate_ArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argc
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------
  Label generic_array_code;

  // Get the Array function.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, rdi);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ movp(rbx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rbx));
    __ Check(not_smi, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(rbx, MAP_TYPE, rcx);
    __ Check(equal, kUnexpectedInitialMapForArrayFunction);
  }

  __ movp(rdx, rdi);
  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(rbx, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

// static
void Builtins::Generate_NumberConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments
  //  -- rdi                 : constructor function
  //  -- rsi                 : context
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // 1. Load the first argument into rbx.
  Label no_arguments;
  {
    StackArgumentsAccessor args(rsp, rax);
    __ testp(rax, rax);
    __ j(zero, &no_arguments, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(1));
  }

  // 2a. Convert the first argument to a number.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ Integer32ToSmi(rax, rax);
    __ EnterBuiltinFrame(rsi, rdi, rax);
    __ movp(rax, rbx);
    __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(rsi, rdi, rbx);  // Argc popped to rbx.
    __ SmiToInteger32(rbx, rbx);
  }

  {
    // Drop all arguments including the receiver.
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rbx, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(rcx);
    __ Ret();
  }

  // 2b. No arguments, return +0 (already in rax).
  __ bind(&no_arguments);
  __ ret(1 * kPointerSize);
}

// static
void Builtins::Generate_NumberConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments
  //  -- rdi                 : constructor function
  //  -- rdx                 : new target
  //  -- rsi                 : context
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Store argc in r8.
  __ Integer32ToSmi(r8, rax);

  // 2. Load the first argument into rbx.
  {
    StackArgumentsAccessor args(rsp, rax);
    Label no_arguments, done;
    __ testp(rax, rax);
    __ j(zero, &no_arguments, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(1));
    __ jmp(&done, Label::kNear);
    __ bind(&no_arguments);
    __ Move(rbx, Smi::kZero);
    __ bind(&done);
  }

  // 3. Make sure rbx is a number.
  {
    Label done_convert;
    __ JumpIfSmi(rbx, &done_convert);
    __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(equal, &done_convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterBuiltinFrame(rsi, rdi, r8);
      __ Push(rdx);
      __ Move(rax, rbx);
      __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
      __ Move(rbx, rax);
      __ Pop(rdx);
      __ LeaveBuiltinFrame(rsi, rdi, r8);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ cmpp(rdx, rdi);
  __ j(not_equal, &new_object);

  // 5. Allocate a JSValue wrapper for the number.
  __ AllocateJSValue(rax, rdi, rbx, rcx, &new_object);
  __ jmp(&drop_frame_and_ret, Label::kNear);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ EnterBuiltinFrame(rsi, rdi, r8);
    __ Push(rbx);  // the first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(FieldOperand(rax, JSValue::kValueOffset));
    __ LeaveBuiltinFrame(rsi, rdi, r8);
  }

  __ bind(&drop_frame_and_ret);
  {
    // Drop all arguments including the receiver.
    __ PopReturnAddressTo(rcx);
    __ SmiToInteger32(r8, r8);
    __ leap(rsp, Operand(rsp, r8, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(rcx);
    __ Ret();
  }
}

// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments
  //  -- rdi                 : constructor function
  //  -- rsi                 : context
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // 1. Load the first argument into rax.
  Label no_arguments;
  {
    StackArgumentsAccessor args(rsp, rax);
    __ Integer32ToSmi(r8, rax);  // Store argc in r8.
    __ testp(rax, rax);
    __ j(zero, &no_arguments, Label::kNear);
    __ movp(rax, args.GetArgumentOperand(1));
  }

  // 2a. At least one argument, return rax if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label drop_frame_and_ret, to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(rax, &to_string, Label::kNear);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, rdx);
    __ j(above, &to_string, Label::kNear);
    __ j(equal, &symbol_descriptive_string, Label::kNear);
    __ jmp(&drop_frame_and_ret, Label::kNear);
  }

  // 2b. No arguments, return the empty string (and pop the receiver).
  __ bind(&no_arguments);
  {
    __ LoadRoot(rax, Heap::kempty_stringRootIndex);
    __ ret(1 * kPointerSize);
  }

  // 3a. Convert rax to a string.
  __ bind(&to_string);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ EnterBuiltinFrame(rsi, rdi, r8);
    __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(rsi, rdi, r8);
  }
  __ jmp(&drop_frame_and_ret, Label::kNear);

  // 3b. Convert symbol in rax to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ PopReturnAddressTo(rcx);
    __ SmiToInteger32(r8, r8);
    __ leap(rsp, Operand(rsp, r8, times_pointer_size, kPointerSize));
    __ Push(rax);
    __ PushReturnAddressFrom(rcx);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString);
  }

  __ bind(&drop_frame_and_ret);
  {
    // Drop all arguments including the receiver.
    __ PopReturnAddressTo(rcx);
    __ SmiToInteger32(r8, r8);
    __ leap(rsp, Operand(rsp, r8, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(rcx);
    __ Ret();
  }
}

// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments
  //  -- rdi                 : constructor function
  //  -- rdx                 : new target
  //  -- rsi                 : context
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Store argc in r8.
  __ Integer32ToSmi(r8, rax);

  // 2. Load the first argument into rbx.
  {
    StackArgumentsAccessor args(rsp, rax);
    Label no_arguments, done;
    __ testp(rax, rax);
    __ j(zero, &no_arguments, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(1));
    __ jmp(&done, Label::kNear);
    __ bind(&no_arguments);
    __ LoadRoot(rbx, Heap::kempty_stringRootIndex);
    __ bind(&done);
  }

  // 3. Make sure rbx is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(rbx, &convert, Label::kNear);
    __ CmpObjectType(rbx, FIRST_NONSTRING_TYPE, rcx);
    __ j(below, &done_convert);
    __ bind(&convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterBuiltinFrame(rsi, rdi, r8);
      __ Push(rdx);
      __ Move(rax, rbx);
      __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
      __ Move(rbx, rax);
      __ Pop(rdx);
      __ LeaveBuiltinFrame(rsi, rdi, r8);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ cmpp(rdx, rdi);
  __ j(not_equal, &new_object);

  // 5. Allocate a JSValue wrapper for the string.
  __ AllocateJSValue(rax, rdi, rbx, rcx, &new_object);
  __ jmp(&drop_frame_and_ret, Label::kNear);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ EnterBuiltinFrame(rsi, rdi, r8);
    __ Push(rbx);  // the first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(FieldOperand(rax, JSValue::kValueOffset));
    __ LeaveBuiltinFrame(rsi, rdi, r8);
  }

  __ bind(&drop_frame_and_ret);
  {
    // Drop all arguments including the receiver.
    __ PopReturnAddressTo(rcx);
    __ SmiToInteger32(r8, r8);
    __ leap(rsp, Operand(rsp, r8, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(rcx);
    __ Ret();
  }
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ pushq(rbp);
  __ movp(rbp, rsp);

  // Store the arguments adaptor context sentinel.
  __ Push(Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));

  // Push the function on the stack.
  __ Push(rdi);

  // Preserve the number of arguments on the stack. Must preserve rax,
  // rbx and rcx because these registers are used when copying the
  // arguments and the receiver.
  __ Integer32ToSmi(r8, rax);
  __ Push(r8);
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // Retrieve the number of arguments from the stack. Number is a Smi.
  __ movp(rbx, Operand(rbp, ArgumentsAdaptorFrameConstants::kLengthOffset));

  // Leave the frame.
  __ movp(rsp, rbp);
  __ popq(rbp);

  // Remove caller arguments from the stack.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ leap(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);
}

// static
void Builtins::Generate_AllocateInNewSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rdx    : requested object size (untagged)
  //  -- rsp[0] : return address
  // -----------------------------------
  __ Integer32ToSmi(rdx, rdx);
  __ PopReturnAddressTo(rcx);
  __ Push(rdx);
  __ PushReturnAddressFrom(rcx);
  __ Move(rsi, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rdx    : requested object size (untagged)
  //  -- rsp[0] : return address
  // -----------------------------------
  __ Integer32ToSmi(rdx, rdx);
  __ PopReturnAddressTo(rcx);
  __ Push(rdx);
  __ Push(Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ PushReturnAddressFrom(rcx);
  __ Move(rsi, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

// static
void Builtins::Generate_Abort(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rdx    : message_id as Smi
  //  -- rsp[0] : return address
  // -----------------------------------
  __ PopReturnAddressTo(rcx);
  __ Push(rdx);
  __ PushReturnAddressFrom(rcx);
  __ Move(rsi, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbort);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : actual number of arguments
  //  -- rbx : expected number of arguments
  //  -- rdx : new target (passed through to callee)
  //  -- rdi : function (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->arguments_adaptors(), 1);

  Label enough, too_few;
  __ cmpp(rax, rbx);
  __ j(less, &too_few);
  __ cmpp(rbx, Immediate(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ j(equal, &dont_adapt_arguments);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    // The registers rcx and r8 will be modified. The register rbx is only read.
    Generate_StackOverflowCheck(masm, rbx, rcx, &stack_overflow);

    // Copy receiver and all expected arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ leap(rax, Operand(rbp, rax, times_pointer_size, offset));
    __ Set(r8, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incp(r8);
    __ Push(Operand(rax, 0));
    __ subp(rax, Immediate(kPointerSize));
    __ cmpp(r8, rbx);
    __ j(less, &copy);
    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);

    EnterArgumentsAdaptorFrame(masm);
    // The registers rcx and r8 will be modified. The register rbx is only read.
    Generate_StackOverflowCheck(masm, rbx, rcx, &stack_overflow);

    // Copy receiver and all actual arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ leap(rdi, Operand(rbp, rax, times_pointer_size, offset));
    __ Set(r8, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incp(r8);
    __ Push(Operand(rdi, 0));
    __ subp(rdi, Immediate(kPointerSize));
    __ cmpp(r8, rax);
    __ j(less, &copy);

    // Fill remaining expected arguments with undefined values.
    Label fill;
    __ LoadRoot(kScratchRegister, Heap::kUndefinedValueRootIndex);
    __ bind(&fill);
    __ incp(r8);
    __ Push(kScratchRegister);
    __ cmpp(r8, rbx);
    __ j(less, &fill);

    // Restore function pointer.
    __ movp(rdi, Operand(rbp, ArgumentsAdaptorFrameConstants::kFunctionOffset));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ movp(rax, rbx);
  // rax : expected number of arguments
  // rdx : new target (passed through to callee)
  // rdi : function (passed through to callee)
  __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
  __ call(rcx);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Leave frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ ret(0);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
  __ jmp(rcx);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();
  }
}

// static
void Builtins::Generate_Apply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argumentsList
  //  -- rdi    : target
  //  -- rdx    : new.target (checked to be constructor or undefined)
  //  -- rsp[0] : return address.
  //  -- rsp[8] : thisArgument
  // -----------------------------------

  // Create the list of arguments from the array-like argumentsList.
  {
    Label create_arguments, create_array, create_holey_array, create_runtime,
        done_create;
    __ JumpIfSmi(rax, &create_runtime);

    // Load the map of argumentsList into rcx.
    __ movp(rcx, FieldOperand(rax, HeapObject::kMapOffset));

    // Load native context into rbx.
    __ movp(rbx, NativeContextOperand());

    // Check if argumentsList is an (unmodified) arguments object.
    __ cmpp(rcx, ContextOperand(rbx, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    __ j(equal, &create_arguments);
    __ cmpp(rcx, ContextOperand(rbx, Context::STRICT_ARGUMENTS_MAP_INDEX));
    __ j(equal, &create_arguments);

    // Check if argumentsList is a fast JSArray.
    __ CmpInstanceType(rcx, JS_ARRAY_TYPE);
    __ j(equal, &create_array);

    // Ask the runtime to create the list (actually a FixedArray).
    __ bind(&create_runtime);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(rdi);
      __ Push(rdx);
      __ Push(rax);
      __ CallRuntime(Runtime::kCreateListFromArrayLike);
      __ Pop(rdx);
      __ Pop(rdi);
      __ SmiToInteger32(rbx, FieldOperand(rax, FixedArray::kLengthOffset));
    }
    __ jmp(&done_create);

    // Try to create the list from an arguments object.
    __ bind(&create_arguments);
    __ movp(rbx, FieldOperand(rax, JSArgumentsObject::kLengthOffset));
    __ movp(rcx, FieldOperand(rax, JSObject::kElementsOffset));
    __ cmpp(rbx, FieldOperand(rcx, FixedArray::kLengthOffset));
    __ j(not_equal, &create_runtime);
    __ SmiToInteger32(rbx, rbx);
    __ movp(rax, rcx);
    __ jmp(&done_create);

    __ bind(&create_holey_array);
    // For holey JSArrays we need to check that the array prototype chain
    // protector is intact and our prototype is the Array.prototype actually.
    __ movp(rcx, FieldOperand(rax, HeapObject::kMapOffset));
    __ movp(rcx, FieldOperand(rcx, Map::kPrototypeOffset));
    __ cmpp(rcx, ContextOperand(rbx, Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
    __ j(not_equal, &create_runtime);
    __ LoadRoot(rcx, Heap::kArrayProtectorRootIndex);
    __ Cmp(FieldOperand(rcx, PropertyCell::kValueOffset),
           Smi::FromInt(Isolate::kProtectorValid));
    __ j(not_equal, &create_runtime);
    __ SmiToInteger32(rbx, FieldOperand(rax, JSArray::kLengthOffset));
    __ movp(rax, FieldOperand(rax, JSArray::kElementsOffset));
    __ jmp(&done_create);

    // Try to create the list from a JSArray object.
    __ bind(&create_array);
    __ movzxbp(rcx, FieldOperand(rcx, Map::kBitField2Offset));
    __ DecodeField<Map::ElementsKindBits>(rcx);
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    __ cmpl(rcx, Immediate(FAST_HOLEY_SMI_ELEMENTS));
    __ j(equal, &create_holey_array);
    __ cmpl(rcx, Immediate(FAST_HOLEY_ELEMENTS));
    __ j(equal, &create_holey_array);
    __ j(above, &create_runtime);
    __ SmiToInteger32(rbx, FieldOperand(rax, JSArray::kLengthOffset));
    __ movp(rax, FieldOperand(rax, JSArray::kElementsOffset));

    __ bind(&done_create);
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(kScratchRegister, Heap::kRealStackLimitRootIndex);
    __ movp(rcx, rsp);
    // Make rcx the space we have left. The stack might already be overflowed
    // here which will cause rcx to become negative.
    __ subp(rcx, kScratchRegister);
    __ sarp(rcx, Immediate(kPointerSizeLog2));
    // Check if the arguments will overflow the stack.
    __ cmpp(rcx, rbx);
    __ j(greater, &done, Label::kNear);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // ----------- S t a t e -------------
  //  -- rdi    : target
  //  -- rax    : args (a FixedArray built from argumentsList)
  //  -- rbx    : len (number of elements to push from args)
  //  -- rdx    : new.target (checked to be constructor or undefined)
  //  -- rsp[0] : return address.
  //  -- rsp[8] : thisArgument
  // -----------------------------------

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    __ PopReturnAddressTo(r8);
    __ Set(rcx, 0);
    Label done, push, loop;
    __ bind(&loop);
    __ cmpl(rcx, rbx);
    __ j(equal, &done, Label::kNear);
    // Turn the hole into undefined as we go.
    __ movp(r9, FieldOperand(rax, rcx, times_pointer_size,
                             FixedArray::kHeaderSize));
    __ CompareRoot(r9, Heap::kTheHoleValueRootIndex);
    __ j(not_equal, &push, Label::kNear);
    __ LoadRoot(r9, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ Push(r9);
    __ incl(rcx);
    __ jmp(&loop);
    __ bind(&done);
    __ PushReturnAddressFrom(r8);
    __ Move(rax, rcx);
  }

  // Dispatch to Call or Construct depending on whether new.target is undefined.
  {
    __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
    __ j(equal, masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_ForwardVarargs(MacroAssembler* masm,
                                       Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (for [[Construct]] calls)
  //  -- rdi : the target to call (can be any Object)
  //  -- rcx : start index (to support rest parameters)
  // -----------------------------------

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ movp(rbx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ cmpp(Operand(rbx, CommonFrameConstants::kContextOrFrameTypeOffset),
          Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &arguments_adaptor, Label::kNear);
  {
    __ movp(r8, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
    __ movp(r8, FieldOperand(r8, JSFunction::kSharedFunctionInfoOffset));
    __ LoadSharedFunctionInfoSpecialField(
        r8, r8, SharedFunctionInfo::kFormalParameterCountOffset);
    __ movp(rbx, rbp);
  }
  __ jmp(&arguments_done, Label::kNear);
  __ bind(&arguments_adaptor);
  {
    __ SmiToInteger32(
        r8, Operand(rbx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  }
  __ bind(&arguments_done);

  Label stack_done, stack_overflow;
  __ subl(r8, rcx);
  __ j(less_equal, &stack_done);
  {
    // Check for stack overflow.
    Generate_StackOverflowCheck(masm, r8, rcx, &stack_overflow, Label::kNear);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ addl(rax, r8);
      __ PopReturnAddressTo(rcx);
      __ bind(&loop);
      {
        StackArgumentsAccessor args(rbx, r8, ARGUMENTS_DONT_CONTAIN_RECEIVER);
        __ Push(args.GetArgumentOperand(0));
        __ decl(r8);
        __ j(not_zero, &loop);
      }
      __ PushReturnAddressFrom(rcx);
    }
  }
  __ jmp(&stack_done, Label::kNear);
  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ bind(&stack_done);

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

  // Prepare for tail call only if ES2015 tail call elimination is active.
  Label done;
  ExternalReference is_tail_call_elimination_enabled =
      ExternalReference::is_tail_call_elimination_enabled_address(
          masm->isolate());
  __ Move(kScratchRegister, is_tail_call_elimination_enabled);
  __ cmpb(Operand(kScratchRegister, 0), Immediate(0));
  __ j(equal, &done);

  // Drop possible interpreter handler/stub frame.
  {
    Label no_interpreter_frame;
    __ cmpp(Operand(rbp, CommonFrameConstants::kContextOrFrameTypeOffset),
            Immediate(StackFrame::TypeToMarker(StackFrame::STUB)));
    __ j(not_equal, &no_interpreter_frame, Label::kNear);
    __ movp(rbp, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
    __ bind(&no_interpreter_frame);
  }

  // Check if next frame is an arguments adaptor frame.
  Register caller_args_count_reg = scratch1;
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ movp(scratch2, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ cmpp(Operand(scratch2, CommonFrameConstants::kContextOrFrameTypeOffset),
          Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &no_arguments_adaptor, Label::kNear);

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ movp(rbp, scratch2);
  __ SmiToInteger32(
      caller_args_count_reg,
      Operand(rbp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ jmp(&formal_parameter_count_loaded, Label::kNear);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ movp(scratch1, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ movp(scratch1,
          FieldOperand(scratch1, JSFunction::kSharedFunctionInfoOffset));
  __ LoadSharedFunctionInfoSpecialField(
      caller_args_count_reg, scratch1,
      SharedFunctionInfo::kFormalParameterCountOffset);

  __ bind(&formal_parameter_count_loaded);

  ParameterCount callee_args_count(args_reg);
  __ PrepareForTailCall(callee_args_count, caller_args_count_reg, scratch2,
                        scratch3, ReturnAddressState::kOnStack);
  __ bind(&done);
}
}  // namespace

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode,
                                     TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the function to call (checked to be a JSFunction)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);
  __ AssertFunction(rdi);

  // ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ movp(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ testb(FieldOperand(rdx, SharedFunctionInfo::kFunctionKindByteOffset),
           Immediate(SharedFunctionInfo::kClassConstructorBitsWithinByte));
  __ j(not_zero, &class_constructor);

  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the shared function info.
  //  -- rdi : the function to call (checked to be a JSFunction)
  // -----------------------------------

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ testb(FieldOperand(rdx, SharedFunctionInfo::kNativeByteOffset),
           Immediate((1 << SharedFunctionInfo::kNativeBitWithinByte) |
                     (1 << SharedFunctionInfo::kStrictModeBitWithinByte)));
  __ j(not_zero, &done_convert);
  {
    // ----------- S t a t e -------------
    //  -- rax : the number of arguments (not including the receiver)
    //  -- rdx : the shared function info.
    //  -- rdi : the function to call (checked to be a JSFunction)
    //  -- rsi : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(rcx);
    } else {
      Label convert_to_object, convert_receiver;
      __ movp(rcx, args.GetReceiverOperand());
      __ JumpIfSmi(rcx, &convert_to_object, Label::kNear);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CmpObjectType(rcx, FIRST_JS_RECEIVER_TYPE, rbx);
      __ j(above_equal, &done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(rcx, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy, Label::kNear);
        __ JumpIfNotRoot(rcx, Heap::kNullValueRootIndex, &convert_to_object,
                         Label::kNear);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(rcx);
        }
        __ jmp(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ Integer32ToSmi(rax, rax);
        __ Push(rax);
        __ Push(rdi);
        __ movp(rax, rcx);
        __ Push(rsi);
        __ Call(masm->isolate()->builtins()->ToObject(),
                RelocInfo::CODE_TARGET);
        __ Pop(rsi);
        __ movp(rcx, rax);
        __ Pop(rdi);
        __ Pop(rax);
        __ SmiToInteger32(rax, rax);
      }
      __ movp(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ movp(args.GetReceiverOperand(), rcx);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the shared function info.
  //  -- rdi : the function to call (checked to be a JSFunction)
  //  -- rsi : the function context.
  // -----------------------------------

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, rax, rbx, rcx, r8);
  }

  __ LoadSharedFunctionInfoSpecialField(
      rbx, rdx, SharedFunctionInfo::kFormalParameterCountOffset);
  ParameterCount actual(rax);
  ParameterCount expected(rbx);

  __ InvokeFunctionCode(rdi, no_reg, expected, actual, JUMP_FUNCTION,
                        CheckDebugStepCallWrapper());

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ Push(rdi);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : new.target (only in case of [[Construct]])
  //  -- rdi : target (checked to be a JSBoundFunction)
  // -----------------------------------

  // Load [[BoundArguments]] into rcx and length of that into rbx.
  Label no_bound_arguments;
  __ movp(rcx, FieldOperand(rdi, JSBoundFunction::kBoundArgumentsOffset));
  __ SmiToInteger32(rbx, FieldOperand(rcx, FixedArray::kLengthOffset));
  __ testl(rbx, rbx);
  __ j(zero, &no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- rax : the number of arguments (not including the receiver)
    //  -- rdx : new.target (only in case of [[Construct]])
    //  -- rdi : target (checked to be a JSBoundFunction)
    //  -- rcx : the [[BoundArguments]] (implemented as FixedArray)
    //  -- rbx : the number of [[BoundArguments]] (checked to be non-zero)
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ leap(kScratchRegister, Operand(rbx, times_pointer_size, 0));
      __ subp(rsp, kScratchRegister);
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRoot(rsp, Heap::kRealStackLimitRootIndex);
      __ j(greater, &done, Label::kNear);  // Signed comparison.
      // Restore the stack pointer.
      __ leap(rsp, Operand(rsp, rbx, times_pointer_size, 0));
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Adjust effective number of arguments to include return address.
    __ incl(rax);

    // Relocate arguments and return address down the stack.
    {
      Label loop;
      __ Set(rcx, 0);
      __ leap(rbx, Operand(rsp, rbx, times_pointer_size, 0));
      __ bind(&loop);
      __ movp(kScratchRegister, Operand(rbx, rcx, times_pointer_size, 0));
      __ movp(Operand(rsp, rcx, times_pointer_size, 0), kScratchRegister);
      __ incl(rcx);
      __ cmpl(rcx, rax);
      __ j(less, &loop);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ movp(rcx, FieldOperand(rdi, JSBoundFunction::kBoundArgumentsOffset));
      __ SmiToInteger32(rbx, FieldOperand(rcx, FixedArray::kLengthOffset));
      __ bind(&loop);
      __ decl(rbx);
      __ movp(kScratchRegister, FieldOperand(rcx, rbx, times_pointer_size,
                                             FixedArray::kHeaderSize));
      __ movp(Operand(rsp, rax, times_pointer_size, 0), kScratchRegister);
      __ leal(rax, Operand(rax, 1));
      __ j(greater, &loop);
    }

    // Adjust effective number of arguments (rax contains the number of
    // arguments from the call plus return address plus the number of
    // [[BoundArguments]]), so we need to subtract one for the return address.
    __ decl(rax);
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm,
                                              TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(rdi);

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, rax, rbx, rcx, r8);
  }

  // Patch the receiver to [[BoundThis]].
  StackArgumentsAccessor args(rsp, rax);
  __ movp(rbx, FieldOperand(rdi, JSBoundFunction::kBoundThisOffset));
  __ movp(args.GetReceiverOperand(), rbx);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ movp(rdi, FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Load(rcx,
          ExternalReference(Builtins::kCall_ReceiverIsAny, masm->isolate()));
  __ leap(rcx, FieldOperand(rcx, Code::kHeaderSize));
  __ jmp(rcx);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode,
                             TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the target to call (can be any Object)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(rdi, &non_callable);
  __ bind(&non_smi);
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(equal, masm->isolate()->builtins()->CallFunction(mode, tail_call_mode),
       RelocInfo::CODE_TARGET);
  __ CmpInstanceType(rcx, JS_BOUND_FUNCTION_TYPE);
  __ j(equal, masm->isolate()->builtins()->CallBoundFunction(tail_call_mode),
       RelocInfo::CODE_TARGET);

  // Check if target has a [[Call]] internal method.
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsCallable));
  __ j(zero, &non_callable);

  __ CmpInstanceType(rcx, JS_PROXY_TYPE);
  __ j(not_equal, &non_function);

  // 0. Prepare for tail call if necessary.
  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, rax, rbx, rcx, r8);
  }

  // 1. Runtime fallback for Proxy [[Call]].
  __ PopReturnAddressTo(kScratchRegister);
  __ Push(rdi);
  __ PushReturnAddressFrom(kScratchRegister);
  // Increase the arguments size to include the pushed function and the
  // existing receiver on the stack.
  __ addp(rax, Immediate(2));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyCall, masm->isolate()));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ movp(args.GetReceiverOperand(), rdi);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, rdi);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdi);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

static void CheckSpreadAndPushToStack(MacroAssembler* masm) {
  Label runtime_call, push_args;
  // Load the spread argument into rbx.
  __ movp(rbx, Operand(rsp, kPointerSize));
  __ JumpIfSmi(rbx, &runtime_call);
  // Load the map of the spread into r15.
  __ movp(r15, FieldOperand(rbx, HeapObject::kMapOffset));
  // Load native context into r14.
  __ movp(r14, NativeContextOperand());

  // Check that the spread is an array.
  __ CmpInstanceType(r15, JS_ARRAY_TYPE);
  __ j(not_equal, &runtime_call);

  // Check that we have the original ArrayPrototype.
  __ movp(rcx, FieldOperand(r15, Map::kPrototypeOffset));
  __ cmpp(rcx, ContextOperand(r14, Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
  __ j(not_equal, &runtime_call);

  // Check that the ArrayPrototype hasn't been modified in a way that would
  // affect iteration.
  __ LoadRoot(rcx, Heap::kArrayIteratorProtectorRootIndex);
  __ Cmp(FieldOperand(rcx, PropertyCell::kValueOffset),
         Smi::FromInt(Isolate::kProtectorValid));
  __ j(not_equal, &runtime_call);

  // Check that the map of the initial array iterator hasn't changed.
  __ movp(rcx,
          ContextOperand(r14, Context::INITIAL_ARRAY_ITERATOR_PROTOTYPE_INDEX));
  __ movp(rcx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ cmpp(rcx, ContextOperand(
                   r14, Context::INITIAL_ARRAY_ITERATOR_PROTOTYPE_MAP_INDEX));
  __ j(not_equal, &runtime_call);

  // For FastPacked kinds, iteration will have the same effect as simply
  // accessing each property in order.
  Label no_protector_check;
  __ movzxbp(rcx, FieldOperand(r15, Map::kBitField2Offset));
  __ DecodeField<Map::ElementsKindBits>(rcx);
  __ cmpp(rcx, Immediate(FAST_HOLEY_ELEMENTS));
  __ j(above, &runtime_call);
  // For non-FastHoley kinds, we can skip the protector check.
  __ cmpp(rcx, Immediate(FAST_SMI_ELEMENTS));
  __ j(equal, &no_protector_check);
  __ cmpp(rcx, Immediate(FAST_ELEMENTS));
  __ j(equal, &no_protector_check);
  // Check the ArrayProtector cell.
  __ LoadRoot(rcx, Heap::kArrayProtectorRootIndex);
  __ Cmp(FieldOperand(rcx, PropertyCell::kValueOffset),
         Smi::FromInt(Isolate::kProtectorValid));
  __ j(not_equal, &runtime_call);

  __ bind(&no_protector_check);
  // Load the FixedArray backing store, but use the length from the array.
  __ SmiToInteger32(r9, FieldOperand(rbx, JSArray::kLengthOffset));
  __ movp(rbx, FieldOperand(rbx, JSArray::kElementsOffset));
  __ jmp(&push_args);

  __ bind(&runtime_call);
  {
    // Call the builtin for the result of the spread.
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdi);  // target
    __ Push(rdx);  // new target
    __ Integer32ToSmi(rax, rax);
    __ Push(rax);  // nargs
    __ Push(rbx);
    __ CallRuntime(Runtime::kSpreadIterableFixed);
    __ movp(rbx, rax);
    __ Pop(rax);  // nargs
    __ SmiToInteger32(rax, rax);
    __ Pop(rdx);  // new target
    __ Pop(rdi);  // target
  }

  {
    // Calculate the new nargs including the result of the spread.
    __ SmiToInteger32(r9, FieldOperand(rbx, FixedArray::kLengthOffset));

    __ bind(&push_args);
    // rax += r9 - 1. Subtract 1 for the spread itself.
    __ leap(rax, Operand(rax, r9, times_1, -1));
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(kScratchRegister, Heap::kRealStackLimitRootIndex);
    __ movp(rcx, rsp);
    // Make rcx the space we have left. The stack might already be overflowed
    // here which will cause rcx to become negative.
    __ subp(rcx, kScratchRegister);
    __ sarp(rcx, Immediate(kPointerSizeLog2));
    // Check if the arguments will overflow the stack.
    __ cmpp(rcx, r9);
    __ j(greater, &done, Label::kNear);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // Put the evaluated spread onto the stack as additional arguments.
  {
    // Pop the return address and spread argument.
    __ PopReturnAddressTo(r8);
    __ Pop(rcx);

    __ Set(rcx, 0);
    Label done, push, loop;
    __ bind(&loop);
    __ cmpl(rcx, r9);
    __ j(equal, &done, Label::kNear);
    __ movp(kScratchRegister, FieldOperand(rbx, rcx, times_pointer_size,
                                           FixedArray::kHeaderSize));
    __ CompareRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
    __ j(not_equal, &push, Label::kNear);
    __ LoadRoot(kScratchRegister, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ Push(kScratchRegister);
    __ incl(rcx);
    __ jmp(&loop);
    __ bind(&done);
    __ PushReturnAddressFrom(r8);
  }
}

// static
void Builtins::Generate_CallWithSpread(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the target to call (can be any Object)
  // -----------------------------------

  // CheckSpreadAndPushToStack will push rdx to save it.
  __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
  CheckSpreadAndPushToStack(masm);
  __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                            TailCallMode::kDisallow),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (checked to be a constructor)
  //  -- rdi : the constructor to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(rdi);

  // Calling convention for function specific ConstructStubs require
  // rbx to contain either an AllocationSite or undefined.
  __ LoadRoot(rbx, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movp(rcx, FieldOperand(rcx, SharedFunctionInfo::kConstructStubOffset));
  __ leap(rcx, FieldOperand(rcx, Code::kHeaderSize));
  __ jmp(rcx);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (checked to be a constructor)
  //  -- rdi : the constructor to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(rdi);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label done;
    __ cmpp(rdi, rdx);
    __ j(not_equal, &done, Label::kNear);
    __ movp(rdx,
            FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&done);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ movp(rdi, FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Load(rcx, ExternalReference(Builtins::kConstruct, masm->isolate()));
  __ leap(rcx, FieldOperand(rcx, Code::kHeaderSize));
  __ jmp(rcx);
}

// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the constructor to call (checked to be a JSProxy)
  //  -- rdx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Call into the Runtime for Proxy [[Construct]].
  __ PopReturnAddressTo(kScratchRegister);
  __ Push(rdi);
  __ Push(rdx);
  __ PushReturnAddressFrom(kScratchRegister);
  // Include the pushed new_target, constructor and the receiver.
  __ addp(rax, Immediate(3));
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyConstruct, masm->isolate()));
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- rdi : the constructor to call (can be any Object)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);

  // Check if target is a Smi.
  Label non_constructor;
  __ JumpIfSmi(rdi, &non_constructor, Label::kNear);

  // Dispatch based on instance type.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(equal, masm->isolate()->builtins()->ConstructFunction(),
       RelocInfo::CODE_TARGET);

  // Check if target has a [[Construct]] internal method.
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsConstructor));
  __ j(zero, &non_constructor, Label::kNear);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ CmpInstanceType(rcx, JS_BOUND_FUNCTION_TYPE);
  __ j(equal, masm->isolate()->builtins()->ConstructBoundFunction(),
       RelocInfo::CODE_TARGET);

  // Only dispatch to proxies after checking whether they are constructors.
  __ CmpInstanceType(rcx, JS_PROXY_TYPE);
  __ j(equal, masm->isolate()->builtins()->ConstructProxy(),
       RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ movp(args.GetReceiverOperand(), rdi);
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, rdi);
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
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- rdi : the constructor to call (can be any Object)
  // -----------------------------------

  CheckSpreadAndPushToStack(masm);
  __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ movp(rax, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
    __ movp(rax, Operand(rax, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ movp(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  }

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ Push(rax);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  Label skip;
  // If the code object is null, just return to the caller.
  __ cmpp(rax, Immediate(0));
  __ j(not_equal, &skip, Label::kNear);
  __ ret(0);

  __ bind(&skip);

  // Drop any potential handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  if (has_handler_frame) {
    __ leave();
  }

  // Load deoptimization data from the code object.
  __ movp(rbx, Operand(rax, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  __ SmiToInteger32(
      rbx, Operand(rbx, FixedArray::OffsetOfElementAt(
                            DeoptimizationInputData::kOsrPcOffsetIndex) -
                            kHeapObjectTag));

  // Compute the target address = code_obj + header_size + osr_offset
  __ leap(rax, Operand(rax, rbx, times_1, Code::kHeaderSize - kHeapObjectTag));

  // Overwrite the return address on the stack.
  __ movq(StackOperandForReturnAddress(0), rax);

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
    constexpr Register gp_regs[]{rax, rbx, rcx, rdx, rsi, rdi};
    constexpr XMMRegister xmm_regs[]{xmm1, xmm2, xmm3, xmm4, xmm5, xmm6};

    for (auto reg : gp_regs) {
      __ Push(reg);
    }
    __ subp(rsp, Immediate(16 * arraysize(xmm_regs)));
    for (int i = 0, e = arraysize(xmm_regs); i < e; ++i) {
      __ movdqu(Operand(rsp, 16 * i), xmm_regs[i]);
    }

    // Initialize rsi register with kZero, CEntryStub will use it to set the
    // current context on the isolate.
    __ Move(rsi, Smi::kZero);
    __ CallRuntime(Runtime::kWasmCompileLazy);
    // Store returned instruction start in r11.
    __ leap(r11, FieldOperand(rax, Code::kHeaderSize));

    // Restore registers.
    for (int i = arraysize(xmm_regs) - 1; i >= 0; --i) {
      __ movdqu(xmm_regs[i], Operand(rsp, 16 * i));
    }
    __ addp(rsp, Immediate(16 * arraysize(xmm_regs)));
    for (int i = arraysize(gp_regs) - 1; i >= 0; --i) {
      __ Pop(gp_regs[i]);
    }
  }
  // Now jump to the instructions of the returned code object.
  __ jmp(r11);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64

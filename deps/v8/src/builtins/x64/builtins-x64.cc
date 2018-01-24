// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/code-factory.h"
#include "src/counters.h"
#include "src/deoptimizer.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/objects-inl.h"
#include "src/objects/debug-objects.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
  __ LoadAddress(rbx, ExternalReference(address, masm->isolate()));
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
  //  -- rax                 : number of arguments excluding receiver
  //  -- rbx                 : entry point
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

  // CEntryStub expects rax to contain the number of arguments including the
  // receiver and the extra arguments.
  __ addp(rax, Immediate(BuiltinExitFrameConstants::kNumExtraArgsWithReceiver));

  // Unconditionally insert argc, target and new target as extra arguments. They
  // will be used by stack frame iterators when constructing the stack trace.
  __ PopReturnAddressTo(kScratchRegister);
  __ Integer32ToSmi(rax, rax);
  __ PushRoot(Heap::kTheHoleValueRootIndex);  // Padding.
  __ Push(rax);
  __ SmiToInteger32(rax, rax);
  __ Push(rdi);
  __ Push(rdx);
  __ PushReturnAddressFrom(kScratchRegister);

  // Jump to the C entry runtime stub directly here instead of using
  // JumpToExternalReference because rbx is loaded by Generate_adaptor.
  CEntryStub ces(masm->isolate(), 1, kDontSaveFPRegs, kArgvOnStack,
                 exit_frame_type == Builtins::BUILTIN_EXIT);
  __ jmp(ces.GetCode(), RelocInfo::CODE_TARGET);
}
}  // namespace

void Builtins::Generate_AdaptorWithExitFrame(MacroAssembler* masm) {
  AdaptorWithExitFrameType(masm, EXIT);
}

void Builtins::Generate_AdaptorWithBuiltinExitFrame(MacroAssembler* masm) {
  AdaptorWithExitFrameType(masm, BUILTIN_EXIT);
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
    __ InvokeFunction(rdi, rdx, actual, CALL_FUNCTION);

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
    __ testl(FieldOperand(rbx, SharedFunctionInfo::kCompilerHintsOffset),
             Immediate(SharedFunctionInfo::kDerivedConstructorMask));
    __ j(not_zero, &not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
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
    __ InvokeFunction(rdi, rdx, actual, CALL_FUNCTION);

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

    // The result is now neither undefined nor an object.
    __ bind(&other_result);
    __ movp(rbx, Operand(rbp, ConstructFrameConstants::kConstructorOffset));
    __ movp(rbx, FieldOperand(rbx, JSFunction::kSharedFunctionInfoOffset));
    __ testl(FieldOperand(rbx, SharedFunctionInfo::kCompilerHintsOffset),
             Immediate(SharedFunctionInfo::kClassConstructorMask));

    if (restrict_constructor_return) {
      // Throw if constructor function is a class constructor
      __ j(Condition::zero, &use_receiver, Label::kNear);
    } else {
      __ j(not_zero, &use_receiver, Label::kNear);
      __ CallRuntime(
          Runtime::kIncrementUseCounterConstructorReturnNonUndefinedPrimitive);
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
    ExternalReference context_address(IsolateAddressId::kContextAddress,
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
    ExternalReference context_address(IsolateAddressId::kContextAddress,
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
    // Argument count in rax. Clobbers rcx.
    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, rax, rcx, &stack_overflow, Label::kNear);
    __ jmp(&enough_stack_space);

    __ bind(&stack_overflow);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // This should be unreachable.
    __ int3();

    __ bind(&enough_stack_space);

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
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the internal frame. Notice that this also removes the empty
    // context and the function left on the stack by the code
    // invocation.
  }

  __ ret(0);
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
  //  -- rdx    : the JSGeneratorObject to resume
  //  -- rsp[0] : return address
  // -----------------------------------
  __ AssertGeneratorObject(rdx);

  // Store input value into generator object.
  __ movp(FieldOperand(rdx, JSGeneratorObject::kInputOrDebugPosOffset), rax);
  __ RecordWriteField(rdx, JSGeneratorObject::kInputOrDebugPosOffset, rax, rcx,
                      kDontSaveFPRegs);

  // Load suspended function and context.
  __ movp(rdi, FieldOperand(rdx, JSGeneratorObject::kFunctionOffset));
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
  __ cmpp(rdx, debug_suspended_generator_operand);
  __ j(equal, &prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ CompareRoot(rsp, Heap::kRealStackLimitRootIndex);
  __ j(below, &stack_overflow);

  // Pop return address.
  __ PopReturnAddressTo(rax);

  // Push receiver.
  __ Push(FieldOperand(rdx, JSGeneratorObject::kReceiverOffset));

  // ----------- S t a t e -------------
  //  -- rax    : return address
  //  -- rdx    : the JSGeneratorObject to resume
  //  -- rdi    : generator function
  //  -- rsi    : generator context
  //  -- rsp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movl(rcx,
          FieldOperand(rcx, SharedFunctionInfo::kFormalParameterCountOffset));
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
    __ movsxlq(rax, FieldOperand(
                        rax, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeOffset));
    __ addp(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(rcx);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdx);
    __ Push(rdi);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(rdx);
    __ movp(rdi, FieldOperand(rdx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdx);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(rdx);
    __ movp(rdi, FieldOperand(rdx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();  // This should be unreachable.
  }
}

// TODO(juliana): if we remove the code below then we don't need all
// the parameters.
static void ReplaceClosureCodeWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {

  // Store the optimized code in the closure.
  __ movp(FieldOperand(closure, JSFunction::kCodeOffset), optimized_code);
  __ movp(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
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

// Tail-call |function_id| if |smi_entry| == |marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register smi_entry,
                                          OptimizationMarker marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ SmiCompare(smi_entry, Smi::FromEnum(marker));
  __ j(not_equal, &no_match, Label::kNear);
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void MaybeTailCallOptimizedCodeSlot(MacroAssembler* masm,
                                           Register feedback_vector,
                                           Register scratch1, Register scratch2,
                                           Register scratch3) {
  // ----------- S t a t e -------------
  //  -- rax : argument count (preserved for callee if needed, and caller)
  //  -- rdx : new target (preserved for callee if needed, and caller)
  //  -- rdi : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  // -----------------------------------
  DCHECK(!AreAliased(feedback_vector, rax, rdx, rdi, scratch1, scratch2,
                     scratch3));

  Label optimized_code_slot_is_cell, fallthrough;

  Register closure = rdi;
  Register optimized_code_entry = scratch1;

  __ movp(optimized_code_entry,
          FieldOperand(feedback_vector, FeedbackVector::kOptimizedCodeOffset));

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret is as a weak cell to a code
  // object.
  __ JumpIfNotSmi(optimized_code_entry, &optimized_code_slot_is_cell);

  {
    // Optimized code slot is a Smi optimization marker.

    // Fall through if no optimization trigger.
    __ SmiCompare(optimized_code_entry,
                  Smi::FromEnum(OptimizationMarker::kNone));
    __ j(equal, &fallthrough);

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
        __ SmiCompare(optimized_code_entry,
                      Smi::FromEnum(OptimizationMarker::kInOptimizationQueue));
        __ Assert(equal, kExpectedOptimizationSentinel);
      }
      __ jmp(&fallthrough);
    }
  }

  {
    // Optimized code slot is a WeakCell.
    __ bind(&optimized_code_slot_is_cell);

    __ movp(optimized_code_entry,
            FieldOperand(optimized_code_entry, WeakCell::kValueOffset));
    __ JumpIfSmi(optimized_code_entry, &fallthrough);

    // Check if the optimized code is marked for deopt. If it is, call the
    // runtime to clear it.
    Label found_deoptimized_code;
    __ movp(scratch2,
            FieldOperand(optimized_code_entry, Code::kCodeDataContainerOffset));
    __ testl(
        FieldOperand(scratch2, CodeDataContainer::kKindSpecificFlagsOffset),
        Immediate(1 << Code::kMarkedForDeoptimizationBit));
    __ j(not_zero, &found_deoptimized_code);

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    // The feedback vector is no longer used, so re-use it as a scratch
    // register.
    ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                        scratch2, scratch3, feedback_vector);
    __ addp(optimized_code_entry,
            Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(optimized_code_entry);

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

  __ Move(bytecode_size_table,
          ExternalReference::bytecode_size_table_address(masm->isolate()));

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label load_size, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  __ cmpb(bytecode, Immediate(0x1));
  __ j(above, &load_size, Label::kNear);
  __ j(equal, &extra_wide, Label::kNear);

  // Load the next bytecode and update table to the wide scaled table.
  __ incl(bytecode_offset);
  __ movzxbp(bytecode, Operand(bytecode_array, bytecode_offset, times_1, 0));
  __ addp(bytecode_size_table,
          Immediate(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&load_size, Label::kNear);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ incl(bytecode_offset);
  __ movzxbp(bytecode, Operand(bytecode_array, bytecode_offset, times_1, 0));
  __ addp(bytecode_size_table,
          Immediate(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&load_size, Label::kNear);

  // Load the size of the current bytecode.
  __ bind(&load_size);
  __ addl(bytecode_offset, Operand(bytecode_size_table, bytecode, times_4, 0));
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o rdi: the JS function object being called
//   o rdx: the incoming new target or generator object
//   o rsi: our context
//   o rbp: the caller's frame pointer
//   o rsp: stack pointer (pointing to return address)
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  Register closure = rdi;
  Register feedback_vector = rbx;

  // Load the feedback vector from the closure.
  __ movp(feedback_vector,
          FieldOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ movp(feedback_vector, FieldOperand(feedback_vector, Cell::kValueOffset));
  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, rcx, r14, r15);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ pushq(rbp);  // Caller's frame pointer.
  __ movp(rbp, rsp);
  __ Push(rsi);  // Callee's context.
  __ Push(rdi);  // Callee's JS function.

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  Label maybe_load_debug_bytecode_array, bytecode_array_loaded;
  __ movp(rax, FieldOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ movp(kInterpreterBytecodeArrayRegister,
          FieldOperand(rax, SharedFunctionInfo::kFunctionDataOffset));
  __ JumpIfNotSmi(FieldOperand(rax, SharedFunctionInfo::kDebugInfoOffset),
                  &maybe_load_debug_bytecode_array);
  __ bind(&bytecode_array_loaded);

  // Increment invocation count for the function.
  __ incl(
      FieldOperand(feedback_vector, FeedbackVector::kInvocationCountOffset));

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
    __ movp(rax, rsp);
    __ subp(rax, rcx);
    __ CompareRoot(rax, Heap::kRealStackLimitRootIndex);
    __ j(above_equal, &ok, Label::kNear);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
    __ j(always, &loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ Push(rax);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ subp(rcx, Immediate(kPointerSize));
    __ j(greater_equal, &loop_header, Label::kNear);
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in rdx.
  Label no_incoming_new_target_or_generator_register;
  __ movsxlq(
      rax,
      FieldOperand(kInterpreterBytecodeArrayRegister,
                   BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ testl(rax, rax);
  __ j(zero, &no_incoming_new_target_or_generator_register, Label::kNear);
  __ movp(Operand(rbp, rax, times_pointer_size, 0), rdx);
  __ bind(&no_incoming_new_target_or_generator_register);

  // Load accumulator with undefined.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));
  __ movzxbp(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ movp(rbx, Operand(kInterpreterDispatchTableRegister, rbx,
                       times_pointer_size, 0));
  __ call(rbx);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ movp(kInterpreterBytecodeArrayRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ movp(kInterpreterBytecodeOffsetRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiToInteger32(kInterpreterBytecodeOffsetRegister,
                    kInterpreterBytecodeOffsetRegister);

  // Check if we should return.
  Label do_return;
  __ movzxbp(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ cmpb(rbx, Immediate(static_cast<int>(interpreter::Bytecode::kReturn)));
  __ j(equal, &do_return, Label::kNear);

  // Advance to the next bytecode and dispatch.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, rbx, rcx);
  __ jmp(&do_dispatch);

  __ bind(&do_return);
  // The return value is in rax.
  LeaveInterpreterFrame(masm, rbx, rcx);
  __ ret(0);

  // Load debug copy of the bytecode array if it exists.
  // kInterpreterBytecodeArrayRegister is already loaded with
  // SharedFunctionInfo::kFunctionDataOffset.
  __ bind(&maybe_load_debug_bytecode_array);
  __ movp(rcx, FieldOperand(rax, SharedFunctionInfo::kDebugInfoOffset));
  __ SmiToInteger32(kScratchRegister,
                    FieldOperand(rcx, DebugInfo::kFlagsOffset));
  __ testl(kScratchRegister, Immediate(DebugInfo::kHasBreakInfo));
  __ j(zero, &bytecode_array_loaded);
  __ movp(kInterpreterBytecodeArrayRegister,
          FieldOperand(rcx, DebugInfo::kDebugBytecodeArrayOffset));
  __ jmp(&bytecode_array_loaded);
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
    InterpreterPushArgsMode mode) {
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

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(rbx);                 // Pass the spread in a register
    __ subp(rax, Immediate(1));  // Subtract one for spread
  }

  // Call the target.
  __ PushReturnAddressFrom(kScratchRegister);  // Re-push return address.

  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ Jump(masm->isolate()->builtins()->CallFunction(receiver_mode),
            RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(BUILTIN_CODE(masm->isolate(), CallWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(receiver_mode),
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

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(rbx);                 // Pass the spread in a register
    __ subp(rax, Immediate(1));  // Subtract one for spread

    // Push return address in preparation for the tail-call.
    __ PushReturnAddressFrom(kScratchRegister);
  } else {
    __ PushReturnAddressFrom(kScratchRegister);
    __ AssertUndefinedOrAllocationSite(rbx);
  }

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
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor (rax, rdx, rdi passed on).
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }

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
  __ Move(rbx, BUILTIN_CODE(masm->isolate(), InterpreterEntryTrampoline));
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
  // Get bytecode array and bytecode offset from the stack frame.
  __ movp(kInterpreterBytecodeArrayRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ movp(kInterpreterBytecodeOffsetRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiToInteger32(kInterpreterBytecodeOffsetRegister,
                    kInterpreterBytecodeOffsetRegister);

  // Load the current bytecode.
  __ movzxbp(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));

  // Advance to the next bytecode.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, rbx, rcx);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ Integer32ToSmi(rbx, kInterpreterBytecodeOffsetRegister);
  __ movp(Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp), rbx);

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CheckOptimizationMarker(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : argument count (preserved for callee)
  //  -- rdx : new target (preserved for callee)
  //  -- rdi : target function (preserved for callee)
  // -----------------------------------
  Register closure = rdi;

  // Get the feedback vector.
  Register feedback_vector = rbx;
  __ movp(feedback_vector,
          FieldOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ movp(feedback_vector, FieldOperand(feedback_vector, Cell::kValueOffset));

  // The feedback vector must be defined.
  if (FLAG_debug_code) {
    __ CompareRoot(feedback_vector, Heap::kUndefinedValueRootIndex);
    __ Assert(not_equal, BailoutReason::kExpectedFeedbackVector);
  }

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, rcx, r14, r15);

  // Otherwise, tail call the SFI code.
  GenerateTailCallToSharedCode(masm);
}

// TODO(jupvfranco): investigate whether there is any case where the CompileLazy
// builtin does not set the code field in the JS function. If there isn't then
// we do not need this builtin and can jump directly to CompileLazy.
void Builtins::Generate_CompileLazyDeoptimizedCode(MacroAssembler* masm) {
  // Set the code slot inside the JSFunction to the trampoline to the
  // interpreter entry.
  __ movq(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movq(rcx, FieldOperand(rcx, SharedFunctionInfo::kCodeOffset));
  __ movq(FieldOperand(rdi, JSFunction::kCodeOffset), rcx);
  __ RecordWriteField(rdi, JSFunction::kCodeOffset, rcx, r15, kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  // Jump to compile lazy.
  Generate_CompileLazy(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : argument count (preserved for callee)
  //  -- rdx : new target (preserved for callee)
  //  -- rdi : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;

  Register closure = rdi;
  Register feedback_vector = rbx;

  // Do we have a valid feedback vector?
  __ movp(feedback_vector,
          FieldOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ movp(feedback_vector, FieldOperand(feedback_vector, Cell::kValueOffset));
  __ JumpIfRoot(feedback_vector, Heap::kUndefinedValueRootIndex,
                &gotta_call_runtime);

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, rcx, r14, r15);

  // We found no optimized code.
  Register entry = rcx;
  __ movp(entry, FieldOperand(closure, JSFunction::kSharedFunctionInfoOffset));

  // If SFI points to anything other than CompileLazy, install that.
  __ movp(entry, FieldOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ Move(rbx, masm->CodeObject());
  __ cmpp(entry, rbx);
  __ j(equal, &gotta_call_runtime);

  // Install the SFI's code entry.
  __ movp(FieldOperand(closure, JSFunction::kCodeOffset), entry);
  __ movp(r14, entry);  // Write barrier clobbers r14 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, r14, r15,
                      kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ leap(entry, FieldOperand(entry, Code::kHeaderSize));
  __ jmp(entry);

  __ bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

// Lazy deserialization design doc: http://goo.gl/dxkYDZ.
void Builtins::Generate_DeserializeLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : argument count (preserved for callee)
  //  -- rdx : new target (preserved for callee)
  //  -- rdi : target function (preserved for callee)
  // -----------------------------------

  Label deserialize_in_runtime;

  Register target = rdi;  // Must be preserved
  Register scratch0 = rbx;
  Register scratch1 = r12;

  CHECK(scratch0 != rax && scratch0 != rdx && scratch0 != rdi);
  CHECK(scratch1 != rax && scratch1 != rdx && scratch1 != rdi);
  CHECK(scratch0 != scratch1);

  // Load the builtin id for lazy deserialization from SharedFunctionInfo.

  __ AssertFunction(target);
  __ movp(scratch0,
          FieldOperand(target, JSFunction::kSharedFunctionInfoOffset));

  __ movp(scratch1,
          FieldOperand(scratch0, SharedFunctionInfo::kFunctionDataOffset));
  __ AssertSmi(scratch1);

  // The builtin may already have been deserialized. If that is the case, it is
  // stored in the builtins table, and we can copy to correct code object to
  // both the shared function info and function without calling into runtime.
  //
  // Otherwise, we need to call into runtime to deserialize.

  {
    // Load the code object at builtins_table[builtin_id] into scratch1.

    __ SmiToInteger32(scratch1, scratch1);
    __ Move(scratch0, ExternalReference::builtins_address(masm->isolate()));
    __ movp(scratch1, Operand(scratch0, scratch1, times_pointer_size, 0));

    // Check if the loaded code object has already been deserialized. This is
    // the case iff it does not equal DeserializeLazy.

    __ Move(scratch0, masm->CodeObject());
    __ cmpp(scratch1, scratch0);
    __ j(equal, &deserialize_in_runtime);
  }

  {
    // If we've reached this spot, the target builtin has been deserialized and
    // we simply need to copy it over. First to the shared function info.

    Register target_builtin = scratch1;
    Register shared = scratch0;

    __ movp(shared,
            FieldOperand(target, JSFunction::kSharedFunctionInfoOffset));

    CHECK(r14 != target && r14 != scratch0 && r14 != scratch1);
    CHECK(r15 != target && r15 != scratch0 && r15 != scratch1);

    __ movp(FieldOperand(shared, SharedFunctionInfo::kCodeOffset),
            target_builtin);
    __ movp(r14, target_builtin);  // Write barrier clobbers r14 below.
    __ RecordWriteField(shared, SharedFunctionInfo::kCodeOffset, r14, r15,
                        kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // And second to the target function.

    __ movp(FieldOperand(target, JSFunction::kCodeOffset), target_builtin);
    __ movp(r14, target_builtin);  // Write barrier clobbers r14 below.
    __ RecordWriteField(target, JSFunction::kCodeOffset, r14, r15,
                        kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // All copying is done. Jump to the deserialized code object.

    __ leap(target_builtin, FieldOperand(target_builtin, Code::kHeaderSize));
    __ jmp(target_builtin);
  }

  __ bind(&deserialize_in_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kDeserializeLazy);
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
  // On failure, tail call back to regular js by re-calling the function
  // which has be reset to the compile lazy builtin.
  __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeOffset));
  __ addp(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(rcx);
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
    __ movq(Operand(rsp,
                    config->num_allocatable_general_registers() * kPointerSize +
                        BuiltinContinuationFrameConstants::kFixedFrameSize),
            rax);
  }
  for (int i = allocatable_register_count - 1; i >= 0; --i) {
    int code = config->GetAllocatableGeneralCode(i);
    __ popq(Register::from_code(code));
    if (java_script_builtin && code == kJavaScriptCallArgCountRegister.code()) {
      __ SmiToInteger32(Register::from_code(code), Register::from_code(code));
    }
  }
  __ movq(
      rbp,
      Operand(rsp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  const int offsetToPC =
      BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp - kPointerSize;
  __ popq(Operand(rsp, offsetToPC));
  __ Drop(offsetToPC / kPointerSize);
  __ addq(Operand(rsp, 0), Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ Ret();
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
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
    // Tear down internal frame.
  }

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), rax.code());
  __ movp(rax, Operand(rsp, kPCOnStackSize));
  __ ret(1 * kPointerSize);  // Remove rax.
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

  // 1. Load receiver into rdi, argArray into rbx (if present), remove all
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
  }

  // ----------- S t a t e -------------
  //  -- rbx     : argArray
  //  -- rdi     : receiver
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(rbx, Heap::kNullValueRootIndex, &no_arguments, Label::kNear);
  __ JumpIfRoot(rbx, Heap::kUndefinedValueRootIndex, &no_arguments,
                Label::kNear);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver. Since we did not create a frame for
  // Function.prototype.apply() yet, we use a normal Call builtin here.
  __ bind(&no_arguments);
  {
    __ Set(rax, 0);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
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

  // 1. Load target into rdi (if present), argumentsList into rbx (if present),
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
  }

  // ----------- S t a t e -------------
  //  -- rbx     : argumentsList
  //  -- rdi     : target
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : thisArgument
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
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : new.target (optional)
  //  -- rsp[16] : argumentsList
  //  -- rsp[24] : target
  //  -- rsp[32] : receiver
  // -----------------------------------

  // 1. Load target into rdi (if present), argumentsList into rbx (if present),
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
  }

  // ----------- S t a t e -------------
  //  -- rbx     : argumentsList
  //  -- rdx     : new.target
  //  -- rdi     : target
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : receiver (undefined)
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

void Builtins::Generate_InternalArrayConstructor(MacroAssembler* masm) {
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
    // Will both indicate a nullptr and a Smi.
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

void Builtins::Generate_ArrayConstructor(MacroAssembler* masm) {
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
    // Will both indicate a nullptr and a Smi.
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

// static
void Builtins::Generate_AbortJS(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rdx    : message as String object
  //  -- rsp[0] : return address
  // -----------------------------------
  __ PopReturnAddressTo(rcx);
  __ Push(rdx);
  __ PushReturnAddressFrom(rcx);
  __ Move(rsi, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbortJS);
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
  __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeOffset));
  __ addp(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
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
  __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeOffset));
  __ addp(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(rcx);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();
  }
}

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- rdi    : target
  //  -- rax    : number of parameters on the stack (not including the receiver)
  //  -- rbx    : arguments list (a FixedArray)
  //  -- rcx    : len (number of elements to push from args)
  //  -- rdx    : new.target (for [[Construct]])
  //  -- rsp[0] : return address
  // -----------------------------------
  __ AssertFixedArray(rbx);

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(kScratchRegister, Heap::kRealStackLimitRootIndex);
    __ movp(r8, rsp);
    // Make r8 the space we have left. The stack might already be overflowed
    // here which will cause r8 to become negative.
    __ subp(r8, kScratchRegister);
    __ sarp(r8, Immediate(kPointerSizeLog2));
    // Check if the arguments will overflow the stack.
    __ cmpp(r8, rcx);
    __ j(greater, &done, Label::kNear);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // Push additional arguments onto the stack.
  {
    __ PopReturnAddressTo(r8);
    __ Set(r9, 0);
    Label done, push, loop;
    __ bind(&loop);
    __ cmpl(r9, rcx);
    __ j(equal, &done, Label::kNear);
    // Turn the hole into undefined as we go.
    __ movp(r11,
            FieldOperand(rbx, r9, times_pointer_size, FixedArray::kHeaderSize));
    __ CompareRoot(r11, Heap::kTheHoleValueRootIndex);
    __ j(not_equal, &push, Label::kNear);
    __ LoadRoot(r11, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ Push(r11);
    __ incl(r9);
    __ jmp(&loop);
    __ bind(&done);
    __ PushReturnAddressFrom(r8);
    __ addq(rax, r9);
  }

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (for [[Construct]] calls)
  //  -- rdi : the target to call (can be any Object)
  //  -- rcx : start index (to support rest parameters)
  // -----------------------------------

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(rdx, &new_target_not_constructor, Label::kNear);
    __ movp(rbx, FieldOperand(rdx, HeapObject::kMapOffset));
    __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsConstructor));
    __ j(not_zero, &new_target_constructor, Label::kNear);
    __ bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(rdx);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ bind(&new_target_constructor);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ movp(rbx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ cmpp(Operand(rbx, CommonFrameConstants::kContextOrFrameTypeOffset),
          Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &arguments_adaptor, Label::kNear);
  {
    __ movp(r8, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
    __ movp(r8, FieldOperand(r8, JSFunction::kSharedFunctionInfoOffset));
    __ movl(r8,
            FieldOperand(r8, SharedFunctionInfo::kFormalParameterCountOffset));
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

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode) {
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
  __ testl(FieldOperand(rdx, SharedFunctionInfo::kCompilerHintsOffset),
           Immediate(SharedFunctionInfo::kClassConstructorMask));
  __ j(not_zero, &class_constructor);

  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the shared function info.
  //  -- rdi : the function to call (checked to be a JSFunction)
  // -----------------------------------

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ testl(FieldOperand(rdx, SharedFunctionInfo::kCompilerHintsOffset),
           Immediate(SharedFunctionInfo::IsNativeBit::kMask |
                     SharedFunctionInfo::IsStrictBit::kMask));
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
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
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

  __ movsxlq(
      rbx, FieldOperand(rdx, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(rax);
  ParameterCount expected(rbx);

  __ InvokeFunctionCode(rdi, no_reg, expected, actual, JUMP_FUNCTION);

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
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(rdi);

  // Patch the receiver to [[BoundThis]].
  StackArgumentsAccessor args(rsp, rax);
  __ movp(rbx, FieldOperand(rdi, JSBoundFunction::kBoundThisOffset));
  __ movp(args.GetReceiverOperand(), rbx);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ movp(rdi, FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the target to call (can be any Object)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(rdi, &non_callable);
  __ bind(&non_smi);
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(equal, masm->isolate()->builtins()->CallFunction(mode),
       RelocInfo::CODE_TARGET);
  __ CmpInstanceType(rcx, JS_BOUND_FUNCTION_TYPE);
  __ j(equal, BUILTIN_CODE(masm->isolate(), CallBoundFunction),
       RelocInfo::CODE_TARGET);

  // Check if target has a [[Call]] internal method.
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsCallable));
  __ j(zero, &non_callable);

  // Check if target is a proxy and call CallProxy external builtin
  __ CmpInstanceType(rcx, JS_PROXY_TYPE);
  __ j(not_equal, &non_function);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ movp(args.GetReceiverOperand(), rdi);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, rdi);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdi);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
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
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
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
  Label non_constructor, non_proxy;
  __ JumpIfSmi(rdi, &non_constructor, Label::kNear);

  // Dispatch based on instance type.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(equal, BUILTIN_CODE(masm->isolate(), ConstructFunction),
       RelocInfo::CODE_TARGET);

  // Check if target has a [[Construct]] internal method.
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsConstructor));
  __ j(zero, &non_constructor, Label::kNear);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ CmpInstanceType(rcx, JS_BOUND_FUNCTION_TYPE);
  __ j(equal, BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
       RelocInfo::CODE_TARGET);

  // Only dispatch to proxies after checking whether they are constructors.
  __ CmpInstanceType(rcx, JS_PROXY_TYPE);
  __ j(not_equal, &non_proxy);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
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
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructedNonConstructable),
          RelocInfo::CODE_TARGET);
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
  __ SmiToInteger32(rbx,
                    Operand(rbx, FixedArray::OffsetOfElementAt(
                                     DeoptimizationData::kOsrPcOffsetIndex) -
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

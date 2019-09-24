// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/api/api-arguments.h"
#include "src/base/adapters.h"
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
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address) {
  __ Move(kJavaScriptCallExtraArg1Register,
          Immediate(ExternalReference::Create(address)));
  __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
          RelocInfo::CODE_TARGET);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- edx : new target (preserved for callee)
  //  -- edi : target function (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the target function and the new target.
    __ push(edi);
    __ push(edx);
    // Function is also the parameter to the runtime call.
    __ push(edi);

    __ CallRuntime(function_id, 1);
    __ mov(ecx, eax);

    // Restore target function and new target.
    __ pop(edx);
    __ pop(edi);
  }

  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  __ JumpCodeObject(ecx);
}

namespace {

void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                 Register scratch, Label* stack_overflow,
                                 bool include_receiver = false) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  ExternalReference real_stack_limit =
      ExternalReference::address_of_real_jslimit(masm->isolate());
  // Compute the space that is left as a negative number in scratch. If
  // we already overflowed, this will be a positive number.
  __ mov(scratch, __ ExternalReferenceAsOperand(real_stack_limit, scratch));
  __ sub(scratch, esp);
  // Add the size of the arguments.
  static_assert(kSystemPointerSize == 4,
                "The next instruction assumes kSystemPointerSize == 4");
  __ lea(scratch, Operand(scratch, num_args, times_system_pointer_size, 0));
  if (include_receiver) {
    __ add(scratch, Immediate(kSystemPointerSize));
  }
  // See if we overflowed, i.e. scratch is positive.
  __ cmp(scratch, Immediate(0));
  __ j(greater, stack_overflow);  // Signed comparison.
}

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax: number of arguments
  //  -- edi: constructor function
  //  -- edx: new target
  //  -- esi: context
  // -----------------------------------

  Label stack_overflow;

  Generate_StackOverflowCheck(masm, eax, ecx, &stack_overflow);

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(eax);
    __ push(esi);
    __ push(eax);
    __ SmiUntag(eax);

    // The receiver for the builtin/api call.
    __ PushRoot(RootIndex::kTheHoleValue);

    // Set up pointer to last argument. We are using esi as scratch register.
    __ lea(esi, Operand(ebp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(ecx, eax);
    // ----------- S t a t e -------------
    //  --                eax: number of arguments (untagged)
    //  --                edi: constructor function
    //  --                edx: new target
    //  --                esi: pointer to last argument
    //  --                ecx: counter
    //  -- sp[0*kSystemPointerSize]: the hole (receiver)
    //  -- sp[1*kSystemPointerSize]: number of arguments (tagged)
    //  -- sp[2*kSystemPointerSize]: context
    // -----------------------------------
    __ jmp(&entry);
    __ bind(&loop);
    __ push(Operand(esi, ecx, times_system_pointer_size, 0));
    __ bind(&entry);
    __ dec(ecx);
    __ j(greater_equal, &loop);

    // Call the function.
    // eax: number of arguments (untagged)
    // edi: constructor function
    // edx: new target
    ParameterCount actual(eax);
    // Reload context from the frame.
    __ mov(esi, Operand(ebp, ConstructFrameConstants::kContextOffset));
    __ InvokeFunction(edi, edx, actual, CALL_FUNCTION);

    // Restore context from the frame.
    __ mov(esi, Operand(ebp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ mov(edx, Operand(ebp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ PopReturnAddressTo(ecx);
  __ lea(esp, Operand(esp, edx, times_half_system_pointer_size,
                      1 * kSystemPointerSize));  // 1 ~ receiver
  __ PushReturnAddressFrom(ecx);
  __ ret(0);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();  // This should be unreachable.
  }
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax: number of arguments (untagged)
  //  -- edi: constructor function
  //  -- edx: new target
  //  -- esi: context
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ mov(ecx, eax);
    __ SmiTag(ecx);
    __ Push(esi);
    __ Push(ecx);
    __ Push(edi);
    __ PushRoot(RootIndex::kTheHoleValue);
    __ Push(edx);

    // ----------- S t a t e -------------
    //  --         sp[0*kSystemPointerSize]: new target
    //  --         sp[1*kSystemPointerSize]: padding
    //  -- edi and sp[2*kSystemPointerSize]: constructor function
    //  --         sp[3*kSystemPointerSize]: argument count
    //  --         sp[4*kSystemPointerSize]: context
    // -----------------------------------

    __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ mov(eax, FieldOperand(eax, SharedFunctionInfo::kFlagsOffset));
    __ DecodeField<SharedFunctionInfo::FunctionKindBits>(eax);
    __ JumpIfIsInRange(eax, kDefaultDerivedConstructor, kDerivedConstructor,
                       ecx, &not_create_implicit_receiver, Label::kNear);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        eax);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ jmp(&post_instantiation_deopt_entry, Label::kNear);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(eax, RootIndex::kTheHoleValue);

    // ----------- S t a t e -------------
    //  --                         eax: implicit receiver
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
    __ Pop(edx);

    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(eax);
    __ Push(eax);

    // ----------- S t a t e -------------
    //  --                edx: new target
    //  -- sp[0*kSystemPointerSize]: implicit receiver
    //  -- sp[1*kSystemPointerSize]: implicit receiver
    //  -- sp[2*kSystemPointerSize]: padding
    //  -- sp[3*kSystemPointerSize]: constructor function
    //  -- sp[4*kSystemPointerSize]: number of arguments (tagged)
    //  -- sp[5*kSystemPointerSize]: context
    // -----------------------------------

    // Restore argument count.
    __ mov(eax, Operand(ebp, ConstructFrameConstants::kLengthOffset));
    __ SmiUntag(eax);

    // Set up pointer to last argument.
    __ lea(edi, Operand(ebp, StandardFrameConstants::kCallerSPOffset));

    // Check if we have enough stack space to push all arguments.
    // Argument count in eax. Clobbers ecx.
    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, eax, ecx, &stack_overflow);
    __ jmp(&enough_stack_space);

    __ bind(&stack_overflow);
    // Restore context from the frame.
    __ mov(esi, Operand(ebp, ConstructFrameConstants::kContextOffset));
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // This should be unreachable.
    __ int3();

    __ bind(&enough_stack_space);

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(ecx, eax);
    // ----------- S t a t e -------------
    //  --                        eax: number of arguments (untagged)
    //  --                        edx: new target
    //  --                        edi: pointer to last argument
    //  --                        ecx: counter (tagged)
    //  --         sp[0*kSystemPointerSize]: implicit receiver
    //  --         sp[1*kSystemPointerSize]: implicit receiver
    //  --         sp[2*kSystemPointerSize]: padding
    //  --         sp[3*kSystemPointerSize]: constructor function
    //  --         sp[4*kSystemPointerSize]: number of arguments (tagged)
    //  --         sp[5*kSystemPointerSize]: context
    // -----------------------------------
    __ jmp(&entry, Label::kNear);
    __ bind(&loop);
    __ Push(Operand(edi, ecx, times_system_pointer_size, 0));
    __ bind(&entry);
    __ dec(ecx);
    __ j(greater_equal, &loop);

    // Restore and and call the constructor function.
    __ mov(edi, Operand(ebp, ConstructFrameConstants::kConstructorOffset));
    ParameterCount actual(eax);
    __ InvokeFunction(edi, edx, actual, CALL_FUNCTION);

    // ----------- S t a t e -------------
    //  --                eax: constructor result
    //  -- sp[0*kSystemPointerSize]: implicit receiver
    //  -- sp[1*kSystemPointerSize]: padding
    //  -- sp[2*kSystemPointerSize]: constructor function
    //  -- sp[3*kSystemPointerSize]: number of arguments
    //  -- sp[4*kSystemPointerSize]: context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore context from the frame.
    __ mov(esi, Operand(ebp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(eax, RootIndex::kUndefinedValue, &use_receiver, Label::kNear);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(eax, &use_receiver, Label::kNear);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, ecx);
    __ j(above_equal, &leave_frame, Label::kNear);
    __ jmp(&use_receiver, Label::kNear);

    __ bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ mov(eax, Operand(esp, 0 * kSystemPointerSize));
    __ JumpIfRoot(eax, RootIndex::kTheHoleValue, &do_throw);

    __ bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ mov(edx, Operand(ebp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ pop(ecx);
  __ lea(esp, Operand(esp, edx, times_half_system_pointer_size,
                      1 * kSystemPointerSize));  // 1 ~ receiver
  __ push(ecx);
  __ ret(0);
}

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ push(edi);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
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
void Generate_JSEntryVariant(MacroAssembler* masm, StackFrame::Type type,
                             Builtins::Name entry_trampoline) {
  Label invoke, handler_entry, exit;
  Label not_outermost_js, not_outermost_js_2;

  {  // NOLINT. Scope block confuses linter.
    NoRootArrayScope uninitialized_root_register(masm);

    // Set up frame.
    __ push(ebp);
    __ mov(ebp, esp);

    // Push marker in two places.
    __ push(Immediate(StackFrame::TypeToMarker(type)));
    // Reserve a slot for the context. It is filled after the root register has
    // been set up.
    __ AllocateStackSpace(kSystemPointerSize);
    // Save callee-saved registers (C calling conventions).
    __ push(edi);
    __ push(esi);
    __ push(ebx);

    // Initialize the root register based on the given Isolate* argument.
    // C calling convention. The first argument is passed on the stack.
    __ mov(kRootRegister,
           Operand(ebp, EntryFrameConstants::kRootRegisterValueOffset));
  }

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp = ExternalReference::Create(
      IsolateAddressId::kCEntryFPAddress, masm->isolate());
  __ push(__ ExternalReferenceAsOperand(c_entry_fp, edi));

  // Store the context address in the previously-reserved slot.
  ExternalReference context_address = ExternalReference::Create(
      IsolateAddressId::kContextAddress, masm->isolate());
  __ mov(edi, __ ExternalReferenceAsOperand(context_address, edi));
  static constexpr int kOffsetToContextSlot = -2 * kSystemPointerSize;
  __ mov(Operand(ebp, kOffsetToContextSlot), edi);

  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp = ExternalReference::Create(
      IsolateAddressId::kJSEntrySPAddress, masm->isolate());
  __ cmp(__ ExternalReferenceAsOperand(js_entry_sp, edi), Immediate(0));
  __ j(not_equal, &not_outermost_js, Label::kNear);
  __ mov(__ ExternalReferenceAsOperand(js_entry_sp, edi), ebp);
  __ push(Immediate(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ jmp(&invoke, Label::kNear);
  __ bind(&not_outermost_js);
  __ push(Immediate(StackFrame::INNER_JSENTRY_FRAME));

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);

  // Store the current pc as the handler offset. It's used later to create the
  // handler table.
  masm->isolate()->builtins()->SetJSEntryHandlerOffset(handler_entry.pos());

  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception = ExternalReference::Create(
      IsolateAddressId::kPendingExceptionAddress, masm->isolate());
  __ mov(__ ExternalReferenceAsOperand(pending_exception, edi), eax);
  __ Move(eax, masm->isolate()->factory()->exception());
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler(edi);

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return.
  Handle<Code> trampoline_code =
      masm->isolate()->builtins()->builtin_handle(entry_trampoline);
  __ Call(trampoline_code, RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler(edi);

  __ bind(&exit);

  // Check if the current stack frame is marked as the outermost JS frame.
  __ pop(edi);
  __ cmp(edi, Immediate(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ j(not_equal, &not_outermost_js_2);
  __ mov(__ ExternalReferenceAsOperand(js_entry_sp, edi), Immediate(0));
  __ bind(&not_outermost_js_2);

  // Restore the top frame descriptor from the stack.
  __ pop(__ ExternalReferenceAsOperand(c_entry_fp, edi));

  // Restore callee-saved registers (C calling conventions).
  __ pop(ebx);
  __ pop(esi);
  __ pop(edi);
  __ add(esp, Immediate(2 * kSystemPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ pop(ebp);
  __ ret(0);
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
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    const Register scratch1 = edx;
    const Register scratch2 = edi;

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address = ExternalReference::Create(
        IsolateAddressId::kContextAddress, masm->isolate());
    __ mov(esi, __ ExternalReferenceAsOperand(context_address, scratch1));

    // Load the previous frame pointer (edx) to access C arguments
    __ mov(scratch1, Operand(ebp, 0));

    // Push the function and the receiver onto the stack.
    __ push(Operand(scratch1, EntryFrameConstants::kFunctionArgOffset));
    __ push(Operand(scratch1, EntryFrameConstants::kReceiverArgOffset));

    // Load the number of arguments and setup pointer to the arguments.
    __ mov(eax, Operand(scratch1, EntryFrameConstants::kArgcOffset));
    __ mov(scratch1, Operand(scratch1, EntryFrameConstants::kArgvOffset));

    // Check if we have enough stack space to push all arguments.
    // Argument count in eax. Clobbers ecx.
    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, eax, ecx, &stack_overflow);
    __ jmp(&enough_stack_space);

    __ bind(&stack_overflow);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // This should be unreachable.
    __ int3();

    __ bind(&enough_stack_space);

    // Copy arguments to the stack in a loop.
    Label loop, entry;
    __ Move(ecx, Immediate(0));
    __ jmp(&entry, Label::kNear);
    __ bind(&loop);
    // Push the parameter from argv.
    __ mov(scratch2, Operand(scratch1, ecx, times_system_pointer_size, 0));
    __ push(Operand(scratch2, 0));  // dereference handle
    __ inc(ecx);
    __ bind(&entry);
    __ cmp(ecx, eax);
    __ j(not_equal, &loop);

    // Load the previous frame pointer (ebx) to access C arguments
    __ mov(scratch2, Operand(ebp, 0));

    // Get the new.target and function from the frame.
    __ mov(edx, Operand(scratch2, EntryFrameConstants::kNewTargetArgOffset));
    __ mov(edi, Operand(scratch2, EntryFrameConstants::kFunctionArgOffset));

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the internal frame. Notice that this also removes the empty.
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

void Builtins::Generate_RunMicrotasksTrampoline(MacroAssembler* masm) {
  // This expects two C++ function parameters passed by Invoke() in
  // execution.cc.
  //   r1: microtask_queue
  __ mov(RunMicrotasksDescriptor::MicrotaskQueueRegister(),
         Operand(ebp, EntryFrameConstants::kMicrotaskQueueArgOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), RunMicrotasks), RelocInfo::CODE_TARGET);
}

static void GetSharedFunctionInfoBytecode(MacroAssembler* masm,
                                          Register sfi_data,
                                          Register scratch1) {
  Label done;

  __ CmpObjectType(sfi_data, INTERPRETER_DATA_TYPE, scratch1);
  __ j(not_equal, &done, Label::kNear);
  __ mov(sfi_data,
         FieldOperand(sfi_data, InterpreterData::kBytecodeArrayOffset));

  __ bind(&done);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : the value to pass to the generator
  //  -- edx    : the JSGeneratorObject to resume
  //  -- esp[0] : return address
  // -----------------------------------
  __ AssertGeneratorObject(edx);

  // Store input value into generator object.
  __ mov(FieldOperand(edx, JSGeneratorObject::kInputOrDebugPosOffset), eax);
  __ RecordWriteField(edx, JSGeneratorObject::kInputOrDebugPosOffset, eax, ecx,
                      kDontSaveFPRegs);

  // Load suspended function and context.
  __ mov(edi, FieldOperand(edx, JSGeneratorObject::kFunctionOffset));
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ cmpb(__ ExternalReferenceAsOperand(debug_hook, ecx), Immediate(0));
  __ j(not_equal, &prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ cmp(edx, __ ExternalReferenceAsOperand(debug_suspended_generator, ecx));
  __ j(equal, &prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ CompareRealStackLimit(esp);
  __ j(below, &stack_overflow);

  // Pop return address.
  __ PopReturnAddressTo(eax);

  // Push receiver.
  __ Push(FieldOperand(edx, JSGeneratorObject::kReceiverOffset));

  // ----------- S t a t e -------------
  //  -- eax    : return address
  //  -- edx    : the JSGeneratorObject to resume
  //  -- edi    : generator function
  //  -- esi    : generator context
  //  -- esp[0] : generator receiver
  // -----------------------------------

  {
    __ movd(xmm0, ebx);

    // Copy the function arguments from the generator object's register file.
    __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ movzx_w(ecx, FieldOperand(
                        ecx, SharedFunctionInfo::kFormalParameterCountOffset));
    __ mov(ebx,
           FieldOperand(edx, JSGeneratorObject::kParametersAndRegistersOffset));
    {
      Label done_loop, loop;
      __ Set(edi, 0);

      __ bind(&loop);
      __ cmp(edi, ecx);
      __ j(greater_equal, &done_loop);
      __ Push(FieldOperand(ebx, edi, times_system_pointer_size,
                           FixedArray::kHeaderSize));
      __ add(edi, Immediate(1));
      __ jmp(&loop);

      __ bind(&done_loop);
    }

    // Restore registers.
    __ mov(edi, FieldOperand(edx, JSGeneratorObject::kFunctionOffset));
    __ movd(ebx, xmm0);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ mov(ecx, FieldOperand(ecx, SharedFunctionInfo::kFunctionDataOffset));
    __ Push(eax);
    GetSharedFunctionInfoBytecode(masm, ecx, eax);
    __ Pop(eax);
    __ CmpObjectType(ecx, BYTECODE_ARRAY_TYPE, ecx);
    __ Assert(equal, AbortReason::kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ PushReturnAddressFrom(eax);
    __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ movzx_w(eax, FieldOperand(
                        eax, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
    __ mov(ecx, FieldOperand(edi, JSFunction::kCodeOffset));
    __ JumpCodeObject(ecx);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(edx);
    __ Push(edi);
    // Push hole as receiver since we do not use it for stepping.
    __ PushRoot(RootIndex::kTheHoleValue);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(edx);
    __ mov(edi, FieldOperand(edx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(edx);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(edx);
    __ mov(edi, FieldOperand(edx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();  // This should be unreachable.
  }
}

static void ReplaceClosureCodeWithOptimizedCode(MacroAssembler* masm,
                                                Register optimized_code,
                                                Register closure,
                                                Register scratch1,
                                                Register scratch2) {
  // Store the optimized code in the closure.
  __ mov(FieldOperand(closure, JSFunction::kCodeOffset), optimized_code);
  __ mov(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch1,
                                  Register scratch2) {
  Register args_count = scratch1;
  Register return_pc = scratch2;

  // Get the arguments + receiver count.
  __ mov(args_count,
         Operand(ebp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ mov(args_count,
         FieldOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ leave();

  // Drop receiver + arguments.
  __ pop(return_pc);
  __ add(esp, args_count);
  __ push(return_pc);
}

// Tail-call |function_id| if |smi_entry| == |marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register smi_entry,
                                          OptimizationMarker marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ cmp(smi_entry, Immediate(Smi::FromEnum(marker)));
  __ j(not_equal, &no_match, Label::kNear);
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void MaybeTailCallOptimizedCodeSlot(MacroAssembler* masm,
                                           Register scratch) {
  // ----------- S t a t e -------------
  //  -- edx : new target (preserved for callee if needed, and caller)
  //  -- edi : target function (preserved for callee if needed, and caller)
  //  -- ecx : feedback vector (also used as scratch, value is not preserved)
  // -----------------------------------
  DCHECK(!AreAliased(edx, edi, scratch));

  Label optimized_code_slot_is_weak_ref, fallthrough;

  Register closure = edi;
  // Scratch contains feedback_vector.
  Register feedback_vector = scratch;

  // Load the optimized code from the feedback vector and re-use the register.
  Register optimized_code_entry = scratch;
  __ mov(optimized_code_entry,
         FieldOperand(feedback_vector,
                      FeedbackVector::kOptimizedCodeWeakOrSmiOffset));

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret it as a weak reference to a code
  // object.
  __ JumpIfNotSmi(optimized_code_entry, &optimized_code_slot_is_weak_ref);

  {
    // Optimized code slot is an optimization marker.

    // Fall through if no optimization trigger.
    __ cmp(optimized_code_entry,
           Immediate(Smi::FromEnum(OptimizationMarker::kNone)));
    __ j(equal, &fallthrough);

    // TODO(v8:8394): The logging of first execution will break if
    // feedback vectors are not allocated. We need to find a different way of
    // logging these events if required.
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
        __ cmp(
            optimized_code_entry,
            Immediate(Smi::FromEnum(OptimizationMarker::kInOptimizationQueue)));
        __ Assert(equal, AbortReason::kExpectedOptimizationSentinel);
      }
      __ jmp(&fallthrough);
    }
  }

  {
    // Optimized code slot is a weak reference.
    __ bind(&optimized_code_slot_is_weak_ref);

    __ LoadWeakValue(optimized_code_entry, &fallthrough);

    __ push(edx);

    // Check if the optimized code is marked for deopt. If it is, bailout to a
    // given label.
    Label found_deoptimized_code;
    __ mov(eax,
           FieldOperand(optimized_code_entry, Code::kCodeDataContainerOffset));
    __ test(FieldOperand(eax, CodeDataContainer::kKindSpecificFlagsOffset),
            Immediate(1 << Code::kMarkedForDeoptimizationBit));
    __ j(not_zero, &found_deoptimized_code);

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                        edx, eax);
    static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
    __ LoadCodeObjectEntry(ecx, optimized_code_entry);
    __ pop(edx);
    __ jmp(ecx);

    // Optimized code slot contains deoptimized code, evict it and re-enter the
    // closure's code.
    __ bind(&found_deoptimized_code);
    __ pop(edx);
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
                                          Register scratch1, Register scratch2,
                                          Label* if_return) {
  Register bytecode_size_table = scratch1;
  Register bytecode = scratch2;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode));
  __ Move(bytecode_size_table,
          Immediate(ExternalReference::bytecode_size_table_address()));

  // Load the current bytecode.
  __ movzx_b(bytecode, Operand(kInterpreterBytecodeArrayRegister,
                               kInterpreterBytecodeOffsetRegister, times_1, 0));

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  STATIC_ASSERT(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  STATIC_ASSERT(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ cmp(bytecode, Immediate(0x3));
  __ j(above, &process_bytecode, Label::kNear);
  __ test(bytecode, Immediate(0x1));
  __ j(not_equal, &extra_wide, Label::kNear);

  // Load the next bytecode and update table to the wide scaled table.
  __ inc(bytecode_offset);
  __ movzx_b(bytecode, Operand(bytecode_array, bytecode_offset, times_1, 0));
  __ add(bytecode_size_table,
         Immediate(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&process_bytecode, Label::kNear);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ inc(bytecode_offset);
  __ movzx_b(bytecode, Operand(bytecode_array, bytecode_offset, times_1, 0));
  __ add(bytecode_size_table,
         Immediate(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));

  __ bind(&process_bytecode);

// Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)                                            \
  __ cmp(bytecode,                                                     \
         Immediate(static_cast<int>(interpreter::Bytecode::k##NAME))); \
  __ j(equal, if_return);
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // Otherwise, load the size of the current bytecode and advance the offset.
  __ add(bytecode_offset,
         Operand(bytecode_size_table, bytecode, times_int_size, 0));
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o edi: the JS function object being called
//   o edx: the incoming new target or generator object
//   o esi: our context
//   o ebp: the caller's frame pointer
//   o esp: stack pointer (pointing to return address)
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  Register closure = edi;

  // The bytecode array could have been flushed from the shared function info,
  // if so, call into CompileLazy.
  Label compile_lazy;
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ecx, FieldOperand(ecx, SharedFunctionInfo::kFunctionDataOffset));
  GetSharedFunctionInfoBytecode(masm, ecx, eax);
  __ CmpObjectType(ecx, BYTECODE_ARRAY_TYPE, eax);
  __ j(not_equal, &compile_lazy);

  Register feedback_vector = ecx;
  Label push_stack_frame;
  // Load feedback vector and check if it is valid. If valid, check for
  // optimized code and update invocation count. Otherwise, setup the stack
  // frame.
  __ mov(feedback_vector,
         FieldOperand(closure, JSFunction::kFeedbackCellOffset));
  __ mov(feedback_vector, FieldOperand(feedback_vector, Cell::kValueOffset));
  __ mov(eax, FieldOperand(feedback_vector, HeapObject::kMapOffset));
  __ CmpInstanceType(eax, FEEDBACK_VECTOR_TYPE);
  __ j(not_equal, &push_stack_frame);

  // Read off the optimized code slot in the closure's feedback vector, and if
  // there is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, ecx);

  // Load the feedback vector and increment the invocation count.
  __ mov(feedback_vector,
         FieldOperand(closure, JSFunction::kFeedbackCellOffset));
  __ mov(feedback_vector, FieldOperand(feedback_vector, Cell::kValueOffset));
  __ inc(FieldOperand(feedback_vector, FeedbackVector::kInvocationCountOffset));

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set
  // up the frame (that is done below).
  __ bind(&push_stack_frame);
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ push(ebp);  // Caller's frame pointer.
  __ mov(ebp, esp);
  __ push(esi);  // Callee's context.
  __ push(edi);  // Callee's JS function.

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(kInterpreterBytecodeArrayRegister,
         FieldOperand(eax, SharedFunctionInfo::kFunctionDataOffset));
  GetSharedFunctionInfoBytecode(masm, kInterpreterBytecodeArrayRegister, eax);

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     eax);
    __ Assert(
        equal,
        AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Reset code age and the OSR arming. The OSR field and BytecodeAgeOffset are
  // 8-bit fields next to each other, so we could just optimize by writing a
  // 16-bit. These static asserts guard our assumption is valid.
  STATIC_ASSERT(BytecodeArray::kBytecodeAgeOffset ==
                BytecodeArray::kOsrNestingLevelOffset + kCharSize);
  STATIC_ASSERT(BytecodeArray::kNoAgeBytecodeAge == 0);
  __ mov_w(FieldOperand(kInterpreterBytecodeArrayRegister,
                        BytecodeArray::kOsrNestingLevelOffset),
           Immediate(0));

  // Push bytecode array.
  __ push(kInterpreterBytecodeArrayRegister);
  // Push Smi tagged initial bytecode array offset.
  __ push(Immediate(Smi::FromInt(BytecodeArray::kHeaderSize - kHeapObjectTag)));

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    Register frame_size = ecx;
    __ mov(frame_size, FieldOperand(kInterpreterBytecodeArrayRegister,
                                    BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ mov(eax, esp);
    __ sub(eax, frame_size);
    __ CompareRealStackLimit(eax);
    __ j(above_equal, &ok);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ Move(eax, masm->isolate()->factory()->undefined_value());
    __ jmp(&loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(eax);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ sub(frame_size, Immediate(kSystemPointerSize));
    __ j(greater_equal, &loop_header);
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in edx.
  Label no_incoming_new_target_or_generator_register;
  __ mov(eax, FieldOperand(
                  kInterpreterBytecodeArrayRegister,
                  BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ test(eax, eax);
  __ j(zero, &no_incoming_new_target_or_generator_register);
  __ mov(Operand(ebp, eax, times_system_pointer_size, 0), edx);
  __ bind(&no_incoming_new_target_or_generator_register);

  // Load accumulator and bytecode offset into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
  __ mov(kInterpreterBytecodeOffsetRegister,
         Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ Move(kInterpreterDispatchTableRegister,
          Immediate(ExternalReference::interpreter_dispatch_table_address(
              masm->isolate())));
  __ movzx_b(ecx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ mov(kJavaScriptCallCodeStartRegister,
         Operand(kInterpreterDispatchTableRegister, ecx,
                 times_system_pointer_size, 0));
  __ call(kJavaScriptCallCodeStartRegister);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ mov(kInterpreterBytecodeArrayRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Either return, or advance to the next bytecode and dispatch.
  Label do_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, ecx,
                                kInterpreterDispatchTableRegister, &do_return);
  __ jmp(&do_dispatch);

  __ bind(&do_return);
  // The return value is in eax.
  LeaveInterpreterFrame(masm, edx, ecx);
  __ ret(0);

  __ bind(&compile_lazy);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
  __ int3();  // Should not return.
}


static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register array_limit,
                                         Register start_address) {
  // ----------- S t a t e -------------
  //  -- start_address : Pointer to the last argument in the args array.
  //  -- array_limit : Pointer to one before the first argument in the
  //                   args array.
  // -----------------------------------
  Label loop_header, loop_check;
  __ jmp(&loop_check);
  __ bind(&loop_header);
  __ Push(Operand(start_address, 0));
  __ sub(start_address, Immediate(kSystemPointerSize));
  __ bind(&loop_check);
  __ cmp(start_address, array_limit);
  __ j(greater, &loop_header, Label::kNear);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- ecx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  //  -- edi : the target to call (can be any Object).
  // -----------------------------------

  const Register scratch = edx;
  const Register argv = ecx;

  Label stack_overflow;
  // Add a stack check before pushing the arguments.
  Generate_StackOverflowCheck(masm, eax, scratch, &stack_overflow, true);

  __ movd(xmm0, eax);  // Spill number of arguments.

  // Compute the expected number of arguments.
  __ mov(scratch, eax);
  __ add(scratch, Immediate(1));  // Add one for receiver.

  // Pop return address to allow tail-call after pushing arguments.
  __ PopReturnAddressTo(eax);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(RootIndex::kUndefinedValue);
    __ sub(scratch, Immediate(1));  // Subtract one for receiver.
  }

  // Find the address of the last argument.
  __ shl(scratch, kSystemPointerSizeLog2);
  __ neg(scratch);
  __ add(scratch, argv);
  Generate_InterpreterPushArgs(masm, scratch, argv);

  // Call the target.

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(ecx);                // Pass the spread in a register
    __ PushReturnAddressFrom(eax);
    __ movd(eax, xmm0);         // Restore number of arguments.
    __ sub(eax, Immediate(1));  // Subtract one for spread
    __ Jump(BUILTIN_CODE(masm->isolate(), CallWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    __ PushReturnAddressFrom(eax);
    __ movd(eax, xmm0);  // Restore number of arguments.
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny),
            RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);

    // This should be unreachable.
    __ int3();
  }
}

namespace {

// This function modifies start_addr, and only reads the contents of num_args
// register. scratch1 and scratch2 are used as temporary registers.
void Generate_InterpreterPushZeroAndArgsAndReturnAddress(
    MacroAssembler* masm, Register num_args, Register start_addr,
    Register scratch1, Register scratch2, int num_slots_to_move,
    Label* stack_overflow) {
  // We have to move return address and the temporary registers above it
  // before we can copy arguments onto the stack. To achieve this:
  // Step 1: Increment the stack pointer by num_args + 1 (for receiver).
  // Step 2: Move the return address and values around it to the top of stack.
  // Step 3: Copy the arguments into the correct locations.
  //  current stack    =====>    required stack layout
  // |             |            | return addr   | (2) <-- esp (1)
  // |             |            | addtl. slot   |
  // |             |            | arg N         | (3)
  // |             |            | ....          |
  // |             |            | arg 1         |
  // | return addr | <-- esp    | arg 0         |
  // | addtl. slot |            | receiver slot |

  // Check for stack overflow before we increment the stack pointer.
  Generate_StackOverflowCheck(masm, num_args, scratch1, stack_overflow, true);

  // Step 1 - Update the stack pointer.

  __ lea(scratch1,
         Operand(num_args, times_system_pointer_size, kSystemPointerSize));
  __ AllocateStackSpace(scratch1);

  // Step 2 move return_address and slots around it to the correct locations.
  // Move from top to bottom, otherwise we may overwrite when num_args = 0 or 1,
  // basically when the source and destination overlap. We at least need one
  // extra slot for receiver, so no extra checks are required to avoid copy.
  for (int i = 0; i < num_slots_to_move + 1; i++) {
    __ mov(scratch1, Operand(esp, num_args, times_system_pointer_size,
                             (i + 1) * kSystemPointerSize));
    __ mov(Operand(esp, i * kSystemPointerSize), scratch1);
  }

  // Step 3 copy arguments to correct locations.
  // Slot meant for receiver contains return address. Reset it so that
  // we will not incorrectly interpret return address as an object.
  __ mov(Operand(esp, num_args, times_system_pointer_size,
                 (num_slots_to_move + 1) * kSystemPointerSize),
         Immediate(0));
  __ mov(scratch1, num_args);

  Label loop_header, loop_check;
  __ jmp(&loop_check);
  __ bind(&loop_header);
  __ mov(scratch2, Operand(start_addr, 0));
  __ mov(Operand(esp, scratch1, times_system_pointer_size,
                 num_slots_to_move * kSystemPointerSize),
         scratch2);
  __ sub(start_addr, Immediate(kSystemPointerSize));
  __ sub(scratch1, Immediate(1));
  __ bind(&loop_check);
  __ cmp(scratch1, Immediate(0));
  __ j(greater, &loop_header, Label::kNear);
}

}  // end anonymous namespace

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- eax     : the number of arguments (not including the receiver)
  //  -- ecx     : the address of the first argument to be pushed. Subsequent
  //               arguments should be consecutive above this, in the same order
  //               as they are to be pushed onto the stack.
  //  -- esp[0]  : return address
  //  -- esp[4]  : allocation site feedback (if available or undefined)
  //  -- esp[8]  : the new target
  //  -- esp[12] : the constructor
  // -----------------------------------

  Label stack_overflow;

  // Push arguments and move return address and stack spill slots to the top of
  // stack. The eax register is readonly. The ecx register will be modified. edx
  // and edi are used as scratch registers.
  Generate_InterpreterPushZeroAndArgsAndReturnAddress(
      masm, eax, ecx, edx, edi,
      InterpreterPushArgsThenConstructDescriptor::kStackArgumentsCount,
      &stack_overflow);

  // Call the appropriate constructor. eax and ecx already contain intended
  // values, remaining registers still need to be initialized from the stack.

  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    // Tail call to the array construct stub (still in the caller context at
    // this point).

    __ movd(xmm0, eax);  // Spill number of arguments.
    __ PopReturnAddressTo(eax);
    __ Pop(kJavaScriptCallExtraArg1Register);
    __ Pop(kJavaScriptCallNewTargetRegister);
    __ Pop(kJavaScriptCallTargetRegister);
    __ PushReturnAddressFrom(eax);

    __ AssertFunction(kJavaScriptCallTargetRegister);
    __ AssertUndefinedOrAllocationSite(kJavaScriptCallExtraArg1Register, eax);

    __ movd(eax, xmm0);  // Reload number of arguments.
    __ Jump(BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl),
            RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ movd(xmm0, eax);  // Spill number of arguments.
    __ PopReturnAddressTo(eax);
    __ Drop(1);  // The allocation site is unused.
    __ Pop(kJavaScriptCallNewTargetRegister);
    __ Pop(kJavaScriptCallTargetRegister);
    __ Pop(ecx);  // Pop the spread (i.e. the first argument), overwriting ecx.
    __ PushReturnAddressFrom(eax);
    __ movd(eax, xmm0);         // Reload number of arguments.
    __ sub(eax, Immediate(1));  // The actual argc thus decrements by one.

    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    __ PopReturnAddressTo(ecx);
    __ Drop(1);  // The allocation site is unused.
    __ Pop(kJavaScriptCallNewTargetRegister);
    __ Pop(kJavaScriptCallTargetRegister);
    __ PushReturnAddressFrom(ecx);

    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ int3();
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Label builtin_trampoline, trampoline_loaded;
  Smi interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);

  static constexpr Register scratch = ecx;

  // If the SFI function_data is an InterpreterData, the function will have a
  // custom copy of the interpreter entry trampoline for profiling. If so,
  // get the custom trampoline, otherwise grab the entry address of the global
  // trampoline.
  __ mov(scratch, Operand(ebp, StandardFrameConstants::kFunctionOffset));
  __ mov(scratch, FieldOperand(scratch, JSFunction::kSharedFunctionInfoOffset));
  __ mov(scratch,
         FieldOperand(scratch, SharedFunctionInfo::kFunctionDataOffset));
  __ Push(eax);
  __ CmpObjectType(scratch, INTERPRETER_DATA_TYPE, eax);
  __ j(not_equal, &builtin_trampoline, Label::kNear);

  __ mov(scratch,
         FieldOperand(scratch, InterpreterData::kInterpreterTrampolineOffset));
  __ add(scratch, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(&trampoline_loaded, Label::kNear);

  __ bind(&builtin_trampoline);
  __ mov(scratch,
         __ ExternalReferenceAsOperand(
             ExternalReference::
                 address_of_interpreter_entry_trampoline_instruction_start(
                     masm->isolate()),
             scratch));

  __ bind(&trampoline_loaded);
  __ Pop(eax);
  __ add(scratch, Immediate(interpreter_entry_return_pc_offset.value()));
  __ push(scratch);

  // Initialize the dispatch table register.
  __ Move(kInterpreterDispatchTableRegister,
          Immediate(ExternalReference::interpreter_dispatch_table_address(
              masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ mov(kInterpreterBytecodeArrayRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     scratch);
    __ Assert(
        equal,
        AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ movzx_b(scratch, Operand(kInterpreterBytecodeArrayRegister,
                              kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ mov(kJavaScriptCallCodeStartRegister,
         Operand(kInterpreterDispatchTableRegister, scratch,
                 times_system_pointer_size, 0));
  __ jmp(kJavaScriptCallCodeStartRegister);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Get bytecode array and bytecode offset from the stack frame.
  __ mov(kInterpreterBytecodeArrayRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(ebp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Advance to the next bytecode.
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, ecx, esi,
                                &if_return);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ mov(ecx, kInterpreterBytecodeOffsetRegister);
  __ SmiTag(ecx);
  __ mov(Operand(ebp, InterpreterFrameConstants::kBytecodeOffsetFromFp), ecx);

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
  //  -- eax : argument count (preserved for callee)
  //  -- edx : new target (preserved for callee)
  //  -- edi : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ mov(ecx, eax);
    // Push the number of arguments to the callee.
    __ SmiTag(eax);
    __ push(eax);
    // Push a copy of the target function and the new target.
    __ push(edi);
    __ push(edx);

    // The function.
    __ push(edi);
    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ cmp(ecx, Immediate(j));
        __ j(not_equal, &over, Label::kNear);
      }
      for (int i = j - 1; i >= 0; --i) {
        __ Push(Operand(ebp, StandardFrameConstants::kCallerSPOffset +
                                 i * kSystemPointerSize));
      }
      for (int i = 0; i < 3 - j; ++i) {
        __ PushRoot(RootIndex::kUndefinedValue);
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
    __ JumpIfSmi(eax, &failed, Label::kNear);

    __ Drop(2);
    __ Pop(ecx);
    __ SmiUntag(ecx);
    scope.GenerateLeaveFrame();

    __ PopReturnAddressTo(edx);
    __ inc(ecx);
    __ lea(esp, Operand(esp, ecx, times_system_pointer_size, 0));
    __ PushReturnAddressFrom(edx);
    __ ret(0);

    __ bind(&failed);
    // Restore target function and new target.
    __ pop(edx);
    __ pop(edi);
    __ pop(eax);
    __ SmiUntag(eax);
  }
  // On failure, tail call back to regular js by re-calling the function
  // which has be reset to the compile lazy builtin.
  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  __ mov(ecx, FieldOperand(edi, JSFunction::kCodeOffset));
  __ JumpCodeObject(ecx);
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
    __ mov(Operand(esp, config->num_allocatable_general_registers() *
                                kSystemPointerSize +
                            BuiltinContinuationFrameConstants::kFixedFrameSize),
           eax);
  }

  // Replace the builtin index Smi on the stack with the start address of the
  // builtin loaded from the builtins table. The ret below will return to this
  // address.
  int offset_to_builtin_index = allocatable_register_count * kSystemPointerSize;
  __ mov(eax, Operand(esp, offset_to_builtin_index));
  __ LoadEntryFromBuiltinIndex(eax);
  __ mov(Operand(esp, offset_to_builtin_index), eax);

  for (int i = allocatable_register_count - 1; i >= 0; --i) {
    int code = config->GetAllocatableGeneralCode(i);
    __ pop(Register::from_code(code));
    if (java_script_builtin && code == kJavaScriptCallArgCountRegister.code()) {
      __ SmiUntag(Register::from_code(code));
    }
  }
  __ mov(
      ebp,
      Operand(esp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  const int offsetToPC =
      BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp -
      kSystemPointerSize;
  __ pop(Operand(esp, offsetToPC));
  __ Drop(offsetToPC / kSystemPointerSize);
  __ ret(0);
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
    // Tear down internal frame.
  }

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), eax.code());
  __ mov(eax, Operand(esp, 1 * kSystemPointerSize));
  __ ret(1 * kSystemPointerSize);  // Remove eax.
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax     : argc
  //  -- esp[0]  : return address
  //  -- esp[4]  : argArray
  //  -- esp[8]  : thisArg
  //  -- esp[12] : receiver
  // -----------------------------------

  // 1. Load receiver into xmm0, argArray into edx (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label no_arg_array, no_this_arg;
    // Spill receiver to allow the usage of edi as a scratch register.
    __ movd(xmm0,
            Operand(esp, eax, times_system_pointer_size, kSystemPointerSize));

    __ LoadRoot(edx, RootIndex::kUndefinedValue);
    __ mov(edi, edx);
    __ test(eax, eax);
    __ j(zero, &no_this_arg, Label::kNear);
    {
      __ mov(edi, Operand(esp, eax, times_system_pointer_size, 0));
      __ cmp(eax, Immediate(1));
      __ j(equal, &no_arg_array, Label::kNear);
      __ mov(edx,
             Operand(esp, eax, times_system_pointer_size, -kSystemPointerSize));
      __ bind(&no_arg_array);
    }
    __ bind(&no_this_arg);
    __ PopReturnAddressTo(ecx);
    __ lea(esp,
           Operand(esp, eax, times_system_pointer_size, kSystemPointerSize));
    __ Push(edi);
    __ PushReturnAddressFrom(ecx);

    // Restore receiver to edi.
    __ movd(edi, xmm0);
  }

  // ----------- S t a t e -------------
  //  -- edx    : argArray
  //  -- edi    : receiver
  //  -- esp[0] : return address
  //  -- esp[4] : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(edx, RootIndex::kNullValue, &no_arguments, Label::kNear);
  __ JumpIfRoot(edx, RootIndex::kUndefinedValue, &no_arguments, Label::kNear);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ Set(eax, 0);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // Stack Layout:
  // esp[0]           : Return address
  // esp[8]           : Argument n
  // esp[16]          : Argument n-1
  //  ...
  // esp[8 * n]       : Argument 1
  // esp[8 * (n + 1)] : Receiver (callable to call)
  //
  // eax contains the number of arguments, n, not counting the receiver.
  //
  // 1. Make sure we have at least one argument.
  {
    Label done;
    __ test(eax, eax);
    __ j(not_zero, &done, Label::kNear);
    __ PopReturnAddressTo(edx);
    __ PushRoot(RootIndex::kUndefinedValue);
    __ PushReturnAddressFrom(edx);
    __ inc(eax);
    __ bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  __ mov(edi, Operand(esp, eax, times_system_pointer_size, kSystemPointerSize));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  {
    Label loop;
    __ mov(ecx, eax);
    __ bind(&loop);
    __ mov(edx, Operand(esp, ecx, times_system_pointer_size, 0));
    __ mov(Operand(esp, ecx, times_system_pointer_size, kSystemPointerSize),
           edx);
    __ dec(ecx);
    __ j(not_sign, &loop);  // While non-negative (to copy return address).
    __ pop(edx);            // Discard copy of return address.
    __ dec(eax);  // One fewer argument (first argument is new receiver).
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax     : argc
  //  -- esp[0]  : return address
  //  -- esp[4]  : argumentsList
  //  -- esp[8]  : thisArgument
  //  -- esp[12] : target
  //  -- esp[16] : receiver
  // -----------------------------------

  // 1. Load target into edi (if present), argumentsList into edx (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label done;
    __ LoadRoot(edi, RootIndex::kUndefinedValue);
    __ mov(edx, edi);
    __ mov(ecx, edi);
    __ cmp(eax, Immediate(1));
    __ j(below, &done, Label::kNear);
    __ mov(edi, Operand(esp, eax, times_system_pointer_size,
                        -0 * kSystemPointerSize));
    __ j(equal, &done, Label::kNear);
    __ mov(ecx, Operand(esp, eax, times_system_pointer_size,
                        -1 * kSystemPointerSize));
    __ cmp(eax, Immediate(3));
    __ j(below, &done, Label::kNear);
    __ mov(edx, Operand(esp, eax, times_system_pointer_size,
                        -2 * kSystemPointerSize));
    __ bind(&done);

    // Spill argumentsList to use edx as a scratch register.
    __ movd(xmm0, edx);

    __ PopReturnAddressTo(edx);
    __ lea(esp,
           Operand(esp, eax, times_system_pointer_size, kSystemPointerSize));
    __ Push(ecx);
    __ PushReturnAddressFrom(edx);

    // Restore argumentsList.
    __ movd(edx, xmm0);
  }

  // ----------- S t a t e -------------
  //  -- edx    : argumentsList
  //  -- edi    : target
  //  -- esp[0] : return address
  //  -- esp[4] : thisArgument
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
  //  -- eax     : argc
  //  -- esp[0]  : return address
  //  -- esp[4]  : new.target (optional)
  //  -- esp[8]  : argumentsList
  //  -- esp[12] : target
  //  -- esp[16] : receiver
  // -----------------------------------

  // 1. Load target into edi (if present), argumentsList into ecx (if present),
  // new.target into edx (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label done;
    __ LoadRoot(edi, RootIndex::kUndefinedValue);
    __ mov(edx, edi);
    __ mov(ecx, edi);
    __ cmp(eax, Immediate(1));
    __ j(below, &done, Label::kNear);
    __ mov(edi, Operand(esp, eax, times_system_pointer_size,
                        -0 * kSystemPointerSize));
    __ mov(edx, edi);
    __ j(equal, &done, Label::kNear);
    __ mov(ecx, Operand(esp, eax, times_system_pointer_size,
                        -1 * kSystemPointerSize));
    __ cmp(eax, Immediate(3));
    __ j(below, &done, Label::kNear);
    __ mov(edx, Operand(esp, eax, times_system_pointer_size,
                        -2 * kSystemPointerSize));
    __ bind(&done);

    // Spill argumentsList to use ecx as a scratch register.
    __ movd(xmm0, ecx);

    __ PopReturnAddressTo(ecx);
    __ lea(esp,
           Operand(esp, eax, times_system_pointer_size, kSystemPointerSize));
    __ PushRoot(RootIndex::kUndefinedValue);
    __ PushReturnAddressFrom(ecx);

    // Restore argumentsList.
    __ movd(ecx, xmm0);
  }

  // ----------- S t a t e -------------
  //  -- ecx    : argumentsList
  //  -- edx    : new.target
  //  -- edi    : target
  //  -- esp[0] : return address
  //  -- esp[4] : receiver (undefined)
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
  //  -- eax : argc
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray function should be a map.
    __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_zero,
              AbortReason::kUnexpectedInitialMapForInternalArrayFunction);
    __ CmpObjectType(ecx, MAP_TYPE, ecx);
    __ Assert(equal,
              AbortReason::kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  __ Jump(BUILTIN_CODE(masm->isolate(), InternalArrayConstructorImpl),
          RelocInfo::CODE_TARGET);
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ push(ebp);
  __ mov(ebp, esp);

  // Store the arguments adaptor context sentinel.
  __ push(Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));

  // Push the function on the stack.
  __ push(edi);

  // Preserve the number of arguments on the stack. Must preserve eax,
  // ebx and ecx because these registers are used when copying the
  // arguments and the receiver.
  STATIC_ASSERT(kSmiTagSize == 1);
  __ lea(edi, Operand(eax, eax, times_1, kSmiTag));
  __ push(edi);

  __ Push(Immediate(0));  // Padding.
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // Retrieve the number of arguments from the stack.
  __ mov(edi, Operand(ebp, ArgumentsAdaptorFrameConstants::kLengthOffset));

  // Leave the frame.
  __ leave();

  // Remove caller arguments from the stack.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ PopReturnAddressTo(ecx);
  __ lea(esp, Operand(esp, edi, times_half_system_pointer_size,
                      1 * kSystemPointerSize));  // 1 ~ receiver
  __ PushReturnAddressFrom(ecx);
}

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- edi    : target
  //  -- esi    : context for the Call / Construct builtin
  //  -- eax    : number of parameters on the stack (not including the receiver)
  //  -- ecx    : len (number of elements to from args)
  //  -- ecx    : new.target (checked to be constructor or undefined)
  //  -- esp[4] : arguments list (a FixedArray)
  //  -- esp[0] : return address.
  // -----------------------------------

  // We need to preserve eax, edi, esi and ebx.
  __ movd(xmm0, edx);
  __ movd(xmm1, edi);
  __ movd(xmm2, eax);
  __ movd(xmm3, esi);  // Spill the context.

  const Register kArgumentsList = esi;
  const Register kArgumentsLength = ecx;

  __ PopReturnAddressTo(edx);
  __ pop(kArgumentsList);
  __ PushReturnAddressFrom(edx);

  if (masm->emit_debug_code()) {
    // Allow kArgumentsList to be a FixedArray, or a FixedDoubleArray if
    // kArgumentsLength == 0.
    Label ok, fail;
    __ AssertNotSmi(kArgumentsList);
    __ mov(edx, FieldOperand(kArgumentsList, HeapObject::kMapOffset));
    __ CmpInstanceType(edx, FIXED_ARRAY_TYPE);
    __ j(equal, &ok);
    __ CmpInstanceType(edx, FIXED_DOUBLE_ARRAY_TYPE);
    __ j(not_equal, &fail);
    __ cmp(kArgumentsLength, 0);
    __ j(equal, &ok);
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  Generate_StackOverflowCheck(masm, kArgumentsLength, edx, &stack_overflow);

  // Push additional arguments onto the stack.
  {
    __ PopReturnAddressTo(edx);
    __ Move(eax, Immediate(0));
    Label done, push, loop;
    __ bind(&loop);
    __ cmp(eax, kArgumentsLength);
    __ j(equal, &done, Label::kNear);
    // Turn the hole into undefined as we go.
    __ mov(edi, FieldOperand(kArgumentsList, eax, times_system_pointer_size,
                             FixedArray::kHeaderSize));
    __ CompareRoot(edi, RootIndex::kTheHoleValue);
    __ j(not_equal, &push, Label::kNear);
    __ LoadRoot(edi, RootIndex::kUndefinedValue);
    __ bind(&push);
    __ Push(edi);
    __ inc(eax);
    __ jmp(&loop);
    __ bind(&done);
    __ PushReturnAddressFrom(edx);
  }

  // Restore eax, edi and edx.
  __ movd(esi, xmm3);  // Restore the context.
  __ movd(eax, xmm2);
  __ movd(edi, xmm1);
  __ movd(edx, xmm0);

  // Compute the actual parameter count.
  __ add(eax, kArgumentsLength);

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);

  __ bind(&stack_overflow);
  __ movd(esi, xmm3);  // Restore the context.
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the target to call (can be any Object)
  //  -- esi : context for the Call / Construct builtin
  //  -- edx : the new target (for [[Construct]] calls)
  //  -- ecx : start index (to support rest parameters)
  // -----------------------------------

  __ movd(xmm0, esi);  // Spill the context.

  Register scratch = esi;

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(edx, &new_target_not_constructor, Label::kNear);
    __ mov(scratch, FieldOperand(edx, HeapObject::kMapOffset));
    __ test_b(FieldOperand(scratch, Map::kBitFieldOffset),
              Immediate(Map::IsConstructorBit::kMask));
    __ j(not_zero, &new_target_constructor, Label::kNear);
    __ bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(edx);
      __ movd(esi, xmm0);  // Restore the context.
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ bind(&new_target_constructor);
  }

  __ movd(xmm1, edx);  // Preserve new.target (in case of [[Construct]]).

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ mov(scratch, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ cmp(Operand(scratch, CommonFrameConstants::kContextOrFrameTypeOffset),
         Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &arguments_adaptor, Label::kNear);
  {
    __ mov(edx, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    __ mov(edx, FieldOperand(edx, JSFunction::kSharedFunctionInfoOffset));
    __ movzx_w(edx, FieldOperand(
                        edx, SharedFunctionInfo::kFormalParameterCountOffset));
    __ mov(scratch, ebp);
  }
  __ jmp(&arguments_done, Label::kNear);
  __ bind(&arguments_adaptor);
  {
    // Just load the length from the ArgumentsAdaptorFrame.
    __ mov(edx,
           Operand(scratch, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ SmiUntag(edx);
  }
  __ bind(&arguments_done);

  Label stack_done, stack_overflow;
  __ sub(edx, ecx);
  __ j(less_equal, &stack_done);
  {
    Generate_StackOverflowCheck(masm, edx, ecx, &stack_overflow);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ add(eax, edx);
      __ PopReturnAddressTo(ecx);
      __ bind(&loop);
      {
        __ Push(Operand(scratch, edx, times_system_pointer_size,
                        1 * kSystemPointerSize));
        __ dec(edx);
        __ j(not_zero, &loop);
      }
      __ PushReturnAddressFrom(ecx);
    }
  }
  __ bind(&stack_done);

  __ movd(edx, xmm1);  // Restore new.target (in case of [[Construct]]).
  __ movd(esi, xmm0);  // Restore the context.

  // Tail-call to the {code} handler.
  __ Jump(code, RelocInfo::CODE_TARGET);

  __ bind(&stack_overflow);
  __ movd(esi, xmm0);  // Restore the context.
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
}

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(edi);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ test(FieldOperand(edx, SharedFunctionInfo::kFlagsOffset),
          Immediate(SharedFunctionInfo::IsClassConstructorBit::kMask));
  __ j(not_zero, &class_constructor);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ test(FieldOperand(edx, SharedFunctionInfo::kFlagsOffset),
          Immediate(SharedFunctionInfo::IsNativeBit::kMask |
                    SharedFunctionInfo::IsStrictBit::kMask));
  __ j(not_zero, &done_convert);
  {
    // ----------- S t a t e -------------
    //  -- eax : the number of arguments (not including the receiver)
    //  -- edx : the shared function info.
    //  -- edi : the function to call (checked to be a JSFunction)
    //  -- esi : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(ecx);
    } else {
      Label convert_to_object, convert_receiver;
      __ mov(ecx,
             Operand(esp, eax, times_system_pointer_size, kSystemPointerSize));
      __ JumpIfSmi(ecx, &convert_to_object, Label::kNear);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CmpObjectType(ecx, FIRST_JS_RECEIVER_TYPE, ecx);  // Clobbers ecx.
      __ j(above_equal, &done_convert);
      // Reload the receiver (it was clobbered by CmpObjectType).
      __ mov(ecx,
             Operand(esp, eax, times_system_pointer_size, kSystemPointerSize));
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(ecx, RootIndex::kUndefinedValue, &convert_global_proxy,
                      Label::kNear);
        __ JumpIfNotRoot(ecx, RootIndex::kNullValue, &convert_to_object,
                         Label::kNear);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(ecx);
        }
        __ jmp(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(eax);
        __ Push(eax);
        __ Push(edi);
        __ mov(eax, ecx);
        __ Push(esi);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(esi);
        __ mov(ecx, eax);
        __ Pop(edi);
        __ Pop(eax);
        __ SmiUntag(eax);
      }
      __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ mov(Operand(esp, eax, times_system_pointer_size, kSystemPointerSize),
           ecx);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the shared function info.
  //  -- edi : the function to call (checked to be a JSFunction)
  //  -- esi : the function context.
  // -----------------------------------

  __ movzx_w(
      ecx, FieldOperand(edx, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(eax);
  ParameterCount expected(ecx);
  __ InvokeFunctionCode(edi, no_reg, expected, actual, JUMP_FUNCTION);
  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ push(edi);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : new.target (only in case of [[Construct]])
  //  -- edi : target (checked to be a JSBoundFunction)
  // -----------------------------------

  __ movd(xmm0, edx);  // Spill edx.

  // Load [[BoundArguments]] into ecx and length of that into edx.
  Label no_bound_arguments;
  __ mov(ecx, FieldOperand(edi, JSBoundFunction::kBoundArgumentsOffset));
  __ mov(edx, FieldOperand(ecx, FixedArray::kLengthOffset));
  __ SmiUntag(edx);
  __ test(edx, edx);
  __ j(zero, &no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- eax  : the number of arguments (not including the receiver)
    //  -- xmm0 : new.target (only in case of [[Construct]])
    //  -- edi  : target (checked to be a JSBoundFunction)
    //  -- ecx  : the [[BoundArguments]] (implemented as FixedArray)
    //  -- edx  : the number of [[BoundArguments]]
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ lea(ecx, Operand(edx, times_system_pointer_size, 0));
      __ sub(esp, ecx);  // Not Windows-friendly, but corrected below.
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRealStackLimit(esp);
      __ j(above_equal, &done, Label::kNear);
      // Restore the stack pointer.
      __ lea(esp, Operand(esp, edx, times_system_pointer_size, 0));
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

#if V8_OS_WIN
    // Correctly allocate the stack space that was checked above.
    {
      Label win_done;
      __ cmp(ecx, TurboAssemblerBase::kStackPageSize);
      __ j(less_equal, &win_done, Label::kNear);
      // Reset esp and walk through the range touching every page.
      __ lea(esp, Operand(esp, edx, times_system_pointer_size, 0));
      __ AllocateStackSpace(ecx);
      __ bind(&win_done);
    }
#endif

    // Adjust effective number of arguments to include return address.
    __ inc(eax);

    // Relocate arguments and return address down the stack.
    {
      Label loop;
      __ Set(ecx, 0);
      __ lea(edx, Operand(esp, edx, times_system_pointer_size, 0));
      __ bind(&loop);
      __ movd(xmm1, Operand(edx, ecx, times_system_pointer_size, 0));
      __ movd(Operand(esp, ecx, times_system_pointer_size, 0), xmm1);
      __ inc(ecx);
      __ cmp(ecx, eax);
      __ j(less, &loop);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ mov(ecx, FieldOperand(edi, JSBoundFunction::kBoundArgumentsOffset));
      __ mov(edx, FieldOperand(ecx, FixedArray::kLengthOffset));
      __ SmiUntag(edx);
      __ bind(&loop);
      __ dec(edx);
      __ movd(xmm1, FieldOperand(ecx, edx, times_tagged_size,
                                 FixedArray::kHeaderSize));
      __ movd(Operand(esp, eax, times_system_pointer_size, 0), xmm1);
      __ lea(eax, Operand(eax, 1));
      __ j(greater, &loop);
    }

    // Adjust effective number of arguments (eax contains the number of
    // arguments from the call plus return address plus the number of
    // [[BoundArguments]]), so we need to subtract one for the return address.
    __ dec(eax);
  }

  __ bind(&no_bound_arguments);
  __ movd(edx, xmm0);  // Reload edx.
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(edi);

  // Patch the receiver to [[BoundThis]].
  __ mov(ecx, FieldOperand(edi, JSBoundFunction::kBoundThisOffset));
  __ mov(Operand(esp, eax, times_system_pointer_size, kSystemPointerSize), ecx);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ mov(edi, FieldOperand(edi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edi : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi, non_jsfunction,
      non_jsboundfunction;
  __ JumpIfSmi(edi, &non_callable);
  __ bind(&non_smi);
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &non_jsfunction);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET);

  __ bind(&non_jsfunction);
  __ CmpInstanceType(ecx, JS_BOUND_FUNCTION_TYPE);
  __ j(not_equal, &non_jsboundfunction);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET);

  // Check if target is a proxy and call CallProxy external builtin
  __ bind(&non_jsboundfunction);
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
            Immediate(Map::IsCallableBit::kMask));
  __ j(zero, &non_callable);

  // Call CallProxy external builtin
  __ CmpInstanceType(ecx, JS_PROXY_TYPE);
  __ j(not_equal, &non_function);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ mov(Operand(esp, eax, times_system_pointer_size, kSystemPointerSize), edi);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadGlobalFunction(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, edi);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(edi);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the new target (checked to be a constructor)
  //  -- edi : the constructor to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertConstructor(edi);
  __ AssertFunction(edi);

  Label call_generic_stub;

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ test(FieldOperand(ecx, SharedFunctionInfo::kFlagsOffset),
          Immediate(SharedFunctionInfo::ConstructAsBuiltinBit::kMask));
  __ j(zero, &call_generic_stub, Label::kNear);

  // Calling convention for function specific ConstructStubs require
  // ecx to contain either an AllocationSite or undefined.
  __ LoadRoot(ecx, RootIndex::kUndefinedValue);
  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET);

  __ bind(&call_generic_stub);
  // Calling convention for function specific ConstructStubs require
  // ecx to contain either an AllocationSite or undefined.
  __ LoadRoot(ecx, RootIndex::kUndefinedValue);
  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the new target (checked to be a constructor)
  //  -- edi : the constructor to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertConstructor(edi);
  __ AssertBoundFunction(edi);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label done;
    __ cmp(edi, edx);
    __ j(not_equal, &done, Label::kNear);
    __ mov(edx, FieldOperand(edi, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&done);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ mov(edi, FieldOperand(edi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : the number of arguments (not including the receiver)
  //  -- edx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- edi : the constructor to call (can be any Object)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor, non_proxy, non_jsfunction, non_jsboundfunction;
  __ JumpIfSmi(edi, &non_constructor);

  // Check if target has a [[Construct]] internal method.
  __ mov(ecx, FieldOperand(edi, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
            Immediate(Map::IsConstructorBit::kMask));
  __ j(zero, &non_constructor);

  // Dispatch based on instance type.
  __ CmpInstanceType(ecx, JS_FUNCTION_TYPE);
  __ j(not_equal, &non_jsfunction);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ bind(&non_jsfunction);
  __ CmpInstanceType(ecx, JS_BOUND_FUNCTION_TYPE);
  __ j(not_equal, &non_jsboundfunction);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET);

  // Only dispatch to proxies after checking whether they are constructors.
  __ bind(&non_jsboundfunction);
  __ CmpInstanceType(ecx, JS_PROXY_TYPE);
  __ j(not_equal, &non_proxy);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
  {
    // Overwrite the original receiver with the (original) target.
    __ mov(Operand(esp, eax, times_system_pointer_size, kSystemPointerSize),
           edi);
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadGlobalFunction(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, edi);
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
  //  -- eax : actual number of arguments
  //  -- ecx : expected number of arguments
  //  -- edx : new target (passed through to callee)
  //  -- edi : function (passed through to callee)
  // -----------------------------------

  const Register kExpectedNumberOfArgumentsRegister = ecx;

  Label invoke, dont_adapt_arguments, stack_overflow, enough, too_few;
  __ cmp(kExpectedNumberOfArgumentsRegister,
         SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  __ j(equal, &dont_adapt_arguments);
  __ cmp(eax, kExpectedNumberOfArgumentsRegister);
  __ j(less, &too_few);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    // edi is used as a scratch register. It should be restored from the frame
    // when needed.
    Generate_StackOverflowCheck(masm, kExpectedNumberOfArgumentsRegister, edi,
                                &stack_overflow);

    // Copy receiver and all expected arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(edi, Operand(ebp, eax, times_system_pointer_size, offset));
    __ mov(eax, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ inc(eax);
    __ push(Operand(edi, 0));
    __ sub(edi, Immediate(kSystemPointerSize));
    __ cmp(eax, kExpectedNumberOfArgumentsRegister);
    __ j(less, &copy);
    // eax now contains the expected number of arguments.
    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);
    // edi is used as a scratch register. It should be restored from the frame
    // when needed.
    Generate_StackOverflowCheck(masm, kExpectedNumberOfArgumentsRegister, edi,
                                &stack_overflow);

    // Remember expected arguments in xmm0.
    __ movd(xmm0, kExpectedNumberOfArgumentsRegister);

    // Copy receiver and all actual arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(edi, Operand(ebp, eax, times_system_pointer_size, offset));
    // ecx = expected - actual.
    __ sub(kExpectedNumberOfArgumentsRegister, eax);
    // eax = -actual - 1
    __ neg(eax);
    __ sub(eax, Immediate(1));

    Label copy;
    __ bind(&copy);
    __ inc(eax);
    __ push(Operand(edi, 0));
    __ sub(edi, Immediate(kSystemPointerSize));
    __ test(eax, eax);
    __ j(not_zero, &copy);

    // Fill remaining expected arguments with undefined values.
    Label fill;
    __ bind(&fill);
    __ inc(eax);
    __ Push(Immediate(masm->isolate()->factory()->undefined_value()));
    __ cmp(eax, kExpectedNumberOfArgumentsRegister);
    __ j(less, &fill);

    // Restore expected arguments.
    __ movd(eax, xmm0);
  }

  // Call the entry point.
  __ bind(&invoke);
  // Restore function pointer.
  __ mov(edi, Operand(ebp, ArgumentsAdaptorFrameConstants::kFunctionOffset));
  // eax : expected number of arguments
  // edx : new target (passed through to callee)
  // edi : function (passed through to callee)
  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  __ mov(ecx, FieldOperand(edi, JSFunction::kCodeOffset));
  __ CallCodeObject(ecx);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Leave frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ ret(0);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
  __ mov(ecx, FieldOperand(edi, JSFunction::kCodeOffset));
  __ JumpCodeObject(ecx);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();
  }
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ mov(eax, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(eax, Operand(eax, JavaScriptFrameConstants::kFunctionOffset));

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(eax);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  Label skip;
  // If the code object is null, just return to the caller.
  __ cmp(eax, Immediate(0));
  __ j(not_equal, &skip, Label::kNear);
  __ ret(0);

  __ bind(&skip);

  // Drop the handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  __ leave();

  // Load deoptimization data from the code object.
  __ mov(ecx, Operand(eax, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  __ mov(ecx, Operand(ecx, FixedArray::OffsetOfElementAt(
                               DeoptimizationData::kOsrPcOffsetIndex) -
                               kHeapObjectTag));
  __ SmiUntag(ecx);

  // Compute the target address = code_obj + header_size + osr_offset
  __ lea(eax, Operand(eax, ecx, times_1, Code::kHeaderSize - kHeapObjectTag));

  // Overwrite the return address on the stack.
  __ mov(Operand(esp, 0), eax);

  // And "return" to the OSR entry point of the function.
  __ ret(0);
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was put in edi by the jump table trampoline.
  // Convert to Smi for the runtime call.
  __ SmiTag(kWasmCompileLazyFuncIndexRegister);
  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameScope scope(masm, StackFrame::WASM_COMPILE_LAZY);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    static_assert(WasmCompileLazyFrameConstants::kNumberOfSavedGpParamRegs ==
                      arraysize(wasm::kGpParamRegisters),
                  "frame size mismatch");
    for (Register reg : wasm::kGpParamRegisters) {
      __ Push(reg);
    }
    static_assert(WasmCompileLazyFrameConstants::kNumberOfSavedFpParamRegs ==
                      arraysize(wasm::kFpParamRegisters),
                  "frame size mismatch");
    __ AllocateStackSpace(kSimd128Size * arraysize(wasm::kFpParamRegisters));
    int offset = 0;
    for (DoubleRegister reg : wasm::kFpParamRegisters) {
      __ movdqu(Operand(esp, offset), reg);
      offset += kSimd128Size;
    }

    // Push the WASM instance as an explicit argument to WasmCompileLazy.
    __ Push(kWasmInstanceRegister);
    // Push the function index as second argument.
    __ Push(kWasmCompileLazyFuncIndexRegister);
    // Load the correct CEntry builtin from the instance object.
    __ mov(ecx, FieldOperand(kWasmInstanceRegister,
                             WasmInstanceObject::kIsolateRootOffset));
    auto centry_id =
        Builtins::kCEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit;
    __ mov(ecx, MemOperand(ecx, IsolateData::builtin_slot_offset(centry_id)));
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Move(kContextRegister, Smi::zero());
    {
      // At this point, ebx has been spilled to the stack but is not yet
      // overwritten with another value. We can still use it as kRootRegister.
      __ CallRuntimeWithCEntry(Runtime::kWasmCompileLazy, ecx);
    }
    // The entrypoint address is the return value.
    __ mov(edi, kReturnRegister0);

    // Restore registers.
    for (DoubleRegister reg : base::Reversed(wasm::kFpParamRegisters)) {
      offset -= kSimd128Size;
      __ movdqu(reg, Operand(esp, offset));
    }
    DCHECK_EQ(0, offset);
    __ add(esp, Immediate(kSimd128Size * arraysize(wasm::kFpParamRegisters)));
    for (Register reg : base::Reversed(wasm::kGpParamRegisters)) {
      __ Pop(reg);
    }
  }
  // Finally, jump to the entrypoint.
  __ jmp(edi);
}

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               SaveFPRegsMode save_doubles, ArgvMode argv_mode,
                               bool builtin_exit_frame) {
  // eax: number of arguments including receiver
  // edx: pointer to C function
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // esi: current context (C callee-saved)
  // edi: JS function of the caller (C callee-saved)
  //
  // If argv_mode == kArgvInRegister:
  // ecx: pointer to the first argument

  STATIC_ASSERT(eax == kRuntimeCallArgCountRegister);
  STATIC_ASSERT(ecx == kRuntimeCallArgvRegister);
  STATIC_ASSERT(edx == kRuntimeCallFunctionRegister);
  STATIC_ASSERT(esi == kContextRegister);
  STATIC_ASSERT(edi == kJSFunctionRegister);

  DCHECK(!AreAliased(kRuntimeCallArgCountRegister, kRuntimeCallArgvRegister,
                     kRuntimeCallFunctionRegister, kContextRegister,
                     kJSFunctionRegister, kRootRegister));

  // Reserve space on the stack for the three arguments passed to the call. If
  // result size is greater than can be returned in registers, also reserve
  // space for the hidden argument for the result location, and space for the
  // result itself.
  int arg_stack_space = 3;

  // Enter the exit frame that transitions from JavaScript to C++.
  if (argv_mode == kArgvInRegister) {
    DCHECK(save_doubles == kDontSaveFPRegs);
    DCHECK(!builtin_exit_frame);
    __ EnterApiExitFrame(arg_stack_space, edi);

    // Move argc and argv into the correct registers.
    __ mov(esi, ecx);
    __ mov(edi, eax);
  } else {
    __ EnterExitFrame(
        arg_stack_space, save_doubles == kSaveFPRegs,
        builtin_exit_frame ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);
  }

  // edx: pointer to C function
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // edi: number of arguments including receiver  (C callee-saved)
  // esi: pointer to the first argument (C callee-saved)

  // Result returned in eax, or eax+edx if result size is 2.

  // Check stack alignment.
  if (FLAG_debug_code) {
    __ CheckStackAlignment();
  }
  // Call C function.
  __ mov(Operand(esp, 0 * kSystemPointerSize), edi);  // argc.
  __ mov(Operand(esp, 1 * kSystemPointerSize), esi);  // argv.
  __ Move(ecx, Immediate(ExternalReference::isolate_address(masm->isolate())));
  __ mov(Operand(esp, 2 * kSystemPointerSize), ecx);
  __ call(kRuntimeCallFunctionRegister);

  // Result is in eax or edx:eax - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(eax, RootIndex::kException);
  __ j(equal, &exception_returned);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    __ push(edx);
    __ LoadRoot(edx, RootIndex::kTheHoleValue);
    Label okay;
    ExternalReference pending_exception_address = ExternalReference::Create(
        IsolateAddressId::kPendingExceptionAddress, masm->isolate());
    __ cmp(edx, __ ExternalReferenceAsOperand(pending_exception_address, ecx));
    // Cannot use check here as it attempts to generate call into runtime.
    __ j(equal, &okay, Label::kNear);
    __ int3();
    __ bind(&okay);
    __ pop(edx);
  }

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(save_doubles == kSaveFPRegs, argv_mode == kArgvOnStack);
  __ ret(0);

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

  // Ask the runtime for help to determine the handler. This will set eax to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler =
      ExternalReference::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, eax);
    __ mov(Operand(esp, 0 * kSystemPointerSize), Immediate(0));  // argc.
    __ mov(Operand(esp, 1 * kSystemPointerSize), Immediate(0));  // argv.
    __ Move(esi,
            Immediate(ExternalReference::isolate_address(masm->isolate())));
    __ mov(Operand(esp, 2 * kSystemPointerSize), esi);
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ mov(esp, __ ExternalReferenceAsOperand(pending_handler_sp_address, esi));
  __ mov(ebp, __ ExternalReferenceAsOperand(pending_handler_fp_address, esi));
  __ mov(esi,
         __ ExternalReferenceAsOperand(pending_handler_context_address, esi));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (esi == 0) for non-JS frames.
  Label skip;
  __ test(esi, esi);
  __ j(zero, &skip, Label::kNear);
  __ mov(Operand(ebp, StandardFrameConstants::kContextOffset), esi);
  __ bind(&skip);

  // Compute the handler entry address and jump to it.
  __ mov(edi, __ ExternalReferenceAsOperand(pending_handler_entrypoint_address,
                                            edi));
  __ jmp(edi);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label check_negative, process_64_bits, done;

  // Account for return address and saved regs.
  const int kArgumentOffset = 4 * kSystemPointerSize;

  MemOperand mantissa_operand(MemOperand(esp, kArgumentOffset));
  MemOperand exponent_operand(
      MemOperand(esp, kArgumentOffset + kDoubleSize / 2));

  // The result is returned on the stack.
  MemOperand return_operand = mantissa_operand;

  Register scratch1 = ebx;

  // Since we must use ecx for shifts below, use some other register (eax)
  // to calculate the result.
  Register result_reg = eax;
  // Save ecx if it isn't the return register and therefore volatile, or if it
  // is the return register, then save the temp register we use in its stead for
  // the result.
  Register save_reg = eax;
  __ push(ecx);
  __ push(scratch1);
  __ push(save_reg);

  __ mov(scratch1, mantissa_operand);
  if (CpuFeatures::IsSupported(SSE3)) {
    CpuFeatureScope scope(masm, SSE3);
    // Load x87 register with heap number.
    __ fld_d(mantissa_operand);
  }
  __ mov(ecx, exponent_operand);

  __ and_(ecx, HeapNumber::kExponentMask);
  __ shr(ecx, HeapNumber::kExponentShift);
  __ lea(result_reg, MemOperand(ecx, -HeapNumber::kExponentBias));
  __ cmp(result_reg, Immediate(HeapNumber::kMantissaBits));
  __ j(below, &process_64_bits);

  // Result is entirely in lower 32-bits of mantissa
  int delta = HeapNumber::kExponentBias + Double::kPhysicalSignificandSize;
  if (CpuFeatures::IsSupported(SSE3)) {
    __ fstp(0);
  }
  __ sub(ecx, Immediate(delta));
  __ xor_(result_reg, result_reg);
  __ cmp(ecx, Immediate(31));
  __ j(above, &done);
  __ shl_cl(scratch1);
  __ jmp(&check_negative);

  __ bind(&process_64_bits);
  if (CpuFeatures::IsSupported(SSE3)) {
    CpuFeatureScope scope(masm, SSE3);
    // Reserve space for 64 bit answer.
    __ AllocateStackSpace(kDoubleSize);  // Nolint.
    // Do conversion, which cannot fail because we checked the exponent.
    __ fisttp_d(Operand(esp, 0));
    __ mov(result_reg, Operand(esp, 0));  // Load low word of answer as result
    __ add(esp, Immediate(kDoubleSize));
    __ jmp(&done);
  } else {
    // Result must be extracted from shifted 32-bit mantissa
    __ sub(ecx, Immediate(delta));
    __ neg(ecx);
    __ mov(result_reg, exponent_operand);
    __ and_(result_reg,
            Immediate(static_cast<uint32_t>(Double::kSignificandMask >> 32)));
    __ add(result_reg,
           Immediate(static_cast<uint32_t>(Double::kHiddenBit >> 32)));
    __ shrd_cl(scratch1, result_reg);
    __ shr_cl(result_reg);
    __ test(ecx, Immediate(32));
    __ cmov(not_equal, scratch1, result_reg);
  }

  // If the double was negative, negate the integer result.
  __ bind(&check_negative);
  __ mov(result_reg, scratch1);
  __ neg(result_reg);
  __ cmp(exponent_operand, Immediate(0));
  __ cmov(greater, result_reg, scratch1);

  // Restore registers
  __ bind(&done);
  __ mov(return_operand, result_reg);
  __ pop(save_reg);
  __ pop(scratch1);
  __ pop(ecx);
  __ ret(0);
}

void Builtins::Generate_InternalArrayConstructorImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argc
  //  -- edi : constructor
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_zero, AbortReason::kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(ecx, MAP_TYPE, ecx);
    __ Assert(equal, AbortReason::kUnexpectedInitialMapForArrayFunction);

    // Figure out the right elements kind
    __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));

    // Load the map's "bit field 2" into |result|. We only need the first byte,
    // but the following masking takes care of that anyway.
    __ mov(ecx, FieldOperand(ecx, Map::kBitField2Offset));
    // Retrieve elements_kind from bit field 2.
    __ DecodeField<Map::ElementsKindBits>(ecx);

    // Initial elements kind should be packed elements.
    __ cmp(ecx, Immediate(PACKED_ELEMENTS));
    __ Assert(equal, AbortReason::kInvalidElementsKindForInternalPackedArray);

    // No arguments should be passed.
    __ test(eax, eax);
    __ Assert(zero, AbortReason::kWrongNumberOfArgumentsForInternalPackedArray);
  }

  __ Jump(
      BUILTIN_CODE(masm->isolate(), InternalArrayNoArgumentConstructor_Packed),
      RelocInfo::CODE_TARGET);
}

namespace {

// Generates an Operand for saving parameters after PrepareCallApiFunction.
Operand ApiParameterOperand(int index) {
  return Operand(esp, index * kSystemPointerSize);
}

// Prepares stack to put arguments (aligns and so on). Reserves
// space for return value if needed (assumes the return value is a handle).
// Arguments must be stored in ApiParameterOperand(0), ApiParameterOperand(1)
// etc. Saves context (esi). If space was reserved for return value then
// stores the pointer to the reserved slot into esi.
void PrepareCallApiFunction(MacroAssembler* masm, int argc, Register scratch) {
  __ EnterApiExitFrame(argc, scratch);
  if (__ emit_debug_code()) {
    __ mov(esi, Immediate(bit_cast<int32_t>(kZapValue)));
  }
}

// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Clobbers esi, edi and
// caller-save registers.  Restores context.  On return removes
// stack_space * kSystemPointerSize (GCed).
void CallApiFunctionAndReturn(MacroAssembler* masm, Register function_address,
                              ExternalReference thunk_ref,
                              Operand thunk_last_arg, int stack_space,
                              Operand* stack_space_operand,
                              Operand return_value_operand) {
  Isolate* isolate = masm->isolate();

  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  ExternalReference limit_address =
      ExternalReference::handle_scope_limit_address(isolate);
  ExternalReference level_address =
      ExternalReference::handle_scope_level_address(isolate);

  DCHECK(edx == function_address);
  // Allocate HandleScope in callee-save registers.
  __ add(__ ExternalReferenceAsOperand(level_address, esi), Immediate(1));
  __ mov(esi, __ ExternalReferenceAsOperand(next_address, esi));
  __ mov(edi, __ ExternalReferenceAsOperand(limit_address, edi));

  Label profiler_enabled, end_profiler_check;
  __ Move(eax, Immediate(ExternalReference::is_profiling_address(isolate)));
  __ cmpb(Operand(eax, 0), Immediate(0));
  __ j(not_zero, &profiler_enabled);
  __ Move(eax, Immediate(ExternalReference::address_of_runtime_stats_flag()));
  __ cmp(Operand(eax, 0), Immediate(0));
  __ j(not_zero, &profiler_enabled);
  {
    // Call the api function directly.
    __ mov(eax, function_address);
    __ jmp(&end_profiler_check);
  }
  __ bind(&profiler_enabled);
  {
    // Additional parameter is the address of the actual getter function.
    __ mov(thunk_last_arg, function_address);
    __ Move(eax, Immediate(thunk_ref));
  }
  __ bind(&end_profiler_check);

  // Call the api function.
  __ call(eax);

  Label prologue;
  // Load the value from ReturnValue
  __ mov(eax, return_value_operand);

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;

  __ bind(&prologue);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ mov(__ ExternalReferenceAsOperand(next_address, ecx), esi);
  __ sub(__ ExternalReferenceAsOperand(level_address, ecx), Immediate(1));
  __ Assert(above_equal, AbortReason::kInvalidHandleScopeLevel);
  __ cmp(edi, __ ExternalReferenceAsOperand(limit_address, ecx));
  __ j(not_equal, &delete_allocated_handles);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  if (stack_space_operand != nullptr) {
    DCHECK_EQ(stack_space, 0);
    __ mov(edx, *stack_space_operand);
  }
  __ LeaveApiExitFrame();

  // Check if the function scheduled an exception.
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address(isolate);
  __ mov(ecx, __ ExternalReferenceAsOperand(scheduled_exception_address, ecx));
  __ CompareRoot(ecx, RootIndex::kTheHoleValue);
  __ j(not_equal, &promote_scheduled_exception);

#if DEBUG
  // Check if the function returned a valid JavaScript value.
  Label ok;
  Register return_value = eax;
  Register map = ecx;

  __ JumpIfSmi(return_value, &ok, Label::kNear);
  __ mov(map, FieldOperand(return_value, HeapObject::kMapOffset));

  __ CmpInstanceType(map, LAST_NAME_TYPE);
  __ j(below_equal, &ok, Label::kNear);

  __ CmpInstanceType(map, FIRST_JS_RECEIVER_TYPE);
  __ j(above_equal, &ok, Label::kNear);

  __ CompareRoot(map, RootIndex::kHeapNumberMap);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(map, RootIndex::kBigIntMap);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, RootIndex::kUndefinedValue);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, RootIndex::kTrueValue);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, RootIndex::kFalseValue);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, RootIndex::kNullValue);
  __ j(equal, &ok, Label::kNear);

  __ Abort(AbortReason::kAPICallReturnedInvalidObject);

  __ bind(&ok);
#endif

  if (stack_space_operand == nullptr) {
    DCHECK_NE(stack_space, 0);
    __ ret(stack_space * kSystemPointerSize);
  } else {
    DCHECK_EQ(0, stack_space);
    __ pop(ecx);
    __ add(esp, edx);
    __ jmp(ecx);
  }

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  ExternalReference delete_extensions =
      ExternalReference::delete_handle_scope_extensions();
  __ bind(&delete_allocated_handles);
  __ mov(__ ExternalReferenceAsOperand(limit_address, ecx), edi);
  __ mov(edi, eax);
  __ Move(eax, Immediate(ExternalReference::isolate_address(isolate)));
  __ mov(Operand(esp, 0), eax);
  __ Move(eax, Immediate(delete_extensions));
  __ call(eax);
  __ mov(eax, edi);
  __ jmp(&leave_exit_frame);
}

}  // namespace

void Builtins::Generate_CallApiCallback(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- esi                 : context
  //  -- edx                 : api function address
  //  -- ecx                 : arguments count (not including the receiver)
  //  -- eax                 : call data
  //  -- edi                 : holder
  //  -- esp[0]              : return address
  //  -- esp[4]              : last argument
  //  -- ...
  //  -- esp[argc * 4]       : first argument
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  Register api_function_address = edx;
  Register argc = ecx;
  Register call_data = eax;
  Register holder = edi;

  // Park argc in xmm0.
  __ movd(xmm0, argc);

  DCHECK(!AreAliased(api_function_address, argc, holder));

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
  // Current state:
  //   esp[0]: return address
  //
  // Target state:
  //   esp[0 * kSystemPointerSize]: return address
  //   esp[1 * kSystemPointerSize]: kHolder
  //   esp[2 * kSystemPointerSize]: kIsolate
  //   esp[3 * kSystemPointerSize]: undefined (kReturnValueDefaultValue)
  //   esp[4 * kSystemPointerSize]: undefined (kReturnValue)
  //   esp[5 * kSystemPointerSize]: kData
  //   esp[6 * kSystemPointerSize]: undefined (kNewTarget)

  __ PopReturnAddressTo(ecx);
  __ PushRoot(RootIndex::kUndefinedValue);
  __ Push(call_data);
  __ PushRoot(RootIndex::kUndefinedValue);
  __ PushRoot(RootIndex::kUndefinedValue);
  __ Push(Immediate(ExternalReference::isolate_address(masm->isolate())));
  __ Push(holder);
  __ PushReturnAddressFrom(ecx);

  // Reload argc from xmm0.
  __ movd(argc, xmm0);

  // Keep a pointer to kHolder (= implicit_args) in a scratch register.
  // We use it below to set up the FunctionCallbackInfo object.
  Register scratch = eax;
  __ lea(scratch, Operand(esp, 1 * kSystemPointerSize));

  // The API function takes a reference to v8::Arguments. If the CPU profiler
  // is enabled, a wrapper function will be called and we need to pass
  // the address of the callback as an additional parameter. Always allocate
  // space for it.
  static constexpr int kApiArgc = 1 + 1;

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  static constexpr int kApiStackSpace = 4;

  PrepareCallApiFunction(masm, kApiArgc + kApiStackSpace, edi);

  // FunctionCallbackInfo::implicit_args_ (points at kHolder as set up above).
  __ mov(ApiParameterOperand(kApiArgc + 0), scratch);

  // FunctionCallbackInfo::values_ (points at the first varargs argument passed
  // on the stack).
  __ lea(scratch, Operand(scratch, argc, times_system_pointer_size,
                          (FCA::kArgsLength - 1) * kSystemPointerSize));
  __ mov(ApiParameterOperand(kApiArgc + 1), scratch);

  // FunctionCallbackInfo::length_.
  __ mov(ApiParameterOperand(kApiArgc + 2), argc);

  // We also store the number of bytes to drop from the stack after returning
  // from the API function here.
  __ lea(scratch,
         Operand(argc, times_system_pointer_size,
                 (FCA::kArgsLength + 1 /* receiver */) * kSystemPointerSize));
  __ mov(ApiParameterOperand(kApiArgc + 3), scratch);

  // v8::InvocationCallback's argument.
  __ lea(scratch, ApiParameterOperand(kApiArgc + 0));
  __ mov(ApiParameterOperand(0), scratch);

  ExternalReference thunk_ref = ExternalReference::invoke_function_callback();

  // There are two stack slots above the arguments we constructed on the stack:
  // the stored ebp (pushed by EnterApiExitFrame), and the return address.
  static constexpr int kStackSlotsAboveFCA = 2;
  Operand return_value_operand(
      ebp,
      (kStackSlotsAboveFCA + FCA::kReturnValueOffset) * kSystemPointerSize);

  static constexpr int kUseStackSpaceOperand = 0;
  Operand stack_space_operand = ApiParameterOperand(kApiArgc + 3);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           ApiParameterOperand(1), kUseStackSpaceOperand,
                           &stack_space_operand, return_value_operand);
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
  Register scratch = edi;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  __ pop(scratch);  // Pop return address to extend the frame.
  __ push(receiver);
  __ push(FieldOperand(callback, AccessorInfo::kDataOffset));
  __ PushRoot(RootIndex::kUndefinedValue);  // ReturnValue
  // ReturnValue default value
  __ PushRoot(RootIndex::kUndefinedValue);
  __ Push(Immediate(ExternalReference::isolate_address(masm->isolate())));
  __ push(holder);
  __ push(Immediate(Smi::zero()));  // should_throw_on_error -> false
  __ push(FieldOperand(callback, AccessorInfo::kNameOffset));
  __ push(scratch);  // Restore return address.

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Allocate v8::PropertyCallbackInfo object, arguments for callback and
  // space for optional callback address parameter (in case CPU profiler is
  // active) in non-GCed stack space.
  const int kApiArgc = 3 + 1;

  PrepareCallApiFunction(masm, kApiArgc, scratch);

  // Load address of v8::PropertyAccessorInfo::args_ array. The value in ebp
  // here corresponds to esp + kSystemPointerSize before PrepareCallApiFunction.
  __ lea(scratch, Operand(ebp, kSystemPointerSize + 2 * kSystemPointerSize));
  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  Operand info_object = ApiParameterOperand(3);
  __ mov(info_object, scratch);

  // Name as handle.
  __ sub(scratch, Immediate(kSystemPointerSize));
  __ mov(ApiParameterOperand(0), scratch);
  // Arguments pointer.
  __ lea(scratch, info_object);
  __ mov(ApiParameterOperand(1), scratch);
  // Reserve space for optional callback address parameter.
  Operand thunk_last_arg = ApiParameterOperand(2);

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback();

  __ mov(scratch, FieldOperand(callback, AccessorInfo::kJsGetterOffset));
  Register function_address = edx;
  __ mov(function_address,
         FieldOperand(scratch, Foreign::kForeignAddressOffset));
  // +3 is to skip prolog, return address and name handle.
  Operand return_value_operand(
      ebp,
      (PropertyCallbackArguments::kReturnValueOffset + 3) * kSystemPointerSize);
  Operand* const kUseStackSpaceConstant = nullptr;
  CallApiFunctionAndReturn(masm, function_address, thunk_ref, thunk_last_arg,
                           kStackUnwindSpace, kUseStackSpaceConstant,
                           return_value_operand);
}

void Builtins::Generate_DirectCEntry(MacroAssembler* masm) {
  __ int3();  // Unused on this architecture.
}

namespace {

enum Direction { FORWARD, BACKWARD };
enum Alignment { MOVE_ALIGNED, MOVE_UNALIGNED };

// Expects registers:
// esi - source, aligned if alignment == ALIGNED
// edi - destination, always aligned
// ecx - count (copy size in bytes)
// edx - loop count (number of 64 byte chunks)
void MemMoveEmitMainLoop(MacroAssembler* masm, Label* move_last_15,
                         Direction direction, Alignment alignment) {
  Register src = esi;
  Register dst = edi;
  Register count = ecx;
  Register loop_count = edx;
  Label loop, move_last_31, move_last_63;
  __ cmp(loop_count, 0);
  __ j(equal, &move_last_63);
  __ bind(&loop);
  // Main loop. Copy in 64 byte chunks.
  if (direction == BACKWARD) __ sub(src, Immediate(0x40));
  __ movdq(alignment == MOVE_ALIGNED, xmm0, Operand(src, 0x00));
  __ movdq(alignment == MOVE_ALIGNED, xmm1, Operand(src, 0x10));
  __ movdq(alignment == MOVE_ALIGNED, xmm2, Operand(src, 0x20));
  __ movdq(alignment == MOVE_ALIGNED, xmm3, Operand(src, 0x30));
  if (direction == FORWARD) __ add(src, Immediate(0x40));
  if (direction == BACKWARD) __ sub(dst, Immediate(0x40));
  __ movdqa(Operand(dst, 0x00), xmm0);
  __ movdqa(Operand(dst, 0x10), xmm1);
  __ movdqa(Operand(dst, 0x20), xmm2);
  __ movdqa(Operand(dst, 0x30), xmm3);
  if (direction == FORWARD) __ add(dst, Immediate(0x40));
  __ dec(loop_count);
  __ j(not_zero, &loop);
  // At most 63 bytes left to copy.
  __ bind(&move_last_63);
  __ test(count, Immediate(0x20));
  __ j(zero, &move_last_31);
  if (direction == BACKWARD) __ sub(src, Immediate(0x20));
  __ movdq(alignment == MOVE_ALIGNED, xmm0, Operand(src, 0x00));
  __ movdq(alignment == MOVE_ALIGNED, xmm1, Operand(src, 0x10));
  if (direction == FORWARD) __ add(src, Immediate(0x20));
  if (direction == BACKWARD) __ sub(dst, Immediate(0x20));
  __ movdqa(Operand(dst, 0x00), xmm0);
  __ movdqa(Operand(dst, 0x10), xmm1);
  if (direction == FORWARD) __ add(dst, Immediate(0x20));
  // At most 31 bytes left to copy.
  __ bind(&move_last_31);
  __ test(count, Immediate(0x10));
  __ j(zero, move_last_15);
  if (direction == BACKWARD) __ sub(src, Immediate(0x10));
  __ movdq(alignment == MOVE_ALIGNED, xmm0, Operand(src, 0));
  if (direction == FORWARD) __ add(src, Immediate(0x10));
  if (direction == BACKWARD) __ sub(dst, Immediate(0x10));
  __ movdqa(Operand(dst, 0), xmm0);
  if (direction == FORWARD) __ add(dst, Immediate(0x10));
}

void MemMoveEmitPopAndReturn(MacroAssembler* masm) {
  __ pop(esi);
  __ pop(edi);
  __ ret(0);
}

}  // namespace

void Builtins::Generate_MemMove(MacroAssembler* masm) {
  // Generated code is put into a fixed, unmovable buffer, and not into
  // the V8 heap. We can't, and don't, refer to any relocatable addresses
  // (e.g. the JavaScript nan-object).

  // 32-bit C declaration function calls pass arguments on stack.

  // Stack layout:
  // esp[12]: Third argument, size.
  // esp[8]: Second argument, source pointer.
  // esp[4]: First argument, destination pointer.
  // esp[0]: return address

  const int kDestinationOffset = 1 * kSystemPointerSize;
  const int kSourceOffset = 2 * kSystemPointerSize;
  const int kSizeOffset = 3 * kSystemPointerSize;

  // When copying up to this many bytes, use special "small" handlers.
  const size_t kSmallCopySize = 8;
  // When copying up to this many bytes, use special "medium" handlers.
  const size_t kMediumCopySize = 63;
  // When non-overlapping region of src and dst is less than this,
  // use a more careful implementation (slightly slower).
  const size_t kMinMoveDistance = 16;
  // Note that these values are dictated by the implementation below,
  // do not just change them and hope things will work!

  int stack_offset = 0;  // Update if we change the stack height.

  Label backward, backward_much_overlap;
  Label forward_much_overlap, small_size, medium_size, pop_and_return;
  __ push(edi);
  __ push(esi);
  stack_offset += 2 * kSystemPointerSize;
  Register dst = edi;
  Register src = esi;
  Register count = ecx;
  Register loop_count = edx;
  __ mov(dst, Operand(esp, stack_offset + kDestinationOffset));
  __ mov(src, Operand(esp, stack_offset + kSourceOffset));
  __ mov(count, Operand(esp, stack_offset + kSizeOffset));

  __ cmp(dst, src);
  __ j(equal, &pop_and_return);

  __ prefetch(Operand(src, 0), 1);
  __ cmp(count, kSmallCopySize);
  __ j(below_equal, &small_size);
  __ cmp(count, kMediumCopySize);
  __ j(below_equal, &medium_size);
  __ cmp(dst, src);
  __ j(above, &backward);

  {
    // |dst| is a lower address than |src|. Copy front-to-back.
    Label unaligned_source, move_last_15, skip_last_move;
    __ mov(eax, src);
    __ sub(eax, dst);
    __ cmp(eax, kMinMoveDistance);
    __ j(below, &forward_much_overlap);
    // Copy first 16 bytes.
    __ movdqu(xmm0, Operand(src, 0));
    __ movdqu(Operand(dst, 0), xmm0);
    // Determine distance to alignment: 16 - (dst & 0xF).
    __ mov(edx, dst);
    __ and_(edx, 0xF);
    __ neg(edx);
    __ add(edx, Immediate(16));
    __ add(dst, edx);
    __ add(src, edx);
    __ sub(count, edx);
    // dst is now aligned. Main copy loop.
    __ mov(loop_count, count);
    __ shr(loop_count, 6);
    // Check if src is also aligned.
    __ test(src, Immediate(0xF));
    __ j(not_zero, &unaligned_source);
    // Copy loop for aligned source and destination.
    MemMoveEmitMainLoop(masm, &move_last_15, FORWARD, MOVE_ALIGNED);
    // At most 15 bytes to copy. Copy 16 bytes at end of string.
    __ bind(&move_last_15);
    __ and_(count, 0xF);
    __ j(zero, &skip_last_move, Label::kNear);
    __ movdqu(xmm0, Operand(src, count, times_1, -0x10));
    __ movdqu(Operand(dst, count, times_1, -0x10), xmm0);
    __ bind(&skip_last_move);
    MemMoveEmitPopAndReturn(masm);

    // Copy loop for unaligned source and aligned destination.
    __ bind(&unaligned_source);
    MemMoveEmitMainLoop(masm, &move_last_15, FORWARD, MOVE_UNALIGNED);
    __ jmp(&move_last_15);

    // Less than kMinMoveDistance offset between dst and src.
    Label loop_until_aligned, last_15_much_overlap;
    __ bind(&loop_until_aligned);
    __ mov_b(eax, Operand(src, 0));
    __ inc(src);
    __ mov_b(Operand(dst, 0), eax);
    __ inc(dst);
    __ dec(count);
    __ bind(&forward_much_overlap);  // Entry point into this block.
    __ test(dst, Immediate(0xF));
    __ j(not_zero, &loop_until_aligned);
    // dst is now aligned, src can't be. Main copy loop.
    __ mov(loop_count, count);
    __ shr(loop_count, 6);
    MemMoveEmitMainLoop(masm, &last_15_much_overlap, FORWARD, MOVE_UNALIGNED);
    __ bind(&last_15_much_overlap);
    __ and_(count, 0xF);
    __ j(zero, &pop_and_return);
    __ cmp(count, kSmallCopySize);
    __ j(below_equal, &small_size);
    __ jmp(&medium_size);
  }

  {
    // |dst| is a higher address than |src|. Copy backwards.
    Label unaligned_source, move_first_15, skip_last_move;
    __ bind(&backward);
    // |dst| and |src| always point to the end of what's left to copy.
    __ add(dst, count);
    __ add(src, count);
    __ mov(eax, dst);
    __ sub(eax, src);
    __ cmp(eax, kMinMoveDistance);
    __ j(below, &backward_much_overlap);
    // Copy last 16 bytes.
    __ movdqu(xmm0, Operand(src, -0x10));
    __ movdqu(Operand(dst, -0x10), xmm0);
    // Find distance to alignment: dst & 0xF
    __ mov(edx, dst);
    __ and_(edx, 0xF);
    __ sub(dst, edx);
    __ sub(src, edx);
    __ sub(count, edx);
    // dst is now aligned. Main copy loop.
    __ mov(loop_count, count);
    __ shr(loop_count, 6);
    // Check if src is also aligned.
    __ test(src, Immediate(0xF));
    __ j(not_zero, &unaligned_source);
    // Copy loop for aligned source and destination.
    MemMoveEmitMainLoop(masm, &move_first_15, BACKWARD, MOVE_ALIGNED);
    // At most 15 bytes to copy. Copy 16 bytes at beginning of string.
    __ bind(&move_first_15);
    __ and_(count, 0xF);
    __ j(zero, &skip_last_move, Label::kNear);
    __ sub(src, count);
    __ sub(dst, count);
    __ movdqu(xmm0, Operand(src, 0));
    __ movdqu(Operand(dst, 0), xmm0);
    __ bind(&skip_last_move);
    MemMoveEmitPopAndReturn(masm);

    // Copy loop for unaligned source and aligned destination.
    __ bind(&unaligned_source);
    MemMoveEmitMainLoop(masm, &move_first_15, BACKWARD, MOVE_UNALIGNED);
    __ jmp(&move_first_15);

    // Less than kMinMoveDistance offset between dst and src.
    Label loop_until_aligned, first_15_much_overlap;
    __ bind(&loop_until_aligned);
    __ dec(src);
    __ dec(dst);
    __ mov_b(eax, Operand(src, 0));
    __ mov_b(Operand(dst, 0), eax);
    __ dec(count);
    __ bind(&backward_much_overlap);  // Entry point into this block.
    __ test(dst, Immediate(0xF));
    __ j(not_zero, &loop_until_aligned);
    // dst is now aligned, src can't be. Main copy loop.
    __ mov(loop_count, count);
    __ shr(loop_count, 6);
    MemMoveEmitMainLoop(masm, &first_15_much_overlap, BACKWARD, MOVE_UNALIGNED);
    __ bind(&first_15_much_overlap);
    __ and_(count, 0xF);
    __ j(zero, &pop_and_return);
    // Small/medium handlers expect dst/src to point to the beginning.
    __ sub(dst, count);
    __ sub(src, count);
    __ cmp(count, kSmallCopySize);
    __ j(below_equal, &small_size);
    __ jmp(&medium_size);
  }
  {
    // Special handlers for 9 <= copy_size < 64. No assumptions about
    // alignment or move distance, so all reads must be unaligned and
    // must happen before any writes.
    Label f9_16, f17_32, f33_48, f49_63;

    __ bind(&f9_16);
    __ movsd(xmm0, Operand(src, 0));
    __ movsd(xmm1, Operand(src, count, times_1, -8));
    __ movsd(Operand(dst, 0), xmm0);
    __ movsd(Operand(dst, count, times_1, -8), xmm1);
    MemMoveEmitPopAndReturn(masm);

    __ bind(&f17_32);
    __ movdqu(xmm0, Operand(src, 0));
    __ movdqu(xmm1, Operand(src, count, times_1, -0x10));
    __ movdqu(Operand(dst, 0x00), xmm0);
    __ movdqu(Operand(dst, count, times_1, -0x10), xmm1);
    MemMoveEmitPopAndReturn(masm);

    __ bind(&f33_48);
    __ movdqu(xmm0, Operand(src, 0x00));
    __ movdqu(xmm1, Operand(src, 0x10));
    __ movdqu(xmm2, Operand(src, count, times_1, -0x10));
    __ movdqu(Operand(dst, 0x00), xmm0);
    __ movdqu(Operand(dst, 0x10), xmm1);
    __ movdqu(Operand(dst, count, times_1, -0x10), xmm2);
    MemMoveEmitPopAndReturn(masm);

    __ bind(&f49_63);
    __ movdqu(xmm0, Operand(src, 0x00));
    __ movdqu(xmm1, Operand(src, 0x10));
    __ movdqu(xmm2, Operand(src, 0x20));
    __ movdqu(xmm3, Operand(src, count, times_1, -0x10));
    __ movdqu(Operand(dst, 0x00), xmm0);
    __ movdqu(Operand(dst, 0x10), xmm1);
    __ movdqu(Operand(dst, 0x20), xmm2);
    __ movdqu(Operand(dst, count, times_1, -0x10), xmm3);
    MemMoveEmitPopAndReturn(masm);

    __ bind(&medium_size);  // Entry point into this block.
    __ mov(eax, count);
    __ dec(eax);
    __ shr(eax, 4);
    if (FLAG_debug_code) {
      Label ok;
      __ cmp(eax, 3);
      __ j(below_equal, &ok);
      __ int3();
      __ bind(&ok);
    }

    // Dispatch to handlers.
    Label eax_is_2_or_3;

    __ cmp(eax, 1);
    __ j(greater, &eax_is_2_or_3);
    __ j(less, &f9_16);  // eax == 0.
    __ jmp(&f17_32);     // eax == 1.

    __ bind(&eax_is_2_or_3);
    __ cmp(eax, 3);
    __ j(less, &f33_48);  // eax == 2.
    __ jmp(&f49_63);      // eax == 3.
  }
  {
    // Specialized copiers for copy_size <= 8 bytes.
    Label f0, f1, f2, f3, f4, f5_8;
    __ bind(&f0);
    MemMoveEmitPopAndReturn(masm);

    __ bind(&f1);
    __ mov_b(eax, Operand(src, 0));
    __ mov_b(Operand(dst, 0), eax);
    MemMoveEmitPopAndReturn(masm);

    __ bind(&f2);
    __ mov_w(eax, Operand(src, 0));
    __ mov_w(Operand(dst, 0), eax);
    MemMoveEmitPopAndReturn(masm);

    __ bind(&f3);
    __ mov_w(eax, Operand(src, 0));
    __ mov_b(edx, Operand(src, 2));
    __ mov_w(Operand(dst, 0), eax);
    __ mov_b(Operand(dst, 2), edx);
    MemMoveEmitPopAndReturn(masm);

    __ bind(&f4);
    __ mov(eax, Operand(src, 0));
    __ mov(Operand(dst, 0), eax);
    MemMoveEmitPopAndReturn(masm);

    __ bind(&f5_8);
    __ mov(eax, Operand(src, 0));
    __ mov(edx, Operand(src, count, times_1, -4));
    __ mov(Operand(dst, 0), eax);
    __ mov(Operand(dst, count, times_1, -4), edx);
    MemMoveEmitPopAndReturn(masm);

    __ bind(&small_size);  // Entry point into this block.
    if (FLAG_debug_code) {
      Label ok;
      __ cmp(count, 8);
      __ j(below_equal, &ok);
      __ int3();
      __ bind(&ok);
    }

    // Dispatch to handlers.
    Label count_is_above_3, count_is_2_or_3;

    __ cmp(count, 3);
    __ j(greater, &count_is_above_3);

    __ cmp(count, 1);
    __ j(greater, &count_is_2_or_3);
    __ j(less, &f0);  // count == 0.
    __ jmp(&f1);      // count == 1.

    __ bind(&count_is_2_or_3);
    __ cmp(count, 3);
    __ j(less, &f2);  // count == 2.
    __ jmp(&f3);      // count == 3.

    __ bind(&count_is_above_3);
    __ cmp(count, 5);
    __ j(less, &f4);  // count == 4.
    __ jmp(&f5_8);    // count in [5, 8[.
  }

  __ bind(&pop_and_return);
  MemMoveEmitPopAndReturn(masm);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32

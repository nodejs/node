// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/api/api-arguments.h"
#include "src/base/bits-iterator.h"
#include "src/base/iterator.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/interface-descriptors-inl.h"
// For interpreter_entry_return_pc_offset. TODO(jkummerow): Drop.
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/register-configuration.h"
#include "src/codegen/x64/assembler-x64.h"
#include "src/common/globals.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"
#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"
#include "src/objects/cell.h"
#include "src/objects/code.h"
#include "src/objects/debug-objects.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-generator.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/baseline/liftoff-assembler-defs.h"
#include "src/wasm/object-access.h"
#include "src/wasm/stacks.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address) {
  __ LoadAddress(kJavaScriptCallExtraArg1Register,
                 ExternalReference::Create(address));
  __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
          RelocInfo::CODE_TARGET);
}

namespace {

constexpr int kReceiverOnStackSize = kSystemPointerSize;

enum class ArgumentsElementType {
  kRaw,    // Push arguments as they are.
  kHandle  // Dereference arguments before pushing.
};

void Generate_PushArguments(MacroAssembler* masm, Register array, Register argc,
                            Register scratch,
                            ArgumentsElementType element_type) {
  DCHECK(!AreAliased(array, argc, scratch, kScratchRegister));
  Register counter = scratch;
  Label loop, entry;
  __ leaq(counter, Operand(argc, -kJSArgcReceiverSlots));
  __ jmp(&entry);
  __ bind(&loop);
  Operand value(array, counter, times_system_pointer_size, 0);
  if (element_type == ArgumentsElementType::kHandle) {
    __ movq(kScratchRegister, value);
    value = Operand(kScratchRegister, 0);
  }
  __ Push(value);
  __ bind(&entry);
  __ decq(counter);
  __ j(greater_equal, &loop, Label::kNear);
}

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  //  -- rdi: constructor function
  //  -- rdx: new target
  //  -- rsi: context
  // -----------------------------------

  Label stack_overflow;
  __ StackOverflowCheck(rax, &stack_overflow, Label::kFar);

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(rcx, rax);
    __ Push(rsi);
    __ Push(rcx);

    // TODO(victorgomes): When the arguments adaptor is completely removed, we
    // should get the formal parameter count and copy the arguments in its
    // correct position (including any undefined), instead of delaying this to
    // InvokeFunction.

    // Set up pointer to first argument (skip receiver).
    __ leaq(rbx, Operand(rbp, StandardFrameConstants::kFixedFrameSizeAboveFp +
                                  kSystemPointerSize));
    // Copy arguments to the expression stack.
    // rbx: Pointer to start of arguments.
    // rax: Number of arguments.
    Generate_PushArguments(masm, rbx, rax, rcx, ArgumentsElementType::kRaw);
    // The receiver for the builtin/api call.
    __ PushRoot(RootIndex::kTheHoleValue);

    // Call the function.
    // rax: number of arguments (untagged)
    // rdi: constructor function
    // rdx: new target
    __ InvokeFunction(rdi, rdx, rax, InvokeType::kCall);

    // Restore smi-tagged arguments count from the frame.
    __ movq(rbx, Operand(rbp, ConstructFrameConstants::kLengthOffset));

    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ DropArguments(rbx, rcx, MacroAssembler::kCountIsSmi,
                   MacroAssembler::kCountIncludesReceiver);

  __ ret(0);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();  // This should be unreachable.
  }
}

// Provides access to exit frame stack space (not GCed).
Operand ExitFrameStackSlotOperand(int index) {
#ifdef V8_TARGET_OS_WIN
  return Operand(rsp, (index + kWindowsHomeStackSlots) * kSystemPointerSize);
#else
  return Operand(rsp, index * kSystemPointerSize);
#endif
}

Operand ExitFrameCallerStackSlotOperand(int index) {
  return Operand(rbp,
                 (BuiltinExitFrameConstants::kFixedSlotCountAboveFp + index) *
                     kSystemPointerSize);
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax: number of arguments (untagged)
  //  -- rdi: constructor function
  //  -- rdx: new target
  //  -- rsi: context
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  FrameScope scope(masm, StackFrame::MANUAL);
  // Enter a construct frame.
  __ EnterFrame(StackFrame::CONSTRUCT);
  Label post_instantiation_deopt_entry, not_create_implicit_receiver;

  // Preserve the incoming parameters on the stack.
  __ SmiTag(rcx, rax);
  __ Push(rsi);
  __ Push(rcx);
  __ Push(rdi);
  __ PushRoot(RootIndex::kTheHoleValue);
  __ Push(rdx);

  // ----------- S t a t e -------------
  //  --         sp[0*kSystemPointerSize]: new target
  //  --         sp[1*kSystemPointerSize]: padding
  //  -- rdi and sp[2*kSystemPointerSize]: constructor function
  //  --         sp[3*kSystemPointerSize]: argument count
  //  --         sp[4*kSystemPointerSize]: context
  // -----------------------------------

  const TaggedRegister shared_function_info(rbx);
  __ LoadTaggedField(shared_function_info,
                     FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movl(rbx,
          FieldOperand(shared_function_info, SharedFunctionInfo::kFlagsOffset));
  __ DecodeField<SharedFunctionInfo::FunctionKindBits>(rbx);
  __ JumpIfIsInRange(
      rbx, static_cast<uint32_t>(FunctionKind::kDefaultDerivedConstructor),
      static_cast<uint32_t>(FunctionKind::kDerivedConstructor),
      &not_create_implicit_receiver, Label::kNear);

  // If not derived class constructor: Allocate the new receiver object.
  __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject), RelocInfo::CODE_TARGET);
  __ jmp(&post_instantiation_deopt_entry, Label::kNear);

  // Else: use TheHoleValue as receiver for constructor call
  __ bind(&not_create_implicit_receiver);
  __ LoadRoot(rax, RootIndex::kTheHoleValue);

  // ----------- S t a t e -------------
  //  -- rax                          implicit receiver
  //  -- Slot 4 / sp[0*kSystemPointerSize]  new target
  //  -- Slot 3 / sp[1*kSystemPointerSize]  padding
  //  -- Slot 2 / sp[2*kSystemPointerSize]  constructor function
  //  -- Slot 1 / sp[3*kSystemPointerSize]  number of arguments (tagged)
  //  -- Slot 0 / sp[4*kSystemPointerSize]  context
  // -----------------------------------
  // Deoptimizer enters here.
  masm->isolate()->heap()->SetConstructStubCreateDeoptPCOffset(
      masm->pc_offset());
  __ bind(&post_instantiation_deopt_entry);

  // Restore new target.
  __ Pop(rdx);

  // Push the allocated receiver to the stack.
  __ Push(rax);

  // We need two copies because we may have to return the original one
  // and the calling conventions dictate that the called function pops the
  // receiver. The second copy is pushed after the arguments, we saved in r8
  // since rax needs to store the number of arguments before
  // InvokingFunction.
  __ movq(r8, rax);

  // Set up pointer to first argument (skip receiver).
  __ leaq(rbx, Operand(rbp, StandardFrameConstants::kFixedFrameSizeAboveFp +
                                kSystemPointerSize));

  // Restore constructor function and argument count.
  __ movq(rdi, Operand(rbp, ConstructFrameConstants::kConstructorOffset));
  __ SmiUntagUnsigned(rax,
                      Operand(rbp, ConstructFrameConstants::kLengthOffset));

  // Check if we have enough stack space to push all arguments.
  // Argument count in rax.
  Label stack_overflow;
  __ StackOverflowCheck(rax, &stack_overflow);

  // TODO(victorgomes): When the arguments adaptor is completely removed, we
  // should get the formal parameter count and copy the arguments in its
  // correct position (including any undefined), instead of delaying this to
  // InvokeFunction.

  // Copy arguments to the expression stack.
  // rbx: Pointer to start of arguments.
  // rax: Number of arguments.
  Generate_PushArguments(masm, rbx, rax, rcx, ArgumentsElementType::kRaw);

  // Push implicit receiver.
  __ Push(r8);

  // Call the function.
  __ InvokeFunction(rdi, rdx, rax, InvokeType::kCall);

  // ----------- S t a t e -------------
  //  -- rax                 constructor result
  //  -- sp[0*kSystemPointerSize]  implicit receiver
  //  -- sp[1*kSystemPointerSize]  padding
  //  -- sp[2*kSystemPointerSize]  constructor function
  //  -- sp[3*kSystemPointerSize]  number of arguments
  //  -- sp[4*kSystemPointerSize]  context
  // -----------------------------------

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
      masm->pc_offset());

  // If the result is an object (in the ECMA sense), we should get rid
  // of the receiver and use the result; see ECMA-262 section 13.2.2-7
  // on page 74.
  Label use_receiver, do_throw, leave_and_return, check_result;

  // If the result is undefined, we'll use the implicit receiver. Otherwise we
  // do a smi check and fall through to check if the return value is a valid
  // receiver.
  __ JumpIfNotRoot(rax, RootIndex::kUndefinedValue, &check_result,
                   Label::kNear);

  // Throw away the result of the constructor invocation and use the
  // on-stack receiver as the result.
  __ bind(&use_receiver);
  __ movq(rax, Operand(rsp, 0 * kSystemPointerSize));
  __ JumpIfRoot(rax, RootIndex::kTheHoleValue, &do_throw, Label::kNear);

  __ bind(&leave_and_return);
  // Restore the arguments count.
  __ movq(rbx, Operand(rbp, ConstructFrameConstants::kLengthOffset));
  __ LeaveFrame(StackFrame::CONSTRUCT);
  // Remove caller arguments from the stack and return.
  __ DropArguments(rbx, rcx, MacroAssembler::kCountIsSmi,
                   MacroAssembler::kCountIncludesReceiver);
  __ ret(0);

  // If the result is a smi, it is *not* an object in the ECMA sense.
  __ bind(&check_result);
  __ JumpIfSmi(rax, &use_receiver, Label::kNear);

  // Check if the type of the result is not an object in the ECMA sense.
  __ JumpIfJSAnyIsNotPrimitive(rax, rcx, &leave_and_return, Label::kNear);
  __ jmp(&use_receiver);

  __ bind(&do_throw);
  // Restore context from the frame.
  __ movq(rsi, Operand(rbp, ConstructFrameConstants::kContextOffset));
  __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);
  // We don't return here.
  __ int3();

  __ bind(&stack_overflow);
  // Restore the context from the frame.
  __ movq(rsi, Operand(rbp, ConstructFrameConstants::kContextOffset));
  __ CallRuntime(Runtime::kThrowStackOverflow);
  // This should be unreachable.
  __ int3();
}

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ Push(rdi);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

namespace {

// Called with the native C calling convention. The corresponding function
// signature is either:
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, Address new_target, Address target,
//       Address receiver, intptr_t argc, Address** argv)>;
// or
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, MicrotaskQueue* microtask_queue)>;
void Generate_JSEntryVariant(MacroAssembler* masm, StackFrame::Type type,
                             Builtin entry_trampoline) {
  Label invoke, handler_entry, exit;
  Label not_outermost_js, not_outermost_js_2;

  {
    NoRootArrayScope uninitialized_root_register(masm);

    // Set up the frame.
    //
    // Note: at this point we are entering V8-generated code from C++ and thus
    // rbp can be an arbitrary value (-fomit-frame-pointer). Since V8 still
    // needs to know where the next interesting frame is for the purpose of
    // stack walks, we instead push the stored EXIT frame fp
    // (IsolateAddressId::kCEntryFPAddress) below to a dedicated slot.
    __ pushq(rbp);
    __ movq(rbp, rsp);

    // Push the stack frame type.
    __ Push(Immediate(StackFrame::TypeToMarker(type)));
    // Reserve a slot for the context. It is filled after the root register has
    // been set up.
    __ AllocateStackSpace(kSystemPointerSize);
    // Save callee-saved registers (X64/X32/Win64 calling conventions).
    __ pushq(r12);
    __ pushq(r13);
    __ pushq(r14);
    __ pushq(r15);
#ifdef V8_TARGET_OS_WIN
    __ pushq(rdi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
    __ pushq(rsi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
#endif
    __ pushq(rbx);

#ifdef V8_TARGET_OS_WIN
    // On Win64 XMM6-XMM15 are callee-save.
    __ AllocateStackSpace(EntryFrameConstants::kXMMRegistersBlockSize);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 0), xmm6);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 1), xmm7);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 2), xmm8);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 3), xmm9);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 4), xmm10);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 5), xmm11);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 6), xmm12);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 7), xmm13);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 8), xmm14);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 9), xmm15);
    static_assert(EntryFrameConstants::kCalleeSaveXMMRegisters == 10);
    static_assert(EntryFrameConstants::kXMMRegistersBlockSize ==
                  EntryFrameConstants::kXMMRegisterSize *
                      EntryFrameConstants::kCalleeSaveXMMRegisters);
#endif

    // Initialize the root register.
    // C calling convention. The first argument is passed in arg_reg_1.
    __ movq(kRootRegister, arg_reg_1);

#ifdef V8_COMPRESS_POINTERS
    // Initialize the pointer cage base register.
    __ LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
#endif
  }

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp = ExternalReference::Create(
      IsolateAddressId::kCEntryFPAddress, masm->isolate());
  {
    // Keep this static_assert to preserve a link between the offset constant
    // and the code location it refers to.
#ifdef V8_TARGET_OS_WIN
    static_assert(EntryFrameConstants::kNextExitFrameFPOffset ==
                  -3 * kSystemPointerSize + -7 * kSystemPointerSize -
                      EntryFrameConstants::kXMMRegistersBlockSize);
#else
    static_assert(EntryFrameConstants::kNextExitFrameFPOffset ==
                  -3 * kSystemPointerSize + -5 * kSystemPointerSize);
#endif  // V8_TARGET_OS_WIN
    Operand c_entry_fp_operand = masm->ExternalReferenceAsOperand(c_entry_fp);
    __ Push(c_entry_fp_operand);

    // Clear c_entry_fp, now we've pushed its previous value to the stack.
    // If the c_entry_fp is not already zero and we don't clear it, the
    // StackFrameIteratorForProfiler will assume we are executing C++ and miss
    // the JS frames on top.
    __ Move(c_entry_fp_operand, 0);
  }

  // Store the context address in the previously-reserved slot.
  ExternalReference context_address = ExternalReference::Create(
      IsolateAddressId::kContextAddress, masm->isolate());
  __ Load(kScratchRegister, context_address);
  static constexpr int kOffsetToContextSlot = -2 * kSystemPointerSize;
  __ movq(Operand(rbp, kOffsetToContextSlot), kScratchRegister);

  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp = ExternalReference::Create(
      IsolateAddressId::kJSEntrySPAddress, masm->isolate());
  __ Load(rax, js_entry_sp);
  __ testq(rax, rax);
  __ j(not_zero, &not_outermost_js);
  __ Push(Immediate(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ movq(rax, rbp);
  __ Store(js_entry_sp, rax);
  Label cont;
  __ jmp(&cont);
  __ bind(&not_outermost_js);
  __ Push(Immediate(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);

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
  __ Store(pending_exception, rax);
  __ LoadRoot(rax, RootIndex::kException);
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler();

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return.
  Handle<Code> trampoline_code =
      masm->isolate()->builtins()->code_handle(entry_trampoline);
  __ Call(trampoline_code, RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);
  // Check if the current stack frame is marked as the outermost JS frame.
  __ Pop(rbx);
  __ cmpq(rbx, Immediate(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ j(not_equal, &not_outermost_js_2);
  __ Move(kScratchRegister, js_entry_sp);
  __ movq(Operand(kScratchRegister, 0), Immediate(0));
  __ bind(&not_outermost_js_2);

  // Restore the top frame descriptor from the stack.
  {
    Operand c_entry_fp_operand = masm->ExternalReferenceAsOperand(c_entry_fp);
    __ Pop(c_entry_fp_operand);
  }

  // Restore callee-saved registers (X64 conventions).
#ifdef V8_TARGET_OS_WIN
  // On Win64 XMM6-XMM15 are callee-save
  __ movdqu(xmm6, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 0));
  __ movdqu(xmm7, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 1));
  __ movdqu(xmm8, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 2));
  __ movdqu(xmm9, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 3));
  __ movdqu(xmm10, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 4));
  __ movdqu(xmm11, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 5));
  __ movdqu(xmm12, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 6));
  __ movdqu(xmm13, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 7));
  __ movdqu(xmm14, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 8));
  __ movdqu(xmm15, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 9));
  __ addq(rsp, Immediate(EntryFrameConstants::kXMMRegistersBlockSize));
#endif

  __ popq(rbx);
#ifdef V8_TARGET_OS_WIN
  // Callee save on in Win64 ABI, arguments/volatile in AMD64 ABI.
  __ popq(rsi);
  __ popq(rdi);
#endif
  __ popq(r15);
  __ popq(r14);
  __ popq(r13);
  __ popq(r12);
  __ addq(rsp, Immediate(2 * kSystemPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ popq(rbp);
  __ ret(0);
}

}  // namespace

void Builtins::Generate_JSEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY, Builtin::kJSEntryTrampoline);
}

void Builtins::Generate_JSConstructEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::CONSTRUCT_ENTRY,
                          Builtin::kJSConstructEntryTrampoline);
}

void Builtins::Generate_JSRunMicrotasksEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY,
                          Builtin::kRunMicrotasksTrampoline);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Expects six C++ function parameters.
  // - Address root_register_value
  // - Address new_target (tagged Object pointer)
  // - Address function (tagged JSFunction pointer)
  // - Address receiver (tagged Object pointer)
  // - intptr_t argc
  // - Address** argv (pointer to array of tagged Object pointers)
  // (see Handle::Invoke in execution.cc).

  // Open a C++ scope for the FrameScope.
  {
    // Platform specific argument handling. After this, the stack contains
    // an internal frame and the pushed function and receiver, and
    // register rax and rbx holds the argument count and argument array,
    // while rdi holds the function pointer, rsi the context, and rdx the
    // new.target.

    // MSVC parameters in:
    // rcx        : root_register_value
    // rdx        : new_target
    // r8         : function
    // r9         : receiver
    // [rsp+0x20] : argc
    // [rsp+0x28] : argv
    //
    // GCC parameters in:
    // rdi : root_register_value
    // rsi : new_target
    // rdx : function
    // rcx : receiver
    // r8  : argc
    // r9  : argv

    __ movq(rdi, arg_reg_3);
    __ Move(rdx, arg_reg_2);
    // rdi : function
    // rdx : new_target

    // Clear the context before we push it when entering the internal frame.
    __ Move(rsi, 0);

    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address = ExternalReference::Create(
        IsolateAddressId::kContextAddress, masm->isolate());
    __ movq(rsi, masm->ExternalReferenceAsOperand(context_address));

    // Push the function onto the stack.
    __ Push(rdi);

#ifdef V8_TARGET_OS_WIN
    // Load the previous frame pointer to access C arguments on stack
    __ movq(kScratchRegister, Operand(rbp, 0));
    // Load the number of arguments and setup pointer to the arguments.
    __ movq(rax, Operand(kScratchRegister, EntryFrameConstants::kArgcOffset));
    __ movq(rbx, Operand(kScratchRegister, EntryFrameConstants::kArgvOffset));
#else   // V8_TARGET_OS_WIN
    // Load the number of arguments and setup pointer to the arguments.
    __ movq(rax, r8);
    __ movq(rbx, r9);
    __ movq(r9, arg_reg_4);  // Temporarily saving the receiver.
#endif  // V8_TARGET_OS_WIN

    // Current stack contents:
    // [rsp + kSystemPointerSize]     : Internal frame
    // [rsp]                          : function
    // Current register contents:
    // rax : argc
    // rbx : argv
    // rsi : context
    // rdi : function
    // rdx : new.target
    // r9  : receiver

    // Check if we have enough stack space to push all arguments.
    // Argument count in rax.
    Label enough_stack_space, stack_overflow;
    __ StackOverflowCheck(rax, &stack_overflow, Label::kNear);
    __ jmp(&enough_stack_space, Label::kNear);

    __ bind(&stack_overflow);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // This should be unreachable.
    __ int3();

    __ bind(&enough_stack_space);

    // Copy arguments to the stack.
    // Register rbx points to array of pointers to handle locations.
    // Push the values of these handles.
    // rbx: Pointer to start of arguments.
    // rax: Number of arguments.
    Generate_PushArguments(masm, rbx, rax, rcx, ArgumentsElementType::kHandle);

    // Push the receiver.
    __ Push(r9);

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

void Builtins::Generate_RunMicrotasksTrampoline(MacroAssembler* masm) {
  // arg_reg_2: microtask_queue
  __ movq(RunMicrotasksDescriptor::MicrotaskQueueRegister(), arg_reg_2);
  __ Jump(BUILTIN_CODE(masm->isolate(), RunMicrotasks), RelocInfo::CODE_TARGET);
}

static void AssertCodeIsBaselineAllowClobber(MacroAssembler* masm,
                                             Register code, Register scratch) {
  // Verify that the code kind is baseline code via the CodeKind.
  __ movl(scratch, FieldOperand(code, Code::kFlagsOffset));
  __ DecodeField<Code::KindField>(scratch);
  __ cmpl(scratch, Immediate(static_cast<int>(CodeKind::BASELINE)));
  __ Assert(equal, AbortReason::kExpectedBaselineData);
}

static void AssertCodeIsBaseline(MacroAssembler* masm, Register code,
                                 Register scratch) {
  DCHECK(!AreAliased(code, scratch));
  return AssertCodeIsBaselineAllowClobber(masm, code, scratch);
}

static void GetSharedFunctionInfoBytecodeOrBaseline(MacroAssembler* masm,
                                                    Register sfi_data,
                                                    Register scratch1,
                                                    Label* is_baseline) {
  ASM_CODE_COMMENT(masm);
  Label done;
  __ LoadMap(scratch1, sfi_data);

#ifndef V8_JITLESS
  __ CmpInstanceType(scratch1, CODE_TYPE);
  if (v8_flags.debug_code) {
    Label not_baseline;
    __ j(not_equal, &not_baseline);
    AssertCodeIsBaseline(masm, sfi_data, scratch1);
    __ j(equal, is_baseline);
    __ bind(&not_baseline);
  } else {
    __ j(equal, is_baseline);
  }
#endif  // !V8_JITLESS

  __ CmpInstanceType(scratch1, INTERPRETER_DATA_TYPE);
  __ j(not_equal, &done, Label::kNear);

  __ LoadTaggedField(
      sfi_data, FieldOperand(sfi_data, InterpreterData::kBytecodeArrayOffset));

  __ bind(&done);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : the value to pass to the generator
  //  -- rdx    : the JSGeneratorObject to resume
  //  -- rsp[0] : return address
  // -----------------------------------

  // Store input value into generator object.
  __ StoreTaggedField(
      FieldOperand(rdx, JSGeneratorObject::kInputOrDebugPosOffset), rax);
  Register object = WriteBarrierDescriptor::ObjectRegister();
  __ Move(object, rdx);
  __ RecordWriteField(object, JSGeneratorObject::kInputOrDebugPosOffset, rax,
                      WriteBarrierDescriptor::SlotAddressRegister(),
                      SaveFPRegsMode::kIgnore);
  // Check that rdx is still valid, RecordWrite might have clobbered it.
  __ AssertGeneratorObject(rdx);

  Register decompr_scratch1 = COMPRESS_POINTERS_BOOL ? r8 : no_reg;

  // Load suspended function and context.
  __ LoadTaggedField(rdi,
                     FieldOperand(rdx, JSGeneratorObject::kFunctionOffset));
  __ LoadTaggedField(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  Operand debug_hook_operand = masm->ExternalReferenceAsOperand(debug_hook);
  __ cmpb(debug_hook_operand, Immediate(0));
  __ j(not_equal, &prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  Operand debug_suspended_generator_operand =
      masm->ExternalReferenceAsOperand(debug_suspended_generator);
  __ cmpq(rdx, debug_suspended_generator_operand);
  __ j(equal, &prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ cmpq(rsp, __ StackLimitAsOperand(StackLimitKind::kRealStackLimit));
  __ j(below, &stack_overflow);

  // Pop return address.
  __ PopReturnAddressTo(rax);

  // ----------- S t a t e -------------
  //  -- rax    : return address
  //  -- rdx    : the JSGeneratorObject to resume
  //  -- rdi    : generator function
  //  -- rsi    : generator context
  // -----------------------------------

  // Copy the function arguments from the generator object's register file.
  __ LoadTaggedField(rcx,
                     FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movzxwq(
      rcx, FieldOperand(rcx, SharedFunctionInfo::kFormalParameterCountOffset));
  __ decq(rcx);  // Exclude receiver.
  __ LoadTaggedField(
      rbx, FieldOperand(rdx, JSGeneratorObject::kParametersAndRegistersOffset));

  {
    Label done_loop, loop;
    __ bind(&loop);
    __ decq(rcx);
    __ j(less, &done_loop, Label::kNear);
    __ PushTaggedField(
        FieldOperand(rbx, rcx, times_tagged_size, FixedArray::kHeaderSize),
        decompr_scratch1);
    __ jmp(&loop);
    __ bind(&done_loop);

    // Push the receiver.
    __ PushTaggedField(FieldOperand(rdx, JSGeneratorObject::kReceiverOffset),
                       decompr_scratch1);
  }

  // Underlying function needs to have bytecode available.
  if (v8_flags.debug_code) {
    Label is_baseline, ok;
    __ LoadTaggedField(
        rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ LoadTaggedField(
        rcx, FieldOperand(rcx, SharedFunctionInfo::kFunctionDataOffset));
    GetSharedFunctionInfoBytecodeOrBaseline(masm, rcx, kScratchRegister,
                                            &is_baseline);
    __ IsObjectType(rcx, BYTECODE_ARRAY_TYPE, rcx);
    __ Assert(equal, AbortReason::kMissingBytecodeArray);
    __ jmp(&ok);

    __ bind(&is_baseline);
    __ IsObjectType(rcx, CODE_TYPE, rcx);
    __ Assert(equal, AbortReason::kMissingBytecodeArray);

    __ bind(&ok);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ PushReturnAddressFrom(rax);
    __ LoadTaggedField(
        rax, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ movzxwq(rax, FieldOperand(
                        rax, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
    __ LoadTaggedField(rcx, FieldOperand(rdi, JSFunction::kCodeOffset));
    __ JumpCodeObject(rcx);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdx);
    __ Push(rdi);
    // Push hole as receiver since we do not use it for stepping.
    __ PushRoot(RootIndex::kTheHoleValue);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(rdx);
    __ LoadTaggedField(rdi,
                       FieldOperand(rdx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdx);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(rdx);
    __ LoadTaggedField(rdi,
                       FieldOperand(rdx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();  // This should be unreachable.
  }
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch1,
                                  Register scratch2) {
  ASM_CODE_COMMENT(masm);
  Register params_size = scratch1;
  // Get the size of the formal parameters (in bytes).
  __ movq(params_size,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ movl(params_size,
          FieldOperand(params_size, BytecodeArray::kParameterSizeOffset));

  Register actual_params_size = scratch2;
  // Compute the size of the actual parameters (in bytes).
  __ movq(actual_params_size,
          Operand(rbp, StandardFrameConstants::kArgCOffset));
  __ leaq(actual_params_size,
          Operand(actual_params_size, times_system_pointer_size, 0));

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label corrected_args_count;
  __ cmpq(params_size, actual_params_size);
  __ j(greater_equal, &corrected_args_count, Label::kNear);
  __ movq(params_size, actual_params_size);
  __ bind(&corrected_args_count);

  // Leave the frame (also dropping the register file).
  __ leave();

  // Drop receiver + arguments.
  __ DropArguments(params_size, scratch2, MacroAssembler::kCountIsBytes,
                   MacroAssembler::kCountIncludesReceiver);
}

// Tail-call |function_id| if |actual_state| == |expected_state|
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
  ASM_CODE_COMMENT(masm);
  Register bytecode_size_table = scratch1;

  // The bytecode offset value will be increased by one in wide and extra wide
  // cases. In the case of having a wide or extra wide JumpLoop bytecode, we
  // will restore the original bytecode. In order to simplify the code, we have
  // a backup of it.
  Register original_bytecode_offset = scratch2;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode,
                     bytecode_size_table, original_bytecode_offset));

  __ movq(original_bytecode_offset, bytecode_offset);

  __ Move(bytecode_size_table,
          ExternalReference::bytecode_size_table_address());

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  static_assert(0 == static_cast<int>(interpreter::Bytecode::kWide));
  static_assert(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  static_assert(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  static_assert(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ cmpb(bytecode, Immediate(0x3));
  __ j(above, &process_bytecode, Label::kNear);
  // The code to load the next bytecode is common to both wide and extra wide.
  // We can hoist them up here. incl has to happen before testb since it
  // modifies the ZF flag.
  __ incl(bytecode_offset);
  __ testb(bytecode, Immediate(0x1));
  __ movzxbq(bytecode, Operand(bytecode_array, bytecode_offset, times_1, 0));
  __ j(not_equal, &extra_wide, Label::kNear);

  // Update table to the wide scaled table.
  __ addq(bytecode_size_table,
          Immediate(kByteSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&process_bytecode, Label::kNear);

  __ bind(&extra_wide);
  // Update table to the extra wide scaled table.
  __ addq(bytecode_size_table,
          Immediate(2 * kByteSize * interpreter::Bytecodes::kBytecodeCount));

  __ bind(&process_bytecode);

// Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)                                             \
  __ cmpb(bytecode,                                                     \
          Immediate(static_cast<int>(interpreter::Bytecode::k##NAME))); \
  __ j(equal, if_return, Label::kFar);
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // If this is a JumpLoop, re-execute it to perform the jump to the beginning
  // of the loop.
  Label end, not_jump_loop;
  __ cmpb(bytecode,
          Immediate(static_cast<int>(interpreter::Bytecode::kJumpLoop)));
  __ j(not_equal, &not_jump_loop, Label::kNear);
  // We need to restore the original bytecode_offset since we might have
  // increased it to skip the wide / extra-wide prefix bytecode.
  __ movq(bytecode_offset, original_bytecode_offset);
  __ jmp(&end, Label::kNear);

  __ bind(&not_jump_loop);
  // Otherwise, load the size of the current bytecode and advance the offset.
  __ movzxbl(kScratchRegister,
             Operand(bytecode_size_table, bytecode, times_1, 0));
  __ addl(bytecode_offset, kScratchRegister);

  __ bind(&end);
}

namespace {

void ResetBytecodeAge(MacroAssembler* masm, Register bytecode_array) {
  __ movw(FieldOperand(bytecode_array, BytecodeArray::kBytecodeAgeOffset),
          Immediate(0));
}

void ResetFeedbackVectorOsrUrgency(MacroAssembler* masm,
                                   Register feedback_vector, Register scratch) {
  __ movb(scratch,
          FieldOperand(feedback_vector, FeedbackVector::kOsrStateOffset));
  __ andb(scratch,
          Immediate(FeedbackVector::MaybeHasOptimizedOsrCodeBit::kMask));
  __ movb(FieldOperand(feedback_vector, FeedbackVector::kOsrStateOffset),
          scratch);
}

}  // namespace

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.
//
// The live registers are:
//   o rax: actual argument count
//   o rdi: the JS function object being called
//   o rdx: the incoming new target or generator object
//   o rsi: our context
//   o rbp: the caller's frame pointer
//   o rsp: stack pointer (pointing to return address)
//
// The function builds an interpreter frame. See InterpreterFrameConstants in
// frame-constants.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(
    MacroAssembler* masm, InterpreterEntryTrampolineMode mode) {
  Register closure = rdi;

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  const TaggedRegister shared_function_info(kScratchRegister);
  __ LoadTaggedField(
      shared_function_info,
      FieldOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ LoadTaggedField(kInterpreterBytecodeArrayRegister,
                     FieldOperand(shared_function_info,
                                  SharedFunctionInfo::kFunctionDataOffset));

  Label is_baseline;
  GetSharedFunctionInfoBytecodeOrBaseline(
      masm, kInterpreterBytecodeArrayRegister, kScratchRegister, &is_baseline);

  // The bytecode array could have been flushed from the shared function info,
  // if so, call into CompileLazy.
  Label compile_lazy;
  __ IsObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                  kScratchRegister);
  __ j(not_equal, &compile_lazy);

#ifndef V8_JITLESS
  // Load the feedback vector from the closure.
  Register feedback_vector = rbx;
  TaggedRegister feedback_cell(feedback_vector);
  __ LoadTaggedField(feedback_cell,
                     FieldOperand(closure, JSFunction::kFeedbackCellOffset));
  __ LoadTaggedField(feedback_vector,
                     FieldOperand(feedback_cell, Cell::kValueOffset));

  Label push_stack_frame;
  // Check if feedback vector is valid. If valid, check for optimized code
  // and update invocation count. Otherwise, setup the stack frame.
  __ IsObjectType(feedback_vector, FEEDBACK_VECTOR_TYPE, rcx);
  __ j(not_equal, &push_stack_frame);

  // Check the tiering state.
  Label flags_need_processing;
  Register flags = rcx;
  __ LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
      flags, feedback_vector, CodeKind::INTERPRETED_FUNCTION,
      &flags_need_processing);

  ResetFeedbackVectorOsrUrgency(masm, feedback_vector, kScratchRegister);

  // Increment invocation count for the function.
  __ incl(
      FieldOperand(feedback_vector, FeedbackVector::kInvocationCountOffset));

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  __ bind(&push_stack_frame);
#else
  // Note: By omitting the above code in jitless mode we also disable:
  // - kFlagsLogNextExecution: only used for logging/profiling; and
  // - kInvocationCountOffset: only used for tiering heuristics and code
  //   coverage.
#endif  // !V8_JITLESS
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ pushq(rbp);  // Caller's frame pointer.
  __ movq(rbp, rsp);
  __ Push(kContextRegister);                 // Callee's context.
  __ Push(kJavaScriptCallTargetRegister);    // Callee's JS function.
  __ Push(kJavaScriptCallArgCountRegister);  // Actual argument count.

  ResetBytecodeAge(masm, kInterpreterBytecodeArrayRegister);

  // Load initial bytecode offset.
  __ Move(kInterpreterBytecodeOffsetRegister,
          BytecodeArray::kHeaderSize - kHeapObjectTag);

  // Push bytecode array and Smi tagged bytecode offset.
  __ Push(kInterpreterBytecodeArrayRegister);
  __ SmiTag(rcx, kInterpreterBytecodeOffsetRegister);
  __ Push(rcx);

  // Allocate the local and temporary register file on the stack.
  Label stack_overflow;
  {
    // Load frame size from the BytecodeArray object.
    __ movl(rcx, FieldOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    __ movq(rax, rsp);
    __ subq(rax, rcx);
    __ cmpq(rax, __ StackLimitAsOperand(StackLimitKind::kRealStackLimit));
    __ j(below, &stack_overflow);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
    __ jmp(&loop_check, Label::kNear);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ Push(kInterpreterAccumulatorRegister);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ subq(rcx, Immediate(kSystemPointerSize));
    __ j(greater_equal, &loop_header, Label::kNear);
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in rdx.
  Label no_incoming_new_target_or_generator_register;
  __ movsxlq(
      rcx,
      FieldOperand(kInterpreterBytecodeArrayRegister,
                   BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ testl(rcx, rcx);
  __ j(zero, &no_incoming_new_target_or_generator_register, Label::kNear);
  __ movq(Operand(rbp, rcx, times_system_pointer_size, 0), rdx);
  __ bind(&no_incoming_new_target_or_generator_register);

  // Perform interrupt stack check.
  // TODO(solanes): Merge with the real stack limit check above.
  Label stack_check_interrupt, after_stack_check_interrupt;
  __ cmpq(rsp, __ StackLimitAsOperand(StackLimitKind::kInterruptStackLimit));
  __ j(below, &stack_check_interrupt);
  __ bind(&after_stack_check_interrupt);

  // The accumulator is already loaded with undefined.

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));
  __ movzxbq(kScratchRegister,
             Operand(kInterpreterBytecodeArrayRegister,
                     kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ movq(kJavaScriptCallCodeStartRegister,
          Operand(kInterpreterDispatchTableRegister, kScratchRegister,
                  times_system_pointer_size, 0));
  __ call(kJavaScriptCallCodeStartRegister);

  __ RecordComment("--- InterpreterEntryReturnPC point ---");
  if (mode == InterpreterEntryTrampolineMode::kDefault) {
    masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(
        masm->pc_offset());
  } else {
    DCHECK_EQ(mode, InterpreterEntryTrampolineMode::kForProfiling);
    // Both versions must be the same up to this point otherwise the builtins
    // will not be interchangable.
    CHECK_EQ(
        masm->isolate()->heap()->interpreter_entry_return_pc_offset().value(),
        masm->pc_offset());
  }

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ movq(kInterpreterBytecodeArrayRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ SmiUntagUnsigned(
      kInterpreterBytecodeOffsetRegister,
      Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  // Either return, or advance to the next bytecode and dispatch.
  Label do_return;
  __ movzxbq(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, rbx, rcx,
                                r8, &do_return);
  __ jmp(&do_dispatch);

  __ bind(&do_return);
  // The return value is in rax.
  LeaveInterpreterFrame(masm, rbx, rcx);
  __ ret(0);

  __ bind(&stack_check_interrupt);
  // Modify the bytecode offset in the stack to be kFunctionEntryBytecodeOffset
  // for the call to the StackGuard.
  __ Move(Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp),
          Smi::FromInt(BytecodeArray::kHeaderSize - kHeapObjectTag +
                       kFunctionEntryBytecodeOffset));
  __ CallRuntime(Runtime::kStackGuard);

  // After the call, restore the bytecode array, bytecode offset and accumulator
  // registers again. Also, restore the bytecode offset in the stack to its
  // previous value.
  __ movq(kInterpreterBytecodeArrayRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Move(kInterpreterBytecodeOffsetRegister,
          BytecodeArray::kHeaderSize - kHeapObjectTag);
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);

  __ SmiTag(rcx, kInterpreterBytecodeArrayRegister);
  __ movq(Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp), rcx);

  __ jmp(&after_stack_check_interrupt);

  __ bind(&compile_lazy);
  __ GenerateTailCallToReturnedCode(Runtime::kCompileLazy);
  __ int3();  // Should not return.

#ifndef V8_JITLESS
  __ bind(&flags_need_processing);
  __ OptimizeCodeOrTailCallOptimizedCodeSlot(flags, feedback_vector, closure);

  __ bind(&is_baseline);
  {
    // Load the feedback vector from the closure.
    TaggedRegister feedback_cell(feedback_vector);
    __ LoadTaggedField(feedback_cell,
                       FieldOperand(closure, JSFunction::kFeedbackCellOffset));
    __ LoadTaggedField(feedback_vector,
                       FieldOperand(feedback_cell, Cell::kValueOffset));

    Label install_baseline_code;
    // Check if feedback vector is valid. If not, call prepare for baseline to
    // allocate it.
    __ IsObjectType(feedback_vector, FEEDBACK_VECTOR_TYPE, rcx);
    __ j(not_equal, &install_baseline_code);

    // Check the tiering state.
    __ LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
        flags, feedback_vector, CodeKind::BASELINE, &flags_need_processing);

    // Load the baseline code into the closure.
    __ Move(rcx, kInterpreterBytecodeArrayRegister);
    static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
    __ ReplaceClosureCodeWithOptimizedCode(
        rcx, closure, kInterpreterBytecodeArrayRegister,
        WriteBarrierDescriptor::SlotAddressRegister());
    __ JumpCodeObject(rcx);

    __ bind(&install_baseline_code);
    __ GenerateTailCallToReturnedCode(Runtime::kInstallBaselineCode);
  }
#endif  // !V8_JITLESS

  __ bind(&stack_overflow);
  __ CallRuntime(Runtime::kThrowStackOverflow);
  __ int3();  // Should not return.
}

static void GenerateInterpreterPushArgs(MacroAssembler* masm, Register num_args,
                                        Register start_address,
                                        Register scratch) {
  ASM_CODE_COMMENT(masm);
  // Find the argument with lowest address.
  __ movq(scratch, num_args);
  __ negq(scratch);
  __ leaq(start_address,
          Operand(start_address, scratch, times_system_pointer_size,
                  kSystemPointerSize));
  // Push the arguments.
  __ PushArray(start_address, num_args, scratch,
               MacroAssembler::PushArrayOrder::kReverse);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments
  //  -- rbx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  //  -- rdi : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // The spread argument should not be pushed.
    __ decl(rax);
  }

  __ movl(rcx, rax);
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ decl(rcx);  // Exclude receiver.
  }

  // Add a stack check before pushing arguments.
  __ StackOverflowCheck(rcx, &stack_overflow);

  // Pop return address to allow tail-call after pushing arguments.
  __ PopReturnAddressTo(kScratchRegister);

  // rbx and rdx will be modified.
  GenerateInterpreterPushArgs(masm, rcx, rbx, rdx);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(RootIndex::kUndefinedValue);
  }

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Pass the spread in the register rbx.
    // rbx already points to the penultime argument, the spread
    // is below that.
    __ movq(rbx, Operand(rbx, -kSystemPointerSize));
  }

  // Call the target.
  __ PushReturnAddressFrom(kScratchRegister);  // Re-push return address.

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
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
  //  -- rax : the number of arguments
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
  __ StackOverflowCheck(rax, &stack_overflow);

  // Pop return address to allow tail-call after pushing arguments.
  __ PopReturnAddressTo(kScratchRegister);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // The spread argument should not be pushed.
    __ decl(rax);
  }

  // rcx and r8 will be modified.
  Register argc_without_receiver = r11;
  __ leaq(argc_without_receiver, Operand(rax, -kJSArgcReceiverSlots));
  GenerateInterpreterPushArgs(masm, argc_without_receiver, rcx, r8);

  // Push slot for the receiver to be constructed.
  __ Push(Immediate(0));

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Pass the spread in the register rbx.
    __ movq(rbx, Operand(rcx, -kSystemPointerSize));
    // Push return address in preparation for the tail-call.
    __ PushReturnAddressFrom(kScratchRegister);
  } else {
    __ PushReturnAddressFrom(kScratchRegister);
    __ AssertUndefinedOrAllocationSite(rbx);
  }

  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    // Tail call to the array construct stub (still in the caller
    // context at this point).
    __ AssertFunction(rdi);
    // Jump to the constructor function (rax, rbx, rdx passed on).
    __ Jump(BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl),
            RelocInfo::CODE_TARGET);
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
  Label builtin_trampoline, trampoline_loaded;
  Smi interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::zero());

  // If the SFI function_data is an InterpreterData, the function will have a
  // custom copy of the interpreter entry trampoline for profiling. If so,
  // get the custom trampoline, otherwise grab the entry address of the global
  // trampoline.
  __ movq(rbx, Operand(rbp, StandardFrameConstants::kFunctionOffset));
  const TaggedRegister shared_function_info(rbx);
  __ LoadTaggedField(shared_function_info,
                     FieldOperand(rbx, JSFunction::kSharedFunctionInfoOffset));
  __ LoadTaggedField(rbx,
                     FieldOperand(shared_function_info,
                                  SharedFunctionInfo::kFunctionDataOffset));
  __ IsObjectType(rbx, INTERPRETER_DATA_TYPE, kScratchRegister);
  __ j(not_equal, &builtin_trampoline, Label::kNear);

  __ LoadTaggedField(
      rbx, FieldOperand(rbx, InterpreterData::kInterpreterTrampolineOffset));
  __ LoadCodeEntry(rbx, rbx);
  __ jmp(&trampoline_loaded, Label::kNear);

  __ bind(&builtin_trampoline);
  // TODO(jgruber): Replace this by a lookup in the builtin entry table.
  __ movq(rbx,
          __ ExternalReferenceAsOperand(
              ExternalReference::
                  address_of_interpreter_entry_trampoline_instruction_start(
                      masm->isolate()),
              kScratchRegister));

  __ bind(&trampoline_loaded);
  __ addq(rbx, Immediate(interpreter_entry_return_pc_offset.value()));
  __ Push(rbx);

  // Initialize dispatch table register.
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Get the bytecode array pointer from the frame.
  __ movq(kInterpreterBytecodeArrayRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (v8_flags.debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ IsObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                    rbx);
    __ Assert(
        equal,
        AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ SmiUntagUnsigned(
      kInterpreterBytecodeOffsetRegister,
      Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  if (v8_flags.debug_code) {
    Label okay;
    __ cmpq(kInterpreterBytecodeOffsetRegister,
            Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag));
    __ j(greater_equal, &okay, Label::kNear);
    __ int3();
    __ bind(&okay);
  }

  // Dispatch to the target bytecode.
  __ movzxbq(kScratchRegister,
             Operand(kInterpreterBytecodeArrayRegister,
                     kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ movq(kJavaScriptCallCodeStartRegister,
          Operand(kInterpreterDispatchTableRegister, kScratchRegister,
                  times_system_pointer_size, 0));
  __ jmp(kJavaScriptCallCodeStartRegister);
}

void Builtins::Generate_InterpreterEnterAtNextBytecode(MacroAssembler* masm) {
  // Get bytecode array and bytecode offset from the stack frame.
  __ movq(kInterpreterBytecodeArrayRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ SmiUntagUnsigned(
      kInterpreterBytecodeOffsetRegister,
      Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Label enter_bytecode, function_entry_bytecode;
  __ cmpq(kInterpreterBytecodeOffsetRegister,
          Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag +
                    kFunctionEntryBytecodeOffset));
  __ j(equal, &function_entry_bytecode);

  // Load the current bytecode.
  __ movzxbq(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));

  // Advance to the next bytecode.
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, rbx, rcx,
                                r8, &if_return);

  __ bind(&enter_bytecode);
  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(kInterpreterBytecodeOffsetRegister);
  __ movq(Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp),
          kInterpreterBytecodeOffsetRegister);

  Generate_InterpreterEnterBytecode(masm);

  __ bind(&function_entry_bytecode);
  // If the code deoptimizes during the implicit function entry stack interrupt
  // check, it will have a bailout ID of kFunctionEntryBytecodeOffset, which is
  // not a valid bytecode offset. Detect this case and advance to the first
  // actual bytecode.
  __ Move(kInterpreterBytecodeOffsetRegister,
          BytecodeArray::kHeaderSize - kHeapObjectTag);
  __ jmp(&enter_bytecode);

  // We should never take the if_return path.
  __ bind(&if_return);
  __ Abort(AbortReason::kInvalidBytecodeAdvance);
}

void Builtins::Generate_InterpreterEnterAtBytecode(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

// static
void Builtins::Generate_BaselineOutOfLinePrologue(MacroAssembler* masm) {
  Register feedback_vector = r8;
  Register flags = rcx;
  Register return_address = r15;

#ifdef DEBUG
  for (auto reg : BaselineOutOfLinePrologueDescriptor::registers()) {
    DCHECK(!AreAliased(feedback_vector, flags, return_address, reg));
  }
#endif

  auto descriptor =
      Builtins::CallInterfaceDescriptorFor(Builtin::kBaselineOutOfLinePrologue);
  Register closure = descriptor.GetRegisterParameter(
      BaselineOutOfLinePrologueDescriptor::kClosure);
  // Load the feedback vector from the closure.
  TaggedRegister feedback_cell(feedback_vector);
  __ LoadTaggedField(feedback_cell,
                     FieldOperand(closure, JSFunction::kFeedbackCellOffset));
  __ LoadTaggedField(feedback_vector,
                     FieldOperand(feedback_cell, Cell::kValueOffset));
  __ AssertFeedbackVector(feedback_vector);

  // Check the tiering state.
  Label flags_need_processing;
  __ LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
      flags, feedback_vector, CodeKind::BASELINE, &flags_need_processing);

  ResetFeedbackVectorOsrUrgency(masm, feedback_vector, kScratchRegister);

  // Increment invocation count for the function.
  __ incl(
      FieldOperand(feedback_vector, FeedbackVector::kInvocationCountOffset));

    // Save the return address, so that we can push it to the end of the newly
    // set-up frame once we're done setting it up.
    __ PopReturnAddressTo(return_address);
    FrameScope frame_scope(masm, StackFrame::MANUAL);
    {
      ASM_CODE_COMMENT_STRING(masm, "Frame Setup");
      __ EnterFrame(StackFrame::BASELINE);

      __ Push(descriptor.GetRegisterParameter(
          BaselineOutOfLinePrologueDescriptor::kCalleeContext));  // Callee's
                                                                  // context.
      Register callee_js_function = descriptor.GetRegisterParameter(
          BaselineOutOfLinePrologueDescriptor::kClosure);
      DCHECK_EQ(callee_js_function, kJavaScriptCallTargetRegister);
      DCHECK_EQ(callee_js_function, kJSFunctionRegister);
      __ Push(callee_js_function);  // Callee's JS function.
      __ Push(descriptor.GetRegisterParameter(
          BaselineOutOfLinePrologueDescriptor::
              kJavaScriptCallArgCount));  // Actual argument
                                          // count.

      // We'll use the bytecode for both code age/OSR resetting, and pushing
      // onto the frame, so load it into a register.
      Register bytecode_array = descriptor.GetRegisterParameter(
          BaselineOutOfLinePrologueDescriptor::kInterpreterBytecodeArray);
      ResetBytecodeAge(masm, bytecode_array);
      __ Push(bytecode_array);

      // Baseline code frames store the feedback vector where interpreter would
      // store the bytecode offset.
      __ Push(feedback_vector);
    }

  Register new_target = descriptor.GetRegisterParameter(
      BaselineOutOfLinePrologueDescriptor::kJavaScriptCallNewTarget);

  Label call_stack_guard;
  Register frame_size = descriptor.GetRegisterParameter(
      BaselineOutOfLinePrologueDescriptor::kStackFrameSize);
  {
    ASM_CODE_COMMENT_STRING(masm, " Stack/interrupt check");
    // Stack check. This folds the checks for both the interrupt stack limit
    // check and the real stack limit into one by just checking for the
    // interrupt limit. The interrupt limit is either equal to the real stack
    // limit or tighter. By ensuring we have space until that limit after
    // building the frame we can quickly precheck both at once.
    //
    // TODO(v8:11429): Backport this folded check to the
    // InterpreterEntryTrampoline.
    __ Move(kScratchRegister, rsp);
    DCHECK_NE(frame_size, new_target);
    __ subq(kScratchRegister, frame_size);
    __ cmpq(kScratchRegister,
            __ StackLimitAsOperand(StackLimitKind::kInterruptStackLimit));
    __ j(below, &call_stack_guard);
  }

  // Push the return address back onto the stack for return.
  __ PushReturnAddressFrom(return_address);
  // Return to caller pushed pc, without any frame teardown.
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
  __ Ret();

  __ bind(&flags_need_processing);
  {
    ASM_CODE_COMMENT_STRING(masm, "Optimized marker check");
    // Drop the return address, rebalancing the return stack buffer by using
    // JumpMode::kPushAndReturn. We can't leave the slot and overwrite it on
    // return since we may do a runtime call along the way that requires the
    // stack to only contain valid frames.
    __ Drop(1);
    __ OptimizeCodeOrTailCallOptimizedCodeSlot(flags, feedback_vector, closure,
                                               JumpMode::kPushAndReturn);
    __ Trap();
  }

  __ bind(&call_stack_guard);
  {
    ASM_CODE_COMMENT_STRING(masm, "Stack/interrupt call");
    {
      // Push the baseline code return address now, as if it had been pushed by
      // the call to this builtin.
      __ PushReturnAddressFrom(return_address);
      FrameScope inner_frame_scope(masm, StackFrame::INTERNAL);
      // Save incoming new target or generator
      __ Push(new_target);
      __ SmiTag(frame_size);
      __ Push(frame_size);
      __ CallRuntime(Runtime::kStackGuardWithGap, 1);
      __ Pop(new_target);
    }

    // Return to caller pushed pc, without any frame teardown.
    __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
    __ Ret();
  }
}

// static
void Builtins::Generate_BaselineOutOfLinePrologueDeopt(MacroAssembler* masm) {
  // We're here because we got deopted during BaselineOutOfLinePrologue's stack
  // check. Undo all its frame creation and call into the interpreter instead.

  // Drop bytecode offset (was the feedback vector but got replaced during
  // deopt).
  __ Pop(kScratchRegister);
  // Drop bytecode array
  __ Pop(kScratchRegister);

  // argc.
  __ Pop(kJavaScriptCallArgCountRegister);
  // Closure.
  __ Pop(kJavaScriptCallTargetRegister);
  // Context.
  __ Pop(kContextRegister);

  // Drop frame pointer
  __ LeaveFrame(StackFrame::BASELINE);

  // Enter the interpreter.
  __ TailCallBuiltin(Builtin::kInterpreterEntryTrampoline);
}

namespace {
void Generate_ContinueToBuiltinHelper(MacroAssembler* masm,
                                      bool java_script_builtin,
                                      bool with_result) {
  ASM_CODE_COMMENT(masm);
  const RegisterConfiguration* config(RegisterConfiguration::Default());
  int allocatable_register_count = config->num_allocatable_general_registers();
  if (with_result) {
    if (java_script_builtin) {
      // kScratchRegister is not included in the allocateable registers.
      __ movq(kScratchRegister, rax);
    } else {
      // Overwrite the hole inserted by the deoptimizer with the return value
      // from the LAZY deopt point.
      __ movq(
          Operand(rsp, config->num_allocatable_general_registers() *
                               kSystemPointerSize +
                           BuiltinContinuationFrameConstants::kFixedFrameSize),
          rax);
    }
  }
  for (int i = allocatable_register_count - 1; i >= 0; --i) {
    int code = config->GetAllocatableGeneralCode(i);
    __ popq(Register::from_code(code));
    if (java_script_builtin && code == kJavaScriptCallArgCountRegister.code()) {
      __ SmiUntagUnsigned(Register::from_code(code));
    }
  }
  if (with_result && java_script_builtin) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point. rax contains the arguments count, the return value
    // from LAZY is always the last argument.
    __ movq(Operand(rsp, rax, times_system_pointer_size,
                    BuiltinContinuationFrameConstants::kFixedFrameSize -
                        kJSArgcReceiverSlots * kSystemPointerSize),
            kScratchRegister);
  }
  __ movq(
      rbp,
      Operand(rsp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  const int offsetToPC =
      BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp -
      kSystemPointerSize;
  __ popq(Operand(rsp, offsetToPC));
  __ Drop(offsetToPC / kSystemPointerSize);

  // Replace the builtin index Smi on the stack with the instruction start
  // address of the builtin from the builtins table, and then Ret to this
  // address
  __ movq(kScratchRegister, Operand(rsp, 0));
  __ movq(kScratchRegister,
          __ EntryFromBuiltinIndexAsOperand(kScratchRegister));
  __ movq(Operand(rsp, 0), kScratchRegister);

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
  __ movq(rax, Operand(rsp, kPCOnStackSize));
  __ ret(1 * kSystemPointerSize);  // Remove rax.
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[1]  : receiver
  //  -- rsp[2]  : thisArg
  //  -- rsp[3]  : argArray
  // -----------------------------------

  // 1. Load receiver into rdi, argArray into rbx (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label no_arg_array, no_this_arg;
    StackArgumentsAccessor args(rax);
    __ LoadRoot(rdx, RootIndex::kUndefinedValue);
    __ movq(rbx, rdx);
    __ movq(rdi, args[0]);
    __ cmpq(rax, Immediate(JSParameterCount(0)));
    __ j(equal, &no_this_arg, Label::kNear);
    {
      __ movq(rdx, args[1]);
      __ cmpq(rax, Immediate(JSParameterCount(1)));
      __ j(equal, &no_arg_array, Label::kNear);
      __ movq(rbx, args[2]);
      __ bind(&no_arg_array);
    }
    __ bind(&no_this_arg);
    __ DropArgumentsAndPushNewReceiver(rax, rdx, rcx,
                                       MacroAssembler::kCountIsInteger,
                                       MacroAssembler::kCountIncludesReceiver);
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
  __ JumpIfRoot(rbx, RootIndex::kNullValue, &no_arguments, Label::kNear);
  __ JumpIfRoot(rbx, RootIndex::kUndefinedValue, &no_arguments, Label::kNear);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver. Since we did not create a frame for
  // Function.prototype.apply() yet, we use a normal Call builtin here.
  __ bind(&no_arguments);
  {
    __ Move(rax, JSParameterCount(0));
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // Stack Layout:
  // rsp[0]           : Return address
  // rsp[8]           : Argument 0 (receiver: callable to call)
  // rsp[16]          : Argument 1
  //  ...
  // rsp[8 * n]       : Argument n-1
  // rsp[8 * (n + 1)] : Argument n
  // rax contains the number of arguments, n.

  // 1. Get the callable to call (passed as receiver) from the stack.
  {
    StackArgumentsAccessor args(rax);
    __ movq(rdi, args.GetReceiverOperand());
  }

  // 2. Save the return address and drop the callable.
  __ PopReturnAddressTo(rbx);
  __ Pop(kScratchRegister);

  // 3. Make sure we have at least one argument.
  {
    Label done;
    __ cmpq(rax, Immediate(JSParameterCount(0)));
    __ j(greater, &done, Label::kNear);
    __ PushRoot(RootIndex::kUndefinedValue);
    __ incq(rax);
    __ bind(&done);
  }

  // 4. Push back the return address one slot down on the stack (overwriting the
  // original callable), making the original first argument the new receiver.
  __ PushReturnAddressFrom(rbx);
  __ decq(rax);  // One fewer argument (first argument is new receiver).

  // 5. Call the callable.
  // Since we did not create a frame for Function.prototype.call() yet,
  // we use a normal Call builtin here.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : receiver
  //  -- rsp[16] : target         (if argc >= 1)
  //  -- rsp[24] : thisArgument   (if argc >= 2)
  //  -- rsp[32] : argumentsList  (if argc == 3)
  // -----------------------------------

  // 1. Load target into rdi (if present), argumentsList into rbx (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label done;
    StackArgumentsAccessor args(rax);
    __ LoadRoot(rdi, RootIndex::kUndefinedValue);
    __ movq(rdx, rdi);
    __ movq(rbx, rdi);
    __ cmpq(rax, Immediate(JSParameterCount(1)));
    __ j(below, &done, Label::kNear);
    __ movq(rdi, args[1]);  // target
    __ j(equal, &done, Label::kNear);
    __ movq(rdx, args[2]);  // thisArgument
    __ cmpq(rax, Immediate(JSParameterCount(3)));
    __ j(below, &done, Label::kNear);
    __ movq(rbx, args[3]);  // argumentsList
    __ bind(&done);
    __ DropArgumentsAndPushNewReceiver(rax, rdx, rcx,
                                       MacroAssembler::kCountIsInteger,
                                       MacroAssembler::kCountIncludesReceiver);
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
  //  -- rsp[8]  : receiver
  //  -- rsp[16] : target
  //  -- rsp[24] : argumentsList
  //  -- rsp[32] : new.target (optional)
  // -----------------------------------

  // 1. Load target into rdi (if present), argumentsList into rbx (if present),
  // new.target into rdx (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label done;
    StackArgumentsAccessor args(rax);
    __ LoadRoot(rdi, RootIndex::kUndefinedValue);
    __ movq(rdx, rdi);
    __ movq(rbx, rdi);
    __ cmpq(rax, Immediate(JSParameterCount(1)));
    __ j(below, &done, Label::kNear);
    __ movq(rdi, args[1]);                     // target
    __ movq(rdx, rdi);                         // new.target defaults to target
    __ j(equal, &done, Label::kNear);
    __ movq(rbx, args[2]);  // argumentsList
    __ cmpq(rax, Immediate(JSParameterCount(3)));
    __ j(below, &done, Label::kNear);
    __ movq(rdx, args[3]);  // new.target
    __ bind(&done);
    __ DropArgumentsAndPushNewReceiver(
        rax, masm->RootAsOperand(RootIndex::kUndefinedValue), rcx,
        MacroAssembler::kCountIsInteger,
        MacroAssembler::kCountIncludesReceiver);
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

namespace {

// Allocate new stack space for |count| arguments and shift all existing
// arguments already on the stack. |pointer_to_new_space_out| points to the
// first free slot on the stack to copy additional arguments to and
// |argc_in_out| is updated to include |count|.
void Generate_AllocateSpaceAndShiftExistingArguments(
    MacroAssembler* masm, Register count, Register argc_in_out,
    Register pointer_to_new_space_out, Register scratch1, Register scratch2) {
  DCHECK(!AreAliased(count, argc_in_out, pointer_to_new_space_out, scratch1,
                     scratch2, kScratchRegister));
  // Use pointer_to_new_space_out as scratch until we set it to the correct
  // value at the end.
  Register old_rsp = pointer_to_new_space_out;
  Register new_space = kScratchRegister;
  __ movq(old_rsp, rsp);

  __ leaq(new_space, Operand(count, times_system_pointer_size, 0));
  __ AllocateStackSpace(new_space);

  Register copy_count = argc_in_out;
  Register current = scratch2;
  Register value = kScratchRegister;

  Label loop, entry;
  __ Move(current, 0);
  __ jmp(&entry);
  __ bind(&loop);
  __ movq(value, Operand(old_rsp, current, times_system_pointer_size, 0));
  __ movq(Operand(rsp, current, times_system_pointer_size, 0), value);
  __ incq(current);
  __ bind(&entry);
  __ cmpq(current, copy_count);
  __ j(less_equal, &loop, Label::kNear);

  // Point to the next free slot above the shifted arguments (copy_count + 1
  // slot for the return address).
  __ leaq(
      pointer_to_new_space_out,
      Operand(rsp, copy_count, times_system_pointer_size, kSystemPointerSize));
  // We use addl instead of addq here because we can omit REX.W, saving 1 byte.
  // We are especially constrained here because we are close to reaching the
  // limit for a near jump to the stackoverflow label, so every byte counts.
  __ addl(argc_in_out, count);  // Update total number of arguments.
}

}  // namespace

// static
// TODO(v8:11615): Observe InstructionStream::kMaxArguments in
// CallOrConstructVarargs
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- rdi    : target
  //  -- rax    : number of parameters on the stack
  //  -- rbx    : arguments list (a FixedArray)
  //  -- rcx    : len (number of elements to push from args)
  //  -- rdx    : new.target (for [[Construct]])
  //  -- rsp[0] : return address
  // -----------------------------------

  if (v8_flags.debug_code) {
    // Allow rbx to be a FixedArray, or a FixedDoubleArray if rcx == 0.
    Label ok, fail;
    __ AssertNotSmi(rbx);
    Register map = r9;
    __ LoadMap(map, rbx);
    __ CmpInstanceType(map, FIXED_ARRAY_TYPE);
    __ j(equal, &ok);
    __ CmpInstanceType(map, FIXED_DOUBLE_ARRAY_TYPE);
    __ j(not_equal, &fail);
    __ Cmp(rcx, 0);
    __ j(equal, &ok);
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  Label stack_overflow;
  __ StackOverflowCheck(rcx, &stack_overflow, Label::kNear);

  // Push additional arguments onto the stack.
  // Move the arguments already in the stack,
  // including the receiver and the return address.
  // rcx: Number of arguments to make room for.
  // rax: Number of arguments already on the stack.
  // r8: Points to first free slot on the stack after arguments were shifted.
  Generate_AllocateSpaceAndShiftExistingArguments(masm, rcx, rax, r8, r9, r12);
  // Copy the additional arguments onto the stack.
  {
    Register value = r12;
    Register src = rbx, dest = r8, num = rcx, current = r9;
    __ Move(current, 0);
    Label done, push, loop;
    __ bind(&loop);
    __ cmpl(current, num);
    __ j(equal, &done, Label::kNear);
    // Turn the hole into undefined as we go.
    __ LoadTaggedField(value, FieldOperand(src, current, times_tagged_size,
                                           FixedArray::kHeaderSize));
    __ CompareRoot(value, RootIndex::kTheHoleValue);
    __ j(not_equal, &push, Label::kNear);
    __ LoadRoot(value, RootIndex::kUndefinedValue);
    __ bind(&push);
    __ movq(Operand(dest, current, times_system_pointer_size, 0), value);
    __ incl(current);
    __ jmp(&loop);
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
  //  -- rax : the number of arguments
  //  -- rdx : the new target (for [[Construct]] calls)
  //  -- rdi : the target to call (can be any Object)
  //  -- rcx : start index (to support rest parameters)
  // -----------------------------------

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(rdx, &new_target_not_constructor, Label::kNear);
    __ LoadMap(rbx, rdx);
    __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
             Immediate(Map::Bits1::IsConstructorBit::kMask));
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

  Label stack_done, stack_overflow;
  __ movq(r8, Operand(rbp, StandardFrameConstants::kArgCOffset));
  __ decq(r8);  // Exclude receiver.
  __ subl(r8, rcx);
  __ j(less_equal, &stack_done);
  {
    // ----------- S t a t e -------------
    //  -- rax : the number of arguments already in the stack
    //  -- rbp : point to the caller stack frame
    //  -- rcx : start index (to support rest parameters)
    //  -- rdx : the new target (for [[Construct]] calls)
    //  -- rdi : the target to call (can be any Object)
    //  -- r8  : number of arguments to copy, i.e. arguments count - start index
    // -----------------------------------

    // Check for stack overflow.
    __ StackOverflowCheck(r8, &stack_overflow, Label::kNear);

    // Forward the arguments from the caller frame.
    // Move the arguments already in the stack,
    // including the receiver and the return address.
    // r8: Number of arguments to make room for.
    // rax: Number of arguments already on the stack.
    // r9: Points to first free slot on the stack after arguments were shifted.
    Generate_AllocateSpaceAndShiftExistingArguments(masm, r8, rax, r9, r12,
                                                    r15);

    // Point to the first argument to copy (skipping receiver).
    __ leaq(rcx, Operand(rcx, times_system_pointer_size,
                         CommonFrameConstants::kFixedFrameSizeAboveFp +
                             kSystemPointerSize));
    __ addq(rcx, rbp);

    // Copy the additional caller arguments onto the stack.
    // TODO(victorgomes): Consider using forward order as potentially more cache
    // friendly.
    {
      Register src = rcx, dest = r9, num = r8;
      Label loop;
      __ bind(&loop);
      __ decq(num);
      __ movq(kScratchRegister,
              Operand(src, num, times_system_pointer_size, 0));
      __ movq(Operand(dest, num, times_system_pointer_size, 0),
              kScratchRegister);
      __ j(not_zero, &loop);
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
  //  -- rax : the number of arguments
  //  -- rdi : the function to call (checked to be a JSFunction)
  // -----------------------------------

  StackArgumentsAccessor args(rax);
  __ AssertCallableFunction(rdi);

  __ LoadTaggedField(rdx,
                     FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments
  //  -- rdx : the shared function info.
  //  -- rdi : the function to call (checked to be a JSFunction)
  // -----------------------------------

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ LoadTaggedField(rsi, FieldOperand(rdi, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ testl(FieldOperand(rdx, SharedFunctionInfo::kFlagsOffset),
           Immediate(SharedFunctionInfo::IsNativeBit::kMask |
                     SharedFunctionInfo::IsStrictBit::kMask));
  __ j(not_zero, &done_convert);
  {
    // ----------- S t a t e -------------
    //  -- rax : the number of arguments
    //  -- rdx : the shared function info.
    //  -- rdi : the function to call (checked to be a JSFunction)
    //  -- rsi : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(rcx);
    } else {
      Label convert_to_object, convert_receiver;
      __ movq(rcx, args.GetReceiverOperand());
      __ JumpIfSmi(rcx, &convert_to_object, Label::kNear);
      __ JumpIfJSAnyIsNotPrimitive(rcx, rbx, &done_convert, Label::kNear);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(rcx, RootIndex::kUndefinedValue, &convert_global_proxy,
                      Label::kNear);
        __ JumpIfNotRoot(rcx, RootIndex::kNullValue, &convert_to_object,
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
        __ SmiTag(rax);
        __ Push(rax);
        __ Push(rdi);
        __ movq(rax, rcx);
        __ Push(rsi);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(rsi);
        __ movq(rcx, rax);
        __ Pop(rdi);
        __ Pop(rax);
        __ SmiUntagUnsigned(rax);
      }
      __ LoadTaggedField(
          rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ movq(args.GetReceiverOperand(), rcx);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- rax : the number of arguments
  //  -- rdx : the shared function info.
  //  -- rdi : the function to call (checked to be a JSFunction)
  //  -- rsi : the function context.
  // -----------------------------------

  __ movzxwq(
      rbx, FieldOperand(rdx, SharedFunctionInfo::kFormalParameterCountOffset));
  __ InvokeFunctionCode(rdi, no_reg, rbx, rax, InvokeType::kJump);
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments
  //  -- rdx : new.target (only in case of [[Construct]])
  //  -- rdi : target (checked to be a JSBoundFunction)
  // -----------------------------------

  // Load [[BoundArguments]] into rcx and length of that into rbx.
  Label no_bound_arguments;
  __ LoadTaggedField(rcx,
                     FieldOperand(rdi, JSBoundFunction::kBoundArgumentsOffset));
  __ SmiUntagFieldUnsigned(rbx, FieldOperand(rcx, FixedArray::kLengthOffset));
  __ testl(rbx, rbx);
  __ j(zero, &no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- rax : the number of arguments
    //  -- rdx : new.target (only in case of [[Construct]])
    //  -- rdi : target (checked to be a JSBoundFunction)
    //  -- rcx : the [[BoundArguments]] (implemented as FixedArray)
    //  -- rbx : the number of [[BoundArguments]] (checked to be non-zero)
    // -----------------------------------

    // TODO(victor): Use Generate_StackOverflowCheck here.
    // Check the stack for overflow.
    {
      Label done;
      __ shlq(rbx, Immediate(kSystemPointerSizeLog2));
      __ movq(kScratchRegister, rsp);
      __ subq(kScratchRegister, rbx);

      // We are not trying to catch interruptions (i.e. debug break and
      // preemption) here, so check the "real stack limit".
      __ cmpq(kScratchRegister,
              __ StackLimitAsOperand(StackLimitKind::kRealStackLimit));
      __ j(above_equal, &done, Label::kNear);
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Save Return Address and Receiver into registers.
    __ Pop(r8);
    __ Pop(r10);

    // Push [[BoundArguments]] to the stack.
    {
      Label loop;
      __ LoadTaggedField(
          rcx, FieldOperand(rdi, JSBoundFunction::kBoundArgumentsOffset));
      __ SmiUntagFieldUnsigned(rbx,
                               FieldOperand(rcx, FixedArray::kLengthOffset));
      __ addq(rax, rbx);  // Adjust effective number of arguments.
      __ bind(&loop);
      // Instead of doing decl(rbx) here subtract kTaggedSize from the header
      // offset in order to be able to move decl(rbx) right before the loop
      // condition. This is necessary in order to avoid flags corruption by
      // pointer decompression code.
      __ LoadTaggedField(r12,
                         FieldOperand(rcx, rbx, times_tagged_size,
                                      FixedArray::kHeaderSize - kTaggedSize));
      __ Push(r12);
      __ decl(rbx);
      __ j(greater, &loop);
    }

    // Recover Receiver and Return Address.
    __ Push(r10);
    __ Push(r8);
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments
  //  -- rdi : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(rdi);

  // Patch the receiver to [[BoundThis]].
  StackArgumentsAccessor args(rax);
  __ LoadTaggedField(rbx, FieldOperand(rdi, JSBoundFunction::kBoundThisOffset));
  __ movq(args.GetReceiverOperand(), rbx);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ LoadTaggedField(
      rdi, FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments
  //  -- rdi : the target to call (can be any Object)
  // -----------------------------------
  Register argc = rax;
  Register target = rdi;
  Register map = rcx;
  Register instance_type = rdx;
  DCHECK(!AreAliased(argc, target, map, instance_type));

  StackArgumentsAccessor args(argc);

  Label non_callable, class_constructor;
  __ JumpIfSmi(target, &non_callable);
  __ LoadMap(map, target);
  __ CmpInstanceTypeRange(map, instance_type, FIRST_CALLABLE_JS_FUNCTION_TYPE,
                          LAST_CALLABLE_JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, below_equal);

  __ cmpw(instance_type, Immediate(JS_BOUND_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, equal);

  // Check if target has a [[Call]] internal method.
  __ testb(FieldOperand(map, Map::kBitFieldOffset),
           Immediate(Map::Bits1::IsCallableBit::kMask));
  __ j(zero, &non_callable, Label::kNear);

  // Check if target is a proxy and call CallProxy external builtin
  __ cmpw(instance_type, Immediate(JS_PROXY_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET,
          equal);

  // Check if target is a wrapped function and call CallWrappedFunction external
  // builtin
  __ cmpw(instance_type, Immediate(JS_WRAPPED_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWrappedFunction),
          RelocInfo::CODE_TARGET, equal);

  // ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  __ cmpw(instance_type, Immediate(JS_CLASS_CONSTRUCTOR_TYPE));
  __ j(equal, &class_constructor);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).

  // Overwrite the original receiver with the (original) target.
  __ movq(args.GetReceiverOperand(), target);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(target, Context::CALL_AS_FUNCTION_DELEGATE_INDEX);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(target);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
    __ Trap();  // Unreachable.
  }

  // 4. The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ Push(target);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
    __ Trap();  // Unreachable.
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments
  //  -- rdx : the new target (checked to be a constructor)
  //  -- rdi : the constructor to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertConstructor(rdi);
  __ AssertFunction(rdi);

  // Calling convention for function specific ConstructStubs require
  // rbx to contain either an AllocationSite or undefined.
  __ LoadRoot(rbx, RootIndex::kUndefinedValue);

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  const TaggedRegister shared_function_info(rcx);
  __ LoadTaggedField(shared_function_info,
                     FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ testl(FieldOperand(shared_function_info, SharedFunctionInfo::kFlagsOffset),
           Immediate(SharedFunctionInfo::ConstructAsBuiltinBit::kMask));
  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET, not_zero);

  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments
  //  -- rdx : the new target (checked to be a constructor)
  //  -- rdi : the constructor to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertConstructor(rdi);
  __ AssertBoundFunction(rdi);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label done;
    __ cmpq(rdi, rdx);
    __ j(not_equal, &done, Label::kNear);
    __ LoadTaggedField(
        rdx, FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&done);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ LoadTaggedField(
      rdi, FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments
  //  -- rdx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- rdi : the constructor to call (can be any Object)
  // -----------------------------------
  Register argc = rax;
  Register target = rdi;
  Register map = rcx;
  Register instance_type = r8;
  DCHECK(!AreAliased(argc, target, map, instance_type));

  StackArgumentsAccessor args(argc);

  // Check if target is a Smi.
  Label non_constructor;
  __ JumpIfSmi(target, &non_constructor);

  // Check if target has a [[Construct]] internal method.
  __ LoadMap(map, target);
  __ testb(FieldOperand(map, Map::kBitFieldOffset),
           Immediate(Map::Bits1::IsConstructorBit::kMask));
  __ j(zero, &non_constructor);

  // Dispatch based on instance type.
  __ CmpInstanceTypeRange(map, instance_type, FIRST_JS_FUNCTION_TYPE,
                          LAST_JS_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, below_equal);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ cmpw(instance_type, Immediate(JS_BOUND_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, equal);

  // Only dispatch to proxies after checking whether they are constructors.
  __ cmpw(instance_type, Immediate(JS_PROXY_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy), RelocInfo::CODE_TARGET,
          equal);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ movq(args.GetReceiverOperand(), target);
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(target,
                             Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructedNonConstructable),
          RelocInfo::CODE_TARGET);
}

namespace {

void Generate_OSREntry(MacroAssembler* masm, Register entry_address) {
  // Overwrite the return address on the stack and "return" to the OSR entry
  // point of the function.
  __ movq(Operand(rsp, 0), entry_address);
  __ ret(0);
}

enum class OsrSourceTier {
  kInterpreter,
  kBaseline,
  kMaglev,
};

void OnStackReplacement(MacroAssembler* masm, OsrSourceTier source,
                        Register maybe_target_code) {
  Label jump_to_optimized_code;
  {
    // If maybe_target_code is not null, no need to call into runtime. A
    // precondition here is: if maybe_target_code is a InstructionStream object,
    // it must NOT be marked_for_deoptimization (callers must ensure this).
    __ testq(maybe_target_code, maybe_target_code);
    __ j(not_equal, &jump_to_optimized_code, Label::kNear);
  }

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kCompileOptimizedOSR);
  }

  // If the code object is null, just return to the caller.
  __ testq(rax, rax);
  __ j(not_equal, &jump_to_optimized_code, Label::kNear);
  __ ret(0);

  __ bind(&jump_to_optimized_code);
  DCHECK_EQ(maybe_target_code, rax);  // Already in the right spot.

  if (source == OsrSourceTier::kMaglev) {
    // Maglev doesn't enter OSR'd code itself, since OSR depends on the
    // unoptimized (~= Ignition) stack frame layout. Instead, return to Maglev
    // code and let it deoptimize.
    __ ret(0);
    return;
  }

  // OSR entry tracing.
  {
    Label next;
    __ cmpb(
        __ ExternalReferenceAsOperand(
            ExternalReference::address_of_log_or_trace_osr(), kScratchRegister),
        Immediate(0));
    __ j(equal, &next, Label::kNear);

    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(rax);  // Preserve the code object.
      __ CallRuntime(Runtime::kLogOrTraceOptimizedOSREntry, 0);
      __ Pop(rax);
    }

    __ bind(&next);
  }

  if (source == OsrSourceTier::kInterpreter) {
    // Drop the handler frame that is be sitting on top of the actual
    // JavaScript frame.
    __ leave();
  }

  // Load deoptimization data from the code object.
  const TaggedRegister deopt_data(rbx);
  __ LoadTaggedField(
      deopt_data,
      FieldOperand(rax, Code::kDeoptimizationDataOrInterpreterDataOffset));

  // Load the OSR entrypoint offset from the deoptimization data.
  __ SmiUntagField(
      rbx,
      FieldOperand(deopt_data, FixedArray::OffsetOfElementAt(
                                   DeoptimizationData::kOsrPcOffsetIndex)));

  __ LoadCodeEntry(rax, rax);

  // Compute the target address = code_entry + osr_offset
  __ addq(rax, rbx);

  Generate_OSREntry(masm, rax);
}

}  // namespace

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  using D = OnStackReplacementDescriptor;
  static_assert(D::kParameterCount == 1);
  OnStackReplacement(masm, OsrSourceTier::kInterpreter,
                     D::MaybeTargetCodeRegister());
}

void Builtins::Generate_BaselineOnStackReplacement(MacroAssembler* masm) {
  using D = OnStackReplacementDescriptor;
  static_assert(D::kParameterCount == 1);
  __ movq(kContextRegister,
          MemOperand(rbp, BaselineFrameConstants::kContextOffset));
  OnStackReplacement(masm, OsrSourceTier::kBaseline,
                     D::MaybeTargetCodeRegister());
}

void Builtins::Generate_MaglevOnStackReplacement(MacroAssembler* masm) {
  using D =
      i::CallInterfaceDescriptorFor<Builtin::kMaglevOnStackReplacement>::type;
  static_assert(D::kParameterCount == 1);
  OnStackReplacement(masm, OsrSourceTier::kMaglev,
                     D::MaybeTargetCodeRegister());
}

#if V8_ENABLE_WEBASSEMBLY

// Returns the offset beyond the last saved FP register.
int SaveWasmParams(MacroAssembler* masm) {
  // Save all parameter registers (see wasm-linkage.h). They might be
  // overwritten in the subsequent runtime call. We don't have any callee-saved
  // registers in wasm, so no need to store anything else.
  static_assert(WasmLiftoffSetupFrameConstants::kNumberOfSavedGpParamRegs + 1 ==
                    arraysize(wasm::kGpParamRegisters),
                "frame size mismatch");
  for (Register reg : wasm::kGpParamRegisters) {
    __ Push(reg);
  }
  static_assert(WasmLiftoffSetupFrameConstants::kNumberOfSavedFpParamRegs ==
                    arraysize(wasm::kFpParamRegisters),
                "frame size mismatch");
  __ AllocateStackSpace(kSimd128Size * arraysize(wasm::kFpParamRegisters));
  int offset = 0;
  for (DoubleRegister reg : wasm::kFpParamRegisters) {
    __ movdqu(Operand(rsp, offset), reg);
    offset += kSimd128Size;
  }
  return offset;
}

// Consumes the offset beyond the last saved FP register (as returned by
// {SaveWasmParams}).
void RestoreWasmParams(MacroAssembler* masm, int offset) {
  for (DoubleRegister reg : base::Reversed(wasm::kFpParamRegisters)) {
    offset -= kSimd128Size;
    __ movdqu(reg, Operand(rsp, offset));
  }
  DCHECK_EQ(0, offset);
  __ addq(rsp, Immediate(kSimd128Size * arraysize(wasm::kFpParamRegisters)));
  for (Register reg : base::Reversed(wasm::kGpParamRegisters)) {
    __ Pop(reg);
  }
}

// When this builtin is called, the topmost stack entry is the calling pc.
// This is replaced with the following:
//
// [    calling pc     ]  <-- rsp; popped by {ret}.
// [  feedback vector  ]
// [   Wasm instance   ]
// [ WASM frame marker ]
// [    saved rbp      ]  <-- rbp; this is where "calling pc" used to be.
void Builtins::Generate_WasmLiftoffFrameSetup(MacroAssembler* masm) {
  Register func_index = wasm::kLiftoffFrameSetupFunctionReg;
  Register vector = r15;
  Register calling_pc = rdi;

  __ Pop(calling_pc);
  __ Push(rbp);
  __ Move(rbp, rsp);
  __ Push(Immediate(StackFrame::TypeToMarker(StackFrame::WASM)));
  __ LoadTaggedField(vector,
                     FieldOperand(kWasmInstanceRegister,
                                  WasmInstanceObject::kFeedbackVectorsOffset));
  __ LoadTaggedField(vector, FieldOperand(vector, func_index, times_tagged_size,
                                          FixedArray::kHeaderSize));
  Label allocate_vector, done;
  __ JumpIfSmi(vector, &allocate_vector);
  __ bind(&done);
  __ Push(kWasmInstanceRegister);
  __ Push(vector);
  __ Push(calling_pc);
  __ ret(0);

  __ bind(&allocate_vector);
  // Feedback vector doesn't exist yet. Call the runtime to allocate it.
  // We temporarily change the frame type for this, because we need special
  // handling by the stack walker in case of GC.
  // For the runtime call, we create the following stack layout:
  //
  // [ reserved slot for NativeModule ]  <-- arg[2]
  // [  ("declared") function index   ]  <-- arg[1] for runtime func.
  // [         Wasm instance          ]  <-- arg[0]
  // [ ...spilled Wasm parameters...  ]
  // [           calling pc           ]
  // [   WASM_LIFTOFF_SETUP marker    ]
  // [           saved rbp            ]
  __ movq(Operand(rbp, TypedFrameConstants::kFrameTypeOffset),
          Immediate(StackFrame::TypeToMarker(StackFrame::WASM_LIFTOFF_SETUP)));
  __ set_has_frame(true);
  __ Push(calling_pc);
  int offset = SaveWasmParams(masm);

  // Arguments to the runtime function: instance, func_index.
  __ Push(kWasmInstanceRegister);
  __ SmiTag(func_index);
  __ Push(func_index);
  // Allocate a stack slot where the runtime function can spill a pointer
  // to the NativeModule.
  __ Push(rsp);
  __ Move(kContextRegister, Smi::zero());
  __ CallRuntime(Runtime::kWasmAllocateFeedbackVector, 3);
  __ movq(vector, kReturnRegister0);

  RestoreWasmParams(masm, offset);
  __ Pop(calling_pc);
  // Restore correct frame type.
  __ movq(Operand(rbp, TypedFrameConstants::kFrameTypeOffset),
          Immediate(StackFrame::TypeToMarker(StackFrame::WASM)));
  __ jmp(&done);
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was pushed to the stack by the caller as int32.
  __ Pop(r15);
  // Convert to Smi for the runtime call.
  __ SmiTag(r15);

  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameScope scope(masm, StackFrame::INTERNAL);

    int offset = SaveWasmParams(masm);

    // Push arguments for the runtime function.
    __ Push(kWasmInstanceRegister);
    __ Push(r15);
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Move(kContextRegister, Smi::zero());
    __ CallRuntime(Runtime::kWasmCompileLazy, 2);
    // The runtime function returns the jump table slot offset as a Smi. Use
    // that to compute the jump target in r15.
    __ SmiUntagUnsigned(kReturnRegister0);
    __ movq(r15, kReturnRegister0);

    RestoreWasmParams(masm, offset);
    // After the instance register has been restored, we can add the jump table
    // start to the jump table offset already stored in r15.
    __ addq(r15, MemOperand(kWasmInstanceRegister,
                            wasm::ObjectAccess::ToTagged(
                                WasmInstanceObject::kJumpTableStartOffset)));
  }

  // Finally, jump to the jump table slot for the function.
  __ jmp(r15);
}

void Builtins::Generate_WasmDebugBreak(MacroAssembler* masm) {
  HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
  {
    FrameScope scope(masm, StackFrame::WASM_DEBUG_BREAK);

    // Save all parameter registers. They might hold live values, we restore
    // them after the runtime call.
    for (Register reg :
         base::Reversed(WasmDebugBreakFrameConstants::kPushedGpRegs)) {
      __ Push(reg);
    }

    constexpr int kFpStackSize =
        kSimd128Size * WasmDebugBreakFrameConstants::kNumPushedFpRegisters;
    __ AllocateStackSpace(kFpStackSize);
    int offset = kFpStackSize;
    for (DoubleRegister reg :
         base::Reversed(WasmDebugBreakFrameConstants::kPushedFpRegs)) {
      offset -= kSimd128Size;
      __ movdqu(Operand(rsp, offset), reg);
    }

    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Move(kContextRegister, Smi::zero());
    __ CallRuntime(Runtime::kWasmDebugBreak, 0);

    // Restore registers.
    for (DoubleRegister reg : WasmDebugBreakFrameConstants::kPushedFpRegs) {
      __ movdqu(reg, Operand(rsp, offset));
      offset += kSimd128Size;
    }
    __ addq(rsp, Immediate(kFpStackSize));
    for (Register reg : WasmDebugBreakFrameConstants::kPushedGpRegs) {
      __ Pop(reg);
    }
  }

  __ ret(0);
}

namespace {
// Helper functions for the GenericJSToWasmWrapper.
void PrepareForBuiltinCall(MacroAssembler* masm, MemOperand GCScanSlotPlace,
                           const int GCScanSlotCount, Register current_param,
                           Register param_limit,
                           Register current_int_param_slot,
                           Register current_float_param_slot,
                           Register valuetypes_array_ptr,
                           Register wasm_instance, Register function_data,
                           Register original_fp) {
  // Pushes and puts the values in order onto the stack before builtin calls for
  // the GenericJSToWasmWrapper.
  __ Move(GCScanSlotPlace, GCScanSlotCount);
  __ pushq(current_param);
  __ pushq(param_limit);
  __ pushq(current_int_param_slot);
  __ pushq(current_float_param_slot);
  __ pushq(valuetypes_array_ptr);
  if (original_fp != rbp) {
    // For stack-switching only: keep the pointer to the parent stack alive.
    __ pushq(original_fp);
  }
  __ pushq(wasm_instance);
  __ pushq(function_data);
  // We had to prepare the parameters for the Call: we have to put the context
  // into rsi.
  __ LoadTaggedField(
      rsi,
      MemOperand(wasm_instance, wasm::ObjectAccess::ToTagged(
                                    WasmInstanceObject::kNativeContextOffset)));
}

void RestoreAfterBuiltinCall(MacroAssembler* masm, Register function_data,
                             Register wasm_instance,
                             Register valuetypes_array_ptr,
                             Register current_float_param_slot,
                             Register current_int_param_slot,
                             Register param_limit, Register current_param,
                             Register original_fp) {
  // Pop and load values from the stack in order into the registers after
  // builtin calls for the GenericJSToWasmWrapper.
  __ popq(function_data);
  __ popq(wasm_instance);
  if (original_fp != rbp) {
    __ popq(original_fp);
  }
  __ popq(valuetypes_array_ptr);
  __ popq(current_float_param_slot);
  __ popq(current_int_param_slot);
  __ popq(param_limit);
  __ popq(current_param);
}

// Check that the stack was in the old state (if generated code assertions are
// enabled), and switch to the new state.
void SwitchStackState(MacroAssembler* masm, Register jmpbuf,
                      wasm::JumpBuffer::StackState old_state,
                      wasm::JumpBuffer::StackState new_state) {
  if (v8_flags.debug_code) {
    __ cmpl(MemOperand(jmpbuf, wasm::kJmpBufStateOffset), Immediate(old_state));
    Label ok;
    __ j(equal, &ok, Label::kNear);
    __ Trap();
    __ bind(&ok);
  }
  __ movl(MemOperand(jmpbuf, wasm::kJmpBufStateOffset), Immediate(new_state));
}

void FillJumpBuffer(MacroAssembler* masm, Register jmpbuf, Label* pc) {
  __ movq(MemOperand(jmpbuf, wasm::kJmpBufSpOffset), rsp);
  __ movq(MemOperand(jmpbuf, wasm::kJmpBufFpOffset), rbp);
  __ movq(kScratchRegister,
          __ StackLimitAsOperand(StackLimitKind::kRealStackLimit));
  __ movq(MemOperand(jmpbuf, wasm::kJmpBufStackLimitOffset), kScratchRegister);
  __ leaq(kScratchRegister, MemOperand(pc, 0));
  __ movq(MemOperand(jmpbuf, wasm::kJmpBufPcOffset), kScratchRegister);
}

void LoadJumpBuffer(MacroAssembler* masm, Register jmpbuf, bool load_pc) {
  __ movq(rsp, MemOperand(jmpbuf, wasm::kJmpBufSpOffset));
  __ movq(rbp, MemOperand(jmpbuf, wasm::kJmpBufFpOffset));
  SwitchStackState(masm, jmpbuf, wasm::JumpBuffer::Inactive,
                   wasm::JumpBuffer::Active);
  if (load_pc) {
    __ jmp(MemOperand(jmpbuf, wasm::kJmpBufPcOffset));
  }
  // The stack limit is set separately under the ExecutionAccess lock.
}

void SaveState(MacroAssembler* masm, Register active_continuation, Register tmp,
               Label* suspend) {
  Register jmpbuf = tmp;
  __ LoadExternalPointerField(
      jmpbuf,
      FieldOperand(active_continuation, WasmContinuationObject::kJmpbufOffset),
      kWasmContinuationJmpbufTag, kScratchRegister);
  FillJumpBuffer(masm, jmpbuf, suspend);
}

// Returns the new suspender in rax.
void AllocateSuspender(MacroAssembler* masm, Register function_data,
                       Register wasm_instance) {
  MemOperand GCScanSlotPlace =
      MemOperand(rbp, BuiltinWasmWrapperConstants::kGCScanSlotCountOffset);
  __ Move(GCScanSlotPlace, 2);
  __ Push(wasm_instance);
  __ Push(function_data);
  __ LoadTaggedField(
      kContextRegister,
      MemOperand(wasm_instance, wasm::ObjectAccess::ToTagged(
                                    WasmInstanceObject::kNativeContextOffset)));
  __ CallRuntime(Runtime::kWasmAllocateSuspender);
  __ Pop(function_data);
  __ Pop(wasm_instance);
  static_assert(kReturnRegister0 == rax);
}

void LoadTargetJumpBuffer(MacroAssembler* masm, Register target_continuation) {
  Register target_jmpbuf = target_continuation;
  __ LoadExternalPointerField(
      target_jmpbuf,
      FieldOperand(target_continuation, WasmContinuationObject::kJmpbufOffset),
      kWasmContinuationJmpbufTag, kScratchRegister);
  MemOperand GCScanSlotPlace =
      MemOperand(rbp, BuiltinWasmWrapperConstants::kGCScanSlotCountOffset);
  __ Move(GCScanSlotPlace, 0);
  // Switch stack!
  LoadJumpBuffer(masm, target_jmpbuf, false);
}

void ReloadParentContinuation(MacroAssembler* masm, Register wasm_instance,
                              Register return_reg, Register tmp1,
                              Register tmp2) {
  Register active_continuation = tmp1;
  __ LoadRoot(active_continuation, RootIndex::kActiveContinuation);

  // We don't need to save the full register state since we are switching out of
  // this stack for the last time. Mark the stack as retired.
  Register jmpbuf = kScratchRegister;
  __ LoadExternalPointerField(
      jmpbuf,
      FieldOperand(active_continuation, WasmContinuationObject::kJmpbufOffset),
      kWasmContinuationJmpbufTag, tmp2);
  SwitchStackState(masm, jmpbuf, wasm::JumpBuffer::Active,
                   wasm::JumpBuffer::Retired);

  Register parent = tmp2;
  __ LoadTaggedField(
      parent,
      FieldOperand(active_continuation, WasmContinuationObject::kParentOffset));

  // Update active continuation root.
  __ movq(masm->RootAsOperand(RootIndex::kActiveContinuation), parent);
  jmpbuf = parent;
  __ LoadExternalPointerField(
      jmpbuf, FieldOperand(parent, WasmContinuationObject::kJmpbufOffset),
      kWasmContinuationJmpbufTag, tmp1);

  // Switch stack!
  LoadJumpBuffer(masm, jmpbuf, false);
  MemOperand GCScanSlotPlace =
      MemOperand(rbp, BuiltinWasmWrapperConstants::kGCScanSlotCountOffset);
  __ Move(GCScanSlotPlace, 1);
  __ Push(return_reg);
  __ Push(wasm_instance);  // Spill.
  __ Move(kContextRegister, Smi::zero());
  __ CallRuntime(Runtime::kWasmSyncStackLimit);
  __ Pop(wasm_instance);
  __ Pop(return_reg);
}

void RestoreParentSuspender(MacroAssembler* masm, Register tmp1,
                            Register tmp2) {
  Register suspender = tmp1;
  __ LoadRoot(suspender, RootIndex::kActiveSuspender);
  __ StoreTaggedSignedField(
      FieldOperand(suspender, WasmSuspenderObject::kStateOffset),
      Smi::FromInt(WasmSuspenderObject::kInactive));
  __ LoadTaggedField(
      suspender, FieldOperand(suspender, WasmSuspenderObject::kParentOffset));
  __ CompareRoot(suspender, RootIndex::kUndefinedValue);
  Label undefined;
  __ j(equal, &undefined, Label::kNear);
#ifdef DEBUG
  // Check that the parent suspender is active.
  Label parent_inactive;
  Register state = tmp2;
  __ LoadTaggedSignedField(
      state, FieldOperand(suspender, WasmSuspenderObject::kStateOffset));
  __ SmiCompare(state, Smi::FromInt(WasmSuspenderObject::kActive));
  __ j(equal, &parent_inactive, Label::kNear);
  __ Trap();
  __ bind(&parent_inactive);
#endif
  __ StoreTaggedSignedField(
      FieldOperand(suspender, WasmSuspenderObject::kStateOffset),
      Smi::FromInt(WasmSuspenderObject::kActive));
  __ bind(&undefined);
  __ movq(masm->RootAsOperand(RootIndex::kActiveSuspender), suspender);
}

void LoadFunctionDataAndWasmInstance(MacroAssembler* masm,
                                     Register function_data,
                                     Register wasm_instance) {
  Register closure = function_data;
  Register shared_function_info = closure;
  __ LoadTaggedField(
      shared_function_info,
      MemOperand(
          closure,
          wasm::ObjectAccess::SharedFunctionInfoOffsetInTaggedJSFunction()));
  closure = no_reg;
  __ LoadTaggedField(
      function_data,
      MemOperand(shared_function_info,
                 SharedFunctionInfo::kFunctionDataOffset - kHeapObjectTag));
  shared_function_info = no_reg;

  __ LoadTaggedField(
      wasm_instance,
      MemOperand(function_data,
                 WasmExportedFunctionData::kInstanceOffset - kHeapObjectTag));
}

void LoadValueTypesArray(MacroAssembler* masm, Register function_data,
                         Register valuetypes_array_ptr, Register return_count,
                         Register param_count) {
  Register signature = valuetypes_array_ptr;
  __ LoadExternalPointerField(
      signature,
      FieldOperand(function_data, WasmExportedFunctionData::kSigOffset),
      kWasmExportedFunctionDataSignatureTag, kScratchRegister);
  __ movq(return_count,
          MemOperand(signature, wasm::FunctionSig::kReturnCountOffset));
  __ movq(param_count,
          MemOperand(signature, wasm::FunctionSig::kParameterCountOffset));
  valuetypes_array_ptr = signature;
  __ movq(valuetypes_array_ptr,
          MemOperand(signature, wasm::FunctionSig::kRepsOffset));
}

void GenericJSToWasmWrapperHelper(MacroAssembler* masm, bool stack_switch) {
  // Set up the stackframe.
  __ EnterFrame(stack_switch ? StackFrame::STACK_SWITCH
                             : StackFrame::JS_TO_WASM);

  // -------------------------------------------
  // Compute offsets and prepare for GC.
  // -------------------------------------------
  constexpr int kGCScanSlotCountOffset =
      BuiltinWasmWrapperConstants::kGCScanSlotCountOffset;
  // The number of parameters passed to this function.
  constexpr int kInParamCountOffset =
      BuiltinWasmWrapperConstants::kInParamCountOffset;
  // The number of parameters according to the signature.
  constexpr int kParamCountOffset =
      BuiltinWasmWrapperConstants::kParamCountOffset;
  constexpr int kSuspenderOffset =
      BuiltinWasmWrapperConstants::kSuspenderOffset;
  constexpr int kReturnCountOffset = kSuspenderOffset - kSystemPointerSize;
  constexpr int kValueTypesArrayStartOffset =
      kReturnCountOffset - kSystemPointerSize;
  // A boolean flag to check if one of the parameters is a reference. If so, we
  // iterate over the parameters two times, first for all value types, and then
  // for all references.
  constexpr int kHasRefTypesOffset =
      kValueTypesArrayStartOffset - kSystemPointerSize;
  // We set and use this slot only when moving parameters into the parameter
  // registers (so no GC scan is needed).
  constexpr int kFunctionDataOffset = kHasRefTypesOffset - kSystemPointerSize;
  constexpr int kLastSpillOffset = kFunctionDataOffset;
  constexpr int kNumSpillSlots =
      (-TypedFrameConstants::kFixedFrameSizeFromFp - kLastSpillOffset) >>
      kSystemPointerSizeLog2;
  __ subq(rsp, Immediate(kNumSpillSlots * kSystemPointerSize));
  // Put the in_parameter count on the stack, we only  need it at the very end
  // when we pop the parameters off the stack.
  Register in_param_count = rax;
  __ decq(in_param_count);  // Exclude receiver.
  __ movq(MemOperand(rbp, kInParamCountOffset), in_param_count);
  in_param_count = no_reg;

  Register function_data = rdi;
  Register wasm_instance = rsi;
  LoadFunctionDataAndWasmInstance(masm, function_data, wasm_instance);

  Label compile_wrapper, compile_wrapper_done;
  if (!stack_switch) {
    // -------------------------------------------
    // Decrement the budget of the generic wrapper in function data.
    // -------------------------------------------
    __ SmiAddConstant(
        MemOperand(
            function_data,
            WasmExportedFunctionData::kWrapperBudgetOffset - kHeapObjectTag),
        Smi::FromInt(-1));

    // -------------------------------------------
    // Check if the budget of the generic wrapper reached 0 (zero).
    // -------------------------------------------
    // Instead of a specific comparison, we can directly use the flags set
    // from the previous addition.
    __ j(less_equal, &compile_wrapper);
    __ bind(&compile_wrapper_done);
  }

  Label suspend;
  if (stack_switch) {
    // Set the suspender spill slot to a sentinel value, in case a GC happens
    // before we set the actual value.
    __ LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
    __ movq(MemOperand(rbp, kSuspenderOffset), kScratchRegister);
    Register active_continuation = rbx;
    __ LoadRoot(active_continuation, RootIndex::kActiveContinuation);
    SaveState(masm, active_continuation, rcx, &suspend);
    AllocateSuspender(masm, function_data, wasm_instance);
    Register suspender = rax;  // Fixed.
    __ movq(MemOperand(rbp, kSuspenderOffset), suspender);
    Register target_continuation = rax;
    __ LoadTaggedField(
        target_continuation,
        FieldOperand(suspender, WasmSuspenderObject::kContinuationOffset));
    suspender = no_reg;
    // Save the old stack's rbp in r9, and use it to access the parameters in
    // the parent frame.
    // We also distribute the spill slots across the two stacks as needed by
    // creating a "shadow frame":
    //
    //      old stack:                    new stack:
    //      +-----------------+
    //      | <parent frame>  |
    //      +-----------------+
    //      | pc              |
    //      +-----------------+           +-----------------+
    //      | caller rbp      |           | 0 (jmpbuf rbp)  |
    // r9-> +-----------------+     rbp-> +-----------------+
    //      | frame marker    |           | frame marker    |
    //      +-----------------+           +-----------------+
    //      |kGCScanSlotCount |           |kGCScanSlotCount |
    //      +-----------------+           +-----------------+
    //      | kInParamCount   |           |      /          |
    //      +-----------------+           +-----------------+
    //      | kParamCount     |           |      /          |
    //      +-----------------+           +-----------------+
    //      | kSuspender      |           |      /          |
    //      +-----------------+           +-----------------+
    //      |      /          |           | kReturnCount    |
    //      +-----------------+           +-----------------+
    //      |      /          |           |kValueTypesArray |
    //      +-----------------+           +-----------------+
    //      |      /          |           | kHasRefTypes    |
    //      +-----------------+           +-----------------+
    //      |      /          |           | kFunctionData   |
    //      +-----------------+    rsp->  +-----------------+
    //          seal stack                         |
    //                                             V
    //
    // - When we first enter the prompt, we have access to both frames, so it
    // does not matter where the values are spilled.
    // - When we suspend for the first time, we longjmp to the original frame
    // (left).  So the frame needs to contain the necessary information to
    // properly deconstruct itself (actual param count and signature param
    // count).
    // - When we suspend for the second time, we longjmp to the frame that was
    // set up by the WasmResume builtin, which has the same layout as the
    // original frame (left).
    // - When the closure finally resolves, we use the value types pointer
    // stored in the shadow frame to get the return type and convert the return
    // value accordingly.
    __ movq(r9, rbp);
    LoadTargetJumpBuffer(masm, target_continuation);
    // Push the loaded rbp. We know it is null, because there is no frame yet,
    // so we could also push 0 directly. In any case we need to push it, because
    // this marks the base of the stack segment for the stack frame iterator.
    __ EnterFrame(StackFrame::STACK_SWITCH);
    __ subq(rsp, Immediate(kNumSpillSlots * kSystemPointerSize));
    // Set a sentinel value for the suspender spill slot in the new frame.
    __ LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
    __ movq(MemOperand(rbp, kSuspenderOffset), kScratchRegister);
  }
  Register original_fp = stack_switch ? r9 : rbp;

  // -------------------------------------------
  // Load values from the signature.
  // -------------------------------------------
  Register valuetypes_array_ptr = r11;
  Register return_count = r8;
  Register param_count = rcx;
  LoadValueTypesArray(masm, function_data, valuetypes_array_ptr, return_count,
                      param_count);

  // Initialize the {HasRefTypes} slot.
  __ movq(MemOperand(rbp, kHasRefTypesOffset), Immediate(0));

  // -------------------------------------------
  // Store signature-related values to the stack.
  // -------------------------------------------
  // We store values on the stack to restore them after function calls.
  // We cannot push values onto the stack right before the wasm call. The wasm
  // function expects the parameters, that didn't fit into the registers, on the
  // top of the stack.
  __ movq(MemOperand(original_fp, kParamCountOffset), param_count);
  __ movq(MemOperand(rbp, kReturnCountOffset), return_count);
  __ movq(MemOperand(rbp, kValueTypesArrayStartOffset), valuetypes_array_ptr);

  // -------------------------------------------
  // Parameter handling.
  // -------------------------------------------
  Label prepare_for_wasm_call;
  __ Cmp(param_count, 0);

  // IF we have 0 params: jump through parameter handling.
  __ j(equal, &prepare_for_wasm_call);

  // -------------------------------------------
  // Create 2 sections for integer and float params.
  // -------------------------------------------
  // We will create 2 sections on the stack for the evaluated parameters:
  // Integer and Float section, both with parameter count size. We will place
  // the parameters into these sections depending on their valuetype. This way
  // we can easily fill the general purpose and floating point parameter
  // registers and place the remaining parameters onto the stack in proper order
  // for the Wasm function. These remaining params are the final stack
  // parameters for the call to WebAssembly. Example of the stack layout after
  // processing 2 int and 1 float parameters when param_count is 4.
  //   +-----------------+
  //   |      rbp        |
  //   |-----------------|-------------------------------
  //   |                 |   Slots we defined
  //   |   Saved values  |    when setting up
  //   |                 |     the stack
  //   |                 |
  //   +-Integer section-+--- <--- start_int_section ----
  //   |  1st int param  |
  //   |- - - - - - - - -|
  //   |  2nd int param  |
  //   |- - - - - - - - -|  <----- current_int_param_slot
  //   |                 |       (points to the stackslot
  //   |- - - - - - - - -|  where the next int param should be placed)
  //   |                 |
  //   +--Float section--+--- <--- start_float_section --
  //   | 1st float param |
  //   |- - - - - - - - -|  <----  current_float_param_slot
  //   |                 |       (points to the stackslot
  //   |- - - - - - - - -|  where the next float param should be placed)
  //   |                 |
  //   |- - - - - - - - -|
  //   |                 |
  //   +---Final stack---+------------------------------
  //   +-parameters for--+------------------------------
  //   +-the Wasm call---+------------------------------
  //   |      . . .      |

  constexpr int kIntegerSectionStartOffset =
      kLastSpillOffset - kSystemPointerSize;
  // For Integer section.
  // Set the current_int_param_slot to point to the start of the section.
  Register current_int_param_slot = r10;
  __ leaq(current_int_param_slot, MemOperand(rsp, -kSystemPointerSize));
  Register params_size = param_count;
  param_count = no_reg;
  __ shlq(params_size, Immediate(kSystemPointerSizeLog2));
  __ subq(rsp, params_size);

  // For Float section.
  // Set the current_float_param_slot to point to the start of the section.
  Register current_float_param_slot = r15;
  __ leaq(current_float_param_slot, MemOperand(rsp, -kSystemPointerSize));
  __ subq(rsp, params_size);
  params_size = no_reg;
  param_count = rcx;
  __ movq(param_count, MemOperand(original_fp, kParamCountOffset));

  // -------------------------------------------
  // Set up for the param evaluation loop.
  // -------------------------------------------
  // We will loop through the params starting with the 1st param.
  // The order of processing the params is important. We have to evaluate them
  // in an increasing order.
  //       +-----------------+---------------
  //       |     param n     |
  //       |- - - - - - - - -|
  //       |    param n-1    |   Caller
  //       |       ...       | frame slots
  //       |     param 1     |
  //       |- - - - - - - - -|
  //       |    receiver     |
  //       +-----------------+---------------
  //       |  return addr    |
  //   FP->|- - - - - - - - -|
  //       |      rbp        |   Spill slots
  //       |- - - - - - - - -|
  //
  // [rbp + current_param] gives us the parameter we are processing.
  // We iterate through half-open interval <1st param, [rbp + param_limit]).

  Register current_param = rbx;
  Register param_limit = rdx;
  __ Move(current_param,
          kFPOnStackSize + kPCOnStackSize + kReceiverOnStackSize);
  __ movq(param_limit, param_count);
  __ shlq(param_limit, Immediate(kSystemPointerSizeLog2));
  __ addq(param_limit,
          Immediate(kFPOnStackSize + kPCOnStackSize + kReceiverOnStackSize));
  const int increment = kSystemPointerSize;
  Register param = rax;
  // We have to check the types of the params. The ValueType array contains
  // first the return then the param types.
  constexpr int kValueTypeSize = sizeof(wasm::ValueType);
  static_assert(kValueTypeSize == 4);
  const int32_t kValueTypeSizeLog2 = log2(kValueTypeSize);
  // Set the ValueType array pointer to point to the first parameter.
  Register returns_size = return_count;
  return_count = no_reg;
  __ shlq(returns_size, Immediate(kValueTypeSizeLog2));
  __ addq(valuetypes_array_ptr, returns_size);
  returns_size = no_reg;
  Register valuetype = r12;

  Label numeric_params_done;
  if (stack_switch) {
    // Prepare for materializing the suspender parameter. We don't materialize
    // it here but in the next loop that processes references. Here we only
    // adjust the pointers to keep the state consistent:
    // - Skip the first valuetype in the signature,
    // - Adjust the param limit which is off by one because of the extra
    // param in the signature,
    // - Set HasRefTypes to 1 to ensure that the reference loop is entered.
    __ addq(valuetypes_array_ptr, Immediate(kValueTypeSize));
    __ subq(param_limit, Immediate(kSystemPointerSize));
    __ movq(MemOperand(rbp, kHasRefTypesOffset), Immediate(1));
    __ cmpq(current_param, param_limit);
    __ j(equal, &numeric_params_done);
  }

  // -------------------------------------------
  // Param evaluation loop.
  // -------------------------------------------
  Label loop_through_params;
  __ bind(&loop_through_params);

  __ movq(param, MemOperand(original_fp, current_param, times_1, 0));
  __ movl(valuetype,
          Operand(valuetypes_array_ptr, wasm::ValueType::bit_field_offset()));

  // -------------------------------------------
  // Param conversion.
  // -------------------------------------------
  // If param is a Smi we can easily convert it. Otherwise we'll call a builtin
  // for conversion.
  Label convert_param;
  __ cmpq(valuetype, Immediate(wasm::kWasmI32.raw_bit_field()));
  __ j(not_equal, &convert_param);
  __ JumpIfNotSmi(param, &convert_param);
  // Change the param from Smi to int32 (zero extend).
  __ SmiToInt32(param);
  // Place the param into the proper slot in Integer section.
  __ movq(MemOperand(current_int_param_slot, 0), param);
  __ subq(current_int_param_slot, Immediate(kSystemPointerSize));

  // -------------------------------------------
  // Param conversion done.
  // -------------------------------------------
  Label param_conversion_done;
  __ bind(&param_conversion_done);

  __ addq(current_param, Immediate(increment));
  __ addq(valuetypes_array_ptr, Immediate(kValueTypeSize));

  __ cmpq(current_param, param_limit);
  __ j(not_equal, &loop_through_params);
  __ bind(&numeric_params_done);

  // -------------------------------------------
  // Second loop to handle references.
  // -------------------------------------------
  // In this loop we iterate over all parameters for a second time and copy all
  // reference parameters at the end of the integer parameters section.
  Label ref_params_done;
  // We check if we have seen a reference in the first parameter loop.
  Register ref_param_count = param_count;
  __ movq(ref_param_count, Immediate(0));
  __ cmpq(MemOperand(rbp, kHasRefTypesOffset), Immediate(0));
  __ j(equal, &ref_params_done);
  // We re-calculate the beginning of the value-types array and the beginning of
  // the parameters ({valuetypes_array_ptr} and {current_param}).
  __ movq(valuetypes_array_ptr, MemOperand(rbp, kValueTypesArrayStartOffset));
  return_count = current_param;
  current_param = no_reg;
  __ movq(return_count, MemOperand(rbp, kReturnCountOffset));
  returns_size = return_count;
  return_count = no_reg;
  __ shlq(returns_size, Immediate(kValueTypeSizeLog2));
  __ addq(valuetypes_array_ptr, returns_size);

  current_param = returns_size;
  returns_size = no_reg;
  __ Move(current_param,
          kFPOnStackSize + kPCOnStackSize + kReceiverOnStackSize);

  if (stack_switch) {
    // Materialize the suspender param
    __ movq(param, MemOperand(original_fp, kSuspenderOffset));
    __ movq(MemOperand(current_int_param_slot, 0), param);
    __ subq(current_int_param_slot, Immediate(kSystemPointerSize));
    __ addq(valuetypes_array_ptr, Immediate(kValueTypeSize));
    __ addq(ref_param_count, Immediate(1));
    __ cmpq(current_param, param_limit);
    __ j(equal, &ref_params_done);
  }

  Label ref_loop_through_params;
  Label ref_loop_end;
  // Start of the loop.
  __ bind(&ref_loop_through_params);

  // Load the current parameter with type.
  __ movq(param, MemOperand(original_fp, current_param, times_1, 0));
  __ movl(valuetype,
          Operand(valuetypes_array_ptr, wasm::ValueType::bit_field_offset()));
  // Extract the ValueKind of the type, to check for kRef and kRefNull.
  __ andl(valuetype, Immediate(wasm::kWasmValueKindBitsMask));
  Label move_ref_to_slot;
  __ cmpq(valuetype, Immediate(wasm::ValueKind::kRefNull));
  __ j(equal, &move_ref_to_slot);
  __ cmpq(valuetype, Immediate(wasm::ValueKind::kRef));
  __ j(equal, &move_ref_to_slot);
  __ jmp(&ref_loop_end);

  // Place the param into the proper slot in Integer section.
  __ bind(&move_ref_to_slot);
  __ addq(ref_param_count, Immediate(1));
  __ movq(MemOperand(current_int_param_slot, 0), param);
  __ subq(current_int_param_slot, Immediate(kSystemPointerSize));

  // Move to the next parameter.
  __ bind(&ref_loop_end);
  __ addq(current_param, Immediate(increment));
  __ addq(valuetypes_array_ptr, Immediate(kValueTypeSize));

  // Check if we finished all parameters.
  __ cmpq(current_param, param_limit);
  __ j(not_equal, &ref_loop_through_params);

  __ bind(&ref_params_done);
  __ movq(valuetype, ref_param_count);
  ref_param_count = valuetype;
  valuetype = no_reg;
  // -------------------------------------------
  // Move the parameters into the proper param registers.
  // -------------------------------------------
  // The Wasm function expects that the params can be popped from the top of the
  // stack in an increasing order.
  // We can always move the values on the beginning of the sections into the GP
  // or FP parameter registers. If the parameter count is less than the number
  // of parameter registers, we may move values into the registers that are not
  // in the section.
  // ----------- S t a t e -------------
  //  -- r8  : start_int_section
  //  -- rdi : start_float_section
  //  -- r10 : current_int_param_slot
  //  -- r15 : current_float_param_slot
  //  -- r11 : valuetypes_array_ptr
  //  -- r12 : valuetype
  //  -- rsi : wasm_instance
  //  -- GpParamRegisters = rax, rdx, rcx, rbx, r9
  // -----------------------------------

  Register temp_params_size = rax;
  __ movq(temp_params_size, MemOperand(original_fp, kParamCountOffset));
  __ shlq(temp_params_size, Immediate(kSystemPointerSizeLog2));
  // We want to use the register of the function_data = rdi.
  __ movq(MemOperand(rbp, kFunctionDataOffset), function_data);
  Register start_float_section = function_data;
  function_data = no_reg;
  __ movq(start_float_section, rbp);
  __ addq(start_float_section, Immediate(kIntegerSectionStartOffset));
  __ subq(start_float_section, temp_params_size);
  temp_params_size = no_reg;
  // Fill the FP param registers.
  __ Movsd(xmm1, MemOperand(start_float_section, 0));
  __ Movsd(xmm2, MemOperand(start_float_section, -kSystemPointerSize));
  __ Movsd(xmm3, MemOperand(start_float_section, -2 * kSystemPointerSize));
  __ Movsd(xmm4, MemOperand(start_float_section, -3 * kSystemPointerSize));
  __ Movsd(xmm5, MemOperand(start_float_section, -4 * kSystemPointerSize));
  __ Movsd(xmm6, MemOperand(start_float_section, -5 * kSystemPointerSize));
  // We want the start to point to the last properly placed param.
  __ subq(start_float_section, Immediate(5 * kSystemPointerSize));

  Register start_int_section = r8;
  __ movq(start_int_section, rbp);
  __ addq(start_int_section, Immediate(kIntegerSectionStartOffset));
  // Fill the GP param registers.
  __ movq(rax, MemOperand(start_int_section, 0));
  __ movq(rdx, MemOperand(start_int_section, -kSystemPointerSize));
  __ movq(rcx, MemOperand(start_int_section, -2 * kSystemPointerSize));
  __ movq(rbx, MemOperand(start_int_section, -3 * kSystemPointerSize));
  __ movq(r9, MemOperand(start_int_section, -4 * kSystemPointerSize));
  // We want the start to point to the last properly placed param.
  __ subq(start_int_section, Immediate(4 * kSystemPointerSize));

  // -------------------------------------------
  // Place the final stack parameters to the proper place.
  // -------------------------------------------
  // We want the current_param_slot (insertion) pointers to point at the last
  // param of the section instead of the next free slot.
  __ addq(current_int_param_slot, Immediate(kSystemPointerSize));
  __ addq(current_float_param_slot, Immediate(kSystemPointerSize));

  // -------------------------------------------
  // Final stack parameters loop.
  // -------------------------------------------
  // The parameters that didn't fit into the registers should be placed on the
  // top of the stack contiguously. The interval of parameters between the
  // start_section and the current_param_slot pointers define the remaining
  // parameters of the section.
  // We can iterate through the valuetypes array to decide from which section we
  // need to push the parameter onto the top of the stack. By iterating in a
  // reversed order we can easily pick the last parameter of the proper section.
  // The parameter of the section is pushed on the top of the stack only if the
  // interval of remaining params is not empty. This way we ensure that only
  // params that didn't fit into param registers are pushed again.

  Label loop_through_valuetypes;
  Label loop_place_ref_params;
  __ bind(&loop_place_ref_params);
  __ testq(ref_param_count, ref_param_count);
  __ j(zero, &loop_through_valuetypes);

  __ cmpq(start_int_section, current_int_param_slot);
  // if no int or ref param remains, directly iterate valuetypes
  __ j(less_equal, &loop_through_valuetypes);

  __ pushq(MemOperand(current_int_param_slot, 0));
  __ addq(current_int_param_slot, Immediate(kSystemPointerSize));
  __ subq(ref_param_count, Immediate(1));
  __ jmp(&loop_place_ref_params);

  valuetype = ref_param_count;
  ref_param_count = no_reg;
  __ bind(&loop_through_valuetypes);

  // We iterated through the valuetypes array, we are one field over the end in
  // the beginning. Also, we have to decrement it in each iteration.
  __ subq(valuetypes_array_ptr, Immediate(kValueTypeSize));

  // Check if there are still remaining integer params.
  Label continue_loop;
  __ cmpq(start_int_section, current_int_param_slot);
  // If there are remaining integer params.
  __ j(greater, &continue_loop);

  // Check if there are still remaining float params.
  __ cmpq(start_float_section, current_float_param_slot);
  // If there aren't any params remaining.
  Label params_done;
  __ j(less_equal, &params_done);

  __ bind(&continue_loop);
  __ movl(valuetype,
          Operand(valuetypes_array_ptr, wasm::ValueType::bit_field_offset()));
  Label place_integer_param;
  Label place_float_param;
  __ cmpq(valuetype, Immediate(wasm::kWasmI32.raw_bit_field()));
  __ j(equal, &place_integer_param);

  __ cmpq(valuetype, Immediate(wasm::kWasmI64.raw_bit_field()));
  __ j(equal, &place_integer_param);

  __ cmpq(valuetype, Immediate(wasm::kWasmF32.raw_bit_field()));
  __ j(equal, &place_float_param);

  __ cmpq(valuetype, Immediate(wasm::kWasmF64.raw_bit_field()));
  __ j(equal, &place_float_param);

  // ref params have already been pushed, so go through directly
  __ jmp(&loop_through_valuetypes);

  // All other types are reference types. We can just fall through to place them
  // in the integer section.

  __ bind(&place_integer_param);
  __ cmpq(start_int_section, current_int_param_slot);
  // If there aren't any integer params remaining, just floats, then go to the
  // next valuetype.
  __ j(less_equal, &loop_through_valuetypes);

  // Copy the param from the integer section to the actual parameter area.
  __ pushq(MemOperand(current_int_param_slot, 0));
  __ addq(current_int_param_slot, Immediate(kSystemPointerSize));
  __ jmp(&loop_through_valuetypes);

  __ bind(&place_float_param);
  __ cmpq(start_float_section, current_float_param_slot);
  // If there aren't any float params remaining, just integers, then go to the
  // next valuetype.
  __ j(less_equal, &loop_through_valuetypes);

  // Copy the param from the float section to the actual parameter area.
  __ pushq(MemOperand(current_float_param_slot, 0));
  __ addq(current_float_param_slot, Immediate(kSystemPointerSize));
  __ jmp(&loop_through_valuetypes);

  __ bind(&params_done);
  // Restore function_data after we are done with parameter placement.
  function_data = rdi;
  __ movq(function_data, MemOperand(rbp, kFunctionDataOffset));

  __ bind(&prepare_for_wasm_call);
  // -------------------------------------------
  // Prepare for the Wasm call.
  // -------------------------------------------
  // Set thread_in_wasm_flag.
  Register thread_in_wasm_flag_addr = r12;
  __ movq(
      thread_in_wasm_flag_addr,
      MemOperand(kRootRegister, Isolate::thread_in_wasm_flag_address_offset()));
  __ movl(MemOperand(thread_in_wasm_flag_addr, 0), Immediate(1));
  thread_in_wasm_flag_addr = no_reg;

  Register function_entry = function_data;
  Register scratch = r12;
  __ LoadTaggedField(
      function_entry,
      FieldOperand(function_data, WasmExportedFunctionData::kInternalOffset));
  __ LoadExternalPointerField(
      function_entry,
      FieldOperand(function_entry, WasmInternalFunction::kCallTargetOffset),
      kWasmInternalFunctionCallTargetTag, scratch);
  function_data = no_reg;
  scratch = no_reg;

  // We set the indicating value for the GC to the proper one for Wasm call.
  constexpr int kWasmCallGCScanSlotCount = 0;
  __ Move(MemOperand(rbp, kGCScanSlotCountOffset), kWasmCallGCScanSlotCount);

  // -------------------------------------------
  // Call the Wasm function.
  // -------------------------------------------
  __ call(function_entry);
  // Note: we might be returning to a different frame if the stack was suspended
  // and resumed during the call. The new frame is set up by WasmResume and has
  // a compatible layout.
  function_entry = no_reg;

  // -------------------------------------------
  // Resetting after the Wasm call.
  // -------------------------------------------
  // Restore rsp to free the reserved stack slots for the sections.
  __ leaq(rsp, MemOperand(rbp, kLastSpillOffset));

  // Unset thread_in_wasm_flag.
  thread_in_wasm_flag_addr = r8;
  __ movq(
      thread_in_wasm_flag_addr,
      MemOperand(kRootRegister, Isolate::thread_in_wasm_flag_address_offset()));
  __ movl(MemOperand(thread_in_wasm_flag_addr, 0), Immediate(0));
  thread_in_wasm_flag_addr = no_reg;

  // -------------------------------------------
  // Return handling.
  // -------------------------------------------
  return_count = r8;
  __ movq(return_count, MemOperand(rbp, kReturnCountOffset));
  Register return_reg = rax;

  // If we have 1 return value, then jump to conversion.
  __ cmpl(return_count, Immediate(1));
  Label convert_return;
  __ j(equal, &convert_return);

  // Otherwise load undefined.
  __ LoadRoot(return_reg, RootIndex::kUndefinedValue);

  Label return_done;
  __ bind(&return_done);
  if (stack_switch) {
    ReloadParentContinuation(masm, wasm_instance, return_reg, rbx, rcx);
    RestoreParentSuspender(masm, rbx, rcx);
  }
  __ bind(&suspend);
  // No need to process the return value if the stack is suspended, there is a
  // single 'externref' value (the promise) which doesn't require conversion.

  __ movq(param_count, MemOperand(rbp, kParamCountOffset));

  // Calculate the number of parameters we have to pop off the stack. This
  // number is max(in_param_count, param_count).
  in_param_count = rdx;
  __ movq(in_param_count, MemOperand(rbp, kInParamCountOffset));
  __ cmpq(param_count, in_param_count);
  __ cmovq(less, param_count, in_param_count);

  // -------------------------------------------
  // Deconstrunct the stack frame.
  // -------------------------------------------
  __ LeaveFrame(stack_switch ? StackFrame::STACK_SWITCH
                             : StackFrame::JS_TO_WASM);

  // We have to remove the caller frame slots:
  //  - JS arguments
  //  - the receiver
  // and transfer the control to the return address (the return address is
  // expected to be on the top of the stack).
  // We cannot use just the ret instruction for this, because we cannot pass the
  // number of slots to remove in a Register as an argument.
  __ DropArguments(param_count, rbx, MacroAssembler::kCountIsInteger,
                   MacroAssembler::kCountExcludesReceiver);
  __ ret(0);

  // --------------------------------------------------------------------------
  //                          Deferred code.
  // --------------------------------------------------------------------------

  // -------------------------------------------
  // Param conversion builtins.
  // -------------------------------------------
  __ bind(&convert_param);
  // Restore function_data register (which was clobbered by the code above,
  // but was valid when jumping here earlier).
  function_data = rdi;
  // The order of pushes is important. We want the heap objects, that should be
  // scanned by GC, to be on the top of the stack.
  // We have to set the indicating value for the GC to the number of values on
  // the top of the stack that have to be scanned before calling the builtin
  // function.
  // The builtin expects the parameter to be in register param = rax.

  constexpr int kBuiltinCallGCScanSlotCount = 2;
  PrepareForBuiltinCall(masm, MemOperand(rbp, kGCScanSlotCountOffset),
                        kBuiltinCallGCScanSlotCount, current_param, param_limit,
                        current_int_param_slot, current_float_param_slot,
                        valuetypes_array_ptr, wasm_instance, function_data,
                        original_fp);

  Label param_kWasmI32_not_smi;
  Label param_kWasmI64;
  Label param_kWasmF32;
  Label param_kWasmF64;

  __ cmpq(valuetype, Immediate(wasm::kWasmI32.raw_bit_field()));
  __ j(equal, &param_kWasmI32_not_smi);

  __ cmpq(valuetype, Immediate(wasm::kWasmI64.raw_bit_field()));
  __ j(equal, &param_kWasmI64);

  __ cmpq(valuetype, Immediate(wasm::kWasmF32.raw_bit_field()));
  __ j(equal, &param_kWasmF32);

  __ cmpq(valuetype, Immediate(wasm::kWasmF64.raw_bit_field()));
  __ j(equal, &param_kWasmF64);

  // The parameter is a reference. We do not convert the parameter immediately.
  // Instead we will later loop over all parameters again to handle reference
  // parameters. The reason is that later value type parameters may trigger a
  // GC, and we cannot keep reference parameters alive then. Instead we leave
  // reference parameters at their initial place on the stack and only copy them
  // once no GC can happen anymore.
  // As an optimization we set a flag here that indicates that we have seen a
  // reference so far. If there was no reference parameter, we would not iterate
  // over the parameters for a second time.
  __ movq(MemOperand(rbp, kHasRefTypesOffset), Immediate(1));
  RestoreAfterBuiltinCall(masm, function_data, wasm_instance,
                          valuetypes_array_ptr, current_float_param_slot,
                          current_int_param_slot, param_limit, current_param,
                          original_fp);
  __ jmp(&param_conversion_done);

  __ int3();

  __ bind(&param_kWasmI32_not_smi);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedNonSmiToInt32),
          RelocInfo::CODE_TARGET);
  // Param is the result of the builtin.
  __ AssertZeroExtended(param);
  RestoreAfterBuiltinCall(masm, function_data, wasm_instance,
                          valuetypes_array_ptr, current_float_param_slot,
                          current_int_param_slot, param_limit, current_param,
                          original_fp);
  __ movq(MemOperand(current_int_param_slot, 0), param);
  __ subq(current_int_param_slot, Immediate(kSystemPointerSize));
  __ jmp(&param_conversion_done);

  __ bind(&param_kWasmI64);
  __ Call(BUILTIN_CODE(masm->isolate(), BigIntToI64), RelocInfo::CODE_TARGET);
  RestoreAfterBuiltinCall(masm, function_data, wasm_instance,
                          valuetypes_array_ptr, current_float_param_slot,
                          current_int_param_slot, param_limit, current_param,
                          original_fp);
  __ movq(MemOperand(current_int_param_slot, 0), param);
  __ subq(current_int_param_slot, Immediate(kSystemPointerSize));
  __ jmp(&param_conversion_done);

  __ bind(&param_kWasmF32);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedToFloat64),
          RelocInfo::CODE_TARGET);
  RestoreAfterBuiltinCall(masm, function_data, wasm_instance,
                          valuetypes_array_ptr, current_float_param_slot,
                          current_int_param_slot, param_limit, current_param,
                          original_fp);
  // Clear higher bits.
  __ Xorpd(xmm1, xmm1);
  // Truncate float64 to float32.
  __ Cvtsd2ss(xmm1, xmm0);
  __ Movsd(MemOperand(current_float_param_slot, 0), xmm1);
  __ subq(current_float_param_slot, Immediate(kSystemPointerSize));
  __ jmp(&param_conversion_done);

  __ bind(&param_kWasmF64);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedToFloat64),
          RelocInfo::CODE_TARGET);
  RestoreAfterBuiltinCall(masm, function_data, wasm_instance,
                          valuetypes_array_ptr, current_float_param_slot,
                          current_int_param_slot, param_limit, current_param,
                          original_fp);
  __ Movsd(MemOperand(current_float_param_slot, 0), xmm0);
  __ subq(current_float_param_slot, Immediate(kSystemPointerSize));
  __ jmp(&param_conversion_done);

  // -------------------------------------------
  // Return conversions.
  // -------------------------------------------
  __ bind(&convert_return);
  // We have to make sure that the kGCScanSlotCount is set correctly when we
  // call the builtins for conversion. For these builtins it's the same as for
  // the Wasm call, that is, kGCScanSlotCount = 0, so we don't have to reset it.
  // We don't need the JS context for these builtin calls.

  __ movq(valuetypes_array_ptr, MemOperand(rbp, kValueTypesArrayStartOffset));
  // The first valuetype of the array is the return's valuetype.
  __ movl(valuetype,
          Operand(valuetypes_array_ptr, wasm::ValueType::bit_field_offset()));

  Label return_kWasmI32;
  Label return_kWasmI64;
  Label return_kWasmF32;
  Label return_kWasmF64;
  Label return_kWasmFuncRef;

  __ cmpq(valuetype, Immediate(wasm::kWasmI32.raw_bit_field()));
  __ j(equal, &return_kWasmI32);

  __ cmpq(valuetype, Immediate(wasm::kWasmI64.raw_bit_field()));
  __ j(equal, &return_kWasmI64);

  __ cmpq(valuetype, Immediate(wasm::kWasmF32.raw_bit_field()));
  __ j(equal, &return_kWasmF32);

  __ cmpq(valuetype, Immediate(wasm::kWasmF64.raw_bit_field()));
  __ j(equal, &return_kWasmF64);

  __ cmpq(valuetype, Immediate(wasm::kWasmFuncRef.raw_bit_field()));
  __ j(equal, &return_kWasmFuncRef);

  // All types that are not SIMD are reference types.
  __ cmpq(valuetype, Immediate(wasm::kWasmS128.raw_bit_field()));
  // References can be passed to JavaScript as is.
  __ j(not_equal, &return_done);

  __ int3();

  __ bind(&return_kWasmI32);
  Label to_heapnumber;
  // If pointer compression is disabled, we can convert the return to a smi.
  if (SmiValuesAre32Bits()) {
    __ SmiTag(return_reg);
  } else {
    Register temp = rbx;
    __ movq(temp, return_reg);
    // Double the return value to test if it can be a Smi.
    __ addl(temp, return_reg);
    temp = no_reg;
    // If there was overflow, convert the return value to a HeapNumber.
    __ j(overflow, &to_heapnumber);
    // If there was no overflow, we can convert to Smi.
    __ SmiTag(return_reg);
  }
  __ jmp(&return_done);

  // Handle the conversion of the I32 return value to HeapNumber when it cannot
  // be a smi.
  __ bind(&to_heapnumber);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmInt32ToHeapNumber),
          RelocInfo::CODE_TARGET);
  __ jmp(&return_done);

  __ bind(&return_kWasmI64);
  __ Call(BUILTIN_CODE(masm->isolate(), I64ToBigInt), RelocInfo::CODE_TARGET);
  __ jmp(&return_done);

  __ bind(&return_kWasmF32);
  // The builtin expects the value to be in xmm0.
  __ Movss(xmm0, xmm1);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmFloat32ToNumber),
          RelocInfo::CODE_TARGET);
  __ jmp(&return_done);

  __ bind(&return_kWasmF64);
  // The builtin expects the value to be in xmm0.
  __ Movsd(xmm0, xmm1);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmFloat64ToNumber),
          RelocInfo::CODE_TARGET);
  __ jmp(&return_done);

  __ bind(&return_kWasmFuncRef);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmFuncRefToJS),
          RelocInfo::CODE_TARGET);
  __ jmp(&return_done);

  if (!stack_switch) {
    // -------------------------------------------
    // Kick off compilation.
    // -------------------------------------------
    __ bind(&compile_wrapper);
    // Enable GC.
    MemOperand GCScanSlotPlace = MemOperand(rbp, kGCScanSlotCountOffset);
    __ Move(GCScanSlotPlace, 4);
    // Save registers to the stack.
    __ pushq(wasm_instance);
    __ pushq(function_data);
    // Push the arguments for the runtime call.
    __ Push(wasm_instance);  // first argument
    __ Push(function_data);  // second argument
                             // Set up context.
    __ Move(kContextRegister, Smi::zero());
    // Call the runtime function that kicks off compilation.
    __ CallRuntime(Runtime::kWasmCompileWrapper, 2);
    // Pop the result.
    __ movq(r9, kReturnRegister0);
    // Restore registers from the stack.
    __ popq(function_data);
    __ popq(wasm_instance);
    __ jmp(&compile_wrapper_done);
  }
}
}  // namespace

void Builtins::Generate_GenericJSToWasmWrapper(MacroAssembler* masm) {
  GenericJSToWasmWrapperHelper(masm, false);
}

void Builtins::Generate_WasmReturnPromiseOnSuspend(MacroAssembler* masm) {
  GenericJSToWasmWrapperHelper(masm, true);
}

void Builtins::Generate_WasmSuspend(MacroAssembler* masm) {
  // Set up the stackframe.
  __ EnterFrame(StackFrame::STACK_SWITCH);

  Register promise = rax;
  Register suspender = rbx;

  __ subq(rsp, Immediate(-(BuiltinWasmWrapperConstants::kGCScanSlotCountOffset -
                           TypedFrameConstants::kFixedFrameSizeFromFp)));

  // -------------------------------------------
  // Save current state in active jump buffer.
  // -------------------------------------------
  Label resume;
  Register continuation = rcx;
  __ LoadRoot(continuation, RootIndex::kActiveContinuation);
  Register jmpbuf = rdx;
  __ LoadExternalPointerField(
      jmpbuf, FieldOperand(continuation, WasmContinuationObject::kJmpbufOffset),
      kWasmContinuationJmpbufTag, r8);
  FillJumpBuffer(masm, jmpbuf, &resume);
  SwitchStackState(masm, jmpbuf, wasm::JumpBuffer::Active,
                   wasm::JumpBuffer::Inactive);
  __ StoreTaggedSignedField(
      FieldOperand(suspender, WasmSuspenderObject::kStateOffset),
      Smi::FromInt(WasmSuspenderObject::kSuspended));
  jmpbuf = no_reg;
  // live: [rax, rbx, rcx]

  Register suspender_continuation = rdx;
  __ LoadTaggedField(
      suspender_continuation,
      FieldOperand(suspender, WasmSuspenderObject::kContinuationOffset));
#ifdef DEBUG
  // -------------------------------------------
  // Check that the suspender's continuation is the active continuation.
  // -------------------------------------------
  // TODO(thibaudm): Once we add core stack-switching instructions, this check
  // will not hold anymore: it's possible that the active continuation changed
  // (due to an internal switch), so we have to update the suspender.
  __ cmpq(suspender_continuation, continuation);
  Label ok;
  __ j(equal, &ok);
  __ Trap();
  __ bind(&ok);
#endif

  // -------------------------------------------
  // Update roots.
  // -------------------------------------------
  Register caller = rcx;
  __ LoadTaggedField(caller,
                     FieldOperand(suspender_continuation,
                                  WasmContinuationObject::kParentOffset));
  __ movq(masm->RootAsOperand(RootIndex::kActiveContinuation), caller);
  Register parent = rdx;
  __ LoadTaggedField(
      parent, FieldOperand(suspender, WasmSuspenderObject::kParentOffset));
  __ movq(masm->RootAsOperand(RootIndex::kActiveSuspender), parent);
  parent = no_reg;
  // live: [rax, rcx]

  // -------------------------------------------
  // Load jump buffer.
  // -------------------------------------------
  MemOperand GCScanSlotPlace =
      MemOperand(rbp, BuiltinWasmWrapperConstants::kGCScanSlotCountOffset);
  __ Move(GCScanSlotPlace, 2);
  __ Push(promise);
  __ Push(caller);
  __ Move(kContextRegister, Smi::zero());
  __ CallRuntime(Runtime::kWasmSyncStackLimit);
  __ Pop(caller);
  __ Pop(promise);
  jmpbuf = caller;
  __ LoadExternalPointerField(
      jmpbuf, FieldOperand(caller, WasmContinuationObject::kJmpbufOffset),
      kWasmContinuationJmpbufTag, r8);
  caller = no_reg;
  __ movq(kReturnRegister0, promise);
  __ Move(GCScanSlotPlace, 0);
  LoadJumpBuffer(masm, jmpbuf, true);
  __ Trap();
  __ bind(&resume);
  __ LeaveFrame(StackFrame::STACK_SWITCH);
  __ ret(0);
}

namespace {
// Resume the suspender stored in the closure. We generate two variants of this
// builtin: the onFulfilled variant resumes execution at the saved PC and
// forwards the value, the onRejected variant throws the value.

void Generate_WasmResumeHelper(MacroAssembler* masm, wasm::OnResume on_resume) {
  __ EnterFrame(StackFrame::STACK_SWITCH);

  Register param_count = rax;
  __ decq(param_count);                    // Exclude receiver.
  Register closure = kJSFunctionRegister;  // rdi

  // These slots are not used in this builtin. But when we return from the
  // resumed continuation, we return to the GenericJSToWasmWrapper code, which
  // expects these slots to be set.
  constexpr int kInParamCountOffset =
      BuiltinWasmWrapperConstants::kInParamCountOffset;
  constexpr int kParamCountOffset =
      BuiltinWasmWrapperConstants::kParamCountOffset;
  __ subq(rsp, Immediate(3 * kSystemPointerSize));
  __ movq(MemOperand(rbp, kParamCountOffset), param_count);
  __ movq(MemOperand(rbp, kInParamCountOffset), param_count);
  // Set a sentinel value for the spill slot visited by the GC.
  __ LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
  __ movq(MemOperand(rbp, BuiltinWasmWrapperConstants::kSuspenderOffset),
          kScratchRegister);

  param_count = no_reg;

  // -------------------------------------------
  // Load suspender from closure.
  // -------------------------------------------
  Register sfi = closure;
  __ LoadTaggedField(
      sfi,
      MemOperand(
          closure,
          wasm::ObjectAccess::SharedFunctionInfoOffsetInTaggedJSFunction()));
  Register function_data = sfi;
  __ LoadTaggedField(
      function_data,
      FieldOperand(sfi, SharedFunctionInfo::kFunctionDataOffset));
  // The write barrier uses a fixed register for the host object (rdi). The next
  // barrier is on the suspender, so load it in rdi directly.
  Register suspender = rdi;
  __ LoadTaggedField(
      suspender, FieldOperand(function_data, WasmResumeData::kSuspenderOffset));
  // Check the suspender state.
  Label suspender_is_suspended;
  Register state = rdx;
  __ LoadTaggedSignedField(
      state, FieldOperand(suspender, WasmSuspenderObject::kStateOffset));
  __ SmiCompare(state, Smi::FromInt(WasmSuspenderObject::kSuspended));
  __ j(equal, &suspender_is_suspended);
  __ Trap();  // TODO(thibaudm): Throw a wasm trap.
  closure = no_reg;
  sfi = no_reg;

  __ bind(&suspender_is_suspended);
  // -------------------------------------------
  // Save current state.
  // -------------------------------------------
  Label suspend;
  Register active_continuation = r9;
  __ LoadRoot(active_continuation, RootIndex::kActiveContinuation);
  Register current_jmpbuf = rax;
  __ LoadExternalPointerField(
      current_jmpbuf,
      FieldOperand(active_continuation, WasmContinuationObject::kJmpbufOffset),
      kWasmContinuationJmpbufTag, rdx);
  FillJumpBuffer(masm, current_jmpbuf, &suspend);
  SwitchStackState(masm, current_jmpbuf, wasm::JumpBuffer::Active,
                   wasm::JumpBuffer::Inactive);
  current_jmpbuf = no_reg;

  // -------------------------------------------
  // Set the suspender and continuation parents and update the roots
  // -------------------------------------------
  Register active_suspender = rcx;
  Register slot_address = WriteBarrierDescriptor::SlotAddressRegister();
  // Check that the fixed register isn't one that is already in use.
  DCHECK(slot_address == rbx || slot_address == r8);
  __ LoadRoot(active_suspender, RootIndex::kActiveSuspender);
  __ StoreTaggedField(
      FieldOperand(suspender, WasmSuspenderObject::kParentOffset),
      active_suspender);
  __ RecordWriteField(suspender, WasmSuspenderObject::kParentOffset,
                      active_suspender, slot_address, SaveFPRegsMode::kIgnore);
  __ StoreTaggedSignedField(
      FieldOperand(suspender, WasmSuspenderObject::kStateOffset),
      Smi::FromInt(WasmSuspenderObject::kActive));
  __ movq(masm->RootAsOperand(RootIndex::kActiveSuspender), suspender);

  Register target_continuation = suspender;
  __ LoadTaggedField(
      target_continuation,
      FieldOperand(suspender, WasmSuspenderObject::kContinuationOffset));
  suspender = no_reg;
  __ StoreTaggedField(
      FieldOperand(target_continuation, WasmContinuationObject::kParentOffset),
      active_continuation);
  __ RecordWriteField(
      target_continuation, WasmContinuationObject::kParentOffset,
      active_continuation, slot_address, SaveFPRegsMode::kIgnore);
  active_continuation = no_reg;
  __ movq(masm->RootAsOperand(RootIndex::kActiveContinuation),
          target_continuation);

  MemOperand GCScanSlotPlace =
      MemOperand(rbp, BuiltinWasmWrapperConstants::kGCScanSlotCountOffset);
  __ Move(GCScanSlotPlace, 1);
  __ Push(target_continuation);
  __ Move(kContextRegister, Smi::zero());
  __ CallRuntime(Runtime::kWasmSyncStackLimit);
  __ Pop(target_continuation);

  // -------------------------------------------
  // Load state from target jmpbuf (longjmp).
  // -------------------------------------------
  Register target_jmpbuf = rdi;
  __ LoadExternalPointerField(
      target_jmpbuf,
      FieldOperand(target_continuation, WasmContinuationObject::kJmpbufOffset),
      kWasmContinuationJmpbufTag, rax);
  // Move resolved value to return register.
  __ movq(kReturnRegister0, Operand(rbp, 3 * kSystemPointerSize));
  __ Move(GCScanSlotPlace, 0);
  if (on_resume == wasm::OnResume::kThrow) {
    // Switch to the continuation's stack without restoring the PC.
    LoadJumpBuffer(masm, target_jmpbuf, false);
    // Forward the onRejected value to kThrow.
    __ pushq(kReturnRegister0);
    __ CallRuntime(Runtime::kThrow);
  } else {
    // Resume the continuation normally.
    LoadJumpBuffer(masm, target_jmpbuf, true);
  }
  __ Trap();
  __ bind(&suspend);
  __ LeaveFrame(StackFrame::STACK_SWITCH);
  // Pop receiver + parameter.
  __ ret(2 * kSystemPointerSize);
}
}  // namespace

void Builtins::Generate_WasmResume(MacroAssembler* masm) {
  Generate_WasmResumeHelper(masm, wasm::OnResume::kContinue);
}

void Builtins::Generate_WasmReject(MacroAssembler* masm) {
  Generate_WasmResumeHelper(masm, wasm::OnResume::kThrow);
}

void Builtins::Generate_WasmOnStackReplace(MacroAssembler* masm) {
  MemOperand OSRTargetSlot(rbp, -wasm::kOSRTargetOffset);
  __ movq(kScratchRegister, OSRTargetSlot);
  __ Move(OSRTargetSlot, 0);
  __ jmp(kScratchRegister);
}

#endif  // V8_ENABLE_WEBASSEMBLY

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               ArgvMode argv_mode, bool builtin_exit_frame) {
  CHECK(result_size == 1 || result_size == 2);

  using ER = ExternalReference;

  // rax: number of arguments including receiver
  // rbx: pointer to C function  (C callee-saved)
  // rbp: frame pointer of calling JS frame (restored after C call)
  // rsp: stack pointer  (restored after C call)
  // rsi: current context (restored)
  //
  // If argv_mode == ArgvMode::kRegister:
  // r15: pointer to the first argument

#ifdef V8_TARGET_OS_WIN
  // Windows 64-bit ABI only allows a single-word to be returned in register
  // rax. Larger return sizes must be written to an address passed as a hidden
  // first argument.
  static constexpr int kMaxRegisterResultSize = 1;
  const int kReservedStackSlots =
      result_size <= kMaxRegisterResultSize ? 0 : result_size;
#else
  // Simple results are returned in rax, and a struct of two pointers are
  // returned in rax+rdx.
  static constexpr int kMaxRegisterResultSize = 2;
  static constexpr int kReservedStackSlots = 0;
  CHECK_LE(result_size, kMaxRegisterResultSize);
#endif  // V8_TARGET_OS_WIN

  __ EnterExitFrame(kReservedStackSlots, builtin_exit_frame
                                             ? StackFrame::BUILTIN_EXIT
                                             : StackFrame::EXIT);

  // Set up argv in a callee-saved register. It is reused below so it must be
  // retained across the C call. In case of ArgvMode::kRegister, r15 has
  // already been set by the caller.
  static constexpr Register kArgvRegister = r15;
  if (argv_mode == ArgvMode::kStack) {
    int offset =
        StandardFrameConstants::kFixedFrameSizeAboveFp - kReceiverOnStackSize;
    __ leaq(kArgvRegister,
            Operand(rbp, rax, times_system_pointer_size, offset));
  }

  // rbx: pointer to builtin function  (C callee-saved).
  // rbp: frame pointer of exit frame  (restored after C call).
  // rsp: stack pointer (restored after C call).
  // rax: number of arguments including receiver
  // r15: argv pointer (C callee-saved).

  // Check stack alignment.
  if (v8_flags.debug_code) {
    __ CheckStackAlignment();
  }

  // Call C function. The arguments object will be created by stubs declared by
  // DECLARE_RUNTIME_FUNCTION().
  if (result_size <= kMaxRegisterResultSize) {
    // Pass a pointer to the Arguments object as the first argument.
    // Return result in single register (rax), or a register pair (rax, rdx).
    __ movq(arg_reg_1, rax);            // argc.
    __ movq(arg_reg_2, kArgvRegister);  // argv.
    __ Move(arg_reg_3, ER::isolate_address(masm->isolate()));
  } else {
#ifdef V8_TARGET_OS_WIN
    DCHECK_LE(result_size, 2);
    // Pass a pointer to the result location as the first argument.
    __ leaq(arg_reg_1, ExitFrameStackSlotOperand(0));
    // Pass a pointer to the Arguments object as the second argument.
    __ movq(arg_reg_2, rax);            // argc.
    __ movq(arg_reg_3, kArgvRegister);  // argv.
    __ Move(arg_reg_4, ER::isolate_address(masm->isolate()));
#else
    UNREACHABLE();
#endif  // V8_TARGET_OS_WIN
  }
  __ call(rbx);

#ifdef V8_TARGET_OS_WIN
  if (result_size > kMaxRegisterResultSize) {
    // Read result values stored on stack.
    DCHECK_EQ(result_size, 2);
    __ movq(kReturnRegister0, ExitFrameStackSlotOperand(0));
    __ movq(kReturnRegister1, ExitFrameStackSlotOperand(1));
  }
#endif  // V8_TARGET_OS_WIN

  // Result is in rax or rdx:rax - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(rax, RootIndex::kException);
  __ j(equal, &exception_returned);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (v8_flags.debug_code) {
    Label okay;
    __ LoadRoot(kScratchRegister, RootIndex::kTheHoleValue);
    ER pending_exception_address =
        ER::Create(IsolateAddressId::kPendingExceptionAddress, masm->isolate());
    __ cmp_tagged(kScratchRegister,
                  masm->ExternalReferenceAsOperand(pending_exception_address));
    __ j(equal, &okay, Label::kNear);
    __ int3();
    __ bind(&okay);
  }

  __ LeaveExitFrame();
  if (argv_mode == ArgvMode::kStack) {
    // Drop arguments and the receiver from the caller stack.
    __ PopReturnAddressTo(rcx);
    __ leaq(rsp, Operand(kArgvRegister, kReceiverOnStackSize));
    __ PushReturnAddressFrom(rcx);
  }
  __ ret(0);

  // Handling of exception.
  __ bind(&exception_returned);

  ER pending_handler_context_address = ER::Create(
      IsolateAddressId::kPendingHandlerContextAddress, masm->isolate());
  ER pending_handler_entrypoint_address = ER::Create(
      IsolateAddressId::kPendingHandlerEntrypointAddress, masm->isolate());
  ER pending_handler_fp_address =
      ER::Create(IsolateAddressId::kPendingHandlerFPAddress, masm->isolate());
  ER pending_handler_sp_address =
      ER::Create(IsolateAddressId::kPendingHandlerSPAddress, masm->isolate());

  // Ask the runtime for help to determine the handler. This will set rax to
  // contain the current pending exception, don't clobber it.
  ER find_handler = ER::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ Move(arg_reg_1, 0);  // argc.
    __ Move(arg_reg_2, 0);  // argv.
    __ Move(arg_reg_3, ER::isolate_address(masm->isolate()));
    __ PrepareCallCFunction(3);
    __ CallCFunction(find_handler, 3);
  }

#ifdef V8_ENABLE_CET_SHADOW_STACK
  // Drop frames from the shadow stack.
  ER num_frames_above_pending_handler_address = ER::Create(
      IsolateAddressId::kNumFramesAbovePendingHandlerAddress, masm->isolate());
  __ movq(rcx, masm->ExternalReferenceAsOperand(
                   num_frames_above_pending_handler_address));
  __ IncsspqIfSupported(rcx, kScratchRegister);
#endif  // V8_ENABLE_CET_SHADOW_STACK

  // Retrieve the handler context, SP and FP.
  __ movq(rsi,
          masm->ExternalReferenceAsOperand(pending_handler_context_address));
  __ movq(rsp, masm->ExternalReferenceAsOperand(pending_handler_sp_address));
  __ movq(rbp, masm->ExternalReferenceAsOperand(pending_handler_fp_address));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (rsi == 0) for non-JS frames.
  Label skip;
  __ testq(rsi, rsi);
  __ j(zero, &skip, Label::kNear);
  __ movq(Operand(rbp, StandardFrameConstants::kContextOffset), rsi);
  __ bind(&skip);

  // Clear c_entry_fp, like we do in `LeaveExitFrame`.
  ER c_entry_fp_address =
      ER::Create(IsolateAddressId::kCEntryFPAddress, masm->isolate());
  Operand c_entry_fp_operand =
      masm->ExternalReferenceAsOperand(c_entry_fp_address);
  __ movq(c_entry_fp_operand, Immediate(0));

  // Compute the handler entry address and jump to it.
  __ movq(rdi,
          masm->ExternalReferenceAsOperand(pending_handler_entrypoint_address));
  __ jmp(rdi);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label check_negative, process_64_bits, done;

  // Account for return address and saved regs.
  const int kArgumentOffset = 4 * kSystemPointerSize;

  MemOperand mantissa_operand(MemOperand(rsp, kArgumentOffset));
  MemOperand exponent_operand(
      MemOperand(rsp, kArgumentOffset + kDoubleSize / 2));

  // The result is returned on the stack.
  MemOperand return_operand = mantissa_operand;

  Register scratch1 = rbx;

  // Since we must use rcx for shifts below, use some other register (rax)
  // to calculate the result if ecx is the requested return register.
  Register result_reg = rax;
  // Save ecx if it isn't the return register and therefore volatile, or if it
  // is the return register, then save the temp register we use in its stead
  // for the result.
  Register save_reg = rax;
  __ pushq(rcx);
  __ pushq(scratch1);
  __ pushq(save_reg);

  __ movl(scratch1, mantissa_operand);
  __ Movsd(kScratchDoubleReg, mantissa_operand);
  __ movl(rcx, exponent_operand);

  __ andl(rcx, Immediate(HeapNumber::kExponentMask));
  __ shrl(rcx, Immediate(HeapNumber::kExponentShift));
  __ leal(result_reg, MemOperand(rcx, -HeapNumber::kExponentBias));
  __ cmpl(result_reg, Immediate(HeapNumber::kMantissaBits));
  __ j(below, &process_64_bits, Label::kNear);

  // Result is entirely in lower 32-bits of mantissa
  int delta =
      HeapNumber::kExponentBias + base::Double::kPhysicalSignificandSize;
  __ subl(rcx, Immediate(delta));
  __ xorl(result_reg, result_reg);
  __ cmpl(rcx, Immediate(31));
  __ j(above, &done, Label::kNear);
  __ shll_cl(scratch1);
  __ jmp(&check_negative, Label::kNear);

  __ bind(&process_64_bits);
  __ Cvttsd2siq(result_reg, kScratchDoubleReg);
  __ jmp(&done, Label::kNear);

  // If the double was negative, negate the integer result.
  __ bind(&check_negative);
  __ movl(result_reg, scratch1);
  __ negl(result_reg);
  __ cmpl(exponent_operand, Immediate(0));
  __ cmovl(greater, result_reg, scratch1);

  // Restore registers
  __ bind(&done);
  __ movl(return_operand, result_reg);
  __ popq(save_reg);
  __ popq(scratch1);
  __ popq(rcx);
  __ ret(0);
}

namespace {

int Offset(ExternalReference ref0, ExternalReference ref1) {
  int64_t offset = (ref0.address() - ref1.address());
  // Check that fits into int.
  DCHECK(static_cast<int>(offset) == offset);
  return static_cast<int>(offset);
}

// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Clobbers r12, r15, rbx and
// caller-save registers.  Restores context.  On return removes
// stack_space * kSystemPointerSize (GCed).
void CallApiFunctionAndReturn(MacroAssembler* masm, Register function_address,
                              ExternalReference thunk_ref,
                              Register thunk_last_arg, int stack_space,
                              Operand* stack_space_operand,
                              Operand return_value_operand) {
  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;

  Isolate* isolate = masm->isolate();
  Factory* factory = isolate->factory();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  const int kNextOffset = 0;
  const int kLimitOffset = Offset(
      ExternalReference::handle_scope_limit_address(isolate), next_address);
  const int kLevelOffset = Offset(
      ExternalReference::handle_scope_level_address(isolate), next_address);
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address(isolate);

  DCHECK(rdx == function_address || r8 == function_address);
  // Allocate HandleScope in callee-save registers.
  Register prev_next_address_reg = r12;
  Register prev_limit_reg = rbx;
  Register base_reg = r15;
  __ Move(base_reg, next_address);
  __ movq(prev_next_address_reg, Operand(base_reg, kNextOffset));
  __ movq(prev_limit_reg, Operand(base_reg, kLimitOffset));
  __ addl(Operand(base_reg, kLevelOffset), Immediate(1));

  Label profiler_enabled, done_api_call;
  __ cmpb(__ ExternalReferenceAsOperand(
              ExternalReference::is_profiling_address(isolate), rax),
          Immediate(0));
  __ j(not_zero, &profiler_enabled);
#ifdef V8_RUNTIME_CALL_STATS
  __ Move(rax, ExternalReference::address_of_runtime_stats_flag());
  __ cmpl(Operand(rax, 0), Immediate(0));
  __ j(not_zero, &profiler_enabled);
#endif  // V8_RUNTIME_CALL_STATS

  // Call the api function directly.
  __ call(function_address);
  __ bind(&done_api_call);

  // Load the value from ReturnValue
  __ movq(rax, return_value_operand);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ subl(Operand(base_reg, kLevelOffset), Immediate(1));
  __ movq(Operand(base_reg, kNextOffset), prev_next_address_reg);
  __ cmpq(prev_limit_reg, Operand(base_reg, kLimitOffset));
  __ j(not_equal, &delete_allocated_handles);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  if (stack_space_operand != nullptr) {
    DCHECK_EQ(stack_space, 0);
    __ movq(rbx, *stack_space_operand);
  }
  __ LeaveExitFrame();

  // Check if the function scheduled an exception.
  __ Move(rdi, scheduled_exception_address);
  __ Cmp(Operand(rdi, 0), factory->the_hole_value());
  __ j(not_equal, &promote_scheduled_exception);

#if DEBUG
  // Check if the function returned a valid JavaScript value.
  Label ok;
  Register return_value = rax;
  Register map = rcx;

  __ JumpIfSmi(return_value, &ok, Label::kNear);
  __ LoadMap(map, return_value);
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
    DCHECK_EQ(stack_space, 0);
    __ PopReturnAddressTo(rcx);
    // {stack_space_operand} was loaded into {rbx} above.
    __ addq(rsp, rbx);
    // Push and ret (instead of jmp) to keep the RSB and the CET shadow stack
    // balanced.
    __ PushReturnAddressFrom(rcx);
    __ ret(0);
  }

  // Call the api function via thunk wrapper.
  __ bind(&profiler_enabled);
  // Third parameter is the address of the actual getter function.
  __ Move(thunk_last_arg, function_address);
  __ Move(rax, thunk_ref);
  __ call(rax);
  __ jmp(&done_api_call);

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ movq(Operand(base_reg, kLimitOffset), prev_limit_reg);
  __ movq(prev_limit_reg, rax);
  __ LoadAddress(arg_reg_1, ExternalReference::isolate_address(isolate));
  __ LoadAddress(rax, ExternalReference::delete_handle_scope_extensions());
  __ call(rax);
  __ movq(rax, prev_limit_reg);
  __ jmp(&leave_exit_frame);
}

}  // namespace

// TODO(jgruber): Instead of explicitly setting up implicit_args_ on the stack
// in CallApiCallback, we could use the calling convention to set up the stack
// correctly in the first place.
//
// TODO(jgruber): I suspect that most of CallApiCallback could be implemented
// as a C++ trampoline, vastly simplifying the assembly implementation.

void Builtins::Generate_CallApiCallback(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rsi                 : context
  //  -- rdx                 : api function address
  //  -- rcx                 : arguments count (not including the receiver)
  //  -- rbx                 : call data
  //  -- rdi                 : holder
  //  -- rsp[0]              : return address
  //  -- rsp[8]              : argument 0 (receiver)
  //  -- rsp[16]             : argument 1
  //  -- ...
  //  -- rsp[argc * 8]       : argument (argc - 1)
  //  -- rsp[(argc + 1) * 8] : argument argc
  // -----------------------------------

  Register api_function_address = rdx;
  Register argc = rcx;
  Register call_data = rbx;
  Register holder = rdi;

  DCHECK(!AreAliased(api_function_address, argc, holder, call_data,
                     kScratchRegister));

  using FCI = FunctionCallbackInfo<v8::Value>;
  using FCA = FunctionCallbackArguments;

  static_assert(FCA::kArgsLength == 6);
  static_assert(FCA::kNewTargetIndex == 5);
  static_assert(FCA::kDataIndex == 4);
  static_assert(FCA::kReturnValueIndex == 3);
  static_assert(FCA::kReturnValueDefaultValueIndex == 2);
  static_assert(FCA::kIsolateIndex == 1);
  static_assert(FCA::kHolderIndex == 0);

  // Set up FunctionCallbackInfo's implicit_args on the stack as follows:
  //
  // Current state:
  //   rsp[0]: return address
  //
  // Target state:
  //   rsp[0 * kSystemPointerSize]: return address
  //   rsp[1 * kSystemPointerSize]: kHolder
  //   rsp[2 * kSystemPointerSize]: kIsolate
  //   rsp[3 * kSystemPointerSize]: undefined (kReturnValueDefaultValue)
  //   rsp[4 * kSystemPointerSize]: undefined (kReturnValue)
  //   rsp[5 * kSystemPointerSize]: kData
  //   rsp[6 * kSystemPointerSize]: undefined (kNewTarget)

  __ PopReturnAddressTo(rax);
  __ LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
  __ Push(kScratchRegister);
  __ Push(call_data);
  __ Push(kScratchRegister);
  __ Push(kScratchRegister);
  __ PushAddress(ExternalReference::isolate_address(masm->isolate()));
  __ Push(holder);
  // Keep a pointer to kHolder (= implicit_args) in a scratch register.
  // We use it below to set up the FunctionCallbackInfo object.
  Register scratch = rbx;
  __ movq(scratch, rsp);

  __ PushReturnAddressFrom(rax);

  // Allocate the v8::Arguments structure in the arguments' space since it's
  // not controlled by GC.
  static constexpr int kApiStackSpace = 4;
  static_assert(kApiStackSpace == sizeof(FCI) / kSystemPointerSize + 1);

  __ EnterExitFrame(kApiStackSpace, StackFrame::EXIT);

  // FunctionCallbackInfo::implicit_args_ (points at kHolder as set up above).
  constexpr int kImplicitArgsOffset = 0;
  static_assert(kImplicitArgsOffset ==
                offsetof(FCI, implicit_args_) / kSystemPointerSize);
  __ movq(ExitFrameStackSlotOperand(kImplicitArgsOffset), scratch);

  // FunctionCallbackInfo::values_ (points at the first varargs argument passed
  // on the stack).
  constexpr int kValuesOffset = 1;
  static_assert(kValuesOffset == offsetof(FCI, values_) / kSystemPointerSize);
  __ leaq(scratch,
          Operand(scratch, (FCA::kArgsLength + 1) * kSystemPointerSize));
  __ movq(ExitFrameStackSlotOperand(kValuesOffset), scratch);

  // FunctionCallbackInfo::length_.
  constexpr int kLengthOffset = 2;
  static_assert(kLengthOffset == offsetof(FCI, length_) / kSystemPointerSize);
  __ movq(ExitFrameStackSlotOperand(2), argc);

  // We also store the number of bytes to drop from the stack after returning
  // from the API function here.
  constexpr int kBytesToDropOffset = kLengthOffset + 1;
  static_assert(kBytesToDropOffset == kApiStackSpace - 1);
  __ leaq(kScratchRegister, Operand(argc, times_system_pointer_size,
                                    FCA::kArgsLength * kSystemPointerSize +
                                        kReceiverOnStackSize));
  __ movq(ExitFrameStackSlotOperand(kBytesToDropOffset), kScratchRegister);

  Register arguments_arg = arg_reg_1;
  Register callback_arg = arg_reg_2;

  // It's okay if api_function_address == callback_arg, but not arguments_arg.
  DCHECK(api_function_address != arguments_arg);

  // v8::InvocationCallback's argument.
  __ leaq(arguments_arg, ExitFrameStackSlotOperand(kImplicitArgsOffset));

  ExternalReference thunk_ref = ExternalReference::invoke_function_callback();

  Operand return_value_operand =
      ExitFrameCallerStackSlotOperand(FCA::kReturnValueIndex);
  static constexpr int kUseExitFrameStackSlotOperand = 0;
  Operand stack_space_operand = ExitFrameStackSlotOperand(3);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, callback_arg,
                           kUseExitFrameStackSlotOperand, &stack_space_operand,
                           return_value_operand);
}

void Builtins::Generate_CallApiGetter(MacroAssembler* masm) {
  Register name_arg = arg_reg_1;
  Register accessor_info_arg = arg_reg_2;
  Register getter_arg = arg_reg_3;
  Register api_function_address = r8;
  Register receiver = ApiGetterDescriptor::ReceiverRegister();
  Register holder = ApiGetterDescriptor::HolderRegister();
  Register callback = ApiGetterDescriptor::CallbackRegister();
  Register scratch = rax;
  Register decompr_scratch1 = COMPRESS_POINTERS_BOOL ? r15 : no_reg;

  DCHECK(!AreAliased(receiver, holder, callback, scratch, decompr_scratch1));

  // Build v8::PropertyCallbackInfo::args_ array on the stack and push property
  // name below the exit frame to make GC aware of them.
  using PCA = PropertyCallbackArguments;
  static_assert(PCA::kShouldThrowOnErrorIndex == 0);
  static_assert(PCA::kHolderIndex == 1);
  static_assert(PCA::kIsolateIndex == 2);
  static_assert(PCA::kReturnValueDefaultValueIndex == 3);
  static_assert(PCA::kReturnValueIndex == 4);
  static_assert(PCA::kDataIndex == 5);
  static_assert(PCA::kThisIndex == 6);
  static_assert(PCA::kArgsLength == 7);

  // Insert additional parameters into the stack frame above return address.
  __ PopReturnAddressTo(scratch);
  __ Push(receiver);
  __ PushTaggedField(FieldOperand(callback, AccessorInfo::kDataOffset),
                     decompr_scratch1);
  __ LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
  __ Push(kScratchRegister);  // return value
  __ Push(kScratchRegister);  // return value default
  __ PushAddress(ExternalReference::isolate_address(masm->isolate()));
  __ Push(holder);
  __ Push(Smi::zero());  // should_throw_on_error -> false
  __ PushTaggedField(FieldOperand(callback, AccessorInfo::kNameOffset),
                     decompr_scratch1);
  __ PushReturnAddressFrom(scratch);

  // v8::PropertyCallbackInfo::args_ array and name handle.
  static constexpr int kNameHandleStackSize = 1;
  static constexpr int kStackUnwindSpace =
      PCA::kArgsLength + kNameHandleStackSize;

  // Load address of v8::PropertyAccessorInfo::args_ array.
  __ leaq(scratch, Operand(rsp, 2 * kSystemPointerSize));

  // Allocate v8::PropertyCallbackInfo in non-GCed stack space.
  static constexpr int kArgStackSpace = 1;
  static_assert(kArgStackSpace ==
                sizeof(PropertyCallbackInfo<v8::Value>) / kSystemPointerSize);

  __ EnterExitFrame(kArgStackSpace, StackFrame::EXIT);

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // its args_ field.
  Operand info_object = ExitFrameStackSlotOperand(0);
  __ movq(info_object, scratch);

  __ leaq(name_arg, Operand(scratch, -kSystemPointerSize));
  // The context register (rsi) has been saved in EnterApiExitFrame and
  // could be used to pass arguments.
  __ leaq(accessor_info_arg, info_object);

  // It's okay if api_function_address == getter_arg, but not accessor_info_arg
  // or name_arg.
  DCHECK(api_function_address != accessor_info_arg);
  DCHECK(api_function_address != name_arg);
  __ LoadExternalPointerField(
      api_function_address,
      FieldOperand(callback, AccessorInfo::kMaybeRedirectedGetterOffset),
      kAccessorInfoGetterTag, kScratchRegister);

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback();
  Operand return_value_operand = ExitFrameCallerStackSlotOperand(
      PCA::kReturnValueIndex + kNameHandleStackSize);
  Operand* const kUseStackSpaceConstant = nullptr;

  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, getter_arg,
                           kStackUnwindSpace, kUseStackSpaceConstant,
                           return_value_operand);
}

void Builtins::Generate_DirectCEntry(MacroAssembler* masm) {
  __ int3();  // Unused on this architecture.
}

namespace {

void Generate_DeoptimizationEntry(MacroAssembler* masm,
                                  DeoptimizeKind deopt_kind) {
  Isolate* isolate = masm->isolate();

  // Save all double registers, they will later be copied to the deoptimizer's
  // FrameDescription.
  static constexpr int kDoubleRegsSize =
      kDoubleSize * XMMRegister::kNumRegisters;
  __ AllocateStackSpace(kDoubleRegsSize);

  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    XMMRegister xmm_reg = XMMRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ Movsd(Operand(rsp, offset), xmm_reg);
  }

  // Save all general purpose registers, they will later be copied to the
  // deoptimizer's FrameDescription.
  static constexpr int kNumberOfRegisters = Register::kNumRegisters;
  for (int i = 0; i < kNumberOfRegisters; i++) {
    __ pushq(Register::from_code(i));
  }

  static constexpr int kSavedRegistersAreaSize =
      kNumberOfRegisters * kSystemPointerSize + kDoubleRegsSize;
  static constexpr int kCurrentOffsetToReturnAddress = kSavedRegistersAreaSize;
  static constexpr int kCurrentOffsetToParentSP =
      kCurrentOffsetToReturnAddress + kPCOnStackSize;

  __ Store(
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate),
      rbp);

  // Get the address of the location in the code object
  // and compute the fp-to-sp delta in register arg5.
  __ movq(arg_reg_3, Operand(rsp, kCurrentOffsetToReturnAddress));
  // Load the fp-to-sp-delta.
  __ leaq(arg_reg_4, Operand(rsp, kCurrentOffsetToParentSP));
  __ subq(arg_reg_4, rbp);
  __ negq(arg_reg_4);

  // Allocate a new deoptimizer object.
  __ PrepareCallCFunction(5);
  __ Move(rax, 0);
  Label context_check;
  __ movq(rdi, Operand(rbp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(rdi, &context_check);
  __ movq(rax, Operand(rbp, StandardFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ movq(arg_reg_1, rax);
  __ Move(arg_reg_2, static_cast<int>(deopt_kind));
  // Args 3 and 4 are already in the right registers.

  // On windows put the arguments on the stack (PrepareCallCFunction
  // has created space for this). On linux pass the arguments in r8.
#ifdef V8_TARGET_OS_WIN
  Register arg5 = r15;
  __ LoadAddress(arg5, ExternalReference::isolate_address(isolate));
  __ movq(Operand(rsp, 4 * kSystemPointerSize), arg5);
#else
  // r8 is arg_reg_5 on Linux
  __ LoadAddress(r8, ExternalReference::isolate_address(isolate));
#endif

  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::new_deoptimizer_function(), 5);
  }
  // Preserve deoptimizer object in register rax and get the input
  // frame descriptor pointer.
  __ movq(rbx, Operand(rax, Deoptimizer::input_offset()));

  // Fill in the input registers.
  for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
    int offset =
        (i * kSystemPointerSize) + FrameDescription::registers_offset();
    __ PopQuad(Operand(rbx, offset));
  }

  // Fill in the double input registers.
  int double_regs_offset = FrameDescription::double_registers_offset();
  for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
    int dst_offset = i * kDoubleSize + double_regs_offset;
    __ popq(Operand(rbx, dst_offset));
  }

  // Mark the stack as not iterable for the CPU profiler which won't be able to
  // walk the stack without the return address.
  __ movb(__ ExternalReferenceAsOperand(
              ExternalReference::stack_is_iterable_address(isolate)),
          Immediate(0));

  // Remove the return address from the stack.
  __ addq(rsp, Immediate(kPCOnStackSize));

  // Compute a pointer to the unwinding limit in register rcx; that is
  // the first stack slot not part of the input frame.
  __ movq(rcx, Operand(rbx, FrameDescription::frame_size_offset()));
  __ addq(rcx, rsp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ leaq(rdx, Operand(rbx, FrameDescription::frame_content_offset()));
  Label pop_loop_header;
  __ jmp(&pop_loop_header);
  Label pop_loop;
  __ bind(&pop_loop);
  __ Pop(Operand(rdx, 0));
  __ addq(rdx, Immediate(sizeof(intptr_t)));
  __ bind(&pop_loop_header);
  __ cmpq(rcx, rsp);
  __ j(not_equal, &pop_loop);

  // Compute the output frame in the deoptimizer.
  __ pushq(rax);
  __ PrepareCallCFunction(2);
  __ movq(arg_reg_1, rax);
  __ LoadAddress(arg_reg_2, ExternalReference::isolate_address(isolate));
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::compute_output_frames_function(), 2);
  }
  __ popq(rax);

  __ movq(rsp, Operand(rax, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop, outer_loop_header, inner_loop_header;
  // Outer loop state: rax = current FrameDescription**, rdx = one past the
  // last FrameDescription**.
  __ movl(rdx, Operand(rax, Deoptimizer::output_count_offset()));
  __ movq(rax, Operand(rax, Deoptimizer::output_offset()));
  __ leaq(rdx, Operand(rax, rdx, times_system_pointer_size, 0));
  __ jmp(&outer_loop_header);
  __ bind(&outer_push_loop);
  // Inner loop state: rbx = current FrameDescription*, rcx = loop index.
  __ movq(rbx, Operand(rax, 0));
  __ movq(rcx, Operand(rbx, FrameDescription::frame_size_offset()));
  __ jmp(&inner_loop_header);
  __ bind(&inner_push_loop);
  __ subq(rcx, Immediate(sizeof(intptr_t)));
  __ Push(Operand(rbx, rcx, times_1, FrameDescription::frame_content_offset()));
  __ bind(&inner_loop_header);
  __ testq(rcx, rcx);
  __ j(not_zero, &inner_push_loop);
  __ addq(rax, Immediate(kSystemPointerSize));
  __ bind(&outer_loop_header);
  __ cmpq(rax, rdx);
  __ j(below, &outer_push_loop);

  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    XMMRegister xmm_reg = XMMRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ Movsd(xmm_reg, Operand(rbx, src_offset));
  }

  // Push pc and continuation from the last output frame.
  __ PushQuad(Operand(rbx, FrameDescription::pc_offset()));
  __ PushQuad(Operand(rbx, FrameDescription::continuation_offset()));

  // Push the registers from the last output frame.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset =
        (i * kSystemPointerSize) + FrameDescription::registers_offset();
    __ PushQuad(Operand(rbx, offset));
  }

  // Restore the registers from the stack.
  for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
    Register r = Register::from_code(i);
    // Do not restore rsp, simply pop the value into the next register
    // and overwrite this afterwards.
    if (r == rsp) {
      DCHECK_GT(i, 0);
      r = Register::from_code(i - 1);
    }
    __ popq(r);
  }

  __ movb(__ ExternalReferenceAsOperand(
              ExternalReference::stack_is_iterable_address(isolate)),
          Immediate(1));

  // Return to the continuation point.
  __ ret(0);
}

}  // namespace

void Builtins::Generate_DeoptimizationEntry_Eager(MacroAssembler* masm) {
  Generate_DeoptimizationEntry(masm, DeoptimizeKind::kEager);
}

void Builtins::Generate_DeoptimizationEntry_Lazy(MacroAssembler* masm) {
  Generate_DeoptimizationEntry(masm, DeoptimizeKind::kLazy);
}

namespace {

// Restarts execution either at the current or next (in execution order)
// bytecode. If there is baseline code on the shared function info, converts an
// interpreter frame into a baseline frame and continues execution in baseline
// code. Otherwise execution continues with bytecode.
void Generate_BaselineOrInterpreterEntry(MacroAssembler* masm,
                                         bool next_bytecode,
                                         bool is_osr = false) {
  Label start;
  __ bind(&start);

  // Get function from the frame.
  Register closure = rdi;
  __ movq(closure, MemOperand(rbp, StandardFrameConstants::kFunctionOffset));

  // Get the InstructionStream object from the shared function info.
  Register code_obj = rbx;
  TaggedRegister shared_function_info(code_obj);
  __ LoadTaggedField(
      shared_function_info,
      FieldOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ LoadTaggedField(code_obj,
                     FieldOperand(shared_function_info,
                                  SharedFunctionInfo::kFunctionDataOffset));

  // Check if we have baseline code. For OSR entry it is safe to assume we
  // always have baseline code.
  if (!is_osr) {
    Label start_with_baseline;
    __ IsObjectType(code_obj, CODE_TYPE, kScratchRegister);
    __ j(equal, &start_with_baseline);

    // Start with bytecode as there is no baseline code.
    Builtin builtin_id = next_bytecode
                             ? Builtin::kInterpreterEnterAtNextBytecode
                             : Builtin::kInterpreterEnterAtBytecode;
    __ Jump(masm->isolate()->builtins()->code_handle(builtin_id),
            RelocInfo::CODE_TARGET);

    // Start with baseline code.
    __ bind(&start_with_baseline);
  } else if (v8_flags.debug_code) {
    __ IsObjectType(code_obj, CODE_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kExpectedBaselineData);
  }

  if (v8_flags.debug_code) {
    AssertCodeIsBaseline(masm, code_obj, r11);
  }

  // Load the feedback vector.
  Register feedback_vector = r11;

  TaggedRegister feedback_cell(feedback_vector);
  __ LoadTaggedField(feedback_cell,
                     FieldOperand(closure, JSFunction::kFeedbackCellOffset));
  __ LoadTaggedField(feedback_vector,
                     FieldOperand(feedback_cell, Cell::kValueOffset));

  Label install_baseline_code;
  // Check if feedback vector is valid. If not, call prepare for baseline to
  // allocate it.
  __ IsObjectType(feedback_vector, FEEDBACK_VECTOR_TYPE, kScratchRegister);
  __ j(not_equal, &install_baseline_code);

  // Save BytecodeOffset from the stack frame.
  __ SmiUntagUnsigned(
      kInterpreterBytecodeOffsetRegister,
      MemOperand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  // Replace BytecodeOffset with the feedback vector.
  __ movq(MemOperand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp),
          feedback_vector);
  feedback_vector = no_reg;

  // Compute baseline pc for bytecode offset.
  ExternalReference get_baseline_pc_extref;
  if (next_bytecode || is_osr) {
    get_baseline_pc_extref =
        ExternalReference::baseline_pc_for_next_executed_bytecode();
  } else {
    get_baseline_pc_extref =
        ExternalReference::baseline_pc_for_bytecode_offset();
  }
  Register get_baseline_pc = r11;
  __ LoadAddress(get_baseline_pc, get_baseline_pc_extref);

  // If the code deoptimizes during the implicit function entry stack interrupt
  // check, it will have a bailout ID of kFunctionEntryBytecodeOffset, which is
  // not a valid bytecode offset.
  // TODO(pthier): Investigate if it is feasible to handle this special case
  // in TurboFan instead of here.
  Label valid_bytecode_offset, function_entry_bytecode;
  if (!is_osr) {
    __ cmpq(kInterpreterBytecodeOffsetRegister,
            Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag +
                      kFunctionEntryBytecodeOffset));
    __ j(equal, &function_entry_bytecode);
  }

  __ subq(kInterpreterBytecodeOffsetRegister,
          Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag));

  __ bind(&valid_bytecode_offset);
  // Get bytecode array from the stack frame.
  __ movq(kInterpreterBytecodeArrayRegister,
          MemOperand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ pushq(kInterpreterAccumulatorRegister);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ PrepareCallCFunction(3);
    __ movq(arg_reg_1, code_obj);
    __ movq(arg_reg_2, kInterpreterBytecodeOffsetRegister);
    __ movq(arg_reg_3, kInterpreterBytecodeArrayRegister);
    __ CallCFunction(get_baseline_pc, 3);
  }
  __ LoadCodeEntry(code_obj, code_obj);
  __ addq(code_obj, kReturnRegister0);
  __ popq(kInterpreterAccumulatorRegister);

  if (is_osr) {
    ResetBytecodeAge(masm, kInterpreterBytecodeArrayRegister);
    Generate_OSREntry(masm, code_obj);
  } else {
    __ jmp(code_obj);
  }
  __ Trap();  // Unreachable.

  if (!is_osr) {
    __ bind(&function_entry_bytecode);
    // If the bytecode offset is kFunctionEntryOffset, get the start address of
    // the first bytecode.
    __ Move(kInterpreterBytecodeOffsetRegister, 0);
    if (next_bytecode) {
      __ LoadAddress(get_baseline_pc,
                     ExternalReference::baseline_pc_for_bytecode_offset());
    }
    __ jmp(&valid_bytecode_offset);
  }

  __ bind(&install_baseline_code);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ pushq(kInterpreterAccumulatorRegister);
    __ Push(closure);
    __ CallRuntime(Runtime::kInstallBaselineCode, 1);
    __ popq(kInterpreterAccumulatorRegister);
  }
  // Retry from the start after installing baseline code.
  __ jmp(&start);
}

}  // namespace

void Builtins::Generate_BaselineOrInterpreterEnterAtBytecode(
    MacroAssembler* masm) {
  Generate_BaselineOrInterpreterEntry(masm, false);
}

void Builtins::Generate_BaselineOrInterpreterEnterAtNextBytecode(
    MacroAssembler* masm) {
  Generate_BaselineOrInterpreterEntry(masm, true);
}

void Builtins::Generate_InterpreterOnStackReplacement_ToBaseline(
    MacroAssembler* masm) {
  Generate_BaselineOrInterpreterEntry(masm, false, true);
}

void Builtins::Generate_RestartFrameTrampoline(MacroAssembler* masm) {
  // Restart the current frame:
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.

  __ movq(rdi, Operand(rbp, StandardFrameConstants::kFunctionOffset));
  __ movq(rax, Operand(rbp, StandardFrameConstants::kArgCOffset));

  __ LeaveFrame(StackFrame::INTERPRETED);

  // The arguments are already in the stack (including any necessary padding),
  // we should not try to massage the arguments again.
  __ movq(rbx, Immediate(kDontAdaptArgumentsSentinel));
  __ InvokeFunction(rdi, no_reg, rbx, rax, InvokeType::kJump);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64

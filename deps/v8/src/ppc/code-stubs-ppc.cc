// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include "src/api-arguments-inl.h"
#include "src/assembler-inl.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/double.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/objects/api-callbacks.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

#include "src/ppc/code-stubs-ppc.h"  // Cannot be the first include.

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void JSEntryStub::Generate(MacroAssembler* masm) {
  // r3: code entry
  // r4: function
  // r5: receiver
  // r6: argc
  // [sp+0]: argv

  Label invoke, handler_entry, exit;

// Called from C
  __ function_descriptor();

  {
    NoRootArrayScope no_root_array(masm);
    ProfileEntryHookStub::MaybeCallEntryHook(masm);

    // PPC LINUX ABI:
    // preserve LR in pre-reserved slot in caller's frame
    __ mflr(r0);
    __ StoreP(r0, MemOperand(sp, kStackFrameLRSlot * kPointerSize));

    // Save callee saved registers on the stack.
    __ MultiPush(kCalleeSaved);

    // Save callee-saved double registers.
    __ MultiPushDoubles(kCalleeSavedDoubles);
    // Set up the reserved register for 0.0.
    __ LoadDoubleLiteral(kDoubleRegZero, Double(0.0), r0);

    __ InitializeRootRegister();
  }

  // Push a frame with special values setup to mark it as an entry frame.
  // r3: code entry
  // r4: function
  // r5: receiver
  // r6: argc
  // r7: argv
  __ li(r0, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  __ push(r0);
  if (FLAG_enable_embedded_constant_pool) {
    __ li(kConstantPoolRegister, Operand::Zero());
    __ push(kConstantPoolRegister);
  }
  StackFrame::Type marker = type();
  __ mov(r0, Operand(StackFrame::TypeToMarker(marker)));
  __ push(r0);
  __ push(r0);
  // Save copies of the top frame descriptor on the stack.
  __ mov(r8, Operand(ExternalReference::Create(
                 IsolateAddressId::kCEntryFPAddress, isolate())));
  __ LoadP(r0, MemOperand(r8));
  __ push(r0);

  // Set up frame pointer for the frame to be pushed.
  __ addi(fp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp =
      ExternalReference::Create(IsolateAddressId::kJSEntrySPAddress, isolate());
  __ mov(r8, Operand(js_entry_sp));
  __ LoadP(r9, MemOperand(r8));
  __ cmpi(r9, Operand::Zero());
  __ bne(&non_outermost_js);
  __ StoreP(fp, MemOperand(r8));
  __ mov(ip, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont);
  __ bind(&non_outermost_js);
  __ mov(ip, Operand(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);
  __ push(ip);  // frame-type

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ b(&invoke);

  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushStackHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ mov(ip, Operand(ExternalReference::Create(
                 IsolateAddressId::kPendingExceptionAddress, isolate())));

  __ StoreP(r3, MemOperand(ip));
  __ LoadRoot(r3, RootIndex::kException);
  __ b(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  // Must preserve r3-r7.
  __ PushStackHandler();
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the b(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Expected registers by Builtins::JSEntryTrampoline
  // r3: code entry
  // r4: function
  // r5: receiver
  // r6: argc
  // r7: argv
  __ Call(EntryTrampoline(), RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // r3 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(r8);
  __ cmpi(r8, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ bne(&non_outermost_js_2);
  __ mov(r9, Operand::Zero());
  __ mov(r8, Operand(js_entry_sp));
  __ StoreP(r9, MemOperand(r8));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(r6);
  __ mov(ip, Operand(ExternalReference::Create(
                 IsolateAddressId::kCEntryFPAddress, isolate())));
  __ StoreP(r6, MemOperand(ip));

  // Reset the stack to the callee saved registers.
  __ addi(sp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

  // Restore callee-saved double registers.
  __ MultiPopDoubles(kCalleeSavedDoubles);

  // Restore callee-saved registers.
  __ MultiPop(kCalleeSaved);

  // Return
  __ LoadP(r0, MemOperand(sp, kStackFrameLRSlot * kPointerSize));
  __ mtlr(r0);
  __ blr();
}

// This stub is paired with DirectCEntryStub::GenerateCall
void DirectCEntryStub::Generate(MacroAssembler* masm) {
  // Place the return address on the stack, making the call
  // GC safe. The RegExp backend also relies on this.
  __ mflr(r0);
  __ StoreP(r0, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize));
  __ Call(ip);  // Call the C++ function.
  __ LoadP(r0, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize));
  __ mtlr(r0);
  __ blr();
}


void DirectCEntryStub::GenerateCall(MacroAssembler* masm, Register target) {
  if (FLAG_embedded_builtins) {
    if (masm->root_array_available() &&
        isolate()->ShouldLoadConstantsFromRootList()) {
      // This is basically an inlined version of Call(Handle<Code>) that loads
      // the code object into lr instead of ip.
      DCHECK_NE(ip, target);
      __ IndirectLoadConstant(ip, GetCode());
      __ addi(r0, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
      __ Move(ip, target);
      __ Call(r0);
      return;
    }
  }
  if (ABI_USES_FUNCTION_DESCRIPTORS) {
    // AIX/PPC64BE Linux use a function descriptor.
    __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(target, kPointerSize));
    __ LoadP(ip, MemOperand(target, 0));  // Instruction address
  } else {
    // ip needs to be set for DirectCEentryStub::Generate, and also
    // for ABI_CALL_VIA_IP.
    __ Move(ip, target);
  }

  intptr_t code = reinterpret_cast<intptr_t>(GetCode().location());
  __ mov(r0, Operand(code, RelocInfo::CODE_TARGET));
  __ Call(r0);  // Call the stub.
}


void ProfileEntryHookStub::MaybeCallEntryHookDelayed(TurboAssembler* tasm,
                                                     Zone* zone) {
  if (tasm->isolate()->function_entry_hook() != nullptr) {
    PredictableCodeSizeScope predictable(tasm,
#if V8_TARGET_ARCH_PPC64
                                         14 * kInstrSize);
#else
                                         11 * kInstrSize);
#endif
    tasm->mflr(r0);
    tasm->Push(r0, ip);
    tasm->CallStubDelayed(new (zone) ProfileEntryHookStub(nullptr));
    tasm->Pop(r0, ip);
    tasm->mtlr(r0);
  }
}

void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != nullptr) {
    PredictableCodeSizeScope predictable(masm,
#if V8_TARGET_ARCH_PPC64
                                         14 * kInstrSize);
#else
                                         11 * kInstrSize);
#endif
    ProfileEntryHookStub stub(masm->isolate());
    __ mflr(r0);
    __ Push(r0, ip);
    __ CallStub(&stub);
    __ Pop(r0, ip);
    __ mtlr(r0);
  }
}


void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
  // The entry hook is a "push lr, ip" instruction, followed by a call.
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + 3 * kInstrSize;

  // This should contain all kJSCallerSaved registers.
  const RegList kSavedRegs = kJSCallerSaved |  // Caller saved registers.
                             r15.bit();        // Saved stack pointer.

  // We also save lr, so the count here is one higher than the mask indicates.
  const int32_t kNumSavedRegs = kNumJSCallerSaved + 2;

  // Save all caller-save registers as this may be called from anywhere.
  __ mflr(ip);
  __ MultiPush(kSavedRegs | ip.bit());

  // Compute the function's address for the first argument.
  __ subi(r3, ip, Operand(kReturnAddressDistanceFromFunctionStart));

  // The caller's return address is two slots above the saved temporaries.
  // Grab that for the second argument to the hook.
  __ addi(r4, sp, Operand((kNumSavedRegs + 1) * kPointerSize));

  // Align the stack if necessary.
  int frame_alignment = masm->ActivationFrameAlignment();
  if (frame_alignment > kPointerSize) {
    __ mr(r15, sp);
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    __ ClearRightImm(sp, sp, Operand(WhichPowerOf2(frame_alignment)));
  }

#if !defined(USE_SIMULATOR)
  uintptr_t entry_hook =
      reinterpret_cast<uintptr_t>(isolate()->function_entry_hook());
#else
  // Under the simulator we need to indirect the entry hook through a
  // trampoline function at a known address.
  ApiFunction dispatcher(FUNCTION_ADDR(EntryHookTrampoline));
  ExternalReference entry_hook =
      ExternalReference::Create(&dispatcher, ExternalReference::BUILTIN_CALL);

  // It additionally takes an isolate as a third parameter
  __ mov(r5, Operand(ExternalReference::isolate_address(isolate())));
#endif

  __ mov(ip, Operand(entry_hook));

  if (ABI_USES_FUNCTION_DESCRIPTORS) {
    __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(ip, kPointerSize));
    __ LoadP(ip, MemOperand(ip, 0));
  }
  // ip set above, so nothing more to do for ABI_CALL_VIA_IP.

  // PPC LINUX ABI:
  __ li(r0, Operand::Zero());
  __ StorePU(r0, MemOperand(sp, -kNumRequiredStackFrameSlots * kPointerSize));

  __ Call(ip);

  __ addi(sp, sp, Operand(kNumRequiredStackFrameSlots * kPointerSize));

  // Restore the stack pointer if needed.
  if (frame_alignment > kPointerSize) {
    __ mr(sp, r15);
  }

  // Also pop lr to get Ret(0).
  __ MultiPop(kSavedRegs | ip.bit());
  __ mtlr(ip);
  __ Ret();
}

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
    Label profiler_disabled;
    Label end_profiler_check;
    __ beq(&profiler_disabled);
    __ Move(scratch, thunk_ref);
    __ b(&end_profiler_check);
    __ bind(&profiler_disabled);
    __ mr(scratch, function_address);
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

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, r3);
    __ Move(r3, ExternalReference::isolate_address(isolate));
    __ CallCFunction(ExternalReference::log_enter_external_function(), 1);
    __ PopSafepointRegisters();
  }

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
  DirectCEntryStub stub(isolate);
  stub.GenerateCall(masm, scratch);

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, r3);
    __ Move(r3, ExternalReference::isolate_address(isolate));
    __ CallCFunction(ExternalReference::log_leave_external_function(), 1);
    __ PopSafepointRegisters();
  }

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
    __ lwz(r14, *stack_space_operand);
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

void CallApiCallbackStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r7                  : call_data
  //  -- r5                  : holder
  //  -- r4                  : api_function_address
  //  -- cp                  : context
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1)* 4]   : first argument
  //  -- sp[argc * 4]        : receiver
  // -----------------------------------

  Register call_data = r7;
  Register holder = r5;
  Register api_function_address = r4;

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kArgsLength == 6);
  STATIC_ASSERT(FCA::kNewTargetIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);

  // new target
  __ PushRoot(RootIndex::kUndefinedValue);

  // call data
  __ push(call_data);

  Register scratch = call_data;
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  // return value
  __ push(scratch);
  // return value default
  __ push(scratch);
  // isolate
  __ Move(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ push(scratch);
  // holder
  __ push(holder);

  // Prepare arguments.
  __ mr(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  // PPC LINUX ABI:
  //
  // Create 4 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1-3] FunctionCallbackInfo
  const int kApiStackSpace = 4;
  const int kFunctionCallbackInfoOffset =
      (kStackFrameExtraParamSlot + 1) * kPointerSize;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  DCHECK(api_function_address != r3 && scratch != r3);
  // r3 = FunctionCallbackInfo&
  // Arguments is after the return address.
  __ addi(r3, sp, Operand(kFunctionCallbackInfoOffset));
  // FunctionCallbackInfo::implicit_args_
  __ StoreP(scratch, MemOperand(r3, 0 * kPointerSize));
  // FunctionCallbackInfo::values_
  __ addi(ip, scratch, Operand((FCA::kArgsLength - 1 + argc()) * kPointerSize));
  __ StoreP(ip, MemOperand(r3, 1 * kPointerSize));
  // FunctionCallbackInfo::length_ = argc
  __ li(ip, Operand(argc()));
  __ stw(ip, MemOperand(r3, 2 * kPointerSize));

  ExternalReference thunk_ref = ExternalReference::invoke_function_callback();

  AllowExternalCallThatCantCauseGC scope(masm);
  // Stores return the first js argument
  int return_value_offset = 2 + FCA::kReturnValueOffset;
  MemOperand return_value_operand(fp, return_value_offset * kPointerSize);
  const int stack_space = argc() + FCA::kArgsLength + 1;
  MemOperand* stack_space_operand = nullptr;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, stack_space,
                           stack_space_operand, return_value_operand);
}


void CallApiGetterStub::Generate(MacroAssembler* masm) {
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
  __ LoadP(scratch, FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ push(scratch);
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ Push(scratch, scratch);
  __ Move(scratch, ExternalReference::isolate_address(isolate()));
  __ Push(scratch, holder);
  __ Push(Smi::kZero);  // should_throw_on_error -> false
  __ LoadP(scratch, FieldMemOperand(callback, AccessorInfo::kNameOffset));
  __ push(scratch);

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ mr(r3, sp);                               // r3 = Handle<Name>
  __ addi(r4, r3, Operand(1 * kPointerSize));  // r4 = v8::PCI::args_

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
    __ StoreP(r3, MemOperand(sp, arg0Slot * kPointerSize));
    __ addi(r3, sp, Operand(arg0Slot * kPointerSize));
  }

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ StoreP(r4, MemOperand(sp, accessorInfoSlot * kPointerSize));
  __ addi(r4, sp, Operand(accessorInfoSlot * kPointerSize));
  // r4 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback();

  __ LoadP(scratch, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ LoadP(api_function_address,
        FieldMemOperand(scratch, Foreign::kForeignAddressOffset));

  // +3 is to skip prolog, return address and name handle.
  MemOperand return_value_operand(
      fp, (PropertyCallbackArguments::kReturnValueOffset + 3) * kPointerSize);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kStackUnwindSpace, nullptr, return_value_operand);
}

#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC

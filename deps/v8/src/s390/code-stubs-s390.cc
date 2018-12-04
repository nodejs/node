// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/api-arguments-inl.h"
#include "src/assembler-inl.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/objects/api-callbacks.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

#include "src/s390/code-stubs-s390.h"  // Cannot be the first include.

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void JSEntryStub::Generate(MacroAssembler* masm) {
  // r2: code entry
  // r3: function
  // r4: receiver
  // r5: argc
  // r6: argv

  Label invoke, handler_entry, exit;

  {
    NoRootArrayScope no_root_array(masm);
    ProfileEntryHookStub::MaybeCallEntryHook(masm);

// saving floating point registers
#if V8_TARGET_ARCH_S390X
    // 64bit ABI requires f8 to f15 be saved
    __ lay(sp, MemOperand(sp, -8 * kDoubleSize));
    __ std(d8, MemOperand(sp));
    __ std(d9, MemOperand(sp, 1 * kDoubleSize));
    __ std(d10, MemOperand(sp, 2 * kDoubleSize));
    __ std(d11, MemOperand(sp, 3 * kDoubleSize));
    __ std(d12, MemOperand(sp, 4 * kDoubleSize));
    __ std(d13, MemOperand(sp, 5 * kDoubleSize));
    __ std(d14, MemOperand(sp, 6 * kDoubleSize));
    __ std(d15, MemOperand(sp, 7 * kDoubleSize));
#else
    // 31bit ABI requires you to store f4 and f6:
    // http://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_s390.html#AEN417
    __ lay(sp, MemOperand(sp, -2 * kDoubleSize));
    __ std(d4, MemOperand(sp));
    __ std(d6, MemOperand(sp, kDoubleSize));
#endif

    // zLinux ABI
    //    Incoming parameters:
    //          r2: code entry
    //          r3: function
    //          r4: receiver
    //          r5: argc
    //          r6: argv
    //    Requires us to save the callee-preserved registers r6-r13
    //    General convention is to also save r14 (return addr) and
    //    sp/r15 as well in a single STM/STMG
    __ lay(sp, MemOperand(sp, -10 * kPointerSize));
    __ StoreMultipleP(r6, sp, MemOperand(sp, 0));

    // Set up the reserved register for 0.0.
    // __ LoadDoubleLiteral(kDoubleRegZero, 0.0, r0);

    // Push a frame with special values setup to mark it as an entry frame.
    //   Bad FP (-1)
    //   SMI Marker
    //   SMI Marker
    //   kCEntryFPAddress
    //   Frame type
    __ lay(sp, MemOperand(sp, -5 * kPointerSize));

    // Push a bad frame pointer to fail if it is used.
    __ LoadImmP(r10, Operand(-1));

    StackFrame::Type marker = type();
    __ Load(r9, Operand(StackFrame::TypeToMarker(marker)));
    __ Load(r8, Operand(StackFrame::TypeToMarker(marker)));
    // Save copies of the top frame descriptor on the stack.
    __ mov(r7, Operand(ExternalReference::Create(
                   IsolateAddressId::kCEntryFPAddress, isolate())));
    __ LoadP(r7, MemOperand(r7));
    __ StoreMultipleP(r7, r10, MemOperand(sp, kPointerSize));
    // Set up frame pointer for the frame to be pushed.
    // Need to add kPointerSize, because sp has one extra
    // frame already for the frame type being pushed later.
    __ lay(fp, MemOperand(
                   sp, -EntryFrameConstants::kCallerFPOffset + kPointerSize));

    __ InitializeRootRegister();
  }

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp =
      ExternalReference::Create(IsolateAddressId::kJSEntrySPAddress, isolate());
  __ mov(r7, Operand(js_entry_sp));
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
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushStackHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ mov(ip, Operand(ExternalReference::Create(
                 IsolateAddressId::kPendingExceptionAddress, isolate())));

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

  // Expected registers by Builtins::JSEntryTrampoline
  // r2: code entry
  // r3: function
  // r4: receiver
  // r5: argc
  // r6: argv
  __ Call(EntryTrampoline(), RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();
  __ bind(&exit);  // r2 holds result

  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(r7);
  __ CmpP(r7, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ bne(&non_outermost_js_2, Label::kNear);
  __ mov(r8, Operand::Zero());
  __ mov(r7, Operand(js_entry_sp));
  __ StoreP(r8, MemOperand(r7));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(r5);
  __ mov(ip, Operand(ExternalReference::Create(
                 IsolateAddressId::kCEntryFPAddress, isolate())));
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

// This stub is paired with DirectCEntryStub::GenerateCall
void DirectCEntryStub::Generate(MacroAssembler* masm) {
  __ CleanseP(r14);

  __ b(ip);  // Callee will return to R14 directly
}

void DirectCEntryStub::GenerateCall(MacroAssembler* masm, Register target) {
  if (FLAG_embedded_builtins) {
    if (masm->root_array_available() &&
        isolate()->ShouldLoadConstantsFromRootList()) {
      // This is basically an inlined version of Call(Handle<Code>) that loads
      // the code object into lr instead of ip.
      __ Move(ip, target);
      __ IndirectLoadConstant(r1, GetCode());
      __ AddP(r1, r1, Operand(Code::kHeaderSize - kHeapObjectTag));
      __ Call(r1);
      return;
    }
  }
#if ABI_USES_FUNCTION_DESCRIPTORS && !defined(USE_SIMULATOR)
  // Native AIX/S390X Linux use a function descriptor.
  __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(target, kPointerSize));
  __ LoadP(target, MemOperand(target, 0));  // Instruction address
#else
  // ip needs to be set for DirectCEentryStub::Generate, and also
  // for ABI_CALL_VIA_IP.
  __ Move(ip, target);
#endif

  __ call(GetCode(), RelocInfo::CODE_TARGET);  // Call the stub.
}

void ProfileEntryHookStub::MaybeCallEntryHookDelayed(TurboAssembler* tasm,
                                                     Zone* zone) {
  if (tasm->isolate()->function_entry_hook() != nullptr) {
    PredictableCodeSizeScope predictable(tasm,
#if V8_TARGET_ARCH_S390X
                                         40);
#elif V8_HOST_ARCH_S390
                                         36);
#else
                                         32);
#endif
    tasm->CleanseP(r14);
    tasm->Push(r14, ip);
    tasm->CallStubDelayed(new (zone) ProfileEntryHookStub(nullptr));
    tasm->Pop(r14, ip);
  }
}

void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != nullptr) {
    PredictableCodeSizeScope predictable(masm,
#if V8_TARGET_ARCH_S390X
                                         40);
#elif V8_HOST_ARCH_S390
                                         36);
#else
                                         32);
#endif
    ProfileEntryHookStub stub(masm->isolate());
    __ CleanseP(r14);
    __ Push(r14, ip);
    __ CallStub(&stub);  // BRASL
    __ Pop(r14, ip);
  }
}

void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
// The entry hook is a "push lr" instruction (LAY+ST/STG), followed by a call.
#if V8_TARGET_ARCH_S390X
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + 18;  // LAY + STG * 2
#elif V8_HOST_ARCH_S390
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + 18;  // NILH + LAY + ST * 2
#else
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + 14;  // LAY + ST * 2
#endif

  // This should contain all kJSCallerSaved registers.
  const RegList kSavedRegs = kJSCallerSaved |  // Caller saved registers.
                             r7.bit();         // Saved stack pointer.

  // We also save r14+ip, so count here is one higher than the mask indicates.
  const int32_t kNumSavedRegs = kNumJSCallerSaved + 3;

  // Save all caller-save registers as this may be called from anywhere.
  __ CleanseP(r14);
  __ LoadRR(ip, r14);
  __ MultiPush(kSavedRegs | ip.bit());

  // Compute the function's address for the first argument.

  __ SubP(r2, ip, Operand(kReturnAddressDistanceFromFunctionStart));

  // The caller's return address is two slots above the saved temporaries.
  // Grab that for the second argument to the hook.
  __ lay(r3, MemOperand(sp, kNumSavedRegs * kPointerSize));

  // Align the stack if necessary.
  int frame_alignment = masm->ActivationFrameAlignment();
  if (frame_alignment > kPointerSize) {
    __ LoadRR(r7, sp);
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    __ ClearRightImm(sp, sp, Operand(WhichPowerOf2(frame_alignment)));
  }

#if !defined(USE_SIMULATOR)
  uintptr_t entry_hook =
      reinterpret_cast<uintptr_t>(isolate()->function_entry_hook());
  __ mov(ip, Operand(entry_hook));

#if ABI_USES_FUNCTION_DESCRIPTORS
  // Function descriptor
  __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(ip, kPointerSize));
  __ LoadP(ip, MemOperand(ip, 0));
// ip already set.
#endif
#endif

  // zLinux ABI requires caller's frame to have sufficient space for callee
  // preserved regsiter save area.
  __ LoadImmP(r0, Operand::Zero());
  __ lay(sp, MemOperand(sp, -kCalleeRegisterSaveAreaSize -
                                kNumRequiredStackFrameSlots * kPointerSize));
  __ StoreP(r0, MemOperand(sp));
#if defined(USE_SIMULATOR)
  // Under the simulator we need to indirect the entry hook through a
  // trampoline function at a known address.
  // It additionally takes an isolate as a third parameter
  __ mov(r4, Operand(ExternalReference::isolate_address(isolate())));

  ApiFunction dispatcher(FUNCTION_ADDR(EntryHookTrampoline));
  __ mov(ip, Operand(ExternalReference::Create(
                 &dispatcher, ExternalReference::BUILTIN_CALL)));
#endif
  __ Call(ip);

  // zLinux ABI requires caller's frame to have sufficient space for callee
  // preserved regsiter save area.
  __ la(sp, MemOperand(sp, kCalleeRegisterSaveAreaSize +
                               kNumRequiredStackFrameSlots * kPointerSize));

  // Restore the stack pointer if needed.
  if (frame_alignment > kPointerSize) {
    __ LoadRR(sp, r7);
  }

  // Also pop lr to get Ret(0).
  __ MultiPop(kSavedRegs | ip.bit());
  __ LoadRR(r14, ip);
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

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
  DirectCEntryStub stub(isolate);
  stub.GenerateCall(masm, scratch);

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
  if (stack_space_operand != nullptr) {
    __ l(r6, *stack_space_operand);
  } else {
    __ mov(r6, Operand(stack_space));
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

void CallApiCallbackStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r6                  : call_data
  //  -- r4                  : holder
  //  -- r3                  : api_function_address
  //  -- cp                  : context
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1) * 4]  : first argument
  //  -- sp[argc * 4]        : receiver
  // -----------------------------------

  Register call_data = r6;
  Register holder = r4;
  Register api_function_address = r3;

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
  __ LoadRR(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  // S390 LINUX ABI:
  //
  // Create 4 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1-3] FunctionCallbackInfo
  const int kApiStackSpace = 4;
  const int kFunctionCallbackInfoOffset =
      (kStackFrameExtraParamSlot + 1) * kPointerSize;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  DCHECK(api_function_address != r2 && scratch != r2);
  // r2 = FunctionCallbackInfo&
  // Arguments is after the return address.
  __ AddP(r2, sp, Operand(kFunctionCallbackInfoOffset));
  // FunctionCallbackInfo::implicit_args_
  __ StoreP(scratch, MemOperand(r2, 0 * kPointerSize));
  // FunctionCallbackInfo::values_
  __ AddP(ip, scratch, Operand((FCA::kArgsLength - 1 + argc()) * kPointerSize));
  __ StoreP(ip, MemOperand(r2, 1 * kPointerSize));
  // FunctionCallbackInfo::length_ = argc
  __ LoadImmP(ip, Operand(argc()));
  __ StoreW(ip, MemOperand(r2, 2 * kPointerSize));

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
  Register scratch = r6;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  Register api_function_address = r4;

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
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kStackUnwindSpace, nullptr, return_value_operand);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390

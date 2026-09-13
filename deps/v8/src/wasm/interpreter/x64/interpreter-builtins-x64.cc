// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/code-factory.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/register-configuration.h"
#include "src/codegen/signature.h"
#include "src/execution/frame-constants.h"
#include "src/execution/isolate.h"
#include "src/objects/shared-function-info.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/interpreter/wasm-interpreter-runtime.h"
#include "src/wasm/interpreter/wasm-interpreter.h"
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

#if V8_ENABLE_WEBASSEMBLY

void Builtins::Generate_WasmInterpreterEntry(MacroAssembler* masm) {
  Register wasm_instance = kWasmImplicitArgRegister;
  Register function_index = r12;
  Register array_start = r15;
  Register result_start = rax;
  Register ref_params_array = rbx;

  // Set up the stackframe.
  __ EnterFrame(StackFrame::WASM_INTERPRETER_ENTRY);

  // Push tagged values first so they are contiguous for GC scanning.
  __ pushq(wasm_instance);
  __ pushq(ref_params_array);
  __ pushq(function_index);
  __ pushq(array_start);
  __ pushq(result_start);
  __ Move(wasm_instance, 0);
  __ CallRuntime(Runtime::kWasmRunInterpreter, 5);

  // Deconstruct the stack frame.
  __ LeaveFrame(StackFrame::WASM_INTERPRETER_ENTRY);
  __ ret(0);
}

// Given a WasmExportedFunctionData in |function_data|, derive the Wasm
// instance object into |wasm_instance|. The two registers may alias.
void LoadWasmInstanceFromFunctionData(MacroAssembler* masm,
                                      Register function_data,
                                      Register wasm_instance) {
  Register trusted_instance_data = wasm_instance;
#if V8_ENABLE_SANDBOX
  __ DecompressProtected(
      trusted_instance_data,
      MemOperand(function_data,
                 offsetof(WasmExportedFunctionData, protected_instance_data_) -
                     kHeapObjectTag));
#else
  __ LoadTaggedField(
      trusted_instance_data,
      MemOperand(function_data,
                 offsetof(WasmExportedFunctionData, protected_instance_data_) -
                     kHeapObjectTag));
#endif
  __ LoadTaggedField(wasm_instance,
                     MemOperand(trusted_instance_data,
                                WasmTrustedInstanceData::kInstanceObjectOffset -
                                    kHeapObjectTag));
}

void LoadFunctionDataAndWasmInstance(MacroAssembler* masm,
                                     Register function_data,
                                     Register wasm_instance) {
  Register closure = function_data;
  Register shared_function_info = closure;
  __ LoadTaggedField(
      shared_function_info,
      FieldMemOperand(closure, offsetof(JSFunction, shared_function_info_)));
  closure = no_reg;
  __ LoadTrustedPointerField(
      function_data,
      FieldOperand(shared_function_info,
                   offsetof(SharedFunctionInfo, trusted_function_data_)),
      kWasmExportedFunctionDataIndirectPointerTag, kScratchRegister);

  shared_function_info = no_reg;

  LoadWasmInstanceFromFunctionData(masm, function_data, wasm_instance);
}

void LoadFromSignature(MacroAssembler* masm, Register valuetypes_array_ptr,
                       Register return_count, Register param_count) {
  Register signature = valuetypes_array_ptr;
  __ movq(return_count,
          MemOperand(signature, wasm::FunctionSig::kReturnCountOffset));
  __ movq(param_count,
          MemOperand(signature, wasm::FunctionSig::kParameterCountOffset));
  valuetypes_array_ptr = signature;
  __ movq(valuetypes_array_ptr,
          MemOperand(signature, wasm::FunctionSig::kRepsOffset));
}

void LoadValueTypesArray(MacroAssembler* masm, Register function_data,
                         Register valuetypes_array_ptr, Register return_count,
                         Register param_count, Register signature_data) {
  __ LoadTaggedField(
      signature_data,
      FieldOperand(function_data,
                   offsetof(WasmExportedFunctionData, packed_args_size_)));
  __ SmiToInt32(signature_data);

  Register internal_function = valuetypes_array_ptr;
  __ LoadProtectedPointerField(
      internal_function,
      MemOperand(
          function_data,
          offsetof(WasmFunctionData, protected_internal_) - kHeapObjectTag));

  Register signature = internal_function;
  __ movq(signature,
          MemOperand(internal_function,
                     offsetof(WasmInternalFunction, sig_) - kHeapObjectTag));
  LoadFromSignature(masm, valuetypes_array_ptr, return_count, param_count);
}

void Builtins::Generate_JSToWasmInterpreterWrapperAsm(MacroAssembler* masm) {
  __ EnterFrame(StackFrame::JS_TO_WASM);

  /* Current frame */
  // fp + kImplicitArgOffset: functionData
  // fp + kResultArrayParamOffset: result array
  // fp + 8 * kSystemPointerSize: return address
  // fp:  old fp
  // fp - 8 : type
  // fp -16 : wrapperbuffer
  // fp -24 : GcScanSlotCount
  // fp -32 : WasmInstance
  // fp -40 : result_array

  // Load the implicit argument (instance data or import data) from the frame.
  Register wasm_instance = kWasmImplicitArgRegister;  // rsi
  Register implicitArg = rax;
  Register result_array = kScratchRegister;
  Register wrapper_buffer =
      WasmJSToWasmWrapperDescriptor::WrapperBufferRegister();

  // Leave the frame space
  __ subq(rsp, Immediate(4 * kSystemPointerSize));
  __ movq(implicitArg,
          MemOperand(rbp, JSToWasmWrapperFrameConstants::kImplicitArgOffset));
  LoadWasmInstanceFromFunctionData(masm, /*function_data=*/implicitArg,
                                   /*wasm_instance=*/wasm_instance);
  __ movq(
      result_array,
      MemOperand(rbp, JSToWasmWrapperFrameConstants::kResultArrayParamOffset));
  __ movq(MemOperand(rbp, WasmInterpreterWrapperConstants::kImplicitArgOffset),
          implicitArg);
  __ movq(MemOperand(rbp, WasmInterpreterWrapperConstants::kResultArrayOffset),
          result_array);

  Register params_start = r15;
  __ movq(
      params_start,
      MemOperand(wrapper_buffer,
                 WasmInterpreterWrapperConstants::kWrapperBufferParamStart));

  Register function_index = r12;
  __ movl(
      function_index,
      MemOperand(wrapper_buffer,
                 WasmInterpreterWrapperConstants::kWrapperBufferCallTarget));
  __ SmiTag(function_index);
  __ AssertSmi(function_index);
  // Set kGCScanSlotCountOffset
  __ Move(
      MemOperand(rbp, WasmInterpreterWrapperConstants::kGCScanSlotCountOffset),
      0);
  // Load the FixedArray of converted reference parameters (or Undefined)
  // from the wrapper buffer into rbx, which WasmInterpreterEntry will
  // forward to Runtime_WasmRunInterpreter.
  Register ref_params_array = rbx;
  __ movq(ref_params_array,
          MemOperand(
              wrapper_buffer,
              WasmInterpreterWrapperConstants::kWrapperBufferRefParamsArray));
  // -------------------------------------------
  // Call the Wasm function.
  // -------------------------------------------
  {
    Register result_size = rax;
    __ movq(result_size, MemOperand(wrapper_buffer,
                                    JSToWasmWrapperFrameConstants::
                                        kWrapperBufferStackReturnBufferSize));
    __ shlq(result_size, Immediate(kSystemPointerSizeLog2));
    __ subq(rsp, result_size);
    __ movq(
        MemOperand(wrapper_buffer, JSToWasmWrapperFrameConstants::
                                       kWrapperBufferStackReturnBufferStart),
        rsp);
  }
  Register result_start = rax;
  __ movq(result_start, rsp);
  __ pushq(params_start);
  __ pushq(wrapper_buffer);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmInterpreterEntry),
          RelocInfo::CODE_TARGET);
  __ popq(wrapper_buffer);
  __ popq(params_start);

  // Reload from the caller's outgoing-arg slot rather than from our own
  // kImplicitArgOffset spill. The caller (Torque JSToWasmInterpreterWrapper)
  // keeps `functionData` alive AND updates it across GC, per
  // WasmJSToWasmWrapperDescriptor's stack-parameter contract. Our own spill
  // at WasmInterpreterWrapperConstants::kImplicitArgOffset is intentionally
  // not visited by JsToWasmFrame::Iterate and therefore must not be read
  // after any safepoint (e.g. the WasmInterpreterEntry call above).
  Register function_data = rax;
  __ movq(function_data,
          MemOperand(rbp, JSToWasmWrapperFrameConstants::kImplicitArgOffset));
  LoadWasmInstanceFromFunctionData(masm, function_data,
                                   /*wasm_instance=*/wasm_instance);
  function_data = no_reg;

  __ movq(
      rax,
      MemOperand(rbp, JSToWasmWrapperFrameConstants::kResultArrayParamOffset));
  __ Move(rbx, wrapper_buffer);
  // rax: result_array
  // rbx: wrapper_buffer
  __ CallBuiltin(Builtin::kJSToWasmInterpreterHandleReturns);
  __ LeaveFrame(StackFrame::JS_TO_WASM);

  __ Ret();
}

// For compiled code, V8 generates signature-specific CWasmEntries that manage
// the transition from C++ code to a JS function called from Wasm code and takes
// care of handling exceptions that arise from JS (see
// WasmWrapperGraphBuilder::BuildCWasmEntry()).
// This builtin does the same for the Wasm interpreter and it is used for
// Wasm-to-JS calls. It invokes GenericWasmToJSInterpreterWrapper and installs a
// specific frame of type C_WASM_ENTRY which is used in
// Isolate::UnwindAndFindHandler() to correctly unwind interpreter stack frames
// and handle exceptions.
void Builtins::Generate_WasmInterpreterCWasmEntry(MacroAssembler* masm) {
  Label invoke, handler_entry, exit;
  Register isolate_root = kCArgRegs[2];  // Windows: r8, Posix: rdx

  // -------------------------------------------
  // CWasmEntryFrame: (Win64)
  // rbp-0xe8  rbx
  // rbp-0xe0  rsi
  // rbp-0xd8  rdi
  // rbp-0xd0  r12
  // rbp-0xc8  r13
  // rbp-0xc0  r14
  // rbp-0xb8  r15
  // -------------------------------------------
  // rbp-0xb0  xmm6
  //           ...
  // rbp-0x20  xmm15
  // -------------------------------------------
  // rbp-0x18  rsp
  // rbp-0x10  CEntryFp
  // rbp-0x08  Marker(StackFrame::C_WASM_ENTRY)
  // rbp       Old RBP

  // -------------------------------------------
  // CWasmEntryFrame: (AMD64 ABI)
  // rbp-0x40  rbx
  // rbp-0x38  r12
  // rbp-0x30  r13
  // rbp-0x28  r14
  // rbp-0x20  r15
  // -------------------------------------------
  // rbp-0x18  rsp
  // rbp-0x10  CEntryFp
  // rbp-0x08  Marker(StackFrame::C_WASM_ENTRY)
  // rbp       Old RBP

#ifdef V8_TARGET_OS_WIN
  // Offsets for arguments passed in WasmToJSCallSig. See declaration of
  // {WasmToJSCallSig} in src/wasm/interpreter/wasm-interpreter-runtime.h.
  constexpr int kCEntryFpParameterOffset = 0x30;
  constexpr int kCallableOffset = 0x38;
#endif  // V8_TARGET_OS_WIN

  // Set up the stackframe.
  __ EnterFrame(StackFrame::C_WASM_ENTRY);

  // Space to store c_entry_fp and current rsp (used by exception handler).
  __ subq(rsp, Immediate(0x10));

  // Save registers
#ifdef V8_TARGET_OS_WIN
  // On Win64 XMM6-XMM15 are callee-save.
  __ subq(rsp, Immediate(0xa0));
  __ movdqu(Operand(rsp, 0x00), xmm6);
  __ movdqu(Operand(rsp, 0x10), xmm7);
  __ movdqu(Operand(rsp, 0x20), xmm8);
  __ movdqu(Operand(rsp, 0x30), xmm9);
  __ movdqu(Operand(rsp, 0x40), xmm10);
  __ movdqu(Operand(rsp, 0x50), xmm11);
  __ movdqu(Operand(rsp, 0x60), xmm12);
  __ movdqu(Operand(rsp, 0x70), xmm13);
  __ movdqu(Operand(rsp, 0x80), xmm14);
  __ movdqu(Operand(rsp, 0x90), xmm15);
#endif  // V8_TARGET_OS_WIN
  __ pushq(r15);
  __ pushq(r14);
  __ pushq(r13);
  __ pushq(r12);
#ifdef V8_TARGET_OS_WIN
  __ pushq(rdi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
  __ pushq(rsi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
#endif            // V8_TARGET_OS_WIN
  __ pushq(rbx);

  // InitializeRootRegister
  __ movq(kRootRegister, isolate_root);  // kRootRegister: r13
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  __ LoadRootRelative(kPtrComprCageBaseRegister,
                      IsolateData::cage_base_offset());
#endif
  isolate_root = no_reg;

  Register callable = r8;
#ifdef V8_TARGET_OS_WIN
  // Store c_entry_fp into slot
  __ movq(rbx, MemOperand(rbp, kCEntryFpParameterOffset));
  __ movq(MemOperand(rbp, WasmInterpreterCWasmEntryConstants::kCEntryFPOffset),
          rbx);
  __ movq(callable, MemOperand(rbp, kCallableOffset));
#else   // !V8_TARGET_OS_WIN
  __ movq(MemOperand(rbp, WasmInterpreterCWasmEntryConstants::kCEntryFPOffset),
          r8);            // saved_c_entry_fp
  __ movq(callable, r9);  // callable
#endif  // V8_TARGET_OS_WIN

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);

  // Handler.
  __ bind(&handler_entry);

  // Store the current pc as the handler offset. It's used later to create the
  // handler table.
  masm->isolate()->builtins()->SetCWasmInterpreterEntryHandlerOffset(
      handler_entry.pos());
  // Caught exception.
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ movq(MemOperand(rbp, WasmInterpreterCWasmEntryConstants::kSPFPOffset),
          rsp);
  __ PushStackHandler();
  __ Call(BUILTIN_CODE(masm->isolate(), GenericWasmToJSInterpreterWrapper),
          RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);
  // Restore registers.
  __ popq(rbx);
#ifdef V8_TARGET_OS_WIN
  __ popq(rsi);
  __ popq(rdi);
#endif  // V8_TARGET_OS_WIN
  __ popq(r12);
  __ popq(r13);
  __ popq(r14);
  __ popq(r15);
#ifdef V8_TARGET_OS_WIN
  // On Win64 XMM6-XMM15 are callee-save.
  __ movdqu(xmm15, Operand(rsp, 0x90));
  __ movdqu(xmm14, Operand(rsp, 0x80));
  __ movdqu(xmm13, Operand(rsp, 0x70));
  __ movdqu(xmm12, Operand(rsp, 0x60));
  __ movdqu(xmm11, Operand(rsp, 0x50));
  __ movdqu(xmm10, Operand(rsp, 0x40));
  __ movdqu(xmm9, Operand(rsp, 0x30));
  __ movdqu(xmm8, Operand(rsp, 0x20));
  __ movdqu(xmm7, Operand(rsp, 0x10));
  __ movdqu(xmm6, Operand(rsp, 0x00));
#endif  // V8_TARGET_OS_WIN

  // Deconstruct the stack frame.
  __ LeaveFrame(StackFrame::C_WASM_ENTRY);
  __ ret(0);
}

void Builtins::Generate_GenericWasmToJSInterpreterWrapper(
    MacroAssembler* masm) {
  Register target_js_function = kCArgRegs[0];  // Win: rcx, Posix: rdi
  Register packed_args = kCArgRegs[1];         // Win: rdx, Posix: rsi
  Register callable = rdi;
  Register signature = kCArgRegs[3];  // Win: r9, Posix: rcx

  // Set up the stackframe.
  __ EnterFrame(StackFrame::WASM_TO_JS);

  // -------------------------------------------
  // Compute offsets and prepare for GC.
  // -------------------------------------------
  // GenericWasmToJSInterpreterWrapperFrame:
  // rbp-N     receiver               ^
  // ...       JS arg 0               | Tagged
  // ...       ...                    | objects
  // rbp-0x68  JS arg n-1             |
  // rbp-0x60  context                v
  // -------------------------------------------
  // rbp-0x58  current_param_slot_index
  // rbp-0x50  valuetypes_array_ptr
  // rbp-0x48  param_index/return_index
  // rbp-0x40  signature
  // rbp-0x38  param_count
  // rbp-0x30  return_count
  // rbp-0x28  expected_arity
  // rbp-0x20  packed_array
  // rbp-0x18  GC_SP
  // rbp-0x10  GCScanSlotLimit
  // rbp-0x08  Marker(StackFrame::WASM_TO_JS)
  // rbp       Old RBP

  constexpr int kMarkerOffset =
      WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset +
      kSystemPointerSize;
  static_assert(WasmToJSInterpreterFrameConstants::kGCSPOffset ==
                WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset -
                    kSystemPointerSize);
  constexpr int kPackedArrayOffset =
      WasmToJSInterpreterFrameConstants::kGCSPOffset - kSystemPointerSize;
  constexpr int kExpectedArityOffset = kPackedArrayOffset - kSystemPointerSize;
  constexpr int kReturnCountOffset = kExpectedArityOffset - kSystemPointerSize;
  constexpr int kParamCountOffset = kReturnCountOffset - kSystemPointerSize;
  constexpr int kSignatureOffset = kParamCountOffset - kSystemPointerSize;
  constexpr int kParamIndexOffset = kSignatureOffset - kSystemPointerSize;
  // Reuse this slot when iterating over return values.
  constexpr int kResultIndexOffset = kParamIndexOffset;
  constexpr int kValueTypesArrayStartOffset =
      kParamIndexOffset - kSystemPointerSize;
  constexpr int kCurrentParamOffset =
      kValueTypesArrayStartOffset - kSystemPointerSize;
  // Reuse this slot when iterating over return values.
  constexpr int kCurrentResultAddressOffset = kCurrentParamOffset;
  constexpr int kNumSpillSlots =
      (kMarkerOffset - kCurrentResultAddressOffset) / kSystemPointerSize;
  __ subq(rsp, Immediate(kNumSpillSlots * kSystemPointerSize));

  __ movq(MemOperand(rbp, kPackedArrayOffset), packed_args);
  __ Move(MemOperand(rbp, WasmToJSInterpreterFrameConstants::kGCSPOffset), 0);

  // Points to the end of the stack frame area that contains tagged objects that
  // need to be visited during GC.
  __ movq(MemOperand(rbp,
                     WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset),
          rsp);

#ifndef V8_TARGET_OS_WIN
  // Windows has a different calling convention.
  signature = r9;
  __ movq(signature, kCArgRegs[3]);
  target_js_function = rcx;
  __ movq(target_js_function, kCArgRegs[0]);
  packed_args = rdx;
  __ movq(packed_args, kCArgRegs[1]);
#endif                    // !V8_TARGET_OS_WIN
  __ movq(callable, r8);  // Callable passed in r8.

  Register shared_function_info = r15;
  __ LoadTaggedField(
      shared_function_info,
      FieldMemOperand(target_js_function,
                      offsetof(JSFunction, shared_function_info_)));

  // Set the context of the function; the call has to run in the function
  // context.
  Register context = rsi;
  __ LoadTaggedField(context, FieldOperand(target_js_function,
                                           offsetof(JSFunction, context_)));
  target_js_function = no_reg;

  // Store context to be retrieved after the call.
  __ pushq(context);

  // Load global receiver if sloppy else use undefined.
  Label receiver_undefined;
  Label calculate_js_function_arity;
  Register receiver = r11;
  Register flags = rbx;
  __ movl(flags, FieldOperand(shared_function_info,
                              offsetof(SharedFunctionInfo, flags_)));
  __ testq(flags, Immediate(SharedFunctionInfo::IsNativeBit::kMask |
                            SharedFunctionInfo::IsStrictBit::kMask));
  flags = no_reg;
  __ j(not_equal, &receiver_undefined);
  __ LoadGlobalProxy(receiver);
  __ jmp(&calculate_js_function_arity);

  __ bind(&receiver_undefined);
  __ LoadRoot(receiver, RootIndex::kUndefinedValue);

  __ bind(&calculate_js_function_arity);

  // Load values from the signature.
  __ movq(MemOperand(rbp, kSignatureOffset), signature);
  Register valuetypes_array_ptr = signature;
  Register return_count = r8;
  Register param_count = rcx;
  LoadFromSignature(masm, valuetypes_array_ptr, return_count, param_count);
  __ movq(MemOperand(rbp, kParamCountOffset), param_count);
  shared_function_info = no_reg;

  // Calculate target function arity.
  Register expected_arity = rbx;
  __ movq(expected_arity, param_count);

  // Make room to pass args and store the context.
  __ movq(rax, expected_arity);
  __ shlq(rax, Immediate(kSystemPointerSizeLog2));
  __ subq(rsp, rax);  // Args.

  Register param_index = param_count;
  __ Move(param_index, 0);

  // -------------------------------------------
  // Store signature-related values to the stack.
  // -------------------------------------------
  // We store values on the stack to restore them after function calls.
  __ movq(MemOperand(rbp, kReturnCountOffset), return_count);
  __ movq(MemOperand(rbp, kValueTypesArrayStartOffset), valuetypes_array_ptr);

  Label prepare_for_js_call;
  __ Cmp(expected_arity, 0);
  // If we have 0 params: jump through parameter handling.
  __ j(equal, &prepare_for_js_call);

  // Loop through the params starting with the first.
  Register current_param_slot_offset = r10;
  __ Move(current_param_slot_offset, Immediate(0));
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

  // -------------------------------------------
  // Copy reference type params first and initialize the stack for JS arguments.
  // -------------------------------------------

  // Heap pointers for ref type values in packed_args can be invalidated if GC
  // is triggered when converting wasm numbers to JS numbers and allocating
  // heap numbers. So, we have to move them to the stack first.
  {
    Label loop_copy_param_ref, load_ref_param, set_and_move;

    __ bind(&loop_copy_param_ref);
    __ movl(valuetype, Operand(valuetypes_array_ptr, 0));
    __ testl(valuetype, Immediate(1));
    __ j(not_equal, &load_ref_param);

    // Initialize non-ref type slots to zero since they can be visited by GC
    // when converting wasm numbers into heap numbers.
    __ Move(param, Smi::zero());

    Label inc_param_32bit;
    __ cmpq(valuetype, Immediate(wasm::kWasmI32.raw_bit_field()));
    __ j(equal, &inc_param_32bit);
    __ cmpq(valuetype, Immediate(wasm::kWasmF32.raw_bit_field()));
    __ j(equal, &inc_param_32bit);

    Label inc_param_64bit;
    __ cmpq(valuetype, Immediate(wasm::kWasmI64.raw_bit_field()));
    __ j(equal, &inc_param_64bit);
    __ cmpq(valuetype, Immediate(wasm::kWasmF64.raw_bit_field()));
    __ j(equal, &inc_param_64bit);

    // Invalid type. Wasm cannot pass Simd arguments to JavaScript.
    __ int3();

    __ bind(&inc_param_32bit);
    __ addq(current_param_slot_offset, Immediate(sizeof(int32_t)));
    __ jmp(&set_and_move);

    __ bind(&inc_param_64bit);
    __ addq(current_param_slot_offset, Immediate(sizeof(int64_t)));
    __ jmp(&set_and_move);

    __ bind(&load_ref_param);
    __ movq(param,
            MemOperand(packed_args, current_param_slot_offset, times_1, 0));
    __ addq(current_param_slot_offset, Immediate(kSystemPointerSize));

    __ bind(&set_and_move);
    __ movq(MemOperand(rsp, param_index, times_system_pointer_size, 0), param);
    __ addq(valuetypes_array_ptr, Immediate(kValueTypeSize));
    __ incq(param_index);
    __ cmpq(param_index, MemOperand(rbp, kParamCountOffset));
    __ j(less, &loop_copy_param_ref);
  }

  // Reset pointers for the second param conversion loop.
  returns_size = r8;
  __ movq(returns_size, MemOperand(rbp, kReturnCountOffset));
  __ shlq(returns_size, Immediate(kValueTypeSizeLog2));
  __ movq(valuetypes_array_ptr, MemOperand(rbp, kValueTypesArrayStartOffset));
  __ addq(valuetypes_array_ptr, returns_size);
  returns_size = no_reg;
  __ movq(current_param_slot_offset, Immediate(0));
  __ movq(param_index, Immediate(0));

  // -------------------------------------------
  // Param evaluation loop.
  // -------------------------------------------
  Label loop_through_params;
  __ bind(&loop_through_params);

  __ movl(valuetype, Operand(valuetypes_array_ptr, 0));

  // -------------------------------------------
  // Param conversion.
  // -------------------------------------------
  // If param is a Smi we can easily convert it. Otherwise we'll call a builtin
  // for conversion.
  Label param_conversion_done, check_ref_param, skip_ref_param, convert_param;

  __ cmpq(valuetype, Immediate(wasm::kWasmI32.raw_bit_field()));
  __ j(not_equal, &check_ref_param);

  // I32 param: change to Smi.
  __ movl(param,
          MemOperand(packed_args, current_param_slot_offset, times_1, 0));
  // If pointer compression is disabled, we can convert to a Smi.
  if (SmiValuesAre32Bits()) {
    __ SmiTag(param);
  } else {
    Register temp = r15;
    __ movq(temp, param);
    // Double the return value to test if it can be a Smi.
    __ addl(temp, param);
    temp = no_reg;
    // If there was overflow, convert the return value to a HeapNumber.
    __ j(overflow, &convert_param);
    // If there was no overflow, we can convert to Smi.
    __ SmiTag(param);
  }

  // Place the param into the proper slot.
  __ movq(MemOperand(rsp, param_index, times_system_pointer_size, 0), param);
  __ addq(current_param_slot_offset, Immediate(sizeof(int32_t)));
  __ jmp(&param_conversion_done);

  // Skip Ref params. We already copied reference params in the first loop.
  __ bind(&check_ref_param);
  __ testl(valuetype, Immediate(1));
  __ j(equal, &convert_param);

  __ bind(&skip_ref_param);
  __ addq(current_param_slot_offset, Immediate(kSystemPointerSize));

  // -------------------------------------------
  // Param conversion done.
  // -------------------------------------------
  __ bind(&param_conversion_done);
  __ addq(valuetypes_array_ptr, Immediate(kValueTypeSize));
  __ incq(param_index);
  __ decq(expected_arity);
  __ j(equal, &prepare_for_js_call);
  __ cmpq(param_index, MemOperand(rbp, kParamCountOffset));
  __ j(not_equal, &loop_through_params);

  // -------------------------------------------
  // Prepare for the function call.
  // -------------------------------------------
  __ bind(&prepare_for_js_call);

  // -------------------------------------------
  // Call the JS function.
  // -------------------------------------------
  // Call_ReceiverIsAny expects the arguments in the stack in this order:
  // rsp + offset_PC  Receiver
  // rsp + 0x10       JS arg 0
  // ...              ...
  // rsp + N          JS arg n-1
  //
  // It also expects two arguments passed in registers:
  // rax: number of arguments + 1 (receiver)
  // rdi: target JSFunction|JSBoundFunction|...
  //
  __ pushq(receiver);

  // The process of calling a JS function might increase the number of tagged
  // values on the stack (arguments adaptation, BuiltinExitFrame arguments,
  // v8::FunctionCallbackInfo implicit arguments, etc.). In any case these
  // additional values must be visited by GC too.
  // We save the current stack pointer to restore it after the call.
  __ movq(MemOperand(rbp, WasmToJSInterpreterFrameConstants::kGCSPOffset), rsp);

  __ movq(rax, MemOperand(rbp, kParamCountOffset));
  __ incq(rax);  // Count receiver.
  __ Call(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);

  __ movq(rsp, MemOperand(rbp, WasmToJSInterpreterFrameConstants::kGCSPOffset));
  __ movq(MemOperand(rbp, WasmToJSInterpreterFrameConstants::kGCSPOffset),
          Immediate(0));

  __ popq(receiver);

  // Retrieve context.
  __ movq(context,  // GC_scan_limit
          MemOperand(
              rbp, WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset));
  __ subq(context, Immediate(kSystemPointerSize));
  __ movq(context, MemOperand(context, 0));
  __ movq(MemOperand(rbp,
                     WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset),
          rsp);

  // -------------------------------------------
  // Return handling.
  // -------------------------------------------
  Register return_reg = rax;
  return_count = rcx;
  __ movq(return_count, MemOperand(rbp, kReturnCountOffset));
  __ movq(packed_args, MemOperand(rbp, kPackedArrayOffset));
  __ movq(signature, MemOperand(rbp, kSignatureOffset));
  __ movq(valuetypes_array_ptr,
          MemOperand(signature, wasm::FunctionSig::kRepsOffset));
  Register result_index = r8;
  __ movq(result_index, Immediate(0));

  // If we have return values, convert them from JS types back to Wasm types.
  Label convert_return;
  Label return_done;
  Label all_done;
  Label loop_copy_return_refs;
  Register fixed_array = r11;
  __ movq(fixed_array, Immediate(0));
  __ cmpl(return_count, Immediate(1));
  __ j(less, &all_done);
  __ j(equal, &convert_return);

  // We have multiple results. Convert the result into a FixedArray.
  // The builtin expects three args:
  // rax: object.
  // rbx: return_count as Smi.
  // rsi: context.
  __ movq(rbx, MemOperand(rbp, kReturnCountOffset));
  __ addq(rbx, rbx);
  __ pushq(return_reg);  // result
  __ pushq(context);

  // We can have a GC here!
  __ Call(BUILTIN_CODE(masm->isolate(), IterableToFixedArrayForWasm),
          RelocInfo::CODE_TARGET);
  __ movq(MemOperand(rbp,
                     WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset),
          rsp);
  __ movq(fixed_array, rax);
  __ popq(context);
  __ popq(return_reg);
  __ movq(return_count, MemOperand(rbp, kReturnCountOffset));
  __ movq(packed_args, MemOperand(rbp, kPackedArrayOffset));
  __ movq(signature, MemOperand(rbp, kSignatureOffset));
  __ movq(valuetypes_array_ptr,
          MemOperand(signature, wasm::FunctionSig::kRepsOffset));
  __ movq(result_index, Immediate(0));

  __ LoadTaggedField(return_reg,
                     FieldOperand(fixed_array, result_index,
                                  static_cast<ScaleFactor>(kTaggedSizeLog2),
                                  OFFSET_OF_DATA_START(FixedArray)));
  __ jmp(&convert_return);

  // A result converted.
  __ bind(&return_done);

  // Restore after builtin call
  __ popq(context);
  __ popq(fixed_array);
  __ movq(valuetypes_array_ptr, MemOperand(rbp, kValueTypesArrayStartOffset));

  __ addq(valuetypes_array_ptr, Immediate(kValueTypeSize));
  __ movq(result_index, MemOperand(rbp, kResultIndexOffset));
  __ incq(result_index);
  __ cmpq(result_index, MemOperand(rbp, kReturnCountOffset));  // return_count
  __ j(greater_equal, &loop_copy_return_refs);

  __ LoadTaggedField(return_reg,
                     FieldOperand(fixed_array, result_index,
                                  static_cast<ScaleFactor>(kTaggedSizeLog2),
                                  OFFSET_OF_DATA_START(FixedArray)));
  __ jmp(&convert_return);

  // -------------------------------------------
  // Update refs after calling all builtins.
  // -------------------------------------------

  // Some builtin calls for return value conversion may trigger GC, and some
  // heap pointers of ref types might become invalid in the conversion loop.
  // Thus, copy the ref values again after finishing all the conversions.
  __ bind(&loop_copy_return_refs);

  // If there is only one return value, there should be no heap pointer in the
  // packed_args while calling any builtin. So, we don't need to update refs.
  __ movq(return_count, MemOperand(rbp, kReturnCountOffset));
  __ cmpl(return_count, Immediate(1));
  __ j(equal, &all_done);

  Label copy_return_if_ref, copy_return_ref, done_copy_return_ref;
  __ movq(packed_args, MemOperand(rbp, kPackedArrayOffset));
  __ movq(signature, MemOperand(rbp, kSignatureOffset));
  __ movq(valuetypes_array_ptr,
          MemOperand(signature, wasm::FunctionSig::kRepsOffset));
  __ movq(result_index, Immediate(0));

  // Copy if the current return value is a ref type.
  __ bind(&copy_return_if_ref);
  __ movl(valuetype, Operand(valuetypes_array_ptr, 0));

  __ testl(valuetype, Immediate(1));
  __ j(not_equal, &copy_return_ref);

  Label inc_result_32bit;
  __ cmpq(valuetype, Immediate(wasm::kWasmI32.raw_bit_field()));
  __ j(equal, &inc_result_32bit);
  __ cmpq(valuetype, Immediate(wasm::kWasmF32.raw_bit_field()));
  __ j(equal, &inc_result_32bit);

  Label inc_result_64bit;
  __ cmpq(valuetype, Immediate(wasm::kWasmI64.raw_bit_field()));
  __ j(equal, &inc_result_64bit);
  __ cmpq(valuetype, Immediate(wasm::kWasmF64.raw_bit_field()));
  __ j(equal, &inc_result_64bit);

  // Invalid type. JavaScript cannot return Simd values to WebAssembly.
  __ int3();

  __ bind(&inc_result_32bit);
  __ addq(packed_args, Immediate(sizeof(int32_t)));
  __ jmp(&done_copy_return_ref);

  __ bind(&inc_result_64bit);
  __ addq(packed_args, Immediate(sizeof(int64_t)));
  __ jmp(&done_copy_return_ref);

  __ bind(&copy_return_ref);
  __ LoadTaggedField(return_reg,
                     FieldOperand(fixed_array, result_index,
                                  static_cast<ScaleFactor>(kTaggedSizeLog2),
                                  OFFSET_OF_DATA_START(FixedArray)));
  __ movq(MemOperand(packed_args, 0), return_reg);
  __ addq(packed_args, Immediate(kSystemPointerSize));

  // Move pointers.
  __ bind(&done_copy_return_ref);
  __ addq(valuetypes_array_ptr, Immediate(kValueTypeSize));
  __ incq(result_index);
  __ cmpq(result_index, MemOperand(rbp, kReturnCountOffset));
  __ j(less, &copy_return_if_ref);

  // -------------------------------------------
  // All done.
  // -------------------------------------------

  __ bind(&all_done);

  // Deconstruct the stack frame.
  __ LeaveFrame(StackFrame::WASM_TO_JS);

  __ movq(rax, Immediate(WasmToJSInterpreterFrameConstants::kSuccess));
  __ ret(0);

  // --------------------------------------------------------------------------
  //                          Deferred code.
  // --------------------------------------------------------------------------

  // -------------------------------------------------
  // Param conversion builtins (Wasm type -> JS type).
  // -------------------------------------------------
  __ bind(&convert_param);

  // Prepare for builtin call.

  // Need to specify how many heap objects, that should be scanned by GC, are
  // on the top of the stack. (Only the context).
  // The builtin expects the parameter to be in register param = rax.

  __ movq(MemOperand(rbp, kExpectedArityOffset), expected_arity);
  __ movq(MemOperand(rbp, kParamIndexOffset), param_index);
  __ movq(MemOperand(rbp, kValueTypesArrayStartOffset), valuetypes_array_ptr);
  __ movq(MemOperand(rbp, kCurrentParamOffset), current_param_slot_offset);

  __ pushq(receiver);
  __ pushq(callable);
  __ pushq(context);

  Label param_kWasmI32_not_smi;
  Label param_kWasmI64;
  Label param_kWasmF32;
  Label param_kWasmF64;
  Label finish_param_conversion;

  __ cmpq(valuetype, Immediate(wasm::kWasmI32.raw_bit_field()));
  __ j(equal, &param_kWasmI32_not_smi);

  __ cmpq(valuetype, Immediate(wasm::kWasmI64.raw_bit_field()));
  __ j(equal, &param_kWasmI64);

  __ cmpq(valuetype, Immediate(wasm::kWasmF32.raw_bit_field()));
  __ j(equal, &param_kWasmF32);

  __ cmpq(valuetype, Immediate(wasm::kWasmF64.raw_bit_field()));
  __ j(equal, &param_kWasmF64);

  // Invalid type. Wasm cannot pass Simd arguments to JavaScript.
  __ int3();

  __ bind(&param_kWasmI32_not_smi);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmInt32ToHeapNumber),
          RelocInfo::CODE_TARGET);
  // Param is the result of the builtin.
  __ movq(rbx, Immediate(sizeof(int32_t)));
  __ jmp(&finish_param_conversion);

  __ bind(&param_kWasmI64);
  __ movq(param,
          MemOperand(packed_args, current_param_slot_offset, times_1, 0));
  __ Call(BUILTIN_CODE(masm->isolate(), I64ToBigInt), RelocInfo::CODE_TARGET);
  __ movq(rbx, Immediate(sizeof(int64_t)));
  __ jmp(&finish_param_conversion);

  __ bind(&param_kWasmF32);
  __ Movss(xmm0,
           MemOperand(packed_args, current_param_slot_offset, times_1, 0));
  __ Call(BUILTIN_CODE(masm->isolate(), WasmFloat32ToNumber),
          RelocInfo::CODE_TARGET);
  __ movq(rbx, Immediate(sizeof(float)));
  __ jmp(&finish_param_conversion);

  __ bind(&param_kWasmF64);
  __ movq(xmm0, MemOperand(packed_args, current_param_slot_offset, times_1, 0));
  __ Call(BUILTIN_CODE(masm->isolate(), WasmFloat64ToNumber),
          RelocInfo::CODE_TARGET);
  __ movq(rbx, Immediate(sizeof(double)));

  // Restore after builtin call.
  __ bind(&finish_param_conversion);
  __ popq(context);
  __ popq(callable);
  __ popq(receiver);

  __ movq(current_param_slot_offset, MemOperand(rbp, kCurrentParamOffset));
  __ addq(current_param_slot_offset, rbx);
  __ movq(valuetypes_array_ptr, MemOperand(rbp, kValueTypesArrayStartOffset));
  __ movq(param_index, MemOperand(rbp, kParamIndexOffset));
  __ movq(expected_arity, MemOperand(rbp, kExpectedArityOffset));
  __ movq(packed_args, MemOperand(rbp, kPackedArrayOffset));

  __ movq(MemOperand(rsp, param_index, times_system_pointer_size, 0), param);
  __ jmp(&param_conversion_done);

  // -------------------------------------------
  // Return conversions (JS type -> Wasm type).
  // -------------------------------------------
  __ bind(&convert_return);

  // Save registers in the stack before the builtin call.
  __ movq(MemOperand(rbp, kResultIndexOffset), result_index);
  __ movq(MemOperand(rbp, kValueTypesArrayStartOffset), valuetypes_array_ptr);
  __ movq(MemOperand(rbp, kCurrentResultAddressOffset), packed_args);

  __ movq(MemOperand(rbp,
                     WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset),
          rsp);
  __ pushq(fixed_array);
  __ pushq(context);

  // The builtin expects the parameter to be in register param = rax.

  // The first valuetype of the array is the return's valuetype.
  __ movl(valuetype, Operand(valuetypes_array_ptr, 0));

  Label return_kWasmI32;
  Label return_kWasmI32_not_smi;
  Label return_kWasmI64;
  Label return_kWasmF32;
  Label return_kWasmF64;
  Label return_kWasmRef;

  // Prepare for builtin call.

  __ cmpq(valuetype, Immediate(wasm::kWasmI32.raw_bit_field()));
  __ j(equal, &return_kWasmI32);

  __ cmpq(valuetype, Immediate(wasm::kWasmI64.raw_bit_field()));
  __ j(equal, &return_kWasmI64);

  __ cmpq(valuetype, Immediate(wasm::kWasmF32.raw_bit_field()));
  __ j(equal, &return_kWasmF32);

  __ cmpq(valuetype, Immediate(wasm::kWasmF64.raw_bit_field()));
  __ j(equal, &return_kWasmF64);

  __ testl(valuetype, Immediate(1));
  __ j(not_equal, &return_kWasmRef);

  // Invalid type. JavaScript cannot return Simd results to WebAssembly.
  __ int3();

  __ bind(&return_kWasmI32);
  __ JumpIfNotSmi(return_reg, &return_kWasmI32_not_smi);
  // Change the param from Smi to int32.
  __ SmiUntag(return_reg);
  // Zero extend.
  __ movl(return_reg, return_reg);
  __ movl(MemOperand(packed_args, 0), return_reg);
  __ addq(packed_args, Immediate(sizeof(int32_t)));
  __ jmp(&return_done);

  __ bind(&return_kWasmI32_not_smi);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedNonSmiToInt32),
          RelocInfo::CODE_TARGET);
  __ AssertZeroExtended(return_reg);
  __ movq(packed_args, MemOperand(rbp, kCurrentResultAddressOffset));
  __ movl(MemOperand(packed_args, 0), return_reg);
  __ addq(packed_args, Immediate(sizeof(int32_t)));
  __ jmp(&return_done);

  __ bind(&return_kWasmI64);
  __ Call(BUILTIN_CODE(masm->isolate(), BigIntToI64), RelocInfo::CODE_TARGET);
  __ movq(packed_args, MemOperand(rbp, kCurrentResultAddressOffset));
  __ movq(MemOperand(packed_args, 0), return_reg);
  __ addq(packed_args, Immediate(sizeof(int64_t)));
  __ jmp(&return_done);

  __ bind(&return_kWasmF32);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedToFloat32),
          RelocInfo::CODE_TARGET);
  __ movq(packed_args, MemOperand(rbp, kCurrentResultAddressOffset));
  __ Movss(MemOperand(packed_args, 0), xmm0);
  __ addq(packed_args, Immediate(sizeof(float)));
  __ jmp(&return_done);

  __ bind(&return_kWasmF64);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedToFloat64),
          RelocInfo::CODE_TARGET);
  __ movq(packed_args, MemOperand(rbp, kCurrentResultAddressOffset));
  __ Movsd(MemOperand(packed_args, 0), xmm0);
  __ addq(packed_args, Immediate(sizeof(double)));
  __ jmp(&return_done);

  __ bind(&return_kWasmRef);
  __ movq(MemOperand(packed_args, 0), return_reg);
  __ addq(packed_args, Immediate(kSystemPointerSize));
  __ jmp(&return_done);
}

#ifndef V8_DRUMBRAKE_BOUNDS_CHECKS

namespace {

enum IntMemoryType {
  kIntS8,
  kIntU8,
  kIntS16,
  kIntU16,
  kIntS32,
  kIntU32,
  kInt64
};

enum IntValueType { kValueInt32, kValueInt64 };

enum FloatType { kFloat32, kFloat64 };

#ifdef V8_ENABLE_DRUMBRAKE_TRACING

using TracePopType = wasm::WasmInterpreterRuntime::TracePopType;
using TracePushType = wasm::WasmInterpreterRuntime::TracePushType;

template <TracePopType type>
void EmitTracePop(MacroAssembler* masm, Register wasm_runtime) {
  FrameScope scope(masm, StackFrame::MANUAL);
  __ PushCallerSaved(SaveFPRegsMode::kSave, kReturnRegister0);

  Register trace_pop = kReturnRegister0;
  int offset = type == TracePopType::kDuplicate
                   ? wasm::WasmInterpreterRuntime::trace_pop2_func_offset()
                   : wasm::WasmInterpreterRuntime::trace_pop_func_offset();
  __ movq(trace_pop, MemOperand(wasm_runtime, offset));

  DCHECK_NE(wasm_runtime, kReturnRegister0);
  __ Move(kCArgRegs[0], wasm_runtime);
  __ PrepareCallCFunction(1);
  __ CallCFunction(trace_pop, 1, SetIsolateDataSlots::kNo);

  __ PopCallerSaved(SaveFPRegsMode::kSave, kReturnRegister0);
}

template <TracePushType type>
void EmitTracePush(MacroAssembler* masm, Register wasm_runtime,
                   wasm::ValueKind kind, Register slot_offset) {
  FrameScope scope(masm, StackFrame::MANUAL);
  __ PushCallerSaved(SaveFPRegsMode::kSave, kReturnRegister0);

  Register trace_push = kReturnRegister0;
  int offset = type == TracePushType::kReplace
                   ? wasm::WasmInterpreterRuntime::trace_replace_func_offset()
                   : wasm::WasmInterpreterRuntime::trace_push_func_offset();
  __ movq(trace_push, MemOperand(wasm_runtime, offset));

  DCHECK_NE(wasm_runtime, kReturnRegister0);
  __ Move(kCArgRegs[0], wasm_runtime);
  __ Move(kCArgRegs[1], Immediate(kind));
  DCHECK_NE(slot_offset, kReturnRegister0);
  DCHECK_NE(slot_offset, kCArgRegs[0]);
  DCHECK_NE(slot_offset, kCArgRegs[1]);
  __ Move(kCArgRegs[2], slot_offset);
  __ shlq(kCArgRegs[2], Immediate(log2(wasm::kSlotSize)));
  __ PrepareCallCFunction(3);
  __ CallCFunction(trace_push, 3, SetIsolateDataSlots::kNo);

  __ PopCallerSaved(SaveFPRegsMode::kSave, kReturnRegister0);
}
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

void EmitLoadInstruction(MacroAssembler* masm, Register result,
                         Register memory_start, Register memory_index,
                         IntValueType value_type, IntMemoryType memory_type) {
  switch (memory_type) {
    case kInt64:
      switch (value_type) {
        case kValueInt64:
          __ movq(result, Operand(memory_start, memory_index, times_1, 0));
          break;
        default:
          UNREACHABLE();
      }
      break;
    case kIntS32:
      switch (value_type) {
        case kValueInt64:
          __ movsxlq(result, Operand(memory_start, memory_index, times_1, 0));
          break;
        case kValueInt32:
          __ movl(result, Operand(memory_start, memory_index, times_1, 0));
          break;
      }
      break;
    case kIntU32:
      switch (value_type) {
        case kValueInt64:
          __ movl(result, Operand(memory_start, memory_index, times_1, 0));
          break;
        default:
          UNREACHABLE();
      }
      break;
    case kIntS16:
      switch (value_type) {
        case kValueInt64:
          __ movsxwq(result, Operand(memory_start, memory_index, times_1, 0));
          break;
        case kValueInt32:
          __ movsxwl(result, Operand(memory_start, memory_index, times_1, 0));
          break;
      }
      break;
    case kIntU16:
      switch (value_type) {
        case kValueInt64:
          __ movzxwq(result, Operand(memory_start, memory_index, times_1, 0));
          break;
        case kValueInt32:
          __ movzxwl(result, Operand(memory_start, memory_index, times_1, 0));
          break;
      }
      break;
    case kIntS8:
      switch (value_type) {
        case kValueInt64:
          __ movsxbq(result, Operand(memory_start, memory_index, times_1, 0));
          break;
        case kValueInt32:
          __ movsxbl(result, Operand(memory_start, memory_index, times_1, 0));
          break;
      }
      break;
    case kIntU8:
      switch (value_type) {
        case kValueInt64:
          __ movzxbq(result, Operand(memory_start, memory_index, times_1, 0));
          break;
        case kValueInt32:
          __ movzxbl(result, Operand(memory_start, memory_index, times_1, 0));
          break;
      }
      break;
    default:
      UNREACHABLE();
  }
}

void EmitLoadInstruction(MacroAssembler* masm, Register memory_start,
                         Register memory_offset, XMMRegister result,
                         FloatType float_type) {
  switch (float_type) {
    case kFloat32:
      __ movss(xmm0, Operand(memory_start, memory_offset, times_1, 0));
      __ cvtss2sd(result, xmm0);
      break;
    case kFloat64:
      __ movsd(result, Operand(memory_start, memory_offset, times_1, 0));
      break;
    default:
      UNREACHABLE();
  }
}

void EmitLoadInstruction(MacroAssembler* masm, Register memory_start,
                         Register memory_offset, Register sp,
                         Register slot_offset, FloatType float_type) {
  switch (float_type) {
    case kFloat32:
      __ movss(xmm0, Operand(memory_start, memory_offset, times_1, 0));
      __ movss(Operand(sp, slot_offset, times_4, 0), xmm0);
      break;
    case kFloat64:
      __ movsd(xmm0, Operand(memory_start, memory_offset, times_1, 0));
      __ movsd(Operand(sp, slot_offset, times_4, 0), xmm0);
      break;
    default:
      UNREACHABLE();
  }
}

void WriteToSlot(MacroAssembler* masm, Register sp, Register slot_offset,
                 Register value, IntValueType value_type) {
  switch (value_type) {
    case kValueInt64:
      __ movq(Operand(sp, slot_offset, times_4, 0), value);
      break;
    case kValueInt32:
      __ movl(Operand(sp, slot_offset, times_4, 0), value);
      break;
  }
}

void EmitStoreInstruction(MacroAssembler* masm, Register value,
                          Register memory_start, Register memory_index,
                          IntMemoryType memory_type) {
  switch (memory_type) {
    case kInt64:
      __ movq(Operand(memory_start, memory_index, times_1, 0), value);
      break;
    case kIntS32:
      __ movl(Operand(memory_start, memory_index, times_1, 0), value);
      break;
    case kIntS16:
      __ movw(Operand(memory_start, memory_index, times_1, 0), value);
      break;
    case kIntS8:
      __ movb(Operand(memory_start, memory_index, times_1, 0), value);
      break;
    default:
      UNREACHABLE();
  }
}

void EmitStoreInstruction(MacroAssembler* masm, XMMRegister value,
                          Register memory_start, Register memory_index,
                          FloatType float_type) {
  switch (float_type) {
    case kFloat32:
      __ movss(Operand(memory_start, memory_index, times_1, 0), value);
      break;
    case kFloat64:
      __ movsd(Operand(memory_start, memory_index, times_1, 0), value);
      break;
    default:
      UNREACHABLE();
  }
}

void EmitLoadNextInstructionId(MacroAssembler* masm, Register next_handler_id,
                               Register code, uint32_t code_offset) {
  // An InstructionHandler id is stored in the WasmBytecode as a uint16_t, so we
  // need to move a 16-bit word here.
  __ movzxwq(next_handler_id, MemOperand(code, code_offset));

  // Currently, there cannot be more than kInstructionTableSize = 1024 different
  // handlers, so (for additional security) we do a bitwise AND with 1023 to
  // make sure some attacker might somehow generate invalid WasmBytecode data
  // and force an indirect jump through memory outside the handler table.
  __ andq(next_handler_id, Immediate(wasm::kInstructionTableMask));
}

template <bool Compressed>
class WasmInterpreterHandlerCodeEmitter {
 public:
  static void EmitLoadSlotOffset(MacroAssembler* masm, Register slot_offset,
                                 const MemOperand& operand);
  static void EmitLoadMemoryOffset(MacroAssembler* masm, Register memory_offset,
                                   const MemOperand& operand);
};

template <>
void WasmInterpreterHandlerCodeEmitter<true>::EmitLoadSlotOffset(
    MacroAssembler* masm, Register slot_offset, const MemOperand& operand) {
  __ movzxwq(slot_offset, operand);
}
template <>
void WasmInterpreterHandlerCodeEmitter<true>::EmitLoadMemoryOffset(
    MacroAssembler* masm, Register memory_offset, const MemOperand& operand) {
  __ movl(memory_offset, operand);
}

template <>
void WasmInterpreterHandlerCodeEmitter<false>::EmitLoadSlotOffset(
    MacroAssembler* masm, Register slot_offset, const MemOperand& operand) {
  __ movl(slot_offset, operand);
}
template <>
void WasmInterpreterHandlerCodeEmitter<false>::EmitLoadMemoryOffset(
    MacroAssembler* masm, Register memory_offset, const MemOperand& operand) {
  __ movq(memory_offset, operand);
}

template <bool Compressed>
class WasmInterpreterHandlerBuiltins {
  using slot_offset_t = wasm::handler_traits<Compressed>::slot_offset_t;
  using memory_offset32_t = wasm::handler_traits<Compressed>::memory_offset32_t;
  using handler_id_t = wasm::handler_traits<Compressed>::handler_id_t;
  using emitter = WasmInterpreterHandlerCodeEmitter<Compressed>;

 public:
  static void Generate_r2r_ILoadMem(MacroAssembler* masm,
                                    IntValueType value_type,
                                    IntMemoryType memory_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kNextHandlerId =
        kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register wasm_runtime = r8;
    Register memory_index = r9;

    __ movl(memory_index, memory_index);

    Register memory_start_plus_index = memory_index;
    __ addq(memory_start_plus_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_offset = rax;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register result = r9;
    EmitLoadInstruction(masm, result, memory_start_plus_index, memory_offset,
                        value_type, memory_type);

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_r2r_FLoadMem(MacroAssembler* masm,
                                    FloatType float_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kNextHandlerId =
        kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register wasm_runtime = r8;
    Register memory_index = r9;

    __ movl(memory_index, memory_index);

    Register memory_start_plus_index = memory_index;
    __ addq(memory_start_plus_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_offset = rax;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    EmitLoadInstruction(masm, memory_start_plus_index, memory_offset, xmm4,
                        float_type);

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_r2s_ILoadMem(MacroAssembler* masm,
                                    IntValueType value_type,
                                    IntMemoryType memory_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kSlotOffset = kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId = kSlotOffset + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register sp = rdx;
    Register wasm_runtime = r8;
    Register memory_index = r9;

    __ movl(memory_index, memory_index);

    Register memory_start_plus_index = memory_index;
    __ addq(memory_start_plus_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_offset = rax;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register value = r10;
    EmitLoadInstruction(masm, value, memory_start_plus_index, memory_offset,
                        value_type, memory_type);

    Register slot_offset = r11;
    emitter::EmitLoadSlotOffset(masm, slot_offset,
                                MemOperand(code, kSlotOffset));

    WriteToSlot(masm, sp, slot_offset, value, value_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePush<TracePushType::kNormal>(masm, wasm_runtime,
                                          value_type == kValueInt64
                                              ? wasm::ValueKind::kI64
                                              : wasm::ValueKind::kI32,
                                          slot_offset);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_r2s_FLoadMem(MacroAssembler* masm,
                                    FloatType float_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kSlotOffset = kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId = kSlotOffset + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register sp = rdx;
    Register wasm_runtime = r8;
    Register memory_index = r9;

    __ movl(memory_index, memory_index);

    Register memory_start_plus_index = memory_index;
    __ addq(memory_start_plus_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_offset = rax;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register slot_offset = r11;
    emitter::EmitLoadSlotOffset(masm, slot_offset,
                                MemOperand(code, kSlotOffset));

    EmitLoadInstruction(masm, memory_start_plus_index, memory_offset, sp,
                        slot_offset, float_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePush<TracePushType::kNormal>(
        masm, wasm_runtime,
        float_type == kFloat64 ? wasm::ValueKind::kF64 : wasm::ValueKind::kF32,
        slot_offset);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_s2r_ILoadMem(MacroAssembler* masm,
                                    IntValueType value_type,
                                    IntMemoryType memory_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kMemoryIndexSlot =
        kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId =
        kMemoryIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register sp = rdx;
    Register wasm_runtime = r8;

    Register memory_offset = r10;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register memory_index_slot_offset = rax;
    emitter::EmitLoadSlotOffset(masm, memory_index_slot_offset,
                                MemOperand(code, kMemoryIndexSlot));

    Register memory_start_plus_offset = memory_offset;
    __ addq(memory_start_plus_offset,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_index = memory_index_slot_offset;
    __ movl(memory_index, Operand(sp, memory_index_slot_offset, times_4, 0));

    Register value = r9;
    EmitLoadInstruction(masm, value, memory_start_plus_offset, memory_index,
                        value_type, memory_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kNormal>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_s2r_FLoadMem(MacroAssembler* masm,
                                    FloatType float_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kMemoryIndexSlot =
        kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId =
        kMemoryIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register sp = rdx;
    Register wasm_runtime = r8;

    Register memory_offset = r10;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register memory_index_slot_offset = rax;
    emitter::EmitLoadSlotOffset(masm, memory_index_slot_offset,
                                MemOperand(code, kMemoryIndexSlot));

    Register memory_start_plus_offset = memory_offset;
    __ addq(memory_start_plus_offset,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_index = memory_index_slot_offset;
    __ movl(memory_index, Operand(sp, memory_index_slot_offset, times_4, 0));

    EmitLoadInstruction(masm, memory_start_plus_offset, memory_index, xmm4,
                        float_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kNormal>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_s2s_ILoadMem(MacroAssembler* masm,
                                    IntValueType value_type,
                                    IntMemoryType memory_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kIndexSlot = kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kResultSlot = kIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kNextHandlerId = kResultSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register sp = rdx;
    Register wasm_runtime = r8;

    Register memory_offset = r10;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register memory_index_slot_offset = rax;
    emitter::EmitLoadSlotOffset(masm, memory_index_slot_offset,
                                MemOperand(code, kIndexSlot));

    Register memory_start_plus_offset = memory_offset;
    __ addq(memory_start_plus_offset,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_index = r9;
    __ movl(memory_index, Operand(sp, memory_index_slot_offset, times_4, 0));

    Register result_slot_offset = r11;
    emitter::EmitLoadSlotOffset(masm, result_slot_offset,
                                MemOperand(code, kResultSlot));

    Register value = rax;
    EmitLoadInstruction(masm, value, memory_start_plus_offset, memory_index,
                        value_type, memory_type);

    WriteToSlot(masm, sp, result_slot_offset, value, value_type);

#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePush<TracePushType::kReplace>(masm, wasm_runtime,
                                           value_type == kValueInt64
                                               ? wasm::ValueKind::kI64
                                               : wasm::ValueKind::kI32,
                                           result_slot_offset);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_s2s_FLoadMem(MacroAssembler* masm,
                                    FloatType float_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kIndexSlot = kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kResultSlot = kIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kNextHandlerId = kResultSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register sp = rdx;
    Register wasm_runtime = r8;

    Register memory_offset = r10;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register memory_index_slot_offset = rax;
    emitter::EmitLoadSlotOffset(masm, memory_index_slot_offset,
                                MemOperand(code, kIndexSlot));

    Register memory_start_plus_offset = memory_offset;
    __ addq(memory_start_plus_offset,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_index = r9;
    __ movl(memory_index, Operand(sp, memory_index_slot_offset, times_4, 0));

    Register result_slot_offset = r11;
    emitter::EmitLoadSlotOffset(masm, result_slot_offset,
                                MemOperand(code, kResultSlot));

    EmitLoadInstruction(masm, memory_start_plus_offset, memory_index, sp,
                        result_slot_offset, float_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePush<TracePushType::kReplace>(
        masm, wasm_runtime,
        float_type == kFloat64 ? wasm::ValueKind::kF64 : wasm::ValueKind::kF32,
        result_slot_offset);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_s2s_ILoadMem_LocalSet(MacroAssembler* masm,
                                             IntValueType value_type,
                                             IntMemoryType memory_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kIndexSlot = kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kSetSlot = kIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kNextHandlerId = kSetSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register sp = rdx;
    Register wasm_runtime = r8;

    Register memory_offset = r10;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register memory_index_slot_offset = rax;
    emitter::EmitLoadSlotOffset(masm, memory_index_slot_offset,
                                MemOperand(code, kIndexSlot));

    Register memory_start_plus_offset = memory_offset;
    __ addq(memory_start_plus_offset,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_index = memory_index_slot_offset;
    __ movl(memory_index, Operand(sp, memory_index_slot_offset, times_4, 0));

    Register set_slot_offset = r11;
    emitter::EmitLoadSlotOffset(masm, set_slot_offset,
                                MemOperand(code, kSetSlot));

    Register value = rax;
    EmitLoadInstruction(masm, value, memory_start_plus_offset, memory_index,
                        value_type, memory_type);

    WriteToSlot(masm, sp, set_slot_offset, value, value_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kNormal>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_s2s_FLoadMem_LocalSet(MacroAssembler* masm,
                                             FloatType float_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kIndexSlot = kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kSetSlot = kIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kNextHandlerId = kSetSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register sp = rdx;
    Register wasm_runtime = r8;

    Register memory_offset = r10;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register memory_index_slot_offset = rax;
    emitter::EmitLoadSlotOffset(masm, memory_index_slot_offset,
                                MemOperand(code, kIndexSlot));

    Register memory_start_plus_offset = memory_offset;
    __ addq(memory_start_plus_offset,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_index = memory_index_slot_offset;
    __ movl(memory_index, Operand(sp, memory_index_slot_offset, times_4, 0));

    Register set_slot_offset = r11;
    emitter::EmitLoadSlotOffset(masm, set_slot_offset,
                                MemOperand(code, kSetSlot));

    EmitLoadInstruction(masm, memory_start_plus_offset, memory_index, sp,
                        set_slot_offset, float_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kNormal>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_r2s_IStoreMem(MacroAssembler* masm,
                                     IntValueType /*value_type*/,
                                     IntMemoryType memory_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kMemoryIndexSlot =
        kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId =
        kMemoryIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register sp = rdx;
    Register wasm_runtime = r8;
    Register value = r9;

    Register memory_offset = r10;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register memory_index_slot_offset = r11;
    emitter::EmitLoadSlotOffset(masm, memory_index_slot_offset,
                                MemOperand(code, kMemoryIndexSlot));

    Register memory_start_plus_offset = memory_offset;
    __ addq(memory_start_plus_offset,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_index = memory_index_slot_offset;
    __ movl(memory_index, Operand(sp, memory_index_slot_offset, times_4, 0));

    EmitStoreInstruction(masm, value, memory_start_plus_offset, memory_index,
                         memory_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kNormal>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_r2s_FStoreMem(MacroAssembler* masm,
                                     FloatType float_type) {
    constexpr uint32_t kMemoryOffset = 0;
    constexpr uint32_t kMemoryIndexSlot =
        kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId =
        kMemoryIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register code = rcx;
    Register sp = rdx;
    Register wasm_runtime = r8;

    XMMRegister value = xmm4;
    if (float_type == kFloat32) {
      __ cvtsd2ss(value, xmm4);
    }

    Register memory_offset = r10;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register memory_index_slot_offset = r11;
    emitter::EmitLoadSlotOffset(masm, memory_index_slot_offset,
                                MemOperand(code, kMemoryIndexSlot));

    Register memory_start_plus_offset = memory_offset;
    __ addq(memory_start_plus_offset,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_index = memory_index_slot_offset;
    __ movl(memory_index, Operand(sp, memory_index_slot_offset, times_4, 0));

    EmitStoreInstruction(masm, value, memory_start_plus_offset, memory_index,
                         float_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kNormal>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_s2s_IStoreMem(MacroAssembler* masm,
                                     IntValueType /*value_type*/,
                                     IntMemoryType memory_type) {
    constexpr uint32_t kValueSlot = 0;
    constexpr uint32_t kMemoryOffset = kValueSlot + sizeof(slot_offset_t);
    constexpr uint32_t kMemoryIndexSlot =
        kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId =
        kMemoryIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register sp = rdx;
    Register code = rcx;
    Register wasm_runtime = r8;

    Register value_slot_offset = rax;
    emitter::EmitLoadSlotOffset(masm, value_slot_offset,
                                MemOperand(code, kValueSlot));

    Register value = value_slot_offset;
    switch (memory_type) {
      case kInt64:
        __ movq(value, MemOperand(sp, value_slot_offset, times_4, 0));
        break;
      case kIntS32:
        __ movl(value, MemOperand(sp, value_slot_offset, times_4, 0));
        break;
      case kIntS16:
        __ movw(value, MemOperand(sp, value_slot_offset, times_4, 0));
        break;
      case kIntS8:
        __ movb(value, MemOperand(sp, value_slot_offset, times_4, 0));
        break;
      default:
        UNREACHABLE();
    }

    Register memory_offset = r10;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register memory_index_slot_offset = r11;
    emitter::EmitLoadSlotOffset(masm, memory_index_slot_offset,
                                MemOperand(code, kMemoryIndexSlot));

    Register memory_start_plus_offset = memory_offset;
    __ addq(memory_start_plus_offset,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_index = memory_index_slot_offset;
    __ movl(memory_index, Operand(sp, memory_index_slot_offset, times_4, 0));

    EmitStoreInstruction(masm, value, memory_start_plus_offset, memory_index,
                         memory_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kDuplicate>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = rax;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = r9;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_s2s_FStoreMem(MacroAssembler* masm,
                                     FloatType float_type) {
    constexpr uint32_t kValueSlot = 0;
    constexpr uint32_t kMemoryOffset = kValueSlot + sizeof(slot_offset_t);
    constexpr uint32_t kMemoryIndexSlot =
        kMemoryOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId =
        kMemoryIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register sp = rdx;
    Register code = rcx;
    Register wasm_runtime = r8;

    Register value_slot_offset = rax;
    emitter::EmitLoadSlotOffset(masm, value_slot_offset,
                                MemOperand(code, kValueSlot));

    XMMRegister value = xmm0;
    switch (float_type) {
      case kFloat32:
        __ movss(value, MemOperand(sp, value_slot_offset, times_4, 0));
        break;
      case kFloat64:
        __ movsd(value, MemOperand(sp, value_slot_offset, times_4, 0));
        break;
      default:
        UNREACHABLE();
    }

    Register memory_offset = r10;
    emitter::EmitLoadMemoryOffset(masm, memory_offset,
                                  MemOperand(code, kMemoryOffset));

    Register memory_index_slot_offset = r11;
    emitter::EmitLoadSlotOffset(masm, memory_index_slot_offset,
                                MemOperand(code, kMemoryIndexSlot));

    Register memory_start_plus_offset = memory_offset;
    __ addq(memory_start_plus_offset,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register memory_index = memory_index_slot_offset;
    __ movl(memory_index, Operand(sp, memory_index_slot_offset, times_4, 0));

    EmitStoreInstruction(masm, value, memory_start_plus_offset, memory_index,
                         float_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kDuplicate>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = r10;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = rax;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_r2s_ILoadStoreMem(MacroAssembler* masm,
                                         IntValueType value_type,
                                         IntMemoryType memory_type) {
    constexpr uint32_t kLoadOffset = 0;
    constexpr uint32_t kStoreOffset = kLoadOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kStoreIndexSlot =
        kStoreOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId = kStoreIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register sp = rdx;
    Register code = rcx;
    Register wasm_runtime = r8;
    Register load_index = r9;

    __ movl(load_index, load_index);

    Register memory_start_plus_load_index = load_index;
    __ addq(memory_start_plus_load_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register load_offset = rax;
    emitter::EmitLoadMemoryOffset(masm, load_offset,
                                  MemOperand(code, kLoadOffset));

    Register value = r10;
    EmitLoadInstruction(masm, value, memory_start_plus_load_index, load_offset,
                        value_type, memory_type);

    Register store_index_slot_offset = r9;
    emitter::EmitLoadSlotOffset(masm, store_index_slot_offset,
                                MemOperand(code, kStoreIndexSlot));

    Register store_index = store_index_slot_offset;
    __ movl(store_index, MemOperand(sp, store_index_slot_offset, times_4, 0));

    Register store_offset = r11;
    emitter::EmitLoadMemoryOffset(masm, store_offset,
                                  MemOperand(code, kStoreOffset));

    Register memory_start_plus_store_index = store_index;
    __ addq(memory_start_plus_store_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    EmitStoreInstruction(masm, value, memory_start_plus_store_index,
                         store_offset, memory_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kNormal>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = rax;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = r9;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_s2s_ILoadStoreMem(MacroAssembler* masm,
                                         IntValueType value_type,
                                         IntMemoryType memory_type) {
    constexpr uint32_t kLoadOffset = 0;
    constexpr uint32_t kLoadIndexSlot = kLoadOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kStoreOffset = kLoadIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kStoreIndexSlot =
        kStoreOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId = kStoreIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register sp = rdx;
    Register code = rcx;
    Register wasm_runtime = r8;

    Register load_index_slot_offset = r9;
    emitter::EmitLoadSlotOffset(masm, load_index_slot_offset,
                                MemOperand(code, kLoadIndexSlot));

    Register load_index = load_index_slot_offset;
    __ movl(load_index, Operand(sp, load_index_slot_offset, times_4, 0));

    Register memory_start_plus_load_index = load_index;
    __ addq(memory_start_plus_load_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register load_offset = rax;
    emitter::EmitLoadMemoryOffset(masm, load_offset,
                                  MemOperand(code, kLoadOffset));

    Register value = r10;
    EmitLoadInstruction(masm, value, memory_start_plus_load_index, load_offset,
                        value_type, memory_type);

    Register store_index_slot_offset = r9;
    emitter::EmitLoadSlotOffset(masm, store_index_slot_offset,
                                MemOperand(code, kStoreIndexSlot));

    Register store_index = store_index_slot_offset;
    __ movl(store_index, MemOperand(sp, store_index_slot_offset, times_4, 0));

    Register store_offset = r11;
    emitter::EmitLoadMemoryOffset(masm, store_offset,
                                  MemOperand(code, kStoreOffset));

    Register memory_start_plus_store_index = store_index;
    __ addq(memory_start_plus_store_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    EmitStoreInstruction(masm, value, memory_start_plus_store_index,
                         store_offset, memory_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kDuplicate>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = rax;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = r9;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_r2s_FLoadStoreMem(MacroAssembler* masm,
                                         FloatType float_type) {
    constexpr uint32_t kLoadOffset = 0;
    constexpr uint32_t kStoreOffset = kLoadOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kStoreIndexSlot =
        kStoreOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId = kStoreIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register sp = rdx;
    Register code = rcx;
    Register wasm_runtime = r8;
    Register load_index = r9;

    __ movl(load_index, load_index);

    Register memory_start_plus_load_index = load_index;
    __ addq(memory_start_plus_load_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    Register load_offset = rax;
    emitter::EmitLoadMemoryOffset(masm, load_offset,
                                  MemOperand(code, kLoadOffset));

    XMMRegister value = xmm0;
    switch (float_type) {
      case kFloat32:
        __ movss(value, Operand(memory_start_plus_load_index, load_offset,
                                times_1, 0));
        break;
      case kFloat64:
        __ movsd(value, Operand(memory_start_plus_load_index, load_offset,
                                times_1, 0));
        break;
      default:
        UNREACHABLE();
    }

    Register store_index_slot_offset = r9;
    emitter::EmitLoadSlotOffset(masm, store_index_slot_offset,
                                MemOperand(code, kStoreIndexSlot));

    Register store_index = store_index_slot_offset;
    __ movl(store_index, MemOperand(sp, store_index_slot_offset, times_4, 0));

    Register store_offset = r11;
    emitter::EmitLoadMemoryOffset(masm, store_offset,
                                  MemOperand(code, kStoreOffset));

    Register memory_start_plus_store_index = store_index;
    __ addq(memory_start_plus_store_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    EmitStoreInstruction(masm, value, memory_start_plus_store_index,
                         store_offset, float_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kNormal>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = rax;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = r9;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }

  static void Generate_s2s_FLoadStoreMem(MacroAssembler* masm,
                                         FloatType float_type) {
    constexpr uint32_t kLoadOffset = 0;
    constexpr uint32_t kLoadIndexSlot = kLoadOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kStoreOffset = kLoadIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kStoreIndexSlot =
        kStoreOffset + sizeof(memory_offset32_t);
    constexpr uint32_t kNextHandlerId = kStoreIndexSlot + sizeof(slot_offset_t);
    constexpr uint32_t kInstructionCodeLength =
        kNextHandlerId + sizeof(handler_id_t);

    Register sp = rdx;
    Register code = rcx;
    Register wasm_runtime = r8;

    Register load_index_slot_offset = r9;
    emitter::EmitLoadSlotOffset(masm, load_index_slot_offset,
                                MemOperand(code, kLoadIndexSlot));

    Register load_index = load_index_slot_offset;
    __ movl(load_index, Operand(sp, load_index_slot_offset, times_4, 0));

    Register memory_start_plus_load_index = load_index;
    __ addq(memory_start_plus_load_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));
    Register load_offset = rax;
    emitter::EmitLoadMemoryOffset(masm, load_offset,
                                  MemOperand(code, kLoadOffset));

    XMMRegister value = xmm0;
    switch (float_type) {
      case kFloat32:
        __ movss(value, Operand(memory_start_plus_load_index, load_offset,
                                times_1, 0));
        break;
      case kFloat64:
        __ movsd(value, Operand(memory_start_plus_load_index, load_offset,
                                times_1, 0));
        break;
      default:
        UNREACHABLE();
    }

    Register store_index_slot_offset = r9;
    emitter::EmitLoadSlotOffset(masm, store_index_slot_offset,
                                MemOperand(code, kStoreIndexSlot));

    Register store_index = store_index_slot_offset;
    __ movl(store_index, MemOperand(sp, store_index_slot_offset, times_4, 0));

    Register store_offset = r11;
    emitter::EmitLoadMemoryOffset(masm, store_offset,
                                  MemOperand(code, kStoreOffset));

    Register memory_start_plus_store_index = store_index;
    __ addq(memory_start_plus_store_index,
            MemOperand(wasm_runtime,
                       wasm::WasmInterpreterRuntime::memory_start_offset()));

    EmitStoreInstruction(masm, value, memory_start_plus_store_index,
                         store_offset, float_type);
#ifdef V8_ENABLE_DRUMBRAKE_TRACING
    EmitTracePop<TracePopType::kDuplicate>(masm, wasm_runtime);
#endif  // V8_ENABLE_DRUMBRAKE_TRACING

    Register next_handler_id = rax;
    EmitLoadNextInstructionId(masm, next_handler_id, code, kNextHandlerId);
    __ addq(code, Immediate(kInstructionCodeLength));

    Register instr_table = r9;
    __ movq(
        instr_table,
        MemOperand(wasm_runtime,
                   wasm::WasmInterpreterRuntime::instruction_table_offset()));

    Register next_handler_addr = rax;
    __ movq(next_handler_addr,
            MemOperand(instr_table, next_handler_id, times_8, 0));
    __ jmp(next_handler_addr);
  }
};  // class WasmInterpreterHandlerBuiltins<Compressed>

}  // namespace

#define FOREACH_INT_LOADSTORE_BUILTIN(V)                                     \
  V(r2r_I32LoadMem8S, Generate_r2r_ILoadMem, kValueInt32, kIntS8)            \
  V(r2r_I32LoadMem8U, Generate_r2r_ILoadMem, kValueInt32, kIntU8)            \
  V(r2r_I32LoadMem16S, Generate_r2r_ILoadMem, kValueInt32, kIntS16)          \
  V(r2r_I32LoadMem16U, Generate_r2r_ILoadMem, kValueInt32, kIntU16)          \
  V(r2r_I64LoadMem8S, Generate_r2r_ILoadMem, kValueInt64, kIntS8)            \
  V(r2r_I64LoadMem8U, Generate_r2r_ILoadMem, kValueInt64, kIntU8)            \
  V(r2r_I64LoadMem16S, Generate_r2r_ILoadMem, kValueInt64, kIntS16)          \
  V(r2r_I64LoadMem16U, Generate_r2r_ILoadMem, kValueInt64, kIntU16)          \
  V(r2r_I64LoadMem32S, Generate_r2r_ILoadMem, kValueInt64, kIntS32)          \
  V(r2r_I64LoadMem32U, Generate_r2r_ILoadMem, kValueInt64, kIntU32)          \
  V(r2r_I32LoadMem, Generate_r2r_ILoadMem, kValueInt32, kIntS32)             \
  V(r2r_I64LoadMem, Generate_r2r_ILoadMem, kValueInt64, kInt64)              \
                                                                             \
  V(r2s_I32LoadMem8S, Generate_r2s_ILoadMem, kValueInt32, kIntS8)            \
  V(r2s_I32LoadMem8U, Generate_r2s_ILoadMem, kValueInt32, kIntU8)            \
  V(r2s_I32LoadMem16S, Generate_r2s_ILoadMem, kValueInt32, kIntS16)          \
  V(r2s_I32LoadMem16U, Generate_r2s_ILoadMem, kValueInt32, kIntU16)          \
  V(r2s_I64LoadMem8S, Generate_r2s_ILoadMem, kValueInt64, kIntS8)            \
  V(r2s_I64LoadMem8U, Generate_r2s_ILoadMem, kValueInt64, kIntU8)            \
  V(r2s_I64LoadMem16S, Generate_r2s_ILoadMem, kValueInt64, kIntS16)          \
  V(r2s_I64LoadMem16U, Generate_r2s_ILoadMem, kValueInt64, kIntU16)          \
  V(r2s_I64LoadMem32S, Generate_r2s_ILoadMem, kValueInt64, kIntS32)          \
  V(r2s_I64LoadMem32U, Generate_r2s_ILoadMem, kValueInt64, kIntU32)          \
  V(r2s_I32LoadMem, Generate_r2s_ILoadMem, kValueInt32, kIntS32)             \
  V(r2s_I64LoadMem, Generate_r2s_ILoadMem, kValueInt64, kInt64)              \
                                                                             \
  V(s2r_I32LoadMem8S, Generate_s2r_ILoadMem, kValueInt32, kIntS8)            \
  V(s2r_I32LoadMem8U, Generate_s2r_ILoadMem, kValueInt32, kIntU8)            \
  V(s2r_I32LoadMem16S, Generate_s2r_ILoadMem, kValueInt32, kIntS16)          \
  V(s2r_I32LoadMem16U, Generate_s2r_ILoadMem, kValueInt32, kIntU16)          \
  V(s2r_I64LoadMem8S, Generate_s2r_ILoadMem, kValueInt64, kIntS8)            \
  V(s2r_I64LoadMem8U, Generate_s2r_ILoadMem, kValueInt64, kIntU8)            \
  V(s2r_I64LoadMem16S, Generate_s2r_ILoadMem, kValueInt64, kIntS16)          \
  V(s2r_I64LoadMem16U, Generate_s2r_ILoadMem, kValueInt64, kIntU16)          \
  V(s2r_I64LoadMem32S, Generate_s2r_ILoadMem, kValueInt64, kIntS32)          \
  V(s2r_I64LoadMem32U, Generate_s2r_ILoadMem, kValueInt64, kIntU32)          \
  V(s2r_I32LoadMem, Generate_s2r_ILoadMem, kValueInt32, kIntS32)             \
  V(s2r_I64LoadMem, Generate_s2r_ILoadMem, kValueInt64, kInt64)              \
                                                                             \
  V(s2s_I32LoadMem8S, Generate_s2s_ILoadMem, kValueInt32, kIntS8)            \
  V(s2s_I32LoadMem8U, Generate_s2s_ILoadMem, kValueInt32, kIntU8)            \
  V(s2s_I32LoadMem16S, Generate_s2s_ILoadMem, kValueInt32, kIntS16)          \
  V(s2s_I32LoadMem16U, Generate_s2s_ILoadMem, kValueInt32, kIntU16)          \
  V(s2s_I64LoadMem8S, Generate_s2s_ILoadMem, kValueInt64, kIntS8)            \
  V(s2s_I64LoadMem8U, Generate_s2s_ILoadMem, kValueInt64, kIntU8)            \
  V(s2s_I64LoadMem16S, Generate_s2s_ILoadMem, kValueInt64, kIntS16)          \
  V(s2s_I64LoadMem16U, Generate_s2s_ILoadMem, kValueInt64, kIntU16)          \
  V(s2s_I64LoadMem32S, Generate_s2s_ILoadMem, kValueInt64, kIntS32)          \
  V(s2s_I64LoadMem32U, Generate_s2s_ILoadMem, kValueInt64, kIntU32)          \
  V(s2s_I32LoadMem, Generate_s2s_ILoadMem, kValueInt32, kIntS32)             \
  V(s2s_I64LoadMem, Generate_s2s_ILoadMem, kValueInt64, kInt64)              \
                                                                             \
  V(s2s_I32LoadMem8S_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt32,  \
    kIntS8)                                                                  \
  V(s2s_I32LoadMem8U_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt32,  \
    kIntU8)                                                                  \
  V(s2s_I32LoadMem16S_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt32, \
    kIntS16)                                                                 \
  V(s2s_I32LoadMem16U_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt32, \
    kIntU16)                                                                 \
  V(s2s_I64LoadMem8S_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt64,  \
    kIntS8)                                                                  \
  V(s2s_I64LoadMem8U_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt64,  \
    kIntU8)                                                                  \
  V(s2s_I64LoadMem16S_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt64, \
    kIntS16)                                                                 \
  V(s2s_I64LoadMem16U_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt64, \
    kIntU16)                                                                 \
  V(s2s_I64LoadMem32S_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt64, \
    kIntS32)                                                                 \
  V(s2s_I64LoadMem32U_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt64, \
    kIntU32)                                                                 \
  V(s2s_I32LoadMem_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt32,    \
    kIntS32)                                                                 \
  V(s2s_I64LoadMem_LocalSet, Generate_s2s_ILoadMem_LocalSet, kValueInt64,    \
    kInt64)                                                                  \
                                                                             \
  V(r2s_I32StoreMem8, Generate_r2s_IStoreMem, kValueInt32, kIntS8)           \
  V(r2s_I32StoreMem16, Generate_r2s_IStoreMem, kValueInt32, kIntS16)         \
  V(r2s_I64StoreMem8, Generate_r2s_IStoreMem, kValueInt64, kIntS8)           \
  V(r2s_I64StoreMem16, Generate_r2s_IStoreMem, kValueInt64, kIntS16)         \
  V(r2s_I64StoreMem32, Generate_r2s_IStoreMem, kValueInt64, kIntS32)         \
  V(r2s_I32StoreMem, Generate_r2s_IStoreMem, kValueInt32, kIntS32)           \
  V(r2s_I64StoreMem, Generate_r2s_IStoreMem, kValueInt64, kInt64)            \
                                                                             \
  V(s2s_I32StoreMem8, Generate_s2s_IStoreMem, kValueInt32, kIntS8)           \
  V(s2s_I32StoreMem16, Generate_s2s_IStoreMem, kValueInt32, kIntS16)         \
  V(s2s_I64StoreMem8, Generate_s2s_IStoreMem, kValueInt64, kIntS8)           \
  V(s2s_I64StoreMem16, Generate_s2s_IStoreMem, kValueInt64, kIntS16)         \
  V(s2s_I64StoreMem32, Generate_s2s_IStoreMem, kValueInt64, kIntS32)         \
  V(s2s_I32StoreMem, Generate_s2s_IStoreMem, kValueInt32, kIntS32)           \
  V(s2s_I64StoreMem, Generate_s2s_IStoreMem, kValueInt64, kInt64)            \
                                                                             \
  V(r2s_I32LoadStoreMem, Generate_r2s_ILoadStoreMem, kValueInt32, kIntS32)   \
  V(r2s_I64LoadStoreMem, Generate_r2s_ILoadStoreMem, kValueInt64, kInt64)    \
                                                                             \
  V(s2s_I32LoadStoreMem, Generate_s2s_ILoadStoreMem, kValueInt32, kIntS32)   \
  V(s2s_I64LoadStoreMem, Generate_s2s_ILoadStoreMem, kValueInt64, kInt64)

#define GENERATE_INT_LOADSTORE_BUILTIN(builtin_name, generator, value_type,   \
                                       memory_type)                           \
  void Builtins::Generate_##builtin_name##_s(MacroAssembler* masm) {          \
    return WasmInterpreterHandlerBuiltins<true>::generator(masm, value_type,  \
                                                           memory_type);      \
  }                                                                           \
  void Builtins::Generate_##builtin_name##_l(MacroAssembler* masm) {          \
    return WasmInterpreterHandlerBuiltins<false>::generator(masm, value_type, \
                                                            memory_type);     \
  }

FOREACH_INT_LOADSTORE_BUILTIN(GENERATE_INT_LOADSTORE_BUILTIN)
#undef FOREACH_INT_LOADSTORE_BUILTIN

#define FOREACH_FLOAT_LOADMEM_BUILTIN(V)                               \
  V(r2r_F32LoadMem, Generate_r2r_FLoadMem, kFloat32)                   \
  V(r2r_F64LoadMem, Generate_r2r_FLoadMem, kFloat64)                   \
  V(r2s_F32LoadMem, Generate_r2s_FLoadMem, kFloat32)                   \
  V(r2s_F64LoadMem, Generate_r2s_FLoadMem, kFloat64)                   \
  V(s2r_F32LoadMem, Generate_s2r_FLoadMem, kFloat32)                   \
  V(s2r_F64LoadMem, Generate_s2r_FLoadMem, kFloat64)                   \
  V(s2s_F32LoadMem, Generate_s2s_FLoadMem, kFloat32)                   \
  V(s2s_F64LoadMem, Generate_s2s_FLoadMem, kFloat64)                   \
  V(s2s_F32LoadMem_LocalSet, Generate_s2s_FLoadMem_LocalSet, kFloat32) \
  V(s2s_F64LoadMem_LocalSet, Generate_s2s_FLoadMem_LocalSet, kFloat64) \
  V(r2s_F32StoreMem, Generate_r2s_FStoreMem, kFloat32)                 \
  V(r2s_F64StoreMem, Generate_r2s_FStoreMem, kFloat64)                 \
  V(s2s_F32StoreMem, Generate_s2s_FStoreMem, kFloat32)                 \
  V(s2s_F64StoreMem, Generate_s2s_FStoreMem, kFloat64)                 \
  V(r2s_F32LoadStoreMem, Generate_r2s_FLoadStoreMem, kFloat32)         \
  V(r2s_F64LoadStoreMem, Generate_r2s_FLoadStoreMem, kFloat64)         \
  V(s2s_F32LoadStoreMem, Generate_s2s_FLoadStoreMem, kFloat32)         \
  V(s2s_F64LoadStoreMem, Generate_s2s_FLoadStoreMem, kFloat64)

#define GENERATE_FLOAT_LOADMEM_BUILTIN(builtin_name, generator, value_type)    \
  void Builtins::Generate_##builtin_name##_s(MacroAssembler* masm) {           \
    return WasmInterpreterHandlerBuiltins<true>::generator(masm, value_type);  \
  }                                                                            \
  void Builtins::Generate_##builtin_name##_l(MacroAssembler* masm) {           \
    return WasmInterpreterHandlerBuiltins<false>::generator(masm, value_type); \
  }

FOREACH_FLOAT_LOADMEM_BUILTIN(GENERATE_FLOAT_LOADMEM_BUILTIN)
#undef FOREACH_FLOAT_LOADMEM_BUILTIN

#endif  // !V8_DRUMBRAKE_BOUNDS_CHECKS

#endif  // V8_ENABLE_WEBASSEMBLY

#undef __

}  // namespace internal
}  // namespace v8

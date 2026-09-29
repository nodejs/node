// Copyright 2026 the V8 project authors. All rights reserved.
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
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

#if V8_ENABLE_WEBASSEMBLY
class RegisterAllocator {
 public:
  class Scoped {
   public:
    Scoped(RegisterAllocator* allocator, Register* reg)
        : allocator_(allocator), reg_(reg) {}
    ~Scoped() { allocator_->Free(reg_); }

   private:
    RegisterAllocator* allocator_;
    Register* reg_;
  };

  explicit RegisterAllocator(const RegList& registers)
      : initial_(registers), available_(registers) {}
  void Ask(Register* reg) {
    DCHECK_EQ(*reg, no_reg);
    DCHECK(!available_.is_empty());
    *reg = available_.PopFirst();
    allocated_registers_.push_back(reg);
  }

  bool RegisterIsAvailable(const Register& reg) { return available_.has(reg); }

  void Pinned(const Register& requested, Register* reg) {
    DCHECK(RegisterIsAvailable(requested));
    *reg = requested;
    Reserve(requested);
    allocated_registers_.push_back(reg);
  }

  void Free(Register* reg) {
    DCHECK_NE(*reg, no_reg);
    available_.set(*reg);
    *reg = no_reg;
    allocated_registers_.erase(
        find(allocated_registers_.begin(), allocated_registers_.end(), reg));
  }

  void Reserve(const Register& reg) {
    if (reg == no_reg) {
      return;
    }
    DCHECK(RegisterIsAvailable(reg));
    available_.clear(reg);
  }

  void Reserve(const Register& reg1, const Register& reg2,
               const Register& reg3 = no_reg, const Register& reg4 = no_reg,
               const Register& reg5 = no_reg, const Register& reg6 = no_reg) {
    Reserve(reg1);
    Reserve(reg2);
    Reserve(reg3);
    Reserve(reg4);
    Reserve(reg5);
    Reserve(reg6);
  }

  bool IsUsed(const Register& reg) {
    return initial_.has(reg) && !RegisterIsAvailable(reg);
  }

  void ResetExcept(const Register& reg1 = no_reg, const Register& reg2 = no_reg,
                   const Register& reg3 = no_reg, const Register& reg4 = no_reg,
                   const Register& reg5 = no_reg, const Register& reg6 = no_reg,
                   const Register& reg7 = no_reg) {
    available_ = initial_;
    available_.clear(reg1);
    available_.clear(reg2);
    available_.clear(reg3);
    available_.clear(reg4);
    available_.clear(reg5);
    available_.clear(reg6);
    available_.clear(reg7);

    auto it = allocated_registers_.begin();
    while (it != allocated_registers_.end()) {
      if (RegisterIsAvailable(**it)) {
        **it = no_reg;
        allocated_registers_.erase(it);
      } else {
        it++;
      }
    }
  }

  static RegisterAllocator WithAllocatableGeneralRegisters() {
    RegList list;
    const RegisterConfiguration* config(RegisterConfiguration::Default());

    for (int i = 0; i < config->num_allocatable_general_registers(); ++i) {
      int code = config->GetAllocatableGeneralCode(i);
      Register candidate = Register::from_code(code);
      list.set(candidate);
    }
    return RegisterAllocator(list);
  }

 private:
  std::vector<Register*> allocated_registers_;
  const RegList initial_;
  RegList available_;
};

#define DEFINE_REG(Name)  \
  Register Name = no_reg; \
  regs.Ask(&Name);

#define ASSIGN_REG(Name) regs.Ask(&Name);

#define DEFINE_PINNED(Name, Reg) \
  Register Name = no_reg;        \
  regs.Pinned(Reg, &Name);

#define ASSIGN_PINNED(Name, Reg) regs.Pinned(Reg, &Name);

#define DEFINE_SCOPED(Name) \
  DEFINE_REG(Name)          \
  RegisterAllocator::Scoped scope_##Name(&regs, &Name);

#define FREE_REG(Name) regs.Free(&Name);

void Builtins::Generate_WasmInterpreterEntry(MacroAssembler* masm) {
  // Input registers:
  //  a7 (kWasmImplicitArgRegister): wasm_instance
  //  a6: array_start
  //  a5: function_index
  //  a4: result_start
  //  a3: ref_params_array
  auto regs = RegisterAllocator::WithAllocatableGeneralRegisters();

  DEFINE_PINNED(wasm_instance, a7);
  DEFINE_PINNED(array_start, a6);
  DEFINE_PINNED(function_index, a5);
  DEFINE_PINNED(result_start, a4);
  DEFINE_PINNED(ref_params_array, a3);

  // Set up the stackframe:
  //
  // fp-0x18  ref_params_array  (GC-scanned)
  // fp-0x10  wasm_instance     (GC-scanned)
  // fp-0x08  Marker(StackFrame::WASM_INTERPRETER_ENTRY)
  // fp       Old fp
  // fp+0x08  ra
  __ EnterFrame(StackFrame::WASM_INTERPRETER_ENTRY);

  // Push args. kWasmImplicitArgRegister at fp-16 and ref_params_array at
  // fp-24 are GC-scanned by WasmInterpreterEntryFrame::Iterate.
  __ Push(kWasmImplicitArgRegister, ref_params_array, function_index,
          array_start);
  __ Push(result_start);
  __ Move(kWasmImplicitArgRegister, zero_reg);
  __ CallRuntime(Runtime::kWasmRunInterpreter, 5);

  // Deconstruct the stack frame.
  __ LeaveFrame(StackFrame::WASM_INTERPRETER_ENTRY);
  __ Ret();
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
  __ LoadTaggedField(
      wasm_instance,
      FieldMemOperand(trusted_instance_data,
                      WasmTrustedInstanceData::kInstanceObjectOffset));
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
      FieldMemOperand(shared_function_info,
                      offsetof(SharedFunctionInfo, trusted_function_data_)),
      kWasmExportedFunctionDataIndirectPointerTag);
  shared_function_info = no_reg;

  LoadWasmInstanceFromFunctionData(masm, function_data, wasm_instance);
}

void LoadFromSignature(MacroAssembler* masm, Register valuetypes_array_ptr,
                       Register return_count, Register param_count) {
  Register signature = valuetypes_array_ptr;
  __ LoadWord(return_count,
              MemOperand(signature, wasm::FunctionSig::kReturnCountOffset));
  __ LoadWord(param_count,
              MemOperand(signature, wasm::FunctionSig::kParameterCountOffset));
  valuetypes_array_ptr = signature;
  __ LoadWord(valuetypes_array_ptr,
              MemOperand(signature, wasm::FunctionSig::kRepsOffset));
}

void LoadValueTypesArray(MacroAssembler* masm, Register function_data,
                         Register valuetypes_array_ptr, Register return_count,
                         Register param_count, Register signature_data) {
  __ LoadTaggedField(
      signature_data,
      FieldMemOperand(function_data,
                      offsetof(WasmExportedFunctionData, packed_args_size_)));
  __ SmiToInt32(signature_data);

  Register internal_function = valuetypes_array_ptr;
  __ LoadProtectedPointerField(
      internal_function,
      MemOperand(
          function_data,
          offsetof(WasmFunctionData, protected_internal_) - kHeapObjectTag));

  Register signature = internal_function;
  __ LoadWord(signature, MemOperand(internal_function,
                                    offsetof(WasmInternalFunction, sig_) -
                                        kHeapObjectTag));
  LoadFromSignature(masm, valuetypes_array_ptr, return_count, param_count);
}

// TODO(paolosev@microsoft.com): this should be converted into a Torque builtin,
// like it was done for GenericJSToWasmWrapper.
void Builtins::Generate_JSToWasmInterpreterWrapperAsm(MacroAssembler* masm) {
  auto regs = RegisterAllocator::WithAllocatableGeneralRegisters();
  __ EnterFrame(StackFrame::JS_TO_WASM);

  /* Current frame */
  // fp + kImplicitArgOffset: functionData
  // fp + kResultArrayParamOffset: result array
  // fp + 8 * kSystemPointerSize: return address
  // fp:  old fp
  // fp - 8 : type
  // fp -16 : wrpperbuffer
  // fp -24 : GcScanSlotCount
  // fp -32 : WasmInstance
  // fp -40 : result_array

  // Load the implicit argument (instance data or import data) from the frame.
  DEFINE_PINNED(wasm_instance, kWasmImplicitArgRegister);
  DEFINE_PINNED(implicitArg, a1);
  DEFINE_REG(result_array);
  DEFINE_PINNED(wrapper_buffer,
                WasmJSToWasmWrapperDescriptor::WrapperBufferRegister());

  // Leave the frame space
  __ SubWord(sp, sp, Operand(4 * kSystemPointerSize));
  __ LoadWord(
      implicitArg,
      MemOperand(fp, JSToWasmWrapperFrameConstants::kImplicitArgOffset));
  LoadWasmInstanceFromFunctionData(masm, /*function_data=*/implicitArg,
                                   /*wasm_instance=*/wasm_instance);
  __ LoadWord(
      result_array,
      MemOperand(fp, JSToWasmWrapperFrameConstants::kResultArrayParamOffset));
  __ StoreWord(
      implicitArg,
      MemOperand(fp, WasmInterpreterWrapperConstants::kImplicitArgOffset));
  __ StoreWord(
      result_array,
      MemOperand(fp, WasmInterpreterWrapperConstants::kResultArrayOffset));

  DEFINE_PINNED(params_start, a6);  // array_start
  __ LoadWord(
      params_start,
      MemOperand(wrapper_buffer,
                 WasmInterpreterWrapperConstants::kWrapperBufferParamStart));

  DEFINE_PINNED(function_index, a5);
  __ Lw(function_index,
        MemOperand(wrapper_buffer,
                   WasmInterpreterWrapperConstants::kWrapperBufferCallTarget));
  __ SmiTag(function_index);
  __ AssertSmi(function_index);
  __ StoreWord(
      zero_reg,
      MemOperand(fp, WasmInterpreterWrapperConstants::kGCScanSlotCountOffset));
  // -------------------------------------------
  // Call the Wasm function.
  // -------------------------------------------
  {
    DEFINE_SCOPED(result_size);
    __ LoadWord(
        result_size,
        MemOperand(wrapper_buffer, JSToWasmWrapperFrameConstants::
                                       kWrapperBufferStackReturnBufferSize));
    // The `result_size` is the number of slots needed on the stack to store the
    // return values of the wasm function. If `result_size` is an odd number, we
    // have to add `1` to preserve stack pointer alignment.
    __ AddWord(result_size, result_size, 1);
    __ And(result_size, result_size, Operand(-2));  // clear bit 0 to align
    __ SllWord(result_size, result_size, kSystemPointerSizeLog2);
    __ SubWord(sp, sp, Operand(result_size));
  }
  DEFINE_PINNED(result_start, a4);
  __ Move(result_start, sp);
  __ StoreWord(
      result_start,
      MemOperand(
          wrapper_buffer,
          JSToWasmWrapperFrameConstants::kWrapperBufferStackReturnBufferStart));
  __ Push(params_start, wrapper_buffer);
  // Load the FixedArray of converted reference parameters (or Undefined)
  // from the wrapper buffer into a3, which WasmInterpreterEntry will
  // forward to Runtime_WasmRunInterpreter.
  DEFINE_PINNED(ref_params_array, a3);
  __ LoadWord(
      ref_params_array,
      MemOperand(
          wrapper_buffer,
          WasmInterpreterWrapperConstants::kWrapperBufferRefParamsArray));
  __ Call(BUILTIN_CODE(masm->isolate(), WasmInterpreterEntry),
          RelocInfo::CODE_TARGET);
  __ Pop(params_start, wrapper_buffer);
  regs.ResetExcept(wasm_instance, wrapper_buffer);
  // The wrapper_buffer has to be in a2 as the correct parameter register.
  regs.Reserve(kReturnRegister0, kReturnRegister1, a2);
  // Reload from the caller's outgoing-arg slot rather than from our own
  // kImplicitArgOffset spill. The caller (Torque JSToWasmInterpreterWrapper)
  // keeps `functionData` alive AND updates it across GC, per
  // WasmJSToWasmWrapperDescriptor's stack-parameter contract. Our own spill
  // at WasmInterpreterWrapperConstants::kImplicitArgOffset is intentionally
  // not visited by JsToWasmFrame::Iterate and therefore must not be read
  // after any safepoint (e.g. the WasmInterpreterEntry call above).
  __ LoadWord(
      wasm_instance,
      MemOperand(fp, JSToWasmWrapperFrameConstants::kImplicitArgOffset));
  LoadWasmInstanceFromFunctionData(masm, /*function_data=*/wasm_instance,
                                   /*wasm_instance=*/wasm_instance);

  __ LoadWord(
      a0,
      MemOperand(fp, JSToWasmWrapperFrameConstants::kResultArrayParamOffset));
  __ mv(a1, wrapper_buffer);
  // a0: cp
  // a1: result_array
  // a2: wrapper_buffer
  __ CallBuiltin(Builtin::kJSToWasmInterpreterHandleReturns);
  __ LeaveFrame(StackFrame::JS_TO_WASM);
  // incoming argument count matches both cases:
  //  instance and result array
  //   constexpr int64_t stack_arguments_in = 2;
  //   __ DropArguments(stack_arguments_in);
  __ AddWord(sp, sp, Operand(2 * kSystemPointerSize));
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
  auto regs = RegisterAllocator::WithAllocatableGeneralRegisters();
  Label invoke, handler_entry, exit;

  __ EnterFrame(StackFrame::C_WASM_ENTRY);

  // Space to store c_entry_fp and current sp (used by exception handler).
  __ Push(zero_reg, zero_reg);

  {
    NoRootArrayScope no_root_array(masm);

    // Save callee-saved FPU registers.
    __ MultiPushFPU(kCalleeSavedFPU);
    // Save callee saved registers on the stack.
    __ MultiPush(kCalleeSaved);

    // Set up the reserved register for 0.0.
    __ LoadFPRImmediate(kDoubleRegZero, 0.0);
    __ LoadFPRImmediate(kSingleRegZero, 0.0f);

    // Initialize the root register.
    // see wasm-interpreter-runtime.cc:CallWasmToJSBuiltin
    __ Move(kRootRegister, a2);

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
    // Initialize the pointer cage base register.
    __ LoadRootRelative(kPtrComprCageBaseRegister,
                        IsolateData::cage_base_offset());
#endif
  }

  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ Move(scratch, sp);
    __ StoreWord(
        scratch,
        MemOperand(fp, WasmInterpreterCWasmEntryConstants::kSPFPOffset));
  }

  DEFINE_REG(centry_fp_address);
  DEFINE_REG(centry_fp);
  __ li(centry_fp_address,
        ExternalReference::Create(IsolateFieldId::kCEntryFP, masm->isolate()));
  __ LoadWord(centry_fp, MemOperand(centry_fp_address));  // s4 = C entry FP.
  __ StoreWord(
      centry_fp,
      MemOperand(fp, WasmInterpreterCWasmEntryConstants::kCEntryFPOffset));

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ Branch(&invoke);

  // Prevent the constant pool from being emitted between the record of the
  // handler_entry position and the first instruction of the sequence here.
  // There is no risk because Assembler::Emit() emits the instruction before
  // checking for constant pool emission, but we do not want to depend on
  // that.
  {
    Assembler::BlockPoolsScope block_pools(masm);

    __ BindExceptionHandler(&handler_entry);

    // Store the current pc as the handler offset. It's used later to create the
    // handler table.
    masm->isolate()->builtins()->SetCWasmInterpreterEntryHandlerOffset(
        handler_entry.pos());
  }
  __ Branch(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);

  // Link the current handler as the next handler.
  DEFINE_REG(handler);
  __ LoadWord(handler, __ AsMemOperand(IsolateFieldId::kHandler));
  __ Push(handler);

  // Set this new handler as the current one.
  __ StoreWord(sp, __ AsMemOperand(IsolateFieldId::kHandler));

  // Invoke the JS function through the GenericWasmToJSInterpreterWrapper.
  __ Call(BUILTIN_CODE(masm->isolate(), GenericWasmToJSInterpreterWrapper),
          RelocInfo::CODE_TARGET);

  // Pop the stack handler and unlink this frame from the handler chain.
  static_assert(StackHandlerConstants::kNextOffset == 0 * kSystemPointerSize,
                "Unexpected offset for StackHandlerConstants::kNextOffset");
  __ Pop(handler);
  __ Drop(StackHandlerConstants::kSlotCount - 2);
  __ StoreWord(handler, __ AsMemOperand(IsolateFieldId::kHandler));

  __ bind(&exit);

  // Pop callee saved registers.
  __ MultiPop(kCalleeSaved);
  __ MultiPopFPU(kCalleeSavedFPU);

  // Deconstruct the stack frame.
  __ LeaveFrame(StackFrame::C_WASM_ENTRY);
  __ Ret();
}

void Builtins::Generate_GenericWasmToJSInterpreterWrapper(
    MacroAssembler* masm) {
  auto regs = RegisterAllocator::WithAllocatableGeneralRegisters();

  DEFINE_PINNED(target_js_function, a0);
  DEFINE_PINNED(packed_args, a1);
  DEFINE_PINNED(signature, a3);
  DEFINE_PINNED(callable, a5);

  // Set up the stackframe.
  __ EnterFrame(StackFrame::WASM_TO_JS);

  // -------------------------------------------
  // Compute offsets and prepare for GC.
  // -------------------------------------------
  // GenericJSToWasmInterpreterWrapperFrame:
  // sp = fp-N receiver                      ^
  // ...       JS arg 0                      |
  // ...       ...                           | Tagged
  // ...       JS arg n-1                    | objects
  // ...       (padding if num args is odd)  |
  // fp-0x60   context                       |
  // fp-0x58   callable / call result        v
  // -------------------------------------------
  // fp-0x50   current_param_offset/current_result_offset
  // fp-0x48   valuetypes_array_ptr
  //
  // fp-0x40   param_index/return_index
  // fp-0x38   signature
  //
  // fp-0x30   param_count
  // fp-0x28   return_count
  //
  // fp-0x20   packed_array
  // fp-0x18   GC_SP (not used on arm64)
  //
  // fp-0x10   GCScanSlotLimit
  // fp-0x08   Marker(StackFrame::WASM_TO_JS)
  //
  // fp        Old fp
  // fp+0x08   return address

  static_assert(WasmToJSInterpreterFrameConstants::kGCSPOffset ==
                WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset -
                    kSystemPointerSize);
  constexpr int kPackedArrayOffset =
      WasmToJSInterpreterFrameConstants::kGCSPOffset - kSystemPointerSize;
  constexpr int kReturnCountOffset = kPackedArrayOffset - kSystemPointerSize;
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
  constexpr int kCurrentResultOffset = kCurrentParamOffset;
  constexpr int kCallableOffset = kCurrentParamOffset - kSystemPointerSize;
  constexpr int kContextOffset = kCallableOffset - kSystemPointerSize;
  constexpr int kNumSpillSlots =
      (WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset -
       kCurrentParamOffset) /
      kSystemPointerSize;
  static_assert((kNumSpillSlots % 2) == 0);  // 16-bytes aligned.

  __ SubWord(sp, sp, Operand(kNumSpillSlots * kSystemPointerSize));

  __ StoreWord(packed_args, MemOperand(fp, kPackedArrayOffset));

  // Not used on RISCV?.
  __ StoreWord(zero_reg,
               MemOperand(fp, WasmToJSInterpreterFrameConstants::kGCSPOffset));

  __ StoreWord(
      zero_reg,
      MemOperand(fp,
                 WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset));

  DEFINE_REG(shared_function_info);
  __ LoadTaggedField(
      shared_function_info,
      FieldMemOperand(target_js_function,
                      offsetof(JSFunction, shared_function_info_)));

  // Set the context of the function; the call has to run in the function
  // context.
  DEFINE_PINNED(context, cp);
  __ LoadTaggedField(context, FieldMemOperand(target_js_function,
                                              offsetof(JSFunction, context_)));
  __ StoreWord(context, MemOperand(fp, kContextOffset));

  // Load global receiver if sloppy else use undefined.
  Label receiver_undefined;
  Label calculate_js_function_arity;
  DEFINE_REG(receiver);
  DEFINE_REG(flags);
  __ LoadWord(flags, FieldMemOperand(shared_function_info,
                                     offsetof(SharedFunctionInfo, flags_)));
  __ Branch(&receiver_undefined, ne, flags,
            Operand(SharedFunctionInfo::IsNativeBit::kMask |
                    SharedFunctionInfo::IsStrictBit::kMask));
  FREE_REG(flags);
  __ LoadGlobalProxy(receiver);
  __ Branch(&calculate_js_function_arity);

  __ bind(&receiver_undefined);
  __ LoadRoot(receiver, RootIndex::kUndefinedValue);

  __ bind(&calculate_js_function_arity);

  // Load values from the signature.
  DEFINE_REG(return_count);
  DEFINE_REG(param_count);
  __ StoreWord(signature, MemOperand(fp, kSignatureOffset));
  Register valuetypes_array_ptr = signature;
  LoadFromSignature(masm, valuetypes_array_ptr, return_count, param_count);
  __ StoreWord(param_count, MemOperand(fp, kParamCountOffset));
  FREE_REG(shared_function_info);

  // Make room to pass the args and the receiver.
  DEFINE_REG(array_size);
  DEFINE_REG(scratch);

  // Points to the end of the stack frame area that contains tagged objects
  // that need to be visited during GC.
  __ StoreWord(
      sp, MemOperand(
              fp, WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset));

  // Store callable and context.
  __ Push(callable, context);
  __ AddWord(array_size, param_count, Operand(1));
  // Ensure that the array is 16-bytes aligned.
  __ AddWord(scratch, array_size, Operand(1));
  __ And(array_size, scratch, Operand(-2));
  __ slli(scratch, array_size, kSystemPointerSizeLog2);
  __ SubWord(sp, sp, Operand(scratch));

  // Make sure that the padding slot (if present) is reset to zero. The other
  // slots will be initialized with the arguments.
  __ SubWord(scratch, array_size, Operand(1));
  __ slli(scratch, scratch, kSystemPointerSizeLog2);
  __ AddWord(scratch, sp, scratch);
  __ StoreWord(zero_reg, MemOperand(scratch, 0));
  FREE_REG(array_size);

  DEFINE_REG(param_index);
  __ Move(param_index, zero_reg);

  // Store the receiver at the top of the stack.
  __ StoreWord(receiver, MemOperand(sp, 0));

  // -------------------------------------------
  // Store signature-related values to the stack.
  // -------------------------------------------
  // We store values on the stack to restore them after function calls.
  __ StoreWord(return_count, MemOperand(fp, kReturnCountOffset));
  __ StoreWord(valuetypes_array_ptr,
               MemOperand(fp, kValueTypesArrayStartOffset));

  Label prepare_for_js_call;
  // If we have 0 params: jump through parameter handling.
  __ Branch(&prepare_for_js_call, eq, param_count, Operand(0));

  // Loop through the params starting with the first.
  DEFINE_REG(current_param_slot_offset);
  __ li(current_param_slot_offset, Operand(0));

  // We have to check the types of the params. The ValueType array contains
  // first the return then the param types.

  // Set the ValueType array pointer to point to the first parameter.
  constexpr int kValueTypeSize = sizeof(wasm::ValueType);
  static_assert(kValueTypeSize == 4);
  const int32_t kValueTypeSizeLog2 = log2(kValueTypeSize);
  __ slli(scratch, return_count, kValueTypeSizeLog2);
  __ AddWord(valuetypes_array_ptr, valuetypes_array_ptr, Operand(scratch));
  DEFINE_REG(valuetype);

  // -------------------------------------------
  // Copy reference type params first and initialize the stack for JS arguments.
  // -------------------------------------------

  // Heap pointers for ref type values in packed_args can be invalidated if GC
  // is triggered when converting wasm numbers to JS numbers and allocating
  // heap numbers. So, we have to move them to the stack first.
  FREE_REG(target_js_function);
  DEFINE_PINNED(param, a0);
  {
    Label loop_copy_param_ref, load_ref_param, set_and_move;

    __ bind(&loop_copy_param_ref);
    __ Lw(valuetype, MemOperand(valuetypes_array_ptr, 0));
    // Test bit 0 (reference type indicator): valuetype & 1
    {
      Register tmp = scratch;
      __ And(tmp, valuetype, Operand(1));
      __ Branch(&load_ref_param, ne, tmp, Operand(zero_reg));
    }

    // Initialize non-ref type slots to zero since they can be visited by GC
    // when converting wasm numbers into heap numbers.
    __ Move(param, Smi::zero());

    Label inc_param_32bit;
    __ Branch(&inc_param_32bit, eq, valuetype,
              Operand(wasm::kWasmI32.raw_bit_field()));
    __ Branch(&inc_param_32bit, eq, valuetype,
              Operand(wasm::kWasmF32.raw_bit_field()));

    Label inc_param_64bit;
    __ Branch(&inc_param_64bit, eq, valuetype,
              Operand(wasm::kWasmI64.raw_bit_field()));
    __ Branch(&inc_param_64bit, eq, valuetype,
              Operand(wasm::kWasmF64.raw_bit_field()));

    // Invalid type. Wasm cannot pass Simd arguments to JavaScript.
    __ DebugBreak();

    __ bind(&inc_param_32bit);
    __ AddWord(current_param_slot_offset, current_param_slot_offset,
               Operand(sizeof(int32_t)));
    __ Branch(&set_and_move);

    __ bind(&inc_param_64bit);
    __ AddWord(current_param_slot_offset, current_param_slot_offset,
               Operand(sizeof(int64_t)));
    __ Branch(&set_and_move);

    __ bind(&load_ref_param);
    // No need to align packed_args for ref values in wasm-to-js, because the
    // alignment is only required for GC code that visits the stack, and in this
    // case we are storing into the stack only heap (or Smi) objects, always
    // aligned.
    __ AddWord(scratch, packed_args, current_param_slot_offset);
    __ LoadWord(param, MemOperand(scratch, 0));
    __ AddWord(current_param_slot_offset, current_param_slot_offset,
               Operand(kSystemPointerSize));

    __ bind(&set_and_move);
    __ AddWord(param_index, param_index, 1);
    // Pre-increment param_index to skip receiver slot.
    __ slli(scratch, param_index, kSystemPointerSizeLog2);
    __ AddWord(scratch, sp, scratch);
    __ StoreWord(param, MemOperand(scratch, 0));
    __ AddWord(valuetypes_array_ptr, valuetypes_array_ptr, kValueTypeSize);
    __ Branch(&loop_copy_param_ref, lt, param_index, Operand(param_count));
  }

  // Reset pointers for the second param conversion loop.
  __ LoadWord(return_count, MemOperand(fp, kReturnCountOffset));
  __ LoadWord(valuetypes_array_ptr,
              MemOperand(fp, kValueTypesArrayStartOffset));
  __ slli(return_count, return_count, kValueTypeSizeLog2);
  __ AddWord(valuetypes_array_ptr, valuetypes_array_ptr, Operand(return_count));
  __ mv(current_param_slot_offset, zero_reg);
  __ mv(param_index, zero_reg);

  // -------------------------------------------
  // Param evaluation loop.
  // -------------------------------------------
  Label loop_through_params;
  __ bind(&loop_through_params);

  __ Lw(valuetype, MemOperand(valuetypes_array_ptr, 0));

  // -------------------------------------------
  // Param conversion.
  // -------------------------------------------
  // If param is a Smi we can easily convert it. Otherwise we'll call a builtin
  // for conversion.
  Label param_conversion_done, check_ref_param, skip_ref_param, convert_param;
  __ Branch(&check_ref_param, ne, valuetype,
            Operand(wasm::kWasmI32.raw_bit_field()));

  // I32 param: change to Smi.
  __ AddWord(scratch, packed_args, current_param_slot_offset);
  __ Lw(param, MemOperand(scratch, 0));

  // If pointer compression is disabled, we can convert to a smi.
  if (SmiValuesAre32Bits()) {
    __ SmiTag(param);
  } else {
    // Double the return value to test if it can be a Smi.
    // Overflow detection: (doubled XOR param) < 0
    Register doubled = scratch;
    Register sign_diff = t0;  // temporary register for XOR result
    __ Add32(doubled, param, Operand(param));
    __ Xor(sign_diff, doubled, param);
    // If overflow occurred (sign changed), convert the return value to a
    // HeapNumber.
    __ Branch(&convert_param, lt, sign_diff, Operand(zero_reg));
    // If there was no overflow, we can convert to Smi.
    __ SmiTag(param);
  }

  // Place the param into the proper slot.
  // Pre-increment param_index to skip the receiver slot.
  __ AddWord(param_index, param_index, 1);
  __ slli(scratch, param_index, kSystemPointerSizeLog2);
  __ AddWord(scratch, sp, scratch);
  __ StoreWord(param, MemOperand(scratch, 0));
  __ AddWord(current_param_slot_offset, current_param_slot_offset,
             sizeof(int32_t));

  __ Branch(&param_conversion_done);

  // Skip Ref params. We already copied reference params in the first loop.
  __ bind(&check_ref_param);
  {
    Register tmp = scratch;
    __ And(tmp, valuetype, Operand(1));
    __ Branch(&convert_param, eq, tmp, Operand(zero_reg));
  }

  __ bind(&skip_ref_param);
  __ AddWord(param_index, param_index, Operand(1));
  __ AddWord(current_param_slot_offset, current_param_slot_offset,
             Operand(kSystemPointerSize));
  __ Branch(&param_conversion_done);

  // -------------------------------------------------
  // Param conversion builtins (Wasm type -> JS type).
  // -------------------------------------------------
  __ bind(&convert_param);

  // Prepare for builtin call.

  // Need to specify how many heap objects, that should be scanned by GC, are
  // on the top of the stack. (Only the context).
  // The builtin expects the parameter to be in register param = a0.

  __ StoreWord(param_index, MemOperand(fp, kParamIndexOffset));
  __ StoreWord(valuetypes_array_ptr,
               MemOperand(fp, kValueTypesArrayStartOffset));
  __ StoreWord(current_param_slot_offset, MemOperand(fp, kCurrentParamOffset));

  Label param_kWasmI32_not_smi;
  Label param_kWasmI64;
  Label param_kWasmF32;
  Label param_kWasmF64;
  Label finish_param_conversion;

  __ Branch(&param_kWasmI32_not_smi, eq, valuetype,
            Operand(wasm::kWasmI32.raw_bit_field()));
  __ Branch(&param_kWasmI64, eq, valuetype,
            Operand(wasm::kWasmI64.raw_bit_field()));
  __ Branch(&param_kWasmF32, eq, valuetype,
            Operand(wasm::kWasmF32.raw_bit_field()));
  __ Branch(&param_kWasmF64, eq, valuetype,
            Operand(wasm::kWasmF64.raw_bit_field()));

  // Invalid type. Wasm cannot pass Simd arguments to JavaScript.
  __ DebugBreak();

  Register increment = scratch;
  __ bind(&param_kWasmI32_not_smi);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmInt32ToHeapNumber),
          RelocInfo::CODE_TARGET);
  // Param is the result of the builtin.
  __ li(increment, Operand(sizeof(int32_t)));
  __ Branch(&finish_param_conversion);

  __ bind(&param_kWasmI64);
  __ AddWord(scratch, packed_args, current_param_slot_offset);
  __ LoadWord(param, MemOperand(scratch, 0));
  __ Call(BUILTIN_CODE(masm->isolate(), I64ToBigInt), RelocInfo::CODE_TARGET);
  __ li(increment, Operand(sizeof(int64_t)));
  __ Branch(&finish_param_conversion);

  __ bind(&param_kWasmF32);
  __ AddWord(scratch, packed_args, current_param_slot_offset);
  __ LoadFloat(fa0, MemOperand(scratch, 0));
  __ Call(BUILTIN_CODE(masm->isolate(), WasmFloat32ToNumber),
          RelocInfo::CODE_TARGET);
  __ li(increment, Operand(sizeof(float)));
  __ Branch(&finish_param_conversion);

  __ bind(&param_kWasmF64);
  __ AddWord(scratch, packed_args, current_param_slot_offset);
  __ LoadDouble(fa0, MemOperand(scratch, 0));
  __ Call(BUILTIN_CODE(masm->isolate(), WasmFloat64ToNumber),
          RelocInfo::CODE_TARGET);
  __ li(increment, Operand(sizeof(double)));

  // Restore after builtin call.
  __ bind(&finish_param_conversion);

  __ LoadWord(current_param_slot_offset, MemOperand(fp, kCurrentParamOffset));
  __ AddWord(current_param_slot_offset, current_param_slot_offset, increment);
  __ LoadWord(valuetypes_array_ptr,
              MemOperand(fp, kValueTypesArrayStartOffset));
  __ LoadWord(param_index, MemOperand(fp, kParamIndexOffset));
  __ LoadWord(packed_args, MemOperand(fp, kPackedArrayOffset));
  __ LoadWord(param_count, MemOperand(fp, kParamCountOffset));

  __ AddWord(param_index, param_index, 1);
  __ slli(scratch, param_index, kSystemPointerSizeLog2);
  __ AddWord(scratch, sp, scratch);
  __ StoreWord(param, MemOperand(scratch, 0));

  // -------------------------------------------
  // Param conversion done.
  // -------------------------------------------
  __ bind(&param_conversion_done);

  __ AddWord(valuetypes_array_ptr, valuetypes_array_ptr,
             Operand(kValueTypeSize));
  __ Branch(&loop_through_params, lt, param_index, Operand(param_count));

  // -------------------------------------------
  // Prepare for the function call.
  // -------------------------------------------
  __ bind(&prepare_for_js_call);

  regs.ResetExcept(packed_args, valuetypes_array_ptr, context, return_count,
                   valuetype, scratch);

  // -------------------------------------------
  // Call the JS function.
  // -------------------------------------------
  // Call_ReceiverIsAny expects the arguments in the stack in this order:
  // sp + (n * 0x08)  JS arg n-1
  // ...              ...
  // sp + 0x08        JS arg 0
  // sp               Receiver
  //
  // It also expects two arguments passed in registers:
  // a0: number of arguments + 1 (receiver)
  // a1: target (JSFunction|JSBoundFunction|...)

  // a0: Receiver.
  __ LoadWord(a0, MemOperand(fp, kParamCountOffset));
  __ AddWord(a0, a0, 1);  // AddWord 1 to count receiver.

  // a1: callable.
  __ LoadWord(kJSFunctionRegister, MemOperand(fp, kCallableOffset));

  __ Call(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);

  __ LoadWord(context, MemOperand(fp, kContextOffset));

  // The JS function returns its result in register a0.
  DEFINE_PINNED(return_reg, kReturnRegister0);

  // No slots to visit during GC.
  __ Move(scratch, sp);
  __ StoreWord(
      scratch,
      MemOperand(fp,
                 WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset));

  // -------------------------------------------
  // Return handling.
  // -------------------------------------------
  __ LoadWord(return_count, MemOperand(fp, kReturnCountOffset));
  __ LoadWord(packed_args, MemOperand(fp, kPackedArrayOffset));
  __ LoadWord(signature, MemOperand(fp, kSignatureOffset));
  __ LoadWord(valuetypes_array_ptr,
              MemOperand(signature, wasm::FunctionSig::kRepsOffset));

  DEFINE_REG(result_index);
  __ Move(result_index, zero_reg);
  DEFINE_REG(current_result_offset);
  __ Move(current_result_offset, zero_reg);

  // If we have return values, convert them from JS types back to Wasm types.
  Label convert_return;
  Label return_done;
  Label all_done;
  Label loop_copy_return_refs;
  __ Branch(&all_done, lt, return_count, Operand(1));
  __ Branch(&convert_return, eq, return_count, Operand(1));

  // We have multiple results. Convert the result into a FixedArray.
  DEFINE_REG(fixed_array);
  __ Move(fixed_array, zero_reg);

  // The builtin expects three args:
  // a0: object.
  // a1: return_count as Smi.
  // s7 (cp): context.
  __ LoadWord(a1, MemOperand(fp, kReturnCountOffset));
  __ AddWord(a1, a1, a1);

  // Two tagged objects at the top of the stack (the context and the result,
  // which we store at the place of the function object we called).
  static_assert(
      kCurrentParamOffset == kContextOffset + 0x10,
      "Expected two (tagged) slots between 'context' and 'current_param'.");
  __ StoreWord(a0, MemOperand(fp, kCallableOffset));
  // Here sp points to the Context spilled slot: [fp - kContextOffset].
  __ AddWord(scratch, sp, Operand(kSystemPointerSize * 2));
  __ StoreWord(
      scratch,
      MemOperand(fp,
                 WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset));

  // We can have a GC here!
  __ Call(BUILTIN_CODE(masm->isolate(), IterableToFixedArrayForWasm),
          RelocInfo::CODE_TARGET);
  __ Move(fixed_array, kReturnRegister0);

  // Store fixed_array at the second top of the stack (in place of callable).
  __ StoreWord(fixed_array, MemOperand(sp, kSystemPointerSize));
  __ AddWord(scratch, sp, Operand(2 * kSystemPointerSize));
  __ StoreWord(
      scratch,
      MemOperand(fp,
                 WasmToJSInterpreterFrameConstants::kGCScanSlotLimitOffset));

  __ LoadWord(return_count, MemOperand(fp, kReturnCountOffset));
  __ LoadWord(packed_args, MemOperand(fp, kPackedArrayOffset));
  __ LoadWord(signature, MemOperand(fp, kSignatureOffset));
  __ LoadWord(valuetypes_array_ptr,
              MemOperand(signature, wasm::FunctionSig::kRepsOffset));
  __ Move(result_index, zero_reg);
  __ Move(current_result_offset, zero_reg);

  __ AddWord(scratch, fixed_array,
             OFFSET_OF_DATA_START(FixedArray) - kHeapObjectTag);
  //   __ LoadTaggedField(return_reg,
  //                      MemOperand(scratch, result_index, LSL,
  //                      kTaggedSizeLog2));
  __ LoadTaggedField(return_reg, MemOperand(scratch, 0));

  // -------------------------------------------
  // Return conversions (JS type -> Wasm type).
  // -------------------------------------------
  __ bind(&convert_return);

  // Save registers in the stack before the builtin call.
  __ StoreWord(current_result_offset, MemOperand(fp, kCurrentResultOffset));
  __ StoreWord(result_index, MemOperand(fp, kResultIndexOffset));
  __ StoreWord(valuetypes_array_ptr,
               MemOperand(fp, kValueTypesArrayStartOffset));

  // The builtin expects the parameter to be in register param = a0.

  // The first valuetype of the array is the return's valuetype.
  __ Lw(valuetype, MemOperand(valuetypes_array_ptr, 0));

  Label return_kWasmI32;
  Label return_kWasmI32_not_smi;
  Label return_kWasmI64;
  Label return_kWasmF32;
  Label return_kWasmF64;
  Label return_kWasmRef;

  // Prepare for builtin call.

  __ Branch(&return_kWasmI32, eq, valuetype,
            Operand(wasm::kWasmI32.raw_bit_field()));
  __ Branch(&return_kWasmI64, eq, valuetype,
            Operand(wasm::kWasmI64.raw_bit_field()));
  __ Branch(&return_kWasmF32, eq, valuetype,
            Operand(wasm::kWasmF32.raw_bit_field()));
  __ Branch(&return_kWasmF64, eq, valuetype,
            Operand(wasm::kWasmF64.raw_bit_field()));

  // Test bit 0 (reference type indicator): valuetype & 1
  {
    Register tmp = scratch;
    __ And(tmp, valuetype, Operand(1));
    __ Branch(&return_kWasmRef, ne, tmp, Operand(zero_reg));
  }

  // Invalid type. JavaScript cannot return Simd results to WebAssembly.
  __ DebugBreak();

  __ bind(&return_kWasmI32);
  __ JumpIfNotSmi(return_reg, &return_kWasmI32_not_smi);
  // Change the param from Smi to int32.
  __ SmiUntag(return_reg);
  __ AssertZeroExtended(return_reg);
  __ LoadWord(packed_args, MemOperand(fp, kPackedArrayOffset));
  __ AddWord(scratch, packed_args, current_result_offset);
  __ Sw(return_reg, MemOperand(scratch, 0));
  __ AddWord(current_result_offset, current_result_offset,
             Operand(sizeof(int32_t)));
  __ Branch(&return_done);

  __ bind(&return_kWasmI32_not_smi);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedNonSmiToInt32),
          RelocInfo::CODE_TARGET);
  __ AssertZeroExtended(return_reg);
  __ LoadWord(packed_args, MemOperand(fp, kPackedArrayOffset));
  __ LoadWord(current_result_offset, MemOperand(fp, kCurrentResultOffset));
  __ AddWord(scratch, packed_args, current_result_offset);
  __ Sw(return_reg, MemOperand(scratch, 0));
  __ AddWord(current_result_offset, current_result_offset,
             Operand(sizeof(int32_t)));
  __ Branch(&return_done);

  __ bind(&return_kWasmI64);
  __ Call(BUILTIN_CODE(masm->isolate(), BigIntToI64), RelocInfo::CODE_TARGET);
  __ LoadWord(packed_args, MemOperand(fp, kPackedArrayOffset));
  __ LoadWord(current_result_offset, MemOperand(fp, kCurrentResultOffset));
  __ AddWord(scratch, packed_args, current_result_offset);
  __ StoreWord(return_reg, MemOperand(scratch, 0));
  __ AddWord(current_result_offset, current_result_offset,
             Operand(sizeof(int64_t)));
  __ Branch(&return_done);

  __ bind(&return_kWasmF32);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedToFloat32),
          RelocInfo::CODE_TARGET);
  __ LoadWord(packed_args, MemOperand(fp, kPackedArrayOffset));
  __ LoadWord(current_result_offset, MemOperand(fp, kCurrentResultOffset));
  __ AddWord(scratch, packed_args, current_result_offset);
  __ StoreFloat(kFPReturnRegister0, MemOperand(scratch, 0));
  __ AddWord(current_result_offset, current_result_offset,
             Operand(sizeof(float)));
  __ Branch(&return_done);

  __ bind(&return_kWasmF64);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedToFloat64),
          RelocInfo::CODE_TARGET);
  __ LoadWord(packed_args, MemOperand(fp, kPackedArrayOffset));
  __ LoadWord(current_result_offset, MemOperand(fp, kCurrentResultOffset));
  __ AddWord(scratch, packed_args, current_result_offset);
  __ StoreDouble(kFPReturnRegister0, MemOperand(scratch, 0));
  __ AddWord(current_result_offset, current_result_offset,
             Operand(sizeof(double)));
  __ Branch(&return_done);

  __ bind(&return_kWasmRef);
  __ LoadWord(packed_args, MemOperand(fp, kPackedArrayOffset));
  __ AddWord(scratch, packed_args, current_result_offset);
  __ StoreWord(return_reg, MemOperand(scratch, 0));
  __ AddWord(current_result_offset, current_result_offset,
             Operand(kSystemPointerSize));

  // A result converted.
  __ bind(&return_done);

  // Restore after builtin call
  __ LoadWord(context, MemOperand(fp, kContextOffset));
  __ LoadWord(fixed_array, MemOperand(sp, kSystemPointerSize));
  __ LoadWord(valuetypes_array_ptr,
              MemOperand(fp, kValueTypesArrayStartOffset));
  __ AddWord(valuetypes_array_ptr, valuetypes_array_ptr,
             Operand(kValueTypeSize));
  __ LoadWord(result_index, MemOperand(fp, kResultIndexOffset));
  __ AddWord(result_index, result_index, Operand(1));
  __ LoadWord(scratch, MemOperand(fp, kReturnCountOffset));
  __ Branch(&loop_copy_return_refs, ge, result_index, Operand(scratch));

  __ AddWord(scratch, fixed_array,
             OFFSET_OF_DATA_START(FixedArray) - kHeapObjectTag);
  DEFINE_REG(scratch2);
  __ slli(scratch2, result_index, kTaggedSizeLog2);
  __ AddWord(scratch, scratch, scratch2);
  __ LoadTaggedField(return_reg, MemOperand(scratch, 0));
  __ Branch(&convert_return);

  // -------------------------------------------
  // Update refs after calling all builtins.
  // -------------------------------------------

  // Some builtin calls for return value conversion may trigger GC, and some
  // heap pointers of ref types might become invalid in the conversion loop.
  // Thus, copy the ref values again after finishing all the conversions.
  __ bind(&loop_copy_return_refs);

  // If there is only one return value, there should be no heap pointer in the
  // packed_args while calling any builtin. So, we don't need to update refs.
  __ LoadWord(return_count, MemOperand(fp, kReturnCountOffset));
  __ Branch(&all_done, eq, return_count, Operand(1));

  Label copy_return_if_ref, copy_return_ref, done_copy_return_ref;
  __ LoadWord(packed_args, MemOperand(fp, kPackedArrayOffset));
  __ LoadWord(signature, MemOperand(fp, kSignatureOffset));
  __ LoadWord(valuetypes_array_ptr,
              MemOperand(signature, wasm::FunctionSig::kRepsOffset));
  __ Move(result_index, zero_reg);
  __ Move(current_result_offset, zero_reg);

  // Copy if the current return value is a ref type.
  __ bind(&copy_return_if_ref);
  __ Lw(valuetype, MemOperand(valuetypes_array_ptr, 0));

  // Test bit 0 (reference type indicator): valuetype & 1
  {
    Register tmp = scratch;
    __ And(tmp, valuetype, Operand(1));
    __ Branch(&copy_return_ref, ne, tmp, Operand(zero_reg));
  }

  Label inc_result_32bit;
  __ Branch(&inc_result_32bit, eq, valuetype,
            Operand(wasm::kWasmI32.raw_bit_field()));
  __ Branch(&inc_result_32bit, eq, valuetype,
            Operand(wasm::kWasmF32.raw_bit_field()));

  Label inc_result_64bit;
  __ Branch(&inc_result_64bit, eq, valuetype,
            Operand(wasm::kWasmI64.raw_bit_field()));
  __ Branch(&inc_result_64bit, eq, valuetype,
            Operand(wasm::kWasmF64.raw_bit_field()));

  // Invalid type. JavaScript cannot return Simd values to WebAssembly.
  __ DebugBreak();

  __ bind(&inc_result_32bit);
  __ AddWord(current_result_offset, current_result_offset,
             Operand(sizeof(int32_t)));
  __ Branch(&done_copy_return_ref);

  __ bind(&inc_result_64bit);
  __ AddWord(current_result_offset, current_result_offset,
             Operand(sizeof(int64_t)));
  __ Branch(&done_copy_return_ref);

  __ bind(&copy_return_ref);
  __ AddWord(scratch, fixed_array,
             OFFSET_OF_DATA_START(FixedArray) - kHeapObjectTag);
  __ slli(scratch2, result_index, kTaggedSizeLog2);
  __ AddWord(scratch, scratch, scratch2);
  __ LoadTaggedField(return_reg, MemOperand(scratch, 0));
  __ AddWord(scratch, packed_args, current_result_offset);
  __ StoreWord(return_reg, MemOperand(scratch, 0));
  __ AddWord(current_result_offset, current_result_offset,
             Operand(kSystemPointerSize));

  // Move pointers.
  __ bind(&done_copy_return_ref);
  __ AddWord(valuetypes_array_ptr, valuetypes_array_ptr,
             Operand(kValueTypeSize));
  __ AddWord(result_index, result_index, Operand(1));
  __ Branch(&copy_return_if_ref, lt, result_index, Operand(return_count));

  // -------------------------------------------
  // All done.
  // -------------------------------------------

  __ bind(&all_done);

  // Deconstruct the stack frame.
  __ LeaveFrame(StackFrame::WASM_TO_JS);

  __ li(a0, WasmToJSInterpreterFrameConstants::kSuccess);
  __ Ret();
}

#endif  // V8_ENABLE_WEBASSEMBLY

#undef __

}  // namespace internal
}  // namespace v8

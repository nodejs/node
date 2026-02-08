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

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/interpreter/wasm-interpreter-runtime.h"
#include "src/wasm/object-access.h"
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

#if V8_ENABLE_WEBASSEMBLY

namespace {
// Helper functions for the GenericJSToWasmInterpreterWrapper.

void PrepareForJsToWasmConversionBuiltinCall(MacroAssembler* masm,
                                             Register current_param_slot,
                                             Register valuetypes_array_ptr,
                                             Register wasm_instance,
                                             Register function_data) {
  UseScratchRegisterScope temps(masm);
  Register GCScanCount = temps.Acquire();
  // Pushes and puts the values in order onto the stack before builtin calls for
  // the GenericJSToWasmInterpreterWrapper.
  // The last two slots contain tagged objects that need to be visited during
  // GC.
  __ li(GCScanCount, 2);
  __ StoreWord(
      GCScanCount,
      MemOperand(
          fp, BuiltinWasmInterpreterWrapperConstants::kGCScanSlotCountOffset));
  __ Push(current_param_slot, valuetypes_array_ptr, wasm_instance,
          function_data);
  // We had to prepare the parameters for the Call: we have to put the context
  // into cp(s7).
  Register wasm_trusted_instance = wasm_instance;
  __ LoadTrustedPointerField(
      wasm_trusted_instance,
      FieldMemOperand(wasm_instance, WasmInstanceObject::kTrustedDataOffset),
      kWasmTrustedInstanceDataIndirectPointerTag);
  __ LoadTaggedField(
      kContextRegister,  // cp(s7)
      MemOperand(wasm_trusted_instance,
                 wasm::ObjectAccess::ToTagged(
                     WasmTrustedInstanceData::kNativeContextOffset)));
}

void RestoreAfterJsToWasmConversionBuiltinCall(MacroAssembler* masm,
                                               Register function_data,
                                               Register wasm_instance,
                                               Register valuetypes_array_ptr,
                                               Register current_param_slot) {
  // Pop and load values from the stack in order into the registers after
  // builtin calls for the GenericJSToWasmInterpreterWrapper.
  __ Pop(current_param_slot, valuetypes_array_ptr, wasm_instance,
         function_data);
  __ StoreWord(
      zero_reg,
      MemOperand(
          fp, BuiltinWasmInterpreterWrapperConstants::kGCScanSlotCountOffset));
}

void PrepareForBuiltinCall(MacroAssembler* masm, Register array_start,
                           Register return_count, Register wasm_instance) {
  UseScratchRegisterScope temps(masm);
  Register GCScanCount = temps.Acquire();
  // Pushes and puts the values in order onto the stack before builtin calls for
  // the GenericJSToWasmInterpreterWrapper.
  __ li(GCScanCount, 1);
  __ StoreWord(
      GCScanCount,
      MemOperand(
          fp, BuiltinWasmInterpreterWrapperConstants::kGCScanSlotCountOffset));
  // The last slot contains a tagged object that need to be visited during GC.
  __ Push(array_start, return_count, zero_reg, wasm_instance);
  // We had to prepare the parameters for the Call: we have to put the context
  // into s7.
  Register wasm_trusted_instance = wasm_instance;
  __ LoadTrustedPointerField(
      wasm_trusted_instance,
      FieldMemOperand(wasm_instance, WasmInstanceObject::kTrustedDataOffset),
      kWasmTrustedInstanceDataIndirectPointerTag);
  __ LoadTaggedField(
      kContextRegister,  // cp(s7)
      MemOperand(wasm_trusted_instance,
                 wasm::ObjectAccess::ToTagged(
                     WasmTrustedInstanceData::kNativeContextOffset)));
}

void RestoreAfterBuiltinCall(MacroAssembler* masm, Register wasm_instance,
                             Register return_count, Register array_start) {
  // Pop and load values from the stack in order into the registers after
  // builtin calls for the GenericJSToWasmInterpreterWrapper.
  __ Pop(array_start, return_count, zero_reg, wasm_instance);
}

void PrepareForWasmToJsConversionBuiltinCall(
    MacroAssembler* masm, Register return_count, Register result_index,
    Register current_return_slot, Register valuetypes_array_ptr,
    Register wasm_instance, Register fixed_array, Register jsarray,
    bool load_native_context = true) {
  UseScratchRegisterScope temps(masm);
  Register GCScanCount = temps.Acquire();
  // Pushes and puts the values in order onto the stack before builtin calls
  // for the GenericJSToWasmInterpreterWrapper.
  // The last three slots contain tagged objects that need to be visited during
  // GC.
  __ li(GCScanCount, 3);
  __ StoreWord(
      GCScanCount,
      MemOperand(
          fp, BuiltinWasmInterpreterWrapperConstants::kGCScanSlotCountOffset));
  __ Push(return_count, result_index, current_return_slot, valuetypes_array_ptr,
          zero_reg, wasm_instance, fixed_array, jsarray);
  if (load_native_context) {
    // Put the context into s7.
    Register wasm_trusted_instance = wasm_instance;
    __ LoadTrustedPointerField(
        wasm_trusted_instance,
        FieldMemOperand(wasm_instance, WasmInstanceObject::kTrustedDataOffset),
        kWasmTrustedInstanceDataIndirectPointerTag);
    __ LoadTaggedField(
        kContextRegister,  // cp(s7)
        MemOperand(wasm_trusted_instance,
                   wasm::ObjectAccess::ToTagged(
                       WasmTrustedInstanceData::kNativeContextOffset)));
  }
}

void RestoreAfterWasmToJsConversionBuiltinCall(
    MacroAssembler* masm, Register jsarray, Register fixed_array,
    Register wasm_instance, Register valuetypes_array_ptr,
    Register current_return_slot, Register result_index,
    Register return_count) {
  // Pop and load values from the stack in order into the registers after
  // builtin calls for the GenericJSToWasmInterpreterWrapper.
  __ Pop(return_count, result_index, current_return_slot, valuetypes_array_ptr,
         zero_reg, wasm_instance, fixed_array, jsarray);
  __ StoreWord(
      zero_reg,
      MemOperand(
          fp, BuiltinWasmInterpreterWrapperConstants::kGCScanSlotCountOffset));
}

}  // namespace

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
  //  s9: array_start
  //  s10: function_index
  auto regs = RegisterAllocator::WithAllocatableGeneralRegisters();

  DEFINE_PINNED(array_start, s9);
  DEFINE_PINNED(function_index, s10);

  // Set up the stackframe:
  //
  // fp-0x10  Marker(StackFrame::WASM_INTERPRETER_ENTRY)
  // fp       Old fp
  // fp_0x08  ra
  __ EnterFrame(StackFrame::WASM_INTERPRETER_ENTRY);

  // Push args
  __ Push(kWasmImplicitArgRegister, function_index, array_start);
  __ Move(kWasmImplicitArgRegister, zero_reg);
  __ CallRuntime(Runtime::kWasmRunInterpreter, 3);

  // Deconstruct the stack frame.
  __ LeaveFrame(StackFrame::WASM_INTERPRETER_ENTRY);
  __ Ret();
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
  __ LoadTrustedPointerField(
      function_data,
      FieldMemOperand(shared_function_info,
                      SharedFunctionInfo::kTrustedFunctionDataOffset),
      kWasmFunctionDataIndirectPointerTag);
  shared_function_info = no_reg;

  Register trusted_instance_data = wasm_instance;
#if V8_ENABLE_SANDBOX
  __ DecompressProtected(
      trusted_instance_data,
      MemOperand(function_data,
                 WasmExportedFunctionData::kProtectedInstanceDataOffset -
                     kHeapObjectTag));
#else
  __ LoadTaggedField(
      trusted_instance_data,
      MemOperand(function_data,
                 WasmExportedFunctionData::kProtectedInstanceDataOffset -
                     kHeapObjectTag));
#endif
  __ LoadTaggedField(
      wasm_instance,
      FieldMemOperand(trusted_instance_data,
                      WasmTrustedInstanceData::kInstanceObjectOffset));
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
                      WasmExportedFunctionData::kPackedArgsSizeOffset));
  __ SmiToInt32(signature_data);

  Register internal_function = valuetypes_array_ptr;
  __ LoadProtectedPointerField(
      internal_function,
      MemOperand(
          function_data,
          WasmExportedFunctionData::kProtectedInternalOffset - kHeapObjectTag));

  Register signature = internal_function;
  __ LoadWord(signature,
              MemOperand(internal_function,
                         WasmInternalFunction::kSigOffset - kHeapObjectTag));
  LoadFromSignature(masm, valuetypes_array_ptr, return_count, param_count);
}

// TODO(paolosev@microsoft.com): this should be converted into a Torque builtin,
// like it was done for GenericJSToWasmWrapper.
void Builtins::Generate_GenericJSToWasmInterpreterWrapper(
    MacroAssembler* masm) {
  auto regs = RegisterAllocator::WithAllocatableGeneralRegisters();
  // Set up the stackframe.
  __ EnterFrame(StackFrame::JS_TO_WASM);

  // -------------------------------------------
  // Compute offsets and prepare for GC.
  // -------------------------------------------
  // GenericJSToWasmInterpreterWrapperFrame:
  // fp-N     Args/retvals array for Wasm call
  // ...       ...
  // fp-0x58  SignatureData
  // fp-0x50  CurrentIndex
  // fp-0x48  ArgRetsIsArgs
  // fp-0x40  ArgRetsAddress
  // fp-0x38  ValueTypesArray
  // fp-0x30  SigReps
  // fp-0x28  ReturnCount
  // fp-0x20  ParamCount
  // fp-0x18  InParamCount
  // fp-0x10  GCScanSlotCount
  // fp-0x08  Marker(StackFrame::JS_TO_WASM)
  // fp       Old RBP

  constexpr int kMarkerOffset =
      BuiltinWasmInterpreterWrapperConstants::kGCScanSlotCountOffset +
      kSystemPointerSize;
  // The number of parameters passed to this function.
  constexpr int kInParamCountOffset =
      BuiltinWasmInterpreterWrapperConstants::kGCScanSlotCountOffset -
      kSystemPointerSize;
  // The number of parameters according to the signature.
  constexpr int kParamCountOffset =
      BuiltinWasmInterpreterWrapperConstants::kParamCountOffset;
  constexpr int kReturnCountOffset =
      BuiltinWasmInterpreterWrapperConstants::kReturnCountOffset;
  constexpr int kSigRepsOffset =
      BuiltinWasmInterpreterWrapperConstants::kSigRepsOffset;
  constexpr int kValueTypesArrayStartOffset =
      BuiltinWasmInterpreterWrapperConstants::kValueTypesArrayStartOffset;
  // Array for arguments and return values. They will be scanned by GC.
  constexpr int kArgRetsAddressOffset =
      BuiltinWasmInterpreterWrapperConstants::kArgRetsAddressOffset;
  // Arg/Return arrays use the same stack address. So, we should keep a flag
  // whether we are using the array for args or returns. (1 = Args, 0 = Rets)
  constexpr int kArgRetsIsArgsOffset =
      BuiltinWasmInterpreterWrapperConstants::kArgRetsIsArgsOffset;
  // The index of the argument being converted.
  constexpr int kCurrentIndexOffset =
      BuiltinWasmInterpreterWrapperConstants::kCurrentIndexOffset;
  // Precomputed signature data, a uint32_t with the format:
  // bit 0-14: PackedArgsSize
  // bit 15:   HasRefArgs
  // bit 16:   HasRefRets
  constexpr int kSignatureDataOffset =
      BuiltinWasmInterpreterWrapperConstants::kSignatureDataOffset;
  // We set and use this slot only when moving parameters into the parameter
  // registers (so no GC scan is needed).
  constexpr int kNumSpillSlots =
      (kMarkerOffset - kSignatureDataOffset) / kSystemPointerSize;
  constexpr int kNum16BytesAlignedSpillSlots = 2 * ((kNumSpillSlots + 1) / 2);

  __ SubWord(sp, sp,
             Operand(kNum16BytesAlignedSpillSlots * kSystemPointerSize));
  // Put the in_parameter count on the stack, we only  need it at the very end
  // when we pop the parameters off the stack.
  __ SubWord(kJavaScriptCallArgCountRegister, kJavaScriptCallArgCountRegister,
             1);
  __ StoreWord(kJavaScriptCallArgCountRegister,
               MemOperand(fp, kInParamCountOffset));

  // -------------------------------------------
  // Load the Wasm exported function data and the Wasm instance.
  // -------------------------------------------
  DEFINE_PINNED(function_data, kJSFunctionRegister);       // a1
  DEFINE_PINNED(wasm_instance, kWasmImplicitArgRegister);  // a7
  LoadFunctionDataAndWasmInstance(masm, function_data, wasm_instance);

  regs.ResetExcept(function_data, wasm_instance);

  // -------------------------------------------
  // Load values from the signature.
  // -------------------------------------------

  // Param should be a0 for calling Runtime in the conversion loop.
  DEFINE_PINNED(param, a0);
  // These registers stays alive until we load params to param registers.
  // To prevent aliasing assign higher register here.
  DEFINE_PINNED(valuetypes_array_ptr, t1);

  DEFINE_REG(return_count);
  DEFINE_REG(param_count);
  DEFINE_REG(signature_data);
  DEFINE_REG(scratch);

  // -------------------------------------------
  // Load values from the signature.
  // -------------------------------------------
  LoadValueTypesArray(masm, function_data, valuetypes_array_ptr, return_count,
                      param_count, signature_data);
  __ StoreWord(signature_data, MemOperand(fp, kSignatureDataOffset));
  Register array_size = signature_data;
  __ And(array_size, array_size,
         Operand(wasm::WasmInterpreterRuntime::PackedArgsSizeField::kMask));
  // -------------------------------------------
  // Store signature-related values to the stack.
  // -------------------------------------------
  // We store values on the stack to restore them after function calls.
  // We cannot push values onto the stack right before the wasm call. The wasm
  // function expects the parameters, that didn't fit into the registers, on the
  // top of the stack.
  __ StoreWord(param_count, MemOperand(fp, kParamCountOffset));
  __ StoreWord(return_count, MemOperand(fp, kReturnCountOffset));
  __ StoreWord(valuetypes_array_ptr, MemOperand(fp, kSigRepsOffset));
  __ StoreWord(valuetypes_array_ptr,
               MemOperand(fp, kValueTypesArrayStartOffset));

  // -------------------------------------------
  // Allocate array for args and return value.
  // -------------------------------------------

  // Leave space for WasmInstance.
  __ AddWord(array_size, array_size, Operand(kSystemPointerSize));
  // Ensure that the array is 16-bytes aligned.
  __ AddWord(scratch, array_size, Operand(8));
  __ And(array_size, scratch, Operand(-16));

  DEFINE_PINNED(array_start, s9);
  __ SubWord(array_start, sp, array_size);
  __ mv(sp, array_start);

  __ li(scratch, 1);
  __ StoreWord(scratch, MemOperand(fp, kArgRetsIsArgsOffset));

  __ StoreWord(zero_reg, MemOperand(fp, kCurrentIndexOffset));

  // Set the current_param_slot to point to the start of the section, after the
  // WasmInstance object.
  DEFINE_REG(current_param_slot);
  __ AddWord(current_param_slot, array_start, Operand(kSystemPointerSize));
  __ StoreWord(current_param_slot, MemOperand(fp, kArgRetsAddressOffset));

  Label prepare_for_wasm_call;
  // If we have 0 params: jump through parameter handling.
  __ Branch(&prepare_for_wasm_call, eq, param_count, Operand(0));

  // Create a section on the stack to pass the evaluated parameters to the
  // interpreter and to receive the results. This section represents the array
  // expected as argument by the Runtime_WasmRunInterpreter.
  // Arguments are stored one after the other without holes, starting at the
  // beginning of the array, and the interpreter puts the returned values in the
  // same array, also starting at the beginning.

  // Loop through the params starting with the first.
  // 'fp + kFPOnStackSize + kPCOnStackSize + kReceiverOnStackSize' points to the
  // first JS parameter we are processing.

  // We have to check the types of the params. The ValueType array contains
  // first the return then the param types.

  // Set the ValueType array pointer to point to the first parameter.
  constexpr int kValueTypeSize = sizeof(wasm::ValueType);
  static_assert(kValueTypeSize == 4);
  const int32_t kValueTypeSizeLog2 = log2(kValueTypeSize);
  // Set the ValueType array pointer to point to the first parameter.
  __ slli(scratch, return_count, kValueTypeSizeLog2);
  __ AddWord(valuetypes_array_ptr, valuetypes_array_ptr, Operand(scratch));

  DEFINE_REG(current_index);
  __ mv(current_index, zero_reg);

  // -------------------------------------------
  // Param evaluation loop.
  // -------------------------------------------
  Label loop_through_params;
  __ bind(&loop_through_params);

  constexpr int kReceiverOnStackSize = kSystemPointerSize;
  constexpr int kArgsOffset =
      kFPOnStackSize + kPCOnStackSize + kReceiverOnStackSize;
  // Read JS argument into 'param'.
  DEFINE_REG(scratch2);
  __ AddWord(scratch, fp, kArgsOffset);
  __ SllWord(scratch2, current_index, kSystemPointerSizeLog2);
  __ AddWord(scratch, scratch, scratch2);
  __ LoadWord(param, MemOperand(scratch, 0));
  __ StoreWord(current_index, MemOperand(fp, kCurrentIndexOffset));

  DEFINE_REG(valuetype);
  __ Lw(valuetype, MemOperand(valuetypes_array_ptr, 0));

  // -------------------------------------------
  // Param conversion.
  // -------------------------------------------
  // If param is a Smi we can easily convert it. Otherwise we'll call a builtin
  // for conversion.
  Label param_conversion_done;
  Label check_ref_param;
  Label convert_param;
  __ Branch(&check_ref_param, ne, valuetype,
            Operand(wasm::kWasmI32.raw_bit_field()));
  __ JumpIfNotSmi(param, &convert_param);

  // Change the param from Smi to int32.
  __ SmiUntag(param);
  // Place the param into the proper slot in Integer section.
  __ Sw(param, MemOperand(current_param_slot, 0));
  __ AddWord(current_param_slot, current_param_slot, Operand(sizeof(int32_t)));
  __ Branch(&param_conversion_done);

  Label handle_ref_param;
  __ bind(&check_ref_param);

  // wasm::ValueKind::kRefNull is not representable as a cmp Operand operand.
  __ Branch(&convert_param, eq, valuetype, Operand(1));

  // Place the reference param into the proper slot.
  __ bind(&handle_ref_param);
  // Make sure slot for ref args are 64-bit aligned.
  __ And(scratch, current_param_slot, Operand(0x04));
  __ AddWord(current_param_slot, current_param_slot, scratch);
  __ StoreWord(param, MemOperand(current_param_slot, 0));
  __ AddWord(current_param_slot, current_param_slot,
             Operand(kSystemPointerSize));

  // -------------------------------------------
  // Param conversion done.
  // -------------------------------------------
  __ bind(&param_conversion_done);

  __ AddWord(valuetypes_array_ptr, valuetypes_array_ptr, kValueTypeSize);

  __ LoadWord(current_index, MemOperand(fp, kCurrentIndexOffset));
  __ LoadWord(scratch, MemOperand(fp, kParamCountOffset));
  __ AddWord(current_index, current_index, 1);
  __ Branch(&loop_through_params, lt, current_index, Operand(scratch));
  __ StoreWord(current_index, MemOperand(fp, kCurrentIndexOffset));
  __ Branch(&prepare_for_wasm_call);

  // -------------------------------------------
  // Param conversion builtins.
  // -------------------------------------------
  __ bind(&convert_param);
  // The order of pushes is important. We want the heap objects, that should be
  // scanned by GC, to be on the top of the stack.
  // We have to set the indicating value for the GC to the number of values on
  // the top of the stack that have to be scanned before calling the builtin
  // function.
  // We don't need the JS context for these builtin calls.
  // The builtin expects the parameter to be in register param = rax.

  PrepareForJsToWasmConversionBuiltinCall(masm, current_param_slot,
                                          valuetypes_array_ptr, wasm_instance,
                                          function_data);

  Label param_kWasmI32_not_smi, param_kWasmI64, param_kWasmF32, param_kWasmF64,
      throw_type_error;

  __ Branch(&param_kWasmI32_not_smi, eq, valuetype,
            Operand(wasm::kWasmI32.raw_bit_field()));
  __ Branch(&param_kWasmI64, eq, valuetype,
            Operand(wasm::kWasmI64.raw_bit_field()));
  __ Branch(&param_kWasmF32, eq, valuetype,
            Operand(wasm::kWasmF32.raw_bit_field()));
  __ Branch(&param_kWasmF64, eq, valuetype,
            Operand(wasm::kWasmF64.raw_bit_field()));
  // Simd arguments cannot be passed from JavaScript.
  __ Branch(&throw_type_error, eq, valuetype,
            Operand(wasm::kWasmS128.raw_bit_field()));

  // Invalid type.
  __ DebugBreak();

  __ bind(&param_kWasmI32_not_smi);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedNonSmiToInt32),
          RelocInfo::CODE_TARGET);
  // Param is the result of the builtin.
  RestoreAfterJsToWasmConversionBuiltinCall(masm, function_data, wasm_instance,
                                            valuetypes_array_ptr,
                                            current_param_slot);
  __ Sw(param, MemOperand(current_param_slot, 0));
  __ AddWord(current_param_slot, current_param_slot, Operand(sizeof(int32_t)));
  __ Branch(&param_conversion_done);

  __ bind(&param_kWasmI64);
  __ Call(BUILTIN_CODE(masm->isolate(), BigIntToI64), RelocInfo::CODE_TARGET);
  RestoreAfterJsToWasmConversionBuiltinCall(masm, function_data, wasm_instance,
                                            valuetypes_array_ptr,
                                            current_param_slot);
  __ StoreWord(param, MemOperand(current_param_slot, 0));
  __ AddWord(current_param_slot, current_param_slot, Operand(sizeof(int64_t)));
  __ Branch(&param_conversion_done);

  __ bind(&param_kWasmF32);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedToFloat32),
          RelocInfo::CODE_TARGET);
  RestoreAfterJsToWasmConversionBuiltinCall(masm, function_data, wasm_instance,
                                            valuetypes_array_ptr,
                                            current_param_slot);
  __ StoreFloat(kFPReturnRegister0, MemOperand(current_param_slot, 0));
  __ AddWord(current_param_slot, current_param_slot, Operand(sizeof(float)));
  __ Branch(&param_conversion_done);

  __ bind(&param_kWasmF64);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmTaggedToFloat64),
          RelocInfo::CODE_TARGET);
  RestoreAfterJsToWasmConversionBuiltinCall(masm, function_data, wasm_instance,
                                            valuetypes_array_ptr,
                                            current_param_slot);
  __ StoreDouble(kFPReturnRegister0, MemOperand(current_param_slot, 0));
  __ AddWord(current_param_slot, current_param_slot, Operand(sizeof(double)));
  __ Branch(&param_conversion_done);

  __ bind(&throw_type_error);
  // CallRuntime expects kRootRegister (x26) to contain the root.
  __ CallRuntime(Runtime::kWasmThrowJSTypeError);
  __ DebugBreak();  // Should not return.

  // -------------------------------------------
  // Prepare for the Wasm call.
  // -------------------------------------------

  regs.ResetExcept(function_data, wasm_instance, array_start, scratch);

  __ bind(&prepare_for_wasm_call);

  DEFINE_PINNED(function_index, s10);
  __ Lw(
      function_index,
      MemOperand(function_data, WasmExportedFunctionData::kFunctionIndexOffset -
                                    kHeapObjectTag));
  // We pass function_index as Smi.

  // One tagged object (the wasm_instance) to be visited if there is a GC
  // during the call.
  constexpr int kWasmCallGCScanSlotCount = 1;
  __ li(scratch, Operand(kWasmCallGCScanSlotCount));
  __ StoreWord(
      scratch,
      MemOperand(
          fp, BuiltinWasmInterpreterWrapperConstants::kGCScanSlotCountOffset));

  // -------------------------------------------
  // Call the Wasm function.
  // -------------------------------------------

  // Here array_start == sp.
  __ StoreWord(wasm_instance, MemOperand(sp));
  // Skip wasm_instance.
  __ LoadWord(array_start, MemOperand(fp, kArgRetsAddressOffset));
  // Here array_start == sp + kSystemPointerSize.
  __ Call(BUILTIN_CODE(masm->isolate(), WasmInterpreterEntry),
          RelocInfo::CODE_TARGET);
  __ LoadWord(wasm_instance, MemOperand(sp));
  __ LoadWord(array_start, MemOperand(fp, kArgRetsAddressOffset));

  __ StoreWord(zero_reg, MemOperand(fp, kArgRetsIsArgsOffset));

  regs.ResetExcept(wasm_instance, array_start, scratch);

  // -------------------------------------------
  // Return handling.
  // -------------------------------------------
  DEFINE_PINNED(return_value, kReturnRegister0);  // a0
  ASSIGN_REG(return_count);
  __ LoadWord(return_count, MemOperand(fp, kReturnCountOffset));

  // All return values are already in the packed array.
  __ StoreWord(
      return_count,
      MemOperand(fp,
                 BuiltinWasmInterpreterWrapperConstants::kCurrentIndexOffset));

  DEFINE_PINNED(fixed_array, t4);
  __ mv(fixed_array, zero_reg);
  DEFINE_REG(jsarray);
  __ mv(jsarray, zero_reg);

  Label all_results_conversion_done, start_return_conversion, return_jsarray;

  __ Branch(&start_return_conversion, eq, return_count, Operand(1));
  __ Branch(&return_jsarray, gt, return_count, Operand(1));

  // If no return value, load undefined.
  __ LoadRoot(return_value, RootIndex::kUndefinedValue);
  __ Branch(&all_results_conversion_done);

  // If we have more than one return value, we need to return a JSArray.
  __ bind(&return_jsarray);
  PrepareForBuiltinCall(masm, array_start, return_count, wasm_instance);
  __ mv(return_value, return_count);
  __ SmiTag(return_value);

  // Create JSArray to hold results.
  __ Call(BUILTIN_CODE(masm->isolate(), WasmAllocateJSArray),
          RelocInfo::CODE_TARGET);
  __ mv(jsarray, return_value);

  RestoreAfterBuiltinCall(masm, wasm_instance, return_count, array_start);
  __ LoadTaggedField(fixed_array, MemOperand(jsarray, JSArray::kElementsOffset -
                                                          kHeapObjectTag));

  __ bind(&start_return_conversion);
  Register current_return_slot = array_start;
  __ LoadWord(current_return_slot, MemOperand(fp, kArgRetsAddressOffset));

  DEFINE_PINNED(result_index, s1);
  __ mv(result_index, zero_reg);

  ASSIGN_REG(valuetypes_array_ptr);
  __ LoadWord(valuetypes_array_ptr,
              MemOperand(fp, kValueTypesArrayStartOffset));

  // -------------------------------------------
  // Return conversions.
  // -------------------------------------------
  Label convert_return_value;
  __ bind(&convert_return_value);
  // We have to make sure that the kGCScanSlotCount is set correctly when we
  // call the builtins for conversion. For these builtins it's the same as for
  // the Wasm call, that is, kGCScanSlotCount = 0, so we don't have to reset it.
  // We don't need the JS context for these builtin calls.

  // The first valuetype of the array is the return's valuetype.
  ASSIGN_REG(valuetype);
  __ Lw(valuetype, MemOperand(valuetypes_array_ptr, 0));

  Label return_kWasmI32, return_kWasmI64, return_kWasmF32, return_kWasmF64,
      return_kWasmRef;

  __ Branch(&return_kWasmI32, eq, valuetype,
            Operand(wasm::kWasmI32.raw_bit_field()));
  __ Branch(&return_kWasmI64, eq, valuetype,
            Operand(wasm::kWasmI64.raw_bit_field()));
  __ Branch(&return_kWasmF32, eq, valuetype,
            Operand(wasm::kWasmF32.raw_bit_field()));
  __ Branch(&return_kWasmF64, eq, valuetype,
            Operand(wasm::kWasmF64.raw_bit_field()));

  {
    __ Branch(&return_kWasmRef, ne, valuetype, Operand(1));
    // Invalid type. Wasm cannot return Simd results to JavaScript.
    __ DebugBreak();
  }

  Label return_value_done;

  Label to_heapnumber;
  {
    __ bind(&return_kWasmI32);
    __ LoadWord(return_value, MemOperand(current_return_slot, 0));
    __ AddWord(current_return_slot, current_return_slot,
               Operand(sizeof(int32_t)));
    // If pointer compression is disabled, we can convert the return to a smi.
    if (SmiValuesAre32Bits()) {
      __ SmiTag(return_value);
    } else {
      // Double the return value to test if it can be a Smi.
      __ Add32(scratch, return_value, return_value);
      __ Xor(scratch, return_value, Operand(scratch));
      // If there was overflow, convert the return value to a HeapNumber.
      __ Branch(&to_heapnumber, lt, scratch, Operand(0));
      // If there was no overflow, we can convert to Smi.
      __ SmiTag(return_value);
    }
  }
  __ Branch(&return_value_done);

  // Handle the conversion of the I32 return value to HeapNumber when it cannot
  // be a smi.
  __ bind(&to_heapnumber);

  PrepareForWasmToJsConversionBuiltinCall(
      masm, return_count, result_index, current_return_slot,
      valuetypes_array_ptr, wasm_instance, fixed_array, jsarray);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmInt32ToHeapNumber),
          RelocInfo::CODE_TARGET);
  RestoreAfterWasmToJsConversionBuiltinCall(
      masm, jsarray, fixed_array, wasm_instance, valuetypes_array_ptr,
      current_return_slot, result_index, return_count);
  __ Branch(&return_value_done);

  __ bind(&return_kWasmI64);
  __ LoadWord(return_value, MemOperand(current_return_slot, 0));
  __ AddWord(current_return_slot, current_return_slot,
             Operand(sizeof(int64_t)));
  PrepareForWasmToJsConversionBuiltinCall(
      masm, return_count, result_index, current_return_slot,
      valuetypes_array_ptr, wasm_instance, fixed_array, jsarray);
  __ Call(BUILTIN_CODE(masm->isolate(), I64ToBigInt), RelocInfo::CODE_TARGET);
  RestoreAfterWasmToJsConversionBuiltinCall(
      masm, jsarray, fixed_array, wasm_instance, valuetypes_array_ptr,
      current_return_slot, result_index, return_count);
  __ Branch(&return_value_done);

  __ bind(&return_kWasmF32);
  __ LoadFloat(fa0, MemOperand(current_return_slot, 0));
  __ AddWord(current_return_slot, current_return_slot, Operand(sizeof(float)));
  PrepareForWasmToJsConversionBuiltinCall(
      masm, return_count, result_index, current_return_slot,
      valuetypes_array_ptr, wasm_instance, fixed_array, jsarray);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmFloat32ToNumber),
          RelocInfo::CODE_TARGET);
  RestoreAfterWasmToJsConversionBuiltinCall(
      masm, jsarray, fixed_array, wasm_instance, valuetypes_array_ptr,
      current_return_slot, result_index, return_count);
  __ Branch(&return_value_done);

  __ bind(&return_kWasmF64);
  __ LoadDouble(fa0, MemOperand(current_return_slot, 0));
  __ AddWord(current_return_slot, current_return_slot, Operand(sizeof(double)));
  PrepareForWasmToJsConversionBuiltinCall(
      masm, return_count, result_index, current_return_slot,
      valuetypes_array_ptr, wasm_instance, fixed_array, jsarray);
  __ Call(BUILTIN_CODE(masm->isolate(), WasmFloat64ToNumber),
          RelocInfo::CODE_TARGET);
  RestoreAfterWasmToJsConversionBuiltinCall(
      masm, jsarray, fixed_array, wasm_instance, valuetypes_array_ptr,
      current_return_slot, result_index, return_count);
  __ jmp(&return_value_done);

  __ bind(&return_kWasmRef);
  // Make sure slot for ref args are 64-bit aligned.
  __ And(scratch, current_return_slot, Operand(0x04));
  __ AddWord(current_return_slot, current_return_slot, scratch);
  __ LoadWord(return_value, MemOperand(current_return_slot, 0));
  __ AddWord(current_return_slot, current_return_slot,
             Operand(kSystemPointerSize));

  Label next_return_value;

  __ bind(&return_value_done);
  __ AddWord(valuetypes_array_ptr, valuetypes_array_ptr,
             Operand(kValueTypeSize));
  __ Branch(&next_return_value, eq, fixed_array, Operand(zero_reg));

  // Store result into JSArray.
  DEFINE_REG(array_items);
  __ AddWord(array_items, fixed_array,
             OFFSET_OF_DATA_START(FixedArray) - kHeapObjectTag);
  __ slli(scratch, result_index, kTaggedSizeLog2);
  __ AddWord(scratch, array_items, Operand(scratch));
  __ StoreTaggedField(return_value, MemOperand(scratch, 0));

  Label skip_write_barrier;
  // See the arm64 version of LiftoffAssembler::StoreTaggedPointer.
  __ CheckPageFlag(array_items,
                   MemoryChunk::kPointersFromHereAreInterestingMask, kZero,
                   &skip_write_barrier);
  __ JumpIfSmi(return_value, &skip_write_barrier);
  __ CheckPageFlag(return_value, MemoryChunk::kPointersToHereAreInterestingMask,
                   eq, &skip_write_barrier);
  PrepareForWasmToJsConversionBuiltinCall(
      masm, return_count, result_index, current_return_slot,
      valuetypes_array_ptr, wasm_instance, fixed_array, jsarray, false);
  Register offset = return_count;
  __ li(offset, Operand(OFFSET_OF_DATA_START(FixedArray) - kHeapObjectTag));
  __ slli(scratch, result_index, kTaggedSizeLog2);
  __ AddWord(offset, offset, Operand(scratch));
  __ CallRecordWriteStubSaveRegisters(fixed_array, Operand(offset),
                                      SaveFPRegsMode::kSave);
  RestoreAfterWasmToJsConversionBuiltinCall(
      masm, jsarray, fixed_array, wasm_instance, valuetypes_array_ptr,
      current_return_slot, result_index, return_count);
  __ bind(&skip_write_barrier);

  __ bind(&next_return_value);
  __ AddWord(result_index, result_index, 1);
  __ Branch(&convert_return_value, lt, result_index, Operand(return_count));

  __ bind(&all_results_conversion_done);
  ASSIGN_REG(param_count);
  __ LoadWord(param_count, MemOperand(fp, kParamCountOffset));  // ???

  Label do_return;
  __ Branch(&do_return, eq, fixed_array, Operand(zero_reg));
  // The result is jsarray.
  __ Move(return_value, jsarray);

  __ bind(&do_return);
  // Calculate the number of parameters we have to pop off the stack. This
  // number is max(in_param_count, param_count).
  DEFINE_REG(in_param_count);
  __ LoadWord(in_param_count, MemOperand(fp, kInParamCountOffset));
  {
    if (CpuFeatures::IsSupported(ZBB)) {
      __ max(param_count, param_count, in_param_count);
    } else {
      Label done;
      __ Branch(&done, ge, param_count, Operand(in_param_count));
      __ mv(param_count, in_param_count);
      __ bind(&done);
    }
  }

  // -------------------------------------------
  // Deconstruct the stack frame.
  // -------------------------------------------
  __ LeaveFrame(StackFrame::JS_TO_WASM);

  // We have to remove the caller frame slots:
  //  - JS arguments
  //  - the receiver
  // and transfer the control to the return address (the return address is
  // expected to be on the top of the stack).
  // AddWord 1 to include the receiver in the cleanup count.
  // We cannot use just the ret instruction for this, because we cannot pass the
  // number of slots to remove in a Register as an argument.
  __ AddWord(param_count, param_count, Operand(1));
  __ DropArguments(param_count);
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
    __ Move(kRootRegister, a0);  //? Is right

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
  __ StoreWord(sp, MemOperand(centry_fp_address));

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
      MemOperand(
          target_js_function,
          wasm::ObjectAccess::SharedFunctionInfoOffsetInTaggedJSFunction()));

  // Set the context of the function; the call has to run in the function
  // context.
  DEFINE_PINNED(context, cp);
  __ LoadTaggedField(
      context, FieldMemOperand(target_js_function, JSFunction::kContextOffset));
  __ StoreWord(context, MemOperand(fp, kContextOffset));

  // Load global receiver if sloppy else use undefined.
  Label receiver_undefined;
  Label calculate_js_function_arity;
  DEFINE_REG(receiver);
  DEFINE_REG(flags);
  __ LoadWord(flags, FieldMemOperand(shared_function_info,
                                     SharedFunctionInfo::kFlagsOffset));
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
    __ Branch(&load_ref_param, ne, valuetype, Operand(1));

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
    __ Add32(scratch, param, Operand(param));
    // If there was overflow, convert the return value to a HeapNumber.
    __ Branch(&convert_param, lt, scratch, Operand(0));
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
  __ Branch(&convert_param, eq, valuetype, Operand(1));

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

  __ Branch(&return_kWasmRef, ne, valuetype, Operand(1));

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

  __ Branch(&copy_return_ref, ne, valuetype, Operand(1));

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

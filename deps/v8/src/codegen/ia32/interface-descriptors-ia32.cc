// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/codegen/interface-descriptors.h"

#include "src/execution/frames.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return esi; }

void CallInterfaceDescriptor::DefaultInitializePlatformSpecific(
    CallInterfaceDescriptorData* data, int register_parameter_count) {
  constexpr Register default_stub_registers[] = {eax, ecx, edx, edi};
  STATIC_ASSERT(arraysize(default_stub_registers) == kMaxBuiltinRegisterParams);
  CHECK_LE(static_cast<size_t>(register_parameter_count),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(register_parameter_count,
                                   default_stub_registers);
}

void RecordWriteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static const Register default_stub_registers[] = {ecx, edx, esi, edi,
                                                    kReturnRegister0};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

void DynamicCheckMapsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register default_stub_registers[] = {eax, ecx, edx, edi, esi};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

void EphemeronKeyBarrierDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static const Register default_stub_registers[] = {ecx, edx, esi, edi,
                                                    kReturnRegister0};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

const Register LoadDescriptor::ReceiverRegister() { return edx; }
const Register LoadDescriptor::NameRegister() { return ecx; }
const Register LoadDescriptor::SlotRegister() { return eax; }

const Register LoadWithVectorDescriptor::VectorRegister() { return no_reg; }

const Register
LoadWithReceiverAndVectorDescriptor::LookupStartObjectRegister() {
  return edi;
}

const Register StoreDescriptor::ReceiverRegister() { return edx; }
const Register StoreDescriptor::NameRegister() { return ecx; }
const Register StoreDescriptor::ValueRegister() { return no_reg; }
const Register StoreDescriptor::SlotRegister() { return no_reg; }

const Register StoreWithVectorDescriptor::VectorRegister() { return no_reg; }

const Register StoreTransitionDescriptor::SlotRegister() { return no_reg; }
const Register StoreTransitionDescriptor::VectorRegister() { return no_reg; }
const Register StoreTransitionDescriptor::MapRegister() { return edi; }

const Register ApiGetterDescriptor::HolderRegister() { return ecx; }
const Register ApiGetterDescriptor::CallbackRegister() { return eax; }

const Register GrowArrayElementsDescriptor::ObjectRegister() { return eax; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return ecx; }

const Register BaselineLeaveFrameDescriptor::ParamsSizeRegister() {
  return esi;
}
const Register BaselineLeaveFrameDescriptor::WeightRegister() { return edi; }

// static
const Register TypeConversionDescriptor::ArgumentRegister() { return eax; }

void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // edi : the target to call
  Register registers[] = {edi, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // ecx : arguments list length (untagged)
  // On the stack : arguments list (FixedArray)
  Register registers[] = {edi, eax, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // ecx : start index (to support rest parameters)
  // edi : the target to call
  Register registers[] = {edi, eax, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallFunctionTemplateDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // edx : function template info
  // ecx : number of arguments (on the stack, not including receiver)
  Register registers[] = {edx, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // ecx : the object to spread
  Register registers[] = {edi, eax, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // edi : the target to call
  // edx : the arguments list
  Register registers[] = {edi, edx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // edx : the new target
  // ecx : arguments list length (untagged)
  // On the stack : arguments list (FixedArray)
  Register registers[] = {edi, edx, eax, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // edx : the new target
  // ecx : start index (to support rest parameters)
  // edi : the target to call
  Register registers[] = {edi, edx, eax, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // edx : the new target
  // ecx : the object to spread
  Register registers[] = {edi, edx, eax, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // edi : the target to call
  // edx : the new target
  // ecx : the arguments list
  Register registers[] = {edi, edx, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructStubDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // edx : the new target
  // edi : the target to call
  // ecx : allocation site or undefined
  // TODO(jgruber): Remove the unused allocation site parameter.
  Register registers[] = {edi, edx, eax, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AbortDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void Compare_BaselineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edx, eax, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void BinaryOp_BaselineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edx, eax, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ApiCallbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      edx,  // kApiFunctionAddress
      ecx,  // kArgc
      eax,  // kCallData
      edi,  // kHolder
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterDispatchDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      kInterpreterAccumulatorRegister, kInterpreterBytecodeOffsetRegister,
      kInterpreterBytecodeArrayRegister, kInterpreterDispatchTableRegister};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsThenCallDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      eax,  // argument count (not including receiver)
      ecx,  // address of first argument
      edi   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsThenConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      eax,  // argument count (not including receiver)
      ecx,  // address of first argument
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ResumeGeneratorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      eax,  // the value to pass to the generator
      edx   // the JSGeneratorObject to resume
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FrameDropperTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      eax,  // loaded new FP
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void RunMicrotasksEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

void WasmFloat32ToNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // Work around using eax, whose register code is 0, and leads to the FP
  // parameter being passed via xmm0, which is not allocatable on ia32.
  Register registers[] = {ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void WasmFloat64ToNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // Work around using eax, whose register code is 0, and leads to the FP
  // parameter being passed via xmm0, which is not allocatable on ia32.
  Register registers[] = {ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32

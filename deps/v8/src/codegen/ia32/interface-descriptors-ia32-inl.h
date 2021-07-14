// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_IA32_INTERFACE_DESCRIPTORS_IA32_INL_H_
#define V8_CODEGEN_IA32_INTERFACE_DESCRIPTORS_IA32_INL_H_

#if V8_TARGET_ARCH_IA32

#include "src/codegen/interface-descriptors.h"

namespace v8 {
namespace internal {

constexpr auto CallInterfaceDescriptor::DefaultRegisterArray() {
  auto registers = RegisterArray(eax, ecx, edx, edi);
  STATIC_ASSERT(registers.size() == kMaxBuiltinRegisterParams);
  return registers;
}

// static
constexpr auto RecordWriteDescriptor::registers() {
  return RegisterArray(ecx, edx, esi, edi, kReturnRegister0);
}

// static
constexpr auto DynamicCheckMapsDescriptor::registers() {
  return RegisterArray(eax, ecx, edx, edi, esi);
}

// static
constexpr auto EphemeronKeyBarrierDescriptor::registers() {
  return RegisterArray(ecx, edx, esi, edi, kReturnRegister0);
}

// static
constexpr Register LoadDescriptor::ReceiverRegister() { return edx; }
// static
constexpr Register LoadDescriptor::NameRegister() { return ecx; }
// static
constexpr Register LoadDescriptor::SlotRegister() { return eax; }

// static
constexpr Register LoadWithVectorDescriptor::VectorRegister() { return no_reg; }

// static
constexpr Register
LoadWithReceiverAndVectorDescriptor::LookupStartObjectRegister() {
  return edi;
}

// static
constexpr Register StoreDescriptor::ReceiverRegister() { return edx; }
// static
constexpr Register StoreDescriptor::NameRegister() { return ecx; }
// static
constexpr Register StoreDescriptor::ValueRegister() { return no_reg; }
// static
constexpr Register StoreDescriptor::SlotRegister() { return no_reg; }

// static
constexpr Register StoreWithVectorDescriptor::VectorRegister() {
  return no_reg;
}

// static
constexpr Register StoreTransitionDescriptor::MapRegister() { return edi; }

// static
constexpr Register ApiGetterDescriptor::HolderRegister() { return ecx; }
// static
constexpr Register ApiGetterDescriptor::CallbackRegister() { return eax; }

// static
constexpr Register GrowArrayElementsDescriptor::ObjectRegister() { return eax; }
// static
constexpr Register GrowArrayElementsDescriptor::KeyRegister() { return ecx; }

// static
constexpr Register BaselineLeaveFrameDescriptor::ParamsSizeRegister() {
  return esi;
}
// static
constexpr Register BaselineLeaveFrameDescriptor::WeightRegister() {
  return edi;
}

// static
constexpr Register TypeConversionDescriptor::ArgumentRegister() { return eax; }

// static
constexpr auto TypeofDescriptor::registers() { return RegisterArray(ecx); }

// static
constexpr auto CallTrampolineDescriptor::registers() {
  // eax : number of arguments
  // edi : the target to call
  return RegisterArray(edi, eax);
}

// static
constexpr auto CallVarargsDescriptor::registers() {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // ecx : arguments list length (untagged)
  // On the stack : arguments list (FixedArray)
  return RegisterArray(edi, eax, ecx);
}

// static
constexpr auto CallForwardVarargsDescriptor::registers() {
  // eax : number of arguments
  // ecx : start index (to support rest parameters)
  // edi : the target to call
  return RegisterArray(edi, eax, ecx);
}

// static
constexpr auto CallFunctionTemplateDescriptor::registers() {
  // edx : function template info
  // ecx : number of arguments (on the stack, not including receiver)
  return RegisterArray(edx, ecx);
}

// static
constexpr auto CallWithSpreadDescriptor::registers() {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // ecx : the object to spread
  return RegisterArray(edi, eax, ecx);
}

// static
constexpr auto CallWithArrayLikeDescriptor::registers() {
  // edi : the target to call
  // edx : the arguments list
  return RegisterArray(edi, edx);
}

// static
constexpr auto ConstructVarargsDescriptor::registers() {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // edx : the new target
  // ecx : arguments list length (untagged)
  // On the stack : arguments list (FixedArray)
  return RegisterArray(edi, edx, eax, ecx);
}

// static
constexpr auto ConstructForwardVarargsDescriptor::registers() {
  // eax : number of arguments
  // edx : the new target
  // ecx : start index (to support rest parameters)
  // edi : the target to call
  return RegisterArray(edi, edx, eax, ecx);
}

// static
constexpr auto ConstructWithSpreadDescriptor::registers() {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // edx : the new target
  // ecx : the object to spread
  return RegisterArray(edi, edx, eax, ecx);
}

// static
constexpr auto ConstructWithArrayLikeDescriptor::registers() {
  // edi : the target to call
  // edx : the new target
  // ecx : the arguments list
  return RegisterArray(edi, edx, ecx);
}

// static
constexpr auto ConstructStubDescriptor::registers() {
  // eax : number of arguments
  // edx : the new target
  // edi : the target to call
  // ecx : allocation site or undefined
  // TODO(jgruber): Remove the unused allocation site parameter.
  return RegisterArray(edi, edx, eax, ecx);
}

// static
constexpr auto AbortDescriptor::registers() { return RegisterArray(edx); }

// static
constexpr auto CompareDescriptor::registers() {
  return RegisterArray(edx, eax);
}

// static
constexpr auto Compare_BaselineDescriptor::registers() {
  return RegisterArray(edx, eax, ecx);
}

// static
constexpr auto BinaryOpDescriptor::registers() {
  return RegisterArray(edx, eax);
}

// static
constexpr auto BinaryOp_BaselineDescriptor::registers() {
  return RegisterArray(edx, eax, ecx);
}

// static
constexpr auto ApiCallbackDescriptor::registers() {
  return RegisterArray(edx,   // kApiFunctionAddress
                       ecx,   // kArgc
                       eax,   // kCallData
                       edi);  // kHolder
}

// static
constexpr auto InterpreterDispatchDescriptor::registers() {
  return RegisterArray(
      kInterpreterAccumulatorRegister, kInterpreterBytecodeOffsetRegister,
      kInterpreterBytecodeArrayRegister, kInterpreterDispatchTableRegister);
}

// static
constexpr auto InterpreterPushArgsThenCallDescriptor::registers() {
  return RegisterArray(eax,   // argument count (not including receiver)
                       ecx,   // address of first argument
                       edi);  // the target callable to be call
}

// static
constexpr auto InterpreterPushArgsThenConstructDescriptor::registers() {
  return RegisterArray(eax,   // argument count (not including receiver)
                       ecx);  // address of first argument
}

// static
constexpr auto ResumeGeneratorDescriptor::registers() {
  return RegisterArray(eax,   // the value to pass to the generator
                       edx);  // the JSGeneratorObject to resume
}

// static
constexpr auto RunMicrotasksEntryDescriptor::registers() {
  return RegisterArray();
}

// static
constexpr auto WasmFloat32ToNumberDescriptor::registers() {
  // Work around using eax, whose register code is 0, and leads to the FP
  // parameter being passed via xmm0, which is not allocatable on ia32.
  return RegisterArray(ecx);
}

// static
constexpr auto WasmFloat64ToNumberDescriptor::registers() {
  // Work around using eax, whose register code is 0, and leads to the FP
  // parameter being passed via xmm0, which is not allocatable on ia32.
  return RegisterArray(ecx);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32

#endif  // V8_CODEGEN_IA32_INTERFACE_DESCRIPTORS_IA32_INL_H_

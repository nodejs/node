// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_MIPS_INTERFACE_DESCRIPTORS_MIPS_INL_H_
#define V8_CODEGEN_MIPS_INTERFACE_DESCRIPTORS_MIPS_INL_H_

#if V8_TARGET_ARCH_MIPS

#include "src/codegen/interface-descriptors.h"
#include "src/execution/frames.h"

namespace v8 {
namespace internal {

constexpr auto CallInterfaceDescriptor::DefaultRegisterArray() {
  auto registers = RegisterArray(a0, a1, a2, a3, t0);
  STATIC_ASSERT(registers.size() == kMaxBuiltinRegisterParams);
  return registers;
}

#if DEBUG
template <typename DerivedDescriptor>
void StaticCallInterfaceDescriptor<DerivedDescriptor>::
    VerifyArgumentRegisterCount(CallInterfaceDescriptorData* data, int argc) {
  RegList allocatable_regs = data->allocatable_registers();
  if (argc >= 1) DCHECK(allocatable_regs | a0.bit());
  if (argc >= 2) DCHECK(allocatable_regs | a1.bit());
  if (argc >= 3) DCHECK(allocatable_regs | a2.bit());
  if (argc >= 4) DCHECK(allocatable_regs | a3.bit());
  // Additional arguments are passed on the stack.
}
#endif  // DEBUG

// static
constexpr auto WriteBarrierDescriptor::registers() {
  return RegisterArray(a1, v0, a0, a2, a3);
}

// static
constexpr auto DynamicCheckMapsDescriptor::registers() {
  STATIC_ASSERT(kReturnRegister0 == v0);
  return RegisterArray(kReturnRegister0, a0, a1, a2, cp);
}

// static
constexpr auto DynamicCheckMapsWithFeedbackVectorDescriptor::registers() {
  STATIC_ASSERT(kReturnRegister0 == v0);
  return RegisterArray(kReturnRegister0, a0, a1, a2, cp);
}

// static
constexpr Register LoadDescriptor::ReceiverRegister() { return a1; }
// static
constexpr Register LoadDescriptor::NameRegister() { return a2; }
// static
constexpr Register LoadDescriptor::SlotRegister() { return a0; }

// static
constexpr Register LoadWithVectorDescriptor::VectorRegister() { return a3; }

// static
constexpr Register
LoadWithReceiverAndVectorDescriptor::LookupStartObjectRegister() {
  return t0;
}

// static
constexpr Register StoreDescriptor::ReceiverRegister() { return a1; }
// static
constexpr Register StoreDescriptor::NameRegister() { return a2; }
// static
constexpr Register StoreDescriptor::ValueRegister() { return a0; }
// static
constexpr Register StoreDescriptor::SlotRegister() { return t0; }

// static
constexpr Register StoreWithVectorDescriptor::VectorRegister() { return a3; }

// static
constexpr Register StoreTransitionDescriptor::MapRegister() { return t1; }

// static
constexpr Register ApiGetterDescriptor::HolderRegister() { return a0; }
// static
constexpr Register ApiGetterDescriptor::CallbackRegister() { return a3; }

// static
constexpr Register GrowArrayElementsDescriptor::ObjectRegister() { return a0; }
// static
constexpr Register GrowArrayElementsDescriptor::KeyRegister() { return a3; }

// static
constexpr Register BaselineLeaveFrameDescriptor::ParamsSizeRegister() {
  return a2;
}

// static
constexpr Register BaselineLeaveFrameDescriptor::WeightRegister() {
  // TODO(v8:11421): Implement on this platform.
  return a3;
}

// static
constexpr Register TypeConversionDescriptor::ArgumentRegister() { return a0; }

// static
constexpr auto TypeofDescriptor::registers() { return RegisterArray(a3); }

// static
constexpr auto CallTrampolineDescriptor::registers() {
  // a1: target
  // a0: number of arguments
  return RegisterArray(a1, a0);
}

// static
constexpr auto CallVarargsDescriptor::registers() {
  // a0 : number of arguments (on the stack, not including receiver)
  // a1 : the target to call
  // t0 : arguments list length (untagged)
  // a2 : arguments list (FixedArray)
  return RegisterArray(a1, a0, t0, a2);
}

// static
constexpr auto CallForwardVarargsDescriptor::registers() {
  // a1: the target to call
  // a0: number of arguments
  // a2: start index (to support rest parameters)
  return RegisterArray(a1, a0, a2);
}

// static
constexpr auto CallFunctionTemplateDescriptor::registers() {
  // a1 : function template info
  // a0 : number of arguments (on the stack, not including receiver)
  return RegisterArray(a1, a0);
}

// static
constexpr auto CallWithSpreadDescriptor::registers() {
  // a0 : number of arguments (on the stack, not including receiver)
  // a1 : the target to call
  // a2 : the object to spread
  return RegisterArray(a1, a0, a2);
}

// static
constexpr auto CallWithArrayLikeDescriptor::registers() {
  // a1 : the target to call
  // a2 : the arguments list
  return RegisterArray(a1, a2);
}

// static
constexpr auto ConstructVarargsDescriptor::registers() {
  // a0 : number of arguments (on the stack, not including receiver)
  // a1 : the target to call
  // a3 : the new target
  // t0 : arguments list length (untagged)
  // a2 : arguments list (FixedArray)
  return RegisterArray(a1, a3, a0, t0, a2);
}

// static
constexpr auto ConstructForwardVarargsDescriptor::registers() {
  // a1: the target to call
  // a3: new target
  // a0: number of arguments
  // a2: start index (to support rest parameters)
  return RegisterArray(a1, a3, a0, a2);
}

// static
constexpr auto ConstructWithSpreadDescriptor::registers() {
  // a0 : number of arguments (on the stack, not including receiver)
  // a1 : the target to call
  // a3 : the new target
  // a2 : the object to spread
  return RegisterArray(a1, a3, a0, a2);
}

// static
constexpr auto ConstructWithArrayLikeDescriptor::registers() {
  // a1 : the target to call
  // a3 : the new target
  // a2 : the arguments list
  return RegisterArray(a1, a3, a2);
}

// static
constexpr auto ConstructStubDescriptor::registers() {
  // a1: target
  // a3: new target
  // a0: number of arguments
  // a2: allocation site or undefined
  return RegisterArray(a1, a3, a0, a2);
}

// static
constexpr auto AbortDescriptor::registers() { return RegisterArray(a0); }

// static
constexpr auto CompareDescriptor::registers() { return RegisterArray(a1, a0); }

// static
constexpr auto Compare_BaselineDescriptor::registers() {
  // a1: left operand
  // a0: right operand
  // a2: feedback slot
  return RegisterArray(a1, a0, a2);
}

// static
constexpr auto BinaryOpDescriptor::registers() { return RegisterArray(a1, a0); }

// static
constexpr auto BinaryOp_BaselineDescriptor::registers() {
  // TODO(v8:11421): Implement on this platform.
  return RegisterArray(a1, a0, a2);
}

// static
constexpr auto ApiCallbackDescriptor::registers() {
  // a1 : kApiFunctionAddress
  // a2 : kArgc
  // a3 : kCallData
  // a0 : kHolder
  return RegisterArray(a1, a2, a3, a0);
}

// static
constexpr auto InterpreterDispatchDescriptor::registers() {
  return RegisterArray(
      kInterpreterAccumulatorRegister, kInterpreterBytecodeOffsetRegister,
      kInterpreterBytecodeArrayRegister, kInterpreterDispatchTableRegister);
}

// static
constexpr auto InterpreterPushArgsThenCallDescriptor::registers() {
  // a0 : argument count (not including receiver
  // a2 : address of first argument
  // a1 : the target callable to be call
  return RegisterArray(a0, a2, a1);
}

// static
constexpr auto InterpreterPushArgsThenConstructDescriptor::registers() {
  // a0 : argument count (not including receiver)
  // t4 : address of the first argument
  // a1 : constructor to call
  // a3 : new target
  // a2 : allocation site feedback if available, undefined otherwise
  return RegisterArray(a0, t4, a1, a3, a2);
}

// static
constexpr auto ResumeGeneratorDescriptor::registers() {
  // v0 : the value to pass to the generator
  // a1 : the JSGeneratorObject to resume
  return RegisterArray(v0, a1);
}

// static
constexpr auto RunMicrotasksEntryDescriptor::registers() {
  return RegisterArray(a0, a1);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS

#endif  // V8_CODEGEN_MIPS_INTERFACE_DESCRIPTORS_MIPS_INL_H_

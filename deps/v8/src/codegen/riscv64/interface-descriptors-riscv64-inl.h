// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV64_INTERFACE_DESCRIPTORS_RISCV64_INL_H_
#define V8_CODEGEN_RISCV64_INTERFACE_DESCRIPTORS_RISCV64_INL_H_

#if V8_TARGET_ARCH_RISCV64

#include "src/base/template-utils.h"
#include "src/codegen/interface-descriptors.h"
#include "src/execution/frames.h"

namespace v8 {
namespace internal {

constexpr auto CallInterfaceDescriptor::DefaultRegisterArray() {
  auto registers = RegisterArray(a0, a1, a2, a3, a4);
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
  if (argc >= 5) DCHECK(allocatable_regs | a4.bit());
  if (argc >= 6) DCHECK(allocatable_regs | a5.bit());
  if (argc >= 7) DCHECK(allocatable_regs | a6.bit());
  if (argc >= 8) DCHECK(allocatable_regs | a7.bit());
  // Additional arguments are passed on the stack.
}
#endif  // DEBUG

// static
constexpr auto WriteBarrierDescriptor::registers() {
  return RegisterArray(a1, a5, a4, a2, a0, a3);
}

// static
constexpr auto DynamicCheckMapsDescriptor::registers() {
  STATIC_ASSERT(kReturnRegister0 == a0);
  return RegisterArray(kReturnRegister0, a1, a2, a3, cp);
}

// static
constexpr auto DynamicCheckMapsWithFeedbackVectorDescriptor::registers() {
  STATIC_ASSERT(kReturnRegister0 == a0);
  return RegisterArray(kReturnRegister0, a1, a2, a3, cp);
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
  return a4;
}

// static
constexpr Register StoreDescriptor::ReceiverRegister() { return a1; }
// static
constexpr Register StoreDescriptor::NameRegister() { return a2; }
// static
constexpr Register StoreDescriptor::ValueRegister() { return a0; }
// static
constexpr Register StoreDescriptor::SlotRegister() { return a4; }

// static
constexpr Register StoreWithVectorDescriptor::VectorRegister() { return a3; }

// static
constexpr Register StoreTransitionDescriptor::MapRegister() { return a5; }

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
constexpr Register BaselineLeaveFrameDescriptor::WeightRegister() { return a3; }

// static
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
  // a4 : arguments list length (untagged)
  // a2 : arguments list (FixedArray)
  return RegisterArray(a1, a0, a4, a2);
}

// static
constexpr auto CallForwardVarargsDescriptor::registers() {
  // a1: target
  // a0: number of arguments
  // a2: start index (to supported rest parameters)
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
  // a4 : arguments list length (untagged)
  // a2 : arguments list (FixedArray)
  return RegisterArray(a1, a3, a0, a4, a2);
}

// static
constexpr auto ConstructForwardVarargsDescriptor::registers() {
  // a3: new target
  // a1: target
  // a0: number of arguments
  // a2: start index (to supported rest parameters)
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
  // a3: new target
  // a1: target
  // a0: number of arguments
  // a2: allocation site or undefined
  return RegisterArray(a1, a3, a0, a2);
}

// static
constexpr auto AbortDescriptor::registers() { return RegisterArray(a0); }

// static
constexpr auto CompareDescriptor::registers() {
  // a1: left operand
  // a0: right operand
  return RegisterArray(a1, a0);
}

// static
constexpr auto Compare_BaselineDescriptor::registers() {
  // a1: left operand
  // a0: right operand
  // a2: feedback slot
  return RegisterArray(a1, a0, a2);
}

// static
constexpr auto BinaryOpDescriptor::registers() {
  // a1: left operand
  // a0: right operand
  return RegisterArray(a1, a0);
}

// static
constexpr auto BinaryOp_BaselineDescriptor::registers() {
  // a1: left operand
  // a0: right operand
  // a2: feedback slot
  return RegisterArray(a1, a0, a2);
}

// static
constexpr auto ApiCallbackDescriptor::registers() {
  return RegisterArray(a1,   // kApiFunctionAddress
                       a2,   // kArgc
                       a3,   // kCallData
                       a0);  // kHolder
}

// static
constexpr auto InterpreterDispatchDescriptor::registers() {
  return RegisterArray(
      kInterpreterAccumulatorRegister, kInterpreterBytecodeOffsetRegister,
      kInterpreterBytecodeArrayRegister, kInterpreterDispatchTableRegister);
}

// static
constexpr auto InterpreterPushArgsThenCallDescriptor::registers() {
  return RegisterArray(a0,   // argument count (not including receiver)
                       a2,   // address of first argument
                       a1);  // the target callable to be call
}

// static
constexpr auto InterpreterPushArgsThenConstructDescriptor::registers() {
  return RegisterArray(
      a0,   // argument count (not including receiver)
      a4,   // address of the first argument
      a1,   // constructor to call
      a3,   // new target
      a2);  // allocation site feedback if available, undefined otherwise
}

// static
constexpr auto ResumeGeneratorDescriptor::registers() {
  return RegisterArray(a0,   // the value to pass to the generator
                       a1);  // the JSGeneratorObject to resume
}

// static
constexpr auto RunMicrotasksEntryDescriptor::registers() {
  return RegisterArray(a0, a1);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_RISCV64

#endif  // V8_CODEGEN_RISCV64_INTERFACE_DESCRIPTORS_RISCV64_INL_H_

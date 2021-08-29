// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_ARM_INTERFACE_DESCRIPTORS_ARM_INL_H_
#define V8_CODEGEN_ARM_INTERFACE_DESCRIPTORS_ARM_INL_H_

#if V8_TARGET_ARCH_ARM

#include "src/codegen/interface-descriptors.h"
#include "src/execution/frames.h"

namespace v8 {
namespace internal {

constexpr auto CallInterfaceDescriptor::DefaultRegisterArray() {
  auto registers = RegisterArray(r0, r1, r2, r3, r4);
  STATIC_ASSERT(registers.size() == kMaxBuiltinRegisterParams);
  return registers;
}

#if DEBUG
template <typename DerivedDescriptor>
void StaticCallInterfaceDescriptor<DerivedDescriptor>::
    VerifyArgumentRegisterCount(CallInterfaceDescriptorData* data, int argc) {
  RegList allocatable_regs = data->allocatable_registers();
  if (argc >= 1) DCHECK(allocatable_regs | r0.bit());
  if (argc >= 2) DCHECK(allocatable_regs | r1.bit());
  if (argc >= 3) DCHECK(allocatable_regs | r2.bit());
  if (argc >= 4) DCHECK(allocatable_regs | r3.bit());
  if (argc >= 5) DCHECK(allocatable_regs | r4.bit());
  if (argc >= 6) DCHECK(allocatable_regs | r5.bit());
  if (argc >= 7) DCHECK(allocatable_regs | r6.bit());
  if (argc >= 8) DCHECK(allocatable_regs | r7.bit());
  // Additional arguments are passed on the stack.
}
#endif  // DEBUG

// static
constexpr auto WriteBarrierDescriptor::registers() {
  return RegisterArray(r1, r5, r4, r2, r0);
}

// static
constexpr auto DynamicCheckMapsDescriptor::registers() {
  STATIC_ASSERT(kReturnRegister0 == r0);
  return RegisterArray(r0, r1, r2, r3, cp);
}

// static
constexpr auto DynamicCheckMapsWithFeedbackVectorDescriptor::registers() {
  STATIC_ASSERT(kReturnRegister0 == r0);
  return RegisterArray(r0, r1, r2, r3, cp);
}

// static
constexpr Register LoadDescriptor::ReceiverRegister() { return r1; }
// static
constexpr Register LoadDescriptor::NameRegister() { return r2; }
// static
constexpr Register LoadDescriptor::SlotRegister() { return r0; }

// static
constexpr Register LoadWithVectorDescriptor::VectorRegister() { return r3; }

// static
constexpr Register
LoadWithReceiverAndVectorDescriptor::LookupStartObjectRegister() {
  return r4;
}

// static
constexpr Register StoreDescriptor::ReceiverRegister() { return r1; }
// static
constexpr Register StoreDescriptor::NameRegister() { return r2; }
// static
constexpr Register StoreDescriptor::ValueRegister() { return r0; }
// static
constexpr Register StoreDescriptor::SlotRegister() { return r4; }

// static
constexpr Register StoreWithVectorDescriptor::VectorRegister() { return r3; }

// static
constexpr Register StoreTransitionDescriptor::MapRegister() { return r5; }

// static
constexpr Register ApiGetterDescriptor::HolderRegister() { return r0; }
// static
constexpr Register ApiGetterDescriptor::CallbackRegister() { return r3; }

// static
constexpr Register GrowArrayElementsDescriptor::ObjectRegister() { return r0; }
// static
constexpr Register GrowArrayElementsDescriptor::KeyRegister() { return r3; }

// static
constexpr Register BaselineLeaveFrameDescriptor::ParamsSizeRegister() {
  return r3;
}
// static
constexpr Register BaselineLeaveFrameDescriptor::WeightRegister() { return r4; }

// static
// static
constexpr Register TypeConversionDescriptor::ArgumentRegister() { return r0; }

// static
constexpr auto TypeofDescriptor::registers() { return RegisterArray(r3); }

// static
constexpr auto CallTrampolineDescriptor::registers() {
  // r0 : number of arguments
  // r1 : the target to call
  return RegisterArray(r1, r0);
}

// static
constexpr auto CallVarargsDescriptor::registers() {
  // r0 : number of arguments (on the stack, not including receiver)
  // r1 : the target to call
  // r4 : arguments list length (untagged)
  // r2 : arguments list (FixedArray)
  return RegisterArray(r1, r0, r4, r2);
}

// static
constexpr auto CallForwardVarargsDescriptor::registers() {
  // r0 : number of arguments
  // r2 : start index (to support rest parameters)
  // r1 : the target to call
  return RegisterArray(r1, r0, r2);
}

// static
constexpr auto CallFunctionTemplateDescriptor::registers() {
  // r1 : function template info
  // r2 : number of arguments (on the stack, not including receiver)
  return RegisterArray(r1, r2);
}

// static
constexpr auto CallWithSpreadDescriptor::registers() {
  // r0 : number of arguments (on the stack, not including receiver)
  // r1 : the target to call
  // r2 : the object to spread
  return RegisterArray(r1, r0, r2);
}

// static
constexpr auto CallWithArrayLikeDescriptor::registers() {
  // r1 : the target to call
  // r2 : the arguments list
  return RegisterArray(r1, r2);
}

// static
constexpr auto ConstructVarargsDescriptor::registers() {
  // r0 : number of arguments (on the stack, not including receiver)
  // r1 : the target to call
  // r3 : the new target
  // r4 : arguments list length (untagged)
  // r2 : arguments list (FixedArray)
  return RegisterArray(r1, r3, r0, r4, r2);
}

// static
constexpr auto ConstructForwardVarargsDescriptor::registers() {
  // r0 : number of arguments
  // r3 : the new target
  // r2 : start index (to support rest parameters)
  // r1 : the target to call
  return RegisterArray(r1, r3, r0, r2);
}

// static
constexpr auto ConstructWithSpreadDescriptor::registers() {
  // r0 : number of arguments (on the stack, not including receiver)
  // r1 : the target to call
  // r3 : the new target
  // r2 : the object to spread
  return RegisterArray(r1, r3, r0, r2);
}

// static
constexpr auto ConstructWithArrayLikeDescriptor::registers() {
  // r1 : the target to call
  // r3 : the new target
  // r2 : the arguments list
  return RegisterArray(r1, r3, r2);
}

// static
constexpr auto ConstructStubDescriptor::registers() {
  // r0 : number of arguments
  // r1 : the target to call
  // r3 : the new target
  // r2 : allocation site or undefined
  return RegisterArray(r1, r3, r0, r2);
}

// static
constexpr auto AbortDescriptor::registers() { return RegisterArray(r1); }

// static
constexpr auto CompareDescriptor::registers() { return RegisterArray(r1, r0); }

// static
constexpr auto Compare_BaselineDescriptor::registers() {
  // r1: left operand
  // r0: right operand
  // r2: feedback slot
  return RegisterArray(r1, r0, r2);
}

// static
constexpr auto BinaryOpDescriptor::registers() { return RegisterArray(r1, r0); }

// static
constexpr auto BinaryOp_BaselineDescriptor::registers() {
  // r1: left operand
  // r0: right operand
  // r2: feedback slot
  return RegisterArray(r1, r0, r2);
}

// static
constexpr auto ApiCallbackDescriptor::registers() {
  return RegisterArray(r1,   // kApiFunctionAddress
                       r2,   // kArgc
                       r3,   // kCallData
                       r0);  // kHolder
}

// static
constexpr auto InterpreterDispatchDescriptor::registers() {
  return RegisterArray(
      kInterpreterAccumulatorRegister, kInterpreterBytecodeOffsetRegister,
      kInterpreterBytecodeArrayRegister, kInterpreterDispatchTableRegister);
}

// static
constexpr auto InterpreterPushArgsThenCallDescriptor::registers() {
  return RegisterArray(r0,   // argument count (not including receiver)
                       r2,   // address of first argument
                       r1);  // the target callable to be call
}

// static
constexpr auto InterpreterPushArgsThenConstructDescriptor::registers() {
  return RegisterArray(
      r0,   // argument count (not including receiver)
      r4,   // address of the first argument
      r1,   // constructor to call
      r3,   // new target
      r2);  // allocation site feedback if available, undefined otherwise
}

// static
constexpr auto ResumeGeneratorDescriptor::registers() {
  return RegisterArray(r0,   // the value to pass to the generator
                       r1);  // the JSGeneratorObject to resume
}

// static
constexpr auto RunMicrotasksEntryDescriptor::registers() {
  return RegisterArray(r0, r1);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

#endif  // V8_CODEGEN_ARM_INTERFACE_DESCRIPTORS_ARM_INL_H_

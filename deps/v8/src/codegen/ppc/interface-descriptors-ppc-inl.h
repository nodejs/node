// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_PPC_INTERFACE_DESCRIPTORS_PPC_INL_H_
#define V8_CODEGEN_PPC_INTERFACE_DESCRIPTORS_PPC_INL_H_

#if V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64

#include "src/codegen/interface-descriptors.h"
#include "src/execution/frames.h"

namespace v8 {
namespace internal {

constexpr auto CallInterfaceDescriptor::DefaultRegisterArray() {
  auto registers = RegisterArray(r3, r4, r5, r6, r7);
  STATIC_ASSERT(registers.size() == kMaxBuiltinRegisterParams);
  return registers;
}

// static
constexpr auto RecordWriteDescriptor::registers() {
  return RegisterArray(r3, r4, r5, r6, r7, kReturnRegister0);
}

// static
constexpr auto DynamicCheckMapsDescriptor::registers() {
  return RegisterArray(r3, r4, r5, r6, cp);
}

// static
constexpr auto EphemeronKeyBarrierDescriptor::registers() {
  return RegisterArray(r3, r4, r5, r6, r7, kReturnRegister0);
}

// static
constexpr Register LoadDescriptor::ReceiverRegister() { return r4; }
// static
constexpr Register LoadDescriptor::NameRegister() { return r5; }
// static
constexpr Register LoadDescriptor::SlotRegister() { return r3; }

// static
constexpr Register LoadWithVectorDescriptor::VectorRegister() { return r6; }

// static
constexpr Register
LoadWithReceiverAndVectorDescriptor::LookupStartObjectRegister() {
  return r7;
}

// static
constexpr Register StoreDescriptor::ReceiverRegister() { return r4; }
// static
constexpr Register StoreDescriptor::NameRegister() { return r5; }
// static
constexpr Register StoreDescriptor::ValueRegister() { return r3; }
// static
constexpr Register StoreDescriptor::SlotRegister() { return r7; }

// static
constexpr Register StoreWithVectorDescriptor::VectorRegister() { return r6; }

// static
constexpr Register StoreTransitionDescriptor::MapRegister() { return r8; }

// static
constexpr Register ApiGetterDescriptor::HolderRegister() { return r3; }
// static
constexpr Register ApiGetterDescriptor::CallbackRegister() { return r6; }

// static
constexpr Register GrowArrayElementsDescriptor::ObjectRegister() { return r3; }
// static
constexpr Register GrowArrayElementsDescriptor::KeyRegister() { return r6; }

// static
constexpr Register BaselineLeaveFrameDescriptor::ParamsSizeRegister() {
  // TODO(v8:11421): Implement on this platform.
  return r6;
}
// static
constexpr Register BaselineLeaveFrameDescriptor::WeightRegister() {
  // TODO(v8:11421): Implement on this platform.
  return r7;
}

// static
// static
constexpr Register TypeConversionDescriptor::ArgumentRegister() { return r3; }

// static
constexpr auto TypeofDescriptor::registers() { return RegisterArray(r6); }

// static
constexpr auto CallTrampolineDescriptor::registers() {
  // r3 : number of arguments
  // r4 : the target to call
  return RegisterArray(r4, r3);
}

// static
constexpr auto CallVarargsDescriptor::registers() {
  // r3 : number of arguments (on the stack, not including receiver)
  // r4 : the target to call
  // r7 : arguments list length (untagged)
  // r5 : arguments list (FixedArray)
  return RegisterArray(r4, r3, r7, r5);
}

// static
constexpr auto CallForwardVarargsDescriptor::registers() {
  // r3 : number of arguments
  // r5 : start index (to support rest parameters)
  // r4 : the target to call
  return RegisterArray(r4, r3, r5);
}

// static
constexpr auto CallFunctionTemplateDescriptor::registers() {
  // r4 : function template info
  // r5 : number of arguments (on the stack, not including receiver)
  return RegisterArray(r4, r5);
}

// static
constexpr auto CallWithSpreadDescriptor::registers() {
  // r3 : number of arguments (on the stack, not including receiver)
  // r4 : the target to call
  // r5 : the object to spread
  return RegisterArray(r4, r3, r5);
}

// static
constexpr auto CallWithArrayLikeDescriptor::registers() {
  // r4 : the target to call
  // r5 : the arguments list
  return RegisterArray(r4, r5);
}

// static
constexpr auto ConstructVarargsDescriptor::registers() {
  // r3 : number of arguments (on the stack, not including receiver)
  // r4 : the target to call
  // r6 : the new target
  // r7 : arguments list length (untagged)
  // r5 : arguments list (FixedArray)
  return RegisterArray(r4, r6, r3, r7, r5);
}

// static
constexpr auto ConstructForwardVarargsDescriptor::registers() {
  // r3 : number of arguments
  // r6 : the new target
  // r5 : start index (to support rest parameters)
  // r4 : the target to call
  return RegisterArray(r4, r6, r3, r5);
}

// static
constexpr auto ConstructWithSpreadDescriptor::registers() {
  // r3 : number of arguments (on the stack, not including receiver)
  // r4 : the target to call
  // r6 : the new target
  // r5 : the object to spread
  return RegisterArray(r4, r6, r3, r5);
}

// static
constexpr auto ConstructWithArrayLikeDescriptor::registers() {
  // r4 : the target to call
  // r6 : the new target
  // r5 : the arguments list
  return RegisterArray(r4, r6, r5);
}

// static
constexpr auto ConstructStubDescriptor::registers() {
  // r3 : number of arguments
  // r4 : the target to call
  // r6 : the new target
  // r5 : allocation site or undefined
  return RegisterArray(r4, r6, r3, r5);
}

// static
constexpr auto AbortDescriptor::registers() { return RegisterArray(r4); }

// static
constexpr auto CompareDescriptor::registers() { return RegisterArray(r4, r3); }

// static
constexpr auto Compare_BaselineDescriptor::registers() {
  // TODO(v8:11421): Implement on this platform.
  return DefaultRegisterArray();
}

// static
constexpr auto BinaryOpDescriptor::registers() { return RegisterArray(r4, r3); }

// static
constexpr auto BinaryOp_BaselineDescriptor::registers() {
  // TODO(v8:11421): Implement on this platform.
  return DefaultRegisterArray();
}

// static
constexpr auto ApiCallbackDescriptor::registers() {
  return RegisterArray(r4,   // kApiFunctionAddress
                       r5,   // kArgc
                       r6,   // kCallData
                       r3);  // kHolder
}

// static
constexpr auto InterpreterDispatchDescriptor::registers() {
  return RegisterArray(
      kInterpreterAccumulatorRegister, kInterpreterBytecodeOffsetRegister,
      kInterpreterBytecodeArrayRegister, kInterpreterDispatchTableRegister);
}

// static
constexpr auto InterpreterPushArgsThenCallDescriptor::registers() {
  return RegisterArray(r3,   // argument count (not including receiver)
                       r5,   // address of first argument
                       r4);  // the target callable to be call
}

// static
constexpr auto InterpreterPushArgsThenConstructDescriptor::registers() {
  return RegisterArray(
      r3,   // argument count (not including receiver)
      r7,   // address of the first argument
      r4,   // constructor to call
      r6,   // new target
      r5);  // allocation site feedback if available, undefined otherwise
}

// static
constexpr auto ResumeGeneratorDescriptor::registers() {
  return RegisterArray(r3,   // the value to pass to the generator
                       r4);  // the JSGeneratorObject to resume
}

// static
constexpr auto RunMicrotasksEntryDescriptor::registers() {
  return RegisterArray(r3, r4);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64

#endif  // V8_CODEGEN_PPC_INTERFACE_DESCRIPTORS_PPC_INL_H_

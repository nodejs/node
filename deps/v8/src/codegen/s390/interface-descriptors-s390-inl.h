// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_S390_INTERFACE_DESCRIPTORS_S390_INL_H_
#define V8_CODEGEN_S390_INTERFACE_DESCRIPTORS_S390_INL_H_

#if V8_TARGET_ARCH_S390

#include "src/codegen/interface-descriptors.h"
#include "src/execution/frames.h"

namespace v8 {
namespace internal {

constexpr auto CallInterfaceDescriptor::DefaultRegisterArray() {
  auto registers = RegisterArray(r2, r3, r4, r5, r6);
  STATIC_ASSERT(registers.size() == kMaxBuiltinRegisterParams);
  return registers;
}

// static
constexpr auto RecordWriteDescriptor::registers() {
  return RegisterArray(r2, r3, r4, r5, r6, kReturnRegister0);
}

// static
constexpr auto DynamicCheckMapsDescriptor::registers() {
  return RegisterArray(r2, r3, r4, r5, cp);
}

// static
constexpr auto EphemeronKeyBarrierDescriptor::registers() {
  return RegisterArray(r2, r3, r4, r5, r6, kReturnRegister0);
}

// static
constexpr Register LoadDescriptor::ReceiverRegister() { return r3; }
// static
constexpr Register LoadDescriptor::NameRegister() { return r4; }
// static
constexpr Register LoadDescriptor::SlotRegister() { return r2; }

// static
constexpr Register LoadWithVectorDescriptor::VectorRegister() { return r5; }

// static
constexpr Register
LoadWithReceiverAndVectorDescriptor::LookupStartObjectRegister() {
  return r6;
}

// static
constexpr Register StoreDescriptor::ReceiverRegister() { return r3; }
// static
constexpr Register StoreDescriptor::NameRegister() { return r4; }
// static
constexpr Register StoreDescriptor::ValueRegister() { return r2; }
// static
constexpr Register StoreDescriptor::SlotRegister() { return r6; }

// static
constexpr Register StoreWithVectorDescriptor::VectorRegister() { return r5; }

// static
constexpr Register StoreTransitionDescriptor::MapRegister() { return r7; }

// static
constexpr Register ApiGetterDescriptor::HolderRegister() { return r2; }
// static
constexpr Register ApiGetterDescriptor::CallbackRegister() { return r5; }

// static
constexpr Register GrowArrayElementsDescriptor::ObjectRegister() { return r2; }
// static
constexpr Register GrowArrayElementsDescriptor::KeyRegister() { return r5; }

// static
constexpr Register BaselineLeaveFrameDescriptor::ParamsSizeRegister() {
  // TODO(v8:11421): Implement on this platform.
  return r5;
}
// static
constexpr Register BaselineLeaveFrameDescriptor::WeightRegister() {
  // TODO(v8:11421): Implement on this platform.
  return r6;
}

// static
// static
constexpr Register TypeConversionDescriptor::ArgumentRegister() { return r2; }

// static
constexpr auto TypeofDescriptor::registers() { return RegisterArray(r5); }

// static
constexpr auto CallTrampolineDescriptor::registers() {
  // r2 : number of arguments
  // r3 : the target to call
  return RegisterArray(r3, r2);
}

// static
constexpr auto CallVarargsDescriptor::registers() {
  // r2 : number of arguments (on the stack, not including receiver)
  // r3 : the target to call
  // r6 : arguments list length (untagged)
  // r4 : arguments list (FixedArray)
  return RegisterArray(r3, r2, r6, r4);
}

// static
constexpr auto CallForwardVarargsDescriptor::registers() {
  // r2 : number of arguments
  // r4 : start index (to support rest parameters)
  // r3 : the target to call
  return RegisterArray(r3, r2, r4);
}

// static
constexpr auto CallFunctionTemplateDescriptor::registers() {
  // r3 : function template info
  // r4 : number of arguments (on the stack, not including receiver)
  return RegisterArray(r3, r4);
}

// static
constexpr auto CallWithSpreadDescriptor::registers() {
  // r2: number of arguments (on the stack, not including receiver)
  // r3 : the target to call
  // r4 : the object to spread
  return RegisterArray(r3, r2, r4);
}

// static
constexpr auto CallWithArrayLikeDescriptor::registers() {
  // r3 : the target to call
  // r4 : the arguments list
  return RegisterArray(r3, r4);
}

// static
constexpr auto ConstructVarargsDescriptor::registers() {
  // r2 : number of arguments (on the stack, not including receiver)
  // r3 : the target to call
  // r5 : the new target
  // r6 : arguments list length (untagged)
  // r4 : arguments list (FixedArray)
  return RegisterArray(r3, r5, r2, r6, r4);
}

// static
constexpr auto ConstructForwardVarargsDescriptor::registers() {
  // r2 : number of arguments
  // r5 : the new target
  // r4 : start index (to support rest parameters)
  // r3 : the target to call
  return RegisterArray(r3, r5, r2, r4);
}

// static
constexpr auto ConstructWithSpreadDescriptor::registers() {
  // r2 : number of arguments (on the stack, not including receiver)
  // r3 : the target to call
  // r5 : the new target
  // r4 : the object to spread
  return RegisterArray(r3, r5, r2, r4);
}

// static
constexpr auto ConstructWithArrayLikeDescriptor::registers() {
  // r3 : the target to call
  // r5 : the new target
  // r4 : the arguments list
  return RegisterArray(r3, r5, r4);
}

// static
constexpr auto ConstructStubDescriptor::registers() {
  // r2 : number of arguments
  // r3 : the target to call
  // r5 : the new target
  // r4 : allocation site or undefined
  return RegisterArray(r3, r5, r2, r4);
}

// static
constexpr auto AbortDescriptor::registers() { return RegisterArray(r3); }

// static
constexpr auto CompareDescriptor::registers() { return RegisterArray(r3, r2); }

// static
constexpr auto Compare_BaselineDescriptor::registers() {
  // TODO(v8:11421): Implement on this platform.
  return DefaultRegisterArray();
}

// static
constexpr auto BinaryOpDescriptor::registers() { return RegisterArray(r3, r2); }

// static
constexpr auto BinaryOp_BaselineDescriptor::registers() {
  // TODO(v8:11421): Implement on this platform.
  return DefaultRegisterArray();
}

// static
constexpr auto ApiCallbackDescriptor::registers() {
  return RegisterArray(r3,   // kApiFunctionAddress
                       r4,   // kArgc
                       r5,   // kCallData
                       r2);  // kHolder
}

// static
constexpr auto InterpreterDispatchDescriptor::registers() {
  return RegisterArray(
      kInterpreterAccumulatorRegister, kInterpreterBytecodeOffsetRegister,
      kInterpreterBytecodeArrayRegister, kInterpreterDispatchTableRegister);
}

// static
constexpr auto InterpreterPushArgsThenCallDescriptor::registers() {
  return RegisterArray(r2,   // argument count (not including receiver)
                       r4,   // address of first argument
                       r3);  // the target callable to be call
}

// static
constexpr auto InterpreterPushArgsThenConstructDescriptor::registers() {
  return RegisterArray(
      r2,   // argument count (not including receiver)
      r6,   // address of the first argument
      r3,   // constructor to call
      r5,   // new target
      r4);  // allocation site feedback if available, undefined otherwise
}

// static
constexpr auto ResumeGeneratorDescriptor::registers() {
  return RegisterArray(r2,   // the value to pass to the generator
                       r3);  // the JSGeneratorObject to resume
}

// static
constexpr auto RunMicrotasksEntryDescriptor::registers() {
  return RegisterArray(r2, r3);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390

#endif  // V8_CODEGEN_S390_INTERFACE_DESCRIPTORS_S390_INL_H_

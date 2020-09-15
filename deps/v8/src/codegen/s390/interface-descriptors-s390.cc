// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/codegen/interface-descriptors.h"

#include "src/execution/frames.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return cp; }

void CallInterfaceDescriptor::DefaultInitializePlatformSpecific(
    CallInterfaceDescriptorData* data, int register_parameter_count) {
  const Register default_stub_registers[] = {r2, r3, r4, r5, r6};
  CHECK_LE(static_cast<size_t>(register_parameter_count),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(register_parameter_count,
                                   default_stub_registers);
}

void RecordWriteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  const Register default_stub_registers[] = {r2, r3, r4, r5, r6};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

void EphemeronKeyBarrierDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  const Register default_stub_registers[] = {r2, r3, r4, r5, r6};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

const Register LoadDescriptor::ReceiverRegister() { return r3; }
const Register LoadDescriptor::NameRegister() { return r4; }
const Register LoadDescriptor::SlotRegister() { return r2; }

const Register LoadWithVectorDescriptor::VectorRegister() { return r5; }

const Register
LoadWithReceiverAndVectorDescriptor::LookupStartObjectRegister() {
  return r6;
}

const Register StoreDescriptor::ReceiverRegister() { return r3; }
const Register StoreDescriptor::NameRegister() { return r4; }
const Register StoreDescriptor::ValueRegister() { return r2; }
const Register StoreDescriptor::SlotRegister() { return r6; }

const Register StoreWithVectorDescriptor::VectorRegister() { return r5; }

const Register StoreTransitionDescriptor::SlotRegister() { return r6; }
const Register StoreTransitionDescriptor::VectorRegister() { return r5; }
const Register StoreTransitionDescriptor::MapRegister() { return r7; }

const Register ApiGetterDescriptor::HolderRegister() { return r2; }
const Register ApiGetterDescriptor::CallbackRegister() { return r5; }

const Register GrowArrayElementsDescriptor::ObjectRegister() { return r2; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return r5; }

// static
const Register TypeConversionDescriptor::ArgumentRegister() { return r2; }

void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments
  // r3 : the target to call
  Register registers[] = {r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments (on the stack, not including receiver)
  // r3 : the target to call
  // r6 : arguments list length (untagged)
  // r4 : arguments list (FixedArray)
  Register registers[] = {r3, r2, r6, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments
  // r4 : start index (to support rest parameters)
  // r3 : the target to call
  Register registers[] = {r3, r2, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallFunctionTemplateDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : function template info
  // r4 : number of arguments (on the stack, not including receiver)
  Register registers[] = {r3, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments (on the stack, not including receiver)
  // r3 : the target to call
  // r4 : the object to spread
  Register registers[] = {r3, r2, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : the target to call
  // r4 : the arguments list
  Register registers[] = {r3, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments (on the stack, not including receiver)
  // r3 : the target to call
  // r5 : the new target
  // r6 : arguments list length (untagged)
  // r4 : arguments list (FixedArray)
  Register registers[] = {r3, r5, r2, r6, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments
  // r5 : the new target
  // r4 : start index (to support rest parameters)
  // r3 : the target to call
  Register registers[] = {r3, r5, r2, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments (on the stack, not including receiver)
  // r3 : the target to call
  // r5 : the new target
  // r4 : the object to spread
  Register registers[] = {r3, r5, r2, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : the target to call
  // r5 : the new target
  // r4 : the arguments list
  Register registers[] = {r3, r5, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructStubDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments
  // r3 : the target to call
  // r5 : the new target
  // r4 : allocation site or undefined
  Register registers[] = {r3, r5, r2, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AbortDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ArgumentsAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r3,  // JSFunction
      r5,  // the new target
      r2,  // actual number of arguments
      r4,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ApiCallbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r3,  // kApiFunctionAddress
      r4,  // kArgc
      r5,  // kCallData
      r2,  // kHolder
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
      r2,  // argument count (not including receiver)
      r4,  // address of first argument
      r3   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsThenConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r2,  // argument count (not including receiver)
      r6,  // address of the first argument
      r3,  // constructor to call
      r5,  // new target
      r4,  // allocation site feedback if available, undefined otherwise
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ResumeGeneratorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r2,  // the value to pass to the generator
      r3   // the JSGeneratorObject to resume
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FrameDropperTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r3,  // loaded new FP
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void RunMicrotasksEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r2, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void BinaryOp_WithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // TODO(v8:8888): Implement on this platform.
  DefaultInitializePlatformSpecific(data, 4);
}

void CallTrampoline_WithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // TODO(v8:8888): Implement on this platform.
  DefaultInitializePlatformSpecific(data, 4);
}

void CallWithArrayLike_WithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // TODO(v8:8888): Implement on this platform.
  DefaultInitializePlatformSpecific(data, 4);
}

void CallWithSpread_WithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // TODO(v8:8888): Implement on this platform.
  DefaultInitializePlatformSpecific(data, 4);
}

void ConstructWithArrayLike_WithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // TODO(v8:8888): Implement on this platform.
  DefaultInitializePlatformSpecific(data, 4);
}

void ConstructWithSpread_WithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // TODO(v8:8888): Implement on this platform.
  DefaultInitializePlatformSpecific(data, 4);
}

void Compare_WithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // TODO(v8:8888): Implement on this platform.
  DefaultInitializePlatformSpecific(data, 4);
}

void UnaryOp_WithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // TODO(v8:8888): Implement on this platform.
  DefaultInitializePlatformSpecific(data, 3);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390

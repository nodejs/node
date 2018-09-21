// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return cp; }

void CallInterfaceDescriptor::DefaultInitializePlatformSpecific(
    CallInterfaceDescriptorData* data, int register_parameter_count) {
  const Register default_stub_registers[] = {r0, r1, r2, r3, r4};
  CHECK_LE(static_cast<size_t>(register_parameter_count),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(register_parameter_count,
                                   default_stub_registers);
}

void RecordWriteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  const Register default_stub_registers[] = {r0, r1, r2, r3, r4};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

const Register FastNewFunctionContextDescriptor::ScopeInfoRegister() {
  return r1;
}
const Register FastNewFunctionContextDescriptor::SlotsRegister() { return r0; }

const Register LoadDescriptor::ReceiverRegister() { return r1; }
const Register LoadDescriptor::NameRegister() { return r2; }
const Register LoadDescriptor::SlotRegister() { return r0; }

const Register LoadWithVectorDescriptor::VectorRegister() { return r3; }

const Register StoreDescriptor::ReceiverRegister() { return r1; }
const Register StoreDescriptor::NameRegister() { return r2; }
const Register StoreDescriptor::ValueRegister() { return r0; }
const Register StoreDescriptor::SlotRegister() { return r4; }

const Register StoreWithVectorDescriptor::VectorRegister() { return r3; }

const Register StoreTransitionDescriptor::SlotRegister() { return r4; }
const Register StoreTransitionDescriptor::VectorRegister() { return r3; }
const Register StoreTransitionDescriptor::MapRegister() { return r5; }

const Register ApiGetterDescriptor::HolderRegister() { return r0; }
const Register ApiGetterDescriptor::CallbackRegister() { return r3; }

const Register GrowArrayElementsDescriptor::ObjectRegister() { return r0; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return r3; }


// static
const Register TypeConversionDescriptor::ArgumentRegister() { return r0; }

void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments
  // r1 : the target to call
  Register registers[] = {r1, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments (on the stack, not including receiver)
  // r1 : the target to call
  // r2 : arguments list (FixedArray)
  // r4 : arguments list length (untagged)
  Register registers[] = {r1, r0, r2, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments
  // r2 : start index (to support rest parameters)
  // r1 : the target to call
  Register registers[] = {r1, r0, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments (on the stack, not including receiver)
  // r1 : the target to call
  // r2 : the object to spread
  Register registers[] = {r1, r0, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r1 : the target to call
  // r2 : the arguments list
  Register registers[] = {r1, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments (on the stack, not including receiver)
  // r1 : the target to call
  // r3 : the new target
  // r2 : arguments list (FixedArray)
  // r4 : arguments list length (untagged)
  Register registers[] = {r1, r3, r0, r2, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments
  // r3 : the new target
  // r2 : start index (to support rest parameters)
  // r1 : the target to call
  Register registers[] = {r1, r3, r0, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments (on the stack, not including receiver)
  // r1 : the target to call
  // r3 : the new target
  // r2 : the object to spread
  Register registers[] = {r1, r3, r0, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r1 : the target to call
  // r3 : the new target
  // r2 : the arguments list
  Register registers[] = {r1, r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructStubDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments
  // r1 : the target to call
  // r3 : the new target
  // r2 : allocation site or undefined
  Register registers[] = {r1, r3, r0, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AbortDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ArgumentAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r1,  // JSFunction
      r3,  // the new target
      r0,  // actual number of arguments
      r2,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ApiCallbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      JavaScriptFrame::context_register(),  // callee context
      r4,                                   // call_data
      r2,                                   // holder
      r1,                                   // api_function_address
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
      r0,  // argument count (not including receiver)
      r2,  // address of first argument
      r1   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsThenConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r0,  // argument count (not including receiver)
      r3,  // new target
      r1,  // constructor to call
      r2,  // allocation site feedback if available, undefined otherwise
      r4   // address of the first argument
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ResumeGeneratorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r0,  // the value to pass to the generator
      r1   // the JSGeneratorObject to resume
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FrameDropperTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r1,  // loaded new FP
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/codegen/interface-descriptors.h"

#include "src/execution/frames.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return cp; }

void CallInterfaceDescriptor::DefaultInitializePlatformSpecific(
    CallInterfaceDescriptorData* data, int register_parameter_count) {
  const Register default_stub_registers[] = {x0, x1, x2, x3, x4};
  CHECK_LE(static_cast<size_t>(register_parameter_count),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(register_parameter_count,
                                   default_stub_registers);
}

void RecordWriteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  const Register default_stub_registers[] = {x0, x1, x2, x3, x4};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

void DynamicCheckMapsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register default_stub_registers[] = {x0, x1, x2, x3, cp};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

void EphemeronKeyBarrierDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  const Register default_stub_registers[] = {x0, x1, x2, x3, x4};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

const Register LoadDescriptor::ReceiverRegister() { return x1; }
const Register LoadDescriptor::NameRegister() { return x2; }
const Register LoadDescriptor::SlotRegister() { return x0; }

const Register LoadWithVectorDescriptor::VectorRegister() { return x3; }

const Register
LoadWithReceiverAndVectorDescriptor::LookupStartObjectRegister() {
  return x4;
}

const Register StoreDescriptor::ReceiverRegister() { return x1; }
const Register StoreDescriptor::NameRegister() { return x2; }
const Register StoreDescriptor::ValueRegister() { return x0; }
const Register StoreDescriptor::SlotRegister() { return x4; }

const Register StoreWithVectorDescriptor::VectorRegister() { return x3; }

const Register StoreTransitionDescriptor::SlotRegister() { return x4; }
const Register StoreTransitionDescriptor::VectorRegister() { return x3; }
const Register StoreTransitionDescriptor::MapRegister() { return x5; }

const Register ApiGetterDescriptor::HolderRegister() { return x0; }
const Register ApiGetterDescriptor::CallbackRegister() { return x3; }

const Register GrowArrayElementsDescriptor::ObjectRegister() { return x0; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return x3; }

// static
const Register TypeConversionDescriptor::ArgumentRegister() { return x0; }

void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {x3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: target
  // x0: number of arguments
  Register registers[] = {x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x0 : number of arguments (on the stack, not including receiver)
  // x1 : the target to call
  // x4 : arguments list length (untagged)
  // x2 : arguments list (FixedArray)
  Register registers[] = {x1, x0, x4, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: target
  // x0: number of arguments
  // x2: start index (to supported rest parameters)
  Register registers[] = {x1, x0, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallFunctionTemplateDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1 : function template info
  // x2 : number of arguments (on the stack, not including receiver)
  Register registers[] = {x1, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x0 : number of arguments (on the stack, not including receiver)
  // x1 : the target to call
  // x2 : the object to spread
  Register registers[] = {x1, x0, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1 : the target to call
  // x2 : the arguments list
  Register registers[] = {x1, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x0 : number of arguments (on the stack, not including receiver)
  // x1 : the target to call
  // x3 : the new target
  // x4 : arguments list length (untagged)
  // x2 : arguments list (FixedArray)
  Register registers[] = {x1, x3, x0, x4, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x3: new target
  // x1: target
  // x0: number of arguments
  // x2: start index (to supported rest parameters)
  Register registers[] = {x1, x3, x0, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x0 : number of arguments (on the stack, not including receiver)
  // x1 : the target to call
  // x3 : the new target
  // x2 : the object to spread
  Register registers[] = {x1, x3, x0, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1 : the target to call
  // x3 : the new target
  // x2 : the arguments list
  Register registers[] = {x1, x3, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructStubDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x3: new target
  // x1: target
  // x0: number of arguments
  // x2: allocation site or undefined
  Register registers[] = {x1, x3, x0, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AbortDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: left operand
  // x0: right operand
  Register registers[] = {x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: left operand
  // x0: right operand
  Register registers[] = {x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ArgumentsAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      x1,  // JSFunction
      x3,  // the new target
      x0,  // actual number of arguments
      x2,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ApiCallbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      x1,  // kApiFunctionAddress
      x2,  // kArgc
      x3,  // kCallData
      x0,  // kHolder
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
      x0,  // argument count (not including receiver)
      x2,  // address of first argument
      x1   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsThenConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      x0,  // argument count (not including receiver)
      x4,  // address of the first argument
      x1,  // constructor to call
      x3,  // new target
      x2,  // allocation site feedback if available, undefined otherwise
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ResumeGeneratorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      x0,  // the value to pass to the generator
      x1   // the JSGeneratorObject to resume
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FrameDropperTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      x1,  // loaded new FP
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void RunMicrotasksEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {x0, x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64

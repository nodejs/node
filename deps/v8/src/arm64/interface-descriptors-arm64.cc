// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arm64/interface-descriptors-arm64.h"

#if V8_TARGET_ARCH_ARM64

#include "src/interface-descriptors.h"

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

const Register FastNewFunctionContextDescriptor::FunctionRegister() {
  return x1;
}
const Register FastNewFunctionContextDescriptor::SlotsRegister() { return x0; }

const Register LoadDescriptor::ReceiverRegister() { return x1; }
const Register LoadDescriptor::NameRegister() { return x2; }
const Register LoadDescriptor::SlotRegister() { return x0; }

const Register LoadWithVectorDescriptor::VectorRegister() { return x3; }

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

const Register MathPowTaggedDescriptor::exponent() { return x11; }


const Register MathPowIntegerDescriptor::exponent() { return x12; }


const Register GrowArrayElementsDescriptor::ObjectRegister() { return x0; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return x3; }


void FastNewClosureDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: function info
  // x2: feedback vector
  // x3: slot
  Register registers[] = {x1, x2, x3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

// static
const Register TypeConversionDescriptor::ArgumentRegister() { return x0; }

void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {x3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1  function    the function to call
  Register registers[] = {x1};
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
  // x2 : arguments list (FixedArray)
  // x4 : arguments list length (untagged)
  Register registers[] = {x1, x0, x2, x4};
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
  // x2 : arguments list (FixedArray)
  // x4 : arguments list length (untagged)
  Register registers[] = {x1, x3, x0, x2, x4};
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


void ConstructTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x3: new target
  // x1: target
  // x0: number of arguments
  Register registers[] = {x1, x3, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void TransitionElementsKindDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x0: value (js_array)
  // x1: to_map
  Register registers[] = {x0, x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AbortJSDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr, nullptr);
}

void ArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kActualArgumentsCount, kAllocationSite
  Register registers[] = {x1, x3, x0, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void ArrayNoArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // x1: function
  // x2: allocation site with elements kind
  // x0: number of arguments to the constructor function
  Register registers[] = {x1, x2, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void ArraySingleArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // x0: number of arguments
  // x1: function
  // x2: allocation site with elements kind
  Register registers[] = {x1, x2, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {x1, x2, x0};
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

void StringAddDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: left operand
  // x0: right operand
  Register registers[] = {x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ArgumentAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      x1,  // JSFunction
      x3,  // the new target
      x0,  // actual number of arguments
      x2,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &default_descriptor);
}

void ApiCallbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      JavaScriptFrame::context_register(),  // callee context
      x4,                                   // call_data
      x2,                                   // holder
      x1,                                   // api_function_address
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &default_descriptor);
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
      x3,  // new target
      x1,  // constructor to call
      x2,  // allocation site feedback if available, undefined otherwise
      x4   // address of the first argument
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterCEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      x0,   // argument count (argc)
      x11,  // address of first argument (argv)
      x1    // the runtime function to call
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

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64

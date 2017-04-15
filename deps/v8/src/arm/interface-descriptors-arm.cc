// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arm/interface-descriptors-arm.h"

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

const Register FastNewFunctionContextDescriptor::FunctionRegister() {
  return r1;
}
const Register FastNewFunctionContextDescriptor::SlotsRegister() { return r0; }

const Register LoadDescriptor::ReceiverRegister() { return r1; }
const Register LoadDescriptor::NameRegister() { return r2; }
const Register LoadDescriptor::SlotRegister() { return r0; }

const Register LoadWithVectorDescriptor::VectorRegister() { return r3; }

const Register LoadICProtoArrayDescriptor::HandlerRegister() { return r4; }

const Register StoreDescriptor::ReceiverRegister() { return r1; }
const Register StoreDescriptor::NameRegister() { return r2; }
const Register StoreDescriptor::ValueRegister() { return r0; }
const Register StoreDescriptor::SlotRegister() { return r4; }

const Register StoreWithVectorDescriptor::VectorRegister() { return r3; }

const Register StoreTransitionDescriptor::SlotRegister() { return r4; }
const Register StoreTransitionDescriptor::VectorRegister() { return r3; }
const Register StoreTransitionDescriptor::MapRegister() { return r5; }

const Register StringCompareDescriptor::LeftRegister() { return r1; }
const Register StringCompareDescriptor::RightRegister() { return r0; }

const Register ApiGetterDescriptor::HolderRegister() { return r0; }
const Register ApiGetterDescriptor::CallbackRegister() { return r3; }

const Register MathPowTaggedDescriptor::exponent() { return r2; }


const Register MathPowIntegerDescriptor::exponent() {
  return MathPowTaggedDescriptor::exponent();
}


const Register GrowArrayElementsDescriptor::ObjectRegister() { return r0; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return r3; }


void FastNewClosureDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1, r2, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FastNewRestParameterDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastNewSloppyArgumentsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastNewStrictArgumentsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


// static
const Register TypeConversionDescriptor::ArgumentRegister() { return r0; }

void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneRegExpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r2, r1, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneShallowArrayDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r2, r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneShallowObjectDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r2, r1, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CreateAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r2, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CreateWeakCellDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r2, r3, r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionWithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionWithFeedbackAndVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1, r0, r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments
  // r1 : the function to call
  // r2 : feedback vector
  // r3 : slot in feedback vector (Smi, for RecordCallTarget)
  // r4 : new target (for IsSuperConstructorCall)
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {r0, r1, r4, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments
  // r1 : the target to call
  Register registers[] = {r1, r0};
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


void ConstructTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments
  // r1 : the target to call
  // r3 : the new target
  Register registers[] = {r1, r3, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void TransitionElementsKindDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r0, r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr, nullptr);
}

#define SIMD128_ALLOC_DESC(TYPE, Type, type, lane_count, lane_type) \
  void Allocate##Type##Descriptor::InitializePlatformSpecific(      \
      CallInterfaceDescriptorData* data) {                          \
    data->InitializePlatformSpecific(0, nullptr, nullptr);          \
  }
SIMD128_TYPES(SIMD128_ALLOC_DESC)
#undef SIMD128_ALLOC_DESC

void ArrayNoArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // r0 -- number of arguments
  // r1 -- function
  // r2 -- allocation site with elements kind
  Register registers[] = {r1, r2, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}

void ArraySingleArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // r0 -- number of arguments
  // r1 -- function
  // r2 -- allocation site with elements kind
  Register registers[] = {r1, r2, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}

void ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {r1, r2, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void VarArgFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (arg count)
  Register registers[] = {r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
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


void BinaryOpWithAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r2, r1, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void BinaryOpWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // r1 -- lhs
  // r0 -- rhs
  // r4 -- slot id
  // r3 -- vector
  Register registers[] = {r1, r0, r4, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CountOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void StringAddDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void KeyedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor noInlineDescriptor =
      PlatformInterfaceDescriptor(NEVER_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      r2,  // key
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &noInlineDescriptor);
}


void NamedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor noInlineDescriptor =
      PlatformInterfaceDescriptor(NEVER_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      r2,  // name
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &noInlineDescriptor);
}


void CallHandlerDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      r0,  // receiver
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &default_descriptor);
}


void ArgumentAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      r1,  // JSFunction
      r3,  // the new target
      r0,  // actual number of arguments
      r2,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &default_descriptor);
}

void ApiCallbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      r0,  // callee
      r4,  // call_data
      r2,  // holder
      r1,  // api_function_address
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

void InterpreterPushArgsAndCallDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r0,  // argument count (not including receiver)
      r2,  // address of first argument
      r1   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsAndConstructDescriptor::InitializePlatformSpecific(
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

void InterpreterPushArgsAndConstructArrayDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r0,  // argument count (not including receiver)
      r1,  // target to call checked to be Array function
      r2,  // allocation site feedback if available, undefined otherwise
      r3   // address of the first argument
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterCEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r0,  // argument count (argc)
      r2,  // address of first argument (argv)
      r1   // the runtime function to call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ResumeGeneratorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r0,  // the value to pass to the generator
      r1,  // the JSGeneratorObject to resume
      r2   // the resume mode (tagged)
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return cp; }


const Register LoadDescriptor::ReceiverRegister() { return r1; }
const Register LoadDescriptor::NameRegister() { return r2; }
const Register LoadDescriptor::SlotRegister() { return r0; }


const Register LoadWithVectorDescriptor::VectorRegister() { return r3; }


const Register StoreDescriptor::ReceiverRegister() { return r1; }
const Register StoreDescriptor::NameRegister() { return r2; }
const Register StoreDescriptor::ValueRegister() { return r0; }


const Register VectorStoreICTrampolineDescriptor::SlotRegister() { return r4; }


const Register VectorStoreICDescriptor::VectorRegister() { return r3; }


const Register StoreTransitionDescriptor::MapRegister() { return r3; }


const Register ElementTransitionAndStoreDescriptor::MapRegister() { return r3; }


const Register InstanceofDescriptor::left() { return r0; }
const Register InstanceofDescriptor::right() { return r1; }


const Register ArgumentsAccessReadDescriptor::index() { return r1; }
const Register ArgumentsAccessReadDescriptor::parameter_count() { return r0; }


const Register ApiGetterDescriptor::function_address() { return r2; }


const Register MathPowTaggedDescriptor::exponent() { return r2; }


const Register MathPowIntegerDescriptor::exponent() {
  return MathPowTaggedDescriptor::exponent();
}


const Register GrowArrayElementsDescriptor::ObjectRegister() { return r0; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return r3; }


void FastNewClosureDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastNewContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ToNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void NumberToStringDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3};
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


void StoreArrayLiteralElementDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r0};
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
  Register registers[] = {r1, r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r0 : number of arguments
  // r1 : the function to call
  // r2 : feedback vector
  // r3 : (only if r2 is not the megamorphic symbol) slot in feedback
  //      vector (Smi)
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {r0, r1, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void RegExpConstructResultDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r2, r1, r0};
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


void ArrayConstructorConstantArgCountDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // r0 -- number of arguments
  // r1 -- function
  // r2 -- allocation site with elements kind
  Register registers[] = {r1, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {r1, r2, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InternalArrayConstructorConstantArgCountDescriptor::
    InitializePlatformSpecific(CallInterfaceDescriptorData* data) {
  // register state
  // r0 -- number of arguments
  // r1 -- constructor function
  Register registers[] = {r1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InternalArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {r1, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r1, r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CompareNilDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ToBooleanDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r0};
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
      r0,  // actual number of arguments
      r2,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &default_descriptor);
}


void ApiFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      r0,  // callee
      r4,  // call_data
      r2,  // holder
      r1,  // api_function_address
      r3,  // actual number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &default_descriptor);
}


void ApiAccessorDescriptor::InitializePlatformSpecific(
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


void MathRoundVariantDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r1,  // math rounding function
      r3,  // vector slot id
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

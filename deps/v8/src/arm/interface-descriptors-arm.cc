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


const Register VectorLoadICTrampolineDescriptor::SlotRegister() { return r0; }


const Register VectorLoadICDescriptor::VectorRegister() { return r3; }


const Register StoreDescriptor::ReceiverRegister() { return r1; }
const Register StoreDescriptor::NameRegister() { return r2; }
const Register StoreDescriptor::ValueRegister() { return r0; }


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


void FastNewClosureDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r2};
  data->Initialize(arraysize(registers), registers, NULL);
}


void FastNewContextDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r1};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ToNumberDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void NumberToStringDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void FastCloneShallowArrayDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r3, r2, r1};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(), Representation::Smi(),
      Representation::Tagged()};
  data->Initialize(arraysize(registers), registers, representations);
}


void FastCloneShallowObjectDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r3, r2, r1, r0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CreateAllocationSiteDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r2, r3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void StoreArrayLiteralElementDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r3, r0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CallFunctionDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r1};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CallFunctionWithFeedbackDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r1, r3};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::Tagged(),
                                      Representation::Smi()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CallConstructDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // r0 : number of arguments
  // r1 : the function to call
  // r2 : feedback vector
  // r3 : (only if r2 is not the megamorphic symbol) slot in feedback
  //      vector (Smi)
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {cp, r0, r1, r2};
  data->Initialize(arraysize(registers), registers, NULL);
}


void RegExpConstructResultDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r2, r1, r0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void TransitionElementsKindDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r0, r1};
  data->Initialize(arraysize(registers), registers, NULL);
}


void AllocateHeapNumberDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // register state
  // cp -- context
  Register registers[] = {cp};
  data->Initialize(arraysize(registers), registers, nullptr);
}


void ArrayConstructorConstantArgCountDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // register state
  // cp -- context
  // r0 -- number of arguments
  // r1 -- function
  // r2 -- allocation site with elements kind
  Register registers[] = {cp, r1, r2};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ArrayConstructorDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {cp, r1, r2, r0};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(),
      Representation::Tagged(), Representation::Integer32()};
  data->Initialize(arraysize(registers), registers, representations);
}


void InternalArrayConstructorConstantArgCountDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // register state
  // cp -- context
  // r0 -- number of arguments
  // r1 -- constructor function
  Register registers[] = {cp, r1};
  data->Initialize(arraysize(registers), registers, NULL);
}


void InternalArrayConstructorDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {cp, r1, r0};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::Tagged(),
                                      Representation::Integer32()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CompareNilDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ToBooleanDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void BinaryOpDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r1, r0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void BinaryOpWithAllocationSiteDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r2, r1, r0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void StringAddDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r1, r0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void KeyedDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor noInlineDescriptor =
      PlatformInterfaceDescriptor(NEVER_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      cp,  // context
      r2,  // key
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // key
  };
  data->Initialize(arraysize(registers), registers, representations,
                   &noInlineDescriptor);
}


void NamedDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor noInlineDescriptor =
      PlatformInterfaceDescriptor(NEVER_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      cp,  // context
      r2,  // name
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // name
  };
  data->Initialize(arraysize(registers), registers, representations,
                   &noInlineDescriptor);
}


void CallHandlerDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      cp,  // context
      r0,  // receiver
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // receiver
  };
  data->Initialize(arraysize(registers), registers, representations,
                   &default_descriptor);
}


void ArgumentAdaptorDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      cp,  // context
      r1,  // JSFunction
      r0,  // actual number of arguments
      r2,  // expected number of arguments
  };
  Representation representations[] = {
      Representation::Tagged(),     // context
      Representation::Tagged(),     // JSFunction
      Representation::Integer32(),  // actual number of arguments
      Representation::Integer32(),  // expected number of arguments
  };
  data->Initialize(arraysize(registers), registers, representations,
                   &default_descriptor);
}


void ApiFunctionDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      cp,  // context
      r0,  // callee
      r4,  // call_data
      r2,  // holder
      r1,  // api_function_address
  };
  Representation representations[] = {
      Representation::Tagged(),    // context
      Representation::Tagged(),    // callee
      Representation::Tagged(),    // call_data
      Representation::Tagged(),    // holder
      Representation::External(),  // api_function_address
  };
  data->Initialize(arraysize(registers), registers, representations,
                   &default_descriptor);
}
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM

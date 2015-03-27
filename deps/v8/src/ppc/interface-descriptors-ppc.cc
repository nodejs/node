// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_PPC

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return cp; }


const Register LoadDescriptor::ReceiverRegister() { return r4; }
const Register LoadDescriptor::NameRegister() { return r5; }


const Register VectorLoadICTrampolineDescriptor::SlotRegister() { return r3; }


const Register VectorLoadICDescriptor::VectorRegister() { return r6; }


const Register StoreDescriptor::ReceiverRegister() { return r4; }
const Register StoreDescriptor::NameRegister() { return r5; }
const Register StoreDescriptor::ValueRegister() { return r3; }


const Register StoreTransitionDescriptor::MapRegister() { return r6; }


const Register ElementTransitionAndStoreDescriptor::MapRegister() { return r6; }


const Register InstanceofDescriptor::left() { return r3; }
const Register InstanceofDescriptor::right() { return r4; }


const Register ArgumentsAccessReadDescriptor::index() { return r4; }
const Register ArgumentsAccessReadDescriptor::parameter_count() { return r3; }


const Register ApiGetterDescriptor::function_address() { return r5; }


const Register MathPowTaggedDescriptor::exponent() { return r5; }


const Register MathPowIntegerDescriptor::exponent() {
  return MathPowTaggedDescriptor::exponent();
}


void FastNewClosureDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r5};
  data->Initialize(arraysize(registers), registers, NULL);
}


void FastNewContextDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r4};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ToNumberDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void NumberToStringDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void FastCloneShallowArrayDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r6, r5, r4};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(), Representation::Smi(),
      Representation::Tagged()};
  data->Initialize(arraysize(registers), registers, representations);
}


void FastCloneShallowObjectDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r6, r5, r4, r3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CreateAllocationSiteDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r5, r6};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::Tagged(),
                                      Representation::Smi()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CreateWeakCellDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r5, r6, r4};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(), Representation::Smi(),
      Representation::Tagged()};
  data->Initialize(arraysize(registers), registers, representations);
}


void StoreArrayLiteralElementDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r6, r3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CallFunctionDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r4};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CallFunctionWithFeedbackDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r4, r6};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::Tagged(),
                                      Representation::Smi()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CallFunctionWithFeedbackAndVectorDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r4, r6, r5};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(), Representation::Smi(),
      Representation::Tagged()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CallConstructDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // r3 : number of arguments
  // r4 : the function to call
  // r5 : feedback vector
  // r6 : (only if r5 is not the megamorphic symbol) slot in feedback
  //      vector (Smi)
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {cp, r3, r4, r5};
  data->Initialize(arraysize(registers), registers, NULL);
}


void RegExpConstructResultDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r5, r4, r3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void TransitionElementsKindDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r3, r4};
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
  // r3 -- number of arguments
  // r4 -- function
  // r5 -- allocation site with elements kind
  Register registers[] = {cp, r4, r5};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ArrayConstructorDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {cp, r4, r5, r3};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(),
      Representation::Tagged(), Representation::Integer32()};
  data->Initialize(arraysize(registers), registers, representations);
}


void InternalArrayConstructorConstantArgCountDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // register state
  // cp -- context
  // r3 -- number of arguments
  // r4 -- constructor function
  Register registers[] = {cp, r4};
  data->Initialize(arraysize(registers), registers, NULL);
}


void InternalArrayConstructorDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {cp, r4, r3};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::Tagged(),
                                      Representation::Integer32()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CompareNilDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ToBooleanDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void BinaryOpDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r4, r3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void BinaryOpWithAllocationSiteDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r5, r4, r3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void StringAddDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, r4, r3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void KeyedDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      cp,  // context
      r5,  // key
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // key
  };
  data->Initialize(arraysize(registers), registers, representations);
}


void NamedDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      cp,  // context
      r5,  // name
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // name
  };
  data->Initialize(arraysize(registers), registers, representations);
}


void CallHandlerDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      cp,  // context
      r3,  // receiver
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // receiver
  };
  data->Initialize(arraysize(registers), registers, representations);
}


void ArgumentAdaptorDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      cp,  // context
      r4,  // JSFunction
      r3,  // actual number of arguments
      r5,  // expected number of arguments
  };
  Representation representations[] = {
      Representation::Tagged(),     // context
      Representation::Tagged(),     // JSFunction
      Representation::Integer32(),  // actual number of arguments
      Representation::Integer32(),  // expected number of arguments
  };
  data->Initialize(arraysize(registers), registers, representations);
}


void ApiFunctionDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      cp,  // context
      r3,  // callee
      r7,  // call_data
      r5,  // holder
      r4,  // api_function_address
      r6,  // actual number of arguments
  };
  Representation representations[] = {
      Representation::Tagged(),     // context
      Representation::Tagged(),     // callee
      Representation::Tagged(),     // call_data
      Representation::Tagged(),     // holder
      Representation::External(),   // api_function_address
      Representation::Integer32(),  // actual number of arguments
  };
  data->Initialize(arraysize(registers), registers, representations);
}


void ApiAccessorDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      cp,  // context
      r3,  // callee
      r7,  // call_data
      r5,  // holder
      r4,  // api_function_address
  };
  Representation representations[] = {
      Representation::Tagged(),    // context
      Representation::Tagged(),    // callee
      Representation::Tagged(),    // call_data
      Representation::Tagged(),    // holder
      Representation::External(),  // api_function_address
  };
  data->Initialize(arraysize(registers), registers, representations);
}
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_PPC

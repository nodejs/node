// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM64

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return cp; }


const Register LoadDescriptor::ReceiverRegister() { return x1; }
const Register LoadDescriptor::NameRegister() { return x2; }


const Register VectorLoadICTrampolineDescriptor::SlotRegister() { return x0; }


const Register VectorLoadICDescriptor::VectorRegister() { return x3; }


const Register StoreDescriptor::ReceiverRegister() { return x1; }
const Register StoreDescriptor::NameRegister() { return x2; }
const Register StoreDescriptor::ValueRegister() { return x0; }


const Register StoreTransitionDescriptor::MapRegister() { return x3; }


const Register ElementTransitionAndStoreDescriptor::MapRegister() { return x3; }


const Register InstanceofDescriptor::left() {
  // Object to check (instanceof lhs).
  return x11;
}


const Register InstanceofDescriptor::right() {
  // Constructor function (instanceof rhs).
  return x10;
}


const Register ArgumentsAccessReadDescriptor::index() { return x1; }
const Register ArgumentsAccessReadDescriptor::parameter_count() { return x0; }


const Register ApiGetterDescriptor::function_address() { return x2; }


const Register MathPowTaggedDescriptor::exponent() { return x11; }


const Register MathPowIntegerDescriptor::exponent() { return x12; }


void FastNewClosureDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // cp: context
  // x2: function info
  Register registers[] = {cp, x2};
  data->Initialize(arraysize(registers), registers, NULL);
}


void FastNewContextDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // cp: context
  // x1: function
  Register registers[] = {cp, x1};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ToNumberDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // cp: context
  // x0: value
  Register registers[] = {cp, x0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void NumberToStringDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // cp: context
  // x0: value
  Register registers[] = {cp, x0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void FastCloneShallowArrayDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // cp: context
  // x3: array literals array
  // x2: array literal index
  // x1: constant elements
  Register registers[] = {cp, x3, x2, x1};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(), Representation::Smi(),
      Representation::Tagged()};
  data->Initialize(arraysize(registers), registers, representations);
}


void FastCloneShallowObjectDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // cp: context
  // x3: object literals array
  // x2: object literal index
  // x1: constant properties
  // x0: object literal flags
  Register registers[] = {cp, x3, x2, x1, x0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CreateAllocationSiteDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // cp: context
  // x2: feedback vector
  // x3: call feedback slot
  Register registers[] = {cp, x2, x3};
  data->Initialize(arraysize(registers), registers, NULL);
}


void StoreArrayLiteralElementDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, x3, x0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CallFunctionDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // x1  function    the function to call
  Register registers[] = {cp, x1};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CallFunctionWithFeedbackDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {cp, x1, x3};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::Tagged(),
                                      Representation::Smi()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CallConstructDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // x0 : number of arguments
  // x1 : the function to call
  // x2 : feedback vector
  // x3 : slot in feedback vector (smi) (if r2 is not the megamorphic symbol)
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {cp, x0, x1, x2};
  data->Initialize(arraysize(registers), registers, NULL);
}


void RegExpConstructResultDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // cp: context
  // x2: length
  // x1: index (of last match)
  // x0: string
  Register registers[] = {cp, x2, x1, x0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void TransitionElementsKindDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // cp: context
  // x0: value (js_array)
  // x1: to_map
  Register registers[] = {cp, x0, x1};
  data->Initialize(arraysize(registers), registers, NULL);
}


void AllocateHeapNumberDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // cp: context
  Register registers[] = {cp};
  data->Initialize(arraysize(registers), registers, nullptr);
}


void ArrayConstructorConstantArgCountDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // cp: context
  // x1: function
  // x2: allocation site with elements kind
  // x0: number of arguments to the constructor function
  Register registers[] = {cp, x1, x2};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ArrayConstructorDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {cp, x1, x2, x0};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(),
      Representation::Tagged(), Representation::Integer32()};
  data->Initialize(arraysize(registers), registers, representations);
}


void InternalArrayConstructorConstantArgCountDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // cp: context
  // x1: constructor function
  // x0: number of arguments to the constructor function
  Register registers[] = {cp, x1};
  data->Initialize(arraysize(registers), registers, NULL);
}


void InternalArrayConstructorDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {cp, x1, x0};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::Tagged(),
                                      Representation::Integer32()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CompareNilDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // cp: context
  // x0: value to compare
  Register registers[] = {cp, x0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ToBooleanDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // cp: context
  // x0: value
  Register registers[] = {cp, x0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void BinaryOpDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // cp: context
  // x1: left operand
  // x0: right operand
  Register registers[] = {cp, x1, x0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void BinaryOpWithAllocationSiteDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // cp: context
  // x2: allocation site
  // x1: left operand
  // x0: right operand
  Register registers[] = {cp, x2, x1, x0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void StringAddDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // cp: context
  // x1: left operand
  // x0: right operand
  Register registers[] = {cp, x1, x0};
  data->Initialize(arraysize(registers), registers, NULL);
}


void KeyedDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor noInlineDescriptor =
      PlatformInterfaceDescriptor(NEVER_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      cp,  // context
      x2,  // key
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
      x2,  // name
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
      x0,  // receiver
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
      x1,  // JSFunction
      x0,  // actual number of arguments
      x2,  // expected number of arguments
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
      x0,  // callee
      x4,  // call_data
      x2,  // holder
      x1,  // api_function_address
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

#endif  // V8_TARGET_ARCH_ARM64

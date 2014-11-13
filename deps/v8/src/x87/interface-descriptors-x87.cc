// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X87

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return esi; }


const Register LoadDescriptor::ReceiverRegister() { return edx; }
const Register LoadDescriptor::NameRegister() { return ecx; }


const Register VectorLoadICTrampolineDescriptor::SlotRegister() { return eax; }


const Register VectorLoadICDescriptor::VectorRegister() { return ebx; }


const Register StoreDescriptor::ReceiverRegister() { return edx; }
const Register StoreDescriptor::NameRegister() { return ecx; }
const Register StoreDescriptor::ValueRegister() { return eax; }


const Register StoreTransitionDescriptor::MapRegister() { return ebx; }


const Register ElementTransitionAndStoreDescriptor::MapRegister() {
  return ebx;
}


const Register InstanceofDescriptor::left() { return eax; }
const Register InstanceofDescriptor::right() { return edx; }


const Register ArgumentsAccessReadDescriptor::index() { return edx; }
const Register ArgumentsAccessReadDescriptor::parameter_count() { return eax; }


const Register ApiGetterDescriptor::function_address() { return edx; }


const Register MathPowTaggedDescriptor::exponent() { return eax; }


const Register MathPowIntegerDescriptor::exponent() {
  return MathPowTaggedDescriptor::exponent();
}


void FastNewClosureDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, ebx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void FastNewContextDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, edi};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ToNumberDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // ToNumberStub invokes a function, and therefore needs a context.
  Register registers[] = {esi, eax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void NumberToStringDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, eax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void FastCloneShallowArrayDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, eax, ebx, ecx};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(), Representation::Smi(),
      Representation::Tagged()};
  data->Initialize(arraysize(registers), registers, representations);
}


void FastCloneShallowObjectDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, eax, ebx, ecx, edx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CreateAllocationSiteDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, ebx, edx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void StoreArrayLiteralElementDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, ecx, eax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CallFunctionDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, edi};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CallFunctionWithFeedbackDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, edi, edx};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::Tagged(),
                                      Representation::Smi()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CallConstructDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // ebx : feedback vector
  // edx : (only if ebx is not the megamorphic symbol) slot in feedback
  //       vector (Smi)
  // edi : constructor function
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {esi, eax, edi, ebx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void RegExpConstructResultDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, ecx, ebx, eax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void TransitionElementsKindDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, eax, ebx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void AllocateHeapNumberDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // register state
  // esi -- context
  Register registers[] = {esi};
  data->Initialize(arraysize(registers), registers, nullptr);
}


void ArrayConstructorConstantArgCountDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // register state
  // eax -- number of arguments
  // edi -- function
  // ebx -- allocation site with elements kind
  Register registers[] = {esi, edi, ebx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ArrayConstructorDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {esi, edi, ebx, eax};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(),
      Representation::Tagged(), Representation::Integer32()};
  data->Initialize(arraysize(registers), registers, representations);
}


void InternalArrayConstructorConstantArgCountDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // register state
  // eax -- number of arguments
  // edi -- function
  Register registers[] = {esi, edi};
  data->Initialize(arraysize(registers), registers, NULL);
}


void InternalArrayConstructorDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {esi, edi, eax};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::Tagged(),
                                      Representation::Integer32()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CompareNilDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, eax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ToBooleanDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, eax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void BinaryOpDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, edx, eax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void BinaryOpWithAllocationSiteDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, ecx, edx, eax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void StringAddDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {esi, edx, eax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void KeyedDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      esi,  // context
      ecx,  // key
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // key
  };
  data->Initialize(arraysize(registers), registers, representations);
}


void NamedDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      esi,  // context
      ecx,  // name
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // name
  };
  data->Initialize(arraysize(registers), registers, representations);
}


void CallHandlerDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      esi,  // context
      edx,  // name
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // receiver
  };
  data->Initialize(arraysize(registers), registers, representations);
}


void ArgumentAdaptorDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      esi,  // context
      edi,  // JSFunction
      eax,  // actual number of arguments
      ebx,  // expected number of arguments
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
      esi,  // context
      eax,  // callee
      ebx,  // call_data
      ecx,  // holder
      edx,  // api_function_address
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

#endif  // V8_TARGET_ARCH_X87

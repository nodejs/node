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
const Register LoadDescriptor::SlotRegister() { return eax; }

const Register LoadWithVectorDescriptor::VectorRegister() { return ebx; }


const Register StoreDescriptor::ReceiverRegister() { return edx; }
const Register StoreDescriptor::NameRegister() { return ecx; }
const Register StoreDescriptor::ValueRegister() { return eax; }


const Register VectorStoreICTrampolineDescriptor::SlotRegister() { return edi; }


const Register VectorStoreICDescriptor::VectorRegister() { return ebx; }


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


const Register GrowArrayElementsDescriptor::ObjectRegister() { return eax; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return ebx; }


void FastNewClosureDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void FastNewContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edi};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void ToNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // ToNumberStub invokes a function, and therefore needs a context.
  Register registers[] = {eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void NumberToStringDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void FastCloneShallowArrayDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {eax, ebx, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneShallowObjectDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {eax, ebx, ecx, edx};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void CreateAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ebx, edx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CreateWeakCellDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ebx, edx, edi};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void StoreArrayLiteralElementDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ecx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void CallFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edi};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void CallFunctionWithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edi, edx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionWithFeedbackAndVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edi, edx, ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // ebx : feedback vector
  // edx : (only if ebx is not the megamorphic symbol) slot in feedback
  //       vector (Smi)
  // edi : constructor function
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {eax, edi, ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void RegExpConstructResultDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ecx, ebx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void TransitionElementsKindDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {eax, ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  data->InitializePlatformSpecific(0, nullptr, nullptr);
}


void ArrayConstructorConstantArgCountDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // eax -- number of arguments
  // edi -- function
  // ebx -- allocation site with elements kind
  Register registers[] = {edi, ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void ArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {edi, ebx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InternalArrayConstructorConstantArgCountDescriptor::
    InitializePlatformSpecific(CallInterfaceDescriptorData* data) {
  // register state
  // eax -- number of arguments
  // edi -- function
  Register registers[] = {edi};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void InternalArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {edi, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void CompareNilDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void ToBooleanDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void BinaryOpWithAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ecx, edx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void StringAddDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void KeyedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      ecx,  // key
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void NamedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      ecx,  // name
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallHandlerDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      edx,  // name
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ArgumentAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      edi,  // JSFunction
      eax,  // actual number of arguments
      ebx,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ApiFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      edi,  // callee
      ebx,  // call_data
      ecx,  // holder
      edx,  // api_function_address
      eax,  // actual number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ApiAccessorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      edi,  // callee
      ebx,  // call_data
      ecx,  // holder
      edx,  // api_function_address
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void MathRoundVariantDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      edi,  // math rounding function
      edx,  // vector slot id
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X87

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X64

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return rsi; }


const Register LoadDescriptor::ReceiverRegister() { return rdx; }
const Register LoadDescriptor::NameRegister() { return rcx; }


const Register VectorLoadICTrampolineDescriptor::SlotRegister() { return rax; }


const Register VectorLoadICDescriptor::VectorRegister() { return rbx; }


const Register StoreDescriptor::ReceiverRegister() { return rdx; }
const Register StoreDescriptor::NameRegister() { return rcx; }
const Register StoreDescriptor::ValueRegister() { return rax; }


const Register ElementTransitionAndStoreDescriptor::MapRegister() {
  return rbx;
}


const Register InstanceofDescriptor::left() { return rax; }
const Register InstanceofDescriptor::right() { return rdx; }


const Register ArgumentsAccessReadDescriptor::index() { return rdx; }
const Register ArgumentsAccessReadDescriptor::parameter_count() { return rax; }


const Register ApiGetterDescriptor::function_address() { return r8; }


const Register MathPowTaggedDescriptor::exponent() { return rdx; }


const Register MathPowIntegerDescriptor::exponent() {
  return MathPowTaggedDescriptor::exponent();
}


void FastNewClosureDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rbx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void FastNewContextDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rdi};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ToNumberDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // ToNumberStub invokes a function, and therefore needs a context.
  Register registers[] = {rsi, rax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void NumberToStringDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void FastCloneShallowArrayDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rax, rbx, rcx};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(), Representation::Smi(),
      Representation::Tagged()};
  data->Initialize(arraysize(registers), registers, representations);
}


void FastCloneShallowObjectDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rax, rbx, rcx, rdx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CreateAllocationSiteDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rbx, rdx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void StoreArrayLiteralElementDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rcx, rax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CallFunctionDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rdi};
  data->Initialize(arraysize(registers), registers, NULL);
}


void CallFunctionWithFeedbackDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rdi, rdx};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::Tagged(),
                                      Representation::Smi()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CallConstructDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // rax : number of arguments
  // rbx : feedback vector
  // rdx : (only if rbx is not the megamorphic symbol) slot in feedback
  //       vector (Smi)
  // rdi : constructor function
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {rsi, rax, rdi, rbx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void RegExpConstructResultDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rcx, rbx, rax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void TransitionElementsKindDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rax, rbx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ArrayConstructorConstantArgCountDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // register state
  // rax -- number of arguments
  // rdi -- function
  // rbx -- allocation site with elements kind
  Register registers[] = {rsi, rdi, rbx};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ArrayConstructorDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {rsi, rdi, rbx, rax};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(),
      Representation::Tagged(), Representation::Integer32()};
  data->Initialize(arraysize(registers), registers, representations);
}


void InternalArrayConstructorConstantArgCountDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // register state
  // rsi -- context
  // rax -- number of arguments
  // rdi -- constructor function
  Register registers[] = {rsi, rdi};
  data->Initialize(arraysize(registers), registers, NULL);
}


void InternalArrayConstructorDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {rsi, rdi, rax};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::Tagged(),
                                      Representation::Integer32()};
  data->Initialize(arraysize(registers), registers, representations);
}


void CompareNilDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ToBooleanDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void BinaryOpDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rdx, rax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void BinaryOpWithAllocationSiteDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rcx, rdx, rax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void StringAddDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {rsi, rdx, rax};
  data->Initialize(arraysize(registers), registers, NULL);
}


void KeyedDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rsi,  // context
      rcx,  // key
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // key
  };
  data->Initialize(arraysize(registers), registers, representations);
}


void NamedDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rsi,  // context
      rcx,  // name
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // name
  };
  data->Initialize(arraysize(registers), registers, representations);
}


void CallHandlerDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rsi,  // context
      rdx,  // receiver
  };
  Representation representations[] = {
      Representation::Tagged(),  // context
      Representation::Tagged(),  // receiver
  };
  data->Initialize(arraysize(registers), registers, representations);
}


void ArgumentAdaptorDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rsi,  // context
      rdi,  // JSFunction
      rax,  // actual number of arguments
      rbx,  // expected number of arguments
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
      rsi,  // context
      rax,  // callee
      rbx,  // call_data
      rcx,  // holder
      rdx,  // api_function_address
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

#endif  // V8_TARGET_ARCH_X64

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return rsi; }


const Register LoadDescriptor::ReceiverRegister() { return rdx; }
const Register LoadDescriptor::NameRegister() { return rcx; }
const Register LoadDescriptor::SlotRegister() { return rax; }


const Register LoadWithVectorDescriptor::VectorRegister() { return rbx; }


const Register StoreDescriptor::ReceiverRegister() { return rdx; }
const Register StoreDescriptor::NameRegister() { return rcx; }
const Register StoreDescriptor::ValueRegister() { return rax; }


const Register VectorStoreICTrampolineDescriptor::SlotRegister() { return rdi; }


const Register VectorStoreICDescriptor::VectorRegister() { return rbx; }


const Register VectorStoreTransitionDescriptor::SlotRegister() { return rdi; }
const Register VectorStoreTransitionDescriptor::VectorRegister() { return rbx; }
const Register VectorStoreTransitionDescriptor::MapRegister() { return r11; }


const Register StoreTransitionDescriptor::MapRegister() { return rbx; }


const Register LoadGlobalViaContextDescriptor::SlotRegister() { return rbx; }


const Register StoreGlobalViaContextDescriptor::SlotRegister() { return rbx; }
const Register StoreGlobalViaContextDescriptor::ValueRegister() { return rax; }


const Register InstanceOfDescriptor::LeftRegister() { return rdx; }
const Register InstanceOfDescriptor::RightRegister() { return rax; }


const Register StringCompareDescriptor::LeftRegister() { return rdx; }
const Register StringCompareDescriptor::RightRegister() { return rax; }


const Register ArgumentsAccessReadDescriptor::index() { return rdx; }
const Register ArgumentsAccessReadDescriptor::parameter_count() { return rax; }


const Register ArgumentsAccessNewDescriptor::function() { return rdi; }
const Register ArgumentsAccessNewDescriptor::parameter_count() { return rcx; }
const Register ArgumentsAccessNewDescriptor::parameter_pointer() { return rdx; }


const Register RestParamAccessDescriptor::parameter_count() { return rcx; }
const Register RestParamAccessDescriptor::parameter_pointer() { return rdx; }
const Register RestParamAccessDescriptor::rest_parameter_index() { return rbx; }


const Register ApiGetterDescriptor::function_address() { return r8; }


const Register MathPowTaggedDescriptor::exponent() { return rdx; }


const Register MathPowIntegerDescriptor::exponent() {
  return MathPowTaggedDescriptor::exponent();
}


const Register GrowArrayElementsDescriptor::ObjectRegister() { return rax; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return rbx; }


void FastNewClosureDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastNewContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rdi};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ToNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // ToNumberStub invokes a function, and therefore needs a context.
  Register registers[] = {rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


// static
const Register ToLengthDescriptor::ReceiverRegister() { return rax; }


// static
const Register ToStringDescriptor::ReceiverRegister() { return rax; }


// static
const Register ToObjectDescriptor::ReceiverRegister() { return rax; }


void NumberToStringDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneRegExpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rdi, rax, rcx, rdx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneShallowArrayDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rax, rbx, rcx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneShallowObjectDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rax, rbx, rcx, rdx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CreateAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rbx, rdx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CreateWeakCellDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rbx, rdx, rdi};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void StoreArrayLiteralElementDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rcx, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rdi};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionWithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rdi, rdx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionWithFeedbackAndVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rdi, rdx, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments
  // rbx : feedback vector
  // rdx : slot in feedback vector (Smi, for RecordCallTarget)
  // rdi : constructor function
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {rax, rdi, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments
  // rdi : the target to call
  Register registers[] = {rdi, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ConstructStubDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments
  // rdx : the new target
  // rdi : the target to call
  // rbx : allocation site or undefined
  Register registers[] = {rdi, rdx, rax, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ConstructTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments
  // rdx : the new target
  // rdi : the target to call
  Register registers[] = {rdi, rdx, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void RegExpConstructResultDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rcx, rbx, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void TransitionElementsKindDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rax, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr, nullptr);
}


void AllocateInNewSpaceDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ArrayConstructorConstantArgCountDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // rax -- number of arguments
  // rdi -- function
  // rbx -- allocation site with elements kind
  Register registers[] = {rdi, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {rdi, rbx, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InternalArrayConstructorConstantArgCountDescriptor::
    InitializePlatformSpecific(CallInterfaceDescriptorData* data) {
  // register state
  // rax -- number of arguments
  // rdi -- constructor function
  Register registers[] = {rdi};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InternalArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {rdi, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rdx, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CompareNilDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ToBooleanDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rdx, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void BinaryOpWithAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rcx, rdx, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void StringAddDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rdx, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void KeyedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rcx,  // key
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void NamedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rcx,  // name
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallHandlerDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rdx,  // receiver
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ArgumentAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rdi,  // JSFunction
      rdx,  // the new target
      rax,  // actual number of arguments
      rbx,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ApiFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rdi,  // callee
      rbx,  // call_data
      rcx,  // holder
      rdx,  // api_function_address
      rax,  // actual number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ApiAccessorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rdi,  // callee
      rbx,  // call_data
      rcx,  // holder
      rdx,  // api_function_address
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InterpreterPushArgsAndCallDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rax,  // argument count (not including receiver)
      rbx,  // address of first argument
      rdi   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InterpreterPushArgsAndConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rax,  // argument count (not including receiver)
      rdx,  // new target
      rdi,  // constructor
      rbx,  // address of first argument
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InterpreterCEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rax,  // argument count (argc)
      r15,  // address of first argument (argv)
      rbx   // the runtime function to call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64

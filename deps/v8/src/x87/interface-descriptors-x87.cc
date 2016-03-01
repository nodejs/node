// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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


const Register VectorStoreTransitionDescriptor::SlotRegister() {
  return no_reg;
}


const Register VectorStoreTransitionDescriptor::VectorRegister() { return ebx; }


const Register VectorStoreTransitionDescriptor::MapRegister() { return edi; }


const Register StoreTransitionDescriptor::MapRegister() { return ebx; }


const Register LoadGlobalViaContextDescriptor::SlotRegister() { return ebx; }


const Register StoreGlobalViaContextDescriptor::SlotRegister() { return ebx; }
const Register StoreGlobalViaContextDescriptor::ValueRegister() { return eax; }


const Register InstanceOfDescriptor::LeftRegister() { return edx; }
const Register InstanceOfDescriptor::RightRegister() { return eax; }


const Register StringCompareDescriptor::LeftRegister() { return edx; }
const Register StringCompareDescriptor::RightRegister() { return eax; }


const Register ArgumentsAccessReadDescriptor::index() { return edx; }
const Register ArgumentsAccessReadDescriptor::parameter_count() { return eax; }


const Register ArgumentsAccessNewDescriptor::function() { return edi; }
const Register ArgumentsAccessNewDescriptor::parameter_count() { return ecx; }
const Register ArgumentsAccessNewDescriptor::parameter_pointer() { return edx; }


const Register RestParamAccessDescriptor::parameter_count() { return ecx; }
const Register RestParamAccessDescriptor::parameter_pointer() { return edx; }
const Register RestParamAccessDescriptor::rest_parameter_index() { return ebx; }


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


// static
const Register ToLengthDescriptor::ReceiverRegister() { return eax; }


// static
const Register ToStringDescriptor::ReceiverRegister() { return eax; }


// static
const Register ToObjectDescriptor::ReceiverRegister() { return eax; }


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


void FastCloneRegExpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edi, eax, ecx, edx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
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
  // ecx : new target (for IsSuperConstructorCall)
  // edx : slot in feedback vector (Smi, for RecordCallTarget)
  // edi : constructor function
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {eax, edi, ecx, ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // edi : the target to call
  Register registers[] = {edi, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ConstructStubDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // edx : the new target
  // edi : the target to call
  // ebx : allocation site or undefined
  Register registers[] = {edi, edx, eax, ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ConstructTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // edx : the new target
  // edi : the target to call
  Register registers[] = {edi, edx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
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


void AllocateInNewSpaceDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {eax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
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
      edx,  // the new target
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


void InterpreterPushArgsAndCallDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      eax,  // argument count (not including receiver)
      ebx,  // address of first argument
      edi   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InterpreterPushArgsAndConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      eax,  // argument count (not including receiver)
      edx,  // new target
      edi,  // constructor
      ebx,  // address of first argument
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InterpreterCEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      eax,  // argument count (argc)
      ecx,  // address of first argument (argv)
      ebx   // the runtime function to call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X87

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return cp; }


const Register LoadDescriptor::ReceiverRegister() { return a1; }
const Register LoadDescriptor::NameRegister() { return a2; }
const Register LoadDescriptor::SlotRegister() { return a0; }


const Register LoadWithVectorDescriptor::VectorRegister() { return a3; }


const Register StoreDescriptor::ReceiverRegister() { return a1; }
const Register StoreDescriptor::NameRegister() { return a2; }
const Register StoreDescriptor::ValueRegister() { return a0; }


const Register VectorStoreICTrampolineDescriptor::SlotRegister() { return t0; }


const Register VectorStoreICDescriptor::VectorRegister() { return a3; }


const Register VectorStoreTransitionDescriptor::SlotRegister() { return t0; }
const Register VectorStoreTransitionDescriptor::VectorRegister() { return a3; }
const Register VectorStoreTransitionDescriptor::MapRegister() { return t1; }


const Register StoreTransitionDescriptor::MapRegister() { return a3; }


const Register LoadGlobalViaContextDescriptor::SlotRegister() { return a2; }


const Register StoreGlobalViaContextDescriptor::SlotRegister() { return a2; }
const Register StoreGlobalViaContextDescriptor::ValueRegister() { return a0; }


const Register InstanceOfDescriptor::LeftRegister() { return a1; }
const Register InstanceOfDescriptor::RightRegister() { return a0; }


const Register StringCompareDescriptor::LeftRegister() { return a1; }
const Register StringCompareDescriptor::RightRegister() { return a0; }


const Register ApiGetterDescriptor::function_address() { return a2; }


const Register MathPowTaggedDescriptor::exponent() { return a2; }


const Register MathPowIntegerDescriptor::exponent() {
  return MathPowTaggedDescriptor::exponent();
}


const Register GrowArrayElementsDescriptor::ObjectRegister() { return a0; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return a3; }


void FastNewClosureDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a2};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void FastNewContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}

void FastNewObjectDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1, a3};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}

void FastNewRestParameterDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void FastNewSloppyArgumentsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void FastNewStrictArgumentsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void ToNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


// static
const Register ToLengthDescriptor::ReceiverRegister() { return a0; }


// static
const Register ToStringDescriptor::ReceiverRegister() { return a0; }


// static
const Register ToNameDescriptor::ReceiverRegister() { return a0; }


// static
const Register ToObjectDescriptor::ReceiverRegister() { return a0; }


void NumberToStringDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a3};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void FastCloneRegExpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a3, a2, a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneShallowArrayDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a3, a2, a1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneShallowObjectDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a3, a2, a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void CreateAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a2, a3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CreateWeakCellDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a2, a3, a1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void CallFunctionWithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1, a3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionWithFeedbackAndVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1, a3, a2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a0 : number of arguments
  // a1 : the function to call
  // a2 : feedback vector
  // a3 : slot in feedback vector (Smi, for RecordCallTarget)
  // t0 : new target (for IsSuperConstructorCall)
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {a0, a1, t0, a2};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a1: target
  // a0: number of arguments
  Register registers[] = {a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ConstructStubDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a1: target
  // a3: new target
  // a0: number of arguments
  // a2: allocation site or undefined
  Register registers[] = {a1, a3, a0, a2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ConstructTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a1: target
  // a3: new target
  // a0: number of arguments
  Register registers[] = {a1, a3, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void RegExpConstructResultDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a2, a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void TransitionElementsKindDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a0, a1};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  data->InitializePlatformSpecific(0, nullptr, nullptr);
}


void AllocateInNewSpaceDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ArrayConstructorConstantArgCountDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // a0 -- number of arguments
  // a1 -- function
  // a2 -- allocation site with elements kind
  Register registers[] = {a1, a2};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void ArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {a1, a2, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InternalArrayConstructorConstantArgCountDescriptor::
    InitializePlatformSpecific(CallInterfaceDescriptorData* data) {
  // register state
  // a0 -- number of arguments
  // a1 -- constructor function
  Register registers[] = {a1};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void InternalArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void CompareNilDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void ToBooleanDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void BinaryOpWithAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a2, a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void StringAddDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}


void KeyedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      a2,  // key
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void NamedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      a2,  // name
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallHandlerDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      a0,  // receiver
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ArgumentAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      a1,  // JSFunction
      a3,  // the new target
      a0,  // actual number of arguments
      a2,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ApiFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      a0,  // callee
      t0,  // call_data
      a2,  // holder
      a1,  // api_function_address
      a3,  // actual number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ApiAccessorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      a0,  // callee
      t0,  // call_data
      a2,  // holder
      a1,  // api_function_address
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterDispatchDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      kInterpreterAccumulatorRegister, kInterpreterRegisterFileRegister,
      kInterpreterBytecodeOffsetRegister, kInterpreterBytecodeArrayRegister,
      kInterpreterDispatchTableRegister};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsAndCallDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      a0,  // argument count (not including receiver)
      a2,  // address of first argument
      a1   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsAndConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      a0,  // argument count (not including receiver)
      a3,  // new target
      a1,  // constructor to call
      a2   // address of the first argument
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterCEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      a0,  // argument count (argc)
      a2,  // address of first argument (argv)
      a1   // the runtime function to call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS

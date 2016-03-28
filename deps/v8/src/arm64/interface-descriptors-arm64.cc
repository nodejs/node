// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arm64/interface-descriptors-arm64.h"

#if V8_TARGET_ARCH_ARM64

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return cp; }


const Register LoadDescriptor::ReceiverRegister() { return x1; }
const Register LoadDescriptor::NameRegister() { return x2; }
const Register LoadDescriptor::SlotRegister() { return x0; }


const Register LoadWithVectorDescriptor::VectorRegister() { return x3; }


const Register StoreDescriptor::ReceiverRegister() { return x1; }
const Register StoreDescriptor::NameRegister() { return x2; }
const Register StoreDescriptor::ValueRegister() { return x0; }


const Register VectorStoreICTrampolineDescriptor::SlotRegister() { return x4; }


const Register VectorStoreICDescriptor::VectorRegister() { return x3; }


const Register VectorStoreTransitionDescriptor::SlotRegister() { return x4; }
const Register VectorStoreTransitionDescriptor::VectorRegister() { return x3; }
const Register VectorStoreTransitionDescriptor::MapRegister() { return x5; }


const Register StoreTransitionDescriptor::MapRegister() { return x3; }


const Register LoadGlobalViaContextDescriptor::SlotRegister() { return x2; }


const Register StoreGlobalViaContextDescriptor::SlotRegister() { return x2; }
const Register StoreGlobalViaContextDescriptor::ValueRegister() { return x0; }


const Register InstanceOfDescriptor::LeftRegister() { return x1; }
const Register InstanceOfDescriptor::RightRegister() { return x0; }


const Register StringCompareDescriptor::LeftRegister() { return x1; }
const Register StringCompareDescriptor::RightRegister() { return x0; }


const Register ApiGetterDescriptor::function_address() { return x2; }


const Register MathPowTaggedDescriptor::exponent() { return x11; }


const Register MathPowIntegerDescriptor::exponent() { return x12; }


const Register GrowArrayElementsDescriptor::ObjectRegister() { return x0; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return x3; }


void FastNewClosureDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x2: function info
  Register registers[] = {x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastNewContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: function
  Register registers[] = {x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FastNewObjectDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {x1, x3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FastNewRestParameterDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: function
  Register registers[] = {x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastNewSloppyArgumentsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: function
  Register registers[] = {x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastNewStrictArgumentsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: function
  Register registers[] = {x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ToNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x0: value
  Register registers[] = {x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


// static
const Register ToLengthDescriptor::ReceiverRegister() { return x0; }


// static
const Register ToStringDescriptor::ReceiverRegister() { return x0; }


// static
const Register ToNameDescriptor::ReceiverRegister() { return x0; }


// static
const Register ToObjectDescriptor::ReceiverRegister() { return x0; }


void NumberToStringDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x0: value
  Register registers[] = {x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {x3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneRegExpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x3: closure
  // x2: object literal index
  // x1: constant properties
  // x0: object literal flags
  Register registers[] = {x3, x2, x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneShallowArrayDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x3: closure
  // x2: array literal index
  // x1: constant elements
  Register registers[] = {x3, x2, x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void FastCloneShallowObjectDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x3: closure
  // x2: object literal index
  // x1: constant properties
  // x0: object literal flags
  Register registers[] = {x3, x2, x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CreateAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x2: feedback vector
  // x3: call feedback slot
  Register registers[] = {x2, x3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CreateWeakCellDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x2: feedback vector
  // x3: call feedback slot
  // x1: tagged value to put in the weak cell
  Register registers[] = {x2, x3, x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1  function    the function to call
  Register registers[] = {x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionWithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {x1, x3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallFunctionWithFeedbackAndVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {x1, x3, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x0 : number of arguments
  // x1 : the function to call
  // x2 : feedback vector
  // x3 : slot in feedback vector (Smi, for RecordCallTarget)
  // x4 : new target (for IsSuperConstructorCall)
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {x0, x1, x4, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: target
  // x0: number of arguments
  Register registers[] = {x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ConstructStubDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x3: new target
  // x1: target
  // x0: number of arguments
  // x2: allocation site or undefined
  Register registers[] = {x1, x3, x0, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ConstructTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x3: new target
  // x1: target
  // x0: number of arguments
  Register registers[] = {x1, x3, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void RegExpConstructResultDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x2: length
  // x1: index (of last match)
  // x0: string
  Register registers[] = {x2, x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void TransitionElementsKindDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x0: value (js_array)
  // x1: to_map
  Register registers[] = {x0, x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr, nullptr);
}


void AllocateInNewSpaceDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ArrayConstructorConstantArgCountDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: function
  // x2: allocation site with elements kind
  // x0: number of arguments to the constructor function
  Register registers[] = {x1, x2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {x1, x2, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InternalArrayConstructorConstantArgCountDescriptor::
    InitializePlatformSpecific(CallInterfaceDescriptorData* data) {
  // x1: constructor function
  // x0: number of arguments to the constructor function
  Register registers[] = {x1};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InternalArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: left operand
  // x0: right operand
  Register registers[] = {x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void CompareNilDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x0: value to compare
  Register registers[] = {x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ToBooleanDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x0: value
  Register registers[] = {x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: left operand
  // x0: right operand
  Register registers[] = {x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void BinaryOpWithAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x2: allocation site
  // x1: left operand
  // x0: right operand
  Register registers[] = {x2, x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void StringAddDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // x1: left operand
  // x0: right operand
  Register registers[] = {x1, x0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void KeyedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor noInlineDescriptor =
      PlatformInterfaceDescriptor(NEVER_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      x2,  // key
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &noInlineDescriptor);
}


void NamedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor noInlineDescriptor =
      PlatformInterfaceDescriptor(NEVER_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      x2,  // name
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &noInlineDescriptor);
}


void CallHandlerDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      x0,  // receiver
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &default_descriptor);
}


void ArgumentAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      x1,  // JSFunction
      x3,  // the new target
      x0,  // actual number of arguments
      x2,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &default_descriptor);
}


void ApiFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      x0,  // callee
      x4,  // call_data
      x2,  // holder
      x1,  // api_function_address
      x3,  // actual number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &default_descriptor);
}


void ApiAccessorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static PlatformInterfaceDescriptor default_descriptor =
      PlatformInterfaceDescriptor(CAN_INLINE_TARGET_ADDRESS);

  Register registers[] = {
      x0,  // callee
      x4,  // call_data
      x2,  // holder
      x1,  // api_function_address
  };
  data->InitializePlatformSpecific(arraysize(registers), registers,
                                   &default_descriptor);
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
      x0,  // argument count (not including receiver)
      x2,  // address of first argument
      x1   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsAndConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      x0,  // argument count (not including receiver)
      x3,  // new target
      x1,  // constructor to call
      x2   // address of the first argument
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterCEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      x0,   // argument count (argc)
      x11,  // address of first argument (argv)
      x1    // the runtime function to call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64

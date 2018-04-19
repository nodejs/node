// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return cp; }

void CallInterfaceDescriptor::DefaultInitializePlatformSpecific(
    CallInterfaceDescriptorData* data, int register_parameter_count) {
  const Register default_stub_registers[] = {r3, r4, r5, r6, r7};
  CHECK_LE(static_cast<size_t>(register_parameter_count),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(register_parameter_count,
                                   default_stub_registers);
}

void RecordWriteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  const Register default_stub_registers[] = {r3, r4, r5, r6, r7};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

const Register FastNewFunctionContextDescriptor::FunctionRegister() {
  return r4;
}
const Register FastNewFunctionContextDescriptor::SlotsRegister() { return r3; }

const Register LoadDescriptor::ReceiverRegister() { return r4; }
const Register LoadDescriptor::NameRegister() { return r5; }
const Register LoadDescriptor::SlotRegister() { return r3; }

const Register LoadWithVectorDescriptor::VectorRegister() { return r6; }

const Register StoreDescriptor::ReceiverRegister() { return r4; }
const Register StoreDescriptor::NameRegister() { return r5; }
const Register StoreDescriptor::ValueRegister() { return r3; }
const Register StoreDescriptor::SlotRegister() { return r7; }

const Register StoreWithVectorDescriptor::VectorRegister() { return r6; }

const Register StoreTransitionDescriptor::SlotRegister() { return r7; }
const Register StoreTransitionDescriptor::VectorRegister() { return r6; }
const Register StoreTransitionDescriptor::MapRegister() { return r8; }

const Register ApiGetterDescriptor::HolderRegister() { return r3; }
const Register ApiGetterDescriptor::CallbackRegister() { return r6; }

const Register MathPowTaggedDescriptor::exponent() { return r5; }

const Register MathPowIntegerDescriptor::exponent() {
  return MathPowTaggedDescriptor::exponent();
}


const Register GrowArrayElementsDescriptor::ObjectRegister() { return r3; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return r6; }


// static
const Register TypeConversionDescriptor::ArgumentRegister() { return r3; }

void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r6};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : number of arguments
  // r4 : the target to call
  Register registers[] = {r4, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : number of arguments (on the stack, not including receiver)
  // r4 : the target to call
  // r5 : arguments list (FixedArray)
  // r7 : arguments list length (untagged)
  Register registers[] = {r4, r3, r5, r7};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : number of arguments
  // r5 : start index (to support rest parameters)
  // r4 : the target to call
  Register registers[] = {r4, r3, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : number of arguments (on the stack, not including receiver)
  // r4 : the target to call
  // r5 : the object to spread
  Register registers[] = {r4, r3, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r4 : the target to call
  // r5 : the arguments list
  Register registers[] = {r4, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : number of arguments (on the stack, not including receiver)
  // r4 : the target to call
  // r6 : the new target
  // r5 : arguments list (FixedArray)
  // r7 : arguments list length (untagged)
  Register registers[] = {r4, r6, r3, r5, r7};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : number of arguments
  // r6 : the new target
  // r5 : start index (to support rest parameters)
  // r4 : the target to call
  Register registers[] = {r4, r6, r3, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : number of arguments (on the stack, not including receiver)
  // r4 : the target to call
  // r6 : the new target
  // r5 : the object to spread
  Register registers[] = {r4, r6, r3, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r4 : the target to call
  // r6 : the new target
  // r5 : the arguments list
  Register registers[] = {r4, r6, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructStubDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : number of arguments
  // r4 : the target to call
  // r6 : the new target
  // r5 : allocation site or undefined
  Register registers[] = {r4, r6, r3, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ConstructTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r3 : number of arguments
  // r4 : the target to call
  // r6 : the new target
  Register registers[] = {r4, r6, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void TransitionElementsKindDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AbortJSDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr, nullptr);
}

void ArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kActualArgumentsCount, kAllocationSite
  Register registers[] = {r4, r6, r3, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void ArrayNoArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // r3 -- number of arguments
  // r4 -- function
  // r5 -- allocation site with elements kind
  Register registers[] = {r4, r5, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ArraySingleArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // r3 -- number of arguments
  // r4 -- function
  // r5 -- allocation site with elements kind
  Register registers[] = {r4, r5, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}


void ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {r4, r5, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r4, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r4, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void StringAddDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r4, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ArgumentAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r4,  // JSFunction
      r6,  // the new target
      r3,  // actual number of arguments
      r5,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ApiCallbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      JavaScriptFrame::context_register(),  // callee context
      r7,                                   // call_data
      r5,                                   // holder
      r4,                                   // api_function_address
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterDispatchDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      kInterpreterAccumulatorRegister, kInterpreterBytecodeOffsetRegister,
      kInterpreterBytecodeArrayRegister, kInterpreterDispatchTableRegister};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsThenCallDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r3,  // argument count (not including receiver)
      r5,  // address of first argument
      r4   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsThenConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r3,  // argument count (not including receiver)
      r6,  // new target
      r4,  // constructor to call
      r5,  // allocation site feedback if available, undefined otherwise
      r7   // address of the first argument
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterCEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r3,  // argument count (argc)
      r5,  // address of first argument (argv)
      r4   // the runtime function to call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ResumeGeneratorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r3,  // the value to pass to the generator
      r4   // the JSGeneratorObject to resume
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FrameDropperTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r4,  // loaded new FP
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC

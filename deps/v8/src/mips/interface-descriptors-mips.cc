// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return cp; }

void CallInterfaceDescriptor::DefaultInitializePlatformSpecific(
    CallInterfaceDescriptorData* data, int register_parameter_count) {
  const Register default_stub_registers[] = {a0, a1, a2, a3, t0};
  CHECK_LE(static_cast<size_t>(register_parameter_count),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(register_parameter_count,
                                   default_stub_registers);
}

void RecordWriteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  const Register default_stub_registers[] = {a0, a1, a2, a3, kReturnRegister0};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

const Register FastNewFunctionContextDescriptor::FunctionRegister() {
  return a1;
}
const Register FastNewFunctionContextDescriptor::SlotsRegister() { return a0; }

const Register LoadDescriptor::ReceiverRegister() { return a1; }
const Register LoadDescriptor::NameRegister() { return a2; }
const Register LoadDescriptor::SlotRegister() { return a0; }

const Register LoadWithVectorDescriptor::VectorRegister() { return a3; }

const Register StoreDescriptor::ReceiverRegister() { return a1; }
const Register StoreDescriptor::NameRegister() { return a2; }
const Register StoreDescriptor::ValueRegister() { return a0; }
const Register StoreDescriptor::SlotRegister() { return t0; }

const Register StoreWithVectorDescriptor::VectorRegister() { return a3; }

const Register StoreTransitionDescriptor::SlotRegister() { return t0; }
const Register StoreTransitionDescriptor::VectorRegister() { return a3; }
const Register StoreTransitionDescriptor::MapRegister() { return t1; }

const Register ApiGetterDescriptor::HolderRegister() { return a0; }
const Register ApiGetterDescriptor::CallbackRegister() { return a3; }

const Register MathPowTaggedDescriptor::exponent() { return a2; }

const Register MathPowIntegerDescriptor::exponent() {
  return MathPowTaggedDescriptor::exponent();
}


const Register GrowArrayElementsDescriptor::ObjectRegister() { return a0; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return a3; }


void FastNewClosureDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1, a2, a3};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

// static
const Register TypeConversionDescriptor::ArgumentRegister() { return a0; }

void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a3};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void CallFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a1: target
  // a0: number of arguments
  Register registers[] = {a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a0 : number of arguments (on the stack, not including receiver)
  // a1 : the target to call
  // a2 : arguments list (FixedArray)
  // t0 : arguments list length (untagged)
  Register registers[] = {a1, a0, a2, t0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a1: the target to call
  // a0: number of arguments
  // a2: start index (to support rest parameters)
  Register registers[] = {a1, a0, a2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a0 : number of arguments (on the stack, not including receiver)
  // a1 : the target to call
  // a2 : the object to spread
  Register registers[] = {a1, a0, a2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a1 : the target to call
  // a2 : the arguments list
  Register registers[] = {a1, a2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a0 : number of arguments (on the stack, not including receiver)
  // a1 : the target to call
  // a3 : the new target
  // a2 : arguments list (FixedArray)
  // t0 : arguments list length (untagged)
  Register registers[] = {a1, a3, a0, a2, t0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a1: the target to call
  // a3: new target
  // a0: number of arguments
  // a2: start index (to support rest parameters)
  Register registers[] = {a1, a3, a0, a2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a0 : number of arguments (on the stack, not including receiver)
  // a1 : the target to call
  // a3 : the new target
  // a2 : the object to spread
  Register registers[] = {a1, a3, a0, a2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // a1 : the target to call
  // a3 : the new target
  // a2 : the arguments list
  Register registers[] = {a1, a3, a2};
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


void TransitionElementsKindDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a0, a1};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void AbortJSDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  data->InitializePlatformSpecific(0, nullptr, nullptr);
}

void ArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kActualArgumentsCount, kAllocationSite
  Register registers[] = {a1, a3, a0, a2};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void ArrayNoArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // a0 -- number of arguments
  // a1 -- function
  // a2 -- allocation site with elements kind
  Register registers[] = {a1, a2, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void ArraySingleArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // a0 -- number of arguments
  // a1 -- function
  // a2 -- allocation site with elements kind
  Register registers[] = {a1, a2, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {a1, a2, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}


void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void StringAddDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {a1, a0};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
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

void ApiCallbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      JavaScriptFrame::context_register(),  // callee context
      t0,                                   // call_data
      a2,                                   // holder
      a1,                                   // api_function_address
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
      a0,  // argument count (not including receiver)
      a2,  // address of first argument
      a1   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsThenConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      a0,  // argument count (not including receiver)
      a3,  // new target
      a1,  // constructor to call
      a2,  // allocation site feedback if available, undefined otherwise.
      t4   // address of the first argument
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

void ResumeGeneratorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      v0,  // the value to pass to the generator
      a1   // the JSGeneratorObject to resume
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FrameDropperTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      a1,  // loaded new FP
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return esi; }

void CallInterfaceDescriptor::DefaultInitializePlatformSpecific(
    CallInterfaceDescriptorData* data, int register_parameter_count) {
  const Register default_stub_registers[] = {eax, ebx, ecx, edx, edi};
  CHECK_LE(static_cast<size_t>(register_parameter_count),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(register_parameter_count,
                                   default_stub_registers);
}

void RecordWriteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  static const Register default_stub_registers[] = {ebx, ecx, edx, edi,
                                                    kReturnRegister0};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

const Register FastNewFunctionContextDescriptor::FunctionRegister() {
  return edi;
}
const Register FastNewFunctionContextDescriptor::SlotsRegister() { return eax; }

const Register LoadDescriptor::ReceiverRegister() { return edx; }
const Register LoadDescriptor::NameRegister() { return ecx; }
const Register LoadDescriptor::SlotRegister() { return eax; }

const Register LoadWithVectorDescriptor::VectorRegister() { return ebx; }

const Register LoadICProtoArrayDescriptor::HandlerRegister() { return edi; }

const Register StoreDescriptor::ReceiverRegister() { return edx; }
const Register StoreDescriptor::NameRegister() { return ecx; }
const Register StoreDescriptor::ValueRegister() { return eax; }
const Register StoreDescriptor::SlotRegister() { return edi; }

const Register StoreWithVectorDescriptor::VectorRegister() { return ebx; }

const Register StoreTransitionDescriptor::SlotRegister() { return no_reg; }
const Register StoreTransitionDescriptor::VectorRegister() { return ebx; }
const Register StoreTransitionDescriptor::MapRegister() { return edi; }

const Register ApiGetterDescriptor::HolderRegister() { return ecx; }
const Register ApiGetterDescriptor::CallbackRegister() { return eax; }

const Register MathPowTaggedDescriptor::exponent() { return eax; }

const Register MathPowIntegerDescriptor::exponent() {
  return MathPowTaggedDescriptor::exponent();
}


const Register GrowArrayElementsDescriptor::ObjectRegister() { return eax; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return ebx; }


void FastNewClosureDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // SharedFunctionInfo, vector, slot index.
  Register registers[] = {ebx, ecx, edx};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

// static
const Register TypeConversionDescriptor::ArgumentRegister() { return eax; }

void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void CallFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edi};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // edi : the target to call
  Register registers[] = {edi, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // ebx : arguments list (FixedArray)
  // ecx : arguments list length (untagged)
  Register registers[] = {edi, eax, ebx, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // ecx : start index (to support rest parameters)
  // edi : the target to call
  Register registers[] = {edi, eax, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // ebx : the object to spread
  Register registers[] = {edi, eax, ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // edi : the target to call
  // ebx : the arguments list
  Register registers[] = {edi, ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // edx : the new target
  // ebx : arguments list (FixedArray)
  // ecx : arguments list length (untagged)
  Register registers[] = {edi, edx, eax, ebx, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments
  // edx : the new target
  // ecx : start index (to support rest parameters)
  // edi : the target to call
  Register registers[] = {edi, edx, eax, ecx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // eax : number of arguments (on the stack, not including receiver)
  // edi : the target to call
  // edx : the new target
  // ebx : the object to spread
  Register registers[] = {edi, edx, eax, ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // edi : the target to call
  // edx : the new target
  // ebx : the arguments list
  Register registers[] = {edi, edx, ebx};
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


void TransitionElementsKindDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {eax, ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}


void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  data->InitializePlatformSpecific(0, nullptr, nullptr);
}

void ArrayConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kActualArgumentsCount, kAllocationSite
  Register registers[] = {edi, edx, eax, ebx};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void ArrayNoArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // eax -- number of arguments
  // edi -- function
  // ebx -- allocation site with elements kind
  Register registers[] = {edi, ebx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void ArraySingleArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // eax -- number of arguments
  // edi -- function
  // ebx -- allocation site with elements kind
  Register registers[] = {edi, ebx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // eax -- number of arguments
  // edi -- function
  // ebx -- allocation site with elements kind
  Register registers[] = {edi, ebx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}


void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
}

void StringAddDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {edx, eax};
  data->InitializePlatformSpecific(arraysize(registers), registers, nullptr);
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

void ApiCallbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      JavaScriptFrame::context_register(),  // callee context
      ebx,                                  // call_data
      ecx,                                  // holder
      edx,                                  // api_function_address
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
      eax,  // argument count (not including receiver)
      ebx,  // address of first argument
      edi   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsThenConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      eax,  // argument count (not including receiver)
      edx,  // new target
      edi,  // constructor
      ebx,  // allocation site feedback
      ecx,  // address of first argument
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

void ResumeGeneratorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      eax,  // the value to pass to the generator
      edx   // the JSGeneratorObject to resume
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FrameDropperTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      ebx,  // loaded new FP
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32

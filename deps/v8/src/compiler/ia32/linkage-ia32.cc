// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler.h"
#include "src/code-stubs.h"
#include "src/compiler/linkage.h"
#include "src/compiler/linkage-impl.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

struct IA32LinkageHelperTraits {
  static Register ReturnValueReg() { return eax; }
  static Register ReturnValue2Reg() { return edx; }
  static Register JSCallFunctionReg() { return edi; }
  static Register ContextReg() { return esi; }
  static Register RuntimeCallFunctionReg() { return ebx; }
  static Register RuntimeCallArgCountReg() { return eax; }
  static RegList CCalleeSaveRegisters() {
    return esi.bit() | edi.bit() | ebx.bit();
  }
  static RegList CCalleeSaveFPRegisters() { return 0; }
  static Register CRegisterParameter(int i) { return no_reg; }
  static int CRegisterParametersLength() { return 0; }
  static int CStackBackingStoreLength() { return 0; }
};

typedef LinkageHelper<IA32LinkageHelperTraits> LH;

CallDescriptor* Linkage::GetJSCallDescriptor(Zone* zone, bool is_osr,
                                             int parameter_count,
                                             CallDescriptor::Flags flags) {
  return LH::GetJSCallDescriptor(zone, is_osr, parameter_count, flags);
}


CallDescriptor* Linkage::GetRuntimeCallDescriptor(
    Zone* zone, Runtime::FunctionId function, int parameter_count,
    Operator::Properties properties) {
  return LH::GetRuntimeCallDescriptor(zone, function, parameter_count,
                                      properties);
}


CallDescriptor* Linkage::GetStubCallDescriptor(
    Isolate* isolate, Zone* zone, const CallInterfaceDescriptor& descriptor,
    int stack_parameter_count, CallDescriptor::Flags flags,
    Operator::Properties properties, MachineType return_type) {
  return LH::GetStubCallDescriptor(isolate, zone, descriptor,
                                   stack_parameter_count, flags, properties,
                                   return_type);
}


CallDescriptor* Linkage::GetSimplifiedCDescriptor(Zone* zone,
                                                  const MachineSignature* sig) {
  return LH::GetSimplifiedCDescriptor(zone, sig);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

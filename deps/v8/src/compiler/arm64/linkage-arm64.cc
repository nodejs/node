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

struct Arm64LinkageHelperTraits {
  static Register ReturnValueReg() { return x0; }
  static Register ReturnValue2Reg() { return x1; }
  static Register JSCallFunctionReg() { return x1; }
  static Register ContextReg() { return cp; }
  static Register RuntimeCallFunctionReg() { return x1; }
  static Register RuntimeCallArgCountReg() { return x0; }
  static RegList CCalleeSaveRegisters() {
    return (1 << x19.code()) | (1 << x20.code()) | (1 << x21.code()) |
           (1 << x22.code()) | (1 << x23.code()) | (1 << x24.code()) |
           (1 << x25.code()) | (1 << x26.code()) | (1 << x27.code()) |
           (1 << x28.code()) | (1 << x29.code()) | (1 << x30.code());
  }
  static RegList CCalleeSaveFPRegisters() {
    return (1 << d8.code()) | (1 << d9.code()) | (1 << d10.code()) |
           (1 << d11.code()) | (1 << d12.code()) | (1 << d13.code()) |
           (1 << d14.code()) | (1 << d15.code());
  }
  static Register CRegisterParameter(int i) {
    static Register register_parameters[] = {x0, x1, x2, x3, x4, x5, x6, x7};
    return register_parameters[i];
  }
  static int CRegisterParametersLength() { return 8; }
  static int CStackBackingStoreLength() { return 0; }
};


typedef LinkageHelper<Arm64LinkageHelperTraits> LH;

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

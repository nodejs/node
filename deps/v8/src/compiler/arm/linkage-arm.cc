// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/assembler.h"
#include "src/code-stubs.h"
#include "src/compiler/linkage.h"
#include "src/compiler/linkage-impl.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

struct LinkageHelperTraits {
  static Register ReturnValueReg() { return r0; }
  static Register ReturnValue2Reg() { return r1; }
  static Register JSCallFunctionReg() { return r1; }
  static Register ContextReg() { return cp; }
  static Register RuntimeCallFunctionReg() { return r1; }
  static Register RuntimeCallArgCountReg() { return r0; }
  static RegList CCalleeSaveRegisters() {
    return r4.bit() | r5.bit() | r6.bit() | r7.bit() | r8.bit() | r9.bit() |
           r10.bit();
  }
  static Register CRegisterParameter(int i) {
    static Register register_parameters[] = {r0, r1, r2, r3};
    return register_parameters[i];
  }
  static int CRegisterParametersLength() { return 4; }
};


CallDescriptor* Linkage::GetJSCallDescriptor(int parameter_count, Zone* zone) {
  return LinkageHelper::GetJSCallDescriptor<LinkageHelperTraits>(
      zone, parameter_count);
}


CallDescriptor* Linkage::GetRuntimeCallDescriptor(
    Runtime::FunctionId function, int parameter_count,
    Operator::Property properties,
    CallDescriptor::DeoptimizationSupport can_deoptimize, Zone* zone) {
  return LinkageHelper::GetRuntimeCallDescriptor<LinkageHelperTraits>(
      zone, function, parameter_count, properties, can_deoptimize);
}


CallDescriptor* Linkage::GetStubCallDescriptor(
    CodeStubInterfaceDescriptor* descriptor, int stack_parameter_count,
    CallDescriptor::DeoptimizationSupport can_deoptimize, Zone* zone) {
  return LinkageHelper::GetStubCallDescriptor<LinkageHelperTraits>(
      zone, descriptor, stack_parameter_count, can_deoptimize);
}


CallDescriptor* Linkage::GetSimplifiedCDescriptor(
    Zone* zone, int num_params, MachineType return_type,
    const MachineType* param_types) {
  return LinkageHelper::GetSimplifiedCDescriptor<LinkageHelperTraits>(
      zone, num_params, return_type, param_types);
}
}
}
}  // namespace v8::internal::compiler

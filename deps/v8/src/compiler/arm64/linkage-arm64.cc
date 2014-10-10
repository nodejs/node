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

struct Arm64LinkageHelperTraits {
  static Register ReturnValueReg() { return x0; }
  static Register ReturnValue2Reg() { return x1; }
  static Register JSCallFunctionReg() { return x1; }
  static Register ContextReg() { return cp; }
  static Register RuntimeCallFunctionReg() { return x1; }
  static Register RuntimeCallArgCountReg() { return x0; }
  static RegList CCalleeSaveRegisters() {
    // TODO(dcarney): correct callee saved registers.
    return 0;
  }
  static Register CRegisterParameter(int i) {
    static Register register_parameters[] = {x0, x1, x2, x3, x4, x5, x6, x7};
    return register_parameters[i];
  }
  static int CRegisterParametersLength() { return 8; }
};


typedef LinkageHelper<Arm64LinkageHelperTraits> LH;

CallDescriptor* Linkage::GetJSCallDescriptor(int parameter_count, Zone* zone) {
  return LH::GetJSCallDescriptor(zone, parameter_count);
}


CallDescriptor* Linkage::GetRuntimeCallDescriptor(
    Runtime::FunctionId function, int parameter_count,
    Operator::Properties properties, Zone* zone) {
  return LH::GetRuntimeCallDescriptor(zone, function, parameter_count,
                                      properties);
}


CallDescriptor* Linkage::GetStubCallDescriptor(
    CallInterfaceDescriptor descriptor, int stack_parameter_count,
    CallDescriptor::Flags flags, Zone* zone) {
  return LH::GetStubCallDescriptor(zone, descriptor, stack_parameter_count,
                                   flags);
}


CallDescriptor* Linkage::GetSimplifiedCDescriptor(Zone* zone,
                                                  MachineSignature* sig) {
  return LH::GetSimplifiedCDescriptor(zone, sig);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

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

struct ArmLinkageHelperTraits {
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


typedef LinkageHelper<ArmLinkageHelperTraits> LH;

CallDescriptor* Linkage::GetJSCallDescriptor(int parameter_count, Zone* zone,
                                             CallDescriptor::Flags flags) {
  return LH::GetJSCallDescriptor(zone, parameter_count, flags);
}


CallDescriptor* Linkage::GetRuntimeCallDescriptor(
    Runtime::FunctionId function, int parameter_count,
    Operator::Properties properties, Zone* zone) {
  return LH::GetRuntimeCallDescriptor(zone, function, parameter_count,
                                      properties);
}


CallDescriptor* Linkage::GetStubCallDescriptor(
    const CallInterfaceDescriptor& descriptor, int stack_parameter_count,
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

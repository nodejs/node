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

#ifdef _WIN64
const bool kWin64 = true;
#else
const bool kWin64 = false;
#endif

struct X64LinkageHelperTraits {
  static Register ReturnValueReg() { return rax; }
  static Register ReturnValue2Reg() { return rdx; }
  static Register JSCallFunctionReg() { return rdi; }
  static Register ContextReg() { return rsi; }
  static Register RuntimeCallFunctionReg() { return rbx; }
  static Register RuntimeCallArgCountReg() { return rax; }
  static RegList CCalleeSaveRegisters() {
    if (kWin64) {
      return rbx.bit() | rdi.bit() | rsi.bit() | r12.bit() | r13.bit() |
             r14.bit() | r15.bit();
    } else {
      return rbx.bit() | r12.bit() | r13.bit() | r14.bit() | r15.bit();
    }
  }
  static Register CRegisterParameter(int i) {
    if (kWin64) {
      static Register register_parameters[] = {rcx, rdx, r8, r9};
      return register_parameters[i];
    } else {
      static Register register_parameters[] = {rdi, rsi, rdx, rcx, r8, r9};
      return register_parameters[i];
    }
  }
  static int CRegisterParametersLength() { return kWin64 ? 4 : 6; }
};

typedef LinkageHelper<X64LinkageHelperTraits> LH;

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

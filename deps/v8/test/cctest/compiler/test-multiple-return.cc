// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jochen): Remove this after the setting is turned on globally.
#define V8_IMMINENT_DEPRECATION_WARNINGS

#include <cmath>
#include <functional>
#include <limits>

#include "src/assembler.h"
#include "src/base/bits.h"
#include "src/base/utils/random-number-generator.h"
#include "src/codegen.h"
#include "src/compiler.h"
#include "src/compiler/linkage.h"
#include "src/macro-assembler.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/value-helper.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

CallDescriptor* GetCallDescriptor(Zone* zone, int return_count,
                                  int param_count) {
  MachineSignature::Builder msig(zone, return_count, param_count);
  LocationSignature::Builder locations(zone, return_count, param_count);
  const RegisterConfiguration* config =
      RegisterConfiguration::ArchDefault(RegisterConfiguration::TURBOFAN);

  // Add return location(s).
  DCHECK(return_count <= config->num_allocatable_general_registers());
  for (int i = 0; i < return_count; i++) {
    msig.AddReturn(compiler::kMachInt32);
    locations.AddReturn(
        LinkageLocation::ForRegister(config->allocatable_general_codes()[i]));
  }

  // Add register and/or stack parameter(s).
  DCHECK(param_count <= config->num_allocatable_general_registers());
  for (int i = 0; i < param_count; i++) {
    msig.AddParam(compiler::kMachInt32);
    locations.AddParam(
        LinkageLocation::ForRegister(config->allocatable_general_codes()[i]));
  }

  const RegList kCalleeSaveRegisters = 0;
  const RegList kCalleeSaveFPRegisters = 0;

  // The target for WASM calls is always a code object.
  MachineType target_type = compiler::kMachAnyTagged;
  LinkageLocation target_loc = LinkageLocation::ForAnyRegister();
  return new (zone) CallDescriptor(       // --
      CallDescriptor::kCallCodeObject,    // kind
      target_type,                        // target MachineType
      target_loc,                         // target location
      msig.Build(),                       // machine_sig
      locations.Build(),                  // location_sig
      0,                                  // js_parameter_count
      compiler::Operator::kNoProperties,  // properties
      kCalleeSaveRegisters,               // callee-saved registers
      kCalleeSaveFPRegisters,             // callee-saved fp regs
      CallDescriptor::kNoFlags,           // flags
      "c-call");
}
}  // namespace


TEST(ReturnThreeValues) {
  Zone zone;
  CallDescriptor* desc = GetCallDescriptor(&zone, 3, 2);
  HandleAndZoneScope handles;
  RawMachineAssembler m(handles.main_isolate(),
                        new (handles.main_zone()) Graph(handles.main_zone()),
                        desc, kMachPtr,
                        InstructionSelector::SupportedMachineOperatorFlags());

  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Int32Add(p0, p1);
  Node* sub = m.Int32Sub(p0, p1);
  Node* mul = m.Int32Mul(p0, p1);
  m.Return(add, sub, mul);

  CompilationInfo info("testing", handles.main_isolate(), handles.main_zone());
  Handle<Code> code =
      Pipeline::GenerateCodeForTesting(&info, desc, m.graph(), m.Export());
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_code) {
    OFStream os(stdout);
    code->Disassemble("three_value", os);
  }
#endif

  RawMachineAssemblerTester<int32_t> mt;
  Node* a = mt.Int32Constant(123);
  Node* b = mt.Int32Constant(456);
  Node* ret3 = mt.AddNode(mt.common()->Call(desc), mt.HeapConstant(code), a, b);
  Node* x = mt.AddNode(mt.common()->Projection(0), ret3);
  Node* y = mt.AddNode(mt.common()->Projection(1), ret3);
  Node* z = mt.AddNode(mt.common()->Projection(2), ret3);
  Node* ret = mt.Int32Add(mt.Int32Add(x, y), z);
  mt.Return(ret);
#ifdef ENABLE_DISASSEMBLER
  Handle<Code> code2 = mt.GetCode();
  if (FLAG_print_code) {
    OFStream os(stdout);
    code2->Disassemble("three_value_call", os);
  }
#endif
  CHECK_EQ((123 + 456) + (123 - 456) + (123 * 456), mt.Call());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

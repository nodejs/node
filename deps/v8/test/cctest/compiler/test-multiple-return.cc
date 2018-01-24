// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/value-helper.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

CallDescriptor* GetCallDescriptor(Zone* zone, int return_count,
                                  int param_count) {
  LocationSignature::Builder locations(zone, return_count, param_count);
  const RegisterConfiguration* config = RegisterConfiguration::Default();

  // Add return location(s).
  CHECK(return_count <= config->num_allocatable_general_registers());
  for (int i = 0; i < return_count; i++) {
    locations.AddReturn(LinkageLocation::ForRegister(
        config->allocatable_general_codes()[i], MachineType::AnyTagged()));
  }

  // Add register and/or stack parameter(s).
  CHECK(param_count <= config->num_allocatable_general_registers());
  for (int i = 0; i < param_count; i++) {
    locations.AddParam(LinkageLocation::ForRegister(
        config->allocatable_general_codes()[i], MachineType::AnyTagged()));
  }

  const RegList kCalleeSaveRegisters = 0;
  const RegList kCalleeSaveFPRegisters = 0;

  // The target for wasm calls is always a code object.
  MachineType target_type = MachineType::AnyTagged();
  LinkageLocation target_loc = LinkageLocation::ForAnyRegister();
  return new (zone) CallDescriptor(       // --
      CallDescriptor::kCallCodeObject,    // kind
      target_type,                        // target MachineType
      target_loc,                         // target location
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
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  CallDescriptor* desc = GetCallDescriptor(&zone, 3, 2);
  HandleAndZoneScope handles;
  RawMachineAssembler m(handles.main_isolate(),
                        new (handles.main_zone()) Graph(handles.main_zone()),
                        desc, MachineType::PointerRepresentation(),
                        InstructionSelector::SupportedMachineOperatorFlags());

  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Int32Add(p0, p1);
  Node* sub = m.Int32Sub(p0, p1);
  Node* mul = m.Int32Mul(p0, p1);
  m.Return(add, sub, mul);

  CompilationInfo info(ArrayVector("testing"), handles.main_zone(), Code::STUB);
  Handle<Code> code = Pipeline::GenerateCodeForTesting(
      &info, handles.main_isolate(), desc, m.graph(), m.Export());
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

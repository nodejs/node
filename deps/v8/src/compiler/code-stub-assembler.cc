// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-stub-assembler.h"

#include <ostream>

#include "src/code-factory.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/schedule.h"
#include "src/frames.h"
#include "src/interface-descriptors.h"
#include "src/interpreter/bytecodes.h"
#include "src/machine-type.h"
#include "src/macro-assembler.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {


CodeStubAssembler::CodeStubAssembler(Isolate* isolate, Zone* zone,
                                     const CallInterfaceDescriptor& descriptor,
                                     Code::Kind kind, const char* name)
    : raw_assembler_(new RawMachineAssembler(
          isolate, new (zone) Graph(zone),
          Linkage::GetStubCallDescriptor(isolate, zone, descriptor, 0,
                                         CallDescriptor::kNoFlags))),
      kind_(kind),
      name_(name),
      code_generated_(false) {}


CodeStubAssembler::~CodeStubAssembler() {}


Handle<Code> CodeStubAssembler::GenerateCode() {
  DCHECK(!code_generated_);

  Schedule* schedule = raw_assembler_->Export();
  Handle<Code> code = Pipeline::GenerateCodeForCodeStub(
      isolate(), raw_assembler_->call_descriptor(), graph(), schedule, kind_,
      name_);

  code_generated_ = true;
  return code;
}


Node* CodeStubAssembler::Int32Constant(int value) {
  return raw_assembler_->Int32Constant(value);
}


Node* CodeStubAssembler::IntPtrConstant(intptr_t value) {
  return raw_assembler_->IntPtrConstant(value);
}


Node* CodeStubAssembler::NumberConstant(double value) {
  return raw_assembler_->NumberConstant(value);
}


Node* CodeStubAssembler::HeapConstant(Handle<HeapObject> object) {
  return raw_assembler_->HeapConstant(object);
}


Node* CodeStubAssembler::BooleanConstant(bool value) {
  return raw_assembler_->BooleanConstant(value);
}


Node* CodeStubAssembler::Parameter(int value) {
  return raw_assembler_->Parameter(value);
}


void CodeStubAssembler::Return(Node* value) {
  return raw_assembler_->Return(value);
}


Node* CodeStubAssembler::SmiShiftBitsConstant() {
  return Int32Constant(kSmiShiftSize + kSmiTagSize);
}


Node* CodeStubAssembler::SmiTag(Node* value) {
  return raw_assembler_->WordShl(value, SmiShiftBitsConstant());
}


Node* CodeStubAssembler::SmiUntag(Node* value) {
  return raw_assembler_->WordSar(value, SmiShiftBitsConstant());
}


Node* CodeStubAssembler::IntPtrAdd(Node* a, Node* b) {
  return raw_assembler_->IntPtrAdd(a, b);
}


Node* CodeStubAssembler::IntPtrSub(Node* a, Node* b) {
  return raw_assembler_->IntPtrSub(a, b);
}


Node* CodeStubAssembler::WordShl(Node* value, int shift) {
  return raw_assembler_->WordShl(value, Int32Constant(shift));
}


Node* CodeStubAssembler::LoadObjectField(Node* object, int offset) {
  return raw_assembler_->Load(MachineType::AnyTagged(), object,
                              IntPtrConstant(offset - kHeapObjectTag));
}


Node* CodeStubAssembler::CallN(CallDescriptor* descriptor, Node* code_target,
                               Node** args) {
  return raw_assembler_->CallN(descriptor, code_target, args);
}


Node* CodeStubAssembler::TailCallN(CallDescriptor* descriptor,
                                   Node* code_target, Node** args) {
  return raw_assembler_->TailCallN(descriptor, code_target, args);
}


Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1) {
  return raw_assembler_->CallRuntime1(function_id, arg1, context);
}


Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1, Node* arg2) {
  return raw_assembler_->CallRuntime2(function_id, arg1, arg2, context);
}


Node* CodeStubAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                         Node* context, Node* arg1) {
  return raw_assembler_->TailCallRuntime1(function_id, arg1, context);
}


Node* CodeStubAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                         Node* context, Node* arg1,
                                         Node* arg2) {
  return raw_assembler_->TailCallRuntime2(function_id, arg1, arg2, context);
}


// RawMachineAssembler delegate helpers:
Isolate* CodeStubAssembler::isolate() { return raw_assembler_->isolate(); }


Graph* CodeStubAssembler::graph() { return raw_assembler_->graph(); }


Zone* CodeStubAssembler::zone() { return raw_assembler_->zone(); }


}  // namespace compiler
}  // namespace internal
}  // namespace v8

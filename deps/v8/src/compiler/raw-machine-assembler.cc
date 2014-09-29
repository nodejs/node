// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/scheduler.h"

namespace v8 {
namespace internal {
namespace compiler {

RawMachineAssembler::RawMachineAssembler(
    Graph* graph, MachineCallDescriptorBuilder* call_descriptor_builder,
    MachineType word)
    : GraphBuilder(graph),
      schedule_(new (zone()) Schedule(zone())),
      machine_(zone(), word),
      common_(zone()),
      call_descriptor_builder_(call_descriptor_builder),
      parameters_(NULL),
      exit_label_(schedule()->exit()),
      current_block_(schedule()->entry()) {
  Node* s = graph->NewNode(common_.Start(parameter_count()));
  graph->SetStart(s);
  if (parameter_count() == 0) return;
  parameters_ = zone()->NewArray<Node*>(parameter_count());
  for (int i = 0; i < parameter_count(); ++i) {
    parameters_[i] = NewNode(common()->Parameter(i), graph->start());
  }
}


Schedule* RawMachineAssembler::Export() {
  // Compute the correct codegen order.
  DCHECK(schedule_->rpo_order()->empty());
  Scheduler::ComputeSpecialRPO(schedule_);
  // Invalidate MachineAssembler.
  Schedule* schedule = schedule_;
  schedule_ = NULL;
  return schedule;
}


Node* RawMachineAssembler::Parameter(int index) {
  DCHECK(0 <= index && index < parameter_count());
  return parameters_[index];
}


RawMachineAssembler::Label* RawMachineAssembler::Exit() {
  exit_label_.used_ = true;
  return &exit_label_;
}


void RawMachineAssembler::Goto(Label* label) {
  DCHECK(current_block_ != schedule()->exit());
  schedule()->AddGoto(CurrentBlock(), Use(label));
  current_block_ = NULL;
}


void RawMachineAssembler::Branch(Node* condition, Label* true_val,
                                 Label* false_val) {
  DCHECK(current_block_ != schedule()->exit());
  Node* branch = NewNode(common()->Branch(), condition);
  schedule()->AddBranch(CurrentBlock(), branch, Use(true_val), Use(false_val));
  current_block_ = NULL;
}


void RawMachineAssembler::Return(Node* value) {
  schedule()->AddReturn(CurrentBlock(), value);
  current_block_ = NULL;
}


void RawMachineAssembler::Deoptimize(Node* state) {
  Node* deopt = graph()->NewNode(common()->Deoptimize(), state);
  schedule()->AddDeoptimize(CurrentBlock(), deopt);
  current_block_ = NULL;
}


Node* RawMachineAssembler::CallJS0(Node* function, Node* receiver,
                                   Label* continuation, Label* deoptimization) {
  CallDescriptor* descriptor = Linkage::GetJSCallDescriptor(1, zone());
  Node* call = graph()->NewNode(common()->Call(descriptor), function, receiver);
  schedule()->AddCall(CurrentBlock(), call, Use(continuation),
                      Use(deoptimization));
  current_block_ = NULL;
  return call;
}


Node* RawMachineAssembler::CallRuntime1(Runtime::FunctionId function,
                                        Node* arg0, Label* continuation,
                                        Label* deoptimization) {
  CallDescriptor* descriptor =
      Linkage::GetRuntimeCallDescriptor(function, 1, Operator::kNoProperties,
                                        CallDescriptor::kCanDeoptimize, zone());

  Node* centry = HeapConstant(CEntryStub(isolate(), 1).GetCode());
  Node* ref = NewNode(
      common()->ExternalConstant(ExternalReference(function, isolate())));
  Node* arity = Int32Constant(1);
  Node* context = Parameter(1);

  Node* call = graph()->NewNode(common()->Call(descriptor), centry, arg0, ref,
                                arity, context);
  schedule()->AddCall(CurrentBlock(), call, Use(continuation),
                      Use(deoptimization));
  current_block_ = NULL;
  return call;
}


void RawMachineAssembler::Bind(Label* label) {
  DCHECK(current_block_ == NULL);
  DCHECK(!label->bound_);
  label->bound_ = true;
  current_block_ = EnsureBlock(label);
}


BasicBlock* RawMachineAssembler::Use(Label* label) {
  label->used_ = true;
  return EnsureBlock(label);
}


BasicBlock* RawMachineAssembler::EnsureBlock(Label* label) {
  if (label->block_ == NULL) label->block_ = schedule()->NewBasicBlock();
  return label->block_;
}


BasicBlock* RawMachineAssembler::CurrentBlock() {
  DCHECK(current_block_);
  return current_block_;
}


Node* RawMachineAssembler::MakeNode(Operator* op, int input_count,
                                    Node** inputs) {
  DCHECK(ScheduleValid());
  DCHECK(current_block_ != NULL);
  Node* node = graph()->NewNode(op, input_count, inputs);
  BasicBlock* block = op->opcode() == IrOpcode::kParameter ? schedule()->start()
                                                           : CurrentBlock();
  schedule()->AddNode(block, node);
  return node;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

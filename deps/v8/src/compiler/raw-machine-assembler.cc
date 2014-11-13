// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-factory.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/scheduler.h"

namespace v8 {
namespace internal {
namespace compiler {

RawMachineAssembler::RawMachineAssembler(Graph* graph,
                                         MachineSignature* machine_sig,
                                         MachineType word,
                                         MachineOperatorBuilder::Flags flags)
    : GraphBuilder(graph),
      schedule_(new (zone()) Schedule(zone())),
      machine_(word, flags),
      common_(zone()),
      machine_sig_(machine_sig),
      call_descriptor_(
          Linkage::GetSimplifiedCDescriptor(graph->zone(), machine_sig)),
      parameters_(NULL),
      exit_label_(schedule()->end()),
      current_block_(schedule()->start()) {
  int param_count = static_cast<int>(parameter_count());
  Node* s = graph->NewNode(common_.Start(param_count));
  graph->SetStart(s);
  if (parameter_count() == 0) return;
  parameters_ = zone()->NewArray<Node*>(param_count);
  for (size_t i = 0; i < parameter_count(); ++i) {
    parameters_[i] =
        NewNode(common()->Parameter(static_cast<int>(i)), graph->start());
  }
}


Schedule* RawMachineAssembler::Export() {
  // Compute the correct codegen order.
  DCHECK(schedule_->rpo_order()->empty());
  ZonePool zone_pool(isolate());
  Scheduler::ComputeSpecialRPO(&zone_pool, schedule_);
  // Invalidate MachineAssembler.
  Schedule* schedule = schedule_;
  schedule_ = NULL;
  return schedule;
}


Node* RawMachineAssembler::Parameter(size_t index) {
  DCHECK(index < parameter_count());
  return parameters_[index];
}


RawMachineAssembler::Label* RawMachineAssembler::Exit() {
  exit_label_.used_ = true;
  return &exit_label_;
}


void RawMachineAssembler::Goto(Label* label) {
  DCHECK(current_block_ != schedule()->end());
  schedule()->AddGoto(CurrentBlock(), Use(label));
  current_block_ = NULL;
}


void RawMachineAssembler::Branch(Node* condition, Label* true_val,
                                 Label* false_val) {
  DCHECK(current_block_ != schedule()->end());
  Node* branch = NewNode(common()->Branch(), condition);
  schedule()->AddBranch(CurrentBlock(), branch, Use(true_val), Use(false_val));
  current_block_ = NULL;
}


void RawMachineAssembler::Return(Node* value) {
  schedule()->AddReturn(CurrentBlock(), value);
  current_block_ = NULL;
}


Node* RawMachineAssembler::CallFunctionStub0(Node* function, Node* receiver,
                                             Node* context, Node* frame_state,
                                             CallFunctionFlags flags) {
  Callable callable = CodeFactory::CallFunction(isolate(), 0, flags);
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      callable.descriptor(), 1, CallDescriptor::kNeedsFrameState, zone());
  Node* stub_code = HeapConstant(callable.code());
  Node* call = graph()->NewNode(common()->Call(desc), stub_code, function,
                                receiver, context, frame_state);
  schedule()->AddNode(CurrentBlock(), call);
  return call;
}


Node* RawMachineAssembler::CallJS0(Node* function, Node* receiver,
                                   Node* context, Node* frame_state) {
  CallDescriptor* descriptor =
      Linkage::GetJSCallDescriptor(1, zone(), CallDescriptor::kNeedsFrameState);
  Node* call = graph()->NewNode(common()->Call(descriptor), function, receiver,
                                context, frame_state);
  schedule()->AddNode(CurrentBlock(), call);
  return call;
}


Node* RawMachineAssembler::CallRuntime1(Runtime::FunctionId function,
                                        Node* arg0, Node* context,
                                        Node* frame_state) {
  CallDescriptor* descriptor = Linkage::GetRuntimeCallDescriptor(
      function, 1, Operator::kNoProperties, zone());

  Node* centry = HeapConstant(CEntryStub(isolate(), 1).GetCode());
  Node* ref = NewNode(
      common()->ExternalConstant(ExternalReference(function, isolate())));
  Node* arity = Int32Constant(1);

  Node* call = graph()->NewNode(common()->Call(descriptor), centry, arg0, ref,
                                arity, context, frame_state);
  schedule()->AddNode(CurrentBlock(), call);
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


Node* RawMachineAssembler::MakeNode(const Operator* op, int input_count,
                                    Node** inputs, bool incomplete) {
  DCHECK(ScheduleValid());
  DCHECK(current_block_ != NULL);
  Node* node = graph()->NewNode(op, input_count, inputs, incomplete);
  BasicBlock* block = op->opcode() == IrOpcode::kParameter ? schedule()->start()
                                                           : CurrentBlock();
  schedule()->AddNode(block, node);
  return node;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

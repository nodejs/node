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

RawMachineAssembler::RawMachineAssembler(Isolate* isolate, Graph* graph,
                                         const MachineSignature* machine_sig,
                                         MachineType word,
                                         MachineOperatorBuilder::Flags flags)
    : GraphBuilder(isolate, graph),
      schedule_(new (zone()) Schedule(zone())),
      machine_(zone(), word, flags),
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
  Scheduler::ComputeSpecialRPO(zone(), schedule_);
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


void RawMachineAssembler::Switch(Node* index, Label* default_label,
                                 int32_t* case_values, Label** case_labels,
                                 size_t case_count) {
  DCHECK_NE(schedule()->end(), current_block_);
  size_t succ_count = case_count + 1;
  Node* switch_node = NewNode(common()->Switch(succ_count), index);
  BasicBlock** succ_blocks = zone()->NewArray<BasicBlock*>(succ_count);
  for (size_t index = 0; index < case_count; ++index) {
    int32_t case_value = case_values[index];
    BasicBlock* case_block = Use(case_labels[index]);
    Node* case_node =
        graph()->NewNode(common()->IfValue(case_value), switch_node);
    schedule()->AddNode(case_block, case_node);
    succ_blocks[index] = case_block;
  }
  BasicBlock* default_block = Use(default_label);
  Node* default_node = graph()->NewNode(common()->IfDefault(), switch_node);
  schedule()->AddNode(default_block, default_node);
  succ_blocks[case_count] = default_block;
  schedule()->AddSwitch(CurrentBlock(), switch_node, succ_blocks, succ_count);
  current_block_ = nullptr;
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
      isolate(), zone(), callable.descriptor(), 1,
      CallDescriptor::kNeedsFrameState, Operator::kNoProperties);
  Node* stub_code = HeapConstant(callable.code());
  Node* call = graph()->NewNode(common()->Call(desc), stub_code, function,
                                receiver, context, frame_state);
  schedule()->AddNode(CurrentBlock(), call);
  return call;
}


Node* RawMachineAssembler::CallJS0(Node* function, Node* receiver,
                                   Node* context, Node* frame_state) {
  CallDescriptor* descriptor = Linkage::GetJSCallDescriptor(
      zone(), false, 1, CallDescriptor::kNeedsFrameState);
  Node* call = graph()->NewNode(common()->Call(descriptor), function, receiver,
                                context, frame_state);
  schedule()->AddNode(CurrentBlock(), call);
  return call;
}


Node* RawMachineAssembler::CallRuntime1(Runtime::FunctionId function,
                                        Node* arg0, Node* context,
                                        Node* frame_state) {
  CallDescriptor* descriptor = Linkage::GetRuntimeCallDescriptor(
      zone(), function, 1, Operator::kNoProperties);

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

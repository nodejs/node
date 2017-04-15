// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/raw-machine-assembler.h"

#include "src/compiler/node-properties.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/scheduler.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

RawMachineAssembler::RawMachineAssembler(
    Isolate* isolate, Graph* graph, CallDescriptor* call_descriptor,
    MachineRepresentation word, MachineOperatorBuilder::Flags flags,
    MachineOperatorBuilder::AlignmentRequirements alignment_requirements)
    : isolate_(isolate),
      graph_(graph),
      schedule_(new (zone()) Schedule(zone())),
      machine_(zone(), word, flags, alignment_requirements),
      common_(zone()),
      call_descriptor_(call_descriptor),
      parameters_(parameter_count(), zone()),
      current_block_(schedule()->start()) {
  int param_count = static_cast<int>(parameter_count());
  // Add an extra input for the JSFunction parameter to the start node.
  graph->SetStart(graph->NewNode(common_.Start(param_count + 1)));
  for (size_t i = 0; i < parameter_count(); ++i) {
    parameters_[i] =
        AddNode(common()->Parameter(static_cast<int>(i)), graph->start());
  }
  graph->SetEnd(graph->NewNode(common_.End(0)));
}

Node* RawMachineAssembler::RelocatableIntPtrConstant(intptr_t value,
                                                     RelocInfo::Mode rmode) {
  return kPointerSize == 8
             ? RelocatableInt64Constant(value, rmode)
             : RelocatableInt32Constant(static_cast<int>(value), rmode);
}

Schedule* RawMachineAssembler::Export() {
  // Compute the correct codegen order.
  DCHECK(schedule_->rpo_order()->empty());
  OFStream os(stdout);
  if (FLAG_trace_turbo_scheduler) {
    PrintF("--- RAW SCHEDULE -------------------------------------------\n");
    os << *schedule_;
  }
  schedule_->EnsureCFGWellFormedness();
  Scheduler::ComputeSpecialRPO(zone(), schedule_);
  schedule_->PropagateDeferredMark();
  if (FLAG_trace_turbo_scheduler) {
    PrintF("--- EDGE SPLIT AND PROPAGATED DEFERRED SCHEDULE ------------\n");
    os << *schedule_;
  }
  // Invalidate RawMachineAssembler.
  Schedule* schedule = schedule_;
  schedule_ = nullptr;
  return schedule;
}


Node* RawMachineAssembler::Parameter(size_t index) {
  DCHECK(index < parameter_count());
  return parameters_[index];
}


void RawMachineAssembler::Goto(RawMachineLabel* label) {
  DCHECK(current_block_ != schedule()->end());
  schedule()->AddGoto(CurrentBlock(), Use(label));
  current_block_ = nullptr;
}


void RawMachineAssembler::Branch(Node* condition, RawMachineLabel* true_val,
                                 RawMachineLabel* false_val) {
  DCHECK(current_block_ != schedule()->end());
  Node* branch = MakeNode(common()->Branch(), 1, &condition);
  schedule()->AddBranch(CurrentBlock(), branch, Use(true_val), Use(false_val));
  current_block_ = nullptr;
}

void RawMachineAssembler::Continuations(Node* call, RawMachineLabel* if_success,
                                        RawMachineLabel* if_exception) {
  DCHECK_NOT_NULL(schedule_);
  DCHECK_NOT_NULL(current_block_);
  schedule()->AddCall(CurrentBlock(), call, Use(if_success), Use(if_exception));
  current_block_ = nullptr;
}

void RawMachineAssembler::Switch(Node* index, RawMachineLabel* default_label,
                                 const int32_t* case_values,
                                 RawMachineLabel** case_labels,
                                 size_t case_count) {
  DCHECK_NE(schedule()->end(), current_block_);
  size_t succ_count = case_count + 1;
  Node* switch_node = AddNode(common()->Switch(succ_count), index);
  BasicBlock** succ_blocks = zone()->NewArray<BasicBlock*>(succ_count);
  for (size_t index = 0; index < case_count; ++index) {
    int32_t case_value = case_values[index];
    BasicBlock* case_block = schedule()->NewBasicBlock();
    Node* case_node =
        graph()->NewNode(common()->IfValue(case_value), switch_node);
    schedule()->AddNode(case_block, case_node);
    schedule()->AddGoto(case_block, Use(case_labels[index]));
    succ_blocks[index] = case_block;
  }
  BasicBlock* default_block = schedule()->NewBasicBlock();
  Node* default_node = graph()->NewNode(common()->IfDefault(), switch_node);
  schedule()->AddNode(default_block, default_node);
  schedule()->AddGoto(default_block, Use(default_label));
  succ_blocks[case_count] = default_block;
  schedule()->AddSwitch(CurrentBlock(), switch_node, succ_blocks, succ_count);
  current_block_ = nullptr;
}

void RawMachineAssembler::Return(Node* value) {
  Node* values[] = {Int32Constant(0), value};
  Node* ret = MakeNode(common()->Return(1), 2, values);
  schedule()->AddReturn(CurrentBlock(), ret);
  current_block_ = nullptr;
}


void RawMachineAssembler::Return(Node* v1, Node* v2) {
  Node* values[] = {Int32Constant(0), v1, v2};
  Node* ret = MakeNode(common()->Return(2), 3, values);
  schedule()->AddReturn(CurrentBlock(), ret);
  current_block_ = nullptr;
}


void RawMachineAssembler::Return(Node* v1, Node* v2, Node* v3) {
  Node* values[] = {Int32Constant(0), v1, v2, v3};
  Node* ret = MakeNode(common()->Return(3), 4, values);
  schedule()->AddReturn(CurrentBlock(), ret);
  current_block_ = nullptr;
}

void RawMachineAssembler::PopAndReturn(Node* pop, Node* value) {
  Node* values[] = {pop, value};
  Node* ret = MakeNode(common()->Return(1), 2, values);
  schedule()->AddReturn(CurrentBlock(), ret);
  current_block_ = nullptr;
}

void RawMachineAssembler::PopAndReturn(Node* pop, Node* v1, Node* v2) {
  Node* values[] = {pop, v1, v2};
  Node* ret = MakeNode(common()->Return(2), 3, values);
  schedule()->AddReturn(CurrentBlock(), ret);
  current_block_ = nullptr;
}

void RawMachineAssembler::PopAndReturn(Node* pop, Node* v1, Node* v2,
                                       Node* v3) {
  Node* values[] = {pop, v1, v2, v3};
  Node* ret = MakeNode(common()->Return(3), 4, values);
  schedule()->AddReturn(CurrentBlock(), ret);
  current_block_ = nullptr;
}

void RawMachineAssembler::DebugBreak() { AddNode(machine()->DebugBreak()); }

void RawMachineAssembler::Comment(const char* msg) {
  AddNode(machine()->Comment(msg));
}

Node* RawMachineAssembler::CallN(CallDescriptor* desc, int input_count,
                                 Node* const* inputs) {
  DCHECK(!desc->NeedsFrameState());
  // +1 is for target.
  DCHECK_EQ(input_count, desc->ParameterCount() + 1);
  return AddNode(common()->Call(desc), input_count, inputs);
}

Node* RawMachineAssembler::CallNWithFrameState(CallDescriptor* desc,
                                               int input_count,
                                               Node* const* inputs) {
  DCHECK(desc->NeedsFrameState());
  // +2 is for target and frame state.
  DCHECK_EQ(input_count, desc->ParameterCount() + 2);
  return AddNode(common()->Call(desc), input_count, inputs);
}

Node* RawMachineAssembler::TailCallN(CallDescriptor* desc, int input_count,
                                     Node* const* inputs) {
  // +1 is for target.
  DCHECK_EQ(input_count, desc->ParameterCount() + 1);
  Node* tail_call = MakeNode(common()->TailCall(desc), input_count, inputs);
  schedule()->AddTailCall(CurrentBlock(), tail_call);
  current_block_ = nullptr;
  return tail_call;
}

Node* RawMachineAssembler::CallCFunction0(MachineType return_type,
                                          Node* function) {
  MachineSignature::Builder builder(zone(), 1, 0);
  builder.AddReturn(return_type);
  const CallDescriptor* descriptor =
      Linkage::GetSimplifiedCDescriptor(zone(), builder.Build());

  return AddNode(common()->Call(descriptor), function);
}


Node* RawMachineAssembler::CallCFunction1(MachineType return_type,
                                          MachineType arg0_type, Node* function,
                                          Node* arg0) {
  MachineSignature::Builder builder(zone(), 1, 1);
  builder.AddReturn(return_type);
  builder.AddParam(arg0_type);
  const CallDescriptor* descriptor =
      Linkage::GetSimplifiedCDescriptor(zone(), builder.Build());

  return AddNode(common()->Call(descriptor), function, arg0);
}


Node* RawMachineAssembler::CallCFunction2(MachineType return_type,
                                          MachineType arg0_type,
                                          MachineType arg1_type, Node* function,
                                          Node* arg0, Node* arg1) {
  MachineSignature::Builder builder(zone(), 1, 2);
  builder.AddReturn(return_type);
  builder.AddParam(arg0_type);
  builder.AddParam(arg1_type);
  const CallDescriptor* descriptor =
      Linkage::GetSimplifiedCDescriptor(zone(), builder.Build());

  return AddNode(common()->Call(descriptor), function, arg0, arg1);
}

Node* RawMachineAssembler::CallCFunction3(MachineType return_type,
                                          MachineType arg0_type,
                                          MachineType arg1_type,
                                          MachineType arg2_type, Node* function,
                                          Node* arg0, Node* arg1, Node* arg2) {
  MachineSignature::Builder builder(zone(), 1, 3);
  builder.AddReturn(return_type);
  builder.AddParam(arg0_type);
  builder.AddParam(arg1_type);
  builder.AddParam(arg2_type);
  const CallDescriptor* descriptor =
      Linkage::GetSimplifiedCDescriptor(zone(), builder.Build());

  return AddNode(common()->Call(descriptor), function, arg0, arg1, arg2);
}

Node* RawMachineAssembler::CallCFunction8(
    MachineType return_type, MachineType arg0_type, MachineType arg1_type,
    MachineType arg2_type, MachineType arg3_type, MachineType arg4_type,
    MachineType arg5_type, MachineType arg6_type, MachineType arg7_type,
    Node* function, Node* arg0, Node* arg1, Node* arg2, Node* arg3, Node* arg4,
    Node* arg5, Node* arg6, Node* arg7) {
  MachineSignature::Builder builder(zone(), 1, 8);
  builder.AddReturn(return_type);
  builder.AddParam(arg0_type);
  builder.AddParam(arg1_type);
  builder.AddParam(arg2_type);
  builder.AddParam(arg3_type);
  builder.AddParam(arg4_type);
  builder.AddParam(arg5_type);
  builder.AddParam(arg6_type);
  builder.AddParam(arg7_type);
  Node* args[] = {function, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7};
  const CallDescriptor* descriptor =
      Linkage::GetSimplifiedCDescriptor(zone(), builder.Build());
  return AddNode(common()->Call(descriptor), arraysize(args), args);
}


void RawMachineAssembler::Bind(RawMachineLabel* label) {
  DCHECK(current_block_ == nullptr);
  DCHECK(!label->bound_);
  label->bound_ = true;
  current_block_ = EnsureBlock(label);
  current_block_->set_deferred(label->deferred_);
}


BasicBlock* RawMachineAssembler::Use(RawMachineLabel* label) {
  label->used_ = true;
  return EnsureBlock(label);
}


BasicBlock* RawMachineAssembler::EnsureBlock(RawMachineLabel* label) {
  if (label->block_ == nullptr) label->block_ = schedule()->NewBasicBlock();
  return label->block_;
}


BasicBlock* RawMachineAssembler::CurrentBlock() {
  DCHECK(current_block_);
  return current_block_;
}

Node* RawMachineAssembler::Phi(MachineRepresentation rep, int input_count,
                               Node* const* inputs) {
  Node** buffer = new (zone()->New(sizeof(Node*) * (input_count + 1)))
      Node*[input_count + 1];
  std::copy(inputs, inputs + input_count, buffer);
  buffer[input_count] = graph()->start();
  return AddNode(common()->Phi(rep, input_count), input_count + 1, buffer);
}

void RawMachineAssembler::AppendPhiInput(Node* phi, Node* new_input) {
  const Operator* op = phi->op();
  const Operator* new_op = common()->ResizeMergeOrPhi(op, phi->InputCount());
  phi->InsertInput(zone(), phi->InputCount() - 1, new_input);
  NodeProperties::ChangeOp(phi, new_op);
}

Node* RawMachineAssembler::AddNode(const Operator* op, int input_count,
                                   Node* const* inputs) {
  DCHECK_NOT_NULL(schedule_);
  DCHECK_NOT_NULL(current_block_);
  Node* node = MakeNode(op, input_count, inputs);
  schedule()->AddNode(CurrentBlock(), node);
  return node;
}

Node* RawMachineAssembler::MakeNode(const Operator* op, int input_count,
                                    Node* const* inputs) {
  // The raw machine assembler nodes do not have effect and control inputs,
  // so we disable checking input counts here.
  return graph()->NewNodeUnchecked(op, input_count, inputs);
}

RawMachineLabel::~RawMachineLabel() { DCHECK(bound_ || !used_); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8

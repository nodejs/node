// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-builder.h"

#include "src/compiler.h"
#include "src/compiler/generic-graph.h"
#include "src/compiler/generic-node.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/operator-properties-inl.h"

namespace v8 {
namespace internal {
namespace compiler {


StructuredGraphBuilder::StructuredGraphBuilder(Zone* local_zone, Graph* graph,
                                               CommonOperatorBuilder* common)
    : GraphBuilder(graph),
      common_(common),
      environment_(NULL),
      local_zone_(local_zone),
      input_buffer_size_(0),
      input_buffer_(NULL),
      current_context_(NULL),
      exit_control_(NULL) {
  EnsureInputBufferSize(kInputBufferSizeIncrement);
}


Node** StructuredGraphBuilder::EnsureInputBufferSize(int size) {
  if (size > input_buffer_size_) {
    size += kInputBufferSizeIncrement;
    input_buffer_ = local_zone()->NewArray<Node*>(size);
  }
  return input_buffer_;
}


Node* StructuredGraphBuilder::MakeNode(const Operator* op,
                                       int value_input_count,
                                       Node** value_inputs, bool incomplete) {
  DCHECK(op->InputCount() == value_input_count);

  bool has_context = OperatorProperties::HasContextInput(op);
  bool has_framestate = OperatorProperties::HasFrameStateInput(op);
  bool has_control = op->ControlInputCount() == 1;
  bool has_effect = op->EffectInputCount() == 1;

  DCHECK(op->ControlInputCount() < 2);
  DCHECK(op->EffectInputCount() < 2);

  Node* result = NULL;
  if (!has_context && !has_framestate && !has_control && !has_effect) {
    result = graph()->NewNode(op, value_input_count, value_inputs, incomplete);
  } else {
    int input_count_with_deps = value_input_count;
    if (has_context) ++input_count_with_deps;
    if (has_framestate) ++input_count_with_deps;
    if (has_control) ++input_count_with_deps;
    if (has_effect) ++input_count_with_deps;
    Node** buffer = EnsureInputBufferSize(input_count_with_deps);
    memcpy(buffer, value_inputs, kPointerSize * value_input_count);
    Node** current_input = buffer + value_input_count;
    if (has_context) {
      *current_input++ = current_context();
    }
    if (has_framestate) {
      // The frame state will be inserted later. Here we misuse
      // the dead_control node as a sentinel to be later overwritten
      // with the real frame state.
      *current_input++ = dead_control();
    }
    if (has_effect) {
      *current_input++ = environment_->GetEffectDependency();
    }
    if (has_control) {
      *current_input++ = environment_->GetControlDependency();
    }
    result = graph()->NewNode(op, input_count_with_deps, buffer, incomplete);
    if (has_effect) {
      environment_->UpdateEffectDependency(result);
    }
    if (result->op()->ControlOutputCount() > 0 &&
        !environment()->IsMarkedAsUnreachable()) {
      environment_->UpdateControlDependency(result);
    }
  }

  return result;
}


void StructuredGraphBuilder::UpdateControlDependencyToLeaveFunction(
    Node* exit) {
  if (environment()->IsMarkedAsUnreachable()) return;
  if (exit_control() != NULL) {
    exit = MergeControl(exit_control(), exit);
  }
  environment()->MarkAsUnreachable();
  set_exit_control(exit);
}


StructuredGraphBuilder::Environment* StructuredGraphBuilder::CopyEnvironment(
    Environment* env) {
  return new (local_zone()) Environment(*env);
}


StructuredGraphBuilder::Environment::Environment(
    StructuredGraphBuilder* builder, Node* control_dependency)
    : builder_(builder),
      control_dependency_(control_dependency),
      effect_dependency_(control_dependency),
      values_(zone()) {}


StructuredGraphBuilder::Environment::Environment(const Environment& copy)
    : builder_(copy.builder()),
      control_dependency_(copy.control_dependency_),
      effect_dependency_(copy.effect_dependency_),
      values_(copy.zone()) {
  const size_t kStackEstimate = 7;  // optimum from experimentation!
  values_.reserve(copy.values_.size() + kStackEstimate);
  values_.insert(values_.begin(), copy.values_.begin(), copy.values_.end());
}


void StructuredGraphBuilder::Environment::Merge(Environment* other) {
  DCHECK(values_.size() == other->values_.size());

  // Nothing to do if the other environment is dead.
  if (other->IsMarkedAsUnreachable()) return;

  // Resurrect a dead environment by copying the contents of the other one and
  // placing a singleton merge as the new control dependency.
  if (this->IsMarkedAsUnreachable()) {
    Node* other_control = other->control_dependency_;
    Node* inputs[] = {other_control};
    control_dependency_ =
        graph()->NewNode(common()->Merge(1), arraysize(inputs), inputs, true);
    effect_dependency_ = other->effect_dependency_;
    values_ = other->values_;
    return;
  }

  // Create a merge of the control dependencies of both environments and update
  // the current environment's control dependency accordingly.
  Node* control = builder_->MergeControl(this->GetControlDependency(),
                                         other->GetControlDependency());
  UpdateControlDependency(control);

  // Create a merge of the effect dependencies of both environments and update
  // the current environment's effect dependency accordingly.
  Node* effect = builder_->MergeEffect(this->GetEffectDependency(),
                                       other->GetEffectDependency(), control);
  UpdateEffectDependency(effect);

  // Introduce Phi nodes for values that have differing input at merge points,
  // potentially extending an existing Phi node if possible.
  for (int i = 0; i < static_cast<int>(values_.size()); ++i) {
    values_[i] = builder_->MergeValue(values_[i], other->values_[i], control);
  }
}


void StructuredGraphBuilder::Environment::PrepareForLoop(BitVector* assigned) {
  Node* control = GetControlDependency();
  int size = static_cast<int>(values()->size());
  if (assigned == NULL) {
    // Assume that everything is updated in the loop.
    for (int i = 0; i < size; ++i) {
      Node* phi = builder_->NewPhi(1, values()->at(i), control);
      values()->at(i) = phi;
    }
  } else {
    // Only build phis for those locals assigned in this loop.
    for (int i = 0; i < size; ++i) {
      if (i < assigned->length() && !assigned->Contains(i)) continue;
      Node* phi = builder_->NewPhi(1, values()->at(i), control);
      values()->at(i) = phi;
    }
  }
  Node* effect = builder_->NewEffectPhi(1, GetEffectDependency(), control);
  UpdateEffectDependency(effect);
}


Node* StructuredGraphBuilder::NewPhi(int count, Node* input, Node* control) {
  const Operator* phi_op = common()->Phi(kMachAnyTagged, count);
  Node** buffer = EnsureInputBufferSize(count + 1);
  MemsetPointer(buffer, input, count);
  buffer[count] = control;
  return graph()->NewNode(phi_op, count + 1, buffer, true);
}


// TODO(mstarzinger): Revisit this once we have proper effect states.
Node* StructuredGraphBuilder::NewEffectPhi(int count, Node* input,
                                           Node* control) {
  const Operator* phi_op = common()->EffectPhi(count);
  Node** buffer = EnsureInputBufferSize(count + 1);
  MemsetPointer(buffer, input, count);
  buffer[count] = control;
  return graph()->NewNode(phi_op, count + 1, buffer, true);
}


Node* StructuredGraphBuilder::MergeControl(Node* control, Node* other) {
  int inputs = control->op()->ControlInputCount() + 1;
  if (control->opcode() == IrOpcode::kLoop) {
    // Control node for loop exists, add input.
    const Operator* op = common()->Loop(inputs);
    control->AppendInput(graph_zone(), other);
    control->set_op(op);
  } else if (control->opcode() == IrOpcode::kMerge) {
    // Control node for merge exists, add input.
    const Operator* op = common()->Merge(inputs);
    control->AppendInput(graph_zone(), other);
    control->set_op(op);
  } else {
    // Control node is a singleton, introduce a merge.
    const Operator* op = common()->Merge(inputs);
    Node* inputs[] = {control, other};
    control = graph()->NewNode(op, arraysize(inputs), inputs, true);
  }
  return control;
}


Node* StructuredGraphBuilder::MergeEffect(Node* value, Node* other,
                                          Node* control) {
  int inputs = control->op()->ControlInputCount();
  if (value->opcode() == IrOpcode::kEffectPhi &&
      NodeProperties::GetControlInput(value) == control) {
    // Phi already exists, add input.
    value->set_op(common()->EffectPhi(inputs));
    value->InsertInput(graph_zone(), inputs - 1, other);
  } else if (value != other) {
    // Phi does not exist yet, introduce one.
    value = NewEffectPhi(inputs, value, control);
    value->ReplaceInput(inputs - 1, other);
  }
  return value;
}


Node* StructuredGraphBuilder::MergeValue(Node* value, Node* other,
                                         Node* control) {
  int inputs = control->op()->ControlInputCount();
  if (value->opcode() == IrOpcode::kPhi &&
      NodeProperties::GetControlInput(value) == control) {
    // Phi already exists, add input.
    value->set_op(common()->Phi(kMachAnyTagged, inputs));
    value->InsertInput(graph_zone(), inputs - 1, other);
  } else if (value != other) {
    // Phi does not exist yet, introduce one.
    value = NewPhi(inputs, value, control);
    value->ReplaceInput(inputs - 1, other);
  }
  return value;
}


Node* StructuredGraphBuilder::dead_control() {
  if (!dead_control_.is_set()) {
    Node* dead_node = graph()->NewNode(common_->Dead());
    dead_control_.set(dead_node);
    return dead_node;
  }
  return dead_control_.get();
}
}
}
}  // namespace v8::internal::compiler

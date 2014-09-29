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


StructuredGraphBuilder::StructuredGraphBuilder(Graph* graph,
                                               CommonOperatorBuilder* common)
    : GraphBuilder(graph),
      common_(common),
      environment_(NULL),
      current_context_(NULL),
      exit_control_(NULL) {}


Node* StructuredGraphBuilder::MakeNode(Operator* op, int value_input_count,
                                       Node** value_inputs) {
  bool has_context = OperatorProperties::HasContextInput(op);
  bool has_control = OperatorProperties::GetControlInputCount(op) == 1;
  bool has_effect = OperatorProperties::GetEffectInputCount(op) == 1;

  DCHECK(OperatorProperties::GetControlInputCount(op) < 2);
  DCHECK(OperatorProperties::GetEffectInputCount(op) < 2);

  Node* result = NULL;
  if (!has_context && !has_control && !has_effect) {
    result = graph()->NewNode(op, value_input_count, value_inputs);
  } else {
    int input_count_with_deps = value_input_count;
    if (has_context) ++input_count_with_deps;
    if (has_control) ++input_count_with_deps;
    if (has_effect) ++input_count_with_deps;
    void* raw_buffer = alloca(kPointerSize * input_count_with_deps);
    Node** buffer = reinterpret_cast<Node**>(raw_buffer);
    memcpy(buffer, value_inputs, kPointerSize * value_input_count);
    Node** current_input = buffer + value_input_count;
    if (has_context) {
      *current_input++ = current_context();
    }
    if (has_effect) {
      *current_input++ = environment_->GetEffectDependency();
    }
    if (has_control) {
      *current_input++ = environment_->GetControlDependency();
    }
    result = graph()->NewNode(op, input_count_with_deps, buffer);
    if (has_effect) {
      environment_->UpdateEffectDependency(result);
    }
    if (OperatorProperties::HasControlOutput(result->op()) &&
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
  return new (zone()) Environment(*env);
}


StructuredGraphBuilder::Environment::Environment(
    StructuredGraphBuilder* builder, Node* control_dependency)
    : builder_(builder),
      control_dependency_(control_dependency),
      effect_dependency_(control_dependency),
      values_(NodeVector::allocator_type(zone())) {}


StructuredGraphBuilder::Environment::Environment(const Environment& copy)
    : builder_(copy.builder()),
      control_dependency_(copy.control_dependency_),
      effect_dependency_(copy.effect_dependency_),
      values_(copy.values_) {}


void StructuredGraphBuilder::Environment::Merge(Environment* other) {
  DCHECK(values_.size() == other->values_.size());

  // Nothing to do if the other environment is dead.
  if (other->IsMarkedAsUnreachable()) return;

  // Resurrect a dead environment by copying the contents of the other one and
  // placing a singleton merge as the new control dependency.
  if (this->IsMarkedAsUnreachable()) {
    Node* other_control = other->control_dependency_;
    control_dependency_ = graph()->NewNode(common()->Merge(1), other_control);
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


void StructuredGraphBuilder::Environment::PrepareForLoop() {
  Node* control = GetControlDependency();
  for (int i = 0; i < static_cast<int>(values()->size()); ++i) {
    Node* phi = builder_->NewPhi(1, values()->at(i), control);
    values()->at(i) = phi;
  }
  Node* effect = builder_->NewEffectPhi(1, GetEffectDependency(), control);
  UpdateEffectDependency(effect);
}


Node* StructuredGraphBuilder::NewPhi(int count, Node* input, Node* control) {
  Operator* phi_op = common()->Phi(count);
  void* raw_buffer = alloca(kPointerSize * (count + 1));
  Node** buffer = reinterpret_cast<Node**>(raw_buffer);
  MemsetPointer(buffer, input, count);
  buffer[count] = control;
  return graph()->NewNode(phi_op, count + 1, buffer);
}


// TODO(mstarzinger): Revisit this once we have proper effect states.
Node* StructuredGraphBuilder::NewEffectPhi(int count, Node* input,
                                           Node* control) {
  Operator* phi_op = common()->EffectPhi(count);
  void* raw_buffer = alloca(kPointerSize * (count + 1));
  Node** buffer = reinterpret_cast<Node**>(raw_buffer);
  MemsetPointer(buffer, input, count);
  buffer[count] = control;
  return graph()->NewNode(phi_op, count + 1, buffer);
}


Node* StructuredGraphBuilder::MergeControl(Node* control, Node* other) {
  int inputs = OperatorProperties::GetControlInputCount(control->op()) + 1;
  if (control->opcode() == IrOpcode::kLoop) {
    // Control node for loop exists, add input.
    Operator* op = common()->Loop(inputs);
    control->AppendInput(zone(), other);
    control->set_op(op);
  } else if (control->opcode() == IrOpcode::kMerge) {
    // Control node for merge exists, add input.
    Operator* op = common()->Merge(inputs);
    control->AppendInput(zone(), other);
    control->set_op(op);
  } else {
    // Control node is a singleton, introduce a merge.
    Operator* op = common()->Merge(inputs);
    control = graph()->NewNode(op, control, other);
  }
  return control;
}


Node* StructuredGraphBuilder::MergeEffect(Node* value, Node* other,
                                          Node* control) {
  int inputs = OperatorProperties::GetControlInputCount(control->op());
  if (value->opcode() == IrOpcode::kEffectPhi &&
      NodeProperties::GetControlInput(value) == control) {
    // Phi already exists, add input.
    value->set_op(common()->EffectPhi(inputs));
    value->InsertInput(zone(), inputs - 1, other);
  } else if (value != other) {
    // Phi does not exist yet, introduce one.
    value = NewEffectPhi(inputs, value, control);
    value->ReplaceInput(inputs - 1, other);
  }
  return value;
}


Node* StructuredGraphBuilder::MergeValue(Node* value, Node* other,
                                         Node* control) {
  int inputs = OperatorProperties::GetControlInputCount(control->op());
  if (value->opcode() == IrOpcode::kPhi &&
      NodeProperties::GetControlInput(value) == control) {
    // Phi already exists, add input.
    value->set_op(common()->Phi(inputs));
    value->InsertInput(zone(), inputs - 1, other);
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

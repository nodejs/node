// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-type-feedback-lowering.h"

#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

JSTypeFeedbackLowering::JSTypeFeedbackLowering(Editor* editor, Flags flags,
                                               JSGraph* jsgraph)
    : AdvancedReducer(editor),
      flags_(flags),
      jsgraph_(jsgraph),
      simplified_(graph()->zone()) {}


Reduction JSTypeFeedbackLowering::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSLoadNamed:
      return ReduceJSLoadNamed(node);
    default:
      break;
  }
  return NoChange();
}


Reduction JSTypeFeedbackLowering::ReduceJSLoadNamed(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadNamed, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Type* receiver_type = NodeProperties::GetBounds(receiver).upper;
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  // We need to make optimistic assumptions to continue.
  if (!(flags() & kDeoptimizationEnabled)) return NoChange();
  LoadNamedParameters const& p = LoadNamedParametersOf(node->op());
  Handle<TypeFeedbackVector> vector;
  if (!p.feedback().vector().ToHandle(&vector)) return NoChange();
  if (p.name().handle().is_identical_to(factory()->length_string())) {
    LoadICNexus nexus(vector, p.feedback().slot());
    MapHandleList maps;
    if (nexus.ExtractMaps(&maps) > 0) {
      for (Handle<Map> map : maps) {
        if (map->instance_type() >= FIRST_NONSTRING_TYPE) return NoChange();
      }
      // Optimistic optimization for "length" property of strings.
      if (receiver_type->Maybe(Type::TaggedSigned())) {
        Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), receiver);
        Node* branch = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                        check, control);
        Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
        Node* deoptimize = graph()->NewNode(common()->Deoptimize(), frame_state,
                                            effect, if_true);
        // TODO(bmeurer): This should be on the AdvancedReducer somehow.
        NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
        control = graph()->NewNode(common()->IfFalse(), branch);
      }
      Node* receiver_map = effect =
          graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                           receiver, effect, control);
      Node* receiver_instance_type = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForMapInstanceType()),
          receiver_map, effect, control);
      Node* check =
          graph()->NewNode(machine()->Uint32LessThan(), receiver_instance_type,
                           jsgraph()->Uint32Constant(FIRST_NONSTRING_TYPE));
      Node* branch =
          graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);
      Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
      Node* deoptimize = graph()->NewNode(common()->Deoptimize(), frame_state,
                                          effect, if_false);
      // TODO(bmeurer): This should be on the AdvancedReducer somehow.
      NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
      control = graph()->NewNode(common()->IfTrue(), branch);
      Node* value = effect =
          graph()->NewNode(simplified()->LoadField(
                               AccessBuilder::ForStringLength(graph()->zone())),
                           receiver, effect, control);
      ReplaceWithValue(node, value, effect, control);
      return Replace(value);
    }
  }
  return NoChange();
}


Factory* JSTypeFeedbackLowering::factory() const {
  return isolate()->factory();
}


CommonOperatorBuilder* JSTypeFeedbackLowering::common() const {
  return jsgraph()->common();
}


Graph* JSTypeFeedbackLowering::graph() const { return jsgraph()->graph(); }


Isolate* JSTypeFeedbackLowering::isolate() const {
  return jsgraph()->isolate();
}


MachineOperatorBuilder* JSTypeFeedbackLowering::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

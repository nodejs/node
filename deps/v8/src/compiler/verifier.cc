// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/verifier.h"

#include "src/compiler/generic-algorithm.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/generic-node.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"

namespace v8 {
namespace internal {
namespace compiler {


static bool IsDefUseChainLinkPresent(Node* def, Node* use) {
  Node::Uses uses = def->uses();
  for (Node::Uses::iterator it = uses.begin(); it != uses.end(); ++it) {
    if (*it == use) return true;
  }
  return false;
}


static bool IsUseDefChainLinkPresent(Node* def, Node* use) {
  Node::Inputs inputs = use->inputs();
  for (Node::Inputs::iterator it = inputs.begin(); it != inputs.end(); ++it) {
    if (*it == def) return true;
  }
  return false;
}


class Verifier::Visitor : public NullNodeVisitor {
 public:
  explicit Visitor(Zone* zone)
      : reached_from_start(NodeSet::key_compare(),
                           NodeSet::allocator_type(zone)),
        reached_from_end(NodeSet::key_compare(),
                         NodeSet::allocator_type(zone)) {}

  // Fulfills the PreNodeCallback interface.
  GenericGraphVisit::Control Pre(Node* node);

  bool from_start;
  NodeSet reached_from_start;
  NodeSet reached_from_end;
};


GenericGraphVisit::Control Verifier::Visitor::Pre(Node* node) {
  int value_count = OperatorProperties::GetValueInputCount(node->op());
  int context_count = OperatorProperties::GetContextInputCount(node->op());
  int effect_count = OperatorProperties::GetEffectInputCount(node->op());
  int control_count = OperatorProperties::GetControlInputCount(node->op());

  // Verify number of inputs matches up.
  int input_count = value_count + context_count + effect_count + control_count;
  CHECK_EQ(input_count, node->InputCount());

  // Verify all value inputs actually produce a value.
  for (int i = 0; i < value_count; ++i) {
    Node* value = NodeProperties::GetValueInput(node, i);
    CHECK(OperatorProperties::HasValueOutput(value->op()));
    CHECK(IsDefUseChainLinkPresent(value, node));
    CHECK(IsUseDefChainLinkPresent(value, node));
  }

  // Verify all context inputs are value nodes.
  for (int i = 0; i < context_count; ++i) {
    Node* context = NodeProperties::GetContextInput(node);
    CHECK(OperatorProperties::HasValueOutput(context->op()));
    CHECK(IsDefUseChainLinkPresent(context, node));
    CHECK(IsUseDefChainLinkPresent(context, node));
  }

  // Verify all effect inputs actually have an effect.
  for (int i = 0; i < effect_count; ++i) {
    Node* effect = NodeProperties::GetEffectInput(node);
    CHECK(OperatorProperties::HasEffectOutput(effect->op()));
    CHECK(IsDefUseChainLinkPresent(effect, node));
    CHECK(IsUseDefChainLinkPresent(effect, node));
  }

  // Verify all control inputs are control nodes.
  for (int i = 0; i < control_count; ++i) {
    Node* control = NodeProperties::GetControlInput(node, i);
    CHECK(OperatorProperties::HasControlOutput(control->op()));
    CHECK(IsDefUseChainLinkPresent(control, node));
    CHECK(IsUseDefChainLinkPresent(control, node));
  }

  // Verify all successors are projections if multiple value outputs exist.
  if (OperatorProperties::GetValueOutputCount(node->op()) > 1) {
    Node::Uses uses = node->uses();
    for (Node::Uses::iterator it = uses.begin(); it != uses.end(); ++it) {
      CHECK(!NodeProperties::IsValueEdge(it.edge()) ||
            (*it)->opcode() == IrOpcode::kProjection ||
            (*it)->opcode() == IrOpcode::kParameter);
    }
  }

  switch (node->opcode()) {
    case IrOpcode::kStart:
      // Start has no inputs.
      CHECK_EQ(0, input_count);
      break;
    case IrOpcode::kEnd:
      // End has no outputs.
      CHECK(!OperatorProperties::HasValueOutput(node->op()));
      CHECK(!OperatorProperties::HasEffectOutput(node->op()));
      CHECK(!OperatorProperties::HasControlOutput(node->op()));
      break;
    case IrOpcode::kDead:
      // Dead is never connected to the graph.
      UNREACHABLE();
    case IrOpcode::kBranch: {
      // Branch uses are IfTrue and IfFalse.
      Node::Uses uses = node->uses();
      bool got_true = false, got_false = false;
      for (Node::Uses::iterator it = uses.begin(); it != uses.end(); ++it) {
        CHECK(((*it)->opcode() == IrOpcode::kIfTrue && !got_true) ||
              ((*it)->opcode() == IrOpcode::kIfFalse && !got_false));
        if ((*it)->opcode() == IrOpcode::kIfTrue) got_true = true;
        if ((*it)->opcode() == IrOpcode::kIfFalse) got_false = true;
      }
      // TODO(rossberg): Currently fails for various tests.
      // CHECK(got_true && got_false);
      break;
    }
    case IrOpcode::kIfTrue:
    case IrOpcode::kIfFalse:
      CHECK_EQ(IrOpcode::kBranch,
               NodeProperties::GetControlInput(node, 0)->opcode());
      break;
    case IrOpcode::kLoop:
    case IrOpcode::kMerge:
      break;
    case IrOpcode::kReturn:
      // TODO(rossberg): check successor is End
      break;
    case IrOpcode::kThrow:
      // TODO(rossberg): what are the constraints on these?
      break;
    case IrOpcode::kParameter: {
      // Parameters have the start node as inputs.
      CHECK_EQ(1, input_count);
      CHECK_EQ(IrOpcode::kStart,
               NodeProperties::GetValueInput(node, 0)->opcode());
      // Parameter has an input that produces enough values.
      int index = static_cast<Operator1<int>*>(node->op())->parameter();
      Node* input = NodeProperties::GetValueInput(node, 0);
      // Currently, parameter indices start at -1 instead of 0.
      CHECK_GT(OperatorProperties::GetValueOutputCount(input->op()), index + 1);
      break;
    }
    case IrOpcode::kInt32Constant:
    case IrOpcode::kInt64Constant:
    case IrOpcode::kFloat64Constant:
    case IrOpcode::kExternalConstant:
    case IrOpcode::kNumberConstant:
    case IrOpcode::kHeapConstant:
      // Constants have no inputs.
      CHECK_EQ(0, input_count);
      break;
    case IrOpcode::kPhi: {
      // Phi input count matches parent control node.
      CHECK_EQ(1, control_count);
      Node* control = NodeProperties::GetControlInput(node, 0);
      CHECK_EQ(value_count,
               OperatorProperties::GetControlInputCount(control->op()));
      break;
    }
    case IrOpcode::kEffectPhi: {
      // EffectPhi input count matches parent control node.
      CHECK_EQ(1, control_count);
      Node* control = NodeProperties::GetControlInput(node, 0);
      CHECK_EQ(effect_count,
               OperatorProperties::GetControlInputCount(control->op()));
      break;
    }
    case IrOpcode::kLazyDeoptimization:
      // TODO(jarin): what are the constraints on these?
      break;
    case IrOpcode::kDeoptimize:
      // TODO(jarin): what are the constraints on these?
      break;
    case IrOpcode::kFrameState:
      // TODO(jarin): what are the constraints on these?
      break;
    case IrOpcode::kCall:
      // TODO(rossberg): what are the constraints on these?
      break;
    case IrOpcode::kContinuation:
      // TODO(jarin): what are the constraints on these?
      break;
    case IrOpcode::kProjection: {
      // Projection has an input that produces enough values.
      int index = static_cast<Operator1<int>*>(node->op())->parameter();
      Node* input = NodeProperties::GetValueInput(node, 0);
      CHECK_GT(OperatorProperties::GetValueOutputCount(input->op()), index);
      break;
    }
    default:
      // TODO(rossberg): Check other node kinds.
      break;
  }

  if (from_start) {
    reached_from_start.insert(node);
  } else {
    reached_from_end.insert(node);
  }

  return GenericGraphVisit::CONTINUE;
}


void Verifier::Run(Graph* graph) {
  Visitor visitor(graph->zone());

  CHECK_NE(NULL, graph->start());
  visitor.from_start = true;
  graph->VisitNodeUsesFromStart(&visitor);
  CHECK_NE(NULL, graph->end());
  visitor.from_start = false;
  graph->VisitNodeInputsFromEnd(&visitor);

  // All control nodes reachable from end are reachable from start.
  for (NodeSet::iterator it = visitor.reached_from_end.begin();
       it != visitor.reached_from_end.end(); ++it) {
    CHECK(!NodeProperties::IsControl(*it) ||
          visitor.reached_from_start.count(*it));
  }
}
}
}
}  // namespace v8::internal::compiler

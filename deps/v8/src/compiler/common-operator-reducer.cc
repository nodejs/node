// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator-reducer.h"

#include <algorithm>

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

Decision DecideCondition(JSHeapBroker* broker, Node* const cond) {
  switch (cond->opcode()) {
    case IrOpcode::kInt32Constant: {
      Int32Matcher mcond(cond);
      return mcond.Value() ? Decision::kTrue : Decision::kFalse;
    }
    case IrOpcode::kHeapConstant: {
      HeapObjectMatcher mcond(cond);
      return mcond.Ref(broker).BooleanValue() ? Decision::kTrue
                                              : Decision::kFalse;
    }
    default:
      return Decision::kUnknown;
  }
}

}  // namespace

CommonOperatorReducer::CommonOperatorReducer(Editor* editor, Graph* graph,
                                             JSHeapBroker* js_heap_broker,
                                             CommonOperatorBuilder* common,
                                             MachineOperatorBuilder* machine,
                                             Zone* temp_zone)
    : AdvancedReducer(editor),
      graph_(graph),
      js_heap_broker_(js_heap_broker),
      common_(common),
      machine_(machine),
      dead_(graph->NewNode(common->Dead())),
      zone_(temp_zone) {
  NodeProperties::SetType(dead_, Type::None());
}

Reduction CommonOperatorReducer::Reduce(Node* node) {
  DisallowHeapAccess no_heap_access;
  switch (node->opcode()) {
    case IrOpcode::kBranch:
      return ReduceBranch(node);
    case IrOpcode::kDeoptimizeIf:
    case IrOpcode::kDeoptimizeUnless:
      return ReduceDeoptimizeConditional(node);
    case IrOpcode::kMerge:
      return ReduceMerge(node);
    case IrOpcode::kEffectPhi:
      return ReduceEffectPhi(node);
    case IrOpcode::kPhi:
      return ReducePhi(node);
    case IrOpcode::kReturn:
      return ReduceReturn(node);
    case IrOpcode::kSelect:
      return ReduceSelect(node);
    case IrOpcode::kSwitch:
      return ReduceSwitch(node);
    default:
      break;
  }
  return NoChange();
}


Reduction CommonOperatorReducer::ReduceBranch(Node* node) {
  DCHECK_EQ(IrOpcode::kBranch, node->opcode());
  Node* const cond = node->InputAt(0);
  // Swap IfTrue/IfFalse on {branch} if {cond} is a BooleanNot and use the input
  // to BooleanNot as new condition for {branch}. Note we assume that {cond} was
  // already properly optimized before we get here (as guaranteed by the graph
  // reduction logic). The same applies if {cond} is a Select acting as boolean
  // not (i.e. true being returned in the false case and vice versa).
  if (cond->opcode() == IrOpcode::kBooleanNot ||
      (cond->opcode() == IrOpcode::kSelect &&
       DecideCondition(js_heap_broker(), cond->InputAt(1)) ==
           Decision::kFalse &&
       DecideCondition(js_heap_broker(), cond->InputAt(2)) ==
           Decision::kTrue)) {
    for (Node* const use : node->uses()) {
      switch (use->opcode()) {
        case IrOpcode::kIfTrue:
          NodeProperties::ChangeOp(use, common()->IfFalse());
          break;
        case IrOpcode::kIfFalse:
          NodeProperties::ChangeOp(use, common()->IfTrue());
          break;
        default:
          UNREACHABLE();
      }
    }
    // Update the condition of {branch}. No need to mark the uses for revisit,
    // since we tell the graph reducer that the {branch} was changed and the
    // graph reduction logic will ensure that the uses are revisited properly.
    node->ReplaceInput(0, cond->InputAt(0));
    // Negate the hint for {branch}.
    NodeProperties::ChangeOp(
        node, common()->Branch(NegateBranchHint(BranchHintOf(node->op()))));
    return Changed(node);
  }
  Decision const decision = DecideCondition(js_heap_broker(), cond);
  if (decision == Decision::kUnknown) return NoChange();
  Node* const control = node->InputAt(1);
  for (Node* const use : node->uses()) {
    switch (use->opcode()) {
      case IrOpcode::kIfTrue:
        Replace(use, (decision == Decision::kTrue) ? control : dead());
        break;
      case IrOpcode::kIfFalse:
        Replace(use, (decision == Decision::kFalse) ? control : dead());
        break;
      default:
        UNREACHABLE();
    }
  }
  return Replace(dead());
}

Reduction CommonOperatorReducer::ReduceDeoptimizeConditional(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kDeoptimizeIf ||
         node->opcode() == IrOpcode::kDeoptimizeUnless);
  bool condition_is_true = node->opcode() == IrOpcode::kDeoptimizeUnless;
  DeoptimizeParameters p = DeoptimizeParametersOf(node->op());
  Node* condition = NodeProperties::GetValueInput(node, 0);
  Node* frame_state = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  // Swap DeoptimizeIf/DeoptimizeUnless on {node} if {cond} is a BooleaNot
  // and use the input to BooleanNot as new condition for {node}.  Note we
  // assume that {cond} was already properly optimized before we get here
  // (as guaranteed by the graph reduction logic).
  if (condition->opcode() == IrOpcode::kBooleanNot) {
    NodeProperties::ReplaceValueInput(node, condition->InputAt(0), 0);
    NodeProperties::ChangeOp(
        node,
        condition_is_true
            ? common()->DeoptimizeIf(p.kind(), p.reason(), p.feedback())
            : common()->DeoptimizeUnless(p.kind(), p.reason(), p.feedback()));
    return Changed(node);
  }
  Decision const decision = DecideCondition(js_heap_broker(), condition);
  if (decision == Decision::kUnknown) return NoChange();
  if (condition_is_true == (decision == Decision::kTrue)) {
    ReplaceWithValue(node, dead(), effect, control);
  } else {
    control = graph()->NewNode(
        common()->Deoptimize(p.kind(), p.reason(), p.feedback()), frame_state,
        effect, control);
    // TODO(bmeurer): This should be on the AdvancedReducer somehow.
    NodeProperties::MergeControlToEnd(graph(), common(), control);
    Revisit(graph()->end());
  }
  return Replace(dead());
}

Reduction CommonOperatorReducer::ReduceMerge(Node* node) {
  DCHECK_EQ(IrOpcode::kMerge, node->opcode());
  //
  // Check if this is a merge that belongs to an unused diamond, which means
  // that:
  //
  //  a) the {Merge} has no {Phi} or {EffectPhi} uses, and
  //  b) the {Merge} has two inputs, one {IfTrue} and one {IfFalse}, which are
  //     both owned by the Merge, and
  //  c) and the {IfTrue} and {IfFalse} nodes point to the same {Branch}.
  //
  if (node->InputCount() == 2) {
    for (Node* const use : node->uses()) {
      if (IrOpcode::IsPhiOpcode(use->opcode())) return NoChange();
    }
    Node* if_true = node->InputAt(0);
    Node* if_false = node->InputAt(1);
    if (if_true->opcode() != IrOpcode::kIfTrue) std::swap(if_true, if_false);
    if (if_true->opcode() == IrOpcode::kIfTrue &&
        if_false->opcode() == IrOpcode::kIfFalse &&
        if_true->InputAt(0) == if_false->InputAt(0) && if_true->OwnedBy(node) &&
        if_false->OwnedBy(node)) {
      Node* const branch = if_true->InputAt(0);
      DCHECK_EQ(IrOpcode::kBranch, branch->opcode());
      DCHECK(branch->OwnedBy(if_true, if_false));
      Node* const control = branch->InputAt(1);
      // Mark the {branch} as {Dead}.
      branch->TrimInputCount(0);
      NodeProperties::ChangeOp(branch, common()->Dead());
      return Replace(control);
    }
  }
  return NoChange();
}


Reduction CommonOperatorReducer::ReduceEffectPhi(Node* node) {
  DCHECK_EQ(IrOpcode::kEffectPhi, node->opcode());
  Node::Inputs inputs = node->inputs();
  int const effect_input_count = inputs.count() - 1;
  DCHECK_LE(1, effect_input_count);
  Node* const merge = inputs[effect_input_count];
  DCHECK(IrOpcode::IsMergeOpcode(merge->opcode()));
  DCHECK_EQ(effect_input_count, merge->InputCount());
  Node* const effect = inputs[0];
  DCHECK_NE(node, effect);
  for (int i = 1; i < effect_input_count; ++i) {
    Node* const input = inputs[i];
    if (input == node) {
      // Ignore redundant inputs.
      DCHECK_EQ(IrOpcode::kLoop, merge->opcode());
      continue;
    }
    if (input != effect) return NoChange();
  }
  // We might now be able to further reduce the {merge} node.
  Revisit(merge);
  return Replace(effect);
}


Reduction CommonOperatorReducer::ReducePhi(Node* node) {
  DCHECK_EQ(IrOpcode::kPhi, node->opcode());
  Node::Inputs inputs = node->inputs();
  int const value_input_count = inputs.count() - 1;
  DCHECK_LE(1, value_input_count);
  Node* const merge = inputs[value_input_count];
  DCHECK(IrOpcode::IsMergeOpcode(merge->opcode()));
  DCHECK_EQ(value_input_count, merge->InputCount());
  if (value_input_count == 2) {
    Node* vtrue = inputs[0];
    Node* vfalse = inputs[1];
    Node::Inputs merge_inputs = merge->inputs();
    Node* if_true = merge_inputs[0];
    Node* if_false = merge_inputs[1];
    if (if_true->opcode() != IrOpcode::kIfTrue) {
      std::swap(if_true, if_false);
      std::swap(vtrue, vfalse);
    }
    if (if_true->opcode() == IrOpcode::kIfTrue &&
        if_false->opcode() == IrOpcode::kIfFalse &&
        if_true->InputAt(0) == if_false->InputAt(0)) {
      Node* const branch = if_true->InputAt(0);
      // Check that the branch is not dead already.
      if (branch->opcode() != IrOpcode::kBranch) return NoChange();
      Node* const cond = branch->InputAt(0);
      if (cond->opcode() == IrOpcode::kFloat32LessThan) {
        Float32BinopMatcher mcond(cond);
        if (mcond.left().Is(0.0) && mcond.right().Equals(vtrue) &&
            vfalse->opcode() == IrOpcode::kFloat32Sub) {
          Float32BinopMatcher mvfalse(vfalse);
          if (mvfalse.left().IsZero() && mvfalse.right().Equals(vtrue)) {
            // We might now be able to further reduce the {merge} node.
            Revisit(merge);
            return Change(node, machine()->Float32Abs(), vtrue);
          }
        }
      } else if (cond->opcode() == IrOpcode::kFloat64LessThan) {
        Float64BinopMatcher mcond(cond);
        if (mcond.left().Is(0.0) && mcond.right().Equals(vtrue) &&
            vfalse->opcode() == IrOpcode::kFloat64Sub) {
          Float64BinopMatcher mvfalse(vfalse);
          if (mvfalse.left().IsZero() && mvfalse.right().Equals(vtrue)) {
            // We might now be able to further reduce the {merge} node.
            Revisit(merge);
            return Change(node, machine()->Float64Abs(), vtrue);
          }
        }
      }
    }
  }
  Node* const value = inputs[0];
  DCHECK_NE(node, value);
  for (int i = 1; i < value_input_count; ++i) {
    Node* const input = inputs[i];
    if (input == node) {
      // Ignore redundant inputs.
      DCHECK_EQ(IrOpcode::kLoop, merge->opcode());
      continue;
    }
    if (input != value) return NoChange();
  }
  // We might now be able to further reduce the {merge} node.
  Revisit(merge);
  return Replace(value);
}

Reduction CommonOperatorReducer::ReduceReturn(Node* node) {
  DCHECK_EQ(IrOpcode::kReturn, node->opcode());
  Node* effect = NodeProperties::GetEffectInput(node);
  if (effect->opcode() == IrOpcode::kCheckpoint) {
    // Any {Return} node can never be used to insert a deoptimization point,
    // hence checkpoints can be cut out of the effect chain flowing into it.
    effect = NodeProperties::GetEffectInput(effect);
    NodeProperties::ReplaceEffectInput(node, effect);
    Reduction const reduction = ReduceReturn(node);
    return reduction.Changed() ? reduction : Changed(node);
  }
  // TODO(ahaas): Extend the reduction below to multiple return values.
  if (ValueInputCountOfReturn(node->op()) != 1) {
    return NoChange();
  }
  Node* pop_count = NodeProperties::GetValueInput(node, 0);
  Node* value = NodeProperties::GetValueInput(node, 1);
  Node* control = NodeProperties::GetControlInput(node);
  if (value->opcode() == IrOpcode::kPhi &&
      NodeProperties::GetControlInput(value) == control &&
      control->opcode() == IrOpcode::kMerge) {
    // This optimization pushes {Return} nodes through merges. It checks that
    // the return value is actually a {Phi} and the return control dependency
    // is the {Merge} to which the {Phi} belongs.

    // Value1 ... ValueN Control1 ... ControlN
    //   ^          ^       ^            ^
    //   |          |       |            |
    //   +----+-----+       +------+-----+
    //        |                    |
    //       Phi --------------> Merge
    //        ^                    ^
    //        |                    |
    //        |  +-----------------+
    //        |  |
    //       Return -----> Effect
    //         ^
    //         |
    //        End

    // Now the effect input to the {Return} node can be either an {EffectPhi}
    // hanging off the same {Merge}, or the {Merge} node is only connected to
    // the {Return} and the {Phi}, in which case we know that the effect input
    // must somehow dominate all merged branches.

    Node::Inputs control_inputs = control->inputs();
    Node::Inputs value_inputs = value->inputs();
    DCHECK_NE(0, control_inputs.count());
    DCHECK_EQ(control_inputs.count(), value_inputs.count() - 1);
    DCHECK_EQ(IrOpcode::kEnd, graph()->end()->opcode());
    DCHECK_NE(0, graph()->end()->InputCount());
    if (control->OwnedBy(node, value)) {
      for (int i = 0; i < control_inputs.count(); ++i) {
        // Create a new {Return} and connect it to {end}. We don't need to mark
        // {end} as revisit, because we mark {node} as {Dead} below, which was
        // previously connected to {end}, so we know for sure that at some point
        // the reducer logic will visit {end} again.
        Node* ret = graph()->NewNode(node->op(), pop_count, value_inputs[i],
                                     effect, control_inputs[i]);
        NodeProperties::MergeControlToEnd(graph(), common(), ret);
      }
      // Mark the Merge {control} and Return {node} as {dead}.
      Replace(control, dead());
      return Replace(dead());
    } else if (effect->opcode() == IrOpcode::kEffectPhi &&
               NodeProperties::GetControlInput(effect) == control) {
      Node::Inputs effect_inputs = effect->inputs();
      DCHECK_EQ(control_inputs.count(), effect_inputs.count() - 1);
      for (int i = 0; i < control_inputs.count(); ++i) {
        // Create a new {Return} and connect it to {end}. We don't need to mark
        // {end} as revisit, because we mark {node} as {Dead} below, which was
        // previously connected to {end}, so we know for sure that at some point
        // the reducer logic will visit {end} again.
        Node* ret = graph()->NewNode(node->op(), pop_count, value_inputs[i],
                                     effect_inputs[i], control_inputs[i]);
        NodeProperties::MergeControlToEnd(graph(), common(), ret);
      }
      // Mark the Merge {control} and Return {node} as {dead}.
      Replace(control, dead());
      return Replace(dead());
    }
  }
  return NoChange();
}

Reduction CommonOperatorReducer::ReduceSelect(Node* node) {
  DCHECK_EQ(IrOpcode::kSelect, node->opcode());
  Node* const cond = node->InputAt(0);
  Node* const vtrue = node->InputAt(1);
  Node* const vfalse = node->InputAt(2);
  if (vtrue == vfalse) return Replace(vtrue);
  switch (DecideCondition(js_heap_broker(), cond)) {
    case Decision::kTrue:
      return Replace(vtrue);
    case Decision::kFalse:
      return Replace(vfalse);
    case Decision::kUnknown:
      break;
  }
  switch (cond->opcode()) {
    case IrOpcode::kFloat32LessThan: {
      Float32BinopMatcher mcond(cond);
      if (mcond.left().Is(0.0) && mcond.right().Equals(vtrue) &&
          vfalse->opcode() == IrOpcode::kFloat32Sub) {
        Float32BinopMatcher mvfalse(vfalse);
        if (mvfalse.left().IsZero() && mvfalse.right().Equals(vtrue)) {
          return Change(node, machine()->Float32Abs(), vtrue);
        }
      }
      break;
    }
    case IrOpcode::kFloat64LessThan: {
      Float64BinopMatcher mcond(cond);
      if (mcond.left().Is(0.0) && mcond.right().Equals(vtrue) &&
          vfalse->opcode() == IrOpcode::kFloat64Sub) {
        Float64BinopMatcher mvfalse(vfalse);
        if (mvfalse.left().IsZero() && mvfalse.right().Equals(vtrue)) {
          return Change(node, machine()->Float64Abs(), vtrue);
        }
      }
      break;
    }
    default:
      break;
  }
  return NoChange();
}

Reduction CommonOperatorReducer::ReduceSwitch(Node* node) {
  DCHECK_EQ(IrOpcode::kSwitch, node->opcode());
  Node* const switched_value = node->InputAt(0);
  Node* const control = node->InputAt(1);

  // Attempt to constant match the switched value against the IfValue cases. If
  // no case matches, then use the IfDefault. We don't bother marking
  // non-matching cases as dead code (same for an unused IfDefault), because the
  // Switch itself will be marked as dead code.
  Int32Matcher mswitched(switched_value);
  if (mswitched.HasValue()) {
    bool matched = false;

    size_t const projection_count = node->op()->ControlOutputCount();
    Node** projections = zone_->NewArray<Node*>(projection_count);
    NodeProperties::CollectControlProjections(node, projections,
                                              projection_count);
    for (size_t i = 0; i < projection_count - 1; i++) {
      Node* if_value = projections[i];
      DCHECK_EQ(IrOpcode::kIfValue, if_value->opcode());
      const IfValueParameters& p = IfValueParametersOf(if_value->op());
      if (p.value() == mswitched.Value()) {
        matched = true;
        Replace(if_value, control);
        break;
      }
    }
    if (!matched) {
      Node* if_default = projections[projection_count - 1];
      DCHECK_EQ(IrOpcode::kIfDefault, if_default->opcode());
      Replace(if_default, control);
    }
    return Replace(dead());
  }
  return NoChange();
}

Reduction CommonOperatorReducer::Change(Node* node, Operator const* op,
                                        Node* a) {
  node->ReplaceInput(0, a);
  node->TrimInputCount(1);
  NodeProperties::ChangeOp(node, op);
  return Changed(node);
}


Reduction CommonOperatorReducer::Change(Node* node, Operator const* op, Node* a,
                                        Node* b) {
  node->ReplaceInput(0, a);
  node->ReplaceInput(1, b);
  node->TrimInputCount(2);
  NodeProperties::ChangeOp(node, op);
  return Changed(node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

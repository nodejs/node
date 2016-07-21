// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/effect-control-linearizer.h"

#include "src/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/schedule.h"

namespace v8 {
namespace internal {
namespace compiler {

EffectControlLinearizer::EffectControlLinearizer(JSGraph* js_graph,
                                                 Schedule* schedule,
                                                 Zone* temp_zone)
    : js_graph_(js_graph), schedule_(schedule), temp_zone_(temp_zone) {}

Graph* EffectControlLinearizer::graph() const { return js_graph_->graph(); }
CommonOperatorBuilder* EffectControlLinearizer::common() const {
  return js_graph_->common();
}
SimplifiedOperatorBuilder* EffectControlLinearizer::simplified() const {
  return js_graph_->simplified();
}
MachineOperatorBuilder* EffectControlLinearizer::machine() const {
  return js_graph_->machine();
}

namespace {

struct BlockEffectControlData {
  Node* current_effect = nullptr;  // New effect.
  Node* current_control = nullptr;  // New control.
};

// Effect phis that need to be updated after the first pass.
struct PendingEffectPhi {
  Node* effect_phi;
  BasicBlock* block;

  PendingEffectPhi(Node* effect_phi, BasicBlock* block)
      : effect_phi(effect_phi), block(block) {}
};

void UpdateEffectPhi(Node* node, BasicBlock* block,
                     ZoneVector<BlockEffectControlData>* block_effects) {
  // Update all inputs to an effect phi with the effects from the given
  // block->effect map.
  DCHECK_EQ(IrOpcode::kEffectPhi, node->opcode());
  DCHECK_EQ(node->op()->EffectInputCount(), block->PredecessorCount());
  for (int i = 0; i < node->op()->EffectInputCount(); i++) {
    Node* input = node->InputAt(i);
    BasicBlock* predecessor = block->PredecessorAt(static_cast<size_t>(i));
    Node* input_effect =
        (*block_effects)[predecessor->rpo_number()].current_effect;
    if (input != input_effect) {
      node->ReplaceInput(i, input_effect);
    }
  }
}

void UpdateBlockControl(BasicBlock* block,
                        ZoneVector<BlockEffectControlData>* block_effects) {
  Node* control = block->NodeAt(0);
  DCHECK(NodeProperties::IsControl(control));

  // Do not rewire the end node.
  if (control->opcode() == IrOpcode::kEnd) return;

  // Update all inputs to the given control node with the correct control.
  DCHECK_EQ(control->op()->ControlInputCount(), block->PredecessorCount());
  for (int i = 0; i < control->op()->ControlInputCount(); i++) {
    Node* input = NodeProperties::GetControlInput(control, i);
    BasicBlock* predecessor = block->PredecessorAt(static_cast<size_t>(i));
    Node* input_control =
        (*block_effects)[predecessor->rpo_number()].current_control;
    if (input != input_control) {
      NodeProperties::ReplaceControlInput(control, input_control, i);
    }
  }
}

bool HasIncomingBackEdges(BasicBlock* block) {
  for (BasicBlock* pred : block->predecessors()) {
    if (pred->rpo_number() >= block->rpo_number()) {
      return true;
    }
  }
  return false;
}

void RemoveRegionNode(Node* node) {
  DCHECK(IrOpcode::kFinishRegion == node->opcode() ||
         IrOpcode::kBeginRegion == node->opcode());
  // Update the value/context uses to the value input of the finish node and
  // the effect uses to the effect input.
  for (Edge edge : node->use_edges()) {
    DCHECK(!edge.from()->IsDead());
    if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(NodeProperties::GetEffectInput(node));
    } else {
      DCHECK(!NodeProperties::IsControlEdge(edge));
      DCHECK(!NodeProperties::IsFrameStateEdge(edge));
      edge.UpdateTo(node->InputAt(0));
    }
  }
  node->Kill();
}

}  // namespace

void EffectControlLinearizer::Run() {
  ZoneVector<BlockEffectControlData> block_effects(temp_zone());
  ZoneVector<PendingEffectPhi> pending_effect_phis(temp_zone());
  ZoneVector<BasicBlock*> pending_block_controls(temp_zone());
  block_effects.resize(schedule()->RpoBlockCount());
  NodeVector inputs_buffer(temp_zone());

  for (BasicBlock* block : *(schedule()->rpo_order())) {
    size_t instr = 0;

    // The control node should be the first.
    Node* control = block->NodeAt(instr);
    DCHECK(NodeProperties::IsControl(control));
    // Update the control inputs.
    if (HasIncomingBackEdges(block)) {
      // If there are back edges, we need to update later because we have not
      // computed the control yet. This should only happen for loops.
      DCHECK_EQ(IrOpcode::kLoop, control->opcode());
      pending_block_controls.push_back(block);
    } else {
      // If there are no back edges, we can update now.
      UpdateBlockControl(block, &block_effects);
    }
    instr++;

    // Iterate over the phis and update the effect phis.
    Node* effect = nullptr;
    Node* terminate = nullptr;
    for (; instr < block->NodeCount(); instr++) {
      Node* node = block->NodeAt(instr);
      // Only go through the phis and effect phis.
      if (node->opcode() == IrOpcode::kEffectPhi) {
        // There should be at most one effect phi in a block.
        DCHECK_NULL(effect);
        // IfException blocks should not have effect phis.
        DCHECK_NE(IrOpcode::kIfException, control->opcode());
        effect = node;

        // Make sure we update the inputs to the incoming blocks' effects.
        if (HasIncomingBackEdges(block)) {
          // In case of loops, we do not update the effect phi immediately
          // because the back predecessor has not been handled yet. We just
          // record the effect phi for later processing.
          pending_effect_phis.push_back(PendingEffectPhi(node, block));
        } else {
          UpdateEffectPhi(node, block, &block_effects);
        }
      } else if (node->opcode() == IrOpcode::kPhi) {
        // Just skip phis.
      } else if (node->opcode() == IrOpcode::kTerminate) {
        DCHECK(terminate == nullptr);
        terminate = node;
      } else {
        break;
      }
    }

    if (effect == nullptr) {
      // There was no effect phi.
      DCHECK(!HasIncomingBackEdges(block));
      if (block == schedule()->start()) {
        // Start block => effect is start.
        DCHECK_EQ(graph()->start(), control);
        effect = graph()->start();
      } else if (control->opcode() == IrOpcode::kEnd) {
        // End block is just a dummy, no effect needed.
        DCHECK_EQ(BasicBlock::kNone, block->control());
        DCHECK_EQ(1u, block->size());
        effect = nullptr;
      } else {
        // If all the predecessors have the same effect, we can use it
        // as our current effect.
        int rpo_number = block->PredecessorAt(0)->rpo_number();
        effect = block_effects[rpo_number].current_effect;
        for (size_t i = 1; i < block->PredecessorCount(); i++) {
          int rpo_number = block->PredecessorAt(i)->rpo_number();
          if (block_effects[rpo_number].current_effect != effect) {
            effect = nullptr;
            break;
          }
        }
        if (effect == nullptr) {
          DCHECK_NE(IrOpcode::kIfException, control->opcode());
          // The input blocks do not have the same effect. We have
          // to create an effect phi node.
          inputs_buffer.clear();
          inputs_buffer.resize(block->PredecessorCount(), graph()->start());
          inputs_buffer.push_back(control);
          effect = graph()->NewNode(
              common()->EffectPhi(static_cast<int>(block->PredecessorCount())),
              static_cast<int>(inputs_buffer.size()), &(inputs_buffer.front()));
          // Let us update the effect phi node later.
          pending_effect_phis.push_back(PendingEffectPhi(effect, block));
        } else if (control->opcode() == IrOpcode::kIfException) {
          // The IfException is connected into the effect chain, so we need
          // to update the effect here.
          NodeProperties::ReplaceEffectInput(control, effect);
          effect = control;
        }
      }
    }

    // Fixup the Terminate node.
    if (terminate != nullptr) {
      NodeProperties::ReplaceEffectInput(terminate, effect);
    }

    // Process the ordinary instructions.
    for (; instr < block->NodeCount(); instr++) {
      Node* node = block->NodeAt(instr);
      ProcessNode(node, &effect, &control);
    }

    switch (block->control()) {
      case BasicBlock::kGoto:
      case BasicBlock::kNone:
        break;

      case BasicBlock::kCall:
      case BasicBlock::kTailCall:
      case BasicBlock::kBranch:
      case BasicBlock::kSwitch:
      case BasicBlock::kReturn:
      case BasicBlock::kDeoptimize:
      case BasicBlock::kThrow:
        ProcessNode(block->control_input(), &effect, &control);
        break;
    }

    // Store the effect for later use.
    block_effects[block->rpo_number()].current_effect = effect;
    block_effects[block->rpo_number()].current_control = control;
  }

  // Update the incoming edges of the effect phis that could not be processed
  // during the first pass (because they could have incoming back edges).
  for (const PendingEffectPhi& pending_effect_phi : pending_effect_phis) {
    UpdateEffectPhi(pending_effect_phi.effect_phi, pending_effect_phi.block,
                    &block_effects);
  }
  for (BasicBlock* pending_block_control : pending_block_controls) {
    UpdateBlockControl(pending_block_control, &block_effects);
  }
}

namespace {

void TryScheduleCallIfSuccess(Node* node, Node** control) {
  // Schedule the call's IfSuccess node if there is no exception use.
  if (!NodeProperties::IsExceptionalCall(node)) {
    for (Edge edge : node->use_edges()) {
      if (NodeProperties::IsControlEdge(edge) &&
          edge.from()->opcode() == IrOpcode::kIfSuccess) {
        *control = edge.from();
      }
    }
  }
}

}  // namespace

void EffectControlLinearizer::ProcessNode(Node* node, Node** effect,
                                          Node** control) {
  // If the node needs to be wired into the effect/control chain, do this
  // here.
  if (TryWireInStateEffect(node, effect, control)) {
    return;
  }

  // Remove the end markers of 'atomic' allocation region because the
  // region should be wired-in now.
  if (node->opcode() == IrOpcode::kFinishRegion ||
      node->opcode() == IrOpcode::kBeginRegion) {
    // Update the value uses to the value input of the finish node and
    // the effect uses to the effect input.
    return RemoveRegionNode(node);
  }

  // Special treatment for CheckPoint nodes.
  // TODO(epertoso): Pickup the current frame state.
  if (node->opcode() == IrOpcode::kCheckPoint) {
    // Unlink the check point; effect uses will be updated to the incoming
    // effect that is passed.
    node->Kill();
    return;
  }

  if (node->opcode() == IrOpcode::kIfSuccess) {
    // We always schedule IfSuccess with its call, so skip it here.
    DCHECK_EQ(IrOpcode::kCall, node->InputAt(0)->opcode());
    // The IfSuccess node should not belong to an exceptional call node
    // because such IfSuccess nodes should only start a basic block (and
    // basic block start nodes are not handled in the ProcessNode method).
    DCHECK(!NodeProperties::IsExceptionalCall(node->InputAt(0)));
    return;
  }

  // If the node takes an effect, replace with the current one.
  if (node->op()->EffectInputCount() > 0) {
    DCHECK_EQ(1, node->op()->EffectInputCount());
    Node* input_effect = NodeProperties::GetEffectInput(node);

    if (input_effect != *effect) {
      NodeProperties::ReplaceEffectInput(node, *effect);
    }

    // If the node produces an effect, update our current effect. (However,
    // ignore new effect chains started with ValueEffect.)
    if (node->op()->EffectOutputCount() > 0) {
      DCHECK_EQ(1, node->op()->EffectOutputCount());
      *effect = node;
    }
  } else {
    // New effect chain is only started with a Start or ValueEffect node.
    DCHECK(node->op()->EffectOutputCount() == 0 ||
           node->opcode() == IrOpcode::kStart);
  }

  // Rewire control inputs.
  for (int i = 0; i < node->op()->ControlInputCount(); i++) {
    NodeProperties::ReplaceControlInput(node, *control, i);
  }
  // Update the current control and wire IfSuccess right after calls.
  if (node->op()->ControlOutputCount() > 0) {
    *control = node;
    if (node->opcode() == IrOpcode::kCall) {
      // Schedule the call's IfSuccess node (if there is no exception use).
      TryScheduleCallIfSuccess(node, control);
    }
  }
}

bool EffectControlLinearizer::TryWireInStateEffect(Node* node, Node** effect,
                                                   Node** control) {
  ValueEffectControl state(nullptr, nullptr, nullptr);
  switch (node->opcode()) {
    case IrOpcode::kTypeGuard:
      state = LowerTypeGuard(node, *effect, *control);
      break;
    case IrOpcode::kChangeBitToTagged:
      state = LowerChangeBitToTagged(node, *effect, *control);
      break;
    case IrOpcode::kChangeInt31ToTaggedSigned:
      state = LowerChangeInt31ToTaggedSigned(node, *effect, *control);
      break;
    case IrOpcode::kChangeInt32ToTagged:
      state = LowerChangeInt32ToTagged(node, *effect, *control);
      break;
    case IrOpcode::kChangeUint32ToTagged:
      state = LowerChangeUint32ToTagged(node, *effect, *control);
      break;
    case IrOpcode::kChangeFloat64ToTagged:
      state = LowerChangeFloat64ToTagged(node, *effect, *control);
      break;
    case IrOpcode::kChangeTaggedSignedToInt32:
      state = LowerChangeTaggedSignedToInt32(node, *effect, *control);
      break;
    case IrOpcode::kChangeTaggedToBit:
      state = LowerChangeTaggedToBit(node, *effect, *control);
      break;
    case IrOpcode::kChangeTaggedToInt32:
      state = LowerChangeTaggedToInt32(node, *effect, *control);
      break;
    case IrOpcode::kChangeTaggedToUint32:
      state = LowerChangeTaggedToUint32(node, *effect, *control);
      break;
    case IrOpcode::kChangeTaggedToFloat64:
      state = LowerChangeTaggedToFloat64(node, *effect, *control);
      break;
    case IrOpcode::kTruncateTaggedToWord32:
      state = LowerTruncateTaggedToWord32(node, *effect, *control);
      break;
    case IrOpcode::kObjectIsCallable:
      state = LowerObjectIsCallable(node, *effect, *control);
      break;
    case IrOpcode::kObjectIsNumber:
      state = LowerObjectIsNumber(node, *effect, *control);
      break;
    case IrOpcode::kObjectIsReceiver:
      state = LowerObjectIsReceiver(node, *effect, *control);
      break;
    case IrOpcode::kObjectIsSmi:
      state = LowerObjectIsSmi(node, *effect, *control);
      break;
    case IrOpcode::kObjectIsString:
      state = LowerObjectIsString(node, *effect, *control);
      break;
    case IrOpcode::kObjectIsUndetectable:
      state = LowerObjectIsUndetectable(node, *effect, *control);
      break;
    default:
      return false;
  }
  NodeProperties::ReplaceUses(node, state.value);
  *effect = state.effect;
  *control = state.control;
  return true;
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerTypeGuard(Node* node, Node* effect,
                                        Node* control) {
  Node* value = node->InputAt(0);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeFloat64ToTagged(Node* node, Node* effect,
                                                    Node* control) {
  Node* value = node->InputAt(0);

  Node* value32 = graph()->NewNode(machine()->RoundFloat64ToInt32(), value);
  Node* check_same = graph()->NewNode(
      machine()->Float64Equal(), value,
      graph()->NewNode(machine()->ChangeInt32ToFloat64(), value32));
  Node* branch_same = graph()->NewNode(common()->Branch(), check_same, control);

  Node* if_smi = graph()->NewNode(common()->IfTrue(), branch_same);
  Node* vsmi;
  Node* if_box = graph()->NewNode(common()->IfFalse(), branch_same);

  // Check if {value} is -0.
  Node* check_zero = graph()->NewNode(machine()->Word32Equal(), value32,
                                      jsgraph()->Int32Constant(0));
  Node* branch_zero = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check_zero, if_smi);

  Node* if_zero = graph()->NewNode(common()->IfTrue(), branch_zero);
  Node* if_notzero = graph()->NewNode(common()->IfFalse(), branch_zero);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  Node* check_negative = graph()->NewNode(
      machine()->Int32LessThan(),
      graph()->NewNode(machine()->Float64ExtractHighWord32(), value),
      jsgraph()->Int32Constant(0));
  Node* branch_negative = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                           check_negative, if_zero);

  Node* if_negative = graph()->NewNode(common()->IfTrue(), branch_negative);
  Node* if_notnegative = graph()->NewNode(common()->IfFalse(), branch_negative);

  // We need to create a box for negative 0.
  if_smi = graph()->NewNode(common()->Merge(2), if_notzero, if_notnegative);
  if_box = graph()->NewNode(common()->Merge(2), if_box, if_negative);

  // On 64-bit machines we can just wrap the 32-bit integer in a smi, for 32-bit
  // machines we need to deal with potential overflow and fallback to boxing.
  if (machine()->Is64()) {
    vsmi = ChangeInt32ToSmi(value32);
  } else {
    Node* smi_tag =
        graph()->NewNode(machine()->Int32AddWithOverflow(), value32, value32);

    Node* check_ovf = graph()->NewNode(common()->Projection(1), smi_tag);
    Node* branch_ovf = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                        check_ovf, if_smi);

    Node* if_ovf = graph()->NewNode(common()->IfTrue(), branch_ovf);
    if_box = graph()->NewNode(common()->Merge(2), if_ovf, if_box);

    if_smi = graph()->NewNode(common()->IfFalse(), branch_ovf);
    vsmi = graph()->NewNode(common()->Projection(0), smi_tag);
  }

  // Allocate the box for the {value}.
  ValueEffectControl box = AllocateHeapNumberWithValue(value, effect, if_box);

  control = graph()->NewNode(common()->Merge(2), if_smi, box.control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vsmi, box.value, control);
  effect =
      graph()->NewNode(common()->EffectPhi(2), effect, box.effect, control);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeBitToTagged(Node* node, Node* effect,
                                                Node* control) {
  Node* value = node->InputAt(0);

  Node* branch = graph()->NewNode(common()->Branch(), value, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* vtrue = jsgraph()->TrueConstant();

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = jsgraph()->FalseConstant();

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeInt31ToTaggedSigned(Node* node,
                                                        Node* effect,
                                                        Node* control) {
  Node* value = node->InputAt(0);
  value = ChangeInt32ToSmi(value);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeInt32ToTagged(Node* node, Node* effect,
                                                  Node* control) {
  Node* value = node->InputAt(0);

  if (machine()->Is64()) {
    return ValueEffectControl(ChangeInt32ToSmi(value), effect, control);
  }

  Node* add = graph()->NewNode(machine()->Int32AddWithOverflow(), value, value);

  Node* ovf = graph()->NewNode(common()->Projection(1), add);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), ovf, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  ValueEffectControl alloc =
      AllocateHeapNumberWithValue(ChangeInt32ToFloat64(value), effect, if_true);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = graph()->NewNode(common()->Projection(0), add);

  Node* merge = graph()->NewNode(common()->Merge(2), alloc.control, if_false);
  Node* phi = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                               alloc.value, vfalse, merge);
  Node* ephi =
      graph()->NewNode(common()->EffectPhi(2), alloc.effect, effect, merge);

  return ValueEffectControl(phi, ephi, merge);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeUint32ToTagged(Node* node, Node* effect,
                                                   Node* control) {
  Node* value = node->InputAt(0);

  Node* check = graph()->NewNode(machine()->Uint32LessThanOrEqual(), value,
                                 SmiMaxValueConstant());
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* vtrue = ChangeUint32ToSmi(value);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  ValueEffectControl alloc = AllocateHeapNumberWithValue(
      ChangeUint32ToFloat64(value), effect, if_false);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, alloc.control);
  Node* phi = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                               vtrue, alloc.value, merge);
  Node* ephi =
      graph()->NewNode(common()->EffectPhi(2), effect, alloc.effect, merge);

  return ValueEffectControl(phi, ephi, merge);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeTaggedSignedToInt32(Node* node,
                                                        Node* effect,
                                                        Node* control) {
  Node* value = node->InputAt(0);
  value = ChangeSmiToInt32(value);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeTaggedToBit(Node* node, Node* effect,
                                                Node* control) {
  Node* value = node->InputAt(0);
  value = graph()->NewNode(machine()->WordEqual(), value,
                           jsgraph()->TrueConstant());
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeTaggedToInt32(Node* node, Node* effect,
                                                  Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
    vfalse = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
        efalse, if_false);
    vfalse = graph()->NewNode(machine()->ChangeFloat64ToInt32(), vfalse);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeTaggedToUint32(Node* node, Node* effect,
                                                   Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
    vfalse = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
        efalse, if_false);
    vfalse = graph()->NewNode(machine()->ChangeFloat64ToUint32(), vfalse);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeTaggedToFloat64(Node* node, Node* effect,
                                                    Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue;
  {
    vtrue = ChangeSmiToInt32(value);
    vtrue = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue);
  }

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
    vfalse = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
        efalse, if_false);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerTruncateTaggedToWord32(Node* node, Node* effect,
                                                     Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
    vfalse = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
        efalse, if_false);
    vfalse = graph()->NewNode(machine()->TruncateFloat64ToWord32(), vfalse);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsCallable(Node* node, Node* effect,
                                               Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->Int32Constant(0);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    Node* value_bit_field = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForMapBitField()), value_map,
        efalse, if_false);
    vfalse = graph()->NewNode(
        machine()->Word32Equal(),
        jsgraph()->Int32Constant(1 << Map::kIsCallable),
        graph()->NewNode(
            machine()->Word32And(), value_bit_field,
            jsgraph()->Int32Constant((1 << Map::kIsCallable) |
                                     (1 << Map::kIsUndetectable))));
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, 2), vtrue,
                           vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsNumber(Node* node, Node* effect,
                                             Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch = graph()->NewNode(common()->Branch(), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->Int32Constant(1);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    vfalse = graph()->NewNode(machine()->WordEqual(), value_map,
                              jsgraph()->HeapNumberMapConstant());
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, 2), vtrue,
                           vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsReceiver(Node* node, Node* effect,
                                               Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->Int32Constant(0);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    Node* value_instance_type = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForMapInstanceType()), value_map,
        efalse, if_false);
    vfalse = graph()->NewNode(machine()->Uint32LessThanOrEqual(),
                              jsgraph()->Uint32Constant(FIRST_JS_RECEIVER_TYPE),
                              value_instance_type);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, 2), vtrue,
                           vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsSmi(Node* node, Node* effect,
                                          Node* control) {
  Node* value = node->InputAt(0);
  value = ObjectIsSmi(value);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsString(Node* node, Node* effect,
                                             Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->Int32Constant(0);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    Node* value_instance_type = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForMapInstanceType()), value_map,
        efalse, if_false);
    vfalse = graph()->NewNode(machine()->Uint32LessThan(), value_instance_type,
                              jsgraph()->Uint32Constant(FIRST_NONSTRING_TYPE));
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, 2), vtrue,
                           vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsUndetectable(Node* node, Node* effect,
                                                   Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->Int32Constant(0);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    Node* value_bit_field = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForMapBitField()), value_map,
        efalse, if_false);
    vfalse = graph()->NewNode(
        machine()->Word32Equal(),
        graph()->NewNode(
            machine()->Word32Equal(), jsgraph()->Int32Constant(0),
            graph()->NewNode(
                machine()->Word32And(), value_bit_field,
                jsgraph()->Int32Constant(1 << Map::kIsUndetectable))),
        jsgraph()->Int32Constant(0));
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, 2), vtrue,
                           vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::AllocateHeapNumberWithValue(Node* value, Node* effect,
                                                     Node* control) {
  Node* result = effect = graph()->NewNode(
      simplified()->Allocate(NOT_TENURED),
      jsgraph()->Int32Constant(HeapNumber::kSize), effect, control);
  effect = graph()->NewNode(simplified()->StoreField(AccessBuilder::ForMap()),
                            result, jsgraph()->HeapNumberMapConstant(), effect,
                            control);
  effect = graph()->NewNode(
      simplified()->StoreField(AccessBuilder::ForHeapNumberValue()), result,
      value, effect, control);
  return ValueEffectControl(result, effect, control);
}

Node* EffectControlLinearizer::ChangeInt32ToSmi(Node* value) {
  if (machine()->Is64()) {
    value = graph()->NewNode(machine()->ChangeInt32ToInt64(), value);
  }
  return graph()->NewNode(machine()->WordShl(), value, SmiShiftBitsConstant());
}

Node* EffectControlLinearizer::ChangeUint32ToSmi(Node* value) {
  if (machine()->Is64()) {
    value = graph()->NewNode(machine()->ChangeUint32ToUint64(), value);
  }
  return graph()->NewNode(machine()->WordShl(), value, SmiShiftBitsConstant());
}

Node* EffectControlLinearizer::ChangeInt32ToFloat64(Node* value) {
  return graph()->NewNode(machine()->ChangeInt32ToFloat64(), value);
}

Node* EffectControlLinearizer::ChangeUint32ToFloat64(Node* value) {
  return graph()->NewNode(machine()->ChangeUint32ToFloat64(), value);
}

Node* EffectControlLinearizer::ChangeSmiToInt32(Node* value) {
  value = graph()->NewNode(machine()->WordSar(), value, SmiShiftBitsConstant());
  if (machine()->Is64()) {
    value = graph()->NewNode(machine()->TruncateInt64ToInt32(), value);
  }
  return value;
}

Node* EffectControlLinearizer::ObjectIsSmi(Node* value) {
  return graph()->NewNode(
      machine()->WordEqual(),
      graph()->NewNode(machine()->WordAnd(), value,
                       jsgraph()->IntPtrConstant(kSmiTagMask)),
      jsgraph()->IntPtrConstant(kSmiTag));
}

Node* EffectControlLinearizer::SmiMaxValueConstant() {
  return jsgraph()->Int32Constant(Smi::kMaxValue);
}

Node* EffectControlLinearizer::SmiShiftBitsConstant() {
  return jsgraph()->IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

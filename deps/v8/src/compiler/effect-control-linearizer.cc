// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/effect-control-linearizer.h"

#include "src/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
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
  Node* current_effect = nullptr;       // New effect.
  Node* current_control = nullptr;      // New control.
  Node* current_frame_state = nullptr;  // New frame state.
};

class BlockEffectControlMap {
 public:
  explicit BlockEffectControlMap(Zone* temp_zone) : map_(temp_zone) {}

  BlockEffectControlData& For(BasicBlock* from, BasicBlock* to) {
    return map_[std::make_pair(from->rpo_number(), to->rpo_number())];
  }

  const BlockEffectControlData& For(BasicBlock* from, BasicBlock* to) const {
    return map_.at(std::make_pair(from->rpo_number(), to->rpo_number()));
  }

 private:
  typedef std::pair<int32_t, int32_t> Key;
  typedef ZoneMap<Key, BlockEffectControlData> Map;

  Map map_;
};

// Effect phis that need to be updated after the first pass.
struct PendingEffectPhi {
  Node* effect_phi;
  BasicBlock* block;

  PendingEffectPhi(Node* effect_phi, BasicBlock* block)
      : effect_phi(effect_phi), block(block) {}
};

void UpdateEffectPhi(Node* node, BasicBlock* block,
                     BlockEffectControlMap* block_effects) {
  // Update all inputs to an effect phi with the effects from the given
  // block->effect map.
  DCHECK_EQ(IrOpcode::kEffectPhi, node->opcode());
  DCHECK_EQ(node->op()->EffectInputCount(), block->PredecessorCount());
  for (int i = 0; i < node->op()->EffectInputCount(); i++) {
    Node* input = node->InputAt(i);
    BasicBlock* predecessor = block->PredecessorAt(static_cast<size_t>(i));
    const BlockEffectControlData& block_effect =
        block_effects->For(predecessor, block);
    if (input != block_effect.current_effect) {
      node->ReplaceInput(i, block_effect.current_effect);
    }
  }
}

void UpdateBlockControl(BasicBlock* block,
                        BlockEffectControlMap* block_effects) {
  Node* control = block->NodeAt(0);
  DCHECK(NodeProperties::IsControl(control));

  // Do not rewire the end node.
  if (control->opcode() == IrOpcode::kEnd) return;

  // Update all inputs to the given control node with the correct control.
  DCHECK(control->opcode() == IrOpcode::kMerge ||
         control->op()->ControlInputCount() == block->PredecessorCount());
  if (control->op()->ControlInputCount() != block->PredecessorCount()) {
    return;  // We already re-wired the control inputs of this node.
  }
  for (int i = 0; i < control->op()->ControlInputCount(); i++) {
    Node* input = NodeProperties::GetControlInput(control, i);
    BasicBlock* predecessor = block->PredecessorAt(static_cast<size_t>(i));
    const BlockEffectControlData& block_effect =
        block_effects->For(predecessor, block);
    if (input != block_effect.current_control) {
      NodeProperties::ReplaceControlInput(control, block_effect.current_control,
                                          i);
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

void TryCloneBranch(Node* node, BasicBlock* block, Graph* graph,
                    CommonOperatorBuilder* common,
                    BlockEffectControlMap* block_effects) {
  DCHECK_EQ(IrOpcode::kBranch, node->opcode());

  // This optimization is a special case of (super)block cloning. It takes an
  // input graph as shown below and clones the Branch node for every predecessor
  // to the Merge, essentially removing the Merge completely. This avoids
  // materializing the bit for the Phi and may offer potential for further
  // branch folding optimizations (i.e. because one or more inputs to the Phi is
  // a constant). Note that there may be more Phi nodes hanging off the Merge,
  // but we can only a certain subset of them currently (actually only Phi and
  // EffectPhi nodes whose uses have either the IfTrue or IfFalse as control
  // input).

  //   Control1 ... ControlN
  //      ^            ^
  //      |            |   Cond1 ... CondN
  //      +----+  +----+     ^         ^
  //           |  |          |         |
  //           |  |     +----+         |
  //          Merge<--+ | +------------+
  //            ^      \|/
  //            |      Phi
  //            |       |
  //          Branch----+
  //            ^
  //            |
  //      +-----+-----+
  //      |           |
  //    IfTrue     IfFalse
  //      ^           ^
  //      |           |

  // The resulting graph (modulo the Phi and EffectPhi nodes) looks like this:

  // Control1 Cond1 ... ControlN CondN
  //    ^      ^           ^      ^
  //    \      /           \      /
  //     Branch     ...     Branch
  //       ^                  ^
  //       |                  |
  //   +---+---+          +---+----+
  //   |       |          |        |
  // IfTrue IfFalse ... IfTrue  IfFalse
  //   ^       ^          ^        ^
  //   |       |          |        |
  //   +--+ +-------------+        |
  //      | |  +--------------+ +--+
  //      | |                 | |
  //     Merge               Merge
  //       ^                   ^
  //       |                   |

  Node* branch = node;
  Node* cond = NodeProperties::GetValueInput(branch, 0);
  if (!cond->OwnedBy(branch) || cond->opcode() != IrOpcode::kPhi) return;
  Node* merge = NodeProperties::GetControlInput(branch);
  if (merge->opcode() != IrOpcode::kMerge ||
      NodeProperties::GetControlInput(cond) != merge) {
    return;
  }
  // Grab the IfTrue/IfFalse projections of the Branch.
  BranchMatcher matcher(branch);
  // Check/collect other Phi/EffectPhi nodes hanging off the Merge.
  NodeVector phis(graph->zone());
  for (Node* const use : merge->uses()) {
    if (use == branch || use == cond) continue;
    // We cannot currently deal with non-Phi/EffectPhi nodes hanging off the
    // Merge. Ideally, we would just clone the nodes (and everything that
    // depends on it to some distant join point), but that requires knowledge
    // about dominance/post-dominance.
    if (!NodeProperties::IsPhi(use)) return;
    for (Edge edge : use->use_edges()) {
      // Right now we can only handle Phi/EffectPhi nodes whose uses are
      // directly control-dependend on either the IfTrue or the IfFalse
      // successor, because we know exactly how to update those uses.
      if (edge.from()->op()->ControlInputCount() != 1) return;
      Node* control = NodeProperties::GetControlInput(edge.from());
      if (NodeProperties::IsPhi(edge.from())) {
        control = NodeProperties::GetControlInput(control, edge.index());
      }
      if (control != matcher.IfTrue() && control != matcher.IfFalse()) return;
    }
    phis.push_back(use);
  }
  BranchHint const hint = BranchHintOf(branch->op());
  int const input_count = merge->op()->ControlInputCount();
  DCHECK_LE(1, input_count);
  Node** const inputs = graph->zone()->NewArray<Node*>(2 * input_count);
  Node** const merge_true_inputs = &inputs[0];
  Node** const merge_false_inputs = &inputs[input_count];
  for (int index = 0; index < input_count; ++index) {
    Node* cond1 = NodeProperties::GetValueInput(cond, index);
    Node* control1 = NodeProperties::GetControlInput(merge, index);
    Node* branch1 = graph->NewNode(common->Branch(hint), cond1, control1);
    merge_true_inputs[index] = graph->NewNode(common->IfTrue(), branch1);
    merge_false_inputs[index] = graph->NewNode(common->IfFalse(), branch1);
  }
  Node* const merge_true = matcher.IfTrue();
  Node* const merge_false = matcher.IfFalse();
  merge_true->TrimInputCount(0);
  merge_false->TrimInputCount(0);
  for (int i = 0; i < input_count; ++i) {
    merge_true->AppendInput(graph->zone(), merge_true_inputs[i]);
    merge_false->AppendInput(graph->zone(), merge_false_inputs[i]);
  }
  DCHECK_EQ(2, block->SuccessorCount());
  NodeProperties::ChangeOp(matcher.IfTrue(), common->Merge(input_count));
  NodeProperties::ChangeOp(matcher.IfFalse(), common->Merge(input_count));
  int const true_index =
      block->SuccessorAt(0)->NodeAt(0) == matcher.IfTrue() ? 0 : 1;
  BlockEffectControlData* true_block_data =
      &block_effects->For(block, block->SuccessorAt(true_index));
  BlockEffectControlData* false_block_data =
      &block_effects->For(block, block->SuccessorAt(true_index ^ 1));
  for (Node* const phi : phis) {
    for (int index = 0; index < input_count; ++index) {
      inputs[index] = phi->InputAt(index);
    }
    inputs[input_count] = merge_true;
    Node* phi_true = graph->NewNode(phi->op(), input_count + 1, inputs);
    inputs[input_count] = merge_false;
    Node* phi_false = graph->NewNode(phi->op(), input_count + 1, inputs);
    if (phi->UseCount() == 0) {
      DCHECK_EQ(phi->opcode(), IrOpcode::kEffectPhi);
    } else {
      for (Edge edge : phi->use_edges()) {
        Node* control = NodeProperties::GetControlInput(edge.from());
        if (NodeProperties::IsPhi(edge.from())) {
          control = NodeProperties::GetControlInput(control, edge.index());
        }
        DCHECK(control == matcher.IfTrue() || control == matcher.IfFalse());
        edge.UpdateTo((control == matcher.IfTrue()) ? phi_true : phi_false);
      }
    }
    if (phi->opcode() == IrOpcode::kEffectPhi) {
      true_block_data->current_effect = phi_true;
      false_block_data->current_effect = phi_false;
    }
    phi->Kill();
  }
  // Fix up IfTrue and IfFalse and kill all dead nodes.
  if (branch == block->control_input()) {
    true_block_data->current_control = merge_true;
    false_block_data->current_control = merge_false;
  }
  branch->Kill();
  cond->Kill();
  merge->Kill();
}
}  // namespace

void EffectControlLinearizer::Run() {
  BlockEffectControlMap block_effects(temp_zone());
  ZoneVector<PendingEffectPhi> pending_effect_phis(temp_zone());
  ZoneVector<BasicBlock*> pending_block_controls(temp_zone());
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
        // If all the predecessors have the same effect, we can use it as our
        // current effect.
        effect =
            block_effects.For(block->PredecessorAt(0), block).current_effect;
        for (size_t i = 1; i < block->PredecessorCount(); ++i) {
          if (block_effects.For(block->PredecessorAt(i), block)
                  .current_effect != effect) {
            effect = nullptr;
            break;
          }
        }
        if (effect == nullptr) {
          DCHECK_NE(IrOpcode::kIfException, control->opcode());
          // The input blocks do not have the same effect. We have
          // to create an effect phi node.
          inputs_buffer.clear();
          inputs_buffer.resize(block->PredecessorCount(), jsgraph()->Dead());
          inputs_buffer.push_back(control);
          effect = graph()->NewNode(
              common()->EffectPhi(static_cast<int>(block->PredecessorCount())),
              static_cast<int>(inputs_buffer.size()), &(inputs_buffer.front()));
          // For loops, we update the effect phi node later to break cycles.
          if (control->opcode() == IrOpcode::kLoop) {
            pending_effect_phis.push_back(PendingEffectPhi(effect, block));
          } else {
            UpdateEffectPhi(effect, block, &block_effects);
          }
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

    // The frame state at block entry is determined by the frame states leaving
    // all predecessors. In case there is no frame state dominating this block,
    // we can rely on a checkpoint being present before the next deoptimization.
    // TODO(mstarzinger): Eventually we will need to go hunt for a frame state
    // once deoptimizing nodes roam freely through the schedule.
    Node* frame_state = nullptr;
    if (block != schedule()->start()) {
      // If all the predecessors have the same effect, we can use it
      // as our current effect.
      frame_state =
          block_effects.For(block->PredecessorAt(0), block).current_frame_state;
      for (size_t i = 1; i < block->PredecessorCount(); i++) {
        if (block_effects.For(block->PredecessorAt(i), block)
                .current_frame_state != frame_state) {
          frame_state = nullptr;
          break;
        }
      }
    }

    // Process the ordinary instructions.
    for (; instr < block->NodeCount(); instr++) {
      Node* node = block->NodeAt(instr);
      ProcessNode(node, &frame_state, &effect, &control);
    }

    switch (block->control()) {
      case BasicBlock::kGoto:
      case BasicBlock::kNone:
        break;

      case BasicBlock::kCall:
      case BasicBlock::kTailCall:
      case BasicBlock::kSwitch:
      case BasicBlock::kReturn:
      case BasicBlock::kDeoptimize:
      case BasicBlock::kThrow:
        ProcessNode(block->control_input(), &frame_state, &effect, &control);
        break;

      case BasicBlock::kBranch:
        ProcessNode(block->control_input(), &frame_state, &effect, &control);
        TryCloneBranch(block->control_input(), block, graph(), common(),
                       &block_effects);
        break;
    }

    // Store the effect, control and frame state for later use.
    for (BasicBlock* successor : block->successors()) {
      BlockEffectControlData* data = &block_effects.For(block, successor);
      if (data->current_effect == nullptr) {
        data->current_effect = effect;
      }
      if (data->current_control == nullptr) {
        data->current_control = control;
      }
      data->current_frame_state = frame_state;
    }
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

void EffectControlLinearizer::ProcessNode(Node* node, Node** frame_state,
                                          Node** effect, Node** control) {
  // If the node needs to be wired into the effect/control chain, do this
  // here. Pass current frame state for lowering to eager deoptimization.
  if (TryWireInStateEffect(node, *frame_state, effect, control)) {
    return;
  }

  // If the node has a visible effect, then there must be a checkpoint in the
  // effect chain before we are allowed to place another eager deoptimization
  // point. We zap the frame state to ensure this invariant is maintained.
  if (region_observability_ == RegionObservability::kObservable &&
      !node->op()->HasProperty(Operator::kNoWrite)) {
    *frame_state = nullptr;
  }

  // Remove the end markers of 'atomic' allocation region because the
  // region should be wired-in now.
  if (node->opcode() == IrOpcode::kFinishRegion) {
    // Reset the current region observability.
    region_observability_ = RegionObservability::kObservable;
    // Update the value uses to the value input of the finish node and
    // the effect uses to the effect input.
    return RemoveRegionNode(node);
  }
  if (node->opcode() == IrOpcode::kBeginRegion) {
    // Determine the observability for this region and use that for all
    // nodes inside the region (i.e. ignore the absence of kNoWrite on
    // StoreField and other operators).
    DCHECK_NE(RegionObservability::kNotObservable, region_observability_);
    region_observability_ = RegionObservabilityOf(node->op());
    // Update the value uses to the value input of the finish node and
    // the effect uses to the effect input.
    return RemoveRegionNode(node);
  }

  // Special treatment for checkpoint nodes.
  if (node->opcode() == IrOpcode::kCheckpoint) {
    // Unlink the check point; effect uses will be updated to the incoming
    // effect that is passed. The frame state is preserved for lowering.
    DCHECK_EQ(RegionObservability::kObservable, region_observability_);
    *frame_state = NodeProperties::GetFrameStateInput(node);
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

bool EffectControlLinearizer::TryWireInStateEffect(Node* node,
                                                   Node* frame_state,
                                                   Node** effect,
                                                   Node** control) {
  ValueEffectControl state(nullptr, nullptr, nullptr);
  switch (node->opcode()) {
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
    case IrOpcode::kTruncateTaggedToBit:
      state = LowerTruncateTaggedToBit(node, *effect, *control);
      break;
    case IrOpcode::kTruncateTaggedToFloat64:
      state = LowerTruncateTaggedToFloat64(node, *effect, *control);
      break;
    case IrOpcode::kCheckBounds:
      state = LowerCheckBounds(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckMaps:
      state = LowerCheckMaps(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckNumber:
      state = LowerCheckNumber(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckString:
      state = LowerCheckString(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckIf:
      state = LowerCheckIf(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckHeapObject:
      state = LowerCheckHeapObject(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedInt32Add:
      state = LowerCheckedInt32Add(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedInt32Sub:
      state = LowerCheckedInt32Sub(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedInt32Div:
      state = LowerCheckedInt32Div(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedInt32Mod:
      state = LowerCheckedInt32Mod(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedUint32Div:
      state = LowerCheckedUint32Div(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedUint32Mod:
      state = LowerCheckedUint32Mod(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedInt32Mul:
      state = LowerCheckedInt32Mul(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedInt32ToTaggedSigned:
      state =
          LowerCheckedInt32ToTaggedSigned(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedUint32ToInt32:
      state = LowerCheckedUint32ToInt32(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedUint32ToTaggedSigned:
      state = LowerCheckedUint32ToTaggedSigned(node, frame_state, *effect,
                                               *control);
      break;
    case IrOpcode::kCheckedFloat64ToInt32:
      state = LowerCheckedFloat64ToInt32(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedTaggedSignedToInt32:
      state =
          LowerCheckedTaggedSignedToInt32(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedTaggedToInt32:
      state = LowerCheckedTaggedToInt32(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedTaggedToFloat64:
      state = LowerCheckedTaggedToFloat64(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedTaggedToTaggedSigned:
      state = LowerCheckedTaggedToTaggedSigned(node, frame_state, *effect,
                                               *control);
      break;
    case IrOpcode::kTruncateTaggedToWord32:
      state = LowerTruncateTaggedToWord32(node, *effect, *control);
      break;
    case IrOpcode::kCheckedTruncateTaggedToWord32:
      state = LowerCheckedTruncateTaggedToWord32(node, frame_state, *effect,
                                                 *control);
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
    case IrOpcode::kArrayBufferWasNeutered:
      state = LowerArrayBufferWasNeutered(node, *effect, *control);
      break;
    case IrOpcode::kStringFromCharCode:
      state = LowerStringFromCharCode(node, *effect, *control);
      break;
    case IrOpcode::kStringFromCodePoint:
      state = LowerStringFromCodePoint(node, *effect, *control);
      break;
    case IrOpcode::kStringCharCodeAt:
      state = LowerStringCharCodeAt(node, *effect, *control);
      break;
    case IrOpcode::kStringEqual:
      state = LowerStringEqual(node, *effect, *control);
      break;
    case IrOpcode::kStringLessThan:
      state = LowerStringLessThan(node, *effect, *control);
      break;
    case IrOpcode::kStringLessThanOrEqual:
      state = LowerStringLessThanOrEqual(node, *effect, *control);
      break;
    case IrOpcode::kCheckFloat64Hole:
      state = LowerCheckFloat64Hole(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckTaggedHole:
      state = LowerCheckTaggedHole(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kConvertTaggedHoleToUndefined:
      state = LowerConvertTaggedHoleToUndefined(node, *effect, *control);
      break;
    case IrOpcode::kPlainPrimitiveToNumber:
      state = LowerPlainPrimitiveToNumber(node, *effect, *control);
      break;
    case IrOpcode::kPlainPrimitiveToWord32:
      state = LowerPlainPrimitiveToWord32(node, *effect, *control);
      break;
    case IrOpcode::kPlainPrimitiveToFloat64:
      state = LowerPlainPrimitiveToFloat64(node, *effect, *control);
      break;
    case IrOpcode::kEnsureWritableFastElements:
      state = LowerEnsureWritableFastElements(node, *effect, *control);
      break;
    case IrOpcode::kMaybeGrowFastElements:
      state = LowerMaybeGrowFastElements(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kTransitionElementsKind:
      state = LowerTransitionElementsKind(node, *effect, *control);
      break;
    case IrOpcode::kLoadTypedElement:
      state = LowerLoadTypedElement(node, *effect, *control);
      break;
    case IrOpcode::kStoreTypedElement:
      state = LowerStoreTypedElement(node, *effect, *control);
      break;
    case IrOpcode::kFloat64RoundUp:
      state = LowerFloat64RoundUp(node, *effect, *control);
      break;
    case IrOpcode::kFloat64RoundDown:
      state = LowerFloat64RoundDown(node, *effect, *control);
      break;
    case IrOpcode::kFloat64RoundTruncate:
      state = LowerFloat64RoundTruncate(node, *effect, *control);
      break;
    default:
      return false;
  }
  NodeProperties::ReplaceUses(node, state.value, state.effect, state.control);
  *effect = state.effect;
  *control = state.control;
  return true;
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeFloat64ToTagged(Node* node, Node* effect,
                                                    Node* control) {
  Node* value = node->InputAt(0);
  return AllocateHeapNumberWithValue(value, effect, control);
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

  Node* add = graph()->NewNode(machine()->Int32AddWithOverflow(), value, value,
                               control);

  Node* ovf = graph()->NewNode(common()->Projection(1), add, control);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), ovf, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  ValueEffectControl alloc =
      AllocateHeapNumberWithValue(ChangeInt32ToFloat64(value), effect, if_true);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = graph()->NewNode(common()->Projection(0), add, if_false);

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
EffectControlLinearizer::LowerTruncateTaggedToBit(Node* node, Node* effect,
                                                  Node* control) {
  Node* value = node->InputAt(0);
  Node* one = jsgraph()->Int32Constant(1);
  Node* zero = jsgraph()->Int32Constant(0);
  Node* fzero = jsgraph()->Float64Constant(0.0);

  // Collect effect/control/value triples.
  int count = 0;
  Node* values[7];
  Node* effects[7];
  Node* controls[6];

  // Check if {value} is a Smi.
  Node* check_smi = ObjectIsSmi(value);
  Node* branch_smi = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                      check_smi, control);

  // If {value} is a Smi, then we only need to check that it's not zero.
  Node* if_smi = graph()->NewNode(common()->IfTrue(), branch_smi);
  Node* esmi = effect;
  {
    controls[count] = if_smi;
    effects[count] = esmi;
    values[count] =
        graph()->NewNode(machine()->Word32Equal(),
                         graph()->NewNode(machine()->WordEqual(), value,
                                          jsgraph()->ZeroConstant()),
                         zero);
    count++;
  }
  control = graph()->NewNode(common()->IfFalse(), branch_smi);

  // Load the map instance type of {value}.
  Node* value_map = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMap()), value, effect, control);
  Node* value_instance_type = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapInstanceType()), value_map,
      effect, control);

  // Check if {value} is an Oddball.
  Node* check_oddball =
      graph()->NewNode(machine()->Word32Equal(), value_instance_type,
                       jsgraph()->Int32Constant(ODDBALL_TYPE));
  Node* branch_oddball = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                          check_oddball, control);

  // The only Oddball {value} that is trueish is true itself.
  Node* if_oddball = graph()->NewNode(common()->IfTrue(), branch_oddball);
  Node* eoddball = effect;
  {
    controls[count] = if_oddball;
    effects[count] = eoddball;
    values[count] = graph()->NewNode(machine()->WordEqual(), value,
                                     jsgraph()->TrueConstant());
    count++;
  }
  control = graph()->NewNode(common()->IfFalse(), branch_oddball);

  // Check if {value} is a String.
  Node* check_string =
      graph()->NewNode(machine()->Int32LessThan(), value_instance_type,
                       jsgraph()->Int32Constant(FIRST_NONSTRING_TYPE));
  Node* branch_string =
      graph()->NewNode(common()->Branch(), check_string, control);

  // For String {value}, we need to check that the length is not zero.
  Node* if_string = graph()->NewNode(common()->IfTrue(), branch_string);
  Node* estring = effect;
  {
    // Load the {value} length.
    Node* value_length = estring = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForStringLength()), value,
        estring, if_string);

    controls[count] = if_string;
    effects[count] = estring;
    values[count] =
        graph()->NewNode(machine()->Word32Equal(),
                         graph()->NewNode(machine()->WordEqual(), value_length,
                                          jsgraph()->ZeroConstant()),
                         zero);
    count++;
  }
  control = graph()->NewNode(common()->IfFalse(), branch_string);

  // Check if {value} is a HeapNumber.
  Node* check_heapnumber =
      graph()->NewNode(machine()->Word32Equal(), value_instance_type,
                       jsgraph()->Int32Constant(HEAP_NUMBER_TYPE));
  Node* branch_heapnumber =
      graph()->NewNode(common()->Branch(), check_heapnumber, control);

  // For HeapNumber {value}, just check that its value is not 0.0, -0.0 or NaN.
  Node* if_heapnumber = graph()->NewNode(common()->IfTrue(), branch_heapnumber);
  Node* eheapnumber = effect;
  {
    // Load the raw value of {value}.
    Node* value_value = eheapnumber = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
        eheapnumber, if_heapnumber);

    // Check if {value} is either less than 0.0 or greater than 0.0.
    Node* check =
        graph()->NewNode(machine()->Float64LessThan(), fzero, value_value);
    Node* branch = graph()->NewNode(common()->Branch(), check, if_heapnumber);

    controls[count] = graph()->NewNode(common()->IfTrue(), branch);
    effects[count] = eheapnumber;
    values[count] = one;
    count++;

    controls[count] = graph()->NewNode(common()->IfFalse(), branch);
    effects[count] = eheapnumber;
    values[count] =
        graph()->NewNode(machine()->Float64LessThan(), value_value, fzero);
    count++;
  }
  control = graph()->NewNode(common()->IfFalse(), branch_heapnumber);

  // The {value} is either a JSReceiver, a Symbol or some Simd128Value. In
  // those cases we can just the undetectable bit on the map, which will only
  // be set for certain JSReceivers, i.e. document.all.
  {
    // Load the {value} map bit field.
    Node* value_map_bitfield = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForMapBitField()), value_map,
        effect, control);

    controls[count] = control;
    effects[count] = effect;
    values[count] = graph()->NewNode(
        machine()->Word32Equal(),
        graph()->NewNode(machine()->Word32And(), value_map_bitfield,
                         jsgraph()->Int32Constant(1 << Map::kIsUndetectable)),
        zero);
    count++;
  }

  // Merge the different controls.
  control = graph()->NewNode(common()->Merge(count), count, controls);
  effects[count] = control;
  effect = graph()->NewNode(common()->EffectPhi(count), count + 1, effects);
  values[count] = control;
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, count),
                           count + 1, values);

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
  return LowerTruncateTaggedToFloat64(node, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerTruncateTaggedToFloat64(Node* node, Node* effect,
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
EffectControlLinearizer::LowerCheckBounds(Node* node, Node* frame_state,
                                          Node* effect, Node* control) {
  Node* index = node->InputAt(0);
  Node* limit = node->InputAt(1);

  Node* check = graph()->NewNode(machine()->Uint32LessThan(), index, limit);
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kOutOfBounds), check,
      frame_state, effect, control);

  return ValueEffectControl(index, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckMaps(Node* node, Node* frame_state,
                                        Node* effect, Node* control) {
  Node* value = node->InputAt(0);

  // Load the current map of the {value}.
  Node* value_map = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMap()), value, effect, control);

  int const map_count = node->op()->ValueInputCount() - 1;
  Node** controls = temp_zone()->NewArray<Node*>(map_count);
  Node** effects = temp_zone()->NewArray<Node*>(map_count + 1);

  for (int i = 0; i < map_count; ++i) {
    Node* map = node->InputAt(1 + i);

    Node* check = graph()->NewNode(machine()->WordEqual(), value_map, map);
    if (i == map_count - 1) {
      controls[i] = effects[i] = graph()->NewNode(
          common()->DeoptimizeUnless(DeoptimizeReason::kWrongMap), check,
          frame_state, effect, control);
    } else {
      control = graph()->NewNode(common()->Branch(), check, control);
      controls[i] = graph()->NewNode(common()->IfTrue(), control);
      control = graph()->NewNode(common()->IfFalse(), control);
      effects[i] = effect;
    }
  }

  control = graph()->NewNode(common()->Merge(map_count), map_count, controls);
  effects[map_count] = control;
  effect =
      graph()->NewNode(common()->EffectPhi(map_count), map_count + 1, effects);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckNumber(Node* node, Node* frame_state,
                                          Node* effect, Node* control) {
  Node* value = node->InputAt(0);

  Node* check0 = ObjectIsSmi(value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  {
    Node* value_map = efalse0 =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse0, if_false0);
    Node* check1 = graph()->NewNode(machine()->WordEqual(), value_map,
                                    jsgraph()->HeapNumberMapConstant());
    if_false0 = efalse0 = graph()->NewNode(
        common()->DeoptimizeUnless(DeoptimizeReason::kNotAHeapNumber), check1,
        frame_state, efalse0, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckString(Node* node, Node* frame_state,
                                          Node* effect, Node* control) {
  Node* value = node->InputAt(0);

  Node* check0 = ObjectIsSmi(value);
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kSmi), check0,
                       frame_state, effect, control);

  Node* value_map = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMap()), value, effect, control);
  Node* value_instance_type = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapInstanceType()), value_map,
      effect, control);

  Node* check1 =
      graph()->NewNode(machine()->Uint32LessThan(), value_instance_type,
                       jsgraph()->Uint32Constant(FIRST_NONSTRING_TYPE));
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kWrongInstanceType), check1,
      frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckIf(Node* node, Node* frame_state,
                                      Node* effect, Node* control) {
  Node* value = node->InputAt(0);

  control = effect =
      graph()->NewNode(common()->DeoptimizeUnless(DeoptimizeReason::kNoReason),
                       value, frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckHeapObject(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kSmi), check,
                       frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedInt32Add(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* value =
      graph()->NewNode(machine()->Int32AddWithOverflow(), lhs, rhs, control);

  Node* check = graph()->NewNode(common()->Projection(1), value, control);
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kOverflow),
                       check, frame_state, effect, control);

  value = graph()->NewNode(common()->Projection(0), value, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedInt32Sub(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* value =
      graph()->NewNode(machine()->Int32SubWithOverflow(), lhs, rhs, control);

  Node* check = graph()->NewNode(common()->Projection(1), value, control);
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kOverflow),
                       check, frame_state, effect, control);

  value = graph()->NewNode(common()->Projection(0), value, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedInt32Div(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  Node* zero = jsgraph()->Int32Constant(0);
  Node* minusone = jsgraph()->Int32Constant(-1);
  Node* minint = jsgraph()->Int32Constant(std::numeric_limits<int32_t>::min());

  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  // Check if {rhs} is positive (and not zero).
  Node* check0 = graph()->NewNode(machine()->Int32LessThan(), zero, rhs);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    // Fast case, no additional checking required.
    vtrue0 = graph()->NewNode(machine()->Int32Div(), lhs, rhs, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    // Check if {rhs} is zero.
    Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
    if_false0 = efalse0 = graph()->NewNode(
        common()->DeoptimizeIf(DeoptimizeReason::kDivisionByZero), check,
        frame_state, efalse0, if_false0);

    // Check if {lhs} is zero, as that would produce minus zero.
    check = graph()->NewNode(machine()->Word32Equal(), lhs, zero);
    if_false0 = efalse0 =
        graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kMinusZero),
                         check, frame_state, efalse0, if_false0);

    // Check if {lhs} is kMinInt and {rhs} is -1, in which case we'd have
    // to return -kMinInt, which is not representable.
    Node* check1 = graph()->NewNode(machine()->Word32Equal(), lhs, minint);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    {
      // Check if {rhs} is -1.
      Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, minusone);
      if_true1 = etrue1 =
          graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kOverflow),
                           check, frame_state, etrue1, if_true1);
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);

    // Perform the actual integer division.
    vfalse0 = graph()->NewNode(machine()->Int32Div(), lhs, rhs, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2), vtrue0,
                       vfalse0, control);

  // Check if the remainder is non-zero.
  Node* check =
      graph()->NewNode(machine()->Word32Equal(), lhs,
                       graph()->NewNode(machine()->Int32Mul(), rhs, value));
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kLostPrecision), check,
      frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedInt32Mod(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  Node* zero = jsgraph()->Int32Constant(0);
  Node* one = jsgraph()->Int32Constant(1);

  // General case for signed integer modulus, with optimization for (unknown)
  // power of 2 right hand side.
  //
  //   if rhs <= 0 then
  //     rhs = -rhs
  //     deopt if rhs == 0
  //   if lhs < 0 then
  //     let res = lhs % rhs in
  //     deopt if res == 0
  //     res
  //   else
  //     let msk = rhs - 1 in
  //     if rhs & msk == 0 then
  //       lhs & msk
  //     else
  //       lhs % rhs
  //
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  // Check if {rhs} is not strictly positive.
  Node* check0 = graph()->NewNode(machine()->Int32LessThanOrEqual(), rhs, zero);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    // Negate {rhs}, might still produce a negative result in case of
    // -2^31, but that is handled safely below.
    vtrue0 = graph()->NewNode(machine()->Int32Sub(), zero, rhs);

    // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
    Node* check = graph()->NewNode(machine()->Word32Equal(), vtrue0, zero);
    if_true0 = etrue0 = graph()->NewNode(
        common()->DeoptimizeIf(DeoptimizeReason::kDivisionByZero), check,
        frame_state, etrue0, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0 = rhs;

  // At this point {rhs} is either greater than zero or -2^31, both are
  // fine for the code that follows.
  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  rhs = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                         vtrue0, vfalse0, control);

  // Check if {lhs} is negative.
  Node* check1 = graph()->NewNode(machine()->Int32LessThan(), lhs, zero);
  Node* branch1 =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check1, control);

  Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
  Node* etrue1 = effect;
  Node* vtrue1;
  {
    // Compute the remainder using {lhs % msk}.
    vtrue1 = graph()->NewNode(machine()->Int32Mod(), lhs, rhs, if_true1);

    // Check if we would have to return -0.
    Node* check = graph()->NewNode(machine()->Word32Equal(), vtrue1, zero);
    if_true1 = etrue1 =
        graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kMinusZero),
                         check, frame_state, etrue1, if_true1);
  }

  Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
  Node* efalse1 = effect;
  Node* vfalse1;
  {
    Node* msk = graph()->NewNode(machine()->Int32Sub(), rhs, one);

    // Check if {rhs} minus one is a valid mask.
    Node* check2 = graph()->NewNode(
        machine()->Word32Equal(),
        graph()->NewNode(machine()->Word32And(), rhs, msk), zero);
    Node* branch2 = graph()->NewNode(common()->Branch(), check2, if_false1);

    // Compute the remainder using {lhs & msk}.
    Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
    Node* vtrue2 = graph()->NewNode(machine()->Word32And(), lhs, msk);

    // Compute the remainder using the generic {lhs % rhs}.
    Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
    Node* vfalse2 =
        graph()->NewNode(machine()->Int32Mod(), lhs, rhs, if_false2);

    if_false1 = graph()->NewNode(common()->Merge(2), if_true2, if_false2);
    vfalse1 = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                               vtrue2, vfalse2, if_false1);
  }

  control = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2), vtrue1,
                       vfalse1, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedUint32Div(Node* node, Node* frame_state,
                                               Node* effect, Node* control) {
  Node* zero = jsgraph()->Int32Constant(0);

  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
  Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
  control = effect = graph()->NewNode(
      common()->DeoptimizeIf(DeoptimizeReason::kDivisionByZero), check,
      frame_state, effect, control);

  // Perform the actual unsigned integer division.
  Node* value = graph()->NewNode(machine()->Uint32Div(), lhs, rhs, control);

  // Check if the remainder is non-zero.
  check = graph()->NewNode(machine()->Word32Equal(), lhs,
                           graph()->NewNode(machine()->Int32Mul(), rhs, value));
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kLostPrecision), check,
      frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedUint32Mod(Node* node, Node* frame_state,
                                               Node* effect, Node* control) {
  Node* zero = jsgraph()->Int32Constant(0);

  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
  Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
  control = effect = graph()->NewNode(
      common()->DeoptimizeIf(DeoptimizeReason::kDivisionByZero), check,
      frame_state, effect, control);

  // Perform the actual unsigned integer modulus.
  Node* value = graph()->NewNode(machine()->Uint32Mod(), lhs, rhs, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedInt32Mul(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  CheckForMinusZeroMode mode = CheckMinusZeroModeOf(node->op());
  Node* zero = jsgraph()->Int32Constant(0);
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* projection =
      graph()->NewNode(machine()->Int32MulWithOverflow(), lhs, rhs, control);

  Node* check = graph()->NewNode(common()->Projection(1), projection, control);
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kOverflow),
                       check, frame_state, effect, control);

  Node* value = graph()->NewNode(common()->Projection(0), projection, control);

  if (mode == CheckForMinusZeroMode::kCheckForMinusZero) {
    Node* check_zero = graph()->NewNode(machine()->Word32Equal(), value, zero);
    Node* branch_zero = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                         check_zero, control);

    Node* if_zero = graph()->NewNode(common()->IfTrue(), branch_zero);
    Node* e_if_zero = effect;
    {
      // We may need to return negative zero.
      Node* or_inputs = graph()->NewNode(machine()->Word32Or(), lhs, rhs);
      Node* check_or =
          graph()->NewNode(machine()->Int32LessThan(), or_inputs, zero);
      if_zero = e_if_zero =
          graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kMinusZero),
                           check_or, frame_state, e_if_zero, if_zero);
    }

    Node* if_not_zero = graph()->NewNode(common()->IfFalse(), branch_zero);
    Node* e_if_not_zero = effect;

    control = graph()->NewNode(common()->Merge(2), if_zero, if_not_zero);
    effect = graph()->NewNode(common()->EffectPhi(2), e_if_zero, e_if_not_zero,
                              control);
  }

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedInt32ToTaggedSigned(Node* node,
                                                         Node* frame_state,
                                                         Node* effect,
                                                         Node* control) {
  DCHECK(SmiValuesAre31Bits());
  Node* value = node->InputAt(0);

  Node* add = graph()->NewNode(machine()->Int32AddWithOverflow(), value, value,
                               control);

  Node* check = graph()->NewNode(common()->Projection(1), add, control);
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kOverflow),
                       check, frame_state, effect, control);

  value = graph()->NewNode(common()->Projection(0), add, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedUint32ToInt32(Node* node,
                                                   Node* frame_state,
                                                   Node* effect,
                                                   Node* control) {
  Node* value = node->InputAt(0);
  Node* max_int = jsgraph()->Int32Constant(std::numeric_limits<int32_t>::max());
  Node* is_safe =
      graph()->NewNode(machine()->Uint32LessThanOrEqual(), value, max_int);
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kLostPrecision), is_safe,
      frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedUint32ToTaggedSigned(Node* node,
                                                          Node* frame_state,
                                                          Node* effect,
                                                          Node* control) {
  Node* value = node->InputAt(0);
  Node* check = graph()->NewNode(machine()->Uint32LessThanOrEqual(), value,
                                 SmiMaxValueConstant());
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kLostPrecision), check,
      frame_state, effect, control);
  value = ChangeUint32ToSmi(value);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::BuildCheckedFloat64ToInt32(CheckForMinusZeroMode mode,
                                                    Node* value,
                                                    Node* frame_state,
                                                    Node* effect,
                                                    Node* control) {
  Node* value32 = graph()->NewNode(machine()->RoundFloat64ToInt32(), value);
  Node* check_same = graph()->NewNode(
      machine()->Float64Equal(), value,
      graph()->NewNode(machine()->ChangeInt32ToFloat64(), value32));
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kLostPrecisionOrNaN),
      check_same, frame_state, effect, control);

  if (mode == CheckForMinusZeroMode::kCheckForMinusZero) {
    // Check if {value} is -0.
    Node* check_zero = graph()->NewNode(machine()->Word32Equal(), value32,
                                        jsgraph()->Int32Constant(0));
    Node* branch_zero = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                         check_zero, control);

    Node* if_zero = graph()->NewNode(common()->IfTrue(), branch_zero);
    Node* if_notzero = graph()->NewNode(common()->IfFalse(), branch_zero);

    // In case of 0, we need to check the high bits for the IEEE -0 pattern.
    Node* check_negative = graph()->NewNode(
        machine()->Int32LessThan(),
        graph()->NewNode(machine()->Float64ExtractHighWord32(), value),
        jsgraph()->Int32Constant(0));

    Node* deopt_minus_zero =
        graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kMinusZero),
                         check_negative, frame_state, effect, if_zero);

    control =
        graph()->NewNode(common()->Merge(2), deopt_minus_zero, if_notzero);
    effect = graph()->NewNode(common()->EffectPhi(2), deopt_minus_zero, effect,
                              control);
  }

  return ValueEffectControl(value32, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedFloat64ToInt32(Node* node,
                                                    Node* frame_state,
                                                    Node* effect,
                                                    Node* control) {
  CheckForMinusZeroMode mode = CheckMinusZeroModeOf(node->op());
  Node* value = node->InputAt(0);

  return BuildCheckedFloat64ToInt32(mode, value, frame_state, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedTaggedSignedToInt32(Node* node,
                                                         Node* frame_state,
                                                         Node* effect,
                                                         Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  control = effect =
      graph()->NewNode(common()->DeoptimizeUnless(DeoptimizeReason::kNotASmi),
                       check, frame_state, effect, control);
  value = ChangeSmiToInt32(value);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedTaggedToInt32(Node* node,
                                                   Node* frame_state,
                                                   Node* effect,
                                                   Node* control) {
  CheckForMinusZeroMode mode = CheckMinusZeroModeOf(node->op());
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  // In the Smi case, just convert to int32.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);

  // In the non-Smi case, check the heap numberness, load the number and convert
  // to int32.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    Node* check = graph()->NewNode(machine()->WordEqual(), value_map,
                                   jsgraph()->HeapNumberMapConstant());
    if_false = efalse = graph()->NewNode(
        common()->DeoptimizeUnless(DeoptimizeReason::kNotAHeapNumber), check,
        frame_state, efalse, if_false);
    vfalse = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
        efalse, if_false);
    ValueEffectControl state =
        BuildCheckedFloat64ToInt32(mode, vfalse, frame_state, efalse, if_false);
    if_false = state.control;
    efalse = state.effect;
    vfalse = state.value;
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::BuildCheckedHeapNumberOrOddballToFloat64(
    CheckTaggedInputMode mode, Node* value, Node* frame_state, Node* effect,
    Node* control) {
  Node* value_map = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMap()), value, effect, control);

  Node* check_number = graph()->NewNode(machine()->WordEqual(), value_map,
                                        jsgraph()->HeapNumberMapConstant());

  switch (mode) {
    case CheckTaggedInputMode::kNumber: {
      control = effect = graph()->NewNode(
          common()->DeoptimizeUnless(DeoptimizeReason::kNotAHeapNumber),
          check_number, frame_state, effect, control);
      break;
    }
    case CheckTaggedInputMode::kNumberOrOddball: {
      Node* branch =
          graph()->NewNode(common()->Branch(), check_number, control);

      Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
      Node* etrue = effect;

      Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
      // For oddballs also contain the numeric value, let us just check that
      // we have an oddball here.
      Node* efalse = effect;
      Node* instance_type = efalse = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForMapInstanceType()),
          value_map, efalse, if_false);
      Node* check_oddball =
          graph()->NewNode(machine()->Word32Equal(), instance_type,
                           jsgraph()->Int32Constant(ODDBALL_TYPE));
      if_false = efalse = graph()->NewNode(
          common()->DeoptimizeUnless(
              DeoptimizeReason::kNotAHeapNumberUndefinedBoolean),
          check_oddball, frame_state, efalse, if_false);
      STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);

      control = graph()->NewNode(common()->Merge(2), if_true, if_false);
      effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
      break;
    }
  }

  value = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
      effect, control);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedTaggedToFloat64(Node* node,
                                                     Node* frame_state,
                                                     Node* effect,
                                                     Node* control) {
  CheckTaggedInputMode mode = CheckTaggedInputModeOf(node->op());
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch = graph()->NewNode(common()->Branch(), check, control);

  // In the Smi case, just convert to int32 and then float64.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);
  vtrue = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue);

  // Otherwise, check heap numberness and load the number.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  ValueEffectControl number_state = BuildCheckedHeapNumberOrOddballToFloat64(
      mode, value, frame_state, effect, if_false);

  Node* merge =
      graph()->NewNode(common()->Merge(2), if_true, number_state.control);
  Node* effect_phi = graph()->NewNode(common()->EffectPhi(2), etrue,
                                      number_state.effect, merge);
  Node* result =
      graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2), vtrue,
                       number_state.value, merge);

  return ValueEffectControl(result, effect_phi, merge);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedTaggedToTaggedSigned(Node* node,
                                                          Node* frame_state,
                                                          Node* effect,
                                                          Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  control = effect =
      graph()->NewNode(common()->DeoptimizeUnless(DeoptimizeReason::kNotASmi),
                       check, frame_state, effect, control);

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
EffectControlLinearizer::LowerCheckedTruncateTaggedToWord32(Node* node,
                                                            Node* frame_state,
                                                            Node* effect,
                                                            Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  // In the Smi case, just convert to int32.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);

  // Otherwise, check that it's a heap number or oddball and truncate the value
  // to int32.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  ValueEffectControl false_state = BuildCheckedHeapNumberOrOddballToFloat64(
      CheckTaggedInputMode::kNumberOrOddball, value, frame_state, effect,
      if_false);
  false_state.value =
      graph()->NewNode(machine()->TruncateFloat64ToWord32(), false_state.value);

  Node* merge =
      graph()->NewNode(common()->Merge(2), if_true, false_state.control);
  Node* effect_phi = graph()->NewNode(common()->EffectPhi(2), etrue,
                                      false_state.effect, merge);
  Node* result =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2), vtrue,
                       false_state.value, merge);

  return ValueEffectControl(result, effect_phi, merge);
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
EffectControlLinearizer::LowerArrayBufferWasNeutered(Node* node, Node* effect,
                                                     Node* control) {
  Node* value = node->InputAt(0);

  Node* value_bit_field = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayBufferBitField()), value,
      effect, control);
  value = graph()->NewNode(
      machine()->Word32Equal(),
      graph()->NewNode(machine()->Word32Equal(),
                       graph()->NewNode(machine()->Word32And(), value_bit_field,
                                        jsgraph()->Int32Constant(
                                            JSArrayBuffer::WasNeutered::kMask)),
                       jsgraph()->Int32Constant(0)),
      jsgraph()->Int32Constant(0));

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerStringCharCodeAt(Node* node, Node* effect,
                                               Node* control) {
  Node* subject = node->InputAt(0);
  Node* index = node->InputAt(1);

  // We may need to loop several times for ConsString/SlicedString {subject}s.
  Node* loop =
      graph()->NewNode(common()->Loop(4), control, control, control, control);
  Node* lsubject =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 4),
                       subject, subject, subject, subject, loop);
  Node* lindex =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 4), index,
                       index, index, index, loop);
  Node* leffect = graph()->NewNode(common()->EffectPhi(4), effect, effect,
                                   effect, effect, loop);

  control = loop;
  effect = leffect;

  // Determine the instance type of {lsubject}.
  Node* lsubject_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       lsubject, effect, control);
  Node* lsubject_instance_type = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapInstanceType()),
      lsubject_map, effect, control);

  // Check if {lsubject} is a SeqString.
  Node* check0 = graph()->NewNode(
      machine()->Word32Equal(),
      graph()->NewNode(machine()->Word32And(), lsubject_instance_type,
                       jsgraph()->Int32Constant(kStringRepresentationMask)),
      jsgraph()->Int32Constant(kSeqStringTag));
  Node* branch0 = graph()->NewNode(common()->Branch(), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    // Check if the {lsubject} is a TwoByteSeqString or a OneByteSeqString.
    Node* check1 = graph()->NewNode(
        machine()->Word32Equal(),
        graph()->NewNode(machine()->Word32And(), lsubject_instance_type,
                         jsgraph()->Int32Constant(kStringEncodingMask)),
        jsgraph()->Int32Constant(kTwoByteStringTag));
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = etrue0;
    Node* vtrue1 = etrue1 =
        graph()->NewNode(simplified()->LoadElement(
                             AccessBuilder::ForSeqTwoByteStringCharacter()),
                         lsubject, lindex, etrue1, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = etrue0;
    Node* vfalse1 = efalse1 =
        graph()->NewNode(simplified()->LoadElement(
                             AccessBuilder::ForSeqOneByteStringCharacter()),
                         lsubject, lindex, efalse1, if_false1);

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    etrue0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_true0);
    vtrue0 = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                              vtrue1, vfalse1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    // Check if the {lsubject} is a ConsString.
    Node* check1 = graph()->NewNode(
        machine()->Word32Equal(),
        graph()->NewNode(machine()->Word32And(), lsubject_instance_type,
                         jsgraph()->Int32Constant(kStringRepresentationMask)),
        jsgraph()->Int32Constant(kConsStringTag));
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    {
      // Load the right hand side of the {lsubject} ConsString.
      Node* lsubject_second = etrue1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForConsStringSecond()),
          lsubject, etrue1, if_true1);

      // Check whether the right hand side is the empty string (i.e. if
      // this is really a flat string in a cons string). If that is not
      // the case we flatten the string first.
      Node* check2 = graph()->NewNode(machine()->WordEqual(), lsubject_second,
                                      jsgraph()->EmptyStringConstant());
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                       check2, if_true1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* etrue2 = etrue1;
      Node* vtrue2 = etrue2 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForConsStringFirst()),
          lsubject, etrue2, if_true2);

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* efalse2 = etrue1;
      Node* vfalse2;
      {
        // Flatten the {lsubject} ConsString first.
        Operator::Properties properties =
            Operator::kNoDeopt | Operator::kNoThrow;
        Runtime::FunctionId id = Runtime::kFlattenString;
        CallDescriptor const* desc = Linkage::GetRuntimeCallDescriptor(
            graph()->zone(), id, 1, properties, CallDescriptor::kNoFlags);
        vfalse2 = efalse2 = graph()->NewNode(
            common()->Call(desc), jsgraph()->CEntryStubConstant(1), lsubject,
            jsgraph()->ExternalConstant(ExternalReference(id, isolate())),
            jsgraph()->Int32Constant(1), jsgraph()->NoContextConstant(),
            efalse2, if_false2);
      }

      // Retry the {loop} with the new subject.
      loop->ReplaceInput(1, if_true2);
      lindex->ReplaceInput(1, lindex);
      leffect->ReplaceInput(1, etrue2);
      lsubject->ReplaceInput(1, vtrue2);
      loop->ReplaceInput(2, if_false2);
      lindex->ReplaceInput(2, lindex);
      leffect->ReplaceInput(2, efalse2);
      lsubject->ReplaceInput(2, vfalse2);
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;
    Node* vfalse1;
    {
      // Check if the {lsubject} is an ExternalString.
      Node* check2 = graph()->NewNode(
          machine()->Word32Equal(),
          graph()->NewNode(machine()->Word32And(), lsubject_instance_type,
                           jsgraph()->Int32Constant(kStringRepresentationMask)),
          jsgraph()->Int32Constant(kExternalStringTag));
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                       check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* etrue2 = efalse1;
      Node* vtrue2;
      {
        // Check if the {lsubject} is a short external string.
        Node* check3 = graph()->NewNode(
            machine()->Word32Equal(),
            graph()->NewNode(
                machine()->Word32And(), lsubject_instance_type,
                jsgraph()->Int32Constant(kShortExternalStringMask)),
            jsgraph()->Int32Constant(0));
        Node* branch3 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                         check3, if_true2);

        Node* if_true3 = graph()->NewNode(common()->IfTrue(), branch3);
        Node* etrue3 = etrue2;
        Node* vtrue3;
        {
          // Load the actual resource data from the {lsubject}.
          Node* lsubject_resource_data = etrue3 = graph()->NewNode(
              simplified()->LoadField(
                  AccessBuilder::ForExternalStringResourceData()),
              lsubject, etrue3, if_true3);

          // Check if the {lsubject} is a TwoByteExternalString or a
          // OneByteExternalString.
          Node* check4 = graph()->NewNode(
              machine()->Word32Equal(),
              graph()->NewNode(machine()->Word32And(), lsubject_instance_type,
                               jsgraph()->Int32Constant(kStringEncodingMask)),
              jsgraph()->Int32Constant(kTwoByteStringTag));
          Node* branch4 =
              graph()->NewNode(common()->Branch(), check4, if_true3);

          Node* if_true4 = graph()->NewNode(common()->IfTrue(), branch4);
          Node* etrue4 = etrue3;
          Node* vtrue4 = etrue4 = graph()->NewNode(
              simplified()->LoadElement(
                  AccessBuilder::ForExternalTwoByteStringCharacter()),
              lsubject_resource_data, lindex, etrue4, if_true4);

          Node* if_false4 = graph()->NewNode(common()->IfFalse(), branch4);
          Node* efalse4 = etrue3;
          Node* vfalse4 = efalse4 = graph()->NewNode(
              simplified()->LoadElement(
                  AccessBuilder::ForExternalOneByteStringCharacter()),
              lsubject_resource_data, lindex, efalse4, if_false4);

          if_true3 = graph()->NewNode(common()->Merge(2), if_true4, if_false4);
          etrue3 = graph()->NewNode(common()->EffectPhi(2), etrue4, efalse4,
                                    if_true3);
          vtrue3 =
              graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                               vtrue4, vfalse4, if_true3);
        }

        Node* if_false3 = graph()->NewNode(common()->IfFalse(), branch3);
        Node* efalse3 = etrue2;
        Node* vfalse3;
        {
          // The {lsubject} might be compressed, call the runtime.
          Operator::Properties properties =
              Operator::kNoDeopt | Operator::kNoThrow;
          Runtime::FunctionId id = Runtime::kExternalStringGetChar;
          CallDescriptor const* desc = Linkage::GetRuntimeCallDescriptor(
              graph()->zone(), id, 2, properties, CallDescriptor::kNoFlags);
          vfalse3 = efalse3 = graph()->NewNode(
              common()->Call(desc), jsgraph()->CEntryStubConstant(1), lsubject,
              ChangeInt32ToSmi(lindex),
              jsgraph()->ExternalConstant(ExternalReference(id, isolate())),
              jsgraph()->Int32Constant(2), jsgraph()->NoContextConstant(),
              efalse3, if_false3);
          vfalse3 = ChangeSmiToInt32(vfalse3);
        }

        if_true2 = graph()->NewNode(common()->Merge(2), if_true3, if_false3);
        etrue2 =
            graph()->NewNode(common()->EffectPhi(2), etrue3, efalse3, if_true2);
        vtrue2 =
            graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                             vtrue3, vfalse3, if_true2);
      }

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* efalse2 = efalse1;
      {
        // The {lsubject} is a SlicedString, continue with its parent.
        Node* lsubject_parent = efalse2 = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForSlicedStringParent()),
            lsubject, efalse2, if_false2);
        Node* lsubject_offset = efalse2 = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForSlicedStringOffset()),
            lsubject, efalse2, if_false2);
        Node* lsubject_index = graph()->NewNode(
            machine()->Int32Add(), lindex, ChangeSmiToInt32(lsubject_offset));

        // Retry the {loop} with the parent subject.
        loop->ReplaceInput(3, if_false2);
        leffect->ReplaceInput(3, efalse2);
        lindex->ReplaceInput(3, lsubject_index);
        lsubject->ReplaceInput(3, lsubject_parent);
      }

      if_false1 = if_true2;
      efalse1 = etrue2;
      vfalse1 = vtrue2;
    }

    if_false0 = if_false1;
    efalse0 = efalse1;
    vfalse0 = vfalse1;
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2), vtrue0,
                       vfalse0, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerStringFromCharCode(Node* node, Node* effect,
                                                 Node* control) {
  Node* value = node->InputAt(0);

  // Compute the character code.
  Node* code =
      graph()->NewNode(machine()->Word32And(), value,
                       jsgraph()->Int32Constant(String::kMaxUtf16CodeUnit));

  // Check if the {code} is a one-byte char code.
  Node* check0 =
      graph()->NewNode(machine()->Int32LessThanOrEqual(), code,
                       jsgraph()->Int32Constant(String::kMaxOneByteCharCode));
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    // Load the isolate wide single character string cache.
    Node* cache =
        jsgraph()->HeapConstant(factory()->single_character_string_cache());

    // Compute the {cache} index for {code}.
    Node* index =
        machine()->Is32() ? code : graph()->NewNode(
                                       machine()->ChangeUint32ToUint64(), code);

    // Check if we have an entry for the {code} in the single character string
    // cache already.
    Node* entry = etrue0 = graph()->NewNode(
        simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()), cache,
        index, etrue0, if_true0);

    Node* check1 = graph()->NewNode(machine()->WordEqual(), entry,
                                    jsgraph()->UndefinedConstant());
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = etrue0;
    Node* vtrue1;
    {
      // Allocate a new SeqOneByteString for {code}.
      vtrue1 = etrue1 = graph()->NewNode(
          simplified()->Allocate(NOT_TENURED),
          jsgraph()->Int32Constant(SeqOneByteString::SizeFor(1)), etrue1,
          if_true1);
      etrue1 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForMap()), vtrue1,
          jsgraph()->HeapConstant(factory()->one_byte_string_map()), etrue1,
          if_true1);
      etrue1 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForNameHashField()), vtrue1,
          jsgraph()->IntPtrConstant(Name::kEmptyHashField), etrue1, if_true1);
      etrue1 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForStringLength()), vtrue1,
          jsgraph()->SmiConstant(1), etrue1, if_true1);
      etrue1 = graph()->NewNode(
          machine()->Store(StoreRepresentation(MachineRepresentation::kWord8,
                                               kNoWriteBarrier)),
          vtrue1, jsgraph()->IntPtrConstant(SeqOneByteString::kHeaderSize -
                                            kHeapObjectTag),
          code, etrue1, if_true1);

      // Remember it in the {cache}.
      etrue1 = graph()->NewNode(
          simplified()->StoreElement(AccessBuilder::ForFixedArrayElement()),
          cache, index, vtrue1, etrue1, if_true1);
    }

    // Use the {entry} from the {cache}.
    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = etrue0;
    Node* vfalse1 = entry;

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    etrue0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_true0);
    vtrue0 = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                              vtrue1, vfalse1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    // Allocate a new SeqTwoByteString for {code}.
    vfalse0 = efalse0 =
        graph()->NewNode(simplified()->Allocate(NOT_TENURED),
                         jsgraph()->Int32Constant(SeqTwoByteString::SizeFor(1)),
                         efalse0, if_false0);
    efalse0 = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForMap()), vfalse0,
        jsgraph()->HeapConstant(factory()->string_map()), efalse0, if_false0);
    efalse0 = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForNameHashField()), vfalse0,
        jsgraph()->IntPtrConstant(Name::kEmptyHashField), efalse0, if_false0);
    efalse0 = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForStringLength()), vfalse0,
        jsgraph()->SmiConstant(1), efalse0, if_false0);
    efalse0 = graph()->NewNode(
        machine()->Store(StoreRepresentation(MachineRepresentation::kWord16,
                                             kNoWriteBarrier)),
        vfalse0, jsgraph()->IntPtrConstant(SeqTwoByteString::kHeaderSize -
                                           kHeapObjectTag),
        code, efalse0, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue0, vfalse0, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerStringFromCodePoint(Node* node, Node* effect,
                                                  Node* control) {
  Node* value = node->InputAt(0);
  Node* code = value;

  Node* etrue0 = effect;
  Node* vtrue0;

  // Check if the {code} is a single code unit
  Node* check0 = graph()->NewNode(machine()->Uint32LessThanOrEqual(), code,
                                  jsgraph()->Uint32Constant(0xFFFF));
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  {
    // Check if the {code} is a one byte character
    Node* check1 = graph()->NewNode(
        machine()->Uint32LessThanOrEqual(), code,
        jsgraph()->Uint32Constant(String::kMaxOneByteCharCode));
    Node* branch1 =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = etrue0;
    Node* vtrue1;
    {
      // Load the isolate wide single character string cache.
      Node* cache =
          jsgraph()->HeapConstant(factory()->single_character_string_cache());

      // Compute the {cache} index for {code}.
      Node* index =
          machine()->Is32()
              ? code
              : graph()->NewNode(machine()->ChangeUint32ToUint64(), code);

      // Check if we have an entry for the {code} in the single character string
      // cache already.
      Node* entry = etrue1 = graph()->NewNode(
          simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()),
          cache, index, etrue1, if_true1);

      Node* check2 = graph()->NewNode(machine()->WordEqual(), entry,
                                      jsgraph()->UndefinedConstant());
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check2, if_true1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* etrue2 = etrue1;
      Node* vtrue2;
      {
        // Allocate a new SeqOneByteString for {code}.
        vtrue2 = etrue2 = graph()->NewNode(
            simplified()->Allocate(NOT_TENURED),
            jsgraph()->Int32Constant(SeqOneByteString::SizeFor(1)), etrue2,
            if_true2);
        etrue2 = graph()->NewNode(
            simplified()->StoreField(AccessBuilder::ForMap()), vtrue2,
            jsgraph()->HeapConstant(factory()->one_byte_string_map()), etrue2,
            if_true2);
        etrue2 = graph()->NewNode(
            simplified()->StoreField(AccessBuilder::ForNameHashField()), vtrue2,
            jsgraph()->IntPtrConstant(Name::kEmptyHashField), etrue2, if_true2);
        etrue2 = graph()->NewNode(
            simplified()->StoreField(AccessBuilder::ForStringLength()), vtrue2,
            jsgraph()->SmiConstant(1), etrue2, if_true2);
        etrue2 = graph()->NewNode(
            machine()->Store(StoreRepresentation(MachineRepresentation::kWord8,
                                                 kNoWriteBarrier)),
            vtrue2, jsgraph()->IntPtrConstant(SeqOneByteString::kHeaderSize -
                                              kHeapObjectTag),
            code, etrue2, if_true2);

        // Remember it in the {cache}.
        etrue2 = graph()->NewNode(
            simplified()->StoreElement(AccessBuilder::ForFixedArrayElement()),
            cache, index, vtrue2, etrue2, if_true2);
      }

      // Use the {entry} from the {cache}.
      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* efalse2 = etrue0;
      Node* vfalse2 = entry;

      if_true1 = graph()->NewNode(common()->Merge(2), if_true2, if_false2);
      etrue1 =
          graph()->NewNode(common()->EffectPhi(2), etrue2, efalse2, if_true1);
      vtrue1 =
          graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue2, vfalse2, if_true1);
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = effect;
    Node* vfalse1;
    {
      // Allocate a new SeqTwoByteString for {code}.
      vfalse1 = efalse1 = graph()->NewNode(
          simplified()->Allocate(NOT_TENURED),
          jsgraph()->Int32Constant(SeqTwoByteString::SizeFor(1)), efalse1,
          if_false1);
      efalse1 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForMap()), vfalse1,
          jsgraph()->HeapConstant(factory()->string_map()), efalse1, if_false1);
      efalse1 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForNameHashField()), vfalse1,
          jsgraph()->IntPtrConstant(Name::kEmptyHashField), efalse1, if_false1);
      efalse1 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForStringLength()), vfalse1,
          jsgraph()->SmiConstant(1), efalse1, if_false1);
      efalse1 = graph()->NewNode(
          machine()->Store(StoreRepresentation(MachineRepresentation::kWord16,
                                               kNoWriteBarrier)),
          vfalse1, jsgraph()->IntPtrConstant(SeqTwoByteString::kHeaderSize -
                                             kHeapObjectTag),
          code, efalse1, if_false1);
    }

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    etrue0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_true0);
    vtrue0 = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                              vtrue1, vfalse1, if_true0);
  }

  // Generate surrogate pair string
  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    switch (UnicodeEncodingOf(node->op())) {
      case UnicodeEncoding::UTF16:
        break;

      case UnicodeEncoding::UTF32: {
        // Convert UTF32 to UTF16 code units, and store as a 32 bit word.
        Node* lead_offset = jsgraph()->Int32Constant(0xD800 - (0x10000 >> 10));

        // lead = (codepoint >> 10) + LEAD_OFFSET
        Node* lead =
            graph()->NewNode(machine()->Int32Add(),
                             graph()->NewNode(machine()->Word32Shr(), code,
                                              jsgraph()->Int32Constant(10)),
                             lead_offset);

        // trail = (codepoint & 0x3FF) + 0xDC00;
        Node* trail =
            graph()->NewNode(machine()->Int32Add(),
                             graph()->NewNode(machine()->Word32And(), code,
                                              jsgraph()->Int32Constant(0x3FF)),
                             jsgraph()->Int32Constant(0xDC00));

        // codpoint = (trail << 16) | lead;
        code = graph()->NewNode(machine()->Word32Or(),
                                graph()->NewNode(machine()->Word32Shl(), trail,
                                                 jsgraph()->Int32Constant(16)),
                                lead);
        break;
      }
    }

    // Allocate a new SeqTwoByteString for {code}.
    vfalse0 = efalse0 =
        graph()->NewNode(simplified()->Allocate(NOT_TENURED),
                         jsgraph()->Int32Constant(SeqTwoByteString::SizeFor(2)),
                         efalse0, if_false0);
    efalse0 = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForMap()), vfalse0,
        jsgraph()->HeapConstant(factory()->string_map()), efalse0, if_false0);
    efalse0 = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForNameHashField()), vfalse0,
        jsgraph()->IntPtrConstant(Name::kEmptyHashField), efalse0, if_false0);
    efalse0 = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForStringLength()), vfalse0,
        jsgraph()->SmiConstant(2), efalse0, if_false0);
    efalse0 = graph()->NewNode(
        machine()->Store(StoreRepresentation(MachineRepresentation::kWord32,
                                             kNoWriteBarrier)),
        vfalse0, jsgraph()->IntPtrConstant(SeqTwoByteString::kHeaderSize -
                                           kHeapObjectTag),
        code, efalse0, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue0, vfalse0, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerStringComparison(Callable const& callable,
                                               Node* node, Node* effect,
                                               Node* control) {
  Operator::Properties properties = Operator::kEliminatable;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), callable.descriptor(), 0, flags, properties);
  node->InsertInput(graph()->zone(), 0,
                    jsgraph()->HeapConstant(callable.code()));
  node->AppendInput(graph()->zone(), jsgraph()->NoContextConstant());
  node->AppendInput(graph()->zone(), effect);
  NodeProperties::ChangeOp(node, common()->Call(desc));
  return ValueEffectControl(node, node, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerStringEqual(Node* node, Node* effect,
                                          Node* control) {
  return LowerStringComparison(CodeFactory::StringEqual(isolate()), node,
                               effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerStringLessThan(Node* node, Node* effect,
                                             Node* control) {
  return LowerStringComparison(CodeFactory::StringLessThan(isolate()), node,
                               effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerStringLessThanOrEqual(Node* node, Node* effect,
                                                    Node* control) {
  return LowerStringComparison(CodeFactory::StringLessThanOrEqual(isolate()),
                               node, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckFloat64Hole(Node* node, Node* frame_state,
                                               Node* effect, Node* control) {
  // If we reach this point w/o eliminating the {node} that's marked
  // with allow-return-hole, we cannot do anything, so just deoptimize
  // in case of the hole NaN (similar to Crankshaft).
  Node* value = node->InputAt(0);
  Node* check = graph()->NewNode(
      machine()->Word32Equal(),
      graph()->NewNode(machine()->Float64ExtractHighWord32(), value),
      jsgraph()->Int32Constant(kHoleNanUpper32));
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kHole), check,
                       frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckTaggedHole(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  Node* value = node->InputAt(0);
  Node* check = graph()->NewNode(machine()->WordEqual(), value,
                                 jsgraph()->TheHoleConstant());
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kHole), check,
                       frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerConvertTaggedHoleToUndefined(Node* node,
                                                           Node* effect,
                                                           Node* control) {
  Node* value = node->InputAt(0);
  Node* check = graph()->NewNode(machine()->WordEqual(), value,
                                 jsgraph()->TheHoleConstant());
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* vtrue = jsgraph()->UndefinedConstant();

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = value;

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue, vfalse, control);

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

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerPlainPrimitiveToNumber(Node* node, Node* effect,
                                                     Node* control) {
  Node* value = node->InputAt(0);
  Node* result = effect =
      graph()->NewNode(ToNumberOperator(), jsgraph()->ToNumberBuiltinConstant(),
                       value, jsgraph()->NoContextConstant(), effect);
  return ValueEffectControl(result, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerPlainPrimitiveToWord32(Node* node, Node* effect,
                                                     Node* control) {
  Node* value = node->InputAt(0);

  Node* check0 = ObjectIsSmi(value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0 = ChangeSmiToInt32(value);

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    vfalse0 = efalse0 = graph()->NewNode(
        ToNumberOperator(), jsgraph()->ToNumberBuiltinConstant(), value,
        jsgraph()->NoContextConstant(), efalse0);

    Node* check1 = ObjectIsSmi(vfalse0);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    Node* vtrue1 = ChangeSmiToInt32(vfalse0);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;
    Node* vfalse1;
    {
      vfalse1 = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), efalse0,
          efalse1, if_false1);
      vfalse1 = graph()->NewNode(machine()->TruncateFloat64ToWord32(), vfalse1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                               vtrue1, vfalse1, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue0, vfalse0, control);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerPlainPrimitiveToFloat64(Node* node, Node* effect,
                                                      Node* control) {
  Node* value = node->InputAt(0);

  Node* check0 = ObjectIsSmi(value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    vtrue0 = ChangeSmiToInt32(value);
    vtrue0 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    vfalse0 = efalse0 = graph()->NewNode(
        ToNumberOperator(), jsgraph()->ToNumberBuiltinConstant(), value,
        jsgraph()->NoContextConstant(), efalse0);

    Node* check1 = ObjectIsSmi(vfalse0);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    Node* vtrue1;
    {
      vtrue1 = ChangeSmiToInt32(vfalse0);
      vtrue1 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue1);
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;
    Node* vfalse1;
    {
      vfalse1 = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), efalse0,
          efalse1, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                         vtrue1, vfalse1, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue0, vfalse0, control);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerEnsureWritableFastElements(Node* node,
                                                         Node* effect,
                                                         Node* control) {
  Node* object = node->InputAt(0);
  Node* elements = node->InputAt(1);

  // Load the current map of {elements}.
  Node* elements_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       elements, effect, control);

  // Check if {elements} is not a copy-on-write FixedArray.
  Node* check = graph()->NewNode(machine()->WordEqual(), elements_map,
                                 jsgraph()->FixedArrayMapConstant());
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  // Nothing to do if the {elements} are not copy-on-write.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = elements;

  // We need to take a copy of the {elements} and set them up for {object}.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    // We need to create a copy of the {elements} for {object}.
    Operator::Properties properties = Operator::kEliminatable;
    Callable callable = CodeFactory::CopyFastSmiOrObjectElements(isolate());
    CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
    CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0, flags,
        properties);
    vfalse = efalse = graph()->NewNode(
        common()->Call(desc), jsgraph()->HeapConstant(callable.code()), object,
        jsgraph()->NoContextConstant(), efalse);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  Node* value = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerMaybeGrowFastElements(Node* node,
                                                    Node* frame_state,
                                                    Node* effect,
                                                    Node* control) {
  GrowFastElementsFlags flags = GrowFastElementsFlagsOf(node->op());
  Node* object = node->InputAt(0);
  Node* elements = node->InputAt(1);
  Node* index = node->InputAt(2);
  Node* length = node->InputAt(3);

  Node* check0 = graph()->NewNode((flags & GrowFastElementsFlag::kHoleyElements)
                                      ? machine()->Uint32LessThanOrEqual()
                                      : machine()->Word32Equal(),
                                  length, index);
  Node* branch0 = graph()->NewNode(common()->Branch(), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0 = elements;
  {
    // Load the length of the {elements} backing store.
    Node* elements_length = etrue0 = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForFixedArrayLength()), elements,
        etrue0, if_true0);
    elements_length = ChangeSmiToInt32(elements_length);

    // Check if we need to grow the {elements} backing store.
    Node* check1 =
        graph()->NewNode(machine()->Uint32LessThan(), index, elements_length);
    Node* branch1 =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = etrue0;
    Node* vtrue1 = vtrue0;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = etrue0;
    Node* vfalse1 = vtrue0;
    {
      // We need to grow the {elements} for {object}.
      Operator::Properties properties = Operator::kEliminatable;
      Callable callable =
          (flags & GrowFastElementsFlag::kDoubleElements)
              ? CodeFactory::GrowFastDoubleElements(isolate())
              : CodeFactory::GrowFastSmiOrObjectElements(isolate());
      CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
      CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
          isolate(), graph()->zone(), callable.descriptor(), 0, flags,
          properties);
      vfalse1 = efalse1 = graph()->NewNode(
          common()->Call(desc), jsgraph()->HeapConstant(callable.code()),
          object, ChangeInt32ToSmi(index), jsgraph()->NoContextConstant(),
          efalse1);

      // Ensure that we were able to grow the {elements}.
      // TODO(turbofan): We use kSmi as reason here similar to Crankshaft,
      // but maybe we should just introduce a reason that makes sense.
      efalse1 = if_false1 = graph()->NewNode(
          common()->DeoptimizeIf(DeoptimizeReason::kSmi), ObjectIsSmi(vfalse1),
          frame_state, efalse1, if_false1);
    }

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    etrue0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_true0);
    vtrue0 = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                              vtrue1, vfalse1, if_true0);

    // For JSArray {object}s we also need to update the "length".
    if (flags & GrowFastElementsFlag::kArrayObject) {
      // Compute the new {length}.
      Node* object_length = ChangeInt32ToSmi(graph()->NewNode(
          machine()->Int32Add(), index, jsgraph()->Int32Constant(1)));

      // Update the "length" property of the {object}.
      etrue0 =
          graph()->NewNode(simplified()->StoreField(
                               AccessBuilder::ForJSArrayLength(FAST_ELEMENTS)),
                           object, object_length, etrue0, if_true0);
    }
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0 = elements;
  {
    // In case of non-holey {elements}, we need to verify that the {index} is
    // in-bounds, otherwise for holey {elements}, the check above already
    // guards the index (and the operator forces {index} to be unsigned).
    if (!(flags & GrowFastElementsFlag::kHoleyElements)) {
      Node* check1 =
          graph()->NewNode(machine()->Uint32LessThan(), index, length);
      efalse0 = if_false0 = graph()->NewNode(
          common()->DeoptimizeUnless(DeoptimizeReason::kOutOfBounds), check1,
          frame_state, efalse0, if_false0);
    }
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2), vtrue0,
                       vfalse0, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerTransitionElementsKind(Node* node, Node* effect,
                                                     Node* control) {
  ElementsTransition const transition = ElementsTransitionOf(node->op());
  Node* object = node->InputAt(0);
  Node* source_map = node->InputAt(1);
  Node* target_map = node->InputAt(2);

  // Load the current map of {object}.
  Node* object_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()), object,
                       effect, control);

  // Check if {object_map} is the same as {source_map}.
  Node* check =
      graph()->NewNode(machine()->WordEqual(), object_map, source_map);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  // Migrate the {object} from {source_map} to {target_map}.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  {
    switch (transition) {
      case ElementsTransition::kFastTransition: {
        // In-place migration of {object}, just store the {target_map}.
        etrue =
            graph()->NewNode(simplified()->StoreField(AccessBuilder::ForMap()),
                             object, target_map, etrue, if_true);
        break;
      }
      case ElementsTransition::kSlowTransition: {
        // Instance migration, call out to the runtime for {object}.
        Operator::Properties properties =
            Operator::kNoDeopt | Operator::kNoThrow;
        Runtime::FunctionId id = Runtime::kTransitionElementsKind;
        CallDescriptor const* desc = Linkage::GetRuntimeCallDescriptor(
            graph()->zone(), id, 2, properties, CallDescriptor::kNoFlags);
        etrue = graph()->NewNode(
            common()->Call(desc), jsgraph()->CEntryStubConstant(1), object,
            target_map,
            jsgraph()->ExternalConstant(ExternalReference(id, isolate())),
            jsgraph()->Int32Constant(2), jsgraph()->NoContextConstant(), etrue,
            if_true);
        break;
      }
    }
  }

  // Nothing to do if the {object} doesn't have the {source_map}.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);

  return ValueEffectControl(nullptr, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerLoadTypedElement(Node* node, Node* effect,
                                               Node* control) {
  ExternalArrayType array_type = ExternalArrayTypeOf(node->op());
  Node* buffer = node->InputAt(0);
  Node* base = node->InputAt(1);
  Node* external = node->InputAt(2);
  Node* index = node->InputAt(3);

  // We need to keep the {buffer} alive so that the GC will not release the
  // ArrayBuffer (if there's any) as long as we are still operating on it.
  effect = graph()->NewNode(common()->Retain(), buffer, effect);

  // Compute the effective storage pointer.
  Node* storage = effect = graph()->NewNode(machine()->UnsafePointerAdd(), base,
                                            external, effect, control);

  // Perform the actual typed element access.
  Node* value = effect = graph()->NewNode(
      simplified()->LoadElement(
          AccessBuilder::ForTypedArrayElement(array_type, true)),
      storage, index, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerStoreTypedElement(Node* node, Node* effect,
                                                Node* control) {
  ExternalArrayType array_type = ExternalArrayTypeOf(node->op());
  Node* buffer = node->InputAt(0);
  Node* base = node->InputAt(1);
  Node* external = node->InputAt(2);
  Node* index = node->InputAt(3);
  Node* value = node->InputAt(4);

  // We need to keep the {buffer} alive so that the GC will not release the
  // ArrayBuffer (if there's any) as long as we are still operating on it.
  effect = graph()->NewNode(common()->Retain(), buffer, effect);

  // Compute the effective storage pointer.
  Node* storage = effect = graph()->NewNode(machine()->UnsafePointerAdd(), base,
                                            external, effect, control);

  // Perform the actual typed element access.
  effect = graph()->NewNode(
      simplified()->StoreElement(
          AccessBuilder::ForTypedArrayElement(array_type, true)),
      storage, index, value, effect, control);

  return ValueEffectControl(nullptr, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerFloat64RoundUp(Node* node, Node* effect,
                                             Node* control) {
  // Nothing to be done if a fast hardware instruction is available.
  if (machine()->Float64RoundUp().IsSupported()) {
    return ValueEffectControl(node, effect, control);
  }

  Node* const one = jsgraph()->Float64Constant(1.0);
  Node* const zero = jsgraph()->Float64Constant(0.0);
  Node* const minus_zero = jsgraph()->Float64Constant(-0.0);
  Node* const two_52 = jsgraph()->Float64Constant(4503599627370496.0E0);
  Node* const minus_two_52 = jsgraph()->Float64Constant(-4503599627370496.0E0);
  Node* const input = node->InputAt(0);

  // General case for ceil.
  //
  //   if 0.0 < input then
  //     if 2^52 <= input then
  //       input
  //     else
  //       let temp1 = (2^52 + input) - 2^52 in
  //       if temp1 < input then
  //         temp1 + 1
  //       else
  //         temp1
  //   else
  //     if input == 0 then
  //       input
  //     else
  //       if input <= -2^52 then
  //         input
  //       else
  //         let temp1 = -0 - input in
  //         let temp2 = (2^52 + temp1) - 2^52 in
  //         let temp3 = (if temp1 < temp2 then temp2 - 1 else temp2) in
  //         -0 - temp3
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.

  Node* check0 = graph()->NewNode(machine()->Float64LessThan(), zero, input);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* vtrue0;
  {
    Node* check1 =
        graph()->NewNode(machine()->Float64LessThanOrEqual(), two_52, input);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* temp1 = graph()->NewNode(
          machine()->Float64Sub(),
          graph()->NewNode(machine()->Float64Add(), two_52, input), two_52);
      vfalse1 = graph()->NewNode(
          common()->Select(MachineRepresentation::kFloat64),
          graph()->NewNode(machine()->Float64LessThan(), temp1, input),
          graph()->NewNode(machine()->Float64Add(), temp1, one), temp1);
    }

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vtrue0 = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                              vtrue1, vfalse1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* vfalse0;
  {
    Node* check1 = graph()->NewNode(machine()->Float64Equal(), input, zero);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* check2 = graph()->NewNode(machine()->Float64LessThanOrEqual(),
                                      input, minus_two_52);
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* vtrue2 = input;

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* vfalse2;
      {
        Node* temp1 =
            graph()->NewNode(machine()->Float64Sub(), minus_zero, input);
        Node* temp2 = graph()->NewNode(
            machine()->Float64Sub(),
            graph()->NewNode(machine()->Float64Add(), two_52, temp1), two_52);
        Node* temp3 = graph()->NewNode(
            common()->Select(MachineRepresentation::kFloat64),
            graph()->NewNode(machine()->Float64LessThan(), temp1, temp2),
            graph()->NewNode(machine()->Float64Sub(), temp2, one), temp2);
        vfalse2 = graph()->NewNode(machine()->Float64Sub(), minus_zero, temp3);
      }

      if_false1 = graph()->NewNode(common()->Merge(2), if_true2, if_false2);
      vfalse1 =
          graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue2, vfalse2, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vfalse0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                         vtrue1, vfalse1, if_false0);
  }

  Node* merge0 = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                       vtrue0, vfalse0, merge0);
  return ValueEffectControl(value, effect, merge0);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerFloat64RoundDown(Node* node, Node* effect,
                                               Node* control) {
  // Nothing to be done if a fast hardware instruction is available.
  if (machine()->Float64RoundDown().IsSupported()) {
    return ValueEffectControl(node, effect, control);
  }

  Node* const one = jsgraph()->Float64Constant(1.0);
  Node* const zero = jsgraph()->Float64Constant(0.0);
  Node* const minus_one = jsgraph()->Float64Constant(-1.0);
  Node* const minus_zero = jsgraph()->Float64Constant(-0.0);
  Node* const two_52 = jsgraph()->Float64Constant(4503599627370496.0E0);
  Node* const minus_two_52 = jsgraph()->Float64Constant(-4503599627370496.0E0);
  Node* const input = node->InputAt(0);

  // General case for floor.
  //
  //   if 0.0 < input then
  //     if 2^52 <= input then
  //       input
  //     else
  //       let temp1 = (2^52 + input) - 2^52 in
  //       if input < temp1 then
  //         temp1 - 1
  //       else
  //         temp1
  //   else
  //     if input == 0 then
  //       input
  //     else
  //       if input <= -2^52 then
  //         input
  //       else
  //         let temp1 = -0 - input in
  //         let temp2 = (2^52 + temp1) - 2^52 in
  //         if temp2 < temp1 then
  //           -1 - temp2
  //         else
  //           -0 - temp2
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.

  Node* check0 = graph()->NewNode(machine()->Float64LessThan(), zero, input);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* vtrue0;
  {
    Node* check1 =
        graph()->NewNode(machine()->Float64LessThanOrEqual(), two_52, input);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* temp1 = graph()->NewNode(
          machine()->Float64Sub(),
          graph()->NewNode(machine()->Float64Add(), two_52, input), two_52);
      vfalse1 = graph()->NewNode(
          common()->Select(MachineRepresentation::kFloat64),
          graph()->NewNode(machine()->Float64LessThan(), input, temp1),
          graph()->NewNode(machine()->Float64Sub(), temp1, one), temp1);
    }

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vtrue0 = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                              vtrue1, vfalse1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* vfalse0;
  {
    Node* check1 = graph()->NewNode(machine()->Float64Equal(), input, zero);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* check2 = graph()->NewNode(machine()->Float64LessThanOrEqual(),
                                      input, minus_two_52);
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* vtrue2 = input;

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* vfalse2;
      {
        Node* temp1 =
            graph()->NewNode(machine()->Float64Sub(), minus_zero, input);
        Node* temp2 = graph()->NewNode(
            machine()->Float64Sub(),
            graph()->NewNode(machine()->Float64Add(), two_52, temp1), two_52);
        vfalse2 = graph()->NewNode(
            common()->Select(MachineRepresentation::kFloat64),
            graph()->NewNode(machine()->Float64LessThan(), temp2, temp1),
            graph()->NewNode(machine()->Float64Sub(), minus_one, temp2),
            graph()->NewNode(machine()->Float64Sub(), minus_zero, temp2));
      }

      if_false1 = graph()->NewNode(common()->Merge(2), if_true2, if_false2);
      vfalse1 =
          graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue2, vfalse2, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vfalse0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                         vtrue1, vfalse1, if_false0);
  }

  Node* merge0 = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                       vtrue0, vfalse0, merge0);
  return ValueEffectControl(value, effect, merge0);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerFloat64RoundTruncate(Node* node, Node* effect,
                                                   Node* control) {
  // Nothing to be done if a fast hardware instruction is available.
  if (machine()->Float64RoundTruncate().IsSupported()) {
    return ValueEffectControl(node, effect, control);
  }

  Node* const one = jsgraph()->Float64Constant(1.0);
  Node* const zero = jsgraph()->Float64Constant(0.0);
  Node* const minus_zero = jsgraph()->Float64Constant(-0.0);
  Node* const two_52 = jsgraph()->Float64Constant(4503599627370496.0E0);
  Node* const minus_two_52 = jsgraph()->Float64Constant(-4503599627370496.0E0);
  Node* const input = node->InputAt(0);

  // General case for trunc.
  //
  //   if 0.0 < input then
  //     if 2^52 <= input then
  //       input
  //     else
  //       let temp1 = (2^52 + input) - 2^52 in
  //       if input < temp1 then
  //         temp1 - 1
  //       else
  //         temp1
  //   else
  //     if input == 0 then
  //       input
  //     else
  //       if input <= -2^52 then
  //         input
  //       else
  //         let temp1 = -0 - input in
  //         let temp2 = (2^52 + temp1) - 2^52 in
  //         let temp3 = (if temp1 < temp2 then temp2 - 1 else temp2) in
  //         -0 - temp3
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.

  Node* check0 = graph()->NewNode(machine()->Float64LessThan(), zero, input);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* vtrue0;
  {
    Node* check1 =
        graph()->NewNode(machine()->Float64LessThanOrEqual(), two_52, input);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* temp1 = graph()->NewNode(
          machine()->Float64Sub(),
          graph()->NewNode(machine()->Float64Add(), two_52, input), two_52);
      vfalse1 = graph()->NewNode(
          common()->Select(MachineRepresentation::kFloat64),
          graph()->NewNode(machine()->Float64LessThan(), input, temp1),
          graph()->NewNode(machine()->Float64Sub(), temp1, one), temp1);
    }

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vtrue0 = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                              vtrue1, vfalse1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* vfalse0;
  {
    Node* check1 = graph()->NewNode(machine()->Float64Equal(), input, zero);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* vtrue1 = input;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* vfalse1;
    {
      Node* check2 = graph()->NewNode(machine()->Float64LessThanOrEqual(),
                                      input, minus_two_52);
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* vtrue2 = input;

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* vfalse2;
      {
        Node* temp1 =
            graph()->NewNode(machine()->Float64Sub(), minus_zero, input);
        Node* temp2 = graph()->NewNode(
            machine()->Float64Sub(),
            graph()->NewNode(machine()->Float64Add(), two_52, temp1), two_52);
        Node* temp3 = graph()->NewNode(
            common()->Select(MachineRepresentation::kFloat64),
            graph()->NewNode(machine()->Float64LessThan(), temp1, temp2),
            graph()->NewNode(machine()->Float64Sub(), temp2, one), temp2);
        vfalse2 = graph()->NewNode(machine()->Float64Sub(), minus_zero, temp3);
      }

      if_false1 = graph()->NewNode(common()->Merge(2), if_true2, if_false2);
      vfalse1 =
          graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue2, vfalse2, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    vfalse0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                         vtrue1, vfalse1, if_false0);
  }

  Node* merge0 = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                       vtrue0, vfalse0, merge0);
  return ValueEffectControl(value, effect, merge0);
}

Factory* EffectControlLinearizer::factory() const {
  return isolate()->factory();
}

Isolate* EffectControlLinearizer::isolate() const {
  return jsgraph()->isolate();
}

Operator const* EffectControlLinearizer::ToNumberOperator() {
  if (!to_number_operator_.is_set()) {
    Callable callable = CodeFactory::ToNumber(isolate());
    CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0, flags,
        Operator::kEliminatable);
    to_number_operator_.set(common()->Call(desc));
  }
  return to_number_operator_.get();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/effect-control-linearizer.h"

#include "src/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/schedule.h"
#include "src/heap/factory-inl.h"
#include "src/objects/heap-number.h"
#include "src/objects/oddball.h"

namespace v8 {
namespace internal {
namespace compiler {

EffectControlLinearizer::EffectControlLinearizer(
    JSGraph* js_graph, Schedule* schedule, Zone* temp_zone,
    SourcePositionTable* source_positions, NodeOriginTable* node_origins,
    MaskArrayIndexEnable mask_array_index)
    : js_graph_(js_graph),
      schedule_(schedule),
      temp_zone_(temp_zone),
      mask_array_index_(mask_array_index),
      source_positions_(source_positions),
      node_origins_(node_origins),
      graph_assembler_(js_graph, nullptr, nullptr, temp_zone),
      frame_state_zapper_(nullptr) {}

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

void ConnectUnreachableToEnd(Node* effect, Node* control, JSGraph* jsgraph) {
  Graph* graph = jsgraph->graph();
  CommonOperatorBuilder* common = jsgraph->common();
  if (effect->opcode() == IrOpcode::kDead) return;
  if (effect->opcode() != IrOpcode::kUnreachable) {
    effect = graph->NewNode(common->Unreachable(), effect, control);
  }
  Node* throw_node = graph->NewNode(common->Throw(), effect, control);
  NodeProperties::MergeControlToEnd(graph, common, throw_node);
}

void UpdateEffectPhi(Node* node, BasicBlock* block,
                     BlockEffectControlMap* block_effects, JSGraph* jsgraph) {
  // Update all inputs to an effect phi with the effects from the given
  // block->effect map.
  DCHECK_EQ(IrOpcode::kEffectPhi, node->opcode());
  DCHECK_EQ(static_cast<size_t>(node->op()->EffectInputCount()),
            block->PredecessorCount());
  for (int i = 0; i < node->op()->EffectInputCount(); i++) {
    Node* input = node->InputAt(i);
    BasicBlock* predecessor = block->PredecessorAt(static_cast<size_t>(i));
    const BlockEffectControlData& block_effect =
        block_effects->For(predecessor, block);
    Node* effect = block_effect.current_effect;
    if (input != effect) {
      node->ReplaceInput(i, effect);
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
         static_cast<size_t>(control->op()->ControlInputCount()) ==
             block->PredecessorCount());
  if (static_cast<size_t>(control->op()->ControlInputCount()) !=
      block->PredecessorCount()) {
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

void RemoveRenameNode(Node* node) {
  DCHECK(IrOpcode::kFinishRegion == node->opcode() ||
         IrOpcode::kBeginRegion == node->opcode() ||
         IrOpcode::kTypeGuard == node->opcode());
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

void TryCloneBranch(Node* node, BasicBlock* block, Zone* temp_zone,
                    Graph* graph, CommonOperatorBuilder* common,
                    BlockEffectControlMap* block_effects,
                    SourcePositionTable* source_positions,
                    NodeOriginTable* node_origins) {
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

  SourcePositionTable::Scope scope(source_positions,
                                   source_positions->GetSourcePosition(node));
  NodeOriginTable::Scope origin_scope(node_origins, "clone branch", node);
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
  NodeVector phis(temp_zone);
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
  DCHECK_EQ(2u, block->SuccessorCount());
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
    Node* effect_phi = nullptr;
    Node* terminate = nullptr;
    for (; instr < block->NodeCount(); instr++) {
      Node* node = block->NodeAt(instr);
      // Only go through the phis and effect phis.
      if (node->opcode() == IrOpcode::kEffectPhi) {
        // There should be at most one effect phi in a block.
        DCHECK_NULL(effect_phi);
        // IfException blocks should not have effect phis.
        DCHECK_NE(IrOpcode::kIfException, control->opcode());
        effect_phi = node;
      } else if (node->opcode() == IrOpcode::kPhi) {
        // Just skip phis.
      } else if (node->opcode() == IrOpcode::kTerminate) {
        DCHECK_NULL(terminate);
        terminate = node;
      } else {
        break;
      }
    }

    if (effect_phi) {
      // Make sure we update the inputs to the incoming blocks' effects.
      if (HasIncomingBackEdges(block)) {
        // In case of loops, we do not update the effect phi immediately
        // because the back predecessor has not been handled yet. We just
        // record the effect phi for later processing.
        pending_effect_phis.push_back(PendingEffectPhi(effect_phi, block));
      } else {
        UpdateEffectPhi(effect_phi, block, &block_effects, jsgraph());
      }
    }

    Node* effect = effect_phi;
    if (effect == nullptr) {
      // There was no effect phi.
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
        for (size_t i = 0; i < block->PredecessorCount(); ++i) {
          const BlockEffectControlData& data =
              block_effects.For(block->PredecessorAt(i), block);
          if (!effect) effect = data.current_effect;
          if (data.current_effect != effect) {
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
            UpdateEffectPhi(effect, block, &block_effects, jsgraph());
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
          frame_state_zapper_ = graph()->end();
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
        TryCloneBranch(block->control_input(), block, temp_zone(), graph(),
                       common(), &block_effects, source_positions_,
                       node_origins_);
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

  for (BasicBlock* pending_block_control : pending_block_controls) {
    UpdateBlockControl(pending_block_control, &block_effects);
  }
  // Update the incoming edges of the effect phis that could not be processed
  // during the first pass (because they could have incoming back edges).
  for (const PendingEffectPhi& pending_effect_phi : pending_effect_phis) {
    UpdateEffectPhi(pending_effect_phi.effect_phi, pending_effect_phi.block,
                    &block_effects, jsgraph());
  }
}

void EffectControlLinearizer::ProcessNode(Node* node, Node** frame_state,
                                          Node** effect, Node** control) {
  SourcePositionTable::Scope scope(source_positions_,
                                   source_positions_->GetSourcePosition(node));
  NodeOriginTable::Scope origin_scope(node_origins_, "process node", node);

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
    frame_state_zapper_ = node;
  }

  // Remove the end markers of 'atomic' allocation region because the
  // region should be wired-in now.
  if (node->opcode() == IrOpcode::kFinishRegion) {
    // Reset the current region observability.
    region_observability_ = RegionObservability::kObservable;
    // Update the value uses to the value input of the finish node and
    // the effect uses to the effect input.
    return RemoveRenameNode(node);
  }
  if (node->opcode() == IrOpcode::kBeginRegion) {
    // Determine the observability for this region and use that for all
    // nodes inside the region (i.e. ignore the absence of kNoWrite on
    // StoreField and other operators).
    DCHECK_NE(RegionObservability::kNotObservable, region_observability_);
    region_observability_ = RegionObservabilityOf(node->op());
    // Update the value uses to the value input of the finish node and
    // the effect uses to the effect input.
    return RemoveRenameNode(node);
  }
  if (node->opcode() == IrOpcode::kTypeGuard) {
    return RemoveRenameNode(node);
  }

  // Special treatment for checkpoint nodes.
  if (node->opcode() == IrOpcode::kCheckpoint) {
    // Unlink the check point; effect uses will be updated to the incoming
    // effect that is passed. The frame state is preserved for lowering.
    DCHECK_EQ(RegionObservability::kObservable, region_observability_);
    *frame_state = NodeProperties::GetFrameStateInput(node);
    return;
  }

  // The IfSuccess nodes should always start a basic block (and basic block
  // start nodes are not handled in the ProcessNode method).
  DCHECK_NE(IrOpcode::kIfSuccess, node->opcode());

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
  // Update the current control.
  if (node->op()->ControlOutputCount() > 0) {
    *control = node;
  }

  // Break the effect chain on {Unreachable} and reconnect to the graph end.
  // Mark the following code for deletion by connecting to the {Dead} node.
  if (node->opcode() == IrOpcode::kUnreachable) {
    ConnectUnreachableToEnd(*effect, *control, jsgraph());
    *effect = *control = jsgraph()->Dead();
  }
}

bool EffectControlLinearizer::TryWireInStateEffect(Node* node,
                                                   Node* frame_state,
                                                   Node** effect,
                                                   Node** control) {
  gasm()->Reset(*effect, *control);
  Node* result = nullptr;
  switch (node->opcode()) {
    case IrOpcode::kChangeBitToTagged:
      result = LowerChangeBitToTagged(node);
      break;
    case IrOpcode::kChangeInt31ToTaggedSigned:
      result = LowerChangeInt31ToTaggedSigned(node);
      break;
    case IrOpcode::kChangeInt32ToTagged:
      result = LowerChangeInt32ToTagged(node);
      break;
    case IrOpcode::kChangeInt64ToTagged:
      result = LowerChangeInt64ToTagged(node);
      break;
    case IrOpcode::kChangeUint32ToTagged:
      result = LowerChangeUint32ToTagged(node);
      break;
    case IrOpcode::kChangeUint64ToTagged:
      result = LowerChangeUint64ToTagged(node);
      break;
    case IrOpcode::kChangeFloat64ToTagged:
      result = LowerChangeFloat64ToTagged(node);
      break;
    case IrOpcode::kChangeFloat64ToTaggedPointer:
      result = LowerChangeFloat64ToTaggedPointer(node);
      break;
    case IrOpcode::kChangeTaggedSignedToInt32:
      result = LowerChangeTaggedSignedToInt32(node);
      break;
    case IrOpcode::kChangeTaggedSignedToInt64:
      result = LowerChangeTaggedSignedToInt64(node);
      break;
    case IrOpcode::kChangeTaggedToBit:
      result = LowerChangeTaggedToBit(node);
      break;
    case IrOpcode::kChangeTaggedToInt32:
      result = LowerChangeTaggedToInt32(node);
      break;
    case IrOpcode::kChangeTaggedToUint32:
      result = LowerChangeTaggedToUint32(node);
      break;
    case IrOpcode::kChangeTaggedToInt64:
      result = LowerChangeTaggedToInt64(node);
      break;
    case IrOpcode::kChangeTaggedToFloat64:
      result = LowerChangeTaggedToFloat64(node);
      break;
    case IrOpcode::kChangeTaggedToTaggedSigned:
      result = LowerChangeTaggedToTaggedSigned(node);
      break;
    case IrOpcode::kTruncateTaggedToBit:
      result = LowerTruncateTaggedToBit(node);
      break;
    case IrOpcode::kTruncateTaggedPointerToBit:
      result = LowerTruncateTaggedPointerToBit(node);
      break;
    case IrOpcode::kTruncateTaggedToFloat64:
      result = LowerTruncateTaggedToFloat64(node);
      break;
    case IrOpcode::kPoisonIndex:
      result = LowerPoisonIndex(node);
      break;
    case IrOpcode::kCheckMaps:
      LowerCheckMaps(node, frame_state);
      break;
    case IrOpcode::kCompareMaps:
      result = LowerCompareMaps(node);
      break;
    case IrOpcode::kCheckNumber:
      result = LowerCheckNumber(node, frame_state);
      break;
    case IrOpcode::kCheckReceiver:
      result = LowerCheckReceiver(node, frame_state);
      break;
    case IrOpcode::kCheckReceiverOrNullOrUndefined:
      result = LowerCheckReceiverOrNullOrUndefined(node, frame_state);
      break;
    case IrOpcode::kCheckSymbol:
      result = LowerCheckSymbol(node, frame_state);
      break;
    case IrOpcode::kCheckString:
      result = LowerCheckString(node, frame_state);
      break;
    case IrOpcode::kCheckInternalizedString:
      result = LowerCheckInternalizedString(node, frame_state);
      break;
    case IrOpcode::kCheckIf:
      LowerCheckIf(node, frame_state);
      break;
    case IrOpcode::kCheckedInt32Add:
      result = LowerCheckedInt32Add(node, frame_state);
      break;
    case IrOpcode::kCheckedInt32Sub:
      result = LowerCheckedInt32Sub(node, frame_state);
      break;
    case IrOpcode::kCheckedInt32Div:
      result = LowerCheckedInt32Div(node, frame_state);
      break;
    case IrOpcode::kCheckedInt32Mod:
      result = LowerCheckedInt32Mod(node, frame_state);
      break;
    case IrOpcode::kCheckedUint32Div:
      result = LowerCheckedUint32Div(node, frame_state);
      break;
    case IrOpcode::kCheckedUint32Mod:
      result = LowerCheckedUint32Mod(node, frame_state);
      break;
    case IrOpcode::kCheckedInt32Mul:
      result = LowerCheckedInt32Mul(node, frame_state);
      break;
    case IrOpcode::kCheckedInt32ToTaggedSigned:
      result = LowerCheckedInt32ToTaggedSigned(node, frame_state);
      break;
    case IrOpcode::kCheckedInt64ToInt32:
      result = LowerCheckedInt64ToInt32(node, frame_state);
      break;
    case IrOpcode::kCheckedInt64ToTaggedSigned:
      result = LowerCheckedInt64ToTaggedSigned(node, frame_state);
      break;
    case IrOpcode::kCheckedUint32Bounds:
      result = LowerCheckedUint32Bounds(node, frame_state);
      break;
    case IrOpcode::kCheckedUint32ToInt32:
      result = LowerCheckedUint32ToInt32(node, frame_state);
      break;
    case IrOpcode::kCheckedUint32ToTaggedSigned:
      result = LowerCheckedUint32ToTaggedSigned(node, frame_state);
      break;
    case IrOpcode::kCheckedUint64Bounds:
      result = LowerCheckedUint64Bounds(node, frame_state);
      break;
    case IrOpcode::kCheckedUint64ToInt32:
      result = LowerCheckedUint64ToInt32(node, frame_state);
      break;
    case IrOpcode::kCheckedUint64ToTaggedSigned:
      result = LowerCheckedUint64ToTaggedSigned(node, frame_state);
      break;
    case IrOpcode::kCheckedFloat64ToInt32:
      result = LowerCheckedFloat64ToInt32(node, frame_state);
      break;
    case IrOpcode::kCheckedFloat64ToInt64:
      result = LowerCheckedFloat64ToInt64(node, frame_state);
      break;
    case IrOpcode::kCheckedTaggedSignedToInt32:
      if (frame_state == nullptr) {
        FATAL("No frame state (zapped by #%d: %s)", frame_state_zapper_->id(),
              frame_state_zapper_->op()->mnemonic());
      }
      result = LowerCheckedTaggedSignedToInt32(node, frame_state);
      break;
    case IrOpcode::kCheckedTaggedToInt32:
      result = LowerCheckedTaggedToInt32(node, frame_state);
      break;
    case IrOpcode::kCheckedTaggedToInt64:
      result = LowerCheckedTaggedToInt64(node, frame_state);
      break;
    case IrOpcode::kCheckedTaggedToFloat64:
      result = LowerCheckedTaggedToFloat64(node, frame_state);
      break;
    case IrOpcode::kCheckedTaggedToTaggedSigned:
      result = LowerCheckedTaggedToTaggedSigned(node, frame_state);
      break;
    case IrOpcode::kCheckedTaggedToTaggedPointer:
      result = LowerCheckedTaggedToTaggedPointer(node, frame_state);
      break;
    case IrOpcode::kTruncateTaggedToWord32:
      result = LowerTruncateTaggedToWord32(node);
      break;
    case IrOpcode::kCheckedTruncateTaggedToWord32:
      result = LowerCheckedTruncateTaggedToWord32(node, frame_state);
      break;
    case IrOpcode::kNumberToString:
      result = LowerNumberToString(node);
      break;
    case IrOpcode::kObjectIsArrayBufferView:
      result = LowerObjectIsArrayBufferView(node);
      break;
    case IrOpcode::kObjectIsBigInt:
      result = LowerObjectIsBigInt(node);
      break;
    case IrOpcode::kObjectIsCallable:
      result = LowerObjectIsCallable(node);
      break;
    case IrOpcode::kObjectIsConstructor:
      result = LowerObjectIsConstructor(node);
      break;
    case IrOpcode::kObjectIsDetectableCallable:
      result = LowerObjectIsDetectableCallable(node);
      break;
    case IrOpcode::kObjectIsMinusZero:
      result = LowerObjectIsMinusZero(node);
      break;
    case IrOpcode::kNumberIsMinusZero:
      result = LowerNumberIsMinusZero(node);
      break;
    case IrOpcode::kObjectIsNaN:
      result = LowerObjectIsNaN(node);
      break;
    case IrOpcode::kNumberIsNaN:
      result = LowerNumberIsNaN(node);
      break;
    case IrOpcode::kObjectIsNonCallable:
      result = LowerObjectIsNonCallable(node);
      break;
    case IrOpcode::kObjectIsNumber:
      result = LowerObjectIsNumber(node);
      break;
    case IrOpcode::kObjectIsReceiver:
      result = LowerObjectIsReceiver(node);
      break;
    case IrOpcode::kObjectIsSmi:
      result = LowerObjectIsSmi(node);
      break;
    case IrOpcode::kObjectIsString:
      result = LowerObjectIsString(node);
      break;
    case IrOpcode::kObjectIsSymbol:
      result = LowerObjectIsSymbol(node);
      break;
    case IrOpcode::kObjectIsUndetectable:
      result = LowerObjectIsUndetectable(node);
      break;
    case IrOpcode::kArgumentsFrame:
      result = LowerArgumentsFrame(node);
      break;
    case IrOpcode::kArgumentsLength:
      result = LowerArgumentsLength(node);
      break;
    case IrOpcode::kToBoolean:
      result = LowerToBoolean(node);
      break;
    case IrOpcode::kTypeOf:
      result = LowerTypeOf(node);
      break;
    case IrOpcode::kNewDoubleElements:
      result = LowerNewDoubleElements(node);
      break;
    case IrOpcode::kNewSmiOrObjectElements:
      result = LowerNewSmiOrObjectElements(node);
      break;
    case IrOpcode::kNewArgumentsElements:
      result = LowerNewArgumentsElements(node);
      break;
    case IrOpcode::kNewConsString:
      result = LowerNewConsString(node);
      break;
    case IrOpcode::kSameValue:
      result = LowerSameValue(node);
      break;
    case IrOpcode::kDeadValue:
      result = LowerDeadValue(node);
      break;
    case IrOpcode::kStringConcat:
      result = LowerStringConcat(node);
      break;
    case IrOpcode::kStringFromSingleCharCode:
      result = LowerStringFromSingleCharCode(node);
      break;
    case IrOpcode::kStringFromSingleCodePoint:
      result = LowerStringFromSingleCodePoint(node);
      break;
    case IrOpcode::kStringIndexOf:
      result = LowerStringIndexOf(node);
      break;
    case IrOpcode::kStringLength:
      result = LowerStringLength(node);
      break;
    case IrOpcode::kStringToNumber:
      result = LowerStringToNumber(node);
      break;
    case IrOpcode::kStringCharCodeAt:
      result = LowerStringCharCodeAt(node);
      break;
    case IrOpcode::kStringCodePointAt:
      result = LowerStringCodePointAt(node, UnicodeEncodingOf(node->op()));
      break;
    case IrOpcode::kStringToLowerCaseIntl:
      result = LowerStringToLowerCaseIntl(node);
      break;
    case IrOpcode::kStringToUpperCaseIntl:
      result = LowerStringToUpperCaseIntl(node);
      break;
    case IrOpcode::kStringSubstring:
      result = LowerStringSubstring(node);
      break;
    case IrOpcode::kStringEqual:
      result = LowerStringEqual(node);
      break;
    case IrOpcode::kStringLessThan:
      result = LowerStringLessThan(node);
      break;
    case IrOpcode::kStringLessThanOrEqual:
      result = LowerStringLessThanOrEqual(node);
      break;
    case IrOpcode::kNumberIsFloat64Hole:
      result = LowerNumberIsFloat64Hole(node);
      break;
    case IrOpcode::kNumberIsFinite:
      result = LowerNumberIsFinite(node);
      break;
    case IrOpcode::kObjectIsFiniteNumber:
      result = LowerObjectIsFiniteNumber(node);
      break;
    case IrOpcode::kNumberIsInteger:
      result = LowerNumberIsInteger(node);
      break;
    case IrOpcode::kObjectIsInteger:
      result = LowerObjectIsInteger(node);
      break;
    case IrOpcode::kNumberIsSafeInteger:
      result = LowerNumberIsSafeInteger(node);
      break;
    case IrOpcode::kObjectIsSafeInteger:
      result = LowerObjectIsSafeInteger(node);
      break;
    case IrOpcode::kCheckFloat64Hole:
      result = LowerCheckFloat64Hole(node, frame_state);
      break;
    case IrOpcode::kCheckNotTaggedHole:
      result = LowerCheckNotTaggedHole(node, frame_state);
      break;
    case IrOpcode::kConvertTaggedHoleToUndefined:
      result = LowerConvertTaggedHoleToUndefined(node);
      break;
    case IrOpcode::kCheckEqualsInternalizedString:
      LowerCheckEqualsInternalizedString(node, frame_state);
      break;
    case IrOpcode::kAllocate:
      result = LowerAllocate(node);
      break;
    case IrOpcode::kCheckEqualsSymbol:
      LowerCheckEqualsSymbol(node, frame_state);
      break;
    case IrOpcode::kPlainPrimitiveToNumber:
      result = LowerPlainPrimitiveToNumber(node);
      break;
    case IrOpcode::kPlainPrimitiveToWord32:
      result = LowerPlainPrimitiveToWord32(node);
      break;
    case IrOpcode::kPlainPrimitiveToFloat64:
      result = LowerPlainPrimitiveToFloat64(node);
      break;
    case IrOpcode::kEnsureWritableFastElements:
      result = LowerEnsureWritableFastElements(node);
      break;
    case IrOpcode::kMaybeGrowFastElements:
      result = LowerMaybeGrowFastElements(node, frame_state);
      break;
    case IrOpcode::kTransitionElementsKind:
      LowerTransitionElementsKind(node);
      break;
    case IrOpcode::kLoadFieldByIndex:
      result = LowerLoadFieldByIndex(node);
      break;
    case IrOpcode::kLoadTypedElement:
      result = LowerLoadTypedElement(node);
      break;
    case IrOpcode::kLoadDataViewElement:
      result = LowerLoadDataViewElement(node);
      break;
    case IrOpcode::kStoreTypedElement:
      LowerStoreTypedElement(node);
      break;
    case IrOpcode::kStoreDataViewElement:
      LowerStoreDataViewElement(node);
      break;
    case IrOpcode::kStoreSignedSmallElement:
      LowerStoreSignedSmallElement(node);
      break;
    case IrOpcode::kFindOrderedHashMapEntry:
      result = LowerFindOrderedHashMapEntry(node);
      break;
    case IrOpcode::kFindOrderedHashMapEntryForInt32Key:
      result = LowerFindOrderedHashMapEntryForInt32Key(node);
      break;
    case IrOpcode::kTransitionAndStoreNumberElement:
      LowerTransitionAndStoreNumberElement(node);
      break;
    case IrOpcode::kTransitionAndStoreNonNumberElement:
      LowerTransitionAndStoreNonNumberElement(node);
      break;
    case IrOpcode::kTransitionAndStoreElement:
      LowerTransitionAndStoreElement(node);
      break;
    case IrOpcode::kRuntimeAbort:
      LowerRuntimeAbort(node);
      break;
    case IrOpcode::kConvertReceiver:
      result = LowerConvertReceiver(node);
      break;
    case IrOpcode::kFloat64RoundUp:
      if (!LowerFloat64RoundUp(node).To(&result)) {
        return false;
      }
      break;
    case IrOpcode::kFloat64RoundDown:
      if (!LowerFloat64RoundDown(node).To(&result)) {
        return false;
      }
      break;
    case IrOpcode::kFloat64RoundTruncate:
      if (!LowerFloat64RoundTruncate(node).To(&result)) {
        return false;
      }
      break;
    case IrOpcode::kFloat64RoundTiesEven:
      if (!LowerFloat64RoundTiesEven(node).To(&result)) {
        return false;
      }
      break;
    case IrOpcode::kDateNow:
      result = LowerDateNow(node);
      break;
    default:
      return false;
  }

  if ((result ? 1 : 0) != node->op()->ValueOutputCount()) {
    FATAL(
        "Effect control linearizer lowering of '%s':"
        " value output count does not agree.",
        node->op()->mnemonic());
  }

  *effect = gasm()->ExtractCurrentEffect();
  *control = gasm()->ExtractCurrentControl();
  NodeProperties::ReplaceUses(node, result, *effect, *control);
  return true;
}

#define __ gasm()->

Node* EffectControlLinearizer::LowerChangeFloat64ToTagged(Node* node) {
  CheckForMinusZeroMode mode = CheckMinusZeroModeOf(node->op());
  Node* value = node->InputAt(0);

  auto done = __ MakeLabel(MachineRepresentation::kTagged);
  auto if_heapnumber = __ MakeDeferredLabel();
  auto if_int32 = __ MakeLabel();

  Node* value32 = __ RoundFloat64ToInt32(value);
  __ GotoIf(__ Float64Equal(value, __ ChangeInt32ToFloat64(value32)),
            &if_int32);
  __ Goto(&if_heapnumber);

  __ Bind(&if_int32);
  {
    if (mode == CheckForMinusZeroMode::kCheckForMinusZero) {
      Node* zero = __ Int32Constant(0);
      auto if_zero = __ MakeDeferredLabel();
      auto if_smi = __ MakeLabel();

      __ GotoIf(__ Word32Equal(value32, zero), &if_zero);
      __ Goto(&if_smi);

      __ Bind(&if_zero);
      {
        // In case of 0, we need to check the high bits for the IEEE -0 pattern.
        __ GotoIf(__ Int32LessThan(__ Float64ExtractHighWord32(value), zero),
                  &if_heapnumber);
        __ Goto(&if_smi);
      }

      __ Bind(&if_smi);
    }

    if (SmiValuesAre32Bits()) {
      Node* value_smi = ChangeInt32ToSmi(value32);
      __ Goto(&done, value_smi);
    } else {
      DCHECK(SmiValuesAre31Bits());
      Node* add = __ Int32AddWithOverflow(value32, value32);
      Node* ovf = __ Projection(1, add);
      __ GotoIf(ovf, &if_heapnumber);
      Node* value_smi = __ Projection(0, add);
      value_smi = ChangeInt32ToIntPtr(value_smi);
      __ Goto(&done, value_smi);
    }
  }

  __ Bind(&if_heapnumber);
  {
    Node* value_number = AllocateHeapNumberWithValue(value);
    __ Goto(&done, value_number);
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeFloat64ToTaggedPointer(Node* node) {
  Node* value = node->InputAt(0);
  return AllocateHeapNumberWithValue(value);
}

Node* EffectControlLinearizer::LowerChangeBitToTagged(Node* node) {
  Node* value = node->InputAt(0);

  auto if_true = __ MakeLabel();
  auto done = __ MakeLabel(MachineRepresentation::kTagged);

  __ GotoIf(value, &if_true);
  __ Goto(&done, __ FalseConstant());

  __ Bind(&if_true);
  __ Goto(&done, __ TrueConstant());

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeInt31ToTaggedSigned(Node* node) {
  Node* value = node->InputAt(0);
  return ChangeInt32ToSmi(value);
}

Node* EffectControlLinearizer::LowerChangeInt32ToTagged(Node* node) {
  Node* value = node->InputAt(0);

  if (SmiValuesAre32Bits()) {
    return ChangeInt32ToSmi(value);
  }
  DCHECK(SmiValuesAre31Bits());

  auto if_overflow = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kTagged);

  Node* add = __ Int32AddWithOverflow(value, value);
  Node* ovf = __ Projection(1, add);
  __ GotoIf(ovf, &if_overflow);
  Node* value_smi = __ Projection(0, add);
  value_smi = ChangeInt32ToIntPtr(value_smi);
  __ Goto(&done, value_smi);

  __ Bind(&if_overflow);
  Node* number = AllocateHeapNumberWithValue(__ ChangeInt32ToFloat64(value));
  __ Goto(&done, number);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeInt64ToTagged(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_in_smi_range = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kTagged);

  Node* value32 = __ TruncateInt64ToInt32(value);
  __ GotoIfNot(__ Word64Equal(__ ChangeInt32ToInt64(value32), value),
               &if_not_in_smi_range);

  if (SmiValuesAre32Bits()) {
    Node* value_smi = ChangeInt64ToSmi(value);
    __ Goto(&done, value_smi);
  } else {
    Node* add = __ Int32AddWithOverflow(value32, value32);
    Node* ovf = __ Projection(1, add);
    __ GotoIf(ovf, &if_not_in_smi_range);
    Node* value_smi = ChangeInt32ToIntPtr(__ Projection(0, add));
    __ Goto(&done, value_smi);
  }

  __ Bind(&if_not_in_smi_range);
  Node* number = AllocateHeapNumberWithValue(__ ChangeInt64ToFloat64(value));
  __ Goto(&done, number);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeUint32ToTagged(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_in_smi_range = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kTagged);

  Node* check = __ Uint32LessThanOrEqual(value, SmiMaxValueConstant());
  __ GotoIfNot(check, &if_not_in_smi_range);
  __ Goto(&done, ChangeUint32ToSmi(value));

  __ Bind(&if_not_in_smi_range);
  Node* number = AllocateHeapNumberWithValue(__ ChangeUint32ToFloat64(value));

  __ Goto(&done, number);
  __ Bind(&done);

  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeUint64ToTagged(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_in_smi_range = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kTagged);

  Node* check =
      __ Uint64LessThanOrEqual(value, __ Int64Constant(Smi::kMaxValue));
  __ GotoIfNot(check, &if_not_in_smi_range);
  __ Goto(&done, ChangeInt64ToSmi(value));

  __ Bind(&if_not_in_smi_range);
  Node* number = AllocateHeapNumberWithValue(__ ChangeInt64ToFloat64(value));

  __ Goto(&done, number);
  __ Bind(&done);

  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeTaggedSignedToInt32(Node* node) {
  Node* value = node->InputAt(0);
  return ChangeSmiToInt32(value);
}

Node* EffectControlLinearizer::LowerChangeTaggedSignedToInt64(Node* node) {
  Node* value = node->InputAt(0);
  return ChangeSmiToInt64(value);
}

Node* EffectControlLinearizer::LowerChangeTaggedToBit(Node* node) {
  Node* value = node->InputAt(0);
  return __ WordEqual(value, __ TrueConstant());
}

void EffectControlLinearizer::TruncateTaggedPointerToBit(
    Node* node, GraphAssemblerLabel<1>* done) {
  Node* value = node->InputAt(0);

  auto if_heapnumber = __ MakeDeferredLabel();
  auto if_bigint = __ MakeDeferredLabel();

  Node* zero = __ Int32Constant(0);
  Node* fzero = __ Float64Constant(0.0);

  // Check if {value} is false.
  __ GotoIf(__ WordEqual(value, __ FalseConstant()), done, zero);

  // Check if {value} is the empty string.
  __ GotoIf(__ WordEqual(value, __ EmptyStringConstant()), done, zero);

  // Load the map of {value}.
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);

  // Check if the {value} is undetectable and immediately return false.
  // This includes undefined and null.
  Node* value_map_bitfield =
      __ LoadField(AccessBuilder::ForMapBitField(), value_map);
  __ GotoIfNot(
      __ Word32Equal(
          __ Word32And(value_map_bitfield,
                       __ Int32Constant(Map::IsUndetectableBit::kMask)),
          zero),
      done, zero);

  // Check if {value} is a HeapNumber.
  __ GotoIf(__ WordEqual(value_map, __ HeapNumberMapConstant()),
            &if_heapnumber);

  // Check if {value} is a BigInt.
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
  __ GotoIf(__ Word32Equal(value_instance_type, __ Int32Constant(BIGINT_TYPE)),
            &if_bigint);

  // All other values that reach here are true.
  __ Goto(done, __ Int32Constant(1));

  __ Bind(&if_heapnumber);
  {
    // For HeapNumber {value}, just check that its value is not 0.0, -0.0 or
    // NaN.
    Node* value_value =
        __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
    __ Goto(done, __ Float64LessThan(fzero, __ Float64Abs(value_value)));
  }

  __ Bind(&if_bigint);
  {
    Node* bitfield = __ LoadField(AccessBuilder::ForBigIntBitfield(), value);
    Node* length_is_zero = __ Word32Equal(
        __ Word32And(bitfield, __ Int32Constant(BigInt::LengthBits::kMask)),
        __ Int32Constant(0));
    __ Goto(done, __ Word32Equal(length_is_zero, zero));
  }
}

Node* EffectControlLinearizer::LowerTruncateTaggedToBit(Node* node) {
  auto done = __ MakeLabel(MachineRepresentation::kBit);
  auto if_smi = __ MakeDeferredLabel();

  Node* value = node->InputAt(0);
  __ GotoIf(ObjectIsSmi(value), &if_smi);

  TruncateTaggedPointerToBit(node, &done);

  __ Bind(&if_smi);
  {
    // If {value} is a Smi, then we only need to check that it's not zero.
    __ Goto(&done, __ Word32Equal(__ WordEqual(value, __ IntPtrConstant(0)),
                                  __ Int32Constant(0)));
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerTruncateTaggedPointerToBit(Node* node) {
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  TruncateTaggedPointerToBit(node, &done);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeTaggedToInt32(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoIfNot(check, &if_not_smi);
  __ Goto(&done, ChangeSmiToInt32(value));

  __ Bind(&if_not_smi);
  STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
  Node* vfalse = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  vfalse = __ ChangeFloat64ToInt32(vfalse);
  __ Goto(&done, vfalse);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeTaggedToUint32(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoIfNot(check, &if_not_smi);
  __ Goto(&done, ChangeSmiToInt32(value));

  __ Bind(&if_not_smi);
  STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
  Node* vfalse = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  vfalse = __ ChangeFloat64ToUint32(vfalse);
  __ Goto(&done, vfalse);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeTaggedToInt64(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kWord64);

  Node* check = ObjectIsSmi(value);
  __ GotoIfNot(check, &if_not_smi);
  __ Goto(&done, ChangeSmiToInt64(value));

  __ Bind(&if_not_smi);
  STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
  Node* vfalse = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  vfalse = __ ChangeFloat64ToInt64(vfalse);
  __ Goto(&done, vfalse);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeTaggedToFloat64(Node* node) {
  return LowerTruncateTaggedToFloat64(node);
}

Node* EffectControlLinearizer::LowerChangeTaggedToTaggedSigned(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoIfNot(check, &if_not_smi);
  __ Goto(&done, value);

  __ Bind(&if_not_smi);
  STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
  Node* vfalse = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  vfalse = __ ChangeFloat64ToInt32(vfalse);
  vfalse = ChangeInt32ToSmi(vfalse);
  __ Goto(&done, vfalse);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerTruncateTaggedToFloat64(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kFloat64);

  Node* check = ObjectIsSmi(value);
  __ GotoIfNot(check, &if_not_smi);
  Node* vtrue = ChangeSmiToInt32(value);
  vtrue = __ ChangeInt32ToFloat64(vtrue);
  __ Goto(&done, vtrue);

  __ Bind(&if_not_smi);
  STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
  Node* vfalse = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  __ Goto(&done, vfalse);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerPoisonIndex(Node* node) {
  Node* index = node->InputAt(0);
  if (mask_array_index_ == kMaskArrayIndex) {
    index = __ Word32PoisonOnSpeculation(index);
  }
  return index;
}

void EffectControlLinearizer::LowerCheckMaps(Node* node, Node* frame_state) {
  CheckMapsParameters const& p = CheckMapsParametersOf(node->op());
  Node* value = node->InputAt(0);

  ZoneHandleSet<Map> const& maps = p.maps();
  size_t const map_count = maps.size();

  if (p.flags() & CheckMapsFlag::kTryMigrateInstance) {
    auto done = __ MakeLabel();
    auto migrate = __ MakeDeferredLabel();

    // Load the current map of the {value}.
    Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);

    // Perform the map checks.
    for (size_t i = 0; i < map_count; ++i) {
      Node* map = __ HeapConstant(maps[i]);
      Node* check = __ WordEqual(value_map, map);
      if (i == map_count - 1) {
        __ Branch(check, &done, &migrate, IsSafetyCheck::kCriticalSafetyCheck);
      } else {
        auto next_map = __ MakeLabel();
        __ Branch(check, &done, &next_map, IsSafetyCheck::kCriticalSafetyCheck);
        __ Bind(&next_map);
      }
    }

    // Perform the (deferred) instance migration.
    __ Bind(&migrate);
    {
      // If map is not deprecated the migration attempt does not make sense.
      Node* bitfield3 =
          __ LoadField(AccessBuilder::ForMapBitField3(), value_map);
      Node* if_not_deprecated = __ WordEqual(
          __ Word32And(bitfield3,
                       __ Int32Constant(Map::IsDeprecatedBit::kMask)),
          __ Int32Constant(0));
      __ DeoptimizeIf(DeoptimizeReason::kWrongMap, p.feedback(),
                      if_not_deprecated, frame_state,
                      IsSafetyCheck::kCriticalSafetyCheck);

      Operator::Properties properties = Operator::kNoDeopt | Operator::kNoThrow;
      Runtime::FunctionId id = Runtime::kTryMigrateInstance;
      auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
          graph()->zone(), id, 1, properties, CallDescriptor::kNoFlags);
      Node* result = __ Call(call_descriptor, __ CEntryStubConstant(1), value,
                             __ ExternalConstant(ExternalReference::Create(id)),
                             __ Int32Constant(1), __ NoContextConstant());
      Node* check = ObjectIsSmi(result);
      __ DeoptimizeIf(DeoptimizeReason::kInstanceMigrationFailed, p.feedback(),
                      check, frame_state, IsSafetyCheck::kCriticalSafetyCheck);
    }

    // Reload the current map of the {value}.
    value_map = __ LoadField(AccessBuilder::ForMap(), value);

    // Perform the map checks again.
    for (size_t i = 0; i < map_count; ++i) {
      Node* map = __ HeapConstant(maps[i]);
      Node* check = __ WordEqual(value_map, map);
      if (i == map_count - 1) {
        __ DeoptimizeIfNot(DeoptimizeReason::kWrongMap, p.feedback(), check,
                           frame_state, IsSafetyCheck::kCriticalSafetyCheck);
      } else {
        auto next_map = __ MakeLabel();
        __ Branch(check, &done, &next_map, IsSafetyCheck::kCriticalSafetyCheck);
        __ Bind(&next_map);
      }
    }

    __ Goto(&done);
    __ Bind(&done);
  } else {
    auto done = __ MakeLabel();

    // Load the current map of the {value}.
    Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);

    for (size_t i = 0; i < map_count; ++i) {
      Node* map = __ HeapConstant(maps[i]);
      Node* check = __ WordEqual(value_map, map);
      if (i == map_count - 1) {
        __ DeoptimizeIfNot(DeoptimizeReason::kWrongMap, p.feedback(), check,
                           frame_state, IsSafetyCheck::kCriticalSafetyCheck);
      } else {
        auto next_map = __ MakeLabel();
        __ Branch(check, &done, &next_map, IsSafetyCheck::kCriticalSafetyCheck);
        __ Bind(&next_map);
      }
    }
    __ Goto(&done);
    __ Bind(&done);
  }
}

Node* EffectControlLinearizer::LowerCompareMaps(Node* node) {
  ZoneHandleSet<Map> const& maps = CompareMapsParametersOf(node->op()).maps();
  size_t const map_count = maps.size();
  Node* value = node->InputAt(0);

  auto done = __ MakeLabel(MachineRepresentation::kBit);

  // Load the current map of the {value}.
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);

  for (size_t i = 0; i < map_count; ++i) {
    Node* map = __ HeapConstant(maps[i]);
    Node* check = __ WordEqual(value_map, map);
    auto next_map = __ MakeLabel();
    auto passed = __ MakeLabel();
    __ Branch(check, &passed, &next_map, IsSafetyCheck::kCriticalSafetyCheck);

    __ Bind(&passed);
    __ Goto(&done, __ Int32Constant(1));

    __ Bind(&next_map);
  }
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerCheckNumber(Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());

  auto if_not_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel();

  Node* check0 = ObjectIsSmi(value);
  __ GotoIfNot(check0, &if_not_smi);
  __ Goto(&done);

  __ Bind(&if_not_smi);
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* check1 = __ WordEqual(value_map, __ HeapNumberMapConstant());
  __ DeoptimizeIfNot(DeoptimizeReason::kNotAHeapNumber, params.feedback(),
                     check1, frame_state);
  __ Goto(&done);

  __ Bind(&done);
  return value;
}

Node* EffectControlLinearizer::LowerCheckReceiver(Node* node,
                                                  Node* frame_state) {
  Node* value = node->InputAt(0);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);

  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  Node* check = __ Uint32LessThanOrEqual(
      __ Uint32Constant(FIRST_JS_RECEIVER_TYPE), value_instance_type);
  __ DeoptimizeIfNot(DeoptimizeReason::kNotAJavaScriptObject, VectorSlotPair(),
                     check, frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerCheckReceiverOrNullOrUndefined(
    Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);

  // Rule out all primitives except oddballs (true, false, undefined, null).
  STATIC_ASSERT(LAST_PRIMITIVE_TYPE == ODDBALL_TYPE);
  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  Node* check0 = __ Uint32LessThanOrEqual(__ Uint32Constant(ODDBALL_TYPE),
                                          value_instance_type);
  __ DeoptimizeIfNot(DeoptimizeReason::kNotAJavaScriptObjectOrNullOrUndefined,
                     VectorSlotPair(), check0, frame_state);

  // Rule out booleans.
  Node* check1 = __ WordEqual(value_map, __ BooleanMapConstant());
  __ DeoptimizeIf(DeoptimizeReason::kNotAJavaScriptObjectOrNullOrUndefined,
                  VectorSlotPair(), check1, frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerCheckSymbol(Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);

  Node* check =
      __ WordEqual(value_map, __ HeapConstant(factory()->symbol_map()));
  __ DeoptimizeIfNot(DeoptimizeReason::kNotASymbol, VectorSlotPair(), check,
                     frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerCheckString(Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);

  Node* check = __ Uint32LessThan(value_instance_type,
                                  __ Uint32Constant(FIRST_NONSTRING_TYPE));
  __ DeoptimizeIfNot(DeoptimizeReason::kNotAString, params.feedback(), check,
                     frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerCheckInternalizedString(Node* node,
                                                            Node* frame_state) {
  Node* value = node->InputAt(0);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);

  Node* check = __ Word32Equal(
      __ Word32And(value_instance_type,
                   __ Int32Constant(kIsNotStringMask | kIsNotInternalizedMask)),
      __ Int32Constant(kInternalizedTag));
  __ DeoptimizeIfNot(DeoptimizeReason::kWrongInstanceType, VectorSlotPair(),
                     check, frame_state);

  return value;
}

void EffectControlLinearizer::LowerCheckIf(Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckIfParameters& p = CheckIfParametersOf(node->op());
  __ DeoptimizeIfNot(p.reason(), p.feedback(), value, frame_state);
}

Node* EffectControlLinearizer::LowerStringConcat(Node* node) {
  Node* lhs = node->InputAt(1);
  Node* rhs = node->InputAt(2);

  Callable const callable =
      CodeFactory::StringAdd(isolate(), STRING_ADD_CHECK_NONE);
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), CallDescriptor::kNoFlags,
      Operator::kNoDeopt | Operator::kNoWrite | Operator::kNoThrow);

  Node* value =
      __ Call(call_descriptor, jsgraph()->HeapConstant(callable.code()), lhs,
              rhs, __ NoContextConstant());

  return value;
}

Node* EffectControlLinearizer::LowerCheckedInt32Add(Node* node,
                                                    Node* frame_state) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* value = __ Int32AddWithOverflow(lhs, rhs);
  Node* check = __ Projection(1, value);
  __ DeoptimizeIf(DeoptimizeReason::kOverflow, VectorSlotPair(), check,
                  frame_state);
  return __ Projection(0, value);
}

Node* EffectControlLinearizer::LowerCheckedInt32Sub(Node* node,
                                                    Node* frame_state) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* value = __ Int32SubWithOverflow(lhs, rhs);
  Node* check = __ Projection(1, value);
  __ DeoptimizeIf(DeoptimizeReason::kOverflow, VectorSlotPair(), check,
                  frame_state);
  return __ Projection(0, value);
}

Node* EffectControlLinearizer::LowerCheckedInt32Div(Node* node,
                                                    Node* frame_state) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);
  Node* zero = __ Int32Constant(0);

  // Check if the {rhs} is a known power of two.
  Int32Matcher m(rhs);
  if (m.IsPowerOf2()) {
    // Since we know that {rhs} is a power of two, we can perform a fast
    // check to see if the relevant least significant bits of the {lhs}
    // are all zero, and if so we know that we can perform a division
    // safely (and fast by doing an arithmetic - aka sign preserving -
    // right shift on {lhs}).
    int32_t divisor = m.Value();
    Node* mask = __ Int32Constant(divisor - 1);
    Node* shift = __ Int32Constant(WhichPowerOf2(divisor));
    Node* check = __ Word32Equal(__ Word32And(lhs, mask), zero);
    __ DeoptimizeIfNot(DeoptimizeReason::kLostPrecision, VectorSlotPair(),
                       check, frame_state);
    return __ Word32Sar(lhs, shift);
  } else {
    auto if_rhs_positive = __ MakeLabel();
    auto if_rhs_negative = __ MakeDeferredLabel();
    auto done = __ MakeLabel(MachineRepresentation::kWord32);

    // Check if {rhs} is positive (and not zero).
    Node* check_rhs_positive = __ Int32LessThan(zero, rhs);
    __ Branch(check_rhs_positive, &if_rhs_positive, &if_rhs_negative);

    __ Bind(&if_rhs_positive);
    {
      // Fast case, no additional checking required.
      __ Goto(&done, __ Int32Div(lhs, rhs));
    }

    __ Bind(&if_rhs_negative);
    {
      auto if_lhs_minint = __ MakeDeferredLabel();
      auto if_lhs_notminint = __ MakeLabel();

      // Check if {rhs} is zero.
      Node* check_rhs_zero = __ Word32Equal(rhs, zero);
      __ DeoptimizeIf(DeoptimizeReason::kDivisionByZero, VectorSlotPair(),
                      check_rhs_zero, frame_state);

      // Check if {lhs} is zero, as that would produce minus zero.
      Node* check_lhs_zero = __ Word32Equal(lhs, zero);
      __ DeoptimizeIf(DeoptimizeReason::kMinusZero, VectorSlotPair(),
                      check_lhs_zero, frame_state);

      // Check if {lhs} is kMinInt and {rhs} is -1, in which case we'd have
      // to return -kMinInt, which is not representable as Word32.
      Node* check_lhs_minint = graph()->NewNode(machine()->Word32Equal(), lhs,
                                                __ Int32Constant(kMinInt));
      __ Branch(check_lhs_minint, &if_lhs_minint, &if_lhs_notminint);

      __ Bind(&if_lhs_minint);
      {
        // Check that {rhs} is not -1, otherwise result would be -kMinInt.
        Node* check_rhs_minusone = __ Word32Equal(rhs, __ Int32Constant(-1));
        __ DeoptimizeIf(DeoptimizeReason::kOverflow, VectorSlotPair(),
                        check_rhs_minusone, frame_state);

        // Perform the actual integer division.
        __ Goto(&done, __ Int32Div(lhs, rhs));
      }

      __ Bind(&if_lhs_notminint);
      {
        // Perform the actual integer division.
        __ Goto(&done, __ Int32Div(lhs, rhs));
      }
    }

    __ Bind(&done);
    Node* value = done.PhiAt(0);

    // Check if the remainder is non-zero.
    Node* check = __ Word32Equal(lhs, __ Int32Mul(value, rhs));
    __ DeoptimizeIfNot(DeoptimizeReason::kLostPrecision, VectorSlotPair(),
                       check, frame_state);

    return value;
  }
}

Node* EffectControlLinearizer::BuildUint32Mod(Node* lhs, Node* rhs) {
  auto if_rhs_power_of_two = __ MakeLabel();
  auto done = __ MakeLabel(MachineRepresentation::kWord32);

  // Compute the mask for the {rhs}.
  Node* one = __ Int32Constant(1);
  Node* msk = __ Int32Sub(rhs, one);

  // Check if the {rhs} is a power of two.
  __ GotoIf(__ Word32Equal(__ Word32And(rhs, msk), __ Int32Constant(0)),
            &if_rhs_power_of_two);
  {
    // The {rhs} is not a power of two, do a generic Uint32Mod.
    __ Goto(&done, __ Uint32Mod(lhs, rhs));
  }

  __ Bind(&if_rhs_power_of_two);
  {
    // The {rhs} is a power of two, just do a fast bit masking.
    __ Goto(&done, __ Word32And(lhs, msk));
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerCheckedInt32Mod(Node* node,
                                                    Node* frame_state) {
  // General case for signed integer modulus, with optimization for (unknown)
  // power of 2 right hand side.
  //
  //   if rhs <= 0 then
  //     rhs = -rhs
  //     deopt if rhs == 0
  //   let msk = rhs - 1 in
  //   if lhs < 0 then
  //     let lhs_abs = -lsh in
  //     let res = if rhs & msk == 0 then
  //                 lhs_abs & msk
  //               else
  //                 lhs_abs % rhs in
  //     if lhs < 0 then
  //       deopt if res == 0
  //       -res
  //     else
  //       res
  //   else
  //     if rhs & msk == 0 then
  //       lhs & msk
  //     else
  //       lhs % rhs
  //
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  auto if_rhs_not_positive = __ MakeDeferredLabel();
  auto if_lhs_negative = __ MakeDeferredLabel();
  auto if_rhs_power_of_two = __ MakeLabel();
  auto rhs_checked = __ MakeLabel(MachineRepresentation::kWord32);
  auto done = __ MakeLabel(MachineRepresentation::kWord32);

  Node* zero = __ Int32Constant(0);

  // Check if {rhs} is not strictly positive.
  Node* check0 = __ Int32LessThanOrEqual(rhs, zero);
  __ GotoIf(check0, &if_rhs_not_positive);
  __ Goto(&rhs_checked, rhs);

  __ Bind(&if_rhs_not_positive);
  {
    // Negate {rhs}, might still produce a negative result in case of
    // -2^31, but that is handled safely below.
    Node* vtrue0 = __ Int32Sub(zero, rhs);

    // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
    __ DeoptimizeIf(DeoptimizeReason::kDivisionByZero, VectorSlotPair(),
                    __ Word32Equal(vtrue0, zero), frame_state);
    __ Goto(&rhs_checked, vtrue0);
  }

  __ Bind(&rhs_checked);
  rhs = rhs_checked.PhiAt(0);

  __ GotoIf(__ Int32LessThan(lhs, zero), &if_lhs_negative);
  {
    // The {lhs} is a non-negative integer.
    __ Goto(&done, BuildUint32Mod(lhs, rhs));
  }

  __ Bind(&if_lhs_negative);
  {
    // The {lhs} is a negative integer. This is very unlikely and
    // we intentionally don't use the BuildUint32Mod() here, which
    // would try to figure out whether {rhs} is a power of two,
    // since this is intended to be a slow-path.
    Node* res = __ Uint32Mod(__ Int32Sub(zero, lhs), rhs);

    // Check if we would have to return -0.
    __ DeoptimizeIf(DeoptimizeReason::kMinusZero, VectorSlotPair(),
                    __ Word32Equal(res, zero), frame_state);
    __ Goto(&done, __ Int32Sub(zero, res));
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerCheckedUint32Div(Node* node,
                                                     Node* frame_state) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);
  Node* zero = __ Int32Constant(0);

  // Check if the {rhs} is a known power of two.
  Uint32Matcher m(rhs);
  if (m.IsPowerOf2()) {
    // Since we know that {rhs} is a power of two, we can perform a fast
    // check to see if the relevant least significant bits of the {lhs}
    // are all zero, and if so we know that we can perform a division
    // safely (and fast by doing a logical - aka zero extending - right
    // shift on {lhs}).
    uint32_t divisor = m.Value();
    Node* mask = __ Uint32Constant(divisor - 1);
    Node* shift = __ Uint32Constant(WhichPowerOf2(divisor));
    Node* check = __ Word32Equal(__ Word32And(lhs, mask), zero);
    __ DeoptimizeIfNot(DeoptimizeReason::kLostPrecision, VectorSlotPair(),
                       check, frame_state);
    return __ Word32Shr(lhs, shift);
  } else {
    // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
    Node* check = __ Word32Equal(rhs, zero);
    __ DeoptimizeIf(DeoptimizeReason::kDivisionByZero, VectorSlotPair(), check,
                    frame_state);

    // Perform the actual unsigned integer division.
    Node* value = __ Uint32Div(lhs, rhs);

    // Check if the remainder is non-zero.
    check = __ Word32Equal(lhs, __ Int32Mul(rhs, value));
    __ DeoptimizeIfNot(DeoptimizeReason::kLostPrecision, VectorSlotPair(),
                       check, frame_state);
    return value;
  }
}

Node* EffectControlLinearizer::LowerCheckedUint32Mod(Node* node,
                                                     Node* frame_state) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* zero = __ Int32Constant(0);

  // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
  Node* check = __ Word32Equal(rhs, zero);
  __ DeoptimizeIf(DeoptimizeReason::kDivisionByZero, VectorSlotPair(), check,
                  frame_state);

  // Perform the actual unsigned integer modulus.
  return BuildUint32Mod(lhs, rhs);
}

Node* EffectControlLinearizer::LowerCheckedInt32Mul(Node* node,
                                                    Node* frame_state) {
  CheckForMinusZeroMode mode = CheckMinusZeroModeOf(node->op());
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* projection = __ Int32MulWithOverflow(lhs, rhs);
  Node* check = __ Projection(1, projection);
  __ DeoptimizeIf(DeoptimizeReason::kOverflow, VectorSlotPair(), check,
                  frame_state);

  Node* value = __ Projection(0, projection);

  if (mode == CheckForMinusZeroMode::kCheckForMinusZero) {
    auto if_zero = __ MakeDeferredLabel();
    auto check_done = __ MakeLabel();
    Node* zero = __ Int32Constant(0);
    Node* check_zero = __ Word32Equal(value, zero);
    __ GotoIf(check_zero, &if_zero);
    __ Goto(&check_done);

    __ Bind(&if_zero);
    // We may need to return negative zero.
    Node* check_or = __ Int32LessThan(__ Word32Or(lhs, rhs), zero);
    __ DeoptimizeIf(DeoptimizeReason::kMinusZero, VectorSlotPair(), check_or,
                    frame_state);
    __ Goto(&check_done);

    __ Bind(&check_done);
  }

  return value;
}

Node* EffectControlLinearizer::LowerCheckedInt32ToTaggedSigned(
    Node* node, Node* frame_state) {
  DCHECK(SmiValuesAre31Bits());
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());

  Node* add = __ Int32AddWithOverflow(value, value);
  Node* check = __ Projection(1, add);
  __ DeoptimizeIf(DeoptimizeReason::kLostPrecision, params.feedback(), check,
                  frame_state);
  Node* result = __ Projection(0, add);
  result = ChangeInt32ToIntPtr(result);
  return result;
}

Node* EffectControlLinearizer::LowerCheckedInt64ToInt32(Node* node,
                                                        Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());

  Node* value32 = __ TruncateInt64ToInt32(value);
  Node* check = __ Word64Equal(__ ChangeInt32ToInt64(value32), value);
  __ DeoptimizeIfNot(DeoptimizeReason::kLostPrecision, params.feedback(), check,
                     frame_state);
  return value32;
}

Node* EffectControlLinearizer::LowerCheckedInt64ToTaggedSigned(
    Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());

  Node* value32 = __ TruncateInt64ToInt32(value);
  Node* check = __ Word64Equal(__ ChangeInt32ToInt64(value32), value);
  __ DeoptimizeIfNot(DeoptimizeReason::kLostPrecision, params.feedback(), check,
                     frame_state);

  if (SmiValuesAre32Bits()) {
    return ChangeInt64ToSmi(value);
  } else {
    Node* add = __ Int32AddWithOverflow(value32, value32);
    Node* check = __ Projection(1, add);
    __ DeoptimizeIf(DeoptimizeReason::kLostPrecision, params.feedback(), check,
                    frame_state);
    Node* result = __ Projection(0, add);
    result = ChangeInt32ToIntPtr(result);
    return result;
  }
}

Node* EffectControlLinearizer::LowerCheckedUint32Bounds(Node* node,
                                                        Node* frame_state) {
  Node* index = node->InputAt(0);
  Node* limit = node->InputAt(1);
  const CheckParameters& params = CheckParametersOf(node->op());

  Node* check = __ Uint32LessThan(index, limit);
  __ DeoptimizeIfNot(DeoptimizeReason::kOutOfBounds, params.feedback(), check,
                     frame_state, IsSafetyCheck::kCriticalSafetyCheck);
  return index;
}

Node* EffectControlLinearizer::LowerCheckedUint32ToInt32(Node* node,
                                                         Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());
  Node* unsafe = __ Int32LessThan(value, __ Int32Constant(0));
  __ DeoptimizeIf(DeoptimizeReason::kLostPrecision, params.feedback(), unsafe,
                  frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerCheckedUint32ToTaggedSigned(
    Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());
  Node* check = __ Uint32LessThanOrEqual(value, SmiMaxValueConstant());
  __ DeoptimizeIfNot(DeoptimizeReason::kLostPrecision, params.feedback(), check,
                     frame_state);
  return ChangeUint32ToSmi(value);
}

Node* EffectControlLinearizer::LowerCheckedUint64Bounds(Node* node,
                                                        Node* frame_state) {
  CheckParameters const& params = CheckParametersOf(node->op());
  Node* const index = node->InputAt(0);
  Node* const limit = node->InputAt(1);

  Node* check = __ Uint64LessThan(index, limit);
  __ DeoptimizeIfNot(DeoptimizeReason::kOutOfBounds, params.feedback(), check,
                     frame_state, IsSafetyCheck::kCriticalSafetyCheck);
  return index;
}

Node* EffectControlLinearizer::LowerCheckedUint64ToInt32(Node* node,
                                                         Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());

  Node* check = __ Uint64LessThanOrEqual(value, __ Int64Constant(kMaxInt));
  __ DeoptimizeIfNot(DeoptimizeReason::kLostPrecision, params.feedback(), check,
                     frame_state);
  return __ TruncateInt64ToInt32(value);
}

Node* EffectControlLinearizer::LowerCheckedUint64ToTaggedSigned(
    Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());

  Node* check =
      __ Uint64LessThanOrEqual(value, __ Int64Constant(Smi::kMaxValue));
  __ DeoptimizeIfNot(DeoptimizeReason::kLostPrecision, params.feedback(), check,
                     frame_state);
  return ChangeInt64ToSmi(value);
}

Node* EffectControlLinearizer::BuildCheckedFloat64ToInt32(
    CheckForMinusZeroMode mode, const VectorSlotPair& feedback, Node* value,
    Node* frame_state) {
  Node* value32 = __ RoundFloat64ToInt32(value);
  Node* check_same = __ Float64Equal(value, __ ChangeInt32ToFloat64(value32));
  __ DeoptimizeIfNot(DeoptimizeReason::kLostPrecisionOrNaN, feedback,
                     check_same, frame_state);

  if (mode == CheckForMinusZeroMode::kCheckForMinusZero) {
    // Check if {value} is -0.
    auto if_zero = __ MakeDeferredLabel();
    auto check_done = __ MakeLabel();

    Node* check_zero = __ Word32Equal(value32, __ Int32Constant(0));
    __ GotoIf(check_zero, &if_zero);
    __ Goto(&check_done);

    __ Bind(&if_zero);
    // In case of 0, we need to check the high bits for the IEEE -0 pattern.
    Node* check_negative = __ Int32LessThan(__ Float64ExtractHighWord32(value),
                                            __ Int32Constant(0));
    __ DeoptimizeIf(DeoptimizeReason::kMinusZero, feedback, check_negative,
                    frame_state);
    __ Goto(&check_done);

    __ Bind(&check_done);
  }
  return value32;
}

Node* EffectControlLinearizer::LowerCheckedFloat64ToInt32(Node* node,
                                                          Node* frame_state) {
  const CheckMinusZeroParameters& params =
      CheckMinusZeroParametersOf(node->op());
  Node* value = node->InputAt(0);
  return BuildCheckedFloat64ToInt32(params.mode(), params.feedback(), value,
                                    frame_state);
}

Node* EffectControlLinearizer::BuildCheckedFloat64ToInt64(
    CheckForMinusZeroMode mode, const VectorSlotPair& feedback, Node* value,
    Node* frame_state) {
  Node* value64 = __ TruncateFloat64ToInt64(value);
  Node* check_same = __ Float64Equal(value, __ ChangeInt64ToFloat64(value64));
  __ DeoptimizeIfNot(DeoptimizeReason::kLostPrecisionOrNaN, feedback,
                     check_same, frame_state);

  if (mode == CheckForMinusZeroMode::kCheckForMinusZero) {
    // Check if {value} is -0.
    auto if_zero = __ MakeDeferredLabel();
    auto check_done = __ MakeLabel();

    Node* check_zero = __ Word64Equal(value64, __ Int64Constant(0));
    __ GotoIf(check_zero, &if_zero);
    __ Goto(&check_done);

    __ Bind(&if_zero);
    // In case of 0, we need to check the high bits for the IEEE -0 pattern.
    Node* check_negative = __ Int32LessThan(__ Float64ExtractHighWord32(value),
                                            __ Int32Constant(0));
    __ DeoptimizeIf(DeoptimizeReason::kMinusZero, feedback, check_negative,
                    frame_state);
    __ Goto(&check_done);

    __ Bind(&check_done);
  }
  return value64;
}

Node* EffectControlLinearizer::LowerCheckedFloat64ToInt64(Node* node,
                                                          Node* frame_state) {
  const CheckMinusZeroParameters& params =
      CheckMinusZeroParametersOf(node->op());
  Node* value = node->InputAt(0);
  return BuildCheckedFloat64ToInt64(params.mode(), params.feedback(), value,
                                    frame_state);
}

Node* EffectControlLinearizer::LowerCheckedTaggedSignedToInt32(
    Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());
  Node* check = ObjectIsSmi(value);
  __ DeoptimizeIfNot(DeoptimizeReason::kNotASmi, params.feedback(), check,
                     frame_state);
  return ChangeSmiToInt32(value);
}

Node* EffectControlLinearizer::LowerCheckedTaggedToInt32(Node* node,
                                                         Node* frame_state) {
  const CheckMinusZeroParameters& params =
      CheckMinusZeroParametersOf(node->op());
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoIfNot(check, &if_not_smi);
  // In the Smi case, just convert to int32.
  __ Goto(&done, ChangeSmiToInt32(value));

  // In the non-Smi case, check the heap numberness, load the number and convert
  // to int32.
  __ Bind(&if_not_smi);
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* check_map = __ WordEqual(value_map, __ HeapNumberMapConstant());
  __ DeoptimizeIfNot(DeoptimizeReason::kNotAHeapNumber, params.feedback(),
                     check_map, frame_state);
  Node* vfalse = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  vfalse = BuildCheckedFloat64ToInt32(params.mode(), params.feedback(), vfalse,
                                      frame_state);
  __ Goto(&done, vfalse);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerCheckedTaggedToInt64(Node* node,
                                                         Node* frame_state) {
  const CheckMinusZeroParameters& params =
      CheckMinusZeroParametersOf(node->op());
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kWord64);

  Node* check = ObjectIsSmi(value);
  __ GotoIfNot(check, &if_not_smi);
  // In the Smi case, just convert to int64.
  __ Goto(&done, ChangeSmiToInt64(value));

  // In the non-Smi case, check the heap numberness, load the number and convert
  // to int64.
  __ Bind(&if_not_smi);
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* check_map = __ WordEqual(value_map, __ HeapNumberMapConstant());
  __ DeoptimizeIfNot(DeoptimizeReason::kNotAHeapNumber, params.feedback(),
                     check_map, frame_state);
  Node* vfalse = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  vfalse = BuildCheckedFloat64ToInt64(params.mode(), params.feedback(), vfalse,
                                      frame_state);
  __ Goto(&done, vfalse);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::BuildCheckedHeapNumberOrOddballToFloat64(
    CheckTaggedInputMode mode, const VectorSlotPair& feedback, Node* value,
    Node* frame_state) {
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* check_number = __ WordEqual(value_map, __ HeapNumberMapConstant());
  switch (mode) {
    case CheckTaggedInputMode::kNumber: {
      __ DeoptimizeIfNot(DeoptimizeReason::kNotAHeapNumber, feedback,
                         check_number, frame_state);
      break;
    }
    case CheckTaggedInputMode::kNumberOrOddball: {
      auto check_done = __ MakeLabel();

      __ GotoIf(check_number, &check_done);
      // For oddballs also contain the numeric value, let us just check that
      // we have an oddball here.
      Node* instance_type =
          __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
      Node* check_oddball =
          __ Word32Equal(instance_type, __ Int32Constant(ODDBALL_TYPE));
      __ DeoptimizeIfNot(DeoptimizeReason::kNotANumberOrOddball, feedback,
                         check_oddball, frame_state);
      STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
      __ Goto(&check_done);

      __ Bind(&check_done);
      break;
    }
  }
  return __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
}

Node* EffectControlLinearizer::LowerCheckedTaggedToFloat64(Node* node,
                                                           Node* frame_state) {
  CheckTaggedInputParameters const& p =
      CheckTaggedInputParametersOf(node->op());
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeLabel();
  auto done = __ MakeLabel(MachineRepresentation::kFloat64);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);

  // In the Smi case, just convert to int32 and then float64.
  // Otherwise, check heap numberness and load the number.
  Node* number = BuildCheckedHeapNumberOrOddballToFloat64(
      p.mode(), p.feedback(), value, frame_state);
  __ Goto(&done, number);

  __ Bind(&if_smi);
  Node* from_smi = ChangeSmiToInt32(value);
  from_smi = __ ChangeInt32ToFloat64(from_smi);
  __ Goto(&done, from_smi);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerCheckedTaggedToTaggedSigned(
    Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());

  Node* check = ObjectIsSmi(value);
  __ DeoptimizeIfNot(DeoptimizeReason::kNotASmi, params.feedback(), check,
                     frame_state);

  return value;
}

Node* EffectControlLinearizer::LowerCheckedTaggedToTaggedPointer(
    Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  const CheckParameters& params = CheckParametersOf(node->op());

  Node* check = ObjectIsSmi(value);
  __ DeoptimizeIf(DeoptimizeReason::kSmi, params.feedback(), check,
                  frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerTruncateTaggedToWord32(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoIfNot(check, &if_not_smi);
  __ Goto(&done, ChangeSmiToInt32(value));

  __ Bind(&if_not_smi);
  STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
  Node* vfalse = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  vfalse = __ TruncateFloat64ToWord32(vfalse);
  __ Goto(&done, vfalse);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerCheckedTruncateTaggedToWord32(
    Node* node, Node* frame_state) {
  const CheckTaggedInputParameters& params =
      CheckTaggedInputParametersOf(node->op());
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeLabel();
  auto done = __ MakeLabel(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoIfNot(check, &if_not_smi);
  // In the Smi case, just convert to int32.
  __ Goto(&done, ChangeSmiToInt32(value));

  // Otherwise, check that it's a heap number or oddball and truncate the value
  // to int32.
  __ Bind(&if_not_smi);
  Node* number = BuildCheckedHeapNumberOrOddballToFloat64(
      params.mode(), params.feedback(), value, frame_state);
  number = __ TruncateFloat64ToWord32(number);
  __ Goto(&done, number);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerAllocate(Node* node) {
  Node* size = node->InputAt(0);
  PretenureFlag pretenure = PretenureFlagOf(node->op());
  Node* new_node = __ Allocate(pretenure, size);
  return new_node;
}

Node* EffectControlLinearizer::LowerNumberToString(Node* node) {
  Node* argument = node->InputAt(0);

  Callable const callable =
      Builtins::CallableFor(isolate(), Builtins::kNumberToString);
  Operator::Properties properties = Operator::kEliminatable;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  return __ Call(call_descriptor, __ HeapConstant(callable.code()), argument,
                 __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerObjectIsArrayBufferView(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
  STATIC_ASSERT(JS_TYPED_ARRAY_TYPE + 1 == JS_DATA_VIEW_TYPE);
  Node* vfalse = __ Uint32LessThan(
      __ Int32Sub(value_instance_type, __ Int32Constant(JS_TYPED_ARRAY_TYPE)),
      __ Int32Constant(2));
  __ Goto(&done, vfalse);

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsBigInt(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
  Node* vfalse =
      __ Word32Equal(value_instance_type, __ Uint32Constant(BIGINT_TYPE));
  __ Goto(&done, vfalse);

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsCallable(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_bit_field =
      __ LoadField(AccessBuilder::ForMapBitField(), value_map);
  Node* vfalse =
      __ Word32Equal(__ Int32Constant(Map::IsCallableBit::kMask),
                     __ Word32And(value_bit_field,
                                  __ Int32Constant(Map::IsCallableBit::kMask)));
  __ Goto(&done, vfalse);

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsConstructor(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_bit_field =
      __ LoadField(AccessBuilder::ForMapBitField(), value_map);
  Node* vfalse = __ Word32Equal(
      __ Int32Constant(Map::IsConstructorBit::kMask),
      __ Word32And(value_bit_field,
                   __ Int32Constant(Map::IsConstructorBit::kMask)));
  __ Goto(&done, vfalse);

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsDetectableCallable(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_bit_field =
      __ LoadField(AccessBuilder::ForMapBitField(), value_map);
  Node* vfalse = __ Word32Equal(
      __ Int32Constant(Map::IsCallableBit::kMask),
      __ Word32And(value_bit_field,
                   __ Int32Constant((Map::IsCallableBit::kMask) |
                                    (Map::IsUndetectableBit::kMask))));
  __ Goto(&done, vfalse);

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerNumberIsFloat64Hole(Node* node) {
  Node* value = node->InputAt(0);
  Node* check = __ Word32Equal(__ Float64ExtractHighWord32(value),
                               __ Int32Constant(kHoleNanUpper32));
  return check;
}

Node* EffectControlLinearizer::LowerNumberIsFinite(Node* node) {
  Node* number = node->InputAt(0);
  Node* diff = __ Float64Sub(number, number);
  Node* check = __ Float64Equal(diff, diff);
  return check;
}

Node* EffectControlLinearizer::LowerObjectIsFiniteNumber(Node* node) {
  Node* object = node->InputAt(0);
  Node* zero = __ Int32Constant(0);
  Node* one = __ Int32Constant(1);

  auto done = __ MakeLabel(MachineRepresentation::kBit);

  // Check if {object} is a Smi.
  __ GotoIf(ObjectIsSmi(object), &done, one);

  // Check if {object} is a HeapNumber.
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), object);
  __ GotoIfNot(__ WordEqual(value_map, __ HeapNumberMapConstant()), &done,
               zero);

  // {object} is a HeapNumber.
  Node* value = __ LoadField(AccessBuilder::ForHeapNumberValue(), object);
  Node* diff = __ Float64Sub(value, value);
  Node* check = __ Float64Equal(diff, diff);
  __ Goto(&done, check);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerNumberIsInteger(Node* node) {
  Node* number = node->InputAt(0);
  Node* trunc = BuildFloat64RoundTruncate(number);
  Node* diff = __ Float64Sub(number, trunc);
  Node* check = __ Float64Equal(diff, __ Float64Constant(0));
  return check;
}

Node* EffectControlLinearizer::LowerObjectIsInteger(Node* node) {
  Node* object = node->InputAt(0);
  Node* zero = __ Int32Constant(0);
  Node* one = __ Int32Constant(1);

  auto done = __ MakeLabel(MachineRepresentation::kBit);

  // Check if {object} is a Smi.
  __ GotoIf(ObjectIsSmi(object), &done, one);

  // Check if {object} is a HeapNumber.
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), object);
  __ GotoIfNot(__ WordEqual(value_map, __ HeapNumberMapConstant()), &done,
               zero);

  // {object} is a HeapNumber.
  Node* value = __ LoadField(AccessBuilder::ForHeapNumberValue(), object);
  Node* trunc = BuildFloat64RoundTruncate(value);
  Node* diff = __ Float64Sub(value, trunc);
  Node* check = __ Float64Equal(diff, __ Float64Constant(0));
  __ Goto(&done, check);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerNumberIsSafeInteger(Node* node) {
  Node* number = node->InputAt(0);
  Node* zero = __ Int32Constant(0);
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  Node* trunc = BuildFloat64RoundTruncate(number);
  Node* diff = __ Float64Sub(number, trunc);
  Node* check = __ Float64Equal(diff, __ Float64Constant(0));
  __ GotoIfNot(check, &done, zero);
  Node* in_range = __ Float64LessThanOrEqual(
      __ Float64Abs(trunc), __ Float64Constant(kMaxSafeInteger));
  __ Goto(&done, in_range);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsSafeInteger(Node* node) {
  Node* object = node->InputAt(0);
  Node* zero = __ Int32Constant(0);
  Node* one = __ Int32Constant(1);

  auto done = __ MakeLabel(MachineRepresentation::kBit);

  // Check if {object} is a Smi.
  __ GotoIf(ObjectIsSmi(object), &done, one);

  // Check if {object} is a HeapNumber.
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), object);
  __ GotoIfNot(__ WordEqual(value_map, __ HeapNumberMapConstant()), &done,
               zero);

  // {object} is a HeapNumber.
  Node* value = __ LoadField(AccessBuilder::ForHeapNumberValue(), object);
  Node* trunc = BuildFloat64RoundTruncate(value);
  Node* diff = __ Float64Sub(value, trunc);
  Node* check = __ Float64Equal(diff, __ Float64Constant(0));
  __ GotoIfNot(check, &done, zero);
  Node* in_range = __ Float64LessThanOrEqual(
      __ Float64Abs(trunc), __ Float64Constant(kMaxSafeInteger));
  __ Goto(&done, in_range);

  __ Bind(&done);
  return done.PhiAt(0);
}

namespace {

const int64_t kMinusZeroBits = bit_cast<int64_t>(-0.0);
const int32_t kMinusZeroLoBits = static_cast<int32_t>(kMinusZeroBits);
const int32_t kMinusZeroHiBits = static_cast<int32_t>(kMinusZeroBits >> 32);

}  // namespace

Node* EffectControlLinearizer::LowerObjectIsMinusZero(Node* node) {
  Node* value = node->InputAt(0);
  Node* zero = __ Int32Constant(0);

  auto done = __ MakeLabel(MachineRepresentation::kBit);

  // Check if {value} is a Smi.
  __ GotoIf(ObjectIsSmi(value), &done, zero);

  // Check if {value} is a HeapNumber.
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  __ GotoIfNot(__ WordEqual(value_map, __ HeapNumberMapConstant()), &done,
               zero);

  // Check if {value} contains -0.
  Node* value_value = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  if (machine()->Is64()) {
    Node* value64 = __ BitcastFloat64ToInt64(value_value);
    __ Goto(&done, __ Word64Equal(value64, __ Int64Constant(kMinusZeroBits)));
  } else {
    Node* value_lo = __ Float64ExtractLowWord32(value_value);
    __ GotoIfNot(__ Word32Equal(value_lo, __ Int32Constant(kMinusZeroLoBits)),
                 &done, zero);
    Node* value_hi = __ Float64ExtractHighWord32(value_value);
    __ Goto(&done,
            __ Word32Equal(value_hi, __ Int32Constant(kMinusZeroHiBits)));
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerNumberIsMinusZero(Node* node) {
  Node* value = node->InputAt(0);

  if (machine()->Is64()) {
    Node* value64 = __ BitcastFloat64ToInt64(value);
    return __ Word64Equal(value64, __ Int64Constant(kMinusZeroBits));
  } else {
    auto done = __ MakeLabel(MachineRepresentation::kBit);

    Node* value_lo = __ Float64ExtractLowWord32(value);
    __ GotoIfNot(__ Word32Equal(value_lo, __ Int32Constant(kMinusZeroLoBits)),
                 &done, __ Int32Constant(0));
    Node* value_hi = __ Float64ExtractHighWord32(value);
    __ Goto(&done,
            __ Word32Equal(value_hi, __ Int32Constant(kMinusZeroHiBits)));

    __ Bind(&done);
    return done.PhiAt(0);
  }
}

Node* EffectControlLinearizer::LowerObjectIsNaN(Node* node) {
  Node* value = node->InputAt(0);
  Node* zero = __ Int32Constant(0);

  auto done = __ MakeLabel(MachineRepresentation::kBit);

  // Check if {value} is a Smi.
  __ GotoIf(ObjectIsSmi(value), &done, zero);

  // Check if {value} is a HeapNumber.
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  __ GotoIfNot(__ WordEqual(value_map, __ HeapNumberMapConstant()), &done,
               zero);

  // Check if {value} contains a NaN.
  Node* value_value = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  __ Goto(&done,
          __ Word32Equal(__ Float64Equal(value_value, value_value), zero));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerNumberIsNaN(Node* node) {
  Node* number = node->InputAt(0);
  Node* diff = __ Float64Equal(number, number);
  Node* check = __ Word32Equal(diff, __ Int32Constant(0));
  return check;
}

Node* EffectControlLinearizer::LowerObjectIsNonCallable(Node* node) {
  Node* value = node->InputAt(0);

  auto if_primitive = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  Node* check0 = ObjectIsSmi(value);
  __ GotoIf(check0, &if_primitive);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  Node* check1 = __ Uint32LessThanOrEqual(
      __ Uint32Constant(FIRST_JS_RECEIVER_TYPE), value_instance_type);
  __ GotoIfNot(check1, &if_primitive);

  Node* value_bit_field =
      __ LoadField(AccessBuilder::ForMapBitField(), value_map);
  Node* check2 =
      __ Word32Equal(__ Int32Constant(0),
                     __ Word32And(value_bit_field,
                                  __ Int32Constant(Map::IsCallableBit::kMask)));
  __ Goto(&done, check2);

  __ Bind(&if_primitive);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsNumber(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeLabel();
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  __ GotoIf(ObjectIsSmi(value), &if_smi);
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  __ Goto(&done, __ WordEqual(value_map, __ HeapNumberMapConstant()));

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(1));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsReceiver(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  __ GotoIf(ObjectIsSmi(value), &if_smi);

  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
  Node* result = __ Uint32LessThanOrEqual(
      __ Uint32Constant(FIRST_JS_RECEIVER_TYPE), value_instance_type);
  __ Goto(&done, result);

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsSmi(Node* node) {
  Node* value = node->InputAt(0);
  return ObjectIsSmi(value);
}

Node* EffectControlLinearizer::LowerObjectIsString(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
  Node* vfalse = __ Uint32LessThan(value_instance_type,
                                   __ Uint32Constant(FIRST_NONSTRING_TYPE));
  __ Goto(&done, vfalse);

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsSymbol(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
  Node* vfalse =
      __ Word32Equal(value_instance_type, __ Uint32Constant(SYMBOL_TYPE));
  __ Goto(&done, vfalse);

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsUndetectable(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kBit);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_bit_field =
      __ LoadField(AccessBuilder::ForMapBitField(), value_map);
  Node* vfalse = __ Word32Equal(
      __ Word32Equal(
          __ Int32Constant(0),
          __ Word32And(value_bit_field,
                       __ Int32Constant(Map::IsUndetectableBit::kMask))),
      __ Int32Constant(0));
  __ Goto(&done, vfalse);

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerTypeOf(Node* node) {
  Node* obj = node->InputAt(0);
  Callable const callable = Builtins::CallableFor(isolate(), Builtins::kTypeof);
  Operator::Properties const properties = Operator::kEliminatable;
  CallDescriptor::Flags const flags = CallDescriptor::kNoAllocate;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  return __ Call(call_descriptor, __ HeapConstant(callable.code()), obj,
                 __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerToBoolean(Node* node) {
  Node* obj = node->InputAt(0);
  Callable const callable =
      Builtins::CallableFor(isolate(), Builtins::kToBoolean);
  Operator::Properties const properties = Operator::kEliminatable;
  CallDescriptor::Flags const flags = CallDescriptor::kNoAllocate;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  return __ Call(call_descriptor, __ HeapConstant(callable.code()), obj,
                 __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerArgumentsLength(Node* node) {
  Node* arguments_frame = NodeProperties::GetValueInput(node, 0);
  int formal_parameter_count = FormalParameterCountOf(node->op());
  bool is_rest_length = IsRestLengthOf(node->op());
  DCHECK_LE(0, formal_parameter_count);

  if (is_rest_length) {
    // The ArgumentsLength node is computing the number of rest parameters,
    // which is max(0, actual_parameter_count - formal_parameter_count).
    // We have to distinguish the case, when there is an arguments adaptor frame
    // (i.e., arguments_frame != LoadFramePointer()).
    auto if_adaptor_frame = __ MakeLabel();
    auto done = __ MakeLabel(MachineRepresentation::kTaggedSigned);

    Node* frame = __ LoadFramePointer();
    __ GotoIf(__ WordEqual(arguments_frame, frame), &done, __ SmiConstant(0));
    __ Goto(&if_adaptor_frame);

    __ Bind(&if_adaptor_frame);
    Node* arguments_length = __ Load(
        MachineType::TaggedSigned(), arguments_frame,
        __ IntPtrConstant(ArgumentsAdaptorFrameConstants::kLengthOffset));

    Node* rest_length =
        __ IntSub(arguments_length, __ SmiConstant(formal_parameter_count));
    __ GotoIf(__ IntLessThan(rest_length, __ SmiConstant(0)), &done,
              __ SmiConstant(0));
    __ Goto(&done, rest_length);

    __ Bind(&done);
    return done.PhiAt(0);
  } else {
    // The ArgumentsLength node is computing the actual number of arguments.
    // We have to distinguish the case when there is an arguments adaptor frame
    // (i.e., arguments_frame != LoadFramePointer()).
    auto if_adaptor_frame = __ MakeLabel();
    auto done = __ MakeLabel(MachineRepresentation::kTaggedSigned);

    Node* frame = __ LoadFramePointer();
    __ GotoIf(__ WordEqual(arguments_frame, frame), &done,
              __ SmiConstant(formal_parameter_count));
    __ Goto(&if_adaptor_frame);

    __ Bind(&if_adaptor_frame);
    Node* arguments_length = __ Load(
        MachineType::TaggedSigned(), arguments_frame,
        __ IntPtrConstant(ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ Goto(&done, arguments_length);

    __ Bind(&done);
    return done.PhiAt(0);
  }
}

Node* EffectControlLinearizer::LowerArgumentsFrame(Node* node) {
  auto done = __ MakeLabel(MachineType::PointerRepresentation());

  Node* frame = __ LoadFramePointer();
  Node* parent_frame =
      __ Load(MachineType::Pointer(), frame,
              __ IntPtrConstant(StandardFrameConstants::kCallerFPOffset));
  Node* parent_frame_type = __ Load(
      MachineType::AnyTagged(), parent_frame,
      __ IntPtrConstant(CommonFrameConstants::kContextOrFrameTypeOffset));
  __ GotoIf(__ WordEqual(parent_frame_type,
                         __ IntPtrConstant(StackFrame::TypeToMarker(
                             StackFrame::ARGUMENTS_ADAPTOR))),
            &done, parent_frame);
  __ Goto(&done, frame);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerNewDoubleElements(Node* node) {
  PretenureFlag const pretenure = PretenureFlagOf(node->op());
  Node* length = node->InputAt(0);

  auto done = __ MakeLabel(MachineRepresentation::kTaggedPointer);
  Node* zero_length = __ Word32Equal(length, __ Int32Constant(0));
  __ GotoIf(zero_length, &done,
            jsgraph()->HeapConstant(factory()->empty_fixed_array()));

  // Compute the effective size of the backing store.
  Node* size =
      __ Int32Add(__ Word32Shl(length, __ Int32Constant(kDoubleSizeLog2)),
                  __ Int32Constant(FixedDoubleArray::kHeaderSize));

  // Allocate the result and initialize the header.
  Node* result = __ Allocate(pretenure, size);
  __ StoreField(AccessBuilder::ForMap(), result,
                __ FixedDoubleArrayMapConstant());
  __ StoreField(AccessBuilder::ForFixedArrayLength(), result,
                ChangeInt32ToSmi(length));

  // Initialize the backing store with holes.
  STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
  Node* limit = ChangeUint32ToUintPtr(length);
  Node* the_hole =
      __ LoadField(AccessBuilder::ForHeapNumberValue(), __ TheHoleConstant());
  auto loop = __ MakeLoopLabel(MachineType::PointerRepresentation());
  __ Goto(&loop, __ IntPtrConstant(0));
  __ Bind(&loop);
  {
    // Check if we've initialized everything.
    Node* index = loop.PhiAt(0);
    Node* check = __ UintLessThan(index, limit);
    __ GotoIfNot(check, &done, result);

    // Storing "the_hole" doesn't need a write barrier.
    StoreRepresentation rep(MachineRepresentation::kFloat64, kNoWriteBarrier);
    Node* offset = __ IntAdd(
        __ WordShl(index, __ IntPtrConstant(kDoubleSizeLog2)),
        __ IntPtrConstant(FixedDoubleArray::kHeaderSize - kHeapObjectTag));
    __ Store(rep, result, offset, the_hole);

    // Advance the {index}.
    index = __ IntAdd(index, __ IntPtrConstant(1));
    __ Goto(&loop, index);
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerNewSmiOrObjectElements(Node* node) {
  PretenureFlag const pretenure = PretenureFlagOf(node->op());
  Node* length = node->InputAt(0);

  auto done = __ MakeLabel(MachineRepresentation::kTaggedPointer);
  Node* zero_length = __ Word32Equal(length, __ Int32Constant(0));
  __ GotoIf(zero_length, &done,
            jsgraph()->HeapConstant(factory()->empty_fixed_array()));

  // Compute the effective size of the backing store.
  Node* size =
      __ Int32Add(__ Word32Shl(length, __ Int32Constant(kTaggedSizeLog2)),
                  __ Int32Constant(FixedArray::kHeaderSize));

  // Allocate the result and initialize the header.
  Node* result = __ Allocate(pretenure, size);
  __ StoreField(AccessBuilder::ForMap(), result, __ FixedArrayMapConstant());
  __ StoreField(AccessBuilder::ForFixedArrayLength(), result,
                ChangeInt32ToSmi(length));

  // Initialize the backing store with holes.
  Node* limit = ChangeUint32ToUintPtr(length);
  Node* the_hole = __ TheHoleConstant();
  auto loop = __ MakeLoopLabel(MachineType::PointerRepresentation());
  __ Goto(&loop, __ IntPtrConstant(0));
  __ Bind(&loop);
  {
    // Check if we've initialized everything.
    Node* index = loop.PhiAt(0);
    Node* check = __ UintLessThan(index, limit);
    __ GotoIfNot(check, &done, result);

    // Storing "the_hole" doesn't need a write barrier.
    StoreRepresentation rep(MachineRepresentation::kTagged, kNoWriteBarrier);
    Node* offset =
        __ IntAdd(__ WordShl(index, __ IntPtrConstant(kTaggedSizeLog2)),
                  __ IntPtrConstant(FixedArray::kHeaderSize - kHeapObjectTag));
    __ Store(rep, result, offset, the_hole);

    // Advance the {index}.
    index = __ IntAdd(index, __ IntPtrConstant(1));
    __ Goto(&loop, index);
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerNewArgumentsElements(Node* node) {
  Node* frame = NodeProperties::GetValueInput(node, 0);
  Node* length = NodeProperties::GetValueInput(node, 1);
  int mapped_count = NewArgumentsElementsMappedCountOf(node->op());

  Callable const callable =
      Builtins::CallableFor(isolate(), Builtins::kNewArgumentsElements);
  Operator::Properties const properties = node->op()->properties();
  CallDescriptor::Flags const flags = CallDescriptor::kNoFlags;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  return __ Call(call_descriptor, __ HeapConstant(callable.code()), frame,
                 length, __ SmiConstant(mapped_count), __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerNewConsString(Node* node) {
  Node* length = node->InputAt(0);
  Node* first = node->InputAt(1);
  Node* second = node->InputAt(2);

  // Determine the instance types of {first} and {second}.
  Node* first_map = __ LoadField(AccessBuilder::ForMap(), first);
  Node* first_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), first_map);
  Node* second_map = __ LoadField(AccessBuilder::ForMap(), second);
  Node* second_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), second_map);

  // Determine the proper map for the resulting ConsString.
  // If both {first} and {second} are one-byte strings, we
  // create a new ConsOneByteString, otherwise we create a
  // new ConsString instead.
  auto if_onebyte = __ MakeLabel();
  auto if_twobyte = __ MakeLabel();
  auto done = __ MakeLabel(MachineRepresentation::kTaggedPointer);
  STATIC_ASSERT(kOneByteStringTag != 0);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  Node* instance_type = __ Word32And(first_instance_type, second_instance_type);
  Node* encoding =
      __ Word32And(instance_type, __ Int32Constant(kStringEncodingMask));
  __ Branch(__ Word32Equal(encoding, __ Int32Constant(kTwoByteStringTag)),
            &if_twobyte, &if_onebyte);
  __ Bind(&if_onebyte);
  __ Goto(&done,
          jsgraph()->HeapConstant(factory()->cons_one_byte_string_map()));
  __ Bind(&if_twobyte);
  __ Goto(&done, jsgraph()->HeapConstant(factory()->cons_string_map()));
  __ Bind(&done);
  Node* result_map = done.PhiAt(0);

  // Allocate the resulting ConsString.
  Node* result = __ Allocate(NOT_TENURED, __ Int32Constant(ConsString::kSize));
  __ StoreField(AccessBuilder::ForMap(), result, result_map);
  __ StoreField(AccessBuilder::ForNameHashField(), result,
                __ Int32Constant(Name::kEmptyHashField));
  __ StoreField(AccessBuilder::ForStringLength(), result, length);
  __ StoreField(AccessBuilder::ForConsStringFirst(), result, first);
  __ StoreField(AccessBuilder::ForConsStringSecond(), result, second);
  return result;
}

Node* EffectControlLinearizer::LowerSameValue(Node* node) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Callable const callable =
      Builtins::CallableFor(isolate(), Builtins::kSameValue);
  Operator::Properties properties = Operator::kEliminatable;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  return __ Call(call_descriptor, __ HeapConstant(callable.code()), lhs, rhs,
                 __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerDeadValue(Node* node) {
  Node* input = NodeProperties::GetValueInput(node, 0);
  if (input->opcode() != IrOpcode::kUnreachable) {
    Node* unreachable = __ Unreachable();
    NodeProperties::ReplaceValueInput(node, unreachable, 0);
  }
  return node;
}

Node* EffectControlLinearizer::LowerStringToNumber(Node* node) {
  Node* string = node->InputAt(0);

  Callable const callable =
      Builtins::CallableFor(isolate(), Builtins::kStringToNumber);
  Operator::Properties properties = Operator::kEliminatable;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  return __ Call(call_descriptor, __ HeapConstant(callable.code()), string,
                 __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerStringCharCodeAt(Node* node) {
  Node* receiver = node->InputAt(0);
  Node* position = node->InputAt(1);

  // We need a loop here to properly deal with indirect strings
  // (SlicedString, ConsString and ThinString).
  auto loop = __ MakeLoopLabel(MachineRepresentation::kTagged,
                               MachineType::PointerRepresentation());
  auto loop_next = __ MakeLabel(MachineRepresentation::kTagged,
                                MachineType::PointerRepresentation());
  auto loop_done = __ MakeLabel(MachineRepresentation::kWord32);
  __ Goto(&loop, receiver, position);
  __ Bind(&loop);
  {
    Node* receiver = loop.PhiAt(0);
    Node* position = loop.PhiAt(1);
    Node* receiver_map = __ LoadField(AccessBuilder::ForMap(), receiver);
    Node* receiver_instance_type =
        __ LoadField(AccessBuilder::ForMapInstanceType(), receiver_map);
    Node* receiver_representation = __ Word32And(
        receiver_instance_type, __ Int32Constant(kStringRepresentationMask));

    // Dispatch on the current {receiver}s string representation.
    auto if_seqstring = __ MakeLabel();
    auto if_consstring = __ MakeLabel();
    auto if_thinstring = __ MakeLabel();
    auto if_externalstring = __ MakeLabel();
    auto if_slicedstring = __ MakeLabel();
    auto if_runtime = __ MakeDeferredLabel();
    __ GotoIf(__ Word32Equal(receiver_representation,
                             __ Int32Constant(kSeqStringTag)),
              &if_seqstring);
    __ GotoIf(__ Word32Equal(receiver_representation,
                             __ Int32Constant(kConsStringTag)),
              &if_consstring);
    __ GotoIf(__ Word32Equal(receiver_representation,
                             __ Int32Constant(kThinStringTag)),
              &if_thinstring);
    __ GotoIf(__ Word32Equal(receiver_representation,
                             __ Int32Constant(kExternalStringTag)),
              &if_externalstring);
    __ Branch(__ Word32Equal(receiver_representation,
                             __ Int32Constant(kSlicedStringTag)),
              &if_slicedstring, &if_runtime);

    __ Bind(&if_seqstring);
    {
      Node* receiver_is_onebyte = __ Word32Equal(
          __ Word32Equal(__ Word32And(receiver_instance_type,
                                      __ Int32Constant(kStringEncodingMask)),
                         __ Int32Constant(kTwoByteStringTag)),
          __ Int32Constant(0));
      Node* result = LoadFromSeqString(receiver, position, receiver_is_onebyte);
      __ Goto(&loop_done, result);
    }

    __ Bind(&if_thinstring);
    {
      Node* receiver_actual =
          __ LoadField(AccessBuilder::ForThinStringActual(), receiver);
      __ Goto(&loop_next, receiver_actual, position);
    }

    __ Bind(&if_consstring);
    {
      Node* receiver_second =
          __ LoadField(AccessBuilder::ForConsStringSecond(), receiver);
      __ GotoIfNot(__ WordEqual(receiver_second, __ EmptyStringConstant()),
                   &if_runtime);
      Node* receiver_first =
          __ LoadField(AccessBuilder::ForConsStringFirst(), receiver);
      __ Goto(&loop_next, receiver_first, position);
    }

    __ Bind(&if_externalstring);
    {
      // We need to bailout to the runtime for uncached external strings.
      __ GotoIf(__ Word32Equal(
                    __ Word32And(receiver_instance_type,
                                 __ Int32Constant(kUncachedExternalStringMask)),
                    __ Int32Constant(kUncachedExternalStringTag)),
                &if_runtime);

      Node* receiver_data = __ LoadField(
          AccessBuilder::ForExternalStringResourceData(), receiver);

      auto if_onebyte = __ MakeLabel();
      auto if_twobyte = __ MakeLabel();
      __ Branch(
          __ Word32Equal(__ Word32And(receiver_instance_type,
                                      __ Int32Constant(kStringEncodingMask)),
                         __ Int32Constant(kTwoByteStringTag)),
          &if_twobyte, &if_onebyte);

      __ Bind(&if_onebyte);
      {
        Node* result = __ Load(MachineType::Uint8(), receiver_data, position);
        __ Goto(&loop_done, result);
      }

      __ Bind(&if_twobyte);
      {
        Node* result = __ Load(MachineType::Uint16(), receiver_data,
                               __ WordShl(position, __ IntPtrConstant(1)));
        __ Goto(&loop_done, result);
      }
    }

    __ Bind(&if_slicedstring);
    {
      Node* receiver_offset =
          __ LoadField(AccessBuilder::ForSlicedStringOffset(), receiver);
      Node* receiver_parent =
          __ LoadField(AccessBuilder::ForSlicedStringParent(), receiver);
      __ Goto(&loop_next, receiver_parent,
              __ IntAdd(position, ChangeSmiToIntPtr(receiver_offset)));
    }

    __ Bind(&if_runtime);
    {
      Operator::Properties properties = Operator::kNoDeopt | Operator::kNoThrow;
      Runtime::FunctionId id = Runtime::kStringCharCodeAt;
      auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
          graph()->zone(), id, 2, properties, CallDescriptor::kNoFlags);
      Node* result = __ Call(call_descriptor, __ CEntryStubConstant(1),
                             receiver, ChangeIntPtrToSmi(position),
                             __ ExternalConstant(ExternalReference::Create(id)),
                             __ Int32Constant(2), __ NoContextConstant());
      __ Goto(&loop_done, ChangeSmiToInt32(result));
    }

    __ Bind(&loop_next);
    __ Goto(&loop, loop_next.PhiAt(0), loop_next.PhiAt(1));
  }
  __ Bind(&loop_done);
  return loop_done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerStringCodePointAt(
    Node* node, UnicodeEncoding encoding) {
  Node* receiver = node->InputAt(0);
  Node* position = node->InputAt(1);

  Builtins::Name builtin = encoding == UnicodeEncoding::UTF16
                               ? Builtins::kStringCodePointAtUTF16
                               : Builtins::kStringCodePointAtUTF32;

  Callable const callable = Builtins::CallableFor(isolate(), builtin);
  Operator::Properties properties = Operator::kNoThrow | Operator::kNoWrite;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  return __ Call(call_descriptor, __ HeapConstant(callable.code()), receiver,
                 position, __ NoContextConstant());
}

Node* EffectControlLinearizer::LoadFromSeqString(Node* receiver, Node* position,
                                                 Node* is_one_byte) {
  auto one_byte_load = __ MakeLabel();
  auto done = __ MakeLabel(MachineRepresentation::kWord32);
  __ GotoIf(is_one_byte, &one_byte_load);
  Node* two_byte_result = __ LoadElement(
      AccessBuilder::ForSeqTwoByteStringCharacter(), receiver, position);
  __ Goto(&done, two_byte_result);

  __ Bind(&one_byte_load);
  Node* one_byte_element = __ LoadElement(
      AccessBuilder::ForSeqOneByteStringCharacter(), receiver, position);
  __ Goto(&done, one_byte_element);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerStringFromSingleCharCode(Node* node) {
  Node* value = node->InputAt(0);
  Node* code = __ Word32And(value, __ Uint32Constant(0xFFFF));

  auto if_not_one_byte = __ MakeDeferredLabel();
  auto cache_miss = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kTagged);

  // Check if the {code} is a one byte character
  Node* check1 = __ Uint32LessThanOrEqual(
      code, __ Uint32Constant(String::kMaxOneByteCharCode));
  __ GotoIfNot(check1, &if_not_one_byte);
  {
    // Load the isolate wide single character string cache.
    Node* cache = __ HeapConstant(factory()->single_character_string_cache());

    // Compute the {cache} index for {code}.
    Node* index = machine()->Is32() ? code : __ ChangeUint32ToUint64(code);

    // Check if we have an entry for the {code} in the single character string
    // cache already.
    Node* entry =
        __ LoadElement(AccessBuilder::ForFixedArrayElement(), cache, index);

    Node* check2 = __ WordEqual(entry, __ UndefinedConstant());
    __ GotoIf(check2, &cache_miss);

    // Use the {entry} from the {cache}.
    __ Goto(&done, entry);

    __ Bind(&cache_miss);
    {
      // Allocate a new SeqOneByteString for {code}.
      Node* vtrue2 = __ Allocate(
          NOT_TENURED, __ Int32Constant(SeqOneByteString::SizeFor(1)));
      __ StoreField(AccessBuilder::ForMap(), vtrue2,
                    __ HeapConstant(factory()->one_byte_string_map()));
      __ StoreField(AccessBuilder::ForNameHashField(), vtrue2,
                    __ Int32Constant(Name::kEmptyHashField));
      __ StoreField(AccessBuilder::ForStringLength(), vtrue2,
                    __ Int32Constant(1));
      __ Store(
          StoreRepresentation(MachineRepresentation::kWord8, kNoWriteBarrier),
          vtrue2,
          __ IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag),
          code);

      // Remember it in the {cache}.
      __ StoreElement(AccessBuilder::ForFixedArrayElement(), cache, index,
                      vtrue2);
      __ Goto(&done, vtrue2);
    }
  }

  __ Bind(&if_not_one_byte);
  {
    // Allocate a new SeqTwoByteString for {code}.
    Node* vfalse1 = __ Allocate(NOT_TENURED,
                                __ Int32Constant(SeqTwoByteString::SizeFor(1)));
    __ StoreField(AccessBuilder::ForMap(), vfalse1,
                  __ HeapConstant(factory()->string_map()));
    __ StoreField(AccessBuilder::ForNameHashField(), vfalse1,
                  __ Int32Constant(Name::kEmptyHashField));
    __ StoreField(AccessBuilder::ForStringLength(), vfalse1,
                  __ Int32Constant(1));
    __ Store(
        StoreRepresentation(MachineRepresentation::kWord16, kNoWriteBarrier),
        vfalse1,
        __ IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag),
        code);
    __ Goto(&done, vfalse1);
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

#ifdef V8_INTL_SUPPORT

Node* EffectControlLinearizer::LowerStringToLowerCaseIntl(Node* node) {
  Node* receiver = node->InputAt(0);

  Callable callable =
      Builtins::CallableFor(isolate(), Builtins::kStringToLowerCaseIntl);
  Operator::Properties properties = Operator::kNoDeopt | Operator::kNoThrow;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  return __ Call(call_descriptor, __ HeapConstant(callable.code()), receiver,
                 __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerStringToUpperCaseIntl(Node* node) {
  Node* receiver = node->InputAt(0);
  Operator::Properties properties = Operator::kNoDeopt | Operator::kNoThrow;
  Runtime::FunctionId id = Runtime::kStringToUpperCaseIntl;
  auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
      graph()->zone(), id, 1, properties, CallDescriptor::kNoFlags);
  return __ Call(call_descriptor, __ CEntryStubConstant(1), receiver,
                 __ ExternalConstant(ExternalReference::Create(id)),
                 __ Int32Constant(1), __ NoContextConstant());
}

#else

Node* EffectControlLinearizer::LowerStringToLowerCaseIntl(Node* node) {
  UNREACHABLE();
  return nullptr;
}

Node* EffectControlLinearizer::LowerStringToUpperCaseIntl(Node* node) {
  UNREACHABLE();
  return nullptr;
}

#endif  // V8_INTL_SUPPORT

Node* EffectControlLinearizer::LowerStringFromSingleCodePoint(Node* node) {
  Node* value = node->InputAt(0);
  Node* code = value;

  auto if_not_single_code = __ MakeDeferredLabel();
  auto if_not_one_byte = __ MakeDeferredLabel();
  auto cache_miss = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kTagged);

  // Check if the {code} is a single code unit
  Node* check0 = __ Uint32LessThanOrEqual(code, __ Uint32Constant(0xFFFF));
  __ GotoIfNot(check0, &if_not_single_code);

  {
    // Check if the {code} is a one byte character
    Node* check1 = __ Uint32LessThanOrEqual(
        code, __ Uint32Constant(String::kMaxOneByteCharCode));
    __ GotoIfNot(check1, &if_not_one_byte);
    {
      // Load the isolate wide single character string cache.
      Node* cache = __ HeapConstant(factory()->single_character_string_cache());

      // Compute the {cache} index for {code}.
      Node* index = machine()->Is32() ? code : __ ChangeUint32ToUint64(code);

      // Check if we have an entry for the {code} in the single character string
      // cache already.
      Node* entry =
          __ LoadElement(AccessBuilder::ForFixedArrayElement(), cache, index);

      Node* check2 = __ WordEqual(entry, __ UndefinedConstant());
      __ GotoIf(check2, &cache_miss);

      // Use the {entry} from the {cache}.
      __ Goto(&done, entry);

      __ Bind(&cache_miss);
      {
        // Allocate a new SeqOneByteString for {code}.
        Node* vtrue2 = __ Allocate(
            NOT_TENURED, __ Int32Constant(SeqOneByteString::SizeFor(1)));
        __ StoreField(AccessBuilder::ForMap(), vtrue2,
                      __ HeapConstant(factory()->one_byte_string_map()));
        __ StoreField(AccessBuilder::ForNameHashField(), vtrue2,
                      __ Int32Constant(Name::kEmptyHashField));
        __ StoreField(AccessBuilder::ForStringLength(), vtrue2,
                      __ Int32Constant(1));
        __ Store(
            StoreRepresentation(MachineRepresentation::kWord8, kNoWriteBarrier),
            vtrue2,
            __ IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag),
            code);

        // Remember it in the {cache}.
        __ StoreElement(AccessBuilder::ForFixedArrayElement(), cache, index,
                        vtrue2);
        __ Goto(&done, vtrue2);
      }
    }

    __ Bind(&if_not_one_byte);
    {
      // Allocate a new SeqTwoByteString for {code}.
      Node* vfalse1 = __ Allocate(
          NOT_TENURED, __ Int32Constant(SeqTwoByteString::SizeFor(1)));
      __ StoreField(AccessBuilder::ForMap(), vfalse1,
                    __ HeapConstant(factory()->string_map()));
      __ StoreField(AccessBuilder::ForNameHashField(), vfalse1,
                    __ IntPtrConstant(Name::kEmptyHashField));
      __ StoreField(AccessBuilder::ForStringLength(), vfalse1,
                    __ Int32Constant(1));
      __ Store(
          StoreRepresentation(MachineRepresentation::kWord16, kNoWriteBarrier),
          vfalse1,
          __ IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag),
          code);
      __ Goto(&done, vfalse1);
    }
  }

  __ Bind(&if_not_single_code);
  // Generate surrogate pair string
  {
    switch (UnicodeEncodingOf(node->op())) {
      case UnicodeEncoding::UTF16:
        break;

      case UnicodeEncoding::UTF32: {
        // Convert UTF32 to UTF16 code units, and store as a 32 bit word.
        Node* lead_offset = __ Int32Constant(0xD800 - (0x10000 >> 10));

        // lead = (codepoint >> 10) + LEAD_OFFSET
        Node* lead =
            __ Int32Add(__ Word32Shr(code, __ Int32Constant(10)), lead_offset);

        // trail = (codepoint & 0x3FF) + 0xDC00;
        Node* trail = __ Int32Add(__ Word32And(code, __ Int32Constant(0x3FF)),
                                  __ Int32Constant(0xDC00));

        // codpoint = (trail << 16) | lead;
#if V8_TARGET_BIG_ENDIAN
        code = __ Word32Or(__ Word32Shl(lead, __ Int32Constant(16)), trail);
#else
        code = __ Word32Or(__ Word32Shl(trail, __ Int32Constant(16)), lead);
#endif
        break;
      }
    }

    // Allocate a new SeqTwoByteString for {code}.
    Node* vfalse0 = __ Allocate(NOT_TENURED,
                                __ Int32Constant(SeqTwoByteString::SizeFor(2)));
    __ StoreField(AccessBuilder::ForMap(), vfalse0,
                  __ HeapConstant(factory()->string_map()));
    __ StoreField(AccessBuilder::ForNameHashField(), vfalse0,
                  __ Int32Constant(Name::kEmptyHashField));
    __ StoreField(AccessBuilder::ForStringLength(), vfalse0,
                  __ Int32Constant(2));
    __ Store(
        StoreRepresentation(MachineRepresentation::kWord32, kNoWriteBarrier),
        vfalse0,
        __ IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag),
        code);
    __ Goto(&done, vfalse0);
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerStringIndexOf(Node* node) {
  Node* subject = node->InputAt(0);
  Node* search_string = node->InputAt(1);
  Node* position = node->InputAt(2);

  Callable callable =
      Builtins::CallableFor(isolate(), Builtins::kStringIndexOf);
  Operator::Properties properties = Operator::kEliminatable;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  return __ Call(call_descriptor, __ HeapConstant(callable.code()), subject,
                 search_string, position, __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerStringLength(Node* node) {
  Node* subject = node->InputAt(0);

  return __ LoadField(AccessBuilder::ForStringLength(), subject);
}

Node* EffectControlLinearizer::LowerStringComparison(Callable const& callable,
                                                     Node* node) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Operator::Properties properties = Operator::kEliminatable;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  return __ Call(call_descriptor, __ HeapConstant(callable.code()), lhs, rhs,
                 __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerStringSubstring(Node* node) {
  Node* receiver = node->InputAt(0);
  Node* start = ChangeInt32ToIntPtr(node->InputAt(1));
  Node* end = ChangeInt32ToIntPtr(node->InputAt(2));

  Callable callable =
      Builtins::CallableFor(isolate(), Builtins::kStringSubstring);
  Operator::Properties properties = Operator::kEliminatable;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  return __ Call(call_descriptor, __ HeapConstant(callable.code()), receiver,
                 start, end, __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerStringEqual(Node* node) {
  return LowerStringComparison(
      Builtins::CallableFor(isolate(), Builtins::kStringEqual), node);
}

Node* EffectControlLinearizer::LowerStringLessThan(Node* node) {
  return LowerStringComparison(
      Builtins::CallableFor(isolate(), Builtins::kStringLessThan), node);
}

Node* EffectControlLinearizer::LowerStringLessThanOrEqual(Node* node) {
  return LowerStringComparison(
      Builtins::CallableFor(isolate(), Builtins::kStringLessThanOrEqual), node);
}

Node* EffectControlLinearizer::LowerCheckFloat64Hole(Node* node,
                                                     Node* frame_state) {
  // If we reach this point w/o eliminating the {node} that's marked
  // with allow-return-hole, we cannot do anything, so just deoptimize
  // in case of the hole NaN.
  CheckFloat64HoleParameters const& params =
      CheckFloat64HoleParametersOf(node->op());
  Node* value = node->InputAt(0);

  auto if_nan = __ MakeDeferredLabel();
  auto done = __ MakeLabel();

  // First check whether {value} is a NaN at all...
  __ Branch(__ Float64Equal(value, value), &done, &if_nan);

  __ Bind(&if_nan);
  {
    // ...and only if {value} is a NaN, perform the expensive bit
    // check. See http://crbug.com/v8/8264 for details.
    Node* check = __ Word32Equal(__ Float64ExtractHighWord32(value),
                                 __ Int32Constant(kHoleNanUpper32));
    __ DeoptimizeIf(DeoptimizeReason::kHole, params.feedback(), check,
                    frame_state);
    __ Goto(&done);
  }

  __ Bind(&done);
  return value;
}

Node* EffectControlLinearizer::LowerCheckNotTaggedHole(Node* node,
                                                       Node* frame_state) {
  Node* value = node->InputAt(0);
  Node* check = __ WordEqual(value, __ TheHoleConstant());
  __ DeoptimizeIf(DeoptimizeReason::kHole, VectorSlotPair(), check,
                  frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerConvertTaggedHoleToUndefined(Node* node) {
  Node* value = node->InputAt(0);

  auto if_is_hole = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kTagged);

  Node* check = __ WordEqual(value, __ TheHoleConstant());
  __ GotoIf(check, &if_is_hole);
  __ Goto(&done, value);

  __ Bind(&if_is_hole);
  __ Goto(&done, __ UndefinedConstant());

  __ Bind(&done);
  return done.PhiAt(0);
}

void EffectControlLinearizer::LowerCheckEqualsInternalizedString(
    Node* node, Node* frame_state) {
  Node* exp = node->InputAt(0);
  Node* val = node->InputAt(1);

  auto if_same = __ MakeLabel();
  auto if_notsame = __ MakeDeferredLabel();
  auto if_thinstring = __ MakeLabel();
  auto if_notthinstring = __ MakeLabel();

  // Check if {exp} and {val} are the same, which is the likely case.
  __ Branch(__ WordEqual(exp, val), &if_same, &if_notsame);

  __ Bind(&if_notsame);
  {
    // Now {val} could still be a non-internalized String that matches {exp}.
    __ DeoptimizeIf(DeoptimizeReason::kWrongName, VectorSlotPair(),
                    ObjectIsSmi(val), frame_state);
    Node* val_map = __ LoadField(AccessBuilder::ForMap(), val);
    Node* val_instance_type =
        __ LoadField(AccessBuilder::ForMapInstanceType(), val_map);

    // Check for the common case of ThinString first.
    __ GotoIf(__ Word32Equal(val_instance_type,
                             __ Int32Constant(THIN_ONE_BYTE_STRING_TYPE)),
              &if_thinstring);
    __ Branch(
        __ Word32Equal(val_instance_type, __ Int32Constant(THIN_STRING_TYPE)),
        &if_thinstring, &if_notthinstring);

    __ Bind(&if_notthinstring);
    {
      // Check that the {val} is a non-internalized String, if it's anything
      // else it cannot match the recorded feedback {exp} anyways.
      __ DeoptimizeIfNot(
          DeoptimizeReason::kWrongName, VectorSlotPair(),
          __ Word32Equal(__ Word32And(val_instance_type,
                                      __ Int32Constant(kIsNotStringMask |
                                                       kIsNotInternalizedMask)),
                         __ Int32Constant(kStringTag | kNotInternalizedTag)),
          frame_state);

      // Try to find the {val} in the string table.
      MachineSignature::Builder builder(graph()->zone(), 1, 2);
      builder.AddReturn(MachineType::AnyTagged());
      builder.AddParam(MachineType::Pointer());
      builder.AddParam(MachineType::AnyTagged());
      Node* try_internalize_string_function = __ ExternalConstant(
          ExternalReference::try_internalize_string_function());
      Node* const isolate_ptr =
          __ ExternalConstant(ExternalReference::isolate_address(isolate()));
      auto call_descriptor =
          Linkage::GetSimplifiedCDescriptor(graph()->zone(), builder.Build());
      Node* val_internalized =
          __ Call(common()->Call(call_descriptor),
                  try_internalize_string_function, isolate_ptr, val);

      // Now see if the results match.
      __ DeoptimizeIfNot(DeoptimizeReason::kWrongName, VectorSlotPair(),
                         __ WordEqual(exp, val_internalized), frame_state);
      __ Goto(&if_same);
    }

    __ Bind(&if_thinstring);
    {
      // The {val} is a ThinString, let's check the actual value.
      Node* val_actual =
          __ LoadField(AccessBuilder::ForThinStringActual(), val);
      __ DeoptimizeIfNot(DeoptimizeReason::kWrongName, VectorSlotPair(),
                         __ WordEqual(exp, val_actual), frame_state);
      __ Goto(&if_same);
    }
  }

  __ Bind(&if_same);
}

void EffectControlLinearizer::LowerCheckEqualsSymbol(Node* node,
                                                     Node* frame_state) {
  Node* exp = node->InputAt(0);
  Node* val = node->InputAt(1);
  Node* check = __ WordEqual(exp, val);
  __ DeoptimizeIfNot(DeoptimizeReason::kWrongName, VectorSlotPair(), check,
                     frame_state);
}

Node* EffectControlLinearizer::AllocateHeapNumberWithValue(Node* value) {
  Node* result = __ Allocate(NOT_TENURED, __ Int32Constant(HeapNumber::kSize));
  __ StoreField(AccessBuilder::ForMap(), result, __ HeapNumberMapConstant());
  __ StoreField(AccessBuilder::ForHeapNumberValue(), result, value);
  return result;
}

Node* EffectControlLinearizer::ChangeIntPtrToSmi(Node* value) {
  // Do shift on 32bit values if Smis are stored in the lower word.
  if (machine()->Is64() && SmiValuesAre31Bits()) {
    return __ ChangeInt32ToInt64(
        __ Word32Shl(__ TruncateInt64ToInt32(value), SmiShiftBitsConstant()));
  }
  return __ WordShl(value, SmiShiftBitsConstant());
}

Node* EffectControlLinearizer::ChangeInt32ToIntPtr(Node* value) {
  if (machine()->Is64()) {
    value = __ ChangeInt32ToInt64(value);
  }
  return value;
}

Node* EffectControlLinearizer::ChangeIntPtrToInt32(Node* value) {
  if (machine()->Is64()) {
    value = __ TruncateInt64ToInt32(value);
  }
  return value;
}

Node* EffectControlLinearizer::ChangeInt32ToSmi(Node* value) {
  // Do shift on 32bit values if Smis are stored in the lower word.
  if (machine()->Is64() && SmiValuesAre31Bits()) {
    return __ ChangeInt32ToInt64(__ Word32Shl(value, SmiShiftBitsConstant()));
  }
  return ChangeIntPtrToSmi(ChangeInt32ToIntPtr(value));
}

Node* EffectControlLinearizer::ChangeInt64ToSmi(Node* value) {
  DCHECK(machine()->Is64());
  return ChangeIntPtrToSmi(value);
}

Node* EffectControlLinearizer::ChangeUint32ToUintPtr(Node* value) {
  if (machine()->Is64()) {
    value = __ ChangeUint32ToUint64(value);
  }
  return value;
}

Node* EffectControlLinearizer::ChangeUint32ToSmi(Node* value) {
  // Do shift on 32bit values if Smis are stored in the lower word.
  if (machine()->Is64() && SmiValuesAre31Bits()) {
    return __ ChangeUint32ToUint64(__ Word32Shl(value, SmiShiftBitsConstant()));
  } else {
    return __ WordShl(ChangeUint32ToUintPtr(value), SmiShiftBitsConstant());
  }
}

Node* EffectControlLinearizer::ChangeSmiToIntPtr(Node* value) {
  // Do shift on 32bit values if Smis are stored in the lower word.
  if (machine()->Is64() && SmiValuesAre31Bits()) {
    return __ ChangeInt32ToInt64(
        __ Word32Sar(__ TruncateInt64ToInt32(value), SmiShiftBitsConstant()));
  }
  return __ WordSar(value, SmiShiftBitsConstant());
}

Node* EffectControlLinearizer::ChangeSmiToInt32(Node* value) {
  // Do shift on 32bit values if Smis are stored in the lower word.
  if (machine()->Is64() && SmiValuesAre31Bits()) {
    return __ Word32Sar(__ TruncateInt64ToInt32(value), SmiShiftBitsConstant());
  }
  if (machine()->Is64()) {
    return __ TruncateInt64ToInt32(ChangeSmiToIntPtr(value));
  }
  return ChangeSmiToIntPtr(value);
}

Node* EffectControlLinearizer::ChangeSmiToInt64(Node* value) {
  CHECK(machine()->Is64());
  return ChangeSmiToIntPtr(value);
}

Node* EffectControlLinearizer::ObjectIsSmi(Node* value) {
  return __ WordEqual(__ WordAnd(value, __ IntPtrConstant(kSmiTagMask)),
                      __ IntPtrConstant(kSmiTag));
}

Node* EffectControlLinearizer::SmiMaxValueConstant() {
  return __ Int32Constant(Smi::kMaxValue);
}

Node* EffectControlLinearizer::SmiShiftBitsConstant() {
  if (machine()->Is64() && SmiValuesAre31Bits()) {
    return __ Int32Constant(kSmiShiftSize + kSmiTagSize);
  }
  return __ IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}

Node* EffectControlLinearizer::LowerPlainPrimitiveToNumber(Node* node) {
  Node* value = node->InputAt(0);
  return __ ToNumber(value);
}

Node* EffectControlLinearizer::LowerPlainPrimitiveToWord32(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel();
  auto if_to_number_smi = __ MakeLabel();
  auto done = __ MakeLabel(MachineRepresentation::kWord32);

  Node* check0 = ObjectIsSmi(value);
  __ GotoIfNot(check0, &if_not_smi);
  __ Goto(&done, ChangeSmiToInt32(value));

  __ Bind(&if_not_smi);
  Node* to_number = __ ToNumber(value);

  Node* check1 = ObjectIsSmi(to_number);
  __ GotoIf(check1, &if_to_number_smi);
  Node* number = __ LoadField(AccessBuilder::ForHeapNumberValue(), to_number);
  __ Goto(&done, __ TruncateFloat64ToWord32(number));

  __ Bind(&if_to_number_smi);
  __ Goto(&done, ChangeSmiToInt32(to_number));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerPlainPrimitiveToFloat64(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel();
  auto if_to_number_smi = __ MakeLabel();
  auto done = __ MakeLabel(MachineRepresentation::kFloat64);

  Node* check0 = ObjectIsSmi(value);
  __ GotoIfNot(check0, &if_not_smi);
  Node* from_smi = ChangeSmiToInt32(value);
  __ Goto(&done, __ ChangeInt32ToFloat64(from_smi));

  __ Bind(&if_not_smi);
  Node* to_number = __ ToNumber(value);
  Node* check1 = ObjectIsSmi(to_number);
  __ GotoIf(check1, &if_to_number_smi);

  Node* number = __ LoadField(AccessBuilder::ForHeapNumberValue(), to_number);
  __ Goto(&done, number);

  __ Bind(&if_to_number_smi);
  Node* number_from_smi = ChangeSmiToInt32(to_number);
  number_from_smi = __ ChangeInt32ToFloat64(number_from_smi);
  __ Goto(&done, number_from_smi);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerEnsureWritableFastElements(Node* node) {
  Node* object = node->InputAt(0);
  Node* elements = node->InputAt(1);

  auto if_not_fixed_array = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kTagged);

  // Load the current map of {elements}.
  Node* elements_map = __ LoadField(AccessBuilder::ForMap(), elements);

  // Check if {elements} is not a copy-on-write FixedArray.
  Node* check = __ WordEqual(elements_map, __ FixedArrayMapConstant());
  __ GotoIfNot(check, &if_not_fixed_array);
  // Nothing to do if the {elements} are not copy-on-write.
  __ Goto(&done, elements);

  __ Bind(&if_not_fixed_array);
  // We need to take a copy of the {elements} and set them up for {object}.
  Operator::Properties properties = Operator::kEliminatable;
  Callable callable =
      Builtins::CallableFor(isolate(), Builtins::kCopyFastSmiOrObjectElements);
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), flags, properties);
  Node* result = __ Call(call_descriptor, __ HeapConstant(callable.code()),
                         object, __ NoContextConstant());
  __ Goto(&done, result);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerMaybeGrowFastElements(Node* node,
                                                          Node* frame_state) {
  GrowFastElementsParameters params = GrowFastElementsParametersOf(node->op());
  Node* object = node->InputAt(0);
  Node* elements = node->InputAt(1);
  Node* index = node->InputAt(2);
  Node* elements_length = node->InputAt(3);

  auto done = __ MakeLabel(MachineRepresentation::kTagged);
  auto if_grow = __ MakeDeferredLabel();
  auto if_not_grow = __ MakeLabel();

  // Check if we need to grow the {elements} backing store.
  Node* check = __ Uint32LessThan(index, elements_length);
  __ GotoIfNot(check, &if_grow);
  __ Goto(&done, elements);

  __ Bind(&if_grow);
  // We need to grow the {elements} for {object}.
  Operator::Properties properties = Operator::kEliminatable;
  Callable callable =
      (params.mode() == GrowFastElementsMode::kDoubleElements)
          ? Builtins::CallableFor(isolate(), Builtins::kGrowFastDoubleElements)
          : Builtins::CallableFor(isolate(),
                                  Builtins::kGrowFastSmiOrObjectElements);
  CallDescriptor::Flags call_flags = CallDescriptor::kNoFlags;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), call_flags, properties);
  Node* new_elements =
      __ Call(call_descriptor, __ HeapConstant(callable.code()), object,
              ChangeInt32ToSmi(index), __ NoContextConstant());

  // Ensure that we were able to grow the {elements}.
  __ DeoptimizeIf(DeoptimizeReason::kCouldNotGrowElements, params.feedback(),
                  ObjectIsSmi(new_elements), frame_state);
  __ Goto(&done, new_elements);

  __ Bind(&done);
  return done.PhiAt(0);
}

void EffectControlLinearizer::LowerTransitionElementsKind(Node* node) {
  ElementsTransition const transition = ElementsTransitionOf(node->op());
  Node* object = node->InputAt(0);

  auto if_map_same = __ MakeDeferredLabel();
  auto done = __ MakeLabel();

  Node* source_map = __ HeapConstant(transition.source());
  Node* target_map = __ HeapConstant(transition.target());

  // Load the current map of {object}.
  Node* object_map = __ LoadField(AccessBuilder::ForMap(), object);

  // Check if {object_map} is the same as {source_map}.
  Node* check = __ WordEqual(object_map, source_map);
  __ GotoIf(check, &if_map_same);
  __ Goto(&done);

  __ Bind(&if_map_same);
  switch (transition.mode()) {
    case ElementsTransition::kFastTransition:
      // In-place migration of {object}, just store the {target_map}.
      __ StoreField(AccessBuilder::ForMap(), object, target_map);
      break;
    case ElementsTransition::kSlowTransition: {
      // Instance migration, call out to the runtime for {object}.
      Operator::Properties properties = Operator::kNoDeopt | Operator::kNoThrow;
      Runtime::FunctionId id = Runtime::kTransitionElementsKind;
      auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
          graph()->zone(), id, 2, properties, CallDescriptor::kNoFlags);
      __ Call(call_descriptor, __ CEntryStubConstant(1), object, target_map,
              __ ExternalConstant(ExternalReference::Create(id)),
              __ Int32Constant(2), __ NoContextConstant());
      break;
    }
  }
  __ Goto(&done);

  __ Bind(&done);
}

Node* EffectControlLinearizer::LowerLoadFieldByIndex(Node* node) {
  Node* object = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* zero = __ IntPtrConstant(0);
  Node* one = __ IntPtrConstant(1);

  // Sign-extend the {index} on 64-bit architectures.
  if (machine()->Is64()) {
    index = __ ChangeInt32ToInt64(index);
  }

  auto if_double = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kTagged);

  // Check if field is a mutable double field.
  __ GotoIfNot(__ WordEqual(__ WordAnd(index, one), zero), &if_double);

  // The field is a proper Tagged field on {object}. The {index} is shifted
  // to the left by one in the code below.
  {
    // Check if field is in-object or out-of-object.
    auto if_outofobject = __ MakeLabel();
    __ GotoIf(__ IntLessThan(index, zero), &if_outofobject);

    // The field is located in the {object} itself.
    {
      Node* offset =
          __ IntAdd(__ WordShl(index, __ IntPtrConstant(kTaggedSizeLog2 - 1)),
                    __ IntPtrConstant(JSObject::kHeaderSize - kHeapObjectTag));
      Node* result = __ Load(MachineType::AnyTagged(), object, offset);
      __ Goto(&done, result);
    }

    // The field is located in the properties backing store of {object}.
    // The {index} is equal to the negated out of property index plus 1.
    __ Bind(&if_outofobject);
    {
      Node* properties =
          __ LoadField(AccessBuilder::ForJSObjectPropertiesOrHash(), object);
      Node* offset =
          __ IntAdd(__ WordShl(__ IntSub(zero, index),
                               __ IntPtrConstant(kTaggedSizeLog2 - 1)),
                    __ IntPtrConstant((FixedArray::kHeaderSize - kTaggedSize) -
                                      kHeapObjectTag));
      Node* result = __ Load(MachineType::AnyTagged(), properties, offset);
      __ Goto(&done, result);
    }
  }

  // The field is a Double field, either unboxed in the object on 64-bit
  // architectures, or as MutableHeapNumber.
  __ Bind(&if_double);
  {
    auto done_double = __ MakeLabel(MachineRepresentation::kFloat64);

    index = __ WordSar(index, one);

    // Check if field is in-object or out-of-object.
    auto if_outofobject = __ MakeLabel();
    __ GotoIf(__ IntLessThan(index, zero), &if_outofobject);

    // The field is located in the {object} itself.
    {
      Node* offset =
          __ IntAdd(__ WordShl(index, __ IntPtrConstant(kTaggedSizeLog2)),
                    __ IntPtrConstant(JSObject::kHeaderSize - kHeapObjectTag));
      if (FLAG_unbox_double_fields) {
        Node* result = __ Load(MachineType::Float64(), object, offset);
        __ Goto(&done_double, result);
      } else {
        Node* result = __ Load(MachineType::AnyTagged(), object, offset);
        result = __ LoadField(AccessBuilder::ForHeapNumberValue(), result);
        __ Goto(&done_double, result);
      }
    }

    __ Bind(&if_outofobject);
    {
      Node* properties =
          __ LoadField(AccessBuilder::ForJSObjectPropertiesOrHash(), object);
      Node* offset =
          __ IntAdd(__ WordShl(__ IntSub(zero, index),
                               __ IntPtrConstant(kTaggedSizeLog2)),
                    __ IntPtrConstant((FixedArray::kHeaderSize - kTaggedSize) -
                                      kHeapObjectTag));
      Node* result = __ Load(MachineType::AnyTagged(), properties, offset);
      result = __ LoadField(AccessBuilder::ForHeapNumberValue(), result);
      __ Goto(&done_double, result);
    }

    __ Bind(&done_double);
    {
      Node* result = AllocateHeapNumberWithValue(done_double.PhiAt(0));
      __ Goto(&done, result);
    }
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::BuildReverseBytes(ExternalArrayType type,
                                                 Node* value) {
  switch (type) {
    case kExternalInt8Array:
    case kExternalUint8Array:
    case kExternalUint8ClampedArray:
      return value;

    case kExternalInt16Array: {
      Node* result = __ Word32ReverseBytes(value);
      result = __ Word32Sar(result, __ Int32Constant(16));
      return result;
    }

    case kExternalUint16Array: {
      Node* result = __ Word32ReverseBytes(value);
      result = __ Word32Shr(result, __ Int32Constant(16));
      return result;
    }

    case kExternalInt32Array:  // Fall through.
    case kExternalUint32Array:
      return __ Word32ReverseBytes(value);

    case kExternalFloat32Array: {
      Node* result = __ BitcastFloat32ToInt32(value);
      result = __ Word32ReverseBytes(result);
      result = __ BitcastInt32ToFloat32(result);
      return result;
    }

    case kExternalFloat64Array: {
      if (machine()->Is64()) {
        Node* result = __ BitcastFloat64ToInt64(value);
        result = __ Word64ReverseBytes(result);
        result = __ BitcastInt64ToFloat64(result);
        return result;
      } else {
        Node* lo = __ Word32ReverseBytes(__ Float64ExtractLowWord32(value));
        Node* hi = __ Word32ReverseBytes(__ Float64ExtractHighWord32(value));
        Node* result = __ Float64Constant(0.0);
        result = __ Float64InsertLowWord32(result, hi);
        result = __ Float64InsertHighWord32(result, lo);
        return result;
      }
    }

    case kExternalBigInt64Array:
    case kExternalBigUint64Array:
      UNREACHABLE();
  }
}

Node* EffectControlLinearizer::LowerLoadDataViewElement(Node* node) {
  ExternalArrayType element_type = ExternalArrayTypeOf(node->op());
  Node* buffer = node->InputAt(0);
  Node* storage = node->InputAt(1);
  Node* byte_offset = node->InputAt(2);
  Node* index = node->InputAt(3);
  Node* is_little_endian = node->InputAt(4);

  // We need to keep the {buffer} alive so that the GC will not release the
  // ArrayBuffer (if there's any) as long as we are still operating on it.
  __ Retain(buffer);

  // Compute the effective offset.
  Node* offset = __ IntAdd(byte_offset, index);

  MachineType const machine_type =
      AccessBuilder::ForTypedArrayElement(element_type, true).machine_type;

  Node* value = __ LoadUnaligned(machine_type, storage, offset);
  auto big_endian = __ MakeLabel();
  auto done = __ MakeLabel(machine_type.representation());

  __ GotoIfNot(is_little_endian, &big_endian);
  {  // Little-endian load.
#if V8_TARGET_LITTLE_ENDIAN
    __ Goto(&done, value);
#else
    __ Goto(&done, BuildReverseBytes(element_type, value));
#endif  // V8_TARGET_LITTLE_ENDIAN
  }

  __ Bind(&big_endian);
  {  // Big-endian load.
#if V8_TARGET_LITTLE_ENDIAN
    __ Goto(&done, BuildReverseBytes(element_type, value));
#else
    __ Goto(&done, value);
#endif  // V8_TARGET_LITTLE_ENDIAN
  }

  // We're done, return {result}.
  __ Bind(&done);
  return done.PhiAt(0);
}

void EffectControlLinearizer::LowerStoreDataViewElement(Node* node) {
  ExternalArrayType element_type = ExternalArrayTypeOf(node->op());
  Node* buffer = node->InputAt(0);
  Node* storage = node->InputAt(1);
  Node* byte_offset = node->InputAt(2);
  Node* index = node->InputAt(3);
  Node* value = node->InputAt(4);
  Node* is_little_endian = node->InputAt(5);

  // We need to keep the {buffer} alive so that the GC will not release the
  // ArrayBuffer (if there's any) as long as we are still operating on it.
  __ Retain(buffer);

  // Compute the effective offset.
  Node* offset = __ IntAdd(byte_offset, index);

  MachineType const machine_type =
      AccessBuilder::ForTypedArrayElement(element_type, true).machine_type;

  auto big_endian = __ MakeLabel();
  auto done = __ MakeLabel(machine_type.representation());

  __ GotoIfNot(is_little_endian, &big_endian);
  {  // Little-endian store.
#if V8_TARGET_LITTLE_ENDIAN
    __ Goto(&done, value);
#else
    __ Goto(&done, BuildReverseBytes(element_type, value));
#endif  // V8_TARGET_LITTLE_ENDIAN
  }

  __ Bind(&big_endian);
  {  // Big-endian store.
#if V8_TARGET_LITTLE_ENDIAN
    __ Goto(&done, BuildReverseBytes(element_type, value));
#else
    __ Goto(&done, value);
#endif  // V8_TARGET_LITTLE_ENDIAN
  }

  __ Bind(&done);
  __ StoreUnaligned(machine_type.representation(), storage, offset,
                    done.PhiAt(0));
}

Node* EffectControlLinearizer::LowerLoadTypedElement(Node* node) {
  ExternalArrayType array_type = ExternalArrayTypeOf(node->op());
  Node* buffer = node->InputAt(0);
  Node* base = node->InputAt(1);
  Node* external = node->InputAt(2);
  Node* index = node->InputAt(3);

  // We need to keep the {buffer} alive so that the GC will not release the
  // ArrayBuffer (if there's any) as long as we are still operating on it.
  __ Retain(buffer);

  // Compute the effective storage pointer, handling the case where the
  // {external} pointer is the effective storage pointer (i.e. the {base}
  // is Smi zero).
  Node* storage = IntPtrMatcher(base).Is(0)
                      ? external
                      : __ UnsafePointerAdd(base, external);

  // Perform the actual typed element access.
  return __ LoadElement(AccessBuilder::ForTypedArrayElement(
                            array_type, true, LoadSensitivity::kCritical),
                        storage, index);
}

void EffectControlLinearizer::LowerStoreTypedElement(Node* node) {
  ExternalArrayType array_type = ExternalArrayTypeOf(node->op());
  Node* buffer = node->InputAt(0);
  Node* base = node->InputAt(1);
  Node* external = node->InputAt(2);
  Node* index = node->InputAt(3);
  Node* value = node->InputAt(4);

  // We need to keep the {buffer} alive so that the GC will not release the
  // ArrayBuffer (if there's any) as long as we are still operating on it.
  __ Retain(buffer);

  // Compute the effective storage pointer, handling the case where the
  // {external} pointer is the effective storage pointer (i.e. the {base}
  // is Smi zero).
  Node* storage = IntPtrMatcher(base).Is(0)
                      ? external
                      : __ UnsafePointerAdd(base, external);

  // Perform the actual typed element access.
  __ StoreElement(AccessBuilder::ForTypedArrayElement(array_type, true),
                  storage, index, value);
}

void EffectControlLinearizer::TransitionElementsTo(Node* node, Node* array,
                                                   ElementsKind from,
                                                   ElementsKind to) {
  DCHECK(IsMoreGeneralElementsKindTransition(from, to));
  DCHECK(to == HOLEY_ELEMENTS || to == HOLEY_DOUBLE_ELEMENTS);

  Handle<Map> target(to == HOLEY_ELEMENTS ? FastMapParameterOf(node->op())
                                          : DoubleMapParameterOf(node->op()));
  Node* target_map = __ HeapConstant(target);

  if (IsSimpleMapChangeTransition(from, to)) {
    __ StoreField(AccessBuilder::ForMap(), array, target_map);
  } else {
    // Instance migration, call out to the runtime for {array}.
    Operator::Properties properties = Operator::kNoDeopt | Operator::kNoThrow;
    Runtime::FunctionId id = Runtime::kTransitionElementsKind;
    auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
        graph()->zone(), id, 2, properties, CallDescriptor::kNoFlags);
    __ Call(call_descriptor, __ CEntryStubConstant(1), array, target_map,
            __ ExternalConstant(ExternalReference::Create(id)),
            __ Int32Constant(2), __ NoContextConstant());
  }
}

Node* EffectControlLinearizer::IsElementsKindGreaterThan(
    Node* kind, ElementsKind reference_kind) {
  Node* ref_kind = __ Int32Constant(reference_kind);
  Node* ret = __ Int32LessThan(ref_kind, kind);
  return ret;
}

void EffectControlLinearizer::LowerTransitionAndStoreElement(Node* node) {
  Node* array = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  // Possibly transition array based on input and store.
  //
  //   -- TRANSITION PHASE -----------------
  //   kind = ElementsKind(array)
  //   if value is not smi {
  //     if kind == HOLEY_SMI_ELEMENTS {
  //       if value is heap number {
  //         Transition array to HOLEY_DOUBLE_ELEMENTS
  //         kind = HOLEY_DOUBLE_ELEMENTS
  //       } else {
  //         Transition array to HOLEY_ELEMENTS
  //         kind = HOLEY_ELEMENTS
  //       }
  //     } else if kind == HOLEY_DOUBLE_ELEMENTS {
  //       if value is not heap number {
  //         Transition array to HOLEY_ELEMENTS
  //         kind = HOLEY_ELEMENTS
  //       }
  //     }
  //   }
  //
  //   -- STORE PHASE ----------------------
  //   [make sure {kind} is up-to-date]
  //   if kind == HOLEY_DOUBLE_ELEMENTS {
  //     if value is smi {
  //       float_value = convert smi to float
  //       Store array[index] = float_value
  //     } else {
  //       float_value = value
  //       Store array[index] = float_value
  //     }
  //   } else {
  //     // kind is HOLEY_SMI_ELEMENTS or HOLEY_ELEMENTS
  //     Store array[index] = value
  //   }
  //
  Node* map = __ LoadField(AccessBuilder::ForMap(), array);
  Node* kind;
  {
    Node* bit_field2 = __ LoadField(AccessBuilder::ForMapBitField2(), map);
    Node* mask = __ Int32Constant(Map::ElementsKindBits::kMask);
    Node* andit = __ Word32And(bit_field2, mask);
    Node* shift = __ Int32Constant(Map::ElementsKindBits::kShift);
    kind = __ Word32Shr(andit, shift);
  }

  auto do_store = __ MakeLabel(MachineRepresentation::kWord32);
  // We can store a smi anywhere.
  __ GotoIf(ObjectIsSmi(value), &do_store, kind);

  // {value} is a HeapObject.
  auto transition_smi_array = __ MakeDeferredLabel();
  auto transition_double_to_fast = __ MakeDeferredLabel();
  {
    __ GotoIfNot(IsElementsKindGreaterThan(kind, HOLEY_SMI_ELEMENTS),
                 &transition_smi_array);
    __ GotoIfNot(IsElementsKindGreaterThan(kind, HOLEY_ELEMENTS), &do_store,
                 kind);

    // We have double elements kind. Only a HeapNumber can be stored
    // without effecting a transition.
    Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
    Node* heap_number_map = __ HeapNumberMapConstant();
    Node* check = __ WordEqual(value_map, heap_number_map);
    __ GotoIfNot(check, &transition_double_to_fast);
    __ Goto(&do_store, kind);
  }

  __ Bind(&transition_smi_array);  // deferred code.
  {
    // Transition {array} from HOLEY_SMI_ELEMENTS to HOLEY_DOUBLE_ELEMENTS or
    // to HOLEY_ELEMENTS.
    auto if_value_not_heap_number = __ MakeLabel();
    Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
    Node* heap_number_map = __ HeapNumberMapConstant();
    Node* check = __ WordEqual(value_map, heap_number_map);
    __ GotoIfNot(check, &if_value_not_heap_number);
    {
      // {value} is a HeapNumber.
      TransitionElementsTo(node, array, HOLEY_SMI_ELEMENTS,
                           HOLEY_DOUBLE_ELEMENTS);
      __ Goto(&do_store, __ Int32Constant(HOLEY_DOUBLE_ELEMENTS));
    }
    __ Bind(&if_value_not_heap_number);
    {
      TransitionElementsTo(node, array, HOLEY_SMI_ELEMENTS, HOLEY_ELEMENTS);
      __ Goto(&do_store, __ Int32Constant(HOLEY_ELEMENTS));
    }
  }

  __ Bind(&transition_double_to_fast);  // deferred code.
  {
    TransitionElementsTo(node, array, HOLEY_DOUBLE_ELEMENTS, HOLEY_ELEMENTS);
    __ Goto(&do_store, __ Int32Constant(HOLEY_ELEMENTS));
  }

  // Make sure kind is up-to-date.
  __ Bind(&do_store);
  kind = do_store.PhiAt(0);

  Node* elements = __ LoadField(AccessBuilder::ForJSObjectElements(), array);
  auto if_kind_is_double = __ MakeLabel();
  auto done = __ MakeLabel();
  __ GotoIf(IsElementsKindGreaterThan(kind, HOLEY_ELEMENTS),
            &if_kind_is_double);
  {
    // Our ElementsKind is HOLEY_SMI_ELEMENTS or HOLEY_ELEMENTS.
    __ StoreElement(AccessBuilder::ForFixedArrayElement(HOLEY_ELEMENTS),
                    elements, index, value);
    __ Goto(&done);
  }
  __ Bind(&if_kind_is_double);
  {
    // Our ElementsKind is HOLEY_DOUBLE_ELEMENTS.
    auto do_double_store = __ MakeLabel();
    __ GotoIfNot(ObjectIsSmi(value), &do_double_store);
    {
      Node* int_value = ChangeSmiToInt32(value);
      Node* float_value = __ ChangeInt32ToFloat64(int_value);
      __ StoreElement(AccessBuilder::ForFixedDoubleArrayElement(), elements,
                      index, float_value);
      __ Goto(&done);
    }
    __ Bind(&do_double_store);
    {
      Node* float_value =
          __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
      __ StoreElement(AccessBuilder::ForFixedDoubleArrayElement(), elements,
                      index, float_value);
      __ Goto(&done);
    }
  }

  __ Bind(&done);
}

void EffectControlLinearizer::LowerTransitionAndStoreNumberElement(Node* node) {
  Node* array = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);  // This is a Float64, not tagged.

  // Possibly transition array based on input and store.
  //
  //   -- TRANSITION PHASE -----------------
  //   kind = ElementsKind(array)
  //   if kind == HOLEY_SMI_ELEMENTS {
  //     Transition array to HOLEY_DOUBLE_ELEMENTS
  //   } else if kind != HOLEY_DOUBLE_ELEMENTS {
  //     This is UNREACHABLE, execute a debug break.
  //   }
  //
  //   -- STORE PHASE ----------------------
  //   Store array[index] = value (it's a float)
  //
  Node* map = __ LoadField(AccessBuilder::ForMap(), array);
  Node* kind;
  {
    Node* bit_field2 = __ LoadField(AccessBuilder::ForMapBitField2(), map);
    Node* mask = __ Int32Constant(Map::ElementsKindBits::kMask);
    Node* andit = __ Word32And(bit_field2, mask);
    Node* shift = __ Int32Constant(Map::ElementsKindBits::kShift);
    kind = __ Word32Shr(andit, shift);
  }

  auto do_store = __ MakeLabel();

  // {value} is a float64.
  auto transition_smi_array = __ MakeDeferredLabel();
  {
    __ GotoIfNot(IsElementsKindGreaterThan(kind, HOLEY_SMI_ELEMENTS),
                 &transition_smi_array);
    // We expect that our input array started at HOLEY_SMI_ELEMENTS, and
    // climbs the lattice up to HOLEY_DOUBLE_ELEMENTS. Force a debug break
    // if this assumption is broken. It also would be the case that
    // loop peeling can break this assumption.
    __ GotoIf(__ Word32Equal(kind, __ Int32Constant(HOLEY_DOUBLE_ELEMENTS)),
              &do_store);
    // TODO(turbofan): It would be good to have an "Unreachable()" node type.
    __ DebugBreak();
    __ Goto(&do_store);
  }

  __ Bind(&transition_smi_array);  // deferred code.
  {
    // Transition {array} from HOLEY_SMI_ELEMENTS to HOLEY_DOUBLE_ELEMENTS.
    TransitionElementsTo(node, array, HOLEY_SMI_ELEMENTS,
                         HOLEY_DOUBLE_ELEMENTS);
    __ Goto(&do_store);
  }

  __ Bind(&do_store);

  Node* elements = __ LoadField(AccessBuilder::ForJSObjectElements(), array);
  __ StoreElement(AccessBuilder::ForFixedDoubleArrayElement(), elements, index,
                  value);
}

void EffectControlLinearizer::LowerTransitionAndStoreNonNumberElement(
    Node* node) {
  Node* array = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  // Possibly transition array based on input and store.
  //
  //   -- TRANSITION PHASE -----------------
  //   kind = ElementsKind(array)
  //   if kind == HOLEY_SMI_ELEMENTS {
  //     Transition array to HOLEY_ELEMENTS
  //   } else if kind == HOLEY_DOUBLE_ELEMENTS {
  //     Transition array to HOLEY_ELEMENTS
  //   }
  //
  //   -- STORE PHASE ----------------------
  //   // kind is HOLEY_ELEMENTS
  //   Store array[index] = value
  //
  Node* map = __ LoadField(AccessBuilder::ForMap(), array);
  Node* kind;
  {
    Node* bit_field2 = __ LoadField(AccessBuilder::ForMapBitField2(), map);
    Node* mask = __ Int32Constant(Map::ElementsKindBits::kMask);
    Node* andit = __ Word32And(bit_field2, mask);
    Node* shift = __ Int32Constant(Map::ElementsKindBits::kShift);
    kind = __ Word32Shr(andit, shift);
  }

  auto do_store = __ MakeLabel();

  auto transition_smi_array = __ MakeDeferredLabel();
  auto transition_double_to_fast = __ MakeDeferredLabel();
  {
    __ GotoIfNot(IsElementsKindGreaterThan(kind, HOLEY_SMI_ELEMENTS),
                 &transition_smi_array);
    __ GotoIf(IsElementsKindGreaterThan(kind, HOLEY_ELEMENTS),
              &transition_double_to_fast);
    __ Goto(&do_store);
  }

  __ Bind(&transition_smi_array);  // deferred code.
  {
    // Transition {array} from HOLEY_SMI_ELEMENTS to HOLEY_ELEMENTS.
    TransitionElementsTo(node, array, HOLEY_SMI_ELEMENTS, HOLEY_ELEMENTS);
    __ Goto(&do_store);
  }

  __ Bind(&transition_double_to_fast);  // deferred code.
  {
    TransitionElementsTo(node, array, HOLEY_DOUBLE_ELEMENTS, HOLEY_ELEMENTS);
    __ Goto(&do_store);
  }

  __ Bind(&do_store);

  Node* elements = __ LoadField(AccessBuilder::ForJSObjectElements(), array);
  // Our ElementsKind is HOLEY_ELEMENTS.
  ElementAccess access = AccessBuilder::ForFixedArrayElement(HOLEY_ELEMENTS);
  Type value_type = ValueTypeParameterOf(node->op());
  if (value_type.Is(Type::BooleanOrNullOrUndefined())) {
    access.type = value_type;
    access.write_barrier_kind = kNoWriteBarrier;
  }
  __ StoreElement(access, elements, index, value);
}

void EffectControlLinearizer::LowerStoreSignedSmallElement(Node* node) {
  Node* array = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);  // int32

  // Store a signed small in an output array.
  //
  //   kind = ElementsKind(array)
  //
  //   -- STORE PHASE ----------------------
  //   if kind == HOLEY_DOUBLE_ELEMENTS {
  //     float_value = convert int32 to float
  //     Store array[index] = float_value
  //   } else {
  //     // kind is HOLEY_SMI_ELEMENTS or HOLEY_ELEMENTS
  //     smi_value = convert int32 to smi
  //     Store array[index] = smi_value
  //   }
  //
  Node* map = __ LoadField(AccessBuilder::ForMap(), array);
  Node* kind;
  {
    Node* bit_field2 = __ LoadField(AccessBuilder::ForMapBitField2(), map);
    Node* mask = __ Int32Constant(Map::ElementsKindBits::kMask);
    Node* andit = __ Word32And(bit_field2, mask);
    Node* shift = __ Int32Constant(Map::ElementsKindBits::kShift);
    kind = __ Word32Shr(andit, shift);
  }

  Node* elements = __ LoadField(AccessBuilder::ForJSObjectElements(), array);
  auto if_kind_is_double = __ MakeLabel();
  auto done = __ MakeLabel();
  __ GotoIf(IsElementsKindGreaterThan(kind, HOLEY_ELEMENTS),
            &if_kind_is_double);
  {
    // Our ElementsKind is HOLEY_SMI_ELEMENTS or HOLEY_ELEMENTS.
    // In this case, we know our value is a signed small, and we can optimize
    // the ElementAccess information.
    ElementAccess access = AccessBuilder::ForFixedArrayElement();
    access.type = Type::SignedSmall();
    access.machine_type = MachineType::TaggedSigned();
    access.write_barrier_kind = kNoWriteBarrier;
    Node* smi_value = ChangeInt32ToSmi(value);
    __ StoreElement(access, elements, index, smi_value);
    __ Goto(&done);
  }
  __ Bind(&if_kind_is_double);
  {
    // Our ElementsKind is HOLEY_DOUBLE_ELEMENTS.
    Node* float_value = __ ChangeInt32ToFloat64(value);
    __ StoreElement(AccessBuilder::ForFixedDoubleArrayElement(), elements,
                    index, float_value);
    __ Goto(&done);
  }

  __ Bind(&done);
}

void EffectControlLinearizer::LowerRuntimeAbort(Node* node) {
  AbortReason reason = AbortReasonOf(node->op());
  Operator::Properties properties = Operator::kNoDeopt | Operator::kNoThrow;
  Runtime::FunctionId id = Runtime::kAbort;
  auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
      graph()->zone(), id, 1, properties, CallDescriptor::kNoFlags);
  __ Call(call_descriptor, __ CEntryStubConstant(1),
          jsgraph()->SmiConstant(static_cast<int>(reason)),
          __ ExternalConstant(ExternalReference::Create(id)),
          __ Int32Constant(1), __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerConvertReceiver(Node* node) {
  ConvertReceiverMode const mode = ConvertReceiverModeOf(node->op());
  Node* value = node->InputAt(0);
  Node* global_proxy = node->InputAt(1);

  switch (mode) {
    case ConvertReceiverMode::kNullOrUndefined: {
      return global_proxy;
    }
    case ConvertReceiverMode::kNotNullOrUndefined: {
      auto convert_to_object = __ MakeDeferredLabel();
      auto done_convert = __ MakeLabel(MachineRepresentation::kTagged);

      // Check if {value} is already a JSReceiver.
      __ GotoIf(ObjectIsSmi(value), &convert_to_object);
      STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
      Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
      Node* value_instance_type =
          __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
      Node* check = __ Uint32LessThan(
          value_instance_type, __ Uint32Constant(FIRST_JS_RECEIVER_TYPE));
      __ GotoIf(check, &convert_to_object);
      __ Goto(&done_convert, value);

      // Wrap the primitive {value} into a JSValue.
      __ Bind(&convert_to_object);
      Operator::Properties properties = Operator::kEliminatable;
      Callable callable = Builtins::CallableFor(isolate(), Builtins::kToObject);
      CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          graph()->zone(), callable.descriptor(),
          callable.descriptor().GetStackParameterCount(), flags, properties);
      Node* native_context = __ LoadField(
          AccessBuilder::ForJSGlobalProxyNativeContext(), global_proxy);
      Node* result = __ Call(call_descriptor, __ HeapConstant(callable.code()),
                             value, native_context);
      __ Goto(&done_convert, result);

      __ Bind(&done_convert);
      return done_convert.PhiAt(0);
    }
    case ConvertReceiverMode::kAny: {
      auto convert_to_object = __ MakeDeferredLabel();
      auto convert_global_proxy = __ MakeDeferredLabel();
      auto done_convert = __ MakeLabel(MachineRepresentation::kTagged);

      // Check if {value} is already a JSReceiver, or null/undefined.
      __ GotoIf(ObjectIsSmi(value), &convert_to_object);
      STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
      Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
      Node* value_instance_type =
          __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
      Node* check = __ Uint32LessThan(
          value_instance_type, __ Uint32Constant(FIRST_JS_RECEIVER_TYPE));
      __ GotoIf(check, &convert_to_object);
      __ Goto(&done_convert, value);

      // Wrap the primitive {value} into a JSValue.
      __ Bind(&convert_to_object);
      __ GotoIf(__ WordEqual(value, __ UndefinedConstant()),
                &convert_global_proxy);
      __ GotoIf(__ WordEqual(value, __ NullConstant()), &convert_global_proxy);
      Operator::Properties properties = Operator::kEliminatable;
      Callable callable = Builtins::CallableFor(isolate(), Builtins::kToObject);
      CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          graph()->zone(), callable.descriptor(),
          callable.descriptor().GetStackParameterCount(), flags, properties);
      Node* native_context = __ LoadField(
          AccessBuilder::ForJSGlobalProxyNativeContext(), global_proxy);
      Node* result = __ Call(call_descriptor, __ HeapConstant(callable.code()),
                             value, native_context);
      __ Goto(&done_convert, result);

      // Replace the {value} with the {global_proxy}.
      __ Bind(&convert_global_proxy);
      __ Goto(&done_convert, global_proxy);

      __ Bind(&done_convert);
      return done_convert.PhiAt(0);
    }
  }

  UNREACHABLE();
  return nullptr;
}

Maybe<Node*> EffectControlLinearizer::LowerFloat64RoundUp(Node* node) {
  // Nothing to be done if a fast hardware instruction is available.
  if (machine()->Float64RoundUp().IsSupported()) {
    return Nothing<Node*>();
  }

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

  auto if_not_positive = __ MakeDeferredLabel();
  auto if_greater_than_two_52 = __ MakeDeferredLabel();
  auto if_less_than_minus_two_52 = __ MakeDeferredLabel();
  auto if_zero = __ MakeDeferredLabel();
  auto done_temp3 = __ MakeLabel(MachineRepresentation::kFloat64);
  auto done = __ MakeLabel(MachineRepresentation::kFloat64);

  Node* const zero = __ Float64Constant(0.0);
  Node* const two_52 = __ Float64Constant(4503599627370496.0E0);
  Node* const one = __ Float64Constant(1.0);

  Node* check0 = __ Float64LessThan(zero, input);
  __ GotoIfNot(check0, &if_not_positive);
  {
    Node* check1 = __ Float64LessThanOrEqual(two_52, input);
    __ GotoIf(check1, &if_greater_than_two_52);
    {
      Node* temp1 = __ Float64Sub(__ Float64Add(two_52, input), two_52);
      __ GotoIfNot(__ Float64LessThan(temp1, input), &done, temp1);
      __ Goto(&done, __ Float64Add(temp1, one));
    }

    __ Bind(&if_greater_than_two_52);
    __ Goto(&done, input);
  }

  __ Bind(&if_not_positive);
  {
    Node* check1 = __ Float64Equal(input, zero);
    __ GotoIf(check1, &if_zero);

    Node* const minus_two_52 = __ Float64Constant(-4503599627370496.0E0);
    Node* check2 = __ Float64LessThanOrEqual(input, minus_two_52);
    __ GotoIf(check2, &if_less_than_minus_two_52);

    {
      Node* const minus_zero = __ Float64Constant(-0.0);
      Node* temp1 = __ Float64Sub(minus_zero, input);
      Node* temp2 = __ Float64Sub(__ Float64Add(two_52, temp1), two_52);
      Node* check3 = __ Float64LessThan(temp1, temp2);
      __ GotoIfNot(check3, &done_temp3, temp2);
      __ Goto(&done_temp3, __ Float64Sub(temp2, one));

      __ Bind(&done_temp3);
      Node* temp3 = done_temp3.PhiAt(0);
      __ Goto(&done, __ Float64Sub(minus_zero, temp3));
    }
    __ Bind(&if_less_than_minus_two_52);
    __ Goto(&done, input);

    __ Bind(&if_zero);
    __ Goto(&done, input);
  }
  __ Bind(&done);
  return Just(done.PhiAt(0));
}

Node* EffectControlLinearizer::BuildFloat64RoundDown(Node* value) {
  if (machine()->Float64RoundDown().IsSupported()) {
    return __ Float64RoundDown(value);
  }

  Node* const input = value;

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

  auto if_not_positive = __ MakeDeferredLabel();
  auto if_greater_than_two_52 = __ MakeDeferredLabel();
  auto if_less_than_minus_two_52 = __ MakeDeferredLabel();
  auto if_temp2_lt_temp1 = __ MakeLabel();
  auto if_zero = __ MakeDeferredLabel();
  auto done = __ MakeLabel(MachineRepresentation::kFloat64);

  Node* const zero = __ Float64Constant(0.0);
  Node* const two_52 = __ Float64Constant(4503599627370496.0E0);

  Node* check0 = __ Float64LessThan(zero, input);
  __ GotoIfNot(check0, &if_not_positive);
  {
    Node* check1 = __ Float64LessThanOrEqual(two_52, input);
    __ GotoIf(check1, &if_greater_than_two_52);
    {
      Node* const one = __ Float64Constant(1.0);
      Node* temp1 = __ Float64Sub(__ Float64Add(two_52, input), two_52);
      __ GotoIfNot(__ Float64LessThan(input, temp1), &done, temp1);
      __ Goto(&done, __ Float64Sub(temp1, one));
    }

    __ Bind(&if_greater_than_two_52);
    __ Goto(&done, input);
  }

  __ Bind(&if_not_positive);
  {
    Node* check1 = __ Float64Equal(input, zero);
    __ GotoIf(check1, &if_zero);

    Node* const minus_two_52 = __ Float64Constant(-4503599627370496.0E0);
    Node* check2 = __ Float64LessThanOrEqual(input, minus_two_52);
    __ GotoIf(check2, &if_less_than_minus_two_52);

    {
      Node* const minus_zero = __ Float64Constant(-0.0);
      Node* temp1 = __ Float64Sub(minus_zero, input);
      Node* temp2 = __ Float64Sub(__ Float64Add(two_52, temp1), two_52);
      Node* check3 = __ Float64LessThan(temp2, temp1);
      __ GotoIf(check3, &if_temp2_lt_temp1);
      __ Goto(&done, __ Float64Sub(minus_zero, temp2));

      __ Bind(&if_temp2_lt_temp1);
      __ Goto(&done, __ Float64Sub(__ Float64Constant(-1.0), temp2));
    }
    __ Bind(&if_less_than_minus_two_52);
    __ Goto(&done, input);

    __ Bind(&if_zero);
    __ Goto(&done, input);
  }
  __ Bind(&done);
  return done.PhiAt(0);
}

Maybe<Node*> EffectControlLinearizer::LowerFloat64RoundDown(Node* node) {
  // Nothing to be done if a fast hardware instruction is available.
  if (machine()->Float64RoundDown().IsSupported()) {
    return Nothing<Node*>();
  }

  Node* const input = node->InputAt(0);
  return Just(BuildFloat64RoundDown(input));
}

Maybe<Node*> EffectControlLinearizer::LowerFloat64RoundTiesEven(Node* node) {
  // Nothing to be done if a fast hardware instruction is available.
  if (machine()->Float64RoundTiesEven().IsSupported()) {
    return Nothing<Node*>();
  }

  Node* const input = node->InputAt(0);

  // Generate case for round ties to even:
  //
  //   let value = floor(input) in
  //   let temp1 = input - value in
  //   if temp1 < 0.5 then
  //     value
  //   else if 0.5 < temp1 then
  //     value + 1.0
  //   else
  //     let temp2 = value % 2.0 in
  //     if temp2 == 0.0 then
  //       value
  //     else
  //       value + 1.0

  auto if_is_half = __ MakeLabel();
  auto done = __ MakeLabel(MachineRepresentation::kFloat64);

  Node* value = BuildFloat64RoundDown(input);
  Node* temp1 = __ Float64Sub(input, value);

  Node* const half = __ Float64Constant(0.5);
  Node* check0 = __ Float64LessThan(temp1, half);
  __ GotoIf(check0, &done, value);

  Node* const one = __ Float64Constant(1.0);
  Node* check1 = __ Float64LessThan(half, temp1);
  __ GotoIfNot(check1, &if_is_half);
  __ Goto(&done, __ Float64Add(value, one));

  __ Bind(&if_is_half);
  Node* temp2 = __ Float64Mod(value, __ Float64Constant(2.0));
  Node* check2 = __ Float64Equal(temp2, __ Float64Constant(0.0));
  __ GotoIf(check2, &done, value);
  __ Goto(&done, __ Float64Add(value, one));

  __ Bind(&done);
  return Just(done.PhiAt(0));
}

Node* EffectControlLinearizer::BuildFloat64RoundTruncate(Node* input) {
  if (machine()->Float64RoundTruncate().IsSupported()) {
    return __ Float64RoundTruncate(input);
  }
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

  auto if_not_positive = __ MakeDeferredLabel();
  auto if_greater_than_two_52 = __ MakeDeferredLabel();
  auto if_less_than_minus_two_52 = __ MakeDeferredLabel();
  auto if_zero = __ MakeDeferredLabel();
  auto done_temp3 = __ MakeLabel(MachineRepresentation::kFloat64);
  auto done = __ MakeLabel(MachineRepresentation::kFloat64);

  Node* const zero = __ Float64Constant(0.0);
  Node* const two_52 = __ Float64Constant(4503599627370496.0E0);
  Node* const one = __ Float64Constant(1.0);

  Node* check0 = __ Float64LessThan(zero, input);
  __ GotoIfNot(check0, &if_not_positive);
  {
    Node* check1 = __ Float64LessThanOrEqual(two_52, input);
    __ GotoIf(check1, &if_greater_than_two_52);
    {
      Node* temp1 = __ Float64Sub(__ Float64Add(two_52, input), two_52);
      __ GotoIfNot(__ Float64LessThan(input, temp1), &done, temp1);
      __ Goto(&done, __ Float64Sub(temp1, one));
    }

    __ Bind(&if_greater_than_two_52);
    __ Goto(&done, input);
  }

  __ Bind(&if_not_positive);
  {
    Node* check1 = __ Float64Equal(input, zero);
    __ GotoIf(check1, &if_zero);

    Node* const minus_two_52 = __ Float64Constant(-4503599627370496.0E0);
    Node* check2 = __ Float64LessThanOrEqual(input, minus_two_52);
    __ GotoIf(check2, &if_less_than_minus_two_52);

    {
      Node* const minus_zero = __ Float64Constant(-0.0);
      Node* temp1 = __ Float64Sub(minus_zero, input);
      Node* temp2 = __ Float64Sub(__ Float64Add(two_52, temp1), two_52);
      Node* check3 = __ Float64LessThan(temp1, temp2);
      __ GotoIfNot(check3, &done_temp3, temp2);
      __ Goto(&done_temp3, __ Float64Sub(temp2, one));

      __ Bind(&done_temp3);
      Node* temp3 = done_temp3.PhiAt(0);
      __ Goto(&done, __ Float64Sub(minus_zero, temp3));
    }
    __ Bind(&if_less_than_minus_two_52);
    __ Goto(&done, input);

    __ Bind(&if_zero);
    __ Goto(&done, input);
  }
  __ Bind(&done);
  return done.PhiAt(0);
}

Maybe<Node*> EffectControlLinearizer::LowerFloat64RoundTruncate(Node* node) {
  // Nothing to be done if a fast hardware instruction is available.
  if (machine()->Float64RoundTruncate().IsSupported()) {
    return Nothing<Node*>();
  }

  Node* const input = node->InputAt(0);
  return Just(BuildFloat64RoundTruncate(input));
}

Node* EffectControlLinearizer::LowerFindOrderedHashMapEntry(Node* node) {
  Node* table = NodeProperties::GetValueInput(node, 0);
  Node* key = NodeProperties::GetValueInput(node, 1);

  {
    Callable const callable =
        Builtins::CallableFor(isolate(), Builtins::kFindOrderedHashMapEntry);
    Operator::Properties const properties = node->op()->properties();
    CallDescriptor::Flags const flags = CallDescriptor::kNoFlags;
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        graph()->zone(), callable.descriptor(),
        callable.descriptor().GetStackParameterCount(), flags, properties);
    return __ Call(call_descriptor, __ HeapConstant(callable.code()), table,
                   key, __ NoContextConstant());
  }
}

Node* EffectControlLinearizer::ComputeUnseededHash(Node* value) {
  // See v8::internal::ComputeUnseededHash()
  value = __ Int32Add(__ Word32Xor(value, __ Int32Constant(0xFFFFFFFF)),
                      __ Word32Shl(value, __ Int32Constant(15)));
  value = __ Word32Xor(value, __ Word32Shr(value, __ Int32Constant(12)));
  value = __ Int32Add(value, __ Word32Shl(value, __ Int32Constant(2)));
  value = __ Word32Xor(value, __ Word32Shr(value, __ Int32Constant(4)));
  value = __ Int32Mul(value, __ Int32Constant(2057));
  value = __ Word32Xor(value, __ Word32Shr(value, __ Int32Constant(16)));
  value = __ Word32And(value, __ Int32Constant(0x3FFFFFFF));
  return value;
}

Node* EffectControlLinearizer::LowerFindOrderedHashMapEntryForInt32Key(
    Node* node) {
  Node* table = NodeProperties::GetValueInput(node, 0);
  Node* key = NodeProperties::GetValueInput(node, 1);

  // Compute the integer hash code.
  Node* hash = ChangeUint32ToUintPtr(ComputeUnseededHash(key));

  Node* number_of_buckets = ChangeSmiToIntPtr(__ LoadField(
      AccessBuilder::ForOrderedHashMapOrSetNumberOfBuckets(), table));
  hash = __ WordAnd(hash, __ IntSub(number_of_buckets, __ IntPtrConstant(1)));
  Node* first_entry = ChangeSmiToIntPtr(__ Load(
      MachineType::TaggedSigned(), table,
      __ IntAdd(__ WordShl(hash, __ IntPtrConstant(kTaggedSizeLog2)),
                __ IntPtrConstant(OrderedHashMap::HashTableStartOffset() -
                                  kHeapObjectTag))));

  auto loop = __ MakeLoopLabel(MachineType::PointerRepresentation());
  auto done = __ MakeLabel(MachineType::PointerRepresentation());
  __ Goto(&loop, first_entry);
  __ Bind(&loop);
  {
    Node* entry = loop.PhiAt(0);
    Node* check =
        __ WordEqual(entry, __ IntPtrConstant(OrderedHashMap::kNotFound));
    __ GotoIf(check, &done, entry);
    entry = __ IntAdd(
        __ IntMul(entry, __ IntPtrConstant(OrderedHashMap::kEntrySize)),
        number_of_buckets);

    Node* candidate_key = __ Load(
        MachineType::AnyTagged(), table,
        __ IntAdd(__ WordShl(entry, __ IntPtrConstant(kTaggedSizeLog2)),
                  __ IntPtrConstant(OrderedHashMap::HashTableStartOffset() -
                                    kHeapObjectTag)));

    auto if_match = __ MakeLabel();
    auto if_notmatch = __ MakeLabel();
    auto if_notsmi = __ MakeDeferredLabel();
    __ GotoIfNot(ObjectIsSmi(candidate_key), &if_notsmi);
    __ Branch(__ Word32Equal(ChangeSmiToInt32(candidate_key), key), &if_match,
              &if_notmatch);

    __ Bind(&if_notsmi);
    __ GotoIfNot(
        __ WordEqual(__ LoadField(AccessBuilder::ForMap(), candidate_key),
                     __ HeapNumberMapConstant()),
        &if_notmatch);
    __ Branch(__ Float64Equal(__ LoadField(AccessBuilder::ForHeapNumberValue(),
                                           candidate_key),
                              __ ChangeInt32ToFloat64(key)),
              &if_match, &if_notmatch);

    __ Bind(&if_match);
    __ Goto(&done, entry);

    __ Bind(&if_notmatch);
    {
      Node* next_entry = ChangeSmiToIntPtr(__ Load(
          MachineType::TaggedSigned(), table,
          __ IntAdd(
              __ WordShl(entry, __ IntPtrConstant(kTaggedSizeLog2)),
              __ IntPtrConstant(OrderedHashMap::HashTableStartOffset() +
                                OrderedHashMap::kChainOffset * kTaggedSize -
                                kHeapObjectTag))));
      __ Goto(&loop, next_entry);
    }
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerDateNow(Node* node) {
  Operator::Properties properties = Operator::kNoDeopt | Operator::kNoThrow;
  Runtime::FunctionId id = Runtime::kDateCurrentTime;
  auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
      graph()->zone(), id, 0, properties, CallDescriptor::kNoFlags);
  return __ Call(call_descriptor, __ CEntryStubConstant(1),
                 __ ExternalConstant(ExternalReference::Create(id)),
                 __ Int32Constant(0), __ NoContextConstant());
}

#undef __

Factory* EffectControlLinearizer::factory() const {
  return isolate()->factory();
}

Isolate* EffectControlLinearizer::isolate() const {
  return jsgraph()->isolate();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

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
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/schedule.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

EffectControlLinearizer::EffectControlLinearizer(
    JSGraph* js_graph, Schedule* schedule, Zone* temp_zone,
    SourcePositionTable* source_positions)
    : js_graph_(js_graph),
      schedule_(schedule),
      temp_zone_(temp_zone),
      source_positions_(source_positions),
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

void UpdateEffectPhi(Node* node, BasicBlock* block,
                     BlockEffectControlMap* block_effects) {
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

void TryCloneBranch(Node* node, BasicBlock* block, Zone* temp_zone,
                    Graph* graph, CommonOperatorBuilder* common,
                    BlockEffectControlMap* block_effects,
                    SourcePositionTable* source_positions) {
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
                       common(), &block_effects, source_positions_);
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

void EffectControlLinearizer::ProcessNode(Node* node, Node** frame_state,
                                          Node** effect, Node** control) {
  SourcePositionTable::Scope scope(source_positions_,
                                   source_positions_->GetSourcePosition(node));

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
  // Update the current control.
  if (node->op()->ControlOutputCount() > 0) {
    *control = node;
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
    case IrOpcode::kChangeUint32ToTagged:
      result = LowerChangeUint32ToTagged(node);
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
    case IrOpcode::kChangeTaggedToBit:
      result = LowerChangeTaggedToBit(node);
      break;
    case IrOpcode::kChangeTaggedToInt32:
      result = LowerChangeTaggedToInt32(node);
      break;
    case IrOpcode::kChangeTaggedToUint32:
      result = LowerChangeTaggedToUint32(node);
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
    case IrOpcode::kCheckBounds:
      result = LowerCheckBounds(node, frame_state);
      break;
    case IrOpcode::kCheckMaps:
      result = LowerCheckMaps(node, frame_state);
      break;
    case IrOpcode::kCheckNumber:
      result = LowerCheckNumber(node, frame_state);
      break;
    case IrOpcode::kCheckReceiver:
      result = LowerCheckReceiver(node, frame_state);
      break;
    case IrOpcode::kCheckString:
      result = LowerCheckString(node, frame_state);
      break;
    case IrOpcode::kCheckInternalizedString:
      result = LowerCheckInternalizedString(node, frame_state);
      break;
    case IrOpcode::kCheckIf:
      result = LowerCheckIf(node, frame_state);
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
    case IrOpcode::kCheckedUint32ToInt32:
      result = LowerCheckedUint32ToInt32(node, frame_state);
      break;
    case IrOpcode::kCheckedUint32ToTaggedSigned:
      result = LowerCheckedUint32ToTaggedSigned(node, frame_state);
      break;
    case IrOpcode::kCheckedFloat64ToInt32:
      result = LowerCheckedFloat64ToInt32(node, frame_state);
      break;
    case IrOpcode::kCheckedTaggedSignedToInt32:
      if (frame_state == nullptr) {
        V8_Fatal(__FILE__, __LINE__, "No frame state (zapped by #%d: %s)",
                 frame_state_zapper_->id(),
                 frame_state_zapper_->op()->mnemonic());
      }
      result = LowerCheckedTaggedSignedToInt32(node, frame_state);
      break;
    case IrOpcode::kCheckedTaggedToInt32:
      result = LowerCheckedTaggedToInt32(node, frame_state);
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
    case IrOpcode::kObjectIsDetectableCallable:
      result = LowerObjectIsDetectableCallable(node);
      break;
    case IrOpcode::kObjectIsNaN:
      result = LowerObjectIsNaN(node);
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
    case IrOpcode::kNewUnmappedArgumentsElements:
      result = LowerNewUnmappedArgumentsElements(node);
      break;
    case IrOpcode::kArrayBufferWasNeutered:
      result = LowerArrayBufferWasNeutered(node);
      break;
    case IrOpcode::kStringFromCharCode:
      result = LowerStringFromCharCode(node);
      break;
    case IrOpcode::kStringFromCodePoint:
      result = LowerStringFromCodePoint(node);
      break;
    case IrOpcode::kStringIndexOf:
      result = LowerStringIndexOf(node);
      break;
    case IrOpcode::kStringCharAt:
      result = LowerStringCharAt(node);
      break;
    case IrOpcode::kStringCharCodeAt:
      result = LowerStringCharCodeAt(node);
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
    case IrOpcode::kCheckFloat64Hole:
      result = LowerCheckFloat64Hole(node, frame_state);
      break;
    case IrOpcode::kCheckTaggedHole:
      result = LowerCheckTaggedHole(node, frame_state);
      break;
    case IrOpcode::kConvertTaggedHoleToUndefined:
      result = LowerConvertTaggedHoleToUndefined(node);
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
    case IrOpcode::kLoadTypedElement:
      result = LowerLoadTypedElement(node);
      break;
    case IrOpcode::kStoreTypedElement:
      LowerStoreTypedElement(node);
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
    default:
      return false;
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

  auto done = __ MakeLabel<2>(MachineRepresentation::kTagged);
  auto if_heapnumber =
      __ MakeLabelFor(GraphAssemblerLabelType::kNonDeferred,
                      1 + (mode == CheckForMinusZeroMode::kCheckForMinusZero) +
                          !machine()->Is64());
  auto if_int32 = __ MakeLabel<1>();

  Node* value32 = __ RoundFloat64ToInt32(value);
  __ GotoIf(__ Float64Equal(value, __ ChangeInt32ToFloat64(value32)),
            &if_int32);
  __ Goto(&if_heapnumber);

  __ Bind(&if_int32);
  {
    if (mode == CheckForMinusZeroMode::kCheckForMinusZero) {
      Node* zero = __ Int32Constant(0);
      auto if_zero = __ MakeDeferredLabel<1>();
      auto if_smi = __ MakeLabel<2>();

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

    if (machine()->Is64()) {
      Node* value_smi = ChangeInt32ToSmi(value32);
      __ Goto(&done, value_smi);
    } else {
      Node* add = __ Int32AddWithOverflow(value32, value32);
      Node* ovf = __ Projection(1, add);
      __ GotoIf(ovf, &if_heapnumber);
      Node* value_smi = __ Projection(0, add);
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

  auto if_true = __ MakeLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kTagged);

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

  if (machine()->Is64()) {
    return ChangeInt32ToSmi(value);
  }

  auto if_overflow = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kTagged);

  Node* add = __ Int32AddWithOverflow(value, value);
  Node* ovf = __ Projection(1, add);
  __ GotoIf(ovf, &if_overflow);
  __ Goto(&done, __ Projection(0, add));

  __ Bind(&if_overflow);
  Node* number = AllocateHeapNumberWithValue(__ ChangeInt32ToFloat64(value));
  __ Goto(&done, number);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeUint32ToTagged(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_in_smi_range = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kTagged);

  Node* check = __ Uint32LessThanOrEqual(value, SmiMaxValueConstant());
  __ GotoUnless(check, &if_not_in_smi_range);
  __ Goto(&done, ChangeUint32ToSmi(value));

  __ Bind(&if_not_in_smi_range);
  Node* number = AllocateHeapNumberWithValue(__ ChangeUint32ToFloat64(value));

  __ Goto(&done, number);
  __ Bind(&done);

  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeTaggedSignedToInt32(Node* node) {
  Node* value = node->InputAt(0);
  return ChangeSmiToInt32(value);
}

Node* EffectControlLinearizer::LowerChangeTaggedToBit(Node* node) {
  Node* value = node->InputAt(0);
  return __ WordEqual(value, __ TrueConstant());
}

Node* EffectControlLinearizer::LowerTruncateTaggedToBit(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeDeferredLabel<1>();
  auto if_heapnumber = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<6>(MachineRepresentation::kBit);

  Node* zero = __ Int32Constant(0);
  Node* fzero = __ Float64Constant(0.0);

  // Check if {value} is false.
  __ GotoIf(__ WordEqual(value, __ FalseConstant()), &done, zero);

  // Check if {value} is a Smi.
  Node* check_smi = ObjectIsSmi(value);
  __ GotoIf(check_smi, &if_smi);

  // Check if {value} is the empty string.
  __ GotoIf(__ WordEqual(value, __ EmptyStringConstant()), &done, zero);

  // Load the map of {value}.
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);

  // Check if the {value} is undetectable and immediately return false.
  Node* value_map_bitfield =
      __ LoadField(AccessBuilder::ForMapBitField(), value_map);
  __ GotoUnless(
      __ Word32Equal(__ Word32And(value_map_bitfield,
                                  __ Int32Constant(1 << Map::kIsUndetectable)),
                     zero),
      &done, zero);

  // Check if {value} is a HeapNumber.
  __ GotoIf(__ WordEqual(value_map, __ HeapNumberMapConstant()),
            &if_heapnumber);

  // All other values that reach here are true.
  __ Goto(&done, __ Int32Constant(1));

  __ Bind(&if_heapnumber);
  {
    // For HeapNumber {value}, just check that its value is not 0.0, -0.0 or
    // NaN.
    Node* value_value =
        __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
    __ Goto(&done, __ Float64LessThan(fzero, __ Float64Abs(value_value)));
  }

  __ Bind(&if_smi);
  {
    // If {value} is a Smi, then we only need to check that it's not zero.
    __ Goto(&done,
            __ Word32Equal(__ WordEqual(value, __ IntPtrConstant(0)), zero));
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerTruncateTaggedPointerToBit(Node* node) {
  Node* value = node->InputAt(0);

  auto if_heapnumber = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<5>(MachineRepresentation::kBit);

  Node* zero = __ Int32Constant(0);
  Node* fzero = __ Float64Constant(0.0);

  // Check if {value} is false.
  __ GotoIf(__ WordEqual(value, __ FalseConstant()), &done, zero);

  // Check if {value} is the empty string.
  __ GotoIf(__ WordEqual(value, __ EmptyStringConstant()), &done, zero);

  // Load the map of {value}.
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);

  // Check if the {value} is undetectable and immediately return false.
  Node* value_map_bitfield =
      __ LoadField(AccessBuilder::ForMapBitField(), value_map);
  __ GotoUnless(
      __ Word32Equal(__ Word32And(value_map_bitfield,
                                  __ Int32Constant(1 << Map::kIsUndetectable)),
                     zero),
      &done, zero);

  // Check if {value} is a HeapNumber.
  __ GotoIf(__ WordEqual(value_map, __ HeapNumberMapConstant()),
            &if_heapnumber);

  // All other values that reach here are true.
  __ Goto(&done, __ Int32Constant(1));

  __ Bind(&if_heapnumber);
  {
    // For HeapNumber {value}, just check that its value is not 0.0, -0.0 or
    // NaN.
    Node* value_value =
        __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
    __ Goto(&done, __ Float64LessThan(fzero, __ Float64Abs(value_value)));
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeTaggedToInt32(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoUnless(check, &if_not_smi);
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

  auto if_not_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoUnless(check, &if_not_smi);
  __ Goto(&done, ChangeSmiToInt32(value));

  __ Bind(&if_not_smi);
  STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
  Node* vfalse = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  vfalse = __ ChangeFloat64ToUint32(vfalse);
  __ Goto(&done, vfalse);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerChangeTaggedToFloat64(Node* node) {
  return LowerTruncateTaggedToFloat64(node);
}

Node* EffectControlLinearizer::LowerChangeTaggedToTaggedSigned(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoUnless(check, &if_not_smi);
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

  auto if_not_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kFloat64);

  Node* check = ObjectIsSmi(value);
  __ GotoUnless(check, &if_not_smi);
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

Node* EffectControlLinearizer::LowerCheckBounds(Node* node, Node* frame_state) {
  Node* index = node->InputAt(0);
  Node* limit = node->InputAt(1);

  Node* check = __ Uint32LessThan(index, limit);
  __ DeoptimizeUnless(DeoptimizeReason::kOutOfBounds, check, frame_state);
  return index;
}

Node* EffectControlLinearizer::LowerCheckMaps(Node* node, Node* frame_state) {
  CheckMapsParameters const& p = CheckMapsParametersOf(node->op());
  Node* value = node->InputAt(0);

  ZoneHandleSet<Map> const& maps = p.maps();
  size_t const map_count = maps.size();

  if (p.flags() & CheckMapsFlag::kTryMigrateInstance) {
    auto done =
        __ MakeLabelFor(GraphAssemblerLabelType::kNonDeferred, map_count * 2);
    auto migrate = __ MakeDeferredLabel<1>();

    // Load the current map of the {value}.
    Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);

    // Perform the map checks.
    for (size_t i = 0; i < map_count; ++i) {
      Node* map = __ HeapConstant(maps[i]);
      Node* check = __ WordEqual(value_map, map);
      if (i == map_count - 1) {
        __ GotoUnless(check, &migrate);
        __ Goto(&done);
      } else {
        __ GotoIf(check, &done);
      }
    }

    // Perform the (deferred) instance migration.
    __ Bind(&migrate);
    {
      // If map is not deprecated the migration attempt does not make sense.
      Node* bitfield3 =
          __ LoadField(AccessBuilder::ForMapBitField3(), value_map);
      Node* if_not_deprecated = __ WordEqual(
          __ Word32And(bitfield3, __ Int32Constant(Map::Deprecated::kMask)),
          __ Int32Constant(0));
      __ DeoptimizeIf(DeoptimizeReason::kWrongMap, if_not_deprecated,
                      frame_state);

      Operator::Properties properties = Operator::kNoDeopt | Operator::kNoThrow;
      Runtime::FunctionId id = Runtime::kTryMigrateInstance;
      CallDescriptor const* desc = Linkage::GetRuntimeCallDescriptor(
          graph()->zone(), id, 1, properties, CallDescriptor::kNoFlags);
      Node* result =
          __ Call(desc, __ CEntryStubConstant(1), value,
                  __ ExternalConstant(ExternalReference(id, isolate())),
                  __ Int32Constant(1), __ NoContextConstant());
      Node* check = ObjectIsSmi(result);
      __ DeoptimizeIf(DeoptimizeReason::kInstanceMigrationFailed, check,
                      frame_state);
    }

    // Reload the current map of the {value}.
    value_map = __ LoadField(AccessBuilder::ForMap(), value);

    // Perform the map checks again.
    for (size_t i = 0; i < map_count; ++i) {
      Node* map = __ HeapConstant(maps[i]);
      Node* check = __ WordEqual(value_map, map);
      if (i == map_count - 1) {
        __ DeoptimizeUnless(DeoptimizeReason::kWrongMap, check, frame_state);
      } else {
        __ GotoIf(check, &done);
      }
    }

    __ Goto(&done);
    __ Bind(&done);
  } else {
    auto done =
        __ MakeLabelFor(GraphAssemblerLabelType::kNonDeferred, map_count);

    // Load the current map of the {value}.
    Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);

    for (size_t i = 0; i < map_count; ++i) {
      Node* map = __ HeapConstant(maps[i]);
      Node* check = __ WordEqual(value_map, map);
      if (i == map_count - 1) {
        __ DeoptimizeUnless(DeoptimizeReason::kWrongMap, check, frame_state);
      } else {
        __ GotoIf(check, &done);
      }
    }
    __ Goto(&done);
    __ Bind(&done);
  }
  return value;
}

Node* EffectControlLinearizer::LowerCheckNumber(Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>();

  Node* check0 = ObjectIsSmi(value);
  __ GotoUnless(check0, &if_not_smi);
  __ Goto(&done);

  __ Bind(&if_not_smi);
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* check1 = __ WordEqual(value_map, __ HeapNumberMapConstant());
  __ DeoptimizeUnless(DeoptimizeReason::kNotAHeapNumber, check1, frame_state);
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
  __ DeoptimizeUnless(DeoptimizeReason::kNotAJavaScriptObject, check,
                      frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerCheckString(Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);

  Node* check = __ Uint32LessThan(value_instance_type,
                                  __ Uint32Constant(FIRST_NONSTRING_TYPE));
  __ DeoptimizeUnless(DeoptimizeReason::kWrongInstanceType, check, frame_state);
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
  __ DeoptimizeUnless(DeoptimizeReason::kWrongInstanceType, check, frame_state);

  return value;
}

Node* EffectControlLinearizer::LowerCheckIf(Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  __ DeoptimizeUnless(DeoptimizeKind::kEager, DeoptimizeReason::kNoReason,
                      value, frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerCheckedInt32Add(Node* node,
                                                    Node* frame_state) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* value = __ Int32AddWithOverflow(lhs, rhs);
  Node* check = __ Projection(1, value);
  __ DeoptimizeIf(DeoptimizeReason::kOverflow, check, frame_state);
  return __ Projection(0, value);
}

Node* EffectControlLinearizer::LowerCheckedInt32Sub(Node* node,
                                                    Node* frame_state) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* value = __ Int32SubWithOverflow(lhs, rhs);
  Node* check = __ Projection(1, value);
  __ DeoptimizeIf(DeoptimizeReason::kOverflow, check, frame_state);
  return __ Projection(0, value);
}

Node* EffectControlLinearizer::LowerCheckedInt32Div(Node* node,
                                                    Node* frame_state) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  auto if_not_positive = __ MakeDeferredLabel<1>();
  auto if_is_minint = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kWord32);
  auto minint_check_done = __ MakeLabel<2>();

  Node* zero = __ Int32Constant(0);

  // Check if {rhs} is positive (and not zero).
  Node* check0 = __ Int32LessThan(zero, rhs);
  __ GotoUnless(check0, &if_not_positive);

  // Fast case, no additional checking required.
  __ Goto(&done, __ Int32Div(lhs, rhs));

  {
    __ Bind(&if_not_positive);

    // Check if {rhs} is zero.
    Node* check = __ Word32Equal(rhs, zero);
    __ DeoptimizeIf(DeoptimizeReason::kDivisionByZero, check, frame_state);

    // Check if {lhs} is zero, as that would produce minus zero.
    check = __ Word32Equal(lhs, zero);
    __ DeoptimizeIf(DeoptimizeReason::kMinusZero, check, frame_state);

    // Check if {lhs} is kMinInt and {rhs} is -1, in which case we'd have
    // to return -kMinInt, which is not representable.
    Node* minint = __ Int32Constant(std::numeric_limits<int32_t>::min());
    Node* check1 = graph()->NewNode(machine()->Word32Equal(), lhs, minint);
    __ GotoIf(check1, &if_is_minint);
    __ Goto(&minint_check_done);

    __ Bind(&if_is_minint);
    // Check if {rhs} is -1.
    Node* minusone = __ Int32Constant(-1);
    Node* is_minus_one = __ Word32Equal(rhs, minusone);
    __ DeoptimizeIf(DeoptimizeReason::kOverflow, is_minus_one, frame_state);
    __ Goto(&minint_check_done);

    __ Bind(&minint_check_done);
    // Perform the actual integer division.
    __ Goto(&done, __ Int32Div(lhs, rhs));
  }

  __ Bind(&done);
  Node* value = done.PhiAt(0);

  // Check if the remainder is non-zero.
  Node* check = __ Word32Equal(lhs, __ Int32Mul(rhs, value));
  __ DeoptimizeUnless(DeoptimizeReason::kLostPrecision, check, frame_state);

  return value;
}

Node* EffectControlLinearizer::LowerCheckedInt32Mod(Node* node,
                                                    Node* frame_state) {
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

  auto if_rhs_not_positive = __ MakeDeferredLabel<1>();
  auto if_lhs_negative = __ MakeDeferredLabel<1>();
  auto if_power_of_two = __ MakeLabel<1>();
  auto rhs_checked = __ MakeLabel<2>(MachineRepresentation::kWord32);
  auto done = __ MakeLabel<3>(MachineRepresentation::kWord32);

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
    Node* check = __ Word32Equal(vtrue0, zero);
    __ DeoptimizeIf(DeoptimizeReason::kDivisionByZero, check, frame_state);
    __ Goto(&rhs_checked, vtrue0);
  }

  __ Bind(&rhs_checked);
  rhs = rhs_checked.PhiAt(0);

  // Check if {lhs} is negative.
  Node* check1 = __ Int32LessThan(lhs, zero);
  __ GotoIf(check1, &if_lhs_negative);

  // {lhs} non-negative.
  {
    Node* one = __ Int32Constant(1);
    Node* msk = __ Int32Sub(rhs, one);

    // Check if {rhs} minus one is a valid mask.
    Node* check2 = __ Word32Equal(__ Word32And(rhs, msk), zero);
    __ GotoIf(check2, &if_power_of_two);
    // Compute the remainder using the generic {lhs % rhs}.
    __ Goto(&done, __ Int32Mod(lhs, rhs));

    __ Bind(&if_power_of_two);
    // Compute the remainder using {lhs & msk}.
    __ Goto(&done, __ Word32And(lhs, msk));
  }

  __ Bind(&if_lhs_negative);
  {
    // Compute the remainder using {lhs % msk}.
    Node* vtrue1 = __ Int32Mod(lhs, rhs);

    // Check if we would have to return -0.
    Node* check = __ Word32Equal(vtrue1, zero);
    __ DeoptimizeIf(DeoptimizeReason::kMinusZero, check, frame_state);
    __ Goto(&done, vtrue1);
  }

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerCheckedUint32Div(Node* node,
                                                     Node* frame_state) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* zero = __ Int32Constant(0);

  // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
  Node* check = __ Word32Equal(rhs, zero);
  __ DeoptimizeIf(DeoptimizeReason::kDivisionByZero, check, frame_state);

  // Perform the actual unsigned integer division.
  Node* value = __ Uint32Div(lhs, rhs);

  // Check if the remainder is non-zero.
  check = __ Word32Equal(lhs, __ Int32Mul(rhs, value));
  __ DeoptimizeUnless(DeoptimizeReason::kLostPrecision, check, frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerCheckedUint32Mod(Node* node,
                                                     Node* frame_state) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* zero = __ Int32Constant(0);

  // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
  Node* check = __ Word32Equal(rhs, zero);
  __ DeoptimizeIf(DeoptimizeReason::kDivisionByZero, check, frame_state);

  // Perform the actual unsigned integer modulus.
  return __ Uint32Mod(lhs, rhs);
}

Node* EffectControlLinearizer::LowerCheckedInt32Mul(Node* node,
                                                    Node* frame_state) {
  CheckForMinusZeroMode mode = CheckMinusZeroModeOf(node->op());
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* projection = __ Int32MulWithOverflow(lhs, rhs);
  Node* check = __ Projection(1, projection);
  __ DeoptimizeIf(DeoptimizeReason::kOverflow, check, frame_state);

  Node* value = __ Projection(0, projection);

  if (mode == CheckForMinusZeroMode::kCheckForMinusZero) {
    auto if_zero = __ MakeDeferredLabel<1>();
    auto check_done = __ MakeLabel<2>();
    Node* zero = __ Int32Constant(0);
    Node* check_zero = __ Word32Equal(value, zero);
    __ GotoIf(check_zero, &if_zero);
    __ Goto(&check_done);

    __ Bind(&if_zero);
    // We may need to return negative zero.
    Node* check_or = __ Int32LessThan(__ Word32Or(lhs, rhs), zero);
    __ DeoptimizeIf(DeoptimizeReason::kMinusZero, check_or, frame_state);
    __ Goto(&check_done);

    __ Bind(&check_done);
  }

  return value;
}

Node* EffectControlLinearizer::LowerCheckedInt32ToTaggedSigned(
    Node* node, Node* frame_state) {
  DCHECK(SmiValuesAre31Bits());
  Node* value = node->InputAt(0);

  Node* add = __ Int32AddWithOverflow(value, value);
  Node* check = __ Projection(1, add);
  __ DeoptimizeIf(DeoptimizeReason::kOverflow, check, frame_state);
  return __ Projection(0, add);
}

Node* EffectControlLinearizer::LowerCheckedUint32ToInt32(Node* node,
                                                         Node* frame_state) {
  Node* value = node->InputAt(0);
  Node* unsafe = __ Int32LessThan(value, __ Int32Constant(0));
  __ DeoptimizeIf(DeoptimizeReason::kLostPrecision, unsafe, frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerCheckedUint32ToTaggedSigned(
    Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  Node* check = __ Uint32LessThanOrEqual(value, SmiMaxValueConstant());
  __ DeoptimizeUnless(DeoptimizeReason::kLostPrecision, check, frame_state);
  return ChangeUint32ToSmi(value);
}

Node* EffectControlLinearizer::BuildCheckedFloat64ToInt32(
    CheckForMinusZeroMode mode, Node* value, Node* frame_state) {
  Node* value32 = __ RoundFloat64ToInt32(value);
  Node* check_same = __ Float64Equal(value, __ ChangeInt32ToFloat64(value32));
  __ DeoptimizeUnless(DeoptimizeReason::kLostPrecisionOrNaN, check_same,
                      frame_state);

  if (mode == CheckForMinusZeroMode::kCheckForMinusZero) {
    // Check if {value} is -0.
    auto if_zero = __ MakeDeferredLabel<1>();
    auto check_done = __ MakeLabel<2>();

    Node* check_zero = __ Word32Equal(value32, __ Int32Constant(0));
    __ GotoIf(check_zero, &if_zero);
    __ Goto(&check_done);

    __ Bind(&if_zero);
    // In case of 0, we need to check the high bits for the IEEE -0 pattern.
    Node* check_negative = __ Int32LessThan(__ Float64ExtractHighWord32(value),
                                            __ Int32Constant(0));
    __ DeoptimizeIf(DeoptimizeReason::kMinusZero, check_negative, frame_state);
    __ Goto(&check_done);

    __ Bind(&check_done);
  }
  return value32;
}

Node* EffectControlLinearizer::LowerCheckedFloat64ToInt32(Node* node,
                                                          Node* frame_state) {
  CheckForMinusZeroMode mode = CheckMinusZeroModeOf(node->op());
  Node* value = node->InputAt(0);
  return BuildCheckedFloat64ToInt32(mode, value, frame_state);
}

Node* EffectControlLinearizer::LowerCheckedTaggedSignedToInt32(
    Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);
  Node* check = ObjectIsSmi(value);
  __ DeoptimizeUnless(DeoptimizeReason::kNotASmi, check, frame_state);
  return ChangeSmiToInt32(value);
}

Node* EffectControlLinearizer::LowerCheckedTaggedToInt32(Node* node,
                                                         Node* frame_state) {
  CheckForMinusZeroMode mode = CheckMinusZeroModeOf(node->op());
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoUnless(check, &if_not_smi);
  // In the Smi case, just convert to int32.
  __ Goto(&done, ChangeSmiToInt32(value));

  // In the non-Smi case, check the heap numberness, load the number and convert
  // to int32.
  __ Bind(&if_not_smi);
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* check_map = __ WordEqual(value_map, __ HeapNumberMapConstant());
  __ DeoptimizeUnless(DeoptimizeReason::kNotAHeapNumber, check_map,
                      frame_state);
  Node* vfalse = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  vfalse = BuildCheckedFloat64ToInt32(mode, vfalse, frame_state);
  __ Goto(&done, vfalse);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::BuildCheckedHeapNumberOrOddballToFloat64(
    CheckTaggedInputMode mode, Node* value, Node* frame_state) {
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* check_number = __ WordEqual(value_map, __ HeapNumberMapConstant());
  switch (mode) {
    case CheckTaggedInputMode::kNumber: {
      __ DeoptimizeUnless(DeoptimizeReason::kNotAHeapNumber, check_number,
                          frame_state);
      break;
    }
    case CheckTaggedInputMode::kNumberOrOddball: {
      auto check_done = __ MakeLabel<2>();

      __ GotoIf(check_number, &check_done);
      // For oddballs also contain the numeric value, let us just check that
      // we have an oddball here.
      Node* instance_type =
          __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
      Node* check_oddball =
          __ Word32Equal(instance_type, __ Int32Constant(ODDBALL_TYPE));
      __ DeoptimizeUnless(DeoptimizeReason::kNotANumberOrOddball, check_oddball,
                          frame_state);
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
  CheckTaggedInputMode mode = CheckTaggedInputModeOf(node->op());
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kFloat64);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);

  // In the Smi case, just convert to int32 and then float64.
  // Otherwise, check heap numberness and load the number.
  Node* number =
      BuildCheckedHeapNumberOrOddballToFloat64(mode, value, frame_state);
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

  Node* check = ObjectIsSmi(value);
  __ DeoptimizeUnless(DeoptimizeReason::kNotASmi, check, frame_state);

  return value;
}

Node* EffectControlLinearizer::LowerCheckedTaggedToTaggedPointer(
    Node* node, Node* frame_state) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  __ DeoptimizeIf(DeoptimizeReason::kSmi, check, frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerTruncateTaggedToWord32(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoUnless(check, &if_not_smi);
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
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kWord32);

  Node* check = ObjectIsSmi(value);
  __ GotoUnless(check, &if_not_smi);
  // In the Smi case, just convert to int32.
  __ Goto(&done, ChangeSmiToInt32(value));

  // Otherwise, check that it's a heap number or oddball and truncate the value
  // to int32.
  __ Bind(&if_not_smi);
  Node* number = BuildCheckedHeapNumberOrOddballToFloat64(
      CheckTaggedInputMode::kNumberOrOddball, value, frame_state);
  number = __ TruncateFloat64ToWord32(number);
  __ Goto(&done, number);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsDetectableCallable(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kBit);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_bit_field =
      __ LoadField(AccessBuilder::ForMapBitField(), value_map);
  Node* vfalse = __ Word32Equal(
      __ Int32Constant(1 << Map::kIsCallable),
      __ Word32And(value_bit_field,
                   __ Int32Constant((1 << Map::kIsCallable) |
                                    (1 << Map::kIsUndetectable))));
  __ Goto(&done, vfalse);

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsNaN(Node* node) {
  Node* value = node->InputAt(0);
  Node* zero = __ Int32Constant(0);

  auto done = __ MakeLabel<3>(MachineRepresentation::kBit);

  // Check if {value} is a Smi.
  __ GotoIf(ObjectIsSmi(value), &done, zero);

  // Check if {value} is a HeapNumber.
  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  __ GotoUnless(__ WordEqual(value_map, __ HeapNumberMapConstant()), &done,
                zero);

  // Check if {value} contains a NaN.
  Node* value_value = __ LoadField(AccessBuilder::ForHeapNumberValue(), value);
  __ Goto(&done,
          __ Word32Equal(__ Float64Equal(value_value, value_value), zero));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsNonCallable(Node* node) {
  Node* value = node->InputAt(0);

  auto if_primitive = __ MakeDeferredLabel<2>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kBit);

  Node* check0 = ObjectIsSmi(value);
  __ GotoIf(check0, &if_primitive);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_instance_type =
      __ LoadField(AccessBuilder::ForMapInstanceType(), value_map);
  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  Node* check1 = __ Uint32LessThanOrEqual(
      __ Uint32Constant(FIRST_JS_RECEIVER_TYPE), value_instance_type);
  __ GotoUnless(check1, &if_primitive);

  Node* value_bit_field =
      __ LoadField(AccessBuilder::ForMapBitField(), value_map);
  Node* check2 = __ Word32Equal(
      __ Int32Constant(0),
      __ Word32And(value_bit_field, __ Int32Constant(1 << Map::kIsCallable)));
  __ Goto(&done, check2);

  __ Bind(&if_primitive);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerObjectIsNumber(Node* node) {
  Node* value = node->InputAt(0);

  auto if_smi = __ MakeLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kBit);

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

  auto if_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kBit);

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

  auto if_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kBit);

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

  auto if_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kBit);

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

  auto if_smi = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kBit);

  Node* check = ObjectIsSmi(value);
  __ GotoIf(check, &if_smi);

  Node* value_map = __ LoadField(AccessBuilder::ForMap(), value);
  Node* value_bit_field =
      __ LoadField(AccessBuilder::ForMapBitField(), value_map);
  Node* vfalse = __ Word32Equal(
      __ Word32Equal(__ Int32Constant(0),
                     __ Word32And(value_bit_field,
                                  __ Int32Constant(1 << Map::kIsUndetectable))),
      __ Int32Constant(0));
  __ Goto(&done, vfalse);

  __ Bind(&if_smi);
  __ Goto(&done, __ Int32Constant(0));

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerArgumentsLength(Node* node) {
  Node* arguments_frame = NodeProperties::GetValueInput(node, 0);
  int formal_parameter_count = FormalParameterCountOf(node->op());
  bool is_rest_length = IsRestLengthOf(node->op());
  DCHECK(formal_parameter_count >= 0);

  if (is_rest_length) {
    // The ArgumentsLength node is computing the number of rest parameters,
    // which is max(0, actual_parameter_count - formal_parameter_count).
    // We have to distinguish the case, when there is an arguments adaptor frame
    // (i.e., arguments_frame != LoadFramePointer()).
    auto if_adaptor_frame = __ MakeLabel<1>();
    auto done = __ MakeLabel<3>(MachineRepresentation::kTaggedSigned);

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
    auto if_adaptor_frame = __ MakeLabel<1>();
    auto done = __ MakeLabel<2>(MachineRepresentation::kTaggedSigned);

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
  auto done = __ MakeLabel<2>(MachineType::PointerRepresentation());

  Node* frame = __ LoadFramePointer();
  Node* parent_frame =
      __ Load(MachineType::AnyTagged(), frame,
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

Node* EffectControlLinearizer::LowerNewUnmappedArgumentsElements(Node* node) {
  Node* frame = NodeProperties::GetValueInput(node, 0);
  Node* length = NodeProperties::GetValueInput(node, 1);

  Callable const callable =
      CodeFactory::NewUnmappedArgumentsElements(isolate());
  Operator::Properties const properties = node->op()->properties();
  CallDescriptor::Flags const flags = CallDescriptor::kNoFlags;
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), callable.descriptor(), 0, flags, properties);
  return __ Call(desc, __ HeapConstant(callable.code()), frame, length,
                 __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerArrayBufferWasNeutered(Node* node) {
  Node* value = node->InputAt(0);

  Node* value_bit_field =
      __ LoadField(AccessBuilder::ForJSArrayBufferBitField(), value);
  return __ Word32Equal(
      __ Word32Equal(
          __ Word32And(value_bit_field,
                       __ Int32Constant(JSArrayBuffer::WasNeutered::kMask)),
          __ Int32Constant(0)),
      __ Int32Constant(0));
}

Node* EffectControlLinearizer::LowerStringCharAt(Node* node) {
  Node* receiver = node->InputAt(0);
  Node* position = node->InputAt(1);

  Callable const callable = CodeFactory::StringCharAt(isolate());
  Operator::Properties properties = Operator::kNoThrow | Operator::kNoWrite;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), callable.descriptor(), 0, flags, properties);
  return __ Call(desc, __ HeapConstant(callable.code()), receiver, position,
                 __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerStringCharCodeAt(Node* node) {
  Node* receiver = node->InputAt(0);
  Node* position = node->InputAt(1);

  Callable const callable = CodeFactory::StringCharCodeAt(isolate());
  Operator::Properties properties = Operator::kNoThrow | Operator::kNoWrite;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), callable.descriptor(), 0, flags, properties,
      MachineType::TaggedSigned());
  return __ Call(desc, __ HeapConstant(callable.code()), receiver, position,
                 __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerStringFromCharCode(Node* node) {
  Node* value = node->InputAt(0);

  auto runtime_call = __ MakeDeferredLabel<2>();
  auto if_undefined = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kTagged);

  // Compute the character code.
  Node* code = __ Word32And(value, __ Int32Constant(String::kMaxUtf16CodeUnit));

  // Check if the {code} is a one-byte char code.
  Node* check0 = __ Int32LessThanOrEqual(
      code, __ Int32Constant(String::kMaxOneByteCharCode));
  __ GotoUnless(check0, &runtime_call);

  // Load the isolate wide single character string cache.
  Node* cache = __ HeapConstant(factory()->single_character_string_cache());

  // Compute the {cache} index for {code}.
  Node* index = machine()->Is32() ? code : __ ChangeUint32ToUint64(code);

  // Check if we have an entry for the {code} in the single character string
  // cache already.
  Node* entry =
      __ LoadElement(AccessBuilder::ForFixedArrayElement(), cache, index);

  Node* check1 = __ WordEqual(entry, __ UndefinedConstant());
  __ GotoIf(check1, &runtime_call);
  __ Goto(&done, entry);

  // Let %StringFromCharCode handle this case.
  // TODO(turbofan): At some point we may consider adding a stub for this
  // deferred case, so that we don't need to call to C++ here.
  __ Bind(&runtime_call);
  {
    Operator::Properties properties = Operator::kNoDeopt | Operator::kNoThrow;
    Runtime::FunctionId id = Runtime::kStringCharFromCode;
    CallDescriptor const* desc = Linkage::GetRuntimeCallDescriptor(
        graph()->zone(), id, 1, properties, CallDescriptor::kNoFlags);
    Node* vtrue1 =
        __ Call(desc, __ CEntryStubConstant(1), ChangeInt32ToSmi(code),
                __ ExternalConstant(ExternalReference(id, isolate())),
                __ Int32Constant(1), __ NoContextConstant());
    __ Goto(&done, vtrue1);
  }
  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerStringFromCodePoint(Node* node) {
  Node* value = node->InputAt(0);
  Node* code = value;

  auto if_not_single_code = __ MakeDeferredLabel<1>();
  auto if_not_one_byte = __ MakeDeferredLabel<1>();
  auto cache_miss = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<4>(MachineRepresentation::kTagged);

  // Check if the {code} is a single code unit
  Node* check0 = __ Uint32LessThanOrEqual(code, __ Uint32Constant(0xFFFF));
  __ GotoUnless(check0, &if_not_single_code);

  {
    // Check if the {code} is a one byte character
    Node* check1 = __ Uint32LessThanOrEqual(
        code, __ Uint32Constant(String::kMaxOneByteCharCode));
    __ GotoUnless(check1, &if_not_one_byte);
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
                      __ IntPtrConstant(Name::kEmptyHashField));
        __ StoreField(AccessBuilder::ForStringLength(), vtrue2,
                      __ SmiConstant(1));
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
                    __ SmiConstant(1));
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
        code = __ Word32Or(__ Word32Shl(trail, __ Int32Constant(16)), lead);
        break;
      }
    }

    // Allocate a new SeqTwoByteString for {code}.
    Node* vfalse0 = __ Allocate(NOT_TENURED,
                                __ Int32Constant(SeqTwoByteString::SizeFor(2)));
    __ StoreField(AccessBuilder::ForMap(), vfalse0,
                  __ HeapConstant(factory()->string_map()));
    __ StoreField(AccessBuilder::ForNameHashField(), vfalse0,
                  __ IntPtrConstant(Name::kEmptyHashField));
    __ StoreField(AccessBuilder::ForStringLength(), vfalse0, __ SmiConstant(2));
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

  Callable callable = CodeFactory::StringIndexOf(isolate());
  Operator::Properties properties = Operator::kEliminatable;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), callable.descriptor(), 0, flags, properties);
  return __ Call(desc, __ HeapConstant(callable.code()), subject, search_string,
                 position, __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerStringComparison(Callable const& callable,
                                                     Node* node) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Operator::Properties properties = Operator::kEliminatable;
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), callable.descriptor(), 0, flags, properties);
  return __ Call(desc, __ HeapConstant(callable.code()), lhs, rhs,
                 __ NoContextConstant());
}

Node* EffectControlLinearizer::LowerStringEqual(Node* node) {
  return LowerStringComparison(CodeFactory::StringEqual(isolate()), node);
}

Node* EffectControlLinearizer::LowerStringLessThan(Node* node) {
  return LowerStringComparison(CodeFactory::StringLessThan(isolate()), node);
}

Node* EffectControlLinearizer::LowerStringLessThanOrEqual(Node* node) {
  return LowerStringComparison(CodeFactory::StringLessThanOrEqual(isolate()),
                               node);
}

Node* EffectControlLinearizer::LowerCheckFloat64Hole(Node* node,
                                                     Node* frame_state) {
  // If we reach this point w/o eliminating the {node} that's marked
  // with allow-return-hole, we cannot do anything, so just deoptimize
  // in case of the hole NaN (similar to Crankshaft).
  Node* value = node->InputAt(0);
  Node* check = __ Word32Equal(__ Float64ExtractHighWord32(value),
                               __ Int32Constant(kHoleNanUpper32));
  __ DeoptimizeIf(DeoptimizeReason::kHole, check, frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerCheckTaggedHole(Node* node,
                                                    Node* frame_state) {
  Node* value = node->InputAt(0);
  Node* check = __ WordEqual(value, __ TheHoleConstant());
  __ DeoptimizeIf(DeoptimizeReason::kHole, check, frame_state);
  return value;
}

Node* EffectControlLinearizer::LowerConvertTaggedHoleToUndefined(Node* node) {
  Node* value = node->InputAt(0);

  auto if_is_hole = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kTagged);

  Node* check = __ WordEqual(value, __ TheHoleConstant());
  __ GotoIf(check, &if_is_hole);
  __ Goto(&done, value);

  __ Bind(&if_is_hole);
  __ Goto(&done, __ UndefinedConstant());

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::AllocateHeapNumberWithValue(Node* value) {
  Node* result = __ Allocate(NOT_TENURED, __ Int32Constant(HeapNumber::kSize));
  __ StoreField(AccessBuilder::ForMap(), result, __ HeapNumberMapConstant());
  __ StoreField(AccessBuilder::ForHeapNumberValue(), result, value);
  return result;
}

Node* EffectControlLinearizer::ChangeInt32ToSmi(Node* value) {
  if (machine()->Is64()) {
    value = __ ChangeInt32ToInt64(value);
  }
  return __ WordShl(value, SmiShiftBitsConstant());
}

Node* EffectControlLinearizer::ChangeUint32ToSmi(Node* value) {
  if (machine()->Is64()) {
    value = __ ChangeUint32ToUint64(value);
  }
  return __ WordShl(value, SmiShiftBitsConstant());
}

Node* EffectControlLinearizer::ChangeSmiToInt32(Node* value) {
  value = __ WordSar(value, SmiShiftBitsConstant());
  if (machine()->Is64()) {
    value = __ TruncateInt64ToInt32(value);
  }
  return value;
}

Node* EffectControlLinearizer::ObjectIsSmi(Node* value) {
  return __ WordEqual(__ WordAnd(value, __ IntPtrConstant(kSmiTagMask)),
                      __ IntPtrConstant(kSmiTag));
}

Node* EffectControlLinearizer::SmiMaxValueConstant() {
  return __ Int32Constant(Smi::kMaxValue);
}

Node* EffectControlLinearizer::SmiShiftBitsConstant() {
  return __ IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}

Node* EffectControlLinearizer::LowerPlainPrimitiveToNumber(Node* node) {
  Node* value = node->InputAt(0);
  return __ ToNumber(value);
}

Node* EffectControlLinearizer::LowerPlainPrimitiveToWord32(Node* node) {
  Node* value = node->InputAt(0);

  auto if_not_smi = __ MakeDeferredLabel<1>();
  auto if_to_number_smi = __ MakeLabel<1>();
  auto done = __ MakeLabel<3>(MachineRepresentation::kWord32);

  Node* check0 = ObjectIsSmi(value);
  __ GotoUnless(check0, &if_not_smi);
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

  auto if_not_smi = __ MakeDeferredLabel<1>();
  auto if_to_number_smi = __ MakeLabel<1>();
  auto done = __ MakeLabel<3>(MachineRepresentation::kFloat64);

  Node* check0 = ObjectIsSmi(value);
  __ GotoUnless(check0, &if_not_smi);
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

  auto if_not_fixed_array = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>(MachineRepresentation::kTagged);

  // Load the current map of {elements}.
  Node* elements_map = __ LoadField(AccessBuilder::ForMap(), elements);

  // Check if {elements} is not a copy-on-write FixedArray.
  Node* check = __ WordEqual(elements_map, __ FixedArrayMapConstant());
  __ GotoUnless(check, &if_not_fixed_array);
  // Nothing to do if the {elements} are not copy-on-write.
  __ Goto(&done, elements);

  __ Bind(&if_not_fixed_array);
  // We need to take a copy of the {elements} and set them up for {object}.
  Operator::Properties properties = Operator::kEliminatable;
  Callable callable = CodeFactory::CopyFastSmiOrObjectElements(isolate());
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), callable.descriptor(), 0, flags, properties);
  Node* result = __ Call(desc, __ HeapConstant(callable.code()), object,
                         __ NoContextConstant());
  __ Goto(&done, result);

  __ Bind(&done);
  return done.PhiAt(0);
}

Node* EffectControlLinearizer::LowerMaybeGrowFastElements(Node* node,
                                                          Node* frame_state) {
  GrowFastElementsFlags flags = GrowFastElementsFlagsOf(node->op());
  Node* object = node->InputAt(0);
  Node* elements = node->InputAt(1);
  Node* index = node->InputAt(2);
  Node* length = node->InputAt(3);

  auto done = __ MakeLabel<2>(MachineRepresentation::kTagged);
  auto done_grow = __ MakeLabel<2>(MachineRepresentation::kTagged);
  auto if_grow = __ MakeDeferredLabel<1>();
  auto if_not_grow = __ MakeLabel<1>();

  Node* check0 = (flags & GrowFastElementsFlag::kHoleyElements)
                     ? __ Uint32LessThanOrEqual(length, index)
                     : __ Word32Equal(length, index);
  __ GotoUnless(check0, &if_not_grow);
  {
    // Load the length of the {elements} backing store.
    Node* elements_length =
        __ LoadField(AccessBuilder::ForFixedArrayLength(), elements);
    elements_length = ChangeSmiToInt32(elements_length);

    // Check if we need to grow the {elements} backing store.
    Node* check1 = __ Uint32LessThan(index, elements_length);
    __ GotoUnless(check1, &if_grow);
    __ Goto(&done_grow, elements);

    __ Bind(&if_grow);
    // We need to grow the {elements} for {object}.
    Operator::Properties properties = Operator::kEliminatable;
    Callable callable =
        (flags & GrowFastElementsFlag::kDoubleElements)
            ? CodeFactory::GrowFastDoubleElements(isolate())
            : CodeFactory::GrowFastSmiOrObjectElements(isolate());
    CallDescriptor::Flags call_flags = CallDescriptor::kNoFlags;
    CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0, call_flags,
        properties);
    Node* new_object = __ Call(desc, __ HeapConstant(callable.code()), object,
                               ChangeInt32ToSmi(index), __ NoContextConstant());

    // Ensure that we were able to grow the {elements}.
    // TODO(turbofan): We use kSmi as reason here similar to Crankshaft,
    // but maybe we should just introduce a reason that makes sense.
    __ DeoptimizeIf(DeoptimizeReason::kSmi, ObjectIsSmi(new_object),
                    frame_state);
    __ Goto(&done_grow, new_object);

    __ Bind(&done_grow);

    // For JSArray {object}s we also need to update the "length".
    if (flags & GrowFastElementsFlag::kArrayObject) {
      // Compute the new {length}.
      Node* object_length =
          ChangeInt32ToSmi(__ Int32Add(index, __ Int32Constant(1)));

      // Update the "length" property of the {object}.
      __ StoreField(AccessBuilder::ForJSArrayLength(FAST_ELEMENTS), object,
                    object_length);
    }
    __ Goto(&done, done_grow.PhiAt(0));
  }

  __ Bind(&if_not_grow);
  {
    // In case of non-holey {elements}, we need to verify that the {index} is
    // in-bounds, otherwise for holey {elements}, the check above already
    // guards the index (and the operator forces {index} to be unsigned).
    if (!(flags & GrowFastElementsFlag::kHoleyElements)) {
      Node* check1 = __ Uint32LessThan(index, length);
      __ DeoptimizeUnless(DeoptimizeReason::kOutOfBounds, check1, frame_state);
    }
    __ Goto(&done, elements);
  }
  __ Bind(&done);
  return done.PhiAt(0);
}

void EffectControlLinearizer::LowerTransitionElementsKind(Node* node) {
  ElementsTransition const transition = ElementsTransitionOf(node->op());
  Node* object = node->InputAt(0);

  auto if_map_same = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<2>();

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
      CallDescriptor const* desc = Linkage::GetRuntimeCallDescriptor(
          graph()->zone(), id, 2, properties, CallDescriptor::kNoFlags);
      __ Call(desc, __ CEntryStubConstant(1), object, target_map,
              __ ExternalConstant(ExternalReference(id, isolate())),
              __ Int32Constant(2), __ NoContextConstant());
      break;
    }
  }
  __ Goto(&done);

  __ Bind(&done);
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
  Node* storage = NumberMatcher(base).Is(0) ? external : __ UnsafePointerAdd(
                                                             base, external);

  // Perform the actual typed element access.
  return __ LoadElement(AccessBuilder::ForTypedArrayElement(array_type, true),
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
  Node* storage = NumberMatcher(base).Is(0) ? external : __ UnsafePointerAdd(
                                                             base, external);

  // Perform the actual typed element access.
  __ StoreElement(AccessBuilder::ForTypedArrayElement(array_type, true),
                  storage, index, value);
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

  auto if_not_positive = __ MakeDeferredLabel<1>();
  auto if_greater_than_two_52 = __ MakeDeferredLabel<1>();
  auto if_less_than_minus_two_52 = __ MakeDeferredLabel<1>();
  auto if_zero = __ MakeDeferredLabel<1>();
  auto done_temp3 = __ MakeLabel<2>(MachineRepresentation::kFloat64);
  auto done = __ MakeLabel<6>(MachineRepresentation::kFloat64);

  Node* const zero = __ Float64Constant(0.0);
  Node* const two_52 = __ Float64Constant(4503599627370496.0E0);
  Node* const one = __ Float64Constant(1.0);

  Node* check0 = __ Float64LessThan(zero, input);
  __ GotoUnless(check0, &if_not_positive);
  {
    Node* check1 = __ Float64LessThanOrEqual(two_52, input);
    __ GotoIf(check1, &if_greater_than_two_52);
    {
      Node* temp1 = __ Float64Sub(__ Float64Add(two_52, input), two_52);
      __ GotoUnless(__ Float64LessThan(temp1, input), &done, temp1);
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
      __ GotoUnless(check3, &done_temp3, temp2);
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
  Node* round_down = __ Float64RoundDown(value);
  if (round_down != nullptr) {
    return round_down;
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

  auto if_not_positive = __ MakeDeferredLabel<1>();
  auto if_greater_than_two_52 = __ MakeDeferredLabel<1>();
  auto if_less_than_minus_two_52 = __ MakeDeferredLabel<1>();
  auto if_temp2_lt_temp1 = __ MakeLabel<1>();
  auto if_zero = __ MakeDeferredLabel<1>();
  auto done = __ MakeLabel<7>(MachineRepresentation::kFloat64);

  Node* const zero = __ Float64Constant(0.0);
  Node* const two_52 = __ Float64Constant(4503599627370496.0E0);

  Node* check0 = __ Float64LessThan(zero, input);
  __ GotoUnless(check0, &if_not_positive);
  {
    Node* check1 = __ Float64LessThanOrEqual(two_52, input);
    __ GotoIf(check1, &if_greater_than_two_52);
    {
      Node* const one = __ Float64Constant(1.0);
      Node* temp1 = __ Float64Sub(__ Float64Add(two_52, input), two_52);
      __ GotoUnless(__ Float64LessThan(input, temp1), &done, temp1);
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

  auto if_is_half = __ MakeLabel<1>();
  auto done = __ MakeLabel<4>(MachineRepresentation::kFloat64);

  Node* value = BuildFloat64RoundDown(input);
  Node* temp1 = __ Float64Sub(input, value);

  Node* const half = __ Float64Constant(0.5);
  Node* check0 = __ Float64LessThan(temp1, half);
  __ GotoIf(check0, &done, value);

  Node* const one = __ Float64Constant(1.0);
  Node* check1 = __ Float64LessThan(half, temp1);
  __ GotoUnless(check1, &if_is_half);
  __ Goto(&done, __ Float64Add(value, one));

  __ Bind(&if_is_half);
  Node* temp2 = __ Float64Mod(value, __ Float64Constant(2.0));
  Node* check2 = __ Float64Equal(temp2, __ Float64Constant(0.0));
  __ GotoIf(check2, &done, value);
  __ Goto(&done, __ Float64Add(value, one));

  __ Bind(&done);
  return Just(done.PhiAt(0));
}

Maybe<Node*> EffectControlLinearizer::LowerFloat64RoundTruncate(Node* node) {
  // Nothing to be done if a fast hardware instruction is available.
  if (machine()->Float64RoundTruncate().IsSupported()) {
    return Nothing<Node*>();
  }

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

  auto if_not_positive = __ MakeDeferredLabel<1>();
  auto if_greater_than_two_52 = __ MakeDeferredLabel<1>();
  auto if_less_than_minus_two_52 = __ MakeDeferredLabel<1>();
  auto if_zero = __ MakeDeferredLabel<1>();
  auto done_temp3 = __ MakeLabel<2>(MachineRepresentation::kFloat64);
  auto done = __ MakeLabel<6>(MachineRepresentation::kFloat64);

  Node* const zero = __ Float64Constant(0.0);
  Node* const two_52 = __ Float64Constant(4503599627370496.0E0);
  Node* const one = __ Float64Constant(1.0);

  Node* check0 = __ Float64LessThan(zero, input);
  __ GotoUnless(check0, &if_not_positive);
  {
    Node* check1 = __ Float64LessThanOrEqual(two_52, input);
    __ GotoIf(check1, &if_greater_than_two_52);
    {
      Node* temp1 = __ Float64Sub(__ Float64Add(two_52, input), two_52);
      __ GotoUnless(__ Float64LessThan(input, temp1), &done, temp1);
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
      __ GotoUnless(check3, &done_temp3, temp2);
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

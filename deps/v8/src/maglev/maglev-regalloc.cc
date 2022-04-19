// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-regalloc.h"

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/compiler/backend/instruction.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-regalloc-data.h"

namespace v8 {
namespace internal {

namespace maglev {

namespace {

constexpr RegisterStateFlags initialized_node{true, false};
constexpr RegisterStateFlags initialized_merge{true, true};

using BlockReverseIterator = std::vector<BasicBlock>::reverse_iterator;

// A target is a fallthrough of a control node if its ID is the next ID
// after the control node.
//
// TODO(leszeks): Consider using the block iterator instead.
bool IsTargetOfNodeFallthrough(ControlNode* node, BasicBlock* target) {
  return node->id() + 1 == target->first_id();
}

ControlNode* NearestPostDominatingHole(ControlNode* node) {
  // Conditional control nodes don't cause holes themselves. So, the nearest
  // post-dominating hole is the conditional control node's next post-dominating
  // hole.
  if (node->Is<ConditionalControlNode>()) {
    return node->next_post_dominating_hole();
  }

  // If the node is a Jump, it may be a hole, but only if it is not a
  // fallthrough (jump to the immediately next block). Otherwise, it will point
  // to the nearest post-dominating hole in its own "next" field.
  if (Jump* jump = node->TryCast<Jump>()) {
    if (IsTargetOfNodeFallthrough(jump, jump->target())) {
      return jump->next_post_dominating_hole();
    }
  }

  return node;
}

bool IsLiveAtTarget(ValueNode* node, ControlNode* source, BasicBlock* target) {
  DCHECK_NOT_NULL(node);
  DCHECK(!node->is_dead());

  // If we're looping, a value can only be live if it was live before the loop.
  if (target->control_node()->id() <= source->id()) {
    // Gap moves may already be inserted in the target, so skip over those.
    return node->id() < target->FirstNonGapMoveId();
  }
  // TODO(verwaest): This should be true but isn't because we don't yet
  // eliminate dead code.
  // DCHECK_GT(node->next_use, source->id());
  // TODO(verwaest): Since we don't support deopt yet we can only deal with
  // direct branches. Add support for holes.
  return node->live_range().end >= target->first_id();
}

}  // namespace

StraightForwardRegisterAllocator::StraightForwardRegisterAllocator(
    MaglevCompilationUnit* compilation_unit, Graph* graph)
    : compilation_unit_(compilation_unit) {
  ComputePostDominatingHoles(graph);
  AllocateRegisters(graph);
  graph->set_stack_slots(top_of_stack_);
}

StraightForwardRegisterAllocator::~StraightForwardRegisterAllocator() = default;

// Compute, for all forward control nodes (i.e. excluding Return and JumpLoop) a
// tree of post-dominating control flow holes.
//
// Control flow which interrupts linear control flow fallthrough for basic
// blocks is considered to introduce a control flow "hole".
//
//                   A──────┐                │
//                   │ Jump │                │
//                   └──┬───┘                │
//                  {   │  B──────┐          │
//     Control flow {   │  │ Jump │          │ Linear control flow
//     hole after A {   │  └─┬────┘          │
//                  {   ▼    ▼ Fallthrough   │
//                     C──────┐              │
//                     │Return│              │
//                     └──────┘              ▼
//
// It is interesting, for each such hole, to know what the next hole will be
// that we will unconditionally reach on our way to an exit node. Such
// subsequent holes are in "post-dominators" of the current block.
//
// As an example, consider the following CFG, with the annotated holes. The
// post-dominating hole tree is the transitive closure of the post-dominator
// tree, up to nodes which are holes (in this example, A, D, F and H).
//
//                       CFG               Immediate       Post-dominating
//                                      post-dominators          holes
//                   A──────┐
//                   │ Jump │               A                 A
//                   └──┬───┘               │                 │
//                  {   │  B──────┐         │                 │
//     Control flow {   │  │ Jump │         │   B             │       B
//     hole after A {   │  └─┬────┘         │   │             │       │
//                  {   ▼    ▼              │   │             │       │
//                     C──────┐             │   │             │       │
//                     │Branch│             └►C◄┘             │   C   │
//                     └┬────┬┘               │               │   │   │
//                      ▼    │                │               │   │   │
//                   D──────┐│                │               │   │   │
//                   │ Jump ││              D │               │ D │   │
//                   └──┬───┘▼              │ │               │ │ │   │
//                  {   │  E──────┐         │ │               │ │ │   │
//     Control flow {   │  │ Jump │         │ │ E             │ │ │ E │
//     hole after D {   │  └─┬────┘         │ │ │             │ │ │ │ │
//                  {   ▼    ▼              │ │ │             │ │ │ │ │
//                     F──────┐             │ ▼ │             │ │ ▼ │ │
//                     │ Jump │             └►F◄┘             └─┴►F◄┴─┘
//                     └─────┬┘               │                   │
//                  {        │  G──────┐      │                   │
//     Control flow {        │  │ Jump │      │ G                 │ G
//     hole after F {        │  └─┬────┘      │ │                 │ │
//                  {        ▼    ▼           │ │                 │ │
//                          H──────┐          ▼ │                 ▼ │
//                          │Return│          H◄┘                 H◄┘
//                          └──────┘
//
// Since we only care about forward control, loop jumps are treated the same as
// returns -- they terminate the post-dominating hole chain.
//
void StraightForwardRegisterAllocator::ComputePostDominatingHoles(
    Graph* graph) {
  // For all blocks, find the list of jumps that jump over code unreachable from
  // the block. Such a list of jumps terminates in return or jumploop.
  for (BasicBlock* block : base::Reversed(*graph)) {
    ControlNode* control = block->control_node();
    if (auto node = control->TryCast<Jump>()) {
      // If the current control node is a jump, prepend it to the list of jumps
      // at the target.
      control->set_next_post_dominating_hole(
          NearestPostDominatingHole(node->target()->control_node()));
    } else if (auto node = control->TryCast<ConditionalControlNode>()) {
      ControlNode* first =
          NearestPostDominatingHole(node->if_true()->control_node());
      ControlNode* second =
          NearestPostDominatingHole(node->if_false()->control_node());

      // Either find the merge-point of both branches, or the highest reachable
      // control-node of the longest branch after the last node of the shortest
      // branch.

      // As long as there's no merge-point.
      while (first != second) {
        // Walk the highest branch to find where it goes.
        if (first->id() > second->id()) std::swap(first, second);

        // If the first branch returns or jumps back, we've found highest
        // reachable control-node of the longest branch (the second control
        // node).
        if (first->Is<Return>() || first->Is<Deopt>() ||
            first->Is<JumpLoop>()) {
          control->set_next_post_dominating_hole(second);
          break;
        }

        // Continue one step along the highest branch. This may cross over the
        // lowest branch in case it returns or loops. If labelled blocks are
        // involved such swapping of which branch is the highest branch can
        // occur multiple times until a return/jumploop/merge is discovered.
        first = first->next_post_dominating_hole();
      }

      // Once the branches merged, we've found the gap-chain that's relevant for
      // the control node.
      control->set_next_post_dominating_hole(first);
    }
  }
}

void StraightForwardRegisterAllocator::PrintLiveRegs() const {
  bool first = true;
  for (Register reg : used_registers()) {
    ValueNode* node = GetRegisterValue(reg);
    if (first) {
      first = false;
    } else {
      printing_visitor_->os() << ", ";
    }
    printing_visitor_->os() << reg << "=v" << node->id();
  }
}

void StraightForwardRegisterAllocator::AllocateRegisters(Graph* graph) {
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_.reset(new MaglevPrintingVisitor(std::cout));
    printing_visitor_->PreProcessGraph(compilation_unit_, graph);
  }

  for (block_it_ = graph->begin(); block_it_ != graph->end(); ++block_it_) {
    BasicBlock* block = *block_it_;

    // Restore mergepoint state.
    if (block->has_state()) {
      InitializeRegisterValues(block->state()->register_state());
    }

    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->PreProcessBasicBlock(compilation_unit_, block);
      printing_visitor_->os() << "live regs: ";
      PrintLiveRegs();

      ControlNode* control = NearestPostDominatingHole(block->control_node());
      if (!control->Is<JumpLoop>()) {
        printing_visitor_->os() << "\n[holes:";
        while (true) {
          if (control->Is<Jump>()) {
            BasicBlock* target = control->Cast<Jump>()->target();
            printing_visitor_->os()
                << " " << control->id() << "-" << target->first_id();
            control = control->next_post_dominating_hole();
            DCHECK_NOT_NULL(control);
            continue;
          } else if (control->Is<Return>()) {
            printing_visitor_->os() << " " << control->id() << ".";
            break;
          } else if (control->Is<Deopt>()) {
            printing_visitor_->os() << " " << control->id() << "✖️";
            break;
          } else if (control->Is<JumpLoop>()) {
            printing_visitor_->os() << " " << control->id() << "↰";
            break;
          }
          UNREACHABLE();
        }
        printing_visitor_->os() << "]";
      }
      printing_visitor_->os() << std::endl;
    }

    // Activate phis.
    if (block->has_phi()) {
      // Firstly, make the phi live, and try to assign it to an input
      // location.
      for (Phi* phi : *block->phis()) {
        phi->SetNoSpillOrHint();
        TryAllocateToInput(phi);
      }
      // Secondly try to assign the phi to a free register.
      for (Phi* phi : *block->phis()) {
        if (phi->result().operand().IsAllocated()) continue;
        compiler::InstructionOperand allocation = TryAllocateRegister(phi);
        if (allocation.IsAllocated()) {
          phi->result().SetAllocated(
              compiler::AllocatedOperand::cast(allocation));
          if (FLAG_trace_maglev_regalloc) {
            printing_visitor_->Process(
                phi, ProcessingState(compilation_unit_, block_it_));
            printing_visitor_->os()
                << "phi (new reg) " << phi->result().operand() << std::endl;
          }
        }
      }
      // Finally just use a stack slot.
      for (Phi* phi : *block->phis()) {
        if (phi->result().operand().IsAllocated()) continue;
        AllocateSpillSlot(phi);
        // TODO(verwaest): Will this be used at all?
        phi->result().SetAllocated(phi->spill_slot());
        if (FLAG_trace_maglev_regalloc) {
          printing_visitor_->Process(
              phi, ProcessingState(compilation_unit_, block_it_));
          printing_visitor_->os()
              << "phi (stack) " << phi->result().operand() << std::endl;
        }
      }

      if (FLAG_trace_maglev_regalloc) {
        printing_visitor_->os() << "live regs: ";
        PrintLiveRegs();
        printing_visitor_->os() << std::endl;
      }
    }

    node_it_ = block->nodes().begin();
    for (; node_it_ != block->nodes().end(); ++node_it_) {
      AllocateNode(*node_it_);
    }
    AllocateControlNode(block->control_node(), block);
  }
}

void StraightForwardRegisterAllocator::UpdateUse(
    ValueNode* node, InputLocation* input_location) {
  DCHECK(!node->is_dead());

  // Update the next use.
  node->set_next_use(input_location->next_use_id());

  if (!node->is_dead()) return;

  // If a value is dead, make sure it's cleared.
  FreeRegisters(node);
}

void StraightForwardRegisterAllocator::UpdateUse(
    const EagerDeoptInfo& deopt_info) {
  const CompactInterpreterFrameState* checkpoint_state =
      deopt_info.state.register_frame;
  int index = 0;
  checkpoint_state->ForEachValue(
      *compilation_unit_, [&](ValueNode* node, interpreter::Register reg) {
        InputLocation* input = &deopt_info.input_locations[index++];
        input->InjectAllocated(node->allocation());
        UpdateUse(node, input);
      });
}

void StraightForwardRegisterAllocator::UpdateUse(
    const LazyDeoptInfo& deopt_info) {
  const CompactInterpreterFrameState* checkpoint_state =
      deopt_info.state.register_frame;
  int index = 0;
  checkpoint_state->ForEachValue(
      *compilation_unit_, [&](ValueNode* node, interpreter::Register reg) {
        // Skip over the result location.
        if (reg == deopt_info.result_location) return;
        InputLocation* input = &deopt_info.input_locations[index++];
        input->InjectAllocated(node->allocation());
        UpdateUse(node, input);
      });
}

void StraightForwardRegisterAllocator::AllocateNode(Node* node) {
  for (Input& input : *node) AssignInput(input);
  AssignTemporaries(node);
  if (node->properties().can_eager_deopt()) {
    UpdateUse(*node->eager_deopt_info());
  }
  for (Input& input : *node) UpdateUse(&input);

  if (node->properties().is_call()) SpillAndClearRegisters();

  // Allocate node output.
  if (node->Is<ValueNode>()) AllocateNodeResult(node->Cast<ValueNode>());

  // Lazy deopts are semantically after the node, so update them last.
  if (node->properties().can_lazy_deopt()) {
    UpdateUse(*node->lazy_deopt_info());
  }

  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->Process(node,
                               ProcessingState(compilation_unit_, block_it_));
    printing_visitor_->os() << "live regs: ";
    PrintLiveRegs();
    printing_visitor_->os() << "\n";
  }
}

void StraightForwardRegisterAllocator::AllocateNodeResult(ValueNode* node) {
  DCHECK(!node->Is<Phi>());

  node->SetNoSpillOrHint();

  compiler::UnallocatedOperand operand =
      compiler::UnallocatedOperand::cast(node->result().operand());

  if (operand.basic_policy() == compiler::UnallocatedOperand::FIXED_SLOT) {
    DCHECK(node->Is<InitialValue>());
    DCHECK_LT(operand.fixed_slot_index(), 0);
    // Set the stack slot to exactly where the value is.
    compiler::AllocatedOperand location(compiler::AllocatedOperand::STACK_SLOT,
                                        MachineRepresentation::kTagged,
                                        operand.fixed_slot_index());
    node->result().SetAllocated(location);
    node->Spill(location);
    return;
  }

  switch (operand.extended_policy()) {
    case compiler::UnallocatedOperand::FIXED_REGISTER: {
      Register r = Register::from_code(operand.fixed_register_index());
      node->result().SetAllocated(ForceAllocate(r, node));
      break;
    }

    case compiler::UnallocatedOperand::MUST_HAVE_REGISTER:
      node->result().SetAllocated(AllocateRegister(node));
      break;

    case compiler::UnallocatedOperand::SAME_AS_INPUT: {
      Input& input = node->input(operand.input_index());
      Register r = input.AssignedRegister();
      node->result().SetAllocated(ForceAllocate(r, node));
      break;
    }

    case compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT:
    case compiler::UnallocatedOperand::NONE:
    case compiler::UnallocatedOperand::FIXED_FP_REGISTER:
    case compiler::UnallocatedOperand::MUST_HAVE_SLOT:
    case compiler::UnallocatedOperand::REGISTER_OR_SLOT:
      UNREACHABLE();
  }

  // Immediately kill the register use if the node doesn't have a valid
  // live-range.
  // TODO(verwaest): Remove once we can avoid allocating such registers.
  if (!node->has_valid_live_range() &&
      node->result().operand().IsAnyRegister()) {
    DCHECK(node->has_register());
    FreeRegisters(node);
    DCHECK(!node->has_register());
    DCHECK(node->is_dead());
  }
}

void StraightForwardRegisterAllocator::DropRegisterValue(Register reg) {
  // The register should not already be free.
  DCHECK(!free_registers_.has(reg));

  ValueNode* node = GetRegisterValue(reg);

  // Remove the register from the node's list.
  node->RemoveRegister(reg);

  // Return if the removed value already has another register or is spilled.
  if (node->has_register() || node->is_spilled()) return;

  // Try to move the value to another register.
  if (free_registers_ != kEmptyRegList) {
    Register target_reg = free_registers_.PopFirst();
    SetRegister(target_reg, node);
    // Emit a gapmove.
    compiler::AllocatedOperand source(compiler::LocationOperand::REGISTER,
                                      MachineRepresentation::kTagged,
                                      reg.code());
    compiler::AllocatedOperand target(compiler::LocationOperand::REGISTER,
                                      MachineRepresentation::kTagged,
                                      target_reg.code());

    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->os()
          << "gap move: " << PrintNodeLabel(graph_labeller(), node) << ": "
          << target << " ← " << source << std::endl;
    }
    AddMoveBeforeCurrentNode(source, target);
    return;
  }

  // If all else fails, spill the value.
  Spill(node);
}

void StraightForwardRegisterAllocator::InitializeConditionalBranchRegisters(
    ConditionalControlNode* control_node, BasicBlock* target) {
  if (target->is_empty_block()) {
    // Jumping over an empty block, so we're in fact merging.
    Jump* jump = target->control_node()->Cast<Jump>();
    target = jump->target();
    return MergeRegisterValues(control_node, target, jump->predecessor_id());
  }
  if (target->has_state()) {
    // Not a fall-through branch, copy the state over.
    return InitializeBranchTargetRegisterValues(control_node, target);
  }
  // Clear dead fall-through registers.
  DCHECK_EQ(control_node->id() + 1, target->first_id());
  RegList registers = used_registers();
  while (registers != kEmptyRegList) {
    Register reg = registers.PopFirst();
    ValueNode* node = GetRegisterValue(reg);
    if (!IsLiveAtTarget(node, control_node, target)) {
      FreeRegisters(node);
      // Update the registers we're visiting to avoid revisiting this node.
      registers.clear(free_registers_);
    }
  }
}

void StraightForwardRegisterAllocator::AllocateControlNode(ControlNode* node,
                                                           BasicBlock* block) {
  for (Input& input : *node) AssignInput(input);
  AssignTemporaries(node);
  if (node->properties().can_eager_deopt()) {
    UpdateUse(*node->eager_deopt_info());
  }
  for (Input& input : *node) UpdateUse(&input);

  if (node->properties().is_call()) SpillAndClearRegisters();

  // Inject allocation into target phis.
  if (auto unconditional = node->TryCast<UnconditionalControlNode>()) {
    BasicBlock* target = unconditional->target();
    if (target->has_phi()) {
      Phi::List* phis = target->phis();
      for (Phi* phi : *phis) {
        Input& input = phi->input(block->predecessor_id());
        input.InjectAllocated(input.node()->allocation());
      }
      for (Phi* phi : *phis) UpdateUse(&phi->input(block->predecessor_id()));
    }
  }

  // TODO(verwaest): This isn't a good idea :)
  if (node->properties().can_eager_deopt()) SpillRegisters();

  // Merge register values. Values only flowing into phis and not being
  // independently live will be killed as part of the merge.
  if (auto unconditional = node->TryCast<UnconditionalControlNode>()) {
    // Empty blocks are immediately merged at the control of their predecessor.
    if (!block->is_empty_block()) {
      MergeRegisterValues(unconditional, unconditional->target(),
                          block->predecessor_id());
    }
  } else if (auto conditional = node->TryCast<ConditionalControlNode>()) {
    InitializeConditionalBranchRegisters(conditional, conditional->if_true());
    InitializeConditionalBranchRegisters(conditional, conditional->if_false());
  }

  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->Process(node,
                               ProcessingState(compilation_unit_, block_it_));
  }
}

void StraightForwardRegisterAllocator::TryAllocateToInput(Phi* phi) {
  // Try allocate phis to a register used by any of the inputs.
  for (Input& input : *phi) {
    if (input.operand().IsRegister()) {
      Register reg = input.AssignedRegister();
      if (free_registers_.has(reg)) {
        phi->result().SetAllocated(ForceAllocate(reg, phi));
        if (FLAG_trace_maglev_regalloc) {
          printing_visitor_->Process(
              phi, ProcessingState(compilation_unit_, block_it_));
          printing_visitor_->os()
              << "phi (reuse) " << input.operand() << std::endl;
        }
        return;
      }
    }
  }
}

void StraightForwardRegisterAllocator::AddMoveBeforeCurrentNode(
    compiler::AllocatedOperand source, compiler::AllocatedOperand target) {
  GapMove* gap_move =
      Node::New<GapMove>(compilation_unit_->zone(), {}, source, target);
  if (compilation_unit_->has_graph_labeller()) {
    graph_labeller()->RegisterNode(gap_move);
  }
  if (*node_it_ == nullptr) {
    // We're at the control node, so append instead.
    (*block_it_)->nodes().Add(gap_move);
    node_it_ = (*block_it_)->nodes().end();
  } else {
    DCHECK_NE(node_it_, (*block_it_)->nodes().end());
    node_it_.InsertBefore(gap_move);
  }
}

void StraightForwardRegisterAllocator::Spill(ValueNode* node) {
  if (node->is_spilled()) return;
  AllocateSpillSlot(node);
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "spill: " << node->spill_slot() << " ← "
        << PrintNodeLabel(graph_labeller(), node) << std::endl;
  }
}

void StraightForwardRegisterAllocator::AssignInput(Input& input) {
  compiler::UnallocatedOperand operand =
      compiler::UnallocatedOperand::cast(input.operand());
  ValueNode* node = input.node();
  compiler::AllocatedOperand location = node->allocation();

  switch (operand.extended_policy()) {
    case compiler::UnallocatedOperand::REGISTER_OR_SLOT:
    case compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT:
      input.SetAllocated(location);
      break;

    case compiler::UnallocatedOperand::FIXED_REGISTER: {
      Register reg = Register::from_code(operand.fixed_register_index());
      input.SetAllocated(ForceAllocate(reg, node));
      break;
    }

    case compiler::UnallocatedOperand::MUST_HAVE_REGISTER:
      if (location.IsAnyRegister()) {
        input.SetAllocated(location);
      } else {
        input.SetAllocated(AllocateRegister(node));
      }
      break;

    case compiler::UnallocatedOperand::FIXED_FP_REGISTER:
    case compiler::UnallocatedOperand::SAME_AS_INPUT:
    case compiler::UnallocatedOperand::NONE:
    case compiler::UnallocatedOperand::MUST_HAVE_SLOT:
      UNREACHABLE();
  }

  compiler::AllocatedOperand allocated =
      compiler::AllocatedOperand::cast(input.operand());
  if (location != allocated) {
    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->os()
          << "gap move: " << allocated << " ← " << location << std::endl;
    }
    AddMoveBeforeCurrentNode(location, allocated);
  }
}

void StraightForwardRegisterAllocator::SpillRegisters() {
  for (Register reg : used_registers()) {
    ValueNode* node = GetRegisterValue(reg);
    Spill(node);
  }
}

void StraightForwardRegisterAllocator::SpillAndClearRegisters() {
  while (used_registers() != kEmptyRegList) {
    Register reg = used_registers().first();
    ValueNode* node = GetRegisterValue(reg);
    Spill(node);
    FreeRegisters(node);
    DCHECK(!used_registers().has(reg));
  }
}

void StraightForwardRegisterAllocator::AllocateSpillSlot(ValueNode* node) {
  DCHECK(!node->is_spilled());
  uint32_t free_slot = top_of_stack_++;
  compilation_unit_->push_stack_value_repr(node->value_representation());
  node->Spill(compiler::AllocatedOperand(compiler::AllocatedOperand::STACK_SLOT,
                                         MachineRepresentation::kTagged,
                                         free_slot));
}

void StraightForwardRegisterAllocator::FreeSomeRegister() {
  int furthest_use = 0;
  Register best = Register::no_reg();
  for (Register reg : used_registers()) {
    ValueNode* value = GetRegisterValue(reg);
    // The cheapest register to clear is a register containing a value that's
    // contained in another register as well.
    if (value->num_registers() > 1) {
      best = reg;
      break;
    }
    int use = value->next_use();
    if (use > furthest_use) {
      furthest_use = use;
      best = reg;
    }
  }
  DCHECK(best.is_valid());
  DropRegisterValue(best);
  FreeRegister(best);
}

compiler::AllocatedOperand StraightForwardRegisterAllocator::AllocateRegister(
    ValueNode* node) {
  if (free_registers_ == kEmptyRegList) FreeSomeRegister();
  compiler::InstructionOperand allocation = TryAllocateRegister(node);
  DCHECK(allocation.IsAllocated());
  return compiler::AllocatedOperand::cast(allocation);
}

compiler::AllocatedOperand StraightForwardRegisterAllocator::ForceAllocate(
    Register reg, ValueNode* node) {
  if (free_registers_.has(reg)) {
    // If it's already free, remove it from the free list.
    free_registers_.clear(reg);
  } else if (GetRegisterValue(reg) == node) {
    return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                      MachineRepresentation::kTagged,
                                      reg.code());
  } else {
    DropRegisterValue(reg);
  }
#ifdef DEBUG
  DCHECK(!free_registers_.has(reg));
#endif
  SetRegister(reg, node);
  return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                    MachineRepresentation::kTagged, reg.code());
}

void StraightForwardRegisterAllocator::SetRegister(Register reg,
                                                   ValueNode* node) {
  DCHECK(!free_registers_.has(reg));
  register_values_[reg.code()] = node;
  node->AddRegister(reg);
}

compiler::InstructionOperand
StraightForwardRegisterAllocator::TryAllocateRegister(ValueNode* node) {
  if (free_registers_ == kEmptyRegList) return compiler::InstructionOperand();
  Register reg = free_registers_.PopFirst();

  // Allocation succeeded. This might have found an existing allocation.
  // Simply update the state anyway.
  SetRegister(reg, node);
  return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                    MachineRepresentation::kTagged, reg.code());
}

void StraightForwardRegisterAllocator::AssignTemporaries(NodeBase* node) {
  int num_temporaries_needed = node->num_temporaries_needed();
  int num_free_registers = free_registers_.Count();

  // Free extra registers if necessary.
  for (int i = num_free_registers; i < num_temporaries_needed; ++i) {
    FreeSomeRegister();
  }

  DCHECK_GE(free_registers_.Count(), num_temporaries_needed);
  node->assign_temporaries(free_registers_);
}

void StraightForwardRegisterAllocator::InitializeRegisterValues(
    MergePointRegisterState& target_state) {
  // First clear the register state.
  while (used_registers() != kEmptyRegList) {
    Register reg = used_registers().first();
    ValueNode* node = GetRegisterValue(reg);
    FreeRegisters(node);
    DCHECK(!used_registers().has(reg));
  }

  // All registers should be free by now.
  DCHECK_EQ(free_registers_, kAllocatableGeneralRegisters);

  // Then fill it in with target information.
  for (auto entry : target_state) {
    Register reg = entry.reg;

    ValueNode* node;
    RegisterMerge* merge;
    LoadMergeState(entry.state, &node, &merge);
    if (node != nullptr) {
      free_registers_.clear(reg);
      SetRegister(reg, node);
    } else {
      DCHECK(!entry.state.GetPayload().is_merge);
    }
  }
}

void StraightForwardRegisterAllocator::EnsureInRegister(
    MergePointRegisterState& target_state, ValueNode* incoming) {
#ifdef DEBUG
  bool found = false;
  for (auto entry : target_state) {
    ValueNode* node;
    RegisterMerge* merge;
    LoadMergeState(entry.state, &node, &merge);
    if (node == incoming) found = true;
  }
  DCHECK(found);
#endif
}

void StraightForwardRegisterAllocator::InitializeBranchTargetRegisterValues(
    ControlNode* source, BasicBlock* target) {
  MergePointRegisterState& target_state = target->state()->register_state();
  DCHECK(!target_state.is_initialized());
  for (auto entry : target_state) {
    Register reg = entry.reg;
    ValueNode* node = nullptr;
    if (!free_registers_.has(reg)) {
      node = GetRegisterValue(reg);
      if (!IsLiveAtTarget(node, source, target)) node = nullptr;
    }
    entry.state = {node, initialized_node};
  }
}

void StraightForwardRegisterAllocator::MergeRegisterValues(ControlNode* control,
                                                           BasicBlock* target,
                                                           int predecessor_id) {
  MergePointRegisterState& target_state = target->state()->register_state();
  if (!target_state.is_initialized()) {
    // This is the first block we're merging, initialize the values.
    return InitializeBranchTargetRegisterValues(control, target);
  }

  int predecessor_count = target->state()->predecessor_count();
  for (auto entry : target_state) {
    Register reg = entry.reg;

    ValueNode* node;
    RegisterMerge* merge;
    LoadMergeState(entry.state, &node, &merge);

    compiler::AllocatedOperand register_info = {
        compiler::LocationOperand::REGISTER, MachineRepresentation::kTagged,
        reg.code()};

    ValueNode* incoming = nullptr;
    if (!free_registers_.has(reg)) {
      incoming = GetRegisterValue(reg);
      if (!IsLiveAtTarget(incoming, control, target)) {
        incoming = nullptr;
      }
    }

    if (incoming == node) {
      // We're using the same register as the target already has. If registers
      // are merged, add input information.
      if (merge) merge->operand(predecessor_id) = register_info;
      continue;
    }

    if (merge) {
      // The register is already occupied with a different node. Figure out
      // where that node is allocated on the incoming branch.
      merge->operand(predecessor_id) = node->allocation();

      // If there's a value in the incoming state, that value is either
      // already spilled or in another place in the merge state.
      if (incoming != nullptr && incoming->is_spilled()) {
        EnsureInRegister(target_state, incoming);
      }
      continue;
    }

    DCHECK_IMPLIES(node == nullptr, incoming != nullptr);
    if (node == nullptr && !incoming->is_spilled()) {
      // If the register is unallocated at the merge point, and the incoming
      // value isn't spilled, that means we must have seen it already in a
      // different register.
      EnsureInRegister(target_state, incoming);
      continue;
    }

    const size_t size = sizeof(RegisterMerge) +
                        predecessor_count * sizeof(compiler::AllocatedOperand);
    void* buffer = compilation_unit_->zone()->Allocate<void*>(size);
    merge = new (buffer) RegisterMerge();
    merge->node = node == nullptr ? incoming : node;

    // If the register is unallocated at the merge point, allocation so far
    // is the spill slot for the incoming value. Otherwise all incoming
    // branches agree that the current node is in the register info.
    compiler::AllocatedOperand info_so_far =
        node == nullptr
            ? compiler::AllocatedOperand::cast(incoming->spill_slot())
            : register_info;

    // Initialize the entire array with info_so_far since we don't know in
    // which order we've seen the predecessors so far. Predecessors we
    // haven't seen yet will simply overwrite their entry later.
    for (int i = 0; i < predecessor_count; i++) {
      merge->operand(i) = info_so_far;
    }
    // If the register is unallocated at the merge point, fill in the
    // incoming value. Otherwise find the merge-point node in the incoming
    // state.
    if (node == nullptr) {
      merge->operand(predecessor_id) = register_info;
    } else {
      merge->operand(predecessor_id) = node->allocation();
    }
    entry.state = {merge, initialized_merge};
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

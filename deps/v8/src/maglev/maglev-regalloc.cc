// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-regalloc.h"

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/compiler/backend/instruction.h"
#include "src/maglev/maglev-compilation-info.h"
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

template <typename RegisterT>
void ClearDeadFallthroughRegisters(RegisterFrameState<RegisterT> registers,
                                   ConditionalControlNode* control_node,
                                   BasicBlock* target) {
  RegListBase<RegisterT> list = registers.used();
  while (list != registers.empty()) {
    RegisterT reg = list.PopFirst();
    ValueNode* node = registers.GetValue(reg);
    if (!IsLiveAtTarget(node, control_node, target)) {
      registers.FreeRegistersUsedBy(node);
      // Update the registers we're visiting to avoid revisiting this node.
      list.clear(registers.free());
    }
  }
}
}  // namespace

StraightForwardRegisterAllocator::StraightForwardRegisterAllocator(
    MaglevCompilationInfo* compilation_info, Graph* graph)
    : compilation_info_(compilation_info) {
  ComputePostDominatingHoles(graph);
  AllocateRegisters(graph);
  graph->set_tagged_stack_slots(tagged_.top);
  graph->set_untagged_stack_slots(untagged_.top);
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
  auto print = [&](auto reg, ValueNode* node) {
    if (first) {
      first = false;
    } else {
      printing_visitor_->os() << ", ";
    }
    printing_visitor_->os() << reg << "=v" << node->id();
  };
  general_registers_.ForEachUsedRegister(print);
  double_registers_.ForEachUsedRegister(print);
}

void StraightForwardRegisterAllocator::AllocateRegisters(Graph* graph) {
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_.reset(new MaglevPrintingVisitor(std::cout));
    printing_visitor_->PreProcessGraph(compilation_info_, graph);
  }

  for (block_it_ = graph->begin(); block_it_ != graph->end(); ++block_it_) {
    BasicBlock* block = *block_it_;

    // Restore mergepoint state.
    if (block->has_state()) {
      InitializeRegisterValues(block->state()->register_state());
    }

    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->PreProcessBasicBlock(compilation_info_, block);
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
        // We assume that Phis are always untagged, and so are always allocated
        // in a general register.
        compiler::InstructionOperand allocation =
            general_registers_.TryAllocateRegister(phi);
        if (allocation.IsAllocated()) {
          phi->result().SetAllocated(
              compiler::AllocatedOperand::cast(allocation));
          if (FLAG_trace_maglev_regalloc) {
            printing_visitor_->Process(
                phi, ProcessingState(compilation_info_, block_it_));
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
              phi, ProcessingState(compilation_info_, block_it_));
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

void StraightForwardRegisterAllocator::FreeRegistersUsedBy(ValueNode* node) {
  if (node->use_double_register()) {
    double_registers_.FreeRegistersUsedBy(node);
  } else {
    general_registers_.FreeRegistersUsedBy(node);
  }
}

void StraightForwardRegisterAllocator::UpdateUse(
    ValueNode* node, InputLocation* input_location) {
  DCHECK(!node->is_dead());

  // Update the next use.
  node->set_next_use(input_location->next_use_id());

  if (!node->is_dead()) return;

  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "freeing " << PrintNodeLabel(graph_labeller(), node) << "\n";
  }

  // If a value is dead, make sure it's cleared.
  FreeRegistersUsedBy(node);

  // If the stack slot is a local slot, free it so it can be reused.
  if (node->is_spilled()) {
    compiler::AllocatedOperand slot = node->spill_slot();
    if (slot.index() > 0) {
      SpillSlots& slots =
          slot.representation() == MachineRepresentation::kTagged ? tagged_
                                                                  : untagged_;
      DCHECK_IMPLIES(
          slots.free_slots.size() > 0,
          slots.free_slots.back().freed_at_position <= node->live_range().end);
      slots.free_slots.emplace_back(slot.index(), node->live_range().end);
    }
  }
}

void StraightForwardRegisterAllocator::UpdateUse(
    const EagerDeoptInfo& deopt_info) {
  int index = 0;
  UpdateUse(deopt_info.unit, &deopt_info.state, deopt_info.input_locations,
            index);
}

void StraightForwardRegisterAllocator::UpdateUse(
    const LazyDeoptInfo& deopt_info) {
  const CompactInterpreterFrameState* checkpoint_state =
      deopt_info.state.register_frame;
  int index = 0;
  checkpoint_state->ForEachValue(
      deopt_info.unit, [&](ValueNode* node, interpreter::Register reg) {
        // Skip over the result location.
        if (reg == deopt_info.result_location) return;
        InputLocation* input = &deopt_info.input_locations[index++];
        input->InjectAllocated(node->allocation());
        UpdateUse(node, input);
      });
}

void StraightForwardRegisterAllocator::UpdateUse(
    const MaglevCompilationUnit& unit,
    const CheckpointedInterpreterState* state, InputLocation* input_locations,
    int& index) {
  if (state->parent) {
    UpdateUse(*unit.caller(), state->parent, input_locations, index);
  }
  const CompactInterpreterFrameState* checkpoint_state = state->register_frame;
  checkpoint_state->ForEachValue(
      unit, [&](ValueNode* node, interpreter::Register reg) {
        InputLocation* input = &input_locations[index++];
        input->InjectAllocated(node->allocation());
        UpdateUse(node, input);
      });
}

void StraightForwardRegisterAllocator::AllocateNode(Node* node) {
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "Allocating " << PrintNodeLabel(graph_labeller(), node)
        << " inputs...\n";
  }
  for (Input& input : *node) AssignInput(input);
  AssignTemporaries(node);

  if (node->properties().is_call()) SpillAndClearRegisters();

  // Allocate node output.
  if (node->Is<ValueNode>()) {
    AllocateNodeResult(node->Cast<ValueNode>());
  }

  // Update uses only after allocating the node result. This order is necessary
  // to avoid emitting input-clobbering gap moves during node result allocation
  // -- a separate mechanism using AllocationStage ensures that the node result
  // allocation is allowed to use the registers of nodes that are about to be
  // dead.
  if (node->properties().can_eager_deopt()) {
    UpdateUse(*node->eager_deopt_info());
  }
  for (Input& input : *node) UpdateUse(&input);

  // Lazy deopts are semantically after the node, so update them last.
  if (node->properties().can_lazy_deopt()) {
    UpdateUse(*node->lazy_deopt_info());
  }

  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->Process(node,
                               ProcessingState(compilation_info_, block_it_));
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
                                        node->GetMachineRepresentation(),
                                        operand.fixed_slot_index());
    node->result().SetAllocated(location);
    node->Spill(location);
    return;
  }

  switch (operand.extended_policy()) {
    case compiler::UnallocatedOperand::FIXED_REGISTER: {
      Register r = Register::from_code(operand.fixed_register_index());
      node->result().SetAllocated(
          ForceAllocate(r, node, AllocationStage::kAtEnd));
      break;
    }

    case compiler::UnallocatedOperand::MUST_HAVE_REGISTER:
      node->result().SetAllocated(
          AllocateRegister(node, AllocationStage::kAtEnd));
      break;

    case compiler::UnallocatedOperand::SAME_AS_INPUT: {
      Input& input = node->input(operand.input_index());
      node->result().SetAllocated(
          ForceAllocate(input, node, AllocationStage::kAtEnd));
      break;
    }

    case compiler::UnallocatedOperand::FIXED_FP_REGISTER: {
      DoubleRegister r =
          DoubleRegister::from_code(operand.fixed_register_index());
      node->result().SetAllocated(
          ForceAllocate(r, node, AllocationStage::kAtEnd));
      break;
    }

    case compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT:
    case compiler::UnallocatedOperand::NONE:
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
    FreeRegistersUsedBy(node);
    DCHECK(!node->has_register());
    DCHECK(node->is_dead());
  }
}

template <typename RegisterT>
void StraightForwardRegisterAllocator::DropRegisterValue(
    RegisterFrameState<RegisterT>& registers, RegisterT reg,
    AllocationStage stage) {
  // The register should not already be free.
  DCHECK(!registers.free().has(reg));

  ValueNode* node = registers.GetValue(reg);
  MachineRepresentation mach_repr = node->GetMachineRepresentation();

  // Remove the register from the node's list.
  node->RemoveRegister(reg);

  // Return if the removed value already has another register or is spilled.
  if (node->has_register() || node->is_spilled()) return;

  // If we are at the end of the current node, and the last use of the given
  // node is the current node, allow it to be dropped.
  if (stage == AllocationStage::kAtEnd &&
      node->live_range().end == node_it_->id())
    return;

  // Try to move the value to another register.
  if (!registers.FreeIsEmpty()) {
    RegisterT target_reg = registers.TakeFirstFree();
    registers.SetValue(target_reg, node);
    // Emit a gapmove.
    compiler::AllocatedOperand source(compiler::LocationOperand::REGISTER,
                                      mach_repr, reg.code());
    compiler::AllocatedOperand target(compiler::LocationOperand::REGISTER,
                                      mach_repr, target_reg.code());

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

void StraightForwardRegisterAllocator::DropRegisterValue(
    Register reg, AllocationStage stage) {
  DropRegisterValue<Register>(general_registers_, reg, stage);
}

void StraightForwardRegisterAllocator::DropRegisterValue(
    DoubleRegister reg, AllocationStage stage) {
  DropRegisterValue<DoubleRegister>(double_registers_, reg, stage);
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
  ClearDeadFallthroughRegisters<Register>(general_registers_, control_node,
                                          target);
  ClearDeadFallthroughRegisters<DoubleRegister>(double_registers_, control_node,
                                                target);
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
  if (node->Is<JumpToInlined>()) {
    // Do nothing.
    // TODO(leszeks): DCHECK any useful invariants here.
  } else if (auto unconditional = node->TryCast<UnconditionalControlNode>()) {
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
                               ProcessingState(compilation_info_, block_it_));
  }
}

void StraightForwardRegisterAllocator::TryAllocateToInput(Phi* phi) {
  // Try allocate phis to a register used by any of the inputs.
  for (Input& input : *phi) {
    if (input.operand().IsRegister()) {
      // We assume Phi nodes only point to tagged values, and so they use a
      // general register.
      Register reg = input.AssignedGeneralRegister();
      if (general_registers_.free().has(reg)) {
        phi->result().SetAllocated(
            ForceAllocate(reg, phi, AllocationStage::kAtStart));
        if (FLAG_trace_maglev_regalloc) {
          printing_visitor_->Process(
              phi, ProcessingState(compilation_info_, block_it_));
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
      Node::New<GapMove>(compilation_info_->zone(), {}, source, target);
  if (compilation_info_->has_graph_labeller()) {
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
      input.SetAllocated(ForceAllocate(reg, node, AllocationStage::kAtStart));
      break;
    }

    case compiler::UnallocatedOperand::MUST_HAVE_REGISTER:
      if (location.IsAnyRegister()) {
        input.SetAllocated(location);
      } else {
        input.SetAllocated(AllocateRegister(node, AllocationStage::kAtStart));
      }
      break;

    case compiler::UnallocatedOperand::FIXED_FP_REGISTER: {
      DoubleRegister reg =
          DoubleRegister::from_code(operand.fixed_register_index());
      input.SetAllocated(ForceAllocate(reg, node, AllocationStage::kAtStart));
      break;
    }

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
  auto spill = [&](auto reg, ValueNode* node) { Spill(node); };
  general_registers_.ForEachUsedRegister(spill);
  double_registers_.ForEachUsedRegister(spill);
}

template <typename RegisterT>
void StraightForwardRegisterAllocator::SpillAndClearRegisters(
    RegisterFrameState<RegisterT>& registers) {
  while (registers.used() != registers.empty()) {
    RegisterT reg = registers.used().first();
    ValueNode* node = registers.GetValue(reg);
    Spill(node);
    registers.FreeRegistersUsedBy(node);
    DCHECK(!registers.used().has(reg));
  }
}

void StraightForwardRegisterAllocator::SpillAndClearRegisters() {
  SpillAndClearRegisters(general_registers_);
  SpillAndClearRegisters(double_registers_);
}

void StraightForwardRegisterAllocator::AllocateSpillSlot(ValueNode* node) {
  DCHECK(!node->is_spilled());
  uint32_t free_slot;
  bool is_tagged = (node->properties().value_representation() ==
                    ValueRepresentation::kTagged);
  // TODO(v8:7700): We will need a new class of SpillSlots for doubles in 32-bit
  // architectures.
  SpillSlots& slots = is_tagged ? tagged_ : untagged_;
  MachineRepresentation representation = node->GetMachineRepresentation();
  if (slots.free_slots.empty()) {
    free_slot = slots.top++;
  } else {
    NodeIdT start = node->live_range().start;
    auto it =
        std::upper_bound(slots.free_slots.begin(), slots.free_slots.end(),
                         start, [](NodeIdT s, const SpillSlotInfo& slot_info) {
                           return slot_info.freed_at_position < s;
                         });
    if (it != slots.free_slots.end()) {
      free_slot = it->slot_index;
      slots.free_slots.erase(it);
    } else {
      free_slot = slots.top++;
    }
  }
  node->Spill(compiler::AllocatedOperand(compiler::AllocatedOperand::STACK_SLOT,
                                         representation, free_slot));
}

template <typename RegisterT>
void StraightForwardRegisterAllocator::FreeSomeRegister(
    RegisterFrameState<RegisterT>& registers, AllocationStage stage) {
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os() << "need to free a register... ";
  }
  int furthest_use = 0;
  RegisterT best = RegisterT::no_reg();
  for (RegisterT reg : registers.used()) {
    ValueNode* value = registers.GetValue(reg);
    // If we're freeing at the end of allocation, and the given register's value
    // will already be dead after being used as an input to this node, allow
    // and indeed prefer using this register.
    if (stage == AllocationStage::kAtEnd &&
        value->live_range().end == node_it_->id()) {
      best = reg;
      break;
    }
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
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "chose " << best << " with next use " << furthest_use << "\n";
  }
  DCHECK(best.is_valid());
  DropRegisterValue(registers, best, stage);
  registers.AddToFree(best);
}

void StraightForwardRegisterAllocator::FreeSomeGeneralRegister(
    AllocationStage stage) {
  return FreeSomeRegister(general_registers_, stage);
}

void StraightForwardRegisterAllocator::FreeSomeDoubleRegister(
    AllocationStage stage) {
  return FreeSomeRegister(double_registers_, stage);
}
compiler::AllocatedOperand StraightForwardRegisterAllocator::AllocateRegister(
    ValueNode* node, AllocationStage stage) {
  compiler::InstructionOperand allocation;
  if (node->use_double_register()) {
    if (double_registers_.FreeIsEmpty()) FreeSomeDoubleRegister(stage);
    allocation = double_registers_.TryAllocateRegister(node);
  } else {
    if (general_registers_.FreeIsEmpty()) FreeSomeGeneralRegister(stage);
    allocation = general_registers_.TryAllocateRegister(node);
  }
  DCHECK(allocation.IsAllocated());
  return compiler::AllocatedOperand::cast(allocation);
}

template <typename RegisterT>
compiler::AllocatedOperand StraightForwardRegisterAllocator::ForceAllocate(
    RegisterFrameState<RegisterT>& registers, RegisterT reg, ValueNode* node,
    AllocationStage stage) {
  if (registers.free().has(reg)) {
    // If it's already free, remove it from the free list.
    registers.RemoveFromFree(reg);
  } else if (registers.GetValue(reg) == node) {
    return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                      node->GetMachineRepresentation(),
                                      reg.code());
  } else {
    DropRegisterValue(registers, reg, stage);
  }
#ifdef DEBUG
  DCHECK(!registers.free().has(reg));
#endif
  registers.SetValue(reg, node);
  return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                    node->GetMachineRepresentation(),
                                    reg.code());
}

compiler::AllocatedOperand StraightForwardRegisterAllocator::ForceAllocate(
    Register reg, ValueNode* node, AllocationStage stage) {
  DCHECK(!node->use_double_register());
  return ForceAllocate<Register>(general_registers_, reg, node,
                                 AllocationStage::kAtStart);
}

compiler::AllocatedOperand StraightForwardRegisterAllocator::ForceAllocate(
    DoubleRegister reg, ValueNode* node, AllocationStage stage) {
  DCHECK(node->use_double_register());
  return ForceAllocate<DoubleRegister>(double_registers_, reg, node, stage);
}

compiler::AllocatedOperand StraightForwardRegisterAllocator::ForceAllocate(
    const Input& input, ValueNode* node, AllocationStage stage) {
  if (input.IsDoubleRegister()) {
    return ForceAllocate(input.AssignedDoubleRegister(), node, stage);
  } else {
    return ForceAllocate(input.AssignedGeneralRegister(), node, stage);
  }
}

template <typename RegisterT>
compiler::InstructionOperand RegisterFrameState<RegisterT>::TryAllocateRegister(
    ValueNode* node) {
  if (free_ == kEmptyRegList) return compiler::InstructionOperand();
  RegisterT reg = free_.PopFirst();

  // Allocation succeeded. This might have found an existing allocation.
  // Simply update the state anyway.
  SetValue(reg, node);
  return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                    node->GetMachineRepresentation(),
                                    reg.code());
}

void StraightForwardRegisterAllocator::AssignTemporaries(NodeBase* node) {
  // TODO(victorgomes): Support double registers as temporaries.
  RegList initial_temporaries = node->temporaries();

  // Make sure that any initially set temporaries are definitely free.
  for (Register reg : initial_temporaries) {
    if (general_registers_.free().has(reg)) continue;
    DropRegisterValue(general_registers_, reg, AllocationStage::kAtStart);
    general_registers_.AddToFree(reg);
  }

  int num_temporaries_needed = node->num_temporaries_needed();
  int num_free_registers = general_registers_.free().Count();

  // Free extra registers if necessary.
  for (int i = num_free_registers; i < num_temporaries_needed; ++i) {
    FreeSomeGeneralRegister(AllocationStage::kAtStart);
  }

  DCHECK_GE(general_registers_.free().Count(), num_temporaries_needed);
  DCHECK_EQ(general_registers_.free() | initial_temporaries,
            general_registers_.free());
  node->assign_temporaries(general_registers_.free());
}

namespace {
template <typename RegisterT>
void ClearRegisterState(RegisterFrameState<RegisterT>& registers) {
  while (registers.used() != RegisterFrameState<RegisterT>::kEmptyRegList) {
    RegisterT reg = registers.used().first();
    ValueNode* node = registers.GetValue(reg);
    registers.FreeRegistersUsedBy(node);
    DCHECK(!registers.used().has(reg));
  }
}
}  // namespace

template <typename Function>
void StraightForwardRegisterAllocator::ForEachMergePointRegisterState(
    MergePointRegisterState& merge_point_state, Function&& f) {
  merge_point_state.ForEachGeneralRegister(
      [&](Register reg, RegisterState& state) {
        f(general_registers_, reg, state);
      });
  merge_point_state.ForEachDoubleRegister(
      [&](DoubleRegister reg, RegisterState& state) {
        f(double_registers_, reg, state);
      });
}

void StraightForwardRegisterAllocator::InitializeRegisterValues(
    MergePointRegisterState& target_state) {
  // First clear the register state.
  ClearRegisterState(general_registers_);
  ClearRegisterState(double_registers_);

  // All registers should be free by now.
  DCHECK_EQ(general_registers_.free(), kAllocatableGeneralRegisters);
  DCHECK_EQ(double_registers_.free(), kAllocatableDoubleRegisters);

  // Then fill it in with target information.
  auto fill = [&](auto& registers, auto reg, RegisterState& state) {
    ValueNode* node;
    RegisterMerge* merge;
    LoadMergeState(state, &node, &merge);
    if (node != nullptr) {
      registers.RemoveFromFree(reg);
      registers.SetValue(reg, node);
    } else {
      DCHECK(!state.GetPayload().is_merge);
    }
  };
  ForEachMergePointRegisterState(target_state, fill);
}

void StraightForwardRegisterAllocator::EnsureInRegister(
    MergePointRegisterState& target_state, ValueNode* incoming) {
#ifdef DEBUG
  bool found = false;
  auto find = [&found, &incoming](auto reg, RegisterState& state) {
    ValueNode* node;
    RegisterMerge* merge;
    LoadMergeState(state, &node, &merge);
    if (node == incoming) found = true;
  };
  if (incoming->use_double_register()) {
    target_state.ForEachDoubleRegister(find);
  } else {
    target_state.ForEachGeneralRegister(find);
  }
  DCHECK(found);
#endif
}

void StraightForwardRegisterAllocator::InitializeBranchTargetRegisterValues(
    ControlNode* source, BasicBlock* target) {
  MergePointRegisterState& target_state = target->state()->register_state();
  DCHECK(!target_state.is_initialized());
  auto init = [&](auto& registers, auto reg, RegisterState& state) {
    ValueNode* node = nullptr;
    if (!registers.free().has(reg)) {
      node = registers.GetValue(reg);
      if (!IsLiveAtTarget(node, source, target)) node = nullptr;
    }
    state = {node, initialized_node};
  };
  ForEachMergePointRegisterState(target_state, init);
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
  auto merge = [&](auto& registers, auto reg, RegisterState& state) {
    ValueNode* node;
    RegisterMerge* merge;
    LoadMergeState(state, &node, &merge);

    MachineRepresentation mach_repr = node == nullptr
                                          ? MachineRepresentation::kTagged
                                          : node->GetMachineRepresentation();
    compiler::AllocatedOperand register_info = {
        compiler::LocationOperand::REGISTER, mach_repr, reg.code()};

    ValueNode* incoming = nullptr;
    if (!registers.free().has(reg)) {
      incoming = registers.GetValue(reg);
      if (!IsLiveAtTarget(incoming, control, target)) {
        incoming = nullptr;
      }
    }

    if (incoming == node) {
      // We're using the same register as the target already has. If registers
      // are merged, add input information.
      if (merge) merge->operand(predecessor_id) = register_info;
      return;
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
      return;
    }

    DCHECK_IMPLIES(node == nullptr, incoming != nullptr);
    if (node == nullptr && !incoming->is_spilled()) {
      // If the register is unallocated at the merge point, and the incoming
      // value isn't spilled, that means we must have seen it already in a
      // different register.
      EnsureInRegister(target_state, incoming);
      return;
    }

    const size_t size = sizeof(RegisterMerge) +
                        predecessor_count * sizeof(compiler::AllocatedOperand);
    void* buffer = compilation_info_->zone()->Allocate<void*>(size);
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
    state = {merge, initialized_merge};
  };
  ForEachMergePointRegisterState(target_state, merge);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

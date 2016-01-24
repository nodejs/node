// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-scheduler.h"

#include "src/base/adapters.h"

namespace v8 {
namespace internal {
namespace compiler {

InstructionScheduler::ScheduleGraphNode::ScheduleGraphNode(
    Zone* zone,
    Instruction* instr)
    : instr_(instr),
      successors_(zone),
      unscheduled_predecessors_count_(0),
      latency_(GetInstructionLatency(instr)),
      total_latency_(-1),
      start_cycle_(-1) {
}


void InstructionScheduler::ScheduleGraphNode::AddSuccessor(
    ScheduleGraphNode* node) {
  successors_.push_back(node);
  node->unscheduled_predecessors_count_++;
}


InstructionScheduler::InstructionScheduler(Zone* zone,
                                           InstructionSequence* sequence)
    : zone_(zone),
      sequence_(sequence),
      graph_(zone),
      last_side_effect_instr_(nullptr),
      pending_loads_(zone),
      last_live_in_reg_marker_(nullptr) {
}


void InstructionScheduler::StartBlock(RpoNumber rpo) {
  DCHECK(graph_.empty());
  DCHECK(last_side_effect_instr_ == nullptr);
  DCHECK(pending_loads_.empty());
  DCHECK(last_live_in_reg_marker_ == nullptr);
  sequence()->StartBlock(rpo);
}


void InstructionScheduler::EndBlock(RpoNumber rpo) {
  ScheduleBlock();
  sequence()->EndBlock(rpo);
  graph_.clear();
  last_side_effect_instr_ = nullptr;
  pending_loads_.clear();
  last_live_in_reg_marker_ = nullptr;
}


void InstructionScheduler::AddInstruction(Instruction* instr) {
  ScheduleGraphNode* new_node = new (zone()) ScheduleGraphNode(zone(), instr);

  if (IsBlockTerminator(instr)) {
    // Make sure that basic block terminators are not moved by adding them
    // as successor of every instruction.
    for (auto node : graph_) {
      node->AddSuccessor(new_node);
    }
  } else if (IsFixedRegisterParameter(instr)) {
    if (last_live_in_reg_marker_ != nullptr) {
      last_live_in_reg_marker_->AddSuccessor(new_node);
    }
    last_live_in_reg_marker_ = new_node;
  } else {
    if (last_live_in_reg_marker_ != nullptr) {
      last_live_in_reg_marker_->AddSuccessor(new_node);
    }

    // Instructions with side effects and memory operations can't be
    // reordered with respect to each other.
    if (HasSideEffect(instr)) {
      if (last_side_effect_instr_ != nullptr) {
        last_side_effect_instr_->AddSuccessor(new_node);
      }
      for (auto load : pending_loads_) {
        load->AddSuccessor(new_node);
      }
      pending_loads_.clear();
      last_side_effect_instr_ = new_node;
    } else if (IsLoadOperation(instr)) {
      // Load operations can't be reordered with side effects instructions but
      // independent loads can be reordered with respect to each other.
      if (last_side_effect_instr_ != nullptr) {
        last_side_effect_instr_->AddSuccessor(new_node);
      }
      pending_loads_.push_back(new_node);
    }

    // Look for operand dependencies.
    for (auto node : graph_) {
      if (HasOperandDependency(node->instruction(), instr)) {
        node->AddSuccessor(new_node);
      }
    }
  }

  graph_.push_back(new_node);
}


bool InstructionScheduler::CompareNodes(ScheduleGraphNode *node1,
                                        ScheduleGraphNode *node2) const {
  return node1->total_latency() > node2->total_latency();
}


void InstructionScheduler::ScheduleBlock() {
  ZoneLinkedList<ScheduleGraphNode*> ready_list(zone());

  // Compute total latencies so that we can schedule the critical path first.
  ComputeTotalLatencies();

  // Add nodes which don't have dependencies to the ready list.
  for (auto node : graph_) {
    if (!node->HasUnscheduledPredecessor()) {
      ready_list.push_back(node);
    }
  }

  // Go through the ready list and schedule the instructions.
  int cycle = 0;
  while (!ready_list.empty()) {
    auto candidate = ready_list.end();
    for (auto iterator = ready_list.begin(); iterator != ready_list.end();
         ++iterator) {
      // Look for the best candidate to schedule.
      // We only consider instructions that have all their operands ready and
      // we try to schedule the critical path first (we look for the instruction
      // with the highest latency on the path to reach the end of the graph).
      if (cycle >= (*iterator)->start_cycle()) {
        if ((candidate == ready_list.end()) ||
            CompareNodes(*iterator, *candidate)) {
          candidate = iterator;
        }
      }
    }

    if (candidate != ready_list.end()) {
      sequence()->AddInstruction((*candidate)->instruction());

      for (auto successor : (*candidate)->successors()) {
        successor->DropUnscheduledPredecessor();
        successor->set_start_cycle(
            std::max(successor->start_cycle(),
                     cycle + (*candidate)->latency()));

        if (!successor->HasUnscheduledPredecessor()) {
          ready_list.push_back(successor);
        }
      }

      ready_list.erase(candidate);
    }

    cycle++;
  }
}


int InstructionScheduler::GetInstructionFlags(const Instruction* instr) const {
  switch (instr->arch_opcode()) {
    case kArchNop:
    case kArchStackPointer:
    case kArchFramePointer:
    case kArchTruncateDoubleToI:
      return kNoOpcodeFlags;

    case kArchPrepareCallCFunction:
    case kArchPrepareTailCall:
    case kArchCallCFunction:
    case kArchCallCodeObject:
    case kArchCallJSFunction:
    case kArchLazyBailout:
      return kHasSideEffect;

    case kArchTailCallCodeObject:
    case kArchTailCallJSFunction:
      return kHasSideEffect | kIsBlockTerminator;

    case kArchDeoptimize:
    case kArchJmp:
    case kArchLookupSwitch:
    case kArchTableSwitch:
    case kArchRet:
    case kArchThrowTerminator:
      return kIsBlockTerminator;

    case kCheckedLoadInt8:
    case kCheckedLoadUint8:
    case kCheckedLoadInt16:
    case kCheckedLoadUint16:
    case kCheckedLoadWord32:
    case kCheckedLoadWord64:
    case kCheckedLoadFloat32:
    case kCheckedLoadFloat64:
      return kIsLoadOperation;

    case kCheckedStoreWord8:
    case kCheckedStoreWord16:
    case kCheckedStoreWord32:
    case kCheckedStoreWord64:
    case kCheckedStoreFloat32:
    case kCheckedStoreFloat64:
    case kArchStoreWithWriteBarrier:
      return kHasSideEffect;

#define CASE(Name) case k##Name:
    TARGET_ARCH_OPCODE_LIST(CASE)
#undef CASE
      return GetTargetInstructionFlags(instr);
  }

  UNREACHABLE();
  return kNoOpcodeFlags;
}


bool InstructionScheduler::HasOperandDependency(
    const Instruction* instr1, const Instruction* instr2) const {
  for (size_t i = 0; i < instr1->OutputCount(); ++i) {
    for (size_t j = 0; j < instr2->InputCount(); ++j) {
      const InstructionOperand* output = instr1->OutputAt(i);
      const InstructionOperand* input = instr2->InputAt(j);

      if (output->IsUnallocated() && input->IsUnallocated() &&
          (UnallocatedOperand::cast(output)->virtual_register() ==
           UnallocatedOperand::cast(input)->virtual_register())) {
        return true;
      }

      if (output->IsConstant() && input->IsUnallocated() &&
          (ConstantOperand::cast(output)->virtual_register() ==
           UnallocatedOperand::cast(input)->virtual_register())) {
        return true;
      }
    }
  }

  // TODO(bafsa): Do we need to look for anti-dependencies/output-dependencies?

  return false;
}


bool InstructionScheduler::IsBlockTerminator(const Instruction* instr) const {
  return ((GetInstructionFlags(instr) & kIsBlockTerminator) ||
          (instr->flags_mode() == kFlags_branch));
}


void InstructionScheduler::ComputeTotalLatencies() {
  for (auto node : base::Reversed(graph_)) {
    int max_latency = 0;

    for (auto successor : node->successors()) {
      DCHECK(successor->total_latency() != -1);
      if (successor->total_latency() > max_latency) {
        max_latency = successor->total_latency();
      }
    }

    node->set_total_latency(max_latency + node->latency());
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-scheduler.h"

#include "src/base/adapters.h"
#include "src/base/utils/random-number-generator.h"

namespace v8 {
namespace internal {
namespace compiler {

void InstructionScheduler::SchedulingQueueBase::AddNode(
    ScheduleGraphNode* node) {
  // We keep the ready list sorted by total latency so that we can quickly find
  // the next best candidate to schedule.
  auto it = nodes_.begin();
  while ((it != nodes_.end()) &&
         ((*it)->total_latency() >= node->total_latency())) {
    ++it;
  }
  nodes_.insert(it, node);
}


InstructionScheduler::ScheduleGraphNode*
InstructionScheduler::CriticalPathFirstQueue::PopBestCandidate(int cycle) {
  DCHECK(!IsEmpty());
  auto candidate = nodes_.end();
  for (auto iterator = nodes_.begin(); iterator != nodes_.end(); ++iterator) {
    // We only consider instructions that have all their operands ready.
    if (cycle >= (*iterator)->start_cycle()) {
      candidate = iterator;
      break;
    }
  }

  if (candidate != nodes_.end()) {
    ScheduleGraphNode *result = *candidate;
    nodes_.erase(candidate);
    return result;
  }

  return nullptr;
}


InstructionScheduler::ScheduleGraphNode*
InstructionScheduler::StressSchedulerQueue::PopBestCandidate(int cycle) {
  DCHECK(!IsEmpty());
  // Choose a random element from the ready list.
  auto candidate = nodes_.begin();
  std::advance(candidate, isolate()->random_number_generator()->NextInt(
      static_cast<int>(nodes_.size())));
  ScheduleGraphNode *result = *candidate;
  nodes_.erase(candidate);
  return result;
}


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
      last_live_in_reg_marker_(nullptr),
      last_deopt_or_trap_(nullptr),
      operands_map_(zone) {}

void InstructionScheduler::StartBlock(RpoNumber rpo) {
  DCHECK(graph_.empty());
  DCHECK_NULL(last_side_effect_instr_);
  DCHECK(pending_loads_.empty());
  DCHECK_NULL(last_live_in_reg_marker_);
  DCHECK_NULL(last_deopt_or_trap_);
  DCHECK(operands_map_.empty());
  sequence()->StartBlock(rpo);
}


void InstructionScheduler::EndBlock(RpoNumber rpo) {
  if (FLAG_turbo_stress_instruction_scheduling) {
    ScheduleBlock<StressSchedulerQueue>();
  } else {
    ScheduleBlock<CriticalPathFirstQueue>();
  }
  sequence()->EndBlock(rpo);
  graph_.clear();
  last_side_effect_instr_ = nullptr;
  pending_loads_.clear();
  last_live_in_reg_marker_ = nullptr;
  last_deopt_or_trap_ = nullptr;
  operands_map_.clear();
}


void InstructionScheduler::AddInstruction(Instruction* instr) {
  ScheduleGraphNode* new_node = new (zone()) ScheduleGraphNode(zone(), instr);

  if (IsBlockTerminator(instr)) {
    // Make sure that basic block terminators are not moved by adding them
    // as successor of every instruction.
    for (ScheduleGraphNode* node : graph_) {
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

    // Make sure that instructions are not scheduled before the last
    // deoptimization or trap point when they depend on it.
    if ((last_deopt_or_trap_ != nullptr) && DependsOnDeoptOrTrap(instr)) {
      last_deopt_or_trap_->AddSuccessor(new_node);
    }

    // Instructions with side effects and memory operations can't be
    // reordered with respect to each other.
    if (HasSideEffect(instr)) {
      if (last_side_effect_instr_ != nullptr) {
        last_side_effect_instr_->AddSuccessor(new_node);
      }
      for (ScheduleGraphNode* load : pending_loads_) {
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
    } else if (instr->IsDeoptimizeCall() || instr->IsTrap()) {
      // Ensure that deopts or traps are not reordered with respect to
      // side-effect instructions.
      if (last_side_effect_instr_ != nullptr) {
        last_side_effect_instr_->AddSuccessor(new_node);
      }
      last_deopt_or_trap_ = new_node;
    }

    // Look for operand dependencies.
    for (size_t i = 0; i < instr->InputCount(); ++i) {
      const InstructionOperand* input = instr->InputAt(i);
      if (input->IsUnallocated()) {
        int32_t vreg = UnallocatedOperand::cast(input)->virtual_register();
        auto it = operands_map_.find(vreg);
        if (it != operands_map_.end()) {
          it->second->AddSuccessor(new_node);
        }
      }
    }

    // Record the virtual registers defined by this instruction.
    for (size_t i = 0; i < instr->OutputCount(); ++i) {
      const InstructionOperand* output = instr->OutputAt(i);
      if (output->IsUnallocated()) {
        operands_map_[UnallocatedOperand::cast(output)->virtual_register()] =
            new_node;
      } else if (output->IsConstant()) {
        operands_map_[ConstantOperand::cast(output)->virtual_register()] =
            new_node;
      }
    }
  }

  graph_.push_back(new_node);
}


template <typename QueueType>
void InstructionScheduler::ScheduleBlock() {
  QueueType ready_list(this);

  // Compute total latencies so that we can schedule the critical path first.
  ComputeTotalLatencies();

  // Add nodes which don't have dependencies to the ready list.
  for (ScheduleGraphNode* node : graph_) {
    if (!node->HasUnscheduledPredecessor()) {
      ready_list.AddNode(node);
    }
  }

  // Go through the ready list and schedule the instructions.
  int cycle = 0;
  while (!ready_list.IsEmpty()) {
    ScheduleGraphNode* candidate = ready_list.PopBestCandidate(cycle);

    if (candidate != nullptr) {
      sequence()->AddInstruction(candidate->instruction());

      for (ScheduleGraphNode* successor : candidate->successors()) {
        successor->DropUnscheduledPredecessor();
        successor->set_start_cycle(
            std::max(successor->start_cycle(),
                     cycle + candidate->latency()));

        if (!successor->HasUnscheduledPredecessor()) {
          ready_list.AddNode(successor);
        }
      }
    }

    cycle++;
  }
}


int InstructionScheduler::GetInstructionFlags(const Instruction* instr) const {
  switch (instr->arch_opcode()) {
    case kArchNop:
    case kArchFramePointer:
    case kArchParentFramePointer:
    case kArchStackSlot:  // Despite its name this opcode will produce a
                          // reference to a frame slot, so it is not affected
                          // by the arm64 dual stack issues mentioned below.
    case kArchComment:
      return kNoOpcodeFlags;

    case kArchTruncateDoubleToI:
    case kIeee754Float64Acos:
    case kIeee754Float64Acosh:
    case kIeee754Float64Asin:
    case kIeee754Float64Asinh:
    case kIeee754Float64Atan:
    case kIeee754Float64Atanh:
    case kIeee754Float64Atan2:
    case kIeee754Float64Cbrt:
    case kIeee754Float64Cos:
    case kIeee754Float64Cosh:
    case kIeee754Float64Exp:
    case kIeee754Float64Expm1:
    case kIeee754Float64Log:
    case kIeee754Float64Log1p:
    case kIeee754Float64Log10:
    case kIeee754Float64Log2:
    case kIeee754Float64Pow:
    case kIeee754Float64Sin:
    case kIeee754Float64Sinh:
    case kIeee754Float64Tan:
    case kIeee754Float64Tanh:
      return kNoOpcodeFlags;

    case kArchStackPointer:
      // ArchStackPointer instruction loads the current stack pointer value and
      // must not be reordered with instruction with side effects.
      return kIsLoadOperation;

    case kArchPrepareCallCFunction:
    case kArchSaveCallerRegisters:
    case kArchRestoreCallerRegisters:
    case kArchPrepareTailCall:
    case kArchCallCFunction:
    case kArchCallCodeObject:
    case kArchCallJSFunction:
    case kArchCallWasmFunction:
      return kHasSideEffect;

    case kArchTailCallCodeObjectFromJSFunction:
    case kArchTailCallCodeObject:
    case kArchTailCallAddress:
    case kArchTailCallWasm:
      return kHasSideEffect | kIsBlockTerminator;

    case kArchDeoptimize:
    case kArchJmp:
    case kArchLookupSwitch:
    case kArchTableSwitch:
    case kArchRet:
    case kArchDebugAbort:
    case kArchDebugBreak:
    case kArchThrowTerminator:
      return kIsBlockTerminator;

    case kArchStoreWithWriteBarrier:
      return kHasSideEffect;

    case kAtomicLoadInt8:
    case kAtomicLoadUint8:
    case kAtomicLoadInt16:
    case kAtomicLoadUint16:
    case kAtomicLoadWord32:
      return kIsLoadOperation;

    case kAtomicStoreWord8:
    case kAtomicStoreWord16:
    case kAtomicStoreWord32:
      return kHasSideEffect;

    case kAtomicExchangeInt8:
    case kAtomicExchangeUint8:
    case kAtomicExchangeInt16:
    case kAtomicExchangeUint16:
    case kAtomicExchangeWord32:
    case kAtomicCompareExchangeInt8:
    case kAtomicCompareExchangeUint8:
    case kAtomicCompareExchangeInt16:
    case kAtomicCompareExchangeUint16:
    case kAtomicCompareExchangeWord32:
    case kAtomicAddInt8:
    case kAtomicAddUint8:
    case kAtomicAddInt16:
    case kAtomicAddUint16:
    case kAtomicAddWord32:
    case kAtomicSubInt8:
    case kAtomicSubUint8:
    case kAtomicSubInt16:
    case kAtomicSubUint16:
    case kAtomicSubWord32:
    case kAtomicAndInt8:
    case kAtomicAndUint8:
    case kAtomicAndInt16:
    case kAtomicAndUint16:
    case kAtomicAndWord32:
    case kAtomicOrInt8:
    case kAtomicOrUint8:
    case kAtomicOrInt16:
    case kAtomicOrUint16:
    case kAtomicOrWord32:
    case kAtomicXorInt8:
    case kAtomicXorUint8:
    case kAtomicXorInt16:
    case kAtomicXorUint16:
    case kAtomicXorWord32:
      return kHasSideEffect;

#define CASE(Name) case k##Name:
    TARGET_ARCH_OPCODE_LIST(CASE)
#undef CASE
      return GetTargetInstructionFlags(instr);
  }

  UNREACHABLE();
}


bool InstructionScheduler::IsBlockTerminator(const Instruction* instr) const {
  return ((GetInstructionFlags(instr) & kIsBlockTerminator) ||
          (instr->flags_mode() == kFlags_branch));
}


void InstructionScheduler::ComputeTotalLatencies() {
  for (ScheduleGraphNode* node : base::Reversed(graph_)) {
    int max_latency = 0;

    for (ScheduleGraphNode* successor : node->successors()) {
      DCHECK_NE(-1, successor->total_latency());
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

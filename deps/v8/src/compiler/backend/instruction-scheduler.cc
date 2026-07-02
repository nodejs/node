// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/instruction-scheduler.h"

#include <optional>
#include <sstream>

#include "src/base/iterator.h"
#include "src/base/utils/random-number-generator.h"
#include "src/compiler/backend/instruction-codes.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...)                                                        \
  do {                                                                    \
    if (v8_flags.trace_turbo_instruction_scheduling) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_INST(instr)                              \
  do {                                                 \
    if (v8_flags.trace_turbo_instruction_scheduling) { \
      std::stringstream ss;                            \
      ss << ArchOpcodeField::decode(instr->opcode());  \
      PrintF("- %s\n", ss.str().c_str());              \
    }                                                  \
  } while (false)

void ResourceAllocation::PrintState() const {
#define RESOURCE_NAME(resource) "- " #resource
#define TRACE_UNITS(resource)     \
  TRACE(RESOURCE_NAME(resource)); \
  TRACE(": %d\n", GetFreeUnits(ArchInstResource::k##resource));

  TRACE("Free Units:\n");
#define DUMP_UNITS(resource) TRACE_UNITS(resource)
  FOREACH_ARCH_RESOURCE(DUMP_UNITS)

#undef DUMP_UNITS
#undef TRACE_UNITS
#undef RESOURCE_NAME
}

InstructionScheduler::SchedulingQueue::SchedulingQueue(
    ResourceAllocation resource_table, Zone* zone)
    : ready_(zone),
      waiting_(zone),
      resource_table_(resource_table),
      random_number_generator_(
          base::RandomNumberGenerator(v8_flags.random_seed)) {}

void InstructionScheduler::SchedulingQueue::AddNode(ScheduleGraphNode* node) {
  waiting_.push_back(node);
}

void InstructionScheduler::SchedulingQueue::AddReady(ScheduleGraphNode* node) {
  ready_.push_back(node);
}

void InstructionScheduler::SchedulingQueue::Advance(int cycle) {
  TRACE("Advancing Cycle\n");
  auto IsReady = [cycle](ScheduleGraphNode* n) {
    return cycle >= n->start_cycle();
  };
  auto it = std::partition(waiting_.begin(), waiting_.end(), IsReady);
  ready_.insert(ready_.end(), waiting_.begin(), it);
  waiting_.erase(waiting_.begin(), it);
  resource_table().Reset();
}

InstructionScheduler::ScheduleGraphNode*
InstructionScheduler::SchedulingQueue::PopBestCandidate(int cycle) {
  DCHECK(!IsEmpty());

  if (ready_.empty()) {
    TRACE("Ready queue empty\n");
    return nullptr;
  }

  if (v8_flags.trace_turbo_instruction_scheduling) {
    TRACE("Ready instructions:\n");
    for (auto& candidate : ready_) {
      TRACE_INST(candidate->instruction());
    }
    resource_table().PrintState();
  }

  if (!CanIssue(ArchInstResource::kFetch)) {
    TRACE("Can't fetch anymore\n");
    return nullptr;
  }

  // Filter the ready queue to only look at instructions for which we can issue.
  base::SmallVector<ScheduleGraphNode*, 8> issuable;
  std::copy_if(ready_.begin(), ready_.end(), std::back_inserter(issuable),
               [this](auto c) { return CanIssue(c); });

  if (issuable.empty()) {
    TRACE("None of ready queue can issue.\n");
    return nullptr;
  }

  if (V8_UNLIKELY(v8_flags.turbo_stress_instruction_scheduling)) {
    // Pop a random node from the queue to perform stress tests on the
    // scheduler.
    auto candidate = issuable.begin();
    std::advance(candidate, random_number_generator_->NextInt(
                                static_cast<int>(issuable.size())));
    ScheduleGraphNode* result = *candidate;
    ready_.erase(std::find(ready_.begin(), ready_.end(), result));
    MarkIssue(result->resource());
    return result;
  }

  // Prioritize the instruction with the highest latency on the path to reach
  // the end of the graph.
  auto best_candidate = std::max_element(
      issuable.begin(), issuable.end(), [this](auto l, auto r) {
        if (v8_flags.experimental_turbo_instruction_scheduling) {
          if (GetTotalLatency(l) == GetTotalLatency(r)) {
            // Tie-breaking strategies.
            return GetTieBreakLatency(l) < GetTieBreakLatency(r);
          }
        }
        return GetTotalLatency(l) < GetTotalLatency(r);
      });

  ScheduleGraphNode* result = *best_candidate;
  ready_.erase(std::find(ready_.begin(), ready_.end(), result));
  if (v8_flags.trace_turbo_instruction_scheduling) {
    TRACE("Ready instructions:\n");
    for (auto& candidate : ready_) {
      TRACE_INST(candidate->instruction());
    }
    TRACE("Selected:\n");
    TRACE_INST(result->instruction());
  }
  MarkIssue(result->resource());
  return result;
}

InstructionScheduler::ScheduleGraphNode::ScheduleGraphNode(
    Zone* zone, Instruction* instr, ArchInstResource resource)
    : instr_(instr),
      successors_(zone),
      data_successors_(zone),
      unscheduled_predecessors_count_(0),
      latency_(GetInstructionLatency(instr)),
      total_latency_(-1),
      start_cycle_(-1),
      resource_(resource) {}

void InstructionScheduler::ScheduleGraphNode::AddSuccessor(
    ScheduleGraphNode* node) {
  successors_.push_back(node);
  node->unscheduled_predecessors_count_++;
}

void InstructionScheduler::ScheduleGraphNode::AddDataSuccessor(
    ScheduleGraphNode* node) {
  data_successors_.push_back(node);
  AddSuccessor(node);
}

InstructionScheduler::InstructionScheduler(Zone* zone,
                                           InstructionSequence* sequence)
    : zone_(zone),
      sequence_(sequence),
      graph_(sequence->instructions().size(), zone),
      ready_list_(GetResourceTable(), zone),
      last_side_effect_instr_(nullptr),
      pending_loads_(zone),
      last_deopt_or_trap_(nullptr),
      operands_map_(zone) {}

void InstructionScheduler::StartBlock(RpoNumber rpo) {
  DCHECK(graph_.empty());
  DCHECK_NULL(last_side_effect_instr_);
  DCHECK(pending_loads_.empty());
  DCHECK_NULL(last_deopt_or_trap_);
  DCHECK(operands_map_.empty());
  sequence()->StartBlock(rpo);
}

void InstructionScheduler::EndBlock(RpoNumber rpo, Instruction* terminator) {
  Schedule();
  sequence()->EndBlock(rpo, terminator);
}

void InstructionScheduler::AddInstruction(Instruction* instr) {
  int flags = GetInstructionFlags(instr);
  // Avoid adding barriers into the graph and just schedule instead. Do the
  // same for FixedRegisterParameter which are regalloc placeholders that can't
  // be moved.
  if (IsBarrier(flags) || IsFixedRegisterParameter(instr)) {
    Schedule();
    sequence()->AddInstruction(instr);
    return;
  }

  ArchInstResource resource = GetInstructionResource(instr);
  ScheduleGraphNode* new_node =
      zone()->New<ScheduleGraphNode>(zone(), instr, resource);

  // We should not have branches in the middle of a block.
  DCHECK_NE(instr->flags_mode(), kFlags_branch);

  // Make sure that instructions are not scheduled before the last
  // deoptimization or trap point when they depend on it.
  if ((last_deopt_or_trap_ != nullptr) && DependsOnDeoptOrTrap(instr, flags)) {
    last_deopt_or_trap_->AddSuccessor(new_node);
  }

  // Instructions with side effects and memory operations can't be
  // reordered with respect to each other.
  if (HasSideEffect(flags)) {
    if (last_side_effect_instr_ != nullptr) {
      last_side_effect_instr_->AddSuccessor(new_node);
    }
    for (ScheduleGraphNode* load : pending_loads_) {
      load->AddSuccessor(new_node);
    }
    pending_loads_.clear();
    last_side_effect_instr_ = new_node;
  } else if (IsLoadOperation(flags)) {
    // Load operations can't be reordered with side effects instructions but
    // independent loads can be reordered with respect to each other.
    if (last_side_effect_instr_ != nullptr) {
      last_side_effect_instr_->AddSuccessor(new_node);
    }
    pending_loads_.push_back(new_node);
  } else if (instr->IsDeoptimizeCall() || CanTrap(instr)) {
    // Ensure that deopts or traps are not reordered with respect to
    // side-effect instructions.
    if (last_side_effect_instr_ != nullptr) {
      last_side_effect_instr_->AddSuccessor(new_node);
    }
  }

  // Update last deoptimization or trap point.
  if (instr->IsDeoptimizeCall() || CanTrap(instr)) {
    last_deopt_or_trap_ = new_node;
  }

  // Look for operand dependencies.
  for (size_t i = 0; i < instr->InputCount(); ++i) {
    const InstructionOperand* input = instr->InputAt(i);
    if (input->IsUnallocated()) {
      int32_t vreg = UnallocatedOperand::cast(input)->virtual_register();
      auto it = operands_map_.find(vreg);
      if (it != operands_map_.end()) {
        it->second->AddDataSuccessor(new_node);
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

  graph_.push_back(new_node);
}

void InstructionScheduler::Schedule() {
  // Compute total latencies so that we can schedule the critical path first.
  ComputeTotalLatencies();

  // Add nodes which don't have dependencies to the ready list.
  for (ScheduleGraphNode* node : graph_) {
    if (!node->HasUnscheduledPredecessor()) {
      ready_list_.AddReady(node);
    }
  }

  // Go through the ready list and schedule the instructions.
  int cycle = 0;
  while (!ready_list_.IsEmpty()) {
    while (!ready_list_.IsReadyEmpty()) {
      if (ScheduleGraphNode* candidate = ready_list_.PopBestCandidate(cycle)) {
        sequence()->AddInstruction(candidate->instruction());

        for (ScheduleGraphNode* successor : candidate->successors()) {
          successor->DropUnscheduledPredecessor();
          successor->set_start_cycle(
              std::max(successor->start_cycle(), cycle + candidate->latency()));

          if (!successor->HasUnscheduledPredecessor()) {
            ready_list_.AddNode(successor);
          }
        }
      } else {
        // There's no candidate, so break update the cycle.
        break;
      }
    }
    cycle++;
    ready_list_.Advance(cycle);
  }

  // Reset own state.
  graph_.clear();
  operands_map_.clear();
  pending_loads_.clear();
  last_deopt_or_trap_ = nullptr;
  last_side_effect_instr_ = nullptr;
}

int InstructionScheduler::GetInstructionFlags(const Instruction* instr) const {
  switch (instr->arch_opcode()) {
    case kArchNop:
    case kArchPause:
    case kArchStackCheckOffset:
    case kArchFramePointer:
    case kArchParentFramePointer:
    case kArchRootPointer:
    case kArchStackSlot:  // Despite its name this opcode will produce a
                          // reference to a frame slot, so it is not affected
                          // by the arm64 dual stack issues mentioned below.
    case kArchComment:
    case kArchDeoptimize:
    case kArchJmp:
    case kArchBinarySearchSwitch:
    case kArchRet:
    case kArchTableSwitch:
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

    case kArchStackPointerGreaterThan:
      // The ArchStackPointerGreaterThan instruction loads the current stack
      // pointer value and must not be reordered with instructions with side
      // effects.
      return kIsLoadOperation;

#if V8_ENABLE_WEBASSEMBLY
    case kArchStackPointer:
    case kArchSetStackPointer:
      // Instructions that load or set the stack pointer must not be reordered
      // with instructions with side effects or with each other.
      return kHasSideEffect;
    case kArchTrap:
      return kHasSideEffect;
#endif  // V8_ENABLE_WEBASSEMBLY

#if V8_ENABLE_SANDBOX
    case kArchLoadTrustedPointer:
      // May trap.
      return kIsLoadOperation | kHasSideEffect;
#endif  // V8_ENABLE_SANDBOX

    case kArchPrepareCallCFunction:
    case kArchPrepareTailCall:
    case kArchTailCallCodeObject:
    case kArchTailCallAddress:
#if V8_ENABLE_WEBASSEMBLY
    case kArchTailCallWasm:
    case kArchTailCallWasmIndirect:
#endif  // V8_ENABLE_WEBASSEMBLY
    case kArchAbortCSADcheck:
      return kHasSideEffect;

    case kArchDebugBreak:
      return kIsBarrier;

    case kArchSaveCallerRegisters:
    case kArchRestoreCallerRegisters:
      return kIsBarrier;

    case kArchCallCFunction:
    case kArchCallCodeObject:
    case kArchCallJSFunction:
#if V8_ENABLE_WEBASSEMBLY
    case kArchCallWasmFunction:
    case kArchCallWasmFunctionIndirect:
#endif  // V8_ENABLE_WEBASSEMBLY
    case kArchCallBuiltinPointer:
      // Calls can cause GC and GC may relocate objects. If a pure instruction
      // operates on a tagged pointer that was cast to a word then it may be
      // incorrect to move the instruction across the call. Hence we mark all
      // (non-tail-)calls as barriers.
      return kIsBarrier;

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
    case kArchSwitchSandboxMode:
      return kIsBarrier;
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

    case kArchStoreWithWriteBarrier:
    case kArchAtomicStoreWithWriteBarrier:
    case kArchStoreIndirectWithWriteBarrier:
      return kHasSideEffect;

    case kArchStoreSkippedWriteBarrier:
    case kArchAtomicStoreSkippedWriteBarrier:
    case kArchStoreIndirectSkippedWriteBarrier:
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
    case kAtomicExchangeWithWriteBarrier:
    case kAtomicCompareExchangeInt8:
    case kAtomicCompareExchangeUint8:
    case kAtomicCompareExchangeInt16:
    case kAtomicCompareExchangeUint16:
    case kAtomicCompareExchangeWord32:
    case kAtomicCompareExchangeWithWriteBarrier:
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

#undef TRACE
#undef TRACE_INST

}  // namespace compiler
}  // namespace internal
}  // namespace v8

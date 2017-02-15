// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INSTRUCTION_SCHEDULER_H_
#define V8_COMPILER_INSTRUCTION_SCHEDULER_H_

#include "src/compiler/instruction.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// A set of flags describing properties of the instructions so that the
// scheduler is aware of dependencies between instructions.
enum ArchOpcodeFlags {
  kNoOpcodeFlags = 0,
  kIsBlockTerminator = 1,  // The instruction marks the end of a basic block
                           // e.g.: jump and return instructions.
  kHasSideEffect = 2,      // The instruction has some side effects (memory
                           // store, function call...)
  kIsLoadOperation = 4,    // The instruction is a memory load.
  kMayNeedDeoptCheck = 8,  // The instruction might be associated with a deopt
                           // check. This is the case of instruction which can
                           // blow up with particular inputs (e.g.: division by
                           // zero on Intel platforms).
};

class InstructionScheduler final : public ZoneObject {
 public:
  InstructionScheduler(Zone* zone, InstructionSequence* sequence);

  void StartBlock(RpoNumber rpo);
  void EndBlock(RpoNumber rpo);

  void AddInstruction(Instruction* instr);

  static bool SchedulerSupported();

 private:
  // A scheduling graph node.
  // Represent an instruction and their dependencies.
  class ScheduleGraphNode: public ZoneObject {
   public:
    ScheduleGraphNode(Zone* zone, Instruction* instr);

    // Mark the instruction represented by 'node' as a dependecy of this one.
    // The current instruction will be registered as an unscheduled predecessor
    // of 'node' (i.e. it must be scheduled before 'node').
    void AddSuccessor(ScheduleGraphNode* node);

    // Check if all the predecessors of this instruction have been scheduled.
    bool HasUnscheduledPredecessor() {
      return unscheduled_predecessors_count_ != 0;
    }

    // Record that we have scheduled one of the predecessors of this node.
    void DropUnscheduledPredecessor() {
      DCHECK(unscheduled_predecessors_count_ > 0);
      unscheduled_predecessors_count_--;
    }

    Instruction* instruction() { return instr_; }
    ZoneDeque<ScheduleGraphNode*>& successors() { return successors_; }
    int latency() const { return latency_; }

    int total_latency() const { return total_latency_; }
    void set_total_latency(int latency) { total_latency_ = latency; }

    int start_cycle() const { return start_cycle_; }
    void set_start_cycle(int start_cycle) { start_cycle_ = start_cycle; }

   private:
    Instruction* instr_;
    ZoneDeque<ScheduleGraphNode*> successors_;

    // Number of unscheduled predecessors for this node.
    int unscheduled_predecessors_count_;

    // Estimate of the instruction latency (the number of cycles it takes for
    // instruction to complete).
    int latency_;

    // The sum of all the latencies on the path from this node to the end of
    // the graph (i.e. a node with no successor).
    int total_latency_;

    // The scheduler keeps a nominal cycle count to keep track of when the
    // result of an instruction is available. This field is updated by the
    // scheduler to indicate when the value of all the operands of this
    // instruction will be available.
    int start_cycle_;
  };

  // Keep track of all nodes ready to be scheduled (i.e. all their dependencies
  // have been scheduled. Note that this class is inteded to be extended by
  // concrete implementation of the scheduling queue which define the policy
  // to pop node from the queue.
  class SchedulingQueueBase {
   public:
    explicit SchedulingQueueBase(InstructionScheduler* scheduler)
      : scheduler_(scheduler),
        nodes_(scheduler->zone()) {
    }

    void AddNode(ScheduleGraphNode* node);

    bool IsEmpty() const {
      return nodes_.empty();
    }

   protected:
    InstructionScheduler* scheduler_;
    ZoneLinkedList<ScheduleGraphNode*> nodes_;
  };

  // A scheduling queue which prioritize nodes on the critical path (we look
  // for the instruction with the highest latency on the path to reach the end
  // of the graph).
  class CriticalPathFirstQueue : public SchedulingQueueBase  {
   public:
    explicit CriticalPathFirstQueue(InstructionScheduler* scheduler)
      : SchedulingQueueBase(scheduler) { }

    // Look for the best candidate to schedule, remove it from the queue and
    // return it.
    ScheduleGraphNode* PopBestCandidate(int cycle);
  };

  // A queue which pop a random node from the queue to perform stress tests on
  // the scheduler.
  class StressSchedulerQueue : public SchedulingQueueBase  {
   public:
    explicit StressSchedulerQueue(InstructionScheduler* scheduler)
      : SchedulingQueueBase(scheduler) { }

    ScheduleGraphNode* PopBestCandidate(int cycle);

   private:
    Isolate *isolate() {
      return scheduler_->isolate();
    }
  };

  // Perform scheduling for the current block specifying the queue type to
  // use to determine the next best candidate.
  template <typename QueueType>
  void ScheduleBlock();

  // Return the scheduling properties of the given instruction.
  int GetInstructionFlags(const Instruction* instr) const;
  int GetTargetInstructionFlags(const Instruction* instr) const;

  // Return true if the instruction is a basic block terminator.
  bool IsBlockTerminator(const Instruction* instr) const;

  // Check whether the given instruction has side effects (e.g. function call,
  // memory store).
  bool HasSideEffect(const Instruction* instr) const {
    return (GetInstructionFlags(instr) & kHasSideEffect) != 0;
  }

  // Return true if the instruction is a memory load.
  bool IsLoadOperation(const Instruction* instr) const {
    return (GetInstructionFlags(instr) & kIsLoadOperation) != 0;
  }

  // Return true if this instruction is usually associated with a deopt check
  // to validate its input.
  bool MayNeedDeoptCheck(const Instruction* instr) const {
    return (GetInstructionFlags(instr) & kMayNeedDeoptCheck) != 0;
  }

  // Return true if the instruction cannot be moved before the last deopt
  // point we encountered.
  bool DependsOnDeoptimization(const Instruction* instr) const {
    return MayNeedDeoptCheck(instr) || instr->IsDeoptimizeCall() ||
           HasSideEffect(instr) || IsLoadOperation(instr);
  }

  // Identify nops used as a definition point for live-in registers at
  // function entry.
  bool IsFixedRegisterParameter(const Instruction* instr) const {
    return (instr->arch_opcode() == kArchNop) && (instr->OutputCount() == 1) &&
           (instr->OutputAt(0)->IsUnallocated()) &&
           (UnallocatedOperand::cast(instr->OutputAt(0))
                ->HasFixedRegisterPolicy() ||
            UnallocatedOperand::cast(instr->OutputAt(0))
                ->HasFixedFPRegisterPolicy());
  }

  void ComputeTotalLatencies();

  static int GetInstructionLatency(const Instruction* instr);

  Zone* zone() { return zone_; }
  InstructionSequence* sequence() { return sequence_; }
  Isolate* isolate() { return sequence()->isolate(); }

  Zone* zone_;
  InstructionSequence* sequence_;
  ZoneVector<ScheduleGraphNode*> graph_;

  // Last side effect instruction encountered while building the graph.
  ScheduleGraphNode* last_side_effect_instr_;

  // Set of load instructions encountered since the last side effect instruction
  // which will be added as predecessors of the next instruction with side
  // effects.
  ZoneVector<ScheduleGraphNode*> pending_loads_;

  // Live-in register markers are nop instructions which are emitted at the
  // beginning of a basic block so that the register allocator will find a
  // defining instruction for live-in values. They must not be moved.
  // All these nops are chained together and added as a predecessor of every
  // other instructions in the basic block.
  ScheduleGraphNode* last_live_in_reg_marker_;

  // Last deoptimization instruction encountered while building the graph.
  ScheduleGraphNode* last_deopt_;

  // Keep track of definition points for virtual registers. This is used to
  // record operand dependencies in the scheduling graph.
  ZoneMap<int32_t, ScheduleGraphNode*> operands_map_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INSTRUCTION_SCHEDULER_H_

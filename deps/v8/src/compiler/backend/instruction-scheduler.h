// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_INSTRUCTION_SCHEDULER_H_
#define V8_COMPILER_BACKEND_INSTRUCTION_SCHEDULER_H_

#include <optional>

#include "src/base/utils/random-number-generator.h"
#include "src/compiler/backend/instruction.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// A set of flags describing properties of the instructions so that the
// scheduler is aware of dependencies between instructions.
enum ArchOpcodeFlags {
  kNoOpcodeFlags = 0,
  kHasSideEffect = 1,    // The instruction has some side effects (memory
                         // store, function call...)
  kIsLoadOperation = 2,  // The instruction is a memory load.
  kMayNeedDeoptOrTrapCheck = 4,  // The instruction may be associated with a
                                 // deopt or trap check which must be run before
                                 // instruction e.g. div on Intel platform which
                                 // will raise an exception when the divisor is
                                 // zero.
  kIsBarrier = 8,  // The instruction can cause GC or it reads/writes registers
                   // that are not explicitly given. Nothing can be reordered
                   // across such an instruction.
};

#define FOREACH_ARCH_RESOURCE(V) \
  V(Fetch)                       \
  V(IntSingle)                   \
  V(IntMulti)                    \
  V(FP)                          \
  V(Load)                        \
  V(Store)

enum class ArchInstResource : uint8_t {
#define DEFINE_RESOURCE(resource) k##resource,
  FOREACH_ARCH_RESOURCE(DEFINE_RESOURCE)
#undef DEFINE_RESOURCE
      kNumResources,
};

class ResourceAllocation {
 public:
  using TableEntry = std::pair<ArchInstResource, int8_t>;
  constexpr static int kNumResources =
      static_cast<int>(ArchInstResource::kNumResources);

  explicit ResourceAllocation(std::array<TableEntry, kNumResources> resources) {
#ifdef DEBUG
    // Check all the entries in the array are for unique resources.
    std::unordered_set<ArchInstResource> resource_set;
    for (auto [resource, units] : resources) {
      resource_set.insert(resource);
    }
    DCHECK_EQ(resource_set.size(), resources.size());
#endif  // DEBUG
    for (auto [resource, units] : resources) {
      SetUnits(resource, units);
    }
    Reset();
  }

  void PrintState() const;

  void MarkIssue(ArchInstResource resource) {
    int index = static_cast<int>(resource);
    DCHECK_GT(free_units_[index], 0);
    free_units_[index]--;
  }

  inline bool CanIssue(ArchInstResource resource) const {
    DCHECK_GE(GetFreeUnits(resource), 0);
    return GetFreeUnits(resource) != 0;
  }

  // Get the number of free units of the given resource.
  int8_t GetFreeUnits(ArchInstResource resource) const {
    return free_units_[static_cast<int>(resource)];
  }

  // Reset the allocation table.
  void Reset() {
    for (int i = 0; i < static_cast<int>(ArchInstResource::kNumResources);
         ++i) {
      FreeUnits(static_cast<ArchInstResource>(i));
    }
  }

 private:
  // Set the maximum number of units for the given resource.
  void SetUnits(ArchInstResource resource, int8_t units) {
    total_units_[static_cast<int>(resource)] = units;
  }

  // Set the number of free units to the maximum available.
  void FreeUnits(ArchInstResource resource) {
    int8_t total = GetTotalUnits(resource);
    free_units_[static_cast<int>(resource)] = total;
  }

  // Get the total number of units of the given resource.
  int8_t GetTotalUnits(ArchInstResource resource) const {
    return total_units_[static_cast<int>(resource)];
  }

  std::array<int8_t, kNumResources> total_units_ = {0};
  std::array<int8_t, kNumResources> free_units_ = {0};
};

enum class SchedulingDirection : uint8_t {
  kForward,
  kBackward,
};

enum class UseTieBreaker : uint8_t { kDisable, kEnable };

class InstructionScheduler final : public ZoneObject {
 public:
  V8_EXPORT_PRIVATE InstructionScheduler(Zone* zone,
                                         InstructionSequence* sequence);

  V8_EXPORT_PRIVATE void StartBlock(RpoNumber rpo);
  V8_EXPORT_PRIVATE void EndBlock(RpoNumber rpo, Instruction* terminator);

  V8_EXPORT_PRIVATE void AddInstruction(Instruction* instr);

  static bool SchedulerSupported();

 private:
  // A scheduling graph node.
  // Represent an instruction and their dependencies.
  class ScheduleGraphNode : public ZoneObject {
   public:
    using SuccessorList = SmallZoneVector<ScheduleGraphNode*, 8>;

    ScheduleGraphNode(Zone* zone, Instruction* instr,
                      ArchInstResource resource);

    // Mark the instruction represented by 'node' as a dependency of this one.
    // The current instruction will be registered as an unscheduled predecessor
    // of 'node' (i.e. it must be scheduled before 'node').
    void AddSuccessor(ScheduleGraphNode* node);

    // Mark the instruction represented by 'node' as a data-dependency of this
    // one. The current instruction will be registered as an unscheduled
    // predecessor of 'node' (i.e. it must be scheduled before 'node').
    void AddDataSuccessor(ScheduleGraphNode* node);

    // Mark the current instruction as dependent upon the instruction
    // represented by 'node'. The instruction at 'node' will be registered as an
    // unscheduled successor of the current instruction.
    void AddPredecessor(ScheduleGraphNode* node);

    // Mark the current instruction as data-dependent upon the instruction
    // represented by 'node'. The instruction at 'node' will be registered as an
    // unscheduled successor of the current instruction.
    void AddDataPredecessor(ScheduleGraphNode* node);

    // Check if all the predecessors of this instruction have been scheduled.
    bool HasUnscheduledPredecessor() {
      return unscheduled_predecessors_count_ != 0;
    }
    // Check if all the successors of this instruction have been scheduled.
    bool HasUnscheduledSuccessor() { return unscheduled_successor_count_ != 0; }

    // Record that we have scheduled one of the predecessors of this node.
    void DropUnscheduledPredecessor() {
      DCHECK_LT(0, unscheduled_predecessors_count_);
      unscheduled_predecessors_count_--;
    }

    // Record that we have scheduled one of the successors of this node.
    void DropUnscheduledSuccessor() {
      DCHECK_LT(0, unscheduled_successor_count_);
      unscheduled_successor_count_--;
    }

    int unscheduled_predecessors_count() const {
      return unscheduled_predecessors_count_;
    }
    int unscheduled_successor_count() const {
      return unscheduled_successor_count_;
    }

    Instruction* instruction() { return instr_; }
    SuccessorList& successors() { return successors_; }
    SuccessorList& data_successors() { return data_successors_; }
    const SuccessorList& data_successors() const { return data_successors_; }
    SuccessorList& predecessors() { return predecessors_; }
    SuccessorList& data_predecessors() { return data_predecessors_; }
    const SuccessorList& data_predecessors() const {
      return data_predecessors_;
    }
    int latency() const { return latency_; }

    int total_forward_latency() const { return total_forward_latency_; }
    void set_total_forward_latency(int latency) {
      total_forward_latency_ = latency;
    }

    int total_backward_latency() const { return total_backward_latency_; }
    void set_total_backward_latency(int latency) {
      total_backward_latency_ = latency;
    }

    int start_cycle() const { return start_cycle_; }
    void set_start_cycle(int start_cycle) { start_cycle_ = start_cycle; }

    ArchInstResource resource() const { return resource_; }

   private:
    Instruction* instr_;
    SuccessorList successors_;
    SuccessorList data_successors_;
    SuccessorList predecessors_;
    SuccessorList data_predecessors_;

    // Number of unscheduled predecessors for this node.
    int unscheduled_predecessors_count_;

    // Number of unscheduled successors for this node.
    int unscheduled_successor_count_;

    // Estimate of the instruction latency (the number of cycles it takes for
    // instruction to complete).
    int latency_;

    // The sum of all the latencies on the path from this node to the end of
    // the graph (i.e. a node with no successor).
    int total_forward_latency_;

    // The sum of all the latencies on the path from this node to the start of
    // the graph (i.e. a node with no predecessor).
    int total_backward_latency_;

    // The scheduler keeps a nominal cycle count to keep track of when the
    // result of an instruction is available. This field is updated by the
    // scheduler to indicate when the value of all the operands of this
    // instruction will be available.
    int start_cycle_;

    // The hardware resource that instr requires.
    ArchInstResource resource_;
  };

  // Keep track of all nodes ready to be scheduled (i.e. all their dependencies
  // have been scheduled.
  template <SchedulingDirection kDirection>
  class SchedulingQueue {
   public:
    explicit SchedulingQueue(ResourceAllocation resource_table, Zone* zone);

    void Advance(int cycle);
    void AddNode(ScheduleGraphNode* node);
    void AddReady(ScheduleGraphNode* node);

    bool IsEmpty() const { return IsWaitingEmpty() && IsReadyEmpty(); }
    bool IsReadyEmpty() const { return ready_.empty(); }
    bool IsWaitingEmpty() const { return waiting_.empty(); }
    template <UseTieBreaker kUseTieBreaker>
    ScheduleGraphNode* PopBestCandidate(int cycle);

   protected:
    struct ReadyQueuePayload {
      ScheduleGraphNode* node;
      ArchInstResource resource;
    };
    inline bool CanIssue(ArchInstResource resource) {
      return resource_table().CanIssue(resource);
    }
    inline bool CanIssue(const ScheduleGraphNode* node) {
      return resource_table().CanIssue(node->resource());
    }
    void MarkIssue(ArchInstResource resource) {
      resource_table().MarkIssue(resource);
      resource_table().MarkIssue(ArchInstResource::kFetch);
    }

    int GetTotalLatency(const ScheduleGraphNode* node) const {
      if constexpr (kDirection == SchedulingDirection::kForward) {
        return node->total_forward_latency();
      } else {
        return node->total_backward_latency();
      }
    }

    // Use this heuristic when total latencies are the same.
    size_t GetTieBreakLatency(const ScheduleGraphNode* node) const {
      // The main heuristic looks at total latency, so start with only the
      // latency of the node for the tie-breaker.
      int latency = node->latency();
      if constexpr (kDirection == SchedulingDirection::kForward) {
        // Accumulate the total latency of all the successors that are just
        // waiting for node to be scheduled.
        for (ScheduleGraphNode* successor : node->data_successors()) {
          if (successor->unscheduled_predecessors_count() == 1) {
            latency += successor->total_forward_latency();
          }
        }
      } else {
        // Accumulate the total latency of all the predecessors that are just
        // waiting for node to be scheduled.
        for (ScheduleGraphNode* predecessor : node->data_predecessors()) {
          if (predecessor->unscheduled_successor_count() == 1) {
            latency += predecessor->total_backward_latency();
          }
        }
      }
      return latency;
    }

    ResourceAllocation& resource_table() { return resource_table_; }

   private:
    SmallZoneVector<ScheduleGraphNode*, 8> ready_;
    SmallZoneVector<ScheduleGraphNode*, 16> waiting_;
    ResourceAllocation resource_table_;
    std::optional<base::RandomNumberGenerator> random_number_generator_;
  };

  using ForwardSchedulingQueue = SchedulingQueue<SchedulingDirection::kForward>;
  using BackwardSchedulingQueue =
      SchedulingQueue<SchedulingDirection::kBackward>;

  // Perform scheduling for the current block specifying the queue type to
  // use to determine the next best candidate.
  void Schedule();
  // Reset own state.
  void Reset();
  // Perform forward scheduling for the current block.
  template <UseTieBreaker kUseTieBreaker>
  int ScheduleForward();
  // Perform backwardscheduling for the current block.
  template <UseTieBreaker kUseTieBreaker>
  int ScheduleBackward();

  // Return the scheduling properties of the given instruction.
  V8_EXPORT_PRIVATE int GetInstructionFlags(const Instruction* instr) const;
  int GetTargetInstructionFlags(const Instruction* instr) const;

  bool IsBarrier(int flags) const { return (flags & kIsBarrier) != 0; }

  // Check whether the given instruction has side effects (e.g. function call,
  // memory store).
  bool HasSideEffect(int flags) const { return (flags & kHasSideEffect) != 0; }

  // Return true if the instruction is a memory load.
  bool IsLoadOperation(int flags) const {
    return (flags & kIsLoadOperation) != 0;
  }

  bool CanTrap(const Instruction* instr) const {
    return instr->IsTrap() || instr->IsConditionalTrap() ||
           (instr->HasMemoryAccessMode() &&
            instr->memory_access_mode() != kMemoryAccessDirect);
  }

  // The scheduler will not move the following instructions before the last
  // deopt/trap check:
  //  * loads (this is conservative)
  //  * instructions with side effect
  //  * other deopts/traps
  // Any other instruction can be moved, apart from those that raise exceptions
  // on specific inputs - these are filtered out by the deopt/trap check.
  bool MayNeedDeoptOrTrapCheck(const Instruction* instr) const {
    return (GetInstructionFlags(instr) & kMayNeedDeoptOrTrapCheck) != 0;
  }

  // Return true if the instruction cannot be moved before the last deopt or
  // trap point we encountered.
  bool DependsOnDeoptOrTrap(const Instruction* instr, int flags) const {
    return MayNeedDeoptOrTrapCheck(instr) || instr->IsDeoptimizeCall() ||
           CanTrap(instr) || HasSideEffect(flags) || IsLoadOperation(flags);
  }

  // Live-in register markers are nop instructions which are emitted at the
  // beginning of a basic block so that the register allocator will find a
  // defining instruction for live-in values. They must not be moved.
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
  static ArchInstResource GetInstructionResource(const Instruction* instr);
  static ResourceAllocation GetResourceTable();

  Zone* zone() { return zone_; }
  InstructionSequence* sequence() { return sequence_; }

  using TempInstrList = SmallZoneVector<Instruction*, 8>;
  TempInstrList& forward_sequence() { return forward_sequence_; }
  TempInstrList& backward_sequence() { return backward_sequence_; }

  Zone* zone_;
  InstructionSequence* sequence_;
  TempInstrList forward_sequence_;
  TempInstrList backward_sequence_;
  ZoneVector<ScheduleGraphNode*> graph_;
  ForwardSchedulingQueue forward_ready_list_;
  BackwardSchedulingQueue backward_ready_list_;

  friend class InstructionSchedulerTester;

  // Last side effect instruction encountered while building the graph.
  ScheduleGraphNode* last_side_effect_instr_;

  // Set of load instructions encountered since the last side effect instruction
  // which will be added as predecessors of the next instruction with side
  // effects.
  ZoneVector<ScheduleGraphNode*> pending_loads_;

  // Last deoptimization or trap instruction encountered while building the
  // graph.
  ScheduleGraphNode* last_deopt_or_trap_;

  // Keep track of definition points for virtual registers. This is used to
  // record operand dependencies in the scheduling graph.
  ZoneUnorderedMap<int32_t, ScheduleGraphNode*> operands_map_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_INSTRUCTION_SCHEDULER_H_

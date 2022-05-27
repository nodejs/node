// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_REGALLOC_H_
#define V8_MAGLEV_MAGLEV_REGALLOC_H_

#include "src/codegen/reglist.h"
#include "src/compiler/backend/instruction.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-regalloc-data.h"

namespace v8 {
namespace internal {
namespace maglev {

class MaglevCompilationInfo;
class MaglevPrintingVisitor;
class MergePointRegisterState;

template <typename RegisterT>
class RegisterFrameState {
 public:
  static constexpr bool kIsGeneralRegister =
      std::is_same<Register, RegisterT>();
  static constexpr bool kIsDoubleRegister =
      std::is_same<DoubleRegister, RegisterT>();

  static_assert(kIsGeneralRegister || kIsDoubleRegister,
                "RegisterFrameState should be used only for Register and "
                "DoubleRegister.");

  using RegList = RegListBase<RegisterT>;

  static constexpr RegList kAllocatableRegisters =
      AllocatableRegisters<RegisterT>::kRegisters;
  static constexpr RegList kEmptyRegList = {};

  RegList empty() const { return kEmptyRegList; }
  RegList free() const { return free_; }
  RegList used() const {
    // Only allocatable registers should be free.
    DCHECK_EQ(free_, free_ & kAllocatableRegisters);
    return kAllocatableRegisters ^ free_;
  }

  bool FreeIsEmpty() const { return free_ == kEmptyRegList; }

  template <typename Function>
  void ForEachUsedRegister(Function&& f) const {
    for (RegisterT reg : used()) {
      f(reg, GetValue(reg));
    }
  }

  RegisterT TakeFirstFree() { return free_.PopFirst(); }
  void RemoveFromFree(RegisterT reg) { free_.clear(reg); }
  void AddToFree(RegisterT reg) { free_.set(reg); }

  void FreeRegistersUsedBy(ValueNode* node) {
    RegList list = node->ClearRegisters<RegisterT>();
    DCHECK_EQ(free_ & list, kEmptyRegList);
    free_ |= list;
  }

  void SetValue(RegisterT reg, ValueNode* node) {
    DCHECK(!free_.has(reg));
    values_[reg.code()] = node;
    node->AddRegister(reg);
  }
  ValueNode* GetValue(RegisterT reg) const {
    DCHECK(!free_.has(reg));
    ValueNode* node = values_[reg.code()];
    DCHECK_NOT_NULL(node);
    return node;
  }

  compiler::InstructionOperand TryAllocateRegister(ValueNode* node);

 private:
  ValueNode* values_[RegisterT::kNumRegisters];
  RegList free_ = kAllocatableRegisters;
};

class StraightForwardRegisterAllocator {
 public:
  StraightForwardRegisterAllocator(MaglevCompilationInfo* compilation_info,
                                   Graph* graph);
  ~StraightForwardRegisterAllocator();

 private:
  enum class AllocationStage { kAtStart, kAtEnd };

  RegisterFrameState<Register> general_registers_;
  RegisterFrameState<DoubleRegister> double_registers_;
  RegList free_general_registers_before_node_;
  DoubleRegList free_double_registers_before_node_;

  struct SpillSlotInfo {
    SpillSlotInfo(uint32_t slot_index, NodeIdT freed_at_position)
        : slot_index(slot_index), freed_at_position(freed_at_position) {}
    uint32_t slot_index;
    NodeIdT freed_at_position;
  };
  struct SpillSlots {
    int top = 0;
    // Sorted from earliest freed_at_position to latest freed_at_position.
    std::vector<SpillSlotInfo> free_slots;
  };

  SpillSlots untagged_;
  SpillSlots tagged_;

  void ComputePostDominatingHoles(Graph* graph);
  void AllocateRegisters(Graph* graph);

  void PrintLiveRegs() const;

  void UpdateUse(Input* input) { return UpdateUse(input->node(), input); }
  void UpdateUse(ValueNode* node, InputLocation* input_location);
  void UpdateUse(const EagerDeoptInfo& deopt_info);
  void UpdateUse(const LazyDeoptInfo& deopt_info);
  void UpdateUse(const MaglevCompilationUnit& unit,
                 const CheckpointedInterpreterState* state,
                 InputLocation* input_locations, int& index);

  void AllocateControlNode(ControlNode* node, BasicBlock* block);
  void AllocateNode(Node* node);
  void AllocateNodeResult(ValueNode* node);
  void AssignInput(Input& input);
  void AssignTemporaries(NodeBase* node);
  void TryAllocateToInput(Phi* phi);

  void AddMoveBeforeCurrentNode(compiler::AllocatedOperand source,
                                compiler::AllocatedOperand target);

  void AllocateSpillSlot(ValueNode* node);
  void Spill(ValueNode* node);
  void SpillRegisters();

  template <typename RegisterT>
  void SpillAndClearRegisters(RegisterFrameState<RegisterT>& registers);
  void SpillAndClearRegisters();

  void FreeRegistersUsedBy(ValueNode* node);
  template <typename RegisterT>
  void FreeSomeRegister(RegisterFrameState<RegisterT>& registers,
                        AllocationStage stage);
  void FreeSomeGeneralRegister(AllocationStage stage);
  void FreeSomeDoubleRegister(AllocationStage stage);

  template <typename RegisterT>
  void DropRegisterValue(RegisterFrameState<RegisterT>& registers,
                         RegisterT reg, AllocationStage stage);
  void DropRegisterValue(Register reg, AllocationStage stage);
  void DropRegisterValue(DoubleRegister reg, AllocationStage stage);

  compiler::AllocatedOperand AllocateRegister(ValueNode* node,
                                              AllocationStage stage);

  template <typename RegisterT>
  compiler::AllocatedOperand ForceAllocate(
      RegisterFrameState<RegisterT>& registers, RegisterT reg, ValueNode* node,
      AllocationStage stage);
  compiler::AllocatedOperand ForceAllocate(Register reg, ValueNode* node,
                                           AllocationStage stage);
  compiler::AllocatedOperand ForceAllocate(DoubleRegister reg, ValueNode* node,
                                           AllocationStage stage);
  compiler::AllocatedOperand ForceAllocate(const Input& input, ValueNode* node,
                                           AllocationStage stage);

  template <typename Function>
  void ForEachMergePointRegisterState(
      MergePointRegisterState& merge_point_state, Function&& f);

  void InitializeRegisterValues(MergePointRegisterState& target_state);
  void EnsureInRegister(MergePointRegisterState& target_state,
                        ValueNode* incoming);

  void InitializeBranchTargetRegisterValues(ControlNode* source,
                                            BasicBlock* target);
  void InitializeConditionalBranchRegisters(ConditionalControlNode* source,
                                            BasicBlock* target);
  void MergeRegisterValues(ControlNode* control, BasicBlock* target,
                           int predecessor_id);

  MaglevGraphLabeller* graph_labeller() const {
    return compilation_info_->graph_labeller();
  }

  MaglevCompilationInfo* compilation_info_;
  std::unique_ptr<MaglevPrintingVisitor> printing_visitor_;
  BlockConstIterator block_it_;
  NodeIterator node_it_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_REGALLOC_H_

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

  using RegTList = RegListBase<RegisterT>;

  static constexpr RegTList kAllocatableRegisters =
      AllocatableRegisters<RegisterT>::kRegisters;
  static constexpr RegTList kEmptyRegList = {};

  RegTList empty() const { return kEmptyRegList; }
  RegTList free() const { return free_; }
  RegTList unblocked_free() const { return free_ - blocked_; }
  RegTList used() const {
    // Only allocatable registers should be free.
    DCHECK_EQ(free_, free_ & kAllocatableRegisters);
    return kAllocatableRegisters ^ free_;
  }

  bool UnblockedFreeIsEmpty() const { return unblocked_free().is_empty(); }

  template <typename Function>
  void ForEachUsedRegister(Function&& f) const {
    for (RegisterT reg : used()) {
      f(reg, GetValue(reg));
    }
  }

  void RemoveFromFree(RegisterT reg) { free_.clear(reg); }
  void AddToFree(RegisterT reg) { free_.set(reg); }
  void AddToFree(RegTList list) { free_ |= list; }

  void FreeRegistersUsedBy(ValueNode* node) {
    RegTList list = node->ClearRegisters<RegisterT>();
    DCHECK_EQ(free_ & list, kEmptyRegList);
    free_ |= list;
  }

  void SetValue(RegisterT reg, ValueNode* node) {
    DCHECK(!free_.has(reg));
    DCHECK(!blocked_.has(reg));
    values_[reg.code()] = node;
    block(reg);
    node->AddRegister(reg);
  }
  void SetValueWithoutBlocking(RegisterT reg, ValueNode* node) {
    DCHECK(!free_.has(reg));
    DCHECK(!blocked_.has(reg));
    values_[reg.code()] = node;
    node->AddRegister(reg);
  }
  ValueNode* GetValue(RegisterT reg) const {
    DCHECK(!free_.has(reg));
    ValueNode* node = values_[reg.code()];
    DCHECK_NOT_NULL(node);
    return node;
  }
  RegTList blocked() const { return blocked_; }
  void block(RegisterT reg) { blocked_.set(reg); }
  void unblock(RegisterT reg) { blocked_.clear(reg); }
  bool is_blocked(RegisterT reg) { return blocked_.has(reg); }
  void clear_blocked() { blocked_ = kEmptyRegList; }

  compiler::AllocatedOperand ChooseInputRegister(ValueNode* node);
  compiler::AllocatedOperand AllocateRegister(ValueNode* node);

 private:
  ValueNode* values_[RegisterT::kNumRegisters];
  RegTList free_ = kAllocatableRegisters;
  RegTList blocked_ = kEmptyRegList;
};

class StraightForwardRegisterAllocator {
 public:
  StraightForwardRegisterAllocator(MaglevCompilationInfo* compilation_info,
                                   Graph* graph);
  ~StraightForwardRegisterAllocator();

 private:
  RegisterFrameState<Register> general_registers_;
  RegisterFrameState<DoubleRegister> double_registers_;

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

  void ComputePostDominatingHoles();
  void AllocateRegisters();

  void PrintLiveRegs() const;

  void UpdateUse(Input* input) { return UpdateUse(input->node(), input); }
  void UpdateUse(ValueNode* node, InputLocation* input_location);
  void UpdateUse(const EagerDeoptInfo& deopt_info);
  void UpdateUse(const LazyDeoptInfo& deopt_info);

  void AllocateControlNode(ControlNode* node, BasicBlock* block);
  void AllocateNode(Node* node);
  void AllocateNodeResult(ValueNode* node);
  void AssignFixedInput(Input& input);
  void AssignArbitraryRegisterInput(Input& input);
  void AssignAnyInput(Input& input);
  void AssignInputs(NodeBase* node);
  template <typename RegisterT>
  void AssignFixedTemporaries(RegisterFrameState<RegisterT>& registers,
                              NodeBase* node);
  void AssignFixedTemporaries(NodeBase* node);
  template <typename RegisterT>
  void AssignArbitraryTemporaries(RegisterFrameState<RegisterT>& registers,
                                  NodeBase* node);
  void AssignArbitraryTemporaries(NodeBase* node);
  void TryAllocateToInput(Phi* phi);

  void VerifyInputs(NodeBase* node);
  void VerifyRegisterState();

  void AddMoveBeforeCurrentNode(ValueNode* node,
                                compiler::InstructionOperand source,
                                compiler::AllocatedOperand target);

  void AllocateSpillSlot(ValueNode* node);
  void Spill(ValueNode* node);
  void SpillRegisters();

  template <typename RegisterT>
  void SpillAndClearRegisters(RegisterFrameState<RegisterT>& registers);
  void SpillAndClearRegisters();

  void SaveRegisterSnapshot(NodeBase* node);

  void FreeRegistersUsedBy(ValueNode* node);
  template <typename RegisterT>
  RegisterT FreeUnblockedRegister();
  template <typename RegisterT>
  RegisterT PickRegisterToFree(RegListBase<RegisterT> reserved);

  template <typename RegisterT>
  RegisterFrameState<RegisterT>& GetRegisterFrameState() {
    if constexpr (std::is_same<RegisterT, Register>::value) {
      return general_registers_;
    } else {
      return double_registers_;
    }
  }

  template <typename RegisterT>
  void DropRegisterValueAtEnd(RegisterT reg);
  template <typename RegisterT>
  void EnsureFreeRegisterAtEnd();
  compiler::AllocatedOperand AllocateRegisterAtEnd(ValueNode* node);

  template <typename RegisterT>
  void DropRegisterValue(RegisterFrameState<RegisterT>& registers,
                         RegisterT reg);
  void DropRegisterValue(Register reg);
  void DropRegisterValue(DoubleRegister reg);

  compiler::AllocatedOperand AllocateRegister(ValueNode* node);

  template <typename RegisterT>
  compiler::AllocatedOperand ForceAllocate(
      RegisterFrameState<RegisterT>& registers, RegisterT reg, ValueNode* node);
  compiler::AllocatedOperand ForceAllocate(Register reg, ValueNode* node);
  compiler::AllocatedOperand ForceAllocate(DoubleRegister reg, ValueNode* node);
  compiler::AllocatedOperand ForceAllocate(const Input& input, ValueNode* node);

  template <typename Function>
  void ForEachMergePointRegisterState(
      MergePointRegisterState& merge_point_state, Function&& f);

  void ClearRegisterValues();
  void InitializeRegisterValues(MergePointRegisterState& target_state);
#ifdef DEBUG
  bool IsInRegister(MergePointRegisterState& target_state, ValueNode* incoming);
#endif

  void InitializeBranchTargetRegisterValues(ControlNode* source,
                                            BasicBlock* target);
  void InitializeEmptyBlockRegisterValues(ControlNode* source,
                                          BasicBlock* target);
  void InitializeBranchTargetPhis(int predecessor_id, BasicBlock* target);
  void InitializeConditionalBranchTarget(ConditionalControlNode* source,
                                         BasicBlock* target);
  void MergeRegisterValues(ControlNode* control, BasicBlock* target,
                           int predecessor_id);

  MaglevGraphLabeller* graph_labeller() const {
    return compilation_info_->graph_labeller();
  }

  MaglevCompilationInfo* compilation_info_;
  std::unique_ptr<MaglevPrintingVisitor> printing_visitor_;
  Graph* graph_;
  BlockConstIterator block_it_;
  NodeIterator node_it_;
  // The current node, whether a Node in the body or the ControlNode.
  NodeBase* current_node_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_REGALLOC_H_

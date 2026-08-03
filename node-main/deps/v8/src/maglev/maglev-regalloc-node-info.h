// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_REGALLOC_NODE_INFO_H_
#define V8_MAGLEV_MAGLEV_REGALLOC_NODE_INFO_H_

#include <cstdint>

#include "src/compiler/backend/instruction.h"

namespace v8 {
namespace internal {
namespace maglev {

using NodeIdT = uint32_t;
static constexpr NodeIdT kInvalidNodeId = 0;
static constexpr NodeIdT kFirstValidNodeId = 1;

struct LiveRange {
  NodeIdT start = kInvalidNodeId;
  NodeIdT end = kInvalidNodeId;  // Inclusive.
};

class ValueLocation {
 public:
  ValueLocation() = default;

  template <typename... Args>
  void SetUnallocated(Args&&... args) {
    DCHECK(operand_.IsInvalid());
    operand_ = compiler::UnallocatedOperand(args...);
  }

  template <typename... Args>
  void SetAllocated(Args&&... args) {
    DCHECK(operand_.IsUnallocated());
    operand_ = compiler::AllocatedOperand(args...);
  }

  // Only to be used on inputs that inherit allocation.
  void InjectLocation(compiler::InstructionOperand location) {
    operand_ = location;
  }

  // We use USED_AT_START to indicate that the input will be clobbered.
  bool Cloberred() {
    DCHECK(operand_.IsUnallocated());
    return compiler::UnallocatedOperand::cast(operand_).IsUsedAtStart();
  }

  template <typename... Args>
  void SetConstant(Args&&... args) {
    DCHECK(operand_.IsUnallocated());
    operand_ = compiler::ConstantOperand(args...);
  }

  Register AssignedGeneralRegister() const {
    DCHECK(!IsDoubleRegister());
    return compiler::AllocatedOperand::cast(operand_).GetRegister();
  }

  DoubleRegister AssignedDoubleRegister() const {
    DCHECK(IsDoubleRegister());
    return compiler::AllocatedOperand::cast(operand_).GetDoubleRegister();
  }

  bool IsAnyRegister() const { return operand_.IsAnyRegister(); }
  bool IsGeneralRegister() const { return operand_.IsRegister(); }
  bool IsDoubleRegister() const { return operand_.IsDoubleRegister(); }

  const compiler::InstructionOperand& operand() const { return operand_; }
  const compiler::InstructionOperand& operand() { return operand_; }

 private:
  compiler::InstructionOperand operand_;
};

class InputLocation : public ValueLocation {
 public:
  NodeIdT next_use_id() const { return next_use_id_; }
  // Used in ValueNode::mark_use
  NodeIdT* get_next_use_id_address() { return &next_use_id_; }

 private:
  NodeIdT next_use_id_ = kInvalidNodeId;
};

// RegallocNodeInfo holds information about a node during register allocation.
// It stores an (ordered) id for live analysis and the temporary registers
// needed for the node's codegen.
class RegallocNodeInfo {
 public:
  RegallocNodeInfo(Zone* zone, int input_count)
      : input_locations_(zone->AllocateArray<InputLocation>(input_count)) {
    for (int i = 0; i < input_count; i++) {
      new (&input_locations_[i]) InputLocation();
    }
#ifdef DEBUG
    input_count_ = input_count;
#endif  // DEBUG
  }

  constexpr NodeIdT id() const { return id_; }
  void set_id(NodeIdT id) {
    DCHECK_EQ(id_, kInvalidNodeId);
    DCHECK_NE(id, kInvalidNodeId);
    id_ = id;
  }

  template <typename RegisterT>
  RegListBase<RegisterT>& temporaries() {
    if constexpr (std::is_same_v<RegisterT, Register>) {
      return temporaries_;
    } else {
      return double_temporaries_;
    }
  }

  RegList& general_temporaries() { return temporaries<Register>(); }
  DoubleRegList& double_temporaries() { return temporaries<DoubleRegister>(); }

  InputLocation* input_location(int index) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, input_count_);
    return &input_locations_[index];
  }

 protected:
  NodeIdT id_ = kInvalidNodeId;
  RegList temporaries_ = {};
  DoubleRegList double_temporaries_ = {};
  InputLocation* input_locations_;

#ifdef DEBUG
  int input_count_ = 0;
#endif  // DEBUG
};

// RegallocValueNodeInfo extends RegallocNodeInfo with information specific to
// ValueNodes. This includes the live range of the value, its location
// (registers, stack, or constant). It also stores temporary data and hints for
// the register allocator.
class RegallocValueNodeInfo : public RegallocNodeInfo {
 public:
  explicit RegallocValueNodeInfo(Zone* zone, int input_count,
                                 MachineRepresentation representation)
      : RegallocNodeInfo(zone, input_count),
        representation_(representation),
        last_uses_next_use_id_(&next_use_),
        hint_(compiler::InstructionOperand())
#ifdef DEBUG
        ,
        state_(kLastUse)
#endif  // DEBUG
  {
    if (use_double_register()) {
      double_registers_with_result_ = kEmptyDoubleRegList;
    } else {
      registers_with_result_ = kEmptyRegList;
    }
  }

  // Called by the LiveRangeAndNextUseProcessor to update the node live range.
  void record_next_use(NodeIdT id, InputLocation* input_location) {
    DCHECK_EQ(state_, kLastUse);
    DCHECK_NE(id, kInvalidNodeId);
    DCHECK_LT(id_, id);
    DCHECK_IMPLIES(has_valid_live_range(), id >= end_id_);  // range_.end);
    end_id_ = id;
    *last_uses_next_use_id_ = id;
    last_uses_next_use_id_ = input_location->get_next_use_id_address();
    DCHECK_EQ(*last_uses_next_use_id_, kInvalidNodeId);
  }

  bool has_valid_live_range() const { return end_id_ != kInvalidNodeId; }
  LiveRange live_range() const { return LiveRange{id_, end_id_}; }
  NodeIdT end_id() const { return end_id_; }

  // The following methods should only be used during register allocation, to
  // mark the _current_ state according to the register allocator.
  void advance_next_use(NodeIdT use) { next_use_ = use; }
  NodeIdT current_next_use() const { return next_use_; }
  bool has_no_more_uses() const { return next_use_ == kInvalidNodeId; }

  bool is_loadable() const {
    DCHECK_EQ(state_, kSpill);
    return spill_.IsConstant() || spill_.IsAnyStackSlot();
  }

  bool is_spilled() const {
    DCHECK_EQ(state_, kSpill);
    return spill_.IsAnyStackSlot();
  }

  compiler::InstructionOperand loadable_slot() const {
    DCHECK_EQ(state_, kSpill);
    DCHECK(is_loadable());
    return spill_;
  }

  compiler::AllocatedOperand spill_slot() const {
    DCHECK(is_spilled());
    return compiler::AllocatedOperand::cast(loadable_slot());
  }

  void Spill(compiler::AllocatedOperand operand) {
#ifdef DEBUG
    if (state_ == kLastUse) {
      state_ = kSpill;
    } else {
      DCHECK(!is_loadable());
    }
#endif  // DEBUG
    DCHECK(operand.IsAnyStackSlot());
    spill_ = operand;
    DCHECK(spill_.IsAnyStackSlot());
  }

  void SetNoSpill() {
#ifdef DEBUG
    state_ = kSpill;
#endif  // DEBUG
    spill_ = compiler::InstructionOperand();
  }

  void SetConstantLocation() {
#ifdef DEBUG
    state_ = kSpill;
#endif  // DEBUG
    spill_ = compiler::ConstantOperand(
        compiler::UnallocatedOperand::cast(result().operand())
            .virtual_register());
  }

  void AddRegister(Register reg) {
    DCHECK(!use_double_register());
    registers_with_result_.set(reg);
  }
  void AddRegister(DoubleRegister reg) {
    DCHECK(use_double_register());
    double_registers_with_result_.set(reg);
  }

  void RemoveRegister(Register reg) {
    DCHECK(!use_double_register());
    registers_with_result_.clear(reg);
  }
  void RemoveRegister(DoubleRegister reg) {
    DCHECK(use_double_register());
    double_registers_with_result_.clear(reg);
  }

  template <typename T>
  inline RegListBase<T> ClearRegisters();

  int num_registers() const {
    if (use_double_register()) {
      return double_registers_with_result_.Count();
    }
    return registers_with_result_.Count();
  }
  bool has_register() const {
    if (use_double_register()) {
      return double_registers_with_result_ != kEmptyDoubleRegList;
    }
    return registers_with_result_ != kEmptyRegList;
  }
  bool is_in_register(Register reg) const {
    DCHECK(!use_double_register());
    return registers_with_result_.has(reg);
  }
  bool is_in_register(DoubleRegister reg) const {
    DCHECK(use_double_register());
    return double_registers_with_result_.has(reg);
  }

  template <typename T>
  RegListBase<T> result_registers() {
    if constexpr (std::is_same_v<T, DoubleRegister>) {
      DCHECK(use_double_register());
      return double_registers_with_result_;
    } else {
      DCHECK(!use_double_register());
      return registers_with_result_;
    }
  }

  ValueLocation& result() { return result_; }
  const ValueLocation& result() const { return result_; }

  compiler::InstructionOperand allocation() const {
    if (has_register()) {
      return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                        representation_, FirstRegisterCode());
    }
    CHECK(is_loadable());
    return loadable_slot();
  }

  bool has_hint() { return !hint_.IsInvalid(); }
  void clear_hint() { hint_ = compiler::InstructionOperand(); }
  void set_hint(compiler::InstructionOperand hint) { hint_ = hint; }
  const compiler::InstructionOperand& hint() const {
    DCHECK(hint_.IsInvalid() || hint_.IsUnallocated());
    return hint_;
  }

  template <typename RegisterT>
  RegisterT GetRegisterHint() {
    if (hint_.IsInvalid()) return RegisterT::no_reg();
    return RegisterT::from_code(
        compiler::UnallocatedOperand::cast(hint_).fixed_register_index());
  }

 private:
  NodeIdT end_id_ = kInvalidNodeId;
  NodeIdT next_use_ = kInvalidNodeId;
  MachineRepresentation representation_;

  union {
    // Pointer to the current last use's next_use_id field. Most of the time
    // this will be a pointer to an Input's next_use_id_ field, but it's
    // initialized to this node's next_use_ to track the first use.
    NodeIdT*
        last_uses_next_use_id_;  // Only used by LiveRangeAndNextUseProcessor.
    compiler::InstructionOperand spill_;  // Only used by RegAlloc.
  };

  // TODO(victorgomes): These have the location of the node?
  ValueLocation result_;
  union {
    RegList registers_with_result_;
    DoubleRegList double_registers_with_result_;
  };
  compiler::InstructionOperand hint_;

#ifdef DEBUG
  enum { kLastUse, kSpill } state_;
#endif  // DEBUG

  bool use_double_register() const {
    return representation_ == MachineRepresentation::kFloat64;
  }

  int FirstRegisterCode() const {
    if (use_double_register()) {
      return double_registers_with_result_.first().code();
    }
    return registers_with_result_.first().code();
  }
};

template <>
inline RegList RegallocValueNodeInfo::ClearRegisters() {
  DCHECK(!use_double_register());
  return std::exchange(registers_with_result_, kEmptyRegList);
}

template <>
inline DoubleRegList RegallocValueNodeInfo::ClearRegisters() {
  DCHECK(use_double_register());
  return std::exchange(double_registers_with_result_, kEmptyDoubleRegList);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_REGALLOC_NODE_INFO_H_

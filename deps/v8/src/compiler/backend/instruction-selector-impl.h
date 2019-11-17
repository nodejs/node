// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_IMPL_H_
#define V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_IMPL_H_

#include "src/codegen/macro-assembler.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/schedule.h"

namespace v8 {
namespace internal {
namespace compiler {

struct CaseInfo {
  int32_t value;  // The case value.
  int32_t order;  // The order for lowering to comparisons (less means earlier).
  BasicBlock* branch;  // The basic blocks corresponding to the case value.
};

inline bool operator<(const CaseInfo& l, const CaseInfo& r) {
  return l.order < r.order;
}

// Helper struct containing data about a table or lookup switch.
class SwitchInfo {
 public:
  SwitchInfo(ZoneVector<CaseInfo> const& cases, int32_t min_value,
             int32_t max_value, BasicBlock* default_branch)
      : cases_(cases),
        min_value_(min_value),
        max_value_(max_value),
        default_branch_(default_branch) {
    if (cases.size() != 0) {
      DCHECK_LE(min_value, max_value);
      // Note that {value_range} can be 0 if {min_value} is -2^31 and
      // {max_value} is 2^31-1, so don't assume that it's non-zero below.
      value_range_ =
          1u + bit_cast<uint32_t>(max_value) - bit_cast<uint32_t>(min_value);
    } else {
      value_range_ = 0;
    }
  }

  // Ensure that comparison order of if-cascades is preserved.
  std::vector<CaseInfo> CasesSortedByOriginalOrder() const {
    std::vector<CaseInfo> result(cases_.begin(), cases_.end());
    std::stable_sort(result.begin(), result.end());
    return result;
  }
  std::vector<CaseInfo> CasesSortedByValue() const {
    std::vector<CaseInfo> result(cases_.begin(), cases_.end());
    std::stable_sort(result.begin(), result.end(),
                     [](CaseInfo a, CaseInfo b) { return a.value < b.value; });
    return result;
  }
  const ZoneVector<CaseInfo>& CasesUnsorted() const { return cases_; }
  int32_t min_value() const { return min_value_; }
  int32_t max_value() const { return max_value_; }
  size_t value_range() const { return value_range_; }
  size_t case_count() const { return cases_.size(); }
  BasicBlock* default_branch() const { return default_branch_; }

 private:
  const ZoneVector<CaseInfo>& cases_;
  int32_t min_value_;   // minimum value of {cases_}
  int32_t max_value_;   // maximum value of {cases_}
  size_t value_range_;  // |max_value - min_value| + 1
  BasicBlock* default_branch_;
};

// A helper class for the instruction selector that simplifies construction of
// Operands. This class implements a base for architecture-specific helpers.
class OperandGenerator {
 public:
  explicit OperandGenerator(InstructionSelector* selector)
      : selector_(selector) {}

  InstructionOperand NoOutput() {
    return InstructionOperand();  // Generates an invalid operand.
  }

  InstructionOperand DefineAsRegister(Node* node) {
    return Define(node,
                  UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                     GetVReg(node)));
  }

  InstructionOperand DefineSameAsFirst(Node* node) {
    return Define(node,
                  UnallocatedOperand(UnallocatedOperand::SAME_AS_FIRST_INPUT,
                                     GetVReg(node)));
  }

  InstructionOperand DefineAsFixed(Node* node, Register reg) {
    return Define(node, UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                                           reg.code(), GetVReg(node)));
  }

  template <typename FPRegType>
  InstructionOperand DefineAsFixed(Node* node, FPRegType reg) {
    return Define(node,
                  UnallocatedOperand(UnallocatedOperand::FIXED_FP_REGISTER,
                                     reg.code(), GetVReg(node)));
  }

  InstructionOperand DefineAsConstant(Node* node) {
    selector()->MarkAsDefined(node);
    int virtual_register = GetVReg(node);
    sequence()->AddConstant(virtual_register, ToConstant(node));
    return ConstantOperand(virtual_register);
  }

  InstructionOperand DefineAsLocation(Node* node, LinkageLocation location) {
    return Define(node, ToUnallocatedOperand(location, GetVReg(node)));
  }

  InstructionOperand DefineAsDualLocation(Node* node,
                                          LinkageLocation primary_location,
                                          LinkageLocation secondary_location) {
    return Define(node,
                  ToDualLocationUnallocatedOperand(
                      primary_location, secondary_location, GetVReg(node)));
  }

  InstructionOperand Use(Node* node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::NONE,
                                        UnallocatedOperand::USED_AT_START,
                                        GetVReg(node)));
  }

  InstructionOperand UseAnyAtEnd(Node* node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::REGISTER_OR_SLOT,
                                        UnallocatedOperand::USED_AT_END,
                                        GetVReg(node)));
  }

  InstructionOperand UseAny(Node* node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::REGISTER_OR_SLOT,
                                        UnallocatedOperand::USED_AT_START,
                                        GetVReg(node)));
  }

  InstructionOperand UseRegisterOrSlotOrConstant(Node* node) {
    return Use(node, UnallocatedOperand(
                         UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT,
                         UnallocatedOperand::USED_AT_START, GetVReg(node)));
  }

  InstructionOperand UseUniqueRegisterOrSlotOrConstant(Node* node) {
    return Use(node, UnallocatedOperand(
                         UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT,
                         GetVReg(node)));
  }

  InstructionOperand UseRegister(Node* node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                        UnallocatedOperand::USED_AT_START,
                                        GetVReg(node)));
  }

  InstructionOperand UseUniqueSlot(Node* node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::MUST_HAVE_SLOT,
                                        GetVReg(node)));
  }

  // Use register or operand for the node. If a register is chosen, it won't
  // alias any temporary or output registers.
  InstructionOperand UseUnique(Node* node) {
    return Use(node,
               UnallocatedOperand(UnallocatedOperand::NONE, GetVReg(node)));
  }

  // Use a unique register for the node that does not alias any temporary or
  // output registers.
  InstructionOperand UseUniqueRegister(Node* node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                        GetVReg(node)));
  }

  InstructionOperand UseFixed(Node* node, Register reg) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                                        reg.code(), GetVReg(node)));
  }

  template <typename FPRegType>
  InstructionOperand UseFixed(Node* node, FPRegType reg) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::FIXED_FP_REGISTER,
                                        reg.code(), GetVReg(node)));
  }

  InstructionOperand UseImmediate(int immediate) {
    return sequence()->AddImmediate(Constant(immediate));
  }

  InstructionOperand UseImmediate(Node* node) {
    return sequence()->AddImmediate(ToConstant(node));
  }

  InstructionOperand UseNegatedImmediate(Node* node) {
    return sequence()->AddImmediate(ToNegatedConstant(node));
  }

  InstructionOperand UseLocation(Node* node, LinkageLocation location) {
    return Use(node, ToUnallocatedOperand(location, GetVReg(node)));
  }

  // Used to force gap moves from the from_location to the to_location
  // immediately before an instruction.
  InstructionOperand UsePointerLocation(LinkageLocation to_location,
                                        LinkageLocation from_location) {
    UnallocatedOperand casted_from_operand =
        UnallocatedOperand::cast(TempLocation(from_location));
    selector_->Emit(kArchNop, casted_from_operand);
    return ToUnallocatedOperand(to_location,
                                casted_from_operand.virtual_register());
  }

  InstructionOperand TempRegister() {
    return UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                              UnallocatedOperand::USED_AT_START,
                              sequence()->NextVirtualRegister());
  }

  int AllocateVirtualRegister() { return sequence()->NextVirtualRegister(); }

  InstructionOperand DefineSameAsFirstForVreg(int vreg) {
    return UnallocatedOperand(UnallocatedOperand::SAME_AS_FIRST_INPUT, vreg);
  }

  InstructionOperand DefineAsRegistertForVreg(int vreg) {
    return UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg);
  }

  InstructionOperand UseRegisterForVreg(int vreg) {
    return UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                              UnallocatedOperand::USED_AT_START, vreg);
  }

  InstructionOperand TempDoubleRegister() {
    UnallocatedOperand op = UnallocatedOperand(
        UnallocatedOperand::MUST_HAVE_REGISTER,
        UnallocatedOperand::USED_AT_START, sequence()->NextVirtualRegister());
    sequence()->MarkAsRepresentation(MachineRepresentation::kFloat64,
                                     op.virtual_register());
    return op;
  }

  InstructionOperand TempSimd128Register() {
    UnallocatedOperand op = UnallocatedOperand(
        UnallocatedOperand::MUST_HAVE_REGISTER,
        UnallocatedOperand::USED_AT_START, sequence()->NextVirtualRegister());
    sequence()->MarkAsRepresentation(MachineRepresentation::kSimd128,
                                     op.virtual_register());
    return op;
  }

  InstructionOperand TempRegister(Register reg) {
    return UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER, reg.code(),
                              InstructionOperand::kInvalidVirtualRegister);
  }

  template <typename FPRegType>
  InstructionOperand TempFpRegister(FPRegType reg) {
    UnallocatedOperand op =
        UnallocatedOperand(UnallocatedOperand::FIXED_FP_REGISTER, reg.code(),
                           sequence()->NextVirtualRegister());
    sequence()->MarkAsRepresentation(MachineRepresentation::kSimd128,
                                     op.virtual_register());
    return op;
  }

  InstructionOperand TempImmediate(int32_t imm) {
    return sequence()->AddImmediate(Constant(imm));
  }

  InstructionOperand TempLocation(LinkageLocation location) {
    return ToUnallocatedOperand(location, sequence()->NextVirtualRegister());
  }

  InstructionOperand Label(BasicBlock* block) {
    return sequence()->AddImmediate(
        Constant(RpoNumber::FromInt(block->rpo_number())));
  }

 protected:
  InstructionSelector* selector() const { return selector_; }
  InstructionSequence* sequence() const { return selector()->sequence(); }
  Zone* zone() const { return selector()->instruction_zone(); }

 private:
  int GetVReg(Node* node) const { return selector_->GetVirtualRegister(node); }

  static Constant ToConstant(const Node* node) {
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
        return Constant(OpParameter<int32_t>(node->op()));
      case IrOpcode::kInt64Constant:
        return Constant(OpParameter<int64_t>(node->op()));
      case IrOpcode::kFloat32Constant:
        return Constant(OpParameter<float>(node->op()));
      case IrOpcode::kRelocatableInt32Constant:
      case IrOpcode::kRelocatableInt64Constant:
        return Constant(OpParameter<RelocatablePtrConstantInfo>(node->op()));
      case IrOpcode::kFloat64Constant:
      case IrOpcode::kNumberConstant:
        return Constant(OpParameter<double>(node->op()));
      case IrOpcode::kExternalConstant:
        return Constant(OpParameter<ExternalReference>(node->op()));
      case IrOpcode::kComment: {
        // We cannot use {intptr_t} here, since the Constant constructor would
        // be ambiguous on some architectures.
        using ptrsize_int_t =
            std::conditional<kSystemPointerSize == 8, int64_t, int32_t>::type;
        return Constant(reinterpret_cast<ptrsize_int_t>(
            OpParameter<const char*>(node->op())));
      }
      case IrOpcode::kHeapConstant:
        return Constant(HeapConstantOf(node->op()));
      case IrOpcode::kCompressedHeapConstant:
        return Constant(HeapConstantOf(node->op()), true);
      case IrOpcode::kDelayedStringConstant:
        return Constant(StringConstantBaseOf(node->op()));
      case IrOpcode::kDeadValue: {
        switch (DeadValueRepresentationOf(node->op())) {
          case MachineRepresentation::kBit:
          case MachineRepresentation::kWord32:
          case MachineRepresentation::kTagged:
          case MachineRepresentation::kTaggedSigned:
          case MachineRepresentation::kTaggedPointer:
          case MachineRepresentation::kCompressed:
          case MachineRepresentation::kCompressedSigned:
          case MachineRepresentation::kCompressedPointer:
            return Constant(static_cast<int32_t>(0));
          case MachineRepresentation::kFloat64:
            return Constant(static_cast<double>(0));
          case MachineRepresentation::kFloat32:
            return Constant(static_cast<float>(0));
          default:
            UNREACHABLE();
        }
        break;
      }
      default:
        break;
    }
    UNREACHABLE();
  }

  static Constant ToNegatedConstant(const Node* node) {
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
        return Constant(-OpParameter<int32_t>(node->op()));
      case IrOpcode::kInt64Constant:
        return Constant(-OpParameter<int64_t>(node->op()));
      default:
        break;
    }
    UNREACHABLE();
  }

  UnallocatedOperand Define(Node* node, UnallocatedOperand operand) {
    DCHECK_NOT_NULL(node);
    DCHECK_EQ(operand.virtual_register(), GetVReg(node));
    selector()->MarkAsDefined(node);
    return operand;
  }

  UnallocatedOperand Use(Node* node, UnallocatedOperand operand) {
    DCHECK_NOT_NULL(node);
    DCHECK_EQ(operand.virtual_register(), GetVReg(node));
    selector()->MarkAsUsed(node);
    return operand;
  }

  UnallocatedOperand ToDualLocationUnallocatedOperand(
      LinkageLocation primary_location, LinkageLocation secondary_location,
      int virtual_register) {
    // We only support the primary location being a register and the secondary
    // one a slot.
    DCHECK(primary_location.IsRegister() &&
           secondary_location.IsCalleeFrameSlot());
    int reg_id = primary_location.AsRegister();
    int slot_id = secondary_location.AsCalleeFrameSlot();
    return UnallocatedOperand(reg_id, slot_id, virtual_register);
  }

  UnallocatedOperand ToUnallocatedOperand(LinkageLocation location,
                                          int virtual_register) {
    if (location.IsAnyRegister()) {
      // any machine register.
      return UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                virtual_register);
    }
    if (location.IsCallerFrameSlot()) {
      // a location on the caller frame.
      return UnallocatedOperand(UnallocatedOperand::FIXED_SLOT,
                                location.AsCallerFrameSlot(), virtual_register);
    }
    if (location.IsCalleeFrameSlot()) {
      // a spill location on this (callee) frame.
      return UnallocatedOperand(UnallocatedOperand::FIXED_SLOT,
                                location.AsCalleeFrameSlot(), virtual_register);
    }
    // a fixed register.
    if (IsFloatingPoint(location.GetType().representation())) {
      return UnallocatedOperand(UnallocatedOperand::FIXED_FP_REGISTER,
                                location.AsRegister(), virtual_register);
    }
    return UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                              location.AsRegister(), virtual_register);
  }

  InstructionSelector* selector_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_IMPL_H_

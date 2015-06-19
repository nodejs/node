// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INSTRUCTION_SELECTOR_IMPL_H_
#define V8_COMPILER_INSTRUCTION_SELECTOR_IMPL_H_

#include "src/compiler/instruction.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/linkage.h"
#include "src/compiler/schedule.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {
namespace compiler {

// Helper struct containing data about a table or lookup switch.
struct SwitchInfo {
  int32_t min_value;           // minimum value of {case_values}
  int32_t max_value;           // maximum value of {case_values}
  size_t value_range;          // |max_value - min_value| + 1
  size_t case_count;           // number of cases
  int32_t* case_values;        // actual case values, unsorted
  BasicBlock** case_branches;  // basic blocks corresponding to case values
  BasicBlock* default_branch;  // default branch target
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
                                           Register::ToAllocationIndex(reg),
                                           GetVReg(node)));
  }

  InstructionOperand DefineAsFixed(Node* node, DoubleRegister reg) {
    return Define(node,
                  UnallocatedOperand(UnallocatedOperand::FIXED_DOUBLE_REGISTER,
                                     DoubleRegister::ToAllocationIndex(reg),
                                     GetVReg(node)));
  }

  InstructionOperand DefineAsConstant(Node* node) {
    selector()->MarkAsDefined(node);
    int virtual_register = GetVReg(node);
    sequence()->AddConstant(virtual_register, ToConstant(node));
    return ConstantOperand(virtual_register);
  }

  InstructionOperand DefineAsLocation(Node* node, LinkageLocation location,
                                      MachineType type) {
    return Define(node, ToUnallocatedOperand(location, type, GetVReg(node)));
  }

  InstructionOperand Use(Node* node) {
    return Use(node, UnallocatedOperand(UnallocatedOperand::NONE,
                                        UnallocatedOperand::USED_AT_START,
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
                                        Register::ToAllocationIndex(reg),
                                        GetVReg(node)));
  }

  InstructionOperand UseFixed(Node* node, DoubleRegister reg) {
    return Use(node,
               UnallocatedOperand(UnallocatedOperand::FIXED_DOUBLE_REGISTER,
                                  DoubleRegister::ToAllocationIndex(reg),
                                  GetVReg(node)));
  }

  InstructionOperand UseImmediate(Node* node) {
    return sequence()->AddImmediate(ToConstant(node));
  }

  InstructionOperand UseLocation(Node* node, LinkageLocation location,
                                 MachineType type) {
    return Use(node, ToUnallocatedOperand(location, type, GetVReg(node)));
  }

  InstructionOperand TempRegister() {
    return UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                              UnallocatedOperand::USED_AT_START,
                              sequence()->NextVirtualRegister());
  }

  InstructionOperand TempDoubleRegister() {
    UnallocatedOperand op = UnallocatedOperand(
        UnallocatedOperand::MUST_HAVE_REGISTER,
        UnallocatedOperand::USED_AT_START, sequence()->NextVirtualRegister());
    sequence()->MarkAsRepresentation(kRepFloat64, op.virtual_register());
    return op;
  }

  InstructionOperand TempRegister(Register reg) {
    return UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                              Register::ToAllocationIndex(reg),
                              InstructionOperand::kInvalidVirtualRegister);
  }

  InstructionOperand TempImmediate(int32_t imm) {
    return sequence()->AddImmediate(Constant(imm));
  }

  InstructionOperand TempLocation(LinkageLocation location, MachineType type) {
    return ToUnallocatedOperand(location, type,
                                sequence()->NextVirtualRegister());
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
        return Constant(OpParameter<int32_t>(node));
      case IrOpcode::kInt64Constant:
        return Constant(OpParameter<int64_t>(node));
      case IrOpcode::kFloat32Constant:
        return Constant(OpParameter<float>(node));
      case IrOpcode::kFloat64Constant:
      case IrOpcode::kNumberConstant:
        return Constant(OpParameter<double>(node));
      case IrOpcode::kExternalConstant:
        return Constant(OpParameter<ExternalReference>(node));
      case IrOpcode::kHeapConstant:
        return Constant(OpParameter<Unique<HeapObject> >(node).handle());
      default:
        break;
    }
    UNREACHABLE();
    return Constant(static_cast<int32_t>(0));
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

  UnallocatedOperand ToUnallocatedOperand(LinkageLocation location,
                                          MachineType type,
                                          int virtual_register) {
    if (location.location_ == LinkageLocation::ANY_REGISTER) {
      // any machine register.
      return UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                virtual_register);
    }
    if (location.location_ < 0) {
      // a location on the caller frame.
      return UnallocatedOperand(UnallocatedOperand::FIXED_SLOT,
                                location.location_, virtual_register);
    }
    if (location.location_ > LinkageLocation::ANY_REGISTER) {
      // a spill location on this (callee) frame.
      return UnallocatedOperand(
          UnallocatedOperand::FIXED_SLOT,
          location.location_ - LinkageLocation::ANY_REGISTER - 1,
          virtual_register);
    }
    // a fixed register.
    if (RepresentationOf(type) == kRepFloat64) {
      return UnallocatedOperand(UnallocatedOperand::FIXED_DOUBLE_REGISTER,
                                location.location_, virtual_register);
    }
    return UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                              location.location_, virtual_register);
  }

  InstructionSelector* selector_;
};


// The flags continuation is a way to combine a branch or a materialization
// of a boolean value with an instruction that sets the flags register.
// The whole instruction is treated as a unit by the register allocator, and
// thus no spills or moves can be introduced between the flags-setting
// instruction and the branch or set it should be combined with.
class FlagsContinuation final {
 public:
  FlagsContinuation() : mode_(kFlags_none) {}

  // Creates a new flags continuation from the given condition and true/false
  // blocks.
  FlagsContinuation(FlagsCondition condition, BasicBlock* true_block,
                    BasicBlock* false_block)
      : mode_(kFlags_branch),
        condition_(condition),
        true_block_(true_block),
        false_block_(false_block) {
    DCHECK_NOT_NULL(true_block);
    DCHECK_NOT_NULL(false_block);
  }

  // Creates a new flags continuation from the given condition and result node.
  FlagsContinuation(FlagsCondition condition, Node* result)
      : mode_(kFlags_set), condition_(condition), result_(result) {
    DCHECK_NOT_NULL(result);
  }

  bool IsNone() const { return mode_ == kFlags_none; }
  bool IsBranch() const { return mode_ == kFlags_branch; }
  bool IsSet() const { return mode_ == kFlags_set; }
  FlagsCondition condition() const {
    DCHECK(!IsNone());
    return condition_;
  }
  Node* result() const {
    DCHECK(IsSet());
    return result_;
  }
  BasicBlock* true_block() const {
    DCHECK(IsBranch());
    return true_block_;
  }
  BasicBlock* false_block() const {
    DCHECK(IsBranch());
    return false_block_;
  }

  void Negate() {
    DCHECK(!IsNone());
    condition_ = NegateFlagsCondition(condition_);
  }

  void Commute() {
    DCHECK(!IsNone());
    switch (condition_) {
      case kEqual:
      case kNotEqual:
      case kOverflow:
      case kNotOverflow:
        return;
      case kSignedLessThan:
        condition_ = kSignedGreaterThan;
        return;
      case kSignedGreaterThanOrEqual:
        condition_ = kSignedLessThanOrEqual;
        return;
      case kSignedLessThanOrEqual:
        condition_ = kSignedGreaterThanOrEqual;
        return;
      case kSignedGreaterThan:
        condition_ = kSignedLessThan;
        return;
      case kUnsignedLessThan:
        condition_ = kUnsignedGreaterThan;
        return;
      case kUnsignedGreaterThanOrEqual:
        condition_ = kUnsignedLessThanOrEqual;
        return;
      case kUnsignedLessThanOrEqual:
        condition_ = kUnsignedGreaterThanOrEqual;
        return;
      case kUnsignedGreaterThan:
        condition_ = kUnsignedLessThan;
        return;
      case kUnorderedEqual:
      case kUnorderedNotEqual:
        return;
    }
    UNREACHABLE();
  }

  void OverwriteAndNegateIfEqual(FlagsCondition condition) {
    bool negate = condition_ == kEqual;
    condition_ = condition;
    if (negate) Negate();
  }

  // Encodes this flags continuation into the given opcode.
  InstructionCode Encode(InstructionCode opcode) {
    opcode |= FlagsModeField::encode(mode_);
    if (mode_ != kFlags_none) {
      opcode |= FlagsConditionField::encode(condition_);
    }
    return opcode;
  }

 private:
  FlagsMode mode_;
  FlagsCondition condition_;
  Node* result_;             // Only valid if mode_ == kFlags_set.
  BasicBlock* true_block_;   // Only valid if mode_ == kFlags_branch.
  BasicBlock* false_block_;  // Only valid if mode_ == kFlags_branch.
};


// An internal helper class for generating the operands to calls.
// TODO(bmeurer): Get rid of the CallBuffer business and make
// InstructionSelector::VisitCall platform independent instead.
struct CallBuffer {
  CallBuffer(Zone* zone, const CallDescriptor* descriptor,
             FrameStateDescriptor* frame_state);

  const CallDescriptor* descriptor;
  FrameStateDescriptor* frame_state_descriptor;
  NodeVector output_nodes;
  InstructionOperandVector outputs;
  InstructionOperandVector instruction_args;
  NodeVector pushed_nodes;

  size_t input_count() const { return descriptor->InputCount(); }

  size_t frame_state_count() const { return descriptor->FrameStateCount(); }

  size_t frame_state_value_count() const {
    return (frame_state_descriptor == NULL)
               ? 0
               : (frame_state_descriptor->GetTotalSize() +
                  1);  // Include deopt id.
  }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INSTRUCTION_SELECTOR_IMPL_H_

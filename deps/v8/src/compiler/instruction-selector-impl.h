// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INSTRUCTION_SELECTOR_IMPL_H_
#define V8_COMPILER_INSTRUCTION_SELECTOR_IMPL_H_

#include "src/compiler/instruction.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/linkage.h"

namespace v8 {
namespace internal {
namespace compiler {

// A helper class for the instruction selector that simplifies construction of
// Operands. This class implements a base for architecture-specific helpers.
class OperandGenerator {
 public:
  explicit OperandGenerator(InstructionSelector* selector)
      : selector_(selector) {}

  InstructionOperand* DefineAsRegister(Node* node) {
    return Define(node, new (zone())
                  UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER));
  }

  InstructionOperand* DefineAsDoubleRegister(Node* node) {
    return Define(node, new (zone())
                  UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER));
  }

  InstructionOperand* DefineSameAsFirst(Node* result) {
    return Define(result, new (zone())
                  UnallocatedOperand(UnallocatedOperand::SAME_AS_FIRST_INPUT));
  }

  InstructionOperand* DefineAsFixed(Node* node, Register reg) {
    return Define(node, new (zone())
                  UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                                     Register::ToAllocationIndex(reg)));
  }

  InstructionOperand* DefineAsFixedDouble(Node* node, DoubleRegister reg) {
    return Define(node, new (zone())
                  UnallocatedOperand(UnallocatedOperand::FIXED_DOUBLE_REGISTER,
                                     DoubleRegister::ToAllocationIndex(reg)));
  }

  InstructionOperand* DefineAsConstant(Node* node) {
    selector()->MarkAsDefined(node);
    sequence()->AddConstant(node->id(), ToConstant(node));
    return ConstantOperand::Create(node->id(), zone());
  }

  InstructionOperand* DefineAsLocation(Node* node, LinkageLocation location) {
    return Define(node, ToUnallocatedOperand(location));
  }

  InstructionOperand* Use(Node* node) {
    return Use(node,
               new (zone()) UnallocatedOperand(
                   UnallocatedOperand::ANY, UnallocatedOperand::USED_AT_START));
  }

  InstructionOperand* UseRegister(Node* node) {
    return Use(node, new (zone())
               UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                  UnallocatedOperand::USED_AT_START));
  }

  InstructionOperand* UseDoubleRegister(Node* node) {
    return Use(node, new (zone())
               UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                  UnallocatedOperand::USED_AT_START));
  }

  // Use register or operand for the node. If a register is chosen, it won't
  // alias any temporary or output registers.
  InstructionOperand* UseUnique(Node* node) {
    return Use(node, new (zone()) UnallocatedOperand(UnallocatedOperand::ANY));
  }

  // Use a unique register for the node that does not alias any temporary or
  // output registers.
  InstructionOperand* UseUniqueRegister(Node* node) {
    return Use(node, new (zone())
               UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER));
  }

  // Use a unique double register for the node that does not alias any temporary
  // or output double registers.
  InstructionOperand* UseUniqueDoubleRegister(Node* node) {
    return Use(node, new (zone())
               UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER));
  }

  InstructionOperand* UseFixed(Node* node, Register reg) {
    return Use(node, new (zone())
               UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                                  Register::ToAllocationIndex(reg)));
  }

  InstructionOperand* UseFixedDouble(Node* node, DoubleRegister reg) {
    return Use(node, new (zone())
               UnallocatedOperand(UnallocatedOperand::FIXED_DOUBLE_REGISTER,
                                  DoubleRegister::ToAllocationIndex(reg)));
  }

  InstructionOperand* UseImmediate(Node* node) {
    int index = sequence()->AddImmediate(ToConstant(node));
    return ImmediateOperand::Create(index, zone());
  }

  InstructionOperand* UseLocation(Node* node, LinkageLocation location) {
    return Use(node, ToUnallocatedOperand(location));
  }

  InstructionOperand* TempRegister() {
    UnallocatedOperand* op =
        new (zone()) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                        UnallocatedOperand::USED_AT_START);
    op->set_virtual_register(sequence()->NextVirtualRegister());
    return op;
  }

  InstructionOperand* TempDoubleRegister() {
    UnallocatedOperand* op =
        new (zone()) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER,
                                        UnallocatedOperand::USED_AT_START);
    op->set_virtual_register(sequence()->NextVirtualRegister());
    sequence()->MarkAsDouble(op->virtual_register());
    return op;
  }

  InstructionOperand* TempRegister(Register reg) {
    return new (zone()) UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                                           Register::ToAllocationIndex(reg));
  }

  InstructionOperand* TempImmediate(int32_t imm) {
    int index = sequence()->AddImmediate(Constant(imm));
    return ImmediateOperand::Create(index, zone());
  }

  InstructionOperand* Label(BasicBlock* block) {
    // TODO(bmeurer): We misuse ImmediateOperand here.
    return TempImmediate(block->id());
  }

 protected:
  Graph* graph() const { return selector()->graph(); }
  InstructionSelector* selector() const { return selector_; }
  InstructionSequence* sequence() const { return selector()->sequence(); }
  Isolate* isolate() const { return zone()->isolate(); }
  Zone* zone() const { return selector()->instruction_zone(); }

 private:
  static Constant ToConstant(const Node* node) {
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
        return Constant(ValueOf<int32_t>(node->op()));
      case IrOpcode::kInt64Constant:
        return Constant(ValueOf<int64_t>(node->op()));
      case IrOpcode::kNumberConstant:
      case IrOpcode::kFloat64Constant:
        return Constant(ValueOf<double>(node->op()));
      case IrOpcode::kExternalConstant:
        return Constant(ValueOf<ExternalReference>(node->op()));
      case IrOpcode::kHeapConstant:
        return Constant(ValueOf<Handle<HeapObject> >(node->op()));
      default:
        break;
    }
    UNREACHABLE();
    return Constant(static_cast<int32_t>(0));
  }

  UnallocatedOperand* Define(Node* node, UnallocatedOperand* operand) {
    DCHECK_NOT_NULL(node);
    DCHECK_NOT_NULL(operand);
    operand->set_virtual_register(node->id());
    selector()->MarkAsDefined(node);
    return operand;
  }

  UnallocatedOperand* Use(Node* node, UnallocatedOperand* operand) {
    DCHECK_NOT_NULL(node);
    DCHECK_NOT_NULL(operand);
    operand->set_virtual_register(node->id());
    selector()->MarkAsUsed(node);
    return operand;
  }

  UnallocatedOperand* ToUnallocatedOperand(LinkageLocation location) {
    if (location.location_ == LinkageLocation::ANY_REGISTER) {
      return new (zone())
          UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER);
    }
    if (location.location_ < 0) {
      return new (zone()) UnallocatedOperand(UnallocatedOperand::FIXED_SLOT,
                                             location.location_);
    }
    if (location.rep_ == kMachineFloat64) {
      return new (zone()) UnallocatedOperand(
          UnallocatedOperand::FIXED_DOUBLE_REGISTER, location.location_);
    }
    return new (zone()) UnallocatedOperand(UnallocatedOperand::FIXED_REGISTER,
                                           location.location_);
  }

  InstructionSelector* selector_;
};


// The flags continuation is a way to combine a branch or a materialization
// of a boolean value with an instruction that sets the flags register.
// The whole instruction is treated as a unit by the register allocator, and
// thus no spills or moves can be introduced between the flags-setting
// instruction and the branch or set it should be combined with.
class FlagsContinuation V8_FINAL {
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
    condition_ = static_cast<FlagsCondition>(condition_ ^ 1);
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
      case kUnorderedLessThan:
        condition_ = kUnorderedGreaterThan;
        return;
      case kUnorderedGreaterThanOrEqual:
        condition_ = kUnorderedLessThanOrEqual;
        return;
      case kUnorderedLessThanOrEqual:
        condition_ = kUnorderedGreaterThanOrEqual;
        return;
      case kUnorderedGreaterThan:
        condition_ = kUnorderedLessThan;
        return;
    }
    UNREACHABLE();
  }

  void OverwriteAndNegateIfEqual(FlagsCondition condition) {
    bool negate = condition_ == kEqual;
    condition_ = condition;
    if (negate) Negate();
  }

  void SwapBlocks() { std::swap(true_block_, false_block_); }

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
  CallBuffer(Zone* zone, CallDescriptor* descriptor);

  int output_count;
  CallDescriptor* descriptor;
  Node** output_nodes;
  InstructionOperand** outputs;
  InstructionOperand** fixed_and_control_args;
  int fixed_count;
  Node** pushed_nodes;
  int pushed_count;

  int input_count() { return descriptor->InputCount(); }

  int control_count() { return descriptor->CanLazilyDeoptimize() ? 2 : 0; }

  int fixed_and_control_count() { return fixed_count + control_count(); }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INSTRUCTION_SELECTOR_IMPL_H_

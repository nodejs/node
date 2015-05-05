// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGISTER_ALLOCATOR_VERIFIER_H_
#define V8_REGISTER_ALLOCATOR_VERIFIER_H_

#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class InstructionOperand;
class InstructionSequence;

class RegisterAllocatorVerifier FINAL : public ZoneObject {
 public:
  RegisterAllocatorVerifier(Zone* zone, const RegisterConfiguration* config,
                            const InstructionSequence* sequence);

  void VerifyAssignment();
  void VerifyGapMoves();

 private:
  enum ConstraintType {
    kConstant,
    kImmediate,
    kRegister,
    kFixedRegister,
    kDoubleRegister,
    kFixedDoubleRegister,
    kSlot,
    kDoubleSlot,
    kFixedSlot,
    kNone,
    kNoneDouble,
    kSameAsFirst
  };

  struct OperandConstraint {
    ConstraintType type_;
    int value_;  // subkind index when relevant
    int virtual_register_;
  };

  struct InstructionConstraint {
    const Instruction* instruction_;
    size_t operand_constaints_size_;
    OperandConstraint* operand_constraints_;
  };

  class BlockMaps;

  typedef ZoneVector<InstructionConstraint> Constraints;

  Zone* zone() const { return zone_; }
  const RegisterConfiguration* config() { return config_; }
  const InstructionSequence* sequence() const { return sequence_; }
  Constraints* constraints() { return &constraints_; }

  static void VerifyInput(const OperandConstraint& constraint);
  static void VerifyTemp(const OperandConstraint& constraint);
  static void VerifyOutput(const OperandConstraint& constraint);

  void BuildConstraint(const InstructionOperand* op,
                       OperandConstraint* constraint);
  void CheckConstraint(const InstructionOperand* op,
                       const OperandConstraint* constraint);

  void VerifyGapMoves(BlockMaps* outgoing_mappings, bool initial_pass);

  Zone* const zone_;
  const RegisterConfiguration* config_;
  const InstructionSequence* const sequence_;
  Constraints constraints_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAllocatorVerifier);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif

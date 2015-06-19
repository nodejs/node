// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INSTRUCTION_H_
#define V8_COMPILER_INSTRUCTION_H_

#include <deque>
#include <iosfwd>
#include <map>
#include <set>

#include "src/compiler/common-operator.h"
#include "src/compiler/frame.h"
#include "src/compiler/instruction-codes.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/register-configuration.h"
#include "src/compiler/source-position.h"
#include "src/zone-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {

class Schedule;

class InstructionOperand {
 public:
  static const int kInvalidVirtualRegister = -1;

  // TODO(dcarney): recover bit. INVALID can be represented as UNALLOCATED with
  // kInvalidVirtualRegister and some DCHECKS.
  enum Kind { INVALID, UNALLOCATED, CONSTANT, IMMEDIATE, ALLOCATED };

  InstructionOperand() : InstructionOperand(INVALID) {}

  Kind kind() const { return KindField::decode(value_); }

#define INSTRUCTION_OPERAND_PREDICATE(name, type) \
  bool Is##name() const { return kind() == type; }
  INSTRUCTION_OPERAND_PREDICATE(Invalid, INVALID)
  INSTRUCTION_OPERAND_PREDICATE(Unallocated, UNALLOCATED)
  INSTRUCTION_OPERAND_PREDICATE(Constant, CONSTANT)
  INSTRUCTION_OPERAND_PREDICATE(Immediate, IMMEDIATE)
  INSTRUCTION_OPERAND_PREDICATE(Allocated, ALLOCATED)
#undef INSTRUCTION_OPERAND_PREDICATE

  inline bool IsRegister() const;
  inline bool IsDoubleRegister() const;
  inline bool IsStackSlot() const;
  inline bool IsDoubleStackSlot() const;

  template <typename SubKindOperand>
  static SubKindOperand* New(Zone* zone, const SubKindOperand& op) {
    void* buffer = zone->New(sizeof(op));
    return new (buffer) SubKindOperand(op);
  }

  static void ReplaceWith(InstructionOperand* dest,
                          const InstructionOperand* src) {
    *dest = *src;
  }

  bool Equals(const InstructionOperand& that) const {
    return this->value_ == that.value_;
  }

  bool Compare(const InstructionOperand& that) const {
    return this->value_ < that.value_;
  }

  bool EqualsModuloType(const InstructionOperand& that) const {
    return this->GetValueModuloType() == that.GetValueModuloType();
  }

  bool CompareModuloType(const InstructionOperand& that) const {
    return this->GetValueModuloType() < that.GetValueModuloType();
  }

 protected:
  explicit InstructionOperand(Kind kind) : value_(KindField::encode(kind)) {}

  inline uint64_t GetValueModuloType() const;

  class KindField : public BitField64<Kind, 0, 3> {};

  uint64_t value_;
};


struct PrintableInstructionOperand {
  const RegisterConfiguration* register_configuration_;
  InstructionOperand op_;
};


std::ostream& operator<<(std::ostream& os,
                         const PrintableInstructionOperand& op);


#define INSTRUCTION_OPERAND_CASTS(OperandType, OperandKind)      \
                                                                 \
  static OperandType* cast(InstructionOperand* op) {             \
    DCHECK_EQ(OperandKind, op->kind());                          \
    return static_cast<OperandType*>(op);                        \
  }                                                              \
                                                                 \
  static const OperandType* cast(const InstructionOperand* op) { \
    DCHECK_EQ(OperandKind, op->kind());                          \
    return static_cast<const OperandType*>(op);                  \
  }                                                              \
                                                                 \
  static OperandType cast(const InstructionOperand& op) {        \
    DCHECK_EQ(OperandKind, op.kind());                           \
    return *static_cast<const OperandType*>(&op);                \
  }

class UnallocatedOperand : public InstructionOperand {
 public:
  enum BasicPolicy { FIXED_SLOT, EXTENDED_POLICY };

  enum ExtendedPolicy {
    NONE,
    ANY,
    FIXED_REGISTER,
    FIXED_DOUBLE_REGISTER,
    MUST_HAVE_REGISTER,
    MUST_HAVE_SLOT,
    SAME_AS_FIRST_INPUT
  };

  // Lifetime of operand inside the instruction.
  enum Lifetime {
    // USED_AT_START operand is guaranteed to be live only at
    // instruction start. Register allocator is free to assign the same register
    // to some other operand used inside instruction (i.e. temporary or
    // output).
    USED_AT_START,

    // USED_AT_END operand is treated as live until the end of
    // instruction. This means that register allocator will not reuse it's
    // register for any other operand inside instruction.
    USED_AT_END
  };

  UnallocatedOperand(ExtendedPolicy policy, int virtual_register)
      : UnallocatedOperand(virtual_register) {
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(USED_AT_END);
  }

  UnallocatedOperand(BasicPolicy policy, int index, int virtual_register)
      : UnallocatedOperand(virtual_register) {
    DCHECK(policy == FIXED_SLOT);
    value_ |= BasicPolicyField::encode(policy);
    value_ |= static_cast<int64_t>(index) << FixedSlotIndexField::kShift;
    DCHECK(this->fixed_slot_index() == index);
  }

  UnallocatedOperand(ExtendedPolicy policy, int index, int virtual_register)
      : UnallocatedOperand(virtual_register) {
    DCHECK(policy == FIXED_REGISTER || policy == FIXED_DOUBLE_REGISTER);
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(USED_AT_END);
    value_ |= FixedRegisterField::encode(index);
  }

  UnallocatedOperand(ExtendedPolicy policy, Lifetime lifetime,
                     int virtual_register)
      : UnallocatedOperand(virtual_register) {
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(lifetime);
  }

  // Predicates for the operand policy.
  bool HasAnyPolicy() const {
    return basic_policy() == EXTENDED_POLICY && extended_policy() == ANY;
  }
  bool HasFixedPolicy() const {
    return basic_policy() == FIXED_SLOT ||
           extended_policy() == FIXED_REGISTER ||
           extended_policy() == FIXED_DOUBLE_REGISTER;
  }
  bool HasRegisterPolicy() const {
    return basic_policy() == EXTENDED_POLICY &&
           extended_policy() == MUST_HAVE_REGISTER;
  }
  bool HasSlotPolicy() const {
    return basic_policy() == EXTENDED_POLICY &&
           extended_policy() == MUST_HAVE_SLOT;
  }
  bool HasSameAsInputPolicy() const {
    return basic_policy() == EXTENDED_POLICY &&
           extended_policy() == SAME_AS_FIRST_INPUT;
  }
  bool HasFixedSlotPolicy() const { return basic_policy() == FIXED_SLOT; }
  bool HasFixedRegisterPolicy() const {
    return basic_policy() == EXTENDED_POLICY &&
           extended_policy() == FIXED_REGISTER;
  }
  bool HasFixedDoubleRegisterPolicy() const {
    return basic_policy() == EXTENDED_POLICY &&
           extended_policy() == FIXED_DOUBLE_REGISTER;
  }

  // [basic_policy]: Distinguish between FIXED_SLOT and all other policies.
  BasicPolicy basic_policy() const {
    DCHECK_EQ(UNALLOCATED, kind());
    return BasicPolicyField::decode(value_);
  }

  // [extended_policy]: Only for non-FIXED_SLOT. The finer-grained policy.
  ExtendedPolicy extended_policy() const {
    DCHECK(basic_policy() == EXTENDED_POLICY);
    return ExtendedPolicyField::decode(value_);
  }

  // [fixed_slot_index]: Only for FIXED_SLOT.
  int fixed_slot_index() const {
    DCHECK(HasFixedSlotPolicy());
    return static_cast<int>(static_cast<int64_t>(value_) >>
                            FixedSlotIndexField::kShift);
  }

  // [fixed_register_index]: Only for FIXED_REGISTER or FIXED_DOUBLE_REGISTER.
  int fixed_register_index() const {
    DCHECK(HasFixedRegisterPolicy() || HasFixedDoubleRegisterPolicy());
    return FixedRegisterField::decode(value_);
  }

  // [virtual_register]: The virtual register ID for this operand.
  int32_t virtual_register() const {
    DCHECK_EQ(UNALLOCATED, kind());
    return static_cast<int32_t>(VirtualRegisterField::decode(value_));
  }

  // TODO(dcarney): remove this.
  void set_virtual_register(int32_t id) {
    DCHECK_EQ(UNALLOCATED, kind());
    value_ = VirtualRegisterField::update(value_, static_cast<uint32_t>(id));
  }

  // [lifetime]: Only for non-FIXED_SLOT.
  bool IsUsedAtStart() const {
    DCHECK(basic_policy() == EXTENDED_POLICY);
    return LifetimeField::decode(value_) == USED_AT_START;
  }

  INSTRUCTION_OPERAND_CASTS(UnallocatedOperand, UNALLOCATED);

  // The encoding used for UnallocatedOperand operands depends on the policy
  // that is
  // stored within the operand. The FIXED_SLOT policy uses a compact encoding
  // because it accommodates a larger pay-load.
  //
  // For FIXED_SLOT policy:
  //     +------------------------------------------------+
  //     |      slot_index   | 0 | virtual_register | 001 |
  //     +------------------------------------------------+
  //
  // For all other (extended) policies:
  //     +-----------------------------------------------------+
  //     |  reg_index  | L | PPP |  1 | virtual_register | 001 |
  //     +-----------------------------------------------------+
  //     L ... Lifetime
  //     P ... Policy
  //
  // The slot index is a signed value which requires us to decode it manually
  // instead of using the BitField utility class.

  STATIC_ASSERT(KindField::kSize == 3);

  class VirtualRegisterField : public BitField64<uint32_t, 3, 32> {};

  // BitFields for all unallocated operands.
  class BasicPolicyField : public BitField64<BasicPolicy, 35, 1> {};

  // BitFields specific to BasicPolicy::FIXED_SLOT.
  class FixedSlotIndexField : public BitField64<int, 36, 28> {};

  // BitFields specific to BasicPolicy::EXTENDED_POLICY.
  class ExtendedPolicyField : public BitField64<ExtendedPolicy, 36, 3> {};
  class LifetimeField : public BitField64<Lifetime, 39, 1> {};
  class FixedRegisterField : public BitField64<int, 40, 6> {};

 private:
  explicit UnallocatedOperand(int virtual_register)
      : InstructionOperand(UNALLOCATED) {
    value_ |=
        VirtualRegisterField::encode(static_cast<uint32_t>(virtual_register));
  }
};


class ConstantOperand : public InstructionOperand {
 public:
  explicit ConstantOperand(int virtual_register)
      : InstructionOperand(CONSTANT) {
    value_ |=
        VirtualRegisterField::encode(static_cast<uint32_t>(virtual_register));
  }

  int32_t virtual_register() const {
    return static_cast<int32_t>(VirtualRegisterField::decode(value_));
  }

  static ConstantOperand* New(Zone* zone, int virtual_register) {
    return InstructionOperand::New(zone, ConstantOperand(virtual_register));
  }

  INSTRUCTION_OPERAND_CASTS(ConstantOperand, CONSTANT);

  STATIC_ASSERT(KindField::kSize == 3);
  class VirtualRegisterField : public BitField64<uint32_t, 3, 32> {};
};


class ImmediateOperand : public InstructionOperand {
 public:
  enum ImmediateType { INLINE, INDEXED };

  explicit ImmediateOperand(ImmediateType type, int32_t value)
      : InstructionOperand(IMMEDIATE) {
    value_ |= TypeField::encode(type);
    value_ |= static_cast<int64_t>(value) << ValueField::kShift;
  }

  ImmediateType type() const { return TypeField::decode(value_); }

  int32_t inline_value() const {
    DCHECK_EQ(INLINE, type());
    return static_cast<int64_t>(value_) >> ValueField::kShift;
  }

  int32_t indexed_value() const {
    DCHECK_EQ(INDEXED, type());
    return static_cast<int64_t>(value_) >> ValueField::kShift;
  }

  static ImmediateOperand* New(Zone* zone, ImmediateType type, int32_t value) {
    return InstructionOperand::New(zone, ImmediateOperand(type, value));
  }

  INSTRUCTION_OPERAND_CASTS(ImmediateOperand, IMMEDIATE);

  STATIC_ASSERT(KindField::kSize == 3);
  class TypeField : public BitField64<ImmediateType, 3, 1> {};
  class ValueField : public BitField64<int32_t, 32, 32> {};
};


class AllocatedOperand : public InstructionOperand {
 public:
  // TODO(dcarney): machine_type makes this now redundant. Just need to know is
  // the operand is a slot or a register.
  enum AllocatedKind {
    STACK_SLOT,
    DOUBLE_STACK_SLOT,
    REGISTER,
    DOUBLE_REGISTER
  };

  AllocatedOperand(AllocatedKind kind, MachineType machine_type, int index)
      : InstructionOperand(ALLOCATED) {
    DCHECK_IMPLIES(kind == REGISTER || kind == DOUBLE_REGISTER, index >= 0);
    DCHECK(IsSupportedMachineType(machine_type));
    value_ |= AllocatedKindField::encode(kind);
    value_ |= MachineTypeField::encode(machine_type);
    value_ |= static_cast<int64_t>(index) << IndexField::kShift;
  }

  int index() const {
    return static_cast<int64_t>(value_) >> IndexField::kShift;
  }

  AllocatedKind allocated_kind() const {
    return AllocatedKindField::decode(value_);
  }

  MachineType machine_type() const { return MachineTypeField::decode(value_); }

  static AllocatedOperand* New(Zone* zone, AllocatedKind kind,
                               MachineType machine_type, int index) {
    return InstructionOperand::New(zone,
                                   AllocatedOperand(kind, machine_type, index));
  }

  static bool IsSupportedMachineType(MachineType machine_type) {
    if (RepresentationOf(machine_type) != machine_type) return false;
    switch (machine_type) {
      case kRepWord32:
      case kRepWord64:
      case kRepFloat32:
      case kRepFloat64:
      case kRepTagged:
        return true;
      default:
        return false;
    }
  }

  INSTRUCTION_OPERAND_CASTS(AllocatedOperand, ALLOCATED);

  STATIC_ASSERT(KindField::kSize == 3);
  class AllocatedKindField : public BitField64<AllocatedKind, 3, 2> {};
  class MachineTypeField : public BitField64<MachineType, 5, 16> {};
  class IndexField : public BitField64<int32_t, 35, 29> {};
};


#undef INSTRUCTION_OPERAND_CASTS


#define ALLOCATED_OPERAND_LIST(V)       \
  V(StackSlot, STACK_SLOT)              \
  V(DoubleStackSlot, DOUBLE_STACK_SLOT) \
  V(Register, REGISTER)                 \
  V(DoubleRegister, DOUBLE_REGISTER)


#define ALLOCATED_OPERAND_IS(SubKind, kOperandKind)          \
  bool InstructionOperand::Is##SubKind() const {             \
    return IsAllocated() &&                                  \
           AllocatedOperand::cast(this)->allocated_kind() == \
               AllocatedOperand::kOperandKind;               \
  }
ALLOCATED_OPERAND_LIST(ALLOCATED_OPERAND_IS)
#undef ALLOCATED_OPERAND_IS


// TODO(dcarney): these subkinds are now pretty useless, nuke.
#define ALLOCATED_OPERAND_CLASS(SubKind, kOperandKind)                       \
  class SubKind##Operand final : public AllocatedOperand {                   \
   public:                                                                   \
    explicit SubKind##Operand(MachineType machine_type, int index)           \
        : AllocatedOperand(kOperandKind, machine_type, index) {}             \
                                                                             \
    static SubKind##Operand* New(Zone* zone, MachineType machine_type,       \
                                 int index) {                                \
      return InstructionOperand::New(zone,                                   \
                                     SubKind##Operand(machine_type, index)); \
    }                                                                        \
                                                                             \
    static SubKind##Operand* cast(InstructionOperand* op) {                  \
      DCHECK_EQ(kOperandKind, AllocatedOperand::cast(op)->allocated_kind()); \
      return reinterpret_cast<SubKind##Operand*>(op);                        \
    }                                                                        \
                                                                             \
    static const SubKind##Operand* cast(const InstructionOperand* op) {      \
      DCHECK_EQ(kOperandKind, AllocatedOperand::cast(op)->allocated_kind()); \
      return reinterpret_cast<const SubKind##Operand*>(op);                  \
    }                                                                        \
                                                                             \
    static SubKind##Operand cast(const InstructionOperand& op) {             \
      DCHECK_EQ(kOperandKind, AllocatedOperand::cast(op).allocated_kind());  \
      return *static_cast<const SubKind##Operand*>(&op);                     \
    }                                                                        \
  };
ALLOCATED_OPERAND_LIST(ALLOCATED_OPERAND_CLASS)
#undef ALLOCATED_OPERAND_CLASS


uint64_t InstructionOperand::GetValueModuloType() const {
  if (IsAllocated()) {
    // TODO(dcarney): put machine type last and mask.
    return AllocatedOperand::MachineTypeField::update(this->value_, kMachNone);
  }
  return this->value_;
}


// Required for maps that don't care about machine type.
struct CompareOperandModuloType {
  bool operator()(const InstructionOperand& a,
                  const InstructionOperand& b) const {
    return a.CompareModuloType(b);
  }
};


class MoveOperands final : public ZoneObject {
 public:
  MoveOperands(const InstructionOperand& source,
               const InstructionOperand& destination)
      : source_(source), destination_(destination) {
    DCHECK(!source.IsInvalid() && !destination.IsInvalid());
  }

  const InstructionOperand& source() const { return source_; }
  InstructionOperand& source() { return source_; }
  void set_source(const InstructionOperand& operand) { source_ = operand; }

  const InstructionOperand& destination() const { return destination_; }
  InstructionOperand& destination() { return destination_; }
  void set_destination(const InstructionOperand& operand) {
    destination_ = operand;
  }

  // The gap resolver marks moves as "in-progress" by clearing the
  // destination (but not the source).
  bool IsPending() const {
    return destination_.IsInvalid() && !source_.IsInvalid();
  }
  void SetPending() { destination_ = InstructionOperand(); }

  // True if this move a move into the given destination operand.
  bool Blocks(const InstructionOperand& operand) const {
    return !IsEliminated() && source().EqualsModuloType(operand);
  }

  // A move is redundant if it's been eliminated or if its source and
  // destination are the same.
  bool IsRedundant() const {
    DCHECK_IMPLIES(!destination_.IsInvalid(), !destination_.IsConstant());
    return IsEliminated() || source_.EqualsModuloType(destination_);
  }

  // We clear both operands to indicate move that's been eliminated.
  void Eliminate() { source_ = destination_ = InstructionOperand(); }
  bool IsEliminated() const {
    DCHECK_IMPLIES(source_.IsInvalid(), destination_.IsInvalid());
    return source_.IsInvalid();
  }

 private:
  InstructionOperand source_;
  InstructionOperand destination_;

  DISALLOW_COPY_AND_ASSIGN(MoveOperands);
};


struct PrintableMoveOperands {
  const RegisterConfiguration* register_configuration_;
  const MoveOperands* move_operands_;
};


std::ostream& operator<<(std::ostream& os, const PrintableMoveOperands& mo);


class ParallelMove final : public ZoneVector<MoveOperands*>, public ZoneObject {
 public:
  explicit ParallelMove(Zone* zone) : ZoneVector<MoveOperands*>(zone) {
    reserve(4);
  }

  MoveOperands* AddMove(const InstructionOperand& from,
                        const InstructionOperand& to) {
    auto zone = get_allocator().zone();
    auto move = new (zone) MoveOperands(from, to);
    push_back(move);
    return move;
  }

  bool IsRedundant() const;

  // Prepare this ParallelMove to insert move as if it happened in a subsequent
  // ParallelMove.  move->source() may be changed.  The MoveOperand returned
  // must be Eliminated.
  MoveOperands* PrepareInsertAfter(MoveOperands* move) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParallelMove);
};


struct PrintableParallelMove {
  const RegisterConfiguration* register_configuration_;
  const ParallelMove* parallel_move_;
};


std::ostream& operator<<(std::ostream& os, const PrintableParallelMove& pm);


class ReferenceMap final : public ZoneObject {
 public:
  explicit ReferenceMap(Zone* zone)
      : reference_operands_(8, zone), instruction_position_(-1) {}

  const ZoneVector<InstructionOperand>& reference_operands() const {
    return reference_operands_;
  }
  int instruction_position() const { return instruction_position_; }

  void set_instruction_position(int pos) {
    DCHECK(instruction_position_ == -1);
    instruction_position_ = pos;
  }

  void RecordReference(const AllocatedOperand& op);

 private:
  friend std::ostream& operator<<(std::ostream& os, const ReferenceMap& pm);

  ZoneVector<InstructionOperand> reference_operands_;
  int instruction_position_;
};

std::ostream& operator<<(std::ostream& os, const ReferenceMap& pm);

class Instruction final {
 public:
  size_t OutputCount() const { return OutputCountField::decode(bit_field_); }
  const InstructionOperand* OutputAt(size_t i) const {
    DCHECK(i < OutputCount());
    return &operands_[i];
  }
  InstructionOperand* OutputAt(size_t i) {
    DCHECK(i < OutputCount());
    return &operands_[i];
  }

  bool HasOutput() const { return OutputCount() == 1; }
  const InstructionOperand* Output() const { return OutputAt(0); }
  InstructionOperand* Output() { return OutputAt(0); }

  size_t InputCount() const { return InputCountField::decode(bit_field_); }
  const InstructionOperand* InputAt(size_t i) const {
    DCHECK(i < InputCount());
    return &operands_[OutputCount() + i];
  }
  InstructionOperand* InputAt(size_t i) {
    DCHECK(i < InputCount());
    return &operands_[OutputCount() + i];
  }

  size_t TempCount() const { return TempCountField::decode(bit_field_); }
  const InstructionOperand* TempAt(size_t i) const {
    DCHECK(i < TempCount());
    return &operands_[OutputCount() + InputCount() + i];
  }
  InstructionOperand* TempAt(size_t i) {
    DCHECK(i < TempCount());
    return &operands_[OutputCount() + InputCount() + i];
  }

  InstructionCode opcode() const { return opcode_; }
  ArchOpcode arch_opcode() const { return ArchOpcodeField::decode(opcode()); }
  AddressingMode addressing_mode() const {
    return AddressingModeField::decode(opcode());
  }
  FlagsMode flags_mode() const { return FlagsModeField::decode(opcode()); }
  FlagsCondition flags_condition() const {
    return FlagsConditionField::decode(opcode());
  }

  // TODO(titzer): make call into a flags.
  static Instruction* New(Zone* zone, InstructionCode opcode) {
    return New(zone, opcode, 0, NULL, 0, NULL, 0, NULL);
  }

  static Instruction* New(Zone* zone, InstructionCode opcode,
                          size_t output_count, InstructionOperand* outputs,
                          size_t input_count, InstructionOperand* inputs,
                          size_t temp_count, InstructionOperand* temps) {
    DCHECK(opcode >= 0);
    DCHECK(output_count == 0 || outputs != NULL);
    DCHECK(input_count == 0 || inputs != NULL);
    DCHECK(temp_count == 0 || temps != NULL);
    size_t total_extra_ops = output_count + input_count + temp_count;
    if (total_extra_ops != 0) total_extra_ops--;
    int size = static_cast<int>(
        RoundUp(sizeof(Instruction), sizeof(InstructionOperand)) +
        total_extra_ops * sizeof(InstructionOperand));
    return new (zone->New(size)) Instruction(
        opcode, output_count, outputs, input_count, inputs, temp_count, temps);
  }

  Instruction* MarkAsCall() {
    bit_field_ = IsCallField::update(bit_field_, true);
    return this;
  }
  bool IsCall() const { return IsCallField::decode(bit_field_); }
  bool NeedsReferenceMap() const { return IsCall(); }
  bool HasReferenceMap() const { return reference_map_ != NULL; }

  bool ClobbersRegisters() const { return IsCall(); }
  bool ClobbersTemps() const { return IsCall(); }
  bool ClobbersDoubleRegisters() const { return IsCall(); }
  ReferenceMap* reference_map() const { return reference_map_; }

  void set_reference_map(ReferenceMap* map) {
    DCHECK(NeedsReferenceMap());
    DCHECK(!reference_map_);
    reference_map_ = map;
  }

  void OverwriteWithNop() {
    opcode_ = ArchOpcodeField::encode(kArchNop);
    bit_field_ = 0;
    reference_map_ = NULL;
  }

  bool IsNop() const {
    return arch_opcode() == kArchNop && InputCount() == 0 &&
           OutputCount() == 0 && TempCount() == 0;
  }

  enum GapPosition {
    START,
    END,
    FIRST_GAP_POSITION = START,
    LAST_GAP_POSITION = END
  };

  ParallelMove* GetOrCreateParallelMove(GapPosition pos, Zone* zone) {
    if (parallel_moves_[pos] == nullptr) {
      parallel_moves_[pos] = new (zone) ParallelMove(zone);
    }
    return parallel_moves_[pos];
  }

  ParallelMove* GetParallelMove(GapPosition pos) {
    return parallel_moves_[pos];
  }

  const ParallelMove* GetParallelMove(GapPosition pos) const {
    return parallel_moves_[pos];
  }

  bool AreMovesRedundant() const;

  ParallelMove* const* parallel_moves() const { return &parallel_moves_[0]; }
  ParallelMove** parallel_moves() { return &parallel_moves_[0]; }

 private:
  explicit Instruction(InstructionCode opcode);

  Instruction(InstructionCode opcode, size_t output_count,
              InstructionOperand* outputs, size_t input_count,
              InstructionOperand* inputs, size_t temp_count,
              InstructionOperand* temps);

  typedef BitField<size_t, 0, 8> OutputCountField;
  typedef BitField<size_t, 8, 16> InputCountField;
  typedef BitField<size_t, 24, 6> TempCountField;
  typedef BitField<bool, 30, 1> IsCallField;

  InstructionCode opcode_;
  uint32_t bit_field_;
  ParallelMove* parallel_moves_[2];
  ReferenceMap* reference_map_;
  InstructionOperand operands_[1];

  DISALLOW_COPY_AND_ASSIGN(Instruction);
};


struct PrintableInstruction {
  const RegisterConfiguration* register_configuration_;
  const Instruction* instr_;
};
std::ostream& operator<<(std::ostream& os, const PrintableInstruction& instr);


class RpoNumber final {
 public:
  static const int kInvalidRpoNumber = -1;
  int ToInt() const {
    DCHECK(IsValid());
    return index_;
  }
  size_t ToSize() const {
    DCHECK(IsValid());
    return static_cast<size_t>(index_);
  }
  bool IsValid() const { return index_ >= 0; }
  static RpoNumber FromInt(int index) { return RpoNumber(index); }
  static RpoNumber Invalid() { return RpoNumber(kInvalidRpoNumber); }

  bool IsNext(const RpoNumber other) const {
    DCHECK(IsValid());
    return other.index_ == this->index_ + 1;
  }

  bool operator==(RpoNumber other) const {
    return this->index_ == other.index_;
  }

 private:
  explicit RpoNumber(int32_t index) : index_(index) {}
  int32_t index_;
};


std::ostream& operator<<(std::ostream&, const RpoNumber&);


class Constant final {
 public:
  enum Type {
    kInt32,
    kInt64,
    kFloat32,
    kFloat64,
    kExternalReference,
    kHeapObject,
    kRpoNumber
  };

  explicit Constant(int32_t v);
  explicit Constant(int64_t v) : type_(kInt64), value_(v) {}
  explicit Constant(float v) : type_(kFloat32), value_(bit_cast<int32_t>(v)) {}
  explicit Constant(double v) : type_(kFloat64), value_(bit_cast<int64_t>(v)) {}
  explicit Constant(ExternalReference ref)
      : type_(kExternalReference), value_(bit_cast<intptr_t>(ref)) {}
  explicit Constant(Handle<HeapObject> obj)
      : type_(kHeapObject), value_(bit_cast<intptr_t>(obj)) {}
  explicit Constant(RpoNumber rpo) : type_(kRpoNumber), value_(rpo.ToInt()) {}

  Type type() const { return type_; }

  int32_t ToInt32() const {
    DCHECK(type() == kInt32 || type() == kInt64);
    const int32_t value = static_cast<int32_t>(value_);
    DCHECK_EQ(value_, static_cast<int64_t>(value));
    return value;
  }

  int64_t ToInt64() const {
    if (type() == kInt32) return ToInt32();
    DCHECK_EQ(kInt64, type());
    return value_;
  }

  float ToFloat32() const {
    DCHECK_EQ(kFloat32, type());
    return bit_cast<float>(static_cast<int32_t>(value_));
  }

  double ToFloat64() const {
    if (type() == kInt32) return ToInt32();
    DCHECK_EQ(kFloat64, type());
    return bit_cast<double>(value_);
  }

  ExternalReference ToExternalReference() const {
    DCHECK_EQ(kExternalReference, type());
    return bit_cast<ExternalReference>(static_cast<intptr_t>(value_));
  }

  RpoNumber ToRpoNumber() const {
    DCHECK_EQ(kRpoNumber, type());
    return RpoNumber::FromInt(static_cast<int>(value_));
  }

  Handle<HeapObject> ToHeapObject() const {
    DCHECK_EQ(kHeapObject, type());
    return bit_cast<Handle<HeapObject> >(static_cast<intptr_t>(value_));
  }

 private:
  Type type_;
  int64_t value_;
};


class FrameStateDescriptor : public ZoneObject {
 public:
  FrameStateDescriptor(Zone* zone, const FrameStateCallInfo& state_info,
                       size_t parameters_count, size_t locals_count,
                       size_t stack_count,
                       FrameStateDescriptor* outer_state = NULL);

  FrameStateType type() const { return type_; }
  BailoutId bailout_id() const { return bailout_id_; }
  OutputFrameStateCombine state_combine() const { return frame_state_combine_; }
  size_t parameters_count() const { return parameters_count_; }
  size_t locals_count() const { return locals_count_; }
  size_t stack_count() const { return stack_count_; }
  FrameStateDescriptor* outer_state() const { return outer_state_; }
  MaybeHandle<JSFunction> jsfunction() const { return jsfunction_; }
  bool HasContext() const { return type_ == JS_FRAME; }

  size_t GetSize(OutputFrameStateCombine combine =
                     OutputFrameStateCombine::Ignore()) const;
  size_t GetTotalSize() const;
  size_t GetFrameCount() const;
  size_t GetJSFrameCount() const;

  MachineType GetType(size_t index) const;
  void SetType(size_t index, MachineType type);

 private:
  FrameStateType type_;
  BailoutId bailout_id_;
  OutputFrameStateCombine frame_state_combine_;
  size_t parameters_count_;
  size_t locals_count_;
  size_t stack_count_;
  ZoneVector<MachineType> types_;
  FrameStateDescriptor* outer_state_;
  MaybeHandle<JSFunction> jsfunction_;
};

std::ostream& operator<<(std::ostream& os, const Constant& constant);


class PhiInstruction final : public ZoneObject {
 public:
  typedef ZoneVector<InstructionOperand> Inputs;

  PhiInstruction(Zone* zone, int virtual_register, size_t input_count);

  void SetInput(size_t offset, int virtual_register);

  int virtual_register() const { return virtual_register_; }
  const IntVector& operands() const { return operands_; }

  // TODO(dcarney): this has no real business being here, since it's internal to
  // the register allocator, but putting it here was convenient.
  const InstructionOperand& output() const { return output_; }
  InstructionOperand& output() { return output_; }

 private:
  const int virtual_register_;
  InstructionOperand output_;
  IntVector operands_;
};


// Analogue of BasicBlock for Instructions instead of Nodes.
class InstructionBlock final : public ZoneObject {
 public:
  InstructionBlock(Zone* zone, RpoNumber rpo_number, RpoNumber loop_header,
                   RpoNumber loop_end, bool deferred);

  // Instruction indexes (used by the register allocator).
  int first_instruction_index() const {
    DCHECK(code_start_ >= 0);
    DCHECK(code_end_ > 0);
    DCHECK(code_end_ >= code_start_);
    return code_start_;
  }
  int last_instruction_index() const {
    DCHECK(code_start_ >= 0);
    DCHECK(code_end_ > 0);
    DCHECK(code_end_ >= code_start_);
    return code_end_ - 1;
  }

  int32_t code_start() const { return code_start_; }
  void set_code_start(int32_t start) { code_start_ = start; }

  int32_t code_end() const { return code_end_; }
  void set_code_end(int32_t end) { code_end_ = end; }

  bool IsDeferred() const { return deferred_; }

  RpoNumber ao_number() const { return ao_number_; }
  RpoNumber rpo_number() const { return rpo_number_; }
  RpoNumber loop_header() const { return loop_header_; }
  RpoNumber loop_end() const {
    DCHECK(IsLoopHeader());
    return loop_end_;
  }
  inline bool IsLoopHeader() const { return loop_end_.IsValid(); }

  typedef ZoneVector<RpoNumber> Predecessors;
  Predecessors& predecessors() { return predecessors_; }
  const Predecessors& predecessors() const { return predecessors_; }
  size_t PredecessorCount() const { return predecessors_.size(); }
  size_t PredecessorIndexOf(RpoNumber rpo_number) const;

  typedef ZoneVector<RpoNumber> Successors;
  Successors& successors() { return successors_; }
  const Successors& successors() const { return successors_; }
  size_t SuccessorCount() const { return successors_.size(); }

  typedef ZoneVector<PhiInstruction*> PhiInstructions;
  const PhiInstructions& phis() const { return phis_; }
  void AddPhi(PhiInstruction* phi) { phis_.push_back(phi); }

  void set_ao_number(RpoNumber ao_number) { ao_number_ = ao_number; }

  bool needs_frame() const { return needs_frame_; }
  void mark_needs_frame() { needs_frame_ = true; }

  bool must_construct_frame() const { return must_construct_frame_; }
  void mark_must_construct_frame() { must_construct_frame_ = true; }

  bool must_deconstruct_frame() const { return must_deconstruct_frame_; }
  void mark_must_deconstruct_frame() { must_deconstruct_frame_ = true; }

 private:
  Successors successors_;
  Predecessors predecessors_;
  PhiInstructions phis_;
  RpoNumber ao_number_;  // Assembly order number.
  const RpoNumber rpo_number_;
  const RpoNumber loop_header_;
  const RpoNumber loop_end_;
  int32_t code_start_;   // start index of arch-specific code.
  int32_t code_end_;     // end index of arch-specific code.
  const bool deferred_;  // Block contains deferred code.
  bool needs_frame_;
  bool must_construct_frame_;
  bool must_deconstruct_frame_;
};

typedef ZoneDeque<Constant> ConstantDeque;
typedef std::map<int, Constant, std::less<int>,
                 zone_allocator<std::pair<int, Constant> > > ConstantMap;

typedef ZoneDeque<Instruction*> InstructionDeque;
typedef ZoneDeque<ReferenceMap*> ReferenceMapDeque;
typedef ZoneVector<FrameStateDescriptor*> DeoptimizationVector;
typedef ZoneVector<InstructionBlock*> InstructionBlocks;

struct PrintableInstructionSequence;


// Represents architecture-specific generated code before, during, and after
// register allocation.
// TODO(titzer): s/IsDouble/IsFloat64/
class InstructionSequence final : public ZoneObject {
 public:
  static InstructionBlocks* InstructionBlocksFor(Zone* zone,
                                                 const Schedule* schedule);
  // Puts the deferred blocks last.
  static void ComputeAssemblyOrder(InstructionBlocks* blocks);

  InstructionSequence(Isolate* isolate, Zone* zone,
                      InstructionBlocks* instruction_blocks);

  int NextVirtualRegister();
  int VirtualRegisterCount() const { return next_virtual_register_; }

  const InstructionBlocks& instruction_blocks() const {
    return *instruction_blocks_;
  }

  int InstructionBlockCount() const {
    return static_cast<int>(instruction_blocks_->size());
  }

  InstructionBlock* InstructionBlockAt(RpoNumber rpo_number) {
    return instruction_blocks_->at(rpo_number.ToSize());
  }

  int LastLoopInstructionIndex(const InstructionBlock* block) {
    return instruction_blocks_->at(block->loop_end().ToSize() - 1)
        ->last_instruction_index();
  }

  const InstructionBlock* InstructionBlockAt(RpoNumber rpo_number) const {
    return instruction_blocks_->at(rpo_number.ToSize());
  }

  InstructionBlock* GetInstructionBlock(int instruction_index) const;

  static MachineType DefaultRepresentation() {
    return kPointerSize == 8 ? kRepWord64 : kRepWord32;
  }
  MachineType GetRepresentation(int virtual_register) const;
  void MarkAsRepresentation(MachineType machine_type, int virtual_register);

  bool IsReference(int virtual_register) const {
    return GetRepresentation(virtual_register) == kRepTagged;
  }
  bool IsFloat(int virtual_register) const {
    switch (GetRepresentation(virtual_register)) {
      case kRepFloat32:
      case kRepFloat64:
        return true;
      default:
        return false;
    }
  }

  Instruction* GetBlockStart(RpoNumber rpo) const;

  typedef InstructionDeque::const_iterator const_iterator;
  const_iterator begin() const { return instructions_.begin(); }
  const_iterator end() const { return instructions_.end(); }
  const InstructionDeque& instructions() const { return instructions_; }

  Instruction* InstructionAt(int index) const {
    DCHECK(index >= 0);
    DCHECK(index < static_cast<int>(instructions_.size()));
    return instructions_[index];
  }

  Isolate* isolate() const { return isolate_; }
  const ReferenceMapDeque* reference_maps() const { return &reference_maps_; }
  Zone* zone() const { return zone_; }

  // Used by the instruction selector while adding instructions.
  int AddInstruction(Instruction* instr);
  void StartBlock(RpoNumber rpo);
  void EndBlock(RpoNumber rpo);

  int AddConstant(int virtual_register, Constant constant) {
    // TODO(titzer): allow RPO numbers as constants?
    DCHECK(constant.type() != Constant::kRpoNumber);
    DCHECK(virtual_register >= 0 && virtual_register < next_virtual_register_);
    DCHECK(constants_.find(virtual_register) == constants_.end());
    constants_.insert(std::make_pair(virtual_register, constant));
    return virtual_register;
  }
  Constant GetConstant(int virtual_register) const {
    ConstantMap::const_iterator it = constants_.find(virtual_register);
    DCHECK(it != constants_.end());
    DCHECK_EQ(virtual_register, it->first);
    return it->second;
  }

  typedef ZoneVector<Constant> Immediates;
  Immediates& immediates() { return immediates_; }

  ImmediateOperand AddImmediate(const Constant& constant) {
    if (constant.type() == Constant::kInt32) {
      return ImmediateOperand(ImmediateOperand::INLINE, constant.ToInt32());
    }
    int index = static_cast<int>(immediates_.size());
    immediates_.push_back(constant);
    return ImmediateOperand(ImmediateOperand::INDEXED, index);
  }

  Constant GetImmediate(const ImmediateOperand* op) const {
    switch (op->type()) {
      case ImmediateOperand::INLINE:
        return Constant(op->inline_value());
      case ImmediateOperand::INDEXED: {
        int index = op->indexed_value();
        DCHECK(index >= 0);
        DCHECK(index < static_cast<int>(immediates_.size()));
        return immediates_[index];
      }
    }
    UNREACHABLE();
    return Constant(static_cast<int32_t>(0));
  }

  class StateId {
   public:
    static StateId FromInt(int id) { return StateId(id); }
    int ToInt() const { return id_; }

   private:
    explicit StateId(int id) : id_(id) {}
    int id_;
  };

  StateId AddFrameStateDescriptor(FrameStateDescriptor* descriptor);
  FrameStateDescriptor* GetFrameStateDescriptor(StateId deoptimization_id);
  int GetFrameStateDescriptorCount();

  RpoNumber InputRpo(Instruction* instr, size_t index);

  bool GetSourcePosition(const Instruction* instr,
                         SourcePosition* result) const;
  void SetSourcePosition(const Instruction* instr, SourcePosition value);

 private:
  friend std::ostream& operator<<(std::ostream& os,
                                  const PrintableInstructionSequence& code);

  typedef std::set<int, std::less<int>, ZoneIntAllocator> VirtualRegisterSet;
  typedef ZoneMap<const Instruction*, SourcePosition> SourcePositionMap;

  Isolate* isolate_;
  Zone* const zone_;
  InstructionBlocks* const instruction_blocks_;
  SourcePositionMap source_positions_;
  IntVector block_starts_;
  ConstantMap constants_;
  Immediates immediates_;
  InstructionDeque instructions_;
  int next_virtual_register_;
  ReferenceMapDeque reference_maps_;
  ZoneVector<MachineType> representations_;
  DeoptimizationVector deoptimization_entries_;

  DISALLOW_COPY_AND_ASSIGN(InstructionSequence);
};


struct PrintableInstructionSequence {
  const RegisterConfiguration* register_configuration_;
  const InstructionSequence* sequence_;
};


std::ostream& operator<<(std::ostream& os,
                         const PrintableInstructionSequence& code);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INSTRUCTION_H_

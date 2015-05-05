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

// A couple of reserved opcodes are used for internal use.
const InstructionCode kGapInstruction = -1;
const InstructionCode kSourcePositionInstruction = -2;

#define INSTRUCTION_OPERAND_LIST(V)     \
  V(Constant, CONSTANT)                 \
  V(Immediate, IMMEDIATE)               \
  V(StackSlot, STACK_SLOT)              \
  V(DoubleStackSlot, DOUBLE_STACK_SLOT) \
  V(Register, REGISTER)                 \
  V(DoubleRegister, DOUBLE_REGISTER)

class InstructionOperand {
 public:
  static const int kInvalidVirtualRegister = -1;

  enum Kind {
    INVALID,
    UNALLOCATED,
    CONSTANT,
    IMMEDIATE,
    STACK_SLOT,
    DOUBLE_STACK_SLOT,
    REGISTER,
    DOUBLE_REGISTER
  };

  InstructionOperand() { ConvertTo(INVALID, 0, kInvalidVirtualRegister); }

  InstructionOperand(Kind kind, int index) {
    DCHECK(kind != UNALLOCATED && kind != INVALID);
    ConvertTo(kind, index, kInvalidVirtualRegister);
  }

  static InstructionOperand* New(Zone* zone, Kind kind, int index) {
    return New(zone, InstructionOperand(kind, index));
  }

  Kind kind() const { return KindField::decode(value_); }
  // TODO(dcarney): move this to subkind operand.
  int index() const {
    DCHECK(kind() != UNALLOCATED && kind() != INVALID);
    return static_cast<int64_t>(value_) >> IndexField::kShift;
  }
#define INSTRUCTION_OPERAND_PREDICATE(name, type) \
  bool Is##name() const { return kind() == type; }
  INSTRUCTION_OPERAND_LIST(INSTRUCTION_OPERAND_PREDICATE)
  INSTRUCTION_OPERAND_PREDICATE(Unallocated, UNALLOCATED)
  INSTRUCTION_OPERAND_PREDICATE(Invalid, INVALID)
#undef INSTRUCTION_OPERAND_PREDICATE
  bool Equals(const InstructionOperand* other) const {
    return value_ == other->value_;
  }

  void ConvertTo(Kind kind, int index) {
    DCHECK(kind != UNALLOCATED && kind != INVALID);
    ConvertTo(kind, index, kInvalidVirtualRegister);
  }

  // Useful for map/set keys.
  bool operator<(const InstructionOperand& op) const {
    return value_ < op.value_;
  }

 protected:
  template <typename SubKindOperand>
  static SubKindOperand* New(Zone* zone, const SubKindOperand& op) {
    void* buffer = zone->New(sizeof(op));
    return new (buffer) SubKindOperand(op);
  }

  InstructionOperand(Kind kind, int index, int virtual_register) {
    ConvertTo(kind, index, virtual_register);
  }

  void ConvertTo(Kind kind, int index, int virtual_register) {
    if (kind == REGISTER || kind == DOUBLE_REGISTER) DCHECK(index >= 0);
    if (kind != UNALLOCATED) {
      DCHECK(virtual_register == kInvalidVirtualRegister);
    }
    value_ = KindField::encode(kind);
    value_ |=
        VirtualRegisterField::encode(static_cast<uint32_t>(virtual_register));
    value_ |= static_cast<int64_t>(index) << IndexField::kShift;
    DCHECK(((kind == UNALLOCATED || kind == INVALID) && index == 0) ||
           this->index() == index);
  }

  typedef BitField64<Kind, 0, 3> KindField;
  typedef BitField64<uint32_t, 3, 32> VirtualRegisterField;
  typedef BitField64<int32_t, 35, 29> IndexField;

  uint64_t value_;
};

struct PrintableInstructionOperand {
  const RegisterConfiguration* register_configuration_;
  const InstructionOperand* op_;
};

std::ostream& operator<<(std::ostream& os,
                         const PrintableInstructionOperand& op);

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
      : InstructionOperand(UNALLOCATED, 0, virtual_register) {
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(USED_AT_END);
  }

  UnallocatedOperand(BasicPolicy policy, int index, int virtual_register)
      : InstructionOperand(UNALLOCATED, 0, virtual_register) {
    DCHECK(policy == FIXED_SLOT);
    value_ |= BasicPolicyField::encode(policy);
    value_ |= static_cast<int64_t>(index) << FixedSlotIndexField::kShift;
    DCHECK(this->fixed_slot_index() == index);
  }

  UnallocatedOperand(ExtendedPolicy policy, int index, int virtual_register)
      : InstructionOperand(UNALLOCATED, 0, virtual_register) {
    DCHECK(policy == FIXED_REGISTER || policy == FIXED_DOUBLE_REGISTER);
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(USED_AT_END);
    value_ |= FixedRegisterField::encode(index);
  }

  UnallocatedOperand(ExtendedPolicy policy, Lifetime lifetime,
                     int virtual_register)
      : InstructionOperand(UNALLOCATED, 0, virtual_register) {
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(lifetime);
  }

  UnallocatedOperand* Copy(Zone* zone) { return New(zone, *this); }

  UnallocatedOperand* CopyUnconstrained(Zone* zone) {
    return New(zone, UnallocatedOperand(ANY, virtual_register()));
  }

  static const UnallocatedOperand* cast(const InstructionOperand* op) {
    DCHECK(op->IsUnallocated());
    return static_cast<const UnallocatedOperand*>(op);
  }

  static UnallocatedOperand* cast(InstructionOperand* op) {
    DCHECK(op->IsUnallocated());
    return static_cast<UnallocatedOperand*>(op);
  }

  static UnallocatedOperand cast(const InstructionOperand& op) {
    DCHECK(op.IsUnallocated());
    return *static_cast<const UnallocatedOperand*>(&op);
  }

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

  // All bits fit into the index field.
  STATIC_ASSERT(IndexField::kShift == 35);

  // BitFields for all unallocated operands.
  class BasicPolicyField : public BitField64<BasicPolicy, 35, 1> {};

  // BitFields specific to BasicPolicy::FIXED_SLOT.
  class FixedSlotIndexField : public BitField64<int, 36, 28> {};

  // BitFields specific to BasicPolicy::EXTENDED_POLICY.
  class ExtendedPolicyField : public BitField64<ExtendedPolicy, 36, 3> {};
  class LifetimeField : public BitField64<Lifetime, 39, 1> {};
  class FixedRegisterField : public BitField64<int, 40, 6> {};

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
};


class MoveOperands FINAL {
 public:
  MoveOperands(InstructionOperand* source, InstructionOperand* destination)
      : source_(source), destination_(destination) {}

  InstructionOperand* source() const { return source_; }
  void set_source(InstructionOperand* operand) { source_ = operand; }

  InstructionOperand* destination() const { return destination_; }
  void set_destination(InstructionOperand* operand) { destination_ = operand; }

  // The gap resolver marks moves as "in-progress" by clearing the
  // destination (but not the source).
  bool IsPending() const { return destination_ == NULL && source_ != NULL; }

  // True if this move a move into the given destination operand.
  bool Blocks(InstructionOperand* operand) const {
    return !IsEliminated() && source()->Equals(operand);
  }

  // A move is redundant if it's been eliminated, if its source and
  // destination are the same, or if its destination is  constant.
  bool IsRedundant() const {
    return IsEliminated() || source_->Equals(destination_) ||
           (destination_ != NULL && destination_->IsConstant());
  }

  // We clear both operands to indicate move that's been eliminated.
  void Eliminate() { source_ = destination_ = NULL; }
  bool IsEliminated() const {
    DCHECK(source_ != NULL || destination_ == NULL);
    return source_ == NULL;
  }

 private:
  InstructionOperand* source_;
  InstructionOperand* destination_;
};


struct PrintableMoveOperands {
  const RegisterConfiguration* register_configuration_;
  const MoveOperands* move_operands_;
};


std::ostream& operator<<(std::ostream& os, const PrintableMoveOperands& mo);


#define INSTRUCTION_SUBKIND_OPERAND_CLASS(SubKind, kOperandKind)        \
  class SubKind##Operand FINAL : public InstructionOperand {            \
   public:                                                              \
    explicit SubKind##Operand(int index)                                \
        : InstructionOperand(kOperandKind, index) {}                    \
                                                                        \
    static SubKind##Operand* New(int index, Zone* zone) {               \
      return InstructionOperand::New(zone, SubKind##Operand(index));    \
    }                                                                   \
                                                                        \
    static SubKind##Operand* cast(InstructionOperand* op) {             \
      DCHECK(op->kind() == kOperandKind);                               \
      return reinterpret_cast<SubKind##Operand*>(op);                   \
    }                                                                   \
                                                                        \
    static const SubKind##Operand* cast(const InstructionOperand* op) { \
      DCHECK(op->kind() == kOperandKind);                               \
      return reinterpret_cast<const SubKind##Operand*>(op);             \
    }                                                                   \
                                                                        \
    static SubKind##Operand cast(const InstructionOperand& op) {        \
      DCHECK(op.kind() == kOperandKind);                                \
      return *static_cast<const SubKind##Operand*>(&op);                \
    }                                                                   \
  };
INSTRUCTION_OPERAND_LIST(INSTRUCTION_SUBKIND_OPERAND_CLASS)
#undef INSTRUCTION_SUBKIND_OPERAND_CLASS


class ParallelMove FINAL : public ZoneObject {
 public:
  explicit ParallelMove(Zone* zone) : move_operands_(4, zone) {}

  void AddMove(InstructionOperand* from, InstructionOperand* to, Zone* zone) {
    move_operands_.Add(MoveOperands(from, to), zone);
  }

  bool IsRedundant() const;

  ZoneList<MoveOperands>* move_operands() { return &move_operands_; }
  const ZoneList<MoveOperands>* move_operands() const {
    return &move_operands_;
  }

  // Prepare this ParallelMove to insert move as if it happened in a subsequent
  // ParallelMove.  move->source() may be changed.  The MoveOperand returned
  // must be Eliminated and, as it points directly into move_operands_, it must
  // be Eliminated before any further mutation.
  MoveOperands* PrepareInsertAfter(MoveOperands* move) const;

 private:
  ZoneList<MoveOperands> move_operands_;
};


struct PrintableParallelMove {
  const RegisterConfiguration* register_configuration_;
  const ParallelMove* parallel_move_;
};


std::ostream& operator<<(std::ostream& os, const PrintableParallelMove& pm);


class PointerMap FINAL : public ZoneObject {
 public:
  explicit PointerMap(Zone* zone)
      : pointer_operands_(8, zone),
        untagged_operands_(0, zone),
        instruction_position_(-1) {}

  const ZoneList<InstructionOperand*>* GetNormalizedOperands() {
    for (int i = 0; i < untagged_operands_.length(); ++i) {
      RemovePointer(untagged_operands_[i]);
    }
    untagged_operands_.Clear();
    return &pointer_operands_;
  }
  int instruction_position() const { return instruction_position_; }

  void set_instruction_position(int pos) {
    DCHECK(instruction_position_ == -1);
    instruction_position_ = pos;
  }

  void RecordPointer(InstructionOperand* op, Zone* zone);
  void RemovePointer(InstructionOperand* op);
  void RecordUntagged(InstructionOperand* op, Zone* zone);

 private:
  friend std::ostream& operator<<(std::ostream& os, const PointerMap& pm);

  ZoneList<InstructionOperand*> pointer_operands_;
  ZoneList<InstructionOperand*> untagged_operands_;
  int instruction_position_;
};

std::ostream& operator<<(std::ostream& os, const PointerMap& pm);

// TODO(titzer): s/PointerMap/ReferenceMap/
class Instruction {
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
  bool NeedsPointerMap() const { return IsCall(); }
  bool HasPointerMap() const { return pointer_map_ != NULL; }

  bool IsGapMoves() const { return opcode() == kGapInstruction; }
  bool IsSourcePosition() const {
    return opcode() == kSourcePositionInstruction;
  }

  bool ClobbersRegisters() const { return IsCall(); }
  bool ClobbersTemps() const { return IsCall(); }
  bool ClobbersDoubleRegisters() const { return IsCall(); }
  PointerMap* pointer_map() const { return pointer_map_; }

  void set_pointer_map(PointerMap* map) {
    DCHECK(NeedsPointerMap());
    DCHECK(!pointer_map_);
    pointer_map_ = map;
  }

  void OverwriteWithNop() {
    opcode_ = ArchOpcodeField::encode(kArchNop);
    bit_field_ = 0;
    pointer_map_ = NULL;
  }

  bool IsNop() const {
    return arch_opcode() == kArchNop && InputCount() == 0 &&
           OutputCount() == 0 && TempCount() == 0;
  }

 protected:
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
  PointerMap* pointer_map_;
  InstructionOperand operands_[1];

 private:
  DISALLOW_COPY_AND_ASSIGN(Instruction);
};


struct PrintableInstruction {
  const RegisterConfiguration* register_configuration_;
  const Instruction* instr_;
};
std::ostream& operator<<(std::ostream& os, const PrintableInstruction& instr);


// Represents moves inserted before an instruction due to register allocation.
// TODO(titzer): squash GapInstruction back into Instruction, since essentially
// every instruction can possibly have moves inserted before it.
class GapInstruction : public Instruction {
 public:
  enum InnerPosition {
    START,
    END,
    FIRST_INNER_POSITION = START,
    LAST_INNER_POSITION = END
  };

  ParallelMove* GetOrCreateParallelMove(InnerPosition pos, Zone* zone) {
    if (parallel_moves_[pos] == NULL) {
      parallel_moves_[pos] = new (zone) ParallelMove(zone);
    }
    return parallel_moves_[pos];
  }

  ParallelMove* GetParallelMove(InnerPosition pos) {
    return parallel_moves_[pos];
  }

  const ParallelMove* GetParallelMove(InnerPosition pos) const {
    return parallel_moves_[pos];
  }

  bool IsRedundant() const;

  ParallelMove** parallel_moves() { return parallel_moves_; }

  static GapInstruction* New(Zone* zone) {
    void* buffer = zone->New(sizeof(GapInstruction));
    return new (buffer) GapInstruction(kGapInstruction);
  }

  static GapInstruction* cast(Instruction* instr) {
    DCHECK(instr->IsGapMoves());
    return static_cast<GapInstruction*>(instr);
  }

  static const GapInstruction* cast(const Instruction* instr) {
    DCHECK(instr->IsGapMoves());
    return static_cast<const GapInstruction*>(instr);
  }

 protected:
  explicit GapInstruction(InstructionCode opcode) : Instruction(opcode) {
    parallel_moves_[START] = NULL;
    parallel_moves_[END] = NULL;
  }

 private:
  friend std::ostream& operator<<(std::ostream& os,
                                  const PrintableInstruction& instr);
  ParallelMove* parallel_moves_[LAST_INNER_POSITION + 1];
};


class SourcePositionInstruction FINAL : public Instruction {
 public:
  static SourcePositionInstruction* New(Zone* zone, SourcePosition position) {
    void* buffer = zone->New(sizeof(SourcePositionInstruction));
    return new (buffer) SourcePositionInstruction(position);
  }

  SourcePosition source_position() const { return source_position_; }

  static SourcePositionInstruction* cast(Instruction* instr) {
    DCHECK(instr->IsSourcePosition());
    return static_cast<SourcePositionInstruction*>(instr);
  }

  static const SourcePositionInstruction* cast(const Instruction* instr) {
    DCHECK(instr->IsSourcePosition());
    return static_cast<const SourcePositionInstruction*>(instr);
  }

 private:
  explicit SourcePositionInstruction(SourcePosition source_position)
      : Instruction(kSourcePositionInstruction),
        source_position_(source_position) {
    DCHECK(!source_position_.IsInvalid());
    DCHECK(!source_position_.IsUnknown());
  }

  SourcePosition source_position_;
};


class RpoNumber FINAL {
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


class Constant FINAL {
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


class PhiInstruction FINAL : public ZoneObject {
 public:
  typedef ZoneVector<InstructionOperand> Inputs;

  PhiInstruction(Zone* zone, int virtual_register, size_t input_count);

  void SetInput(size_t offset, int virtual_register);

  int virtual_register() const { return virtual_register_; }
  const IntVector& operands() const { return operands_; }

  const InstructionOperand& output() const { return output_; }
  InstructionOperand& output() { return output_; }
  const Inputs& inputs() const { return inputs_; }
  Inputs& inputs() { return inputs_; }

 private:
  // TODO(dcarney): some of these fields are only for verification, move them to
  // verifier.
  const int virtual_register_;
  InstructionOperand output_;
  IntVector operands_;
  Inputs inputs_;
};


// Analogue of BasicBlock for Instructions instead of Nodes.
class InstructionBlock FINAL : public ZoneObject {
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
};

typedef ZoneDeque<Constant> ConstantDeque;
typedef std::map<int, Constant, std::less<int>,
                 zone_allocator<std::pair<int, Constant> > > ConstantMap;

typedef ZoneDeque<Instruction*> InstructionDeque;
typedef ZoneDeque<PointerMap*> PointerMapDeque;
typedef ZoneVector<FrameStateDescriptor*> DeoptimizationVector;
typedef ZoneVector<InstructionBlock*> InstructionBlocks;

struct PrintableInstructionSequence;


// Represents architecture-specific generated code before, during, and after
// register allocation.
// TODO(titzer): s/IsDouble/IsFloat64/
class InstructionSequence FINAL : public ZoneObject {
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

  const InstructionBlock* GetInstructionBlock(int instruction_index) const;

  bool IsReference(int virtual_register) const;
  bool IsDouble(int virtual_register) const;

  void MarkAsReference(int virtual_register);
  void MarkAsDouble(int virtual_register);

  void AddGapMove(int index, InstructionOperand* from, InstructionOperand* to);

  GapInstruction* GetBlockStart(RpoNumber rpo) const;

  typedef InstructionDeque::const_iterator const_iterator;
  const_iterator begin() const { return instructions_.begin(); }
  const_iterator end() const { return instructions_.end(); }
  const InstructionDeque& instructions() const { return instructions_; }

  GapInstruction* GapAt(int index) const {
    return GapInstruction::cast(InstructionAt(index));
  }
  bool IsGapAt(int index) const { return InstructionAt(index)->IsGapMoves(); }
  Instruction* InstructionAt(int index) const {
    DCHECK(index >= 0);
    DCHECK(index < static_cast<int>(instructions_.size()));
    return instructions_[index];
  }

  Isolate* isolate() const { return isolate_; }
  const PointerMapDeque* pointer_maps() const { return &pointer_maps_; }
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

  int AddImmediate(Constant constant) {
    int index = static_cast<int>(immediates_.size());
    immediates_.push_back(constant);
    return index;
  }
  Constant GetImmediate(int index) const {
    DCHECK(index >= 0);
    DCHECK(index < static_cast<int>(immediates_.size()));
    return immediates_[index];
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

  RpoNumber InputRpo(Instruction* instr, size_t index) {
    InstructionOperand* operand = instr->InputAt(index);
    Constant constant = operand->IsImmediate() ? GetImmediate(operand->index())
                                               : GetConstant(operand->index());
    return constant.ToRpoNumber();
  }

 private:
  friend std::ostream& operator<<(std::ostream& os,
                                  const PrintableInstructionSequence& code);

  typedef std::set<int, std::less<int>, ZoneIntAllocator> VirtualRegisterSet;

  Isolate* isolate_;
  Zone* const zone_;
  InstructionBlocks* const instruction_blocks_;
  IntVector block_starts_;
  ConstantMap constants_;
  Immediates immediates_;
  InstructionDeque instructions_;
  int next_virtual_register_;
  PointerMapDeque pointer_maps_;
  VirtualRegisterSet doubles_;
  VirtualRegisterSet references_;
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

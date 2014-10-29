// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INSTRUCTION_H_
#define V8_COMPILER_INSTRUCTION_H_

#include <deque>
#include <map>
#include <set>

#include "src/compiler/common-operator.h"
#include "src/compiler/frame.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction-codes.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/schedule.h"
// TODO(titzer): don't include the macro-assembler?
#include "src/macro-assembler.h"
#include "src/zone-allocator.h"

namespace v8 {
namespace internal {

// Forward declarations.
class OStream;

namespace compiler {

// Forward declarations.
class Linkage;

// A couple of reserved opcodes are used for internal use.
const InstructionCode kGapInstruction = -1;
const InstructionCode kBlockStartInstruction = -2;
const InstructionCode kSourcePositionInstruction = -3;


#define INSTRUCTION_OPERAND_LIST(V)              \
  V(Constant, CONSTANT, 128)                     \
  V(Immediate, IMMEDIATE, 128)                   \
  V(StackSlot, STACK_SLOT, 128)                  \
  V(DoubleStackSlot, DOUBLE_STACK_SLOT, 128)     \
  V(Register, REGISTER, Register::kNumRegisters) \
  V(DoubleRegister, DOUBLE_REGISTER, DoubleRegister::kMaxNumRegisters)

class InstructionOperand : public ZoneObject {
 public:
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

  InstructionOperand() : value_(KindField::encode(INVALID)) {}
  InstructionOperand(Kind kind, int index) { ConvertTo(kind, index); }

  Kind kind() const { return KindField::decode(value_); }
  int index() const { return static_cast<int>(value_) >> KindField::kSize; }
#define INSTRUCTION_OPERAND_PREDICATE(name, type, number) \
  bool Is##name() const { return kind() == type; }
  INSTRUCTION_OPERAND_LIST(INSTRUCTION_OPERAND_PREDICATE)
  INSTRUCTION_OPERAND_PREDICATE(Unallocated, UNALLOCATED, 0)
  INSTRUCTION_OPERAND_PREDICATE(Ignored, INVALID, 0)
#undef INSTRUCTION_OPERAND_PREDICATE
  bool Equals(InstructionOperand* other) const {
    return value_ == other->value_;
  }

  void ConvertTo(Kind kind, int index) {
    if (kind == REGISTER || kind == DOUBLE_REGISTER) DCHECK(index >= 0);
    value_ = KindField::encode(kind);
    value_ |= index << KindField::kSize;
    DCHECK(this->index() == index);
  }

  // Calls SetUpCache()/TearDownCache() for each subclass.
  static void SetUpCaches();
  static void TearDownCaches();

 protected:
  typedef BitField<Kind, 0, 3> KindField;

  unsigned value_;
};

typedef ZoneVector<InstructionOperand*> InstructionOperandVector;

OStream& operator<<(OStream& os, const InstructionOperand& op);

class UnallocatedOperand : public InstructionOperand {
 public:
  enum BasicPolicy { FIXED_SLOT, EXTENDED_POLICY };

  enum ExtendedPolicy {
    NONE,
    ANY,
    FIXED_REGISTER,
    FIXED_DOUBLE_REGISTER,
    MUST_HAVE_REGISTER,
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

  explicit UnallocatedOperand(ExtendedPolicy policy)
      : InstructionOperand(UNALLOCATED, 0) {
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(USED_AT_END);
  }

  UnallocatedOperand(BasicPolicy policy, int index)
      : InstructionOperand(UNALLOCATED, 0) {
    DCHECK(policy == FIXED_SLOT);
    value_ |= BasicPolicyField::encode(policy);
    value_ |= index << FixedSlotIndexField::kShift;
    DCHECK(this->fixed_slot_index() == index);
  }

  UnallocatedOperand(ExtendedPolicy policy, int index)
      : InstructionOperand(UNALLOCATED, 0) {
    DCHECK(policy == FIXED_REGISTER || policy == FIXED_DOUBLE_REGISTER);
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(USED_AT_END);
    value_ |= FixedRegisterField::encode(index);
  }

  UnallocatedOperand(ExtendedPolicy policy, Lifetime lifetime)
      : InstructionOperand(UNALLOCATED, 0) {
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(lifetime);
  }

  UnallocatedOperand* CopyUnconstrained(Zone* zone) {
    UnallocatedOperand* result = new (zone) UnallocatedOperand(ANY);
    result->set_virtual_register(virtual_register());
    return result;
  }

  static const UnallocatedOperand* cast(const InstructionOperand* op) {
    DCHECK(op->IsUnallocated());
    return static_cast<const UnallocatedOperand*>(op);
  }

  static UnallocatedOperand* cast(InstructionOperand* op) {
    DCHECK(op->IsUnallocated());
    return static_cast<UnallocatedOperand*>(op);
  }

  // The encoding used for UnallocatedOperand operands depends on the policy
  // that is
  // stored within the operand. The FIXED_SLOT policy uses a compact encoding
  // because it accommodates a larger pay-load.
  //
  // For FIXED_SLOT policy:
  //     +------------------------------------------+
  //     |       slot_index      |  vreg  | 0 | 001 |
  //     +------------------------------------------+
  //
  // For all other (extended) policies:
  //     +------------------------------------------+
  //     |  reg_index  | L | PPP |  vreg  | 1 | 001 |    L ... Lifetime
  //     +------------------------------------------+    P ... Policy
  //
  // The slot index is a signed value which requires us to decode it manually
  // instead of using the BitField utility class.

  // The superclass has a KindField.
  STATIC_ASSERT(KindField::kSize == 3);

  // BitFields for all unallocated operands.
  class BasicPolicyField : public BitField<BasicPolicy, 3, 1> {};
  class VirtualRegisterField : public BitField<unsigned, 4, 18> {};

  // BitFields specific to BasicPolicy::FIXED_SLOT.
  class FixedSlotIndexField : public BitField<int, 22, 10> {};

  // BitFields specific to BasicPolicy::EXTENDED_POLICY.
  class ExtendedPolicyField : public BitField<ExtendedPolicy, 22, 3> {};
  class LifetimeField : public BitField<Lifetime, 25, 1> {};
  class FixedRegisterField : public BitField<int, 26, 6> {};

  static const int kMaxVirtualRegisters = VirtualRegisterField::kMax + 1;
  static const int kFixedSlotIndexWidth = FixedSlotIndexField::kSize;
  static const int kMaxFixedSlotIndex = (1 << (kFixedSlotIndexWidth - 1)) - 1;
  static const int kMinFixedSlotIndex = -(1 << (kFixedSlotIndexWidth - 1));

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
  BasicPolicy basic_policy() const { return BasicPolicyField::decode(value_); }

  // [extended_policy]: Only for non-FIXED_SLOT. The finer-grained policy.
  ExtendedPolicy extended_policy() const {
    DCHECK(basic_policy() == EXTENDED_POLICY);
    return ExtendedPolicyField::decode(value_);
  }

  // [fixed_slot_index]: Only for FIXED_SLOT.
  int fixed_slot_index() const {
    DCHECK(HasFixedSlotPolicy());
    return static_cast<int>(value_) >> FixedSlotIndexField::kShift;
  }

  // [fixed_register_index]: Only for FIXED_REGISTER or FIXED_DOUBLE_REGISTER.
  int fixed_register_index() const {
    DCHECK(HasFixedRegisterPolicy() || HasFixedDoubleRegisterPolicy());
    return FixedRegisterField::decode(value_);
  }

  // [virtual_register]: The virtual register ID for this operand.
  int virtual_register() const { return VirtualRegisterField::decode(value_); }
  void set_virtual_register(unsigned id) {
    value_ = VirtualRegisterField::update(value_, id);
  }

  // [lifetime]: Only for non-FIXED_SLOT.
  bool IsUsedAtStart() {
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
  // destination are the same, or if its destination is unneeded or constant.
  bool IsRedundant() const {
    return IsEliminated() || source_->Equals(destination_) || IsIgnored() ||
           (destination_ != NULL && destination_->IsConstant());
  }

  bool IsIgnored() const {
    return destination_ != NULL && destination_->IsIgnored();
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

OStream& operator<<(OStream& os, const MoveOperands& mo);

template <InstructionOperand::Kind kOperandKind, int kNumCachedOperands>
class SubKindOperand FINAL : public InstructionOperand {
 public:
  static SubKindOperand* Create(int index, Zone* zone) {
    DCHECK(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new (zone) SubKindOperand(index);
  }

  static SubKindOperand* cast(InstructionOperand* op) {
    DCHECK(op->kind() == kOperandKind);
    return reinterpret_cast<SubKindOperand*>(op);
  }

  static void SetUpCache();
  static void TearDownCache();

 private:
  static SubKindOperand* cache;

  SubKindOperand() : InstructionOperand() {}
  explicit SubKindOperand(int index)
      : InstructionOperand(kOperandKind, index) {}
};


#define INSTRUCTION_TYPEDEF_SUBKIND_OPERAND_CLASS(name, type, number) \
  typedef SubKindOperand<InstructionOperand::type, number> name##Operand;
INSTRUCTION_OPERAND_LIST(INSTRUCTION_TYPEDEF_SUBKIND_OPERAND_CLASS)
#undef INSTRUCTION_TYPEDEF_SUBKIND_OPERAND_CLASS


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

 private:
  ZoneList<MoveOperands> move_operands_;
};

OStream& operator<<(OStream& os, const ParallelMove& pm);

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
  friend OStream& operator<<(OStream& os, const PointerMap& pm);

  ZoneList<InstructionOperand*> pointer_operands_;
  ZoneList<InstructionOperand*> untagged_operands_;
  int instruction_position_;
};

OStream& operator<<(OStream& os, const PointerMap& pm);

// TODO(titzer): s/PointerMap/ReferenceMap/
class Instruction : public ZoneObject {
 public:
  size_t OutputCount() const { return OutputCountField::decode(bit_field_); }
  InstructionOperand* OutputAt(size_t i) const {
    DCHECK(i < OutputCount());
    return operands_[i];
  }

  bool HasOutput() const { return OutputCount() == 1; }
  InstructionOperand* Output() const { return OutputAt(0); }

  size_t InputCount() const { return InputCountField::decode(bit_field_); }
  InstructionOperand* InputAt(size_t i) const {
    DCHECK(i < InputCount());
    return operands_[OutputCount() + i];
  }

  size_t TempCount() const { return TempCountField::decode(bit_field_); }
  InstructionOperand* TempAt(size_t i) const {
    DCHECK(i < TempCount());
    return operands_[OutputCount() + InputCount() + i];
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

  // TODO(titzer): make control and call into flags.
  static Instruction* New(Zone* zone, InstructionCode opcode) {
    return New(zone, opcode, 0, NULL, 0, NULL, 0, NULL);
  }

  static Instruction* New(Zone* zone, InstructionCode opcode,
                          size_t output_count, InstructionOperand** outputs,
                          size_t input_count, InstructionOperand** inputs,
                          size_t temp_count, InstructionOperand** temps) {
    DCHECK(opcode >= 0);
    DCHECK(output_count == 0 || outputs != NULL);
    DCHECK(input_count == 0 || inputs != NULL);
    DCHECK(temp_count == 0 || temps != NULL);
    InstructionOperand* none = NULL;
    USE(none);
    int size = static_cast<int>(RoundUp(sizeof(Instruction), kPointerSize) +
                                (output_count + input_count + temp_count - 1) *
                                    sizeof(none));
    return new (zone->New(size)) Instruction(
        opcode, output_count, outputs, input_count, inputs, temp_count, temps);
  }

  // TODO(titzer): another holdover from lithium days; register allocator
  // should not need to know about control instructions.
  Instruction* MarkAsControl() {
    bit_field_ = IsControlField::update(bit_field_, true);
    return this;
  }
  Instruction* MarkAsCall() {
    bit_field_ = IsCallField::update(bit_field_, true);
    return this;
  }
  bool IsControl() const { return IsControlField::decode(bit_field_); }
  bool IsCall() const { return IsCallField::decode(bit_field_); }
  bool NeedsPointerMap() const { return IsCall(); }
  bool HasPointerMap() const { return pointer_map_ != NULL; }

  bool IsGapMoves() const {
    return opcode() == kGapInstruction || opcode() == kBlockStartInstruction;
  }
  bool IsBlockStart() const { return opcode() == kBlockStartInstruction; }
  bool IsSourcePosition() const {
    return opcode() == kSourcePositionInstruction;
  }

  bool ClobbersRegisters() const { return IsCall(); }
  bool ClobbersTemps() const { return IsCall(); }
  bool ClobbersDoubleRegisters() const { return IsCall(); }
  PointerMap* pointer_map() const { return pointer_map_; }

  void set_pointer_map(PointerMap* map) {
    DCHECK(NeedsPointerMap());
    DCHECK_EQ(NULL, pointer_map_);
    pointer_map_ = map;
  }

  // Placement new operator so that we can smash instructions into
  // zone-allocated memory.
  void* operator new(size_t, void* location) { return location; }

  void operator delete(void* pointer, void* location) { UNREACHABLE(); }

 protected:
  explicit Instruction(InstructionCode opcode)
      : opcode_(opcode),
        bit_field_(OutputCountField::encode(0) | InputCountField::encode(0) |
                   TempCountField::encode(0) | IsCallField::encode(false) |
                   IsControlField::encode(false)),
        pointer_map_(NULL) {}

  Instruction(InstructionCode opcode, size_t output_count,
              InstructionOperand** outputs, size_t input_count,
              InstructionOperand** inputs, size_t temp_count,
              InstructionOperand** temps)
      : opcode_(opcode),
        bit_field_(OutputCountField::encode(output_count) |
                   InputCountField::encode(input_count) |
                   TempCountField::encode(temp_count) |
                   IsCallField::encode(false) | IsControlField::encode(false)),
        pointer_map_(NULL) {
    for (size_t i = 0; i < output_count; ++i) {
      operands_[i] = outputs[i];
    }
    for (size_t i = 0; i < input_count; ++i) {
      operands_[output_count + i] = inputs[i];
    }
    for (size_t i = 0; i < temp_count; ++i) {
      operands_[output_count + input_count + i] = temps[i];
    }
  }

 protected:
  typedef BitField<size_t, 0, 8> OutputCountField;
  typedef BitField<size_t, 8, 16> InputCountField;
  typedef BitField<size_t, 24, 6> TempCountField;
  typedef BitField<bool, 30, 1> IsCallField;
  typedef BitField<bool, 31, 1> IsControlField;

  InstructionCode opcode_;
  uint32_t bit_field_;
  PointerMap* pointer_map_;
  InstructionOperand* operands_[1];
};

OStream& operator<<(OStream& os, const Instruction& instr);

// Represents moves inserted before an instruction due to register allocation.
// TODO(titzer): squash GapInstruction back into Instruction, since essentially
// every instruction can possibly have moves inserted before it.
class GapInstruction : public Instruction {
 public:
  enum InnerPosition {
    BEFORE,
    START,
    END,
    AFTER,
    FIRST_INNER_POSITION = BEFORE,
    LAST_INNER_POSITION = AFTER
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
    parallel_moves_[BEFORE] = NULL;
    parallel_moves_[START] = NULL;
    parallel_moves_[END] = NULL;
    parallel_moves_[AFTER] = NULL;
  }

 private:
  friend OStream& operator<<(OStream& os, const Instruction& instr);
  ParallelMove* parallel_moves_[LAST_INNER_POSITION + 1];
};


// This special kind of gap move instruction represents the beginning of a
// block of code.
// TODO(titzer): move code_start and code_end from BasicBlock to here.
class BlockStartInstruction FINAL : public GapInstruction {
 public:
  BasicBlock* block() const { return block_; }
  Label* label() { return &label_; }

  static BlockStartInstruction* New(Zone* zone, BasicBlock* block) {
    void* buffer = zone->New(sizeof(BlockStartInstruction));
    return new (buffer) BlockStartInstruction(block);
  }

  static BlockStartInstruction* cast(Instruction* instr) {
    DCHECK(instr->IsBlockStart());
    return static_cast<BlockStartInstruction*>(instr);
  }

 private:
  explicit BlockStartInstruction(BasicBlock* block)
      : GapInstruction(kBlockStartInstruction), block_(block) {}

  BasicBlock* block_;
  Label label_;
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


class Constant FINAL {
 public:
  enum Type {
    kInt32,
    kInt64,
    kFloat32,
    kFloat64,
    kExternalReference,
    kHeapObject
  };

  explicit Constant(int32_t v) : type_(kInt32), value_(v) {}
  explicit Constant(int64_t v) : type_(kInt64), value_(v) {}
  explicit Constant(float v) : type_(kFloat32), value_(bit_cast<int32_t>(v)) {}
  explicit Constant(double v) : type_(kFloat64), value_(bit_cast<int64_t>(v)) {}
  explicit Constant(ExternalReference ref)
      : type_(kExternalReference), value_(bit_cast<intptr_t>(ref)) {}
  explicit Constant(Handle<HeapObject> obj)
      : type_(kHeapObject), value_(bit_cast<intptr_t>(obj)) {}

  Type type() const { return type_; }

  int32_t ToInt32() const {
    DCHECK_EQ(kInt32, type());
    return static_cast<int32_t>(value_);
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
  FrameStateDescriptor(const FrameStateCallInfo& state_info,
                       size_t parameters_count, size_t locals_count,
                       size_t stack_count,
                       FrameStateDescriptor* outer_state = NULL)
      : type_(state_info.type()),
        bailout_id_(state_info.bailout_id()),
        frame_state_combine_(state_info.state_combine()),
        parameters_count_(parameters_count),
        locals_count_(locals_count),
        stack_count_(stack_count),
        outer_state_(outer_state),
        jsfunction_(state_info.jsfunction()) {}

  FrameStateType type() const { return type_; }
  BailoutId bailout_id() const { return bailout_id_; }
  OutputFrameStateCombine state_combine() const { return frame_state_combine_; }
  size_t parameters_count() const { return parameters_count_; }
  size_t locals_count() const { return locals_count_; }
  size_t stack_count() const { return stack_count_; }
  FrameStateDescriptor* outer_state() const { return outer_state_; }
  MaybeHandle<JSFunction> jsfunction() const { return jsfunction_; }

  size_t size() const {
    return parameters_count_ + locals_count_ + stack_count_ +
           (HasContext() ? 1 : 0);
  }

  size_t GetTotalSize() const {
    size_t total_size = 0;
    for (const FrameStateDescriptor* iter = this; iter != NULL;
         iter = iter->outer_state_) {
      total_size += iter->size();
    }
    return total_size;
  }

  size_t GetHeight(OutputFrameStateCombine override) const {
    size_t height = size() - parameters_count();
    switch (override) {
      case kPushOutput:
        ++height;
        break;
      case kIgnoreOutput:
        break;
    }
    return height;
  }

  size_t GetFrameCount() const {
    size_t count = 0;
    for (const FrameStateDescriptor* iter = this; iter != NULL;
         iter = iter->outer_state_) {
      ++count;
    }
    return count;
  }

  size_t GetJSFrameCount() const {
    size_t count = 0;
    for (const FrameStateDescriptor* iter = this; iter != NULL;
         iter = iter->outer_state_) {
      if (iter->type_ == JS_FRAME) {
        ++count;
      }
    }
    return count;
  }

  bool HasContext() const { return type_ == JS_FRAME; }

 private:
  FrameStateType type_;
  BailoutId bailout_id_;
  OutputFrameStateCombine frame_state_combine_;
  size_t parameters_count_;
  size_t locals_count_;
  size_t stack_count_;
  FrameStateDescriptor* outer_state_;
  MaybeHandle<JSFunction> jsfunction_;
};

OStream& operator<<(OStream& os, const Constant& constant);

typedef ZoneDeque<Constant> ConstantDeque;
typedef std::map<int, Constant, std::less<int>,
                 zone_allocator<std::pair<int, Constant> > > ConstantMap;

typedef ZoneDeque<Instruction*> InstructionDeque;
typedef ZoneDeque<PointerMap*> PointerMapDeque;
typedef ZoneVector<FrameStateDescriptor*> DeoptimizationVector;

// Represents architecture-specific generated code before, during, and after
// register allocation.
// TODO(titzer): s/IsDouble/IsFloat64/
class InstructionSequence FINAL {
 public:
  InstructionSequence(Linkage* linkage, Graph* graph, Schedule* schedule)
      : graph_(graph),
        linkage_(linkage),
        schedule_(schedule),
        constants_(ConstantMap::key_compare(),
                   ConstantMap::allocator_type(zone())),
        immediates_(zone()),
        instructions_(zone()),
        next_virtual_register_(graph->NodeCount()),
        pointer_maps_(zone()),
        doubles_(std::less<int>(), VirtualRegisterSet::allocator_type(zone())),
        references_(std::less<int>(),
                    VirtualRegisterSet::allocator_type(zone())),
        deoptimization_entries_(zone()) {}

  int NextVirtualRegister() { return next_virtual_register_++; }
  int VirtualRegisterCount() const { return next_virtual_register_; }

  int ValueCount() const { return graph_->NodeCount(); }

  int BasicBlockCount() const {
    return static_cast<int>(schedule_->rpo_order()->size());
  }

  BasicBlock* BlockAt(int rpo_number) const {
    return (*schedule_->rpo_order())[rpo_number];
  }

  BasicBlock* GetContainingLoop(BasicBlock* block) {
    return block->loop_header_;
  }

  int GetLoopEnd(BasicBlock* block) const { return block->loop_end_; }

  BasicBlock* GetBasicBlock(int instruction_index);

  int GetVirtualRegister(Node* node) const { return node->id(); }

  bool IsReference(int virtual_register) const;
  bool IsDouble(int virtual_register) const;

  void MarkAsReference(int virtual_register);
  void MarkAsDouble(int virtual_register);

  void AddGapMove(int index, InstructionOperand* from, InstructionOperand* to);

  Label* GetLabel(BasicBlock* block);
  BlockStartInstruction* GetBlockStart(BasicBlock* block);

  typedef InstructionDeque::const_iterator const_iterator;
  const_iterator begin() const { return instructions_.begin(); }
  const_iterator end() const { return instructions_.end(); }

  GapInstruction* GapAt(int index) const {
    return GapInstruction::cast(InstructionAt(index));
  }
  bool IsGapAt(int index) const { return InstructionAt(index)->IsGapMoves(); }
  Instruction* InstructionAt(int index) const {
    DCHECK(index >= 0);
    DCHECK(index < static_cast<int>(instructions_.size()));
    return instructions_[index];
  }

  Frame* frame() { return &frame_; }
  Graph* graph() const { return graph_; }
  Isolate* isolate() const { return zone()->isolate(); }
  Linkage* linkage() const { return linkage_; }
  Schedule* schedule() const { return schedule_; }
  const PointerMapDeque* pointer_maps() const { return &pointer_maps_; }
  Zone* zone() const { return graph_->zone(); }

  // Used by the code generator while adding instructions.
  int AddInstruction(Instruction* instr, BasicBlock* block);
  void StartBlock(BasicBlock* block);
  void EndBlock(BasicBlock* block);

  void AddConstant(int virtual_register, Constant constant) {
    DCHECK(constants_.find(virtual_register) == constants_.end());
    constants_.insert(std::make_pair(virtual_register, constant));
  }
  Constant GetConstant(int virtual_register) const {
    ConstantMap::const_iterator it = constants_.find(virtual_register);
    DCHECK(it != constants_.end());
    DCHECK_EQ(virtual_register, it->first);
    return it->second;
  }

  typedef ConstantDeque Immediates;
  const Immediates& immediates() const { return immediates_; }

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

 private:
  friend OStream& operator<<(OStream& os, const InstructionSequence& code);

  typedef std::set<int, std::less<int>, ZoneIntAllocator> VirtualRegisterSet;

  Graph* graph_;
  Linkage* linkage_;
  Schedule* schedule_;
  ConstantMap constants_;
  ConstantDeque immediates_;
  InstructionDeque instructions_;
  int next_virtual_register_;
  PointerMapDeque pointer_maps_;
  VirtualRegisterSet doubles_;
  VirtualRegisterSet references_;
  Frame frame_;
  DeoptimizationVector deoptimization_entries_;
};

OStream& operator<<(OStream& os, const InstructionSequence& code);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INSTRUCTION_H_

// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_LITHIUM_H_
#define V8_LITHIUM_H_

#include "allocation.h"
#include "hydrogen.h"
#include "safepoint-table.h"

namespace v8 {
namespace internal {

#define LITHIUM_OPERAND_LIST(V)         \
  V(ConstantOperand, CONSTANT_OPERAND)  \
  V(StackSlot,       STACK_SLOT)        \
  V(DoubleStackSlot, DOUBLE_STACK_SLOT) \
  V(Register,        REGISTER)          \
  V(DoubleRegister,  DOUBLE_REGISTER)


class LOperand: public ZoneObject {
 public:
  enum Kind {
    INVALID,
    UNALLOCATED,
    CONSTANT_OPERAND,
    STACK_SLOT,
    DOUBLE_STACK_SLOT,
    REGISTER,
    DOUBLE_REGISTER,
    ARGUMENT
  };

  LOperand() : value_(KindField::encode(INVALID)) { }

  Kind kind() const { return KindField::decode(value_); }
  int index() const { return static_cast<int>(value_) >> kKindFieldWidth; }
#define LITHIUM_OPERAND_PREDICATE(name, type) \
  bool Is##name() const { return kind() == type; }
  LITHIUM_OPERAND_LIST(LITHIUM_OPERAND_PREDICATE)
  LITHIUM_OPERAND_PREDICATE(Argument, ARGUMENT)
  LITHIUM_OPERAND_PREDICATE(Unallocated, UNALLOCATED)
  LITHIUM_OPERAND_PREDICATE(Ignored, INVALID)
#undef LITHIUM_OPERAND_PREDICATE
  bool Equals(LOperand* other) const { return value_ == other->value_; }

  void PrintTo(StringStream* stream);
  void ConvertTo(Kind kind, int index) {
    value_ = KindField::encode(kind);
    value_ |= index << kKindFieldWidth;
    ASSERT(this->index() == index);
  }

  // Calls SetUpCache()/TearDownCache() for each subclass.
  static void SetUpCaches();
  static void TearDownCaches();

 protected:
  static const int kKindFieldWidth = 3;
  class KindField : public BitField<Kind, 0, kKindFieldWidth> { };

  LOperand(Kind kind, int index) { ConvertTo(kind, index); }

  unsigned value_;
};


class LUnallocated: public LOperand {
 public:
  enum BasicPolicy {
    FIXED_SLOT,
    EXTENDED_POLICY
  };

  enum ExtendedPolicy {
    NONE,
    ANY,
    FIXED_REGISTER,
    FIXED_DOUBLE_REGISTER,
    MUST_HAVE_REGISTER,
    WRITABLE_REGISTER,
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

  explicit LUnallocated(ExtendedPolicy policy) : LOperand(UNALLOCATED, 0) {
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(USED_AT_END);
  }

  LUnallocated(BasicPolicy policy, int index) : LOperand(UNALLOCATED, 0) {
    ASSERT(policy == FIXED_SLOT);
    value_ |= BasicPolicyField::encode(policy);
    value_ |= index << FixedSlotIndexField::kShift;
    ASSERT(this->fixed_slot_index() == index);
  }

  LUnallocated(ExtendedPolicy policy, int index) : LOperand(UNALLOCATED, 0) {
    ASSERT(policy == FIXED_REGISTER || policy == FIXED_DOUBLE_REGISTER);
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(USED_AT_END);
    value_ |= FixedRegisterField::encode(index);
  }

  LUnallocated(ExtendedPolicy policy, Lifetime lifetime)
      : LOperand(UNALLOCATED, 0) {
    value_ |= BasicPolicyField::encode(EXTENDED_POLICY);
    value_ |= ExtendedPolicyField::encode(policy);
    value_ |= LifetimeField::encode(lifetime);
  }

  LUnallocated* CopyUnconstrained(Zone* zone) {
    LUnallocated* result = new(zone) LUnallocated(ANY);
    result->set_virtual_register(virtual_register());
    return result;
  }

  static LUnallocated* cast(LOperand* op) {
    ASSERT(op->IsUnallocated());
    return reinterpret_cast<LUnallocated*>(op);
  }

  // The encoding used for LUnallocated operands depends on the policy that is
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
  STATIC_ASSERT(kKindFieldWidth == 3);

  // BitFields for all unallocated operands.
  class BasicPolicyField     : public BitField<BasicPolicy,     3,  1> {};
  class VirtualRegisterField : public BitField<unsigned,        4, 18> {};

  // BitFields specific to BasicPolicy::FIXED_SLOT.
  class FixedSlotIndexField  : public BitField<int,            22, 10> {};

  // BitFields specific to BasicPolicy::EXTENDED_POLICY.
  class ExtendedPolicyField  : public BitField<ExtendedPolicy, 22,  3> {};
  class LifetimeField        : public BitField<Lifetime,       25,  1> {};
  class FixedRegisterField   : public BitField<int,            26,  6> {};

  static const int kMaxVirtualRegisters = VirtualRegisterField::kMax + 1;
  static const int kFixedSlotIndexWidth = FixedSlotIndexField::kSize;
  static const int kMaxFixedSlotIndex = (1 << (kFixedSlotIndexWidth - 1)) - 1;
  static const int kMinFixedSlotIndex = -(1 << (kFixedSlotIndexWidth - 1));

  // Predicates for the operand policy.
  bool HasAnyPolicy() const {
    return basic_policy() == EXTENDED_POLICY &&
        extended_policy() == ANY;
  }
  bool HasFixedPolicy() const {
    return basic_policy() == FIXED_SLOT ||
        extended_policy() == FIXED_REGISTER ||
        extended_policy() == FIXED_DOUBLE_REGISTER;
  }
  bool HasRegisterPolicy() const {
    return basic_policy() == EXTENDED_POLICY && (
        extended_policy() == WRITABLE_REGISTER ||
        extended_policy() == MUST_HAVE_REGISTER);
  }
  bool HasSameAsInputPolicy() const {
    return basic_policy() == EXTENDED_POLICY &&
        extended_policy() == SAME_AS_FIRST_INPUT;
  }
  bool HasFixedSlotPolicy() const {
    return basic_policy() == FIXED_SLOT;
  }
  bool HasFixedRegisterPolicy() const {
    return basic_policy() == EXTENDED_POLICY &&
        extended_policy() == FIXED_REGISTER;
  }
  bool HasFixedDoubleRegisterPolicy() const {
    return basic_policy() == EXTENDED_POLICY &&
        extended_policy() == FIXED_DOUBLE_REGISTER;
  }
  bool HasWritableRegisterPolicy() const {
    return basic_policy() == EXTENDED_POLICY &&
        extended_policy() == WRITABLE_REGISTER;
  }

  // [basic_policy]: Distinguish between FIXED_SLOT and all other policies.
  BasicPolicy basic_policy() const {
    return BasicPolicyField::decode(value_);
  }

  // [extended_policy]: Only for non-FIXED_SLOT. The finer-grained policy.
  ExtendedPolicy extended_policy() const {
    ASSERT(basic_policy() == EXTENDED_POLICY);
    return ExtendedPolicyField::decode(value_);
  }

  // [fixed_slot_index]: Only for FIXED_SLOT.
  int fixed_slot_index() const {
    ASSERT(HasFixedSlotPolicy());
    return static_cast<int>(value_) >> FixedSlotIndexField::kShift;
  }

  // [fixed_register_index]: Only for FIXED_REGISTER or FIXED_DOUBLE_REGISTER.
  int fixed_register_index() const {
    ASSERT(HasFixedRegisterPolicy() || HasFixedDoubleRegisterPolicy());
    return FixedRegisterField::decode(value_);
  }

  // [virtual_register]: The virtual register ID for this operand.
  int virtual_register() const {
    return VirtualRegisterField::decode(value_);
  }
  void set_virtual_register(unsigned id) {
    value_ = VirtualRegisterField::update(value_, id);
  }

  // [lifetime]: Only for non-FIXED_SLOT.
  bool IsUsedAtStart() {
    ASSERT(basic_policy() == EXTENDED_POLICY);
    return LifetimeField::decode(value_) == USED_AT_START;
  }
};


class LMoveOperands BASE_EMBEDDED {
 public:
  LMoveOperands(LOperand* source, LOperand* destination)
      : source_(source), destination_(destination) {
  }

  LOperand* source() const { return source_; }
  void set_source(LOperand* operand) { source_ = operand; }

  LOperand* destination() const { return destination_; }
  void set_destination(LOperand* operand) { destination_ = operand; }

  // The gap resolver marks moves as "in-progress" by clearing the
  // destination (but not the source).
  bool IsPending() const {
    return destination_ == NULL && source_ != NULL;
  }

  // True if this move a move into the given destination operand.
  bool Blocks(LOperand* operand) const {
    return !IsEliminated() && source()->Equals(operand);
  }

  // A move is redundant if it's been eliminated, if its source and
  // destination are the same, or if its destination is unneeded.
  bool IsRedundant() const {
    return IsEliminated() || source_->Equals(destination_) || IsIgnored();
  }

  bool IsIgnored() const {
    return destination_ != NULL && destination_->IsIgnored();
  }

  // We clear both operands to indicate move that's been eliminated.
  void Eliminate() { source_ = destination_ = NULL; }
  bool IsEliminated() const {
    ASSERT(source_ != NULL || destination_ == NULL);
    return source_ == NULL;
  }

 private:
  LOperand* source_;
  LOperand* destination_;
};


class LConstantOperand: public LOperand {
 public:
  static LConstantOperand* Create(int index, Zone* zone) {
    ASSERT(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new(zone) LConstantOperand(index);
  }

  static LConstantOperand* cast(LOperand* op) {
    ASSERT(op->IsConstantOperand());
    return reinterpret_cast<LConstantOperand*>(op);
  }

  static void SetUpCache();
  static void TearDownCache();

 private:
  static const int kNumCachedOperands = 128;
  static LConstantOperand* cache;

  LConstantOperand() : LOperand() { }
  explicit LConstantOperand(int index) : LOperand(CONSTANT_OPERAND, index) { }
};


class LArgument: public LOperand {
 public:
  explicit LArgument(int index) : LOperand(ARGUMENT, index) { }

  static LArgument* cast(LOperand* op) {
    ASSERT(op->IsArgument());
    return reinterpret_cast<LArgument*>(op);
  }
};


class LStackSlot: public LOperand {
 public:
  static LStackSlot* Create(int index, Zone* zone) {
    ASSERT(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new(zone) LStackSlot(index);
  }

  static LStackSlot* cast(LOperand* op) {
    ASSERT(op->IsStackSlot());
    return reinterpret_cast<LStackSlot*>(op);
  }

  static void SetUpCache();
  static void TearDownCache();

 private:
  static const int kNumCachedOperands = 128;
  static LStackSlot* cache;

  LStackSlot() : LOperand() { }
  explicit LStackSlot(int index) : LOperand(STACK_SLOT, index) { }
};


class LDoubleStackSlot: public LOperand {
 public:
  static LDoubleStackSlot* Create(int index, Zone* zone) {
    ASSERT(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new(zone) LDoubleStackSlot(index);
  }

  static LDoubleStackSlot* cast(LOperand* op) {
    ASSERT(op->IsStackSlot());
    return reinterpret_cast<LDoubleStackSlot*>(op);
  }

  static void SetUpCache();
  static void TearDownCache();

 private:
  static const int kNumCachedOperands = 128;
  static LDoubleStackSlot* cache;

  LDoubleStackSlot() : LOperand() { }
  explicit LDoubleStackSlot(int index) : LOperand(DOUBLE_STACK_SLOT, index) { }
};


class LRegister: public LOperand {
 public:
  static LRegister* Create(int index, Zone* zone) {
    ASSERT(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new(zone) LRegister(index);
  }

  static LRegister* cast(LOperand* op) {
    ASSERT(op->IsRegister());
    return reinterpret_cast<LRegister*>(op);
  }

  static void SetUpCache();
  static void TearDownCache();

 private:
  static const int kNumCachedOperands = 16;
  static LRegister* cache;

  LRegister() : LOperand() { }
  explicit LRegister(int index) : LOperand(REGISTER, index) { }
};


class LDoubleRegister: public LOperand {
 public:
  static LDoubleRegister* Create(int index, Zone* zone) {
    ASSERT(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new(zone) LDoubleRegister(index);
  }

  static LDoubleRegister* cast(LOperand* op) {
    ASSERT(op->IsDoubleRegister());
    return reinterpret_cast<LDoubleRegister*>(op);
  }

  static void SetUpCache();
  static void TearDownCache();

 private:
  static const int kNumCachedOperands = 16;
  static LDoubleRegister* cache;

  LDoubleRegister() : LOperand() { }
  explicit LDoubleRegister(int index) : LOperand(DOUBLE_REGISTER, index) { }
};


class LParallelMove : public ZoneObject {
 public:
  explicit LParallelMove(Zone* zone) : move_operands_(4, zone) { }

  void AddMove(LOperand* from, LOperand* to, Zone* zone) {
    move_operands_.Add(LMoveOperands(from, to), zone);
  }

  bool IsRedundant() const;

  const ZoneList<LMoveOperands>* move_operands() const {
    return &move_operands_;
  }

  void PrintDataTo(StringStream* stream) const;

 private:
  ZoneList<LMoveOperands> move_operands_;
};


class LPointerMap: public ZoneObject {
 public:
  explicit LPointerMap(int position, Zone* zone)
      : pointer_operands_(8, zone),
        untagged_operands_(0, zone),
        position_(position),
        lithium_position_(-1) { }

  const ZoneList<LOperand*>* GetNormalizedOperands() {
    for (int i = 0; i < untagged_operands_.length(); ++i) {
      RemovePointer(untagged_operands_[i]);
    }
    untagged_operands_.Clear();
    return &pointer_operands_;
  }
  int position() const { return position_; }
  int lithium_position() const { return lithium_position_; }

  void set_lithium_position(int pos) {
    ASSERT(lithium_position_ == -1);
    lithium_position_ = pos;
  }

  void RecordPointer(LOperand* op, Zone* zone);
  void RemovePointer(LOperand* op);
  void RecordUntagged(LOperand* op, Zone* zone);
  void PrintTo(StringStream* stream);

 private:
  ZoneList<LOperand*> pointer_operands_;
  ZoneList<LOperand*> untagged_operands_;
  int position_;
  int lithium_position_;
};


class LEnvironment: public ZoneObject {
 public:
  LEnvironment(Handle<JSFunction> closure,
               FrameType frame_type,
               BailoutId ast_id,
               int parameter_count,
               int argument_count,
               int value_count,
               LEnvironment* outer,
               HEnterInlined* entry,
               Zone* zone)
      : closure_(closure),
        frame_type_(frame_type),
        arguments_stack_height_(argument_count),
        deoptimization_index_(Safepoint::kNoDeoptimizationIndex),
        translation_index_(-1),
        ast_id_(ast_id),
        parameter_count_(parameter_count),
        pc_offset_(-1),
        values_(value_count, zone),
        is_tagged_(value_count, zone),
        is_uint32_(value_count, zone),
        spilled_registers_(NULL),
        spilled_double_registers_(NULL),
        outer_(outer),
        entry_(entry),
        zone_(zone) { }

  Handle<JSFunction> closure() const { return closure_; }
  FrameType frame_type() const { return frame_type_; }
  int arguments_stack_height() const { return arguments_stack_height_; }
  int deoptimization_index() const { return deoptimization_index_; }
  int translation_index() const { return translation_index_; }
  BailoutId ast_id() const { return ast_id_; }
  int parameter_count() const { return parameter_count_; }
  int pc_offset() const { return pc_offset_; }
  LOperand** spilled_registers() const { return spilled_registers_; }
  LOperand** spilled_double_registers() const {
    return spilled_double_registers_;
  }
  const ZoneList<LOperand*>* values() const { return &values_; }
  LEnvironment* outer() const { return outer_; }
  HEnterInlined* entry() { return entry_; }

  void AddValue(LOperand* operand,
                Representation representation,
                bool is_uint32) {
    values_.Add(operand, zone());
    if (representation.IsSmiOrTagged()) {
      ASSERT(!is_uint32);
      is_tagged_.Add(values_.length() - 1);
    }

    if (is_uint32) {
      is_uint32_.Add(values_.length() - 1);
    }
  }

  bool HasTaggedValueAt(int index) const {
    return is_tagged_.Contains(index);
  }

  bool HasUint32ValueAt(int index) const {
    return is_uint32_.Contains(index);
  }

  void Register(int deoptimization_index,
                int translation_index,
                int pc_offset) {
    ASSERT(!HasBeenRegistered());
    deoptimization_index_ = deoptimization_index;
    translation_index_ = translation_index;
    pc_offset_ = pc_offset;
  }
  bool HasBeenRegistered() const {
    return deoptimization_index_ != Safepoint::kNoDeoptimizationIndex;
  }

  void SetSpilledRegisters(LOperand** registers,
                           LOperand** double_registers) {
    spilled_registers_ = registers;
    spilled_double_registers_ = double_registers;
  }

  void PrintTo(StringStream* stream);

  Zone* zone() const { return zone_; }

 private:
  Handle<JSFunction> closure_;
  FrameType frame_type_;
  int arguments_stack_height_;
  int deoptimization_index_;
  int translation_index_;
  BailoutId ast_id_;
  int parameter_count_;
  int pc_offset_;
  ZoneList<LOperand*> values_;
  BitVector is_tagged_;
  BitVector is_uint32_;

  // Allocation index indexed arrays of spill slot operands for registers
  // that are also in spill slots at an OSR entry.  NULL for environments
  // that do not correspond to an OSR entry.
  LOperand** spilled_registers_;
  LOperand** spilled_double_registers_;

  LEnvironment* outer_;
  HEnterInlined* entry_;

  Zone* zone_;
};


// Iterates over the non-null, non-constant operands in an environment.
class ShallowIterator BASE_EMBEDDED {
 public:
  explicit ShallowIterator(LEnvironment* env)
      : env_(env),
        limit_(env != NULL ? env->values()->length() : 0),
        current_(0) {
    SkipUninteresting();
  }

  bool Done() { return current_ >= limit_; }

  LOperand* Current() {
    ASSERT(!Done());
    ASSERT(env_->values()->at(current_) != NULL);
    return env_->values()->at(current_);
  }

  void Advance() {
    ASSERT(!Done());
    ++current_;
    SkipUninteresting();
  }

  LEnvironment* env() { return env_; }

 private:
  bool ShouldSkip(LOperand* op) {
    return op == NULL || op->IsConstantOperand() || op->IsArgument();
  }

  // Skip until something interesting, beginning with and including current_.
  void SkipUninteresting() {
    while (current_ < limit_ && ShouldSkip(env_->values()->at(current_))) {
      ++current_;
    }
  }

  LEnvironment* env_;
  int limit_;
  int current_;
};


// Iterator for non-null, non-constant operands incl. outer environments.
class DeepIterator BASE_EMBEDDED {
 public:
  explicit DeepIterator(LEnvironment* env)
      : current_iterator_(env) {
    SkipUninteresting();
  }

  bool Done() { return current_iterator_.Done(); }

  LOperand* Current() {
    ASSERT(!current_iterator_.Done());
    ASSERT(current_iterator_.Current() != NULL);
    return current_iterator_.Current();
  }

  void Advance() {
    current_iterator_.Advance();
    SkipUninteresting();
  }

 private:
  void SkipUninteresting() {
    while (current_iterator_.env() != NULL && current_iterator_.Done()) {
      current_iterator_ = ShallowIterator(current_iterator_.env()->outer());
    }
  }

  ShallowIterator current_iterator_;
};


class LPlatformChunk;
class LGap;
class LLabel;

// Superclass providing data and behavior common to all the
// arch-specific LPlatformChunk classes.
class LChunk: public ZoneObject {
 public:
  static LChunk* NewChunk(HGraph* graph);

  void AddInstruction(LInstruction* instruction, HBasicBlock* block);
  LConstantOperand* DefineConstantOperand(HConstant* constant);
  HConstant* LookupConstant(LConstantOperand* operand) const;
  Representation LookupLiteralRepresentation(LConstantOperand* operand) const;

  int ParameterAt(int index);
  int GetParameterStackSlot(int index) const;
  int spill_slot_count() const { return spill_slot_count_; }
  CompilationInfo* info() const { return info_; }
  HGraph* graph() const { return graph_; }
  Isolate* isolate() const { return graph_->isolate(); }
  const ZoneList<LInstruction*>* instructions() const { return &instructions_; }
  void AddGapMove(int index, LOperand* from, LOperand* to);
  LGap* GetGapAt(int index) const;
  bool IsGapAt(int index) const;
  int NearestGapPos(int index) const;
  void MarkEmptyBlocks();
  const ZoneList<LPointerMap*>* pointer_maps() const { return &pointer_maps_; }
  LLabel* GetLabel(int block_id) const;
  int LookupDestination(int block_id) const;
  Label* GetAssemblyLabel(int block_id) const;

  const ZoneList<Handle<JSFunction> >* inlined_closures() const {
    return &inlined_closures_;
  }

  void AddInlinedClosure(Handle<JSFunction> closure) {
    inlined_closures_.Add(closure, zone());
  }

  Zone* zone() const { return info_->zone(); }

  Handle<Code> Codegen();

  void set_allocated_double_registers(BitVector* allocated_registers);
  BitVector* allocated_double_registers() {
    return allocated_double_registers_;
  }

 protected:
  LChunk(CompilationInfo* info, HGraph* graph);

  int spill_slot_count_;

 private:
  CompilationInfo* info_;
  HGraph* const graph_;
  BitVector* allocated_double_registers_;
  ZoneList<LInstruction*> instructions_;
  ZoneList<LPointerMap*> pointer_maps_;
  ZoneList<Handle<JSFunction> > inlined_closures_;
};


int ElementsKindToShiftSize(ElementsKind elements_kind);
int StackSlotOffset(int index);

enum NumberUntagDMode {
  NUMBER_CANDIDATE_IS_SMI,
  NUMBER_CANDIDATE_IS_ANY_TAGGED,
  NUMBER_CANDIDATE_IS_ANY_TAGGED_CONVERT_HOLE
};


} }  // namespace v8::internal

#endif  // V8_LITHIUM_H_

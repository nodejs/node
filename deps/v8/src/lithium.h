// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LITHIUM_H_
#define V8_LITHIUM_H_

#include <set>

#include "src/allocation.h"
#include "src/bailout-reason.h"
#include "src/hydrogen.h"
#include "src/safepoint-table.h"
#include "src/zone-allocator.h"

namespace v8 {
namespace internal {

#define LITHIUM_OPERAND_LIST(V)               \
  V(ConstantOperand, CONSTANT_OPERAND,  128)  \
  V(StackSlot,       STACK_SLOT,        128)  \
  V(DoubleStackSlot, DOUBLE_STACK_SLOT, 128)  \
  V(Register,        REGISTER,          16)   \
  V(DoubleRegister,  DOUBLE_REGISTER,   16)

class LOperand : public ZoneObject {
 public:
  enum Kind {
    INVALID,
    UNALLOCATED,
    CONSTANT_OPERAND,
    STACK_SLOT,
    DOUBLE_STACK_SLOT,
    REGISTER,
    DOUBLE_REGISTER
  };

  LOperand() : value_(KindField::encode(INVALID)) { }

  Kind kind() const { return KindField::decode(value_); }
  int index() const { return static_cast<int>(value_) >> kKindFieldWidth; }
#define LITHIUM_OPERAND_PREDICATE(name, type, number) \
  bool Is##name() const { return kind() == type; }
  LITHIUM_OPERAND_LIST(LITHIUM_OPERAND_PREDICATE)
  LITHIUM_OPERAND_PREDICATE(Unallocated, UNALLOCATED, 0)
  LITHIUM_OPERAND_PREDICATE(Ignored, INVALID, 0)
#undef LITHIUM_OPERAND_PREDICATE
  bool Equals(LOperand* other) const { return value_ == other->value_; }

  void PrintTo(StringStream* stream);
  void ConvertTo(Kind kind, int index) {
    if (kind == REGISTER) DCHECK(index >= 0);
    value_ = KindField::encode(kind);
    value_ |= index << kKindFieldWidth;
    DCHECK(this->index() == index);
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


class LUnallocated : public LOperand {
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
    MUST_HAVE_DOUBLE_REGISTER,
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
    DCHECK(policy == FIXED_SLOT);
    value_ |= BasicPolicyField::encode(policy);
    value_ |= index << FixedSlotIndexField::kShift;
    DCHECK(this->fixed_slot_index() == index);
  }

  LUnallocated(ExtendedPolicy policy, int index) : LOperand(UNALLOCATED, 0) {
    DCHECK(policy == FIXED_REGISTER || policy == FIXED_DOUBLE_REGISTER);
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
    DCHECK(op->IsUnallocated());
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
  bool HasDoubleRegisterPolicy() const {
    return basic_policy() == EXTENDED_POLICY &&
        extended_policy() == MUST_HAVE_DOUBLE_REGISTER;
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
  int virtual_register() const {
    return VirtualRegisterField::decode(value_);
  }
  void set_virtual_register(unsigned id) {
    value_ = VirtualRegisterField::update(value_, id);
  }

  // [lifetime]: Only for non-FIXED_SLOT.
  bool IsUsedAtStart() {
    DCHECK(basic_policy() == EXTENDED_POLICY);
    return LifetimeField::decode(value_) == USED_AT_START;
  }
};


class LMoveOperands FINAL BASE_EMBEDDED {
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
  // destination are the same, or if its destination is unneeded or constant.
  bool IsRedundant() const {
    return IsEliminated() || source_->Equals(destination_) || IsIgnored() ||
           (destination_ != NULL && destination_->IsConstantOperand());
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
  LOperand* source_;
  LOperand* destination_;
};


template<LOperand::Kind kOperandKind, int kNumCachedOperands>
class LSubKindOperand FINAL : public LOperand {
 public:
  static LSubKindOperand* Create(int index, Zone* zone) {
    DCHECK(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new(zone) LSubKindOperand(index);
  }

  static LSubKindOperand* cast(LOperand* op) {
    DCHECK(op->kind() == kOperandKind);
    return reinterpret_cast<LSubKindOperand*>(op);
  }

  static void SetUpCache();
  static void TearDownCache();

 private:
  static LSubKindOperand* cache;

  LSubKindOperand() : LOperand() { }
  explicit LSubKindOperand(int index) : LOperand(kOperandKind, index) { }
};


#define LITHIUM_TYPEDEF_SUBKIND_OPERAND_CLASS(name, type, number)   \
typedef LSubKindOperand<LOperand::type, number> L##name;
LITHIUM_OPERAND_LIST(LITHIUM_TYPEDEF_SUBKIND_OPERAND_CLASS)
#undef LITHIUM_TYPEDEF_SUBKIND_OPERAND_CLASS


class LParallelMove FINAL : public ZoneObject {
 public:
  explicit LParallelMove(Zone* zone) : move_operands_(4, zone) { }

  void AddMove(LOperand* from, LOperand* to, Zone* zone) {
    move_operands_.Add(LMoveOperands(from, to), zone);
  }

  bool IsRedundant() const;

  ZoneList<LMoveOperands>* move_operands() { return &move_operands_; }

  void PrintDataTo(StringStream* stream) const;

 private:
  ZoneList<LMoveOperands> move_operands_;
};


class LPointerMap FINAL : public ZoneObject {
 public:
  explicit LPointerMap(Zone* zone)
      : pointer_operands_(8, zone),
        untagged_operands_(0, zone),
        lithium_position_(-1) { }

  const ZoneList<LOperand*>* GetNormalizedOperands() {
    for (int i = 0; i < untagged_operands_.length(); ++i) {
      RemovePointer(untagged_operands_[i]);
    }
    untagged_operands_.Clear();
    return &pointer_operands_;
  }
  int lithium_position() const { return lithium_position_; }

  void set_lithium_position(int pos) {
    DCHECK(lithium_position_ == -1);
    lithium_position_ = pos;
  }

  void RecordPointer(LOperand* op, Zone* zone);
  void RemovePointer(LOperand* op);
  void RecordUntagged(LOperand* op, Zone* zone);
  void PrintTo(StringStream* stream);

 private:
  ZoneList<LOperand*> pointer_operands_;
  ZoneList<LOperand*> untagged_operands_;
  int lithium_position_;
};


class LEnvironment FINAL : public ZoneObject {
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
        translation_size_(value_count),
        parameter_count_(parameter_count),
        pc_offset_(-1),
        values_(value_count, zone),
        is_tagged_(value_count, zone),
        is_uint32_(value_count, zone),
        object_mapping_(0, zone),
        outer_(outer),
        entry_(entry),
        zone_(zone),
        has_been_used_(false) { }

  Handle<JSFunction> closure() const { return closure_; }
  FrameType frame_type() const { return frame_type_; }
  int arguments_stack_height() const { return arguments_stack_height_; }
  int deoptimization_index() const { return deoptimization_index_; }
  int translation_index() const { return translation_index_; }
  BailoutId ast_id() const { return ast_id_; }
  int translation_size() const { return translation_size_; }
  int parameter_count() const { return parameter_count_; }
  int pc_offset() const { return pc_offset_; }
  const ZoneList<LOperand*>* values() const { return &values_; }
  LEnvironment* outer() const { return outer_; }
  HEnterInlined* entry() { return entry_; }
  Zone* zone() const { return zone_; }

  bool has_been_used() const { return has_been_used_; }
  void set_has_been_used() { has_been_used_ = true; }

  void AddValue(LOperand* operand,
                Representation representation,
                bool is_uint32) {
    values_.Add(operand, zone());
    if (representation.IsSmiOrTagged()) {
      DCHECK(!is_uint32);
      is_tagged_.Add(values_.length() - 1, zone());
    }

    if (is_uint32) {
      is_uint32_.Add(values_.length() - 1, zone());
    }
  }

  bool HasTaggedValueAt(int index) const {
    return is_tagged_.Contains(index);
  }

  bool HasUint32ValueAt(int index) const {
    return is_uint32_.Contains(index);
  }

  void AddNewObject(int length, bool is_arguments) {
    uint32_t encoded = LengthOrDupeField::encode(length) |
                       IsArgumentsField::encode(is_arguments) |
                       IsDuplicateField::encode(false);
    object_mapping_.Add(encoded, zone());
  }

  void AddDuplicateObject(int dupe_of) {
    uint32_t encoded = LengthOrDupeField::encode(dupe_of) |
                       IsDuplicateField::encode(true);
    object_mapping_.Add(encoded, zone());
  }

  int ObjectDuplicateOfAt(int index) {
    DCHECK(ObjectIsDuplicateAt(index));
    return LengthOrDupeField::decode(object_mapping_[index]);
  }

  int ObjectLengthAt(int index) {
    DCHECK(!ObjectIsDuplicateAt(index));
    return LengthOrDupeField::decode(object_mapping_[index]);
  }

  bool ObjectIsArgumentsAt(int index) {
    DCHECK(!ObjectIsDuplicateAt(index));
    return IsArgumentsField::decode(object_mapping_[index]);
  }

  bool ObjectIsDuplicateAt(int index) {
    return IsDuplicateField::decode(object_mapping_[index]);
  }

  void Register(int deoptimization_index,
                int translation_index,
                int pc_offset) {
    DCHECK(!HasBeenRegistered());
    deoptimization_index_ = deoptimization_index;
    translation_index_ = translation_index;
    pc_offset_ = pc_offset;
  }
  bool HasBeenRegistered() const {
    return deoptimization_index_ != Safepoint::kNoDeoptimizationIndex;
  }

  void PrintTo(StringStream* stream);

  // Marker value indicating a de-materialized object.
  static LOperand* materialization_marker() { return NULL; }

  // Encoding used for the object_mapping map below.
  class LengthOrDupeField : public BitField<int,   0, 30> { };
  class IsArgumentsField  : public BitField<bool, 30,  1> { };
  class IsDuplicateField  : public BitField<bool, 31,  1> { };

 private:
  Handle<JSFunction> closure_;
  FrameType frame_type_;
  int arguments_stack_height_;
  int deoptimization_index_;
  int translation_index_;
  BailoutId ast_id_;
  int translation_size_;
  int parameter_count_;
  int pc_offset_;

  // Value array: [parameters] [locals] [expression stack] [de-materialized].
  //              |>--------- translation_size ---------<|
  ZoneList<LOperand*> values_;
  GrowableBitVector is_tagged_;
  GrowableBitVector is_uint32_;

  // Map with encoded information about materialization_marker operands.
  ZoneList<uint32_t> object_mapping_;

  LEnvironment* outer_;
  HEnterInlined* entry_;
  Zone* zone_;
  bool has_been_used_;
};


// Iterates over the non-null, non-constant operands in an environment.
class ShallowIterator FINAL BASE_EMBEDDED {
 public:
  explicit ShallowIterator(LEnvironment* env)
      : env_(env),
        limit_(env != NULL ? env->values()->length() : 0),
        current_(0) {
    SkipUninteresting();
  }

  bool Done() { return current_ >= limit_; }

  LOperand* Current() {
    DCHECK(!Done());
    DCHECK(env_->values()->at(current_) != NULL);
    return env_->values()->at(current_);
  }

  void Advance() {
    DCHECK(!Done());
    ++current_;
    SkipUninteresting();
  }

  LEnvironment* env() { return env_; }

 private:
  bool ShouldSkip(LOperand* op) {
    return op == NULL || op->IsConstantOperand();
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
class DeepIterator FINAL BASE_EMBEDDED {
 public:
  explicit DeepIterator(LEnvironment* env)
      : current_iterator_(env) {
    SkipUninteresting();
  }

  bool Done() { return current_iterator_.Done(); }

  LOperand* Current() {
    DCHECK(!current_iterator_.Done());
    DCHECK(current_iterator_.Current() != NULL);
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
class LChunk : public ZoneObject {
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

  void AddDeprecationDependency(Handle<Map> map) {
    DCHECK(!map->is_deprecated());
    if (!map->CanBeDeprecated()) return;
    DCHECK(!info_->IsStub());
    deprecation_dependencies_.insert(map);
  }

  void AddStabilityDependency(Handle<Map> map) {
    DCHECK(map->is_stable());
    if (!map->CanTransition()) return;
    DCHECK(!info_->IsStub());
    stability_dependencies_.insert(map);
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
  typedef std::less<Handle<Map> > MapLess;
  typedef zone_allocator<Handle<Map> > MapAllocator;
  typedef std::set<Handle<Map>, MapLess, MapAllocator> MapSet;

  void CommitDependencies(Handle<Code> code) const;

  CompilationInfo* info_;
  HGraph* const graph_;
  BitVector* allocated_double_registers_;
  ZoneList<LInstruction*> instructions_;
  ZoneList<LPointerMap*> pointer_maps_;
  ZoneList<Handle<JSFunction> > inlined_closures_;
  MapSet deprecation_dependencies_;
  MapSet stability_dependencies_;
};


class LChunkBuilderBase BASE_EMBEDDED {
 public:
  explicit LChunkBuilderBase(CompilationInfo* info, HGraph* graph)
      : argument_count_(0),
        chunk_(NULL),
        info_(info),
        graph_(graph),
        status_(UNUSED),
        zone_(graph->zone()) {}

  virtual ~LChunkBuilderBase() { }

  void Abort(BailoutReason reason);
  void Retry(BailoutReason reason);

 protected:
  enum Status { UNUSED, BUILDING, DONE, ABORTED };

  LPlatformChunk* chunk() const { return chunk_; }
  CompilationInfo* info() const { return info_; }
  HGraph* graph() const { return graph_; }
  int argument_count() const { return argument_count_; }
  Isolate* isolate() const { return graph_->isolate(); }
  Heap* heap() const { return isolate()->heap(); }

  bool is_unused() const { return status_ == UNUSED; }
  bool is_building() const { return status_ == BUILDING; }
  bool is_done() const { return status_ == DONE; }
  bool is_aborted() const { return status_ == ABORTED; }

  // An input operand in register, stack slot or a constant operand.
  // Will not be moved to a register even if one is freely available.
  virtual MUST_USE_RESULT LOperand* UseAny(HValue* value) = 0;

  LEnvironment* CreateEnvironment(HEnvironment* hydrogen_env,
                                  int* argument_index_accumulator,
                                  ZoneList<HValue*>* objects_to_materialize);
  void AddObjectToMaterialize(HValue* value,
                              ZoneList<HValue*>* objects_to_materialize,
                              LEnvironment* result);

  Zone* zone() const { return zone_; }

  int argument_count_;
  LPlatformChunk* chunk_;
  CompilationInfo* info_;
  HGraph* const graph_;
  Status status_;

 private:
  Zone* zone_;
};


int StackSlotOffset(int index);

enum NumberUntagDMode {
  NUMBER_CANDIDATE_IS_SMI,
  NUMBER_CANDIDATE_IS_ANY_TAGGED
};


class LPhase : public CompilationPhase {
 public:
  LPhase(const char* name, LChunk* chunk)
      : CompilationPhase(name, chunk->info()),
        chunk_(chunk) { }
  ~LPhase();

 private:
  LChunk* chunk_;

  DISALLOW_COPY_AND_ASSIGN(LPhase);
};


// A register-allocator view of a Lithium instruction. It contains the id of
// the output operand and a list of input operand uses.

enum RegisterKind {
  UNALLOCATED_REGISTERS,
  GENERAL_REGISTERS,
  DOUBLE_REGISTERS
};

// Iterator for non-null temp operands.
class TempIterator BASE_EMBEDDED {
 public:
  inline explicit TempIterator(LInstruction* instr);
  inline bool Done();
  inline LOperand* Current();
  inline void Advance();

 private:
  inline void SkipUninteresting();
  LInstruction* instr_;
  int limit_;
  int current_;
};


// Iterator for non-constant input operands.
class InputIterator BASE_EMBEDDED {
 public:
  inline explicit InputIterator(LInstruction* instr);
  inline bool Done();
  inline LOperand* Current();
  inline void Advance();

 private:
  inline void SkipUninteresting();
  LInstruction* instr_;
  int limit_;
  int current_;
};


class UseIterator BASE_EMBEDDED {
 public:
  inline explicit UseIterator(LInstruction* instr);
  inline bool Done();
  inline LOperand* Current();
  inline void Advance();

 private:
  InputIterator input_iterator_;
  DeepIterator env_iterator_;
};

class LInstruction;
class LCodeGen;
} }  // namespace v8::internal

#endif  // V8_LITHIUM_H_

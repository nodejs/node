// Copyright 2010 the V8 project authors. All rights reserved.
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

#ifndef V8_LITHIUM_ALLOCATOR_H_
#define V8_LITHIUM_ALLOCATOR_H_

#include "v8.h"

#include "data-flow.h"
#include "zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
class HBasicBlock;
class HGraph;
class HInstruction;
class HPhi;
class HTracer;
class HValue;
class BitVector;
class StringStream;

class LArgument;
class LChunk;
class LConstantOperand;
class LGap;
class LParallelMove;
class LPointerMap;
class LStackSlot;
class LRegister;


// This class represents a single point of a LOperand's lifetime.
// For each lithium instruction there are exactly two lifetime positions:
// the beginning and the end of the instruction. Lifetime positions for
// different lithium instructions are disjoint.
class LifetimePosition {
 public:
  // Return the lifetime position that corresponds to the beginning of
  // the instruction with the given index.
  static LifetimePosition FromInstructionIndex(int index) {
    return LifetimePosition(index * kStep);
  }

  // Returns a numeric representation of this lifetime position.
  int Value() const {
    return value_;
  }

  // Returns the index of the instruction to which this lifetime position
  // corresponds.
  int InstructionIndex() const {
    ASSERT(IsValid());
    return value_ / kStep;
  }

  // Returns true if this lifetime position corresponds to the instruction
  // start.
  bool IsInstructionStart() const {
    return (value_ & (kStep - 1)) == 0;
  }

  // Returns the lifetime position for the start of the instruction which
  // corresponds to this lifetime position.
  LifetimePosition InstructionStart() const {
    ASSERT(IsValid());
    return LifetimePosition(value_ & ~(kStep - 1));
  }

  // Returns the lifetime position for the end of the instruction which
  // corresponds to this lifetime position.
  LifetimePosition InstructionEnd() const {
    ASSERT(IsValid());
    return LifetimePosition(InstructionStart().Value() + kStep/2);
  }

  // Returns the lifetime position for the beginning of the next instruction.
  LifetimePosition NextInstruction() const {
    ASSERT(IsValid());
    return LifetimePosition(InstructionStart().Value() + kStep);
  }

  // Returns the lifetime position for the beginning of the previous
  // instruction.
  LifetimePosition PrevInstruction() const {
    ASSERT(IsValid());
    ASSERT(value_ > 1);
    return LifetimePosition(InstructionStart().Value() - kStep);
  }

  // Constructs the lifetime position which does not correspond to any
  // instruction.
  LifetimePosition() : value_(-1) {}

  // Returns true if this lifetime positions corrensponds to some
  // instruction.
  bool IsValid() const { return value_ != -1; }

  static inline LifetimePosition Invalid() { return LifetimePosition(); }

  static inline LifetimePosition MaxPosition() {
    // We have to use this kind of getter instead of static member due to
    // crash bug in GDB.
    return LifetimePosition(kMaxInt);
  }

 private:
  static const int kStep = 2;

  // Code relies on kStep being a power of two.
  STATIC_ASSERT(IS_POWER_OF_TWO(kStep));

  explicit LifetimePosition(int value) : value_(value) { }

  int value_;
};


enum RegisterKind {
  NONE,
  GENERAL_REGISTERS,
  DOUBLE_REGISTERS
};


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
  bool IsConstantOperand() const { return kind() == CONSTANT_OPERAND; }
  bool IsStackSlot() const { return kind() == STACK_SLOT; }
  bool IsDoubleStackSlot() const { return kind() == DOUBLE_STACK_SLOT; }
  bool IsRegister() const { return kind() == REGISTER; }
  bool IsDoubleRegister() const { return kind() == DOUBLE_REGISTER; }
  bool IsArgument() const { return kind() == ARGUMENT; }
  bool IsUnallocated() const { return kind() == UNALLOCATED; }
  bool Equals(LOperand* other) const { return value_ == other->value_; }
  int VirtualRegister();

  void PrintTo(StringStream* stream);
  void ConvertTo(Kind kind, int index) {
    value_ = KindField::encode(kind);
    value_ |= index << kKindFieldWidth;
    ASSERT(this->index() == index);
  }

 protected:
  static const int kKindFieldWidth = 3;
  class KindField : public BitField<Kind, 0, kKindFieldWidth> { };

  LOperand(Kind kind, int index) { ConvertTo(kind, index); }

  unsigned value_;
};


class LUnallocated: public LOperand {
 public:
  enum Policy {
    NONE,
    ANY,
    FIXED_REGISTER,
    FIXED_DOUBLE_REGISTER,
    FIXED_SLOT,
    MUST_HAVE_REGISTER,
    WRITABLE_REGISTER,
    SAME_AS_FIRST_INPUT,
    IGNORE
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

  explicit LUnallocated(Policy policy) : LOperand(UNALLOCATED, 0) {
    Initialize(policy, 0, USED_AT_END);
  }

  LUnallocated(Policy policy, int fixed_index) : LOperand(UNALLOCATED, 0) {
    Initialize(policy, fixed_index, USED_AT_END);
  }

  LUnallocated(Policy policy, Lifetime lifetime) : LOperand(UNALLOCATED, 0) {
    Initialize(policy, 0, lifetime);
  }

  // The superclass has a KindField.  Some policies have a signed fixed
  // index in the upper bits.
  static const int kPolicyWidth = 4;
  static const int kLifetimeWidth = 1;
  static const int kVirtualRegisterWidth = 17;

  static const int kPolicyShift = kKindFieldWidth;
  static const int kLifetimeShift = kPolicyShift + kPolicyWidth;
  static const int kVirtualRegisterShift = kLifetimeShift + kLifetimeWidth;
  static const int kFixedIndexShift =
      kVirtualRegisterShift + kVirtualRegisterWidth;

  class PolicyField : public BitField<Policy, kPolicyShift, kPolicyWidth> { };

  class LifetimeField
      : public BitField<Lifetime, kLifetimeShift, kLifetimeWidth> {
  };

  class VirtualRegisterField
      : public BitField<unsigned,
                        kVirtualRegisterShift,
                        kVirtualRegisterWidth> {
  };

  static const int kMaxVirtualRegisters = 1 << (kVirtualRegisterWidth + 1);
  static const int kMaxFixedIndices = 128;

  bool HasIgnorePolicy() const { return policy() == IGNORE; }
  bool HasNoPolicy() const { return policy() == NONE; }
  bool HasAnyPolicy() const {
    return policy() == ANY;
  }
  bool HasFixedPolicy() const {
    return policy() == FIXED_REGISTER ||
        policy() == FIXED_DOUBLE_REGISTER ||
        policy() == FIXED_SLOT;
  }
  bool HasRegisterPolicy() const {
    return policy() == WRITABLE_REGISTER || policy() == MUST_HAVE_REGISTER;
  }
  bool HasSameAsInputPolicy() const {
    return policy() == SAME_AS_FIRST_INPUT;
  }
  Policy policy() const { return PolicyField::decode(value_); }
  void set_policy(Policy policy) {
    value_ &= ~PolicyField::mask();
    value_ |= PolicyField::encode(policy);
  }
  int fixed_index() const {
    return static_cast<int>(value_) >> kFixedIndexShift;
  }

  unsigned virtual_register() const {
    return VirtualRegisterField::decode(value_);
  }

  void set_virtual_register(unsigned id) {
    value_ &= ~VirtualRegisterField::mask();
    value_ |= VirtualRegisterField::encode(id);
  }

  LUnallocated* CopyUnconstrained() {
    LUnallocated* result = new LUnallocated(ANY);
    result->set_virtual_register(virtual_register());
    return result;
  }

  static LUnallocated* cast(LOperand* op) {
    ASSERT(op->IsUnallocated());
    return reinterpret_cast<LUnallocated*>(op);
  }

  bool IsUsedAtStart() {
    return LifetimeField::decode(value_) == USED_AT_START;
  }

 private:
  void Initialize(Policy policy, int fixed_index, Lifetime lifetime) {
    value_ |= PolicyField::encode(policy);
    value_ |= LifetimeField::encode(lifetime);
    value_ |= fixed_index << kFixedIndexShift;
    ASSERT(this->fixed_index() == fixed_index);
  }
};


class LMoveOperands BASE_EMBEDDED {
 public:
  LMoveOperands(LOperand* from, LOperand* to) : from_(from), to_(to) { }

  LOperand* from() const { return from_; }
  LOperand* to() const { return to_; }
  bool IsRedundant() const {
    return IsEliminated() || from_->Equals(to_) || IsIgnored();
  }
  bool IsEliminated() const { return from_ == NULL; }
  bool IsIgnored() const {
    if (to_ != NULL && to_->IsUnallocated() &&
      LUnallocated::cast(to_)->HasIgnorePolicy()) {
      return true;
    }
    return false;
  }

  void Eliminate() { from_ = to_ = NULL; }

 private:
  LOperand* from_;
  LOperand* to_;
};


class LConstantOperand: public LOperand {
 public:
  static LConstantOperand* Create(int index) {
    ASSERT(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new LConstantOperand(index);
  }

  static LConstantOperand* cast(LOperand* op) {
    ASSERT(op->IsConstantOperand());
    return reinterpret_cast<LConstantOperand*>(op);
  }

  static void SetupCache();

 private:
  static const int kNumCachedOperands = 128;
  static LConstantOperand cache[];

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
  static LStackSlot* Create(int index) {
    ASSERT(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new LStackSlot(index);
  }

  static LStackSlot* cast(LOperand* op) {
    ASSERT(op->IsStackSlot());
    return reinterpret_cast<LStackSlot*>(op);
  }

  static void SetupCache();

 private:
  static const int kNumCachedOperands = 128;
  static LStackSlot cache[];

  LStackSlot() : LOperand() { }
  explicit LStackSlot(int index) : LOperand(STACK_SLOT, index) { }
};


class LDoubleStackSlot: public LOperand {
 public:
  static LDoubleStackSlot* Create(int index) {
    ASSERT(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new LDoubleStackSlot(index);
  }

  static LDoubleStackSlot* cast(LOperand* op) {
    ASSERT(op->IsStackSlot());
    return reinterpret_cast<LDoubleStackSlot*>(op);
  }

  static void SetupCache();

 private:
  static const int kNumCachedOperands = 128;
  static LDoubleStackSlot cache[];

  LDoubleStackSlot() : LOperand() { }
  explicit LDoubleStackSlot(int index) : LOperand(DOUBLE_STACK_SLOT, index) { }
};


class LRegister: public LOperand {
 public:
  static LRegister* Create(int index) {
    ASSERT(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new LRegister(index);
  }

  static LRegister* cast(LOperand* op) {
    ASSERT(op->IsRegister());
    return reinterpret_cast<LRegister*>(op);
  }

  static void SetupCache();

 private:
  static const int kNumCachedOperands = 16;
  static LRegister cache[];

  LRegister() : LOperand() { }
  explicit LRegister(int index) : LOperand(REGISTER, index) { }
};


class LDoubleRegister: public LOperand {
 public:
  static LDoubleRegister* Create(int index) {
    ASSERT(index >= 0);
    if (index < kNumCachedOperands) return &cache[index];
    return new LDoubleRegister(index);
  }

  static LDoubleRegister* cast(LOperand* op) {
    ASSERT(op->IsDoubleRegister());
    return reinterpret_cast<LDoubleRegister*>(op);
  }

  static void SetupCache();

 private:
  static const int kNumCachedOperands = 16;
  static LDoubleRegister cache[];

  LDoubleRegister() : LOperand() { }
  explicit LDoubleRegister(int index) : LOperand(DOUBLE_REGISTER, index) { }
};


// A register-allocator view of a Lithium instruction. It contains the id of
// the output operand and a list of input operand uses.
class InstructionSummary: public ZoneObject {
 public:
  InstructionSummary()
      : output_operand_(NULL),
        input_count_(0),
        operands_(4),
        is_call_(false),
        is_save_doubles_(false) {}

  // Output operands.
  LOperand* Output() const { return output_operand_; }
  void SetOutput(LOperand* output) {
    ASSERT(output_operand_ == NULL);
    output_operand_ = output;
  }

  // Input operands.
  int InputCount() const { return input_count_; }
  LOperand* InputAt(int i) const {
    ASSERT(i < input_count_);
    return operands_[i];
  }
  void AddInput(LOperand* input) {
    operands_.InsertAt(input_count_, input);
    input_count_++;
  }

  // Temporary operands.
  int TempCount() const { return operands_.length() - input_count_; }
  LOperand* TempAt(int i) const { return operands_[i + input_count_]; }
  void AddTemp(LOperand* temp) { operands_.Add(temp); }

  void MarkAsCall() { is_call_ = true; }
  bool IsCall() const { return is_call_; }

  void MarkAsSaveDoubles() { is_save_doubles_ = true; }
  bool IsSaveDoubles() const { return is_save_doubles_; }

 private:
  LOperand* output_operand_;
  int input_count_;
  ZoneList<LOperand*> operands_;
  bool is_call_;
  bool is_save_doubles_;
};

// Representation of the non-empty interval [start,end[.
class UseInterval: public ZoneObject {
 public:
  UseInterval(LifetimePosition start, LifetimePosition end)
      : start_(start), end_(end), next_(NULL) {
    ASSERT(start.Value() < end.Value());
  }

  LifetimePosition start() const { return start_; }
  LifetimePosition end() const { return end_; }
  UseInterval* next() const { return next_; }

  // Split this interval at the given position without effecting the
  // live range that owns it. The interval must contain the position.
  void SplitAt(LifetimePosition pos);

  // If this interval intersects with other return smallest position
  // that belongs to both of them.
  LifetimePosition Intersect(const UseInterval* other) const {
    if (other->start().Value() < start_.Value()) return other->Intersect(this);
    if (other->start().Value() < end_.Value()) return other->start();
    return LifetimePosition::Invalid();
  }

  bool Contains(LifetimePosition point) const {
    return start_.Value() <= point.Value() && point.Value() < end_.Value();
  }

 private:
  void set_start(LifetimePosition start) { start_ = start; }
  void set_next(UseInterval* next) { next_ = next; }

  LifetimePosition start_;
  LifetimePosition end_;
  UseInterval* next_;

  friend class LiveRange;  // Assigns to start_.
};

// Representation of a use position.
class UsePosition: public ZoneObject {
 public:
  UsePosition(LifetimePosition pos, LOperand* operand)
      : operand_(operand),
        hint_(NULL),
        pos_(pos),
        next_(NULL),
        requires_reg_(false),
        register_beneficial_(true) {
    if (operand_ != NULL && operand_->IsUnallocated()) {
      LUnallocated* unalloc = LUnallocated::cast(operand_);
      requires_reg_ = unalloc->HasRegisterPolicy();
      register_beneficial_ = !unalloc->HasAnyPolicy();
    }
    ASSERT(pos_.IsValid());
  }

  LOperand* operand() const { return operand_; }
  bool HasOperand() const { return operand_ != NULL; }

  LOperand* hint() const { return hint_; }
  void set_hint(LOperand* hint) { hint_ = hint; }
  bool HasHint() const { return hint_ != NULL && !hint_->IsUnallocated(); }
  bool RequiresRegister() const;
  bool RegisterIsBeneficial() const;

  LifetimePosition pos() const { return pos_; }
  UsePosition* next() const { return next_; }

 private:
  void set_next(UsePosition* next) { next_ = next; }

  LOperand* operand_;
  LOperand* hint_;
  LifetimePosition pos_;
  UsePosition* next_;
  bool requires_reg_;
  bool register_beneficial_;

  friend class LiveRange;
};

// Representation of SSA values' live ranges as a collection of (continuous)
// intervals over the instruction ordering.
class LiveRange: public ZoneObject {
 public:
  static const int kInvalidAssignment = 0x7fffffff;

  explicit LiveRange(int id)
      : id_(id),
        spilled_(false),
        assigned_register_(kInvalidAssignment),
        assigned_register_kind_(NONE),
        last_interval_(NULL),
        first_interval_(NULL),
        first_pos_(NULL),
        parent_(NULL),
        next_(NULL),
        current_interval_(NULL),
        last_processed_use_(NULL),
        spill_start_index_(kMaxInt) {
    spill_operand_ = new LUnallocated(LUnallocated::IGNORE);
  }

  UseInterval* first_interval() const { return first_interval_; }
  UsePosition* first_pos() const { return first_pos_; }
  LiveRange* parent() const { return parent_; }
  LiveRange* TopLevel() { return (parent_ == NULL) ? this : parent_; }
  LiveRange* next() const { return next_; }
  bool IsChild() const { return parent() != NULL; }
  bool IsParent() const { return parent() == NULL; }
  int id() const { return id_; }
  bool IsFixed() const { return id_ < 0; }
  bool IsEmpty() const { return first_interval() == NULL; }
  LOperand* CreateAssignedOperand();
  int assigned_register() const { return assigned_register_; }
  int spill_start_index() const { return spill_start_index_; }
  void set_assigned_register(int reg, RegisterKind register_kind) {
    ASSERT(!HasRegisterAssigned() && !IsSpilled());
    assigned_register_ = reg;
    assigned_register_kind_ = register_kind;
    ConvertOperands();
  }
  void MakeSpilled() {
    ASSERT(!IsSpilled());
    ASSERT(TopLevel()->HasAllocatedSpillOperand());
    spilled_ = true;
    assigned_register_ = kInvalidAssignment;
    ConvertOperands();
  }

  // Returns use position in this live range that follows both start
  // and last processed use position.
  // Modifies internal state of live range!
  UsePosition* NextUsePosition(LifetimePosition start);

  // Returns use position for which register is required in this live
  // range and which follows both start and last processed use position
  // Modifies internal state of live range!
  UsePosition* NextRegisterPosition(LifetimePosition start);

  // Returns use position for which register is beneficial in this live
  // range and which follows both start and last processed use position
  // Modifies internal state of live range!
  UsePosition* NextUsePositionRegisterIsBeneficial(LifetimePosition start);

  // Can this live range be spilled at this position.
  bool CanBeSpilled(LifetimePosition pos);

  // Split this live range at the given position which must follow the start of
  // the range.
  // All uses following the given position will be moved from this
  // live range to the result live range.
  void SplitAt(LifetimePosition position, LiveRange* result);

  bool IsDouble() const { return assigned_register_kind_ == DOUBLE_REGISTERS; }
  bool HasRegisterAssigned() const {
    return assigned_register_ != kInvalidAssignment;
  }
  bool IsSpilled() const { return spilled_; }
  UsePosition* FirstPosWithHint() const;

  LOperand* FirstHint() const {
    UsePosition* pos = FirstPosWithHint();
    if (pos != NULL) return pos->hint();
    return NULL;
  }

  LifetimePosition Start() const {
    ASSERT(!IsEmpty());
    return first_interval()->start();
  }

  LifetimePosition End() const {
    ASSERT(!IsEmpty());
    return last_interval_->end();
  }

  bool HasAllocatedSpillOperand() const {
    return spill_operand_ != NULL && !spill_operand_->IsUnallocated();
  }

  LOperand* GetSpillOperand() const { return spill_operand_; }
  void SetSpillOperand(LOperand* operand) {
    ASSERT(!operand->IsUnallocated());
    ASSERT(spill_operand_ != NULL);
    ASSERT(spill_operand_->IsUnallocated());
    spill_operand_->ConvertTo(operand->kind(), operand->index());
  }

  void SetSpillStartIndex(int start) {
    spill_start_index_ = Min(start, spill_start_index_);
  }

  bool ShouldBeAllocatedBefore(const LiveRange* other) const;
  bool CanCover(LifetimePosition position) const;
  bool Covers(LifetimePosition position);
  LifetimePosition FirstIntersection(LiveRange* other);

  // Add a new interval or a new use position to this live range.
  void EnsureInterval(LifetimePosition start, LifetimePosition end);
  void AddUseInterval(LifetimePosition start, LifetimePosition end);
  UsePosition* AddUsePosition(LifetimePosition pos, LOperand* operand);
  UsePosition* AddUsePosition(LifetimePosition pos);

  // Shorten the most recently added interval by setting a new start.
  void ShortenTo(LifetimePosition start);

#ifdef DEBUG
  // True if target overlaps an existing interval.
  bool HasOverlap(UseInterval* target) const;
  void Verify() const;
#endif

 private:
  void ConvertOperands();
  UseInterval* FirstSearchIntervalForPosition(LifetimePosition position) const;
  void AdvanceLastProcessedMarker(UseInterval* to_start_of,
                                  LifetimePosition but_not_past) const;

  int id_;
  bool spilled_;
  int assigned_register_;
  RegisterKind assigned_register_kind_;
  UseInterval* last_interval_;
  UseInterval* first_interval_;
  UsePosition* first_pos_;
  LiveRange* parent_;
  LiveRange* next_;
  // This is used as a cache, it doesn't affect correctness.
  mutable UseInterval* current_interval_;
  UsePosition* last_processed_use_;
  LOperand* spill_operand_;
  int spill_start_index_;
};


class GrowableBitVector BASE_EMBEDDED {
 public:
  GrowableBitVector() : bits_(NULL) { }

  bool Contains(int value) const {
    if (!InBitsRange(value)) return false;
    return bits_->Contains(value);
  }

  void Add(int value) {
    EnsureCapacity(value);
    bits_->Add(value);
  }

 private:
  static const int kInitialLength = 1024;

  bool InBitsRange(int value) const {
    return bits_ != NULL && bits_->length() > value;
  }

  void EnsureCapacity(int value) {
    if (InBitsRange(value)) return;
    int new_length = bits_ == NULL ? kInitialLength : bits_->length();
    while (new_length <= value) new_length *= 2;
    BitVector* new_bits = new BitVector(new_length);
    if (bits_ != NULL) new_bits->CopyFrom(*bits_);
    bits_ = new_bits;
  }

  BitVector* bits_;
};


class LAllocator BASE_EMBEDDED {
 public:
  explicit LAllocator(int first_virtual_register, HGraph* graph)
      : chunk_(NULL),
        summaries_(0),
        next_summary_(NULL),
        summary_stack_(2),
        live_in_sets_(0),
        live_ranges_(16),
        fixed_live_ranges_(8),
        fixed_double_live_ranges_(8),
        unhandled_live_ranges_(8),
        active_live_ranges_(8),
        inactive_live_ranges_(8),
        reusable_slots_(8),
        next_virtual_register_(first_virtual_register),
        first_artificial_register_(first_virtual_register),
        mode_(NONE),
        num_registers_(-1),
        graph_(graph),
        has_osr_entry_(false) {}

  static void Setup();
  static void TraceAlloc(const char* msg, ...);

  // Lithium translation support.
  // Record a use of an input operand in the current instruction.
  void RecordUse(HValue* value, LUnallocated* operand);
  // Record the definition of the output operand.
  void RecordDefinition(HInstruction* instr, LUnallocated* operand);
  // Record a temporary operand.
  void RecordTemporary(LUnallocated* operand);

  // Marks the current instruction as a call.
  void MarkAsCall();

  // Marks the current instruction as requiring saving double registers.
  void MarkAsSaveDoubles();

  // Checks whether the value of a given virtual register is tagged.
  bool HasTaggedValue(int virtual_register) const;

  // Returns the register kind required by the given virtual register.
  RegisterKind RequiredRegisterKind(int virtual_register) const;

  // Begin a new instruction.
  void BeginInstruction();

  // Summarize the current instruction.
  void SummarizeInstruction(int index);

  // Summarize the current instruction.
  void OmitInstruction();

  // Control max function size.
  static int max_initial_value_ids();

  void Allocate(LChunk* chunk);

  const ZoneList<LiveRange*>* live_ranges() const { return &live_ranges_; }
  const ZoneList<LiveRange*>* fixed_live_ranges() const {
    return &fixed_live_ranges_;
  }
  const ZoneList<LiveRange*>* fixed_double_live_ranges() const {
    return &fixed_double_live_ranges_;
  }

  LChunk* chunk() const { return chunk_; }
  HGraph* graph() const { return graph_; }

  void MarkAsOsrEntry() {
    // There can be only one.
    ASSERT(!has_osr_entry_);
    // Simply set a flag to find and process instruction later.
    has_osr_entry_ = true;
  }

#ifdef DEBUG
  void Verify() const;
#endif

 private:
  void MeetRegisterConstraints();
  void ResolvePhis();
  void BuildLiveRanges();
  void AllocateGeneralRegisters();
  void AllocateDoubleRegisters();
  void ConnectRanges();
  void ResolveControlFlow();
  void PopulatePointerMaps();
  void ProcessOsrEntry();
  void AllocateRegisters();
  bool CanEagerlyResolveControlFlow(HBasicBlock* block) const;
  inline bool SafePointsAreInOrder() const;

  // Liveness analysis support.
  void InitializeLivenessAnalysis();
  BitVector* ComputeLiveOut(HBasicBlock* block);
  void AddInitialIntervals(HBasicBlock* block, BitVector* live_out);
  void ProcessInstructions(HBasicBlock* block, BitVector* live);
  void MeetRegisterConstraints(HBasicBlock* block);
  void MeetConstraintsBetween(InstructionSummary* first,
                              InstructionSummary* second,
                              int gap_index);
  void ResolvePhis(HBasicBlock* block);

  // Helper methods for building intervals.
  LOperand* AllocateFixed(LUnallocated* operand, int pos, bool is_tagged);
  LiveRange* LiveRangeFor(LOperand* operand);
  void Define(LifetimePosition position, LOperand* operand, LOperand* hint);
  void Use(LifetimePosition block_start,
           LifetimePosition position,
           LOperand* operand,
           LOperand* hint);
  void AddConstraintsGapMove(int index, LOperand* from, LOperand* to);

  // Helper methods for updating the life range lists.
  void AddToActive(LiveRange* range);
  void AddToInactive(LiveRange* range);
  void AddToUnhandledSorted(LiveRange* range);
  void AddToUnhandledUnsorted(LiveRange* range);
  void SortUnhandled();
  bool UnhandledIsSorted();
  void ActiveToHandled(LiveRange* range);
  void ActiveToInactive(LiveRange* range);
  void InactiveToHandled(LiveRange* range);
  void InactiveToActive(LiveRange* range);
  void FreeSpillSlot(LiveRange* range);
  LOperand* TryReuseSpillSlot(LiveRange* range);

  // Helper methods for allocating registers.
  bool TryAllocateFreeReg(LiveRange* range);
  void AllocateBlockedReg(LiveRange* range);

  // Live range splitting helpers.

  // Split the given range at the given position.
  // If range starts at or after the given position then the
  // original range is returned.
  // Otherwise returns the live range that starts at pos and contains
  // all uses from the original range that follow pos. Uses at pos will
  // still be owned by the original range after splitting.
  LiveRange* SplitAt(LiveRange* range, LifetimePosition pos);

  // Split the given range in a position from the interval [start, end].
  LiveRange* SplitBetween(LiveRange* range,
                          LifetimePosition start,
                          LifetimePosition end);

  // Find a lifetime position in the interval [start, end] which
  // is optimal for splitting: it is either header of the outermost
  // loop covered by this interval or the latest possible position.
  LifetimePosition FindOptimalSplitPos(LifetimePosition start,
                                       LifetimePosition end);

  // Spill the given life range after position pos.
  void SpillAfter(LiveRange* range, LifetimePosition pos);

  // Spill the given life range after position start and up to position end.
  void SpillBetween(LiveRange* range,
                    LifetimePosition start,
                    LifetimePosition end);

  void SplitAndSpillIntersecting(LiveRange* range);

  void Spill(LiveRange* range);
  bool IsBlockBoundary(LifetimePosition pos);
  void AddGapMove(int pos, LiveRange* prev, LiveRange* next);

  // Helper methods for resolving control flow.
  void ResolveControlFlow(LiveRange* range,
                          HBasicBlock* block,
                          HBasicBlock* pred);

  // Return parallel move that should be used to connect ranges split at the
  // given position.
  LParallelMove* GetConnectingParallelMove(LifetimePosition pos);

  // Return the block which contains give lifetime position.
  HBasicBlock* GetBlock(LifetimePosition pos);

  // Current active summary.
  InstructionSummary* current_summary() const { return summary_stack_.last(); }

  // Get summary for given instruction index.
  InstructionSummary* GetSummary(int index) const { return summaries_[index]; }

  // Helper methods for the fixed registers.
  int RegisterCount() const;
  static int FixedLiveRangeID(int index) { return -index - 1; }
  static int FixedDoubleLiveRangeID(int index);
  LiveRange* FixedLiveRangeFor(int index);
  LiveRange* FixedDoubleLiveRangeFor(int index);
  LiveRange* LiveRangeFor(int index);
  HPhi* LookupPhi(LOperand* operand) const;
  LGap* GetLastGap(HBasicBlock* block) const;

  const char* RegisterName(int allocation_index);

  LChunk* chunk_;
  ZoneList<InstructionSummary*> summaries_;
  InstructionSummary* next_summary_;

  ZoneList<InstructionSummary*> summary_stack_;

  // During liveness analysis keep a mapping from block id to live_in sets
  // for blocks already analyzed.
  ZoneList<BitVector*> live_in_sets_;

  // Liveness analysis results.
  ZoneList<LiveRange*> live_ranges_;

  // Lists of live ranges
  ZoneList<LiveRange*> fixed_live_ranges_;
  ZoneList<LiveRange*> fixed_double_live_ranges_;
  ZoneList<LiveRange*> unhandled_live_ranges_;
  ZoneList<LiveRange*> active_live_ranges_;
  ZoneList<LiveRange*> inactive_live_ranges_;
  ZoneList<LiveRange*> reusable_slots_;

  // Next virtual register number to be assigned to temporaries.
  int next_virtual_register_;
  int first_artificial_register_;
  GrowableBitVector double_artificial_registers_;

  RegisterKind mode_;
  int num_registers_;

  HGraph* graph_;

  bool has_osr_entry_;

  DISALLOW_COPY_AND_ASSIGN(LAllocator);
};


} }  // namespace v8::internal

#endif  // V8_LITHIUM_ALLOCATOR_H_

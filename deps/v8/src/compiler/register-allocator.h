// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGISTER_ALLOCATOR_H_
#define V8_REGISTER_ALLOCATOR_H_

#include "src/compiler/instruction.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

enum RegisterKind {
  UNALLOCATED_REGISTERS,
  GENERAL_REGISTERS,
  DOUBLE_REGISTERS
};


// This class represents a single point of a InstructionOperand's lifetime. For
// each instruction there are exactly two lifetime positions: the beginning and
// the end of the instruction. Lifetime positions for different instructions are
// disjoint.
class LifetimePosition FINAL {
 public:
  // Return the lifetime position that corresponds to the beginning of
  // the instruction with the given index.
  static LifetimePosition FromInstructionIndex(int index) {
    return LifetimePosition(index * kStep);
  }

  // Returns a numeric representation of this lifetime position.
  int Value() const { return value_; }

  // Returns the index of the instruction to which this lifetime position
  // corresponds.
  int InstructionIndex() const {
    DCHECK(IsValid());
    return value_ / kStep;
  }

  // Returns true if this lifetime position corresponds to the instruction
  // start.
  bool IsInstructionStart() const { return (value_ & (kStep - 1)) == 0; }

  // Returns the lifetime position for the start of the instruction which
  // corresponds to this lifetime position.
  LifetimePosition InstructionStart() const {
    DCHECK(IsValid());
    return LifetimePosition(value_ & ~(kStep - 1));
  }

  // Returns the lifetime position for the end of the instruction which
  // corresponds to this lifetime position.
  LifetimePosition InstructionEnd() const {
    DCHECK(IsValid());
    return LifetimePosition(InstructionStart().Value() + kStep / 2);
  }

  // Returns the lifetime position for the beginning of the next instruction.
  LifetimePosition NextInstruction() const {
    DCHECK(IsValid());
    return LifetimePosition(InstructionStart().Value() + kStep);
  }

  // Returns the lifetime position for the beginning of the previous
  // instruction.
  LifetimePosition PrevInstruction() const {
    DCHECK(IsValid());
    DCHECK(value_ > 1);
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

  explicit LifetimePosition(int value) : value_(value) {}

  int value_;
};


// Representation of the non-empty interval [start,end[.
class UseInterval FINAL : public ZoneObject {
 public:
  UseInterval(LifetimePosition start, LifetimePosition end)
      : start_(start), end_(end), next_(nullptr) {
    DCHECK(start.Value() < end.Value());
  }

  LifetimePosition start() const { return start_; }
  LifetimePosition end() const { return end_; }
  UseInterval* next() const { return next_; }

  // Split this interval at the given position without effecting the
  // live range that owns it. The interval must contain the position.
  void SplitAt(LifetimePosition pos, Zone* zone);

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

  void set_start(LifetimePosition start) { start_ = start; }
  void set_next(UseInterval* next) { next_ = next; }

  LifetimePosition start_;
  LifetimePosition end_;
  UseInterval* next_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UseInterval);
};


enum class UsePositionType : uint8_t { kAny, kRequiresRegister, kRequiresSlot };


// Representation of a use position.
class UsePosition FINAL : public ZoneObject {
 public:
  UsePosition(LifetimePosition pos, InstructionOperand* operand,
              InstructionOperand* hint);

  InstructionOperand* operand() const { return operand_; }
  bool HasOperand() const { return operand_ != nullptr; }

  InstructionOperand* hint() const { return hint_; }
  bool HasHint() const;
  bool RegisterIsBeneficial() const {
    return RegisterBeneficialField::decode(flags_);
  }
  UsePositionType type() const { return TypeField::decode(flags_); }

  LifetimePosition pos() const { return pos_; }
  UsePosition* next() const { return next_; }

  void set_next(UsePosition* next) { next_ = next; }
  void set_type(UsePositionType type, bool register_beneficial);

  InstructionOperand* const operand_;
  InstructionOperand* const hint_;
  LifetimePosition const pos_;
  UsePosition* next_;

 private:
  typedef BitField8<UsePositionType, 0, 2> TypeField;
  typedef BitField8<bool, 2, 1> RegisterBeneficialField;
  uint8_t flags_;

  DISALLOW_COPY_AND_ASSIGN(UsePosition);
};

class SpillRange;


// TODO(dcarney): remove this cache.
class InstructionOperandCache FINAL : public ZoneObject {
 public:
  InstructionOperandCache();

  InstructionOperand* RegisterOperand(int index) {
    DCHECK(index >= 0 &&
           index < static_cast<int>(arraysize(general_register_operands_)));
    return &general_register_operands_[index];
  }
  InstructionOperand* DoubleRegisterOperand(int index) {
    DCHECK(index >= 0 &&
           index < static_cast<int>(arraysize(double_register_operands_)));
    return &double_register_operands_[index];
  }

 private:
  InstructionOperand
      general_register_operands_[RegisterConfiguration::kMaxGeneralRegisters];
  InstructionOperand
      double_register_operands_[RegisterConfiguration::kMaxDoubleRegisters];

  DISALLOW_COPY_AND_ASSIGN(InstructionOperandCache);
};


// Representation of SSA values' live ranges as a collection of (continuous)
// intervals over the instruction ordering.
class LiveRange FINAL : public ZoneObject {
 public:
  static const int kInvalidAssignment = 0x7fffffff;

  LiveRange(int id, Zone* zone);

  UseInterval* first_interval() const { return first_interval_; }
  UsePosition* first_pos() const { return first_pos_; }
  LiveRange* parent() const { return parent_; }
  LiveRange* TopLevel() { return (parent_ == nullptr) ? this : parent_; }
  const LiveRange* TopLevel() const {
    return (parent_ == nullptr) ? this : parent_;
  }
  LiveRange* next() const { return next_; }
  bool IsChild() const { return parent() != nullptr; }
  int id() const { return id_; }
  bool IsFixed() const { return id_ < 0; }
  bool IsEmpty() const { return first_interval() == nullptr; }
  // TODO(dcarney): remove this.
  InstructionOperand* GetAssignedOperand(InstructionOperandCache* cache) const;
  InstructionOperand GetAssignedOperand() const;
  int assigned_register() const { return assigned_register_; }
  int spill_start_index() const { return spill_start_index_; }
  void set_assigned_register(int reg, InstructionOperandCache* cache);
  void MakeSpilled();
  bool is_phi() const { return is_phi_; }
  void set_is_phi(bool is_phi) { is_phi_ = is_phi; }
  bool is_non_loop_phi() const { return is_non_loop_phi_; }
  void set_is_non_loop_phi(bool is_non_loop_phi) {
    is_non_loop_phi_ = is_non_loop_phi;
  }
  bool has_slot_use() const { return has_slot_use_; }
  void set_has_slot_use(bool has_slot_use) { has_slot_use_ = has_slot_use; }

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

  // Returns use position for which register is beneficial in this live
  // range and which precedes start.
  UsePosition* PreviousUsePositionRegisterIsBeneficial(LifetimePosition start);

  // Can this live range be spilled at this position.
  bool CanBeSpilled(LifetimePosition pos);

  // Split this live range at the given position which must follow the start of
  // the range.
  // All uses following the given position will be moved from this
  // live range to the result live range.
  void SplitAt(LifetimePosition position, LiveRange* result, Zone* zone);

  RegisterKind Kind() const { return kind_; }
  bool HasRegisterAssigned() const {
    return assigned_register_ != kInvalidAssignment;
  }
  bool IsSpilled() const { return spilled_; }

  InstructionOperand* current_hint_operand() const {
    DCHECK(current_hint_operand_ == FirstHint());
    return current_hint_operand_;
  }
  InstructionOperand* FirstHint() const {
    UsePosition* pos = first_pos_;
    while (pos != nullptr && !pos->HasHint()) pos = pos->next();
    if (pos != nullptr) return pos->hint();
    return nullptr;
  }

  LifetimePosition Start() const {
    DCHECK(!IsEmpty());
    return first_interval()->start();
  }

  LifetimePosition End() const {
    DCHECK(!IsEmpty());
    return last_interval_->end();
  }

  enum class SpillType { kNoSpillType, kSpillOperand, kSpillRange };
  SpillType spill_type() const { return spill_type_; }
  InstructionOperand* GetSpillOperand() const {
    return spill_type_ == SpillType::kSpillOperand ? spill_operand_ : nullptr;
  }
  SpillRange* GetSpillRange() const {
    return spill_type_ == SpillType::kSpillRange ? spill_range_ : nullptr;
  }
  bool HasNoSpillType() const { return spill_type_ == SpillType::kNoSpillType; }
  bool HasSpillOperand() const {
    return spill_type_ == SpillType::kSpillOperand;
  }
  bool HasSpillRange() const { return spill_type_ == SpillType::kSpillRange; }

  void SpillAtDefinition(Zone* zone, int gap_index,
                         InstructionOperand* operand);
  void SetSpillOperand(InstructionOperand* operand);
  void SetSpillRange(SpillRange* spill_range);
  void CommitSpillOperand(InstructionOperand* operand);
  void CommitSpillsAtDefinition(InstructionSequence* sequence,
                                InstructionOperand* operand,
                                bool might_be_duplicated);

  void SetSpillStartIndex(int start) {
    spill_start_index_ = Min(start, spill_start_index_);
  }

  bool ShouldBeAllocatedBefore(const LiveRange* other) const;
  bool CanCover(LifetimePosition position) const;
  bool Covers(LifetimePosition position);
  LifetimePosition FirstIntersection(LiveRange* other);

  // Add a new interval or a new use position to this live range.
  void EnsureInterval(LifetimePosition start, LifetimePosition end, Zone* zone);
  void AddUseInterval(LifetimePosition start, LifetimePosition end, Zone* zone);
  void AddUsePosition(LifetimePosition pos, InstructionOperand* operand,
                      InstructionOperand* hint, Zone* zone);

  // Shorten the most recently added interval by setting a new start.
  void ShortenTo(LifetimePosition start);

#ifdef DEBUG
  // True if target overlaps an existing interval.
  bool HasOverlap(UseInterval* target) const;
  void Verify() const;
#endif

 private:
  struct SpillAtDefinitionList;

  void ConvertUsesToOperand(InstructionOperand* op,
                            InstructionOperand* spill_op);
  UseInterval* FirstSearchIntervalForPosition(LifetimePosition position) const;
  void AdvanceLastProcessedMarker(UseInterval* to_start_of,
                                  LifetimePosition but_not_past) const;

  // TODO(dcarney): pack this structure better.
  int id_;
  bool spilled_ : 1;
  bool has_slot_use_ : 1;  // Relevant only for parent.
  bool is_phi_ : 1;
  bool is_non_loop_phi_ : 1;
  RegisterKind kind_;
  int assigned_register_;
  UseInterval* last_interval_;
  UseInterval* first_interval_;
  UsePosition* first_pos_;
  LiveRange* parent_;
  LiveRange* next_;
  // This is used as a cache, it doesn't affect correctness.
  mutable UseInterval* current_interval_;
  UsePosition* last_processed_use_;
  // This is used as a cache, it's invalid outside of BuildLiveRanges.
  InstructionOperand* current_hint_operand_;
  int spill_start_index_;
  SpillType spill_type_;
  union {
    InstructionOperand* spill_operand_;
    SpillRange* spill_range_;
  };
  SpillAtDefinitionList* spills_at_definition_;

  friend class RegisterAllocator;  // Assigns to kind_.

  DISALLOW_COPY_AND_ASSIGN(LiveRange);
};


class SpillRange FINAL : public ZoneObject {
 public:
  SpillRange(LiveRange* range, Zone* zone);

  UseInterval* interval() const { return use_interval_; }
  RegisterKind Kind() const { return live_ranges_[0]->Kind(); }
  bool IsEmpty() const { return live_ranges_.empty(); }
  bool TryMerge(SpillRange* other);
  void SetOperand(InstructionOperand* op);

 private:
  LifetimePosition End() const { return end_position_; }
  ZoneVector<LiveRange*>& live_ranges() { return live_ranges_; }
  bool IsIntersectingWith(SpillRange* other) const;
  // Merge intervals, making sure the use intervals are sorted
  void MergeDisjointIntervals(UseInterval* other);

  ZoneVector<LiveRange*> live_ranges_;
  UseInterval* use_interval_;
  LifetimePosition end_position_;

  DISALLOW_COPY_AND_ASSIGN(SpillRange);
};


class RegisterAllocator FINAL : public ZoneObject {
 public:
  explicit RegisterAllocator(const RegisterConfiguration* config,
                             Zone* local_zone, Frame* frame,
                             InstructionSequence* code,
                             const char* debug_name = nullptr);

  const ZoneVector<LiveRange*>& live_ranges() const { return live_ranges_; }
  const ZoneVector<LiveRange*>& fixed_live_ranges() const {
    return fixed_live_ranges_;
  }
  const ZoneVector<LiveRange*>& fixed_double_live_ranges() const {
    return fixed_double_live_ranges_;
  }
  InstructionSequence* code() const { return code_; }
  // This zone is for datastructures only needed during register allocation.
  Zone* local_zone() const { return local_zone_; }

  // Phase 1 : insert moves to account for fixed register operands.
  void MeetRegisterConstraints();

  // Phase 2: deconstruct SSA by inserting moves in successors and the headers
  // of blocks containing phis.
  void ResolvePhis();

  // Phase 3: compute liveness of all virtual register.
  void BuildLiveRanges();
  bool ExistsUseWithoutDefinition();

  // Phase 4: compute register assignments.
  void AllocateGeneralRegisters();
  void AllocateDoubleRegisters();

  // Phase 5: assign spill splots.
  void AssignSpillSlots();

  // Phase 6: commit assignment.
  void CommitAssignment();

  // Phase 7: compute values for pointer maps.
  void PopulatePointerMaps();  // TODO(titzer): rename to PopulateReferenceMaps.

  // Phase 8: reconnect split ranges with moves.
  void ConnectRanges();

  // Phase 9: insert moves to connect ranges across basic blocks.
  void ResolveControlFlow();

 private:
  int GetVirtualRegister() { return code()->NextVirtualRegister(); }

  // Checks whether the value of a given virtual register is a reference.
  // TODO(titzer): rename this to IsReference.
  bool HasTaggedValue(int virtual_register) const;

  // Returns the register kind required by the given virtual register.
  RegisterKind RequiredRegisterKind(int virtual_register) const;

  // Creates a new live range.
  LiveRange* NewLiveRange(int index);

  // This zone is for InstructionOperands and moves that live beyond register
  // allocation.
  Zone* code_zone() const { return code()->zone(); }

  BitVector* assigned_registers() { return assigned_registers_; }
  BitVector* assigned_double_registers() { return assigned_double_registers_; }

#ifdef DEBUG
  void Verify() const;
#endif

  void AllocateRegisters();
  bool CanEagerlyResolveControlFlow(const InstructionBlock* block) const;
  bool SafePointsAreInOrder() const;

  // Liveness analysis support.
  BitVector* ComputeLiveOut(const InstructionBlock* block);
  void AddInitialIntervals(const InstructionBlock* block, BitVector* live_out);
  bool IsOutputRegisterOf(Instruction* instr, int index);
  bool IsOutputDoubleRegisterOf(Instruction* instr, int index);
  void ProcessInstructions(const InstructionBlock* block, BitVector* live);
  void MeetRegisterConstraints(const InstructionBlock* block);
  void MeetConstraintsBetween(Instruction* first, Instruction* second,
                              int gap_index);
  void MeetRegisterConstraintsForLastInstructionInBlock(
      const InstructionBlock* block);
  void ResolvePhis(const InstructionBlock* block);

  // Helper methods for building intervals.
  InstructionOperand* AllocateFixed(UnallocatedOperand* operand, int pos,
                                    bool is_tagged);
  LiveRange* LiveRangeFor(InstructionOperand* operand);
  void Define(LifetimePosition position, InstructionOperand* operand,
              InstructionOperand* hint);
  void Use(LifetimePosition block_start, LifetimePosition position,
           InstructionOperand* operand, InstructionOperand* hint);
  void AddGapMove(int index, GapInstruction::InnerPosition position,
                  InstructionOperand* from, InstructionOperand* to);

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

  // Helper methods for allocating registers.
  bool TryReuseSpillForPhi(LiveRange* range);
  bool TryAllocateFreeReg(LiveRange* range);
  void AllocateBlockedReg(LiveRange* range);
  SpillRange* AssignSpillRangeToLiveRange(LiveRange* range);

  // Live range splitting helpers.

  // Split the given range at the given position.
  // If range starts at or after the given position then the
  // original range is returned.
  // Otherwise returns the live range that starts at pos and contains
  // all uses from the original range that follow pos. Uses at pos will
  // still be owned by the original range after splitting.
  LiveRange* SplitRangeAt(LiveRange* range, LifetimePosition pos);

  // Split the given range in a position from the interval [start, end].
  LiveRange* SplitBetween(LiveRange* range, LifetimePosition start,
                          LifetimePosition end);

  // Find a lifetime position in the interval [start, end] which
  // is optimal for splitting: it is either header of the outermost
  // loop covered by this interval or the latest possible position.
  LifetimePosition FindOptimalSplitPos(LifetimePosition start,
                                       LifetimePosition end);

  // Spill the given life range after position pos.
  void SpillAfter(LiveRange* range, LifetimePosition pos);

  // Spill the given life range after position [start] and up to position [end].
  void SpillBetween(LiveRange* range, LifetimePosition start,
                    LifetimePosition end);

  // Spill the given life range after position [start] and up to position [end].
  // Range is guaranteed to be spilled at least until position [until].
  void SpillBetweenUntil(LiveRange* range, LifetimePosition start,
                         LifetimePosition until, LifetimePosition end);

  void SplitAndSpillIntersecting(LiveRange* range);

  // If we are trying to spill a range inside the loop try to
  // hoist spill position out to the point just before the loop.
  LifetimePosition FindOptimalSpillingPos(LiveRange* range,
                                          LifetimePosition pos);

  void Spill(LiveRange* range);
  bool IsBlockBoundary(LifetimePosition pos);

  // Helper methods for resolving control flow.
  void ResolveControlFlow(const InstructionBlock* block,
                          InstructionOperand* cur_op,
                          const InstructionBlock* pred,
                          InstructionOperand* pred_op);

  void SetLiveRangeAssignedRegister(LiveRange* range, int reg);

  // Return the block which contains give lifetime position.
  const InstructionBlock* GetInstructionBlock(LifetimePosition pos);

  // Helper methods for the fixed registers.
  int RegisterCount() const;
  static int FixedLiveRangeID(int index) { return -index - 1; }
  int FixedDoubleLiveRangeID(int index);
  LiveRange* FixedLiveRangeFor(int index);
  LiveRange* FixedDoubleLiveRangeFor(int index);
  LiveRange* LiveRangeFor(int index);
  GapInstruction* GetLastGap(const InstructionBlock* block);

  const char* RegisterName(int allocation_index);

  Instruction* InstructionAt(int index) { return code()->InstructionAt(index); }

  Frame* frame() const { return frame_; }
  const char* debug_name() const { return debug_name_; }
  const RegisterConfiguration* config() const { return config_; }
  InstructionOperandCache* operand_cache() const { return operand_cache_; }
  ZoneVector<LiveRange*>& live_ranges() { return live_ranges_; }
  ZoneVector<LiveRange*>& fixed_live_ranges() { return fixed_live_ranges_; }
  ZoneVector<LiveRange*>& fixed_double_live_ranges() {
    return fixed_double_live_ranges_;
  }
  ZoneVector<LiveRange*>& unhandled_live_ranges() {
    return unhandled_live_ranges_;
  }
  ZoneVector<LiveRange*>& active_live_ranges() { return active_live_ranges_; }
  ZoneVector<LiveRange*>& inactive_live_ranges() {
    return inactive_live_ranges_;
  }
  ZoneVector<SpillRange*>& spill_ranges() { return spill_ranges_; }

  struct PhiMapValue {
    PhiMapValue(PhiInstruction* phi, const InstructionBlock* block)
        : phi(phi), block(block) {}
    PhiInstruction* const phi;
    const InstructionBlock* const block;
  };
  typedef ZoneMap<int, PhiMapValue> PhiMap;

  Zone* const local_zone_;
  Frame* const frame_;
  InstructionSequence* const code_;
  const char* const debug_name_;

  const RegisterConfiguration* config_;
  InstructionOperandCache* const operand_cache_;
  PhiMap phi_map_;

  // During liveness analysis keep a mapping from block id to live_in sets
  // for blocks already analyzed.
  ZoneVector<BitVector*> live_in_sets_;

  // Liveness analysis results.
  ZoneVector<LiveRange*> live_ranges_;

  // Lists of live ranges
  ZoneVector<LiveRange*> fixed_live_ranges_;
  ZoneVector<LiveRange*> fixed_double_live_ranges_;
  ZoneVector<LiveRange*> unhandled_live_ranges_;
  ZoneVector<LiveRange*> active_live_ranges_;
  ZoneVector<LiveRange*> inactive_live_ranges_;
  ZoneVector<SpillRange*> spill_ranges_;

  RegisterKind mode_;
  int num_registers_;

  BitVector* assigned_registers_;
  BitVector* assigned_double_registers_;

#ifdef DEBUG
  LifetimePosition allocation_finger_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RegisterAllocator);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_REGISTER_ALLOCATOR_H_

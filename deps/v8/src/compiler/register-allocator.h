// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_REGISTER_ALLOCATOR_H_
#define V8_COMPILER_REGISTER_ALLOCATOR_H_

#include "src/base/bits.h"
#include "src/base/compiler-specific.h"
#include "src/compiler/instruction.h"
#include "src/globals.h"
#include "src/ostreams.h"
#include "src/register-configuration.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

enum RegisterKind { GENERAL_REGISTERS, FP_REGISTERS };

// This class represents a single point of a InstructionOperand's lifetime. For
// each instruction there are four lifetime positions:
//
//   [[START, END], [START, END]]
//
// Where the first half position corresponds to
//
//  [GapPosition::START, GapPosition::END]
//
// and the second half position corresponds to
//
//  [Lifetime::USED_AT_START, Lifetime::USED_AT_END]
//
class LifetimePosition final {
 public:
  // Return the lifetime position that corresponds to the beginning of
  // the gap with the given index.
  static LifetimePosition GapFromInstructionIndex(int index) {
    return LifetimePosition(index * kStep);
  }
  // Return the lifetime position that corresponds to the beginning of
  // the instruction with the given index.
  static LifetimePosition InstructionFromInstructionIndex(int index) {
    return LifetimePosition(index * kStep + kHalfStep);
  }

  static bool ExistsGapPositionBetween(LifetimePosition pos1,
                                       LifetimePosition pos2) {
    if (pos1 > pos2) std::swap(pos1, pos2);
    LifetimePosition next(pos1.value_ + 1);
    if (next.IsGapPosition()) return next < pos2;
    return next.NextFullStart() < pos2;
  }

  // Returns a numeric representation of this lifetime position.
  int value() const { return value_; }

  // Returns the index of the instruction to which this lifetime position
  // corresponds.
  int ToInstructionIndex() const {
    DCHECK(IsValid());
    return value_ / kStep;
  }

  // Returns true if this lifetime position corresponds to a START value
  bool IsStart() const { return (value_ & (kHalfStep - 1)) == 0; }
  // Returns true if this lifetime position corresponds to an END value
  bool IsEnd() const { return (value_ & (kHalfStep - 1)) == 1; }
  // Returns true if this lifetime position corresponds to a gap START value
  bool IsFullStart() const { return (value_ & (kStep - 1)) == 0; }

  bool IsGapPosition() const { return (value_ & 0x2) == 0; }
  bool IsInstructionPosition() const { return !IsGapPosition(); }

  // Returns the lifetime position for the current START.
  LifetimePosition Start() const {
    DCHECK(IsValid());
    return LifetimePosition(value_ & ~(kHalfStep - 1));
  }

  // Returns the lifetime position for the current gap START.
  LifetimePosition FullStart() const {
    DCHECK(IsValid());
    return LifetimePosition(value_ & ~(kStep - 1));
  }

  // Returns the lifetime position for the current END.
  LifetimePosition End() const {
    DCHECK(IsValid());
    return LifetimePosition(Start().value_ + kHalfStep / 2);
  }

  // Returns the lifetime position for the beginning of the next START.
  LifetimePosition NextStart() const {
    DCHECK(IsValid());
    return LifetimePosition(Start().value_ + kHalfStep);
  }

  // Returns the lifetime position for the beginning of the next gap START.
  LifetimePosition NextFullStart() const {
    DCHECK(IsValid());
    return LifetimePosition(FullStart().value_ + kStep);
  }

  // Returns the lifetime position for the beginning of the previous START.
  LifetimePosition PrevStart() const {
    DCHECK(IsValid());
    DCHECK_LE(kHalfStep, value_);
    return LifetimePosition(Start().value_ - kHalfStep);
  }

  // Constructs the lifetime position which does not correspond to any
  // instruction.
  LifetimePosition() : value_(-1) {}

  // Returns true if this lifetime positions corrensponds to some
  // instruction.
  bool IsValid() const { return value_ != -1; }

  bool operator<(const LifetimePosition& that) const {
    return this->value_ < that.value_;
  }

  bool operator<=(const LifetimePosition& that) const {
    return this->value_ <= that.value_;
  }

  bool operator==(const LifetimePosition& that) const {
    return this->value_ == that.value_;
  }

  bool operator!=(const LifetimePosition& that) const {
    return this->value_ != that.value_;
  }

  bool operator>(const LifetimePosition& that) const {
    return this->value_ > that.value_;
  }

  bool operator>=(const LifetimePosition& that) const {
    return this->value_ >= that.value_;
  }

  void Print() const;

  static inline LifetimePosition Invalid() { return LifetimePosition(); }

  static inline LifetimePosition MaxPosition() {
    // We have to use this kind of getter instead of static member due to
    // crash bug in GDB.
    return LifetimePosition(kMaxInt);
  }

  static inline LifetimePosition FromInt(int value) {
    return LifetimePosition(value);
  }

 private:
  static const int kHalfStep = 2;
  static const int kStep = 2 * kHalfStep;

  static_assert(base::bits::IsPowerOfTwo(kHalfStep),
                "Code relies on kStep and kHalfStep being a power of two");

  explicit LifetimePosition(int value) : value_(value) {}

  int value_;
};


std::ostream& operator<<(std::ostream& os, const LifetimePosition pos);


// Representation of the non-empty interval [start,end[.
class UseInterval final : public ZoneObject {
 public:
  UseInterval(LifetimePosition start, LifetimePosition end)
      : start_(start), end_(end), next_(nullptr) {
    DCHECK(start < end);
  }

  LifetimePosition start() const { return start_; }
  void set_start(LifetimePosition start) { start_ = start; }
  LifetimePosition end() const { return end_; }
  void set_end(LifetimePosition end) { end_ = end; }
  UseInterval* next() const { return next_; }
  void set_next(UseInterval* next) { next_ = next; }

  // Split this interval at the given position without effecting the
  // live range that owns it. The interval must contain the position.
  UseInterval* SplitAt(LifetimePosition pos, Zone* zone);

  // If this interval intersects with other return smallest position
  // that belongs to both of them.
  LifetimePosition Intersect(const UseInterval* other) const {
    if (other->start() < start_) return other->Intersect(this);
    if (other->start() < end_) return other->start();
    return LifetimePosition::Invalid();
  }

  bool Contains(LifetimePosition point) const {
    return start_ <= point && point < end_;
  }

  // Returns the index of the first gap covered by this interval.
  int FirstGapIndex() const {
    int ret = start_.ToInstructionIndex();
    if (start_.IsInstructionPosition()) {
      ++ret;
    }
    return ret;
  }

  // Returns the index of the last gap covered by this interval.
  int LastGapIndex() const {
    int ret = end_.ToInstructionIndex();
    if (end_.IsGapPosition() && end_.IsStart()) {
      --ret;
    }
    return ret;
  }

 private:
  LifetimePosition start_;
  LifetimePosition end_;
  UseInterval* next_;

  DISALLOW_COPY_AND_ASSIGN(UseInterval);
};

enum class UsePositionType : uint8_t {
  kRegisterOrSlot,
  kRegisterOrSlotOrConstant,
  kRequiresRegister,
  kRequiresSlot
};

enum class UsePositionHintType : uint8_t {
  kNone,
  kOperand,
  kUsePos,
  kPhi,
  kUnresolved
};


static const int32_t kUnassignedRegister =
    RegisterConfiguration::kMaxGeneralRegisters;

static_assert(kUnassignedRegister <= RegisterConfiguration::kMaxFPRegisters,
              "kUnassignedRegister too small");

// Representation of a use position.
class V8_EXPORT_PRIVATE UsePosition final
    : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  UsePosition(LifetimePosition pos, InstructionOperand* operand, void* hint,
              UsePositionHintType hint_type);

  InstructionOperand* operand() const { return operand_; }
  bool HasOperand() const { return operand_ != nullptr; }

  bool RegisterIsBeneficial() const {
    return RegisterBeneficialField::decode(flags_);
  }
  UsePositionType type() const { return TypeField::decode(flags_); }
  void set_type(UsePositionType type, bool register_beneficial);

  LifetimePosition pos() const { return pos_; }

  UsePosition* next() const { return next_; }
  void set_next(UsePosition* next) { next_ = next; }

  // For hinting only.
  void set_assigned_register(int register_code) {
    flags_ = AssignedRegisterField::update(flags_, register_code);
  }

  UsePositionHintType hint_type() const {
    return HintTypeField::decode(flags_);
  }
  bool HasHint() const;
  bool HintRegister(int* register_code) const;
  void SetHint(UsePosition* use_pos);
  void ResolveHint(UsePosition* use_pos);
  bool IsResolved() const {
    return hint_type() != UsePositionHintType::kUnresolved;
  }
  static UsePositionHintType HintTypeForOperand(const InstructionOperand& op);

 private:
  typedef BitField<UsePositionType, 0, 2> TypeField;
  typedef BitField<UsePositionHintType, 2, 3> HintTypeField;
  typedef BitField<bool, 5, 1> RegisterBeneficialField;
  typedef BitField<int32_t, 6, 6> AssignedRegisterField;

  InstructionOperand* const operand_;
  void* hint_;
  UsePosition* next_;
  LifetimePosition const pos_;
  uint32_t flags_;

  DISALLOW_COPY_AND_ASSIGN(UsePosition);
};


class SpillRange;
class RegisterAllocationData;
class TopLevelLiveRange;
class LiveRangeGroup;

// Representation of SSA values' live ranges as a collection of (continuous)
// intervals over the instruction ordering.
class V8_EXPORT_PRIVATE LiveRange : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  UseInterval* first_interval() const { return first_interval_; }
  UsePosition* first_pos() const { return first_pos_; }
  TopLevelLiveRange* TopLevel() { return top_level_; }
  const TopLevelLiveRange* TopLevel() const { return top_level_; }

  bool IsTopLevel() const;

  LiveRange* next() const { return next_; }

  int relative_id() const { return relative_id_; }

  bool IsEmpty() const { return first_interval() == nullptr; }

  InstructionOperand GetAssignedOperand() const;

  MachineRepresentation representation() const {
    return RepresentationField::decode(bits_);
  }

  int assigned_register() const { return AssignedRegisterField::decode(bits_); }
  bool HasRegisterAssigned() const {
    return assigned_register() != kUnassignedRegister;
  }
  void set_assigned_register(int reg);
  void UnsetAssignedRegister();

  bool spilled() const { return SpilledField::decode(bits_); }
  void Spill();

  RegisterKind kind() const;

  // Returns use position in this live range that follows both start
  // and last processed use position.
  UsePosition* NextUsePosition(LifetimePosition start) const;

  // Returns use position for which register is required in this live
  // range and which follows both start and last processed use position
  UsePosition* NextRegisterPosition(LifetimePosition start) const;

  // Returns the first use position requiring stack slot, or nullptr.
  UsePosition* NextSlotPosition(LifetimePosition start) const;

  // Returns use position for which register is beneficial in this live
  // range and which follows both start and last processed use position
  UsePosition* NextUsePositionRegisterIsBeneficial(
      LifetimePosition start) const;

  // Returns lifetime position for which register is beneficial in this live
  // range and which follows both start and last processed use position.
  LifetimePosition NextLifetimePositionRegisterIsBeneficial(
      const LifetimePosition& start) const;

  // Returns use position for which register is beneficial in this live
  // range and which precedes start.
  UsePosition* PreviousUsePositionRegisterIsBeneficial(
      LifetimePosition start) const;

  // Can this live range be spilled at this position.
  bool CanBeSpilled(LifetimePosition pos) const;

  // Splitting primitive used by both splitting and splintering members.
  // Performs the split, but does not link the resulting ranges.
  // The given position must follow the start of the range.
  // All uses following the given position will be moved from this
  // live range to the result live range.
  // The current range will terminate at position, while result will start from
  // position.
  enum HintConnectionOption : bool {
    DoNotConnectHints = false,
    ConnectHints = true
  };
  UsePosition* DetachAt(LifetimePosition position, LiveRange* result,
                        Zone* zone, HintConnectionOption connect_hints);

  // Detaches at position, and then links the resulting ranges. Returns the
  // child, which starts at position.
  LiveRange* SplitAt(LifetimePosition position, Zone* zone);

  // Returns nullptr when no register is hinted, otherwise sets register_index.
  UsePosition* FirstHintPosition(int* register_index) const;
  UsePosition* FirstHintPosition() const {
    int register_index;
    return FirstHintPosition(&register_index);
  }

  UsePosition* current_hint_position() const {
    DCHECK(current_hint_position_ == FirstHintPosition());
    return current_hint_position_;
  }

  LifetimePosition Start() const {
    DCHECK(!IsEmpty());
    return first_interval()->start();
  }

  LifetimePosition End() const {
    DCHECK(!IsEmpty());
    return last_interval_->end();
  }

  bool ShouldBeAllocatedBefore(const LiveRange* other) const;
  bool CanCover(LifetimePosition position) const;
  bool Covers(LifetimePosition position) const;
  LifetimePosition FirstIntersection(LiveRange* other) const;

  void VerifyChildStructure() const {
    VerifyIntervals();
    VerifyPositions();
  }

  void ConvertUsesToOperand(const InstructionOperand& op,
                            const InstructionOperand& spill_op);
  void SetUseHints(int register_index);
  void UnsetUseHints() { SetUseHints(kUnassignedRegister); }

  void Print(const RegisterConfiguration* config, bool with_children) const;
  void Print(bool with_children) const;

 private:
  friend class TopLevelLiveRange;
  explicit LiveRange(int relative_id, MachineRepresentation rep,
                     TopLevelLiveRange* top_level);

  void UpdateParentForAllChildren(TopLevelLiveRange* new_top_level);

  void set_spilled(bool value) { bits_ = SpilledField::update(bits_, value); }

  UseInterval* FirstSearchIntervalForPosition(LifetimePosition position) const;
  void AdvanceLastProcessedMarker(UseInterval* to_start_of,
                                  LifetimePosition but_not_past) const;

  void VerifyPositions() const;
  void VerifyIntervals() const;

  typedef BitField<bool, 0, 1> SpilledField;
  typedef BitField<int32_t, 6, 6> AssignedRegisterField;
  typedef BitField<MachineRepresentation, 12, 8> RepresentationField;

  // Unique among children and splinters of the same virtual register.
  int relative_id_;
  uint32_t bits_;
  UseInterval* last_interval_;
  UseInterval* first_interval_;
  UsePosition* first_pos_;
  TopLevelLiveRange* top_level_;
  LiveRange* next_;
  // This is used as a cache, it doesn't affect correctness.
  mutable UseInterval* current_interval_;
  // This is used as a cache, it doesn't affect correctness.
  mutable UsePosition* last_processed_use_;
  // This is used as a cache, it's invalid outside of BuildLiveRanges.
  mutable UsePosition* current_hint_position_;
  // Cache the last position splintering stopped at.
  mutable UsePosition* splitting_pointer_;

  DISALLOW_COPY_AND_ASSIGN(LiveRange);
};


class LiveRangeGroup final : public ZoneObject {
 public:
  explicit LiveRangeGroup(Zone* zone) : ranges_(zone) {}
  ZoneVector<LiveRange*>& ranges() { return ranges_; }
  const ZoneVector<LiveRange*>& ranges() const { return ranges_; }

  int assigned_register() const { return assigned_register_; }
  void set_assigned_register(int reg) { assigned_register_ = reg; }

 private:
  ZoneVector<LiveRange*> ranges_;
  int assigned_register_;
  DISALLOW_COPY_AND_ASSIGN(LiveRangeGroup);
};

class V8_EXPORT_PRIVATE TopLevelLiveRange final : public LiveRange {
 public:
  explicit TopLevelLiveRange(int vreg, MachineRepresentation rep);
  int spill_start_index() const { return spill_start_index_; }

  bool IsFixed() const { return vreg_ < 0; }

  bool is_phi() const { return IsPhiField::decode(bits_); }
  void set_is_phi(bool value) { bits_ = IsPhiField::update(bits_, value); }

  bool is_non_loop_phi() const { return IsNonLoopPhiField::decode(bits_); }
  void set_is_non_loop_phi(bool value) {
    bits_ = IsNonLoopPhiField::update(bits_, value);
  }

  bool has_slot_use() const { return HasSlotUseField::decode(bits_); }
  void set_has_slot_use(bool value) {
    bits_ = HasSlotUseField::update(bits_, value);
  }

  // Add a new interval or a new use position to this live range.
  void EnsureInterval(LifetimePosition start, LifetimePosition end, Zone* zone);
  void AddUseInterval(LifetimePosition start, LifetimePosition end, Zone* zone);
  void AddUsePosition(UsePosition* pos);

  // Shorten the most recently added interval by setting a new start.
  void ShortenTo(LifetimePosition start);

  // Detaches between start and end, and attributes the resulting range to
  // result.
  // The current range is pointed to as "splintered_from". No parent/child
  // relationship is established between this and result.
  void Splinter(LifetimePosition start, LifetimePosition end, Zone* zone);

  // Assuming other was splintered from this range, embeds other and its
  // children as part of the children sequence of this range.
  void Merge(TopLevelLiveRange* other, Zone* zone);

  // Spill range management.
  void SetSpillRange(SpillRange* spill_range);
  enum class SpillType { kNoSpillType, kSpillOperand, kSpillRange };
  void set_spill_type(SpillType value) {
    bits_ = SpillTypeField::update(bits_, value);
  }
  SpillType spill_type() const { return SpillTypeField::decode(bits_); }
  InstructionOperand* GetSpillOperand() const {
    DCHECK_EQ(SpillType::kSpillOperand, spill_type());
    return spill_operand_;
  }

  SpillRange* GetAllocatedSpillRange() const {
    DCHECK_NE(SpillType::kSpillOperand, spill_type());
    return spill_range_;
  }

  SpillRange* GetSpillRange() const {
    DCHECK_EQ(SpillType::kSpillRange, spill_type());
    return spill_range_;
  }
  bool HasNoSpillType() const {
    return spill_type() == SpillType::kNoSpillType;
  }
  bool HasSpillOperand() const {
    return spill_type() == SpillType::kSpillOperand;
  }
  bool HasSpillRange() const { return spill_type() == SpillType::kSpillRange; }

  AllocatedOperand GetSpillRangeOperand() const;

  void RecordSpillLocation(Zone* zone, int gap_index,
                           InstructionOperand* operand);
  void SetSpillOperand(InstructionOperand* operand);
  void SetSpillStartIndex(int start) {
    spill_start_index_ = Min(start, spill_start_index_);
  }

  void CommitSpillMoves(InstructionSequence* sequence,
                        const InstructionOperand& operand,
                        bool might_be_duplicated);

  // If all the children of this range are spilled in deferred blocks, and if
  // for any non-spilled child with a use position requiring a slot, that range
  // is contained in a deferred block, mark the range as
  // IsSpilledOnlyInDeferredBlocks, so that we avoid spilling at definition,
  // and instead let the LiveRangeConnector perform the spills within the
  // deferred blocks. If so, we insert here spills for non-spilled ranges
  // with slot use positions.
  void TreatAsSpilledInDeferredBlock(Zone* zone, int total_block_count) {
    spill_start_index_ = -1;
    spilled_in_deferred_blocks_ = true;
    spill_move_insertion_locations_ = nullptr;
    list_of_blocks_requiring_spill_operands_ =
        new (zone) BitVector(total_block_count, zone);
  }

  void CommitSpillInDeferredBlocks(RegisterAllocationData* data,
                                   const InstructionOperand& spill_operand,
                                   BitVector* necessary_spill_points);

  TopLevelLiveRange* splintered_from() const { return splintered_from_; }
  bool IsSplinter() const { return splintered_from_ != nullptr; }
  bool MayRequireSpillRange() const {
    DCHECK(!IsSplinter());
    return !HasSpillOperand() && spill_range_ == nullptr;
  }
  void UpdateSpillRangePostMerge(TopLevelLiveRange* merged);
  int vreg() const { return vreg_; }

#if DEBUG
  int debug_virt_reg() const;
#endif

  void Verify() const;
  void VerifyChildrenInOrder() const;

  int GetNextChildId() {
    return IsSplinter() ? splintered_from()->GetNextChildId()
                        : ++last_child_id_;
  }

  int GetChildCount() const { return last_child_id_ + 1; }

  bool IsSpilledOnlyInDeferredBlocks() const {
    return spilled_in_deferred_blocks_;
  }

  struct SpillMoveInsertionList;

  SpillMoveInsertionList* GetSpillMoveInsertionLocations() const {
    DCHECK(!IsSpilledOnlyInDeferredBlocks());
    return spill_move_insertion_locations_;
  }
  TopLevelLiveRange* splinter() const { return splinter_; }
  void SetSplinter(TopLevelLiveRange* splinter) {
    DCHECK_NULL(splinter_);
    DCHECK_NOT_NULL(splinter);

    splinter_ = splinter;
    splinter->relative_id_ = GetNextChildId();
    splinter->set_spill_type(spill_type());
    splinter->SetSplinteredFrom(this);
  }

  void MarkHasPreassignedSlot() { has_preassigned_slot_ = true; }
  bool has_preassigned_slot() const { return has_preassigned_slot_; }

  void AddBlockRequiringSpillOperand(RpoNumber block_id) {
    DCHECK(IsSpilledOnlyInDeferredBlocks());
    GetListOfBlocksRequiringSpillOperands()->Add(block_id.ToInt());
  }

  BitVector* GetListOfBlocksRequiringSpillOperands() const {
    DCHECK(IsSpilledOnlyInDeferredBlocks());
    return list_of_blocks_requiring_spill_operands_;
  }

 private:
  void SetSplinteredFrom(TopLevelLiveRange* splinter_parent);

  typedef BitField<bool, 1, 1> HasSlotUseField;
  typedef BitField<bool, 2, 1> IsPhiField;
  typedef BitField<bool, 3, 1> IsNonLoopPhiField;
  typedef BitField<SpillType, 4, 2> SpillTypeField;

  int vreg_;
  int last_child_id_;
  TopLevelLiveRange* splintered_from_;
  union {
    // Correct value determined by spill_type()
    InstructionOperand* spill_operand_;
    SpillRange* spill_range_;
  };

  union {
    SpillMoveInsertionList* spill_move_insertion_locations_;
    BitVector* list_of_blocks_requiring_spill_operands_;
  };

  // TODO(mtrofin): generalize spilling after definition, currently specialized
  // just for spill in a single deferred block.
  bool spilled_in_deferred_blocks_;
  int spill_start_index_;
  UsePosition* last_pos_;
  TopLevelLiveRange* splinter_;
  bool has_preassigned_slot_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelLiveRange);
};


struct PrintableLiveRange {
  const RegisterConfiguration* register_configuration_;
  const LiveRange* range_;
};


std::ostream& operator<<(std::ostream& os,
                         const PrintableLiveRange& printable_range);


class SpillRange final : public ZoneObject {
 public:
  static const int kUnassignedSlot = -1;
  SpillRange(TopLevelLiveRange* range, Zone* zone);

  UseInterval* interval() const { return use_interval_; }

  bool IsEmpty() const { return live_ranges_.empty(); }
  bool TryMerge(SpillRange* other);
  bool HasSlot() const { return assigned_slot_ != kUnassignedSlot; }

  void set_assigned_slot(int index) {
    DCHECK_EQ(kUnassignedSlot, assigned_slot_);
    assigned_slot_ = index;
  }
  int assigned_slot() {
    DCHECK_NE(kUnassignedSlot, assigned_slot_);
    return assigned_slot_;
  }
  const ZoneVector<TopLevelLiveRange*>& live_ranges() const {
    return live_ranges_;
  }
  ZoneVector<TopLevelLiveRange*>& live_ranges() { return live_ranges_; }
  // Spill slots can be 4, 8, or 16 bytes wide.
  int byte_width() const { return byte_width_; }
  void Print() const;

 private:
  LifetimePosition End() const { return end_position_; }
  bool IsIntersectingWith(SpillRange* other) const;
  // Merge intervals, making sure the use intervals are sorted
  void MergeDisjointIntervals(UseInterval* other);

  ZoneVector<TopLevelLiveRange*> live_ranges_;
  UseInterval* use_interval_;
  LifetimePosition end_position_;
  int assigned_slot_;
  int byte_width_;

  DISALLOW_COPY_AND_ASSIGN(SpillRange);
};


class RegisterAllocationData final : public ZoneObject {
 public:
  class PhiMapValue : public ZoneObject {
   public:
    PhiMapValue(PhiInstruction* phi, const InstructionBlock* block, Zone* zone);

    const PhiInstruction* phi() const { return phi_; }
    const InstructionBlock* block() const { return block_; }

    // For hinting.
    int assigned_register() const { return assigned_register_; }
    void set_assigned_register(int register_code) {
      DCHECK_EQ(assigned_register_, kUnassignedRegister);
      assigned_register_ = register_code;
    }
    void UnsetAssignedRegister() { assigned_register_ = kUnassignedRegister; }

    void AddOperand(InstructionOperand* operand);
    void CommitAssignment(const InstructionOperand& operand);

   private:
    PhiInstruction* const phi_;
    const InstructionBlock* const block_;
    ZoneVector<InstructionOperand*> incoming_operands_;
    int assigned_register_;
  };
  typedef ZoneMap<int, PhiMapValue*> PhiMap;

  struct DelayedReference {
    ReferenceMap* map;
    InstructionOperand* operand;
  };
  typedef ZoneVector<DelayedReference> DelayedReferences;
  typedef ZoneVector<std::pair<TopLevelLiveRange*, int>>
      RangesWithPreassignedSlots;

  RegisterAllocationData(const RegisterConfiguration* config,
                         Zone* allocation_zone, Frame* frame,
                         InstructionSequence* code,
                         const char* debug_name = nullptr);

  const ZoneVector<TopLevelLiveRange*>& live_ranges() const {
    return live_ranges_;
  }
  ZoneVector<TopLevelLiveRange*>& live_ranges() { return live_ranges_; }
  const ZoneVector<TopLevelLiveRange*>& fixed_live_ranges() const {
    return fixed_live_ranges_;
  }
  ZoneVector<TopLevelLiveRange*>& fixed_live_ranges() {
    return fixed_live_ranges_;
  }
  ZoneVector<TopLevelLiveRange*>& fixed_float_live_ranges() {
    return fixed_float_live_ranges_;
  }
  const ZoneVector<TopLevelLiveRange*>& fixed_float_live_ranges() const {
    return fixed_float_live_ranges_;
  }
  ZoneVector<TopLevelLiveRange*>& fixed_double_live_ranges() {
    return fixed_double_live_ranges_;
  }
  const ZoneVector<TopLevelLiveRange*>& fixed_double_live_ranges() const {
    return fixed_double_live_ranges_;
  }
  ZoneVector<TopLevelLiveRange*>& fixed_simd128_live_ranges() {
    return fixed_simd128_live_ranges_;
  }
  const ZoneVector<TopLevelLiveRange*>& fixed_simd128_live_ranges() const {
    return fixed_simd128_live_ranges_;
  }
  ZoneVector<BitVector*>& live_in_sets() { return live_in_sets_; }
  ZoneVector<BitVector*>& live_out_sets() { return live_out_sets_; }
  ZoneVector<SpillRange*>& spill_ranges() { return spill_ranges_; }
  DelayedReferences& delayed_references() { return delayed_references_; }
  InstructionSequence* code() const { return code_; }
  // This zone is for data structures only needed during register allocation
  // phases.
  Zone* allocation_zone() const { return allocation_zone_; }
  // This zone is for InstructionOperands and moves that live beyond register
  // allocation.
  Zone* code_zone() const { return code()->zone(); }
  Frame* frame() const { return frame_; }
  const char* debug_name() const { return debug_name_; }
  const RegisterConfiguration* config() const { return config_; }

  MachineRepresentation RepresentationFor(int virtual_register);

  TopLevelLiveRange* GetOrCreateLiveRangeFor(int index);
  // Creates a new live range.
  TopLevelLiveRange* NewLiveRange(int index, MachineRepresentation rep);
  TopLevelLiveRange* NextLiveRange(MachineRepresentation rep);

  SpillRange* AssignSpillRangeToLiveRange(TopLevelLiveRange* range);
  SpillRange* CreateSpillRangeForLiveRange(TopLevelLiveRange* range);

  MoveOperands* AddGapMove(int index, Instruction::GapPosition position,
                           const InstructionOperand& from,
                           const InstructionOperand& to);

  bool IsReference(TopLevelLiveRange* top_range) const {
    return code()->IsReference(top_range->vreg());
  }

  bool ExistsUseWithoutDefinition();
  bool RangesDefinedInDeferredStayInDeferred();

  void MarkAllocated(MachineRepresentation rep, int index);

  PhiMapValue* InitializePhiMap(const InstructionBlock* block,
                                PhiInstruction* phi);
  PhiMapValue* GetPhiMapValueFor(TopLevelLiveRange* top_range);
  PhiMapValue* GetPhiMapValueFor(int virtual_register);
  bool IsBlockBoundary(LifetimePosition pos) const;

  RangesWithPreassignedSlots& preassigned_slot_ranges() {
    return preassigned_slot_ranges_;
  }

 private:
  int GetNextLiveRangeId();

  Zone* const allocation_zone_;
  Frame* const frame_;
  InstructionSequence* const code_;
  const char* const debug_name_;
  const RegisterConfiguration* const config_;
  PhiMap phi_map_;
  ZoneVector<BitVector*> live_in_sets_;
  ZoneVector<BitVector*> live_out_sets_;
  ZoneVector<TopLevelLiveRange*> live_ranges_;
  ZoneVector<TopLevelLiveRange*> fixed_live_ranges_;
  ZoneVector<TopLevelLiveRange*> fixed_float_live_ranges_;
  ZoneVector<TopLevelLiveRange*> fixed_double_live_ranges_;
  ZoneVector<TopLevelLiveRange*> fixed_simd128_live_ranges_;
  ZoneVector<SpillRange*> spill_ranges_;
  DelayedReferences delayed_references_;
  BitVector* assigned_registers_;
  BitVector* assigned_double_registers_;
  int virtual_register_count_;
  RangesWithPreassignedSlots preassigned_slot_ranges_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAllocationData);
};


class ConstraintBuilder final : public ZoneObject {
 public:
  explicit ConstraintBuilder(RegisterAllocationData* data);

  // Phase 1 : insert moves to account for fixed register operands.
  void MeetRegisterConstraints();

  // Phase 2: deconstruct SSA by inserting moves in successors and the headers
  // of blocks containing phis.
  void ResolvePhis();

 private:
  RegisterAllocationData* data() const { return data_; }
  InstructionSequence* code() const { return data()->code(); }
  Zone* allocation_zone() const { return data()->allocation_zone(); }

  InstructionOperand* AllocateFixed(UnallocatedOperand* operand, int pos,
                                    bool is_tagged);
  void MeetRegisterConstraints(const InstructionBlock* block);
  void MeetConstraintsBefore(int index);
  void MeetConstraintsAfter(int index);
  void MeetRegisterConstraintsForLastInstructionInBlock(
      const InstructionBlock* block);
  void ResolvePhis(const InstructionBlock* block);

  RegisterAllocationData* const data_;

  DISALLOW_COPY_AND_ASSIGN(ConstraintBuilder);
};


class LiveRangeBuilder final : public ZoneObject {
 public:
  explicit LiveRangeBuilder(RegisterAllocationData* data, Zone* local_zone);

  // Phase 3: compute liveness of all virtual register.
  void BuildLiveRanges();
  static BitVector* ComputeLiveOut(const InstructionBlock* block,
                                   RegisterAllocationData* data);

 private:
  RegisterAllocationData* data() const { return data_; }
  InstructionSequence* code() const { return data()->code(); }
  Zone* allocation_zone() const { return data()->allocation_zone(); }
  Zone* code_zone() const { return code()->zone(); }
  const RegisterConfiguration* config() const { return data()->config(); }
  ZoneVector<BitVector*>& live_in_sets() const {
    return data()->live_in_sets();
  }

  // Verification.
  void Verify() const;
  bool IntervalStartsAtBlockBoundary(const UseInterval* interval) const;
  bool IntervalPredecessorsCoveredByRange(const UseInterval* interval,
                                          const TopLevelLiveRange* range) const;
  bool NextIntervalStartsInDifferentBlocks(const UseInterval* interval) const;

  // Liveness analysis support.
  void AddInitialIntervals(const InstructionBlock* block, BitVector* live_out);
  void ProcessInstructions(const InstructionBlock* block, BitVector* live);
  void ProcessPhis(const InstructionBlock* block, BitVector* live);
  void ProcessLoopHeader(const InstructionBlock* block, BitVector* live);

  static int FixedLiveRangeID(int index) { return -index - 1; }
  int FixedFPLiveRangeID(int index, MachineRepresentation rep);
  TopLevelLiveRange* FixedLiveRangeFor(int index);
  TopLevelLiveRange* FixedFPLiveRangeFor(int index, MachineRepresentation rep);

  void MapPhiHint(InstructionOperand* operand, UsePosition* use_pos);
  void ResolvePhiHint(InstructionOperand* operand, UsePosition* use_pos);

  UsePosition* NewUsePosition(LifetimePosition pos, InstructionOperand* operand,
                              void* hint, UsePositionHintType hint_type);
  UsePosition* NewUsePosition(LifetimePosition pos) {
    return NewUsePosition(pos, nullptr, nullptr, UsePositionHintType::kNone);
  }
  TopLevelLiveRange* LiveRangeFor(InstructionOperand* operand);
  // Helper methods for building intervals.
  UsePosition* Define(LifetimePosition position, InstructionOperand* operand,
                      void* hint, UsePositionHintType hint_type);
  void Define(LifetimePosition position, InstructionOperand* operand) {
    Define(position, operand, nullptr, UsePositionHintType::kNone);
  }
  UsePosition* Use(LifetimePosition block_start, LifetimePosition position,
                   InstructionOperand* operand, void* hint,
                   UsePositionHintType hint_type);
  void Use(LifetimePosition block_start, LifetimePosition position,
           InstructionOperand* operand) {
    Use(block_start, position, operand, nullptr, UsePositionHintType::kNone);
  }

  RegisterAllocationData* const data_;
  ZoneMap<InstructionOperand*, UsePosition*> phi_hints_;

  DISALLOW_COPY_AND_ASSIGN(LiveRangeBuilder);
};


class RegisterAllocator : public ZoneObject {
 public:
  RegisterAllocator(RegisterAllocationData* data, RegisterKind kind);

 protected:
  RegisterAllocationData* data() const { return data_; }
  InstructionSequence* code() const { return data()->code(); }
  RegisterKind mode() const { return mode_; }
  int num_registers() const { return num_registers_; }
  int num_allocatable_registers() const { return num_allocatable_registers_; }
  const int* allocatable_register_codes() const {
    return allocatable_register_codes_;
  }
  // Returns true iff. we must check float register aliasing.
  bool check_fp_aliasing() const { return check_fp_aliasing_; }

  // TODO(mtrofin): explain why splitting in gap START is always OK.
  LifetimePosition GetSplitPositionForInstruction(const LiveRange* range,
                                                  int instruction_index);

  Zone* allocation_zone() const { return data()->allocation_zone(); }

  // Find the optimal split for ranges defined by a memory operand, e.g.
  // constants or function parameters passed on the stack.
  void SplitAndSpillRangesDefinedByMemoryOperand();

  // Split the given range at the given position.
  // If range starts at or after the given position then the
  // original range is returned.
  // Otherwise returns the live range that starts at pos and contains
  // all uses from the original range that follow pos. Uses at pos will
  // still be owned by the original range after splitting.
  LiveRange* SplitRangeAt(LiveRange* range, LifetimePosition pos);

  bool CanProcessRange(LiveRange* range) const {
    return range != nullptr && !range->IsEmpty() && range->kind() == mode();
  }


  // Split the given range in a position from the interval [start, end].
  LiveRange* SplitBetween(LiveRange* range, LifetimePosition start,
                          LifetimePosition end);

  // Find a lifetime position in the interval [start, end] which
  // is optimal for splitting: it is either header of the outermost
  // loop covered by this interval or the latest possible position.
  LifetimePosition FindOptimalSplitPos(LifetimePosition start,
                                       LifetimePosition end);

  void Spill(LiveRange* range);

  // If we are trying to spill a range inside the loop try to
  // hoist spill position out to the point just before the loop.
  LifetimePosition FindOptimalSpillingPos(LiveRange* range,
                                          LifetimePosition pos);

  const ZoneVector<TopLevelLiveRange*>& GetFixedRegisters() const;
  const char* RegisterName(int allocation_index) const;

 private:
  RegisterAllocationData* const data_;
  const RegisterKind mode_;
  const int num_registers_;
  int num_allocatable_registers_;
  const int* allocatable_register_codes_;
  bool check_fp_aliasing_;

 private:
  bool no_combining_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAllocator);
};


class LinearScanAllocator final : public RegisterAllocator {
 public:
  LinearScanAllocator(RegisterAllocationData* data, RegisterKind kind,
                      Zone* local_zone);

  // Phase 4: compute register assignments.
  void AllocateRegisters();

 private:
  ZoneVector<LiveRange*>& unhandled_live_ranges() {
    return unhandled_live_ranges_;
  }
  ZoneVector<LiveRange*>& active_live_ranges() { return active_live_ranges_; }
  ZoneVector<LiveRange*>& inactive_live_ranges() {
    return inactive_live_ranges_;
  }

  void SetLiveRangeAssignedRegister(LiveRange* range, int reg);

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
  bool TryReuseSpillForPhi(TopLevelLiveRange* range);
  bool TryAllocateFreeReg(LiveRange* range,
                          const Vector<LifetimePosition>& free_until_pos);
  bool TryAllocatePreferredReg(LiveRange* range,
                               const Vector<LifetimePosition>& free_until_pos);
  void GetFPRegisterSet(MachineRepresentation rep, int* num_regs,
                        int* num_codes, const int** codes) const;
  void FindFreeRegistersForRange(LiveRange* range,
                                 Vector<LifetimePosition> free_until_pos);
  void ProcessCurrentRange(LiveRange* current);
  void AllocateBlockedReg(LiveRange* range);
  bool TrySplitAndSpillSplinter(LiveRange* range);

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

  ZoneVector<LiveRange*> unhandled_live_ranges_;
  ZoneVector<LiveRange*> active_live_ranges_;
  ZoneVector<LiveRange*> inactive_live_ranges_;

#ifdef DEBUG
  LifetimePosition allocation_finger_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LinearScanAllocator);
};


class SpillSlotLocator final : public ZoneObject {
 public:
  explicit SpillSlotLocator(RegisterAllocationData* data);

  void LocateSpillSlots();

 private:
  RegisterAllocationData* data() const { return data_; }

  RegisterAllocationData* const data_;

  DISALLOW_COPY_AND_ASSIGN(SpillSlotLocator);
};


class OperandAssigner final : public ZoneObject {
 public:
  explicit OperandAssigner(RegisterAllocationData* data);

  // Phase 5: assign spill splots.
  void AssignSpillSlots();

  // Phase 6: commit assignment.
  void CommitAssignment();

 private:
  RegisterAllocationData* data() const { return data_; }

  RegisterAllocationData* const data_;

  DISALLOW_COPY_AND_ASSIGN(OperandAssigner);
};


class ReferenceMapPopulator final : public ZoneObject {
 public:
  explicit ReferenceMapPopulator(RegisterAllocationData* data);

  // Phase 7: compute values for pointer maps.
  void PopulateReferenceMaps();

 private:
  RegisterAllocationData* data() const { return data_; }

  bool SafePointsAreInOrder() const;

  RegisterAllocationData* const data_;

  DISALLOW_COPY_AND_ASSIGN(ReferenceMapPopulator);
};


class LiveRangeBoundArray;
// Insert moves of the form
//
//          Operand(child_(k+1)) = Operand(child_k)
//
// where child_k and child_(k+1) are consecutive children of a range (so
// child_k->next() == child_(k+1)), and Operand(...) refers to the
// assigned operand, be it a register or a slot.
class LiveRangeConnector final : public ZoneObject {
 public:
  explicit LiveRangeConnector(RegisterAllocationData* data);

  // Phase 8: reconnect split ranges with moves, when the control flow
  // between the ranges is trivial (no branches).
  void ConnectRanges(Zone* local_zone);

  // Phase 9: insert moves to connect ranges across basic blocks, when the
  // control flow between them cannot be trivially resolved, such as joining
  // branches.
  void ResolveControlFlow(Zone* local_zone);

 private:
  RegisterAllocationData* data() const { return data_; }
  InstructionSequence* code() const { return data()->code(); }
  Zone* code_zone() const { return code()->zone(); }

  bool CanEagerlyResolveControlFlow(const InstructionBlock* block) const;

  int ResolveControlFlow(const InstructionBlock* block,
                         const InstructionOperand& cur_op,
                         const InstructionBlock* pred,
                         const InstructionOperand& pred_op);

  void CommitSpillsInDeferredBlocks(TopLevelLiveRange* range,
                                    LiveRangeBoundArray* array,
                                    Zone* temp_zone);

  RegisterAllocationData* const data_;

  DISALLOW_COPY_AND_ASSIGN(LiveRangeConnector);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_REGISTER_ALLOCATOR_H_

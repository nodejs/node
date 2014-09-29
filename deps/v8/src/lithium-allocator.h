// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LITHIUM_ALLOCATOR_H_
#define V8_LITHIUM_ALLOCATOR_H_

#include "src/v8.h"

#include "src/allocation.h"
#include "src/lithium.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
class HBasicBlock;
class HGraph;
class HPhi;
class HTracer;
class HValue;
class BitVector;
class StringStream;

class LPlatformChunk;
class LOperand;
class LUnallocated;
class LGap;
class LParallelMove;
class LPointerMap;


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
    DCHECK(IsValid());
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
    DCHECK(IsValid());
    return LifetimePosition(value_ & ~(kStep - 1));
  }

  // Returns the lifetime position for the end of the instruction which
  // corresponds to this lifetime position.
  LifetimePosition InstructionEnd() const {
    DCHECK(IsValid());
    return LifetimePosition(InstructionStart().Value() + kStep/2);
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

  explicit LifetimePosition(int value) : value_(value) { }

  int value_;
};


// Representation of the non-empty interval [start,end[.
class UseInterval: public ZoneObject {
 public:
  UseInterval(LifetimePosition start, LifetimePosition end)
      : start_(start), end_(end), next_(NULL) {
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
  UsePosition(LifetimePosition pos, LOperand* operand, LOperand* hint);

  LOperand* operand() const { return operand_; }
  bool HasOperand() const { return operand_ != NULL; }

  LOperand* hint() const { return hint_; }
  bool HasHint() const;
  bool RequiresRegister() const;
  bool RegisterIsBeneficial() const;

  LifetimePosition pos() const { return pos_; }
  UsePosition* next() const { return next_; }

 private:
  void set_next(UsePosition* next) { next_ = next; }

  LOperand* const operand_;
  LOperand* const hint_;
  LifetimePosition const pos_;
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

  LiveRange(int id, Zone* zone);

  UseInterval* first_interval() const { return first_interval_; }
  UsePosition* first_pos() const { return first_pos_; }
  LiveRange* parent() const { return parent_; }
  LiveRange* TopLevel() { return (parent_ == NULL) ? this : parent_; }
  LiveRange* next() const { return next_; }
  bool IsChild() const { return parent() != NULL; }
  int id() const { return id_; }
  bool IsFixed() const { return id_ < 0; }
  bool IsEmpty() const { return first_interval() == NULL; }
  LOperand* CreateAssignedOperand(Zone* zone);
  int assigned_register() const { return assigned_register_; }
  int spill_start_index() const { return spill_start_index_; }
  void set_assigned_register(int reg, Zone* zone);
  void MakeSpilled(Zone* zone);

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

  LOperand* current_hint_operand() const {
    DCHECK(current_hint_operand_ == FirstHint());
    return current_hint_operand_;
  }
  LOperand* FirstHint() const {
    UsePosition* pos = first_pos_;
    while (pos != NULL && !pos->HasHint()) pos = pos->next();
    if (pos != NULL) return pos->hint();
    return NULL;
  }

  LifetimePosition Start() const {
    DCHECK(!IsEmpty());
    return first_interval()->start();
  }

  LifetimePosition End() const {
    DCHECK(!IsEmpty());
    return last_interval_->end();
  }

  bool HasAllocatedSpillOperand() const;
  LOperand* GetSpillOperand() const { return spill_operand_; }
  void SetSpillOperand(LOperand* operand);

  void SetSpillStartIndex(int start) {
    spill_start_index_ = Min(start, spill_start_index_);
  }

  bool ShouldBeAllocatedBefore(const LiveRange* other) const;
  bool CanCover(LifetimePosition position) const;
  bool Covers(LifetimePosition position);
  LifetimePosition FirstIntersection(LiveRange* other);

  // Add a new interval or a new use position to this live range.
  void EnsureInterval(LifetimePosition start,
                      LifetimePosition end,
                      Zone* zone);
  void AddUseInterval(LifetimePosition start,
                      LifetimePosition end,
                      Zone* zone);
  void AddUsePosition(LifetimePosition pos,
                      LOperand* operand,
                      LOperand* hint,
                      Zone* zone);

  // Shorten the most recently added interval by setting a new start.
  void ShortenTo(LifetimePosition start);

#ifdef DEBUG
  // True if target overlaps an existing interval.
  bool HasOverlap(UseInterval* target) const;
  void Verify() const;
#endif

 private:
  void ConvertOperands(Zone* zone);
  UseInterval* FirstSearchIntervalForPosition(LifetimePosition position) const;
  void AdvanceLastProcessedMarker(UseInterval* to_start_of,
                                  LifetimePosition but_not_past) const;

  int id_;
  bool spilled_;
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
  LOperand* current_hint_operand_;
  LOperand* spill_operand_;
  int spill_start_index_;

  friend class LAllocator;  // Assigns to kind_.
};


class LAllocator BASE_EMBEDDED {
 public:
  LAllocator(int first_virtual_register, HGraph* graph);

  static void TraceAlloc(const char* msg, ...);

  // Checks whether the value of a given virtual register is tagged.
  bool HasTaggedValue(int virtual_register) const;

  // Returns the register kind required by the given virtual register.
  RegisterKind RequiredRegisterKind(int virtual_register) const;

  bool Allocate(LChunk* chunk);

  const ZoneList<LiveRange*>* live_ranges() const { return &live_ranges_; }
  const Vector<LiveRange*>* fixed_live_ranges() const {
    return &fixed_live_ranges_;
  }
  const Vector<LiveRange*>* fixed_double_live_ranges() const {
    return &fixed_double_live_ranges_;
  }

  LPlatformChunk* chunk() const { return chunk_; }
  HGraph* graph() const { return graph_; }
  Isolate* isolate() const { return graph_->isolate(); }
  Zone* zone() { return &zone_; }

  int GetVirtualRegister() {
    if (next_virtual_register_ >= LUnallocated::kMaxVirtualRegisters) {
      allocation_ok_ = false;
      // Maintain the invariant that we return something below the maximum.
      return 0;
    }
    return next_virtual_register_++;
  }

  bool AllocationOk() { return allocation_ok_; }

  void MarkAsOsrEntry() {
    // There can be only one.
    DCHECK(!has_osr_entry_);
    // Simply set a flag to find and process instruction later.
    has_osr_entry_ = true;
  }

#ifdef DEBUG
  void Verify() const;
#endif

  BitVector* assigned_registers() {
    return assigned_registers_;
  }
  BitVector* assigned_double_registers() {
    return assigned_double_registers_;
  }

 private:
  void MeetRegisterConstraints();
  void ResolvePhis();
  void BuildLiveRanges();
  void AllocateGeneralRegisters();
  void AllocateDoubleRegisters();
  void ConnectRanges();
  void ResolveControlFlow();
  void PopulatePointerMaps();
  void AllocateRegisters();
  bool CanEagerlyResolveControlFlow(HBasicBlock* block) const;
  inline bool SafePointsAreInOrder() const;

  // Liveness analysis support.
  void InitializeLivenessAnalysis();
  BitVector* ComputeLiveOut(HBasicBlock* block);
  void AddInitialIntervals(HBasicBlock* block, BitVector* live_out);
  void ProcessInstructions(HBasicBlock* block, BitVector* live);
  void MeetRegisterConstraints(HBasicBlock* block);
  void MeetConstraintsBetween(LInstruction* first,
                              LInstruction* second,
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
  LiveRange* SplitRangeAt(LiveRange* range, LifetimePosition pos);

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

  // Spill the given life range after position [start] and up to position [end].
  void SpillBetween(LiveRange* range,
                    LifetimePosition start,
                    LifetimePosition end);

  // Spill the given life range after position [start] and up to position [end].
  // Range is guaranteed to be spilled at least until position [until].
  void SpillBetweenUntil(LiveRange* range,
                         LifetimePosition start,
                         LifetimePosition until,
                         LifetimePosition end);

  void SplitAndSpillIntersecting(LiveRange* range);

  // If we are trying to spill a range inside the loop try to
  // hoist spill position out to the point just before the loop.
  LifetimePosition FindOptimalSpillingPos(LiveRange* range,
                                          LifetimePosition pos);

  void Spill(LiveRange* range);
  bool IsBlockBoundary(LifetimePosition pos);

  // Helper methods for resolving control flow.
  void ResolveControlFlow(LiveRange* range,
                          HBasicBlock* block,
                          HBasicBlock* pred);

  inline void SetLiveRangeAssignedRegister(LiveRange* range, int reg);

  // Return parallel move that should be used to connect ranges split at the
  // given position.
  LParallelMove* GetConnectingParallelMove(LifetimePosition pos);

  // Return the block which contains give lifetime position.
  HBasicBlock* GetBlock(LifetimePosition pos);

  // Helper methods for the fixed registers.
  int RegisterCount() const;
  static int FixedLiveRangeID(int index) { return -index - 1; }
  static int FixedDoubleLiveRangeID(int index);
  LiveRange* FixedLiveRangeFor(int index);
  LiveRange* FixedDoubleLiveRangeFor(int index);
  LiveRange* LiveRangeFor(int index);
  HPhi* LookupPhi(LOperand* operand) const;
  LGap* GetLastGap(HBasicBlock* block);

  const char* RegisterName(int allocation_index);

  inline bool IsGapAt(int index);

  inline LInstruction* InstructionAt(int index);

  inline LGap* GapAt(int index);

  Zone zone_;

  LPlatformChunk* chunk_;

  // During liveness analysis keep a mapping from block id to live_in sets
  // for blocks already analyzed.
  ZoneList<BitVector*> live_in_sets_;

  // Liveness analysis results.
  ZoneList<LiveRange*> live_ranges_;

  // Lists of live ranges
  EmbeddedVector<LiveRange*, Register::kMaxNumAllocatableRegisters>
      fixed_live_ranges_;
  EmbeddedVector<LiveRange*, DoubleRegister::kMaxNumAllocatableRegisters>
      fixed_double_live_ranges_;
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

  BitVector* assigned_registers_;
  BitVector* assigned_double_registers_;

  HGraph* graph_;

  bool has_osr_entry_;

  // Indicates success or failure during register allocation.
  bool allocation_ok_;

#ifdef DEBUG
  LifetimePosition allocation_finger_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LAllocator);
};


class LAllocatorPhase : public CompilationPhase {
 public:
  LAllocatorPhase(const char* name, LAllocator* allocator);
  ~LAllocatorPhase();

 private:
  LAllocator* allocator_;
  unsigned allocator_zone_start_allocation_size_;

  DISALLOW_COPY_AND_ASSIGN(LAllocatorPhase);
};


} }  // namespace v8::internal

#endif  // V8_LITHIUM_ALLOCATOR_H_

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/adapters.h"
#include "src/compiler/linkage.h"
#include "src/compiler/register-allocator.h"
#include "src/string-stream.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...)                             \
  do {                                         \
    if (FLAG_trace_alloc) PrintF(__VA_ARGS__); \
  } while (false)


namespace {

void RemoveElement(ZoneVector<LiveRange*>* v, LiveRange* range) {
  auto it = std::find(v->begin(), v->end(), range);
  DCHECK(it != v->end());
  v->erase(it);
}


int GetRegisterCount(const RegisterConfiguration* cfg, RegisterKind kind) {
  return kind == DOUBLE_REGISTERS ? cfg->num_aliased_double_registers()
                                  : cfg->num_general_registers();
}


const InstructionBlock* GetContainingLoop(const InstructionSequence* sequence,
                                          const InstructionBlock* block) {
  auto index = block->loop_header();
  if (!index.IsValid()) return nullptr;
  return sequence->InstructionBlockAt(index);
}


const InstructionBlock* GetInstructionBlock(const InstructionSequence* code,
                                            LifetimePosition pos) {
  return code->GetInstructionBlock(pos.ToInstructionIndex());
}


Instruction* GetLastInstruction(InstructionSequence* code,
                                const InstructionBlock* block) {
  return code->InstructionAt(block->last_instruction_index());
}


bool IsOutputRegisterOf(Instruction* instr, int index) {
  for (size_t i = 0; i < instr->OutputCount(); i++) {
    auto output = instr->OutputAt(i);
    if (output->IsRegister() &&
        RegisterOperand::cast(output)->index() == index) {
      return true;
    }
  }
  return false;
}


bool IsOutputDoubleRegisterOf(Instruction* instr, int index) {
  for (size_t i = 0; i < instr->OutputCount(); i++) {
    auto output = instr->OutputAt(i);
    if (output->IsDoubleRegister() &&
        DoubleRegisterOperand::cast(output)->index() == index) {
      return true;
    }
  }
  return false;
}


// TODO(dcarney): fix frame to allow frame accesses to half size location.
int GetByteWidth(MachineType machine_type) {
  DCHECK_EQ(RepresentationOf(machine_type), machine_type);
  switch (machine_type) {
    case kRepBit:
    case kRepWord8:
    case kRepWord16:
    case kRepWord32:
    case kRepTagged:
      return kPointerSize;
    case kRepFloat32:
    case kRepWord64:
    case kRepFloat64:
      return 8;
    default:
      UNREACHABLE();
      return 0;
  }
}

}  // namespace


UsePosition::UsePosition(LifetimePosition pos, InstructionOperand* operand,
                         void* hint, UsePositionHintType hint_type)
    : operand_(operand), hint_(hint), next_(nullptr), pos_(pos), flags_(0) {
  DCHECK_IMPLIES(hint == nullptr, hint_type == UsePositionHintType::kNone);
  bool register_beneficial = true;
  UsePositionType type = UsePositionType::kAny;
  if (operand_ != nullptr && operand_->IsUnallocated()) {
    const UnallocatedOperand* unalloc = UnallocatedOperand::cast(operand_);
    if (unalloc->HasRegisterPolicy()) {
      type = UsePositionType::kRequiresRegister;
    } else if (unalloc->HasSlotPolicy()) {
      type = UsePositionType::kRequiresSlot;
      register_beneficial = false;
    } else {
      register_beneficial = !unalloc->HasAnyPolicy();
    }
  }
  flags_ = TypeField::encode(type) | HintTypeField::encode(hint_type) |
           RegisterBeneficialField::encode(register_beneficial) |
           AssignedRegisterField::encode(kUnassignedRegister);
  DCHECK(pos_.IsValid());
}


bool UsePosition::HasHint() const {
  int hint_register;
  return HintRegister(&hint_register);
}


bool UsePosition::HintRegister(int* register_index) const {
  if (hint_ == nullptr) return false;
  switch (HintTypeField::decode(flags_)) {
    case UsePositionHintType::kNone:
    case UsePositionHintType::kUnresolved:
      return false;
    case UsePositionHintType::kUsePos: {
      auto use_pos = reinterpret_cast<UsePosition*>(hint_);
      int assigned_register = AssignedRegisterField::decode(use_pos->flags_);
      if (assigned_register == kUnassignedRegister) return false;
      *register_index = assigned_register;
      return true;
    }
    case UsePositionHintType::kOperand: {
      auto operand = reinterpret_cast<InstructionOperand*>(hint_);
      int assigned_register = AllocatedOperand::cast(operand)->index();
      *register_index = assigned_register;
      return true;
    }
    case UsePositionHintType::kPhi: {
      auto phi = reinterpret_cast<RegisterAllocationData::PhiMapValue*>(hint_);
      int assigned_register = phi->assigned_register();
      if (assigned_register == kUnassignedRegister) return false;
      *register_index = assigned_register;
      return true;
    }
  }
  UNREACHABLE();
  return false;
}


UsePositionHintType UsePosition::HintTypeForOperand(
    const InstructionOperand& op) {
  switch (op.kind()) {
    case InstructionOperand::CONSTANT:
    case InstructionOperand::IMMEDIATE:
      return UsePositionHintType::kNone;
    case InstructionOperand::UNALLOCATED:
      return UsePositionHintType::kUnresolved;
    case InstructionOperand::ALLOCATED:
      switch (AllocatedOperand::cast(op).allocated_kind()) {
        case AllocatedOperand::REGISTER:
        case AllocatedOperand::DOUBLE_REGISTER:
          return UsePositionHintType::kOperand;
        case AllocatedOperand::STACK_SLOT:
        case AllocatedOperand::DOUBLE_STACK_SLOT:
          return UsePositionHintType::kNone;
      }
    case InstructionOperand::INVALID:
      break;
  }
  UNREACHABLE();
  return UsePositionHintType::kNone;
}


void UsePosition::ResolveHint(UsePosition* use_pos) {
  DCHECK_NOT_NULL(use_pos);
  if (HintTypeField::decode(flags_) != UsePositionHintType::kUnresolved) return;
  hint_ = use_pos;
  flags_ = HintTypeField::update(flags_, UsePositionHintType::kUsePos);
}


void UsePosition::set_type(UsePositionType type, bool register_beneficial) {
  DCHECK_IMPLIES(type == UsePositionType::kRequiresSlot, !register_beneficial);
  DCHECK_EQ(kUnassignedRegister, AssignedRegisterField::decode(flags_));
  flags_ = TypeField::encode(type) |
           RegisterBeneficialField::encode(register_beneficial) |
           HintTypeField::encode(HintTypeField::decode(flags_)) |
           AssignedRegisterField::encode(kUnassignedRegister);
}


UseInterval* UseInterval::SplitAt(LifetimePosition pos, Zone* zone) {
  DCHECK(Contains(pos) && pos != start());
  auto after = new (zone) UseInterval(pos, end_);
  after->next_ = next_;
  next_ = nullptr;
  end_ = pos;
  return after;
}


std::ostream& operator<<(std::ostream& os, const LifetimePosition pos) {
  os << '@' << pos.ToInstructionIndex();
  if (pos.IsGapPosition()) {
    os << 'g';
  } else {
    os << 'i';
  }
  if (pos.IsStart()) {
    os << 's';
  } else {
    os << 'e';
  }
  return os;
}


const float LiveRange::kInvalidWeight = -1;
const float LiveRange::kMaxWeight = std::numeric_limits<float>::max();


LiveRange::LiveRange(int relative_id, MachineType machine_type,
                     TopLevelLiveRange* top_level)
    : relative_id_(relative_id),
      bits_(0),
      last_interval_(nullptr),
      first_interval_(nullptr),
      first_pos_(nullptr),
      top_level_(top_level),
      next_(nullptr),
      current_interval_(nullptr),
      last_processed_use_(nullptr),
      current_hint_position_(nullptr),
      size_(kInvalidSize),
      weight_(kInvalidWeight),
      group_(nullptr) {
  DCHECK(AllocatedOperand::IsSupportedMachineType(machine_type));
  bits_ = AssignedRegisterField::encode(kUnassignedRegister) |
          MachineTypeField::encode(machine_type);
}


void LiveRange::Verify() const {
  // Walk the positions, verifying that each is in an interval.
  auto interval = first_interval_;
  for (auto pos = first_pos_; pos != nullptr; pos = pos->next()) {
    CHECK(Start() <= pos->pos());
    CHECK(pos->pos() <= End());
    CHECK(interval != nullptr);
    while (!interval->Contains(pos->pos()) && interval->end() != pos->pos()) {
      interval = interval->next();
      CHECK(interval != nullptr);
    }
  }
}


void LiveRange::set_assigned_register(int reg) {
  DCHECK(!HasRegisterAssigned() && !spilled());
  bits_ = AssignedRegisterField::update(bits_, reg);
}


void LiveRange::UnsetAssignedRegister() {
  DCHECK(HasRegisterAssigned() && !spilled());
  bits_ = AssignedRegisterField::update(bits_, kUnassignedRegister);
}


void LiveRange::Spill() {
  DCHECK(!spilled());
  DCHECK(!TopLevel()->HasNoSpillType());
  set_spilled(true);
  bits_ = AssignedRegisterField::update(bits_, kUnassignedRegister);
}


RegisterKind LiveRange::kind() const {
  switch (RepresentationOf(machine_type())) {
    case kRepFloat32:
    case kRepFloat64:
      return DOUBLE_REGISTERS;
    default:
      break;
  }
  return GENERAL_REGISTERS;
}


UsePosition* LiveRange::FirstHintPosition(int* register_index) const {
  for (auto pos = first_pos_; pos != nullptr; pos = pos->next()) {
    if (pos->HintRegister(register_index)) return pos;
  }
  return nullptr;
}


UsePosition* LiveRange::NextUsePosition(LifetimePosition start) const {
  UsePosition* use_pos = last_processed_use_;
  if (use_pos == nullptr || use_pos->pos() > start) {
    use_pos = first_pos();
  }
  while (use_pos != nullptr && use_pos->pos() < start) {
    use_pos = use_pos->next();
  }
  last_processed_use_ = use_pos;
  return use_pos;
}


UsePosition* LiveRange::NextUsePositionRegisterIsBeneficial(
    LifetimePosition start) const {
  UsePosition* pos = NextUsePosition(start);
  while (pos != nullptr && !pos->RegisterIsBeneficial()) {
    pos = pos->next();
  }
  return pos;
}


UsePosition* LiveRange::PreviousUsePositionRegisterIsBeneficial(
    LifetimePosition start) const {
  auto pos = first_pos();
  UsePosition* prev = nullptr;
  while (pos != nullptr && pos->pos() < start) {
    if (pos->RegisterIsBeneficial()) prev = pos;
    pos = pos->next();
  }
  return prev;
}


UsePosition* LiveRange::NextRegisterPosition(LifetimePosition start) const {
  UsePosition* pos = NextUsePosition(start);
  while (pos != nullptr && pos->type() != UsePositionType::kRequiresRegister) {
    pos = pos->next();
  }
  return pos;
}


UsePosition* LiveRange::NextSlotPosition(LifetimePosition start) const {
  for (UsePosition* pos = NextUsePosition(start); pos != nullptr;
       pos = pos->next()) {
    if (pos->type() != UsePositionType::kRequiresSlot) continue;
    return pos;
  }
  return nullptr;
}


bool LiveRange::CanBeSpilled(LifetimePosition pos) const {
  // We cannot spill a live range that has a use requiring a register
  // at the current or the immediate next position.
  auto use_pos = NextRegisterPosition(pos);
  if (use_pos == nullptr) return true;
  return use_pos->pos() > pos.NextStart().End();
}


bool LiveRange::IsTopLevel() const { return top_level_ == this; }


InstructionOperand LiveRange::GetAssignedOperand() const {
  if (HasRegisterAssigned()) {
    DCHECK(!spilled());
    switch (kind()) {
      case GENERAL_REGISTERS:
        return RegisterOperand(machine_type(), assigned_register());
      case DOUBLE_REGISTERS:
        return DoubleRegisterOperand(machine_type(), assigned_register());
    }
  }
  DCHECK(spilled());
  DCHECK(!HasRegisterAssigned());
  if (TopLevel()->HasSpillOperand()) {
    auto op = TopLevel()->GetSpillOperand();
    DCHECK(!op->IsUnallocated());
    return *op;
  }
  return TopLevel()->GetSpillRangeOperand();
}


UseInterval* LiveRange::FirstSearchIntervalForPosition(
    LifetimePosition position) const {
  if (current_interval_ == nullptr) return first_interval_;
  if (current_interval_->start() > position) {
    current_interval_ = nullptr;
    return first_interval_;
  }
  return current_interval_;
}


void LiveRange::AdvanceLastProcessedMarker(
    UseInterval* to_start_of, LifetimePosition but_not_past) const {
  if (to_start_of == nullptr) return;
  if (to_start_of->start() > but_not_past) return;
  auto start = current_interval_ == nullptr ? LifetimePosition::Invalid()
                                            : current_interval_->start();
  if (to_start_of->start() > start) {
    current_interval_ = to_start_of;
  }
}


LiveRange* LiveRange::SplitAt(LifetimePosition position, Zone* zone) {
  int new_id = TopLevel()->GetNextChildId();
  LiveRange* child = new (zone) LiveRange(new_id, machine_type(), TopLevel());
  DetachAt(position, child, zone);

  child->top_level_ = TopLevel();
  child->next_ = next_;
  next_ = child;
  if (child->next() == nullptr) {
    TopLevel()->set_last_child(child);
  }
  return child;
}


void LiveRange::DetachAt(LifetimePosition position, LiveRange* result,
                         Zone* zone) {
  DCHECK(Start() < position);
  DCHECK(End() > position);
  DCHECK(result->IsEmpty());
  // Find the last interval that ends before the position. If the
  // position is contained in one of the intervals in the chain, we
  // split that interval and use the first part.
  auto current = FirstSearchIntervalForPosition(position);

  // If the split position coincides with the beginning of a use interval
  // we need to split use positons in a special way.
  bool split_at_start = false;

  if (current->start() == position) {
    // When splitting at start we need to locate the previous use interval.
    current = first_interval_;
  }

  UseInterval* after = nullptr;
  while (current != nullptr) {
    if (current->Contains(position)) {
      after = current->SplitAt(position, zone);
      break;
    }
    auto next = current->next();
    if (next->start() >= position) {
      split_at_start = (next->start() == position);
      after = next;
      current->set_next(nullptr);
      break;
    }
    current = next;
  }
  DCHECK(nullptr != after);

  // Partition original use intervals to the two live ranges.
  auto before = current;
  result->last_interval_ =
      (last_interval_ == before)
          ? after            // Only interval in the range after split.
          : last_interval_;  // Last interval of the original range.
  result->first_interval_ = after;
  last_interval_ = before;

  // Find the last use position before the split and the first use
  // position after it.
  auto use_after = first_pos_;
  UsePosition* use_before = nullptr;
  if (split_at_start) {
    // The split position coincides with the beginning of a use interval (the
    // end of a lifetime hole). Use at this position should be attributed to
    // the split child because split child owns use interval covering it.
    while (use_after != nullptr && use_after->pos() < position) {
      use_before = use_after;
      use_after = use_after->next();
    }
  } else {
    while (use_after != nullptr && use_after->pos() <= position) {
      use_before = use_after;
      use_after = use_after->next();
    }
  }

  // Partition original use positions to the two live ranges.
  if (use_before != nullptr) {
    use_before->set_next(nullptr);
  } else {
    first_pos_ = nullptr;
  }
  result->first_pos_ = use_after;

  // Discard cached iteration state. It might be pointing
  // to the use that no longer belongs to this live range.
  last_processed_use_ = nullptr;
  current_interval_ = nullptr;

  // Invalidate size and weight of this range. The child range has them
  // invalid at construction.
  size_ = kInvalidSize;
  weight_ = kInvalidWeight;
#ifdef DEBUG
  Verify();
  result->Verify();
#endif
}


void LiveRange::AppendAsChild(TopLevelLiveRange* other) {
  next_ = other;

  other->UpdateParentForAllChildren(TopLevel());
  TopLevel()->UpdateSpillRangePostMerge(other);
  TopLevel()->set_last_child(other->last_child());
}


void LiveRange::UpdateParentForAllChildren(TopLevelLiveRange* new_top_level) {
  LiveRange* child = this;
  for (; child != nullptr; child = child->next()) {
    child->top_level_ = new_top_level;
  }
}


void LiveRange::ConvertUsesToOperand(const InstructionOperand& op,
                                     const InstructionOperand& spill_op) {
  for (auto pos = first_pos(); pos != nullptr; pos = pos->next()) {
    DCHECK(Start() <= pos->pos() && pos->pos() <= End());
    if (!pos->HasOperand()) continue;
    switch (pos->type()) {
      case UsePositionType::kRequiresSlot:
        DCHECK(spill_op.IsStackSlot() || spill_op.IsDoubleStackSlot());
        InstructionOperand::ReplaceWith(pos->operand(), &spill_op);
        break;
      case UsePositionType::kRequiresRegister:
        DCHECK(op.IsRegister() || op.IsDoubleRegister());
      // Fall through.
      case UsePositionType::kAny:
        InstructionOperand::ReplaceWith(pos->operand(), &op);
        break;
    }
  }
}


// This implements an ordering on live ranges so that they are ordered by their
// start positions.  This is needed for the correctness of the register
// allocation algorithm.  If two live ranges start at the same offset then there
// is a tie breaker based on where the value is first used.  This part of the
// ordering is merely a heuristic.
bool LiveRange::ShouldBeAllocatedBefore(const LiveRange* other) const {
  LifetimePosition start = Start();
  LifetimePosition other_start = other->Start();
  if (start == other_start) {
    UsePosition* pos = first_pos();
    if (pos == nullptr) return false;
    UsePosition* other_pos = other->first_pos();
    if (other_pos == nullptr) return true;
    return pos->pos() < other_pos->pos();
  }
  return start < other_start;
}


void LiveRange::SetUseHints(int register_index) {
  for (auto pos = first_pos(); pos != nullptr; pos = pos->next()) {
    if (!pos->HasOperand()) continue;
    switch (pos->type()) {
      case UsePositionType::kRequiresSlot:
        break;
      case UsePositionType::kRequiresRegister:
      case UsePositionType::kAny:
        pos->set_assigned_register(register_index);
        break;
    }
  }
}


bool LiveRange::CanCover(LifetimePosition position) const {
  if (IsEmpty()) return false;
  return Start() <= position && position < End();
}


bool LiveRange::Covers(LifetimePosition position) const {
  if (!CanCover(position)) return false;
  auto start_search = FirstSearchIntervalForPosition(position);
  for (auto interval = start_search; interval != nullptr;
       interval = interval->next()) {
    DCHECK(interval->next() == nullptr ||
           interval->next()->start() >= interval->start());
    AdvanceLastProcessedMarker(interval, position);
    if (interval->Contains(position)) return true;
    if (interval->start() > position) return false;
  }
  return false;
}


LifetimePosition LiveRange::FirstIntersection(LiveRange* other) const {
  auto b = other->first_interval();
  if (b == nullptr) return LifetimePosition::Invalid();
  auto advance_last_processed_up_to = b->start();
  auto a = FirstSearchIntervalForPosition(b->start());
  while (a != nullptr && b != nullptr) {
    if (a->start() > other->End()) break;
    if (b->start() > End()) break;
    auto cur_intersection = a->Intersect(b);
    if (cur_intersection.IsValid()) {
      return cur_intersection;
    }
    if (a->start() < b->start()) {
      a = a->next();
      if (a == nullptr || a->start() > other->End()) break;
      AdvanceLastProcessedMarker(a, advance_last_processed_up_to);
    } else {
      b = b->next();
    }
  }
  return LifetimePosition::Invalid();
}


unsigned LiveRange::GetSize() {
  if (size_ == kInvalidSize) {
    size_ = 0;
    for (auto interval = first_interval(); interval != nullptr;
         interval = interval->next()) {
      size_ += (interval->end().value() - interval->start().value());
    }
  }

  return static_cast<unsigned>(size_);
}


struct TopLevelLiveRange::SpillAtDefinitionList : ZoneObject {
  SpillAtDefinitionList(int gap_index, InstructionOperand* operand,
                        SpillAtDefinitionList* next)
      : gap_index(gap_index), operand(operand), next(next) {}
  const int gap_index;
  InstructionOperand* const operand;
  SpillAtDefinitionList* const next;
};


TopLevelLiveRange::TopLevelLiveRange(int vreg, MachineType machine_type)
    : LiveRange(0, machine_type, this),
      vreg_(vreg),
      last_child_id_(0),
      splintered_from_(nullptr),
      spill_operand_(nullptr),
      spills_at_definition_(nullptr),
      spilled_in_deferred_blocks_(false),
      spill_start_index_(kMaxInt),
      last_child_(this),
      last_insertion_point_(this) {
  bits_ |= SpillTypeField::encode(SpillType::kNoSpillType);
}


void TopLevelLiveRange::SpillAtDefinition(Zone* zone, int gap_index,
                                          InstructionOperand* operand) {
  DCHECK(HasNoSpillType());
  spills_at_definition_ = new (zone)
      SpillAtDefinitionList(gap_index, operand, spills_at_definition_);
}


bool TopLevelLiveRange::TryCommitSpillInDeferredBlock(
    InstructionSequence* code, const InstructionOperand& spill_operand) {
  if (!FLAG_turbo_preprocess_ranges || IsEmpty() || HasNoSpillType() ||
      spill_operand.IsConstant() || spill_operand.IsImmediate()) {
    return false;
  }

  int count = 0;
  for (const LiveRange* child = this; child != nullptr; child = child->next()) {
    int first_instr = child->Start().ToInstructionIndex();

    // If the range starts at instruction end, the first instruction index is
    // the next one.
    if (!child->Start().IsGapPosition() && !child->Start().IsStart()) {
      ++first_instr;
    }

    // We only look at where the range starts. It doesn't matter where it ends:
    // if it ends past this block, then either there is a phi there already,
    // or ResolveControlFlow will adapt the last instruction gap of this block
    // as if there were a phi. In either case, data flow will be correct.
    const InstructionBlock* block = code->GetInstructionBlock(first_instr);

    // If we have slot uses in a subrange, bail out, because we need the value
    // on the stack before that use.
    bool has_slot_use = child->NextSlotPosition(child->Start()) != nullptr;
    if (!block->IsDeferred()) {
      if (child->spilled() || has_slot_use) {
        TRACE(
            "Live Range %d must be spilled at definition: found a "
            "slot-requiring non-deferred child range %d.\n",
            TopLevel()->vreg(), child->relative_id());
        return false;
      }
    } else {
      if (child->spilled() || has_slot_use) ++count;
    }
  }
  if (count == 0) return false;

  spill_start_index_ = -1;
  spilled_in_deferred_blocks_ = true;

  TRACE("Live Range %d will be spilled only in deferred blocks.\n", vreg());
  // If we have ranges that aren't spilled but require the operand on the stack,
  // make sure we insert the spill.
  for (const LiveRange* child = this; child != nullptr; child = child->next()) {
    if (!child->spilled() &&
        child->NextSlotPosition(child->Start()) != nullptr) {
      auto instr = code->InstructionAt(child->Start().ToInstructionIndex());
      // Insert spill at the end to let live range connections happen at START.
      auto move =
          instr->GetOrCreateParallelMove(Instruction::END, code->zone());
      InstructionOperand assigned = child->GetAssignedOperand();
      if (TopLevel()->has_slot_use()) {
        bool found = false;
        for (auto move_op : *move) {
          if (move_op->IsEliminated()) continue;
          if (move_op->source().Equals(assigned) &&
              move_op->destination().Equals(spill_operand)) {
            found = true;
            break;
          }
        }
        if (found) continue;
      }

      move->AddMove(assigned, spill_operand);
    }
  }

  return true;
}


void TopLevelLiveRange::CommitSpillsAtDefinition(InstructionSequence* sequence,
                                                 const InstructionOperand& op,
                                                 bool might_be_duplicated) {
  DCHECK_IMPLIES(op.IsConstant(), spills_at_definition_ == nullptr);
  auto zone = sequence->zone();

  for (auto to_spill = spills_at_definition_; to_spill != nullptr;
       to_spill = to_spill->next) {
    auto instr = sequence->InstructionAt(to_spill->gap_index);
    auto move = instr->GetOrCreateParallelMove(Instruction::START, zone);
    // Skip insertion if it's possible that the move exists already as a
    // constraint move from a fixed output register to a slot.
    if (might_be_duplicated) {
      bool found = false;
      for (auto move_op : *move) {
        if (move_op->IsEliminated()) continue;
        if (move_op->source().Equals(*to_spill->operand) &&
            move_op->destination().Equals(op)) {
          found = true;
          break;
        }
      }
      if (found) continue;
    }
    move->AddMove(*to_spill->operand, op);
  }
}


void TopLevelLiveRange::SetSpillOperand(InstructionOperand* operand) {
  DCHECK(HasNoSpillType());
  DCHECK(!operand->IsUnallocated() && !operand->IsImmediate());
  set_spill_type(SpillType::kSpillOperand);
  spill_operand_ = operand;
}


void TopLevelLiveRange::SetSpillRange(SpillRange* spill_range) {
  DCHECK(!HasSpillOperand());
  DCHECK(spill_range);
  spill_range_ = spill_range;
}


AllocatedOperand TopLevelLiveRange::GetSpillRangeOperand() const {
  auto spill_range = GetSpillRange();
  int index = spill_range->assigned_slot();
  switch (kind()) {
    case GENERAL_REGISTERS:
      return StackSlotOperand(machine_type(), index);
    case DOUBLE_REGISTERS:
      return DoubleStackSlotOperand(machine_type(), index);
  }
  UNREACHABLE();
  return StackSlotOperand(kMachNone, 0);
}


void TopLevelLiveRange::Splinter(LifetimePosition start, LifetimePosition end,
                                 TopLevelLiveRange* result, Zone* zone) {
  DCHECK(start != Start() || end != End());
  DCHECK(start < end);

  result->set_spill_type(spill_type());

  if (start <= Start()) {
    // TODO(mtrofin): here, the TopLevel part is in the deferred range, so we
    // may want to continue processing the splinter. However, if the value is
    // defined in a cold block, and then used in a hot block, it follows that
    // it should terminate on the RHS of a phi, defined on the hot path. We
    // should check this, however, this may not be the place, because we don't
    // have access to the instruction sequence.
    DCHECK(end < End());
    DetachAt(end, result, zone);
    next_ = nullptr;
  } else if (end >= End()) {
    DCHECK(start > Start());
    DetachAt(start, result, zone);
    next_ = nullptr;
  } else {
    DCHECK(start < End() && Start() < end);

    const int kInvalidId = std::numeric_limits<int>::max();

    DetachAt(start, result, zone);

    LiveRange end_part(kInvalidId, this->machine_type(), nullptr);
    result->DetachAt(end, &end_part, zone);

    next_ = end_part.next_;
    last_interval_->set_next(end_part.first_interval_);
    // The next splinter will happen either at or after the current interval.
    // We can optimize DetachAt by setting current_interval_ accordingly,
    // which will then be picked up by FirstSearchIntervalForPosition.
    current_interval_ = last_interval_;
    last_interval_ = end_part.last_interval_;


    if (first_pos_ == nullptr) {
      first_pos_ = end_part.first_pos_;
    } else {
      UsePosition* pos = first_pos_;
      for (; pos->next() != nullptr; pos = pos->next()) {
      }
      pos->set_next(end_part.first_pos_);
    }
  }
  result->next_ = nullptr;
  result->top_level_ = result;

  result->SetSplinteredFrom(this);
  // Ensure the result's relative ID is unique within the IDs used for this
  // virtual register's children and splinters.
  result->relative_id_ = GetNextChildId();
}


void TopLevelLiveRange::SetSplinteredFrom(TopLevelLiveRange* splinter_parent) {
  // The splinter parent is always the original "Top".
  DCHECK(splinter_parent->Start() < Start());

  splintered_from_ = splinter_parent;
  if (!HasSpillOperand() && splinter_parent->spill_range_ != nullptr) {
    SetSpillRange(splinter_parent->spill_range_);
  }
}


void TopLevelLiveRange::UpdateSpillRangePostMerge(TopLevelLiveRange* merged) {
  DCHECK(merged->TopLevel() == this);

  if (HasNoSpillType() && merged->HasSpillRange()) {
    set_spill_type(merged->spill_type());
    DCHECK(GetSpillRange()->live_ranges().size() > 0);
    merged->spill_range_ = nullptr;
    merged->bits_ =
        SpillTypeField::update(merged->bits_, SpillType::kNoSpillType);
  }
}


void TopLevelLiveRange::Merge(TopLevelLiveRange* other, Zone* zone) {
  DCHECK(Start() < other->Start());
  DCHECK(other->splintered_from() == this);

  LiveRange* last_other = other->last_child();
  LiveRange* last_me = last_child();

  // Simple case: we just append at the end.
  if (last_me->End() <= other->Start()) return last_me->AppendAsChild(other);

  DCHECK(last_me->End() > last_other->End());

  // In the more general case, we need to find the ranges between which to
  // insert.
  if (other->Start() < last_insertion_point_->Start()) {
    last_insertion_point_ = this;
  }

  for (; last_insertion_point_->next() != nullptr &&
         last_insertion_point_->next()->Start() <= other->Start();
       last_insertion_point_ = last_insertion_point_->next()) {
  }

  // When we splintered the original range, we reconstituted the original range
  // into one range without children, but with discontinuities. To merge the
  // splinter back in, we need to split the range - or a child obtained after
  // register allocation splitting.
  LiveRange* after = last_insertion_point_->next();
  if (last_insertion_point_->End() > other->Start()) {
    LiveRange* new_after = last_insertion_point_->SplitAt(other->Start(), zone);
    new_after->set_spilled(last_insertion_point_->spilled());
    if (!new_after->spilled())
      new_after->set_assigned_register(
          last_insertion_point_->assigned_register());
    after = new_after;
  }

  last_other->next_ = after;
  last_insertion_point_->next_ = other;
  other->UpdateParentForAllChildren(TopLevel());
  TopLevel()->UpdateSpillRangePostMerge(other);
}


void TopLevelLiveRange::ShortenTo(LifetimePosition start) {
  TRACE("Shorten live range %d to [%d\n", vreg(), start.value());
  DCHECK(first_interval_ != nullptr);
  DCHECK(first_interval_->start() <= start);
  DCHECK(start < first_interval_->end());
  first_interval_->set_start(start);
}


void TopLevelLiveRange::EnsureInterval(LifetimePosition start,
                                       LifetimePosition end, Zone* zone) {
  TRACE("Ensure live range %d in interval [%d %d[\n", vreg(), start.value(),
        end.value());
  auto new_end = end;
  while (first_interval_ != nullptr && first_interval_->start() <= end) {
    if (first_interval_->end() > end) {
      new_end = first_interval_->end();
    }
    first_interval_ = first_interval_->next();
  }

  auto new_interval = new (zone) UseInterval(start, new_end);
  new_interval->set_next(first_interval_);
  first_interval_ = new_interval;
  if (new_interval->next() == nullptr) {
    last_interval_ = new_interval;
  }
}


void TopLevelLiveRange::AddUseInterval(LifetimePosition start,
                                       LifetimePosition end, Zone* zone) {
  TRACE("Add to live range %d interval [%d %d[\n", vreg(), start.value(),
        end.value());
  if (first_interval_ == nullptr) {
    auto interval = new (zone) UseInterval(start, end);
    first_interval_ = interval;
    last_interval_ = interval;
  } else {
    if (end == first_interval_->start()) {
      first_interval_->set_start(start);
    } else if (end < first_interval_->start()) {
      auto interval = new (zone) UseInterval(start, end);
      interval->set_next(first_interval_);
      first_interval_ = interval;
    } else {
      // Order of instruction's processing (see ProcessInstructions) guarantees
      // that each new use interval either precedes or intersects with
      // last added interval.
      DCHECK(start < first_interval_->end());
      first_interval_->set_start(Min(start, first_interval_->start()));
      first_interval_->set_end(Max(end, first_interval_->end()));
    }
  }
}


void TopLevelLiveRange::AddUsePosition(UsePosition* use_pos) {
  auto pos = use_pos->pos();
  TRACE("Add to live range %d use position %d\n", vreg(), pos.value());
  UsePosition* prev_hint = nullptr;
  UsePosition* prev = nullptr;
  auto current = first_pos_;
  while (current != nullptr && current->pos() < pos) {
    prev_hint = current->HasHint() ? current : prev_hint;
    prev = current;
    current = current->next();
  }

  if (prev == nullptr) {
    use_pos->set_next(first_pos_);
    first_pos_ = use_pos;
  } else {
    use_pos->set_next(prev->next());
    prev->set_next(use_pos);
  }

  if (prev_hint == nullptr && use_pos->HasHint()) {
    current_hint_position_ = use_pos;
  }
}


static bool AreUseIntervalsIntersecting(UseInterval* interval1,
                                        UseInterval* interval2) {
  while (interval1 != nullptr && interval2 != nullptr) {
    if (interval1->start() < interval2->start()) {
      if (interval1->end() > interval2->start()) {
        return true;
      }
      interval1 = interval1->next();
    } else {
      if (interval2->end() > interval1->start()) {
        return true;
      }
      interval2 = interval2->next();
    }
  }
  return false;
}


std::ostream& operator<<(std::ostream& os,
                         const PrintableLiveRange& printable_range) {
  const LiveRange* range = printable_range.range_;
  os << "Range: " << range->TopLevel()->vreg() << ":" << range->relative_id()
     << " ";
  if (range->TopLevel()->is_phi()) os << "phi ";
  if (range->TopLevel()->is_non_loop_phi()) os << "nlphi ";

  os << "{" << std::endl;
  auto interval = range->first_interval();
  auto use_pos = range->first_pos();
  PrintableInstructionOperand pio;
  pio.register_configuration_ = printable_range.register_configuration_;
  while (use_pos != nullptr) {
    if (use_pos->HasOperand()) {
      pio.op_ = *use_pos->operand();
      os << pio << use_pos->pos() << " ";
    }
    use_pos = use_pos->next();
  }
  os << std::endl;

  while (interval != nullptr) {
    os << '[' << interval->start() << ", " << interval->end() << ')'
       << std::endl;
    interval = interval->next();
  }
  os << "}";
  return os;
}


SpillRange::SpillRange(TopLevelLiveRange* parent, Zone* zone)
    : live_ranges_(zone),
      assigned_slot_(kUnassignedSlot),
      byte_width_(GetByteWidth(parent->machine_type())),
      kind_(parent->kind()) {
  // Spill ranges are created for top level, non-splintered ranges. This is so
  // that, when merging decisions are made, we consider the full extent of the
  // virtual register, and avoid clobbering it.
  DCHECK(!parent->IsSplinter());
  UseInterval* result = nullptr;
  UseInterval* node = nullptr;
  // Copy the intervals for all ranges.
  for (LiveRange* range = parent; range != nullptr; range = range->next()) {
    auto src = range->first_interval();
    while (src != nullptr) {
      auto new_node = new (zone) UseInterval(src->start(), src->end());
      if (result == nullptr) {
        result = new_node;
      } else {
        node->set_next(new_node);
      }
      node = new_node;
      src = src->next();
    }
  }
  use_interval_ = result;
  live_ranges().push_back(parent);
  end_position_ = node->end();
  parent->SetSpillRange(this);
}


int SpillRange::ByteWidth() const {
  return GetByteWidth(live_ranges_[0]->machine_type());
}


bool SpillRange::IsIntersectingWith(SpillRange* other) const {
  if (this->use_interval_ == nullptr || other->use_interval_ == nullptr ||
      this->End() <= other->use_interval_->start() ||
      other->End() <= this->use_interval_->start()) {
    return false;
  }
  return AreUseIntervalsIntersecting(use_interval_, other->use_interval_);
}


bool SpillRange::TryMerge(SpillRange* other) {
  // TODO(dcarney): byte widths should be compared here not kinds.
  if (live_ranges_[0]->kind() != other->live_ranges_[0]->kind() ||
      IsIntersectingWith(other)) {
    return false;
  }

  auto max = LifetimePosition::MaxPosition();
  if (End() < other->End() && other->End() != max) {
    end_position_ = other->End();
  }
  other->end_position_ = max;

  MergeDisjointIntervals(other->use_interval_);
  other->use_interval_ = nullptr;

  for (auto range : other->live_ranges()) {
    DCHECK(range->GetSpillRange() == other);
    range->SetSpillRange(this);
  }

  live_ranges().insert(live_ranges().end(), other->live_ranges().begin(),
                       other->live_ranges().end());
  other->live_ranges().clear();

  return true;
}


void SpillRange::MergeDisjointIntervals(UseInterval* other) {
  UseInterval* tail = nullptr;
  auto current = use_interval_;
  while (other != nullptr) {
    // Make sure the 'current' list starts first
    if (current == nullptr || current->start() > other->start()) {
      std::swap(current, other);
    }
    // Check disjointness
    DCHECK(other == nullptr || current->end() <= other->start());
    // Append the 'current' node to the result accumulator and move forward
    if (tail == nullptr) {
      use_interval_ = current;
    } else {
      tail->set_next(current);
    }
    tail = current;
    current = current->next();
  }
  // Other list is empty => we are done
}


RegisterAllocationData::PhiMapValue::PhiMapValue(PhiInstruction* phi,
                                                 const InstructionBlock* block,
                                                 Zone* zone)
    : phi_(phi),
      block_(block),
      incoming_operands_(zone),
      assigned_register_(kUnassignedRegister) {
  incoming_operands_.reserve(phi->operands().size());
}


void RegisterAllocationData::PhiMapValue::AddOperand(
    InstructionOperand* operand) {
  incoming_operands_.push_back(operand);
}


void RegisterAllocationData::PhiMapValue::CommitAssignment(
    const InstructionOperand& assigned) {
  for (auto operand : incoming_operands_) {
    InstructionOperand::ReplaceWith(operand, &assigned);
  }
}


RegisterAllocationData::RegisterAllocationData(
    const RegisterConfiguration* config, Zone* zone, Frame* frame,
    InstructionSequence* code, const char* debug_name)
    : allocation_zone_(zone),
      frame_(frame),
      code_(code),
      debug_name_(debug_name),
      config_(config),
      phi_map_(allocation_zone()),
      live_in_sets_(code->InstructionBlockCount(), nullptr, allocation_zone()),
      live_out_sets_(code->InstructionBlockCount(), nullptr, allocation_zone()),
      live_ranges_(code->VirtualRegisterCount() * 2, nullptr,
                   allocation_zone()),
      fixed_live_ranges_(this->config()->num_general_registers(), nullptr,
                         allocation_zone()),
      fixed_double_live_ranges_(this->config()->num_double_registers(), nullptr,
                                allocation_zone()),
      spill_ranges_(code->VirtualRegisterCount(), nullptr, allocation_zone()),
      delayed_references_(allocation_zone()),
      assigned_registers_(nullptr),
      assigned_double_registers_(nullptr),
      virtual_register_count_(code->VirtualRegisterCount()) {
  DCHECK(this->config()->num_general_registers() <=
         RegisterConfiguration::kMaxGeneralRegisters);
  DCHECK(this->config()->num_double_registers() <=
         RegisterConfiguration::kMaxDoubleRegisters);
  assigned_registers_ = new (code_zone())
      BitVector(this->config()->num_general_registers(), code_zone());
  assigned_double_registers_ = new (code_zone())
      BitVector(this->config()->num_aliased_double_registers(), code_zone());
  this->frame()->SetAllocatedRegisters(assigned_registers_);
  this->frame()->SetAllocatedDoubleRegisters(assigned_double_registers_);
}


MoveOperands* RegisterAllocationData::AddGapMove(
    int index, Instruction::GapPosition position,
    const InstructionOperand& from, const InstructionOperand& to) {
  auto instr = code()->InstructionAt(index);
  auto moves = instr->GetOrCreateParallelMove(position, code_zone());
  return moves->AddMove(from, to);
}


MachineType RegisterAllocationData::MachineTypeFor(int virtual_register) {
  DCHECK_LT(virtual_register, code()->VirtualRegisterCount());
  return code()->GetRepresentation(virtual_register);
}


TopLevelLiveRange* RegisterAllocationData::GetOrCreateLiveRangeFor(int index) {
  if (index >= static_cast<int>(live_ranges().size())) {
    live_ranges().resize(index + 1, nullptr);
  }
  auto result = live_ranges()[index];
  if (result == nullptr) {
    result = NewLiveRange(index, MachineTypeFor(index));
    live_ranges()[index] = result;
  }
  return result;
}


TopLevelLiveRange* RegisterAllocationData::NewLiveRange(
    int index, MachineType machine_type) {
  return new (allocation_zone()) TopLevelLiveRange(index, machine_type);
}


int RegisterAllocationData::GetNextLiveRangeId() {
  int vreg = virtual_register_count_++;
  if (vreg >= static_cast<int>(live_ranges().size())) {
    live_ranges().resize(vreg + 1, nullptr);
  }
  return vreg;
}


TopLevelLiveRange* RegisterAllocationData::NextLiveRange(
    MachineType machine_type) {
  int vreg = GetNextLiveRangeId();
  TopLevelLiveRange* ret = NewLiveRange(vreg, machine_type);
  return ret;
}


RegisterAllocationData::PhiMapValue* RegisterAllocationData::InitializePhiMap(
    const InstructionBlock* block, PhiInstruction* phi) {
  auto map_value = new (allocation_zone())
      RegisterAllocationData::PhiMapValue(phi, block, allocation_zone());
  auto res =
      phi_map_.insert(std::make_pair(phi->virtual_register(), map_value));
  DCHECK(res.second);
  USE(res);
  return map_value;
}


RegisterAllocationData::PhiMapValue* RegisterAllocationData::GetPhiMapValueFor(
    int virtual_register) {
  auto it = phi_map_.find(virtual_register);
  DCHECK(it != phi_map_.end());
  return it->second;
}


RegisterAllocationData::PhiMapValue* RegisterAllocationData::GetPhiMapValueFor(
    TopLevelLiveRange* top_range) {
  return GetPhiMapValueFor(top_range->vreg());
}


bool RegisterAllocationData::ExistsUseWithoutDefinition() {
  bool found = false;
  BitVector::Iterator iterator(live_in_sets()[0]);
  while (!iterator.Done()) {
    found = true;
    int operand_index = iterator.Current();
    PrintF("Register allocator error: live v%d reached first block.\n",
           operand_index);
    LiveRange* range = GetOrCreateLiveRangeFor(operand_index);
    PrintF("  (first use is at %d)\n", range->first_pos()->pos().value());
    if (debug_name() == nullptr) {
      PrintF("\n");
    } else {
      PrintF("  (function: %s)\n", debug_name());
    }
    iterator.Advance();
  }
  return found;
}


SpillRange* RegisterAllocationData::AssignSpillRangeToLiveRange(
    TopLevelLiveRange* range) {
  DCHECK(!range->HasSpillOperand());

  SpillRange* spill_range = range->GetAllocatedSpillRange();
  if (spill_range == nullptr) {
    DCHECK(!range->IsSplinter());
    spill_range = new (allocation_zone()) SpillRange(range, allocation_zone());
  }
  range->set_spill_type(TopLevelLiveRange::SpillType::kSpillRange);

  int spill_range_index =
      range->IsSplinter() ? range->splintered_from()->vreg() : range->vreg();

  spill_ranges()[spill_range_index] = spill_range;

  return spill_range;
}


SpillRange* RegisterAllocationData::CreateSpillRangeForLiveRange(
    TopLevelLiveRange* range) {
  DCHECK(!range->HasSpillOperand());
  DCHECK(!range->IsSplinter());
  auto spill_range =
      new (allocation_zone()) SpillRange(range, allocation_zone());
  return spill_range;
}


void RegisterAllocationData::MarkAllocated(RegisterKind kind, int index) {
  if (kind == DOUBLE_REGISTERS) {
    assigned_double_registers_->Add(index);
  } else {
    DCHECK(kind == GENERAL_REGISTERS);
    assigned_registers_->Add(index);
  }
}


bool RegisterAllocationData::IsBlockBoundary(LifetimePosition pos) const {
  return pos.IsFullStart() &&
         code()->GetInstructionBlock(pos.ToInstructionIndex())->code_start() ==
             pos.ToInstructionIndex();
}


void RegisterAllocationData::Print(
    const InstructionSequence* instructionSequence) {
  OFStream os(stdout);
  PrintableInstructionSequence wrapper;
  wrapper.register_configuration_ = config();
  wrapper.sequence_ = instructionSequence;
  os << wrapper << std::endl;
}


void RegisterAllocationData::Print(const Instruction* instruction) {
  OFStream os(stdout);
  PrintableInstruction wrapper;
  wrapper.instr_ = instruction;
  wrapper.register_configuration_ = config();
  os << wrapper << std::endl;
}


void RegisterAllocationData::Print(const LiveRange* range, bool with_children) {
  OFStream os(stdout);
  PrintableLiveRange wrapper;
  wrapper.register_configuration_ = config();
  for (const LiveRange* i = range; i != nullptr; i = i->next()) {
    wrapper.range_ = i;
    os << wrapper << std::endl;
    if (!with_children) break;
  }
}


void RegisterAllocationData::Print(const InstructionOperand& op) {
  OFStream os(stdout);
  PrintableInstructionOperand wrapper;
  wrapper.register_configuration_ = config();
  wrapper.op_ = op;
  os << wrapper << std::endl;
}


void RegisterAllocationData::Print(const MoveOperands* move) {
  OFStream os(stdout);
  PrintableInstructionOperand wrapper;
  wrapper.register_configuration_ = config();
  wrapper.op_ = move->destination();
  os << wrapper << " = ";
  wrapper.op_ = move->source();
  os << wrapper << std::endl;
}


void RegisterAllocationData::Print(const SpillRange* spill_range) {
  OFStream os(stdout);
  os << "{" << std::endl;
  for (TopLevelLiveRange* range : spill_range->live_ranges()) {
    os << range->vreg() << " ";
  }
  os << std::endl;

  for (UseInterval* interval = spill_range->interval(); interval != nullptr;
       interval = interval->next()) {
    os << '[' << interval->start() << ", " << interval->end() << ')'
       << std::endl;
  }
  os << "}" << std::endl;
}


ConstraintBuilder::ConstraintBuilder(RegisterAllocationData* data)
    : data_(data) {}


InstructionOperand* ConstraintBuilder::AllocateFixed(
    UnallocatedOperand* operand, int pos, bool is_tagged) {
  TRACE("Allocating fixed reg for op %d\n", operand->virtual_register());
  DCHECK(operand->HasFixedPolicy());
  InstructionOperand allocated;
  MachineType machine_type = InstructionSequence::DefaultRepresentation();
  int virtual_register = operand->virtual_register();
  if (virtual_register != InstructionOperand::kInvalidVirtualRegister) {
    machine_type = data()->MachineTypeFor(virtual_register);
  }
  if (operand->HasFixedSlotPolicy()) {
    AllocatedOperand::AllocatedKind kind =
        IsFloatingPoint(machine_type) ? AllocatedOperand::DOUBLE_STACK_SLOT
                                      : AllocatedOperand::STACK_SLOT;
    allocated =
        AllocatedOperand(kind, machine_type, operand->fixed_slot_index());
  } else if (operand->HasFixedRegisterPolicy()) {
    allocated = AllocatedOperand(AllocatedOperand::REGISTER, machine_type,
                                 operand->fixed_register_index());
  } else if (operand->HasFixedDoubleRegisterPolicy()) {
    DCHECK_NE(InstructionOperand::kInvalidVirtualRegister, virtual_register);
    allocated = AllocatedOperand(AllocatedOperand::DOUBLE_REGISTER,
                                 machine_type, operand->fixed_register_index());
  } else {
    UNREACHABLE();
  }
  InstructionOperand::ReplaceWith(operand, &allocated);
  if (is_tagged) {
    TRACE("Fixed reg is tagged at %d\n", pos);
    auto instr = code()->InstructionAt(pos);
    if (instr->HasReferenceMap()) {
      instr->reference_map()->RecordReference(*AllocatedOperand::cast(operand));
    }
  }
  return operand;
}


void ConstraintBuilder::MeetRegisterConstraints() {
  for (auto block : code()->instruction_blocks()) {
    MeetRegisterConstraints(block);
  }
}


void ConstraintBuilder::MeetRegisterConstraints(const InstructionBlock* block) {
  int start = block->first_instruction_index();
  int end = block->last_instruction_index();
  DCHECK_NE(-1, start);
  for (int i = start; i <= end; ++i) {
    MeetConstraintsBefore(i);
    if (i != end) MeetConstraintsAfter(i);
  }
  // Meet register constraints for the instruction in the end.
  MeetRegisterConstraintsForLastInstructionInBlock(block);
}


void ConstraintBuilder::MeetRegisterConstraintsForLastInstructionInBlock(
    const InstructionBlock* block) {
  int end = block->last_instruction_index();
  auto last_instruction = code()->InstructionAt(end);
  for (size_t i = 0; i < last_instruction->OutputCount(); i++) {
    auto output_operand = last_instruction->OutputAt(i);
    DCHECK(!output_operand->IsConstant());
    auto output = UnallocatedOperand::cast(output_operand);
    int output_vreg = output->virtual_register();
    auto range = data()->GetOrCreateLiveRangeFor(output_vreg);
    bool assigned = false;
    if (output->HasFixedPolicy()) {
      AllocateFixed(output, -1, false);
      // This value is produced on the stack, we never need to spill it.
      if (output->IsStackSlot()) {
        DCHECK(StackSlotOperand::cast(output)->index() <
               data()->frame()->GetSpillSlotCount());
        range->SetSpillOperand(StackSlotOperand::cast(output));
        range->SetSpillStartIndex(end);
        assigned = true;
      }

      for (auto succ : block->successors()) {
        const InstructionBlock* successor = code()->InstructionBlockAt(succ);
        DCHECK(successor->PredecessorCount() == 1);
        int gap_index = successor->first_instruction_index();
        // Create an unconstrained operand for the same virtual register
        // and insert a gap move from the fixed output to the operand.
        UnallocatedOperand output_copy(UnallocatedOperand::ANY, output_vreg);
        data()->AddGapMove(gap_index, Instruction::START, *output, output_copy);
      }
    }

    if (!assigned) {
      for (auto succ : block->successors()) {
        const InstructionBlock* successor = code()->InstructionBlockAt(succ);
        DCHECK(successor->PredecessorCount() == 1);
        int gap_index = successor->first_instruction_index();
        range->SpillAtDefinition(allocation_zone(), gap_index, output);
        range->SetSpillStartIndex(gap_index);
      }
    }
  }
}


void ConstraintBuilder::MeetConstraintsAfter(int instr_index) {
  auto first = code()->InstructionAt(instr_index);
  // Handle fixed temporaries.
  for (size_t i = 0; i < first->TempCount(); i++) {
    auto temp = UnallocatedOperand::cast(first->TempAt(i));
    if (temp->HasFixedPolicy()) AllocateFixed(temp, instr_index, false);
  }
  // Handle constant/fixed output operands.
  for (size_t i = 0; i < first->OutputCount(); i++) {
    InstructionOperand* output = first->OutputAt(i);
    if (output->IsConstant()) {
      int output_vreg = ConstantOperand::cast(output)->virtual_register();
      auto range = data()->GetOrCreateLiveRangeFor(output_vreg);
      range->SetSpillStartIndex(instr_index + 1);
      range->SetSpillOperand(output);
      continue;
    }
    auto first_output = UnallocatedOperand::cast(output);
    auto range =
        data()->GetOrCreateLiveRangeFor(first_output->virtual_register());
    bool assigned = false;
    if (first_output->HasFixedPolicy()) {
      int output_vreg = first_output->virtual_register();
      UnallocatedOperand output_copy(UnallocatedOperand::ANY, output_vreg);
      bool is_tagged = code()->IsReference(output_vreg);
      AllocateFixed(first_output, instr_index, is_tagged);

      // This value is produced on the stack, we never need to spill it.
      if (first_output->IsStackSlot()) {
        DCHECK(StackSlotOperand::cast(first_output)->index() <
               data()->frame()->GetTotalFrameSlotCount());
        range->SetSpillOperand(StackSlotOperand::cast(first_output));
        range->SetSpillStartIndex(instr_index + 1);
        assigned = true;
      }
      data()->AddGapMove(instr_index + 1, Instruction::START, *first_output,
                         output_copy);
    }
    // Make sure we add a gap move for spilling (if we have not done
    // so already).
    if (!assigned) {
      range->SpillAtDefinition(allocation_zone(), instr_index + 1,
                               first_output);
      range->SetSpillStartIndex(instr_index + 1);
    }
  }
}


void ConstraintBuilder::MeetConstraintsBefore(int instr_index) {
  auto second = code()->InstructionAt(instr_index);
  // Handle fixed input operands of second instruction.
  for (size_t i = 0; i < second->InputCount(); i++) {
    auto input = second->InputAt(i);
    if (input->IsImmediate()) continue;  // Ignore immediates.
    auto cur_input = UnallocatedOperand::cast(input);
    if (cur_input->HasFixedPolicy()) {
      int input_vreg = cur_input->virtual_register();
      UnallocatedOperand input_copy(UnallocatedOperand::ANY, input_vreg);
      bool is_tagged = code()->IsReference(input_vreg);
      AllocateFixed(cur_input, instr_index, is_tagged);
      data()->AddGapMove(instr_index, Instruction::END, input_copy, *cur_input);
    }
  }
  // Handle "output same as input" for second instruction.
  for (size_t i = 0; i < second->OutputCount(); i++) {
    auto output = second->OutputAt(i);
    if (!output->IsUnallocated()) continue;
    auto second_output = UnallocatedOperand::cast(output);
    if (!second_output->HasSameAsInputPolicy()) continue;
    DCHECK(i == 0);  // Only valid for first output.
    UnallocatedOperand* cur_input =
        UnallocatedOperand::cast(second->InputAt(0));
    int output_vreg = second_output->virtual_register();
    int input_vreg = cur_input->virtual_register();
    UnallocatedOperand input_copy(UnallocatedOperand::ANY, input_vreg);
    cur_input->set_virtual_register(second_output->virtual_register());
    auto gap_move = data()->AddGapMove(instr_index, Instruction::END,
                                       input_copy, *cur_input);
    if (code()->IsReference(input_vreg) && !code()->IsReference(output_vreg)) {
      if (second->HasReferenceMap()) {
        RegisterAllocationData::DelayedReference delayed_reference = {
            second->reference_map(), &gap_move->source()};
        data()->delayed_references().push_back(delayed_reference);
      }
    } else if (!code()->IsReference(input_vreg) &&
               code()->IsReference(output_vreg)) {
      // The input is assumed to immediately have a tagged representation,
      // before the pointer map can be used. I.e. the pointer map at the
      // instruction will include the output operand (whose value at the
      // beginning of the instruction is equal to the input operand). If
      // this is not desired, then the pointer map at this instruction needs
      // to be adjusted manually.
    }
  }
}


void ConstraintBuilder::ResolvePhis() {
  // Process the blocks in reverse order.
  for (InstructionBlock* block : base::Reversed(code()->instruction_blocks())) {
    ResolvePhis(block);
  }
}


void ConstraintBuilder::ResolvePhis(const InstructionBlock* block) {
  for (auto phi : block->phis()) {
    int phi_vreg = phi->virtual_register();
    auto map_value = data()->InitializePhiMap(block, phi);
    auto& output = phi->output();
    // Map the destination operands, so the commitment phase can find them.
    for (size_t i = 0; i < phi->operands().size(); ++i) {
      InstructionBlock* cur_block =
          code()->InstructionBlockAt(block->predecessors()[i]);
      UnallocatedOperand input(UnallocatedOperand::ANY, phi->operands()[i]);
      auto move = data()->AddGapMove(cur_block->last_instruction_index(),
                                     Instruction::END, input, output);
      map_value->AddOperand(&move->destination());
      DCHECK(!code()
                  ->InstructionAt(cur_block->last_instruction_index())
                  ->HasReferenceMap());
    }
    auto live_range = data()->GetOrCreateLiveRangeFor(phi_vreg);
    int gap_index = block->first_instruction_index();
    live_range->SpillAtDefinition(allocation_zone(), gap_index, &output);
    live_range->SetSpillStartIndex(gap_index);
    // We use the phi-ness of some nodes in some later heuristics.
    live_range->set_is_phi(true);
    live_range->set_is_non_loop_phi(!block->IsLoopHeader());
  }
}


LiveRangeBuilder::LiveRangeBuilder(RegisterAllocationData* data,
                                   Zone* local_zone)
    : data_(data), phi_hints_(local_zone) {}


BitVector* LiveRangeBuilder::ComputeLiveOut(const InstructionBlock* block,
                                            RegisterAllocationData* data) {
  size_t block_index = block->rpo_number().ToSize();
  BitVector* live_out = data->live_out_sets()[block_index];
  if (live_out == nullptr) {
    // Compute live out for the given block, except not including backward
    // successor edges.
    Zone* zone = data->allocation_zone();
    const InstructionSequence* code = data->code();

    live_out = new (zone) BitVector(code->VirtualRegisterCount(), zone);

    // Process all successor blocks.
    for (const RpoNumber& succ : block->successors()) {
      // Add values live on entry to the successor.
      if (succ <= block->rpo_number()) continue;
      BitVector* live_in = data->live_in_sets()[succ.ToSize()];
      if (live_in != nullptr) live_out->Union(*live_in);

      // All phi input operands corresponding to this successor edge are live
      // out from this block.
      auto successor = code->InstructionBlockAt(succ);
      size_t index = successor->PredecessorIndexOf(block->rpo_number());
      DCHECK(index < successor->PredecessorCount());
      for (PhiInstruction* phi : successor->phis()) {
        live_out->Add(phi->operands()[index]);
      }
    }
    data->live_out_sets()[block_index] = live_out;
  }
  return live_out;
}


void LiveRangeBuilder::AddInitialIntervals(const InstructionBlock* block,
                                           BitVector* live_out) {
  // Add an interval that includes the entire block to the live range for
  // each live_out value.
  auto start = LifetimePosition::GapFromInstructionIndex(
      block->first_instruction_index());
  auto end = LifetimePosition::InstructionFromInstructionIndex(
                 block->last_instruction_index()).NextStart();
  BitVector::Iterator iterator(live_out);
  while (!iterator.Done()) {
    int operand_index = iterator.Current();
    auto range = data()->GetOrCreateLiveRangeFor(operand_index);
    range->AddUseInterval(start, end, allocation_zone());
    iterator.Advance();
  }
}


int LiveRangeBuilder::FixedDoubleLiveRangeID(int index) {
  return -index - 1 - config()->num_general_registers();
}


TopLevelLiveRange* LiveRangeBuilder::FixedLiveRangeFor(int index) {
  DCHECK(index < config()->num_general_registers());
  auto result = data()->fixed_live_ranges()[index];
  if (result == nullptr) {
    result = data()->NewLiveRange(FixedLiveRangeID(index),
                                  InstructionSequence::DefaultRepresentation());
    DCHECK(result->IsFixed());
    result->set_assigned_register(index);
    data()->MarkAllocated(GENERAL_REGISTERS, index);
    data()->fixed_live_ranges()[index] = result;
  }
  return result;
}


TopLevelLiveRange* LiveRangeBuilder::FixedDoubleLiveRangeFor(int index) {
  DCHECK(index < config()->num_aliased_double_registers());
  auto result = data()->fixed_double_live_ranges()[index];
  if (result == nullptr) {
    result = data()->NewLiveRange(FixedDoubleLiveRangeID(index), kRepFloat64);
    DCHECK(result->IsFixed());
    result->set_assigned_register(index);
    data()->MarkAllocated(DOUBLE_REGISTERS, index);
    data()->fixed_double_live_ranges()[index] = result;
  }
  return result;
}


TopLevelLiveRange* LiveRangeBuilder::LiveRangeFor(InstructionOperand* operand) {
  if (operand->IsUnallocated()) {
    return data()->GetOrCreateLiveRangeFor(
        UnallocatedOperand::cast(operand)->virtual_register());
  } else if (operand->IsConstant()) {
    return data()->GetOrCreateLiveRangeFor(
        ConstantOperand::cast(operand)->virtual_register());
  } else if (operand->IsRegister()) {
    return FixedLiveRangeFor(RegisterOperand::cast(operand)->index());
  } else if (operand->IsDoubleRegister()) {
    return FixedDoubleLiveRangeFor(
        DoubleRegisterOperand::cast(operand)->index());
  } else {
    return nullptr;
  }
}


UsePosition* LiveRangeBuilder::NewUsePosition(LifetimePosition pos,
                                              InstructionOperand* operand,
                                              void* hint,
                                              UsePositionHintType hint_type) {
  return new (allocation_zone()) UsePosition(pos, operand, hint, hint_type);
}


UsePosition* LiveRangeBuilder::Define(LifetimePosition position,
                                      InstructionOperand* operand, void* hint,
                                      UsePositionHintType hint_type) {
  auto range = LiveRangeFor(operand);
  if (range == nullptr) return nullptr;

  if (range->IsEmpty() || range->Start() > position) {
    // Can happen if there is a definition without use.
    range->AddUseInterval(position, position.NextStart(), allocation_zone());
    range->AddUsePosition(NewUsePosition(position.NextStart()));
  } else {
    range->ShortenTo(position);
  }
  if (!operand->IsUnallocated()) return nullptr;
  auto unalloc_operand = UnallocatedOperand::cast(operand);
  auto use_pos = NewUsePosition(position, unalloc_operand, hint, hint_type);
  range->AddUsePosition(use_pos);
  return use_pos;
}


UsePosition* LiveRangeBuilder::Use(LifetimePosition block_start,
                                   LifetimePosition position,
                                   InstructionOperand* operand, void* hint,
                                   UsePositionHintType hint_type) {
  auto range = LiveRangeFor(operand);
  if (range == nullptr) return nullptr;
  UsePosition* use_pos = nullptr;
  if (operand->IsUnallocated()) {
    UnallocatedOperand* unalloc_operand = UnallocatedOperand::cast(operand);
    use_pos = NewUsePosition(position, unalloc_operand, hint, hint_type);
    range->AddUsePosition(use_pos);
  }
  range->AddUseInterval(block_start, position, allocation_zone());
  return use_pos;
}


void LiveRangeBuilder::ProcessInstructions(const InstructionBlock* block,
                                           BitVector* live) {
  int block_start = block->first_instruction_index();
  auto block_start_position =
      LifetimePosition::GapFromInstructionIndex(block_start);

  for (int index = block->last_instruction_index(); index >= block_start;
       index--) {
    auto curr_position =
        LifetimePosition::InstructionFromInstructionIndex(index);
    auto instr = code()->InstructionAt(index);
    DCHECK(instr != nullptr);
    DCHECK(curr_position.IsInstructionPosition());
    // Process output, inputs, and temps of this instruction.
    for (size_t i = 0; i < instr->OutputCount(); i++) {
      auto output = instr->OutputAt(i);
      if (output->IsUnallocated()) {
        // Unsupported.
        DCHECK(!UnallocatedOperand::cast(output)->HasSlotPolicy());
        int out_vreg = UnallocatedOperand::cast(output)->virtual_register();
        live->Remove(out_vreg);
      } else if (output->IsConstant()) {
        int out_vreg = ConstantOperand::cast(output)->virtual_register();
        live->Remove(out_vreg);
      }
      if (block->IsHandler() && index == block_start) {
        // The register defined here is blocked from gap start - it is the
        // exception value.
        // TODO(mtrofin): should we explore an explicit opcode for
        // the first instruction in the handler?
        Define(LifetimePosition::GapFromInstructionIndex(index), output);
      } else {
        Define(curr_position, output);
      }
    }

    if (instr->ClobbersRegisters()) {
      for (int i = 0; i < config()->num_general_registers(); ++i) {
        if (!IsOutputRegisterOf(instr, i)) {
          auto range = FixedLiveRangeFor(i);
          range->AddUseInterval(curr_position, curr_position.End(),
                                allocation_zone());
        }
      }
    }

    if (instr->ClobbersDoubleRegisters()) {
      for (int i = 0; i < config()->num_aliased_double_registers(); ++i) {
        if (!IsOutputDoubleRegisterOf(instr, i)) {
          auto range = FixedDoubleLiveRangeFor(i);
          range->AddUseInterval(curr_position, curr_position.End(),
                                allocation_zone());
        }
      }
    }

    for (size_t i = 0; i < instr->InputCount(); i++) {
      auto input = instr->InputAt(i);
      if (input->IsImmediate()) continue;  // Ignore immediates.
      LifetimePosition use_pos;
      if (input->IsUnallocated() &&
          UnallocatedOperand::cast(input)->IsUsedAtStart()) {
        use_pos = curr_position;
      } else {
        use_pos = curr_position.End();
      }

      if (input->IsUnallocated()) {
        UnallocatedOperand* unalloc = UnallocatedOperand::cast(input);
        int vreg = unalloc->virtual_register();
        live->Add(vreg);
        if (unalloc->HasSlotPolicy()) {
          data()->GetOrCreateLiveRangeFor(vreg)->set_has_slot_use(true);
        }
      }
      Use(block_start_position, use_pos, input);
    }

    for (size_t i = 0; i < instr->TempCount(); i++) {
      auto temp = instr->TempAt(i);
      // Unsupported.
      DCHECK_IMPLIES(temp->IsUnallocated(),
                     !UnallocatedOperand::cast(temp)->HasSlotPolicy());
      if (instr->ClobbersTemps()) {
        if (temp->IsRegister()) continue;
        if (temp->IsUnallocated()) {
          UnallocatedOperand* temp_unalloc = UnallocatedOperand::cast(temp);
          if (temp_unalloc->HasFixedPolicy()) {
            continue;
          }
        }
      }
      Use(block_start_position, curr_position.End(), temp);
      Define(curr_position, temp);
    }

    // Process the moves of the instruction's gaps, making their sources live.
    const Instruction::GapPosition kPositions[] = {Instruction::END,
                                                   Instruction::START};
    curr_position = curr_position.PrevStart();
    DCHECK(curr_position.IsGapPosition());
    for (auto position : kPositions) {
      auto move = instr->GetParallelMove(position);
      if (move == nullptr) continue;
      if (position == Instruction::END) {
        curr_position = curr_position.End();
      } else {
        curr_position = curr_position.Start();
      }
      for (auto cur : *move) {
        auto& from = cur->source();
        auto& to = cur->destination();
        void* hint = &to;
        UsePositionHintType hint_type = UsePosition::HintTypeForOperand(to);
        UsePosition* to_use = nullptr;
        int phi_vreg = -1;
        if (to.IsUnallocated()) {
          int to_vreg = UnallocatedOperand::cast(to).virtual_register();
          auto to_range = data()->GetOrCreateLiveRangeFor(to_vreg);
          if (to_range->is_phi()) {
            phi_vreg = to_vreg;
            if (to_range->is_non_loop_phi()) {
              hint = to_range->current_hint_position();
              hint_type = hint == nullptr ? UsePositionHintType::kNone
                                          : UsePositionHintType::kUsePos;
            } else {
              hint_type = UsePositionHintType::kPhi;
              hint = data()->GetPhiMapValueFor(to_vreg);
            }
          } else {
            if (live->Contains(to_vreg)) {
              to_use = Define(curr_position, &to, &from,
                              UsePosition::HintTypeForOperand(from));
              live->Remove(to_vreg);
            } else {
              cur->Eliminate();
              continue;
            }
          }
        } else {
          Define(curr_position, &to);
        }
        auto from_use =
            Use(block_start_position, curr_position, &from, hint, hint_type);
        // Mark range live.
        if (from.IsUnallocated()) {
          live->Add(UnallocatedOperand::cast(from).virtual_register());
        }
        // Resolve use position hints just created.
        if (to_use != nullptr && from_use != nullptr) {
          to_use->ResolveHint(from_use);
          from_use->ResolveHint(to_use);
        }
        DCHECK_IMPLIES(to_use != nullptr, to_use->IsResolved());
        DCHECK_IMPLIES(from_use != nullptr, from_use->IsResolved());
        // Potentially resolve phi hint.
        if (phi_vreg != -1) ResolvePhiHint(&from, from_use);
      }
    }
  }
}


void LiveRangeBuilder::ProcessPhis(const InstructionBlock* block,
                                   BitVector* live) {
  for (auto phi : block->phis()) {
    // The live range interval already ends at the first instruction of the
    // block.
    int phi_vreg = phi->virtual_register();
    live->Remove(phi_vreg);
    InstructionOperand* hint = nullptr;
    auto instr = GetLastInstruction(
        code(), code()->InstructionBlockAt(block->predecessors()[0]));
    for (auto move : *instr->GetParallelMove(Instruction::END)) {
      auto& to = move->destination();
      if (to.IsUnallocated() &&
          UnallocatedOperand::cast(to).virtual_register() == phi_vreg) {
        hint = &move->source();
        break;
      }
    }
    DCHECK(hint != nullptr);
    auto block_start = LifetimePosition::GapFromInstructionIndex(
        block->first_instruction_index());
    auto use_pos = Define(block_start, &phi->output(), hint,
                          UsePosition::HintTypeForOperand(*hint));
    MapPhiHint(hint, use_pos);
  }
}


void LiveRangeBuilder::ProcessLoopHeader(const InstructionBlock* block,
                                         BitVector* live) {
  DCHECK(block->IsLoopHeader());
  // Add a live range stretching from the first loop instruction to the last
  // for each value live on entry to the header.
  BitVector::Iterator iterator(live);
  auto start = LifetimePosition::GapFromInstructionIndex(
      block->first_instruction_index());
  auto end = LifetimePosition::GapFromInstructionIndex(
                 code()->LastLoopInstructionIndex(block)).NextFullStart();
  while (!iterator.Done()) {
    int operand_index = iterator.Current();
    TopLevelLiveRange* range = data()->GetOrCreateLiveRangeFor(operand_index);
    range->EnsureInterval(start, end, allocation_zone());
    iterator.Advance();
  }
  // Insert all values into the live in sets of all blocks in the loop.
  for (int i = block->rpo_number().ToInt() + 1; i < block->loop_end().ToInt();
       ++i) {
    live_in_sets()[i]->Union(*live);
  }
}


void LiveRangeBuilder::BuildLiveRanges() {
  // Process the blocks in reverse order.
  for (int block_id = code()->InstructionBlockCount() - 1; block_id >= 0;
       --block_id) {
    auto block = code()->InstructionBlockAt(RpoNumber::FromInt(block_id));
    auto live = ComputeLiveOut(block, data());
    // Initially consider all live_out values live for the entire block. We
    // will shorten these intervals if necessary.
    AddInitialIntervals(block, live);
    // Process the instructions in reverse order, generating and killing
    // live values.
    ProcessInstructions(block, live);
    // All phi output operands are killed by this block.
    ProcessPhis(block, live);
    // Now live is live_in for this block except not including values live
    // out on backward successor edges.
    if (block->IsLoopHeader()) ProcessLoopHeader(block, live);
    live_in_sets()[block_id] = live;
  }
  // Postprocess the ranges.
  for (auto range : data()->live_ranges()) {
    if (range == nullptr) continue;
    // Give slots to all ranges with a non fixed slot use.
    if (range->has_slot_use() && range->HasNoSpillType()) {
      data()->AssignSpillRangeToLiveRange(range);
    }
    // TODO(bmeurer): This is a horrible hack to make sure that for constant
    // live ranges, every use requires the constant to be in a register.
    // Without this hack, all uses with "any" policy would get the constant
    // operand assigned.
    if (range->HasSpillOperand() && range->GetSpillOperand()->IsConstant()) {
      for (auto pos = range->first_pos(); pos != nullptr; pos = pos->next()) {
        if (pos->type() == UsePositionType::kRequiresSlot) continue;
        UsePositionType new_type = UsePositionType::kAny;
        // Can't mark phis as needing a register.
        if (!pos->pos().IsGapPosition()) {
          new_type = UsePositionType::kRequiresRegister;
        }
        pos->set_type(new_type, true);
      }
    }
  }
#ifdef DEBUG
  Verify();
#endif
}


void LiveRangeBuilder::MapPhiHint(InstructionOperand* operand,
                                  UsePosition* use_pos) {
  DCHECK(!use_pos->IsResolved());
  auto res = phi_hints_.insert(std::make_pair(operand, use_pos));
  DCHECK(res.second);
  USE(res);
}


void LiveRangeBuilder::ResolvePhiHint(InstructionOperand* operand,
                                      UsePosition* use_pos) {
  auto it = phi_hints_.find(operand);
  if (it == phi_hints_.end()) return;
  DCHECK(!it->second->IsResolved());
  it->second->ResolveHint(use_pos);
}


void LiveRangeBuilder::Verify() const {
  for (auto& hint : phi_hints_) {
    CHECK(hint.second->IsResolved());
  }
  for (LiveRange* current : data()->live_ranges()) {
    if (current != nullptr) current->Verify();
  }
}


RegisterAllocator::RegisterAllocator(RegisterAllocationData* data,
                                     RegisterKind kind)
    : data_(data),
      mode_(kind),
      num_registers_(GetRegisterCount(data->config(), kind)) {}


LiveRange* RegisterAllocator::SplitRangeAt(LiveRange* range,
                                           LifetimePosition pos) {
  DCHECK(!range->TopLevel()->IsFixed());
  TRACE("Splitting live range %d:%d at %d\n", range->TopLevel()->vreg(),
        range->relative_id(), pos.value());

  if (pos <= range->Start()) return range;

  // We can't properly connect liveranges if splitting occurred at the end
  // a block.
  DCHECK(pos.IsStart() || pos.IsGapPosition() ||
         (GetInstructionBlock(code(), pos)->last_instruction_index() !=
          pos.ToInstructionIndex()));

  LiveRange* result = range->SplitAt(pos, allocation_zone());
  return result;
}


LiveRange* RegisterAllocator::SplitBetween(LiveRange* range,
                                           LifetimePosition start,
                                           LifetimePosition end) {
  DCHECK(!range->TopLevel()->IsFixed());
  TRACE("Splitting live range %d:%d in position between [%d, %d]\n",
        range->TopLevel()->vreg(), range->relative_id(), start.value(),
        end.value());

  auto split_pos = FindOptimalSplitPos(start, end);
  DCHECK(split_pos >= start);
  return SplitRangeAt(range, split_pos);
}


LifetimePosition RegisterAllocator::FindOptimalSplitPos(LifetimePosition start,
                                                        LifetimePosition end) {
  int start_instr = start.ToInstructionIndex();
  int end_instr = end.ToInstructionIndex();
  DCHECK(start_instr <= end_instr);

  // We have no choice
  if (start_instr == end_instr) return end;

  auto start_block = GetInstructionBlock(code(), start);
  auto end_block = GetInstructionBlock(code(), end);

  if (end_block == start_block) {
    // The interval is split in the same basic block. Split at the latest
    // possible position.
    return end;
  }

  auto block = end_block;
  // Find header of outermost loop.
  // TODO(titzer): fix redundancy below.
  while (GetContainingLoop(code(), block) != nullptr &&
         GetContainingLoop(code(), block)->rpo_number().ToInt() >
             start_block->rpo_number().ToInt()) {
    block = GetContainingLoop(code(), block);
  }

  // We did not find any suitable outer loop. Split at the latest possible
  // position unless end_block is a loop header itself.
  if (block == end_block && !end_block->IsLoopHeader()) return end;

  return LifetimePosition::GapFromInstructionIndex(
      block->first_instruction_index());
}


LifetimePosition RegisterAllocator::FindOptimalSpillingPos(
    LiveRange* range, LifetimePosition pos) {
  auto block = GetInstructionBlock(code(), pos.Start());
  auto loop_header =
      block->IsLoopHeader() ? block : GetContainingLoop(code(), block);

  if (loop_header == nullptr) return pos;

  auto prev_use = range->PreviousUsePositionRegisterIsBeneficial(pos);

  while (loop_header != nullptr) {
    // We are going to spill live range inside the loop.
    // If possible try to move spilling position backwards to loop header.
    // This will reduce number of memory moves on the back edge.
    auto loop_start = LifetimePosition::GapFromInstructionIndex(
        loop_header->first_instruction_index());

    if (range->Covers(loop_start)) {
      if (prev_use == nullptr || prev_use->pos() < loop_start) {
        // No register beneficial use inside the loop before the pos.
        pos = loop_start;
      }
    }

    // Try hoisting out to an outer loop.
    loop_header = GetContainingLoop(code(), loop_header);
  }

  return pos;
}


void RegisterAllocator::Spill(LiveRange* range) {
  DCHECK(!range->spilled());
  TopLevelLiveRange* first = range->TopLevel();
  TRACE("Spilling live range %d:%d\n", first->vreg(), range->relative_id());

  if (first->HasNoSpillType()) {
    data()->AssignSpillRangeToLiveRange(first);
  }
  range->Spill();
}


const ZoneVector<TopLevelLiveRange*>& RegisterAllocator::GetFixedRegisters()
    const {
  return mode() == DOUBLE_REGISTERS ? data()->fixed_double_live_ranges()
                                    : data()->fixed_live_ranges();
}


const char* RegisterAllocator::RegisterName(int allocation_index) const {
  if (mode() == GENERAL_REGISTERS) {
    return data()->config()->general_register_name(allocation_index);
  } else {
    return data()->config()->double_register_name(allocation_index);
  }
}


LinearScanAllocator::LinearScanAllocator(RegisterAllocationData* data,
                                         RegisterKind kind, Zone* local_zone)
    : RegisterAllocator(data, kind),
      unhandled_live_ranges_(local_zone),
      active_live_ranges_(local_zone),
      inactive_live_ranges_(local_zone) {
  unhandled_live_ranges().reserve(
      static_cast<size_t>(code()->VirtualRegisterCount() * 2));
  active_live_ranges().reserve(8);
  inactive_live_ranges().reserve(8);
  // TryAllocateFreeReg and AllocateBlockedReg assume this
  // when allocating local arrays.
  DCHECK(RegisterConfiguration::kMaxDoubleRegisters >=
         this->data()->config()->num_general_registers());
}


void LinearScanAllocator::AllocateRegisters() {
  DCHECK(unhandled_live_ranges().empty());
  DCHECK(active_live_ranges().empty());
  DCHECK(inactive_live_ranges().empty());

  for (LiveRange* range : data()->live_ranges()) {
    if (range == nullptr) continue;
    if (range->kind() == mode()) {
      AddToUnhandledUnsorted(range);
    }
  }
  SortUnhandled();
  DCHECK(UnhandledIsSorted());

  auto& fixed_ranges = GetFixedRegisters();
  for (auto current : fixed_ranges) {
    if (current != nullptr) {
      DCHECK_EQ(mode(), current->kind());
      AddToInactive(current);
    }
  }

  while (!unhandled_live_ranges().empty()) {
    DCHECK(UnhandledIsSorted());
    auto current = unhandled_live_ranges().back();
    unhandled_live_ranges().pop_back();
    DCHECK(UnhandledIsSorted());
    auto position = current->Start();
#ifdef DEBUG
    allocation_finger_ = position;
#endif
    TRACE("Processing interval %d:%d start=%d\n", current->TopLevel()->vreg(),
          current->relative_id(), position.value());

    if (current->IsTopLevel() && !current->TopLevel()->HasNoSpillType()) {
      TRACE("Live range %d:%d already has a spill operand\n",
            current->TopLevel()->vreg(), current->relative_id());
      auto next_pos = position;
      if (next_pos.IsGapPosition()) {
        next_pos = next_pos.NextStart();
      }
      auto pos = current->NextUsePositionRegisterIsBeneficial(next_pos);
      // If the range already has a spill operand and it doesn't need a
      // register immediately, split it and spill the first part of the range.
      if (pos == nullptr) {
        Spill(current);
        continue;
      } else if (pos->pos() > current->Start().NextStart()) {
        // Do not spill live range eagerly if use position that can benefit from
        // the register is too close to the start of live range.
        SpillBetween(current, current->Start(), pos->pos());
        DCHECK(UnhandledIsSorted());
        continue;
      }
    }

    if (current->IsTopLevel() && TryReuseSpillForPhi(current->TopLevel()))
      continue;

    for (size_t i = 0; i < active_live_ranges().size(); ++i) {
      auto cur_active = active_live_ranges()[i];
      if (cur_active->End() <= position) {
        ActiveToHandled(cur_active);
        --i;  // The live range was removed from the list of active live ranges.
      } else if (!cur_active->Covers(position)) {
        ActiveToInactive(cur_active);
        --i;  // The live range was removed from the list of active live ranges.
      }
    }

    for (size_t i = 0; i < inactive_live_ranges().size(); ++i) {
      auto cur_inactive = inactive_live_ranges()[i];
      if (cur_inactive->End() <= position) {
        InactiveToHandled(cur_inactive);
        --i;  // Live range was removed from the list of inactive live ranges.
      } else if (cur_inactive->Covers(position)) {
        InactiveToActive(cur_inactive);
        --i;  // Live range was removed from the list of inactive live ranges.
      }
    }

    DCHECK(!current->HasRegisterAssigned() && !current->spilled());

    bool result = TryAllocateFreeReg(current);
    if (!result) AllocateBlockedReg(current);
    if (current->HasRegisterAssigned()) {
      AddToActive(current);
    }
  }
}


void LinearScanAllocator::SetLiveRangeAssignedRegister(LiveRange* range,
                                                       int reg) {
  data()->MarkAllocated(range->kind(), reg);
  range->set_assigned_register(reg);
  range->SetUseHints(reg);
  if (range->IsTopLevel() && range->TopLevel()->is_phi()) {
    data()->GetPhiMapValueFor(range->TopLevel())->set_assigned_register(reg);
  }
}


void LinearScanAllocator::AddToActive(LiveRange* range) {
  TRACE("Add live range %d:%d to active\n", range->TopLevel()->vreg(),
        range->relative_id());
  active_live_ranges().push_back(range);
}


void LinearScanAllocator::AddToInactive(LiveRange* range) {
  TRACE("Add live range %d:%d to inactive\n", range->TopLevel()->vreg(),
        range->relative_id());
  inactive_live_ranges().push_back(range);
}


void LinearScanAllocator::AddToUnhandledSorted(LiveRange* range) {
  if (range == nullptr || range->IsEmpty()) return;
  DCHECK(!range->HasRegisterAssigned() && !range->spilled());
  DCHECK(allocation_finger_ <= range->Start());
  for (int i = static_cast<int>(unhandled_live_ranges().size() - 1); i >= 0;
       --i) {
    auto cur_range = unhandled_live_ranges().at(i);
    if (!range->ShouldBeAllocatedBefore(cur_range)) continue;
    TRACE("Add live range %d:%d to unhandled at %d\n",
          range->TopLevel()->vreg(), range->relative_id(), i + 1);
    auto it = unhandled_live_ranges().begin() + (i + 1);
    unhandled_live_ranges().insert(it, range);
    DCHECK(UnhandledIsSorted());
    return;
  }
  TRACE("Add live range %d:%d to unhandled at start\n",
        range->TopLevel()->vreg(), range->relative_id());
  unhandled_live_ranges().insert(unhandled_live_ranges().begin(), range);
  DCHECK(UnhandledIsSorted());
}


void LinearScanAllocator::AddToUnhandledUnsorted(LiveRange* range) {
  if (range == nullptr || range->IsEmpty()) return;
  DCHECK(!range->HasRegisterAssigned() && !range->spilled());
  TRACE("Add live range %d:%d to unhandled unsorted at end\n",
        range->TopLevel()->vreg(), range->relative_id());
  unhandled_live_ranges().push_back(range);
}


static bool UnhandledSortHelper(LiveRange* a, LiveRange* b) {
  DCHECK(!a->ShouldBeAllocatedBefore(b) || !b->ShouldBeAllocatedBefore(a));
  if (a->ShouldBeAllocatedBefore(b)) return false;
  if (b->ShouldBeAllocatedBefore(a)) return true;
  return a->TopLevel()->vreg() < b->TopLevel()->vreg();
}


// Sort the unhandled live ranges so that the ranges to be processed first are
// at the end of the array list.  This is convenient for the register allocation
// algorithm because it is efficient to remove elements from the end.
void LinearScanAllocator::SortUnhandled() {
  TRACE("Sort unhandled\n");
  std::sort(unhandled_live_ranges().begin(), unhandled_live_ranges().end(),
            &UnhandledSortHelper);
}


bool LinearScanAllocator::UnhandledIsSorted() {
  size_t len = unhandled_live_ranges().size();
  for (size_t i = 1; i < len; i++) {
    auto a = unhandled_live_ranges().at(i - 1);
    auto b = unhandled_live_ranges().at(i);
    if (a->Start() < b->Start()) return false;
  }
  return true;
}


void LinearScanAllocator::ActiveToHandled(LiveRange* range) {
  RemoveElement(&active_live_ranges(), range);
  TRACE("Moving live range %d:%d from active to handled\n",
        range->TopLevel()->vreg(), range->relative_id());
}


void LinearScanAllocator::ActiveToInactive(LiveRange* range) {
  RemoveElement(&active_live_ranges(), range);
  inactive_live_ranges().push_back(range);
  TRACE("Moving live range %d:%d from active to inactive\n",
        range->TopLevel()->vreg(), range->relative_id());
}


void LinearScanAllocator::InactiveToHandled(LiveRange* range) {
  RemoveElement(&inactive_live_ranges(), range);
  TRACE("Moving live range %d:%d from inactive to handled\n",
        range->TopLevel()->vreg(), range->relative_id());
}


void LinearScanAllocator::InactiveToActive(LiveRange* range) {
  RemoveElement(&inactive_live_ranges(), range);
  active_live_ranges().push_back(range);
  TRACE("Moving live range %d:%d from inactive to active\n",
        range->TopLevel()->vreg(), range->relative_id());
}


bool LinearScanAllocator::TryAllocateFreeReg(LiveRange* current) {
  LifetimePosition free_until_pos[RegisterConfiguration::kMaxDoubleRegisters];

  for (int i = 0; i < num_registers(); i++) {
    free_until_pos[i] = LifetimePosition::MaxPosition();
  }

  for (auto cur_active : active_live_ranges()) {
    free_until_pos[cur_active->assigned_register()] =
        LifetimePosition::GapFromInstructionIndex(0);
  }

  for (auto cur_inactive : inactive_live_ranges()) {
    DCHECK(cur_inactive->End() > current->Start());
    auto next_intersection = cur_inactive->FirstIntersection(current);
    if (!next_intersection.IsValid()) continue;
    int cur_reg = cur_inactive->assigned_register();
    free_until_pos[cur_reg] = Min(free_until_pos[cur_reg], next_intersection);
  }

  int hint_register;
  if (current->FirstHintPosition(&hint_register) != nullptr) {
    TRACE(
        "Found reg hint %s (free until [%d) for live range %d:%d (end %d[).\n",
        RegisterName(hint_register), free_until_pos[hint_register].value(),
        current->TopLevel()->vreg(), current->relative_id(),
        current->End().value());

    // The desired register is free until the end of the current live range.
    if (free_until_pos[hint_register] >= current->End()) {
      TRACE("Assigning preferred reg %s to live range %d:%d\n",
            RegisterName(hint_register), current->TopLevel()->vreg(),
            current->relative_id());
      SetLiveRangeAssignedRegister(current, hint_register);
      return true;
    }
  }

  // Find the register which stays free for the longest time.
  int reg = 0;
  for (int i = 1; i < num_registers(); ++i) {
    if (free_until_pos[i] > free_until_pos[reg]) {
      reg = i;
    }
  }

  auto pos = free_until_pos[reg];

  if (pos <= current->Start()) {
    // All registers are blocked.
    return false;
  }

  if (pos < current->End()) {
    // Register reg is available at the range start but becomes blocked before
    // the range end. Split current at position where it becomes blocked.
    auto tail = SplitRangeAt(current, pos);
    AddToUnhandledSorted(tail);
  }

  // Register reg is available at the range start and is free until
  // the range end.
  DCHECK(pos >= current->End());
  TRACE("Assigning free reg %s to live range %d:%d\n", RegisterName(reg),
        current->TopLevel()->vreg(), current->relative_id());
  SetLiveRangeAssignedRegister(current, reg);

  return true;
}


void LinearScanAllocator::AllocateBlockedReg(LiveRange* current) {
  auto register_use = current->NextRegisterPosition(current->Start());
  if (register_use == nullptr) {
    // There is no use in the current live range that requires a register.
    // We can just spill it.
    Spill(current);
    return;
  }

  LifetimePosition use_pos[RegisterConfiguration::kMaxDoubleRegisters];
  LifetimePosition block_pos[RegisterConfiguration::kMaxDoubleRegisters];

  for (int i = 0; i < num_registers(); i++) {
    use_pos[i] = block_pos[i] = LifetimePosition::MaxPosition();
  }

  for (auto range : active_live_ranges()) {
    int cur_reg = range->assigned_register();
    if (range->TopLevel()->IsFixed() ||
        !range->CanBeSpilled(current->Start())) {
      block_pos[cur_reg] = use_pos[cur_reg] =
          LifetimePosition::GapFromInstructionIndex(0);
    } else {
      auto next_use =
          range->NextUsePositionRegisterIsBeneficial(current->Start());
      if (next_use == nullptr) {
        use_pos[cur_reg] = range->End();
      } else {
        use_pos[cur_reg] = next_use->pos();
      }
    }
  }

  for (auto range : inactive_live_ranges()) {
    DCHECK(range->End() > current->Start());
    auto next_intersection = range->FirstIntersection(current);
    if (!next_intersection.IsValid()) continue;
    int cur_reg = range->assigned_register();
    if (range->TopLevel()->IsFixed()) {
      block_pos[cur_reg] = Min(block_pos[cur_reg], next_intersection);
      use_pos[cur_reg] = Min(block_pos[cur_reg], use_pos[cur_reg]);
    } else {
      use_pos[cur_reg] = Min(use_pos[cur_reg], next_intersection);
    }
  }

  int reg = 0;
  for (int i = 1; i < num_registers(); ++i) {
    if (use_pos[i] > use_pos[reg]) {
      reg = i;
    }
  }

  auto pos = use_pos[reg];

  if (pos < register_use->pos()) {
    // All registers are blocked before the first use that requires a register.
    // Spill starting part of live range up to that use.
    SpillBetween(current, current->Start(), register_use->pos());
    return;
  }

  if (block_pos[reg] < current->End()) {
    // Register becomes blocked before the current range end. Split before that
    // position.
    LiveRange* tail =
        SplitBetween(current, current->Start(), block_pos[reg].Start());
    AddToUnhandledSorted(tail);
  }

  // Register reg is not blocked for the whole range.
  DCHECK(block_pos[reg] >= current->End());
  TRACE("Assigning blocked reg %s to live range %d:%d\n", RegisterName(reg),
        current->TopLevel()->vreg(), current->relative_id());
  SetLiveRangeAssignedRegister(current, reg);

  // This register was not free. Thus we need to find and spill
  // parts of active and inactive live regions that use the same register
  // at the same lifetime positions as current.
  SplitAndSpillIntersecting(current);
}


void LinearScanAllocator::SplitAndSpillIntersecting(LiveRange* current) {
  DCHECK(current->HasRegisterAssigned());
  int reg = current->assigned_register();
  auto split_pos = current->Start();
  for (size_t i = 0; i < active_live_ranges().size(); ++i) {
    auto range = active_live_ranges()[i];
    if (range->assigned_register() == reg) {
      auto next_pos = range->NextRegisterPosition(current->Start());
      auto spill_pos = FindOptimalSpillingPos(range, split_pos);
      if (next_pos == nullptr) {
        SpillAfter(range, spill_pos);
      } else {
        // When spilling between spill_pos and next_pos ensure that the range
        // remains spilled at least until the start of the current live range.
        // This guarantees that we will not introduce new unhandled ranges that
        // start before the current range as this violates allocation invariant
        // and will lead to an inconsistent state of active and inactive
        // live-ranges: ranges are allocated in order of their start positions,
        // ranges are retired from active/inactive when the start of the
        // current live-range is larger than their end.
        SpillBetweenUntil(range, spill_pos, current->Start(), next_pos->pos());
      }
      ActiveToHandled(range);
      --i;
    }
  }

  for (size_t i = 0; i < inactive_live_ranges().size(); ++i) {
    auto range = inactive_live_ranges()[i];
    DCHECK(range->End() > current->Start());
    if (range->assigned_register() == reg && !range->TopLevel()->IsFixed()) {
      LifetimePosition next_intersection = range->FirstIntersection(current);
      if (next_intersection.IsValid()) {
        UsePosition* next_pos = range->NextRegisterPosition(current->Start());
        if (next_pos == nullptr) {
          SpillAfter(range, split_pos);
        } else {
          next_intersection = Min(next_intersection, next_pos->pos());
          SpillBetween(range, split_pos, next_intersection);
        }
        InactiveToHandled(range);
        --i;
      }
    }
  }
}


bool LinearScanAllocator::TryReuseSpillForPhi(TopLevelLiveRange* range) {
  if (!range->is_phi()) return false;

  DCHECK(!range->HasSpillOperand());
  auto phi_map_value = data()->GetPhiMapValueFor(range);
  auto phi = phi_map_value->phi();
  auto block = phi_map_value->block();
  // Count the number of spilled operands.
  size_t spilled_count = 0;
  LiveRange* first_op = nullptr;
  for (size_t i = 0; i < phi->operands().size(); i++) {
    int op = phi->operands()[i];
    LiveRange* op_range = data()->GetOrCreateLiveRangeFor(op);
    if (!op_range->TopLevel()->HasSpillRange()) continue;
    auto pred = code()->InstructionBlockAt(block->predecessors()[i]);
    auto pred_end = LifetimePosition::InstructionFromInstructionIndex(
        pred->last_instruction_index());
    while (op_range != nullptr && !op_range->CanCover(pred_end)) {
      op_range = op_range->next();
    }
    if (op_range != nullptr && op_range->spilled()) {
      spilled_count++;
      if (first_op == nullptr) {
        first_op = op_range->TopLevel();
      }
    }
  }

  // Only continue if more than half of the operands are spilled.
  if (spilled_count * 2 <= phi->operands().size()) {
    return false;
  }

  // Try to merge the spilled operands and count the number of merged spilled
  // operands.
  DCHECK(first_op != nullptr);
  auto first_op_spill = first_op->TopLevel()->GetSpillRange();
  size_t num_merged = 1;
  for (size_t i = 1; i < phi->operands().size(); i++) {
    int op = phi->operands()[i];
    auto op_range = data()->GetOrCreateLiveRangeFor(op);
    if (!op_range->HasSpillRange()) continue;
    auto op_spill = op_range->GetSpillRange();
    if (op_spill == first_op_spill || first_op_spill->TryMerge(op_spill)) {
      num_merged++;
    }
  }

  // Only continue if enough operands could be merged to the
  // same spill slot.
  if (num_merged * 2 <= phi->operands().size() ||
      AreUseIntervalsIntersecting(first_op_spill->interval(),
                                  range->first_interval())) {
    return false;
  }

  // If the range does not need register soon, spill it to the merged
  // spill range.
  auto next_pos = range->Start();
  if (next_pos.IsGapPosition()) next_pos = next_pos.NextStart();
  auto pos = range->NextUsePositionRegisterIsBeneficial(next_pos);
  if (pos == nullptr) {
    auto spill_range =
        range->TopLevel()->HasSpillRange()
            ? range->TopLevel()->GetSpillRange()
            : data()->AssignSpillRangeToLiveRange(range->TopLevel());
    bool merged = first_op_spill->TryMerge(spill_range);
    CHECK(merged);
    Spill(range);
    return true;
  } else if (pos->pos() > range->Start().NextStart()) {
    auto spill_range =
        range->TopLevel()->HasSpillRange()
            ? range->TopLevel()->GetSpillRange()
            : data()->AssignSpillRangeToLiveRange(range->TopLevel());
    bool merged = first_op_spill->TryMerge(spill_range);
    CHECK(merged);
    SpillBetween(range, range->Start(), pos->pos());
    DCHECK(UnhandledIsSorted());
    return true;
  }
  return false;
}


void LinearScanAllocator::SpillAfter(LiveRange* range, LifetimePosition pos) {
  auto second_part = SplitRangeAt(range, pos);
  Spill(second_part);
}


void LinearScanAllocator::SpillBetween(LiveRange* range, LifetimePosition start,
                                       LifetimePosition end) {
  SpillBetweenUntil(range, start, start, end);
}


void LinearScanAllocator::SpillBetweenUntil(LiveRange* range,
                                            LifetimePosition start,
                                            LifetimePosition until,
                                            LifetimePosition end) {
  CHECK(start < end);
  auto second_part = SplitRangeAt(range, start);

  if (second_part->Start() < end) {
    // The split result intersects with [start, end[.
    // Split it at position between ]start+1, end[, spill the middle part
    // and put the rest to unhandled.
    auto third_part_end = end.PrevStart().End();
    if (data()->IsBlockBoundary(end.Start())) {
      third_part_end = end.Start();
    }
    auto third_part = SplitBetween(
        second_part, Max(second_part->Start().End(), until), third_part_end);

    DCHECK(third_part != second_part);

    Spill(second_part);
    AddToUnhandledSorted(third_part);
  } else {
    // The split result does not intersect with [start, end[.
    // Nothing to spill. Just put it to unhandled as whole.
    AddToUnhandledSorted(second_part);
  }
}


SpillSlotLocator::SpillSlotLocator(RegisterAllocationData* data)
    : data_(data) {}


void SpillSlotLocator::LocateSpillSlots() {
  auto code = data()->code();
  for (TopLevelLiveRange* range : data()->live_ranges()) {
    if (range == nullptr || range->IsEmpty()) continue;
    // We care only about ranges which spill in the frame.
    if (!range->HasSpillRange()) continue;
    auto spills = range->spills_at_definition();
    DCHECK_NOT_NULL(spills);
    for (; spills != nullptr; spills = spills->next) {
      code->GetInstructionBlock(spills->gap_index)->mark_needs_frame();
    }
  }
}


OperandAssigner::OperandAssigner(RegisterAllocationData* data) : data_(data) {}


void OperandAssigner::AssignSpillSlots() {
  ZoneVector<SpillRange*>& spill_ranges = data()->spill_ranges();
  // Merge disjoint spill ranges
  for (size_t i = 0; i < spill_ranges.size(); ++i) {
    SpillRange* range = spill_ranges[i];
    if (range == nullptr) continue;
    if (range->IsEmpty()) continue;
    for (size_t j = i + 1; j < spill_ranges.size(); ++j) {
      SpillRange* other = spill_ranges[j];
      if (other != nullptr && !other->IsEmpty()) {
        range->TryMerge(other);
      }
    }
  }
  // Allocate slots for the merged spill ranges.
  for (SpillRange* range : spill_ranges) {
    if (range == nullptr || range->IsEmpty()) continue;
    // Allocate a new operand referring to the spill slot.
    int byte_width = range->ByteWidth();
    int index = data()->frame()->AllocateSpillSlot(byte_width);
    range->set_assigned_slot(index);
  }
}


void OperandAssigner::CommitAssignment() {
  for (TopLevelLiveRange* top_range : data()->live_ranges()) {
    if (top_range == nullptr || top_range->IsEmpty()) continue;
    InstructionOperand spill_operand;
    if (top_range->HasSpillOperand()) {
      spill_operand = *top_range->TopLevel()->GetSpillOperand();
    } else if (top_range->TopLevel()->HasSpillRange()) {
      spill_operand = top_range->TopLevel()->GetSpillRangeOperand();
    }
    if (top_range->is_phi()) {
      data()->GetPhiMapValueFor(top_range)->CommitAssignment(
          top_range->GetAssignedOperand());
    }
    for (LiveRange* range = top_range; range != nullptr;
         range = range->next()) {
      auto assigned = range->GetAssignedOperand();
      range->ConvertUsesToOperand(assigned, spill_operand);
    }

    if (!spill_operand.IsInvalid()) {
      // If this top level range has a child spilled in a deferred block, we use
      // the range and control flow connection mechanism instead of spilling at
      // definition. Refer to the ConnectLiveRanges and ResolveControlFlow
      // phases. Normally, when we spill at definition, we do not insert a
      // connecting move when a successor child range is spilled - because the
      // spilled range picks up its value from the slot which was assigned at
      // definition. For ranges that are determined to spill only in deferred
      // blocks, we let ConnectLiveRanges and ResolveControlFlow insert such
      // moves between ranges. Because of how the ranges are split around
      // deferred blocks, this amounts to spilling and filling inside such
      // blocks.
      if (!top_range->TryCommitSpillInDeferredBlock(data()->code(),
                                                    spill_operand)) {
        // Spill at definition if the range isn't spilled only in deferred
        // blocks.
        top_range->CommitSpillsAtDefinition(
            data()->code(), spill_operand,
            top_range->has_slot_use() || top_range->spilled());
      }
    }
  }
}


ReferenceMapPopulator::ReferenceMapPopulator(RegisterAllocationData* data)
    : data_(data) {}


bool ReferenceMapPopulator::SafePointsAreInOrder() const {
  int safe_point = 0;
  for (auto map : *data()->code()->reference_maps()) {
    if (safe_point > map->instruction_position()) return false;
    safe_point = map->instruction_position();
  }
  return true;
}


void ReferenceMapPopulator::PopulateReferenceMaps() {
  DCHECK(SafePointsAreInOrder());
  // Map all delayed references.
  for (auto& delayed_reference : data()->delayed_references()) {
    delayed_reference.map->RecordReference(
        AllocatedOperand::cast(*delayed_reference.operand));
  }
  // Iterate over all safe point positions and record a pointer
  // for all spilled live ranges at this point.
  int last_range_start = 0;
  auto reference_maps = data()->code()->reference_maps();
  ReferenceMapDeque::const_iterator first_it = reference_maps->begin();
  for (TopLevelLiveRange* range : data()->live_ranges()) {
    if (range == nullptr) continue;
    // Skip non-reference values.
    if (!data()->IsReference(range)) continue;
    // Skip empty live ranges.
    if (range->IsEmpty()) continue;

    // Find the extent of the range and its children.
    int start = range->Start().ToInstructionIndex();
    int end = 0;
    for (LiveRange* cur = range; cur != nullptr; cur = cur->next()) {
      auto this_end = cur->End();
      if (this_end.ToInstructionIndex() > end)
        end = this_end.ToInstructionIndex();
      DCHECK(cur->Start().ToInstructionIndex() >= start);
    }

    // Most of the ranges are in order, but not all.  Keep an eye on when they
    // step backwards and reset the first_it so we don't miss any safe points.
    if (start < last_range_start) first_it = reference_maps->begin();
    last_range_start = start;

    // Step across all the safe points that are before the start of this range,
    // recording how far we step in order to save doing this for the next range.
    for (; first_it != reference_maps->end(); ++first_it) {
      auto map = *first_it;
      if (map->instruction_position() >= start) break;
    }

    InstructionOperand spill_operand;
    if (((range->HasSpillOperand() &&
          !range->GetSpillOperand()->IsConstant()) ||
         range->HasSpillRange())) {
      if (range->HasSpillOperand()) {
        spill_operand = *range->GetSpillOperand();
      } else {
        spill_operand = range->GetSpillRangeOperand();
      }
      DCHECK(spill_operand.IsStackSlot());
      DCHECK_EQ(kRepTagged,
                AllocatedOperand::cast(spill_operand).machine_type());
    }

    // Step through the safe points to see whether they are in the range.
    for (auto it = first_it; it != reference_maps->end(); ++it) {
      auto map = *it;
      int safe_point = map->instruction_position();

      // The safe points are sorted so we can stop searching here.
      if (safe_point - 1 > end) break;

      // Advance to the next active range that covers the current
      // safe point position.
      auto safe_point_pos =
          LifetimePosition::InstructionFromInstructionIndex(safe_point);
      LiveRange* cur = range;
      while (cur != nullptr && !cur->Covers(safe_point_pos)) {
        cur = cur->next();
      }
      if (cur == nullptr) continue;

      // Check if the live range is spilled and the safe point is after
      // the spill position.
      int spill_index = range->IsSpilledOnlyInDeferredBlocks()
                            ? cur->Start().ToInstructionIndex()
                            : range->spill_start_index();

      if (!spill_operand.IsInvalid() && safe_point >= spill_index) {
        TRACE("Pointer for range %d (spilled at %d) at safe point %d\n",
              range->vreg(), spill_index, safe_point);
        map->RecordReference(AllocatedOperand::cast(spill_operand));
      }

      if (!cur->spilled()) {
        TRACE(
            "Pointer in register for range %d:%d (start at %d) "
            "at safe point %d\n",
            range->vreg(), cur->relative_id(), cur->Start().value(),
            safe_point);
        auto operand = cur->GetAssignedOperand();
        DCHECK(!operand.IsStackSlot());
        DCHECK_EQ(kRepTagged, AllocatedOperand::cast(operand).machine_type());
        map->RecordReference(AllocatedOperand::cast(operand));
      }
    }
  }
}


namespace {

class LiveRangeBound {
 public:
  explicit LiveRangeBound(const LiveRange* range)
      : range_(range), start_(range->Start()), end_(range->End()) {
    DCHECK(!range->IsEmpty());
  }

  bool CanCover(LifetimePosition position) {
    return start_ <= position && position < end_;
  }

  const LiveRange* const range_;
  const LifetimePosition start_;
  const LifetimePosition end_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveRangeBound);
};


struct FindResult {
  const LiveRange* cur_cover_;
  const LiveRange* pred_cover_;
};


class LiveRangeBoundArray {
 public:
  LiveRangeBoundArray() : length_(0), start_(nullptr) {}

  bool ShouldInitialize() { return start_ == nullptr; }

  void Initialize(Zone* zone, const LiveRange* const range) {
    size_t length = 0;
    for (auto i = range; i != nullptr; i = i->next()) length++;
    start_ = zone->NewArray<LiveRangeBound>(length);
    length_ = length;
    auto curr = start_;
    for (auto i = range; i != nullptr; i = i->next(), ++curr) {
      new (curr) LiveRangeBound(i);
    }
  }

  LiveRangeBound* Find(const LifetimePosition position) const {
    size_t left_index = 0;
    size_t right_index = length_;
    while (true) {
      size_t current_index = left_index + (right_index - left_index) / 2;
      DCHECK(right_index > current_index);
      auto bound = &start_[current_index];
      if (bound->start_ <= position) {
        if (position < bound->end_) return bound;
        DCHECK(left_index < current_index);
        left_index = current_index;
      } else {
        right_index = current_index;
      }
    }
  }

  LiveRangeBound* FindPred(const InstructionBlock* pred) {
    auto pred_end = LifetimePosition::InstructionFromInstructionIndex(
        pred->last_instruction_index());
    return Find(pred_end);
  }

  LiveRangeBound* FindSucc(const InstructionBlock* succ) {
    auto succ_start = LifetimePosition::GapFromInstructionIndex(
        succ->first_instruction_index());
    return Find(succ_start);
  }

  void Find(const InstructionBlock* block, const InstructionBlock* pred,
            FindResult* result) const {
    auto pred_end = LifetimePosition::InstructionFromInstructionIndex(
        pred->last_instruction_index());
    auto bound = Find(pred_end);
    result->pred_cover_ = bound->range_;
    auto cur_start = LifetimePosition::GapFromInstructionIndex(
        block->first_instruction_index());
    // Common case.
    if (bound->CanCover(cur_start)) {
      result->cur_cover_ = bound->range_;
      return;
    }
    result->cur_cover_ = Find(cur_start)->range_;
    DCHECK(result->pred_cover_ != nullptr && result->cur_cover_ != nullptr);
  }

 private:
  size_t length_;
  LiveRangeBound* start_;

  DISALLOW_COPY_AND_ASSIGN(LiveRangeBoundArray);
};


class LiveRangeFinder {
 public:
  explicit LiveRangeFinder(const RegisterAllocationData* data, Zone* zone)
      : data_(data),
        bounds_length_(static_cast<int>(data_->live_ranges().size())),
        bounds_(zone->NewArray<LiveRangeBoundArray>(bounds_length_)),
        zone_(zone) {
    for (int i = 0; i < bounds_length_; ++i) {
      new (&bounds_[i]) LiveRangeBoundArray();
    }
  }

  LiveRangeBoundArray* ArrayFor(int operand_index) {
    DCHECK(operand_index < bounds_length_);
    auto range = data_->live_ranges()[operand_index];
    DCHECK(range != nullptr && !range->IsEmpty());
    auto array = &bounds_[operand_index];
    if (array->ShouldInitialize()) {
      array->Initialize(zone_, range);
    }
    return array;
  }

 private:
  const RegisterAllocationData* const data_;
  const int bounds_length_;
  LiveRangeBoundArray* const bounds_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(LiveRangeFinder);
};


typedef std::pair<ParallelMove*, InstructionOperand> DelayedInsertionMapKey;


struct DelayedInsertionMapCompare {
  bool operator()(const DelayedInsertionMapKey& a,
                  const DelayedInsertionMapKey& b) const {
    if (a.first == b.first) {
      return a.second.Compare(b.second);
    }
    return a.first < b.first;
  }
};


typedef ZoneMap<DelayedInsertionMapKey, InstructionOperand,
                DelayedInsertionMapCompare> DelayedInsertionMap;

}  // namespace


LiveRangeConnector::LiveRangeConnector(RegisterAllocationData* data)
    : data_(data) {}


bool LiveRangeConnector::CanEagerlyResolveControlFlow(
    const InstructionBlock* block) const {
  if (block->PredecessorCount() != 1) return false;
  return block->predecessors()[0].IsNext(block->rpo_number());
}


void LiveRangeConnector::ResolveControlFlow(Zone* local_zone) {
  // Lazily linearize live ranges in memory for fast lookup.
  LiveRangeFinder finder(data(), local_zone);
  auto& live_in_sets = data()->live_in_sets();
  for (auto block : code()->instruction_blocks()) {
    if (CanEagerlyResolveControlFlow(block)) continue;
    auto live = live_in_sets[block->rpo_number().ToInt()];
    BitVector::Iterator iterator(live);
    while (!iterator.Done()) {
      auto* array = finder.ArrayFor(iterator.Current());
      for (auto pred : block->predecessors()) {
        FindResult result;
        const auto* pred_block = code()->InstructionBlockAt(pred);
        array->Find(block, pred_block, &result);
        if (result.cur_cover_ == result.pred_cover_ ||
            (!result.cur_cover_->TopLevel()->IsSpilledOnlyInDeferredBlocks() &&
             result.cur_cover_->spilled()))
          continue;
        auto pred_op = result.pred_cover_->GetAssignedOperand();
        auto cur_op = result.cur_cover_->GetAssignedOperand();
        if (pred_op.Equals(cur_op)) continue;
        ResolveControlFlow(block, cur_op, pred_block, pred_op);
      }
      iterator.Advance();
    }
  }
}


void LiveRangeConnector::ResolveControlFlow(const InstructionBlock* block,
                                            const InstructionOperand& cur_op,
                                            const InstructionBlock* pred,
                                            const InstructionOperand& pred_op) {
  DCHECK(!pred_op.Equals(cur_op));
  int gap_index;
  Instruction::GapPosition position;
  if (block->PredecessorCount() == 1) {
    gap_index = block->first_instruction_index();
    position = Instruction::START;
  } else {
    DCHECK(pred->SuccessorCount() == 1);
    DCHECK(!code()
                ->InstructionAt(pred->last_instruction_index())
                ->HasReferenceMap());
    gap_index = pred->last_instruction_index();
    position = Instruction::END;
  }
  data()->AddGapMove(gap_index, position, pred_op, cur_op);
}


void LiveRangeConnector::ConnectRanges(Zone* local_zone) {
  DelayedInsertionMap delayed_insertion_map(local_zone);
  for (TopLevelLiveRange* top_range : data()->live_ranges()) {
    if (top_range == nullptr) continue;
    bool connect_spilled = top_range->IsSpilledOnlyInDeferredBlocks();
    LiveRange* first_range = top_range;
    for (LiveRange *second_range = first_range->next(); second_range != nullptr;
         first_range = second_range, second_range = second_range->next()) {
      auto pos = second_range->Start();
      // Add gap move if the two live ranges touch and there is no block
      // boundary.
      if (!connect_spilled && second_range->spilled()) continue;
      if (first_range->End() != pos) continue;
      if (data()->IsBlockBoundary(pos) &&
          !CanEagerlyResolveControlFlow(GetInstructionBlock(code(), pos))) {
        continue;
      }
      auto prev_operand = first_range->GetAssignedOperand();
      auto cur_operand = second_range->GetAssignedOperand();
      if (prev_operand.Equals(cur_operand)) continue;
      bool delay_insertion = false;
      Instruction::GapPosition gap_pos;
      int gap_index = pos.ToInstructionIndex();
      if (pos.IsGapPosition()) {
        gap_pos = pos.IsStart() ? Instruction::START : Instruction::END;
      } else {
        if (pos.IsStart()) {
          delay_insertion = true;
        } else {
          gap_index++;
        }
        gap_pos = delay_insertion ? Instruction::END : Instruction::START;
      }
      auto move = code()->InstructionAt(gap_index)->GetOrCreateParallelMove(
          gap_pos, code_zone());
      if (!delay_insertion) {
        move->AddMove(prev_operand, cur_operand);
      } else {
        delayed_insertion_map.insert(
            std::make_pair(std::make_pair(move, prev_operand), cur_operand));
      }
    }
  }
  if (delayed_insertion_map.empty()) return;
  // Insert all the moves which should occur after the stored move.
  ZoneVector<MoveOperands*> to_insert(local_zone);
  ZoneVector<MoveOperands*> to_eliminate(local_zone);
  to_insert.reserve(4);
  to_eliminate.reserve(4);
  auto moves = delayed_insertion_map.begin()->first.first;
  for (auto it = delayed_insertion_map.begin();; ++it) {
    bool done = it == delayed_insertion_map.end();
    if (done || it->first.first != moves) {
      // Commit the MoveOperands for current ParallelMove.
      for (auto move : to_eliminate) {
        move->Eliminate();
      }
      for (auto move : to_insert) {
        moves->push_back(move);
      }
      if (done) break;
      // Reset state.
      to_eliminate.clear();
      to_insert.clear();
      moves = it->first.first;
    }
    // Gather all MoveOperands for a single ParallelMove.
    auto move = new (code_zone()) MoveOperands(it->first.second, it->second);
    auto eliminate = moves->PrepareInsertAfter(move);
    to_insert.push_back(move);
    if (eliminate != nullptr) to_eliminate.push_back(eliminate);
  }
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8

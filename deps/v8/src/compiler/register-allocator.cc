// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/register-allocator.h"

#include "src/compiler/linkage.h"
#include "src/hydrogen.h"
#include "src/string-stream.h"

namespace v8 {
namespace internal {
namespace compiler {

static inline LifetimePosition Min(LifetimePosition a, LifetimePosition b) {
  return a.Value() < b.Value() ? a : b;
}


static inline LifetimePosition Max(LifetimePosition a, LifetimePosition b) {
  return a.Value() > b.Value() ? a : b;
}


UsePosition::UsePosition(LifetimePosition pos, InstructionOperand* operand,
                         InstructionOperand* hint)
    : operand_(operand),
      hint_(hint),
      pos_(pos),
      next_(NULL),
      requires_reg_(false),
      register_beneficial_(true) {
  if (operand_ != NULL && operand_->IsUnallocated()) {
    const UnallocatedOperand* unalloc = UnallocatedOperand::cast(operand_);
    requires_reg_ = unalloc->HasRegisterPolicy();
    register_beneficial_ = !unalloc->HasAnyPolicy();
  }
  DCHECK(pos_.IsValid());
}


bool UsePosition::HasHint() const {
  return hint_ != NULL && !hint_->IsUnallocated();
}


bool UsePosition::RequiresRegister() const { return requires_reg_; }


bool UsePosition::RegisterIsBeneficial() const { return register_beneficial_; }


void UseInterval::SplitAt(LifetimePosition pos, Zone* zone) {
  DCHECK(Contains(pos) && pos.Value() != start().Value());
  UseInterval* after = new (zone) UseInterval(pos, end_);
  after->next_ = next_;
  next_ = after;
  end_ = pos;
}


#ifdef DEBUG


void LiveRange::Verify() const {
  UsePosition* cur = first_pos_;
  while (cur != NULL) {
    DCHECK(Start().Value() <= cur->pos().Value() &&
           cur->pos().Value() <= End().Value());
    cur = cur->next();
  }
}


bool LiveRange::HasOverlap(UseInterval* target) const {
  UseInterval* current_interval = first_interval_;
  while (current_interval != NULL) {
    // Intervals overlap if the start of one is contained in the other.
    if (current_interval->Contains(target->start()) ||
        target->Contains(current_interval->start())) {
      return true;
    }
    current_interval = current_interval->next();
  }
  return false;
}


#endif


LiveRange::LiveRange(int id, Zone* zone)
    : id_(id),
      spilled_(false),
      is_phi_(false),
      is_non_loop_phi_(false),
      kind_(UNALLOCATED_REGISTERS),
      assigned_register_(kInvalidAssignment),
      last_interval_(NULL),
      first_interval_(NULL),
      first_pos_(NULL),
      parent_(NULL),
      next_(NULL),
      current_interval_(NULL),
      last_processed_use_(NULL),
      current_hint_operand_(NULL),
      spill_operand_(new (zone) InstructionOperand()),
      spill_start_index_(kMaxInt) {}


void LiveRange::set_assigned_register(int reg, Zone* zone) {
  DCHECK(!HasRegisterAssigned() && !IsSpilled());
  assigned_register_ = reg;
  ConvertOperands(zone);
}


void LiveRange::MakeSpilled(Zone* zone) {
  DCHECK(!IsSpilled());
  DCHECK(TopLevel()->HasAllocatedSpillOperand());
  spilled_ = true;
  assigned_register_ = kInvalidAssignment;
  ConvertOperands(zone);
}


bool LiveRange::HasAllocatedSpillOperand() const {
  DCHECK(spill_operand_ != NULL);
  return !spill_operand_->IsIgnored();
}


void LiveRange::SetSpillOperand(InstructionOperand* operand) {
  DCHECK(!operand->IsUnallocated());
  DCHECK(spill_operand_ != NULL);
  DCHECK(spill_operand_->IsIgnored());
  spill_operand_->ConvertTo(operand->kind(), operand->index());
}


UsePosition* LiveRange::NextUsePosition(LifetimePosition start) {
  UsePosition* use_pos = last_processed_use_;
  if (use_pos == NULL) use_pos = first_pos();
  while (use_pos != NULL && use_pos->pos().Value() < start.Value()) {
    use_pos = use_pos->next();
  }
  last_processed_use_ = use_pos;
  return use_pos;
}


UsePosition* LiveRange::NextUsePositionRegisterIsBeneficial(
    LifetimePosition start) {
  UsePosition* pos = NextUsePosition(start);
  while (pos != NULL && !pos->RegisterIsBeneficial()) {
    pos = pos->next();
  }
  return pos;
}


UsePosition* LiveRange::PreviousUsePositionRegisterIsBeneficial(
    LifetimePosition start) {
  UsePosition* pos = first_pos();
  UsePosition* prev = NULL;
  while (pos != NULL && pos->pos().Value() < start.Value()) {
    if (pos->RegisterIsBeneficial()) prev = pos;
    pos = pos->next();
  }
  return prev;
}


UsePosition* LiveRange::NextRegisterPosition(LifetimePosition start) {
  UsePosition* pos = NextUsePosition(start);
  while (pos != NULL && !pos->RequiresRegister()) {
    pos = pos->next();
  }
  return pos;
}


bool LiveRange::CanBeSpilled(LifetimePosition pos) {
  // We cannot spill a live range that has a use requiring a register
  // at the current or the immediate next position.
  UsePosition* use_pos = NextRegisterPosition(pos);
  if (use_pos == NULL) return true;
  return use_pos->pos().Value() >
         pos.NextInstruction().InstructionEnd().Value();
}


InstructionOperand* LiveRange::CreateAssignedOperand(Zone* zone) {
  InstructionOperand* op = NULL;
  if (HasRegisterAssigned()) {
    DCHECK(!IsSpilled());
    switch (Kind()) {
      case GENERAL_REGISTERS:
        op = RegisterOperand::Create(assigned_register(), zone);
        break;
      case DOUBLE_REGISTERS:
        op = DoubleRegisterOperand::Create(assigned_register(), zone);
        break;
      default:
        UNREACHABLE();
    }
  } else if (IsSpilled()) {
    DCHECK(!HasRegisterAssigned());
    op = TopLevel()->GetSpillOperand();
    DCHECK(!op->IsUnallocated());
  } else {
    UnallocatedOperand* unalloc =
        new (zone) UnallocatedOperand(UnallocatedOperand::NONE);
    unalloc->set_virtual_register(id_);
    op = unalloc;
  }
  return op;
}


UseInterval* LiveRange::FirstSearchIntervalForPosition(
    LifetimePosition position) const {
  if (current_interval_ == NULL) return first_interval_;
  if (current_interval_->start().Value() > position.Value()) {
    current_interval_ = NULL;
    return first_interval_;
  }
  return current_interval_;
}


void LiveRange::AdvanceLastProcessedMarker(
    UseInterval* to_start_of, LifetimePosition but_not_past) const {
  if (to_start_of == NULL) return;
  if (to_start_of->start().Value() > but_not_past.Value()) return;
  LifetimePosition start = current_interval_ == NULL
                               ? LifetimePosition::Invalid()
                               : current_interval_->start();
  if (to_start_of->start().Value() > start.Value()) {
    current_interval_ = to_start_of;
  }
}


void LiveRange::SplitAt(LifetimePosition position, LiveRange* result,
                        Zone* zone) {
  DCHECK(Start().Value() < position.Value());
  DCHECK(result->IsEmpty());
  // Find the last interval that ends before the position. If the
  // position is contained in one of the intervals in the chain, we
  // split that interval and use the first part.
  UseInterval* current = FirstSearchIntervalForPosition(position);

  // If the split position coincides with the beginning of a use interval
  // we need to split use positons in a special way.
  bool split_at_start = false;

  if (current->start().Value() == position.Value()) {
    // When splitting at start we need to locate the previous use interval.
    current = first_interval_;
  }

  while (current != NULL) {
    if (current->Contains(position)) {
      current->SplitAt(position, zone);
      break;
    }
    UseInterval* next = current->next();
    if (next->start().Value() >= position.Value()) {
      split_at_start = (next->start().Value() == position.Value());
      break;
    }
    current = next;
  }

  // Partition original use intervals to the two live ranges.
  UseInterval* before = current;
  UseInterval* after = before->next();
  result->last_interval_ =
      (last_interval_ == before)
          ? after            // Only interval in the range after split.
          : last_interval_;  // Last interval of the original range.
  result->first_interval_ = after;
  last_interval_ = before;

  // Find the last use position before the split and the first use
  // position after it.
  UsePosition* use_after = first_pos_;
  UsePosition* use_before = NULL;
  if (split_at_start) {
    // The split position coincides with the beginning of a use interval (the
    // end of a lifetime hole). Use at this position should be attributed to
    // the split child because split child owns use interval covering it.
    while (use_after != NULL && use_after->pos().Value() < position.Value()) {
      use_before = use_after;
      use_after = use_after->next();
    }
  } else {
    while (use_after != NULL && use_after->pos().Value() <= position.Value()) {
      use_before = use_after;
      use_after = use_after->next();
    }
  }

  // Partition original use positions to the two live ranges.
  if (use_before != NULL) {
    use_before->next_ = NULL;
  } else {
    first_pos_ = NULL;
  }
  result->first_pos_ = use_after;

  // Discard cached iteration state. It might be pointing
  // to the use that no longer belongs to this live range.
  last_processed_use_ = NULL;
  current_interval_ = NULL;

  // Link the new live range in the chain before any of the other
  // ranges linked from the range before the split.
  result->parent_ = (parent_ == NULL) ? this : parent_;
  result->kind_ = result->parent_->kind_;
  result->next_ = next_;
  next_ = result;

#ifdef DEBUG
  Verify();
  result->Verify();
#endif
}


// This implements an ordering on live ranges so that they are ordered by their
// start positions.  This is needed for the correctness of the register
// allocation algorithm.  If two live ranges start at the same offset then there
// is a tie breaker based on where the value is first used.  This part of the
// ordering is merely a heuristic.
bool LiveRange::ShouldBeAllocatedBefore(const LiveRange* other) const {
  LifetimePosition start = Start();
  LifetimePosition other_start = other->Start();
  if (start.Value() == other_start.Value()) {
    UsePosition* pos = first_pos();
    if (pos == NULL) return false;
    UsePosition* other_pos = other->first_pos();
    if (other_pos == NULL) return true;
    return pos->pos().Value() < other_pos->pos().Value();
  }
  return start.Value() < other_start.Value();
}


void LiveRange::ShortenTo(LifetimePosition start) {
  RegisterAllocator::TraceAlloc("Shorten live range %d to [%d\n", id_,
                                start.Value());
  DCHECK(first_interval_ != NULL);
  DCHECK(first_interval_->start().Value() <= start.Value());
  DCHECK(start.Value() < first_interval_->end().Value());
  first_interval_->set_start(start);
}


void LiveRange::EnsureInterval(LifetimePosition start, LifetimePosition end,
                               Zone* zone) {
  RegisterAllocator::TraceAlloc("Ensure live range %d in interval [%d %d[\n",
                                id_, start.Value(), end.Value());
  LifetimePosition new_end = end;
  while (first_interval_ != NULL &&
         first_interval_->start().Value() <= end.Value()) {
    if (first_interval_->end().Value() > end.Value()) {
      new_end = first_interval_->end();
    }
    first_interval_ = first_interval_->next();
  }

  UseInterval* new_interval = new (zone) UseInterval(start, new_end);
  new_interval->next_ = first_interval_;
  first_interval_ = new_interval;
  if (new_interval->next() == NULL) {
    last_interval_ = new_interval;
  }
}


void LiveRange::AddUseInterval(LifetimePosition start, LifetimePosition end,
                               Zone* zone) {
  RegisterAllocator::TraceAlloc("Add to live range %d interval [%d %d[\n", id_,
                                start.Value(), end.Value());
  if (first_interval_ == NULL) {
    UseInterval* interval = new (zone) UseInterval(start, end);
    first_interval_ = interval;
    last_interval_ = interval;
  } else {
    if (end.Value() == first_interval_->start().Value()) {
      first_interval_->set_start(start);
    } else if (end.Value() < first_interval_->start().Value()) {
      UseInterval* interval = new (zone) UseInterval(start, end);
      interval->set_next(first_interval_);
      first_interval_ = interval;
    } else {
      // Order of instruction's processing (see ProcessInstructions) guarantees
      // that each new use interval either precedes or intersects with
      // last added interval.
      DCHECK(start.Value() < first_interval_->end().Value());
      first_interval_->start_ = Min(start, first_interval_->start_);
      first_interval_->end_ = Max(end, first_interval_->end_);
    }
  }
}


void LiveRange::AddUsePosition(LifetimePosition pos,
                               InstructionOperand* operand,
                               InstructionOperand* hint, Zone* zone) {
  RegisterAllocator::TraceAlloc("Add to live range %d use position %d\n", id_,
                                pos.Value());
  UsePosition* use_pos = new (zone) UsePosition(pos, operand, hint);
  UsePosition* prev_hint = NULL;
  UsePosition* prev = NULL;
  UsePosition* current = first_pos_;
  while (current != NULL && current->pos().Value() < pos.Value()) {
    prev_hint = current->HasHint() ? current : prev_hint;
    prev = current;
    current = current->next();
  }

  if (prev == NULL) {
    use_pos->set_next(first_pos_);
    first_pos_ = use_pos;
  } else {
    use_pos->next_ = prev->next_;
    prev->next_ = use_pos;
  }

  if (prev_hint == NULL && use_pos->HasHint()) {
    current_hint_operand_ = hint;
  }
}


void LiveRange::ConvertOperands(Zone* zone) {
  InstructionOperand* op = CreateAssignedOperand(zone);
  UsePosition* use_pos = first_pos();
  while (use_pos != NULL) {
    DCHECK(Start().Value() <= use_pos->pos().Value() &&
           use_pos->pos().Value() <= End().Value());

    if (use_pos->HasOperand()) {
      DCHECK(op->IsRegister() || op->IsDoubleRegister() ||
             !use_pos->RequiresRegister());
      use_pos->operand()->ConvertTo(op->kind(), op->index());
    }
    use_pos = use_pos->next();
  }
}


bool LiveRange::CanCover(LifetimePosition position) const {
  if (IsEmpty()) return false;
  return Start().Value() <= position.Value() &&
         position.Value() < End().Value();
}


bool LiveRange::Covers(LifetimePosition position) {
  if (!CanCover(position)) return false;
  UseInterval* start_search = FirstSearchIntervalForPosition(position);
  for (UseInterval* interval = start_search; interval != NULL;
       interval = interval->next()) {
    DCHECK(interval->next() == NULL ||
           interval->next()->start().Value() >= interval->start().Value());
    AdvanceLastProcessedMarker(interval, position);
    if (interval->Contains(position)) return true;
    if (interval->start().Value() > position.Value()) return false;
  }
  return false;
}


LifetimePosition LiveRange::FirstIntersection(LiveRange* other) {
  UseInterval* b = other->first_interval();
  if (b == NULL) return LifetimePosition::Invalid();
  LifetimePosition advance_last_processed_up_to = b->start();
  UseInterval* a = FirstSearchIntervalForPosition(b->start());
  while (a != NULL && b != NULL) {
    if (a->start().Value() > other->End().Value()) break;
    if (b->start().Value() > End().Value()) break;
    LifetimePosition cur_intersection = a->Intersect(b);
    if (cur_intersection.IsValid()) {
      return cur_intersection;
    }
    if (a->start().Value() < b->start().Value()) {
      a = a->next();
      if (a == NULL || a->start().Value() > other->End().Value()) break;
      AdvanceLastProcessedMarker(a, advance_last_processed_up_to);
    } else {
      b = b->next();
    }
  }
  return LifetimePosition::Invalid();
}


RegisterAllocator::RegisterAllocator(InstructionSequence* code)
    : zone_(code->isolate()),
      code_(code),
      live_in_sets_(code->BasicBlockCount(), zone()),
      live_ranges_(code->VirtualRegisterCount() * 2, zone()),
      fixed_live_ranges_(NULL),
      fixed_double_live_ranges_(NULL),
      unhandled_live_ranges_(code->VirtualRegisterCount() * 2, zone()),
      active_live_ranges_(8, zone()),
      inactive_live_ranges_(8, zone()),
      reusable_slots_(8, zone()),
      mode_(UNALLOCATED_REGISTERS),
      num_registers_(-1),
      allocation_ok_(true) {}


void RegisterAllocator::InitializeLivenessAnalysis() {
  // Initialize the live_in sets for each block to NULL.
  int block_count = code()->BasicBlockCount();
  live_in_sets_.Initialize(block_count, zone());
  live_in_sets_.AddBlock(NULL, block_count, zone());
}


BitVector* RegisterAllocator::ComputeLiveOut(BasicBlock* block) {
  // Compute live out for the given block, except not including backward
  // successor edges.
  BitVector* live_out =
      new (zone()) BitVector(code()->VirtualRegisterCount(), zone());

  // Process all successor blocks.
  BasicBlock::Successors successors = block->successors();
  for (BasicBlock::Successors::iterator i = successors.begin();
       i != successors.end(); ++i) {
    // Add values live on entry to the successor. Note the successor's
    // live_in will not be computed yet for backwards edges.
    BasicBlock* successor = *i;
    BitVector* live_in = live_in_sets_[successor->rpo_number_];
    if (live_in != NULL) live_out->Union(*live_in);

    // All phi input operands corresponding to this successor edge are live
    // out from this block.
    int index = successor->PredecessorIndexOf(block);
    DCHECK(index >= 0);
    DCHECK(index < static_cast<int>(successor->PredecessorCount()));
    for (BasicBlock::const_iterator j = successor->begin();
         j != successor->end(); ++j) {
      Node* phi = *j;
      if (phi->opcode() != IrOpcode::kPhi) continue;
      Node* input = phi->InputAt(index);
      live_out->Add(input->id());
    }
  }

  return live_out;
}


void RegisterAllocator::AddInitialIntervals(BasicBlock* block,
                                            BitVector* live_out) {
  // Add an interval that includes the entire block to the live range for
  // each live_out value.
  LifetimePosition start =
      LifetimePosition::FromInstructionIndex(block->first_instruction_index());
  LifetimePosition end = LifetimePosition::FromInstructionIndex(
                             block->last_instruction_index()).NextInstruction();
  BitVector::Iterator iterator(live_out);
  while (!iterator.Done()) {
    int operand_index = iterator.Current();
    LiveRange* range = LiveRangeFor(operand_index);
    range->AddUseInterval(start, end, zone());
    iterator.Advance();
  }
}


int RegisterAllocator::FixedDoubleLiveRangeID(int index) {
  return -index - 1 - Register::kMaxNumAllocatableRegisters;
}


InstructionOperand* RegisterAllocator::AllocateFixed(
    UnallocatedOperand* operand, int pos, bool is_tagged) {
  TraceAlloc("Allocating fixed reg for op %d\n", operand->virtual_register());
  DCHECK(operand->HasFixedPolicy());
  if (operand->HasFixedSlotPolicy()) {
    operand->ConvertTo(InstructionOperand::STACK_SLOT,
                       operand->fixed_slot_index());
  } else if (operand->HasFixedRegisterPolicy()) {
    int reg_index = operand->fixed_register_index();
    operand->ConvertTo(InstructionOperand::REGISTER, reg_index);
  } else if (operand->HasFixedDoubleRegisterPolicy()) {
    int reg_index = operand->fixed_register_index();
    operand->ConvertTo(InstructionOperand::DOUBLE_REGISTER, reg_index);
  } else {
    UNREACHABLE();
  }
  if (is_tagged) {
    TraceAlloc("Fixed reg is tagged at %d\n", pos);
    Instruction* instr = InstructionAt(pos);
    if (instr->HasPointerMap()) {
      instr->pointer_map()->RecordPointer(operand, code_zone());
    }
  }
  return operand;
}


LiveRange* RegisterAllocator::FixedLiveRangeFor(int index) {
  DCHECK(index < Register::kMaxNumAllocatableRegisters);
  LiveRange* result = fixed_live_ranges_[index];
  if (result == NULL) {
    // TODO(titzer): add a utility method to allocate a new LiveRange:
    // The LiveRange object itself can go in this zone, but the
    // InstructionOperand needs
    // to go in the code zone, since it may survive register allocation.
    result = new (zone()) LiveRange(FixedLiveRangeID(index), code_zone());
    DCHECK(result->IsFixed());
    result->kind_ = GENERAL_REGISTERS;
    SetLiveRangeAssignedRegister(result, index);
    fixed_live_ranges_[index] = result;
  }
  return result;
}


LiveRange* RegisterAllocator::FixedDoubleLiveRangeFor(int index) {
  DCHECK(index < DoubleRegister::NumAllocatableAliasedRegisters());
  LiveRange* result = fixed_double_live_ranges_[index];
  if (result == NULL) {
    result = new (zone()) LiveRange(FixedDoubleLiveRangeID(index), code_zone());
    DCHECK(result->IsFixed());
    result->kind_ = DOUBLE_REGISTERS;
    SetLiveRangeAssignedRegister(result, index);
    fixed_double_live_ranges_[index] = result;
  }
  return result;
}


LiveRange* RegisterAllocator::LiveRangeFor(int index) {
  if (index >= live_ranges_.length()) {
    live_ranges_.AddBlock(NULL, index - live_ranges_.length() + 1, zone());
  }
  LiveRange* result = live_ranges_[index];
  if (result == NULL) {
    result = new (zone()) LiveRange(index, code_zone());
    live_ranges_[index] = result;
  }
  return result;
}


GapInstruction* RegisterAllocator::GetLastGap(BasicBlock* block) {
  int last_instruction = block->last_instruction_index();
  return code()->GapAt(last_instruction - 1);
}


LiveRange* RegisterAllocator::LiveRangeFor(InstructionOperand* operand) {
  if (operand->IsUnallocated()) {
    return LiveRangeFor(UnallocatedOperand::cast(operand)->virtual_register());
  } else if (operand->IsRegister()) {
    return FixedLiveRangeFor(operand->index());
  } else if (operand->IsDoubleRegister()) {
    return FixedDoubleLiveRangeFor(operand->index());
  } else {
    return NULL;
  }
}


void RegisterAllocator::Define(LifetimePosition position,
                               InstructionOperand* operand,
                               InstructionOperand* hint) {
  LiveRange* range = LiveRangeFor(operand);
  if (range == NULL) return;

  if (range->IsEmpty() || range->Start().Value() > position.Value()) {
    // Can happen if there is a definition without use.
    range->AddUseInterval(position, position.NextInstruction(), zone());
    range->AddUsePosition(position.NextInstruction(), NULL, NULL, zone());
  } else {
    range->ShortenTo(position);
  }

  if (operand->IsUnallocated()) {
    UnallocatedOperand* unalloc_operand = UnallocatedOperand::cast(operand);
    range->AddUsePosition(position, unalloc_operand, hint, zone());
  }
}


void RegisterAllocator::Use(LifetimePosition block_start,
                            LifetimePosition position,
                            InstructionOperand* operand,
                            InstructionOperand* hint) {
  LiveRange* range = LiveRangeFor(operand);
  if (range == NULL) return;
  if (operand->IsUnallocated()) {
    UnallocatedOperand* unalloc_operand = UnallocatedOperand::cast(operand);
    range->AddUsePosition(position, unalloc_operand, hint, zone());
  }
  range->AddUseInterval(block_start, position, zone());
}


void RegisterAllocator::AddConstraintsGapMove(int index,
                                              InstructionOperand* from,
                                              InstructionOperand* to) {
  GapInstruction* gap = code()->GapAt(index);
  ParallelMove* move =
      gap->GetOrCreateParallelMove(GapInstruction::START, code_zone());
  if (from->IsUnallocated()) {
    const ZoneList<MoveOperands>* move_operands = move->move_operands();
    for (int i = 0; i < move_operands->length(); ++i) {
      MoveOperands cur = move_operands->at(i);
      InstructionOperand* cur_to = cur.destination();
      if (cur_to->IsUnallocated()) {
        if (UnallocatedOperand::cast(cur_to)->virtual_register() ==
            UnallocatedOperand::cast(from)->virtual_register()) {
          move->AddMove(cur.source(), to, code_zone());
          return;
        }
      }
    }
  }
  move->AddMove(from, to, code_zone());
}


void RegisterAllocator::MeetRegisterConstraints(BasicBlock* block) {
  int start = block->first_instruction_index();
  int end = block->last_instruction_index();
  DCHECK_NE(-1, start);
  for (int i = start; i <= end; ++i) {
    if (code()->IsGapAt(i)) {
      Instruction* instr = NULL;
      Instruction* prev_instr = NULL;
      if (i < end) instr = InstructionAt(i + 1);
      if (i > start) prev_instr = InstructionAt(i - 1);
      MeetConstraintsBetween(prev_instr, instr, i);
      if (!AllocationOk()) return;
    }
  }

  // Meet register constraints for the instruction in the end.
  if (!code()->IsGapAt(end)) {
    MeetRegisterConstraintsForLastInstructionInBlock(block);
  }
}


void RegisterAllocator::MeetRegisterConstraintsForLastInstructionInBlock(
    BasicBlock* block) {
  int end = block->last_instruction_index();
  Instruction* last_instruction = InstructionAt(end);
  for (size_t i = 0; i < last_instruction->OutputCount(); i++) {
    InstructionOperand* output_operand = last_instruction->OutputAt(i);
    DCHECK(!output_operand->IsConstant());
    UnallocatedOperand* output = UnallocatedOperand::cast(output_operand);
    int output_vreg = output->virtual_register();
    LiveRange* range = LiveRangeFor(output_vreg);
    bool assigned = false;
    if (output->HasFixedPolicy()) {
      AllocateFixed(output, -1, false);
      // This value is produced on the stack, we never need to spill it.
      if (output->IsStackSlot()) {
        range->SetSpillOperand(output);
        range->SetSpillStartIndex(end);
        assigned = true;
      }

      BasicBlock::Successors successors = block->successors();
      for (BasicBlock::Successors::iterator succ = successors.begin();
           succ != successors.end(); ++succ) {
        DCHECK((*succ)->PredecessorCount() == 1);
        int gap_index = (*succ)->first_instruction_index() + 1;
        DCHECK(code()->IsGapAt(gap_index));

        // Create an unconstrained operand for the same virtual register
        // and insert a gap move from the fixed output to the operand.
        UnallocatedOperand* output_copy =
            new (code_zone()) UnallocatedOperand(UnallocatedOperand::ANY);
        output_copy->set_virtual_register(output_vreg);

        code()->AddGapMove(gap_index, output, output_copy);
      }
    }

    if (!assigned) {
      BasicBlock::Successors successors = block->successors();
      for (BasicBlock::Successors::iterator succ = successors.begin();
           succ != successors.end(); ++succ) {
        DCHECK((*succ)->PredecessorCount() == 1);
        int gap_index = (*succ)->first_instruction_index() + 1;
        range->SetSpillStartIndex(gap_index);

        // This move to spill operand is not a real use. Liveness analysis
        // and splitting of live ranges do not account for it.
        // Thus it should be inserted to a lifetime position corresponding to
        // the instruction end.
        GapInstruction* gap = code()->GapAt(gap_index);
        ParallelMove* move =
            gap->GetOrCreateParallelMove(GapInstruction::BEFORE, code_zone());
        move->AddMove(output, range->GetSpillOperand(), code_zone());
      }
    }
  }
}


void RegisterAllocator::MeetConstraintsBetween(Instruction* first,
                                               Instruction* second,
                                               int gap_index) {
  if (first != NULL) {
    // Handle fixed temporaries.
    for (size_t i = 0; i < first->TempCount(); i++) {
      UnallocatedOperand* temp = UnallocatedOperand::cast(first->TempAt(i));
      if (temp->HasFixedPolicy()) {
        AllocateFixed(temp, gap_index - 1, false);
      }
    }

    // Handle constant/fixed output operands.
    for (size_t i = 0; i < first->OutputCount(); i++) {
      InstructionOperand* output = first->OutputAt(i);
      if (output->IsConstant()) {
        int output_vreg = output->index();
        LiveRange* range = LiveRangeFor(output_vreg);
        range->SetSpillStartIndex(gap_index - 1);
        range->SetSpillOperand(output);
      } else {
        UnallocatedOperand* first_output = UnallocatedOperand::cast(output);
        LiveRange* range = LiveRangeFor(first_output->virtual_register());
        bool assigned = false;
        if (first_output->HasFixedPolicy()) {
          UnallocatedOperand* output_copy =
              first_output->CopyUnconstrained(code_zone());
          bool is_tagged = HasTaggedValue(first_output->virtual_register());
          AllocateFixed(first_output, gap_index, is_tagged);

          // This value is produced on the stack, we never need to spill it.
          if (first_output->IsStackSlot()) {
            range->SetSpillOperand(first_output);
            range->SetSpillStartIndex(gap_index - 1);
            assigned = true;
          }
          code()->AddGapMove(gap_index, first_output, output_copy);
        }

        // Make sure we add a gap move for spilling (if we have not done
        // so already).
        if (!assigned) {
          range->SetSpillStartIndex(gap_index);

          // This move to spill operand is not a real use. Liveness analysis
          // and splitting of live ranges do not account for it.
          // Thus it should be inserted to a lifetime position corresponding to
          // the instruction end.
          GapInstruction* gap = code()->GapAt(gap_index);
          ParallelMove* move =
              gap->GetOrCreateParallelMove(GapInstruction::BEFORE, code_zone());
          move->AddMove(first_output, range->GetSpillOperand(), code_zone());
        }
      }
    }
  }

  if (second != NULL) {
    // Handle fixed input operands of second instruction.
    for (size_t i = 0; i < second->InputCount(); i++) {
      InstructionOperand* input = second->InputAt(i);
      if (input->IsImmediate()) continue;  // Ignore immediates.
      UnallocatedOperand* cur_input = UnallocatedOperand::cast(input);
      if (cur_input->HasFixedPolicy()) {
        UnallocatedOperand* input_copy =
            cur_input->CopyUnconstrained(code_zone());
        bool is_tagged = HasTaggedValue(cur_input->virtual_register());
        AllocateFixed(cur_input, gap_index + 1, is_tagged);
        AddConstraintsGapMove(gap_index, input_copy, cur_input);
      }
    }

    // Handle "output same as input" for second instruction.
    for (size_t i = 0; i < second->OutputCount(); i++) {
      InstructionOperand* output = second->OutputAt(i);
      if (!output->IsUnallocated()) continue;
      UnallocatedOperand* second_output = UnallocatedOperand::cast(output);
      if (second_output->HasSameAsInputPolicy()) {
        DCHECK(i == 0);  // Only valid for first output.
        UnallocatedOperand* cur_input =
            UnallocatedOperand::cast(second->InputAt(0));
        int output_vreg = second_output->virtual_register();
        int input_vreg = cur_input->virtual_register();

        UnallocatedOperand* input_copy =
            cur_input->CopyUnconstrained(code_zone());
        cur_input->set_virtual_register(second_output->virtual_register());
        AddConstraintsGapMove(gap_index, input_copy, cur_input);

        if (HasTaggedValue(input_vreg) && !HasTaggedValue(output_vreg)) {
          int index = gap_index + 1;
          Instruction* instr = InstructionAt(index);
          if (instr->HasPointerMap()) {
            instr->pointer_map()->RecordPointer(input_copy, code_zone());
          }
        } else if (!HasTaggedValue(input_vreg) && HasTaggedValue(output_vreg)) {
          // The input is assumed to immediately have a tagged representation,
          // before the pointer map can be used. I.e. the pointer map at the
          // instruction will include the output operand (whose value at the
          // beginning of the instruction is equal to the input operand). If
          // this is not desired, then the pointer map at this instruction needs
          // to be adjusted manually.
        }
      }
    }
  }
}


bool RegisterAllocator::IsOutputRegisterOf(Instruction* instr, int index) {
  for (size_t i = 0; i < instr->OutputCount(); i++) {
    InstructionOperand* output = instr->OutputAt(i);
    if (output->IsRegister() && output->index() == index) return true;
  }
  return false;
}


bool RegisterAllocator::IsOutputDoubleRegisterOf(Instruction* instr,
                                                 int index) {
  for (size_t i = 0; i < instr->OutputCount(); i++) {
    InstructionOperand* output = instr->OutputAt(i);
    if (output->IsDoubleRegister() && output->index() == index) return true;
  }
  return false;
}


void RegisterAllocator::ProcessInstructions(BasicBlock* block,
                                            BitVector* live) {
  int block_start = block->first_instruction_index();

  LifetimePosition block_start_position =
      LifetimePosition::FromInstructionIndex(block_start);

  for (int index = block->last_instruction_index(); index >= block_start;
       index--) {
    LifetimePosition curr_position =
        LifetimePosition::FromInstructionIndex(index);

    Instruction* instr = InstructionAt(index);
    DCHECK(instr != NULL);
    if (instr->IsGapMoves()) {
      // Process the moves of the gap instruction, making their sources live.
      GapInstruction* gap = code()->GapAt(index);

      // TODO(titzer): no need to create the parallel move if it doesn't exist.
      ParallelMove* move =
          gap->GetOrCreateParallelMove(GapInstruction::START, code_zone());
      const ZoneList<MoveOperands>* move_operands = move->move_operands();
      for (int i = 0; i < move_operands->length(); ++i) {
        MoveOperands* cur = &move_operands->at(i);
        if (cur->IsIgnored()) continue;
        InstructionOperand* from = cur->source();
        InstructionOperand* to = cur->destination();
        InstructionOperand* hint = to;
        if (to->IsUnallocated()) {
          int to_vreg = UnallocatedOperand::cast(to)->virtual_register();
          LiveRange* to_range = LiveRangeFor(to_vreg);
          if (to_range->is_phi()) {
            if (to_range->is_non_loop_phi()) {
              hint = to_range->current_hint_operand();
            }
          } else {
            if (live->Contains(to_vreg)) {
              Define(curr_position, to, from);
              live->Remove(to_vreg);
            } else {
              cur->Eliminate();
              continue;
            }
          }
        } else {
          Define(curr_position, to, from);
        }
        Use(block_start_position, curr_position, from, hint);
        if (from->IsUnallocated()) {
          live->Add(UnallocatedOperand::cast(from)->virtual_register());
        }
      }
    } else {
      // Process output, inputs, and temps of this non-gap instruction.
      for (size_t i = 0; i < instr->OutputCount(); i++) {
        InstructionOperand* output = instr->OutputAt(i);
        if (output->IsUnallocated()) {
          int out_vreg = UnallocatedOperand::cast(output)->virtual_register();
          live->Remove(out_vreg);
        } else if (output->IsConstant()) {
          int out_vreg = output->index();
          live->Remove(out_vreg);
        }
        Define(curr_position, output, NULL);
      }

      if (instr->ClobbersRegisters()) {
        for (int i = 0; i < Register::kMaxNumAllocatableRegisters; ++i) {
          if (!IsOutputRegisterOf(instr, i)) {
            LiveRange* range = FixedLiveRangeFor(i);
            range->AddUseInterval(curr_position, curr_position.InstructionEnd(),
                                  zone());
          }
        }
      }

      if (instr->ClobbersDoubleRegisters()) {
        for (int i = 0; i < DoubleRegister::NumAllocatableAliasedRegisters();
             ++i) {
          if (!IsOutputDoubleRegisterOf(instr, i)) {
            LiveRange* range = FixedDoubleLiveRangeFor(i);
            range->AddUseInterval(curr_position, curr_position.InstructionEnd(),
                                  zone());
          }
        }
      }

      for (size_t i = 0; i < instr->InputCount(); i++) {
        InstructionOperand* input = instr->InputAt(i);
        if (input->IsImmediate()) continue;  // Ignore immediates.
        LifetimePosition use_pos;
        if (input->IsUnallocated() &&
            UnallocatedOperand::cast(input)->IsUsedAtStart()) {
          use_pos = curr_position;
        } else {
          use_pos = curr_position.InstructionEnd();
        }

        Use(block_start_position, use_pos, input, NULL);
        if (input->IsUnallocated()) {
          live->Add(UnallocatedOperand::cast(input)->virtual_register());
        }
      }

      for (size_t i = 0; i < instr->TempCount(); i++) {
        InstructionOperand* temp = instr->TempAt(i);
        if (instr->ClobbersTemps()) {
          if (temp->IsRegister()) continue;
          if (temp->IsUnallocated()) {
            UnallocatedOperand* temp_unalloc = UnallocatedOperand::cast(temp);
            if (temp_unalloc->HasFixedPolicy()) {
              continue;
            }
          }
        }
        Use(block_start_position, curr_position.InstructionEnd(), temp, NULL);
        Define(curr_position, temp, NULL);
      }
    }
  }
}


void RegisterAllocator::ResolvePhis(BasicBlock* block) {
  for (BasicBlock::const_iterator i = block->begin(); i != block->end(); ++i) {
    Node* phi = *i;
    if (phi->opcode() != IrOpcode::kPhi) continue;

    UnallocatedOperand* phi_operand =
        new (code_zone()) UnallocatedOperand(UnallocatedOperand::NONE);
    phi_operand->set_virtual_register(phi->id());

    int j = 0;
    Node::Inputs inputs = phi->inputs();
    for (Node::Inputs::iterator iter(inputs.begin()); iter != inputs.end();
         ++iter, ++j) {
      Node* op = *iter;
      // TODO(mstarzinger): Use a ValueInputIterator instead.
      if (j >= block->PredecessorCount()) continue;
      UnallocatedOperand* operand =
          new (code_zone()) UnallocatedOperand(UnallocatedOperand::ANY);
      operand->set_virtual_register(op->id());
      BasicBlock* cur_block = block->PredecessorAt(j);
      // The gap move must be added without any special processing as in
      // the AddConstraintsGapMove.
      code()->AddGapMove(cur_block->last_instruction_index() - 1, operand,
                         phi_operand);

      Instruction* branch = InstructionAt(cur_block->last_instruction_index());
      DCHECK(!branch->HasPointerMap());
      USE(branch);
    }

    LiveRange* live_range = LiveRangeFor(phi->id());
    BlockStartInstruction* block_start = code()->GetBlockStart(block);
    block_start->GetOrCreateParallelMove(GapInstruction::START, code_zone())
        ->AddMove(phi_operand, live_range->GetSpillOperand(), code_zone());
    live_range->SetSpillStartIndex(block->first_instruction_index());

    // We use the phi-ness of some nodes in some later heuristics.
    live_range->set_is_phi(true);
    if (!block->IsLoopHeader()) {
      live_range->set_is_non_loop_phi(true);
    }
  }
}


bool RegisterAllocator::Allocate() {
  assigned_registers_ = new (code_zone())
      BitVector(Register::NumAllocatableRegisters(), code_zone());
  assigned_double_registers_ = new (code_zone())
      BitVector(DoubleRegister::NumAllocatableAliasedRegisters(), code_zone());
  MeetRegisterConstraints();
  if (!AllocationOk()) return false;
  ResolvePhis();
  BuildLiveRanges();
  AllocateGeneralRegisters();
  if (!AllocationOk()) return false;
  AllocateDoubleRegisters();
  if (!AllocationOk()) return false;
  PopulatePointerMaps();
  ConnectRanges();
  ResolveControlFlow();
  code()->frame()->SetAllocatedRegisters(assigned_registers_);
  code()->frame()->SetAllocatedDoubleRegisters(assigned_double_registers_);
  return true;
}


void RegisterAllocator::MeetRegisterConstraints() {
  RegisterAllocatorPhase phase("L_Register constraints", this);
  for (int i = 0; i < code()->BasicBlockCount(); ++i) {
    MeetRegisterConstraints(code()->BlockAt(i));
    if (!AllocationOk()) return;
  }
}


void RegisterAllocator::ResolvePhis() {
  RegisterAllocatorPhase phase("L_Resolve phis", this);

  // Process the blocks in reverse order.
  for (int i = code()->BasicBlockCount() - 1; i >= 0; --i) {
    ResolvePhis(code()->BlockAt(i));
  }
}


void RegisterAllocator::ResolveControlFlow(LiveRange* range, BasicBlock* block,
                                           BasicBlock* pred) {
  LifetimePosition pred_end =
      LifetimePosition::FromInstructionIndex(pred->last_instruction_index());
  LifetimePosition cur_start =
      LifetimePosition::FromInstructionIndex(block->first_instruction_index());
  LiveRange* pred_cover = NULL;
  LiveRange* cur_cover = NULL;
  LiveRange* cur_range = range;
  while (cur_range != NULL && (cur_cover == NULL || pred_cover == NULL)) {
    if (cur_range->CanCover(cur_start)) {
      DCHECK(cur_cover == NULL);
      cur_cover = cur_range;
    }
    if (cur_range->CanCover(pred_end)) {
      DCHECK(pred_cover == NULL);
      pred_cover = cur_range;
    }
    cur_range = cur_range->next();
  }

  if (cur_cover->IsSpilled()) return;
  DCHECK(pred_cover != NULL && cur_cover != NULL);
  if (pred_cover != cur_cover) {
    InstructionOperand* pred_op =
        pred_cover->CreateAssignedOperand(code_zone());
    InstructionOperand* cur_op = cur_cover->CreateAssignedOperand(code_zone());
    if (!pred_op->Equals(cur_op)) {
      GapInstruction* gap = NULL;
      if (block->PredecessorCount() == 1) {
        gap = code()->GapAt(block->first_instruction_index());
      } else {
        DCHECK(pred->SuccessorCount() == 1);
        gap = GetLastGap(pred);

        Instruction* branch = InstructionAt(pred->last_instruction_index());
        DCHECK(!branch->HasPointerMap());
        USE(branch);
      }
      gap->GetOrCreateParallelMove(GapInstruction::START, code_zone())
          ->AddMove(pred_op, cur_op, code_zone());
    }
  }
}


ParallelMove* RegisterAllocator::GetConnectingParallelMove(
    LifetimePosition pos) {
  int index = pos.InstructionIndex();
  if (code()->IsGapAt(index)) {
    GapInstruction* gap = code()->GapAt(index);
    return gap->GetOrCreateParallelMove(
        pos.IsInstructionStart() ? GapInstruction::START : GapInstruction::END,
        code_zone());
  }
  int gap_pos = pos.IsInstructionStart() ? (index - 1) : (index + 1);
  return code()->GapAt(gap_pos)->GetOrCreateParallelMove(
      (gap_pos < index) ? GapInstruction::AFTER : GapInstruction::BEFORE,
      code_zone());
}


BasicBlock* RegisterAllocator::GetBlock(LifetimePosition pos) {
  return code()->GetBasicBlock(pos.InstructionIndex());
}


void RegisterAllocator::ConnectRanges() {
  RegisterAllocatorPhase phase("L_Connect ranges", this);
  for (int i = 0; i < live_ranges()->length(); ++i) {
    LiveRange* first_range = live_ranges()->at(i);
    if (first_range == NULL || first_range->parent() != NULL) continue;

    LiveRange* second_range = first_range->next();
    while (second_range != NULL) {
      LifetimePosition pos = second_range->Start();

      if (!second_range->IsSpilled()) {
        // Add gap move if the two live ranges touch and there is no block
        // boundary.
        if (first_range->End().Value() == pos.Value()) {
          bool should_insert = true;
          if (IsBlockBoundary(pos)) {
            should_insert = CanEagerlyResolveControlFlow(GetBlock(pos));
          }
          if (should_insert) {
            ParallelMove* move = GetConnectingParallelMove(pos);
            InstructionOperand* prev_operand =
                first_range->CreateAssignedOperand(code_zone());
            InstructionOperand* cur_operand =
                second_range->CreateAssignedOperand(code_zone());
            move->AddMove(prev_operand, cur_operand, code_zone());
          }
        }
      }

      first_range = second_range;
      second_range = second_range->next();
    }
  }
}


bool RegisterAllocator::CanEagerlyResolveControlFlow(BasicBlock* block) const {
  if (block->PredecessorCount() != 1) return false;
  return block->PredecessorAt(0)->rpo_number_ == block->rpo_number_ - 1;
}


void RegisterAllocator::ResolveControlFlow() {
  RegisterAllocatorPhase phase("L_Resolve control flow", this);
  for (int block_id = 1; block_id < code()->BasicBlockCount(); ++block_id) {
    BasicBlock* block = code()->BlockAt(block_id);
    if (CanEagerlyResolveControlFlow(block)) continue;
    BitVector* live = live_in_sets_[block->rpo_number_];
    BitVector::Iterator iterator(live);
    while (!iterator.Done()) {
      int operand_index = iterator.Current();
      BasicBlock::Predecessors predecessors = block->predecessors();
      for (BasicBlock::Predecessors::iterator i = predecessors.begin();
           i != predecessors.end(); ++i) {
        BasicBlock* cur = *i;
        LiveRange* cur_range = LiveRangeFor(operand_index);
        ResolveControlFlow(cur_range, block, cur);
      }
      iterator.Advance();
    }
  }
}


void RegisterAllocator::BuildLiveRanges() {
  RegisterAllocatorPhase phase("L_Build live ranges", this);
  InitializeLivenessAnalysis();
  // Process the blocks in reverse order.
  for (int block_id = code()->BasicBlockCount() - 1; block_id >= 0;
       --block_id) {
    BasicBlock* block = code()->BlockAt(block_id);
    BitVector* live = ComputeLiveOut(block);
    // Initially consider all live_out values live for the entire block. We
    // will shorten these intervals if necessary.
    AddInitialIntervals(block, live);

    // Process the instructions in reverse order, generating and killing
    // live values.
    ProcessInstructions(block, live);
    // All phi output operands are killed by this block.
    for (BasicBlock::const_iterator i = block->begin(); i != block->end();
         ++i) {
      Node* phi = *i;
      if (phi->opcode() != IrOpcode::kPhi) continue;

      // The live range interval already ends at the first instruction of the
      // block.
      live->Remove(phi->id());

      InstructionOperand* hint = NULL;
      InstructionOperand* phi_operand = NULL;
      GapInstruction* gap = GetLastGap(block->PredecessorAt(0));

      // TODO(titzer): no need to create the parallel move if it doesn't exit.
      ParallelMove* move =
          gap->GetOrCreateParallelMove(GapInstruction::START, code_zone());
      for (int j = 0; j < move->move_operands()->length(); ++j) {
        InstructionOperand* to = move->move_operands()->at(j).destination();
        if (to->IsUnallocated() &&
            UnallocatedOperand::cast(to)->virtual_register() == phi->id()) {
          hint = move->move_operands()->at(j).source();
          phi_operand = to;
          break;
        }
      }
      DCHECK(hint != NULL);

      LifetimePosition block_start = LifetimePosition::FromInstructionIndex(
          block->first_instruction_index());
      Define(block_start, phi_operand, hint);
    }

    // Now live is live_in for this block except not including values live
    // out on backward successor edges.
    live_in_sets_[block_id] = live;

    if (block->IsLoopHeader()) {
      // Add a live range stretching from the first loop instruction to the last
      // for each value live on entry to the header.
      BitVector::Iterator iterator(live);
      LifetimePosition start = LifetimePosition::FromInstructionIndex(
          block->first_instruction_index());
      int end_index =
          code()->BlockAt(block->loop_end_)->last_instruction_index();
      LifetimePosition end =
          LifetimePosition::FromInstructionIndex(end_index).NextInstruction();
      while (!iterator.Done()) {
        int operand_index = iterator.Current();
        LiveRange* range = LiveRangeFor(operand_index);
        range->EnsureInterval(start, end, zone());
        iterator.Advance();
      }

      // Insert all values into the live in sets of all blocks in the loop.
      for (int i = block->rpo_number_ + 1; i < block->loop_end_; ++i) {
        live_in_sets_[i]->Union(*live);
      }
    }

#ifdef DEBUG
    if (block_id == 0) {
      BitVector::Iterator iterator(live);
      bool found = false;
      while (!iterator.Done()) {
        found = true;
        int operand_index = iterator.Current();
        PrintF("Register allocator error: live v%d reached first block.\n",
               operand_index);
        LiveRange* range = LiveRangeFor(operand_index);
        PrintF("  (first use is at %d)\n", range->first_pos()->pos().Value());
        CompilationInfo* info = code()->linkage()->info();
        if (info->IsStub()) {
          if (info->code_stub() == NULL) {
            PrintF("\n");
          } else {
            CodeStub::Major major_key = info->code_stub()->MajorKey();
            PrintF("  (function: %s)\n", CodeStub::MajorName(major_key, false));
          }
        } else {
          DCHECK(info->IsOptimizing());
          AllowHandleDereference allow_deref;
          PrintF("  (function: %s)\n",
                 info->function()->debug_name()->ToCString().get());
        }
        iterator.Advance();
      }
      DCHECK(!found);
    }
#endif
  }

  for (int i = 0; i < live_ranges_.length(); ++i) {
    if (live_ranges_[i] != NULL) {
      live_ranges_[i]->kind_ = RequiredRegisterKind(live_ranges_[i]->id());

      // TODO(bmeurer): This is a horrible hack to make sure that for constant
      // live ranges, every use requires the constant to be in a register.
      // Without this hack, all uses with "any" policy would get the constant
      // operand assigned.
      LiveRange* range = live_ranges_[i];
      if (range->HasAllocatedSpillOperand() &&
          range->GetSpillOperand()->IsConstant()) {
        for (UsePosition* pos = range->first_pos(); pos != NULL;
             pos = pos->next_) {
          pos->register_beneficial_ = true;
          pos->requires_reg_ = true;
        }
      }
    }
  }
}


bool RegisterAllocator::SafePointsAreInOrder() const {
  int safe_point = 0;
  const PointerMapDeque* pointer_maps = code()->pointer_maps();
  for (PointerMapDeque::const_iterator it = pointer_maps->begin();
       it != pointer_maps->end(); ++it) {
    PointerMap* map = *it;
    if (safe_point > map->instruction_position()) return false;
    safe_point = map->instruction_position();
  }
  return true;
}


void RegisterAllocator::PopulatePointerMaps() {
  RegisterAllocatorPhase phase("L_Populate pointer maps", this);

  DCHECK(SafePointsAreInOrder());

  // Iterate over all safe point positions and record a pointer
  // for all spilled live ranges at this point.
  int last_range_start = 0;
  const PointerMapDeque* pointer_maps = code()->pointer_maps();
  PointerMapDeque::const_iterator first_it = pointer_maps->begin();
  for (int range_idx = 0; range_idx < live_ranges()->length(); ++range_idx) {
    LiveRange* range = live_ranges()->at(range_idx);
    if (range == NULL) continue;
    // Iterate over the first parts of multi-part live ranges.
    if (range->parent() != NULL) continue;
    // Skip non-reference values.
    if (!HasTaggedValue(range->id())) continue;
    // Skip empty live ranges.
    if (range->IsEmpty()) continue;

    // Find the extent of the range and its children.
    int start = range->Start().InstructionIndex();
    int end = 0;
    for (LiveRange* cur = range; cur != NULL; cur = cur->next()) {
      LifetimePosition this_end = cur->End();
      if (this_end.InstructionIndex() > end) end = this_end.InstructionIndex();
      DCHECK(cur->Start().InstructionIndex() >= start);
    }

    // Most of the ranges are in order, but not all.  Keep an eye on when they
    // step backwards and reset the first_it so we don't miss any safe points.
    if (start < last_range_start) first_it = pointer_maps->begin();
    last_range_start = start;

    // Step across all the safe points that are before the start of this range,
    // recording how far we step in order to save doing this for the next range.
    for (; first_it != pointer_maps->end(); ++first_it) {
      PointerMap* map = *first_it;
      if (map->instruction_position() >= start) break;
    }

    // Step through the safe points to see whether they are in the range.
    for (PointerMapDeque::const_iterator it = first_it;
         it != pointer_maps->end(); ++it) {
      PointerMap* map = *it;
      int safe_point = map->instruction_position();

      // The safe points are sorted so we can stop searching here.
      if (safe_point - 1 > end) break;

      // Advance to the next active range that covers the current
      // safe point position.
      LifetimePosition safe_point_pos =
          LifetimePosition::FromInstructionIndex(safe_point);
      LiveRange* cur = range;
      while (cur != NULL && !cur->Covers(safe_point_pos)) {
        cur = cur->next();
      }
      if (cur == NULL) continue;

      // Check if the live range is spilled and the safe point is after
      // the spill position.
      if (range->HasAllocatedSpillOperand() &&
          safe_point >= range->spill_start_index() &&
          !range->GetSpillOperand()->IsConstant()) {
        TraceAlloc("Pointer for range %d (spilled at %d) at safe point %d\n",
                   range->id(), range->spill_start_index(), safe_point);
        map->RecordPointer(range->GetSpillOperand(), code_zone());
      }

      if (!cur->IsSpilled()) {
        TraceAlloc(
            "Pointer in register for range %d (start at %d) "
            "at safe point %d\n",
            cur->id(), cur->Start().Value(), safe_point);
        InstructionOperand* operand = cur->CreateAssignedOperand(code_zone());
        DCHECK(!operand->IsStackSlot());
        map->RecordPointer(operand, code_zone());
      }
    }
  }
}


void RegisterAllocator::AllocateGeneralRegisters() {
  RegisterAllocatorPhase phase("L_Allocate general registers", this);
  num_registers_ = Register::NumAllocatableRegisters();
  mode_ = GENERAL_REGISTERS;
  AllocateRegisters();
}


void RegisterAllocator::AllocateDoubleRegisters() {
  RegisterAllocatorPhase phase("L_Allocate double registers", this);
  num_registers_ = DoubleRegister::NumAllocatableAliasedRegisters();
  mode_ = DOUBLE_REGISTERS;
  AllocateRegisters();
}


void RegisterAllocator::AllocateRegisters() {
  DCHECK(unhandled_live_ranges_.is_empty());

  for (int i = 0; i < live_ranges_.length(); ++i) {
    if (live_ranges_[i] != NULL) {
      if (live_ranges_[i]->Kind() == mode_) {
        AddToUnhandledUnsorted(live_ranges_[i]);
      }
    }
  }
  SortUnhandled();
  DCHECK(UnhandledIsSorted());

  DCHECK(reusable_slots_.is_empty());
  DCHECK(active_live_ranges_.is_empty());
  DCHECK(inactive_live_ranges_.is_empty());

  if (mode_ == DOUBLE_REGISTERS) {
    for (int i = 0; i < DoubleRegister::NumAllocatableAliasedRegisters(); ++i) {
      LiveRange* current = fixed_double_live_ranges_.at(i);
      if (current != NULL) {
        AddToInactive(current);
      }
    }
  } else {
    DCHECK(mode_ == GENERAL_REGISTERS);
    for (int i = 0; i < fixed_live_ranges_.length(); ++i) {
      LiveRange* current = fixed_live_ranges_.at(i);
      if (current != NULL) {
        AddToInactive(current);
      }
    }
  }

  while (!unhandled_live_ranges_.is_empty()) {
    DCHECK(UnhandledIsSorted());
    LiveRange* current = unhandled_live_ranges_.RemoveLast();
    DCHECK(UnhandledIsSorted());
    LifetimePosition position = current->Start();
#ifdef DEBUG
    allocation_finger_ = position;
#endif
    TraceAlloc("Processing interval %d start=%d\n", current->id(),
               position.Value());

    if (current->HasAllocatedSpillOperand()) {
      TraceAlloc("Live range %d already has a spill operand\n", current->id());
      LifetimePosition next_pos = position;
      if (code()->IsGapAt(next_pos.InstructionIndex())) {
        next_pos = next_pos.NextInstruction();
      }
      UsePosition* pos = current->NextUsePositionRegisterIsBeneficial(next_pos);
      // If the range already has a spill operand and it doesn't need a
      // register immediately, split it and spill the first part of the range.
      if (pos == NULL) {
        Spill(current);
        continue;
      } else if (pos->pos().Value() >
                 current->Start().NextInstruction().Value()) {
        // Do not spill live range eagerly if use position that can benefit from
        // the register is too close to the start of live range.
        SpillBetween(current, current->Start(), pos->pos());
        if (!AllocationOk()) return;
        DCHECK(UnhandledIsSorted());
        continue;
      }
    }

    for (int i = 0; i < active_live_ranges_.length(); ++i) {
      LiveRange* cur_active = active_live_ranges_.at(i);
      if (cur_active->End().Value() <= position.Value()) {
        ActiveToHandled(cur_active);
        --i;  // The live range was removed from the list of active live ranges.
      } else if (!cur_active->Covers(position)) {
        ActiveToInactive(cur_active);
        --i;  // The live range was removed from the list of active live ranges.
      }
    }

    for (int i = 0; i < inactive_live_ranges_.length(); ++i) {
      LiveRange* cur_inactive = inactive_live_ranges_.at(i);
      if (cur_inactive->End().Value() <= position.Value()) {
        InactiveToHandled(cur_inactive);
        --i;  // Live range was removed from the list of inactive live ranges.
      } else if (cur_inactive->Covers(position)) {
        InactiveToActive(cur_inactive);
        --i;  // Live range was removed from the list of inactive live ranges.
      }
    }

    DCHECK(!current->HasRegisterAssigned() && !current->IsSpilled());

    bool result = TryAllocateFreeReg(current);
    if (!AllocationOk()) return;

    if (!result) AllocateBlockedReg(current);
    if (!AllocationOk()) return;

    if (current->HasRegisterAssigned()) {
      AddToActive(current);
    }
  }

  reusable_slots_.Rewind(0);
  active_live_ranges_.Rewind(0);
  inactive_live_ranges_.Rewind(0);
}


const char* RegisterAllocator::RegisterName(int allocation_index) {
  if (mode_ == GENERAL_REGISTERS) {
    return Register::AllocationIndexToString(allocation_index);
  } else {
    return DoubleRegister::AllocationIndexToString(allocation_index);
  }
}


void RegisterAllocator::TraceAlloc(const char* msg, ...) {
  if (FLAG_trace_alloc) {
    va_list arguments;
    va_start(arguments, msg);
    base::OS::VPrint(msg, arguments);
    va_end(arguments);
  }
}


bool RegisterAllocator::HasTaggedValue(int virtual_register) const {
  return code()->IsReference(virtual_register);
}


RegisterKind RegisterAllocator::RequiredRegisterKind(
    int virtual_register) const {
  return (code()->IsDouble(virtual_register)) ? DOUBLE_REGISTERS
                                              : GENERAL_REGISTERS;
}


void RegisterAllocator::AddToActive(LiveRange* range) {
  TraceAlloc("Add live range %d to active\n", range->id());
  active_live_ranges_.Add(range, zone());
}


void RegisterAllocator::AddToInactive(LiveRange* range) {
  TraceAlloc("Add live range %d to inactive\n", range->id());
  inactive_live_ranges_.Add(range, zone());
}


void RegisterAllocator::AddToUnhandledSorted(LiveRange* range) {
  if (range == NULL || range->IsEmpty()) return;
  DCHECK(!range->HasRegisterAssigned() && !range->IsSpilled());
  DCHECK(allocation_finger_.Value() <= range->Start().Value());
  for (int i = unhandled_live_ranges_.length() - 1; i >= 0; --i) {
    LiveRange* cur_range = unhandled_live_ranges_.at(i);
    if (range->ShouldBeAllocatedBefore(cur_range)) {
      TraceAlloc("Add live range %d to unhandled at %d\n", range->id(), i + 1);
      unhandled_live_ranges_.InsertAt(i + 1, range, zone());
      DCHECK(UnhandledIsSorted());
      return;
    }
  }
  TraceAlloc("Add live range %d to unhandled at start\n", range->id());
  unhandled_live_ranges_.InsertAt(0, range, zone());
  DCHECK(UnhandledIsSorted());
}


void RegisterAllocator::AddToUnhandledUnsorted(LiveRange* range) {
  if (range == NULL || range->IsEmpty()) return;
  DCHECK(!range->HasRegisterAssigned() && !range->IsSpilled());
  TraceAlloc("Add live range %d to unhandled unsorted at end\n", range->id());
  unhandled_live_ranges_.Add(range, zone());
}


static int UnhandledSortHelper(LiveRange* const* a, LiveRange* const* b) {
  DCHECK(!(*a)->ShouldBeAllocatedBefore(*b) ||
         !(*b)->ShouldBeAllocatedBefore(*a));
  if ((*a)->ShouldBeAllocatedBefore(*b)) return 1;
  if ((*b)->ShouldBeAllocatedBefore(*a)) return -1;
  return (*a)->id() - (*b)->id();
}


// Sort the unhandled live ranges so that the ranges to be processed first are
// at the end of the array list.  This is convenient for the register allocation
// algorithm because it is efficient to remove elements from the end.
void RegisterAllocator::SortUnhandled() {
  TraceAlloc("Sort unhandled\n");
  unhandled_live_ranges_.Sort(&UnhandledSortHelper);
}


bool RegisterAllocator::UnhandledIsSorted() {
  int len = unhandled_live_ranges_.length();
  for (int i = 1; i < len; i++) {
    LiveRange* a = unhandled_live_ranges_.at(i - 1);
    LiveRange* b = unhandled_live_ranges_.at(i);
    if (a->Start().Value() < b->Start().Value()) return false;
  }
  return true;
}


void RegisterAllocator::FreeSpillSlot(LiveRange* range) {
  // Check that we are the last range.
  if (range->next() != NULL) return;

  if (!range->TopLevel()->HasAllocatedSpillOperand()) return;

  InstructionOperand* spill_operand = range->TopLevel()->GetSpillOperand();
  if (spill_operand->IsConstant()) return;
  if (spill_operand->index() >= 0) {
    reusable_slots_.Add(range, zone());
  }
}


InstructionOperand* RegisterAllocator::TryReuseSpillSlot(LiveRange* range) {
  if (reusable_slots_.is_empty()) return NULL;
  if (reusable_slots_.first()->End().Value() >
      range->TopLevel()->Start().Value()) {
    return NULL;
  }
  InstructionOperand* result =
      reusable_slots_.first()->TopLevel()->GetSpillOperand();
  reusable_slots_.Remove(0);
  return result;
}


void RegisterAllocator::ActiveToHandled(LiveRange* range) {
  DCHECK(active_live_ranges_.Contains(range));
  active_live_ranges_.RemoveElement(range);
  TraceAlloc("Moving live range %d from active to handled\n", range->id());
  FreeSpillSlot(range);
}


void RegisterAllocator::ActiveToInactive(LiveRange* range) {
  DCHECK(active_live_ranges_.Contains(range));
  active_live_ranges_.RemoveElement(range);
  inactive_live_ranges_.Add(range, zone());
  TraceAlloc("Moving live range %d from active to inactive\n", range->id());
}


void RegisterAllocator::InactiveToHandled(LiveRange* range) {
  DCHECK(inactive_live_ranges_.Contains(range));
  inactive_live_ranges_.RemoveElement(range);
  TraceAlloc("Moving live range %d from inactive to handled\n", range->id());
  FreeSpillSlot(range);
}


void RegisterAllocator::InactiveToActive(LiveRange* range) {
  DCHECK(inactive_live_ranges_.Contains(range));
  inactive_live_ranges_.RemoveElement(range);
  active_live_ranges_.Add(range, zone());
  TraceAlloc("Moving live range %d from inactive to active\n", range->id());
}


// TryAllocateFreeReg and AllocateBlockedReg assume this
// when allocating local arrays.
STATIC_ASSERT(DoubleRegister::kMaxNumAllocatableRegisters >=
              Register::kMaxNumAllocatableRegisters);


bool RegisterAllocator::TryAllocateFreeReg(LiveRange* current) {
  LifetimePosition free_until_pos[DoubleRegister::kMaxNumAllocatableRegisters];

  for (int i = 0; i < num_registers_; i++) {
    free_until_pos[i] = LifetimePosition::MaxPosition();
  }

  for (int i = 0; i < active_live_ranges_.length(); ++i) {
    LiveRange* cur_active = active_live_ranges_.at(i);
    free_until_pos[cur_active->assigned_register()] =
        LifetimePosition::FromInstructionIndex(0);
  }

  for (int i = 0; i < inactive_live_ranges_.length(); ++i) {
    LiveRange* cur_inactive = inactive_live_ranges_.at(i);
    DCHECK(cur_inactive->End().Value() > current->Start().Value());
    LifetimePosition next_intersection =
        cur_inactive->FirstIntersection(current);
    if (!next_intersection.IsValid()) continue;
    int cur_reg = cur_inactive->assigned_register();
    free_until_pos[cur_reg] = Min(free_until_pos[cur_reg], next_intersection);
  }

  InstructionOperand* hint = current->FirstHint();
  if (hint != NULL && (hint->IsRegister() || hint->IsDoubleRegister())) {
    int register_index = hint->index();
    TraceAlloc(
        "Found reg hint %s (free until [%d) for live range %d (end %d[).\n",
        RegisterName(register_index), free_until_pos[register_index].Value(),
        current->id(), current->End().Value());

    // The desired register is free until the end of the current live range.
    if (free_until_pos[register_index].Value() >= current->End().Value()) {
      TraceAlloc("Assigning preferred reg %s to live range %d\n",
                 RegisterName(register_index), current->id());
      SetLiveRangeAssignedRegister(current, register_index);
      return true;
    }
  }

  // Find the register which stays free for the longest time.
  int reg = 0;
  for (int i = 1; i < RegisterCount(); ++i) {
    if (free_until_pos[i].Value() > free_until_pos[reg].Value()) {
      reg = i;
    }
  }

  LifetimePosition pos = free_until_pos[reg];

  if (pos.Value() <= current->Start().Value()) {
    // All registers are blocked.
    return false;
  }

  if (pos.Value() < current->End().Value()) {
    // Register reg is available at the range start but becomes blocked before
    // the range end. Split current at position where it becomes blocked.
    LiveRange* tail = SplitRangeAt(current, pos);
    if (!AllocationOk()) return false;
    AddToUnhandledSorted(tail);
  }


  // Register reg is available at the range start and is free until
  // the range end.
  DCHECK(pos.Value() >= current->End().Value());
  TraceAlloc("Assigning free reg %s to live range %d\n", RegisterName(reg),
             current->id());
  SetLiveRangeAssignedRegister(current, reg);

  return true;
}


void RegisterAllocator::AllocateBlockedReg(LiveRange* current) {
  UsePosition* register_use = current->NextRegisterPosition(current->Start());
  if (register_use == NULL) {
    // There is no use in the current live range that requires a register.
    // We can just spill it.
    Spill(current);
    return;
  }


  LifetimePosition use_pos[DoubleRegister::kMaxNumAllocatableRegisters];
  LifetimePosition block_pos[DoubleRegister::kMaxNumAllocatableRegisters];

  for (int i = 0; i < num_registers_; i++) {
    use_pos[i] = block_pos[i] = LifetimePosition::MaxPosition();
  }

  for (int i = 0; i < active_live_ranges_.length(); ++i) {
    LiveRange* range = active_live_ranges_[i];
    int cur_reg = range->assigned_register();
    if (range->IsFixed() || !range->CanBeSpilled(current->Start())) {
      block_pos[cur_reg] = use_pos[cur_reg] =
          LifetimePosition::FromInstructionIndex(0);
    } else {
      UsePosition* next_use =
          range->NextUsePositionRegisterIsBeneficial(current->Start());
      if (next_use == NULL) {
        use_pos[cur_reg] = range->End();
      } else {
        use_pos[cur_reg] = next_use->pos();
      }
    }
  }

  for (int i = 0; i < inactive_live_ranges_.length(); ++i) {
    LiveRange* range = inactive_live_ranges_.at(i);
    DCHECK(range->End().Value() > current->Start().Value());
    LifetimePosition next_intersection = range->FirstIntersection(current);
    if (!next_intersection.IsValid()) continue;
    int cur_reg = range->assigned_register();
    if (range->IsFixed()) {
      block_pos[cur_reg] = Min(block_pos[cur_reg], next_intersection);
      use_pos[cur_reg] = Min(block_pos[cur_reg], use_pos[cur_reg]);
    } else {
      use_pos[cur_reg] = Min(use_pos[cur_reg], next_intersection);
    }
  }

  int reg = 0;
  for (int i = 1; i < RegisterCount(); ++i) {
    if (use_pos[i].Value() > use_pos[reg].Value()) {
      reg = i;
    }
  }

  LifetimePosition pos = use_pos[reg];

  if (pos.Value() < register_use->pos().Value()) {
    // All registers are blocked before the first use that requires a register.
    // Spill starting part of live range up to that use.
    SpillBetween(current, current->Start(), register_use->pos());
    return;
  }

  if (block_pos[reg].Value() < current->End().Value()) {
    // Register becomes blocked before the current range end. Split before that
    // position.
    LiveRange* tail = SplitBetween(current, current->Start(),
                                   block_pos[reg].InstructionStart());
    if (!AllocationOk()) return;
    AddToUnhandledSorted(tail);
  }

  // Register reg is not blocked for the whole range.
  DCHECK(block_pos[reg].Value() >= current->End().Value());
  TraceAlloc("Assigning blocked reg %s to live range %d\n", RegisterName(reg),
             current->id());
  SetLiveRangeAssignedRegister(current, reg);

  // This register was not free. Thus we need to find and spill
  // parts of active and inactive live regions that use the same register
  // at the same lifetime positions as current.
  SplitAndSpillIntersecting(current);
}


LifetimePosition RegisterAllocator::FindOptimalSpillingPos(
    LiveRange* range, LifetimePosition pos) {
  BasicBlock* block = GetBlock(pos.InstructionStart());
  BasicBlock* loop_header =
      block->IsLoopHeader() ? block : code()->GetContainingLoop(block);

  if (loop_header == NULL) return pos;

  UsePosition* prev_use = range->PreviousUsePositionRegisterIsBeneficial(pos);

  while (loop_header != NULL) {
    // We are going to spill live range inside the loop.
    // If possible try to move spilling position backwards to loop header.
    // This will reduce number of memory moves on the back edge.
    LifetimePosition loop_start = LifetimePosition::FromInstructionIndex(
        loop_header->first_instruction_index());

    if (range->Covers(loop_start)) {
      if (prev_use == NULL || prev_use->pos().Value() < loop_start.Value()) {
        // No register beneficial use inside the loop before the pos.
        pos = loop_start;
      }
    }

    // Try hoisting out to an outer loop.
    loop_header = code()->GetContainingLoop(loop_header);
  }

  return pos;
}


void RegisterAllocator::SplitAndSpillIntersecting(LiveRange* current) {
  DCHECK(current->HasRegisterAssigned());
  int reg = current->assigned_register();
  LifetimePosition split_pos = current->Start();
  for (int i = 0; i < active_live_ranges_.length(); ++i) {
    LiveRange* range = active_live_ranges_[i];
    if (range->assigned_register() == reg) {
      UsePosition* next_pos = range->NextRegisterPosition(current->Start());
      LifetimePosition spill_pos = FindOptimalSpillingPos(range, split_pos);
      if (next_pos == NULL) {
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
      if (!AllocationOk()) return;
      ActiveToHandled(range);
      --i;
    }
  }

  for (int i = 0; i < inactive_live_ranges_.length(); ++i) {
    LiveRange* range = inactive_live_ranges_[i];
    DCHECK(range->End().Value() > current->Start().Value());
    if (range->assigned_register() == reg && !range->IsFixed()) {
      LifetimePosition next_intersection = range->FirstIntersection(current);
      if (next_intersection.IsValid()) {
        UsePosition* next_pos = range->NextRegisterPosition(current->Start());
        if (next_pos == NULL) {
          SpillAfter(range, split_pos);
        } else {
          next_intersection = Min(next_intersection, next_pos->pos());
          SpillBetween(range, split_pos, next_intersection);
        }
        if (!AllocationOk()) return;
        InactiveToHandled(range);
        --i;
      }
    }
  }
}


bool RegisterAllocator::IsBlockBoundary(LifetimePosition pos) {
  return pos.IsInstructionStart() &&
         InstructionAt(pos.InstructionIndex())->IsBlockStart();
}


LiveRange* RegisterAllocator::SplitRangeAt(LiveRange* range,
                                           LifetimePosition pos) {
  DCHECK(!range->IsFixed());
  TraceAlloc("Splitting live range %d at %d\n", range->id(), pos.Value());

  if (pos.Value() <= range->Start().Value()) return range;

  // We can't properly connect liveranges if split occured at the end
  // of control instruction.
  DCHECK(pos.IsInstructionStart() ||
         !InstructionAt(pos.InstructionIndex())->IsControl());

  int vreg = GetVirtualRegister();
  if (!AllocationOk()) return NULL;
  LiveRange* result = LiveRangeFor(vreg);
  range->SplitAt(pos, result, zone());
  return result;
}


LiveRange* RegisterAllocator::SplitBetween(LiveRange* range,
                                           LifetimePosition start,
                                           LifetimePosition end) {
  DCHECK(!range->IsFixed());
  TraceAlloc("Splitting live range %d in position between [%d, %d]\n",
             range->id(), start.Value(), end.Value());

  LifetimePosition split_pos = FindOptimalSplitPos(start, end);
  DCHECK(split_pos.Value() >= start.Value());
  return SplitRangeAt(range, split_pos);
}


LifetimePosition RegisterAllocator::FindOptimalSplitPos(LifetimePosition start,
                                                        LifetimePosition end) {
  int start_instr = start.InstructionIndex();
  int end_instr = end.InstructionIndex();
  DCHECK(start_instr <= end_instr);

  // We have no choice
  if (start_instr == end_instr) return end;

  BasicBlock* start_block = GetBlock(start);
  BasicBlock* end_block = GetBlock(end);

  if (end_block == start_block) {
    // The interval is split in the same basic block. Split at the latest
    // possible position.
    return end;
  }

  BasicBlock* block = end_block;
  // Find header of outermost loop.
  // TODO(titzer): fix redundancy below.
  while (code()->GetContainingLoop(block) != NULL &&
         code()->GetContainingLoop(block)->rpo_number_ >
             start_block->rpo_number_) {
    block = code()->GetContainingLoop(block);
  }

  // We did not find any suitable outer loop. Split at the latest possible
  // position unless end_block is a loop header itself.
  if (block == end_block && !end_block->IsLoopHeader()) return end;

  return LifetimePosition::FromInstructionIndex(
      block->first_instruction_index());
}


void RegisterAllocator::SpillAfter(LiveRange* range, LifetimePosition pos) {
  LiveRange* second_part = SplitRangeAt(range, pos);
  if (!AllocationOk()) return;
  Spill(second_part);
}


void RegisterAllocator::SpillBetween(LiveRange* range, LifetimePosition start,
                                     LifetimePosition end) {
  SpillBetweenUntil(range, start, start, end);
}


void RegisterAllocator::SpillBetweenUntil(LiveRange* range,
                                          LifetimePosition start,
                                          LifetimePosition until,
                                          LifetimePosition end) {
  CHECK(start.Value() < end.Value());
  LiveRange* second_part = SplitRangeAt(range, start);
  if (!AllocationOk()) return;

  if (second_part->Start().Value() < end.Value()) {
    // The split result intersects with [start, end[.
    // Split it at position between ]start+1, end[, spill the middle part
    // and put the rest to unhandled.
    LiveRange* third_part = SplitBetween(
        second_part, Max(second_part->Start().InstructionEnd(), until),
        end.PrevInstruction().InstructionEnd());
    if (!AllocationOk()) return;

    DCHECK(third_part != second_part);

    Spill(second_part);
    AddToUnhandledSorted(third_part);
  } else {
    // The split result does not intersect with [start, end[.
    // Nothing to spill. Just put it to unhandled as whole.
    AddToUnhandledSorted(second_part);
  }
}


void RegisterAllocator::Spill(LiveRange* range) {
  DCHECK(!range->IsSpilled());
  TraceAlloc("Spilling live range %d\n", range->id());
  LiveRange* first = range->TopLevel();

  if (!first->HasAllocatedSpillOperand()) {
    InstructionOperand* op = TryReuseSpillSlot(range);
    if (op == NULL) {
      // Allocate a new operand referring to the spill slot.
      RegisterKind kind = range->Kind();
      int index = code()->frame()->AllocateSpillSlot(kind == DOUBLE_REGISTERS);
      if (kind == DOUBLE_REGISTERS) {
        op = DoubleStackSlotOperand::Create(index, zone());
      } else {
        DCHECK(kind == GENERAL_REGISTERS);
        op = StackSlotOperand::Create(index, zone());
      }
    }
    first->SetSpillOperand(op);
  }
  range->MakeSpilled(code_zone());
}


int RegisterAllocator::RegisterCount() const { return num_registers_; }


#ifdef DEBUG


void RegisterAllocator::Verify() const {
  for (int i = 0; i < live_ranges()->length(); ++i) {
    LiveRange* current = live_ranges()->at(i);
    if (current != NULL) current->Verify();
  }
}


#endif


void RegisterAllocator::SetLiveRangeAssignedRegister(LiveRange* range,
                                                     int reg) {
  if (range->Kind() == DOUBLE_REGISTERS) {
    assigned_double_registers_->Add(reg);
  } else {
    DCHECK(range->Kind() == GENERAL_REGISTERS);
    assigned_registers_->Add(reg);
  }
  range->set_assigned_register(reg, code_zone());
}


RegisterAllocatorPhase::RegisterAllocatorPhase(const char* name,
                                               RegisterAllocator* allocator)
    : CompilationPhase(name, allocator->code()->linkage()->info()),
      allocator_(allocator) {
  if (FLAG_turbo_stats) {
    allocator_zone_start_allocation_size_ =
        allocator->zone()->allocation_size();
  }
}


RegisterAllocatorPhase::~RegisterAllocatorPhase() {
  if (FLAG_turbo_stats) {
    unsigned size = allocator_->zone()->allocation_size() -
                    allocator_zone_start_allocation_size_;
    isolate()->GetTStatistics()->SaveTiming(name(), base::TimeDelta(), size);
  }
#ifdef DEBUG
  if (allocator_ != NULL) allocator_->Verify();
#endif
}
}
}
}  // namespace v8::internal::compiler

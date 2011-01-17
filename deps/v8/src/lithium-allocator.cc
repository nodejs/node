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

#include "lithium-allocator.h"

#include "hydrogen.h"
#include "string-stream.h"

#if V8_TARGET_ARCH_IA32
#include "ia32/lithium-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/lithium-x64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/lithium-arm.h"
#else
#error "Unknown architecture."
#endif

namespace v8 {
namespace internal {


#define DEFINE_OPERAND_CACHE(name, type)            \
  name name::cache[name::kNumCachedOperands];       \
  void name::SetupCache() {                         \
    for (int i = 0; i < kNumCachedOperands; i++) {  \
      cache[i].ConvertTo(type, i);                  \
    }                                               \
  }

DEFINE_OPERAND_CACHE(LConstantOperand, CONSTANT_OPERAND)
DEFINE_OPERAND_CACHE(LStackSlot,       STACK_SLOT)
DEFINE_OPERAND_CACHE(LDoubleStackSlot, DOUBLE_STACK_SLOT)
DEFINE_OPERAND_CACHE(LRegister,        REGISTER)
DEFINE_OPERAND_CACHE(LDoubleRegister,  DOUBLE_REGISTER)

#undef DEFINE_OPERAND_CACHE


static inline LifetimePosition Min(LifetimePosition a, LifetimePosition b) {
  return a.Value() < b.Value() ? a : b;
}


static inline LifetimePosition Max(LifetimePosition a, LifetimePosition b) {
  return a.Value() > b.Value() ? a : b;
}


void LOperand::PrintTo(StringStream* stream) {
  LUnallocated* unalloc = NULL;
  switch (kind()) {
    case INVALID:
      break;
    case UNALLOCATED:
      unalloc = LUnallocated::cast(this);
      stream->Add("v%d", unalloc->virtual_register());
      switch (unalloc->policy()) {
        case LUnallocated::NONE:
          break;
        case LUnallocated::FIXED_REGISTER: {
          const char* register_name =
              Register::AllocationIndexToString(unalloc->fixed_index());
          stream->Add("(=%s)", register_name);
          break;
        }
        case LUnallocated::FIXED_DOUBLE_REGISTER: {
          const char* double_register_name =
              DoubleRegister::AllocationIndexToString(unalloc->fixed_index());
          stream->Add("(=%s)", double_register_name);
          break;
        }
        case LUnallocated::FIXED_SLOT:
          stream->Add("(=%dS)", unalloc->fixed_index());
          break;
        case LUnallocated::MUST_HAVE_REGISTER:
          stream->Add("(R)");
          break;
        case LUnallocated::WRITABLE_REGISTER:
          stream->Add("(WR)");
          break;
        case LUnallocated::SAME_AS_FIRST_INPUT:
          stream->Add("(1)");
          break;
        case LUnallocated::ANY:
          stream->Add("(-)");
          break;
        case LUnallocated::IGNORE:
          stream->Add("(0)");
          break;
      }
      break;
    case CONSTANT_OPERAND:
      stream->Add("[constant:%d]", index());
      break;
    case STACK_SLOT:
      stream->Add("[stack:%d]", index());
      break;
    case DOUBLE_STACK_SLOT:
      stream->Add("[double_stack:%d]", index());
      break;
    case REGISTER:
      stream->Add("[%s|R]", Register::AllocationIndexToString(index()));
      break;
    case DOUBLE_REGISTER:
      stream->Add("[%s|R]", DoubleRegister::AllocationIndexToString(index()));
      break;
    case ARGUMENT:
      stream->Add("[arg:%d]", index());
      break;
  }
}

int LOperand::VirtualRegister() {
  LUnallocated* unalloc = LUnallocated::cast(this);
  return unalloc->virtual_register();
}


bool UsePosition::RequiresRegister() const {
  return requires_reg_;
}


bool UsePosition::RegisterIsBeneficial() const {
  return register_beneficial_;
}


void UseInterval::SplitAt(LifetimePosition pos) {
  ASSERT(Contains(pos) && pos.Value() != start().Value());
  UseInterval* after = new UseInterval(pos, end_);
  after->next_ = next_;
  next_ = after;
  end_ = pos;
}


#ifdef DEBUG


void LiveRange::Verify() const {
  UsePosition* cur = first_pos_;
  while (cur != NULL) {
    ASSERT(Start().Value() <= cur->pos().Value() &&
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


UsePosition* LiveRange::NextRegisterPosition(LifetimePosition start) {
  UsePosition* pos = NextUsePosition(start);
  while (pos != NULL && !pos->RequiresRegister()) {
    pos = pos->next();
  }
  return pos;
}


bool LiveRange::CanBeSpilled(LifetimePosition pos) {
  // TODO(kmillikin): Comment. Now.
  if (pos.Value() <= Start().Value() && HasRegisterAssigned()) return false;

  // We cannot spill a live range that has a use requiring a register
  // at the current or the immediate next position.
  UsePosition* use_pos = NextRegisterPosition(pos);
  if (use_pos == NULL) return true;
  return use_pos->pos().Value() > pos.NextInstruction().Value();
}


UsePosition* LiveRange::FirstPosWithHint() const {
  UsePosition* pos = first_pos_;
  while (pos != NULL && !pos->HasHint()) pos = pos->next();
  return pos;
}


LOperand* LiveRange::CreateAssignedOperand() {
  LOperand* op = NULL;
  if (HasRegisterAssigned()) {
    ASSERT(!IsSpilled());
    if (IsDouble()) {
      op = LDoubleRegister::Create(assigned_register());
    } else {
      op = LRegister::Create(assigned_register());
    }
  } else if (IsSpilled()) {
    ASSERT(!HasRegisterAssigned());
    op = TopLevel()->GetSpillOperand();
    ASSERT(!op->IsUnallocated());
  } else {
    LUnallocated* unalloc = new LUnallocated(LUnallocated::NONE);
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
  LifetimePosition start =
      current_interval_ == NULL ? LifetimePosition::Invalid()
                                : current_interval_->start();
  if (to_start_of->start().Value() > start.Value()) {
    current_interval_ = to_start_of;
  }
}


void LiveRange::SplitAt(LifetimePosition position, LiveRange* result) {
  ASSERT(Start().Value() < position.Value());
  ASSERT(result->IsEmpty());
  // Find the last interval that ends before the position. If the
  // position is contained in one of the intervals in the chain, we
  // split that interval and use the first part.
  UseInterval* current = FirstSearchIntervalForPosition(position);

  // If the split position coincides with the beginning of a use interval
  // we need to split use positons in a special way.
  bool split_at_start = false;

  while (current != NULL) {
    if (current->Contains(position)) {
      current->SplitAt(position);
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
  result->last_interval_ = (last_interval_ == before)
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

  // Link the new live range in the chain before any of the other
  // ranges linked from the range before the split.
  result->parent_ = (parent_ == NULL) ? this : parent_;
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
    UsePosition* pos = FirstPosWithHint();
    if (pos == NULL) return false;
    UsePosition* other_pos = other->first_pos();
    if (other_pos == NULL) return true;
    return pos->pos().Value() < other_pos->pos().Value();
  }
  return start.Value() < other_start.Value();
}


void LiveRange::ShortenTo(LifetimePosition start) {
  LAllocator::TraceAlloc("Shorten live range %d to [%d\n", id_, start.Value());
  ASSERT(first_interval_ != NULL);
  ASSERT(first_interval_->start().Value() <= start.Value());
  ASSERT(start.Value() < first_interval_->end().Value());
  first_interval_->set_start(start);
}


void LiveRange::EnsureInterval(LifetimePosition start, LifetimePosition end) {
  LAllocator::TraceAlloc("Ensure live range %d in interval [%d %d[\n",
                         id_,
                         start.Value(),
                         end.Value());
  LifetimePosition new_end = end;
  while (first_interval_ != NULL &&
         first_interval_->start().Value() <= end.Value()) {
    if (first_interval_->end().Value() > end.Value()) {
      new_end = first_interval_->end();
    }
    first_interval_ = first_interval_->next();
  }

  UseInterval* new_interval = new UseInterval(start, new_end);
  new_interval->next_ = first_interval_;
  first_interval_ = new_interval;
  if (new_interval->next() == NULL) {
    last_interval_ = new_interval;
  }
}


void LiveRange::AddUseInterval(LifetimePosition start, LifetimePosition end) {
  LAllocator::TraceAlloc("Add to live range %d interval [%d %d[\n",
                         id_,
                         start.Value(),
                         end.Value());
  if (first_interval_ == NULL) {
    UseInterval* interval = new UseInterval(start, end);
    first_interval_ = interval;
    last_interval_ = interval;
  } else {
    if (end.Value() == first_interval_->start().Value()) {
      first_interval_->set_start(start);
    } else if (end.Value() < first_interval_->start().Value()) {
      UseInterval* interval = new UseInterval(start, end);
      interval->set_next(first_interval_);
      first_interval_ = interval;
    } else {
      // Order of instruction's processing (see ProcessInstructions) guarantees
      // that each new use interval either precedes or intersects with
      // last added interval.
      ASSERT(start.Value() < first_interval_->end().Value());
      first_interval_->start_ = Min(start, first_interval_->start_);
      first_interval_->end_ = Max(end, first_interval_->end_);
    }
  }
}


UsePosition* LiveRange::AddUsePosition(LifetimePosition pos,
                                       LOperand* operand) {
  LAllocator::TraceAlloc("Add to live range %d use position %d\n",
                         id_,
                         pos.Value());
  UsePosition* use_pos = new UsePosition(pos, operand);
  UsePosition* prev = NULL;
  UsePosition* current = first_pos_;
  while (current != NULL && current->pos().Value() < pos.Value()) {
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

  return use_pos;
}


void LiveRange::ConvertOperands() {
  LOperand* op = CreateAssignedOperand();
  UsePosition* use_pos = first_pos();
  while (use_pos != NULL) {
    ASSERT(Start().Value() <= use_pos->pos().Value() &&
           use_pos->pos().Value() <= End().Value());

    if (use_pos->HasOperand()) {
      ASSERT(op->IsRegister() || op->IsDoubleRegister() ||
             !use_pos->RequiresRegister());
      use_pos->operand()->ConvertTo(op->kind(), op->index());
    }
    use_pos = use_pos->next();
  }
}


UsePosition* LiveRange::AddUsePosition(LifetimePosition pos) {
  return AddUsePosition(pos, CreateAssignedOperand());
}


bool LiveRange::CanCover(LifetimePosition position) const {
  if (IsEmpty()) return false;
  return Start().Value() <= position.Value() &&
         position.Value() < End().Value();
}


bool LiveRange::Covers(LifetimePosition position) {
  if (!CanCover(position)) return false;
  UseInterval* start_search = FirstSearchIntervalForPosition(position);
  for (UseInterval* interval = start_search;
       interval != NULL;
       interval = interval->next()) {
    ASSERT(interval->next() == NULL ||
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


void LAllocator::InitializeLivenessAnalysis() {
  // Initialize the live_in sets for each block to NULL.
  int block_count = graph()->blocks()->length();
  live_in_sets_.Initialize(block_count);
  live_in_sets_.AddBlock(NULL, block_count);
}


BitVector* LAllocator::ComputeLiveOut(HBasicBlock* block) {
  // Compute live out for the given block, except not including backward
  // successor edges.
  BitVector* live_out = new BitVector(next_virtual_register_);

  // Process all successor blocks.
  HBasicBlock* successor = block->end()->FirstSuccessor();
  while (successor != NULL) {
    // Add values live on entry to the successor. Note the successor's
    // live_in will not be computed yet for backwards edges.
    BitVector* live_in = live_in_sets_[successor->block_id()];
    if (live_in != NULL) live_out->Union(*live_in);

    // All phi input operands corresponding to this successor edge are live
    // out from this block.
    int index = successor->PredecessorIndexOf(block);
    const ZoneList<HPhi*>* phis = successor->phis();
    for (int i = 0; i < phis->length(); ++i) {
      HPhi* phi = phis->at(i);
      if (!phi->OperandAt(index)->IsConstant()) {
        live_out->Add(phi->OperandAt(index)->id());
      }
    }

    // Check if we are done with second successor.
    if (successor == block->end()->SecondSuccessor()) break;

    successor = block->end()->SecondSuccessor();
  }

  return live_out;
}


void LAllocator::AddInitialIntervals(HBasicBlock* block,
                                     BitVector* live_out) {
  // Add an interval that includes the entire block to the live range for
  // each live_out value.
  LifetimePosition start = LifetimePosition::FromInstructionIndex(
      block->first_instruction_index());
  LifetimePosition end = LifetimePosition::FromInstructionIndex(
      block->last_instruction_index()).NextInstruction();
  BitVector::Iterator iterator(live_out);
  while (!iterator.Done()) {
    int operand_index = iterator.Current();
    LiveRange* range = LiveRangeFor(operand_index);
    range->AddUseInterval(start, end);
    iterator.Advance();
  }
}


int LAllocator::FixedDoubleLiveRangeID(int index) {
  return -index - 1 - Register::kNumAllocatableRegisters;
}


LOperand* LAllocator::AllocateFixed(LUnallocated* operand,
                                    int pos,
                                    bool is_tagged) {
  TraceAlloc("Allocating fixed reg for op %d\n", operand->virtual_register());
  ASSERT(operand->HasFixedPolicy());
  if (operand->policy() == LUnallocated::FIXED_SLOT) {
    operand->ConvertTo(LOperand::STACK_SLOT, operand->fixed_index());
  } else if (operand->policy() == LUnallocated::FIXED_REGISTER) {
    int reg_index = operand->fixed_index();
    operand->ConvertTo(LOperand::REGISTER, reg_index);
  } else if (operand->policy() == LUnallocated::FIXED_DOUBLE_REGISTER) {
    int reg_index = operand->fixed_index();
    operand->ConvertTo(LOperand::DOUBLE_REGISTER, reg_index);
  } else {
    UNREACHABLE();
  }
  if (is_tagged) {
    TraceAlloc("Fixed reg is tagged at %d\n", pos);
    LInstruction* instr = chunk_->instructions()->at(pos);
    if (instr->HasPointerMap()) {
      instr->pointer_map()->RecordPointer(operand);
    }
  }
  return operand;
}


LiveRange* LAllocator::FixedLiveRangeFor(int index) {
  if (index >= fixed_live_ranges_.length()) {
    fixed_live_ranges_.AddBlock(NULL,
                                index - fixed_live_ranges_.length() + 1);
  }

  LiveRange* result = fixed_live_ranges_[index];
  if (result == NULL) {
    result = new LiveRange(FixedLiveRangeID(index));
    ASSERT(result->IsFixed());
    result->set_assigned_register(index, GENERAL_REGISTERS);
    fixed_live_ranges_[index] = result;
  }
  return result;
}


LiveRange* LAllocator::FixedDoubleLiveRangeFor(int index) {
  if (index >= fixed_double_live_ranges_.length()) {
    fixed_double_live_ranges_.AddBlock(NULL,
                                index - fixed_double_live_ranges_.length() + 1);
  }

  LiveRange* result = fixed_double_live_ranges_[index];
  if (result == NULL) {
    result = new LiveRange(FixedDoubleLiveRangeID(index));
    ASSERT(result->IsFixed());
    result->set_assigned_register(index, DOUBLE_REGISTERS);
    fixed_double_live_ranges_[index] = result;
  }
  return result;
}

LiveRange* LAllocator::LiveRangeFor(int index) {
  if (index >= live_ranges_.length()) {
    live_ranges_.AddBlock(NULL, index - live_ranges_.length() + 1);
  }
  LiveRange* result = live_ranges_[index];
  if (result == NULL) {
    result = new LiveRange(index);
    live_ranges_[index] = result;
  }
  return result;
}


LGap* LAllocator::GetLastGap(HBasicBlock* block) const {
  int last_instruction = block->last_instruction_index();
  int index = chunk_->NearestGapPos(last_instruction);
  return chunk_->GetGapAt(index);
}


HPhi* LAllocator::LookupPhi(LOperand* operand) const {
  if (!operand->IsUnallocated()) return NULL;
  int index = operand->VirtualRegister();
  HValue* instr = graph()->LookupValue(index);
  if (instr != NULL && instr->IsPhi()) {
    return HPhi::cast(instr);
  }
  return NULL;
}


LiveRange* LAllocator::LiveRangeFor(LOperand* operand) {
  if (operand->IsUnallocated()) {
    return LiveRangeFor(LUnallocated::cast(operand)->virtual_register());
  } else if (operand->IsRegister()) {
    return FixedLiveRangeFor(operand->index());
  } else if (operand->IsDoubleRegister()) {
    return FixedDoubleLiveRangeFor(operand->index());
  } else {
    return NULL;
  }
}


void LAllocator::Define(LifetimePosition position,
                        LOperand* operand,
                        LOperand* hint) {
  LiveRange* range = LiveRangeFor(operand);
  if (range == NULL) return;

  if (range->IsEmpty() || range->Start().Value() > position.Value()) {
    // Can happen if there is a definition without use.
    range->AddUseInterval(position, position.NextInstruction());
    range->AddUsePosition(position.NextInstruction(), NULL);
  } else {
    range->ShortenTo(position);
  }

  if (operand->IsUnallocated()) {
    LUnallocated* unalloc_operand = LUnallocated::cast(operand);
    range->AddUsePosition(position, unalloc_operand)->set_hint(hint);
  }
}


void LAllocator::Use(LifetimePosition block_start,
                     LifetimePosition position,
                     LOperand* operand,
                     LOperand* hint) {
  LiveRange* range = LiveRangeFor(operand);
  if (range == NULL) return;
  if (operand->IsUnallocated()) {
    LUnallocated* unalloc_operand = LUnallocated::cast(operand);
    range->AddUsePosition(position, unalloc_operand)->set_hint(hint);
  }
  range->AddUseInterval(block_start, position);
}


void LAllocator::AddConstraintsGapMove(int index,
                                       LOperand* from,
                                       LOperand* to) {
  LGap* gap = chunk_->GetGapAt(index);
  LParallelMove* move = gap->GetOrCreateParallelMove(LGap::START);
  if (from->IsUnallocated()) {
    const ZoneList<LMoveOperands>* move_operands = move->move_operands();
    for (int i = 0; i < move_operands->length(); ++i) {
      LMoveOperands cur = move_operands->at(i);
      LOperand* cur_to = cur.to();
      if (cur_to->IsUnallocated()) {
        if (cur_to->VirtualRegister() == from->VirtualRegister()) {
          move->AddMove(cur.from(), to);
          return;
        }
      }
    }
  }
  move->AddMove(from, to);
}


void LAllocator::MeetRegisterConstraints(HBasicBlock* block) {
  int start = block->first_instruction_index();
  int end = block->last_instruction_index();
  for (int i = start; i <= end; ++i) {
    if (chunk_->IsGapAt(i)) {
      InstructionSummary* summary = NULL;
      InstructionSummary* prev_summary = NULL;
      if (i < end) summary = GetSummary(i + 1);
      if (i > start) prev_summary = GetSummary(i - 1);
      MeetConstraintsBetween(prev_summary, summary, i);
    }
  }
}


void LAllocator::MeetConstraintsBetween(InstructionSummary* first,
                                        InstructionSummary* second,
                                        int gap_index) {
  // Handle fixed temporaries.
  if (first != NULL) {
    for (int i = 0; i < first->TempCount(); ++i) {
      LUnallocated* temp = LUnallocated::cast(first->TempAt(i));
      if (temp->HasFixedPolicy()) {
        AllocateFixed(temp, gap_index - 1, false);
      }
    }
  }

  // Handle fixed output operand.
  if (first != NULL && first->Output() != NULL) {
    LUnallocated* first_output = LUnallocated::cast(first->Output());
    LiveRange* range = LiveRangeFor(first_output->VirtualRegister());
    bool assigned = false;
    if (first_output->HasFixedPolicy()) {
      LUnallocated* output_copy = first_output->CopyUnconstrained();
      bool is_tagged = HasTaggedValue(first_output->VirtualRegister());
      AllocateFixed(first_output, gap_index, is_tagged);

      // This value is produced on the stack, we never need to spill it.
      if (first_output->IsStackSlot()) {
        range->SetSpillOperand(first_output);
        range->SetSpillStartIndex(gap_index - 1);
        assigned = true;
      }
      chunk_->AddGapMove(gap_index, first_output, output_copy);
    }

    if (!assigned) {
      range->SetSpillStartIndex(gap_index);

      // This move to spill operand is not a real use. Liveness analysis
      // and splitting of live ranges do not account for it.
      // Thus it should be inserted to a lifetime position corresponding to
      // the instruction end.
      LGap* gap = chunk_->GetGapAt(gap_index);
      LParallelMove* move = gap->GetOrCreateParallelMove(LGap::BEFORE);
      move->AddMove(first_output, range->GetSpillOperand());
    }
  }

  // Handle fixed input operands of second instruction.
  if (second != NULL) {
    for (int i = 0; i < second->InputCount(); ++i) {
      LUnallocated* cur_input = LUnallocated::cast(second->InputAt(i));
      if (cur_input->HasFixedPolicy()) {
        LUnallocated* input_copy = cur_input->CopyUnconstrained();
        bool is_tagged = HasTaggedValue(cur_input->VirtualRegister());
        AllocateFixed(cur_input, gap_index + 1, is_tagged);
        AddConstraintsGapMove(gap_index, input_copy, cur_input);
      } else if (cur_input->policy() == LUnallocated::WRITABLE_REGISTER) {
        // The live range of writable input registers always goes until the end
        // of the instruction.
        ASSERT(!cur_input->IsUsedAtStart());

        LUnallocated* input_copy = cur_input->CopyUnconstrained();
        cur_input->set_virtual_register(next_virtual_register_++);

        if (RequiredRegisterKind(input_copy->virtual_register()) ==
            DOUBLE_REGISTERS) {
          double_artificial_registers_.Add(
              cur_input->virtual_register() - first_artificial_register_);
        }

        AddConstraintsGapMove(gap_index, input_copy, cur_input);
      }
    }
  }

  // Handle "output same as input" for second instruction.
  if (second != NULL && second->Output() != NULL) {
    LUnallocated* second_output = LUnallocated::cast(second->Output());
    if (second_output->HasSameAsInputPolicy()) {
      LUnallocated* cur_input = LUnallocated::cast(second->InputAt(0));
      int output_vreg = second_output->VirtualRegister();
      int input_vreg = cur_input->VirtualRegister();

      LUnallocated* input_copy = cur_input->CopyUnconstrained();
      cur_input->set_virtual_register(second_output->virtual_register());
      AddConstraintsGapMove(gap_index, input_copy, cur_input);

      if (HasTaggedValue(input_vreg) && !HasTaggedValue(output_vreg)) {
        int index = gap_index + 1;
        LInstruction* instr = chunk_->instructions()->at(index);
        if (instr->HasPointerMap()) {
          instr->pointer_map()->RecordPointer(input_copy);
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


void LAllocator::ProcessInstructions(HBasicBlock* block, BitVector* live) {
  int block_start = block->first_instruction_index();
  int index = block->last_instruction_index();

  LifetimePosition block_start_position =
      LifetimePosition::FromInstructionIndex(block_start);

  while (index >= block_start) {
    LifetimePosition curr_position =
        LifetimePosition::FromInstructionIndex(index);

    if (chunk_->IsGapAt(index)) {
      // We have a gap at this position.
      LGap* gap = chunk_->GetGapAt(index);
      LParallelMove* move = gap->GetOrCreateParallelMove(LGap::START);
      const ZoneList<LMoveOperands>* move_operands = move->move_operands();
      for (int i = 0; i < move_operands->length(); ++i) {
        LMoveOperands* cur = &move_operands->at(i);
        if (cur->IsIgnored()) continue;
        LOperand* from = cur->from();
        LOperand* to = cur->to();
        HPhi* phi = LookupPhi(to);
        LOperand* hint = to;
        if (phi != NULL) {
          // This is a phi resolving move.
          if (!phi->block()->IsLoopHeader()) {
            hint = LiveRangeFor(phi->id())->FirstHint();
          }
        } else {
          if (to->IsUnallocated()) {
            if (live->Contains(to->VirtualRegister())) {
              Define(curr_position, to, from);
              live->Remove(to->VirtualRegister());
            } else {
              cur->Eliminate();
              continue;
            }
          } else {
            Define(curr_position, to, from);
          }
        }
        Use(block_start_position, curr_position, from, hint);
        if (from->IsUnallocated()) {
          live->Add(from->VirtualRegister());
        }
      }
    } else {
      ASSERT(!chunk_->IsGapAt(index));
      InstructionSummary* summary = GetSummary(index);

      if (summary != NULL) {
        LOperand* output = summary->Output();
        if (output != NULL) {
          if (output->IsUnallocated()) live->Remove(output->VirtualRegister());
          Define(curr_position, output, NULL);
        }

        if (summary->IsCall()) {
          for (int i = 0; i < Register::kNumAllocatableRegisters; ++i) {
            if (output == NULL || !output->IsRegister() ||
                output->index() != i) {
              LiveRange* range = FixedLiveRangeFor(i);
              range->AddUseInterval(curr_position,
                                    curr_position.InstructionEnd());
            }
          }
        }

        if (summary->IsCall() || summary->IsSaveDoubles()) {
          for (int i = 0; i < DoubleRegister::kNumAllocatableRegisters; ++i) {
            if (output == NULL || !output->IsDoubleRegister() ||
                output->index() != i) {
              LiveRange* range = FixedDoubleLiveRangeFor(i);
              range->AddUseInterval(curr_position,
                                    curr_position.InstructionEnd());
            }
          }
        }

        for (int i = 0; i < summary->InputCount(); ++i) {
          LOperand* input = summary->InputAt(i);

          LifetimePosition use_pos;
          if (input->IsUnallocated() &&
              LUnallocated::cast(input)->IsUsedAtStart()) {
            use_pos = curr_position;
          } else {
            use_pos = curr_position.InstructionEnd();
          }

          Use(block_start_position, use_pos, input, NULL);
          if (input->IsUnallocated()) live->Add(input->VirtualRegister());
        }

        for (int i = 0; i < summary->TempCount(); ++i) {
          LOperand* temp = summary->TempAt(i);
          if (summary->IsCall()) {
            if (temp->IsRegister()) continue;
            if (temp->IsUnallocated()) {
              LUnallocated* temp_unalloc = LUnallocated::cast(temp);
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

    index = index - 1;
  }
}


void LAllocator::ResolvePhis(HBasicBlock* block) {
  const ZoneList<HPhi*>* phis = block->phis();
  for (int i = 0; i < phis->length(); ++i) {
    HPhi* phi = phis->at(i);
    LUnallocated* phi_operand = new LUnallocated(LUnallocated::NONE);
    phi_operand->set_virtual_register(phi->id());
    for (int j = 0; j < phi->OperandCount(); ++j) {
      HValue* op = phi->OperandAt(j);
      LOperand* operand = NULL;
      if (op->IsConstant() && op->EmitAtUses()) {
        HConstant* constant = HConstant::cast(op);
        operand = chunk_->DefineConstantOperand(constant);
      } else {
        ASSERT(!op->EmitAtUses());
        LUnallocated* unalloc = new LUnallocated(LUnallocated::NONE);
        unalloc->set_virtual_register(op->id());
        operand = unalloc;
      }
      HBasicBlock* cur_block = block->predecessors()->at(j);
      // The gap move must be added without any special processing as in
      // the AddConstraintsGapMove.
      chunk_->AddGapMove(cur_block->last_instruction_index() - 1,
                         operand,
                         phi_operand);
    }

    LiveRange* live_range = LiveRangeFor(phi->id());
    LLabel* label = chunk_->GetLabel(phi->block()->block_id());
    label->GetOrCreateParallelMove(LGap::START)->
        AddMove(phi_operand, live_range->GetSpillOperand());
    live_range->SetSpillStartIndex(phi->block()->first_instruction_index());
  }
}


void LAllocator::Allocate(LChunk* chunk) {
  ASSERT(chunk_ == NULL);
  chunk_ = chunk;
  MeetRegisterConstraints();
  ResolvePhis();
  BuildLiveRanges();
  AllocateGeneralRegisters();
  AllocateDoubleRegisters();
  PopulatePointerMaps();
  if (has_osr_entry_) ProcessOsrEntry();
  ConnectRanges();
  ResolveControlFlow();
}


void LAllocator::MeetRegisterConstraints() {
  HPhase phase("Register constraints", chunk());
  first_artificial_register_ = next_virtual_register_;
  const ZoneList<HBasicBlock*>* blocks = graph()->blocks();
  for (int i = 0; i < blocks->length(); ++i) {
    HBasicBlock* block = blocks->at(i);
    MeetRegisterConstraints(block);
  }
}


void LAllocator::ResolvePhis() {
  HPhase phase("Resolve phis", chunk());

  // Process the blocks in reverse order.
  const ZoneList<HBasicBlock*>* blocks = graph()->blocks();
  for (int block_id = blocks->length() - 1; block_id >= 0; --block_id) {
    HBasicBlock* block = blocks->at(block_id);
    ResolvePhis(block);
  }
}


void LAllocator::ResolveControlFlow(LiveRange* range,
                                    HBasicBlock* block,
                                    HBasicBlock* pred) {
  LifetimePosition pred_end =
      LifetimePosition::FromInstructionIndex(pred->last_instruction_index()).
      PrevInstruction();

  LifetimePosition cur_start =
      LifetimePosition::FromInstructionIndex(block->first_instruction_index());
  LiveRange* pred_cover = NULL;
  LiveRange* cur_cover = NULL;
  LiveRange* cur_range = range;
  while (cur_range != NULL && (cur_cover == NULL || pred_cover == NULL)) {
    if (cur_range->CanCover(cur_start)) {
      ASSERT(cur_cover == NULL);
      cur_cover = cur_range;
    }
    if (cur_range->CanCover(pred_end)) {
      ASSERT(pred_cover == NULL);
      pred_cover = cur_range;
    }
    cur_range = cur_range->next();
  }

  if (cur_cover->IsSpilled()) return;
  ASSERT(pred_cover != NULL && cur_cover != NULL);
  if (pred_cover != cur_cover) {
    LOperand* pred_op = pred_cover->CreateAssignedOperand();
    LOperand* cur_op = cur_cover->CreateAssignedOperand();
    if (!pred_op->Equals(cur_op)) {
      LGap* gap = NULL;
      if (block->predecessors()->length() == 1) {
        gap = chunk_->GetGapAt(block->first_instruction_index());
      } else {
        ASSERT(pred->end()->SecondSuccessor() == NULL);
        gap = GetLastGap(pred);
      }
      gap->GetOrCreateParallelMove(LGap::START)->AddMove(pred_op, cur_op);
    }
  }
}


LParallelMove* LAllocator::GetConnectingParallelMove(LifetimePosition pos) {
  int index = pos.InstructionIndex();
  if (chunk_->IsGapAt(index)) {
    LGap* gap = chunk_->GetGapAt(index);
    return gap->GetOrCreateParallelMove(
        pos.IsInstructionStart() ? LGap::START : LGap::END);
  }
  int gap_pos = pos.IsInstructionStart() ? (index - 1) : (index + 1);
  return chunk_->GetGapAt(gap_pos)->GetOrCreateParallelMove(
      (gap_pos < index) ? LGap::AFTER : LGap::BEFORE);
}


HBasicBlock* LAllocator::GetBlock(LifetimePosition pos) {
  LGap* gap = chunk_->GetGapAt(chunk_->NearestGapPos(pos.InstructionIndex()));
  return gap->block();
}


void LAllocator::ConnectRanges() {
  HPhase phase("Connect ranges", this);
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
            LParallelMove* move = GetConnectingParallelMove(pos);
            LOperand* prev_operand = first_range->CreateAssignedOperand();
            LOperand* cur_operand = second_range->CreateAssignedOperand();
            move->AddMove(prev_operand, cur_operand);
          }
        }
      }

      first_range = second_range;
      second_range = second_range->next();
    }
  }
}


bool LAllocator::CanEagerlyResolveControlFlow(HBasicBlock* block) const {
  if (block->predecessors()->length() != 1) return false;
  return block->predecessors()->first()->block_id() == block->block_id() - 1;
}


void LAllocator::ResolveControlFlow() {
  HPhase phase("Resolve control flow", this);
  const ZoneList<HBasicBlock*>* blocks = graph()->blocks();
  for (int block_id = 1; block_id < blocks->length(); ++block_id) {
    HBasicBlock* block = blocks->at(block_id);
    if (CanEagerlyResolveControlFlow(block)) continue;
    BitVector* live = live_in_sets_[block->block_id()];
    BitVector::Iterator iterator(live);
    while (!iterator.Done()) {
      int operand_index = iterator.Current();
      for (int i = 0; i < block->predecessors()->length(); ++i) {
        HBasicBlock* cur = block->predecessors()->at(i);
        LiveRange* cur_range = LiveRangeFor(operand_index);
        ResolveControlFlow(cur_range, block, cur);
      }
      iterator.Advance();
    }
  }
}


void LAllocator::BuildLiveRanges() {
  HPhase phase("Build live ranges", this);
  InitializeLivenessAnalysis();
  // Process the blocks in reverse order.
  const ZoneList<HBasicBlock*>* blocks = graph()->blocks();
  for (int block_id = blocks->length() - 1; block_id >= 0; --block_id) {
    HBasicBlock* block = blocks->at(block_id);
    BitVector* live = ComputeLiveOut(block);
    // Initially consider all live_out values live for the entire block. We
    // will shorten these intervals if necessary.
    AddInitialIntervals(block, live);

    // Process the instructions in reverse order, generating and killing
    // live values.
    ProcessInstructions(block, live);
    // All phi output operands are killed by this block.
    const ZoneList<HPhi*>* phis = block->phis();
    for (int i = 0; i < phis->length(); ++i) {
      // The live range interval already ends at the first instruction of the
      // block.
      HPhi* phi = phis->at(i);
      live->Remove(phi->id());

      LOperand* hint = NULL;
      LOperand* phi_operand = NULL;
      LGap* gap = GetLastGap(phi->block()->predecessors()->at(0));
      LParallelMove* move = gap->GetOrCreateParallelMove(LGap::START);
      for (int j = 0; j < move->move_operands()->length(); ++j) {
        LOperand* to = move->move_operands()->at(j).to();
        if (to->IsUnallocated() && to->VirtualRegister() == phi->id()) {
          hint = move->move_operands()->at(j).from();
          phi_operand = to;
          break;
        }
      }
      ASSERT(hint != NULL);

      LifetimePosition block_start = LifetimePosition::FromInstructionIndex(
              block->first_instruction_index());
      Define(block_start, phi_operand, hint);
    }

    // Now live is live_in for this block except not including values live
    // out on backward successor edges.
    live_in_sets_[block_id] = live;

    // If this block is a loop header go back and patch up the necessary
    // predecessor blocks.
    if (block->IsLoopHeader()) {
      // TODO(kmillikin): Need to be able to get the last block of the loop
      // in the loop information. Add a live range stretching from the first
      // loop instruction to the last for each value live on entry to the
      // header.
      HBasicBlock* back_edge = block->loop_information()->GetLastBackEdge();
      BitVector::Iterator iterator(live);
      LifetimePosition start = LifetimePosition::FromInstructionIndex(
          block->first_instruction_index());
      LifetimePosition end = LifetimePosition::FromInstructionIndex(
          back_edge->last_instruction_index());
      while (!iterator.Done()) {
        int operand_index = iterator.Current();
        LiveRange* range = LiveRangeFor(operand_index);
        range->EnsureInterval(start, end);
        iterator.Advance();
      }

      for (int i = block->block_id() + 1; i <= back_edge->block_id(); ++i) {
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
        PrintF("Function: %s\n",
               *graph()->info()->function()->debug_name()->ToCString());
        PrintF("Value %d used before first definition!\n", operand_index);
        LiveRange* range = LiveRangeFor(operand_index);
        PrintF("First use is at %d\n", range->first_pos()->pos().Value());
        iterator.Advance();
      }
      ASSERT(!found);
    }
#endif
  }
}


bool LAllocator::SafePointsAreInOrder() const {
  const ZoneList<LPointerMap*>* pointer_maps = chunk_->pointer_maps();
  int safe_point = 0;
  for (int i = 0; i < pointer_maps->length(); ++i) {
    LPointerMap* map = pointer_maps->at(i);
    if (safe_point > map->lithium_position()) return false;
    safe_point = map->lithium_position();
  }
  return true;
}


void LAllocator::PopulatePointerMaps() {
  HPhase phase("Populate pointer maps", this);
  const ZoneList<LPointerMap*>* pointer_maps = chunk_->pointer_maps();

  ASSERT(SafePointsAreInOrder());

  // Iterate over all safe point positions and record a pointer
  // for all spilled live ranges at this point.
  int first_safe_point_index = 0;
  int last_range_start = 0;
  for (int range_idx = 0; range_idx < live_ranges()->length(); ++range_idx) {
    LiveRange* range = live_ranges()->at(range_idx);
    if (range == NULL) continue;
    // Iterate over the first parts of multi-part live ranges.
    if (range->parent() != NULL) continue;
    // Skip non-pointer values.
    if (!HasTaggedValue(range->id())) continue;
    // Skip empty live ranges.
    if (range->IsEmpty()) continue;

    // Find the extent of the range and its children.
    int start = range->Start().InstructionIndex();
    int end = 0;
    for (LiveRange* cur = range; cur != NULL; cur = cur->next()) {
      LifetimePosition this_end = cur->End();
      if (this_end.InstructionIndex() > end) end = this_end.InstructionIndex();
      ASSERT(cur->Start().InstructionIndex() >= start);
    }

    // Most of the ranges are in order, but not all.  Keep an eye on when
    // they step backwards and reset the first_safe_point_index so we don't
    // miss any safe points.
    if (start < last_range_start) {
      first_safe_point_index = 0;
    }
    last_range_start = start;

    // Step across all the safe points that are before the start of this range,
    // recording how far we step in order to save doing this for the next range.
    while (first_safe_point_index < pointer_maps->length()) {
      LPointerMap* map = pointer_maps->at(first_safe_point_index);
      int safe_point = map->lithium_position();
      if (safe_point >= start) break;
      first_safe_point_index++;
    }

    // Step through the safe points to see whether they are in the range.
    for (int safe_point_index = first_safe_point_index;
         safe_point_index < pointer_maps->length();
         ++safe_point_index) {
      LPointerMap* map = pointer_maps->at(safe_point_index);
      int safe_point = map->lithium_position();

      // The safe points are sorted so we can stop searching here.
      if (safe_point - 1 > end) break;

      // Advance to the next active range that covers the current
      // safe point position.
      LifetimePosition safe_point_pos =
          LifetimePosition::FromInstructionIndex(safe_point);
      LiveRange* cur = range;
      while (cur != NULL && !cur->Covers(safe_point_pos.PrevInstruction())) {
        cur = cur->next();
      }
      if (cur == NULL) continue;

      // Check if the live range is spilled and the safe point is after
      // the spill position.
      if (range->HasAllocatedSpillOperand() &&
          safe_point >= range->spill_start_index()) {
        TraceAlloc("Pointer for range %d (spilled at %d) at safe point %d\n",
                   range->id(), range->spill_start_index(), safe_point);
        map->RecordPointer(range->GetSpillOperand());
      }

      if (!cur->IsSpilled()) {
        TraceAlloc("Pointer in register for range %d (start at %d) "
                   "at safe point %d\n",
                   cur->id(), cur->Start().Value(), safe_point);
        LOperand* operand = cur->CreateAssignedOperand();
        ASSERT(!operand->IsStackSlot());
        map->RecordPointer(operand);
      }
    }
  }
}


void LAllocator::ProcessOsrEntry() {
  const ZoneList<LInstruction*>* instrs = chunk_->instructions();

  // Linear search for the OSR entry instruction in the chunk.
  int index = -1;
  while (++index < instrs->length() &&
         !instrs->at(index)->IsOsrEntry()) {
  }
  ASSERT(index < instrs->length());
  LOsrEntry* instruction = LOsrEntry::cast(instrs->at(index));

  LifetimePosition position = LifetimePosition::FromInstructionIndex(index);
  for (int i = 0; i < live_ranges()->length(); ++i) {
    LiveRange* range = live_ranges()->at(i);
    if (range != NULL) {
      if (range->Covers(position) &&
          range->HasRegisterAssigned() &&
          range->TopLevel()->HasAllocatedSpillOperand()) {
        int reg_index = range->assigned_register();
        LOperand* spill_operand = range->TopLevel()->GetSpillOperand();
        if (range->IsDouble()) {
          instruction->MarkSpilledDoubleRegister(reg_index, spill_operand);
        } else {
          instruction->MarkSpilledRegister(reg_index, spill_operand);
        }
      }
    }
  }
}


void LAllocator::AllocateGeneralRegisters() {
  HPhase phase("Allocate general registers", this);
  num_registers_ = Register::kNumAllocatableRegisters;
  mode_ = GENERAL_REGISTERS;
  AllocateRegisters();
}


void LAllocator::AllocateDoubleRegisters() {
  HPhase phase("Allocate double registers", this);
  num_registers_ = DoubleRegister::kNumAllocatableRegisters;
  mode_ = DOUBLE_REGISTERS;
  AllocateRegisters();
}


void LAllocator::AllocateRegisters() {
  ASSERT(mode_ != NONE);
  reusable_slots_.Clear();

  for (int i = 0; i < live_ranges_.length(); ++i) {
    if (live_ranges_[i] != NULL) {
      if (RequiredRegisterKind(live_ranges_[i]->id()) == mode_) {
        AddToUnhandledUnsorted(live_ranges_[i]);
      }
    }
  }
  SortUnhandled();
  ASSERT(UnhandledIsSorted());

  ASSERT(active_live_ranges_.is_empty());
  ASSERT(inactive_live_ranges_.is_empty());

  if (mode_ == DOUBLE_REGISTERS) {
    for (int i = 0; i < fixed_double_live_ranges_.length(); ++i) {
      LiveRange* current = fixed_double_live_ranges_.at(i);
      if (current != NULL) {
        AddToInactive(current);
      }
    }
  } else {
    for (int i = 0; i < fixed_live_ranges_.length(); ++i) {
      LiveRange* current = fixed_live_ranges_.at(i);
      if (current != NULL) {
        AddToInactive(current);
      }
    }
  }

  while (!unhandled_live_ranges_.is_empty()) {
    ASSERT(UnhandledIsSorted());
    LiveRange* current = unhandled_live_ranges_.RemoveLast();
    ASSERT(UnhandledIsSorted());
    LifetimePosition position = current->Start();
    TraceAlloc("Processing interval %d start=%d\n",
               current->id(),
               position.Value());

    if (current->HasAllocatedSpillOperand()) {
      TraceAlloc("Live range %d already has a spill operand\n", current->id());
      LifetimePosition next_pos = position;
      if (chunk_->IsGapAt(next_pos.InstructionIndex())) {
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
        ASSERT(UnhandledIsSorted());
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

    ASSERT(!current->HasRegisterAssigned() && !current->IsSpilled());

    bool result = TryAllocateFreeReg(current);
    if (!result) {
      AllocateBlockedReg(current);
    }

    if (current->HasRegisterAssigned()) {
      AddToActive(current);
    }
  }

  active_live_ranges_.Clear();
  inactive_live_ranges_.Clear();
}


void LAllocator::Setup() {
  LConstantOperand::SetupCache();
  LStackSlot::SetupCache();
  LDoubleStackSlot::SetupCache();
  LRegister::SetupCache();
  LDoubleRegister::SetupCache();
}


const char* LAllocator::RegisterName(int allocation_index) {
  ASSERT(mode_ != NONE);
  if (mode_ == GENERAL_REGISTERS) {
    return Register::AllocationIndexToString(allocation_index);
  } else {
    return DoubleRegister::AllocationIndexToString(allocation_index);
  }
}


void LAllocator::TraceAlloc(const char* msg, ...) {
  if (FLAG_trace_alloc) {
    va_list arguments;
    va_start(arguments, msg);
    OS::VPrint(msg, arguments);
    va_end(arguments);
  }
}


void LAllocator::RecordUse(HValue* value, LUnallocated* operand) {
  operand->set_virtual_register(value->id());
  current_summary()->AddInput(operand);
}


bool LAllocator::HasTaggedValue(int virtual_register) const {
  HValue* value = graph()->LookupValue(virtual_register);
  if (value == NULL) return false;
  return value->representation().IsTagged();
}


RegisterKind LAllocator::RequiredRegisterKind(int virtual_register) const {
  if (virtual_register < first_artificial_register_) {
    HValue* value = graph()->LookupValue(virtual_register);
    if (value != NULL && value->representation().IsDouble()) {
      return DOUBLE_REGISTERS;
    }
  } else if (double_artificial_registers_.Contains(
      virtual_register - first_artificial_register_)) {
    return DOUBLE_REGISTERS;
  }

  return GENERAL_REGISTERS;
}


void LAllocator::MarkAsCall() {
  // Call instructions can use only fixed registers as
  // temporaries and outputs because all registers
  // are blocked by the calling convention.
  // Inputs can use either fixed register or have a short lifetime (be
  // used at start of the instruction).
  InstructionSummary* summary = current_summary();
#ifdef DEBUG
  ASSERT(summary->Output() == NULL ||
         LUnallocated::cast(summary->Output())->HasFixedPolicy() ||
         !LUnallocated::cast(summary->Output())->HasRegisterPolicy());
  for (int i = 0; i < summary->InputCount(); i++) {
    ASSERT(LUnallocated::cast(summary->InputAt(i))->HasFixedPolicy() ||
           LUnallocated::cast(summary->InputAt(i))->IsUsedAtStart() ||
           !LUnallocated::cast(summary->InputAt(i))->HasRegisterPolicy());
  }
  for (int i = 0; i < summary->TempCount(); i++) {
    ASSERT(LUnallocated::cast(summary->TempAt(i))->HasFixedPolicy() ||
           !LUnallocated::cast(summary->TempAt(i))->HasRegisterPolicy());
  }
#endif
  summary->MarkAsCall();
}


void LAllocator::MarkAsSaveDoubles() {
  current_summary()->MarkAsSaveDoubles();
}


void LAllocator::RecordDefinition(HInstruction* instr, LUnallocated* operand) {
  operand->set_virtual_register(instr->id());
  current_summary()->SetOutput(operand);
}


void LAllocator::RecordTemporary(LUnallocated* operand) {
  ASSERT(next_virtual_register_ < LUnallocated::kMaxVirtualRegisters);
  if (!operand->HasFixedPolicy()) {
    operand->set_virtual_register(next_virtual_register_++);
  }
  current_summary()->AddTemp(operand);
}


int LAllocator::max_initial_value_ids() {
  return LUnallocated::kMaxVirtualRegisters / 32;
}


void LAllocator::BeginInstruction() {
  if (next_summary_ == NULL) {
    next_summary_ = new InstructionSummary();
  }
  summary_stack_.Add(next_summary_);
  next_summary_ = NULL;
}


void LAllocator::SummarizeInstruction(int index) {
  InstructionSummary* sum = summary_stack_.RemoveLast();
  if (summaries_.length() <= index) {
    summaries_.AddBlock(NULL, index + 1 - summaries_.length());
  }
  ASSERT(summaries_[index] == NULL);
  if (sum->Output() != NULL || sum->InputCount() > 0 || sum->TempCount() > 0) {
    summaries_[index] = sum;
  } else {
    next_summary_ = sum;
  }
}


void LAllocator::OmitInstruction() {
  summary_stack_.RemoveLast();
}


void LAllocator::AddToActive(LiveRange* range) {
  TraceAlloc("Add live range %d to active\n", range->id());
  active_live_ranges_.Add(range);
}


void LAllocator::AddToInactive(LiveRange* range) {
  TraceAlloc("Add live range %d to inactive\n", range->id());
  inactive_live_ranges_.Add(range);
}


void LAllocator::AddToUnhandledSorted(LiveRange* range) {
  if (range == NULL || range->IsEmpty()) return;
  ASSERT(!range->HasRegisterAssigned() && !range->IsSpilled());
  for (int i = unhandled_live_ranges_.length() - 1; i >= 0; --i) {
    LiveRange* cur_range = unhandled_live_ranges_.at(i);
    if (range->ShouldBeAllocatedBefore(cur_range)) {
      TraceAlloc("Add live range %d to unhandled at %d\n", range->id(), i + 1);
      unhandled_live_ranges_.InsertAt(i + 1, range);
      ASSERT(UnhandledIsSorted());
      return;
    }
  }
  TraceAlloc("Add live range %d to unhandled at start\n", range->id());
  unhandled_live_ranges_.InsertAt(0, range);
  ASSERT(UnhandledIsSorted());
}


void LAllocator::AddToUnhandledUnsorted(LiveRange* range) {
  if (range == NULL || range->IsEmpty()) return;
  ASSERT(!range->HasRegisterAssigned() && !range->IsSpilled());
  TraceAlloc("Add live range %d to unhandled unsorted at end\n", range->id());
  unhandled_live_ranges_.Add(range);
}


static int UnhandledSortHelper(LiveRange* const* a, LiveRange* const* b) {
  ASSERT(!(*a)->ShouldBeAllocatedBefore(*b) ||
         !(*b)->ShouldBeAllocatedBefore(*a));
  if ((*a)->ShouldBeAllocatedBefore(*b)) return 1;
  if ((*b)->ShouldBeAllocatedBefore(*a)) return -1;
  return (*a)->id() - (*b)->id();
}


// Sort the unhandled live ranges so that the ranges to be processed first are
// at the end of the array list.  This is convenient for the register allocation
// algorithm because it is efficient to remove elements from the end.
void LAllocator::SortUnhandled() {
  TraceAlloc("Sort unhandled\n");
  unhandled_live_ranges_.Sort(&UnhandledSortHelper);
}


bool LAllocator::UnhandledIsSorted() {
  int len = unhandled_live_ranges_.length();
  for (int i = 1; i < len; i++) {
    LiveRange* a = unhandled_live_ranges_.at(i - 1);
    LiveRange* b = unhandled_live_ranges_.at(i);
    if (a->Start().Value() < b->Start().Value()) return false;
  }
  return true;
}


void LAllocator::FreeSpillSlot(LiveRange* range) {
  // Check that we are the last range.
  if (range->next() != NULL) return;

  if (!range->TopLevel()->HasAllocatedSpillOperand()) return;

  int index = range->TopLevel()->GetSpillOperand()->index();
  if (index >= 0) {
    reusable_slots_.Add(range);
  }
}


LOperand* LAllocator::TryReuseSpillSlot(LiveRange* range) {
  if (reusable_slots_.is_empty()) return NULL;
  if (reusable_slots_.first()->End().Value() >
      range->TopLevel()->Start().Value()) {
    return NULL;
  }
  LOperand* result = reusable_slots_.first()->TopLevel()->GetSpillOperand();
  reusable_slots_.Remove(0);
  return result;
}


void LAllocator::ActiveToHandled(LiveRange* range) {
  ASSERT(active_live_ranges_.Contains(range));
  active_live_ranges_.RemoveElement(range);
  TraceAlloc("Moving live range %d from active to handled\n", range->id());
  FreeSpillSlot(range);
}


void LAllocator::ActiveToInactive(LiveRange* range) {
  ASSERT(active_live_ranges_.Contains(range));
  active_live_ranges_.RemoveElement(range);
  inactive_live_ranges_.Add(range);
  TraceAlloc("Moving live range %d from active to inactive\n", range->id());
}


void LAllocator::InactiveToHandled(LiveRange* range) {
  ASSERT(inactive_live_ranges_.Contains(range));
  inactive_live_ranges_.RemoveElement(range);
  TraceAlloc("Moving live range %d from inactive to handled\n", range->id());
  FreeSpillSlot(range);
}


void LAllocator::InactiveToActive(LiveRange* range) {
  ASSERT(inactive_live_ranges_.Contains(range));
  inactive_live_ranges_.RemoveElement(range);
  active_live_ranges_.Add(range);
  TraceAlloc("Moving live range %d from inactive to active\n", range->id());
}


// TryAllocateFreeReg and AllocateBlockedReg assume this
// when allocating local arrays.
STATIC_ASSERT(DoubleRegister::kNumAllocatableRegisters >=
              Register::kNumAllocatableRegisters);


bool LAllocator::TryAllocateFreeReg(LiveRange* current) {
  LifetimePosition free_until_pos[DoubleRegister::kNumAllocatableRegisters];

  for (int i = 0; i < DoubleRegister::kNumAllocatableRegisters; i++) {
    free_until_pos[i] = LifetimePosition::MaxPosition();
  }

  for (int i = 0; i < active_live_ranges_.length(); ++i) {
    LiveRange* cur_active = active_live_ranges_.at(i);
    free_until_pos[cur_active->assigned_register()] =
        LifetimePosition::FromInstructionIndex(0);
  }

  for (int i = 0; i < inactive_live_ranges_.length(); ++i) {
    LiveRange* cur_inactive = inactive_live_ranges_.at(i);
    ASSERT(cur_inactive->End().Value() > current->Start().Value());
    LifetimePosition next_intersection =
        cur_inactive->FirstIntersection(current);
    if (!next_intersection.IsValid()) continue;
    int cur_reg = cur_inactive->assigned_register();
    free_until_pos[cur_reg] = Min(free_until_pos[cur_reg], next_intersection);
  }

  UsePosition* hinted_use = current->FirstPosWithHint();
  if (hinted_use != NULL) {
    LOperand* hint = hinted_use->hint();
    if (hint->IsRegister() || hint->IsDoubleRegister()) {
      int register_index = hint->index();
      TraceAlloc(
          "Found reg hint %s (free until [%d) for live range %d (end %d[).\n",
          RegisterName(register_index),
          free_until_pos[register_index].Value(),
          current->id(),
          current->End().Value());

      // The desired register is free until the end of the current live range.
      if (free_until_pos[register_index].Value() >= current->End().Value()) {
        TraceAlloc("Assigning preferred reg %s to live range %d\n",
                   RegisterName(register_index),
                   current->id());
        current->set_assigned_register(register_index, mode_);
        return true;
      }
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
    LiveRange* tail = SplitAt(current, pos);
    AddToUnhandledSorted(tail);
  }


  // Register reg is available at the range start and is free until
  // the range end.
  ASSERT(pos.Value() >= current->End().Value());
  TraceAlloc("Assigning free reg %s to live range %d\n",
             RegisterName(reg),
             current->id());
  current->set_assigned_register(reg, mode_);

  return true;
}


void LAllocator::AllocateBlockedReg(LiveRange* current) {
  UsePosition* register_use = current->NextRegisterPosition(current->Start());
  if (register_use == NULL) {
    // There is no use in the current live range that requires a register.
    // We can just spill it.
    Spill(current);
    return;
  }


  LifetimePosition use_pos[DoubleRegister::kNumAllocatableRegisters];
  LifetimePosition block_pos[DoubleRegister::kNumAllocatableRegisters];

  for (int i = 0; i < DoubleRegister::kNumAllocatableRegisters; i++) {
    use_pos[i] = block_pos[i] = LifetimePosition::MaxPosition();
  }

  for (int i = 0; i < active_live_ranges_.length(); ++i) {
    LiveRange* range = active_live_ranges_[i];
    int cur_reg = range->assigned_register();
    if (range->IsFixed() || !range->CanBeSpilled(current->Start())) {
      block_pos[cur_reg] = use_pos[cur_reg] =
          LifetimePosition::FromInstructionIndex(0);
    } else {
      UsePosition* next_use = range->NextUsePositionRegisterIsBeneficial(
          current->Start());
      if (next_use == NULL) {
        use_pos[cur_reg] = range->End();
      } else {
        use_pos[cur_reg] = next_use->pos();
      }
    }
  }

  for (int i = 0; i < inactive_live_ranges_.length(); ++i) {
    LiveRange* range = inactive_live_ranges_.at(i);
    ASSERT(range->End().Value() > current->Start().Value());
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
    //
    // Corner case: the first use position is equal to the start of the range.
    // In this case we have nothing to spill and SpillBetween will just return
    // this range to the list of unhandled ones. This will lead to the infinite
    // loop.
    ASSERT(current->Start().Value() < register_use->pos().Value());
    SpillBetween(current, current->Start(), register_use->pos());
    return;
  }

  if (block_pos[reg].Value() < current->End().Value()) {
    // Register becomes blocked before the current range end. Split before that
    // position.
    LiveRange* tail = SplitBetween(current,
                                   current->Start(),
                                   block_pos[reg].InstructionStart());
    AddToUnhandledSorted(tail);
  }

  // Register reg is not blocked for the whole range.
  ASSERT(block_pos[reg].Value() >= current->End().Value());
  TraceAlloc("Assigning blocked reg %s to live range %d\n",
             RegisterName(reg),
             current->id());
  current->set_assigned_register(reg, mode_);

  // This register was not free. Thus we need to find and spill
  // parts of active and inactive live regions that use the same register
  // at the same lifetime positions as current.
  SplitAndSpillIntersecting(current);
}


void LAllocator::SplitAndSpillIntersecting(LiveRange* current) {
  ASSERT(current->HasRegisterAssigned());
  int reg = current->assigned_register();
  LifetimePosition split_pos = current->Start();
  for (int i = 0; i < active_live_ranges_.length(); ++i) {
    LiveRange* range = active_live_ranges_[i];
    if (range->assigned_register() == reg) {
      UsePosition* next_pos = range->NextRegisterPosition(current->Start());
      if (next_pos == NULL) {
        SpillAfter(range, split_pos);
      } else {
        SpillBetween(range, split_pos, next_pos->pos());
      }
      ActiveToHandled(range);
      --i;
    }
  }

  for (int i = 0; i < inactive_live_ranges_.length(); ++i) {
    LiveRange* range = inactive_live_ranges_[i];
    ASSERT(range->End().Value() > current->Start().Value());
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
        InactiveToHandled(range);
        --i;
      }
    }
  }
}


bool LAllocator::IsBlockBoundary(LifetimePosition pos) {
  return pos.IsInstructionStart() &&
      chunk_->instructions()->at(pos.InstructionIndex())->IsLabel();
}


void LAllocator::AddGapMove(int pos, LiveRange* prev, LiveRange* next) {
  UsePosition* prev_pos = prev->AddUsePosition(
      LifetimePosition::FromInstructionIndex(pos));
  UsePosition* next_pos = next->AddUsePosition(
      LifetimePosition::FromInstructionIndex(pos));
  LOperand* prev_operand = prev_pos->operand();
  LOperand* next_operand = next_pos->operand();
  LGap* gap = chunk_->GetGapAt(pos);
  gap->GetOrCreateParallelMove(LGap::START)->
      AddMove(prev_operand, next_operand);
  next_pos->set_hint(prev_operand);
}


LiveRange* LAllocator::SplitAt(LiveRange* range, LifetimePosition pos) {
  ASSERT(!range->IsFixed());
  TraceAlloc("Splitting live range %d at %d\n", range->id(), pos.Value());

  if (pos.Value() <= range->Start().Value()) return range;

  LiveRange* result = LiveRangeFor(next_virtual_register_++);
  range->SplitAt(pos, result);
  return result;
}


LiveRange* LAllocator::SplitBetween(LiveRange* range,
                                    LifetimePosition start,
                                    LifetimePosition end) {
  ASSERT(!range->IsFixed());
  TraceAlloc("Splitting live range %d in position between [%d, %d]\n",
             range->id(),
             start.Value(),
             end.Value());

  LifetimePosition split_pos = FindOptimalSplitPos(start, end);
  ASSERT(split_pos.Value() >= start.Value());
  return SplitAt(range, split_pos);
}


LifetimePosition LAllocator::FindOptimalSplitPos(LifetimePosition start,
                                                 LifetimePosition end) {
  int start_instr = start.InstructionIndex();
  int end_instr = end.InstructionIndex();
  ASSERT(start_instr <= end_instr);

  // We have no choice
  if (start_instr == end_instr) return end;

  HBasicBlock* end_block = GetBlock(start);
  HBasicBlock* start_block = GetBlock(end);

  if (end_block == start_block) {
    // The interval is split in the same basic block. Split at latest possible
    // position.
    return end;
  }

  HBasicBlock* block = end_block;
  // Find header of outermost loop.
  while (block->parent_loop_header() != NULL &&
      block->parent_loop_header()->block_id() > start_block->block_id()) {
    block = block->parent_loop_header();
  }

  if (block == end_block) return end;

  return LifetimePosition::FromInstructionIndex(
      block->first_instruction_index());
}


void LAllocator::SpillAfter(LiveRange* range, LifetimePosition pos) {
  LiveRange* second_part = SplitAt(range, pos);
  Spill(second_part);
}


void LAllocator::SpillBetween(LiveRange* range,
                              LifetimePosition start,
                              LifetimePosition end) {
  ASSERT(start.Value() < end.Value());
  LiveRange* second_part = SplitAt(range, start);

  if (second_part->Start().Value() < end.Value()) {
    // The split result intersects with [start, end[.
    // Split it at position between ]start+1, end[, spill the middle part
    // and put the rest to unhandled.
    LiveRange* third_part = SplitBetween(
        second_part,
        second_part->Start().InstructionEnd(),
        end.PrevInstruction().InstructionEnd());

    ASSERT(third_part != second_part);

    Spill(second_part);
    AddToUnhandledSorted(third_part);
  } else {
    // The split result does not intersect with [start, end[.
    // Nothing to spill. Just put it to unhandled as whole.
    AddToUnhandledSorted(second_part);
  }
}


void LAllocator::Spill(LiveRange* range) {
  ASSERT(!range->IsSpilled());
  TraceAlloc("Spilling live range %d\n", range->id());
  LiveRange* first = range->TopLevel();

  if (!first->HasAllocatedSpillOperand()) {
    LOperand* op = TryReuseSpillSlot(range);
    if (op == NULL) op = chunk_->GetNextSpillSlot(mode_ == DOUBLE_REGISTERS);
    first->SetSpillOperand(op);
  }
  range->MakeSpilled();
}


int LAllocator::RegisterCount() const {
  return num_registers_;
}


#ifdef DEBUG


void LAllocator::Verify() const {
  for (int i = 0; i < live_ranges()->length(); ++i) {
    LiveRange* current = live_ranges()->at(i);
    if (current != NULL) current->Verify();
  }
}


#endif


} }  // namespace v8::internal

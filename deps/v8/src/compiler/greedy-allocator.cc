// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/greedy-allocator.h"
#include "src/compiler/register-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...)                             \
  do {                                         \
    if (FLAG_trace_alloc) PrintF(__VA_ARGS__); \
  } while (false)


namespace {


void UpdateOperands(LiveRange* range, RegisterAllocationData* data) {
  int reg_id = range->assigned_register();
  range->SetUseHints(reg_id);
  if (range->is_phi()) {
    data->GetPhiMapValueFor(range->id())->set_assigned_register(reg_id);
  }
}


LiveRange* Split(LiveRange* range, RegisterAllocationData* data,
                 LifetimePosition pos) {
  DCHECK(range->Start() < pos && pos < range->End());
  DCHECK(pos.IsStart() || pos.IsGapPosition() ||
         (data->code()
              ->GetInstructionBlock(pos.ToInstructionIndex())
              ->last_instruction_index() != pos.ToInstructionIndex()));
  LiveRange* result = data->NewChildRangeFor(range);
  range->SplitAt(pos, result, data->allocation_zone());
  return result;
}


// TODO(mtrofin): explain why splitting in gap START is always OK.
LifetimePosition GetSplitPositionForInstruction(const LiveRange* range,
                                                const InstructionSequence* code,
                                                int instruction_index) {
  LifetimePosition ret = LifetimePosition::Invalid();

  ret = LifetimePosition::GapFromInstructionIndex(instruction_index);
  if (range->Start() >= ret || ret >= range->End()) {
    return LifetimePosition::Invalid();
  }
  return ret;
}


int GetFirstGapIndex(const UseInterval* interval) {
  LifetimePosition start = interval->start();
  int ret = start.ToInstructionIndex();
  return ret;
}


int GetLastGapIndex(const UseInterval* interval) {
  LifetimePosition end = interval->end();
  return end.ToInstructionIndex();
}


// Basic heuristic for advancing the algorithm, if any other splitting heuristic
// failed.
LifetimePosition GetLastResortSplitPosition(const LiveRange* range,
                                            const InstructionSequence* code) {
  if (range->first_interval()->next() != nullptr) {
    return range->first_interval()->next()->start();
  }

  UseInterval* interval = range->first_interval();
  int first = GetFirstGapIndex(interval);
  int last = GetLastGapIndex(interval);
  if (first == last) return LifetimePosition::Invalid();

  // TODO(mtrofin:) determine why we can't just split somewhere arbitrary
  // within the range, e.g. it's middle.
  for (UsePosition* pos = range->first_pos(); pos != nullptr;
       pos = pos->next()) {
    if (pos->type() != UsePositionType::kRequiresRegister) continue;
    LifetimePosition before = GetSplitPositionForInstruction(
        range, code, pos->pos().ToInstructionIndex());
    if (before.IsValid()) return before;
    LifetimePosition after = GetSplitPositionForInstruction(
        range, code, pos->pos().ToInstructionIndex() + 1);
    if (after.IsValid()) return after;
  }
  return LifetimePosition::Invalid();
}


bool IsProgressPossible(const LiveRange* range,
                        const InstructionSequence* code) {
  return range->CanBeSpilled(range->Start()) ||
         GetLastResortSplitPosition(range, code).IsValid();
}
}  // namespace


AllocationCandidate AllocationScheduler::GetNext() {
  DCHECK(!queue_.empty());
  AllocationCandidate ret = queue_.top();
  queue_.pop();
  return ret;
}


void AllocationScheduler::Schedule(LiveRange* range) {
  TRACE("Scheduling live range %d.\n", range->id());
  queue_.push(AllocationCandidate(range));
}

GreedyAllocator::GreedyAllocator(RegisterAllocationData* data,
                                 RegisterKind kind, Zone* local_zone)
    : RegisterAllocator(data, kind),
      local_zone_(local_zone),
      allocations_(local_zone),
      scheduler_(local_zone) {}


void GreedyAllocator::AssignRangeToRegister(int reg_id, LiveRange* range) {
  TRACE("Assigning register %s to live range %d\n", RegisterName(reg_id),
        range->id());

  DCHECK(!range->HasRegisterAssigned());

  current_allocations(reg_id)->AllocateRange(range);

  TRACE("Assigning %s to range %d\n", RegisterName(reg_id), range->id());
  range->set_assigned_register(reg_id);

  DCHECK(current_allocations(reg_id)->VerifyAllocationsAreValid());
}


void GreedyAllocator::PreallocateFixedRanges() {
  allocations_.resize(num_registers());
  for (int i = 0; i < num_registers(); i++) {
    allocations_[i] = new (local_zone()) CoalescedLiveRanges(local_zone());
  }

  for (LiveRange* fixed_range : GetFixedRegisters()) {
    if (fixed_range != nullptr) {
      DCHECK_EQ(mode(), fixed_range->kind());
      DCHECK(fixed_range->IsFixed());

      int reg_nr = fixed_range->assigned_register();
      EnsureValidRangeWeight(fixed_range);
      current_allocations(reg_nr)->AllocateRange(fixed_range);
    }
  }
}


void GreedyAllocator::ScheduleAllocationCandidates() {
  for (auto range : data()->live_ranges()) {
    if (CanProcessRange(range) && !range->spilled()) {
      scheduler().Schedule(range);
    }
  }
}


void GreedyAllocator::TryAllocateCandidate(
    const AllocationCandidate& candidate) {
  // At this point, this is just a live range. TODO: groups.
  TryAllocateLiveRange(candidate.live_range());
}


void GreedyAllocator::TryAllocateLiveRange(LiveRange* range) {
  // TODO(mtrofin): once we introduce groups, we'll want to first try and
  // allocate at the preferred register.
  TRACE("Attempting to allocate live range %d\n", range->id());
  int free_reg = -1;
  int evictable_reg = -1;
  EnsureValidRangeWeight(range);
  DCHECK(range->weight() != LiveRange::kInvalidWeight);

  float smallest_weight = LiveRange::kMaxWeight;

  // Seek either the first free register, or, from the set of registers
  // where the maximum conflict is lower than the candidate's weight, the one
  // with the smallest such weight.
  for (int i = 0; i < num_registers(); i++) {
    float max_conflict_weight =
        current_allocations(i)->GetMaximumConflictingWeight(range);
    if (max_conflict_weight == LiveRange::kInvalidWeight) {
      free_reg = i;
      break;
    }
    if (max_conflict_weight < range->weight() &&
        max_conflict_weight < smallest_weight) {
      smallest_weight = max_conflict_weight;
      evictable_reg = i;
    }
  }

  // We have a free register, so we use it.
  if (free_reg >= 0) {
    TRACE("Found free register %s for live range %d\n", RegisterName(free_reg),
          range->id());
    AssignRangeToRegister(free_reg, range);
    return;
  }

  // We found a register to perform evictions, so we evict and allocate our
  // candidate.
  if (evictable_reg >= 0) {
    TRACE("Found evictable register %s for live range %d\n",
          RegisterName(free_reg), range->id());
    current_allocations(evictable_reg)
        ->EvictAndRescheduleConflicts(range, &scheduler());
    AssignRangeToRegister(evictable_reg, range);
    return;
  }

  // The range needs to be split or spilled.
  SplitOrSpillBlockedRange(range);
}


void GreedyAllocator::SplitAndSpillRangesDefinedByMemoryOperand() {
  size_t initial_range_count = data()->live_ranges().size();
  for (size_t i = 0; i < initial_range_count; ++i) {
    auto range = data()->live_ranges()[i];
    if (!CanProcessRange(range)) continue;
    if (range->HasNoSpillType()) continue;

    LifetimePosition start = range->Start();
    TRACE("Live range %d is defined by a spill operand.\n", range->id());
    auto next_pos = start;
    if (next_pos.IsGapPosition()) {
      next_pos = next_pos.NextStart();
    }
    auto pos = range->NextUsePositionRegisterIsBeneficial(next_pos);
    // If the range already has a spill operand and it doesn't need a
    // register immediately, split it and spill the first part of the range.
    if (pos == nullptr) {
      Spill(range);
    } else if (pos->pos() > range->Start().NextStart()) {
      // Do not spill live range eagerly if use position that can benefit from
      // the register is too close to the start of live range.
      auto split_pos = pos->pos();
      if (data()->IsBlockBoundary(split_pos.Start())) {
        split_pos = split_pos.Start();
      } else {
        split_pos = split_pos.PrevStart().End();
      }
      Split(range, data(), split_pos);
      Spill(range);
    }
  }
}


void GreedyAllocator::AllocateRegisters() {
  CHECK(scheduler().empty());
  CHECK(allocations_.empty());

  TRACE("Begin allocating function %s with the Greedy Allocator\n",
        data()->debug_name());

  SplitAndSpillRangesDefinedByMemoryOperand();
  PreallocateFixedRanges();
  ScheduleAllocationCandidates();

  while (!scheduler().empty()) {
    AllocationCandidate candidate = scheduler().GetNext();
    TryAllocateCandidate(candidate);
  }


  // We do not rely on the hint mechanism used by LinearScan, so no need to
  // actively update/reset operands until the end.
  for (auto range : data()->live_ranges()) {
    if (CanProcessRange(range) && range->HasRegisterAssigned()) {
      UpdateOperands(range, data());
    }
  }

  for (size_t i = 0; i < allocations_.size(); ++i) {
    if (!allocations_[i]->empty()) {
      data()->MarkAllocated(mode(), static_cast<int>(i));
    }
  }
  allocations_.clear();

  TRACE("End allocating function %s with the Greedy Allocator\n",
        data()->debug_name());
}


void GreedyAllocator::EnsureValidRangeWeight(LiveRange* range) {
  // The live range weight will be invalidated when ranges are created or split.
  // Otherwise, it is consistently updated when the range is allocated or
  // unallocated.
  if (range->weight() != LiveRange::kInvalidWeight) return;

  if (range->IsFixed()) {
    range->set_weight(LiveRange::kMaxWeight);
    return;
  }
  if (!IsProgressPossible(range, code())) {
    range->set_weight(LiveRange::kMaxWeight);
    return;
  }

  float use_count = 0.0;
  for (auto pos = range->first_pos(); pos != nullptr; pos = pos->next()) {
    ++use_count;
  }
  range->set_weight(use_count / static_cast<float>(range->GetSize()));
}


void GreedyAllocator::SpillRangeAsLastResort(LiveRange* range) {
  LifetimePosition start = range->Start();
  CHECK(range->CanBeSpilled(start));

  DCHECK(range->NextRegisterPosition(start) == nullptr);
  Spill(range);
}


void GreedyAllocator::SplitOrSpillBlockedRange(LiveRange* range) {
  // TODO(mtrofin): replace the call below with the entry point selecting
  // heuristics, once they exist, out of which GLRSP is the last one.
  auto pos = GetLastResortSplitPosition(range, code());
  if (pos.IsValid()) {
    LiveRange* tail = Split(range, data(), pos);
    DCHECK(tail != range);
    scheduler().Schedule(tail);
    scheduler().Schedule(range);
    return;
  }
  SpillRangeAsLastResort(range);
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8

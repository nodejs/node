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


const float GreedyAllocator::kAllocatedRangeMultiplier = 10.0;


namespace {

void UpdateOperands(LiveRange* range, RegisterAllocationData* data) {
  int reg_id = range->assigned_register();
  range->SetUseHints(reg_id);
  if (range->IsTopLevel() && range->TopLevel()->is_phi()) {
    data->GetPhiMapValueFor(range->TopLevel())->set_assigned_register(reg_id);
  }
}


void UnsetOperands(LiveRange* range, RegisterAllocationData* data) {
  range->UnsetUseHints();
  if (range->IsTopLevel() && range->TopLevel()->is_phi()) {
    data->GetPhiMapValueFor(range->TopLevel())->UnsetAssignedRegister();
  }
}


LiveRange* Split(LiveRange* range, RegisterAllocationData* data,
                 LifetimePosition pos) {
  DCHECK(range->Start() < pos && pos < range->End());
  DCHECK(pos.IsStart() || pos.IsGapPosition() ||
         (data->code()
              ->GetInstructionBlock(pos.ToInstructionIndex())
              ->last_instruction_index() != pos.ToInstructionIndex()));
  LiveRange* result = range->SplitAt(pos, data->allocation_zone());
  return result;
}


}  // namespace


AllocationCandidate AllocationScheduler::GetNext() {
  DCHECK(!queue_.empty());
  AllocationCandidate ret = queue_.top();
  queue_.pop();
  return ret;
}


void AllocationScheduler::Schedule(LiveRange* range) {
  TRACE("Scheduling live range %d:%d.\n", range->TopLevel()->vreg(),
        range->relative_id());
  queue_.push(AllocationCandidate(range));
}


void AllocationScheduler::Schedule(LiveRangeGroup* group) {
  queue_.push(AllocationCandidate(group));
}

GreedyAllocator::GreedyAllocator(RegisterAllocationData* data,
                                 RegisterKind kind, Zone* local_zone)
    : RegisterAllocator(data, kind),
      local_zone_(local_zone),
      allocations_(local_zone),
      scheduler_(local_zone),
      groups_(local_zone) {}


void GreedyAllocator::AssignRangeToRegister(int reg_id, LiveRange* range) {
  TRACE("Assigning register %s to live range %d:%d\n", RegisterName(reg_id),
        range->TopLevel()->vreg(), range->relative_id());

  DCHECK(!range->HasRegisterAssigned());

  AllocateRegisterToRange(reg_id, range);

  TRACE("Assigning %s to range %d%d.\n", RegisterName(reg_id),
        range->TopLevel()->vreg(), range->relative_id());
  range->set_assigned_register(reg_id);
  UpdateOperands(range, data());
}


void GreedyAllocator::PreallocateFixedRanges() {
  allocations_.resize(num_registers());
  for (int i = 0; i < num_registers(); i++) {
    allocations_[i] = new (local_zone()) CoalescedLiveRanges(local_zone());
  }

  for (LiveRange* fixed_range : GetFixedRegisters()) {
    if (fixed_range != nullptr) {
      DCHECK_EQ(mode(), fixed_range->kind());
      DCHECK(fixed_range->TopLevel()->IsFixed());

      int reg_nr = fixed_range->assigned_register();
      EnsureValidRangeWeight(fixed_range);
      AllocateRegisterToRange(reg_nr, fixed_range);
    }
  }
}


void GreedyAllocator::GroupLiveRanges() {
  CoalescedLiveRanges grouper(local_zone());
  for (TopLevelLiveRange* range : data()->live_ranges()) {
    grouper.clear();
    // Skip splinters, because we do not want to optimize for them, and moves
    // due to assigning them to different registers occur in deferred blocks.
    if (!CanProcessRange(range) || range->IsSplinter() || !range->is_phi()) {
      continue;
    }

    // A phi can't be a memory operand, so it couldn't have been split.
    DCHECK(!range->spilled());

    // Maybe this phi range is itself an input to another phi which was already
    // processed.
    LiveRangeGroup* latest_grp = range->group() != nullptr
                                     ? range->group()
                                     : new (local_zone())
                                           LiveRangeGroup(local_zone());

    // Populate the grouper.
    if (range->group() == nullptr) {
      grouper.AllocateRange(range);
    } else {
      for (LiveRange* member : range->group()->ranges()) {
        grouper.AllocateRange(member);
      }
    }
    for (int j : data()->GetPhiMapValueFor(range)->phi()->operands()) {
      // skip output also in input, which may happen for loops.
      if (j == range->vreg()) continue;

      TopLevelLiveRange* other_top = data()->live_ranges()[j];

      if (other_top->IsSplinter()) continue;
      // If the other was a memory operand, it might have been split.
      // So get the unsplit part.
      LiveRange* other =
          other_top->next() == nullptr ? other_top : other_top->next();

      if (other->spilled()) continue;

      LiveRangeGroup* other_group = other->group();
      if (other_group != nullptr) {
        bool can_merge = true;
        for (LiveRange* member : other_group->ranges()) {
          if (grouper.GetConflicts(member).Current() != nullptr) {
            can_merge = false;
            break;
          }
        }
        // If each member doesn't conflict with the current group, then since
        // the members don't conflict with eachother either, we can merge them.
        if (can_merge) {
          latest_grp->ranges().insert(latest_grp->ranges().end(),
                                      other_group->ranges().begin(),
                                      other_group->ranges().end());
          for (LiveRange* member : other_group->ranges()) {
            grouper.AllocateRange(member);
            member->set_group(latest_grp);
          }
          // Clear the other range, so we avoid scheduling it.
          other_group->ranges().clear();
        }
      } else if (grouper.GetConflicts(other).Current() == nullptr) {
        grouper.AllocateRange(other);
        latest_grp->ranges().push_back(other);
        other->set_group(latest_grp);
      }
    }

    if (latest_grp->ranges().size() > 0 && range->group() == nullptr) {
      latest_grp->ranges().push_back(range);
      DCHECK(latest_grp->ranges().size() > 1);
      groups().push_back(latest_grp);
      range->set_group(latest_grp);
    }
  }
}


void GreedyAllocator::ScheduleAllocationCandidates() {
  for (LiveRangeGroup* group : groups()) {
    if (group->ranges().size() > 0) {
      // We shouldn't have added single-range groups.
      DCHECK(group->ranges().size() != 1);
      scheduler().Schedule(group);
    }
  }
  for (LiveRange* range : data()->live_ranges()) {
    if (CanProcessRange(range)) {
      for (LiveRange* child = range; child != nullptr; child = child->next()) {
        if (!child->spilled() && child->group() == nullptr) {
          scheduler().Schedule(child);
        }
      }
    }
  }
}


void GreedyAllocator::TryAllocateCandidate(
    const AllocationCandidate& candidate) {
  if (candidate.is_group()) {
    TryAllocateGroup(candidate.group());
  } else {
    TryAllocateLiveRange(candidate.live_range());
  }
}


void GreedyAllocator::TryAllocateGroup(LiveRangeGroup* group) {
  float group_weight = 0.0;
  for (LiveRange* member : group->ranges()) {
    EnsureValidRangeWeight(member);
    group_weight = Max(group_weight, member->weight());
  }

  float eviction_weight = group_weight;
  int eviction_reg = -1;
  int free_reg = -1;
  for (int i = 0; i < num_allocatable_registers(); ++i) {
    int reg = allocatable_register_code(i);
    float weight = GetMaximumConflictingWeight(reg, group, group_weight);
    if (weight == LiveRange::kInvalidWeight) {
      free_reg = reg;
      break;
    }
    if (weight < eviction_weight) {
      eviction_weight = weight;
      eviction_reg = reg;
    }
  }
  if (eviction_reg < 0 && free_reg < 0) {
    for (LiveRange* member : group->ranges()) {
      scheduler().Schedule(member);
    }
    return;
  }
  if (free_reg < 0) {
    DCHECK(eviction_reg >= 0);
    for (LiveRange* member : group->ranges()) {
      EvictAndRescheduleConflicts(eviction_reg, member);
    }
    free_reg = eviction_reg;
  }

  DCHECK(free_reg >= 0);
  for (LiveRange* member : group->ranges()) {
    AssignRangeToRegister(free_reg, member);
  }
}


void GreedyAllocator::TryAllocateLiveRange(LiveRange* range) {
  // TODO(mtrofin): once we introduce groups, we'll want to first try and
  // allocate at the preferred register.
  TRACE("Attempting to allocate live range %d:%d.\n", range->TopLevel()->vreg(),
        range->relative_id());
  int free_reg = -1;
  int evictable_reg = -1;
  int hinted_reg = -1;

  EnsureValidRangeWeight(range);
  float competing_weight = range->weight();
  DCHECK(competing_weight != LiveRange::kInvalidWeight);

  // Can we allocate at the hinted register?
  if (range->FirstHintPosition(&hinted_reg) != nullptr) {
    DCHECK(hinted_reg >= 0);
    float max_conflict_weight =
        GetMaximumConflictingWeight(hinted_reg, range, competing_weight);
    if (max_conflict_weight == LiveRange::kInvalidWeight) {
      free_reg = hinted_reg;
    } else if (max_conflict_weight < range->weight()) {
      evictable_reg = hinted_reg;
    }
  }

  if (free_reg < 0 && evictable_reg < 0) {
    // There was no hinted reg, or we cannot allocate there.
    float smallest_weight = LiveRange::kMaxWeight;

    // Seek either the first free register, or, from the set of registers
    // where the maximum conflict is lower than the candidate's weight, the one
    // with the smallest such weight.
    for (int i = 0; i < num_allocatable_registers(); i++) {
      int reg = allocatable_register_code(i);
      // Skip unnecessarily re-visiting the hinted register, if any.
      if (reg == hinted_reg) continue;
      float max_conflict_weight =
          GetMaximumConflictingWeight(reg, range, competing_weight);
      if (max_conflict_weight == LiveRange::kInvalidWeight) {
        free_reg = reg;
        break;
      }
      if (max_conflict_weight < range->weight() &&
          max_conflict_weight < smallest_weight) {
        smallest_weight = max_conflict_weight;
        evictable_reg = reg;
      }
    }
  }

  // We have a free register, so we use it.
  if (free_reg >= 0) {
    TRACE("Found free register %s for live range %d:%d.\n",
          RegisterName(free_reg), range->TopLevel()->vreg(),
          range->relative_id());
    AssignRangeToRegister(free_reg, range);
    return;
  }

  // We found a register to perform evictions, so we evict and allocate our
  // candidate.
  if (evictable_reg >= 0) {
    TRACE("Found evictable register %s for live range %d:%d.\n",
          RegisterName(free_reg), range->TopLevel()->vreg(),
          range->relative_id());
    EvictAndRescheduleConflicts(evictable_reg, range);
    AssignRangeToRegister(evictable_reg, range);
    return;
  }

  // The range needs to be split or spilled.
  SplitOrSpillBlockedRange(range);
}


void GreedyAllocator::EvictAndRescheduleConflicts(unsigned reg_id,
                                                  const LiveRange* range) {
  auto conflicts = current_allocations(reg_id)->GetConflicts(range);
  for (LiveRange* conflict = conflicts.Current(); conflict != nullptr;
       conflict = conflicts.RemoveCurrentAndGetNext()) {
    DCHECK(conflict->HasRegisterAssigned());
    CHECK(!conflict->TopLevel()->IsFixed());
    conflict->UnsetAssignedRegister();
    UnsetOperands(conflict, data());
    UpdateWeightAtEviction(conflict);
    scheduler().Schedule(conflict);
    TRACE("Evicted range %d%d.\n", conflict->TopLevel()->vreg(),
          conflict->relative_id());
  }
}


void GreedyAllocator::AllocateRegisters() {
  CHECK(scheduler().empty());
  CHECK(allocations_.empty());

  TRACE("Begin allocating function %s with the Greedy Allocator\n",
        data()->debug_name());

  SplitAndSpillRangesDefinedByMemoryOperand(true);
  GroupLiveRanges();
  ScheduleAllocationCandidates();
  PreallocateFixedRanges();
  while (!scheduler().empty()) {
    AllocationCandidate candidate = scheduler().GetNext();
    TryAllocateCandidate(candidate);
  }

  for (size_t i = 0; i < allocations_.size(); ++i) {
    if (!allocations_[i]->empty()) {
      data()->MarkAllocated(mode(), static_cast<int>(i));
    }
  }
  allocations_.clear();

  TryReuseSpillRangesForGroups();

  TRACE("End allocating function %s with the Greedy Allocator\n",
        data()->debug_name());
}


void GreedyAllocator::TryReuseSpillRangesForGroups() {
  for (TopLevelLiveRange* top : data()->live_ranges()) {
    if (!CanProcessRange(top) || !top->is_phi() || top->group() == nullptr) {
      continue;
    }

    SpillRange* spill_range = nullptr;
    for (LiveRange* member : top->group()->ranges()) {
      if (!member->TopLevel()->HasSpillRange()) continue;
      SpillRange* member_range = member->TopLevel()->GetSpillRange();
      if (spill_range == nullptr) {
        spill_range = member_range;
      } else {
        // This may not always succeed, because we group non-conflicting ranges
        // that may have been splintered, and the splinters may cause conflicts
        // in the spill ranges.
        // TODO(mtrofin): should the splinters own their own spill ranges?
        spill_range->TryMerge(member_range);
      }
    }
  }
}


float GreedyAllocator::GetMaximumConflictingWeight(
    unsigned reg_id, const LiveRange* range, float competing_weight) const {
  float ret = LiveRange::kInvalidWeight;

  auto conflicts = current_allocations(reg_id)->GetConflicts(range);
  for (LiveRange* conflict = conflicts.Current(); conflict != nullptr;
       conflict = conflicts.GetNext()) {
    DCHECK_NE(conflict->weight(), LiveRange::kInvalidWeight);
    if (competing_weight <= conflict->weight()) return LiveRange::kMaxWeight;
    ret = Max(ret, conflict->weight());
    DCHECK(ret < LiveRange::kMaxWeight);
  }

  return ret;
}


float GreedyAllocator::GetMaximumConflictingWeight(unsigned reg_id,
                                                   const LiveRangeGroup* group,
                                                   float group_weight) const {
  float ret = LiveRange::kInvalidWeight;

  for (LiveRange* member : group->ranges()) {
    float member_conflict_weight =
        GetMaximumConflictingWeight(reg_id, member, group_weight);
    if (member_conflict_weight == LiveRange::kMaxWeight) {
      return LiveRange::kMaxWeight;
    }
    if (member_conflict_weight > group_weight) return LiveRange::kMaxWeight;
    ret = Max(member_conflict_weight, ret);
  }

  return ret;
}


void GreedyAllocator::EnsureValidRangeWeight(LiveRange* range) {
  // The live range weight will be invalidated when ranges are created or split.
  // Otherwise, it is consistently updated when the range is allocated or
  // unallocated.
  if (range->weight() != LiveRange::kInvalidWeight) return;

  if (range->TopLevel()->IsFixed()) {
    range->set_weight(LiveRange::kMaxWeight);
    return;
  }
  if (!IsProgressPossible(range)) {
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


LiveRange* GreedyAllocator::GetRemainderAfterSplittingAroundFirstCall(
    LiveRange* range) {
  LiveRange* ret = range;
  for (UseInterval* interval = range->first_interval(); interval != nullptr;
       interval = interval->next()) {
    LifetimePosition start = interval->start();
    LifetimePosition end = interval->end();
    // If the interval starts at instruction end, then the first instruction
    // in the interval is the next one.
    int first_full_instruction = (start.IsGapPosition() || start.IsStart())
                                     ? start.ToInstructionIndex()
                                     : start.ToInstructionIndex() + 1;
    // If the interval ends in a gap or at instruction start, then the last
    // instruction is the previous one.
    int last_full_instruction = (end.IsGapPosition() || end.IsStart())
                                    ? end.ToInstructionIndex() - 1
                                    : end.ToInstructionIndex();

    for (int instruction_index = first_full_instruction;
         instruction_index <= last_full_instruction; ++instruction_index) {
      if (!code()->InstructionAt(instruction_index)->IsCall()) continue;

      LifetimePosition before =
          GetSplitPositionForInstruction(range, instruction_index);
      LiveRange* second_part =
          before.IsValid() ? Split(range, data(), before) : range;

      if (range != second_part) scheduler().Schedule(range);

      LifetimePosition after =
          FindSplitPositionAfterCall(second_part, instruction_index);

      if (after.IsValid()) {
        ret = Split(second_part, data(), after);
      } else {
        ret = nullptr;
      }
      Spill(second_part);
      return ret;
    }
  }
  return ret;
}


bool GreedyAllocator::TrySplitAroundCalls(LiveRange* range) {
  bool modified = false;

  while (range != nullptr) {
    LiveRange* remainder = GetRemainderAfterSplittingAroundFirstCall(range);
    // If we performed no modification, we're done.
    if (remainder == range) {
      break;
    }
    // We performed a modification.
    modified = true;
    range = remainder;
  }
  // If we have a remainder and we made modifications, it means the remainder
  // has no calls and we should schedule it for further processing. If we made
  // no modifications, we will just return false, because we want the algorithm
  // to make progress by trying some other heuristic.
  if (modified && range != nullptr) {
    DCHECK(!range->spilled());
    DCHECK(!range->HasRegisterAssigned());
    scheduler().Schedule(range);
  }
  return modified;
}


LifetimePosition GreedyAllocator::FindSplitPositionAfterCall(
    const LiveRange* range, int call_index) {
  LifetimePosition after_call =
      Max(range->Start(),
          LifetimePosition::GapFromInstructionIndex(call_index + 1));
  UsePosition* next_use = range->NextRegisterPosition(after_call);
  if (!next_use) return LifetimePosition::Invalid();

  LifetimePosition split_pos = FindOptimalSplitPos(after_call, next_use->pos());
  split_pos =
      GetSplitPositionForInstruction(range, split_pos.ToInstructionIndex());
  return split_pos;
}


LifetimePosition GreedyAllocator::FindSplitPositionBeforeLoops(
    LiveRange* range) {
  LifetimePosition end = range->End();
  if (end.ToInstructionIndex() >= code()->LastInstructionIndex()) {
    end =
        LifetimePosition::GapFromInstructionIndex(end.ToInstructionIndex() - 1);
  }
  LifetimePosition pos = FindOptimalSplitPos(range->Start(), end);
  pos = GetSplitPositionForInstruction(range, pos.ToInstructionIndex());
  return pos;
}


void GreedyAllocator::SplitOrSpillBlockedRange(LiveRange* range) {
  if (TrySplitAroundCalls(range)) return;

  LifetimePosition pos = FindSplitPositionBeforeLoops(range);

  if (!pos.IsValid()) pos = GetLastResortSplitPosition(range);
  if (pos.IsValid()) {
    LiveRange* tail = Split(range, data(), pos);
    DCHECK(tail != range);
    scheduler().Schedule(tail);
    scheduler().Schedule(range);
    return;
  }
  SpillRangeAsLastResort(range);
}


// Basic heuristic for advancing the algorithm, if any other splitting heuristic
// failed.
LifetimePosition GreedyAllocator::GetLastResortSplitPosition(
    const LiveRange* range) {
  LifetimePosition previous = range->Start();
  for (UsePosition *pos = range->NextRegisterPosition(previous); pos != nullptr;
       previous = previous.NextFullStart(),
                   pos = range->NextRegisterPosition(previous)) {
    LifetimePosition optimal = FindOptimalSplitPos(previous, pos->pos());
    LifetimePosition before =
        GetSplitPositionForInstruction(range, optimal.ToInstructionIndex());
    if (before.IsValid()) return before;
    LifetimePosition after = GetSplitPositionForInstruction(
        range, pos->pos().ToInstructionIndex() + 1);
    if (after.IsValid()) return after;
  }
  return LifetimePosition::Invalid();
}


bool GreedyAllocator::IsProgressPossible(const LiveRange* range) {
  return range->CanBeSpilled(range->Start()) ||
         GetLastResortSplitPosition(range).IsValid();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

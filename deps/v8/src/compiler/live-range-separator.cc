// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/live-range-separator.h"
#include "src/compiler/register-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {


#define TRACE(...)                             \
  do {                                         \
    if (FLAG_trace_alloc) PrintF(__VA_ARGS__); \
  } while (false)


namespace {


void CreateSplinter(TopLevelLiveRange *range, RegisterAllocationData *data,
                    LifetimePosition first_cut, LifetimePosition last_cut) {
  DCHECK(!range->IsSplinter());
  // We can ignore ranges that live solely in deferred blocks.
  // If a range ends right at the end of a deferred block, it is marked by
  // the range builder as ending at gap start of the next block - since the
  // end is a position where the variable isn't live. We need to take that
  // into consideration.
  LifetimePosition max_allowed_end = last_cut.NextFullStart();

  if (first_cut <= range->Start() && max_allowed_end >= range->End()) {
    return;
  }

  LifetimePosition start = Max(first_cut, range->Start());
  LifetimePosition end = Min(last_cut, range->End());

  if (start < end) {
    // Ensure the original range has a spill range associated, before it gets
    // splintered. Splinters will point to it. This way, when attempting to
    // reuse spill slots of splinters, during allocation, we avoid clobbering
    // such slots.
    if (range->MayRequireSpillRange()) {
      data->CreateSpillRangeForLiveRange(range);
    }
    if (range->splinter() == nullptr) {
      TopLevelLiveRange *splinter =
          data->NextLiveRange(range->representation());
      DCHECK_NULL(data->live_ranges()[splinter->vreg()]);
      data->live_ranges()[splinter->vreg()] = splinter;
      range->SetSplinter(splinter);
    }
    Zone *zone = data->allocation_zone();
    TRACE("creating splinter for range %d between %d and %d\n", range->vreg(),
          start.ToInstructionIndex(), end.ToInstructionIndex());
    range->Splinter(start, end, zone);
  }
}

void SetSlotUse(TopLevelLiveRange *range) {
  range->set_has_slot_use(false);
  for (const UsePosition *pos = range->first_pos();
       !range->has_slot_use() && pos != nullptr; pos = pos->next()) {
    if (pos->type() == UsePositionType::kRequiresSlot) {
      range->set_has_slot_use(true);
    }
  }
}

void SplinterLiveRange(TopLevelLiveRange *range, RegisterAllocationData *data) {
  const InstructionSequence *code = data->code();
  UseInterval *interval = range->first_interval();

  LifetimePosition first_cut = LifetimePosition::Invalid();
  LifetimePosition last_cut = LifetimePosition::Invalid();

  while (interval != nullptr) {
    UseInterval *next_interval = interval->next();
    const InstructionBlock *first_block =
        code->GetInstructionBlock(interval->FirstGapIndex());
    const InstructionBlock *last_block =
        code->GetInstructionBlock(interval->LastGapIndex());
    int first_block_nr = first_block->rpo_number().ToInt();
    int last_block_nr = last_block->rpo_number().ToInt();
    for (int block_id = first_block_nr; block_id <= last_block_nr; ++block_id) {
      const InstructionBlock *current_block =
          code->InstructionBlockAt(RpoNumber::FromInt(block_id));
      if (current_block->IsDeferred()) {
        if (!first_cut.IsValid()) {
          first_cut = LifetimePosition::GapFromInstructionIndex(
              current_block->first_instruction_index());
        }
        last_cut = LifetimePosition::GapFromInstructionIndex(
            current_block->last_instruction_index());
      } else {
        if (first_cut.IsValid()) {
          CreateSplinter(range, data, first_cut, last_cut);
          first_cut = LifetimePosition::Invalid();
          last_cut = LifetimePosition::Invalid();
        }
      }
    }
    interval = next_interval;
  }
  // When the range ends in deferred blocks, first_cut will be valid here.
  // Splinter from there to the last instruction that was in a deferred block.
  if (first_cut.IsValid()) {
    CreateSplinter(range, data, first_cut, last_cut);
  }

  // Redo has_slot_use
  if (range->has_slot_use() && range->splinter() != nullptr) {
    SetSlotUse(range);
    SetSlotUse(range->splinter());
  }
}

}  // namespace


void LiveRangeSeparator::Splinter() {
  size_t virt_reg_count = data()->live_ranges().size();
  for (size_t vreg = 0; vreg < virt_reg_count; ++vreg) {
    TopLevelLiveRange *range = data()->live_ranges()[vreg];
    if (range == nullptr || range->IsEmpty() || range->IsSplinter()) {
      continue;
    }
    int first_instr = range->first_interval()->FirstGapIndex();
    if (!data()->code()->GetInstructionBlock(first_instr)->IsDeferred()) {
      SplinterLiveRange(range, data());
    }
  }
}


void LiveRangeMerger::MarkRangesSpilledInDeferredBlocks() {
  const InstructionSequence *code = data()->code();
  for (TopLevelLiveRange *top : data()->live_ranges()) {
    if (top == nullptr || top->IsEmpty() || top->splinter() == nullptr ||
        top->HasSpillOperand() || !top->splinter()->HasSpillRange()) {
      continue;
    }

    LiveRange *child = top;
    for (; child != nullptr; child = child->next()) {
      if (child->spilled() ||
          child->NextSlotPosition(child->Start()) != nullptr) {
        break;
      }
    }
    if (child == nullptr) {
      top->TreatAsSpilledInDeferredBlock(data()->allocation_zone(),
                                         code->InstructionBlockCount());
    }
  }
}


void LiveRangeMerger::Merge() {
  MarkRangesSpilledInDeferredBlocks();

  int live_range_count = static_cast<int>(data()->live_ranges().size());
  for (int i = 0; i < live_range_count; ++i) {
    TopLevelLiveRange *range = data()->live_ranges()[i];
    if (range == nullptr || range->IsEmpty() || !range->IsSplinter()) {
      continue;
    }
    TopLevelLiveRange *splinter_parent = range->splintered_from();

    int to_remove = range->vreg();
    splinter_parent->Merge(range, data()->allocation_zone());
    data()->live_ranges()[to_remove] = nullptr;
  }
}

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8

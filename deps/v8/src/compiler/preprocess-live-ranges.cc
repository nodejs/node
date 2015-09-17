// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/preprocess-live-ranges.h"
#include "src/compiler/register-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {


#define TRACE(...)                             \
  do {                                         \
    if (FLAG_trace_alloc) PrintF(__VA_ARGS__); \
  } while (false)


namespace {

LiveRange* Split(LiveRange* range, RegisterAllocationData* data,
                 LifetimePosition pos) {
  DCHECK(range->Start() < pos && pos < range->End());
  DCHECK(pos.IsStart() || pos.IsGapPosition() ||
         (data->code()
              ->GetInstructionBlock(pos.ToInstructionIndex())
              ->last_instruction_index() != pos.ToInstructionIndex()));
  LiveRange* result = data->NewChildRangeFor(range);
  range->SplitAt(pos, result, data->allocation_zone());
  TRACE("Split range %d(v%d) @%d => %d.\n", range->id(),
        range->TopLevel()->id(), pos.ToInstructionIndex(), result->id());
  return result;
}


LifetimePosition GetSplitPositionForInstruction(const LiveRange* range,
                                                int instruction_index) {
  LifetimePosition ret = LifetimePosition::Invalid();

  ret = LifetimePosition::GapFromInstructionIndex(instruction_index);
  if (range->Start() >= ret || ret >= range->End()) {
    return LifetimePosition::Invalid();
  }
  return ret;
}


LiveRange* SplitRangeAfterBlock(LiveRange* range, RegisterAllocationData* data,
                                const InstructionBlock* block) {
  const InstructionSequence* code = data->code();
  int last_index = block->last_instruction_index();
  int outside_index = static_cast<int>(code->instructions().size());
  bool has_handler = false;
  for (auto successor_id : block->successors()) {
    const InstructionBlock* successor = code->InstructionBlockAt(successor_id);
    if (successor->IsHandler()) {
      has_handler = true;
    }
    outside_index = Min(outside_index, successor->first_instruction_index());
  }
  int split_at = has_handler ? outside_index : last_index;
  LifetimePosition after_block =
      GetSplitPositionForInstruction(range, split_at);

  if (after_block.IsValid()) {
    return Split(range, data, after_block);
  }

  return range;
}


int GetFirstInstructionIndex(const UseInterval* interval) {
  int ret = interval->start().ToInstructionIndex();
  if (!interval->start().IsGapPosition() && !interval->start().IsStart()) {
    ++ret;
  }
  return ret;
}


bool DoesSubsequenceClobber(const InstructionSequence* code, int start,
                            int end) {
  for (int i = start; i <= end; ++i) {
    if (code->InstructionAt(i)->IsCall()) return true;
  }
  return false;
}


void SplitRangeAtDeferredBlocksWithCalls(LiveRange* range,
                                         RegisterAllocationData* data) {
  DCHECK(!range->IsFixed());
  DCHECK(!range->spilled());
  if (range->TopLevel()->HasSpillOperand()) {
    TRACE(
        "Skipping deferred block analysis for live range %d because it has a "
        "spill operand.\n",
        range->TopLevel()->id());
    return;
  }

  const InstructionSequence* code = data->code();
  LiveRange* current_subrange = range;

  UseInterval* interval = current_subrange->first_interval();

  while (interval != nullptr) {
    int first_index = GetFirstInstructionIndex(interval);
    int last_index = interval->end().ToInstructionIndex();

    if (last_index > code->LastInstructionIndex()) {
      last_index = code->LastInstructionIndex();
    }

    interval = interval->next();

    for (int index = first_index; index <= last_index;) {
      const InstructionBlock* block = code->GetInstructionBlock(index);
      int last_block_index = static_cast<int>(block->last_instruction_index());
      int last_covered_index = Min(last_index, last_block_index);
      int working_index = index;
      index = block->last_instruction_index() + 1;

      if (!block->IsDeferred() ||
          !DoesSubsequenceClobber(code, working_index, last_covered_index)) {
        continue;
      }

      TRACE("Deferred block B%d clobbers range %d(v%d).\n",
            block->rpo_number().ToInt(), current_subrange->id(),
            current_subrange->TopLevel()->id());
      LifetimePosition block_start =
          GetSplitPositionForInstruction(current_subrange, working_index);
      LiveRange* block_and_after = nullptr;
      if (block_start.IsValid()) {
        block_and_after = Split(current_subrange, data, block_start);
      } else {
        block_and_after = current_subrange;
      }
      LiveRange* next = SplitRangeAfterBlock(block_and_after, data, block);
      if (next != current_subrange) interval = next->first_interval();
      current_subrange = next;
      break;
    }
  }
}
}


void PreprocessLiveRanges::PreprocessRanges() {
  SplitRangesAroundDeferredBlocks();
}


void PreprocessLiveRanges::SplitRangesAroundDeferredBlocks() {
  size_t live_range_count = data()->live_ranges().size();
  for (size_t i = 0; i < live_range_count; i++) {
    LiveRange* range = data()->live_ranges()[i];
    if (range != nullptr && !range->IsEmpty() && !range->spilled() &&
        !range->IsFixed() && !range->IsChild()) {
      SplitRangeAtDeferredBlocksWithCalls(range, data());
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

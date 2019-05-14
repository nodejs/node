// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/cfg.h"

#include "src/torque/type-oracle.h"

namespace v8 {
namespace internal {
namespace torque {

void Block::SetInputTypes(const Stack<const Type*>& input_types) {
  if (!input_types_) {
    input_types_ = input_types;
    return;
  } else if (*input_types_ == input_types) {
    return;
  }

  DCHECK_EQ(input_types.Size(), input_types_->Size());
  Stack<const Type*> merged_types;
  bool widened = false;
  auto c2_iterator = input_types.begin();
  for (const Type* c1 : *input_types_) {
    const Type* merged_type = TypeOracle::GetUnionType(c1, *c2_iterator++);
    if (!merged_type->IsSubtypeOf(c1)) {
      widened = true;
    }
    merged_types.Push(merged_type);
  }
  if (merged_types.Size() == input_types_->Size()) {
    if (widened) {
      input_types_ = merged_types;
      Retype();
    }
    return;
  }

  std::stringstream error;
  error << "incompatible types at branch:\n";
  for (intptr_t i = std::max(input_types_->Size(), input_types.Size()) - 1;
       i >= 0; --i) {
    base::Optional<const Type*> left;
    base::Optional<const Type*> right;
    if (static_cast<size_t>(i) < input_types.Size()) {
      left = input_types.Peek(BottomOffset{static_cast<size_t>(i)});
    }
    if (static_cast<size_t>(i) < input_types_->Size()) {
      right = input_types_->Peek(BottomOffset{static_cast<size_t>(i)});
    }
    if (left && right && *left == *right) {
      error << **left << "\n";
    } else {
      if (left) {
        error << **left;
      } else {
        error << "/*missing*/";
      }
      error << "   =>   ";
      if (right) {
        error << **right;
      } else {
        error << "/*missing*/";
      }
      error << "\n";
    }
  }
  ReportError(error.str());
}

void CfgAssembler::Bind(Block* block) {
  DCHECK(current_block_->IsComplete());
  DCHECK(block->instructions().empty());
  DCHECK(block->HasInputTypes());
  current_block_ = block;
  current_stack_ = block->InputTypes();
  cfg_.PlaceBlock(block);
}

void CfgAssembler::Goto(Block* block) {
  if (block->HasInputTypes()) {
    DropTo(block->InputTypes().AboveTop());
  }
  Emit(GotoInstruction{block});
}

StackRange CfgAssembler::Goto(Block* block, size_t preserved_slots) {
  DCHECK(block->HasInputTypes());
  DCHECK_GE(CurrentStack().Size(), block->InputTypes().Size());
  Emit(DeleteRangeInstruction{
      StackRange{block->InputTypes().AboveTop() - preserved_slots,
                 CurrentStack().AboveTop() - preserved_slots}});
  StackRange preserved_slot_range = TopRange(preserved_slots);
  Emit(GotoInstruction{block});
  return preserved_slot_range;
}

void CfgAssembler::Branch(Block* if_true, Block* if_false) {
  Emit(BranchInstruction{if_true, if_false});
}

// Delete the specified range of slots, moving upper slots to fill the gap.
void CfgAssembler::DeleteRange(StackRange range) {
  DCHECK_LE(range.end(), current_stack_.AboveTop());
  if (range.Size() == 0) return;
  Emit(DeleteRangeInstruction{range});
}

void CfgAssembler::DropTo(BottomOffset new_level) {
  DeleteRange(StackRange{new_level, CurrentStack().AboveTop()});
}

StackRange CfgAssembler::Peek(StackRange range,
                              base::Optional<const Type*> type) {
  std::vector<const Type*> lowered_types;
  if (type) {
    lowered_types = LowerType(*type);
    DCHECK_EQ(lowered_types.size(), range.Size());
  }
  for (size_t i = 0; i < range.Size(); ++i) {
    Emit(PeekInstruction{
        range.begin() + i,
        type ? lowered_types[i] : base::Optional<const Type*>{}});
  }
  return TopRange(range.Size());
}

void CfgAssembler::Poke(StackRange destination, StackRange origin,
                        base::Optional<const Type*> type) {
  DCHECK_EQ(destination.Size(), origin.Size());
  DCHECK_LE(destination.end(), origin.begin());
  DCHECK_EQ(origin.end(), CurrentStack().AboveTop());
  std::vector<const Type*> lowered_types;
  if (type) {
    lowered_types = LowerType(*type);
    DCHECK_EQ(lowered_types.size(), origin.Size());
  }
  for (intptr_t i = origin.Size() - 1; i >= 0; --i) {
    Emit(PokeInstruction{
        destination.begin() + i,
        type ? lowered_types[i] : base::Optional<const Type*>{}});
  }
}

void CfgAssembler::Print(std::string s) {
  Emit(PrintConstantStringInstruction{std::move(s)});
}

void CfgAssembler::AssertionFailure(std::string message) {
  Emit(AbortInstruction{AbortInstruction::Kind::kAssertionFailure,
                        std::move(message)});
}

void CfgAssembler::Unreachable() {
  Emit(AbortInstruction{AbortInstruction::Kind::kUnreachable});
}

void CfgAssembler::DebugBreak() {
  Emit(AbortInstruction{AbortInstruction::Kind::kDebugBreak});
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

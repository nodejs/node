// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/adapters.h"
#include "src/compiler/frame-elider.h"

namespace v8 {
namespace internal {
namespace compiler {

FrameElider::FrameElider(InstructionSequence* code) : code_(code) {}

void FrameElider::Run() {
  MarkBlocks();
  PropagateMarks();
  MarkDeConstruction();
}


void FrameElider::MarkBlocks() {
  for (InstructionBlock* block : instruction_blocks()) {
    if (block->needs_frame()) continue;
    for (int i = block->code_start(); i < block->code_end(); ++i) {
      const Instruction* instr = InstructionAt(i);
      if (instr->IsCall() || instr->IsDeoptimizeCall() ||
          instr->arch_opcode() == ArchOpcode::kArchStackPointer ||
          instr->arch_opcode() == ArchOpcode::kArchFramePointer) {
        block->mark_needs_frame();
        break;
      }
    }
  }
}


void FrameElider::PropagateMarks() {
  while (PropagateInOrder() || PropagateReversed()) {
  }
}


void FrameElider::MarkDeConstruction() {
  for (InstructionBlock* block : instruction_blocks()) {
    if (block->needs_frame()) {
      // Special case: The start block needs a frame.
      if (block->predecessors().empty()) {
        block->mark_must_construct_frame();
      }
      // Find "frame -> no frame" transitions, inserting frame
      // deconstructions.
      for (RpoNumber& succ : block->successors()) {
        if (!InstructionBlockAt(succ)->needs_frame()) {
          DCHECK_EQ(1U, block->SuccessorCount());
          const Instruction* last =
              InstructionAt(block->last_instruction_index());
          if (last->IsThrow() || last->IsTailCall() ||
              last->IsDeoptimizeCall()) {
            // We need to keep the frame if we exit the block through any
            // of these.
            continue;
          }
          // The only cases when we need to deconstruct are ret and jump.
          DCHECK(last->IsRet() || last->IsJump());
          block->mark_must_deconstruct_frame();
        }
      }
    } else {
      // Find "no frame -> frame" transitions, inserting frame constructions.
      for (RpoNumber& succ : block->successors()) {
        if (InstructionBlockAt(succ)->needs_frame()) {
          DCHECK_NE(1U, block->SuccessorCount());
          InstructionBlockAt(succ)->mark_must_construct_frame();
        }
      }
    }
  }
}


bool FrameElider::PropagateInOrder() {
  bool changed = false;
  for (InstructionBlock* block : instruction_blocks()) {
    changed |= PropagateIntoBlock(block);
  }
  return changed;
}


bool FrameElider::PropagateReversed() {
  bool changed = false;
  for (InstructionBlock* block : base::Reversed(instruction_blocks())) {
    changed |= PropagateIntoBlock(block);
  }
  return changed;
}


bool FrameElider::PropagateIntoBlock(InstructionBlock* block) {
  // Already marked, nothing to do...
  if (block->needs_frame()) return false;

  // Never mark the dummy end node, otherwise we might incorrectly decide to
  // put frame deconstruction code there later,
  if (block->successors().empty()) return false;

  // Propagate towards the end ("downwards") if there is a predecessor needing
  // a frame, but don't "bleed" from deferred code to non-deferred code.
  for (RpoNumber& pred : block->predecessors()) {
    if (InstructionBlockAt(pred)->needs_frame() &&
        (!InstructionBlockAt(pred)->IsDeferred() || block->IsDeferred())) {
      block->mark_needs_frame();
      return true;
    }
  }

  // Propagate towards start ("upwards")
  bool need_frame_successors = false;
  if (block->SuccessorCount() == 1) {
    // For single successors, propagate the needs_frame information.
    need_frame_successors =
        InstructionBlockAt(block->successors()[0])->needs_frame();
  } else {
    // For multiple successors, each successor must only have a single
    // predecessor (because the graph is in edge-split form), so each successor
    // can independently create/dismantle a frame if needed. Given this
    // independent control, only propagate needs_frame if all non-deferred
    // blocks need a frame.
    for (RpoNumber& succ : block->successors()) {
      InstructionBlock* successor_block = InstructionBlockAt(succ);
      DCHECK_EQ(1, successor_block->PredecessorCount());
      if (!successor_block->IsDeferred()) {
        if (successor_block->needs_frame()) {
          need_frame_successors = true;
        } else {
          return false;
        }
      }
    }
  }
  if (need_frame_successors) {
    block->mark_needs_frame();
    return true;
  } else {
    return false;
  }
}


const InstructionBlocks& FrameElider::instruction_blocks() const {
  return code_->instruction_blocks();
}


InstructionBlock* FrameElider::InstructionBlockAt(RpoNumber rpo_number) const {
  return code_->InstructionBlockAt(rpo_number);
}


Instruction* FrameElider::InstructionAt(int index) const {
  return code_->InstructionAt(index);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

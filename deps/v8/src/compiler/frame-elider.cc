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
  for (auto block : instruction_blocks()) {
    if (block->needs_frame()) continue;
    for (auto i = block->code_start(); i < block->code_end(); ++i) {
      if (InstructionAt(i)->IsCall() ||
          InstructionAt(i)->opcode() == ArchOpcode::kArchDeoptimize) {
        block->mark_needs_frame();
        break;
      }
    }
  }
}


void FrameElider::PropagateMarks() {
  while (PropagateInOrder() && PropagateReversed()) {
  }
}


void FrameElider::MarkDeConstruction() {
  for (auto block : instruction_blocks()) {
    if (block->needs_frame()) {
      // Special case: The start block needs a frame.
      if (block->predecessors().empty()) {
        block->mark_must_construct_frame();
      }
      // Find "frame -> no frame" transitions, inserting frame
      // deconstructions.
      for (auto succ : block->successors()) {
        if (!InstructionBlockAt(succ)->needs_frame()) {
          DCHECK_EQ(1U, block->SuccessorCount());
          block->mark_must_deconstruct_frame();
        }
      }
    } else {
      // Find "no frame -> frame" transitions, inserting frame constructions.
      for (auto succ : block->successors()) {
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
  for (auto block : instruction_blocks()) {
    changed |= PropagateIntoBlock(block);
  }
  return changed;
}


bool FrameElider::PropagateReversed() {
  bool changed = false;
  for (auto block : base::Reversed(instruction_blocks())) {
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
  for (auto pred : block->predecessors()) {
    if (InstructionBlockAt(pred)->needs_frame() &&
        (!InstructionBlockAt(pred)->IsDeferred() || block->IsDeferred())) {
      block->mark_needs_frame();
      return true;
    }
  }

  // Propagate towards start ("upwards") if there are successors and all of
  // them need a frame.
  for (auto succ : block->successors()) {
    if (!InstructionBlockAt(succ)->needs_frame()) return false;
  }
  block->mark_needs_frame();
  return true;
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

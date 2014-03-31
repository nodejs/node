// Copyright 2013 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "arm64/lithium-gap-resolver-arm64.h"
#include "arm64/lithium-codegen-arm64.h"

namespace v8 {
namespace internal {

// We use the root register to spill a value while breaking a cycle in parallel
// moves. We don't need access to roots while resolving the move list and using
// the root register has two advantages:
//  - It is not in crankshaft allocatable registers list, so it can't interfere
//    with any of the moves we are resolving.
//  - We don't need to push it on the stack, as we can reload it with its value
//    once we have resolved a cycle.
#define kSavedValue root

// We use the MacroAssembler floating-point scratch register to break a cycle
// involving double values as the MacroAssembler will not need it for the
// operations performed by the gap resolver.
#define kSavedDoubleValue fp_scratch


LGapResolver::LGapResolver(LCodeGen* owner)
    : cgen_(owner), moves_(32, owner->zone()), root_index_(0), in_cycle_(false),
      saved_destination_(NULL), need_to_restore_root_(false) { }


#define __ ACCESS_MASM(cgen_->masm())

void LGapResolver::Resolve(LParallelMove* parallel_move) {
  ASSERT(moves_.is_empty());

  // Build up a worklist of moves.
  BuildInitialMoveList(parallel_move);

  for (int i = 0; i < moves_.length(); ++i) {
    LMoveOperands move = moves_[i];

    // Skip constants to perform them last. They don't block other moves
    // and skipping such moves with register destinations keeps those
    // registers free for the whole algorithm.
    if (!move.IsEliminated() && !move.source()->IsConstantOperand()) {
      root_index_ = i;  // Any cycle is found when we reach this move again.
      PerformMove(i);
      if (in_cycle_) RestoreValue();
    }
  }

  // Perform the moves with constant sources.
  for (int i = 0; i < moves_.length(); ++i) {
    LMoveOperands move = moves_[i];

    if (!move.IsEliminated()) {
      ASSERT(move.source()->IsConstantOperand());
      EmitMove(i);
    }
  }

  if (need_to_restore_root_) {
    ASSERT(kSavedValue.Is(root));
    __ InitializeRootRegister();
    need_to_restore_root_ = false;
  }

  moves_.Rewind(0);
}


void LGapResolver::BuildInitialMoveList(LParallelMove* parallel_move) {
  // Perform a linear sweep of the moves to add them to the initial list of
  // moves to perform, ignoring any move that is redundant (the source is
  // the same as the destination, the destination is ignored and
  // unallocated, or the move was already eliminated).
  const ZoneList<LMoveOperands>* moves = parallel_move->move_operands();
  for (int i = 0; i < moves->length(); ++i) {
    LMoveOperands move = moves->at(i);
    if (!move.IsRedundant()) moves_.Add(move, cgen_->zone());
  }
  Verify();
}


void LGapResolver::PerformMove(int index) {
  // Each call to this function performs a move and deletes it from the move
  // graph. We first recursively perform any move blocking this one. We
  // mark a move as "pending" on entry to PerformMove in order to detect
  // cycles in the move graph.
  LMoveOperands& current_move = moves_[index];

  ASSERT(!current_move.IsPending());
  ASSERT(!current_move.IsRedundant());

  // Clear this move's destination to indicate a pending move.  The actual
  // destination is saved in a stack allocated local. Multiple moves can
  // be pending because this function is recursive.
  ASSERT(current_move.source() != NULL);  // Otherwise it will look eliminated.
  LOperand* destination = current_move.destination();
  current_move.set_destination(NULL);

  // Perform a depth-first traversal of the move graph to resolve
  // dependencies. Any unperformed, unpending move with a source the same
  // as this one's destination blocks this one so recursively perform all
  // such moves.
  for (int i = 0; i < moves_.length(); ++i) {
    LMoveOperands other_move = moves_[i];
    if (other_move.Blocks(destination) && !other_move.IsPending()) {
      PerformMove(i);
      // If there is a blocking, pending move it must be moves_[root_index_]
      // and all other moves with the same source as moves_[root_index_] are
      // sucessfully executed (because they are cycle-free) by this loop.
    }
  }

  // We are about to resolve this move and don't need it marked as
  // pending, so restore its destination.
  current_move.set_destination(destination);

  // The move may be blocked on a pending move, which must be the starting move.
  // In this case, we have a cycle, and we save the source of this move to
  // a scratch register to break it.
  LMoveOperands other_move = moves_[root_index_];
  if (other_move.Blocks(destination)) {
    ASSERT(other_move.IsPending());
    BreakCycle(index);
    return;
  }

  // This move is no longer blocked.
  EmitMove(index);
}


void LGapResolver::Verify() {
#ifdef ENABLE_SLOW_ASSERTS
  // No operand should be the destination for more than one move.
  for (int i = 0; i < moves_.length(); ++i) {
    LOperand* destination = moves_[i].destination();
    for (int j = i + 1; j < moves_.length(); ++j) {
      SLOW_ASSERT(!destination->Equals(moves_[j].destination()));
    }
  }
#endif
}


void LGapResolver::BreakCycle(int index) {
  ASSERT(moves_[index].destination()->Equals(moves_[root_index_].source()));
  ASSERT(!in_cycle_);

  // We use registers which are not allocatable by crankshaft to break the cycle
  // to be sure they don't interfere with the moves we are resolving.
  ASSERT(!kSavedValue.IsAllocatable());
  ASSERT(!kSavedDoubleValue.IsAllocatable());

  // We save in a register the source of that move and we remember its
  // destination. Then we mark this move as resolved so the cycle is
  // broken and we can perform the other moves.
  in_cycle_ = true;
  LOperand* source = moves_[index].source();
  saved_destination_ = moves_[index].destination();

  if (source->IsRegister()) {
    need_to_restore_root_ = true;
    __ Mov(kSavedValue, cgen_->ToRegister(source));
  } else if (source->IsStackSlot()) {
    need_to_restore_root_ = true;
    __ Ldr(kSavedValue, cgen_->ToMemOperand(source));
  } else if (source->IsDoubleRegister()) {
    ASSERT(cgen_->masm()->FPTmpList()->IncludesAliasOf(kSavedDoubleValue));
    cgen_->masm()->FPTmpList()->Remove(kSavedDoubleValue);
    __ Fmov(kSavedDoubleValue, cgen_->ToDoubleRegister(source));
  } else if (source->IsDoubleStackSlot()) {
    ASSERT(cgen_->masm()->FPTmpList()->IncludesAliasOf(kSavedDoubleValue));
    cgen_->masm()->FPTmpList()->Remove(kSavedDoubleValue);
    __ Ldr(kSavedDoubleValue, cgen_->ToMemOperand(source));
  } else {
    UNREACHABLE();
  }

  // Mark this move as resolved.
  // This move will be actually performed by moving the saved value to this
  // move's destination in LGapResolver::RestoreValue().
  moves_[index].Eliminate();
}


void LGapResolver::RestoreValue() {
  ASSERT(in_cycle_);
  ASSERT(saved_destination_ != NULL);

  if (saved_destination_->IsRegister()) {
    __ Mov(cgen_->ToRegister(saved_destination_), kSavedValue);
  } else if (saved_destination_->IsStackSlot()) {
    __ Str(kSavedValue, cgen_->ToMemOperand(saved_destination_));
  } else if (saved_destination_->IsDoubleRegister()) {
    __ Fmov(cgen_->ToDoubleRegister(saved_destination_), kSavedDoubleValue);
    cgen_->masm()->FPTmpList()->Combine(kSavedDoubleValue);
  } else if (saved_destination_->IsDoubleStackSlot()) {
    __ Str(kSavedDoubleValue, cgen_->ToMemOperand(saved_destination_));
    cgen_->masm()->FPTmpList()->Combine(kSavedDoubleValue);
  } else {
    UNREACHABLE();
  }

  in_cycle_ = false;
  saved_destination_ = NULL;
}


void LGapResolver::EmitMove(int index) {
  LOperand* source = moves_[index].source();
  LOperand* destination = moves_[index].destination();

  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.

  if (source->IsRegister()) {
    Register source_register = cgen_->ToRegister(source);
    if (destination->IsRegister()) {
      __ Mov(cgen_->ToRegister(destination), source_register);
    } else {
      ASSERT(destination->IsStackSlot());
      __ Str(source_register, cgen_->ToMemOperand(destination));
    }

  } else if (source->IsStackSlot()) {
    MemOperand source_operand = cgen_->ToMemOperand(source);
    if (destination->IsRegister()) {
      __ Ldr(cgen_->ToRegister(destination), source_operand);
    } else {
      ASSERT(destination->IsStackSlot());
      EmitStackSlotMove(index);
    }

  } else if (source->IsConstantOperand()) {
    LConstantOperand* constant_source = LConstantOperand::cast(source);
    if (destination->IsRegister()) {
      Register dst = cgen_->ToRegister(destination);
      if (cgen_->IsSmi(constant_source)) {
        __ Mov(dst, cgen_->ToSmi(constant_source));
      } else if (cgen_->IsInteger32Constant(constant_source)) {
        __ Mov(dst, cgen_->ToInteger32(constant_source));
      } else {
        __ LoadObject(dst, cgen_->ToHandle(constant_source));
      }
    } else if (destination->IsDoubleRegister()) {
      DoubleRegister result = cgen_->ToDoubleRegister(destination);
      __ Fmov(result, cgen_->ToDouble(constant_source));
    } else {
      ASSERT(destination->IsStackSlot());
      ASSERT(!in_cycle_);  // Constant moves happen after all cycles are gone.
      need_to_restore_root_ = true;
      if (cgen_->IsSmi(constant_source)) {
        __ Mov(kSavedValue, cgen_->ToSmi(constant_source));
      } else if (cgen_->IsInteger32Constant(constant_source)) {
        __ Mov(kSavedValue, cgen_->ToInteger32(constant_source));
      } else {
        __ LoadObject(kSavedValue, cgen_->ToHandle(constant_source));
      }
      __ Str(kSavedValue, cgen_->ToMemOperand(destination));
    }

  } else if (source->IsDoubleRegister()) {
    DoubleRegister src = cgen_->ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      __ Fmov(cgen_->ToDoubleRegister(destination), src);
    } else {
      ASSERT(destination->IsDoubleStackSlot());
      __ Str(src, cgen_->ToMemOperand(destination));
    }

  } else if (source->IsDoubleStackSlot()) {
    MemOperand src = cgen_->ToMemOperand(source);
    if (destination->IsDoubleRegister()) {
      __ Ldr(cgen_->ToDoubleRegister(destination), src);
    } else {
      ASSERT(destination->IsDoubleStackSlot());
      EmitStackSlotMove(index);
    }

  } else {
    UNREACHABLE();
  }

  // The move has been emitted, we can eliminate it.
  moves_[index].Eliminate();
}


void LGapResolver::EmitStackSlotMove(int index) {
  // We need a temp register to perform a stack slot to stack slot move, and
  // the register must not be involved in breaking cycles.

  // Use the Crankshaft double scratch register as the temporary.
  DoubleRegister temp = crankshaft_fp_scratch;

  LOperand* src = moves_[index].source();
  LOperand* dst = moves_[index].destination();

  ASSERT(src->IsStackSlot());
  ASSERT(dst->IsStackSlot());
  __ Ldr(temp, cgen_->ToMemOperand(src));
  __ Str(temp, cgen_->ToMemOperand(dst));
}

} }  // namespace v8::internal

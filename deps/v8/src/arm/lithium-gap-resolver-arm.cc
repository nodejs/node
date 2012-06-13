// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "arm/lithium-gap-resolver-arm.h"
#include "arm/lithium-codegen-arm.h"

namespace v8 {
namespace internal {

static const Register kSavedValueRegister = { 9 };

LGapResolver::LGapResolver(LCodeGen* owner)
    : cgen_(owner), moves_(32, owner->zone()), root_index_(0), in_cycle_(false),
      saved_destination_(NULL) { }


void LGapResolver::Resolve(LParallelMove* parallel_move) {
  ASSERT(moves_.is_empty());
  // Build up a worklist of moves.
  BuildInitialMoveList(parallel_move);

  for (int i = 0; i < moves_.length(); ++i) {
    LMoveOperands move = moves_[i];
    // Skip constants to perform them last.  They don't block other moves
    // and skipping such moves with register destinations keeps those
    // registers free for the whole algorithm.
    if (!move.IsEliminated() && !move.source()->IsConstantOperand()) {
      root_index_ = i;  // Any cycle is found when by reaching this move again.
      PerformMove(i);
      if (in_cycle_) {
        RestoreValue();
      }
    }
  }

  // Perform the moves with constant sources.
  for (int i = 0; i < moves_.length(); ++i) {
    if (!moves_[i].IsEliminated()) {
      ASSERT(moves_[i].source()->IsConstantOperand());
      EmitMove(i);
    }
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
  // graph.  We first recursively perform any move blocking this one.  We
  // mark a move as "pending" on entry to PerformMove in order to detect
  // cycles in the move graph.

  // We can only find a cycle, when doing a depth-first traversal of moves,
  // be encountering the starting move again. So by spilling the source of
  // the starting move, we break the cycle.  All moves are then unblocked,
  // and the starting move is completed by writing the spilled value to
  // its destination.  All other moves from the spilled source have been
  // completed prior to breaking the cycle.
  // An additional complication is that moves to MemOperands with large
  // offsets (more than 1K or 4K) require us to spill this spilled value to
  // the stack, to free up the register.
  ASSERT(!moves_[index].IsPending());
  ASSERT(!moves_[index].IsRedundant());

  // Clear this move's destination to indicate a pending move.  The actual
  // destination is saved in a stack allocated local.  Multiple moves can
  // be pending because this function is recursive.
  ASSERT(moves_[index].source() != NULL);  // Or else it will look eliminated.
  LOperand* destination = moves_[index].destination();
  moves_[index].set_destination(NULL);

  // Perform a depth-first traversal of the move graph to resolve
  // dependencies.  Any unperformed, unpending move with a source the same
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
  moves_[index].set_destination(destination);

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

#define __ ACCESS_MASM(cgen_->masm())

void LGapResolver::BreakCycle(int index) {
  // We save in a register the value that should end up in the source of
  // moves_[root_index].  After performing all moves in the tree rooted
  // in that move, we save the value to that source.
  ASSERT(moves_[index].destination()->Equals(moves_[root_index_].source()));
  ASSERT(!in_cycle_);
  in_cycle_ = true;
  LOperand* source = moves_[index].source();
  saved_destination_ = moves_[index].destination();
  if (source->IsRegister()) {
    __ mov(kSavedValueRegister, cgen_->ToRegister(source));
  } else if (source->IsStackSlot()) {
    __ ldr(kSavedValueRegister, cgen_->ToMemOperand(source));
  } else if (source->IsDoubleRegister()) {
    __ vmov(kScratchDoubleReg, cgen_->ToDoubleRegister(source));
  } else if (source->IsDoubleStackSlot()) {
    __ vldr(kScratchDoubleReg, cgen_->ToMemOperand(source));
  } else {
    UNREACHABLE();
  }
  // This move will be done by restoring the saved value to the destination.
  moves_[index].Eliminate();
}


void LGapResolver::RestoreValue() {
  ASSERT(in_cycle_);
  ASSERT(saved_destination_ != NULL);

  // Spilled value is in kSavedValueRegister or kSavedDoubleValueRegister.
  if (saved_destination_->IsRegister()) {
    __ mov(cgen_->ToRegister(saved_destination_), kSavedValueRegister);
  } else if (saved_destination_->IsStackSlot()) {
    __ str(kSavedValueRegister, cgen_->ToMemOperand(saved_destination_));
  } else if (saved_destination_->IsDoubleRegister()) {
    __ vmov(cgen_->ToDoubleRegister(saved_destination_), kScratchDoubleReg);
  } else if (saved_destination_->IsDoubleStackSlot()) {
    __ vstr(kScratchDoubleReg, cgen_->ToMemOperand(saved_destination_));
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
      __ mov(cgen_->ToRegister(destination), source_register);
    } else {
      ASSERT(destination->IsStackSlot());
      __ str(source_register, cgen_->ToMemOperand(destination));
    }

  } else if (source->IsStackSlot()) {
    MemOperand source_operand = cgen_->ToMemOperand(source);
    if (destination->IsRegister()) {
      __ ldr(cgen_->ToRegister(destination), source_operand);
    } else {
      ASSERT(destination->IsStackSlot());
      MemOperand destination_operand = cgen_->ToMemOperand(destination);
      if (in_cycle_) {
        if (!destination_operand.OffsetIsUint12Encodable()) {
          // ip is overwritten while saving the value to the destination.
          // Therefore we can't use ip.  It is OK if the read from the source
          // destroys ip, since that happens before the value is read.
          __ vldr(kScratchDoubleReg.low(), source_operand);
          __ vstr(kScratchDoubleReg.low(), destination_operand);
        } else {
          __ ldr(ip, source_operand);
          __ str(ip, destination_operand);
        }
      } else {
        __ ldr(kSavedValueRegister, source_operand);
        __ str(kSavedValueRegister, destination_operand);
      }
    }

  } else if (source->IsConstantOperand()) {
    LConstantOperand* constant_source = LConstantOperand::cast(source);
    if (destination->IsRegister()) {
      Register dst = cgen_->ToRegister(destination);
      if (cgen_->IsInteger32(constant_source)) {
        __ mov(dst, Operand(cgen_->ToInteger32(constant_source)));
      } else {
        __ LoadObject(dst, cgen_->ToHandle(constant_source));
      }
    } else {
      ASSERT(destination->IsStackSlot());
      ASSERT(!in_cycle_);  // Constant moves happen after all cycles are gone.
      if (cgen_->IsInteger32(constant_source)) {
        __ mov(kSavedValueRegister,
               Operand(cgen_->ToInteger32(constant_source)));
      } else {
        __ LoadObject(kSavedValueRegister,
                      cgen_->ToHandle(constant_source));
      }
      __ str(kSavedValueRegister, cgen_->ToMemOperand(destination));
    }

  } else if (source->IsDoubleRegister()) {
    DoubleRegister source_register = cgen_->ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      __ vmov(cgen_->ToDoubleRegister(destination), source_register);
    } else {
      ASSERT(destination->IsDoubleStackSlot());
      __ vstr(source_register, cgen_->ToMemOperand(destination));
    }

  } else if (source->IsDoubleStackSlot()) {
    MemOperand source_operand = cgen_->ToMemOperand(source);
    if (destination->IsDoubleRegister()) {
      __ vldr(cgen_->ToDoubleRegister(destination), source_operand);
    } else {
      ASSERT(destination->IsDoubleStackSlot());
      MemOperand destination_operand = cgen_->ToMemOperand(destination);
      if (in_cycle_) {
        // kSavedDoubleValueRegister was used to break the cycle,
        // but kSavedValueRegister is free.
        MemOperand source_high_operand =
            cgen_->ToHighMemOperand(source);
        MemOperand destination_high_operand =
            cgen_->ToHighMemOperand(destination);
        __ ldr(kSavedValueRegister, source_operand);
        __ str(kSavedValueRegister, destination_operand);
        __ ldr(kSavedValueRegister, source_high_operand);
        __ str(kSavedValueRegister, destination_high_operand);
      } else {
        __ vldr(kScratchDoubleReg, source_operand);
        __ vstr(kScratchDoubleReg, destination_operand);
      }
    }
  } else {
    UNREACHABLE();
  }

  moves_[index].Eliminate();
}


#undef __

} }  // namespace v8::internal

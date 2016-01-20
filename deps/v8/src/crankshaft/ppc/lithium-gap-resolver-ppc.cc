// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/ppc/lithium-gap-resolver-ppc.h"

#include "src/crankshaft/ppc/lithium-codegen-ppc.h"

namespace v8 {
namespace internal {

static const Register kSavedValueRegister = {11};

LGapResolver::LGapResolver(LCodeGen* owner)
    : cgen_(owner),
      moves_(32, owner->zone()),
      root_index_(0),
      in_cycle_(false),
      saved_destination_(NULL) {}


void LGapResolver::Resolve(LParallelMove* parallel_move) {
  DCHECK(moves_.is_empty());
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
      DCHECK(moves_[i].source()->IsConstantOperand());
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
  DCHECK(!moves_[index].IsPending());
  DCHECK(!moves_[index].IsRedundant());

  // Clear this move's destination to indicate a pending move.  The actual
  // destination is saved in a stack allocated local.  Multiple moves can
  // be pending because this function is recursive.
  DCHECK(moves_[index].source() != NULL);  // Or else it will look eliminated.
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
    DCHECK(other_move.IsPending());
    BreakCycle(index);
    return;
  }

  // This move is no longer blocked.
  EmitMove(index);
}


void LGapResolver::Verify() {
#ifdef ENABLE_SLOW_DCHECKS
  // No operand should be the destination for more than one move.
  for (int i = 0; i < moves_.length(); ++i) {
    LOperand* destination = moves_[i].destination();
    for (int j = i + 1; j < moves_.length(); ++j) {
      SLOW_DCHECK(!destination->Equals(moves_[j].destination()));
    }
  }
#endif
}

#define __ ACCESS_MASM(cgen_->masm())

void LGapResolver::BreakCycle(int index) {
  // We save in a register the value that should end up in the source of
  // moves_[root_index].  After performing all moves in the tree rooted
  // in that move, we save the value to that source.
  DCHECK(moves_[index].destination()->Equals(moves_[root_index_].source()));
  DCHECK(!in_cycle_);
  in_cycle_ = true;
  LOperand* source = moves_[index].source();
  saved_destination_ = moves_[index].destination();
  if (source->IsRegister()) {
    __ mr(kSavedValueRegister, cgen_->ToRegister(source));
  } else if (source->IsStackSlot()) {
    __ LoadP(kSavedValueRegister, cgen_->ToMemOperand(source));
  } else if (source->IsDoubleRegister()) {
    __ fmr(kScratchDoubleReg, cgen_->ToDoubleRegister(source));
  } else if (source->IsDoubleStackSlot()) {
    __ lfd(kScratchDoubleReg, cgen_->ToMemOperand(source));
  } else {
    UNREACHABLE();
  }
  // This move will be done by restoring the saved value to the destination.
  moves_[index].Eliminate();
}


void LGapResolver::RestoreValue() {
  DCHECK(in_cycle_);
  DCHECK(saved_destination_ != NULL);

  // Spilled value is in kSavedValueRegister or kSavedDoubleValueRegister.
  if (saved_destination_->IsRegister()) {
    __ mr(cgen_->ToRegister(saved_destination_), kSavedValueRegister);
  } else if (saved_destination_->IsStackSlot()) {
    __ StoreP(kSavedValueRegister, cgen_->ToMemOperand(saved_destination_));
  } else if (saved_destination_->IsDoubleRegister()) {
    __ fmr(cgen_->ToDoubleRegister(saved_destination_), kScratchDoubleReg);
  } else if (saved_destination_->IsDoubleStackSlot()) {
    __ stfd(kScratchDoubleReg, cgen_->ToMemOperand(saved_destination_));
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
      __ mr(cgen_->ToRegister(destination), source_register);
    } else {
      DCHECK(destination->IsStackSlot());
      __ StoreP(source_register, cgen_->ToMemOperand(destination));
    }
  } else if (source->IsStackSlot()) {
    MemOperand source_operand = cgen_->ToMemOperand(source);
    if (destination->IsRegister()) {
      __ LoadP(cgen_->ToRegister(destination), source_operand);
    } else {
      DCHECK(destination->IsStackSlot());
      MemOperand destination_operand = cgen_->ToMemOperand(destination);
      if (in_cycle_) {
        __ LoadP(ip, source_operand);
        __ StoreP(ip, destination_operand);
      } else {
        __ LoadP(kSavedValueRegister, source_operand);
        __ StoreP(kSavedValueRegister, destination_operand);
      }
    }

  } else if (source->IsConstantOperand()) {
    LConstantOperand* constant_source = LConstantOperand::cast(source);
    if (destination->IsRegister()) {
      Register dst = cgen_->ToRegister(destination);
      if (cgen_->IsInteger32(constant_source)) {
        cgen_->EmitLoadIntegerConstant(constant_source, dst);
      } else {
        __ Move(dst, cgen_->ToHandle(constant_source));
      }
    } else if (destination->IsDoubleRegister()) {
      DoubleRegister result = cgen_->ToDoubleRegister(destination);
      double v = cgen_->ToDouble(constant_source);
      __ LoadDoubleLiteral(result, v, ip);
    } else {
      DCHECK(destination->IsStackSlot());
      DCHECK(!in_cycle_);  // Constant moves happen after all cycles are gone.
      if (cgen_->IsInteger32(constant_source)) {
        cgen_->EmitLoadIntegerConstant(constant_source, kSavedValueRegister);
      } else {
        __ Move(kSavedValueRegister, cgen_->ToHandle(constant_source));
      }
      __ StoreP(kSavedValueRegister, cgen_->ToMemOperand(destination));
    }

  } else if (source->IsDoubleRegister()) {
    DoubleRegister source_register = cgen_->ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      __ fmr(cgen_->ToDoubleRegister(destination), source_register);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      __ stfd(source_register, cgen_->ToMemOperand(destination));
    }

  } else if (source->IsDoubleStackSlot()) {
    MemOperand source_operand = cgen_->ToMemOperand(source);
    if (destination->IsDoubleRegister()) {
      __ lfd(cgen_->ToDoubleRegister(destination), source_operand);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      MemOperand destination_operand = cgen_->ToMemOperand(destination);
      if (in_cycle_) {
// kSavedDoubleValueRegister was used to break the cycle,
// but kSavedValueRegister is free.
#if V8_TARGET_ARCH_PPC64
        __ ld(kSavedValueRegister, source_operand);
        __ std(kSavedValueRegister, destination_operand);
#else
        MemOperand source_high_operand = cgen_->ToHighMemOperand(source);
        MemOperand destination_high_operand =
            cgen_->ToHighMemOperand(destination);
        __ lwz(kSavedValueRegister, source_operand);
        __ stw(kSavedValueRegister, destination_operand);
        __ lwz(kSavedValueRegister, source_high_operand);
        __ stw(kSavedValueRegister, destination_high_operand);
#endif
      } else {
        __ lfd(kScratchDoubleReg, source_operand);
        __ stfd(kScratchDoubleReg, destination_operand);
      }
    }
  } else {
    UNREACHABLE();
  }

  moves_[index].Eliminate();
}


#undef __
}  // namespace internal
}  // namespace v8

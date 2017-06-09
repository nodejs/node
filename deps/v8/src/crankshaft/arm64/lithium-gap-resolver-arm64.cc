// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/arm64/lithium-gap-resolver-arm64.h"
#include "src/crankshaft/arm64/delayed-masm-arm64-inl.h"
#include "src/crankshaft/arm64/lithium-codegen-arm64.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM((&masm_))

DelayedGapMasm::DelayedGapMasm(LCodeGen* owner, MacroAssembler* masm)
    : DelayedMasm(owner, masm, root) {
  // We use the root register as an extra scratch register.
  // The root register has two advantages:
  //  - It is not in crankshaft allocatable registers list, so it can't
  //    interfere with the allocatable registers.
  //  - We don't need to push it on the stack, as we can reload it with its
  //    value once we have finish.
}

DelayedGapMasm::~DelayedGapMasm() {}

void DelayedGapMasm::EndDelayedUse() {
  DelayedMasm::EndDelayedUse();
  if (scratch_register_used()) {
    DCHECK(ScratchRegister().Is(root));
    DCHECK(!pending());
    InitializeRootRegister();
    reset_scratch_register_used();
  }
}


LGapResolver::LGapResolver(LCodeGen* owner)
    : cgen_(owner), masm_(owner, owner->masm()), moves_(32, owner->zone()),
      root_index_(0), in_cycle_(false), saved_destination_(NULL) {
}


void LGapResolver::Resolve(LParallelMove* parallel_move) {
  DCHECK(moves_.is_empty());
  DCHECK(!masm_.pending());

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
      DCHECK(move.source()->IsConstantOperand());
      EmitMove(i);
    }
  }

  __ EndDelayedUse();

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

  DCHECK(!current_move.IsPending());
  DCHECK(!current_move.IsRedundant());

  // Clear this move's destination to indicate a pending move.  The actual
  // destination is saved in a stack allocated local. Multiple moves can
  // be pending because this function is recursive.
  DCHECK(current_move.source() != NULL);  // Otherwise it will look eliminated.
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


void LGapResolver::BreakCycle(int index) {
  DCHECK(moves_[index].destination()->Equals(moves_[root_index_].source()));
  DCHECK(!in_cycle_);

  // We save in a register the source of that move and we remember its
  // destination. Then we mark this move as resolved so the cycle is
  // broken and we can perform the other moves.
  in_cycle_ = true;
  LOperand* source = moves_[index].source();
  saved_destination_ = moves_[index].destination();

  if (source->IsRegister()) {
    AcquireSavedValueRegister();
    __ Mov(SavedValueRegister(), cgen_->ToRegister(source));
  } else if (source->IsStackSlot()) {
    AcquireSavedValueRegister();
    __ Load(SavedValueRegister(), cgen_->ToMemOperand(source));
  } else if (source->IsDoubleRegister()) {
    __ Fmov(SavedFPValueRegister(), cgen_->ToDoubleRegister(source));
  } else if (source->IsDoubleStackSlot()) {
    __ Load(SavedFPValueRegister(), cgen_->ToMemOperand(source));
  } else {
    UNREACHABLE();
  }

  // Mark this move as resolved.
  // This move will be actually performed by moving the saved value to this
  // move's destination in LGapResolver::RestoreValue().
  moves_[index].Eliminate();
}


void LGapResolver::RestoreValue() {
  DCHECK(in_cycle_);
  DCHECK(saved_destination_ != NULL);

  if (saved_destination_->IsRegister()) {
    __ Mov(cgen_->ToRegister(saved_destination_), SavedValueRegister());
    ReleaseSavedValueRegister();
  } else if (saved_destination_->IsStackSlot()) {
    __ Store(SavedValueRegister(), cgen_->ToMemOperand(saved_destination_));
    ReleaseSavedValueRegister();
  } else if (saved_destination_->IsDoubleRegister()) {
    __ Fmov(cgen_->ToDoubleRegister(saved_destination_),
            SavedFPValueRegister());
  } else if (saved_destination_->IsDoubleStackSlot()) {
    __ Store(SavedFPValueRegister(), cgen_->ToMemOperand(saved_destination_));
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
      DCHECK(destination->IsStackSlot());
      __ Store(source_register, cgen_->ToMemOperand(destination));
    }

  } else if (source->IsStackSlot()) {
    MemOperand source_operand = cgen_->ToMemOperand(source);
    if (destination->IsRegister()) {
      __ Load(cgen_->ToRegister(destination), source_operand);
    } else {
      DCHECK(destination->IsStackSlot());
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
      DCHECK(destination->IsStackSlot());
      DCHECK(!in_cycle_);  // Constant moves happen after all cycles are gone.
      if (cgen_->IsSmi(constant_source)) {
        Smi* smi = cgen_->ToSmi(constant_source);
        __ StoreConstant(reinterpret_cast<intptr_t>(smi),
                         cgen_->ToMemOperand(destination));
      } else if (cgen_->IsInteger32Constant(constant_source)) {
        __ StoreConstant(cgen_->ToInteger32(constant_source),
                         cgen_->ToMemOperand(destination));
      } else {
        Handle<Object> handle = cgen_->ToHandle(constant_source);
        AllowDeferredHandleDereference smi_object_check;
        if (handle->IsSmi()) {
          Object* obj = *handle;
          DCHECK(!obj->IsHeapObject());
          __ StoreConstant(reinterpret_cast<intptr_t>(obj),
                           cgen_->ToMemOperand(destination));
        } else {
          AcquireSavedValueRegister();
          __ LoadObject(SavedValueRegister(), handle);
          __ Store(SavedValueRegister(), cgen_->ToMemOperand(destination));
          ReleaseSavedValueRegister();
        }
      }
    }

  } else if (source->IsDoubleRegister()) {
    DoubleRegister src = cgen_->ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      __ Fmov(cgen_->ToDoubleRegister(destination), src);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      __ Store(src, cgen_->ToMemOperand(destination));
    }

  } else if (source->IsDoubleStackSlot()) {
    MemOperand src = cgen_->ToMemOperand(source);
    if (destination->IsDoubleRegister()) {
      __ Load(cgen_->ToDoubleRegister(destination), src);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      EmitStackSlotMove(index);
    }

  } else {
    UNREACHABLE();
  }

  // The move has been emitted, we can eliminate it.
  moves_[index].Eliminate();
}

}  // namespace internal
}  // namespace v8

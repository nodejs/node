// Copyright 2011 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_IA32)

#include "ia32/lithium-gap-resolver-ia32.h"
#include "ia32/lithium-codegen-ia32.h"

namespace v8 {
namespace internal {

LGapResolver::LGapResolver(LCodeGen* owner)
    : cgen_(owner),
      moves_(32, owner->zone()),
      source_uses_(),
      destination_uses_(),
      spilled_register_(-1) {}


void LGapResolver::Resolve(LParallelMove* parallel_move) {
  ASSERT(HasBeenReset());
  // Build up a worklist of moves.
  BuildInitialMoveList(parallel_move);

  for (int i = 0; i < moves_.length(); ++i) {
    LMoveOperands move = moves_[i];
    // Skip constants to perform them last.  They don't block other moves
    // and skipping such moves with register destinations keeps those
    // registers free for the whole algorithm.
    if (!move.IsEliminated() && !move.source()->IsConstantOperand()) {
      PerformMove(i);
    }
  }

  // Perform the moves with constant sources.
  for (int i = 0; i < moves_.length(); ++i) {
    if (!moves_[i].IsEliminated()) {
      ASSERT(moves_[i].source()->IsConstantOperand());
      EmitMove(i);
    }
  }

  Finish();
  ASSERT(HasBeenReset());
}


void LGapResolver::BuildInitialMoveList(LParallelMove* parallel_move) {
  // Perform a linear sweep of the moves to add them to the initial list of
  // moves to perform, ignoring any move that is redundant (the source is
  // the same as the destination, the destination is ignored and
  // unallocated, or the move was already eliminated).
  const ZoneList<LMoveOperands>* moves = parallel_move->move_operands();
  for (int i = 0; i < moves->length(); ++i) {
    LMoveOperands move = moves->at(i);
    if (!move.IsRedundant()) AddMove(move);
  }
  Verify();
}


void LGapResolver::PerformMove(int index) {
  // Each call to this function performs a move and deletes it from the move
  // graph.  We first recursively perform any move blocking this one.  We
  // mark a move as "pending" on entry to PerformMove in order to detect
  // cycles in the move graph.  We use operand swaps to resolve cycles,
  // which means that a call to PerformMove could change any source operand
  // in the move graph.

  ASSERT(!moves_[index].IsPending());
  ASSERT(!moves_[index].IsRedundant());

  // Clear this move's destination to indicate a pending move.  The actual
  // destination is saved on the side.
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
      // Though PerformMove can change any source operand in the move graph,
      // this call cannot create a blocking move via a swap (this loop does
      // not miss any).  Assume there is a non-blocking move with source A
      // and this move is blocked on source B and there is a swap of A and
      // B.  Then A and B must be involved in the same cycle (or they would
      // not be swapped).  Since this move's destination is B and there is
      // only a single incoming edge to an operand, this move must also be
      // involved in the same cycle.  In that case, the blocking move will
      // be created but will be "pending" when we return from PerformMove.
      PerformMove(i);
    }
  }

  // We are about to resolve this move and don't need it marked as
  // pending, so restore its destination.
  moves_[index].set_destination(destination);

  // This move's source may have changed due to swaps to resolve cycles and
  // so it may now be the last move in the cycle.  If so remove it.
  if (moves_[index].source()->Equals(destination)) {
    RemoveMove(index);
    return;
  }

  // The move may be blocked on a (at most one) pending move, in which case
  // we have a cycle.  Search for such a blocking move and perform a swap to
  // resolve it.
  for (int i = 0; i < moves_.length(); ++i) {
    LMoveOperands other_move = moves_[i];
    if (other_move.Blocks(destination)) {
      ASSERT(other_move.IsPending());
      EmitSwap(index);
      return;
    }
  }

  // This move is not blocked.
  EmitMove(index);
}


void LGapResolver::AddMove(LMoveOperands move) {
  LOperand* source = move.source();
  if (source->IsRegister()) ++source_uses_[source->index()];

  LOperand* destination = move.destination();
  if (destination->IsRegister()) ++destination_uses_[destination->index()];

  moves_.Add(move, cgen_->zone());
}


void LGapResolver::RemoveMove(int index) {
  LOperand* source = moves_[index].source();
  if (source->IsRegister()) {
    --source_uses_[source->index()];
    ASSERT(source_uses_[source->index()] >= 0);
  }

  LOperand* destination = moves_[index].destination();
  if (destination->IsRegister()) {
    --destination_uses_[destination->index()];
    ASSERT(destination_uses_[destination->index()] >= 0);
  }

  moves_[index].Eliminate();
}


int LGapResolver::CountSourceUses(LOperand* operand) {
  int count = 0;
  for (int i = 0; i < moves_.length(); ++i) {
    if (!moves_[i].IsEliminated() && moves_[i].source()->Equals(operand)) {
      ++count;
    }
  }
  return count;
}


Register LGapResolver::GetFreeRegisterNot(Register reg) {
  int skip_index = reg.is(no_reg) ? -1 : Register::ToAllocationIndex(reg);
  for (int i = 0; i < Register::NumAllocatableRegisters(); ++i) {
    if (source_uses_[i] == 0 && destination_uses_[i] > 0 && i != skip_index) {
      return Register::FromAllocationIndex(i);
    }
  }
  return no_reg;
}


bool LGapResolver::HasBeenReset() {
  if (!moves_.is_empty()) return false;
  if (spilled_register_ >= 0) return false;

  for (int i = 0; i < Register::NumAllocatableRegisters(); ++i) {
    if (source_uses_[i] != 0) return false;
    if (destination_uses_[i] != 0) return false;
  }
  return true;
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

void LGapResolver::Finish() {
  if (spilled_register_ >= 0) {
    __ pop(Register::FromAllocationIndex(spilled_register_));
    spilled_register_ = -1;
  }
  moves_.Rewind(0);
}


void LGapResolver::EnsureRestored(LOperand* operand) {
  if (operand->IsRegister() && operand->index() == spilled_register_) {
    __ pop(Register::FromAllocationIndex(spilled_register_));
    spilled_register_ = -1;
  }
}


Register LGapResolver::EnsureTempRegister() {
  // 1. We may have already spilled to create a temp register.
  if (spilled_register_ >= 0) {
    return Register::FromAllocationIndex(spilled_register_);
  }

  // 2. We may have a free register that we can use without spilling.
  Register free = GetFreeRegisterNot(no_reg);
  if (!free.is(no_reg)) return free;

  // 3. Prefer to spill a register that is not used in any remaining move
  // because it will not need to be restored until the end.
  for (int i = 0; i < Register::NumAllocatableRegisters(); ++i) {
    if (source_uses_[i] == 0 && destination_uses_[i] == 0) {
      Register scratch = Register::FromAllocationIndex(i);
      __ push(scratch);
      spilled_register_ = i;
      return scratch;
    }
  }

  // 4. Use an arbitrary register.  Register 0 is as arbitrary as any other.
  Register scratch = Register::FromAllocationIndex(0);
  __ push(scratch);
  spilled_register_ = 0;
  return scratch;
}


void LGapResolver::EmitMove(int index) {
  LOperand* source = moves_[index].source();
  LOperand* destination = moves_[index].destination();
  EnsureRestored(source);
  EnsureRestored(destination);

  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    ASSERT(destination->IsRegister() || destination->IsStackSlot());
    Register src = cgen_->ToRegister(source);
    Operand dst = cgen_->ToOperand(destination);
    __ mov(dst, src);

  } else if (source->IsStackSlot()) {
    ASSERT(destination->IsRegister() || destination->IsStackSlot());
    Operand src = cgen_->ToOperand(source);
    if (destination->IsRegister()) {
      Register dst = cgen_->ToRegister(destination);
      __ mov(dst, src);
    } else {
      // Spill on demand to use a temporary register for memory-to-memory
      // moves.
      Register tmp = EnsureTempRegister();
      Operand dst = cgen_->ToOperand(destination);
      __ mov(tmp, src);
      __ mov(dst, tmp);
    }

  } else if (source->IsConstantOperand()) {
    LConstantOperand* constant_source = LConstantOperand::cast(source);
    if (destination->IsRegister()) {
      Register dst = cgen_->ToRegister(destination);
      if (cgen_->IsInteger32(constant_source)) {
        __ Set(dst, cgen_->ToInteger32Immediate(constant_source));
      } else {
        __ LoadObject(dst, cgen_->ToHandle(constant_source));
      }
    } else {
      ASSERT(destination->IsStackSlot());
      Operand dst = cgen_->ToOperand(destination);
      if (cgen_->IsInteger32(constant_source)) {
        __ Set(dst, cgen_->ToInteger32Immediate(constant_source));
      } else {
        Register tmp = EnsureTempRegister();
        __ LoadObject(tmp, cgen_->ToHandle(constant_source));
        __ mov(dst, tmp);
      }
    }

  } else if (source->IsDoubleRegister()) {
    if (CpuFeatures::IsSupported(SSE2)) {
      CpuFeatureScope scope(cgen_->masm(), SSE2);
      XMMRegister src = cgen_->ToDoubleRegister(source);
      if (destination->IsDoubleRegister()) {
        XMMRegister dst = cgen_->ToDoubleRegister(destination);
        __ movaps(dst, src);
      } else {
        ASSERT(destination->IsDoubleStackSlot());
        Operand dst = cgen_->ToOperand(destination);
        __ movdbl(dst, src);
      }
    } else {
      // load from the register onto the stack, store in destination, which must
      // be a double stack slot in the non-SSE2 case.
      ASSERT(source->index() == 0);  // source is on top of the stack
      ASSERT(destination->IsDoubleStackSlot());
      Operand dst = cgen_->ToOperand(destination);
      cgen_->ReadX87Operand(dst);
    }
  } else if (source->IsDoubleStackSlot()) {
    if (CpuFeatures::IsSupported(SSE2)) {
      CpuFeatureScope scope(cgen_->masm(), SSE2);
      ASSERT(destination->IsDoubleRegister() ||
             destination->IsDoubleStackSlot());
      Operand src = cgen_->ToOperand(source);
      if (destination->IsDoubleRegister()) {
        XMMRegister dst = cgen_->ToDoubleRegister(destination);
        __ movdbl(dst, src);
      } else {
        // We rely on having xmm0 available as a fixed scratch register.
        Operand dst = cgen_->ToOperand(destination);
        __ movdbl(xmm0, src);
        __ movdbl(dst, xmm0);
      }
    } else {
      // load from the stack slot on top of the floating point stack, and then
      // store in destination. If destination is a double register, then it
      // represents the top of the stack and nothing needs to be done.
      if (destination->IsDoubleStackSlot()) {
        Register tmp = EnsureTempRegister();
        Operand src0 = cgen_->ToOperand(source);
        Operand src1 = cgen_->HighOperand(source);
        Operand dst0 = cgen_->ToOperand(destination);
        Operand dst1 = cgen_->HighOperand(destination);
        __ mov(tmp, src0);  // Then use tmp to copy source to destination.
        __ mov(dst0, tmp);
        __ mov(tmp, src1);
        __ mov(dst1, tmp);
      } else {
        Operand src = cgen_->ToOperand(source);
        if (cgen_->X87StackNonEmpty()) {
          cgen_->PopX87();
        }
        cgen_->PushX87DoubleOperand(src);
      }
    }
  } else {
    UNREACHABLE();
  }

  RemoveMove(index);
}


void LGapResolver::EmitSwap(int index) {
  LOperand* source = moves_[index].source();
  LOperand* destination = moves_[index].destination();
  EnsureRestored(source);
  EnsureRestored(destination);

  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister() && destination->IsRegister()) {
    // Register-register.
    Register src = cgen_->ToRegister(source);
    Register dst = cgen_->ToRegister(destination);
    __ xchg(dst, src);

  } else if ((source->IsRegister() && destination->IsStackSlot()) ||
             (source->IsStackSlot() && destination->IsRegister())) {
    // Register-memory.  Use a free register as a temp if possible.  Do not
    // spill on demand because the simple spill implementation cannot avoid
    // spilling src at this point.
    Register tmp = GetFreeRegisterNot(no_reg);
    Register reg =
        cgen_->ToRegister(source->IsRegister() ? source : destination);
    Operand mem =
        cgen_->ToOperand(source->IsRegister() ? destination : source);
    if (tmp.is(no_reg)) {
      __ xor_(reg, mem);
      __ xor_(mem, reg);
      __ xor_(reg, mem);
    } else {
      __ mov(tmp, mem);
      __ mov(mem, reg);
      __ mov(reg, tmp);
    }

  } else if (source->IsStackSlot() && destination->IsStackSlot()) {
    // Memory-memory.  Spill on demand to use a temporary.  If there is a
    // free register after that, use it as a second temporary.
    Register tmp0 = EnsureTempRegister();
    Register tmp1 = GetFreeRegisterNot(tmp0);
    Operand src = cgen_->ToOperand(source);
    Operand dst = cgen_->ToOperand(destination);
    if (tmp1.is(no_reg)) {
      // Only one temp register available to us.
      __ mov(tmp0, dst);
      __ xor_(tmp0, src);
      __ xor_(src, tmp0);
      __ xor_(tmp0, src);
      __ mov(dst, tmp0);
    } else {
      __ mov(tmp0, dst);
      __ mov(tmp1, src);
      __ mov(dst, tmp1);
      __ mov(src, tmp0);
    }
  } else if (source->IsDoubleRegister() && destination->IsDoubleRegister()) {
    CpuFeatureScope scope(cgen_->masm(), SSE2);
    // XMM register-register swap. We rely on having xmm0
    // available as a fixed scratch register.
    XMMRegister src = cgen_->ToDoubleRegister(source);
    XMMRegister dst = cgen_->ToDoubleRegister(destination);
    __ movaps(xmm0, src);
    __ movaps(src, dst);
    __ movaps(dst, xmm0);
  } else if (source->IsDoubleRegister() || destination->IsDoubleRegister()) {
    CpuFeatureScope scope(cgen_->masm(), SSE2);
    // XMM register-memory swap.  We rely on having xmm0
    // available as a fixed scratch register.
    ASSERT(source->IsDoubleStackSlot() || destination->IsDoubleStackSlot());
    XMMRegister reg = cgen_->ToDoubleRegister(source->IsDoubleRegister()
                                              ? source
                                              : destination);
    Operand other =
        cgen_->ToOperand(source->IsDoubleRegister() ? destination : source);
    __ movdbl(xmm0, other);
    __ movdbl(other, reg);
    __ movdbl(reg, Operand(xmm0));
  } else if (source->IsDoubleStackSlot() && destination->IsDoubleStackSlot()) {
    CpuFeatureScope scope(cgen_->masm(), SSE2);
    // Double-width memory-to-memory.  Spill on demand to use a general
    // purpose temporary register and also rely on having xmm0 available as
    // a fixed scratch register.
    Register tmp = EnsureTempRegister();
    Operand src0 = cgen_->ToOperand(source);
    Operand src1 = cgen_->HighOperand(source);
    Operand dst0 = cgen_->ToOperand(destination);
    Operand dst1 = cgen_->HighOperand(destination);
    __ movdbl(xmm0, dst0);  // Save destination in xmm0.
    __ mov(tmp, src0);  // Then use tmp to copy source to destination.
    __ mov(dst0, tmp);
    __ mov(tmp, src1);
    __ mov(dst1, tmp);
    __ movdbl(src0, xmm0);

  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }

  // The swap of source and destination has executed a move from source to
  // destination.
  RemoveMove(index);

  // Any unperformed (including pending) move with a source of either
  // this move's source or destination needs to have their source
  // changed to reflect the state of affairs after the swap.
  for (int i = 0; i < moves_.length(); ++i) {
    LMoveOperands other_move = moves_[i];
    if (other_move.Blocks(source)) {
      moves_[i].set_source(destination);
    } else if (other_move.Blocks(destination)) {
      moves_[i].set_source(source);
    }
  }

  // In addition to swapping the actual uses as sources, we need to update
  // the use counts.
  if (source->IsRegister() && destination->IsRegister()) {
    int temp = source_uses_[source->index()];
    source_uses_[source->index()] = source_uses_[destination->index()];
    source_uses_[destination->index()] = temp;
  } else if (source->IsRegister()) {
    // We don't have use counts for non-register operands like destination.
    // Compute those counts now.
    source_uses_[source->index()] = CountSourceUses(source);
  } else if (destination->IsRegister()) {
    source_uses_[destination->index()] = CountSourceUses(destination);
  }
}

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32

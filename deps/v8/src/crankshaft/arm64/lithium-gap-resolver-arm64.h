// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_ARM64_LITHIUM_GAP_RESOLVER_ARM64_H_
#define V8_CRANKSHAFT_ARM64_LITHIUM_GAP_RESOLVER_ARM64_H_

#include "src/crankshaft/arm64/delayed-masm-arm64.h"
#include "src/crankshaft/lithium.h"

namespace v8 {
namespace internal {

class LCodeGen;
class LGapResolver;

class DelayedGapMasm : public DelayedMasm {
 public:
  DelayedGapMasm(LCodeGen* owner, MacroAssembler* masm);
  ~DelayedGapMasm();

  void EndDelayedUse();
};


class LGapResolver BASE_EMBEDDED {
 public:
  explicit LGapResolver(LCodeGen* owner);

  // Resolve a set of parallel moves, emitting assembler instructions.
  void Resolve(LParallelMove* parallel_move);

 private:
  // Build the initial list of moves.
  void BuildInitialMoveList(LParallelMove* parallel_move);

  // Perform the move at the moves_ index in question (possibly requiring
  // other moves to satisfy dependencies).
  void PerformMove(int index);

  // If a cycle is found in the series of moves, save the blocking value to
  // a scratch register.  The cycle must be found by hitting the root of the
  // depth-first search.
  void BreakCycle(int index);

  // After a cycle has been resolved, restore the value from the scratch
  // register to its proper destination.
  void RestoreValue();

  // Emit a move and remove it from the move graph.
  void EmitMove(int index);

  // Emit a move from one stack slot to another.
  void EmitStackSlotMove(int index) {
    masm_.StackSlotMove(moves_[index].source(), moves_[index].destination());
  }

  // Verify the move list before performing moves.
  void Verify();

  // Registers used to solve cycles.
  const Register& SavedValueRegister() {
    DCHECK(!RegisterConfiguration::Crankshaft()->IsAllocatableGeneralCode(
        masm_.ScratchRegister().code()));
    return masm_.ScratchRegister();
  }
  // The scratch register is used to break cycles and to store constant.
  // These two methods switch from one mode to the other.
  void AcquireSavedValueRegister() { masm_.AcquireScratchRegister(); }
  void ReleaseSavedValueRegister() { masm_.ReleaseScratchRegister(); }
  const FPRegister& SavedFPValueRegister() {
    // We use the Crankshaft floating-point scratch register to break a cycle
    // involving double values as the MacroAssembler will not need it for the
    // operations performed by the gap resolver.
    DCHECK(!RegisterConfiguration::Crankshaft()->IsAllocatableGeneralCode(
        crankshaft_fp_scratch.code()));
    return crankshaft_fp_scratch;
  }

  LCodeGen* cgen_;
  DelayedGapMasm masm_;

  // List of moves not yet resolved.
  ZoneList<LMoveOperands> moves_;

  int root_index_;
  bool in_cycle_;
  LOperand* saved_destination_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_ARM64_LITHIUM_GAP_RESOLVER_ARM64_H_

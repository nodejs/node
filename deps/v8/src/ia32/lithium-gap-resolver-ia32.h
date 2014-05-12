// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IA32_LITHIUM_GAP_RESOLVER_IA32_H_
#define V8_IA32_LITHIUM_GAP_RESOLVER_IA32_H_

#include "v8.h"

#include "lithium.h"

namespace v8 {
namespace internal {

class LCodeGen;
class LGapResolver;

class LGapResolver V8_FINAL BASE_EMBEDDED {
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

  // Emit any code necessary at the end of a gap move.
  void Finish();

  // Add or delete a move from the move graph without emitting any code.
  // Used to build up the graph and remove trivial moves.
  void AddMove(LMoveOperands move);
  void RemoveMove(int index);

  // Report the count of uses of operand as a source in a not-yet-performed
  // move.  Used to rebuild use counts.
  int CountSourceUses(LOperand* operand);

  // Emit a move and remove it from the move graph.
  void EmitMove(int index);

  // Execute a move by emitting a swap of two operands.  The move from
  // source to destination is removed from the move graph.
  void EmitSwap(int index);

  // Ensure that the given operand is not spilled.
  void EnsureRestored(LOperand* operand);

  // Return a register that can be used as a temp register, spilling
  // something if necessary.
  Register EnsureTempRegister();

  // Return a known free register different from the given one (which could
  // be no_reg---returning any free register), or no_reg if there is no such
  // register.
  Register GetFreeRegisterNot(Register reg);

  // Verify that the state is the initial one, ready to resolve a single
  // parallel move.
  bool HasBeenReset();

  // Verify the move list before performing moves.
  void Verify();

  LCodeGen* cgen_;

  // List of moves not yet resolved.
  ZoneList<LMoveOperands> moves_;

  // Source and destination use counts for the general purpose registers.
  int source_uses_[Register::kMaxNumAllocatableRegisters];
  int destination_uses_[Register::kMaxNumAllocatableRegisters];

  // If we had to spill on demand, the currently spilled register's
  // allocation index.
  int spilled_register_;
};

} }  // namespace v8::internal

#endif  // V8_IA32_LITHIUM_GAP_RESOLVER_IA32_H_

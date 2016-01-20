// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_ARM_LITHIUM_GAP_RESOLVER_ARM_H_
#define V8_CRANKSHAFT_ARM_LITHIUM_GAP_RESOLVER_ARM_H_

#include "src/crankshaft/lithium.h"

namespace v8 {
namespace internal {

class LCodeGen;
class LGapResolver;

class LGapResolver final BASE_EMBEDDED {
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

  // Verify the move list before performing moves.
  void Verify();

  LCodeGen* cgen_;

  // List of moves not yet resolved.
  ZoneList<LMoveOperands> moves_;

  int root_index_;
  bool in_cycle_;
  LOperand* saved_destination_;

  // We use the root register as a scratch in a few places. When that happens,
  // this flag is set to indicate that it needs to be restored.
  bool need_to_restore_root_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_ARM_LITHIUM_GAP_RESOLVER_ARM_H_

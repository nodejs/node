// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_X64_LITHIUM_GAP_RESOLVER_X64_H_
#define V8_X64_LITHIUM_GAP_RESOLVER_X64_H_

#include "src/v8.h"

#include "src/lithium.h"

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

  // Emit a move and remove it from the move graph.
  void EmitMove(int index);

  // Execute a move by emitting a swap of two operands.  The move from
  // source to destination is removed from the move graph.
  void EmitSwap(int index);

  // Verify the move list before performing moves.
  void Verify();

  LCodeGen* cgen_;

  // List of moves not yet resolved.
  ZoneList<LMoveOperands> moves_;
};

} }  // namespace v8::internal

#endif  // V8_X64_LITHIUM_GAP_RESOLVER_X64_H_

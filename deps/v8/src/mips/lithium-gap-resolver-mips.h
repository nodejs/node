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

#ifndef V8_MIPS_LITHIUM_GAP_RESOLVER_MIPS_H_
#define V8_MIPS_LITHIUM_GAP_RESOLVER_MIPS_H_

#include "v8.h"

#include "lithium.h"

namespace v8 {
namespace internal {

class LCodeGen;
class LGapResolver;

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

  // Verify the move list before performing moves.
  void Verify();

  LCodeGen* cgen_;

  // List of moves not yet resolved.
  ZoneList<LMoveOperands> moves_;

  int root_index_;
  bool in_cycle_;
  LOperand* saved_destination_;
};

} }  // namespace v8::internal

#endif  // V8_MIPS_LITHIUM_GAP_RESOLVER_MIPS_H_

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GAP_RESOLVER_H_
#define V8_COMPILER_GAP_RESOLVER_H_

#include "src/compiler/instruction.h"

namespace v8 {
namespace internal {
namespace compiler {

class GapResolver FINAL {
 public:
  // Interface used by the gap resolver to emit moves and swaps.
  class Assembler {
   public:
    virtual ~Assembler() {}

    // Assemble move.
    virtual void AssembleMove(InstructionOperand* source,
                              InstructionOperand* destination) = 0;
    // Assemble swap.
    virtual void AssembleSwap(InstructionOperand* source,
                              InstructionOperand* destination) = 0;
  };

  explicit GapResolver(Assembler* assembler) : assembler_(assembler) {}

  // Resolve a set of parallel moves, emitting assembler instructions.
  void Resolve(ParallelMove* parallel_move) const;

 private:
  // Perform the given move, possibly requiring other moves to satisfy
  // dependencies.
  void PerformMove(ZoneList<MoveOperands>* moves, MoveOperands* move) const;

  // Assembler used to emit moves and save registers.
  Assembler* const assembler_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GAP_RESOLVER_H_

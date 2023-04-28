// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_GAP_RESOLVER_H_
#define V8_COMPILER_BACKEND_GAP_RESOLVER_H_

#include "src/compiler/backend/instruction.h"

namespace v8 {
namespace internal {
namespace compiler {

class GapResolver final {
 public:
  // Interface used by the gap resolver to emit moves and swaps.
  class Assembler {
   public:
    virtual ~Assembler() = default;

    // Assemble move.
    virtual void AssembleMove(InstructionOperand* source,
                              InstructionOperand* destination) = 0;
    // Assemble swap.
    virtual void AssembleSwap(InstructionOperand* source,
                              InstructionOperand* destination) = 0;

    // Helper functions to resolve cyclic dependencies.
    // - {Push} pushes {src} and returns an operand that encodes the new stack
    // slot.
    // - {Pop} pops the topmost stack operand and moves it to {dest}.
    // - {PopTempStackSlots} pops all remaining unpopped stack slots.
    // - {SetPendingMove} reserves scratch registers needed to perform the moves
    // in the cycle.
    // - {MoveToTempLocation} moves an operand to a temporary location, either
    // a scratch register or a new stack slot, depending on the platform and the
    // reserved registers.
    // - {MoveTempLocationTo} moves the temp location to the destination,
    // thereby completing the cycle.
    virtual AllocatedOperand Push(InstructionOperand* src) = 0;
    virtual void Pop(InstructionOperand* dest, MachineRepresentation rep) = 0;
    virtual void PopTempStackSlots() = 0;
    virtual void MoveToTempLocation(InstructionOperand* src,
                                    MachineRepresentation rep) = 0;
    virtual void MoveTempLocationTo(InstructionOperand* dst,
                                    MachineRepresentation rep) = 0;
    virtual void SetPendingMove(MoveOperands* move) = 0;
    int temp_slots_ = 0;
  };

  explicit GapResolver(Assembler* assembler) : assembler_(assembler) {}

  // Resolve a set of parallel moves, emitting assembler instructions.
  V8_EXPORT_PRIVATE void Resolve(ParallelMove* parallel_move);

 private:
  // Take a vector of moves where each move blocks the next one, and the last
  // one blocks the first one, and resolve it using a temporary location.
  void PerformCycle(const std::vector<MoveOperands*>& cycle);
  // Performs the given move, possibly performing other moves to unblock the
  // destination operand.
  void PerformMove(ParallelMove* moves, MoveOperands* move);
  // Perform the move and its dependencies. Also performs simple cyclic
  // dependencies. For more complex cases the method may bail out:
  // in this case, it returns one of the problematic moves. The caller
  // ({PerformMove}) will use a temporary stack slot to unblock the dependencies
  // and try again.
  MoveOperands* PerformMoveHelper(ParallelMove* moves, MoveOperands* move,
                                  std::vector<MoveOperands*>* cycle);
  // Assembler used to emit moves and save registers.
  Assembler* const assembler_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_GAP_RESOLVER_H_

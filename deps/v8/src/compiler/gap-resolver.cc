// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/gap-resolver.h"

#include <algorithm>
#include <functional>
#include <set>

namespace v8 {
namespace internal {
namespace compiler {

typedef ZoneList<MoveOperands>::iterator op_iterator;

#ifdef ENABLE_SLOW_DCHECKS
// TODO(svenpanne) Brush up InstructionOperand with comparison?
struct InstructionOperandComparator {
  bool operator()(const InstructionOperand* x,
                  const InstructionOperand* y) const {
    return (x->kind() < y->kind()) ||
           (x->kind() == y->kind() && x->index() < y->index());
  }
};
#endif

// No operand should be the destination for more than one move.
static void VerifyMovesAreInjective(ZoneList<MoveOperands>* moves) {
#ifdef ENABLE_SLOW_DCHECKS
  std::set<InstructionOperand*, InstructionOperandComparator> seen;
  for (op_iterator i = moves->begin(); i != moves->end(); ++i) {
    SLOW_DCHECK(seen.find(i->destination()) == seen.end());
    seen.insert(i->destination());
  }
#endif
}


void GapResolver::Resolve(ParallelMove* parallel_move) const {
  ZoneList<MoveOperands>* moves = parallel_move->move_operands();
  // TODO(svenpanne) Use the member version of remove_if when we use real lists.
  op_iterator end =
      std::remove_if(moves->begin(), moves->end(),
                     std::mem_fun_ref(&MoveOperands::IsRedundant));
  moves->Rewind(static_cast<int>(end - moves->begin()));

  VerifyMovesAreInjective(moves);

  for (op_iterator move = moves->begin(); move != moves->end(); ++move) {
    if (!move->IsEliminated()) PerformMove(moves, &*move);
  }
}


void GapResolver::PerformMove(ZoneList<MoveOperands>* moves,
                              MoveOperands* move) const {
  // Each call to this function performs a move and deletes it from the move
  // graph.  We first recursively perform any move blocking this one.  We mark a
  // move as "pending" on entry to PerformMove in order to detect cycles in the
  // move graph.  We use operand swaps to resolve cycles, which means that a
  // call to PerformMove could change any source operand in the move graph.
  DCHECK(!move->IsPending());
  DCHECK(!move->IsRedundant());

  // Clear this move's destination to indicate a pending move.  The actual
  // destination is saved on the side.
  DCHECK_NOT_NULL(move->source());  // Or else it will look eliminated.
  InstructionOperand* destination = move->destination();
  move->set_destination(NULL);

  // Perform a depth-first traversal of the move graph to resolve dependencies.
  // Any unperformed, unpending move with a source the same as this one's
  // destination blocks this one so recursively perform all such moves.
  for (op_iterator other = moves->begin(); other != moves->end(); ++other) {
    if (other->Blocks(destination) && !other->IsPending()) {
      // Though PerformMove can change any source operand in the move graph,
      // this call cannot create a blocking move via a swap (this loop does not
      // miss any).  Assume there is a non-blocking move with source A and this
      // move is blocked on source B and there is a swap of A and B.  Then A and
      // B must be involved in the same cycle (or they would not be swapped).
      // Since this move's destination is B and there is only a single incoming
      // edge to an operand, this move must also be involved in the same cycle.
      // In that case, the blocking move will be created but will be "pending"
      // when we return from PerformMove.
      PerformMove(moves, other);
    }
  }

  // We are about to resolve this move and don't need it marked as pending, so
  // restore its destination.
  move->set_destination(destination);

  // This move's source may have changed due to swaps to resolve cycles and so
  // it may now be the last move in the cycle.  If so remove it.
  InstructionOperand* source = move->source();
  if (source->Equals(destination)) {
    move->Eliminate();
    return;
  }

  // The move may be blocked on a (at most one) pending move, in which case we
  // have a cycle.  Search for such a blocking move and perform a swap to
  // resolve it.
  op_iterator blocker = std::find_if(
      moves->begin(), moves->end(),
      std::bind2nd(std::mem_fun_ref(&MoveOperands::Blocks), destination));
  if (blocker == moves->end()) {
    // The easy case: This move is not blocked.
    assembler_->AssembleMove(source, destination);
    move->Eliminate();
    return;
  }

  DCHECK(blocker->IsPending());
  // Ensure source is a register or both are stack slots, to limit swap cases.
  if (source->IsStackSlot() || source->IsDoubleStackSlot()) {
    std::swap(source, destination);
  }
  assembler_->AssembleSwap(source, destination);
  move->Eliminate();

  // Any unperformed (including pending) move with a source of either this
  // move's source or destination needs to have their source changed to
  // reflect the state of affairs after the swap.
  for (op_iterator other = moves->begin(); other != moves->end(); ++other) {
    if (other->Blocks(source)) {
      other->set_source(destination);
    } else if (other->Blocks(destination)) {
      other->set_source(source);
    }
  }
}
}
}
}  // namespace v8::internal::compiler

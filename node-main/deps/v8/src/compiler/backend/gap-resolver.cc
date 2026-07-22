// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/gap-resolver.h"

#include <algorithm>
#include <set>

#include "src/base/enum-set.h"
#include "src/codegen/register-configuration.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

enum MoveOperandKind : uint8_t { kConstant, kGpReg, kFpReg, kStack };

MoveOperandKind GetKind(const InstructionOperand& move) {
  if (move.IsConstant()) return kConstant;
  LocationOperand loc_op = LocationOperand::cast(move);
  if (loc_op.location_kind() != LocationOperand::REGISTER) return kStack;
  return IsFloatingPoint(loc_op.representation()) ? kFpReg : kGpReg;
}

}  // namespace

void GapResolver::Resolve(ParallelMove* moves) {
  base::EnumSet<MoveOperandKind, uint8_t> source_kinds;
  base::EnumSet<MoveOperandKind, uint8_t> destination_kinds;

  // Remove redundant moves, collect source kinds and destination kinds to
  // detect simple non-overlapping moves, and collect FP move representations if
  // aliasing is non-simple.
  size_t nmoves = moves->size();
  for (size_t i = 0; i < nmoves;) {
    MoveOperands* move = (*moves)[i];
    if (move->IsRedundant()) {
      nmoves--;
      if (i < nmoves) (*moves)[i] = (*moves)[nmoves];
      continue;
    }
    i++;
    source_kinds.Add(GetKind(move->source()));
    destination_kinds.Add(GetKind(move->destination()));
  }
  if (nmoves != moves->size()) moves->resize(nmoves);

  if ((source_kinds & destination_kinds).empty() || moves->size() < 2) {
    // Fast path for non-conflicting parallel moves.
    for (MoveOperands* move : *moves) {
      assembler_->AssembleMove(&move->source(), &move->destination());
    }
    return;
  }

  for (size_t i = 0; i < moves->size(); ++i) {
    auto move = (*moves)[i];
    if (!move->IsEliminated()) PerformMove(moves, move);
  }
  assembler_->PopTempStackSlots();
}

// Check if a 2-move cycle is a swap. This is not always the case, for instance:
//
// [fp_stack:-3|s128] = [xmm5|R|s128]
// [xmm5|R|s128] = [fp_stack:-4|s128]
//
// The two stack operands conflict but start at a different stack offset, so a
// swap would be incorrect.
// In general, swapping is allowed if the conflicting operands:
// - Have the same representation, and
// - Are the same register, or are stack slots with the same index
bool IsSwap(MoveOperands* move1, MoveOperands* move2) {
  return move1->source() == move2->destination() &&
         move2->source() == move1->destination();
}

void GapResolver::PerformCycle(const std::vector<MoveOperands*>& cycle) {
  DCHECK(!cycle.empty());
  MoveOperands* move1 = cycle.back();
  if (cycle.size() == 2 && IsSwap(cycle.front(), cycle.back())) {
    // Call {AssembleSwap} which can generate better code than the generic
    // algorithm below in some cases.
    MoveOperands* move2 = cycle.front();
    InstructionOperand* source = &move1->source();
    InstructionOperand* destination = &move1->destination();
    // Ensure source is a register or both are stack slots, to limit swap
    // cases.
    if (source->IsAnyStackSlot()) {
      std::swap(source, destination);
    }
    assembler_->AssembleSwap(source, destination);
    move1->Eliminate();
    move2->Eliminate();
    return;
  }
  // Generic move-cycle algorithm. The cycle of size n is ordered such that the
  // move at index i % n blocks the move at index (i + 1) % n.
  // - Move the source of the last move to a platform-specific temporary
  // location.
  // - Assemble the remaining moves from left to right. The first move was
  // unblocked by the temporary location, and each move unblocks the next one.
  // - Move the temporary location to the last move's destination, thereby
  // completing the cycle.
  // To ensure that the temporary location does not conflict with any scratch
  // register used during the move cycle, the platform implements
  // {SetPendingMove}, which marks the registers needed for the given moves.
  // {MoveToTempLocation} will then choose the location accordingly.
  MachineRepresentation rep =
      LocationOperand::cast(move1->destination()).representation();
  for (size_t i = 0; i < cycle.size() - 1; ++i) {
    assembler_->SetPendingMove(cycle[i]);
  }
  assembler_->MoveToTempLocation(&move1->source(), rep);
  InstructionOperand destination = move1->destination();
  move1->Eliminate();
  for (size_t i = 0; i < cycle.size() - 1; ++i) {
    assembler_->AssembleMove(&cycle[i]->source(), &cycle[i]->destination());
    cycle[i]->Eliminate();
  }
  assembler_->MoveTempLocationTo(&destination, rep);
  // We do not need to update the sources of the remaining moves in the parallel
  // move. If any of the remaining moves had the same source as one of the moves
  // in the cycle, it would block the cycle and would have already been
  // assembled by {PerformMoveHelper}.
}

void GapResolver::PerformMove(ParallelMove* moves, MoveOperands* move) {
  // Try to perform the move and its dependencies with {PerformMoveHelper}.
  // This helper function will be able to solve most cases, including cycles.
  // But for some rare cases, it will bail out and return one of the
  // problematic moves. In this case, push the source to the stack to
  // break the cycles that it belongs to, and try again.
  std::vector<MoveOperands*> cycle;
  while (MoveOperands* blocking_move = PerformMoveHelper(moves, move, &cycle)) {
    // Push an arbitrary operand of the cycle to break it.
    AllocatedOperand scratch = assembler_->Push(&blocking_move->source());
    InstructionOperand source = blocking_move->source();
    for (auto m : *moves) {
      if (m->source() == source) {
        m->set_source(scratch);
      }
    }
    cycle.clear();
  }
}

MoveOperands* GapResolver::PerformMoveHelper(
    ParallelMove* moves, MoveOperands* move,
    std::vector<MoveOperands*>* cycle) {
  // We interpret moves as nodes in a graph. x is a successor of y (x blocks y)
  // if x.source() conflicts with y.destination(). We recursively assemble the
  // moves in this graph in post-order using a DFS traversal, such that all
  // blocking moves are assembled first.
  // We also mark moves in the current DFS branch as pending. If a move is
  // blocked by a pending move, this is a cycle. In this case we just
  // reconstruct the cycle on the way back, and assemble it using {PerformCycle}
  // when we reach the first move.
  // This algorithm can only process one cycle at a time. If another cycle is
  // found while the first one is still being processed, we bail out.
  // The caller breaks the cycle using a temporary stack slot, and we try
  // again.

  DCHECK(!move->IsPending());
  DCHECK(!move->IsRedundant());

  // Clear this move's destination to indicate a pending move.  The actual
  // destination is saved on the side.
  InstructionOperand source = move->source();
  DCHECK(!source.IsInvalid());  // Or else it will look eliminated.
  InstructionOperand destination = move->destination();
  move->SetPending();
  MoveOperands* blocking_move = nullptr;

  for (size_t i = 0; i < moves->size(); ++i) {
    auto other = (*moves)[i];
    if (other->IsEliminated()) continue;
    if (other == move) continue;
    if (other->source().InterferesWith(destination)) {
      if (other->IsPending()) {
        // The conflicting move is pending, we found a cycle. Build the list of
        // moves that belong to the cycle on the way back.
        // If this move already belongs to a cycle, bail out.
        if (!cycle->empty()) {
          blocking_move = cycle->front();
          break;
        }
        // Initialize the cycle with {other} and reconstruct the rest of the
        // cycle on the way back.
        cycle->push_back(other);
      } else {
        std::vector<MoveOperands*> cycle_rec;
        blocking_move = PerformMoveHelper(moves, other, &cycle_rec);
        if (blocking_move) break;
        if (!cycle->empty() && !cycle_rec.empty()) {
          blocking_move = cycle_rec.front();
          break;
        }
        if (cycle->empty() && !cycle_rec.empty()) {
          *cycle = std::move(cycle_rec);
        }
      }
    }
  }

  // We finished processing all the blocking moves and don't need this one
  // marked as pending anymore, restore its destination.
  move->set_destination(destination);

  if (blocking_move != nullptr) return blocking_move;

  if (!cycle->empty()) {
    if (cycle->front() == move) {
      // We returned to the topmost move in the cycle and assembled all the
      // other dependencies. Assemble the cycle.
      PerformCycle(*cycle);
      cycle->clear();
    } else {
      cycle->push_back(move);
    }
  } else {
    assembler_->AssembleMove(&source, &destination);
    move->Eliminate();
  }
  return nullptr;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

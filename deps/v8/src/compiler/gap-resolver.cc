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

namespace {

#define REP_BIT(rep) (1 << static_cast<int>(rep))

const int kFloat32Bit = REP_BIT(MachineRepresentation::kFloat32);
const int kFloat64Bit = REP_BIT(MachineRepresentation::kFloat64);

inline bool Blocks(MoveOperands* move, InstructionOperand destination) {
  return !move->IsEliminated() && move->source().InterferesWith(destination);
}

// Splits a FP move between two location operands into the equivalent series of
// moves between smaller sub-operands, e.g. a double move to two single moves.
// This helps reduce the number of cycles that would normally occur under FP
// aliasing, and makes swaps much easier to implement.
MoveOperands* Split(MoveOperands* move, MachineRepresentation smaller_rep,
                    ParallelMove* moves) {
  DCHECK(!kSimpleFPAliasing);
  // Splitting is only possible when the slot size is the same as float size.
  DCHECK_EQ(kPointerSize, kFloatSize);
  const LocationOperand& src_loc = LocationOperand::cast(move->source());
  const LocationOperand& dst_loc = LocationOperand::cast(move->destination());
  MachineRepresentation dst_rep = dst_loc.representation();
  DCHECK_NE(smaller_rep, dst_rep);
  auto src_kind = src_loc.location_kind();
  auto dst_kind = dst_loc.location_kind();

  int aliases =
      1 << (ElementSizeLog2Of(dst_rep) - ElementSizeLog2Of(smaller_rep));
  int base = -1;
  USE(base);
  DCHECK_EQ(aliases, RegisterConfiguration::Turbofan()->GetAliases(
                         dst_rep, 0, smaller_rep, &base));

  int src_index = -1;
  int slot_size = (1 << ElementSizeLog2Of(smaller_rep)) / kPointerSize;
  int src_step = 1;
  if (src_kind == LocationOperand::REGISTER) {
    src_index = src_loc.register_code() * aliases;
  } else {
    src_index = src_loc.index();
    // For operands that occuply multiple slots, the index refers to the last
    // slot. On little-endian architectures, we start at the high slot and use a
    // negative step so that register-to-slot moves are in the correct order.
    src_step = -slot_size;
  }
  int dst_index = -1;
  int dst_step = 1;
  if (dst_kind == LocationOperand::REGISTER) {
    dst_index = dst_loc.register_code() * aliases;
  } else {
    dst_index = dst_loc.index();
    dst_step = -slot_size;
  }

  // Reuse 'move' for the first fragment. It is not pending.
  move->set_source(AllocatedOperand(src_kind, smaller_rep, src_index));
  move->set_destination(AllocatedOperand(dst_kind, smaller_rep, dst_index));
  // Add the remaining fragment moves.
  for (int i = 1; i < aliases; ++i) {
    src_index += src_step;
    dst_index += dst_step;
    moves->AddMove(AllocatedOperand(src_kind, smaller_rep, src_index),
                   AllocatedOperand(dst_kind, smaller_rep, dst_index));
  }
  // Return the first fragment.
  return move;
}

}  // namespace

void GapResolver::Resolve(ParallelMove* moves) {
  // Clear redundant moves, and collect FP move representations if aliasing
  // is non-simple.
  int reps = 0;
  for (size_t i = 0; i < moves->size();) {
    MoveOperands* move = (*moves)[i];
    if (move->IsRedundant()) {
      (*moves)[i] = moves->back();
      moves->pop_back();
      continue;
    }
    i++;
    if (!kSimpleFPAliasing && move->destination().IsFPRegister()) {
      reps |=
          REP_BIT(LocationOperand::cast(move->destination()).representation());
    }
  }

  if (!kSimpleFPAliasing) {
    if (reps && !base::bits::IsPowerOfTwo32(reps)) {
      // Start with the smallest FP moves, so we never encounter smaller moves
      // in the middle of a cycle of larger moves.
      if ((reps & kFloat32Bit) != 0) {
        split_rep_ = MachineRepresentation::kFloat32;
        for (size_t i = 0; i < moves->size(); ++i) {
          auto move = (*moves)[i];
          if (!move->IsEliminated() && move->destination().IsFloatRegister())
            PerformMove(moves, move);
        }
      }
      if ((reps & kFloat64Bit) != 0) {
        split_rep_ = MachineRepresentation::kFloat64;
        for (size_t i = 0; i < moves->size(); ++i) {
          auto move = (*moves)[i];
          if (!move->IsEliminated() && move->destination().IsDoubleRegister())
            PerformMove(moves, move);
        }
      }
    }
    split_rep_ = MachineRepresentation::kSimd128;
  }

  for (size_t i = 0; i < moves->size(); ++i) {
    auto move = (*moves)[i];
    if (!move->IsEliminated()) PerformMove(moves, move);
  }
}

void GapResolver::PerformMove(ParallelMove* moves, MoveOperands* move) {
  // Each call to this function performs a move and deletes it from the move
  // graph.  We first recursively perform any move blocking this one.  We mark a
  // move as "pending" on entry to PerformMove in order to detect cycles in the
  // move graph.  We use operand swaps to resolve cycles, which means that a
  // call to PerformMove could change any source operand in the move graph.
  DCHECK(!move->IsPending());
  DCHECK(!move->IsRedundant());

  // Clear this move's destination to indicate a pending move.  The actual
  // destination is saved on the side.
  InstructionOperand source = move->source();
  DCHECK(!source.IsInvalid());  // Or else it will look eliminated.
  InstructionOperand destination = move->destination();
  move->SetPending();

  // We may need to split moves between FP locations differently.
  const bool is_fp_loc_move =
      !kSimpleFPAliasing && destination.IsFPLocationOperand();

  // Perform a depth-first traversal of the move graph to resolve dependencies.
  // Any unperformed, unpending move with a source the same as this one's
  // destination blocks this one so recursively perform all such moves.
  for (size_t i = 0; i < moves->size(); ++i) {
    auto other = (*moves)[i];
    if (other->IsEliminated()) continue;
    if (other->IsPending()) continue;
    if (other->source().InterferesWith(destination)) {
      if (is_fp_loc_move &&
          LocationOperand::cast(other->source()).representation() >
              split_rep_) {
        // 'other' must also be an FP location move. Break it into fragments
        // of the same size as 'move'. 'other' is set to one of the fragments,
        // and the rest are appended to 'moves'.
        other = Split(other, split_rep_, moves);
        // 'other' may not block destination now.
        if (!other->source().InterferesWith(destination)) continue;
      }
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

  // This move's source may have changed due to swaps to resolve cycles and so
  // it may now be the last move in the cycle.  If so remove it.
  source = move->source();
  if (source.EqualsCanonicalized(destination)) {
    move->Eliminate();
    return;
  }

  // We are about to resolve this move and don't need it marked as pending, so
  // restore its destination.
  move->set_destination(destination);

  // The move may be blocked on a (at most one) pending move, in which case we
  // have a cycle.  Search for such a blocking move and perform a swap to
  // resolve it.
  auto blocker = std::find_if(moves->begin(), moves->end(),
                              std::bind2nd(std::ptr_fun(&Blocks), destination));
  if (blocker == moves->end()) {
    // The easy case: This move is not blocked.
    assembler_->AssembleMove(&source, &destination);
    move->Eliminate();
    return;
  }

  // Ensure source is a register or both are stack slots, to limit swap cases.
  if (source.IsStackSlot() || source.IsFPStackSlot()) {
    std::swap(source, destination);
  }
  assembler_->AssembleSwap(&source, &destination);
  move->Eliminate();

  // Update outstanding moves whose source may now have been moved.
  if (is_fp_loc_move) {
    // We may have to split larger moves.
    for (size_t i = 0; i < moves->size(); ++i) {
      auto other = (*moves)[i];
      if (other->IsEliminated()) continue;
      if (source.InterferesWith(other->source())) {
        if (LocationOperand::cast(other->source()).representation() >
            split_rep_) {
          other = Split(other, split_rep_, moves);
          if (!source.InterferesWith(other->source())) continue;
        }
        other->set_source(destination);
      } else if (destination.InterferesWith(other->source())) {
        if (LocationOperand::cast(other->source()).representation() >
            split_rep_) {
          other = Split(other, split_rep_, moves);
          if (!destination.InterferesWith(other->source())) continue;
        }
        other->set_source(source);
      }
    }
  } else {
    for (auto other : *moves) {
      if (other->IsEliminated()) continue;
      if (source.EqualsCanonicalized(other->source())) {
        other->set_source(destination);
      } else if (destination.EqualsCanonicalized(other->source())) {
        other->set_source(source);
      }
    }
  }
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8

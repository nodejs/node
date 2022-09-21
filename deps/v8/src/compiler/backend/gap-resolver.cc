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

// Splits a FP move between two location operands into the equivalent series of
// moves between smaller sub-operands, e.g. a double move to two single moves.
// This helps reduce the number of cycles that would normally occur under FP
// aliasing, and makes swaps much easier to implement.
MoveOperands* Split(MoveOperands* move, MachineRepresentation smaller_rep,
                    ParallelMove* moves) {
  DCHECK(kFPAliasing == AliasingKind::kCombine);
  // Splitting is only possible when the slot size is the same as float size.
  DCHECK_EQ(kSystemPointerSize, kFloatSize);
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
  DCHECK_EQ(aliases, RegisterConfiguration::Default()->GetAliases(
                         dst_rep, 0, smaller_rep, &base));

  int src_index = -1;
  int slot_size = (1 << ElementSizeLog2Of(smaller_rep)) / kSystemPointerSize;
  int src_step = 1;
  if (src_kind == LocationOperand::REGISTER) {
    src_index = src_loc.register_code() * aliases;
  } else {
    src_index = src_loc.index();
    // For operands that occupy multiple slots, the index refers to the last
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
  int fp_reps = 0;
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
    if (kFPAliasing == AliasingKind::kCombine &&
        move->destination().IsFPRegister()) {
      fp_reps |= RepresentationBit(
          LocationOperand::cast(move->destination()).representation());
    }
  }
  if (nmoves != moves->size()) moves->resize(nmoves);

  if ((source_kinds & destination_kinds).empty() || moves->size() < 2) {
    // Fast path for non-conflicting parallel moves.
    for (MoveOperands* move : *moves) {
      assembler_->AssembleMove(&move->source(), &move->destination());
    }
    return;
  }

  if (kFPAliasing == AliasingKind::kCombine) {
    if (fp_reps && !base::bits::IsPowerOfTwo(fp_reps)) {
      // Start with the smallest FP moves, so we never encounter smaller moves
      // in the middle of a cycle of larger moves.
      if ((fp_reps & RepresentationBit(MachineRepresentation::kFloat32)) != 0) {
        split_rep_ = MachineRepresentation::kFloat32;
        for (size_t i = 0; i < moves->size(); ++i) {
          auto move = (*moves)[i];
          if (!move->IsEliminated() && move->destination().IsFloatRegister())
            PerformMove(moves, move);
        }
      }
      if ((fp_reps & RepresentationBit(MachineRepresentation::kFloat64)) != 0) {
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
  // {PerformMoveHelper} assembles all the moves that block {move} but are not
  // blocked by {move} (directly or indirectly). By the assumptions described in
  // {PerformMoveHelper}, at most one cycle is left. If it exists, it is
  // returned and we assemble it below.
  auto cycle = PerformMoveHelper(moves, move);
  if (!cycle.has_value()) return;
  DCHECK_EQ(cycle->back(), move);
  if (cycle->size() == 2) {
    // A cycle of size two where the two moves have the same machine
    // representation is a swap. For this case, call {AssembleSwap} which can
    // generate better code than the generic algorithm below in some cases.
    MoveOperands* move2 = cycle->front();
    MachineRepresentation rep =
        LocationOperand::cast(move->source()).representation();
    MachineRepresentation rep2 =
        LocationOperand::cast(move2->source()).representation();
    if (rep == rep2) {
      InstructionOperand* source = &move->source();
      InstructionOperand* destination = &move->destination();
      // Ensure source is a register or both are stack slots, to limit swap
      // cases.
      if (source->IsAnyStackSlot()) {
        std::swap(source, destination);
      }
      assembler_->AssembleSwap(source, destination);
      move->Eliminate();
      move2->Eliminate();
      return;
    }
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
      LocationOperand::cast(move->source()).representation();
  cycle->pop_back();
  for (auto* pending_move : *cycle) {
    assembler_->SetPendingMove(pending_move);
  }
  assembler_->MoveToTempLocation(&move->source());
  InstructionOperand destination = move->destination();
  move->Eliminate();
  for (auto* unblocked_move : *cycle) {
    assembler_->AssembleMove(&unblocked_move->source(),
                             &unblocked_move->destination());
    unblocked_move->Eliminate();
  }
  assembler_->MoveTempLocationTo(&destination, rep);
}

base::Optional<std::vector<MoveOperands*>> GapResolver::PerformMoveHelper(
    ParallelMove* moves, MoveOperands* move) {
  // We interpret moves as nodes in a graph. x is a successor of y (x blocks y)
  // if x.source() conflicts with y.destination(). We recursively assemble the
  // moves in this graph in post-order using a DFS traversal, such that all
  // blocking moves are assembled first.
  // We also mark moves in the current DFS branch as pending. If a move is
  // blocked by a pending move, this is a cycle. In this case we just return the
  // cycle without assembling the moves, and the cycle is processed separately
  // by {PerformMove}.
  // We can show that there is at most one cycle in this connected component
  // with the two following assumptions:
  // - Two moves cannot have conflicting destinations (1)
  // - Operand conflict is transitive (2)
  // From this, it it follows that:
  // - A move cannot block two or more moves (or the destinations of the blocked
  // moves would conflict with each other by (2)).
  // - Therefore the graph is a tree, except possibly for one cycle that goes
  // back to the root
  // (1) must hold by construction of parallel moves. (2) is generally true,
  // except if this is a tail-call gap move and some operands span multiple
  // stack slots. In this case, slots can partially overlap and interference is
  // not transitive. In other cases, conflicting stack operands should have the
  // same base address and machine representation.
  // TODO(thibaudm): Fix the tail-call case.
  DCHECK(!move->IsPending());
  DCHECK(!move->IsRedundant());

  // Clear this move's destination to indicate a pending move.  The actual
  // destination is saved on the side.
  InstructionOperand source = move->source();
  DCHECK(!source.IsInvalid());  // Or else it will look eliminated.
  InstructionOperand destination = move->destination();
  move->SetPending();
  base::Optional<std::vector<MoveOperands*>> cycle;

  // We may need to split moves between FP locations differently.
  const bool is_fp_loc_move = kFPAliasing == AliasingKind::kCombine &&
                              destination.IsFPLocationOperand();

  for (size_t i = 0; i < moves->size(); ++i) {
    auto other = (*moves)[i];
    if (other->IsEliminated()) continue;
    if (other == move) continue;
    if (other->source().InterferesWith(destination)) {
      if (other->IsPending()) {
        // The conflicting move is pending, we found a cycle. Build the list of
        // moves that belong to the cycle on the way back.
        cycle.emplace();
      } else {
        // Recursively perform the conflicting move.
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
        auto cycle_rec = PerformMoveHelper(moves, other);
        if (cycle_rec.has_value()) {
          // Check that our assumption that there is at most one cycle is true.
          DCHECK(!cycle.has_value());
          cycle = cycle_rec;
        }
      }
    }
  }

  // We finished processing all the blocking moves and don't need this one
  // marked as pending anymore, restore its destination.
  move->set_destination(destination);

  if (cycle.has_value()) {
    // Do not assemble the moves in the cycle, just return it.
    cycle->push_back(move);
    return cycle;
  }

  assembler_->AssembleMove(&source, &destination);
  move->Eliminate();
  return {};
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

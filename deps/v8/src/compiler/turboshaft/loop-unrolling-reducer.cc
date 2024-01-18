// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/loop-unrolling-reducer.h"

#include "src/compiler/turboshaft/index.h"

namespace v8::internal::compiler::turboshaft {

void LoopFinder::Run() {
  ZoneVector<Block*> all_loops(phase_zone_);
  for (Block& block : base::Reversed(input_graph_->blocks())) {
    if (block.IsLoop()) {
      LoopInfo info = VisitLoop(&block);
      loop_header_info_.insert({&block, info});
    }
  }
}

// Update the `parent_loops_` of all of the blocks that are inside of the loop
// that starts on `header`.
LoopFinder::LoopInfo LoopFinder::VisitLoop(Block* header) {
  Block* backedge = header->LastPredecessor();
  DCHECK(backedge->LastOperation(*input_graph_).Is<GotoOp>());
  DCHECK_EQ(backedge->LastOperation(*input_graph_).Cast<GotoOp>().destination,
            header);
  DCHECK_GE(backedge->index().id(), header->index().id());

  LoopInfo info;

  queue_.clear();
  queue_.push_back(backedge);
  while (!queue_.empty()) {
    const Block* curr = queue_.back();
    queue_.pop_back();
    if (curr == header) continue;
    if (loop_headers_[curr->index()] != nullptr) {
      Block* curr_parent = loop_headers_[curr->index()];
      if (curr_parent == header) {
        // If {curr}'s parent is already marked as being {header}, then we've
        // already visited {curr}.
        continue;
      } else {
        // If {curr}'s parent is not {header}, then {curr} is part of an inner
        // loop. We should continue the search on the loop header: the
        // predecessors of {curr} will all be in this inner loop.
        queue_.push_back(curr_parent);
        info.has_inner_loops = true;
        continue;
      }
    }
    info.block_count++;
    info.op_count += curr->end().id() - curr->begin().id();
    loop_headers_[curr->index()] = header;
    Block* pred_start = curr->LastPredecessor();
    if (curr->IsLoop()) {
      // Skipping the backedge of inner loops since we don't want to visit inner
      // loops now (they should already have been visited).
      DCHECK_NOT_NULL(pred_start);
      pred_start = pred_start->NeighboringPredecessor();
      info.has_inner_loops = true;
    }
    for (Block* pred : NeighboringPredecessorIterable(pred_start)) {
      queue_.push_back(pred);
    }
  }

  info.start = header;
  info.end = backedge;
  // We increment the `block_count` by 1 to account for the loop header.
  info.block_count += 1;

  return info;
}

void LoopUnrollingAnalyzer::DetectUnrollableLoops() {
  for (const auto& [start, info] : loop_finder_.LoopHeaders()) {
    if (!info.has_inner_loops) {
      int iter_count;
      if (CanFullyUnrollLoop(info, &iter_count)) {
        loop_iteration_count_.insert({start, iter_count});
      }
    }
  }
}

bool LoopUnrollingAnalyzer::CanFullyUnrollLoop(const LoopFinder::LoopInfo& info,
                                               int* iter_count) const {
  Block* start = info.start;
  DCHECK(start->IsLoop());

  // Checking that the loop doesn't contain too many instructions.
  if (info.op_count > kMaxLoopSizeForFullUnrolling) {
    return false;
  }

  // Checking that the condition for the loop can be computed statically, and
  // that the loop contains no more than kMaxLoopIterationsForFullUnrolling
  // iterations.
  const BranchOp* branch =
      start->LastOperation(*input_graph_).TryCast<BranchOp>();
  if (!branch) {
    // This looks like an infinite loop, or like something weird is used to
    // decide whether to loop or not.
    return false;
  }

  // Checking that one of the successor of the loop header is indeed not in the
  // loop (otherwise, the Branch that ends the loop header is not the Branch
  // that decides to exit the loop).
  if (loop_finder_.GetLoopHeader(branch->if_true) ==
      loop_finder_.GetLoopHeader(branch->if_false)) {
    return false;
  }

  return canonical_loop_matcher_.MatchStaticCanonicalForLoop(
      branch->condition(), iter_count);
}

// Tries to match `phi cmp cst` (or `cst cmp phi`).
bool StaticCanonicalForLoopMatcher::MatchPhiCompareCst(
    OpIndex cond_idx, StaticCanonicalForLoopMatcher::CmpOp* cmp_op,
    OpIndex* phi, uint64_t* cst) const {
  const Operation& cond = matcher_.Get(cond_idx);

  if (const ComparisonOp* cmp = cond.TryCast<ComparisonOp>()) {
    *cmp_op = ComparisonKindToCmpOp(cmp->kind);
  } else if (cond.Is<EqualOp>()) {
    *cmp_op = CmpOp::kEqual;
  } else {
    return false;
  }

  OpIndex left = cond.input(0);
  OpIndex right = cond.input(1);

  if (matcher_.MatchPhi(left, 2)) {
    if (matcher_.MatchUnsignedIntegralConstant(right, cst)) {
      *phi = left;
      return true;
    }
  } else if (matcher_.MatchPhi(right, 2)) {
    if (matcher_.MatchUnsignedIntegralConstant(left, cst)) {
      *cmp_op = InvertComparisonOp(*cmp_op);
      *phi = right;
      return true;
    }
  }
  return false;
}

bool StaticCanonicalForLoopMatcher::MatchCheckedOverflowBinop(
    OpIndex idx, OpIndex* left, OpIndex* right, BinOp* binop_op,
    WordRepresentation* binop_rep) const {
  if (const ProjectionOp* proj = matcher_.TryCast<ProjectionOp>(idx)) {
    if (proj->index != OverflowCheckedBinopOp::kValueIndex) return false;
    if (const OverflowCheckedBinopOp* binop =
            matcher_.TryCast<OverflowCheckedBinopOp>(proj->input())) {
      *left = binop->left();
      *right = binop->right();
      *binop_op = BinopFromOverflowCheckedBinopKind(binop->kind);
      *binop_rep = binop->rep;
      return true;
    }
  }
  return false;
}

bool StaticCanonicalForLoopMatcher::MatchWordBinop(
    OpIndex idx, OpIndex* left, OpIndex* right, BinOp* binop_op,
    WordRepresentation* binop_rep) const {
  WordBinopOp::Kind kind;
  if (matcher_.MatchWordBinop(idx, left, right, &kind, binop_rep) &&
      BinopKindIsSupported(kind)) {
    *binop_op = BinopFromWordBinopKind(kind);
    return true;
  }
  return false;
}

bool StaticCanonicalForLoopMatcher::MatchStaticCanonicalForLoop(
    OpIndex cond_idx, int* iter_count) const {
  CmpOp cmp_op;
  OpIndex phi_idx;
  uint64_t cmp_cst;
  if (!MatchPhiCompareCst(cond_idx, &cmp_op, &phi_idx, &cmp_cst)) {
    return false;
  }
  const PhiOp& phi = matcher_.Cast<PhiOp>(phi_idx);

  // We have: phi(..., ...) cmp_op cmp_cst
  // eg, for (i = ...; i < 42; ...)
  uint64_t phi_cst;
  if (matcher_.MatchUnsignedIntegralConstant(phi.input(0), &phi_cst)) {
    // We have: phi(phi_cst, ...) cmp_op cmp_cst
    // eg, for (i = 0; i < 42; ...)
    OpIndex left, right;
    BinOp binop_op;
    WordRepresentation binop_rep;
    if (MatchWordBinop(phi.input(1), &left, &right, &binop_op, &binop_rep) ||
        MatchCheckedOverflowBinop(phi.input(1), &left, &right, &binop_op,
                                  &binop_rep)) {
      // We have: phi(phi_cst, ... binop_op ...) cmp_op cmp_cst
      // eg, for (i = 0; i < 42; i = ... + ...)
      if (left == phi_idx) {
        // We have: phi(phi_cst, phi binop_op ...) cmp_op cmp_cst
        // eg, for (i = 0; i < 42; i = i + ...)
        uint64_t binop_cst;
        if (matcher_.MatchUnsignedIntegralConstant(right, &binop_cst)) {
          // We have: phi(phi_cst, phi binop_op binop_cst) cmp_op cmp_cst
          // eg, for (i = 0; i < 42; i = i + 2)
          return HasFewIterations(cmp_cst, cmp_op, phi_cst, binop_cst, binop_op,
                                  binop_rep, iter_count);
        }
      } else if (right == phi_idx) {
        // We have: phi(phi_cst, ... binop_op phi) cmp_op cmp_cst
        // eg, for (i = 0; i < 42; i = ... + i)
        uint64_t binop_cst;
        if (matcher_.MatchUnsignedIntegralConstant(left, &binop_cst)) {
          // We have: phi(phi_cst, binop_cst binop_op phi) cmp_op cmp_cst
          // eg, for (i = 0; i < 42; i = 2 + i)
          return HasFewIterations(cmp_cst, cmp_op, phi_cst, binop_cst, binop_op,
                                  binop_rep, iter_count);
        }
      }
    }
  }

  // The condition is not an operation that we support.
  return false;
}

constexpr bool StaticCanonicalForLoopMatcher::BinopKindIsSupported(
    WordBinopOp::Kind binop_kind) {
  switch (binop_kind) {
    // This list needs to be kept in sync with the `Next` function that follows.
    case WordBinopOp::Kind::kAdd:
    case WordBinopOp::Kind::kMul:
    case WordBinopOp::Kind::kSub:
    case WordBinopOp::Kind::kBitwiseAnd:
    case WordBinopOp::Kind::kBitwiseOr:
    case WordBinopOp::Kind::kBitwiseXor:
      return true;
    default:
      return false;
  }
}

constexpr StaticCanonicalForLoopMatcher::BinOp
StaticCanonicalForLoopMatcher::BinopFromWordBinopKind(WordBinopOp::Kind kind) {
  DCHECK(BinopKindIsSupported(kind));
  switch (kind) {
    case WordBinopOp::Kind::kAdd:
      return BinOp::kAdd;
    case WordBinopOp::Kind::kMul:
      return BinOp::kMul;
    case WordBinopOp::Kind::kSub:
      return BinOp::kSub;
    case WordBinopOp::Kind::kBitwiseAnd:
      return BinOp::kBitwiseAnd;
    case WordBinopOp::Kind::kBitwiseOr:
      return BinOp::kBitwiseOr;
    case WordBinopOp::Kind::kBitwiseXor:
      return BinOp::kBitwiseXor;
    default:
      UNREACHABLE();
  }
}

constexpr StaticCanonicalForLoopMatcher::BinOp
StaticCanonicalForLoopMatcher::BinopFromOverflowCheckedBinopKind(
    OverflowCheckedBinopOp::Kind kind) {
  switch (kind) {
    case OverflowCheckedBinopOp::Kind::kSignedAdd:
      return BinOp::kOverflowCheckedAdd;
    case OverflowCheckedBinopOp::Kind::kSignedMul:
      return BinOp::kOverflowCheckedMul;
    case OverflowCheckedBinopOp::Kind::kSignedSub:
      return BinOp::kOverflowCheckedSub;
  }
}

namespace {
using CmpOp = StaticCanonicalForLoopMatcher::CmpOp;
using BinOp = StaticCanonicalForLoopMatcher::BinOp;

template <class Int>
base::Optional<Int> Next(Int val, Int incr,
                         StaticCanonicalForLoopMatcher::BinOp binop_op,
                         WordRepresentation binop_rep) {
  switch (binop_op) {
    case BinOp::kAdd:
      return val + incr;
    case BinOp::kMul:
      return val * incr;
    case BinOp::kSub:
      return val - incr;
    case BinOp::kBitwiseAnd:
      return val & incr;
    case BinOp::kBitwiseOr:
      return val | incr;
    case BinOp::kBitwiseXor:
      return val ^ incr;
#define CASE_CHECKED(op)                                                      \
  case BinOp::kOverflowChecked##op: {                                         \
    if (binop_rep == WordRepresentation::Word32()) {                          \
      int32_t res;                                                            \
      if (base::bits::Signed##op##Overflow32(                                 \
              static_cast<int32_t>(val), static_cast<int32_t>(incr), &res)) { \
        return base::nullopt;                                                 \
      }                                                                       \
      return static_cast<Int>(res);                                           \
    } else {                                                                  \
      DCHECK_EQ(binop_rep, WordRepresentation::Word64());                     \
      int64_t res;                                                            \
      if (base::bits::Signed##op##Overflow64(val, incr, &res)) {              \
        return base::nullopt;                                                 \
      }                                                                       \
      return static_cast<Int>(res);                                           \
    }                                                                         \
  }
      CASE_CHECKED(Add)
      CASE_CHECKED(Mul)
      CASE_CHECKED(Sub)
#undef CASE_CHECKED
  }
}

template <class Int>
bool Cmp(Int val, Int max, CmpOp cmp_op) {
  switch (cmp_op) {
    case CmpOp::kSignedLessThan:
    case CmpOp::kUnsignedLessThan:
      return val < max;
    case CmpOp::kSignedLessThanOrEqual:
    case CmpOp::kUnsignedLessThanOrEqual:
      return val <= max;
    case CmpOp::kSignedGreaterThan:
    case CmpOp::kUnsignedGreaterThan:
      return val > max;
    case CmpOp::kSignedGreaterThanOrEqual:
    case CmpOp::kUnsignedGreaterThanOrEqual:
      return val >= max;
    case CmpOp::kEqual:
      return val != max;
  }
}

// Returns true if the loop `for (i = init, i cmp_op max; i = i binop_cst
// binop_op)` has fewer than `max_iter_` iterations.
template <class Int>
bool HasFewerIterationsThan(Int init, Int max, CmpOp cmp_op, Int binop_cst,
                            StaticCanonicalForLoopMatcher::BinOp binop_op,
                            WordRepresentation binop_rep, const int max_iter_,
                            int* iter_count) {
  static_assert(std::is_integral_v<Int>);
  DCHECK_EQ(std::is_unsigned_v<Int>,
            (cmp_op == CmpOp::kUnsignedLessThan ||
             cmp_op == CmpOp::kUnsignedLessThanOrEqual ||
             cmp_op == CmpOp::kUnsignedGreaterThan ||
             cmp_op == CmpOp::kUnsignedGreaterThanOrEqual));
  // It's a bit hard to compute the number of iterations without some kind of
  // (simple) SMT solver, especially when taking overflows into account. Thus,
  // we just simulate the evolution of the loop counter: we repeatedly compute
  // `init binop_op binop_cst`, and compare the result with `max`. This is
  // somewhat inefficient, so it should only be done if `max_iter_` is small.
  DCHECK_LE(max_iter_, 5);

  Int curr = init;
  for (int i = 0; i < max_iter_; i++) {
    if (!Cmp(curr, max, cmp_op)) {
      *iter_count = i;
      return true;
    }
    if (auto next = Next(curr, binop_cst, binop_op, binop_rep)) {
      curr = *next;
    } else {
      // There was an overflow, bailing out.
      return false;
    }
  }
  return false;
}
}  // namespace

// Returns true if the loop `for (i = init, i cmp_op max; i = i binop_cst
// binop_op)` has fewer than `max_iter_` iterations.
bool StaticCanonicalForLoopMatcher::HasFewIterations(
    uint64_t cmp_cst, CmpOp cmp_op, uint64_t initial_input, uint64_t binop_cst,
    StaticCanonicalForLoopMatcher::BinOp binop_op, WordRepresentation binop_rep,
    int* iter_count) const {
  switch (cmp_op) {
    case CmpOp::kSignedLessThan:
    case CmpOp::kSignedLessThanOrEqual:
    case CmpOp::kSignedGreaterThan:
    case CmpOp::kSignedGreaterThanOrEqual:
    case CmpOp::kEqual:
      return HasFewerIterationsThan(static_cast<int64_t>(initial_input),
                                    static_cast<int64_t>(cmp_cst), cmp_op,
                                    static_cast<int64_t>(binop_cst), binop_op,
                                    binop_rep, max_iter_, iter_count);
    case CmpOp::kUnsignedLessThan:
    case CmpOp::kUnsignedLessThanOrEqual:
    case CmpOp::kUnsignedGreaterThan:
    case CmpOp::kUnsignedGreaterThanOrEqual:
      return HasFewerIterationsThan(initial_input, cmp_cst, cmp_op, binop_cst,
                                    binop_op, binop_rep, max_iter_, iter_count);
  }
}

ZoneSet<Block*, LoopUnrollingAnalyzer::BlockCmp>
LoopUnrollingAnalyzer::GetLoopBody(Block* loop_header) {
  DCHECK(!loop_finder_.GetLoopInfo(loop_header).has_inner_loops);
  ZoneSet<Block*, BlockCmp> body(phase_zone_);
  body.insert(loop_header);

  ZoneVector<Block*> queue(phase_zone_);
  queue.push_back(loop_header->LastPredecessor());
  while (!queue.empty()) {
    Block* curr = queue.back();
    queue.pop_back();
    if (body.find(curr) != body.end()) continue;
    body.insert(curr);
    for (Block* pred = curr->LastPredecessor(); pred != nullptr;
         pred = pred->NeighboringPredecessor()) {
      if (pred == loop_header) continue;
      queue.push_back(pred);
    }
  }

  return body;
}

constexpr StaticCanonicalForLoopMatcher::CmpOp
StaticCanonicalForLoopMatcher::ComparisonKindToCmpOp(ComparisonOp::Kind kind) {
  switch (kind) {
    case ComparisonOp::Kind::kSignedLessThan:
      return CmpOp::kSignedLessThan;
    case ComparisonOp::Kind::kSignedLessThanOrEqual:
      return CmpOp::kSignedLessThanOrEqual;
    case ComparisonOp::Kind::kUnsignedLessThan:
      return CmpOp::kUnsignedLessThan;
    case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
      return CmpOp::kUnsignedLessThanOrEqual;
  }
}
constexpr StaticCanonicalForLoopMatcher::CmpOp
StaticCanonicalForLoopMatcher::InvertComparisonOp(CmpOp op) {
  switch (op) {
    case CmpOp::kEqual:
      return CmpOp::kEqual;
    case CmpOp::kSignedLessThan:
      return CmpOp::kSignedGreaterThanOrEqual;
    case CmpOp::kSignedLessThanOrEqual:
      return CmpOp::kSignedGreaterThan;
    case CmpOp::kUnsignedLessThan:
      return CmpOp::kUnsignedGreaterThanOrEqual;
    case CmpOp::kUnsignedLessThanOrEqual:
      return CmpOp::kUnsignedGreaterThan;
    case CmpOp::kSignedGreaterThan:
      return CmpOp::kSignedLessThanOrEqual;
    case CmpOp::kSignedGreaterThanOrEqual:
      return CmpOp::kSignedLessThan;
    case CmpOp::kUnsignedGreaterThan:
      return CmpOp::kUnsignedLessThanOrEqual;
    case CmpOp::kUnsignedGreaterThanOrEqual:
      return CmpOp::kUnsignedLessThan;
  }
}

}  // namespace v8::internal::compiler::turboshaft

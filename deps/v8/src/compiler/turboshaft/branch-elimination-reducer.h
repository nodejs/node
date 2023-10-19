// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_BRANCH_ELIMINATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_BRANCH_ELIMINATION_REDUCER_H_

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/layered-hash-map.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/utils/utils.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <class Next>
class BranchEliminationReducer : public Next {
  // # General overview
  //
  // BranchEliminationAssembler optimizes branches in a few ways:
  //
  //   1- When a branch is nested in another branch and uses the same condition,
  //     then we can get rid of this branch and keep only the correct target.
  //     For instance:
  //
  //         if (cond) {
  //              if (cond) print("B1");
  //              else print("B2");
  //         } else {
  //              if (cond) print("B3");
  //              else print("B4");
  //         }
  //
  //     Will be simplified to:
  //
  //         if (cond) {
  //              print("B1");
  //         } else {
  //              print("B4");
  //         }
  //
  //     Because the 1st nested "if (cond)" is always true, and the 2nd is
  //     always false.
  //
  //     Or, if you prefer a more graph-oriented visual representation:
  //
  //           condition                             condition
  //           |   |   |                                 |
  //       -----   |   ------                            |
  //       |       |        |                            |
  //       |       v        |                            v
  //       |     branch     |                         branch
  //       |     /     \    |                          /   \
  //       |    /       \   |                         /     \
  //       v   /         \  v         becomes        v       v
  //       branch      branch         ======>       B1       B4
  //        /  \        /  \
  //       /    \      /    \
  //      B1     B2   B3     B4
  //
  //
  //   2- When 2 consecutive branches (where the 2nd one is after the merging of
  //     the 1st one) have the same condition, we can pull up the 2nd branch to
  //     get rid of the merge of the 1st branch and the branch of the 2nd
  //     branch. For instance:
  //
  //         if (cond) {
  //             B1;
  //         } else {
  //             B2;
  //         }
  //         B3;
  //         if (cond) {
  //             B4;
  //         } else {
  //             B5;
  //         }
  //
  //     Will be simplified to:
  //
  //         if (cond) {
  //             B1;
  //             B3;
  //             B4;
  //         } else {
  //             B2;
  //             B3;
  //             B5;
  //         }
  //
  //     Or, if you prefer a more graph-oriented visual representation:
  //
  //           condition                           condition
  //           |     |                                 |
  //     -------     |                                 |
  //     |           v                                 v
  //     |        branch                            branch
  //     |         /  \                              /  \
  //     |        /    \                            /    \
  //     |       B1    B2                          B1    B2
  //     |        \    /                           |     |
  //     |         \  /         becomes            |     |
  //     |        merge1        ======>            B3    B3
  //     |          B3                             |     |
  //     -------> branch                           |     |
  //               /  \                            B4    B5
  //              /    \                            \    /
  //             B4    B5                            \  /
  //              \    /                             merge
  //               \  /
  //              merge2
  //
  //   2bis- In the 2nd optimization, if `cond` is a Phi of 2 values that come
  //   from B1 and B2, then the same optimization can be applied for a similar
  //   result. For instance:
  //
  //     if (cond) {                                if (cond) {
  //       x = 1                                        x = 1;
  //     } else {                becomes                B1
  //       x = 0                 ======>            } else {
  //     }                                              x = 0;
  //     if (x) B1 else B2                              B2;
  //                                                }
  //
  //   If `x` is more complex than a simple boolean, then the 2nd branch will
  //   remain, except that it will be on `x`'s value directly rather than on a
  //   Phi (so, it avoids creating a Phi, and it will probably be better for
  //   branch prediction).
  //
  //
  //   3- Optimizing {Return} nodes through merges. It checks that
  //    the return value is actually a {Phi} and the Return is dominated
  //    only by the Phi.
  //
  //    if (c) {                         if (c) {
  //       v = 42;            ====>         v = 42;
  //    } else {                            return v;
  //       v = 5;                        } else {
  //    }                                   v = 5;
  //    return v;                           return v;
  //                                     }
  //
  //    And here's the graph representation:
  //
  //    +----B1----+    <Some other           +----B1'----+     +----B2'----+
  //    | p1 = ... |      block(s):           | p1 = ...  |     | p2 = ...  |
  //    | <...>    |      B2,...>             | <...>     |     | <...>     |
  //    +----------+        /                 | return p1 |     | return p2 |
  //         \             /                  +-----------+     +-----------+
  //          \           /          =====>
  //           \         /
  //            \       |
  //        +--------B3-------+
  //        | p = Phi(p1,...) |
  //        | <...>           |
  //        | return p        |
  //        +-----------------+
  //
  //
  //    4- Eliminating merges: if the 2 merged branches are empty,
  //    and the merge block doesn't have a Phi (which is either the first
  //    operation or is only preceded by FrameState operations),
  //    we can remove the merge and instead Goto the block from the new graph.
  //
  // # Technical overview of the implementation
  //
  // We iterate the graph in dominator order, and maintain a hash map of
  // conditions with a resolved value along the current path. For instance, if
  // we have:
  //     if (c) { B1 } else { B2 }
  // when iterating B1, we'll know that |c| is true, while when iterating
  // over B2, we'll know that |c| is false.
  // When reaching a Branch, we'll insert the condition in the hash map, while
  // when reaching a Merge, we'll remove it.
  //
  // Then, the 1st optimization (nested branches with the same condition) is
  // trivial: we just look in the hashmap if the condition is known, and only
  // generate the right branch target without generating the branch itself.
  //
  // For the 2nd optimization, when generating a Goto, we check if the
  // destination block ends with a branch whose condition is already known. If
  // that's the case, then we copy the destination block, and the 1st
  // optimization will replace its final Branch by a Goto when reaching it.
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  void Bind(Block* new_block) {
    Next::Bind(new_block);

    if (ShouldSkipOptimizationStep()) {
      // It's important to have a ShouldSkipOptimizationStep here, because
      // {known_conditions_} assumes that we perform all branch elimination
      // possible (which implies that we don't ever insert twice the same thing
      // in {known_conditions_}). If we stop doing ReduceBranch because of
      // ShouldSkipOptimizationStep, then this assumption doesn't hold anymore,
      // and we should thus stop updating {known_conditions_} to not trigger
      // some DCHECKs.
      return;
    }

    // Update {known_conditions_} based on where {new_block} is in the dominator
    // tree.
    ResetToBlock(new_block);
    ReplayMissingPredecessors(new_block);
    StartLayer(new_block);

    if (new_block->IsBranchTarget()) {
      // The current block is a branch target, so we add the branch condition
      // along with its value in {known_conditions_}.
      DCHECK_EQ(new_block->PredecessorCount(), 1);
      const Operation& op =
          new_block->LastPredecessor()->LastOperation(Asm().output_graph());
      if (const BranchOp* branch = op.TryCast<BranchOp>()) {
        DCHECK_EQ(new_block, any_of(branch->if_true, branch->if_false));
        bool condition_value = branch->if_true == new_block;
        if (!known_conditions_.Contains(branch->condition())) {
          known_conditions_.InsertNewKey(branch->condition(), condition_value);
        }
      }
    }
  }

  OpIndex REDUCE(Branch)(OpIndex cond, Block* if_true, Block* if_false,
                         BranchHint hint) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceBranch(cond, if_true, if_false, hint);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    if (const Block* if_true_origin = if_true->OriginForBlockStart()) {
      if (const Block* if_false_origin = if_false->OriginForBlockStart()) {
        const Operation& first_op_true =
            if_true_origin->FirstOperation(Asm().input_graph());
        const Operation& first_op_false =
            if_false_origin->FirstOperation(Asm().input_graph());
        const GotoOp* true_goto = first_op_true.template TryCast<GotoOp>();
        const GotoOp* false_goto = first_op_false.template TryCast<GotoOp>();
        // We apply the fourth optimization, replacing empty braches with a
        // Goto to their destination (if it's the same block).
        if (true_goto && false_goto &&
            true_goto->destination == false_goto->destination) {
          Block* merge_block = true_goto->destination;
          if (!merge_block->HasPhis(Asm().input_graph())) {
            // Using `ReduceInputGraphGoto()` here enables more optimizations.
            Asm().Goto(merge_block->MapToNextGraph());
            return OpIndex::Invalid();
          }
        }
      }
    }

    if (auto cond_value = known_conditions_.Get(cond)) {
      // We already know the value of {cond}. We thus remove the branch (this is
      // the "first" optimization in the documentation at the top of this
      // module).
      return Asm().ReduceGoto(*cond_value ? if_true : if_false);
    }
    // We can't optimize this branch.
    goto no_change;
  }

  OpIndex REDUCE(Select)(OpIndex cond, OpIndex vtrue, OpIndex vfalse,
                         RegisterRepresentation rep, BranchHint hint,
                         SelectOp::Implementation implem) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceSelect(cond, vtrue, vfalse, rep, hint, implem);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    if (auto cond_value = known_conditions_.Get(cond)) {
      if (*cond_value) {
        return vtrue;
      } else {
        return vfalse;
      }
    }
    goto no_change;
  }

  OpIndex REDUCE(Goto)(Block* destination) {
    LABEL_BLOCK(no_change) { return Next::ReduceGoto(destination); }
    if (ShouldSkipOptimizationStep()) goto no_change;

    const Block* destination_origin = destination->OriginForBlockStart();
    if (!destination_origin || !destination_origin->IsMerge()) goto no_change;

    if (destination_origin->HasExactlyNPredecessors(1)) {
      // There is no point in trying the 2nd optimization: this would remove
      // neither Phi nor Branch.
      // TODO(dmercadier, tebbi): this block has a single predecessor and a
      // single successor, so we might want to inline it.
      goto no_change;
    }

    const Operation& last_op =
        destination_origin->LastOperation(Asm().input_graph());
    if (const BranchOp* branch = last_op.template TryCast<BranchOp>()) {
      OpIndex condition =
          Asm().template MapToNewGraph<true>(branch->condition());
      if (condition.valid()) {
        base::Optional<bool> condition_value = known_conditions_.Get(condition);
        if (!condition_value.has_value()) {
          // We've already visited the subsequent block's Branch condition, but
          // we don't know its value right now.
          goto no_change;
        }

        // The next block {new_dst} is a Merge, and ends with a Branch whose
        // condition is already known. As per the 2nd optimization, we'll
        // process {new_dst} right away, and we'll end it with a Goto instead of
        // its current Branch.
        Asm().CloneAndInlineBlock(destination_origin);
        return OpIndex::Invalid();
      } else {
        // Optimization 2bis:
        // {condition} hasn't been visited yet, and thus it doesn't have a
        // mapping to the new graph. However, if it's the result of a Phi whose
        // input is coming from the current block, then it still makes sense to
        // inline {destination_origin}: the condition will then be known.
        if (destination_origin->Contains(branch->condition())) {
          const PhiOp* cond = Asm()
                                  .input_graph()
                                  .Get(branch->condition())
                                  .template TryCast<PhiOp>();
          if (!cond) goto no_change;
          Asm().CloneAndInlineBlock(destination_origin);
          return OpIndex::Invalid();
        }
      }
    } else if (const ReturnOp* return_op =
                   last_op.template TryCast<ReturnOp>()) {
      // The destination block in the old graph ends with a Return
      // and the old destination is a merge block, so we can directly
      // inline the destination block in place of the Goto.
      // TODO(nicohartmann@): Temporarily disable this "optimization" because
      // it prevents dead code elimination in some cases. Reevaluate this and
      // reenable if phases have been reordered properly.
      // Asm().CloneAndInlineBlock(old_dst);
      // return OpIndex::Invalid();
    }

    goto no_change;
  }

  OpIndex REDUCE(DeoptimizeIf)(OpIndex condition, OpIndex frame_state,
                               bool negated,
                               const DeoptimizeParameters* parameters) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceDeoptimizeIf(condition, frame_state, negated,
                                      parameters);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    base::Optional<bool> condition_value = known_conditions_.Get(condition);
    if (!condition_value.has_value()) {
      known_conditions_.InsertNewKey(condition, negated);
      goto no_change;
    }

    if ((*condition_value && !negated) || (!*condition_value && negated)) {
      // The condition is true, so we always deoptimize.
      return Next::ReduceDeoptimize(frame_state, parameters);
    } else {
      // The condition is false, so we never deoptimize.
      return OpIndex::Invalid();
    }
  }

  OpIndex REDUCE(TrapIf)(OpIndex condition, OpIndex frame_state, bool negated,
                         const TrapId trap_id) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceTrapIf(condition, frame_state, negated, trap_id);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    base::Optional<bool> condition_value = known_conditions_.Get(condition);
    if (!condition_value.has_value()) {
      known_conditions_.InsertNewKey(condition, negated);
      goto no_change;
    }

    if (Asm().template Is<ConstantOp>(condition)) {
      goto no_change;
    }

    OpIndex static_condition = Asm().Word32Constant(*condition_value);
    if (negated) {
      Asm().TrapIfNot(static_condition, frame_state, trap_id);
    } else {
      Asm().TrapIf(static_condition, frame_state, trap_id);
    }
    return OpIndex::Invalid();
  }

 private:
  // Resets {known_conditions_} and {dominator_path_} up to the 1st dominator of
  // {block} that they contain.
  void ResetToBlock(Block* block) {
    Block* target = block->GetDominator();
    while (!dominator_path_.empty() && target != nullptr &&
           dominator_path_.back() != target) {
      if (dominator_path_.back()->Depth() > target->Depth()) {
        ClearCurrentEntries();
      } else if (dominator_path_.back()->Depth() < target->Depth()) {
        target = target->GetDominator();
      } else {
        // {target} and {dominator_path.back} have the same depth but are not
        // equal, so we go one level up for both.
        ClearCurrentEntries();
        target = target->GetDominator();
      }
    }
  }

  // Removes the latest entry in {known_conditions_} and {dominator_path_}.
  void ClearCurrentEntries() {
    known_conditions_.DropLastLayer();
    dominator_path_.pop_back();
  }

  void StartLayer(Block* block) {
    known_conditions_.StartLayer();
    dominator_path_.push_back(block);
  }

  // ReplayMissingPredecessors adds to {known_conditions_} and {dominator_path_}
  // the conditions/blocks that related to the dominators of {block} that are
  // not already present. This can happen when control-flow changes during the
  // OptimizationPhase, which results in a block being visited not right after
  // its dominator. For instance, when optimizing a double-diamond like:
  //
  //                  B0
  //                 /  \
  //                /    \
  //               B1    B2
  //                \    /
  //                 \  /
  //                  B3
  //                 /  \
  //                /    \
  //               B4    B5
  //                \    /
  //                 \  /
  //                  B6
  //                 /  \
  //                /    \
  //               B7    B8
  //                \    /
  //                 \  /
  //                  B9
  //
  // In this example, where B0, B3 and B6 branch on the same condition, the
  // blocks are actually visited in the following order: B0 - B1 - B3/1 - B2 -
  // B3/2 - B4 - B5 - ... (note how B3 is duplicated and visited twice because
  // from B1/B2 its branch condition is already known; I've noted the duplicated
  // blocks as B3/1 and B3/2). In the new graph, the dominator of B4 is B3/1 and
  // the dominator of B5 is B3/2. Except that upon visiting B4, the last visited
  // block is not B3/1 but rather B3/2, so, we have to reset {known_conditions_}
  // to B0, and thus miss that we actually know branch condition of B0/B3/B6 and
  // we thus won't optimize the 3rd diamond.
  //
  // To overcome this issue, ReplayMissingPredecessors will add the information
  // of the missing predecessors of the current block to {known_conditions_}. In
  // the example above, this means that when visiting B4,
  // ReplayMissingPredecessors will add the information of B3/1 to
  // {known_conditions_}.
  void ReplayMissingPredecessors(Block* new_block) {
    // Collect blocks that need to be replayed.
    base::SmallVector<Block*, 32> missing_blocks;
    for (Block* dom = new_block->GetDominator();
         dom != nullptr && dom != dominator_path_.back();
         dom = dom->GetDominator()) {
      missing_blocks.push_back(dom);
    }
    // Actually does the replaying, starting from the oldest block and finishing
    // with the newest one (so that they will later be removed in the correct
    // order).
    for (auto it = missing_blocks.rbegin(); it != missing_blocks.rend(); ++it) {
      Block* block = *it;
      StartLayer(block);

      if (block->IsBranchTarget()) {
        const Operation& op =
            block->LastPredecessor()->LastOperation(Asm().output_graph());
        if (const BranchOp* branch = op.TryCast<BranchOp>()) {
          DCHECK(branch->if_true->index() == block->index() ||
                 branch->if_false->index() == block->index());
          bool condition_value =
              branch->if_true->index().valid()
                  ? branch->if_true->index() == block->index()
                  : branch->if_false->index() != block->index();
          known_conditions_.InsertNewKey(branch->condition(), condition_value);
        }
      }
    }
  }

  // TODO(dmercadier): use the SnapshotTable to replace {dominator_path_} and
  // {known_conditions_}, and to reuse the existing merging/replay logic of the
  // SnapshotTable.
  ZoneVector<Block*> dominator_path_{Asm().phase_zone()};
  LayeredHashMap<OpIndex, bool> known_conditions_{
      Asm().phase_zone(), Asm().input_graph().DominatorTreeDepth() * 2};
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_BRANCH_ELIMINATION_REDUCER_H_

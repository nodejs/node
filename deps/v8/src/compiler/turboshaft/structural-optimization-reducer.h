// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_STRUCTURAL_OPTIMIZATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_STRUCTURAL_OPTIMIZATION_REDUCER_H_

#include <cstdio>

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/index.h"
#include "src/zone/zone.h"

// The StructuralOptimizationReducer reducer is suitable for changing the
// graph in a way that doesn't reduce individual operations, rather changes
// the structure of the graph.
//
// We currently support a reduction which transforms if-else cascades
// that check if a given value is equal to a 32-bit constant from a given set
// into a switch with cases corresponding to the constants in the set.
//
// So for example code like:
//    [only pure ops 1]
//    if (x == 3) {
//      B1;
//    } else {
//      [only pure ops 2]
//      if (x == 5) {
//        B2;
//      } else {
//        B3;
//      }
//    }
//
// will be transformed to:
//    [only pure ops 1]
//    [only pure ops 2]
//    switch (x) {
//      case 3:
//        B1;
//      case 5:
//        B2;
//      default:
//        B3;
//    }
//
// Or represented graphically:
//                                                 [only pure ops 1]
//       [only pure ops 1]                         [only pure ops 2]
//           x == 3                                    Switch(x)
//           Branch                                    |    |   |
//           |    |                                -----    |   ------
//       -----    ------                    case 3 |        |        | default
//       |             |                           |        |        |
//     T |             | F                         v        |        |
//       v             v                           B1       |        v
//       B1      [only pure ops 2]    becomes               |        B3
//                   x == 5           ======>        case 5 |
//                   Branch                                 v
//                   |    |                                 B2
//               -----    ------
//               |             |
//             T |             | F
//               v             v
//              B2            B3
//

// TODO(mslekova): Introduce a flag and move to a common graph place.
// #define TRACE_REDUCTIONS
#ifdef TRACE_REDUCTIONS
#define TRACE(str, ...) \
  { PrintF(str, ##__VA_ARGS__); }
#else  // TRACE_REDUCTIONS
#define TRACE(str, ...)

#endif  // TRACE_REDUCTIONS

namespace v8::internal::compiler::turboshaft {

template <class Next>
class StructuralOptimizationReducer : public Next {
 public:
  using Next::Asm;

  OpIndex ReduceInputGraphBranch(OpIndex input_index, const BranchOp& branch) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceInputGraphBranch(input_index, branch);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    TRACE("[structural] Calling ReduceInputGraphBranch for index: %u\n",
          static_cast<unsigned int>(input_index.id()));

    base::SmallVector<SwitchOp::Case, 16> cases;
    base::SmallVector<const Block*, 16> false_blocks;

    Block* current_if_false;
    const BranchOp* current_branch = &branch;
    BranchHint default_hint = BranchHint::kNone;

    OpIndex switch_var = OpIndex::Invalid();
    while (true) {
      // If we encounter a condition that is not equality, we can't turn it
      // into a switch case.
      const EqualOp* equal = Asm()
                                 .input_graph()
                                 .Get(current_branch->condition())
                                 .template TryCast<EqualOp>();
      if (!equal || equal->rep != RegisterRepresentation::Word32()) {
        TRACE(
            "\t [bailout] Branch with different condition than Word32 "
            "Equal.\n");
        break;
      }

      // MachineOptimizationReducer should normalize equality to put constants
      // right.
      const Operation& right_op = Asm().input_graph().Get(equal->right());
      if (!right_op.Is<ConstantOp>()) {
        TRACE("\t [bailout] No constant on the right side of Equal.\n");
        break;
      }

      // We can only turn Word32 constant equals to switch cases.
      const ConstantOp& const_op = right_op.Cast<ConstantOp>();
      if (const_op.kind != ConstantOp::Kind::kWord32) {
        TRACE("\t [bailout] Constant is not of type Word32.\n");
        break;
      }

      // If we encounter equal to a different value, we can't introduce
      // a switch.
      OpIndex current_var = equal->left();
      if (!switch_var.valid()) {
        switch_var = current_var;
      } else if (switch_var != current_var) {
        TRACE("\t [bailout] Not all branches compare the same variable.\n");
        break;
      }

      Block* current_if_true = current_branch->if_true;
      current_if_false = current_branch->if_false;
      DCHECK(current_if_true && current_if_false);

      // The current_if_true block becomes the corresponding switch case block.
      uint32_t value = const_op.word32();
      cases.emplace_back(value, current_if_true->MapToNextGraph(),
                         current_branch->hint);

      // All pure ops from the if_false block should be executed before
      // the switch, except the last Branch operation (which we drop).
      false_blocks.push_back(current_if_false);

      // If we encounter a if_false block that doesn't end with a Branch,
      // this means we've reached the end of the cascade.
      const Operation& maybe_branch =
          current_if_false->LastOperation(Asm().input_graph());
      if (!maybe_branch.Is<BranchOp>()) {
        TRACE("\t [break] Reached end of the if-else cascade.\n");
        break;
      }

      default_hint = current_branch->hint;

      // Iterate to the next if_false block in the cascade.
      current_branch = &maybe_branch.template Cast<BranchOp>();

      // As long as the else blocks contain only pure ops, we can keep
      // traversing the if-else cascade.
      if (!ContainsOnlyPureOps(current_branch->if_false, Asm().input_graph())) {
        TRACE("\t [break] End of only-pure-ops cascade reached.\n");
        break;
      }
    }

    // Probably better to keep short if-else cascades as they are.
    if (cases.size() <= 2) {
      TRACE("\t [bailout] Cascade with less than 2 levels of nesting.\n");
      goto no_change;
    }
    CHECK_EQ(cases.size(), false_blocks.size());

    // We're skipping the last false block, as it becomes the default block.
    for (size_t i = 0; i < false_blocks.size() - 1; ++i) {
      const Block* block = false_blocks[i];
      InlineAllOperationsWithoutLast(block);
    }

    TRACE("[reduce] Successfully emit a Switch with %z cases.", cases.size());

    // The last current_if_true block that ends the cascade becomes the default
    // case.
    Block* default_block = current_if_false;
    Asm().Switch(
        Asm().MapToNewGraph(switch_var),
        Asm().output_graph().graph_zone()->CloneVector(base::VectorOf(cases)),
        default_block->MapToNextGraph(), default_hint);
    return OpIndex::Invalid();
  }

 private:
  static bool ContainsOnlyPureOps(const Block* block, const Graph& graph) {
    for (const auto& op : base::IterateWithoutLast(graph.operations(*block))) {
      // We are moving the block content to before the switch, effectively
      // moving it before the previously existing branches.
      if (!op.Effects().hoistable_before_a_branch()) {
        return false;
      }
    }
    return true;
  }

  // Visits and emits {input_block} right now (ie, in the current block)
  // until the one before the last operation is reached.
  void InlineAllOperationsWithoutLast(const Block* input_block) {
    base::iterator_range<Graph::OpIndexIterator> all_ops =
        Asm().input_graph().OperationIndices(*input_block);

    for (OpIndex op : base::IterateWithoutLast(all_ops)) {
      Asm().InlineOp(op, input_block);
    }
  }
};

}  // namespace v8::internal::compiler::turboshaft

#undef TRACE

#endif  // V8_COMPILER_TURBOSHAFT_STRUCTURAL_OPTIMIZATION_REDUCER_H_

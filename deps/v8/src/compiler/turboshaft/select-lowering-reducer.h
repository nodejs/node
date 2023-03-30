// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_SELECT_LOWERING_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_SELECT_LOWERING_REDUCER_H_

#include "src/base/vector.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operations.h"

namespace v8::internal::compiler::turboshaft {

// Lowers Select operations to diamonds.
//
// A Select is conceptually somewhat similar to a ternary if:
//
//       res = Select(cond, val_true, val_false)
//
// means:
//
//       res = cond ? val_true : val_false
//
// SelectLoweringReducer lowers such operations into:
//
//     if (cond) {
//         res = val_true
//     } else {
//         res = val_false
//     }

template <class Next>
class SelectLoweringReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  template <class... Args>
  explicit SelectLoweringReducer(const std::tuple<Args...>& args)
      : Next(args) {}

  OpIndex ReduceSelect(OpIndex cond, OpIndex vtrue, OpIndex vfalse,
                       RegisterRepresentation rep, BranchHint hint,
                       SelectOp::Implementation implem) {
    if (implem == SelectOp::Implementation::kCMove) {
      // We do not lower Select operations that should be implemented with
      // CMove.
      return Next::ReduceSelect(cond, vtrue, vfalse, rep, hint, implem);
    }

    Block* true_block = Asm().NewBlock();
    Block* false_block = Asm().NewBlock();
    Block* merge_block = Asm().NewBlock();

    Asm().Branch(cond, true_block, false_block, hint);

    // Note that it's possible that other reducers of the stack optimizes the
    // Branch that we just introduced into a Goto (if its condition is already
    // known). Thus, we check the return values of Bind, and only insert the
    // Gotos if Bind was successful: if not, then it means that the block
    // ({true_block} or {false_block}) isn't reachable because the Branch was
    // optimized to a Goto.

    bool has_true_block = false;
    bool has_false_block = false;

    if (Asm().Bind(true_block)) {
      has_true_block = true;
      Asm().Goto(merge_block);
    }

    if (Asm().Bind(false_block)) {
      has_false_block = true;
      Asm().Goto(merge_block);
    }

    Asm().BindReachable(merge_block);

    if (has_true_block && has_false_block) {
      return Asm().Phi(base::VectorOf({vtrue, vfalse}), rep);
    } else if (has_true_block) {
      return vtrue;
    } else {
      DCHECK(has_false_block);
      return vfalse;
    }
  }
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_SELECT_LOWERING_REDUCER_H_

// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LOOP_UNROLLING_H_
#define V8_COMPILER_LOOP_UNROLLING_H_

// Loop unrolling is an optimization that copies the body of a loop and creates
// a fresh loop, whose iteration corresponds to 2 or more iterations of the
// initial loop. For a high-level description of the algorithm see
// https://bit.ly/3G0VdWW.

#include "src/compiler/common-operator.h"
#include "src/compiler/loop-analysis.h"

namespace v8 {
namespace internal {
namespace compiler {

static constexpr uint32_t kMaximumUnnestedSize = 50;
static constexpr uint32_t kMaximumUnrollingCount = 5;

// A simple heuristic to decide how many times to unroll a loop. Favors small
// and deeply nested loops.
// TODO(manoskouk): Investigate how this can be improved.
V8_INLINE uint32_t unrolling_count_heuristic(uint32_t size, uint32_t depth) {
  return std::min((depth + 1) * kMaximumUnnestedSize / size,
                  kMaximumUnrollingCount);
}

V8_INLINE uint32_t maximum_unrollable_size(uint32_t depth) {
  return (depth + 1) * kMaximumUnnestedSize;
}

void UnrollLoop(Node* loop_node, ZoneUnorderedSet<Node*>* loop, uint32_t depth,
                TFGraph* graph, CommonOperatorBuilder* common, Zone* tmp_zone,
                SourcePositionTable* source_positions,
                NodeOriginTable* node_origins);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LOOP_UNROLLING_H_

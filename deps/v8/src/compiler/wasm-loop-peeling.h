// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_WASM_LOOP_PEELING_H_
#define V8_COMPILER_WASM_LOOP_PEELING_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/node-origin-table.h"

namespace v8 {
namespace internal {
namespace compiler {

// Loop peeling is an optimization that copies the body of a loop, creating
// a new copy of the body called the "peeled iteration" that represents the
// first iteration. It enables a kind of loop hoisting: repeated computations
// without side-effects in the body of the loop can be computed in the first
// iteration only and reused in the next iterations.
void PeelWasmLoop(Node* loop_node, ZoneUnorderedSet<Node*>* loop, Graph* graph,
                  CommonOperatorBuilder* common, Zone* tmp_zone,
                  SourcePositionTable* source_positions,
                  NodeOriginTable* node_origins);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_LOOP_PEELING_H_

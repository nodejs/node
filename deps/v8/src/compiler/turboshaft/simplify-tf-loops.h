// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_SIMPLIFY_TF_LOOPS_H_
#define V8_COMPILER_TURBOSHAFT_SIMPLIFY_TF_LOOPS_H_

#include "src/compiler/graph-reducer.h"

namespace v8::internal::compiler {

class MachineGraph;

// Constrain loop nodes to have at most two inputs, by introducing additional
// merges as needed.
class SimplifyTFLoops final : public AdvancedReducer {
 public:
  SimplifyTFLoops(Editor* editor, MachineGraph* mcgraph)
      : AdvancedReducer(editor), mcgraph_(mcgraph) {}

  const char* reducer_name() const override { return "SimplifyTFLoops"; }

  Reduction Reduce(Node* node) final;

 private:
  MachineGraph* const mcgraph_;
};

}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_TURBOSHAFT_SIMPLIFY_TF_LOOPS_H_

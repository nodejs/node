// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CHECKPOINT_ELIMINATION_H_
#define V8_COMPILER_CHECKPOINT_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"

namespace v8 {
namespace internal {
namespace compiler {

// Performs elimination of redundant checkpoints within the graph.
class V8_EXPORT_PRIVATE CheckpointElimination final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  explicit CheckpointElimination(Editor* editor);
  ~CheckpointElimination() final {}

  const char* reducer_name() const override { return "CheckpointElimination"; }

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceCheckpoint(Node* node);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CHECKPOINT_ELIMINATION_H_

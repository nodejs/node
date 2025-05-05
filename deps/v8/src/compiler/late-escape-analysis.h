// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LATE_ESCAPE_ANALYSIS_H_
#define V8_COMPILER_LATE_ESCAPE_ANALYSIS_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

class CommonOperatorBuilder;

// Eliminate allocated objects that have no uses besides the stores initializing
// the object.
class LateEscapeAnalysis final : public AdvancedReducer {
 public:
  LateEscapeAnalysis(Editor* editor, TFGraph* graph,
                     CommonOperatorBuilder* common, Zone* zone);

  const char* reducer_name() const override { return "LateEscapeAnalysis"; }

  Reduction Reduce(Node* node) final;
  void Finalize() override;

 private:
  bool IsEscaping(Node* node);
  void RemoveAllocation(Node* node);
  void RecordEscapingAllocation(Node* allocation);
  void RemoveWitness(Node* allocation);
  Node* dead() const { return dead_; }

  Node* dead_;
  ZoneUnorderedSet<Node*> all_allocations_;
  // Key: Allocation; Value: Number of witnesses for the allocation escaping.
  ZoneUnorderedMap<Node*, int> escaping_allocations_;
  NodeDeque revisit_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LATE_ESCAPE_ANALYSIS_H_

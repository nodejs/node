// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ESCAPE_ANALYSIS_REDUCER_H_
#define V8_COMPILER_ESCAPE_ANALYSIS_REDUCER_H_

#include "src/bit-vector.h"
#include "src/compiler/escape-analysis.h"
#include "src/compiler/graph-reducer.h"


namespace v8 {
namespace internal {

// Forward declarations.
class Counters;


namespace compiler {

// Forward declarations.
class JSGraph;


class EscapeAnalysisReducer final : public AdvancedReducer {
 public:
  EscapeAnalysisReducer(Editor* editor, JSGraph* jsgraph,
                        EscapeAnalysis* escape_analysis, Zone* zone);

  Reduction Reduce(Node* node) final;
  void SetExistsVirtualAllocate(bool exists) {
    exists_virtual_allocate_ = exists;
  }
  void VerifyReplacement() const;

 private:
  Reduction ReduceLoad(Node* node);
  Reduction ReduceStore(Node* node);
  Reduction ReduceAllocate(Node* node);
  Reduction ReduceFinishRegion(Node* node);
  Reduction ReduceReferenceEqual(Node* node);
  Reduction ReduceObjectIsSmi(Node* node);
  Reduction ReduceFrameStateUses(Node* node);
  Node* ReduceDeoptState(Node* node, Node* effect, bool multiple_users);
  Node* ReduceStateValueInput(Node* node, int node_index, Node* effect,
                              bool node_multiused, bool already_cloned,
                              bool multiple_users);

  JSGraph* jsgraph() const { return jsgraph_; }
  EscapeAnalysis* escape_analysis() const { return escape_analysis_; }
  Zone* zone() const { return zone_; }
  Counters* counters() const;

  JSGraph* const jsgraph_;
  EscapeAnalysis* escape_analysis_;
  Zone* const zone_;
  // _visited marks nodes we already processed (allocs, loads, stores)
  // and nodes that do not need a visit from ReduceDeoptState etc.
  BitVector fully_reduced_;
  bool exists_virtual_allocate_;

  DISALLOW_COPY_AND_ASSIGN(EscapeAnalysisReducer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ESCAPE_ANALYSIS_REDUCER_H_

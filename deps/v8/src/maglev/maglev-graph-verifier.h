// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_

#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

class Graph;

// TODO(victorgomes): Add more verification.
class MaglevGraphVerifier {
 public:
  explicit MaglevGraphVerifier(MaglevCompilationInfo* compilation_info) {
    if (compilation_info->has_graph_labeller()) {
      graph_labeller_ = compilation_info->graph_labeller();
    }
  }

  void PreProcessGraph(Graph* graph) { seen_.resize(graph->max_block_id()); }
  void PostProcessGraph(Graph* graph) {}
  void PostProcessBasicBlock(BasicBlock* block) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    // Check Ids are unique.
    CHECK(!seen_[block->id()]);
    seen_[block->id()] = true;
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}

  ProcessResult Process(Dead* node, const ProcessingState& state) {
    node->VerifyInputs(graph_labeller_);
    return ProcessResult::kContinue;
  }

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    for (Input& input : *node) {
      Opcode op = input.node()->opcode();
      CHECK_GE(op, kFirstOpcode);
      CHECK_LE(op, kLastOpcode);
    }
    node->VerifyInputs(graph_labeller_);
    return ProcessResult::kContinue;
  }

 private:
  MaglevGraphLabeller* graph_labeller_ = nullptr;
  std::vector<bool> seen_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_

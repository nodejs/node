// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_RANGE_VERIFICATION_H_
#define V8_MAGLEV_MAGLEV_RANGE_VERIFICATION_H_

#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-range-analysis.h"
#include "src/maglev/maglev-reducer.h"

namespace v8::internal::maglev {

class MaglevRangeVerificationProcessor {
 public:
  MaglevRangeVerificationProcessor(Graph* graph, NodeRanges* ranges);

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  void PostProcessBasicBlock(BasicBlock* block);
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block);
  void PostPhiProcessing() {}

  ProcessResult Process(ValueNode* node, const ProcessingState& state);
  ProcessResult Process(NodeBase* node, const ProcessingState& state) {
    return ProcessResult::kContinue;
  }

 private:
  MaglevReducer<MaglevRangeVerificationProcessor> reducer_;
  NodeRanges* ranges_;
};

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_MAGLEV_RANGE_VERIFICATION_H_

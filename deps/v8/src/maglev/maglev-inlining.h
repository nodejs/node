// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_INLINING_H_
#define V8_MAGLEV_MAGLEV_INLINING_H_

#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-builder.h"
#include "src/maglev/maglev-ir.h"

namespace v8::internal::maglev {

// We assume that we have visited all the deopt infos at this point.
// That means that we don't have any uses of ReturnedValue in deopt infos.
// If the node has an use > 0, we must create a conversion to tagged.
class ReturnedValueRepresentationSelector {
 public:
  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  void PostProcessBasicBlock(BasicBlock* block) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}
  ProcessResult Process(NodeBase* node, const ProcessingState& state) {
    return ProcessResult::kContinue;
  }
  ProcessResult Process(ReturnedValue* node, const ProcessingState& state);
};

class MaglevInliner {
 public:
  explicit MaglevInliner(Graph* graph) : graph_(graph) {}

  bool Run();

 private:
  Graph* graph_;

  int max_inlined_bytecode_size_cumulative() const;
  int max_inlined_bytecode_size_small_total() const;
  int max_inlined_bytecode_size_small_with_heapnum_in_out() const;

  bool IsSmallWithHeapNumberInputsOutputs(MaglevCallSiteInfo* call_site) const;

  compiler::JSHeapBroker* broker() const { return graph_->broker(); }
  Zone* zone() const { return graph_->zone(); }

  bool is_tracing_enabled() const { return graph_->is_tracing_enabled(); }

  bool CanInlineCall();
  MaglevCallSiteInfo* ChooseNextCallSite();
  bool InlineCallSites();
  void RunOptimizer();

  enum class InliningResult {
    kDone,
    kFail,
    kAbort,
  };
  InliningResult BuildInlineFunction(MaglevCallSiteInfo* call_site,
                                     bool is_small);

  // Truncates the graph at the given basic block `block`.  All blocks
  // following `block` (exclusive) are removed from the graph and returned.
  // `block` itself is removed from the graph and not returned.
  std::vector<BasicBlock*> TruncateGraphAt(BasicBlock* block);

  void RegisterNode(MaglevGraphBuilder& builder, Node* node) {
    if (builder.has_graph_labeller()) {
      builder.graph_labeller()->RegisterNode(node);
    }
  }

  bool ShouldPrintMaglevGraph() const {
    if (graph_->compilation_info()->is_turbolev()) {
      return v8_flags.trace_turbo_graph || v8_flags.print_turbolev_frontend;
    }
    return v8_flags.print_maglev_graphs && is_tracing_enabled();
  }

  CodeTracer* GetCodeTracer() const;
  void PrintMaglevGraph(const char* msg,
                        compiler::OptionalSharedFunctionInfoRef ref = {});

  static void UpdatePredecessorsOf(BasicBlock* block, BasicBlock* prev_pred,
                                   BasicBlock* new_pred);
};

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_MAGLEV_INLINING_H_

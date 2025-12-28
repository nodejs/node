// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/turbolev-frontend-pipeline.h"

#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-optimizer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph-verifier.h"
#include "src/maglev/maglev-inlining.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-kna-processor.h"
#include "src/maglev/maglev-phi-representation-selector.h"
#include "src/maglev/maglev-post-hoc-optimizations-processors.h"
#include "src/maglev/maglev-range-analysis.h"
#include "src/maglev/maglev-range-verification.h"
#include "src/maglev/maglev-truncation.h"

namespace v8::internal::compiler::turboshaft {

void PrintMaglevGraph(PipelineData& data, maglev::Graph* maglev_graph,
                      const char* msg) {
  CodeTracer* code_tracer = data.GetCodeTracer();
  CodeTracer::StreamScope tracing_scope(code_tracer);
  tracing_scope.stream() << "\n----- " << msg << " -----" << std::endl;

  maglev::PrintGraph(tracing_scope.stream(), maglev_graph);
}

void RunMaglevOptimizer(PipelineData& data, maglev::Graph* graph,
                        maglev::NodeRanges* ranges) {
  maglev::RecomputeKnownNodeAspectsProcessor kna_processor(graph);
  maglev::MaglevGraphOptimizer optimizer(graph, kna_processor, ranges);
  maglev::GraphMultiProcessor<maglev::MaglevGraphOptimizer&,
                              maglev::RecomputeKnownNodeAspectsProcessor&>
      optimization_pass(optimizer, kna_processor);
  optimization_pass.ProcessGraph(graph);

  // Remove unreachable blocks if we have any.
  if (graph->may_have_unreachable_blocks()) {
    graph->RemoveUnreachableBlocks();
  }

  if (V8_UNLIKELY(ShouldPrintMaglevGraph(data))) {
    PrintMaglevGraph(data, graph, "After optimization");
  }
}

// TODO(dmercadier, nicohartmann): consider doing some of these optimizations on
// the Turboshaft graph after the Maglev->Turboshaft translation. For instance,
// MaglevPhiRepresentationSelector is the Maglev equivalent of Turbofan's
// SimplifiedLowering, but is much less powerful (doesn't take truncations into
// account, doesn't do proper range analysis, doesn't run a fixpoint
// analysis...).
bool RunMaglevOptimizations(PipelineData& data,
                            maglev::MaglevCompilationInfo* compilation_info,
                            maglev::Graph* maglev_graph) {
  // Non-eager inlining.
  if (v8_flags.turbolev_non_eager_inlining) {
    maglev::MaglevInliner inliner(maglev_graph);
    if (!inliner.Run()) return false;
  }

  // Truncation pass.
  if (v8_flags.maglev_truncation && maglev_graph->may_have_truncation()) {
    maglev::GraphBackwardProcessor<maglev::PropagateTruncationProcessor>
        propagate;
    propagate.ProcessGraph(maglev_graph);
    // TODO(victorgomes): Support identities to flow to next passes?
    maglev::GraphProcessor<maglev::TruncationProcessor> truncate(
        maglev::TruncationProcessor{maglev_graph});
    truncate.ProcessGraph(maglev_graph);
  }

  if (V8_UNLIKELY(ShouldPrintMaglevGraph(data))) {
    PrintMaglevGraph(data, maglev_graph, "After truncation");
  }

  // Phi untagging.
  {
    maglev::GraphProcessor<maglev::MaglevPhiRepresentationSelector> processor(
        maglev_graph);
    processor.ProcessGraph(maglev_graph);
  }

  if (V8_UNLIKELY(ShouldPrintMaglevGraph(data))) {
    PrintMaglevGraph(data, maglev_graph, "After phi untagging");
  }

  if (v8_flags.maglev_range_analysis) {
    maglev::NodeRanges ranges(maglev_graph);
    ranges.ProcessGraph();
    if (V8_UNLIKELY(v8_flags.trace_maglev_range_analysis)) {
      ranges.Print();
    }

    if (V8_UNLIKELY(v8_flags.maglev_range_verification)) {
      maglev::GraphProcessor<maglev::MaglevRangeVerificationProcessor> verifier(
          maglev_graph, &ranges);
      verifier.ProcessGraph(maglev_graph);
    }

    RunMaglevOptimizer(data, maglev_graph, &ranges);
  }

  // Escape analysis.
  {
    maglev::GraphMultiProcessor<maglev::ReturnedValueRepresentationSelector,
                                maglev::AnyUseMarkingProcessor>
        processor;
    processor.ProcessGraph(maglev_graph);
  }

#ifdef DEBUG
  maglev::GraphProcessor<maglev::MaglevGraphVerifier> verifier(
      compilation_info);
  verifier.ProcessGraph(maglev_graph);
#endif

  // Dead nodes elimination (which, amongst other things, cleans up the left
  // overs of escape analysis).
  {
    maglev::GraphMultiProcessor<maglev::DeadNodeSweepingProcessor> processor;
    processor.ProcessGraph(maglev_graph);
  }

  if (V8_UNLIKELY(ShouldPrintMaglevGraph(data))) {
    PrintMaglevGraph(data, maglev_graph,
                     "After escape analysis and dead node sweeping");
  }

  return true;
}

}  // namespace v8::internal::compiler::turboshaft

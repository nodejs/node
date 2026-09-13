// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/turbolev-frontend-pipeline.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "src/base/logging.h"
#include "src/common/synchronization-point-support.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/pipeline-statistics.h"
#include "src/flags/flags.h"
#include "src/heap/read-only-heap.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-builder.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-optimizer.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph-serializer.h"
#include "src/maglev/maglev-graph-verifier.h"
#include "src/maglev/maglev-inlining.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-kna-processor.h"
#include "src/maglev/maglev-loop-peeler.h"
#include "src/maglev/maglev-phase.h"
#include "src/maglev/maglev-phi-representation-selector.h"
#include "src/maglev/maglev-post-hoc-optimizations-processors.h"
#include "src/maglev/maglev-range-analysis.h"
#include "src/maglev/maglev-range-verification.h"
#include "src/maglev/maglev-truncation.h"
#include "src/maglev/turbolev-escape-analysis.h"

namespace v8::internal::compiler::turboshaft {

enum class PhaseResult { kContinue, kAbort };

inline PhaseResult PhaseResultFromBool(bool success) {
  return success ? PhaseResult::kContinue : PhaseResult::kAbort;
}

// TODO(victorgomes): Should we create a Turbolev phase kind?
#define DECL_TURBOLEV_PHASE_CONSTANTS_IMPL(Name, CallStatsName)             \
  DECL_PIPELINE_PHASE_CONSTANTS_HELPER(CallStatsName, PhaseKind::kTurbolev, \
                                       RuntimeCallStats::kThreadSpecific,   \
                                       "Turbolev" #Name)                    \
                                                                            \
  static constexpr char kPhaseName[] = "V8.TF" #CallStatsName;              \
  static constexpr maglev::MaglevPhase phase = maglev::MaglevPhase::k##Name;

#define DECL_TURBOLEV_PHASE_CONSTANTS(Name) \
  DECL_TURBOLEV_PHASE_CONSTANTS_IMPL(Name, Turbolev##Name)

namespace {
std::string NormalizePhaseName(std::string str) {
  str.erase(std::remove_if(str.begin(), str.end(), [](char c) {
    return c == '-' || c == ' ';
  }), str.end());
  std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
    return std::tolower(c);
  });
  return str;
}
}  // namespace

bool TurbolevFrontendPipeline::TurbolevPhaseMatchesFilter(
    maglev::MaglevPhase phase) {
  if (v8_flags.turbolev_phase_filter.value() == nullptr) return true;
  if (strcmp(v8_flags.turbolev_phase_filter.value(), "*") == 0) return true;

  std::string phase_name = NormalizePhaseName(maglev::PhaseName(phase));
  std::stringstream ss(v8_flags.turbolev_phase_filter.value());
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (phase_name.find(NormalizePhaseName(token)) != std::string::npos) {
      return true;
    }
  }
  return false;
}


TurbolevFrontendPipeline::TurbolevFrontendPipeline(PipelineData* data,
                                                   Linkage* linkage)
    : data_(*data),
      compilation_info_(maglev::MaglevCompilationInfo::NewForTurbolev(
          data->isolate(), data->broker(), data->info()->closure(),
          data->info()->osr_offset(),
          data->info()->function_context_specializing(),
          data->info()->debug_name())) {
  // We need to be certain that the parameter count reported by our output
  // Code object matches what the code we compile expects. Otherwise, this
  // may lead to effectively signature mismatches during function calls. This
  // CHECK is a defense-in-depth measure to ensure this doesn't happen.
  SBXCHECK_EQ(compilation_info_->toplevel_compilation_unit()->parameter_count(),
              linkage->GetIncomingDescriptor()->ParameterSlotCount());

  // We always create a MaglevGraphLabeller in order to record source positions.
  // TODO(victorgomes): Investigate support for Turbolev without
  // MaglevGraphLabeller
  compilation_info_->set_graph_labeller(new maglev::MaglevGraphLabeller());
  compilation_info_->set_optimization_id(data->info()->optimization_id());
}

void TurbolevFrontendPipeline::PrintMaglevGraph(maglev::MaglevPhase phase) {
  CodeTracer* code_tracer = data_.GetCodeTracer();
  CodeTracer::StreamScope tracing_scope(code_tracer);
  tracing_scope.stream() << "\n----- " << maglev::PhaseName(phase) << " -----"
                         << std::endl;

  maglev::PrintGraph(tracing_scope.stream(), graph_, phase);
}

namespace {
#define C_RESET (v8_flags.log_colour ? "\033[0m" : "")
#define C_GRAY (v8_flags.log_colour ? "\033[90m" : "")
#define C_RED (v8_flags.log_colour ? "\033[91m" : "")

void PrintInliningTree(std::ostream& os, compiler::JSHeapBroker* broker,
                       maglev::InliningTreeDebugInfo* node,
                       const std::string& prefix, bool is_last) {
  DCHECK_NOT_NULL(node->details);
  maglev::MaglevCallerDetails* details = node->details;
  DCHECK_IMPLIES(details->is_eager_inline, details->is_small_function);
  int max_budget = details->is_small_function
                       ? v8_flags.max_inlined_bytecode_size_small_total
                       : v8_flags.max_inlined_bytecode_size_cumulative;
  double freq = details->call_frequency;
  int length = node->shared.GetBytecodeArray(broker).length();
  std::string loop_details = "";
  if (details->peeled_iteration_count > 0) {
    if (v8_flags.maglev_optimistic_peeled_loops &&
        node->details->peeled_iteration_count == 1) {
      loop_details = " (speel)";
    } else {
      loop_details = " (peel)";
    }
  }
  os << C_GRAY << prefix << (is_last ? "└" : "├")
     << (node->children.empty() ? "──" : "┬─") << C_RESET << " " << node->order
     << C_GRAY << " (" << node->budget << "/" << max_budget << ")" << C_RESET
     << " " << (details->is_eager_inline ? "⚡ " : "   ")
     << Brief(*node->shared.object()) << C_RED
     << (details->loop_depth > 0 ? " ↺" + std::to_string(details->loop_depth)
                                 : "  ")
     << loop_details << C_RESET;
  os << C_GRAY << " (f:";
  if (freq >= 1000.0) {
    os << std::fixed << std::setprecision(1) << (freq / 1000.0) << "k";
  } else {
    os << std::defaultfloat << freq;
  }
  os << ", l:" << length << ")\n" << C_RESET;
  std::string new_prefix = prefix + (is_last ? " " : "│");
  for (size_t i = 0; i < node->children.size(); ++i) {
    PrintInliningTree(os, broker, node->children[i], new_prefix,
                      i == node->children.size() - 1);
  }
}

void PrintInliningTree(std::ostream& os, maglev::Graph* const graph) {
  maglev::InliningTreeDebugInfo* root = graph->inlining_tree_debug_info();
  if (root == nullptr) return;
  os << "🧩 Functions inlined into " << Brief(*root->shared.object());
  if (graph->is_osr()) {
    os << C_RED << " (OSR)" << C_RESET;
  }
  os << "\n";
  DCHECK_NULL(root->details);
  for (size_t i = 0; i < root->children.size(); ++i) {
    PrintInliningTree(os, graph->broker(), root->children[i], "",
                      i == root->children.size() - 1);
  }
}
}  // namespace

void TurbolevFrontendPipeline::PrintInliningTreeDebugInfo() {
  if (!data_.info()->shared_info()->PassesFilter(v8_flags.trace_turbo_filter)) {
    return;
  }
  CodeTracer* code_tracer = data_.GetCodeTracer();
  CodeTracer::StreamScope tracing_scope(code_tracer);
  PrintInliningTree(tracing_scope.stream(), graph_);
}

void TurbolevFrontendPipeline::PrintBytecode() {
  DCHECK(data_.info()->trace_turbo_graph() || v8_flags.print_turbolev_frontend);
  maglev::MaglevCompilationUnit* top_level_unit =
      compilation_info_->toplevel_compilation_unit();
  CodeTracer* code_tracer = data_.GetCodeTracer();
  CodeTracer::StreamScope tracing_scope(code_tracer);
  tracing_scope.stream()
      << "\n----- Bytecode before MaglevGraphBuilding -----\n"
      << std::endl;
  tracing_scope.stream() << "Function: "
                         << Brief(*compilation_info_->toplevel_function())
                         << std::endl;
  BytecodeArray::Disassemble(top_level_unit->bytecode().object(),
                             tracing_scope.stream());
  Print(*top_level_unit->feedback().object(), tracing_scope.stream());
}

struct MaglevGraphBuilderPhase {
  DECL_TURBOLEV_PHASE_CONSTANTS(MaglevGraphBuilding)

  PhaseResult Run(maglev::Graph* graph) {
    // TODO(victorgomes): These could be initialized inside the graph builder
    // constructor.
    JSHeapBroker* broker = graph->broker();
    LocalIsolate* local_isolate = broker->local_isolate_or_isolate();
    maglev::MaglevCompilationInfo* compilation_info = graph->compilation_info();
    maglev::MaglevGraphBuilder graph_builder(
        local_isolate, compilation_info->toplevel_compilation_unit(), graph);
    return PhaseResultFromBool(graph_builder.Build());
  }
};

struct InlinerPhase {
  DECL_TURBOLEV_PHASE_CONSTANTS(Inlining)

  PhaseResult Run(maglev::Graph* graph) {
    maglev::MaglevInliner inliner(graph);
    return PhaseResultFromBool(inliner.Run());
  }
};

struct LoopPeelerPhase {
  DECL_TURBOLEV_PHASE_CONSTANTS(LoopPeeling)

  PhaseResult Run(maglev::Graph* graph, bool* peeled) {
    maglev::MaglevLoopPeeler peeler(graph);
    *peeled = peeler.Run();
    return PhaseResult::kContinue;
  }
};

struct TruncationPhase {
  DECL_TURBOLEV_PHASE_CONSTANTS(Truncation)

  PhaseResult Run(maglev::Graph* graph) {
    maglev::GraphBackwardProcessor<maglev::PropagateTruncationProcessor>
        propagate;
    propagate.ProcessGraph(graph);
    // TODO(victorgomes): Support identities to flow to next passes?
    maglev::GraphProcessor<maglev::TruncationProcessor> truncate(graph);
    truncate.ProcessGraph(graph);
    return PhaseResult::kContinue;
  }
};

struct PhiUntaggingPhase {
  DECL_TURBOLEV_PHASE_CONSTANTS(PhiUntagging)

  PhaseResult Run(maglev::Graph* graph) {
    maglev::GraphProcessor<maglev::MaglevPhiRepresentationSelector> processor(
        graph);
    processor.ProcessGraph(graph);
    return PhaseResult::kContinue;
  }
};

struct EscapeAnalysisPhase {
  DECL_TURBOLEV_PHASE_CONSTANTS(EscapeAnalysis)

  PhaseResult Run(maglev::Graph* graph) {
    maglev::MaglevCompilationInfo* compilation_info = graph->compilation_info();
    // TODO(dmercadier): use a proper temporary zone.
    Zone* temp_zone = graph->zone();
    maglev::EscapeAnalysis::Run(graph, compilation_info, temp_zone);
    return PhaseResult::kContinue;
  }
};

struct RangeAnalysisPhase {
  DECL_TURBOLEV_PHASE_CONSTANTS(RangeAnalysis)

  PhaseResult Run(maglev::Graph* graph, maglev::NodeRanges& ranges) {
    ranges.ProcessGraph();
    if (V8_UNLIKELY(v8_flags.trace_maglev_range_analysis)) {
      ranges.Print();
    }

    if (V8_UNLIKELY(v8_flags.maglev_range_verification)) {
      maglev::GraphProcessor<maglev::MaglevRangeVerificationProcessor> verifier(
          graph, &ranges);
      verifier.ProcessGraph(graph);
    }
    return PhaseResult::kContinue;
  }
};

struct PostOptimizerPhase {
  DECL_TURBOLEV_PHASE_CONSTANTS(PostOptimizer)

  PhaseResult Run(maglev::Graph* graph, maglev::NodeRanges* ranges) {
    maglev::ReachableExceptionHandlerTracker exception_handler_tracker(graph);
    maglev::RecomputeKnownNodeAspectsProcessor kna_processor(
        graph, exception_handler_tracker);
    maglev::MaglevGraphOptimizer optimizer(graph, kna_processor, ranges);
    maglev::CommonSubexpressionEliminationProcessor<
        maglev::RecomputeKnownNodeAspectsProcessor>
        cse(kna_processor);
    maglev::GraphMultiProcessor<
        maglev::MaglevGraphOptimizer&,
        maglev::CommonSubexpressionEliminationProcessor<
            maglev::RecomputeKnownNodeAspectsProcessor>&,
        maglev::ReachableExceptionHandlerTracker&,
        maglev::RecomputeKnownNodeAspectsProcessor&>
        optimization_pass(optimizer, cse, exception_handler_tracker,
                          kna_processor);
    optimization_pass.ProcessGraph(graph);

    // Remove unreachable blocks if we have any.
    if (graph->may_have_unreachable_blocks()) {
      graph->RemoveUnreachableBlocks();
    }
    return PhaseResult::kContinue;
  }
};

struct PrePhiUntaggingPhase {
  DECL_TURBOLEV_PHASE_CONSTANTS(PrePhiUntagging)

  PhaseResult Run(maglev::Graph* graph) {
    maglev::ReachableExceptionHandlerTracker exception_handler_tracker(graph);
    maglev::RecomputeKnownNodeAspectsProcessor kna_processor(
        graph, exception_handler_tracker);
    maglev::MaglevGraphOptimizer optimizer(graph, kna_processor,
                                           /*ranges=*/nullptr);
    maglev::GraphMultiProcessor<maglev::MaglevGraphOptimizer&,
                                maglev::ReachableExceptionHandlerTracker&,
                                maglev::RecomputeKnownNodeAspectsProcessor&,
                                maglev::RecomputePhiUseHintsProcessor,
                                maglev::BoundsCheckEliminationProcessor>
        optimization_pass(optimizer, exception_handler_tracker, kna_processor,
                          maglev::RecomputePhiUseHintsProcessor{graph->zone()},
                          maglev::BoundsCheckEliminationProcessor{graph});
    optimization_pass.ProcessGraph(graph);

    // Remove unreachable blocks if we have any.
    if (graph->may_have_unreachable_blocks()) {
      graph->RemoveUnreachableBlocks();
    }
    return PhaseResult::kContinue;
  }
};

struct PostHocPhase {
  DECL_TURBOLEV_PHASE_CONSTANTS(AnyUseMarking)

  PhaseResult Run(maglev::Graph* graph) {
    if (!v8_flags.turbolev_escape_analysis) {
      // Unwrap deopt frames before escape analysis.
      graph->UnwrapDeoptFrames();
    }
    // Escape analysis.
    maglev::GraphMultiProcessor<maglev::ReturnedValueRepresentationSelector,
                                maglev::AnyUseMarkingProcessor>
        processor(
            maglev::AnyUseMarkingProcessor{!v8_flags.turbolev_escape_analysis});
    processor.ProcessGraph(graph);
    return PhaseResult::kContinue;
  }
};

struct DeadNodeSweepingPhase {
  DECL_TURBOLEV_PHASE_CONSTANTS(DeadNodeSweeping)

  PhaseResult Run(maglev::Graph* graph) {
    // Dead nodes elimination (which, amongst other things, cleans up the left
    // overs of escape analysis).
    maglev::GraphProcessor<maglev::DeadNodeSweepingProcessor> processor;
    processor.ProcessGraph(graph);
    return PhaseResult::kContinue;
  }
};

template <typename Phase, typename... Args>
auto TurbolevFrontendPipeline::Run(Args&&... args) {
  PhaseScope phase_scope(data_.pipeline_statistics(), Phase::phase_name());
  NodeOriginTable::PhaseScope origin_scope(data_.node_origins(),
                                           Phase::phase_name());
#ifdef V8_RUNTIME_CALL_STATS
  RuntimeCallTimerScope runtime_call_timer_scope(data_.runtime_call_stats(),
                                                 Phase::kRuntimeCallCounterId,
                                                 Phase::kCounterMode);
#endif
  Phase phase;
  SYNCHRONIZATION_POINT(Phase::synchronization_point_name());
  PhaseResult result = phase.Run(graph_, std::forward<Args>(args)...);
  if (V8_UNLIKELY(ShouldPrintMaglevGraph(Phase::phase))) {
    PrintMaglevGraph(Phase::phase);
  }
  if (compilation_info_->trace_json_enabled()) {
    maglev::PrintMaglevGraphAsJSON(compilation_info_.get(), graph_,
                                   Phase::phase);
  }
#ifdef DEBUG
  if (result == PhaseResult::kContinue) {
    maglev::GraphProcessor<maglev::MaglevGraphVerifier> verifier(
        compilation_info_.get(), Phase::phase);
    verifier.ProcessGraph(graph_);
  }
#endif
  return result;
}

#define RUN_MAYBE_ABORT(phase, ...) \
  if (V8_UNLIKELY(Run<phase>(__VA_ARGS__) == PhaseResult::kAbort)) return {};

std::optional<maglev::Graph*> TurbolevFrontendPipeline::Run() {
  if (V8_UNLIKELY(ShouldPrintMaglevGraph())) {
    PrintBytecode();
  }
  graph_ = maglev::Graph::New(compilation_info_.get());
  RUN_MAYBE_ABORT(MaglevGraphBuilderPhase);
  if (v8_flags.turbolev_non_eager_inlining) {
    RUN_MAYBE_ABORT(InlinerPhase);
  }
  if (v8_flags.turbolev_non_eager_loop_peeling) {
    bool peeled = false;
    RUN_MAYBE_ABORT(LoopPeelerPhase, &peeled);
    if (peeled) {
      RUN_MAYBE_ABORT(PostOptimizerPhase, nullptr);
    }
  }
  if (v8_flags.maglev_truncation && graph_->may_have_truncation()) {
    RUN_MAYBE_ABORT(TruncationPhase);
  }
  if (v8_flags.turbolev_untagged_phis) {
    RUN_MAYBE_ABORT(PrePhiUntaggingPhase);
    RUN_MAYBE_ABORT(PhiUntaggingPhase);
  }
  if (v8_flags.maglev_range_analysis) {
    maglev::NodeRanges ranges(graph_);
    RUN_MAYBE_ABORT(RangeAnalysisPhase, ranges);
    RUN_MAYBE_ABORT(PostOptimizerPhase, &ranges);
  }
  if (v8_flags.turbolev_escape_analysis) {
    // TODO(dmercadier): it would make sense to run this before Phi untagging so
    // that Phi untagging can untag the Phis created by Escape Analysis.
    RUN_MAYBE_ABORT(EscapeAnalysisPhase);
    // TODO(dmercadier): can we run this PostOptimizerPhase as part of the
    // Elider phase of escape analysis?
    RUN_MAYBE_ABORT(PostOptimizerPhase, nullptr);
  }
  RUN_MAYBE_ABORT(PostHocPhase);
  RUN_MAYBE_ABORT(DeadNodeSweepingPhase);
  if (V8_UNLIKELY(v8_flags.print_turbolev_inline_functions)) {
    PrintInliningTreeDebugInfo();
  }
  return graph_;
}

#undef RUN_MAYBE_ABORT

}  // namespace v8::internal::compiler::turboshaft

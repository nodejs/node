// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-compiler.h"

#include <optional>
#include <ostream>

#include "src/base/logging.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/common/globals.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/execution/frames.h"
#include "src/flags/flags.h"
#include "src/maglev/maglev-code-generator.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-builder.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph-verifier.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-inlining.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-phi-representation-selector.h"
#include "src/maglev/maglev-post-hoc-optimizations-processors.h"
#include "src/maglev/maglev-pre-regalloc-codegen-processors.h"
#include "src/maglev/maglev-regalloc.h"
#include "src/maglev/maglev-truncation.h"
#include "src/objects/code-inl.h"
#include "src/objects/js-function.h"

#ifdef ALWAYS_MAGLEV_GRAPH_LABELLER
#define ALWAYS_MAGLEV_GRAPH_LABELLER_BOOL true
#else
#define ALWAYS_MAGLEV_GRAPH_LABELLER_BOOL false
#endif

namespace v8 {
namespace internal {
namespace maglev {

namespace {
void PrintGraph(Graph* graph, bool condition, const char* message,
                bool has_regalloc_data = false) {
  if (V8_UNLIKELY(condition &&
                  graph->compilation_info()->is_tracing_enabled())) {
    UnparkedScopeIfOnBackground unparked_scope(
        graph->broker()->local_isolate()->heap());
    std::cout << "\n----- " << message << " -----" << std::endl;
    PrintGraph(std::cout, graph, has_regalloc_data);
  }
}

void VerifyGraph(Graph* graph) {
#ifdef DEBUG
  GraphProcessor<MaglevGraphVerifier> verifier(graph->compilation_info());
  verifier.ProcessGraph(graph);
#endif  // DEBUG
}
}  // namespace

// static
bool MaglevCompiler::Compile(LocalIsolate* local_isolate,
                             MaglevCompilationInfo* compilation_info) {
  std::optional<MaglevGraphLabellerScope> graph_labeller_scope;
  compiler::CurrentHeapBrokerScope current_broker(compilation_info->broker());
  Graph* graph = Graph::New(compilation_info);

  if (V8_UNLIKELY(ALWAYS_MAGLEV_GRAPH_LABELLER_BOOL ||
                  compilation_info->is_tracing_enabled() ||
                  compilation_info->collect_source_positions())) {
    compilation_info->set_graph_labeller(new MaglevGraphLabeller());
    graph_labeller_scope.emplace(compilation_info->graph_labeller());
  }

  {
    UnparkedScopeIfOnBackground unparked_scope(local_isolate->heap());

    if (V8_UNLIKELY(v8_flags.maglev_print_bytecode &&
                    compilation_info->is_tracing_enabled())) {
      MaglevCompilationUnit* top_level_unit =
          compilation_info->toplevel_compilation_unit();
      std::cout << "Compiling " << Brief(*compilation_info->toplevel_function())
                << " with Maglev";
      std::cout << "\n----- Bytecode array -----" << std::endl;
      BytecodeArray::Disassemble(top_level_unit->bytecode().object(),
                                 std::cout);
      if (v8_flags.maglev_print_feedback) {
        Print(*top_level_unit->feedback().object(), std::cout);
      }
    }

    {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.Maglev.GraphBuilding");
      MaglevGraphBuilder graph_builder(
          local_isolate, compilation_info->toplevel_compilation_unit(), graph);
      if (!graph_builder.Build()) return false;
      PrintGraph(graph, v8_flags.print_maglev_graphs, "After graph building");
      VerifyGraph(graph);
    }

    if (v8_flags.maglev_non_eager_inlining) {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.Maglev.Inlining");
      MaglevInliner inliner(graph);
      if (!inliner.Run()) return false;
      VerifyGraph(graph);
    }

    if (v8_flags.maglev_truncation && graph->may_have_truncation()) {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.Maglev.Truncation");
      GraphBackwardProcessor<PropagateTruncationProcessor> propagate;
      propagate.ProcessGraph(graph);
      PrintGraph(graph, v8_flags.print_maglev_graphs,
                 "After propagating truncation");
      GraphProcessor<TruncationProcessor> truncate(graph);
      truncate.ProcessGraph(graph);
      PrintGraph(graph, v8_flags.print_maglev_graphs, "After truncation");
      VerifyGraph(graph);
    }

    if (v8_flags.maglev_licm) {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.Maglev.LoopOptimizations");
      GraphProcessor<LoopOptimizationProcessor> loop_optimizations(
          compilation_info);
      loop_optimizations.ProcessGraph(graph);
      PrintGraph(graph, v8_flags.print_maglev_graphs,
                 "After loop optimizations");
      VerifyGraph(graph);
    }

    if (v8_flags.maglev_untagged_phis) {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.Maglev.PhiUntagging");
      GraphProcessor<MaglevPhiRepresentationSelector> representation_selector(
          graph);
      representation_selector.ProcessGraph(graph);
      PrintGraph(graph, v8_flags.print_maglev_graphs, "After Phi untagging");
      VerifyGraph(graph);
    }
  }

  {
    // Post-hoc optimisation:
    //   - Remove unreachable blocks
    //   - Dead node marking
    //   - Cleaning up identity nodes
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.Maglev.DeadCodeMarking");
    if (graph->may_have_unreachable_blocks()) {
      graph->RemoveUnreachableBlocks();
    }
    GraphMultiProcessor<ReturnedValueRepresentationSelector,
                        AnyUseMarkingProcessor,
                        RegallocNodeInfoAllocationProcessor>
        processor;
    processor.ProcessGraph(graph);
    PrintGraph(graph, v8_flags.print_maglev_graphs, "After use marking",
               /* has_regalloc_data */ true);
    VerifyGraph(graph);
  }

  {
    RegallocBlockInfo regalloc_info;
    {
      // Preprocessing for register allocation and code gen:
      //   - Remove dead nodes
      //   - Collect input/output location constraints
      //   - Find the maximum number of stack arguments passed to calls
      //   - Collect use information, for SSA liveness and next-use distance.
      //   - Mark
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.Maglev.NodeProcessing");
      UnparkedScopeIfOnBackground unparked_scope(local_isolate->heap());
      GraphMultiProcessor<DeadNodeSweepingProcessor,
                          ValueLocationConstraintProcessor,
                          MaxCallDepthProcessor, LiveRangeAndNextUseProcessor,
                          DecompressedUseMarkingProcessor>
          processor(LiveRangeAndNextUseProcessor{compilation_info, graph,
                                                 &regalloc_info});
      processor.ProcessGraph(graph);
      PrintGraph(graph, v8_flags.print_maglev_graphs,
                 "After register allocation pre-processing",
                 /* has_regalloc_data */ true);
    }

    {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.Maglev.RegisterAllocation");
      StraightForwardRegisterAllocator allocator(compilation_info, graph,
                                                 &regalloc_info);
      PrintGraph(graph, v8_flags.print_maglev_graph,
                 "After register allocation", /* has_regalloc_data */ true);
    }
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.Maglev.CodeAssembly");
    UnparkedScopeIfOnBackground unparked_scope(local_isolate->heap());
    std::unique_ptr<MaglevCodeGenerator> code_generator =
        std::make_unique<MaglevCodeGenerator>(local_isolate, compilation_info,
                                              graph);
    bool success = code_generator->Assemble();
    if (!success) {
      return false;
    }

    // Stash the compiled code_generator on the compilation info.
    compilation_info->set_code_generator(std::move(code_generator));
  }

  return true;
}

// static
std::pair<MaybeHandle<Code>, BailoutReason> MaglevCompiler::GenerateCode(
    Isolate* isolate, MaglevCompilationInfo* compilation_info) {
  compiler::CurrentHeapBrokerScope current_broker(compilation_info->broker());
  MaglevCodeGenerator* const code_generator =
      compilation_info->code_generator();
  DCHECK_NOT_NULL(code_generator);

  Handle<Code> code;
  {
    std::optional<MaglevGraphLabellerScope> current_thread_graph_labeller;
    if (compilation_info->has_graph_labeller()) {
      current_thread_graph_labeller.emplace(compilation_info->graph_labeller());
    }
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.Maglev.CodeGeneration");
    if (compilation_info->is_detached() ||
        !code_generator->Generate(isolate).ToHandle(&code)) {
      compilation_info->toplevel_compilation_unit()
          ->shared_function_info()
          .object()
          ->set_maglev_compilation_failed(true);
      return {{}, BailoutReason::kMaglevCodeGenerationFailed};
    }
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.Maglev.CommittingDependencies");
    if (!compilation_info->broker()->dependencies()->Commit(code)) {
      compilation_info->toplevel_function()->SetTieringInProgress(isolate,
                                                                  false);
      // Don't `set_maglev_compilation_failed` s.t. we may reattempt
      // compilation.
      // TODO(v8:7700): Make this more robust, i.e.: don't recompile endlessly.
      compilation_info->toplevel_function()->SetInterruptBudget(
          isolate, BudgetModification::kReduce);
      return {{}, BailoutReason::kBailedOutDueToDependencyChange};
    }
  }

  if (v8_flags.print_maglev_code) {
#ifdef OBJECT_PRINT
    std::unique_ptr<char[]> debug_name =
        compilation_info->toplevel_function()->shared()->DebugNameCStr();
    CodeTracer::StreamScope tracing_scope(isolate->GetCodeTracer());
    auto& os = tracing_scope.stream();
    code->CodePrint(os, debug_name.get());
#else
    Print(*code);
#endif
  }

  return {code, BailoutReason::kNoReason};
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

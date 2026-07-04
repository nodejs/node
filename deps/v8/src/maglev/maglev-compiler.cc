// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-compiler.h"

#include <fstream>
#include <optional>
#include <ostream>

#include "src/base/logging.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/common/globals.h"
#include "src/common/synchronization-point-support.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/diagnostics/code-tracer.h"
#include "src/execution/frames.h"
#include "src/flags/flags.h"
#include "src/maglev/maglev-code-generator.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-builder.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph-serializer.h"
#include "src/maglev/maglev-graph-verifier.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-inlining.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-phase.h"
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

void PrintGraph(Graph* graph, bool condition, MaglevPhase phase) {
  MaglevCompilationInfo* info = graph->compilation_info();
  if (V8_UNLIKELY(condition && info->is_tracing_enabled())) {
    compiler::UnparkedScopeIfNeeded unparked_scope(
        graph->broker()->local_isolate());
    std::cout << "\n----- " << PhaseName(phase) << " -----" << std::endl;
    PrintGraph(std::cout, graph, phase);
  }
  if (V8_UNLIKELY(info->trace_json_enabled())) {
    compiler::UnparkedScopeIfNeeded unparked_scope(
        graph->broker()->local_isolate());
    PrintMaglevGraphAsJSON(info, graph, phase);
  }
}

void VerifyGraph(Graph* graph, MaglevPhase phase) {
#ifdef DEBUG
  bool verify_sweepable_dead_nodes = phase != MaglevPhase::kAnyUseMarking;
  GraphProcessor<MaglevGraphVerifier> verifier(graph->compilation_info(),
                                               verify_sweepable_dead_nodes);
  verifier.ProcessGraph(graph);
#endif  // DEBUG
}

void PrintAndVerify(Graph* graph, bool printing_condition, MaglevPhase phase) {
  PrintGraph(graph, printing_condition, phase);
  VerifyGraph(graph, phase);
}
}  // namespace

// static
bool MaglevCompiler::Compile(LocalIsolate* local_isolate,
                             MaglevCompilationInfo* compilation_info) {
  compilation_info->set_optimization_id(local_isolate->NextOptimizationId());
  std::optional<MaglevGraphLabellerScope> graph_labeller_scope;
  compiler::CurrentHeapBrokerScope current_broker(compilation_info->broker());
  Graph* graph = Graph::New(compilation_info);

  bool enable_labeller = ALWAYS_MAGLEV_GRAPH_LABELLER_BOOL ||
                         compilation_info->is_tracing_enabled() ||
                         compilation_info->trace_json_enabled() ||
                         compilation_info->collect_source_positions() ||
                         v8_flags.code_comments;
#ifdef ENABLE_GDB_JIT_INTERFACE
  enable_labeller = enable_labeller || v8_flags.maglev_gdbjit;
#endif

  if (V8_UNLIKELY(enable_labeller)) {
    compilation_info->set_graph_labeller(new MaglevGraphLabeller());
    graph_labeller_scope.emplace(compilation_info->graph_labeller());
  }

  if (compilation_info->is_tracing_enabled() ||
      compilation_info->trace_json_enabled()) {
    CodeTracer::StreamScope tracing_scope(
        local_isolate->GetMainThreadIsolateUnsafe()->GetCodeTracer());
    tracing_scope.stream()
        << "---------------------------------------------------\n"
        << "Begin compiling method " << compilation_info->function_name()
        << " using Maglev" << std::endl;
  }

  if (compilation_info->trace_json_enabled()) {
    compiler::UnparkedScopeIfNeeded unparked_scope(local_isolate);

    Isolate* isolate = local_isolate->GetMainThreadIsolateUnsafe();
    Handle<SharedFunctionInfo> shared_info =
        compilation_info->toplevel_compilation_unit()
            ->shared_function_info()
            .object();
    DirectHandle<Script> script =
        direct_handle(Cast<Script>(shared_info->script()), local_isolate);

    // TODO(dmercadier): this is opening JSON structures, but if we bail out
    // during compilation, we might never close them. We should use a RAII
    // structure with a desctructor to make sure that the JSON structures are
    // always closed at the end. (Turbolizer can handle some invalid JSON, but
    // it would be better not to rely on that)
    MaglevJsonFile json_of(compilation_info, std::ios_base::trunc);
    json_of << "{\"function\" : ";
    JsonPrintFunctionSource(json_of, -1, compilation_info->function_name(),
                            script, isolate, shared_info);
    json_of << ",\n\"phases\":[\n";
  }

  {
    compiler::UnparkedScopeIfNeeded unparked_scope(local_isolate);

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
      TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                  "V8.Maglev.GraphBuilding");
      SYNCHRONIZATION_POINT("MaglevGraphBuilding");
      MaglevGraphBuilder graph_builder(
          local_isolate, compilation_info->toplevel_compilation_unit(), graph);
      if (!graph_builder.Build()) return false;
      PrintAndVerify(graph, v8_flags.print_maglev_graphs,
                     MaglevPhase::kMaglevGraphBuilding);
    }

    if (v8_flags.maglev_non_eager_inlining) {
      TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                  "V8.Maglev.Inlining");
      SYNCHRONIZATION_POINT("MaglevInlining");
      MaglevInliner inliner(graph);
      if (!inliner.Run()) return false;
      PrintAndVerify(graph, v8_flags.print_maglev_graphs,
                     MaglevPhase::kInlining);
    }

    if (v8_flags.maglev_truncation && graph->may_have_truncation()) {
      TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                  "V8.Maglev.Truncation");
      SYNCHRONIZATION_POINT("MaglevTruncation");
      GraphBackwardProcessor<PropagateTruncationProcessor> propagate;
      propagate.ProcessGraph(graph);
      PrintGraph(graph, v8_flags.print_maglev_graphs,
                 MaglevPhase::kTruncationPropagation);
      GraphProcessor<TruncationProcessor> truncate(graph);
      truncate.ProcessGraph(graph);
      PrintAndVerify(graph, v8_flags.print_maglev_graphs,
                     MaglevPhase::kTruncation);
    }

    if (v8_flags.maglev_licm) {
      TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                  "V8.Maglev.LoopOptimizations");
      SYNCHRONIZATION_POINT("MaglevLoopOptimizations");
      GraphProcessor<LoopOptimizationProcessor> loop_optimizations(
          compilation_info);
      loop_optimizations.ProcessGraph(graph);
      PrintAndVerify(graph, v8_flags.print_maglev_graphs,
                     MaglevPhase::kLoopOptimization);
    }

    if (v8_flags.maglev_untagged_phis) {
      TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                  "V8.Maglev.PhiUntagging");
      SYNCHRONIZATION_POINT("MaglevPhiUntagging");
      ReachableExceptionHandlerTracker tracker(graph);
      MaglevPhiRepresentationSelector representation_selector(graph);
      GraphMultiProcessor<ReachableExceptionHandlerTracker&,
                          MaglevPhiRepresentationSelector&>
          processor{tracker, representation_selector};
      processor.ProcessGraph(graph);
      if (graph->may_have_unreachable_blocks()) {
        graph->RemoveUnreachableBlocks();
      }
      PrintAndVerify(graph, v8_flags.print_maglev_graphs,
                     MaglevPhase::kPhiUntagging);
    }
  }

  {
    // Post-hoc optimisation:
    //   - Remove unreachable blocks
    //   - Dead node marking
    //   - Cleaning up identity nodes
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                "V8.Maglev.DeadCodeMarking");
    SYNCHRONIZATION_POINT("MaglevDeadCodeMarking");
    if (graph->may_have_unreachable_blocks()) {
      graph->RemoveUnreachableBlocks();
    }
    graph->UnwrapDeoptFrames();
    GraphMultiProcessor<ReturnedValueRepresentationSelector,
                        AnyUseMarkingProcessor,
                        RegallocNodeInfoAllocationProcessor>
        processor;
    processor.ProcessGraph(graph);
    PrintAndVerify(graph, v8_flags.print_maglev_graphs,
                   MaglevPhase::kAnyUseMarking);
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
      TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                  "V8.Maglev.NodeProcessing");
      SYNCHRONIZATION_POINT("MaglevNodeProcessing");
      compiler::UnparkedScopeIfNeeded unparked_scope(local_isolate);
      GraphMultiProcessor<DeadNodeSweepingProcessor,
                          ValueLocationConstraintProcessor,
                          MaxCallDepthProcessor, LiveRangeAndNextUseProcessor,
                          DecompressedUseMarkingProcessor>
          processor(LiveRangeAndNextUseProcessor{compilation_info, graph,
                                                 &regalloc_info});
      processor.ProcessGraph(graph);
      PrintAndVerify(graph, v8_flags.print_maglev_graphs,
                     MaglevPhase::kDeadNodeSweeping);
    }

    {
      TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                  "V8.Maglev.RegisterAllocation");
      SYNCHRONIZATION_POINT("MaglevRegisterAllocation");
      StraightForwardRegisterAllocator allocator(compilation_info, graph,
                                                 &regalloc_info);
      PrintGraph(graph, v8_flags.print_maglev_graph, MaglevPhase::kRegAlloc);
#ifdef ENABLE_GDB_JIT_INTERFACE
      if (v8_flags.gdbjit_full && v8_flags.maglev_gdbjit) {
        compiler::UnparkedScopeIfNeeded unparked_scope(local_isolate);
        PrintGraphToFile(graph, MaglevPhase::kRegAlloc);
      }
#endif
    }
  }

  {
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                "V8.Maglev.CodeAssembly");
    SYNCHRONIZATION_POINT("MaglevCodeAssembly");
    compiler::UnparkedScopeIfNeeded unparked_scope(local_isolate);
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
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                "V8.Maglev.CodeGeneration");
    SYNCHRONIZATION_POINT("MaglevCodeGeneration");
    if (compilation_info->is_detached() ||
        !code_generator->Generate(isolate).ToHandle(&code)) {
      compilation_info->toplevel_compilation_unit()
          ->shared_function_info()
          .object()
          ->set_maglev_compilation_failed(true);
      return {{}, BailoutReason::kMaglevCodeGenerationFailed};
    }
  }

  if (compilation_info->trace_json_enabled()) {
    FinalizeMaglevLogging(isolate, compilation_info, code_generator->graph(),
                          code);
  }
  if (compilation_info->is_tracing_enabled() ||
      compilation_info->trace_json_enabled()) {
    CodeTracer::StreamScope tracing_scope(isolate->GetCodeTracer());
    tracing_scope.stream()
        << "---------------------------------------------------\n"
        << "Finished compiling method " << compilation_info->function_name()
        << " using Maglev" << std::endl;
  }

  {
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                "V8.Maglev.CommittingDependencies");
    SYNCHRONIZATION_POINT("MaglevCommittingDependencies");
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

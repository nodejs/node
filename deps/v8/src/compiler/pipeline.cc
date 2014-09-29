// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/pipeline.h"

#include "src/base/platform/elapsed-timer.h"
#include "src/compiler/ast-graph-builder.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/graph-replay.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/instruction.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/js-context-specialization.h"
#include "src/compiler/js-generic-lowering.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/phi-reducer.h"
#include "src/compiler/register-allocator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "src/compiler/simplified-lowering.h"
#include "src/compiler/typer.h"
#include "src/compiler/verifier.h"
#include "src/hydrogen.h"
#include "src/ostreams.h"
#include "src/utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class PhaseStats {
 public:
  enum PhaseKind { CREATE_GRAPH, OPTIMIZATION, CODEGEN };

  PhaseStats(CompilationInfo* info, PhaseKind kind, const char* name)
      : info_(info),
        kind_(kind),
        name_(name),
        size_(info->zone()->allocation_size()) {
    if (FLAG_turbo_stats) {
      timer_.Start();
    }
  }

  ~PhaseStats() {
    if (FLAG_turbo_stats) {
      base::TimeDelta delta = timer_.Elapsed();
      size_t bytes = info_->zone()->allocation_size() - size_;
      HStatistics* stats = info_->isolate()->GetTStatistics();
      stats->SaveTiming(name_, delta, static_cast<int>(bytes));

      switch (kind_) {
        case CREATE_GRAPH:
          stats->IncrementCreateGraph(delta);
          break;
        case OPTIMIZATION:
          stats->IncrementOptimizeGraph(delta);
          break;
        case CODEGEN:
          stats->IncrementGenerateCode(delta);
          break;
      }
    }
  }

 private:
  CompilationInfo* info_;
  PhaseKind kind_;
  const char* name_;
  size_t size_;
  base::ElapsedTimer timer_;
};


void Pipeline::VerifyAndPrintGraph(Graph* graph, const char* phase) {
  if (FLAG_trace_turbo) {
    char buffer[256];
    Vector<char> filename(buffer, sizeof(buffer));
    SmartArrayPointer<char> functionname =
        info_->shared_info()->DebugName()->ToCString();
    if (strlen(functionname.get()) > 0) {
      SNPrintF(filename, "turbo-%s-%s.dot", functionname.get(), phase);
    } else {
      SNPrintF(filename, "turbo-%p-%s.dot", static_cast<void*>(info_), phase);
    }
    std::replace(filename.start(), filename.start() + filename.length(), ' ',
                 '_');
    FILE* file = base::OS::FOpen(filename.start(), "w+");
    OFStream of(file);
    of << AsDOT(*graph);
    fclose(file);

    OFStream os(stdout);
    os << "-- " << phase << " graph printed to file " << filename.start()
       << "\n";
  }
  if (VerifyGraphs()) Verifier::Run(graph);
}


class AstGraphBuilderWithPositions : public AstGraphBuilder {
 public:
  explicit AstGraphBuilderWithPositions(CompilationInfo* info, JSGraph* jsgraph,
                                        SourcePositionTable* source_positions)
      : AstGraphBuilder(info, jsgraph), source_positions_(source_positions) {}

  bool CreateGraph() {
    SourcePositionTable::Scope pos(source_positions_,
                                   SourcePosition::Unknown());
    return AstGraphBuilder::CreateGraph();
  }

#define DEF_VISIT(type)                                               \
  virtual void Visit##type(type* node) V8_OVERRIDE {                  \
    SourcePositionTable::Scope pos(source_positions_,                 \
                                   SourcePosition(node->position())); \
    AstGraphBuilder::Visit##type(node);                               \
  }
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

 private:
  SourcePositionTable* source_positions_;
};


static void TraceSchedule(Schedule* schedule) {
  if (!FLAG_trace_turbo) return;
  OFStream os(stdout);
  os << "-- Schedule --------------------------------------\n" << *schedule;
}


Handle<Code> Pipeline::GenerateCode() {
  if (FLAG_turbo_stats) isolate()->GetTStatistics()->Initialize(info_);

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "---------------------------------------------------\n"
       << "Begin compiling method "
       << info()->function()->debug_name()->ToCString().get()
       << " using Turbofan" << endl;
  }

  // Build the graph.
  Graph graph(zone());
  SourcePositionTable source_positions(&graph);
  source_positions.AddDecorator();
  // TODO(turbofan): there is no need to type anything during initial graph
  // construction.  This is currently only needed for the node cache, which the
  // typer could sweep over later.
  Typer typer(zone());
  CommonOperatorBuilder common(zone());
  JSGraph jsgraph(&graph, &common, &typer);
  Node* context_node;
  {
    PhaseStats graph_builder_stats(info(), PhaseStats::CREATE_GRAPH,
                                   "graph builder");
    AstGraphBuilderWithPositions graph_builder(info(), &jsgraph,
                                               &source_positions);
    graph_builder.CreateGraph();
    context_node = graph_builder.GetFunctionContext();
  }
  {
    PhaseStats phi_reducer_stats(info(), PhaseStats::CREATE_GRAPH,
                                 "phi reduction");
    PhiReducer phi_reducer;
    GraphReducer graph_reducer(&graph);
    graph_reducer.AddReducer(&phi_reducer);
    graph_reducer.ReduceGraph();
    // TODO(mstarzinger): Running reducer once ought to be enough for everyone.
    graph_reducer.ReduceGraph();
    graph_reducer.ReduceGraph();
  }

  VerifyAndPrintGraph(&graph, "Initial untyped");

  if (FLAG_context_specialization) {
    SourcePositionTable::Scope pos_(&source_positions,
                                    SourcePosition::Unknown());
    // Specialize the code to the context as aggressively as possible.
    JSContextSpecializer spec(info(), &jsgraph, context_node);
    spec.SpecializeToContext();
    VerifyAndPrintGraph(&graph, "Context specialized");
  }

  // Print a replay of the initial graph.
  if (FLAG_print_turbo_replay) {
    GraphReplayPrinter::PrintReplay(&graph);
  }

  if (FLAG_turbo_types) {
    {
      // Type the graph.
      PhaseStats typer_stats(info(), PhaseStats::CREATE_GRAPH, "typer");
      typer.Run(&graph, info()->context());
    }
    // All new nodes must be typed.
    typer.DecorateGraph(&graph);
    {
      // Lower JSOperators where we can determine types.
      PhaseStats lowering_stats(info(), PhaseStats::CREATE_GRAPH,
                                "typed lowering");
      JSTypedLowering lowering(&jsgraph, &source_positions);
      lowering.LowerAllNodes();

      VerifyAndPrintGraph(&graph, "Lowered typed");
    }
  }

  Handle<Code> code = Handle<Code>::null();
  if (SupportedTarget()) {
    {
      // Lower any remaining generic JSOperators.
      PhaseStats lowering_stats(info(), PhaseStats::CREATE_GRAPH,
                                "generic lowering");
      MachineOperatorBuilder machine(zone());
      JSGenericLowering lowering(info(), &jsgraph, &machine, &source_positions);
      lowering.LowerAllNodes();

      VerifyAndPrintGraph(&graph, "Lowered generic");
    }

    // Compute a schedule.
    Schedule* schedule = ComputeSchedule(&graph);
    TraceSchedule(schedule);

    {
      // Generate optimized code.
      PhaseStats codegen_stats(info(), PhaseStats::CODEGEN, "codegen");
      Linkage linkage(info());
      code = GenerateCode(&linkage, &graph, schedule, &source_positions);
      info()->SetCode(code);
    }

    // Print optimized code.
    v8::internal::CodeGenerator::PrintCode(code, info());
  }

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "--------------------------------------------------\n"
       << "Finished compiling method "
       << info()->function()->debug_name()->ToCString().get()
       << " using Turbofan" << endl;
  }

  return code;
}


Schedule* Pipeline::ComputeSchedule(Graph* graph) {
  PhaseStats schedule_stats(info(), PhaseStats::CODEGEN, "scheduling");
  return Scheduler::ComputeSchedule(graph);
}


Handle<Code> Pipeline::GenerateCodeForMachineGraph(Linkage* linkage,
                                                   Graph* graph,
                                                   Schedule* schedule) {
  CHECK(SupportedBackend());
  if (schedule == NULL) {
    VerifyAndPrintGraph(graph, "Machine");
    schedule = ComputeSchedule(graph);
  }
  TraceSchedule(schedule);

  SourcePositionTable source_positions(graph);
  Handle<Code> code = GenerateCode(linkage, graph, schedule, &source_positions);
#if ENABLE_DISASSEMBLER
  if (!code.is_null() && FLAG_print_opt_code) {
    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    code->Disassemble("test code", os);
  }
#endif
  return code;
}


Handle<Code> Pipeline::GenerateCode(Linkage* linkage, Graph* graph,
                                    Schedule* schedule,
                                    SourcePositionTable* source_positions) {
  DCHECK_NOT_NULL(graph);
  DCHECK_NOT_NULL(linkage);
  DCHECK_NOT_NULL(schedule);
  CHECK(SupportedBackend());

  InstructionSequence sequence(linkage, graph, schedule);

  // Select and schedule instructions covering the scheduled graph.
  {
    InstructionSelector selector(&sequence, source_positions);
    selector.SelectInstructions();
  }

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "----- Instruction sequence before register allocation -----\n"
       << sequence;
  }

  // Allocate registers.
  {
    int node_count = graph->NodeCount();
    if (node_count > UnallocatedOperand::kMaxVirtualRegisters) {
      linkage->info()->set_bailout_reason(kNotEnoughVirtualRegistersForValues);
      return Handle<Code>::null();
    }
    RegisterAllocator allocator(&sequence);
    if (!allocator.Allocate()) {
      linkage->info()->set_bailout_reason(kNotEnoughVirtualRegistersRegalloc);
      return Handle<Code>::null();
    }
  }

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "----- Instruction sequence after register allocation -----\n"
       << sequence;
  }

  // Generate native sequence.
  CodeGenerator generator(&sequence);
  return generator.GenerateCode();
}


void Pipeline::SetUp() {
  InstructionOperand::SetUpCaches();
}


void Pipeline::TearDown() {
  InstructionOperand::TearDownCaches();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

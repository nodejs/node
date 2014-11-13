// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/pipeline.h"

#include <fstream>  // NOLINT(readability/streams)
#include <sstream>

#include "src/base/platform/elapsed-timer.h"
#include "src/compiler/ast-graph-builder.h"
#include "src/compiler/basic-block-instrumentor.h"
#include "src/compiler/change-lowering.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/control-reducer.h"
#include "src/compiler/graph-replay.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/instruction.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/js-context-specialization.h"
#include "src/compiler/js-generic-lowering.h"
#include "src/compiler/js-inlining.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/machine-operator-reducer.h"
#include "src/compiler/pipeline-statistics.h"
#include "src/compiler/register-allocator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "src/compiler/select-lowering.h"
#include "src/compiler/simplified-lowering.h"
#include "src/compiler/simplified-operator-reducer.h"
#include "src/compiler/typer.h"
#include "src/compiler/value-numbering-reducer.h"
#include "src/compiler/verifier.h"
#include "src/compiler/zone-pool.h"
#include "src/ostreams.h"
#include "src/utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class PipelineData {
 public:
  explicit PipelineData(CompilationInfo* info, ZonePool* zone_pool,
                        PipelineStatistics* pipeline_statistics)
      : isolate_(info->zone()->isolate()),
        outer_zone_(info->zone()),
        zone_pool_(zone_pool),
        pipeline_statistics_(pipeline_statistics),
        graph_zone_scope_(zone_pool_),
        graph_zone_(graph_zone_scope_.zone()),
        graph_(new (graph_zone()) Graph(graph_zone())),
        source_positions_(new SourcePositionTable(graph())),
        machine_(new (graph_zone()) MachineOperatorBuilder(
            kMachPtr, InstructionSelector::SupportedMachineOperatorFlags())),
        common_(new (graph_zone()) CommonOperatorBuilder(graph_zone())),
        javascript_(new (graph_zone()) JSOperatorBuilder(graph_zone())),
        jsgraph_(new (graph_zone())
                 JSGraph(graph(), common(), javascript(), machine())),
        typer_(new Typer(graph(), info->context())),
        schedule_(NULL),
        instruction_zone_scope_(zone_pool_),
        instruction_zone_(instruction_zone_scope_.zone()) {}

  // For machine graph testing only.
  PipelineData(Graph* graph, Schedule* schedule, ZonePool* zone_pool)
      : isolate_(graph->zone()->isolate()),
        outer_zone_(NULL),
        zone_pool_(zone_pool),
        pipeline_statistics_(NULL),
        graph_zone_scope_(zone_pool_),
        graph_zone_(NULL),
        graph_(graph),
        source_positions_(new SourcePositionTable(graph)),
        machine_(NULL),
        common_(NULL),
        javascript_(NULL),
        jsgraph_(NULL),
        typer_(NULL),
        schedule_(schedule),
        instruction_zone_scope_(zone_pool_),
        instruction_zone_(instruction_zone_scope_.zone()) {}

  ~PipelineData() {
    DeleteInstructionZone();
    DeleteGraphZone();
  }

  Isolate* isolate() const { return isolate_; }
  ZonePool* zone_pool() const { return zone_pool_; }
  PipelineStatistics* pipeline_statistics() { return pipeline_statistics_; }

  Zone* graph_zone() const { return graph_zone_; }
  Graph* graph() const { return graph_; }
  SourcePositionTable* source_positions() const {
    return source_positions_.get();
  }
  MachineOperatorBuilder* machine() const { return machine_; }
  CommonOperatorBuilder* common() const { return common_; }
  JSOperatorBuilder* javascript() const { return javascript_; }
  JSGraph* jsgraph() const { return jsgraph_; }
  Typer* typer() const { return typer_.get(); }
  Schedule* schedule() const { return schedule_; }
  void set_schedule(Schedule* schedule) {
    DCHECK_EQ(NULL, schedule_);
    schedule_ = schedule;
  }

  Zone* instruction_zone() const { return instruction_zone_; }

  void DeleteGraphZone() {
    // Destroy objects with destructors first.
    source_positions_.Reset(NULL);
    typer_.Reset(NULL);
    if (graph_zone_ == NULL) return;
    // Destroy zone and clear pointers.
    graph_zone_scope_.Destroy();
    graph_zone_ = NULL;
    graph_ = NULL;
    machine_ = NULL;
    common_ = NULL;
    javascript_ = NULL;
    jsgraph_ = NULL;
    schedule_ = NULL;
  }

  void DeleteInstructionZone() {
    if (instruction_zone_ == NULL) return;
    instruction_zone_scope_.Destroy();
    instruction_zone_ = NULL;
  }

 private:
  Isolate* isolate_;
  Zone* outer_zone_;
  ZonePool* zone_pool_;
  PipelineStatistics* pipeline_statistics_;

  ZonePool::Scope graph_zone_scope_;
  Zone* graph_zone_;
  // All objects in the following group of fields are allocated in graph_zone_.
  // They are all set to NULL when the graph_zone_ is destroyed.
  Graph* graph_;
  // TODO(dcarney): make this into a ZoneObject.
  SmartPointer<SourcePositionTable> source_positions_;
  MachineOperatorBuilder* machine_;
  CommonOperatorBuilder* common_;
  JSOperatorBuilder* javascript_;
  JSGraph* jsgraph_;
  // TODO(dcarney): make this into a ZoneObject.
  SmartPointer<Typer> typer_;
  Schedule* schedule_;

  // All objects in the following group of fields are allocated in
  // instruction_zone_.  They are all set to NULL when the instruction_zone_ is
  // destroyed.
  ZonePool::Scope instruction_zone_scope_;
  Zone* instruction_zone_;

  DISALLOW_COPY_AND_ASSIGN(PipelineData);
};


static inline bool VerifyGraphs() {
#ifdef DEBUG
  return true;
#else
  return FLAG_turbo_verify;
#endif
}


struct TurboCfgFile : public std::ofstream {
  explicit TurboCfgFile(Isolate* isolate)
      : std::ofstream(isolate->GetTurboCfgFileName().c_str(),
                      std::ios_base::app) {}
};


void Pipeline::VerifyAndPrintGraph(
    Graph* graph, const char* phase, bool untyped) {
  if (FLAG_trace_turbo) {
    char buffer[256];
    Vector<char> filename(buffer, sizeof(buffer));
    SmartArrayPointer<char> functionname;
    if (!info_->shared_info().is_null()) {
      functionname = info_->shared_info()->DebugName()->ToCString();
      if (strlen(functionname.get()) > 0) {
        SNPrintF(filename, "turbo-%s-%s", functionname.get(), phase);
      } else {
        SNPrintF(filename, "turbo-%p-%s", static_cast<void*>(info_), phase);
      }
    } else {
      SNPrintF(filename, "turbo-none-%s", phase);
    }
    std::replace(filename.start(), filename.start() + filename.length(), ' ',
                 '_');

    char dot_buffer[256];
    Vector<char> dot_filename(dot_buffer, sizeof(dot_buffer));
    SNPrintF(dot_filename, "%s.dot", filename.start());
    FILE* dot_file = base::OS::FOpen(dot_filename.start(), "w+");
    OFStream dot_of(dot_file);
    dot_of << AsDOT(*graph);
    fclose(dot_file);

    char json_buffer[256];
    Vector<char> json_filename(json_buffer, sizeof(json_buffer));
    SNPrintF(json_filename, "%s.json", filename.start());
    FILE* json_file = base::OS::FOpen(json_filename.start(), "w+");
    OFStream json_of(json_file);
    json_of << AsJSON(*graph);
    fclose(json_file);

    OFStream os(stdout);
    os << "-- " << phase << " graph printed to file " << filename.start()
       << "\n";
  }
  if (VerifyGraphs()) {
    Verifier::Run(graph,
        FLAG_turbo_types && !untyped ? Verifier::TYPED : Verifier::UNTYPED);
  }
}


class AstGraphBuilderWithPositions : public AstGraphBuilder {
 public:
  explicit AstGraphBuilderWithPositions(Zone* local_zone, CompilationInfo* info,
                                        JSGraph* jsgraph,
                                        SourcePositionTable* source_positions)
      : AstGraphBuilder(local_zone, info, jsgraph),
        source_positions_(source_positions) {}

  bool CreateGraph() {
    SourcePositionTable::Scope pos(source_positions_,
                                   SourcePosition::Unknown());
    return AstGraphBuilder::CreateGraph();
  }

#define DEF_VISIT(type)                                               \
  virtual void Visit##type(type* node) OVERRIDE {                     \
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


static SmartArrayPointer<char> GetDebugName(CompilationInfo* info) {
  SmartArrayPointer<char> name;
  if (info->IsStub()) {
    if (info->code_stub() != NULL) {
      CodeStub::Major major_key = info->code_stub()->MajorKey();
      const char* major_name = CodeStub::MajorName(major_key, false);
      size_t len = strlen(major_name);
      name.Reset(new char[len]);
      memcpy(name.get(), major_name, len);
    }
  } else {
    AllowHandleDereference allow_deref;
    name = info->function()->debug_name()->ToCString();
  }
  return name;
}


Handle<Code> Pipeline::GenerateCode() {
  // This list must be kept in sync with DONT_TURBOFAN_NODE in ast.cc.
  if (info()->function()->dont_optimize_reason() == kTryCatchStatement ||
      info()->function()->dont_optimize_reason() == kTryFinallyStatement ||
      // TODO(turbofan): Make ES6 for-of work and remove this bailout.
      info()->function()->dont_optimize_reason() == kForOfStatement ||
      // TODO(turbofan): Make super work and remove this bailout.
      info()->function()->dont_optimize_reason() == kSuperReference ||
      // TODO(turbofan): Make class literals work and remove this bailout.
      info()->function()->dont_optimize_reason() == kClassLiteral ||
      // TODO(turbofan): Make OSR work and remove this bailout.
      info()->is_osr()) {
    return Handle<Code>::null();
  }

  ZonePool zone_pool(isolate());

  SmartPointer<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats) {
    pipeline_statistics.Reset(new PipelineStatistics(info(), &zone_pool));
    pipeline_statistics->BeginPhaseKind("graph creation");
  }

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "---------------------------------------------------\n"
       << "Begin compiling method " << GetDebugName(info()).get()
       << " using Turbofan" << std::endl;
    TurboCfgFile tcf(isolate());
    tcf << AsC1VCompilation(info());
  }

  // Initialize the graph and builders.
  PipelineData data(info(), &zone_pool, pipeline_statistics.get());

  data.source_positions()->AddDecorator();

  Node* context_node;
  {
    PhaseScope phase_scope(pipeline_statistics.get(), "graph builder");
    ZonePool::Scope zone_scope(data.zone_pool());
    AstGraphBuilderWithPositions graph_builder(
        zone_scope.zone(), info(), data.jsgraph(), data.source_positions());
    if (!graph_builder.CreateGraph()) return Handle<Code>::null();
    context_node = graph_builder.GetFunctionContext();
  }

  VerifyAndPrintGraph(data.graph(), "Initial untyped", true);

  {
    PhaseScope phase_scope(pipeline_statistics.get(),
                           "early control reduction");
    SourcePositionTable::Scope pos(data.source_positions(),
                                   SourcePosition::Unknown());
    ZonePool::Scope zone_scope(data.zone_pool());
    ControlReducer::ReduceGraph(zone_scope.zone(), data.jsgraph(),
                                data.common());

    VerifyAndPrintGraph(data.graph(), "Early Control reduced", true);
  }

  if (info()->is_context_specializing()) {
    SourcePositionTable::Scope pos(data.source_positions(),
                                   SourcePosition::Unknown());
    // Specialize the code to the context as aggressively as possible.
    JSContextSpecializer spec(info(), data.jsgraph(), context_node);
    spec.SpecializeToContext();
    VerifyAndPrintGraph(data.graph(), "Context specialized", true);
  }

  if (info()->is_inlining_enabled()) {
    PhaseScope phase_scope(pipeline_statistics.get(), "inlining");
    SourcePositionTable::Scope pos(data.source_positions(),
                                   SourcePosition::Unknown());
    ZonePool::Scope zone_scope(data.zone_pool());
    JSInliner inliner(zone_scope.zone(), info(), data.jsgraph());
    inliner.Inline();
    VerifyAndPrintGraph(data.graph(), "Inlined", true);
  }

  // Print a replay of the initial graph.
  if (FLAG_print_turbo_replay) {
    GraphReplayPrinter::PrintReplay(data.graph());
  }

  // Bailout here in case target architecture is not supported.
  if (!SupportedTarget()) return Handle<Code>::null();

  if (info()->is_typing_enabled()) {
    {
      // Type the graph.
      PhaseScope phase_scope(pipeline_statistics.get(), "typer");
      data.typer()->Run();
      VerifyAndPrintGraph(data.graph(), "Typed");
    }
  }

  if (!pipeline_statistics.is_empty()) {
    pipeline_statistics->BeginPhaseKind("lowering");
  }

  if (info()->is_typing_enabled()) {
    {
      // Lower JSOperators where we can determine types.
      PhaseScope phase_scope(pipeline_statistics.get(), "typed lowering");
      SourcePositionTable::Scope pos(data.source_positions(),
                                     SourcePosition::Unknown());
      ValueNumberingReducer vn_reducer(data.graph_zone());
      JSTypedLowering lowering(data.jsgraph());
      SimplifiedOperatorReducer simple_reducer(data.jsgraph());
      GraphReducer graph_reducer(data.graph());
      graph_reducer.AddReducer(&vn_reducer);
      graph_reducer.AddReducer(&lowering);
      graph_reducer.AddReducer(&simple_reducer);
      graph_reducer.ReduceGraph();

      VerifyAndPrintGraph(data.graph(), "Lowered typed");
    }
    {
      // Lower simplified operators and insert changes.
      PhaseScope phase_scope(pipeline_statistics.get(), "simplified lowering");
      SourcePositionTable::Scope pos(data.source_positions(),
                                     SourcePosition::Unknown());
      SimplifiedLowering lowering(data.jsgraph());
      lowering.LowerAllNodes();
      ValueNumberingReducer vn_reducer(data.graph_zone());
      SimplifiedOperatorReducer simple_reducer(data.jsgraph());
      GraphReducer graph_reducer(data.graph());
      graph_reducer.AddReducer(&vn_reducer);
      graph_reducer.AddReducer(&simple_reducer);
      graph_reducer.ReduceGraph();

      VerifyAndPrintGraph(data.graph(), "Lowered simplified");
    }
    {
      // Lower changes that have been inserted before.
      PhaseScope phase_scope(pipeline_statistics.get(), "change lowering");
      SourcePositionTable::Scope pos(data.source_positions(),
                                     SourcePosition::Unknown());
      Linkage linkage(data.graph_zone(), info());
      ValueNumberingReducer vn_reducer(data.graph_zone());
      SimplifiedOperatorReducer simple_reducer(data.jsgraph());
      ChangeLowering lowering(data.jsgraph(), &linkage);
      MachineOperatorReducer mach_reducer(data.jsgraph());
      GraphReducer graph_reducer(data.graph());
      // TODO(titzer): Figure out if we should run all reducers at once here.
      graph_reducer.AddReducer(&vn_reducer);
      graph_reducer.AddReducer(&simple_reducer);
      graph_reducer.AddReducer(&lowering);
      graph_reducer.AddReducer(&mach_reducer);
      graph_reducer.ReduceGraph();

      // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
      VerifyAndPrintGraph(data.graph(), "Lowered changes", true);
    }

    {
      PhaseScope phase_scope(pipeline_statistics.get(),
                             "late control reduction");
      SourcePositionTable::Scope pos(data.source_positions(),
                                     SourcePosition::Unknown());
      ZonePool::Scope zone_scope(data.zone_pool());
      ControlReducer::ReduceGraph(zone_scope.zone(), data.jsgraph(),
                                  data.common());

      VerifyAndPrintGraph(data.graph(), "Late Control reduced");
    }
  }

  {
    // Lower any remaining generic JSOperators.
    PhaseScope phase_scope(pipeline_statistics.get(), "generic lowering");
    SourcePositionTable::Scope pos(data.source_positions(),
                                   SourcePosition::Unknown());
    JSGenericLowering generic(info(), data.jsgraph());
    SelectLowering select(data.jsgraph()->graph(), data.jsgraph()->common());
    GraphReducer graph_reducer(data.graph());
    graph_reducer.AddReducer(&generic);
    graph_reducer.AddReducer(&select);
    graph_reducer.ReduceGraph();

    // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
    VerifyAndPrintGraph(data.graph(), "Lowered generic", true);
  }

  if (!pipeline_statistics.is_empty()) {
    pipeline_statistics->BeginPhaseKind("block building");
  }

  data.source_positions()->RemoveDecorator();

  // Compute a schedule.
  ComputeSchedule(&data);

  Handle<Code> code = Handle<Code>::null();
  {
    // Generate optimized code.
    Linkage linkage(data.instruction_zone(), info());
    code = GenerateCode(&linkage, &data);
    info()->SetCode(code);
  }

  // Print optimized code.
  v8::internal::CodeGenerator::PrintCode(code, info());

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "--------------------------------------------------\n"
       << "Finished compiling method " << GetDebugName(info()).get()
       << " using Turbofan" << std::endl;
  }

  return code;
}


void Pipeline::ComputeSchedule(PipelineData* data) {
  PhaseScope phase_scope(data->pipeline_statistics(), "scheduling");
  Schedule* schedule =
      Scheduler::ComputeSchedule(data->zone_pool(), data->graph());
  TraceSchedule(schedule);
  if (VerifyGraphs()) ScheduleVerifier::Run(schedule);
  data->set_schedule(schedule);
}


Handle<Code> Pipeline::GenerateCodeForMachineGraph(Linkage* linkage,
                                                   Graph* graph,
                                                   Schedule* schedule) {
  ZonePool zone_pool(isolate());
  CHECK(SupportedBackend());
  PipelineData data(graph, schedule, &zone_pool);
  if (schedule == NULL) {
    // TODO(rossberg): Should this really be untyped?
    VerifyAndPrintGraph(graph, "Machine", true);
    ComputeSchedule(&data);
  } else {
    TraceSchedule(schedule);
  }

  Handle<Code> code = GenerateCode(linkage, &data);
#if ENABLE_DISASSEMBLER
  if (!code.is_null() && FLAG_print_opt_code) {
    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    code->Disassemble("test code", os);
  }
#endif
  return code;
}


Handle<Code> Pipeline::GenerateCode(Linkage* linkage, PipelineData* data) {
  DCHECK_NOT_NULL(linkage);
  DCHECK_NOT_NULL(data->graph());
  DCHECK_NOT_NULL(data->schedule());
  CHECK(SupportedBackend());

  BasicBlockProfiler::Data* profiler_data = NULL;
  if (FLAG_turbo_profiling) {
    profiler_data = BasicBlockInstrumentor::Instrument(info(), data->graph(),
                                                       data->schedule());
  }

  InstructionBlocks* instruction_blocks =
      InstructionSequence::InstructionBlocksFor(data->instruction_zone(),
                                                data->schedule());
  InstructionSequence sequence(data->instruction_zone(), instruction_blocks);

  // Select and schedule instructions covering the scheduled graph.
  {
    PhaseScope phase_scope(data->pipeline_statistics(), "select instructions");
    ZonePool::Scope zone_scope(data->zone_pool());
    InstructionSelector selector(zone_scope.zone(), data->graph(), linkage,
                                 &sequence, data->schedule(),
                                 data->source_positions());
    selector.SelectInstructions();
  }

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    PrintableInstructionSequence printable = {
        RegisterConfiguration::ArchDefault(), &sequence};
    os << "----- Instruction sequence before register allocation -----\n"
       << printable;
    TurboCfgFile tcf(isolate());
    tcf << AsC1V("CodeGen", data->schedule(), data->source_positions(),
                 &sequence);
  }

  data->DeleteGraphZone();

  if (data->pipeline_statistics() != NULL) {
    data->pipeline_statistics()->BeginPhaseKind("register allocation");
  }

  // Allocate registers.
  Frame frame;
  {
    int node_count = sequence.VirtualRegisterCount();
    if (node_count > UnallocatedOperand::kMaxVirtualRegisters) {
      info()->AbortOptimization(kNotEnoughVirtualRegistersForValues);
      return Handle<Code>::null();
    }
    ZonePool::Scope zone_scope(data->zone_pool());

    SmartArrayPointer<char> debug_name;
#ifdef DEBUG
    debug_name = GetDebugName(info());
#endif


    RegisterAllocator allocator(RegisterConfiguration::ArchDefault(),
                                zone_scope.zone(), &frame, &sequence,
                                debug_name.get());
    if (!allocator.Allocate(data->pipeline_statistics())) {
      info()->AbortOptimization(kNotEnoughVirtualRegistersRegalloc);
      return Handle<Code>::null();
    }
    if (FLAG_trace_turbo) {
      TurboCfgFile tcf(isolate());
      tcf << AsC1VAllocator("CodeGen", &allocator);
    }
  }

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    PrintableInstructionSequence printable = {
        RegisterConfiguration::ArchDefault(), &sequence};
    os << "----- Instruction sequence after register allocation -----\n"
       << printable;
  }

  if (data->pipeline_statistics() != NULL) {
    data->pipeline_statistics()->BeginPhaseKind("code generation");
  }

  // Generate native sequence.
  Handle<Code> code;
  {
    PhaseScope phase_scope(data->pipeline_statistics(), "generate code");
    CodeGenerator generator(&frame, linkage, &sequence, info());
    code = generator.GenerateCode();
  }
  if (profiler_data != NULL) {
#if ENABLE_DISASSEMBLER
    std::ostringstream os;
    code->Disassemble(NULL, os);
    profiler_data->SetCode(&os);
#endif
  }
  return code;
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

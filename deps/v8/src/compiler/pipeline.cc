// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/pipeline.h"

#include <fstream>  // NOLINT(readability/streams)
#include <sstream>

#include "src/base/platform/elapsed-timer.h"
#include "src/compiler/ast-graph-builder.h"
#include "src/compiler/ast-loop-assignment-analyzer.h"
#include "src/compiler/basic-block-instrumentor.h"
#include "src/compiler/change-lowering.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/common-operator-reducer.h"
#include "src/compiler/control-reducer.h"
#include "src/compiler/graph-replay.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/instruction.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/js-builtin-reducer.h"
#include "src/compiler/js-context-specialization.h"
#include "src/compiler/js-generic-lowering.h"
#include "src/compiler/js-inlining.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/jump-threading.h"
#include "src/compiler/load-elimination.h"
#include "src/compiler/machine-operator-reducer.h"
#include "src/compiler/move-optimizer.h"
#include "src/compiler/pipeline-statistics.h"
#include "src/compiler/register-allocator.h"
#include "src/compiler/register-allocator-verifier.h"
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
  explicit PipelineData(ZonePool* zone_pool, CompilationInfo* info)
      : isolate_(info->zone()->isolate()),
        info_(info),
        outer_zone_(nullptr),
        zone_pool_(zone_pool),
        pipeline_statistics_(nullptr),
        compilation_failed_(false),
        code_(Handle<Code>::null()),
        graph_zone_scope_(zone_pool_),
        graph_zone_(nullptr),
        graph_(nullptr),
        loop_assignment_(nullptr),
        machine_(nullptr),
        common_(nullptr),
        javascript_(nullptr),
        jsgraph_(nullptr),
        typer_(nullptr),
        context_node_(nullptr),
        schedule_(nullptr),
        instruction_zone_scope_(zone_pool_),
        instruction_zone_(nullptr),
        sequence_(nullptr),
        frame_(nullptr),
        register_allocator_(nullptr) {}

  ~PipelineData() {
    DeleteInstructionZone();
    DeleteGraphZone();
  }

  // For main entry point.
  void Initialize(PipelineStatistics* pipeline_statistics) {
    PhaseScope scope(pipeline_statistics, "init pipeline data");
    outer_zone_ = info()->zone();
    pipeline_statistics_ = pipeline_statistics;
    graph_zone_ = graph_zone_scope_.zone();
    graph_ = new (graph_zone()) Graph(graph_zone());
    source_positions_.Reset(new SourcePositionTable(graph()));
    machine_ = new (graph_zone()) MachineOperatorBuilder(
        graph_zone(), kMachPtr,
        InstructionSelector::SupportedMachineOperatorFlags());
    common_ = new (graph_zone()) CommonOperatorBuilder(graph_zone());
    javascript_ = new (graph_zone()) JSOperatorBuilder(graph_zone());
    jsgraph_ =
        new (graph_zone()) JSGraph(graph(), common(), javascript(), machine());
    typer_.Reset(new Typer(graph(), info()->context()));
    instruction_zone_ = instruction_zone_scope_.zone();
  }

  // For machine graph testing entry point.
  void InitializeTorTesting(Graph* graph, Schedule* schedule) {
    graph_ = graph;
    source_positions_.Reset(new SourcePositionTable(graph));
    schedule_ = schedule;
    instruction_zone_ = instruction_zone_scope_.zone();
  }

  // For register allocation testing entry point.
  void InitializeTorTesting(InstructionSequence* sequence) {
    instruction_zone_ = sequence->zone();
    sequence_ = sequence;
  }

  Isolate* isolate() const { return isolate_; }
  CompilationInfo* info() const { return info_; }
  ZonePool* zone_pool() const { return zone_pool_; }
  PipelineStatistics* pipeline_statistics() { return pipeline_statistics_; }
  bool compilation_failed() const { return compilation_failed_; }
  void set_compilation_failed() { compilation_failed_ = true; }
  Handle<Code> code() { return code_; }
  void set_code(Handle<Code> code) {
    DCHECK(code_.is_null());
    code_ = code;
  }

  // RawMachineAssembler generally produces graphs which cannot be verified.
  bool MayHaveUnverifiableGraph() const { return outer_zone_ == nullptr; }

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

  LoopAssignmentAnalysis* loop_assignment() const { return loop_assignment_; }
  void set_loop_assignment(LoopAssignmentAnalysis* loop_assignment) {
    DCHECK_EQ(nullptr, loop_assignment_);
    loop_assignment_ = loop_assignment;
  }

  Node* context_node() const { return context_node_; }
  void set_context_node(Node* context_node) {
    DCHECK_EQ(nullptr, context_node_);
    context_node_ = context_node;
  }

  Schedule* schedule() const { return schedule_; }
  void set_schedule(Schedule* schedule) {
    DCHECK_EQ(nullptr, schedule_);
    schedule_ = schedule;
  }

  Zone* instruction_zone() const { return instruction_zone_; }
  InstructionSequence* sequence() const { return sequence_; }
  Frame* frame() const { return frame_; }
  RegisterAllocator* register_allocator() const { return register_allocator_; }

  void DeleteGraphZone() {
    // Destroy objects with destructors first.
    source_positions_.Reset(nullptr);
    typer_.Reset(nullptr);
    if (graph_zone_ == nullptr) return;
    // Destroy zone and clear pointers.
    graph_zone_scope_.Destroy();
    graph_zone_ = nullptr;
    graph_ = nullptr;
    loop_assignment_ = nullptr;
    machine_ = nullptr;
    common_ = nullptr;
    javascript_ = nullptr;
    jsgraph_ = nullptr;
    context_node_ = nullptr;
    schedule_ = nullptr;
  }

  void DeleteInstructionZone() {
    if (instruction_zone_ == nullptr) return;
    instruction_zone_scope_.Destroy();
    instruction_zone_ = nullptr;
    sequence_ = nullptr;
    frame_ = nullptr;
    register_allocator_ = nullptr;
  }

  void InitializeInstructionSequence() {
    DCHECK_EQ(nullptr, sequence_);
    InstructionBlocks* instruction_blocks =
        InstructionSequence::InstructionBlocksFor(instruction_zone(),
                                                  schedule());
    sequence_ = new (instruction_zone())
        InstructionSequence(instruction_zone(), instruction_blocks);
  }

  void InitializeRegisterAllocator(Zone* local_zone,
                                   const RegisterConfiguration* config,
                                   const char* debug_name) {
    DCHECK_EQ(nullptr, register_allocator_);
    DCHECK_EQ(nullptr, frame_);
    frame_ = new (instruction_zone()) Frame();
    register_allocator_ = new (instruction_zone())
        RegisterAllocator(config, local_zone, frame(), sequence(), debug_name);
  }

 private:
  Isolate* isolate_;
  CompilationInfo* info_;
  Zone* outer_zone_;
  ZonePool* const zone_pool_;
  PipelineStatistics* pipeline_statistics_;
  bool compilation_failed_;
  Handle<Code> code_;

  // All objects in the following group of fields are allocated in graph_zone_.
  // They are all set to NULL when the graph_zone_ is destroyed.
  ZonePool::Scope graph_zone_scope_;
  Zone* graph_zone_;
  Graph* graph_;
  // TODO(dcarney): make this into a ZoneObject.
  SmartPointer<SourcePositionTable> source_positions_;
  LoopAssignmentAnalysis* loop_assignment_;
  MachineOperatorBuilder* machine_;
  CommonOperatorBuilder* common_;
  JSOperatorBuilder* javascript_;
  JSGraph* jsgraph_;
  // TODO(dcarney): make this into a ZoneObject.
  SmartPointer<Typer> typer_;
  Node* context_node_;
  Schedule* schedule_;

  // All objects in the following group of fields are allocated in
  // instruction_zone_.  They are all set to NULL when the instruction_zone_ is
  // destroyed.
  ZonePool::Scope instruction_zone_scope_;
  Zone* instruction_zone_;
  InstructionSequence* sequence_;
  Frame* frame_;
  RegisterAllocator* register_allocator_;

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


static void TraceSchedule(Schedule* schedule) {
  if (!FLAG_trace_turbo_graph && !FLAG_trace_turbo_scheduler) return;
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


class AstGraphBuilderWithPositions : public AstGraphBuilder {
 public:
  AstGraphBuilderWithPositions(Zone* local_zone, CompilationInfo* info,
                               JSGraph* jsgraph,
                               LoopAssignmentAnalysis* loop_assignment,
                               SourcePositionTable* source_positions)
      : AstGraphBuilder(local_zone, info, jsgraph, loop_assignment),
        source_positions_(source_positions) {}

  bool CreateGraph() {
    SourcePositionTable::Scope pos(source_positions_,
                                   SourcePosition::Unknown());
    return AstGraphBuilder::CreateGraph();
  }

#define DEF_VISIT(type)                                               \
  void Visit##type(type* node) OVERRIDE {                             \
    SourcePositionTable::Scope pos(source_positions_,                 \
                                   SourcePosition(node->position())); \
    AstGraphBuilder::Visit##type(node);                               \
  }
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

  Node* GetFunctionContext() { return AstGraphBuilder::GetFunctionContext(); }

 private:
  SourcePositionTable* source_positions_;
};


class PipelineRunScope {
 public:
  PipelineRunScope(PipelineData* data, const char* phase_name)
      : phase_scope_(
            phase_name == nullptr ? nullptr : data->pipeline_statistics(),
            phase_name),
        zone_scope_(data->zone_pool()) {}

  Zone* zone() { return zone_scope_.zone(); }

 private:
  PhaseScope phase_scope_;
  ZonePool::Scope zone_scope_;
};


template <typename Phase>
void Pipeline::Run() {
  PipelineRunScope scope(this->data_, Phase::phase_name());
  Phase phase;
  phase.Run(this->data_, scope.zone());
}


template <typename Phase, typename Arg0>
void Pipeline::Run(Arg0 arg_0) {
  PipelineRunScope scope(this->data_, Phase::phase_name());
  Phase phase;
  phase.Run(this->data_, scope.zone(), arg_0);
}


struct LoopAssignmentAnalysisPhase {
  static const char* phase_name() { return "loop assignment analysis"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    AstLoopAssignmentAnalyzer analyzer(data->graph_zone(), data->info());
    LoopAssignmentAnalysis* loop_assignment = analyzer.Analyze();
    data->set_loop_assignment(loop_assignment);
  }
};


struct GraphBuilderPhase {
  static const char* phase_name() { return "graph builder"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    AstGraphBuilderWithPositions graph_builder(
        temp_zone, data->info(), data->jsgraph(), data->loop_assignment(),
        data->source_positions());
    if (graph_builder.CreateGraph()) {
      data->set_context_node(graph_builder.GetFunctionContext());
    } else {
      data->set_compilation_failed();
    }
  }
};


struct ContextSpecializerPhase {
  static const char* phase_name() { return "context specializing"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    JSContextSpecializer spec(data->info(), data->jsgraph(),
                              data->context_node());
    GraphReducer graph_reducer(data->graph(), temp_zone);
    graph_reducer.AddReducer(&spec);
    graph_reducer.ReduceGraph();
  }
};


struct InliningPhase {
  static const char* phase_name() { return "inlining"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    JSInliner inliner(temp_zone, data->info(), data->jsgraph());
    inliner.Inline();
  }
};


struct TyperPhase {
  static const char* phase_name() { return "typer"; }

  void Run(PipelineData* data, Zone* temp_zone) { data->typer()->Run(); }
};


struct TypedLoweringPhase {
  static const char* phase_name() { return "typed lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    ValueNumberingReducer vn_reducer(temp_zone);
    LoadElimination load_elimination;
    JSBuiltinReducer builtin_reducer(data->jsgraph());
    JSTypedLowering typed_lowering(data->jsgraph(), temp_zone);
    SimplifiedOperatorReducer simple_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer;
    GraphReducer graph_reducer(data->graph(), temp_zone);
    graph_reducer.AddReducer(&vn_reducer);
    graph_reducer.AddReducer(&builtin_reducer);
    graph_reducer.AddReducer(&typed_lowering);
    graph_reducer.AddReducer(&load_elimination);
    graph_reducer.AddReducer(&simple_reducer);
    graph_reducer.AddReducer(&common_reducer);
    graph_reducer.ReduceGraph();
  }
};


struct SimplifiedLoweringPhase {
  static const char* phase_name() { return "simplified lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    SimplifiedLowering lowering(data->jsgraph(), temp_zone);
    lowering.LowerAllNodes();
    ValueNumberingReducer vn_reducer(temp_zone);
    SimplifiedOperatorReducer simple_reducer(data->jsgraph());
    MachineOperatorReducer machine_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer;
    GraphReducer graph_reducer(data->graph(), temp_zone);
    graph_reducer.AddReducer(&vn_reducer);
    graph_reducer.AddReducer(&simple_reducer);
    graph_reducer.AddReducer(&machine_reducer);
    graph_reducer.AddReducer(&common_reducer);
    graph_reducer.ReduceGraph();
  }
};


struct ChangeLoweringPhase {
  static const char* phase_name() { return "change lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    Linkage linkage(data->graph_zone(), data->info());
    ValueNumberingReducer vn_reducer(temp_zone);
    SimplifiedOperatorReducer simple_reducer(data->jsgraph());
    ChangeLowering lowering(data->jsgraph(), &linkage);
    MachineOperatorReducer machine_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer;
    GraphReducer graph_reducer(data->graph(), temp_zone);
    graph_reducer.AddReducer(&vn_reducer);
    graph_reducer.AddReducer(&simple_reducer);
    graph_reducer.AddReducer(&lowering);
    graph_reducer.AddReducer(&machine_reducer);
    graph_reducer.AddReducer(&common_reducer);
    graph_reducer.ReduceGraph();
  }
};


struct ControlReductionPhase {
  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    ControlReducer::ReduceGraph(temp_zone, data->jsgraph(), data->common());
  }
};


struct EarlyControlReductionPhase : ControlReductionPhase {
  static const char* phase_name() { return "early control reduction"; }
};


struct LateControlReductionPhase : ControlReductionPhase {
  static const char* phase_name() { return "late control reduction"; }
};


struct GenericLoweringPhase {
  static const char* phase_name() { return "generic lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    JSGenericLowering generic(data->info(), data->jsgraph());
    SelectLowering select(data->jsgraph()->graph(), data->jsgraph()->common());
    GraphReducer graph_reducer(data->graph(), temp_zone);
    graph_reducer.AddReducer(&generic);
    graph_reducer.AddReducer(&select);
    graph_reducer.ReduceGraph();
  }
};


struct ComputeSchedulePhase {
  static const char* phase_name() { return "scheduling"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    Schedule* schedule = Scheduler::ComputeSchedule(temp_zone, data->graph());
    TraceSchedule(schedule);
    if (VerifyGraphs()) ScheduleVerifier::Run(schedule);
    data->set_schedule(schedule);
  }
};


struct InstructionSelectionPhase {
  static const char* phase_name() { return "select instructions"; }

  void Run(PipelineData* data, Zone* temp_zone, Linkage* linkage) {
    InstructionSelector selector(temp_zone, data->graph(), linkage,
                                 data->sequence(), data->schedule(),
                                 data->source_positions());
    selector.SelectInstructions();
  }
};


struct MeetRegisterConstraintsPhase {
  static const char* phase_name() { return "meet register constraints"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->register_allocator()->MeetRegisterConstraints();
  }
};


struct ResolvePhisPhase {
  static const char* phase_name() { return "resolve phis"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->register_allocator()->ResolvePhis();
  }
};


struct BuildLiveRangesPhase {
  static const char* phase_name() { return "build live ranges"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->register_allocator()->BuildLiveRanges();
  }
};


struct AllocateGeneralRegistersPhase {
  static const char* phase_name() { return "allocate general registers"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->register_allocator()->AllocateGeneralRegisters();
  }
};


struct AllocateDoubleRegistersPhase {
  static const char* phase_name() { return "allocate double registers"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->register_allocator()->AllocateDoubleRegisters();
  }
};


struct ReuseSpillSlotsPhase {
  static const char* phase_name() { return "reuse spill slots"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->register_allocator()->ReuseSpillSlots();
  }
};


struct CommitAssignmentPhase {
  static const char* phase_name() { return "commit assignment"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->register_allocator()->CommitAssignment();
  }
};


struct PopulatePointerMapsPhase {
  static const char* phase_name() { return "populate pointer maps"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->register_allocator()->PopulatePointerMaps();
  }
};


struct ConnectRangesPhase {
  static const char* phase_name() { return "connect ranges"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->register_allocator()->ConnectRanges();
  }
};


struct ResolveControlFlowPhase {
  static const char* phase_name() { return "resolve control flow"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->register_allocator()->ResolveControlFlow();
  }
};


struct OptimizeMovesPhase {
  static const char* phase_name() { return "optimize moves"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    MoveOptimizer move_optimizer(temp_zone, data->sequence());
    move_optimizer.Run();
  }
};


struct JumpThreadingPhase {
  static const char* phase_name() { return "jump threading"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    ZoneVector<BasicBlock::RpoNumber> result(temp_zone);
    if (JumpThreading::ComputeForwarding(temp_zone, result, data->sequence())) {
      JumpThreading::ApplyForwarding(result, data->sequence());
    }
  }
};


struct GenerateCodePhase {
  static const char* phase_name() { return "generate code"; }

  void Run(PipelineData* data, Zone* temp_zone, Linkage* linkage) {
    CodeGenerator generator(data->frame(), linkage, data->sequence(),
                            data->info());
    data->set_code(generator.GenerateCode());
  }
};


struct PrintGraphPhase {
  static const char* phase_name() { return nullptr; }

  void Run(PipelineData* data, Zone* temp_zone, const char* phase) {
    CompilationInfo* info = data->info();
    Graph* graph = data->graph();
    char buffer[256];
    Vector<char> filename(buffer, sizeof(buffer));
    SmartArrayPointer<char> functionname;
    if (!info->shared_info().is_null()) {
      functionname = info->shared_info()->DebugName()->ToCString();
      if (strlen(functionname.get()) > 0) {
        SNPrintF(filename, "turbo-%s-%s", functionname.get(), phase);
      } else {
        SNPrintF(filename, "turbo-%p-%s", static_cast<void*>(info), phase);
      }
    } else {
      SNPrintF(filename, "turbo-none-%s", phase);
    }
    std::replace(filename.start(), filename.start() + filename.length(), ' ',
                 '_');

    {  // Print dot.
      char dot_buffer[256];
      Vector<char> dot_filename(dot_buffer, sizeof(dot_buffer));
      SNPrintF(dot_filename, "%s.dot", filename.start());
      FILE* dot_file = base::OS::FOpen(dot_filename.start(), "w+");
      if (dot_file == nullptr) return;
      OFStream dot_of(dot_file);
      dot_of << AsDOT(*graph);
      fclose(dot_file);
    }

    {  // Print JSON.
      char json_buffer[256];
      Vector<char> json_filename(json_buffer, sizeof(json_buffer));
      SNPrintF(json_filename, "%s.json", filename.start());
      FILE* json_file = base::OS::FOpen(json_filename.start(), "w+");
      if (json_file == nullptr) return;
      OFStream json_of(json_file);
      json_of << AsJSON(*graph);
      fclose(json_file);
    }

    OFStream os(stdout);
    if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
      os << "-- Graph after " << phase << " -- " << std::endl;
      os << AsRPO(*graph);
    }

    os << "-- " << phase << " graph printed to file " << filename.start()
       << std::endl;
  }
};


struct VerifyGraphPhase {
  static const char* phase_name() { return nullptr; }

  void Run(PipelineData* data, Zone* temp_zone, const bool untyped) {
    Verifier::Run(data->graph(), FLAG_turbo_types && !untyped
                                     ? Verifier::TYPED
                                     : Verifier::UNTYPED);
  }
};


void Pipeline::BeginPhaseKind(const char* phase_kind_name) {
  if (data_->pipeline_statistics() != NULL) {
    data_->pipeline_statistics()->BeginPhaseKind(phase_kind_name);
  }
}


void Pipeline::RunPrintAndVerify(const char* phase, bool untyped) {
  if (FLAG_trace_turbo) {
    Run<PrintGraphPhase>(phase);
  }
  if (VerifyGraphs()) {
    Run<VerifyGraphPhase>(untyped);
  }
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
    pipeline_statistics->BeginPhaseKind("initializing");
  }

  PipelineData data(&zone_pool, info());
  this->data_ = &data;
  data.Initialize(pipeline_statistics.get());

  BeginPhaseKind("graph creation");

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "---------------------------------------------------\n"
       << "Begin compiling method " << GetDebugName(info()).get()
       << " using Turbofan" << std::endl;
    TurboCfgFile tcf(isolate());
    tcf << AsC1VCompilation(info());
  }

  data.source_positions()->AddDecorator();

  if (FLAG_loop_assignment_analysis) {
    Run<LoopAssignmentAnalysisPhase>();
  }

  Run<GraphBuilderPhase>();
  if (data.compilation_failed()) return Handle<Code>::null();
  RunPrintAndVerify("Initial untyped", true);

  Run<EarlyControlReductionPhase>();
  RunPrintAndVerify("Early Control reduced", true);

  if (info()->is_context_specializing()) {
    // Specialize the code to the context as aggressively as possible.
    Run<ContextSpecializerPhase>();
    RunPrintAndVerify("Context specialized", true);
  }

  if (info()->is_inlining_enabled()) {
    Run<InliningPhase>();
    RunPrintAndVerify("Inlined", true);
  }

  if (FLAG_print_turbo_replay) {
    // Print a replay of the initial graph.
    GraphReplayPrinter::PrintReplay(data.graph());
  }

  // Bailout here in case target architecture is not supported.
  if (!SupportedTarget()) return Handle<Code>::null();

  if (info()->is_typing_enabled()) {
    // Type the graph.
    Run<TyperPhase>();
    RunPrintAndVerify("Typed");
  }

  BeginPhaseKind("lowering");

  if (info()->is_typing_enabled()) {
    // Lower JSOperators where we can determine types.
    Run<TypedLoweringPhase>();
    RunPrintAndVerify("Lowered typed");

    // Lower simplified operators and insert changes.
    Run<SimplifiedLoweringPhase>();
    RunPrintAndVerify("Lowered simplified");

    // Lower changes that have been inserted before.
    Run<ChangeLoweringPhase>();
    // // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
    RunPrintAndVerify("Lowered changes", true);

    Run<LateControlReductionPhase>();
    RunPrintAndVerify("Late Control reduced");
  }

  // Lower any remaining generic JSOperators.
  Run<GenericLoweringPhase>();
  // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
  RunPrintAndVerify("Lowered generic", true);

  BeginPhaseKind("block building");

  data.source_positions()->RemoveDecorator();

  // Compute a schedule.
  Run<ComputeSchedulePhase>();

  {
    // Generate optimized code.
    Linkage linkage(data.instruction_zone(), info());
    GenerateCode(&linkage);
  }
  Handle<Code> code = data.code();
  info()->SetCode(code);

  // Print optimized code.
  v8::internal::CodeGenerator::PrintCode(code, info());

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "---------------------------------------------------\n"
       << "Finished compiling method " << GetDebugName(info()).get()
       << " using Turbofan" << std::endl;
  }

  return code;
}


Handle<Code> Pipeline::GenerateCodeForTesting(CompilationInfo* info,
                                              Graph* graph,
                                              Schedule* schedule) {
  CallDescriptor* call_descriptor =
      Linkage::ComputeIncoming(info->zone(), info);
  return GenerateCodeForTesting(info, call_descriptor, graph, schedule);
}


Handle<Code> Pipeline::GenerateCodeForTesting(CallDescriptor* call_descriptor,
                                              Graph* graph,
                                              Schedule* schedule) {
  CompilationInfo info(graph->zone()->isolate(), graph->zone());
  return GenerateCodeForTesting(&info, call_descriptor, graph, schedule);
}


Handle<Code> Pipeline::GenerateCodeForTesting(CompilationInfo* info,
                                              CallDescriptor* call_descriptor,
                                              Graph* graph,
                                              Schedule* schedule) {
  CHECK(SupportedBackend());
  ZonePool zone_pool(info->isolate());
  Pipeline pipeline(info);
  PipelineData data(&zone_pool, info);
  pipeline.data_ = &data;
  data.InitializeTorTesting(graph, schedule);
  if (schedule == NULL) {
    // TODO(rossberg): Should this really be untyped?
    pipeline.RunPrintAndVerify("Machine", true);
    pipeline.Run<ComputeSchedulePhase>();
  } else {
    TraceSchedule(schedule);
  }

  Linkage linkage(info->zone(), call_descriptor);
  pipeline.GenerateCode(&linkage);
  Handle<Code> code = data.code();

#if ENABLE_DISASSEMBLER
  if (!code.is_null() && FLAG_print_opt_code) {
    CodeTracer::Scope tracing_scope(info->isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    code->Disassemble("test code", os);
  }
#endif
  return code;
}


bool Pipeline::AllocateRegistersForTesting(const RegisterConfiguration* config,
                                           InstructionSequence* sequence,
                                           bool run_verifier) {
  CompilationInfo info(sequence->zone()->isolate(), sequence->zone());
  ZonePool zone_pool(sequence->zone()->isolate());
  PipelineData data(&zone_pool, &info);
  data.InitializeTorTesting(sequence);
  Pipeline pipeline(&info);
  pipeline.data_ = &data;
  pipeline.AllocateRegisters(config, run_verifier);
  return !data.compilation_failed();
}


void Pipeline::GenerateCode(Linkage* linkage) {
  PipelineData* data = this->data_;

  DCHECK_NOT_NULL(linkage);
  DCHECK_NOT_NULL(data->graph());
  DCHECK_NOT_NULL(data->schedule());
  CHECK(SupportedBackend());

  BasicBlockProfiler::Data* profiler_data = NULL;
  if (FLAG_turbo_profiling) {
    profiler_data = BasicBlockInstrumentor::Instrument(info(), data->graph(),
                                                       data->schedule());
  }

  data->InitializeInstructionSequence();

  // Select and schedule instructions covering the scheduled graph.
  Run<InstructionSelectionPhase>(linkage);

  if (FLAG_trace_turbo && !data->MayHaveUnverifiableGraph()) {
    TurboCfgFile tcf(isolate());
    tcf << AsC1V("CodeGen", data->schedule(), data->source_positions(),
                 data->sequence());
  }

  data->DeleteGraphZone();

  BeginPhaseKind("register allocation");

  bool run_verifier = false;
#ifdef DEBUG
  run_verifier = true;
#endif
  // Allocate registers.
  AllocateRegisters(RegisterConfiguration::ArchDefault(), run_verifier);
  if (data->compilation_failed()) {
    info()->AbortOptimization(kNotEnoughVirtualRegistersRegalloc);
    return;
  }

  BeginPhaseKind("code generation");

  // Optimimize jumps.
  if (FLAG_turbo_jt) {
    Run<JumpThreadingPhase>();
  }

  // Generate final machine code.
  Run<GenerateCodePhase>(linkage);

  if (profiler_data != NULL) {
#if ENABLE_DISASSEMBLER
    std::ostringstream os;
    data->code()->Disassemble(NULL, os);
    profiler_data->SetCode(&os);
#endif
  }
}


void Pipeline::AllocateRegisters(const RegisterConfiguration* config,
                                 bool run_verifier) {
  PipelineData* data = this->data_;

  int node_count = data->sequence()->VirtualRegisterCount();
  if (node_count > UnallocatedOperand::kMaxVirtualRegisters) {
    data->set_compilation_failed();
    return;
  }

  // Don't track usage for this zone in compiler stats.
  SmartPointer<Zone> verifier_zone;
  RegisterAllocatorVerifier* verifier = nullptr;
  if (run_verifier) {
    verifier_zone.Reset(new Zone(info()->isolate()));
    verifier = new (verifier_zone.get()) RegisterAllocatorVerifier(
        verifier_zone.get(), config, data->sequence());
  }

  SmartArrayPointer<char> debug_name;
#ifdef DEBUG
  debug_name = GetDebugName(data->info());
#endif

  ZonePool::Scope zone_scope(data->zone_pool());
  data->InitializeRegisterAllocator(zone_scope.zone(), config,
                                    debug_name.get());

  Run<MeetRegisterConstraintsPhase>();
  Run<ResolvePhisPhase>();
  Run<BuildLiveRangesPhase>();
  if (FLAG_trace_turbo_graph) {
    OFStream os(stdout);
    PrintableInstructionSequence printable = {config, data->sequence()};
    os << "----- Instruction sequence before register allocation -----\n"
       << printable;
  }
  if (verifier != nullptr) {
    CHECK(!data->register_allocator()->ExistsUseWithoutDefinition());
  }
  Run<AllocateGeneralRegistersPhase>();
  if (!data->register_allocator()->AllocationOk()) {
    data->set_compilation_failed();
    return;
  }
  Run<AllocateDoubleRegistersPhase>();
  if (!data->register_allocator()->AllocationOk()) {
    data->set_compilation_failed();
    return;
  }
  if (FLAG_turbo_reuse_spill_slots) {
    Run<ReuseSpillSlotsPhase>();
  }
  Run<CommitAssignmentPhase>();
  Run<PopulatePointerMapsPhase>();
  Run<ConnectRangesPhase>();
  Run<ResolveControlFlowPhase>();
  if (FLAG_turbo_move_optimization) {
    Run<OptimizeMovesPhase>();
  }

  if (FLAG_trace_turbo_graph) {
    OFStream os(stdout);
    PrintableInstructionSequence printable = {config, data->sequence()};
    os << "----- Instruction sequence after register allocation -----\n"
       << printable;
  }

  if (verifier != nullptr) {
    verifier->VerifyAssignment();
    verifier->VerifyGapMoves();
  }

  if (FLAG_trace_turbo && !data->MayHaveUnverifiableGraph()) {
    TurboCfgFile tcf(data->isolate());
    tcf << AsC1VAllocator("CodeGen", data->register_allocator());
  }
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

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
#include "src/compiler/control-flow-optimizer.h"
#include "src/compiler/control-reducer.h"
#include "src/compiler/graph-replay.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/instruction.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/js-builtin-reducer.h"
#include "src/compiler/js-context-specialization.h"
#include "src/compiler/js-generic-lowering.h"
#include "src/compiler/js-inlining.h"
#include "src/compiler/js-intrinsic-lowering.h"
#include "src/compiler/js-type-feedback.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/jump-threading.h"
#include "src/compiler/load-elimination.h"
#include "src/compiler/loop-analysis.h"
#include "src/compiler/loop-peeling.h"
#include "src/compiler/machine-operator-reducer.h"
#include "src/compiler/move-optimizer.h"
#include "src/compiler/osr.h"
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
#include "src/type-info.h"
#include "src/utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class PipelineData {
 public:
  // For main entry point.
  PipelineData(ZonePool* zone_pool, CompilationInfo* info,
               PipelineStatistics* pipeline_statistics)
      : isolate_(info->isolate()),
        info_(info),
        outer_zone_(info_->zone()),
        zone_pool_(zone_pool),
        pipeline_statistics_(pipeline_statistics),
        compilation_failed_(false),
        code_(Handle<Code>::null()),
        graph_zone_scope_(zone_pool_),
        graph_zone_(graph_zone_scope_.zone()),
        graph_(nullptr),
        loop_assignment_(nullptr),
        machine_(nullptr),
        common_(nullptr),
        javascript_(nullptr),
        jsgraph_(nullptr),
        js_type_feedback_(nullptr),
        typer_(nullptr),
        schedule_(nullptr),
        instruction_zone_scope_(zone_pool_),
        instruction_zone_(instruction_zone_scope_.zone()),
        sequence_(nullptr),
        frame_(nullptr),
        register_allocator_(nullptr) {
    PhaseScope scope(pipeline_statistics, "init pipeline data");
    graph_ = new (graph_zone_) Graph(graph_zone_);
    source_positions_.Reset(new SourcePositionTable(graph_));
    machine_ = new (graph_zone_) MachineOperatorBuilder(
        graph_zone_, kMachPtr,
        InstructionSelector::SupportedMachineOperatorFlags());
    common_ = new (graph_zone_) CommonOperatorBuilder(graph_zone_);
    javascript_ = new (graph_zone_) JSOperatorBuilder(graph_zone_);
    jsgraph_ = new (graph_zone_)
        JSGraph(isolate_, graph_, common_, javascript_, machine_);
    typer_.Reset(new Typer(isolate_, graph_, info_->context()));
  }

  // For machine graph testing entry point.
  PipelineData(ZonePool* zone_pool, CompilationInfo* info, Graph* graph,
               Schedule* schedule)
      : isolate_(info->isolate()),
        info_(info),
        outer_zone_(nullptr),
        zone_pool_(zone_pool),
        pipeline_statistics_(nullptr),
        compilation_failed_(false),
        code_(Handle<Code>::null()),
        graph_zone_scope_(zone_pool_),
        graph_zone_(nullptr),
        graph_(graph),
        source_positions_(new SourcePositionTable(graph_)),
        loop_assignment_(nullptr),
        machine_(nullptr),
        common_(nullptr),
        javascript_(nullptr),
        jsgraph_(nullptr),
        js_type_feedback_(nullptr),
        typer_(nullptr),
        schedule_(schedule),
        instruction_zone_scope_(zone_pool_),
        instruction_zone_(instruction_zone_scope_.zone()),
        sequence_(nullptr),
        frame_(nullptr),
        register_allocator_(nullptr) {}

  // For register allocation testing entry point.
  PipelineData(ZonePool* zone_pool, CompilationInfo* info,
               InstructionSequence* sequence)
      : isolate_(info->isolate()),
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
        js_type_feedback_(nullptr),
        typer_(nullptr),
        schedule_(nullptr),
        instruction_zone_scope_(zone_pool_),
        instruction_zone_(sequence->zone()),
        sequence_(sequence),
        frame_(nullptr),
        register_allocator_(nullptr) {}

  ~PipelineData() {
    DeleteInstructionZone();
    DeleteGraphZone();
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
  JSTypeFeedbackTable* js_type_feedback() { return js_type_feedback_; }
  void set_js_type_feedback(JSTypeFeedbackTable* js_type_feedback) {
    js_type_feedback_ = js_type_feedback;
  }
  Typer* typer() const { return typer_.get(); }

  LoopAssignmentAnalysis* loop_assignment() const { return loop_assignment_; }
  void set_loop_assignment(LoopAssignmentAnalysis* loop_assignment) {
    DCHECK(!loop_assignment_);
    loop_assignment_ = loop_assignment;
  }

  Schedule* schedule() const { return schedule_; }
  void set_schedule(Schedule* schedule) {
    DCHECK(!schedule_);
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
    js_type_feedback_ = nullptr;
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
    DCHECK(!sequence_);
    InstructionBlocks* instruction_blocks =
        InstructionSequence::InstructionBlocksFor(instruction_zone(),
                                                  schedule());
    sequence_ = new (instruction_zone()) InstructionSequence(
        info()->isolate(), instruction_zone(), instruction_blocks);
  }

  void InitializeRegisterAllocator(Zone* local_zone,
                                   const RegisterConfiguration* config,
                                   const char* debug_name) {
    DCHECK(!register_allocator_);
    DCHECK(!frame_);
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
  JSTypeFeedbackTable* js_type_feedback_;
  // TODO(dcarney): make this into a ZoneObject.
  SmartPointer<Typer> typer_;
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
  if (info->code_stub() != NULL) {
    CodeStub::Major major_key = info->code_stub()->MajorKey();
    const char* major_name = CodeStub::MajorName(major_key, false);
    size_t len = strlen(major_name) + 1;
    SmartArrayPointer<char> name(new char[len]);
    memcpy(name.get(), major_name, len);
    return name;
  } else {
    AllowHandleDereference allow_deref;
    return info->function()->debug_name()->ToCString();
  }
}


class AstGraphBuilderWithPositions : public AstGraphBuilder {
 public:
  AstGraphBuilderWithPositions(Zone* local_zone, CompilationInfo* info,
                               JSGraph* jsgraph,
                               LoopAssignmentAnalysis* loop_assignment,
                               JSTypeFeedbackTable* js_type_feedback,
                               SourcePositionTable* source_positions)
      : AstGraphBuilder(local_zone, info, jsgraph, loop_assignment,
                        js_type_feedback),
        source_positions_(source_positions),
        start_position_(info->shared_info()->start_position()) {}

  bool CreateGraph(bool constant_context, bool stack_check) {
    SourcePositionTable::Scope pos_scope(source_positions_, start_position_);
    return AstGraphBuilder::CreateGraph(constant_context, stack_check);
  }

#define DEF_VISIT(type)                                               \
  void Visit##type(type* node) OVERRIDE {                             \
    SourcePositionTable::Scope pos(source_positions_,                 \
                                   SourcePosition(node->position())); \
    AstGraphBuilder::Visit##type(node);                               \
  }
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

 private:
  SourcePositionTable* source_positions_;
  SourcePosition start_position_;
};


namespace {

class SourcePositionWrapper : public Reducer {
 public:
  SourcePositionWrapper(Reducer* reducer, SourcePositionTable* table)
      : reducer_(reducer), table_(table) {}
  virtual ~SourcePositionWrapper() {}

  virtual Reduction Reduce(Node* node) {
    SourcePosition pos = table_->GetSourcePosition(node);
    SourcePositionTable::Scope position(table_, pos);
    return reducer_->Reduce(node);
  }

 private:
  Reducer* reducer_;
  SourcePositionTable* table_;

  DISALLOW_COPY_AND_ASSIGN(SourcePositionWrapper);
};


static void AddReducer(PipelineData* data, GraphReducer* graph_reducer,
                       Reducer* reducer) {
  if (FLAG_turbo_source_positions) {
    void* buffer = data->graph_zone()->New(sizeof(SourcePositionWrapper));
    SourcePositionWrapper* wrapper =
        new (buffer) SourcePositionWrapper(reducer, data->source_positions());
    graph_reducer->AddReducer(wrapper);
  } else {
    graph_reducer->AddReducer(reducer);
  }
}
}  // namespace

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

  void Run(PipelineData* data, Zone* temp_zone, bool constant_context) {
    AstGraphBuilderWithPositions graph_builder(
        temp_zone, data->info(), data->jsgraph(), data->loop_assignment(),
        data->js_type_feedback(), data->source_positions());
    bool stack_check = !data->info()->IsStub();
    if (!graph_builder.CreateGraph(constant_context, stack_check)) {
      data->set_compilation_failed();
    }
  }
};


struct ContextSpecializerPhase {
  static const char* phase_name() { return "context specializing"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    JSContextSpecializer spec(data->jsgraph());
    GraphReducer graph_reducer(data->graph(), temp_zone);
    AddReducer(data, &graph_reducer, &spec);
    graph_reducer.ReduceGraph();
  }
};


struct InliningPhase {
  static const char* phase_name() { return "inlining"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    JSInliner inliner(data->info()->is_inlining_enabled()
                          ? JSInliner::kGeneralInlining
                          : JSInliner::kBuiltinsInlining,
                      temp_zone, data->info(), data->jsgraph());
    GraphReducer graph_reducer(data->graph(), temp_zone);
    AddReducer(data, &graph_reducer, &inliner);
    graph_reducer.ReduceGraph();
  }
};


struct TyperPhase {
  static const char* phase_name() { return "typer"; }

  void Run(PipelineData* data, Zone* temp_zone) { data->typer()->Run(); }
};


struct OsrDeconstructionPhase {
  static const char* phase_name() { return "OSR deconstruction"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    OsrHelper osr_helper(data->info());
    bool success =
        osr_helper.Deconstruct(data->jsgraph(), data->common(), temp_zone);
    if (!success) data->info()->RetryOptimization(kOsrCompileFailed);
  }
};


struct JSTypeFeedbackPhase {
  static const char* phase_name() { return "type feedback specializing"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    Handle<Context> native_context(data->info()->context()->native_context());
    TypeFeedbackOracle oracle(data->isolate(), temp_zone,
                              data->info()->unoptimized_code(),
                              data->info()->feedback_vector(), native_context);
    GraphReducer graph_reducer(data->graph(), temp_zone);
    JSTypeFeedbackSpecializer specializer(data->jsgraph(),
                                          data->js_type_feedback(), &oracle);
    AddReducer(data, &graph_reducer, &specializer);
    graph_reducer.ReduceGraph();
  }
};


struct TypedLoweringPhase {
  static const char* phase_name() { return "typed lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    LoadElimination load_elimination;
    JSBuiltinReducer builtin_reducer(data->jsgraph());
    JSTypedLowering typed_lowering(data->jsgraph(), temp_zone);
    JSIntrinsicLowering intrinsic_lowering(data->jsgraph());
    SimplifiedOperatorReducer simple_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer(data->jsgraph());
    GraphReducer graph_reducer(data->graph(), temp_zone);
    AddReducer(data, &graph_reducer, &builtin_reducer);
    AddReducer(data, &graph_reducer, &typed_lowering);
    AddReducer(data, &graph_reducer, &intrinsic_lowering);
    AddReducer(data, &graph_reducer, &load_elimination);
    AddReducer(data, &graph_reducer, &simple_reducer);
    AddReducer(data, &graph_reducer, &common_reducer);
    graph_reducer.ReduceGraph();
  }
};


struct SimplifiedLoweringPhase {
  static const char* phase_name() { return "simplified lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    SimplifiedLowering lowering(data->jsgraph(), temp_zone,
                                data->source_positions());
    lowering.LowerAllNodes();
    ValueNumberingReducer vn_reducer(temp_zone);
    SimplifiedOperatorReducer simple_reducer(data->jsgraph());
    MachineOperatorReducer machine_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer(data->jsgraph());
    GraphReducer graph_reducer(data->graph(), temp_zone);
    AddReducer(data, &graph_reducer, &vn_reducer);
    AddReducer(data, &graph_reducer, &simple_reducer);
    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &common_reducer);
    graph_reducer.ReduceGraph();
  }
};


struct ControlFlowOptimizationPhase {
  static const char* phase_name() { return "control flow optimization"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    ControlFlowOptimizer optimizer(data->jsgraph(), temp_zone);
    optimizer.Optimize();
  }
};


struct ChangeLoweringPhase {
  static const char* phase_name() { return "change lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    ValueNumberingReducer vn_reducer(temp_zone);
    SimplifiedOperatorReducer simple_reducer(data->jsgraph());
    ChangeLowering lowering(data->jsgraph());
    MachineOperatorReducer machine_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer(data->jsgraph());
    GraphReducer graph_reducer(data->graph(), temp_zone);
    AddReducer(data, &graph_reducer, &vn_reducer);
    AddReducer(data, &graph_reducer, &simple_reducer);
    AddReducer(data, &graph_reducer, &lowering);
    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &common_reducer);
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


struct StressLoopPeelingPhase {
  static const char* phase_name() { return "stress loop peeling"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    // Peel the first outer loop for testing.
    // TODO(titzer): peel all loops? the N'th loop? Innermost loops?
    LoopTree* loop_tree = LoopFinder::BuildLoopTree(data->graph(), temp_zone);
    if (loop_tree != NULL && loop_tree->outer_loops().size() > 0) {
      LoopPeeler::Peel(data->graph(), data->common(), loop_tree,
                       loop_tree->outer_loops()[0], temp_zone);
    }
  }
};


struct GenericLoweringPhase {
  static const char* phase_name() { return "generic lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SourcePositionTable::Scope pos(data->source_positions(),
                                   SourcePosition::Unknown());
    JSGenericLowering generic(data->info()->is_typing_enabled(),
                              data->jsgraph());
    SelectLowering select(data->jsgraph()->graph(), data->jsgraph()->common());
    GraphReducer graph_reducer(data->graph(), temp_zone);
    AddReducer(data, &graph_reducer, &generic);
    AddReducer(data, &graph_reducer, &select);
    graph_reducer.ReduceGraph();
  }
};


struct ComputeSchedulePhase {
  static const char* phase_name() { return "scheduling"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    Schedule* schedule = Scheduler::ComputeSchedule(
        temp_zone, data->graph(), data->info()->is_splitting_enabled()
                                      ? Scheduler::kSplitNodes
                                      : Scheduler::kNoFlags);
    if (FLAG_turbo_verify) ScheduleVerifier::Run(schedule);
    data->set_schedule(schedule);
  }
};


struct InstructionSelectionPhase {
  static const char* phase_name() { return "select instructions"; }

  void Run(PipelineData* data, Zone* temp_zone, Linkage* linkage) {
    InstructionSelector selector(temp_zone, data->graph()->NodeCount(), linkage,
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


struct AssignSpillSlotsPhase {
  static const char* phase_name() { return "assign spill slots"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->register_allocator()->AssignSpillSlots();
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
    ZoneVector<RpoNumber> result(temp_zone);
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

    {  // Print dot.
      FILE* dot_file = OpenVisualizerLogFile(info, phase, "dot", "w+");
      if (dot_file == nullptr) return;
      OFStream dot_of(dot_file);
      dot_of << AsDOT(*graph);
      fclose(dot_file);
    }

    {  // Print JSON.
      FILE* json_file = OpenVisualizerLogFile(info, NULL, "json", "a+");
      if (json_file == nullptr) return;
      OFStream json_of(json_file);
      json_of << "{\"name\":\"" << phase << "\",\"type\":\"graph\",\"data\":"
              << AsJSON(*graph, data->source_positions()) << "},\n";
      fclose(json_file);
    }

    if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
      OFStream os(stdout);
      os << "-- Graph after " << phase << " -- " << std::endl;
      os << AsRPO(*graph);
    }
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
  if (FLAG_turbo_verify) {
    Run<VerifyGraphPhase>(untyped);
  }
}


Handle<Code> Pipeline::GenerateCode() {
  if (info()->is_osr() && !FLAG_turbo_osr) {
    // TODO(turbofan): remove this flag and always handle OSR
    info()->RetryOptimization(kOsrCompileFailed);
    return Handle<Code>::null();
  }

  // TODO(mstarzinger): This is just a temporary hack to make TurboFan work,
  // the correct solution is to restore the context register after invoking
  // builtins from full-codegen.
  Handle<SharedFunctionInfo> shared = info()->shared_info();
  for (int i = 0; i < Builtins::NumberOfJavaScriptBuiltins(); i++) {
    Builtins::JavaScript id = static_cast<Builtins::JavaScript>(i);
    Object* builtin = isolate()->js_builtins_object()->javascript_builtin(id);
    if (*info()->closure() == builtin) return Handle<Code>::null();
  }

  // TODO(dslomov): support turbo optimization of subclass constructors.
  if (IsSubclassConstructor(shared->kind())) {
    shared->DisableOptimization(kSuperReference);
    return Handle<Code>::null();
  }

  ZonePool zone_pool;
  SmartPointer<PipelineStatistics> pipeline_statistics;

  if (FLAG_turbo_stats) {
    pipeline_statistics.Reset(new PipelineStatistics(info(), &zone_pool));
    pipeline_statistics->BeginPhaseKind("initializing");
  }

  if (FLAG_trace_turbo) {
    FILE* json_file = OpenVisualizerLogFile(info(), NULL, "json", "w+");
    if (json_file != nullptr) {
      OFStream json_of(json_file);
      Handle<Script> script = info()->script();
      FunctionLiteral* function = info()->function();
      SmartArrayPointer<char> function_name =
          info()->shared_info()->DebugName()->ToCString();
      int pos = info()->shared_info()->start_position();
      json_of << "{\"function\":\"" << function_name.get()
              << "\", \"sourcePosition\":" << pos << ", \"source\":\"";
      if (!script->IsUndefined() && !script->source()->IsUndefined()) {
        DisallowHeapAllocation no_allocation;
        int start = function->start_position();
        int len = function->end_position() - start;
        String::SubStringRange source(String::cast(script->source()), start,
                                      len);
        for (const auto& c : source) {
          json_of << AsEscapedUC16ForJSON(c);
        }
      }
      json_of << "\",\n\"phases\":[";
      fclose(json_file);
    }
  }

  PipelineData data(&zone_pool, info(), pipeline_statistics.get());
  this->data_ = &data;

  if (info()->is_type_feedback_enabled()) {
    data.set_js_type_feedback(new (data.graph_zone())
                                  JSTypeFeedbackTable(data.graph_zone()));
  }

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

  Run<GraphBuilderPhase>(info()->is_context_specializing());
  if (data.compilation_failed()) return Handle<Code>::null();
  RunPrintAndVerify("Initial untyped", true);

  Run<EarlyControlReductionPhase>();
  RunPrintAndVerify("Early Control reduced", true);

  if (info()->is_context_specializing()) {
    // Specialize the code to the context as aggressively as possible.
    Run<ContextSpecializerPhase>();
    RunPrintAndVerify("Context specialized", true);
  }

  if (info()->is_builtin_inlining_enabled() || info()->is_inlining_enabled()) {
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

    if (FLAG_turbo_stress_loop_peeling) {
      Run<StressLoopPeelingPhase>();
      RunPrintAndVerify("Loop peeled", true);
    }

    if (info()->is_osr()) {
      Run<OsrDeconstructionPhase>();
      if (info()->bailout_reason() != kNoReason) return Handle<Code>::null();
      RunPrintAndVerify("OSR deconstruction");
    }

    if (info()->is_type_feedback_enabled()) {
      Run<JSTypeFeedbackPhase>();
      RunPrintAndVerify("JSType feedback");
    }

    // Lower simplified operators and insert changes.
    Run<SimplifiedLoweringPhase>();
    RunPrintAndVerify("Lowered simplified");

    // Optimize control flow.
    if (FLAG_turbo_cf_optimization) {
      Run<ControlFlowOptimizationPhase>();
      RunPrintAndVerify("Control flow optimized");
    }

    // Lower changes that have been inserted before.
    Run<ChangeLoweringPhase>();
    // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
    RunPrintAndVerify("Lowered changes", true);

    Run<LateControlReductionPhase>();
    RunPrintAndVerify("Late Control reduced");
  } else {
    if (info()->is_osr()) {
      Run<OsrDeconstructionPhase>();
      if (info()->bailout_reason() != kNoReason) return Handle<Code>::null();
      RunPrintAndVerify("OSR deconstruction");
    }
  }

  // Lower any remaining generic JSOperators.
  Run<GenericLoweringPhase>();
  // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
  RunPrintAndVerify("Lowered generic", true);

  BeginPhaseKind("block building");

  data.source_positions()->RemoveDecorator();

  return ScheduleAndGenerateCode(
      Linkage::ComputeIncoming(data.instruction_zone(), info()));
}


Handle<Code> Pipeline::GenerateCodeForTesting(CompilationInfo* info,
                                              Graph* graph,
                                              Schedule* schedule) {
  CallDescriptor* call_descriptor =
      Linkage::ComputeIncoming(info->zone(), info);
  return GenerateCodeForTesting(info, call_descriptor, graph, schedule);
}


Handle<Code> Pipeline::GenerateCodeForTesting(Isolate* isolate,
                                              CallDescriptor* call_descriptor,
                                              Graph* graph,
                                              Schedule* schedule) {
  FakeStubForTesting stub(isolate);
  CompilationInfo info(&stub, isolate, graph->zone());
  return GenerateCodeForTesting(&info, call_descriptor, graph, schedule);
}


Handle<Code> Pipeline::GenerateCodeForTesting(CompilationInfo* info,
                                              CallDescriptor* call_descriptor,
                                              Graph* graph,
                                              Schedule* schedule) {
  // Construct a pipeline for scheduling and code generation.
  ZonePool zone_pool;
  PipelineData data(&zone_pool, info, graph, schedule);
  Pipeline pipeline(info);
  pipeline.data_ = &data;
  if (data.schedule() == nullptr) {
    // TODO(rossberg): Should this really be untyped?
    pipeline.RunPrintAndVerify("Machine", true);
  }

  return pipeline.ScheduleAndGenerateCode(call_descriptor);
}


bool Pipeline::AllocateRegistersForTesting(const RegisterConfiguration* config,
                                           InstructionSequence* sequence,
                                           bool run_verifier) {
  FakeStubForTesting stub(sequence->isolate());
  CompilationInfo info(&stub, sequence->isolate(), sequence->zone());
  ZonePool zone_pool;
  PipelineData data(&zone_pool, &info, sequence);
  Pipeline pipeline(&info);
  pipeline.data_ = &data;
  pipeline.AllocateRegisters(config, run_verifier);
  return !data.compilation_failed();
}


Handle<Code> Pipeline::ScheduleAndGenerateCode(
    CallDescriptor* call_descriptor) {
  PipelineData* data = this->data_;

  DCHECK_NOT_NULL(data->graph());
  CHECK(SupportedBackend());

  if (data->schedule() == nullptr) Run<ComputeSchedulePhase>();
  TraceSchedule(data->schedule());

  BasicBlockProfiler::Data* profiler_data = NULL;
  if (FLAG_turbo_profiling) {
    profiler_data = BasicBlockInstrumentor::Instrument(info(), data->graph(),
                                                       data->schedule());
  }

  data->InitializeInstructionSequence();

  // Select and schedule instructions covering the scheduled graph.
  Linkage linkage(call_descriptor);
  Run<InstructionSelectionPhase>(&linkage);

  if (FLAG_trace_turbo && !data->MayHaveUnverifiableGraph()) {
    TurboCfgFile tcf(isolate());
    tcf << AsC1V("CodeGen", data->schedule(), data->source_positions(),
                 data->sequence());
  }

  data->DeleteGraphZone();

  BeginPhaseKind("register allocation");

  bool run_verifier = FLAG_turbo_verify_allocation;
  // Allocate registers.
  AllocateRegisters(RegisterConfiguration::ArchDefault(), run_verifier);
  if (data->compilation_failed()) {
    info()->AbortOptimization(kNotEnoughVirtualRegistersRegalloc);
    return Handle<Code>();
  }

  BeginPhaseKind("code generation");

  // Optimimize jumps.
  if (FLAG_turbo_jt) {
    Run<JumpThreadingPhase>();
  }

  // Generate final machine code.
  Run<GenerateCodePhase>(&linkage);

  Handle<Code> code = data->code();
  if (profiler_data != NULL) {
#if ENABLE_DISASSEMBLER
    std::ostringstream os;
    code->Disassemble(NULL, os);
    profiler_data->SetCode(&os);
#endif
  }

  info()->SetCode(code);
  v8::internal::CodeGenerator::PrintCode(code, info());

  if (FLAG_trace_turbo) {
    FILE* json_file = OpenVisualizerLogFile(info(), NULL, "json", "a+");
    if (json_file != nullptr) {
      OFStream json_of(json_file);
      json_of
          << "{\"name\":\"disassembly\",\"type\":\"disassembly\",\"data\":\"";
#if ENABLE_DISASSEMBLER
      std::stringstream disassembly_stream;
      code->Disassemble(NULL, disassembly_stream);
      std::string disassembly_string(disassembly_stream.str());
      for (const auto& c : disassembly_string) {
        json_of << AsEscapedUC16ForJSON(c);
      }
#endif  // ENABLE_DISASSEMBLER
      json_of << "\"}\n]}";
      fclose(json_file);
    }
    OFStream os(stdout);
    os << "---------------------------------------------------\n"
       << "Finished compiling method " << GetDebugName(info()).get()
       << " using Turbofan" << std::endl;
  }

  return code;
}


void Pipeline::AllocateRegisters(const RegisterConfiguration* config,
                                 bool run_verifier) {
  PipelineData* data = this->data_;

  // Don't track usage for this zone in compiler stats.
  SmartPointer<Zone> verifier_zone;
  RegisterAllocatorVerifier* verifier = nullptr;
  if (run_verifier) {
    verifier_zone.Reset(new Zone());
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
  if (info()->is_osr()) {
    OsrHelper osr_helper(info());
    osr_helper.SetupFrame(data->frame());
  }

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
  Run<AllocateDoubleRegistersPhase>();
  Run<AssignSpillSlotsPhase>();

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

}  // namespace compiler
}  // namespace internal
}  // namespace v8

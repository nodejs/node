// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/pipeline.h"

#include <fstream>  // NOLINT(readability/streams)
#include <memory>
#include <sstream>

#include "src/base/adapters.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/compiler/ast-graph-builder.h"
#include "src/compiler/ast-loop-assignment-analyzer.h"
#include "src/compiler/basic-block-instrumentor.h"
#include "src/compiler/branch-elimination.h"
#include "src/compiler/bytecode-graph-builder.h"
#include "src/compiler/checkpoint-elimination.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/common-operator-reducer.h"
#include "src/compiler/control-flow-optimizer.h"
#include "src/compiler/dead-code-elimination.h"
#include "src/compiler/effect-control-linearizer.h"
#include "src/compiler/escape-analysis-reducer.h"
#include "src/compiler/escape-analysis.h"
#include "src/compiler/frame-elider.h"
#include "src/compiler/graph-replay.h"
#include "src/compiler/graph-trimmer.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/instruction.h"
#include "src/compiler/js-builtin-reducer.h"
#include "src/compiler/js-call-reducer.h"
#include "src/compiler/js-context-specialization.h"
#include "src/compiler/js-create-lowering.h"
#include "src/compiler/js-frame-specialization.h"
#include "src/compiler/js-generic-lowering.h"
#include "src/compiler/js-global-object-specialization.h"
#include "src/compiler/js-inlining-heuristic.h"
#include "src/compiler/js-intrinsic-lowering.h"
#include "src/compiler/js-native-context-specialization.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/jump-threading.h"
#include "src/compiler/live-range-separator.h"
#include "src/compiler/load-elimination.h"
#include "src/compiler/loop-analysis.h"
#include "src/compiler/loop-peeling.h"
#include "src/compiler/loop-variable-optimizer.h"
#include "src/compiler/machine-graph-verifier.h"
#include "src/compiler/machine-operator-reducer.h"
#include "src/compiler/memory-optimizer.h"
#include "src/compiler/move-optimizer.h"
#include "src/compiler/osr.h"
#include "src/compiler/pipeline-statistics.h"
#include "src/compiler/redundancy-elimination.h"
#include "src/compiler/register-allocator-verifier.h"
#include "src/compiler/register-allocator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "src/compiler/select-lowering.h"
#include "src/compiler/simplified-lowering.h"
#include "src/compiler/simplified-operator-reducer.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/store-store-elimination.h"
#include "src/compiler/tail-call-optimization.h"
#include "src/compiler/type-hint-analyzer.h"
#include "src/compiler/typed-optimization.h"
#include "src/compiler/typer.h"
#include "src/compiler/value-numbering-reducer.h"
#include "src/compiler/verifier.h"
#include "src/compiler/zone-pool.h"
#include "src/isolate-inl.h"
#include "src/ostreams.h"
#include "src/parsing/parse-info.h"
#include "src/register-configuration.h"
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
        debug_name_(info_->GetDebugName()),
        outer_zone_(info_->zone()),
        zone_pool_(zone_pool),
        pipeline_statistics_(pipeline_statistics),
        graph_zone_scope_(zone_pool_),
        graph_zone_(graph_zone_scope_.zone()),
        instruction_zone_scope_(zone_pool_),
        instruction_zone_(instruction_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_pool_),
        register_allocation_zone_(register_allocation_zone_scope_.zone()) {
    PhaseScope scope(pipeline_statistics, "init pipeline data");
    graph_ = new (graph_zone_) Graph(graph_zone_);
    source_positions_ = new (graph_zone_) SourcePositionTable(graph_);
    simplified_ = new (graph_zone_) SimplifiedOperatorBuilder(graph_zone_);
    machine_ = new (graph_zone_) MachineOperatorBuilder(
        graph_zone_, MachineType::PointerRepresentation(),
        InstructionSelector::SupportedMachineOperatorFlags(),
        InstructionSelector::AlignmentRequirements());
    common_ = new (graph_zone_) CommonOperatorBuilder(graph_zone_);
    javascript_ = new (graph_zone_) JSOperatorBuilder(graph_zone_);
    jsgraph_ = new (graph_zone_)
        JSGraph(isolate_, graph_, common_, javascript_, simplified_, machine_);
  }

  // For WASM compile entry point.
  PipelineData(ZonePool* zone_pool, CompilationInfo* info, Graph* graph,
               SourcePositionTable* source_positions)
      : isolate_(info->isolate()),
        info_(info),
        debug_name_(info_->GetDebugName()),
        zone_pool_(zone_pool),
        graph_zone_scope_(zone_pool_),
        graph_(graph),
        source_positions_(source_positions),
        instruction_zone_scope_(zone_pool_),
        instruction_zone_(instruction_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_pool_),
        register_allocation_zone_(register_allocation_zone_scope_.zone()) {}

  // For machine graph testing entry point.
  PipelineData(ZonePool* zone_pool, CompilationInfo* info, Graph* graph,
               Schedule* schedule)
      : isolate_(info->isolate()),
        info_(info),
        debug_name_(info_->GetDebugName()),
        zone_pool_(zone_pool),
        graph_zone_scope_(zone_pool_),
        graph_(graph),
        source_positions_(new (info->zone()) SourcePositionTable(graph_)),
        schedule_(schedule),
        instruction_zone_scope_(zone_pool_),
        instruction_zone_(instruction_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_pool_),
        register_allocation_zone_(register_allocation_zone_scope_.zone()) {}

  // For register allocation testing entry point.
  PipelineData(ZonePool* zone_pool, CompilationInfo* info,
               InstructionSequence* sequence)
      : isolate_(info->isolate()),
        info_(info),
        debug_name_(info_->GetDebugName()),
        zone_pool_(zone_pool),
        graph_zone_scope_(zone_pool_),
        instruction_zone_scope_(zone_pool_),
        instruction_zone_(sequence->zone()),
        sequence_(sequence),
        register_allocation_zone_scope_(zone_pool_),
        register_allocation_zone_(register_allocation_zone_scope_.zone()) {}

  ~PipelineData() {
    DeleteRegisterAllocationZone();
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
  SourcePositionTable* source_positions() const { return source_positions_; }
  MachineOperatorBuilder* machine() const { return machine_; }
  CommonOperatorBuilder* common() const { return common_; }
  JSOperatorBuilder* javascript() const { return javascript_; }
  JSGraph* jsgraph() const { return jsgraph_; }
  MaybeHandle<Context> native_context() const {
    if (info()->is_native_context_specializing()) {
      return handle(info()->native_context(), isolate());
    }
    return MaybeHandle<Context>();
  }

  LoopAssignmentAnalysis* loop_assignment() const { return loop_assignment_; }
  void set_loop_assignment(LoopAssignmentAnalysis* loop_assignment) {
    DCHECK(!loop_assignment_);
    loop_assignment_ = loop_assignment;
  }

  TypeHintAnalysis* type_hint_analysis() const { return type_hint_analysis_; }
  void set_type_hint_analysis(TypeHintAnalysis* type_hint_analysis) {
    DCHECK_NULL(type_hint_analysis_);
    type_hint_analysis_ = type_hint_analysis;
  }

  Schedule* schedule() const { return schedule_; }
  void set_schedule(Schedule* schedule) {
    DCHECK(!schedule_);
    schedule_ = schedule;
  }
  void reset_schedule() { schedule_ = nullptr; }

  Zone* instruction_zone() const { return instruction_zone_; }
  InstructionSequence* sequence() const { return sequence_; }
  Frame* frame() const { return frame_; }

  Zone* register_allocation_zone() const { return register_allocation_zone_; }
  RegisterAllocationData* register_allocation_data() const {
    return register_allocation_data_;
  }

  BasicBlockProfiler::Data* profiler_data() const { return profiler_data_; }
  void set_profiler_data(BasicBlockProfiler::Data* profiler_data) {
    profiler_data_ = profiler_data;
  }

  std::string const& source_position_output() const {
    return source_position_output_;
  }
  void set_source_position_output(std::string const& source_position_output) {
    source_position_output_ = source_position_output;
  }

  void DeleteGraphZone() {
    if (graph_zone_ == nullptr) return;
    graph_zone_scope_.Destroy();
    graph_zone_ = nullptr;
    graph_ = nullptr;
    source_positions_ = nullptr;
    loop_assignment_ = nullptr;
    type_hint_analysis_ = nullptr;
    simplified_ = nullptr;
    machine_ = nullptr;
    common_ = nullptr;
    javascript_ = nullptr;
    jsgraph_ = nullptr;
    schedule_ = nullptr;
  }

  void DeleteInstructionZone() {
    if (instruction_zone_ == nullptr) return;
    instruction_zone_scope_.Destroy();
    instruction_zone_ = nullptr;
    sequence_ = nullptr;
    frame_ = nullptr;
  }

  void DeleteRegisterAllocationZone() {
    if (register_allocation_zone_ == nullptr) return;
    register_allocation_zone_scope_.Destroy();
    register_allocation_zone_ = nullptr;
    register_allocation_data_ = nullptr;
  }

  void InitializeInstructionSequence(const CallDescriptor* descriptor) {
    DCHECK(sequence_ == nullptr);
    InstructionBlocks* instruction_blocks =
        InstructionSequence::InstructionBlocksFor(instruction_zone(),
                                                  schedule());
    sequence_ = new (instruction_zone()) InstructionSequence(
        info()->isolate(), instruction_zone(), instruction_blocks);
    if (descriptor && descriptor->RequiresFrameAsIncoming()) {
      sequence_->instruction_blocks()[0]->mark_needs_frame();
    } else {
      DCHECK_EQ(0, descriptor->CalleeSavedFPRegisters());
      DCHECK_EQ(0, descriptor->CalleeSavedRegisters());
    }
  }

  void InitializeFrameData(CallDescriptor* descriptor) {
    DCHECK(frame_ == nullptr);
    int fixed_frame_size = 0;
    if (descriptor != nullptr) {
      fixed_frame_size = CalculateFixedFrameSize(descriptor);
    }
    frame_ = new (instruction_zone()) Frame(fixed_frame_size);
  }

  void InitializeRegisterAllocationData(const RegisterConfiguration* config,
                                        CallDescriptor* descriptor) {
    DCHECK(register_allocation_data_ == nullptr);
    register_allocation_data_ = new (register_allocation_zone())
        RegisterAllocationData(config, register_allocation_zone(), frame(),
                               sequence(), debug_name_.get());
  }

  void BeginPhaseKind(const char* phase_kind_name) {
    if (pipeline_statistics() != nullptr) {
      pipeline_statistics()->BeginPhaseKind(phase_kind_name);
    }
  }

  void EndPhaseKind() {
    if (pipeline_statistics() != nullptr) {
      pipeline_statistics()->EndPhaseKind();
    }
  }

 private:
  Isolate* const isolate_;
  CompilationInfo* const info_;
  std::unique_ptr<char[]> debug_name_;
  Zone* outer_zone_ = nullptr;
  ZonePool* const zone_pool_;
  PipelineStatistics* pipeline_statistics_ = nullptr;
  bool compilation_failed_ = false;
  Handle<Code> code_ = Handle<Code>::null();

  // All objects in the following group of fields are allocated in graph_zone_.
  // They are all set to nullptr when the graph_zone_ is destroyed.
  ZonePool::Scope graph_zone_scope_;
  Zone* graph_zone_ = nullptr;
  Graph* graph_ = nullptr;
  SourcePositionTable* source_positions_ = nullptr;
  LoopAssignmentAnalysis* loop_assignment_ = nullptr;
  TypeHintAnalysis* type_hint_analysis_ = nullptr;
  SimplifiedOperatorBuilder* simplified_ = nullptr;
  MachineOperatorBuilder* machine_ = nullptr;
  CommonOperatorBuilder* common_ = nullptr;
  JSOperatorBuilder* javascript_ = nullptr;
  JSGraph* jsgraph_ = nullptr;
  Schedule* schedule_ = nullptr;

  // All objects in the following group of fields are allocated in
  // instruction_zone_.  They are all set to nullptr when the instruction_zone_
  // is
  // destroyed.
  ZonePool::Scope instruction_zone_scope_;
  Zone* instruction_zone_;
  InstructionSequence* sequence_ = nullptr;
  Frame* frame_ = nullptr;

  // All objects in the following group of fields are allocated in
  // register_allocation_zone_.  They are all set to nullptr when the zone is
  // destroyed.
  ZonePool::Scope register_allocation_zone_scope_;
  Zone* register_allocation_zone_;
  RegisterAllocationData* register_allocation_data_ = nullptr;

  // Basic block profiling support.
  BasicBlockProfiler::Data* profiler_data_ = nullptr;

  // Source position output for --trace-turbo.
  std::string source_position_output_;

  int CalculateFixedFrameSize(CallDescriptor* descriptor) {
    if (descriptor->IsJSFunctionCall()) {
      return StandardFrameConstants::kFixedSlotCount;
    }
    return descriptor->IsCFunctionCall()
               ? (CommonFrameConstants::kFixedSlotCountAboveFp +
                  CommonFrameConstants::kCPSlotCount)
               : TypedFrameConstants::kFixedSlotCount;
  }

  DISALLOW_COPY_AND_ASSIGN(PipelineData);
};

class PipelineImpl final {
 public:
  explicit PipelineImpl(PipelineData* data) : data_(data) {}

  // Helpers for executing pipeline phases.
  template <typename Phase>
  void Run();
  template <typename Phase, typename Arg0>
  void Run(Arg0 arg_0);
  template <typename Phase, typename Arg0, typename Arg1>
  void Run(Arg0 arg_0, Arg1 arg_1);

  // Run the graph creation and initial optimization passes.
  bool CreateGraph();

  // Run the concurrent optimization passes.
  bool OptimizeGraph(Linkage* linkage);

  // Perform the actual code generation and return handle to a code object.
  Handle<Code> GenerateCode(Linkage* linkage);

  bool ScheduleAndSelectInstructions(Linkage* linkage);
  void RunPrintAndVerify(const char* phase, bool untyped = false);
  Handle<Code> ScheduleAndGenerateCode(CallDescriptor* call_descriptor);
  void AllocateRegisters(const RegisterConfiguration* config,
                         CallDescriptor* descriptor, bool run_verifier);

  CompilationInfo* info() const;
  Isolate* isolate() const;

  PipelineData* const data_;
};

namespace {

struct TurboCfgFile : public std::ofstream {
  explicit TurboCfgFile(Isolate* isolate)
      : std::ofstream(isolate->GetTurboCfgFileName().c_str(),
                      std::ios_base::app) {}
};

struct TurboJsonFile : public std::ofstream {
  TurboJsonFile(CompilationInfo* info, std::ios_base::openmode mode)
      : std::ofstream(GetVisualizerLogFileName(info, nullptr, "json").get(),
                      mode) {}
};

void TraceSchedule(CompilationInfo* info, Schedule* schedule) {
  if (FLAG_trace_turbo) {
    AllowHandleDereference allow_deref;
    TurboJsonFile json_of(info, std::ios_base::app);
    json_of << "{\"name\":\"Schedule\",\"type\":\"schedule\",\"data\":\"";
    std::stringstream schedule_stream;
    schedule_stream << *schedule;
    std::string schedule_string(schedule_stream.str());
    for (const auto& c : schedule_string) {
      json_of << AsEscapedUC16ForJSON(c);
    }
    json_of << "\"},\n";
  }
  if (FLAG_trace_turbo_graph || FLAG_trace_turbo_scheduler) {
    AllowHandleDereference allow_deref;
    CodeTracer::Scope tracing_scope(info->isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "-- Schedule --------------------------------------\n" << *schedule;
  }
}


class AstGraphBuilderWithPositions final : public AstGraphBuilder {
 public:
  AstGraphBuilderWithPositions(Zone* local_zone, CompilationInfo* info,
                               JSGraph* jsgraph,
                               LoopAssignmentAnalysis* loop_assignment,
                               TypeHintAnalysis* type_hint_analysis,
                               SourcePositionTable* source_positions)
      : AstGraphBuilder(local_zone, info, jsgraph, 1.0f, loop_assignment,
                        type_hint_analysis),
        source_positions_(source_positions),
        start_position_(info->shared_info()->start_position()) {}

  bool CreateGraph() {
    SourcePositionTable::Scope pos_scope(source_positions_, start_position_);
    return AstGraphBuilder::CreateGraph();
  }

#define DEF_VISIT(type)                                               \
  void Visit##type(type* node) override {                             \
    SourcePositionTable::Scope pos(source_positions_,                 \
                                   SourcePosition(node->position())); \
    AstGraphBuilder::Visit##type(node);                               \
  }
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

 private:
  SourcePositionTable* const source_positions_;
  SourcePosition const start_position_;
};


class SourcePositionWrapper final : public Reducer {
 public:
  SourcePositionWrapper(Reducer* reducer, SourcePositionTable* table)
      : reducer_(reducer), table_(table) {}
  ~SourcePositionWrapper() final {}

  Reduction Reduce(Node* node) final {
    SourcePosition const pos = table_->GetSourcePosition(node);
    SourcePositionTable::Scope position(table_, pos);
    return reducer_->Reduce(node);
  }

  void Finalize() final { reducer_->Finalize(); }

 private:
  Reducer* const reducer_;
  SourcePositionTable* const table_;

  DISALLOW_COPY_AND_ASSIGN(SourcePositionWrapper);
};


class JSGraphReducer final : public GraphReducer {
 public:
  JSGraphReducer(JSGraph* jsgraph, Zone* zone)
      : GraphReducer(zone, jsgraph->graph(), jsgraph->Dead()) {}
  ~JSGraphReducer() final {}
};


void AddReducer(PipelineData* data, GraphReducer* graph_reducer,
                Reducer* reducer) {
  if (data->info()->is_source_positions_enabled()) {
    void* const buffer = data->graph_zone()->New(sizeof(SourcePositionWrapper));
    SourcePositionWrapper* const wrapper =
        new (buffer) SourcePositionWrapper(reducer, data->source_positions());
    graph_reducer->AddReducer(wrapper);
  } else {
    graph_reducer->AddReducer(reducer);
  }
}


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

PipelineStatistics* CreatePipelineStatistics(CompilationInfo* info,
                                             ZonePool* zone_pool) {
  PipelineStatistics* pipeline_statistics = nullptr;

  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics = new PipelineStatistics(info, zone_pool);
    pipeline_statistics->BeginPhaseKind("initializing");
  }

  if (FLAG_trace_turbo) {
    TurboJsonFile json_of(info, std::ios_base::trunc);
    Handle<Script> script = info->script();
    std::unique_ptr<char[]> function_name = info->GetDebugName();
    int pos = info->shared_info()->start_position();
    json_of << "{\"function\":\"" << function_name.get()
            << "\", \"sourcePosition\":" << pos << ", \"source\":\"";
    Isolate* isolate = info->isolate();
    if (!script->IsUndefined(isolate) &&
        !script->source()->IsUndefined(isolate)) {
      DisallowHeapAllocation no_allocation;
      int start = info->shared_info()->start_position();
      int len = info->shared_info()->end_position() - start;
      String::SubStringRange source(String::cast(script->source()), start, len);
      for (const auto& c : source) {
        json_of << AsEscapedUC16ForJSON(c);
      }
    }
    json_of << "\",\n\"phases\":[";
  }

  return pipeline_statistics;
}

}  // namespace

class PipelineCompilationJob final : public CompilationJob {
 public:
  PipelineCompilationJob(Isolate* isolate, Handle<JSFunction> function)
      // Note that the CompilationInfo is not initialized at the time we pass it
      // to the CompilationJob constructor, but it is not dereferenced there.
      : CompilationJob(isolate, &info_, "TurboFan"),
        zone_(isolate->allocator()),
        zone_pool_(isolate->allocator()),
        parse_info_(&zone_, function),
        info_(&parse_info_, function),
        pipeline_statistics_(CreatePipelineStatistics(info(), &zone_pool_)),
        data_(&zone_pool_, info(), pipeline_statistics_.get()),
        pipeline_(&data_),
        linkage_(nullptr) {}

 protected:
  Status PrepareJobImpl() final;
  Status ExecuteJobImpl() final;
  Status FinalizeJobImpl() final;

 private:
  Zone zone_;
  ZonePool zone_pool_;
  ParseInfo parse_info_;
  CompilationInfo info_;
  std::unique_ptr<PipelineStatistics> pipeline_statistics_;
  PipelineData data_;
  PipelineImpl pipeline_;
  Linkage* linkage_;

  DISALLOW_COPY_AND_ASSIGN(PipelineCompilationJob);
};

PipelineCompilationJob::Status PipelineCompilationJob::PrepareJobImpl() {
  if (info()->shared_info()->asm_function()) {
    if (info()->osr_frame()) info()->MarkAsFrameSpecializing();
    info()->MarkAsFunctionContextSpecializing();
  } else {
    if (!FLAG_always_opt) {
      info()->MarkAsBailoutOnUninitialized();
    }
    if (FLAG_native_context_specialization) {
      info()->MarkAsNativeContextSpecializing();
    }
    if (FLAG_turbo_inlining) {
      info()->MarkAsInliningEnabled();
    }
  }
  if (!info()->shared_info()->asm_function() || FLAG_turbo_asm_deoptimization) {
    info()->MarkAsDeoptimizationEnabled();
  }
  if (!info()->is_optimizing_from_bytecode()) {
    if (FLAG_inline_accessors) {
      info()->MarkAsAccessorInliningEnabled();
    }
    if (info()->is_deoptimization_enabled() && FLAG_turbo_type_feedback) {
      info()->MarkAsTypeFeedbackEnabled();
    }
    if (!Compiler::EnsureDeoptimizationSupport(info())) return FAILED;
  }

  linkage_ = new (&zone_) Linkage(Linkage::ComputeIncoming(&zone_, info()));

  if (!pipeline_.CreateGraph()) {
    if (isolate()->has_pending_exception()) return FAILED;  // Stack overflowed.
    return AbortOptimization(kGraphBuildingFailed);
  }

  return SUCCEEDED;
}

PipelineCompilationJob::Status PipelineCompilationJob::ExecuteJobImpl() {
  if (!pipeline_.OptimizeGraph(linkage_)) return FAILED;
  return SUCCEEDED;
}

PipelineCompilationJob::Status PipelineCompilationJob::FinalizeJobImpl() {
  Handle<Code> code = pipeline_.GenerateCode(linkage_);
  if (code.is_null()) {
    if (info()->bailout_reason() == kNoReason) {
      return AbortOptimization(kCodeGenerationFailed);
    }
    return FAILED;
  }
  info()->dependencies()->Commit(code);
  info()->SetCode(code);
  if (info()->is_deoptimization_enabled()) {
    info()->context()->native_context()->AddOptimizedCode(*code);
    RegisterWeakObjectsInOptimizedCode(code);
  }
  return SUCCEEDED;
}

class PipelineWasmCompilationJob final : public CompilationJob {
 public:
  explicit PipelineWasmCompilationJob(CompilationInfo* info, Graph* graph,
                                      CallDescriptor* descriptor,
                                      SourcePositionTable* source_positions)
      : CompilationJob(info->isolate(), info, "TurboFan",
                       State::kReadyToExecute),
        zone_pool_(info->isolate()->allocator()),
        data_(&zone_pool_, info, graph, source_positions),
        pipeline_(&data_),
        linkage_(descriptor) {}

 protected:
  Status PrepareJobImpl() final;
  Status ExecuteJobImpl() final;
  Status FinalizeJobImpl() final;

 private:
  ZonePool zone_pool_;
  PipelineData data_;
  PipelineImpl pipeline_;
  Linkage linkage_;
};

PipelineWasmCompilationJob::Status
PipelineWasmCompilationJob::PrepareJobImpl() {
  UNREACHABLE();  // Prepare should always be skipped for WasmCompilationJob.
  return SUCCEEDED;
}

PipelineWasmCompilationJob::Status
PipelineWasmCompilationJob::ExecuteJobImpl() {
  if (FLAG_trace_turbo) {
    TurboJsonFile json_of(info(), std::ios_base::trunc);
    json_of << "{\"function\":\"" << info()->GetDebugName().get()
            << "\", \"source\":\"\",\n\"phases\":[";
  }

  pipeline_.RunPrintAndVerify("Machine", true);

  if (!pipeline_.ScheduleAndSelectInstructions(&linkage_)) return FAILED;
  return SUCCEEDED;
}

PipelineWasmCompilationJob::Status
PipelineWasmCompilationJob::FinalizeJobImpl() {
  pipeline_.GenerateCode(&linkage_);
  return SUCCEEDED;
}

template <typename Phase>
void PipelineImpl::Run() {
  PipelineRunScope scope(this->data_, Phase::phase_name());
  Phase phase;
  phase.Run(this->data_, scope.zone());
}

template <typename Phase, typename Arg0>
void PipelineImpl::Run(Arg0 arg_0) {
  PipelineRunScope scope(this->data_, Phase::phase_name());
  Phase phase;
  phase.Run(this->data_, scope.zone(), arg_0);
}

template <typename Phase, typename Arg0, typename Arg1>
void PipelineImpl::Run(Arg0 arg_0, Arg1 arg_1) {
  PipelineRunScope scope(this->data_, Phase::phase_name());
  Phase phase;
  phase.Run(this->data_, scope.zone(), arg_0, arg_1);
}

struct LoopAssignmentAnalysisPhase {
  static const char* phase_name() { return "loop assignment analysis"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    if (!data->info()->is_optimizing_from_bytecode()) {
      AstLoopAssignmentAnalyzer analyzer(data->graph_zone(), data->info());
      LoopAssignmentAnalysis* loop_assignment = analyzer.Analyze();
      data->set_loop_assignment(loop_assignment);
    }
  }
};


struct TypeHintAnalysisPhase {
  static const char* phase_name() { return "type hint analysis"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    if (data->info()->is_type_feedback_enabled()) {
      TypeHintAnalyzer analyzer(data->graph_zone());
      Handle<Code> code(data->info()->shared_info()->code(), data->isolate());
      TypeHintAnalysis* type_hint_analysis = analyzer.Analyze(code);
      data->set_type_hint_analysis(type_hint_analysis);
    }
  }
};


struct GraphBuilderPhase {
  static const char* phase_name() { return "graph builder"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    bool succeeded = false;

    if (data->info()->is_optimizing_from_bytecode()) {
      BytecodeGraphBuilder graph_builder(temp_zone, data->info(),
                                         data->jsgraph(), 1.0f);
      succeeded = graph_builder.CreateGraph();
    } else {
      AstGraphBuilderWithPositions graph_builder(
          temp_zone, data->info(), data->jsgraph(), data->loop_assignment(),
          data->type_hint_analysis(), data->source_positions());
      succeeded = graph_builder.CreateGraph();
    }

    if (!succeeded) {
      data->set_compilation_failed();
    }
  }
};


struct InliningPhase {
  static const char* phase_name() { return "inlining"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->common(), data->machine());
    JSCallReducer::Flags call_reducer_flags = JSCallReducer::kNoFlags;
    if (data->info()->is_bailout_on_uninitialized()) {
      call_reducer_flags |= JSCallReducer::kBailoutOnUninitialized;
    }
    if (data->info()->is_deoptimization_enabled()) {
      call_reducer_flags |= JSCallReducer::kDeoptimizationEnabled;
    }
    JSCallReducer call_reducer(&graph_reducer, data->jsgraph(),
                               call_reducer_flags, data->native_context());
    JSContextSpecialization context_specialization(
        &graph_reducer, data->jsgraph(),
        data->info()->is_function_context_specializing()
            ? handle(data->info()->context())
            : MaybeHandle<Context>());
    JSFrameSpecialization frame_specialization(data->info()->osr_frame(),
                                               data->jsgraph());
    JSGlobalObjectSpecialization global_object_specialization(
        &graph_reducer, data->jsgraph(), data->native_context(),
        data->info()->dependencies());
    JSNativeContextSpecialization::Flags flags =
        JSNativeContextSpecialization::kNoFlags;
    if (data->info()->is_accessor_inlining_enabled()) {
      flags |= JSNativeContextSpecialization::kAccessorInliningEnabled;
    }
    if (data->info()->is_bailout_on_uninitialized()) {
      flags |= JSNativeContextSpecialization::kBailoutOnUninitialized;
    }
    if (data->info()->is_deoptimization_enabled()) {
      flags |= JSNativeContextSpecialization::kDeoptimizationEnabled;
    }
    JSNativeContextSpecialization native_context_specialization(
        &graph_reducer, data->jsgraph(), flags, data->native_context(),
        data->info()->dependencies(), temp_zone);
    JSInliningHeuristic inlining(&graph_reducer,
                                 data->info()->is_inlining_enabled()
                                     ? JSInliningHeuristic::kGeneralInlining
                                     : JSInliningHeuristic::kRestrictedInlining,
                                 temp_zone, data->info(), data->jsgraph());
    JSIntrinsicLowering intrinsic_lowering(
        &graph_reducer, data->jsgraph(),
        data->info()->is_deoptimization_enabled()
            ? JSIntrinsicLowering::kDeoptimizationEnabled
            : JSIntrinsicLowering::kDeoptimizationDisabled);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &common_reducer);
    if (data->info()->is_frame_specializing()) {
      AddReducer(data, &graph_reducer, &frame_specialization);
    }
    if (data->info()->is_deoptimization_enabled()) {
      AddReducer(data, &graph_reducer, &global_object_specialization);
    }
    AddReducer(data, &graph_reducer, &native_context_specialization);
    AddReducer(data, &graph_reducer, &context_specialization);
    AddReducer(data, &graph_reducer, &intrinsic_lowering);
    AddReducer(data, &graph_reducer, &call_reducer);
    AddReducer(data, &graph_reducer, &inlining);
    graph_reducer.ReduceGraph();
  }
};


struct TyperPhase {
  static const char* phase_name() { return "typer"; }

  void Run(PipelineData* data, Zone* temp_zone, Typer* typer) {
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    LoopVariableOptimizer induction_vars(data->jsgraph()->graph(),
                                         data->common(), temp_zone);
    if (FLAG_turbo_loop_variable) induction_vars.Run();
    typer->Run(roots, &induction_vars);
  }
};

#ifdef DEBUG

struct UntyperPhase {
  static const char* phase_name() { return "untyper"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    class RemoveTypeReducer final : public Reducer {
     public:
      Reduction Reduce(Node* node) final {
        if (NodeProperties::IsTyped(node)) {
          NodeProperties::RemoveType(node);
          return Changed(node);
        }
        return NoChange();
      }
    };

    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    RemoveTypeReducer remove_type_reducer;
    AddReducer(data, &graph_reducer, &remove_type_reducer);
    graph_reducer.ReduceGraph();
  }
};

#endif  // DEBUG

struct OsrDeconstructionPhase {
  static const char* phase_name() { return "OSR deconstruction"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    OsrHelper osr_helper(data->info());
    osr_helper.Deconstruct(data->jsgraph(), data->common(), temp_zone);
  }
};


struct TypedLoweringPhase {
  static const char* phase_name() { return "typed lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common());
    JSBuiltinReducer builtin_reducer(
        &graph_reducer, data->jsgraph(),
        data->info()->is_deoptimization_enabled()
            ? JSBuiltinReducer::kDeoptimizationEnabled
            : JSBuiltinReducer::kNoFlags,
        data->info()->dependencies());
    MaybeHandle<LiteralsArray> literals_array =
        data->info()->is_native_context_specializing()
            ? handle(data->info()->closure()->literals(), data->isolate())
            : MaybeHandle<LiteralsArray>();
    JSCreateLowering create_lowering(
        &graph_reducer, data->info()->dependencies(), data->jsgraph(),
        literals_array, data->native_context(), temp_zone);
    JSTypedLowering::Flags typed_lowering_flags = JSTypedLowering::kNoFlags;
    if (data->info()->is_deoptimization_enabled()) {
      typed_lowering_flags |= JSTypedLowering::kDeoptimizationEnabled;
    }
    JSTypedLowering typed_lowering(&graph_reducer, data->info()->dependencies(),
                                   typed_lowering_flags, data->jsgraph(),
                                   temp_zone);
    TypedOptimization typed_optimization(
        &graph_reducer, data->info()->dependencies(),
        data->info()->is_deoptimization_enabled()
            ? TypedOptimization::kDeoptimizationEnabled
            : TypedOptimization::kNoFlags,
        data->jsgraph());
    SimplifiedOperatorReducer simple_reducer(&graph_reducer, data->jsgraph());
    CheckpointElimination checkpoint_elimination(&graph_reducer);
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->common(), data->machine());
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &builtin_reducer);
    if (data->info()->is_deoptimization_enabled()) {
      AddReducer(data, &graph_reducer, &create_lowering);
    }
    AddReducer(data, &graph_reducer, &typed_optimization);
    AddReducer(data, &graph_reducer, &typed_lowering);
    AddReducer(data, &graph_reducer, &simple_reducer);
    AddReducer(data, &graph_reducer, &checkpoint_elimination);
    AddReducer(data, &graph_reducer, &common_reducer);
    graph_reducer.ReduceGraph();
  }
};


struct EscapeAnalysisPhase {
  static const char* phase_name() { return "escape analysis"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    EscapeAnalysis escape_analysis(data->graph(), data->jsgraph()->common(),
                                   temp_zone);
    escape_analysis.Run();
    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    EscapeAnalysisReducer escape_reducer(&graph_reducer, data->jsgraph(),
                                         &escape_analysis, temp_zone);
    AddReducer(data, &graph_reducer, &escape_reducer);
    graph_reducer.ReduceGraph();
    escape_reducer.VerifyReplacement();
  }
};

struct RepresentationSelectionPhase {
  static const char* phase_name() { return "representation selection"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SimplifiedLowering lowering(data->jsgraph(), temp_zone,
                                data->source_positions());
    lowering.LowerAllNodes();
  }
};

struct LoopPeelingPhase {
  static const char* phase_name() { return "loop peeling"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphTrimmer trimmer(temp_zone, data->graph());
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    trimmer.TrimGraph(roots.begin(), roots.end());

    LoopTree* loop_tree =
        LoopFinder::BuildLoopTree(data->jsgraph()->graph(), temp_zone);
    LoopPeeler::PeelInnerLoopsOfTree(data->graph(), data->common(), loop_tree,
                                     temp_zone);
  }
};

struct LoopExitEliminationPhase {
  static const char* phase_name() { return "loop exit elimination"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    LoopPeeler::EliminateLoopExits(data->graph(), temp_zone);
  }
};

struct GenericLoweringPhase {
  static const char* phase_name() { return "generic lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    JSGenericLowering generic_lowering(data->jsgraph());
    AddReducer(data, &graph_reducer, &generic_lowering);
    graph_reducer.ReduceGraph();
  }
};

struct EarlyOptimizationPhase {
  static const char* phase_name() { return "early optimization"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common());
    SimplifiedOperatorReducer simple_reducer(&graph_reducer, data->jsgraph());
    RedundancyElimination redundancy_elimination(&graph_reducer, temp_zone);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    MachineOperatorReducer machine_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->common(), data->machine());
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &simple_reducer);
    AddReducer(data, &graph_reducer, &redundancy_elimination);
    AddReducer(data, &graph_reducer, &value_numbering);
    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &common_reducer);
    graph_reducer.ReduceGraph();
  }
};

struct ControlFlowOptimizationPhase {
  static const char* phase_name() { return "control flow optimization"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    ControlFlowOptimizer optimizer(data->graph(), data->common(),
                                   data->machine(), temp_zone);
    optimizer.Optimize();
  }
};

struct EffectControlLinearizationPhase {
  static const char* phase_name() { return "effect linearization"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    // The scheduler requires the graphs to be trimmed, so trim now.
    // TODO(jarin) Remove the trimming once the scheduler can handle untrimmed
    // graphs.
    GraphTrimmer trimmer(temp_zone, data->graph());
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    trimmer.TrimGraph(roots.begin(), roots.end());

    // Schedule the graph without node splitting so that we can
    // fix the effect and control flow for nodes with low-level side
    // effects (such as changing representation to tagged or
    // 'floating' allocation regions.)
    Schedule* schedule = Scheduler::ComputeSchedule(temp_zone, data->graph(),
                                                    Scheduler::kNoFlags);
    if (FLAG_turbo_verify) ScheduleVerifier::Run(schedule);
    TraceSchedule(data->info(), schedule);

    // Post-pass for wiring the control/effects
    // - connect allocating representation changes into the control&effect
    //   chains and lower them,
    // - get rid of the region markers,
    // - introduce effect phis and rewire effects to get SSA again.
    EffectControlLinearizer linearizer(data->jsgraph(), schedule, temp_zone);
    linearizer.Run();
  }
};

// The store-store elimination greatly benefits from doing a common operator
// reducer and dead code elimination just before it, to eliminate conditional
// deopts with a constant condition.

struct DeadCodeEliminationPhase {
  static const char* phase_name() { return "dead code elimination"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->common(), data->machine());
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &common_reducer);
    graph_reducer.ReduceGraph();
  }
};

struct StoreStoreEliminationPhase {
  static const char* phase_name() { return "store-store elimination"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphTrimmer trimmer(temp_zone, data->graph());
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    trimmer.TrimGraph(roots.begin(), roots.end());

    StoreStoreElimination::Run(data->jsgraph(), temp_zone);
  }
};

struct LoadEliminationPhase {
  static const char* phase_name() { return "load elimination"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    BranchElimination branch_condition_elimination(&graph_reducer,
                                                   data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common());
    RedundancyElimination redundancy_elimination(&graph_reducer, temp_zone);
    LoadElimination load_elimination(&graph_reducer, data->jsgraph(),
                                     temp_zone);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->common(), data->machine());
    AddReducer(data, &graph_reducer, &branch_condition_elimination);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &redundancy_elimination);
    AddReducer(data, &graph_reducer, &load_elimination);
    AddReducer(data, &graph_reducer, &value_numbering);
    AddReducer(data, &graph_reducer, &common_reducer);
    graph_reducer.ReduceGraph();
  }
};

struct MemoryOptimizationPhase {
  static const char* phase_name() { return "memory optimization"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    // The memory optimizer requires the graphs to be trimmed, so trim now.
    GraphTrimmer trimmer(temp_zone, data->graph());
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    trimmer.TrimGraph(roots.begin(), roots.end());

    // Optimize allocations and load/store operations.
    MemoryOptimizer optimizer(data->jsgraph(), temp_zone);
    optimizer.Optimize();
  }
};

struct LateOptimizationPhase {
  static const char* phase_name() { return "late optimization"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    BranchElimination branch_condition_elimination(&graph_reducer,
                                                   data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common());
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    MachineOperatorReducer machine_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->common(), data->machine());
    SelectLowering select_lowering(data->jsgraph()->graph(),
                                   data->jsgraph()->common());
    TailCallOptimization tco(data->common(), data->graph());
    AddReducer(data, &graph_reducer, &branch_condition_elimination);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &value_numbering);
    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &common_reducer);
    AddReducer(data, &graph_reducer, &select_lowering);
    AddReducer(data, &graph_reducer, &tco);
    graph_reducer.ReduceGraph();
  }
};

struct EarlyGraphTrimmingPhase {
  static const char* phase_name() { return "early graph trimming"; }
  void Run(PipelineData* data, Zone* temp_zone) {
    GraphTrimmer trimmer(temp_zone, data->graph());
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    trimmer.TrimGraph(roots.begin(), roots.end());
  }
};


struct LateGraphTrimmingPhase {
  static const char* phase_name() { return "late graph trimming"; }
  void Run(PipelineData* data, Zone* temp_zone) {
    GraphTrimmer trimmer(temp_zone, data->graph());
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    trimmer.TrimGraph(roots.begin(), roots.end());
  }
};


struct StressLoopPeelingPhase {
  static const char* phase_name() { return "stress loop peeling"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    // Peel the first outer loop for testing.
    // TODO(titzer): peel all loops? the N'th loop? Innermost loops?
    LoopTree* loop_tree = LoopFinder::BuildLoopTree(data->graph(), temp_zone);
    if (loop_tree != nullptr && loop_tree->outer_loops().size() > 0) {
      LoopPeeler::Peel(data->graph(), data->common(), loop_tree,
                       loop_tree->outer_loops()[0], temp_zone);
    }
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
    InstructionSelector selector(
        temp_zone, data->graph()->NodeCount(), linkage, data->sequence(),
        data->schedule(), data->source_positions(), data->frame(),
        data->info()->is_source_positions_enabled()
            ? InstructionSelector::kAllSourcePositions
            : InstructionSelector::kCallSourcePositions,
        InstructionSelector::SupportedFeatures(),
        FLAG_turbo_instruction_scheduling
            ? InstructionSelector::kEnableScheduling
            : InstructionSelector::kDisableScheduling,
        data->info()->will_serialize()
            ? InstructionSelector::kEnableSerialization
            : InstructionSelector::kDisableSerialization);
    if (!selector.SelectInstructions()) {
      data->set_compilation_failed();
    }
  }
};


struct MeetRegisterConstraintsPhase {
  static const char* phase_name() { return "meet register constraints"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    ConstraintBuilder builder(data->register_allocation_data());
    builder.MeetRegisterConstraints();
  }
};


struct ResolvePhisPhase {
  static const char* phase_name() { return "resolve phis"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    ConstraintBuilder builder(data->register_allocation_data());
    builder.ResolvePhis();
  }
};


struct BuildLiveRangesPhase {
  static const char* phase_name() { return "build live ranges"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    LiveRangeBuilder builder(data->register_allocation_data(), temp_zone);
    builder.BuildLiveRanges();
  }
};


struct SplinterLiveRangesPhase {
  static const char* phase_name() { return "splinter live ranges"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    LiveRangeSeparator live_range_splinterer(data->register_allocation_data(),
                                             temp_zone);
    live_range_splinterer.Splinter();
  }
};


template <typename RegAllocator>
struct AllocateGeneralRegistersPhase {
  static const char* phase_name() { return "allocate general registers"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    RegAllocator allocator(data->register_allocation_data(), GENERAL_REGISTERS,
                           temp_zone);
    allocator.AllocateRegisters();
  }
};

template <typename RegAllocator>
struct AllocateFPRegistersPhase {
  static const char* phase_name() {
    return "allocate floating point registers";
  }

  void Run(PipelineData* data, Zone* temp_zone) {
    RegAllocator allocator(data->register_allocation_data(), FP_REGISTERS,
                           temp_zone);
    allocator.AllocateRegisters();
  }
};


struct MergeSplintersPhase {
  static const char* phase_name() { return "merge splintered ranges"; }
  void Run(PipelineData* pipeline_data, Zone* temp_zone) {
    RegisterAllocationData* data = pipeline_data->register_allocation_data();
    LiveRangeMerger live_range_merger(data, temp_zone);
    live_range_merger.Merge();
  }
};


struct LocateSpillSlotsPhase {
  static const char* phase_name() { return "locate spill slots"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SpillSlotLocator locator(data->register_allocation_data());
    locator.LocateSpillSlots();
  }
};


struct AssignSpillSlotsPhase {
  static const char* phase_name() { return "assign spill slots"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    OperandAssigner assigner(data->register_allocation_data());
    assigner.AssignSpillSlots();
  }
};


struct CommitAssignmentPhase {
  static const char* phase_name() { return "commit assignment"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    OperandAssigner assigner(data->register_allocation_data());
    assigner.CommitAssignment();
  }
};


struct PopulateReferenceMapsPhase {
  static const char* phase_name() { return "populate pointer maps"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    ReferenceMapPopulator populator(data->register_allocation_data());
    populator.PopulateReferenceMaps();
  }
};


struct ConnectRangesPhase {
  static const char* phase_name() { return "connect ranges"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    LiveRangeConnector connector(data->register_allocation_data());
    connector.ConnectRanges(temp_zone);
  }
};


struct ResolveControlFlowPhase {
  static const char* phase_name() { return "resolve control flow"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    LiveRangeConnector connector(data->register_allocation_data());
    connector.ResolveControlFlow(temp_zone);
  }
};


struct OptimizeMovesPhase {
  static const char* phase_name() { return "optimize moves"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    MoveOptimizer move_optimizer(temp_zone, data->sequence());
    move_optimizer.Run();
  }
};


struct FrameElisionPhase {
  static const char* phase_name() { return "frame elision"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    FrameElider(data->sequence()).Run();
  }
};


struct JumpThreadingPhase {
  static const char* phase_name() { return "jump threading"; }

  void Run(PipelineData* data, Zone* temp_zone, bool frame_at_start) {
    ZoneVector<RpoNumber> result(temp_zone);
    if (JumpThreading::ComputeForwarding(temp_zone, result, data->sequence(),
                                         frame_at_start)) {
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

    {  // Print JSON.
      AllowHandleDereference allow_deref;
      TurboJsonFile json_of(info, std::ios_base::app);
      json_of << "{\"name\":\"" << phase << "\",\"type\":\"graph\",\"data\":"
              << AsJSON(*graph, data->source_positions()) << "},\n";
    }

    if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
      AllowHandleDereference allow_deref;
      CodeTracer::Scope tracing_scope(info->isolate()->GetCodeTracer());
      OFStream os(tracing_scope.file());
      os << "-- Graph after " << phase << " -- " << std::endl;
      os << AsRPO(*graph);
    }
  }
};


struct VerifyGraphPhase {
  static const char* phase_name() { return nullptr; }

  void Run(PipelineData* data, Zone* temp_zone, const bool untyped,
           bool values_only = false) {
    Verifier::Run(data->graph(), !untyped ? Verifier::TYPED : Verifier::UNTYPED,
                  values_only ? Verifier::kValuesOnly : Verifier::kAll);
  }
};

void PipelineImpl::RunPrintAndVerify(const char* phase, bool untyped) {
  if (FLAG_trace_turbo) {
    Run<PrintGraphPhase>(phase);
  }
  if (FLAG_turbo_verify) {
    Run<VerifyGraphPhase>(untyped);
  }
}

bool PipelineImpl::CreateGraph() {
  PipelineData* data = this->data_;

  data->BeginPhaseKind("graph creation");

  if (FLAG_trace_turbo) {
    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "---------------------------------------------------\n"
       << "Begin compiling method " << info()->GetDebugName().get()
       << " using Turbofan" << std::endl;
    TurboCfgFile tcf(isolate());
    tcf << AsC1VCompilation(info());
  }

  data->source_positions()->AddDecorator();

  if (FLAG_loop_assignment_analysis) {
    Run<LoopAssignmentAnalysisPhase>();
  }

  Run<TypeHintAnalysisPhase>();

  Run<GraphBuilderPhase>();
  if (data->compilation_failed()) {
    data->EndPhaseKind();
    return false;
  }
  RunPrintAndVerify("Initial untyped", true);

  // Perform OSR deconstruction.
  if (info()->is_osr()) {
    Run<OsrDeconstructionPhase>();
    RunPrintAndVerify("OSR deconstruction", true);
  }

  // Perform function context specialization and inlining (if enabled).
  Run<InliningPhase>();
  RunPrintAndVerify("Inlined", true);

  // Remove dead->live edges from the graph.
  Run<EarlyGraphTrimmingPhase>();
  RunPrintAndVerify("Early trimmed", true);

  if (FLAG_print_turbo_replay) {
    // Print a replay of the initial graph.
    GraphReplayPrinter::PrintReplay(data->graph());
  }

  // Run the type-sensitive lowerings and optimizations on the graph.
  {
    // Type the graph and keep the Typer running on newly created nodes within
    // this scope; the Typer is automatically unlinked from the Graph once we
    // leave this scope below.
    Typer typer(isolate(), data->graph());
    Run<TyperPhase>(&typer);
    RunPrintAndVerify("Typed");

    data->BeginPhaseKind("lowering");

    // Lower JSOperators where we can determine types.
    Run<TypedLoweringPhase>();
    RunPrintAndVerify("Lowered typed");

    if (FLAG_turbo_loop_peeling) {
      Run<LoopPeelingPhase>();
      RunPrintAndVerify("Loops peeled", true);
    } else {
      Run<LoopExitEliminationPhase>();
      RunPrintAndVerify("Loop exits eliminated", true);
    }

    if (FLAG_turbo_stress_loop_peeling) {
      Run<StressLoopPeelingPhase>();
      RunPrintAndVerify("Loop peeled");
    }

    if (FLAG_turbo_escape) {
      Run<EscapeAnalysisPhase>();
      RunPrintAndVerify("Escape Analysed");
    }

    if (!info()->shared_info()->asm_function() && FLAG_turbo_load_elimination) {
      Run<LoadEliminationPhase>();
      RunPrintAndVerify("Load eliminated");
    }
  }

  // Select representations. This has to run w/o the Typer decorator, because
  // we cannot compute meaningful types anyways, and the computed types might
  // even conflict with the representation/truncation logic.
  Run<RepresentationSelectionPhase>();
  RunPrintAndVerify("Representations selected", true);

#ifdef DEBUG
  // From now on it is invalid to look at types on the nodes, because:
  //
  //  (a) The remaining passes (might) run concurrent to the main thread and
  //      therefore must not access the Heap or the Isolate in an uncontrolled
  //      way (as done by the type system), and
  //  (b) the types on the nodes might not make sense after representation
  //      selection due to the way we handle truncations; if we'd want to look
  //      at types afterwards we'd essentially need to re-type (large portions
  //      of) the graph.
  //
  // In order to catch bugs related to type access after this point we remove
  // the types from the nodes at this point (currently only in Debug builds).
  Run<UntyperPhase>();
  RunPrintAndVerify("Untyped", true);
#endif

  // Run generic lowering pass.
  Run<GenericLoweringPhase>();
  RunPrintAndVerify("Generic lowering", true);

  data->EndPhaseKind();

  return true;
}

bool PipelineImpl::OptimizeGraph(Linkage* linkage) {
  PipelineData* data = this->data_;

  data->BeginPhaseKind("block building");

  // Run early optimization pass.
  Run<EarlyOptimizationPhase>();
  RunPrintAndVerify("Early optimized", true);

  Run<EffectControlLinearizationPhase>();
  RunPrintAndVerify("Effect and control linearized", true);

  Run<DeadCodeEliminationPhase>();
  RunPrintAndVerify("Dead code elimination", true);

  if (FLAG_turbo_store_elimination) {
    Run<StoreStoreEliminationPhase>();
    RunPrintAndVerify("Store-store elimination", true);
  }

  // Optimize control flow.
  if (FLAG_turbo_cf_optimization) {
    Run<ControlFlowOptimizationPhase>();
    RunPrintAndVerify("Control flow optimized", true);
  }

  // Optimize memory access and allocation operations.
  Run<MemoryOptimizationPhase>();
  // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
  RunPrintAndVerify("Memory optimized", true);

  // Lower changes that have been inserted before.
  Run<LateOptimizationPhase>();
  // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
  RunPrintAndVerify("Late optimized", true);

  Run<LateGraphTrimmingPhase>();
  // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
  RunPrintAndVerify("Late trimmed", true);

  data->source_positions()->RemoveDecorator();

  return ScheduleAndSelectInstructions(linkage);
}

Handle<Code> Pipeline::GenerateCodeForCodeStub(Isolate* isolate,
                                               CallDescriptor* call_descriptor,
                                               Graph* graph, Schedule* schedule,
                                               Code::Flags flags,
                                               const char* debug_name) {
  CompilationInfo info(CStrVector(debug_name), isolate, graph->zone(), flags);
  if (isolate->serializer_enabled()) info.PrepareForSerializing();

  // Construct a pipeline for scheduling and code generation.
  ZonePool zone_pool(isolate->allocator());
  PipelineData data(&zone_pool, &info, graph, schedule);
  std::unique_ptr<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics.reset(new PipelineStatistics(&info, &zone_pool));
    pipeline_statistics->BeginPhaseKind("stub codegen");
  }

  PipelineImpl pipeline(&data);
  DCHECK_NOT_NULL(data.schedule());

  if (FLAG_trace_turbo) {
    {
      TurboJsonFile json_of(&info, std::ios_base::trunc);
      json_of << "{\"function\":\"" << info.GetDebugName().get()
              << "\", \"source\":\"\",\n\"phases\":[";
    }
    pipeline.Run<PrintGraphPhase>("Machine");
  }

  pipeline.Run<VerifyGraphPhase>(false, true);
  return pipeline.ScheduleAndGenerateCode(call_descriptor);
}

// static
Handle<Code> Pipeline::GenerateCodeForTesting(CompilationInfo* info) {
  ZonePool zone_pool(info->isolate()->allocator());
  std::unique_ptr<PipelineStatistics> pipeline_statistics(
      CreatePipelineStatistics(info, &zone_pool));
  PipelineData data(&zone_pool, info, pipeline_statistics.get());
  PipelineImpl pipeline(&data);

  Linkage linkage(Linkage::ComputeIncoming(data.instruction_zone(), info));

  if (!pipeline.CreateGraph()) return Handle<Code>::null();
  if (!pipeline.OptimizeGraph(&linkage)) return Handle<Code>::null();
  return pipeline.GenerateCode(&linkage);
}

// static
Handle<Code> Pipeline::GenerateCodeForTesting(CompilationInfo* info,
                                              Graph* graph,
                                              Schedule* schedule) {
  CallDescriptor* call_descriptor =
      Linkage::ComputeIncoming(info->zone(), info);
  return GenerateCodeForTesting(info, call_descriptor, graph, schedule);
}

// static
Handle<Code> Pipeline::GenerateCodeForTesting(CompilationInfo* info,
                                              CallDescriptor* call_descriptor,
                                              Graph* graph,
                                              Schedule* schedule) {
  // Construct a pipeline for scheduling and code generation.
  ZonePool zone_pool(info->isolate()->allocator());
  PipelineData data(&zone_pool, info, graph, schedule);
  std::unique_ptr<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics.reset(new PipelineStatistics(info, &zone_pool));
    pipeline_statistics->BeginPhaseKind("test codegen");
  }

  PipelineImpl pipeline(&data);

  if (FLAG_trace_turbo) {
    TurboJsonFile json_of(info, std::ios_base::trunc);
    json_of << "{\"function\":\"" << info->GetDebugName().get()
            << "\", \"source\":\"\",\n\"phases\":[";
  }
  // TODO(rossberg): Should this really be untyped?
  pipeline.RunPrintAndVerify("Machine", true);

  return pipeline.ScheduleAndGenerateCode(call_descriptor);
}

// static
CompilationJob* Pipeline::NewCompilationJob(Handle<JSFunction> function) {
  return new PipelineCompilationJob(function->GetIsolate(), function);
}

// static
CompilationJob* Pipeline::NewWasmCompilationJob(
    CompilationInfo* info, Graph* graph, CallDescriptor* descriptor,
    SourcePositionTable* source_positions) {
  return new PipelineWasmCompilationJob(info, graph, descriptor,
                                        source_positions);
}

bool Pipeline::AllocateRegistersForTesting(const RegisterConfiguration* config,
                                           InstructionSequence* sequence,
                                           bool run_verifier) {
  CompilationInfo info(ArrayVector("testing"), sequence->isolate(),
                       sequence->zone(), Code::ComputeFlags(Code::STUB));
  ZonePool zone_pool(sequence->isolate()->allocator());
  PipelineData data(&zone_pool, &info, sequence);
  PipelineImpl pipeline(&data);
  pipeline.data_->InitializeFrameData(nullptr);
  pipeline.AllocateRegisters(config, nullptr, run_verifier);
  return !data.compilation_failed();
}

bool PipelineImpl::ScheduleAndSelectInstructions(Linkage* linkage) {
  CallDescriptor* call_descriptor = linkage->GetIncomingDescriptor();
  PipelineData* data = this->data_;

  DCHECK_NOT_NULL(data->graph());

  if (data->schedule() == nullptr) Run<ComputeSchedulePhase>();
  TraceSchedule(data->info(), data->schedule());

  if (FLAG_turbo_profiling) {
    data->set_profiler_data(BasicBlockInstrumentor::Instrument(
        info(), data->graph(), data->schedule()));
  }

  if (FLAG_turbo_verify_machine_graph) {
    Zone temp_zone(data->isolate()->allocator());
    MachineGraphVerifier::Run(data->graph(), data->schedule(), linkage,
                              &temp_zone);
  }

  data->InitializeInstructionSequence(call_descriptor);

  data->InitializeFrameData(call_descriptor);
  // Select and schedule instructions covering the scheduled graph.
  Run<InstructionSelectionPhase>(linkage);
  if (data->compilation_failed()) {
    info()->AbortOptimization(kCodeGenerationFailed);
    data->EndPhaseKind();
    return false;
  }

  if (FLAG_trace_turbo && !data->MayHaveUnverifiableGraph()) {
    AllowHandleDereference allow_deref;
    TurboCfgFile tcf(isolate());
    tcf << AsC1V("CodeGen", data->schedule(), data->source_positions(),
                 data->sequence());
  }

  if (FLAG_trace_turbo) {
    std::ostringstream source_position_output;
    // Output source position information before the graph is deleted.
    data_->source_positions()->Print(source_position_output);
    data_->set_source_position_output(source_position_output.str());
  }

  data->DeleteGraphZone();

  data->BeginPhaseKind("register allocation");

  bool run_verifier = FLAG_turbo_verify_allocation;

  // Allocate registers.
  AllocateRegisters(RegisterConfiguration::Turbofan(), call_descriptor,
                    run_verifier);
  Run<FrameElisionPhase>();
  if (data->compilation_failed()) {
    info()->AbortOptimization(kNotEnoughVirtualRegistersRegalloc);
    data->EndPhaseKind();
    return false;
  }

  // TODO(mtrofin): move this off to the register allocator.
  bool generate_frame_at_start =
      data_->sequence()->instruction_blocks().front()->must_construct_frame();
  // Optimimize jumps.
  if (FLAG_turbo_jt) {
    Run<JumpThreadingPhase>(generate_frame_at_start);
  }

  data->EndPhaseKind();

  return true;
}

Handle<Code> PipelineImpl::GenerateCode(Linkage* linkage) {
  PipelineData* data = this->data_;

  data->BeginPhaseKind("code generation");

  // Generate final machine code.
  Run<GenerateCodePhase>(linkage);

  Handle<Code> code = data->code();
  if (data->profiler_data()) {
#if ENABLE_DISASSEMBLER
    std::ostringstream os;
    code->Disassemble(nullptr, os);
    data->profiler_data()->SetCode(&os);
#endif
  }

  info()->SetCode(code);
  v8::internal::CodeGenerator::PrintCode(code, info());

  if (FLAG_trace_turbo) {
    TurboJsonFile json_of(info(), std::ios_base::app);
    json_of << "{\"name\":\"disassembly\",\"type\":\"disassembly\",\"data\":\"";
#if ENABLE_DISASSEMBLER
    std::stringstream disassembly_stream;
    code->Disassemble(nullptr, disassembly_stream);
    std::string disassembly_string(disassembly_stream.str());
    for (const auto& c : disassembly_string) {
      json_of << AsEscapedUC16ForJSON(c);
    }
#endif  // ENABLE_DISASSEMBLER
    json_of << "\"}\n],\n";
    json_of << "\"nodePositions\":";
    json_of << data->source_position_output();
    json_of << "}";

    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "---------------------------------------------------\n"
       << "Finished compiling method " << info()->GetDebugName().get()
       << " using Turbofan" << std::endl;
  }

  return code;
}

Handle<Code> PipelineImpl::ScheduleAndGenerateCode(
    CallDescriptor* call_descriptor) {
  Linkage linkage(call_descriptor);

  // Schedule the graph, perform instruction selection and register allocation.
  if (!ScheduleAndSelectInstructions(&linkage)) return Handle<Code>();

  // Generate the final machine code.
  return GenerateCode(&linkage);
}

void PipelineImpl::AllocateRegisters(const RegisterConfiguration* config,
                                     CallDescriptor* descriptor,
                                     bool run_verifier) {
  PipelineData* data = this->data_;
  // Don't track usage for this zone in compiler stats.
  std::unique_ptr<Zone> verifier_zone;
  RegisterAllocatorVerifier* verifier = nullptr;
  if (run_verifier) {
    verifier_zone.reset(new Zone(isolate()->allocator()));
    verifier = new (verifier_zone.get()) RegisterAllocatorVerifier(
        verifier_zone.get(), config, data->sequence());
  }

#ifdef DEBUG
  data_->sequence()->ValidateEdgeSplitForm();
  data_->sequence()->ValidateDeferredBlockEntryPaths();
  data_->sequence()->ValidateDeferredBlockExitPaths();
#endif

  data->InitializeRegisterAllocationData(config, descriptor);
  if (info()->is_osr()) {
    AllowHandleDereference allow_deref;
    OsrHelper osr_helper(info());
    osr_helper.SetupFrame(data->frame());
  }

  Run<MeetRegisterConstraintsPhase>();
  Run<ResolvePhisPhase>();
  Run<BuildLiveRangesPhase>();
  if (FLAG_trace_turbo_graph) {
    AllowHandleDereference allow_deref;
    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "----- Instruction sequence before register allocation -----\n"
       << PrintableInstructionSequence({config, data->sequence()});
  }
  if (verifier != nullptr) {
    CHECK(!data->register_allocation_data()->ExistsUseWithoutDefinition());
    CHECK(data->register_allocation_data()
              ->RangesDefinedInDeferredStayInDeferred());
  }

  if (FLAG_turbo_preprocess_ranges) {
    Run<SplinterLiveRangesPhase>();
  }

  Run<AllocateGeneralRegistersPhase<LinearScanAllocator>>();
  Run<AllocateFPRegistersPhase<LinearScanAllocator>>();

  if (FLAG_turbo_preprocess_ranges) {
    Run<MergeSplintersPhase>();
  }

  Run<AssignSpillSlotsPhase>();

  Run<CommitAssignmentPhase>();
  Run<PopulateReferenceMapsPhase>();
  Run<ConnectRangesPhase>();
  Run<ResolveControlFlowPhase>();
  if (FLAG_turbo_move_optimization) {
    Run<OptimizeMovesPhase>();
  }

  Run<LocateSpillSlotsPhase>();

  if (FLAG_trace_turbo_graph) {
    AllowHandleDereference allow_deref;
    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "----- Instruction sequence after register allocation -----\n"
       << PrintableInstructionSequence({config, data->sequence()});
  }

  if (verifier != nullptr) {
    verifier->VerifyAssignment();
    verifier->VerifyGapMoves();
  }

  if (FLAG_trace_turbo && !data->MayHaveUnverifiableGraph()) {
    TurboCfgFile tcf(data->isolate());
    tcf << AsC1VRegisterAllocationData("CodeGen",
                                       data->register_allocation_data());
  }

  data->DeleteRegisterAllocationZone();
}

CompilationInfo* PipelineImpl::info() const { return data_->info(); }

Isolate* PipelineImpl::isolate() const { return info()->isolate(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8

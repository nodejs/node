// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/pipeline.h"

#include <fstream>  // NOLINT(readability/streams)
#include <iostream>
#include <memory>
#include <sstream>

#include "src/base/optional.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/builtins/profile-data-reader.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compiler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/codegen/register-configuration.h"
#include "src/compiler/add-type-assertions-reducer.h"
#include "src/compiler/backend/code-generator.h"
#include "src/compiler/backend/frame-elider.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/backend/jump-threading.h"
#include "src/compiler/backend/mid-tier-register-allocator.h"
#include "src/compiler/backend/move-optimizer.h"
#include "src/compiler/backend/register-allocator-verifier.h"
#include "src/compiler/backend/register-allocator.h"
#include "src/compiler/basic-block-instrumentor.h"
#include "src/compiler/branch-elimination.h"
#include "src/compiler/bytecode-graph-builder.h"
#include "src/compiler/checkpoint-elimination.h"
#include "src/compiler/common-operator-reducer.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/constant-folding-reducer.h"
#include "src/compiler/control-flow-optimizer.h"
#include "src/compiler/csa-load-elimination.h"
#include "src/compiler/dead-code-elimination.h"
#include "src/compiler/decompression-optimizer.h"
#include "src/compiler/effect-control-linearizer.h"
#include "src/compiler/escape-analysis-reducer.h"
#include "src/compiler/escape-analysis.h"
#include "src/compiler/graph-trimmer.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/js-call-reducer.h"
#include "src/compiler/js-context-specialization.h"
#include "src/compiler/js-create-lowering.h"
#include "src/compiler/js-generic-lowering.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/js-heap-copy-reducer.h"
#include "src/compiler/js-inlining-heuristic.h"
#include "src/compiler/js-intrinsic-lowering.h"
#include "src/compiler/js-native-context-specialization.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/load-elimination.h"
#include "src/compiler/loop-analysis.h"
#include "src/compiler/loop-peeling.h"
#include "src/compiler/loop-variable-optimizer.h"
#include "src/compiler/machine-graph-verifier.h"
#include "src/compiler/machine-operator-reducer.h"
#include "src/compiler/memory-optimizer.h"
#include "src/compiler/node-observer.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/osr.h"
#include "src/compiler/pipeline-statistics.h"
#include "src/compiler/redundancy-elimination.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduled-machine-lowering.h"
#include "src/compiler/scheduler.h"
#include "src/compiler/select-lowering.h"
#include "src/compiler/serializer-for-background-compilation.h"
#include "src/compiler/simplified-lowering.h"
#include "src/compiler/simplified-operator-reducer.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/store-store-elimination.h"
#include "src/compiler/type-narrowing-reducer.h"
#include "src/compiler/typed-optimization.h"
#include "src/compiler/typer.h"
#include "src/compiler/value-numbering-reducer.h"
#include "src/compiler/verifier.h"
#include "src/compiler/wasm-compiler.h"
#include "src/compiler/zone-stats.h"
#include "src/diagnostics/code-tracer.h"
#include "src/diagnostics/disassembler.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/local-heap.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/objects/shared-function-info.h"
#include "src/parsing/parse-info.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/traced-value.h"
#include "src/utils/ostreams.h"
#include "src/utils/utils.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {
namespace compiler {

static constexpr char kCodegenZoneName[] = "codegen-zone";
static constexpr char kGraphZoneName[] = "graph-zone";
static constexpr char kInstructionZoneName[] = "instruction-zone";
static constexpr char kMachineGraphVerifierZoneName[] =
    "machine-graph-verifier-zone";
static constexpr char kPipelineCompilationJobZoneName[] =
    "pipeline-compilation-job-zone";
static constexpr char kRegisterAllocationZoneName[] =
    "register-allocation-zone";
static constexpr char kRegisterAllocatorVerifierZoneName[] =
    "register-allocator-verifier-zone";
namespace {

Maybe<OuterContext> GetModuleContext(Handle<JSFunction> closure) {
  Context current = closure->context();
  size_t distance = 0;
  while (!current.IsNativeContext()) {
    if (current.IsModuleContext()) {
      return Just(
          OuterContext(handle(current, current.GetIsolate()), distance));
    }
    current = current.previous();
    distance++;
  }
  return Nothing<OuterContext>();
}

}  // anonymous namespace

class PipelineData {
 public:
  // For main entry point.
  PipelineData(ZoneStats* zone_stats, Isolate* isolate,
               OptimizedCompilationInfo* info,
               PipelineStatistics* pipeline_statistics,
               bool is_concurrent_inlining)
      : isolate_(isolate),
        allocator_(isolate->allocator()),
        info_(info),
        debug_name_(info_->GetDebugName()),
        may_have_unverifiable_graph_(false),
        zone_stats_(zone_stats),
        pipeline_statistics_(pipeline_statistics),
        roots_relative_addressing_enabled_(
            !isolate->serializer_enabled() &&
            !isolate->IsGeneratingEmbeddedBuiltins()),
        graph_zone_scope_(zone_stats_, kGraphZoneName, kCompressGraphZone),
        graph_zone_(graph_zone_scope_.zone()),
        instruction_zone_scope_(zone_stats_, kInstructionZoneName),
        instruction_zone_(instruction_zone_scope_.zone()),
        codegen_zone_scope_(zone_stats_, kCodegenZoneName),
        codegen_zone_(codegen_zone_scope_.zone()),
        broker_(new JSHeapBroker(isolate_, info_->zone(),
                                 info_->trace_heap_broker(),
                                 is_concurrent_inlining, info->code_kind())),
        register_allocation_zone_scope_(zone_stats_,
                                        kRegisterAllocationZoneName),
        register_allocation_zone_(register_allocation_zone_scope_.zone()),
        assembler_options_(AssemblerOptions::Default(isolate)) {
    PhaseScope scope(pipeline_statistics, "V8.TFInitPipelineData");
    graph_ = graph_zone_->New<Graph>(graph_zone_);
    source_positions_ = graph_zone_->New<SourcePositionTable>(graph_);
    node_origins_ = info->trace_turbo_json()
                        ? graph_zone_->New<NodeOriginTable>(graph_)
                        : nullptr;
    simplified_ = graph_zone_->New<SimplifiedOperatorBuilder>(graph_zone_);
    machine_ = graph_zone_->New<MachineOperatorBuilder>(
        graph_zone_, MachineType::PointerRepresentation(),
        InstructionSelector::SupportedMachineOperatorFlags(),
        InstructionSelector::AlignmentRequirements());
    common_ = graph_zone_->New<CommonOperatorBuilder>(graph_zone_);
    javascript_ = graph_zone_->New<JSOperatorBuilder>(graph_zone_);
    jsgraph_ = graph_zone_->New<JSGraph>(isolate_, graph_, common_, javascript_,
                                         simplified_, machine_);
    observe_node_manager_ =
        info->node_observer()
            ? graph_zone_->New<ObserveNodeManager>(graph_zone_)
            : nullptr;
    dependencies_ =
        info_->zone()->New<CompilationDependencies>(broker_, info_->zone());
  }

  // For WebAssembly compile entry point.
  PipelineData(ZoneStats* zone_stats, wasm::WasmEngine* wasm_engine,
               OptimizedCompilationInfo* info, MachineGraph* mcgraph,
               PipelineStatistics* pipeline_statistics,
               SourcePositionTable* source_positions,
               NodeOriginTable* node_origins,
               const AssemblerOptions& assembler_options)
      : isolate_(nullptr),
        wasm_engine_(wasm_engine),
        allocator_(wasm_engine->allocator()),
        info_(info),
        debug_name_(info_->GetDebugName()),
        may_have_unverifiable_graph_(false),
        zone_stats_(zone_stats),
        pipeline_statistics_(pipeline_statistics),
        graph_zone_scope_(zone_stats_, kGraphZoneName, kCompressGraphZone),
        graph_zone_(graph_zone_scope_.zone()),
        graph_(mcgraph->graph()),
        source_positions_(source_positions),
        node_origins_(node_origins),
        machine_(mcgraph->machine()),
        common_(mcgraph->common()),
        mcgraph_(mcgraph),
        instruction_zone_scope_(zone_stats_, kInstructionZoneName),
        instruction_zone_(instruction_zone_scope_.zone()),
        codegen_zone_scope_(zone_stats_, kCodegenZoneName),
        codegen_zone_(codegen_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_stats_,
                                        kRegisterAllocationZoneName),
        register_allocation_zone_(register_allocation_zone_scope_.zone()),
        assembler_options_(assembler_options) {}

  // For CodeStubAssembler and machine graph testing entry point.
  PipelineData(ZoneStats* zone_stats, OptimizedCompilationInfo* info,
               Isolate* isolate, AccountingAllocator* allocator, Graph* graph,
               JSGraph* jsgraph, Schedule* schedule,
               SourcePositionTable* source_positions,
               NodeOriginTable* node_origins, JumpOptimizationInfo* jump_opt,
               const AssemblerOptions& assembler_options,
               const ProfileDataFromFile* profile_data)
      : isolate_(isolate),
        wasm_engine_(isolate_->wasm_engine()),
        allocator_(allocator),
        info_(info),
        debug_name_(info_->GetDebugName()),
        zone_stats_(zone_stats),
        graph_zone_scope_(zone_stats_, kGraphZoneName, kCompressGraphZone),
        graph_zone_(graph_zone_scope_.zone()),
        graph_(graph),
        source_positions_(source_positions),
        node_origins_(node_origins),
        schedule_(schedule),
        instruction_zone_scope_(zone_stats_, kInstructionZoneName),
        instruction_zone_(instruction_zone_scope_.zone()),
        codegen_zone_scope_(zone_stats_, kCodegenZoneName),
        codegen_zone_(codegen_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_stats_,
                                        kRegisterAllocationZoneName),
        register_allocation_zone_(register_allocation_zone_scope_.zone()),
        jump_optimization_info_(jump_opt),
        assembler_options_(assembler_options),
        profile_data_(profile_data) {
    if (jsgraph) {
      jsgraph_ = jsgraph;
      simplified_ = jsgraph->simplified();
      machine_ = jsgraph->machine();
      common_ = jsgraph->common();
      javascript_ = jsgraph->javascript();
    } else {
      simplified_ = graph_zone_->New<SimplifiedOperatorBuilder>(graph_zone_);
      machine_ = graph_zone_->New<MachineOperatorBuilder>(
          graph_zone_, MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags(),
          InstructionSelector::AlignmentRequirements());
      common_ = graph_zone_->New<CommonOperatorBuilder>(graph_zone_);
      javascript_ = graph_zone_->New<JSOperatorBuilder>(graph_zone_);
      jsgraph_ = graph_zone_->New<JSGraph>(isolate_, graph_, common_,
                                           javascript_, simplified_, machine_);
    }
  }

  // For register allocation testing entry point.
  PipelineData(ZoneStats* zone_stats, OptimizedCompilationInfo* info,
               Isolate* isolate, InstructionSequence* sequence)
      : isolate_(isolate),
        allocator_(isolate->allocator()),
        info_(info),
        debug_name_(info_->GetDebugName()),
        zone_stats_(zone_stats),
        graph_zone_scope_(zone_stats_, kGraphZoneName, kCompressGraphZone),
        instruction_zone_scope_(zone_stats_, kInstructionZoneName),
        instruction_zone_(sequence->zone()),
        sequence_(sequence),
        codegen_zone_scope_(zone_stats_, kCodegenZoneName),
        codegen_zone_(codegen_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_stats_,
                                        kRegisterAllocationZoneName),
        register_allocation_zone_(register_allocation_zone_scope_.zone()),
        assembler_options_(AssemblerOptions::Default(isolate)) {}

  ~PipelineData() {
    // Must happen before zones are destroyed.
    delete code_generator_;
    code_generator_ = nullptr;
    DeleteTyper();
    DeleteRegisterAllocationZone();
    DeleteInstructionZone();
    DeleteCodegenZone();
    DeleteGraphZone();
  }

  PipelineData(const PipelineData&) = delete;
  PipelineData& operator=(const PipelineData&) = delete;

  Isolate* isolate() const { return isolate_; }
  AccountingAllocator* allocator() const { return allocator_; }
  OptimizedCompilationInfo* info() const { return info_; }
  ZoneStats* zone_stats() const { return zone_stats_; }
  CompilationDependencies* dependencies() const { return dependencies_; }
  PipelineStatistics* pipeline_statistics() { return pipeline_statistics_; }
  OsrHelper* osr_helper() { return &(*osr_helper_); }
  bool compilation_failed() const { return compilation_failed_; }
  void set_compilation_failed() { compilation_failed_ = true; }

  bool verify_graph() const { return verify_graph_; }
  void set_verify_graph(bool value) { verify_graph_ = value; }

  MaybeHandle<Code> code() { return code_; }
  void set_code(MaybeHandle<Code> code) {
    DCHECK(code_.is_null());
    code_ = code;
  }

  CodeGenerator* code_generator() const { return code_generator_; }

  // RawMachineAssembler generally produces graphs which cannot be verified.
  bool MayHaveUnverifiableGraph() const { return may_have_unverifiable_graph_; }

  Zone* graph_zone() const { return graph_zone_; }
  Graph* graph() const { return graph_; }
  SourcePositionTable* source_positions() const { return source_positions_; }
  NodeOriginTable* node_origins() const { return node_origins_; }
  MachineOperatorBuilder* machine() const { return machine_; }
  CommonOperatorBuilder* common() const { return common_; }
  JSOperatorBuilder* javascript() const { return javascript_; }
  JSGraph* jsgraph() const { return jsgraph_; }
  MachineGraph* mcgraph() const { return mcgraph_; }
  Handle<NativeContext> native_context() const {
    return handle(info()->native_context(), isolate());
  }
  Handle<JSGlobalObject> global_object() const {
    return handle(info()->global_object(), isolate());
  }

  JSHeapBroker* broker() const { return broker_; }
  std::unique_ptr<JSHeapBroker> ReleaseBroker() {
    std::unique_ptr<JSHeapBroker> broker(broker_);
    broker_ = nullptr;
    return broker;
  }

  Schedule* schedule() const { return schedule_; }
  void set_schedule(Schedule* schedule) {
    DCHECK(!schedule_);
    schedule_ = schedule;
  }
  void reset_schedule() { schedule_ = nullptr; }

  ObserveNodeManager* observe_node_manager() const {
    return observe_node_manager_;
  }

  Zone* instruction_zone() const { return instruction_zone_; }
  Zone* codegen_zone() const { return codegen_zone_; }
  InstructionSequence* sequence() const { return sequence_; }
  Frame* frame() const { return frame_; }

  Zone* register_allocation_zone() const { return register_allocation_zone_; }

  RegisterAllocationData* register_allocation_data() const {
    return register_allocation_data_;
  }
  TopTierRegisterAllocationData* top_tier_register_allocation_data() const {
    return TopTierRegisterAllocationData::cast(register_allocation_data_);
  }
  MidTierRegisterAllocationData* mid_tier_register_allocator_data() const {
    return MidTierRegisterAllocationData::cast(register_allocation_data_);
  }

  std::string const& source_position_output() const {
    return source_position_output_;
  }
  void set_source_position_output(std::string const& source_position_output) {
    source_position_output_ = source_position_output;
  }

  JumpOptimizationInfo* jump_optimization_info() const {
    return jump_optimization_info_;
  }

  const AssemblerOptions& assembler_options() const {
    return assembler_options_;
  }

  void ChooseSpecializationContext() {
    if (info()->function_context_specializing()) {
      DCHECK(info()->has_context());
      specialization_context_ =
          Just(OuterContext(handle(info()->context(), isolate()), 0));
    } else {
      specialization_context_ = GetModuleContext(info()->closure());
    }
  }

  Maybe<OuterContext> specialization_context() const {
    return specialization_context_;
  }

  size_t* address_of_max_unoptimized_frame_height() {
    return &max_unoptimized_frame_height_;
  }
  size_t max_unoptimized_frame_height() const {
    return max_unoptimized_frame_height_;
  }
  size_t* address_of_max_pushed_argument_count() {
    return &max_pushed_argument_count_;
  }
  size_t max_pushed_argument_count() const {
    return max_pushed_argument_count_;
  }

  CodeTracer* GetCodeTracer() const {
    return wasm_engine_ == nullptr ? isolate_->GetCodeTracer()
                                   : wasm_engine_->GetCodeTracer();
  }

  Typer* CreateTyper() {
    DCHECK_NULL(typer_);
    typer_ =
        new Typer(broker(), typer_flags_, graph(), &info()->tick_counter());
    return typer_;
  }

  void AddTyperFlag(Typer::Flag flag) {
    DCHECK_NULL(typer_);
    typer_flags_ |= flag;
  }

  void DeleteTyper() {
    delete typer_;
    typer_ = nullptr;
  }

  void DeleteGraphZone() {
    if (graph_zone_ == nullptr) return;
    graph_zone_scope_.Destroy();
    graph_zone_ = nullptr;
    graph_ = nullptr;
    source_positions_ = nullptr;
    node_origins_ = nullptr;
    simplified_ = nullptr;
    machine_ = nullptr;
    common_ = nullptr;
    javascript_ = nullptr;
    jsgraph_ = nullptr;
    mcgraph_ = nullptr;
    schedule_ = nullptr;
  }

  void DeleteInstructionZone() {
    if (instruction_zone_ == nullptr) return;
    instruction_zone_scope_.Destroy();
    instruction_zone_ = nullptr;
    sequence_ = nullptr;
  }

  void DeleteCodegenZone() {
    if (codegen_zone_ == nullptr) return;
    codegen_zone_scope_.Destroy();
    codegen_zone_ = nullptr;
    dependencies_ = nullptr;
    delete broker_;
    broker_ = nullptr;
    frame_ = nullptr;
  }

  void DeleteRegisterAllocationZone() {
    if (register_allocation_zone_ == nullptr) return;
    register_allocation_zone_scope_.Destroy();
    register_allocation_zone_ = nullptr;
    register_allocation_data_ = nullptr;
  }

  void InitializeInstructionSequence(const CallDescriptor* call_descriptor) {
    DCHECK_NULL(sequence_);
    InstructionBlocks* instruction_blocks =
        InstructionSequence::InstructionBlocksFor(instruction_zone(),
                                                  schedule());
    sequence_ = instruction_zone()->New<InstructionSequence>(
        isolate(), instruction_zone(), instruction_blocks);
    if (call_descriptor && call_descriptor->RequiresFrameAsIncoming()) {
      sequence_->instruction_blocks()[0]->mark_needs_frame();
    } else {
      DCHECK_EQ(0u, call_descriptor->CalleeSavedFPRegisters());
      DCHECK_EQ(0u, call_descriptor->CalleeSavedRegisters());
    }
  }

  void InitializeFrameData(CallDescriptor* call_descriptor) {
    DCHECK_NULL(frame_);
    int fixed_frame_size = 0;
    if (call_descriptor != nullptr) {
      fixed_frame_size =
          call_descriptor->CalculateFixedFrameSize(info()->code_kind());
    }
    frame_ = codegen_zone()->New<Frame>(fixed_frame_size);
    if (osr_helper_.has_value()) osr_helper()->SetupFrame(frame());
  }

  void InitializeTopTierRegisterAllocationData(
      const RegisterConfiguration* config, CallDescriptor* call_descriptor,
      RegisterAllocationFlags flags) {
    DCHECK_NULL(register_allocation_data_);
    register_allocation_data_ =
        register_allocation_zone()->New<TopTierRegisterAllocationData>(
            config, register_allocation_zone(), frame(), sequence(), flags,
            &info()->tick_counter(), debug_name());
  }

  void InitializeMidTierRegisterAllocationData(
      const RegisterConfiguration* config, CallDescriptor* call_descriptor) {
    DCHECK_NULL(register_allocation_data_);
    register_allocation_data_ =
        register_allocation_zone()->New<MidTierRegisterAllocationData>(
            config, register_allocation_zone(), frame(), sequence(),
            &info()->tick_counter(), debug_name());
  }

  void InitializeOsrHelper() {
    DCHECK(!osr_helper_.has_value());
    osr_helper_.emplace(info());
  }

  void set_start_source_position(int position) {
    DCHECK_EQ(start_source_position_, kNoSourcePosition);
    start_source_position_ = position;
  }

  void InitializeCodeGenerator(Linkage* linkage,
                               std::unique_ptr<AssemblerBuffer> buffer) {
    DCHECK_NULL(code_generator_);
    code_generator_ = new CodeGenerator(
        codegen_zone(), frame(), linkage, sequence(), info(), isolate(),
        osr_helper_, start_source_position_, jump_optimization_info_,
        info()->GetPoisoningMitigationLevel(), assembler_options_,
        info_->builtin_index(), max_unoptimized_frame_height(),
        max_pushed_argument_count(), std::move(buffer),
        FLAG_trace_turbo_stack_accesses ? debug_name_.get() : nullptr);
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

  const char* debug_name() const { return debug_name_.get(); }

  bool roots_relative_addressing_enabled() {
    return roots_relative_addressing_enabled_;
  }

  const ProfileDataFromFile* profile_data() const { return profile_data_; }
  void set_profile_data(const ProfileDataFromFile* profile_data) {
    profile_data_ = profile_data;
  }

  // RuntimeCallStats that is only available during job execution but not
  // finalization.
  // TODO(delphick): Currently even during execution this can be nullptr, due to
  // JSToWasmWrapperCompilationUnit::Execute. Once a table can be extracted
  // there, this method can DCHECK that it is never nullptr.
  RuntimeCallStats* runtime_call_stats() const { return runtime_call_stats_; }
  void set_runtime_call_stats(RuntimeCallStats* stats) {
    runtime_call_stats_ = stats;
  }

  // Used to skip the "wasm-inlining" phase when there are no JS-to-Wasm calls.
  bool has_js_wasm_calls() const { return has_js_wasm_calls_; }
  void set_has_js_wasm_calls(bool has_js_wasm_calls) {
    has_js_wasm_calls_ = has_js_wasm_calls;
  }

 private:
  Isolate* const isolate_;
  wasm::WasmEngine* const wasm_engine_ = nullptr;
  AccountingAllocator* const allocator_;
  OptimizedCompilationInfo* const info_;
  std::unique_ptr<char[]> debug_name_;
  bool may_have_unverifiable_graph_ = true;
  ZoneStats* const zone_stats_;
  PipelineStatistics* pipeline_statistics_ = nullptr;
  bool compilation_failed_ = false;
  bool verify_graph_ = false;
  int start_source_position_ = kNoSourcePosition;
  base::Optional<OsrHelper> osr_helper_;
  MaybeHandle<Code> code_;
  CodeGenerator* code_generator_ = nullptr;
  Typer* typer_ = nullptr;
  Typer::Flags typer_flags_ = Typer::kNoFlags;
  bool roots_relative_addressing_enabled_ = false;

  // All objects in the following group of fields are allocated in graph_zone_.
  // They are all set to nullptr when the graph_zone_ is destroyed.
  ZoneStats::Scope graph_zone_scope_;
  Zone* graph_zone_ = nullptr;
  Graph* graph_ = nullptr;
  SourcePositionTable* source_positions_ = nullptr;
  NodeOriginTable* node_origins_ = nullptr;
  SimplifiedOperatorBuilder* simplified_ = nullptr;
  MachineOperatorBuilder* machine_ = nullptr;
  CommonOperatorBuilder* common_ = nullptr;
  JSOperatorBuilder* javascript_ = nullptr;
  JSGraph* jsgraph_ = nullptr;
  MachineGraph* mcgraph_ = nullptr;
  Schedule* schedule_ = nullptr;
  ObserveNodeManager* observe_node_manager_ = nullptr;

  // All objects in the following group of fields are allocated in
  // instruction_zone_. They are all set to nullptr when the instruction_zone_
  // is destroyed.
  ZoneStats::Scope instruction_zone_scope_;
  Zone* instruction_zone_;
  InstructionSequence* sequence_ = nullptr;

  // All objects in the following group of fields are allocated in
  // codegen_zone_. They are all set to nullptr when the codegen_zone_
  // is destroyed.
  ZoneStats::Scope codegen_zone_scope_;
  Zone* codegen_zone_;
  CompilationDependencies* dependencies_ = nullptr;
  JSHeapBroker* broker_ = nullptr;
  Frame* frame_ = nullptr;

  // All objects in the following group of fields are allocated in
  // register_allocation_zone_. They are all set to nullptr when the zone is
  // destroyed.
  ZoneStats::Scope register_allocation_zone_scope_;
  Zone* register_allocation_zone_;
  RegisterAllocationData* register_allocation_data_ = nullptr;

  // Source position output for --trace-turbo.
  std::string source_position_output_;

  JumpOptimizationInfo* jump_optimization_info_ = nullptr;
  AssemblerOptions assembler_options_;
  Maybe<OuterContext> specialization_context_ = Nothing<OuterContext>();

  // The maximal combined height of all inlined frames in their unoptimized
  // state, and the maximal number of arguments pushed during function calls.
  // Calculated during instruction selection, applied during code generation.
  size_t max_unoptimized_frame_height_ = 0;
  size_t max_pushed_argument_count_ = 0;

  RuntimeCallStats* runtime_call_stats_ = nullptr;
  const ProfileDataFromFile* profile_data_ = nullptr;

  bool has_js_wasm_calls_ = false;
};

class PipelineImpl final {
 public:
  explicit PipelineImpl(PipelineData* data) : data_(data) {}

  // Helpers for executing pipeline phases.
  template <typename Phase, typename... Args>
  void Run(Args&&... args);

  // Step A.1. Serialize the data needed for the compilation front-end.
  void Serialize();

  // Step A.2. Run the graph creation and initial optimization passes.
  bool CreateGraph();

  // Step B. Run the concurrent optimization passes.
  bool OptimizeGraph(Linkage* linkage);

  // Alternative step B. Run minimal concurrent optimization passes for
  // mid-tier.
  bool OptimizeGraphForMidTier(Linkage* linkage);

  // Substep B.1. Produce a scheduled graph.
  void ComputeScheduledGraph();

  // Substep B.2. Select instructions from a scheduled graph.
  bool SelectInstructions(Linkage* linkage);

  // Step C. Run the code assembly pass.
  void AssembleCode(Linkage* linkage,
                    std::unique_ptr<AssemblerBuffer> buffer = {});

  // Step D. Run the code finalization pass.
  MaybeHandle<Code> FinalizeCode(bool retire_broker = true);

  // Step E. Install any code dependencies.
  bool CommitDependencies(Handle<Code> code);

  void VerifyGeneratedCodeIsIdempotent();
  void RunPrintAndVerify(const char* phase, bool untyped = false);
  bool SelectInstructionsAndAssemble(CallDescriptor* call_descriptor);
  MaybeHandle<Code> GenerateCode(CallDescriptor* call_descriptor);
  void AllocateRegistersForTopTier(const RegisterConfiguration* config,
                                   CallDescriptor* call_descriptor,
                                   bool run_verifier);
  void AllocateRegistersForMidTier(const RegisterConfiguration* config,
                                   CallDescriptor* call_descriptor,
                                   bool run_verifier);

  OptimizedCompilationInfo* info() const;
  Isolate* isolate() const;
  CodeGenerator* code_generator() const;

  ObserveNodeManager* observe_node_manager() const;

 private:
  PipelineData* const data_;
};

namespace {

class SourcePositionWrapper final : public Reducer {
 public:
  SourcePositionWrapper(Reducer* reducer, SourcePositionTable* table)
      : reducer_(reducer), table_(table) {}
  ~SourcePositionWrapper() final = default;
  SourcePositionWrapper(const SourcePositionWrapper&) = delete;
  SourcePositionWrapper& operator=(const SourcePositionWrapper&) = delete;

  const char* reducer_name() const override { return reducer_->reducer_name(); }

  Reduction Reduce(Node* node) final {
    SourcePosition const pos = table_->GetSourcePosition(node);
    SourcePositionTable::Scope position(table_, pos);
    return reducer_->Reduce(node, nullptr);
  }

  void Finalize() final { reducer_->Finalize(); }

 private:
  Reducer* const reducer_;
  SourcePositionTable* const table_;
};

class NodeOriginsWrapper final : public Reducer {
 public:
  NodeOriginsWrapper(Reducer* reducer, NodeOriginTable* table)
      : reducer_(reducer), table_(table) {}
  ~NodeOriginsWrapper() final = default;
  NodeOriginsWrapper(const NodeOriginsWrapper&) = delete;
  NodeOriginsWrapper& operator=(const NodeOriginsWrapper&) = delete;

  const char* reducer_name() const override { return reducer_->reducer_name(); }

  Reduction Reduce(Node* node) final {
    NodeOriginTable::Scope position(table_, reducer_name(), node);
    return reducer_->Reduce(node, nullptr);
  }

  void Finalize() final { reducer_->Finalize(); }

 private:
  Reducer* const reducer_;
  NodeOriginTable* const table_;
};

class V8_NODISCARD PipelineRunScope {
 public:
  PipelineRunScope(
      PipelineData* data, const char* phase_name,
      RuntimeCallCounterId runtime_call_counter_id,
      RuntimeCallStats::CounterMode counter_mode = RuntimeCallStats::kExact)
      : phase_scope_(data->pipeline_statistics(), phase_name),
        zone_scope_(data->zone_stats(), phase_name),
        origin_scope_(data->node_origins(), phase_name),
        runtime_call_timer_scope(data->runtime_call_stats(),
                                 runtime_call_counter_id, counter_mode) {
    DCHECK_NOT_NULL(phase_name);
  }

  Zone* zone() { return zone_scope_.zone(); }

 private:
  PhaseScope phase_scope_;
  ZoneStats::Scope zone_scope_;
  NodeOriginTable::PhaseScope origin_scope_;
  RuntimeCallTimerScope runtime_call_timer_scope;
};

// LocalIsolateScope encapsulates the phase where persistent handles are
// attached to the LocalHeap inside {local_isolate}.
class V8_NODISCARD LocalIsolateScope {
 public:
  explicit LocalIsolateScope(JSHeapBroker* broker,
                             OptimizedCompilationInfo* info,
                             LocalIsolate* local_isolate)
      : broker_(broker), info_(info) {
    broker_->AttachLocalIsolate(info_, local_isolate);
    info_->tick_counter().AttachLocalHeap(local_isolate->heap());
  }

  ~LocalIsolateScope() {
    info_->tick_counter().DetachLocalHeap();
    broker_->DetachLocalIsolate(info_);
  }

 private:
  JSHeapBroker* broker_;
  OptimizedCompilationInfo* info_;
};

void PrintFunctionSource(OptimizedCompilationInfo* info, Isolate* isolate,
                         int source_id, Handle<SharedFunctionInfo> shared) {
  if (!shared->script().IsUndefined(isolate)) {
    Handle<Script> script(Script::cast(shared->script()), isolate);

    if (!script->source().IsUndefined(isolate)) {
      CodeTracer::StreamScope tracing_scope(isolate->GetCodeTracer());
      Object source_name = script->name();
      auto& os = tracing_scope.stream();
      os << "--- FUNCTION SOURCE (";
      if (source_name.IsString()) {
        os << String::cast(source_name).ToCString().get() << ":";
      }
      os << shared->DebugNameCStr().get() << ") id{";
      os << info->optimization_id() << "," << source_id << "} start{";
      os << shared->StartPosition() << "} ---\n";
      {
        DisallowGarbageCollection no_gc;
        int start = shared->StartPosition();
        int len = shared->EndPosition() - start;
        SubStringRange source(String::cast(script->source()), no_gc, start,
                              len);
        for (const auto& c : source) {
          os << AsReversiblyEscapedUC16(c);
        }
      }

      os << "\n--- END ---\n";
    }
  }
}

// Print information for the given inlining: which function was inlined and
// where the inlining occurred.
void PrintInlinedFunctionInfo(
    OptimizedCompilationInfo* info, Isolate* isolate, int source_id,
    int inlining_id, const OptimizedCompilationInfo::InlinedFunctionHolder& h) {
  CodeTracer::StreamScope tracing_scope(isolate->GetCodeTracer());
  auto& os = tracing_scope.stream();
  os << "INLINE (" << h.shared_info->DebugNameCStr().get() << ") id{"
     << info->optimization_id() << "," << source_id << "} AS " << inlining_id
     << " AT ";
  const SourcePosition position = h.position.position;
  if (position.IsKnown()) {
    os << "<" << position.InliningId() << ":" << position.ScriptOffset() << ">";
  } else {
    os << "<?>";
  }
  os << std::endl;
}

// Print the source of all functions that participated in this optimizing
// compilation. For inlined functions print source position of their inlining.
void PrintParticipatingSource(OptimizedCompilationInfo* info,
                              Isolate* isolate) {
  SourceIdAssigner id_assigner(info->inlined_functions().size());
  PrintFunctionSource(info, isolate, -1, info->shared_info());
  const auto& inlined = info->inlined_functions();
  for (unsigned id = 0; id < inlined.size(); id++) {
    const int source_id = id_assigner.GetIdFor(inlined[id].shared_info);
    PrintFunctionSource(info, isolate, source_id, inlined[id].shared_info);
    PrintInlinedFunctionInfo(info, isolate, source_id, id, inlined[id]);
  }
}

// Print the code after compiling it.
void PrintCode(Isolate* isolate, Handle<Code> code,
               OptimizedCompilationInfo* info) {
  if (FLAG_print_opt_source && info->IsOptimizing()) {
    PrintParticipatingSource(info, isolate);
  }

#ifdef ENABLE_DISASSEMBLER
  const bool print_code =
      FLAG_print_code ||
      (info->IsOptimizing() && FLAG_print_opt_code &&
       info->shared_info()->PassesFilter(FLAG_print_opt_code_filter)) ||
      (info->IsNativeContextIndependent() && FLAG_print_nci_code);
  if (print_code) {
    std::unique_ptr<char[]> debug_name = info->GetDebugName();
    CodeTracer::StreamScope tracing_scope(isolate->GetCodeTracer());
    auto& os = tracing_scope.stream();

    // Print the source code if available.
    const bool print_source = info->IsOptimizing();
    if (print_source) {
      Handle<SharedFunctionInfo> shared = info->shared_info();
      if (shared->script().IsScript() &&
          !Script::cast(shared->script()).source().IsUndefined(isolate)) {
        os << "--- Raw source ---\n";
        StringCharacterStream stream(
            String::cast(Script::cast(shared->script()).source()),
            shared->StartPosition());
        // fun->end_position() points to the last character in the stream. We
        // need to compensate by adding one to calculate the length.
        int source_len = shared->EndPosition() - shared->StartPosition() + 1;
        for (int i = 0; i < source_len; i++) {
          if (stream.HasMore()) {
            os << AsReversiblyEscapedUC16(stream.GetNext());
          }
        }
        os << "\n\n";
      }
    }
    if (info->IsOptimizing()) {
      os << "--- Optimized code ---\n"
         << "optimization_id = " << info->optimization_id() << "\n";
    } else {
      os << "--- Code ---\n";
    }
    if (print_source) {
      Handle<SharedFunctionInfo> shared = info->shared_info();
      os << "source_position = " << shared->StartPosition() << "\n";
    }
    code->Disassemble(debug_name.get(), os, isolate);
    os << "--- End code ---\n";
  }
#endif  // ENABLE_DISASSEMBLER
}

void TraceScheduleAndVerify(OptimizedCompilationInfo* info, PipelineData* data,
                            Schedule* schedule, const char* phase_name) {
  if (info->trace_turbo_json()) {
    UnparkedScopeIfNeeded scope(data->broker());
    AllowHandleDereference allow_deref;
    TurboJsonFile json_of(info, std::ios_base::app);
    json_of << "{\"name\":\"" << phase_name << "\",\"type\":\"schedule\""
            << ",\"data\":\"";
    std::stringstream schedule_stream;
    schedule_stream << *schedule;
    std::string schedule_string(schedule_stream.str());
    for (const auto& c : schedule_string) {
      json_of << AsEscapedUC16ForJSON(c);
    }
    json_of << "\"},\n";
  }
  if (info->trace_turbo_graph() || FLAG_trace_turbo_scheduler) {
    UnparkedScopeIfNeeded scope(data->broker());
    AllowHandleDereference allow_deref;
    CodeTracer::StreamScope tracing_scope(data->GetCodeTracer());
    tracing_scope.stream()
        << "-- Schedule --------------------------------------\n"
        << *schedule;
  }

  if (FLAG_turbo_verify) ScheduleVerifier::Run(schedule);
}

void AddReducer(PipelineData* data, GraphReducer* graph_reducer,
                Reducer* reducer) {
  if (data->info()->source_positions()) {
    SourcePositionWrapper* const wrapper =
        data->graph_zone()->New<SourcePositionWrapper>(
            reducer, data->source_positions());
    reducer = wrapper;
  }
  if (data->info()->trace_turbo_json()) {
    NodeOriginsWrapper* const wrapper =
        data->graph_zone()->New<NodeOriginsWrapper>(reducer,
                                                    data->node_origins());
    reducer = wrapper;
  }

  graph_reducer->AddReducer(reducer);
}

PipelineStatistics* CreatePipelineStatistics(Handle<Script> script,
                                             OptimizedCompilationInfo* info,
                                             Isolate* isolate,
                                             ZoneStats* zone_stats) {
  PipelineStatistics* pipeline_statistics = nullptr;

  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("v8.turbofan"),
                                     &tracing_enabled);
  if (tracing_enabled || FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics =
        new PipelineStatistics(info, isolate->GetTurboStatistics(), zone_stats);
    pipeline_statistics->BeginPhaseKind("V8.TFInitializing");
  }

  if (info->trace_turbo_json()) {
    TurboJsonFile json_of(info, std::ios_base::trunc);
    json_of << "{\"function\" : ";
    JsonPrintFunctionSource(json_of, -1, info->GetDebugName(), script, isolate,
                            info->shared_info());
    json_of << ",\n\"phases\":[";
  }

  return pipeline_statistics;
}

PipelineStatistics* CreatePipelineStatistics(
    wasm::WasmEngine* wasm_engine, wasm::FunctionBody function_body,
    const wasm::WasmModule* wasm_module, OptimizedCompilationInfo* info,
    ZoneStats* zone_stats) {
  PipelineStatistics* pipeline_statistics = nullptr;

  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.wasm.turbofan"), &tracing_enabled);
  if (tracing_enabled || FLAG_turbo_stats_wasm) {
    pipeline_statistics = new PipelineStatistics(
        info, wasm_engine->GetOrCreateTurboStatistics(), zone_stats);
    pipeline_statistics->BeginPhaseKind("V8.WasmInitializing");
  }

  if (info->trace_turbo_json()) {
    TurboJsonFile json_of(info, std::ios_base::trunc);
    std::unique_ptr<char[]> function_name = info->GetDebugName();
    json_of << "{\"function\":\"" << function_name.get() << "\", \"source\":\"";
    AccountingAllocator allocator;
    std::ostringstream disassembly;
    std::vector<int> source_positions;
    wasm::PrintRawWasmCode(&allocator, function_body, wasm_module,
                           wasm::kPrintLocals, disassembly, &source_positions);
    for (const auto& c : disassembly.str()) {
      json_of << AsEscapedUC16ForJSON(c);
    }
    json_of << "\",\n\"sourceLineToBytecodePosition\" : [";
    bool insert_comma = false;
    for (auto val : source_positions) {
      if (insert_comma) {
        json_of << ", ";
      }
      json_of << val;
      insert_comma = true;
    }
    json_of << "],\n\"phases\":[";
  }

  return pipeline_statistics;
}

}  // namespace

class PipelineCompilationJob final : public OptimizedCompilationJob {
 public:
  PipelineCompilationJob(Isolate* isolate,
                         Handle<SharedFunctionInfo> shared_info,
                         Handle<JSFunction> function, BytecodeOffset osr_offset,
                         JavaScriptFrame* osr_frame, CodeKind code_kind);
  ~PipelineCompilationJob() final;
  PipelineCompilationJob(const PipelineCompilationJob&) = delete;
  PipelineCompilationJob& operator=(const PipelineCompilationJob&) = delete;

 protected:
  Status PrepareJobImpl(Isolate* isolate) final;
  Status ExecuteJobImpl(RuntimeCallStats* stats,
                        LocalIsolate* local_isolate) final;
  Status FinalizeJobImpl(Isolate* isolate) final;

  // Registers weak object to optimized code dependencies.
  void RegisterWeakObjectsInOptimizedCode(Isolate* isolate,
                                          Handle<NativeContext> context,
                                          Handle<Code> code);

 private:
  Zone zone_;
  ZoneStats zone_stats_;
  OptimizedCompilationInfo compilation_info_;
  std::unique_ptr<PipelineStatistics> pipeline_statistics_;
  PipelineData data_;
  PipelineImpl pipeline_;
  Linkage* linkage_;
};

namespace {

bool ShouldUseConcurrentInlining(CodeKind code_kind, bool is_osr) {
  if (is_osr) return false;
  return code_kind == CodeKind::TURBOPROP || FLAG_concurrent_inlining;
}

}  // namespace

PipelineCompilationJob::PipelineCompilationJob(
    Isolate* isolate, Handle<SharedFunctionInfo> shared_info,
    Handle<JSFunction> function, BytecodeOffset osr_offset,
    JavaScriptFrame* osr_frame, CodeKind code_kind)
    // Note that the OptimizedCompilationInfo is not initialized at the time
    // we pass it to the CompilationJob constructor, but it is not
    // dereferenced there.
    : OptimizedCompilationJob(&compilation_info_, "TurboFan"),
      zone_(function->GetIsolate()->allocator(),
            kPipelineCompilationJobZoneName),
      zone_stats_(function->GetIsolate()->allocator()),
      compilation_info_(&zone_, function->GetIsolate(), shared_info, function,
                        code_kind),
      pipeline_statistics_(CreatePipelineStatistics(
          handle(Script::cast(shared_info->script()), isolate),
          compilation_info(), function->GetIsolate(), &zone_stats_)),
      data_(&zone_stats_, function->GetIsolate(), compilation_info(),
            pipeline_statistics_.get(),
            ShouldUseConcurrentInlining(code_kind, !osr_offset.IsNone())),
      pipeline_(&data_),
      linkage_(nullptr) {
  compilation_info_.SetOptimizingForOsr(osr_offset, osr_frame);
}

PipelineCompilationJob::~PipelineCompilationJob() = default;

namespace {
// Ensure that the RuntimeStats table is set on the PipelineData for
// duration of the job phase and unset immediately afterwards. Each job
// needs to set the correct RuntimeCallStats table depending on whether it
// is running on a background or foreground thread.
class V8_NODISCARD PipelineJobScope {
 public:
  PipelineJobScope(PipelineData* data, RuntimeCallStats* stats) : data_(data) {
    data_->set_runtime_call_stats(stats);
  }

  ~PipelineJobScope() { data_->set_runtime_call_stats(nullptr); }

 private:
  PipelineData* data_;
};
}  // namespace

PipelineCompilationJob::Status PipelineCompilationJob::PrepareJobImpl(
    Isolate* isolate) {
  // Ensure that the RuntimeCallStats table of main thread is available for
  // phases happening during PrepareJob.
  PipelineJobScope scope(&data_, isolate->counters()->runtime_call_stats());

  if (compilation_info()->bytecode_array()->length() >
      FLAG_max_optimized_bytecode_size) {
    return AbortOptimization(BailoutReason::kFunctionTooBig);
  }

  if (!FLAG_always_opt && !compilation_info()->IsNativeContextIndependent()) {
    compilation_info()->set_bailout_on_uninitialized();
  }
  if (FLAG_turbo_loop_peeling) {
    compilation_info()->set_loop_peeling();
  }
  if (FLAG_turbo_inlining && !compilation_info()->IsTurboprop() &&
      !compilation_info()->IsNativeContextIndependent()) {
    compilation_info()->set_inlining();
  }

  // This is the bottleneck for computing and setting poisoning level in the
  // optimizing compiler.
  PoisoningMitigationLevel load_poisoning =
      PoisoningMitigationLevel::kDontPoison;
  if (FLAG_untrusted_code_mitigations) {
    // For full mitigations, this can be changed to
    // PoisoningMitigationLevel::kPoisonAll.
    load_poisoning = PoisoningMitigationLevel::kPoisonCriticalOnly;
  }
  compilation_info()->SetPoisoningMitigationLevel(load_poisoning);

  if (FLAG_turbo_allocation_folding) {
    compilation_info()->set_allocation_folding();
  }

  // Determine whether to specialize the code for the function's context.
  // We can't do this in the case of OSR, because we want to cache the
  // generated code on the native context keyed on SharedFunctionInfo.
  // We also can't do this for native context independent code (yet).
  // TODO(mythria): Check if it is better to key the OSR cache on JSFunction and
  // allow context specialization for OSR code.
  if (compilation_info()->closure()->raw_feedback_cell().map() ==
          ReadOnlyRoots(isolate).one_closure_cell_map() &&
      !compilation_info()->is_osr() &&
      !compilation_info()->IsNativeContextIndependent() &&
      !compilation_info()->IsTurboprop()) {
    compilation_info()->set_function_context_specializing();
    data_.ChooseSpecializationContext();
  }

  if (compilation_info()->source_positions()) {
    SharedFunctionInfo::EnsureSourcePositionsAvailable(
        isolate, compilation_info()->shared_info());
  }

  data_.set_start_source_position(
      compilation_info()->shared_info()->StartPosition());

  linkage_ = compilation_info()->zone()->New<Linkage>(
      Linkage::ComputeIncoming(compilation_info()->zone(), compilation_info()));

  if (compilation_info()->is_osr()) data_.InitializeOsrHelper();

  pipeline_.Serialize();

  if (!data_.broker()->is_concurrent_inlining()) {
    if (!pipeline_.CreateGraph()) {
      CHECK(!isolate->has_pending_exception());
      return AbortOptimization(BailoutReason::kGraphBuildingFailed);
    }
  }

  if (FLAG_turbo_direct_heap_access) {
    isolate->heap()->PublishPendingAllocations();
  }

  return SUCCEEDED;
}

PipelineCompilationJob::Status PipelineCompilationJob::ExecuteJobImpl(
    RuntimeCallStats* stats, LocalIsolate* local_isolate) {
  // Ensure that the RuntimeCallStats table is only available during execution
  // and not during finalization as that might be on a different thread.
  PipelineJobScope scope(&data_, stats);
  LocalIsolateScope local_isolate_scope(data_.broker(), data_.info(),
                                        local_isolate);

  if (data_.broker()->is_concurrent_inlining()) {
    if (!pipeline_.CreateGraph()) {
      return AbortOptimization(BailoutReason::kGraphBuildingFailed);
    }
  }

  // We selectively Unpark inside OptimizeGraph*.
  bool success;
  if (compilation_info_.code_kind() == CodeKind::TURBOPROP) {
    success = pipeline_.OptimizeGraphForMidTier(linkage_);
  } else {
    success = pipeline_.OptimizeGraph(linkage_);
  }
  if (!success) return FAILED;

  pipeline_.AssembleCode(linkage_);

  return SUCCEEDED;
}

PipelineCompilationJob::Status PipelineCompilationJob::FinalizeJobImpl(
    Isolate* isolate) {
  // Ensure that the RuntimeCallStats table of main thread is available for
  // phases happening during PrepareJob.
  PipelineJobScope scope(&data_, isolate->counters()->runtime_call_stats());
  RuntimeCallTimerScope runtimeTimer(
      isolate, RuntimeCallCounterId::kOptimizeFinalizePipelineJob);
  MaybeHandle<Code> maybe_code = pipeline_.FinalizeCode();
  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    if (compilation_info()->bailout_reason() == BailoutReason::kNoReason) {
      return AbortOptimization(BailoutReason::kCodeGenerationFailed);
    }
    return FAILED;
  }
  if (!pipeline_.CommitDependencies(code)) {
    return RetryOptimization(BailoutReason::kBailedOutDueToDependencyChange);
  }

  compilation_info()->SetCode(code);
  Handle<NativeContext> context(compilation_info()->native_context(), isolate);
  if (CodeKindCanDeoptimize(code->kind())) context->AddOptimizedCode(*code);
  RegisterWeakObjectsInOptimizedCode(isolate, context, code);
  return SUCCEEDED;
}

void PipelineCompilationJob::RegisterWeakObjectsInOptimizedCode(
    Isolate* isolate, Handle<NativeContext> context, Handle<Code> code) {
  std::vector<Handle<Map>> maps;
  DCHECK(code->is_optimized_code());
  {
    DisallowGarbageCollection no_gc;
    int const mode_mask = RelocInfo::EmbeddedObjectModeMask();
    for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
      DCHECK(RelocInfo::IsEmbeddedObjectMode(it.rinfo()->rmode()));
      if (code->IsWeakObjectInOptimizedCode(it.rinfo()->target_object())) {
        Handle<HeapObject> object(HeapObject::cast(it.rinfo()->target_object()),
                                  isolate);
        if (object->IsMap()) {
          maps.push_back(Handle<Map>::cast(object));
        }
      }
    }
  }
  for (Handle<Map> map : maps) {
    isolate->heap()->AddRetainedMap(context, map);
  }
  code->set_can_have_weak_objects(true);
}

class WasmHeapStubCompilationJob final : public OptimizedCompilationJob {
 public:
  WasmHeapStubCompilationJob(Isolate* isolate, wasm::WasmEngine* wasm_engine,
                             CallDescriptor* call_descriptor,
                             std::unique_ptr<Zone> zone, Graph* graph,
                             CodeKind kind, std::unique_ptr<char[]> debug_name,
                             const AssemblerOptions& options,
                             SourcePositionTable* source_positions)
      // Note that the OptimizedCompilationInfo is not initialized at the time
      // we pass it to the CompilationJob constructor, but it is not
      // dereferenced there.
      : OptimizedCompilationJob(&info_, "TurboFan",
                                CompilationJob::State::kReadyToExecute),
        debug_name_(std::move(debug_name)),
        info_(CStrVector(debug_name_.get()), graph->zone(), kind),
        call_descriptor_(call_descriptor),
        zone_stats_(zone->allocator()),
        zone_(std::move(zone)),
        graph_(graph),
        data_(&zone_stats_, &info_, isolate, wasm_engine->allocator(), graph_,
              nullptr, nullptr, source_positions,
              zone_->New<NodeOriginTable>(graph_), nullptr, options, nullptr),
        pipeline_(&data_),
        wasm_engine_(wasm_engine) {}

  WasmHeapStubCompilationJob(const WasmHeapStubCompilationJob&) = delete;
  WasmHeapStubCompilationJob& operator=(const WasmHeapStubCompilationJob&) =
      delete;

 protected:
  Status PrepareJobImpl(Isolate* isolate) final;
  Status ExecuteJobImpl(RuntimeCallStats* stats,
                        LocalIsolate* local_isolate) final;
  Status FinalizeJobImpl(Isolate* isolate) final;

 private:
  std::unique_ptr<char[]> debug_name_;
  OptimizedCompilationInfo info_;
  CallDescriptor* call_descriptor_;
  ZoneStats zone_stats_;
  std::unique_ptr<Zone> zone_;
  Graph* graph_;
  PipelineData data_;
  PipelineImpl pipeline_;
  wasm::WasmEngine* wasm_engine_;
};

// static
std::unique_ptr<OptimizedCompilationJob>
Pipeline::NewWasmHeapStubCompilationJob(
    Isolate* isolate, wasm::WasmEngine* wasm_engine,
    CallDescriptor* call_descriptor, std::unique_ptr<Zone> zone, Graph* graph,
    CodeKind kind, std::unique_ptr<char[]> debug_name,
    const AssemblerOptions& options, SourcePositionTable* source_positions) {
  return std::make_unique<WasmHeapStubCompilationJob>(
      isolate, wasm_engine, call_descriptor, std::move(zone), graph, kind,
      std::move(debug_name), options, source_positions);
}

CompilationJob::Status WasmHeapStubCompilationJob::PrepareJobImpl(
    Isolate* isolate) {
  UNREACHABLE();
}

CompilationJob::Status WasmHeapStubCompilationJob::ExecuteJobImpl(
    RuntimeCallStats* stats, LocalIsolate* local_isolate) {
  std::unique_ptr<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics.reset(new PipelineStatistics(
        &info_, wasm_engine_->GetOrCreateTurboStatistics(), &zone_stats_));
    pipeline_statistics->BeginPhaseKind("V8.WasmStubCodegen");
  }
  if (info_.trace_turbo_json() || info_.trace_turbo_graph()) {
    CodeTracer::StreamScope tracing_scope(data_.GetCodeTracer());
    tracing_scope.stream()
        << "---------------------------------------------------\n"
        << "Begin compiling method " << info_.GetDebugName().get()
        << " using TurboFan" << std::endl;
  }
  if (info_.trace_turbo_graph()) {  // Simple textual RPO.
    StdoutStream{} << "-- wasm stub " << CodeKindToString(info_.code_kind())
                   << " graph -- " << std::endl
                   << AsRPO(*data_.graph());
  }

  if (info_.trace_turbo_json()) {
    TurboJsonFile json_of(&info_, std::ios_base::trunc);
    json_of << "{\"function\":\"" << info_.GetDebugName().get()
            << "\", \"source\":\"\",\n\"phases\":[";
  }
  pipeline_.RunPrintAndVerify("V8.WasmMachineCode", true);
  pipeline_.ComputeScheduledGraph();
  if (pipeline_.SelectInstructionsAndAssemble(call_descriptor_)) {
    return CompilationJob::SUCCEEDED;
  }
  return CompilationJob::FAILED;
}

CompilationJob::Status WasmHeapStubCompilationJob::FinalizeJobImpl(
    Isolate* isolate) {
  Handle<Code> code;
  if (!pipeline_.FinalizeCode(call_descriptor_).ToHandle(&code)) {
    V8::FatalProcessOutOfMemory(isolate,
                                "WasmHeapStubCompilationJob::FinalizeJobImpl");
  }
  if (pipeline_.CommitDependencies(code)) {
    info_.SetCode(code);
#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_opt_code) {
      CodeTracer::StreamScope tracing_scope(isolate->GetCodeTracer());
      code->Disassemble(compilation_info()->GetDebugName().get(),
                        tracing_scope.stream(), isolate);
    }
#endif
    return SUCCEEDED;
  }
  return FAILED;
}

template <typename Phase, typename... Args>
void PipelineImpl::Run(Args&&... args) {
  PipelineRunScope scope(this->data_, Phase::phase_name(),
                         Phase::kRuntimeCallCounterId, Phase::kCounterMode);
  Phase phase;
  phase.Run(this->data_, scope.zone(), std::forward<Args>(args)...);
}

#define DECL_PIPELINE_PHASE_CONSTANTS_HELPER(Name, Mode)        \
  static const char* phase_name() { return "V8.TF" #Name; }     \
  static constexpr RuntimeCallCounterId kRuntimeCallCounterId = \
      RuntimeCallCounterId::kOptimize##Name;                    \
  static constexpr RuntimeCallStats::CounterMode kCounterMode = Mode;

#define DECL_PIPELINE_PHASE_CONSTANTS(Name) \
  DECL_PIPELINE_PHASE_CONSTANTS_HELPER(Name, RuntimeCallStats::kThreadSpecific)

#define DECL_MAIN_THREAD_PIPELINE_PHASE_CONSTANTS(Name) \
  DECL_PIPELINE_PHASE_CONSTANTS_HELPER(Name, RuntimeCallStats::kExact)

struct GraphBuilderPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(BytecodeGraphBuilder)

  void Run(PipelineData* data, Zone* temp_zone) {
    BytecodeGraphBuilderFlags flags;
    if (data->info()->analyze_environment_liveness()) {
      flags |= BytecodeGraphBuilderFlag::kAnalyzeEnvironmentLiveness;
    }
    if (data->info()->bailout_on_uninitialized()) {
      flags |= BytecodeGraphBuilderFlag::kBailoutOnUninitialized;
    }

    JSFunctionRef closure(data->broker(), data->info()->closure());
    CallFrequency frequency(1.0f);
    BuildGraphFromBytecode(
        data->broker(), temp_zone, closure.shared(),
        closure.raw_feedback_cell(), data->info()->osr_offset(),
        data->jsgraph(), frequency, data->source_positions(),
        SourcePosition::kNotInlined, data->info()->code_kind(), flags,
        &data->info()->tick_counter(),
        ObserveNodeInfo{data->observe_node_manager(),
                        data->info()->node_observer()});
  }
};

struct InliningPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(Inlining)

  void Run(PipelineData* data, Zone* temp_zone) {
    OptimizedCompilationInfo* info = data->info();
    GraphReducer graph_reducer(temp_zone, data->graph(), &info->tick_counter(),
                               data->broker(), data->jsgraph()->Dead(),
                               data->observe_node_manager());
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    CheckpointElimination checkpoint_elimination(&graph_reducer);
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->broker(), data->common(),
                                         data->machine(), temp_zone);
    JSCallReducer::Flags call_reducer_flags = JSCallReducer::kNoFlags;
    if (data->info()->bailout_on_uninitialized()) {
      call_reducer_flags |= JSCallReducer::kBailoutOnUninitialized;
    }
    if (FLAG_turbo_inline_js_wasm_calls && data->info()->inlining()) {
      call_reducer_flags |= JSCallReducer::kInlineJSToWasmCalls;
    }
    JSCallReducer call_reducer(&graph_reducer, data->jsgraph(), data->broker(),
                               temp_zone, call_reducer_flags,
                               data->dependencies());
    JSContextSpecialization context_specialization(
        &graph_reducer, data->jsgraph(), data->broker(),
        data->specialization_context(),
        data->info()->function_context_specializing()
            ? data->info()->closure()
            : MaybeHandle<JSFunction>());
    JSNativeContextSpecialization::Flags flags =
        JSNativeContextSpecialization::kNoFlags;
    if (data->info()->bailout_on_uninitialized()) {
      flags |= JSNativeContextSpecialization::kBailoutOnUninitialized;
    }
    // Passing the OptimizedCompilationInfo's shared zone here as
    // JSNativeContextSpecialization allocates out-of-heap objects
    // that need to live until code generation.
    JSNativeContextSpecialization native_context_specialization(
        &graph_reducer, data->jsgraph(), data->broker(), flags,
        data->dependencies(), temp_zone, info->zone());
    JSInliningHeuristic inlining(
        &graph_reducer, temp_zone, data->info(), data->jsgraph(),
        data->broker(), data->source_positions(), JSInliningHeuristic::kJSOnly);

    JSIntrinsicLowering intrinsic_lowering(&graph_reducer, data->jsgraph(),
                                           data->broker());
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &checkpoint_elimination);
    AddReducer(data, &graph_reducer, &common_reducer);
    if (!data->info()->IsNativeContextIndependent()) {
      AddReducer(data, &graph_reducer, &native_context_specialization);
      AddReducer(data, &graph_reducer, &context_specialization);
    }
    AddReducer(data, &graph_reducer, &intrinsic_lowering);
    AddReducer(data, &graph_reducer, &call_reducer);
    if (data->info()->inlining()) {
      AddReducer(data, &graph_reducer, &inlining);
    }
    graph_reducer.ReduceGraph();
    info->set_inlined_bytecode_size(inlining.total_inlined_bytecode_size());

    // Skip the "wasm-inlining" phase if there are no Wasm functions calls.
    if (call_reducer.has_wasm_calls()) {
      data->set_has_js_wasm_calls(true);
    }
  }
};

struct WasmInliningPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(WasmInlining)
  void Run(PipelineData* data, Zone* temp_zone) {
    DCHECK(data->has_js_wasm_calls());

    OptimizedCompilationInfo* info = data->info();
    GraphReducer graph_reducer(temp_zone, data->graph(), &info->tick_counter(),
                               data->broker(), data->jsgraph()->Dead());
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->broker(), data->common(),
                                         data->machine(), temp_zone);
    JSInliningHeuristic inlining(&graph_reducer, temp_zone, data->info(),
                                 data->jsgraph(), data->broker(),
                                 data->source_positions(),
                                 JSInliningHeuristic::kWasmOnly);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &common_reducer);
    AddReducer(data, &graph_reducer, &inlining);
    graph_reducer.ReduceGraph();
  }
};

struct TyperPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(Typer)

  void Run(PipelineData* data, Zone* temp_zone, Typer* typer) {
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);

    // Make sure we always type True and False. Needed for escape analysis.
    roots.push_back(data->jsgraph()->TrueConstant());
    roots.push_back(data->jsgraph()->FalseConstant());

    LoopVariableOptimizer induction_vars(data->jsgraph()->graph(),
                                         data->common(), temp_zone);
    if (FLAG_turbo_loop_variable) induction_vars.Run();

    // The typer inspects heap objects, so we need to unpark the local heap.
    UnparkedScopeIfNeeded scope(data->broker());
    typer->Run(roots, &induction_vars);
  }
};

struct UntyperPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(Untyper)

  void Run(PipelineData* data, Zone* temp_zone) {
    class RemoveTypeReducer final : public Reducer {
     public:
      const char* reducer_name() const override { return "RemoveTypeReducer"; }
      Reduction Reduce(Node* node) final {
        if (NodeProperties::IsTyped(node)) {
          NodeProperties::RemoveType(node);
          return Changed(node);
        }
        return NoChange();
      }
    };

    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    for (Node* node : roots) {
      NodeProperties::RemoveType(node);
    }

    GraphReducer graph_reducer(
        temp_zone, data->graph(), &data->info()->tick_counter(), data->broker(),
        data->jsgraph()->Dead(), data->observe_node_manager());
    RemoveTypeReducer remove_type_reducer;
    AddReducer(data, &graph_reducer, &remove_type_reducer);
    graph_reducer.ReduceGraph();
  }
};

struct HeapBrokerInitializationPhase {
  DECL_MAIN_THREAD_PIPELINE_PHASE_CONSTANTS(HeapBrokerInitialization)

  void Run(PipelineData* data, Zone* temp_zone) {
    data->broker()->InitializeAndStartSerializing(data->native_context());
  }
};

struct CopyMetadataForConcurrentCompilePhase {
  DECL_MAIN_THREAD_PIPELINE_PHASE_CONSTANTS(SerializeMetadata)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(
        temp_zone, data->graph(), &data->info()->tick_counter(), data->broker(),
        data->jsgraph()->Dead(), data->observe_node_manager());
    JSHeapCopyReducer heap_copy_reducer(data->broker());
    AddReducer(data, &graph_reducer, &heap_copy_reducer);
    graph_reducer.ReduceGraph();

    // Some nodes that are no longer in the graph might still be in the cache.
    NodeVector cached_nodes(temp_zone);
    data->jsgraph()->GetCachedNodes(&cached_nodes);
    for (Node* const node : cached_nodes) graph_reducer.ReduceNode(node);
  }
};

struct SerializationPhase {
  DECL_MAIN_THREAD_PIPELINE_PHASE_CONSTANTS(Serialization)

  void Run(PipelineData* data, Zone* temp_zone) {
    SerializerForBackgroundCompilationFlags flags;
    if (data->info()->bailout_on_uninitialized()) {
      flags |= SerializerForBackgroundCompilationFlag::kBailoutOnUninitialized;
    }
    if (data->info()->source_positions()) {
      flags |= SerializerForBackgroundCompilationFlag::kCollectSourcePositions;
    }
    if (data->info()->analyze_environment_liveness()) {
      flags |=
          SerializerForBackgroundCompilationFlag::kAnalyzeEnvironmentLiveness;
    }
    if (data->info()->inlining()) {
      flags |= SerializerForBackgroundCompilationFlag::kEnableTurboInlining;
    }
    RunSerializerForBackgroundCompilation(
        data->zone_stats(), data->broker(), data->dependencies(),
        data->info()->closure(), flags, data->info()->osr_offset());
    if (data->specialization_context().IsJust()) {
      ContextRef(data->broker(),
                 data->specialization_context().FromJust().context);
    }
  }
};

struct TypedLoweringPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(TypedLowering)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(
        temp_zone, data->graph(), &data->info()->tick_counter(), data->broker(),
        data->jsgraph()->Dead(), data->observe_node_manager());
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    JSCreateLowering create_lowering(&graph_reducer, data->dependencies(),
                                     data->jsgraph(), data->broker(),
                                     temp_zone);
    JSTypedLowering typed_lowering(&graph_reducer, data->jsgraph(),
                                   data->broker(), temp_zone);
    ConstantFoldingReducer constant_folding_reducer(
        &graph_reducer, data->jsgraph(), data->broker());
    TypedOptimization typed_optimization(&graph_reducer, data->dependencies(),
                                         data->jsgraph(), data->broker());
    SimplifiedOperatorReducer simple_reducer(&graph_reducer, data->jsgraph(),
                                             data->broker());
    CheckpointElimination checkpoint_elimination(&graph_reducer);
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->broker(), data->common(),
                                         data->machine(), temp_zone);
    AddReducer(data, &graph_reducer, &dead_code_elimination);

    if (!data->info()->IsNativeContextIndependent()) {
      AddReducer(data, &graph_reducer, &create_lowering);
    }
    AddReducer(data, &graph_reducer, &constant_folding_reducer);
    AddReducer(data, &graph_reducer, &typed_lowering);
    AddReducer(data, &graph_reducer, &typed_optimization);
    AddReducer(data, &graph_reducer, &simple_reducer);
    AddReducer(data, &graph_reducer, &checkpoint_elimination);
    AddReducer(data, &graph_reducer, &common_reducer);

    // ConstantFoldingReducer, JSCreateLowering, JSTypedLowering, and
    // TypedOptimization access the heap.
    UnparkedScopeIfNeeded scope(data->broker());

    graph_reducer.ReduceGraph();
  }
};


struct EscapeAnalysisPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(EscapeAnalysis)

  void Run(PipelineData* data, Zone* temp_zone) {
    EscapeAnalysis escape_analysis(data->jsgraph(),
                                   &data->info()->tick_counter(), temp_zone);
    escape_analysis.ReduceGraph();

    GraphReducer reducer(temp_zone, data->graph(),
                         &data->info()->tick_counter(), data->broker(),
                         data->jsgraph()->Dead(), data->observe_node_manager());
    EscapeAnalysisReducer escape_reducer(&reducer, data->jsgraph(),
                                         escape_analysis.analysis_result(),
                                         temp_zone);

    AddReducer(data, &reducer, &escape_reducer);

    // EscapeAnalysisReducer accesses the heap.
    UnparkedScopeIfNeeded scope(data->broker());

    reducer.ReduceGraph();
    // TODO(turbofan): Turn this into a debug mode check once we have
    // confidence.
    escape_reducer.VerifyReplacement();
  }
};

struct TypeAssertionsPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(TypeAssertions)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(
        temp_zone, data->graph(), &data->info()->tick_counter(), data->broker(),
        data->jsgraph()->Dead(), data->observe_node_manager());
    AddTypeAssertionsReducer type_assertions(&graph_reducer, data->jsgraph(),
                                             temp_zone);
    AddReducer(data, &graph_reducer, &type_assertions);
    graph_reducer.ReduceGraph();
  }
};

struct SimplifiedLoweringPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(SimplifiedLowering)

  void Run(PipelineData* data, Zone* temp_zone, Linkage* linkage) {
    SimplifiedLowering lowering(
        data->jsgraph(), data->broker(), temp_zone, data->source_positions(),
        data->node_origins(), data->info()->GetPoisoningMitigationLevel(),
        &data->info()->tick_counter(), linkage, data->observe_node_manager());

    // RepresentationChanger accesses the heap.
    UnparkedScopeIfNeeded scope(data->broker());

    lowering.LowerAllNodes();
  }
};

struct LoopPeelingPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(LoopPeeling)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphTrimmer trimmer(temp_zone, data->graph());
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    {
      UnparkedScopeIfNeeded scope(data->broker(), FLAG_trace_turbo_trimming);
      trimmer.TrimGraph(roots.begin(), roots.end());
    }

    LoopTree* loop_tree = LoopFinder::BuildLoopTree(
        data->jsgraph()->graph(), &data->info()->tick_counter(), temp_zone);
    // We call the typer inside of PeelInnerLoopsOfTree which inspects heap
    // objects, so we need to unpark the local heap.
    UnparkedScopeIfNeeded scope(data->broker());
    LoopPeeler(data->graph(), data->common(), loop_tree, temp_zone,
               data->source_positions(), data->node_origins())
        .PeelInnerLoopsOfTree();
  }
};

struct LoopExitEliminationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(LoopExitElimination)

  void Run(PipelineData* data, Zone* temp_zone) {
    LoopPeeler::EliminateLoopExits(data->graph(), temp_zone);
  }
};

struct GenericLoweringPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(GenericLowering)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(
        temp_zone, data->graph(), &data->info()->tick_counter(), data->broker(),
        data->jsgraph()->Dead(), data->observe_node_manager());
    JSGenericLowering generic_lowering(data->jsgraph(), &graph_reducer,
                                       data->broker());
    AddReducer(data, &graph_reducer, &generic_lowering);

    // JSGEnericLowering accesses the heap due to ObjectRef's type checks.
    UnparkedScopeIfNeeded scope(data->broker());

    graph_reducer.ReduceGraph();
  }
};

struct EarlyOptimizationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(EarlyOptimization)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(
        temp_zone, data->graph(), &data->info()->tick_counter(), data->broker(),
        data->jsgraph()->Dead(), data->observe_node_manager());
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    SimplifiedOperatorReducer simple_reducer(&graph_reducer, data->jsgraph(),
                                             data->broker());
    RedundancyElimination redundancy_elimination(&graph_reducer, temp_zone);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    MachineOperatorReducer machine_reducer(&graph_reducer, data->jsgraph());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->broker(), data->common(),
                                         data->machine(), temp_zone);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &simple_reducer);
    AddReducer(data, &graph_reducer, &redundancy_elimination);
    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &common_reducer);
    AddReducer(data, &graph_reducer, &value_numbering);
    graph_reducer.ReduceGraph();
  }
};

struct ControlFlowOptimizationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(ControlFlowOptimization)

  void Run(PipelineData* data, Zone* temp_zone) {
    ControlFlowOptimizer optimizer(data->graph(), data->common(),
                                   data->machine(),
                                   &data->info()->tick_counter(), temp_zone);
    optimizer.Optimize();
  }
};

struct EffectControlLinearizationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(EffectLinearization)

  void Run(PipelineData* data, Zone* temp_zone) {
    {
      // The scheduler requires the graphs to be trimmed, so trim now.
      // TODO(jarin) Remove the trimming once the scheduler can handle untrimmed
      // graphs.
      GraphTrimmer trimmer(temp_zone, data->graph());
      NodeVector roots(temp_zone);
      data->jsgraph()->GetCachedNodes(&roots);
      {
        UnparkedScopeIfNeeded scope(data->broker(), FLAG_trace_turbo_trimming);
        trimmer.TrimGraph(roots.begin(), roots.end());
      }

      // Schedule the graph without node splitting so that we can
      // fix the effect and control flow for nodes with low-level side
      // effects (such as changing representation to tagged or
      // 'floating' allocation regions.)
      Schedule* schedule = Scheduler::ComputeSchedule(
          temp_zone, data->graph(), Scheduler::kTempSchedule,
          &data->info()->tick_counter(), data->profile_data());
      TraceScheduleAndVerify(data->info(), data, schedule,
                             "effect linearization schedule");

      MaskArrayIndexEnable mask_array_index =
          (data->info()->GetPoisoningMitigationLevel() !=
           PoisoningMitigationLevel::kDontPoison)
              ? MaskArrayIndexEnable::kMaskArrayIndex
              : MaskArrayIndexEnable::kDoNotMaskArrayIndex;
      // Post-pass for wiring the control/effects
      // - connect allocating representation changes into the control&effect
      //   chains and lower them,
      // - get rid of the region markers,
      // - introduce effect phis and rewire effects to get SSA again.
      LinearizeEffectControl(data->jsgraph(), schedule, temp_zone,
                             data->source_positions(), data->node_origins(),
                             mask_array_index, MaintainSchedule::kDiscard,
                             data->broker());
    }
    {
      // The {EffectControlLinearizer} might leave {Dead} nodes behind, so we
      // run {DeadCodeElimination} to prune these parts of the graph.
      // Also, the following store-store elimination phase greatly benefits from
      // doing a common operator reducer and dead code elimination just before
      // it, to eliminate conditional deopts with a constant condition.
      GraphReducer graph_reducer(temp_zone, data->graph(),
                                 &data->info()->tick_counter(), data->broker(),
                                 data->jsgraph()->Dead(),
                                 data->observe_node_manager());
      DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                                data->common(), temp_zone);
      CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                           data->broker(), data->common(),
                                           data->machine(), temp_zone);
      AddReducer(data, &graph_reducer, &dead_code_elimination);
      AddReducer(data, &graph_reducer, &common_reducer);
      graph_reducer.ReduceGraph();
    }
  }
};

struct StoreStoreEliminationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(StoreStoreElimination)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphTrimmer trimmer(temp_zone, data->graph());
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    {
      UnparkedScopeIfNeeded scope(data->broker(), FLAG_trace_turbo_trimming);
      trimmer.TrimGraph(roots.begin(), roots.end());
    }

    StoreStoreElimination::Run(data->jsgraph(), &data->info()->tick_counter(),
                               temp_zone);
  }
};

struct LoadEliminationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(LoadElimination)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(
        temp_zone, data->graph(), &data->info()->tick_counter(), data->broker(),
        data->jsgraph()->Dead(), data->observe_node_manager());
    BranchElimination branch_condition_elimination(&graph_reducer,
                                                   data->jsgraph(), temp_zone,
                                                   BranchElimination::kEARLY);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    RedundancyElimination redundancy_elimination(&graph_reducer, temp_zone);
    LoadElimination load_elimination(&graph_reducer, data->jsgraph(),
                                     temp_zone);
    CheckpointElimination checkpoint_elimination(&graph_reducer);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->broker(), data->common(),
                                         data->machine(), temp_zone);
    TypedOptimization typed_optimization(&graph_reducer, data->dependencies(),
                                         data->jsgraph(), data->broker());
    ConstantFoldingReducer constant_folding_reducer(
        &graph_reducer, data->jsgraph(), data->broker());
    TypeNarrowingReducer type_narrowing_reducer(&graph_reducer, data->jsgraph(),
                                                data->broker());

    AddReducer(data, &graph_reducer, &branch_condition_elimination);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &redundancy_elimination);
    AddReducer(data, &graph_reducer, &load_elimination);
    AddReducer(data, &graph_reducer, &type_narrowing_reducer);
    AddReducer(data, &graph_reducer, &constant_folding_reducer);
    AddReducer(data, &graph_reducer, &typed_optimization);
    AddReducer(data, &graph_reducer, &checkpoint_elimination);
    AddReducer(data, &graph_reducer, &common_reducer);
    AddReducer(data, &graph_reducer, &value_numbering);

    // ConstantFoldingReducer and TypedOptimization access the heap.
    UnparkedScopeIfNeeded scope(data->broker());

    graph_reducer.ReduceGraph();
  }
};

struct MemoryOptimizationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(MemoryOptimization)

  void Run(PipelineData* data, Zone* temp_zone) {
    // The memory optimizer requires the graphs to be trimmed, so trim now.
    GraphTrimmer trimmer(temp_zone, data->graph());
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    {
      UnparkedScopeIfNeeded scope(data->broker(), FLAG_trace_turbo_trimming);
      trimmer.TrimGraph(roots.begin(), roots.end());
    }

    // Optimize allocations and load/store operations.
    MemoryOptimizer optimizer(
        data->jsgraph(), temp_zone, data->info()->GetPoisoningMitigationLevel(),
        data->info()->allocation_folding()
            ? MemoryLowering::AllocationFolding::kDoAllocationFolding
            : MemoryLowering::AllocationFolding::kDontAllocationFolding,
        data->debug_name(), &data->info()->tick_counter());
    optimizer.Optimize();
  }
};

struct LateOptimizationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(LateOptimization)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(
        temp_zone, data->graph(), &data->info()->tick_counter(), data->broker(),
        data->jsgraph()->Dead(), data->observe_node_manager());
    BranchElimination branch_condition_elimination(&graph_reducer,
                                                   data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    MachineOperatorReducer machine_reducer(&graph_reducer, data->jsgraph());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->broker(), data->common(),
                                         data->machine(), temp_zone);
    JSGraphAssembler graph_assembler(data->jsgraph(), temp_zone);
    SelectLowering select_lowering(&graph_assembler, data->graph());
    AddReducer(data, &graph_reducer, &branch_condition_elimination);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &common_reducer);
    AddReducer(data, &graph_reducer, &select_lowering);
    AddReducer(data, &graph_reducer, &value_numbering);
    graph_reducer.ReduceGraph();
  }
};

struct MachineOperatorOptimizationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(MachineOperatorOptimization)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(
        temp_zone, data->graph(), &data->info()->tick_counter(), data->broker(),
        data->jsgraph()->Dead(), data->observe_node_manager());
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    MachineOperatorReducer machine_reducer(&graph_reducer, data->jsgraph());

    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &value_numbering);
    graph_reducer.ReduceGraph();
  }
};

struct DecompressionOptimizationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(DecompressionOptimization)

  void Run(PipelineData* data, Zone* temp_zone) {
    if (COMPRESS_POINTERS_BOOL) {
      DecompressionOptimizer decompression_optimizer(
          temp_zone, data->graph(), data->common(), data->machine());
      decompression_optimizer.Reduce();
    }
  }
};

struct ScheduledEffectControlLinearizationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(ScheduledEffectControlLinearization)

  void Run(PipelineData* data, Zone* temp_zone) {
    MaskArrayIndexEnable mask_array_index =
        (data->info()->GetPoisoningMitigationLevel() !=
         PoisoningMitigationLevel::kDontPoison)
            ? MaskArrayIndexEnable::kMaskArrayIndex
            : MaskArrayIndexEnable::kDoNotMaskArrayIndex;
    // Post-pass for wiring the control/effects
    // - connect allocating representation changes into the control&effect
    //   chains and lower them,
    // - get rid of the region markers,
    // - introduce effect phis and rewire effects to get SSA again.
    LinearizeEffectControl(data->jsgraph(), data->schedule(), temp_zone,
                           data->source_positions(), data->node_origins(),
                           mask_array_index, MaintainSchedule::kMaintain,
                           data->broker());

    // TODO(rmcilroy) Avoid having to rebuild rpo_order on schedule each time.
    Scheduler::ComputeSpecialRPO(temp_zone, data->schedule());
    if (FLAG_turbo_verify) Scheduler::GenerateDominatorTree(data->schedule());
    TraceScheduleAndVerify(data->info(), data, data->schedule(),
                           "effect linearization schedule");
  }
};

struct ScheduledMachineLoweringPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(ScheduledMachineLowering)

  void Run(PipelineData* data, Zone* temp_zone) {
    ScheduledMachineLowering machine_lowering(
        data->jsgraph(), data->schedule(), temp_zone, data->source_positions(),
        data->node_origins(), data->info()->GetPoisoningMitigationLevel());
    machine_lowering.Run();

    // TODO(rmcilroy) Avoid having to rebuild rpo_order on schedule each time.
    Scheduler::ComputeSpecialRPO(temp_zone, data->schedule());
    Scheduler::GenerateDominatorTree(data->schedule());
    TraceScheduleAndVerify(data->info(), data, data->schedule(),
                           "machine lowered schedule");
  }
};

struct CsaEarlyOptimizationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(CSAEarlyOptimization)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(
        temp_zone, data->graph(), &data->info()->tick_counter(), data->broker(),
        data->jsgraph()->Dead(), data->observe_node_manager());
    MachineOperatorReducer machine_reducer(&graph_reducer, data->jsgraph());
    BranchElimination branch_condition_elimination(&graph_reducer,
                                                   data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->broker(), data->common(),
                                         data->machine(), temp_zone);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    CsaLoadElimination load_elimination(&graph_reducer, data->jsgraph(),
                                        temp_zone);
    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &branch_condition_elimination);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &common_reducer);
    AddReducer(data, &graph_reducer, &value_numbering);
    AddReducer(data, &graph_reducer, &load_elimination);
    graph_reducer.ReduceGraph();
  }
};

struct CsaOptimizationPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(CSAOptimization)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(
        temp_zone, data->graph(), &data->info()->tick_counter(), data->broker(),
        data->jsgraph()->Dead(), data->observe_node_manager());
    BranchElimination branch_condition_elimination(&graph_reducer,
                                                   data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    MachineOperatorReducer machine_reducer(&graph_reducer, data->jsgraph());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->broker(), data->common(),
                                         data->machine(), temp_zone);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    AddReducer(data, &graph_reducer, &branch_condition_elimination);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &common_reducer);
    AddReducer(data, &graph_reducer, &value_numbering);
    graph_reducer.ReduceGraph();
  }
};

struct EarlyGraphTrimmingPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(EarlyTrimming)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphTrimmer trimmer(temp_zone, data->graph());
    NodeVector roots(temp_zone);
    data->jsgraph()->GetCachedNodes(&roots);
    UnparkedScopeIfNeeded scope(data->broker(), FLAG_trace_turbo_trimming);
    trimmer.TrimGraph(roots.begin(), roots.end());
  }
};


struct LateGraphTrimmingPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(LateGraphTrimming)

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphTrimmer trimmer(temp_zone, data->graph());
    NodeVector roots(temp_zone);
    if (data->jsgraph()) {
      data->jsgraph()->GetCachedNodes(&roots);
    }
    UnparkedScopeIfNeeded scope(data->broker(), FLAG_trace_turbo_trimming);
    trimmer.TrimGraph(roots.begin(), roots.end());
  }
};


struct ComputeSchedulePhase {
  DECL_PIPELINE_PHASE_CONSTANTS(Scheduling)

  void Run(PipelineData* data, Zone* temp_zone) {
    Schedule* schedule = Scheduler::ComputeSchedule(
        temp_zone, data->graph(),
        data->info()->splitting() ? Scheduler::kSplitNodes
                                  : Scheduler::kNoFlags,
        &data->info()->tick_counter(), data->profile_data());
    data->set_schedule(schedule);
  }
};

struct InstructionRangesAsJSON {
  const InstructionSequence* sequence;
  const ZoneVector<std::pair<int, int>>* instr_origins;
};

std::ostream& operator<<(std::ostream& out, const InstructionRangesAsJSON& s) {
  const int max = static_cast<int>(s.sequence->LastInstructionIndex());

  out << ", \"nodeIdToInstructionRange\": {";
  bool need_comma = false;
  for (size_t i = 0; i < s.instr_origins->size(); ++i) {
    std::pair<int, int> offset = (*s.instr_origins)[i];
    if (offset.first == -1) continue;
    const int first = max - offset.first + 1;
    const int second = max - offset.second + 1;
    if (need_comma) out << ", ";
    out << "\"" << i << "\": [" << first << ", " << second << "]";
    need_comma = true;
  }
  out << "}";
  out << ", \"blockIdtoInstructionRange\": {";
  need_comma = false;
  for (auto block : s.sequence->instruction_blocks()) {
    if (need_comma) out << ", ";
    out << "\"" << block->rpo_number() << "\": [" << block->code_start() << ", "
        << block->code_end() << "]";
    need_comma = true;
  }
  out << "}";
  return out;
}

struct InstructionSelectionPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(SelectInstructions)

  void Run(PipelineData* data, Zone* temp_zone, Linkage* linkage) {
    InstructionSelector selector(
        temp_zone, data->graph()->NodeCount(), linkage, data->sequence(),
        data->schedule(), data->source_positions(), data->frame(),
        data->info()->switch_jump_table()
            ? InstructionSelector::kEnableSwitchJumpTable
            : InstructionSelector::kDisableSwitchJumpTable,
        &data->info()->tick_counter(), data->broker(),
        data->address_of_max_unoptimized_frame_height(),
        data->address_of_max_pushed_argument_count(),
        data->info()->source_positions()
            ? InstructionSelector::kAllSourcePositions
            : InstructionSelector::kCallSourcePositions,
        InstructionSelector::SupportedFeatures(),
        FLAG_turbo_instruction_scheduling
            ? InstructionSelector::kEnableScheduling
            : InstructionSelector::kDisableScheduling,
        data->roots_relative_addressing_enabled()
            ? InstructionSelector::kEnableRootsRelativeAddressing
            : InstructionSelector::kDisableRootsRelativeAddressing,
        data->info()->GetPoisoningMitigationLevel(),
        data->info()->trace_turbo_json()
            ? InstructionSelector::kEnableTraceTurboJson
            : InstructionSelector::kDisableTraceTurboJson);
    if (!selector.SelectInstructions()) {
      data->set_compilation_failed();
    }
    if (data->info()->trace_turbo_json()) {
      TurboJsonFile json_of(data->info(), std::ios_base::app);
      json_of << "{\"name\":\"" << phase_name()
              << "\",\"type\":\"instructions\""
              << InstructionRangesAsJSON{data->sequence(),
                                         &selector.instr_origins()}
              << "},\n";
    }
  }
};


struct MeetRegisterConstraintsPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(MeetRegisterConstraints)
  void Run(PipelineData* data, Zone* temp_zone) {
    ConstraintBuilder builder(data->top_tier_register_allocation_data());
    builder.MeetRegisterConstraints();
  }
};


struct ResolvePhisPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(ResolvePhis)

  void Run(PipelineData* data, Zone* temp_zone) {
    ConstraintBuilder builder(data->top_tier_register_allocation_data());
    builder.ResolvePhis();
  }
};


struct BuildLiveRangesPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(BuildLiveRanges)

  void Run(PipelineData* data, Zone* temp_zone) {
    LiveRangeBuilder builder(data->top_tier_register_allocation_data(),
                             temp_zone);
    builder.BuildLiveRanges();
  }
};

struct BuildBundlesPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(BuildLiveRangeBundles)

  void Run(PipelineData* data, Zone* temp_zone) {
    BundleBuilder builder(data->top_tier_register_allocation_data());
    builder.BuildBundles();
  }
};

template <typename RegAllocator>
struct AllocateGeneralRegistersPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(AllocateGeneralRegisters)

  void Run(PipelineData* data, Zone* temp_zone) {
    RegAllocator allocator(data->top_tier_register_allocation_data(),
                           RegisterKind::kGeneral, temp_zone);
    allocator.AllocateRegisters();
  }
};

template <typename RegAllocator>
struct AllocateFPRegistersPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(AllocateFPRegisters)

  void Run(PipelineData* data, Zone* temp_zone) {
    RegAllocator allocator(data->top_tier_register_allocation_data(),
                           RegisterKind::kDouble, temp_zone);
    allocator.AllocateRegisters();
  }
};

struct DecideSpillingModePhase {
  DECL_PIPELINE_PHASE_CONSTANTS(DecideSpillingMode)

  void Run(PipelineData* data, Zone* temp_zone) {
    OperandAssigner assigner(data->top_tier_register_allocation_data());
    assigner.DecideSpillingMode();
  }
};

struct AssignSpillSlotsPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(AssignSpillSlots)

  void Run(PipelineData* data, Zone* temp_zone) {
    OperandAssigner assigner(data->top_tier_register_allocation_data());
    assigner.AssignSpillSlots();
  }
};


struct CommitAssignmentPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(CommitAssignment)

  void Run(PipelineData* data, Zone* temp_zone) {
    OperandAssigner assigner(data->top_tier_register_allocation_data());
    assigner.CommitAssignment();
  }
};


struct PopulateReferenceMapsPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(PopulatePointerMaps)

  void Run(PipelineData* data, Zone* temp_zone) {
    ReferenceMapPopulator populator(data->top_tier_register_allocation_data());
    populator.PopulateReferenceMaps();
  }
};


struct ConnectRangesPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(ConnectRanges)

  void Run(PipelineData* data, Zone* temp_zone) {
    LiveRangeConnector connector(data->top_tier_register_allocation_data());
    connector.ConnectRanges(temp_zone);
  }
};


struct ResolveControlFlowPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(ResolveControlFlow)

  void Run(PipelineData* data, Zone* temp_zone) {
    LiveRangeConnector connector(data->top_tier_register_allocation_data());
    connector.ResolveControlFlow(temp_zone);
  }
};

struct MidTierRegisterOutputDefinitionPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(MidTierRegisterOutputDefinition)

  void Run(PipelineData* data, Zone* temp_zone) {
    DefineOutputs(data->mid_tier_register_allocator_data());
  }
};

struct MidTierRegisterAllocatorPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(MidTierRegisterAllocator)

  void Run(PipelineData* data, Zone* temp_zone) {
    AllocateRegisters(data->mid_tier_register_allocator_data());
  }
};

struct MidTierSpillSlotAllocatorPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(MidTierSpillSlotAllocator)

  void Run(PipelineData* data, Zone* temp_zone) {
    AllocateSpillSlots(data->mid_tier_register_allocator_data());
  }
};

struct MidTierPopulateReferenceMapsPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(MidTierPopulateReferenceMaps)

  void Run(PipelineData* data, Zone* temp_zone) {
    PopulateReferenceMaps(data->mid_tier_register_allocator_data());
  }
};

struct OptimizeMovesPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(OptimizeMoves)

  void Run(PipelineData* data, Zone* temp_zone) {
    MoveOptimizer move_optimizer(temp_zone, data->sequence());
    move_optimizer.Run();
  }
};

struct FrameElisionPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(FrameElision)

  void Run(PipelineData* data, Zone* temp_zone) {
    FrameElider(data->sequence()).Run();
  }
};

struct JumpThreadingPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(JumpThreading)

  void Run(PipelineData* data, Zone* temp_zone, bool frame_at_start) {
    ZoneVector<RpoNumber> result(temp_zone);
    if (JumpThreading::ComputeForwarding(temp_zone, &result, data->sequence(),
                                         frame_at_start)) {
      JumpThreading::ApplyForwarding(temp_zone, result, data->sequence());
    }
  }
};

struct AssembleCodePhase {
  DECL_PIPELINE_PHASE_CONSTANTS(AssembleCode)

  void Run(PipelineData* data, Zone* temp_zone) {
    data->code_generator()->AssembleCode();
  }
};

struct FinalizeCodePhase {
  DECL_MAIN_THREAD_PIPELINE_PHASE_CONSTANTS(FinalizeCode)

  void Run(PipelineData* data, Zone* temp_zone) {
    data->set_code(data->code_generator()->FinalizeCode());
  }
};


struct PrintGraphPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(PrintGraph)

  void Run(PipelineData* data, Zone* temp_zone, const char* phase) {
    OptimizedCompilationInfo* info = data->info();
    Graph* graph = data->graph();

    if (info->trace_turbo_json()) {  // Print JSON.
      UnparkedScopeIfNeeded scope(data->broker());
      AllowHandleDereference allow_deref;

      TurboJsonFile json_of(info, std::ios_base::app);
      json_of << "{\"name\":\"" << phase << "\",\"type\":\"graph\",\"data\":"
              << AsJSON(*graph, data->source_positions(), data->node_origins())
              << "},\n";
    }

    if (info->trace_turbo_scheduled()) {
      AccountingAllocator allocator;
      Schedule* schedule = data->schedule();
      if (schedule == nullptr) {
        schedule = Scheduler::ComputeSchedule(
            temp_zone, data->graph(), Scheduler::kNoFlags,
            &info->tick_counter(), data->profile_data());
      }

      UnparkedScopeIfNeeded scope(data->broker());
      AllowHandleDereference allow_deref;
      CodeTracer::StreamScope tracing_scope(data->GetCodeTracer());
      tracing_scope.stream()
          << "-- Graph after " << phase << " -- " << std::endl
          << AsScheduledGraph(schedule);
    } else if (info->trace_turbo_graph()) {  // Simple textual RPO.
      UnparkedScopeIfNeeded scope(data->broker());
      AllowHandleDereference allow_deref;
      CodeTracer::StreamScope tracing_scope(data->GetCodeTracer());
      tracing_scope.stream()
          << "-- Graph after " << phase << " -- " << std::endl
          << AsRPO(*graph);
    }
  }
};


struct VerifyGraphPhase {
  DECL_PIPELINE_PHASE_CONSTANTS(VerifyGraph)

  void Run(PipelineData* data, Zone* temp_zone, const bool untyped,
           bool values_only = false) {
    Verifier::CodeType code_type;
    switch (data->info()->code_kind()) {
      case CodeKind::WASM_FUNCTION:
      case CodeKind::WASM_TO_CAPI_FUNCTION:
      case CodeKind::WASM_TO_JS_FUNCTION:
      case CodeKind::JS_TO_WASM_FUNCTION:
      case CodeKind::C_WASM_ENTRY:
        code_type = Verifier::kWasm;
        break;
      default:
        code_type = Verifier::kDefault;
    }
    Verifier::Run(data->graph(), !untyped ? Verifier::TYPED : Verifier::UNTYPED,
                  values_only ? Verifier::kValuesOnly : Verifier::kAll,
                  code_type);
  }
};

#undef DECL_MAIN_THREAD_PIPELINE_PHASE_CONSTANTS
#undef DECL_PIPELINE_PHASE_CONSTANTS
#undef DECL_PIPELINE_PHASE_CONSTANTS_HELPER

void PipelineImpl::RunPrintAndVerify(const char* phase, bool untyped) {
  if (info()->trace_turbo_json() || info()->trace_turbo_graph()) {
    Run<PrintGraphPhase>(phase);
  }
  if (FLAG_turbo_verify) {
    Run<VerifyGraphPhase>(untyped);
  }
}

void PipelineImpl::Serialize() {
  PipelineData* data = this->data_;

  data->BeginPhaseKind("V8.TFBrokerInitAndSerialization");

  if (info()->trace_turbo_json() || info()->trace_turbo_graph()) {
    CodeTracer::StreamScope tracing_scope(data->GetCodeTracer());
    tracing_scope.stream()
        << "---------------------------------------------------\n"
        << "Begin compiling method " << info()->GetDebugName().get()
        << " using TurboFan" << std::endl;
  }
  if (info()->trace_turbo_json()) {
    TurboCfgFile tcf(isolate());
    tcf << AsC1VCompilation(info());
  }

  data->source_positions()->AddDecorator();
  if (data->info()->trace_turbo_json()) {
    data->node_origins()->AddDecorator();
  }

  data->broker()->SetTargetNativeContextRef(data->native_context());
  if (data->broker()->is_concurrent_inlining()) {
    Run<HeapBrokerInitializationPhase>();
    Run<SerializationPhase>();
    data->broker()->StopSerializing();
  }
  data->EndPhaseKind();
}

bool PipelineImpl::CreateGraph() {
  PipelineData* data = this->data_;
  UnparkedScopeIfNeeded unparked_scope(data->broker());

  data->BeginPhaseKind("V8.TFGraphCreation");

  Run<GraphBuilderPhase>();
  RunPrintAndVerify(GraphBuilderPhase::phase_name(), true);

  // Perform function context specialization and inlining (if enabled).
  Run<InliningPhase>();
  RunPrintAndVerify(InliningPhase::phase_name(), true);

  // Remove dead->live edges from the graph.
  Run<EarlyGraphTrimmingPhase>();
  RunPrintAndVerify(EarlyGraphTrimmingPhase::phase_name(), true);

  // Determine the Typer operation flags.
  {
    SharedFunctionInfoRef shared_info(data->broker(), info()->shared_info());
    if (is_sloppy(shared_info.language_mode()) &&
        shared_info.IsUserJavaScript()) {
      // Sloppy mode functions always have an Object for this.
      data->AddTyperFlag(Typer::kThisIsReceiver);
    }
    if (IsClassConstructor(shared_info.kind())) {
      // Class constructors cannot be [[Call]]ed.
      data->AddTyperFlag(Typer::kNewTargetIsReceiver);
    }
  }

  // Run the type-sensitive lowerings and optimizations on the graph.
  {
    if (!data->broker()->is_concurrent_inlining()) {
      Run<HeapBrokerInitializationPhase>();
      Run<CopyMetadataForConcurrentCompilePhase>();
      data->broker()->StopSerializing();
    }
  }

  data->EndPhaseKind();

  return true;
}

bool PipelineImpl::OptimizeGraph(Linkage* linkage) {
  PipelineData* data = this->data_;

  data->BeginPhaseKind("V8.TFLowering");

  // Type the graph and keep the Typer running such that new nodes get
  // automatically typed when they are created.
  Run<TyperPhase>(data->CreateTyper());
  RunPrintAndVerify(TyperPhase::phase_name());

  Run<TypedLoweringPhase>();
  RunPrintAndVerify(TypedLoweringPhase::phase_name());

  if (data->info()->loop_peeling()) {
    Run<LoopPeelingPhase>();
    RunPrintAndVerify(LoopPeelingPhase::phase_name(), true);
  } else {
    Run<LoopExitEliminationPhase>();
    RunPrintAndVerify(LoopExitEliminationPhase::phase_name(), true);
  }

  if (FLAG_turbo_load_elimination) {
    Run<LoadEliminationPhase>();
    RunPrintAndVerify(LoadEliminationPhase::phase_name());
  }
  data->DeleteTyper();

  if (FLAG_turbo_escape) {
    Run<EscapeAnalysisPhase>();
    if (data->compilation_failed()) {
      info()->AbortOptimization(
          BailoutReason::kCyclicObjectStateDetectedInEscapeAnalysis);
      data->EndPhaseKind();
      return false;
    }
    RunPrintAndVerify(EscapeAnalysisPhase::phase_name());
  }

  if (FLAG_assert_types) {
    Run<TypeAssertionsPhase>();
    RunPrintAndVerify(TypeAssertionsPhase::phase_name());
  }

  // Perform simplified lowering. This has to run w/o the Typer decorator,
  // because we cannot compute meaningful types anyways, and the computed types
  // might even conflict with the representation/truncation logic.
  Run<SimplifiedLoweringPhase>(linkage);
  RunPrintAndVerify(SimplifiedLoweringPhase::phase_name(), true);

  if (data->has_js_wasm_calls()) {
    DCHECK(FLAG_turbo_inline_js_wasm_calls);
    Run<WasmInliningPhase>();
    RunPrintAndVerify(WasmInliningPhase::phase_name(), true);
  }

  // From now on it is invalid to look at types on the nodes, because the types
  // on the nodes might not make sense after representation selection due to the
  // way we handle truncations; if we'd want to look at types afterwards we'd
  // essentially need to re-type (large portions of) the graph.

  // In order to catch bugs related to type access after this point, we now
  // remove the types from the nodes (currently only in Debug builds).
#ifdef DEBUG
  Run<UntyperPhase>();
  RunPrintAndVerify(UntyperPhase::phase_name(), true);
#endif

  // Run generic lowering pass.
  Run<GenericLoweringPhase>();
  RunPrintAndVerify(GenericLoweringPhase::phase_name(), true);

  data->BeginPhaseKind("V8.TFBlockBuilding");

  data->InitializeFrameData(linkage->GetIncomingDescriptor());

  // Run early optimization pass.
  Run<EarlyOptimizationPhase>();
  RunPrintAndVerify(EarlyOptimizationPhase::phase_name(), true);

  Run<EffectControlLinearizationPhase>();
  RunPrintAndVerify(EffectControlLinearizationPhase::phase_name(), true);

  if (FLAG_turbo_store_elimination) {
    Run<StoreStoreEliminationPhase>();
    RunPrintAndVerify(StoreStoreEliminationPhase::phase_name(), true);
  }

  // Optimize control flow.
  if (FLAG_turbo_cf_optimization) {
    Run<ControlFlowOptimizationPhase>();
    RunPrintAndVerify(ControlFlowOptimizationPhase::phase_name(), true);
  }

  Run<LateOptimizationPhase>();
  RunPrintAndVerify(LateOptimizationPhase::phase_name(), true);

  // Optimize memory access and allocation operations.
  Run<MemoryOptimizationPhase>();
  RunPrintAndVerify(MemoryOptimizationPhase::phase_name(), true);

  // Run value numbering and machine operator reducer to optimize load/store
  // address computation (in particular, reuse the address computation whenever
  // possible).
  Run<MachineOperatorOptimizationPhase>();
  RunPrintAndVerify(MachineOperatorOptimizationPhase::phase_name(), true);

  Run<DecompressionOptimizationPhase>();
  RunPrintAndVerify(DecompressionOptimizationPhase::phase_name(), true);

  data->source_positions()->RemoveDecorator();
  if (data->info()->trace_turbo_json()) {
    data->node_origins()->RemoveDecorator();
  }

  ComputeScheduledGraph();

  return SelectInstructions(linkage);
}

bool PipelineImpl::OptimizeGraphForMidTier(Linkage* linkage) {
  PipelineData* data = this->data_;

  data->BeginPhaseKind("V8.TFLowering");

  // Type the graph and keep the Typer running such that new nodes get
  // automatically typed when they are created.
  Run<TyperPhase>(data->CreateTyper());
  RunPrintAndVerify(TyperPhase::phase_name());

  Run<TypedLoweringPhase>();
  RunPrintAndVerify(TypedLoweringPhase::phase_name());

  // TODO(9684): Consider rolling this into the preceeding phase or not creating
  // LoopExit nodes at all.
  Run<LoopExitEliminationPhase>();
  RunPrintAndVerify(LoopExitEliminationPhase::phase_name(), true);

  data->DeleteTyper();

  if (FLAG_assert_types) {
    Run<TypeAssertionsPhase>();
    RunPrintAndVerify(TypeAssertionsPhase::phase_name());
  }

  // Perform simplified lowering. This has to run w/o the Typer decorator,
  // because we cannot compute meaningful types anyways, and the computed types
  // might even conflict with the representation/truncation logic.
  Run<SimplifiedLoweringPhase>(linkage);
  RunPrintAndVerify(SimplifiedLoweringPhase::phase_name(), true);

  // From now on it is invalid to look at types on the nodes, because the types
  // on the nodes might not make sense after representation selection due to the
  // way we handle truncations; if we'd want to look at types afterwards we'd
  // essentially need to re-type (large portions of) the graph.

  // In order to catch bugs related to type access after this point, we now
  // remove the types from the nodes (currently only in Debug builds).
#ifdef DEBUG
  Run<UntyperPhase>();
  RunPrintAndVerify(UntyperPhase::phase_name(), true);
#endif

  // Run generic lowering pass.
  Run<GenericLoweringPhase>();
  RunPrintAndVerify(GenericLoweringPhase::phase_name(), true);

  data->BeginPhaseKind("V8.TFBlockBuilding");

  data->InitializeFrameData(linkage->GetIncomingDescriptor());

  ComputeScheduledGraph();

  Run<ScheduledEffectControlLinearizationPhase>();
  RunPrintAndVerify(ScheduledEffectControlLinearizationPhase::phase_name(),
                    true);

  Run<ScheduledMachineLoweringPhase>();
  RunPrintAndVerify(ScheduledMachineLoweringPhase::phase_name(), true);

  // The DecompressionOptimizationPhase updates node's operations but does not
  // otherwise rewrite the graph, thus it is safe to run on a scheduled graph.
  Run<DecompressionOptimizationPhase>();
  RunPrintAndVerify(DecompressionOptimizationPhase::phase_name(), true);

  data->source_positions()->RemoveDecorator();
  if (data->info()->trace_turbo_json()) {
    data->node_origins()->RemoveDecorator();
  }

  return SelectInstructions(linkage);
}

namespace {

// Compute a hash of the given graph, in a way that should provide the same
// result in multiple runs of mksnapshot, meaning the hash cannot depend on any
// external pointer values or uncompressed heap constants. This hash can be used
// to reject profiling data if the builtin's current code doesn't match the
// version that was profiled. Hash collisions are not catastrophic; in the worst
// case, we just defer some blocks that ideally shouldn't be deferred. The
// result value is in the valid Smi range.
int HashGraphForPGO(Graph* graph) {
  AccountingAllocator allocator;
  Zone local_zone(&allocator, ZONE_NAME);

  constexpr NodeId kUnassigned = static_cast<NodeId>(-1);

  constexpr byte kUnvisited = 0;
  constexpr byte kOnStack = 1;
  constexpr byte kVisited = 2;

  // Do a depth-first post-order traversal of the graph. For every node, hash:
  //
  //   - the node's traversal number
  //   - the opcode
  //   - the number of inputs
  //   - each input node's traversal number
  //
  // What's a traversal number? We can't use node IDs because they're not stable
  // build-to-build, so we assign a new number for each node as it is visited.

  ZoneVector<byte> state(graph->NodeCount(), kUnvisited, &local_zone);
  ZoneVector<NodeId> traversal_numbers(graph->NodeCount(), kUnassigned,
                                       &local_zone);
  ZoneStack<Node*> stack(&local_zone);

  NodeId visited_count = 0;
  size_t hash = 0;

  stack.push(graph->end());
  state[graph->end()->id()] = kOnStack;
  traversal_numbers[graph->end()->id()] = visited_count++;
  while (!stack.empty()) {
    Node* n = stack.top();
    bool pop = true;
    for (Node* const i : n->inputs()) {
      if (state[i->id()] == kUnvisited) {
        state[i->id()] = kOnStack;
        traversal_numbers[i->id()] = visited_count++;
        stack.push(i);
        pop = false;
        break;
      }
    }
    if (pop) {
      state[n->id()] = kVisited;
      stack.pop();
      hash = base::hash_combine(hash, traversal_numbers[n->id()], n->opcode(),
                                n->InputCount());
      for (Node* const i : n->inputs()) {
        DCHECK(traversal_numbers[i->id()] != kUnassigned);
        hash = base::hash_combine(hash, traversal_numbers[i->id()]);
      }
    }
  }
  return Smi(IntToSmi(static_cast<int>(hash))).value();
}

}  // namespace

MaybeHandle<Code> Pipeline::GenerateCodeForCodeStub(
    Isolate* isolate, CallDescriptor* call_descriptor, Graph* graph,
    JSGraph* jsgraph, SourcePositionTable* source_positions, CodeKind kind,
    const char* debug_name, int32_t builtin_index,
    PoisoningMitigationLevel poisoning_level, const AssemblerOptions& options,
    const ProfileDataFromFile* profile_data) {
  OptimizedCompilationInfo info(CStrVector(debug_name), graph->zone(), kind);
  info.set_builtin_index(builtin_index);

  if (poisoning_level != PoisoningMitigationLevel::kDontPoison) {
    info.SetPoisoningMitigationLevel(poisoning_level);
  }

  // Construct a pipeline for scheduling and code generation.
  ZoneStats zone_stats(isolate->allocator());
  NodeOriginTable node_origins(graph);
  JumpOptimizationInfo jump_opt;
  bool should_optimize_jumps = isolate->serializer_enabled() &&
                               FLAG_turbo_rewrite_far_jumps &&
                               !FLAG_turbo_profiling;
  PipelineData data(&zone_stats, &info, isolate, isolate->allocator(), graph,
                    jsgraph, nullptr, source_positions, &node_origins,
                    should_optimize_jumps ? &jump_opt : nullptr, options,
                    profile_data);
  PipelineJobScope scope(&data, isolate->counters()->runtime_call_stats());
  RuntimeCallTimerScope timer_scope(isolate,
                                    RuntimeCallCounterId::kOptimizeCode);
  data.set_verify_graph(FLAG_verify_csa);
  std::unique_ptr<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics.reset(new PipelineStatistics(
        &info, isolate->GetTurboStatistics(), &zone_stats));
    pipeline_statistics->BeginPhaseKind("V8.TFStubCodegen");
  }

  PipelineImpl pipeline(&data);

  if (info.trace_turbo_json() || info.trace_turbo_graph()) {
    CodeTracer::StreamScope tracing_scope(data.GetCodeTracer());
    tracing_scope.stream()
        << "---------------------------------------------------\n"
        << "Begin compiling " << debug_name << " using TurboFan" << std::endl;
    if (info.trace_turbo_json()) {
      TurboJsonFile json_of(&info, std::ios_base::trunc);
      json_of << "{\"function\" : ";
      JsonPrintFunctionSource(json_of, -1, info.GetDebugName(),
                              Handle<Script>(), isolate,
                              Handle<SharedFunctionInfo>());
      json_of << ",\n\"phases\":[";
    }
    pipeline.Run<PrintGraphPhase>("V8.TFMachineCode");
  }

  pipeline.Run<CsaEarlyOptimizationPhase>();
  pipeline.RunPrintAndVerify(CsaEarlyOptimizationPhase::phase_name(), true);

  // Optimize memory access and allocation operations.
  pipeline.Run<MemoryOptimizationPhase>();
  pipeline.RunPrintAndVerify(MemoryOptimizationPhase::phase_name(), true);

  pipeline.Run<CsaOptimizationPhase>();
  pipeline.RunPrintAndVerify(CsaOptimizationPhase::phase_name(), true);

  pipeline.Run<DecompressionOptimizationPhase>();
  pipeline.RunPrintAndVerify(DecompressionOptimizationPhase::phase_name(),
                             true);

  pipeline.Run<VerifyGraphPhase>(true);

  int graph_hash_before_scheduling = 0;
  if (FLAG_turbo_profiling || profile_data != nullptr) {
    graph_hash_before_scheduling = HashGraphForPGO(data.graph());
  }

  if (profile_data != nullptr &&
      profile_data->hash() != graph_hash_before_scheduling) {
    PrintF("Rejected profile data for %s due to function change\n", debug_name);
    profile_data = nullptr;
    data.set_profile_data(profile_data);
  }

  pipeline.ComputeScheduledGraph();
  DCHECK_NOT_NULL(data.schedule());

  // First run code generation on a copy of the pipeline, in order to be able to
  // repeat it for jump optimization. The first run has to happen on a temporary
  // pipeline to avoid deletion of zones on the main pipeline.
  PipelineData second_data(&zone_stats, &info, isolate, isolate->allocator(),
                           data.graph(), data.jsgraph(), data.schedule(),
                           data.source_positions(), data.node_origins(),
                           data.jump_optimization_info(), options,
                           profile_data);
  PipelineJobScope second_scope(&second_data,
                                isolate->counters()->runtime_call_stats());
  second_data.set_verify_graph(FLAG_verify_csa);
  PipelineImpl second_pipeline(&second_data);
  second_pipeline.SelectInstructionsAndAssemble(call_descriptor);

  if (FLAG_turbo_profiling) {
    info.profiler_data()->SetHash(graph_hash_before_scheduling);
  }

  if (jump_opt.is_optimizable()) {
    jump_opt.set_optimizing();
    return pipeline.GenerateCode(call_descriptor);
  } else {
    return second_pipeline.FinalizeCode();
  }
}

struct BlockStartsAsJSON {
  const ZoneVector<int>* block_starts;
};

std::ostream& operator<<(std::ostream& out, const BlockStartsAsJSON& s) {
  out << ", \"blockIdToOffset\": {";
  bool need_comma = false;
  for (size_t i = 0; i < s.block_starts->size(); ++i) {
    if (need_comma) out << ", ";
    int offset = (*s.block_starts)[i];
    out << "\"" << i << "\":" << offset;
    need_comma = true;
  }
  out << "},";
  return out;
}

// static
wasm::WasmCompilationResult Pipeline::GenerateCodeForWasmNativeStub(
    wasm::WasmEngine* wasm_engine, CallDescriptor* call_descriptor,
    MachineGraph* mcgraph, CodeKind kind, int wasm_kind, const char* debug_name,
    const AssemblerOptions& options, SourcePositionTable* source_positions) {
  Graph* graph = mcgraph->graph();
  OptimizedCompilationInfo info(CStrVector(debug_name), graph->zone(), kind);
  // Construct a pipeline for scheduling and code generation.
  ZoneStats zone_stats(wasm_engine->allocator());
  NodeOriginTable* node_positions = graph->zone()->New<NodeOriginTable>(graph);
  // {instruction_buffer} must live longer than {PipelineData}, since
  // {PipelineData} will reference the {instruction_buffer} via the
  // {AssemblerBuffer} of the {Assembler} contained in the {CodeGenerator}.
  std::unique_ptr<wasm::WasmInstructionBuffer> instruction_buffer =
      wasm::WasmInstructionBuffer::New();
  PipelineData data(&zone_stats, wasm_engine, &info, mcgraph, nullptr,
                    source_positions, node_positions, options);
  std::unique_ptr<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics.reset(new PipelineStatistics(
        &info, wasm_engine->GetOrCreateTurboStatistics(), &zone_stats));
    pipeline_statistics->BeginPhaseKind("V8.WasmStubCodegen");
  }

  PipelineImpl pipeline(&data);

  if (info.trace_turbo_json() || info.trace_turbo_graph()) {
    CodeTracer::StreamScope tracing_scope(data.GetCodeTracer());
    tracing_scope.stream()
        << "---------------------------------------------------\n"
        << "Begin compiling method " << info.GetDebugName().get()
        << " using TurboFan" << std::endl;
  }

  if (info.trace_turbo_graph()) {  // Simple textual RPO.
    StdoutStream{} << "-- wasm stub " << CodeKindToString(kind) << " graph -- "
                   << std::endl
                   << AsRPO(*graph);
  }

  if (info.trace_turbo_json()) {
    TurboJsonFile json_of(&info, std::ios_base::trunc);
    json_of << "{\"function\":\"" << info.GetDebugName().get()
            << "\", \"source\":\"\",\n\"phases\":[";
  }

  pipeline.RunPrintAndVerify("V8.WasmNativeStubMachineCode", true);
  pipeline.ComputeScheduledGraph();

  Linkage linkage(call_descriptor);
  CHECK(pipeline.SelectInstructions(&linkage));
  pipeline.AssembleCode(&linkage, instruction_buffer->CreateView());

  CodeGenerator* code_generator = pipeline.code_generator();
  wasm::WasmCompilationResult result;
  code_generator->tasm()->GetCode(
      nullptr, &result.code_desc, code_generator->safepoint_table_builder(),
      static_cast<int>(code_generator->GetHandlerTableOffset()));
  result.instr_buffer = instruction_buffer->ReleaseBuffer();
  result.source_positions = code_generator->GetSourcePositionTable();
  result.protected_instructions_data =
      code_generator->GetProtectedInstructionsData();
  result.frame_slot_count = code_generator->frame()->GetTotalFrameSlotCount();
  result.tagged_parameter_slots = call_descriptor->GetTaggedParameterSlots();
  result.result_tier = wasm::ExecutionTier::kTurbofan;

  DCHECK(result.succeeded());

  if (info.trace_turbo_json()) {
    TurboJsonFile json_of(&info, std::ios_base::app);
    json_of << "{\"name\":\"disassembly\",\"type\":\"disassembly\""
            << BlockStartsAsJSON{&code_generator->block_starts()}
            << "\"data\":\"";
#ifdef ENABLE_DISASSEMBLER
    std::stringstream disassembler_stream;
    Disassembler::Decode(
        nullptr, &disassembler_stream, result.code_desc.buffer,
        result.code_desc.buffer + result.code_desc.safepoint_table_offset,
        CodeReference(&result.code_desc));
    for (auto const c : disassembler_stream.str()) {
      json_of << AsEscapedUC16ForJSON(c);
    }
#endif  // ENABLE_DISASSEMBLER
    json_of << "\"}\n]";
    json_of << "\n}";
  }

  if (info.trace_turbo_json() || info.trace_turbo_graph()) {
    CodeTracer::StreamScope tracing_scope(data.GetCodeTracer());
    tracing_scope.stream()
        << "---------------------------------------------------\n"
        << "Finished compiling method " << info.GetDebugName().get()
        << " using TurboFan" << std::endl;
  }

  return result;
}

// static
MaybeHandle<Code> Pipeline::GenerateCodeForTesting(
    OptimizedCompilationInfo* info, Isolate* isolate,
    std::unique_ptr<JSHeapBroker>* out_broker) {
  ZoneStats zone_stats(isolate->allocator());
  std::unique_ptr<PipelineStatistics> pipeline_statistics(
      CreatePipelineStatistics(Handle<Script>::null(), info, isolate,
                               &zone_stats));

  PipelineData data(&zone_stats, isolate, info, pipeline_statistics.get(),
                    i::FLAG_concurrent_inlining);
  PipelineImpl pipeline(&data);

  Linkage linkage(Linkage::ComputeIncoming(data.instruction_zone(), info));

  {
    CompilationHandleScope compilation_scope(isolate, info);
    CanonicalHandleScope canonical(isolate, info);
    info->ReopenHandlesInNewHandleScope(isolate);
    pipeline.Serialize();
    // Emulating the proper pipeline, we call CreateGraph on different places
    // (i.e before or after creating a LocalIsolateScope) depending on
    // is_concurrent_inlining.
    if (!data.broker()->is_concurrent_inlining()) {
      if (!pipeline.CreateGraph()) return MaybeHandle<Code>();
    }
  }

  {
    LocalIsolateScope local_isolate_scope(data.broker(), info,
                                          isolate->main_thread_local_isolate());
    if (data.broker()->is_concurrent_inlining()) {
      if (!pipeline.CreateGraph()) return MaybeHandle<Code>();
    }
    // We selectively Unpark inside OptimizeGraph.
    if (!pipeline.OptimizeGraph(&linkage)) return MaybeHandle<Code>();

    pipeline.AssembleCode(&linkage);
  }

  const bool will_retire_broker = out_broker == nullptr;
  if (!will_retire_broker) {
    // If the broker is going to be kept alive, pass the persistent and the
    // canonical handles containers back to the JSHeapBroker since it will
    // outlive the OptimizedCompilationInfo.
    data.broker()->SetPersistentAndCopyCanonicalHandlesForTesting(
        info->DetachPersistentHandles(), info->DetachCanonicalHandles());
  }

  Handle<Code> code;
  if (pipeline.FinalizeCode(will_retire_broker).ToHandle(&code) &&
      pipeline.CommitDependencies(code)) {
    if (!will_retire_broker) *out_broker = data.ReleaseBroker();
    return code;
  }
  return MaybeHandle<Code>();
}

// static
MaybeHandle<Code> Pipeline::GenerateCodeForTesting(
    OptimizedCompilationInfo* info, Isolate* isolate,
    CallDescriptor* call_descriptor, Graph* graph,
    const AssemblerOptions& options, Schedule* schedule) {
  // Construct a pipeline for scheduling and code generation.
  ZoneStats zone_stats(isolate->allocator());
  NodeOriginTable* node_positions = info->zone()->New<NodeOriginTable>(graph);
  PipelineData data(&zone_stats, info, isolate, isolate->allocator(), graph,
                    nullptr, schedule, nullptr, node_positions, nullptr,
                    options, nullptr);
  std::unique_ptr<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics.reset(new PipelineStatistics(
        info, isolate->GetTurboStatistics(), &zone_stats));
    pipeline_statistics->BeginPhaseKind("V8.TFTestCodegen");
  }

  PipelineImpl pipeline(&data);

  if (info->trace_turbo_json()) {
    TurboJsonFile json_of(info, std::ios_base::trunc);
    json_of << "{\"function\":\"" << info->GetDebugName().get()
            << "\", \"source\":\"\",\n\"phases\":[";
  }
  // TODO(rossberg): Should this really be untyped?
  pipeline.RunPrintAndVerify("V8.TFMachineCode", true);

  // Ensure we have a schedule.
  if (data.schedule() == nullptr) {
    pipeline.ComputeScheduledGraph();
  }

  Handle<Code> code;
  if (pipeline.GenerateCode(call_descriptor).ToHandle(&code) &&
      pipeline.CommitDependencies(code)) {
    return code;
  }
  return MaybeHandle<Code>();
}

// static
std::unique_ptr<OptimizedCompilationJob> Pipeline::NewCompilationJob(
    Isolate* isolate, Handle<JSFunction> function, CodeKind code_kind,
    bool has_script, BytecodeOffset osr_offset, JavaScriptFrame* osr_frame) {
  Handle<SharedFunctionInfo> shared =
      handle(function->shared(), function->GetIsolate());
  return std::make_unique<PipelineCompilationJob>(
      isolate, shared, function, osr_offset, osr_frame, code_kind);
}

// static
void Pipeline::GenerateCodeForWasmFunction(
    OptimizedCompilationInfo* info, wasm::WasmEngine* wasm_engine,
    MachineGraph* mcgraph, CallDescriptor* call_descriptor,
    SourcePositionTable* source_positions, NodeOriginTable* node_origins,
    wasm::FunctionBody function_body, const wasm::WasmModule* module,
    int function_index) {
  ZoneStats zone_stats(wasm_engine->allocator());
  std::unique_ptr<PipelineStatistics> pipeline_statistics(
      CreatePipelineStatistics(wasm_engine, function_body, module, info,
                               &zone_stats));
  // {instruction_buffer} must live longer than {PipelineData}, since
  // {PipelineData} will reference the {instruction_buffer} via the
  // {AssemblerBuffer} of the {Assembler} contained in the {CodeGenerator}.
  std::unique_ptr<wasm::WasmInstructionBuffer> instruction_buffer =
      wasm::WasmInstructionBuffer::New();
  PipelineData data(&zone_stats, wasm_engine, info, mcgraph,
                    pipeline_statistics.get(), source_positions, node_origins,
                    WasmAssemblerOptions());

  PipelineImpl pipeline(&data);

  if (data.info()->trace_turbo_json() || data.info()->trace_turbo_graph()) {
    CodeTracer::StreamScope tracing_scope(data.GetCodeTracer());
    tracing_scope.stream()
        << "---------------------------------------------------\n"
        << "Begin compiling method " << data.info()->GetDebugName().get()
        << " using TurboFan" << std::endl;
  }

  pipeline.RunPrintAndVerify("V8.WasmMachineCode", true);

  if (FLAG_wasm_loop_unrolling) {
    pipeline.Run<LoopExitEliminationPhase>();
    pipeline.RunPrintAndVerify("V8.LoopExitEliminationPhase", true);
  }

  data.BeginPhaseKind("V8.WasmOptimization");
  const bool is_asm_js = is_asmjs_module(module);
  if (FLAG_turbo_splitting && !is_asm_js) {
    data.info()->set_splitting();
  }
  if (FLAG_wasm_opt || is_asm_js) {
    PipelineRunScope scope(&data, "V8.WasmFullOptimization",
                           RuntimeCallCounterId::kOptimizeWasmFullOptimization);
    GraphReducer graph_reducer(
        scope.zone(), data.graph(), &data.info()->tick_counter(), data.broker(),
        data.mcgraph()->Dead(), data.observe_node_manager());
    DeadCodeElimination dead_code_elimination(&graph_reducer, data.graph(),
                                              data.common(), scope.zone());
    ValueNumberingReducer value_numbering(scope.zone(), data.graph()->zone());
    const bool allow_signalling_nan = is_asm_js;
    MachineOperatorReducer machine_reducer(&graph_reducer, data.mcgraph(),
                                           allow_signalling_nan);
    CommonOperatorReducer common_reducer(&graph_reducer, data.graph(),
                                         data.broker(), data.common(),
                                         data.machine(), scope.zone());
    AddReducer(&data, &graph_reducer, &dead_code_elimination);
    AddReducer(&data, &graph_reducer, &machine_reducer);
    AddReducer(&data, &graph_reducer, &common_reducer);
    AddReducer(&data, &graph_reducer, &value_numbering);
    graph_reducer.ReduceGraph();
  } else {
    PipelineRunScope scope(&data, "V8.OptimizeWasmBaseOptimization",
                           RuntimeCallCounterId::kOptimizeWasmBaseOptimization);
    GraphReducer graph_reducer(
        scope.zone(), data.graph(), &data.info()->tick_counter(), data.broker(),
        data.mcgraph()->Dead(), data.observe_node_manager());
    ValueNumberingReducer value_numbering(scope.zone(), data.graph()->zone());
    AddReducer(&data, &graph_reducer, &value_numbering);
    graph_reducer.ReduceGraph();
  }
  pipeline.RunPrintAndVerify("V8.WasmOptimization", true);

  if (data.node_origins()) {
    data.node_origins()->RemoveDecorator();
  }

  pipeline.ComputeScheduledGraph();

  Linkage linkage(call_descriptor);
  if (!pipeline.SelectInstructions(&linkage)) return;
  pipeline.AssembleCode(&linkage, instruction_buffer->CreateView());

  auto result = std::make_unique<wasm::WasmCompilationResult>();
  CodeGenerator* code_generator = pipeline.code_generator();
  code_generator->tasm()->GetCode(
      nullptr, &result->code_desc, code_generator->safepoint_table_builder(),
      static_cast<int>(code_generator->GetHandlerTableOffset()));

  result->instr_buffer = instruction_buffer->ReleaseBuffer();
  result->frame_slot_count = code_generator->frame()->GetTotalFrameSlotCount();
  result->tagged_parameter_slots = call_descriptor->GetTaggedParameterSlots();
  result->source_positions = code_generator->GetSourcePositionTable();
  result->protected_instructions_data =
      code_generator->GetProtectedInstructionsData();
  result->result_tier = wasm::ExecutionTier::kTurbofan;

  if (data.info()->trace_turbo_json()) {
    TurboJsonFile json_of(data.info(), std::ios_base::app);
    json_of << "{\"name\":\"disassembly\",\"type\":\"disassembly\""
            << BlockStartsAsJSON{&code_generator->block_starts()}
            << "\"data\":\"";
#ifdef ENABLE_DISASSEMBLER
    std::stringstream disassembler_stream;
    Disassembler::Decode(
        nullptr, &disassembler_stream, result->code_desc.buffer,
        result->code_desc.buffer + result->code_desc.safepoint_table_offset,
        CodeReference(&result->code_desc));
    for (auto const c : disassembler_stream.str()) {
      json_of << AsEscapedUC16ForJSON(c);
    }
#endif  // ENABLE_DISASSEMBLER
    json_of << "\"}\n]";
    json_of << "\n}";
  }

  if (data.info()->trace_turbo_json() || data.info()->trace_turbo_graph()) {
    CodeTracer::StreamScope tracing_scope(data.GetCodeTracer());
    tracing_scope.stream()
        << "---------------------------------------------------\n"
        << "Finished compiling method " << data.info()->GetDebugName().get()
        << " using TurboFan" << std::endl;
  }

  DCHECK(result->succeeded());
  info->SetWasmCompilationResult(std::move(result));
}

bool Pipeline::AllocateRegistersForTesting(const RegisterConfiguration* config,
                                           InstructionSequence* sequence,
                                           bool use_mid_tier_register_allocator,
                                           bool run_verifier) {
  OptimizedCompilationInfo info(ArrayVector("testing"), sequence->zone(),
                                CodeKind::FOR_TESTING);
  ZoneStats zone_stats(sequence->isolate()->allocator());
  PipelineData data(&zone_stats, &info, sequence->isolate(), sequence);
  data.InitializeFrameData(nullptr);

  if (info.trace_turbo_json()) {
    TurboJsonFile json_of(&info, std::ios_base::trunc);
    json_of << "{\"function\":\"" << info.GetDebugName().get()
            << "\", \"source\":\"\",\n\"phases\":[";
  }

  PipelineImpl pipeline(&data);
  if (use_mid_tier_register_allocator) {
    pipeline.AllocateRegistersForMidTier(config, nullptr, run_verifier);
  } else {
    pipeline.AllocateRegistersForTopTier(config, nullptr, run_verifier);
  }

  return !data.compilation_failed();
}

void PipelineImpl::ComputeScheduledGraph() {
  PipelineData* data = this->data_;

  // We should only schedule the graph if it is not scheduled yet.
  DCHECK_NULL(data->schedule());

  Run<LateGraphTrimmingPhase>();
  RunPrintAndVerify(LateGraphTrimmingPhase::phase_name(), true);

  Run<ComputeSchedulePhase>();
  TraceScheduleAndVerify(data->info(), data, data->schedule(), "schedule");
}

bool PipelineImpl::SelectInstructions(Linkage* linkage) {
  auto call_descriptor = linkage->GetIncomingDescriptor();
  PipelineData* data = this->data_;

  // We should have a scheduled graph.
  DCHECK_NOT_NULL(data->graph());
  DCHECK_NOT_NULL(data->schedule());

  if (FLAG_turbo_profiling) {
    UnparkedScopeIfNeeded unparked_scope(data->broker());
    data->info()->set_profiler_data(BasicBlockInstrumentor::Instrument(
        info(), data->graph(), data->schedule(), data->isolate()));
  }

  bool verify_stub_graph =
      data->verify_graph() ||
      (FLAG_turbo_verify_machine_graph != nullptr &&
       (!strcmp(FLAG_turbo_verify_machine_graph, "*") ||
        !strcmp(FLAG_turbo_verify_machine_graph, data->debug_name())));
  // Jump optimization runs instruction selection twice, but the instruction
  // selector mutates nodes like swapping the inputs of a load, which can
  // violate the machine graph verification rules. So we skip the second
  // verification on a graph that already verified before.
  auto jump_opt = data->jump_optimization_info();
  if (jump_opt && jump_opt->is_optimizing()) {
    verify_stub_graph = false;
  }
  if (verify_stub_graph) {
    if (FLAG_trace_verify_csa) {
      UnparkedScopeIfNeeded scope(data->broker());
      AllowHandleDereference allow_deref;
      CodeTracer::StreamScope tracing_scope(data->GetCodeTracer());
      tracing_scope.stream()
          << "--------------------------------------------------\n"
          << "--- Verifying " << data->debug_name()
          << " generated by TurboFan\n"
          << "--------------------------------------------------\n"
          << *data->schedule()
          << "--------------------------------------------------\n"
          << "--- End of " << data->debug_name() << " generated by TurboFan\n"
          << "--------------------------------------------------\n";
    }
    // TODO(jgruber): The parameter is called is_stub but actually contains
    // something different. Update either the name or its contents.
    const bool is_stub =
        !data->info()->IsOptimizing() && !data->info()->IsWasm();
    Zone temp_zone(data->allocator(), kMachineGraphVerifierZoneName);
    MachineGraphVerifier::Run(data->graph(), data->schedule(), linkage, is_stub,
                              data->debug_name(), &temp_zone);
  }

  data->InitializeInstructionSequence(call_descriptor);

  // Depending on which code path led us to this function, the frame may or
  // may not have been initialized. If it hasn't yet, initialize it now.
  if (!data->frame()) {
    data->InitializeFrameData(call_descriptor);
  }
  // Select and schedule instructions covering the scheduled graph.
  Run<InstructionSelectionPhase>(linkage);
  if (data->compilation_failed()) {
    info()->AbortOptimization(BailoutReason::kCodeGenerationFailed);
    data->EndPhaseKind();
    return false;
  }

  if (info()->trace_turbo_json() && !data->MayHaveUnverifiableGraph()) {
    UnparkedScopeIfNeeded scope(data->broker());
    AllowHandleDereference allow_deref;
    TurboCfgFile tcf(isolate());
    tcf << AsC1V("CodeGen", data->schedule(), data->source_positions(),
                 data->sequence());
  }

  if (info()->trace_turbo_json()) {
    std::ostringstream source_position_output;
    // Output source position information before the graph is deleted.
    if (data_->source_positions() != nullptr) {
      data_->source_positions()->PrintJson(source_position_output);
    } else {
      source_position_output << "{}";
    }
    source_position_output << ",\n\"NodeOrigins\" : ";
    data_->node_origins()->PrintJson(source_position_output);
    data_->set_source_position_output(source_position_output.str());
  }

  data->DeleteGraphZone();

  data->BeginPhaseKind("V8.TFRegisterAllocation");

  bool run_verifier = FLAG_turbo_verify_allocation;

  // Allocate registers.
  if (call_descriptor->HasRestrictedAllocatableRegisters()) {
    RegList registers = call_descriptor->AllocatableRegisters();
    DCHECK_LT(0, NumRegs(registers));
    std::unique_ptr<const RegisterConfiguration> config;
    config.reset(RegisterConfiguration::RestrictGeneralRegisters(registers));
    AllocateRegistersForTopTier(config.get(), call_descriptor, run_verifier);
  } else {
    const RegisterConfiguration* config;
    if (data->info()->GetPoisoningMitigationLevel() !=
        PoisoningMitigationLevel::kDontPoison) {
#ifdef V8_TARGET_ARCH_IA32
    FATAL("Poisoning is not supported on ia32.");
#else
      config = RegisterConfiguration::Poisoning();
#endif  // V8_TARGET_ARCH_IA32
    } else {
      config = RegisterConfiguration::Default();
    }

    if (data->info()->IsTurboprop() && FLAG_turboprop_mid_tier_reg_alloc) {
      AllocateRegistersForMidTier(config, call_descriptor, run_verifier);
    } else {
      AllocateRegistersForTopTier(config, call_descriptor, run_verifier);
    }
  }

  // Verify the instruction sequence has the same hash in two stages.
  VerifyGeneratedCodeIsIdempotent();

  Run<FrameElisionPhase>();
  if (data->compilation_failed()) {
    info()->AbortOptimization(
        BailoutReason::kNotEnoughVirtualRegistersRegalloc);
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

void PipelineImpl::VerifyGeneratedCodeIsIdempotent() {
  PipelineData* data = this->data_;
  JumpOptimizationInfo* jump_opt = data->jump_optimization_info();
  if (jump_opt == nullptr) return;

  InstructionSequence* code = data->sequence();
  int instruction_blocks = code->InstructionBlockCount();
  int virtual_registers = code->VirtualRegisterCount();
  size_t hash_code = base::hash_combine(instruction_blocks, virtual_registers);
  for (auto instr : *code) {
    hash_code = base::hash_combine(hash_code, instr->opcode(),
                                   instr->InputCount(), instr->OutputCount());
  }
  for (int i = 0; i < virtual_registers; i++) {
    hash_code = base::hash_combine(hash_code, code->GetRepresentation(i));
  }
  if (jump_opt->is_collecting()) {
    jump_opt->set_hash_code(hash_code);
  } else {
    CHECK_EQ(hash_code, jump_opt->hash_code());
  }
}

struct InstructionStartsAsJSON {
  const ZoneVector<TurbolizerInstructionStartInfo>* instr_starts;
};

std::ostream& operator<<(std::ostream& out, const InstructionStartsAsJSON& s) {
  out << ", \"instructionOffsetToPCOffset\": {";
  bool need_comma = false;
  for (size_t i = 0; i < s.instr_starts->size(); ++i) {
    if (need_comma) out << ", ";
    const TurbolizerInstructionStartInfo& info = (*s.instr_starts)[i];
    out << "\"" << i << "\": {";
    out << "\"gap\": " << info.gap_pc_offset;
    out << ", \"arch\": " << info.arch_instr_pc_offset;
    out << ", \"condition\": " << info.condition_pc_offset;
    out << "}";
    need_comma = true;
  }
  out << "}";
  return out;
}

struct TurbolizerCodeOffsetsInfoAsJSON {
  const TurbolizerCodeOffsetsInfo* offsets_info;
};

std::ostream& operator<<(std::ostream& out,
                         const TurbolizerCodeOffsetsInfoAsJSON& s) {
  out << ", \"codeOffsetsInfo\": {";
  out << "\"codeStartRegisterCheck\": "
      << s.offsets_info->code_start_register_check << ", ";
  out << "\"deoptCheck\": " << s.offsets_info->deopt_check << ", ";
  out << "\"initPoison\": " << s.offsets_info->init_poison << ", ";
  out << "\"blocksStart\": " << s.offsets_info->blocks_start << ", ";
  out << "\"outOfLineCode\": " << s.offsets_info->out_of_line_code << ", ";
  out << "\"deoptimizationExits\": " << s.offsets_info->deoptimization_exits
      << ", ";
  out << "\"pools\": " << s.offsets_info->pools << ", ";
  out << "\"jumpTables\": " << s.offsets_info->jump_tables;
  out << "}";
  return out;
}

void PipelineImpl::AssembleCode(Linkage* linkage,
                                std::unique_ptr<AssemblerBuffer> buffer) {
  PipelineData* data = this->data_;
  data->BeginPhaseKind("V8.TFCodeGeneration");
  data->InitializeCodeGenerator(linkage, std::move(buffer));

  UnparkedScopeIfNeeded unparked_scope(data->broker(), FLAG_code_comments);

  Run<AssembleCodePhase>();
  if (data->info()->trace_turbo_json()) {
    TurboJsonFile json_of(data->info(), std::ios_base::app);
    json_of << "{\"name\":\"code generation\""
            << ", \"type\":\"instructions\""
            << InstructionStartsAsJSON{&data->code_generator()->instr_starts()}
            << TurbolizerCodeOffsetsInfoAsJSON{
                   &data->code_generator()->offsets_info()};
    json_of << "},\n";
  }
  data->DeleteInstructionZone();
  data->EndPhaseKind();
}

MaybeHandle<Code> PipelineImpl::FinalizeCode(bool retire_broker) {
  PipelineData* data = this->data_;
  data->BeginPhaseKind("V8.TFFinalizeCode");
  if (data->broker() && retire_broker) {
    data->broker()->Retire();
  }
  Run<FinalizeCodePhase>();

  MaybeHandle<Code> maybe_code = data->code();
  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    return maybe_code;
  }

  info()->SetCode(code);
  PrintCode(isolate(), code, info());

  if (info()->trace_turbo_json()) {
    TurboJsonFile json_of(info(), std::ios_base::app);

    json_of << "{\"name\":\"disassembly\",\"type\":\"disassembly\""
            << BlockStartsAsJSON{&data->code_generator()->block_starts()}
            << "\"data\":\"";
#ifdef ENABLE_DISASSEMBLER
    std::stringstream disassembly_stream;
    code->Disassemble(nullptr, disassembly_stream, isolate());
    std::string disassembly_string(disassembly_stream.str());
    for (const auto& c : disassembly_string) {
      json_of << AsEscapedUC16ForJSON(c);
    }
#endif  // ENABLE_DISASSEMBLER
    json_of << "\"}\n],\n";
    json_of << "\"nodePositions\":";
    json_of << data->source_position_output() << ",\n";
    JsonPrintAllSourceWithPositions(json_of, data->info(), isolate());
    json_of << "\n}";
  }
  if (info()->trace_turbo_json() || info()->trace_turbo_graph()) {
    CodeTracer::StreamScope tracing_scope(data->GetCodeTracer());
    tracing_scope.stream()
        << "---------------------------------------------------\n"
        << "Finished compiling method " << info()->GetDebugName().get()
        << " using TurboFan" << std::endl;
  }
  data->EndPhaseKind();
  return code;
}

bool PipelineImpl::SelectInstructionsAndAssemble(
    CallDescriptor* call_descriptor) {
  Linkage linkage(call_descriptor);

  // Perform instruction selection and register allocation.
  if (!SelectInstructions(&linkage)) return false;

  // Generate the final machine code.
  AssembleCode(&linkage);
  return true;
}

MaybeHandle<Code> PipelineImpl::GenerateCode(CallDescriptor* call_descriptor) {
  if (!SelectInstructionsAndAssemble(call_descriptor)) {
    return MaybeHandle<Code>();
  }
  return FinalizeCode();
}

bool PipelineImpl::CommitDependencies(Handle<Code> code) {
  return data_->dependencies() == nullptr ||
         data_->dependencies()->Commit(code);
}

namespace {

void TraceSequence(OptimizedCompilationInfo* info, PipelineData* data,
                   const char* phase_name) {
  if (info->trace_turbo_json()) {
    UnparkedScopeIfNeeded scope(data->broker());
    AllowHandleDereference allow_deref;
    TurboJsonFile json_of(info, std::ios_base::app);
    json_of << "{\"name\":\"" << phase_name << "\",\"type\":\"sequence\""
            << ",\"blocks\":" << InstructionSequenceAsJSON{data->sequence()}
            << ",\"register_allocation\":{"
            << RegisterAllocationDataAsJSON{*(data->register_allocation_data()),
                                            *(data->sequence())}
            << "}},\n";
  }
  if (info->trace_turbo_graph()) {
    UnparkedScopeIfNeeded scope(data->broker());
    AllowHandleDereference allow_deref;
    CodeTracer::StreamScope tracing_scope(data->GetCodeTracer());
    tracing_scope.stream() << "----- Instruction sequence " << phase_name
                           << " -----\n"
                           << *data->sequence();
  }
}

}  // namespace

void PipelineImpl::AllocateRegistersForTopTier(
    const RegisterConfiguration* config, CallDescriptor* call_descriptor,
    bool run_verifier) {
  PipelineData* data = this->data_;
  // Don't track usage for this zone in compiler stats.
  std::unique_ptr<Zone> verifier_zone;
  RegisterAllocatorVerifier* verifier = nullptr;
  if (run_verifier) {
    verifier_zone.reset(
        new Zone(data->allocator(), kRegisterAllocatorVerifierZoneName));
    verifier = verifier_zone->New<RegisterAllocatorVerifier>(
        verifier_zone.get(), config, data->sequence(), data->frame());
  }

#ifdef DEBUG
  data_->sequence()->ValidateEdgeSplitForm();
  data_->sequence()->ValidateDeferredBlockEntryPaths();
  data_->sequence()->ValidateDeferredBlockExitPaths();
#endif

  RegisterAllocationFlags flags;
  if (data->info()->trace_turbo_allocation()) {
    flags |= RegisterAllocationFlag::kTraceAllocation;
  }
  data->InitializeTopTierRegisterAllocationData(config, call_descriptor, flags);

  Run<MeetRegisterConstraintsPhase>();
  Run<ResolvePhisPhase>();
  Run<BuildLiveRangesPhase>();
  Run<BuildBundlesPhase>();

  TraceSequence(info(), data, "before register allocation");
  if (verifier != nullptr) {
    CHECK(!data->top_tier_register_allocation_data()
               ->ExistsUseWithoutDefinition());
    CHECK(data->top_tier_register_allocation_data()
              ->RangesDefinedInDeferredStayInDeferred());
  }

  if (info()->trace_turbo_json() && !data->MayHaveUnverifiableGraph()) {
    TurboCfgFile tcf(isolate());
    tcf << AsC1VRegisterAllocationData(
        "PreAllocation", data->top_tier_register_allocation_data());
  }

  Run<AllocateGeneralRegistersPhase<LinearScanAllocator>>();

  if (data->sequence()->HasFPVirtualRegisters()) {
    Run<AllocateFPRegistersPhase<LinearScanAllocator>>();
  }

  Run<DecideSpillingModePhase>();
  Run<AssignSpillSlotsPhase>();
  Run<CommitAssignmentPhase>();

  // TODO(chromium:725559): remove this check once
  // we understand the cause of the bug. We keep just the
  // check at the end of the allocation.
  if (verifier != nullptr) {
    verifier->VerifyAssignment("Immediately after CommitAssignmentPhase.");
  }


  Run<ConnectRangesPhase>();

  Run<ResolveControlFlowPhase>();

  Run<PopulateReferenceMapsPhase>();

  if (FLAG_turbo_move_optimization) {
    Run<OptimizeMovesPhase>();
  }

  TraceSequence(info(), data, "after register allocation");

  if (verifier != nullptr) {
    verifier->VerifyAssignment("End of regalloc pipeline.");
    verifier->VerifyGapMoves();
  }

  if (info()->trace_turbo_json() && !data->MayHaveUnverifiableGraph()) {
    TurboCfgFile tcf(isolate());
    tcf << AsC1VRegisterAllocationData(
        "CodeGen", data->top_tier_register_allocation_data());
  }

  data->DeleteRegisterAllocationZone();
}

void PipelineImpl::AllocateRegistersForMidTier(
    const RegisterConfiguration* config, CallDescriptor* call_descriptor,
    bool run_verifier) {
  PipelineData* data = data_;
  // Don't track usage for this zone in compiler stats.
  std::unique_ptr<Zone> verifier_zone;
  RegisterAllocatorVerifier* verifier = nullptr;
  if (run_verifier) {
    verifier_zone.reset(
        new Zone(data->allocator(), kRegisterAllocatorVerifierZoneName));
    verifier = verifier_zone->New<RegisterAllocatorVerifier>(
        verifier_zone.get(), config, data->sequence(), data->frame());
  }

#ifdef DEBUG
  data->sequence()->ValidateEdgeSplitForm();
  data->sequence()->ValidateDeferredBlockEntryPaths();
  data->sequence()->ValidateDeferredBlockExitPaths();
#endif
  data->InitializeMidTierRegisterAllocationData(config, call_descriptor);

  TraceSequence(info(), data, "before register allocation");

  Run<MidTierRegisterOutputDefinitionPhase>();

  Run<MidTierRegisterAllocatorPhase>();

  Run<MidTierSpillSlotAllocatorPhase>();

  Run<MidTierPopulateReferenceMapsPhase>();

  TraceSequence(info(), data, "after register allocation");

  if (verifier != nullptr) {
    verifier->VerifyAssignment("End of regalloc pipeline.");
    verifier->VerifyGapMoves();
  }

  data->DeleteRegisterAllocationZone();
}

OptimizedCompilationInfo* PipelineImpl::info() const { return data_->info(); }

Isolate* PipelineImpl::isolate() const { return data_->isolate(); }

CodeGenerator* PipelineImpl::code_generator() const {
  return data_->code_generator();
}

ObserveNodeManager* PipelineImpl::observe_node_manager() const {
  return data_->observe_node_manager();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

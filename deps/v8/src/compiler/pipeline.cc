// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/pipeline.h"

#include <fstream>  // NOLINT(readability/streams)
#include <iostream>
#include <memory>
#include <sstream>

#include "src/assembler-inl.h"
#include "src/base/adapters.h"
#include "src/base/optional.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/bootstrapper.h"
#include "src/code-tracer.h"
#include "src/compiler.h"
#include "src/compiler/basic-block-instrumentor.h"
#include "src/compiler/branch-elimination.h"
#include "src/compiler/bytecode-graph-builder.h"
#include "src/compiler/checkpoint-elimination.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/common-operator-reducer.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/constant-folding-reducer.h"
#include "src/compiler/control-flow-optimizer.h"
#include "src/compiler/dead-code-elimination.h"
#include "src/compiler/effect-control-linearizer.h"
#include "src/compiler/escape-analysis-reducer.h"
#include "src/compiler/escape-analysis.h"
#include "src/compiler/frame-elider.h"
#include "src/compiler/graph-trimmer.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/instruction.h"
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
#include "src/compiler/node-origin-table.h"
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
#include "src/compiler/type-narrowing-reducer.h"
#include "src/compiler/typed-optimization.h"
#include "src/compiler/typer.h"
#include "src/compiler/value-numbering-reducer.h"
#include "src/compiler/verifier.h"
#include "src/compiler/wasm-compiler.h"
#include "src/compiler/zone-stats.h"
#include "src/disassembler.h"
#include "src/isolate-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/optimized-compilation-info.h"
#include "src/ostreams.h"
#include "src/parsing/parse-info.h"
#include "src/register-configuration.h"
#include "src/utils.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {

namespace trap_handler {
struct ProtectedInstructionData;
}  // namespace trap_handler

namespace compiler {

// Turbofan can only handle 2^16 control inputs. Since each control flow split
// requires at least two bytes (jump and offset), we limit the bytecode size
// to 128K bytes.
const int kMaxBytecodeSizeForTurbofan = 128 * 1024;

class PipelineData {
 public:
  // For main entry point.
  PipelineData(ZoneStats* zone_stats, Isolate* isolate,
               OptimizedCompilationInfo* info,
               PipelineStatistics* pipeline_statistics)
      : isolate_(isolate),
        allocator_(isolate->allocator()),
        info_(info),
        debug_name_(info_->GetDebugName()),
        may_have_unverifiable_graph_(false),
        zone_stats_(zone_stats),
        pipeline_statistics_(pipeline_statistics),
        graph_zone_scope_(zone_stats_, ZONE_NAME),
        graph_zone_(graph_zone_scope_.zone()),
        instruction_zone_scope_(zone_stats_, ZONE_NAME),
        instruction_zone_(instruction_zone_scope_.zone()),
        codegen_zone_scope_(zone_stats_, ZONE_NAME),
        codegen_zone_(codegen_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_stats_, ZONE_NAME),
        register_allocation_zone_(register_allocation_zone_scope_.zone()),
        assembler_options_(AssemblerOptions::Default(isolate)) {
    PhaseScope scope(pipeline_statistics, "init pipeline data");
    graph_ = new (graph_zone_) Graph(graph_zone_);
    source_positions_ = new (graph_zone_) SourcePositionTable(graph_);
    node_origins_ = info->trace_turbo_json_enabled()
                        ? new (graph_zone_) NodeOriginTable(graph_)
                        : nullptr;
    simplified_ = new (graph_zone_) SimplifiedOperatorBuilder(graph_zone_);
    machine_ = new (graph_zone_) MachineOperatorBuilder(
        graph_zone_, MachineType::PointerRepresentation(),
        InstructionSelector::SupportedMachineOperatorFlags(),
        InstructionSelector::AlignmentRequirements());
    common_ = new (graph_zone_) CommonOperatorBuilder(graph_zone_);
    javascript_ = new (graph_zone_) JSOperatorBuilder(graph_zone_);
    jsgraph_ = new (graph_zone_)
        JSGraph(isolate_, graph_, common_, javascript_, simplified_, machine_);
    js_heap_broker_ = new (info_->zone()) JSHeapBroker(isolate_, info_->zone());
    dependencies_ =
        new (info_->zone()) CompilationDependencies(isolate_, info_->zone());
  }

  // For WebAssembly compile entry point.
  PipelineData(ZoneStats* zone_stats, wasm::WasmEngine* wasm_engine,
               OptimizedCompilationInfo* info, MachineGraph* mcgraph,
               PipelineStatistics* pipeline_statistics,
               SourcePositionTable* source_positions,
               NodeOriginTable* node_origins, int wasm_function_index,
               const AssemblerOptions& assembler_options)
      : isolate_(nullptr),
        wasm_engine_(wasm_engine),
        allocator_(wasm_engine->allocator()),
        info_(info),
        debug_name_(info_->GetDebugName()),
        wasm_function_index_(wasm_function_index),
        may_have_unverifiable_graph_(false),
        zone_stats_(zone_stats),
        pipeline_statistics_(pipeline_statistics),
        graph_zone_scope_(zone_stats_, ZONE_NAME),
        graph_zone_(graph_zone_scope_.zone()),
        graph_(mcgraph->graph()),
        source_positions_(source_positions),
        node_origins_(node_origins),
        machine_(mcgraph->machine()),
        common_(mcgraph->common()),
        mcgraph_(mcgraph),
        instruction_zone_scope_(zone_stats_, ZONE_NAME),
        instruction_zone_(instruction_zone_scope_.zone()),
        codegen_zone_scope_(zone_stats_, ZONE_NAME),
        codegen_zone_(codegen_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_stats_, ZONE_NAME),
        register_allocation_zone_(register_allocation_zone_scope_.zone()),
        assembler_options_(assembler_options) {}

  // For machine graph testing entry point.
  PipelineData(ZoneStats* zone_stats, OptimizedCompilationInfo* info,
               Isolate* isolate, Graph* graph, Schedule* schedule,
               SourcePositionTable* source_positions,
               NodeOriginTable* node_origins, JumpOptimizationInfo* jump_opt,
               const AssemblerOptions& assembler_options)
      : isolate_(isolate),
        allocator_(isolate->allocator()),
        info_(info),
        debug_name_(info_->GetDebugName()),
        zone_stats_(zone_stats),
        graph_zone_scope_(zone_stats_, ZONE_NAME),
        graph_(graph),
        source_positions_(source_positions),
        node_origins_(node_origins),
        schedule_(schedule),
        instruction_zone_scope_(zone_stats_, ZONE_NAME),
        instruction_zone_(instruction_zone_scope_.zone()),
        codegen_zone_scope_(zone_stats_, ZONE_NAME),
        codegen_zone_(codegen_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_stats_, ZONE_NAME),
        register_allocation_zone_(register_allocation_zone_scope_.zone()),
        jump_optimization_info_(jump_opt),
        assembler_options_(assembler_options) {}

  // For register allocation testing entry point.
  PipelineData(ZoneStats* zone_stats, OptimizedCompilationInfo* info,
               Isolate* isolate, InstructionSequence* sequence)
      : isolate_(isolate),
        allocator_(isolate->allocator()),
        info_(info),
        debug_name_(info_->GetDebugName()),
        zone_stats_(zone_stats),
        graph_zone_scope_(zone_stats_, ZONE_NAME),
        instruction_zone_scope_(zone_stats_, ZONE_NAME),
        instruction_zone_(sequence->zone()),
        sequence_(sequence),
        codegen_zone_scope_(zone_stats_, ZONE_NAME),
        codegen_zone_(codegen_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_stats_, ZONE_NAME),
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
  Handle<Context> native_context() const {
    return handle(info()->native_context(), isolate());
  }
  Handle<JSGlobalObject> global_object() const {
    return handle(info()->global_object(), isolate());
  }

  JSHeapBroker* js_heap_broker() const { return js_heap_broker_; }

  Schedule* schedule() const { return schedule_; }
  void set_schedule(Schedule* schedule) {
    DCHECK(!schedule_);
    schedule_ = schedule;
  }
  void reset_schedule() { schedule_ = nullptr; }

  Zone* instruction_zone() const { return instruction_zone_; }
  Zone* codegen_zone() const { return codegen_zone_; }
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

  JumpOptimizationInfo* jump_optimization_info() const {
    return jump_optimization_info_;
  }

  const AssemblerOptions& assembler_options() const {
    return assembler_options_;
  }

  CodeTracer* GetCodeTracer() const {
    return wasm_engine_ == nullptr ? isolate_->GetCodeTracer()
                                   : wasm_engine_->GetCodeTracer();
  }

  Typer* CreateTyper() {
    DCHECK_NULL(typer_);
    typer_ = new Typer(js_heap_broker(), typer_flags_, graph());
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
    js_heap_broker_ = nullptr;
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
    sequence_ = new (instruction_zone())
        InstructionSequence(isolate(), instruction_zone(), instruction_blocks);
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
      fixed_frame_size = call_descriptor->CalculateFixedFrameSize();
    }
    frame_ = new (codegen_zone()) Frame(fixed_frame_size);
  }

  void InitializeRegisterAllocationData(const RegisterConfiguration* config,
                                        CallDescriptor* call_descriptor) {
    DCHECK_NULL(register_allocation_data_);
    register_allocation_data_ = new (register_allocation_zone())
        RegisterAllocationData(config, register_allocation_zone(), frame(),
                               sequence(), debug_name());
  }

  void InitializeOsrHelper() {
    DCHECK(!osr_helper_.has_value());
    osr_helper_.emplace(info());
  }

  void set_start_source_position(int position) {
    DCHECK_EQ(start_source_position_, kNoSourcePosition);
    start_source_position_ = position;
  }

  void InitializeCodeGenerator(Linkage* linkage) {
    DCHECK_NULL(code_generator_);

    code_generator_ = new CodeGenerator(
        codegen_zone(), frame(), linkage, sequence(), info(), isolate(),
        osr_helper_, start_source_position_, jump_optimization_info_,
        info()->GetPoisoningMitigationLevel(), assembler_options_,
        info_->builtin_index());
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

  int wasm_function_index() const { return wasm_function_index_; }

 private:
  Isolate* const isolate_;
  wasm::WasmEngine* const wasm_engine_ = nullptr;
  AccountingAllocator* const allocator_;
  OptimizedCompilationInfo* const info_;
  std::unique_ptr<char[]> debug_name_;
  int wasm_function_index_ = -1;
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
  JSHeapBroker* js_heap_broker_ = nullptr;
  Frame* frame_ = nullptr;

  // All objects in the following group of fields are allocated in
  // register_allocation_zone_. They are all set to nullptr when the zone is
  // destroyed.
  ZoneStats::Scope register_allocation_zone_scope_;
  Zone* register_allocation_zone_;
  RegisterAllocationData* register_allocation_data_ = nullptr;

  // Basic block profiling support.
  BasicBlockProfiler::Data* profiler_data_ = nullptr;

  // Source position output for --trace-turbo.
  std::string source_position_output_;

  JumpOptimizationInfo* jump_optimization_info_ = nullptr;
  AssemblerOptions assembler_options_;

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

  // Step A. Run the graph creation and initial optimization passes.
  bool CreateGraph();

  // B. Run the concurrent optimization passes.
  bool OptimizeGraph(Linkage* linkage);

  // Substep B.1. Produce a scheduled graph.
  void ComputeScheduledGraph();

  // Substep B.2. Select instructions from a scheduled graph.
  bool SelectInstructions(Linkage* linkage);

  // Step C. Run the code assembly pass.
  void AssembleCode(Linkage* linkage);

  // Step D. Run the code finalization pass.
  MaybeHandle<Code> FinalizeCode();

  // Step E. Install any code dependencies.
  bool CommitDependencies(Handle<Code> code);

  void VerifyGeneratedCodeIsIdempotent();
  void RunPrintAndVerify(const char* phase, bool untyped = false);
  MaybeHandle<Code> GenerateCode(CallDescriptor* call_descriptor);
  void AllocateRegisters(const RegisterConfiguration* config,
                         CallDescriptor* call_descriptor, bool run_verifier);

  OptimizedCompilationInfo* info() const;
  Isolate* isolate() const;
  CodeGenerator* code_generator() const;

 private:
  PipelineData* const data_;
};

namespace {

void PrintFunctionSource(OptimizedCompilationInfo* info, Isolate* isolate,
                         int source_id, Handle<SharedFunctionInfo> shared) {
  if (!shared->script()->IsUndefined(isolate)) {
    Handle<Script> script(Script::cast(shared->script()), isolate);

    if (!script->source()->IsUndefined(isolate)) {
      CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
      Object* source_name = script->name();
      OFStream os(tracing_scope.file());
      os << "--- FUNCTION SOURCE (";
      if (source_name->IsString()) {
        os << String::cast(source_name)->ToCString().get() << ":";
      }
      os << shared->DebugName()->ToCString().get() << ") id{";
      os << info->optimization_id() << "," << source_id << "} start{";
      os << shared->StartPosition() << "} ---\n";
      {
        DisallowHeapAllocation no_allocation;
        int start = shared->StartPosition();
        int len = shared->EndPosition() - start;
        String::SubStringRange source(String::cast(script->source()), start,
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
  CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
  OFStream os(tracing_scope.file());
  os << "INLINE (" << h.shared_info->DebugName()->ToCString().get() << ") id{"
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
  AllowDeferredHandleDereference allow_deference_for_print_code;

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
  AllowDeferredHandleDereference allow_deference_for_print_code;
  bool print_code =
      isolate->bootstrapper()->IsActive()
          ? FLAG_print_builtin_code && info->shared_info()->PassesFilter(
                                           FLAG_print_builtin_code_filter)
          : (FLAG_print_code || (info->IsStub() && FLAG_print_code_stubs) ||
             (info->IsOptimizing() && FLAG_print_opt_code &&
              info->shared_info()->PassesFilter(FLAG_print_opt_code_filter)));
  if (print_code) {
    std::unique_ptr<char[]> debug_name = info->GetDebugName();
    CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
    OFStream os(tracing_scope.file());

    // Print the source code if available.
    bool print_source = code->kind() == Code::OPTIMIZED_FUNCTION;
    if (print_source) {
      Handle<SharedFunctionInfo> shared = info->shared_info();
      if (shared->script()->IsScript() &&
          !Script::cast(shared->script())->source()->IsUndefined(isolate)) {
        os << "--- Raw source ---\n";
        StringCharacterStream stream(
            String::cast(Script::cast(shared->script())->source()),
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
    code->Disassemble(debug_name.get(), os);
    os << "--- End code ---\n";
  }
#endif  // ENABLE_DISASSEMBLER
}

void TraceSchedule(OptimizedCompilationInfo* info, PipelineData* data,
                   Schedule* schedule, const char* phase_name) {
  if (info->trace_turbo_json_enabled()) {
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
  if (info->trace_turbo_graph_enabled() || FLAG_trace_turbo_scheduler) {
    AllowHandleDereference allow_deref;
    CodeTracer::Scope tracing_scope(data->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "-- Schedule --------------------------------------\n" << *schedule;
  }
}


class SourcePositionWrapper final : public Reducer {
 public:
  SourcePositionWrapper(Reducer* reducer, SourcePositionTable* table)
      : reducer_(reducer), table_(table) {}
  ~SourcePositionWrapper() final = default;

  const char* reducer_name() const override { return reducer_->reducer_name(); }

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

class NodeOriginsWrapper final : public Reducer {
 public:
  NodeOriginsWrapper(Reducer* reducer, NodeOriginTable* table)
      : reducer_(reducer), table_(table) {}
  ~NodeOriginsWrapper() final = default;

  const char* reducer_name() const override { return reducer_->reducer_name(); }

  Reduction Reduce(Node* node) final {
    NodeOriginTable::Scope position(table_, reducer_name(), node);
    return reducer_->Reduce(node);
  }

  void Finalize() final { reducer_->Finalize(); }

 private:
  Reducer* const reducer_;
  NodeOriginTable* const table_;

  DISALLOW_COPY_AND_ASSIGN(NodeOriginsWrapper);
};

void AddReducer(PipelineData* data, GraphReducer* graph_reducer,
                Reducer* reducer) {
  if (data->info()->is_source_positions_enabled()) {
    void* const buffer = data->graph_zone()->New(sizeof(SourcePositionWrapper));
    SourcePositionWrapper* const wrapper =
        new (buffer) SourcePositionWrapper(reducer, data->source_positions());
    reducer = wrapper;
  }
  if (data->info()->trace_turbo_json_enabled()) {
    void* const buffer = data->graph_zone()->New(sizeof(NodeOriginsWrapper));
    NodeOriginsWrapper* const wrapper =
        new (buffer) NodeOriginsWrapper(reducer, data->node_origins());
    reducer = wrapper;
  }

  graph_reducer->AddReducer(reducer);
}

class PipelineRunScope {
 public:
  PipelineRunScope(PipelineData* data, const char* phase_name)
      : phase_scope_(
            phase_name == nullptr ? nullptr : data->pipeline_statistics(),
            phase_name),
        zone_scope_(data->zone_stats(), ZONE_NAME),
        origin_scope_(data->node_origins(), phase_name) {}

  Zone* zone() { return zone_scope_.zone(); }

 private:
  PhaseScope phase_scope_;
  ZoneStats::Scope zone_scope_;
  NodeOriginTable::PhaseScope origin_scope_;
};

PipelineStatistics* CreatePipelineStatistics(Handle<Script> script,
                                             OptimizedCompilationInfo* info,
                                             Isolate* isolate,
                                             ZoneStats* zone_stats) {
  PipelineStatistics* pipeline_statistics = nullptr;

  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics =
        new PipelineStatistics(info, isolate->GetTurboStatistics(), zone_stats);
    pipeline_statistics->BeginPhaseKind("initializing");
  }

  if (info->trace_turbo_json_enabled()) {
    TurboJsonFile json_of(info, std::ios_base::trunc);
    json_of << "{\"function\" : ";
    JsonPrintFunctionSource(json_of, -1, info->GetDebugName(), script, isolate,
                            info->shared_info());
    json_of << ",\n\"phases\":[";
  }

  return pipeline_statistics;
}

PipelineStatistics* CreatePipelineStatistics(wasm::WasmEngine* wasm_engine,
                                             wasm::FunctionBody function_body,
                                             wasm::WasmModule* wasm_module,
                                             OptimizedCompilationInfo* info,
                                             ZoneStats* zone_stats) {
  PipelineStatistics* pipeline_statistics = nullptr;

  if (FLAG_turbo_stats_wasm) {
    pipeline_statistics = new PipelineStatistics(
        info, wasm_engine->GetOrCreateTurboStatistics(), zone_stats);
    pipeline_statistics->BeginPhaseKind("initializing");
  }

  if (info->trace_turbo_json_enabled()) {
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
                         Handle<JSFunction> function)
      // Note that the OptimizedCompilationInfo is not initialized at the time
      // we pass it to the CompilationJob constructor, but it is not
      // dereferenced there.
      : OptimizedCompilationJob(
            function->GetIsolate()->stack_guard()->real_climit(),
            &compilation_info_, "TurboFan"),
        zone_(function->GetIsolate()->allocator(), ZONE_NAME),
        zone_stats_(function->GetIsolate()->allocator()),
        compilation_info_(&zone_, function->GetIsolate(), shared_info,
                          function),
        pipeline_statistics_(CreatePipelineStatistics(
            handle(Script::cast(shared_info->script()), isolate),
            compilation_info(), function->GetIsolate(), &zone_stats_)),
        data_(&zone_stats_, function->GetIsolate(), compilation_info(),
              pipeline_statistics_.get()),
        pipeline_(&data_),
        linkage_(nullptr) {}

 protected:
  Status PrepareJobImpl(Isolate* isolate) final;
  Status ExecuteJobImpl() final;
  Status FinalizeJobImpl(Isolate* isolate) final;

  // Registers weak object to optimized code dependencies.
  void RegisterWeakObjectsInOptimizedCode(Handle<Code> code, Isolate* isolate);

 private:
  Zone zone_;
  ZoneStats zone_stats_;
  OptimizedCompilationInfo compilation_info_;
  std::unique_ptr<PipelineStatistics> pipeline_statistics_;
  PipelineData data_;
  PipelineImpl pipeline_;
  Linkage* linkage_;

  DISALLOW_COPY_AND_ASSIGN(PipelineCompilationJob);
};

PipelineCompilationJob::Status PipelineCompilationJob::PrepareJobImpl(
    Isolate* isolate) {
  if (compilation_info()->shared_info()->GetBytecodeArray()->length() >
      kMaxBytecodeSizeForTurbofan) {
    return AbortOptimization(BailoutReason::kFunctionTooBig);
  }

  if (!FLAG_always_opt) {
    compilation_info()->MarkAsBailoutOnUninitialized();
  }
  if (FLAG_turbo_loop_peeling) {
    compilation_info()->MarkAsLoopPeelingEnabled();
  }
  if (FLAG_turbo_inlining) {
    compilation_info()->MarkAsInliningEnabled();
  }
  if (FLAG_inline_accessors) {
    compilation_info()->MarkAsAccessorInliningEnabled();
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
    compilation_info()->MarkAsAllocationFoldingEnabled();
  }

  if (compilation_info()->closure()->feedback_cell()->map() ==
      ReadOnlyRoots(isolate).one_closure_cell_map()) {
    compilation_info()->MarkAsFunctionContextSpecializing();
  }

  data_.set_start_source_position(
      compilation_info()->shared_info()->StartPosition());

  linkage_ = new (compilation_info()->zone()) Linkage(
      Linkage::ComputeIncoming(compilation_info()->zone(), compilation_info()));

  if (!pipeline_.CreateGraph()) {
    if (isolate->has_pending_exception()) return FAILED;  // Stack overflowed.
    return AbortOptimization(BailoutReason::kGraphBuildingFailed);
  }

  if (compilation_info()->is_osr()) data_.InitializeOsrHelper();

  // Make sure that we have generated the maximal number of deopt entries.
  // This is in order to avoid triggering the generation of deopt entries later
  // during code assembly.
  Deoptimizer::EnsureCodeForMaxDeoptimizationEntries(isolate);

  return SUCCEEDED;
}

PipelineCompilationJob::Status PipelineCompilationJob::ExecuteJobImpl() {
  if (!pipeline_.OptimizeGraph(linkage_)) return FAILED;
  pipeline_.AssembleCode(linkage_);
  return SUCCEEDED;
}

PipelineCompilationJob::Status PipelineCompilationJob::FinalizeJobImpl(
    Isolate* isolate) {
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
  compilation_info()->context()->native_context()->AddOptimizedCode(*code);
  RegisterWeakObjectsInOptimizedCode(code, isolate);
  return SUCCEEDED;
}

void PipelineCompilationJob::RegisterWeakObjectsInOptimizedCode(
    Handle<Code> code, Isolate* isolate) {
  DCHECK(code->is_optimized_code());
  std::vector<Handle<Map>> maps;
  {
    DisallowHeapAllocation no_gc;
    int const mode_mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
    for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
      RelocInfo::Mode mode = it.rinfo()->rmode();
      if (mode == RelocInfo::EMBEDDED_OBJECT &&
          code->IsWeakObjectInOptimizedCode(it.rinfo()->target_object())) {
        Handle<HeapObject> object(HeapObject::cast(it.rinfo()->target_object()),
                                  isolate);
        if (object->IsMap()) {
          maps.push_back(Handle<Map>::cast(object));
        }
      }
    }
  }
  for (Handle<Map> map : maps) {
    isolate->heap()->AddRetainedMap(map);
  }
  code->set_can_have_weak_objects(true);
}

// The stack limit used during compilation is used to limit the recursion
// depth in, e.g. AST walking. No such recursion happens in WASM compilations.
constexpr uintptr_t kNoStackLimit = 0;

class PipelineWasmCompilationJob final : public OptimizedCompilationJob {
 public:
  explicit PipelineWasmCompilationJob(
      OptimizedCompilationInfo* info, wasm::WasmEngine* wasm_engine,
      MachineGraph* mcgraph, CallDescriptor* call_descriptor,
      SourcePositionTable* source_positions, NodeOriginTable* node_origins,
      wasm::FunctionBody function_body, wasm::WasmModule* wasm_module,
      wasm::NativeModule* native_module, int function_index, bool asmjs_origin)
      : OptimizedCompilationJob(kNoStackLimit, info, "TurboFan",
                                State::kReadyToExecute),
        zone_stats_(wasm_engine->allocator()),
        pipeline_statistics_(CreatePipelineStatistics(
            wasm_engine, function_body, wasm_module, info, &zone_stats_)),
        data_(&zone_stats_, wasm_engine, info, mcgraph,
              pipeline_statistics_.get(), source_positions, node_origins,
              function_index, WasmAssemblerOptions()),
        pipeline_(&data_),
        linkage_(call_descriptor),
        native_module_(native_module),
        asmjs_origin_(asmjs_origin) {}

 protected:
  Status PrepareJobImpl(Isolate* isolate) final;
  Status ExecuteJobImpl() final;
  Status FinalizeJobImpl(Isolate* isolate) final;

 private:
  ZoneStats zone_stats_;
  std::unique_ptr<PipelineStatistics> pipeline_statistics_;
  PipelineData data_;
  PipelineImpl pipeline_;
  Linkage linkage_;
  wasm::NativeModule* native_module_;
  bool asmjs_origin_;
};

PipelineWasmCompilationJob::Status PipelineWasmCompilationJob::PrepareJobImpl(
    Isolate* isolate) {
  UNREACHABLE();  // Prepare should always be skipped for WasmCompilationJob.
}

PipelineWasmCompilationJob::Status
PipelineWasmCompilationJob::ExecuteJobImpl() {
  pipeline_.RunPrintAndVerify("Machine", true);

  PipelineData* data = &data_;
  data->BeginPhaseKind("wasm optimization");
  if (FLAG_wasm_opt || asmjs_origin_) {
    PipelineRunScope scope(data, "wasm full optimization");
    GraphReducer graph_reducer(scope.zone(), data->graph(),
                               data->mcgraph()->Dead());
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), scope.zone());
    ValueNumberingReducer value_numbering(scope.zone(), data->graph()->zone());
    MachineOperatorReducer machine_reducer(data->mcgraph(), asmjs_origin_);
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->js_heap_broker(), data->common(),
                                         data->machine(), scope.zone());
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &common_reducer);
    AddReducer(data, &graph_reducer, &value_numbering);
    graph_reducer.ReduceGraph();
  } else {
    PipelineRunScope scope(data, "wasm base optimization");
    GraphReducer graph_reducer(scope.zone(), data->graph(),
                               data->mcgraph()->Dead());
    ValueNumberingReducer value_numbering(scope.zone(), data->graph()->zone());
    AddReducer(data, &graph_reducer, &value_numbering);
    graph_reducer.ReduceGraph();
  }
  pipeline_.RunPrintAndVerify("wasm optimization", true);

  if (data_.node_origins()) {
    data_.node_origins()->RemoveDecorator();
  }

  pipeline_.ComputeScheduledGraph();
  if (!pipeline_.SelectInstructions(&linkage_)) return FAILED;
  pipeline_.AssembleCode(&linkage_);

  CodeGenerator* code_generator = pipeline_.code_generator();
  CodeDesc code_desc;
  code_generator->tasm()->GetCode(nullptr, &code_desc);

  wasm::WasmCode* code = native_module_->AddCode(
      data_.wasm_function_index(), code_desc,
      code_generator->frame()->GetTotalFrameSlotCount(),
      code_generator->GetSafepointTableOffset(),
      code_generator->GetHandlerTableOffset(),
      code_generator->GetProtectedInstructions(),
      code_generator->GetSourcePositionTable(), wasm::WasmCode::kTurbofan);

  if (data_.info()->trace_turbo_json_enabled()) {
    TurboJsonFile json_of(data_.info(), std::ios_base::app);
    json_of << "{\"name\":\"disassembly\",\"type\":\"disassembly\",\"data\":\"";
#ifdef ENABLE_DISASSEMBLER
    std::stringstream disassembler_stream;
    Disassembler::Decode(
        nullptr, &disassembler_stream, code->instructions().start(),
        code->instructions().start() + code->safepoint_table_offset(),
        CodeReference(code));
    for (auto const c : disassembler_stream.str()) {
      json_of << AsEscapedUC16ForJSON(c);
    }
#endif  // ENABLE_DISASSEMBLER
    json_of << "\"}\n]";
    json_of << "\n}";
  }

  compilation_info()->SetCode(code);

  return SUCCEEDED;
}

PipelineWasmCompilationJob::Status PipelineWasmCompilationJob::FinalizeJobImpl(
    Isolate* isolate) {
  UNREACHABLE();  // Finalize should always be skipped for WasmCompilationJob.
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

struct GraphBuilderPhase {
  static const char* phase_name() { return "bytecode graph builder"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    JSTypeHintLowering::Flags flags = JSTypeHintLowering::kNoFlags;
    if (data->info()->is_bailout_on_uninitialized()) {
      flags |= JSTypeHintLowering::kBailoutOnUninitialized;
    }
    CallFrequency frequency = CallFrequency(1.0f);
    BytecodeGraphBuilder graph_builder(
        temp_zone, data->info()->shared_info(),
        handle(data->info()->closure()->feedback_vector(), data->isolate()),
        data->info()->osr_offset(), data->jsgraph(), frequency,
        data->source_positions(), data->native_context(),
        SourcePosition::kNotInlined, flags, true,
        data->info()->is_analyze_environment_liveness());
    graph_builder.CreateGraph();
  }
};

namespace {

Maybe<OuterContext> GetModuleContext(Handle<JSFunction> closure) {
  Context* current = closure->context();
  size_t distance = 0;
  while (!current->IsNativeContext()) {
    if (current->IsModuleContext()) {
      return Just(
          OuterContext(handle(current, current->GetIsolate()), distance));
    }
    current = current->previous();
    distance++;
  }
  return Nothing<OuterContext>();
}

Maybe<OuterContext> ChooseSpecializationContext(
    Isolate* isolate, OptimizedCompilationInfo* info) {
  if (info->is_function_context_specializing()) {
    DCHECK(info->has_context());
    return Just(OuterContext(handle(info->context(), isolate), 0));
  }
  return GetModuleContext(info->closure());
}

}  // anonymous namespace

struct InliningPhase {
  static const char* phase_name() { return "inlining"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    Isolate* isolate = data->isolate();
    OptimizedCompilationInfo* info = data->info();
    GraphReducer graph_reducer(temp_zone, data->graph(),
                               data->jsgraph()->Dead());
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    CheckpointElimination checkpoint_elimination(&graph_reducer);
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->js_heap_broker(), data->common(),
                                         data->machine(), temp_zone);
    JSCallReducer call_reducer(&graph_reducer, data->jsgraph(),
                               data->js_heap_broker(),
                               data->info()->is_bailout_on_uninitialized()
                                   ? JSCallReducer::kBailoutOnUninitialized
                                   : JSCallReducer::kNoFlags,
                               data->native_context(), data->dependencies());
    JSContextSpecialization context_specialization(
        &graph_reducer, data->jsgraph(), data->js_heap_broker(),
        ChooseSpecializationContext(isolate, data->info()),
        data->info()->is_function_context_specializing()
            ? data->info()->closure()
            : MaybeHandle<JSFunction>());
    JSNativeContextSpecialization::Flags flags =
        JSNativeContextSpecialization::kNoFlags;
    if (data->info()->is_accessor_inlining_enabled()) {
      flags |= JSNativeContextSpecialization::kAccessorInliningEnabled;
    }
    if (data->info()->is_bailout_on_uninitialized()) {
      flags |= JSNativeContextSpecialization::kBailoutOnUninitialized;
    }
    // Passing the OptimizedCompilationInfo's shared zone here as
    // JSNativeContextSpecialization allocates out-of-heap objects
    // that need to live until code generation.
    JSNativeContextSpecialization native_context_specialization(
        &graph_reducer, data->jsgraph(), data->js_heap_broker(), flags,
        data->native_context(), data->dependencies(), temp_zone, info->zone());
    JSInliningHeuristic inlining(
        &graph_reducer, data->info()->is_inlining_enabled()
                            ? JSInliningHeuristic::kGeneralInlining
                            : JSInliningHeuristic::kRestrictedInlining,
        temp_zone, data->info(), data->jsgraph(), data->source_positions());
    JSIntrinsicLowering intrinsic_lowering(&graph_reducer, data->jsgraph());
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &checkpoint_elimination);
    AddReducer(data, &graph_reducer, &common_reducer);
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

    // Make sure we always type True and False. Needed for escape analysis.
    roots.push_back(data->jsgraph()->TrueConstant());
    roots.push_back(data->jsgraph()->FalseConstant());

    LoopVariableOptimizer induction_vars(data->jsgraph()->graph(),
                                         data->common(), temp_zone);
    if (FLAG_turbo_loop_variable) induction_vars.Run();
    typer->Run(roots, &induction_vars);
  }
};

struct UntyperPhase {
  static const char* phase_name() { return "untyper"; }

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

    GraphReducer graph_reducer(temp_zone, data->graph(),
                               data->jsgraph()->Dead());
    RemoveTypeReducer remove_type_reducer;
    AddReducer(data, &graph_reducer, &remove_type_reducer);
    graph_reducer.ReduceGraph();
  }
};

struct SerializeStandardObjectsPhase {
  static const char* phase_name() { return "serialize standard objects"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->js_heap_broker()->SerializeStandardObjects();
  }
};

struct CopyMetadataForConcurrentCompilePhase {
  static const char* phase_name() { return "serialize metadata"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(temp_zone, data->graph(),
                               data->jsgraph()->Dead());
    JSHeapCopyReducer heap_copy_reducer(data->js_heap_broker());
    AddReducer(data, &graph_reducer, &heap_copy_reducer);
    graph_reducer.ReduceGraph();

    // Some nodes that are no longer in the graph might still be in the cache.
    NodeVector cached_nodes(temp_zone);
    data->jsgraph()->GetCachedNodes(&cached_nodes);
    for (Node* const node : cached_nodes) graph_reducer.ReduceNode(node);
  }
};

struct TypedLoweringPhase {
  static const char* phase_name() { return "typed lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(temp_zone, data->graph(),
                               data->jsgraph()->Dead());
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    JSCreateLowering create_lowering(&graph_reducer, data->dependencies(),
                                     data->jsgraph(), data->js_heap_broker(),
                                     temp_zone);
    JSTypedLowering typed_lowering(&graph_reducer, data->jsgraph(),
                                   data->js_heap_broker(), temp_zone);
    ConstantFoldingReducer constant_folding_reducer(
        &graph_reducer, data->jsgraph(), data->js_heap_broker());
    TypedOptimization typed_optimization(&graph_reducer, data->dependencies(),
                                         data->jsgraph(),
                                         data->js_heap_broker());
    SimplifiedOperatorReducer simple_reducer(&graph_reducer, data->jsgraph(),
                                             data->js_heap_broker());
    CheckpointElimination checkpoint_elimination(&graph_reducer);
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->js_heap_broker(), data->common(),
                                         data->machine(), temp_zone);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &create_lowering);
    AddReducer(data, &graph_reducer, &constant_folding_reducer);
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
    EscapeAnalysis escape_analysis(data->jsgraph(), temp_zone);
    escape_analysis.ReduceGraph();
    GraphReducer reducer(temp_zone, data->graph(), data->jsgraph()->Dead());
    EscapeAnalysisReducer escape_reducer(&reducer, data->jsgraph(),
                                         escape_analysis.analysis_result(),
                                         temp_zone);
    AddReducer(data, &reducer, &escape_reducer);
    reducer.ReduceGraph();
    // TODO(tebbi): Turn this into a debug mode check once we have confidence.
    escape_reducer.VerifyReplacement();
  }
};

struct SimplifiedLoweringPhase {
  static const char* phase_name() { return "simplified lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    SimplifiedLowering lowering(data->jsgraph(), data->js_heap_broker(),
                                temp_zone, data->source_positions(),
                                data->node_origins(),
                                data->info()->GetPoisoningMitigationLevel());
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
    LoopPeeler(data->graph(), data->common(), loop_tree, temp_zone,
               data->source_positions(), data->node_origins())
        .PeelInnerLoopsOfTree();
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
    GraphReducer graph_reducer(temp_zone, data->graph(),
                               data->jsgraph()->Dead());
    JSGenericLowering generic_lowering(data->jsgraph());
    AddReducer(data, &graph_reducer, &generic_lowering);
    graph_reducer.ReduceGraph();
  }
};

struct EarlyOptimizationPhase {
  static const char* phase_name() { return "early optimization"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(temp_zone, data->graph(),
                               data->jsgraph()->Dead());
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    SimplifiedOperatorReducer simple_reducer(&graph_reducer, data->jsgraph(),
                                             data->js_heap_broker());
    RedundancyElimination redundancy_elimination(&graph_reducer, temp_zone);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    MachineOperatorReducer machine_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->js_heap_broker(), data->common(),
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
    {
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
                                                      Scheduler::kTempSchedule);
      if (FLAG_turbo_verify) ScheduleVerifier::Run(schedule);
      TraceSchedule(data->info(), data, schedule,
                    "effect linearization schedule");

      EffectControlLinearizer::MaskArrayIndexEnable mask_array_index =
          (data->info()->GetPoisoningMitigationLevel() !=
           PoisoningMitigationLevel::kDontPoison)
              ? EffectControlLinearizer::kMaskArrayIndex
              : EffectControlLinearizer::kDoNotMaskArrayIndex;
      // Post-pass for wiring the control/effects
      // - connect allocating representation changes into the control&effect
      //   chains and lower them,
      // - get rid of the region markers,
      // - introduce effect phis and rewire effects to get SSA again.
      EffectControlLinearizer linearizer(
          data->jsgraph(), schedule, temp_zone, data->source_positions(),
          data->node_origins(), mask_array_index);
      linearizer.Run();
    }
    {
      // The {EffectControlLinearizer} might leave {Dead} nodes behind, so we
      // run {DeadCodeElimination} to prune these parts of the graph.
      // Also, the following store-store elimination phase greatly benefits from
      // doing a common operator reducer and dead code elimination just before
      // it, to eliminate conditional deopts with a constant condition.
      GraphReducer graph_reducer(temp_zone, data->graph(),
                                 data->jsgraph()->Dead());
      DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                                data->common(), temp_zone);
      CommonOperatorReducer common_reducer(
          &graph_reducer, data->graph(), data->js_heap_broker(), data->common(),
          data->machine(), temp_zone);
      AddReducer(data, &graph_reducer, &dead_code_elimination);
      AddReducer(data, &graph_reducer, &common_reducer);
      graph_reducer.ReduceGraph();
    }
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
    GraphReducer graph_reducer(temp_zone, data->graph(),
                               data->jsgraph()->Dead());
    BranchElimination branch_condition_elimination(&graph_reducer,
                                                   data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    RedundancyElimination redundancy_elimination(&graph_reducer, temp_zone);
    LoadElimination load_elimination(&graph_reducer, data->jsgraph(),
                                     temp_zone);
    CheckpointElimination checkpoint_elimination(&graph_reducer);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->js_heap_broker(), data->common(),
                                         data->machine(), temp_zone);
    ConstantFoldingReducer constant_folding_reducer(
        &graph_reducer, data->jsgraph(), data->js_heap_broker());
    TypeNarrowingReducer type_narrowing_reducer(&graph_reducer, data->jsgraph(),
                                                data->js_heap_broker());
    AddReducer(data, &graph_reducer, &branch_condition_elimination);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &redundancy_elimination);
    AddReducer(data, &graph_reducer, &load_elimination);
    AddReducer(data, &graph_reducer, &type_narrowing_reducer);
    AddReducer(data, &graph_reducer, &constant_folding_reducer);
    AddReducer(data, &graph_reducer, &checkpoint_elimination);
    AddReducer(data, &graph_reducer, &common_reducer);
    AddReducer(data, &graph_reducer, &value_numbering);
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
    MemoryOptimizer optimizer(
        data->jsgraph(), temp_zone, data->info()->GetPoisoningMitigationLevel(),
        data->info()->is_allocation_folding_enabled()
            ? MemoryOptimizer::AllocationFolding::kDoAllocationFolding
            : MemoryOptimizer::AllocationFolding::kDontAllocationFolding);
    optimizer.Optimize();
  }
};

struct LateOptimizationPhase {
  static const char* phase_name() { return "late optimization"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    GraphReducer graph_reducer(temp_zone, data->graph(),
                               data->jsgraph()->Dead());
    BranchElimination branch_condition_elimination(&graph_reducer,
                                                   data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    MachineOperatorReducer machine_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->js_heap_broker(), data->common(),
                                         data->machine(), temp_zone);
    SelectLowering select_lowering(data->jsgraph()->graph(),
                                   data->jsgraph()->common());
    AddReducer(data, &graph_reducer, &branch_condition_elimination);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &common_reducer);
    AddReducer(data, &graph_reducer, &select_lowering);
    AddReducer(data, &graph_reducer, &value_numbering);
    graph_reducer.ReduceGraph();
  }
};

struct EarlyGraphTrimmingPhase {
  static const char* phase_name() { return "early trimming"; }
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
    if (data->jsgraph()) {
      data->jsgraph()->GetCachedNodes(&roots);
    }
    trimmer.TrimGraph(roots.begin(), roots.end());
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
  static const char* phase_name() { return "select instructions"; }

  void Run(PipelineData* data, Zone* temp_zone, Linkage* linkage) {
    InstructionSelector selector(
        temp_zone, data->graph()->NodeCount(), linkage, data->sequence(),
        data->schedule(), data->source_positions(), data->frame(),
        data->info()->switch_jump_table_enabled()
            ? InstructionSelector::kEnableSwitchJumpTable
            : InstructionSelector::kDisableSwitchJumpTable,
        data->info()->is_source_positions_enabled()
            ? InstructionSelector::kAllSourcePositions
            : InstructionSelector::kCallSourcePositions,
        InstructionSelector::SupportedFeatures(),
        FLAG_turbo_instruction_scheduling
            ? InstructionSelector::kEnableScheduling
            : InstructionSelector::kDisableScheduling,
        !data->isolate() || data->isolate()->serializer_enabled()
            ? InstructionSelector::kDisableRootsRelativeAddressing
            : InstructionSelector::kEnableRootsRelativeAddressing,
        data->info()->GetPoisoningMitigationLevel(),
        data->info()->trace_turbo_json_enabled()
            ? InstructionSelector::kEnableTraceTurboJson
            : InstructionSelector::kDisableTraceTurboJson);
    if (!selector.SelectInstructions()) {
      data->set_compilation_failed();
    }
    if (data->info()->trace_turbo_json_enabled()) {
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
  static const char* phase_name() { return "allocate f.p. registers"; }

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
      JumpThreading::ApplyForwarding(temp_zone, result, data->sequence());
    }
  }
};

struct AssembleCodePhase {
  static const char* phase_name() { return "assemble code"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->code_generator()->AssembleCode();
  }
};

struct FinalizeCodePhase {
  static const char* phase_name() { return "finalize code"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    data->set_code(data->code_generator()->FinalizeCode());
  }
};


struct PrintGraphPhase {
  static const char* phase_name() { return nullptr; }

  void Run(PipelineData* data, Zone* temp_zone, const char* phase) {
    OptimizedCompilationInfo* info = data->info();
    Graph* graph = data->graph();

    if (info->trace_turbo_json_enabled()) {  // Print JSON.
      AllowHandleDereference allow_deref;

      TurboJsonFile json_of(info, std::ios_base::app);
      json_of << "{\"name\":\"" << phase << "\",\"type\":\"graph\",\"data\":"
              << AsJSON(*graph, data->source_positions(), data->node_origins())
              << "},\n";
    }

    if (info->trace_turbo_scheduled_enabled()) {
      AccountingAllocator allocator;
      Schedule* schedule = data->schedule();
      if (schedule == nullptr) {
        schedule = Scheduler::ComputeSchedule(temp_zone, data->graph(),
                                              Scheduler::kNoFlags);
      }

      AllowHandleDereference allow_deref;
      CodeTracer::Scope tracing_scope(data->GetCodeTracer());
      OFStream os(tracing_scope.file());
      os << "-- Graph after " << phase << " -- " << std::endl;
      os << AsScheduledGraph(schedule);
    } else if (info->trace_turbo_graph_enabled()) {  // Simple textual RPO.
      AllowHandleDereference allow_deref;
      CodeTracer::Scope tracing_scope(data->GetCodeTracer());
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
    Verifier::CodeType code_type;
    switch (data->info()->code_kind()) {
      case Code::WASM_FUNCTION:
      case Code::WASM_TO_JS_FUNCTION:
      case Code::JS_TO_WASM_FUNCTION:
      case Code::WASM_INTERPRETER_ENTRY:
      case Code::C_WASM_ENTRY:
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

void PipelineImpl::RunPrintAndVerify(const char* phase, bool untyped) {
  if (info()->trace_turbo_json_enabled() ||
      info()->trace_turbo_graph_enabled()) {
    Run<PrintGraphPhase>(phase);
  }
  if (FLAG_turbo_verify) {
    Run<VerifyGraphPhase>(untyped);
  }
}

bool PipelineImpl::CreateGraph() {
  PipelineData* data = this->data_;

  data->BeginPhaseKind("graph creation");

  if (info()->trace_turbo_json_enabled() ||
      info()->trace_turbo_graph_enabled()) {
    CodeTracer::Scope tracing_scope(data->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "---------------------------------------------------\n"
       << "Begin compiling method " << info()->GetDebugName().get()
       << " using Turbofan" << std::endl;
  }
  if (info()->trace_turbo_json_enabled()) {
    TurboCfgFile tcf(isolate());
    tcf << AsC1VCompilation(info());
  }

  data->source_positions()->AddDecorator();
  if (data->info()->trace_turbo_json_enabled()) {
    data->node_origins()->AddDecorator();
  }

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
    if (is_sloppy(info()->shared_info()->language_mode()) &&
        info()->shared_info()->IsUserJavaScript()) {
      // Sloppy mode functions always have an Object for this.
      data->AddTyperFlag(Typer::kThisIsReceiver);
    }
    if (IsClassConstructor(info()->shared_info()->kind())) {
      // Class constructors cannot be [[Call]]ed.
      data->AddTyperFlag(Typer::kNewTargetIsReceiver);
    }
  }

  // Run the type-sensitive lowerings and optimizations on the graph.
  {
    if (FLAG_concurrent_compiler_frontend) {
      data->js_heap_broker()->StartSerializing();
      Run<SerializeStandardObjectsPhase>();
      Run<CopyMetadataForConcurrentCompilePhase>();
      data->js_heap_broker()->StopSerializing();
    } else {
      data->js_heap_broker()->SetNativeContextRef();
      // Type the graph and keep the Typer running such that new nodes get
      // automatically typed when they are created.
      Run<TyperPhase>(data->CreateTyper());
      RunPrintAndVerify(TyperPhase::phase_name());
      Run<TypedLoweringPhase>();
      RunPrintAndVerify(TypedLoweringPhase::phase_name());
      data->DeleteTyper();
    }
  }

  data->EndPhaseKind();

  return true;
}

bool PipelineImpl::OptimizeGraph(Linkage* linkage) {
  PipelineData* data = this->data_;

  data->BeginPhaseKind("lowering");

  if (FLAG_concurrent_compiler_frontend) {
    // Type the graph and keep the Typer running such that new nodes get
    // automatically typed when they are created.
    Run<TyperPhase>(data->CreateTyper());
    RunPrintAndVerify(TyperPhase::phase_name());
    Run<TypedLoweringPhase>();
    RunPrintAndVerify(TypedLoweringPhase::phase_name());
    data->DeleteTyper();
  }

  if (data->info()->is_loop_peeling_enabled()) {
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

  // Perform simplified lowering. This has to run w/o the Typer decorator,
  // because we cannot compute meaningful types anyways, and the computed types
  // might even conflict with the representation/truncation logic.
  Run<SimplifiedLoweringPhase>();
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

  data->BeginPhaseKind("block building");

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

  // Optimize memory access and allocation operations.
  Run<MemoryOptimizationPhase>();
  // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
  RunPrintAndVerify(MemoryOptimizationPhase::phase_name(), true);

  // Lower changes that have been inserted before.
  Run<LateOptimizationPhase>();
  // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
  RunPrintAndVerify(LateOptimizationPhase::phase_name(), true);

  data->source_positions()->RemoveDecorator();
  if (data->info()->trace_turbo_json_enabled()) {
    data->node_origins()->RemoveDecorator();
  }

  ComputeScheduledGraph();

  return SelectInstructions(linkage);
}

MaybeHandle<Code> Pipeline::GenerateCodeForCodeStub(
    Isolate* isolate, CallDescriptor* call_descriptor, Graph* graph,
    Schedule* schedule, Code::Kind kind, const char* debug_name,
    uint32_t stub_key, int32_t builtin_index, JumpOptimizationInfo* jump_opt,
    PoisoningMitigationLevel poisoning_level, const AssemblerOptions& options) {
  OptimizedCompilationInfo info(CStrVector(debug_name), graph->zone(), kind);
  info.set_builtin_index(builtin_index);
  info.set_stub_key(stub_key);

  if (poisoning_level != PoisoningMitigationLevel::kDontPoison) {
    info.SetPoisoningMitigationLevel(poisoning_level);
  }

  // Construct a pipeline for scheduling and code generation.
  ZoneStats zone_stats(isolate->allocator());
  NodeOriginTable node_origins(graph);
  PipelineData data(&zone_stats, &info, isolate, graph, schedule, nullptr,
                    &node_origins, jump_opt, options);
  data.set_verify_graph(FLAG_verify_csa);
  std::unique_ptr<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics.reset(new PipelineStatistics(
        &info, isolate->GetTurboStatistics(), &zone_stats));
    pipeline_statistics->BeginPhaseKind("stub codegen");
  }

  PipelineImpl pipeline(&data);
  DCHECK_NOT_NULL(data.schedule());

  if (info.trace_turbo_json_enabled() || info.trace_turbo_graph_enabled()) {
    CodeTracer::Scope tracing_scope(data.GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "---------------------------------------------------\n"
       << "Begin compiling " << debug_name << " using Turbofan" << std::endl;
    if (info.trace_turbo_json_enabled()) {
      TurboJsonFile json_of(&info, std::ios_base::trunc);
      json_of << "{\"function\" : ";
      JsonPrintFunctionSource(json_of, -1, info.GetDebugName(),
                              Handle<Script>(), isolate,
                              Handle<SharedFunctionInfo>());
      json_of << ",\n\"phases\":[";
    }
    pipeline.Run<PrintGraphPhase>("Machine");
  }

  TraceSchedule(data.info(), &data, data.schedule(), "schedule");

  pipeline.Run<VerifyGraphPhase>(false, true);
  return pipeline.GenerateCode(call_descriptor);
}

// static
MaybeHandle<Code> Pipeline::GenerateCodeForWasmStub(
    Isolate* isolate, CallDescriptor* call_descriptor, Graph* graph,
    Code::Kind kind, const char* debug_name, const AssemblerOptions& options,
    SourcePositionTable* source_positions) {
  OptimizedCompilationInfo info(CStrVector(debug_name), graph->zone(), kind);
  // Construct a pipeline for scheduling and code generation.
  ZoneStats zone_stats(isolate->allocator());
  NodeOriginTable* node_positions = new (graph->zone()) NodeOriginTable(graph);
  PipelineData data(&zone_stats, &info, isolate, graph, nullptr,
                    source_positions, node_positions, nullptr, options);
  std::unique_ptr<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics.reset(new PipelineStatistics(
        &info, isolate->GetTurboStatistics(), &zone_stats));
    pipeline_statistics->BeginPhaseKind("wasm stub codegen");
  }

  PipelineImpl pipeline(&data);

  if (info.trace_turbo_graph_enabled()) {  // Simple textual RPO.
    StdoutStream{} << "-- wasm stub " << Code::Kind2String(kind) << " graph -- "
                   << std::endl
                   << AsRPO(*graph);
  }

  if (info.trace_turbo_json_enabled()) {
    TurboJsonFile json_of(&info, std::ios_base::trunc);
    json_of << "{\"function\":\"" << info.GetDebugName().get()
            << "\", \"source\":\"\",\n\"phases\":[";
  }
  // TODO(rossberg): Should this really be untyped?
  pipeline.RunPrintAndVerify("machine", true);
  pipeline.ComputeScheduledGraph();

  Handle<Code> code;
  if (pipeline.GenerateCode(call_descriptor).ToHandle(&code) &&
      pipeline.CommitDependencies(code)) {
    return code;
  }
  return MaybeHandle<Code>();
}

// static
MaybeHandle<Code> Pipeline::GenerateCodeForTesting(
    OptimizedCompilationInfo* info, Isolate* isolate) {
  ZoneStats zone_stats(isolate->allocator());
  std::unique_ptr<PipelineStatistics> pipeline_statistics(
      CreatePipelineStatistics(Handle<Script>::null(), info, isolate,
                               &zone_stats));
  PipelineData data(&zone_stats, isolate, info, pipeline_statistics.get());
  PipelineImpl pipeline(&data);

  Linkage linkage(Linkage::ComputeIncoming(data.instruction_zone(), info));
  Deoptimizer::EnsureCodeForMaxDeoptimizationEntries(isolate);

  if (!pipeline.CreateGraph()) return MaybeHandle<Code>();
  if (!pipeline.OptimizeGraph(&linkage)) return MaybeHandle<Code>();
  pipeline.AssembleCode(&linkage);
  Handle<Code> code;
  if (pipeline.FinalizeCode().ToHandle(&code) &&
      pipeline.CommitDependencies(code)) {
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
  NodeOriginTable* node_positions = new (info->zone()) NodeOriginTable(graph);
  PipelineData data(&zone_stats, info, isolate, graph, schedule, nullptr,
                    node_positions, nullptr, options);
  std::unique_ptr<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics.reset(new PipelineStatistics(
        info, isolate->GetTurboStatistics(), &zone_stats));
    pipeline_statistics->BeginPhaseKind("test codegen");
  }

  PipelineImpl pipeline(&data);

  if (info->trace_turbo_json_enabled()) {
    TurboJsonFile json_of(info, std::ios_base::trunc);
    json_of << "{\"function\":\"" << info->GetDebugName().get()
            << "\", \"source\":\"\",\n\"phases\":[";
  }
  // TODO(rossberg): Should this really be untyped?
  pipeline.RunPrintAndVerify("machine", true);

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
OptimizedCompilationJob* Pipeline::NewCompilationJob(
    Isolate* isolate, Handle<JSFunction> function, bool has_script) {
  Handle<SharedFunctionInfo> shared =
      handle(function->shared(), function->GetIsolate());
  return new PipelineCompilationJob(isolate, shared, function);
}

// static
OptimizedCompilationJob* Pipeline::NewWasmCompilationJob(
    OptimizedCompilationInfo* info, wasm::WasmEngine* wasm_engine,
    MachineGraph* mcgraph, CallDescriptor* call_descriptor,
    SourcePositionTable* source_positions, NodeOriginTable* node_origins,
    wasm::FunctionBody function_body, wasm::WasmModule* wasm_module,
    wasm::NativeModule* native_module, int function_index,
    wasm::ModuleOrigin asmjs_origin) {
  return new PipelineWasmCompilationJob(
      info, wasm_engine, mcgraph, call_descriptor, source_positions,
      node_origins, function_body, wasm_module, native_module, function_index,
      asmjs_origin);
}

bool Pipeline::AllocateRegistersForTesting(const RegisterConfiguration* config,
                                           InstructionSequence* sequence,
                                           bool run_verifier) {
  OptimizedCompilationInfo info(ArrayVector("testing"), sequence->zone(),
                                Code::STUB);
  ZoneStats zone_stats(sequence->isolate()->allocator());
  PipelineData data(&zone_stats, &info, sequence->isolate(), sequence);
  data.InitializeFrameData(nullptr);
  PipelineImpl pipeline(&data);
  pipeline.AllocateRegisters(config, nullptr, run_verifier);
  return !data.compilation_failed();
}

void PipelineImpl::ComputeScheduledGraph() {
  PipelineData* data = this->data_;

  // We should only schedule the graph if it is not scheduled yet.
  DCHECK_NULL(data->schedule());

  Run<LateGraphTrimmingPhase>();
  RunPrintAndVerify(LateGraphTrimmingPhase::phase_name(), true);

  Run<ComputeSchedulePhase>();
  TraceSchedule(data->info(), data, data->schedule(), "schedule");
}

bool PipelineImpl::SelectInstructions(Linkage* linkage) {
  auto call_descriptor = linkage->GetIncomingDescriptor();
  PipelineData* data = this->data_;

  // We should have a scheduled graph.
  DCHECK_NOT_NULL(data->graph());
  DCHECK_NOT_NULL(data->schedule());

  if (FLAG_turbo_profiling) {
    data->set_profiler_data(BasicBlockInstrumentor::Instrument(
        info(), data->graph(), data->schedule(), data->isolate()));
  }

  bool verify_stub_graph = data->verify_graph();
  // Jump optimization runs instruction selection twice, but the instruction
  // selector mutates nodes like swapping the inputs of a load, which can
  // violate the machine graph verification rules. So we skip the second
  // verification on a graph that already verified before.
  auto jump_opt = data->jump_optimization_info();
  if (jump_opt && jump_opt->is_optimizing()) {
    verify_stub_graph = false;
  }
  if (verify_stub_graph ||
      (FLAG_turbo_verify_machine_graph != nullptr &&
       (!strcmp(FLAG_turbo_verify_machine_graph, "*") ||
        !strcmp(FLAG_turbo_verify_machine_graph, data->debug_name())))) {
    if (FLAG_trace_verify_csa) {
      AllowHandleDereference allow_deref;
      CodeTracer::Scope tracing_scope(data->GetCodeTracer());
      OFStream os(tracing_scope.file());
      os << "--------------------------------------------------\n"
         << "--- Verifying " << data->debug_name() << " generated by TurboFan\n"
         << "--------------------------------------------------\n"
         << *data->schedule()
         << "--------------------------------------------------\n"
         << "--- End of " << data->debug_name() << " generated by TurboFan\n"
         << "--------------------------------------------------\n";
    }
    Zone temp_zone(data->allocator(), ZONE_NAME);
    MachineGraphVerifier::Run(data->graph(), data->schedule(), linkage,
                              data->info()->IsStub(), data->debug_name(),
                              &temp_zone);
  }

  data->InitializeInstructionSequence(call_descriptor);

  data->InitializeFrameData(call_descriptor);
  // Select and schedule instructions covering the scheduled graph.
  Run<InstructionSelectionPhase>(linkage);
  if (data->compilation_failed()) {
    info()->AbortOptimization(BailoutReason::kCodeGenerationFailed);
    data->EndPhaseKind();
    return false;
  }

  if (info()->trace_turbo_json_enabled() && !data->MayHaveUnverifiableGraph()) {
    AllowHandleDereference allow_deref;
    TurboCfgFile tcf(isolate());
    tcf << AsC1V("CodeGen", data->schedule(), data->source_positions(),
                 data->sequence());
  }

  if (info()->trace_turbo_json_enabled()) {
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

  data->BeginPhaseKind("register allocation");

  bool run_verifier = FLAG_turbo_verify_allocation;

  // Allocate registers.
  if (call_descriptor->HasRestrictedAllocatableRegisters()) {
    RegList registers = call_descriptor->AllocatableRegisters();
    DCHECK_LT(0, NumRegs(registers));
    std::unique_ptr<const RegisterConfiguration> config;
    config.reset(RegisterConfiguration::RestrictGeneralRegisters(registers));
    AllocateRegisters(config.get(), call_descriptor, run_verifier);
  } else if (data->info()->GetPoisoningMitigationLevel() !=
             PoisoningMitigationLevel::kDontPoison) {
    AllocateRegisters(RegisterConfiguration::Poisoning(), call_descriptor,
                      run_verifier);
#if defined(V8_TARGET_ARCH_IA32) && defined(V8_EMBEDDED_BUILTINS)
  } else if (Builtins::IsBuiltinId(data->info()->builtin_index())) {
    // TODO(v8:6666): Extend support to user code. Ensure that
    // it is mutually exclusive with the Poisoning configuration above; and that
    // it cooperates with restricted allocatable registers above.
    static_assert(kRootRegister == kSpeculationPoisonRegister,
                  "The following checks assume root equals poison register");
    CHECK_IMPLIES(FLAG_embedded_builtins, !FLAG_untrusted_code_mitigations);
    AllocateRegisters(RegisterConfiguration::PreserveRootIA32(),
                      call_descriptor, run_verifier);
#endif  // defined(V8_TARGET_ARCH_IA32) && defined(V8_EMBEDDED_BUILTINS)
  } else {
    AllocateRegisters(RegisterConfiguration::Default(), call_descriptor,
                      run_verifier);
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
  const ZoneVector<int>* instr_starts;
};

std::ostream& operator<<(std::ostream& out, const InstructionStartsAsJSON& s) {
  out << ", \"instructionOffsetToPCOffset\": {";
  bool need_comma = false;
  for (size_t i = 0; i < s.instr_starts->size(); ++i) {
    if (need_comma) out << ", ";
    int offset = (*s.instr_starts)[i];
    out << "\"" << i << "\":" << offset;
    need_comma = true;
  }
  out << "}";
  return out;
}

void PipelineImpl::AssembleCode(Linkage* linkage) {
  PipelineData* data = this->data_;
  data->BeginPhaseKind("code generation");
  data->InitializeCodeGenerator(linkage);
  Run<AssembleCodePhase>();
  if (data->info()->trace_turbo_json_enabled()) {
    TurboJsonFile json_of(data->info(), std::ios_base::app);
    json_of << "{\"name\":\"code generation\""
            << ", \"type\":\"instructions\""
            << InstructionStartsAsJSON{&data->code_generator()->instr_starts()};
    json_of << "},\n";
  }
  data->DeleteInstructionZone();
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

MaybeHandle<Code> PipelineImpl::FinalizeCode() {
  PipelineData* data = this->data_;
  if (data->js_heap_broker() && FLAG_concurrent_compiler_frontend) {
    data->js_heap_broker()->Retire();
  }
  Run<FinalizeCodePhase>();

  MaybeHandle<Code> maybe_code = data->code();
  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    return maybe_code;
  }

  if (data->profiler_data()) {
#ifdef ENABLE_DISASSEMBLER
    std::ostringstream os;
    code->Disassemble(nullptr, os);
    data->profiler_data()->SetCode(&os);
#endif  // ENABLE_DISASSEMBLER
  }

  info()->SetCode(code);
  PrintCode(isolate(), code, info());

  if (info()->trace_turbo_json_enabled()) {
    TurboJsonFile json_of(info(), std::ios_base::app);

    json_of << "{\"name\":\"disassembly\",\"type\":\"disassembly\""
            << BlockStartsAsJSON{&data->code_generator()->block_starts()}
            << "\"data\":\"";
#ifdef ENABLE_DISASSEMBLER
    std::stringstream disassembly_stream;
    code->Disassemble(nullptr, disassembly_stream);
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
  if (info()->trace_turbo_json_enabled() ||
      info()->trace_turbo_graph_enabled()) {
    CodeTracer::Scope tracing_scope(data->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "---------------------------------------------------\n"
       << "Finished compiling method " << info()->GetDebugName().get()
       << " using Turbofan" << std::endl;
  }
  return code;
}

MaybeHandle<Code> PipelineImpl::GenerateCode(CallDescriptor* call_descriptor) {
  Linkage linkage(call_descriptor);

  // Perform instruction selection and register allocation.
  if (!SelectInstructions(&linkage)) return MaybeHandle<Code>();

  // Generate the final machine code.
  AssembleCode(&linkage);
  return FinalizeCode();
}

bool PipelineImpl::CommitDependencies(Handle<Code> code) {
  return data_->dependencies() == nullptr ||
         data_->dependencies()->Commit(code);
}

namespace {

void TraceSequence(OptimizedCompilationInfo* info, PipelineData* data,
                   const RegisterConfiguration* config,
                   const char* phase_name) {
  if (info->trace_turbo_json_enabled()) {
    AllowHandleDereference allow_deref;
    TurboJsonFile json_of(info, std::ios_base::app);
    json_of << "{\"name\":\"" << phase_name << "\",\"type\":\"sequence\",";
    json_of << InstructionSequenceAsJSON{config, data->sequence()};
    json_of << "},\n";
  }
  if (info->trace_turbo_graph_enabled()) {
    AllowHandleDereference allow_deref;
    CodeTracer::Scope tracing_scope(data->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "----- Instruction sequence " << phase_name << " -----\n"
       << PrintableInstructionSequence({config, data->sequence()});
  }
}

}  // namespace

void PipelineImpl::AllocateRegisters(const RegisterConfiguration* config,
                                     CallDescriptor* call_descriptor,
                                     bool run_verifier) {
  PipelineData* data = this->data_;
  // Don't track usage for this zone in compiler stats.
  std::unique_ptr<Zone> verifier_zone;
  RegisterAllocatorVerifier* verifier = nullptr;
  if (run_verifier) {
    verifier_zone.reset(new Zone(data->allocator(), ZONE_NAME));
    verifier = new (verifier_zone.get()) RegisterAllocatorVerifier(
        verifier_zone.get(), config, data->sequence());
  }

#ifdef DEBUG
  data_->sequence()->ValidateEdgeSplitForm();
  data_->sequence()->ValidateDeferredBlockEntryPaths();
  data_->sequence()->ValidateDeferredBlockExitPaths();
#endif

  data->InitializeRegisterAllocationData(config, call_descriptor);
  if (info()->is_osr()) data->osr_helper()->SetupFrame(data->frame());

  Run<MeetRegisterConstraintsPhase>();
  Run<ResolvePhisPhase>();
  Run<BuildLiveRangesPhase>();
  TraceSequence(info(), data, config, "before register allocation");
  if (verifier != nullptr) {
    CHECK(!data->register_allocation_data()->ExistsUseWithoutDefinition());
    CHECK(data->register_allocation_data()
              ->RangesDefinedInDeferredStayInDeferred());
  }

  if (FLAG_turbo_preprocess_ranges) {
    Run<SplinterLiveRangesPhase>();
  }

  Run<AllocateGeneralRegistersPhase<LinearScanAllocator>>();

  if (data->sequence()->HasFPVirtualRegisters()) {
    Run<AllocateFPRegistersPhase<LinearScanAllocator>>();
  }

  if (FLAG_turbo_preprocess_ranges) {
    Run<MergeSplintersPhase>();
  }

  Run<AssignSpillSlotsPhase>();

  Run<CommitAssignmentPhase>();

  // TODO(chromium:725559): remove this check once
  // we understand the cause of the bug. We keep just the
  // check at the end of the allocation.
  if (verifier != nullptr) {
    verifier->VerifyAssignment("Immediately after CommitAssignmentPhase.");
  }

  Run<PopulateReferenceMapsPhase>();
  Run<ConnectRangesPhase>();
  Run<ResolveControlFlowPhase>();
  if (FLAG_turbo_move_optimization) {
    Run<OptimizeMovesPhase>();
  }

  Run<LocateSpillSlotsPhase>();

  TraceSequence(info(), data, config, "after register allocation");

  if (verifier != nullptr) {
    verifier->VerifyAssignment("End of regalloc pipeline.");
    verifier->VerifyGapMoves();
  }

  if (info()->trace_turbo_json_enabled() && !data->MayHaveUnverifiableGraph()) {
    TurboCfgFile tcf(isolate());
    tcf << AsC1VRegisterAllocationData("CodeGen",
                                       data->register_allocation_data());
  }

  data->DeleteRegisterAllocationZone();
}

OptimizedCompilationInfo* PipelineImpl::info() const { return data_->info(); }

Isolate* PipelineImpl::isolate() const { return data_->isolate(); }

CodeGenerator* PipelineImpl::code_generator() const {
  return data_->code_generator();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

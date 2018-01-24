// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/pipeline.h"

#include <fstream>  // NOLINT(readability/streams)
#include <memory>
#include <sstream>

#include "src/assembler-inl.h"
#include "src/base/adapters.h"
#include "src/base/optional.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/bootstrapper.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/compiler/basic-block-instrumentor.h"
#include "src/compiler/branch-elimination.h"
#include "src/compiler/bytecode-graph-builder.h"
#include "src/compiler/checkpoint-elimination.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/common-operator-reducer.h"
#include "src/compiler/compiler-source-position-table.h"
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
#include "src/compiler/js-builtin-reducer.h"
#include "src/compiler/js-call-reducer.h"
#include "src/compiler/js-context-specialization.h"
#include "src/compiler/js-create-lowering.h"
#include "src/compiler/js-generic-lowering.h"
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
#include "src/compiler/typed-optimization.h"
#include "src/compiler/typer.h"
#include "src/compiler/value-numbering-reducer.h"
#include "src/compiler/verifier.h"
#include "src/compiler/zone-stats.h"
#include "src/isolate-inl.h"
#include "src/ostreams.h"
#include "src/parsing/parse-info.h"
#include "src/register-configuration.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

namespace trap_handler {
struct ProtectedInstructionData;
}  // namespace trap_handler

namespace compiler {

class PipelineData {
 public:
  // For main entry point.
  PipelineData(ZoneStats* zone_stats, Isolate* isolate, CompilationInfo* info,
               PipelineStatistics* pipeline_statistics)
      : isolate_(isolate),
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

  // For WebAssembly compile entry point.
  PipelineData(ZoneStats* zone_stats, Isolate* isolate, CompilationInfo* info,
               JSGraph* jsgraph, PipelineStatistics* pipeline_statistics,
               SourcePositionTable* source_positions,
               std::vector<trap_handler::ProtectedInstructionData>*
                   protected_instructions)
      : isolate_(isolate),
        info_(info),
        debug_name_(info_->GetDebugName()),
        zone_stats_(zone_stats),
        pipeline_statistics_(pipeline_statistics),
        graph_zone_scope_(zone_stats_, ZONE_NAME),
        graph_(jsgraph->graph()),
        source_positions_(source_positions),
        machine_(jsgraph->machine()),
        common_(jsgraph->common()),
        javascript_(jsgraph->javascript()),
        jsgraph_(jsgraph),
        instruction_zone_scope_(zone_stats_, ZONE_NAME),
        instruction_zone_(instruction_zone_scope_.zone()),
        codegen_zone_scope_(zone_stats_, ZONE_NAME),
        codegen_zone_(codegen_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_stats_, ZONE_NAME),
        register_allocation_zone_(register_allocation_zone_scope_.zone()),
        protected_instructions_(protected_instructions) {}

  // For machine graph testing entry point.
  PipelineData(ZoneStats* zone_stats, CompilationInfo* info, Isolate* isolate,
               Graph* graph, Schedule* schedule,
               SourcePositionTable* source_positions,
               JumpOptimizationInfo* jump_opt)
      : isolate_(isolate),
        info_(info),
        debug_name_(info_->GetDebugName()),
        zone_stats_(zone_stats),
        graph_zone_scope_(zone_stats_, ZONE_NAME),
        graph_(graph),
        source_positions_(source_positions),
        schedule_(schedule),
        instruction_zone_scope_(zone_stats_, ZONE_NAME),
        instruction_zone_(instruction_zone_scope_.zone()),
        codegen_zone_scope_(zone_stats_, ZONE_NAME),
        codegen_zone_(codegen_zone_scope_.zone()),
        register_allocation_zone_scope_(zone_stats_, ZONE_NAME),
        register_allocation_zone_(register_allocation_zone_scope_.zone()),
        jump_optimization_info_(jump_opt) {}

  // For register allocation testing entry point.
  PipelineData(ZoneStats* zone_stats, CompilationInfo* info, Isolate* isolate,
               InstructionSequence* sequence)
      : isolate_(isolate),
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
        register_allocation_zone_(register_allocation_zone_scope_.zone()) {}

  ~PipelineData() {
    delete code_generator_;  // Must happen before zones are destroyed.
    code_generator_ = nullptr;
    DeleteRegisterAllocationZone();
    DeleteInstructionZone();
    DeleteCodegenZone();
    DeleteGraphZone();
  }

  Isolate* isolate() const { return isolate_; }
  CompilationInfo* info() const { return info_; }
  ZoneStats* zone_stats() const { return zone_stats_; }
  PipelineStatistics* pipeline_statistics() { return pipeline_statistics_; }
  OsrHelper* osr_helper() { return &(*osr_helper_); }
  bool compilation_failed() const { return compilation_failed_; }
  void set_compilation_failed() { compilation_failed_ = true; }

  bool verify_graph() const { return verify_graph_; }
  void set_verify_graph(bool value) { verify_graph_ = value; }

  Handle<Code> code() { return code_; }
  void set_code(Handle<Code> code) {
    DCHECK(code_.is_null());
    code_ = code;
  }

  CodeGenerator* code_generator() const { return code_generator_; }

  // RawMachineAssembler generally produces graphs which cannot be verified.
  bool MayHaveUnverifiableGraph() const { return may_have_unverifiable_graph_; }

  Zone* graph_zone() const { return graph_zone_; }
  Graph* graph() const { return graph_; }
  SourcePositionTable* source_positions() const { return source_positions_; }
  MachineOperatorBuilder* machine() const { return machine_; }
  CommonOperatorBuilder* common() const { return common_; }
  JSOperatorBuilder* javascript() const { return javascript_; }
  JSGraph* jsgraph() const { return jsgraph_; }
  Handle<Context> native_context() const {
    return handle(info()->native_context(), isolate());
  }
  Handle<JSGlobalObject> global_object() const {
    return handle(info()->global_object(), isolate());
  }

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

  std::vector<trap_handler::ProtectedInstructionData>* protected_instructions()
      const {
    return protected_instructions_;
  }

  JumpOptimizationInfo* jump_optimization_info() const {
    return jump_optimization_info_;
  }

  void DeleteGraphZone() {
    if (graph_zone_ == nullptr) return;
    graph_zone_scope_.Destroy();
    graph_zone_ = nullptr;
    graph_ = nullptr;
    source_positions_ = nullptr;
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
  }

  void DeleteCodegenZone() {
    if (codegen_zone_ == nullptr) return;
    codegen_zone_scope_.Destroy();
    codegen_zone_ = nullptr;
    frame_ = nullptr;
  }

  void DeleteRegisterAllocationZone() {
    if (register_allocation_zone_ == nullptr) return;
    register_allocation_zone_scope_.Destroy();
    register_allocation_zone_ = nullptr;
    register_allocation_data_ = nullptr;
  }

  void InitializeInstructionSequence(const CallDescriptor* descriptor) {
    DCHECK_NULL(sequence_);
    InstructionBlocks* instruction_blocks =
        InstructionSequence::InstructionBlocksFor(instruction_zone(),
                                                  schedule());
    sequence_ = new (instruction_zone())
        InstructionSequence(isolate(), instruction_zone(), instruction_blocks);
    if (descriptor && descriptor->RequiresFrameAsIncoming()) {
      sequence_->instruction_blocks()[0]->mark_needs_frame();
    } else {
      DCHECK_EQ(0u, descriptor->CalleeSavedFPRegisters());
      DCHECK_EQ(0u, descriptor->CalleeSavedRegisters());
    }
  }

  void InitializeFrameData(CallDescriptor* descriptor) {
    DCHECK_NULL(frame_);
    int fixed_frame_size = 0;
    if (descriptor != nullptr) {
      fixed_frame_size = descriptor->CalculateFixedFrameSize();
    }
    frame_ = new (codegen_zone()) Frame(fixed_frame_size);
  }

  void InitializeRegisterAllocationData(const RegisterConfiguration* config,
                                        CallDescriptor* descriptor) {
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
    code_generator_ =
        new CodeGenerator(codegen_zone(), frame(), linkage, sequence(), info(),
                          isolate(), osr_helper_, start_source_position_,
                          jump_optimization_info_, protected_instructions_);
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

 private:
  Isolate* const isolate_;
  CompilationInfo* const info_;
  std::unique_ptr<char[]> debug_name_;
  bool may_have_unverifiable_graph_ = true;
  ZoneStats* const zone_stats_;
  PipelineStatistics* pipeline_statistics_ = nullptr;
  bool compilation_failed_ = false;
  bool verify_graph_ = false;
  int start_source_position_ = kNoSourcePosition;
  base::Optional<OsrHelper> osr_helper_;
  Handle<Code> code_ = Handle<Code>::null();
  CodeGenerator* code_generator_ = nullptr;

  // All objects in the following group of fields are allocated in graph_zone_.
  // They are all set to nullptr when the graph_zone_ is destroyed.
  ZoneStats::Scope graph_zone_scope_;
  Zone* graph_zone_ = nullptr;
  Graph* graph_ = nullptr;
  SourcePositionTable* source_positions_ = nullptr;
  SimplifiedOperatorBuilder* simplified_ = nullptr;
  MachineOperatorBuilder* machine_ = nullptr;
  CommonOperatorBuilder* common_ = nullptr;
  JSOperatorBuilder* javascript_ = nullptr;
  JSGraph* jsgraph_ = nullptr;
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

  std::vector<trap_handler::ProtectedInstructionData>* protected_instructions_ =
      nullptr;

  JumpOptimizationInfo* jump_optimization_info_ = nullptr;

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

  // Run the code assembly pass.
  void AssembleCode(Linkage* linkage);

  // Run the code finalization pass.
  Handle<Code> FinalizeCode();

  bool ScheduleAndSelectInstructions(Linkage* linkage, bool trim_graph);
  void RunPrintAndVerify(const char* phase, bool untyped = false);
  Handle<Code> ScheduleAndGenerateCode(CallDescriptor* call_descriptor);
  void AllocateRegisters(const RegisterConfiguration* config,
                         CallDescriptor* descriptor, bool run_verifier);

  CompilationInfo* info() const;
  Isolate* isolate() const;

  PipelineData* const data_;
};

namespace {

// Print function's source if it was not printed before.
// Return a sequential id under which this function was printed.
int PrintFunctionSource(CompilationInfo* info, Isolate* isolate,
                        std::vector<Handle<SharedFunctionInfo>>* printed,
                        int inlining_id, Handle<SharedFunctionInfo> shared) {
  // Outermost function has source id -1 and inlined functions take
  // source ids starting from 0.
  int source_id = -1;
  if (inlining_id != SourcePosition::kNotInlined) {
    for (unsigned i = 0; i < printed->size(); i++) {
      if (printed->at(i).is_identical_to(shared)) {
        return i;
      }
    }
    source_id = static_cast<int>(printed->size());
    printed->push_back(shared);
  }

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
      os << shared->start_position() << "} ---\n";
      {
        DisallowHeapAllocation no_allocation;
        int start = shared->start_position();
        int len = shared->end_position() - start;
        String::SubStringRange source(String::cast(script->source()), start,
                                      len);
        for (const auto& c : source) {
          os << AsReversiblyEscapedUC16(c);
        }
      }

      os << "\n--- END ---\n";
    }
  }

  return source_id;
}

// Print information for the given inlining: which function was inlined and
// where the inlining occurred.
void PrintInlinedFunctionInfo(CompilationInfo* info, Isolate* isolate,
                              int source_id, int inlining_id,
                              const CompilationInfo::InlinedFunctionHolder& h) {
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
void DumpParticipatingSource(CompilationInfo* info, Isolate* isolate) {
  AllowDeferredHandleDereference allow_deference_for_print_code;

  std::vector<Handle<SharedFunctionInfo>> printed;
  printed.reserve(info->inlined_functions().size());

  PrintFunctionSource(info, isolate, &printed, SourcePosition::kNotInlined,
                      info->shared_info());
  const auto& inlined = info->inlined_functions();
  for (unsigned id = 0; id < inlined.size(); id++) {
    const int source_id = PrintFunctionSource(info, isolate, &printed, id,
                                              inlined[id].shared_info);
    PrintInlinedFunctionInfo(info, isolate, source_id, id, inlined[id]);
  }
}

// Print the code after compiling it.
void PrintCode(Handle<Code> code, CompilationInfo* info) {
  Isolate* isolate = code->GetIsolate();
  if (FLAG_print_opt_source && info->IsOptimizing()) {
    DumpParticipatingSource(info, isolate);
  }

#ifdef ENABLE_DISASSEMBLER
  AllowDeferredHandleDereference allow_deference_for_print_code;
  bool print_code =
      isolate->bootstrapper()->IsActive()
          ? FLAG_print_builtin_code
          : (FLAG_print_code || (info->IsStub() && FLAG_print_code_stubs) ||
             (info->IsOptimizing() && FLAG_print_opt_code &&
              info->shared_info()->PassesFilter(FLAG_print_opt_code_filter)) ||
             (info->IsWasm() && FLAG_print_wasm_code));
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
            shared->start_position());
        // fun->end_position() points to the last character in the stream. We
        // need to compensate by adding one to calculate the length.
        int source_len = shared->end_position() - shared->start_position() + 1;
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
      os << "source_position = " << shared->start_position() << "\n";
    }
    code->Disassemble(debug_name.get(), os);
    os << "--- End code ---\n";
  }
#endif  // ENABLE_DISASSEMBLER
}

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

void TraceSchedule(CompilationInfo* info, Isolate* isolate,
                   Schedule* schedule) {
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
    CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "-- Schedule --------------------------------------\n" << *schedule;
  }
}


class SourcePositionWrapper final : public Reducer {
 public:
  SourcePositionWrapper(Reducer* reducer, SourcePositionTable* table)
      : reducer_(reducer), table_(table) {}
  ~SourcePositionWrapper() final {}

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
        zone_scope_(data->zone_stats(), ZONE_NAME) {}

  Zone* zone() { return zone_scope_.zone(); }

 private:
  PhaseScope phase_scope_;
  ZoneStats::Scope zone_scope_;
};

PipelineStatistics* CreatePipelineStatistics(Handle<Script> script,
                                             CompilationInfo* info,
                                             Isolate* isolate,
                                             ZoneStats* zone_stats) {
  PipelineStatistics* pipeline_statistics = nullptr;

  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics = new PipelineStatistics(info, isolate, zone_stats);
    pipeline_statistics->BeginPhaseKind("initializing");
  }

  if (FLAG_trace_turbo) {
    TurboJsonFile json_of(info, std::ios_base::trunc);
    std::unique_ptr<char[]> function_name = info->GetDebugName();
    int pos = info->IsStub() ? 0 : info->shared_info()->start_position();
    json_of << "{\"function\":\"" << function_name.get()
            << "\", \"sourcePosition\":" << pos << ", \"source\":\"";
    if (!script.is_null() && !script->source()->IsUndefined(isolate)) {
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
  PipelineCompilationJob(ParseInfo* parse_info,
                         Handle<SharedFunctionInfo> shared_info,
                         Handle<JSFunction> function)
      // Note that the CompilationInfo is not initialized at the time we pass it
      // to the CompilationJob constructor, but it is not dereferenced there.
      : CompilationJob(parse_info->stack_limit(), parse_info,
                       &compilation_info_, "TurboFan"),
        parse_info_(parse_info),
        zone_stats_(function->GetIsolate()->allocator()),
        compilation_info_(parse_info_.get()->zone(), function->GetIsolate(),
                          shared_info, function),
        pipeline_statistics_(
            CreatePipelineStatistics(parse_info_->script(), compilation_info(),
                                     function->GetIsolate(), &zone_stats_)),
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
  std::unique_ptr<ParseInfo> parse_info_;
  ZoneStats zone_stats_;
  CompilationInfo compilation_info_;
  std::unique_ptr<PipelineStatistics> pipeline_statistics_;
  PipelineData data_;
  PipelineImpl pipeline_;
  Linkage* linkage_;

  DISALLOW_COPY_AND_ASSIGN(PipelineCompilationJob);
};

PipelineCompilationJob::Status PipelineCompilationJob::PrepareJobImpl(
    Isolate* isolate) {
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
  if (compilation_info()->closure()->feedback_vector_cell()->map() ==
      isolate->heap()->one_closure_cell_map()) {
    compilation_info()->MarkAsFunctionContextSpecializing();
  }

  data_.set_start_source_position(
      compilation_info()->shared_info()->start_position());

  linkage_ = new (compilation_info()->zone()) Linkage(
      Linkage::ComputeIncoming(compilation_info()->zone(), compilation_info()));

  if (!pipeline_.CreateGraph()) {
    if (isolate->has_pending_exception()) return FAILED;  // Stack overflowed.
    return AbortOptimization(kGraphBuildingFailed);
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
  Handle<Code> code = pipeline_.FinalizeCode();
  if (code.is_null()) {
    if (compilation_info()->bailout_reason() == kNoReason) {
      return AbortOptimization(kCodeGenerationFailed);
    }
    return FAILED;
  }
  compilation_info()->dependencies()->Commit(code);
  compilation_info()->SetCode(code);

  compilation_info()->context()->native_context()->AddOptimizedCode(*code);
  RegisterWeakObjectsInOptimizedCode(code, isolate);
  return SUCCEEDED;
}

namespace {

void AddWeakObjectToCodeDependency(Isolate* isolate, Handle<HeapObject> object,
                                   Handle<Code> code) {
  Handle<WeakCell> cell = Code::WeakCellFor(code);
  Heap* heap = isolate->heap();
  if (heap->InNewSpace(*object)) {
    heap->AddWeakNewSpaceObjectToCodeDependency(object, cell);
  } else {
    Handle<DependentCode> dep(heap->LookupWeakObjectToCodeDependency(object));
    dep =
        DependentCode::InsertWeakCode(dep, DependentCode::kWeakCodeGroup, cell);
    heap->AddWeakObjectToCodeDependency(object, dep);
  }
}

}  // namespace

void PipelineCompilationJob::RegisterWeakObjectsInOptimizedCode(
    Handle<Code> code, Isolate* isolate) {
  DCHECK(code->is_optimized_code());
  std::vector<Handle<Map>> maps;
  std::vector<Handle<HeapObject>> objects;
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
        } else {
          objects.push_back(object);
        }
      }
    }
  }
  for (Handle<Map> map : maps) {
    if (map->dependent_code()->IsEmpty(DependentCode::kWeakCodeGroup)) {
      isolate->heap()->AddRetainedMap(map);
    }
    Map::AddDependentCode(map, DependentCode::kWeakCodeGroup, code);
  }
  for (Handle<HeapObject> object : objects) {
    AddWeakObjectToCodeDependency(isolate, object, code);
  }
  code->set_can_have_weak_objects(true);
}

class PipelineWasmCompilationJob final : public CompilationJob {
 public:
  explicit PipelineWasmCompilationJob(
      CompilationInfo* info, Isolate* isolate, JSGraph* jsgraph,
      CallDescriptor* descriptor, SourcePositionTable* source_positions,
      std::vector<trap_handler::ProtectedInstructionData>* protected_insts,
      bool asmjs_origin)
      : CompilationJob(isolate->stack_guard()->real_climit(), nullptr, info,
                       "TurboFan", State::kReadyToExecute),
        zone_stats_(isolate->allocator()),
        pipeline_statistics_(CreatePipelineStatistics(
            Handle<Script>::null(), info, isolate, &zone_stats_)),
        data_(&zone_stats_, isolate, info, jsgraph, pipeline_statistics_.get(),
              source_positions, protected_insts),
        pipeline_(&data_),
        linkage_(descriptor),
        asmjs_origin_(asmjs_origin) {}

 protected:
  Status PrepareJobImpl(Isolate* isolate) final;
  Status ExecuteJobImpl() final;
  Status FinalizeJobImpl(Isolate* isolate) final;

 private:
  size_t AllocatedMemory() const override;

  // Temporary regression check while we get the wasm code off the GC heap, and
  // until we decontextualize wasm code.
  // We expect the only embedded objects to be: CEntryStub, undefined, and
  // the various builtins for throwing exceptions like OOB.
  void ValidateImmovableEmbeddedObjects() const;

  ZoneStats zone_stats_;
  std::unique_ptr<PipelineStatistics> pipeline_statistics_;
  PipelineData data_;
  PipelineImpl pipeline_;
  Linkage linkage_;
  bool asmjs_origin_;
};

PipelineWasmCompilationJob::Status PipelineWasmCompilationJob::PrepareJobImpl(
    Isolate* isolate) {
  UNREACHABLE();  // Prepare should always be skipped for WasmCompilationJob.
  return SUCCEEDED;
}

PipelineWasmCompilationJob::Status
PipelineWasmCompilationJob::ExecuteJobImpl() {
  if (FLAG_trace_turbo) {
    TurboJsonFile json_of(compilation_info(), std::ios_base::trunc);
    json_of << "{\"function\":\"" << compilation_info()->GetDebugName().get()
            << "\", \"source\":\"\",\n\"phases\":[";
  }

  pipeline_.RunPrintAndVerify("Machine", true);
  if (FLAG_wasm_opt || asmjs_origin_) {
    PipelineData* data = &data_;
    PipelineRunScope scope(data, "Wasm optimization");
    JSGraphReducer graph_reducer(data->jsgraph(), scope.zone());
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), scope.zone());
    ValueNumberingReducer value_numbering(scope.zone(), data->graph()->zone());
    MachineOperatorReducer machine_reducer(data->jsgraph(), asmjs_origin_);
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->common(), data->machine());
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &machine_reducer);
    AddReducer(data, &graph_reducer, &common_reducer);
    AddReducer(data, &graph_reducer, &value_numbering);
    graph_reducer.ReduceGraph();
    pipeline_.RunPrintAndVerify("Optimized Machine", true);
  }

  if (!pipeline_.ScheduleAndSelectInstructions(&linkage_, true)) return FAILED;
  pipeline_.AssembleCode(&linkage_);
  return SUCCEEDED;
}

size_t PipelineWasmCompilationJob::AllocatedMemory() const {
  return pipeline_.data_->zone_stats()->GetCurrentAllocatedBytes();
}

PipelineWasmCompilationJob::Status PipelineWasmCompilationJob::FinalizeJobImpl(
    Isolate* isolate) {
  if (!FLAG_wasm_jit_to_native) {
    pipeline_.FinalizeCode();
    ValidateImmovableEmbeddedObjects();
  } else {
    CodeGenerator* code_generator = pipeline_.data_->code_generator();
    CompilationInfo::WasmCodeDesc* wasm_code_desc =
        compilation_info()->wasm_code_desc();
    code_generator->tasm()->GetCode(isolate, &wasm_code_desc->code_desc);
    wasm_code_desc->safepoint_table_offset =
        code_generator->GetSafepointTableOffset();
    wasm_code_desc->frame_slot_count =
        code_generator->frame()->GetTotalFrameSlotCount();
    wasm_code_desc->source_positions_table =
        code_generator->GetSourcePositionTable();
    wasm_code_desc->handler_table = code_generator->GetHandlerTable();
  }
  return SUCCEEDED;
}

void PipelineWasmCompilationJob::ValidateImmovableEmbeddedObjects() const {
#if !DEBUG
  return;
#endif
  // We expect the only embedded objects to be those originating from
  // a snapshot, which are immovable.
  DisallowHeapAllocation no_gc;
  Handle<Code> result = pipeline_.data_->code();
  if (result.is_null()) return;
  // TODO(aseemgarg): remove this restriction when
  // wasm-to-js is also internally immovable to include WASM_TO_JS
  if (result->kind() != Code::WASM_FUNCTION) return;
  static const int kAllGCRefs = (1 << (RelocInfo::LAST_GCED_ENUM + 1)) - 1;
  for (RelocIterator it(*result, kAllGCRefs); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    Object* target = nullptr;
    switch (mode) {
      case RelocInfo::CODE_TARGET:
        // this would be either one of the stubs or builtins, because
        // we didn't link yet.
        target = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
        break;
      case RelocInfo::EMBEDDED_OBJECT:
        target = it.rinfo()->target_object();
        break;
      default:
        UNREACHABLE();
    }
    CHECK_NOT_NULL(target);
    bool is_immovable =
        target->IsSmi() || Heap::IsImmovable(HeapObject::cast(target));
    bool is_wasm = target->IsCode() &&
                   (Code::cast(target)->kind() == Code::WASM_FUNCTION ||
                    Code::cast(target)->kind() == Code::WASM_TO_JS_FUNCTION ||
                    Code::cast(target)->kind() == Code::WASM_TO_WASM_FUNCTION);
    bool is_allowed_stub = false;
    if (target->IsCode()) {
      Code* code = Code::cast(target);
      is_allowed_stub =
          code->kind() == Code::STUB &&
          CodeStub::MajorKeyFromKey(code->stub_key()) == CodeStub::DoubleToI;
    }
    CHECK(is_immovable || is_wasm || is_allowed_stub);
  }
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
  static const char* phase_name() { return "graph builder"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    JSTypeHintLowering::Flags flags = JSTypeHintLowering::kNoFlags;
    if (data->info()->is_bailout_on_uninitialized()) {
      flags |= JSTypeHintLowering::kBailoutOnUninitialized;
    }
    BytecodeGraphBuilder graph_builder(
        temp_zone, data->info()->shared_info(),
        handle(data->info()->closure()->feedback_vector()),
        data->info()->osr_offset(), data->jsgraph(), CallFrequency(1.0f),
        data->source_positions(), data->native_context(),
        SourcePosition::kNotInlined, flags);
    graph_builder.CreateGraph();
  }
};

namespace {

Maybe<OuterContext> GetModuleContext(Handle<JSFunction> closure) {
  Context* current = closure->context();
  size_t distance = 0;
  while (!current->IsNativeContext()) {
    if (current->IsModuleContext()) {
      return Just(OuterContext(handle(current), distance));
    }
    current = current->previous();
    distance++;
  }
  return Nothing<OuterContext>();
}

Maybe<OuterContext> ChooseSpecializationContext(CompilationInfo* info) {
  if (info->is_function_context_specializing()) {
    DCHECK(info->has_context());
    return Just(OuterContext(handle(info->context()), 0));
  }
  return GetModuleContext(info->closure());
}

}  // anonymous namespace

struct InliningPhase {
  static const char* phase_name() { return "inlining"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    CheckpointElimination checkpoint_elimination(&graph_reducer);
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->common(), data->machine());
    JSCallReducer call_reducer(&graph_reducer, data->jsgraph(),
                               data->info()->is_bailout_on_uninitialized()
                                   ? JSCallReducer::kBailoutOnUninitialized
                                   : JSCallReducer::kNoFlags,
                               data->native_context(),
                               data->info()->dependencies());
    JSContextSpecialization context_specialization(
        &graph_reducer, data->jsgraph(),
        ChooseSpecializationContext(data->info()),
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
    JSNativeContextSpecialization native_context_specialization(
        &graph_reducer, data->jsgraph(), flags, data->native_context(),
        data->info()->dependencies(), temp_zone);
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

    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    RemoveTypeReducer remove_type_reducer;
    AddReducer(data, &graph_reducer, &remove_type_reducer);
    graph_reducer.ReduceGraph();
  }
};

struct TypedLoweringPhase {
  static const char* phase_name() { return "typed lowering"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
    DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                              data->common(), temp_zone);
    JSBuiltinReducer builtin_reducer(
        &graph_reducer, data->jsgraph(),
        data->info()->dependencies(), data->native_context());
    JSCreateLowering create_lowering(
        &graph_reducer, data->info()->dependencies(), data->jsgraph(),
        data->native_context(), temp_zone);
    JSTypedLowering typed_lowering(&graph_reducer, data->jsgraph(), temp_zone);
    TypedOptimization typed_optimization(
        &graph_reducer, data->info()->dependencies(), data->jsgraph());
    SimplifiedOperatorReducer simple_reducer(&graph_reducer, data->jsgraph());
    CheckpointElimination checkpoint_elimination(&graph_reducer);
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->common(), data->machine());
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &builtin_reducer);
    AddReducer(data, &graph_reducer, &create_lowering);
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
    JSGraphReducer reducer(data->jsgraph(), temp_zone);
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

struct ConcurrentOptimizationPrepPhase {
  static const char* phase_name() { return "concurrency preparation"; }

  void Run(PipelineData* data, Zone* temp_zone) {
    // Make sure we cache these code stubs.
    data->jsgraph()->CEntryStubConstant(1);
    data->jsgraph()->CEntryStubConstant(2);

    // TODO(turbofan): Remove this line once the Array constructor code
    // is a proper builtin and no longer a CodeStub.
    data->jsgraph()->ArrayConstructorStubConstant();

    // This is needed for escape analysis.
    NodeProperties::SetType(data->jsgraph()->FalseConstant(), Type::Boolean());
    NodeProperties::SetType(data->jsgraph()->TrueConstant(), Type::Boolean());
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
                                              data->common(), temp_zone);
    SimplifiedOperatorReducer simple_reducer(&graph_reducer, data->jsgraph());
    RedundancyElimination redundancy_elimination(&graph_reducer, temp_zone);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    MachineOperatorReducer machine_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->common(), data->machine());
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
      TraceSchedule(data->info(), data->isolate(), schedule);

      // Post-pass for wiring the control/effects
      // - connect allocating representation changes into the control&effect
      //   chains and lower them,
      // - get rid of the region markers,
      // - introduce effect phis and rewire effects to get SSA again.
      EffectControlLinearizer::MaskArrayIndexEnable mask_array_index =
          data->info()->has_untrusted_code_mitigations()
              ? EffectControlLinearizer::kMaskArrayIndex
              : EffectControlLinearizer::kDoNotMaskArrayIndex;
      EffectControlLinearizer linearizer(data->jsgraph(), schedule, temp_zone,
                                         data->source_positions(),
                                         mask_array_index);
      linearizer.Run();
    }
    {
      // The {EffectControlLinearizer} might leave {Dead} nodes behind, so we
      // run {DeadCodeElimination} to prune these parts of the graph.
      // Also, the following store-store elimination phase greatly benefits from
      // doing a common operator reducer and dead code elimination just before
      // it, to eliminate conditional deopts with a constant condition.
      JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
      DeadCodeElimination dead_code_elimination(&graph_reducer, data->graph(),
                                                data->common(), temp_zone);
      CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                           data->common(), data->machine());
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
    JSGraphReducer graph_reducer(data->jsgraph(), temp_zone);
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
                                         data->common(), data->machine());
    AddReducer(data, &graph_reducer, &branch_condition_elimination);
    AddReducer(data, &graph_reducer, &dead_code_elimination);
    AddReducer(data, &graph_reducer, &redundancy_elimination);
    AddReducer(data, &graph_reducer, &load_elimination);
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
                                              data->common(), temp_zone);
    ValueNumberingReducer value_numbering(temp_zone, data->graph()->zone());
    MachineOperatorReducer machine_reducer(data->jsgraph());
    CommonOperatorReducer common_reducer(&graph_reducer, data->graph(),
                                         data->common(), data->machine());
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
        data->isolate()->serializer_enabled()
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
      JumpThreading::ApplyForwarding(result, data->sequence());
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
    CompilationInfo* info = data->info();
    Graph* graph = data->graph();

    if (FLAG_trace_turbo) {  // Print JSON.
      AllowHandleDereference allow_deref;
      TurboJsonFile json_of(info, std::ios_base::app);
      json_of << "{\"name\":\"" << phase << "\",\"type\":\"graph\",\"data\":"
              << AsJSON(*graph, data->source_positions()) << "},\n";
    }

    if (FLAG_trace_turbo_scheduled) {  // Scheduled textual output.
      AccountingAllocator allocator;
      Schedule* schedule = data->schedule();
      if (schedule == nullptr) {
        schedule = Scheduler::ComputeSchedule(temp_zone, data->graph(),
                                              Scheduler::kNoFlags);
      }

      AllowHandleDereference allow_deref;
      CodeTracer::Scope tracing_scope(data->isolate()->GetCodeTracer());
      OFStream os(tracing_scope.file());
      os << "-- Graph after " << phase << " -- " << std::endl;
      os << AsScheduledGraph(schedule);
    } else if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
      AllowHandleDereference allow_deref;
      CodeTracer::Scope tracing_scope(data->isolate()->GetCodeTracer());
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
      case Code::WASM_TO_WASM_FUNCTION:
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
  if (FLAG_trace_turbo || FLAG_trace_turbo_graph) {
    Run<PrintGraphPhase>(phase);
  }
  if (FLAG_turbo_verify) {
    Run<VerifyGraphPhase>(untyped);
  }
}

bool PipelineImpl::CreateGraph() {
  PipelineData* data = this->data_;

  data->BeginPhaseKind("graph creation");

  if (FLAG_trace_turbo || FLAG_trace_turbo_graph) {
    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "---------------------------------------------------\n"
       << "Begin compiling method " << info()->GetDebugName().get()
       << " using Turbofan" << std::endl;
  }
  if (FLAG_trace_turbo) {
    TurboCfgFile tcf(isolate());
    tcf << AsC1VCompilation(info());
  }

  data->source_positions()->AddDecorator();

  Run<GraphBuilderPhase>();
  RunPrintAndVerify("Initial untyped", true);

  // Perform function context specialization and inlining (if enabled).
  Run<InliningPhase>();
  RunPrintAndVerify("Inlined", true);

  // Remove dead->live edges from the graph.
  Run<EarlyGraphTrimmingPhase>();
  RunPrintAndVerify("Early trimmed", true);

  // Run the type-sensitive lowerings and optimizations on the graph.
  {
    // Determine the Typer operation flags.
    Typer::Flags flags = Typer::kNoFlags;
    if (is_sloppy(info()->shared_info()->language_mode()) &&
        info()->shared_info()->IsUserJavaScript()) {
      // Sloppy mode functions always have an Object for this.
      flags |= Typer::kThisIsReceiver;
    }
    if (IsClassConstructor(info()->shared_info()->kind())) {
      // Class constructors cannot be [[Call]]ed.
      flags |= Typer::kNewTargetIsReceiver;
    }

    // Type the graph and keep the Typer running on newly created nodes within
    // this scope; the Typer is automatically unlinked from the Graph once we
    // leave this scope below.
    Typer typer(isolate(), flags, data->graph());
    Run<TyperPhase>(&typer);
    RunPrintAndVerify("Typed");

    // Lower JSOperators where we can determine types.
    Run<TypedLoweringPhase>();
    RunPrintAndVerify("Lowered typed");
  }

  // Do some hacky things to prepare for the optimization phase.
  // (caching handles, etc.).
  Run<ConcurrentOptimizationPrepPhase>();

  data->EndPhaseKind();

  return true;
}

bool PipelineImpl::OptimizeGraph(Linkage* linkage) {
  PipelineData* data = this->data_;

  data->BeginPhaseKind("lowering");

  if (data->info()->is_loop_peeling_enabled()) {
    Run<LoopPeelingPhase>();
    RunPrintAndVerify("Loops peeled", true);
  } else {
    Run<LoopExitEliminationPhase>();
    RunPrintAndVerify("Loop exits eliminated", true);
  }

  if (FLAG_turbo_load_elimination) {
    Run<LoadEliminationPhase>();
    RunPrintAndVerify("Load eliminated");
  }

  if (FLAG_turbo_escape) {
    Run<EscapeAnalysisPhase>();
    if (data->compilation_failed()) {
      info()->AbortOptimization(kCyclicObjectStateDetectedInEscapeAnalysis);
      data->EndPhaseKind();
      return false;
    }
    RunPrintAndVerify("Escape Analysed");
  }

  // Perform simplified lowering. This has to run w/o the Typer decorator,
  // because we cannot compute meaningful types anyways, and the computed types
  // might even conflict with the representation/truncation logic.
  Run<SimplifiedLoweringPhase>();
  RunPrintAndVerify("Simplified lowering", true);

  // From now on it is invalid to look at types on the nodes, because the types
  // on the nodes might not make sense after representation selection due to the
  // way we handle truncations; if we'd want to look at types afterwards we'd
  // essentially need to re-type (large portions of) the graph.

  // In order to catch bugs related to type access after this point, we now
  // remove the types from the nodes (currently only in Debug builds).
#ifdef DEBUG
  Run<UntyperPhase>();
  RunPrintAndVerify("Untyped", true);
#endif

  // Run generic lowering pass.
  Run<GenericLoweringPhase>();
  RunPrintAndVerify("Generic lowering", true);

  data->BeginPhaseKind("block building");

  // Run early optimization pass.
  Run<EarlyOptimizationPhase>();
  RunPrintAndVerify("Early optimized", true);

  Run<EffectControlLinearizationPhase>();
  RunPrintAndVerify("Effect and control linearized", true);

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

  data->source_positions()->RemoveDecorator();

  return ScheduleAndSelectInstructions(linkage, true);
}

Handle<Code> Pipeline::GenerateCodeForCodeStub(
    Isolate* isolate, CallDescriptor* call_descriptor, Graph* graph,
    Schedule* schedule, Code::Kind kind, const char* debug_name,
    uint32_t stub_key, int32_t builtin_index, JumpOptimizationInfo* jump_opt) {
  CompilationInfo info(CStrVector(debug_name), graph->zone(), kind);
  info.set_builtin_index(builtin_index);
  info.set_stub_key(stub_key);

  // Construct a pipeline for scheduling and code generation.
  ZoneStats zone_stats(isolate->allocator());
  SourcePositionTable source_positions(graph);
  PipelineData data(&zone_stats, &info, isolate, graph, schedule,
                    &source_positions, jump_opt);
  data.set_verify_graph(FLAG_verify_csa);
  std::unique_ptr<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics.reset(
        new PipelineStatistics(&info, isolate, &zone_stats));
    pipeline_statistics->BeginPhaseKind("stub codegen");
  }

  PipelineImpl pipeline(&data);
  DCHECK_NOT_NULL(data.schedule());

  if (FLAG_trace_turbo || FLAG_trace_turbo_graph) {
    CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "---------------------------------------------------\n"
       << "Begin compiling " << debug_name << " using Turbofan" << std::endl;
    if (FLAG_trace_turbo) {
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
Handle<Code> Pipeline::GenerateCodeForTesting(CompilationInfo* info,
                                              Isolate* isolate) {
  ZoneStats zone_stats(isolate->allocator());
  std::unique_ptr<PipelineStatistics> pipeline_statistics(
      CreatePipelineStatistics(Handle<Script>::null(), info, isolate,
                               &zone_stats));
  PipelineData data(&zone_stats, isolate, info, pipeline_statistics.get());
  PipelineImpl pipeline(&data);

  Linkage linkage(Linkage::ComputeIncoming(data.instruction_zone(), info));
  Deoptimizer::EnsureCodeForMaxDeoptimizationEntries(isolate);

  if (!pipeline.CreateGraph()) return Handle<Code>::null();
  if (!pipeline.OptimizeGraph(&linkage)) return Handle<Code>::null();
  pipeline.AssembleCode(&linkage);
  return pipeline.FinalizeCode();
}

// static
Handle<Code> Pipeline::GenerateCodeForTesting(CompilationInfo* info,
                                              Isolate* isolate, Graph* graph,
                                              Schedule* schedule) {
  CallDescriptor* call_descriptor =
      Linkage::ComputeIncoming(info->zone(), info);
  return GenerateCodeForTesting(info, isolate, call_descriptor, graph,
                                schedule);
}

// static
Handle<Code> Pipeline::GenerateCodeForTesting(
    CompilationInfo* info, Isolate* isolate, CallDescriptor* call_descriptor,
    Graph* graph, Schedule* schedule, SourcePositionTable* source_positions) {
  // Construct a pipeline for scheduling and code generation.
  ZoneStats zone_stats(isolate->allocator());
  // TODO(wasm): Refactor code generation to check for non-existing source
  // table, then remove this conditional allocation.
  if (!source_positions)
    source_positions = new (info->zone()) SourcePositionTable(graph);
  PipelineData data(&zone_stats, info, isolate, graph, schedule,
                    source_positions, nullptr);
  std::unique_ptr<PipelineStatistics> pipeline_statistics;
  if (FLAG_turbo_stats || FLAG_turbo_stats_nvp) {
    pipeline_statistics.reset(
        new PipelineStatistics(info, isolate, &zone_stats));
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
CompilationJob* Pipeline::NewCompilationJob(Handle<JSFunction> function,
                                            bool has_script) {
  Handle<SharedFunctionInfo> shared = handle(function->shared());
  ParseInfo* parse_info;
  if (!has_script) {
    parse_info = ParseInfo::AllocateWithoutScript(shared);
  } else {
    parse_info = new ParseInfo(shared);
  }
  return new PipelineCompilationJob(parse_info, shared, function);
}

// static
CompilationJob* Pipeline::NewWasmCompilationJob(
    CompilationInfo* info, Isolate* isolate, JSGraph* jsgraph,
    CallDescriptor* descriptor, SourcePositionTable* source_positions,
    std::vector<trap_handler::ProtectedInstructionData>* protected_instructions,
    wasm::ModuleOrigin asmjs_origin) {
  return new PipelineWasmCompilationJob(info, isolate, jsgraph, descriptor,
                                        source_positions,
                                        protected_instructions, asmjs_origin);
}

bool Pipeline::AllocateRegistersForTesting(const RegisterConfiguration* config,
                                           InstructionSequence* sequence,
                                           bool run_verifier) {
  CompilationInfo info(ArrayVector("testing"), sequence->zone(), Code::STUB);
  ZoneStats zone_stats(sequence->isolate()->allocator());
  PipelineData data(&zone_stats, &info, sequence->isolate(), sequence);
  PipelineImpl pipeline(&data);
  pipeline.data_->InitializeFrameData(nullptr);
  pipeline.AllocateRegisters(config, nullptr, run_verifier);
  return !data.compilation_failed();
}

bool PipelineImpl::ScheduleAndSelectInstructions(Linkage* linkage,
                                                 bool trim_graph) {
  CallDescriptor* call_descriptor = linkage->GetIncomingDescriptor();
  PipelineData* data = this->data_;

  DCHECK_NOT_NULL(data->graph());

  if (trim_graph) {
    Run<LateGraphTrimmingPhase>();
    RunPrintAndVerify("Late trimmed", true);
  }
  if (data->schedule() == nullptr) Run<ComputeSchedulePhase>();
  TraceSchedule(data->info(), data->isolate(), data->schedule());

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
      CodeTracer::Scope tracing_scope(data->isolate()->GetCodeTracer());
      OFStream os(tracing_scope.file());
      os << "--------------------------------------------------\n"
         << "--- Verifying " << data->debug_name() << " generated by TurboFan\n"
         << "--------------------------------------------------\n"
         << *data->schedule()
         << "--------------------------------------------------\n"
         << "--- End of " << data->debug_name() << " generated by TurboFan\n"
         << "--------------------------------------------------\n";
    }
    Zone temp_zone(data->isolate()->allocator(), ZONE_NAME);
    MachineGraphVerifier::Run(data->graph(), data->schedule(), linkage,
                              data->info()->IsStub(), data->debug_name(),
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
  if (call_descriptor->HasRestrictedAllocatableRegisters()) {
    auto registers = call_descriptor->AllocatableRegisters();
    DCHECK_LT(0, NumRegs(registers));
    std::unique_ptr<const RegisterConfiguration> config;
    config.reset(RegisterConfiguration::RestrictGeneralRegisters(registers));
    AllocateRegisters(config.get(), call_descriptor, run_verifier);
  } else {
    AllocateRegisters(RegisterConfiguration::Default(), call_descriptor,
                      run_verifier);
  }

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

void PipelineImpl::AssembleCode(Linkage* linkage) {
  PipelineData* data = this->data_;
  data->BeginPhaseKind("code generation");
  data->InitializeCodeGenerator(linkage);
  Run<AssembleCodePhase>();
  data->DeleteInstructionZone();
}

Handle<Code> PipelineImpl::FinalizeCode() {
  PipelineData* data = this->data_;
  Run<FinalizeCodePhase>();

  Handle<Code> code = data->code();
  if (data->profiler_data()) {
#if ENABLE_DISASSEMBLER
    std::ostringstream os;
    code->Disassemble(nullptr, os);
    data->profiler_data()->SetCode(&os);
#endif
  }

  info()->SetCode(code);
  PrintCode(code, info());

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
  }
  if (FLAG_trace_turbo || FLAG_trace_turbo_graph) {
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
  if (!ScheduleAndSelectInstructions(&linkage, false)) return Handle<Code>();

  // Generate the final machine code.
  AssembleCode(&linkage);
  return FinalizeCode();
}

void PipelineImpl::AllocateRegisters(const RegisterConfiguration* config,
                                     CallDescriptor* descriptor,
                                     bool run_verifier) {
  PipelineData* data = this->data_;
  // Don't track usage for this zone in compiler stats.
  std::unique_ptr<Zone> verifier_zone;
  RegisterAllocatorVerifier* verifier = nullptr;
  if (run_verifier) {
    verifier_zone.reset(new Zone(isolate()->allocator(), ZONE_NAME));
    verifier = new (verifier_zone.get()) RegisterAllocatorVerifier(
        verifier_zone.get(), config, data->sequence());
  }

#ifdef DEBUG
  data_->sequence()->ValidateEdgeSplitForm();
  data_->sequence()->ValidateDeferredBlockEntryPaths();
  data_->sequence()->ValidateDeferredBlockExitPaths();
#endif

  data->InitializeRegisterAllocationData(config, descriptor);
  if (info()->is_osr()) data->osr_helper()->SetupFrame(data->frame());

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

  if (FLAG_trace_turbo_graph) {
    AllowHandleDereference allow_deref;
    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "----- Instruction sequence after register allocation -----\n"
       << PrintableInstructionSequence({config, data->sequence()});
  }

  if (verifier != nullptr) {
    verifier->VerifyAssignment("End of regalloc pipeline.");
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

Isolate* PipelineImpl::isolate() const { return data_->isolate(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8

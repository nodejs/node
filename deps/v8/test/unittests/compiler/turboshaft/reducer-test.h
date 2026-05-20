// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "src/codegen/interface-descriptors.h"
#include "src/codegen/turboshaft-builtins-assembler-inl.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/turbofan-graph-visualizer.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::compiler::turboshaft {

class TestInstance {
 public:
  using assembler_t = Assembler<VariableReducer>;

  struct CapturedOperation {
    TestInstance* instance;
    OpIndex input;
    std::set<OpIndex> generated_output;

    bool IsEmpty() const { return generated_output.empty(); }

    bool Is(OpIndex index) const {
      return generated_output.size() == 1 && generated_output.contains(index);
    }

    template <typename Op>
    bool Contains() const {
      for (OpIndex o : generated_output) {
        if (instance->graph().Get(o).Is<Op>()) return true;
      }
      return false;
    }

    template <typename Op>
    const underlying_operation_t<Op>* GetFirst() const {
      for (OpIndex o : generated_output) {
        if (auto result = instance->graph().Get(o).TryCast<Op>()) {
          return result;
        }
      }
      return nullptr;
    }

    template <typename Op>
    const underlying_operation_t<Op>* GetAs() const {
      DCHECK_EQ(generated_output.size(), 1);
      return GetFirst<Op>();
    }

    const Operation* Get() const {
      DCHECK_EQ(generated_output.size(), 1);
      return &instance->graph().Get(*generated_output.begin());
    }
  };

  template <typename Builder>
  static TestInstance CreateFromGraph(PipelineData* data, int parameter_count,
                                      const Builder& builder, Isolate* isolate,
                                      Zone* zone, Handle<Context> context) {
    TestInstance instance(data, isolate, zone, context);
    // Generate a function prolog
    Block* start_block = instance.Asm().NewBlock();
    instance.Asm().Bind(start_block);
    for (int i = 0; i < parameter_count; ++i) {
      instance.parameters_.push_back(
          instance.Asm().Parameter(i, RegisterRepresentation::Tagged()));
    }
    builder(instance);
    // Need to clear the Assembler before returning so that we don't end up with
    // multiple Assemblers alive later.
    instance.ClearAssembler();
    return instance;
  }

  template <typename Builder>
  static TestInstance CreateFromGraph(
      PipelineData* data,
      base::Vector<const RegisterRepresentation> parameter_reps,
      const Builder& builder, Isolate* isolate, Zone* zone,
      Handle<Context> context) {
    TestInstance instance(data, isolate, zone, context);
    // Generate a function prolog
    Block* start_block = instance.Asm().NewBlock();
    instance.Asm().Bind(start_block);
    for (size_t i = 0; i < parameter_reps.size(); ++i) {
      instance.parameters_.push_back(
          instance.Asm().Parameter(static_cast<int>(i), parameter_reps[i]));
    }
    builder(instance);
    // Need to clear the Assembler before returning so that we don't end up with
    // multiple Assemblers alive later.
    instance.ClearAssembler();
    return instance;
  }

  assembler_t& Asm() {
    DCHECK(assembler_);
    return *assembler_;
  }
  Graph& graph() { return *graph_; }
  Factory& factory() { return *isolate_->factory(); }
  Zone* zone() { return zone_; }
  V<Context> context() { return Asm().HeapConstantNoHole(context_); }

  assembler_t& operator()() { return Asm(); }

  void ClearAssembler() { assembler_.reset(); }

  template <template <typename> typename... Reducers>
  void Run(bool trace_reductions = v8_flags.turboshaft_trace_reduction) {
    Assembler<GraphVisitor, Reducers...> phase(
        data_, graph(), graph().GetOrCreateCompanion(), zone_);
#ifdef DEBUG
    if (trace_reductions) {
      phase.template VisitGraph<true>();
    } else {
      phase.template VisitGraph<false>();
    }
#else
    phase.template VisitGraph<false>();
#endif
    // Map all captured inputs.
    for (auto& [key, captured] : captured_operations_) {
      std::set<OpIndex> temp = std::move(captured.generated_output);
      for (OpIndex index : graph_->AllOperationIndices()) {
        OpIndex origin = graph_->operation_origins()[index];
        if (temp.contains(origin)) captured.generated_output.insert(index);
      }
    }
  }

  int GetParameterCount() const { return static_cast<int>(parameters_.size()); }

  template <typename T = Object>
  V<T> GetParameter(int index) {
    DCHECK_LE(0, index);
    DCHECK_LT(index, parameters_.size());
    DCHECK(v_traits<T>::allows_representation(
        graph_->Get(parameters_[index]).Cast<ParameterOp>().rep));
    return parameters_[index];
  }
  OpIndex BuildFrameState() {
    FrameStateData::Builder builder;
    // Closure
    builder.AddInput(MachineType::AnyTagged(),
                     Asm().SmiConstant(Smi::FromInt(0)));
    // TODO(nicohartmann@): Parameters, Context, Locals, Accumulator if
    // necessary.

    FrameStateFunctionInfo* function_info =
        zone_->template New<FrameStateFunctionInfo>(
            FrameStateType::kUnoptimizedFunction, 0, 0, 0,
            Handle<SharedFunctionInfo>{}, Handle<BytecodeArray>{});
    const FrameStateInfo* frame_state_info =
        zone_->template New<FrameStateInfo>(BytecodeOffset(0),
                                            OutputFrameStateCombine::Ignore(),
                                            function_info);

    return Asm().FrameState(
        builder.Inputs(), builder.inlined(),
        builder.AllocateFrameStateData(*frame_state_info, zone_));
  }

  OpIndex Capture(OpIndex input, const std::string& key) {
    captured_operations_[key] =
        CapturedOperation{this, input, std::set<OpIndex>{input}};
    return input;
  }
  template <typename T>
  V<T> Capture(V<T> input, const std::string& key) {
    return V<T>::Cast(Capture(static_cast<OpIndex>(input), key));
  }
  const CapturedOperation& GetCapture(const std::string& key) const {
    auto it = captured_operations_.find(key);
    DCHECK_NE(it, captured_operations_.end());
    return it->second;
  }
  const Operation* GetCaptured(const std::string& key) const {
    return GetCapture(key).Get();
  }
  template <typename Op>
  const underlying_operation_t<Op>* GetCapturedAs(
      const std::string& key) const {
    return GetCapture(key).GetAs<Op>();
  }

  size_t CountOp(Opcode opcode) {
    auto operations = graph().AllOperations();
    return std::count_if(
        operations.begin(), operations.end(),
        [opcode](const Operation& op) { return op.opcode == opcode; });
  }

  struct CaptureHelper {
    TestInstance* instance;
    std::string key;
    OpIndex operator=(OpIndex value) { return instance->Capture(value, key); }
  };
  CaptureHelper CaptureHelperForMacro(const std::string& key) {
    return CaptureHelper{this, std::move(key)};
  }

  void PrintGraphForTurbolizer(const char* phase_name) {
    if (!stream_) {
      const testing::TestInfo* test_info =
          testing::UnitTest::GetInstance()->current_test_info();
      std::stringstream file_name;
      file_name << "turbo-" << test_info->test_suite_name() << "_"
                << test_info->name() << ".json";
      stream_ = std::make_unique<std::ofstream>(file_name.str(),
                                                std::ios_base::trunc);
      *stream_ << "{\"function\" : ";
      size_t len = strlen("test_generated_function") + 1;
      auto name = std::make_unique<char[]>(len);
      snprintf(name.get(), len, "test_generated_function");
      JsonPrintFunctionSource(*stream_, -1, std::move(name),
                              DirectHandle<Script>{}, isolate_,
                              DirectHandle<SharedFunctionInfo>{});
      *stream_ << ",\n\"phases\":[";
    }
    PrintTurboshaftGraphForTurbolizer(*stream_, graph(), phase_name, nullptr,
                                      zone_);
    // Flush the output stream to get a proper file even when the test crashes
    // afterwards.
    stream_->flush();
  }

  Handle<Code> CompileWithBuiltinPipeline(CallDescriptor* call_descriptor);
  Handle<Code> CompileAsJSBuiltin();

 private:
  TestInstance(PipelineData* data, Isolate* isolate, Zone* zone,
               Handle<Context> context)
      : data_(data),
        assembler_(std::make_unique<assembler_t>(data, data_->graph(),
                                                 data_->graph(), zone)),
        graph_(&data_->graph()),
        isolate_(isolate),
        zone_(zone),
        context_(context) {}

  PipelineData* data_;
  std::unique_ptr<assembler_t> assembler_;
  Graph* graph_;
  std::unique_ptr<std::ofstream> stream_;
  Isolate* isolate_;
  Zone* zone_;
  Handle<Context> context_;
  base::SmallMap<std::map<std::string, CapturedOperation>> captured_operations_;
  base::SmallVector<OpIndex, 4> parameters_;
};

class ReducerTest : public TestWithNativeContextAndZone {
 public:
  using assembler_t = TestInstance::assembler_t;

  template <typename Builder>
  TestInstance CreateFromGraph(int parameter_count, const Builder& builder) {
    Initialize();
    Handle<Context> context = indirect_handle(native_context());
    return TestInstance::CreateFromGraph(pipeline_data_.get(), parameter_count,
                                         builder, isolate(), zone(), context);
  }

  template <typename Builder>
  TestInstance CreateFromGraph(
      base::Vector<const RegisterRepresentation> parameter_reps,
      const Builder& builder) {
    Initialize();
    Handle<Context> context = indirect_handle(native_context());
    return TestInstance::CreateFromGraph(pipeline_data_.get(), parameter_reps,
                                         builder, isolate(), zone(), context);
  }

 private:
  void Initialize() {
    const testing::TestInfo* test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    std::stringstream file_name;
    const char* debug_name = test_info->name();
    size_t debug_name_len = std::strlen(debug_name);

    info_.reset(new OptimizedCompilationInfo(
        base::Vector<const char>(debug_name, debug_name_len), this->zone(),
        CodeKind::FOR_TESTING_JS));
    pipeline_data_.reset(new turboshaft::PipelineData(
        &zone_stats_, TurboshaftPipelineKind::kJS, this->isolate(), info_.get(),
        AssemblerOptions::Default(this->isolate())));
    pipeline_data_->InitializeGraphComponent(nullptr,
                                             Graph::Origin::kPureTurboshaft);
  }

  void TearDown() override {
    pipeline_data_.reset();
    info_.reset();
  }

  ZoneStats zone_stats_{this->zone()->allocator()};
  std::unique_ptr<OptimizedCompilationInfo> info_;
  std::unique_ptr<turboshaft::PipelineData> pipeline_data_;
};

}  // namespace v8::internal::compiler::turboshaft

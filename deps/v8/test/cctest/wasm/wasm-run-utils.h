// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WASM_RUN_UTILS_H
#define WASM_RUN_UTILS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include "src/base/utils/random-number-generator.h"
#include "src/zone/accounting-allocator.h"

#include "src/compiler/graph-visualizer.h"
#include "src/compiler/int64-lowering.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/wasm-compiler.h"
#include "src/compiler/zone-pool.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

#include "src/zone/zone.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/call-tester.h"
#include "test/cctest/compiler/graph-builder-tester.h"

static const uint32_t kMaxFunctions = 10;

enum WasmExecutionMode { kExecuteInterpreted, kExecuteCompiled };

// TODO(titzer): check traps more robustly in tests.
// Currently, in tests, we just return 0xdeadbeef from the function in which
// the trap occurs if the runtime context is not available to throw a JavaScript
// exception.
#define CHECK_TRAP32(x) \
  CHECK_EQ(0xdeadbeef, (bit_cast<uint32_t>(x)) & 0xFFFFFFFF)
#define CHECK_TRAP64(x) \
  CHECK_EQ(0xdeadbeefdeadbeef, (bit_cast<uint64_t>(x)) & 0xFFFFFFFFFFFFFFFF)
#define CHECK_TRAP(x) CHECK_TRAP32(x)

#define WASM_RUNNER_MAX_NUM_PARAMETERS 4
#define WASM_WRAPPER_RETURN_VALUE 8754

#define BUILD(r, ...)                      \
  do {                                     \
    byte code[] = {__VA_ARGS__};           \
    r.Build(code, code + arraysize(code)); \
  } while (false)

namespace {
using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

const uint32_t kMaxGlobalsSize = 128;

// A helper for module environments that adds the ability to allocate memory
// and global variables. Contains a built-in {WasmModule} and
// {WasmModuleInstance}.
class TestingModule : public ModuleEnv {
 public:
  explicit TestingModule(WasmExecutionMode mode = kExecuteCompiled)
      : execution_mode_(mode),
        instance_(&module_),
        isolate_(CcTest::InitIsolateOnce()),
        global_offset(0),
        interpreter_(mode == kExecuteInterpreted
                         ? new WasmInterpreter(&instance_, &allocator_)
                         : nullptr) {
    module = &module_;
    instance = &instance_;
    instance->module = &module_;
    instance->globals_start = global_data;
    module_.globals_size = kMaxGlobalsSize;
    instance->mem_start = nullptr;
    instance->mem_size = 0;
    origin = kWasmOrigin;
    memset(global_data, 0, sizeof(global_data));
  }

  ~TestingModule() {
    if (instance->mem_start) {
      free(instance->mem_start);
    }
    if (interpreter_) delete interpreter_;
  }

  byte* AddMemory(uint32_t size) {
    CHECK_NULL(instance->mem_start);
    CHECK_EQ(0, instance->mem_size);
    instance->mem_start = reinterpret_cast<byte*>(malloc(size));
    CHECK(instance->mem_start);
    memset(instance->mem_start, 0, size);
    instance->mem_size = size;
    return raw_mem_start<byte>();
  }

  template <typename T>
  T* AddMemoryElems(uint32_t count) {
    AddMemory(count * sizeof(T));
    return raw_mem_start<T>();
  }

  template <typename T>
  T* AddGlobal(LocalType type) {
    const WasmGlobal* global = AddGlobal(type);
    return reinterpret_cast<T*>(instance->globals_start + global->offset);
  }

  byte AddSignature(FunctionSig* sig) {
    module_.signatures.push_back(sig);
    size_t size = module->signatures.size();
    CHECK(size < 127);
    return static_cast<byte>(size - 1);
  }

  template <typename T>
  T* raw_mem_start() {
    DCHECK(instance->mem_start);
    return reinterpret_cast<T*>(instance->mem_start);
  }

  template <typename T>
  T* raw_mem_end() {
    DCHECK(instance->mem_start);
    return reinterpret_cast<T*>(instance->mem_start + instance->mem_size);
  }

  template <typename T>
  T raw_mem_at(int i) {
    DCHECK(instance->mem_start);
    return ReadMemory(&(reinterpret_cast<T*>(instance->mem_start)[i]));
  }

  template <typename T>
  T raw_val_at(int i) {
    return ReadMemory(reinterpret_cast<T*>(instance->mem_start + i));
  }

  template <typename T>
  void WriteMemory(T* p, T val) {
    WriteLittleEndianValue<T>(p, val);
  }

  template <typename T>
  T ReadMemory(T* p) {
    return ReadLittleEndianValue<T>(p);
  }

  // Zero-initialize the memory.
  void BlankMemory() {
    byte* raw = raw_mem_start<byte>();
    memset(raw, 0, instance->mem_size);
  }

  // Pseudo-randomly intialize the memory.
  void RandomizeMemory(unsigned int seed = 88) {
    byte* raw = raw_mem_start<byte>();
    byte* end = raw_mem_end<byte>();
    v8::base::RandomNumberGenerator rng;
    rng.SetSeed(seed);
    rng.NextBytes(raw, end - raw);
  }

  uint32_t AddFunction(FunctionSig* sig, Handle<Code> code) {
    if (module->functions.size() == 0) {
      // TODO(titzer): Reserving space here to avoid the underlying WasmFunction
      // structs from moving.
      module_.functions.reserve(kMaxFunctions);
    }
    uint32_t index = static_cast<uint32_t>(module->functions.size());
    module_.functions.push_back({sig, index, 0, 0, 0, 0, 0, false, false});
    instance->function_code.push_back(code);
    if (interpreter_) {
      const WasmFunction* function = &module->functions.back();
      int interpreter_index = interpreter_->AddFunctionForTesting(function);
      CHECK_EQ(index, static_cast<uint32_t>(interpreter_index));
    }
    DCHECK_LT(index, kMaxFunctions);  // limited for testing.
    return index;
  }

  uint32_t AddJsFunction(FunctionSig* sig, const char* source) {
    Handle<JSFunction> jsfunc = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
        *v8::Local<v8::Function>::Cast(CompileRun(source))));
    uint32_t index = AddFunction(sig, Handle<Code>::null());
    Handle<Code> code =
        CompileWasmToJSWrapper(isolate_, jsfunc, sig, index,
                               Handle<String>::null(), Handle<String>::null());
    instance->function_code[index] = code;
    return index;
  }

  Handle<JSFunction> WrapCode(uint32_t index) {
    // Wrap the code so it can be called as a JS function.
    Handle<String> name = isolate_->factory()->NewStringFromStaticChars("main");
    Handle<JSObject> module_object = Handle<JSObject>(0, isolate_);
    Handle<Code> code = instance->function_code[index];
    WasmJs::InstallWasmMapsIfNeeded(isolate_, isolate_->native_context());
    Handle<Code> ret_code =
        compiler::CompileJSToWasmWrapper(isolate_, this, code, index);
    FunctionSig* funcSig = this->module->functions[index].sig;
    Handle<ByteArray> exportedSig = isolate_->factory()->NewByteArray(
        static_cast<int>(funcSig->parameter_count() + funcSig->return_count()),
        TENURED);
    exportedSig->copy_in(0, reinterpret_cast<const byte*>(funcSig->raw_data()),
                         exportedSig->length());
    Handle<JSFunction> ret = WrapExportCodeAsJSFunction(
        isolate_, ret_code, name,
        static_cast<int>(this->module->functions[index].sig->parameter_count()),
        exportedSig, module_object);
    return ret;
  }

  void SetFunctionCode(uint32_t index, Handle<Code> code) {
    instance->function_code[index] = code;
  }

  void AddIndirectFunctionTable(uint16_t* functions, uint32_t table_size) {
    module_.function_tables.push_back(
        {table_size, table_size, std::vector<int32_t>(), false, false});
    for (uint32_t i = 0; i < table_size; ++i) {
      module_.function_tables.back().values.push_back(functions[i]);
    }

    Handle<FixedArray> values = BuildFunctionTable(
        isolate_, static_cast<int>(module_.function_tables.size() - 1),
        &module_);
    instance->function_tables.push_back(values);
  }

  void PopulateIndirectFunctionTable() {
    for (uint32_t i = 0; i < instance->function_tables.size(); i++) {
      PopulateFunctionTable(instance->function_tables[i],
                            module_.function_tables[i].size,
                            &instance->function_code);
    }
  }

  WasmFunction* GetFunctionAt(int index) { return &module_.functions[index]; }

  WasmInterpreter* interpreter() { return interpreter_; }
  WasmExecutionMode execution_mode() { return execution_mode_; }

 private:
  WasmExecutionMode execution_mode_;
  WasmModule module_;
  WasmModuleInstance instance_;
  Isolate* isolate_;
  v8::internal::AccountingAllocator allocator_;
  uint32_t global_offset;
  V8_ALIGNED(8) byte global_data[kMaxGlobalsSize];  // preallocated global data.
  WasmInterpreter* interpreter_;

  const WasmGlobal* AddGlobal(LocalType type) {
    byte size = WasmOpcodes::MemSize(WasmOpcodes::MachineTypeFor(type));
    global_offset = (global_offset + size - 1) & ~(size - 1);  // align
    module_.globals.push_back(
        {type, true, NO_INIT, global_offset, false, false});
    global_offset += size;
    // limit number of globals.
    CHECK_LT(global_offset, kMaxGlobalsSize);
    return &module->globals.back();
  }
};

inline void TestBuildingGraph(Zone* zone, JSGraph* jsgraph, ModuleEnv* module,
                              FunctionSig* sig,
                              SourcePositionTable* source_position_table,
                              const byte* start, const byte* end) {
  compiler::WasmGraphBuilder builder(zone, jsgraph, sig, source_position_table);
  DecodeResult result =
      BuildTFGraph(zone->allocator(), &builder, module, sig, start, end);
  if (result.failed()) {
    if (!FLAG_trace_wasm_decoder) {
      // Retry the compilation with the tracing flag on, to help in debugging.
      FLAG_trace_wasm_decoder = true;
      result =
          BuildTFGraph(zone->allocator(), &builder, module, sig, start, end);
    }

    ptrdiff_t pc = result.error_pc - result.start;
    ptrdiff_t pt = result.error_pt - result.start;
    std::ostringstream str;
    str << "Verification failed: " << result.error_code << " pc = +" << pc;
    if (result.error_pt) str << ", pt = +" << pt;
    str << ", msg = " << result.error_msg.get();
    FATAL(str.str().c_str());
  }
  builder.Int64LoweringForTesting();
  if (FLAG_trace_turbo_graph) {
    OFStream os(stdout);
    os << AsRPO(*jsgraph->graph());
  }
}

template <typename ReturnType>
class WasmFunctionWrapper : public HandleAndZoneScope,
                            private GraphAndBuilders {
 public:
  WasmFunctionWrapper()
      : GraphAndBuilders(main_zone()),
        inner_code_node_(nullptr),
        signature_(nullptr) {
    // One additional parameter for the pointer to the return value memory.
    Signature<MachineType>::Builder sig_builder(
        zone(), 1, WASM_RUNNER_MAX_NUM_PARAMETERS + 1);

    sig_builder.AddReturn(MachineType::Int32());
    for (int i = 0; i < WASM_RUNNER_MAX_NUM_PARAMETERS + 1; i++) {
      sig_builder.AddParam(MachineType::Pointer());
    }
    signature_ = sig_builder.Build();
  }

  void Init(CallDescriptor* descriptor, MachineType p0 = MachineType::None(),
            MachineType p1 = MachineType::None(),
            MachineType p2 = MachineType::None(),
            MachineType p3 = MachineType::None()) {
    // Create the TF graph for the wrapper. The wrapper always takes four
    // pointers as parameters, but may not pass the values of all pointers to
    // the actual test function.

    // Function, effect, and control.
    Node** parameters =
        zone()->template NewArray<Node*>(WASM_RUNNER_MAX_NUM_PARAMETERS + 3);
    graph()->SetStart(graph()->NewNode(common()->Start(6)));
    Node* effect = graph()->start();
    int parameter_count = 0;

    // Dummy node which gets replaced in SetInnerCode.
    inner_code_node_ = graph()->NewNode(common()->Int32Constant(0));
    parameters[parameter_count++] = inner_code_node_;

    if (p0 != MachineType::None()) {
      parameters[parameter_count] = graph()->NewNode(
          machine()->Load(p0),
          graph()->NewNode(common()->Parameter(0), graph()->start()),
          graph()->NewNode(common()->Int32Constant(0)), effect,
          graph()->start());
      effect = parameters[parameter_count++];
    }
    if (p1 != MachineType::None()) {
      parameters[parameter_count] = graph()->NewNode(
          machine()->Load(p0),
          graph()->NewNode(common()->Parameter(1), graph()->start()),
          graph()->NewNode(common()->Int32Constant(0)), effect,
          graph()->start());
      effect = parameters[parameter_count++];
    }
    if (p2 != MachineType::None()) {
      parameters[parameter_count] = graph()->NewNode(
          machine()->Load(p0),
          graph()->NewNode(common()->Parameter(2), graph()->start()),
          graph()->NewNode(common()->Int32Constant(0)), effect,
          graph()->start());
      effect = parameters[parameter_count++];
    }
    if (p3 != MachineType::None()) {
      parameters[parameter_count] = graph()->NewNode(
          machine()->Load(p0),
          graph()->NewNode(common()->Parameter(3), graph()->start()),
          graph()->NewNode(common()->Int32Constant(0)), effect,
          graph()->start());
      effect = parameters[parameter_count++];
    }

    parameters[parameter_count++] = effect;
    parameters[parameter_count++] = graph()->start();
    Node* call = graph()->NewNode(common()->Call(descriptor), parameter_count,
                                  parameters);

    effect = graph()->NewNode(
        machine()->Store(
            StoreRepresentation(MachineTypeForC<ReturnType>().representation(),
                                WriteBarrierKind::kNoWriteBarrier)),
        graph()->NewNode(common()->Parameter(WASM_RUNNER_MAX_NUM_PARAMETERS),
                         graph()->start()),
        graph()->NewNode(common()->Int32Constant(0)), call, effect,
        graph()->start());
    Node* r = graph()->NewNode(
        common()->Return(),
        graph()->NewNode(common()->Int32Constant(WASM_WRAPPER_RETURN_VALUE)),
        effect, graph()->start());
    graph()->SetEnd(graph()->NewNode(common()->End(2), r, graph()->start()));
  }

  void SetInnerCode(Handle<Code> code_handle) {
    NodeProperties::ChangeOp(inner_code_node_,
                             common()->HeapConstant(code_handle));
  }

  Handle<Code> GetWrapperCode() {
    if (code_.is_null()) {
      Isolate* isolate = CcTest::InitIsolateOnce();

      CallDescriptor* descriptor =
          Linkage::GetSimplifiedCDescriptor(zone(), signature_, true);

      if (kPointerSize == 4) {
        // One additional parameter for the pointer of the return value.
        Signature<MachineRepresentation>::Builder rep_builder(
            zone(), 1, WASM_RUNNER_MAX_NUM_PARAMETERS + 1);

        rep_builder.AddReturn(MachineRepresentation::kWord32);
        for (int i = 0; i < WASM_RUNNER_MAX_NUM_PARAMETERS + 1; i++) {
          rep_builder.AddParam(MachineRepresentation::kWord32);
        }
        Int64Lowering r(graph(), machine(), common(), zone(),
                        rep_builder.Build());
        r.LowerGraph();
      }

      CompilationInfo info(ArrayVector("testing"), isolate, graph()->zone(),
                           Code::ComputeFlags(Code::STUB));
      code_ =
          Pipeline::GenerateCodeForTesting(&info, descriptor, graph(), nullptr);
      CHECK(!code_.is_null());
#ifdef ENABLE_DISASSEMBLER
      if (FLAG_print_opt_code) {
        OFStream os(stdout);
        code_->Disassemble("wasm wrapper", os);
      }
#endif
    }

    return code_;
  }

  Signature<MachineType>* signature() const { return signature_; }

 private:
  Node* inner_code_node_;
  Handle<Code> code_;
  Signature<MachineType>* signature_;
};

// A helper for compiling WASM functions for testing. This class can create a
// standalone function if {module} is NULL or a function within a
// {TestingModule}. It contains the internal state for compilation (i.e.
// TurboFan graph) and interpretation (by adding to the interpreter manually).
class WasmFunctionCompiler : public HandleAndZoneScope,
                             private GraphAndBuilders {
 public:
  explicit WasmFunctionCompiler(
      FunctionSig* sig, WasmExecutionMode mode,
      Vector<const char> debug_name = ArrayVector("<WASM UNNAMED>"))
      : GraphAndBuilders(main_zone()),
        execution_mode_(mode),
        jsgraph(this->isolate(), this->graph(), this->common(), nullptr,
                nullptr, this->machine()),
        sig(sig),
        descriptor_(nullptr),
        testing_module_(nullptr),
        debug_name_(debug_name),
        local_decls(main_zone(), sig),
        source_position_table_(this->graph()),
        interpreter_(nullptr) {
    // Create our own function.
    function_ = new WasmFunction();
    function_->sig = sig;
    function_->func_index = 0;
    function_->sig_index = 0;
    if (mode == kExecuteInterpreted) {
      interpreter_ = new WasmInterpreter(nullptr, zone()->allocator());
      int index = interpreter_->AddFunctionForTesting(function_);
      CHECK_EQ(0, index);
    }
  }

  explicit WasmFunctionCompiler(
      FunctionSig* sig, TestingModule* module,
      Vector<const char> debug_name = ArrayVector("<WASM UNNAMED>"))
      : GraphAndBuilders(main_zone()),
        execution_mode_(module->execution_mode()),
        jsgraph(this->isolate(), this->graph(), this->common(), nullptr,
                nullptr, this->machine()),
        sig(sig),
        descriptor_(nullptr),
        testing_module_(module),
        debug_name_(debug_name),
        local_decls(main_zone(), sig),
        source_position_table_(this->graph()),
        interpreter_(module->interpreter()) {
    // Get a new function from the testing module.
    int index = module->AddFunction(sig, Handle<Code>::null());
    function_ = testing_module_->GetFunctionAt(index);
  }

  ~WasmFunctionCompiler() {
    if (testing_module_) return;  // testing module owns the below things.
    delete function_;
    if (interpreter_) delete interpreter_;
  }

  WasmExecutionMode execution_mode_;
  JSGraph jsgraph;
  FunctionSig* sig;
  // The call descriptor is initialized when the function is compiled.
  CallDescriptor* descriptor_;
  TestingModule* testing_module_;
  Vector<const char> debug_name_;
  WasmFunction* function_;
  LocalDeclEncoder local_decls;
  SourcePositionTable source_position_table_;
  WasmInterpreter* interpreter_;

  Isolate* isolate() { return main_isolate(); }
  Graph* graph() const { return main_graph_; }
  Zone* zone() const { return graph()->zone(); }
  CommonOperatorBuilder* common() { return &main_common_; }
  MachineOperatorBuilder* machine() { return &main_machine_; }
  void InitializeDescriptor() {
    if (descriptor_ == nullptr) {
      descriptor_ = testing_module_->GetWasmCallDescriptor(main_zone(), sig);
    }
  }
  CallDescriptor* descriptor() { return descriptor_; }
  uint32_t function_index() { return function_->func_index; }

  void Build(const byte* start, const byte* end) {
    // Build the TurboFan graph.
    local_decls.Prepend(main_zone(), &start, &end);
    TestBuildingGraph(main_zone(), &jsgraph, testing_module_, sig,
                      &source_position_table_, start, end);
    if (interpreter_) {
      // Add the code to the interpreter.
      CHECK(interpreter_->SetFunctionCodeForTesting(function_, start, end));
    }
  }

  byte AllocateLocal(LocalType type) {
    uint32_t index = local_decls.AddLocals(1, type);
    byte result = static_cast<byte>(index);
    DCHECK_EQ(index, result);
    return result;
  }

  Handle<Code> Compile() {
    InitializeDescriptor();
    CallDescriptor* desc = descriptor_;
    if (kPointerSize == 4) {
      desc = testing_module_->GetI32WasmCallDescriptor(this->zone(), desc);
    }
    CompilationInfo info(debug_name_, this->isolate(), this->zone(),
                         Code::ComputeFlags(Code::WASM_FUNCTION));
    std::unique_ptr<CompilationJob> job(Pipeline::NewWasmCompilationJob(
        &info, graph(), desc, &source_position_table_));
    if (job->ExecuteJob() != CompilationJob::SUCCEEDED ||
        job->FinalizeJob() != CompilationJob::SUCCEEDED)
      return Handle<Code>::null();

    Handle<Code> code = info.code();

    // Length is always 2, since usually <wasm_obj, func_index> is stored in
    // the deopt data. Here, we only store the function index.
    DCHECK(code->deoptimization_data() == nullptr ||
           code->deoptimization_data()->length() == 0);
    Handle<FixedArray> deopt_data =
        isolate()->factory()->NewFixedArray(2, TENURED);
    deopt_data->set(1, Smi::FromInt(static_cast<int>(function_index())));
    deopt_data->set_length(2);
    code->set_deoptimization_data(*deopt_data);

#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_opt_code) {
      OFStream os(stdout);
      code->Disassemble("wasm code", os);
    }
#endif

    return code;
  }

  uint32_t CompileAndAdd(uint16_t sig_index = 0) {
    CHECK(testing_module_);
    function_->sig_index = sig_index;
    Handle<Code> code = Compile();
    testing_module_->SetFunctionCode(function_index(), code);
    return function_index();
  }

  // Set the context, such that e.g. runtime functions can be called.
  void SetModuleContext() {
    if (!testing_module_->instance->context.is_null()) {
      CHECK(testing_module_->instance->context.is_identical_to(
          main_isolate()->native_context()));
      return;
    }
    testing_module_->instance->context = main_isolate()->native_context();
  }
};

// A helper class to build graphs from Wasm bytecode, generate machine
// code, and run that code.
template <typename ReturnType>
class WasmRunner {
 public:
  WasmRunner(WasmExecutionMode execution_mode,
             MachineType p0 = MachineType::None(),
             MachineType p1 = MachineType::None(),
             MachineType p2 = MachineType::None(),
             MachineType p3 = MachineType::None())
      : zone(&allocator_),
        compiled_(false),
        signature_(MachineTypeForC<ReturnType>() == MachineType::None() ? 0 : 1,
                   GetParameterCount(p0, p1, p2, p3), storage_),
        compiler_(&signature_, execution_mode) {
    InitSigStorage(p0, p1, p2, p3);
  }

  WasmRunner(TestingModule* module, MachineType p0 = MachineType::None(),
             MachineType p1 = MachineType::None(),
             MachineType p2 = MachineType::None(),
             MachineType p3 = MachineType::None())
      : zone(&allocator_),
        compiled_(false),
        signature_(MachineTypeForC<ReturnType>() == MachineType::None() ? 0 : 1,
                   GetParameterCount(p0, p1, p2, p3), storage_),
        compiler_(&signature_, module) {
    DCHECK(module);
    InitSigStorage(p0, p1, p2, p3);
  }

  void InitSigStorage(MachineType p0, MachineType p1, MachineType p2,
                      MachineType p3) {
    int index = 0;
    MachineType ret = MachineTypeForC<ReturnType>();
    if (ret != MachineType::None()) {
      storage_[index++] = WasmOpcodes::LocalTypeFor(ret);
    }
    if (p0 != MachineType::None())
      storage_[index++] = WasmOpcodes::LocalTypeFor(p0);
    if (p1 != MachineType::None())
      storage_[index++] = WasmOpcodes::LocalTypeFor(p1);
    if (p2 != MachineType::None())
      storage_[index++] = WasmOpcodes::LocalTypeFor(p2);
    if (p3 != MachineType::None())
      storage_[index++] = WasmOpcodes::LocalTypeFor(p3);

    compiler_.InitializeDescriptor();
    wrapper_.Init(compiler_.descriptor(), p0, p1, p2, p3);
  }

  // Builds a graph from the given Wasm code and generates the machine
  // code and call wrapper for that graph. This method must not be called
  // more than once.
  void Build(const byte* start, const byte* end) {
    CHECK(!compiled_);
    compiled_ = true;
    compiler_.Build(start, end);

    if (!interpret()) {
      // Compile machine code and install it into the module.
      Handle<Code> code = compiler_.Compile();

      if (compiler_.testing_module_) {
        // Update the table of function code in the module.
        compiler_.testing_module_->SetFunctionCode(
            compiler_.function_->func_index, code);
      }

      wrapper_.SetInnerCode(code);
    }
  }

  ReturnType Call() {
    if (interpret()) {
      return CallInterpreter(Vector<WasmVal>(nullptr, 0));
    } else {
      return Call(0, 0, 0, 0);
    }
  }

  template <typename P0>
  ReturnType Call(P0 p0) {
    if (interpret()) {
      WasmVal args[] = {WasmVal(p0)};
      return CallInterpreter(ArrayVector(args));
    } else {
      return Call(p0, 0, 0, 0);
    }
  }

  template <typename P0, typename P1>
  ReturnType Call(P0 p0, P1 p1) {
    if (interpret()) {
      WasmVal args[] = {WasmVal(p0), WasmVal(p1)};
      return CallInterpreter(ArrayVector(args));
    } else {
      return Call(p0, p1, 0, 0);
    }
  }

  template <typename P0, typename P1, typename P2>
  ReturnType Call(P0 p0, P1 p1, P2 p2) {
    if (interpret()) {
      WasmVal args[] = {WasmVal(p0), WasmVal(p1), WasmVal(p2)};
      return CallInterpreter(ArrayVector(args));
    } else {
      return Call(p0, p1, p2, 0);
    }
  }

  template <typename P0, typename P1, typename P2, typename P3>
  ReturnType Call(P0 p0, P1 p1, P2 p2, P3 p3) {
    if (interpret()) {
      WasmVal args[] = {WasmVal(p0), WasmVal(p1), WasmVal(p2), WasmVal(p3)};
      return CallInterpreter(ArrayVector(args));
    } else {
      CodeRunner<int32_t> runner(CcTest::InitIsolateOnce(),
                                 wrapper_.GetWrapperCode(),
                                 wrapper_.signature());
      ReturnType return_value;
      int32_t result = runner.Call<void*, void*, void*, void*, void*>(
          &p0, &p1, &p2, &p3, &return_value);
      CHECK_EQ(WASM_WRAPPER_RETURN_VALUE, result);
      return return_value;
    }
  }

  ReturnType CallInterpreter(Vector<WasmVal> args) {
    CHECK_EQ(args.length(),
             static_cast<int>(compiler_.function_->sig->parameter_count()));
    WasmInterpreter::Thread* thread = interpreter()->GetThread(0);
    thread->Reset();
    thread->PushFrame(compiler_.function_, args.start());
    if (thread->Run() == WasmInterpreter::FINISHED) {
      WasmVal val = thread->GetReturnValue();
      return val.to<ReturnType>();
    } else if (thread->state() == WasmInterpreter::TRAPPED) {
      // TODO(titzer): return the correct trap code
      int64_t result = 0xdeadbeefdeadbeef;
      return static_cast<ReturnType>(result);
    } else {
      // TODO(titzer): falling off end
      ReturnType val = 0;
      return val;
    }
  }

  byte AllocateLocal(LocalType type) { return compiler_.AllocateLocal(type); }

  WasmFunction* function() { return compiler_.function_; }
  WasmInterpreter* interpreter() { return compiler_.interpreter_; }

 protected:
  v8::internal::AccountingAllocator allocator_;
  Zone zone;
  bool compiled_;
  LocalType storage_[WASM_RUNNER_MAX_NUM_PARAMETERS];
  FunctionSig signature_;
  WasmFunctionCompiler compiler_;
  WasmFunctionWrapper<ReturnType> wrapper_;

  bool interpret() { return compiler_.execution_mode_ == kExecuteInterpreted; }

  static size_t GetParameterCount(MachineType p0, MachineType p1,
                                  MachineType p2, MachineType p3) {
    if (p0 == MachineType::None()) return 0;
    if (p1 == MachineType::None()) return 1;
    if (p2 == MachineType::None()) return 2;
    if (p3 == MachineType::None()) return 3;
    return 4;
  }
};

// A macro to define tests that run in different engine configurations.
// Currently only supports compiled tests, but a future
// RunWasmInterpreted_##name version will allow each test to also run in the
// interpreter.
#define WASM_EXEC_TEST(name)                                               \
  void RunWasm_##name(WasmExecutionMode execution_mode);                   \
  TEST(RunWasmCompiled_##name) { RunWasm_##name(kExecuteCompiled); }       \
  TEST(RunWasmInterpreted_##name) { RunWasm_##name(kExecuteInterpreted); } \
  void RunWasm_##name(WasmExecutionMode execution_mode)

}  // namespace

#endif

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WASM_RUN_UTILS_H
#define WASM_RUN_UTILS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/base/utils/random-number-generator.h"

#include "src/compiler/graph-visualizer.h"
#include "src/compiler/int64-lowering.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/wasm-compiler.h"
#include "src/compiler/zone-pool.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

#include "src/zone.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/call-tester.h"
#include "test/cctest/compiler/graph-builder-tester.h"

// TODO(titzer): pull WASM_64 up to a common header.
#if !V8_TARGET_ARCH_32_BIT || V8_TARGET_ARCH_X64
#define WASM_64 1
#else
#define WASM_64 0
#endif

static const uint32_t kMaxFunctions = 10;

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
  TestingModule() : instance_(&module_), global_offset(0) {
    module_.shared_isolate = CcTest::InitIsolateOnce();
    module = &module_;
    instance = &instance_;
    instance->module = &module_;
    instance->globals_start = global_data;
    instance->globals_size = kMaxGlobalsSize;
    instance->mem_start = nullptr;
    instance->mem_size = 0;
    linker = nullptr;
    origin = kWasmOrigin;
    memset(global_data, 0, sizeof(global_data));
  }

  ~TestingModule() {
    if (instance->mem_start) {
      free(instance->mem_start);
    }
  }

  byte* AddMemory(size_t size) {
    CHECK_NULL(instance->mem_start);
    CHECK_EQ(0, instance->mem_size);
    instance->mem_start = reinterpret_cast<byte*>(malloc(size));
    CHECK(instance->mem_start);
    memset(instance->mem_start, 0, size);
    instance->mem_size = size;
    return raw_mem_start<byte>();
  }

  template <typename T>
  T* AddMemoryElems(size_t count) {
    AddMemory(count * sizeof(T));
    return raw_mem_start<T>();
  }

  template <typename T>
  T* AddGlobal(MachineType mem_type) {
    WasmGlobal* global = AddGlobal(mem_type);
    return reinterpret_cast<T*>(instance->globals_start + global->offset);
  }

  byte AddSignature(FunctionSig* sig) {
    module->signatures.push_back(sig);
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
    return reinterpret_cast<T*>(instance->mem_start)[i];
  }

  template <typename T>
  T raw_val_at(int i) {
    T val;
    memcpy(&val, reinterpret_cast<void*>(instance->mem_start + i), sizeof(T));
    return val;
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
      module->functions.reserve(kMaxFunctions);
    }
    uint32_t index = static_cast<uint32_t>(module->functions.size());
    module->functions.push_back({sig, index, 0, 0, 0, 0, 0, false});
    instance->function_code.push_back(code);
    DCHECK_LT(index, kMaxFunctions);  // limited for testing.
    return index;
  }

  uint32_t AddJsFunction(FunctionSig* sig, const char* source) {
    Handle<JSFunction> jsfunc = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
        *v8::Local<v8::Function>::Cast(CompileRun(source))));
    uint32_t index = AddFunction(sig, Handle<Code>::null());
    Isolate* isolate = module->shared_isolate;
    WasmName module_name = ArrayVector("test");
    WasmName function_name;
    Handle<Code> code = CompileWasmToJSWrapper(isolate, this, jsfunc, sig,
                                               module_name, function_name);
    instance->function_code[index] = code;
    return index;
  }

  Handle<JSFunction> WrapCode(uint32_t index) {
    Isolate* isolate = module->shared_isolate;
    // Wrap the code so it can be called as a JS function.
    Handle<String> name = isolate->factory()->NewStringFromStaticChars("main");
    Handle<JSObject> module_object = Handle<JSObject>(0, isolate);
    Handle<Code> code = instance->function_code[index];
    WasmJs::InstallWasmFunctionMap(isolate, isolate->native_context());
    return compiler::CompileJSToWasmWrapper(isolate, this, name, code,
                                            module_object, index);
  }

  void SetFunctionCode(uint32_t index, Handle<Code> code) {
    instance->function_code[index] = code;
  }

  void AddIndirectFunctionTable(int* functions, int table_size) {
    Isolate* isolate = module->shared_isolate;
    Handle<FixedArray> fixed =
        isolate->factory()->NewFixedArray(2 * table_size);
    instance->function_table = fixed;
    DCHECK_EQ(0u, module->function_table.size());
    for (int i = 0; i < table_size; i++) {
      module->function_table.push_back(functions[i]);
    }
  }

  void PopulateIndirectFunctionTable() {
    if (instance->function_table.is_null()) return;
    int table_size = static_cast<int>(module->function_table.size());
    for (int i = 0; i < table_size; i++) {
      int function_index = module->function_table[i];
      WasmFunction* function = &module->functions[function_index];
      instance->function_table->set(i, Smi::FromInt(function->sig_index));
      instance->function_table->set(i + table_size,
                                    *instance->function_code[function_index]);
    }
  }

 private:
  WasmModule module_;
  WasmModuleInstance instance_;
  uint32_t global_offset;
  V8_ALIGNED(8) byte global_data[kMaxGlobalsSize];  // preallocated global data.

  WasmGlobal* AddGlobal(MachineType mem_type) {
    byte size = WasmOpcodes::MemSize(mem_type);
    global_offset = (global_offset + size - 1) & ~(size - 1);  // align
    module->globals.push_back({0, 0, mem_type, global_offset, false});
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
  TreeResult result =
      BuildTFGraph(zone->allocator(), &builder, module, sig, start, end);
  if (result.failed()) {
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

      CompilationInfo info(ArrayVector("testing"), isolate, graph()->zone());
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
// TurboFan graph) and, later, interpretation.
class WasmFunctionCompiler : public HandleAndZoneScope,
                             private GraphAndBuilders {
 public:
  explicit WasmFunctionCompiler(
      FunctionSig* sig, TestingModule* module,
      Vector<const char> debug_name = ArrayVector("<WASM UNNAMED>"))
      : GraphAndBuilders(main_zone()),
        jsgraph(this->isolate(), this->graph(), this->common(), nullptr,
                nullptr, this->machine()),
        sig(sig),
        descriptor_(nullptr),
        testing_module_(module),
        debug_name_(debug_name),
        local_decls(main_zone(), sig),
        source_position_table_(this->graph()) {
    if (module) {
      // Get a new function from the testing module.
      function_ = nullptr;
      function_index_ = module->AddFunction(sig, Handle<Code>::null());
    } else {
      // Create our own function.
      function_ = new WasmFunction();
      function_->sig = sig;
      function_index_ = 0;
    }
  }

  ~WasmFunctionCompiler() {
    if (function_) delete function_;
  }

  JSGraph jsgraph;
  FunctionSig* sig;
  // The call descriptor is initialized when the function is compiled.
  CallDescriptor* descriptor_;
  TestingModule* testing_module_;
  Vector<const char> debug_name_;
  WasmFunction* function_;
  int function_index_;
  LocalDeclEncoder local_decls;
  SourcePositionTable source_position_table_;

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

  void Build(const byte* start, const byte* end) {
    // Build the TurboFan graph.
    local_decls.Prepend(&start, &end);
    TestBuildingGraph(main_zone(), &jsgraph, testing_module_, sig,
                      &source_position_table_, start, end);
    delete[] start;
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
    v8::base::SmartPointer<CompilationJob> job(Pipeline::NewWasmCompilationJob(
        &info, graph(), desc, &source_position_table_));
    if (job->OptimizeGraph() != CompilationJob::SUCCEEDED ||
        job->GenerateCode() != CompilationJob::SUCCEEDED)
      return Handle<Code>::null();

    Handle<Code> code = info.code();

    // Length is always 2, since usually <wasm_obj, func_index> is stored in the
    // deopt data. Here, we only store the function index.
    DCHECK(code->deoptimization_data() == nullptr ||
           code->deoptimization_data()->length() == 0);
    Handle<FixedArray> deopt_data =
        isolate()->factory()->NewFixedArray(2, TENURED);
    deopt_data->set(1, Smi::FromInt(function_index_));
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
    function()->sig_index = sig_index;
    Handle<Code> code = Compile();
    testing_module_->SetFunctionCode(function_index_, code);
    return static_cast<uint32_t>(function_index_);
  }

  WasmFunction* function() {
    if (function_) return function_;
    return &testing_module_->module->functions[function_index_];
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
  WasmRunner(MachineType p0 = MachineType::None(),
             MachineType p1 = MachineType::None(),
             MachineType p2 = MachineType::None(),
             MachineType p3 = MachineType::None())
      : zone(&allocator_),
        compiled_(false),
        signature_(MachineTypeForC<ReturnType>() == MachineType::None() ? 0 : 1,
                   GetParameterCount(p0, p1, p2, p3), storage_),
        compiler_(&signature_, nullptr) {
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

    // Build the TF graph within the compiler.
    compiler_.Build(start, end);
    // Generate code.
    Handle<Code> code = compiler_.Compile();

    if (compiler_.testing_module_) {
      // Update the table of function code in the module.
      compiler_.testing_module_->SetFunctionCode(compiler_.function_index_,
                                                 code);
    }

    wrapper_.SetInnerCode(code);
  }

  ReturnType Call() { return Call(0, 0, 0, 0); }

  template <typename P0>
  ReturnType Call(P0 p0) {
    return Call(p0, 0, 0, 0);
  }

  template <typename P0, typename P1>
  ReturnType Call(P0 p0, P1 p1) {
    return Call(p0, p1, 0, 0);
  }

  template <typename P0, typename P1, typename P2>
  ReturnType Call(P0 p0, P1 p1, P2 p2) {
    return Call(p0, p1, p2, 0);
  }

  template <typename P0, typename P1, typename P2, typename P3>
  ReturnType Call(P0 p0, P1 p1, P2 p2, P3 p3) {
    CodeRunner<int32_t> runner(CcTest::InitIsolateOnce(),
                               wrapper_.GetWrapperCode(), wrapper_.signature());
    ReturnType return_value;
    int32_t result = runner.Call<void*, void*, void*, void*, void*>(
        &p0, &p1, &p2, &p3, &return_value);
    CHECK_EQ(WASM_WRAPPER_RETURN_VALUE, result);
    return return_value;
  }

  byte AllocateLocal(LocalType type) { return compiler_.AllocateLocal(type); }

 protected:
  v8::base::AccountingAllocator allocator_;
  Zone zone;
  bool compiled_;
  LocalType storage_[WASM_RUNNER_MAX_NUM_PARAMETERS];
  FunctionSig signature_;
  WasmFunctionCompiler compiler_;
  WasmFunctionWrapper<ReturnType> wrapper_;

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
#define WASM_EXEC_TEST(name)                         \
  void RunWasm_##name();                             \
  TEST(RunWasmCompiled_##name) { RunWasm_##name(); } \
  void RunWasm_##name()

}  // namespace

#endif

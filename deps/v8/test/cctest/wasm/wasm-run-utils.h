// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WASM_RUN_UTILS_H
#define WASM_RUN_UTILS_H

#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <array>
#include <memory>

#include "src/base/utils/random-number-generator.h"
#include "src/zone/accounting-allocator.h"

#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/int64-lowering.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/wasm-compiler.h"
#include "src/compiler/zone-stats.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-external-refs.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
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
// {WasmInstance}.
class TestingModule : public ModuleEnv {
 public:
  explicit TestingModule(Zone* zone, WasmExecutionMode mode = kExecuteCompiled)
      : ModuleEnv(&module_, &instance_),
        execution_mode_(mode),
        instance_(&module_),
        isolate_(CcTest::InitIsolateOnce()),
        global_offset(0),
        interpreter_(mode == kExecuteInterpreted
                         ? new WasmInterpreter(
                               ModuleBytesEnv(&module_, &instance_,
                                              Vector<const byte>::empty()),
                               zone->allocator())
                         : nullptr) {
    WasmJs::Install(isolate_);
    instance->module = &module_;
    instance->globals_start = global_data;
    module_.globals_size = kMaxGlobalsSize;
    instance->mem_start = nullptr;
    instance->mem_size = 0;
    memset(global_data, 0, sizeof(global_data));
    instance_object_ = InitInstanceObject();
  }

  ~TestingModule() {
    if (instance->mem_start) {
      free(instance->mem_start);
    }
    if (interpreter_) delete interpreter_;
  }

  void ChangeOriginToAsmjs() { module_.origin = kAsmJsOrigin; }

  byte* AddMemory(uint32_t size) {
    CHECK(!module_.has_memory);
    CHECK_NULL(instance->mem_start);
    CHECK_EQ(0, instance->mem_size);
    module_.has_memory = true;
    instance->mem_start = reinterpret_cast<byte*>(malloc(size));
    CHECK(size == 0 || instance->mem_start);
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
  T* AddGlobal(
      ValueType type = WasmOpcodes::ValueTypeFor(MachineTypeForC<T>())) {
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

  void SetMaxMemPages(uint32_t max_mem_pages) {
    module_.max_mem_pages = max_mem_pages;
  }

  uint32_t AddFunction(FunctionSig* sig, Handle<Code> code, const char* name) {
    if (module->functions.size() == 0) {
      // TODO(titzer): Reserving space here to avoid the underlying WasmFunction
      // structs from moving.
      module_.functions.reserve(kMaxFunctions);
    }
    uint32_t index = static_cast<uint32_t>(module->functions.size());
    module_.functions.push_back({sig, index, 0, 0, 0, 0, 0, false, false});
    if (name) {
      Vector<const byte> name_vec = Vector<const byte>::cast(CStrVector(name));
      module_.functions.back().name_offset = AddBytes(name_vec);
      module_.functions.back().name_length = name_vec.length();
    }
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
    uint32_t index = AddFunction(sig, Handle<Code>::null(), nullptr);
    Handle<Code> code = CompileWasmToJSWrapper(
        isolate_, jsfunc, sig, index, Handle<String>::null(),
        Handle<String>::null(), module->origin);
    instance->function_code[index] = code;
    return index;
  }

  Handle<JSFunction> WrapCode(uint32_t index) {
    // Wrap the code so it can be called as a JS function.
    Handle<WasmInstanceObject> instance_obj(0, isolate_);
    Handle<Code> code = instance->function_code[index];
    Handle<Code> ret_code =
        compiler::CompileJSToWasmWrapper(isolate_, &module_, code, index);
    Handle<JSFunction> ret = WasmExportedFunction::New(
        isolate_, instance_obj, MaybeHandle<String>(), static_cast<int>(index),
        static_cast<int>(this->module->functions[index].sig->parameter_count()),
        ret_code);
    return ret;
  }

  void SetFunctionCode(uint32_t index, Handle<Code> code) {
    instance->function_code[index] = code;
  }

  void AddIndirectFunctionTable(uint16_t* function_indexes,
                                uint32_t table_size) {
    module_.function_tables.push_back({table_size, table_size, true,
                                       std::vector<int32_t>(), false, false,
                                       SignatureMap()});
    WasmIndirectFunctionTable& table = module_.function_tables.back();
    table.min_size = table_size;
    table.max_size = table_size;
    for (uint32_t i = 0; i < table_size; ++i) {
      table.values.push_back(function_indexes[i]);
      table.map.FindOrInsert(module_.functions[function_indexes[i]].sig);
    }

    instance->function_tables.push_back(
        isolate_->factory()->NewFixedArray(table_size));
    instance->signature_tables.push_back(
        isolate_->factory()->NewFixedArray(table_size));
  }

  void PopulateIndirectFunctionTable() {
    if (execution_mode_ == kExecuteInterpreted) return;
    // Initialize the fixed arrays in instance->function_tables.
    for (uint32_t i = 0; i < instance->function_tables.size(); i++) {
      WasmIndirectFunctionTable& table = module_.function_tables[i];
      Handle<FixedArray> function_table = instance->function_tables[i];
      Handle<FixedArray> signature_table = instance->signature_tables[i];
      int table_size = static_cast<int>(table.values.size());
      for (int j = 0; j < table_size; j++) {
        WasmFunction& function = module_.functions[table.values[j]];
        signature_table->set(j, Smi::FromInt(table.map.Find(function.sig)));
        function_table->set(j, *instance->function_code[function.func_index]);
      }
    }
  }

  uint32_t AddBytes(Vector<const byte> bytes) {
    Handle<SeqOneByteString> old_bytes(
        instance_object_->compiled_module()->module_bytes(), isolate_);
    uint32_t old_size = static_cast<uint32_t>(old_bytes->length());
    ScopedVector<byte> new_bytes(old_size + bytes.length());
    memcpy(new_bytes.start(), old_bytes->GetChars(), old_size);
    memcpy(new_bytes.start() + old_size, bytes.start(), bytes.length());
    Handle<SeqOneByteString> new_bytes_str = Handle<SeqOneByteString>::cast(
        isolate_->factory()->NewStringFromOneByte(new_bytes).ToHandleChecked());
    instance_object_->compiled_module()->shared()->set_module_bytes(
        *new_bytes_str);
    return old_size;
  }

  WasmFunction* GetFunctionAt(int index) { return &module_.functions[index]; }

  WasmInterpreter* interpreter() { return interpreter_; }
  WasmExecutionMode execution_mode() { return execution_mode_; }
  Isolate* isolate() { return isolate_; }
  Handle<WasmInstanceObject> instance_object() { return instance_object_; }

 private:
  WasmExecutionMode execution_mode_;
  WasmModule module_;
  WasmInstance instance_;
  Isolate* isolate_;
  uint32_t global_offset;
  V8_ALIGNED(8) byte global_data[kMaxGlobalsSize];  // preallocated global data.
  WasmInterpreter* interpreter_;
  Handle<WasmInstanceObject> instance_object_;

  const WasmGlobal* AddGlobal(ValueType type) {
    byte size = WasmOpcodes::MemSize(WasmOpcodes::MachineTypeFor(type));
    global_offset = (global_offset + size - 1) & ~(size - 1);  // align
    module_.globals.push_back(
        {type, true, WasmInitExpr(), global_offset, false, false});
    global_offset += size;
    // limit number of globals.
    CHECK_LT(global_offset, kMaxGlobalsSize);
    return &module->globals.back();
  }

  Handle<WasmInstanceObject> InitInstanceObject() {
    Handle<SeqOneByteString> empty_string = Handle<SeqOneByteString>::cast(
        isolate_->factory()->NewStringFromOneByte({}).ToHandleChecked());
    Handle<Managed<wasm::WasmModule>> module_wrapper =
        Managed<wasm::WasmModule>::New(isolate_, &module_, false);
    Handle<Script> script =
        isolate_->factory()->NewScript(isolate_->factory()->empty_string());
    script->set_type(Script::TYPE_WASM);
    Handle<WasmSharedModuleData> shared_module_data =
        WasmSharedModuleData::New(isolate_, module_wrapper, empty_string,
                                  script, Handle<ByteArray>::null());
    Handle<WasmCompiledModule> compiled_module =
        WasmCompiledModule::New(isolate_, shared_module_data);
    // Minimally initialize the compiled module such that IsWasmCompiledModule
    // passes.
    // If tests need more (correct) information, add it later.
    compiled_module->set_min_mem_pages(0);
    compiled_module->set_max_mem_pages(Smi::kMaxValue);
    DCHECK(WasmCompiledModule::IsWasmCompiledModule(*compiled_module));
    return WasmInstanceObject::New(isolate_, compiled_module);
  }
};

inline void TestBuildingGraph(Zone* zone, JSGraph* jsgraph, ModuleEnv* module,
                              FunctionSig* sig,
                              SourcePositionTable* source_position_table,
                              const byte* start, const byte* end) {
  compiler::WasmGraphBuilder builder(module, zone, jsgraph, sig,
                                     source_position_table);
  DecodeResult result =
      BuildTFGraph(zone->allocator(), &builder, sig, start, end);
  if (result.failed()) {
    if (!FLAG_trace_wasm_decoder) {
      // Retry the compilation with the tracing flag on, to help in debugging.
      FLAG_trace_wasm_decoder = true;
      result = BuildTFGraph(zone->allocator(), &builder, sig, start, end);
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
  if (!CpuFeatures::SupportsSimd128()) {
    builder.SimdScalarLoweringForTesting();
  }
}

class WasmFunctionWrapper : private GraphAndBuilders {
 public:
  explicit WasmFunctionWrapper(Zone* zone, int num_params)
      : GraphAndBuilders(zone), inner_code_node_(nullptr), signature_(nullptr) {
    // One additional parameter for the pointer to the return value memory.
    Signature<MachineType>::Builder sig_builder(zone, 1, num_params + 1);

    sig_builder.AddReturn(MachineType::Int32());
    for (int i = 0; i < num_params + 1; i++) {
      sig_builder.AddParam(MachineType::Pointer());
    }
    signature_ = sig_builder.Build();
  }

  void Init(CallDescriptor* descriptor, MachineType return_type,
            Vector<MachineType> param_types) {
    DCHECK_NOT_NULL(descriptor);
    DCHECK_EQ(signature_->parameter_count(), param_types.length() + 1);

    // Create the TF graph for the wrapper.

    // Function, effect, and control.
    Node** parameters = zone()->NewArray<Node*>(param_types.length() + 3);
    graph()->SetStart(graph()->NewNode(common()->Start(6)));
    Node* effect = graph()->start();
    int parameter_count = 0;

    // Dummy node which gets replaced in SetInnerCode.
    inner_code_node_ = graph()->NewNode(common()->Int32Constant(0));
    parameters[parameter_count++] = inner_code_node_;

    int param_idx = 0;
    for (MachineType t : param_types) {
      DCHECK_NE(MachineType::None(), t);
      parameters[parameter_count] = graph()->NewNode(
          machine()->Load(t),
          graph()->NewNode(common()->Parameter(param_idx++), graph()->start()),
          graph()->NewNode(common()->Int32Constant(0)), effect,
          graph()->start());
      effect = parameters[parameter_count++];
    }

    parameters[parameter_count++] = effect;
    parameters[parameter_count++] = graph()->start();
    Node* call = graph()->NewNode(common()->Call(descriptor), parameter_count,
                                  parameters);

    if (!return_type.IsNone()) {
      effect = graph()->NewNode(
          machine()->Store(StoreRepresentation(
              return_type.representation(), WriteBarrierKind::kNoWriteBarrier)),
          graph()->NewNode(common()->Parameter(param_types.length()),
                           graph()->start()),
          graph()->NewNode(common()->Int32Constant(0)), call, effect,
          graph()->start());
    }
    Node* zero = graph()->NewNode(common()->Int32Constant(0));
    Node* r = graph()->NewNode(
        common()->Return(), zero,
        graph()->NewNode(common()->Int32Constant(WASM_WRAPPER_RETURN_VALUE)),
        effect, graph()->start());
    graph()->SetEnd(graph()->NewNode(common()->End(2), r, graph()->start()));
  }

  template <typename ReturnType, typename... ParamTypes>
  void Init(CallDescriptor* descriptor) {
    std::array<MachineType, sizeof...(ParamTypes)> param_machine_types{
        {MachineTypeForC<ParamTypes>()...}};
    Vector<MachineType> param_vec(param_machine_types.data(),
                                  param_machine_types.size());
    Init(descriptor, MachineTypeForC<ReturnType>(), param_vec);
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
        size_t num_params = signature_->parameter_count();
        // One additional parameter for the pointer of the return value.
        Signature<MachineRepresentation>::Builder rep_builder(zone(), 1,
                                                              num_params + 1);

        rep_builder.AddReturn(MachineRepresentation::kWord32);
        for (size_t i = 0; i < num_params + 1; i++) {
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

// A helper for compiling WASM functions for testing.
// It contains the internal state for compilation (i.e. TurboFan graph) and
// interpretation (by adding to the interpreter manually).
class WasmFunctionCompiler : private GraphAndBuilders {
 public:
  Isolate* isolate() { return testing_module_->isolate(); }
  Graph* graph() const { return main_graph_; }
  Zone* zone() const { return graph()->zone(); }
  CommonOperatorBuilder* common() { return &main_common_; }
  MachineOperatorBuilder* machine() { return &main_machine_; }
  CallDescriptor* descriptor() {
    if (descriptor_ == nullptr) {
      descriptor_ = testing_module_->GetWasmCallDescriptor(zone(), sig);
    }
    return descriptor_;
  }
  uint32_t function_index() { return function_->func_index; }

  void Build(const byte* start, const byte* end) {
    size_t locals_size = local_decls.Size();
    size_t total_size = end - start + locals_size + 1;
    byte* buffer = static_cast<byte*>(zone()->New(total_size));
    // Prepend the local decls to the code.
    local_decls.Emit(buffer);
    // Emit the code.
    memcpy(buffer + locals_size, start, end - start);
    // Append an extra end opcode.
    buffer[total_size - 1] = kExprEnd;

    start = buffer;
    end = buffer + total_size;

    CHECK_GE(kMaxInt, end - start);
    int len = static_cast<int>(end - start);
    function_->code_start_offset =
        testing_module_->AddBytes(Vector<const byte>(start, len));
    function_->code_end_offset = function_->code_start_offset + len;

    if (interpreter_) {
      // Add the code to the interpreter.
      CHECK(interpreter_->SetFunctionCodeForTesting(function_, start, end));
      return;
    }

    // Build the TurboFan graph.
    TestBuildingGraph(zone(), &jsgraph, testing_module_, sig,
                      &source_position_table_, start, end);
    Handle<Code> code = Compile();
    testing_module_->SetFunctionCode(function_index(), code);
  }

  byte AllocateLocal(ValueType type) {
    uint32_t index = local_decls.AddLocals(1, type);
    byte result = static_cast<byte>(index);
    DCHECK_EQ(index, result);
    return result;
  }

  void SetSigIndex(int sig_index) { function_->sig_index = sig_index; }

 private:
  friend class WasmRunnerBase;

  explicit WasmFunctionCompiler(Zone* zone, FunctionSig* sig,
                                TestingModule* module, const char* name)
      : GraphAndBuilders(zone),
        jsgraph(module->isolate(), this->graph(), this->common(), nullptr,
                nullptr, this->machine()),
        sig(sig),
        descriptor_(nullptr),
        testing_module_(module),
        local_decls(zone, sig),
        source_position_table_(this->graph()),
        interpreter_(module->interpreter()) {
    // Get a new function from the testing module.
    int index = module->AddFunction(sig, Handle<Code>::null(), name);
    function_ = testing_module_->GetFunctionAt(index);
  }

  Handle<Code> Compile() {
    CallDescriptor* desc = descriptor();
    if (kPointerSize == 4) {
      desc = testing_module_->GetI32WasmCallDescriptor(this->zone(), desc);
    }
    CompilationInfo info(CStrVector("wasm"), this->isolate(), this->zone(),
                         Code::ComputeFlags(Code::WASM_FUNCTION));
    std::unique_ptr<CompilationJob> job(Pipeline::NewWasmCompilationJob(
        &info, &jsgraph, desc, &source_position_table_, nullptr, false));
    if (job->ExecuteJob() != CompilationJob::SUCCEEDED ||
        job->FinalizeJob() != CompilationJob::SUCCEEDED)
      return Handle<Code>::null();

    Handle<Code> code = info.code();

    // Deopt data holds <WeakCell<wasm_instance>, func_index>.
    DCHECK(code->deoptimization_data() == nullptr ||
           code->deoptimization_data()->length() == 0);
    Handle<FixedArray> deopt_data =
        isolate()->factory()->NewFixedArray(2, TENURED);
    Handle<Object> weak_instance =
        isolate()->factory()->NewWeakCell(testing_module_->instance_object());
    deopt_data->set(0, *weak_instance);
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
};

// A helper class to build a module around Wasm bytecode, generate machine
// code, and run that code.
class WasmRunnerBase : public HandleAndZoneScope {
 public:
  explicit WasmRunnerBase(WasmExecutionMode execution_mode, int num_params)
      : zone_(&allocator_, ZONE_NAME),
        module_(&zone_, execution_mode),
        wrapper_(&zone_, num_params) {}

  // Builds a graph from the given Wasm code and generates the machine
  // code and call wrapper for that graph. This method must not be called
  // more than once.
  void Build(const byte* start, const byte* end) {
    CHECK(!compiled_);
    compiled_ = true;
    functions_[0]->Build(start, end);
  }

  // Resets the state for building the next function.
  // The main function called will always be the first function.
  template <typename ReturnType, typename... ParamTypes>
  WasmFunctionCompiler& NewFunction(const char* name = nullptr) {
    return NewFunction(CreateSig<ReturnType, ParamTypes...>(), name);
  }

  // Resets the state for building the next function.
  // The main function called will be the last generated function.
  // Returns the index of the previously built function.
  WasmFunctionCompiler& NewFunction(FunctionSig* sig,
                                    const char* name = nullptr) {
    functions_.emplace_back(
        new WasmFunctionCompiler(&zone_, sig, &module_, name));
    return *functions_.back();
  }

  byte AllocateLocal(ValueType type) {
    return functions_[0]->AllocateLocal(type);
  }

  WasmFunction* function() { return functions_[0]->function_; }
  WasmInterpreter* interpreter() { return functions_[0]->interpreter_; }
  bool possible_nondeterminism() { return possible_nondeterminism_; }
  TestingModule& module() { return module_; }
  Zone* zone() { return &zone_; }

  // Set the context, such that e.g. runtime functions can be called.
  void SetModuleContext() {
    if (!module_.instance->context.is_null()) {
      CHECK(module_.instance->context.is_identical_to(
          main_isolate()->native_context()));
      return;
    }
    module_.instance->context = main_isolate()->native_context();
  }

 private:
  FunctionSig* CreateSig(MachineType return_type,
                         Vector<MachineType> param_types) {
    int return_count = return_type.IsNone() ? 0 : 1;
    int param_count = param_types.length();

    // Allocate storage array in zone.
    ValueType* sig_types =
        zone_.NewArray<ValueType>(return_count + param_count);

    // Convert machine types to local types, and check that there are no
    // MachineType::None()'s in the parameters.
    int idx = 0;
    if (return_count) sig_types[idx++] = WasmOpcodes::ValueTypeFor(return_type);
    for (MachineType param : param_types) {
      CHECK_NE(MachineType::None(), param);
      sig_types[idx++] = WasmOpcodes::ValueTypeFor(param);
    }
    return new (&zone_) FunctionSig(return_count, param_count, sig_types);
  }

  template <typename ReturnType, typename... ParamTypes>
  FunctionSig* CreateSig() {
    std::array<MachineType, sizeof...(ParamTypes)> param_machine_types{
        {MachineTypeForC<ParamTypes>()...}};
    Vector<MachineType> param_vec(param_machine_types.data(),
                                  param_machine_types.size());
    return CreateSig(MachineTypeForC<ReturnType>(), param_vec);
  }

 protected:
  v8::internal::AccountingAllocator allocator_;
  Zone zone_;
  TestingModule module_;
  std::vector<std::unique_ptr<WasmFunctionCompiler>> functions_;
  WasmFunctionWrapper wrapper_;
  bool compiled_ = false;
  bool possible_nondeterminism_ = false;

  bool interpret() { return module_.execution_mode() == kExecuteInterpreted; }

 public:
  // This field has to be static. Otherwise, gcc complains about the using in
  // the lambda context below.
  static jmp_buf jump_buffer;
};

template <typename ReturnType, typename... ParamTypes>
class WasmRunner : public WasmRunnerBase {
 public:
  explicit WasmRunner(WasmExecutionMode execution_mode,
                      const char* main_fn_name = "main")
      : WasmRunnerBase(execution_mode, sizeof...(ParamTypes)) {
    NewFunction<ReturnType, ParamTypes...>(main_fn_name);
    if (!interpret()) {
      wrapper_.Init<ReturnType, ParamTypes...>(functions_[0]->descriptor());
    }
  }

  ReturnType Call(ParamTypes... p) {
    DCHECK(compiled_);
    if (interpret()) return CallInterpreter(p...);

    // Use setjmp/longjmp to deal with traps in WebAssembly code.
    ReturnType return_value = static_cast<ReturnType>(0xdeadbeefdeadbeef);
    static int setjmp_ret;
    setjmp_ret = setjmp(WasmRunnerBase::jump_buffer);
    // setjmp returns 0 on the first return, 1 (passed to longjmp) after trap.
    if (setjmp_ret == 0) {
      DoCall(static_cast<void*>(&p)..., static_cast<void*>(&return_value));
    }
    return return_value;
  }

  ReturnType CallInterpreter(ParamTypes... p) {
    WasmInterpreter::Thread* thread = interpreter()->GetThread(0);
    thread->Reset();
    std::array<WasmVal, sizeof...(p)> args{{WasmVal(p)...}};
    thread->PushFrame(function(), args.data());
    if (thread->Run() == WasmInterpreter::FINISHED) {
      WasmVal val = thread->GetReturnValue();
      possible_nondeterminism_ |= thread->PossibleNondeterminism();
      return val.to<ReturnType>();
    } else if (thread->state() == WasmInterpreter::TRAPPED) {
      // TODO(titzer): return the correct trap code
      int64_t result = 0xdeadbeefdeadbeef;
      return static_cast<ReturnType>(result);
    } else {
      // TODO(titzer): falling off end
      return ReturnType{0};
    }
  }

 private:
  // Don't inline this function. The setjmp above should be followed immediately
  // by a call.
  template <typename... Ptrs>
  V8_NOINLINE void DoCall(Ptrs... ptrs) {
    auto trap_callback = []() -> void {
      set_trap_callback_for_testing(nullptr);
      longjmp(WasmRunnerBase::jump_buffer, 1);
    };
    set_trap_callback_for_testing(trap_callback);

    wrapper_.SetInnerCode(
        module_.GetFunctionCode(functions_[0]->function_index()));
    CodeRunner<int32_t> runner(CcTest::InitIsolateOnce(),
                               wrapper_.GetWrapperCode(), wrapper_.signature());
    int32_t result = runner.Call(ptrs...);
    // If we arrive here, no trap happened.
    CHECK_EQ(WASM_WRAPPER_RETURN_VALUE, result);
  }
};

// Declare static variable.
jmp_buf WasmRunnerBase::jump_buffer;

// A macro to define tests that run in different engine configurations.
#define WASM_EXEC_TEST(name)                                               \
  void RunWasm_##name(WasmExecutionMode execution_mode);                   \
  TEST(RunWasmCompiled_##name) { RunWasm_##name(kExecuteCompiled); }       \
  TEST(RunWasmInterpreted_##name) { RunWasm_##name(kExecuteInterpreted); } \
  void RunWasm_##name(WasmExecutionMode execution_mode)

#define WASM_EXEC_TEST_WITH_TRAP(name)                                     \
  void RunWasm_##name(WasmExecutionMode execution_mode);                   \
  TEST(RunWasmCompiled_##name) { RunWasm_##name(kExecuteCompiled); }       \
  void RunWasm_##name(WasmExecutionMode execution_mode);                   \
  TEST(RunWasmCompiledWithTrapIf_##name) {                                 \
    bool trap_if = FLAG_wasm_trap_if;                                      \
    FLAG_wasm_trap_if = true;                                              \
    RunWasm_##name(kExecuteCompiled);                                      \
    FLAG_wasm_trap_if = trap_if;                                           \
  }                                                                        \
  TEST(RunWasmInterpreted_##name) { RunWasm_##name(kExecuteInterpreted); } \
  void RunWasm_##name(WasmExecutionMode execution_mode)

#define WASM_EXEC_COMPILED_TEST(name)                                \
  void RunWasm_##name(WasmExecutionMode execution_mode);             \
  TEST(RunWasmCompiled_##name) { RunWasm_##name(kExecuteCompiled); } \
  void RunWasm_##name(WasmExecutionMode execution_mode)

}  // namespace

#endif

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
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/int64-lowering.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/wasm-compiler.h"
#include "src/compiler/zone-stats.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/local-decl-encoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-external-refs.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-tier.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/call-tester.h"
#include "test/cctest/compiler/graph-and-builders.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/common/wasm/flag-utils.h"

namespace v8 {
namespace internal {
namespace wasm {

using base::ReadLittleEndianValue;
using base::WriteLittleEndianValue;

constexpr uint32_t kMaxFunctions = 10;
constexpr uint32_t kMaxGlobalsSize = 128;

using compiler::CallDescriptor;
using compiler::MachineTypeForC;
using compiler::Node;

// TODO(titzer): check traps more robustly in tests.
// Currently, in tests, we just return 0xDEADBEEF from the function in which
// the trap occurs if the runtime context is not available to throw a JavaScript
// exception.
#define CHECK_TRAP32(x) \
  CHECK_EQ(0xDEADBEEF, (bit_cast<uint32_t>(x)) & 0xFFFFFFFF)
#define CHECK_TRAP64(x) \
  CHECK_EQ(0xDEADBEEFDEADBEEF, (bit_cast<uint64_t>(x)) & 0xFFFFFFFFFFFFFFFF)
#define CHECK_TRAP(x) CHECK_TRAP32(x)

#define WASM_WRAPPER_RETURN_VALUE 8754

#define BUILD(r, ...)                      \
  do {                                     \
    byte code[] = {__VA_ARGS__};           \
    r.Build(code, code + arraysize(code)); \
  } while (false)

// For tests that must manually import a JSFunction with source code.
struct ManuallyImportedJSFunction {
  FunctionSig* sig;
  Handle<JSFunction> js_function;
};

// A  Wasm module builder. Globals are pre-set, however, memory and code may be
// progressively added by a test. In turn, we piecemeal update the runtime
// objects, i.e. {WasmInstanceObject}, {WasmModuleObject} and, if necessary,
// the interpreter.
class TestingModuleBuilder {
 public:
  TestingModuleBuilder(Zone*, ManuallyImportedJSFunction*, ExecutionTier,
                       RuntimeExceptionSupport, LowerSimd);

  void ChangeOriginToAsmjs() { test_module_->origin = kAsmJsSloppyOrigin; }

  byte* AddMemory(uint32_t size, SharedFlag shared = SharedFlag::kNotShared);

  size_t CodeTableLength() const { return native_module_->num_functions(); }

  template <typename T>
  T* AddMemoryElems(uint32_t count) {
    AddMemory(count * sizeof(T));
    return raw_mem_start<T>();
  }

  template <typename T>
  T* AddGlobal(
      ValueType type = ValueTypes::ValueTypeFor(MachineTypeForC<T>())) {
    const WasmGlobal* global = AddGlobal(type);
    return reinterpret_cast<T*>(globals_data_ + global->offset);
  }

  byte AddSignature(FunctionSig* sig) {
    DCHECK_EQ(test_module_->signatures.size(),
              test_module_->signature_ids.size());
    test_module_->signatures.push_back(sig);
    auto canonical_sig_num = test_module_->signature_map.FindOrInsert(*sig);
    test_module_->signature_ids.push_back(canonical_sig_num);
    size_t size = test_module_->signatures.size();
    CHECK_GT(127, size);
    return static_cast<byte>(size - 1);
  }

  uint32_t mem_size() { return mem_size_; }

  template <typename T>
  T* raw_mem_start() {
    DCHECK(mem_start_);
    return reinterpret_cast<T*>(mem_start_);
  }

  template <typename T>
  T* raw_mem_end() {
    DCHECK(mem_start_);
    return reinterpret_cast<T*>(mem_start_ + mem_size_);
  }

  template <typename T>
  T raw_mem_at(int i) {
    DCHECK(mem_start_);
    return ReadMemory(&(reinterpret_cast<T*>(mem_start_)[i]));
  }

  template <typename T>
  T raw_val_at(int i) {
    return ReadMemory(reinterpret_cast<T*>(mem_start_ + i));
  }

  template <typename T>
  void WriteMemory(T* p, T val) {
    WriteLittleEndianValue<T>(reinterpret_cast<Address>(p), val);
  }

  template <typename T>
  T ReadMemory(T* p) {
    return ReadLittleEndianValue<T>(reinterpret_cast<Address>(p));
  }

  // Zero-initialize the memory.
  void BlankMemory() {
    byte* raw = raw_mem_start<byte>();
    memset(raw, 0, mem_size_);
  }

  // Pseudo-randomly intialize the memory.
  void RandomizeMemory(unsigned int seed = 88) {
    byte* raw = raw_mem_start<byte>();
    byte* end = raw_mem_end<byte>();
    v8::base::RandomNumberGenerator rng;
    rng.SetSeed(seed);
    rng.NextBytes(raw, end - raw);
  }

  void SetMaxMemPages(uint32_t maximum_pages) {
    test_module_->maximum_pages = maximum_pages;
    if (instance_object()->has_memory_object()) {
      instance_object()->memory_object().set_maximum_pages(maximum_pages);
    }
  }

  void SetHasSharedMemory() { test_module_->has_shared_memory = true; }

  enum FunctionType { kImport, kWasm };
  uint32_t AddFunction(FunctionSig* sig, const char* name, FunctionType type);

  // Freezes the signature map of the module and allocates the storage for
  // export wrappers.
  void FreezeSignatureMapAndInitializeWrapperCache();

  // Wrap the code so it can be called as a JS function.
  Handle<JSFunction> WrapCode(uint32_t index);

  // If function_indexes is {nullptr}, the contents of the table will be
  // initialized with null functions.
  void AddIndirectFunctionTable(const uint16_t* function_indexes,
                                uint32_t table_size);

  uint32_t AddBytes(Vector<const byte> bytes);

  uint32_t AddException(FunctionSig* sig);

  uint32_t AddPassiveDataSegment(Vector<const byte> bytes);
  uint32_t AddPassiveElementSegment(const std::vector<uint32_t>& entries);

  WasmFunction* GetFunctionAt(int index) {
    return &test_module_->functions[index];
  }

  WasmInterpreter* interpreter() const { return interpreter_; }
  bool interpret() const { return interpreter_ != nullptr; }
  LowerSimd lower_simd() const { return lower_simd_; }
  Isolate* isolate() const { return isolate_; }
  Handle<WasmInstanceObject> instance_object() const {
    return instance_object_;
  }
  WasmCode* GetFunctionCode(uint32_t index) const {
    return native_module_->GetCode(index);
  }
  Address globals_start() const {
    return reinterpret_cast<Address>(globals_data_);
  }

  void SetExecutable() { native_module_->SetExecutable(true); }

  CompilationEnv CreateCompilationEnv();

  ExecutionTier execution_tier() const { return execution_tier_; }

  RuntimeExceptionSupport runtime_exception_support() const {
    return runtime_exception_support_;
  }

 private:
  std::shared_ptr<WasmModule> test_module_;
  WasmModule* test_module_ptr_;
  Isolate* isolate_;
  WasmFeatures enabled_features_;
  uint32_t global_offset = 0;
  byte* mem_start_ = nullptr;
  uint32_t mem_size_ = 0;
  alignas(16) byte globals_data_[kMaxGlobalsSize];
  WasmInterpreter* interpreter_ = nullptr;
  ExecutionTier execution_tier_;
  Handle<WasmInstanceObject> instance_object_;
  NativeModule* native_module_ = nullptr;
  RuntimeExceptionSupport runtime_exception_support_;
  LowerSimd lower_simd_;

  // Data segment arrays that are normally allocated on the instance.
  std::vector<byte> data_segment_data_;
  std::vector<Address> data_segment_starts_;
  std::vector<uint32_t> data_segment_sizes_;
  std::vector<byte> dropped_data_segments_;
  std::vector<byte> dropped_elem_segments_;

  const WasmGlobal* AddGlobal(ValueType type);

  Handle<WasmInstanceObject> InitInstanceObject();
};

void TestBuildingGraph(Zone* zone, compiler::JSGraph* jsgraph,
                       CompilationEnv* module, FunctionSig* sig,
                       compiler::SourcePositionTable* source_position_table,
                       const byte* start, const byte* end);

class WasmFunctionWrapper : private compiler::GraphAndBuilders {
 public:
  WasmFunctionWrapper(Zone* zone, int num_params);

  void Init(CallDescriptor* call_descriptor, MachineType return_type,
            Vector<MachineType> param_types);

  template <typename ReturnType, typename... ParamTypes>
  void Init(CallDescriptor* call_descriptor) {
    std::array<MachineType, sizeof...(ParamTypes)> param_machine_types{
        {MachineTypeForC<ParamTypes>()...}};
    Vector<MachineType> param_vec(param_machine_types.data(),
                                  param_machine_types.size());
    Init(call_descriptor, MachineTypeForC<ReturnType>(), param_vec);
  }

  void SetInnerCode(WasmCode* code) {
    intptr_t address = static_cast<intptr_t>(code->instruction_start());
    compiler::NodeProperties::ChangeOp(
        inner_code_node_,
        common()->ExternalConstant(ExternalReference::FromRawAddress(address)));
  }

  const compiler::Operator* IntPtrConstant(intptr_t value) {
    return machine()->Is32()
               ? common()->Int32Constant(static_cast<int32_t>(value))
               : common()->Int64Constant(static_cast<int64_t>(value));
  }

  void SetInstance(Handle<WasmInstanceObject> instance) {
    compiler::NodeProperties::ChangeOp(context_address_,
                                       common()->HeapConstant(instance));
  }

  Handle<Code> GetWrapperCode();

  Signature<MachineType>* signature() const { return signature_; }

 private:
  Node* inner_code_node_;
  Node* context_address_;
  MaybeHandle<Code> code_;
  Signature<MachineType>* signature_;
};

// A helper for compiling wasm functions for testing.
// It contains the internal state for compilation (i.e. TurboFan graph) and
// interpretation (by adding to the interpreter manually).
class WasmFunctionCompiler : public compiler::GraphAndBuilders {
 public:
  ~WasmFunctionCompiler();

  Isolate* isolate() { return builder_->isolate(); }
  CallDescriptor* descriptor() {
    if (descriptor_ == nullptr) {
      descriptor_ = compiler::GetWasmCallDescriptor(zone(), sig);
    }
    return descriptor_;
  }
  uint32_t function_index() { return function_->func_index; }

  void Build(const byte* start, const byte* end);

  byte AllocateLocal(ValueType type) {
    uint32_t index = local_decls.AddLocals(1, type);
    byte result = static_cast<byte>(index);
    DCHECK_EQ(index, result);
    return result;
  }

  void SetSigIndex(int sig_index) { function_->sig_index = sig_index; }

 private:
  friend class WasmRunnerBase;

  WasmFunctionCompiler(Zone* zone, FunctionSig* sig,
                       TestingModuleBuilder* builder, const char* name);

  compiler::JSGraph jsgraph;
  FunctionSig* sig;
  // The call descriptor is initialized when the function is compiled.
  CallDescriptor* descriptor_;
  TestingModuleBuilder* builder_;
  WasmFunction* function_;
  LocalDeclEncoder local_decls;
  compiler::SourcePositionTable source_position_table_;
  WasmInterpreter* interpreter_;
};

// A helper class to build a module around Wasm bytecode, generate machine
// code, and run that code.
class WasmRunnerBase : public HandleAndZoneScope {
 public:
  WasmRunnerBase(ManuallyImportedJSFunction* maybe_import,
                 ExecutionTier execution_tier, int num_params,
                 RuntimeExceptionSupport runtime_exception_support,
                 LowerSimd lower_simd)
      : zone_(&allocator_, ZONE_NAME),
        builder_(&zone_, maybe_import, execution_tier,
                 runtime_exception_support, lower_simd),
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
        new WasmFunctionCompiler(&zone_, sig, &builder_, name));
    builder().AddSignature(sig);
    return *functions_.back();
  }

  byte AllocateLocal(ValueType type) {
    return functions_[0]->AllocateLocal(type);
  }

  uint32_t function_index() { return functions_[0]->function_index(); }
  WasmFunction* function() { return functions_[0]->function_; }
  WasmInterpreter* interpreter() {
    DCHECK(interpret());
    return functions_[0]->interpreter_;
  }
  bool possible_nondeterminism() { return possible_nondeterminism_; }
  TestingModuleBuilder& builder() { return builder_; }
  Zone* zone() { return &zone_; }

  bool interpret() { return builder_.interpret(); }

  template <typename ReturnType, typename... ParamTypes>
  FunctionSig* CreateSig() {
    std::array<MachineType, sizeof...(ParamTypes)> param_machine_types{
        {MachineTypeForC<ParamTypes>()...}};
    Vector<MachineType> param_vec(param_machine_types.data(),
                                  param_machine_types.size());
    return CreateSig(MachineTypeForC<ReturnType>(), param_vec);
  }

 private:
  FunctionSig* CreateSig(MachineType return_type,
                         Vector<MachineType> param_types);

 protected:
  v8::internal::AccountingAllocator allocator_;
  Zone zone_;
  TestingModuleBuilder builder_;
  std::vector<std::unique_ptr<WasmFunctionCompiler>> functions_;
  WasmFunctionWrapper wrapper_;
  bool compiled_ = false;
  bool possible_nondeterminism_ = false;
  int32_t main_fn_index_ = 0;

 public:
  // This field has to be static. Otherwise, gcc complains about the use in
  // the lambda context below.
  static bool trap_happened;
};

template <typename ReturnType, typename... ParamTypes>
class WasmRunner : public WasmRunnerBase {
 public:
  WasmRunner(ExecutionTier execution_tier,
             ManuallyImportedJSFunction* maybe_import = nullptr,
             const char* main_fn_name = "main",
             RuntimeExceptionSupport runtime_exception_support =
                 kNoRuntimeExceptionSupport,
             LowerSimd lower_simd = kNoLowerSimd)
      : WasmRunnerBase(maybe_import, execution_tier, sizeof...(ParamTypes),
                       runtime_exception_support, lower_simd) {
    WasmFunctionCompiler& main_fn =
        NewFunction<ReturnType, ParamTypes...>(main_fn_name);
    // Non-zero if there is an import.
    main_fn_index_ = main_fn.function_index();

    if (!interpret()) {
      wrapper_.Init<ReturnType, ParamTypes...>(main_fn.descriptor());
    }
  }

  WasmRunner(ExecutionTier execution_tier, LowerSimd lower_simd)
      : WasmRunner(execution_tier, nullptr, "main", kNoRuntimeExceptionSupport,
                   lower_simd) {}

  void SetUpTrapCallback() {
    WasmRunnerBase::trap_happened = false;
    auto trap_callback = []() -> void {
      WasmRunnerBase::trap_happened = true;
      set_trap_callback_for_testing(nullptr);
    };
    set_trap_callback_for_testing(trap_callback);
  }

  ReturnType Call(ParamTypes... p) {
    DCHECK(compiled_);
    if (interpret()) return CallInterpreter(p...);

    ReturnType return_value = static_cast<ReturnType>(0xDEADBEEFDEADBEEF);
    SetUpTrapCallback();

    wrapper_.SetInnerCode(builder_.GetFunctionCode(main_fn_index_));
    wrapper_.SetInstance(builder_.instance_object());
    builder_.SetExecutable();
    Handle<Code> wrapper_code = wrapper_.GetWrapperCode();
    compiler::CodeRunner<int32_t> runner(CcTest::InitIsolateOnce(),
                                         wrapper_code, wrapper_.signature());
    int32_t result;
    {
      SetThreadInWasmFlag();

      result = runner.Call(static_cast<void*>(&p)...,
                           static_cast<void*>(&return_value));

      ClearThreadInWasmFlag();
    }
    CHECK_EQ(WASM_WRAPPER_RETURN_VALUE, result);
    return WasmRunnerBase::trap_happened
               ? static_cast<ReturnType>(0xDEADBEEFDEADBEEF)
               : return_value;
  }

  ReturnType CallInterpreter(ParamTypes... p) {
    WasmInterpreter::Thread* thread = interpreter()->GetThread(0);
    thread->Reset();
    std::array<WasmValue, sizeof...(p)> args{{WasmValue(p)...}};
    thread->InitFrame(function(), args.data());
    thread->Run();
    CHECK_GT(thread->NumInterpretedCalls(), 0);
    if (thread->state() == WasmInterpreter::FINISHED) {
      WasmValue val = thread->GetReturnValue();
      possible_nondeterminism_ |= thread->PossibleNondeterminism();
      return val.to<ReturnType>();
    } else if (thread->state() == WasmInterpreter::TRAPPED) {
      // TODO(titzer): return the correct trap code
      int64_t result = 0xDEADBEEFDEADBEEF;
      return static_cast<ReturnType>(result);
    } else {
      // TODO(titzer): falling off end
      return ReturnType{0};
    }
  }

  void CheckCallApplyViaJS(double expected, uint32_t function_index,
                           Handle<Object>* buffer, int count) {
    Isolate* isolate = builder_.isolate();
    SetUpTrapCallback();
    if (jsfuncs_.size() <= function_index) {
      jsfuncs_.resize(function_index + 1);
    }
    if (jsfuncs_[function_index].is_null()) {
      jsfuncs_[function_index] = builder_.WrapCode(function_index);
    }
    Handle<JSFunction> jsfunc = jsfuncs_[function_index];
    Handle<Object> global(isolate->context().global_object(), isolate);
    MaybeHandle<Object> retval =
        Execution::TryCall(isolate, jsfunc, global, count, buffer,
                           Execution::MessageHandling::kReport, nullptr);

    if (retval.is_null() || WasmRunnerBase::trap_happened) {
      CHECK_EQ(expected, static_cast<double>(0xDEADBEEF));
    } else {
      Handle<Object> result = retval.ToHandleChecked();
      if (result->IsSmi()) {
        CHECK_EQ(expected, Smi::ToInt(*result));
      } else {
        CHECK(result->IsHeapNumber());
        CHECK_DOUBLE_EQ(expected, HeapNumber::cast(*result).value());
      }
    }

    if (builder_.interpret()) {
      CHECK_GT(builder_.interpreter()->GetThread(0)->NumInterpretedCalls(), 0);
    }
  }

  void CheckCallViaJS(double expected, ParamTypes... p) {
    Isolate* isolate = builder_.isolate();
    // MSVC doesn't allow empty arrays, so include a dummy at the end.
    Handle<Object> buffer[] = {isolate->factory()->NewNumber(p)...,
                               Handle<Object>()};
    CheckCallApplyViaJS(expected, function()->func_index, buffer, sizeof...(p));
  }

  Handle<Code> GetWrapperCode() { return wrapper_.GetWrapperCode(); }

 private:
  wasm::WasmCodeRefScope code_ref_scope_;
  std::vector<Handle<JSFunction>> jsfuncs_;

  void SetThreadInWasmFlag() {
    *reinterpret_cast<int*>(trap_handler::GetThreadInWasmThreadLocalAddress()) =
        true;
  }

  void ClearThreadInWasmFlag() {
    *reinterpret_cast<int*>(trap_handler::GetThreadInWasmThreadLocalAddress()) =
        false;
  }
};

// A macro to define tests that run in different engine configurations.
#define WASM_EXEC_TEST(name)                                                 \
  void RunWasm_##name(ExecutionTier execution_tier);                         \
  TEST(RunWasmTurbofan_##name) { RunWasm_##name(ExecutionTier::kTurbofan); } \
  TEST(RunWasmLiftoff_##name) { RunWasm_##name(ExecutionTier::kLiftoff); }   \
  TEST(RunWasmInterpreter_##name) {                                          \
    RunWasm_##name(ExecutionTier::kInterpreter);                             \
  }                                                                          \
  void RunWasm_##name(ExecutionTier execution_tier)

#define WASM_COMPILED_EXEC_TEST(name)                                        \
  void RunWasm_##name(ExecutionTier execution_tier);                         \
  TEST(RunWasmTurbofan_##name) { RunWasm_##name(ExecutionTier::kTurbofan); } \
  TEST(RunWasmLiftoff_##name) { RunWasm_##name(ExecutionTier::kLiftoff); }   \
  void RunWasm_##name(ExecutionTier execution_tier)

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif

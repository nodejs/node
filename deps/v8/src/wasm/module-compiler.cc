// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-compiler.h"

#include "src/api.h"
#include "src/asmjs/asm-js.h"
#include "src/base/optional.h"
#include "src/base/template-utils.h"
#include "src/base/utils/random-number-generator.h"
#include "src/compiler/wasm-compiler.h"
#include "src/counters.h"
#include "src/identity-map.h"
#include "src/property-descriptor.h"
#include "src/tracing/trace-event.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-memory.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_wasm_instances) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_COMPILE(...)                             \
  do {                                                 \
    if (FLAG_trace_wasm_compiler) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_STREAMING(...)                            \
  do {                                                  \
    if (FLAG_trace_wasm_streaming) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_LAZY(...)                                        \
  do {                                                         \
    if (FLAG_trace_wasm_lazy_compilation) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {
namespace wasm {

enum class CompilationEvent : uint8_t {
  kFinishedBaselineCompilation,
  kFinishedTopTierCompilation,
  kFailedCompilation
};

enum class CompileMode : uint8_t { kRegular, kTiering };

// The CompilationState keeps track of the compilation state of the
// owning NativeModule, i.e. which functions are left to be compiled.
// It contains a task manager to allow parallel and asynchronous background
// compilation of functions.
class CompilationState {
 public:
  CompilationState(internal::Isolate*, const ModuleEnv&);
  ~CompilationState();

  // Set the number of compilations unit expected to be executed. Needs to be
  // set before {AddCompilationUnits} is run, which triggers background
  // compilation.
  void SetNumberOfFunctionsToCompile(size_t num_functions);

  // Set the callback function to be called on compilation events. Needs to be
  // set before {AddCompilationUnits} is run.
  void SetCallback(
      std::function<void(CompilationEvent, ErrorThrower*)> callback);

  // Inserts new functions to compile and kicks off compilation.
  void AddCompilationUnits(
      std::vector<std::unique_ptr<WasmCompilationUnit>>& baseline_units,
      std::vector<std::unique_ptr<WasmCompilationUnit>>& tiering_units);
  std::unique_ptr<WasmCompilationUnit> GetNextCompilationUnit();
  std::unique_ptr<WasmCompilationUnit> GetNextExecutedUnit();

  bool HasCompilationUnitToFinish();

  void OnError(ErrorThrower* thrower);
  void OnFinishedUnit();
  void ScheduleUnitForFinishing(std::unique_ptr<WasmCompilationUnit> unit,
                                ExecutionTier mode);

  void OnBackgroundTaskStopped(const WasmFeatures& detected);
  void PublishDetectedFeatures(Isolate* isolate, const WasmFeatures& detected);
  void RestartBackgroundTasks(size_t max = std::numeric_limits<size_t>::max());
  // Only one foreground thread (finisher) is allowed to run at a time.
  // {SetFinisherIsRunning} returns whether the flag changed its state.
  bool SetFinisherIsRunning(bool value);
  void ScheduleFinisherTask();

  void Abort();

  Isolate* isolate() const { return isolate_; }

  bool failed() const {
    base::LockGuard<base::Mutex> guard(&mutex_);
    return failed_;
  }

  bool baseline_compilation_finished() const {
    return baseline_compilation_finished_;
  }

  WasmEngine* wasm_engine() const { return wasm_engine_; }
  CompileMode compile_mode() const { return compile_mode_; }
  ModuleEnv* module_env() { return &module_env_; }
  WasmFeatures* detected_features() { return &detected_features_; }

 private:
  void NotifyOnEvent(CompilationEvent event, ErrorThrower* thrower);

  std::vector<std::unique_ptr<WasmCompilationUnit>>& finish_units() {
    return baseline_compilation_finished_ ? tiering_finish_units_
                                          : baseline_finish_units_;
  }

  // TODO(7423): Get rid of the Isolate field to make sure the CompilationState
  // can be shared across multiple Isolates.
  Isolate* const isolate_;
  WasmEngine* const wasm_engine_;
  // TODO(clemensh): Remove ModuleEnv, generate it when needed.
  ModuleEnv module_env_;
  const CompileMode compile_mode_;
  bool baseline_compilation_finished_ = false;

  // This mutex protects all information of this CompilationState which is being
  // accessed concurrently.
  mutable base::Mutex mutex_;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {mutex_}:

  std::vector<std::unique_ptr<WasmCompilationUnit>> baseline_compilation_units_;
  std::vector<std::unique_ptr<WasmCompilationUnit>> tiering_compilation_units_;

  bool finisher_is_running_ = false;
  bool failed_ = false;
  size_t num_background_tasks_ = 0;

  std::vector<std::unique_ptr<WasmCompilationUnit>> baseline_finish_units_;
  std::vector<std::unique_ptr<WasmCompilationUnit>> tiering_finish_units_;

  // Features detected to be used in this module. Features can be detected
  // as a module is being compiled.
  WasmFeatures detected_features_ = kNoWasmFeatures;

  // End of fields protected by {mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  // Callback function to be called on compilation events.
  std::function<void(CompilationEvent, ErrorThrower*)> callback_;

  CancelableTaskManager background_task_manager_;
  CancelableTaskManager foreground_task_manager_;
  std::shared_ptr<v8::TaskRunner> foreground_task_runner_;

  const size_t max_background_tasks_ = 0;

  size_t outstanding_units_ = 0;
  size_t num_tiering_units_ = 0;
};

namespace {

void UpdateFeatureUseCounts(Isolate* isolate, const WasmFeatures& detected) {
  if (detected.threads) {
    isolate->CountUsage(v8::Isolate::UseCounterFeature::kWasmThreadOpcodes);
  }
}

class JSToWasmWrapperCache {
 public:
  Handle<Code> GetOrCompileJSToWasmWrapper(Isolate* isolate, FunctionSig* sig,
                                           bool is_import) {
    std::pair<bool, FunctionSig> key(is_import, *sig);
    Handle<Code>& cached = cache_[key];
    if (cached.is_null()) {
      cached = compiler::CompileJSToWasmWrapper(isolate, sig, is_import)
                   .ToHandleChecked();
    }
    return cached;
  }

 private:
  // We generate different code for calling imports than calling wasm functions
  // in this module. Both are cached separately.
  using CacheKey = std::pair<bool, FunctionSig>;
  std::unordered_map<CacheKey, Handle<Code>, base::hash<CacheKey>> cache_;
};

// A helper class to simplify instantiating a module from a module object.
// It closes over the {Isolate}, the {ErrorThrower}, etc.
class InstanceBuilder {
 public:
  InstanceBuilder(Isolate* isolate, ErrorThrower* thrower,
                  Handle<WasmModuleObject> module_object,
                  MaybeHandle<JSReceiver> ffi,
                  MaybeHandle<JSArrayBuffer> memory);

  // Build an instance, in all of its glory.
  MaybeHandle<WasmInstanceObject> Build();
  // Run the start function, if any.
  bool ExecuteStartFunction();

 private:
  // Represents the initialized state of a table.
  struct TableInstance {
    Handle<WasmTableObject> table_object;  // WebAssembly.Table instance
    Handle<FixedArray> js_wrappers;        // JSFunctions exported
    size_t table_size;
  };

  // A pre-evaluated value to use in import binding.
  struct SanitizedImport {
    Handle<String> module_name;
    Handle<String> import_name;
    Handle<Object> value;
  };

  Isolate* isolate_;
  const WasmFeatures enabled_;
  const WasmModule* const module_;
  ErrorThrower* thrower_;
  Handle<WasmModuleObject> module_object_;
  MaybeHandle<JSReceiver> ffi_;
  MaybeHandle<JSArrayBuffer> memory_;
  Handle<JSArrayBuffer> globals_;
  std::vector<TableInstance> table_instances_;
  std::vector<Handle<JSFunction>> js_wrappers_;
  std::vector<Handle<WasmExceptionObject>> exception_wrappers_;
  Handle<WasmExportedFunction> start_function_;
  JSToWasmWrapperCache js_to_wasm_cache_;
  std::vector<SanitizedImport> sanitized_imports_;

  UseTrapHandler use_trap_handler() const {
    return module_object_->native_module()->use_trap_handler() ? kUseTrapHandler
                                                               : kNoTrapHandler;
  }

// Helper routines to print out errors with imports.
#define ERROR_THROWER_WITH_MESSAGE(TYPE)                                      \
  void Report##TYPE(const char* error, uint32_t index,                        \
                    Handle<String> module_name, Handle<String> import_name) { \
    thrower_->TYPE("Import #%d module=\"%s\" function=\"%s\" error: %s",      \
                   index, module_name->ToCString().get(),                     \
                   import_name->ToCString().get(), error);                    \
  }                                                                           \
                                                                              \
  MaybeHandle<Object> Report##TYPE(const char* error, uint32_t index,         \
                                   Handle<String> module_name) {              \
    thrower_->TYPE("Import #%d module=\"%s\" error: %s", index,               \
                   module_name->ToCString().get(), error);                    \
    return MaybeHandle<Object>();                                             \
  }

  ERROR_THROWER_WITH_MESSAGE(LinkError)
  ERROR_THROWER_WITH_MESSAGE(TypeError)

#undef ERROR_THROWER_WITH_MESSAGE

  // Look up an import value in the {ffi_} object.
  MaybeHandle<Object> LookupImport(uint32_t index, Handle<String> module_name,
                                   Handle<String> import_name);

  // Look up an import value in the {ffi_} object specifically for linking an
  // asm.js module. This only performs non-observable lookups, which allows
  // falling back to JavaScript proper (and hence re-executing all lookups) if
  // module instantiation fails.
  MaybeHandle<Object> LookupImportAsm(uint32_t index,
                                      Handle<String> import_name);

  uint32_t EvalUint32InitExpr(const WasmInitExpr& expr);

  // Load data segments into the memory.
  void LoadDataSegments(Handle<WasmInstanceObject> instance);

  void WriteGlobalValue(const WasmGlobal& global, double value);
  void WriteGlobalValue(const WasmGlobal& global,
                        Handle<WasmGlobalObject> value);

  void SanitizeImports();

  // Find the imported memory buffer if there is one. This is used to see if we
  // need to recompile with bounds checks before creating the instance.
  MaybeHandle<JSArrayBuffer> FindImportedMemoryBuffer() const;

  // Process the imports, including functions, tables, globals, and memory, in
  // order, loading them from the {ffi_} object. Returns the number of imported
  // functions.
  int ProcessImports(Handle<WasmInstanceObject> instance);

  template <typename T>
  T* GetRawGlobalPtr(const WasmGlobal& global);

  // Process initialization of globals.
  void InitGlobals();

  // Allocate memory for a module instance as a new JSArrayBuffer.
  Handle<JSArrayBuffer> AllocateMemory(uint32_t num_pages);

  bool NeedsWrappers() const;

  // Process the exports, creating wrappers for functions, tables, memories,
  // and globals.
  void ProcessExports(Handle<WasmInstanceObject> instance);

  void InitializeTables(Handle<WasmInstanceObject> instance);

  void LoadTableSegments(Handle<WasmInstanceObject> instance);

  // Creates new exception tags for all exceptions. Note that some tags might
  // already exist if they were imported, those tags will be re-used.
  void InitializeExceptions(Handle<WasmInstanceObject> instance);
};

}  // namespace

MaybeHandle<WasmInstanceObject> InstantiateToInstanceObject(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory) {
  InstanceBuilder builder(isolate, thrower, module_object, imports, memory);
  auto instance = builder.Build();
  if (!instance.is_null() && builder.ExecuteStartFunction()) {
    return instance;
  }
  DCHECK(isolate->has_pending_exception() || thrower->error());
  return {};
}

WasmCode* LazyCompileFunction(Isolate* isolate, NativeModule* native_module,
                              int func_index) {
  base::ElapsedTimer compilation_timer;
  DCHECK(!native_module->has_code(static_cast<uint32_t>(func_index)));

  compilation_timer.Start();

  ModuleEnv* module_env = native_module->compilation_state()->module_env();

  TRACE_LAZY("Compiling wasm-function#%d.\n", func_index);

  const uint8_t* module_start = native_module->wire_bytes().start();

  const WasmFunction* func = &module_env->module->functions[func_index];
  FunctionBody body{func->sig, func->code.offset(),
                    module_start + func->code.offset(),
                    module_start + func->code.end_offset()};

  ErrorThrower thrower(isolate, "WasmLazyCompile");
  WasmCompilationUnit unit(isolate->wasm_engine(), module_env, native_module,
                           body, func_index, isolate->counters());
  unit.ExecuteCompilation(
      native_module->compilation_state()->detected_features());
  WasmCode* wasm_code = unit.FinishCompilation(&thrower);

  if (WasmCode::ShouldBeLogged(isolate)) wasm_code->LogCode(isolate);

  // If there is a pending error, something really went wrong. The module was
  // verified before starting execution with lazy compilation.
  // This might be OOM, but then we cannot continue execution anyway.
  // TODO(clemensh): According to the spec, we can actually skip validation at
  // module creation time, and return a function that always traps here.
  CHECK(!thrower.error());

  int64_t func_size =
      static_cast<int64_t>(func->code.end_offset() - func->code.offset());
  int64_t compilation_time = compilation_timer.Elapsed().InMicroseconds();

  auto counters = isolate->counters();
  counters->wasm_lazily_compiled_functions()->Increment();

  counters->wasm_lazy_compilation_throughput()->AddSample(
      compilation_time != 0 ? static_cast<int>(func_size / compilation_time)
                            : 0);

  return wasm_code;
}

Address CompileLazy(Isolate* isolate, NativeModule* native_module,
                    uint32_t func_index) {
  HistogramTimerScope lazy_time_scope(
      isolate->counters()->wasm_lazy_compilation_time());

  DCHECK(!native_module->lazy_compile_frozen());

  NativeModuleModificationScope native_module_modification_scope(native_module);

  WasmCode* result = LazyCompileFunction(isolate, native_module, func_index);
  DCHECK_NOT_NULL(result);
  DCHECK_EQ(func_index, result->index());

  return result->instruction_start();
}

// The CompilationUnitBuilder builds compilation units and stores them in an
// internal buffer. The buffer is moved into the working queue of the
// CompilationState when {Commit} is called.
class CompilationUnitBuilder {
 public:
  explicit CompilationUnitBuilder(NativeModule* native_module)
      : native_module_(native_module),
        compilation_state_(native_module->compilation_state()) {}

  void AddUnit(const WasmFunction* function, uint32_t buffer_offset,
               Vector<const uint8_t> bytes) {
    switch (compilation_state_->compile_mode()) {
      case CompileMode::kTiering:
        tiering_units_.emplace_back(CreateUnit(function, buffer_offset, bytes,
                                               ExecutionTier::kOptimized));
        baseline_units_.emplace_back(CreateUnit(function, buffer_offset, bytes,
                                                ExecutionTier::kBaseline));
        return;
      case CompileMode::kRegular:
        baseline_units_.emplace_back(
            CreateUnit(function, buffer_offset, bytes,
                       WasmCompilationUnit::GetDefaultExecutionTier()));
        return;
    }
    UNREACHABLE();
  }

  bool Commit() {
    if (baseline_units_.empty() && tiering_units_.empty()) return false;
    compilation_state_->AddCompilationUnits(baseline_units_, tiering_units_);
    Clear();
    return true;
  }

  void Clear() {
    baseline_units_.clear();
    tiering_units_.clear();
  }

 private:
  std::unique_ptr<WasmCompilationUnit> CreateUnit(const WasmFunction* function,
                                                  uint32_t buffer_offset,
                                                  Vector<const uint8_t> bytes,
                                                  ExecutionTier mode) {
    return base::make_unique<WasmCompilationUnit>(
        compilation_state_->wasm_engine(), compilation_state_->module_env(),
        native_module_,
        FunctionBody{function->sig, buffer_offset, bytes.begin(), bytes.end()},
        function->func_index,
        compilation_state_->isolate()->async_counters().get(), mode);
  }

  NativeModule* native_module_;
  CompilationState* compilation_state_;
  std::vector<std::unique_ptr<WasmCompilationUnit>> baseline_units_;
  std::vector<std::unique_ptr<WasmCompilationUnit>> tiering_units_;
};

namespace {
bool compile_lazy(const WasmModule* module) {
  return FLAG_wasm_lazy_compilation ||
         (FLAG_asm_wasm_lazy_compilation && module->origin == kAsmJsOrigin);
}

byte* raw_buffer_ptr(MaybeHandle<JSArrayBuffer> buffer, int offset) {
  return static_cast<byte*>(buffer.ToHandleChecked()->backing_store()) + offset;
}

void RecordStats(const Code* code, Counters* counters) {
  counters->wasm_generated_code_size()->Increment(code->body_size());
  counters->wasm_reloc_size()->Increment(code->relocation_info()->length());
}

bool in_bounds(uint32_t offset, size_t size, size_t upper) {
  return offset + size <= upper && offset + size >= offset;
}

using WasmInstanceMap =
    IdentityMap<Handle<WasmInstanceObject>, FreeStoreAllocationPolicy>;

double MonotonicallyIncreasingTimeInMs() {
  return V8::GetCurrentPlatform()->MonotonicallyIncreasingTime() *
         base::Time::kMillisecondsPerSecond;
}

ModuleEnv CreateDefaultModuleEnv(const WasmModule* module,
                                 bool allow_trap_handler = true) {
  UseTrapHandler use_trap_handler =
      trap_handler::IsTrapHandlerEnabled() && allow_trap_handler
          ? kUseTrapHandler
          : kNoTrapHandler;
  return ModuleEnv(module, use_trap_handler, kRuntimeExceptionSupport);
}

// Run by each compilation task and by the main thread (i.e. in both
// foreground and background threads). The no_finisher_callback is called
// within the result_mutex_ lock when no finishing task is running, i.e. when
// the finisher_is_running_ flag is not set.
bool FetchAndExecuteCompilationUnit(CompilationState* compilation_state,
                                    WasmFeatures* detected) {
  DisallowHeapAccess no_heap_access;

  std::unique_ptr<WasmCompilationUnit> unit =
      compilation_state->GetNextCompilationUnit();
  if (unit == nullptr) return false;

  // TODO(kimanh): We need to find out in which mode the unit
  // should be compiled in before compiling it, as it might fallback
  // to Turbofan if it cannot be compiled using Liftoff. This can be removed
  // later as soon as Liftoff can compile any function. Then, we can directly
  // access {unit->mode()} within {ScheduleUnitForFinishing()}.
  ExecutionTier mode = unit->mode();
  unit->ExecuteCompilation(detected);
  compilation_state->ScheduleUnitForFinishing(std::move(unit), mode);

  return true;
}

void InitializeCompilationUnits(NativeModule* native_module) {
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  const WasmModule* module = native_module->module();
  CompilationUnitBuilder builder(native_module);
  uint32_t start = module->num_imported_functions;
  uint32_t end = start + module->num_declared_functions;
  for (uint32_t i = start; i < end; ++i) {
    const WasmFunction* func = &module->functions[i];
    uint32_t buffer_offset = func->code.offset();
    Vector<const uint8_t> bytes(wire_bytes.start() + func->code.offset(),
                                func->code.end_offset() - func->code.offset());

    DCHECK_NOT_NULL(native_module);
    builder.AddUnit(func, buffer_offset, bytes);
  }
  builder.Commit();
}

void FinishCompilationUnits(CompilationState* compilation_state,
                            ErrorThrower* thrower) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "FinishCompilationUnits");
  while (true) {
    if (compilation_state->failed()) break;
    std::unique_ptr<WasmCompilationUnit> unit =
        compilation_state->GetNextExecutedUnit();
    if (unit == nullptr) break;
    WasmCode* result = unit->FinishCompilation(thrower);

    if (thrower->error()) {
      compilation_state->Abort();
      break;
    }

    // Update the compilation state.
    compilation_state->OnFinishedUnit();
    DCHECK_IMPLIES(result == nullptr, thrower->error());
    if (result == nullptr) break;
  }
  if (!compilation_state->failed()) {
    compilation_state->RestartBackgroundTasks();
  }
}

void CompileInParallel(Isolate* isolate, NativeModule* native_module,
                       Handle<WasmModuleObject> module_object,
                       ErrorThrower* thrower) {
  // Data structures for the parallel compilation.

  //-----------------------------------------------------------------------
  // For parallel compilation:
  // 1) The main thread allocates a compilation unit for each wasm function
  //    and stores them in the vector {compilation_units} within the
  //    {compilation_state}. By adding units to the {compilation_state}, new
  //    {BackgroundCompileTasks} instances are spawned which run on
  //    the background threads.
  // 2.a) The background threads and the main thread pick one compilation
  //      unit at a time and execute the parallel phase of the compilation
  //      unit. After finishing the execution of the parallel phase, the
  //      result is enqueued in {baseline_finish_units_}.
  // 2.b) If {baseline_finish_units_} contains a compilation unit, the main
  //      thread dequeues it and finishes the compilation.
  // 3) After the parallel phase of all compilation units has started, the
  //    main thread continues to finish all compilation units as long as
  //    baseline-compilation units are left to be processed.
  // 4) If tier-up is enabled, the main thread restarts background tasks
  //    that take care of compiling and finishing the top-tier compilation
  //    units.

  // Turn on the {CanonicalHandleScope} so that the background threads can
  // use the node cache.
  CanonicalHandleScope canonical(isolate);

  CompilationState* compilation_state = native_module->compilation_state();
  // Make sure that no foreground task is spawned for finishing
  // the compilation units. This foreground thread will be
  // responsible for finishing compilation.
  compilation_state->SetFinisherIsRunning(true);
  uint32_t num_wasm_functions =
      native_module->num_functions() - native_module->num_imported_functions();
  compilation_state->SetNumberOfFunctionsToCompile(num_wasm_functions);

  // 1) The main thread allocates a compilation unit for each wasm function
  //    and stores them in the vector {compilation_units} within the
  //    {compilation_state}. By adding units to the {compilation_state}, new
  //    {BackgroundCompileTask} instances are spawned which run on
  //    background threads.
  InitializeCompilationUnits(native_module);

  // 2.a) The background threads and the main thread pick one compilation
  //      unit at a time and execute the parallel phase of the compilation
  //      unit. After finishing the execution of the parallel phase, the
  //      result is enqueued in {baseline_finish_units_}.
  //      The foreground task bypasses waiting on memory threshold, because
  //      its results will immediately be converted to code (below).
  WasmFeatures detected_features;
  while (
      FetchAndExecuteCompilationUnit(compilation_state, &detected_features) &&
      !compilation_state->baseline_compilation_finished()) {
    // 2.b) If {baseline_finish_units_} contains a compilation unit, the main
    //      thread dequeues it and finishes the compilation unit. Compilation
    //      units are finished concurrently to the background threads to save
    //      memory.
    FinishCompilationUnits(compilation_state, thrower);

    if (compilation_state->failed()) break;
  }

  while (!compilation_state->failed()) {
    // 3) After the parallel phase of all compilation units has started, the
    //    main thread continues to finish compilation units as long as
    //    baseline compilation units are left to be processed. If compilation
    //    already failed, all background tasks have already been canceled
    //    in {FinishCompilationUnits}, and there are no units to finish.
    FinishCompilationUnits(compilation_state, thrower);

    if (compilation_state->baseline_compilation_finished()) break;
  }

  // Publish features from the foreground and background tasks.
  compilation_state->PublishDetectedFeatures(isolate, detected_features);

  // 4) If tiering-compilation is enabled, we need to set the finisher
  //    to false, such that the background threads will spawn a foreground
  //    thread to finish the top-tier compilation units.
  if (!compilation_state->failed() &&
      compilation_state->compile_mode() == CompileMode::kTiering) {
    compilation_state->SetFinisherIsRunning(false);
    compilation_state->RestartBackgroundTasks();
  }
}

void CompileSequentially(Isolate* isolate, NativeModule* native_module,
                         ModuleEnv* module_env, ErrorThrower* thrower) {
  DCHECK(!thrower->error());

  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  const WasmModule* module = module_env->module;
  WasmFeatures detected = kNoWasmFeatures;
  for (uint32_t i = 0; i < module->functions.size(); ++i) {
    const WasmFunction& func = module->functions[i];
    if (func.imported) continue;  // Imports are compiled at instantiation time.

    // Compile the function.
    WasmCode* code = WasmCompilationUnit::CompileWasmFunction(
        isolate, native_module, &detected, thrower, module_env, &func);
    if (code == nullptr) {
      TruncatedUserString<> name(wire_bytes.GetNameOrNull(&func, module));
      thrower->CompileError("Compilation of #%d:%.*s failed.", i, name.length(),
                            name.start());
      break;
    }
  }
  UpdateFeatureUseCounts(isolate, detected);
}

void ValidateSequentially(Isolate* isolate, NativeModule* native_module,
                          ErrorThrower* thrower) {
  DCHECK(!thrower->error());

  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  const WasmModule* module = native_module->module();
  uint32_t start = module->num_imported_functions;
  uint32_t end = start + module->num_declared_functions;
  for (uint32_t i = start; i < end; ++i) {
    const WasmFunction& func = module->functions[i];

    const byte* base = wire_bytes.start();
    FunctionBody body{func.sig, func.code.offset(), base + func.code.offset(),
                      base + func.code.end_offset()};
    DecodeResult result;
    {
      auto time_counter =
          SELECT_WASM_COUNTER(isolate->async_counters(), module->origin,
                              wasm_decode, function_time);

      TimedHistogramScope wasm_decode_function_time_scope(time_counter);
      WasmFeatures detected;
      result = VerifyWasmCode(isolate->allocator(),
                              native_module->enabled_features(), module,
                              &detected, body);
    }
    if (result.failed()) {
      TruncatedUserString<> name(wire_bytes.GetNameOrNull(&func, module));
      thrower->CompileError("Compiling function #%d:%.*s failed: %s @+%u", i,
                            name.length(), name.start(),
                            result.error_msg().c_str(), result.error_offset());
      break;
    }
  }
}

void CompileNativeModule(Isolate* isolate, ErrorThrower* thrower,
                         Handle<WasmModuleObject> module_object,
                         const WasmModule* wasm_module, ModuleEnv* env) {
  NativeModule* const native_module = module_object->native_module();
  ModuleWireBytes wire_bytes(native_module->wire_bytes());

  if (compile_lazy(wasm_module)) {
    if (wasm_module->origin == kWasmOrigin) {
      // Validate wasm modules for lazy compilation. Don't validate asm.js
      // modules, they are valid by construction (otherwise a CHECK will fail
      // during lazy compilation).
      // TODO(clemensh): According to the spec, we can actually skip validation
      // at module creation time, and return a function that always traps at
      // (lazy) compilation time.
      ValidateSequentially(isolate, native_module, thrower);
      if (thrower->error()) return;
    }

    native_module->SetLazyBuiltin(BUILTIN_CODE(isolate, WasmCompileLazy));
  } else {
    size_t funcs_to_compile =
        wasm_module->functions.size() - wasm_module->num_imported_functions;
    bool compile_parallel =
        !FLAG_trace_wasm_decoder && FLAG_wasm_num_compilation_tasks > 0 &&
        funcs_to_compile > 1 &&
        V8::GetCurrentPlatform()->NumberOfWorkerThreads() > 0;

    if (compile_parallel) {
      CompileInParallel(isolate, native_module, module_object, thrower);
    } else {
      CompileSequentially(isolate, native_module, env, thrower);
    }
    if (thrower->error()) return;
  }
}

// The runnable task that finishes compilation in foreground (e.g. updating
// the NativeModule, the code table, etc.).
class FinishCompileTask : public CancelableTask {
 public:
  explicit FinishCompileTask(CompilationState* compilation_state,
                             CancelableTaskManager* task_manager)
      : CancelableTask(task_manager), compilation_state_(compilation_state) {}

  void RunInternal() override {
    Isolate* isolate = compilation_state_->isolate();
    HandleScope scope(isolate);
    SaveContext saved_context(isolate);
    isolate->set_context(nullptr);

    TRACE_COMPILE("(4a) Finishing compilation units...\n");
    if (compilation_state_->failed()) {
      compilation_state_->SetFinisherIsRunning(false);
      return;
    }

    // We execute for 1 ms and then reschedule the task, same as the GC.
    double deadline = MonotonicallyIncreasingTimeInMs() + 1.0;
    while (true) {
      compilation_state_->RestartBackgroundTasks();

      std::unique_ptr<WasmCompilationUnit> unit =
          compilation_state_->GetNextExecutedUnit();

      if (unit == nullptr) {
        // It might happen that a background task just scheduled a unit to be
        // finished, but did not start a finisher task since the flag was still
        // set. Check for this case, and continue if there is more work.
        compilation_state_->SetFinisherIsRunning(false);
        if (compilation_state_->HasCompilationUnitToFinish() &&
            compilation_state_->SetFinisherIsRunning(true)) {
          continue;
        }
        break;
      }

      ErrorThrower thrower(compilation_state_->isolate(), "AsyncCompile");
      WasmCode* result = unit->FinishCompilation(&thrower);

      if (thrower.error()) {
        DCHECK_NULL(result);
        compilation_state_->OnError(&thrower);
        compilation_state_->SetFinisherIsRunning(false);
        thrower.Reset();
        break;
      }

      if (compilation_state_->baseline_compilation_finished()) {
        // If Liftoff compilation finishes it will directly start executing.
        // As soon as we have Turbofan-compiled code available, it will
        // directly be used by Liftoff-compiled code via the jump table.
        DCHECK_EQ(CompileMode::kTiering, compilation_state_->compile_mode());
        DCHECK(!result->is_liftoff());

        if (WasmCode::ShouldBeLogged(isolate)) result->LogCode(isolate);
      }

      // Update the compilation state, and possibly notify
      // threads waiting for events.
      compilation_state_->OnFinishedUnit();

      if (deadline < MonotonicallyIncreasingTimeInMs()) {
        // We reached the deadline. We reschedule this task and return
        // immediately. Since we rescheduled this task already, we do not set
        // the FinisherIsRunning flag to false.
        compilation_state_->ScheduleFinisherTask();
        return;
      }
    }
  }

 private:
  CompilationState* compilation_state_;
};

// The runnable task that performs compilations in the background.
class BackgroundCompileTask : public CancelableTask {
 public:
  explicit BackgroundCompileTask(CompilationState* compilation_state,
                                 CancelableTaskManager* task_manager)
      : CancelableTask(task_manager), compilation_state_(compilation_state) {}

  void RunInternal() override {
    TRACE_COMPILE("(3b) Compiling...\n");
    // The number of currently running background tasks is reduced in
    // {OnBackgroundTaskStopped}.
    while (!compilation_state_->failed()) {
      if (!FetchAndExecuteCompilationUnit(compilation_state_,
                                          &detected_features_)) {
        break;
      }
    }
    compilation_state_->OnBackgroundTaskStopped(detected_features_);
  }

 private:
  CompilationState* compilation_state_;
  WasmFeatures detected_features_ = kNoWasmFeatures;
};
}  // namespace

MaybeHandle<WasmModuleObject> CompileToModuleObject(
    Isolate* isolate, const WasmFeatures& enabled, ErrorThrower* thrower,
    std::shared_ptr<const WasmModule> module, const ModuleWireBytes& wire_bytes,
    Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {
  const WasmModule* wasm_module = module.get();
  TimedHistogramScope wasm_compile_module_time_scope(SELECT_WASM_COUNTER(
      isolate->counters(), wasm_module->origin, wasm_compile, module_time));

  // Embedder usage count for declared shared memories.
  if (wasm_module->has_shared_memory) {
    isolate->CountUsage(v8::Isolate::UseCounterFeature::kWasmSharedMemory);
  }

  // TODO(6792): No longer needed once WebAssembly code is off heap. Use
  // base::Optional to be able to close the scope before notifying the debugger.
  base::Optional<CodeSpaceMemoryModificationScope> modification_scope(
      base::in_place_t(), isolate->heap());

  // Create heap objects for script, module bytes and asm.js offset table to
  // be stored in the module object.
  Handle<Script> script;
  Handle<ByteArray> asm_js_offset_table;
  if (asm_js_script.is_null()) {
    script = CreateWasmScript(isolate, wire_bytes, wasm_module->source_map_url);
  } else {
    script = asm_js_script;
    asm_js_offset_table =
        isolate->factory()->NewByteArray(asm_js_offset_table_bytes.length());
    asm_js_offset_table->copy_in(0, asm_js_offset_table_bytes.start(),
                                 asm_js_offset_table_bytes.length());
  }
  // TODO(wasm): only save the sections necessary to deserialize a
  // {WasmModule}. E.g. function bodies could be omitted.
  OwnedVector<uint8_t> wire_bytes_copy =
      OwnedVector<uint8_t>::Of(wire_bytes.module_bytes());

  // Create the module object.
  // TODO(clemensh): For the same module (same bytes / same hash), we should
  // only have one WasmModuleObject. Otherwise, we might only set
  // breakpoints on a (potentially empty) subset of the instances.
  ModuleEnv env = CreateDefaultModuleEnv(wasm_module);

  // Create the compiled module object and populate with compiled functions
  // and information needed at instantiation time. This object needs to be
  // serializable. Instantiation may occur off a deserialized version of this
  // object.
  Handle<WasmModuleObject> module_object = WasmModuleObject::New(
      isolate, enabled, std::move(module), env, std::move(wire_bytes_copy),
      script, asm_js_offset_table);
  CompileNativeModule(isolate, thrower, module_object, wasm_module, &env);
  if (thrower->error()) return {};

  // Compile JS->wasm wrappers for exported functions.
  CompileJsToWasmWrappers(isolate, module_object);

  // If we created a wasm script, finish it now and make it public to the
  // debugger.
  if (asm_js_script.is_null()) {
    // Close the CodeSpaceMemoryModificationScope before calling into the
    // debugger.
    modification_scope.reset();
    isolate->debug()->OnAfterCompile(script);
  }

  // Log the code within the generated module for profiling.
  module_object->native_module()->LogWasmCodes(isolate);

  return module_object;
}

InstanceBuilder::InstanceBuilder(Isolate* isolate, ErrorThrower* thrower,
                                 Handle<WasmModuleObject> module_object,
                                 MaybeHandle<JSReceiver> ffi,
                                 MaybeHandle<JSArrayBuffer> memory)
    : isolate_(isolate),
      enabled_(module_object->native_module()->enabled_features()),
      module_(module_object->module()),
      thrower_(thrower),
      module_object_(module_object),
      ffi_(ffi),
      memory_(memory) {
  sanitized_imports_.reserve(module_->import_table.size());
}

// Build an instance, in all of its glory.
MaybeHandle<WasmInstanceObject> InstanceBuilder::Build() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "InstanceBuilder::Build");
  // Check that an imports argument was provided, if the module requires it.
  // No point in continuing otherwise.
  if (!module_->import_table.empty() && ffi_.is_null()) {
    thrower_->TypeError(
        "Imports argument must be present and must be an object");
    return {};
  }

  SanitizeImports();
  if (thrower_->error()) return {};

  // TODO(6792): No longer needed once WebAssembly code is off heap.
  CodeSpaceMemoryModificationScope modification_scope(isolate_->heap());
  // From here on, we expect the build pipeline to run without exiting to JS.
  DisallowJavascriptExecution no_js(isolate_);
  // Record build time into correct bucket, then build instance.
  TimedHistogramScope wasm_instantiate_module_time_scope(SELECT_WASM_COUNTER(
      isolate_->counters(), module_->origin, wasm_instantiate, module_time));

  //--------------------------------------------------------------------------
  // Allocate the memory array buffer.
  //--------------------------------------------------------------------------
  // We allocate the memory buffer before cloning or reusing the compiled module
  // so we will know whether we need to recompile with bounds checks.
  uint32_t initial_pages = module_->initial_pages;
  auto initial_pages_counter = SELECT_WASM_COUNTER(
      isolate_->counters(), module_->origin, wasm, min_mem_pages_count);
  initial_pages_counter->AddSample(initial_pages);
  // Asm.js has memory_ already set at this point, so we don't want to
  // overwrite it.
  if (memory_.is_null()) {
    memory_ = FindImportedMemoryBuffer();
  }
  if (!memory_.is_null()) {
    // Set externally passed ArrayBuffer non neuterable.
    Handle<JSArrayBuffer> memory = memory_.ToHandleChecked();
    memory->set_is_neuterable(false);

    DCHECK_IMPLIES(use_trap_handler(), module_->origin == kAsmJsOrigin ||
                                           memory->is_wasm_memory() ||
                                           memory->backing_store() == nullptr);
  } else if (initial_pages > 0 || use_trap_handler()) {
    // We need to unconditionally create a guard region if using trap handlers,
    // even when the size is zero to prevent null-dereference issues
    // (e.g. https://crbug.com/769637).
    // Allocate memory if the initial size is more than 0 pages.
    memory_ = AllocateMemory(initial_pages);
    if (memory_.is_null()) {
      // failed to allocate memory
      DCHECK(isolate_->has_pending_exception() || thrower_->error());
      return {};
    }
  }

  //--------------------------------------------------------------------------
  // Recompile module if using trap handlers but could not get guarded memory
  //--------------------------------------------------------------------------
  if (module_->origin == kWasmOrigin && use_trap_handler()) {
    // Make sure the memory has suitable guard regions.
    WasmMemoryTracker* const memory_tracker =
        isolate_->wasm_engine()->memory_tracker();

    if (!memory_tracker->HasFullGuardRegions(
            memory_.ToHandleChecked()->backing_store())) {
      if (!FLAG_wasm_trap_handler_fallback) {
        thrower_->LinkError(
            "Provided memory is lacking guard regions but fallback was "
            "disabled.");
        return {};
      }

      TRACE("Recompiling module without bounds checks\n");
      constexpr bool allow_trap_handler = false;
      ModuleEnv env = CreateDefaultModuleEnv(module_, allow_trap_handler);
      // Disable trap handlers on this native module.
      NativeModule* native_module = module_object_->native_module();
      native_module->DisableTrapHandler();

      // Recompile all functions in this native module.
      ErrorThrower thrower(isolate_, "recompile");
      CompileNativeModule(isolate_, &thrower, module_object_, module_, &env);
      if (thrower.error()) {
        return {};
      }
      DCHECK(!native_module->use_trap_handler());
    }
  }

  //--------------------------------------------------------------------------
  // Create the WebAssembly.Instance object.
  //--------------------------------------------------------------------------
  NativeModule* native_module = module_object_->native_module();
  TRACE("New module instantiation for %p\n", native_module);
  Handle<WasmInstanceObject> instance =
      WasmInstanceObject::New(isolate_, module_object_);
  NativeModuleModificationScope native_modification_scope(native_module);

  //--------------------------------------------------------------------------
  // Set up the globals for the new instance.
  //--------------------------------------------------------------------------
  uint32_t globals_buffer_size = module_->globals_buffer_size;
  if (globals_buffer_size > 0) {
    void* backing_store =
        isolate_->array_buffer_allocator()->Allocate(globals_buffer_size);
    if (backing_store == nullptr) {
      thrower_->RangeError("Out of memory: wasm globals");
      return {};
    }
    globals_ =
        isolate_->factory()->NewJSArrayBuffer(SharedFlag::kNotShared, TENURED);
    constexpr bool is_external = false;
    constexpr bool is_wasm_memory = false;
    JSArrayBuffer::Setup(globals_, isolate_, is_external, backing_store,
                         globals_buffer_size, SharedFlag::kNotShared,
                         is_wasm_memory);
    if (globals_.is_null()) {
      thrower_->RangeError("Out of memory: wasm globals");
      return {};
    }
    instance->set_globals_start(
        reinterpret_cast<byte*>(globals_->backing_store()));
    instance->set_globals_buffer(*globals_);
  }

  //--------------------------------------------------------------------------
  // Set up the array of references to imported globals' array buffers.
  //--------------------------------------------------------------------------
  if (module_->num_imported_mutable_globals > 0) {
    // TODO(binji): This allocates one slot for each mutable global, which is
    // more than required if multiple globals are imported from the same
    // module.
    Handle<FixedArray> buffers_array = isolate_->factory()->NewFixedArray(
        module_->num_imported_mutable_globals, TENURED);
    instance->set_imported_mutable_globals_buffers(*buffers_array);
  }

  //--------------------------------------------------------------------------
  // Set up the exception table used for exception tag checks.
  //--------------------------------------------------------------------------
  int exceptions_count = static_cast<int>(module_->exceptions.size());
  if (exceptions_count > 0) {
    Handle<FixedArray> exception_table =
        isolate_->factory()->NewFixedArray(exceptions_count, TENURED);
    instance->set_exceptions_table(*exception_table);
    exception_wrappers_.resize(exceptions_count);
  }

  //--------------------------------------------------------------------------
  // Reserve the metadata for indirect function tables.
  //--------------------------------------------------------------------------
  int table_count = static_cast<int>(module_->tables.size());
  table_instances_.resize(table_count);

  //--------------------------------------------------------------------------
  // Process the imports for the module.
  //--------------------------------------------------------------------------
  int num_imported_functions = ProcessImports(instance);
  if (num_imported_functions < 0) return {};

  //--------------------------------------------------------------------------
  // Process the initialization for the module's globals.
  //--------------------------------------------------------------------------
  InitGlobals();

  //--------------------------------------------------------------------------
  // Initialize the indirect tables.
  //--------------------------------------------------------------------------
  if (table_count > 0) {
    InitializeTables(instance);
  }

  //--------------------------------------------------------------------------
  // Initialize the exceptions table.
  //--------------------------------------------------------------------------
  if (exceptions_count > 0) {
    InitializeExceptions(instance);
  }

  //--------------------------------------------------------------------------
  // Create the WebAssembly.Memory object.
  //--------------------------------------------------------------------------
  if (module_->has_memory) {
    if (!instance->has_memory_object()) {
      // No memory object exists. Create one.
      Handle<WasmMemoryObject> memory_object = WasmMemoryObject::New(
          isolate_, memory_,
          module_->maximum_pages != 0 ? module_->maximum_pages : -1);
      instance->set_memory_object(*memory_object);
    }

    // Add the instance object to the list of instances for this memory.
    Handle<WasmMemoryObject> memory_object(instance->memory_object(), isolate_);
    WasmMemoryObject::AddInstance(isolate_, memory_object, instance);

    if (!memory_.is_null()) {
      // Double-check the {memory} array buffer matches the instance.
      Handle<JSArrayBuffer> memory = memory_.ToHandleChecked();
      CHECK_EQ(instance->memory_size(), memory->byte_length());
      CHECK_EQ(instance->memory_start(), memory->backing_store());
    }
  }

  //--------------------------------------------------------------------------
  // Check that indirect function table segments are within bounds.
  //--------------------------------------------------------------------------
  for (const WasmTableInit& table_init : module_->table_inits) {
    DCHECK(table_init.table_index < table_instances_.size());
    uint32_t base = EvalUint32InitExpr(table_init.offset);
    size_t table_size = table_instances_[table_init.table_index].table_size;
    if (!in_bounds(base, table_init.entries.size(), table_size)) {
      thrower_->LinkError("table initializer is out of bounds");
      return {};
    }
  }

  //--------------------------------------------------------------------------
  // Check that memory segments are within bounds.
  //--------------------------------------------------------------------------
  for (const WasmDataSegment& seg : module_->data_segments) {
    uint32_t base = EvalUint32InitExpr(seg.dest_addr);
    if (!in_bounds(base, seg.source.length(), instance->memory_size())) {
      thrower_->LinkError("data segment is out of bounds");
      return {};
    }
  }

  //--------------------------------------------------------------------------
  // Set up the exports object for the new instance.
  //--------------------------------------------------------------------------
  ProcessExports(instance);
  if (thrower_->error()) return {};

  //--------------------------------------------------------------------------
  // Initialize the indirect function tables.
  //--------------------------------------------------------------------------
  if (table_count > 0) {
    LoadTableSegments(instance);
  }

  //--------------------------------------------------------------------------
  // Initialize the memory by loading data segments.
  //--------------------------------------------------------------------------
  if (module_->data_segments.size() > 0) {
    LoadDataSegments(instance);
  }

  //--------------------------------------------------------------------------
  // Debugging support.
  //--------------------------------------------------------------------------
  // Set all breakpoints that were set on the shared module.
  WasmModuleObject::SetBreakpointsOnNewInstance(module_object_, instance);

  if (FLAG_wasm_interpret_all && module_->origin == kWasmOrigin) {
    Handle<WasmDebugInfo> debug_info =
        WasmInstanceObject::GetOrCreateDebugInfo(instance);
    std::vector<int> func_indexes;
    for (int func_index = num_imported_functions,
             num_wasm_functions = static_cast<int>(module_->functions.size());
         func_index < num_wasm_functions; ++func_index) {
      func_indexes.push_back(func_index);
    }
    WasmDebugInfo::RedirectToInterpreter(
        debug_info, Vector<int>(func_indexes.data(),
                                static_cast<int>(func_indexes.size())));
  }

  //--------------------------------------------------------------------------
  // Create a wrapper for the start function.
  //--------------------------------------------------------------------------
  if (module_->start_function_index >= 0) {
    int start_index = module_->start_function_index;
    auto& function = module_->functions[start_index];
    Handle<Code> wrapper_code = js_to_wasm_cache_.GetOrCompileJSToWasmWrapper(
        isolate_, function.sig, function.imported);
    // TODO(clemensh): Don't generate an exported function for the start
    // function. Use CWasmEntry instead.
    start_function_ = WasmExportedFunction::New(
        isolate_, instance, MaybeHandle<String>(), start_index,
        static_cast<int>(function.sig->parameter_count()), wrapper_code);
  }

  DCHECK(!isolate_->has_pending_exception());
  TRACE("Successfully built instance for module %p\n",
        module_object_->native_module());
  return instance;
}

bool InstanceBuilder::ExecuteStartFunction() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
               "InstanceBuilder::ExecuteStartFunction");
  if (start_function_.is_null()) return true;  // No start function.

  HandleScope scope(isolate_);
  // Call the JS function.
  Handle<Object> undefined = isolate_->factory()->undefined_value();
  MaybeHandle<Object> retval =
      Execution::Call(isolate_, start_function_, undefined, 0, nullptr);

  if (retval.is_null()) {
    DCHECK(isolate_->has_pending_exception());
    return false;
  }
  return true;
}

// Look up an import value in the {ffi_} object.
MaybeHandle<Object> InstanceBuilder::LookupImport(uint32_t index,
                                                  Handle<String> module_name,

                                                  Handle<String> import_name) {
  // We pre-validated in the js-api layer that the ffi object is present, and
  // a JSObject, if the module has imports.
  DCHECK(!ffi_.is_null());

  // Look up the module first.
  MaybeHandle<Object> result = Object::GetPropertyOrElement(
      isolate_, ffi_.ToHandleChecked(), module_name);
  if (result.is_null()) {
    return ReportTypeError("module not found", index, module_name);
  }

  Handle<Object> module = result.ToHandleChecked();

  // Look up the value in the module.
  if (!module->IsJSReceiver()) {
    return ReportTypeError("module is not an object or function", index,
                           module_name);
  }

  result = Object::GetPropertyOrElement(isolate_, module, import_name);
  if (result.is_null()) {
    ReportLinkError("import not found", index, module_name, import_name);
    return MaybeHandle<JSFunction>();
  }

  return result;
}

// Look up an import value in the {ffi_} object specifically for linking an
// asm.js module. This only performs non-observable lookups, which allows
// falling back to JavaScript proper (and hence re-executing all lookups) if
// module instantiation fails.
MaybeHandle<Object> InstanceBuilder::LookupImportAsm(
    uint32_t index, Handle<String> import_name) {
  // Check that a foreign function interface object was provided.
  if (ffi_.is_null()) {
    return ReportLinkError("missing imports object", index, import_name);
  }

  // Perform lookup of the given {import_name} without causing any observable
  // side-effect. We only accept accesses that resolve to data properties,
  // which is indicated by the asm.js spec in section 7 ("Linking") as well.
  Handle<Object> result;
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate_, ffi_.ToHandleChecked(), import_name);
  switch (it.state()) {
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::INTEGER_INDEXED_EXOTIC:
    case LookupIterator::INTERCEPTOR:
    case LookupIterator::JSPROXY:
    case LookupIterator::ACCESSOR:
    case LookupIterator::TRANSITION:
      return ReportLinkError("not a data property", index, import_name);
    case LookupIterator::NOT_FOUND:
      // Accepting missing properties as undefined does not cause any
      // observable difference from JavaScript semantics, we are lenient.
      result = isolate_->factory()->undefined_value();
      break;
    case LookupIterator::DATA:
      result = it.GetDataValue();
      break;
  }

  return result;
}

uint32_t InstanceBuilder::EvalUint32InitExpr(const WasmInitExpr& expr) {
  switch (expr.kind) {
    case WasmInitExpr::kI32Const:
      return expr.val.i32_const;
    case WasmInitExpr::kGlobalIndex: {
      uint32_t offset = module_->globals[expr.val.global_index].offset;
      return ReadLittleEndianValue<uint32_t>(
          reinterpret_cast<Address>(raw_buffer_ptr(globals_, offset)));
    }
    default:
      UNREACHABLE();
  }
}

// Load data segments into the memory.
void InstanceBuilder::LoadDataSegments(Handle<WasmInstanceObject> instance) {
  Vector<const uint8_t> wire_bytes =
      module_object_->native_module()->wire_bytes();
  for (const WasmDataSegment& segment : module_->data_segments) {
    uint32_t source_size = segment.source.length();
    // Segments of size == 0 are just nops.
    if (source_size == 0) continue;
    uint32_t dest_offset = EvalUint32InitExpr(segment.dest_addr);
    DCHECK(in_bounds(dest_offset, source_size, instance->memory_size()));
    byte* dest = instance->memory_start() + dest_offset;
    const byte* src = wire_bytes.start() + segment.source.offset();
    memcpy(dest, src, source_size);
  }
}

void InstanceBuilder::WriteGlobalValue(const WasmGlobal& global, double num) {
  TRACE("init [globals_start=%p + %u] = %lf, type = %s\n",
        reinterpret_cast<void*>(raw_buffer_ptr(globals_, 0)), global.offset,
        num, ValueTypes::TypeName(global.type));
  switch (global.type) {
    case kWasmI32:
      WriteLittleEndianValue<int32_t>(GetRawGlobalPtr<int32_t>(global),
                                      static_cast<int32_t>(num));
      break;
    case kWasmI64:
      // TODO(titzer): initialization of imported i64 globals.
      UNREACHABLE();
      break;
    case kWasmF32:
      WriteLittleEndianValue<float>(GetRawGlobalPtr<float>(global),
                                    static_cast<float>(num));
      break;
    case kWasmF64:
      WriteLittleEndianValue<double>(GetRawGlobalPtr<double>(global),
                                     static_cast<double>(num));
      break;
    default:
      UNREACHABLE();
  }
}

void InstanceBuilder::WriteGlobalValue(const WasmGlobal& global,
                                       Handle<WasmGlobalObject> value) {
  TRACE("init [globals_start=%p + %u] = ",
        reinterpret_cast<void*>(raw_buffer_ptr(globals_, 0)), global.offset);
  switch (global.type) {
    case kWasmI32: {
      int32_t num = value->GetI32();
      WriteLittleEndianValue<int32_t>(GetRawGlobalPtr<int32_t>(global), num);
      TRACE("%d", num);
      break;
    }
    case kWasmI64: {
      int64_t num = value->GetI64();
      WriteLittleEndianValue<int64_t>(GetRawGlobalPtr<int64_t>(global), num);
      TRACE("%" PRId64, num);
      break;
    }
    case kWasmF32: {
      float num = value->GetF32();
      WriteLittleEndianValue<float>(GetRawGlobalPtr<float>(global), num);
      TRACE("%f", num);
      break;
    }
    case kWasmF64: {
      double num = value->GetF64();
      WriteLittleEndianValue<double>(GetRawGlobalPtr<double>(global), num);
      TRACE("%lf", num);
      break;
    }
    default:
      UNREACHABLE();
  }
  TRACE(", type = %s (from WebAssembly.Global)\n",
        ValueTypes::TypeName(global.type));
}

void InstanceBuilder::SanitizeImports() {
  Vector<const uint8_t> wire_bytes =
      module_object_->native_module()->wire_bytes();
  for (size_t index = 0; index < module_->import_table.size(); ++index) {
    const WasmImport& import = module_->import_table[index];

    Handle<String> module_name;
    MaybeHandle<String> maybe_module_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(isolate_, wire_bytes,
                                                           import.module_name);
    if (!maybe_module_name.ToHandle(&module_name)) {
      thrower_->LinkError("Could not resolve module name for import %zu",
                          index);
      return;
    }

    Handle<String> import_name;
    MaybeHandle<String> maybe_import_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(isolate_, wire_bytes,
                                                           import.field_name);
    if (!maybe_import_name.ToHandle(&import_name)) {
      thrower_->LinkError("Could not resolve import name for import %zu",
                          index);
      return;
    }

    int int_index = static_cast<int>(index);
    MaybeHandle<Object> result =
        module_->origin == kAsmJsOrigin
            ? LookupImportAsm(int_index, import_name)
            : LookupImport(int_index, module_name, import_name);
    if (thrower_->error()) {
      thrower_->LinkError("Could not find value for import %zu", index);
      return;
    }
    Handle<Object> value = result.ToHandleChecked();
    sanitized_imports_.push_back({module_name, import_name, value});
  }
}

MaybeHandle<JSArrayBuffer> InstanceBuilder::FindImportedMemoryBuffer() const {
  DCHECK_EQ(module_->import_table.size(), sanitized_imports_.size());
  for (size_t index = 0; index < module_->import_table.size(); index++) {
    const WasmImport& import = module_->import_table[index];

    if (import.kind == kExternalMemory) {
      const auto& value = sanitized_imports_[index].value;
      if (!value->IsWasmMemoryObject()) {
        return {};
      }
      auto memory = Handle<WasmMemoryObject>::cast(value);
      Handle<JSArrayBuffer> buffer(memory->array_buffer(), isolate_);
      return buffer;
    }
  }
  return {};
}

// Process the imports, including functions, tables, globals, and memory, in
// order, loading them from the {ffi_} object. Returns the number of imported
// functions.
int InstanceBuilder::ProcessImports(Handle<WasmInstanceObject> instance) {
  int num_imported_functions = 0;
  int num_imported_tables = 0;
  int num_imported_mutable_globals = 0;

  DCHECK_EQ(module_->import_table.size(), sanitized_imports_.size());
  int num_imports = static_cast<int>(module_->import_table.size());
  NativeModule* native_module = instance->module_object()->native_module();
  for (int index = 0; index < num_imports; ++index) {
    const WasmImport& import = module_->import_table[index];

    Handle<String> module_name = sanitized_imports_[index].module_name;
    Handle<String> import_name = sanitized_imports_[index].import_name;
    Handle<Object> value = sanitized_imports_[index].value;

    switch (import.kind) {
      case kExternalFunction: {
        // Function imports must be callable.
        if (!value->IsCallable()) {
          ReportLinkError("function import requires a callable", index,
                          module_name, import_name);
          return -1;
        }
        uint32_t func_index = import.index;
        DCHECK_EQ(num_imported_functions, func_index);
        auto js_receiver = Handle<JSReceiver>::cast(value);
        FunctionSig* expected_sig = module_->functions[func_index].sig;
        auto kind = compiler::GetWasmImportCallKind(js_receiver, expected_sig);
        switch (kind) {
          case compiler::WasmImportCallKind::kLinkError:
            ReportLinkError(
                "imported function does not match the expected type", index,
                module_name, import_name);
            return -1;
          case compiler::WasmImportCallKind::kWasmToWasm: {
            // The imported function is a WASM function from another instance.
            auto imported_function = Handle<WasmExportedFunction>::cast(value);
            Handle<WasmInstanceObject> imported_instance(
                imported_function->instance(), isolate_);
            // The import reference is the instance object itself.
            Address imported_target = imported_function->GetWasmCallTarget();
            ImportedFunctionEntry entry(instance, func_index);
            entry.set_wasm_to_wasm(*imported_instance, imported_target);
            break;
          }
          default: {
            // The imported function is a callable.
            Handle<Code> wrapper_code =
                compiler::CompileWasmImportCallWrapper(
                    isolate_, kind, expected_sig, func_index, module_->origin,
                    use_trap_handler())
                    .ToHandleChecked();
            RecordStats(*wrapper_code, isolate_->counters());

            WasmCode* wasm_code =
                native_module->AddImportWrapper(wrapper_code, func_index);
            ImportedFunctionEntry entry(instance, func_index);
            entry.set_wasm_to_js(*js_receiver, wasm_code);
            break;
          }
        }
        num_imported_functions++;
        break;
      }
      case kExternalTable: {
        if (!value->IsWasmTableObject()) {
          ReportLinkError("table import requires a WebAssembly.Table", index,
                          module_name, import_name);
          return -1;
        }
        uint32_t table_num = import.index;
        DCHECK_EQ(table_num, num_imported_tables);
        const WasmTable& table = module_->tables[table_num];
        TableInstance& table_instance = table_instances_[table_num];
        table_instance.table_object = Handle<WasmTableObject>::cast(value);
        instance->set_table_object(*table_instance.table_object);
        table_instance.js_wrappers = Handle<FixedArray>(
            table_instance.table_object->functions(), isolate_);

        int imported_table_size = table_instance.js_wrappers->length();
        if (imported_table_size < static_cast<int>(table.initial_size)) {
          thrower_->LinkError(
              "table import %d is smaller than initial %d, got %u", index,
              table.initial_size, imported_table_size);
          return -1;
        }

        if (table.has_maximum_size) {
          int64_t imported_maximum_size =
              table_instance.table_object->maximum_length()->Number();
          if (imported_maximum_size < 0) {
            thrower_->LinkError(
                "table import %d has no maximum length, expected %d", index,
                table.maximum_size);
            return -1;
          }
          if (imported_maximum_size > table.maximum_size) {
            thrower_->LinkError(
                " table import %d has a larger maximum size %" PRIx64
                " than the module's declared maximum %u",
                index, imported_maximum_size, table.maximum_size);
            return -1;
          }
        }

        // Allocate a new dispatch table.
        if (!instance->has_indirect_function_table()) {
          WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
              instance, imported_table_size);
          table_instances_[table_num].table_size = imported_table_size;
        }
        // Initialize the dispatch table with the (foreign) JS functions
        // that are already in the table.
        for (int i = 0; i < imported_table_size; ++i) {
          Handle<Object> val(table_instance.js_wrappers->get(i), isolate_);
          // TODO(mtrofin): this is the same logic as WasmTableObject::Set:
          // insert in the local table a wrapper from the other module, and add
          // a reference to the owning instance of the other module.
          if (!val->IsJSFunction()) continue;
          if (!WasmExportedFunction::IsWasmExportedFunction(*val)) {
            thrower_->LinkError("table import %d[%d] is not a wasm function",
                                index, i);
            return -1;
          }
          // Look up the signature's canonical id. If there is no canonical
          // id, then the signature does not appear at all in this module,
          // so putting {-1} in the table will cause checks to always fail.
          auto target = Handle<WasmExportedFunction>::cast(val);
          Handle<WasmInstanceObject> imported_instance =
              handle(target->instance(), isolate_);
          Address exported_call_target = target->GetWasmCallTarget();
          FunctionSig* sig = imported_instance->module()
                                 ->functions[target->function_index()]
                                 .sig;
          IndirectFunctionTableEntry(instance, i)
              .set(module_->signature_map.Find(*sig), *imported_instance,
                   exported_call_target);
        }
        num_imported_tables++;
        break;
      }
      case kExternalMemory: {
        // Validation should have failed if more than one memory object was
        // provided.
        DCHECK(!instance->has_memory_object());
        if (!value->IsWasmMemoryObject()) {
          ReportLinkError("memory import must be a WebAssembly.Memory object",
                          index, module_name, import_name);
          return -1;
        }
        auto memory = Handle<WasmMemoryObject>::cast(value);
        instance->set_memory_object(*memory);
        Handle<JSArrayBuffer> buffer(memory->array_buffer(), isolate_);
        // memory_ should have already been assigned in Build().
        DCHECK_EQ(*memory_.ToHandleChecked(), *buffer);
        uint32_t imported_cur_pages =
            static_cast<uint32_t>(buffer->byte_length() / kWasmPageSize);
        if (imported_cur_pages < module_->initial_pages) {
          thrower_->LinkError(
              "memory import %d is smaller than initial %u, got %u", index,
              module_->initial_pages, imported_cur_pages);
        }
        int32_t imported_maximum_pages = memory->maximum_pages();
        if (module_->has_maximum_pages) {
          if (imported_maximum_pages < 0) {
            thrower_->LinkError(
                "memory import %d has no maximum limit, expected at most %u",
                index, imported_maximum_pages);
            return -1;
          }
          if (static_cast<uint32_t>(imported_maximum_pages) >
              module_->maximum_pages) {
            thrower_->LinkError(
                "memory import %d has a larger maximum size %u than the "
                "module's declared maximum %u",
                index, imported_maximum_pages, module_->maximum_pages);
            return -1;
          }
        }
        if (module_->has_shared_memory != buffer->is_shared()) {
          thrower_->LinkError(
              "mismatch in shared state of memory, declared = %d, imported = "
              "%d",
              module_->has_shared_memory, buffer->is_shared());
          return -1;
        }

        break;
      }
      case kExternalGlobal: {
        // Immutable global imports are converted to numbers and written into
        // the {globals_} array buffer.
        //
        // Mutable global imports instead have their backing array buffers
        // referenced by this instance, and store the address of the imported
        // global in the {imported_mutable_globals_} array.
        const WasmGlobal& global = module_->globals[import.index];

        // The mutable-global proposal allows importing i64 values, but only if
        // they are passed as a WebAssembly.Global object.
        if (global.type == kWasmI64 &&
            !(enabled_.mut_global && value->IsWasmGlobalObject())) {
          ReportLinkError("global import cannot have type i64", index,
                          module_name, import_name);
          return -1;
        }
        if (module_->origin == kAsmJsOrigin) {
          // Accepting {JSFunction} on top of just primitive values here is a
          // workaround to support legacy asm.js code with broken binding. Note
          // that using {NaN} (or Smi::kZero) here is what using the observable
          // conversion via {ToPrimitive} would produce as well.
          // TODO(mstarzinger): Still observable if Function.prototype.valueOf
          // or friends are patched, we might need to check for that as well.
          if (value->IsJSFunction()) value = isolate_->factory()->nan_value();
          if (value->IsPrimitive() && !value->IsSymbol()) {
            if (global.type == kWasmI32) {
              value = Object::ToInt32(isolate_, value).ToHandleChecked();
            } else {
              value = Object::ToNumber(isolate_, value).ToHandleChecked();
            }
          }
        }
        if (enabled_.mut_global) {
          if (value->IsWasmGlobalObject()) {
            auto global_object = Handle<WasmGlobalObject>::cast(value);
            if (global_object->type() != global.type) {
              ReportLinkError(
                  "imported global does not match the expected type", index,
                  module_name, import_name);
              return -1;
            }
            if (global_object->is_mutable() != global.mutability) {
              ReportLinkError(
                  "imported global does not match the expected mutability",
                  index, module_name, import_name);
              return -1;
            }
            if (global.mutability) {
              Handle<JSArrayBuffer> buffer(global_object->array_buffer(),
                                           isolate_);
              int index = num_imported_mutable_globals++;
              instance->imported_mutable_globals_buffers()->set(index, *buffer);
              // It is safe in this case to store the raw pointer to the buffer
              // since the backing store of the JSArrayBuffer will not be
              // relocated.
              instance->imported_mutable_globals()[index] =
                  reinterpret_cast<Address>(
                      raw_buffer_ptr(buffer, global_object->offset()));
            } else {
              WriteGlobalValue(global, global_object);
            }
          } else if (value->IsNumber()) {
            if (global.mutability) {
              ReportLinkError(
                  "imported mutable global must be a WebAssembly.Global object",
                  index, module_name, import_name);
              return -1;
            }
            WriteGlobalValue(global, value->Number());
          } else {
            ReportLinkError(
                "global import must be a number or WebAssembly.Global object",
                index, module_name, import_name);
            return -1;
          }
        } else {
          if (value->IsNumber()) {
            WriteGlobalValue(global, value->Number());
          } else {
            ReportLinkError("global import must be a number", index,
                            module_name, import_name);
            return -1;
          }
        }
        break;
      }
      case kExternalException: {
        if (!value->IsWasmExceptionObject()) {
          ReportLinkError("exception import requires a WebAssembly.Exception",
                          index, module_name, import_name);
          return -1;
        }
        Handle<WasmExceptionObject> imported_exception =
            Handle<WasmExceptionObject>::cast(value);
        if (!imported_exception->IsSignatureEqual(
                module_->exceptions[import.index].sig)) {
          ReportLinkError("imported exception does not match the expected type",
                          index, module_name, import_name);
          return -1;
        }
        Object* exception_tag = imported_exception->exception_tag();
        DCHECK(instance->exceptions_table()->get(import.index)->IsUndefined());
        instance->exceptions_table()->set(import.index, exception_tag);
        exception_wrappers_[import.index] = imported_exception;
        break;
      }
      default:
        UNREACHABLE();
        break;
    }
  }

  DCHECK_EQ(module_->num_imported_mutable_globals,
            num_imported_mutable_globals);

  return num_imported_functions;
}

template <typename T>
T* InstanceBuilder::GetRawGlobalPtr(const WasmGlobal& global) {
  return reinterpret_cast<T*>(raw_buffer_ptr(globals_, global.offset));
}

// Process initialization of globals.
void InstanceBuilder::InitGlobals() {
  for (auto global : module_->globals) {
    if (global.mutability && global.imported) {
      continue;
    }

    switch (global.init.kind) {
      case WasmInitExpr::kI32Const:
        WriteLittleEndianValue<int32_t>(GetRawGlobalPtr<int32_t>(global),
                                        global.init.val.i32_const);
        break;
      case WasmInitExpr::kI64Const:
        WriteLittleEndianValue<int64_t>(GetRawGlobalPtr<int64_t>(global),
                                        global.init.val.i64_const);
        break;
      case WasmInitExpr::kF32Const:
        WriteLittleEndianValue<float>(GetRawGlobalPtr<float>(global),
                                      global.init.val.f32_const);
        break;
      case WasmInitExpr::kF64Const:
        WriteLittleEndianValue<double>(GetRawGlobalPtr<double>(global),
                                       global.init.val.f64_const);
        break;
      case WasmInitExpr::kGlobalIndex: {
        // Initialize with another global.
        uint32_t new_offset = global.offset;
        uint32_t old_offset =
            module_->globals[global.init.val.global_index].offset;
        TRACE("init [globals+%u] = [globals+%d]\n", global.offset, old_offset);
        size_t size = (global.type == kWasmI64 || global.type == kWasmF64)
                          ? sizeof(double)
                          : sizeof(int32_t);
        memcpy(raw_buffer_ptr(globals_, new_offset),
               raw_buffer_ptr(globals_, old_offset), size);
        break;
      }
      case WasmInitExpr::kNone:
        // Happens with imported globals.
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
}

// Allocate memory for a module instance as a new JSArrayBuffer.
Handle<JSArrayBuffer> InstanceBuilder::AllocateMemory(uint32_t num_pages) {
  if (num_pages > FLAG_wasm_max_mem_pages) {
    thrower_->RangeError("Out of memory: wasm memory too large");
    return Handle<JSArrayBuffer>::null();
  }
  const bool is_shared_memory = module_->has_shared_memory && enabled_.threads;
  i::SharedFlag shared_flag =
      is_shared_memory ? i::SharedFlag::kShared : i::SharedFlag::kNotShared;
  Handle<JSArrayBuffer> mem_buffer;
  if (!NewArrayBuffer(isolate_, num_pages * kWasmPageSize, shared_flag)
           .ToHandle(&mem_buffer)) {
    thrower_->RangeError("Out of memory: wasm memory");
  }
  return mem_buffer;
}

bool InstanceBuilder::NeedsWrappers() const {
  if (module_->num_exported_functions > 0) return true;
  for (auto& table_instance : table_instances_) {
    if (!table_instance.js_wrappers.is_null()) return true;
  }
  for (auto& table : module_->tables) {
    if (table.exported) return true;
  }
  return false;
}

// Process the exports, creating wrappers for functions, tables, memories,
// globals, and exceptions.
void InstanceBuilder::ProcessExports(Handle<WasmInstanceObject> instance) {
  Handle<FixedArray> export_wrappers(module_object_->export_wrappers(),
                                     isolate_);
  if (NeedsWrappers()) {
    // Fill the table to cache the exported JSFunction wrappers.
    js_wrappers_.insert(js_wrappers_.begin(), module_->functions.size(),
                        Handle<JSFunction>::null());

    // If an imported WebAssembly function gets exported, the exported function
    // has to be identical to to imported function. Therefore we put all
    // imported WebAssembly functions into the js_wrappers_ list.
    for (int index = 0, end = static_cast<int>(module_->import_table.size());
         index < end; ++index) {
      const WasmImport& import = module_->import_table[index];
      if (import.kind == kExternalFunction) {
        Handle<Object> value = sanitized_imports_[index].value;
        if (WasmExportedFunction::IsWasmExportedFunction(*value)) {
          js_wrappers_[import.index] = Handle<JSFunction>::cast(value);
        }
      }
    }
  }

  Handle<JSObject> exports_object;
  bool is_asm_js = false;
  switch (module_->origin) {
    case kWasmOrigin: {
      // Create the "exports" object.
      exports_object = isolate_->factory()->NewJSObjectWithNullProto();
      break;
    }
    case kAsmJsOrigin: {
      Handle<JSFunction> object_function = Handle<JSFunction>(
          isolate_->native_context()->object_function(), isolate_);
      exports_object = isolate_->factory()->NewJSObject(object_function);
      is_asm_js = true;
      break;
    }
    default:
      UNREACHABLE();
  }
  instance->set_exports_object(*exports_object);

  Handle<String> single_function_name =
      isolate_->factory()->InternalizeUtf8String(AsmJs::kSingleFunctionName);

  PropertyDescriptor desc;
  desc.set_writable(is_asm_js);
  desc.set_enumerable(true);
  desc.set_configurable(is_asm_js);

  // Process each export in the export table.
  int export_index = 0;  // Index into {export_wrappers}.
  for (const WasmExport& exp : module_->export_table) {
    Handle<String> name = WasmModuleObject::ExtractUtf8StringFromModuleBytes(
                              isolate_, module_object_, exp.name)
                              .ToHandleChecked();
    Handle<JSObject> export_to;
    if (is_asm_js && exp.kind == kExternalFunction &&
        String::Equals(isolate_, name, single_function_name)) {
      export_to = instance;
    } else {
      export_to = exports_object;
    }

    switch (exp.kind) {
      case kExternalFunction: {
        // Wrap and export the code as a JSFunction.
        const WasmFunction& function = module_->functions[exp.index];
        Handle<JSFunction> js_function = js_wrappers_[exp.index];
        if (js_function.is_null()) {
          // Wrap the exported code as a JSFunction.
          Handle<Code> export_code =
              export_wrappers->GetValueChecked<Code>(isolate_, export_index);
          MaybeHandle<String> func_name;
          if (is_asm_js) {
            // For modules arising from asm.js, honor the names section.
            WireBytesRef func_name_ref = module_->LookupFunctionName(
                module_object_->native_module()->wire_bytes(),
                function.func_index);
            func_name = WasmModuleObject::ExtractUtf8StringFromModuleBytes(
                            isolate_, module_object_, func_name_ref)
                            .ToHandleChecked();
          }
          js_function = WasmExportedFunction::New(
              isolate_, instance, func_name, function.func_index,
              static_cast<int>(function.sig->parameter_count()), export_code);
          js_wrappers_[exp.index] = js_function;
        }
        desc.set_value(js_function);
        export_index++;
        break;
      }
      case kExternalTable: {
        // Export a table as a WebAssembly.Table object.
        TableInstance& table_instance = table_instances_[exp.index];
        const WasmTable& table = module_->tables[exp.index];
        if (table_instance.table_object.is_null()) {
          uint32_t maximum = table.has_maximum_size ? table.maximum_size
                                                    : FLAG_wasm_max_table_size;
          table_instance.table_object =
              WasmTableObject::New(isolate_, table.initial_size, maximum,
                                   &table_instance.js_wrappers);
        }
        desc.set_value(table_instance.table_object);
        break;
      }
      case kExternalMemory: {
        // Export the memory as a WebAssembly.Memory object. A WasmMemoryObject
        // should already be available if the module has memory, since we always
        // create or import it when building an WasmInstanceObject.
        DCHECK(instance->has_memory_object());
        desc.set_value(
            Handle<WasmMemoryObject>(instance->memory_object(), isolate_));
        break;
      }
      case kExternalGlobal: {
        const WasmGlobal& global = module_->globals[exp.index];
        if (enabled_.mut_global) {
          Handle<JSArrayBuffer> buffer;
          uint32_t offset;

          if (global.mutability && global.imported) {
            Handle<FixedArray> buffers_array(
                instance->imported_mutable_globals_buffers(), isolate_);
            buffer = buffers_array->GetValueChecked<JSArrayBuffer>(
                isolate_, global.index);
            Address global_addr =
                instance->imported_mutable_globals()[global.index];

            size_t buffer_size = buffer->byte_length();
            Address backing_store =
                reinterpret_cast<Address>(buffer->backing_store());
            CHECK(global_addr >= backing_store &&
                  global_addr < backing_store + buffer_size);
            offset = static_cast<uint32_t>(global_addr - backing_store);
          } else {
            buffer = handle(instance->globals_buffer(), isolate_);
            offset = global.offset;
          }

          // Since the global's array buffer is always provided, allocation
          // should never fail.
          Handle<WasmGlobalObject> global_obj =
              WasmGlobalObject::New(isolate_, buffer, global.type, offset,
                                    global.mutability)
                  .ToHandleChecked();
          desc.set_value(global_obj);
        } else {
          // Export the value of the global variable as a number.
          double num = 0;
          switch (global.type) {
            case kWasmI32:
              num = ReadLittleEndianValue<int32_t>(
                  GetRawGlobalPtr<int32_t>(global));
              break;
            case kWasmF32:
              num =
                  ReadLittleEndianValue<float>(GetRawGlobalPtr<float>(global));
              break;
            case kWasmF64:
              num = ReadLittleEndianValue<double>(
                  GetRawGlobalPtr<double>(global));
              break;
            case kWasmI64:
              thrower_->LinkError(
                  "export of globals of type I64 is not allowed.");
              return;
            default:
              UNREACHABLE();
          }
          desc.set_value(isolate_->factory()->NewNumber(num));
        }
        break;
      }
      case kExternalException: {
        const WasmException& exception = module_->exceptions[exp.index];
        Handle<WasmExceptionObject> wrapper = exception_wrappers_[exp.index];
        if (wrapper.is_null()) {
          Handle<HeapObject> exception_tag(
              HeapObject::cast(instance->exceptions_table()->get(exp.index)),
              isolate_);
          wrapper =
              WasmExceptionObject::New(isolate_, exception.sig, exception_tag);
          exception_wrappers_[exp.index] = wrapper;
        }
        desc.set_value(wrapper);
        break;
      }
      default:
        UNREACHABLE();
        break;
    }

    v8::Maybe<bool> status = JSReceiver::DefineOwnProperty(
        isolate_, export_to, name, &desc, kThrowOnError);
    if (!status.IsJust()) {
      TruncatedUserString<> trunc_name(name->GetCharVector<uint8_t>());
      thrower_->LinkError("export of %.*s failed.", trunc_name.length(),
                          trunc_name.start());
      return;
    }
  }
  DCHECK_EQ(export_index, export_wrappers->length());

  if (module_->origin == kWasmOrigin) {
    v8::Maybe<bool> success =
        JSReceiver::SetIntegrityLevel(exports_object, FROZEN, kDontThrow);
    DCHECK(success.FromMaybe(false));
    USE(success);
  }
}

void InstanceBuilder::InitializeTables(Handle<WasmInstanceObject> instance) {
  size_t table_count = module_->tables.size();
  for (size_t index = 0; index < table_count; ++index) {
    const WasmTable& table = module_->tables[index];
    TableInstance& table_instance = table_instances_[index];

    if (!instance->has_indirect_function_table() &&
        table.type == kWasmAnyFunc) {
      WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
          instance, table.initial_size);
      table_instance.table_size = table.initial_size;
    }
  }
}

void InstanceBuilder::LoadTableSegments(Handle<WasmInstanceObject> instance) {
  NativeModule* native_module = module_object_->native_module();
  for (auto& table_init : module_->table_inits) {
    uint32_t base = EvalUint32InitExpr(table_init.offset);
    uint32_t num_entries = static_cast<uint32_t>(table_init.entries.size());
    uint32_t index = table_init.table_index;
    TableInstance& table_instance = table_instances_[index];
    DCHECK(in_bounds(base, num_entries, table_instance.table_size));
    for (uint32_t i = 0; i < num_entries; ++i) {
      uint32_t func_index = table_init.entries[i];
      const WasmFunction* function = &module_->functions[func_index];
      int table_index = static_cast<int>(i + base);

      // Update the local dispatch table first.
      uint32_t sig_id = module_->signature_ids[function->sig_index];
      Handle<WasmInstanceObject> target_instance = instance;
      Address call_target;
      const bool is_import = func_index < module_->num_imported_functions;
      if (is_import) {
        // For imported calls, take target instance and address from the
        // import table.
        ImportedFunctionEntry entry(instance, func_index);
        target_instance = handle(entry.instance(), isolate_);
        call_target = entry.target();
      } else {
        call_target = native_module->GetCallTargetForFunction(func_index);
      }
      IndirectFunctionTableEntry(instance, table_index)
          .set(sig_id, *target_instance, call_target);

      if (!table_instance.table_object.is_null()) {
        // Update the table object's other dispatch tables.
        if (js_wrappers_[func_index].is_null()) {
          // No JSFunction entry yet exists for this function. Create one.
          // TODO(titzer): We compile JS->wasm wrappers for functions are
          // not exported but are in an exported table. This should be done
          // at module compile time and cached instead.

          Handle<Code> wrapper_code =
              js_to_wasm_cache_.GetOrCompileJSToWasmWrapper(
                  isolate_, function->sig, function->imported);
          MaybeHandle<String> func_name;
          if (module_->origin == kAsmJsOrigin) {
            // For modules arising from asm.js, honor the names section.
            WireBytesRef func_name_ref = module_->LookupFunctionName(
                native_module->wire_bytes(), func_index);
            func_name = WasmModuleObject::ExtractUtf8StringFromModuleBytes(
                            isolate_, module_object_, func_name_ref)
                            .ToHandleChecked();
          }
          Handle<WasmExportedFunction> js_function = WasmExportedFunction::New(
              isolate_, instance, func_name, func_index,
              static_cast<int>(function->sig->parameter_count()), wrapper_code);
          js_wrappers_[func_index] = js_function;
        }
        table_instance.js_wrappers->set(table_index, *js_wrappers_[func_index]);
        // UpdateDispatchTables() updates all other dispatch tables, since
        // we have not yet added the dispatch table we are currently building.
        WasmTableObject::UpdateDispatchTables(
            isolate_, table_instance.table_object, table_index, function->sig,
            target_instance, call_target);
      }
    }
  }

  int table_count = static_cast<int>(module_->tables.size());
  for (int index = 0; index < table_count; ++index) {
    TableInstance& table_instance = table_instances_[index];

    // Add the new dispatch table at the end to avoid redundant lookups.
    if (!table_instance.table_object.is_null()) {
      WasmTableObject::AddDispatchTable(isolate_, table_instance.table_object,
                                        instance, index);
    }
  }
}

void InstanceBuilder::InitializeExceptions(
    Handle<WasmInstanceObject> instance) {
  Handle<FixedArray> exceptions_table(instance->exceptions_table(), isolate_);
  for (int index = 0; index < exceptions_table->length(); ++index) {
    if (!exceptions_table->get(index)->IsUndefined(isolate_)) continue;
    // TODO(mstarzinger): Tags provide an object identity for each exception,
    // using {JSObject} here is gigantic hack and we should use a dedicated
    // object with a much lighter footprint for this purpose here.
    Handle<HeapObject> exception_tag =
        isolate_->factory()->NewJSObjectWithNullProto();
    exceptions_table->set(index, *exception_tag);
  }
}

AsyncCompileJob::AsyncCompileJob(
    Isolate* isolate, const WasmFeatures& enabled,
    std::unique_ptr<byte[]> bytes_copy, size_t length, Handle<Context> context,
    std::shared_ptr<CompilationResultResolver> resolver)
    : isolate_(isolate),
      enabled_features_(enabled),
      async_counters_(isolate->async_counters()),
      bytes_copy_(std::move(bytes_copy)),
      wire_bytes_(bytes_copy_.get(), bytes_copy_.get() + length),
      resolver_(std::move(resolver)) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::Platform* platform = V8::GetCurrentPlatform();
  foreground_task_runner_ = platform->GetForegroundTaskRunner(v8_isolate);
  // The handle for the context must be deferred.
  DeferredHandleScope deferred(isolate);
  native_context_ = Handle<Context>(context->native_context(), isolate);
  DCHECK(native_context_->IsNativeContext());
  deferred_handles_.push_back(deferred.Detach());
}

void AsyncCompileJob::Start() {
  DoAsync<DecodeModule>();  // --
}

void AsyncCompileJob::Abort() {
  // Removing this job will trigger the destructor, which will cancel all
  // compilation.
  isolate_->wasm_engine()->RemoveCompileJob(this);
}

class AsyncStreamingProcessor final : public StreamingProcessor {
 public:
  explicit AsyncStreamingProcessor(AsyncCompileJob* job);

  bool ProcessModuleHeader(Vector<const uint8_t> bytes,
                           uint32_t offset) override;

  bool ProcessSection(SectionCode section_code, Vector<const uint8_t> bytes,
                      uint32_t offset) override;

  bool ProcessCodeSectionHeader(size_t functions_count,
                                uint32_t offset) override;

  bool ProcessFunctionBody(Vector<const uint8_t> bytes,
                           uint32_t offset) override;

  void OnFinishedChunk() override;

  void OnFinishedStream(OwnedVector<uint8_t> bytes) override;

  void OnError(DecodeResult result) override;

  void OnAbort() override;

 private:
  // Finishes the AsyncCompileJob with an error.
  void FinishAsyncCompileJobWithError(ResultBase result);

  void CommitCompilationUnits();

  ModuleDecoder decoder_;
  AsyncCompileJob* job_;
  std::unique_ptr<CompilationUnitBuilder> compilation_unit_builder_;
  uint32_t next_function_ = 0;
};

std::shared_ptr<StreamingDecoder> AsyncCompileJob::CreateStreamingDecoder() {
  DCHECK_NULL(stream_);
  stream_.reset(
      new StreamingDecoder(base::make_unique<AsyncStreamingProcessor>(this)));
  return stream_;
}

AsyncCompileJob::~AsyncCompileJob() {
  background_task_manager_.CancelAndWait();
  if (native_module_) native_module_->compilation_state()->Abort();
  // Tell the streaming decoder that the AsyncCompileJob is not available
  // anymore.
  // TODO(ahaas): Is this notification really necessary? Check
  // https://crbug.com/888170.
  if (stream_) stream_->NotifyCompilationEnded();
  CancelPendingForegroundTask();
  for (auto d : deferred_handles_) delete d;
}

// This function assumes that it is executed in a HandleScope, and that a
// context is set on the isolate.
void AsyncCompileJob::FinishCompile() {
  DCHECK_NOT_NULL(isolate_->context());
  // Finish the wasm script now and make it public to the debugger.
  Handle<Script> script(module_object_->script(), isolate_);
  if (script->type() == Script::TYPE_WASM &&
      module_object_->module()->source_map_url.size() != 0) {
    MaybeHandle<String> src_map_str = isolate_->factory()->NewStringFromUtf8(
        CStrVector(module_object_->module()->source_map_url.c_str()), TENURED);
    script->set_source_mapping_url(*src_map_str.ToHandleChecked());
  }
  isolate_->debug()->OnAfterCompile(script);

  // Log the code within the generated module for profiling.
  native_module_->LogWasmCodes(isolate_);

  // We can only update the feature counts once the entire compile is done.
  auto compilation_state = native_module_->compilation_state();
  compilation_state->PublishDetectedFeatures(
      isolate_, *compilation_state->detected_features());

  // TODO(wasm): compiling wrappers should be made async as well.
  DoSync<CompileWrappers>();
}

void AsyncCompileJob::AsyncCompileFailed(Handle<Object> error_reason) {
  // {job} keeps the {this} pointer alive.
  std::shared_ptr<AsyncCompileJob> job =
      isolate_->wasm_engine()->RemoveCompileJob(this);
  resolver_->OnCompilationFailed(error_reason);
}

void AsyncCompileJob::AsyncCompileSucceeded(Handle<WasmModuleObject> result) {
  resolver_->OnCompilationSucceeded(result);
}

// A closure to run a compilation step (either as foreground or background
// task) and schedule the next step(s), if any.
class AsyncCompileJob::CompileStep {
 public:
  virtual ~CompileStep() = default;

  void Run(bool on_foreground) {
    if (on_foreground) {
      HandleScope scope(job_->isolate_);
      SaveContext saved_context(job_->isolate_);
      job_->isolate_->set_context(*job_->native_context_);
      RunInForeground();
    } else {
      RunInBackground();
    }
  }

  virtual void RunInForeground() { UNREACHABLE(); }
  virtual void RunInBackground() { UNREACHABLE(); }

  AsyncCompileJob* job_ = nullptr;
};

class AsyncCompileJob::CompileTask : public CancelableTask {
 public:
  CompileTask(AsyncCompileJob* job, bool on_foreground)
      // We only manage the background tasks with the {CancelableTaskManager} of
      // the {AsyncCompileJob}. Foreground tasks are managed by the system's
      // {CancelableTaskManager}. Background tasks cannot spawn tasks managed by
      // their own task manager.
      : CancelableTask(on_foreground ? job->isolate_->cancelable_task_manager()
                                     : &job->background_task_manager_),
        job_(job),
        on_foreground_(on_foreground) {}

  ~CompileTask() override {
    if (job_ != nullptr && on_foreground_) ResetPendingForegroundTask();
  }

  void RunInternal() final {
    if (!job_) return;
    if (on_foreground_) ResetPendingForegroundTask();
    job_->step_->Run(on_foreground_);
    // After execution, reset {job_} such that we don't try to reset the pending
    // foreground task when the task is deleted.
    job_ = nullptr;
  }

  void Cancel() {
    DCHECK_NOT_NULL(job_);
    job_ = nullptr;
  }

 private:
  // {job_} will be cleared to cancel a pending task.
  AsyncCompileJob* job_;
  bool on_foreground_;

  void ResetPendingForegroundTask() const {
    DCHECK_EQ(this, job_->pending_foreground_task_);
    job_->pending_foreground_task_ = nullptr;
  }
};

void AsyncCompileJob::StartForegroundTask() {
  DCHECK_NULL(pending_foreground_task_);

  auto new_task = base::make_unique<CompileTask>(this, true);
  pending_foreground_task_ = new_task.get();
  foreground_task_runner_->PostTask(std::move(new_task));
}

void AsyncCompileJob::ExecuteForegroundTaskImmediately() {
  DCHECK_NULL(pending_foreground_task_);

  auto new_task = base::make_unique<CompileTask>(this, true);
  pending_foreground_task_ = new_task.get();
  new_task->Run();
}

void AsyncCompileJob::CancelPendingForegroundTask() {
  if (!pending_foreground_task_) return;
  pending_foreground_task_->Cancel();
  pending_foreground_task_ = nullptr;
}

void AsyncCompileJob::StartBackgroundTask() {
  auto task = base::make_unique<CompileTask>(this, false);

  // If --wasm-num-compilation-tasks=0 is passed, do only spawn foreground
  // tasks. This is used to make timing deterministic.
  if (FLAG_wasm_num_compilation_tasks > 0) {
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
  } else {
    foreground_task_runner_->PostTask(std::move(task));
  }
}

template <typename Step, typename... Args>
void AsyncCompileJob::DoSync(Args&&... args) {
  NextStep<Step>(std::forward<Args>(args)...);
  StartForegroundTask();
}

template <typename Step, typename... Args>
void AsyncCompileJob::DoImmediately(Args&&... args) {
  NextStep<Step>(std::forward<Args>(args)...);
  ExecuteForegroundTaskImmediately();
}

template <typename Step, typename... Args>
void AsyncCompileJob::DoAsync(Args&&... args) {
  NextStep<Step>(std::forward<Args>(args)...);
  StartBackgroundTask();
}

template <typename Step, typename... Args>
void AsyncCompileJob::NextStep(Args&&... args) {
  step_.reset(new Step(std::forward<Args>(args)...));
  step_->job_ = this;
}

//==========================================================================
// Step 1: (async) Decode the module.
//==========================================================================
class AsyncCompileJob::DecodeModule : public AsyncCompileJob::CompileStep {
 public:
  void RunInBackground() override {
    ModuleResult result;
    {
      DisallowHandleAllocation no_handle;
      DisallowHeapAllocation no_allocation;
      // Decode the module bytes.
      TRACE_COMPILE("(1) Decoding module...\n");
      result =
          DecodeWasmModule(job_->enabled_features_, job_->wire_bytes_.start(),
                           job_->wire_bytes_.end(), false, kWasmOrigin,
                           job_->async_counters().get(),
                           job_->isolate()->wasm_engine()->allocator());
    }
    if (result.failed()) {
      // Decoding failure; reject the promise and clean up.
      job_->DoSync<DecodeFail>(std::move(result));
    } else {
      // Decode passed.
      job_->DoSync<PrepareAndStartCompile>(std::move(result.val), true);
    }
  }
};

//==========================================================================
// Step 1b: (sync) Fail decoding the module.
//==========================================================================
class AsyncCompileJob::DecodeFail : public CompileStep {
 public:
  explicit DecodeFail(ModuleResult result) : result_(std::move(result)) {}

 private:
  ModuleResult result_;
  void RunInForeground() override {
    TRACE_COMPILE("(1b) Decoding failed.\n");
    ErrorThrower thrower(job_->isolate_, "AsyncCompile");
    thrower.CompileFailed("Wasm decoding failed", result_);
    // {job_} is deleted in AsyncCompileFailed, therefore the {return}.
    return job_->AsyncCompileFailed(thrower.Reify());
  }
};

//==========================================================================
// Step 2 (sync): Create heap-allocated data and start compile.
//==========================================================================
class AsyncCompileJob::PrepareAndStartCompile : public CompileStep {
 public:
  PrepareAndStartCompile(std::shared_ptr<const WasmModule> module,
                         bool start_compilation)
      : module_(std::move(module)), start_compilation_(start_compilation) {}

 private:
  std::shared_ptr<const WasmModule> module_;
  bool start_compilation_;

  void RunInForeground() override {
    TRACE_COMPILE("(2) Prepare and start compile...\n");

    // Make sure all compilation tasks stopped running. Decoding (async step)
    // is done.
    job_->background_task_manager_.CancelAndWait();

    // Embedder usage count for declared shared memories.
    if (module_->has_shared_memory) {
      job_->isolate_->CountUsage(
          v8::Isolate::UseCounterFeature::kWasmSharedMemory);
    }

    // Create heap objects for script and module bytes to be stored in the
    // module object. Asm.js is not compiled asynchronously.
    const WasmModule* module = module_.get();
    Handle<Script> script = CreateWasmScript(job_->isolate_, job_->wire_bytes_,
                                             module->source_map_url);
    Handle<ByteArray> asm_js_offset_table;

    ModuleEnv env = CreateDefaultModuleEnv(module);
    // TODO(wasm): Improve efficiency of storing module wire bytes. Only store
    // relevant sections, not function bodies

    // Create the module object and populate with compiled functions and
    // information needed at instantiation time.
    // TODO(clemensh): For the same module (same bytes / same hash), we should
    // only have one {WasmModuleObject}. Otherwise, we might only set
    // breakpoints on a (potentially empty) subset of the instances.
    // Create the module object.
    job_->module_object_ = WasmModuleObject::New(
        job_->isolate_, job_->enabled_features_, module_, env,
        {std::move(job_->bytes_copy_), job_->wire_bytes_.length()}, script,
        asm_js_offset_table);
    job_->native_module_ = job_->module_object_->native_module();

    {
      DeferredHandleScope deferred(job_->isolate_);
      job_->module_object_ = handle(*job_->module_object_, job_->isolate_);
      job_->deferred_handles_.push_back(deferred.Detach());
    }
    size_t num_functions =
        module->functions.size() - module->num_imported_functions;

    if (num_functions == 0) {
      // Tiering has nothing to do if module is empty.
      job_->tiering_completed_ = true;

      // Degenerate case of an empty module.
      job_->FinishCompile();
      return;
    }

    CompilationState* compilation_state =
        job_->native_module_->compilation_state();
    {
      // Instance field {job_} cannot be captured by copy, therefore
      // we need to add a local helper variable {job}. We want to
      // capture the {job} pointer by copy, as it otherwise is dependent
      // on the current step we are in.
      AsyncCompileJob* job = job_;
      compilation_state->SetCallback(
          [job](CompilationEvent event, ErrorThrower* thrower) {
            // Callback is called from a foreground thread.
            switch (event) {
              case CompilationEvent::kFinishedBaselineCompilation:
                if (job->DecrementAndCheckFinisherCount()) {
                  SaveContext saved_context(job->isolate());
                  job->isolate()->set_context(*job->native_context_);
                  job->FinishCompile();
                }
                return;
              case CompilationEvent::kFinishedTopTierCompilation:
                // If a foreground task or a finisher is pending, we rely on
                // FinishModule to remove the job.
                if (job->pending_foreground_task_ ||
                    job->outstanding_finishers_.load() > 0) {
                  job->tiering_completed_ = true;
                  return;
                }
                job->isolate_->wasm_engine()->RemoveCompileJob(job);
                return;
              case CompilationEvent::kFailedCompilation: {
                // Tier-up compilation should not fail if baseline compilation
                // did not fail.
                DCHECK(!job->native_module_->compilation_state()
                            ->baseline_compilation_finished());

                SaveContext saved_context(job->isolate());
                job->isolate()->set_context(*job->native_context_);
                Handle<Object> error = thrower->Reify();

                DeferredHandleScope deferred(job->isolate());
                error = handle(*error, job->isolate());
                job->deferred_handles_.push_back(deferred.Detach());
                job->DoSync<CompileFailed>(error);
                return;
              }
            }
            UNREACHABLE();
          });
    }
    if (start_compilation_) {
      // TODO(ahaas): Try to remove the {start_compilation_} check when
      // streaming decoding is done in the background. If
      // InitializeCompilationUnits always returns 0 for streaming compilation,
      // then DoAsync would do the same as NextStep already.

      compilation_state->SetNumberOfFunctionsToCompile(
          module->num_declared_functions);
      // Add compilation units and kick off compilation.
      InitializeCompilationUnits(job_->native_module_);
    }
  }
};

//==========================================================================
// Step 4b (sync): Compilation failed. Reject Promise.
//==========================================================================
class AsyncCompileJob::CompileFailed : public CompileStep {
 public:
  explicit CompileFailed(Handle<Object> error_reason)
      : error_reason_(error_reason) {}

  void RunInForeground() override {
    TRACE_COMPILE("(4b) Compilation Failed...\n");
    return job_->AsyncCompileFailed(error_reason_);
  }

 private:
  Handle<Object> error_reason_;
};

//==========================================================================
// Step 5 (sync): Compile JS->wasm wrappers.
//==========================================================================
class AsyncCompileJob::CompileWrappers : public CompileStep {
  // TODO(wasm): Compile all wrappers here, including the start function wrapper
  // and the wrappers for the function table elements.
  void RunInForeground() override {
    TRACE_COMPILE("(5) Compile wrappers...\n");
    // TODO(6792): No longer needed once WebAssembly code is off heap.
    CodeSpaceMemoryModificationScope modification_scope(job_->isolate_->heap());
    // Compile JS->wasm wrappers for exported functions.
    CompileJsToWasmWrappers(job_->isolate_, job_->module_object_);
    job_->DoSync<FinishModule>();
  }
};

//==========================================================================
// Step 6 (sync): Finish the module and resolve the promise.
//==========================================================================
class AsyncCompileJob::FinishModule : public CompileStep {
  void RunInForeground() override {
    TRACE_COMPILE("(6) Finish module...\n");
    job_->AsyncCompileSucceeded(job_->module_object_);

    size_t num_functions = job_->native_module_->num_functions() -
                           job_->native_module_->num_imported_functions();
    if (job_->native_module_->compilation_state()->compile_mode() ==
            CompileMode::kRegular ||
        num_functions == 0) {
      // If we do not tier up, the async compile job is done here and
      // can be deleted.
      job_->isolate_->wasm_engine()->RemoveCompileJob(job_);
      return;
    }
    // If background tiering compilation finished before we resolved the
    // promise, switch to patching now. Otherwise, patching will be scheduled
    // by a callback.
    DCHECK_EQ(CompileMode::kTiering,
              job_->native_module_->compilation_state()->compile_mode());
    if (job_->tiering_completed_) {
      job_->isolate_->wasm_engine()->RemoveCompileJob(job_);
    }
  }
};

AsyncStreamingProcessor::AsyncStreamingProcessor(AsyncCompileJob* job)
    : decoder_(job->enabled_features_),
      job_(job),
      compilation_unit_builder_(nullptr) {}

void AsyncStreamingProcessor::FinishAsyncCompileJobWithError(ResultBase error) {
  // Make sure all background tasks stopped executing before we change the state
  // of the AsyncCompileJob to DecodeFail.
  job_->background_task_manager_.CancelAndWait();

  // Create a ModuleResult from the result we got as parameter. Since there was
  // no error, we don't have to provide a real wasm module to the ModuleResult.
  ModuleResult result(nullptr);
  result.MoveErrorFrom(error);

  // Check if there is already a CompiledModule, in which case we have to clean
  // up the CompilationState as well.
  if (job_->native_module_) {
    job_->native_module_->compilation_state()->Abort();

    if (job_->pending_foreground_task_ == nullptr) {
      job_->DoSync<AsyncCompileJob::DecodeFail>(std::move(result));
    } else {
      job_->NextStep<AsyncCompileJob::DecodeFail>(std::move(result));
    }

    // Clear the {compilation_unit_builder_} if it exists. This is needed
    // because there is a check in the destructor of the
    // {CompilationUnitBuilder} that it is empty.
    if (compilation_unit_builder_) compilation_unit_builder_->Clear();
  } else {
    job_->DoSync<AsyncCompileJob::DecodeFail>(std::move(result));
  }
}

// Process the module header.
bool AsyncStreamingProcessor::ProcessModuleHeader(Vector<const uint8_t> bytes,
                                                  uint32_t offset) {
  TRACE_STREAMING("Process module header...\n");
  decoder_.StartDecoding(job_->async_counters().get(),
                         job_->isolate()->wasm_engine()->allocator());
  decoder_.DecodeModuleHeader(bytes, offset);
  if (!decoder_.ok()) {
    FinishAsyncCompileJobWithError(decoder_.FinishDecoding(false));
    return false;
  }
  return true;
}

// Process all sections except for the code section.
bool AsyncStreamingProcessor::ProcessSection(SectionCode section_code,
                                             Vector<const uint8_t> bytes,
                                             uint32_t offset) {
  TRACE_STREAMING("Process section %d ...\n", section_code);
  if (compilation_unit_builder_) {
    // We reached a section after the code section, we do not need the
    // compilation_unit_builder_ anymore.
    CommitCompilationUnits();
    compilation_unit_builder_.reset();
  }
  if (section_code == SectionCode::kUnknownSectionCode) {
    Decoder decoder(bytes, offset);
    section_code = ModuleDecoder::IdentifyUnknownSection(
        decoder, bytes.start() + bytes.length());
    if (section_code == SectionCode::kUnknownSectionCode) {
      // Skip unknown sections that we do not know how to handle.
      return true;
    }
    // Remove the unknown section tag from the payload bytes.
    offset += decoder.position();
    bytes = bytes.SubVector(decoder.position(), bytes.size());
  }
  constexpr bool verify_functions = false;
  decoder_.DecodeSection(section_code, bytes, offset, verify_functions);
  if (!decoder_.ok()) {
    FinishAsyncCompileJobWithError(decoder_.FinishDecoding(false));
    return false;
  }
  return true;
}

// Start the code section.
bool AsyncStreamingProcessor::ProcessCodeSectionHeader(size_t functions_count,
                                                       uint32_t offset) {
  TRACE_STREAMING("Start the code section with %zu functions...\n",
                  functions_count);
  if (!decoder_.CheckFunctionsCount(static_cast<uint32_t>(functions_count),
                                    offset)) {
    FinishAsyncCompileJobWithError(decoder_.FinishDecoding(false));
    return false;
  }
  // Execute the PrepareAndStartCompile step immediately and not in a separate
  // task.
  job_->DoImmediately<AsyncCompileJob::PrepareAndStartCompile>(
      decoder_.shared_module(), false);

  job_->native_module_->compilation_state()->SetNumberOfFunctionsToCompile(
      functions_count);

  // Set outstanding_finishers_ to 2, because both the AsyncCompileJob and the
  // AsyncStreamingProcessor have to finish.
  job_->outstanding_finishers_.store(2);
  compilation_unit_builder_.reset(
      new CompilationUnitBuilder(job_->native_module_));
  return true;
}

// Process a function body.
bool AsyncStreamingProcessor::ProcessFunctionBody(Vector<const uint8_t> bytes,
                                                  uint32_t offset) {
  TRACE_STREAMING("Process function body %d ...\n", next_function_);

  decoder_.DecodeFunctionBody(
      next_function_, static_cast<uint32_t>(bytes.length()), offset, false);

  uint32_t index = next_function_ + decoder_.module()->num_imported_functions;
  const WasmFunction* func = &decoder_.module()->functions[index];
  compilation_unit_builder_->AddUnit(func, offset, bytes);
  ++next_function_;
  // This method always succeeds. The return value is necessary to comply with
  // the StreamingProcessor interface.
  return true;
}

void AsyncStreamingProcessor::CommitCompilationUnits() {
  DCHECK(compilation_unit_builder_);
  compilation_unit_builder_->Commit();
}

void AsyncStreamingProcessor::OnFinishedChunk() {
  TRACE_STREAMING("FinishChunk...\n");
  if (compilation_unit_builder_) CommitCompilationUnits();
}

// Finish the processing of the stream.
void AsyncStreamingProcessor::OnFinishedStream(OwnedVector<uint8_t> bytes) {
  TRACE_STREAMING("Finish stream...\n");
  ModuleResult result = decoder_.FinishDecoding(false);
  DCHECK(result.ok());
  bool needs_finish = job_->DecrementAndCheckFinisherCount();
  if (job_->native_module_ == nullptr) {
    // We are processing a WebAssembly module without code section. We need to
    // prepare compilation first before we can finish it.
    // {PrepareAndStartCompile} will call {FinishCompile} by itself if there
    // is no code section.
    DCHECK(needs_finish);
    needs_finish = false;
    job_->DoImmediately<AsyncCompileJob::PrepareAndStartCompile>(result.val,
                                                                 true);
  }
  job_->wire_bytes_ = ModuleWireBytes(bytes.as_vector());
  job_->native_module_->set_wire_bytes(std::move(bytes));
  if (needs_finish) {
    HandleScope scope(job_->isolate_);
    SaveContext saved_context(job_->isolate_);
    job_->isolate_->set_context(*job_->native_context_);
    job_->FinishCompile();
  }
}

// Report an error detected in the StreamingDecoder.
void AsyncStreamingProcessor::OnError(DecodeResult result) {
  TRACE_STREAMING("Stream error...\n");
  FinishAsyncCompileJobWithError(std::move(result));
}

void AsyncStreamingProcessor::OnAbort() {
  TRACE_STREAMING("Abort stream...\n");
  job_->Abort();
}

void CompilationStateDeleter::operator()(
    CompilationState* compilation_state) const {
  delete compilation_state;
}

std::unique_ptr<CompilationState, CompilationStateDeleter> NewCompilationState(
    Isolate* isolate, const ModuleEnv& env) {
  return std::unique_ptr<CompilationState, CompilationStateDeleter>(
      new CompilationState(isolate, env));
}

ModuleEnv* GetModuleEnv(CompilationState* compilation_state) {
  return compilation_state->module_env();
}

CompilationState::CompilationState(internal::Isolate* isolate,
                                   const ModuleEnv& env)
    : isolate_(isolate),
      wasm_engine_(isolate->wasm_engine()),
      module_env_(env),
      compile_mode_(FLAG_wasm_tier_up && env.module->origin == kWasmOrigin
                        ? CompileMode::kTiering
                        : CompileMode::kRegular),
      max_background_tasks_(std::max(
          1, std::min(FLAG_wasm_num_compilation_tasks,
                      V8::GetCurrentPlatform()->NumberOfWorkerThreads()))) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  v8::Platform* platform = V8::GetCurrentPlatform();
  foreground_task_runner_ = platform->GetForegroundTaskRunner(v8_isolate);
}

CompilationState::~CompilationState() {
  background_task_manager_.CancelAndWait();
  foreground_task_manager_.CancelAndWait();
}

void CompilationState::SetNumberOfFunctionsToCompile(size_t num_functions) {
  DCHECK(!failed());
  outstanding_units_ = num_functions;

  if (compile_mode_ == CompileMode::kTiering) {
    outstanding_units_ += num_functions;
    num_tiering_units_ = num_functions;
  }
}

void CompilationState::SetCallback(
    std::function<void(CompilationEvent, ErrorThrower*)> callback) {
  DCHECK_NULL(callback_);
  callback_ = std::move(callback);
}

void CompilationState::AddCompilationUnits(
    std::vector<std::unique_ptr<WasmCompilationUnit>>& baseline_units,
    std::vector<std::unique_ptr<WasmCompilationUnit>>& tiering_units) {
  {
    base::LockGuard<base::Mutex> guard(&mutex_);

    if (compile_mode_ == CompileMode::kTiering) {
      DCHECK_EQ(baseline_units.size(), tiering_units.size());
      DCHECK_EQ(tiering_units.back()->mode(), ExecutionTier::kOptimized);
      tiering_compilation_units_.insert(
          tiering_compilation_units_.end(),
          std::make_move_iterator(tiering_units.begin()),
          std::make_move_iterator(tiering_units.end()));
    } else {
      DCHECK(tiering_compilation_units_.empty());
    }

    baseline_compilation_units_.insert(
        baseline_compilation_units_.end(),
        std::make_move_iterator(baseline_units.begin()),
        std::make_move_iterator(baseline_units.end()));
  }

  RestartBackgroundTasks();
}

std::unique_ptr<WasmCompilationUnit>
CompilationState::GetNextCompilationUnit() {
  base::LockGuard<base::Mutex> guard(&mutex_);

  std::vector<std::unique_ptr<WasmCompilationUnit>>& units =
      baseline_compilation_units_.empty() ? tiering_compilation_units_
                                          : baseline_compilation_units_;

  if (!units.empty()) {
    std::unique_ptr<WasmCompilationUnit> unit = std::move(units.back());
    units.pop_back();
    return unit;
  }

  return std::unique_ptr<WasmCompilationUnit>();
}

std::unique_ptr<WasmCompilationUnit> CompilationState::GetNextExecutedUnit() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  std::vector<std::unique_ptr<WasmCompilationUnit>>& units = finish_units();
  if (units.empty()) return {};
  std::unique_ptr<WasmCompilationUnit> ret = std::move(units.back());
  units.pop_back();
  return ret;
}

bool CompilationState::HasCompilationUnitToFinish() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  return !finish_units().empty();
}

void CompilationState::OnError(ErrorThrower* thrower) {
  Abort();
  DCHECK(thrower->error());
  NotifyOnEvent(CompilationEvent::kFailedCompilation, thrower);
}

void CompilationState::OnFinishedUnit() {
  DCHECK_GT(outstanding_units_, 0);
  --outstanding_units_;

  if (outstanding_units_ == 0) {
    background_task_manager_.CancelAndWait();
    baseline_compilation_finished_ = true;

    DCHECK(compile_mode_ == CompileMode::kRegular ||
           compile_mode_ == CompileMode::kTiering);
    NotifyOnEvent(compile_mode_ == CompileMode::kRegular
                      ? CompilationEvent::kFinishedBaselineCompilation
                      : CompilationEvent::kFinishedTopTierCompilation,
                  nullptr);

  } else if (outstanding_units_ == num_tiering_units_) {
    DCHECK_EQ(compile_mode_, CompileMode::kTiering);
    baseline_compilation_finished_ = true;

    // TODO(wasm): For streaming compilation, we want to start top tier
    // compilation before all functions have been compiled with Liftoff, e.g.
    // in the case when all received functions have been compiled with Liftoff
    // and we are waiting for new functions to compile.

    // If we are in {kRegular} mode, {num_tiering_units_} is 0, therefore
    // this case is already caught by the previous check.
    NotifyOnEvent(CompilationEvent::kFinishedBaselineCompilation, nullptr);
    RestartBackgroundTasks();
  }
}

void CompilationState::ScheduleUnitForFinishing(
    std::unique_ptr<WasmCompilationUnit> unit, ExecutionTier mode) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  if (compile_mode_ == CompileMode::kTiering &&
      mode == ExecutionTier::kOptimized) {
    tiering_finish_units_.push_back(std::move(unit));
  } else {
    baseline_finish_units_.push_back(std::move(unit));
  }

  if (!finisher_is_running_ && !failed_) {
    ScheduleFinisherTask();
    // We set the flag here so that not more than one finisher is started.
    finisher_is_running_ = true;
  }
}

void CompilationState::OnBackgroundTaskStopped(const WasmFeatures& detected) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  DCHECK_LE(1, num_background_tasks_);
  --num_background_tasks_;
  UnionFeaturesInto(&detected_features_, detected);
}

void CompilationState::PublishDetectedFeatures(Isolate* isolate,
                                               const WasmFeatures& detected) {
  // Notifying the isolate of the feature counts must take place under
  // the mutex, because even if we have finished baseline compilation,
  // tiering compilations may still occur in the background.
  base::LockGuard<base::Mutex> guard(&mutex_);
  UnionFeaturesInto(&detected_features_, detected);
  UpdateFeatureUseCounts(isolate, detected_features_);
}

void CompilationState::RestartBackgroundTasks(size_t max) {
  size_t num_restart;
  {
    base::LockGuard<base::Mutex> guard(&mutex_);
    // No need to restart tasks if compilation already failed.
    if (failed_) return;

    DCHECK_LE(num_background_tasks_, max_background_tasks_);
    if (num_background_tasks_ == max_background_tasks_) return;
    size_t num_compilation_units =
        baseline_compilation_units_.size() + tiering_compilation_units_.size();
    size_t stopped_tasks = max_background_tasks_ - num_background_tasks_;
    num_restart = std::min(max, std::min(num_compilation_units, stopped_tasks));
    num_background_tasks_ += num_restart;
  }

  for (; num_restart > 0; --num_restart) {
    auto task = base::make_unique<BackgroundCompileTask>(
        this, &background_task_manager_);

    // If --wasm-num-compilation-tasks=0 is passed, do only spawn foreground
    // tasks. This is used to make timing deterministic.
    if (FLAG_wasm_num_compilation_tasks > 0) {
      V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
    } else {
      foreground_task_runner_->PostTask(std::move(task));
    }
  }
}

bool CompilationState::SetFinisherIsRunning(bool value) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  if (finisher_is_running_ == value) return false;
  finisher_is_running_ = value;
  return true;
}

void CompilationState::ScheduleFinisherTask() {
  foreground_task_runner_->PostTask(
      base::make_unique<FinishCompileTask>(this, &foreground_task_manager_));
}

void CompilationState::Abort() {
  {
    base::LockGuard<base::Mutex> guard(&mutex_);
    failed_ = true;
  }
  background_task_manager_.CancelAndWait();
}

void CompilationState::NotifyOnEvent(CompilationEvent event,
                                     ErrorThrower* thrower) {
  if (callback_) callback_(event, thrower);
}

void CompileJsToWasmWrappers(Isolate* isolate,
                             Handle<WasmModuleObject> module_object) {
  JSToWasmWrapperCache js_to_wasm_cache;
  int wrapper_index = 0;
  Handle<FixedArray> export_wrappers(module_object->export_wrappers(), isolate);
  NativeModule* native_module = module_object->native_module();
  const WasmModule* module = native_module->module();
  for (auto exp : module->export_table) {
    if (exp.kind != kExternalFunction) continue;
    auto& function = module->functions[exp.index];
    Handle<Code> wrapper_code = js_to_wasm_cache.GetOrCompileJSToWasmWrapper(
        isolate, function.sig, function.imported);
    export_wrappers->set(wrapper_index, *wrapper_code);
    RecordStats(*wrapper_code, isolate->counters());
    ++wrapper_index;
  }
}

Handle<Script> CreateWasmScript(Isolate* isolate,
                                const ModuleWireBytes& wire_bytes,
                                const std::string& source_map_url) {
  Handle<Script> script =
      isolate->factory()->NewScript(isolate->factory()->empty_string());
  script->set_context_data(isolate->native_context()->debug_context_id());
  script->set_type(Script::TYPE_WASM);

  int hash = StringHasher::HashSequentialString(
      reinterpret_cast<const char*>(wire_bytes.start()),
      static_cast<int>(wire_bytes.length()), kZeroHashSeed);

  const int kBufferSize = 32;
  char buffer[kBufferSize];

  int name_chars = SNPrintF(ArrayVector(buffer), "wasm-%08x", hash);
  DCHECK(name_chars >= 0 && name_chars < kBufferSize);
  MaybeHandle<String> name_str = isolate->factory()->NewStringFromOneByte(
      Vector<const uint8_t>(reinterpret_cast<uint8_t*>(buffer), name_chars),
      TENURED);
  script->set_name(*name_str.ToHandleChecked());

  if (source_map_url.size() != 0) {
    MaybeHandle<String> src_map_str = isolate->factory()->NewStringFromUtf8(
        CStrVector(source_map_url.c_str()), TENURED);
    script->set_source_mapping_url(*src_map_str.ToHandleChecked());
  }
  return script;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef TRACE
#undef TRACE_COMPILE
#undef TRACE_STREAMING
#undef TRACE_LAZY

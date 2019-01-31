// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-compiler.h"

#include "src/api.h"
#include "src/asmjs/asm-js.h"
#include "src/base/enum-set.h"
#include "src/base/optional.h"
#include "src/base/template-utils.h"
#include "src/base/utils/random-number-generator.h"
#include "src/compiler/wasm-compiler.h"
#include "src/counters.h"
#include "src/identity-map.h"
#include "src/property-descriptor.h"
#include "src/task-utils.h"
#include "src/tracing/trace-event.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/js-to-wasm-wrapper-cache-inl.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-import-wrapper-cache-inl.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-memory.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"
#include "src/wasm/wasm-serialization.h"

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

namespace {

enum class CompileMode : uint8_t { kRegular, kTiering };

// The {CompilationStateImpl} keeps track of the compilation state of the
// owning NativeModule, i.e. which functions are left to be compiled.
// It contains a task manager to allow parallel and asynchronous background
// compilation of functions.
// It's public interface {CompilationState} lives in compilation-environment.h.
class CompilationStateImpl {
 public:
  CompilationStateImpl(internal::Isolate*, NativeModule*);
  ~CompilationStateImpl();

  // Cancel all background compilation and wait for all tasks to finish. Call
  // this before destructing this object.
  void CancelAndWait();

  // Set the number of compilations unit expected to be executed. Needs to be
  // set before {AddCompilationUnits} is run, which triggers background
  // compilation.
  void SetNumberOfFunctionsToCompile(size_t num_functions);

  // Add the callback function to be called on compilation events. Needs to be
  // set before {AddCompilationUnits} is run.
  void AddCallback(CompilationState::callback_t);

  // Inserts new functions to compile and kicks off compilation.
  void AddCompilationUnits(
      std::vector<std::unique_ptr<WasmCompilationUnit>>& baseline_units,
      std::vector<std::unique_ptr<WasmCompilationUnit>>& tiering_units);
  std::unique_ptr<WasmCompilationUnit> GetNextCompilationUnit();
  std::unique_ptr<WasmCompilationUnit> GetNextExecutedUnit();

  bool HasCompilationUnitToFinish();

  void OnFinishedUnit(ExecutionTier, WasmCode*);

  void ReportDetectedFeatures(const WasmFeatures& detected);
  void OnBackgroundTaskStopped(const WasmFeatures& detected);
  void PublishDetectedFeatures(Isolate* isolate, const WasmFeatures& detected);
  void RestartBackgroundCompileTask();
  void RestartBackgroundTasks(size_t max = std::numeric_limits<size_t>::max());
  // Only one foreground thread (finisher) is allowed to run at a time.
  // {SetFinisherIsRunning} returns whether the flag changed its state.
  bool SetFinisherIsRunning(bool value);
  void ScheduleFinisherTask();

  void Abort();

  void SetError(uint32_t func_index, const WasmError& error);

  Isolate* isolate() const { return isolate_; }

  bool failed() const {
    return compile_error_.load(std::memory_order_relaxed) != nullptr;
  }

  bool baseline_compilation_finished() const {
    base::MutexGuard guard(&mutex_);
    return outstanding_baseline_units_ == 0 ||
           (compile_mode_ == CompileMode::kTiering &&
            outstanding_tiering_units_ == 0);
  }

  CompileMode compile_mode() const { return compile_mode_; }
  WasmFeatures* detected_features() { return &detected_features_; }

  // Call {GetCompileError} from foreground threads only, since we access
  // NativeModule::wire_bytes, which is set from the foreground thread once the
  // stream has finished.
  WasmError GetCompileError() {
    CompilationError* error = compile_error_.load(std::memory_order_acquire);
    DCHECK_NOT_NULL(error);
    std::ostringstream error_msg;
    error_msg << "Compiling wasm function \"";
    wasm::ModuleWireBytes wire_bytes(native_module_->wire_bytes());
    wasm::WireBytesRef name_ref = native_module_->module()->LookupFunctionName(
        wire_bytes, error->func_index);
    if (name_ref.is_set()) {
      wasm::WasmName name = wire_bytes.GetNameOrNull(name_ref);
      error_msg.write(name.start(), name.length());
    } else {
      error_msg << "wasm-function[" << error->func_index << "]";
    }
    error_msg << "\" failed: " << error->error.message();
    return WasmError{error->error.offset(), error_msg.str()};
  }

  void SetWireBytesStorage(
      std::shared_ptr<WireBytesStorage> wire_bytes_storage) {
    base::MutexGuard guard(&mutex_);
    wire_bytes_storage_ = wire_bytes_storage;
  }

  std::shared_ptr<WireBytesStorage> GetWireBytesStorage() const {
    base::MutexGuard guard(&mutex_);
    DCHECK_NOT_NULL(wire_bytes_storage_);
    return wire_bytes_storage_;
  }

 private:
  struct CompilationError {
    uint32_t const func_index;
    WasmError const error;
    CompilationError(uint32_t func_index, WasmError error)
        : func_index(func_index), error(std::move(error)) {}
  };

  class LogCodesTask : public CancelableTask {
   public:
    LogCodesTask(CancelableTaskManager* manager,
                 CompilationStateImpl* compilation_state, Isolate* isolate)
        : CancelableTask(manager),
          compilation_state_(compilation_state),
          isolate_(isolate) {
      // This task should only be created if we should actually log code.
      DCHECK(WasmCode::ShouldBeLogged(isolate));
    }

    // Hold the compilation state {mutex_} when calling this method.
    void AddCode(WasmCode* code) { code_to_log_.push_back(code); }

    void RunInternal() override {
      // Remove this task from the {CompilationStateImpl}. The next compilation
      // that finishes will allocate and schedule a new task.
      {
        base::MutexGuard guard(&compilation_state_->mutex_);
        DCHECK_EQ(this, compilation_state_->log_codes_task_);
        compilation_state_->log_codes_task_ = nullptr;
      }
      // If by now we shouldn't log code any more, don't log it.
      if (!WasmCode::ShouldBeLogged(isolate_)) return;
      for (WasmCode* code : code_to_log_) {
        code->LogCode(isolate_);
      }
    }

   private:
    CompilationStateImpl* const compilation_state_;
    Isolate* const isolate_;
    std::vector<WasmCode*> code_to_log_;
  };

  class FreeCallbacksTask : public CancelableTask {
   public:
    explicit FreeCallbacksTask(CompilationStateImpl* comp_state)
        : CancelableTask(&comp_state->foreground_task_manager_),
          compilation_state_(comp_state) {}

    void RunInternal() override { compilation_state_->callbacks_.clear(); }

   private:
    CompilationStateImpl* const compilation_state_;
  };

  void NotifyOnEvent(CompilationEvent event, const WasmError* error);

  std::vector<std::unique_ptr<WasmCompilationUnit>>& finish_units() {
    return baseline_compilation_finished() ? tiering_finish_units_
                                           : baseline_finish_units_;
  }

  // TODO(mstarzinger): Get rid of the Isolate field to make sure the
  // {CompilationStateImpl} can be shared across multiple Isolates.
  Isolate* const isolate_;
  NativeModule* const native_module_;
  const CompileMode compile_mode_;
  // Store the value of {WasmCode::ShouldBeLogged()} at creation time of the
  // compilation state.
  // TODO(wasm): We might lose log events if logging is enabled while
  // compilation is running.
  bool const should_log_code_;

  // Compilation error, atomically updated, but at most once (nullptr -> error).
  // Uses acquire-release semantics (acquire on load, release on update).
  // For checking whether an error is set, relaxed semantics can be used.
  std::atomic<CompilationError*> compile_error_{nullptr};

  // This mutex protects all information of this {CompilationStateImpl} which is
  // being accessed concurrently.
  mutable base::Mutex mutex_;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {mutex_}:

  std::vector<std::unique_ptr<WasmCompilationUnit>> baseline_compilation_units_;
  std::vector<std::unique_ptr<WasmCompilationUnit>> tiering_compilation_units_;

  bool finisher_is_running_ = false;
  size_t num_background_tasks_ = 0;

  std::vector<std::unique_ptr<WasmCompilationUnit>> baseline_finish_units_;
  std::vector<std::unique_ptr<WasmCompilationUnit>> tiering_finish_units_;

  // Features detected to be used in this module. Features can be detected
  // as a module is being compiled.
  WasmFeatures detected_features_ = kNoWasmFeatures;

  // The foreground task to log finished wasm code. Is {nullptr} if no such task
  // is currently scheduled.
  LogCodesTask* log_codes_task_ = nullptr;

  // Abstraction over the storage of the wire bytes. Held in a shared_ptr so
  // that background compilation jobs can keep the storage alive while
  // compiling.
  std::shared_ptr<WireBytesStorage> wire_bytes_storage_;

  size_t outstanding_baseline_units_ = 0;
  size_t outstanding_tiering_units_ = 0;

  // End of fields protected by {mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  // Callback functions to be called on compilation events. Only accessible from
  // the foreground thread.
  std::vector<CompilationState::callback_t> callbacks_;

  // Remember whether {Abort()} was called. When set from the foreground this
  // ensures no more callbacks will be called afterwards. No guarantees when set
  // from the background. Only needs to be atomic so that it can be set from
  // foreground and background.
  std::atomic<bool> aborted_{false};

  CancelableTaskManager background_task_manager_;
  CancelableTaskManager foreground_task_manager_;
  std::shared_ptr<v8::TaskRunner> foreground_task_runner_;

  const size_t max_background_tasks_ = 0;
};

void UpdateFeatureUseCounts(Isolate* isolate, const WasmFeatures& detected) {
  if (detected.threads) {
    isolate->CountUsage(v8::Isolate::UseCounterFeature::kWasmThreadOpcodes);
  }
}

CompilationStateImpl* Impl(CompilationState* compilation_state) {
  return reinterpret_cast<CompilationStateImpl*>(compilation_state);
}
const CompilationStateImpl* Impl(const CompilationState* compilation_state) {
  return reinterpret_cast<const CompilationStateImpl*>(compilation_state);
}

}  // namespace

//////////////////////////////////////////////////////
// PIMPL implementation of {CompilationState}.

CompilationState::~CompilationState() { Impl(this)->~CompilationStateImpl(); }

void CompilationState::CancelAndWait() { Impl(this)->CancelAndWait(); }

void CompilationState::SetError(uint32_t func_index, const WasmError& error) {
  Impl(this)->SetError(func_index, error);
}

void CompilationState::SetWireBytesStorage(
    std::shared_ptr<WireBytesStorage> wire_bytes_storage) {
  Impl(this)->SetWireBytesStorage(std::move(wire_bytes_storage));
}

std::shared_ptr<WireBytesStorage> CompilationState::GetWireBytesStorage()
    const {
  return Impl(this)->GetWireBytesStorage();
}

void CompilationState::AddCallback(CompilationState::callback_t callback) {
  return Impl(this)->AddCallback(std::move(callback));
}

bool CompilationState::failed() const { return Impl(this)->failed(); }

// static
std::unique_ptr<CompilationState> CompilationState::New(
    Isolate* isolate, NativeModule* native_module) {
  return std::unique_ptr<CompilationState>(reinterpret_cast<CompilationState*>(
      new CompilationStateImpl(isolate, native_module)));
}

// End of PIMPL implementation of {CompilationState}.
//////////////////////////////////////////////////////

WasmCode* LazyCompileFunction(Isolate* isolate, NativeModule* native_module,
                              int func_index) {
  base::ElapsedTimer compilation_timer;
  DCHECK(!native_module->has_code(static_cast<uint32_t>(func_index)));

  compilation_timer.Start();

  TRACE_LAZY("Compiling wasm-function#%d.\n", func_index);

  const uint8_t* module_start = native_module->wire_bytes().start();

  const WasmFunction* func = &native_module->module()->functions[func_index];
  FunctionBody func_body{func->sig, func->code.offset(),
                         module_start + func->code.offset(),
                         module_start + func->code.end_offset()};

  ExecutionTier tier =
      WasmCompilationUnit::GetDefaultExecutionTier(native_module->module());
  WasmCompilationUnit unit(isolate->wasm_engine(), func_index, tier);
  CompilationEnv env = native_module->CreateCompilationEnv();
  WasmCompilationResult result = unit.ExecuteCompilation(
      &env, native_module->compilation_state()->GetWireBytesStorage(),
      isolate->counters(),
      Impl(native_module->compilation_state())->detected_features());
  WasmCode* code = unit.Publish(std::move(result), native_module);

  // During lazy compilation, we should never get compilation errors. The module
  // was verified before starting execution with lazy compilation.
  // This might be OOM, but then we cannot continue execution anyway.
  // TODO(clemensh): According to the spec, we can actually skip validation at
  // module creation time, and return a function that always traps here.
  CHECK(!native_module->compilation_state()->failed());

  if (WasmCode::ShouldBeLogged(isolate)) code->LogCode(isolate);

  int64_t func_size =
      static_cast<int64_t>(func->code.end_offset() - func->code.offset());
  int64_t compilation_time = compilation_timer.Elapsed().InMicroseconds();

  auto counters = isolate->counters();
  counters->wasm_lazily_compiled_functions()->Increment();

  counters->wasm_lazy_compilation_throughput()->AddSample(
      compilation_time != 0 ? static_cast<int>(func_size / compilation_time)
                            : 0);

  return code;
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

namespace {

// The {CompilationUnitBuilder} builds compilation units and stores them in an
// internal buffer. The buffer is moved into the working queue of the
// {CompilationStateImpl} when {Commit} is called.
class CompilationUnitBuilder {
 public:
  explicit CompilationUnitBuilder(NativeModule* native_module,
                                  WasmEngine* wasm_engine)
      : native_module_(native_module),
        wasm_engine_(wasm_engine),
        default_tier_(WasmCompilationUnit::GetDefaultExecutionTier(
            native_module->module())) {}

  void AddUnit(uint32_t func_index) {
    switch (compilation_state()->compile_mode()) {
      case CompileMode::kTiering:
        tiering_units_.emplace_back(
            CreateUnit(func_index, ExecutionTier::kOptimized));
        baseline_units_.emplace_back(
            CreateUnit(func_index, ExecutionTier::kBaseline));
        return;
      case CompileMode::kRegular:
        baseline_units_.emplace_back(CreateUnit(func_index, default_tier_));
        return;
    }
    UNREACHABLE();
  }

  bool Commit() {
    if (baseline_units_.empty() && tiering_units_.empty()) return false;
    compilation_state()->AddCompilationUnits(baseline_units_, tiering_units_);
    Clear();
    return true;
  }

  void Clear() {
    baseline_units_.clear();
    tiering_units_.clear();
  }

 private:
  std::unique_ptr<WasmCompilationUnit> CreateUnit(uint32_t func_index,
                                                  ExecutionTier tier) {
    return base::make_unique<WasmCompilationUnit>(wasm_engine_, func_index,
                                                  tier);
  }

  CompilationStateImpl* compilation_state() const {
    return Impl(native_module_->compilation_state());
  }

  NativeModule* const native_module_;
  WasmEngine* const wasm_engine_;
  const ExecutionTier default_tier_;
  std::vector<std::unique_ptr<WasmCompilationUnit>> baseline_units_;
  std::vector<std::unique_ptr<WasmCompilationUnit>> tiering_units_;
};

bool compile_lazy(const WasmModule* module) {
  return FLAG_wasm_lazy_compilation ||
         (FLAG_asm_wasm_lazy_compilation && module->origin == kAsmJsOrigin);
}

void RecordStats(const Code code, Counters* counters) {
  counters->wasm_generated_code_size()->Increment(code->body_size());
  counters->wasm_reloc_size()->Increment(code->relocation_info()->length());
}

double MonotonicallyIncreasingTimeInMs() {
  return V8::GetCurrentPlatform()->MonotonicallyIncreasingTime() *
         base::Time::kMillisecondsPerSecond;
}

// Run by each compilation task and by the main thread (i.e. in both
// foreground and background threads). The no_finisher_callback is called
// within the result_mutex_ lock when no finishing task is running, i.e. when
// the finisher_is_running_ flag is not set.
bool FetchAndExecuteCompilationUnit(CompilationEnv* env,
                                    NativeModule* native_module,
                                    CompilationStateImpl* compilation_state,
                                    WasmFeatures* detected,
                                    Counters* counters) {
  DisallowHeapAccess no_heap_access;

  std::unique_ptr<WasmCompilationUnit> unit =
      compilation_state->GetNextCompilationUnit();
  if (unit == nullptr) return false;

  // Get the tier before starting compilation, as compilation can switch tiers
  // if baseline bails out.
  ExecutionTier tier = unit->tier();
  WasmCompilationResult result = unit->ExecuteCompilation(
      env, compilation_state->GetWireBytesStorage(), counters, detected);

  WasmCode* code = unit->Publish(std::move(result), native_module);
  compilation_state->OnFinishedUnit(tier, code);

  return true;
}

void InitializeCompilationUnits(NativeModule* native_module,
                                WasmEngine* wasm_engine) {
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  const WasmModule* module = native_module->module();
  CompilationUnitBuilder builder(native_module, wasm_engine);
  uint32_t start = module->num_imported_functions;
  uint32_t end = start + module->num_declared_functions;
  for (uint32_t i = start; i < end; ++i) {
    builder.AddUnit(i);
  }
  builder.Commit();
}

void FinishCompilationUnits(CompilationStateImpl* compilation_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "FinishCompilationUnits");
  while (!compilation_state->failed()) {
    std::unique_ptr<WasmCompilationUnit> unit =
        compilation_state->GetNextExecutedUnit();
    if (unit == nullptr) break;
  }
}

void CompileInParallel(Isolate* isolate, NativeModule* native_module) {
  // Data structures for the parallel compilation.

  //-----------------------------------------------------------------------
  // For parallel compilation:
  // 1) The main thread allocates a compilation unit for each wasm function
  //    and stores them in the vector {compilation_units} within the
  //    {compilation_state}. By adding units to the {compilation_state}, new
  //    {BackgroundCompileTasks} instances are spawned which run on
  //    the background threads.
  // 2) The background threads and the main thread pick one compilation unit at
  //    a time and execute the parallel phase of the compilation unit.
  // 3) After the parallel phase of all compilation units has started, the
  //    main thread continues to finish all compilation units as long as
  //    baseline-compilation units are left to be processed.
  // 4) If tier-up is enabled, the main thread restarts background tasks
  //    that take care of compiling and finishing the top-tier compilation
  //    units.

  // Turn on the {CanonicalHandleScope} so that the background threads can
  // use the node cache.
  CanonicalHandleScope canonical(isolate);

  CompilationStateImpl* compilation_state =
      Impl(native_module->compilation_state());
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
  InitializeCompilationUnits(native_module, isolate->wasm_engine());

  // 2) The background threads and the main thread pick one compilation unit at
  //    a time and execute the parallel phase of the compilation unit.
  WasmFeatures detected_features;
  CompilationEnv env = native_module->CreateCompilationEnv();
  while (FetchAndExecuteCompilationUnit(&env, native_module, compilation_state,
                                        &detected_features,
                                        isolate->counters()) &&
         !compilation_state->baseline_compilation_finished()) {
    // TODO(clemensh): Refactor ownership of the AsyncCompileJob and remove
    // this.
    FinishCompilationUnits(compilation_state);

    if (compilation_state->failed()) break;
  }

  while (!compilation_state->failed()) {
    // 3) After the parallel phase of all compilation units has started, the
    //    main thread continues to finish compilation units as long as
    //    baseline compilation units are left to be processed. If compilation
    //    already failed, all background tasks have already been canceled
    //    in {FinishCompilationUnits}, and there are no units to finish.
    FinishCompilationUnits(compilation_state);

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
  }
}

void CompileSequentially(Isolate* isolate, NativeModule* native_module,
                         ErrorThrower* thrower) {
  DCHECK(!thrower->error());

  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  const WasmModule* module = native_module->module();
  WasmFeatures detected = kNoWasmFeatures;
  auto* comp_state = Impl(native_module->compilation_state());
  ExecutionTier tier =
      WasmCompilationUnit::GetDefaultExecutionTier(native_module->module());
  for (const WasmFunction& func : module->functions) {
    if (func.imported) continue;  // Imports are compiled at instantiation time.

    // Compile the function.
    WasmCompilationUnit::CompileWasmFunction(isolate, native_module, &detected,
                                             &func, tier);
    if (comp_state->failed()) {
      thrower->CompileFailed(comp_state->GetCompileError());
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
      auto time_counter = SELECT_WASM_COUNTER(
          isolate->counters(), module->origin, wasm_decode, function_time);

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
                            result.error().message().c_str(),
                            result.error().offset());
      break;
    }
  }
}

void CompileNativeModule(Isolate* isolate, ErrorThrower* thrower,
                         const WasmModule* wasm_module,
                         NativeModule* native_module) {
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
      CompileInParallel(isolate, native_module);
    } else {
      CompileSequentially(isolate, native_module, thrower);
    }
    auto* compilation_state = Impl(native_module->compilation_state());
    if (compilation_state->failed()) {
      thrower->CompileFailed(compilation_state->GetCompileError());
    }
  }
}

// The runnable task that finishes compilation in foreground (e.g. updating
// the NativeModule, the code table, etc.).
class FinishCompileTask : public CancelableTask {
 public:
  explicit FinishCompileTask(CompilationStateImpl* compilation_state,
                             CancelableTaskManager* task_manager)
      : CancelableTask(task_manager), compilation_state_(compilation_state) {}

  void RunInternal() override {
    Isolate* isolate = compilation_state_->isolate();
    HandleScope scope(isolate);
    SaveContext saved_context(isolate);
    isolate->set_context(Context());

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

      if (compilation_state_->failed()) break;

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
  CompilationStateImpl* compilation_state_;
};

// The runnable task that performs compilations in the background.
class BackgroundCompileTask : public CancelableTask {
 public:
  explicit BackgroundCompileTask(CancelableTaskManager* task_manager,
                                 NativeModule* native_module,
                                 Counters* counters)
      : CancelableTask(task_manager),
        native_module_(native_module),
        counters_(counters) {}

  void RunInternal() override {
    TRACE_COMPILE("(3b) Compiling...\n");
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
                 "BackgroundCompileTask::RunInternal");
    // The number of currently running background tasks is reduced in
    // {OnBackgroundTaskStopped}.
    CompilationEnv env = native_module_->CreateCompilationEnv();
    auto* compilation_state = Impl(native_module_->compilation_state());
    WasmFeatures detected_features = kNoWasmFeatures;
    double deadline = MonotonicallyIncreasingTimeInMs() + 50.0;
    while (!compilation_state->failed()) {
      if (!FetchAndExecuteCompilationUnit(&env, native_module_,
                                          compilation_state, &detected_features,
                                          counters_)) {
        break;
      }
      if (deadline < MonotonicallyIncreasingTimeInMs()) {
        compilation_state->ReportDetectedFeatures(detected_features);
        compilation_state->RestartBackgroundCompileTask();
        return;
      }
    }
    compilation_state->OnBackgroundTaskStopped(detected_features);
  }

 private:
  NativeModule* const native_module_;
  Counters* const counters_;
};

}  // namespace

std::unique_ptr<NativeModule> CompileToNativeModule(
    Isolate* isolate, const WasmFeatures& enabled, ErrorThrower* thrower,
    std::shared_ptr<const WasmModule> module, const ModuleWireBytes& wire_bytes,
    Handle<FixedArray>* export_wrappers_out) {
  const WasmModule* wasm_module = module.get();
  TimedHistogramScope wasm_compile_module_time_scope(SELECT_WASM_COUNTER(
      isolate->counters(), wasm_module->origin, wasm_compile, module_time));

  // Embedder usage count for declared shared memories.
  if (wasm_module->has_shared_memory) {
    isolate->CountUsage(v8::Isolate::UseCounterFeature::kWasmSharedMemory);
  }
  int export_wrapper_size = static_cast<int>(module->num_exported_functions);

  // TODO(wasm): only save the sections necessary to deserialize a
  // {WasmModule}. E.g. function bodies could be omitted.
  OwnedVector<uint8_t> wire_bytes_copy =
      OwnedVector<uint8_t>::Of(wire_bytes.module_bytes());

  // Create and compile the native module.
  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(module.get());

  // Create a new {NativeModule} first.
  auto native_module = isolate->wasm_engine()->code_manager()->NewNativeModule(
      isolate, enabled, code_size_estimate,
      wasm::NativeModule::kCanAllocateMoreMemory, std::move(module));
  native_module->SetWireBytes(std::move(wire_bytes_copy));
  native_module->SetRuntimeStubs(isolate);

  CompileNativeModule(isolate, thrower, wasm_module, native_module.get());
  if (thrower->error()) return {};

  // Compile JS->wasm wrappers for exported functions.
  *export_wrappers_out =
      isolate->factory()->NewFixedArray(export_wrapper_size, TENURED);
  CompileJsToWasmWrappers(isolate, native_module->module(),
                          *export_wrappers_out);

  // Log the code within the generated module for profiling.
  native_module->LogWasmCodes(isolate);

  return native_module;
}

void CompileNativeModuleWithExplicitBoundsChecks(Isolate* isolate,
                                                 ErrorThrower* thrower,
                                                 const WasmModule* wasm_module,
                                                 NativeModule* native_module) {
  native_module->DisableTrapHandler();
  CompileNativeModule(isolate, thrower, wasm_module, native_module);
}

AsyncCompileJob::AsyncCompileJob(
    Isolate* isolate, const WasmFeatures& enabled,
    std::unique_ptr<byte[]> bytes_copy, size_t length, Handle<Context> context,
    std::shared_ptr<CompilationResultResolver> resolver)
    : isolate_(isolate),
      enabled_features_(enabled),
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
  DoAsync<DecodeModule>(isolate_->counters());  // --
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

  bool ProcessCodeSectionHeader(size_t functions_count, uint32_t offset,
                                std::shared_ptr<WireBytesStorage>) override;

  bool ProcessFunctionBody(Vector<const uint8_t> bytes,
                           uint32_t offset) override;

  void OnFinishedChunk() override;

  void OnFinishedStream(OwnedVector<uint8_t> bytes) override;

  void OnError(const WasmError&) override;

  void OnAbort() override;

  bool Deserialize(Vector<const uint8_t> wire_bytes,
                   Vector<const uint8_t> module_bytes) override;

 private:
  // Finishes the AsyncCompileJob with an error.
  void FinishAsyncCompileJobWithError(const WasmError&);

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
  // If the runtime objects were not created yet, then initial compilation did
  // not finish yet. In this case we can abort compilation.
  if (native_module_ && module_object_.is_null()) {
    Impl(native_module_->compilation_state())->Abort();
  }
  // Tell the streaming decoder that the AsyncCompileJob is not available
  // anymore.
  // TODO(ahaas): Is this notification really necessary? Check
  // https://crbug.com/888170.
  if (stream_) stream_->NotifyCompilationEnded();
  CancelPendingForegroundTask();
  for (auto d : deferred_handles_) delete d;
}

void AsyncCompileJob::CreateNativeModule(
    std::shared_ptr<const WasmModule> module) {
  // Embedder usage count for declared shared memories.
  if (module->has_shared_memory) {
    isolate_->CountUsage(v8::Isolate::UseCounterFeature::kWasmSharedMemory);
  }

  // TODO(wasm): Improve efficiency of storing module wire bytes. Only store
  // relevant sections, not function bodies

  // Create the module object and populate with compiled functions and
  // information needed at instantiation time.
  // TODO(clemensh): For the same module (same bytes / same hash), we should
  // only have one {WasmModuleObject}. Otherwise, we might only set
  // breakpoints on a (potentially empty) subset of the instances.
  // Create the module object.

  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(module.get());
  native_module_ = isolate_->wasm_engine()->code_manager()->NewNativeModule(
      isolate_, enabled_features_, code_size_estimate,
      wasm::NativeModule::kCanAllocateMoreMemory, std::move(module));
  native_module_->SetWireBytes({std::move(bytes_copy_), wire_bytes_.length()});
  native_module_->SetRuntimeStubs(isolate_);

  if (stream_) stream_->NotifyNativeModuleCreated(native_module_);
}

void AsyncCompileJob::PrepareRuntimeObjects() {
  // Create heap objects for script and module bytes to be stored in the
  // module object. Asm.js is not compiled asynchronously.
  const WasmModule* module = native_module_->module();
  Handle<Script> script =
      CreateWasmScript(isolate_, wire_bytes_, module->source_map_url);

  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(module);
  module_object_ = WasmModuleObject::New(isolate_, native_module_, script,
                                         code_size_estimate);

  {
    DeferredHandleScope deferred(isolate_);
    module_object_ = handle(*module_object_, isolate_);
    deferred_handles_.push_back(deferred.Detach());
  }
}

// This function assumes that it is executed in a HandleScope, and that a
// context is set on the isolate.
void AsyncCompileJob::FinishCompile() {
  bool is_after_deserialization = !module_object_.is_null();
  if (!is_after_deserialization) {
    PrepareRuntimeObjects();
  }
  DCHECK(!isolate_->context().is_null());
  // Finish the wasm script now and make it public to the debugger.
  Handle<Script> script(module_object_->script(), isolate_);
  if (script->type() == Script::TYPE_WASM &&
      module_object_->module()->source_map_url.size() != 0) {
    MaybeHandle<String> src_map_str = isolate_->factory()->NewStringFromUtf8(
        CStrVector(module_object_->module()->source_map_url.c_str()), TENURED);
    script->set_source_mapping_url(*src_map_str.ToHandleChecked());
  }
  isolate_->debug()->OnAfterCompile(script);

  // We can only update the feature counts once the entire compile is done.
  auto compilation_state =
      Impl(module_object_->native_module()->compilation_state());
  compilation_state->PublishDetectedFeatures(
      isolate_, *compilation_state->detected_features());

  // TODO(bbudge) Allow deserialization without wrapper compilation, so we can
  // just compile wrappers here.
  if (!is_after_deserialization) {
    // TODO(wasm): compiling wrappers should be made async.
    CompileWrappers();
  }
  FinishModule();
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

class AsyncCompileJob::CompilationStateCallback {
 public:
  explicit CompilationStateCallback(AsyncCompileJob* job) : job_(job) {}

  void operator()(CompilationEvent event, const WasmError* error) {
    // This callback is only being called from a foreground task.
    switch (event) {
      case CompilationEvent::kFinishedBaselineCompilation:
        DCHECK(!last_event_.has_value());
        if (job_->DecrementAndCheckFinisherCount()) {
          SaveContext saved_context(job_->isolate());
          job_->isolate()->set_context(*job_->native_context_);
          job_->FinishCompile();
        }
        break;
      case CompilationEvent::kFinishedTopTierCompilation:
        DCHECK_EQ(CompilationEvent::kFinishedBaselineCompilation, last_event_);
        // This callback should not react to top tier finished callbacks, since
        // the job might already be gone then.
        break;
      case CompilationEvent::kFailedCompilation:
        DCHECK(!last_event_.has_value());
        DCHECK_NOT_NULL(error);
        // Tier-up compilation should not fail if baseline compilation
        // did not fail.
        DCHECK(!Impl(job_->native_module_->compilation_state())
                    ->baseline_compilation_finished());

        {
          SaveContext saved_context(job_->isolate());
          job_->isolate()->set_context(*job_->native_context_);
          ErrorThrower thrower(job_->isolate(), "AsyncCompilation");
          thrower.CompileFailed(nullptr, *error);
          Handle<Object> error = thrower.Reify();

          DeferredHandleScope deferred(job_->isolate());
          error = handle(*error, job_->isolate());
          job_->deferred_handles_.push_back(deferred.Detach());

          job_->DoSync<CompileFailed, kUseExistingForegroundTask>(error);
        }

        break;
      default:
        UNREACHABLE();
    }
#ifdef DEBUG
    last_event_ = event;
#endif
  }

 private:
  AsyncCompileJob* job_;
#ifdef DEBUG
  base::Optional<CompilationEvent> last_event_;
#endif
};

// A closure to run a compilation step (either as foreground or background
// task) and schedule the next step(s), if any.
class AsyncCompileJob::CompileStep {
 public:
  virtual ~CompileStep() = default;

  void Run(AsyncCompileJob* job, bool on_foreground) {
    if (on_foreground) {
      HandleScope scope(job->isolate_);
      SaveContext saved_context(job->isolate_);
      job->isolate_->set_context(*job->native_context_);
      RunInForeground(job);
    } else {
      RunInBackground(job);
    }
  }

  virtual void RunInForeground(AsyncCompileJob*) { UNREACHABLE(); }
  virtual void RunInBackground(AsyncCompileJob*) { UNREACHABLE(); }
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
    job_->step_->Run(job_, on_foreground_);
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

template <typename Step,
          AsyncCompileJob::UseExistingForegroundTask use_existing_fg_task,
          typename... Args>
void AsyncCompileJob::DoSync(Args&&... args) {
  NextStep<Step>(std::forward<Args>(args)...);
  if (use_existing_fg_task && pending_foreground_task_ != nullptr) return;
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
}

//==========================================================================
// Step 1: (async) Decode the module.
//==========================================================================
class AsyncCompileJob::DecodeModule : public AsyncCompileJob::CompileStep {
 public:
  explicit DecodeModule(Counters* counters) : counters_(counters) {}

  void RunInBackground(AsyncCompileJob* job) override {
    ModuleResult result;
    {
      DisallowHandleAllocation no_handle;
      DisallowHeapAllocation no_allocation;
      // Decode the module bytes.
      TRACE_COMPILE("(1) Decoding module...\n");
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
                   "AsyncCompileJob::DecodeModule");
      result = DecodeWasmModule(
          job->enabled_features_, job->wire_bytes_.start(),
          job->wire_bytes_.end(), false, kWasmOrigin, counters_,
          job->isolate()->wasm_engine()->allocator());
    }
    if (result.failed()) {
      // Decoding failure; reject the promise and clean up.
      job->DoSync<DecodeFail>(std::move(result).error());
    } else {
      // Decode passed.
      job->DoSync<PrepareAndStartCompile>(std::move(result).value(), true);
    }
  }

 private:
  Counters* const counters_;
};

//==========================================================================
// Step 1b: (sync) Fail decoding the module.
//==========================================================================
class AsyncCompileJob::DecodeFail : public CompileStep {
 public:
  explicit DecodeFail(WasmError error) : error_(std::move(error)) {}

 private:
  WasmError error_;

  void RunInForeground(AsyncCompileJob* job) override {
    TRACE_COMPILE("(1b) Decoding failed.\n");
    ErrorThrower thrower(job->isolate_, "AsyncCompile");
    thrower.CompileFailed("Wasm decoding failed", error_);
    // {job_} is deleted in AsyncCompileFailed, therefore the {return}.
    return job->AsyncCompileFailed(thrower.Reify());
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

  void RunInForeground(AsyncCompileJob* job) override {
    TRACE_COMPILE("(2) Prepare and start compile...\n");

    // Make sure all compilation tasks stopped running. Decoding (async step)
    // is done.
    job->background_task_manager_.CancelAndWait();

    job->CreateNativeModule(module_);

    size_t num_functions =
        module_->functions.size() - module_->num_imported_functions;

    if (num_functions == 0) {
      // Degenerate case of an empty module.
      job->FinishCompile();
      return;
    }

    CompilationStateImpl* compilation_state =
        Impl(job->native_module_->compilation_state());
    compilation_state->AddCallback(CompilationStateCallback{job});
    if (start_compilation_) {
      // TODO(ahaas): Try to remove the {start_compilation_} check when
      // streaming decoding is done in the background. If
      // InitializeCompilationUnits always returns 0 for streaming compilation,
      // then DoAsync would do the same as NextStep already.

      compilation_state->SetNumberOfFunctionsToCompile(
          module_->num_declared_functions);
      // Add compilation units and kick off compilation.
      InitializeCompilationUnits(job->native_module_.get(),
                                 job->isolate()->wasm_engine());
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

  void RunInForeground(AsyncCompileJob* job) override {
    TRACE_COMPILE("(4b) Compilation Failed...\n");
    return job->AsyncCompileFailed(error_reason_);
  }

 private:
  Handle<Object> error_reason_;
};

void AsyncCompileJob::CompileWrappers() {
  // TODO(wasm): Compile all wrappers here, including the start function wrapper
  // and the wrappers for the function table elements.
  TRACE_COMPILE("(5) Compile wrappers...\n");
  // Compile JS->wasm wrappers for exported functions.
  CompileJsToWasmWrappers(isolate_, module_object_->native_module()->module(),
                          handle(module_object_->export_wrappers(), isolate_));
}

void AsyncCompileJob::FinishModule() {
  TRACE_COMPILE("(6) Finish module...\n");
  AsyncCompileSucceeded(module_object_);

  size_t num_functions = native_module_->num_functions() -
                         native_module_->num_imported_functions();
  auto* compilation_state = Impl(native_module_->compilation_state());
  if (compilation_state->compile_mode() == CompileMode::kRegular ||
      num_functions == 0) {
    // If we do not tier up, the async compile job is done here and
    // can be deleted.
    isolate_->wasm_engine()->RemoveCompileJob(this);
    return;
  }
  DCHECK_EQ(CompileMode::kTiering, compilation_state->compile_mode());
  if (compilation_state->baseline_compilation_finished()) {
    isolate_->wasm_engine()->RemoveCompileJob(this);
  }
}

AsyncStreamingProcessor::AsyncStreamingProcessor(AsyncCompileJob* job)
    : decoder_(job->enabled_features_),
      job_(job),
      compilation_unit_builder_(nullptr) {}

void AsyncStreamingProcessor::FinishAsyncCompileJobWithError(
    const WasmError& error) {
  DCHECK(error.has_error());
  // Make sure all background tasks stopped executing before we change the state
  // of the AsyncCompileJob to DecodeFail.
  job_->background_task_manager_.CancelAndWait();

  // Check if there is already a CompiledModule, in which case we have to clean
  // up the CompilationStateImpl as well.
  if (job_->native_module_) {
    Impl(job_->native_module_->compilation_state())->Abort();

    job_->DoSync<AsyncCompileJob::DecodeFail,
                 AsyncCompileJob::kUseExistingForegroundTask>(error);

    // Clear the {compilation_unit_builder_} if it exists. This is needed
    // because there is a check in the destructor of the
    // {CompilationUnitBuilder} that it is empty.
    if (compilation_unit_builder_) compilation_unit_builder_->Clear();
  } else {
    job_->DoSync<AsyncCompileJob::DecodeFail>(error);
  }
}

// Process the module header.
bool AsyncStreamingProcessor::ProcessModuleHeader(Vector<const uint8_t> bytes,
                                                  uint32_t offset) {
  TRACE_STREAMING("Process module header...\n");
  decoder_.StartDecoding(job_->isolate()->counters(),
                         job_->isolate()->wasm_engine()->allocator());
  decoder_.DecodeModuleHeader(bytes, offset);
  if (!decoder_.ok()) {
    FinishAsyncCompileJobWithError(decoder_.FinishDecoding(false).error());
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
    FinishAsyncCompileJobWithError(decoder_.FinishDecoding(false).error());
    return false;
  }
  return true;
}

// Start the code section.
bool AsyncStreamingProcessor::ProcessCodeSectionHeader(
    size_t functions_count, uint32_t offset,
    std::shared_ptr<WireBytesStorage> wire_bytes_storage) {
  TRACE_STREAMING("Start the code section with %zu functions...\n",
                  functions_count);
  if (!decoder_.CheckFunctionsCount(static_cast<uint32_t>(functions_count),
                                    offset)) {
    FinishAsyncCompileJobWithError(decoder_.FinishDecoding(false).error());
    return false;
  }
  // Execute the PrepareAndStartCompile step immediately and not in a separate
  // task.
  job_->DoImmediately<AsyncCompileJob::PrepareAndStartCompile>(
      decoder_.shared_module(), false);
  job_->native_module_->compilation_state()->SetWireBytesStorage(
      std::move(wire_bytes_storage));

  auto* compilation_state = Impl(job_->native_module_->compilation_state());
  compilation_state->SetNumberOfFunctionsToCompile(functions_count);

  // Set outstanding_finishers_ to 2, because both the AsyncCompileJob and the
  // AsyncStreamingProcessor have to finish.
  job_->outstanding_finishers_.store(2);
  compilation_unit_builder_.reset(new CompilationUnitBuilder(
      job_->native_module_.get(), job_->isolate()->wasm_engine()));
  return true;
}

// Process a function body.
bool AsyncStreamingProcessor::ProcessFunctionBody(Vector<const uint8_t> bytes,
                                                  uint32_t offset) {
  TRACE_STREAMING("Process function body %d ...\n", next_function_);

  decoder_.DecodeFunctionBody(
      next_function_, static_cast<uint32_t>(bytes.length()), offset, false);

  uint32_t index = next_function_ + decoder_.module()->num_imported_functions;
  compilation_unit_builder_->AddUnit(index);
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
  if (result.failed()) {
    FinishAsyncCompileJobWithError(result.error());
    return;
  }
  // We have to open a HandleScope and prepare the Context for
  // CreateNativeModule, PrepareRuntimeObjects and FinishCompile as this is a
  // callback from the embedder.
  HandleScope scope(job_->isolate_);
  SaveContext saved_context(job_->isolate_);
  job_->isolate_->set_context(*job_->native_context_);

  bool needs_finish = job_->DecrementAndCheckFinisherCount();
  if (job_->native_module_ == nullptr) {
    // We are processing a WebAssembly module without code section. Create the
    // runtime objects now (would otherwise happen in {PrepareAndStartCompile}).
    job_->CreateNativeModule(std::move(result).value());
    DCHECK(needs_finish);
  }
  job_->wire_bytes_ = ModuleWireBytes(bytes.as_vector());
  job_->native_module_->SetWireBytes(std::move(bytes));
  if (needs_finish) {
    job_->FinishCompile();
  }
}

// Report an error detected in the StreamingDecoder.
void AsyncStreamingProcessor::OnError(const WasmError& error) {
  TRACE_STREAMING("Stream error...\n");
  FinishAsyncCompileJobWithError(error);
}

void AsyncStreamingProcessor::OnAbort() {
  TRACE_STREAMING("Abort stream...\n");
  job_->Abort();
}

bool AsyncStreamingProcessor::Deserialize(Vector<const uint8_t> module_bytes,
                                          Vector<const uint8_t> wire_bytes) {
  // DeserializeNativeModule and FinishCompile assume that they are executed in
  // a HandleScope, and that a context is set on the isolate.
  HandleScope scope(job_->isolate_);
  SaveContext saved_context(job_->isolate_);
  job_->isolate_->set_context(*job_->native_context_);

  MaybeHandle<WasmModuleObject> result =
      DeserializeNativeModule(job_->isolate_, module_bytes, wire_bytes);
  if (result.is_null()) return false;

  job_->module_object_ = result.ToHandleChecked();
  {
    DeferredHandleScope deferred(job_->isolate_);
    job_->module_object_ = handle(*job_->module_object_, job_->isolate_);
    job_->deferred_handles_.push_back(deferred.Detach());
  }
  job_->native_module_ = job_->module_object_->shared_native_module();
  auto owned_wire_bytes = OwnedVector<uint8_t>::Of(wire_bytes);
  job_->wire_bytes_ = ModuleWireBytes(owned_wire_bytes.as_vector());
  job_->native_module_->SetWireBytes(std::move(owned_wire_bytes));
  job_->FinishCompile();
  return true;
}

CompilationStateImpl::CompilationStateImpl(internal::Isolate* isolate,
                                           NativeModule* native_module)
    : isolate_(isolate),
      native_module_(native_module),
      compile_mode_(FLAG_wasm_tier_up &&
                            native_module->module()->origin == kWasmOrigin
                        ? CompileMode::kTiering
                        : CompileMode::kRegular),
      should_log_code_(WasmCode::ShouldBeLogged(isolate)),
      max_background_tasks_(std::max(
          1, std::min(FLAG_wasm_num_compilation_tasks,
                      V8::GetCurrentPlatform()->NumberOfWorkerThreads()))) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  v8::Platform* platform = V8::GetCurrentPlatform();
  foreground_task_runner_ = platform->GetForegroundTaskRunner(v8_isolate);
}

CompilationStateImpl::~CompilationStateImpl() {
  DCHECK(background_task_manager_.canceled());
  DCHECK(foreground_task_manager_.canceled());
  CompilationError* error = compile_error_.load(std::memory_order_acquire);
  if (error != nullptr) delete error;
}

void CompilationStateImpl::CancelAndWait() {
  background_task_manager_.CancelAndWait();
  foreground_task_manager_.CancelAndWait();
}

void CompilationStateImpl::SetNumberOfFunctionsToCompile(size_t num_functions) {
  DCHECK(!failed());
  base::MutexGuard guard(&mutex_);
  outstanding_baseline_units_ = num_functions;

  if (compile_mode_ == CompileMode::kTiering) {
    outstanding_tiering_units_ = num_functions;
  }
}

void CompilationStateImpl::AddCallback(CompilationState::callback_t callback) {
  callbacks_.emplace_back(std::move(callback));
}

void CompilationStateImpl::AddCompilationUnits(
    std::vector<std::unique_ptr<WasmCompilationUnit>>& baseline_units,
    std::vector<std::unique_ptr<WasmCompilationUnit>>& tiering_units) {
  {
    base::MutexGuard guard(&mutex_);

    if (compile_mode_ == CompileMode::kTiering) {
      DCHECK_EQ(baseline_units.size(), tiering_units.size());
      DCHECK_EQ(tiering_units.back()->tier(), ExecutionTier::kOptimized);
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
CompilationStateImpl::GetNextCompilationUnit() {
  base::MutexGuard guard(&mutex_);

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

std::unique_ptr<WasmCompilationUnit>
CompilationStateImpl::GetNextExecutedUnit() {
  std::vector<std::unique_ptr<WasmCompilationUnit>>& units = finish_units();
  base::MutexGuard guard(&mutex_);
  if (units.empty()) return {};
  std::unique_ptr<WasmCompilationUnit> ret = std::move(units.back());
  units.pop_back();
  return ret;
}

bool CompilationStateImpl::HasCompilationUnitToFinish() {
  return !finish_units().empty();
}

void CompilationStateImpl::OnFinishedUnit(ExecutionTier tier, WasmCode* code) {
  // This mutex guarantees that events happen in the right order.
  base::MutexGuard guard(&mutex_);

  if (failed()) return;

  // If we are *not* compiling in tiering mode, then all units are counted as
  // baseline units.
  bool is_tiering_mode = compile_mode_ == CompileMode::kTiering;
  bool is_tiering_unit = is_tiering_mode && tier == ExecutionTier::kOptimized;

  // Sanity check: If we are not in tiering mode, there cannot be outstanding
  // tiering units.
  DCHECK_IMPLIES(!is_tiering_mode, outstanding_tiering_units_ == 0);

  // Bitset of events to deliver.
  base::EnumSet<CompilationEvent> events;

  if (is_tiering_unit) {
    DCHECK_LT(0, outstanding_tiering_units_);
    --outstanding_tiering_units_;
    if (outstanding_tiering_units_ == 0) {
      // If baseline compilation has not finished yet, then also trigger
      // {kFinishedBaselineCompilation}.
      if (outstanding_baseline_units_ > 0) {
        events.Add(CompilationEvent::kFinishedBaselineCompilation);
      }
      events.Add(CompilationEvent::kFinishedTopTierCompilation);
    }
  } else {
    DCHECK_LT(0, outstanding_baseline_units_);
    --outstanding_baseline_units_;
    if (outstanding_baseline_units_ == 0) {
      events.Add(CompilationEvent::kFinishedBaselineCompilation);
      // If we are not tiering, then we also trigger the "top tier finished"
      // event when baseline compilation is finished.
      if (!is_tiering_mode) {
        events.Add(CompilationEvent::kFinishedTopTierCompilation);
      }
    }
  }

  if (!events.empty()) {
    auto notify_events = [this, events] {
      for (auto event : {CompilationEvent::kFinishedBaselineCompilation,
                         CompilationEvent::kFinishedTopTierCompilation}) {
        if (!events.contains(event)) continue;
        NotifyOnEvent(event, nullptr);
      }
    };
    foreground_task_runner_->PostTask(
        MakeCancelableTask(&foreground_task_manager_, notify_events));
  }

  if (should_log_code_ && code != nullptr) {
    if (log_codes_task_ == nullptr) {
      auto new_task = base::make_unique<LogCodesTask>(&foreground_task_manager_,
                                                      this, isolate_);
      log_codes_task_ = new_task.get();
      foreground_task_runner_->PostTask(std::move(new_task));
    }
    log_codes_task_->AddCode(code);
  }
}

void CompilationStateImpl::RestartBackgroundCompileTask() {
  auto task = base::make_unique<BackgroundCompileTask>(
      &background_task_manager_, native_module_, isolate_->counters());

  // If --wasm-num-compilation-tasks=0 is passed, do only spawn foreground
  // tasks. This is used to make timing deterministic.
  if (FLAG_wasm_num_compilation_tasks == 0) {
    foreground_task_runner_->PostTask(std::move(task));
    return;
  }

  if (baseline_compilation_finished()) {
    V8::GetCurrentPlatform()->CallLowPriorityTaskOnWorkerThread(
        std::move(task));
  } else {
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
  }
}

void CompilationStateImpl::ReportDetectedFeatures(
    const WasmFeatures& detected) {
  base::MutexGuard guard(&mutex_);
  UnionFeaturesInto(&detected_features_, detected);
}

void CompilationStateImpl::OnBackgroundTaskStopped(
    const WasmFeatures& detected) {
  base::MutexGuard guard(&mutex_);
  DCHECK_LE(1, num_background_tasks_);
  --num_background_tasks_;
  UnionFeaturesInto(&detected_features_, detected);
}

void CompilationStateImpl::PublishDetectedFeatures(
    Isolate* isolate, const WasmFeatures& detected) {
  // Notifying the isolate of the feature counts must take place under
  // the mutex, because even if we have finished baseline compilation,
  // tiering compilations may still occur in the background.
  base::MutexGuard guard(&mutex_);
  UnionFeaturesInto(&detected_features_, detected);
  UpdateFeatureUseCounts(isolate, detected_features_);
}

void CompilationStateImpl::RestartBackgroundTasks(size_t max) {
  size_t num_restart;
  {
    base::MutexGuard guard(&mutex_);
    // No need to restart tasks if compilation already failed.
    if (failed()) return;

    DCHECK_LE(num_background_tasks_, max_background_tasks_);
    if (num_background_tasks_ == max_background_tasks_) return;
    size_t num_compilation_units =
        baseline_compilation_units_.size() + tiering_compilation_units_.size();
    size_t stopped_tasks = max_background_tasks_ - num_background_tasks_;
    num_restart = std::min(max, std::min(num_compilation_units, stopped_tasks));
    num_background_tasks_ += num_restart;
  }

  for (; num_restart > 0; --num_restart) {
    RestartBackgroundCompileTask();
  }
}

bool CompilationStateImpl::SetFinisherIsRunning(bool value) {
  base::MutexGuard guard(&mutex_);
  if (finisher_is_running_ == value) return false;
  finisher_is_running_ = value;
  return true;
}

void CompilationStateImpl::ScheduleFinisherTask() {
  foreground_task_runner_->PostTask(
      base::make_unique<FinishCompileTask>(this, &foreground_task_manager_));
}

void CompilationStateImpl::Abort() {
  SetError(0, WasmError{0, "Compilation aborted"});
  background_task_manager_.CancelAndWait();
  // No more callbacks after abort. Don't free the std::function objects here,
  // since this might clear references in the embedder, which is only allowed on
  // the main thread.
  aborted_.store(true);
  if (!callbacks_.empty()) {
    foreground_task_runner_->PostTask(
        base::make_unique<FreeCallbacksTask>(this));
  }
}

void CompilationStateImpl::SetError(uint32_t func_index,
                                    const WasmError& error) {
  DCHECK(error.has_error());
  std::unique_ptr<CompilationError> compile_error =
      base::make_unique<CompilationError>(func_index, error);
  CompilationError* expected = nullptr;
  bool set = compile_error_.compare_exchange_strong(
      expected, compile_error.get(), std::memory_order_acq_rel);
  // Ignore all but the first error. If the previous value is not nullptr, just
  // return (and free the allocated error).
  if (!set) return;
  // If set successfully, give up ownership.
  compile_error.release();
  // Schedule a foreground task to call the callback and notify users about the
  // compile error.
  foreground_task_runner_->PostTask(
      MakeCancelableTask(&foreground_task_manager_, [this] {
        WasmError error = GetCompileError();
        NotifyOnEvent(CompilationEvent::kFailedCompilation, &error);
      }));
}

void CompilationStateImpl::NotifyOnEvent(CompilationEvent event,
                                         const WasmError* error) {
  if (aborted_.load()) return;
  HandleScope scope(isolate_);
  for (auto& callback : callbacks_) callback(event, error);
  // If no more events are expected after this one, clear the callbacks to free
  // memory. We can safely do this here, as this method is only called from
  // foreground tasks.
  if (event >= CompilationEvent::kFirstFinalEvent) callbacks_.clear();
}

void CompileJsToWasmWrappers(Isolate* isolate, const WasmModule* module,
                             Handle<FixedArray> export_wrappers) {
  JSToWasmWrapperCache js_to_wasm_cache;
  int wrapper_index = 0;

  // TODO(6792): Wrappers below are allocated with {Factory::NewCode}. As an
  // optimization we keep the code space unlocked to avoid repeated unlocking
  // because many such wrapper are allocated in sequence below.
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
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
      VectorOf(reinterpret_cast<uint8_t*>(buffer), name_chars), TENURED);
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

#undef TRACE_COMPILE
#undef TRACE_STREAMING
#undef TRACE_LAZY

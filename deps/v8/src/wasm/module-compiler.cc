// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-compiler.h"

#include "src/api.h"
#include "src/asmjs/asm-js.h"
#include "src/assembler-inl.h"
#include "src/base/optional.h"
#include "src/base/template-utils.h"
#include "src/base/utils/random-number-generator.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/compiler/wasm-compiler.h"
#include "src/counters.h"
#include "src/identity-map.h"
#include "src/property-descriptor.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-code-specialization.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-memory.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_wasm_instances) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_CHAIN(instance)        \
  do {                               \
    instance->PrintInstancesChain(); \
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
  kFailedCompilation,
  kDestroyed
};

enum class CompileMode : uint8_t { kRegular, kTiering };

// The CompilationState keeps track of the compilation state of the
// owning NativeModule, i.e. which functions are left to be compiled.
// It contains a task manager to allow parallel and asynchronous background
// compilation of functions.
class CompilationState {
 public:
  CompilationState(internal::Isolate* isolate, ModuleEnv& env);
  ~CompilationState();

  // Needs to be set before {AddCompilationUnits} is run, which triggers
  // background compilation.
  void SetNumberOfFunctionsToCompile(size_t num_functions);
  void AddCallback(
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
                                WasmCompilationUnit::CompilationMode mode);

  void CancelAndWait();
  void OnBackgroundTaskStopped();
  void RestartBackgroundTasks(size_t max = std::numeric_limits<size_t>::max());
  // Only one foreground thread (finisher) is allowed to run at a time.
  // {SetFinisherIsRunning} returns whether the flag changed its state.
  bool SetFinisherIsRunning(bool value);
  void ScheduleFinisherTask();

  bool StopBackgroundCompilationTaskForThrottling();

  void Abort();

  Isolate* isolate() const { return isolate_; }

  bool failed() const {
    base::LockGuard<base::Mutex> guard(&mutex_);
    return failed_;
  }

  bool baseline_compilation_finished() const {
    return baseline_compilation_finished_;
  }

  CompileMode compile_mode() const { return compile_mode_; }

  ModuleEnv* module_env() { return &module_env_; }

  const ModuleWireBytes& wire_bytes() const { return wire_bytes_; }

  void SetWireBytes(const ModuleWireBytes& wire_bytes) {
    DCHECK_NULL(bytes_copy_);
    DCHECK_EQ(0, wire_bytes_.length());
    bytes_copy_ = std::unique_ptr<byte[]>(new byte[wire_bytes.length()]);
    memcpy(bytes_copy_.get(), wire_bytes.start(), wire_bytes.length());
    wire_bytes_ = ModuleWireBytes(bytes_copy_.get(),
                                  bytes_copy_.get() + wire_bytes.length());
  }

 private:
  void NotifyOnEvent(CompilationEvent event, ErrorThrower* thrower);

  std::vector<std::unique_ptr<WasmCompilationUnit>>& finish_units() {
    return baseline_compilation_finished_ ? tiering_finish_units_
                                          : baseline_finish_units_;
  }

  Isolate* const isolate_;
  ModuleEnv module_env_;
  const size_t max_memory_;
  const CompileMode compile_mode_;
  bool baseline_compilation_finished_ = false;

  // TODO(wasm): eventually we want to get rid of this
  // additional copy (see AsyncCompileJob).
  std::unique_ptr<byte[]> bytes_copy_;
  ModuleWireBytes wire_bytes_;

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

  size_t allocated_memory_ = 0;

  // End of fields protected by {mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  // TODO(mstarzinger): We should make sure this allows at most one callback
  // to exist for each {CompilationState} because reifying the error object on
  // the given {ErrorThrower} can be done at most once.
  std::vector<std::function<void(CompilationEvent, ErrorThrower*)>> callbacks_;

  // When canceling the background_task_manager_, use {CancelAndWait} on
  // the CompilationState in order to cleanly clean up.
  CancelableTaskManager background_task_manager_;
  CancelableTaskManager foreground_task_manager_;
  std::shared_ptr<v8::TaskRunner> background_task_runner_;
  std::shared_ptr<v8::TaskRunner> foreground_task_runner_;

  const size_t max_background_tasks_ = 0;

  size_t outstanding_units_ = 0;
  size_t num_tiering_units_ = 0;
};

namespace {

class JSToWasmWrapperCache {
 public:
  Handle<Code> CloneOrCompileJSToWasmWrapper(
      Isolate* isolate, wasm::WasmModule* module, Address call_target,
      uint32_t index, wasm::UseTrapHandler use_trap_handler) {
    const bool is_import = index < module->num_imported_functions;
    DCHECK_EQ(is_import, call_target == kNullAddress);
    const wasm::WasmFunction* func = &module->functions[index];
    // We cannot cache js-to-wasm wrappers for imports, as they hard-code the
    // function index.
    if (!is_import) {
      int cached_idx = sig_map_.Find(func->sig);
      if (cached_idx >= 0) {
        Handle<Code> code =
            isolate->factory()->CopyCode(code_cache_[cached_idx]);
        // Now patch the call to wasm code.
        RelocIterator it(*code,
                         RelocInfo::ModeMask(RelocInfo::JS_TO_WASM_CALL));
        // If there is no reloc info, then it's an incompatible signature or
        // calls an import.
        if (!it.done()) it.rinfo()->set_js_to_wasm_address(call_target);
        return code;
      }
    }

    Handle<Code> code = compiler::CompileJSToWasmWrapper(
        isolate, module, call_target, index, use_trap_handler);
    if (!is_import) {
      uint32_t new_cache_idx = sig_map_.FindOrInsert(func->sig);
      DCHECK_EQ(code_cache_.size(), new_cache_idx);
      USE(new_cache_idx);
      code_cache_.push_back(code);
    }
    return code;
  }

 private:
  // sig_map_ maps signatures to an index in code_cache_.
  wasm::SignatureMap sig_map_;
  std::vector<Handle<Code>> code_cache_;
};

// A helper class to simplify instantiating a module from a compiled module.
// It closes over the {Isolate}, the {ErrorThrower}, the {WasmCompiledModule},
// etc.
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
  WasmModule* const module_;
  const std::shared_ptr<Counters> async_counters_;
  ErrorThrower* thrower_;
  Handle<WasmModuleObject> module_object_;
  MaybeHandle<JSReceiver> ffi_;
  MaybeHandle<JSArrayBuffer> memory_;
  Handle<JSArrayBuffer> globals_;
  Handle<WasmCompiledModule> compiled_module_;
  std::vector<TableInstance> table_instances_;
  std::vector<Handle<JSFunction>> js_wrappers_;
  Handle<WasmExportedFunction> start_function_;
  JSToWasmWrapperCache js_to_wasm_cache_;
  std::vector<SanitizedImport> sanitized_imports_;

  const std::shared_ptr<Counters>& async_counters() const {
    return async_counters_;
  }

  Counters* counters() const { return async_counters().get(); }

  wasm::UseTrapHandler use_trap_handler() const {
    return compiled_module_->GetNativeModule()->use_trap_handler()
               ? kUseTrapHandler
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

  void WriteGlobalValue(WasmGlobal& global, double value);
  void WriteGlobalValue(WasmGlobal& global, Handle<WasmGlobalObject> value);

  void SanitizeImports();

  // Process the imports, including functions, tables, globals, and memory, in
  // order, loading them from the {ffi_} object. Returns the number of imported
  // functions.
  int ProcessImports(Handle<WasmInstanceObject> instance);

  template <typename T>
  T* GetRawGlobalPtr(WasmGlobal& global);

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
  return {};
}

// A helper class to prevent pathological patching behavior for indirect
// references to code which must be updated after lazy compiles.
// Utilizes a reverse mapping to prevent O(n^2) behavior.
class IndirectPatcher {
 public:
  void Patch(Handle<WasmInstanceObject> caller_instance,
             Handle<WasmInstanceObject> target_instance, int func_index,
             Address old_target, Address new_target) {
    TRACE_LAZY(
        "IndirectPatcher::Patch(caller=%p, target=%p, func_index=%i, "
        "old_target=%" PRIuPTR ", new_target=%" PRIuPTR ")\n",
        *caller_instance, *target_instance, func_index, old_target, new_target);
    if (mapping_.size() == 0 || misses_ >= kMaxMisses) {
      BuildMapping(caller_instance);
    }
    // Patch entries for the given function index.
    WasmCodeManager* code_manager =
        caller_instance->GetIsolate()->wasm_engine()->code_manager();
    USE(code_manager);
    auto& entries = mapping_[func_index];
    int patched = 0;
    for (auto index : entries) {
      if (index < 0) {
        // Imported function entry.
        int i = -1 - index;
        ImportedFunctionEntry entry(caller_instance, i);
        if (entry.target() == old_target) {
          DCHECK_EQ(
              func_index,
              code_manager->GetCodeFromStartAddress(entry.target())->index());
          entry.set_wasm_to_wasm(*target_instance, new_target);
          patched++;
        }
      } else {
        // Indirect function table entry.
        int i = index;
        IndirectFunctionTableEntry entry(caller_instance, i);
        if (entry.target() == old_target) {
          DCHECK_EQ(
              func_index,
              code_manager->GetCodeFromStartAddress(entry.target())->index());
          entry.set(entry.sig_id(), *target_instance, new_target);
          patched++;
        }
      }
    }
    if (patched == 0) misses_++;
  }

 private:
  void BuildMapping(Handle<WasmInstanceObject> caller_instance) {
    mapping_.clear();
    misses_ = 0;
    TRACE_LAZY("BuildMapping for (caller=%p)...\n", *caller_instance);
    Isolate* isolate = caller_instance->GetIsolate();
    WasmCodeManager* code_manager = isolate->wasm_engine()->code_manager();
    uint32_t num_imported_functions =
        caller_instance->module()->num_imported_functions;
    // Process the imported function entries.
    for (unsigned i = 0; i < num_imported_functions; i++) {
      ImportedFunctionEntry entry(caller_instance, i);
      WasmCode* code = code_manager->GetCodeFromStartAddress(entry.target());
      if (code->kind() != WasmCode::kLazyStub) continue;
      TRACE_LAZY(" +import[%u] -> #%d (%p)\n", i, code->index(),
                 code->instructions().start());
      DCHECK(!entry.is_js_receiver_entry());
      WasmInstanceObject* target_instance = entry.instance();
      WasmCode* new_code =
          target_instance->compiled_module()->GetNativeModule()->code(
              code->index());
      if (new_code->kind() != WasmCode::kLazyStub) {
        // Patch an imported function entry which is already compiled.
        entry.set_wasm_to_wasm(target_instance, new_code->instruction_start());
      } else {
        int key = code->index();
        int index = -1 - i;
        mapping_[key].push_back(index);
      }
    }
    // Process the indirect function table entries.
    size_t ift_size = caller_instance->indirect_function_table_size();
    for (unsigned i = 0; i < ift_size; i++) {
      IndirectFunctionTableEntry entry(caller_instance, i);
      if (entry.target() == kNullAddress) continue;  // null IFT entry
      WasmCode* code = code_manager->GetCodeFromStartAddress(entry.target());
      if (code->kind() != WasmCode::kLazyStub) continue;
      TRACE_LAZY(" +indirect[%u] -> #%d (lazy:%p)\n", i, code->index(),
                 code->instructions().start());
      WasmInstanceObject* target_instance = entry.instance();
      WasmCode* new_code =
          target_instance->compiled_module()->GetNativeModule()->code(
              code->index());
      if (new_code->kind() != WasmCode::kLazyStub) {
        // Patch an indirect function table entry which is already compiled.
        entry.set(entry.sig_id(), target_instance,
                  new_code->instruction_start());
      } else {
        int key = code->index();
        int index = i;
        mapping_[key].push_back(index);
      }
    }
  }

  static constexpr int kMaxMisses = 5;  // maximum misses before rebuilding
  std::unordered_map<int, std::vector<int>> mapping_;
  int misses_ = 0;
};

ModuleEnv CreateModuleEnvFromModuleObject(
    Isolate* isolate, Handle<WasmModuleObject> module_object) {
  WasmModule* module = module_object->shared()->module();
  wasm::UseTrapHandler use_trap_handler =
      module_object->compiled_module()->GetNativeModule()->use_trap_handler()
          ? kUseTrapHandler
          : kNoTrapHandler;
  return ModuleEnv(module, use_trap_handler, wasm::kRuntimeExceptionSupport);
}

const wasm::WasmCode* LazyCompileFunction(
    Isolate* isolate, Handle<WasmModuleObject> module_object, int func_index) {
  base::ElapsedTimer compilation_timer;
  NativeModule* native_module =
      module_object->compiled_module()->GetNativeModule();
  wasm::WasmCode* existing_code =
      native_module->code(static_cast<uint32_t>(func_index));
  if (existing_code != nullptr &&
      existing_code->kind() == wasm::WasmCode::kFunction) {
    TRACE_LAZY("Function %d already compiled.\n", func_index);
    return existing_code;
  }

  compilation_timer.Start();
  // TODO(wasm): Refactor this to only get the name if it is really needed for
  // tracing / debugging.
  std::string func_name;
  {
    WasmName name = Vector<const char>::cast(
        module_object->shared()->GetRawFunctionName(func_index));
    // Copy to std::string, because the underlying string object might move on
    // the heap.
    func_name.assign(name.start(), static_cast<size_t>(name.length()));
  }

  TRACE_LAZY("Compiling function %s, %d.\n", func_name.c_str(), func_index);

  ModuleEnv module_env =
      CreateModuleEnvFromModuleObject(isolate, module_object);

  const uint8_t* module_start =
      module_object->shared()->module_bytes()->GetChars();

  const WasmFunction* func = &module_env.module->functions[func_index];
  FunctionBody body{func->sig, func->code.offset(),
                    module_start + func->code.offset(),
                    module_start + func->code.end_offset()};

  ErrorThrower thrower(isolate, "WasmLazyCompile");
  WasmCompilationUnit unit(isolate, &module_env, native_module, body,
                           CStrVector(func_name.c_str()), func_index,
                           CodeFactory::CEntry(isolate));
  unit.ExecuteCompilation();
  wasm::WasmCode* wasm_code = unit.FinishCompilation(&thrower);

  if (wasm::WasmCode::ShouldBeLogged(isolate)) wasm_code->LogCode(isolate);

  // If there is a pending error, something really went wrong. The module was
  // verified before starting execution with lazy compilation.
  // This might be OOM, but then we cannot continue execution anyway.
  // TODO(clemensh): According to the spec, we can actually skip validation at
  // module creation time, and return a function that always traps here.
  CHECK(!thrower.error());

  // Now specialize the generated code for this instance.
  CodeSpecialization code_specialization;
  code_specialization.RelocateDirectCalls(native_module);
  code_specialization.ApplyToWasmCode(wasm_code, SKIP_ICACHE_FLUSH);
  int64_t func_size =
      static_cast<int64_t>(func->code.end_offset() - func->code.offset());
  int64_t compilation_time = compilation_timer.Elapsed().InMicroseconds();

  auto counters = isolate->counters();
  counters->wasm_lazily_compiled_functions()->Increment();

  Assembler::FlushICache(wasm_code->instructions().start(),
                         wasm_code->instructions().size());
  counters->wasm_generated_code_size()->Increment(
      static_cast<int>(wasm_code->instructions().size()));
  counters->wasm_reloc_size()->Increment(
      static_cast<int>(wasm_code->reloc_info().size()));

  counters->wasm_lazy_compilation_throughput()->AddSample(
      compilation_time != 0 ? static_cast<int>(func_size / compilation_time)
                            : 0);

  if (trap_handler::IsTrapHandlerEnabled()) {
    wasm_code->RegisterTrapHandlerData();
  }
  return wasm_code;
}

namespace {

int AdvanceSourcePositionTableIterator(SourcePositionTableIterator& iterator,
                                       int offset) {
  DCHECK(!iterator.done());
  int byte_pos;
  do {
    byte_pos = iterator.source_position().ScriptOffset();
    iterator.Advance();
  } while (!iterator.done() && iterator.code_offset() <= offset);
  return byte_pos;
}

const wasm::WasmCode* LazyCompileFromJsToWasm(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    Handle<Code> js_to_wasm_caller, uint32_t callee_func_index) {
  Decoder decoder(nullptr, nullptr);
  Handle<WasmModuleObject> module_object(instance->module_object());
  NativeModule* native_module = instance->compiled_module()->GetNativeModule();

  TRACE_LAZY(
      "Starting lazy compilation (func %u, js_to_wasm: true, patch caller: "
      "true). \n",
      callee_func_index);
  LazyCompileFunction(isolate, module_object, callee_func_index);
  {
    DisallowHeapAllocation no_gc;
    CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
    RelocIterator it(*js_to_wasm_caller,
                     RelocInfo::ModeMask(RelocInfo::JS_TO_WASM_CALL));
    DCHECK(!it.done());
    const wasm::WasmCode* callee_compiled =
        native_module->code(callee_func_index);
    DCHECK_NOT_NULL(callee_compiled);
    DCHECK_EQ(WasmCode::kLazyStub,
              isolate->wasm_engine()
                  ->code_manager()
                  ->GetCodeFromStartAddress(it.rinfo()->js_to_wasm_address())
                  ->kind());
    it.rinfo()->set_js_to_wasm_address(callee_compiled->instruction_start());
    TRACE_LAZY("Patched 1 location in js-to-wasm %p.\n", *js_to_wasm_caller);

#ifdef DEBUG
    it.next();
    DCHECK(it.done());
#endif
  }

  wasm::WasmCode* ret = native_module->code(callee_func_index);
  DCHECK_NOT_NULL(ret);
  DCHECK_EQ(wasm::WasmCode::kFunction, ret->kind());
  return ret;
}

const wasm::WasmCode* LazyCompileIndirectCall(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    uint32_t func_index) {
  TRACE_LAZY(
      "Starting lazy compilation (func %u, js_to_wasm: false, patch caller: "
      "false). \n",
      func_index);
  Handle<WasmModuleObject> module_object(instance->module_object());
  return LazyCompileFunction(isolate, module_object, func_index);
}

const wasm::WasmCode* LazyCompileDirectCall(Isolate* isolate,
                                            Handle<WasmInstanceObject> instance,
                                            const wasm::WasmCode* wasm_caller,
                                            int32_t caller_ret_offset) {
  DCHECK_LE(0, caller_ret_offset);

  Decoder decoder(nullptr, nullptr);

  // Gather all the targets of direct calls inside the code of {wasm_caller}
  // and place their function indexes in {direct_callees}.
  std::vector<int32_t> direct_callees;
  // The last one before {caller_ret_offset} must be the call that triggered
  // this lazy compilation.
  int callee_pos = -1;
  uint32_t num_non_compiled_callees = 0;  // For stats.
  {
    DisallowHeapAllocation no_gc;
    Handle<WasmSharedModuleData> shared(
        wasm_caller->native_module()->shared_module_data(), isolate);
    SeqOneByteString* module_bytes = shared->module_bytes();
    uint32_t caller_func_index = wasm_caller->index();
    SourcePositionTableIterator source_pos_iterator(
        wasm_caller->source_positions());

    const byte* func_bytes =
        module_bytes->GetChars() +
        shared->module()->functions[caller_func_index].code.offset();
    for (RelocIterator it(wasm_caller->instructions(),
                          wasm_caller->reloc_info(),
                          wasm_caller->constant_pool(),
                          RelocInfo::ModeMask(RelocInfo::WASM_CALL));
         !it.done(); it.next()) {
      // TODO(clemensh): Introduce safe_cast<T, bool> which (D)CHECKS
      // (depending on the bool) against limits of T and then static_casts.
      size_t offset_l = it.rinfo()->pc() - wasm_caller->instruction_start();
      DCHECK_GE(kMaxInt, offset_l);
      int offset = static_cast<int>(offset_l);
      int byte_pos =
          AdvanceSourcePositionTableIterator(source_pos_iterator, offset);

      WasmCode* callee = isolate->wasm_engine()->code_manager()->LookupCode(
          it.rinfo()->target_address());
      if (callee->kind() == WasmCode::kLazyStub) {
        // The callee has not been compiled.
        ++num_non_compiled_callees;
        int32_t callee_func_index =
            ExtractDirectCallIndex(decoder, func_bytes + byte_pos);
        DCHECK_LT(callee_func_index,
                  wasm_caller->native_module()->function_count());
        // {caller_ret_offset} points to one instruction after the call.
        // Remember the last called function before that offset.
        if (offset < caller_ret_offset) {
          callee_pos = static_cast<int>(direct_callees.size());
        }
        direct_callees.push_back(callee_func_index);
      } else {
        // If the callee is not the lazy compile stub, assume this callee
        // has already been compiled.
        direct_callees.push_back(-1);
        continue;
      }
    }

    TRACE_LAZY("Found %d non-compiled callees in function=%p.\n",
               num_non_compiled_callees, wasm_caller);
    USE(num_non_compiled_callees);
  }
  CHECK_LE(0, callee_pos);

  // TODO(wasm): compile all functions in non_compiled_callees in
  // background, wait for direct_callees[callee_pos].
  auto callee_func_index = direct_callees[callee_pos];
  TRACE_LAZY(
      "Starting lazy compilation (function=%p retaddr=+%d direct_callees[%d] "
      "-> %d).\n",
      wasm_caller, caller_ret_offset, callee_pos, callee_func_index);

  Handle<WasmModuleObject> module_object(instance->module_object());
  NativeModule* native_module = instance->compiled_module()->GetNativeModule();
  const WasmCode* ret =
      LazyCompileFunction(isolate, module_object, callee_func_index);
  DCHECK_NOT_NULL(ret);

  int patched = 0;
  {
    // Now patch the code in {wasm_caller} with all functions which are now
    // compiled. This will pick up any other compiled functions, not only {ret}.
    size_t pos = 0;
    for (RelocIterator
             it(wasm_caller->instructions(), wasm_caller->reloc_info(),
                wasm_caller->constant_pool(),
                RelocInfo::ModeMask(RelocInfo::WASM_CALL));
         !it.done(); it.next(), ++pos) {
      auto callee_index = direct_callees[pos];
      if (callee_index < 0) continue;  // callee already compiled.
      const WasmCode* callee_compiled = native_module->code(callee_index);
      if (callee_compiled->kind() != WasmCode::kFunction) continue;
      DCHECK_EQ(WasmCode::kLazyStub,
                isolate->wasm_engine()
                    ->code_manager()
                    ->GetCodeFromStartAddress(it.rinfo()->wasm_call_address())
                    ->kind());
      it.rinfo()->set_wasm_call_address(callee_compiled->instruction_start());
      ++patched;
    }
    DCHECK_EQ(direct_callees.size(), pos);
  }

  DCHECK_LT(0, patched);
  TRACE_LAZY("Patched %d calls(s) in %p.\n", patched, wasm_caller);
  USE(patched);

  return ret;
}

}  // namespace

Address CompileLazy(Isolate* isolate,
                    Handle<WasmInstanceObject> target_instance) {
  HistogramTimerScope lazy_time_scope(
      isolate->counters()->wasm_lazy_compilation_time());

  //==========================================================================
  // Begin stack walk.
  //==========================================================================
  StackFrameIterator it(isolate);

  //==========================================================================
  // First frame: C entry stub.
  //==========================================================================
  DCHECK(!it.done());
  DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
  it.Advance();

  //==========================================================================
  // Second frame: WasmCompileLazy builtin.
  //==========================================================================
  DCHECK(!it.done());
  int target_func_index = -1;
  bool indirectly_called = false;
  const wasm::WasmCode* lazy_stub =
      isolate->wasm_engine()->code_manager()->LookupCode(it.frame()->pc());
  CHECK_EQ(wasm::WasmCode::kLazyStub, lazy_stub->kind());
  if (!lazy_stub->IsAnonymous()) {
    // If the lazy stub is not "anonymous", then its copy encodes the target
    // function index. Used for import and indirect calls.
    target_func_index = lazy_stub->index();
    indirectly_called = true;
  }
  it.Advance();

  //==========================================================================
  // Third frame: The calling wasm code (direct or indirect), or js-to-wasm
  // wrapper.
  //==========================================================================
  DCHECK(!it.done());
  DCHECK(it.frame()->is_js_to_wasm() || it.frame()->is_wasm_compiled());
  Handle<Code> js_to_wasm_caller_code;
  Handle<WasmInstanceObject> caller_instance;
  const WasmCode* wasm_caller_code = nullptr;
  int32_t caller_ret_offset = -1;
  if (it.frame()->is_js_to_wasm()) {
    js_to_wasm_caller_code = handle(it.frame()->LookupCode(), isolate);
    // This wasn't actually an indirect call, but a JS->wasm call.
    indirectly_called = false;
  } else {
    caller_instance =
        handle(WasmCompiledFrame::cast(it.frame())->wasm_instance(), isolate);
    wasm_caller_code =
        isolate->wasm_engine()->code_manager()->LookupCode(it.frame()->pc());
    auto offset = it.frame()->pc() - wasm_caller_code->instruction_start();
    caller_ret_offset = static_cast<int32_t>(offset);
    DCHECK_EQ(offset, caller_ret_offset);
  }

  //==========================================================================
  // Begin compilation.
  //==========================================================================
  Handle<WasmCompiledModule> compiled_module(
      target_instance->compiled_module());

  NativeModule* native_module = compiled_module->GetNativeModule();
  DCHECK(!native_module->lazy_compile_frozen());

  NativeModuleModificationScope native_module_modification_scope(native_module);

  const wasm::WasmCode* result = nullptr;

  if (!js_to_wasm_caller_code.is_null()) {
    result = LazyCompileFromJsToWasm(isolate, target_instance,
                                     js_to_wasm_caller_code, target_func_index);
    DCHECK_NOT_NULL(result);
    DCHECK_EQ(target_func_index, result->index());
  } else {
    DCHECK_NOT_NULL(wasm_caller_code);
    if (target_func_index < 0) {
      result = LazyCompileDirectCall(isolate, target_instance, wasm_caller_code,
                                     caller_ret_offset);
      DCHECK_NOT_NULL(result);
    } else {
      result =
          LazyCompileIndirectCall(isolate, target_instance, target_func_index);
      DCHECK_NOT_NULL(result);
    }
  }

  //==========================================================================
  // Update import and indirect function tables in the caller.
  //==========================================================================
  if (indirectly_called) {
    DCHECK(!caller_instance.is_null());
    if (!caller_instance->has_managed_indirect_patcher()) {
      auto patcher = Managed<IndirectPatcher>::Allocate(isolate);
      caller_instance->set_managed_indirect_patcher(*patcher);
    }
    IndirectPatcher* patcher = Managed<IndirectPatcher>::cast(
                                   caller_instance->managed_indirect_patcher())
                                   ->raw();
    Address old_target = lazy_stub->instruction_start();
    patcher->Patch(caller_instance, target_instance, target_func_index,
                   old_target, result->instruction_start());
  }

  return result->instruction_start();
}

namespace {
bool compile_lazy(const WasmModule* module) {
  return FLAG_wasm_lazy_compilation ||
         (FLAG_asm_wasm_lazy_compilation && module->is_asm_js());
}

void FlushICache(const wasm::NativeModule* native_module) {
  for (uint32_t i = native_module->num_imported_functions(),
                e = native_module->function_count();
       i < e; ++i) {
    const wasm::WasmCode* code = native_module->code(i);
    if (code == nullptr) continue;
    Assembler::FlushICache(code->instructions().start(),
                           code->instructions().size());
  }
}

void FlushICache(Handle<FixedArray> functions) {
  for (int i = 0, e = functions->length(); i < e; ++i) {
    if (!functions->get(i)->IsCode()) continue;
    Code* code = Code::cast(functions->get(i));
    Assembler::FlushICache(code->raw_instruction_start(),
                           code->raw_instruction_size());
  }
}

byte* raw_buffer_ptr(MaybeHandle<JSArrayBuffer> buffer, int offset) {
  return static_cast<byte*>(buffer.ToHandleChecked()->backing_store()) + offset;
}

void RecordStats(const Code* code, Counters* counters) {
  counters->wasm_generated_code_size()->Increment(code->body_size());
  counters->wasm_reloc_size()->Increment(code->relocation_info()->length());
}

void RecordStats(const wasm::WasmCode* code, Counters* counters) {
  counters->wasm_generated_code_size()->Increment(
      static_cast<int>(code->instructions().size()));
  counters->wasm_reloc_size()->Increment(
      static_cast<int>(code->reloc_info().size()));
}

void RecordStats(const wasm::NativeModule* native_module, Counters* counters) {
  for (uint32_t i = native_module->num_imported_functions(),
                e = native_module->function_count();
       i < e; ++i) {
    const wasm::WasmCode* code = native_module->code(i);
    if (code != nullptr) RecordStats(code, counters);
  }
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

ModuleEnv CreateDefaultModuleEnv(WasmModule* module) {
  // TODO(kschimpf): Add module-specific policy handling here (see v8:7143)?
  UseTrapHandler use_trap_handler =
      trap_handler::IsTrapHandlerEnabled() ? kUseTrapHandler : kNoTrapHandler;
  return ModuleEnv(module, use_trap_handler, kRuntimeExceptionSupport);
}

Handle<WasmCompiledModule> NewCompiledModule(Isolate* isolate,
                                             WasmModule* module,
                                             ModuleEnv& env) {
  Handle<WasmCompiledModule> compiled_module =
      WasmCompiledModule::New(isolate, module, env);
  return compiled_module;
}

size_t GetMaxUsableMemorySize(Isolate* isolate) {
  return isolate->heap()->memory_allocator()->code_range()->valid()
             ? isolate->heap()->memory_allocator()->code_range()->size()
             : isolate->heap()->code_space()->Capacity();
}

// The CompilationUnitBuilder builds compilation units and stores them in an
// internal buffer. The buffer is moved into the working queue of the
// CompilationState when {Commit} is called.
class CompilationUnitBuilder {
 public:
  explicit CompilationUnitBuilder(NativeModule* native_module,
                                  Handle<Code> centry_stub)
      : native_module_(native_module),
        compilation_state_(native_module->compilation_state()),
        centry_stub_(centry_stub) {}

  void AddUnit(const WasmFunction* function, uint32_t buffer_offset,
               Vector<const uint8_t> bytes, WasmName name) {
    switch (compilation_state_->compile_mode()) {
      case CompileMode::kTiering:
        tiering_units_.emplace_back(
            CreateUnit(function, buffer_offset, bytes, name,
                       WasmCompilationUnit::CompilationMode::kTurbofan));
        baseline_units_.emplace_back(
            CreateUnit(function, buffer_offset, bytes, name,
                       WasmCompilationUnit::CompilationMode::kLiftoff));
        return;
      case CompileMode::kRegular:
        baseline_units_.emplace_back(
            CreateUnit(function, buffer_offset, bytes, name,
                       WasmCompilationUnit::GetDefaultCompilationMode()));
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
  std::unique_ptr<WasmCompilationUnit> CreateUnit(
      const WasmFunction* function, uint32_t buffer_offset,
      Vector<const uint8_t> bytes, WasmName name,
      WasmCompilationUnit::CompilationMode mode) {
    return base::make_unique<WasmCompilationUnit>(
        compilation_state_->isolate(), compilation_state_->module_env(),
        native_module_,
        wasm::FunctionBody{function->sig, buffer_offset, bytes.begin(),
                           bytes.end()},
        name, function->func_index, centry_stub_, mode,
        compilation_state_->isolate()->async_counters().get());
  }

  NativeModule* native_module_;
  CompilationState* compilation_state_;
  Handle<Code> centry_stub_;
  std::vector<std::unique_ptr<WasmCompilationUnit>> baseline_units_;
  std::vector<std::unique_ptr<WasmCompilationUnit>> tiering_units_;
};

// Run by each compilation task and by the main thread (i.e. in both
// foreground and background threads). The no_finisher_callback is called
// within the result_mutex_ lock when no finishing task is running, i.e. when
// the finisher_is_running_ flag is not set.
bool FetchAndExecuteCompilationUnit(CompilationState* compilation_state) {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;
  DisallowCodeDependencyChange no_dependency_change;

  std::unique_ptr<WasmCompilationUnit> unit =
      compilation_state->GetNextCompilationUnit();
  if (unit == nullptr) return false;

  // TODO(kimanh): We need to find out in which mode the unit
  // should be compiled in before compiling it, as it might fallback
  // to Turbofan if it cannot be compiled using Liftoff. This can be removed
  // later as soon as Liftoff can compile any function. Then, we can directly
  // access {unit->mode()} within {ScheduleUnitForFinishing()}.
  WasmCompilationUnit::CompilationMode mode = unit->mode();
  unit->ExecuteCompilation();
  compilation_state->ScheduleUnitForFinishing(std::move(unit), mode);

  return true;
}

size_t GetNumFunctionsToCompile(const WasmModule* wasm_module) {
  // TODO(kimanh): Remove, FLAG_skip_compiling_wasm_funcs: previously used for
  // debugging, and now not necessarily working anymore.
  uint32_t start =
      wasm_module->num_imported_functions + FLAG_skip_compiling_wasm_funcs;
  uint32_t num_funcs = static_cast<uint32_t>(wasm_module->functions.size());
  uint32_t funcs_to_compile = start > num_funcs ? 0 : num_funcs - start;
  return funcs_to_compile;
}

void InitializeCompilationUnits(const std::vector<WasmFunction>& functions,
                                const ModuleWireBytes& wire_bytes,
                                const WasmModule* wasm_module,
                                Handle<Code> centry_stub,
                                NativeModule* native_module) {
  uint32_t start =
      wasm_module->num_imported_functions + FLAG_skip_compiling_wasm_funcs;
  uint32_t num_funcs = static_cast<uint32_t>(functions.size());

  CompilationUnitBuilder builder(native_module, centry_stub);
  for (uint32_t i = start; i < num_funcs; ++i) {
    const WasmFunction* func = &functions[i];
    uint32_t buffer_offset = func->code.offset();
    Vector<const uint8_t> bytes(wire_bytes.start() + func->code.offset(),
                                func->code.end_offset() - func->code.offset());

    WasmName name = wire_bytes.GetName(func, wasm_module);
    DCHECK_NOT_NULL(native_module);
    builder.AddUnit(func, buffer_offset, bytes, name);
  }
  builder.Commit();
}

void FinishCompilationUnits(CompilationState* compilation_state,
                            ErrorThrower* thrower) {
  while (true) {
    if (compilation_state->failed()) break;
    std::unique_ptr<WasmCompilationUnit> unit =
        compilation_state->GetNextExecutedUnit();
    if (unit == nullptr) break;
    wasm::WasmCode* result = unit->FinishCompilation(thrower);

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

void UpdateAllCompiledModulesWithTopTierCode(
    Handle<WasmModuleObject> module_object) {
  WasmModule* module = module_object->shared()->module();
  DCHECK_GT(module->functions.size() - module->num_imported_functions, 0);
  USE(module);

  CodeSpaceMemoryModificationScope modification_scope(
      module_object->GetIsolate()->heap());

  NativeModule* native_module =
      module_object->compiled_module()->GetNativeModule();

  // Link.
  CodeSpecialization code_specialization;
  code_specialization.RelocateDirectCalls(native_module);
  code_specialization.ApplyToWholeModule(native_module, module_object);
}

void CompileInParallel(Isolate* isolate, NativeModule* native_module,
                       const ModuleWireBytes& wire_bytes, ModuleEnv* module_env,
                       Handle<WasmModuleObject> module_object,
                       Handle<Code> centry_stub, ErrorThrower* thrower) {
  const WasmModule* module = module_env->module;
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
  size_t functions_count = GetNumFunctionsToCompile(module);
  compilation_state->SetNumberOfFunctionsToCompile(functions_count);
  compilation_state->SetWireBytes(wire_bytes);

  DeferredHandles* deferred_handles = nullptr;
  Handle<Code> centry_deferred = centry_stub;
  Handle<WasmModuleObject> module_object_deferred;
  if (compilation_state->compile_mode() == CompileMode::kTiering) {
    // Open a deferred handle scope for the centry_stub, in order to allow
    // for background tiering compilation.
    DeferredHandleScope deferred(isolate);
    centry_deferred = Handle<Code>(*centry_stub, isolate);
    module_object_deferred = handle(*module_object, isolate);
    deferred_handles = deferred.Detach();
  }
  compilation_state->AddCallback(
      [module_object_deferred, deferred_handles](
          // Callback is called from a foreground thread.
          CompilationEvent event, ErrorThrower* thrower) mutable {
        switch (event) {
          case CompilationEvent::kFinishedBaselineCompilation:
            // Nothing to do, since we are finishing baseline compilation
            // in this foreground thread.
            return;
          case CompilationEvent::kFinishedTopTierCompilation:
            UpdateAllCompiledModulesWithTopTierCode(module_object_deferred);
            // TODO(wasm): Currently compilation has to finish before the
            // {deferred_handles} can be removed. We need to make sure that
            // we can clean it up at a time when the native module
            // should die (but currently cannot, since it's kept alive
            // through the {deferred_handles} themselves).
            delete deferred_handles;
            deferred_handles = nullptr;
            return;
          case CompilationEvent::kFailedCompilation:
            // If baseline compilation failed, we will reflect this without
            // a callback, in this thread through {thrower}.
            // Tier-up compilation should not fail if baseline compilation
            // did not fail.
            DCHECK(!module_object_deferred->compiled_module()
                        ->GetNativeModule()
                        ->compilation_state()
                        ->baseline_compilation_finished());
            delete deferred_handles;
            deferred_handles = nullptr;
            return;
          case CompilationEvent::kDestroyed:
            if (deferred_handles) delete deferred_handles;
            return;
        }
        UNREACHABLE();
      });

  // 1) The main thread allocates a compilation unit for each wasm function
  //    and stores them in the vector {compilation_units} within the
  //    {compilation_state}. By adding units to the {compilation_state}, new
  //    {BackgroundCompileTask} instances are spawned which run on
  //    background threads.
  InitializeCompilationUnits(module->functions, compilation_state->wire_bytes(),
                             module, centry_deferred, native_module);

  // 2.a) The background threads and the main thread pick one compilation
  //      unit at a time and execute the parallel phase of the compilation
  //      unit. After finishing the execution of the parallel phase, the
  //      result is enqueued in {baseline_finish_units_}.
  //      The foreground task bypasses waiting on memory threshold, because
  //      its results will immediately be converted to code (below).
  while (FetchAndExecuteCompilationUnit(compilation_state) &&
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
                         const ModuleWireBytes& wire_bytes,
                         ModuleEnv* module_env, ErrorThrower* thrower) {
  DCHECK(!thrower->error());

  const WasmModule* module = module_env->module;
  for (uint32_t i = FLAG_skip_compiling_wasm_funcs;
       i < module->functions.size(); ++i) {
    const WasmFunction& func = module->functions[i];
    if (func.imported) continue;  // Imports are compiled at instantiation time.

    // Compile the function.
    wasm::WasmCode* code = WasmCompilationUnit::CompileWasmFunction(
        native_module, thrower, isolate, wire_bytes, module_env, &func);
    if (code == nullptr) {
      TruncatedUserString<> name(wire_bytes.GetName(&func, module));
      thrower->CompileError("Compilation of #%d:%.*s failed.", i, name.length(),
                            name.start());
      break;
    }
  }
}

void ValidateSequentially(Isolate* isolate, const ModuleWireBytes& wire_bytes,
                          ModuleEnv* module_env, ErrorThrower* thrower) {
  DCHECK(!thrower->error());

  const WasmModule* module = module_env->module;
  for (uint32_t i = 0; i < module->functions.size(); ++i) {
    const WasmFunction& func = module->functions[i];
    if (func.imported) continue;

    const byte* base = wire_bytes.start();
    FunctionBody body{func.sig, func.code.offset(), base + func.code.offset(),
                      base + func.code.end_offset()};
    DecodeResult result = VerifyWasmCodeWithStats(
        isolate->allocator(), module, body, module->is_wasm(),
        isolate->async_counters().get());
    if (result.failed()) {
      TruncatedUserString<> name(wire_bytes.GetName(&func, module));
      thrower->CompileError("Compiling function #%d:%.*s failed: %s @+%u", i,
                            name.length(), name.start(),
                            result.error_msg().c_str(), result.error_offset());
      break;
    }
  }
}

MaybeHandle<WasmModuleObject> CompileToModuleObjectInternal(
    Isolate* isolate, ErrorThrower* thrower, std::unique_ptr<WasmModule> module,
    const ModuleWireBytes& wire_bytes, Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {
  WasmModule* wasm_module = module.get();
  Handle<Code> centry_stub = CodeFactory::CEntry(isolate);
  TimedHistogramScope wasm_compile_module_time_scope(
      wasm_module->is_wasm()
          ? isolate->async_counters()->wasm_compile_wasm_module_time()
          : isolate->async_counters()->wasm_compile_asm_module_time());
  // TODO(6792): No longer needed once WebAssembly code is off heap. Use
  // base::Optional to be able to close the scope before notifying the debugger.
  base::Optional<CodeSpaceMemoryModificationScope> modification_scope(
      base::in_place_t(), isolate->heap());

  // Check whether lazy compilation is enabled for this module.
  bool lazy_compile = compile_lazy(wasm_module);

  Factory* factory = isolate->factory();
  // Create heap objects for script, module bytes and asm.js offset table to
  // be stored in the shared module data.
  Handle<Script> script;
  Handle<ByteArray> asm_js_offset_table;
  if (asm_js_script.is_null()) {
    script = CreateWasmScript(isolate, wire_bytes);
  } else {
    script = asm_js_script;
    asm_js_offset_table =
        isolate->factory()->NewByteArray(asm_js_offset_table_bytes.length());
    asm_js_offset_table->copy_in(0, asm_js_offset_table_bytes.start(),
                                 asm_js_offset_table_bytes.length());
  }
  // TODO(wasm): only save the sections necessary to deserialize a
  // {WasmModule}. E.g. function bodies could be omitted.
  Handle<String> module_bytes =
      factory
          ->NewStringFromOneByte({wire_bytes.start(), wire_bytes.length()},
                                 TENURED)
          .ToHandleChecked();
  DCHECK(module_bytes->IsSeqOneByteString());

  // The {managed_module} will take ownership of the {WasmModule} object,
  // and it will be destroyed when the GC reclaims the wrapper object.
  Handle<Managed<WasmModule>> managed_module =
      Managed<WasmModule>::FromUniquePtr(isolate, std::move(module));

  // Create the shared module data.
  // TODO(clemensh): For the same module (same bytes / same hash), we should
  // only have one WasmSharedModuleData. Otherwise, we might only set
  // breakpoints on a (potentially empty) subset of the instances.

  Handle<WasmSharedModuleData> shared = WasmSharedModuleData::New(
      isolate, managed_module, Handle<SeqOneByteString>::cast(module_bytes),
      script, asm_js_offset_table);

  int export_wrappers_size =
      static_cast<int>(wasm_module->num_exported_functions);
  Handle<FixedArray> export_wrappers =
      factory->NewFixedArray(static_cast<int>(export_wrappers_size), TENURED);
  Handle<Code> init_builtin = BUILTIN_CODE(isolate, Illegal);
  for (int i = 0, e = export_wrappers->length(); i < e; ++i) {
    export_wrappers->set(i, *init_builtin);
  }
  ModuleEnv env = CreateDefaultModuleEnv(wasm_module);

  // Create the compiled module object and populate with compiled functions
  // and information needed at instantiation time. This object needs to be
  // serializable. Instantiation may occur off a deserialized version of this
  // object.
  Handle<WasmCompiledModule> compiled_module =
      NewCompiledModule(isolate, shared->module(), env);
  NativeModule* native_module = compiled_module->GetNativeModule();
  compiled_module->GetNativeModule()->SetSharedModuleData(shared);
  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate, compiled_module, export_wrappers, shared);
  if (lazy_compile) {
    if (wasm_module->is_wasm()) {
      // Validate wasm modules for lazy compilation. Don't validate asm.js
      // modules, they are valid by construction (otherwise a CHECK will fail
      // during lazy compilation).
      // TODO(clemensh): According to the spec, we can actually skip validation
      // at module creation time, and return a function that always traps at
      // (lazy) compilation time.
      ValidateSequentially(isolate, wire_bytes, &env, thrower);
      if (thrower->error()) return {};
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
      CompileInParallel(isolate, native_module, wire_bytes, &env, module_object,
                        centry_stub, thrower);
    } else {
      CompileSequentially(isolate, native_module, wire_bytes, &env, thrower);
    }
    if (thrower->error()) return {};

    RecordStats(native_module, isolate->async_counters().get());
  }

  // Compile JS->wasm wrappers for exported functions.
  CompileJsToWasmWrappers(isolate, module_object,
                          isolate->async_counters().get());

  // If we created a wasm script, finish it now and make it public to the
  // debugger.
  if (asm_js_script.is_null()) {
    // Close the CodeSpaceMemoryModificationScope before calling into the
    // debugger.
    modification_scope.reset();
    isolate->debug()->OnAfterCompile(script);
  }

  return module_object;
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
      wasm::WasmCode* result = unit->FinishCompilation(&thrower);

      NativeModule* native_module = unit->native_module();
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
        // directly be used by Liftoff-compiled code. Therefore we need
        // to patch the compiled Turbofan function directly after finishing it.
        DCHECK_EQ(CompileMode::kTiering, compilation_state_->compile_mode());
        DCHECK(!result->is_liftoff());
        CodeSpecialization code_specialization;
        code_specialization.RelocateDirectCalls(native_module);
        code_specialization.ApplyToWasmCode(result);

        if (wasm::WasmCode::ShouldBeLogged(isolate)) result->LogCode(isolate);

        // Update the counters to include the top-tier code.
        RecordStats(result,
                    compilation_state_->isolate()->async_counters().get());
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
    // The number of currently running background tasks is reduced either in
    // {StopBackgroundCompilationTaskForThrottling} or in
    // {OnBackgroundTaskStopped}.
    while (!compilation_state_->StopBackgroundCompilationTaskForThrottling()) {
      if (compilation_state_->failed() ||
          !FetchAndExecuteCompilationUnit(compilation_state_)) {
        compilation_state_->OnBackgroundTaskStopped();
        break;
      }
    }
  }

 private:
  CompilationState* compilation_state_;
};
}  // namespace

MaybeHandle<WasmModuleObject> CompileToModuleObject(
    Isolate* isolate, ErrorThrower* thrower, std::unique_ptr<WasmModule> module,
    const ModuleWireBytes& wire_bytes, Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {
  return CompileToModuleObjectInternal(isolate, thrower, std::move(module),
                                       wire_bytes, asm_js_script,
                                       asm_js_offset_table_bytes);
}

InstanceBuilder::InstanceBuilder(Isolate* isolate, ErrorThrower* thrower,
                                 Handle<WasmModuleObject> module_object,
                                 MaybeHandle<JSReceiver> ffi,
                                 MaybeHandle<JSArrayBuffer> memory)
    : isolate_(isolate),
      module_(module_object->shared()->module()),
      async_counters_(isolate->async_counters()),
      thrower_(thrower),
      module_object_(module_object),
      ffi_(ffi),
      memory_(memory) {
  sanitized_imports_.reserve(module_->import_table.size());
}

// Build an instance, in all of its glory.
MaybeHandle<WasmInstanceObject> InstanceBuilder::Build() {
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
  TimedHistogramScope wasm_instantiate_module_time_scope(
      module_->is_wasm() ? counters()->wasm_instantiate_wasm_module_time()
                         : counters()->wasm_instantiate_asm_module_time());
  Factory* factory = isolate_->factory();

  //--------------------------------------------------------------------------
  // Reuse the compiled module (if no owner), otherwise clone.
  //--------------------------------------------------------------------------
  wasm::NativeModule* native_module = nullptr;
  // Root the old instance, if any, in case later allocation causes GC,
  // to prevent the finalizer running for the old instance.
  MaybeHandle<WasmInstanceObject> old_instance;

  TRACE("Starting new module instantiation\n");
  {
    Handle<WasmCompiledModule> original =
        handle(module_object_->compiled_module());
    if (original->has_instance()) {
      old_instance = handle(original->owning_instance());
      // Clone, but don't insert yet the clone in the instances chain.
      // We do that last. Since we are holding on to the old instance,
      // the owner + original state used for cloning and patching
      // won't be mutated by possible finalizer runs.
      TRACE("Cloning from %zu\n", original->GetNativeModule()->instance_id);
      compiled_module_ = WasmCompiledModule::Clone(isolate_, original);
      native_module = compiled_module_->GetNativeModule();
      RecordStats(native_module, counters());
    } else {
      // No instance owned the original compiled module.
      compiled_module_ = original;
      native_module = compiled_module_->GetNativeModule();
      TRACE("Reusing existing instance %zu\n",
            compiled_module_->GetNativeModule()->instance_id);
    }
  }
  DCHECK_NOT_NULL(native_module);
  wasm::NativeModuleModificationScope native_modification_scope(native_module);

  //--------------------------------------------------------------------------
  // Create the WebAssembly.Instance object.
  //--------------------------------------------------------------------------
  Handle<WasmInstanceObject> instance =
      WasmInstanceObject::New(isolate_, module_object_, compiled_module_);
  Handle<WeakCell> weak_instance = factory->NewWeakCell(instance);

  //--------------------------------------------------------------------------
  // Set up the globals for the new instance.
  //--------------------------------------------------------------------------
  MaybeHandle<JSArrayBuffer> old_globals;
  uint32_t globals_size = module_->globals_size;
  if (globals_size > 0) {
    void* backing_store =
        isolate_->array_buffer_allocator()->Allocate(globals_size);
    if (backing_store == nullptr) {
      thrower_->RangeError("Out of memory: wasm globals");
      return {};
    }
    globals_ =
        isolate_->factory()->NewJSArrayBuffer(SharedFlag::kNotShared, TENURED);
    constexpr bool is_external = false;
    constexpr bool is_wasm_memory = false;
    JSArrayBuffer::Setup(globals_, isolate_, is_external, backing_store,
                         globals_size, SharedFlag::kNotShared, is_wasm_memory);
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
    DCHECK(FLAG_experimental_wasm_mut_global);
    // TODO(binji): This allocates one slot for each mutable global, which is
    // more than required if multiple globals are imported from the same
    // module.
    Handle<FixedArray> buffers_array = isolate_->factory()->NewFixedArray(
        module_->num_imported_mutable_globals, TENURED);
    instance->set_imported_mutable_globals_buffers(*buffers_array);
  }

  //--------------------------------------------------------------------------
  // Reserve the metadata for indirect function tables.
  //--------------------------------------------------------------------------
  int function_table_count = static_cast<int>(module_->function_tables.size());
  table_instances_.reserve(module_->function_tables.size());
  for (int index = 0; index < function_table_count; ++index) {
    table_instances_.emplace_back();
  }

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
  if (function_table_count > 0) {
    InitializeTables(instance);
  }

  //--------------------------------------------------------------------------
  // Allocate the memory array buffer.
  //--------------------------------------------------------------------------
  uint32_t initial_pages = module_->initial_pages;
  (module_->is_wasm() ? counters()->wasm_wasm_min_mem_pages_count()
                      : counters()->wasm_asm_min_mem_pages_count())
      ->AddSample(initial_pages);

  if (!memory_.is_null()) {
    // Set externally passed ArrayBuffer non neuterable.
    Handle<JSArrayBuffer> memory = memory_.ToHandleChecked();
    memory->set_is_neuterable(false);

    DCHECK_IMPLIES(use_trap_handler(),
                   module_->is_asm_js() || memory->is_wasm_memory() ||
                       memory->backing_store() == nullptr ||
                       // TODO(836800) Remove once is_wasm_memory transfers over
                       // post-message.
                       (FLAG_experimental_wasm_threads && memory->is_shared()));
  } else if (initial_pages > 0 || use_trap_handler()) {
    // We need to unconditionally create a guard region if using trap handlers,
    // even when the size is zero to prevent null-dereference issues
    // (e.g. https://crbug.com/769637).
    // Allocate memory if the initial size is more than 0 pages.
    memory_ = AllocateMemory(initial_pages);
    if (memory_.is_null()) return {};  // failed to allocate memory
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
      uint32_t mem_size = 0;
      CHECK(memory->byte_length()->ToUint32(&mem_size));
      CHECK_EQ(instance->memory_size(), mem_size);
      CHECK_EQ(instance->memory_start(), memory->backing_store());
    }
  }

  //--------------------------------------------------------------------------
  // Check that indirect function table segments are within bounds.
  //--------------------------------------------------------------------------
  for (WasmTableInit& table_init : module_->table_inits) {
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
  for (WasmDataSegment& seg : module_->data_segments) {
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
  if (function_table_count > 0) {
    LoadTableSegments(instance);
  }

  //--------------------------------------------------------------------------
  // Initialize the memory by loading data segments.
  //--------------------------------------------------------------------------
  if (module_->data_segments.size() > 0) {
    LoadDataSegments(instance);
  }

  //--------------------------------------------------------------------------
  // Patch all code with the relocations registered in code_specialization.
  //--------------------------------------------------------------------------
  CodeSpecialization code_specialization;
  code_specialization.RelocateDirectCalls(native_module);
  code_specialization.ApplyToWholeModule(native_module, module_object_,
                                         SKIP_ICACHE_FLUSH);
  FlushICache(native_module);
  FlushICache(handle(module_object_->export_wrappers()));

  //--------------------------------------------------------------------------
  // Unpack and notify signal handler of protected instructions.
  //--------------------------------------------------------------------------
  if (use_trap_handler()) {
    native_module->UnpackAndRegisterProtectedInstructions();
  }

  //--------------------------------------------------------------------------
  // Insert the compiled module into the weak list of compiled modules.
  //--------------------------------------------------------------------------
  {
    if (!old_instance.is_null()) {
      // Publish the new instance to the instances chain.
      DisallowHeapAllocation no_gc;
      compiled_module_->InsertInChain(*module_object_);
    }
    module_object_->set_compiled_module(*compiled_module_);
    compiled_module_->set_weak_owning_instance(*weak_instance);
    WasmInstanceObject::InstallFinalizer(isolate_, instance);
  }

  //--------------------------------------------------------------------------
  // Debugging support.
  //--------------------------------------------------------------------------
  // Set all breakpoints that were set on the shared module.
  WasmSharedModuleData::SetBreakpointsOnNewInstance(
      handle(module_object_->shared(), isolate_), instance);

  if (FLAG_wasm_interpret_all && module_->is_wasm()) {
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
    Handle<WasmInstanceObject> start_function_instance = instance;
    Address start_call_address =
        static_cast<uint32_t>(start_index) < module_->num_imported_functions
            ? kNullAddress
            : native_module->GetCallTargetForFunction(start_index);
    FunctionSig* sig = module_->functions[start_index].sig;
    Handle<Code> wrapper_code = js_to_wasm_cache_.CloneOrCompileJSToWasmWrapper(
        isolate_, module_, start_call_address, start_index, use_trap_handler());
    // TODO(clemensh): Don't generate an exported function for the start
    // function. Use CWasmEntry instead.
    start_function_ = WasmExportedFunction::New(
        isolate_, start_function_instance, MaybeHandle<String>(), start_index,
        static_cast<int>(sig->parameter_count()), wrapper_code);
  }

  DCHECK(!isolate_->has_pending_exception());
  TRACE("Successfully built instance %zu\n",
        compiled_module_->GetNativeModule()->instance_id);
  TRACE_CHAIN(module_object_->compiled_module());
  return instance;
}

bool InstanceBuilder::ExecuteStartFunction() {
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
  MaybeHandle<Object> result =
      Object::GetPropertyOrElement(ffi_.ToHandleChecked(), module_name);
  if (result.is_null()) {
    return ReportTypeError("module not found", index, module_name);
  }

  Handle<Object> module = result.ToHandleChecked();

  // Look up the value in the module.
  if (!module->IsJSReceiver()) {
    return ReportTypeError("module is not an object or function", index,
                           module_name);
  }

  result = Object::GetPropertyOrElement(module, import_name);
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
      return *reinterpret_cast<uint32_t*>(raw_buffer_ptr(globals_, offset));
    }
    default:
      UNREACHABLE();
  }
}

// Load data segments into the memory.
void InstanceBuilder::LoadDataSegments(Handle<WasmInstanceObject> instance) {
  Handle<SeqOneByteString> module_bytes(
      module_object_->shared()->module_bytes(), isolate_);
  for (const WasmDataSegment& segment : module_->data_segments) {
    uint32_t source_size = segment.source.length();
    // Segments of size == 0 are just nops.
    if (source_size == 0) continue;
    uint32_t dest_offset = EvalUint32InitExpr(segment.dest_addr);
    DCHECK(in_bounds(dest_offset, source_size, instance->memory_size()));
    byte* dest = instance->memory_start() + dest_offset;
    const byte* src = reinterpret_cast<const byte*>(
        module_bytes->GetCharsAddress() + segment.source.offset());
    memcpy(dest, src, source_size);
  }
}

void InstanceBuilder::WriteGlobalValue(WasmGlobal& global, double num) {
  TRACE("init [globals_start=%p + %u] = %lf, type = %s\n",
        reinterpret_cast<void*>(raw_buffer_ptr(globals_, 0)), global.offset,
        num, ValueTypes::TypeName(global.type));
  switch (global.type) {
    case kWasmI32:
      *GetRawGlobalPtr<int32_t>(global) = static_cast<int32_t>(num);
      break;
    case kWasmI64:
      // TODO(titzer): initialization of imported i64 globals.
      UNREACHABLE();
      break;
    case kWasmF32:
      *GetRawGlobalPtr<float>(global) = static_cast<float>(num);
      break;
    case kWasmF64:
      *GetRawGlobalPtr<double>(global) = static_cast<double>(num);
      break;
    default:
      UNREACHABLE();
  }
}

void InstanceBuilder::WriteGlobalValue(WasmGlobal& global,
                                       Handle<WasmGlobalObject> value) {
  TRACE("init [globals_start=%p + %u] = ",
        reinterpret_cast<void*>(raw_buffer_ptr(globals_, 0)), global.offset);
  switch (global.type) {
    case kWasmI32: {
      int32_t num = value->GetI32();
      *GetRawGlobalPtr<int32_t>(global) = num;
      TRACE("%d", num);
      break;
    }
    case kWasmI64: {
      int64_t num = value->GetI64();
      *GetRawGlobalPtr<int64_t>(global) = num;
      TRACE("%" PRId64, num);
      break;
    }
    case kWasmF32: {
      float num = value->GetF32();
      *GetRawGlobalPtr<float>(global) = num;
      TRACE("%f", num);
      break;
    }
    case kWasmF64: {
      double num = value->GetF64();
      *GetRawGlobalPtr<double>(global) = num;
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
  Handle<SeqOneByteString> module_bytes(
      module_object_->shared()->module_bytes());
  for (size_t index = 0; index < module_->import_table.size(); ++index) {
    WasmImport& import = module_->import_table[index];

    Handle<String> module_name;
    MaybeHandle<String> maybe_module_name =
        WasmSharedModuleData::ExtractUtf8StringFromModuleBytes(
            isolate_, module_bytes, import.module_name);
    if (!maybe_module_name.ToHandle(&module_name)) {
      thrower_->LinkError("Could not resolve module name for import %zu",
                          index);
      return;
    }

    Handle<String> import_name;
    MaybeHandle<String> maybe_import_name =
        WasmSharedModuleData::ExtractUtf8StringFromModuleBytes(
            isolate_, module_bytes, import.field_name);
    if (!maybe_import_name.ToHandle(&import_name)) {
      thrower_->LinkError("Could not resolve import name for import %zu",
                          index);
      return;
    }

    int int_index = static_cast<int>(index);
    MaybeHandle<Object> result =
        module_->is_asm_js()
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

// Process the imports, including functions, tables, globals, and memory, in
// order, loading them from the {ffi_} object. Returns the number of imported
// functions.
int InstanceBuilder::ProcessImports(Handle<WasmInstanceObject> instance) {
  int num_imported_functions = 0;
  int num_imported_tables = 0;
  int num_imported_mutable_globals = 0;

  DCHECK_EQ(module_->import_table.size(), sanitized_imports_.size());
  for (int index = 0; index < static_cast<int>(module_->import_table.size());
       ++index) {
    WasmImport& import = module_->import_table[index];

    Handle<String> module_name = sanitized_imports_[index].module_name;
    Handle<String> import_name = sanitized_imports_[index].import_name;
    Handle<Object> value = sanitized_imports_[index].value;
    NativeModule* native_module =
        instance->compiled_module()->GetNativeModule();

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
        FunctionSig* expected_sig = module_->functions[func_index].sig;
        if (WasmExportedFunction::IsWasmExportedFunction(*value)) {
          // The imported function is a WASM function from another instance.
          Handle<WasmExportedFunction> imported_function(
              WasmExportedFunction::cast(*value), isolate_);
          Handle<WasmInstanceObject> imported_instance(
              imported_function->instance(), isolate_);
          FunctionSig* imported_sig =
              imported_instance->module()
                  ->functions[imported_function->function_index()]
                  .sig;
          if (!imported_sig->Equals(expected_sig)) {
            ReportLinkError(
                "imported function does not match the expected type", index,
                module_name, import_name);
            return -1;
          }
          // The import reference is the instance object itself.
          ImportedFunctionEntry entry(instance, func_index);
          Address imported_target = imported_function->GetWasmCallTarget();
          entry.set_wasm_to_wasm(*imported_instance, imported_target);
        } else {
          // The imported function is a callable.
          Handle<JSReceiver> js_receiver(JSReceiver::cast(*value), isolate_);
          Handle<Code> wrapper_code = compiler::CompileWasmToJSWrapper(
              isolate_, js_receiver, expected_sig, func_index,
              module_->origin(), use_trap_handler());
          RecordStats(*wrapper_code, counters());

          WasmCode* wasm_code = native_module->AddCodeCopy(
              wrapper_code, wasm::WasmCode::kWasmToJsWrapper, func_index);
          ImportedFunctionEntry entry(instance, func_index);
          entry.set_wasm_to_js(*js_receiver, wasm_code);
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
        WasmIndirectFunctionTable& table = module_->function_tables[table_num];
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
              handle(target->instance());
          Address exported_call_target = target->GetWasmCallTarget();
          FunctionSig* sig = imported_instance->module()
                                 ->functions[target->function_index()]
                                 .sig;
          IndirectFunctionTableEntry(instance, i)
              .set(module_->signature_map.Find(sig), *imported_instance,
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
        memory_ = buffer;
        uint32_t imported_cur_pages = static_cast<uint32_t>(
            buffer->byte_length()->Number() / kWasmPageSize);
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
        WasmGlobal& global = module_->globals[import.index];

        // The mutable-global proposal allows importing i64 values, but only if
        // they are passed as a WebAssembly.Global object.
        if (global.type == kWasmI64 && !(FLAG_experimental_wasm_mut_global &&
                                         value->IsWasmGlobalObject())) {
          ReportLinkError("global import cannot have type i64", index,
                          module_name, import_name);
          return -1;
        }
        if (module_->is_asm_js()) {
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
              value = Object::ToNumber(value).ToHandleChecked();
            }
          }
        }
        if (FLAG_experimental_wasm_mut_global) {
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
T* InstanceBuilder::GetRawGlobalPtr(WasmGlobal& global) {
  return reinterpret_cast<T*>(raw_buffer_ptr(globals_, global.offset));
}

// Process initialization of globals.
void InstanceBuilder::InitGlobals() {
  for (auto global : module_->globals) {
    if (global.mutability && global.imported) {
      DCHECK(FLAG_experimental_wasm_mut_global);
      continue;
    }

    switch (global.init.kind) {
      case WasmInitExpr::kI32Const:
        *GetRawGlobalPtr<int32_t>(global) = global.init.val.i32_const;
        break;
      case WasmInitExpr::kI64Const:
        *GetRawGlobalPtr<int64_t>(global) = global.init.val.i64_const;
        break;
      case WasmInitExpr::kF32Const:
        *GetRawGlobalPtr<float>(global) = global.init.val.f32_const;
        break;
      case WasmInitExpr::kF64Const:
        *GetRawGlobalPtr<double>(global) = global.init.val.f64_const;
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
  const bool is_shared_memory =
      module_->has_shared_memory && i::FLAG_experimental_wasm_threads;
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
  for (auto& table : module_->function_tables) {
    if (table.exported) return true;
  }
  return false;
}

// Process the exports, creating wrappers for functions, tables, memories,
// and globals.
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
      WasmImport& import = module_->import_table[index];
      if (import.kind == kExternalFunction) {
        Handle<Object> value = sanitized_imports_[index].value;
        if (WasmExportedFunction::IsWasmExportedFunction(*value)) {
          js_wrappers_[import.index] = Handle<JSFunction>::cast(value);
        }
      }
    }
  }

  Handle<JSObject> exports_object;
  if (module_->is_wasm()) {
    // Create the "exports" object.
    exports_object = isolate_->factory()->NewJSObjectWithNullProto();
  } else if (module_->is_asm_js()) {
    Handle<JSFunction> object_function = Handle<JSFunction>(
        isolate_->native_context()->object_function(), isolate_);
    exports_object = isolate_->factory()->NewJSObject(object_function);
  } else {
    UNREACHABLE();
  }
  instance->set_exports_object(*exports_object);

  Handle<String> single_function_name =
      isolate_->factory()->InternalizeUtf8String(AsmJs::kSingleFunctionName);

  PropertyDescriptor desc;
  desc.set_writable(module_->is_asm_js());
  desc.set_enumerable(true);
  desc.set_configurable(module_->is_asm_js());

  // Process each export in the export table.
  int export_index = 0;  // Index into {export_wrappers}.
  for (WasmExport& exp : module_->export_table) {
    Handle<String> name =
        WasmSharedModuleData::ExtractUtf8StringFromModuleBytes(
            isolate_, handle(module_object_->shared(), isolate_), exp.name)
            .ToHandleChecked();
    Handle<JSObject> export_to;
    if (module_->is_asm_js() && exp.kind == kExternalFunction &&
        String::Equals(name, single_function_name)) {
      export_to = instance;
    } else {
      export_to = exports_object;
    }

    switch (exp.kind) {
      case kExternalFunction: {
        // Wrap and export the code as a JSFunction.
        WasmFunction& function = module_->functions[exp.index];
        Handle<JSFunction> js_function = js_wrappers_[exp.index];
        if (js_function.is_null()) {
          // Wrap the exported code as a JSFunction.
          Handle<Code> export_code =
              export_wrappers->GetValueChecked<Code>(isolate_, export_index);
          MaybeHandle<String> func_name;
          if (module_->is_asm_js()) {
            // For modules arising from asm.js, honor the names section.
            WireBytesRef func_name_ref = module_->LookupName(
                module_object_->shared()->module_bytes(), function.func_index);
            func_name =
                WasmSharedModuleData::ExtractUtf8StringFromModuleBytes(
                    isolate_, handle(module_object_->shared(), isolate_),
                    func_name_ref)
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
        WasmIndirectFunctionTable& table = module_->function_tables[exp.index];
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
        WasmGlobal& global = module_->globals[exp.index];
        if (FLAG_experimental_wasm_mut_global) {
          Handle<JSArrayBuffer> globals_buffer(instance->globals_buffer(),
                                               isolate_);
          // Since the global's array buffer is always provided, allocation
          // should never fail.
          Handle<WasmGlobalObject> global_obj =
              WasmGlobalObject::New(isolate_, globals_buffer, global.type,
                                    global.offset, global.mutability)
                  .ToHandleChecked();
          desc.set_value(global_obj);
        } else {
          // Export the value of the global variable as a number.
          double num = 0;
          switch (global.type) {
            case kWasmI32:
              num = *GetRawGlobalPtr<int32_t>(global);
              break;
            case kWasmF32:
              num = *GetRawGlobalPtr<float>(global);
              break;
            case kWasmF64:
              num = *GetRawGlobalPtr<double>(global);
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

  if (module_->is_wasm()) {
    v8::Maybe<bool> success =
        JSReceiver::SetIntegrityLevel(exports_object, FROZEN, kDontThrow);
    DCHECK(success.FromMaybe(false));
    USE(success);
  }
}

void InstanceBuilder::InitializeTables(Handle<WasmInstanceObject> instance) {
  size_t table_count = module_->function_tables.size();
  for (size_t index = 0; index < table_count; ++index) {
    WasmIndirectFunctionTable& table = module_->function_tables[index];
    TableInstance& table_instance = table_instances_[index];

    if (!instance->has_indirect_function_table()) {
      WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
          instance, table.initial_size);
      table_instance.table_size = table.initial_size;
    }
  }
}

void InstanceBuilder::LoadTableSegments(Handle<WasmInstanceObject> instance) {
  NativeModule* native_module = compiled_module_->GetNativeModule();
  int function_table_count = static_cast<int>(module_->function_tables.size());
  for (int index = 0; index < function_table_count; ++index) {
    TableInstance& table_instance = table_instances_[index];

    // TODO(titzer): this does redundant work if there are multiple tables,
    // since initializations are not sorted by table index.
    for (auto& table_init : module_->table_inits) {
      uint32_t base = EvalUint32InitExpr(table_init.offset);
      uint32_t num_entries = static_cast<uint32_t>(table_init.entries.size());
      DCHECK(in_bounds(base, num_entries, table_instance.table_size));
      for (uint32_t i = 0; i < num_entries; ++i) {
        uint32_t func_index = table_init.entries[i];
        WasmFunction* function = &module_->functions[func_index];
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
                js_to_wasm_cache_.CloneOrCompileJSToWasmWrapper(
                    isolate_, module_, is_import ? kNullAddress : call_target,
                    func_index, use_trap_handler());
            MaybeHandle<String> func_name;
            if (module_->is_asm_js()) {
              // For modules arising from asm.js, honor the names section.
              WireBytesRef func_name_ref = module_->LookupName(
                  module_object_->shared()->module_bytes(), func_index);
              func_name =
                  WasmSharedModuleData::ExtractUtf8StringFromModuleBytes(
                      isolate_, handle(module_object_->shared(), isolate_),
                      func_name_ref)
                      .ToHandleChecked();
            }
            Handle<WasmExportedFunction> js_function =
                WasmExportedFunction::New(
                    isolate_, instance, func_name, func_index,
                    static_cast<int>(function->sig->parameter_count()),
                    wrapper_code);
            js_wrappers_[func_index] = js_function;
          }
          table_instance.js_wrappers->set(table_index,
                                          *js_wrappers_[func_index]);
          // UpdateDispatchTables() updates all other dispatch tables, since
          // we have not yet added the dispatch table we are currently building.
          WasmTableObject::UpdateDispatchTables(
              isolate_, table_instance.table_object, table_index, function->sig,
              target_instance, call_target);
        }
      }
    }

    // Add the new dispatch table at the end to avoid redundant lookups.
    if (!table_instance.table_object.is_null()) {
      // Add the new dispatch table to the WebAssembly.Table object.
      WasmTableObject::AddDispatchTable(isolate_, table_instance.table_object,
                                        instance, index);
    }
  }
}

AsyncCompileJob::AsyncCompileJob(Isolate* isolate,
                                 std::unique_ptr<byte[]> bytes_copy,
                                 size_t length, Handle<Context> context,
                                 Handle<JSPromise> promise)
    : isolate_(isolate),
      async_counters_(isolate->async_counters()),
      bytes_copy_(std::move(bytes_copy)),
      wire_bytes_(bytes_copy_.get(), bytes_copy_.get() + length) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::Platform* platform = V8::GetCurrentPlatform();
  foreground_task_runner_ = platform->GetForegroundTaskRunner(v8_isolate);
  background_task_runner_ = platform->GetWorkerThreadsTaskRunner(v8_isolate);
  // The handles for the context and promise must be deferred.
  DeferredHandleScope deferred(isolate);
  context_ = Handle<Context>(*context);
  module_promise_ = Handle<JSPromise>(*promise);
  deferred_handles_.push_back(deferred.Detach());
}

void AsyncCompileJob::Start() {
  DoAsync<DecodeModule>();  // --
}

void AsyncCompileJob::Abort() {
  background_task_manager_.CancelAndWait();
  if (!compiled_module_.is_null()) {
    compiled_module_->GetNativeModule()->compilation_state()->Abort();
  }
  if (num_pending_foreground_tasks_ == 0) {
    // No task is pending, we can just remove the AsyncCompileJob.
    isolate_->wasm_engine()->RemoveCompileJob(this);
  } else {
    // There is still a compilation task in the task queue. We enter the
    // AbortCompilation state and wait for this compilation task to abort the
    // AsyncCompileJob.
    NextStep<AbortCompilation>();
  }
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

  void OnFinishedStream(std::unique_ptr<uint8_t[]> bytes,
                        size_t length) override;

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
  for (auto d : deferred_handles_) delete d;
}

void AsyncCompileJob::FinishCompile() {
  RecordStats(compiled_module_->GetNativeModule(), counters());

  // Create heap objects for script and module bytes to be stored in the
  // shared module data. Asm.js is not compiled asynchronously.
  Handle<Script> script = CreateWasmScript(isolate_, wire_bytes_);
  Handle<ByteArray> asm_js_offset_table;
  // TODO(wasm): Improve efficiency of storing module wire bytes.
  //   1. Only store relevant sections, not function bodies
  //   2. Don't make a second copy of the bytes here; reuse the copy made
  //      for asynchronous compilation and store it as an external one
  //      byte string for serialization/deserialization.
  Handle<String> module_bytes =
      isolate_->factory()
          ->NewStringFromOneByte({wire_bytes_.start(), wire_bytes_.length()},
                                 TENURED)
          .ToHandleChecked();
  DCHECK(module_bytes->IsSeqOneByteString());
  int export_wrapper_size = static_cast<int>(module_->num_exported_functions);
  Handle<FixedArray> export_wrappers =
      isolate_->factory()->NewFixedArray(export_wrapper_size, TENURED);

  // The {managed_module} will take ownership of the {WasmModule} object,
  // and it will be destroyed when the GC reclaims the wrapper object.
  Handle<Managed<WasmModule>> managed_module =
      Managed<WasmModule>::FromUniquePtr(isolate_, std::move(module_));

  // Create the shared module data.
  // TODO(clemensh): For the same module (same bytes / same hash), we should
  // only have one WasmSharedModuleData. Otherwise, we might only set
  // breakpoints on a (potentially empty) subset of the instances.
  Handle<WasmSharedModuleData> shared = WasmSharedModuleData::New(
      isolate_, managed_module, Handle<SeqOneByteString>::cast(module_bytes),
      script, asm_js_offset_table);
  compiled_module_->GetNativeModule()->SetSharedModuleData(shared);

  // Create the module object.
  module_object_ = WasmModuleObject::New(isolate_, compiled_module_,
                                         export_wrappers, shared);
  {
    DeferredHandleScope deferred(isolate_);
    module_object_ = handle(*module_object_, isolate_);
    deferred_handles_.push_back(deferred.Detach());
  }

  // Finish the wasm script now and make it public to the debugger.
  isolate_->debug()->OnAfterCompile(script);

  // TODO(wasm): compiling wrappers should be made async as well.
  DoSync<CompileWrappers>();
}

void AsyncCompileJob::AsyncCompileFailed(Handle<Object> error_reason) {
  if (stream_) stream_->NotifyError();
  // {job} keeps the {this} pointer alive.
  std::shared_ptr<AsyncCompileJob> job =
      isolate_->wasm_engine()->RemoveCompileJob(this);
  MaybeHandle<Object> promise_result =
      JSPromise::Reject(module_promise_, error_reason);
  CHECK_EQ(promise_result.is_null(), isolate_->has_pending_exception());
}

void AsyncCompileJob::AsyncCompileSucceeded(Handle<Object> result) {
  MaybeHandle<Object> promise_result =
      JSPromise::Resolve(module_promise_, result);
  CHECK_EQ(promise_result.is_null(), isolate_->has_pending_exception());
}

// A closure to run a compilation step (either as foreground or background
// task) and schedule the next step(s), if any.
class AsyncCompileJob::CompileStep {
 public:
  explicit CompileStep(int num_background_tasks = 0)
      : num_background_tasks_(num_background_tasks) {}

  virtual ~CompileStep() {}

  void Run(bool on_foreground) {
    if (on_foreground) {
      HandleScope scope(job_->isolate_);
      --job_->num_pending_foreground_tasks_;
      DCHECK_EQ(0, job_->num_pending_foreground_tasks_);
      SaveContext saved_context(job_->isolate_);
      job_->isolate_->set_context(*job_->context_);
      RunInForeground();
    } else {
      RunInBackground();
    }
  }

  virtual void RunInForeground() { UNREACHABLE(); }
  virtual void RunInBackground() { UNREACHABLE(); }

  int NumberOfBackgroundTasks() { return num_background_tasks_; }

  AsyncCompileJob* job_ = nullptr;
  const int num_background_tasks_;
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

  void RunInternal() override { job_->step_->Run(on_foreground_); }

 private:
  AsyncCompileJob* job_;
  bool on_foreground_;
};

void AsyncCompileJob::StartForegroundTask() {
  ++num_pending_foreground_tasks_;
  DCHECK_EQ(1, num_pending_foreground_tasks_);

  foreground_task_runner_->PostTask(base::make_unique<CompileTask>(this, true));
}

template <typename Step, typename... Args>
void AsyncCompileJob::DoSync(Args&&... args) {
  NextStep<Step>(std::forward<Args>(args)...);
  StartForegroundTask();
}

void AsyncCompileJob::StartBackgroundTask() {
  // If --wasm-num-compilation-tasks=0 is passed, do only spawn foreground
  // tasks. This is used to make timing deterministic.
  v8::TaskRunner* task_runner = FLAG_wasm_num_compilation_tasks > 0
                                    ? background_task_runner_.get()
                                    : foreground_task_runner_.get();
  task_runner->PostTask(base::make_unique<CompileTask>(this, false));
}

template <typename Step, typename... Args>
void AsyncCompileJob::DoAsync(Args&&... args) {
  NextStep<Step>(std::forward<Args>(args)...);
  int end = step_->NumberOfBackgroundTasks();
  for (int i = 0; i < end; ++i) {
    StartBackgroundTask();
  }
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
  DecodeModule() : CompileStep(1) {}

  void RunInBackground() override {
    ModuleResult result;
    {
      DisallowHandleAllocation no_handle;
      DisallowHeapAllocation no_allocation;
      // Decode the module bytes.
      TRACE_COMPILE("(1) Decoding module...\n");
      result = AsyncDecodeWasmModule(job_->isolate_, job_->wire_bytes_.start(),
                                     job_->wire_bytes_.end(), false,
                                     kWasmOrigin, job_->async_counters());
    }
    if (result.failed()) {
      // Decoding failure; reject the promise and clean up.
      job_->DoSync<DecodeFail>(std::move(result));
    } else {
      // Decode passed.
      job_->module_ = std::move(result.val);
      job_->DoSync<PrepareAndStartCompile>(job_->module_.get(), true);
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
  explicit PrepareAndStartCompile(WasmModule* module, bool start_compilation)
      : module_(module), start_compilation_(start_compilation) {}

 private:
  WasmModule* module_;
  bool start_compilation_;

  void RunInForeground() override {
    TRACE_COMPILE("(2) Prepare and start compile...\n");

    // Make sure all compilation tasks stopped running. Decoding (async step)
    // is done.
    job_->background_task_manager_.CancelAndWait();

    Isolate* isolate = job_->isolate_;

    job_->centry_stub_ = CodeFactory::CEntry(isolate);

    DCHECK_LE(module_->num_imported_functions, module_->functions.size());
    // Create the compiled module object and populate with compiled functions
    // and information needed at instantiation time. This object needs to be
    // serializable. Instantiation may occur off a deserialized version of
    // this object.
    ModuleEnv env = CreateDefaultModuleEnv(module_);
    job_->compiled_module_ = NewCompiledModule(job_->isolate_, module_, env);

    {
      DeferredHandleScope deferred(job_->isolate_);
      job_->compiled_module_ = handle(*job_->compiled_module_, job_->isolate_);
      job_->deferred_handles_.push_back(deferred.Detach());
    }
    size_t num_functions =
        module_->functions.size() - module_->num_imported_functions;

    if (num_functions == 0) {
      // Tiering has nothing to do if module is empty.
      job_->tiering_completed_ = true;

      // Degenerate case of an empty module.
      job_->FinishCompile();
      return;
    }

    CompilationState* compilation_state =
        job_->compiled_module_->GetNativeModule()->compilation_state();
    {
      // Instance field {job_} cannot be captured by copy, therefore
      // we need to add a local helper variable {job}. We want to
      // capture the {job} pointer by copy, as it otherwise is dependent
      // on the current step we are in.
      AsyncCompileJob* job = job_;
      compilation_state->AddCallback(
          [job](CompilationEvent event, ErrorThrower* thrower) {
            // Callback is called from a foreground thread.
            switch (event) {
              case CompilationEvent::kFinishedBaselineCompilation:
                if (job->DecrementAndCheckFinisherCount()) {
                  SaveContext saved_context(job->isolate());
                  // TODO(mstarzinger): Make {AsyncCompileJob::context} point
                  // to the native context and also rename to {native_context}.
                  job->isolate()->set_context(job->context_->native_context());
                  job->FinishCompile();
                }
                return;
              case CompilationEvent::kFinishedTopTierCompilation:
                // It is only safe to schedule the UpdateToTopTierCompiledCode
                // step if no foreground task is currently pending, and no
                // finisher is outstanding (streaming compilation).
                if (job->num_pending_foreground_tasks_ == 0 &&
                    job->outstanding_finishers_.Value() == 0) {
                  job->DoSync<UpdateToTopTierCompiledCode>();
                }
                // If a foreground task was pending or a finsher was pending,
                // we will rely on FinishModule to switch the step to
                // UpdateToTopTierCompiledCode.
                job->tiering_completed_ = true;
                return;
              case CompilationEvent::kFailedCompilation: {
                // Tier-up compilation should not fail if baseline compilation
                // did not fail.
                DCHECK(!job->compiled_module_->GetNativeModule()
                            ->compilation_state()
                            ->baseline_compilation_finished());

                SaveContext saved_context(job->isolate());
                job->isolate()->set_context(job->context_->native_context());
                Handle<Object> error = thrower->Reify();

                DeferredHandleScope deferred(job->isolate());
                error = handle(*error, job->isolate());
                job->deferred_handles_.push_back(deferred.Detach());
                job->DoSync<CompileFailed>(error);
                return;
              }
              case CompilationEvent::kDestroyed:
                // Nothing to do.
                return;
            }
            UNREACHABLE();
          });
    }
    if (start_compilation_) {
      // TODO(ahaas): Try to remove the {start_compilation_} check when
      // streaming decoding is done in the background. If
      // InitializeCompilationUnits always returns 0 for streaming compilation,
      // then DoAsync would do the same as NextStep already.

      size_t functions_count = GetNumFunctionsToCompile(env.module);
      compilation_state->SetNumberOfFunctionsToCompile(functions_count);
      // Add compilation units and kick off compilation.
      InitializeCompilationUnits(module_->functions, job_->wire_bytes_,
                                 env.module, job_->centry_stub_,
                                 job_->compiled_module_->GetNativeModule());
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
    CompileJsToWasmWrappers(job_->isolate_, job_->module_object_,
                            job_->counters());
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

    WasmModule* module = job_->module_object_->shared()->module();
    size_t num_functions =
        module->functions.size() - module->num_imported_functions;
    if (job_->compiled_module_->GetNativeModule()
                ->compilation_state()
                ->compile_mode() == CompileMode::kRegular ||
        num_functions == 0) {
      // If we do not tier up, the async compile job is done here and
      // can be deleted.
      job_->isolate_->wasm_engine()->RemoveCompileJob(job_);
      return;
    }
    // If background tiering compilation finished before we resolved the
    // promise, switch to patching now. Otherwise, patching will be scheduled
    // by a callback.
    DCHECK_EQ(CompileMode::kTiering, job_->compiled_module_->GetNativeModule()
                                         ->compilation_state()
                                         ->compile_mode());
    if (job_->tiering_completed_) {
      job_->DoSync<UpdateToTopTierCompiledCode>();
    }
  }
};

//==========================================================================
// Step 7 (sync): Update with top tier code.
//==========================================================================
class AsyncCompileJob::UpdateToTopTierCompiledCode : public CompileStep {
  void RunInForeground() override {
    TRACE_COMPILE("(7) Update native module to use optimized code...\n");

    UpdateAllCompiledModulesWithTopTierCode(job_->module_object_);
    job_->isolate_->wasm_engine()->RemoveCompileJob(job_);
  }
};

class AsyncCompileJob::AbortCompilation : public CompileStep {
  void RunInForeground() override {
    TRACE_COMPILE("Abort asynchronous compilation ...\n");
    job_->isolate_->wasm_engine()->RemoveCompileJob(job_);
  }
};

AsyncStreamingProcessor::AsyncStreamingProcessor(AsyncCompileJob* job)
    : job_(job), compilation_unit_builder_(nullptr) {}

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
  if (!job_->compiled_module_.is_null()) {
    job_->compiled_module_->GetNativeModule()->compilation_state()->Abort();

    if (job_->num_pending_foreground_tasks_ == 0) {
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
  decoder_.StartDecoding(job_->isolate());
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
  job_->NextStep<AsyncCompileJob::PrepareAndStartCompile>(decoder_.module(),
                                                          false);
  // Execute the PrepareAndStartCompile step immediately and not in a separate
  // task. The step expects to be run on a separate foreground thread though, so
  // we to increment {num_pending_foreground_tasks_} to look like one.
  ++job_->num_pending_foreground_tasks_;
  DCHECK_EQ(1, job_->num_pending_foreground_tasks_);
  constexpr bool on_foreground = true;
  job_->step_->Run(on_foreground);

  NativeModule* native_module = job_->compiled_module_->GetNativeModule();
  native_module->compilation_state()->SetNumberOfFunctionsToCompile(
      functions_count);

  // Set outstanding_finishers_ to 2, because both the AsyncCompileJob and the
  // AsyncStreamingProcessor have to finish.
  job_->outstanding_finishers_.SetValue(2);
  compilation_unit_builder_.reset(
      new CompilationUnitBuilder(native_module, job_->centry_stub_));
  return true;
}

// Process a function body.
bool AsyncStreamingProcessor::ProcessFunctionBody(Vector<const uint8_t> bytes,
                                                  uint32_t offset) {
  TRACE_STREAMING("Process function body %d ...\n", next_function_);

  if (next_function_ >= FLAG_skip_compiling_wasm_funcs) {
    decoder_.DecodeFunctionBody(
        next_function_, static_cast<uint32_t>(bytes.length()), offset, false);

    uint32_t index = next_function_ + decoder_.module()->num_imported_functions;
    const WasmFunction* func = &decoder_.module()->functions[index];
    WasmName name = {nullptr, 0};
    compilation_unit_builder_->AddUnit(func, offset, bytes, name);
  }
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
void AsyncStreamingProcessor::OnFinishedStream(std::unique_ptr<uint8_t[]> bytes,
                                               size_t length) {
  TRACE_STREAMING("Finish stream...\n");
  job_->bytes_copy_ = std::move(bytes);
  job_->wire_bytes_ = ModuleWireBytes(job_->bytes_copy_.get(),
                                      job_->bytes_copy_.get() + length);
  ModuleResult result = decoder_.FinishDecoding(false);
  DCHECK(result.ok());
  job_->module_ = std::move(result.val);
  if (job_->DecrementAndCheckFinisherCount()) {
    if (job_->compiled_module_.is_null()) {
      // We are processing a WebAssembly module without code section. We need to
      // prepare compilation first before we can finish it.
      // {PrepareAndStartCompile} will call {FinishCompile} by itself if there
      // is no code section.
      job_->DoSync<AsyncCompileJob::PrepareAndStartCompile>(job_->module_.get(),
                                                            true);
    } else {
      job_->FinishCompile();
    }
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
    Isolate* isolate, ModuleEnv& env) {
  return std::unique_ptr<CompilationState, CompilationStateDeleter>(
      new CompilationState(isolate, env));
}

ModuleEnv* GetModuleEnv(CompilationState* compilation_state) {
  return compilation_state->module_env();
}

CompilationState::CompilationState(internal::Isolate* isolate, ModuleEnv& env)
    : isolate_(isolate),
      module_env_(env),
      max_memory_(GetMaxUsableMemorySize(isolate) / 2),
      // TODO(clemensh): Fix fuzzers such that {env.module} is always non-null.
      compile_mode_(FLAG_wasm_tier_up && (!env.module || env.module->is_wasm())
                        ? CompileMode::kTiering
                        : CompileMode::kRegular),
      wire_bytes_(ModuleWireBytes(nullptr, nullptr)),
      max_background_tasks_(std::max(
          1, std::min(FLAG_wasm_num_compilation_tasks,
                      V8::GetCurrentPlatform()->NumberOfWorkerThreads()))) {
  DCHECK_LT(0, max_memory_);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  v8::Platform* platform = V8::GetCurrentPlatform();
  foreground_task_runner_ = platform->GetForegroundTaskRunner(v8_isolate);
  background_task_runner_ = platform->GetWorkerThreadsTaskRunner(v8_isolate);

  // Register task manager for clean shutdown in case of an isolate shutdown.
  isolate_->wasm_engine()->Register(&background_task_manager_);
  isolate_->wasm_engine()->Register(&foreground_task_manager_);
}

CompilationState::~CompilationState() {
  CancelAndWait();
  foreground_task_manager_.CancelAndWait();
  isolate_->wasm_engine()->Unregister(&foreground_task_manager_);
  NotifyOnEvent(CompilationEvent::kDestroyed, nullptr);
}

void CompilationState::SetNumberOfFunctionsToCompile(size_t num_functions) {
  DCHECK(!failed());
  outstanding_units_ = num_functions;

  if (compile_mode_ == CompileMode::kTiering) {
    outstanding_units_ += num_functions;
    num_tiering_units_ = num_functions;
  }
}

void CompilationState::AddCallback(
    std::function<void(CompilationEvent, ErrorThrower*)> callback) {
  callbacks_.push_back(callback);
}

void CompilationState::AddCompilationUnits(
    std::vector<std::unique_ptr<WasmCompilationUnit>>& baseline_units,
    std::vector<std::unique_ptr<WasmCompilationUnit>>& tiering_units) {
  {
    base::LockGuard<base::Mutex> guard(&mutex_);

    if (compile_mode_ == CompileMode::kTiering) {
      DCHECK_EQ(baseline_units.size(), tiering_units.size());
      DCHECK_EQ(tiering_units.back()->mode(),
                WasmCompilationUnit::CompilationMode::kTurbofan);
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
  allocated_memory_ -= ret->memory_cost();
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
    CancelAndWait();
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
    std::unique_ptr<WasmCompilationUnit> unit,
    WasmCompilationUnit::CompilationMode mode) {
  size_t cost = unit->memory_cost();
  base::LockGuard<base::Mutex> guard(&mutex_);
  if (compile_mode_ == CompileMode::kTiering &&
      mode == WasmCompilationUnit::CompilationMode::kTurbofan) {
    tiering_finish_units_.push_back(std::move(unit));
  } else {
    baseline_finish_units_.push_back(std::move(unit));
  }
  allocated_memory_ += cost;

  if (!finisher_is_running_ && !failed_) {
    ScheduleFinisherTask();
    // We set the flag here so that not more than one finisher is started.
    finisher_is_running_ = true;
  }
}

void CompilationState::CancelAndWait() {
  background_task_manager_.CancelAndWait();
  isolate_->wasm_engine()->Unregister(&background_task_manager_);
}

void CompilationState::OnBackgroundTaskStopped() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  DCHECK_LE(1, num_background_tasks_);
  --num_background_tasks_;
}

void CompilationState::RestartBackgroundTasks(size_t max) {
  size_t num_restart;
  {
    base::LockGuard<base::Mutex> guard(&mutex_);
    // No need to restart tasks if compilation already failed.
    if (failed_) return;

    bool should_increase_workload = allocated_memory_ <= max_memory_ / 2;
    if (!should_increase_workload) return;
    DCHECK_LE(num_background_tasks_, max_background_tasks_);
    if (num_background_tasks_ == max_background_tasks_) return;
    size_t num_compilation_units =
        baseline_compilation_units_.size() + tiering_compilation_units_.size();
    size_t stopped_tasks = max_background_tasks_ - num_background_tasks_;
    num_restart = std::min(max, std::min(num_compilation_units, stopped_tasks));
    num_background_tasks_ += num_restart;
  }

  // If --wasm-num-compilation-tasks=0 is passed, do only spawn foreground
  // tasks. This is used to make timing deterministic.
  v8::TaskRunner* task_runner = FLAG_wasm_num_compilation_tasks > 0
                                    ? background_task_runner_.get()
                                    : foreground_task_runner_.get();
  for (; num_restart > 0; --num_restart) {
    task_runner->PostTask(base::make_unique<BackgroundCompileTask>(
        this, &background_task_manager_));
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

bool CompilationState::StopBackgroundCompilationTaskForThrottling() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  DCHECK_LE(1, num_background_tasks_);
  bool can_accept_work = allocated_memory_ < max_memory_;
  if (can_accept_work) return false;
  --num_background_tasks_;
  return true;
}

void CompilationState::Abort() {
  {
    base::LockGuard<base::Mutex> guard(&mutex_);
    failed_ = true;
  }
  CancelAndWait();
}

void CompilationState::NotifyOnEvent(CompilationEvent event,
                                     ErrorThrower* thrower) {
  for (auto& callback_function : callbacks_) {
    callback_function(event, thrower);
  }
}

void CompileJsToWasmWrappers(Isolate* isolate,
                             Handle<WasmModuleObject> module_object,
                             Counters* counters) {
  JSToWasmWrapperCache js_to_wasm_cache;
  int wrapper_index = 0;
  Handle<FixedArray> export_wrappers(module_object->export_wrappers(), isolate);
  NativeModule* native_module =
      module_object->compiled_module()->GetNativeModule();
  wasm::UseTrapHandler use_trap_handler =
      native_module->use_trap_handler() ? kUseTrapHandler : kNoTrapHandler;
  WasmModule* module = native_module->shared_module_data()->module();
  for (auto exp : module->export_table) {
    if (exp.kind != kExternalFunction) continue;
    Address call_target =
        exp.index < module->num_imported_functions
            ? kNullAddress
            : native_module->GetCallTargetForFunction(exp.index);
    Handle<Code> wrapper_code = js_to_wasm_cache.CloneOrCompileJSToWasmWrapper(
        isolate, module, call_target, exp.index, use_trap_handler);
    export_wrappers->set(wrapper_index, *wrapper_code);
    RecordStats(*wrapper_code, counters);
    ++wrapper_index;
  }
}

Handle<Script> CreateWasmScript(Isolate* isolate,
                                const ModuleWireBytes& wire_bytes) {
  Handle<Script> script =
      isolate->factory()->NewScript(isolate->factory()->empty_string());
  script->set_context_data(isolate->native_context()->debug_context_id());
  script->set_type(Script::TYPE_WASM);

  int hash = StringHasher::HashSequentialString(
      reinterpret_cast<const char*>(wire_bytes.start()),
      static_cast<int>(wire_bytes.length()), kZeroHashSeed);

  const int kBufferSize = 32;
  char buffer[kBufferSize];
  int url_chars = SNPrintF(ArrayVector(buffer), "wasm://wasm/%08x", hash);
  DCHECK(url_chars >= 0 && url_chars < kBufferSize);
  MaybeHandle<String> url_str = isolate->factory()->NewStringFromOneByte(
      Vector<const uint8_t>(reinterpret_cast<uint8_t*>(buffer), url_chars),
      TENURED);
  script->set_source_url(*url_str.ToHandleChecked());

  int name_chars = SNPrintF(ArrayVector(buffer), "wasm-%08x", hash);
  DCHECK(name_chars >= 0 && name_chars < kBufferSize);
  MaybeHandle<String> name_str = isolate->factory()->NewStringFromOneByte(
      Vector<const uint8_t>(reinterpret_cast<uint8_t*>(buffer), name_chars),
      TENURED);
  script->set_name(*name_str.ToHandleChecked());

  return script;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef TRACE
#undef TRACE_CHAIN
#undef TRACE_COMPILE
#undef TRACE_STREAMING
#undef TRACE_LAZY

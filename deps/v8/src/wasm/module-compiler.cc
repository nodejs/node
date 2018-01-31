// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-compiler.h"

#include <atomic>

#include "src/api.h"
#include "src/asmjs/asm-js.h"
#include "src/assembler-inl.h"
#include "src/base/optional.h"
#include "src/base/template-utils.h"
#include "src/base/utils/random-number-generator.h"
#include "src/code-stubs.h"
#include "src/compiler/wasm-compiler.h"
#include "src/counters.h"
#include "src/property-descriptor.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/compilation-manager.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-specialization.h"
#include "src/wasm/wasm-heap.h"
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

static const int kInvalidSigIndex = -1;

namespace v8 {
namespace internal {
namespace wasm {

// A class compiling an entire module.
class ModuleCompiler {
 public:
  ModuleCompiler(Isolate* isolate, WasmModule* module, Handle<Code> centry_stub,
                 wasm::NativeModule* native_module);

  // The actual runnable task that performs compilations in the background.
  class CompilationTask : public CancelableTask {
   public:
    ModuleCompiler* compiler_;
    explicit CompilationTask(ModuleCompiler* compiler)
        : CancelableTask(&compiler->background_task_manager_),
          compiler_(compiler) {}

    void RunInternal() override {
      while (compiler_->executed_units_.CanAcceptWork() &&
             compiler_->FetchAndExecuteCompilationUnit()) {
      }

      compiler_->OnBackgroundTaskStopped();
    }
  };

  // The CompilationUnitBuilder builds compilation units and stores them in an
  // internal buffer. The buffer is moved into the working queue of the
  // ModuleCompiler when {Commit} is called.
  class CompilationUnitBuilder {
   public:
    explicit CompilationUnitBuilder(ModuleCompiler* compiler)
        : compiler_(compiler) {}

    ~CompilationUnitBuilder() { DCHECK(units_.empty()); }

    void AddUnit(compiler::ModuleEnv* module_env,
                 wasm::NativeModule* native_module,
                 const WasmFunction* function, uint32_t buffer_offset,
                 Vector<const uint8_t> bytes, WasmName name) {
      units_.emplace_back(new compiler::WasmCompilationUnit(
          compiler_->isolate_, module_env, native_module,
          wasm::FunctionBody{function->sig, buffer_offset, bytes.begin(),
                             bytes.end()},
          name, function->func_index, compiler_->centry_stub_,
          compiler::WasmCompilationUnit::GetDefaultCompilationMode(),
          compiler_->counters()));
    }

    void Commit() {
      {
        base::LockGuard<base::Mutex> guard(
            &compiler_->compilation_units_mutex_);
        compiler_->compilation_units_.insert(
            compiler_->compilation_units_.end(),
            std::make_move_iterator(units_.begin()),
            std::make_move_iterator(units_.end()));
      }
      units_.clear();
    }

    void Clear() { units_.clear(); }

   private:
    ModuleCompiler* compiler_;
    std::vector<std::unique_ptr<compiler::WasmCompilationUnit>> units_;
  };

  class CodeGenerationSchedule {
   public:
    explicit CodeGenerationSchedule(
        base::RandomNumberGenerator* random_number_generator,
        size_t max_memory = 0);

    void Schedule(std::unique_ptr<compiler::WasmCompilationUnit>&& item);

    bool IsEmpty() const { return schedule_.empty(); }

    std::unique_ptr<compiler::WasmCompilationUnit> GetNext();

    bool CanAcceptWork() const;

    bool ShouldIncreaseWorkload() const;

    void EnableThrottling() { throttle_ = true; }

   private:
    size_t GetRandomIndexInSchedule();

    base::RandomNumberGenerator* random_number_generator_ = nullptr;
    std::vector<std::unique_ptr<compiler::WasmCompilationUnit>> schedule_;
    const size_t max_memory_;
    bool throttle_ = false;
    base::AtomicNumber<size_t> allocated_memory_{0};
  };

  Counters* counters() const { return async_counters_.get(); }

  // Run by each compilation task and by the main thread (i.e. in both
  // foreground and background threads). The no_finisher_callback is called
  // within the result_mutex_ lock when no finishing task is running, i.e. when
  // the finisher_is_running_ flag is not set.
  bool FetchAndExecuteCompilationUnit(
      std::function<void()> no_finisher_callback = nullptr);

  void OnBackgroundTaskStopped();

  void EnableThrottling() { executed_units_.EnableThrottling(); }

  bool CanAcceptWork() const { return executed_units_.CanAcceptWork(); }

  bool ShouldIncreaseWorkload() const {
    return executed_units_.ShouldIncreaseWorkload();
  }

  size_t InitializeCompilationUnits(const std::vector<WasmFunction>& functions,
                                    const ModuleWireBytes& wire_bytes,
                                    compiler::ModuleEnv* module_env);

  void RestartCompilationTasks();

  size_t FinishCompilationUnits(std::vector<Handle<Code>>& results,
                                ErrorThrower* thrower);

  bool IsFinisherRunning() const { return finisher_is_running_; }

  void SetFinisherIsRunning(bool value);

  WasmCodeWrapper FinishCompilationUnit(ErrorThrower* thrower, int* func_index);

  void CompileInParallel(const ModuleWireBytes& wire_bytes,
                         compiler::ModuleEnv* module_env,
                         std::vector<Handle<Code>>& results,
                         ErrorThrower* thrower);

  void CompileSequentially(const ModuleWireBytes& wire_bytes,
                           compiler::ModuleEnv* module_env,
                           std::vector<Handle<Code>>& results,
                           ErrorThrower* thrower);

  void ValidateSequentially(const ModuleWireBytes& wire_bytes,
                            compiler::ModuleEnv* module_env,
                            ErrorThrower* thrower);

  static MaybeHandle<WasmModuleObject> CompileToModuleObject(
      Isolate* isolate, ErrorThrower* thrower,
      std::unique_ptr<WasmModule> module, const ModuleWireBytes& wire_bytes,
      Handle<Script> asm_js_script,
      Vector<const byte> asm_js_offset_table_bytes);

 private:
  MaybeHandle<WasmModuleObject> CompileToModuleObjectInternal(
      ErrorThrower* thrower, std::unique_ptr<WasmModule> module,
      const ModuleWireBytes& wire_bytes, Handle<Script> asm_js_script,
      Vector<const byte> asm_js_offset_table_bytes);

  Isolate* isolate_;
  WasmModule* module_;
  const std::shared_ptr<Counters> async_counters_;
  std::vector<std::unique_ptr<compiler::WasmCompilationUnit>>
      compilation_units_;
  base::Mutex compilation_units_mutex_;
  CodeGenerationSchedule executed_units_;
  base::Mutex result_mutex_;
  const size_t num_background_tasks_;
  // This flag should only be set while holding result_mutex_.
  bool finisher_is_running_ = false;
  CancelableTaskManager background_task_manager_;
  size_t stopped_compilation_tasks_ = 0;
  base::Mutex tasks_mutex_;
  Handle<Code> centry_stub_;
  wasm::NativeModule* native_module_;
};

namespace {

class JSToWasmWrapperCache {
 public:
  void SetContextAddress(Address context_address) {
    // Prevent to have different context addresses in the cache.
    DCHECK(code_cache_.empty());
    context_address_ = context_address;
  }

  Handle<Code> CloneOrCompileJSToWasmWrapper(Isolate* isolate,
                                             wasm::WasmModule* module,
                                             WasmCodeWrapper wasm_code,
                                             uint32_t index) {
    const wasm::WasmFunction* func = &module->functions[index];
    int cached_idx = sig_map_.Find(func->sig);
    if (cached_idx >= 0) {
      Handle<Code> code = isolate->factory()->CopyCode(code_cache_[cached_idx]);
      // Now patch the call to wasm code.
      if (wasm_code.IsCodeObject()) {
        for (RelocIterator it(*code, RelocInfo::kCodeTargetMask);; it.next()) {
          DCHECK(!it.done());
          Code* target =
              Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
          if (target->kind() == Code::WASM_FUNCTION ||
              target->kind() == Code::WASM_TO_JS_FUNCTION ||
              target->kind() == Code::WASM_TO_WASM_FUNCTION ||
              target->builtin_index() == Builtins::kIllegal ||
              target->builtin_index() == Builtins::kWasmCompileLazy) {
            it.rinfo()->set_target_address(
                isolate, wasm_code.GetCode()->instruction_start());
            break;
          }
        }
      } else {
        for (RelocIterator it(*code,
                              RelocInfo::ModeMask(RelocInfo::JS_TO_WASM_CALL));
             ; it.next()) {
          DCHECK(!it.done());
          it.rinfo()->set_js_to_wasm_address(
              isolate, wasm_code.is_null()
                           ? nullptr
                           : wasm_code.GetWasmCode()->instructions().start());
          break;
        }
      }
      return code;
    }

    Handle<Code> code = compiler::CompileJSToWasmWrapper(
        isolate, module, wasm_code, index, context_address_);
    uint32_t new_cache_idx = sig_map_.FindOrInsert(func->sig);
    DCHECK_EQ(code_cache_.size(), new_cache_idx);
    USE(new_cache_idx);
    code_cache_.push_back(code);
    return code;
  }

 private:
  // sig_map_ maps signatures to an index in code_cache_.
  wasm::SignatureMap sig_map_;
  std::vector<Handle<Code>> code_cache_;
  Address context_address_ = nullptr;
};

// A helper class to simplify instantiating a module from a compiled module.
// It closes over the {Isolate}, the {ErrorThrower}, the {WasmCompiledModule},
// etc.
class InstanceBuilder {
 public:
  InstanceBuilder(Isolate* isolate, ErrorThrower* thrower,
                  Handle<WasmModuleObject> module_object,
                  MaybeHandle<JSReceiver> ffi,
                  MaybeHandle<JSArrayBuffer> memory,
                  WeakCallbackInfo<void>::Callback instance_finalizer_callback);

  // Build an instance, in all of its glory.
  MaybeHandle<WasmInstanceObject> Build();

 private:
  // Represents the initialized state of a table.
  struct TableInstance {
    Handle<WasmTableObject> table_object;  // WebAssembly.Table instance
    Handle<FixedArray> js_wrappers;        // JSFunctions exported
    Handle<FixedArray> function_table;     // internal code array
    Handle<FixedArray> signature_table;    // internal sig array
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
  JSToWasmWrapperCache js_to_wasm_cache_;
  WeakCallbackInfo<void>::Callback instance_finalizer_callback_;
  std::vector<SanitizedImport> sanitized_imports_;

  const std::shared_ptr<Counters>& async_counters() const {
    return async_counters_;
  }
  Counters* counters() const { return async_counters().get(); }

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
  void LoadDataSegments(WasmContext* wasm_context);

  void WriteGlobalValue(WasmGlobal& global, Handle<Object> value);

  void SanitizeImports();

  Handle<FixedArray> SetupWasmToJSImportsTable(
      Handle<WasmInstanceObject> instance);

  // Process the imports, including functions, tables, globals, and memory, in
  // order, loading them from the {ffi_} object. Returns the number of imported
  // functions.
  int ProcessImports(Handle<FixedArray> code_table,
                     Handle<WasmInstanceObject> instance);

  template <typename T>
  T* GetRawGlobalPtr(WasmGlobal& global);

  // Process initialization of globals.
  void InitGlobals();

  // Allocate memory for a module instance as a new JSArrayBuffer.
  Handle<JSArrayBuffer> AllocateMemory(uint32_t num_pages);

  bool NeedsWrappers() const;

  // Process the exports, creating wrappers for functions, tables, memories,
  // and globals.
  void ProcessExports(Handle<WasmInstanceObject> instance,
                      Handle<WasmCompiledModule> compiled_module);

  void InitializeTables(Handle<WasmInstanceObject> instance,
                        CodeSpecialization* code_specialization);

  void LoadTableSegments(Handle<FixedArray> code_table,
                         Handle<WasmInstanceObject> instance);
};

// TODO(titzer): move to wasm-objects.cc
void InstanceFinalizer(const v8::WeakCallbackInfo<void>& data) {
  DisallowHeapAllocation no_gc;
  JSObject** p = reinterpret_cast<JSObject**>(data.GetParameter());
  WasmInstanceObject* owner = reinterpret_cast<WasmInstanceObject*>(*p);
  Isolate* isolate = reinterpret_cast<Isolate*>(data.GetIsolate());
  // If a link to shared memory instances exists, update the list of memory
  // instances before the instance is destroyed.
  WasmCompiledModule* compiled_module = owner->compiled_module();
  wasm::NativeModule* native_module = compiled_module->GetNativeModule();
  if (FLAG_wasm_jit_to_native) {
    if (native_module) {
      TRACE("Finalizing %zu {\n", native_module->instance_id);
    } else {
      TRACE("Finalized already cleaned up compiled module\n");
    }
  } else {
    TRACE("Finalizing %d {\n", compiled_module->instance_id());

    if (trap_handler::UseTrapHandler()) {
      // TODO(6792): No longer needed once WebAssembly code is off heap.
      CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
      Handle<FixedArray> code_table = compiled_module->code_table();
      for (int i = 0; i < code_table->length(); ++i) {
        Handle<Code> code = code_table->GetValueChecked<Code>(isolate, i);
        int index = code->trap_handler_index()->value();
        if (index >= 0) {
          trap_handler::ReleaseHandlerData(index);
          code->set_trap_handler_index(
              Smi::FromInt(trap_handler::kInvalidIndex));
        }
      }
    }
  }
  WeakCell* weak_wasm_module = compiled_module->ptr_to_weak_wasm_module();

  // Since the order of finalizers is not guaranteed, it can be the case
  // that {instance->compiled_module()->module()}, which is a
  // {Managed<WasmModule>} has been collected earlier in this GC cycle.
  // Weak references to this instance won't be cleared until
  // the next GC cycle, so we need to manually break some links (such as
  // the weak references from {WasmMemoryObject::instances}.
  if (owner->has_memory_object()) {
    Handle<WasmMemoryObject> memory(owner->memory_object(), isolate);
    Handle<WasmInstanceObject> instance(owner, isolate);
    WasmMemoryObject::RemoveInstance(isolate, memory, instance);
  }

  // weak_wasm_module may have been cleared, meaning the module object
  // was GC-ed. We still want to maintain the links between instances, to
  // release the WasmCompiledModule corresponding to the WasmModuleInstance
  // being finalized here.
  WasmModuleObject* wasm_module = nullptr;
  if (!weak_wasm_module->cleared()) {
    wasm_module = WasmModuleObject::cast(weak_wasm_module->value());
    WasmCompiledModule* current_template = wasm_module->compiled_module();

    TRACE("chain before {\n");
    TRACE_CHAIN(current_template);
    TRACE("}\n");

    DCHECK(!current_template->has_prev_instance());
    if (current_template == compiled_module) {
      if (!compiled_module->has_next_instance()) {
        WasmCompiledModule::Reset(isolate, compiled_module);
      } else {
        WasmModuleObject::cast(wasm_module)
            ->set_compiled_module(compiled_module->ptr_to_next_instance());
      }
    }
  }

  compiled_module->RemoveFromChain();

  if (wasm_module != nullptr) {
    TRACE("chain after {\n");
    TRACE_CHAIN(wasm_module->compiled_module());
    TRACE("}\n");
  }
  compiled_module->reset_weak_owning_instance();
  GlobalHandles::Destroy(reinterpret_cast<Object**>(p));
  TRACE("}\n");
}

}  // namespace

bool SyncValidate(Isolate* isolate, const ModuleWireBytes& bytes) {
  if (bytes.start() == nullptr || bytes.length() == 0) return false;
  ModuleResult result = SyncDecodeWasmModule(isolate, bytes.start(),
                                             bytes.end(), true, kWasmOrigin);
  return result.ok();
}

MaybeHandle<WasmModuleObject> SyncCompileTranslatedAsmJs(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {
  ModuleResult result = SyncDecodeWasmModule(isolate, bytes.start(),
                                             bytes.end(), false, kAsmJsOrigin);
  if (result.failed()) {
    thrower->CompileFailed("Wasm decoding failed", result);
    return {};
  }

  // Transfer ownership of the WasmModule to the {WasmModuleWrapper} generated
  // in {CompileToModuleObject}.
  return ModuleCompiler::CompileToModuleObject(
      isolate, thrower, std::move(result.val), bytes, asm_js_script,
      asm_js_offset_table_bytes);
}

MaybeHandle<WasmModuleObject> SyncCompile(Isolate* isolate,
                                          ErrorThrower* thrower,
                                          const ModuleWireBytes& bytes) {
  // TODO(titzer): only make a copy of the bytes if SharedArrayBuffer
  std::unique_ptr<byte[]> copy(new byte[bytes.length()]);
  memcpy(copy.get(), bytes.start(), bytes.length());
  ModuleWireBytes bytes_copy(copy.get(), copy.get() + bytes.length());

  ModuleResult result = SyncDecodeWasmModule(
      isolate, bytes_copy.start(), bytes_copy.end(), false, kWasmOrigin);
  if (result.failed()) {
    thrower->CompileFailed("Wasm decoding failed", result);
    return {};
  }

  // Transfer ownership of the WasmModule to the {WasmModuleWrapper} generated
  // in {CompileToModuleObject}.
  return ModuleCompiler::CompileToModuleObject(
      isolate, thrower, std::move(result.val), bytes_copy, Handle<Script>(),
      Vector<const byte>());
}

MaybeHandle<WasmInstanceObject> SyncInstantiate(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory) {
  InstanceBuilder builder(isolate, thrower, module_object, imports, memory,
                          &InstanceFinalizer);
  return builder.Build();
}

MaybeHandle<WasmInstanceObject> SyncCompileAndInstantiate(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    MaybeHandle<JSReceiver> imports, MaybeHandle<JSArrayBuffer> memory) {
  MaybeHandle<WasmModuleObject> module = SyncCompile(isolate, thrower, bytes);
  DCHECK_EQ(thrower->error(), module.is_null());
  if (module.is_null()) return {};

  return SyncInstantiate(isolate, thrower, module.ToHandleChecked(), imports,
                         memory);
}

void RejectPromise(Isolate* isolate, Handle<Context> context,
                   ErrorThrower& thrower, Handle<JSPromise> promise) {
  Local<Promise::Resolver> resolver =
      Utils::PromiseToLocal(promise).As<Promise::Resolver>();
  auto maybe = resolver->Reject(Utils::ToLocal(context),
                                Utils::ToLocal(thrower.Reify()));
  CHECK_IMPLIES(!maybe.FromMaybe(false), isolate->has_scheduled_exception());
}

void ResolvePromise(Isolate* isolate, Handle<Context> context,
                    Handle<JSPromise> promise, Handle<Object> result) {
  Local<Promise::Resolver> resolver =
      Utils::PromiseToLocal(promise).As<Promise::Resolver>();
  auto maybe =
      resolver->Resolve(Utils::ToLocal(context), Utils::ToLocal(result));
  CHECK_IMPLIES(!maybe.FromMaybe(false), isolate->has_scheduled_exception());
}

void AsyncInstantiate(Isolate* isolate, Handle<JSPromise> promise,
                      Handle<WasmModuleObject> module_object,
                      MaybeHandle<JSReceiver> imports) {
  ErrorThrower thrower(isolate, nullptr);
  MaybeHandle<WasmInstanceObject> instance_object = SyncInstantiate(
      isolate, &thrower, module_object, imports, Handle<JSArrayBuffer>::null());
  if (thrower.error()) {
    RejectPromise(isolate, handle(isolate->context()), thrower, promise);
    return;
  }
  ResolvePromise(isolate, handle(isolate->context()), promise,
                 instance_object.ToHandleChecked());
}

void AsyncCompile(Isolate* isolate, Handle<JSPromise> promise,
                  const ModuleWireBytes& bytes) {
  if (!FLAG_wasm_async_compilation) {
    ErrorThrower thrower(isolate, "WasmCompile");
    // Compile the module.
    MaybeHandle<WasmModuleObject> module_object =
        SyncCompile(isolate, &thrower, bytes);
    if (thrower.error()) {
      RejectPromise(isolate, handle(isolate->context()), thrower, promise);
      return;
    }
    Handle<WasmModuleObject> module = module_object.ToHandleChecked();
    ResolvePromise(isolate, handle(isolate->context()), promise, module);
    return;
  }

  if (FLAG_wasm_test_streaming) {
    std::shared_ptr<StreamingDecoder> streaming_decoder =
        isolate->wasm_compilation_manager()->StartStreamingCompilation(
            isolate, handle(isolate->context()), promise);
    streaming_decoder->OnBytesReceived(bytes.module_bytes());
    streaming_decoder->Finish();
    return;
  }
  // Make a copy of the wire bytes in case the user program changes them
  // during asynchronous compilation.
  std::unique_ptr<byte[]> copy(new byte[bytes.length()]);
  memcpy(copy.get(), bytes.start(), bytes.length());
  isolate->wasm_compilation_manager()->StartAsyncCompileJob(
      isolate, std::move(copy), bytes.length(), handle(isolate->context()),
      promise);
}

Handle<Code> CompileLazyOnGCHeap(Isolate* isolate) {
  HistogramTimerScope lazy_time_scope(
      isolate->counters()->wasm_lazy_compilation_time());

  // Find the wasm frame which triggered the lazy compile, to get the wasm
  // instance.
  StackFrameIterator it(isolate);
  // First frame: C entry stub.
  DCHECK(!it.done());
  DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
  it.Advance();
  // Second frame: WasmCompileLazy builtin.
  DCHECK(!it.done());
  Handle<Code> lazy_compile_code(it.frame()->LookupCode(), isolate);
  DCHECK_EQ(Builtins::kWasmCompileLazy, lazy_compile_code->builtin_index());
  Handle<WasmInstanceObject> instance;
  Handle<FixedArray> exp_deopt_data;
  int func_index = -1;
  // If the lazy compile stub has deopt data, use that to determine the
  // instance and function index. Otherwise this must be a wasm->wasm call
  // within one instance, so extract the information from the caller.
  if (lazy_compile_code->deoptimization_data()->length() > 0) {
    exp_deopt_data = handle(lazy_compile_code->deoptimization_data(), isolate);
    auto func_info = GetWasmFunctionInfo(isolate, lazy_compile_code);
    instance = func_info.instance.ToHandleChecked();
    func_index = func_info.func_index;
  }
  it.Advance();
  // Third frame: The calling wasm code or js-to-wasm wrapper.
  DCHECK(!it.done());
  DCHECK(it.frame()->is_js_to_wasm() || it.frame()->is_wasm_compiled());
  Handle<Code> caller_code = handle(it.frame()->LookupCode(), isolate);
  if (it.frame()->is_js_to_wasm()) {
    DCHECK(!instance.is_null());
  } else if (instance.is_null()) {
    // Then this is a direct call (otherwise we would have attached the instance
    // via deopt data to the lazy compile stub). Just use the instance of the
    // caller.
    instance =
        handle(WasmInstanceObject::GetOwningInstanceGC(*caller_code), isolate);
  }
  int offset =
      static_cast<int>(it.frame()->pc() - caller_code->instruction_start());
  // Only patch the caller code if this is *no* indirect call.
  // exp_deopt_data will be null if the called function is not exported at all,
  // and its length will be <= 2 if all entries in tables were already patched.
  // Note that this check is conservative: If the first call to an exported
  // function is direct, we will just patch the export tables, and only on the
  // second call we will patch the caller.
  bool patch_caller = caller_code->kind() == Code::JS_TO_WASM_FUNCTION ||
                      exp_deopt_data.is_null() || exp_deopt_data->length() <= 2;

  wasm::LazyCompilationOrchestrator* orchestrator =
      Managed<wasm::LazyCompilationOrchestrator>::cast(
          instance->compiled_module()
              ->shared()
              ->lazy_compilation_orchestrator())
          ->get();
  Handle<Code> compiled_code = orchestrator->CompileLazyOnGCHeap(
      isolate, instance, caller_code, offset, func_index, patch_caller);
  if (!exp_deopt_data.is_null() && exp_deopt_data->length() > 2) {
    TRACE_LAZY("Patching %d position(s) in function tables.\n",
               (exp_deopt_data->length() - 2) / 2);
    // See EnsureExportedLazyDeoptData: exp_deopt_data[2...(len-1)] are pairs of
    // <export_table, index> followed by undefined values.
    // Use this information here to patch all export tables.
    DCHECK_EQ(0, exp_deopt_data->length() % 2);
    for (int idx = 2, end = exp_deopt_data->length(); idx < end; idx += 2) {
      if (exp_deopt_data->get(idx)->IsUndefined(isolate)) break;
      FixedArray* exp_table = FixedArray::cast(exp_deopt_data->get(idx));
      int exp_index = Smi::ToInt(exp_deopt_data->get(idx + 1));
      DCHECK(exp_table->get(exp_index) == *lazy_compile_code);
      exp_table->set(exp_index, *compiled_code);
    }
    // TODO(6792): No longer needed once WebAssembly code is off heap.
    CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
    // After processing, remove the list of exported entries, such that we don't
    // do the patching redundantly.
    Handle<FixedArray> new_deopt_data =
        isolate->factory()->CopyFixedArrayUpTo(exp_deopt_data, 2, TENURED);
    lazy_compile_code->set_deoptimization_data(*new_deopt_data);
  }

  return compiled_code;
}

Address CompileLazy(Isolate* isolate) {
  HistogramTimerScope lazy_time_scope(
      isolate->counters()->wasm_lazy_compilation_time());

  // Find the wasm frame which triggered the lazy compile, to get the wasm
  // instance.
  StackFrameIterator it(isolate);
  // First frame: C entry stub.
  DCHECK(!it.done());
  DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
  it.Advance();
  // Second frame: WasmCompileLazy builtin.
  DCHECK(!it.done());
  Handle<WasmInstanceObject> instance;
  Maybe<uint32_t> func_index_to_compile = Nothing<uint32_t>();
  Handle<Object> exp_deopt_data_entry;
  const wasm::WasmCode* lazy_stub_or_copy =
      isolate->wasm_code_manager()->LookupCode(it.frame()->pc());
  DCHECK_EQ(wasm::WasmCode::LazyStub, lazy_stub_or_copy->kind());
  if (!lazy_stub_or_copy->IsAnonymous()) {
    // Then it's an indirect call or via JS->wasm wrapper.
    instance = lazy_stub_or_copy->owner()->compiled_module()->owning_instance();
    func_index_to_compile = Just(lazy_stub_or_copy->index());
    exp_deopt_data_entry =
        handle(instance->compiled_module()->lazy_compile_data()->get(
                   static_cast<int>(lazy_stub_or_copy->index())),
               isolate);
  }
  it.Advance();
  // Third frame: The calling wasm code (direct or indirect), or js-to-wasm
  // wrapper.
  DCHECK(!it.done());
  DCHECK(it.frame()->is_js_to_wasm() || it.frame()->is_wasm_compiled());
  Handle<Code> js_to_wasm_caller_code;
  const WasmCode* wasm_caller_code = nullptr;
  Maybe<uint32_t> offset = Nothing<uint32_t>();
  if (it.frame()->is_js_to_wasm()) {
    DCHECK(!instance.is_null());
    js_to_wasm_caller_code = handle(it.frame()->LookupCode(), isolate);
  } else {
    wasm_caller_code =
        isolate->wasm_code_manager()->LookupCode(it.frame()->pc());
    offset = Just(static_cast<uint32_t>(
        it.frame()->pc() - wasm_caller_code->instructions().start()));
    if (instance.is_null()) {
      // Then this is a direct call (otherwise we would have attached the
      // instance via deopt data to the lazy compile stub). Just use the
      // instance of the caller.
      instance =
          wasm_caller_code->owner()->compiled_module()->owning_instance();
    }
  }

  Handle<WasmCompiledModule> compiled_module(instance->compiled_module());

  wasm::LazyCompilationOrchestrator* orchestrator =
      Managed<wasm::LazyCompilationOrchestrator>::cast(
          compiled_module->shared()->lazy_compilation_orchestrator())
          ->get();
  const wasm::WasmCode* result = nullptr;
  // The caller may be js to wasm calling a function
  // also available for indirect calls.
  if (!js_to_wasm_caller_code.is_null()) {
    result = orchestrator->CompileFromJsToWasm(
        isolate, instance, js_to_wasm_caller_code,
        func_index_to_compile.ToChecked());
  } else {
    DCHECK_NOT_NULL(wasm_caller_code);
    if (func_index_to_compile.IsNothing() ||
        (!exp_deopt_data_entry.is_null() &&
         !exp_deopt_data_entry->IsFixedArray())) {
      result = orchestrator->CompileDirectCall(
          isolate, instance, func_index_to_compile, wasm_caller_code,
          offset.ToChecked());
    } else {
      result = orchestrator->CompileIndirectCall(
          isolate, instance, func_index_to_compile.ToChecked());
    }
  }
  DCHECK_NOT_NULL(result);

  int func_index = static_cast<int>(result->index());
  if (!exp_deopt_data_entry.is_null() && exp_deopt_data_entry->IsFixedArray()) {
    Handle<FixedArray> exp_deopt_data =
        Handle<FixedArray>::cast(exp_deopt_data_entry);

    TRACE_LAZY("Patching %d position(s) in function tables.\n",
               exp_deopt_data->length() / 2);

    // See EnsureExportedLazyDeoptData: exp_deopt_data[0...(len-1)] are pairs
    // of <export_table, index> followed by undefined values. Use this
    // information here to patch all export tables.
    for (int idx = 0, end = exp_deopt_data->length(); idx < end; idx += 2) {
      if (exp_deopt_data->get(idx)->IsUndefined(isolate)) break;
      FixedArray* exp_table = FixedArray::cast(exp_deopt_data->get(idx));
      int exp_index = Smi::ToInt(exp_deopt_data->get(idx + 1));
      Handle<Foreign> foreign_holder = isolate->factory()->NewForeign(
          result->instructions().start(), TENURED);
      exp_table->set(exp_index, *foreign_holder);
    }
    // TODO(6792): No longer needed once WebAssembly code is off heap.
    CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
    // After processing, remove the list of exported entries, such that we don't
    // do the patching redundantly.
    compiled_module->lazy_compile_data()->set(
        func_index, isolate->heap()->undefined_value());
  }

  return result->instructions().start();
}

compiler::ModuleEnv CreateModuleEnvFromCompiledModule(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  DisallowHeapAllocation no_gc;
  WasmModule* module = compiled_module->module();
  std::vector<Handle<Code>> empty_code;
  if (FLAG_wasm_jit_to_native) {
    NativeModule* native_module = compiled_module->GetNativeModule();
    std::vector<GlobalHandleAddress> function_tables =
        native_module->function_tables();
    std::vector<GlobalHandleAddress> signature_tables =
        native_module->signature_tables();

    compiler::ModuleEnv result = {module,            // --
                                  function_tables,   // --
                                  signature_tables,  // --
                                  empty_code,
                                  BUILTIN_CODE(isolate, WasmCompileLazy)};
    return result;
  } else {
    std::vector<GlobalHandleAddress> function_tables;
    std::vector<GlobalHandleAddress> signature_tables;

    int num_function_tables = static_cast<int>(module->function_tables.size());
    for (int i = 0; i < num_function_tables; ++i) {
      FixedArray* ft = compiled_module->ptr_to_function_tables();
      FixedArray* st = compiled_module->ptr_to_signature_tables();

      // TODO(clemensh): defer these handles for concurrent compilation.
      function_tables.push_back(WasmCompiledModule::GetTableValue(ft, i));
      signature_tables.push_back(WasmCompiledModule::GetTableValue(st, i));
    }

    compiler::ModuleEnv result = {module,            // --
                                  function_tables,   // --
                                  signature_tables,  // --
                                  empty_code,        // --
                                  BUILTIN_CODE(isolate, WasmCompileLazy)};
    return result;
  }
}

const wasm::WasmCode* LazyCompilationOrchestrator::CompileFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int func_index) {
  base::ElapsedTimer compilation_timer;
  compilation_timer.Start();
  Handle<WasmCompiledModule> compiled_module(instance->compiled_module(),
                                             isolate);
  if (FLAG_wasm_jit_to_native) {
    wasm::WasmCode* existing_code = compiled_module->GetNativeModule()->GetCode(
        static_cast<uint32_t>(func_index));
    if (existing_code != nullptr &&
        existing_code->kind() == wasm::WasmCode::Function) {
      TRACE_LAZY("Function %d already compiled.\n", func_index);
      return existing_code;
    }
  } else {
    if (Code::cast(compiled_module->code_table()->get(func_index))->kind() ==
        Code::WASM_FUNCTION) {
      TRACE_LAZY("Function %d already compiled.\n", func_index);
      return nullptr;
    }
  }

  compiler::ModuleEnv module_env =
      CreateModuleEnvFromCompiledModule(isolate, compiled_module);

  const uint8_t* module_start = compiled_module->module_bytes()->GetChars();

  const WasmFunction* func = &module_env.module->functions[func_index];
  FunctionBody body{func->sig, func->code.offset(),
                    module_start + func->code.offset(),
                    module_start + func->code.end_offset()};
  // TODO(wasm): Refactor this to only get the name if it is really needed for
  // tracing / debugging.
  std::string func_name;
  {
    WasmName name = Vector<const char>::cast(
        compiled_module->GetRawFunctionName(func_index));
    // Copy to std::string, because the underlying string object might move on
    // the heap.
    func_name.assign(name.start(), static_cast<size_t>(name.length()));
  }
  ErrorThrower thrower(isolate, "WasmLazyCompile");
  compiler::WasmCompilationUnit unit(isolate, &module_env,
                                     compiled_module->GetNativeModule(), body,
                                     CStrVector(func_name.c_str()), func_index,
                                     CEntryStub(isolate, 1).GetCode());
  unit.ExecuteCompilation();
  // TODO(6792): No longer needed once WebAssembly code is off heap.
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
  WasmCodeWrapper code_wrapper = unit.FinishCompilation(&thrower);

  // If there is a pending error, something really went wrong. The module was
  // verified before starting execution with lazy compilation.
  // This might be OOM, but then we cannot continue execution anyway.
  // TODO(clemensh): According to the spec, we can actually skip validation at
  // module creation time, and return a function that always traps here.
  CHECK(!thrower.error());
  // Now specialize the generated code for this instance.

  // {code} is used only when !FLAG_wasm_jit_to_native, so it may be removed
  // when that flag is removed.
  Handle<Code> code;
  if (code_wrapper.IsCodeObject()) {
    code = code_wrapper.GetCode();
    AttachWasmFunctionInfo(isolate, code, instance, func_index);
    DCHECK_EQ(Builtins::kWasmCompileLazy,
              Code::cast(compiled_module->code_table()->get(func_index))
                  ->builtin_index());
    compiled_module->code_table()->set(func_index, *code);
  }
  Zone specialization_zone(isolate->allocator(), ZONE_NAME);
  CodeSpecialization code_specialization(isolate, &specialization_zone);
  code_specialization.RelocateDirectCalls(instance);
  code_specialization.ApplyToWasmCode(code_wrapper, SKIP_ICACHE_FLUSH);
  int64_t func_size =
      static_cast<int64_t>(func->code.end_offset() - func->code.offset());
  int64_t compilation_time = compilation_timer.Elapsed().InMicroseconds();

  auto counters = isolate->counters();
  counters->wasm_lazily_compiled_functions()->Increment();

  if (!code_wrapper.IsCodeObject()) {
    const wasm::WasmCode* wasm_code = code_wrapper.GetWasmCode();
    Assembler::FlushICache(isolate, wasm_code->instructions().start(),
                           wasm_code->instructions().size());
    counters->wasm_generated_code_size()->Increment(
        static_cast<int>(wasm_code->instructions().size()));
    counters->wasm_reloc_size()->Increment(
        static_cast<int>(wasm_code->reloc_info().size()));

  } else {
    Assembler::FlushICache(isolate, code->instruction_start(),
                           code->instruction_size());
    counters->wasm_generated_code_size()->Increment(code->body_size());
    counters->wasm_reloc_size()->Increment(code->relocation_info()->length());
  }
  counters->wasm_lazy_compilation_throughput()->AddSample(
      compilation_time != 0 ? static_cast<int>(func_size / compilation_time)
                            : 0);
  return !code_wrapper.IsCodeObject() ? code_wrapper.GetWasmCode() : nullptr;
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

Code* ExtractWasmToWasmCallee(Code* wasm_to_wasm) {
  DCHECK_EQ(Code::WASM_TO_WASM_FUNCTION, wasm_to_wasm->kind());
  // Find the one code target in this wrapper.
  RelocIterator it(wasm_to_wasm, RelocInfo::kCodeTargetMask);
  DCHECK(!it.done());
  Code* callee = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
#ifdef DEBUG
  it.next();
  DCHECK(it.done());
#endif
  return callee;
}

void PatchWasmToWasmWrapper(Isolate* isolate, Code* wasm_to_wasm,
                            Code* new_target) {
  DCHECK_EQ(Code::WASM_TO_WASM_FUNCTION, wasm_to_wasm->kind());
  // Find the one code target in this wrapper.
  RelocIterator it(wasm_to_wasm, RelocInfo::kCodeTargetMask);
  DCHECK(!it.done());
  DCHECK_EQ(Builtins::kWasmCompileLazy,
            Code::GetCodeFromTargetAddress(it.rinfo()->target_address())
                ->builtin_index());
  it.rinfo()->set_target_address(isolate, new_target->instruction_start());
#ifdef DEBUG
  it.next();
  DCHECK(it.done());
#endif
}

}  // namespace

Handle<Code> LazyCompilationOrchestrator::CompileLazyOnGCHeap(
    Isolate* isolate, Handle<WasmInstanceObject> instance, Handle<Code> caller,
    int call_offset, int exported_func_index, bool patch_caller) {
  struct NonCompiledFunction {
    int offset;
    int func_index;
  };
  std::vector<NonCompiledFunction> non_compiled_functions;
  int func_to_return_idx = exported_func_index;
  Decoder decoder(nullptr, nullptr);
  bool is_js_to_wasm = caller->kind() == Code::JS_TO_WASM_FUNCTION;
  Handle<WasmCompiledModule> compiled_module(instance->compiled_module(),
                                             isolate);

  TRACE_LAZY(
      "Starting lazy compilation (func %d @%d, js-to-wasm: %d, "
      "patch caller: %d).\n",
      exported_func_index, call_offset, is_js_to_wasm, patch_caller);

  // If this lazy compile stub is being called through a wasm-to-wasm wrapper,
  // remember that code object.
  Handle<Code> wasm_to_wasm_callee;

  // For js-to-wasm wrappers, don't iterate the reloc info. There is just one
  // call site in there anyway.
  if (patch_caller && !is_js_to_wasm) {
    DisallowHeapAllocation no_gc;
    SourcePositionTableIterator source_pos_iterator(
        caller->SourcePositionTable());
    auto caller_func_info = GetWasmFunctionInfo(isolate, caller);
    Handle<WasmCompiledModule> caller_module(
        caller_func_info.instance.ToHandleChecked()->compiled_module(),
        isolate);
    SeqOneByteString* module_bytes = caller_module->module_bytes();
    const byte* func_bytes =
        module_bytes->GetChars() + caller_module->module()
                                       ->functions[caller_func_info.func_index]
                                       .code.offset();
    Code* lazy_callee = nullptr;
    for (RelocIterator it(*caller, RelocInfo::kCodeTargetMask); !it.done();
         it.next()) {
      Code* callee =
          Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
      // TODO(clemensh): Introduce safe_cast<T, bool> which (D)CHECKS
      // (depending on the bool) against limits of T and then static_casts.
      size_t offset_l = it.rinfo()->pc() - caller->instruction_start();
      DCHECK_GE(kMaxInt, offset_l);
      int offset = static_cast<int>(offset_l);
      // Call offset points to the instruction after the call. Remember the last
      // called code object before that offset.
      if (offset < call_offset) lazy_callee = callee;
      if (callee->builtin_index() != Builtins::kWasmCompileLazy) continue;
      int byte_pos =
          AdvanceSourcePositionTableIterator(source_pos_iterator, offset);
      int called_func_index =
          ExtractDirectCallIndex(decoder, func_bytes + byte_pos);
      non_compiled_functions.push_back({offset, called_func_index});
      if (offset < call_offset) func_to_return_idx = called_func_index;
    }
    TRACE_LAZY("Found %zu non-compiled functions in caller.\n",
               non_compiled_functions.size());
    DCHECK_NOT_NULL(lazy_callee);
    if (lazy_callee->kind() == Code::WASM_TO_WASM_FUNCTION) {
      TRACE_LAZY("Callee is a wasm-to-wasm.\n");
      wasm_to_wasm_callee = handle(lazy_callee, isolate);
      // If we call a wasm-to-wasm wrapper, then this wrapper actually
      // tail-called the lazy compile stub. Find it in the wrapper.
      lazy_callee = ExtractWasmToWasmCallee(lazy_callee);
      // This lazy compile stub belongs to the instance that was passed.
      DCHECK_EQ(*instance,
                *GetWasmFunctionInfo(isolate, handle(lazy_callee, isolate))
                     .instance.ToHandleChecked());
      DCHECK_LE(2, lazy_callee->deoptimization_data()->length());
      func_to_return_idx =
          Smi::ToInt(lazy_callee->deoptimization_data()->get(1));
    }
    DCHECK_EQ(Builtins::kWasmCompileLazy, lazy_callee->builtin_index());
    // There must be at least one call to patch (the one that lead to calling
    // the lazy compile stub).
    DCHECK(!non_compiled_functions.empty() || !wasm_to_wasm_callee.is_null());
  }

  TRACE_LAZY("Compiling function %d.\n", func_to_return_idx);

  // TODO(clemensh): compile all functions in non_compiled_functions in
  // background, wait for func_to_return_idx.
  CompileFunction(isolate, instance, func_to_return_idx);

  Handle<Code> compiled_function(
      Code::cast(compiled_module->code_table()->get(func_to_return_idx)),
      isolate);
  DCHECK_EQ(Code::WASM_FUNCTION, compiled_function->kind());

  if (patch_caller || is_js_to_wasm) {
    DisallowHeapAllocation no_gc;
    // TODO(6792): No longer needed once WebAssembly code is off heap.
    CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
    // Now patch the code object with all functions which are now compiled.
    int idx = 0;
    int patched = 0;
    for (RelocIterator it(*caller, RelocInfo::kCodeTargetMask); !it.done();
         it.next()) {
      Code* callee =
          Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
      if (callee->builtin_index() != Builtins::kWasmCompileLazy) {
        // If the callee is the wasm-to-wasm wrapper triggering this lazy
        // compilation, patch it. If is_js_to_wasm is set, we did not set the
        // wasm_to_wasm_callee, so just check the code kind (this is the only
        // call in that wrapper anyway).
        if ((is_js_to_wasm && callee->kind() == Code::WASM_TO_WASM_FUNCTION) ||
            (!wasm_to_wasm_callee.is_null() &&
             callee == *wasm_to_wasm_callee)) {
          TRACE_LAZY("Patching wasm-to-wasm wrapper.\n");
          PatchWasmToWasmWrapper(isolate, callee, *compiled_function);
          ++patched;
        }
        continue;
      }
      int called_func_index = func_to_return_idx;
      if (!is_js_to_wasm) {
        DCHECK_GT(non_compiled_functions.size(), idx);
        called_func_index = non_compiled_functions[idx].func_index;
        DCHECK_EQ(non_compiled_functions[idx].offset,
                  it.rinfo()->pc() - caller->instruction_start());
        ++idx;
      }
      // Check that the callee agrees with our assumed called_func_index.
      DCHECK_IMPLIES(callee->deoptimization_data()->length() > 0,
                     Smi::ToInt(callee->deoptimization_data()->get(1)) ==
                         called_func_index);
      Handle<Code> callee_compiled(
          Code::cast(compiled_module->code_table()->get(called_func_index)));
      if (callee_compiled->builtin_index() == Builtins::kWasmCompileLazy) {
        DCHECK_NE(func_to_return_idx, called_func_index);
        continue;
      }
      DCHECK_EQ(Code::WASM_FUNCTION, callee_compiled->kind());
      it.rinfo()->set_target_address(isolate,
                                     callee_compiled->instruction_start());
      ++patched;
    }
    DCHECK_EQ(non_compiled_functions.size(), idx);
    TRACE_LAZY("Patched %d location(s) in the caller.\n", patched);
    DCHECK_LT(0, patched);
    USE(patched);
  }

  return compiled_function;
}

const wasm::WasmCode* LazyCompilationOrchestrator::CompileFromJsToWasm(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    Handle<Code> js_to_wasm_caller, uint32_t exported_func_index) {
  Decoder decoder(nullptr, nullptr);
  Handle<WasmCompiledModule> compiled_module(instance->compiled_module(),
                                             isolate);

  TRACE_LAZY(
      "Starting lazy compilation (func %u, js_to_wasm: true, patch caller: "
      "true). \n",
      exported_func_index);
  CompileFunction(isolate, instance, exported_func_index);
  {
    DisallowHeapAllocation no_gc;
    int idx = 0;
    for (RelocIterator it(*js_to_wasm_caller,
                          RelocInfo::ModeMask(RelocInfo::JS_TO_WASM_CALL));
         !it.done(); it.next()) {
      ++idx;
      const wasm::WasmCode* callee_compiled =
          compiled_module->GetNativeModule()->GetCode(exported_func_index);
      DCHECK_NOT_NULL(callee_compiled);
      it.rinfo()->set_js_to_wasm_address(
          isolate, callee_compiled->instructions().start());
    }
    DCHECK_EQ(1, idx);
  }

  wasm::WasmCode* ret =
      compiled_module->GetNativeModule()->GetCode(exported_func_index);
  DCHECK_NOT_NULL(ret);
  DCHECK_EQ(wasm::WasmCode::Function, ret->kind());
  return ret;
}

const wasm::WasmCode* LazyCompilationOrchestrator::CompileIndirectCall(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    uint32_t func_index) {
  TRACE_LAZY(
      "Starting lazy compilation (func %u, js_to_wasm: false, patch caller: "
      "false). \n",
      func_index);
  return CompileFunction(isolate, instance, func_index);
}

const wasm::WasmCode* LazyCompilationOrchestrator::CompileDirectCall(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    Maybe<uint32_t> maybe_func_to_return_idx, const wasm::WasmCode* wasm_caller,
    int call_offset) {
  struct WasmDirectCallData {
    uint32_t offset = 0;
    uint32_t func_index = 0;
  };
  std::vector<Maybe<WasmDirectCallData>> non_compiled_functions;
  Decoder decoder(nullptr, nullptr);
  {
    DisallowHeapAllocation no_gc;
    Handle<WasmCompiledModule> caller_module(
        wasm_caller->owner()->compiled_module(), isolate);
    SeqOneByteString* module_bytes = caller_module->module_bytes();
    uint32_t caller_func_index = wasm_caller->index();
    SourcePositionTableIterator source_pos_iterator(
        Handle<ByteArray>(ByteArray::cast(
            caller_module->source_positions()->get(caller_func_index))));

    const byte* func_bytes =
        module_bytes->GetChars() +
        caller_module->module()->functions[caller_func_index].code.offset();
    for (RelocIterator it(wasm_caller->instructions(),
                          wasm_caller->reloc_info(),
                          wasm_caller->constant_pool(),
                          RelocInfo::ModeMask(RelocInfo::WASM_CALL));
         !it.done(); it.next()) {
      const WasmCode* callee = isolate->wasm_code_manager()->LookupCode(
          it.rinfo()->target_address());
      if (callee->kind() != WasmCode::LazyStub) {
        non_compiled_functions.push_back(Nothing<WasmDirectCallData>());
        continue;
      }
      // TODO(clemensh): Introduce safe_cast<T, bool> which (D)CHECKS
      // (depending on the bool) against limits of T and then static_casts.
      size_t offset_l = it.rinfo()->pc() - wasm_caller->instructions().start();
      DCHECK_GE(kMaxInt, offset_l);
      int offset = static_cast<int>(offset_l);
      int byte_pos =
          AdvanceSourcePositionTableIterator(source_pos_iterator, offset);
      uint32_t called_func_index =
          ExtractDirectCallIndex(decoder, func_bytes + byte_pos);
      DCHECK_LT(called_func_index,
                caller_module->GetNativeModule()->FunctionCount());
      WasmDirectCallData data;
      data.offset = offset;
      data.func_index = called_func_index;
      non_compiled_functions.push_back(Just<WasmDirectCallData>(data));
      // Call offset one instruction after the call. Remember the last called
      // function before that offset.
      if (offset < call_offset) {
        maybe_func_to_return_idx = Just(called_func_index);
      }
    }
  }
  uint32_t func_to_return_idx = maybe_func_to_return_idx.ToChecked();

  TRACE_LAZY(
      "Starting lazy compilation (func %u @%d, js_to_wasm: false, patch "
      "caller: true). \n",
      func_to_return_idx, call_offset);

  // TODO(clemensh): compile all functions in non_compiled_functions in
  // background, wait for func_to_return_idx.
  CompileFunction(isolate, instance, func_to_return_idx);

  Handle<WasmCompiledModule> compiled_module(instance->compiled_module(),
                                             isolate);
  WasmCode* ret =
      compiled_module->GetNativeModule()->GetCode(func_to_return_idx);

  DCHECK_NOT_NULL(ret);
  {
    DisallowHeapAllocation no_gc;
    // Now patch the code object with all functions which are now compiled. This
    // will pick up any other compiled functions, not only {ret}.
    size_t idx = 0;
    size_t patched = 0;
    for (RelocIterator
             it(wasm_caller->instructions(), wasm_caller->reloc_info(),
                wasm_caller->constant_pool(),
                RelocInfo::ModeMask(RelocInfo::WASM_CALL));
         !it.done(); it.next(), ++idx) {
      auto& info = non_compiled_functions[idx];
      if (info.IsNothing()) continue;
      uint32_t lookup = info.ToChecked().func_index;
      const WasmCode* callee_compiled =
          compiled_module->GetNativeModule()->GetCode(lookup);
      if (callee_compiled->kind() != WasmCode::Function) continue;
      it.rinfo()->set_wasm_call_address(
          isolate, callee_compiled->instructions().start());
      ++patched;
    }
    DCHECK_EQ(non_compiled_functions.size(), idx);
    TRACE_LAZY("Patched %zu location(s) in the caller.\n", patched);
  }
  return ret;
}

ModuleCompiler::CodeGenerationSchedule::CodeGenerationSchedule(
    base::RandomNumberGenerator* random_number_generator, size_t max_memory)
    : random_number_generator_(random_number_generator),
      max_memory_(max_memory) {
  DCHECK_NOT_NULL(random_number_generator_);
  DCHECK_GT(max_memory_, 0);
}

void ModuleCompiler::CodeGenerationSchedule::Schedule(
    std::unique_ptr<compiler::WasmCompilationUnit>&& item) {
  size_t cost = item->memory_cost();
  schedule_.push_back(std::move(item));
  allocated_memory_.Increment(cost);
}

bool ModuleCompiler::CodeGenerationSchedule::CanAcceptWork() const {
  return (!throttle_ || allocated_memory_.Value() <= max_memory_);
}

bool ModuleCompiler::CodeGenerationSchedule::ShouldIncreaseWorkload() const {
  // Half the memory is unused again, we can increase the workload again.
  return (!throttle_ || allocated_memory_.Value() <= max_memory_ / 2);
}

std::unique_ptr<compiler::WasmCompilationUnit>
ModuleCompiler::CodeGenerationSchedule::GetNext() {
  DCHECK(!IsEmpty());
  size_t index = GetRandomIndexInSchedule();
  auto ret = std::move(schedule_[index]);
  std::swap(schedule_[schedule_.size() - 1], schedule_[index]);
  schedule_.pop_back();
  allocated_memory_.Decrement(ret->memory_cost());
  return ret;
}

size_t ModuleCompiler::CodeGenerationSchedule::GetRandomIndexInSchedule() {
  double factor = random_number_generator_->NextDouble();
  size_t index = (size_t)(factor * schedule_.size());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, schedule_.size());
  return index;
}

ModuleCompiler::ModuleCompiler(Isolate* isolate, WasmModule* module,
                               Handle<Code> centry_stub,
                               wasm::NativeModule* native_module)
    : isolate_(isolate),
      module_(module),
      async_counters_(isolate->async_counters()),
      executed_units_(
          isolate->random_number_generator(),
          (isolate->heap()->memory_allocator()->code_range()->valid()
               ? isolate->heap()->memory_allocator()->code_range()->size()
               : isolate->heap()->code_space()->Capacity()) /
              2),
      num_background_tasks_(
          Min(static_cast<size_t>(FLAG_wasm_num_compilation_tasks),
              V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads())),
      stopped_compilation_tasks_(num_background_tasks_),
      centry_stub_(centry_stub),
      native_module_(native_module) {}

// The actual runnable task that performs compilations in the background.
void ModuleCompiler::OnBackgroundTaskStopped() {
  base::LockGuard<base::Mutex> guard(&tasks_mutex_);
  ++stopped_compilation_tasks_;
  DCHECK_LE(stopped_compilation_tasks_, num_background_tasks_);
}

// Run by each compilation task. The no_finisher_callback is called
// within the result_mutex_ lock when no finishing task is running,
// i.e. when the finisher_is_running_ flag is not set.
bool ModuleCompiler::FetchAndExecuteCompilationUnit(
    std::function<void()> no_finisher_callback) {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;
  DisallowCodeDependencyChange no_dependency_change;

  std::unique_ptr<compiler::WasmCompilationUnit> unit;
  {
    base::LockGuard<base::Mutex> guard(&compilation_units_mutex_);
    if (compilation_units_.empty()) return false;
    unit = std::move(compilation_units_.back());
    compilation_units_.pop_back();
  }
  unit->ExecuteCompilation();
  {
    base::LockGuard<base::Mutex> guard(&result_mutex_);
    executed_units_.Schedule(std::move(unit));
    if (no_finisher_callback != nullptr && !finisher_is_running_) {
      no_finisher_callback();
      // We set the flag here so that not more than one finisher is started.
      finisher_is_running_ = true;
    }
  }
  return true;
}

size_t ModuleCompiler::InitializeCompilationUnits(
    const std::vector<WasmFunction>& functions,
    const ModuleWireBytes& wire_bytes, compiler::ModuleEnv* module_env) {
  uint32_t start = module_env->module->num_imported_functions +
                   FLAG_skip_compiling_wasm_funcs;
  uint32_t num_funcs = static_cast<uint32_t>(functions.size());
  uint32_t funcs_to_compile = start > num_funcs ? 0 : num_funcs - start;
  CompilationUnitBuilder builder(this);
  for (uint32_t i = start; i < num_funcs; ++i) {
    const WasmFunction* func = &functions[i];
    uint32_t buffer_offset = func->code.offset();
    Vector<const uint8_t> bytes(wire_bytes.start() + func->code.offset(),
                                func->code.end_offset() - func->code.offset());
    WasmName name = wire_bytes.GetName(func);
    DCHECK_IMPLIES(FLAG_wasm_jit_to_native, native_module_ != nullptr);
    builder.AddUnit(module_env, native_module_, func, buffer_offset, bytes,
                    name);
  }
  builder.Commit();
  return funcs_to_compile;
}

void ModuleCompiler::RestartCompilationTasks() {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  std::shared_ptr<v8::TaskRunner> task_runner =
      V8::GetCurrentPlatform()->GetBackgroundTaskRunner(v8_isolate);

  base::LockGuard<base::Mutex> guard(&tasks_mutex_);
  for (; stopped_compilation_tasks_ > 0; --stopped_compilation_tasks_) {
    task_runner->PostTask(base::make_unique<CompilationTask>(this));
  }
}

size_t ModuleCompiler::FinishCompilationUnits(
    std::vector<Handle<Code>>& results, ErrorThrower* thrower) {
  size_t finished = 0;
  while (true) {
    int func_index = -1;
    WasmCodeWrapper result = FinishCompilationUnit(thrower, &func_index);
    if (func_index < 0) break;
    ++finished;
    DCHECK_IMPLIES(result.is_null(), thrower->error());
    if (result.is_null()) break;
    if (result.IsCodeObject()) {
      results[func_index] = result.GetCode();
    }
  }
  bool do_restart;
  {
    base::LockGuard<base::Mutex> guard(&compilation_units_mutex_);
    do_restart = !compilation_units_.empty();
  }
  if (do_restart) RestartCompilationTasks();
  return finished;
}

void ModuleCompiler::SetFinisherIsRunning(bool value) {
  base::LockGuard<base::Mutex> guard(&result_mutex_);
  finisher_is_running_ = value;
}

WasmCodeWrapper ModuleCompiler::FinishCompilationUnit(ErrorThrower* thrower,
                                                      int* func_index) {
  std::unique_ptr<compiler::WasmCompilationUnit> unit;
  {
    base::LockGuard<base::Mutex> guard(&result_mutex_);
    if (executed_units_.IsEmpty()) return {};
    unit = executed_units_.GetNext();
  }
  *func_index = unit->func_index();
  return unit->FinishCompilation(thrower);
}

void ModuleCompiler::CompileInParallel(const ModuleWireBytes& wire_bytes,
                                       compiler::ModuleEnv* module_env,
                                       std::vector<Handle<Code>>& results,
                                       ErrorThrower* thrower) {
  const WasmModule* module = module_env->module;
  // Data structures for the parallel compilation.

  //-----------------------------------------------------------------------
  // For parallel compilation:
  // 1) The main thread allocates a compilation unit for each wasm function
  //    and stores them in the vector {compilation_units}.
  // 2) The main thread spawns {CompilationTask} instances which run on
  //    the background threads.
  // 3.a) The background threads and the main thread pick one compilation
  //      unit at a time and execute the parallel phase of the compilation
  //      unit. After finishing the execution of the parallel phase, the
  //      result is enqueued in {executed_units}.
  // 3.b) If {executed_units} contains a compilation unit, the main thread
  //      dequeues it and finishes the compilation.
  // 4) After the parallel phase of all compilation units has started, the
  //    main thread waits for all {CompilationTask} instances to finish.
  // 5) The main thread finishes the compilation.

  // Turn on the {CanonicalHandleScope} so that the background threads can
  // use the node cache.
  CanonicalHandleScope canonical(isolate_);

  // 1) The main thread allocates a compilation unit for each wasm function
  //    and stores them in the vector {compilation_units}.
  InitializeCompilationUnits(module->functions, wire_bytes, module_env);
  executed_units_.EnableThrottling();

  // 2) The main thread spawns {CompilationTask} instances which run on
  //    the background threads.
  RestartCompilationTasks();

  // 3.a) The background threads and the main thread pick one compilation
  //      unit at a time and execute the parallel phase of the compilation
  //      unit. After finishing the execution of the parallel phase, the
  //      result is enqueued in {executed_units}.
  //      The foreground task bypasses waiting on memory threshold, because
  //      its results will immediately be converted to code (below).
  while (FetchAndExecuteCompilationUnit()) {
    // 3.b) If {executed_units} contains a compilation unit, the main thread
    //      dequeues it and finishes the compilation unit. Compilation units
    //      are finished concurrently to the background threads to save
    //      memory.
    FinishCompilationUnits(results, thrower);
  }
  // 4) After the parallel phase of all compilation units has started, the
  //    main thread waits for all {CompilationTask} instances to finish - which
  //    happens once they all realize there's no next work item to process.
  background_task_manager_.CancelAndWait();
  // Finish all compilation units which have been executed while we waited.
  FinishCompilationUnits(results, thrower);
}

void ModuleCompiler::CompileSequentially(const ModuleWireBytes& wire_bytes,
                                         compiler::ModuleEnv* module_env,
                                         std::vector<Handle<Code>>& results,
                                         ErrorThrower* thrower) {
  DCHECK(!thrower->error());

  const WasmModule* module = module_env->module;
  for (uint32_t i = FLAG_skip_compiling_wasm_funcs;
       i < module->functions.size(); ++i) {
    const WasmFunction& func = module->functions[i];
    if (func.imported) continue;  // Imports are compiled at instantiation time.

    // Compile the function.
    WasmCodeWrapper code = compiler::WasmCompilationUnit::CompileWasmFunction(
        native_module_, thrower, isolate_, wire_bytes, module_env, &func);
    if (code.is_null()) {
      TruncatedUserString<> name(wire_bytes.GetName(&func));
      thrower->CompileError("Compilation of #%d:%.*s failed.", i, name.length(),
                            name.start());
      break;
    }
    if (code.IsCodeObject()) {
      results[i] = code.GetCode();
    }
  }
}

void ModuleCompiler::ValidateSequentially(const ModuleWireBytes& wire_bytes,
                                          compiler::ModuleEnv* module_env,
                                          ErrorThrower* thrower) {
  DCHECK(!thrower->error());

  const WasmModule* module = module_env->module;
  for (uint32_t i = 0; i < module->functions.size(); ++i) {
    const WasmFunction& func = module->functions[i];
    if (func.imported) continue;

    const byte* base = wire_bytes.start();
    FunctionBody body{func.sig, func.code.offset(), base + func.code.offset(),
                      base + func.code.end_offset()};
    DecodeResult result = VerifyWasmCodeWithStats(
        isolate_->allocator(), module, body, module->is_wasm(), counters());
    if (result.failed()) {
      TruncatedUserString<> name(wire_bytes.GetName(&func));
      thrower->CompileError("Compiling function #%d:%.*s failed: %s @+%u", i,
                            name.length(), name.start(),
                            result.error_msg().c_str(), result.error_offset());
      break;
    }
  }
}

// static
MaybeHandle<WasmModuleObject> ModuleCompiler::CompileToModuleObject(
    Isolate* isolate, ErrorThrower* thrower, std::unique_ptr<WasmModule> module,
    const ModuleWireBytes& wire_bytes, Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {
  Handle<Code> centry_stub = CEntryStub(isolate, 1).GetCode();
  // TODO(mtrofin): the wasm::NativeModule parameter to the ModuleCompiler
  // constructor is null here, and initialized in CompileToModuleObjectInternal.
  // This is a point-in-time, until we remove the FLAG_wasm_jit_to_native flag,
  // and stop needing a FixedArray for code for the non-native case. Otherwise,
  // we end up moving quite a bit of initialization logic here that is also
  // needed in CompileToModuleObjectInternal, complicating the change.
  ModuleCompiler compiler(isolate, module.get(), centry_stub, nullptr);
  return compiler.CompileToModuleObjectInternal(thrower, std::move(module),
                                                wire_bytes, asm_js_script,
                                                asm_js_offset_table_bytes);
}

namespace {
bool compile_lazy(const WasmModule* module) {
  return FLAG_wasm_lazy_compilation ||
         (FLAG_asm_wasm_lazy_compilation && module->is_asm_js());
}

void FlushICache(Isolate* isolate, const wasm::NativeModule* native_module) {
  for (uint32_t i = 0, e = native_module->FunctionCount(); i < e; ++i) {
    const wasm::WasmCode* code = native_module->GetCode(i);
    if (code == nullptr) continue;
    Assembler::FlushICache(isolate, code->instructions().start(),
                           code->instructions().size());
  }
}

void FlushICache(Isolate* isolate, Handle<FixedArray> functions) {
  for (int i = 0, e = functions->length(); i < e; ++i) {
    if (!functions->get(i)->IsCode()) continue;
    Code* code = Code::cast(functions->get(i));
    Assembler::FlushICache(isolate, code->instruction_start(),
                           code->instruction_size());
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

void RecordStats(WasmCodeWrapper wrapper, Counters* counters) {
  if (wrapper.IsCodeObject()) {
    RecordStats(*wrapper.GetCode(), counters);
  } else {
    RecordStats(wrapper.GetWasmCode(), counters);
  }
}

void RecordStats(Handle<FixedArray> functions, Counters* counters) {
  DisallowHeapAllocation no_gc;
  for (int i = 0; i < functions->length(); ++i) {
    Object* val = functions->get(i);
    if (val->IsCode()) RecordStats(Code::cast(val), counters);
  }
}

void RecordStats(const wasm::NativeModule* native_module, Counters* counters) {
  for (uint32_t i = 0, e = native_module->FunctionCount(); i < e; ++i) {
    const wasm::WasmCode* code = native_module->GetCode(i);
    if (code != nullptr) RecordStats(code, counters);
  }
}

// Ensure that the code object in <code_table> at offset <func_index> has
// deoptimization data attached. This is needed for lazy compile stubs which are
// called from JS_TO_WASM functions or via exported function tables. The deopt
// data is used to determine which function this lazy compile stub belongs to.
// TODO(mtrofin): remove the instance and code_table members once we remove the
// FLAG_wasm_jit_to_native
WasmCodeWrapper EnsureExportedLazyDeoptData(Isolate* isolate,
                                            Handle<WasmInstanceObject> instance,
                                            Handle<FixedArray> code_table,
                                            wasm::NativeModule* native_module,
                                            uint32_t func_index) {
  if (!FLAG_wasm_jit_to_native) {
    Handle<Code> code(Code::cast(code_table->get(func_index)), isolate);
    if (code->builtin_index() != Builtins::kWasmCompileLazy) {
      // No special deopt data needed for compiled functions, and imported
      // functions, which map to Illegal at this point (they get compiled at
      // instantiation time).
      DCHECK(code->kind() == Code::WASM_FUNCTION ||
             code->kind() == Code::WASM_TO_JS_FUNCTION ||
             code->kind() == Code::WASM_TO_WASM_FUNCTION ||
             code->builtin_index() == Builtins::kIllegal);
      return WasmCodeWrapper(code);
    }

    // deopt_data:
    //   #0: weak instance
    //   #1: func_index
    // might be extended later for table exports (see
    // EnsureTableExportLazyDeoptData).
    Handle<FixedArray> deopt_data(code->deoptimization_data());
    DCHECK_EQ(0, deopt_data->length() % 2);
    if (deopt_data->length() == 0) {
      code = isolate->factory()->CopyCode(code);
      code_table->set(func_index, *code);
      AttachWasmFunctionInfo(isolate, code, instance, func_index);
    }
#ifdef DEBUG
    auto func_info = GetWasmFunctionInfo(isolate, code);
    DCHECK_IMPLIES(!instance.is_null(),
                   *func_info.instance.ToHandleChecked() == *instance);
    DCHECK_EQ(func_index, func_info.func_index);
#endif
    return WasmCodeWrapper(code);
  } else {
    wasm::WasmCode* code = native_module->GetCode(func_index);
    // {code} will be nullptr when exporting imports.
    if (code == nullptr || code->kind() != wasm::WasmCode::LazyStub ||
        !code->IsAnonymous()) {
      return WasmCodeWrapper(code);
    }
    // Clone the lazy builtin into the native module.
    return WasmCodeWrapper(native_module->CloneLazyBuiltinInto(func_index));
  }
}

// Ensure that the code object in <code_table> at offset <func_index> has
// deoptimization data attached. This is needed for lazy compile stubs which are
// called from JS_TO_WASM functions or via exported function tables. The deopt
// data is used to determine which function this lazy compile stub belongs to.
// TODO(mtrofin): remove the instance and code_table members once we remove the
// FLAG_wasm_jit_to_native
WasmCodeWrapper EnsureTableExportLazyDeoptData(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    Handle<FixedArray> code_table, wasm::NativeModule* native_module,
    uint32_t func_index, Handle<FixedArray> export_table, int export_index,
    std::unordered_map<uint32_t, uint32_t>* table_export_count) {
  if (!FLAG_wasm_jit_to_native) {
    Handle<Code> code =
        EnsureExportedLazyDeoptData(isolate, instance, code_table,
                                    native_module, func_index)
            .GetCode();
    if (code->builtin_index() != Builtins::kWasmCompileLazy)
      return WasmCodeWrapper(code);

    // TODO(6792): No longer needed once WebAssembly code is off heap.
    CodeSpaceMemoryModificationScope modification_scope(isolate->heap());

    // deopt_data:
    // #0: weak instance
    // #1: func_index
    // [#2: export table
    //  #3: export table index]
    // [#4: export table
    //  #5: export table index]
    // ...
    // table_export_count counts down and determines the index for the new
    // export table entry.
    auto table_export_entry = table_export_count->find(func_index);
    DCHECK(table_export_entry != table_export_count->end());
    DCHECK_LT(0, table_export_entry->second);
    uint32_t this_idx = 2 * table_export_entry->second;
    --table_export_entry->second;
    Handle<FixedArray> deopt_data(code->deoptimization_data());
    DCHECK_EQ(0, deopt_data->length() % 2);
    if (deopt_data->length() == 2) {
      // Then only the "header" (#0 and #1) exists. Extend for the export table
      // entries (make space for this_idx + 2 elements).
      deopt_data = isolate->factory()->CopyFixedArrayAndGrow(deopt_data,
                                                             this_idx, TENURED);
      code->set_deoptimization_data(*deopt_data);
    }
    DCHECK_LE(this_idx + 2, deopt_data->length());
    DCHECK(deopt_data->get(this_idx)->IsUndefined(isolate));
    DCHECK(deopt_data->get(this_idx + 1)->IsUndefined(isolate));
    deopt_data->set(this_idx, *export_table);
    deopt_data->set(this_idx + 1, Smi::FromInt(export_index));
    return WasmCodeWrapper(code);
  } else {
    const wasm::WasmCode* code =
        EnsureExportedLazyDeoptData(isolate, instance, code_table,
                                    native_module, func_index)
            .GetWasmCode();
    if (code == nullptr || code->kind() != wasm::WasmCode::LazyStub)
      return WasmCodeWrapper(code);

    // deopt_data:
    // [#0: export table
    //  #1: export table index]
    // [#2: export table
    //  #3: export table index]
    // ...
    // table_export_count counts down and determines the index for the new
    // export table entry.
    auto table_export_entry = table_export_count->find(func_index);
    DCHECK(table_export_entry != table_export_count->end());
    DCHECK_LT(0, table_export_entry->second);
    --table_export_entry->second;
    uint32_t this_idx = 2 * table_export_entry->second;
    int int_func_index = static_cast<int>(func_index);
    Object* deopt_entry =
        native_module->compiled_module()->lazy_compile_data()->get(
            int_func_index);
    FixedArray* deopt_data = nullptr;
    if (!deopt_entry->IsFixedArray()) {
      // we count indices down, so we enter here first for the
      // largest index.
      deopt_data = *isolate->factory()->NewFixedArray(this_idx + 2, TENURED);
      native_module->compiled_module()->lazy_compile_data()->set(int_func_index,
                                                                 deopt_data);
    } else {
      deopt_data = FixedArray::cast(deopt_entry);
      DCHECK_LE(this_idx + 2, deopt_data->length());
    }
    DCHECK(deopt_data->get(this_idx)->IsUndefined(isolate));
    DCHECK(deopt_data->get(this_idx + 1)->IsUndefined(isolate));
    deopt_data->set(this_idx, *export_table);
    deopt_data->set(this_idx + 1, Smi::FromInt(export_index));
    return WasmCodeWrapper(code);
  }
}

bool in_bounds(uint32_t offset, uint32_t size, uint32_t upper) {
  return offset + size <= upper && offset + size >= offset;
}

using WasmInstanceMap =
    IdentityMap<Handle<WasmInstanceObject>, FreeStoreAllocationPolicy>;

WasmCodeWrapper MakeWasmToWasmWrapper(
    Isolate* isolate, Handle<WasmExportedFunction> imported_function,
    FunctionSig* expected_sig, FunctionSig** sig,
    WasmInstanceMap* imported_instances, Handle<WasmInstanceObject> instance,
    uint32_t index) {
  // TODO(wasm): cache WASM-to-WASM wrappers by signature and clone+patch.
  Handle<WasmInstanceObject> imported_instance(imported_function->instance(),
                                               isolate);
  imported_instances->Set(imported_instance, imported_instance);
  WasmContext* new_wasm_context = imported_instance->wasm_context()->get();
  Address new_wasm_context_address =
      reinterpret_cast<Address>(new_wasm_context);
  *sig = imported_instance->module()
             ->functions[imported_function->function_index()]
             .sig;
  if (expected_sig && !expected_sig->Equals(*sig)) return {};

  if (!FLAG_wasm_jit_to_native) {
    Handle<Code> wrapper_code = compiler::CompileWasmToWasmWrapper(
        isolate, imported_function->GetWasmCode(), *sig,
        new_wasm_context_address);
    // Set the deoptimization data for the WasmToWasm wrapper. This is
    // needed by the interpreter to find the imported instance for
    // a cross-instance call.
    AttachWasmFunctionInfo(isolate, wrapper_code, imported_instance,
                           imported_function->function_index());
    return WasmCodeWrapper(wrapper_code);
  } else {
    Handle<Code> code = compiler::CompileWasmToWasmWrapper(
        isolate, imported_function->GetWasmCode(), *sig,
        new_wasm_context_address);
    return WasmCodeWrapper(
        instance->compiled_module()->GetNativeModule()->AddCodeCopy(
            code, wasm::WasmCode::WasmToWasmWrapper, index));
  }
}

WasmCodeWrapper UnwrapExportOrCompileImportWrapper(
    Isolate* isolate, FunctionSig* sig, Handle<JSReceiver> target,
    uint32_t import_index, ModuleOrigin origin,
    WasmInstanceMap* imported_instances, Handle<FixedArray> js_imports_table,
    Handle<WasmInstanceObject> instance) {
  if (WasmExportedFunction::IsWasmExportedFunction(*target)) {
    FunctionSig* unused = nullptr;
    return MakeWasmToWasmWrapper(
        isolate, Handle<WasmExportedFunction>::cast(target), sig, &unused,
        imported_instances, instance, import_index);
  }
  // No wasm function or being debugged. Compile a new wrapper for the new
  // signature.
  if (FLAG_wasm_jit_to_native) {
    Handle<Code> temp_code = compiler::CompileWasmToJSWrapper(
        isolate, target, sig, import_index, origin, js_imports_table);
    return WasmCodeWrapper(
        instance->compiled_module()->GetNativeModule()->AddCodeCopy(
            temp_code, wasm::WasmCode::WasmToJsWrapper, import_index));
  } else {
    return WasmCodeWrapper(compiler::CompileWasmToJSWrapper(
        isolate, target, sig, import_index, origin, js_imports_table));
  }
}

double MonotonicallyIncreasingTimeInMs() {
  return V8::GetCurrentPlatform()->MonotonicallyIncreasingTime() *
         base::Time::kMillisecondsPerSecond;
}

void FunctionTableFinalizer(const v8::WeakCallbackInfo<void>& data) {
  GlobalHandles::Destroy(reinterpret_cast<Object**>(
      reinterpret_cast<JSObject**>(data.GetParameter())));
}

std::unique_ptr<compiler::ModuleEnv> CreateDefaultModuleEnv(
    Isolate* isolate, WasmModule* module, Handle<Code> illegal_builtin) {
  std::vector<GlobalHandleAddress> function_tables;
  std::vector<GlobalHandleAddress> signature_tables;

  for (size_t i = 0; i < module->function_tables.size(); i++) {
    Handle<Object> func_table =
        isolate->global_handles()->Create(isolate->heap()->undefined_value());
    Handle<Object> sig_table =
        isolate->global_handles()->Create(isolate->heap()->undefined_value());
    GlobalHandles::MakeWeak(func_table.location(), func_table.location(),
                            &FunctionTableFinalizer,
                            v8::WeakCallbackType::kFinalizer);
    GlobalHandles::MakeWeak(sig_table.location(), sig_table.location(),
                            &FunctionTableFinalizer,
                            v8::WeakCallbackType::kFinalizer);
    function_tables.push_back(func_table.address());
    signature_tables.push_back(sig_table.address());
  }

  std::vector<Handle<Code>> empty_code;

  compiler::ModuleEnv result = {
      module,            // --
      function_tables,   // --
      signature_tables,  // --
      empty_code,        // --
      illegal_builtin    // --
  };
  return std::unique_ptr<compiler::ModuleEnv>(new compiler::ModuleEnv(result));
}

// TODO(mtrofin): remove code_table when we don't need FLAG_wasm_jit_to_native
Handle<WasmCompiledModule> NewCompiledModule(Isolate* isolate,
                                             WasmModule* module,
                                             Handle<FixedArray> code_table,
                                             Handle<FixedArray> export_wrappers,
                                             compiler::ModuleEnv* env) {
  Handle<WasmCompiledModule> compiled_module =
      WasmCompiledModule::New(isolate, module, code_table, export_wrappers,
                              env->function_tables, env->signature_tables);
  return compiled_module;
}

template <typename T>
void ReopenHandles(Isolate* isolate, const std::vector<Handle<T>>& vec) {
  auto& mut = const_cast<std::vector<Handle<T>>&>(vec);
  for (size_t i = 0; i < mut.size(); i++) {
    mut[i] = Handle<T>(*mut[i], isolate);
  }
}

}  // namespace

MaybeHandle<WasmModuleObject> ModuleCompiler::CompileToModuleObjectInternal(
    ErrorThrower* thrower, std::unique_ptr<WasmModule> module,
    const ModuleWireBytes& wire_bytes, Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {
  TimedHistogramScope wasm_compile_module_time_scope(
      module_->is_wasm() ? counters()->wasm_compile_wasm_module_time()
                         : counters()->wasm_compile_asm_module_time());
  // TODO(6792): No longer needed once WebAssembly code is off heap. Use
  // base::Optional to be able to close the scope before notifying the debugger.
  base::Optional<CodeSpaceMemoryModificationScope> modification_scope(
      base::in_place_t(), isolate_->heap());
  // The {module} parameter is passed in to transfer ownership of the WasmModule
  // to this function. The WasmModule itself existed already as an instance
  // variable of the ModuleCompiler. We check here that the parameter and the
  // instance variable actually point to the same object.
  DCHECK_EQ(module.get(), module_);
  // Check whether lazy compilation is enabled for this module.
  bool lazy_compile = compile_lazy(module_);

  Factory* factory = isolate_->factory();
  // Create heap objects for script, module bytes and asm.js offset table to
  // be stored in the shared module data.
  Handle<Script> script;
  Handle<ByteArray> asm_js_offset_table;
  if (asm_js_script.is_null()) {
    script = CreateWasmScript(isolate_, wire_bytes);
  } else {
    script = asm_js_script;
    asm_js_offset_table =
        isolate_->factory()->NewByteArray(asm_js_offset_table_bytes.length());
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

  // The {module_wrapper} will take ownership of the {WasmModule} object,
  // and it will be destroyed when the GC reclaims the wrapper object.
  Handle<WasmModuleWrapper> module_wrapper =
      WasmModuleWrapper::From(isolate_, module.release());

  // Create the shared module data.
  // TODO(clemensh): For the same module (same bytes / same hash), we should
  // only have one WasmSharedModuleData. Otherwise, we might only set
  // breakpoints on a (potentially empty) subset of the instances.

  Handle<WasmSharedModuleData> shared = WasmSharedModuleData::New(
      isolate_, module_wrapper, Handle<SeqOneByteString>::cast(module_bytes),
      script, asm_js_offset_table);
  if (lazy_compile) WasmSharedModuleData::PrepareForLazyCompilation(shared);

  Handle<Code> init_builtin = lazy_compile
                                  ? BUILTIN_CODE(isolate_, WasmCompileLazy)
                                  : BUILTIN_CODE(isolate_, Illegal);

  // TODO(mtrofin): remove code_table and code_table_size when we don't
  // need FLAG_wasm_jit_to_native anymore. Keep export_wrappers.
  int code_table_size = static_cast<int>(module_->functions.size());
  int export_wrappers_size = static_cast<int>(module_->num_exported_functions);
  Handle<FixedArray> code_table =
      factory->NewFixedArray(static_cast<int>(code_table_size), TENURED);
  Handle<FixedArray> export_wrappers =
      factory->NewFixedArray(static_cast<int>(export_wrappers_size), TENURED);
  // Initialize the code table.
  for (int i = 0, e = code_table->length(); i < e; ++i) {
    code_table->set(i, *init_builtin);
  }

  for (int i = 0, e = export_wrappers->length(); i < e; ++i) {
    export_wrappers->set(i, *init_builtin);
  }
  auto env = CreateDefaultModuleEnv(isolate_, module_, init_builtin);

  // Create the compiled module object and populate with compiled functions
  // and information needed at instantiation time. This object needs to be
  // serializable. Instantiation may occur off a deserialized version of this
  // object.
  Handle<WasmCompiledModule> compiled_module = NewCompiledModule(
      isolate_, shared->module(), code_table, export_wrappers, env.get());
  native_module_ = compiled_module->GetNativeModule();
  compiled_module->OnWasmModuleDecodingComplete(shared);
  if (lazy_compile && FLAG_wasm_jit_to_native) {
    compiled_module->set_lazy_compile_data(isolate_->factory()->NewFixedArray(
        static_cast<int>(module_->functions.size()), TENURED));
  }

  if (!lazy_compile) {
    size_t funcs_to_compile =
        module_->functions.size() - module_->num_imported_functions;
    bool compile_parallel =
        !FLAG_trace_wasm_decoder && FLAG_wasm_num_compilation_tasks > 0 &&
        funcs_to_compile > 1 &&
        V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads() > 0;
    // Avoid a race condition by collecting results into a second vector.
    std::vector<Handle<Code>> results(
        FLAG_wasm_jit_to_native ? 0 : env->module->functions.size());

    if (compile_parallel) {
      CompileInParallel(wire_bytes, env.get(), results, thrower);
    } else {
      CompileSequentially(wire_bytes, env.get(), results, thrower);
    }
    if (thrower->error()) return {};

    if (!FLAG_wasm_jit_to_native) {
      // At this point, compilation has completed. Update the code table.
      for (size_t i =
               module_->num_imported_functions + FLAG_skip_compiling_wasm_funcs;
           i < results.size(); ++i) {
        Code* code = *results[i];
        code_table->set(static_cast<int>(i), code);
        RecordStats(code, counters());
      }
    } else {
      RecordStats(native_module_, counters());
    }
  } else {
    if (module_->is_wasm()) {
      // Validate wasm modules for lazy compilation. Don't validate asm.js
      // modules, they are valid by construction (otherwise a CHECK will fail
      // during lazy compilation).
      // TODO(clemensh): According to the spec, we can actually skip validation
      // at module creation time, and return a function that always traps at
      // (lazy) compilation time.
      ValidateSequentially(wire_bytes, env.get(), thrower);
    }
    if (FLAG_wasm_jit_to_native) {
      native_module_->SetLazyBuiltin(BUILTIN_CODE(isolate_, WasmCompileLazy));
    }
  }
  if (thrower->error()) return {};

  // Compile JS->wasm wrappers for exported functions.
  CompileJsToWasmWrappers(isolate_, compiled_module, counters());

  Handle<WasmModuleObject> result =
      WasmModuleObject::New(isolate_, compiled_module);

  // If we created a wasm script, finish it now and make it public to the
  // debugger.
  if (asm_js_script.is_null()) {
    // Close the CodeSpaceMemoryModificationScope before calling into the
    // debugger.
    modification_scope.reset();
    script->set_wasm_compiled_module(*compiled_module);
    isolate_->debug()->OnAfterCompile(script);
  }

  return result;
}

InstanceBuilder::InstanceBuilder(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> ffi,
    MaybeHandle<JSArrayBuffer> memory,
    WeakCallbackInfo<void>::Callback instance_finalizer_callback)
    : isolate_(isolate),
      module_(module_object->compiled_module()->module()),
      async_counters_(isolate->async_counters()),
      thrower_(thrower),
      module_object_(module_object),
      ffi_(ffi),
      memory_(memory),
      instance_finalizer_callback_(instance_finalizer_callback) {
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
  // Use base::Optional to be able to close the scope before executing the start
  // function.
  base::Optional<CodeSpaceMemoryModificationScope> modification_scope(
      base::in_place_t(), isolate_->heap());
  // From here on, we expect the build pipeline to run without exiting to JS.
  // Exception is when we run the startup function.
  DisallowJavascriptExecution no_js(isolate_);
  // Record build time into correct bucket, then build instance.
  TimedHistogramScope wasm_instantiate_module_time_scope(
      module_->is_wasm() ? counters()->wasm_instantiate_wasm_module_time()
                         : counters()->wasm_instantiate_asm_module_time());
  Factory* factory = isolate_->factory();

  //--------------------------------------------------------------------------
  // Reuse the compiled module (if no owner), otherwise clone.
  //--------------------------------------------------------------------------
  // TODO(mtrofin): remove code_table and old_code_table
  // when FLAG_wasm_jit_to_native is not needed
  Handle<FixedArray> code_table;
  Handle<FixedArray> wrapper_table;
  // We keep around a copy of the old code table, because we'll be replacing
  // imports for the new instance, and then we need the old imports to be
  // able to relocate.
  Handle<FixedArray> old_code_table;
  MaybeHandle<WasmInstanceObject> owner;
  // native_module is the one we're building now, old_module
  // is the one we clone from. They point to the same place if
  // we don't need to clone.
  wasm::NativeModule* native_module = nullptr;
  wasm::NativeModule* old_module = nullptr;

  TRACE("Starting new module instantiation\n");
  {
    // Root the owner, if any, before doing any allocations, which
    // may trigger GC.
    // Both owner and original template need to be in sync. Even
    // after we lose the original template handle, the code
    // objects we copied from it have data relative to the
    // instance - such as globals addresses.
    Handle<WasmCompiledModule> original;
    {
      DisallowHeapAllocation no_gc;
      original = handle(module_object_->compiled_module());
      if (original->has_weak_owning_instance()) {
        owner = handle(WasmInstanceObject::cast(
            original->weak_owning_instance()->value()));
      }
    }
    DCHECK(!original.is_null());
    if (original->has_weak_owning_instance()) {
      // Clone, but don't insert yet the clone in the instances chain.
      // We do that last. Since we are holding on to the owner instance,
      // the owner + original state used for cloning and patching
      // won't be mutated by possible finalizer runs.
      DCHECK(!owner.is_null());
      if (FLAG_wasm_jit_to_native) {
        TRACE("Cloning from %zu\n", original->GetNativeModule()->instance_id);
        compiled_module_ = WasmCompiledModule::Clone(isolate_, original);
        native_module = compiled_module_->GetNativeModule();
        wrapper_table = compiled_module_->export_wrappers();
      } else {
        TRACE("Cloning from %d\n", original->instance_id());
        old_code_table = original->code_table();
        compiled_module_ = WasmCompiledModule::Clone(isolate_, original);
        code_table = compiled_module_->code_table();
        wrapper_table = compiled_module_->export_wrappers();
        // Avoid creating too many handles in the outer scope.
        HandleScope scope(isolate_);

        // Clone the code for wasm functions and exports.
        for (int i = 0; i < code_table->length(); ++i) {
          Handle<Code> orig_code(Code::cast(code_table->get(i)), isolate_);
          switch (orig_code->kind()) {
            case Code::WASM_TO_JS_FUNCTION:
            case Code::WASM_TO_WASM_FUNCTION:
              // Imports will be overwritten with newly compiled wrappers.
              break;
            case Code::BUILTIN:
              DCHECK_EQ(Builtins::kWasmCompileLazy, orig_code->builtin_index());
              // If this code object has deoptimization data, then we need a
              // unique copy to attach updated deoptimization data.
              if (orig_code->deoptimization_data()->length() > 0) {
                Handle<Code> code = factory->CopyCode(orig_code);
                AttachWasmFunctionInfo(isolate_, code,
                                       Handle<WasmInstanceObject>(), i);
                code_table->set(i, *code);
              }
              break;
            case Code::WASM_FUNCTION: {
              Handle<Code> code = factory->CopyCode(orig_code);
              AttachWasmFunctionInfo(isolate_, code,
                                     Handle<WasmInstanceObject>(), i);
              code_table->set(i, *code);
              break;
            }
            default:
              UNREACHABLE();
          }
        }
      }
      for (int i = 0; i < wrapper_table->length(); ++i) {
        Handle<Code> orig_code(Code::cast(wrapper_table->get(i)), isolate_);
        DCHECK_EQ(orig_code->kind(), Code::JS_TO_WASM_FUNCTION);
        Handle<Code> code = factory->CopyCode(orig_code);
        wrapper_table->set(i, *code);
      }
      if (FLAG_wasm_jit_to_native) {
        RecordStats(native_module, counters());
      } else {
        RecordStats(code_table, counters());
      }
      RecordStats(wrapper_table, counters());
    } else {
      // There was no owner, so we can reuse the original.
      compiled_module_ = original;
      wrapper_table = compiled_module_->export_wrappers();
      if (FLAG_wasm_jit_to_native) {
        old_module = compiled_module_->GetNativeModule();
        native_module = old_module;
        TRACE("Reusing existing instance %zu\n",
              compiled_module_->GetNativeModule()->instance_id);
      } else {
        old_code_table =
            factory->CopyFixedArray(compiled_module_->code_table());
        code_table = compiled_module_->code_table();
        TRACE("Reusing existing instance %d\n",
              compiled_module_->instance_id());
      }
    }
    compiled_module_->set_native_context(isolate_->native_context());
  }

  //--------------------------------------------------------------------------
  // Create the WebAssembly.Instance object.
  //--------------------------------------------------------------------------
  Zone instantiation_zone(isolate_->allocator(), ZONE_NAME);
  CodeSpecialization code_specialization(isolate_, &instantiation_zone);
  Handle<WasmInstanceObject> instance =
      WasmInstanceObject::New(isolate_, compiled_module_);

  //--------------------------------------------------------------------------
  // Set up the globals for the new instance.
  //--------------------------------------------------------------------------
  WasmContext* wasm_context = instance->wasm_context()->get();
  MaybeHandle<JSArrayBuffer> old_globals;
  uint32_t globals_size = module_->globals_size;
  if (globals_size > 0) {
    const bool enable_guard_regions = false;
    Handle<JSArrayBuffer> global_buffer =
        NewArrayBuffer(isolate_, globals_size, enable_guard_regions);
    globals_ = global_buffer;
    if (globals_.is_null()) {
      thrower_->RangeError("Out of memory: wasm globals");
      return {};
    }
    wasm_context->globals_start =
        reinterpret_cast<byte*>(global_buffer->backing_store());
    instance->set_globals_buffer(*global_buffer);
  }

  //--------------------------------------------------------------------------
  // Reserve the metadata for indirect function tables.
  //--------------------------------------------------------------------------
  int function_table_count = static_cast<int>(module_->function_tables.size());
  table_instances_.reserve(module_->function_tables.size());
  for (int index = 0; index < function_table_count; ++index) {
    table_instances_.push_back(
        {Handle<WasmTableObject>::null(), Handle<FixedArray>::null(),
         Handle<FixedArray>::null(), Handle<FixedArray>::null()});
  }

  //--------------------------------------------------------------------------
  // Process the imports for the module.
  //--------------------------------------------------------------------------
  int num_imported_functions = ProcessImports(code_table, instance);
  if (num_imported_functions < 0) return {};

  //--------------------------------------------------------------------------
  // Process the initialization for the module's globals.
  //--------------------------------------------------------------------------
  InitGlobals();

  //--------------------------------------------------------------------------
  // Initialize the indirect tables.
  //--------------------------------------------------------------------------
  if (function_table_count > 0) {
    InitializeTables(instance, &code_specialization);
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

    DCHECK_IMPLIES(trap_handler::UseTrapHandler(),
                   module_->is_asm_js() || memory->has_guard_region());
  } else if (initial_pages > 0) {
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
      // Double-check the {memory} array buffer matches the context.
      Handle<JSArrayBuffer> memory = memory_.ToHandleChecked();
      uint32_t mem_size = 0;
      CHECK(memory->byte_length()->ToUint32(&mem_size));
      CHECK_EQ(wasm_context->mem_size, mem_size);
      CHECK_EQ(wasm_context->mem_start, memory->backing_store());
    }
  }

  //--------------------------------------------------------------------------
  // Check that indirect function table segments are within bounds.
  //--------------------------------------------------------------------------
  for (WasmTableInit& table_init : module_->table_inits) {
    DCHECK(table_init.table_index < table_instances_.size());
    uint32_t base = EvalUint32InitExpr(table_init.offset);
    uint32_t table_size =
        table_instances_[table_init.table_index].function_table->length();
    if (!in_bounds(base, static_cast<uint32_t>(table_init.entries.size()),
                   table_size)) {
      thrower_->LinkError("table initializer is out of bounds");
      return {};
    }
  }

  //--------------------------------------------------------------------------
  // Check that memory segments are within bounds.
  //--------------------------------------------------------------------------
  for (WasmDataSegment& seg : module_->data_segments) {
    uint32_t base = EvalUint32InitExpr(seg.dest_addr);
    if (!in_bounds(base, seg.source.length(), wasm_context->mem_size)) {
      thrower_->LinkError("data segment is out of bounds");
      return {};
    }
  }

  // Set the WasmContext address in wrappers.
  // TODO(wasm): the wasm context should only appear as a constant in wrappers;
  //             this code specialization is applied to the whole instance.
  Address wasm_context_address = reinterpret_cast<Address>(wasm_context);
  code_specialization.RelocateWasmContextReferences(wasm_context_address);
  js_to_wasm_cache_.SetContextAddress(wasm_context_address);

  if (!FLAG_wasm_jit_to_native) {
    //--------------------------------------------------------------------------
    // Set up the runtime support for the new instance.
    //--------------------------------------------------------------------------
    Handle<WeakCell> weak_link = factory->NewWeakCell(instance);

    for (int i = num_imported_functions + FLAG_skip_compiling_wasm_funcs,
             num_functions = static_cast<int>(module_->functions.size());
         i < num_functions; ++i) {
      Handle<Code> code = handle(Code::cast(code_table->get(i)), isolate_);
      if (code->kind() == Code::WASM_FUNCTION) {
        AttachWasmFunctionInfo(isolate_, code, weak_link, i);
        continue;
      }
      DCHECK_EQ(Builtins::kWasmCompileLazy, code->builtin_index());
      int deopt_len = code->deoptimization_data()->length();
      if (deopt_len == 0) continue;
      DCHECK_LE(2, deopt_len);
      DCHECK_EQ(i, Smi::ToInt(code->deoptimization_data()->get(1)));
      code->deoptimization_data()->set(0, *weak_link);
      // Entries [2, deopt_len) encode information about table exports of this
      // function. This is rebuilt in {LoadTableSegments}, so reset it here.
      for (int i = 2; i < deopt_len; ++i) {
        code->deoptimization_data()->set_undefined(isolate_, i);
      }
    }
  }

  //--------------------------------------------------------------------------
  // Set up the exports object for the new instance.
  //--------------------------------------------------------------------------
  ProcessExports(instance, compiled_module_);
  if (thrower_->error()) return {};

  //--------------------------------------------------------------------------
  // Initialize the indirect function tables.
  //--------------------------------------------------------------------------
  if (function_table_count > 0) {
    LoadTableSegments(code_table, instance);
  }

  //--------------------------------------------------------------------------
  // Initialize the memory by loading data segments.
  //--------------------------------------------------------------------------
  if (module_->data_segments.size() > 0) {
    LoadDataSegments(wasm_context);
  }

  // Patch all code with the relocations registered in code_specialization.
  code_specialization.RelocateDirectCalls(instance);
  code_specialization.ApplyToWholeInstance(*instance, SKIP_ICACHE_FLUSH);

  if (FLAG_wasm_jit_to_native) {
    FlushICache(isolate_, native_module);
  } else {
    FlushICache(isolate_, code_table);
  }
  FlushICache(isolate_, wrapper_table);

  //--------------------------------------------------------------------------
  // Unpack and notify signal handler of protected instructions.
  //--------------------------------------------------------------------------
  if (trap_handler::UseTrapHandler()) {
    if (FLAG_wasm_jit_to_native) {
      UnpackAndRegisterProtectedInstructions(isolate_, native_module);
    } else {
      UnpackAndRegisterProtectedInstructionsGC(isolate_, code_table);
    }
  }

  //--------------------------------------------------------------------------
  // Insert the compiled module into the weak list of compiled modules.
  //--------------------------------------------------------------------------
  {
    Handle<Object> global_handle =
        isolate_->global_handles()->Create(*instance);
    Handle<WeakCell> link_to_owning_instance = factory->NewWeakCell(instance);
    if (!owner.is_null()) {
      // Publish the new instance to the instances chain.
      DisallowHeapAllocation no_gc;
      compiled_module_->InsertInChain(*module_object_);
    }
    module_object_->set_compiled_module(*compiled_module_);
    compiled_module_->set_weak_owning_instance(link_to_owning_instance);
    GlobalHandles::MakeWeak(global_handle.location(), global_handle.location(),
                            instance_finalizer_callback_,
                            v8::WeakCallbackType::kFinalizer);
  }

  //--------------------------------------------------------------------------
  // Debugging support.
  //--------------------------------------------------------------------------
  // Set all breakpoints that were set on the shared module.
  WasmSharedModuleData::SetBreakpointsOnNewInstance(compiled_module_->shared(),
                                                    instance);

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
  // Execute the start function if one was specified.
  //--------------------------------------------------------------------------
  if (module_->start_function_index >= 0) {
    HandleScope scope(isolate_);
    int start_index = module_->start_function_index;
    WasmCodeWrapper startup_code = EnsureExportedLazyDeoptData(
        isolate_, instance, code_table, native_module, start_index);
    FunctionSig* sig = module_->functions[start_index].sig;
    Handle<Code> wrapper_code = js_to_wasm_cache_.CloneOrCompileJSToWasmWrapper(
        isolate_, module_, startup_code, start_index);
    Handle<WasmExportedFunction> startup_fct = WasmExportedFunction::New(
        isolate_, instance, MaybeHandle<String>(), start_index,
        static_cast<int>(sig->parameter_count()), wrapper_code);
    RecordStats(startup_code, counters());
    // Call the JS function.
    Handle<Object> undefined = factory->undefined_value();
    // Close the CodeSpaceMemoryModificationScope to execute the start function.
    modification_scope.reset();
    {
      // We're OK with JS execution here. The instance is fully setup.
      AllowJavascriptExecution allow_js(isolate_);
      MaybeHandle<Object> retval =
          Execution::Call(isolate_, startup_fct, undefined, 0, nullptr);

      if (retval.is_null()) {
        DCHECK(isolate_->has_pending_exception());
        // It's unfortunate that the new instance is already linked in the
        // chain. However, we need to set up everything before executing the
        // startup unction, such that stack trace information can be generated
        // correctly already in the start function.
        return {};
      }
    }
  }

  DCHECK(!isolate_->has_pending_exception());
  if (FLAG_wasm_jit_to_native) {
    TRACE("Successfully built instance %zu\n",
          compiled_module_->GetNativeModule()->instance_id);
  } else {
    TRACE("Finishing instance %d\n", compiled_module_->instance_id());
  }
  TRACE_CHAIN(module_object_->compiled_module());
  return instance;
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
void InstanceBuilder::LoadDataSegments(WasmContext* wasm_context) {
  Handle<SeqOneByteString> module_bytes(compiled_module_->module_bytes(),
                                        isolate_);
  for (const WasmDataSegment& segment : module_->data_segments) {
    uint32_t source_size = segment.source.length();
    // Segments of size == 0 are just nops.
    if (source_size == 0) continue;
    uint32_t dest_offset = EvalUint32InitExpr(segment.dest_addr);
    DCHECK(in_bounds(dest_offset, source_size, wasm_context->mem_size));
    byte* dest = wasm_context->mem_start + dest_offset;
    const byte* src = reinterpret_cast<const byte*>(
        module_bytes->GetCharsAddress() + segment.source.offset());
    memcpy(dest, src, source_size);
  }
}

void InstanceBuilder::WriteGlobalValue(WasmGlobal& global,
                                       Handle<Object> value) {
  double num = value->Number();
  TRACE("init [globals_start=%p + %u] = %lf, type = %s\n",
        reinterpret_cast<void*>(raw_buffer_ptr(globals_, 0)), global.offset,
        num, WasmOpcodes::TypeName(global.type));
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

void InstanceBuilder::SanitizeImports() {
  Handle<SeqOneByteString> module_bytes(
      module_object_->compiled_module()->module_bytes());
  for (size_t index = 0; index < module_->import_table.size(); ++index) {
    WasmImport& import = module_->import_table[index];

    Handle<String> module_name;
    MaybeHandle<String> maybe_module_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate_, module_bytes, import.module_name);
    if (!maybe_module_name.ToHandle(&module_name)) {
      thrower_->LinkError("Could not resolve module name for import %zu",
                          index);
      return;
    }

    Handle<String> import_name;
    MaybeHandle<String> maybe_import_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
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

Handle<FixedArray> InstanceBuilder::SetupWasmToJSImportsTable(
    Handle<WasmInstanceObject> instance) {
  // The js_imports_table is set up so that index 0 has isolate->native_context
  // and for every index, 3*index+1 has the JSReceiver, 3*index+2 has function's
  // global proxy and 3*index+3 has function's context. Hence, the fixed array's
  // size is 3*import_table.size+1.
  int size = static_cast<int>(module_->import_table.size());
  CHECK_LE(size, (kMaxInt - 1) / 3);
  Handle<FixedArray> func_table =
      isolate_->factory()->NewFixedArray(3 * size + 1, TENURED);
  Handle<FixedArray> js_imports_table =
      isolate_->global_handles()->Create(*func_table);
  GlobalHandles::MakeWeak(
      reinterpret_cast<Object**>(js_imports_table.location()),
      js_imports_table.location(), &FunctionTableFinalizer,
      v8::WeakCallbackType::kFinalizer);
  instance->set_js_imports_table(*func_table);
  js_imports_table->set(0, *isolate_->native_context());
  return js_imports_table;
}

// Process the imports, including functions, tables, globals, and memory, in
// order, loading them from the {ffi_} object. Returns the number of imported
// functions.
int InstanceBuilder::ProcessImports(Handle<FixedArray> code_table,
                                    Handle<WasmInstanceObject> instance) {
  int num_imported_functions = 0;
  int num_imported_tables = 0;
  Handle<FixedArray> js_imports_table = SetupWasmToJSImportsTable(instance);
  WasmInstanceMap imported_wasm_instances(isolate_->heap());
  DCHECK_EQ(module_->import_table.size(), sanitized_imports_.size());
  for (int index = 0; index < static_cast<int>(module_->import_table.size());
       ++index) {
    WasmImport& import = module_->import_table[index];

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
        WasmCodeWrapper import_code = UnwrapExportOrCompileImportWrapper(
            isolate_, module_->functions[import.index].sig,
            Handle<JSReceiver>::cast(value), num_imported_functions,
            module_->origin(), &imported_wasm_instances, js_imports_table,
            instance);
        if (import_code.is_null()) {
          ReportLinkError("imported function does not match the expected type",
                          index, module_name, import_name);
          return -1;
        }
        if (!FLAG_wasm_jit_to_native) {
          code_table->set(num_imported_functions, *import_code.GetCode());
        }
        RecordStats(import_code, counters());
        num_imported_functions++;
        break;
      }
      case kExternalTable: {
        if (!value->IsWasmTableObject()) {
          ReportLinkError("table import requires a WebAssembly.Table", index,
                          module_name, import_name);
          return -1;
        }
        WasmIndirectFunctionTable& table =
            module_->function_tables[num_imported_tables];
        TableInstance& table_instance = table_instances_[num_imported_tables];
        table_instance.table_object = Handle<WasmTableObject>::cast(value);
        instance->set_table_object(*table_instance.table_object);
        table_instance.js_wrappers = Handle<FixedArray>(
            table_instance.table_object->functions(), isolate_);

        int imported_cur_size = table_instance.js_wrappers->length();
        if (imported_cur_size < static_cast<int>(table.initial_size)) {
          thrower_->LinkError(
              "table import %d is smaller than initial %d, got %u", index,
              table.initial_size, imported_cur_size);
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

        // Allocate a new dispatch table and signature table.
        int table_size = imported_cur_size;
        table_instance.function_table =
            isolate_->factory()->NewFixedArray(table_size);
        table_instance.signature_table =
            isolate_->factory()->NewFixedArray(table_size);
        for (int i = 0; i < table_size; ++i) {
          table_instance.signature_table->set(i,
                                              Smi::FromInt(kInvalidSigIndex));
        }
        // Initialize the dispatch table with the (foreign) JS functions
        // that are already in the table.
        for (int i = 0; i < table_size; ++i) {
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
          if (!FLAG_wasm_jit_to_native) {
            FunctionSig* sig = nullptr;
            Handle<Code> code =
                MakeWasmToWasmWrapper(isolate_, target, nullptr, &sig,
                                      &imported_wasm_instances, instance, 0)
                    .GetCode();
            int sig_index = module_->signature_map.Find(sig);
            table_instance.signature_table->set(i, Smi::FromInt(sig_index));
            table_instance.function_table->set(i, *code);
          } else {
            const wasm::WasmCode* exported_code =
                target->GetWasmCode().GetWasmCode();
            wasm::NativeModule* exporting_module = exported_code->owner();
            Handle<WasmInstanceObject> imported_instance =
                handle(target->instance());
            imported_wasm_instances.Set(imported_instance, imported_instance);
            FunctionSig* sig = imported_instance->module()
                                   ->functions[exported_code->index()]
                                   .sig;
            wasm::WasmCode* wrapper_code =
                exporting_module->GetExportedWrapper(exported_code->index());
            if (wrapper_code == nullptr) {
              WasmContext* other_context =
                  imported_instance->wasm_context()->get();
              Handle<Code> wrapper = compiler::CompileWasmToWasmWrapper(
                  isolate_, target->GetWasmCode(), sig,
                  reinterpret_cast<Address>(other_context));
              wrapper_code = exporting_module->AddExportedWrapper(
                  wrapper, exported_code->index());
            }
            int sig_index = module_->signature_map.Find(sig);
            table_instance.signature_table->set(i, Smi::FromInt(sig_index));
            Handle<Foreign> foreign_holder = isolate_->factory()->NewForeign(
                wrapper_code->instructions().start(), TENURED);
            table_instance.function_table->set(i, *foreign_holder);
          }
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
            buffer->byte_length()->Number() / WasmModule::kPageSize);
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
        // Global imports are converted to numbers and written into the
        // {globals_} array buffer.
        if (module_->globals[import.index].type == kWasmI64) {
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
            if (module_->globals[import.index].type == kWasmI32) {
              value = Object::ToInt32(isolate_, value).ToHandleChecked();
            } else {
              value = Object::ToNumber(value).ToHandleChecked();
            }
          }
        }
        if (!value->IsNumber()) {
          ReportLinkError("global import must be a number", index, module_name,
                          import_name);
          return -1;
        }
        WriteGlobalValue(module_->globals[import.index], value);
        break;
      }
      default:
        UNREACHABLE();
        break;
    }
  }

  if (!imported_wasm_instances.empty()) {
    WasmInstanceMap::IteratableScope iteratable_scope(&imported_wasm_instances);
    Handle<FixedArray> instances_array = isolate_->factory()->NewFixedArray(
        imported_wasm_instances.size(), TENURED);
    instance->set_directly_called_instances(*instances_array);
    int index = 0;
    for (auto it = iteratable_scope.begin(), end = iteratable_scope.end();
         it != end; ++it, ++index) {
      instances_array->set(index, ***it);
    }
  }

  return num_imported_functions;
}

template <typename T>
T* InstanceBuilder::GetRawGlobalPtr(WasmGlobal& global) {
  return reinterpret_cast<T*>(raw_buffer_ptr(globals_, global.offset));
}

// Process initialization of globals.
void InstanceBuilder::InitGlobals() {
  for (auto global : module_->globals) {
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
  const bool enable_guard_regions = trap_handler::UseTrapHandler();
  Handle<JSArrayBuffer> mem_buffer = NewArrayBuffer(
      isolate_, num_pages * WasmModule::kPageSize, enable_guard_regions);

  if (mem_buffer.is_null()) {
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
void InstanceBuilder::ProcessExports(
    Handle<WasmInstanceObject> instance,
    Handle<WasmCompiledModule> compiled_module) {
  Handle<FixedArray> wrapper_table = compiled_module->export_wrappers();
  if (NeedsWrappers()) {
    // Fill the table to cache the exported JSFunction wrappers.
    js_wrappers_.insert(js_wrappers_.begin(), module_->functions.size(),
                        Handle<JSFunction>::null());
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

  // Store weak references to all exported functions.
  Handle<FixedArray> weak_exported_functions;
  if (compiled_module->has_weak_exported_functions()) {
    weak_exported_functions = compiled_module->weak_exported_functions();
  } else {
    int export_count = 0;
    for (WasmExport& exp : module_->export_table) {
      if (exp.kind == kExternalFunction) ++export_count;
    }
    weak_exported_functions = isolate_->factory()->NewFixedArray(export_count);
    compiled_module->set_weak_exported_functions(weak_exported_functions);
  }

  // Process each export in the export table.
  int export_index = 0;  // Index into {weak_exported_functions}.
  for (WasmExport& exp : module_->export_table) {
    Handle<String> name = WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
                              isolate_, compiled_module_, exp.name)
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
              wrapper_table->GetValueChecked<Code>(isolate_, export_index);
          MaybeHandle<String> func_name;
          if (module_->is_asm_js()) {
            // For modules arising from asm.js, honor the names section.
            func_name = WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
                            isolate_, compiled_module_, function.name)
                            .ToHandleChecked();
          }
          js_function = WasmExportedFunction::New(
              isolate_, instance, func_name, function.func_index,
              static_cast<int>(function.sig->parameter_count()), export_code);
          js_wrappers_[exp.index] = js_function;
        }
        desc.set_value(js_function);
        Handle<WeakCell> weak_export =
            isolate_->factory()->NewWeakCell(js_function);
        DCHECK_GT(weak_exported_functions->length(), export_index);
        weak_exported_functions->set(export_index, *weak_export);
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
        // Export the value of the global variable as a number.
        WasmGlobal& global = module_->globals[exp.index];
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
  DCHECK_EQ(export_index, weak_exported_functions->length());

  if (module_->is_wasm()) {
    v8::Maybe<bool> success =
        JSReceiver::SetIntegrityLevel(exports_object, FROZEN, kDontThrow);
    DCHECK(success.FromMaybe(false));
    USE(success);
  }
}

void InstanceBuilder::InitializeTables(
    Handle<WasmInstanceObject> instance,
    CodeSpecialization* code_specialization) {
  size_t function_table_count = module_->function_tables.size();
  std::vector<GlobalHandleAddress> new_function_tables(function_table_count);
  std::vector<GlobalHandleAddress> new_signature_tables(function_table_count);

  wasm::NativeModule* native_module = compiled_module_->GetNativeModule();
  std::vector<GlobalHandleAddress> empty;
  std::vector<GlobalHandleAddress>& old_function_tables =
      FLAG_wasm_jit_to_native ? native_module->function_tables() : empty;
  std::vector<GlobalHandleAddress>& old_signature_tables =
      FLAG_wasm_jit_to_native ? native_module->signature_tables() : empty;

  Handle<FixedArray> old_function_tables_gc =
      FLAG_wasm_jit_to_native ? Handle<FixedArray>::null()
                              : compiled_module_->function_tables();
  Handle<FixedArray> old_signature_tables_gc =
      FLAG_wasm_jit_to_native ? Handle<FixedArray>::null()
                              : compiled_module_->signature_tables();

  // function_table_count is 0 or 1, so we just create these objects even if not
  // needed for native wasm.
  // TODO(mtrofin): remove the {..}_gc variables when we don't need
  // FLAG_wasm_jit_to_native
  Handle<FixedArray> new_function_tables_gc =
      isolate_->factory()->NewFixedArray(static_cast<int>(function_table_count),
                                         TENURED);
  Handle<FixedArray> new_signature_tables_gc =
      isolate_->factory()->NewFixedArray(static_cast<int>(function_table_count),
                                         TENURED);

  // These go on the instance.
  Handle<FixedArray> rooted_function_tables =
      isolate_->factory()->NewFixedArray(static_cast<int>(function_table_count),
                                         TENURED);
  Handle<FixedArray> rooted_signature_tables =
      isolate_->factory()->NewFixedArray(static_cast<int>(function_table_count),
                                         TENURED);

  instance->set_function_tables(*rooted_function_tables);
  instance->set_signature_tables(*rooted_signature_tables);

  if (FLAG_wasm_jit_to_native) {
    DCHECK_EQ(old_function_tables.size(), new_function_tables.size());
    DCHECK_EQ(old_signature_tables.size(), new_signature_tables.size());
  } else {
    DCHECK_EQ(old_function_tables_gc->length(),
              new_function_tables_gc->length());
    DCHECK_EQ(old_signature_tables_gc->length(),
              new_signature_tables_gc->length());
  }
  for (size_t index = 0; index < function_table_count; ++index) {
    WasmIndirectFunctionTable& table = module_->function_tables[index];
    TableInstance& table_instance = table_instances_[index];
    int table_size = static_cast<int>(table.initial_size);

    if (table_instance.function_table.is_null()) {
      // Create a new dispatch table if necessary.
      table_instance.function_table =
          isolate_->factory()->NewFixedArray(table_size);
      table_instance.signature_table =
          isolate_->factory()->NewFixedArray(table_size);
      for (int i = 0; i < table_size; ++i) {
        // Fill the table with invalid signature indexes so that
        // uninitialized entries will always fail the signature check.
        table_instance.signature_table->set(i, Smi::FromInt(kInvalidSigIndex));
      }
    } else {
      // Table is imported, patch table bounds check
      DCHECK_LE(table_size, table_instance.function_table->length());
      code_specialization->PatchTableSize(
          table_size, table_instance.function_table->length());
    }
    int int_index = static_cast<int>(index);

    Handle<FixedArray> global_func_table =
        isolate_->global_handles()->Create(*table_instance.function_table);
    Handle<FixedArray> global_sig_table =
        isolate_->global_handles()->Create(*table_instance.signature_table);
    // Make the handles weak. The table objects are rooted on the instance, as
    // they belong to it. We need the global handles in order to have stable
    // pointers to embed in the instance's specialization (wasm compiled code).
    // The order of finalization doesn't matter, in that the instance finalizer
    // may be called before each table's finalizer, or vice-versa.
    // This is because values used for embedding are only interesting should we
    // {Reset} a specialization, in which case they are interesting as values,
    // they are not dereferenced.
    GlobalHandles::MakeWeak(
        reinterpret_cast<Object**>(global_func_table.location()),
        global_func_table.location(), &FunctionTableFinalizer,
        v8::WeakCallbackType::kFinalizer);
    GlobalHandles::MakeWeak(
        reinterpret_cast<Object**>(global_sig_table.location()),
        global_sig_table.location(), &FunctionTableFinalizer,
        v8::WeakCallbackType::kFinalizer);

    rooted_function_tables->set(int_index, *global_func_table);
    rooted_signature_tables->set(int_index, *global_sig_table);

    GlobalHandleAddress new_func_table_addr = global_func_table.address();
    GlobalHandleAddress new_sig_table_addr = global_sig_table.address();
    GlobalHandleAddress old_func_table_addr;
    GlobalHandleAddress old_sig_table_addr;
    if (!FLAG_wasm_jit_to_native) {
      WasmCompiledModule::SetTableValue(isolate_, new_function_tables_gc,
                                        int_index, new_func_table_addr);
      WasmCompiledModule::SetTableValue(isolate_, new_signature_tables_gc,
                                        int_index, new_sig_table_addr);

      old_func_table_addr =
          WasmCompiledModule::GetTableValue(*old_function_tables_gc, int_index);
      old_sig_table_addr = WasmCompiledModule::GetTableValue(
          *old_signature_tables_gc, int_index);
    } else {
      new_function_tables[int_index] = new_func_table_addr;
      new_signature_tables[int_index] = new_sig_table_addr;

      old_func_table_addr = old_function_tables[int_index];
      old_sig_table_addr = old_signature_tables[int_index];
    }
    code_specialization->RelocatePointer(old_func_table_addr,
                                         new_func_table_addr);
    code_specialization->RelocatePointer(old_sig_table_addr,
                                         new_sig_table_addr);
  }

  if (FLAG_wasm_jit_to_native) {
    native_module->function_tables() = new_function_tables;
    native_module->signature_tables() = new_signature_tables;
  } else {
    compiled_module_->set_function_tables(new_function_tables_gc);
    compiled_module_->set_signature_tables(new_signature_tables_gc);
  }
}

void InstanceBuilder::LoadTableSegments(Handle<FixedArray> code_table,
                                        Handle<WasmInstanceObject> instance) {
  wasm::NativeModule* native_module = compiled_module_->GetNativeModule();
  int function_table_count = static_cast<int>(module_->function_tables.size());
  for (int index = 0; index < function_table_count; ++index) {
    TableInstance& table_instance = table_instances_[index];

    Handle<FixedArray> all_dispatch_tables;
    if (!table_instance.table_object.is_null()) {
      // Get the existing dispatch table(s) with the WebAssembly.Table object.
      all_dispatch_tables =
          handle(table_instance.table_object->dispatch_tables());
    }

    // Count the number of table exports for each function (needed for lazy
    // compilation).
    std::unordered_map<uint32_t, uint32_t> num_table_exports;
    if (compile_lazy(module_)) {
      for (auto& table_init : module_->table_inits) {
        for (uint32_t func_index : table_init.entries) {
          if (!FLAG_wasm_jit_to_native) {
            Code* code =
                Code::cast(code_table->get(static_cast<int>(func_index)));
            // Only increase the counter for lazy compile builtins (it's not
            // needed otherwise).
            if (code->is_wasm_code()) continue;
            DCHECK_EQ(Builtins::kWasmCompileLazy, code->builtin_index());
          } else {
            const wasm::WasmCode* code = native_module->GetCode(func_index);
            // Only increase the counter for lazy compile builtins (it's not
            // needed otherwise).
            if (code->kind() == wasm::WasmCode::Function) continue;
            DCHECK_EQ(wasm::WasmCode::LazyStub, code->kind());
          }
          ++num_table_exports[func_index];
        }
      }
    }

    // TODO(titzer): this does redundant work if there are multiple tables,
    // since initializations are not sorted by table index.
    for (auto& table_init : module_->table_inits) {
      uint32_t base = EvalUint32InitExpr(table_init.offset);
      uint32_t num_entries = static_cast<uint32_t>(table_init.entries.size());
      DCHECK(in_bounds(base, num_entries,
                       table_instance.function_table->length()));
      for (uint32_t i = 0; i < num_entries; ++i) {
        uint32_t func_index = table_init.entries[i];
        WasmFunction* function = &module_->functions[func_index];
        int table_index = static_cast<int>(i + base);
        uint32_t sig_index = module_->signature_ids[function->sig_index];
        table_instance.signature_table->set(table_index,
                                            Smi::FromInt(sig_index));
        WasmCodeWrapper wasm_code = EnsureTableExportLazyDeoptData(
            isolate_, instance, code_table, native_module, func_index,
            table_instance.function_table, table_index, &num_table_exports);
        Handle<Object> value_to_update_with;
        if (!wasm_code.IsCodeObject()) {
          Handle<Foreign> as_foreign = isolate_->factory()->NewForeign(
              wasm_code.GetWasmCode()->instructions().start(), TENURED);
          table_instance.function_table->set(table_index, *as_foreign);
          value_to_update_with = as_foreign;
        } else {
          table_instance.function_table->set(table_index, *wasm_code.GetCode());
          value_to_update_with = wasm_code.GetCode();
        }
        if (!all_dispatch_tables.is_null()) {
          if (js_wrappers_[func_index].is_null()) {
            // No JSFunction entry yet exists for this function. Create one.
            // TODO(titzer): We compile JS->wasm wrappers for functions are
            // not exported but are in an exported table. This should be done
            // at module compile time and cached instead.

            Handle<Code> wrapper_code =
                js_to_wasm_cache_.CloneOrCompileJSToWasmWrapper(
                    isolate_, module_, wasm_code, func_index);
            MaybeHandle<String> func_name;
            if (module_->is_asm_js()) {
              // For modules arising from asm.js, honor the names section.
              func_name = WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
                              isolate_, compiled_module_, function->name)
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
          // When updating dispatch tables, we need to provide a wasm-to-wasm
          // wrapper for wasm_code - unless wasm_code is already a wrapper. If
          // it's a wasm-to-js wrapper, we don't need to construct a
          // wasm-to-wasm wrapper because there's no context switching required.
          // The remaining case is that it's a wasm-to-wasm wrapper, in which
          // case it's already doing "the right thing", and wrapping it again
          // would be redundant.
          if (func_index >= module_->num_imported_functions) {
            value_to_update_with = GetOrCreateIndirectCallWrapper(
                isolate_, instance, wasm_code, func_index, function->sig);
          } else {
            if (wasm_code.IsCodeObject()) {
              DCHECK(wasm_code.GetCode()->kind() == Code::WASM_TO_JS_FUNCTION ||
                     wasm_code.GetCode()->kind() ==
                         Code::WASM_TO_WASM_FUNCTION);
            } else {
              DCHECK(wasm_code.GetWasmCode()->kind() ==
                         WasmCode::WasmToJsWrapper ||
                     wasm_code.GetWasmCode()->kind() ==
                         WasmCode::WasmToWasmWrapper);
            }
          }
          UpdateDispatchTables(isolate_, all_dispatch_tables, table_index,
                               function, value_to_update_with);
        }
      }
    }

#ifdef DEBUG
    // Check that the count of table exports was accurate. The entries are
    // decremented on each export, so all should be zero now.
    for (auto e : num_table_exports) {
      DCHECK_EQ(0, e.second);
    }
#endif

    // TODO(titzer): we add the new dispatch table at the end to avoid
    // redundant work and also because the new instance is not yet fully
    // initialized.
    if (!table_instance.table_object.is_null()) {
      // Add the new dispatch table to the WebAssembly.Table object.
      all_dispatch_tables = WasmTableObject::AddDispatchTable(
          isolate_, table_instance.table_object, instance, index,
          table_instance.function_table, table_instance.signature_table);
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
  background_task_runner_ = platform->GetBackgroundTaskRunner(v8_isolate);
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
  if (num_pending_foreground_tasks_ == 0) {
    // No task is pending, we can just remove the AsyncCompileJob.
    isolate_->wasm_compilation_manager()->RemoveJob(this);
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
  // Finishes the AsyncCOmpileJob with an error.
  void FinishAsyncCompileJobWithError(ResultBase result);

  ModuleDecoder decoder_;
  AsyncCompileJob* job_;
  std::unique_ptr<ModuleCompiler::CompilationUnitBuilder>
      compilation_unit_builder_;
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

void AsyncCompileJob::AsyncCompileFailed(ErrorThrower& thrower) {
  if (stream_) stream_->NotifyError();
  // {job} keeps the {this} pointer alive.
  std::shared_ptr<AsyncCompileJob> job =
      isolate_->wasm_compilation_manager()->RemoveJob(this);
  RejectPromise(isolate_, context_, thrower, module_promise_);
}

void AsyncCompileJob::AsyncCompileSucceeded(Handle<Object> result) {
  // {job} keeps the {this} pointer alive.
  std::shared_ptr<AsyncCompileJob> job =
      isolate_->wasm_compilation_manager()->RemoveJob(this);
  ResolvePromise(isolate_, context_, module_promise_, result);
}

// A closure to run a compilation step (either as foreground or background
// task) and schedule the next step(s), if any.
class AsyncCompileJob::CompileStep {
 public:
  explicit CompileStep(size_t num_background_tasks = 0)
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

  size_t NumberOfBackgroundTasks() { return num_background_tasks_; }

  AsyncCompileJob* job_ = nullptr;
  const size_t num_background_tasks_;
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
  background_task_runner_->PostTask(
      base::make_unique<CompileTask>(this, false));
}

void AsyncCompileJob::RestartBackgroundTasks() {
  size_t num_restarts = stopped_tasks_.Value();
  stopped_tasks_.Decrement(num_restarts);

  for (size_t i = 0; i < num_restarts; ++i) {
    StartBackgroundTask();
  }
}

template <typename Step, typename... Args>
void AsyncCompileJob::DoAsync(Args&&... args) {
  NextStep<Step>(std::forward<Args>(args)...);
  size_t end = step_->NumberOfBackgroundTasks();
  for (size_t i = 0; i < end; ++i) {
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
    return job_->AsyncCompileFailed(thrower);
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
    Isolate* isolate = job_->isolate_;
    Factory* factory = isolate->factory();

    Handle<Code> illegal_builtin = BUILTIN_CODE(isolate, Illegal);
    if (!FLAG_wasm_jit_to_native) {
      // The {code_table} array contains import wrappers and functions (which
      // are both included in {functions.size()}.
      // The results of compilation will be written into it.
      // Initialize {code_table_} with the illegal builtin. All call sites
      // will be patched at instantiation.
      int code_table_size = static_cast<int>(module_->functions.size());
      job_->code_table_ = factory->NewFixedArray(code_table_size, TENURED);

      for (int i = 0, e = module_->num_imported_functions; i < e; ++i) {
        job_->code_table_->set(i, *illegal_builtin);
      }
    } else {
      // Just makes it easier to deal with code that wants code_table, while
      // we have FLAG_wasm_jit_to_native around.
      job_->code_table_ = factory->NewFixedArray(0, TENURED);
    }

    job_->module_env_ =
        CreateDefaultModuleEnv(isolate, module_, illegal_builtin);

    // Transfer ownership of the {WasmModule} to the {ModuleCompiler}, but
    // keep a pointer.
    Handle<Code> centry_stub = CEntryStub(isolate, 1).GetCode();
    {
      // Now reopen the handles in a deferred scope in order to use
      // them in the concurrent steps.
      DeferredHandleScope deferred(isolate);

      centry_stub = Handle<Code>(*centry_stub, isolate);
      job_->code_table_ = Handle<FixedArray>(*job_->code_table_, isolate);
      compiler::ModuleEnv* env = job_->module_env_.get();
      ReopenHandles(isolate, env->function_code);
      Handle<Code>* mut =
          const_cast<Handle<Code>*>(&env->default_function_code);
      *mut = Handle<Code>(**mut, isolate);

      job_->deferred_handles_.push_back(deferred.Detach());
    }

    DCHECK_LE(module_->num_imported_functions, module_->functions.size());
    // Create the compiled module object and populate with compiled functions
    // and information needed at instantiation time. This object needs to be
    // serializable. Instantiation may occur off a deserialized version of
    // this object.
    int export_wrapper_size = static_cast<int>(module_->num_exported_functions);
    Handle<FixedArray> export_wrappers =
        job_->isolate_->factory()->NewFixedArray(export_wrapper_size, TENURED);

    job_->compiled_module_ =
        NewCompiledModule(job_->isolate_, module_, job_->code_table_,
                          export_wrappers, job_->module_env_.get());

    job_->compiler_.reset(
        new ModuleCompiler(isolate, module_, centry_stub,
                           job_->compiled_module_->GetNativeModule()));
    job_->compiler_->EnableThrottling();

    {
      DeferredHandleScope deferred(job_->isolate_);
      job_->compiled_module_ = handle(*job_->compiled_module_, job_->isolate_);
      job_->deferred_handles_.push_back(deferred.Detach());
    }
    size_t num_functions =
        module_->functions.size() - module_->num_imported_functions;

    if (num_functions == 0) {
      // Degenerate case of an empty module.
      job_->DoSync<FinishCompile>();
      return;
    }

    // Start asynchronous compilation tasks.
    size_t num_background_tasks =
        Max(static_cast<size_t>(1),
            Min(num_functions,
                Min(static_cast<size_t>(FLAG_wasm_num_compilation_tasks),
                    V8::GetCurrentPlatform()
                        ->NumberOfAvailableBackgroundThreads())));
    if (start_compilation_) {
      // TODO(ahaas): Try to remove the {start_compilation_} check when
      // streaming decoding is done in the background. If
      // InitializeCompilationUnits always returns 0 for streaming compilation,
      // then DoAsync would do the same as NextStep already.
      job_->outstanding_units_ = job_->compiler_->InitializeCompilationUnits(
          module_->functions, job_->wire_bytes_, job_->module_env_.get());

      job_->DoAsync<ExecuteAndFinishCompilationUnits>(num_background_tasks);
    } else {
      job_->stopped_tasks_ = num_background_tasks;
      job_->NextStep<ExecuteAndFinishCompilationUnits>(num_background_tasks);
    }
  }
};

//==========================================================================
// Step 3 (async x K tasks): Execute compilation units.
//==========================================================================
class AsyncCompileJob::ExecuteAndFinishCompilationUnits : public CompileStep {
 public:
  explicit ExecuteAndFinishCompilationUnits(size_t num_compile_tasks)
      : CompileStep(num_compile_tasks) {}

  void RunInBackground() override {
    std::function<void()> StartFinishCompilationUnit = [this]() {
      if (!failed_) job_->StartForegroundTask();
    };

    TRACE_COMPILE("(3) Compiling...\n");
    while (job_->compiler_->CanAcceptWork()) {
      if (failed_) break;
      DisallowHandleAllocation no_handle;
      DisallowHeapAllocation no_allocation;
      if (!job_->compiler_->FetchAndExecuteCompilationUnit(
              StartFinishCompilationUnit)) {
        finished_ = true;
        break;
      }
    }
    job_->stopped_tasks_.Increment(1);
  }

  void RunInForeground() override {
    // TODO(6792): No longer needed once WebAssembly code is off heap.
    // Use base::Optional to be able to close the scope before we resolve or
    // reject the promise.
    base::Optional<CodeSpaceMemoryModificationScope> modification_scope(
        base::in_place_t(), job_->isolate_->heap());
    TRACE_COMPILE("(4a) Finishing compilation units...\n");
    if (failed_) {
      // The job failed already, no need to do more work.
      job_->compiler_->SetFinisherIsRunning(false);
      return;
    }
    ErrorThrower thrower(job_->isolate_, "AsyncCompile");

    // We execute for 1 ms and then reschedule the task, same as the GC.
    double deadline = MonotonicallyIncreasingTimeInMs() + 1.0;

    while (true) {
      if (!finished_ && job_->compiler_->ShouldIncreaseWorkload()) {
        job_->RestartBackgroundTasks();
      }

      int func_index = -1;

      WasmCodeWrapper result =
          job_->compiler_->FinishCompilationUnit(&thrower, &func_index);

      if (thrower.error()) {
        // An error was detected, we stop compiling and wait for the
        // background tasks to finish.
        failed_ = true;
        break;
      } else if (result.is_null()) {
        // The working queue was empty, we break the loop. If new work units
        // are enqueued, the background task will start this
        // FinishCompilationUnits task again.
        break;
      } else {
        DCHECK_LE(0, func_index);
        if (result.IsCodeObject()) {
          job_->code_table_->set(func_index, *result.GetCode());
        }
        --job_->outstanding_units_;
      }

      if (deadline < MonotonicallyIncreasingTimeInMs()) {
        // We reached the deadline. We reschedule this task and return
        // immediately. Since we rescheduled this task already, we do not set
        // the FinisherIsRunning flat to false.
        job_->StartForegroundTask();
        return;
      }
    }
    // This task finishes without being rescheduled. Therefore we set the
    // FinisherIsRunning flag to false.
    job_->compiler_->SetFinisherIsRunning(false);
    if (thrower.error()) {
      // Make sure all compilation tasks stopped running.
      job_->background_task_manager_.CancelAndWait();

      // Close the CodeSpaceMemoryModificationScope before we reject the promise
      // in AsyncCompileFailed. Promise::Reject calls directly into JavaScript.
      modification_scope.reset();
      return job_->AsyncCompileFailed(thrower);
    }
    if (job_->outstanding_units_ == 0) {
      // Make sure all compilation tasks stopped running.
      job_->background_task_manager_.CancelAndWait();
      if (job_->DecrementAndCheckFinisherCount()) job_->DoSync<FinishCompile>();
    }
  }

 private:
  std::atomic<bool> failed_{false};
  std::atomic<bool> finished_{false};
};

//==========================================================================
// Step 5 (sync): Finish heap-allocated data structures.
//==========================================================================
class AsyncCompileJob::FinishCompile : public CompileStep {
  void RunInForeground() override {
    TRACE_COMPILE("(5b) Finish compile...\n");
    if (FLAG_wasm_jit_to_native) {
      RecordStats(job_->compiled_module_->GetNativeModule(), job_->counters());
    } else {
      // At this point, compilation has completed. Update the code table.
      for (int i = FLAG_skip_compiling_wasm_funcs,
               e = job_->code_table_->length();
           i < e; ++i) {
        Object* val = job_->code_table_->get(i);
        if (val->IsCode()) RecordStats(Code::cast(val), job_->counters());
      }
    }

    // Create heap objects for script and module bytes to be stored in the
    // shared module data. Asm.js is not compiled asynchronously.
    Handle<Script> script = CreateWasmScript(job_->isolate_, job_->wire_bytes_);
    Handle<ByteArray> asm_js_offset_table;
    // TODO(wasm): Improve efficiency of storing module wire bytes.
    //   1. Only store relevant sections, not function bodies
    //   2. Don't make a second copy of the bytes here; reuse the copy made
    //      for asynchronous compilation and store it as an external one
    //      byte string for serialization/deserialization.
    Handle<String> module_bytes =
        job_->isolate_->factory()
            ->NewStringFromOneByte(
                {job_->wire_bytes_.start(), job_->wire_bytes_.length()},
                TENURED)
            .ToHandleChecked();
    DCHECK(module_bytes->IsSeqOneByteString());

    // The {module_wrapper} will take ownership of the {WasmModule} object,
    // and it will be destroyed when the GC reclaims the wrapper object.
    Handle<WasmModuleWrapper> module_wrapper =
        WasmModuleWrapper::From(job_->isolate_, job_->module_.release());

    // Create the shared module data.
    // TODO(clemensh): For the same module (same bytes / same hash), we should
    // only have one WasmSharedModuleData. Otherwise, we might only set
    // breakpoints on a (potentially empty) subset of the instances.

    Handle<WasmSharedModuleData> shared =
        WasmSharedModuleData::New(job_->isolate_, module_wrapper,
                                  Handle<SeqOneByteString>::cast(module_bytes),
                                  script, asm_js_offset_table);
    job_->compiled_module_->OnWasmModuleDecodingComplete(shared);
    script->set_wasm_compiled_module(*job_->compiled_module_);

    // Finish the wasm script now and make it public to the debugger.
    job_->isolate_->debug()->OnAfterCompile(
        handle(job_->compiled_module_->script()));

    // TODO(wasm): compiling wrappers should be made async as well.
    job_->DoSync<CompileWrappers>();
  }
};

//==========================================================================
// Step 6 (sync): Compile JS->wasm wrappers.
//==========================================================================
class AsyncCompileJob::CompileWrappers : public CompileStep {
  // TODO(wasm): Compile all wrappers here, including the start function wrapper
  // and the wrappers for the function table elements.
  void RunInForeground() override {
    TRACE_COMPILE("(6) Compile wrappers...\n");
    // TODO(6792): No longer needed once WebAssembly code is off heap.
    CodeSpaceMemoryModificationScope modification_scope(job_->isolate_->heap());
    // Compile JS->wasm wrappers for exported functions.
    CompileJsToWasmWrappers(job_->isolate_, job_->compiled_module_,
                            job_->counters());
    job_->DoSync<FinishModule>();
  }
};

//==========================================================================
// Step 7 (sync): Finish the module and resolve the promise.
//==========================================================================
class AsyncCompileJob::FinishModule : public CompileStep {
  void RunInForeground() override {
    TRACE_COMPILE("(7) Finish module...\n");
    Handle<WasmModuleObject> result =
        WasmModuleObject::New(job_->isolate_, job_->compiled_module_);
    // {job_} is deleted in AsyncCompileSucceeded, therefore the {return}.
    return job_->AsyncCompileSucceeded(result);
  }
};

class AsyncCompileJob::AbortCompilation : public CompileStep {
  void RunInForeground() override {
    TRACE_COMPILE("Abort asynchronous compilation ...\n");
    job_->isolate_->wasm_compilation_manager()->RemoveJob(job_);
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

  // Check if there is already a ModuleCompiler, in which case we have to clean
  // it up as well.
  if (job_->compiler_) {
    // If {IsFinisherRunning} is true, then there is already a foreground task
    // in the task queue to execute the DecodeFail step. We do not have to start
    // a new task ourselves with DoSync.
    if (job_->compiler_->IsFinisherRunning()) {
      job_->NextStep<AsyncCompileJob::DecodeFail>(std::move(result));
    } else {
      job_->DoSync<AsyncCompileJob::DecodeFail>(std::move(result));
    }

    compilation_unit_builder_->Clear();
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
  if (section_code == SectionCode::kUnknownSectionCode) {
    // No need to decode unknown sections, even the names section. If decoding
    // of the unknown section fails, compilation should succeed anyways, and
    // even decoding the names section is unnecessary because the result comes
    // too late for streaming compilation.
    return true;
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

  job_->outstanding_units_ = functions_count;
  // Set outstanding_finishers_ to 2, because both the AsyncCompileJob and the
  // AsyncStreamingProcessor have to finish.
  job_->outstanding_finishers_.SetValue(2);
  compilation_unit_builder_.reset(
      new ModuleCompiler::CompilationUnitBuilder(job_->compiler_.get()));
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
    compilation_unit_builder_->AddUnit(
        job_->module_env_.get(), job_->compiled_module_->GetNativeModule(),
        func, offset, bytes, name);
  }
  ++next_function_;
  // This method always succeeds. The return value is necessary to comply with
  // the StreamingProcessor interface.
  return true;
}

void AsyncStreamingProcessor::OnFinishedChunk() {
  // TRACE_STREAMING("FinishChunk...\n");
  if (compilation_unit_builder_) {
    compilation_unit_builder_->Commit();
    job_->RestartBackgroundTasks();
  }
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
    if (!job_->compiler_) {
      // We are processing a WebAssembly module without code section. We need to
      // prepare compilation first before we can finish it.
      // {PrepareAndStartCompile} will call {FinishCompile} by itself if there
      // is no code section.
      job_->DoSync<AsyncCompileJob::PrepareAndStartCompile>(job_->module_.get(),
                                                            true);
    } else {
      job_->DoSync<AsyncCompileJob::FinishCompile>();
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

void CompileJsToWasmWrappers(Isolate* isolate,
                             Handle<WasmCompiledModule> compiled_module,
                             Counters* counters) {
  JSToWasmWrapperCache js_to_wasm_cache;
  int wrapper_index = 0;
  Handle<FixedArray> export_wrappers = compiled_module->export_wrappers();
  NativeModule* native_module = compiled_module->GetNativeModule();
  for (auto exp : compiled_module->module()->export_table) {
    if (exp.kind != kExternalFunction) continue;
    WasmCodeWrapper wasm_code = EnsureExportedLazyDeoptData(
        isolate, Handle<WasmInstanceObject>::null(),
        compiled_module->code_table(), native_module, exp.index);
    Handle<Code> wrapper_code = js_to_wasm_cache.CloneOrCompileJSToWasmWrapper(
        isolate, compiled_module->module(), wasm_code, exp.index);
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

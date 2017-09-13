// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <src/wasm/module-compiler.h>

#include <atomic>

#include "src/asmjs/asm-js.h"
#include "src/assembler-inl.h"
#include "src/code-stubs.h"
#include "src/counters.h"
#include "src/property-descriptor.h"
#include "src/wasm/compilation-manager.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
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

static const int kInvalidSigIndex = -1;

namespace v8 {
namespace internal {
namespace wasm {

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

ModuleCompiler::ModuleCompiler(Isolate* isolate,
                               std::unique_ptr<WasmModule> module)
    : isolate_(isolate),
      module_(std::move(module)),
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
      centry_stub_(CEntryStub(isolate, 1).GetCode()) {}

// The actual runnable task that performs compilations in the background.
ModuleCompiler::CompilationTask::CompilationTask(ModuleCompiler* compiler)
    : CancelableTask(&compiler->background_task_manager_),
      compiler_(compiler) {}

void ModuleCompiler::CompilationTask::RunInternal() {
  while (compiler_->executed_units_.CanAcceptWork() &&
         compiler_->FetchAndExecuteCompilationUnit()) {
  }

  compiler_->OnBackgroundTaskStopped();
}

void ModuleCompiler::OnBackgroundTaskStopped() {
  base::LockGuard<base::Mutex> guard(&tasks_mutex_);
  ++stopped_compilation_tasks_;
  DCHECK_LE(stopped_compilation_tasks_, num_background_tasks_);
}

// Run by each compilation task The no_finisher_callback is called
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
    const std::vector<WasmFunction>& functions, ModuleBytesEnv& module_env) {
  uint32_t start = module_env.module_env.module->num_imported_functions +
                   FLAG_skip_compiling_wasm_funcs;
  uint32_t num_funcs = static_cast<uint32_t>(functions.size());
  uint32_t funcs_to_compile = start > num_funcs ? 0 : num_funcs - start;
  CompilationUnitBuilder builder(this);
  for (uint32_t i = start; i < num_funcs; ++i) {
    const WasmFunction* func = &functions[i];
    uint32_t buffer_offset = func->code.offset();
    Vector<const uint8_t> bytes(
        module_env.wire_bytes.start() + func->code.offset(),
        func->code.end_offset() - func->code.offset());
    WasmName name = module_env.wire_bytes.GetName(func);
    builder.AddUnit(&module_env.module_env, func, buffer_offset, bytes, name);
  }
  builder.Commit();
  return funcs_to_compile;
}

void ModuleCompiler::ReopenHandlesInDeferredScope() {
  centry_stub_ = handle(*centry_stub_, isolate_);
}

void ModuleCompiler::RestartCompilationTasks() {
  base::LockGuard<base::Mutex> guard(&tasks_mutex_);
  for (; stopped_compilation_tasks_ > 0; --stopped_compilation_tasks_) {
    V8::GetCurrentPlatform()->CallOnBackgroundThread(
        new CompilationTask(this),
        v8::Platform::ExpectedRuntime::kShortRunningTask);
  }
}

size_t ModuleCompiler::FinishCompilationUnits(
    std::vector<Handle<Code>>& results, ErrorThrower* thrower) {
  size_t finished = 0;
  while (true) {
    int func_index = -1;
    MaybeHandle<Code> result = FinishCompilationUnit(thrower, &func_index);
    if (func_index < 0) break;
    ++finished;
    DCHECK_IMPLIES(result.is_null(), thrower->error());
    if (result.is_null()) break;
    results[func_index] = result.ToHandleChecked();
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

MaybeHandle<Code> ModuleCompiler::FinishCompilationUnit(ErrorThrower* thrower,
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

void ModuleCompiler::CompileInParallel(ModuleBytesEnv* module_env,
                                       std::vector<Handle<Code>>& results,
                                       ErrorThrower* thrower) {
  const WasmModule* module = module_env->module_env.module;
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
  InitializeCompilationUnits(module->functions, *module_env);
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

void ModuleCompiler::CompileSequentially(ModuleBytesEnv* module_env,
                                         std::vector<Handle<Code>>& results,
                                         ErrorThrower* thrower) {
  DCHECK(!thrower->error());

  const WasmModule* module = module_env->module_env.module;
  for (uint32_t i = FLAG_skip_compiling_wasm_funcs;
       i < module->functions.size(); ++i) {
    const WasmFunction& func = module->functions[i];
    if (func.imported) continue;  // Imports are compiled at instantiation time.

    // Compile the function.
    MaybeHandle<Code> code = compiler::WasmCompilationUnit::CompileWasmFunction(
        thrower, isolate_, module_env, &func);
    if (code.is_null()) {
      WasmName str = module_env->wire_bytes.GetName(&func);
      // TODO(clemensh): Truncate the function name in the output.
      thrower->CompileError("Compilation of #%d:%.*s failed.", i, str.length(),
                            str.start());
      break;
    }
    results[i] = code.ToHandleChecked();
  }
}

void ModuleCompiler::ValidateSequentially(ModuleBytesEnv* module_env,
                                          ErrorThrower* thrower) {
  DCHECK(!thrower->error());

  const WasmModule* module = module_env->module_env.module;
  for (uint32_t i = 0; i < module->functions.size(); ++i) {
    const WasmFunction& func = module->functions[i];
    if (func.imported) continue;

    const byte* base = module_env->wire_bytes.start();
    FunctionBody body{func.sig, func.code.offset(), base + func.code.offset(),
                      base + func.code.end_offset()};
    DecodeResult result = VerifyWasmCode(isolate_->allocator(),
                                         module_env->module_env.module, body);
    if (result.failed()) {
      WasmName str = module_env->wire_bytes.GetName(&func);
      thrower->CompileError("Compiling function #%d:%.*s failed: %s @+%u", i,
                            str.length(), str.start(),
                            result.error_msg().c_str(), result.error_offset());
      break;
    }
  }
}

MaybeHandle<WasmModuleObject> ModuleCompiler::CompileToModuleObject(
    ErrorThrower* thrower, const ModuleWireBytes& wire_bytes,
    Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {
  Factory* factory = isolate_->factory();
  WasmInstance temp_instance(module_.get());
  temp_instance.context = isolate_->native_context();
  temp_instance.mem_size = WasmModule::kPageSize * module_->min_mem_pages;
  temp_instance.mem_start = nullptr;
  temp_instance.globals_start = nullptr;

  // Initialize the indirect tables with placeholders.
  int function_table_count = static_cast<int>(module_->function_tables.size());
  Handle<FixedArray> function_tables =
      factory->NewFixedArray(function_table_count, TENURED);
  Handle<FixedArray> signature_tables =
      factory->NewFixedArray(function_table_count, TENURED);
  for (int i = 0; i < function_table_count; ++i) {
    temp_instance.function_tables[i] = factory->NewFixedArray(1, TENURED);
    temp_instance.signature_tables[i] = factory->NewFixedArray(1, TENURED);
    function_tables->set(i, *temp_instance.function_tables[i]);
    signature_tables->set(i, *temp_instance.signature_tables[i]);
  }

  TimedHistogramScope wasm_compile_module_time_scope(
      module_->is_wasm() ? counters()->wasm_compile_wasm_module_time()
                         : counters()->wasm_compile_asm_module_time());
  return CompileToModuleObjectInternal(
      thrower, wire_bytes, asm_js_script, asm_js_offset_table_bytes, factory,
      &temp_instance, &function_tables, &signature_tables);
}

namespace {
bool compile_lazy(const WasmModule* module) {
  return FLAG_wasm_lazy_compilation ||
         (FLAG_asm_wasm_lazy_compilation && module->is_asm_js());
}

void FlushICache(Isolate* isolate, Handle<FixedArray> code_table) {
  for (int i = 0; i < code_table->length(); ++i) {
    Handle<Code> code = code_table->GetValueChecked<Code>(isolate, i);
    Assembler::FlushICache(isolate, code->instruction_start(),
                           code->instruction_size());
  }
}

byte* raw_buffer_ptr(MaybeHandle<JSArrayBuffer> buffer, int offset) {
  return static_cast<byte*>(buffer.ToHandleChecked()->backing_store()) + offset;
}

void RecordStats(Code* code, Counters* counters) {
  counters->wasm_generated_code_size()->Increment(code->body_size());
  counters->wasm_reloc_size()->Increment(code->relocation_info()->length());
}

void RecordStats(Handle<FixedArray> functions, Counters* counters) {
  DisallowHeapAllocation no_gc;
  for (int i = 0; i < functions->length(); ++i) {
    RecordStats(Code::cast(functions->get(i)), counters);
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

// Ensure that the code object in <code_table> at offset <func_index> has
// deoptimization data attached. This is needed for lazy compile stubs which are
// called from JS_TO_WASM functions or via exported function tables. The deopt
// data is used to determine which function this lazy compile stub belongs to.
Handle<Code> EnsureExportedLazyDeoptData(Isolate* isolate,
                                         Handle<WasmInstanceObject> instance,
                                         Handle<FixedArray> code_table,
                                         int func_index) {
  Handle<Code> code(Code::cast(code_table->get(func_index)), isolate);
  if (code->builtin_index() != Builtins::kWasmCompileLazy) {
    // No special deopt data needed for compiled functions, and imported
    // functions, which map to Illegal at this point (they get compiled at
    // instantiation time).
    DCHECK(code->kind() == Code::WASM_FUNCTION ||
           code->kind() == Code::WASM_TO_JS_FUNCTION ||
           code->builtin_index() == Builtins::kIllegal);
    return code;
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
    deopt_data = isolate->factory()->NewFixedArray(2, TENURED);
    code->set_deoptimization_data(*deopt_data);
    if (!instance.is_null()) {
      Handle<WeakCell> weak_instance =
          isolate->factory()->NewWeakCell(instance);
      deopt_data->set(0, *weak_instance);
    }
    deopt_data->set(1, Smi::FromInt(func_index));
  }
  DCHECK_IMPLIES(!instance.is_null(),
                 WeakCell::cast(code->deoptimization_data()->get(0))->value() ==
                     *instance);
  DCHECK_EQ(func_index, Smi::ToInt(code->deoptimization_data()->get(1)));
  return code;
}

// Ensure that the code object in <code_table> at offset <func_index> has
// deoptimization data attached. This is needed for lazy compile stubs which are
// called from JS_TO_WASM functions or via exported function tables. The deopt
// data is used to determine which function this lazy compile stub belongs to.
Handle<Code> EnsureTableExportLazyDeoptData(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    Handle<FixedArray> code_table, int func_index,
    Handle<FixedArray> export_table, int export_index,
    std::unordered_map<uint32_t, uint32_t>& table_export_count) {
  Handle<Code> code =
      EnsureExportedLazyDeoptData(isolate, instance, code_table, func_index);
  if (code->builtin_index() != Builtins::kWasmCompileLazy) return code;

  // deopt_data:
  // #0: weak instance
  // #1: func_index
  // [#2: export table
  //  #3: export table index]
  // [#4: export table
  //  #5: export table index]
  // ...
  // table_export_count counts down and determines the index for the new export
  // table entry.
  auto table_export_entry = table_export_count.find(func_index);
  DCHECK(table_export_entry != table_export_count.end());
  DCHECK_LT(0, table_export_entry->second);
  uint32_t this_idx = 2 * table_export_entry->second;
  --table_export_entry->second;
  Handle<FixedArray> deopt_data(code->deoptimization_data());
  DCHECK_EQ(0, deopt_data->length() % 2);
  if (deopt_data->length() == 2) {
    // Then only the "header" (#0 and #1) exists. Extend for the export table
    // entries (make space for this_idx + 2 elements).
    deopt_data = isolate->factory()->CopyFixedArrayAndGrow(deopt_data, this_idx,
                                                           TENURED);
    code->set_deoptimization_data(*deopt_data);
  }
  DCHECK_LE(this_idx + 2, deopt_data->length());
  DCHECK(deopt_data->get(this_idx)->IsUndefined(isolate));
  DCHECK(deopt_data->get(this_idx + 1)->IsUndefined(isolate));
  deopt_data->set(this_idx, *export_table);
  deopt_data->set(this_idx + 1, Smi::FromInt(export_index));
  return code;
}

bool in_bounds(uint32_t offset, uint32_t size, uint32_t upper) {
  return offset + size <= upper && offset + size >= offset;
}

using WasmInstanceMap =
    IdentityMap<Handle<WasmInstanceObject>, FreeStoreAllocationPolicy>;

Handle<Code> UnwrapOrCompileImportWrapper(
    Isolate* isolate, int index, FunctionSig* sig, Handle<JSReceiver> target,
    Handle<String> module_name, MaybeHandle<String> import_name,
    ModuleOrigin origin, WasmInstanceMap* imported_instances) {
  WasmFunction* other_func = GetWasmFunctionForImportWrapper(isolate, target);
  if (other_func) {
    if (!sig->Equals(other_func->sig)) return Handle<Code>::null();
    // Signature matched. Unwrap the import wrapper and return the raw wasm
    // function code.
    // Remember the wasm instance of the import. We have to keep it alive.
    Handle<WasmInstanceObject> imported_instance(
        Handle<WasmExportedFunction>::cast(target)->instance(), isolate);
    imported_instances->Set(imported_instance, imported_instance);
    return UnwrapImportWrapper(target);
  }
  // No wasm function or being debugged. Compile a new wrapper for the new
  // signature.
  return compiler::CompileWasmToJSWrapper(isolate, target, sig, index,
                                          module_name, import_name, origin);
}

double MonotonicallyIncreasingTimeInMs() {
  return V8::GetCurrentPlatform()->MonotonicallyIncreasingTime() *
         base::Time::kMillisecondsPerSecond;
}

void RejectPromise(Isolate* isolate, Handle<Context> context,
                   ErrorThrower& thrower, Handle<JSPromise> promise) {
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Utils::PromiseToLocal(promise).As<v8::Promise::Resolver>();
  auto maybe = resolver->Reject(v8::Utils::ToLocal(context),
                                v8::Utils::ToLocal(thrower.Reify()));
  CHECK_IMPLIES(!maybe.FromMaybe(false), isolate->has_scheduled_exception());
}

void ResolvePromise(Isolate* isolate, Handle<Context> context,
                    Handle<JSPromise> promise, Handle<Object> result) {
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Utils::PromiseToLocal(promise).As<v8::Promise::Resolver>();
  auto maybe = resolver->Resolve(v8::Utils::ToLocal(context),
                                 v8::Utils::ToLocal(result));
  CHECK_IMPLIES(!maybe.FromMaybe(false), isolate->has_scheduled_exception());
}

}  // namespace

MaybeHandle<WasmModuleObject> ModuleCompiler::CompileToModuleObjectInternal(
    ErrorThrower* thrower, const ModuleWireBytes& wire_bytes,
    Handle<Script> asm_js_script, Vector<const byte> asm_js_offset_table_bytes,
    Factory* factory, WasmInstance* temp_instance,
    Handle<FixedArray>* function_tables, Handle<FixedArray>* signature_tables) {
  ModuleBytesEnv module_env(module_.get(), temp_instance, wire_bytes);

  // The {code_table} array contains import wrappers and functions (which
  // are both included in {functions.size()}, and export wrappers.
  int code_table_size = static_cast<int>(module_->functions.size() +
                                         module_->num_exported_functions);
  Handle<FixedArray> code_table =
      factory->NewFixedArray(static_cast<int>(code_table_size), TENURED);

  // Check whether lazy compilation is enabled for this module.
  bool lazy_compile = compile_lazy(module_.get());

  // If lazy compile: Initialize the code table with the lazy compile builtin.
  // Otherwise: Initialize with the illegal builtin. All call sites will be
  // patched at instantiation.
  Handle<Code> init_builtin = lazy_compile
                                  ? isolate_->builtins()->WasmCompileLazy()
                                  : isolate_->builtins()->Illegal();
  for (int i = 0, e = static_cast<int>(module_->functions.size()); i < e; ++i) {
    code_table->set(i, *init_builtin);
    temp_instance->function_code[i] = init_builtin;
  }

  (module_->is_wasm() ? counters()->wasm_functions_per_wasm_module()
                      : counters()->wasm_functions_per_asm_module())
      ->AddSample(static_cast<int>(module_->functions.size()));

  if (!lazy_compile) {
    size_t funcs_to_compile =
        module_->functions.size() - module_->num_imported_functions;
    bool compile_parallel =
        !FLAG_trace_wasm_decoder && FLAG_wasm_num_compilation_tasks > 0 &&
        funcs_to_compile > 1 &&
        V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads() > 0;
    if (compile_parallel) {
      // Avoid a race condition by collecting results into a second vector.
      std::vector<Handle<Code>> results(temp_instance->function_code);
      CompileInParallel(&module_env, results, thrower);
      temp_instance->function_code.swap(results);
    } else {
      CompileSequentially(&module_env, temp_instance->function_code, thrower);
    }
  } else if (module_->is_wasm()) {
    // Validate wasm modules for lazy compilation. Don't validate asm.js
    // modules, they are valid by construction (otherwise a CHECK will fail
    // during lazy compilation).
    // TODO(clemensh): According to the spec, we can actually skip validation
    // at module creation time, and return a function that always traps at
    // (lazy) compilation time.
    ValidateSequentially(&module_env, thrower);
  }
  if (thrower->error()) return {};

  // At this point, compilation has completed. Update the code table.
  for (size_t i = FLAG_skip_compiling_wasm_funcs;
       i < temp_instance->function_code.size(); ++i) {
    Code* code = *temp_instance->function_code[i];
    code_table->set(static_cast<int>(i), code);
    RecordStats(code, counters());
  }

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
      WasmModuleWrapper::New(isolate_, module_.release());
  WasmModule* module = module_wrapper->get();

  // Create the shared module data.
  // TODO(clemensh): For the same module (same bytes / same hash), we should
  // only have one WasmSharedModuleData. Otherwise, we might only set
  // breakpoints on a (potentially empty) subset of the instances.

  Handle<WasmSharedModuleData> shared = WasmSharedModuleData::New(
      isolate_, module_wrapper, Handle<SeqOneByteString>::cast(module_bytes),
      script, asm_js_offset_table);
  if (lazy_compile) WasmSharedModuleData::PrepareForLazyCompilation(shared);

  // Create the compiled module object, and populate with compiled functions
  // and information needed at instantiation time. This object needs to be
  // serializable. Instantiation may occur off a deserialized version of this
  // object.
  Handle<WasmCompiledModule> compiled_module = WasmCompiledModule::New(
      isolate_, shared, code_table, *function_tables, *signature_tables);

  // If we created a wasm script, finish it now and make it public to the
  // debugger.
  if (asm_js_script.is_null()) {
    script->set_wasm_compiled_module(*compiled_module);
    isolate_->debug()->OnAfterCompile(script);
  }

  // Compile JS->wasm wrappers for exported functions.
  JSToWasmWrapperCache js_to_wasm_cache;
  int func_index = 0;
  for (auto exp : module->export_table) {
    if (exp.kind != kExternalFunction) continue;
    Handle<Code> wasm_code = EnsureExportedLazyDeoptData(
        isolate_, Handle<WasmInstanceObject>::null(), code_table, exp.index);
    Handle<Code> wrapper_code = js_to_wasm_cache.CloneOrCompileJSToWasmWrapper(
        isolate_, module, wasm_code, exp.index);
    int export_index = static_cast<int>(module->functions.size() + func_index);
    code_table->set(export_index, *wrapper_code);
    RecordStats(*wrapper_code, counters());
    func_index++;
  }

  return WasmModuleObject::New(isolate_, compiled_module);
}

Handle<Code> JSToWasmWrapperCache::CloneOrCompileJSToWasmWrapper(
    Isolate* isolate, const wasm::WasmModule* module, Handle<Code> wasm_code,
    uint32_t index) {
  const wasm::WasmFunction* func = &module->functions[index];
  int cached_idx = sig_map_.Find(func->sig);
  if (cached_idx >= 0) {
    Handle<Code> code = isolate->factory()->CopyCode(code_cache_[cached_idx]);
    // Now patch the call to wasm code.
    for (RelocIterator it(*code, RelocInfo::kCodeTargetMask);; it.next()) {
      DCHECK(!it.done());
      Code* target =
          Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
      if (target->kind() == Code::WASM_FUNCTION ||
          target->kind() == Code::WASM_TO_JS_FUNCTION ||
          target->builtin_index() == Builtins::kIllegal ||
          target->builtin_index() == Builtins::kWasmCompileLazy) {
        it.rinfo()->set_target_address(isolate, wasm_code->instruction_start());
        break;
      }
    }
    return code;
  }

  Handle<Code> code =
      compiler::CompileJSToWasmWrapper(isolate, module, wasm_code, index);
  uint32_t new_cache_idx = sig_map_.FindOrInsert(func->sig);
  DCHECK_EQ(code_cache_.size(), new_cache_idx);
  USE(new_cache_idx);
  code_cache_.push_back(code);
  return code;
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
      instance_finalizer_callback_(instance_finalizer_callback) {}

// Build an instance, in all of its glory.
MaybeHandle<WasmInstanceObject> InstanceBuilder::Build() {
  // Check that an imports argument was provided, if the module requires it.
  // No point in continuing otherwise.
  if (!module_->import_table.empty() && ffi_.is_null()) {
    thrower_->TypeError(
        "Imports argument must be present and must be an object");
    return {};
  }

  // Record build time into correct bucket, then build instance.
  TimedHistogramScope wasm_instantiate_module_time_scope(
      module_->is_wasm() ? counters()->wasm_instantiate_wasm_module_time()
                         : counters()->wasm_instantiate_asm_module_time());
  Factory* factory = isolate_->factory();

  //--------------------------------------------------------------------------
  // Reuse the compiled module (if no owner), otherwise clone.
  //--------------------------------------------------------------------------
  Handle<FixedArray> code_table;
  // We keep around a copy of the old code table, because we'll be replacing
  // imports for the new instance, and then we need the old imports to be
  // able to relocate.
  Handle<FixedArray> old_code_table;
  MaybeHandle<WasmInstanceObject> owner;

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
      TRACE("Cloning from %d\n", original->instance_id());
      old_code_table = original->code_table();
      compiled_module_ = WasmCompiledModule::Clone(isolate_, original);
      code_table = compiled_module_->code_table();
      // Avoid creating too many handles in the outer scope.
      HandleScope scope(isolate_);

      // Clone the code for wasm functions and exports.
      for (int i = 0; i < code_table->length(); ++i) {
        Handle<Code> orig_code(Code::cast(code_table->get(i)), isolate_);
        switch (orig_code->kind()) {
          case Code::WASM_TO_JS_FUNCTION:
            // Imports will be overwritten with newly compiled wrappers.
            break;
          case Code::BUILTIN:
            DCHECK_EQ(Builtins::kWasmCompileLazy, orig_code->builtin_index());
            // If this code object has deoptimization data, then we need a
            // unique copy to attach updated deoptimization data.
            if (orig_code->deoptimization_data()->length() > 0) {
              Handle<Code> code = factory->CopyCode(orig_code);
              Handle<FixedArray> deopt_data =
                  factory->NewFixedArray(2, TENURED);
              deopt_data->set(1, Smi::FromInt(i));
              code->set_deoptimization_data(*deopt_data);
              code_table->set(i, *code);
            }
            break;
          case Code::JS_TO_WASM_FUNCTION:
          case Code::WASM_FUNCTION: {
            Handle<Code> code = factory->CopyCode(orig_code);
            code_table->set(i, *code);
            break;
          }
          default:
            UNREACHABLE();
        }
      }
      RecordStats(code_table, counters());
    } else {
      // There was no owner, so we can reuse the original.
      compiled_module_ = original;
      old_code_table = factory->CopyFixedArray(compiled_module_->code_table());
      code_table = compiled_module_->code_table();
      TRACE("Reusing existing instance %d\n", compiled_module_->instance_id());
    }
    compiled_module_->set_native_context(isolate_->native_context());
  }

  //--------------------------------------------------------------------------
  // Allocate the instance object.
  //--------------------------------------------------------------------------
  Zone instantiation_zone(isolate_->allocator(), ZONE_NAME);
  CodeSpecialization code_specialization(isolate_, &instantiation_zone);
  Handle<WasmInstanceObject> instance =
      WasmInstanceObject::New(isolate_, compiled_module_);

  //--------------------------------------------------------------------------
  // Set up the globals for the new instance.
  //--------------------------------------------------------------------------
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
    Address old_globals_start = compiled_module_->GetGlobalsStartOrNull();
    Address new_globals_start =
        static_cast<Address>(global_buffer->backing_store());
    code_specialization.RelocateGlobals(old_globals_start, new_globals_start);
    // The address of the backing buffer for the golbals is in native memory
    // and, thus, not moving. We need it saved for
    // serialization/deserialization purposes - so that the other end
    // understands how to relocate the references. We still need to save the
    // JSArrayBuffer on the instance, to keep it all alive.
    WasmCompiledModule::SetGlobalsStartAddressFrom(factory, compiled_module_,
                                                   global_buffer);
    instance->set_globals_buffer(*global_buffer);
  }

  //--------------------------------------------------------------------------
  // Prepare for initialization of function tables.
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
  // Set up the indirect function tables for the new instance.
  //--------------------------------------------------------------------------
  if (function_table_count > 0)
    InitializeTables(instance, &code_specialization);

  //--------------------------------------------------------------------------
  // Set up the memory for the new instance.
  //--------------------------------------------------------------------------
  uint32_t min_mem_pages = module_->min_mem_pages;
  (module_->is_wasm() ? counters()->wasm_wasm_min_mem_pages_count()
                      : counters()->wasm_asm_min_mem_pages_count())
      ->AddSample(min_mem_pages);

  if (!memory_.is_null()) {
    Handle<JSArrayBuffer> memory = memory_.ToHandleChecked();
    // Set externally passed ArrayBuffer non neuterable.
    memory->set_is_neuterable(false);
    memory->set_is_wasm_buffer(true);

    DCHECK_IMPLIES(EnableGuardRegions(),
                   module_->is_asm_js() || memory->has_guard_region());
  } else if (min_mem_pages > 0) {
    memory_ = AllocateMemory(min_mem_pages);
    if (memory_.is_null()) return {};  // failed to allocate memory
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
    uint32_t mem_size = 0;
    if (!memory_.is_null()) {
      CHECK(memory_.ToHandleChecked()->byte_length()->ToUint32(&mem_size));
    }
    if (!in_bounds(base, seg.source.length(), mem_size)) {
      thrower_->LinkError("data segment is out of bounds");
      return {};
    }
  }

  //--------------------------------------------------------------------------
  // Initialize memory.
  //--------------------------------------------------------------------------
  if (!memory_.is_null()) {
    Handle<JSArrayBuffer> memory = memory_.ToHandleChecked();
    Address mem_start = static_cast<Address>(memory->backing_store());
    uint32_t mem_size;
    CHECK(memory->byte_length()->ToUint32(&mem_size));
    LoadDataSegments(mem_start, mem_size);

    uint32_t old_mem_size = compiled_module_->mem_size();
    Address old_mem_start = compiled_module_->GetEmbeddedMemStartOrNull();
    // We might get instantiated again with the same memory. No patching
    // needed in this case.
    if (old_mem_start != mem_start || old_mem_size != mem_size) {
      code_specialization.RelocateMemoryReferences(old_mem_start, old_mem_size,
                                                   mem_start, mem_size);
    }
    // Just like with globals, we need to keep both the JSArrayBuffer
    // and save the start pointer.
    instance->set_memory_buffer(*memory);
    WasmCompiledModule::SetSpecializationMemInfoFrom(factory, compiled_module_,
                                                     memory);
  }

  //--------------------------------------------------------------------------
  // Set up the runtime support for the new instance.
  //--------------------------------------------------------------------------
  Handle<WeakCell> weak_link = factory->NewWeakCell(instance);

  for (int i = num_imported_functions + FLAG_skip_compiling_wasm_funcs,
           num_functions = static_cast<int>(module_->functions.size());
       i < num_functions; ++i) {
    Handle<Code> code = handle(Code::cast(code_table->get(i)), isolate_);
    if (code->kind() == Code::WASM_FUNCTION) {
      Handle<FixedArray> deopt_data = factory->NewFixedArray(2, TENURED);
      deopt_data->set(0, *weak_link);
      deopt_data->set(1, Smi::FromInt(i));
      code->set_deoptimization_data(*deopt_data);
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

  //--------------------------------------------------------------------------
  // Set up the exports object for the new instance.
  //--------------------------------------------------------------------------
  ProcessExports(code_table, instance, compiled_module_);
  if (thrower_->error()) return {};

  //--------------------------------------------------------------------------
  // Add instance to Memory object
  //--------------------------------------------------------------------------
  if (instance->has_memory_object()) {
    Handle<WasmMemoryObject> memory(instance->memory_object(), isolate_);
    WasmMemoryObject::AddInstance(isolate_, memory, instance);
  }

  //--------------------------------------------------------------------------
  // Initialize the indirect function tables.
  //--------------------------------------------------------------------------
  if (function_table_count > 0) LoadTableSegments(code_table, instance);

  // Patch all code with the relocations registered in code_specialization.
  code_specialization.RelocateDirectCalls(instance);
  code_specialization.ApplyToWholeInstance(*instance, SKIP_ICACHE_FLUSH);

  FlushICache(isolate_, code_table);

  //--------------------------------------------------------------------------
  // Unpack and notify signal handler of protected instructions.
  //--------------------------------------------------------------------------
  if (trap_handler::UseTrapHandler()) {
    UnpackAndRegisterProtectedInstructions(isolate_, code_table);
  }

  //--------------------------------------------------------------------------
  // Set up and link the new instance.
  //--------------------------------------------------------------------------
  {
    Handle<Object> global_handle =
        isolate_->global_handles()->Create(*instance);
    Handle<WeakCell> link_to_clone = factory->NewWeakCell(compiled_module_);
    Handle<WeakCell> link_to_owning_instance = factory->NewWeakCell(instance);
    MaybeHandle<WeakCell> link_to_original;
    MaybeHandle<WasmCompiledModule> original;
    if (!owner.is_null()) {
      // prepare the data needed for publishing in a chain, but don't link
      // just yet, because
      // we want all the publishing to happen free from GC interruptions, and
      // so we do it in
      // one GC-free scope afterwards.
      original = handle(owner.ToHandleChecked()->compiled_module());
      link_to_original = factory->NewWeakCell(original.ToHandleChecked());
    }
    // Publish the new instance to the instances chain.
    {
      DisallowHeapAllocation no_gc;
      if (!link_to_original.is_null()) {
        compiled_module_->set_weak_next_instance(
            link_to_original.ToHandleChecked());
        original.ToHandleChecked()->set_weak_prev_instance(link_to_clone);
        compiled_module_->set_weak_wasm_module(
            original.ToHandleChecked()->weak_wasm_module());
      }
      module_object_->set_compiled_module(*compiled_module_);
      compiled_module_->set_weak_owning_instance(link_to_owning_instance);
      GlobalHandles::MakeWeak(
          global_handle.location(), global_handle.location(),
          instance_finalizer_callback_, v8::WeakCallbackType::kFinalizer);
    }
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
  // Run the start function if one was specified.
  //--------------------------------------------------------------------------
  if (module_->start_function_index >= 0) {
    HandleScope scope(isolate_);
    int start_index = module_->start_function_index;
    Handle<Code> startup_code = EnsureExportedLazyDeoptData(
        isolate_, instance, code_table, start_index);
    FunctionSig* sig = module_->functions[start_index].sig;
    Handle<Code> wrapper_code = js_to_wasm_cache_.CloneOrCompileJSToWasmWrapper(
        isolate_, module_, startup_code, start_index);
    Handle<WasmExportedFunction> startup_fct = WasmExportedFunction::New(
        isolate_, instance, MaybeHandle<String>(), start_index,
        static_cast<int>(sig->parameter_count()), wrapper_code);
    RecordStats(*startup_code, counters());
    // Call the JS function.
    Handle<Object> undefined = factory->undefined_value();
    MaybeHandle<Object> retval =
        Execution::Call(isolate_, startup_fct, undefined, 0, nullptr);

    if (retval.is_null()) {
      DCHECK(isolate_->has_pending_exception());
      // It's unfortunate that the new instance is already linked in the
      // chain. However, we need to set up everything before executing the
      // start function, such that stack trace information can be generated
      // correctly already in the start function.
      return {};
    }
  }

  DCHECK(!isolate_->has_pending_exception());
  TRACE("Finishing instance %d\n", compiled_module_->instance_id());
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
void InstanceBuilder::LoadDataSegments(Address mem_addr, size_t mem_size) {
  Handle<SeqOneByteString> module_bytes(compiled_module_->module_bytes(),
                                        isolate_);
  for (const WasmDataSegment& segment : module_->data_segments) {
    uint32_t source_size = segment.source.length();
    // Segments of size == 0 are just nops.
    if (source_size == 0) continue;
    uint32_t dest_offset = EvalUint32InitExpr(segment.dest_addr);
    DCHECK(
        in_bounds(dest_offset, source_size, static_cast<uint32_t>(mem_size)));
    byte* dest = mem_addr + dest_offset;
    const byte* src = reinterpret_cast<const byte*>(
        module_bytes->GetCharsAddress() + segment.source.offset());
    memcpy(dest, src, source_size);
  }
}

void InstanceBuilder::WriteGlobalValue(WasmGlobal& global,
                                       Handle<Object> value) {
  double num = value->Number();
  TRACE("init [globals+%u] = %lf, type = %s\n", global.offset, num,
        WasmOpcodes::TypeName(global.type));
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

// Process the imports, including functions, tables, globals, and memory, in
// order, loading them from the {ffi_} object. Returns the number of imported
// functions.
int InstanceBuilder::ProcessImports(Handle<FixedArray> code_table,
                                    Handle<WasmInstanceObject> instance) {
  int num_imported_functions = 0;
  int num_imported_tables = 0;
  WasmInstanceMap imported_wasm_instances(isolate_->heap());
  for (int index = 0; index < static_cast<int>(module_->import_table.size());
       ++index) {
    WasmImport& import = module_->import_table[index];

    Handle<String> module_name;
    MaybeHandle<String> maybe_module_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate_, compiled_module_, import.module_name);
    if (!maybe_module_name.ToHandle(&module_name)) return -1;

    Handle<String> import_name;
    MaybeHandle<String> maybe_import_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate_, compiled_module_, import.field_name);
    if (!maybe_import_name.ToHandle(&import_name)) return -1;

    MaybeHandle<Object> result =
        module_->is_asm_js() ? LookupImportAsm(index, import_name)
                             : LookupImport(index, module_name, import_name);
    if (thrower_->error()) return -1;
    Handle<Object> value = result.ToHandleChecked();

    switch (import.kind) {
      case kExternalFunction: {
        // Function imports must be callable.
        if (!value->IsCallable()) {
          ReportLinkError("function import requires a callable", index,
                          module_name, import_name);
          return -1;
        }

        Handle<Code> import_wrapper = UnwrapOrCompileImportWrapper(
            isolate_, index, module_->functions[import.index].sig,
            Handle<JSReceiver>::cast(value), module_name, import_name,
            module_->origin(), &imported_wasm_instances);
        if (import_wrapper.is_null()) {
          ReportLinkError("imported function does not match the expected type",
                          index, module_name, import_name);
          return -1;
        }
        code_table->set(num_imported_functions, *import_wrapper);
        RecordStats(*import_wrapper, counters());
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
        table_instance.js_wrappers = Handle<FixedArray>(
            table_instance.table_object->functions(), isolate_);

        int imported_cur_size = table_instance.js_wrappers->length();
        if (imported_cur_size < static_cast<int>(table.min_size)) {
          thrower_->LinkError(
              "table import %d is smaller than minimum %d, got %u", index,
              table.min_size, imported_cur_size);
          return -1;
        }

        if (table.has_max) {
          int64_t imported_max_size =
              table_instance.table_object->maximum_length()->Number();
          if (imported_max_size < 0) {
            thrower_->LinkError(
                "table import %d has no maximum length, expected %d", index,
                table.max_size);
            return -1;
          }
          if (imported_max_size > table.max_size) {
            thrower_->LinkError(
                "table import %d has maximum larger than maximum %d, "
                "got %" PRIx64,
                index, table.max_size, imported_max_size);
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
          if (!val->IsJSFunction()) continue;
          WasmFunction* function =
              GetWasmFunctionForImportWrapper(isolate_, val);
          if (function == nullptr) {
            thrower_->LinkError("table import %d[%d] is not a wasm function",
                                index, i);
            return -1;
          }
          int sig_index = table.map.FindOrInsert(function->sig);
          table_instance.signature_table->set(i, Smi::FromInt(sig_index));
          table_instance.function_table->set(i, *UnwrapImportWrapper(val));
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
        if (imported_cur_pages < module_->min_mem_pages) {
          thrower_->LinkError(
              "memory import %d is smaller than maximum %u, got %u", index,
              module_->min_mem_pages, imported_cur_pages);
        }
        int32_t imported_max_pages = memory->maximum_pages();
        if (module_->has_max_mem) {
          if (imported_max_pages < 0) {
            thrower_->LinkError(
                "memory import %d has no maximum limit, expected at most %u",
                index, imported_max_pages);
            return -1;
          }
          if (static_cast<uint32_t>(imported_max_pages) >
              module_->max_mem_pages) {
            thrower_->LinkError(
                "memory import %d has larger maximum than maximum %u, got %d",
                index, module_->max_mem_pages, imported_max_pages);
            return -1;
          }
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
Handle<JSArrayBuffer> InstanceBuilder::AllocateMemory(uint32_t min_mem_pages) {
  if (min_mem_pages > FLAG_wasm_max_mem_pages) {
    thrower_->RangeError("Out of memory: wasm memory too large");
    return Handle<JSArrayBuffer>::null();
  }
  const bool enable_guard_regions = EnableGuardRegions();
  Handle<JSArrayBuffer> mem_buffer = NewArrayBuffer(
      isolate_, min_mem_pages * WasmModule::kPageSize, enable_guard_regions);

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
    Handle<FixedArray> code_table, Handle<WasmInstanceObject> instance,
    Handle<WasmCompiledModule> compiled_module) {
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
  Handle<String> exports_name =
      isolate_->factory()->InternalizeUtf8String("exports");
  JSObject::AddProperty(instance, exports_name, exports_object, NONE);

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
        int func_index =
            static_cast<int>(module_->functions.size() + export_index);
        Handle<JSFunction> js_function = js_wrappers_[exp.index];
        if (js_function.is_null()) {
          // Wrap the exported code as a JSFunction.
          Handle<Code> export_code =
              code_table->GetValueChecked<Code>(isolate_, func_index);
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
          uint32_t maximum =
              table.has_max ? table.max_size : FLAG_wasm_max_table_size;
          table_instance.table_object = WasmTableObject::New(
              isolate_, table.min_size, maximum, &table_instance.js_wrappers);
        }
        desc.set_value(table_instance.table_object);
        break;
      }
      case kExternalMemory: {
        // Export the memory as a WebAssembly.Memory object.
        Handle<WasmMemoryObject> memory_object;
        if (!instance->has_memory_object()) {
          // If there was no imported WebAssembly.Memory object, create one.
          memory_object = WasmMemoryObject::New(
              isolate_,
              (instance->has_memory_buffer())
                  ? handle(instance->memory_buffer())
                  : Handle<JSArrayBuffer>::null(),
              (module_->max_mem_pages != 0) ? module_->max_mem_pages : -1);
          instance->set_memory_object(*memory_object);
        } else {
          memory_object =
              Handle<WasmMemoryObject>(instance->memory_object(), isolate_);
        }

        desc.set_value(memory_object);
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
        isolate_, export_to, name, &desc, Object::THROW_ON_ERROR);
    if (!status.IsJust()) {
      thrower_->LinkError("export of %.*s failed.", name->length(),
                          name->ToCString().get());
      return;
    }
  }
  DCHECK_EQ(export_index, weak_exported_functions->length());

  if (module_->is_wasm()) {
    v8::Maybe<bool> success = JSReceiver::SetIntegrityLevel(
        exports_object, FROZEN, Object::DONT_THROW);
    DCHECK(success.FromMaybe(false));
    USE(success);
  }
}

void InstanceBuilder::InitializeTables(
    Handle<WasmInstanceObject> instance,
    CodeSpecialization* code_specialization) {
  int function_table_count = static_cast<int>(module_->function_tables.size());
  Handle<FixedArray> new_function_tables =
      isolate_->factory()->NewFixedArray(function_table_count);
  Handle<FixedArray> new_signature_tables =
      isolate_->factory()->NewFixedArray(function_table_count);
  for (int index = 0; index < function_table_count; ++index) {
    WasmIndirectFunctionTable& table = module_->function_tables[index];
    TableInstance& table_instance = table_instances_[index];
    int table_size = static_cast<int>(table.min_size);

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
      DCHECK(table_size <= table_instance.function_table->length());
      if (table_size < table_instance.function_table->length()) {
        code_specialization->PatchTableSize(
            table_size, table_instance.function_table->length());
      }
    }

    new_function_tables->set(static_cast<int>(index),
                             *table_instance.function_table);
    new_signature_tables->set(static_cast<int>(index),
                              *table_instance.signature_table);
  }

  FixedArray* old_function_tables = compiled_module_->ptr_to_function_tables();
  DCHECK_EQ(old_function_tables->length(), new_function_tables->length());
  for (int i = 0, e = new_function_tables->length(); i < e; ++i) {
    code_specialization->RelocateObject(
        handle(old_function_tables->get(i), isolate_),
        handle(new_function_tables->get(i), isolate_));
  }
  FixedArray* old_signature_tables =
      compiled_module_->ptr_to_signature_tables();
  DCHECK_EQ(old_signature_tables->length(), new_signature_tables->length());
  for (int i = 0, e = new_signature_tables->length(); i < e; ++i) {
    code_specialization->RelocateObject(
        handle(old_signature_tables->get(i), isolate_),
        handle(new_signature_tables->get(i), isolate_));
  }

  compiled_module_->set_function_tables(new_function_tables);
  compiled_module_->set_signature_tables(new_signature_tables);
}

void InstanceBuilder::LoadTableSegments(Handle<FixedArray> code_table,
                                        Handle<WasmInstanceObject> instance) {
  int function_table_count = static_cast<int>(module_->function_tables.size());
  for (int index = 0; index < function_table_count; ++index) {
    WasmIndirectFunctionTable& table = module_->function_tables[index];
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
      for (auto table_init : module_->table_inits) {
        for (uint32_t func_index : table_init.entries) {
          Code* code =
              Code::cast(code_table->get(static_cast<int>(func_index)));
          // Only increase the counter for lazy compile builtins (it's not
          // needed otherwise).
          if (code->is_wasm_code()) continue;
          DCHECK_EQ(Builtins::kWasmCompileLazy, code->builtin_index());
          ++num_table_exports[func_index];
        }
      }
    }

    // TODO(titzer): this does redundant work if there are multiple tables,
    // since initializations are not sorted by table index.
    for (auto table_init : module_->table_inits) {
      uint32_t base = EvalUint32InitExpr(table_init.offset);
      uint32_t num_entries = static_cast<uint32_t>(table_init.entries.size());
      DCHECK(in_bounds(base, num_entries,
                       table_instance.function_table->length()));
      for (uint32_t i = 0; i < num_entries; ++i) {
        uint32_t func_index = table_init.entries[i];
        WasmFunction* function = &module_->functions[func_index];
        int table_index = static_cast<int>(i + base);
        int32_t sig_index = table.map.Find(function->sig);
        DCHECK_GE(sig_index, 0);
        table_instance.signature_table->set(table_index,
                                            Smi::FromInt(sig_index));
        Handle<Code> wasm_code = EnsureTableExportLazyDeoptData(
            isolate_, instance, code_table, func_index,
            table_instance.function_table, table_index, num_table_exports);
        table_instance.function_table->set(table_index, *wasm_code);

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

          UpdateDispatchTables(isolate_, all_dispatch_tables, table_index,
                               function, wasm_code);
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
  // The handles for the context and promise must be deferred.
  DeferredHandleScope deferred(isolate);
  context_ = Handle<Context>(*context);
  module_promise_ = Handle<JSPromise>(*promise);
  deferred_handles_.push_back(deferred.Detach());
}

void AsyncCompileJob::Start() {
  DoAsync<DecodeModule>();  // --
}

AsyncCompileJob::~AsyncCompileJob() {
  background_task_manager_.CancelAndWait();
  for (auto d : deferred_handles_) delete d;
}

void AsyncCompileJob::ReopenHandlesInDeferredScope() {
  DeferredHandleScope deferred(isolate_);
  function_tables_ = handle(*function_tables_, isolate_);
  signature_tables_ = handle(*signature_tables_, isolate_);
  code_table_ = handle(*code_table_, isolate_);
  temp_instance_->ReopenHandles(isolate_);
  compiler_->ReopenHandlesInDeferredScope();
  deferred_handles_.push_back(deferred.Detach());
}

void AsyncCompileJob::AsyncCompileFailed(ErrorThrower& thrower) {
  RejectPromise(isolate_, context_, thrower, module_promise_);
  isolate_->wasm_compilation_manager()->RemoveJob(this);
}

void AsyncCompileJob::AsyncCompileSucceeded(Handle<Object> result) {
  ResolvePromise(isolate_, context_, module_promise_, result);
  isolate_->wasm_compilation_manager()->RemoveJob(this);
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
      DCHECK_EQ(1, job_->num_pending_foreground_tasks_--);
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
  DCHECK_EQ(0, num_pending_foreground_tasks_++);

  V8::GetCurrentPlatform()->CallOnForegroundThread(
      reinterpret_cast<v8::Isolate*>(isolate_), new CompileTask(this, true));
}

template <typename State, typename... Args>
void AsyncCompileJob::DoSync(Args&&... args) {
  step_.reset(new State(std::forward<Args>(args)...));
  step_->job_ = this;
  StartForegroundTask();
}

void AsyncCompileJob::StartBackgroundTask() {
  V8::GetCurrentPlatform()->CallOnBackgroundThread(
      new CompileTask(this, false), v8::Platform::kShortRunningTask);
}

template <typename State, typename... Args>
void AsyncCompileJob::DoAsync(Args&&... args) {
  step_.reset(new State(std::forward<Args>(args)...));
  step_->job_ = this;
  size_t end = step_->NumberOfBackgroundTasks();
  for (size_t i = 0; i < end; ++i) {
    StartBackgroundTask();
  }
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
      job_->DoSync<PrepareAndStartCompile>(std::move(result.val));
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
    HandleScope scope(job_->isolate_);
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
  explicit PrepareAndStartCompile(std::unique_ptr<WasmModule> module)
      : module_(std::move(module)) {}

 private:
  std::unique_ptr<WasmModule> module_;
  void RunInForeground() override {
    TRACE_COMPILE("(2) Prepare and start compile...\n");
    HandleScope scope(job_->isolate_);

    Factory* factory = job_->isolate_->factory();
    job_->temp_instance_.reset(new WasmInstance(module_.get()));
    job_->temp_instance_->context = job_->context_;
    job_->temp_instance_->mem_size =
        WasmModule::kPageSize * module_->min_mem_pages;
    job_->temp_instance_->mem_start = nullptr;
    job_->temp_instance_->globals_start = nullptr;

    // Initialize the indirect tables with placeholders.
    int function_table_count =
        static_cast<int>(module_->function_tables.size());
    job_->function_tables_ =
        factory->NewFixedArray(function_table_count, TENURED);
    job_->signature_tables_ =
        factory->NewFixedArray(function_table_count, TENURED);
    for (int i = 0; i < function_table_count; ++i) {
      job_->temp_instance_->function_tables[i] =
          factory->NewFixedArray(1, TENURED);
      job_->temp_instance_->signature_tables[i] =
          factory->NewFixedArray(1, TENURED);
      job_->function_tables_->set(i, *job_->temp_instance_->function_tables[i]);
      job_->signature_tables_->set(i,
                                   *job_->temp_instance_->signature_tables[i]);
    }

    // The {code_table} array contains import wrappers and functions (which
    // are both included in {functions.size()}, and export wrappers.
    // The results of compilation will be written into it.
    int code_table_size = static_cast<int>(module_->functions.size() +
                                           module_->num_exported_functions);
    job_->code_table_ = factory->NewFixedArray(code_table_size, TENURED);

    // Initialize {code_table_} with the illegal builtin. All call sites
    // will be patched at instantiation.
    Handle<Code> illegal_builtin = job_->isolate_->builtins()->Illegal();
    // TODO(wasm): Fix this for lazy compilation.
    for (uint32_t i = 0; i < module_->functions.size(); ++i) {
      job_->code_table_->set(static_cast<int>(i), *illegal_builtin);
      job_->temp_instance_->function_code[i] = illegal_builtin;
    }

    job_->counters()->wasm_functions_per_wasm_module()->AddSample(
        static_cast<int>(module_->functions.size()));

    // Transfer ownership of the {WasmModule} to the {ModuleCompiler}, but
    // keep a pointer.
    WasmModule* module = module_.get();
    job_->compiler_.reset(
        new ModuleCompiler(job_->isolate_, std::move(module_)));
    job_->compiler_->EnableThrottling();

    // Reopen all handles which should survive in the DeferredHandleScope.
    job_->ReopenHandlesInDeferredScope();

    DCHECK_LE(module->num_imported_functions, module->functions.size());
    size_t num_functions =
        module->functions.size() - module->num_imported_functions;
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
    job_->module_bytes_env_.reset(new ModuleBytesEnv(
        module, job_->temp_instance_.get(), job_->wire_bytes_));

    job_->outstanding_units_ = job_->compiler_->InitializeCompilationUnits(
        module->functions, *job_->module_bytes_env_);

    job_->DoAsync<ExecuteAndFinishCompilationUnits>(num_background_tasks);
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
    stopped_tasks_.Increment(1);
  }

  void RestartCompilationTasks() {
    size_t num_restarts = stopped_tasks_.Value();
    stopped_tasks_.Decrement(num_restarts);

    for (size_t i = 0; i < num_restarts; ++i) {
      job_->StartBackgroundTask();
    }
  }

  void RunInForeground() override {
    TRACE_COMPILE("(4a) Finishing compilation units...\n");
    if (failed_) {
      // The job failed already, no need to do more work.
      job_->compiler_->SetFinisherIsRunning(false);
      return;
    }
    HandleScope scope(job_->isolate_);
    ErrorThrower thrower(job_->isolate_, "AsyncCompile");

    // We execute for 1 ms and then reschedule the task, same as the GC.
    double deadline = MonotonicallyIncreasingTimeInMs() + 1.0;

    while (true) {
      if (!finished_ && job_->compiler_->ShouldIncreaseWorkload()) {
        RestartCompilationTasks();
      }

      int func_index = -1;

      MaybeHandle<Code> result =
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
        DCHECK(func_index >= 0);
        job_->code_table_->set(func_index, *result.ToHandleChecked());
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
      return job_->AsyncCompileFailed(thrower);
    }
    if (job_->outstanding_units_ == 0) {
      // Make sure all compilation tasks stopped running.
      job_->background_task_manager_.CancelAndWait();
      job_->DoSync<FinishCompile>();
    }
  }

 private:
  std::atomic<bool> failed_{false};
  std::atomic<bool> finished_{false};
  base::AtomicNumber<size_t> stopped_tasks_{0};
};

//==========================================================================
// Step 5 (sync): Finish heap-allocated data structures.
//==========================================================================
class AsyncCompileJob::FinishCompile : public CompileStep {
  void RunInForeground() override {
    TRACE_COMPILE("(5b) Finish compile...\n");
    HandleScope scope(job_->isolate_);
    // At this point, compilation has completed. Update the code table.
    for (size_t i = FLAG_skip_compiling_wasm_funcs;
         i < job_->temp_instance_->function_code.size(); ++i) {
      Code* code = Code::cast(job_->code_table_->get(static_cast<int>(i)));
      RecordStats(code, job_->counters());
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
    Handle<WasmModuleWrapper> module_wrapper = WasmModuleWrapper::New(
        job_->isolate_, job_->compiler_->ReleaseModule().release());

    // Create the shared module data.
    // TODO(clemensh): For the same module (same bytes / same hash), we should
    // only have one WasmSharedModuleData. Otherwise, we might only set
    // breakpoints on a (potentially empty) subset of the instances.

    Handle<WasmSharedModuleData> shared =
        WasmSharedModuleData::New(job_->isolate_, module_wrapper,
                                  Handle<SeqOneByteString>::cast(module_bytes),
                                  script, asm_js_offset_table);

    // Create the compiled module object and populate with compiled functions
    // and information needed at instantiation time. This object needs to be
    // serializable. Instantiation may occur off a deserialized version of
    // this object.
    job_->compiled_module_ = WasmCompiledModule::New(
        job_->isolate_, shared, job_->code_table_, job_->function_tables_,
        job_->signature_tables_);

    // Finish the wasm script now and make it public to the debugger.
    script->set_wasm_compiled_module(*job_->compiled_module_);
    job_->isolate_->debug()->OnAfterCompile(script);

    DeferredHandleScope deferred(job_->isolate_);
    job_->compiled_module_ = handle(*job_->compiled_module_, job_->isolate_);
    job_->deferred_handles_.push_back(deferred.Detach());
    // TODO(wasm): compiling wrappers should be made async as well.
    job_->DoSync<CompileWrappers>();
  }
};

//==========================================================================
// Step 6 (sync): Compile JS->wasm wrappers.
//==========================================================================
class AsyncCompileJob::CompileWrappers : public CompileStep {
  void RunInForeground() override {
    TRACE_COMPILE("(6) Compile wrappers...\n");
    // Compile JS->wasm wrappers for exported functions.
    HandleScope scope(job_->isolate_);
    JSToWasmWrapperCache js_to_wasm_cache;
    int func_index = 0;
    WasmModule* module = job_->compiled_module_->module();
    for (auto exp : module->export_table) {
      if (exp.kind != kExternalFunction) continue;
      Handle<Code> wasm_code(Code::cast(job_->code_table_->get(exp.index)),
                             job_->isolate_);
      Handle<Code> wrapper_code =
          js_to_wasm_cache.CloneOrCompileJSToWasmWrapper(job_->isolate_, module,
                                                         wasm_code, exp.index);
      int export_index =
          static_cast<int>(module->functions.size() + func_index);
      job_->code_table_->set(export_index, *wrapper_code);
      RecordStats(*wrapper_code, job_->counters());
      func_index++;
    }

    job_->DoSync<FinishModule>();
  }
};

//==========================================================================
// Step 7 (sync): Finish the module and resolve the promise.
//==========================================================================
class AsyncCompileJob::FinishModule : public CompileStep {
  void RunInForeground() override {
    TRACE_COMPILE("(7) Finish module...\n");
    HandleScope scope(job_->isolate_);
    Handle<WasmModuleObject> result =
        WasmModuleObject::New(job_->isolate_, job_->compiled_module_);
    // {job_} is deleted in AsyncCompileSucceeded, therefore the {return}.
    return job_->AsyncCompileSucceeded(result);
  }
};
}  // namespace wasm
}  // namespace internal
}  // namespace v8

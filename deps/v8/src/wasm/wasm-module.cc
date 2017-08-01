// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "src/asmjs/asm-js.h"
#include "src/assembler-inl.h"
#include "src/base/atomic-utils.h"
#include "src/base/utils/random-number-generator.h"
#include "src/code-stubs.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/interface-types.h"
#include "src/frames-inl.h"
#include "src/objects.h"
#include "src/property-descriptor.h"
#include "src/simulator.h"
#include "src/snapshot/snapshot.h"
#include "src/trap-handler/trap-handler.h"
#include "src/v8.h"

#include "src/wasm/function-body-decoder.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-specialization.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

using namespace v8::internal;
using namespace v8::internal::wasm;
namespace base = v8::base;

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

namespace {

static const int kInvalidSigIndex = -1;

byte* raw_buffer_ptr(MaybeHandle<JSArrayBuffer> buffer, int offset) {
  return static_cast<byte*>(buffer.ToHandleChecked()->backing_store()) + offset;
}

static void RecordStats(Isolate* isolate, Code* code, bool is_sync) {
  if (is_sync) {
    // TODO(karlschimpf): Make this work when asynchronous.
    // https://bugs.chromium.org/p/v8/issues/detail?id=6361
    isolate->counters()->wasm_generated_code_size()->Increment(
        code->body_size());
    isolate->counters()->wasm_reloc_size()->Increment(
        code->relocation_info()->length());
  }
}

static void RecordStats(Isolate* isolate, Handle<FixedArray> functions,
                        bool is_sync) {
  DisallowHeapAllocation no_gc;
  for (int i = 0; i < functions->length(); ++i) {
    RecordStats(isolate, Code::cast(functions->get(i)), is_sync);
  }
}

void* TryAllocateBackingStore(Isolate* isolate, size_t size,
                              bool enable_guard_regions, void*& allocation_base,
                              size_t& allocation_length) {
  // TODO(eholk): Right now enable_guard_regions has no effect on 32-bit
  // systems. It may be safer to fail instead, given that other code might do
  // things that would be unsafe if they expected guard pages where there
  // weren't any.
  if (enable_guard_regions && kGuardRegionsSupported) {
    // TODO(eholk): On Windows we want to make sure we don't commit the guard
    // pages yet.

    // We always allocate the largest possible offset into the heap, so the
    // addressable memory after the guard page can be made inaccessible.
    allocation_length = RoundUp(kWasmMaxHeapOffset, base::OS::CommitPageSize());
    DCHECK_EQ(0, size % base::OS::CommitPageSize());

    // AllocateGuarded makes the whole region inaccessible by default.
    allocation_base =
        isolate->array_buffer_allocator()->Reserve(allocation_length);
    if (allocation_base == nullptr) {
      return nullptr;
    }

    void* memory = allocation_base;

    // Make the part we care about accessible.
    isolate->array_buffer_allocator()->SetProtection(
        memory, size, v8::ArrayBuffer::Allocator::Protection::kReadWrite);

    reinterpret_cast<v8::Isolate*>(isolate)
        ->AdjustAmountOfExternalAllocatedMemory(size);

    return memory;
  } else {
    void* memory = isolate->array_buffer_allocator()->Allocate(size);
    allocation_base = memory;
    allocation_length = size;
    return memory;
  }
}

void FlushICache(Isolate* isolate, Handle<FixedArray> code_table) {
  for (int i = 0; i < code_table->length(); ++i) {
    Handle<Code> code = code_table->GetValueChecked<Code>(isolate, i);
    Assembler::FlushICache(isolate, code->instruction_start(),
                           code->instruction_size());
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

class JSToWasmWrapperCache {
 public:
  Handle<Code> CloneOrCompileJSToWasmWrapper(Isolate* isolate,
                                             const wasm::WasmModule* module,
                                             Handle<Code> wasm_code,
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
          it.rinfo()->set_target_address(isolate,
                                         wasm_code->instruction_start());
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

 private:
  // sig_map_ maps signatures to an index in code_cache_.
  wasm::SignatureMap sig_map_;
  std::vector<Handle<Code>> code_cache_;
};

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
  DCHECK_EQ(func_index,
            Smi::cast(code->deoptimization_data()->get(1))->value());
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

bool compile_lazy(const WasmModule* module) {
  return FLAG_wasm_lazy_compilation ||
         (FLAG_asm_wasm_lazy_compilation && module->is_asm_js());
}

// A helper for compiling an entire module.
class CompilationHelper {
 public:
  // The compilation helper takes ownership of the {WasmModule}.
  // In {CompileToModuleObject}, it will transfer ownership to the generated
  // {WasmModuleWrapper}. If this method is not called, ownership may be
  // reclaimed by explicitely releasing the {module_} field.
  CompilationHelper(Isolate* isolate, std::unique_ptr<WasmModule> module,
                    bool is_sync)
      : isolate_(isolate),
        module_(std::move(module)),
        is_sync_(is_sync),
        executed_units_(
            isolate->random_number_generator(),
            (isolate->heap()->memory_allocator()->code_range()->valid()
                 ? isolate->heap()->memory_allocator()->code_range()->size()
                 : isolate->heap()->code_space()->Capacity()) /
                2),
        num_background_tasks_(Min(
            static_cast<size_t>(FLAG_wasm_num_compilation_tasks),
            V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads())),
        stopped_compilation_tasks_(num_background_tasks_) {}

  bool GetNextUncompiledFunctionId(size_t* index) {
    DCHECK_NOT_NULL(index);
    // - 1 because AtomicIncrement returns the value after the atomic increment.
    *index = next_unit_.Increment(1) - 1;
    return *index < compilation_units_.size();
  }

  // The actual runnable task that performs compilations in the background.
  class CompilationTask : public CancelableTask {
   public:
    CompilationHelper* helper_;
    explicit CompilationTask(CompilationHelper* helper)
        : CancelableTask(helper->isolate_, &helper->background_task_manager_),
          helper_(helper) {}

    void RunInternal() override {
      size_t index = 0;
      while (helper_->executed_units_.CanAcceptWork() &&
             helper_->GetNextUncompiledFunctionId(&index)) {
        helper_->CompileAndSchedule(index);
      }
      helper_->OnBackgroundTaskStopped();
    }
  };

  void OnBackgroundTaskStopped() {
    base::LockGuard<base::Mutex> guard(&tasks_mutex_);
    ++stopped_compilation_tasks_;
    DCHECK_LE(stopped_compilation_tasks_, num_background_tasks_);
  }

  void CompileAndSchedule(size_t index) {
    DisallowHeapAllocation no_allocation;
    DisallowHandleAllocation no_handles;
    DisallowHandleDereference no_deref;
    DisallowCodeDependencyChange no_dependency_change;
    DCHECK_LT(index, compilation_units_.size());

    std::unique_ptr<compiler::WasmCompilationUnit> unit =
        std::move(compilation_units_.at(index));
    unit->ExecuteCompilation();
    {
      base::LockGuard<base::Mutex> guard(&result_mutex_);
      executed_units_.Schedule(std::move(unit));
    }
  }

  class CodeGenerationSchedule {
   public:
    explicit CodeGenerationSchedule(
        base::RandomNumberGenerator* random_number_generator,
        size_t max_memory = 0);

    void Schedule(std::unique_ptr<compiler::WasmCompilationUnit>&& item);

    bool IsEmpty() const { return schedule_.empty(); }

    std::unique_ptr<compiler::WasmCompilationUnit> GetNext();

    bool CanAcceptWork() const;

    void EnableThrottling() { throttle_ = true; }

   private:
    size_t GetRandomIndexInSchedule();

    base::RandomNumberGenerator* random_number_generator_ = nullptr;
    std::vector<std::unique_ptr<compiler::WasmCompilationUnit>> schedule_;
    const size_t max_memory_;
    bool throttle_ = false;
    base::AtomicNumber<size_t> allocated_memory_{0};
  };

  Isolate* isolate_;
  std::unique_ptr<WasmModule> module_;
  bool is_sync_;
  std::vector<std::unique_ptr<compiler::WasmCompilationUnit>>
      compilation_units_;
  CodeGenerationSchedule executed_units_;
  base::Mutex result_mutex_;
  base::AtomicNumber<size_t> next_unit_;
  const size_t num_background_tasks_ = 0;
  CancelableTaskManager background_task_manager_;

  // Run by each compilation task and by the main thread.
  bool FetchAndExecuteCompilationUnit() {
    DisallowHeapAllocation no_allocation;
    DisallowHandleAllocation no_handles;
    DisallowHandleDereference no_deref;
    DisallowCodeDependencyChange no_dependency_change;

    // - 1 because AtomicIncrement returns the value after the atomic increment.
    size_t index = next_unit_.Increment(1) - 1;
    if (index >= compilation_units_.size()) {
      return false;
    }

    std::unique_ptr<compiler::WasmCompilationUnit> unit =
        std::move(compilation_units_.at(index));
    unit->ExecuteCompilation();
    base::LockGuard<base::Mutex> guard(&result_mutex_);
    executed_units_.Schedule(std::move(unit));
    return true;
  }

  size_t InitializeParallelCompilation(
      const std::vector<WasmFunction>& functions, ModuleBytesEnv& module_env) {
    uint32_t start = module_env.module_env.module->num_imported_functions +
                     FLAG_skip_compiling_wasm_funcs;
    uint32_t num_funcs = static_cast<uint32_t>(functions.size());
    uint32_t funcs_to_compile = start > num_funcs ? 0 : num_funcs - start;
    compilation_units_.reserve(funcs_to_compile);
    for (uint32_t i = start; i < num_funcs; ++i) {
      const WasmFunction* func = &functions[i];
      constexpr bool is_sync = true;
      compilation_units_.push_back(
          std::unique_ptr<compiler::WasmCompilationUnit>(
              new compiler::WasmCompilationUnit(isolate_, &module_env, func,
                                                !is_sync)));
    }
    return funcs_to_compile;
  }

  void RestartCompilationTasks() {
    base::LockGuard<base::Mutex> guard(&tasks_mutex_);
    for (; stopped_compilation_tasks_ > 0; --stopped_compilation_tasks_) {
      V8::GetCurrentPlatform()->CallOnBackgroundThread(
          new CompilationTask(this), v8::Platform::kShortRunningTask);
    }
  }

  void WaitForCompilationTasks(uint32_t* task_ids) {
    for (size_t i = 0; i < num_background_tasks_; ++i) {
      // If the task has not started yet, then we abort it. Otherwise we wait
      // for it to finish.
      if (isolate_->cancelable_task_manager()->TryAbort(task_ids[i]) !=
          CancelableTaskManager::kTaskAborted) {
        module_->pending_tasks.get()->Wait();
      }
    }
  }

  size_t FinishCompilationUnits(std::vector<Handle<Code>>& results,
                                ErrorThrower* thrower) {
    size_t finished = 0;
    while (true) {
      int func_index = -1;
      Handle<Code> result = FinishCompilationUnit(thrower, &func_index);
      if (func_index < 0) break;
      results[func_index] = result;
      ++finished;
    }
    RestartCompilationTasks();
    return finished;
  }

  Handle<Code> FinishCompilationUnit(ErrorThrower* thrower, int* func_index) {
    std::unique_ptr<compiler::WasmCompilationUnit> unit;
    {
      base::LockGuard<base::Mutex> guard(&result_mutex_);
      if (executed_units_.IsEmpty()) return Handle<Code>::null();
      unit = executed_units_.GetNext();
    }
    *func_index = unit->func_index();
    Handle<Code> result = unit->FinishCompilation(thrower);
    return result;
  }

  void CompileInParallel(ModuleBytesEnv* module_env,
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
    InitializeParallelCompilation(module->functions, *module_env);

    executed_units_.EnableThrottling();

    // 2) The main thread spawns {CompilationTask} instances which run on
    //    the background threads.
    RestartCompilationTasks();

    size_t finished_functions = 0;
    while (finished_functions < compilation_units_.size()) {
      // 3.a) The background threads and the main thread pick one compilation
      //      unit at a time and execute the parallel phase of the compilation
      //      unit. After finishing the execution of the parallel phase, the
      //      result is enqueued in {executed_units}.
      size_t index = 0;
      if (GetNextUncompiledFunctionId(&index)) {
        CompileAndSchedule(index);
      }
      // 3.b) If {executed_units} contains a compilation unit, the main thread
      //      dequeues it and finishes the compilation unit. Compilation units
      //      are finished concurrently to the background threads to save
      //      memory.
      finished_functions += FinishCompilationUnits(results, thrower);
    }
    // 4) After the parallel phase of all compilation units has started, the
    //    main thread waits for all {CompilationTask} instances to finish -
    //    which happens once they all realize there's no next work item to
    //    process.
    background_task_manager_.CancelAndWait();
  }

  void CompileSequentially(ModuleBytesEnv* module_env,
                           std::vector<Handle<Code>>& results,
                           ErrorThrower* thrower) {
    DCHECK(!thrower->error());

    const WasmModule* module = module_env->module_env.module;
    for (uint32_t i = FLAG_skip_compiling_wasm_funcs;
         i < module->functions.size(); ++i) {
      const WasmFunction& func = module->functions[i];
      if (func.imported)
        continue;  // Imports are compiled at instantiation time.

      // Compile the function.
      Handle<Code> code = compiler::WasmCompilationUnit::CompileWasmFunction(
          thrower, isolate_, module_env, &func);
      if (code.is_null()) {
        WasmName str = module_env->wire_bytes.GetName(&func);
        thrower->CompileError("Compilation of #%d:%.*s failed.", i,
                              str.length(), str.start());
        break;
      }
      results[i] = code;
    }
  }

  MaybeHandle<WasmModuleObject> CompileToModuleObject(
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
    int function_table_count =
        static_cast<int>(module_->function_tables.size());
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

    if (is_sync_) {
      // TODO(karlschimpf): Make this work when asynchronous.
      // https://bugs.chromium.org/p/v8/issues/detail?id=6361
      HistogramTimerScope wasm_compile_module_time_scope(
          module_->is_wasm()
              ? isolate_->counters()->wasm_compile_wasm_module_time()
              : isolate_->counters()->wasm_compile_asm_module_time());
      return CompileToModuleObjectInternal(
          thrower, wire_bytes, asm_js_script, asm_js_offset_table_bytes,
          factory, &temp_instance, &function_tables, &signature_tables);
    }
    return CompileToModuleObjectInternal(
        thrower, wire_bytes, asm_js_script, asm_js_offset_table_bytes, factory,
        &temp_instance, &function_tables, &signature_tables);
  }

 private:
  MaybeHandle<WasmModuleObject> CompileToModuleObjectInternal(
      ErrorThrower* thrower, const ModuleWireBytes& wire_bytes,
      Handle<Script> asm_js_script,
      Vector<const byte> asm_js_offset_table_bytes, Factory* factory,
      WasmInstance* temp_instance, Handle<FixedArray>* function_tables,
      Handle<FixedArray>* signature_tables) {
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
    for (int i = 0, e = static_cast<int>(module_->functions.size()); i < e;
         ++i) {
      code_table->set(i, *init_builtin);
      temp_instance->function_code[i] = init_builtin;
    }

    if (is_sync_)
      // TODO(karlschimpf): Make this work when asynchronous.
      // https://bugs.chromium.org/p/v8/issues/detail?id=6361
      (module_->is_wasm()
           ? isolate_->counters()->wasm_functions_per_wasm_module()
           : isolate_->counters()->wasm_functions_per_asm_module())
          ->AddSample(static_cast<int>(module_->functions.size()));

    if (!lazy_compile) {
      size_t funcs_to_compile =
          module_->functions.size() - module_->num_imported_functions;
      if (!FLAG_trace_wasm_decoder && FLAG_wasm_num_compilation_tasks != 0 &&
          funcs_to_compile > 1) {
        // Avoid a race condition by collecting results into a second vector.
        std::vector<Handle<Code>> results(temp_instance->function_code);
        CompileInParallel(&module_env, results, thrower);
        temp_instance->function_code.swap(results);
      } else {
        CompileSequentially(&module_env, temp_instance->function_code, thrower);
      }
      if (thrower->error()) return {};
    }

    // At this point, compilation has completed. Update the code table.
    for (size_t i = FLAG_skip_compiling_wasm_funcs;
         i < temp_instance->function_code.size(); ++i) {
      Code* code = *temp_instance->function_code[i];
      code_table->set(static_cast<int>(i), code);
      RecordStats(isolate_, code, is_sync_);
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

    // Compile JS->WASM wrappers for exported functions.
    JSToWasmWrapperCache js_to_wasm_cache;
    int func_index = 0;
    for (auto exp : module->export_table) {
      if (exp.kind != kExternalFunction) continue;
      Handle<Code> wasm_code = EnsureExportedLazyDeoptData(
          isolate_, Handle<WasmInstanceObject>::null(), code_table, exp.index);
      Handle<Code> wrapper_code =
          js_to_wasm_cache.CloneOrCompileJSToWasmWrapper(isolate_, module,
                                                         wasm_code, exp.index);
      int export_index =
          static_cast<int>(module->functions.size() + func_index);
      code_table->set(export_index, *wrapper_code);
      RecordStats(isolate_, *wrapper_code, is_sync_);
      func_index++;
    }

    return WasmModuleObject::New(isolate_, compiled_module);
  }
  size_t stopped_compilation_tasks_ = 0;
  base::Mutex tasks_mutex_;
};

CompilationHelper::CodeGenerationSchedule::CodeGenerationSchedule(
    base::RandomNumberGenerator* random_number_generator, size_t max_memory)
    : random_number_generator_(random_number_generator),
      max_memory_(max_memory) {
  DCHECK_NOT_NULL(random_number_generator_);
  DCHECK_GT(max_memory_, 0);
}

void CompilationHelper::CodeGenerationSchedule::Schedule(
    std::unique_ptr<compiler::WasmCompilationUnit>&& item) {
  size_t cost = item->memory_cost();
  schedule_.push_back(std::move(item));
  allocated_memory_.Increment(cost);
}

bool CompilationHelper::CodeGenerationSchedule::CanAcceptWork() const {
  return (!throttle_ || allocated_memory_.Value() <= max_memory_);
}

std::unique_ptr<compiler::WasmCompilationUnit>
CompilationHelper::CodeGenerationSchedule::GetNext() {
  DCHECK(!IsEmpty());
  size_t index = GetRandomIndexInSchedule();
  auto ret = std::move(schedule_[index]);
  std::swap(schedule_[schedule_.size() - 1], schedule_[index]);
  schedule_.pop_back();
  allocated_memory_.Decrement(ret->memory_cost());
  return ret;
}

size_t CompilationHelper::CodeGenerationSchedule::GetRandomIndexInSchedule() {
  double factor = random_number_generator_->NextDouble();
  size_t index = (size_t)(factor * schedule_.size());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, schedule_.size());
  return index;
}

static void MemoryInstanceFinalizer(Isolate* isolate,
                                    WasmInstanceObject* instance) {
  DisallowHeapAllocation no_gc;
  // If the memory object is destroyed, nothing needs to be done here.
  if (!instance->has_memory_object()) return;
  Handle<WasmInstanceWrapper> instance_wrapper =
      handle(instance->instance_wrapper());
  DCHECK(WasmInstanceWrapper::IsWasmInstanceWrapper(*instance_wrapper));
  DCHECK(instance_wrapper->has_instance());
  bool has_prev = instance_wrapper->has_previous();
  bool has_next = instance_wrapper->has_next();
  Handle<WasmMemoryObject> memory_object(instance->memory_object());

  if (!has_prev && !has_next) {
    memory_object->ResetInstancesLink(isolate);
    return;
  } else {
    Handle<WasmInstanceWrapper> next_wrapper, prev_wrapper;
    if (!has_prev) {
      Handle<WasmInstanceWrapper> next_wrapper =
          instance_wrapper->next_wrapper();
      next_wrapper->reset_previous_wrapper();
      // As this is the first link in the memory object, destroying
      // without updating memory object would corrupt the instance chain in
      // the memory object.
      memory_object->set_instances_link(*next_wrapper);
    } else if (!has_next) {
      instance_wrapper->previous_wrapper()->reset_next_wrapper();
    } else {
      DCHECK(has_next && has_prev);
      Handle<WasmInstanceWrapper> prev_wrapper =
          instance_wrapper->previous_wrapper();
      Handle<WasmInstanceWrapper> next_wrapper =
          instance_wrapper->next_wrapper();
      prev_wrapper->set_next_wrapper(*next_wrapper);
      next_wrapper->set_previous_wrapper(*prev_wrapper);
    }
    // Reset to avoid dangling pointers
    instance_wrapper->reset();
  }
}

static void InstanceFinalizer(const v8::WeakCallbackInfo<void>& data) {
  DisallowHeapAllocation no_gc;
  JSObject** p = reinterpret_cast<JSObject**>(data.GetParameter());
  WasmInstanceObject* owner = reinterpret_cast<WasmInstanceObject*>(*p);
  Isolate* isolate = reinterpret_cast<Isolate*>(data.GetIsolate());
  // If a link to shared memory instances exists, update the list of memory
  // instances before the instance is destroyed.
  if (owner->has_instance_wrapper()) MemoryInstanceFinalizer(isolate, owner);
  WasmCompiledModule* compiled_module = owner->compiled_module();
  TRACE("Finalizing %d {\n", compiled_module->instance_id());
  DCHECK(compiled_module->has_weak_wasm_module());
  WeakCell* weak_wasm_module = compiled_module->ptr_to_weak_wasm_module();

  if (trap_handler::UseTrapHandler()) {
    Handle<FixedArray> code_table = compiled_module->code_table();
    for (int i = 0; i < code_table->length(); ++i) {
      Handle<Code> code = code_table->GetValueChecked<Code>(isolate, i);
      int index = code->trap_handler_index()->value();
      if (index >= 0) {
        trap_handler::ReleaseHandlerData(index);
        code->set_trap_handler_index(Smi::FromInt(-1));
      }
    }
  }

  // weak_wasm_module may have been cleared, meaning the module object
  // was GC-ed. In that case, there won't be any new instances created,
  // and we don't need to maintain the links between instances.
  if (!weak_wasm_module->cleared()) {
    JSObject* wasm_module = JSObject::cast(weak_wasm_module->value());
    WasmCompiledModule* current_template =
        WasmCompiledModule::cast(wasm_module->GetEmbedderField(0));

    TRACE("chain before {\n");
    TRACE_CHAIN(current_template);
    TRACE("}\n");

    DCHECK(!current_template->has_weak_prev_instance());
    WeakCell* next = compiled_module->maybe_ptr_to_weak_next_instance();
    WeakCell* prev = compiled_module->maybe_ptr_to_weak_prev_instance();

    if (current_template == compiled_module) {
      if (next == nullptr) {
        WasmCompiledModule::Reset(isolate, compiled_module);
      } else {
        DCHECK(next->value()->IsFixedArray());
        wasm_module->SetEmbedderField(0, next->value());
        DCHECK_NULL(prev);
        WasmCompiledModule::cast(next->value())->reset_weak_prev_instance();
      }
    } else {
      DCHECK(!(prev == nullptr && next == nullptr));
      // the only reason prev or next would be cleared is if the
      // respective objects got collected, but if that happened,
      // we would have relinked the list.
      if (prev != nullptr) {
        DCHECK(!prev->cleared());
        if (next == nullptr) {
          WasmCompiledModule::cast(prev->value())->reset_weak_next_instance();
        } else {
          WasmCompiledModule::cast(prev->value())
              ->set_ptr_to_weak_next_instance(next);
        }
      }
      if (next != nullptr) {
        DCHECK(!next->cleared());
        if (prev == nullptr) {
          WasmCompiledModule::cast(next->value())->reset_weak_prev_instance();
        } else {
          WasmCompiledModule::cast(next->value())
              ->set_ptr_to_weak_prev_instance(prev);
        }
      }
    }
    TRACE("chain after {\n");
    TRACE_CHAIN(WasmCompiledModule::cast(wasm_module->GetEmbedderField(0)));
    TRACE("}\n");
  }
  compiled_module->reset_weak_owning_instance();
  GlobalHandles::Destroy(reinterpret_cast<Object**>(p));
  TRACE("}\n");
}

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

int ExtractDirectCallIndex(wasm::Decoder& decoder, const byte* pc) {
  DCHECK_EQ(static_cast<int>(kExprCallFunction), static_cast<int>(*pc));
  // Read the leb128 encoded u32 value (up to 5 bytes starting at pc + 1).
  decoder.Reset(pc + 1, pc + 6);
  uint32_t call_idx = decoder.consume_u32v("call index");
  DCHECK(decoder.ok());
  DCHECK_GE(kMaxInt, call_idx);
  return static_cast<int>(call_idx);
}

void RecordLazyCodeStats(Isolate* isolate, Code* code) {
  isolate->counters()->wasm_lazily_compiled_functions()->Increment();
  isolate->counters()->wasm_generated_code_size()->Increment(code->body_size());
  isolate->counters()->wasm_reloc_size()->Increment(
      code->relocation_info()->length());
}

}  // namespace

Handle<JSArrayBuffer> wasm::SetupArrayBuffer(Isolate* isolate,
                                             void* allocation_base,
                                             size_t allocation_length,
                                             void* backing_store, size_t size,
                                             bool is_external,
                                             bool enable_guard_regions) {
  Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
  JSArrayBuffer::Setup(buffer, isolate, is_external, allocation_base,
                       allocation_length, backing_store,
                       static_cast<int>(size));
  buffer->set_is_neuterable(false);
  buffer->set_is_wasm_buffer(true);
  buffer->set_has_guard_region(enable_guard_regions);
  return buffer;
}

Handle<JSArrayBuffer> wasm::NewArrayBuffer(Isolate* isolate, size_t size,
                                           bool enable_guard_regions) {
  if (size > (FLAG_wasm_max_mem_pages * WasmModule::kPageSize)) {
    // TODO(titzer): lift restriction on maximum memory allocated here.
    return Handle<JSArrayBuffer>::null();
  }

  enable_guard_regions = enable_guard_regions && kGuardRegionsSupported;

  void* allocation_base = nullptr;  // Set by TryAllocateBackingStore
  size_t allocation_length = 0;     // Set by TryAllocateBackingStore
  void* memory = TryAllocateBackingStore(isolate, size, enable_guard_regions,
                                         allocation_base, allocation_length);

  if (memory == nullptr) {
    return Handle<JSArrayBuffer>::null();
  }

#if DEBUG
  // Double check the API allocator actually zero-initialized the memory.
  const byte* bytes = reinterpret_cast<const byte*>(memory);
  for (size_t i = 0; i < size; ++i) {
    DCHECK_EQ(0, bytes[i]);
  }
#endif

  const bool is_external = false;
  return SetupArrayBuffer(isolate, allocation_base, allocation_length, memory,
                          size, is_external, enable_guard_regions);
}

void wasm::UnpackAndRegisterProtectedInstructions(
    Isolate* isolate, Handle<FixedArray> code_table) {
  for (int i = 0; i < code_table->length(); ++i) {
    Handle<Code> code;
    // This is sometimes undefined when we're called from cctests.
    if (!code_table->GetValue<Code>(isolate, i).ToHandle(&code)) {
      continue;
    }

    if (code->kind() != Code::WASM_FUNCTION) {
      continue;
    }

    const intptr_t base = reinterpret_cast<intptr_t>(code->entry());

    Zone zone(isolate->allocator(), "Wasm Module");
    ZoneVector<trap_handler::ProtectedInstructionData> unpacked(&zone);
    const int mode_mask =
        RelocInfo::ModeMask(RelocInfo::WASM_PROTECTED_INSTRUCTION_LANDING);
    for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
      trap_handler::ProtectedInstructionData data;
      data.instr_offset = it.rinfo()->data();
      data.landing_offset = reinterpret_cast<intptr_t>(it.rinfo()->pc()) - base;
      unpacked.emplace_back(data);
    }
    if (unpacked.size() > 0) {
      int size = code->CodeSize();
      const int index = RegisterHandlerData(reinterpret_cast<void*>(base), size,
                                            unpacked.size(), &unpacked[0]);
      // TODO(eholk): if index is negative, fail.
      DCHECK(index >= 0);
      code->set_trap_handler_index(Smi::FromInt(index));
    }
  }
}

std::ostream& wasm::operator<<(std::ostream& os, const WasmFunctionName& name) {
  os << "#" << name.function_->func_index;
  if (name.function_->name_offset > 0) {
    if (name.name_.start()) {
      os << ":";
      os.write(name.name_.start(), name.name_.length());
    }
  } else {
    os << "?";
  }
  return os;
}

WasmInstanceObject* wasm::GetOwningWasmInstance(Code* code) {
  DisallowHeapAllocation no_gc;
  DCHECK(code->kind() == Code::WASM_FUNCTION ||
         code->kind() == Code::WASM_INTERPRETER_ENTRY);
  FixedArray* deopt_data = code->deoptimization_data();
  DCHECK_EQ(code->kind() == Code::WASM_INTERPRETER_ENTRY ? 1 : 2,
            deopt_data->length());
  Object* weak_link = deopt_data->get(0);
  DCHECK(weak_link->IsWeakCell());
  WeakCell* cell = WeakCell::cast(weak_link);
  if (cell->cleared()) return nullptr;
  return WasmInstanceObject::cast(cell->value());
}

WasmModule::WasmModule(std::unique_ptr<Zone> owned)
    : signature_zone(std::move(owned)), pending_tasks(new base::Semaphore(0)) {}

namespace {

WasmFunction* GetWasmFunctionForImportWrapper(Isolate* isolate,
                                              Handle<Object> target) {
  if (target->IsJSFunction()) {
    Handle<JSFunction> func = Handle<JSFunction>::cast(target);
    if (func->code()->kind() == Code::JS_TO_WASM_FUNCTION) {
      auto exported = Handle<WasmExportedFunction>::cast(func);
      Handle<WasmInstanceObject> other_instance(exported->instance(), isolate);
      int func_index = exported->function_index();
      return &other_instance->module()->functions[func_index];
    }
  }
  return nullptr;
}

static Handle<Code> UnwrapImportWrapper(Handle<Object> import_wrapper) {
  Handle<JSFunction> func = Handle<JSFunction>::cast(import_wrapper);
  Handle<Code> export_wrapper_code = handle(func->code());
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET);
  for (RelocIterator it(*export_wrapper_code, mask);; it.next()) {
    DCHECK(!it.done());
    Code* target = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
    if (target->kind() != Code::WASM_FUNCTION &&
        target->kind() != Code::WASM_TO_JS_FUNCTION &&
        target->kind() != Code::WASM_INTERPRETER_ENTRY)
      continue;
// There should only be this one call to wasm code.
#ifdef DEBUG
    for (it.next(); !it.done(); it.next()) {
      Code* code = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
      DCHECK(code->kind() != Code::WASM_FUNCTION &&
             code->kind() != Code::WASM_TO_JS_FUNCTION &&
             code->kind() != Code::WASM_INTERPRETER_ENTRY);
    }
#endif
    return handle(target);
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

Handle<Code> CompileImportWrapper(Isolate* isolate, int index, FunctionSig* sig,
                                  Handle<JSReceiver> target,
                                  Handle<String> module_name,
                                  MaybeHandle<String> import_name,
                                  ModuleOrigin origin) {
  WasmFunction* other_func = GetWasmFunctionForImportWrapper(isolate, target);
  if (other_func) {
    if (!sig->Equals(other_func->sig)) return Handle<Code>::null();
    // Signature matched. Unwrap the JS->WASM wrapper and return the raw
    // WASM function code.
    return UnwrapImportWrapper(target);
  }
  // No wasm function or being debugged. Compile a new wrapper for the new
  // signature.
  return compiler::CompileWasmToJSWrapper(isolate, target, sig, index,
                                          module_name, import_name, origin);
}

void UpdateDispatchTablesInternal(Isolate* isolate,
                                  Handle<FixedArray> dispatch_tables, int index,
                                  WasmFunction* function, Handle<Code> code) {
  DCHECK_EQ(0, dispatch_tables->length() % 4);
  for (int i = 0; i < dispatch_tables->length(); i += 4) {
    int table_index = Smi::cast(dispatch_tables->get(i + 1))->value();
    Handle<FixedArray> function_table(
        FixedArray::cast(dispatch_tables->get(i + 2)), isolate);
    Handle<FixedArray> signature_table(
        FixedArray::cast(dispatch_tables->get(i + 3)), isolate);
    if (function) {
      // TODO(titzer): the signature might need to be copied to avoid
      // a dangling pointer in the signature map.
      Handle<WasmInstanceObject> instance(
          WasmInstanceObject::cast(dispatch_tables->get(i)), isolate);
      auto& func_table = instance->module()->function_tables[table_index];
      uint32_t sig_index = func_table.map.FindOrInsert(function->sig);
      signature_table->set(index, Smi::FromInt(static_cast<int>(sig_index)));
      function_table->set(index, *code);
    } else {
      signature_table->set(index, Smi::FromInt(-1));
      function_table->set(index, Smi::kZero);
    }
  }
}

}  // namespace

void wasm::UpdateDispatchTables(Isolate* isolate,
                                Handle<FixedArray> dispatch_tables, int index,
                                Handle<JSFunction> function) {
  if (function.is_null()) {
    UpdateDispatchTablesInternal(isolate, dispatch_tables, index, nullptr,
                                 Handle<Code>::null());
  } else {
    UpdateDispatchTablesInternal(
        isolate, dispatch_tables, index,
        GetWasmFunctionForImportWrapper(isolate, function),
        UnwrapImportWrapper(function));
  }
}

// A helper class to simplify instantiating a module from a compiled module.
// It closes over the {Isolate}, the {ErrorThrower}, the {WasmCompiledModule},
// etc.
class InstantiationHelper {
 public:
  InstantiationHelper(Isolate* isolate, ErrorThrower* thrower,
                      Handle<WasmModuleObject> module_object,
                      MaybeHandle<JSReceiver> ffi,
                      MaybeHandle<JSArrayBuffer> memory)
      : isolate_(isolate),
        module_(module_object->compiled_module()->module()),
        thrower_(thrower),
        module_object_(module_object),
        ffi_(ffi.is_null() ? Handle<JSReceiver>::null()
                           : ffi.ToHandleChecked()),
        memory_(memory.is_null() ? Handle<JSArrayBuffer>::null()
                                 : memory.ToHandleChecked()) {}

  // Build an instance, in all of its glory.
  MaybeHandle<WasmInstanceObject> Build() {
    // Check that an imports argument was provided, if the module requires it.
    // No point in continuing otherwise.
    if (!module_->import_table.empty() && ffi_.is_null()) {
      thrower_->TypeError(
          "Imports argument must be present and must be an object");
      return {};
    }

    // Record build time into correct bucket, then build instance.
    HistogramTimerScope wasm_instantiate_module_time_scope(
        module_->is_wasm()
            ? isolate_->counters()->wasm_instantiate_wasm_module_time()
            : isolate_->counters()->wasm_instantiate_asm_module_time());
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

        // Clone the code for WASM functions and exports.
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
        RecordStats(isolate_, code_table, is_sync_);
      } else {
        // There was no owner, so we can reuse the original.
        compiled_module_ = original;
        old_code_table =
            factory->CopyFixedArray(compiled_module_->code_table());
        code_table = compiled_module_->code_table();
        TRACE("Reusing existing instance %d\n",
              compiled_module_->instance_id());
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
    int function_table_count =
        static_cast<int>(module_->function_tables.size());
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
    (module_->is_wasm() ? isolate_->counters()->wasm_wasm_min_mem_pages_count()
                        : isolate_->counters()->wasm_asm_min_mem_pages_count())
        ->AddSample(min_mem_pages);

    if (!memory_.is_null()) {
      // Set externally passed ArrayBuffer non neuterable.
      memory_->set_is_neuterable(false);
      memory_->set_is_wasm_buffer(true);

      DCHECK_IMPLIES(EnableGuardRegions(),
                     module_->is_asm_js() || memory_->has_guard_region());
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
      uint32_t mem_size = memory_.is_null()
          ? 0 : static_cast<uint32_t>(memory_->byte_length()->Number());
      if (!in_bounds(base, seg.source_size, mem_size)) {
        thrower_->LinkError("data segment is out of bounds");
        return {};
      }
    }

    //--------------------------------------------------------------------------
    // Initialize memory.
    //--------------------------------------------------------------------------
    if (!memory_.is_null()) {
      Address mem_start = static_cast<Address>(memory_->backing_store());
      uint32_t mem_size =
          static_cast<uint32_t>(memory_->byte_length()->Number());
      LoadDataSegments(mem_start, mem_size);

      uint32_t old_mem_size = compiled_module_->mem_size();
      Address old_mem_start = compiled_module_->GetEmbeddedMemStartOrNull();
      // We might get instantiated again with the same memory. No patching
      // needed in this case.
      if (old_mem_start != mem_start || old_mem_size != mem_size) {
        code_specialization.RelocateMemoryReferences(
            old_mem_start, old_mem_size, mem_start, mem_size);
      }
      // Just like with globals, we need to keep both the JSArrayBuffer
      // and save the start pointer.
      instance->set_memory_buffer(*memory_);
      WasmCompiledModule::SetSpecializationMemInfoFrom(
          factory, compiled_module_, memory_);
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
      if (code->deoptimization_data()->length() == 0) continue;
      DCHECK_LE(2, code->deoptimization_data()->length());
      DCHECK_EQ(i, Smi::cast(code->deoptimization_data()->get(1))->value());
      code->deoptimization_data()->set(0, *weak_link);
    }

    //--------------------------------------------------------------------------
    // Set up the exports object for the new instance.
    //--------------------------------------------------------------------------
    ProcessExports(code_table, instance, compiled_module_);

    //--------------------------------------------------------------------------
    // Add instance to Memory object
    //--------------------------------------------------------------------------
    DCHECK(wasm::IsWasmInstance(*instance));
    if (instance->has_memory_object()) {
      instance->memory_object()->AddInstance(isolate_, instance);
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
        module_object_->SetEmbedderField(0, *compiled_module_);
        compiled_module_->set_weak_owning_instance(link_to_owning_instance);
        GlobalHandles::MakeWeak(global_handle.location(),
                                global_handle.location(), &InstanceFinalizer,
                                v8::WeakCallbackType::kFinalizer);
      }
    }

    //--------------------------------------------------------------------------
    // Debugging support.
    //--------------------------------------------------------------------------
    // Set all breakpoints that were set on the shared module.
    WasmSharedModuleData::SetBreakpointsOnNewInstance(
        compiled_module_->shared(), instance);

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
      Handle<Code> wrapper_code =
          js_to_wasm_cache_.CloneOrCompileJSToWasmWrapper(
              isolate_, module_, startup_code, start_index);
      Handle<WasmExportedFunction> startup_fct = WasmExportedFunction::New(
          isolate_, instance, MaybeHandle<String>(), start_index,
          static_cast<int>(sig->parameter_count()), wrapper_code);
      RecordStats(isolate_, *startup_code, is_sync_);
      // Call the JS function.
      Handle<Object> undefined = factory->undefined_value();
      MaybeHandle<Object> retval =
          Execution::Call(isolate_, startup_fct, undefined, 0, nullptr);

      if (retval.is_null()) {
        DCHECK(isolate_->has_pending_exception());
        isolate_->OptionalRescheduleException(false);
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

 private:
  // Represents the initialized state of a table.
  struct TableInstance {
    Handle<WasmTableObject> table_object;    // WebAssembly.Table instance
    Handle<FixedArray> js_wrappers;          // JSFunctions exported
    Handle<FixedArray> function_table;       // internal code array
    Handle<FixedArray> signature_table;      // internal sig array
  };

  Isolate* isolate_;
  WasmModule* const module_;
  constexpr static bool is_sync_ = true;
  ErrorThrower* thrower_;
  Handle<WasmModuleObject> module_object_;
  Handle<JSReceiver> ffi_;        // TODO(titzer): Use MaybeHandle
  Handle<JSArrayBuffer> memory_;  // TODO(titzer): Use MaybeHandle
  Handle<JSArrayBuffer> globals_;
  Handle<WasmCompiledModule> compiled_module_;
  std::vector<TableInstance> table_instances_;
  std::vector<Handle<JSFunction>> js_wrappers_;
  JSToWasmWrapperCache js_to_wasm_cache_;

// Helper routines to print out errors with imports.
#define ERROR_THROWER_WITH_MESSAGE(TYPE)                                      \
  void Report##TYPE(const char* error, uint32_t index,                        \
                    Handle<String> module_name, Handle<String> import_name) { \
    thrower_->TYPE("Import #%d module=\"%.*s\" function=\"%.*s\" error: %s",  \
                   index, module_name->length(),                              \
                   module_name->ToCString().get(), import_name->length(),     \
                   import_name->ToCString().get(), error);                    \
  }                                                                           \
                                                                              \
  MaybeHandle<Object> Report##TYPE(const char* error, uint32_t index,         \
                                   Handle<String> module_name) {              \
    thrower_->TYPE("Import #%d module=\"%.*s\" error: %s", index,             \
                   module_name->length(), module_name->ToCString().get(),     \
                   error);                                                    \
    return MaybeHandle<Object>();                                             \
  }

  ERROR_THROWER_WITH_MESSAGE(LinkError)
  ERROR_THROWER_WITH_MESSAGE(TypeError)

  // Look up an import value in the {ffi_} object.
  MaybeHandle<Object> LookupImport(uint32_t index, Handle<String> module_name,
                                   Handle<String> import_name) {
    // We pre-validated in the js-api layer that the ffi object is present, and
    // a JSObject, if the module has imports.
    DCHECK(!ffi_.is_null());

    // Look up the module first.
    MaybeHandle<Object> result =
        Object::GetPropertyOrElement(ffi_, module_name);
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

  uint32_t EvalUint32InitExpr(const WasmInitExpr& expr) {
    switch (expr.kind) {
      case WasmInitExpr::kI32Const:
        return expr.val.i32_const;
      case WasmInitExpr::kGlobalIndex: {
        uint32_t offset = module_->globals[expr.val.global_index].offset;
        return *reinterpret_cast<uint32_t*>(raw_buffer_ptr(globals_, offset));
      }
      default:
        UNREACHABLE();
        return 0;
    }
  }

  bool in_bounds(uint32_t offset, uint32_t size, uint32_t upper) {
    return offset + size <= upper && offset + size >= offset;
  }

  // Load data segments into the memory.
  void LoadDataSegments(Address mem_addr, size_t mem_size) {
    Handle<SeqOneByteString> module_bytes(compiled_module_->module_bytes(),
                                          isolate_);
    for (const WasmDataSegment& segment : module_->data_segments) {
      uint32_t source_size = segment.source_size;
      // Segments of size == 0 are just nops.
      if (source_size == 0) continue;
      uint32_t dest_offset = EvalUint32InitExpr(segment.dest_addr);
      DCHECK(in_bounds(dest_offset, source_size,
                       static_cast<uint32_t>(mem_size)));
      byte* dest = mem_addr + dest_offset;
      const byte* src = reinterpret_cast<const byte*>(
          module_bytes->GetCharsAddress() + segment.source_offset);
      memcpy(dest, src, source_size);
    }
  }

  void WriteGlobalValue(WasmGlobal& global, Handle<Object> value) {
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
  int ProcessImports(Handle<FixedArray> code_table,
                     Handle<WasmInstanceObject> instance) {
    int num_imported_functions = 0;
    int num_imported_tables = 0;
    for (int index = 0; index < static_cast<int>(module_->import_table.size());
         ++index) {
      WasmImport& import = module_->import_table[index];

      Handle<String> module_name;
      MaybeHandle<String> maybe_module_name =
          WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
              isolate_, compiled_module_, import.module_name_offset,
              import.module_name_length);
      if (!maybe_module_name.ToHandle(&module_name)) return -1;

      Handle<String> import_name;
      MaybeHandle<String> maybe_import_name =
          WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
              isolate_, compiled_module_, import.field_name_offset,
              import.field_name_length);
      if (!maybe_import_name.ToHandle(&import_name)) return -1;

      MaybeHandle<Object> result =
          LookupImport(index, module_name, import_name);
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

          Handle<Code> import_wrapper = CompileImportWrapper(
              isolate_, index, module_->functions[import.index].sig,
              Handle<JSReceiver>::cast(value), module_name, import_name,
              module_->get_origin());
          if (import_wrapper.is_null()) {
            ReportLinkError(
                "imported function does not match the expected type", index,
                module_name, import_name);
            return -1;
          }
          code_table->set(num_imported_functions, *import_wrapper);
          RecordStats(isolate_, *import_wrapper, is_sync_);
          num_imported_functions++;
          break;
        }
        case kExternalTable: {
          if (!WasmJs::IsWasmTableObject(isolate_, value)) {
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
                table_instance.table_object->maximum_length();
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
              thrower_->LinkError("table import %d[%d] is not a WASM function",
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
          if (!WasmJs::IsWasmMemoryObject(isolate_, value)) {
            ReportLinkError("memory import must be a WebAssembly.Memory object",
                            index, module_name, import_name);
            return -1;
          }
          auto memory = Handle<WasmMemoryObject>::cast(value);
          DCHECK(WasmJs::IsWasmMemoryObject(isolate_, memory));
          instance->set_memory_object(*memory);
          memory_ = Handle<JSArrayBuffer>(memory->buffer(), isolate_);
          uint32_t imported_cur_pages = static_cast<uint32_t>(
              memory_->byte_length()->Number() / WasmModule::kPageSize);
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
            if (module_->globals[import.index].type == kWasmI32) {
              value = Object::ToInt32(isolate_, value).ToHandleChecked();
            } else {
              value = Object::ToNumber(value).ToHandleChecked();
            }
          }
          if (!value->IsNumber()) {
            ReportLinkError("global import must be a number", index,
                            module_name, import_name);
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
    return num_imported_functions;
  }

  template <typename T>
  T* GetRawGlobalPtr(WasmGlobal& global) {
    return reinterpret_cast<T*>(raw_buffer_ptr(globals_, global.offset));
  }

  // Process initialization of globals.
  void InitGlobals() {
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
          TRACE("init [globals+%u] = [globals+%d]\n", global.offset,
                old_offset);
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
  Handle<JSArrayBuffer> AllocateMemory(uint32_t min_mem_pages) {
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

  bool NeedsWrappers() {
    if (module_->num_exported_functions > 0) return true;
    for (auto table_instance : table_instances_) {
      if (!table_instance.js_wrappers.is_null()) return true;
    }
    for (auto table : module_->function_tables) {
      if (table.exported) return true;
    }
    return false;
  }

  // Process the exports, creating wrappers for functions, tables, memories,
  // and globals.
  void ProcessExports(Handle<FixedArray> code_table,
                      Handle<WasmInstanceObject> instance,
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
      weak_exported_functions =
          isolate_->factory()->NewFixedArray(export_count);
      compiled_module->set_weak_exported_functions(weak_exported_functions);
    }

    // Process each export in the export table.
    int export_index = 0;  // Index into {weak_exported_functions}.
    for (WasmExport& exp : module_->export_table) {
      Handle<String> name =
          WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
              isolate_, compiled_module_, exp.name_offset, exp.name_length)
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
                              isolate_, compiled_module_, function.name_offset,
                              function.name_length)
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
          WasmIndirectFunctionTable& table =
              module_->function_tables[exp.index];
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
            DCHECK(WasmJs::IsWasmMemoryObject(isolate_, memory_object));
            memory_object->ResetInstancesLink(isolate_);
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
              break;
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

  void InitializeTables(Handle<WasmInstanceObject> instance,
                        CodeSpecialization* code_specialization) {
    int function_table_count =
        static_cast<int>(module_->function_tables.size());
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
          table_instance.signature_table->set(i,
                                              Smi::FromInt(kInvalidSigIndex));
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

    FixedArray* old_function_tables =
        compiled_module_->ptr_to_function_tables();
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

  void LoadTableSegments(Handle<FixedArray> code_table,
                         Handle<WasmInstanceObject> instance) {
    int function_table_count =
        static_cast<int>(module_->function_tables.size());
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
        DCHECK(in_bounds(base, static_cast<uint32_t>(table_init.entries.size()),
                         table_instance.function_table->length()));
        for (int i = 0, e = static_cast<int>(table_init.entries.size()); i < e;
             ++i) {
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
              // TODO(titzer): We compile JS->WASM wrappers for functions are
              // not exported but are in an exported table. This should be done
              // at module compile time and cached instead.

              Handle<Code> wrapper_code =
                  js_to_wasm_cache_.CloneOrCompileJSToWasmWrapper(
                      isolate_, module_, wasm_code, func_index);
              MaybeHandle<String> func_name;
              if (module_->is_asm_js()) {
                // For modules arising from asm.js, honor the names section.
                func_name =
                    WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
                        isolate_, compiled_module_, function->name_offset,
                        function->name_length)
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

            UpdateDispatchTablesInternal(isolate_, all_dispatch_tables,
                                         table_index, function, wasm_code);
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
};

bool wasm::IsWasmInstance(Object* object) {
  return WasmInstanceObject::IsWasmInstanceObject(object);
}

Handle<Script> wasm::GetScript(Handle<JSObject> instance) {
  WasmCompiledModule* compiled_module =
      WasmInstanceObject::cast(*instance)->compiled_module();
  return handle(compiled_module->script());
}

bool wasm::IsWasmCodegenAllowed(Isolate* isolate, Handle<Context> context) {
  return isolate->allow_code_gen_callback() == nullptr ||
         isolate->allow_code_gen_callback()(v8::Utils::ToLocal(context));
}

void wasm::DetachWebAssemblyMemoryBuffer(Isolate* isolate,
                                         Handle<JSArrayBuffer> buffer,
                                         bool free_memory) {
  int64_t byte_length =
      buffer->byte_length()->IsNumber()
          ? static_cast<uint32_t>(buffer->byte_length()->Number())
          : 0;
  if (buffer.is_null() || byte_length == 0) return;
  const bool is_external = buffer->is_external();
  DCHECK(!buffer->is_neuterable());
  if (!is_external) {
    buffer->set_is_external(true);
    isolate->heap()->UnregisterArrayBuffer(*buffer);
    if (free_memory) {
      // We need to free the memory before neutering the buffer because
      // FreeBackingStore reads buffer->allocation_base(), which is nulled out
      // by Neuter. This means there is a dangling pointer until we neuter the
      // buffer. Since there is no way for the user to directly call
      // FreeBackingStore, we can ensure this is safe.
      buffer->FreeBackingStore();
    }
  }
  buffer->set_is_neuterable(true);
  buffer->Neuter();
}

void testing::ValidateInstancesChain(Isolate* isolate,
                                     Handle<WasmModuleObject> module_obj,
                                     int instance_count) {
  CHECK_GE(instance_count, 0);
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = module_obj->compiled_module();
  CHECK_EQ(JSObject::cast(compiled_module->ptr_to_weak_wasm_module()->value()),
           *module_obj);
  Object* prev = nullptr;
  int found_instances = compiled_module->has_weak_owning_instance() ? 1 : 0;
  WasmCompiledModule* current_instance = compiled_module;
  while (current_instance->has_weak_next_instance()) {
    CHECK((prev == nullptr && !current_instance->has_weak_prev_instance()) ||
          current_instance->ptr_to_weak_prev_instance()->value() == prev);
    CHECK_EQ(current_instance->ptr_to_weak_wasm_module()->value(), *module_obj);
    CHECK(IsWasmInstance(
        current_instance->ptr_to_weak_owning_instance()->value()));
    prev = current_instance;
    current_instance = WasmCompiledModule::cast(
        current_instance->ptr_to_weak_next_instance()->value());
    ++found_instances;
    CHECK_LE(found_instances, instance_count);
  }
  CHECK_EQ(found_instances, instance_count);
}

void testing::ValidateModuleState(Isolate* isolate,
                                  Handle<WasmModuleObject> module_obj) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = module_obj->compiled_module();
  CHECK(compiled_module->has_weak_wasm_module());
  CHECK_EQ(compiled_module->ptr_to_weak_wasm_module()->value(), *module_obj);
  CHECK(!compiled_module->has_weak_prev_instance());
  CHECK(!compiled_module->has_weak_next_instance());
  CHECK(!compiled_module->has_weak_owning_instance());
}

void testing::ValidateOrphanedInstance(Isolate* isolate,
                                       Handle<WasmInstanceObject> instance) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = instance->compiled_module();
  CHECK(compiled_module->has_weak_wasm_module());
  CHECK(compiled_module->ptr_to_weak_wasm_module()->cleared());
}

Handle<JSArray> wasm::GetImports(Isolate* isolate,
                                 Handle<WasmModuleObject> module_object) {
  Handle<WasmCompiledModule> compiled_module(module_object->compiled_module(),
                                             isolate);
  Factory* factory = isolate->factory();

  Handle<String> module_string = factory->InternalizeUtf8String("module");
  Handle<String> name_string = factory->InternalizeUtf8String("name");
  Handle<String> kind_string = factory->InternalizeUtf8String("kind");

  Handle<String> function_string = factory->InternalizeUtf8String("function");
  Handle<String> table_string = factory->InternalizeUtf8String("table");
  Handle<String> memory_string = factory->InternalizeUtf8String("memory");
  Handle<String> global_string = factory->InternalizeUtf8String("global");

  // Create the result array.
  WasmModule* module = compiled_module->module();
  int num_imports = static_cast<int>(module->import_table.size());
  Handle<JSArray> array_object = factory->NewJSArray(FAST_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_imports);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_imports));

  Handle<JSFunction> object_function =
      Handle<JSFunction>(isolate->native_context()->object_function(), isolate);

  // Populate the result array.
  for (int index = 0; index < num_imports; ++index) {
    WasmImport& import = module->import_table[index];

    Handle<JSObject> entry = factory->NewJSObject(object_function);

    Handle<String> import_kind;
    switch (import.kind) {
      case kExternalFunction:
        import_kind = function_string;
        break;
      case kExternalTable:
        import_kind = table_string;
        break;
      case kExternalMemory:
        import_kind = memory_string;
        break;
      case kExternalGlobal:
        import_kind = global_string;
        break;
      default:
        UNREACHABLE();
    }

    MaybeHandle<String> import_module =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, import.module_name_offset,
            import.module_name_length);

    MaybeHandle<String> import_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, import.field_name_offset,
            import.field_name_length);

    JSObject::AddProperty(entry, module_string, import_module.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, name_string, import_name.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, kind_string, import_kind, NONE);

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> wasm::GetExports(Isolate* isolate,
                                 Handle<WasmModuleObject> module_object) {
  Handle<WasmCompiledModule> compiled_module(module_object->compiled_module(),
                                             isolate);
  Factory* factory = isolate->factory();

  Handle<String> name_string = factory->InternalizeUtf8String("name");
  Handle<String> kind_string = factory->InternalizeUtf8String("kind");

  Handle<String> function_string = factory->InternalizeUtf8String("function");
  Handle<String> table_string = factory->InternalizeUtf8String("table");
  Handle<String> memory_string = factory->InternalizeUtf8String("memory");
  Handle<String> global_string = factory->InternalizeUtf8String("global");

  // Create the result array.
  WasmModule* module = compiled_module->module();
  int num_exports = static_cast<int>(module->export_table.size());
  Handle<JSArray> array_object = factory->NewJSArray(FAST_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_exports);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_exports));

  Handle<JSFunction> object_function =
      Handle<JSFunction>(isolate->native_context()->object_function(), isolate);

  // Populate the result array.
  for (int index = 0; index < num_exports; ++index) {
    WasmExport& exp = module->export_table[index];

    Handle<String> export_kind;
    switch (exp.kind) {
      case kExternalFunction:
        export_kind = function_string;
        break;
      case kExternalTable:
        export_kind = table_string;
        break;
      case kExternalMemory:
        export_kind = memory_string;
        break;
      case kExternalGlobal:
        export_kind = global_string;
        break;
      default:
        UNREACHABLE();
    }

    Handle<JSObject> entry = factory->NewJSObject(object_function);

    MaybeHandle<String> export_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, exp.name_offset, exp.name_length);

    JSObject::AddProperty(entry, name_string, export_name.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, kind_string, export_kind, NONE);

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> wasm::GetCustomSections(Isolate* isolate,
                                        Handle<WasmModuleObject> module_object,
                                        Handle<String> name,
                                        ErrorThrower* thrower) {
  Handle<WasmCompiledModule> compiled_module(module_object->compiled_module(),
                                             isolate);
  Factory* factory = isolate->factory();

  std::vector<CustomSectionOffset> custom_sections;
  {
    DisallowHeapAllocation no_gc;  // for raw access to string bytes.
    Handle<SeqOneByteString> module_bytes(compiled_module->module_bytes(),
                                          isolate);
    const byte* start =
        reinterpret_cast<const byte*>(module_bytes->GetCharsAddress());
    const byte* end = start + module_bytes->length();
    custom_sections = DecodeCustomSections(start, end);
  }

  std::vector<Handle<Object>> matching_sections;

  // Gather matching sections.
  for (auto section : custom_sections) {
    MaybeHandle<String> section_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, section.name_offset, section.name_length);

    if (!name->Equals(*section_name.ToHandleChecked())) continue;

    // Make a copy of the payload data in the section.
    void* allocation_base = nullptr;  // Set by TryAllocateBackingStore
    size_t allocation_length = 0;     // Set by TryAllocateBackingStore
    const bool enable_guard_regions = false;
    void* memory = TryAllocateBackingStore(isolate, section.payload_length,
                                           enable_guard_regions,
                                           allocation_base, allocation_length);

    Handle<Object> section_data = factory->undefined_value();
    if (memory) {
      Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
      const bool is_external = false;
      JSArrayBuffer::Setup(buffer, isolate, is_external, allocation_base,
                           allocation_length, memory,
                           static_cast<int>(section.payload_length));
      DisallowHeapAllocation no_gc;  // for raw access to string bytes.
      Handle<SeqOneByteString> module_bytes(compiled_module->module_bytes(),
                                            isolate);
      const byte* start =
          reinterpret_cast<const byte*>(module_bytes->GetCharsAddress());
      memcpy(memory, start + section.payload_offset, section.payload_length);
      section_data = buffer;
    } else {
      thrower->RangeError("out of memory allocating custom section data");
      return Handle<JSArray>();
    }

    matching_sections.push_back(section_data);
  }

  int num_custom_sections = static_cast<int>(matching_sections.size());
  Handle<JSArray> array_object = factory->NewJSArray(FAST_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_custom_sections);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_custom_sections));

  for (int i = 0; i < num_custom_sections; i++) {
    storage->set(i, *matching_sections[i]);
  }

  return array_object;
}

bool wasm::SyncValidate(Isolate* isolate, const ModuleWireBytes& bytes) {
  if (bytes.start() == nullptr || bytes.length() == 0) return false;
  ModuleResult result =
      DecodeWasmModule(isolate, bytes.start(), bytes.end(), true, kWasmOrigin);
  return result.ok();
}

MaybeHandle<WasmModuleObject> wasm::SyncCompileTranslatedAsmJs(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {

  ModuleResult result = DecodeWasmModule(isolate, bytes.start(), bytes.end(),
                                         false, kAsmJsOrigin);
  if (result.failed()) {
    thrower->CompileFailed("Wasm decoding failed", result);
    return {};
  }

  // Transfer ownership to the {WasmModuleWrapper} generated in
  // {CompileToModuleObject}.
  constexpr bool is_sync = true;
  CompilationHelper helper(isolate, std::move(result.val), is_sync);
  return helper.CompileToModuleObject(thrower, bytes, asm_js_script,
                                      asm_js_offset_table_bytes);
}

MaybeHandle<WasmModuleObject> wasm::SyncCompile(Isolate* isolate,
                                                ErrorThrower* thrower,
                                                const ModuleWireBytes& bytes) {
  if (!IsWasmCodegenAllowed(isolate, isolate->native_context())) {
    thrower->CompileError("Wasm code generation disallowed in this context");
    return {};
  }

  ModuleResult result =
      DecodeWasmModule(isolate, bytes.start(), bytes.end(), false, kWasmOrigin);
  if (result.failed()) {
    thrower->CompileFailed("Wasm decoding failed", result);
    return {};
  }

  // Transfer ownership to the {WasmModuleWrapper} generated in
  // {CompileToModuleObject}.
  constexpr bool is_sync = true;
  CompilationHelper helper(isolate, std::move(result.val), is_sync);
  return helper.CompileToModuleObject(thrower, bytes, Handle<Script>(),
                                      Vector<const byte>());
}

MaybeHandle<WasmInstanceObject> wasm::SyncInstantiate(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory) {
  InstantiationHelper helper(isolate, thrower, module_object, imports, memory);
  return helper.Build();
}

namespace {

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

void wasm::AsyncInstantiate(Isolate* isolate, Handle<JSPromise> promise,
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

// Encapsulates all the state and steps of an asynchronous compilation.
// An asynchronous compile job consists of a number of tasks that are executed
// as foreground and background tasks. Any phase that touches the V8 heap or
// allocates on the V8 heap (e.g. creating the module object) must be a
// foreground task. All other tasks (e.g. decoding and validating, the majority
// of the work of compilation) can be background tasks.
// TODO(wasm): factor out common parts of this with the synchronous pipeline.
//
// Note: In predictable mode, DoSync and DoAsync execute the referenced function
// immediately before returning. Thus we handle the predictable mode specially,
// e.g. when we synchronizing tasks or when we delete the AyncCompileJob.
class AsyncCompileJob {
  // TODO(ahaas): Fix https://bugs.chromium.org/p/v8/issues/detail?id=6263 to
  // make sure that d8 does not shut down before the AsyncCompileJob is
  // finished.
 public:
  explicit AsyncCompileJob(Isolate* isolate, std::unique_ptr<byte[]> bytes_copy,
                           size_t length, Handle<Context> context,
                           Handle<JSPromise> promise)
      : isolate_(isolate),
        bytes_copy_(std::move(bytes_copy)),
        wire_bytes_(bytes_copy_.get(), bytes_copy_.get() + length) {
    // The handles for the context and promise must be deferred.
    DeferredHandleScope deferred(isolate);
    context_ = Handle<Context>(*context);
    module_promise_ = Handle<JSPromise>(*promise);
    deferred_handles_.push_back(deferred.Detach());
  }

  void Start() {
    DoAsync<DecodeModule>();  // --
  }

  ~AsyncCompileJob() {
    for (auto d : deferred_handles_) delete d;
  }

 private:
  Isolate* isolate_;
  std::unique_ptr<byte[]> bytes_copy_;
  ModuleWireBytes wire_bytes_;
  Handle<Context> context_;
  Handle<JSPromise> module_promise_;
  std::unique_ptr<CompilationHelper> helper_;
  std::unique_ptr<ModuleBytesEnv> module_bytes_env_;

  bool failed_ = false;
  std::vector<DeferredHandles*> deferred_handles_;
  Handle<WasmModuleObject> module_object_;
  Handle<FixedArray> function_tables_;
  Handle<FixedArray> signature_tables_;
  Handle<WasmCompiledModule> compiled_module_;
  Handle<FixedArray> code_table_;
  std::unique_ptr<WasmInstance> temp_instance_ = nullptr;
  size_t outstanding_units_ = 0;
  size_t num_background_tasks_ = 0;

  void ReopenHandlesInDeferredScope() {
    DeferredHandleScope deferred(isolate_);
    function_tables_ = handle(*function_tables_, isolate_);
    signature_tables_ = handle(*signature_tables_, isolate_);
    code_table_ = handle(*code_table_, isolate_);
    temp_instance_->ReopenHandles(isolate_);
    for (auto& unit : helper_->compilation_units_) {
      unit->ReopenCentryStub();
    }
    deferred_handles_.push_back(deferred.Detach());
  }

  void AsyncCompileFailed(ErrorThrower& thrower) {
    RejectPromise(isolate_, context_, thrower, module_promise_);
    // The AsyncCompileJob is finished, we resolved the promise, we do not need
    // the data anymore. We can delete the AsyncCompileJob object.
    if (!FLAG_verify_predictable) delete this;
  }

  void AsyncCompileSucceeded(Handle<Object> result) {
    ResolvePromise(isolate_, context_, module_promise_, result);
    // The AsyncCompileJob is finished, we resolved the promise, we do not need
    // the data anymore. We can delete the AsyncCompileJob object.
    if (!FLAG_verify_predictable) delete this;
  }

  enum TaskType { SYNC, ASYNC };

  // A closure to run a compilation step (either as foreground or background
  // task) and schedule the next step(s), if any.
  class CompileTask : NON_EXPORTED_BASE(public v8::Task) {
   public:
    AsyncCompileJob* job_ = nullptr;
    CompileTask() {}
    void Run() override = 0;  // Force sub-classes to override Run().
  };

  class AsyncCompileTask : public CompileTask {};

  class SyncCompileTask : public CompileTask {
   public:
    void Run() final {
      SaveContext saved_context(job_->isolate_);
      job_->isolate_->set_context(*job_->context_);
      RunImpl();
    }

   protected:
    virtual void RunImpl() = 0;
  };

  template <typename Task, typename... Args>
  void DoSync(Args&&... args) {
    static_assert(std::is_base_of<SyncCompileTask, Task>::value,
                  "Scheduled type must be sync");
    Task* task = new Task(std::forward<Args>(args)...);
    task->job_ = this;
    V8::GetCurrentPlatform()->CallOnForegroundThread(
        reinterpret_cast<v8::Isolate*>(isolate_), task);
  }

  template <typename Task, typename... Args>
  void DoAsync(Args&&... args) {
    static_assert(std::is_base_of<AsyncCompileTask, Task>::value,
                  "Scheduled type must be async");
    Task* task = new Task(std::forward<Args>(args)...);
    task->job_ = this;
    V8::GetCurrentPlatform()->CallOnBackgroundThread(
        task, v8::Platform::kShortRunningTask);
  }

  //==========================================================================
  // Step 1: (async) Decode the module.
  //==========================================================================
  class DecodeModule : public AsyncCompileTask {
    void Run() override {
      ModuleResult result;
      {
        DisallowHandleAllocation no_handle;
        DisallowHeapAllocation no_allocation;
        // Decode the module bytes.
        TRACE_COMPILE("(1) Decoding module...\n");
        constexpr bool is_sync = true;
        result = DecodeWasmModule(job_->isolate_, job_->wire_bytes_.start(),
                                  job_->wire_bytes_.end(), false, kWasmOrigin,
                                  !is_sync);
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
  class DecodeFail : public SyncCompileTask {
   public:
    explicit DecodeFail(ModuleResult result) : result_(std::move(result)) {}

   private:
    ModuleResult result_;
    void RunImpl() override {
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
  class PrepareAndStartCompile : public SyncCompileTask {
   public:
    explicit PrepareAndStartCompile(std::unique_ptr<WasmModule> module)
        : module_(std::move(module)) {}

   private:
    std::unique_ptr<WasmModule> module_;
    void RunImpl() override {
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
        job_->function_tables_->set(i,
                                    *job_->temp_instance_->function_tables[i]);
        job_->signature_tables_->set(
            i, *job_->temp_instance_->signature_tables[i]);
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

      job_->isolate_->counters()->wasm_functions_per_wasm_module()->AddSample(
          static_cast<int>(module_->functions.size()));

      // Transfer ownership of the {WasmModule} to the {CompilationHelper}, but
      // keep a pointer.
      WasmModule* module = module_.get();
      constexpr bool is_sync = true;
      job_->helper_.reset(
          new CompilationHelper(job_->isolate_, std::move(module_), !is_sync));

      DCHECK_LE(module->num_imported_functions, module->functions.size());
      size_t num_functions =
          module->functions.size() - module->num_imported_functions;
      if (num_functions == 0) {
        job_->ReopenHandlesInDeferredScope();
        // Degenerate case of an empty module.
        job_->DoSync<FinishCompile>();
        return;
      }

      // Start asynchronous compilation tasks.
      job_->num_background_tasks_ =
          Max(static_cast<size_t>(1),
              Min(num_functions,
                  Min(static_cast<size_t>(FLAG_wasm_num_compilation_tasks),
                      V8::GetCurrentPlatform()
                          ->NumberOfAvailableBackgroundThreads())));
      job_->module_bytes_env_.reset(new ModuleBytesEnv(
          module, job_->temp_instance_.get(), job_->wire_bytes_));
      job_->outstanding_units_ = job_->helper_->InitializeParallelCompilation(
          module->functions, *job_->module_bytes_env_);

      // Reopen all handles which should survive in the DeferredHandleScope.
      job_->ReopenHandlesInDeferredScope();
      for (size_t i = 0; i < job_->num_background_tasks_; ++i) {
        job_->DoAsync<ExecuteCompilationUnits>();
      }
    }
  };

  //==========================================================================
  // Step 3 (async x K tasks): Execute compilation units.
  //==========================================================================
  class ExecuteCompilationUnits : public AsyncCompileTask {
    void Run() override {
      TRACE_COMPILE("(3) Compiling...\n");
      for (;;) {
        {
          DisallowHandleAllocation no_handle;
          DisallowHeapAllocation no_allocation;
          if (!job_->helper_->FetchAndExecuteCompilationUnit()) break;
        }
        // TODO(ahaas): Create one FinishCompilationUnit job for all compilation
        // units.
        job_->DoSync<FinishCompilationUnit>();
        // TODO(ahaas): Limit the number of outstanding compilation units to be
        // finished to reduce memory overhead.
      }
      // Special handling for predictable mode, see above.
      if (!FLAG_verify_predictable)
        job_->helper_->module_->pending_tasks.get()->Signal();
    }
  };

  //==========================================================================
  // Step 4 (sync x each function): Finish a single compilation unit.
  //==========================================================================
  class FinishCompilationUnit : public SyncCompileTask {
    void RunImpl() override {
      TRACE_COMPILE("(4a) Finishing compilation unit...\n");
      HandleScope scope(job_->isolate_);
      if (job_->failed_) return;  // already failed

      int func_index = -1;
      ErrorThrower thrower(job_->isolate_, "AsyncCompile");
      Handle<Code> result =
          job_->helper_->FinishCompilationUnit(&thrower, &func_index);
      if (thrower.error()) {
        job_->failed_ = true;
      } else {
        DCHECK(func_index >= 0);
        job_->code_table_->set(func_index, *(result));
      }
      if (thrower.error() || --job_->outstanding_units_ == 0) {
        // All compilation units are done. We still need to wait for the
        // background tasks to shut down and only then is it safe to finish the
        // compile and delete this job. We can wait for that to happen also
        // in a background task.
        job_->DoAsync<WaitForBackgroundTasks>(std::move(thrower));
      }
    }
  };

  //==========================================================================
  // Step 4b (async): Wait for all background tasks to finish.
  //==========================================================================
  class WaitForBackgroundTasks : public AsyncCompileTask {
   public:
    explicit WaitForBackgroundTasks(ErrorThrower thrower)
        : thrower_(std::move(thrower)) {}

   private:
    ErrorThrower thrower_;

    void Run() override {
      TRACE_COMPILE("(4b) Waiting for background tasks...\n");
      // Bump next_unit_, such that background tasks stop processing the queue.
      job_->helper_->next_unit_.SetValue(
          job_->helper_->compilation_units_.size());
      // Special handling for predictable mode, see above.
      if (!FLAG_verify_predictable) {
        for (size_t i = 0; i < job_->num_background_tasks_; ++i) {
          // We wait for it to finish.
          job_->helper_->module_->pending_tasks.get()->Wait();
        }
      }
      if (thrower_.error()) {
        job_->DoSync<FailCompile>(std::move(thrower_));
      } else {
        job_->DoSync<FinishCompile>();
      }
    }
  };

  //==========================================================================
  // Step 5a (sync): Fail compilation (reject promise).
  //==========================================================================
  class FailCompile : public SyncCompileTask {
   public:
    explicit FailCompile(ErrorThrower thrower) : thrower_(std::move(thrower)) {}

   private:
    ErrorThrower thrower_;

    void RunImpl() override {
      TRACE_COMPILE("(5a) Fail compilation...\n");
      HandleScope scope(job_->isolate_);
      return job_->AsyncCompileFailed(thrower_);
    }
  };

  //==========================================================================
  // Step 5b (sync): Finish heap-allocated data structures.
  //==========================================================================
  class FinishCompile : public SyncCompileTask {
    void RunImpl() override {
      TRACE_COMPILE("(5b) Finish compile...\n");
      HandleScope scope(job_->isolate_);
      // At this point, compilation has completed. Update the code table.
      constexpr bool is_sync = true;
      for (size_t i = FLAG_skip_compiling_wasm_funcs;
           i < job_->temp_instance_->function_code.size(); ++i) {
        Code* code = Code::cast(job_->code_table_->get(static_cast<int>(i)));
        RecordStats(job_->isolate_, code, !is_sync);
      }

      // Create heap objects for script and module bytes to be stored in the
      // shared module data. Asm.js is not compiled asynchronously.
      Handle<Script> script =
          CreateWasmScript(job_->isolate_, job_->wire_bytes_);
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
          job_->isolate_, job_->helper_->module_.release());

      // Create the shared module data.
      // TODO(clemensh): For the same module (same bytes / same hash), we should
      // only have one WasmSharedModuleData. Otherwise, we might only set
      // breakpoints on a (potentially empty) subset of the instances.

      Handle<WasmSharedModuleData> shared = WasmSharedModuleData::New(
          job_->isolate_, module_wrapper,
          Handle<SeqOneByteString>::cast(module_bytes), script,
          asm_js_offset_table);

      // Create the compiled module object and populate with compiled functions
      // and information needed at instantiation time. This object needs to be
      // serializable. Instantiation may occur off a deserialized version of
      // this object.
      job_->compiled_module_ = WasmCompiledModule::New(
          job_->isolate_, shared, job_->code_table_, job_->function_tables_,
          job_->signature_tables_);

      // Finish the WASM script now and make it public to the debugger.
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
  // Step 6 (sync): Compile JS->WASM wrappers.
  //==========================================================================
  class CompileWrappers : public SyncCompileTask {
    void RunImpl() override {
      TRACE_COMPILE("(6) Compile wrappers...\n");
      // Compile JS->WASM wrappers for exported functions.
      HandleScope scope(job_->isolate_);
      JSToWasmWrapperCache js_to_wasm_cache;
      int func_index = 0;
      constexpr bool is_sync = true;
      WasmModule* module = job_->compiled_module_->module();
      for (auto exp : module->export_table) {
        if (exp.kind != kExternalFunction) continue;
        Handle<Code> wasm_code(Code::cast(job_->code_table_->get(exp.index)),
                               job_->isolate_);
        Handle<Code> wrapper_code =
            js_to_wasm_cache.CloneOrCompileJSToWasmWrapper(
                job_->isolate_, module, wasm_code, exp.index);
        int export_index =
            static_cast<int>(module->functions.size() + func_index);
        job_->code_table_->set(export_index, *wrapper_code);
        RecordStats(job_->isolate_, *wrapper_code, !is_sync);
        func_index++;
      }

      job_->DoSync<FinishModule>();
    }
  };

  //==========================================================================
  // Step 7 (sync): Finish the module and resolve the promise.
  //==========================================================================
  class FinishModule : public SyncCompileTask {
    void RunImpl() override {
      TRACE_COMPILE("(7) Finish module...\n");
      HandleScope scope(job_->isolate_);
      Handle<WasmModuleObject> result =
          WasmModuleObject::New(job_->isolate_, job_->compiled_module_);
      // {job_} is deleted in AsyncCompileSucceeded, therefore the {return}.
      return job_->AsyncCompileSucceeded(result);
    }
  };
};

void wasm::AsyncCompile(Isolate* isolate, Handle<JSPromise> promise,
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

  // Make a copy of the wire bytes in case the user program changes them
  // during asynchronous compilation.
  std::unique_ptr<byte[]> copy(new byte[bytes.length()]);
  memcpy(copy.get(), bytes.start(), bytes.length());
  auto job = new AsyncCompileJob(isolate, std::move(copy), bytes.length(),
                                 handle(isolate->context()), promise);
  job->Start();
  // Special handling for predictable mode, see above.
  if (FLAG_verify_predictable) delete job;
}

Handle<Code> wasm::CompileLazy(Isolate* isolate) {
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
  if (lazy_compile_code->deoptimization_data()->length() > 0) {
    // Then it's an indirect call or via JS->WASM wrapper.
    DCHECK_LE(2, lazy_compile_code->deoptimization_data()->length());
    exp_deopt_data = handle(lazy_compile_code->deoptimization_data(), isolate);
    auto* weak_cell = WeakCell::cast(exp_deopt_data->get(0));
    instance = handle(WasmInstanceObject::cast(weak_cell->value()), isolate);
    func_index = Smi::cast(exp_deopt_data->get(1))->value();
  }
  it.Advance();
  // Third frame: The calling wasm code or js-to-wasm wrapper.
  DCHECK(!it.done());
  DCHECK(it.frame()->is_js_to_wasm() || it.frame()->is_wasm_compiled());
  Handle<Code> caller_code = handle(it.frame()->LookupCode(), isolate);
  if (it.frame()->is_js_to_wasm()) {
    DCHECK(!instance.is_null());
  } else if (instance.is_null()) {
    instance = handle(wasm::GetOwningWasmInstance(*caller_code), isolate);
  } else {
    DCHECK(*instance == wasm::GetOwningWasmInstance(*caller_code));
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

  Handle<Code> compiled_code = WasmCompiledModule::CompileLazy(
      isolate, instance, caller_code, offset, func_index, patch_caller);
  if (!exp_deopt_data.is_null() && exp_deopt_data->length() > 2) {
    // See EnsureExportedLazyDeoptData: exp_deopt_data[2...(len-1)] are pairs of
    // <export_table, index> followed by undefined values.
    // Use this information here to patch all export tables.
    DCHECK_EQ(0, exp_deopt_data->length() % 2);
    for (int idx = 2, end = exp_deopt_data->length(); idx < end; idx += 2) {
      if (exp_deopt_data->get(idx)->IsUndefined(isolate)) break;
      FixedArray* exp_table = FixedArray::cast(exp_deopt_data->get(idx));
      int exp_index = Smi::cast(exp_deopt_data->get(idx + 1))->value();
      DCHECK(exp_table->get(exp_index) == *lazy_compile_code);
      exp_table->set(exp_index, *compiled_code);
    }
    // After processing, remove the list of exported entries, such that we don't
    // do the patching redundantly.
    Handle<FixedArray> new_deopt_data =
        isolate->factory()->CopyFixedArrayUpTo(exp_deopt_data, 2, TENURED);
    lazy_compile_code->set_deoptimization_data(*new_deopt_data);
  }

  return compiled_code;
}

void LazyCompilationOrchestrator::CompileFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int func_index) {
  Handle<WasmCompiledModule> compiled_module(instance->compiled_module(),
                                             isolate);
  if (Code::cast(compiled_module->code_table()->get(func_index))->kind() ==
      Code::WASM_FUNCTION) {
    return;
  }

  size_t num_function_tables =
      compiled_module->module()->function_tables.size();
  // Store a vector of handles to be embedded in the generated code.
  // TODO(clemensh): For concurrent compilation, these will have to live in a
  // DeferredHandleScope.
  std::vector<Handle<FixedArray>> fun_tables(num_function_tables);
  std::vector<Handle<FixedArray>> sig_tables(num_function_tables);
  for (size_t i = 0; i < num_function_tables; ++i) {
    Object* fun_table =
        compiled_module->function_tables()->get(static_cast<int>(i));
    fun_tables[i] = handle(FixedArray::cast(fun_table), isolate);
    Object* sig_table =
        compiled_module->signature_tables()->get(static_cast<int>(i));
    sig_tables[i] = handle(FixedArray::cast(sig_table), isolate);
  }
  wasm::ModuleEnv module_env(compiled_module->module(), &fun_tables,
                             &sig_tables);
  uint8_t* module_start = compiled_module->module_bytes()->GetChars();
  const WasmFunction* func = &module_env.module->functions[func_index];
  wasm::FunctionBody body{func->sig, module_start,
                          module_start + func->code_start_offset,
                          module_start + func->code_end_offset};
  // TODO(wasm): Refactor this to only get the name if it is really needed for
  // tracing / debugging.
  std::string func_name;
  {
    wasm::WasmName name = Vector<const char>::cast(
        compiled_module->GetRawFunctionName(func_index));
    // Copy to std::string, because the underlying string object might move on
    // the heap.
    func_name.assign(name.start(), static_cast<size_t>(name.length()));
  }
  ErrorThrower thrower(isolate, "WasmLazyCompile");
  compiler::WasmCompilationUnit unit(isolate, &module_env, body,
                                     CStrVector(func_name.c_str()), func_index);
  unit.ExecuteCompilation();
  Handle<Code> code = unit.FinishCompilation(&thrower);

  // If there is a pending error, something really went wrong. The module was
  // verified before starting execution with lazy compilation.
  // This might be OOM, but then we cannot continue execution anyway.
  CHECK(!thrower.error());

  Handle<FixedArray> deopt_data = isolate->factory()->NewFixedArray(2, TENURED);
  Handle<WeakCell> weak_instance = isolate->factory()->NewWeakCell(instance);
  // TODO(wasm): Introduce constants for the indexes in wasm deopt data.
  deopt_data->set(0, *weak_instance);
  deopt_data->set(1, Smi::FromInt(func_index));
  code->set_deoptimization_data(*deopt_data);

  DCHECK_EQ(Builtins::kWasmCompileLazy,
            Code::cast(compiled_module->code_table()->get(func_index))
                ->builtin_index());
  compiled_module->code_table()->set(func_index, *code);

  // Now specialize the generated code for this instance.
  Zone specialization_zone(isolate->allocator(), ZONE_NAME);
  CodeSpecialization code_specialization(isolate, &specialization_zone);
  if (module_env.module->globals_size) {
    Address globals_start =
        reinterpret_cast<Address>(compiled_module->globals_start());
    code_specialization.RelocateGlobals(nullptr, globals_start);
  }
  if (instance->has_memory_buffer()) {
    Address mem_start =
        reinterpret_cast<Address>(instance->memory_buffer()->backing_store());
    int mem_size = instance->memory_buffer()->byte_length()->Number();
    DCHECK_IMPLIES(mem_size == 0, mem_start == nullptr);
    if (mem_size > 0) {
      code_specialization.RelocateMemoryReferences(nullptr, 0, mem_start,
                                                   mem_size);
    }
  }
  code_specialization.RelocateDirectCalls(instance);
  code_specialization.ApplyToWasmCode(*code, SKIP_ICACHE_FLUSH);
  Assembler::FlushICache(isolate, code->instruction_start(),
                         code->instruction_size());
  RecordLazyCodeStats(isolate, *code);
}

Handle<Code> LazyCompilationOrchestrator::CompileLazy(
    Isolate* isolate, Handle<WasmInstanceObject> instance, Handle<Code> caller,
    int call_offset, int exported_func_index, bool patch_caller) {
  struct NonCompiledFunction {
    int offset;
    int func_index;
  };
  std::vector<NonCompiledFunction> non_compiled_functions;
  int func_to_return_idx = exported_func_index;
  wasm::Decoder decoder(nullptr, nullptr);
  bool is_js_to_wasm = caller->kind() == Code::JS_TO_WASM_FUNCTION;
  Handle<WasmCompiledModule> compiled_module(instance->compiled_module(),
                                             isolate);

  if (is_js_to_wasm) {
    non_compiled_functions.push_back({0, exported_func_index});
  } else if (patch_caller) {
    DisallowHeapAllocation no_gc;
    SeqOneByteString* module_bytes = compiled_module->module_bytes();
    SourcePositionTableIterator source_pos_iterator(
        caller->SourcePositionTable());
    DCHECK_EQ(2, caller->deoptimization_data()->length());
    int caller_func_index =
        Smi::cast(caller->deoptimization_data()->get(1))->value();
    const byte* func_bytes =
        module_bytes->GetChars() + compiled_module->module()
                                       ->functions[caller_func_index]
                                       .code_start_offset;
    for (RelocIterator it(*caller, RelocInfo::kCodeTargetMask); !it.done();
         it.next()) {
      Code* callee =
          Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
      if (callee->builtin_index() != Builtins::kWasmCompileLazy) continue;
      // TODO(clemensh): Introduce safe_cast<T, bool> which (D)CHECKS
      // (depending on the bool) against limits of T and then static_casts.
      size_t offset_l = it.rinfo()->pc() - caller->instruction_start();
      DCHECK_GE(kMaxInt, offset_l);
      int offset = static_cast<int>(offset_l);
      int byte_pos =
          AdvanceSourcePositionTableIterator(source_pos_iterator, offset);
      int called_func_index =
          ExtractDirectCallIndex(decoder, func_bytes + byte_pos);
      non_compiled_functions.push_back({offset, called_func_index});
      // Call offset one instruction after the call. Remember the last called
      // function before that offset.
      if (offset < call_offset) func_to_return_idx = called_func_index;
    }
  }

  // TODO(clemensh): compile all functions in non_compiled_functions in
  // background, wait for func_to_return_idx.
  CompileFunction(isolate, instance, func_to_return_idx);

  if (is_js_to_wasm || patch_caller) {
    DisallowHeapAllocation no_gc;
    // Now patch the code object with all functions which are now compiled.
    int idx = 0;
    for (RelocIterator it(*caller, RelocInfo::kCodeTargetMask); !it.done();
         it.next()) {
      Code* callee =
          Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
      if (callee->builtin_index() != Builtins::kWasmCompileLazy) continue;
      DCHECK_GT(non_compiled_functions.size(), idx);
      int called_func_index = non_compiled_functions[idx].func_index;
      // Check that the callee agrees with our assumed called_func_index.
      DCHECK_IMPLIES(
          callee->deoptimization_data()->length() > 0,
          Smi::cast(callee->deoptimization_data()->get(1))->value() ==
              called_func_index);
      if (is_js_to_wasm) {
        DCHECK_EQ(func_to_return_idx, called_func_index);
      } else {
        DCHECK_EQ(non_compiled_functions[idx].offset,
                  it.rinfo()->pc() - caller->instruction_start());
      }
      ++idx;
      Handle<Code> callee_compiled(
          Code::cast(compiled_module->code_table()->get(called_func_index)));
      if (callee_compiled->builtin_index() == Builtins::kWasmCompileLazy) {
        DCHECK_NE(func_to_return_idx, called_func_index);
        continue;
      }
      DCHECK_EQ(Code::WASM_FUNCTION, callee_compiled->kind());
      it.rinfo()->set_target_address(isolate,
                                     callee_compiled->instruction_start());
    }
    DCHECK_EQ(non_compiled_functions.size(), idx);
  }

  Code* ret =
      Code::cast(compiled_module->code_table()->get(func_to_return_idx));
  DCHECK_EQ(Code::WASM_FUNCTION, ret->kind());
  return handle(ret, isolate);
}

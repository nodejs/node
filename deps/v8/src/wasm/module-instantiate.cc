// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-instantiate.h"

#include "src/asmjs/asm-js.h"
#include "src/logging/counters.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/tracing/trace-event.h"
#include "src/utils/utils.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-external-refs.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_wasm_instances) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {
namespace wasm {

namespace {
byte* raw_buffer_ptr(MaybeHandle<JSArrayBuffer> buffer, int offset) {
  return static_cast<byte*>(buffer.ToHandleChecked()->backing_store()) + offset;
}

uint32_t EvalUint32InitExpr(Handle<WasmInstanceObject> instance,
                            const WasmInitExpr& expr) {
  switch (expr.kind) {
    case WasmInitExpr::kI32Const:
      return expr.val.i32_const;
    case WasmInitExpr::kGlobalIndex: {
      uint32_t offset =
          instance->module()->globals[expr.val.global_index].offset;
      auto raw_addr = reinterpret_cast<Address>(
                          instance->untagged_globals_buffer().backing_store()) +
                      offset;
      return ReadLittleEndianValue<uint32_t>(raw_addr);
    }
    default:
      UNREACHABLE();
  }
}

// Queue of import wrapper keys to compile for an instance.
class ImportWrapperQueue {
 public:
  // Removes an arbitrary cache key from the queue and returns it.
  // If the queue is empty, returns nullopt.
  // Thread-safe.
  base::Optional<WasmImportWrapperCache::CacheKey> pop() {
    base::Optional<WasmImportWrapperCache::CacheKey> key = base::nullopt;
    base::LockGuard<base::Mutex> lock(&mutex_);
    auto it = queue_.begin();
    if (it != queue_.end()) {
      key = *it;
      queue_.erase(it);
    }
    return key;
  }

  // Add the given key to the queue.
  // Not thread-safe.
  void insert(const WasmImportWrapperCache::CacheKey& key) {
    queue_.insert(key);
  }

 private:
  base::Mutex mutex_;
  std::unordered_set<WasmImportWrapperCache::CacheKey,
                     WasmImportWrapperCache::CacheKeyHash>
      queue_;
};

class CompileImportWrapperTask final : public CancelableTask {
 public:
  CompileImportWrapperTask(
      CancelableTaskManager* task_manager, WasmEngine* engine,
      Counters* counters, NativeModule* native_module,
      ImportWrapperQueue* queue,
      WasmImportWrapperCache::ModificationScope* cache_scope)
      : CancelableTask(task_manager),
        engine_(engine),
        counters_(counters),
        native_module_(native_module),
        queue_(queue),
        cache_scope_(cache_scope) {}

  void RunInternal() override {
    while (base::Optional<WasmImportWrapperCache::CacheKey> key =
               queue_->pop()) {
      CompileImportWrapper(engine_, native_module_, counters_, key->first,
                           key->second, cache_scope_);
    }
  }

 private:
  WasmEngine* const engine_;
  Counters* const counters_;
  NativeModule* const native_module_;
  ImportWrapperQueue* const queue_;
  WasmImportWrapperCache::ModificationScope* const cache_scope_;
};

}  // namespace

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
  Handle<JSArrayBuffer> untagged_globals_;
  Handle<FixedArray> tagged_globals_;
  std::vector<Handle<WasmExceptionObject>> exception_wrappers_;
  Handle<WasmExportedFunction> start_function_;
  std::vector<SanitizedImport> sanitized_imports_;

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

  // Load data segments into the memory.
  void LoadDataSegments(Handle<WasmInstanceObject> instance);

  void WriteGlobalValue(const WasmGlobal& global, double value);
  void WriteGlobalValue(const WasmGlobal& global, int64_t num);
  void WriteGlobalValue(const WasmGlobal& global,
                        Handle<WasmGlobalObject> value);

  void WriteGlobalAnyRef(const WasmGlobal& global, Handle<Object> value);

  void SanitizeImports();

  // Find the imported memory buffer if there is one. This is used to see if we
  // need to recompile with bounds checks before creating the instance.
  MaybeHandle<JSArrayBuffer> FindImportedMemoryBuffer() const;

  // Processes a single imported function.
  bool ProcessImportedFunction(Handle<WasmInstanceObject> instance,
                               int import_index, int func_index,
                               Handle<String> module_name,
                               Handle<String> import_name,
                               Handle<Object> value);

  // Initialize imported tables of type anyfunc.
  bool InitializeImportedIndirectFunctionTable(
      Handle<WasmInstanceObject> instance, int import_index,
      Handle<WasmTableObject> table_object);

  // Process a single imported table.
  bool ProcessImportedTable(Handle<WasmInstanceObject> instance,
                            int import_index, int table_index,
                            Handle<String> module_name,
                            Handle<String> import_name, Handle<Object> value);

  // Process a single imported memory.
  bool ProcessImportedMemory(Handle<WasmInstanceObject> instance,
                             int import_index, Handle<String> module_name,
                             Handle<String> import_name, Handle<Object> value);

  // Process a single imported global.
  bool ProcessImportedGlobal(Handle<WasmInstanceObject> instance,
                             int import_index, int global_index,
                             Handle<String> module_name,
                             Handle<String> import_name, Handle<Object> value);

  // Process a single imported WasmGlobalObject.
  bool ProcessImportedWasmGlobalObject(Handle<WasmInstanceObject> instance,
                                       int import_index,
                                       Handle<String> module_name,
                                       Handle<String> import_name,
                                       const WasmGlobal& global,
                                       Handle<WasmGlobalObject> global_object);

  // Compile import wrappers in parallel. The result goes into the native
  // module's import_wrapper_cache.
  void CompileImportWrappers(Handle<WasmInstanceObject> instance);

  // Process the imports, including functions, tables, globals, and memory, in
  // order, loading them from the {ffi_} object. Returns the number of imported
  // functions.
  int ProcessImports(Handle<WasmInstanceObject> instance);

  template <typename T>
  T* GetRawGlobalPtr(const WasmGlobal& global);

  // Process initialization of globals.
  void InitGlobals(Handle<WasmInstanceObject> instance);

  // Allocate memory for a module instance as a new JSArrayBuffer.
  Handle<JSArrayBuffer> AllocateMemory(uint32_t initial_pages,
                                       uint32_t maximum_pages);

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

  // From here on, we expect the build pipeline to run without exiting to JS.
  DisallowJavascriptExecution no_js(isolate_);
  // Record build time into correct bucket, then build instance.
  TimedHistogramScope wasm_instantiate_module_time_scope(SELECT_WASM_COUNTER(
      isolate_->counters(), module_->origin, wasm_instantiate, module_time));
  NativeModule* native_module = module_object_->native_module();

  //--------------------------------------------------------------------------
  // Allocate the memory array buffer.
  //--------------------------------------------------------------------------
  uint32_t initial_pages = module_->initial_pages;
  auto initial_pages_counter = SELECT_WASM_COUNTER(
      isolate_->counters(), module_->origin, wasm, min_mem_pages_count);
  initial_pages_counter->AddSample(initial_pages);
  if (module_->has_maximum_pages) {
    DCHECK_EQ(kWasmOrigin, module_->origin);
    auto max_pages_counter =
        isolate_->counters()->wasm_wasm_max_mem_pages_count();
    max_pages_counter->AddSample(module_->maximum_pages);
  }
  // Asm.js has memory_ already set at this point, so we don't want to
  // overwrite it.
  if (memory_.is_null()) {
    memory_ = FindImportedMemoryBuffer();
  }
  if (!memory_.is_null()) {
    // Set externally passed ArrayBuffer non detachable.
    Handle<JSArrayBuffer> memory = memory_.ToHandleChecked();
    memory->set_is_detachable(false);

    DCHECK_IMPLIES(native_module->use_trap_handler(),
                   module_->origin == kAsmJsOrigin ||
                       memory->is_wasm_memory() ||
                       memory->backing_store() == nullptr);
  } else if (initial_pages > 0 || native_module->use_trap_handler()) {
    // We need to unconditionally create a guard region if using trap handlers,
    // even when the size is zero to prevent null-dereference issues
    // (e.g. https://crbug.com/769637).
    // Allocate memory if the initial size is more than 0 pages.
    memory_ = AllocateMemory(initial_pages, module_->maximum_pages);
    if (memory_.is_null()) {
      // failed to allocate memory
      DCHECK(isolate_->has_pending_exception() || thrower_->error());
      return {};
    }
  }

  //--------------------------------------------------------------------------
  // Create the WebAssembly.Instance object.
  //--------------------------------------------------------------------------
  TRACE("New module instantiation for %p\n", native_module);
  Handle<WasmInstanceObject> instance =
      WasmInstanceObject::New(isolate_, module_object_);
  NativeModuleModificationScope native_modification_scope(native_module);

  //--------------------------------------------------------------------------
  // Set up the globals for the new instance.
  //--------------------------------------------------------------------------
  uint32_t untagged_globals_buffer_size = module_->untagged_globals_buffer_size;
  if (untagged_globals_buffer_size > 0) {
    void* backing_store = isolate_->array_buffer_allocator()->Allocate(
        untagged_globals_buffer_size);
    if (backing_store == nullptr) {
      thrower_->RangeError("Out of memory: wasm globals");
      return {};
    }
    untagged_globals_ = isolate_->factory()->NewJSArrayBuffer(
        SharedFlag::kNotShared, AllocationType::kOld);
    constexpr bool is_external = false;
    constexpr bool is_wasm_memory = false;
    JSArrayBuffer::Setup(untagged_globals_, isolate_, is_external,
                         backing_store, untagged_globals_buffer_size,
                         SharedFlag::kNotShared, is_wasm_memory);
    if (untagged_globals_.is_null()) {
      thrower_->RangeError("Out of memory: wasm globals");
      return {};
    }
    instance->set_globals_start(
        reinterpret_cast<byte*>(untagged_globals_->backing_store()));
    instance->set_untagged_globals_buffer(*untagged_globals_);
  }

  uint32_t tagged_globals_buffer_size = module_->tagged_globals_buffer_size;
  if (tagged_globals_buffer_size > 0) {
    tagged_globals_ = isolate_->factory()->NewFixedArray(
        static_cast<int>(tagged_globals_buffer_size));
    instance->set_tagged_globals_buffer(*tagged_globals_);
  }

  //--------------------------------------------------------------------------
  // Set up the array of references to imported globals' array buffers.
  //--------------------------------------------------------------------------
  if (module_->num_imported_mutable_globals > 0) {
    // TODO(binji): This allocates one slot for each mutable global, which is
    // more than required if multiple globals are imported from the same
    // module.
    Handle<FixedArray> buffers_array = isolate_->factory()->NewFixedArray(
        module_->num_imported_mutable_globals, AllocationType::kOld);
    instance->set_imported_mutable_globals_buffers(*buffers_array);
  }

  //--------------------------------------------------------------------------
  // Set up the exception table used for exception tag checks.
  //--------------------------------------------------------------------------
  int exceptions_count = static_cast<int>(module_->exceptions.size());
  if (exceptions_count > 0) {
    Handle<FixedArray> exception_table = isolate_->factory()->NewFixedArray(
        exceptions_count, AllocationType::kOld);
    instance->set_exceptions_table(*exception_table);
    exception_wrappers_.resize(exceptions_count);
  }

  //--------------------------------------------------------------------------
  // Set up table storage space.
  //--------------------------------------------------------------------------
  int table_count = static_cast<int>(module_->tables.size());
  Handle<FixedArray> tables = isolate_->factory()->NewFixedArray(table_count);
  for (int i = module_->num_imported_tables; i < table_count; i++) {
    const WasmTable& table = module_->tables[i];
    Handle<WasmTableObject> table_obj = WasmTableObject::New(
        isolate_, table.type, table.initial_size, table.has_maximum_size,
        table.maximum_size, nullptr);
    tables->set(i, *table_obj);
  }
  instance->set_tables(*tables);

  //--------------------------------------------------------------------------
  // Process the imports for the module.
  //--------------------------------------------------------------------------
  int num_imported_functions = ProcessImports(instance);
  if (num_imported_functions < 0) return {};

  //--------------------------------------------------------------------------
  // Process the initialization for the module's globals.
  //--------------------------------------------------------------------------
  InitGlobals(instance);

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

  // The bulk memory proposal changes the MVP behavior here; the segments are
  // written as if `memory.init` and `table.init` are executed directly, and
  // not bounds checked ahead of time.
  if (!enabled_.bulk_memory) {
    //--------------------------------------------------------------------------
    // Check that indirect function table segments are within bounds.
    //--------------------------------------------------------------------------
    for (const WasmElemSegment& elem_segment : module_->elem_segments) {
      if (!elem_segment.active) continue;
      DCHECK_LT(elem_segment.table_index, table_count);
      uint32_t base = EvalUint32InitExpr(instance, elem_segment.offset);
      // Because of imported tables, {table_size} has to come from the table
      // object itself.
      auto table_object = handle(WasmTableObject::cast(instance->tables().get(
                                     elem_segment.table_index)),
                                 isolate_);
      size_t table_size = table_object->entries().length();
      if (!IsInBounds(base, elem_segment.entries.size(), table_size)) {
        thrower_->LinkError("table initializer is out of bounds");
        return {};
      }
    }

    //--------------------------------------------------------------------------
    // Check that memory segments are within bounds.
    //--------------------------------------------------------------------------
    for (const WasmDataSegment& seg : module_->data_segments) {
      if (!seg.active) continue;
      uint32_t base = EvalUint32InitExpr(instance, seg.dest_addr);
      if (!IsInBounds(base, seg.source.length(), instance->memory_size())) {
        thrower_->LinkError("data segment is out of bounds");
        return {};
      }
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
    if (thrower_->error()) return {};
  }

  //--------------------------------------------------------------------------
  // Initialize the memory by loading data segments.
  //--------------------------------------------------------------------------
  if (module_->data_segments.size() > 0) {
    LoadDataSegments(instance);
    if (thrower_->error()) return {};
  }

  //--------------------------------------------------------------------------
  // Debugging support.
  //--------------------------------------------------------------------------
  // Set all breakpoints that were set on the shared module.
  WasmModuleObject::SetBreakpointsOnNewInstance(module_object_, instance);

  //--------------------------------------------------------------------------
  // Create a wrapper for the start function.
  //--------------------------------------------------------------------------
  if (module_->start_function_index >= 0) {
    int start_index = module_->start_function_index;
    auto& function = module_->functions[start_index];
    Handle<Code> wrapper_code = compiler::CompileJSToWasmWrapper(
                                    isolate_, function.sig, function.imported)
                                    .ToHandleChecked();
    // TODO(clemensh): Don't generate an exported function for the start
    // function. Use CWasmEntry instead.
    start_function_ = WasmExportedFunction::New(
        isolate_, instance, start_index,
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

// Load data segments into the memory.
void InstanceBuilder::LoadDataSegments(Handle<WasmInstanceObject> instance) {
  Vector<const uint8_t> wire_bytes =
      module_object_->native_module()->wire_bytes();
  for (const WasmDataSegment& segment : module_->data_segments) {
    uint32_t size = segment.source.length();

    if (enabled_.bulk_memory) {
      // Passive segments are not copied during instantiation.
      if (!segment.active) continue;

      uint32_t dest_offset = EvalUint32InitExpr(instance, segment.dest_addr);
      bool ok = ClampToBounds(dest_offset, &size,
                              static_cast<uint32_t>(instance->memory_size()));
      Address dest_addr =
          reinterpret_cast<Address>(instance->memory_start()) + dest_offset;
      Address src_addr = reinterpret_cast<Address>(wire_bytes.begin()) +
                         segment.source.offset();
      memory_copy_wrapper(dest_addr, src_addr, size);
      if (!ok) {
        thrower_->LinkError("data segment is out of bounds");
        return;
      }
    } else {
      DCHECK(segment.active);
      // Segments of size == 0 are just nops.
      if (size == 0) continue;

      uint32_t dest_offset = EvalUint32InitExpr(instance, segment.dest_addr);
      DCHECK(IsInBounds(dest_offset, size, instance->memory_size()));
      byte* dest = instance->memory_start() + dest_offset;
      const byte* src = wire_bytes.begin() + segment.source.offset();
      memcpy(dest, src, size);
    }
  }
}

void InstanceBuilder::WriteGlobalValue(const WasmGlobal& global, double num) {
  TRACE("init [globals_start=%p + %u] = %lf, type = %s\n",
        raw_buffer_ptr(untagged_globals_, 0), global.offset, num,
        ValueTypes::TypeName(global.type));
  switch (global.type) {
    case kWasmI32:
      WriteLittleEndianValue<int32_t>(GetRawGlobalPtr<int32_t>(global),
                                      DoubleToInt32(num));
      break;
    case kWasmI64:
      // The Wasm-BigInt proposal currently says that i64 globals may
      // only be initialized with BigInts. See:
      // https://github.com/WebAssembly/JS-BigInt-integration/issues/12
      UNREACHABLE();
    case kWasmF32:
      WriteLittleEndianValue<float>(GetRawGlobalPtr<float>(global),
                                    DoubleToFloat32(num));
      break;
    case kWasmF64:
      WriteLittleEndianValue<double>(GetRawGlobalPtr<double>(global), num);
      break;
    default:
      UNREACHABLE();
  }
}

void InstanceBuilder::WriteGlobalValue(const WasmGlobal& global, int64_t num) {
  TRACE("init [globals_start=%p + %u] = %" PRId64 ", type = %s\n",
        raw_buffer_ptr(untagged_globals_, 0), global.offset, num,
        ValueTypes::TypeName(global.type));
  DCHECK_EQ(kWasmI64, global.type);
  WriteLittleEndianValue<int64_t>(GetRawGlobalPtr<int64_t>(global), num);
}

void InstanceBuilder::WriteGlobalValue(const WasmGlobal& global,
                                       Handle<WasmGlobalObject> value) {
  TRACE("init [globals_start=%p + %u] = ", raw_buffer_ptr(untagged_globals_, 0),
        global.offset);
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
    case kWasmAnyRef:
    case kWasmAnyFunc:
    case kWasmExceptRef: {
      tagged_globals_->set(global.offset, *value->GetRef());
      break;
    }
    default:
      UNREACHABLE();
  }
  TRACE(", type = %s (from WebAssembly.Global)\n",
        ValueTypes::TypeName(global.type));
}

void InstanceBuilder::WriteGlobalAnyRef(const WasmGlobal& global,
                                        Handle<Object> value) {
  tagged_globals_->set(global.offset, *value, UPDATE_WRITE_BARRIER);
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

bool InstanceBuilder::ProcessImportedFunction(
    Handle<WasmInstanceObject> instance, int import_index, int func_index,
    Handle<String> module_name, Handle<String> import_name,
    Handle<Object> value) {
  // Function imports must be callable.
  if (!value->IsCallable()) {
    ReportLinkError("function import requires a callable", import_index,
                    module_name, import_name);
    return false;
  }
  auto js_receiver = Handle<JSReceiver>::cast(value);
  FunctionSig* expected_sig = module_->functions[func_index].sig;
  auto kind = compiler::GetWasmImportCallKind(js_receiver, expected_sig,
                                              enabled_.bigint);
  switch (kind) {
    case compiler::WasmImportCallKind::kLinkError:
      ReportLinkError("imported function does not match the expected type",
                      import_index, module_name, import_name);
      return false;
    case compiler::WasmImportCallKind::kWasmToWasm: {
      // The imported function is a WASM function from another instance.
      auto imported_function = Handle<WasmExportedFunction>::cast(value);
      Handle<WasmInstanceObject> imported_instance(
          imported_function->instance(), isolate_);
      // The import reference is the instance object itself.
      Address imported_target = imported_function->GetWasmCallTarget();
      ImportedFunctionEntry entry(instance, func_index);
      entry.SetWasmToWasm(*imported_instance, imported_target);
      // Also store the {WasmExportedFunction} in the instance to preserve its
      // identity.
      WasmInstanceObject::SetWasmExportedFunction(
          isolate_, instance, func_index, imported_function);
      break;
    }
    case compiler::WasmImportCallKind::kWasmToCapi: {
      NativeModule* native_module = instance->module_object().native_module();
      Address host_address = WasmCapiFunction::cast(*value).GetHostCallTarget();
      WasmCodeRefScope code_ref_scope;
      WasmCode* wasm_code = compiler::CompileWasmCapiCallWrapper(
          isolate_->wasm_engine(), native_module, expected_sig, host_address);
      isolate_->counters()->wasm_generated_code_size()->Increment(
          wasm_code->instructions().length());
      isolate_->counters()->wasm_reloc_size()->Increment(
          wasm_code->reloc_info().length());

      ImportedFunctionEntry entry(instance, func_index);
      // We re-use the SetWasmToJs infrastructure because it passes the
      // callable to the wrapper, which we need to get the function data.
      entry.SetWasmToJs(isolate_, js_receiver, wasm_code);
      break;
    }
    default: {
      // The imported function is a callable.
      NativeModule* native_module = instance->module_object().native_module();
      WasmCode* wasm_code =
          native_module->import_wrapper_cache()->Get(kind, expected_sig);
      DCHECK_NOT_NULL(wasm_code);
      ImportedFunctionEntry entry(instance, func_index);
      if (wasm_code->kind() == WasmCode::kWasmToJsWrapper) {
        // Wasm to JS wrappers are treated specially in the import table.
        entry.SetWasmToJs(isolate_, js_receiver, wasm_code);
      } else {
        // Wasm math intrinsics are compiled as regular Wasm functions.
        DCHECK(kind >= compiler::WasmImportCallKind::kFirstMathIntrinsic &&
               kind <= compiler::WasmImportCallKind::kLastMathIntrinsic);
        entry.SetWasmToWasm(*instance, wasm_code->instruction_start());
      }
      break;
    }
  }
  return true;
}

bool InstanceBuilder::InitializeImportedIndirectFunctionTable(
    Handle<WasmInstanceObject> instance, int import_index,
    Handle<WasmTableObject> table_object) {
  int imported_table_size = table_object->entries().length();
  // Allocate a new dispatch table.
  if (!instance->has_indirect_function_table()) {
    WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
        instance, imported_table_size);
  }
  // Initialize the dispatch table with the (foreign) JS functions
  // that are already in the table.
  for (int i = 0; i < imported_table_size; ++i) {
    bool is_valid;
    bool is_null;
    MaybeHandle<WasmInstanceObject> maybe_target_instance;
    int function_index;
    WasmTableObject::GetFunctionTableEntry(isolate_, table_object, i, &is_valid,
                                           &is_null, &maybe_target_instance,
                                           &function_index);
    if (!is_valid) {
      thrower_->LinkError("table import %d[%d] is not a wasm function",
                          import_index, i);
      return false;
    }
    if (is_null) continue;

    Handle<WasmInstanceObject> target_instance =
        maybe_target_instance.ToHandleChecked();
    FunctionSig* sig = target_instance->module_object()
                           .module()
                           ->functions[function_index]
                           .sig;

    // Look up the signature's canonical id. If there is no canonical
    // id, then the signature does not appear at all in this module,
    // so putting {-1} in the table will cause checks to always fail.
    IndirectFunctionTableEntry(instance, i)
        .Set(module_->signature_map.Find(*sig), target_instance,
             function_index);
  }
  return true;
}

bool InstanceBuilder::ProcessImportedTable(Handle<WasmInstanceObject> instance,
                                           int import_index, int table_index,
                                           Handle<String> module_name,
                                           Handle<String> import_name,
                                           Handle<Object> value) {
  if (!value->IsWasmTableObject()) {
    ReportLinkError("table import requires a WebAssembly.Table", import_index,
                    module_name, import_name);
    return false;
  }
  const WasmTable& table = module_->tables[table_index];

  instance->tables().set(table_index, *value);
  auto table_object = Handle<WasmTableObject>::cast(value);

  int imported_table_size = table_object->entries().length();
  if (imported_table_size < static_cast<int>(table.initial_size)) {
    thrower_->LinkError("table import %d is smaller than initial %d, got %u",
                        import_index, table.initial_size, imported_table_size);
    return false;
  }

  if (table.has_maximum_size) {
    if (table_object->maximum_length().IsUndefined(isolate_)) {
      thrower_->LinkError("table import %d has no maximum length, expected %d",
                          import_index, table.maximum_size);
      return false;
    }
    int64_t imported_maximum_size = table_object->maximum_length().Number();
    if (imported_maximum_size < 0) {
      thrower_->LinkError("table import %d has no maximum length, expected %d",
                          import_index, table.maximum_size);
      return false;
    }
    if (imported_maximum_size > table.maximum_size) {
      thrower_->LinkError("table import %d has a larger maximum size %" PRIx64
                          " than the module's declared maximum %u",
                          import_index, imported_maximum_size,
                          table.maximum_size);
      return false;
    }
  }

  if (table.type != table_object->type()) {
    ReportLinkError("imported table does not match the expected type",
                    import_index, module_name, import_name);
    return false;
  }

  // The indirect function table only exists for table 0.
  if (table.type == kWasmAnyFunc && table_index == 0 &&
      !InitializeImportedIndirectFunctionTable(instance, import_index,
                                               table_object)) {
    return false;
  }

  return true;
}

bool InstanceBuilder::ProcessImportedMemory(Handle<WasmInstanceObject> instance,
                                            int import_index,
                                            Handle<String> module_name,
                                            Handle<String> import_name,
                                            Handle<Object> value) {
  // Validation should have failed if more than one memory object was
  // provided.
  DCHECK(!instance->has_memory_object());
  if (!value->IsWasmMemoryObject()) {
    ReportLinkError("memory import must be a WebAssembly.Memory object",
                    import_index, module_name, import_name);
    return false;
  }
  auto memory = Handle<WasmMemoryObject>::cast(value);
  instance->set_memory_object(*memory);
  Handle<JSArrayBuffer> buffer(memory->array_buffer(), isolate_);
  // memory_ should have already been assigned in Build().
  DCHECK_EQ(*memory_.ToHandleChecked(), *buffer);
  uint32_t imported_cur_pages =
      static_cast<uint32_t>(buffer->byte_length() / kWasmPageSize);
  if (imported_cur_pages < module_->initial_pages) {
    thrower_->LinkError("memory import %d is smaller than initial %u, got %u",
                        import_index, module_->initial_pages,
                        imported_cur_pages);
    return false;
  }
  int32_t imported_maximum_pages = memory->maximum_pages();
  if (module_->has_maximum_pages) {
    if (imported_maximum_pages < 0) {
      thrower_->LinkError(
          "memory import %d has no maximum limit, expected at most %u",
          import_index, imported_maximum_pages);
      return false;
    }
    if (static_cast<uint32_t>(imported_maximum_pages) >
        module_->maximum_pages) {
      thrower_->LinkError(
          "memory import %d has a larger maximum size %u than the "
          "module's declared maximum %u",
          import_index, imported_maximum_pages, module_->maximum_pages);
      return false;
    }
  }
  if (module_->has_shared_memory != buffer->is_shared()) {
    thrower_->LinkError(
        "mismatch in shared state of memory, declared = %d, imported = %d",
        module_->has_shared_memory, buffer->is_shared());
    return false;
  }

  return true;
}

bool InstanceBuilder::ProcessImportedWasmGlobalObject(
    Handle<WasmInstanceObject> instance, int import_index,
    Handle<String> module_name, Handle<String> import_name,
    const WasmGlobal& global, Handle<WasmGlobalObject> global_object) {
  if (global_object->is_mutable() != global.mutability) {
    ReportLinkError("imported global does not match the expected mutability",
                    import_index, module_name, import_name);
    return false;
  }

  bool is_sub_type = ValueTypes::IsSubType(global.type, global_object->type());
  bool is_same_type = global_object->type() == global.type;
  bool valid_type = global.mutability ? is_same_type : is_sub_type;

  if (!valid_type) {
    ReportLinkError("imported global does not match the expected type",
                    import_index, module_name, import_name);
    return false;
  }
  if (global.mutability) {
    DCHECK_LT(global.index, module_->num_imported_mutable_globals);
    Handle<Object> buffer;
    Address address_or_offset;
    if (ValueTypes::IsReferenceType(global.type)) {
      static_assert(sizeof(global_object->offset()) <= sizeof(Address),
                    "The offset into the globals buffer does not fit into "
                    "the imported_mutable_globals array");
      buffer = handle(global_object->tagged_buffer(), isolate_);
      // For anyref globals we use a relative offset, not an absolute address.
      address_or_offset = static_cast<Address>(global_object->offset());
    } else {
      buffer = handle(global_object->untagged_buffer(), isolate_);
      // It is safe in this case to store the raw pointer to the buffer
      // since the backing store of the JSArrayBuffer will not be
      // relocated.
      address_or_offset = reinterpret_cast<Address>(raw_buffer_ptr(
          Handle<JSArrayBuffer>::cast(buffer), global_object->offset()));
    }
    instance->imported_mutable_globals_buffers().set(global.index, *buffer);
    instance->imported_mutable_globals()[global.index] = address_or_offset;
    return true;
  }

  WriteGlobalValue(global, global_object);
  return true;
}

bool InstanceBuilder::ProcessImportedGlobal(Handle<WasmInstanceObject> instance,
                                            int import_index, int global_index,
                                            Handle<String> module_name,
                                            Handle<String> import_name,
                                            Handle<Object> value) {
  // Immutable global imports are converted to numbers and written into
  // the {untagged_globals_} array buffer.
  //
  // Mutable global imports instead have their backing array buffers
  // referenced by this instance, and store the address of the imported
  // global in the {imported_mutable_globals_} array.
  const WasmGlobal& global = module_->globals[global_index];

  // The mutable-global proposal allows importing i64 values, but only if
  // they are passed as a WebAssembly.Global object.
  //
  // However, the bigint proposal allows importing constant i64 values,
  // as non WebAssembly.Global object.
  if (global.type == kWasmI64 && !enabled_.bigint &&
      !value->IsWasmGlobalObject()) {
    ReportLinkError("global import cannot have type i64", import_index,
                    module_name, import_name);
    return false;
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

  if (value->IsWasmGlobalObject()) {
    auto global_object = Handle<WasmGlobalObject>::cast(value);
    return ProcessImportedWasmGlobalObject(instance, import_index, module_name,
                                           import_name, global, global_object);
  }

  if (global.mutability) {
    ReportLinkError(
        "imported mutable global must be a WebAssembly.Global object",
        import_index, module_name, import_name);
    return false;
  }

  if (ValueTypes::IsReferenceType(global.type)) {
    // There shouldn't be any null-ref globals.
    DCHECK_NE(ValueType::kWasmNullRef, global.type);
    if (global.type == ValueType::kWasmAnyFunc) {
      if (!value->IsNull(isolate_) &&
          !WasmExportedFunction::IsWasmExportedFunction(*value)) {
        ReportLinkError(
            "imported anyfunc global must be null or an exported function",
            import_index, module_name, import_name);
        return false;
      }
    }
    WriteGlobalAnyRef(global, value);
    return true;
  }

  if (value->IsNumber() && global.type != kWasmI64) {
    WriteGlobalValue(global, value->Number());
    return true;
  }

  if (enabled_.bigint && global.type == kWasmI64) {
    Handle<BigInt> bigint;

    if (!BigInt::FromObject(isolate_, value).ToHandle(&bigint)) {
      return false;
    }
    WriteGlobalValue(global, bigint->AsInt64());
    return true;
  }

  ReportLinkError("global import must be a number or WebAssembly.Global object",
                  import_index, module_name, import_name);
  return false;
}

void InstanceBuilder::CompileImportWrappers(
    Handle<WasmInstanceObject> instance) {
  int num_imports = static_cast<int>(module_->import_table.size());
  NativeModule* native_module = instance->module_object().native_module();
  WasmImportWrapperCache::ModificationScope cache_scope(
      native_module->import_wrapper_cache());

  // Compilation is done in two steps:
  // 1) Insert nullptr entries in the cache for wrappers that need to be
  // compiled. 2) Compile wrappers in background tasks using the
  // ImportWrapperQueue. This way the cache won't invalidate other iterators
  // when inserting a new WasmCode, since the key will already be there.
  ImportWrapperQueue import_wrapper_queue;
  for (int index = 0; index < num_imports; ++index) {
    Handle<Object> value = sanitized_imports_[index].value;
    if (module_->import_table[index].kind != kExternalFunction ||
        !value->IsCallable()) {
      continue;
    }
    auto js_receiver = Handle<JSReceiver>::cast(value);
    uint32_t func_index = module_->import_table[index].index;
    FunctionSig* sig = module_->functions[func_index].sig;
    auto kind =
        compiler::GetWasmImportCallKind(js_receiver, sig, enabled_.bigint);
    if (kind == compiler::WasmImportCallKind::kWasmToWasm ||
        kind == compiler::WasmImportCallKind::kLinkError ||
        kind == compiler::WasmImportCallKind::kWasmToCapi) {
      continue;
    }
    WasmImportWrapperCache::CacheKey key(kind, sig);
    if (cache_scope[key] != nullptr) {
      // Cache entry already exists, no need to compile it again.
      continue;
    }
    import_wrapper_queue.insert(key);
  }

  CancelableTaskManager task_manager;
  const int max_background_tasks = GetMaxBackgroundTasks();
  for (int i = 0; i < max_background_tasks; ++i) {
    auto task = base::make_unique<CompileImportWrapperTask>(
        &task_manager, isolate_->wasm_engine(), isolate_->counters(),
        native_module, &import_wrapper_queue, &cache_scope);
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
  }

  // Also compile in the current thread, in case there are no worker threads.
  while (base::Optional<WasmImportWrapperCache::CacheKey> key =
             import_wrapper_queue.pop()) {
    CompileImportWrapper(isolate_->wasm_engine(), native_module,
                         isolate_->counters(), key->first, key->second,
                         &cache_scope);
  }
  task_manager.CancelAndWait();
}

// Process the imports, including functions, tables, globals, and memory, in
// order, loading them from the {ffi_} object. Returns the number of imported
// functions.
int InstanceBuilder::ProcessImports(Handle<WasmInstanceObject> instance) {
  int num_imported_functions = 0;
  int num_imported_tables = 0;

  DCHECK_EQ(module_->import_table.size(), sanitized_imports_.size());

  CompileImportWrappers(instance);
  int num_imports = static_cast<int>(module_->import_table.size());
  for (int index = 0; index < num_imports; ++index) {
    const WasmImport& import = module_->import_table[index];

    Handle<String> module_name = sanitized_imports_[index].module_name;
    Handle<String> import_name = sanitized_imports_[index].import_name;
    Handle<Object> value = sanitized_imports_[index].value;

    switch (import.kind) {
      case kExternalFunction: {
        uint32_t func_index = import.index;
        DCHECK_EQ(num_imported_functions, func_index);
        if (!ProcessImportedFunction(instance, index, func_index, module_name,
                                     import_name, value)) {
          return -1;
        }
        num_imported_functions++;
        break;
      }
      case kExternalTable: {
        uint32_t table_index = import.index;
        DCHECK_EQ(table_index, num_imported_tables);
        if (!ProcessImportedTable(instance, index, table_index, module_name,
                                  import_name, value)) {
          return -1;
        }
        num_imported_tables++;
        break;
      }
      case kExternalMemory: {
        if (!ProcessImportedMemory(instance, index, module_name, import_name,
                                   value)) {
          return -1;
        }
        break;
      }
      case kExternalGlobal: {
        if (!ProcessImportedGlobal(instance, index, import.index, module_name,
                                   import_name, value)) {
          return -1;
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
        Object exception_tag = imported_exception->exception_tag();
        DCHECK(instance->exceptions_table().get(import.index).IsUndefined());
        instance->exceptions_table().set(import.index, exception_tag);
        exception_wrappers_[import.index] = imported_exception;
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
T* InstanceBuilder::GetRawGlobalPtr(const WasmGlobal& global) {
  return reinterpret_cast<T*>(raw_buffer_ptr(untagged_globals_, global.offset));
}

// Process initialization of globals.
void InstanceBuilder::InitGlobals(Handle<WasmInstanceObject> instance) {
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
      case WasmInitExpr::kRefNullConst:
        DCHECK(enabled_.anyref || enabled_.eh);
        if (global.imported) break;  // We already initialized imported globals.

        tagged_globals_->set(global.offset,
                             ReadOnlyRoots(isolate_).null_value(),
                             SKIP_WRITE_BARRIER);
        break;
      case WasmInitExpr::kRefFuncConst: {
        DCHECK(enabled_.anyref);
        auto function = WasmInstanceObject::GetOrCreateWasmExportedFunction(
            isolate_, instance, global.init.val.function_index);
        tagged_globals_->set(global.offset, *function);
        break;
      }
      case WasmInitExpr::kGlobalIndex: {
        // Initialize with another global.
        uint32_t new_offset = global.offset;
        uint32_t old_offset =
            module_->globals[global.init.val.global_index].offset;
        TRACE("init [globals+%u] = [globals+%d]\n", global.offset, old_offset);
        if (ValueTypes::IsReferenceType(global.type)) {
          DCHECK(enabled_.anyref || enabled_.eh);
          tagged_globals_->set(new_offset, tagged_globals_->get(old_offset));
        } else {
          size_t size = (global.type == kWasmI64 || global.type == kWasmF64)
                            ? sizeof(double)
                            : sizeof(int32_t);
          memcpy(raw_buffer_ptr(untagged_globals_, new_offset),
                 raw_buffer_ptr(untagged_globals_, old_offset), size);
        }
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
Handle<JSArrayBuffer> InstanceBuilder::AllocateMemory(uint32_t initial_pages,
                                                      uint32_t maximum_pages) {
  if (initial_pages > max_mem_pages()) {
    thrower_->RangeError("Out of memory: wasm memory too large");
    return Handle<JSArrayBuffer>::null();
  }
  const bool is_shared_memory = module_->has_shared_memory && enabled_.threads;
  Handle<JSArrayBuffer> mem_buffer;
  if (is_shared_memory) {
    if (!NewSharedArrayBuffer(isolate_, initial_pages * kWasmPageSize,
                              maximum_pages * kWasmPageSize)
             .ToHandle(&mem_buffer)) {
      thrower_->RangeError("Out of memory: wasm shared memory");
    }
  } else {
    if (!NewArrayBuffer(isolate_, initial_pages * kWasmPageSize)
             .ToHandle(&mem_buffer)) {
      thrower_->RangeError("Out of memory: wasm memory");
    }
  }
  return mem_buffer;
}

bool InstanceBuilder::NeedsWrappers() const {
  if (module_->num_exported_functions > 0) return true;
  for (auto& table : module_->tables) {
    if (table.type == kWasmAnyFunc) return true;
  }
  return false;
}

// Process the exports, creating wrappers for functions, tables, memories,
// globals, and exceptions.
void InstanceBuilder::ProcessExports(Handle<WasmInstanceObject> instance) {
  if (NeedsWrappers()) {
    // If an imported WebAssembly function gets exported, the exported function
    // has to be identical to to imported function. Therefore we cache all
    // imported WebAssembly functions in the instance.
    for (int index = 0, end = static_cast<int>(module_->import_table.size());
         index < end; ++index) {
      const WasmImport& import = module_->import_table[index];
      if (import.kind == kExternalFunction) {
        Handle<Object> value = sanitized_imports_[index].value;
        if (WasmExportedFunction::IsWasmExportedFunction(*value)) {
          WasmInstanceObject::SetWasmExportedFunction(
              isolate_, instance, import.index,
              Handle<WasmExportedFunction>::cast(value));
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
        // TODO(wasm): reduce duplication with LoadElemSegment() further below
        MaybeHandle<WasmExportedFunction> wasm_exported_function =
            WasmInstanceObject::GetOrCreateWasmExportedFunction(
                isolate_, instance, exp.index);

        desc.set_value(wasm_exported_function.ToHandleChecked());
        break;
      }
      case kExternalTable: {
        desc.set_value(handle(instance->tables().get(exp.index), isolate_));
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
        Handle<JSArrayBuffer> untagged_buffer;
        Handle<FixedArray> tagged_buffer;
        uint32_t offset;

        if (global.mutability && global.imported) {
          Handle<FixedArray> buffers_array(
              instance->imported_mutable_globals_buffers(), isolate_);
          if (ValueTypes::IsReferenceType(global.type)) {
            tagged_buffer = handle(
                FixedArray::cast(buffers_array->get(global.index)), isolate_);
            // For anyref globals we store the relative offset in the
            // imported_mutable_globals array instead of an absolute address.
            Address addr = instance->imported_mutable_globals()[global.index];
            DCHECK_LE(addr, static_cast<Address>(
                                std::numeric_limits<uint32_t>::max()));
            offset = static_cast<uint32_t>(addr);
          } else {
            untagged_buffer =
                handle(JSArrayBuffer::cast(buffers_array->get(global.index)),
                       isolate_);
            Address global_addr =
                instance->imported_mutable_globals()[global.index];

            size_t buffer_size = untagged_buffer->byte_length();
            Address backing_store =
                reinterpret_cast<Address>(untagged_buffer->backing_store());
            CHECK(global_addr >= backing_store &&
                  global_addr < backing_store + buffer_size);
            offset = static_cast<uint32_t>(global_addr - backing_store);
          }
        } else {
          if (ValueTypes::IsReferenceType(global.type)) {
            tagged_buffer = handle(instance->tagged_globals_buffer(), isolate_);
          } else {
            untagged_buffer =
                handle(instance->untagged_globals_buffer(), isolate_);
          }
          offset = global.offset;
        }

        // Since the global's array untagged_buffer is always provided,
        // allocation should never fail.
        Handle<WasmGlobalObject> global_obj =
            WasmGlobalObject::New(isolate_, untagged_buffer, tagged_buffer,
                                  global.type, offset, global.mutability)
                .ToHandleChecked();
        desc.set_value(global_obj);
        break;
      }
      case kExternalException: {
        const WasmException& exception = module_->exceptions[exp.index];
        Handle<WasmExceptionObject> wrapper = exception_wrappers_[exp.index];
        if (wrapper.is_null()) {
          Handle<HeapObject> exception_tag(
              HeapObject::cast(instance->exceptions_table().get(exp.index)),
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
        isolate_, export_to, name, &desc, Just(kThrowOnError));
    if (!status.IsJust()) {
      DisallowHeapAllocation no_gc;
      TruncatedUserString<> trunc_name(name->GetCharVector<uint8_t>(no_gc));
      thrower_->LinkError("export of %.*s failed.", trunc_name.length(),
                          trunc_name.start());
      return;
    }
  }

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

    if (!instance->has_indirect_function_table() &&
        table.type == kWasmAnyFunc) {
      WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
          instance, table.initial_size);
    }
  }
}

bool LoadElemSegmentImpl(Isolate* isolate, Handle<WasmInstanceObject> instance,
                         Handle<WasmTableObject> table_object,
                         const WasmElemSegment& elem_segment, uint32_t dst,
                         uint32_t src, size_t count) {
  // TODO(wasm): Move this functionality into wasm-objects, since it is used
  // for both instantiation and in the implementation of the table.init
  // instruction.
  bool ok =
      ClampToBounds<size_t>(dst, &count, table_object->entries().length());
  // Use & instead of && so the clamp is not short-circuited.
  ok &= ClampToBounds<size_t>(src, &count, elem_segment.entries.size());

  const WasmModule* module = instance->module();
  for (size_t i = 0; i < count; ++i) {
    uint32_t func_index = elem_segment.entries[src + i];
    int entry_index = static_cast<int>(dst + i);

    if (func_index == WasmElemSegment::kNullIndex) {
      if (table_object->type() == kWasmAnyFunc) {
        IndirectFunctionTableEntry(instance, entry_index).clear();
      }
      WasmTableObject::Set(isolate, table_object, entry_index,
                           isolate->factory()->null_value());
      continue;
    }

    const WasmFunction* function = &module->functions[func_index];

    // Update the local dispatch table first if necessary. We only have to
    // update the dispatch table if the first table of the instance is changed.
    // For all other tables, function calls do not use a dispatch table at
    // the moment.
    if (elem_segment.table_index == 0 && table_object->type() == kWasmAnyFunc) {
      uint32_t sig_id = module->signature_ids[function->sig_index];
      IndirectFunctionTableEntry(instance, entry_index)
          .Set(sig_id, instance, func_index);
    }

    // For AnyRef tables, we have to generate the WasmExportedFunction eagerly.
    // Later we cannot know if an entry is a placeholder or not.
    if (table_object->type() == kWasmAnyRef) {
      Handle<WasmExportedFunction> wasm_exported_function =
          WasmInstanceObject::GetOrCreateWasmExportedFunction(isolate, instance,
                                                              func_index);
      WasmTableObject::Set(isolate, table_object, entry_index,
                           wasm_exported_function);
    } else {
      // Update the table object's other dispatch tables.
      MaybeHandle<WasmExportedFunction> wasm_exported_function =
          WasmInstanceObject::GetWasmExportedFunction(isolate, instance,
                                                      func_index);
      if (wasm_exported_function.is_null()) {
        // No JSFunction entry yet exists for this function. Create a {Tuple2}
        // holding the information to lazily allocate one.
        WasmTableObject::SetFunctionTablePlaceholder(
            isolate, table_object, entry_index, instance, func_index);
      } else {
        table_object->entries().set(entry_index,
                                    *wasm_exported_function.ToHandleChecked());
      }
      // UpdateDispatchTables() updates all other dispatch tables, since
      // we have not yet added the dispatch table we are currently building.
      WasmTableObject::UpdateDispatchTables(isolate, table_object, entry_index,
                                            function->sig, instance,
                                            func_index);
    }
  }
  return ok;
}

void InstanceBuilder::LoadTableSegments(Handle<WasmInstanceObject> instance) {
  for (auto& elem_segment : module_->elem_segments) {
    // Passive segments are not copied during instantiation.
    if (!elem_segment.active) continue;

    uint32_t dst = EvalUint32InitExpr(instance, elem_segment.offset);
    uint32_t src = 0;
    size_t count = elem_segment.entries.size();

    bool success = LoadElemSegmentImpl(
        isolate_, instance,
        handle(WasmTableObject::cast(
                   instance->tables().get(elem_segment.table_index)),
               isolate_),
        elem_segment, dst, src, count);
    if (enabled_.bulk_memory) {
      if (!success) {
        thrower_->LinkError("table initializer is out of bounds");
        // Break out instead of returning; we don't want to continue to
        // initialize any further element segments, but still need to add
        // dispatch tables below.
        break;
      }
    } else {
      CHECK(success);
    }
  }

  int table_count = static_cast<int>(module_->tables.size());
  for (int index = 0; index < table_count; ++index) {
    if (module_->tables[index].type == kWasmAnyFunc) {
      auto table_object = handle(
          WasmTableObject::cast(instance->tables().get(index)), isolate_);

      // Add the new dispatch table at the end to avoid redundant lookups.
      WasmTableObject::AddDispatchTable(isolate_, table_object, instance,
                                        index);
    }
  }
}

void InstanceBuilder::InitializeExceptions(
    Handle<WasmInstanceObject> instance) {
  Handle<FixedArray> exceptions_table(instance->exceptions_table(), isolate_);
  for (int index = 0; index < exceptions_table->length(); ++index) {
    if (!exceptions_table->get(index).IsUndefined(isolate_)) continue;
    Handle<WasmExceptionTag> exception_tag =
        WasmExceptionTag::New(isolate_, index);
    exceptions_table->set(index, *exception_tag);
  }
}

bool LoadElemSegment(Isolate* isolate, Handle<WasmInstanceObject> instance,
                     uint32_t table_index, uint32_t segment_index, uint32_t dst,
                     uint32_t src, uint32_t count) {
  // This code path is only used for passive element segments with the
  // table.init instruction. This instruction was introduced in the
  // bulk-memory-operations proposal. At the moment, table.init can only operate
  // on table-0. If table.init should work for tables with higher indices, then
  // we have to adjust the code in {LoadElemSegmentImpl}. The code there uses
  // {IndirectFunctionTableEntry} at the moment, which only works for table-0.
  CHECK_EQ(table_index, 0);
  auto& elem_segment = instance->module()->elem_segments[segment_index];
  return LoadElemSegmentImpl(
      isolate, instance,
      handle(WasmTableObject::cast(instance->tables().get(table_index)),
             isolate),
      elem_segment, dst, src, count);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef TRACE

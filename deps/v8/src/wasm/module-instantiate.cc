// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-instantiate.h"
#include "src/asmjs/asm-js.h"
#include "src/property-descriptor.h"
#include "src/utils.h"
#include "src/wasm/js-to-wasm-wrapper-cache-inl.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-import-wrapper-cache-inl.h"
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
  Handle<JSArrayBuffer> untagged_globals_;
  Handle<FixedArray> tagged_globals_;
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
    // Set externally passed ArrayBuffer non detachable.
    Handle<JSArrayBuffer> memory = memory_.ToHandleChecked();
    memory->set_is_detachable(false);

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
      ErrorThrower thrower(isolate_, "recompile");
      auto native_module = module_object_->native_module();
      CompileNativeModuleWithExplicitBoundsChecks(isolate_, &thrower, module_,
                                                  native_module);
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
  uint32_t untagged_globals_buffer_size = module_->untagged_globals_buffer_size;
  if (untagged_globals_buffer_size > 0) {
    void* backing_store = isolate_->array_buffer_allocator()->Allocate(
        untagged_globals_buffer_size);
    if (backing_store == nullptr) {
      thrower_->RangeError("Out of memory: wasm globals");
      return {};
    }
    untagged_globals_ =
        isolate_->factory()->NewJSArrayBuffer(SharedFlag::kNotShared, TENURED);
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
  for (const WasmElemSegment& elem_segment : module_->elem_segments) {
    if (!elem_segment.active) continue;
    DCHECK(elem_segment.table_index < table_instances_.size());
    uint32_t base = EvalUint32InitExpr(elem_segment.offset);
    size_t table_size = table_instances_[elem_segment.table_index].table_size;
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
    uint32_t base = EvalUint32InitExpr(seg.dest_addr);
    if (!IsInBounds(base, seg.source.length(), instance->memory_size())) {
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
    WasmDebugInfo::RedirectToInterpreter(debug_info, VectorOf(func_indexes));
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
          reinterpret_cast<Address>(raw_buffer_ptr(untagged_globals_, offset)));
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
    // Passive segments are not copied during instantiation.
    if (!segment.active) continue;
    uint32_t dest_offset = EvalUint32InitExpr(segment.dest_addr);
    DCHECK(IsInBounds(dest_offset, source_size, instance->memory_size()));
    byte* dest = instance->memory_start() + dest_offset;
    const byte* src = wire_bytes.start() + segment.source.offset();
    memcpy(dest, src, source_size);
  }
}

void InstanceBuilder::WriteGlobalValue(const WasmGlobal& global, double num) {
  TRACE("init [globals_start=%p + %u] = %lf, type = %s\n",
        reinterpret_cast<void*>(raw_buffer_ptr(untagged_globals_, 0)),
        global.offset, num, ValueTypes::TypeName(global.type));
  switch (global.type) {
    case kWasmI32:
      WriteLittleEndianValue<int32_t>(GetRawGlobalPtr<int32_t>(global),
                                      static_cast<int32_t>(num));
      break;
    case kWasmI64:
      WriteLittleEndianValue<int64_t>(GetRawGlobalPtr<int64_t>(global),
                                      static_cast<int64_t>(num));
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
        reinterpret_cast<void*>(raw_buffer_ptr(untagged_globals_, 0)),
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
      break;
    }
    default: {
      // The imported function is a callable.
      NativeModule* native_module = instance->module_object()->native_module();
      WasmCode* wasm_code = native_module->import_wrapper_cache()->GetOrCompile(
          isolate_->wasm_engine(), isolate_->counters(), kind, expected_sig);
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
  TableInstance& table_instance = table_instances_[table_index];
  table_instance.table_object = Handle<WasmTableObject>::cast(value);
  instance->set_table_object(*table_instance.table_object);
  table_instance.js_wrappers =
      Handle<FixedArray>(table_instance.table_object->functions(), isolate_);

  int imported_table_size = table_instance.js_wrappers->length();
  if (imported_table_size < static_cast<int>(table.initial_size)) {
    thrower_->LinkError("table import %d is smaller than initial %d, got %u",
                        import_index, table.initial_size, imported_table_size);
    return false;
  }

  if (table.has_maximum_size) {
    int64_t imported_maximum_size =
        table_instance.table_object->maximum_length()->Number();
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

  // Allocate a new dispatch table.
  if (!instance->has_indirect_function_table()) {
    WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
        instance, imported_table_size);
    table_instances_[table_index].table_size = imported_table_size;
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
                          import_index, i);
      return false;
    }
    auto target_func = Handle<WasmExportedFunction>::cast(val);
    Handle<WasmInstanceObject> target_instance =
        handle(target_func->instance(), isolate_);
    // Look up the signature's canonical id. If there is no canonical
    // id, then the signature does not appear at all in this module,
    // so putting {-1} in the table will cause checks to always fail.
    FunctionSig* sig = target_func->sig();
    IndirectFunctionTableEntry(instance, i)
        .Set(module_->signature_map.Find(*sig), target_instance,
             target_func->function_index());
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
  if (global_object->type() != global.type) {
    ReportLinkError("imported global does not match the expected type",
                    import_index, module_name, import_name);
    return false;
  }
  if (global_object->is_mutable() != global.mutability) {
    ReportLinkError("imported global does not match the expected mutability",
                    import_index, module_name, import_name);
    return false;
  }
  if (global.mutability) {
    DCHECK_LT(global.index, module_->num_imported_mutable_globals);
    Handle<Object> buffer;
    Address address_or_offset;
    if (global.type == kWasmAnyRef) {
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
    instance->imported_mutable_globals_buffers()->set(global.index, *buffer);
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

  if (global.type == ValueType::kWasmAnyRef) {
    WriteGlobalAnyRef(global, value);
    return true;
  }

  if (value->IsNumber()) {
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

// Process the imports, including functions, tables, globals, and memory, in
// order, loading them from the {ffi_} object. Returns the number of imported
// functions.
int InstanceBuilder::ProcessImports(Handle<WasmInstanceObject> instance) {
  int num_imported_functions = 0;
  int num_imported_tables = 0;

  DCHECK_EQ(module_->import_table.size(), sanitized_imports_.size());
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
  return num_imported_functions;
}

template <typename T>
T* InstanceBuilder::GetRawGlobalPtr(const WasmGlobal& global) {
  return reinterpret_cast<T*>(raw_buffer_ptr(untagged_globals_, global.offset));
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
      case WasmInitExpr::kAnyRefConst:
        DCHECK(enabled_.anyref);
        if (global.imported) break;  // We already initialized imported globals.

        tagged_globals_->set(global.offset,
                             ReadOnlyRoots(isolate_).null_value(),
                             SKIP_WRITE_BARRIER);
        break;
      case WasmInitExpr::kGlobalIndex: {
        if (global.type == ValueType::kWasmAnyRef) {
          DCHECK(enabled_.anyref);
          int other_offset =
              module_->globals[global.init.val.global_index].offset;

          tagged_globals_->set(global.offset,
                               tagged_globals_->get(other_offset),
                               SKIP_WRITE_BARRIER);
        }
        // Initialize with another global.
        uint32_t new_offset = global.offset;
        uint32_t old_offset =
            module_->globals[global.init.val.global_index].offset;
        TRACE("init [globals+%u] = [globals+%d]\n", global.offset, old_offset);
        size_t size = (global.type == kWasmI64 || global.type == kWasmF64)
                          ? sizeof(double)
                          : sizeof(int32_t);
        memcpy(raw_buffer_ptr(untagged_globals_, new_offset),
               raw_buffer_ptr(untagged_globals_, old_offset), size);
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
  if (num_pages > max_mem_pages()) {
    thrower_->RangeError("Out of memory: wasm memory too large");
    return Handle<JSArrayBuffer>::null();
  }
  const bool is_shared_memory = module_->has_shared_memory && enabled_.threads;
  SharedFlag shared_flag =
      is_shared_memory ? SharedFlag::kShared : SharedFlag::kNotShared;
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
                ModuleWireBytes(module_object_->native_module()->wire_bytes()),
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
        instance->set_table_object(*table_instance.table_object);
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
        Handle<JSArrayBuffer> untagged_buffer;
        Handle<FixedArray> tagged_buffer;
        uint32_t offset;

        if (global.mutability && global.imported) {
          Handle<FixedArray> buffers_array(
              instance->imported_mutable_globals_buffers(), isolate_);
          if (global.type == kWasmAnyRef) {
            tagged_buffer = buffers_array->GetValueChecked<FixedArray>(
                isolate_, global.index);
            // For anyref globals we store the relative offset in the
            // imported_mutable_globals array instead of an absolute address.
            Address addr = instance->imported_mutable_globals()[global.index];
            DCHECK_LE(addr, static_cast<Address>(
                                std::numeric_limits<uint32_t>::max()));
            offset = static_cast<uint32_t>(addr);
          } else {
            untagged_buffer = buffers_array->GetValueChecked<JSArrayBuffer>(
                isolate_, global.index);
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
          if (global.type == kWasmAnyRef) {
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
      DisallowHeapAllocation no_gc;
      TruncatedUserString<> trunc_name(name->GetCharVector<uint8_t>(no_gc));
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
  for (auto& elem_segment : module_->elem_segments) {
    // Passive segments are not copied during instantiation.
    if (!elem_segment.active) continue;

    uint32_t base = EvalUint32InitExpr(elem_segment.offset);
    uint32_t num_entries = static_cast<uint32_t>(elem_segment.entries.size());
    uint32_t index = elem_segment.table_index;
    TableInstance& table_instance = table_instances_[index];
    DCHECK(IsInBounds(base, num_entries, table_instance.table_size));
    for (uint32_t i = 0; i < num_entries; ++i) {
      uint32_t func_index = elem_segment.entries[i];
      const WasmFunction* function = &module_->functions[func_index];
      int table_index = static_cast<int>(i + base);

      // Update the local dispatch table first.
      uint32_t sig_id = module_->signature_ids[function->sig_index];
      IndirectFunctionTableEntry(instance, table_index)
          .Set(sig_id, instance, func_index);

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
                ModuleWireBytes(native_module->wire_bytes()), func_index);
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
            instance, func_index);
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
    Handle<WasmExceptionTag> exception_tag =
        WasmExceptionTag::New(isolate_, index);
    exceptions_table->set(index, *exception_tag);
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef TRACE

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-instantiate.h"

#include "src/api/api-inl.h"
#include "src/asmjs/asm-js.h"
#include "src/base/atomicops.h"
#include "src/logging/counters-scopes.h"
#include "src/logging/metrics.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/descriptor-array-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/tracing/trace-event.h"
#include "src/utils/utils.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/constant-expression-interface.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-external-refs.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "src/wasm/wasm-subtyping.h"
#include "src/wasm/wasm-value.h"

#define TRACE(...)                                          \
  do {                                                      \
    if (v8_flags.trace_wasm_instances) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {
namespace wasm {

namespace {

byte* raw_buffer_ptr(MaybeHandle<JSArrayBuffer> buffer, int offset) {
  return static_cast<byte*>(buffer.ToHandleChecked()->backing_store()) + offset;
}

using ImportWrapperQueue =
    WrapperQueue<WasmImportWrapperCache::CacheKey, const FunctionSig*,
                 WasmImportWrapperCache::CacheKeyHash>;

class CompileImportWrapperJob final : public JobTask {
 public:
  CompileImportWrapperJob(
      Counters* counters, NativeModule* native_module,
      ImportWrapperQueue* queue,
      WasmImportWrapperCache::ModificationScope* cache_scope)
      : counters_(counters),
        native_module_(native_module),
        queue_(queue),
        cache_scope_(cache_scope) {}

  size_t GetMaxConcurrency(size_t worker_count) const override {
    size_t flag_limit = static_cast<size_t>(
        std::max(1, v8_flags.wasm_num_compilation_tasks.value()));
    // Add {worker_count} to the queue size because workers might still be
    // processing units that have already been popped from the queue.
    return std::min(flag_limit, worker_count + queue_->size());
  }

  void Run(JobDelegate* delegate) override {
    TRACE_EVENT0("v8.wasm", "wasm.CompileImportWrapperJob.Run");
    while (base::Optional<std::pair<const WasmImportWrapperCache::CacheKey,
                                    const FunctionSig*>>
               key = queue_->pop()) {
      // TODO(wasm): Batch code publishing, to avoid repeated locking and
      // permission switching.
      CompileImportWrapper(native_module_, counters_, key->first.kind,
                           key->second, key->first.canonical_type_index,
                           key->first.expected_arity, key->first.suspend,
                           cache_scope_);
      if (delegate->ShouldYield()) return;
    }
  }

 private:
  Counters* const counters_;
  NativeModule* const native_module_;
  ImportWrapperQueue* const queue_;
  WasmImportWrapperCache::ModificationScope* const cache_scope_;
};

Handle<DescriptorArray> CreateArrayDescriptorArray(
    Isolate* isolate, const wasm::ArrayType* type) {
  uint32_t kDescriptorsCount = 1;
  Handle<DescriptorArray> descriptors =
      isolate->factory()->NewDescriptorArray(kDescriptorsCount);

  // TODO(ishell): cache Wasm field type in FieldType value.
  MaybeObject any_type = MaybeObject::FromObject(FieldType::Any());
  DCHECK(any_type->IsSmi());

  // Add descriptor for length property.
  PropertyDetails details(PropertyKind::kData, FROZEN, PropertyLocation::kField,
                          PropertyConstness::kConst,
                          Representation::WasmValue(), static_cast<int>(0));
  descriptors->Set(InternalIndex(0), *isolate->factory()->length_string(),
                   any_type, details);

  descriptors->Sort();
  return descriptors;
}

Handle<Map> CreateStructMap(Isolate* isolate, const WasmModule* module,
                            int struct_index, Handle<Map> opt_rtt_parent,
                            Handle<WasmInstanceObject> instance) {
  const wasm::StructType* type = module->struct_type(struct_index);
  const int inobject_properties = 0;
  // We have to use the variable size sentinel because the instance size
  // stored directly in a Map is capped at 255 pointer sizes.
  const int map_instance_size = kVariableSizeSentinel;
  const int real_instance_size = WasmStruct::Size(type);
  const InstanceType instance_type = WASM_STRUCT_TYPE;
  // TODO(jkummerow): If NO_ELEMENTS were supported, we could use that here.
  const ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND;
  Handle<WasmTypeInfo> type_info = isolate->factory()->NewWasmTypeInfo(
      reinterpret_cast<Address>(type), opt_rtt_parent, real_instance_size,
      instance, struct_index);
  Handle<Map> map = isolate->factory()->NewMap(
      instance_type, map_instance_size, elements_kind, inobject_properties);
  map->set_wasm_type_info(*type_info);
  map->SetInstanceDescriptors(isolate,
                              *isolate->factory()->empty_descriptor_array(), 0);
  map->set_is_extensible(false);
  WasmStruct::EncodeInstanceSizeInMap(real_instance_size, *map);
  return map;
}

Handle<Map> CreateArrayMap(Isolate* isolate, const WasmModule* module,
                           int array_index, Handle<Map> opt_rtt_parent,
                           Handle<WasmInstanceObject> instance) {
  const wasm::ArrayType* type = module->array_type(array_index);
  const int inobject_properties = 0;
  const int instance_size = kVariableSizeSentinel;
  // Wasm Arrays don't have a static instance size.
  const int cached_instance_size = 0;
  const InstanceType instance_type = WASM_ARRAY_TYPE;
  const ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND;
  Handle<WasmTypeInfo> type_info = isolate->factory()->NewWasmTypeInfo(
      reinterpret_cast<Address>(type), opt_rtt_parent, cached_instance_size,
      instance, array_index);
  // TODO(ishell): get canonical descriptor array for WasmArrays from roots.
  Handle<DescriptorArray> descriptors =
      CreateArrayDescriptorArray(isolate, type);
  Handle<Map> map = isolate->factory()->NewMap(
      instance_type, instance_size, elements_kind, inobject_properties);
  map->set_wasm_type_info(*type_info);
  map->SetInstanceDescriptors(isolate, *descriptors,
                              descriptors->number_of_descriptors());
  map->set_is_extensible(false);
  WasmArray::EncodeElementSizeInMap(type->element_type().value_kind_size(),
                                    *map);
  return map;
}

Handle<Map> CreateFuncRefMap(Isolate* isolate, const WasmModule* module,
                             Handle<Map> opt_rtt_parent,
                             Handle<WasmInstanceObject> instance) {
  const int inobject_properties = 0;
  const int instance_size =
      Map::cast(isolate->root(RootIndex::kWasmInternalFunctionMap))
          .instance_size();
  const InstanceType instance_type = WASM_INTERNAL_FUNCTION_TYPE;
  const ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND;
  constexpr uint32_t kNoIndex = ~0u;
  Handle<WasmTypeInfo> type_info = isolate->factory()->NewWasmTypeInfo(
      kNullAddress, opt_rtt_parent, instance_size, instance, kNoIndex);
  Handle<Map> map = isolate->factory()->NewMap(
      instance_type, instance_size, elements_kind, inobject_properties);
  map->set_wasm_type_info(*type_info);
  return map;
}

void CreateMapForType(Isolate* isolate, const WasmModule* module,
                      int type_index, Handle<WasmInstanceObject> instance,
                      Handle<FixedArray> maps) {
  // Recursive calls for supertypes may already have created this map.
  if (maps->get(type_index).IsMap()) return;

  Handle<WeakArrayList> canonical_rtts;
  uint32_t canonical_type_index =
      module->isorecursive_canonical_type_ids[type_index];

  // Try to find the canonical map for this type in the isolate store.
  canonical_rtts = handle(isolate->heap()->wasm_canonical_rtts(), isolate);
  DCHECK_GT(static_cast<uint32_t>(canonical_rtts->length()),
            canonical_type_index);
  MaybeObject maybe_canonical_map = canonical_rtts->Get(canonical_type_index);
  if (maybe_canonical_map.IsStrongOrWeak() &&
      maybe_canonical_map.GetHeapObject().IsMap()) {
    maps->set(type_index, maybe_canonical_map.GetHeapObject());
    return;
  }

  Handle<Map> rtt_parent;
  // If the type with {type_index} has an explicit supertype, make sure the
  // map for that supertype is created first, so that the supertypes list
  // that's cached on every RTT can be set up correctly.
  uint32_t supertype = module->supertype(type_index);
  if (supertype != kNoSuperType) {
    // This recursion is safe, because kV8MaxRttSubtypingDepth limits the
    // number of recursive steps, so we won't overflow the stack.
    CreateMapForType(isolate, module, supertype, instance, maps);
    rtt_parent = handle(Map::cast(maps->get(supertype)), isolate);
  }
  Handle<Map> map;
  switch (module->types[type_index].kind) {
    case TypeDefinition::kStruct:
      map = CreateStructMap(isolate, module, type_index, rtt_parent, instance);
      break;
    case TypeDefinition::kArray:
      map = CreateArrayMap(isolate, module, type_index, rtt_parent, instance);
      break;
    case TypeDefinition::kFunction:
      map = CreateFuncRefMap(isolate, module, rtt_parent, instance);
      break;
  }
  canonical_rtts->Set(canonical_type_index, HeapObjectReference::Weak(*map));
  maps->set(type_index, *map);
}

}  // namespace

// A helper class to simplify instantiating a module from a module object.
// It closes over the {Isolate}, the {ErrorThrower}, etc.
class InstanceBuilder {
 public:
  InstanceBuilder(Isolate* isolate, v8::metrics::Recorder::ContextId context_id,
                  ErrorThrower* thrower, Handle<WasmModuleObject> module_object,
                  MaybeHandle<JSReceiver> ffi,
                  MaybeHandle<JSArrayBuffer> memory_buffer);

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
  v8::metrics::Recorder::ContextId context_id_;
  const WasmFeatures enabled_;
  const WasmModule* const module_;
  ErrorThrower* thrower_;
  Handle<WasmModuleObject> module_object_;
  MaybeHandle<JSReceiver> ffi_;
  MaybeHandle<JSArrayBuffer> memory_buffer_;
  Handle<WasmMemoryObject> memory_object_;
  Handle<JSArrayBuffer> untagged_globals_;
  Handle<FixedArray> tagged_globals_;
  std::vector<Handle<WasmTagObject>> tags_wrappers_;
  Handle<WasmExportedFunction> start_function_;
  std::vector<SanitizedImport> sanitized_imports_;
  // We pass this {Zone} to the temporary {WasmFullDecoder} we allocate during
  // each call to {EvaluateConstantExpression}. This has been found to improve
  // performance a bit over allocating a new {Zone} each time.
  Zone init_expr_zone_;

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

  void WriteGlobalValue(const WasmGlobal& global, const WasmValue& value);

  void SanitizeImports();

  // Find the imported memory if there is one.
  bool FindImportedMemory();

  // Allocate the memory.
  bool AllocateMemory();

  // Processes a single imported function.
  bool ProcessImportedFunction(Handle<WasmInstanceObject> instance,
                               int import_index, int func_index,
                               Handle<String> module_name,
                               Handle<String> import_name,
                               Handle<Object> value);

  // Initialize imported tables of type funcref.
  bool InitializeImportedIndirectFunctionTable(
      Handle<WasmInstanceObject> instance, int table_index, int import_index,
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
  // functions, or {-1} on error.
  int ProcessImports(Handle<WasmInstanceObject> instance);

  template <typename T>
  T* GetRawUntaggedGlobalPtr(const WasmGlobal& global);

  // Process initialization of globals.
  void InitGlobals(Handle<WasmInstanceObject> instance);

  // Process the exports, creating wrappers for functions, tables, memories,
  // and globals.
  void ProcessExports(Handle<WasmInstanceObject> instance);

  void SetTableInitialValues(Handle<WasmInstanceObject> instance);

  void LoadTableSegments(Handle<WasmInstanceObject> instance);

  // Creates new tags. Note that some tags might already exist if they were
  // imported, those tags will be re-used.
  void InitializeTags(Handle<WasmInstanceObject> instance);
};

namespace {
class ReportLazyCompilationTimesTask : public v8::Task {
 public:
  ReportLazyCompilationTimesTask(std::weak_ptr<Counters> counters,
                                 std::weak_ptr<NativeModule> native_module,
                                 int delay_in_seconds)
      : counters_(std::move(counters)),
        native_module_(std::move(native_module)),
        delay_in_seconds_(delay_in_seconds) {}

  void Run() final {
    std::shared_ptr<NativeModule> native_module = native_module_.lock();
    if (!native_module) return;
    std::shared_ptr<Counters> counters = counters_.lock();
    if (!counters) return;
    int num_compilations = native_module->num_lazy_compilations();
    // If no compilations happened, we don't add samples. Experiments showed
    // many cases of num_compilations == 0, and adding these cases would make
    // other cases less visible.
    if (!num_compilations) return;
    if (delay_in_seconds_ == 5) {
      counters->wasm_num_lazy_compilations_5sec()->AddSample(num_compilations);
      counters->wasm_sum_lazy_compilation_time_5sec()->AddSample(
          static_cast<int>(native_module->sum_lazy_compilation_time_in_ms()));
      counters->wasm_max_lazy_compilation_time_5sec()->AddSample(
          static_cast<int>(native_module->max_lazy_compilation_time_in_ms()));
      return;
    }
    if (delay_in_seconds_ == 20) {
      counters->wasm_num_lazy_compilations_20sec()->AddSample(num_compilations);
      counters->wasm_sum_lazy_compilation_time_20sec()->AddSample(
          static_cast<int>(native_module->sum_lazy_compilation_time_in_ms()));
      counters->wasm_max_lazy_compilation_time_20sec()->AddSample(
          static_cast<int>(native_module->max_lazy_compilation_time_in_ms()));
      return;
    }
    if (delay_in_seconds_ == 60) {
      counters->wasm_num_lazy_compilations_60sec()->AddSample(num_compilations);
      counters->wasm_sum_lazy_compilation_time_60sec()->AddSample(
          static_cast<int>(native_module->sum_lazy_compilation_time_in_ms()));
      counters->wasm_max_lazy_compilation_time_60sec()->AddSample(
          static_cast<int>(native_module->max_lazy_compilation_time_in_ms()));
      return;
    }
    if (delay_in_seconds_ == 120) {
      counters->wasm_num_lazy_compilations_120sec()->AddSample(
          num_compilations);
      counters->wasm_sum_lazy_compilation_time_120sec()->AddSample(
          static_cast<int>(native_module->sum_lazy_compilation_time_in_ms()));
      counters->wasm_max_lazy_compilation_time_120sec()->AddSample(
          static_cast<int>(native_module->max_lazy_compilation_time_in_ms()));
      return;
    }
    UNREACHABLE();
  }

 private:
  std::weak_ptr<Counters> counters_;
  std::weak_ptr<NativeModule> native_module_;
  int delay_in_seconds_;
};
}  // namespace

MaybeHandle<WasmInstanceObject> InstantiateToInstanceObject(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory_buffer) {
  v8::metrics::Recorder::ContextId context_id =
      isolate->GetOrRegisterRecorderContextId(isolate->native_context());
  InstanceBuilder builder(isolate, context_id, thrower, module_object, imports,
                          memory_buffer);
  auto instance = builder.Build();
  if (!instance.is_null()) {
    // Post tasks for lazy compilation metrics before we call the start
    // function.
    if (v8_flags.wasm_lazy_compilation &&
        module_object->native_module()
            ->ShouldLazyCompilationMetricsBeReported()) {
      V8::GetCurrentPlatform()->CallDelayedOnWorkerThread(
          std::make_unique<ReportLazyCompilationTimesTask>(
              isolate->async_counters(), module_object->shared_native_module(),
              5),
          5.0);
      V8::GetCurrentPlatform()->CallDelayedOnWorkerThread(
          std::make_unique<ReportLazyCompilationTimesTask>(
              isolate->async_counters(), module_object->shared_native_module(),
              20),
          20.0);
      V8::GetCurrentPlatform()->CallDelayedOnWorkerThread(
          std::make_unique<ReportLazyCompilationTimesTask>(
              isolate->async_counters(), module_object->shared_native_module(),
              60),
          60.0);
      V8::GetCurrentPlatform()->CallDelayedOnWorkerThread(
          std::make_unique<ReportLazyCompilationTimesTask>(
              isolate->async_counters(), module_object->shared_native_module(),
              120),
          120.0);
    }
    if (builder.ExecuteStartFunction()) {
      return instance;
    }
  }
  DCHECK(isolate->has_pending_exception() || thrower->error());
  return {};
}

InstanceBuilder::InstanceBuilder(Isolate* isolate,
                                 v8::metrics::Recorder::ContextId context_id,
                                 ErrorThrower* thrower,
                                 Handle<WasmModuleObject> module_object,
                                 MaybeHandle<JSReceiver> ffi,
                                 MaybeHandle<JSArrayBuffer> memory_buffer)
    : isolate_(isolate),
      context_id_(context_id),
      enabled_(module_object->native_module()->enabled_features()),
      module_(module_object->module()),
      thrower_(thrower),
      module_object_(module_object),
      ffi_(ffi),
      memory_buffer_(memory_buffer),
      init_expr_zone_(isolate_->allocator(), "constant expression zone") {
  sanitized_imports_.reserve(module_->import_table.size());
}

// Build an instance, in all of its glory.
MaybeHandle<WasmInstanceObject> InstanceBuilder::Build() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.InstanceBuilder.Build");
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
  // Start a timer for instantiation time, if we have a high resolution timer.
  base::ElapsedTimer timer;
  if (base::TimeTicks::IsHighResolution()) {
    timer.Start();
  }
  v8::metrics::WasmModuleInstantiated wasm_module_instantiated;
  NativeModule* native_module = module_object_->native_module();

  //--------------------------------------------------------------------------
  // Set up the memory buffer and memory objects.
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

  if (is_asmjs_module(module_)) {
    Handle<JSArrayBuffer> buffer;
    if (memory_buffer_.ToHandle(&buffer)) {
      // asm.js instantiation should have changed the state of the buffer.
      CHECK(!buffer->is_detachable());
      CHECK(buffer->is_asmjs_memory());
    } else {
      // Use an empty JSArrayBuffer for degenerate asm.js modules.
      memory_buffer_ = isolate_->factory()->NewJSArrayBufferAndBackingStore(
          0, InitializedFlag::kUninitialized);
      if (!memory_buffer_.ToHandle(&buffer)) {
        thrower_->RangeError("Out of memory: asm.js memory");
        return {};
      }
      buffer->set_is_asmjs_memory(true);
      buffer->set_is_detachable(false);
    }

    // The maximum number of pages isn't strictly necessary for memory
    // objects used for asm.js, as they are never visible, but we might
    // as well make it accurate.
    auto maximum_pages =
        static_cast<int>(RoundUp(buffer->byte_length(), wasm::kWasmPageSize) /
                         wasm::kWasmPageSize);
    memory_object_ = WasmMemoryObject::New(isolate_, buffer, maximum_pages)
                         .ToHandleChecked();
  } else {
    // Actual wasm module must have either imported or created memory.
    CHECK(memory_buffer_.is_null());
    if (!FindImportedMemory()) {
      if (module_->has_memory && !AllocateMemory()) {
        DCHECK(isolate_->has_pending_exception() || thrower_->error());
        return {};
      }
    }
  }

  //--------------------------------------------------------------------------
  // Create the WebAssembly.Instance object.
  //--------------------------------------------------------------------------
  TRACE("New module instantiation for %p\n", native_module);
  Handle<WasmInstanceObject> instance =
      WasmInstanceObject::New(isolate_, module_object_);

  //--------------------------------------------------------------------------
  // Attach the memory to the instance.
  //--------------------------------------------------------------------------
  if (module_->has_memory) {
    DCHECK(!memory_object_.is_null());
    if (!instance->has_memory_object()) {
      instance->set_memory_object(*memory_object_);
    }
    // Add the instance object to the list of instances for this memory.
    WasmMemoryObject::AddInstance(isolate_, memory_object_, instance);

    // Double-check the {memory} array buffer matches the instance.
    Handle<JSArrayBuffer> memory = memory_buffer_.ToHandleChecked();
    CHECK_EQ(instance->memory_size(), memory->byte_length());
    CHECK_EQ(instance->memory_start(), memory->backing_store());
  }

  //--------------------------------------------------------------------------
  // Set up the globals for the new instance.
  //--------------------------------------------------------------------------
  uint32_t untagged_globals_buffer_size = module_->untagged_globals_buffer_size;
  if (untagged_globals_buffer_size > 0) {
    MaybeHandle<JSArrayBuffer> result =
        isolate_->factory()->NewJSArrayBufferAndBackingStore(
            untagged_globals_buffer_size, InitializedFlag::kZeroInitialized,
            AllocationType::kOld);

    if (!result.ToHandle(&untagged_globals_)) {
      thrower_->RangeError("Out of memory: wasm globals");
      return {};
    }

    instance->set_untagged_globals_buffer(*untagged_globals_);
    instance->set_globals_start(
        reinterpret_cast<byte*>(untagged_globals_->backing_store()));
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
  // Set up the tag table used for exception tag checks.
  //--------------------------------------------------------------------------
  int tags_count = static_cast<int>(module_->tags.size());
  if (tags_count > 0) {
    Handle<FixedArray> tag_table =
        isolate_->factory()->NewFixedArray(tags_count, AllocationType::kOld);
    instance->set_tags_table(*tag_table);
    tags_wrappers_.resize(tags_count);
  }

  //--------------------------------------------------------------------------
  // Set up table storage space.
  //--------------------------------------------------------------------------
  instance->set_isorecursive_canonical_types(
      module_->isorecursive_canonical_type_ids.data());
  int table_count = static_cast<int>(module_->tables.size());
  {
    for (int i = 0; i < table_count; i++) {
      const WasmTable& table = module_->tables[i];
      if (table.initial_size > v8_flags.wasm_max_table_size) {
        thrower_->RangeError(
            "initial table size (%u elements) is larger than implementation "
            "limit (%u elements)",
            table.initial_size, v8_flags.wasm_max_table_size.value());
        return {};
      }
    }

    Handle<FixedArray> tables = isolate_->factory()->NewFixedArray(table_count);
    for (int i = module_->num_imported_tables; i < table_count; i++) {
      const WasmTable& table = module_->tables[i];
      // Initialize tables with null for now. We will initialize non-defaultable
      // tables later, in {SetTableInitialValues}.
      Handle<WasmTableObject> table_obj = WasmTableObject::New(
          isolate_, instance, table.type, table.initial_size,
          table.has_maximum_size, table.maximum_size, nullptr,
          isolate_->factory()->null_value());
      tables->set(i, *table_obj);
    }
    instance->set_tables(*tables);
  }

  {
    Handle<FixedArray> tables = isolate_->factory()->NewFixedArray(table_count);
    for (int i = 0; i < table_count; ++i) {
      const WasmTable& table = module_->tables[i];
      if (IsSubtypeOf(table.type, kWasmFuncRef, module_)) {
        Handle<WasmIndirectFunctionTable> table_obj =
            WasmIndirectFunctionTable::New(isolate_, table.initial_size);
        tables->set(i, *table_obj);
      }
    }
    instance->set_indirect_function_tables(*tables);
  }

  instance->SetIndirectFunctionTableShortcuts(isolate_);

  //--------------------------------------------------------------------------
  // Process the imports for the module.
  //--------------------------------------------------------------------------
  if (!module_->import_table.empty()) {
    int num_imported_functions = ProcessImports(instance);
    if (num_imported_functions < 0) return {};
    wasm_module_instantiated.imported_function_count = num_imported_functions;
  }

  //--------------------------------------------------------------------------
  // Create maps for managed objects (GC proposal).
  // Must happen before {InitGlobals} because globals can refer to these maps.
  // We do not need to cache the canonical rtts to (rtt.canon any)'s subtype
  // list.
  //--------------------------------------------------------------------------
  if (enabled_.has_gc()) {
    if (module_->isorecursive_canonical_type_ids.size() > 0) {
      // Make sure all canonical indices have been set.
      DCHECK_NE(module_->MaxCanonicalTypeIndex(), kNoSuperType);
      isolate_->heap()->EnsureWasmCanonicalRttsSize(
          module_->MaxCanonicalTypeIndex() + 1);
    }
    Handle<FixedArray> maps = isolate_->factory()->NewFixedArray(
        static_cast<int>(module_->types.size()));
    for (uint32_t index = 0; index < module_->types.size(); index++) {
      CreateMapForType(isolate_, module_, index, instance, maps);
    }
    instance->set_managed_object_maps(*maps);
  }

  //--------------------------------------------------------------------------
  // Allocate type feedback vectors for functions.
  //--------------------------------------------------------------------------
  if (v8_flags.wasm_speculative_inlining) {
    int num_functions = static_cast<int>(module_->num_declared_functions);
    Handle<FixedArray> vectors =
        isolate_->factory()->NewFixedArray(num_functions, AllocationType::kOld);
    instance->set_feedback_vectors(*vectors);
    for (int i = 0; i < num_functions; i++) {
      int func_index = module_->num_imported_functions + i;
      int slots = NumFeedbackSlots(module_, func_index);
      if (slots == 0) continue;
      if (v8_flags.trace_wasm_speculative_inlining) {
        PrintF("[Function %d (declared %d): allocating %d feedback slots]\n",
               func_index, i, slots);
      }
      Handle<FixedArray> feedback =
          isolate_->factory()->NewFixedArrayWithZeroes(slots);
      vectors->set(i, *feedback);
    }
  }

  //--------------------------------------------------------------------------
  // Process the initialization for the module's globals.
  //--------------------------------------------------------------------------
  InitGlobals(instance);

  //--------------------------------------------------------------------------
  // Initialize the indirect function tables and dispatch tables. We do this
  // before initializing non-defaultable tables and loading element segments, so
  // that indirect function tables in this module are included in the updates
  // when we do so.
  //--------------------------------------------------------------------------
  for (int table_index = 0;
       table_index < static_cast<int>(module_->tables.size()); ++table_index) {
    const WasmTable& table = module_->tables[table_index];

    if (IsSubtypeOf(table.type, kWasmFuncRef, module_)) {
      WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
          instance, table_index, table.initial_size);
      if (thrower_->error()) return {};
      auto table_object = handle(
          WasmTableObject::cast(instance->tables().get(table_index)), isolate_);
      WasmTableObject::AddDispatchTable(isolate_, table_object, instance,
                                        table_index);
    }
  }

  //--------------------------------------------------------------------------
  // Initialize non-defaultable tables.
  //--------------------------------------------------------------------------
  if (v8_flags.experimental_wasm_typed_funcref) {
    SetTableInitialValues(instance);
  }

  //--------------------------------------------------------------------------
  // Initialize the tags table.
  //--------------------------------------------------------------------------
  if (tags_count > 0) {
    InitializeTags(instance);
  }

  //--------------------------------------------------------------------------
  // Set up the exports object for the new instance.
  //--------------------------------------------------------------------------
  ProcessExports(instance);
  if (thrower_->error()) return {};

  //--------------------------------------------------------------------------
  // Load element segments into tables.
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
  // Create a wrapper for the start function.
  //--------------------------------------------------------------------------
  if (module_->start_function_index >= 0) {
    int start_index = module_->start_function_index;
    auto& function = module_->functions[start_index];
    uint32_t canonical_sig_index =
        module_->isorecursive_canonical_type_ids[module_->functions[start_index]
                                                     .sig_index];
    Handle<CodeT> wrapper_code =
        JSToWasmWrapperCompilationUnit::CompileJSToWasmWrapper(
            isolate_, function.sig, canonical_sig_index, module_,
            function.imported);
    // TODO(clemensb): Don't generate an exported function for the start
    // function. Use CWasmEntry instead.
    start_function_ = WasmExportedFunction::New(
        isolate_, instance, start_index,
        static_cast<int>(function.sig->parameter_count()), wrapper_code);

    if (function.imported) {
      ImportedFunctionEntry entry(instance, module_->start_function_index);
      Object callable = entry.maybe_callable();
      if (callable.IsJSFunction()) {
        // If the start function was imported and calls into Blink, we have
        // to pretend that the V8 API was used to enter its correct context.
        // To get that context to {ExecuteStartFunction} below, we install it
        // as the context of the wrapper we just compiled. That's a bit of a
        // hack because it's not really the wrapper's context, only its wrapped
        // target's context, but the end result is the same, and since the
        // start function wrapper doesn't leak, neither does this
        // implementation detail.
        start_function_->set_context(JSFunction::cast(callable).context());
      }
    }
  }

  DCHECK(!isolate_->has_pending_exception());
  TRACE("Successfully built instance for module %p\n",
        module_object_->native_module());
  wasm_module_instantiated.success = true;
  if (timer.IsStarted()) {
    base::TimeDelta instantiation_time = timer.Elapsed();
    wasm_module_instantiated.wall_clock_duration_in_us =
        instantiation_time.InMicroseconds();
    SELECT_WASM_COUNTER(isolate_->counters(), module_->origin, wasm_instantiate,
                        module_time)
        ->AddTimedSample(instantiation_time);
    isolate_->metrics_recorder()->DelayMainThreadEvent(wasm_module_instantiated,
                                                       context_id_);
  }
  return instance;
}

bool InstanceBuilder::ExecuteStartFunction() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.ExecuteStartFunction");
  if (start_function_.is_null()) return true;  // No start function.

  HandleScope scope(isolate_);
  // In case the start function calls out to Blink, we have to make sure that
  // the correct "entered context" is available. This is the equivalent of
  // v8::Context::Enter() and must happen in addition to the function call
  // sequence doing the compiled version of "isolate->set_context(...)".
  HandleScopeImplementer* hsi = isolate_->handle_scope_implementer();
  hsi->EnterContext(start_function_->native_context());

  // Call the JS function.
  Handle<Object> undefined = isolate_->factory()->undefined_value();
  MaybeHandle<Object> retval =
      Execution::Call(isolate_, start_function_, undefined, 0, nullptr);
  hsi->LeaveContext();

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

namespace {
bool HasDefaultToNumberBehaviour(Isolate* isolate,
                                 Handle<JSFunction> function) {
  // Disallow providing a [Symbol.toPrimitive] member.
  LookupIterator to_primitive_it{isolate, function,
                                 isolate->factory()->to_primitive_symbol()};
  if (to_primitive_it.state() != LookupIterator::NOT_FOUND) return false;

  // The {valueOf} member must be the default "ObjectPrototypeValueOf".
  LookupIterator value_of_it{isolate, function,
                             isolate->factory()->valueOf_string()};
  if (value_of_it.state() != LookupIterator::DATA) return false;
  Handle<Object> value_of = value_of_it.GetDataValue();
  if (!value_of->IsJSFunction()) return false;
  Builtin value_of_builtin_id =
      Handle<JSFunction>::cast(value_of)->code().builtin_id();
  if (value_of_builtin_id != Builtin::kObjectPrototypeValueOf) return false;

  // The {toString} member must be the default "FunctionPrototypeToString".
  LookupIterator to_string_it{isolate, function,
                              isolate->factory()->toString_string()};
  if (to_string_it.state() != LookupIterator::DATA) return false;
  Handle<Object> to_string = to_string_it.GetDataValue();
  if (!to_string->IsJSFunction()) return false;
  Builtin to_string_builtin_id =
      Handle<JSFunction>::cast(to_string)->code().builtin_id();
  if (to_string_builtin_id != Builtin::kFunctionPrototypeToString) return false;

  // Just a default function, which will convert to "Nan". Accept this.
  return true;
}

bool MaybeMarkError(ValueOrError value, ErrorThrower* thrower) {
  if (is_error(value)) {
    thrower->RuntimeError("%s",
                          MessageFormatter::TemplateString(to_error(value)));
    return true;
  }
  return false;
}
}  // namespace

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
  PropertyKey key(isolate_, Handle<Name>::cast(import_name));
  LookupIterator it(isolate_, ffi_.ToHandleChecked(), key);
  switch (it.state()) {
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::INTEGER_INDEXED_EXOTIC:
    case LookupIterator::INTERCEPTOR:
    case LookupIterator::JSPROXY:
    case LookupIterator::WASM_OBJECT:
    case LookupIterator::ACCESSOR:
    case LookupIterator::TRANSITION:
      return ReportLinkError("not a data property", index, import_name);
    case LookupIterator::NOT_FOUND:
      // Accepting missing properties as undefined does not cause any
      // observable difference from JavaScript semantics, we are lenient.
      return isolate_->factory()->undefined_value();
    case LookupIterator::DATA: {
      Handle<Object> value = it.GetDataValue();
      // For legacy reasons, we accept functions for imported globals (see
      // {ProcessImportedGlobal}), but only if we can easily determine that
      // their Number-conversion is side effect free and returns NaN (which is
      // the case as long as "valueOf" (or others) are not overwritten).
      if (value->IsJSFunction() &&
          module_->import_table[index].kind == kExternalGlobal &&
          !HasDefaultToNumberBehaviour(isolate_,
                                       Handle<JSFunction>::cast(value))) {
        return ReportLinkError("function has special ToNumber behaviour", index,
                               import_name);
      }
      return value;
    }
  }
}

// Load data segments into the memory.
void InstanceBuilder::LoadDataSegments(Handle<WasmInstanceObject> instance) {
  base::Vector<const uint8_t> wire_bytes =
      module_object_->native_module()->wire_bytes();
  for (const WasmDataSegment& segment : module_->data_segments) {
    uint32_t size = segment.source.length();

    // Passive segments are not copied during instantiation.
    if (!segment.active) continue;

    size_t dest_offset;
    if (module_->is_memory64) {
      ValueOrError result = EvaluateConstantExpression(
          &init_expr_zone_, segment.dest_addr, kWasmI64, isolate_, instance);
      if (MaybeMarkError(result, thrower_)) return;
      uint64_t dest_offset_64 = to_value(result).to_u64();

      // Clamp to {std::numeric_limits<size_t>::max()}, which is always an
      // invalid offset.
      DCHECK_GT(std::numeric_limits<size_t>::max(), instance->memory_size());
      dest_offset = static_cast<size_t>(std::min(
          dest_offset_64, uint64_t{std::numeric_limits<size_t>::max()}));
    } else {
      ValueOrError result = EvaluateConstantExpression(
          &init_expr_zone_, segment.dest_addr, kWasmI32, isolate_, instance);
      if (MaybeMarkError(result, thrower_)) return;
      dest_offset = to_value(result).to_u32();
    }

    if (!base::IsInBounds<size_t>(dest_offset, size, instance->memory_size())) {
      thrower_->RuntimeError("data segment is out of bounds");
      return;
    }

    std::memcpy(instance->memory_start() + dest_offset,
                wire_bytes.begin() + segment.source.offset(), size);
  }
}

void InstanceBuilder::WriteGlobalValue(const WasmGlobal& global,
                                       const WasmValue& value) {
  TRACE("init [globals_start=%p + %u] = %s, type = %s\n",
        global.type.is_reference()
            ? reinterpret_cast<byte*>(tagged_globals_->address())
            : raw_buffer_ptr(untagged_globals_, 0),
        global.offset, value.to_string().c_str(), global.type.name().c_str());
  DCHECK(IsSubtypeOf(value.type(), global.type, module_));
  if (global.type.is_numeric()) {
    value.CopyTo(GetRawUntaggedGlobalPtr<byte>(global));
  } else {
    tagged_globals_->set(global.offset, *value.to_ref());
  }
}

void InstanceBuilder::SanitizeImports() {
  base::Vector<const uint8_t> wire_bytes =
      module_object_->native_module()->wire_bytes();
  for (size_t index = 0; index < module_->import_table.size(); ++index) {
    const WasmImport& import = module_->import_table[index];

    Handle<String> module_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate_, wire_bytes, import.module_name, kInternalize);

    Handle<String> import_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate_, wire_bytes, import.field_name, kInternalize);

    int int_index = static_cast<int>(index);
    MaybeHandle<Object> result =
        is_asmjs_module(module_)
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

bool InstanceBuilder::FindImportedMemory() {
  DCHECK_EQ(module_->import_table.size(), sanitized_imports_.size());
  for (size_t index = 0; index < module_->import_table.size(); index++) {
    WasmImport import = module_->import_table[index];

    if (import.kind == kExternalMemory) {
      auto& value = sanitized_imports_[index].value;
      if (!value->IsWasmMemoryObject()) return false;
      memory_object_ = Handle<WasmMemoryObject>::cast(value);
      memory_buffer_ =
          Handle<JSArrayBuffer>(memory_object_->array_buffer(), isolate_);
      return true;
    }
  }
  return false;
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
  // Store any {WasmExternalFunction} callable in the instance before the call
  // is resolved to preserve its identity. This handles exported functions as
  // well as functions constructed via other means (e.g. WebAssembly.Function).
  if (WasmExternalFunction::IsWasmExternalFunction(*value)) {
    WasmInstanceObject::SetWasmInternalFunction(
        isolate_, instance, func_index,
        WasmInternalFunction::FromExternal(
            Handle<WasmExternalFunction>::cast(value), isolate_)
            .ToHandleChecked());
  }
  auto js_receiver = Handle<JSReceiver>::cast(value);
  const FunctionSig* expected_sig = module_->functions[func_index].sig;
  auto resolved = compiler::ResolveWasmImportCall(js_receiver, expected_sig,
                                                  module_, enabled_);
  compiler::WasmImportCallKind kind = resolved.kind;
  js_receiver = resolved.callable;
  switch (kind) {
    case compiler::WasmImportCallKind::kLinkError:
      ReportLinkError("imported function does not match the expected type",
                      import_index, module_name, import_name);
      return false;
    case compiler::WasmImportCallKind::kWasmToWasm: {
      // The imported function is a Wasm function from another instance.
      auto imported_function = Handle<WasmExportedFunction>::cast(js_receiver);
      Handle<WasmInstanceObject> imported_instance(
          imported_function->instance(), isolate_);
      // The import reference is the instance object itself.
      Address imported_target = imported_function->GetWasmCallTarget();
      ImportedFunctionEntry entry(instance, func_index);
      entry.SetWasmToWasm(*imported_instance, imported_target);
      break;
    }
    case compiler::WasmImportCallKind::kWasmToCapi: {
      NativeModule* native_module = instance->module_object().native_module();
      int expected_arity = static_cast<int>(expected_sig->parameter_count());
      WasmImportWrapperCache* cache = native_module->import_wrapper_cache();
      // TODO(jkummerow): Consider precompiling CapiCallWrappers in parallel,
      // just like other import wrappers.
      uint32_t canonical_type_index =
          module_->isorecursive_canonical_type_ids
              [module_->functions[func_index].sig_index];
      WasmCode* wasm_code = cache->MaybeGet(kind, canonical_type_index,
                                            expected_arity, kNoSuspend);
      if (wasm_code == nullptr) {
        WasmCodeRefScope code_ref_scope;
        WasmImportWrapperCache::ModificationScope cache_scope(cache);
        wasm_code =
            compiler::CompileWasmCapiCallWrapper(native_module, expected_sig);
        WasmImportWrapperCache::CacheKey key(kind, canonical_type_index,
                                             expected_arity, kNoSuspend);
        cache_scope[key] = wasm_code;
        wasm_code->IncRef();
        isolate_->counters()->wasm_generated_code_size()->Increment(
            wasm_code->instructions().length());
        isolate_->counters()->wasm_reloc_size()->Increment(
            wasm_code->reloc_info().length());
      }

      ImportedFunctionEntry entry(instance, func_index);
      // We re-use the SetWasmToJs infrastructure because it passes the
      // callable to the wrapper, which we need to get the function data.
      entry.SetWasmToJs(isolate_, js_receiver, wasm_code, kNoSuspend);
      break;
    }
    case compiler::WasmImportCallKind::kWasmToJSFastApi: {
      NativeModule* native_module = instance->module_object().native_module();
      DCHECK(js_receiver->IsJSFunction() || js_receiver->IsJSBoundFunction());
      WasmCodeRefScope code_ref_scope;
      WasmCode* wasm_code = compiler::CompileWasmJSFastCallWrapper(
          native_module, expected_sig, js_receiver);
      ImportedFunctionEntry entry(instance, func_index);
      entry.SetWasmToJs(isolate_, js_receiver, wasm_code, kNoSuspend);
      break;
    }
    default: {
      // The imported function is a callable.

      int expected_arity = static_cast<int>(expected_sig->parameter_count());
      if (kind == compiler::WasmImportCallKind::kJSFunctionArityMismatch) {
        Handle<JSFunction> function = Handle<JSFunction>::cast(js_receiver);
        SharedFunctionInfo shared = function->shared();
        expected_arity =
            shared.internal_formal_parameter_count_without_receiver();
      }

      NativeModule* native_module = instance->module_object().native_module();
      uint32_t canonical_type_index =
          module_->isorecursive_canonical_type_ids
              [module_->functions[func_index].sig_index];
      WasmCode* wasm_code = native_module->import_wrapper_cache()->Get(
          kind, canonical_type_index, expected_arity, resolved.suspend);
      DCHECK_NOT_NULL(wasm_code);
      ImportedFunctionEntry entry(instance, func_index);
      if (wasm_code->kind() == WasmCode::kWasmToJsWrapper) {
        // Wasm to JS wrappers are treated specially in the import table.
        entry.SetWasmToJs(isolate_, js_receiver, wasm_code, resolved.suspend);
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
    Handle<WasmInstanceObject> instance, int table_index, int import_index,
    Handle<WasmTableObject> table_object) {
  int imported_table_size = table_object->current_length();
  // Allocate a new dispatch table.
  WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
      instance, table_index, imported_table_size);
  // Initialize the dispatch table with the (foreign) JS functions
  // that are already in the table.
  for (int i = 0; i < imported_table_size; ++i) {
    bool is_valid;
    bool is_null;
    MaybeHandle<WasmInstanceObject> maybe_target_instance;
    int function_index;
    MaybeHandle<WasmJSFunction> maybe_js_function;
    WasmTableObject::GetFunctionTableEntry(
        isolate_, module_, table_object, i, &is_valid, &is_null,
        &maybe_target_instance, &function_index, &maybe_js_function);
    if (!is_valid) {
      thrower_->LinkError("table import %d[%d] is not a wasm function",
                          import_index, i);
      return false;
    }
    if (is_null) continue;
    Handle<WasmJSFunction> js_function;
    if (maybe_js_function.ToHandle(&js_function)) {
      WasmInstanceObject::ImportWasmJSFunctionIntoTable(
          isolate_, instance, table_index, i, js_function);
      continue;
    }

    Handle<WasmInstanceObject> target_instance =
        maybe_target_instance.ToHandleChecked();
    const WasmModule* target_module = target_instance->module_object().module();
    const WasmFunction& function = target_module->functions[function_index];

    FunctionTargetAndRef entry(target_instance, function_index);
    uint32_t canonicalized_sig_index =
        target_module->isorecursive_canonical_type_ids[function.sig_index];
    instance->GetIndirectFunctionTable(isolate_, table_index)
        ->Set(i, canonicalized_sig_index, entry.call_target(), *entry.ref());
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

  auto table_object = Handle<WasmTableObject>::cast(value);

  uint32_t imported_table_size =
      static_cast<uint32_t>(table_object->current_length());
  if (imported_table_size < table.initial_size) {
    thrower_->LinkError("table import %d is smaller than initial %u, got %u",
                        import_index, table.initial_size, imported_table_size);
    return false;
  }

  if (table.has_maximum_size) {
    if (table_object->maximum_length().IsUndefined(isolate_)) {
      thrower_->LinkError("table import %d has no maximum length, expected %u",
                          import_index, table.maximum_size);
      return false;
    }
    int64_t imported_maximum_size = table_object->maximum_length().Number();
    if (imported_maximum_size < 0) {
      thrower_->LinkError("table import %d has no maximum length, expected %u",
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

  const WasmModule* table_type_module =
      !table_object->instance().IsUndefined()
          ? WasmInstanceObject::cast(table_object->instance()).module()
          : instance->module();

  if (!EquivalentTypes(table.type, table_object->type(), module_,
                       table_type_module)) {
    ReportLinkError("imported table does not match the expected type",
                    import_index, module_name, import_name);
    return false;
  }

  if (IsSubtypeOf(table.type, kWasmFuncRef, module_) &&
      !InitializeImportedIndirectFunctionTable(instance, table_index,
                                               import_index, table_object)) {
    return false;
  }

  instance->tables().set(table_index, *value);
  return true;
}

bool InstanceBuilder::ProcessImportedMemory(Handle<WasmInstanceObject> instance,
                                            int import_index,
                                            Handle<String> module_name,
                                            Handle<String> import_name,
                                            Handle<Object> value) {
  if (!value->IsWasmMemoryObject()) {
    ReportLinkError("memory import must be a WebAssembly.Memory object",
                    import_index, module_name, import_name);
    return false;
  }
  auto memory_object = Handle<WasmMemoryObject>::cast(value);

  // The imported memory should have been already set up early.
  CHECK_EQ(instance->memory_object(), *memory_object);

  Handle<JSArrayBuffer> buffer(memory_object_->array_buffer(), isolate_);
  // memory_ should have already been assigned in Build().
  DCHECK_EQ(*memory_buffer_.ToHandleChecked(), *buffer);
  uint32_t imported_cur_pages =
      static_cast<uint32_t>(buffer->byte_length() / kWasmPageSize);
  if (imported_cur_pages < module_->initial_pages) {
    thrower_->LinkError("memory import %d is smaller than initial %u, got %u",
                        import_index, module_->initial_pages,
                        imported_cur_pages);
    return false;
  }
  int32_t imported_maximum_pages = memory_object_->maximum_pages();
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
  if (static_cast<bool>(global_object->is_mutable()) != global.mutability) {
    ReportLinkError("imported global does not match the expected mutability",
                    import_index, module_name, import_name);
    return false;
  }

  const WasmModule* global_type_module =
      !global_object->instance().IsUndefined()
          ? WasmInstanceObject::cast(global_object->instance()).module()
          : instance->module();

  bool valid_type =
      global.mutability
          ? EquivalentTypes(global_object->type(), global.type,
                            global_type_module, instance->module())
          : IsSubtypeOf(global_object->type(), global.type, global_type_module,
                        instance->module());

  if (!valid_type) {
    ReportLinkError("imported global does not match the expected type",
                    import_index, module_name, import_name);
    return false;
  }
  if (global.mutability) {
    DCHECK_LT(global.index, module_->num_imported_mutable_globals);
    Handle<Object> buffer;
    if (global.type.is_reference()) {
      static_assert(sizeof(global_object->offset()) <= sizeof(Address),
                    "The offset into the globals buffer does not fit into "
                    "the imported_mutable_globals array");
      buffer = handle(global_object->tagged_buffer(), isolate_);
      // For externref globals we use a relative offset, not an absolute
      // address.
      instance->imported_mutable_globals().set_int(
          global.index * kSystemPointerSize, global_object->offset());
    } else {
      buffer = handle(global_object->untagged_buffer(), isolate_);
      // It is safe in this case to store the raw pointer to the buffer
      // since the backing store of the JSArrayBuffer will not be
      // relocated.
      Address address = reinterpret_cast<Address>(raw_buffer_ptr(
          Handle<JSArrayBuffer>::cast(buffer), global_object->offset()));
      instance->imported_mutable_globals().set_sandboxed_pointer(
          global.index * kSystemPointerSize, address);
    }
    instance->imported_mutable_globals_buffers().set(global.index, *buffer);
    return true;
  }

  WasmValue value;
  switch (global_object->type().kind()) {
    case kI32:
      value = WasmValue(global_object->GetI32());
      break;
    case kI64:
      value = WasmValue(global_object->GetI64());
      break;
    case kF32:
      value = WasmValue(global_object->GetF32());
      break;
    case kF64:
      value = WasmValue(global_object->GetF64());
      break;
    case kRtt:
    case kRef:
    case kRefNull:
      value = WasmValue(global_object->GetRef(), global_object->type());
      break;
    case kVoid:
    case kS128:
    case kBottom:
    case kI8:
    case kI16:
      UNREACHABLE();
  }

  WriteGlobalValue(global, value);
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

  // SIMD proposal allows modules to define an imported v128 global, and only
  // supports importing a WebAssembly.Global object for this global, but also
  // defines constructing a WebAssembly.Global of v128 to be a TypeError.
  // We *should* never hit this case in the JS API, but the module should should
  // be allowed to declare such a global (no validation error).
  if (global.type == kWasmS128 && !value->IsWasmGlobalObject()) {
    ReportLinkError("global import of type v128 must be a WebAssembly.Global",
                    import_index, module_name, import_name);
    return false;
  }

  if (is_asmjs_module(module_)) {
    // Accepting {JSFunction} on top of just primitive values here is a
    // workaround to support legacy asm.js code with broken binding. Note
    // that using {NaN} (or Smi::zero()) here is what using the observable
    // conversion via {ToPrimitive} would produce as well. {LookupImportAsm}
    // checked via {HasDefaultToNumberBehaviour} that "valueOf" or friends have
    // not been patched.
    if (value->IsJSFunction()) value = isolate_->factory()->nan_value();
    if (value->IsPrimitive()) {
      MaybeHandle<Object> converted = global.type == kWasmI32
                                          ? Object::ToInt32(isolate_, value)
                                          : Object::ToNumber(isolate_, value);
      if (!converted.ToHandle(&value)) {
        // Conversion is known to fail for Symbols and BigInts.
        ReportLinkError("global import must be a number", import_index,
                        module_name, import_name);
        return false;
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

  if (global.type.is_reference()) {
    const char* error_message;
    Handle<Object> wasm_value;
    if (!wasm::JSToWasmObject(isolate_, module_, value, global.type,
                              &error_message)
             .ToHandle(&wasm_value)) {
      ReportLinkError(error_message, global_index, module_name, import_name);
      return false;
    }
    WriteGlobalValue(global, WasmValue(wasm_value, global.type));
    return true;
  }

  if (value->IsNumber() && global.type != kWasmI64) {
    double number_value = value->Number();
    // The Wasm-BigInt proposal currently says that i64 globals may
    // only be initialized with BigInts. See:
    // https://github.com/WebAssembly/JS-BigInt-integration/issues/12
    WasmValue wasm_value = global.type == kWasmI32
                               ? WasmValue(DoubleToInt32(number_value))
                               : global.type == kWasmF32
                                     ? WasmValue(DoubleToFloat32(number_value))
                                     : WasmValue(number_value);
    WriteGlobalValue(global, wasm_value);
    return true;
  }

  if (global.type == kWasmI64 && value->IsBigInt()) {
    WriteGlobalValue(global, WasmValue(BigInt::cast(*value).AsInt64()));
    return true;
  }

  ReportLinkError(
      "global import must be a number, valid Wasm reference, or "
      "WebAssembly.Global object",
      import_index, module_name, import_name);
  return false;
}

void InstanceBuilder::CompileImportWrappers(
    Handle<WasmInstanceObject> instance) {
  int num_imports = static_cast<int>(module_->import_table.size());
  TRACE_EVENT1("v8.wasm", "wasm.CompileImportWrappers", "num_imports",
               num_imports);
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
    const FunctionSig* sig = module_->functions[func_index].sig;
    auto resolved =
        compiler::ResolveWasmImportCall(js_receiver, sig, module_, enabled_);
    compiler::WasmImportCallKind kind = resolved.kind;
    if (kind == compiler::WasmImportCallKind::kWasmToWasm ||
        kind == compiler::WasmImportCallKind::kLinkError ||
        kind == compiler::WasmImportCallKind::kWasmToCapi ||
        kind == compiler::WasmImportCallKind::kWasmToJSFastApi) {
      continue;
    }

    int expected_arity = static_cast<int>(sig->parameter_count());
    if (resolved.kind ==
        compiler::WasmImportCallKind::kJSFunctionArityMismatch) {
      Handle<JSFunction> function = Handle<JSFunction>::cast(resolved.callable);
      SharedFunctionInfo shared = function->shared();
      expected_arity =
          shared.internal_formal_parameter_count_without_receiver();
    }
    uint32_t canonical_type_index =
        module_->isorecursive_canonical_type_ids[module_->functions[func_index]
                                                     .sig_index];
    WasmImportWrapperCache::CacheKey key(kind, canonical_type_index,
                                         expected_arity, resolved.suspend);
    if (cache_scope[key] != nullptr) {
      // Cache entry already exists, no need to compile it again.
      continue;
    }
    import_wrapper_queue.insert(key, sig);
  }

  auto compile_job_task = std::make_unique<CompileImportWrapperJob>(
      isolate_->counters(), native_module, &import_wrapper_queue, &cache_scope);
  auto compile_job = V8::GetCurrentPlatform()->CreateJob(
      TaskPriority::kUserVisible, std::move(compile_job_task));

  // Wait for the job to finish, while contributing in this thread.
  compile_job->Join();
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
        USE(num_imported_tables);
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
      case kExternalTag: {
        if (!value->IsWasmTagObject()) {
          ReportLinkError("tag import requires a WebAssembly.Tag", index,
                          module_name, import_name);
          return -1;
        }
        Handle<WasmTagObject> imported_tag = Handle<WasmTagObject>::cast(value);
        if (!imported_tag->MatchesSignature(module_->tags[import.index].sig)) {
          ReportLinkError("imported tag does not match the expected type",
                          index, module_name, import_name);
          return -1;
        }
        Object tag = imported_tag->tag();
        DCHECK(instance->tags_table().get(import.index).IsUndefined());
        instance->tags_table().set(import.index, tag);
        tags_wrappers_[import.index] = imported_tag;
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  return num_imported_functions;
}

template <typename T>
T* InstanceBuilder::GetRawUntaggedGlobalPtr(const WasmGlobal& global) {
  return reinterpret_cast<T*>(raw_buffer_ptr(untagged_globals_, global.offset));
}

// Process initialization of globals.
void InstanceBuilder::InitGlobals(Handle<WasmInstanceObject> instance) {
  for (const WasmGlobal& global : module_->globals) {
    if (global.mutability && global.imported) continue;
    // Happens with imported globals.
    if (!global.init.is_set()) continue;

    ValueOrError result = EvaluateConstantExpression(
        &init_expr_zone_, global.init, global.type, isolate_, instance);
    if (MaybeMarkError(result, thrower_)) return;

    if (global.type.is_reference()) {
      tagged_globals_->set(global.offset, *to_value(result).to_ref());
    } else {
      to_value(result).CopyTo(GetRawUntaggedGlobalPtr<byte>(global));
    }
  }
}

// Allocate memory for a module instance as a new JSArrayBuffer.
bool InstanceBuilder::AllocateMemory() {
  int initial_pages = static_cast<int>(module_->initial_pages);
  int maximum_pages = module_->has_maximum_pages
                          ? static_cast<int>(module_->maximum_pages)
                          : WasmMemoryObject::kNoMaximum;
  auto shared = (module_->has_shared_memory && enabled_.has_threads())
                    ? SharedFlag::kShared
                    : SharedFlag::kNotShared;

  auto mem_type = module_->is_memory64 ? WasmMemoryFlag::kWasmMemory64
                                       : WasmMemoryFlag::kWasmMemory32;
  if (!WasmMemoryObject::New(isolate_, initial_pages, maximum_pages, shared,
                             mem_type)
           .ToHandle(&memory_object_)) {
    thrower_->RangeError(
        "Out of memory: Cannot allocate Wasm memory for new instance");
    return false;
  }
  memory_buffer_ =
      Handle<JSArrayBuffer>(memory_object_->array_buffer(), isolate_);
  return true;
}

// Process the exports, creating wrappers for functions, tables, memories,
// globals, and exceptions.
void InstanceBuilder::ProcessExports(Handle<WasmInstanceObject> instance) {
  std::unordered_map<int, Handle<Object>> imported_globals;

  // If an imported WebAssembly function or global gets exported, the export
  // has to be identical to to import. Therefore we cache all imported
  // WebAssembly functions in the instance, and all imported globals in a map
  // here.
  for (int index = 0, end = static_cast<int>(module_->import_table.size());
       index < end; ++index) {
    const WasmImport& import = module_->import_table[index];
    if (import.kind == kExternalFunction) {
      Handle<Object> value = sanitized_imports_[index].value;
      if (WasmExternalFunction::IsWasmExternalFunction(*value)) {
        WasmInstanceObject::SetWasmInternalFunction(
            isolate_, instance, import.index,
            WasmInternalFunction::FromExternal(
                Handle<WasmExternalFunction>::cast(value), isolate_)
                .ToHandleChecked());
      }
    } else if (import.kind == kExternalGlobal) {
      Handle<Object> value = sanitized_imports_[index].value;
      if (value->IsWasmGlobalObject()) {
        imported_globals[import.index] = value;
      }
    }
  }

  Handle<JSObject> exports_object;
  MaybeHandle<String> single_function_name;
  bool is_asm_js = is_asmjs_module(module_);
  if (is_asm_js) {
    Handle<JSFunction> object_function = Handle<JSFunction>(
        isolate_->native_context()->object_function(), isolate_);
    exports_object = isolate_->factory()->NewJSObject(object_function);
    single_function_name =
        isolate_->factory()->InternalizeUtf8String(AsmJs::kSingleFunctionName);
  } else {
    exports_object = isolate_->factory()->NewJSObjectWithNullProto();
  }
  instance->set_exports_object(*exports_object);

  PropertyDescriptor desc;
  desc.set_writable(is_asm_js);
  desc.set_enumerable(true);
  desc.set_configurable(is_asm_js);

  // Process each export in the export table.
  for (const WasmExport& exp : module_->export_table) {
    Handle<String> name = WasmModuleObject::ExtractUtf8StringFromModuleBytes(
        isolate_, module_object_, exp.name, kInternalize);
    Handle<JSObject> export_to = exports_object;
    switch (exp.kind) {
      case kExternalFunction: {
        // Wrap and export the code as a JSFunction.
        // TODO(wasm): reduce duplication with LoadElemSegment() further below
        Handle<WasmInternalFunction> internal =
            WasmInstanceObject::GetOrCreateWasmInternalFunction(
                isolate_, instance, exp.index);
        Handle<WasmExternalFunction> wasm_external_function =
            handle(WasmExternalFunction::cast(internal->external()), isolate_);
        desc.set_value(wasm_external_function);

        if (is_asm_js &&
            String::Equals(isolate_, name,
                           single_function_name.ToHandleChecked())) {
          export_to = instance;
        }
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
        if (global.imported) {
          auto cached_global = imported_globals.find(exp.index);
          if (cached_global != imported_globals.end()) {
            desc.set_value(cached_global->second);
            break;
          }
        }
        Handle<JSArrayBuffer> untagged_buffer;
        Handle<FixedArray> tagged_buffer;
        uint32_t offset;

        if (global.mutability && global.imported) {
          Handle<FixedArray> buffers_array(
              instance->imported_mutable_globals_buffers(), isolate_);
          if (global.type.is_reference()) {
            tagged_buffer = handle(
                FixedArray::cast(buffers_array->get(global.index)), isolate_);
            // For externref globals we store the relative offset in the
            // imported_mutable_globals array instead of an absolute address.
            offset = instance->imported_mutable_globals().get_int(
                global.index * kSystemPointerSize);
          } else {
            untagged_buffer =
                handle(JSArrayBuffer::cast(buffers_array->get(global.index)),
                       isolate_);
            Address global_addr =
                instance->imported_mutable_globals().get_sandboxed_pointer(
                    global.index * kSystemPointerSize);

            size_t buffer_size = untagged_buffer->byte_length();
            Address backing_store =
                reinterpret_cast<Address>(untagged_buffer->backing_store());
            CHECK(global_addr >= backing_store &&
                  global_addr < backing_store + buffer_size);
            offset = static_cast<uint32_t>(global_addr - backing_store);
          }
        } else {
          if (global.type.is_reference()) {
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
            WasmGlobalObject::New(isolate_, instance, untagged_buffer,
                                  tagged_buffer, global.type, offset,
                                  global.mutability)
                .ToHandleChecked();
        desc.set_value(global_obj);
        break;
      }
      case kExternalTag: {
        const WasmTag& tag = module_->tags[exp.index];
        Handle<WasmTagObject> wrapper = tags_wrappers_[exp.index];
        if (wrapper.is_null()) {
          Handle<HeapObject> tag_object(
              HeapObject::cast(instance->tags_table().get(exp.index)),
              isolate_);
          wrapper = WasmTagObject::New(isolate_, tag.sig, tag_object);
          tags_wrappers_[exp.index] = wrapper;
        }
        desc.set_value(wrapper);
        break;
      }
      default:
        UNREACHABLE();
    }

    v8::Maybe<bool> status = JSReceiver::DefineOwnProperty(
        isolate_, export_to, name, &desc, Just(kThrowOnError));
    if (!status.IsJust()) {
      DisallowGarbageCollection no_gc;
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

namespace {
V8_INLINE void SetFunctionTablePlaceholder(Isolate* isolate,
                                           Handle<WasmInstanceObject> instance,
                                           Handle<WasmTableObject> table_object,
                                           uint32_t entry_index,
                                           uint32_t func_index) {
  const WasmModule* module = instance->module();
  const WasmFunction* function = &module->functions[func_index];
  MaybeHandle<WasmInternalFunction> wasm_internal_function =
      WasmInstanceObject::GetWasmInternalFunction(isolate, instance,
                                                  func_index);
  if (wasm_internal_function.is_null()) {
    // No JSFunction entry yet exists for this function. Create a {Tuple2}
    // holding the information to lazily allocate one.
    WasmTableObject::SetFunctionTablePlaceholder(
        isolate, table_object, entry_index, instance, func_index);
  } else {
    table_object->entries().set(entry_index,
                                *wasm_internal_function.ToHandleChecked());
  }
  WasmTableObject::UpdateDispatchTables(isolate, *table_object, entry_index,
                                        function, *instance);
}

V8_INLINE void SetFunctionTableNullEntry(Isolate* isolate,
                                         Handle<WasmTableObject> table_object,
                                         uint32_t entry_index) {
  table_object->entries().set(entry_index, *isolate->factory()->null_value());
  WasmTableObject::ClearDispatchTables(isolate, table_object, entry_index);
}
}  // namespace

void InstanceBuilder::SetTableInitialValues(
    Handle<WasmInstanceObject> instance) {
  for (int table_index = 0;
       table_index < static_cast<int>(module_->tables.size()); ++table_index) {
    const WasmTable& table = module_->tables[table_index];
    if (table.initial_value.is_set()) {
      auto table_object = handle(
          WasmTableObject::cast(instance->tables().get(table_index)), isolate_);
      bool is_function_table = IsSubtypeOf(table.type, kWasmFuncRef, module_);
      if (is_function_table &&
          table.initial_value.kind() == ConstantExpression::kRefFunc) {
        for (uint32_t entry_index = 0; entry_index < table.initial_size;
             entry_index++) {
          SetFunctionTablePlaceholder(isolate_, instance, table_object,
                                      entry_index, table.initial_value.index());
        }
      } else if (is_function_table &&
                 table.initial_value.kind() == ConstantExpression::kRefNull) {
        for (uint32_t entry_index = 0; entry_index < table.initial_size;
             entry_index++) {
          SetFunctionTableNullEntry(isolate_, table_object, entry_index);
        }
      } else {
        ValueOrError result =
            EvaluateConstantExpression(&init_expr_zone_, table.initial_value,
                                       table.type, isolate_, instance);
        if (MaybeMarkError(result, thrower_)) return;
        for (uint32_t entry_index = 0; entry_index < table.initial_size;
             entry_index++) {
          WasmTableObject::Set(isolate_, table_object, entry_index,
                               to_value(result).to_ref());
        }
      }
    }
  }
}

namespace {
// If the operation succeeds, returns an empty {Optional}. Otherwise, returns an
// {Optional} containing the {MessageTemplate} code of the error.
base::Optional<MessageTemplate> LoadElemSegmentImpl(
    Zone* zone, Isolate* isolate, Handle<WasmInstanceObject> instance,
    Handle<WasmTableObject> table_object, uint32_t table_index,
    uint32_t segment_index, uint32_t dst, uint32_t src, size_t count) {
  DCHECK_LT(segment_index, instance->module()->elem_segments.size());
  auto& elem_segment = instance->module()->elem_segments[segment_index];
  // TODO(wasm): Move this functionality into wasm-objects, since it is used
  // for both instantiation and in the implementation of the table.init
  // instruction.
  if (!base::IsInBounds<uint64_t>(dst, count, table_object->current_length())) {
    return {MessageTemplate::kWasmTrapTableOutOfBounds};
  }
  if (!base::IsInBounds<uint64_t>(
          src, count,
          instance->dropped_elem_segments().get(segment_index) == 0
              ? elem_segment.entries.size()
              : 0)) {
    return {MessageTemplate::kWasmTrapElementSegmentOutOfBounds};
  }

  bool is_function_table =
      IsSubtypeOf(table_object->type(), kWasmFuncRef, instance->module());

  ErrorThrower thrower(isolate, "LoadElemSegment");

  for (size_t i = 0; i < count; ++i) {
    ConstantExpression entry = elem_segment.entries[src + i];
    int entry_index = static_cast<int>(dst + i);
    if (is_function_table && entry.kind() == ConstantExpression::kRefFunc) {
      SetFunctionTablePlaceholder(isolate, instance, table_object, entry_index,
                                  entry.index());
    } else if (is_function_table &&
               entry.kind() == ConstantExpression::kRefNull) {
      SetFunctionTableNullEntry(isolate, table_object, entry_index);
    } else {
      ValueOrError result = EvaluateConstantExpression(
          zone, entry, elem_segment.type, isolate, instance);
      if (is_error(result)) return to_error(result);
      WasmTableObject::Set(isolate, table_object, entry_index,
                           to_value(result).to_ref());
    }
  }
  return {};
}
}  // namespace

void InstanceBuilder::LoadTableSegments(Handle<WasmInstanceObject> instance) {
  for (uint32_t segment_index = 0;
       segment_index < module_->elem_segments.size(); ++segment_index) {
    auto& elem_segment = instance->module()->elem_segments[segment_index];
    // Passive segments are not copied during instantiation.
    if (elem_segment.status != WasmElemSegment::kStatusActive) continue;

    uint32_t table_index = elem_segment.table_index;
    ValueOrError value = EvaluateConstantExpression(
        &init_expr_zone_, elem_segment.offset, kWasmI32, isolate_, instance);
    if (MaybeMarkError(value, thrower_)) return;
    uint32_t dst = std::get<WasmValue>(value).to_u32();
    if (thrower_->error()) return;
    uint32_t src = 0;
    size_t count = elem_segment.entries.size();

    base::Optional<MessageTemplate> opt_error = LoadElemSegmentImpl(
        &init_expr_zone_, isolate_, instance,
        handle(WasmTableObject::cast(
                   instance->tables().get(elem_segment.table_index)),
               isolate_),
        table_index, segment_index, dst, src, count);
    // Set the active segments to being already dropped, since table.init on
    // a dropped passive segment and an active segment have the same behavior.
    instance->dropped_elem_segments().set(segment_index, 1);
    if (opt_error.has_value()) {
      thrower_->RuntimeError(
          "%s", MessageFormatter::TemplateString(opt_error.value()));
      return;
    }
  }
}

void InstanceBuilder::InitializeTags(Handle<WasmInstanceObject> instance) {
  Handle<FixedArray> tags_table(instance->tags_table(), isolate_);
  for (int index = 0; index < tags_table->length(); ++index) {
    if (!tags_table->get(index).IsUndefined(isolate_)) continue;
    Handle<WasmExceptionTag> tag = WasmExceptionTag::New(isolate_, index);
    tags_table->set(index, *tag);
  }
}

base::Optional<MessageTemplate> LoadElemSegment(
    Isolate* isolate, Handle<WasmInstanceObject> instance, uint32_t table_index,
    uint32_t segment_index, uint32_t dst, uint32_t src, uint32_t count) {
  AccountingAllocator allocator;
  // This {Zone} will be used only by the temporary WasmFullDecoder allocated
  // down the line from this call. Therefore it is safe to stack-allocate it
  // here.
  Zone zone(&allocator, "LoadElemSegment");
  return LoadElemSegmentImpl(
      &zone, isolate, instance,
      handle(WasmTableObject::cast(instance->tables().get(table_index)),
             isolate),
      table_index, segment_index, dst, src, count);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef TRACE
